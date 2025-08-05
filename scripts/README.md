### Compress Tracked Gaussian Splats Sequences via Video Codec (Video-based Anchor)
If you're interested in MPEG Gaussian Splats Coding, we have implemented a simple coding method for compressing temporally tracked Gaussian Splats using Video Codec as the core component. The figure shows the pipeline of video-based anchor.
![Pipeline of video-based anchor](assets/video_anchor_pipeline.png)
The key idea is 1) to organize the sequence of Gaussian Splats into multiple attribute videos of Gaussian Splats, and 2) to use video codec to compress these attribute videos. If you want to reproduce our latest results, please checkout below **updated version of guidance**.
<details open>
<summary>Updated version of guidance</summary>

**Preparation**

1.Install modified PLAS:

[Original PLAS](https://github.com/fraunhoferhhi/PLAS) can not guarantee the identical sorted results on different types of GPU (e.g. Nvidia RTX 3090 vs. 3080 Ti), even given the exactly same input data, same random seeds. This issue stems from the `torch.randperm` function. We make a workaround on this issue, and upload the modified code to the [repository](https://github.com/JasonLSC/PLAS).

So if you want to reproduce the identical sorted results, please install modified PLAS:
```
# If you have installed original PLAS, you need to uninstall it and install modified version.
pip install git+https://github.com/JasonLSC/PLAS
```

2.Compile video codec:
You can just use our pre-compiled executables under the path `examples/helper/HM-18.0`. 
Or you also can place the source code of [HM-18.0](https://vcgit.hhi.fraunhofer.de/jvet/HM/-/tree/HM-18.0?ref_type=tags) under the path `examples/helper/HM-18.0`. Then you just need to compile it:

```
cd examples/helper/HM-18.0
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

Note: Since the original code from HM-18.0 fails during compilation due to certain warnings, I added the line `set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-array-bounds")` in CMakeLists.txt to suppress this issue.

**Scripts**

Go back to the `examples` folder and run the scripts:

```
cd ../../../
bash benchmarks/mpeg152/1f_vid_hm/bartender.sh
```


**Note**: 
Before using the scripts mentioned above, please note the following:
1. Please modify the input parameters ``--ply_dir``, ``--data_dir``, ``--ply_filename``(needed for single frame input), ``--masks``(needed for object-centric content) in the script to match your local paths.
2. These experiments involve third-party programs, including QMIV (quality evaluation software). If you need newer versions, you can compile them yourself and replace the current executables under ``examples/helper``.

**Experimental results collections**

After successfully completing experiment of one scene (where one script corresponds to one scene), you can find the experimental results saved in CSV format in the results folder, e.g. ``examples/results/mpeg152/1f_vid_hm/<scene_name>``. The relevant data can be easily pasted into the MPEG GSC Excel Template.

</details>

<details>
    <summary>Old version of guidance</summary>

You can try this method using the script below.
```bash
cd examples
bash benchmarks/mpeg/video_anchor_bench.sh
```

To compare with the Point Cloud Compression-based approach in MPEG Gaussian Splats Coding group, we have developed a simple wrapper based on their software. This allows us to evaluate both methods using the same evaluation protocol. You can try the Point Cloud Compression-based approach using the script below.
```bash
cd examples
bash benchmarks/mpeg/pcc_anchor_bench.sh
```

***Running the above script will reproduce both our proposed approach from contribution m72063 and the baseline results that we used for comparison.***

**Note**: 
Before using the scripts mentioned above, please note the following:
1. Please modify the input parameters ``--ply_dir``, ``--data_dir``, and ``--result_dir`` in the script to match your local paths.
2. These experiments involve third-party programs, including point cloud codec and QMIV (quality evaluation software). If you need newer versions, you can compile them yourself and replace the current executables under ``examples/helper``.
</details>

### Pre- & Post-Processing of Video-based Anchor
We provide standalone Python scripts to handle preprocessing and postprocessing of Gaussian Splats parameters in [Video-based Anchor](#compress-tracked-gaussian-splats-sequences-via-video-codec-video-based-anchor).

Specifically, the preprocessing includes quaternion normalization and fixed-point quantization for all parameters. In fixed-point quantization, we employ per-channel min-max quantization for all attributes. While the means representing Gaussian Splats positions are uniformly quantized to 16 bits, other attributes use 8-bit uniform quantization. The per-channel maximum and minimum values for each component, along with their bitdepth information, are stored in a JSON file as metadata for subsequent postprocessing.

``` shell
cd examples
python gs_ply_process.py \
    --preprocess \
    --raw_ply_path data/GSC_splats/m71763_bartender_stable/track/frame000.ply \
    --exp_dir results/gs_ply_process/bartender \
```

The postprocessing involves dequantizing the input PLY file using the metadata information. Note that postprocessing cannot be skipped since it converts the values back to the I-3DGS domain.

``` shell
cd examples
# Note: Place the decoded PLY file in the '--exp_dir' directory and provide its filename via the '--quantized_ply_filename' parameter.
python gs_ply_process.py \
    --postprocess \
    --quantized_ply_filename quantized.ply \ 
    --exp_dir results/gs_ply_process/bartender \
    --dequantized_ply_filename dequantized.ply
```

You can check out "examples/benchmarks/gs_ply_process/gs_ply_process.sh" and "examples/gs_ply_process.py" for more details.

### Running I-3DGS Quality Assessment via MPEG GSC Metrcis Software

**Installation**
Get submodules for MPEG GSC Software. 
```
git submodule update --init --recursive
```

1. Install mpeg-gsc-metrics  
Go into the "mpeg-gsc-metrics" submodule folder and simply run build.sh.

```
cd third_party/mpeg-gsc-metrics
bash build.sh
```

For more details, please refer to the [README.md](third_party/mpeg-gsc-metrics/README.md) in the ["mpeg-gsc-metrics"](https://git.mpeg.expert/MPEG/Explorations/GSC/gsc-software/mpeg-gsc-metrics) project.

2. Install mpeg-gsc-tools/gsTools  
This software is used to add camera information into 3DGS ply files. Camera information is needed when using mpeg-gsc-metrics.

Go into the gsTools folder under the "mpeg-gsc-tools" submodule and simply run build.sh.

```
cd third_party/mpeg-gsc-tools/gsTools
bash build.sh
```

**Steps to run**

Running 1f-vid experiments:
1. Add camera information to source ply files

    Camera information must be included in the ply files because the mpeg-gsc-metrics software requires it to support rendering and metric evaluation under specified camera parameters.
    ```
    bash scripts/1f_vid/add_cam_info_to_src_ply.sh
    ```
2. Run compression experiments
    ```
    bash scripts/1f_vid/run_1f_exps.sh
    ```
3. Add camera information to decoded ply files obtained from compression experiments
    ```
    bash scripts/1f_vid/add_cam_info_to_dec_ply.sh
    ```
4. Run mpeg_gsc_metrcis software to get visual quality metrics
    ```
    bash scripts/1f_vid/run_mpeg_gsc_metrics.sh
    ```
5. Get final summarized rate-distortion results in csv file
    ```
    bash scripts/1f_vid/run_metrics_extraction.sh
    ```

    We also provide all-in-one script for experiments:
    ```
    bash scripts/1f_vid/run_1f_vid_full_pipeline.sh
    ```

Running main track experiments:
1. Add camera information to source ply files

    Camera information must be included in the ply files because the mpeg-gsc-metrics software requires it to support rendering and metric evaluation under specified camera parameters.
    ```
    bash scripts/main_track_vid/add_cam_info_to_src_ply.sh
    ```
2. Run compression experiments
    ```
    bash scripts/main_track_vid/run_1f_exps.sh
    ```
3. Add camera information to decoded ply files obtained from compression experiments
    ```
    bash scripts/main_track_vid/add_cam_info_to_dec_ply.sh
    ```
4. Run mpeg_gsc_metrcis software to get visual quality metrics
    ```
    bash scripts/main_track_vid/run_mpeg_gsc_metrics.sh
    ```
5. Get final summarized rate-distortion results in csv file
    ```
    bash scripts/main_track_vid/run_metrics_extraction.sh
    ```
    We also provide all-in-one script for experiments:
    ```
    bash scripts/main_track_vid/run_main_track_vid_full_pipeline.sh
    ```

---