#!/bin/bash

# Interactive pipeline script to run 1-frame experiments and generate metrics
# Allows users to choose which steps to execute

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

# Function to ask user confirmation
ask_confirmation() {
    while true; do
        read -p "$(echo -e "${YELLOW}$1 (y/n):${NC} ")" yn
        case $yn in
            [Yy]* ) return 0;;
            [Nn]* ) return 1;;
            * ) echo "Please answer yes or no.";;
        esac
    done
}

# Create logs directory
LOGS_DIR="logs/pipeline_interactive_$(date '+%Y%m%d_%H%M%S')"
mkdir -p "$LOGS_DIR"

print_status "🚀 Interactive Pipeline Execution"
print_status "Logs will be saved to: $LOGS_DIR"
echo

# Step selection
echo -e "${BLUE}Available steps:${NC}"
echo "  1. Run 1-frame experiments (run_1f_exps.sh)"
echo "  2. Add camera info to decoded PLY files (add_cam_info_to_dec_ply.sh)"
echo "  3. Run MPEG-GSC metrics (run_mpeg_gsc_metrics.sh)"
echo "  4. Extract metrics and merge to Excel (run_metrics_extraction.sh)"
echo

# Step 1
if ask_confirmation "Execute Step 1: Run 1-frame experiments?"; then
    print_status "Step 1/4: Running 1-frame experiments..."
    if bash scripts/run_1f_exps.sh 2>&1 | tee "$LOGS_DIR/step1_run_1f_exps.log"; then
        print_success "Step 1 completed: 1-frame experiments finished"
    else
        print_error "Step 1 failed: 1-frame experiments failed."
        if ! ask_confirmation "Continue to next step anyway?"; then
            exit 1
        fi
    fi
    echo
else
    print_warning "Skipping Step 1: 1-frame experiments"
    echo
fi

# Step 2
if ask_confirmation "Execute Step 2: Add camera info to decoded PLY files?"; then
    print_status "Step 2/4: Adding camera info to decoded PLY files..."
    if bash scripts/add_cam_info_to_dec_ply.sh 2>&1 | tee "$LOGS_DIR/step2_add_cam_info.log"; then
        print_success "Step 2 completed: Camera info added to PLY files"
    else
        print_error "Step 2 failed: Adding camera info failed."
        if ! ask_confirmation "Continue to next step anyway?"; then
            exit 1
        fi
    fi
    echo
else
    print_warning "Skipping Step 2: Adding camera info"
    echo
fi

# Step 3
if ask_confirmation "Execute Step 3: Run MPEG-GSC metrics?"; then
    print_status "Step 3/4: Running MPEG-GSC metrics..."
    if bash scripts/run_mpeg_gsc_metrics.sh 2>&1 | tee "$LOGS_DIR/step3_mpeg_gsc_metrics.log"; then
        print_success "Step 3 completed: MPEG-GSC metrics calculated"
    else
        print_error "Step 3 failed: MPEG-GSC metrics failed."
        if ! ask_confirmation "Continue to next step anyway?"; then
            exit 1
        fi
    fi
    echo
else
    print_warning "Skipping Step 3: MPEG-GSC metrics"
    echo
fi

# Step 4
if ask_confirmation "Execute Step 4: Extract metrics and merge to Excel?"; then
    print_status "Step 4/4: Extracting metrics and merging to Excel..."
    if bash scripts/run_metrics_extraction.sh 2>&1 | tee "$LOGS_DIR/step4_metrics_extraction.log"; then
        print_success "Step 4 completed: Metrics extracted and merged to Excel"
    else
        print_error "Step 4 failed: Metrics extraction failed."
    fi
    echo
else
    print_warning "Skipping Step 4: Metrics extraction"
    echo
fi

# Final summary
print_success "🎉 Interactive pipeline execution completed!"
print_status "Results summary:"
print_status "  - Log files: $LOGS_DIR/"

# Show final merged metrics location
RESULTS_DIR="examples/results/mpeg152/1f_vid_hm"
if [ -f "$RESULTS_DIR/merged_metrics.xlsx" ]; then
    print_success "Final merged metrics file: $RESULTS_DIR/merged_metrics.xlsx"
elif [ -f "$RESULTS_DIR/merged_metrics.csv" ]; then
    print_success "Final merged metrics file: $RESULTS_DIR/merged_metrics.csv"
else
    print_warning "Merged metrics file not found in expected location"
fi

print_status "Total execution time: $SECONDS seconds" 