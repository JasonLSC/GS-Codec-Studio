#!/usr/bin/env bash
set -euo pipefail

CONFIG_PATH="examples/configs/mcmc_tt_sim.yaml"
DATA_ROOT="examples/data/tandt"
RESULT_ROOT="results/new_cfg_tt"
SCENES=(train truck)
GPU_LIST=(0)
RD_LAMBDA=${1:-0.01}
CKPT_STEP=29999

NUM_GPUS=${#GPU_LIST[@]}
if [[ $NUM_GPUS -eq 0 ]]; then
    echo "[ERROR] GPU_LIST is empty." >&2
    exit 1
fi

scene_idx=0
for scene in "${SCENES[@]}"; do
    gpu_id=${GPU_LIST[$((scene_idx % NUM_GPUS))]}
    data_dir="${DATA_ROOT}/${scene}"
    result_dir="${RESULT_ROOT}/${scene}"
    ckpt_path="${result_dir}/ckpts/ckpt_${CKPT_STEP}_rank0.pt"

    if [[ ! -d "$data_dir" ]]; then
        echo "[ERROR] Scene directory not found: $data_dir" >&2
        echo "请修改 DATA_ROOT 或创建到实际数据集的软链接后再运行。" >&2
        exit 1
    fi

    mkdir -p "$result_dir"

    echo "[TRAIN] Scene: $scene on GPU $gpu_id"
    CUDA_VISIBLE_DEVICES="$gpu_id" \
    python examples/simple_trainer.py mcmc \
        --config "$CONFIG_PATH" \
        --disable_viewer \
        --eval_steps -1 \
        --data_factor 1 \
        --data_dir "$data_dir" \
        --result_dir "$result_dir" \
        --compression png \
        --rd_lambda "$RD_LAMBDA"

    echo "[EVAL] Scene: $scene on GPU $gpu_id"
    CUDA_VISIBLE_DEVICES="$gpu_id" \
    python examples/simple_trainer.py mcmc \
        --config "$CONFIG_PATH" \
        --disable_viewer \
        --data_factor 1 \
        --data_dir "$data_dir" \
        --result_dir "$result_dir" \
        --compression png \
        --lpips_net vgg \
        --ckpt "$ckpt_path"

    scene_idx=$((scene_idx + 1))

done

echo "[SUMMARY] results -> $RESULT_ROOT"
python examples/benchmarks/compression/summarize_stats.py \
    --results_dir "$RESULT_ROOT" \
    --scenes "${SCENES[@]}"
