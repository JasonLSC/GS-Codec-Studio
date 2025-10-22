import json
# import math
import os
import time
import shutil
import glob
import subprocess
from contextlib import nullcontext
from dataclasses import dataclass, field
from collections import defaultdict
from typing import Dict, List, Optional, Tuple, Union, ContextManager, TypedDict, Any

# import imageio
# import nerfview
import numpy as np
import torch
import torch.nn.functional as F
from tqdm import tqdm
import tyro
# import viser
# import yaml
import fpnge
from datasets.colmap import Dataset, GSCDataset, Parser
from datasets.traj import (
    generate_interpolated_path,
    generate_ellipse_path_z,
    generate_spiral_path,
)
from torch import Tensor
from torch.nn.parallel import DistributedDataParallel as DDP
from torch.utils.tensorboard import SummaryWriter
from torchmetrics.image import PeakSignalNoiseRatio, StructuralSimilarityIndexMeasure
from fused_ssim import fused_ssim
from torchmetrics.image.lpip import LearnedPerceptualImagePatchSimilarity
from typing_extensions import Literal, assert_never
from gsplat import strategy
from gsplat.compression.entropy_coding_compression import EntropyCodingCompression
from gsplat.compression_simulation import simulation
from utils import AppearanceOptModule, CameraOptModule, knn, rgb_to_sh, save_ply, set_random_seed, load_ply, verify_random_seed
from lib_bilagrid import (
    BilateralGrid,
    slice,
    color_correct,
    total_variation_loss,
)

from gsplat.compression import SeqHevcCompression, SeqYUVCompression, SeqYUVCodec
from gsplat.distributed import cli
from gsplat.rendering import rasterization
from gsplat.strategy import DefaultStrategy, MCMCStrategy
from gsplat.optimizers import SelectiveAdam
from gsplat.compression_simulation import CompressionSimulation
from gsplat.compression_simulation.entropy_model import Entropy_factorized_optimized_refactor, Entropy_gaussian

from helper.ges_tm.pre_process_gaussian import load_ply_and_quant
from helper.ges_tm.post_process_gaussian import inverse_load_ply


@dataclass
class CodecConfig:
    encode: str
    decode: str

@dataclass
class AttributeCodecs:
    means: CodecConfig = field(default_factory=lambda: CodecConfig("_compress_png_16bit", "_decompress_png_16bit"))
    scales: CodecConfig = field(default_factory=lambda: CodecConfig("_compress_factorized_ans", "_decompress_factorized_ans"))
    quats: CodecConfig = field(default_factory=lambda: CodecConfig("_compress_factorized_ans", "_decompress_factorized_ans"))
    opacities: CodecConfig = field(default_factory=lambda: CodecConfig("_compress_png", "_decompress_png"))
    sh0: CodecConfig = field(default_factory=lambda: CodecConfig("_compress_png", "_decompress_png"))
    shN: CodecConfig = field(default_factory=lambda: CodecConfig("_compress_masked_kmeans", "_decompress_masked_kmeans"))
    
    def to_dict(self) -> Dict[str, Dict[str, str]]:
        return {
            attr: {"encode": getattr(self, attr).encode, "decode": getattr(self, attr).decode}
            for attr in ["means", "scales", "quats", "opacities", "sh0", "shN"]
        }
    

def default_qp_values() -> Dict[str, Union[int, Dict[str, Any]]]:
    """default qp values"""
    return {
        "means": -1,
        "opacities": 4,
        "quats": 4,
        "scales": 4,
        "sh0": 4,
        "shN":{
            "sh1": 4,
            "sh2": 4,
            "sh3": 4
        }
    }

def default_attribute_configs() -> Dict[str, List[str]]:
    """default attribute configs"""
    return {
        "means": {"qp": -1, "pix_fmt": "yuv444p"},
        "opacities": {"qp": 22, "pix_fmt": "yuv400p"},
        "quats": {
            "w": {"qp": 14, "pix_fmt": "yuv400p"},
            "xyz": {"qp": 14, "pix_fmt": "yuv444p"},
        },
        "scales": {"qp": 14, "pix_fmt": "yuv444p"},
        "sh0": {"qp": 4, "pix_fmt": "yuv444p"},
        "shN": {
            "sh1": {"qp": 14, "pix_fmt": "yuv444p"},
            "sh2": {"qp": 22, "pix_fmt": "yuv444p"},
            "sh3": {"qp": 25, "pix_fmt": "yuv444p"},
        },
        "default": {"qp": -1, "pix_fmt": "yuv444p"}, 
}


@dataclass
class CompressionConfig:
    rate_point: str = "rp0"

@dataclass
class PCCompressionConfig(CompressionConfig):
    pcc_config_filename: str = "encoder_r05.cfg"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "pcc_config_filename": self.pcc_config_filename
        }
        
@dataclass
class SeqYUVCodecConfig(CompressionConfig):
    # Video codec name, e.g. "vvc", "av1", "hevc"
    video_codec_type: str = "vtm"
    # Use PLAS sort in compression or not
    use_sort: bool = True
    # sort type
    sort_type: Literal["plas", "morton"] = "morton"
    # Verbose or not
    verbose: bool = True
    # QP configuration - can be either int or dict for different attributes
    qp: Dict[str, Union[int, Dict[str, Any]]] = field(default_factory=default_qp_values)
    # Maps attribute names to their codec functions
    attribute_configs: Dict[str, List[str]] = field(default_factory=default_attribute_configs)
    # Enable All Intra coding mode
    use_all_intra: bool = False
    # Enable debug mode
    debug: bool = False
    # Indicate which attributes to transform domain before sorting and compression, 
    # e.g. SH: RGB -> YCbCr, Quat: Unit Quaternion -> Euler Angles
    transform_attributes: Dict[str, bool] = field(
        default_factory=lambda: {
            "means": False,
            "scales": False,
            "quats": True, # always normalize quats
            "opacities": False,
            "sh0": False,
            "shN": False
        }
    )
    # Chroma subsampling for shN compression
    chroma_subsampling: Dict[str, str] = field(default_factory=lambda: {
        "sh0": "444",
        "shN": "420"
    })
    # Use chroma qp offset for sh0 and shN compression
    use_chroma_qp_offset: Dict[str, bool]= field(default_factory=lambda: {
        "sh0": False,
        "shN": False
    })

    def to_dict(self) -> Dict[str, Any]:
        """
        Convert the CompressionConfig instance to a dictionary.
        If attribute_codec_registry is not None, it will be converted to a dictionary using its to_dict method.
        """
        # Get only attributes defined in VideoCompressionConfig
        video_compression_attrs = [
            "video_codec_type", "use_sort", "sort_type", "verbose", "qp", 
            "use_all_intra", "debug", "transform_attributes",
            "chroma_subsampling", "use_chroma_qp_offset", "attribute_configs",
        ]
        
        result = {attr: getattr(self, attr) for attr in video_compression_attrs}

        return result


