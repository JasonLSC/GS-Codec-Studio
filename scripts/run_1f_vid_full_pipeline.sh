#!/bin/bash

# Full pipeline script to run 1-frame experiments and generate metrics
# This script runs all steps in sequence with error checking and logging

set -e  # Exit on any error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[$(date '+%Y-%m-%d %H:%M:%S')] ✓${NC} $1"
}

print_error() {
    echo -e "${RED}[$(date '+%Y-%m-%d %H:%M:%S')] ✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[$(date '+%Y-%m-%d %H:%M:%S')] ⚠${NC} $1"
}

# Create logs directory
LOGS_DIR="logs/full_pipeline_$(date '+%Y%m%d_%H%M%S')"
mkdir -p "$LOGS_DIR"

print_status "Starting full pipeline execution..."
print_status "Logs will be saved to: $LOGS_DIR"

# Step 1: Run 1-frame experiments
print_status "Step 1/4: Running 1-frame experiments..."
if bash scripts/run_1f_exps.sh > "$LOGS_DIR/step1_run_1f_exps.log" 2>&1; then
    print_success "Step 1 completed: 1-frame experiments finished"
else
    print_error "Step 1 failed: 1-frame experiments failed. Check $LOGS_DIR/step1_run_1f_exps.log"
    exit 1
fi

# Step 2: Add camera info to decoded ply files
print_status "Step 2/4: Adding camera info to decoded PLY files..."
if bash scripts/add_cam_info_to_dec_ply.sh > "$LOGS_DIR/step2_add_cam_info.log" 2>&1; then
    print_success "Step 2 completed: Camera info added to PLY files"
else
    print_error "Step 2 failed: Adding camera info failed. Check $LOGS_DIR/step2_add_cam_info.log"
    exit 1
fi

# Step 3: Run MPEG-GSC metrics
print_status "Step 3/4: Running MPEG-GSC metrics..."
if bash scripts/run_mpeg_gsc_metrics.sh > "$LOGS_DIR/step3_mpeg_gsc_metrics.log" 2>&1; then
    print_success "Step 3 completed: MPEG-GSC metrics calculated"
else
    print_error "Step 3 failed: MPEG-GSC metrics failed. Check $LOGS_DIR/step3_mpeg_gsc_metrics.log"
    exit 1
fi

# Step 4: Extract metrics and merge to Excel
print_status "Step 4/4: Extracting metrics and merging to Excel..."
if bash scripts/run_metrics_extraction.sh > "$LOGS_DIR/step4_metrics_extraction.log" 2>&1; then
    print_success "Step 4 completed: Metrics extracted and merged to Excel"
else
    print_error "Step 4 failed: Metrics extraction failed. Check $LOGS_DIR/step4_metrics_extraction.log"
    exit 1
fi

# Final summary
print_success "🎉 Full pipeline completed successfully!"
print_status "Results summary:"
print_status "  - Log files: $LOGS_DIR/"
print_status "  - Merged metrics: examples/results/mpeg152/1f_vid_hm/merged_metrics.xlsx"

# Show final merged metrics location
RESULTS_DIR="examples/results/mpeg152/1f_vid_hm"
if [ -f "$RESULTS_DIR/merged_metrics.xlsx" ]; then
    print_success "Final merged metrics file: $RESULTS_DIR/merged_metrics.xlsx"
elif [ -f "$RESULTS_DIR/merged_metrics.csv" ]; then
    print_success "Final merged metrics file: $RESULTS_DIR/merged_metrics.csv"
else
    print_warning "Merged metrics file not found in expected location"
fi

print_status "Pipeline execution time: $SECONDS seconds" 