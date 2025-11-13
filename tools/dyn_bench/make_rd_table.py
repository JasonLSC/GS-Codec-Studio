#!/usr/bin/env python3
"""Generate a LaTeX rate–distortion table from dynamic benchmark statistics."""

from __future__ import annotations

import argparse
import json
import math
import shutil
import subprocess
import tempfile
from collections import OrderedDict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple


@dataclass(frozen=True)
class MetricSpec:
    name: str
    header: str
    decimals: int
    higher_is_better: bool
    path: Sequence[str]


METRICS: Sequence[MetricSpec] = (
    MetricSpec(name="psnr", header=r"\textbf{PSNR}$\uparrow$", decimals=2, higher_is_better=True, path=("quality_metrics", "PSNR")),
    MetricSpec(name="ssim", header=r"\textbf{SSIM}$\uparrow$", decimals=3, higher_is_better=True, path=("quality_metrics", "SSIM")),
    MetricSpec(name="lpips", header=r"\textbf{LPIPS}$\downarrow$", decimals=3, higher_is_better=False, path=("quality_metrics", "LPIPS")),
    MetricSpec(name="bitrate", header=r"\textbf{Bitrate}$\downarrow$", decimals=2, higher_is_better=False, path=("memory_metrics", "bitrate(Mb/s)")),
)


COLORBOX_BY_RANK = {1: "tabfirst", 2: "tabsecond", 3: "tabthird"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate LaTeX RD table from dynamic benchmark statistics.")
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("open_stats/dyn_rd.json"),
        help="Path to the input JSON file.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("tools/dyn_bench/dyn_rd_table.tex"),
        help="Path to the output LaTeX file.",
    )
    parser.add_argument(
        "--pdf",
        type=Path,
        default=None,
        help="Optional path to the output PDF file.",
    )
    return parser.parse_args()


def load_stats(json_path: Path) -> OrderedDict:
    with json_path.open("r", encoding="utf-8") as handle:
        return json.load(handle, object_pairs_hook=OrderedDict)


def latex_escape(text: str) -> str:
    replacements = {
        "\\": r"\\textbackslash{}",
        "&": r"\\&",
        "%": r"\\%",
        "$": r"\\$",
        "#": r"\\#",
        "_": r"\\_",
        "{": r"\\{",
        "}": r"\\}",
        "~": r"\\textasciitilde{}",
        "^": r"\\textasciicircum{}",
    }
    escaped = []
    for char in text:
        escaped.append(replacements.get(char, char))
    return "".join(escaped)


def format_display_name(method: str, variant: Optional[str]) -> str:
    base = latex_escape(method)
    if variant is None:
        return base

    variant_clean = variant.strip()
    if not variant_clean or variant_clean.lower() == "default":
        return base

    return f"{base} ({latex_escape(variant_clean)})"


def extract_metric(entry: Dict, spec: MetricSpec) -> Optional[float]:
    current = entry
    for key in spec.path:
        if not isinstance(current, dict):
            return None
        current = current.get(key)
        if current is None:
            return None
    if isinstance(current, (int, float)):
        return float(current)
    return None


def format_metric(value: Optional[float], decimals: int) -> str:
    if value is None:
        return "--"
    fmt = f"{{:.{decimals}f}}"
    return fmt.format(value)


def compute_rankings(rows: Sequence[Dict]) -> List[Dict[Tuple[str, Optional[str]], int]]:
    rankings: List[Dict[Tuple[str, Optional[str]], int]] = []
    for metric_index, spec in enumerate(METRICS):
        values: List[Tuple[Tuple[str, Optional[str]], float]] = []
        for row in rows:
            value = row["metrics"][metric_index]
            if value is None:
                continue
            values.append((row["row_id"], value))

        reverse = spec.higher_is_better
        sorted_values = sorted(values, key=lambda item: item[1], reverse=reverse)

        ranking: Dict[Tuple[str, Optional[str]], int] = {}
        current_rank = 1
        for index, (row_id, value) in enumerate(sorted_values):
            if index > 0:
                prev_value = sorted_values[index - 1][1]
                if not math.isclose(prev_value, value, rel_tol=1e-6, abs_tol=1e-9):
                    current_rank = index + 1
            if current_rank > 3:
                break
            ranking[row_id] = current_rank
        rankings.append(ranking)
    return rankings


def apply_highlight(
    row_id: Tuple[str, Optional[str]],
    metric_index: int,
    value: str,
    rankings: Sequence[Dict[Tuple[str, Optional[str]], int]],
) -> str:
    rank = rankings[metric_index].get(row_id)
    if rank in COLORBOX_BY_RANK:
        color = COLORBOX_BY_RANK[rank]
        return f"\\colorbox{{{color}}}{{{value}}}"
    return value