@dataclass
class VideoCompressionConfig(CompressionConfig):
    # Use PLAS sort in compression or not
    use_sort: bool = True
    # Verbose or not
    verbose: bool = True
    # QP configuration - can be either int or dict for different attributes
    qp: Dict[str, Union[int, Dict[str, Any]]] = field(default_factory=default_qp_values)
    # Number of cluster of VQ for shN compression
    n_clusters: int = 32768
    # Maps attribute names to their codec functions
    attribute_codec_registry: Optional[AttributeCodecs] = field(default_factory=lambda: AttributeCodecs())
    # Enable All Intra coding mode
    use_all_intra: bool = False
    # Enable debug mode
    debug: bool = False
    # Indicate which attributes to transform domain before sorting and compression, 
    # e.g. SH: RGB -> YCbCr, Quat: Unit Quaternion -> Euler Angles
    transform_attributes: Dict[str, bool] = field(
        default_factory=lambda: {
            "means": False,
            "scales": False,
            "quats": False,
            "opacities": False,
            "sh0": False,
            "shN": False
        }
    )
    # Chroma subsampling for shN compression
    chroma_subsampling: Dict[str, str] = field(default_factory=lambda: {
        "sh0": "444",
        "shN": "420"
    })
    # Use chroma qp offset for sh0 and shN compression
    use_chroma_qp_offset: Dict[str, bool]= field(default_factory=lambda: {
        "sh0": False,
        "shN": False
    })

    def to_dict(self) -> Dict[str, Any]:
        """
        Convert the CompressionConfig instance to a dictionary.
        If attribute_codec_registry is not None, it will be converted to a dictionary using its to_dict method.
        """
        # Get only attributes defined in VideoCompressionConfig
        video_compression_attrs = [
            "use_sort", "verbose", "qp", "n_clusters", 
            "use_all_intra", "debug", "transform_attributes",
            "chroma_subsampling", "use_chroma_qp_offset"
        ]
        
        result = {attr: getattr(self, attr) for attr in video_compression_attrs}

        # handle attribute_codec_registry
        if self.attribute_codec_registry is not None:
            result["attribute_codec_registry"] = self.attribute_codec_registry.to_dict()

        return result

@dataclass
class Config:
    # Disable viewer
    disable_viewer: bool = False
    # Path to the .pt files. If provide, it will skip training and run evaluation only.
    ckpt: Optional[List[str]] = None
    # Name of compression strategy to use
    compression: Optional[Literal["seq_hevc", "seq_yuv", "seq_yuv_codec"]] = None
    # # Quantization parameters when set to hevc
    # qp: Optional[int] = None
    # Configuration for compression methods
    compression_cfg: CompressionConfig = field(
        default_factory=VideoCompressionConfig
    )

    # Enable profiler
    profiler_enabled: bool = False

    # Enable compression simulation
    compression_sim: bool = False
    # Name of quantization simulation strategy to use
    quantization_sim: Optional[Literal["round", "noise", "vq"]] = None

    # Enable entropy model
    entropy_model_opt: bool = False
    # Define the type of entropy model
    entropy_model_type: Literal["factorized_model", "gaussian_model"] = "factorized_model"
    # Bit-rate distortion trade-off parameter
    rd_lambda: float = 1e-2 # default: 1e-2
    # Steps to enable entropy model into training pipeline
    # factorized model:
    entropy_steps: Dict[str, int] = field(default_factory=lambda: {"means": -1, 
                                                                   "quats": 10_000, 
                                                                   "scales": 10_000, 
                                                                   "opacities": 10_000, 
                                                                   "sh0": 20_000, 
                                                                   "shN": 10_000})
    # gaussian model:
    # entropy_steps: Dict[str, int] = field(default_factory=lambda: {"means": -1, 
    #                                                                "quats": 10_000, 
    #                                                                "scales": 10_000, 
    #                                                                "opacities": 10_000, 
    #                                                                "sh0": 20_000, 
    #                                                                "shN": -1})

    # Enable shN adaptive mask
    shN_ada_mask_opt: bool = False
    # Steps to enable shN adaptive mask
    ada_mask_steps: int = 10_000
    # Strategy to obtain adaptive mask
    shN_ada_mask_strategy: Optional[str] = "learnable" # "gradient"
    
    # Render trajectory path
    render_traj_path: str = "interp"

    # Path to the Mip-NeRF 360 dataset
    # data_dir: str = "data/360_v2/garden"
    # Downsample factor for the dataset
    data_factor: int = 4
    # Directory to save results
    result_dir: str = "results/garden"
    # Every N images there is a test image
    test_every: int = 8
    # Random crop size for training  (experimental)
    patch_size: Optional[int] = None
    # A global scaler that applies to the scene size related parameters
    global_scale: float = 1.0
    # Normalize the world space
    normalize_world_space: bool = True
    # Camera model
    camera_model: Literal["pinhole", "ortho", "fisheye"] = "pinhole"

    # Port for the viewer server
    port: int = 8080

    # Batch size for training. Learning rates are scaled automatically
    batch_size: int = 1
    # A global factor to scale the number of training steps
    steps_scaler: float = 1.0

    # Number of training steps
    max_steps: int = 30_000
    # Steps to evaluate the model
    eval_steps: List[int] = field(default_factory=lambda: [7_000, 30_000])
    # Steps to save the model
    save_steps: List[int] = field(default_factory=lambda: [7_000, 30_000])

    # Initialization strategy
    init_type: str = "sfm"
    # Initial number of GSs. Ignored if using sfm
    init_num_pts: int = 100_000
    # Initial extent of GSs as a multiple of the camera extent. Ignored if using sfm
    init_extent: float = 3.0
    # Degree of spherical harmonics
    sh_degree: int = 3
    # Turn on another SH degree every this steps
    sh_degree_interval: int = 1000
    # Initial opacity of GS
    init_opa: float = 0.1
    # Initial scale of GS
    init_scale: float = 1.0
    # Weight for SSIM loss
    ssim_lambda: float = 0.2

    # Near plane clipping distance
    near_plane: float = 0.01
    # Far plane clipping distance
    far_plane: float = 1e10

    # Strategy for GS densification
    strategy: Union[DefaultStrategy, MCMCStrategy] = field(
        default_factory=DefaultStrategy
    )
    # Use packed mode for rasterization, this leads to less memory usage but slightly slower.
    packed: bool = False
    # Use sparse gradients for optimization. (experimental)
    sparse_grad: bool = False
    # Use visible adam from Taming 3DGS. (experimental)
    visible_adam: bool = False
    # Anti-aliasing in rasterization. Might slightly hurt quantitative metrics.
    antialiased: bool = False

    # Use random background for training to discourage transparency
    random_bkgd: bool = False

    # Opacity regularization
    opacity_reg: float = 0.0
    # Scale regularization
    scale_reg: float = 0.0

    # Enable camera optimization.
    pose_opt: bool = False
    # Learning rate for camera optimization
    pose_opt_lr: float = 1e-5
    # Regularization for camera optimization as weight decay
    pose_opt_reg: float = 1e-6
    # Add noise to camera extrinsics. This is only to test the camera pose optimization.
    pose_noise: float = 0.0

    # Enable appearance optimization. (experimental)
    app_opt: bool = False
    # Appearance embedding dimension
    app_embed_dim: int = 16
    # Learning rate for appearance optimization
    app_opt_lr: float = 1e-3
    # Regularization for appearance optimization as weight decay
    app_opt_reg: float = 1e-6

    # Enable bilateral grid. (experimental)
    use_bilateral_grid: bool = False
    # Shape of the bilateral grid (X, Y, W)
    bilateral_grid_shape: Tuple[int, int, int] = (16, 16, 8)

    # Enable depth loss. (experimental)
    depth_loss: bool = False
    # Weight for depth loss
    depth_lambda: float = 1e-2

    # Dump information to tensorboard every this steps
    tb_every: int = 100
    # Save training images to tensorboard
    tb_save_image: bool = False

    # Scene type
    scene_type: Literal["GSC", "default"] = "default"
    # Test view id
    test_view_id: Optional[Union[List[int], Literal["all"]]] = None

    lpips_net: Literal["vgg", "alex"] = "alex"
    # Enable LPIPS calculation
    with_lpips: bool = False

    ### specific for I-3DGS compression
    # folder containing plys
    ply_dir: str = ""
    # ply filename, only used when frame_num is 1
    ply_filename: Optional[str] = None
    # folder containing colmap
    data_dir: str = ""
    # folder containing masks, only used when object-centric content in MPEG GSC
    mask_dir: Optional[str] = None
    # frame num
    frame_num: int = 1
    # anchor type
    anchor_type: Literal["video","video_codec" "pcc"] = "video"
    # GOP size
    gop_size: int = 16
    # decode only
    decode_only: bool = False

