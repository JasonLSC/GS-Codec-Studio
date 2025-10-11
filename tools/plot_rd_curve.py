#!/usr/bin/env python3
"""
Plot RD curves (PSNR/SSIM/LPIPS vs Bitrate) from a JSON statistics file.

Usage:
  python tools/plot_rd_curve.py \
    --input stats/dy_gs_for_thesis.json \
    --output outputs/rd_curve.png

Notes:
  - X-axis is bitrate in Mbit/s.
  - Three subplots arranged horizontally: PSNR, SSIM, LPIPS.
  - All subplots share a single legend displayed above them.
  - Entries missing either bitrate or the requested metric are skipped.
"""

from __future__ import annotations

import argparse
import json
import math
import os
from typing import Any, Dict, List, Optional, Tuple

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load_stats(json_path: str) -> Dict[str, Any]:
    with open(json_path, "r", encoding="utf-8") as f:
        return json.load(f)


def collect_method_points(
    data: Dict[str, Any],
) -> Dict[str, Dict[str, List[Tuple[float, float]]]]:
    """
    Transform the raw JSON dict into a structure:
      method -> metric_name -> list of (bitrate, value)

    Expected JSON structure per method:
      method: {
        variantA: {
          quality_metrics: { PSNR, SSIM, LPIPS },
          memory_metrics: { storage(MB), bitrate(Mb/s) }
        },
        variantB: { ... }
      }

    If a method has only "default", it is treated as a single variant.
    """
    result: Dict[str, Dict[str, List[Tuple[float, float]]]] = {}

    for method_name, variants in data.items():
        # Variants is a dict like {"default": {...}} or {"L": {...}, "S": {...}}
        method_points: Dict[str, List[Tuple[float, float]]] = {
            "PSNR": [],
            "SSIM": [],
            "LPIPS": [],
        }

        for variant_name, record in variants.items():
            quality = record.get("quality_metrics", {})
            memory = record.get("memory_metrics", {})

            bitrate = memory.get("bitrate(Mb/s)")
            if bitrate is None or (isinstance(bitrate, float) and math.isnan(bitrate)):
                continue

            def add_point(metric_key: str) -> None:
                value = quality.get(metric_key)
                if value is None:
                    return
                if isinstance(value, float) and math.isnan(value):
                    return
                method_points[metric_key].append((float(bitrate), float(value)))

            add_point("PSNR")
            add_point("SSIM")
            add_point("LPIPS")

        # Keep only metrics with at least one point
        filtered = {
            metric: sorted(points, key=lambda t: t[0])
            for metric, points in method_points.items()
            if len(points) > 0
        }
        if len(filtered) > 0:
            result[method_name] = filtered

    return result


def ensure_dir(path: str) -> None:
    directory = os.path.dirname(path)
    if directory and not os.path.exists(directory):
        os.makedirs(directory, exist_ok=True)


def plot_rd_curves(
    methods: Dict[str, Dict[str, List[Tuple[float, float]]]],
    output_path: str,
    figsize: Tuple[int, int] = (18, 5),
    dpi: int = 150,
) -> None:
    # Set up three side-by-side subplots for PSNR, SSIM, LPIPS
    fig, axes = plt.subplots(1, 3, figsize=figsize, dpi=dpi, sharex=False)
    metrics = [
        ("PSNR", "PSNR (dB)", True),
        ("SSIM", "SSIM", True),
        ("LPIPS", "LPIPS (lower is better)", True),
    ]

    # Use a consistent color cycle across subplots
    for ax in axes:
        ax.grid(True, linestyle="--", alpha=0.3)
        ax.set_xlabel("Bitrate (Mbit/s)")
        ax.set_xscale("log")

    # Collect handles and labels for a shared legend
    legend_handles: List[Any] = []
    legend_labels: List[str] = []

    # Define a fixed style map for consistent colors/markers across subplots
    # The order of methods will follow iteration order of the dict
    method_names = list(methods.keys())
    color_cycle = plt.get_cmap("tab10")
    markers = ["o"]

    for ax, (metric_key, ylabel, is_higher_better) in zip(axes, metrics):
        ax.set_ylabel(ylabel)

        for idx, (method_name, metric_map) in enumerate(methods.items()):
            points = metric_map.get(metric_key)
            if not points:
                continue

            x = [p[0] for p in points]
            y = [p[1] for p in points]

            # Sort to ensure line plots are monotonic in x
            order = sorted(range(len(x)), key=lambda i: x[i])
            x = [x[i] for i in order]
            y = [y[i] for i in order]

            color = color_cycle(idx % 10)
            marker = markers[idx % len(markers)]
            (line,) = ax.plot(
                x,
                y,
                marker=marker,
                linewidth=2.0,
                markersize=6,
                color=color,
                alpha=0.9,
                label=method_name,
            )

            # Capture legend entry once per method
            if method_name not in legend_labels:
                legend_handles.append(line)
                legend_labels.append(method_name)

        # Keep LPIPS with regular orientation (larger at top, smaller at bottom).

    # Shared legend across subplots, placed on top
    fig.legend(
        legend_handles,
        legend_labels,
        loc="upper center",
        ncol=min(len(legend_labels), 6),
        frameon=False,
        bbox_to_anchor=(0.5, 1.06),
    )

    plt.tight_layout(rect=(0, 0, 1, 0.95))

    ensure_dir(output_path)
    plt.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot RD curves from JSON stats.")
    parser.add_argument(
        "--input",
        type=str,
        required=True,
        help="Path to JSON file (e.g., stats/dy_gs_for_thesis.json)",
    )
    parser.add_argument(
        "--output",
        type=str,
        default="outputs/rd_curve.png",
        help="Path to save the output figure (PNG)",
    )
    parser.add_argument(
        "--dpi",
        type=int,
        default=150,
        help="Figure DPI",
    )

    args = parser.parse_args()

    stats = load_stats(args.input)
    # The JSON top-level may include wrapping keys; detect the first dict of methods
    if isinstance(stats, dict) and len(stats) == 1 and isinstance(next(iter(stats.values())), dict):
        stats = next(iter(stats.values()))

    methods = collect_method_points(stats)
    if not methods:
        raise SystemExit("No valid (bitrate, metric) points found in the input JSON.")

    plot_rd_curves(methods, args.output, dpi=args.dpi)


if __name__ == "__main__":
    main()


