#!/usr/bin/env python3
"""
Summarize Rate-Distortion Results from Experiment Directories

This script processes Gaussian Splatting compression experiment results and generates 
a comprehensive JSON summary containing quality metrics and memory usage for different 
rate points and scenes.

## Overview
The script analyzes experiment directories with the following structure:
```
experiment_dir/
├── rd_0.001/
│   ├── train/
│   │   ├── stats/
│   │   │   └── post_training_compress_step29999.json
│   │   └── post_training_compression/
│   │       └── step29999_rank0/
│   │           └── compressed_data/
│   └── truck/
│       ├── stats/
│       │   └── post_training_compress_step29999.json
│       └── post_training_compression/
│           └── step29999_rank0/
│               └── compressed_data/
├── rd_0.005/
│   └── ...
└── rd_0.1/
    └── ...
```

## Features
- **Automatic Discovery**: Automatically finds all rate point directories (rd_*)
- **Quality Metrics Extraction**: Extracts PSNR, SSIM, and LPIPS from JSON stats files
- **Memory Usage Calculation**: Calculates total compressed data size in MB
- **Average Computation**: Computes average metrics across all scenes for each rate point
- **Structured Output**: Generates well-formatted JSON with hierarchical organization

## Input Data Sources
1. **Quality Metrics**: Read from `stats/post_training_compress_*.json` files
   - PSNR (Peak Signal-to-Noise Ratio)
   - SSIM (Structural Similarity Index)
   - LPIPS (Learned Perceptual Image Patch Similarity)
   
2. **Memory Usage**: Calculated from `post_training_compression/step29999_rank0/compressed_data/` directory
   - Sums all file sizes in the compressed data folder
   - Converts to MB for easy interpretation

## Output Format
The script generates a JSON file with the following structure:
```json
{
  "experiment_name": {
    "rd_0.1": {
      "train": {
        "PSNR": 20.59,
        "SSIM": 0.717,
        "LPIPS": 0.345,
        "Mem.(MB)": 4.68
      },
      "truck": {
        "PSNR": 23.82,
        "SSIM": 0.805,
        "LPIPS": 0.274,
        "Mem.(MB)": 4.59
      },
      "Average": {
        "PSNR": 22.21,
        "SSIM": 0.761,
        "LPIPS": 0.309,
        "Mem.(MB)": 4.64
      }
    }
  }
}
```

## Usage Examples
```bash
# Basic usage - process experiment directory
python tools/summarize_rd_from_exps.py results/tt_mcmc_comp_sim

# Specify custom output file
python tools/summarize_rd_from_exps.py results/tt_mcmc_comp_sim -o my_summary.json

# Process different experiment
python tools/summarize_rd_from_exps.py results/another_experiment -o results.json
```

## Error Handling
- **Missing Directories**: Warns about missing stats or compressed data directories
- **Invalid JSON**: Handles corrupted or missing JSON files gracefully
- **File Access**: Continues processing even if some files are inaccessible
- **Empty Results**: Provides default values (0.0) for missing metrics

## Requirements
- Python 3.6+
- Standard library modules: os, json, argparse, pathlib
- No external dependencies required

"""

import os
import json
import argparse
from pathlib import Path
from typing import Dict, Any, List


def calculate_directory_size(directory_path: str) -> float:
    """
    Calculate the total size of all files in a directory in MB.
    
    Args:
        directory_path: Path to the directory to calculate size for
        
    Returns:
        Total size in MB
    """
    total_size = 0
    if not os.path.exists(directory_path):
        return 0.0
        
    for root, dirs, files in os.walk(directory_path):
        for file in files:
            file_path = os.path.join(root, file)
            try:
                total_size += os.path.getsize(file_path)
            except (OSError, FileNotFoundError):
                continue
    
    return total_size / (1024 * 1024)  # Convert to MB


def load_metrics_from_json(json_path: str) -> Dict[str, float]:
    """
    Load PSNR, SSIM, and LPIPS metrics from a JSON file.
    
    Args:
        json_path: Path to the JSON file containing metrics
        
    Returns:
        Dictionary containing PSNR, SSIM, and LPIPS values
    """
    try:
        with open(json_path, 'r') as f:
            data = json.load(f)
        
        return {
            "PSNR": data.get("psnr", 0.0),
            "SSIM": data.get("ssim", 0.0),
            "LPIPS": data.get("lpips", 0.0)
        }
    except (FileNotFoundError, json.JSONDecodeError, KeyError) as e:
        print(f"Warning: Could not load metrics from {json_path}: {e}")
        return {"PSNR": 0.0, "SSIM": 0.0, "LPIPS": 0.0}


