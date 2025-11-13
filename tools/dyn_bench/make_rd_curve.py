#!/usr/bin/env python3
"""
Render rate–distortion (RD) curves for dynamic Gaussian Splatting benchmarks.

Input:
    open_stats/dyn_rd.json – nested dictionaries of method -> variant -> metrics,
    including quality metrics (PSNR/SSIM/LPIPS) and bitrate information.

Output:
    dyn_rd_curves.png – a figure with three side-by-side subplots for
    PSNR / SSIM / LPIPS. The x-axis uses bitrate (Mbps) on a logarithmic scale,
    while the y-axis shows the corresponding quality metric.

Highlights:
    - Reuses the palette and marker sequence from the static bench tool to keep styling consistent.
    - Filters out points with missing or non-positive bitrate or metric values.
    - Supports --show (interactive display), --out (custom output path), --dpi, etc.
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
from matplotlib.ticker import FormatStrFormatter, MultipleLocator

# Shared color palette with the static RD plotting utility.
DEFAULT_COLORS = [
    "#FF5B5B",  # red
    "#47B39C",  # teal
    "#5F8DD3",  # blue
    "#FFB240",  # orange
    "#9B6EFF",  # purple
    "#FF80B2",  
    "#2E8B57",  # sea green
    "#8B7355",  # khaki brown  
]

# Marker configuration: all methods use a unified circular marker.
DEFAULT_MARKERS = ["o"]

# Fixed order of displayed metrics and their axis labels.
METRIC_ORDER = [
    ("PSNR", "PSNR (dB)"),
    ("SSIM", "SSIM"),
    ("LPIPS", "LPIPS"),
]

# Opacity configuration for lines, markers, and legend proxies.
LINE_ALPHA = 0.65
MARKER_ALPHA = 0.82


def parse_args() -> argparse.Namespace:
    """Parse CLI arguments controlling input/output paths and render options.

    Returns
    -------
    argparse.Namespace
        Parsed arguments including `data` (Path), `out` (Path), `dpi` (int), and
        `show` (bool) flags.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    project_root = Path(__file__).resolve().parents[2]
    parser.add_argument(
        "--data",
        type=Path,
        default=project_root / "open_stats" / "dyn_rd.json",
        help="Path to the dynamic RD statistics JSON file",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=project_root / "tools" / "dyn_bench" / "dyn_rd_curves.png",
        help="Output PNG file path",
    )
    parser.add_argument(
        "--dpi",
        type=int,
        default=250,
        help="DPI to use when saving the figure",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Display the figure interactively instead of saving",
    )
    return parser.parse_args()


def load_results(path: Path) -> OrderedDict[str, OrderedDict[str, dict]]:
    """Load the dynamic benchmark JSON into a nested OrderedDict structure.

    Parameters
    ----------
    path : Path
        Location of the JSON file describing method -> variant -> metrics.

    Returns
    -------
    OrderedDict[str, OrderedDict[str, dict]]
        Mapping from method name to an ordered mapping of variants, each storing
        the raw metric dictionaries from the JSON.

    Raises
    ------
    ValueError
        If the JSON file is empty.
    """
    with path.open("r", encoding="utf-8") as fp:
        raw = json.load(fp)
    if not raw:
        raise ValueError(f"Input JSON is empty: {path}")
    return OrderedDict((method, OrderedDict(variants.items())) for method, variants in raw.items())


def prepare_styles(methods: Iterable[str]) -> Dict[str, Tuple[str, str]]:
    """Assign plotting styles (color, marker) for every method label.

    Parameters
    ----------
    methods : Iterable[str]
        Sequence of method names to be rendered in the figure legend/curves.

    Returns
    -------
    Dict[str, Tuple[str, str]]
        Mapping of method name to `(color_hex, marker_symbol)` pairs, reusing the
        predefined palette and marker cycle.
    """
    styles: Dict[str, Tuple[str, str]] = {}
    methods_list = list(methods)

    # Slight color tweaks for the last two methods to improve separation.
    tweaked_last_two_colors = ["#228B22", "#A0522D"]  # forest green, sienna

    for idx, method in enumerate(methods_list):
        color = DEFAULT_COLORS[idx % len(DEFAULT_COLORS)]
        marker = DEFAULT_MARKERS[0]

        # For the last two methods, switch marker to square and tweak color.
        if idx >= max(0, len(methods_list) - 2):
            marker = "s"
            tweaked_idx = idx - (len(methods_list) - 2)
            if 0 <= tweaked_idx < len(tweaked_last_two_colors):
                color = tweaked_last_two_colors[tweaked_idx]

        styles[method] = (color, marker)
    return styles


