import json
import os
from pathlib import Path

def merge_summaries(base_dir):
    # Create the nested dictionary structure
    merged_data = {
        base_dir: {}
    }
    
    # Iterate through all rp directories
    for i in range(4):  # Assuming rp0 to rp3
        rp_dir = os.path.join(base_dir, f'rp{i}')
        summary_path = os.path.join(rp_dir, 'summary.json')
        
        if os.path.exists(summary_path):
            with open(summary_path, 'r') as f:
                summary_data = json.load(f)
                merged_data[base_dir][f'rp{i}'] = summary_data
    
    # Save the merged data
    output_path = os.path.join(base_dir, 'all_rp_summary.json')
    with open(output_path, 'w') as f:
        json.dump(merged_data, f, indent=4)

if __name__ == '__main__':
    import sys
    if len(sys.argv) > 1:
        base_dir = sys.argv[1]
        merge_summaries(base_dir)
    else:
        print("Please provide the base directory path") 