#!/bin/bash

# root directory of the experiments, should be specified 
EXP_ROOT_DIR=examples/results/mpeg152/1f_vid_hm
# scenes to process
SCENES=(bartender cinema breakfast man_with_fruit)
# number of frames in the sequence
FRAME_NUM=1

# run the metrics extraction for each scene
for SCENE in "${SCENES[@]}"; do
    SCENE_DIR="$EXP_ROOT_DIR/$SCENE"
    python scripts/extract_metrics.py "$SCENE_DIR" --get_bitrate --frame_num $FRAME_NUM
done

python scripts/merge_csv_to_excel.py $EXP_ROOT_DIR