#!/usr/bin/env python3

"""Plot rate–distortion curves for static Gaussian Splat compression baselines.

## Overview
This utility consumes static benchmark statistics and renders an ``N × M``
figure grid summarising rate–distortion trends. Each row corresponds to a
dataset, while columns correspond to user-selected metrics (default: PSNR,
SSIM, LPIPS). The x-axis shows memory footprint (MB); the y-axis shows the
metric value. Markers, colours, and linestyles remain consistent across all
subplots to ease cross-dataset comparisons.

## Features
- Handles multiple datasets and methods from a single JSON summary.
- Groups methods by broad categories to provide structured legends.
- Automatically adjusts axis limits and grid layout for balanced aesthetics.
- Supports interactive display (``--show``) or direct PNG export.
- Allows filtering/ordering of metrics via ``--metrics``.

## Input Data
- ``--data``: Path to a JSON file matching ``open_stats/static_rd.json``. The
  layout must follow:
  ``{ "results_<Dataset>": { "Method": { "Variant": [PSNR, SSIM, LPIPS, Mem] }}}``.

## Output
- ``--out``: PNG figure containing an array of subplots with RD curves and a
  consolidated legend. The DPI is configurable through ``--dpi``.

## Usage Examples
```bash
# Generate default figure into tools/static_bench/static_rd_curves.png
python tools/static_bench/make_rd_curve.py

# Custom metrics and output location
python tools/static_bench/make_rd_curve.py --metrics psnr lpips \
       --out outputs/tt_rd_curves.png

# Show interactively without saving
python tools/static_bench/make_rd_curve.py --show
```

## Error Handling
- Raises a ``ValueError`` if the input JSON is empty or metrics are missing.
- Skips methods without samples for a given dataset/metric.
- Propagates JSON parsing errors so they can be surfaced to the caller.

## Requirements
- Python 3.8+
- ``matplotlib``
- Standard library: ``argparse``, ``json``, ``pathlib``, ``math``
"""

from __future__ import annotations

import argparse
import json
import math
from collections import OrderedDict
from pathlib import Path
from typing import Dict, Iterable, List, Tuple

import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

# Default palette/markers follow the Jupyter plotting style used internally and
# cover at least eight distinct methods before repeating.
DEFAULT_COLORS = [
    "#FF5B5B",  # red
    "#47B39C",  # teal
    "#4C8DFF",  # blue
    "#FFB240",  # orange
    "#9B6EFF",  # purple
    "#FF6060",  # light red
    "#2CA05A",  # light green
    "#6060FF",  # light blue
]

DEFAULT_MARKERS = ["o"]


METRICS: Dict[str, Tuple[int, str]] = {
    "psnr": (0, "PSNR (dB)"),
    "ssim": (1, "SSIM"),
    "lpips": (2, "LPIPS"),
}

GROUP_CONFIG = {
    "scaffold": {
        "methods": {
            "HAC",
            "HAC++",
            "ContextGS",
            "HEMGS",
            "CAT-3DGS",
        },
        "marker": "o",
        "linestyle": "--",
    },
    "vanilla": {
        "methods": {
            "Ours",
            "gsplat_comp.",
            "gsplat_comp",
            "SOGS",
        },
        "marker": "s",
        "linestyle": "-",
    },
    "other": {
        "methods": set(),
        "marker": "^",
        "linestyle": "-.",
    },
}

GROUP_LABELS = {
    "scaffold": "Scaffold-GS-based solutions",
    "vanilla": "Vanilla GS-based solutions",
    "other": "Other solutions",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--data",
        type=Path,
        default=Path(__file__).resolve().parent / "static_rd.json",
        help="Path to the JSON file containing static RD statistics",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=Path(__file__).resolve().parent / "static_rd_curves.png",
        help="Output path for the combined RD curve figure (PNG)",
    )
    parser.add_argument(
        "--metrics",
        nargs="*",
        default=["psnr", "ssim", "lpips"],
        choices=sorted(METRICS.keys()),
        help="Metrics to plot from left to right",
    )
    parser.add_argument(
        "--dpi",
        type=int,
        default=250,
        help="DPI for the saved figure",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Display the figure interactively instead of saving",
    )
    return parser.parse_args()


def load_results(path: Path) -> OrderedDict[str, OrderedDict[str, Dict[str, List[float]]]]:
    with path.open("r", encoding="utf-8") as fp:
        raw = json.load(fp)
    return OrderedDict(
        (dataset, OrderedDict(methods.items())) for dataset, methods in raw.items()
    )


