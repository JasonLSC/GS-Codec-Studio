SCENE_DIR="data/GSC"
SCENE_LIST="Bartender" # CBA Bartender Cinema

declare -A TEST_VIEWS
TEST_VIEWS=(
    ["CBA"]="7 22"
    ["Bartender"]="9 11"
    ["Cinema"]="9 11"
)

declare -A START_FRAMES
START_FRAMES=(
    ["CBA"]=0
    ["Bartender"]=50
    ["Cinema"]=235
)

RESULT_DIR="results/dyngs"

NUM_FRAME=${NUM_FRAME:-65}
DEFAULT_TOTAL_FRAMES=${DEFAULT_TOTAL_FRAMES:-300}
DATA_ROOT_SUBDIR=${DATA_ROOT_SUBDIR:-colmap}

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
    local TEST_VIEW_IDS=${TEST_VIEWS[$SCENE]}
    local START_FRAME=${START_FRAMES[$SCENE]:-0}

    local gof=$NUM_FRAME
    local total_frames
    total_frames=$(get_total_frames $SCENE)

    local base_data_dir="${SCENE_DIR}/${SCENE}/${DATA_ROOT_SUBDIR}/colmap_${START_FRAME}"

    echo "Running ${SCENE} | GOF=${gof} | total_frames=${total_frames} | start_frame=${START_FRAME}"

    CUDA_VISIBLE_DEVICES=$GPU_ID python simple_trainer_dyngs.py compression_sim \
        --model_path "$RESULT_DIR/$SCENE" \
        --data_dir "$base_data_dir" \
        --result_dir "$RESULT_DIR/$SCENE" \
        --downscale_factor 1 \
        --duration "$gof" \
        --total_frames "$total_frames" \
        --data_start_frame "$START_FRAME" \
        --batch_size 2 \
        --max_steps 50_000 \
        --refine_start_iter 1_500 \
        --refine_stop_iter 45_000 \
        --refine_every 100 \
        --reset_every 5_000 \
        --pause_refine_after_reset 2_000 \
        --strategy Modified_STG_Strategy \
        --test_view_id $TEST_VIEW_IDS

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

        local actual_start=$(( START_FRAME + rel_start ))
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
        CUDA_VISIBLE_DEVICES=$GPU_ID python simple_trainer_dyngs.py default \
            --model_path "$group_root" \
            --data_dir "$data_dir" \
            --result_dir "$group_root" \
            --downscale_factor 1 \
            --duration "$frames" \
            --data_start_frame "$actual_start" \
            --lpips_net vgg \
            --compression stg \
            --ckpt "$ckpt_path" \
            --test_view_id $TEST_VIEW_IDS
    done
}

GPU_LIST=(0)
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
    }

done
