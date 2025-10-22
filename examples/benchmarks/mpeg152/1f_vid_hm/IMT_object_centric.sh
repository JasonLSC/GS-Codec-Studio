#!/bin/bash

# Define the list of GPU IDs to use
GPU_IDS=(0 1 2 3 4 5 6 7)  # You can modify this list, e.g., GPU_IDS=(0 2 5 7)

dataset_list=("LEGO_Bugatti" "LEGO_Ferrari" "Plant" "Tennis_player" "Cricket_player" "Tango_duo" "Solo_Tango_Female" "Solo_Tango_Male")

frame_num=1

# Function to run a single experiment
run_experiment() {
    local gpu_id=$1
    local rp_id=$2
    local dataset=$3

    echo "Starting scene ${dataset} experiment rp${rp_id} on GPU ${gpu_id}"

    EXP_DIR=results/mpeg152/1f_vid_hm/${dataset}
    
    CUDA_VISIBLE_DEVICES=${gpu_id} python codec_ply_sequence.py rp${rp_id} \
        --data_factor 1 \
        --ply_dir data/GSC_splats/m73251_Humans_and_Objects_1f/${dataset} \
        --ply_filename data/GSC_splats/m73251_Humans_and_Objects_1f/${dataset}/${dataset}_GS.ply \
        --data_dir data/GSC_splats/m73251_Humans_and_Objects_1f/${dataset}/colmap_SFM \
        --mask_dir data/GSC_splats/m73251_Humans_and_Objects_1f/${dataset}/colmap_SFM/masks \
        --result_dir ${EXP_DIR}/rp${rp_id} \
        --frame_num ${frame_num} \
        --gop_size 16 \
        --no-with_lpips \
        --lpips_net vgg \
        --no-normalize_world_space \
        --scene_type GSC \
        --test_view_id all \
        --compression_cfg.use_sort \
        --compression_cfg.sort_type plas \
        --compression_cfg.use_all_intra \
        --compression_cfg.video_codec_type hm
    
    echo "Finished scene ${dataset} experiment rp${rp_id} on GPU ${gpu_id}"
}

# Function to run all rp experiments for a single dataset sequentially
run_dataset_experiments() {
    local gpu_id=$1
    local dataset=$2

    echo "Starting all ratepoints experiments for dataset ${dataset} on GPU ${gpu_id}"
    
    # Run rp0 to rp3 sequentially for this dataset
    for rp_id in {0..3}; do
        run_experiment ${gpu_id} ${rp_id} ${dataset}
    done
    
    echo "Completed all ratepoints experiments for dataset ${dataset} on GPU ${gpu_id}"

    EXP_DIR=results/mpeg152/1f_vid_hm/${dataset}
    python benchmarks/mpeg152/rate_distortion_stats_to_csv.py --exp-dir ${EXP_DIR}
}

# Launch dataset experiments in parallel (one dataset per GPU)
for i in {0..7}; do
    if [ $i -lt ${#dataset_list[@]} ] && [ $i -lt ${#GPU_IDS[@]} ]; then
        run_dataset_experiments ${GPU_IDS[$i]} ${dataset_list[$i]} &
        echo "Launched dataset ${dataset_list[$i]} experiments on GPU ${GPU_IDS[$i]} in background"
    else
        echo "Skipping dataset index ${i} due to insufficient datasets or GPUs"
    fi
done

# Wait for all background processes to complete
wait

echo "All dataset experiments completed"