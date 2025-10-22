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
        "Psnr YUV (avg)": None,
        "SSIM (avg)": None,
        "IVSSIM YUV": None,
    }
    
    patterns = {
        "Psnr RGB (avg)": re.compile(r"Psnr RGB \(avg\)\s+=\s+([\d\.]+)"),
        "Psnr YUV (avg)": re.compile(r"Psnr YUV \(avg\)\s+=\s+([\d\.]+)"),
        "SSIM (avg)": re.compile(r"SSIM \(avg\)\s+=\s+([\d\.]+)"),
        "IVSSIM YUV": re.compile(r"IVSSIM YUV\s+=\s+([\d\.]+)"),
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

def get_component_sizes_from_compression_folder(compression_path):
    """Aggregate sizes (in bytes) for each component in the compression folder.

    Components include: means, sh0, sh1, sh2, sh3, quats, scales, opacities, meta.
    Unknown files are counted into meta.
    """
    components = ['means', 'sh0', 'sh1', 'sh2', 'sh3', 'quats', 'scales', 'opacities', 'meta']
    sizes = {k: 0 for k in components}

    if not os.path.exists(compression_path):
        print(f"Warning: compression folder not found at {compression_path}")
        return sizes

    try:
        for root, _, files in os.walk(compression_path):
            for file in files:
                file_path = os.path.join(root, file)
                if not os.path.isfile(file_path):
                    continue
                size = os.path.getsize(file_path)
                name = file.lower()

                if 'means' in name:
                    sizes['means'] += size
                elif 'sh0' in name:
                    sizes['sh0'] += size
                elif 'sh1' in name:
                    sizes['sh1'] += size
                elif 'sh2' in name:
                    sizes['sh2'] += size
                elif 'sh3' in name:
                    sizes['sh3'] += size
                elif 'quat' in name:  # quats / quat
                    sizes['quats'] += size
                elif 'scale' in name:
                    sizes['scales'] += size
                elif 'opa' in name:  # opacity / opacities
                    sizes['opacities'] += size
                elif 'meta' in name or name.endswith('.json') or name.endswith('.txt') or 'index' in name or 'cfg' in name or 'log' in name:
                    sizes['meta'] += size
                else:
                    sizes['meta'] += size
    except Exception as e:
        print(f"Error while aggregating component sizes in {compression_path}: {e}")

    return sizes

def bytes_to_kbps(size_bytes, frame_num, fps=30):
    """Convert total bytes across a sequence to kbps at given fps and frame_num.
    kbps = (size_bytes / frame_num) * 8 * fps / 1024
    """
    if size_bytes == 'N/A' or size_bytes is None:
        return 'N/A'
    try:
        if frame_num <= 0:
            return 'N/A'
        return round((float(size_bytes) / frame_num) * 8 * fps / 1024, 3)
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
    headers = ["Rate Point", "Psnr RGB (avg)", "Psnr YUV (avg)", "SSIM (avg)", "IVSSIM YUV"]
    if args.get_bitrate:
        headers.extend(["Bitrate (kbps)", "Total Size", "Total Size (Bytes)", "Memory_per_frame (Bytes)"])
        # Append component breakdown columns
        component_order = ['means', 'sh0', 'sh1', 'sh2', 'sh3', 'quats', 'scales', 'opacities', 'meta']
        label_map = {
            'means': 'Means', 'sh0': 'SH0', 'sh1': 'SH1', 'sh2': 'SH2', 'sh3': 'SH3',
            'quats': 'Quats', 'scales': 'Scales', 'opacities': 'Opacities', 'meta': 'Meta'
        }
        # First, bytes-related columns for all components
        for comp in component_order:
            headers.extend([f"{label_map[comp]} Size"])
        for comp in component_order:
            headers.extend([f"{label_map[comp]} Size (Bytes)"])
        # Then, kbps-related columns for all components
        for comp in component_order:
            headers.extend([f"{label_map[comp]} Kbps"])

    # Assuming rate points are rp1, rp2, rp3, rp4
    for i in range(1,5):
        rp = f"rp{i}"
        log_file_path = os.path.join(scene_path, rp, "logs", "mpeg_gsc_metrics.log")
        
        metrics = parse_log_file(log_file_path)
        row = [
            rp, 
            metrics.get("Psnr RGB (avg)"), 
            metrics.get("Psnr YUV (avg)"), 
            metrics.get("SSIM (avg)"),
            metrics.get("IVSSIM YUV")
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

            # Append component breakdown values
            component_order = ['means', 'sh0', 'sh1', 'sh2', 'sh3', 'quats', 'scales', 'opacities', 'meta']
            comp_sizes = get_component_sizes_from_compression_folder(compression_path)
            # First, append all bytes-related values
            for comp in component_order:
                size_bytes = comp_sizes.get(comp, 0)
                row.extend([format_size(size_bytes)])
            for comp in component_order:
                size_bytes = comp_sizes.get(comp, 0)
                row.extend([size_bytes])
            # Then, append all kbps-related values
            for comp in component_order:
                size_bytes = comp_sizes.get(comp, 0)
                size_kbps = bytes_to_kbps(size_bytes, args.frame_num)
                row.extend([size_kbps])
        
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