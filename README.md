# GSCodec Studio

GSCodec Studio is an open-source framework for Gaussian Splats Compression, including static and dynamic splats representation, reconstruction and compression. It is bulit upon an open-source 3D Gaussian Splatting library [gsplat](https://github.com/nerfstudio-project/gsplat), and extended to support 1) dynamic splats representation, 2) training-time compression simulation, 3) more test-time compression strategies.

![Teaser](./assets/Teaser.png)

## News
**June 5, 2025:** The [paper](https://arxiv.org/abs/2506.01822) about this framework has been released on arXiv.

## Installation
### Repo. & Environment
Please first clone the repository and cd to the root directory.
```bash
git clone --recursive https://git.mpeg.expert/MPEG/Explorations/GSC/gsc-software/gscodec_studio.git
cd GSCodec_Studio
```

If you are interested in MPEG GSC Video Anchor, please check out related repository:
```bash
git checkout MPEG152_video_anchor_1f_vid
```

Set up conda environment.
```bash
# Make a conda environment
conda create --name gscodec_studio python=3.10
conda activate gscodec_studio
```

### Packages Installation

Please install [Pytorch](https://pytorch.org/get-started/locally/) first. Then, you can install the gsplat library extended with more compression features from source code. In this way it will build the CUDA code during installation.

```bash
pip install .
```

If you want to do further development based on this framework, you use following command to install Python packages in editable mode.
```bash
pip install -e . # (develop)
```

## Examples

**Preparations**

Same as gsplat, we need to install some extra dependencies and download the relevant datasets before the evaluation.

```bash
cd examples
pip install -r requirements.txt
# download mipnerf_360 benchmark data
python datasets/download_dataset.py
```
You can download the Tanks and Temples dataset and Deep Blending dataset used in original 3DGS via [this link](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/datasets/input/tandt_db.zip) and place these datasets under 'data' folder.
```bash
# place other dataset, e.g Tanks and Temples dataset, under 'data' folder
ln -s /xxxx/Dataset/tandt data/tandt 
```

We also use third-party library, 'python-fpnge', to accelerate image saving operations during the experiment for now. We also use third-party library, 'gridencoder', to facilitate hash encoding.

```bash
cd ..
pip install third_party/python-fpnge-master
pip install third_party/gridencoder
```

If you are interested in post-training compression, you also need to install library for [vector quantization](https://github.com/DeMoriarty/TorchPQ?tab=readme-ov-file#install) and [plas sorting](https://github.com/fraunhoferhhi/PLAS), before running scripts.
```bash
# refer to https://github.com/DeMoriarty/TorchPQ?tab=readme-ov-file#install to see how to install TorchPQ

# install original implementation of PLAS
pip install git+https://github.com/fraunhoferhhi/PLAS.git
```

### MPEG GSC Activities
If you are interested in MPEG GSC activities, you can find related descriptions in [this file](scripts/README.md). This covers how to run Video-based anchor, how to perform pre and post-processing of Gaussian Splats, and how to conduct Quality Assessment after completing compression experiments.

### Static Gaussian Splats Training and Compression

We provide a script that enables more memory-efficient Gaussian splats while maintaining high visual quality, such as representing the Truck scene with only about 8MB of storage. The script includes 1) the static splats training with compression simulation, 2) the compression of trained static splats, and 3) the metric evaluation of uncompressed and compressed static splats.

```bash
# Tanks and Temples dataset
bash benchmarks/compression/final_exp/mcmc_tt_sim.sh
```

### Dynamic Gaussian Splats Training and Compression

First, please follow the dataset preprocessing instruction described in [this file](mpeg_gsc_utils/multiview_video_preprocess/README.md) for training data prepration.

Next, run the script for dynamic gaussian splats training and compression.
```bash
cd examples
bash benchmarks/dyngs/dyngs.sh
```

### Extract Per-Frame Static Gaussian from Dynamic Splats

If you finsh the training of dynamic splats, then you can use the script to extract static gaussian splats stored at discrte timesteps in ".ply" file
```bash
cd examples
bash benchmarks/dyngs/export_plys.sh
```

### Load .ply File and Render

If you want to directly read and render splats exported as PLY files, you can use the following script.
Note: You need to modify the PLY file path in the scripts.
```bash
cd examples
bash benchmarks/load_ply_and_render.sh
```

## Contributors

This project is developed by the following contributors:

- Sicheng Li: jasonlisicheng@zju.edu.cn
- Chengzhen Wu: chengzhenwu@zju.edu.cn
- Zhiwei Zhu: zhuzhiwei21@zju.edu.cn

If you have any questions about this project, please feel free to contact us.

## Acknowledgement
This project is bulit on [gsplat](https://github.com/nerfstudio-project/gsplat). We thank all contributors from [gsplat](https://github.com/nerfstudio-project/gsplat) for building such a great open-source project.