#!/bin/bash

SCENE_NAMES=("coffee_martini" "cook_spinach" "cut_roasted_beef" "flame_salmon_1" "flame_steak" "sear_steak")

for SCENE_NAME in "${SCENE_NAMES[@]}"; do
    echo "Processing $SCENE_NAME"
    python n3d_utils/video_preprocess.py --videopath examples/data/neural_3d/$SCENE_NAME
    # echo "Done"
done