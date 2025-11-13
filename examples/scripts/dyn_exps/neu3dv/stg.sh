SCENE_DIR="examples/data/neural_3d"

SCENE_LIST="coffee_martini cook_spinach cut_roasted_beef flame_salmon_1 flame_steak sear_steak" # SCENE_LIST="coffee_martini cook_spinach cut_roasted_beef flame_salmon_1 flame_steak sear_steak"

GPU_LIST=(0 1 2 3 4 5)

RESULT_DIR="results/stg_neu3d"

GOF_SIZE=50

DEFAULT_GPU_ID=${GPU_LIST[0]}

run_single_scene() {
    local rd_lambda="$1"
    local scene="$2"
    local gpu_id="${3:-$DEFAULT_GPU_ID}"

    # Training Phase
    CUDA_VISIBLE_DEVICES=$gpu_id python examples/simple_trainer_STG.py default \
        --model_path "$RESULT_DIR/default/$scene" \
        --data_dir "$SCENE_DIR/$scene" \
        --result_dir "$RESULT_DIR/default/$scene" \
        --gof_num $GOF_SIZE \
        --test_view_id 0 \
        --disable_viewer \

    # Compression Phase
    CUDA_VISIBLE_DEVICES=$gpu_id python examples/simple_trainer_STG.py default \
        --model_path "$RESULT_DIR/default/$scene" \
        --data_dir "$SCENE_DIR/$scene" \
        --result_dir "$RESULT_DIR/default/$scene" \
        --gof_num $GOF_SIZE \
        --test_view_id 0 \
        --disable_viewer \
        --compression_only \
        --ckpt_name "ckpt_best_rank0.pt" \
        --compression "stg"
}


idx=0
for scene in $SCENE_LIST; do
    gpu_id=${GPU_LIST[$((idx % ${#GPU_LIST[@]}))]}
    run_single_scene "$rd_lambda" "$scene" "$gpu_id" &
    idx=$((idx+1))
done
wait
echo "Finished running all scenes."