class Runner:
    def __init__(
        self, local_rank: int, world_rank, world_size: int, cfg: Config
    ) -> None:
        os.makedirs(cfg.result_dir, exist_ok=True)
        set_random_seed(42)
        verify_random_seed(cfg.result_dir)

        self.cfg = cfg
        self.world_rank = world_rank
        self.local_rank = local_rank
        self.world_size = world_size
        self.device = f"cuda:{local_rank}"

        # Where to dump results.
        os.makedirs(cfg.result_dir, exist_ok=True)

        # Setup output directories.
        # self.ckpt_dir = f"{cfg.result_dir}/ckpts"
        # os.makedirs(self.ckpt_dir, exist_ok=True)
        self.stats_dir = f"{cfg.result_dir}/stats"
        os.makedirs(self.stats_dir, exist_ok=True)
        self.render_dir = f"{cfg.result_dir}/renders"
        os.makedirs(self.render_dir, exist_ok=True)

        # Losses & Metrics.
        self.ssim = StructuralSimilarityIndexMeasure(data_range=1.0).to(self.device)
        self.psnr = PeakSignalNoiseRatio(data_range=1.0).to(self.device)

        if cfg.with_lpips:
            if cfg.lpips_net == "alex":
                self.lpips = LearnedPerceptualImagePatchSimilarity(
                    net_type="alex", normalize=True
                ).to(self.device)
            elif cfg.lpips_net == "vgg":
                # The 3DGS official repo uses lpips vgg, which is equivalent with the following:
                self.lpips = LearnedPerceptualImagePatchSimilarity(
                    net_type="vgg", normalize=False
                ).to(self.device)
            else:
                raise ValueError(f"Unknown LPIPS network: {cfg.lpips_net}")
        else:
            self.lpips = None

        # frame num
        self.frame_num = cfg.frame_num

        # load ply sequences
        self.splats_list = self.load_ply_sequences(cfg.ply_dir, cfg.frame_num, cfg.ply_filename)

        # load dataset
        self.trainset_list, self.valset_list = self.set_up_datasets(cfg.data_dir, cfg.frame_num, cfg)

        self.compression_cfg = cfg.compression_cfg.to_dict()
        
        if cfg.compression == "seq_hevc":
            self.compression_method = SeqHevcCompression(**self.compression_cfg)
        elif cfg.compression == "seq_yuv":
            self.compression_method = SeqYUVCompression(**self.compression_cfg)
        elif cfg.compression == "seq_yuv_codec":
            self.compression_method = SeqYUVCodec(**self.compression_cfg)
        else:
            raise ValueError(f"Unknown compression method: {cfg.compression}")

    def load_ply_sequences(
        self, ply_dir: str, frame_num: int, ply_filename: Optional[str] = None
    ) -> List[torch.nn.ParameterDict]:
        assert frame_num > 0, "frame_num must be greater than 0"

        splats_list = []
        if frame_num > 1:
            self.ply_filename_list = sorted(glob.glob(os.path.join(ply_dir, "*.ply")))

            for filename in tqdm(self.ply_filename_list[:frame_num], desc="Loading .ply file"):
                splats = load_ply(filename)
                splats_list.append(splats.to("cuda"))
        else:
            self.ply_filename_list = [ply_filename]
            assert ply_filename is not None, "ply_filename must be provided if frame_num is 1"
            splats = load_ply(ply_filename)
            splats_list = [splats.to("cuda")]
        
        return splats_list
    
    def set_up_datasets(
        self, data_dir: str, frame_num: int, cfg: Config
    ) -> Tuple[List[Dataset], List[Dataset]]:
        assert frame_num > 0, "frame_num must be greater than 0"

        trainset_list = []
        valset_list = []
        if frame_num > 1:
            print(f"Loading multiple frame colmap data from {data_dir}")
            all_items = sorted(glob.glob(os.path.join(data_dir, "*")))
            folders = [item for item in all_items if os.path.isdir(item)]

            for folder in tqdm(folders[:frame_num], desc="Loading colmap results"):
                parser = Parser(
                    data_dir=folder,
                    factor=cfg.data_factor,
                    normalize=cfg.normalize_world_space,
                    test_every=cfg.test_every,
                    mask_dir=cfg.mask_dir,
                )
                trainset = GSCDataset(
                    parser,
                    split="train",
                    patch_size=cfg.patch_size,
                    load_depths=cfg.depth_loss,
                    test_view_ids=cfg.test_view_id,
                )
                valset = GSCDataset(
                    parser, 
                    split="val", 
                    test_view_ids=cfg.test_view_id,)
                
                trainset_list.append(trainset)
                valset_list.append(valset)
        else:
            print(f"Loading single frame colmap data from {data_dir}")
            folder = data_dir
            parser = Parser(
                data_dir=folder,
                factor=cfg.data_factor,
                normalize=cfg.normalize_world_space,
                test_every=cfg.test_every,
                mask_dir=cfg.mask_dir,
            )
            trainset = GSCDataset(
                parser,
                split="train",
                patch_size=cfg.patch_size,
                load_depths=cfg.depth_loss,
                test_view_ids=cfg.test_view_id,
            )
            valset = GSCDataset(
                parser, 
                split="val", 
                test_view_ids=cfg.test_view_id,)
            trainset_list.append(trainset)
            valset_list.append(valset)

        return trainset_list, valset_list
 
    def video_encode(self, compress_dir):
        """Entry for running video anchor encoding."""
        print("Running video anchor encoding...")

        if os.path.exists(compress_dir):
            shutil.rmtree(compress_dir)
        os.makedirs(compress_dir)

        # transform splats_list
        splats_list = self.compression_method.param_transform(self.splats_list, cfg.compression_cfg.transform_attributes)   
        # compression: loop on GOPs
        num_gop = (self.frame_num + self.cfg.gop_size - 1) // self.cfg.gop_size
        for gop_id in range(num_gop):
            gop_start_frame_id = gop_id * self.cfg.gop_size
            gop_end_frame_id = min(gop_start_frame_id + self.cfg.gop_size, self.frame_num)
            gop_splats_list = splats_list[gop_start_frame_id:gop_end_frame_id]
            self.compression_method.compress(gop_splats_list, compress_dir, gop_id)
   
    def video_decode(self, compress_dir):
        """Entry for running video anchor decoding."""
        print("Running video anchor decoding...")

        if not os.path.exists(compress_dir):
            raise FileNotFoundError(f"Compression directory {compress_dir} does not exist. Please run video_encode first.")
        # decompression: loop on GOPs
        full_splats_list_c = []
        num_gop = (self.frame_num + self.cfg.gop_size - 1) // self.cfg.gop_size
        for gop_id in range(num_gop):
            splats_list_c = self.compression_method.decompress(compress_dir, gop_id)
            full_splats_list_c.extend(splats_list_c)
            
        # inverse transform splats_list
        full_splats_list_c = self.compression_method.param_inverse_transform(full_splats_list_c, cfg.compression_cfg.transform_attributes)
        return full_splats_list_c
    
    def compress(self, compress_dir):
        """Entry for running video anchor compression."""
        print("Running video anchor compression...")

        # compress
        splats_list = self.compression_method.param_transform(self.splats_list, cfg.compression_cfg.transform_attributes)   
        # compression: loop on GOPs
        num_gop = (self.frame_num + self.cfg.gop_size - 1) // self.cfg.gop_size
        for gop_id in range(num_gop):
            gop_start_frame_id = gop_id * self.cfg.gop_size
            gop_end_frame_id = min(gop_start_frame_id + self.cfg.gop_size, self.frame_num)

            splats_videos = self.compression_method.reorganize(splats_list[gop_start_frame_id:gop_end_frame_id], gop_id)
            self.compression_method.compress(splats_videos, compress_dir, gop_id)

        # decompression: loop on GOPs
        full_splats_list_c = []
        for gop_id in range(num_gop):
            gop_start_frame_id = gop_id * self.cfg.gop_size
            gop_end_frame_id = min(gop_start_frame_id + self.cfg.gop_size, self.frame_num)

        # decompress
        video_splats_c = self.compression_method.decompress(compress_dir, gop_id)
        splats_list_c = self.compression_method.deorganize(video_splats_c)
        splats_list_c = self.compression_method.param_inverse_transform(splats_list_c, cfg.compression_cfg.transform_attributes)
        full_splats_list_c.extend(splats_list_c)

        return full_splats_list_c

    def pcc_compress(self, compress_dir):
        """Entry for running pc anchor compression."""
        print("Running pc anchor compression...")

        intermediate_dir = f"{cfg.result_dir}/intermediate"
        log_dir = f"{cfg.result_dir}/log"
        rec_dir = f"{cfg.result_dir}/rec"

        if os.path.exists(compress_dir) or os.path.exists(intermediate_dir):
            shutil.rmtree(compress_dir)
            shutil.rmtree(intermediate_dir)
            shutil.rmtree(log_dir)
            # shutil.rmtree(rec_dir)
        os.makedirs(compress_dir, exist_ok=True)
        os.makedirs(intermediate_dir, exist_ok=True)
        os.makedirs(log_dir, exist_ok=True)
        os.makedirs(rec_dir, exist_ok=True)

        for f_id, ply_file in enumerate(self.ply_filename_list[:self.frame_num]):
            # preprocess: fixed-point quantization
            temp_frame_dir = os.path.join(intermediate_dir, f"frame{f_id:03d}")
            os.makedirs(temp_frame_dir, exist_ok=True)
            load_ply_and_quant(ply_file, temp_frame_dir)

            # encode
            print(f"Encode frame{f_id:03d} via GeS-TM.")
            quant_ply_file = temp_frame_dir + f"/quant_splats.ply"
            encoded_bin_file = compress_dir + f"/frame{f_id:03d}.bin"
            encode_cmd = [
                './helper/ges_tm/tmc3',
                '-c',
                f"./helper/ges_tm/{self.compression_cfg['pcc_config_filename']}",
                f'--uncompressedDataPath={quant_ply_file}',
                f'--compressedStreamPath={encoded_bin_file}'
            ]

            encode_log_file = os.path.join(log_dir, f"frame{f_id:03d}_encode_log.txt")
            with open(encode_log_file, 'w') as log_file:
                result = subprocess.run(encode_cmd, 
                                    capture_output=True, 
                                    text=True, # output text rather than byte
                                    )
                log_file.write(result.stdout)
                log_file.write(result.stderr)
            
            # decode
            print(f"Decode frame{f_id:03d} via GeS-TM.")
            decoded_ply_file = temp_frame_dir + f"/decoded_quant_splats.ply"
            decode_cmd = [
                './helper/ges_tm/tmc3',
                '-c',
                './helper/ges_tm/decoder.cfg',
                f'--compressedStreamPath={encoded_bin_file}',
                f'--reconstructedDataPath={decoded_ply_file}'
            ]

            decode_log_file = os.path.join(log_dir, f"frame{f_id:03d}_decode_log.txt")
            with open(decode_log_file, 'w') as log_file:

                start_time = time.time()
                result = subprocess.run(decode_cmd, 
                                    capture_output=True, 
                                    text=True, # output text rather than byte
                                    )
                
                end_time = time.time()
                elapsed_time = end_time - start_time
                
                log_file.write(result.stdout)
                log_file.write(f"\nExecution time of decoding: {elapsed_time:.3f} seconds\n")

            print(f"Execution time of decoding: {elapsed_time:.3f} seconds")

            # postprocess
            output_filename = os.path.join(rec_dir, f"frame{f_id:03d}.ply")
            inverse_load_ply(decoded_ply_file, output_filename)    

        splats_list_c = self.load_ply_sequences(rec_dir, self.frame_num)
        return splats_list_c

    def rasterize_splats(
        self,
        camtoworlds: Tensor,
        Ks: Tensor,
        width: int,
        height: int,
        masks: Optional[Tensor] = None,
        splats: Optional[torch.nn.ParameterDict] = None,
        **kwargs,
    ) -> Tuple[Tensor, Tensor, Dict]:
        if splats is not None:
            means = splats["means"] # [N, 3]
            quats = splats["quats"] # [N, 4]
            scales = torch.exp(splats["scales"])  # [N, 3]
            opacities = torch.sigmoid(splats["opacities"])  # [N,]
            sh0, shN = splats["sh0"], splats["shN"]
        else:
            raise NotImplementedError(f"Should pass splats dict.")
    
        colors = torch.cat([sh0, shN], 1)  # [N, K, 3]

        rasterize_mode = "antialiased" if self.cfg.antialiased else "classic"
        render_colors, render_alphas, info = rasterization(
            means=means,
            quats=quats,
            scales=scales,
            opacities=opacities,
            colors=colors,
            viewmats=torch.linalg.inv(camtoworlds),  # [C, 4, 4]
            Ks=Ks,  # [C, 3, 3]
            width=width,
            height=height,
            packed=self.cfg.packed,
            absgrad=(
                self.cfg.strategy.absgrad
                if isinstance(self.cfg.strategy, DefaultStrategy)
                else False
            ),
            sparse_grad=self.cfg.sparse_grad,
            rasterize_mode=rasterize_mode,
            distributed=self.world_size > 1,
            camera_model=self.cfg.camera_model,
            **kwargs,
        )
        if masks is not None:
            render_colors[~masks] = 0
        return render_colors, render_alphas, info
    
    @torch.no_grad()
    def eval(self, stage: str = "val", splats_list: Optional[List[Dict]] = None):
        """Entry for evaluation."""
        print("Running evaluation...")
        cfg = self.cfg
        device = self.device
        world_rank = self.world_rank
        world_size = self.world_size        

        # dict to save metrics of each frame
        seq_stats = defaultdict(dict) 

        # if splats_list is not provided, use the default splats_list
        if splats_list is None:
            splats_list_to_render = self.splats_list
        else:
            splats_list_to_render = splats_list
            if not isinstance(splats_list_to_render[0], torch.nn.ParameterDict):
                splats_list_to_render = [
                    torch.nn.ParameterDict({
                        k: torch.nn.Parameter(v) if isinstance(v, torch.Tensor) else v 
                        for k, v in splats.items()
                    }) 
                    for splats in splats_list_to_render
                ]

        # loop on frame
        for f_id, (splats, val_dataset, train_dataset) in enumerate(zip(splats_list_to_render, self.valset_list, self.trainset_list)):
            valloader = torch.utils.data.DataLoader(
                val_dataset, batch_size=1, shuffle=False, num_workers=1
            )
            ellipse_time = 0
            metrics = defaultdict(list)            
            # loop on view
            for v_id, data in enumerate(valloader):
                camtoworlds = data["camtoworld"].to(device)
                Ks = data["K"].to(device)
                pixels = data["image"].to(device) / 255.0
                masks = data["mask"].to(device) if "mask" in data else None
                height, width = pixels.shape[1:3]
                splats = splats.to(device)

                torch.cuda.synchronize()
                tic = time.time()
                colors, _, _ = self.rasterize_splats(
                    camtoworlds=camtoworlds,
                    Ks=Ks,
                    width=width,
                    height=height,
                    sh_degree=cfg.sh_degree,
                    near_plane=cfg.near_plane,
                    far_plane=cfg.far_plane,
                    masks=masks,
                    splats=splats # must need
                )  # [1, H, W, 3]
                torch.cuda.synchronize()
                ellipse_time += time.time() - tic

                colors = torch.clamp(colors, 0.0, 1.0)
                canvas_list = [pixels, colors]

                if world_rank == 0:
                    # write images 
                    # canvas = torch.cat(canvas_list, dim=2).squeeze(0).cpu().numpy() # side by side
                    canvas = canvas_list[1].squeeze(0).cpu().numpy() # signle image
                    canvas = (canvas * 255).astype(np.uint8)

                    # save img with imageio (relatively slow)
                    # imageio.imwrite(
                    #     f"{self.render_dir}/{stage}_frame{f_id:03d}_testv{v_id:03d}.png",
                    #     canvas,
                    # )

                    # save img with fpnge (relatively fast)
                    png = fpnge.fromNP(canvas) # fpnge needs tensor in order as [H,W,C]
                    with open(f"{self.render_dir}/{stage}_frame{f_id:03d}_testv{v_id:03d}.png", "wb") as f:
                        f.write(png)

                    pixels_p = pixels.permute(0, 3, 1, 2)  # [1, 3, H, W]
                    colors_p = colors.permute(0, 3, 1, 2)  # [1, 3, H, W]
                    metrics["psnr"].append(self.psnr(colors_p, pixels_p))
                    metrics["ssim"].append(self.ssim(colors_p, pixels_p))
                    if self.cfg.with_lpips:
                        try:
                            metrics["lpips"].append(self.lpips(colors_p, pixels_p))
                        except Exception as e:
                            print(f"Error in LPIPS calculation: {e}")
                            metrics["lpips"].append(torch.tensor(float('nan')))
                    else:
                        metrics["lpips"].append(torch.tensor(float('nan')))
        
            if world_rank == 0:
                ellipse_time /= len(valloader)

                stats = {k: torch.stack(v).mean().item() for k, v in metrics.items()}
                stats.update(
                    {
                        "ellipse_time": ellipse_time,
                        "num_GS": len(splats["means"]),
                    }
                )
                print(
                    f"Metrics on frame{f_id}:"
                    f"PSNR: {stats['psnr']:.3f}, SSIM: {stats['ssim']:.4f}, LPIPS: {stats['lpips']:.3f} "
                    f"Time: {stats['ellipse_time']:.3f}s/image "
                    f"Number of GS: {stats['num_GS']}"
                )
                # write into dict
                seq_stats[f"frame{f_id:03d}"] = stats

        # calculate average
        total_metrics = {k: 0 for k, v in stats.items()}
        frame_count = len(seq_stats)
        
        for frame, metrics in seq_stats.items():
            for metric, value in metrics.items():
                total_metrics[metric] += value
        
        avg_metrics = {metric: total/frame_count for metric, total in total_metrics.items()}

        seq_stats[f"average"] = avg_metrics
        print(
            f"Average Metrics:"
            f"PSNR: {avg_metrics['psnr']:.3f}, SSIM: {avg_metrics['ssim']:.4f}, LPIPS: {avg_metrics['lpips']:.3f} "
            f"Time: {avg_metrics['ellipse_time']:.3f}s/image "
            f"Number of GS: {avg_metrics['num_GS']}"
        )
        # save metrics
        with open(f"{self.stats_dir}/{stage}.json", "w") as f:
            json.dump(seq_stats, f, indent=4)        

        return seq_stats

    def eval_pngs_with_gsc_ctc_metrics(self, ):
        from helper.mpeg_gsc.gsc_metric import run_QMIV_metric_for_pngs, run_LPIPS_for_pngs
        from pathlib import Path
        height, width = self.valset_list[0][0]["image"].shape[0:2]
        resolution = f"{width}x{height}"

        os.makedirs(f"{self.cfg.result_dir}/logs", exist_ok=True)

        gsc_metrics_across_test_views = defaultdict(dict)
        
        # Create progress bar
        test_view_ids = self.valset_list[0].indices
        pbar = tqdm(range(len(test_view_ids)), desc="Calculating quality metrics")
        
        for i, test_view_id in enumerate(test_view_ids):
            render_png_filename = Path(f"{self.cfg.result_dir}/renders/compress_frame{{:03d}}_testv{test_view_id:03d}.png")
            ref_png_filename = Path(f"{self.cfg.result_dir}/renders/val_frame{{:03d}}_testv{test_view_id:03d}.png")
            saved_log_file = Path(f"{self.cfg.result_dir}/logs/QMIV_testv{test_view_id:03d}.txt")


            # Record QMIV timing
            start_time = time.time()
            gsc_metrics = run_QMIV_metric_for_pngs(render_png_filename,
                                                ref_png_filename,
                                                resolution=resolution,
                                                saved_log_file=saved_log_file)
            qmiv_time = time.time() - start_time
            
            # Record LPIPS timing
            if self.cfg.with_lpips:
                start_time = time.time()
                lpips_dict = run_LPIPS_for_pngs(render_png_filename,
                                            ref_png_filename,
                                            lpips_calculator=self.lpips)
                lpips_time = time.time() - start_time
                gsc_metrics.update(lpips_dict)
            else:
                lpips_time = 0.0
                gsc_metrics.update({"LPIPS": float('nan')})
            
            gsc_metrics_across_test_views[f"testv{test_view_id:03d}"] = gsc_metrics
            
            # Update progress bar with timing info
            pbar.set_postfix({
                'QMIV': f'{qmiv_time:.1f}s',
                'LPIPS': f'{lpips_time:.1f}s',
                'Total': f'{qmiv_time + lpips_time:.1f}s'
            })
            pbar.update(1)
        
        pbar.close()

        metric_names = gsc_metrics_across_test_views[f"testv{0:03d}"].keys()
        for metric in metric_names:
            total = sum(gsc_metrics_across_test_views[f"testv{i:03d}"][metric] 
                    for i in range(len(self.cfg.test_view_id)))
            gsc_metrics_across_test_views["average"][metric] = total / len(self.cfg.test_view_id)
        
        # save quality metrics from each views and average metrics
        with open(os.path.join(self.cfg.result_dir, "stats", "gsc_metrics.json"), "w") as fp:
            json.dump(gsc_metrics_across_test_views, fp, indent=4)

    def summary(self,):
        import pandas as pd
        def format_size(size_bytes):
            """Convert byte size to readable format (KB, MB, GB, etc.)"""
            if size_bytes < 1024:
                return f"{size_bytes} B"
            elif size_bytes < 1024**2:
                return f"{size_bytes/1024:.2f} KB"
            elif size_bytes < 1024**3:
                return f"{size_bytes/(1024**2):.2f} MB"
            else:
                return f"{size_bytes/(1024**3):.2f} GB"
            
        ### rate summary
        directory_path = os.path.join(self.cfg.result_dir, "compression")
        
        # Check if directory exists
        if not os.path.exists(directory_path):
            print(f"Error: Directory '{directory_path}' does not exist")
            return
        
        # Store file and size information
        file_sizes = {}
        total_size = 0
        
        # Get file sizes from files in the "compression" directory
        for item in os.listdir(directory_path):
            item_path = os.path.join(directory_path, item)
            if os.path.isfile(item_path):
                size = os.path.getsize(item_path)
                file_sizes[item] = size
                total_size += size

        # Get bitrate
        Byte_to_Kbps = lambda filesize, n_frame: filesize / 1024 / n_frame * 8 * 30
        bitrate = Byte_to_Kbps(total_size, self.frame_num)

        # Calculate percentage
        percentages = {name: (size / total_size) * 100 for name, size in file_sizes.items()}
    
        # Create table data
        table_data = []
        for name, size in sorted(file_sizes.items(), key=lambda x: x[1], reverse=True):
            size_formatted = format_size(size)
            percentage = percentages[name]
            table_data.append([name, size_formatted, f"{percentage:.2f}%"])
        
        # Create pandas DataFrame for table
        df = pd.DataFrame(table_data, columns=["Filename", "Size", "Percentage"])
        csv_path = os.path.join(self.cfg.result_dir, "stats", "memory_breakdown.csv")
        df.to_csv(csv_path, index=False)
        print(f"CSV file saved to: {csv_path}")

        ### distortion summary
        # compressed vs GT
        with open(os.path.join(self.cfg.result_dir, "stats", "compress.json"), "r") as fp:
            quality_metrics = json.load(fp)
            avg_quality_metrics = quality_metrics["average"]
        
        # compressed vs val (before compression vs after compression)
        with open(os.path.join(self.cfg.result_dir, "stats", "gsc_metrics.json"), "r") as fp:
            gsc_metrics = json.load(fp)
            avg_gsc_metrics = gsc_metrics["average"]
        
        # save summary into a json file
        rd_summary = {"quality_vs_GT":{key: value for key, value in avg_quality_metrics.items() if key != "ellipse_time"}}
        rd_summary["quality_vs_val"] = {key: value for key, value in avg_gsc_metrics.items()}
        rd_summary["bitrate"] = bitrate
        rd_summary["total_size"] = format_size(total_size)
        rd_summary["total_size_bytes"] = total_size
        with open(os.path.join(self.cfg.result_dir, "summary.json"), "w") as fp:
            json.dump(rd_summary, fp, indent=4)

    def stack_render_img_to_vid(self):
        # remove existing video files
        for ext in ["*.mp4", "*.yuv"]:
            for file in glob.glob(os.path.join(self.cfg.result_dir, "renders", ext)):
                os.remove(file)

        for stage in ["compress", "val"]:
            for test_view_id in range(len(self.cfg.test_view_id)):
                # png sequence to mp4 for visualization
                cmd = (f'ffmpeg -framerate 30 -i "{self.cfg.result_dir}/renders/{stage}_frame%03d_testv{test_view_id:03d}.png" '
                    f'-c:v libx264 -pix_fmt yuv420p -crf 20 -preset medium '
                    f'-profile:v high -level 4.1 -movflags +faststart "{self.cfg.result_dir}/renders/{stage}_testv{test_view_id:03d}.mp4"')
                
                try:
                    subprocess.run(cmd, shell=True, check=True, capture_output=True, text=True)
                except subprocess.CalledProcessError as e:
                    print(f"Error running ffmpeg command for {stage}, test view {test_view_id}:")
                    print(f"Command: {cmd}")
                    print(f"Error output: {e.stderr}")
    
    def test_transform(self):
        '''
        Test the transform and inverse transform of the splats list
        '''
        splats_list = self.splats_list
        splats_list = self.compression_method.param_transform(splats_list, self.cfg.compression_cfg.transform_attributes)
        splats_list = self.compression_method.param_inverse_transform(splats_list, self.cfg.compression_cfg.transform_attributes)

        seq_stats = self.eval(splats_list=splats_list)

        return seq_stats

    def compare_render_stats(self, stats1: Dict, stats2: Dict, name1: str = "Original", name2: str = "Modified") -> None:
        """
        Compare rendering statistics between two sets of results.
        
        Args:
            stats1 (Dict): First set of rendering statistics
            stats2 (Dict): Second set of rendering statistics
            name1 (str): Name/label for the first set of statistics (default: "Original")
            name2 (str): Name/label for the second set of statistics (default: "Modified")
        
        The function prints a comparison of PSNR, SSIM, and LPIPS metrics for each frame
        and the average across all frames, showing the difference between the two sets.
        """
        print(f"\n=== Rendering Comparison: {name1} vs {name2} ===")
        
        for frame_id in stats1.keys():
            if frame_id == "average":
                print("\n=== Average Metrics Comparison ===")
            else:
                print(f"\n=== Frame {frame_id} Comparison ===")
            
            metrics1 = stats1[frame_id]
            metrics2 = stats2[frame_id]
            
            for metric in ["psnr", "ssim", "lpips"]:
                value1 = metrics1[metric]
                value2 = metrics2[metric]
                diff = value2 - value1
                diff_str = f"+{diff:.4f}" if diff > 0 else f"{diff:.4f}"
                
                print(f"{metric.upper():<8}: {value1:.4f} -> {value2:.4f} ({diff_str})")