def identify_group(method: str) -> str:
    for group_name, cfg in GROUP_CONFIG.items():
        if method in cfg["methods"]:
            return group_name
    return "other"


def prepare_styles(methods: Iterable[str]) -> Tuple[Dict[str, Tuple[str, str, str]], Dict[str, str]]:
    styles: Dict[str, Tuple[str, str, str]] = {}
    method_groups: Dict[str, str] = {}
    for idx, method in enumerate(methods):
        group_name = identify_group(method)
        cfg = GROUP_CONFIG[group_name]
        base_color = DEFAULT_COLORS[idx % len(DEFAULT_COLORS)]
        if group_name == "scaffold":
            color = _lighten_hex_color(base_color, factor=0.35)
        else:
            color = base_color
        marker = cfg["marker"]
        linestyle = cfg["linestyle"]
        styles[method] = (color, marker, linestyle)
        method_groups[method] = group_name
    return styles, method_groups


def _lighten_hex_color(hex_color: str, factor: float = 0.3) -> str:
    hex_color = hex_color.lstrip("#")
    if len(hex_color) != 6:
        return f"#{hex_color}"
    rgb = tuple(int(hex_color[i : i + 2], 16) for i in range(0, 6, 2))
    lightened = tuple(
        min(255, int(channel + (255 - channel) * factor)) for channel in rgb
    )
    return "#" + "".join(f"{value:02X}" for value in lightened)


def compute_limits(values: List[float], margin: float = 0.12) -> Tuple[float, float]:
    if not values:
        return 0.0, 1.0
    vmin = min(values)
    vmax = max(values)
    if vmin == vmax:
        pad = max(abs(vmin) * margin, 1e-3)
        return vmin - pad, vmax + pad
    span = vmax - vmin
    pad = span * margin
    return vmin - pad, vmax + pad


def gather_method_order(results: OrderedDict) -> List[str]:
    method_order: OrderedDict[str, None] = OrderedDict()
    for dataset_results in results.values():
        for method in dataset_results.keys():
            method_order.setdefault(method, None)
    return list(method_order.keys())


def format_dataset_label(dataset_key: str) -> str:
    if dataset_key.startswith("results_"):
        dataset_key = dataset_key[len("results_") :]
    return dataset_key.replace("_", " ")


