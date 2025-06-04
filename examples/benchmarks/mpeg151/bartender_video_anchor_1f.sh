#!/bin/bash

# Define the list of GPU IDs to use
GPU_IDS=(0 1 2 7)  # You can modify this list, e.g., GPU_IDS=(0 2 5 7)

EXP_DIR=results/mpeg151/video_anchor_1f/bartender

# Function to run a single experiment
run_experiment() {
    local gpu_id=$1
    local rp_id=$2
    
    echo "Starting experiment rp${rp_id} on GPU ${gpu_id}"
    
    CUDA_VISIBLE_DEVICES=${gpu_id} python compress_ply_sequence.py x265_compression_rp${rp_id} \
        --data_factor 1 \
        --ply_dir /work/Users/lisicheng/Dataset/GSC_splats/m71763_bartender_stable/track \
        --data_dir /work/Users/lisicheng/Dataset/GSC_splats/m71763_bartender_stable/colmap_data \
        --result_dir ${EXP_DIR}/rp${rp_id} \
        --frame_num 32 \
        --gop_size 16 \
        --lpips_net vgg \
        --no-normalize_world_space \
        --scene_type GSC \
        --test_view_id {0..20} \
        --compression_cfg.use_sort \
        --compression_cfg.use_all_intra
    
    echo "Experiment rp${rp_id} started on GPU ${gpu_id}"
}

# Check if the number of GPUs is sufficient
if [ ${#GPU_IDS[@]} -lt 4 ]; then
    echo "Warning: Number of GPUs is less than the number of experiments, some experiments will be skipped"
fi

# Launch experiments in parallel
for i in {0..3}; do
    if [ $i -lt ${#GPU_IDS[@]} ]; then
        run_experiment ${GPU_IDS[$i]} $i &
        echo "Launched experiment rp${i} on GPU ${GPU_IDS[$i]} in background"
    else
        echo "Skipping experiment rp${i} due to insufficient GPUs"
    fi
done

# Wait for all background processes to complete
wait

echo "All experiments completed"

# Run the Python script to generate CSV after all experiments
python benchmarks/mpeg151/rate_distortion_stats_to_csv.py --exp-dir ${EXP_DIR}