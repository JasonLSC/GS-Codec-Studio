#!/bin/bash

# ============================================================================
# Multi Rate Point Experiment Script for Deep Blending Dataset
# ============================================================================
# This script runs experiments across multiple RD lambda values by calling
# single_ratepoint.sh for each rate point sequentially.
#
# Usage:
#   bash examples/scripts/static_exps/db/train_multi_ratepoint.sh [RD_LAMBDA_1 RD_LAMBDA_2 ...]
#
# Examples:
#   # Use default rate points
#   bash examples/scripts/static_exps/db/train_multi_ratepoint.sh
#
#   # Custom rate points
#   bash examples/scripts/static_exps/db/train_multi_ratepoint.sh 0.01 0.05 0.1
# ============================================================================

# ----------------- Configuration -------------- #

# Default RD lambda values (common rate points)
DEFAULT_RD_LAMBDA_LIST=(0.002 0.006 0.01)

# Allow custom list via command line arguments
if [ $# -gt 0 ]; then
    RD_LAMBDA_LIST=("$@")
else
    RD_LAMBDA_LIST=("${DEFAULT_RD_LAMBDA_LIST[@]}")
fi

echo "=============================================="
echo "Multi Rate Point Experiment"
echo "=============================================="
echo "RD Lambda values: ${RD_LAMBDA_LIST[@]}"
echo "=============================================="

# ----------------- Main Loop ------------------ #

TOTAL_RATE_POINTS=${#RD_LAMBDA_LIST[@]}
CURRENT_IDX=0

for RD_LAMBDA in "${RD_LAMBDA_LIST[@]}"; do
    CURRENT_IDX=$((CURRENT_IDX + 1))
    
    echo ""
    echo "=============================================="
    echo "Rate Point [$CURRENT_IDX/$TOTAL_RATE_POINTS]: RD_LAMBDA=$RD_LAMBDA"
    echo "Started at: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "=============================================="
    
    # Call single_ratepoint.sh for this rate point
    bash examples/scripts/static_exps/db/train_single_ratepoint.sh $RD_LAMBDA
    
    if [ $? -ne 0 ]; then
        echo "[ERROR] Failed at RD_LAMBDA=$RD_LAMBDA"
        exit 1
    fi
    
    echo "=============================================="
    echo "Rate Point [$CURRENT_IDX/$TOTAL_RATE_POINTS]: RD_LAMBDA=$RD_LAMBDA COMPLETED"
    echo "Finished at: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "=============================================="
done

echo ""
echo "=============================================="
echo "All Rate Points Completed Successfully!"
echo "=============================================="
echo "Results saved in: results/db_mcmc_comp_sim/"
echo "Rate points processed: ${RD_LAMBDA_LIST[@]}"
echo "=============================================="
