#!/bin/bash

# Loop over each scene and rp
SCENE_NAMES=(bartender cinema breakfast) # bartender cinema breakfast
RP_LIST=(rp1 rp2 rp3 rp4)  

for SCENE in "${SCENE_NAMES[@]}"; do
    # Run each rp in parallel, then wait for all to finish
    for RP in "${RP_LIST[@]}"; do
        # Set the source and decoded ply directories for the current scene and rp
        SOURCE_PLY_DIR=examples/data/GSC_splats/m71763_${SCENE}_stable/track_cam_info
        EXP_DIR=examples/results/mpeg152/main_track_vid_hm/${SCENE}/${RP}
        DECODED_PLY_DIR=${EXP_DIR}/decoded_ply
        LOG_DIR=${EXP_DIR}/logs

        mkdir -p ${LOG_DIR}

        echo "Processing scene: ${SCENE}, rp: ${RP}"

        # Run the mpeg-gsc-metrics tool for the current configuration in background
        third_party/mpeg-gsc-metrics/build/Release/bin/mpeg-gsc-metrics \
            -a ${SOURCE_PLY_DIR}/frame%03d.ply \
            -b ${DECODED_PLY_DIR}/frame%03d_pos.ply \
            --frameCount=32 \
            --startFrame=0 \
            --verbose=1 \
            --cpu=1 \
            --width=1920 \
            --height=1080 \
            --useCameraPosition=1 > ${LOG_DIR}/mpeg_gsc_metrics.log 2>&1 
    done
    # Wait for all background processes to finish
    wait
done

SCENE_NAMES=(man_with_fruit) 
RP_LIST=(rp1 rp2 rp3 rp4)  

for SCENE in "${SCENE_NAMES[@]}"; do
    # Run each rp in parallel, then wait for all to finish
    for RP in "${RP_LIST[@]}"; do
        # Set the source and decoded ply directories for the current scene and rp
        SOURCE_PLY_DIR=examples/data/GSC_splats/m71903_bust_dataset/${SCENE}/ply_cam_info
        EXP_DIR=examples/results/mpeg152/main_track_vid_hm/${SCENE}/${RP}
        DECODED_PLY_DIR=${EXP_DIR}/decoded_ply
        LOG_DIR=${EXP_DIR}/logs

        mkdir -p ${LOG_DIR}

        echo "Processing scene: ${SCENE}, rp: ${RP}"

        # Run the mpeg-gsc-metrics tool for the current configuration in background
        third_party/mpeg-gsc-metrics/build/Release/bin/mpeg-gsc-metrics \
            -a ${SOURCE_PLY_DIR}/0081.ply \
            -b ${DECODED_PLY_DIR}/0081_pos.ply \
            --frameCount=1 \
            --startFrame=0 \
            --verbose=1 \
            --cpu=1 \
            --width=3840 \
            --height=2160 \
            --useCameraPosition=1 > ${LOG_DIR}/mpeg_gsc_metrics.log 2>&1 
    done
    # Wait for all background processes to finish
    wait
done