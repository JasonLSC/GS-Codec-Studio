#! /bin/bash

### Examples of running the script

### Preprocess
python gs_ply_process.py \
    --preprocess \
    --raw_ply_path data/GSC_splats/m71763_bartender_stable/track/frame000.ply \
    --exp_dir results/gs_ply_process/bartender \

### Postprocess
python gs_ply_process.py \
    --postprocess \
    --quantized_ply_filename quantized.ply \
    --exp_dir results/gs_ply_process/bartender \
    --dequantized_ply_filename dequantized.ply

### A quick test to check PSNR between original and dequantized PLY
python gs_ply_process.py \
    --evaluate \
    --exp_dir results/gs_ply_process/bartender \
    --colmap_path data/GSC_splats/m71763_bartender_stable/colmap_data/frame000 \
    --raw_ply_path data/GSC_splats/m71763_bartender_stable/track/frame000.ply \

