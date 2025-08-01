import os
import pandas as pd
import argparse
from pathlib import Path
import glob

def merge_csv_files(root_dir, output_filename=None, output_format="excel", preview_rows=10):
    """
    Merge all scene CSV files in the root directory into one file.
    
    Args:
        root_dir: Root directory containing scene subdirectories
        output_filename: Output filename (optional)
        output_format: "excel" or "csv"
        preview_rows: Number of rows to show in preview (default: 10)
    """
    root_path = Path(root_dir)
    
    if not root_path.exists():
        print(f"Error: Directory {root_dir} does not exist.")
        return
    
    # Find all CSV files matching the pattern {scene_name}_metrics.csv
    csv_pattern = "*/*_metrics.csv"
    csv_files = list(root_path.glob(csv_pattern))
    
    if not csv_files:
        print(f"No CSV files found matching pattern '{csv_pattern}' in {root_dir}")
        return
    
    print(f"Found {len(csv_files)} CSV files:")
    for csv_file in csv_files:
        print(f"  {csv_file}")
    
    # Read and combine all CSV files
    all_dataframes = []
    
    for csv_file in csv_files:
        try:
            # Extract scene name from filename or parent directory
            scene_name = csv_file.stem.replace("_metrics", "")
            
            df = pd.read_csv(csv_file)
            
            # Add scene column if it doesn't exist
            if 'Scene' not in df.columns:
                df.insert(0, 'Scene', scene_name)
            
            all_dataframes.append(df)
            print(f"Successfully loaded {len(df)} rows from {scene_name}")
            
        except Exception as e:
            print(f"Error reading {csv_file}: {e}")
            continue
    
    if not all_dataframes:
        print("No valid CSV files were loaded.")
        return
    
    # Combine all dataframes
    combined_df = pd.concat(all_dataframes, ignore_index=True)
    
    # Determine output filename and format
    if output_filename:
        output_path = root_path / output_filename
    else:
        if output_format == "excel":
            output_path = root_path / "merged_metrics.xlsx"
        else:
            output_path = root_path / "merged_metrics.csv"
    
    # Write to file
    try:
        if output_format == "excel":
            try:
                combined_df.to_excel(output_path, index=False, sheet_name="All Scenes")
            except ImportError:
                print("Warning: openpyxl not installed. Saving as CSV instead.")
                output_path = root_path / "merged_metrics.csv"
                combined_df.to_csv(output_path, index=False)
        else:
            combined_df.to_csv(output_path, index=False)
            
        print(f"\nSuccessfully merged {len(combined_df)} rows into: {output_path}")
        
        # Show preview
        print(f"\nPreview of merged data (first {min(preview_rows, len(combined_df))} rows):")
        print(combined_df.head(preview_rows).to_string(index=False))
        
        # Show tail if there are more rows than preview
        if len(combined_df) > preview_rows:
            print(f"\nLast {min(preview_rows, len(combined_df))} rows:")
            print(combined_df.tail(preview_rows).to_string(index=False))
        
        # Show summary by scene
        if 'Scene' in combined_df.columns:
            scene_counts = combined_df['Scene'].value_counts()
            print(f"\nData points per scene:")
            for scene, count in scene_counts.items():
                print(f"  {scene}: {count} rows")
                
    except Exception as e:
        print(f"Error writing file: {e}")

def main():
    parser = argparse.ArgumentParser(description="Merge scene CSV files into one file.")
    parser.add_argument("root_dir", help="Root directory containing scene subdirectories with CSV files")
    parser.add_argument("--output", "-o", help="Output filename")
    parser.add_argument("--format", choices=["excel", "csv"], default="excel", 
                       help="Output format (default: excel)")
    parser.add_argument("--preview", "-p", type=int, default=20,
                       help="Number of rows to show in preview (default: 20)")
    
    args = parser.parse_args()
    
    merge_csv_files(args.root_dir, args.output, args.format, args.preview)

if __name__ == "__main__":
    main() 