def create_dir(path):
    if os.path.exists(path):
        shutil.rmtree(path)
    os.makedirs(path)

def main(local_rank: int, world_rank, world_size: int, cfg: Config):
    runner = Runner(local_rank, world_rank, world_size, cfg)
    
    render_stats = runner.eval('val')

    ### function test for transform and inverse transform
    # transform_stats = runner.test_transform()
    # runner.compare_render_stats(render_stats, transform_stats)
    compress_dir = f"{cfg.result_dir}/compression"
    if not cfg.decode_only:
        create_dir(compress_dir)

    if cfg.anchor_type == "video": # TODO: change "video" to "video_deprecated"
        splats_list_c = runner.compress(compress_dir)
    elif cfg.anchor_type == "video_codec": # TODO: change "video_codec" to "video"
        if not cfg.decode_only:
            runner.video_encode(compress_dir)
        splats_list_c = runner.video_decode(compress_dir)
    elif cfg.anchor_type == "pcc":
        splats_list_c = runner.pcc_compress(compress_dir)
    else:
        raise NotImplementedError(f"{cfg.anchor_type} Anchor has not been implemented.")
    
    # save the sequences of decoded gaussian splats into the sequences of ply files
    decoded_ply_dir = f"{cfg.result_dir}/decoded_ply"
    create_dir(decoded_ply_dir)
    for idx, splats_c in enumerate(splats_list_c):
        decoded_ply_filename = os.path.basename(runner.ply_filename_list[idx])
        save_ply(splats_c, os.path.join(decoded_ply_dir, decoded_ply_filename))
    
    for splats, splats_c in zip(runner.splats_list, splats_list_c):
        for k in splats.keys():
            splats[k].data = splats_c[k].to(runner.device)
           
    compress_stats = runner.eval(stage="compress")
    runner.compare_render_stats(render_stats, compress_stats, name1="Uncompressed", name2="Compressed")

    runner.stack_render_img_to_vid()
    runner.eval_pngs_with_gsc_ctc_metrics()
    # runner.summary()