def gather_method_order(results: OrderedDict[str, OrderedDict[str, dict]]) -> List[str]:
    """Return the display order of methods exactly as they appear in the JSON.

    Parameters
    ----------
    results : OrderedDict[str, OrderedDict[str, dict]]
        Parsed benchmark data keyed by method name.

    Returns
    -------
    List[str]
        Method names in the same insertion order as the source JSON.
    """
    return list(results.keys())


def filter_points_for_metric(
    variants: OrderedDict[str, dict], metric_key: str
) -> Tuple[List[float], List[float]]:
    """Collect bitrate/metric samples for one method metric, filtering invalid rows.

    Parameters
    ----------
    variants : OrderedDict[str, dict]
        Variants (rate points) belonging to a single method, each containing
        nested `quality_metrics` and `memory_metrics` dictionaries.
    metric_key : str
        Name of the quality metric to extract (e.g., "PSNR", "SSIM", "LPIPS").

    Returns
    -------
    Tuple[List[float], List[float]]
        Two equally sized lists: sorted bitrates (Mbps) and the corresponding
        metric values. Entries with missing/invalid data are skipped.
    """

    pairs: List[Tuple[float, float]] = []
    for variant_data in variants.values():
        metrics = variant_data.get("quality_metrics", {})
        memory = variant_data.get("memory_metrics", {})
        value = metrics.get(metric_key)
        bitrate = memory.get("bitrate(Mb/s)")
        if value is None or bitrate is None:
            continue
        if not isinstance(bitrate, (int, float)) or bitrate <= 0:
            continue
        pairs.append((float(bitrate), float(value)))
    pairs.sort(key=lambda item: item[0])
    if not pairs:
        return [], []
    bitrates, values = zip(*pairs)
    return list(bitrates), list(values)


def compute_limits(values: List[float], margin: float) -> Tuple[float, float]:
    """Expand plotting bounds slightly around observed values for aesthetics.

    Parameters
    ----------
    values : List[float]
        Sampled values for a given metric axis.
    margin : float
        Fractional padding relative to the value span.

    Returns
    -------
    Tuple[float, float]
        Lower and upper axis limits after applying padding. Falls back to
        `[0.0, 1.0]` when no values are present.
    """
    if not values:
        return 0.0, 1.0
    vmin, vmax = min(values), max(values)
    if vmin == vmax:
        pad = max(abs(vmin) * margin, 1e-3)
        return vmin - pad, vmax + pad
    span = vmax - vmin
    pad = span * margin
    return vmin - pad, vmax + pad


