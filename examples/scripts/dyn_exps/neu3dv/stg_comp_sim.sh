SCENE_DIR="examples/data/neural_3d"

SCENE_LIST="coffee_martini cook_spinach cut_roasted_beef flame_salmon_1 flame_steak sear_steak" # SCENE_LIST="coffee_martini cook_spinach cut_roasted_beef flame_salmon_1 flame_steak sear_steak"

GPU_LIST=(0 1 2 3 4 5)

RESULT_DIR="results/stg_neu3d_comp_sim"

GOF_SIZE=50

RD_LAMBDA=(0.005 0.01 0.02) # refers to 3 ratepoints

DEFAULT_GPU_ID=${GPU_LIST[0]}

run_single_scene() {
    local rd_lambda="$1"
    local scene="$2"
    local gpu_id="${3:-$DEFAULT_GPU_ID}"

    # Training Phase
    CUDA_VISIBLE_DEVICES=$gpu_id python examples/simple_trainer_STG.py compression_sim \
        --model_path "$RESULT_DIR/rd_${rd_lambda}/$scene" \
        --data_dir "$SCENE_DIR/$scene" \
        --result_dir "$RESULT_DIR/rd_${rd_lambda}/$scene" \
        --gof_num $GOF_SIZE \
        --test_view_id 0 \
        --disable_viewer \
        --entropy_model_opt --rd_lambda $rd_lambda # enable entropy model with rd_lambda 0.01

    # Compression Phase
    CUDA_VISIBLE_DEVICES=$gpu_id python examples/simple_trainer_STG.py default \
        --model_path "$RESULT_DIR/rd_${rd_lambda}/$scene" \
        --data_dir "$SCENE_DIR/$scene" \
        --result_dir "$RESULT_DIR/rd_${rd_lambda}/$scene" \
        --gof_num $GOF_SIZE \
        --test_view_id 0 \
        --disable_viewer \
        --compression_only \
        --ckpt_name "ckpt_best_rank0.pt" \
        --compression "stg"
}

for rd_lambda in "${RD_LAMBDA[@]}"; do
    idx=0
    for scene in $SCENE_LIST; do
        gpu_id=${GPU_LIST[$((idx % ${#GPU_LIST[@]}))]}
        run_single_scene "$rd_lambda" "$scene" "$gpu_id" &
        idx=$((idx+1))
    done
    wait
    echo "Finished running $rd_lambda on all scenes."
done