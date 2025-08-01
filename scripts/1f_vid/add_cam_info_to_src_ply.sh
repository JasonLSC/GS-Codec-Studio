## Bartender, Cinema, Breakfast
# SCENE_NAME=(bartender cinema breakfast)

for scene_name in ${SCENE_NAME[@]}
do
    PLY_DIR=examples/data/GSC_splats/m71763_${scene_name}_stable/track
    COLMAP_DIR=examples/data/GSC_splats/m71763_${scene_name}_stable/colmap_data

    # make a dir to store new ply files
    NEW_PLY_DIR="${PLY_DIR}_cam_info"
    mkdir -p ${NEW_PLY_DIR}

    for i in $(seq 0 31)
    do
        frame_id=$(printf "%03d" $i)
        third_party/mpeg-gsc-tools/gsTools/build/msvc/Release/bin/cameraPosition \
            --input=${PLY_DIR}/frame${frame_id}.ply \
            --camera=${COLMAP_DIR}/frame${frame_id}/sparse/0/cameras.txt \
            --image=${COLMAP_DIR}/frame${frame_id}/sparse/0/images.txt \
            --output=${NEW_PLY_DIR}/frame${frame_id}.ply \
            -v 1
    done
done

## Man_with_fruit
# Just for reference, since I maybe have changed folder structure for convenience

PLY_DIR=examples/data/GSC_splats/m71903_bust_dataset/man_with_fruit/ply
COLMAP_DIR=examples/data/GSC_splats/m71903_bust_dataset/man_with_fruit/colmap_data

# make a dir to store new ply files
NEW_PLY_DIR="${PLY_DIR}_cam_info"
mkdir -p ${NEW_PLY_DIR}

for i in $(seq 0 0) # 1f
do
    frame_id="0081"
    third_party/mpeg-gsc-tools/gsTools/build/msvc/Release/bin/cameraPosition \
        --input=${PLY_DIR}/${frame_id}.ply \
        --camera=${COLMAP_DIR}/00${frame_id}/sparse/0/cameras.bin \
        --image=${COLMAP_DIR}/00${frame_id}/sparse/0/images.bin \
        --output=${NEW_PLY_DIR}/${frame_id}.ply \
        -v 1
done

## m73251_Humans_and_Objects_1f
## including LEGO_Bugatti LEGO_Ferrari Plant Cricket_player Tennis_player Solo_Tango_Female Solo_Tango_Male Tango_duo

# SCENES=(LEGO_Bugatti LEGO_Ferrari Plant Cricket_player Tennis_player Solo_Tango_Female Solo_Tango_Male Tango_duo)

# for SCENE in ${SCENES[@]}
# do 
#     PLY_DIR=examples/data/GSC_splats/m73251_Humans_and_Objects_1f/${SCENE}
#     COLMAP_DIR=examples/data/GSC_splats/m73251_Humans_and_Objects_1f/${SCENE}/colmap_SFM

#     # make a dir to store new ply files
#     NEW_PLY_DIR="${PLY_DIR}_cam_info"
#     mkdir -p ${NEW_PLY_DIR}

#     for i in $(seq 0 0) # 1f
#     do
#         third_party/mpeg-gsc-tools/gsTools/build/msvc/Release/bin/cameraPosition \
#             --input=${PLY_DIR}/${SCENE}_GS.ply \
#             --camera=${COLMAP_DIR}/sparse/0/cameras.bin \
#             --image=${COLMAP_DIR}/sparse/0/images.bin \
#             --output=${NEW_PLY_DIR}/${SCENE}_GS.ply \
#             -v 1
#     done
# done