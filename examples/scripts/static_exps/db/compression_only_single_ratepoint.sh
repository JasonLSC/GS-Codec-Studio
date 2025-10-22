#!/bin/bash

# ============================================================================
# Compression-Only Single Rate Point Experiment Script for Deep Blending Dataset
# ============================================================================
# This script runs compression experiments for all scenes at a single RD lambda value,
# with multi-GPU parallel support. This version skips training and only runs compression
# on existing checkpoints from training results.
#
# Usage:
#   bash examples/scripts/static_exps/db/compression_only_single_ratepoint.sh [RD_LAMBDA]
#
# Example:
#   bash examples/scripts/static_exps/db/compression_only_single_ratepoint.sh 0.01
# ============================================================================

# ----------------- Configuration -------------- #

# Dataset configuration
SCENE_DIR="examples/data/db"
SCENE_LIST="drjohnson playroom"

# Result directory base
RESULT_DIR_BASE="results/db_mcmc_comp_sim"

# Configuration file
CONFIG_FILE="examples/configs/mcmc_comp_sim.yaml"

# GPU configuration
GPU_LIST=(1 2)
GPU_COUNT=${#GPU_LIST[@]}

# RD Lambda parameter
RD_LAMBDA=${1:-0.01}  # Default to 0.01 if not provided

# Result directory for this rate point
RESULT_DIR="${RESULT_DIR_BASE}/rd_${RD_LAMBDA}"

echo "=============================================="
echo "Compression-Only Single Rate Point Experiment"
echo "=============================================="
echo "RD_LAMBDA: $RD_LAMBDA"
echo "Result Directory: $RESULT_DIR"
echo "Scenes: $SCENE_LIST"
echo "GPUs: ${GPU_LIST[@]}"
echo "=============================================="

# ----------------- Main Function -------------- #

run_compression_only_scene() {
    local GPU_ID=$1
    local SCENE=$2
    
    echo "----------------------------------------------"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Starting compression for scene: $SCENE on GPU $GPU_ID"
    echo "----------------------------------------------"
    
    # Check if training checkpoint exists
    CHECKPOINT_PATH="$RESULT_DIR/$SCENE/ckpts/ckpt_29999_rank0.pt"
    if [ ! -f "$CHECKPOINT_PATH" ]; then
        echo "[ERROR] Training checkpoint not found: $CHECKPOINT_PATH"
        echo "[ERROR] Please run training first or check the path."
        return 1
    fi
    
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Found checkpoint: $CHECKPOINT_PATH"
    
    # Compression phase only
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Running compression for $SCENE..."
    CUDA_VISIBLE_DEVICES=$GPU_ID python examples/simple_trainer.py mcmc \
        --config $CONFIG_FILE \
        --mode compress \
        --data-dir $SCENE_DIR/$SCENE/ \
        --result-dir $RESULT_DIR/$SCENE/ \
        --ckpt $CHECKPOINT_PATH \
        --lpips-net vgg \
        --disable-viewer
    
    if [ $? -ne 0 ]; then
        echo "[ERROR] Compression failed for scene: $SCENE"
        return 1
    fi
    
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Compression completed for $SCENE"
    echo "----------------------------------------------"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Finished compression for scene: $SCENE"
    echo "----------------------------------------------"
}

# ----------------- Main Loop ------------------ #

SCENE_IDX=0

for SCENE in $SCENE_LIST; do
    # Assign GPU using round-robin
    GPU_ID=${GPU_LIST[$((SCENE_IDX % GPU_COUNT))]}
    
    # Run scene in background for parallel execution
    run_compression_only_scene $GPU_ID $SCENE &
    
    SCENE_IDX=$((SCENE_IDX + 1))
done

# Wait for all scenes to complete
echo "Waiting for all compression tasks to complete..."
wait

echo "=============================================="
echo "All compression tasks completed for RD_LAMBDA=$RD_LAMBDA"
echo "=============================================="
