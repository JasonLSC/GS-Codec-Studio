SCENE_DIR="data/neural_3d"
SCENE_LIST="coffee_martini cook_spinach cut_roasted_beef flame_salmon_1 flame_steak sear_steak"

RESULT_DIR="results/stg_neu3d"

NUM_FRAME=${NUM_FRAME:-50}
DEFAULT_TOTAL_FRAMES=${DEFAULT_TOTAL_FRAMES:-300}
DATA_SUBDIR=${DATA_SUBDIR:-colmap_0}

if (( NUM_FRAME <= 0 )); then
    echo "NUM_FRAME must be a positive integer"
    exit 1
fi

resolve_data_dir() {
    local base_path=$1
    local start_frame=$2
    local normalized=${base_path%/}

    if [[ $normalized =~ (.*colmap_)[0-9]+(.*)$ ]]; then
        echo "${BASH_REMATCH[1]}${start_frame}${BASH_REMATCH[2]}"
    else
        echo "${normalized}/colmap_${start_frame}"
    fi
}

get_base_start_frame() {
    local base_path=$1
    local normalized=${base_path%/}

    if [[ $normalized =~ colmap_([0-9]+) ]]; then
        echo "${BASH_REMATCH[1]}"
    else
        echo 0
    fi
}

get_total_frames() {
    local scene=$1
    local scene_upper=${scene^^}
    scene_upper=${scene_upper//-/_}
    scene_upper=${scene_upper// /_}
    local var_name=TOTAL_FRAMES_${scene_upper}
    local value=${!var_name}

    if [[ -z $value ]]; then
        echo $DEFAULT_TOTAL_FRAMES
    else
        echo $value
    fi
}

run_single_scene() {
    local GPU_ID=$1
    local SCENE=$2

    local gof=$NUM_FRAME
    local total_frames
    total_frames=$(get_total_frames $SCENE)

    local base_data_dir="${SCENE_DIR}/${SCENE}/${DATA_SUBDIR}"
    local base_start
    base_start=$(get_base_start_frame "$base_data_dir")

    echo "Running ${SCENE} | GOF=${gof} | total_frames=${total_frames} | base_start=${base_start}"

    CUDA_VISIBLE_DEVICES=$GPU_ID python simple_trainer_STG.py compression_sim \
        --model_path "$RESULT_DIR/$SCENE" \
        --data_dir "$base_data_dir" \
        --result_dir "$RESULT_DIR/$SCENE" \
        --duration "$gof" \
        --total_frames "$total_frames" \
        --data_start_frame "$base_start"

    local group_count=$(( (total_frames + gof - 1) / gof ))
    local gof_tag
    gof_tag=$(printf "gof_%03d" "$gof")

    for ((group_idx=0; group_idx<group_count; group_idx++)); do
        local rel_start=$(( group_idx * gof ))
        local frames=$gof
        if (( rel_start + frames > total_frames )); then
            frames=$(( total_frames - rel_start ))
        fi
        if (( frames <= 0 )); then
            continue
        fi

        local actual_start=$(( base_start + rel_start ))
        local data_dir
        data_dir=$(resolve_data_dir "$base_data_dir" "$actual_start")
        local group_tag
        group_tag=$(printf "group_%03d" "$group_idx")
        local group_root="$RESULT_DIR/$SCENE/$gof_tag/$group_tag"
        local ckpt_path="$group_root/ckpts/ckpt_best_rank0.pt"

        if [[ ! -f $ckpt_path ]]; then
            echo "  -> Skip ${group_tag}, missing ckpt at ${ckpt_path}" >&2
            continue
        fi

        echo "  -> Compress ${group_tag}: start=${actual_start}, frames=${frames}"
        CUDA_VISIBLE_DEVICES=$GPU_ID python simple_trainer_STG.py default \
            --model_path "$group_root" \
            --data_dir "$data_dir" \
            --result_dir "$group_root" \
            --duration "$frames" \
            --data_start_frame "$actual_start" \
            --lpips_net vgg \
            --compression stg \
            --ckpt "$ckpt_path"
    done
}

GPU_LIST=(0 1 2 3 4 5)
GPU_COUNT=${#GPU_LIST[@]}

SCENE_IDX=-1

for SCENE in $SCENE_LIST;
do
    SCENE_IDX=$((SCENE_IDX + 1))
    {
        if (( SCENE_IDX >= GPU_COUNT )); then
            echo "No GPU assigned for scene ${SCENE}" >&2
            exit 1
        fi
        run_single_scene ${GPU_LIST[$SCENE_IDX]} $SCENE
    } &

done

wait

if command -v zip &> /dev/null
then
    echo "Zipping results"
    python benchmarks/stg/summarize_stats.py --results_dir "$RESULT_DIR" --scenes $SCENE_LIST
else
    echo "zip command not found, skipping zipping"
fi
