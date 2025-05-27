import json
import csv
import os
from typing import Annotated
import tyro

def main(
    exp_dir: Annotated[str, "Path to the experiment directory, e.g. results/mpeg151/video_anchor/bartender"]
):
    # File paths for four rate points
    all_rps = ["rp0", "rp1", "rp2", "rp3"]
    metrics_paths = [
        os.path.join(exp_dir, f"{rp}/stats/gsc_metrics.json") for rp in all_rps
    ]
    summary_paths = [
        os.path.join(exp_dir, f"{rp}/summary.json") for rp in all_rps
    ]

    # Load all data for each rate point
    all_data = []
    for path in metrics_paths:
        with open(path, "r") as f:
            all_data.append(json.load(f))

    # Get all test_view keys (assuming all files have the same order)
    all_v_ids = [k for k in all_data[0].keys() if k.startswith("testv")]

    # Extract bitrate for each rp (in bps)
    bitrates = []
    for path in summary_paths:
        with open(path, "r") as f:
            data = json.load(f)
            bitrate_kbps = data["bitrate"]
            bitrates.append(bitrate_kbps)

    # Write combined CSV
    header = ["view", "rate_point", "RGB_PSNR", "YUV_PSNR", "YUV_SSIM", "YUV_IVSSIM", "LPIPS", "bitrate(kbps)"]
    csv_path = os.path.join(exp_dir, "gsc_metrics_all.csv")
    with open(csv_path, "w", newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(header)
        for v_id in all_v_ids:
            for rp_idx, rp in enumerate(all_rps):
                metrics = all_data[rp_idx][v_id]
                row = [
                    v_id, rp,
                    metrics["RGB_PSNR"], metrics["YUV_PSNR"], metrics["YUV_SSIM"], metrics["YUV_IVSSIM"], metrics["LPIPS"],
                    bitrates[rp_idx]
                ]
                writer.writerow(row)
            # Add empty rows for the missing rate points
            if rp_idx < 4:
                for _ in range(4 - rp_idx):
                    writer.writerow([])

    print(f"gsc_metrics_all.csv has been generated at {csv_path} and can be opened with Excel.")

if __name__ == "__main__":
    tyro.cli(main) 