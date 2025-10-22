#!/usr/bin/env python3
"""Generate a LaTeX rate–distortion table from static benchmark statistics.

## Overview
This script aggregates static RD benchmark metrics into a multi-dataset LaTeX
table. It supports optional PDF compilation, enabling quick inclusion of
formatted tables in reports or papers.

## Features
- Normalises dataset/method labels and variant names for readability.
- Highlights the top three performers per metric with configurable colours.
- Produces booktabs-style tables and optional LaTeX-to-PDF compilation.
- Handles methods with multiple rate points by merging rows across datasets.
- Provides CLI parameters for custom input/output locations.

## Input Data
- ``--input``: JSON file shaped like ``open_stats/static_rd.json`` with nested
  structure ``results_<Dataset> → Method → Variant → [PSNR, SSIM, LPIPS, Mem]``.

## Outputs
- ``--output``: LaTeX source containing a full ``table*`` environment.
- ``--pdf`` (optional): When provided and a LaTeX compiler is available, the
  script writes a compiled PDF table to the specified path.

## Usage Examples
```bash
# Generate LaTeX table only
python tools/static_bench/make_rd_table.py

# Custom I/O paths
python tools/static_bench/make_rd_table.py --input my_stats.json \
       --output outputs/static_rd.tex

# Generate LaTeX and PDF simultaneously
python tools/static_bench/make_rd_table.py --pdf outputs/static_rd.pdf
```

## PDF Generation Notes
- The script searches for ``pdflatex``, ``latexmk``, or ``tectonic`` in ``PATH``.
- On success, auxiliary compilation files are confined to a temporary directory.
- If no compiler is found or compilation fails, the LaTeX source is still
  generated and an informative warning is printed.

## Requirements
- Python 3.8+
- Optional: a LaTeX distribution for PDF compilation
- Standard library only
"""

from __future__ import annotations

import argparse
import json
import math
import re
import shutil
import subprocess
import tempfile
from collections import OrderedDict
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple


@dataclass(frozen=True)
class MetricSpec:
    name: str
    header: str
    decimals: int
    higher_is_better: bool


METRICS: Sequence[MetricSpec] = (
    MetricSpec(name="psnr", header=r"\textbf{PSNR}$\uparrow$", decimals=2, higher_is_better=True),
    MetricSpec(name="ssim", header=r"\textbf{SSIM}$\uparrow$", decimals=3, higher_is_better=True),
    MetricSpec(name="lpips", header=r"\textbf{LPIPS}$\downarrow$", decimals=3, higher_is_better=False),
    MetricSpec(name="size", header=r"\textbf{Size}$\downarrow$", decimals=2, higher_is_better=False),
)

COLORBOX_BY_RANK = {1: "tabfirst", 2: "tabsecond", 3: "tabthird"}

