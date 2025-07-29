import os
import re
import csv
import argparse
import json

def format_size(size_bytes):
    """Convert byte size to readable format (KB, MB, GB, etc.)"""
    if size_bytes == 'N/A':
        return 'N/A'
    try:
        size_bytes = float(size_bytes)
        if size_bytes < 1024:
            return f"{size_bytes:.0f} B"
        elif size_bytes < 1024**2:
            return f"{size_bytes/1024:.2f} KB"
        elif size_bytes < 1024**3:
            return f"{size_bytes/(1024**2):.2f} MB"
        else:
            return f"{size_bytes/(1024**3):.2f} GB"
    except (ValueError, TypeError):
        return 'N/A'

def parse_log_file(file_path):
    """Parses a log file to extract metrics."""
    metrics = {
        "Psnr RGB (avg)": None,
        "Psnr YUV (wavg)": None,
        "SSIM Avg": None,
    }
    
    patterns = {
        "Psnr RGB (avg)": re.compile(r"Psnr RGB \(avg\)\s+=\s+([\d\.]+)"),
        "Psnr YUV (wavg)": re.compile(r"Psnr YUV \(wavg\)\s+=\s+([\d\.]+)"),
        "SSIM Avg": re.compile(r"SSIM Avg\s+=\s+([\d\.]+)"),
    }

    if not os.path.exists(file_path):
        print(f"Warning: Log file not found at {file_path}")
        return {key: 'N/A' for key in metrics}

    with open(file_path, 'r') as f:
        content = f.read()
        for key, pattern in patterns.items():
            match = pattern.search(content)
            if match:
                metrics[key] = float(match.group(1))

    return metrics

def get_bitrate_from_compression_folder(compression_path, frame_num):
    """Calculate bitrate from compression folder size and frame number."""
    if not os.path.exists(compression_path):
        print(f"Warning: compression folder not found at {compression_path}")
        return 'N/A', 'N/A'
    
    try:
        # Calculate total size of all files in compression folder
        total_size = 0
        for root, dirs, files in os.walk(compression_path):
            for file in files:
                file_path = os.path.join(root, file)
                if os.path.isfile(file_path):
                    total_size += os.path.getsize(file_path)
        
        if total_size == 0:
            return 'N/A', 'N/A'
        
        # Calculate bitrate: total_size / frame_num * 8 * 30 / 1024 (kbps)
        # This matches the formula in codec_ply_sequence.py
        bitrate = total_size / 1024 / frame_num * 8 * 30
        
        return round(bitrate, 3), total_size
        
    except Exception as e:
        print(f"Error calculating bitrate from {compression_path}: {e}")
        return 'N/A', 'N/A'

def calculate_memory_per_frame(bitrate):
    """Calculate memory per frame in bytes from bitrate in kbps."""
    if bitrate == 'N/A' or bitrate is None:
        return 'N/A'
    try:
        # Convert kbps to bytes per frame (assuming 30fps)
        # kbps -> bps -> Bps -> bytes per frame
        memory_per_frame = (float(bitrate) * 1024) / (8 * 30)
        return round(memory_per_frame, 2)
    except (ValueError, TypeError):
        return 'N/A'

def main():
    """Main function to process a scene and generate CSV."""
    parser = argparse.ArgumentParser(description="Extract metrics from log files for a given scene.")
    parser.add_argument("scene_dir", help="The directory of the scene to process.")
    parser.add_argument("--get_bitrate", action="store_true", help="Include bitrate from compression folder in the output.")
    parser.add_argument("--frame_num", type=int, default=1, help="Number of frames for bitrate calculation (default: 1)")
    args = parser.parse_args()

    scene_path = args.scene_dir
    if not os.path.isdir(scene_path):
        print(f"Error: Directory not found at {scene_path}")
        return

    scene_name = os.path.basename(os.path.normpath(scene_path))
    output_csv_path = os.path.join(scene_path, f"{scene_name}_metrics.csv")
    
    results = []
    headers = ["Rate Point", "Psnr RGB (avg)", "Psnr YUV (wavg)", "SSIM Avg"]
    if args.get_bitrate:
        headers.extend(["Bitrate (kbps)", "Total Size", "Total Size (Bytes)", "Memory_per_frame (Bytes)"])

    # Assuming rate points are rp0, rp1, rp2, rp3
    for i in range(1,6):
        rp = f"rp{i}"
        log_file_path = os.path.join(scene_path, rp, "logs", "mpeg_gsc_metrics.log")
        
        metrics = parse_log_file(log_file_path)
        row = [
            rp, 
            metrics.get("Psnr RGB (avg)"), 
            metrics.get("Psnr YUV (wavg)"), 
            metrics.get("SSIM Avg")
        ]

        if args.get_bitrate:
            compression_path = os.path.join(scene_path, rp, "compression")
            bitrate, total_size = get_bitrate_from_compression_folder(compression_path, args.frame_num)
            # Only calculate memory_per_frame when frame_num == 1, otherwise set to 'N/A'
            if args.frame_num == 1:
                memory_per_frame = total_size
            else:
                memory_per_frame = 'N/A'
            total_size_formatted = format_size(total_size)
            row.extend([bitrate, total_size_formatted, total_size, memory_per_frame])
        
        results.append(row)

    with open(output_csv_path, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(headers)
        writer.writerows(results)
        
    print(f"Generated metrics CSV for scene '{scene_name}' at: {output_csv_path}")
    print("\nCSV content:")
    with open(output_csv_path, 'r') as csvfile:
        print(csvfile.read())

if __name__ == "__main__":
    main() 