if __name__ == "__main__":
    configs = {
        "seq_yuv_codec_debug": (
            "Use SeqYUVCodec.",
            Config(
                anchor_type="video_codec",
                compression="seq_yuv_codec",
                compression_cfg=SeqYUVCodecConfig(
                    transform_attributes={
                        "means": False,
                        "opacities": False,
                        "quats": True,
                        "scales": False,
                        "sh0": False,
                        "shN": False,
                    },
                )
            )
        ),
        "hm_debug": (
            "Use SeqYUVCodec, and use HM to encode the yuv file.",
            Config(
                anchor_type="video_codec",
                compression="seq_yuv_codec",
                compression_cfg=SeqYUVCodecConfig(
                    attribute_configs={
                        "means": {"qp": -1, "pix_fmt": "yuv444p"},
                        "opacities": {"qp": 4, "pix_fmt": "yuv400p"},
                        "quats": {
                            "w": {"qp": 4, "pix_fmt": "yuv400p"},
                            "xyz": {"qp": 4, "pix_fmt": "yuv444p"},
                        },
                        "scales": {"qp": 4, "pix_fmt": "yuv444p"},
                        "sh0": {"qp": 4, "pix_fmt": "yuv444p"},
                        "shN": {
                            "sh1": {"qp": 4, "pix_fmt": "yuv444p"},
                            "sh2": {"qp": 4, "pix_fmt": "yuv444p"},
                            "sh3": {"qp": 4, "pix_fmt": "yuv444p"},
                        },
                        "default": {"qp": -1, "pix_fmt": "yuv444p"},
                    }
                )
            )
        ),
        "old_rp0": (
            "Use SeqYUVCodec.",
            Config(
                anchor_type="video_codec",
                compression="seq_yuv_codec",
                compression_cfg=SeqYUVCodecConfig(
                    attribute_configs={
                        "means": {"qp": -1, "pix_fmt": "yuv444p"},
                        "opacities": {"qp": 4, "pix_fmt": "yuv400p"},
                        "quats": {
                            "w": {"qp": 4, "pix_fmt": "yuv400p"},
                            "xyz": {"qp": 4, "pix_fmt": "yuv444p"},
                        },
                        "scales": {"qp": 4, "pix_fmt": "yuv444p"},
                        "sh0": {"qp": 4, "pix_fmt": "yuv444p"},
                        "shN": {
                            "sh1": {"qp": 4, "pix_fmt": "yuv444p"},
                            "sh2": {"qp": 4, "pix_fmt": "yuv444p"},
                            "sh3": {"qp": 4, "pix_fmt": "yuv444p"},
                        },
                        "default": {"qp": -1, "pix_fmt": "yuv444p"},
                    }
                )
            )
        ),
        "old_rp1": (
            "Use SeqYUVCodec.",
            Config(
                anchor_type="video_codec",
                compression="seq_yuv_codec",
                compression_cfg=SeqYUVCodecConfig(
                    attribute_configs={
                        "means": {"qp": -1, "pix_fmt": "yuv444p"},
                        "opacities": {"qp": 4, "pix_fmt": "yuv400p"},
                        "quats": {
                            "w": {"qp": 10, "pix_fmt": "yuv400p"},
                            "xyz": {"qp": 10, "pix_fmt": "yuv444p"},
                        },
                        "scales": {"qp": 10, "pix_fmt": "yuv444p"},
                        "sh0": {"qp": 4, "pix_fmt": "yuv444p"},
                        "shN": {
                            "sh1": {"qp": 16, "pix_fmt": "yuv444p"},
                            "sh2": {"qp": 22, "pix_fmt": "yuv444p"},
                            "sh3": {"qp": 28, "pix_fmt": "yuv444p"},
                        },
                        "default": {"qp": -1, "pix_fmt": "yuv444p"},
                    }
                )
            )
        ),
        "old_rp2": (
            "Use SeqYUVCodec.",
            Config(
                anchor_type="video_codec",
                compression="seq_yuv_codec",
                compression_cfg=SeqYUVCodecConfig(
                    attribute_configs={
                        "means": {"qp": -1, "pix_fmt": "yuv444p"},
                        "opacities": {"qp": 10, "pix_fmt": "yuv400p"},
                        "quats": {
                            "w": {"qp": 16, "pix_fmt": "yuv400p"},
                            "xyz": {"qp": 16, "pix_fmt": "yuv444p"},
                        },
                        "scales": {"qp": 16, "pix_fmt": "yuv444p"},
                        "sh0": {"qp": 10, "pix_fmt": "yuv444p"},
                        "shN": {
                            "sh1": {"qp": 22, "pix_fmt": "yuv444p"},
                            "sh2": {"qp": 28, "pix_fmt": "yuv444p"},
                            "sh3": {"qp": 34, "pix_fmt": "yuv444p"},
                        },
                        "default": {"qp": -1, "pix_fmt": "yuv444p"},
                    }
                )
            )
        ),
        "old_rp3": (
            "Use SeqYUVCodec.",
            Config(
                anchor_type="video_codec",
                compression="seq_yuv_codec",
                compression_cfg=SeqYUVCodecConfig(
                    attribute_configs={
                        "means": {"qp": -1, "pix_fmt": "yuv444p"},
                        "opacities": {"qp": 16, "pix_fmt": "yuv400p"},
                        "quats": {
                            "w": {"qp": 22, "pix_fmt": "yuv400p"},
                            "xyz": {"qp": 22, "pix_fmt": "yuv444p"},
                        },
                        "scales": {"qp": 22, "pix_fmt": "yuv444p"},
                        "sh0": {"qp": 16, "pix_fmt": "yuv444p"},
                        "shN": {
                            "sh1": {"qp": 28, "pix_fmt": "yuv444p"},
                            "sh2": {"qp": 34, "pix_fmt": "yuv444p"},
                            "sh3": {"qp": 40, "pix_fmt": "yuv444p"},
                        },
                        "default": {"qp": -1, "pix_fmt": "yuv444p"},
                    }
                )
            )
        ),
        "rp4": (
            "Use SeqYUVCodec.",
            Config(
                anchor_type="video_codec",
                compression="seq_yuv_codec",
                compression_cfg=SeqYUVCodecConfig(
                    attribute_configs={
                        "means": {"qp": -1, "pix_fmt": "yuv444p"},
                        "opacities": {"qp": 7, "pix_fmt": "yuv400p"},
                        "quats": {
                            "w": {"qp": 2, "pix_fmt": "yuv400p"},
                            "xyz": {"qp": 2, "pix_fmt": "yuv444p"},
                        },
                        "scales": {"qp": 7, "pix_fmt": "yuv444p"},
                        "sh0": {"qp": 7, "pix_fmt": "yuv444p"},
                        "shN": {
                            "sh1": {"qp": 7, "pix_fmt": "yuv444p"},
                            "sh2": {"qp": 12, "pix_fmt": "yuv444p"},
                            "sh3": {"qp": 17, "pix_fmt": "yuv444p"},
                        },
                        "default": {"qp": -1, "pix_fmt": "yuv444p"},
                    }
                )
            )
        ),
        "rp3": (
            "Use SeqYUVCodec.",
            Config(
                anchor_type="video_codec",
                compression="seq_yuv_codec",
                compression_cfg=SeqYUVCodecConfig(
                    attribute_configs={
                        "means": {"qp": -1, "pix_fmt": "yuv444p"},
                        "opacities": {"qp": 17, "pix_fmt": "yuv400p"},
                        "quats": {
                            "w": {"qp": 2, "pix_fmt": "yuv400p"},
                            "xyz": {"qp": 2, "pix_fmt": "yuv444p"},
                        },
                        "scales": {"qp": 7, "pix_fmt": "yuv444p"},
                        "sh0": {"qp": 7, "pix_fmt": "yuv444p"},
                        "shN": {
                            "sh1": {"qp": 17, "pix_fmt": "yuv444p"},
                            "sh2": {"qp": 22, "pix_fmt": "yuv444p"},
                            "sh3": {"qp": 27, "pix_fmt": "yuv444p"},
                        },
                        "default": {"qp": -1, "pix_fmt": "yuv444p"},
                    }
                )
            )
        ),
        "rp2": (
            "Use SeqYUVCodec.",
            Config(
                anchor_type="video_codec",
                compression="seq_yuv_codec",
                compression_cfg=SeqYUVCodecConfig(
                    attribute_configs={
                        "means": {"qp": -1, "pix_fmt": "yuv444p"},
                        "opacities": {"qp": 17, "pix_fmt": "yuv400p"},
                        "quats": {
                            "w": {"qp": 7, "pix_fmt": "yuv400p"},
                            "xyz": {"qp": 7, "pix_fmt": "yuv444p"},
                        },
                        "scales": {"qp": 12, "pix_fmt": "yuv444p"},
                        "sh0": {"qp": 7, "pix_fmt": "yuv444p"},
                        "shN": {
                            "sh1": {"qp": 22, "pix_fmt": "yuv444p"},
                            "sh2": {"qp": 27, "pix_fmt": "yuv444p"},
                            "sh3": {"qp": 32, "pix_fmt": "yuv444p"},
                        },
                        "default": {"qp": -1, "pix_fmt": "yuv444p"},
                    }
                )
            )
        ),
        "rp1": (
            "Use SeqYUVCodec.",
            Config(
                anchor_type="video_codec",
                compression="seq_yuv_codec",
                compression_cfg=SeqYUVCodecConfig(
                    attribute_configs={
                        "means": {"qp": -1, "pix_fmt": "yuv444p"},
                        "opacities": {"qp": 22, "pix_fmt": "yuv400p"},
                        "quats": {
                            "w": {"qp": 17, "pix_fmt": "yuv400p"},
                            "xyz": {"qp": 17, "pix_fmt": "yuv444p"},
                        },
                        "scales": {"qp": 12, "pix_fmt": "yuv444p"},
                        "sh0": {"qp": 7, "pix_fmt": "yuv444p"},
                        "shN": {
                            "sh1": {"qp": 32, "pix_fmt": "yuv444p"},
                            "sh2": {"qp": 37, "pix_fmt": "yuv444p"},
                            "sh3": {"qp": 42, "pix_fmt": "yuv444p"},
                        },
                        "default": {"qp": -1, "pix_fmt": "yuv444p"},
                    }
                )
            )
        ),
    }
    cfg = tyro.extras.overridable_config_cli(configs)

    # try import extra dependencies
    if cfg.compression == "png":
        try:
            import plas
            import torchpq
        except:
            raise ImportError(
                "To use PNG compression, you need to install "
                "torchpq (instruction at https://github.com/DeMoriarty/TorchPQ?tab=readme-ov-file#install) "
                "and plas (via 'pip install git+https://github.com/fraunhoferhhi/PLAS.git') "
            )

    cli(main, cfg, verbose=True)
