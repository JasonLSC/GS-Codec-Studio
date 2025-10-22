#!/bin/bash

# root directory of the experiments, should be specified 
EXP_ROOT_DIR=examples/results/mpeg152/main_track_vid_hm
# scenes to process
SCENES=(bartender cinema breakfast)
# number of frames in the sequence
FRAME_NUM=32

# run the metrics extraction for each scene
for SCENE in "${SCENES[@]}"; do
    SCENE_DIR="$EXP_ROOT_DIR/$SCENE"
    python scripts/main_track_vid/extract_metrics.py "$SCENE_DIR" --get_bitrate --frame_num $FRAME_NUM
done

SCENES=(man_with_fruit)
FRAME_NUM=1
for SCENE in "${SCENES[@]}"; do
    SCENE_DIR="$EXP_ROOT_DIR/$SCENE"
    python scripts/main_track_vid/extract_metrics.py "$SCENE_DIR" --get_bitrate --frame_num $FRAME_NUM
done

python scripts/main_track_vid/merge_csv_to_excel.py $EXP_ROOT_DIR