def process_scene(scene_path: str, scene_name: str) -> Dict[str, Any]:
    """
    Process a single scene directory to extract metrics and memory usage.
    
    Args:
        scene_path: Path to the scene directory
        scene_name: Name of the scene
        
    Returns:
        Dictionary containing scene metrics
    """
    # Look for the stats JSON file
    stats_dir = os.path.join(scene_path, "stats")
    json_file = None
    
    if os.path.exists(stats_dir):
        # Find the post_training_compress JSON file
        for file in os.listdir(stats_dir):
            if file.startswith("post_training_compress") and file.endswith(".json"):
                json_file = os.path.join(stats_dir, file)
                break
    
    if not json_file:
        print(f"Warning: No post_training_compress JSON found in {stats_dir}")
        return {"PSNR": 0.0, "SSIM": 0.0, "LPIPS": 0.0, "Mem.(MB)": 0.0}
    
    # Load metrics from JSON
    metrics = load_metrics_from_json(json_file)
    
    # Calculate memory usage from compressed_data directory
    compressed_data_path = os.path.join(scene_path, "post_training_compression", "step29999_rank0", "compressed_data")
    memory_mb = calculate_directory_size(compressed_data_path)
    
    metrics["Mem.(MB)"] = round(memory_mb, 2)
    
    return metrics


def calculate_average_metrics(scene_metrics: Dict[str, Dict[str, float]]) -> Dict[str, float]:
    """
    Calculate average metrics across all scenes.
    
    Args:
        scene_metrics: Dictionary containing metrics for each scene
        
    Returns:
        Dictionary containing average metrics
    """
    if not scene_metrics:
        return {"PSNR": 0.0, "SSIM": 0.0, "LPIPS": 0.0, "Mem.(MB)": 0.0}
    
    # Exclude "Average" from calculation if it exists
    scenes_to_average = {k: v for k, v in scene_metrics.items() if k != "Average"}
    
    if not scenes_to_average:
        return {"PSNR": 0.0, "SSIM": 0.0, "LPIPS": 0.0, "Mem.(MB)": 0.0}
    
    averages = {}
    for metric in ["PSNR", "SSIM", "LPIPS", "Mem.(MB)"]:
        values = [scene[metric] for scene in scenes_to_average.values() if metric in scene]
        if values:
            averages[metric] = round(sum(values) / len(values), 4)
        else:
            averages[metric] = 0.0
    
    return averages


def process_rate_point(rate_point_path: str, rate_point_name: str) -> Dict[str, Dict[str, Any]]:
    """
    Process a single rate point directory containing multiple scenes.
    
    Args:
        rate_point_path: Path to the rate point directory
        rate_point_name: Name of the rate point (e.g., "rd_0.1")
        
    Returns:
        Dictionary containing metrics for all scenes in this rate point
    """
    rate_point_metrics = {}
    
    # Find all scene directories
    for item in os.listdir(rate_point_path):
        scene_path = os.path.join(rate_point_path, item)
        if os.path.isdir(scene_path):
            print(f"Processing scene: {item}")
            rate_point_metrics[item] = process_scene(scene_path, item)
    
    # Calculate average metrics
    rate_point_metrics["Average"] = calculate_average_metrics(rate_point_metrics)
    
    return rate_point_metrics


def summarize_rd_from_exps(experiment_dir: str, output_file: str = None) -> Dict[str, Any]:
    """
    Main function to summarize rate-distortion results from experiment directory.
    
    Args:
        experiment_dir: Path to the experiment directory (e.g., "results/tt_mcmc_comp_sim")
        output_file: Optional output file path for JSON results
        
    Returns:
        Dictionary containing summarized results
    """
    experiment_path = Path(experiment_dir)
    if not experiment_path.exists():
        raise ValueError(f"Experiment directory does not exist: {experiment_dir}")
    
    experiment_name = experiment_path.name
    results = {experiment_name: {}}
    
    # Find all rate point directories (rd_*)
    rate_point_dirs = [d for d in os.listdir(experiment_dir) 
                      if os.path.isdir(os.path.join(experiment_dir, d)) and d.startswith("rd_")]
    
    rate_point_dirs.sort()  # Sort for consistent ordering
    
    for rate_point_dir in rate_point_dirs:
        rate_point_path = os.path.join(experiment_dir, rate_point_dir)
        print(f"Processing rate point: {rate_point_dir}")
        
        rate_point_metrics = process_rate_point(rate_point_path, rate_point_dir)
        results[experiment_name][rate_point_dir] = rate_point_metrics
    
    # Save results to JSON file
    if output_file:
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2)
        print(f"Results saved to: {output_file}")
    
    return results


def main():
    """Command line interface for the script."""
    parser = argparse.ArgumentParser(description="Summarize rate-distortion results from experiment directories")
    parser.add_argument("experiment_dir", 
                       help="Path to experiment directory (e.g., results/tt_mcmc_comp_sim) - REQUIRED")
    parser.add_argument("-o", "--output", 
                       help="Output JSON file path (default: experiment_name_summary.json)")
    
    args = parser.parse_args()
    
    # Generate default output filename if not provided
    if not args.output:
        experiment_name = Path(args.experiment_dir).name
        args.output = f"{experiment_name}_summary.json"
    
    try:
        results = summarize_rd_from_exps(args.experiment_dir, args.output)
        print("Summary completed successfully!")
        print(f"Processed {len(results[list(results.keys())[0]])} rate points")
    except Exception as e:
        print(f"Error: {e}")
        return 1
    
    return 0


if __name__ == "__main__":
    exit(main())