METHOD_RENAMES = {
    "SOGS": "SOG",
    "gsplat_comp.": "gsplat-comp.",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate LaTeX RD table from JSON statistics.")
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("open_stats/static_rd.json"),
        help="Path to the input JSON file.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("tools/static_bench/static_rd_table.tex"),
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


def normalize_dataset_label(raw_key: str) -> str:
    label = raw_key
    if label.startswith("results_"):
        label = label[len("results_") :]
    label = label.replace("_", " ")
    replacements = {
        "Mipnerf": "MipNeRF",
    }
    for token, replacement in replacements.items():
        label = re.sub(token, replacement, label, flags=re.IGNORECASE)
    return label


def normalize_variant_key(method: str, variant_key: str, has_multiple: bool) -> Optional[str]:
    if not has_multiple:
        return None

    if variant_key.lower() == method.lower():
        return "base"

    lambda_match = re.search(r"lambda[_-]?([0-9.]+)", variant_key, re.IGNORECASE)
    if lambda_match:
        return f"lambda_{lambda_match.group(1)}"

    stripped = variant_key
    for sep in ("-", "_"):
        prefix = f"{method}{sep}"
        if variant_key.startswith(prefix):
            stripped = variant_key[len(prefix) :]
            break

    stripped = stripped.strip()
    if not stripped:
        return "base"

    return stripped


def format_lambda_value(raw_value: str) -> str:
    try:
        value = float(raw_value)
    except (ValueError, TypeError):
        try:
            value = float(Decimal(raw_value))
        except (InvalidOperation, ValueError):
            return latex_escape(raw_value)

    if value == 0.0:
        return "0"

    exponent = int(math.floor(math.log10(abs(value))))
    mantissa = value / (10 ** exponent)

    if math.isclose(mantissa, round(mantissa)):
        mantissa_str = str(int(round(mantissa)))
    else:
        mantissa_str = f"{mantissa:.2f}".rstrip("0").rstrip(".")

    return f"{mantissa_str}{{\\times}}10^{{{exponent}}}"


def format_display_name(method: str, normalized_variant: Optional[str]) -> str:
    base = METHOD_RENAMES.get(method, method)
    base = latex_escape(base)

    if normalized_variant is None or normalized_variant == "base":
        return base

    lower_variant = normalized_variant.lower()
    if lower_variant in {"lowrate", "low"}:
        return f"{base} (low)"
    if lower_variant in {"highrate", "high"}:
        return f"{base} (high)"
    if lower_variant.startswith("lambda_"):
        value = normalized_variant.split("_", 1)[1]
        lambda_str = format_lambda_value(value)
        return f"{base} ($\\lambda{{=}}{lambda_str}$)"
    if re.match(r"^[0-9.]+$", normalized_variant):
        return f"{base} ({normalized_variant})"
    return f"{base} ({latex_escape(normalized_variant)})"


def format_metric(value: Optional[float], decimals: int) -> str:
    if value is None:
        return "--"
    fmt = f"{{:.{decimals}f}}"
    return fmt.format(value)


def apply_highlight(row_id: Tuple[str, Optional[str]], dataset: str, metric_index: int, value: str,
                    rankings: Dict[str, List[Dict[Tuple[str, Optional[str]], int]]]) -> str:
    rank = rankings.get(dataset, [{}])[metric_index].get(row_id)
    if rank in COLORBOX_BY_RANK:
        color = COLORBOX_BY_RANK[rank]
        return f"\\colorbox{{{color}}}{{{value}}}"
    return value


def compute_rankings(
    rows: Sequence[Dict],
    dataset_labels: Sequence[str],
) -> Dict[str, List[Dict[Tuple[str, Optional[str]], int]]]:
    result: Dict[str, List[Dict[Tuple[str, Optional[str]], int]]] = {}
    for dataset in dataset_labels:
        metric_rankings: List[Dict[Tuple[str, Optional[str]], int]] = []
        for metric_index, spec in enumerate(METRICS):
            values: List[Tuple[Tuple[str, Optional[str]], float]] = []
            for row in rows:
                metrics = row["datasets"].get(dataset)
                if metrics is None:
                    continue
                value = metrics[metric_index]
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
            metric_rankings.append(ranking)
        result[dataset] = metric_rankings
    return result


def build_rows(data: OrderedDict) -> Tuple[List[Dict], List[str]]:
    dataset_labels: List[str] = []
    rows: List[Dict] = []
    row_index: Dict[Tuple[str, Optional[str]], int] = {}

    for dataset_key, methods in data.items():
        dataset_label = normalize_dataset_label(dataset_key)
        dataset_labels.append(dataset_label)
        for method, variants in methods.items():
            variant_items = list(variants.items())
            has_multiple = len(variant_items) > 1
            for variant_key, metrics in variant_items:
                normalized_variant = normalize_variant_key(method, variant_key, has_multiple)
                row_id = (method, normalized_variant)
                if row_id not in row_index:
                    row_index[row_id] = len(rows)
                    rows.append(
                        {
                            "row_id": row_id,
                            "method": method,
                            "normalized_variant": normalized_variant,
                            "display_name": format_display_name(method, normalized_variant),
                            "datasets": {dataset_label: metrics},
                        }
                    )
                else:
                    rows[row_index[row_id]]["datasets"][dataset_label] = metrics

    return rows, dataset_labels


def build_table(rows: Sequence[Dict], dataset_labels: Sequence[str]) -> str:
    rankings = compute_rankings(rows, dataset_labels)

    header_columns = ["\\multirow{2}{*}{\\textbf{Method}}"]
    for dataset in dataset_labels[:-1]:
        header_columns.append(
            f"\\multicolumn{{{len(METRICS)}}}{{c|}}{{\\textbf{{{latex_escape(dataset)}}}}}"
        )
    last_dataset = dataset_labels[-1]
    header_columns.append(
        f"\\multicolumn{{{len(METRICS)}}}{{c}}{{\\textbf{{{latex_escape(last_dataset)}}}}}"
    )

    first_header = header_columns[0]
    for col in header_columns[1:]:
        first_header += f" & {col}"

    second_header_cells = [""]
    for _dataset in dataset_labels:
        for spec in METRICS:
            second_header_cells.append(spec.header)

    second_header = " & ".join(["", *second_header_cells[1:]])

    column_spec_parts = ["l"]
    for index, _ in enumerate(dataset_labels):
        column_spec_parts.append("c" * len(METRICS))
    column_spec_str = "|".join(column_spec_parts)

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
    lines.append(r"\caption{\textbf{Rate--distortion comparison on static scenes.} The \colorbox{tabfirst}{best}, \colorbox{tabsecond}{second best}, and \colorbox{tabthird}{third best} results are highlighted for each metric.}")
    lines.append(f"\\begin{{tabular}}{{{column_spec_str}}}")
    lines.append(r"\toprule")
    lines.append(first_header + r"\\")
    cmidrule_end = 1 + len(dataset_labels) * len(METRICS)
    lines.append(r"\cmidrule{2-" + f"{cmidrule_end}" + "}")
    lines.append(second_header + r"\\")
    lines.append(r"\midrule")

    for idx, row in enumerate(rows):
        tokens: List[str] = [row["display_name"]]
        for dataset in dataset_labels:
            metrics = row["datasets"].get(dataset)
            for metric_index, spec in enumerate(METRICS):
                value = metrics[metric_index] if metrics is not None else None
                numeric_str = format_metric(value, spec.decimals)
                highlighted = apply_highlight(row["row_id"], dataset, metric_index, numeric_str, rankings)
                tokens.append(highlighted)
        line = " & ".join(tokens) + r"\\"
        lines.append(line)

        next_method = rows[idx + 1]["method"] if idx + 1 < len(rows) else None
        if next_method is not None and next_method != row["method"]:
            lines.append(r"\addlinespace")

    lines.append(r"\bottomrule")
    lines.append(r"\end{tabular}")
    lines.append(r"\label{tab:static_rd_table}")
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
        table_body_for_pdf = (
            table_body
            .replace("\\begin{table*}", "\\begin{table}")
            .replace("\\end{table*}", "\\end{table}")
        )
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
    rows, dataset_labels = build_rows(data)
    table_content = build_table(rows, dataset_labels)

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