def build_rows(data: OrderedDict) -> List[Dict]:
    rows: List[Dict] = []
    for method, variants in data.items():
        variant_items = list(variants.items())
        has_multiple = len(variant_items) > 1
        for variant_key, metrics in variant_items:
            variant_name = variant_key if has_multiple else None
            row_metrics = [extract_metric(metrics, spec) for spec in METRICS]
            rows.append(
                {
                    "row_id": (method, variant_name),
                    "method": method,
                    "display_name": format_display_name(method, variant_name),
                    "metrics": row_metrics,
                }
            )
    return rows


def build_table(rows: Sequence[Dict]) -> str:
    rankings = compute_rankings(rows)

    lines: List[str] = []
    lines.append(r"\definecolor{tabfirst}{RGB}{255, 223, 127}")
    lines.append(r"\definecolor{tabsecond}{RGB}{224, 224, 224}")
    lines.append(r"\definecolor{tabthird}{RGB}{205, 179, 139}")
    lines.append("")
    lines.append(r"\begin{table*}[t]")
    lines.append(r"\centering")
    lines.append("")
    lines.append(r"\tiny")
    lines.append(r"\setlength{\tabcolsep}{4pt}")
    lines.append(r"\caption{\textbf{Rate--distortion comparison on dynamic scenes.} The \colorbox{tabfirst}{best}, \colorbox{tabsecond}{second best}, and \colorbox{tabthird}{third best} results are highlighted for each metric.}")
    lines.append(r"\begin{tabular}{l|cccc}")
    lines.append(r"\toprule")
    header_cells = ["\\textbf{Method}"] + [spec.header for spec in METRICS]
    lines.append(" & ".join(header_cells) + r"\\")
    lines.append(r"\midrule")

    for idx, row in enumerate(rows):
        tokens: List[str] = [row["display_name"]]
        for metric_index, spec in enumerate(METRICS):
            value = row["metrics"][metric_index]
            formatted = format_metric(value, spec.decimals)
            highlighted = apply_highlight(row["row_id"], metric_index, formatted, rankings)
            tokens.append(highlighted)
        lines.append(" & ".join(tokens) + r"\\")

        next_method = rows[idx + 1]["method"] if idx + 1 < len(rows) else None
        if next_method is not None and next_method != row["method"]:
            lines.append(r"\addlinespace")

    lines.append(r"\bottomrule")
    lines.append(r"\end{tabular}")
    lines.append(r"\label{tab:dyn_rd_table}")
    lines.append(r"\end{table*}")

    return "\n".join(lines) + "\n"


def wrap_table_in_document(table_body: str) -> str:
    parts = [
        r"\documentclass{article}",
        r"\usepackage[margin=1in]{geometry}",
        r"\usepackage{booktabs}",
        r"\usepackage{multirow}",
        r"\usepackage{colortbl}",
        r"\usepackage{xcolor}",
        r"\begin{document}",
        table_body,
        r"\end{document}",
        "",
    ]
    return "\n".join(parts)


def find_compiler() -> Optional[List[str]]:
    candidates = [
        ("pdflatex", ["pdflatex", "-interaction=nonstopmode", "-halt-on-error"]),
        ("latexmk", ["latexmk", "-pdf", "-interaction=nonstopmode"]),
        ("tectonic", ["tectonic", "--keep-intermediates"]),
    ]
    for executable, command in candidates:
        path = shutil.which(executable)
        if path:
            command[0] = path
            return command
    return None


def compile_pdf(table_body: str, pdf_path: Path) -> bool:
    compiler_command = find_compiler()
    if compiler_command is None:
        print("LaTeX compiler not found. Skipping PDF generation.")
        return False

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        tex_file = tmpdir_path / "table.tex"
        table_body_for_pdf = table_body.replace("\\begin{table*}", "\\begin{table}").replace("\\end{table*}", "\\end{table}")
        tex_content = wrap_table_in_document(table_body_for_pdf)
        tex_file.write_text(tex_content, encoding="utf-8")

        compile_command = list(compiler_command)
        compile_command.append(str(tex_file.name))

        try:
            subprocess.run(
                compile_command,
                cwd=tmpdir,
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
        except subprocess.CalledProcessError as exc:
            print("PDF compilation failed. Output:")
            print(exc.stdout.decode("utf-8", errors="ignore"))
            print(exc.stderr.decode("utf-8", errors="ignore"))
            return False

        pdf_output = tex_file.with_suffix(".pdf")
        if not pdf_output.exists():
            print("Compilation did not produce a PDF file.")
            return False

        pdf_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(pdf_output, pdf_path)
        return True


def main() -> None:
    args = parse_args()

    data = load_stats(args.input)
    rows = build_rows(data)
    if not rows:
        raise ValueError("No rows generated from input data.")

    table_content = build_table(rows)

    output_path = args.output
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(table_content, encoding="utf-8")

    print(f"LaTeX table written to {output_path}")

    if args.pdf is not None:
        success = compile_pdf(table_content, args.pdf)
        if success:
            print(f"PDF table written to {args.pdf}")
        else:
            print("PDF generation skipped or failed.")


if __name__ == "__main__":
    main()


