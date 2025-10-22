#!/bin/bash

# ============================================================================
# Compression-Only Single Scene Script for Mip-NeRF 360 Dataset
# ============================================================================
# 用法:
#   bash examples/scripts/static_exps/mip/compression_only_single_scene.sh \
#        <SCENE> <GPU_ID> [RD_LAMBDA] [CONFIG_FILE]
#
# 示例:
#   bash examples/scripts/static_exps/mip/compression_only_single_scene.sh bicycle 0
#   bash examples/scripts/static_exps/mip/compression_only_single_scene.sh bonsai 1 0.01 examples/configs/mcmc_comp_sim.yaml
# ============================================================================

# ----------------- 参数解析 -------------- #
SCENE=${1:?Usage: $0 <SCENE> <GPU_ID> [RD_LAMBDA] [CONFIG_FILE]}
GPU_ID=${2:?Usage: $0 <SCENE> <GPU_ID> [RD_LAMBDA] [CONFIG_FILE]}
RD_LAMBDA=${3:-0.01}
CONFIG_FILE=${4:-examples/configs/mcmc_comp_sim.yaml}

# ----------------- 基础配置 -------------- #
SCENE_DIR="data/360_v2"
RESULT_DIR_BASE="results/mip_mcmc_comp_sim"
RESULT_DIR="${RESULT_DIR_BASE}/rd_${RD_LAMBDA}/${SCENE}"

echo "=============================================="
echo "Compression-Only Single Scene"
echo "=============================================="
echo "SCENE:         ${SCENE}"
echo "GPU_ID:        ${GPU_ID}"
echo "RD_LAMBDA:     ${RD_LAMBDA}"
echo "CONFIG_FILE:   ${CONFIG_FILE}"
echo "DATA_DIR:      ${SCENE_DIR}/${SCENE}/"
echo "RESULT_DIR:    ${RESULT_DIR}/"
echo "=============================================="

# ----------------- 前置检查 -------------- #
CHECKPOINT_PATH="${RESULT_DIR}/ckpts/ckpt_29999_rank0.pt"
if [ ! -f "${CHECKPOINT_PATH}" ]; then
    echo "[ERROR] Training checkpoint not found: ${CHECKPOINT_PATH}"
    echo "[ERROR] Please run training first or check the path."
    exit 1
fi

# ----------------- 压缩执行 -------------- #
echo "[$(date '+%Y-%m-%d %H:%M:%S')] Running compression for scene: ${SCENE} on GPU ${GPU_ID}"

CUDA_VISIBLE_DEVICES=${GPU_ID} python examples/simple_trainer.py mcmc \
    --config ${CONFIG_FILE} \
    --mode compress \
    --data-dir ${SCENE_DIR}/${SCENE}/ \
    --result-dir ${RESULT_DIR}/ \
    --ckpt ${CHECKPOINT_PATH} \
    --lpips-net vgg \
    --disable-viewer

STATUS=$?
if [ ${STATUS} -ne 0 ]; then
    echo "[ERROR] Compression failed for scene: ${SCENE}"
    exit ${STATUS}
fi

echo "[$(date '+%Y-%m-%d %H:%M:%S')] Compression completed for scene: ${SCENE}"
echo "Output:"
echo "  - ${RESULT_DIR}/compressed_data/metadata_for_decoding.json"
echo "  - ${RESULT_DIR}/compression_info.json"
echo "=============================================="
