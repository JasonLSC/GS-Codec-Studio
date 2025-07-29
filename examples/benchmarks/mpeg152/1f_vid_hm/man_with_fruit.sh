#!/bin/bash

# Define the list of GPU IDs to use
GPU_IDS=(0 1 2 3 4)  # You can modify this list, e.g., GPU_IDS=(0 2 5 7)

dataset=man_with_fruit
frame_num=1

EXP_DIR=results/mpeg152/1f_vid_hm/${dataset}

# Function to run a single experiment
run_experiment() {
    local gpu_id=$1
    local rp_id=$2
    
    echo "Starting experiment rp${rp_id} on GPU ${gpu_id}"
    
    CUDA_VISIBLE_DEVICES=${gpu_id} python codec_ply_sequence.py rp${rp_id} \
        --data_factor 1 \
        --ply_dir data/GSC_splats/m71903_bust_dataset/man_with_fruit/ply \
        --ply_filename data/GSC_splats/m71903_bust_dataset/man_with_fruit/ply/0081.ply \
        --data_dir data/GSC_splats/m71903_bust_dataset/man_with_fruit/colmap_data/000081 \
        --mask_dir data/GSC_splats/m71903_bust_dataset/man_with_fruit/colmap_data/000081/masks \
        --result_dir ${EXP_DIR}/rp${rp_id} \
        --frame_num ${frame_num} \
        --gop_size 16 \
        --lpips_net vgg \
        --no-normalize_world_space \
        --scene_type GSC \
        --test_view_id {0..23} \
        --compression_cfg.use_sort \
        --compression_cfg.sort_type plas \
        --compression_cfg.use_all_intra \
        --compression_cfg.video_codec_type hm \
        # --decode_only
    
    echo "Experiment rp${rp_id} started on GPU ${gpu_id}"
}

# Check if the number of GPUs is sufficient
if [ ${#GPU_IDS[@]} -lt 4 ]; then
    echo "Warning: Number of GPUs is less than the number of experiments, some experiments will be skipped"
fi

# Launch experiments in parallel
for i in {0..4}; do
    if [ $i -lt ${#GPU_IDS[@]} ]; then
        run_experiment ${GPU_IDS[$i]} $((i+1)) &
        echo "Launched experiment rp$((i+1)) on GPU ${GPU_IDS[$i]} in background"
    else
        echo "Skipping experiment rp$((i+1)) due to insufficient GPUs"
    fi
done

# Wait for all background processes to complete
wait

echo "All experiments completed"

# Run the Python script to generate CSV after all experiments
# python benchmarks/mpeg152/rate_distortion_stats_to_csv.py --exp-dir ${EXP_DIR}