#!/usr/bin/env python3
"""Merge experiment RD summaries into the consolidated statistics JSON.

## Overview
This utility ingests per-method experiment summaries (for example the JSON
produced by ``tools/summarize_rd_from_exps.py``) and transfers the averaged
metrics into the public ``open_stats`` tables used by the static benchmarks.
Each experiment file must expose a single top-level key whose value enumerates
rate-point variants (``rd_*``) containing an ``Average`` section.

## Features
- Extracts ``PSNR``, ``SSIM``, ``LPIPS``, and ``Mem.(MB)`` from each variant's
  ``Average`` block.
- Creates or updates the chosen method entry (defaults to ``new``) inside the
  requested dataset.
- Supports optional dry runs, custom variant prefixes, and alternate output
  destinations.
- Falls back to legacy ``results_<Dataset>`` sections if the modern dataset key
  is absent.

## Input Data
- ``input_json`` (positional): experiment summary with the layout:
  ``{"experiment": {"rd_0.001": {"Average": {...}}}}``.
- ``--existing_data``: statistics file to merge into (default:
  ``open_stats/static_rd.json``). The script updates the in-memory structure but
  writes the result to ``--output_json``.
- ``--dataset``: dataset block to target (for example ``Tanks_and_Temples``,
  ``MipNeRF_360``).
- ``--method_name``: method dictionary to populate (default ``new``).

## Output
- ``--output_json``: merged JSON (default ``open_stats/new_static_rd.json``).
  The original ``--existing_data`` file is left untouched unless both paths are
  the same.

## Usage Examples
```bash
# Preview planned updates without writing any file
python tools/merge_results.py my_results.json \
    --dataset Tanks_and_Temples \
    --existing_data open_stats/static_rd.json \
    --dry_run

# Merge and write to a fresh file while keeping the original stats intact
python tools/merge_results.py my_results.json \
    --dataset Tanks_and_Temples \
    --existing_data open_stats/static_rd.json \
    --output_json open_stats/new_static_rd.json

# Overwrite the consolidated statistics directly with a custom prefix
python tools/merge_results.py results/mip_summary.json \
    --dataset MipNeRF_360 \
    --existing_data open_stats/static_rd.json \
    --output_json open_stats/static_rd.json \
    --variant_prefix custom_mip_rd_lambda
```
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, Iterable, Tuple


AVERAGE_KEY = "Average"
METRICS_ORDER: Tuple[str, ...] = ("PSNR", "SSIM", "LPIPS", "Mem.(MB)")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Merge averaged rate-distortion metrics from an experiment JSON into "
            "the consolidated statistics file."
        )
    )
    parser.add_argument(
        "input_json",
        type=Path,
        help="Path to the experiment JSON file (e.g., my_results.json)",
    )
    parser.add_argument(
        "--dataset",
        required=True,
        help="Dataset name that matches the section in the existing statistics JSON.",
    )
    parser.add_argument(
        "--method_name",
        default="new",
        help="Method entry to update under the dataset (default: new).",
    )
    parser.add_argument(
        "--existing_data",
        type=Path,
        default=Path("open_stats/static_rd.json"),
        help="Path to the existing statistics JSON to read (default: open_stats/static_rd.json).",
    )
    parser.add_argument(
        "--output_json",
        type=Path,
        help="Output path for the merged JSON (default: open_stats/new_static_rd.json).",
    )
    parser.add_argument(
        "--variant_prefix",
        help=(
            "Custom prefix for generated variant keys. Defaults to "
            "'<method_name>_<dataset_token>_rd_lambda'."
        ),
    )
    parser.add_argument(
        "--dry_run",
        action="store_true",
        help="Show the planned updates without writing to disk.",
    )
    return parser.parse_args()


def load_json(path: Path) -> Dict:
    try:
        with path.open("r", encoding="utf-8") as fh:
            return json.load(fh)
    except FileNotFoundError:
        raise FileNotFoundError(f"JSON file not found: {path}") from None
    except json.JSONDecodeError as exc:
        raise ValueError(f"Failed to decode JSON at {path}: {exc}") from exc


def ensure_dataset_section(data: Dict, dataset: str) -> Tuple[Dict, str]:
    if dataset in data:
        return data[dataset], dataset

    legacy_key = f"results_{dataset}"
    if legacy_key in data:
        print(
            f"[warning] Dataset '{dataset}' not found. Using legacy key '{legacy_key}' instead.",
            file=sys.stderr,
        )
        return data[legacy_key], legacy_key

    data[dataset] = {}
    return data[dataset], dataset


def sanitise_dataset_token(dataset: str) -> str:
    token = dataset
    if token.startswith("results_"):
        token = token[len("results_") :]
    return token.replace(" ", "")


def build_variant_key(prefix: str, rate_label: str) -> str:
    suffix = rate_label.split("_", 1)[-1]
    return f"{prefix}_{suffix}"


def extract_average_values(variant_payload: Dict) -> Iterable[float]:
    average_section = variant_payload.get(AVERAGE_KEY)
    if not isinstance(average_section, dict):
        raise KeyError(f"Missing '{AVERAGE_KEY}' section or it is not a dict")

    values = []
    for metric in METRICS_ORDER:
        if metric not in average_section:
            raise KeyError(f"Missing metric '{metric}' in '{AVERAGE_KEY}' section")
        values.append(float(average_section[metric]))
    return values


def main() -> int:
    args = parse_args()

    input_json = args.input_json.resolve()
    existing_json = args.existing_data.resolve()
    output_json = (
        args.output_json.resolve()
        if args.output_json
        else Path("open_stats/new_static_rd.json").resolve()
    )

    experiment_data = load_json(input_json)
    if not isinstance(experiment_data, dict) or len(experiment_data) != 1:
        print(
            "[error] The experiment JSON must contain exactly one top-level key.",
            file=sys.stderr,
        )
        return 1

    (_, variants), = experiment_data.items()
    if not isinstance(variants, dict):
        print("[error] Variant payload must be a dictionary of rd_* entries.", file=sys.stderr)
        return 1

    if existing_json.exists():
        consolidated = load_json(existing_json)
    else:
        print(f"[info] Existing statistics JSON not found. Creating a new structure at {existing_json}.")
        consolidated = {}

    dataset_block, dataset_key = ensure_dataset_section(consolidated, args.dataset)
    if not isinstance(dataset_block, dict):
        print(
            f"[error] Dataset entry '{dataset_key}' must be a dictionary in the existing JSON.",
            file=sys.stderr,
        )
        return 1

    method_block = dataset_block.setdefault(args.method_name, {})
    if not isinstance(method_block, dict):
        print(
            f"[error] Method entry '{args.method_name}' under '{dataset_key}' must be a dict.",
            file=sys.stderr,
        )
        return 1

    prefix = args.variant_prefix
    if not prefix:
        dataset_token = sanitise_dataset_token(args.dataset)
        prefix = f"{args.method_name}_{dataset_token}_rd_lambda"

    updates = {}
    skipped = 0

    for rate_label, payload in variants.items():
        if not isinstance(payload, dict):
            print(f"[warning] Skipping '{rate_label}' because its payload is not a dict.")
            skipped += 1
            continue

        try:
            metric_values = list(extract_average_values(payload))
        except KeyError as exc:
            print(f"[warning] Skipping '{rate_label}': {exc}")
            skipped += 1
            continue

        variant_key = build_variant_key(prefix, rate_label)
        previous = method_block.get(variant_key)
        method_block[variant_key] = metric_values
        updates[variant_key] = {
            "previous": previous,
            "new": metric_values,
        }

    if not updates:
        print("[warning] No entries were merged. Nothing to do.")
        return 1

    print(f"[info] Prepared {len(updates)} update(s) for dataset '{dataset_key}' / method '{args.method_name}'.")
    for variant_key, diff in updates.items():
        if diff["previous"] is None:
            print(f"  [add] {variant_key} -> {diff['new']}")
        else:
            print(f"  [update] {variant_key} -> {diff['new']} (was {diff['previous']})")

    if skipped:
        print(f"[info] Skipped {skipped} variant(s) due to missing data.")

    if args.dry_run:
        print("[info] Dry run enabled; no file was written.")
        return 0

    if not output_json.parent.exists():
        output_json.parent.mkdir(parents=True, exist_ok=True)

    with output_json.open("w", encoding="utf-8") as fh:
        json.dump(consolidated, fh, indent=4)
        fh.write("\n")

    print(f"[info] Merged statistics written to {output_json}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

