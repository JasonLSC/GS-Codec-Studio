## Bartender, Cinema, Breakfast
SCENE_NAME=(bartender cinema breakfast)
FRAME_NUM=1 

for scene_name in ${SCENE_NAME[@]}
do
    COLMAP_DIR=examples/data/GSC_splats/m71763_${scene_name}_stable/colmap_data

    for rp in $(seq 1 4)
    do
        PLY_DIR=examples/results/mpeg152/1f_vid_hm/${scene_name}/rp${rp}/decoded_ply
        for i in $(seq 0 $((FRAME_NUM-1)))
        do
            frame_id=$(printf "%03d" $i)
            third_party/mpeg-gsc-tools/gsTools/build/msvc/Release/bin/cameraPosition \
                --input=${PLY_DIR}/frame${frame_id}.ply \
                --camera=${COLMAP_DIR}/frame${frame_id}/sparse/0/cameras.txt \
                --image=${COLMAP_DIR}/frame${frame_id}/sparse/0/images.txt \
                --output=${PLY_DIR}/frame${frame_id}_pos.ply \
                -v 1
        done
    done
done

## Man_with_fruit
# Just for reference, since I maybe have changed folder structure for convenience

SCENE_NAME=(man_with_fruit)
FRAME_NUM=1 

for scene_name in ${SCENE_NAME[@]}
do
    COLMAP_DIR=examples/data/GSC_splats/m71903_bust_dataset/${scene_name}/colmap_data

    for rp in $(seq 1 4)
    do
        PLY_DIR=examples/results/mpeg152/1f_vid_hm/${scene_name}/rp${rp}/decoded_ply

        frame_id=0081
        third_party/mpeg-gsc-tools/gsTools/build/msvc/Release/bin/cameraPosition \
            --input=${PLY_DIR}/${frame_id}.ply \
            --camera=${COLMAP_DIR}/00${frame_id}/sparse/0/cameras.bin \
            --image=${COLMAP_DIR}/00${frame_id}/sparse/0/images.bin \
            --output=${PLY_DIR}/${frame_id}_pos.ply \
            -v 1

    done
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