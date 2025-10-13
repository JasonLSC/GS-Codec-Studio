#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------- #
# Static GSCodec training + compression simulation driver (MCMC strategy).
#
# This script replays the legacy mcmc_tt_sim.sh experiment, but routes through
# the refactored configuration system (examples/configs/mcmc_tt_sim.yaml).
# Configure dataset / output locations through environment variables or by
# editing the defaults below.
# ---------------------------------------------------------------------------- #

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/../../.." && pwd)

# Base YAML config that enables compression simulation + learnable mask.
CONFIG_PATH=${CONFIG_PATH:-"${REPO_ROOT}/examples/configs/mcmc_tt_sim.yaml"}

# Dataset root containing per-scene COLMAP folders (defaults to T&T under examples).
SCENE_ROOT=${SCENE_ROOT:-"${REPO_ROOT}/examples/data/tandt"}

# Comma/space separated list of scene names.
SCENE_LIST=${SCENE_LIST:-"train truck"}

# Output directory root (results are stored under <RESULT_ROOT>/<scene>/).
RESULT_ROOT=${RESULT_ROOT:-"${REPO_ROOT}/examples/results/Ours_TT"}

# GPU ids to cycle through (space or comma separated). Defaults to single GPU 0.
GPU_LIST=${GPU_LIST:-"0"}

# Rate-distortion weight; overrides YAML rd_lambda when provided.
RD_LAMBDA=${RD_LAMBDA:-"0.01"}

# Checkpoint step to evaluate after training.
CKPT_STEP=${CKPT_STEP:-"29999"}

# Whether to run the stats summarizer at the end (true/false).
RUN_SUMMARY=${RUN_SUMMARY:-"true"}

# Whether to skip training stage and go straight to evaluation.
EVAL_ONLY=${EVAL_ONLY:-"false"}

parse_list() {
    local value=$1
    value=${value//,/ }
    read -r -a parsed <<< "$value"
    echo "${parsed[@]}"
}

GPU_ARRAY=($(parse_list "$GPU_LIST"))
SCENE_ARRAY=($(parse_list "$SCENE_LIST"))
NUM_GPUS=${#GPU_ARRAY[@]}

if [[ ! -f "$CONFIG_PATH" ]]; then
    echo "[ERROR] Config file not found: $CONFIG_PATH" >&2
    exit 1
fi

run_single_scene() {
    local gpu_id=$1
    local scene=$2

    local scene_dir="${SCENE_ROOT}/${scene}"
    local result_dir="${RESULT_ROOT}/${scene}"
    local ckpt_path="${result_dir}/ckpts/ckpt_${CKPT_STEP}_rank0.pt"

    if [[ ! -d "$scene_dir" ]]; then
        echo "[ERROR] Scene directory missing: $scene_dir" >&2
        exit 1
    fi

    mkdir -p "$result_dir"

    if [[ "$EVAL_ONLY" != "true" ]]; then
        echo "--- Training scene '$scene' on GPU ${gpu_id} ---"
        CUDA_VISIBLE_DEVICES="$gpu_id" \
        python "${REPO_ROOT}/examples/simple_trainer.py" mcmc \
            --config "$CONFIG_PATH" \
            --disable_viewer \
            --eval_steps -1 \
            --data_factor 1 \
            --data_dir "$scene_dir" \
            --result_dir "$result_dir" \
            --compression png \
            --rd_lambda "$RD_LAMBDA"
    else
        if [[ ! -f "$ckpt_path" ]]; then
            echo "[ERROR] Expected checkpoint missing for evaluation: $ckpt_path" >&2
            exit 1
        fi
    fi

    echo "--- Evaluating scene '$scene' on GPU ${gpu_id} ---"
    CUDA_VISIBLE_DEVICES="$gpu_id" \
    python "${REPO_ROOT}/examples/simple_trainer.py" mcmc \
        --config "$CONFIG_PATH" \
        --disable_viewer \
        --data_factor 1 \
        --data_dir "$scene_dir" \
        --result_dir "$result_dir" \
        --compression png \
        --lpips_net vgg \
        --ckpt "$ckpt_path"
}

scene_idx=0
for scene in "${SCENE_ARRAY[@]}"; do
    gpu=${GPU_ARRAY[$((scene_idx % NUM_GPUS))]}
    run_single_scene "$gpu" "$scene"
    scene_idx=$((scene_idx + 1))
    echo
    echo "[INFO] Completed scene '${scene}'."
    echo
done

echo "All scenes finished."

if [[ "$RUN_SUMMARY" == "true" ]]; then
    if command -v zip >/dev/null 2>&1; then
        echo "Zipping + summarizing results under $RESULT_ROOT"
    else
        echo "zip command not found; stats summary will run without archive."
    fi
    python "${REPO_ROOT}/examples/benchmarks/compression/summarize_stats.py" \
        --results_dir "$RESULT_ROOT" \
        --scenes "${SCENE_ARRAY[@]}"
fi
