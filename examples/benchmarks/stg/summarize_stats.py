import json
import os
import subprocess
from collections import defaultdict
from typing import Dict, List

import numpy as np
import torch
import tyro
import yaml


def _list_subdirs(path: str, prefix: str) -> List[str]:
    if not os.path.isdir(path):
        return []
    return sorted(
        [name for name in os.listdir(path) if name.startswith(prefix) and os.path.isdir(os.path.join(path, name))]
    )


def _load_best_step(group_root: str) -> int:
    ckpt_path = os.path.join(group_root, "ckpts", "ckpt_best_rank0.pt")
    if not os.path.exists(ckpt_path):
        raise FileNotFoundError(f"Checkpoint not found: {ckpt_path}")

    try:
        ckpt = torch.load(ckpt_path, map_location="cpu", weights_only=True)
    except TypeError:
        ckpt = torch.load(ckpt_path, map_location="cpu")
    return ckpt["step"]


def _load_group_cfg(group_root: str) -> Dict:
    cfg_path = os.path.join(group_root, "cfg.yml")
    if os.path.exists(cfg_path):
        with open(cfg_path, "r") as f:
            return yaml.safe_load(f) or {}
    return {}


def _zip_compression(group_root: str) -> int:
    compression_dir = os.path.join(group_root, "compression")
    if not os.path.isdir(compression_dir):
        raise FileNotFoundError(f"Compression directory not found: {compression_dir}")

    zip_path = os.path.join(group_root, "compression.zip")
    if os.path.exists(zip_path):
        os.remove(zip_path)

    subprocess.run(
        f"zip -r {zip_path} {compression_dir}/",
        shell=True,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    out = subprocess.run(
        f"stat -c%s {zip_path}",
        shell=True,
        check=True,
        capture_output=True,
        text=True,
    )
    return int(out.stdout.strip())


def _accumulate(summary: Dict[str, Dict[str, List[float]]], key: str, metric: str, value: float) -> None:
    summary.setdefault(key, defaultdict(list))[metric].append(value)


def main(results_dir: str, scenes: List[str]) -> None:
    summary: Dict[str, Dict[str, List[float]]] = {}

    for scene in scenes:
        scene_dir = os.path.join(results_dir, scene)
        gof_dirs = _list_subdirs(scene_dir, "gof_")
        if not gof_dirs:
            print(f"No GOF outputs found for scene {scene}, skipping")
            continue

        for gof_dir in gof_dirs:
            gof_path = os.path.join(scene_dir, gof_dir)
            group_dirs = _list_subdirs(gof_path, "group_")
            if not group_dirs:
                print(f"Scene {scene} GOF {gof_dir} has no group directories, skipping")
                continue

            key = f"{scene}/{gof_dir}"

            for group_dir in group_dirs:
                group_root = os.path.join(gof_path, group_dir)
                try:
                    step = _load_best_step(group_root)
                except FileNotFoundError as exc:
                    print(f"{exc}, skipping {group_root}")
                    continue

                cfg = _load_group_cfg(group_root)
                group_frames = cfg.get("group_frames") or cfg.get("duration")
                if not group_frames:
                    raise ValueError(f"Unable to determine frame count for {group_root}")

                # Compress stage metrics
                compression_dir = os.path.join(group_root, "compression")
                if os.path.isdir(compression_dir):
                    try:
                        size = _zip_compression(group_root)
                    except FileNotFoundError:
                        size = None
                    if size is not None:
                        bitrate = size * 8 / 1024 ** 2 / group_frames * 30
                        mb_per_frame = size / 1024 ** 2 / group_frames
                        _accumulate(summary, key, "size", size)
                        _accumulate(summary, key, "bitrate", bitrate)
                        _accumulate(summary, key, "MB_per_frame", mb_per_frame)

                compress_stats_path = os.path.join(group_root, "stats", f"compress_step{step}.json")
                if os.path.exists(compress_stats_path):
                    with open(compress_stats_path, "r") as f:
                        stats = json.load(f)
                    for metric, value in stats.items():
                        _accumulate(summary, key, f"compress_{metric}", value)

                # Validation metrics
                val_stats_path = os.path.join(group_root, "stats", f"val_step{step}.json")
                if os.path.exists(val_stats_path):
                    with open(val_stats_path, "r") as f:
                        stats = json.load(f)
                    for metric in ["psnr", "ssim", "lpips"]:
                        if metric in stats:
                            _accumulate(summary, key, f"val_{metric}", stats[metric])

    for key, metrics in summary.items():
        print(f"=== {key} ===")
        for metric, values in metrics.items():
            print(f"  {metric}: {np.mean(values):.4f}")

    mean_summary = {key: {metric: float(np.mean(values)) for metric, values in metrics.items()} for key, metrics in summary.items()}

    output_path = os.path.join(results_dir, "comp_summary.json")
    with open(output_path, "w") as fp:
        json.dump(mean_summary, fp, indent=2)
    print(f"Saved summary to {output_path}")


if __name__ == "__main__":
    tyro.cli(main)