def plot_grid(
    results: OrderedDict,
    metric_keys: List[str],
    styles: Dict[str, Tuple[str, str, str]],
    method_groups: Dict[str, str],
    out_path: Path,
    dpi: int,
    show: bool,
) -> None:
    if not metric_keys:
        raise ValueError("At least one metric must be provided.")

    datasets = list(results.keys())
    if not datasets:
        raise ValueError("The input JSON does not contain any dataset entries.")

    n_rows = len(datasets)
    n_cols = len(metric_keys)

    base_height = 3.0
    base_width = base_height * (5.0 / 3.0)
    fig_width = base_width * n_cols
    fig_height = base_height * n_rows

    fig, axes = plt.subplots(
        n_rows,
        n_cols,
        figsize=(fig_width, fig_height),
        squeeze=False,
        gridspec_kw={"wspace": 0.22, "hspace": 0.32},
    )

    methods_with_points: set[str] = set()

    grid_params = {
        "linestyle": "--",
        "alpha": 0.2,
        "color": "gray",
        "which": "both",
        "zorder": 0,
    }

    for row_index, dataset_key in enumerate(datasets):
        dataset_label = format_dataset_label(dataset_key)
        dataset_results = results[dataset_key]

        method_sizes: Dict[str, List[float]] = {}
        method_metrics: Dict[str, Dict[str, List[float]]] = {
            metric_key: {} for metric_key in metric_keys
        }

        for method, variants in dataset_results.items():
            ordered = sorted(variants.values(), key=lambda values: values[3])
            if not ordered:
                continue

            sizes = [values[3] for values in ordered]
            method_sizes[method] = sizes

            for metric_key in metric_keys:
                metric_idx, _ = METRICS[metric_key]
                method_metrics[metric_key][method] = [
                    values[metric_idx] for values in ordered
                ]

        row_axes = axes[row_index]

        for col_index, (axis, metric_key) in enumerate(zip(row_axes, metric_keys)):
            metric_idx, metric_label = METRICS[metric_key]

            axis.set_axisbelow(True)
            axis.grid(True, **grid_params)
            axis.set_xlabel("Memory (MB)")
            axis.set_ylabel(metric_label)

            all_sizes: List[float] = []
            all_metric: List[float] = []

            for method in dataset_results.keys():
                sizes = method_sizes.get(method)
                metric_values = method_metrics[metric_key].get(method)
                if not sizes or not metric_values:
                    continue

                all_sizes.extend(sizes)
                all_metric.extend(metric_values)

                color, marker, linestyle = styles[method]
                group = method_groups.get(method, "other")
                line_alpha = 1
                scatter_alpha = 1
                if group == "scaffold":
                    line_alpha = 0.60
                    scatter_alpha = 0.70

                if len(sizes) > 1:
                    axis.plot(
                        sizes,
                        metric_values,
                        color=color,
                        linestyle=linestyle,
                        linewidth=1.5,
                        alpha=line_alpha,
                        zorder=2,
                    )

                scatter = axis.scatter(
                    sizes,
                    metric_values,
                    color=color,
                    marker=marker,
                    s=42,
                    linewidth=0.0,
                    label=method,
                    zorder=3,
                    alpha=scatter_alpha,
                )

                methods_with_points.add(method)

            x_min, x_max = compute_limits(all_sizes, margin=0.05)
            y_min, y_max = compute_limits(all_metric, margin=0.18)

            if x_max <= x_min:
                pad = max(abs(x_max), 1.0) * 0.2
                axis.set_xlim(x_min - pad, x_max + pad)
            else:
                span = x_max - x_min
                axis.set_xlim(x_min, x_max + span * 0.1)

            axis.set_ylim(y_min, y_max)

        # Add dataset label on the left margin of the row.
        row_axes[0].text(
            -0.25,
            0.5,
            dataset_label,
            transform=row_axes[0].transAxes,
            rotation=90,
            va="center",
            ha="right",
            fontsize=11,
            fontweight="bold",
        )

    group_entries: Dict[str, List[Tuple[Line2D, str]]] = {name: [] for name in GROUP_CONFIG}
    for method in styles.keys():
        if method not in methods_with_points:
            continue
        color, marker, linestyle = styles[method]
        proxy = Line2D(
            [0],
            [0],
            color=color,
            marker=marker,
            linestyle=linestyle,
            linewidth=1.6,
            markersize=6,
            alpha=0.78,
        )
        group_name = method_groups.get(method, "other")
        group_entries.setdefault(group_name, []).append((proxy, method))

    ordered_groups: List[str] = [name for name in ("scaffold", "vanilla", "other") if group_entries.get(name)]

    if ordered_groups:
        grouped_entries: List[List[Tuple[Line2D, str]]] = [group_entries[name] for name in ordered_groups]
        max_len = max(len(entries) for entries in grouped_entries)
        ncols = max_len + 1 if max_len > 0 else 1

        group_headers = {
            "scaffold": "Scaffold-GS based methods:",
            "vanilla": "Vanilla GS based methods:",
            "other": "Other methods:",
        }

        grid_entries: List[List[Tuple[Line2D | None, str]]] = []
        for group_name, entries in zip(ordered_groups, grouped_entries):
            header_label = group_headers.get(group_name, f"{group_name.title()} methods:")
            row: List[Tuple[Line2D | None, str]] = [(None, header_label)] + list(entries)
            pad_needed = ncols - len(row)
            for _ in range(pad_needed):
                row.append((None, ""))
            grid_entries.append(row)

        handles: List[Line2D] = []
        labels: List[str] = []

        nrows = len(grid_entries)
        for col in range(ncols):
            for row_idx in range(nrows):
                handle, label = grid_entries[row_idx][col]
                if handle is None:
                    blank = Line2D([], [], linestyle="None")
                    blank.set_alpha(0.0)
                    handles.append(blank)
                else:
                    handles.append(handle)
                labels.append(label)

        legend = fig.legend(
            handles,
            labels,
            loc="upper center",
            ncol=ncols,
            frameon=True,
            bbox_to_anchor=(0.5, 0.985),
            borderpad=0.6,
            columnspacing=2,
            labelspacing=1.0,
            handlelength=2.6,
        )
        legend.get_frame().set_linewidth(0.8)
        legend.get_frame().set_edgecolor("#888888")

        header_labels = {
            "Scaffold-GS based methods:",
            "Vanilla GS based methods:",
            "Other methods:",
        }


    if show:
        plt.show()
    else:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(out_path, dpi=dpi, bbox_inches="tight")
        print(f"saved {out_path}")
    plt.close(fig)


def main() -> None:
    args = parse_args()
    results = load_results(args.data)
    metric_keys = args.metrics

    method_order = gather_method_order(results)
    styles, method_groups = prepare_styles(method_order)

    plot_grid(
        results,
        metric_keys,
        styles,
        method_groups,
        args.out,
        args.dpi,
        args.show,
    )


if __name__ == "__main__":
    main()