def plot_rd_curves(
    results: OrderedDict[str, OrderedDict[str, dict]],
    metric_order: List[Tuple[str, str]],
    styles: Dict[str, Tuple[str, str]],
    method_order: List[str],
    out_path: Path,
    dpi: int,
    show: bool,
) -> None:
    """Draw the RD figure containing one subplot per metric and save/display it.

    Parameters
    ----------
    results : OrderedDict[str, OrderedDict[str, dict]]
        Parsed benchmark hierarchy of methods and their variants with metrics.
    metric_order : List[Tuple[str, str]]
        Sequence of `(metric_key, axis_label)` tuples defining subplot order.
    styles : Dict[str, Tuple[str, str]]
        Mapping of method names to `(color, marker)` styling applied to plots.
    method_order : List[str]
        Original method order from JSON for consistent legend display.
    out_path : Path
        Destination file path for the exported PNG.
    dpi : int
        Resolution used when saving the figure.
    show : bool
        If True, show the matplotlib window instead of writing to disk.
    """
    fig, axes = plt.subplots(
        nrows=1,
        ncols=len(metric_order),
        figsize=(len(metric_order) * 3.8, 4.4),
        squeeze=False,
        gridspec_kw={"wspace": 0.25, "hspace": 0.32},
    )

    axes_row = axes[0]
    methods_with_points: set[str] = set()

    # Shared grid styling for all panels.
    grid_params = {
        "linestyle": "--",
        "alpha": 0.22,
        "color": "gray",
        "which": "both",
        "zorder": 0,
    }

    for axis, (metric_key, ylabel) in zip(axes_row, metric_order):
        all_bitrates: List[float] = []
        all_values: List[float] = []

        axis.set_axisbelow(True)
        axis.grid(True, **grid_params)
        axis.set_xscale("log")
        axis.set_xlabel("Bitrate (Mbps)")
        axis.set_ylabel(ylabel)
        
        # Format SSIM y-axis to show 2 decimal places and set tick interval to 0.01
        if metric_key == "SSIM":
            axis.yaxis.set_major_formatter(FormatStrFormatter("%.2f"))
            axis.yaxis.set_major_locator(MultipleLocator(0.01))

        for method, variants in results.items():
            bitrates, metric_vals = filter_points_for_metric(variants, metric_key)
            if not bitrates:
                continue

            color, marker = styles[method]
            if len(bitrates) > 1:
                axis.plot(
                    bitrates,
                    metric_vals,
                    color=color,
                    linestyle="-",
                    linewidth=1.6,
                    alpha=LINE_ALPHA,
                    zorder=2,
                )

            axis.scatter(
                bitrates,
                metric_vals,
                color=color,
                marker=marker,
                s=38,
                linewidth=0.0,
                alpha=MARKER_ALPHA,
                zorder=3,
                label=method,
            )

            methods_with_points.add(method)
            all_bitrates.extend(bitrates)
            all_values.extend(metric_vals)

        if all_bitrates:
            x_min = min(all_bitrates)
            x_max = max(all_bitrates)
            span = math.log10(x_max) - math.log10(x_min)
            pad = max(span * 0.1, 0.07)
            axis.set_xlim(10 ** (math.log10(x_min) - pad), 10 ** (math.log10(x_max) + pad))
        else:
            axis.set_xlim(0.1, 10.0)

        y_min, y_max = compute_limits(all_values, margin=0.10)
        axis.set_ylim(y_min, y_max)

    if methods_with_points:
        # Preserve the original JSON order for legend display.
        legend_methods = [m for m in method_order if m in methods_with_points]

        handles: List[Line2D] = []
        labels: List[str] = []
        for method in legend_methods:
            color, marker = styles[method]
            proxy = Line2D(
                [0],
                [0],
                color=color,
                marker=marker,
                linestyle="-",
                linewidth=1.8,
                markersize=6.5,
                alpha=MARKER_ALPHA,
            )
            handles.append(proxy)
            labels.append(method)

        legend = fig.legend(
            handles,
            labels,
            loc="upper center",
            ncol=4,
            frameon=True,
            bbox_to_anchor=(0.5, 1.02),
            borderpad=0.6,
            columnspacing=2.0,
            labelspacing=0.7,
            handlelength=2.6,
            handletextpad=0.8,
        )
        legend.get_frame().set_linewidth(0.8)
        legend.get_frame().set_edgecolor("#888888")

    fig.subplots_adjust(top=0.82, bottom=0.15, left=0.08, right=0.98)

    if show:
        plt.show()
    else:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(out_path, dpi=dpi, bbox_inches="tight")
        print(f"saved {out_path}")
    plt.close(fig)


def main() -> None:
    """Entry point for the CLI: parse args, load data, render/save the figure."""
    args = parse_args()
    results = load_results(args.data)
    method_order = gather_method_order(results)
    styles = prepare_styles(method_order)
    plot_rd_curves(
        results,
        METRIC_ORDER,
        styles,
        method_order,
        args.out,
        args.dpi,
        args.show,
    )


if __name__ == "__main__":
    main()


