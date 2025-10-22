import copy
import json
import math
import os
import time
import shutil
import sys
from contextlib import nullcontext
from dataclasses import dataclass, field, fields
from collections import defaultdict
from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple, Union, ContextManager, TypedDict, Any

import imageio
import nerfview
import numpy as np
import torch
import torch.nn.functional as F
import tqdm
import tyro
import viser
import yaml
from cfg_sys import (
    CONFIG_FILE_FLAGS,
    CONFIG_SAVE_FLAGS,
    DEFAULT_CONFIG_SNAPSHOT,
    build_from_registry,
    load_config_updates,
    pop_flag_value,
    prepare_presets,
    save_config_snapshot,
    synchronize_compression_config,
)
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
from gsplat.compression.entropy_coding_compression import EntropyCodingCompression
from gsplat.compression.post_training import (
    PTCompConfig,
    InputSpec,
    CodecConfig as PTCodecConfig,
    PreprocessConfig,
    PruningConfig,
    MappingConfig,
    QuantConfig,
    QuantFieldConfig,
    PostTrainingCompressor,
)
from utils import AppearanceOptModule, CameraOptModule, knn, rgb_to_sh, set_random_seed, save_ply, load_ply
from lib_bilagrid import (
    BilateralGrid,
    slice,
    color_correct,
    total_variation_loss,
)

from gsplat.compression import PngCompression, HevcCompression
from gsplat.distributed import cli
from gsplat.rendering import rasterization
from gsplat.strategy import DefaultStrategy, MCMCStrategy, Strategy
from gsplat.optimizers import SelectiveAdam

from gsplat.compression_simulation import (
    CompSimConfig,
    NullCompressionSimulation,
    DefaultCompressionSimulation,
)
from gsplat.compression_simulation.entropy_model import Entropy_factorized_optimized_refactor, Entropy_gaussian

class ProfilerConfig:
    def __init__(self):
        self.enabled = False
        self.activities = [
            torch.profiler.ProfilerActivity.CPU,
            torch.profiler.ProfilerActivity.CUDA,
        ]
        
        self.wait = 1 
        self.warmup = 2 
        self.active = 30_000 
        
        self.schedule = self._create_schedule()
        
        self.on_trace_ready = torch.profiler.tensorboard_trace_handler('./log/profiler')
        self.record_shapes = True
        self.profile_memory = True
        self.with_stack = True
    
    def _create_schedule(self):
        return torch.profiler.schedule(
            wait=self.wait,
            warmup=self.warmup,
            active=self.active,
        )
    
    def update_schedule(self, **kwargs):
        for key, value in kwargs.items():
            if hasattr(self, key):
                setattr(self, key, value)
        self.schedule = self._create_schedule()


@dataclass
class Config:
    # Disable viewer
    disable_viewer: bool = False
    # Mode of operation: 'train', 'compress'
    mode: Literal["train", "compress"] = "train"
    # Path to the .pt files (required for eval/compress mode)
    ckpt: Optional[List[str]] = None

    # Enable profiler
    profiler_enabled: bool = False

    # Configuration for compression simulation during training
    compression_sim_cfg: CompSimConfig = field(default_factory=CompSimConfig)

    # Configuration for post-training compression
    post_training_comp_cfg: PTCompConfig = field(default_factory=PTCompConfig)
    
    # Render trajectory path
    render_traj_path: str = "interp"

    # Path to the Mip-NeRF 360 dataset
    data_dir: str = "data/360_v2/garden"
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
    test_view_id: Optional[List[int]] = None

    lpips_net: Literal["vgg", "alex"] = "alex"

    def adjust_steps(self, factor: float):
        self.eval_steps = [int(i * factor) for i in self.eval_steps]
        self.save_steps = [int(i * factor) for i in self.save_steps]
        self.max_steps = int(self.max_steps * factor)
        self.sh_degree_interval = int(self.sh_degree_interval * factor)

        strategy = self.strategy
        if isinstance(strategy, DefaultStrategy):
            strategy.refine_start_iter = int(strategy.refine_start_iter * factor)
            strategy.refine_stop_iter = int(strategy.refine_stop_iter * factor)
            strategy.reset_every = int(strategy.reset_every * factor)
            strategy.refine_every = int(strategy.refine_every * factor)
        elif isinstance(strategy, MCMCStrategy):
            strategy.refine_start_iter = int(strategy.refine_start_iter * factor)
            strategy.refine_stop_iter = int(strategy.refine_stop_iter * factor)
            strategy.refine_every = int(strategy.refine_every * factor)
        else:
            assert_never(strategy)


_STRATEGY_REGISTRY = {
    "DefaultStrategy": DefaultStrategy,
    "MCMCStrategy": MCMCStrategy,
}

def _strategy_registry_handler(
    value: Any,
    current: Optional[Strategy],
    path: str,
    apply_fn: Callable[[Any, Dict[str, Any], str], None],
):
    is_training_strategy = False

    if isinstance(value, dict):
        type_name = value.get("type")
        if type_name in _STRATEGY_REGISTRY:
            is_training_strategy = True
        elif isinstance(current, Strategy):
            field_names = {f.name for f in fields(type(current))}
            if set(value.keys()).issubset(field_names):
                is_training_strategy = True
    elif isinstance(value, str) and value in _STRATEGY_REGISTRY:
        is_training_strategy = True
    elif isinstance(current, Strategy):
        is_training_strategy = True

    if not is_training_strategy:
        return current if isinstance(current, Strategy) else value

    return build_from_registry(value, _STRATEGY_REGISTRY, current, apply_fn=apply_fn, path=path)

_REGISTRY_HANDLERS = {
    "strategy": _strategy_registry_handler,
}

def _strategy_serializer(value: Any, recurse: Callable[[Any], Any]) -> Optional[Any]:
    if isinstance(value, Strategy):
        payload = {f.name: recurse(getattr(value, f.name)) for f in fields(value)}
        return {"type": type(value).__name__, "params": payload}
    return None

_CONFIG_SERIALIZERS = [_strategy_serializer]

def create_splats_with_optimizers(
    parser: Parser,
    init_type: str = "sfm",
    init_num_pts: int = 100_000,
    init_extent: float = 3.0,
    init_opacity: float = 0.1,
    init_scale: float = 1.0,
    scene_scale: float = 1.0,
    sh_degree: int = 3,
    sparse_grad: bool = False,
    visible_adam: bool = False,
    batch_size: int = 1,
    feature_dim: Optional[int] = None,
    device: str = "cuda",
    world_rank: int = 0,
    world_size: int = 1,
) -> Tuple[torch.nn.ParameterDict, Dict[str, torch.optim.Optimizer]]:
    if init_type == "sfm":
        points = torch.from_numpy(parser.points).float()
        rgbs = torch.from_numpy(parser.points_rgb / 255.0).float()
    elif init_type == "random":
        points = init_extent * scene_scale * (torch.rand((init_num_pts, 3)) * 2 - 1)
        rgbs = torch.rand((init_num_pts, 3))
    else:
        raise ValueError("Please specify a correct init_type: sfm or random")

    # Initialize the GS size to be the average dist of the 3 nearest neighbors
    dist2_avg = (knn(points, 4)[:, 1:] ** 2).mean(dim=-1)  # [N,]
    dist_avg = torch.sqrt(dist2_avg)
    scales = torch.log(dist_avg * init_scale).unsqueeze(-1).repeat(1, 3)  # [N, 3]

    # Distribute the GSs to different ranks (also works for single rank)
    points = points[world_rank::world_size]
    rgbs = rgbs[world_rank::world_size]
    scales = scales[world_rank::world_size]

    N = points.shape[0]
    quats = torch.rand((N, 4))  # [N, 4]
    opacities = torch.logit(torch.full((N,), init_opacity))  # [N,]

    params = [
        # name, value, lr
        ("means", torch.nn.Parameter(points), 1.6e-4 * scene_scale),
        ("scales", torch.nn.Parameter(scales), 5e-3),
        ("quats", torch.nn.Parameter(quats), 1e-3),
        ("opacities", torch.nn.Parameter(opacities), 5e-2),
    ]

    if feature_dim is None:
        # color is SH coefficients.
        colors = torch.zeros((N, (sh_degree + 1) ** 2, 3))  # [N, K, 3]
        colors[:, 0, :] = rgb_to_sh(rgbs)
        params.append(("sh0", torch.nn.Parameter(colors[:, :1, :]), 2.5e-3))
        params.append(("shN", torch.nn.Parameter(colors[:, 1:, :]), 2.5e-3 / 20))
    else:
        # features will be used for appearance and view-dependent shading
        features = torch.rand(N, feature_dim)  # [N, feature_dim]
        params.append(("features", torch.nn.Parameter(features), 2.5e-3))
        colors = torch.logit(rgbs)  # [N, 3]
        params.append(("colors", torch.nn.Parameter(colors), 2.5e-3))

    splats = torch.nn.ParameterDict({n: v for n, v, _ in params}).to(device)
    # Scale learning rate based on batch size, reference:
    # https://www.cs.princeton.edu/~smalladi/blog/2024/01/22/SDEs-ScalingRules/
    # Note that this would not make the training exactly equivalent, see
    # https://arxiv.org/pdf/2402.18824v1
    BS = batch_size * world_size
    optimizer_class = None
    if sparse_grad:
        optimizer_class = torch.optim.SparseAdam
    elif visible_adam:
        optimizer_class = SelectiveAdam
    else:
        optimizer_class = torch.optim.Adam
    optimizers = {
        name: optimizer_class(
            [{"params": splats[name], "lr": lr * math.sqrt(BS), "name": name}],
            eps=1e-15 / math.sqrt(BS),
            # TODO: check betas logic when BS is larger than 10 betas[0] will be zero.
            betas=(1 - BS * (1 - 0.9), 1 - BS * (1 - 0.999)),
        )
        for name, _, lr in params
    }
    return splats, optimizers


class Runner:
    """Engine for training and testing."""

    def __init__(
        self, local_rank: int, world_rank, world_size: int, cfg: Config
    ) -> None:
        set_random_seed(42 + local_rank)

        self.cfg = cfg
        self.world_rank = world_rank
        self.local_rank = local_rank
        self.world_size = world_size
        self.device = f"cuda:{local_rank}"

        # Where to dump results.
        os.makedirs(cfg.result_dir, exist_ok=True)

        # Setup output directories.
        self.ckpt_dir = f"{cfg.result_dir}/ckpts"
        os.makedirs(self.ckpt_dir, exist_ok=True)
        self.stats_dir = f"{cfg.result_dir}/stats"
        os.makedirs(self.stats_dir, exist_ok=True)
        self.render_dir = f"{cfg.result_dir}/renders"
        os.makedirs(self.render_dir, exist_ok=True)

        # Tensorboard
        self.writer = SummaryWriter(log_dir=f"{cfg.result_dir}/tb")

        # Load data: Training data should contain initial points and colors.
        self.parser = Parser(
            data_dir=cfg.data_dir,
            factor=cfg.data_factor,
            normalize=cfg.normalize_world_space,
            test_every=cfg.test_every,
        )
        if cfg.scene_type == "GSC" and cfg.test_view_id is not None: # GSC mode
            self.trainset = GSCDataset(
                self.parser,
                split="train",
                patch_size=cfg.patch_size,
                load_depths=cfg.depth_loss,
                test_view_ids=cfg.test_view_id,
            )
            self.valset = GSCDataset(
                self.parser, 
                split="val", 
                test_view_ids=cfg.test_view_id,)
        else: # default mode
            self.trainset = Dataset(
                self.parser,
                split="train",
                patch_size=cfg.patch_size,
                load_depths=cfg.depth_loss,
            )
            self.valset = Dataset(self.parser, split="val")
        self.scene_scale = self.parser.scene_scale * 1.1 * cfg.global_scale
        print("Scene scale:", self.scene_scale)

        # Model
        feature_dim = 32 if cfg.app_opt else None
        self.splats, self.optimizers = create_splats_with_optimizers(
            self.parser,
            init_type=cfg.init_type,
            init_num_pts=cfg.init_num_pts,
            init_extent=cfg.init_extent,
            init_opacity=cfg.init_opa,
            init_scale=cfg.init_scale,
            scene_scale=self.scene_scale,
            sh_degree=cfg.sh_degree,
            sparse_grad=cfg.sparse_grad,
            visible_adam=cfg.visible_adam,
            batch_size=cfg.batch_size,
            feature_dim=feature_dim,
            device=self.device,
            world_rank=world_rank,
            world_size=world_size,
        )
        print("Model initialized. Number of GS:", len(self.splats["means"]))

        # Densification Strategy
        self.cfg.strategy.check_sanity(self.splats, self.optimizers)

        if isinstance(self.cfg.strategy, DefaultStrategy):
            self.strategy_state = self.cfg.strategy.initialize_state(
                scene_scale=self.scene_scale
            )
        elif isinstance(self.cfg.strategy, MCMCStrategy):
            self.strategy_state = self.cfg.strategy.initialize_state()
        else:
            assert_never(self.cfg.strategy)

        self.entropy_min_step = 0

        # Compression Strategy
        # self.compression_method = None
        # if cfg.compression is not None:
        #     if cfg.compression == "png":
        #         self.compression_method = PngCompression()
        #     elif  cfg.compression == "entropy_coding":
        #         compression_cfg = cfg.compression_cfg.to_dict()
        #         self.compression_method = EntropyCodingCompression(**compression_cfg)
        #     elif cfg.compression == "hevc":
        #         compression_cfg = cfg.compression_cfg.to_dict()
        #         self.compression_method = HevcCompression(**compression_cfg)
        #     else:
        #         raise ValueError(f"Unknown compression strategy: {cfg.compression}")
        
        self.comp_sim_config = copy.deepcopy(cfg.compression_sim_cfg)

        self.compression_simulation = NullCompressionSimulation(device=self.device)

        self.comp_sim_result = None
        self.esti_bits_dict = {name: None for name in self.comp_sim_config.quantizer.attributes.keys()}

        self.entropy_models: Dict[str, torch.nn.Module] = {}

        if self.comp_sim_config.enabled:
            self.compression_simulation = DefaultCompressionSimulation(
                config=self.comp_sim_config,
                device=self.device,
            )
            self.entropy_models = self.compression_simulation.get_entropy_models()
            if self.comp_sim_config.entropy.enabled:
                steps = self.comp_sim_config.entropy.steps
                positives = [k for k, v in steps.items() if v > 0]
                if positives:
                    selected_key = min(positives, key=lambda k: steps[k])
                    self.entropy_min_step = steps[selected_key]
        
        # Handle post_training_comp_cfg, similar to compression_sim_cfg pattern
        self.pt_comp_config = copy.deepcopy(cfg.post_training_comp_cfg)
        
        # Dynamically adjust input_spec based on ckpt parameter
        if cfg.ckpt is not None and len(cfg.ckpt) > 0:
            self.pt_comp_config.input_spec.path = Path(cfg.ckpt[0])
            self.pt_comp_config.input_spec.input_type = "ckpt"
        
        # Profiler
        self.profiler: Optional[torch.profiler.profile] = None
        self.profiler_config = ProfilerConfig()
        if cfg.profiler_enabled:
            self.profiler_config.enabled = True

        self.pose_optimizers = []
        if cfg.pose_opt:
            self.pose_adjust = CameraOptModule(len(self.trainset)).to(self.device)
            self.pose_adjust.zero_init()
            self.pose_optimizers = [
                torch.optim.Adam(
                    self.pose_adjust.parameters(),
                    lr=cfg.pose_opt_lr * math.sqrt(cfg.batch_size),
                    weight_decay=cfg.pose_opt_reg,
                )
            ]
            if world_size > 1:
                self.pose_adjust = DDP(self.pose_adjust)

        if cfg.pose_noise > 0.0:
            self.pose_perturb = CameraOptModule(len(self.trainset)).to(self.device)
            self.pose_perturb.random_init(cfg.pose_noise)
            if world_size > 1:
                self.pose_perturb = DDP(self.pose_perturb)

        self.app_optimizers = []
        if cfg.app_opt:
            assert feature_dim is not None
            self.app_module = AppearanceOptModule(
                len(self.trainset), feature_dim, cfg.app_embed_dim, cfg.sh_degree
            ).to(self.device)
            # initialize the last layer to be zero so that the initial output is zero.
            torch.nn.init.zeros_(self.app_module.color_head[-1].weight)
            torch.nn.init.zeros_(self.app_module.color_head[-1].bias)
            self.app_optimizers = [
                torch.optim.Adam(
                    self.app_module.embeds.parameters(),
                    lr=cfg.app_opt_lr * math.sqrt(cfg.batch_size) * 10.0,
                    weight_decay=cfg.app_opt_reg,
                ),
                torch.optim.Adam(
                    self.app_module.color_head.parameters(),
                    lr=cfg.app_opt_lr * math.sqrt(cfg.batch_size),
                ),
            ]
            if world_size > 1:
                self.app_module = DDP(self.app_module)

        self.bil_grid_optimizers = []
        if cfg.use_bilateral_grid:
            self.bil_grids = BilateralGrid(
                len(self.trainset),
                grid_X=cfg.bilateral_grid_shape[0],
                grid_Y=cfg.bilateral_grid_shape[1],
                grid_W=cfg.bilateral_grid_shape[2],
            ).to(self.device)
            self.bil_grid_optimizers = [
                torch.optim.Adam(
                    self.bil_grids.parameters(),
                    lr=2e-3 * math.sqrt(cfg.batch_size),
                    eps=1e-15,
                ),
            ]

        # Losses & Metrics.
        self.ssim = StructuralSimilarityIndexMeasure(data_range=1.0).to(self.device)
        self.psnr = PeakSignalNoiseRatio(data_range=1.0).to(self.device)

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

        # Viewer
        if not self.cfg.disable_viewer:
            self.server = viser.ViserServer(port=cfg.port, verbose=False)
            self.viewer = nerfview.Viewer(
                server=self.server,
                render_fn=self._viewer_render_fn,
                mode="training",
            )

    def get_profiler(self, tb_writer) -> ContextManager:
        if self.profiler_config.enabled:
            return torch.profiler.profile(
                activities=self.profiler_config.activities,
                schedule=self.profiler_config.schedule,
                # on_trace_ready=self.profiler_config.on_trace_ready, 
                on_trace_ready=torch.profiler.tensorboard_trace_handler(tb_writer.log_dir),
                record_shapes=self.profiler_config.record_shapes,
                profile_memory=self.profiler_config.profile_memory,
                with_stack=self.profiler_config.with_stack
            )
        return nullcontext()

    def step_profiler(self):
        """step profiler"""
        if self.profiler is not None:
            self.profiler.step()

    def rasterize_splats(
        self,
        camtoworlds: Tensor,
        Ks: Tensor,
        width: int,
        height: int,
        masks: Optional[Tensor] = None,
        **kwargs,
    ) -> Tuple[Tensor, Tensor, Dict]:
        active_splats = self.comp_sim_result.splats if self.comp_sim_result is not None else self.splats
        means = active_splats["means"]  # [N, 3]
        quats = active_splats["quats"]  # [N, 4]
        scales = torch.exp(active_splats["scales"])  # [N, 3]
        opacities = torch.sigmoid(active_splats["opacities"])  # [N,]
        sh0, shN = active_splats["sh0"], active_splats["shN"]

        image_ids = kwargs.pop("image_ids", None)
        if self.cfg.app_opt:
            colors = self.app_module(
                features=self.splats["features"],
                embed_ids=image_ids,
                dirs=means[None, :, :] - camtoworlds[:, None, :3, 3],
                sh_degree=kwargs.pop("sh_degree", self.cfg.sh_degree),
            )
            colors = colors + self.splats["colors"]
            colors = torch.sigmoid(colors)
        else:
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

    def train(self):
        cfg = self.cfg
        device = self.device
        world_rank = self.world_rank
        world_size = self.world_size

        # Dump cfg.
        if world_rank == 0:
            with open(f"{cfg.result_dir}/cfg.yml", "w") as f:
                yaml.dump(vars(cfg), f)

        max_steps = cfg.max_steps
        init_step = 0

        schedulers = [
            # means has a learning rate schedule, that end at 0.01 of the initial value
            torch.optim.lr_scheduler.ExponentialLR(
                self.optimizers["means"], gamma=0.01 ** (1.0 / max_steps)
            ),
        ]
        if cfg.pose_opt:
            # pose optimization has a learning rate schedule
            schedulers.append(
                torch.optim.lr_scheduler.ExponentialLR(
                    self.pose_optimizers[0], gamma=0.01 ** (1.0 / max_steps)
                )
            )
        if cfg.use_bilateral_grid:
            # bilateral grid has a learning rate schedule. Linear warmup for 1000 steps.
            schedulers.append(
                torch.optim.lr_scheduler.ChainedScheduler(
                    [
                        torch.optim.lr_scheduler.LinearLR(
                            self.bil_grid_optimizers[0],
                            start_factor=0.01,
                            total_iters=1000,
                        ),
                        torch.optim.lr_scheduler.ExponentialLR(
                            self.bil_grid_optimizers[0], gamma=0.01 ** (1.0 / max_steps)
                        ),
                    ]
                )
            )

        trainloader = torch.utils.data.DataLoader(
            self.trainset,
            batch_size=cfg.batch_size,
            shuffle=True,
            num_workers=4,
            persistent_workers=True,
            pin_memory=True,
        )
        trainloader_iter = iter(trainloader)

        with self.get_profiler(self.writer) as prof:
            self.profiler = prof if self.profiler_config.enabled else None

            # Training loop.
            global_tic = time.time()
            pbar = tqdm.tqdm(range(init_step, max_steps))
            for step in pbar:
                if not cfg.disable_viewer:
                    while self.viewer.state.status == "paused":
                        time.sleep(0.01)
                    self.viewer.lock.acquire()
                    tic = time.time()

                try:
                    data = next(trainloader_iter)
                except StopIteration:
                    trainloader_iter = iter(trainloader)
                    data = next(trainloader_iter)

                camtoworlds = camtoworlds_gt = data["camtoworld"].to(device)  # [1, 4, 4]
                Ks = data["K"].to(device)  # [1, 3, 3]
                pixels = data["image"].to(device) / 255.0  # [1, H, W, 3]
                num_train_rays_per_step = (
                    pixels.shape[0] * pixels.shape[1] * pixels.shape[2]
                )
                image_ids = data["image_id"].to(device)
                masks = data["mask"].to(device) if "mask" in data else None  # [1, H, W]
                if cfg.depth_loss:
                    points = data["points"].to(device)  # [1, M, 2]
                    depths_gt = data["depths"].to(device)  # [1, M]

                height, width = pixels.shape[1:3]

                if cfg.pose_noise:
                    camtoworlds = self.pose_perturb(camtoworlds, image_ids)

                if cfg.pose_opt:
                    camtoworlds = self.pose_adjust(camtoworlds, image_ids)

                # sh schedule
                sh_degree_to_use = min(step // cfg.sh_degree_interval, cfg.sh_degree)

                simulation_result = self.compression_simulation.run(self.splats, step)
                self.comp_sim_result = simulation_result
                entropy_bits = simulation_result.metrics.get("entropy_bits")
                if entropy_bits is not None:
                    for name in self.esti_bits_dict.keys():
                        self.esti_bits_dict[name] = entropy_bits.get(name)
                else:
                    for name in self.esti_bits_dict.keys():
                        self.esti_bits_dict[name] = None

                # forward
                renders, alphas, info = self.rasterize_splats(
                    camtoworlds=camtoworlds,
                    Ks=Ks,
                    width=width,
                    height=height,
                    sh_degree=sh_degree_to_use,
                    near_plane=cfg.near_plane,
                    far_plane=cfg.far_plane,
                    image_ids=image_ids,
                    render_mode="RGB+ED" if cfg.depth_loss else "RGB",
                    masks=masks,
                )
                if renders.shape[-1] == 4:
                    colors, depths = renders[..., 0:3], renders[..., 3:4]
                else:
                    colors, depths = renders, None

                if cfg.use_bilateral_grid:
                    grid_y, grid_x = torch.meshgrid(
                        (torch.arange(height, device=self.device) + 0.5) / height,
                        (torch.arange(width, device=self.device) + 0.5) / width,
                        indexing="ij",
                    )
                    grid_xy = torch.stack([grid_x, grid_y], dim=-1).unsqueeze(0)
                    colors = slice(self.bil_grids, grid_xy, colors, image_ids)["rgb"]

                if cfg.random_bkgd:
                    bkgd = torch.rand(1, 3, device=device)
                    colors = colors + bkgd * (1.0 - alphas)

                self.cfg.strategy.step_pre_backward(
                    params=self.splats,
                    optimizers=self.optimizers,
                    state=self.strategy_state,
                    step=step,
                    info=info,
                )

                # loss
                l1loss = F.l1_loss(colors, pixels)
                ssimloss = 1.0 - fused_ssim(
                    colors.permute(0, 3, 1, 2), pixels.permute(0, 3, 1, 2), padding="valid"
                )
                loss = l1loss * (1.0 - cfg.ssim_lambda) + ssimloss * cfg.ssim_lambda
                if cfg.depth_loss:
                    # query depths from depth map
                    points = torch.stack(
                        [
                            points[:, :, 0] / (width - 1) * 2 - 1,
                            points[:, :, 1] / (height - 1) * 2 - 1,
                        ],
                        dim=-1,
                    )  # normalize to [-1, 1]
                    grid = points.unsqueeze(2)  # [1, M, 1, 2]
                    depths = F.grid_sample(
                        depths.permute(0, 3, 1, 2), grid, align_corners=True
                    )  # [1, 1, M, 1]
                    depths = depths.squeeze(3).squeeze(1)  # [1, M]
                    # calculate loss in disparity space
                    disp = torch.where(depths > 0.0, 1.0 / depths, torch.zeros_like(depths))
                    disp_gt = 1.0 / depths_gt  # [1, M]
                    depthloss = F.l1_loss(disp, disp_gt) * self.scene_scale
                    loss += depthloss * cfg.depth_lambda
                if cfg.use_bilateral_grid:
                    tvloss = 10 * total_variation_loss(self.bil_grids.grids)
                    loss += tvloss

                # regularizations
                if cfg.opacity_reg > 0.0:
                    loss = (
                        loss
                        + cfg.opacity_reg
                        * torch.abs(torch.sigmoid(self.splats["opacities"])).mean()
                    )
                if cfg.scale_reg > 0.0:
                    loss = (
                        loss
                        + cfg.scale_reg * torch.abs(torch.exp(self.splats["scales"])).mean()
                    )

                total_esti_bits = None
                if self.comp_sim_result is not None:
                    for loss_name, loss_value in self.comp_sim_result.loss_terms.items():
                        if loss_value is None:
                            continue
                        if loss_name.startswith("entropy/"):
                            weighted = cfg.compression_sim_cfg.entropy.rd_lambda * loss_value
                            loss = loss + weighted
                        else: # adaptive mask loss
                            loss = loss + loss_value

                # bpp loss (w/o multiple rd_lambda)
                if cfg.compression_sim_cfg.entropy.enabled and step > self.entropy_min_step:
                    if total_esti_bits is None:
                        total_esti_bits = 0.0
                    for n, n_step in cfg.compression_sim_cfg.entropy.steps.items():
                        bits = self.esti_bits_dict.get(n)
                        if step > n_step and bits is not None:
                            total_esti_bits += torch.sum(bits) / bits.numel()

                # tmp workaround
                loss_show = loss.detach().cpu()
                loss.backward()
                
                desc = f"loss={loss_show.item():.3f}| " f"sh degree={sh_degree_to_use}| "
                if cfg.depth_loss:
                    desc += f"depth loss={depthloss.item():.6f}| "
                if cfg.pose_opt and cfg.pose_noise:
                    # monitor the pose error if we inject noise
                    pose_err = F.l1_loss(camtoworlds_gt, camtoworlds)
                    desc += f"pose err={pose_err.item():.6f}| "
                pbar.set_description(desc)

                # tensorboard monitor
                if world_rank == 0 and cfg.tb_every > 0 and step % cfg.tb_every == 0:
                    mem = torch.cuda.max_memory_allocated() / 1024**3
                    self.writer.add_scalar("train/loss", loss.item(), step)
                    self.writer.add_scalar("train/l1loss", l1loss.item(), step)
                    self.writer.add_scalar("train/ssimloss", ssimloss.item(), step)
                    self.writer.add_scalar("train/num_GS", len(self.splats["means"]), step)
                    self.writer.add_scalar("train/mem", mem, step)
                    if cfg.depth_loss:
                        self.writer.add_scalar("train/depthloss", depthloss.item(), step)
                    if cfg.use_bilateral_grid:
                        self.writer.add_scalar("train/tvloss", tvloss.item(), step)
                    if cfg.tb_save_image:
                        canvas = torch.cat([pixels, colors], dim=2).detach().cpu().numpy()
                        canvas = canvas.reshape(-1, *canvas.shape[2:])
                        self.writer.add_image("train/render", canvas, step)
                    if cfg.compression_sim_cfg.enabled:
                        if cfg.compression_sim_cfg.entropy.enabled and step>self.entropy_min_step:
                            self.writer.add_histogram("train_hist/quats", self.splats["quats"], step)
                            self.writer.add_histogram("train_hist/scales", self.splats["scales"], step)
                            self.writer.add_histogram("train_hist/opacities", self.splats["opacities"], step)
                            self.writer.add_histogram("train_hist/sh0", self.splats["sh0"], step)
                            if total_esti_bits is not None and total_esti_bits > 0:
                                self.writer.add_scalar("train/bpp_loss", total_esti_bits.item(), step)
                        mask_ratio = None
                        mask_threshold = None
                        if self.comp_sim_result is not None:
                            mask_ratio = self.comp_sim_result.metrics.get("mask_ratio")
                            mask_threshold = self.comp_sim_result.metrics.get("mask_grad_threshold")
                        if mask_ratio is not None:
                            self.writer.add_scalar("train/ada_mask_ratio", mask_ratio, step)
                        if mask_threshold is not None:
                            self.writer.add_scalar("train/ada_mask_grad_threshold", mask_threshold, step)
                        
                    self.writer.add_histogram("train_hist/means", self.splats["means"], step)
                    self.writer.flush()

                # save checkpoint before updating the model
                if step in [i - 1 for i in cfg.save_steps] or step == max_steps - 1:
                    mem = torch.cuda.max_memory_allocated() / 1024**3
                    stats = {
                        "mem": mem,
                        "ellipse_time": time.time() - global_tic,
                        "num_GS": len(self.splats["means"]),
                    }
                    print("Step: ", step, stats)
                    with open(
                        f"{self.stats_dir}/train_step{step:04d}_rank{self.world_rank}.json",
                        "w",
                    ) as f:
                        json.dump(stats, f)
                    
                    mask_binary = None
                    if cfg.compression_sim_cfg.mask.enabled and step > cfg.compression_sim_cfg.mask.start_step:
                        mask_binary = self.compression_simulation.get_mask_binary()
                        if mask_binary is not None:
                            self.splats["shN"].data = self.splats["shN"].data * mask_binary

                    # prepare data to be saved
                    data = {"step": step, "splats": self.splats.state_dict()}
                    if mask_binary is not None:
                        data["shN_ada_mask"] = mask_binary.detach().cpu()
                    if cfg.pose_opt:
                        if world_size > 1:
                            data["pose_adjust"] = self.pose_adjust.module.state_dict()
                        else:
                            data["pose_adjust"] = self.pose_adjust.state_dict()
                    if cfg.app_opt:
                        if world_size > 1:
                            data["app_module"] = self.app_module.module.state_dict()
                        else:
                            data["app_module"] = self.app_module.state_dict()

                    if cfg.compression_sim_cfg.enabled and cfg.compression_sim_cfg.entropy.enabled:
                        for name, entropy_model in self.compression_simulation.get_entropy_models().items():
                            if entropy_model is not None:
                                data[name + "_entropy_model"] = entropy_model.state_dict()

                    torch.save(
                        data, f"{self.ckpt_dir}/ckpt_{step}_rank{self.world_rank}.pt"
                    )

                
                # Turn Gradients into Sparse Tensor before running optimizer
                if cfg.sparse_grad:
                    assert cfg.packed, "Sparse gradients only work with packed mode."
                    gaussian_ids = info["gaussian_ids"]
                    for k in self.splats.keys():
                        grad = self.splats[k].grad
                        if grad is None or grad.is_sparse:
                            continue
                        self.splats[k].grad = torch.sparse_coo_tensor(
                            indices=gaussian_ids[None],  # [1, nnz]
                            values=grad[gaussian_ids],  # [nnz, ...]
                            size=self.splats[k].size(),  # [N, ...]
                            is_coalesced=len(Ks) == 1,
                        )

                # logic for 'visible_adam'
                if cfg.visible_adam:
                    gaussian_cnt = self.splats.means.shape[0]
                    if cfg.packed:
                        visibility_mask = torch.zeros_like(
                            self.splats["opacities"], dtype=bool
                        )
                        visibility_mask.scatter_(0, info["gaussian_ids"], 1)
                    else:
                        visibility_mask = (info["radii"] > 0).any(0)

                # optimize
                for optimizer in self.optimizers.values():
                    if cfg.visible_adam:
                        optimizer.step(visibility_mask)
                    else:
                        optimizer.step()
                    optimizer.zero_grad(set_to_none=True)
                for optimizer in self.pose_optimizers:
                    optimizer.step()
                    optimizer.zero_grad(set_to_none=True)
                for optimizer in self.app_optimizers:
                    optimizer.step()
                    optimizer.zero_grad(set_to_none=True)
                for optimizer in self.bil_grid_optimizers:
                    optimizer.step()
                    optimizer.zero_grad(set_to_none=True)
                for scheduler in schedulers:
                    scheduler.step()
                # (optional) compression simulation optimizers
                self.compression_simulation.step_optimizers(step)

                # Run post-backward steps after backward and optimizer
                if isinstance(self.cfg.strategy, DefaultStrategy):
                    self.cfg.strategy.step_post_backward(
                        params=self.splats,
                        optimizers=self.optimizers,
                        state=self.strategy_state,
                        step=step,
                        info=info,
                        packed=cfg.packed,
                    )
                elif isinstance(self.cfg.strategy, MCMCStrategy):
                    self.cfg.strategy.step_post_backward(
                        params=self.splats,
                        optimizers=self.optimizers,
                        state=self.strategy_state,
                        step=step,
                        info=info,
                        lr=schedulers[0].get_last_lr()[0],
                    )
                else:
                    assert_never(self.cfg.strategy)

                self.step_profiler()

                # eval the full set
                if step in [i - 1 for i in cfg.eval_steps]:
                    vis_splats = self.comp_sim_result.splats if self.comp_sim_result is not None else {k: v for k, v in self.splats.items()}
                    self.run_param_distribution_vis(vis_splats, 
                                                    f"{cfg.result_dir}/visualization/comp_sim_step{step}")
                    self.eval(step)
                    self.render_traj(step)

                # run compression
                # if cfg.compression is not None and step in [i - 1 for i in cfg.eval_steps]:
                #     self.run_compression(step=step)

                # run post-training compression
                # if cfg.post_training_comp_cfg is not None and step in [i - 1 for i in cfg.eval_steps]:
                #     self.run_post_training_compression(step=step)

                if not cfg.disable_viewer:
                    self.viewer.lock.release()
                    num_train_steps_per_sec = 1.0 / (time.time() - tic)
                    num_train_rays_per_sec = (
                        num_train_rays_per_step * num_train_steps_per_sec
                    )
                    # Update the viewer state.
                    self.viewer.state.num_train_rays_per_sec = num_train_rays_per_sec
                    # Update the scene.
                    self.viewer.update(step, num_train_rays_per_step)
        

    @torch.no_grad()
    def eval(self, step: int, stage: str = "val"):
        """Entry for evaluation."""
        print("Running evaluation...")
        cfg = self.cfg
        device = self.device
        world_rank = self.world_rank
        world_size = self.world_size

        valloader = torch.utils.data.DataLoader(
            self.valset, batch_size=1, shuffle=False, num_workers=1
        )
        ellipse_time = 0
        metrics = defaultdict(list)
        for i, data in enumerate(valloader):
            camtoworlds = data["camtoworld"].to(device)
            Ks = data["K"].to(device)
            pixels = data["image"].to(device) / 255.0
            masks = data["mask"].to(device) if "mask" in data else None
            height, width = pixels.shape[1:3]

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
            )  # [1, H, W, 3]
            torch.cuda.synchronize()
            ellipse_time += time.time() - tic

            colors = torch.clamp(colors, 0.0, 1.0)
            canvas_list = [pixels, colors]

            if world_rank == 0:
                # write images 
                canvas = torch.cat(canvas_list, dim=2).squeeze(0).cpu().numpy() # side by side
                # canvas = canvas_list[1].squeeze(0).cpu().numpy() # signle image
                canvas = (canvas * 255).astype(np.uint8)
                imageio.imwrite(
                    f"{self.render_dir}/{stage}_step{step}_{i:04d}.png",
                    canvas,
                )

                pixels_p = pixels.permute(0, 3, 1, 2)  # [1, 3, H, W]
                colors_p = colors.permute(0, 3, 1, 2)  # [1, 3, H, W]
                metrics["psnr"].append(self.psnr(colors_p, pixels_p))
                metrics["ssim"].append(self.ssim(colors_p, pixels_p))
                metrics["lpips"].append(self.lpips(colors_p, pixels_p))
                if cfg.use_bilateral_grid:
                    cc_colors = color_correct(colors, pixels)
                    cc_colors_p = cc_colors.permute(0, 3, 1, 2)  # [1, 3, H, W]
                    metrics["cc_psnr"].append(self.psnr(cc_colors_p, pixels_p))

        if world_rank == 0:
            ellipse_time /= len(valloader)

            stats = {k: torch.stack(v).mean().item() for k, v in metrics.items()}
            stats.update(
                {
                    "ellipse_time": ellipse_time,
                    "num_GS": len(self.splats["means"]),
                }
            )
            print(
                f"PSNR: {stats['psnr']:.3f}, SSIM: {stats['ssim']:.4f}, LPIPS: {stats['lpips']:.3f} "
                f"Time: {stats['ellipse_time']:.3f}s/image "
                f"Number of GS: {stats['num_GS']}"
            )
            # save stats as json
            with open(f"{self.stats_dir}/{stage}_step{step:04d}.json", "w") as f:
                json.dump(stats, f)
            # save stats to tensorboard
            for k, v in stats.items():
                self.writer.add_scalar(f"{stage}/{k}", v, step)
            self.writer.flush()

    @torch.no_grad()
    def render_traj(self, step: int, stage: str = "val"):
        """Entry for trajectory rendering."""
        print("Running trajectory rendering...")
        cfg = self.cfg
        device = self.device

        num_imgs = len(self.parser.camtoworlds)

        camtoworlds_all = self.parser.camtoworlds[: num_imgs//2]
        if cfg.render_traj_path == "interp":
            camtoworlds_all = generate_interpolated_path(
                camtoworlds_all, 6 #1
            )  # [N, 3, 4]
        elif cfg.render_traj_path == "ellipse":
            height = camtoworlds_all[:, 2, 3].mean()
            camtoworlds_all = generate_ellipse_path_z(
                camtoworlds_all, height=height
            )  # [N, 3, 4]
        elif cfg.render_traj_path == "spiral":
            camtoworlds_all = generate_spiral_path(
                camtoworlds_all,
                bounds=self.parser.bounds * self.scene_scale,
                spiral_scale_r=self.parser.extconf["spiral_radius_scale"],
            )
        else:
            raise ValueError(
                f"Render trajectory type not supported: {cfg.render_traj_path}"
            )

        camtoworlds_all = np.concatenate(
            [
                camtoworlds_all,
                np.repeat(
                    np.array([[[0.0, 0.0, 0.0, 1.0]]]), len(camtoworlds_all), axis=0
                ),
            ],
            axis=1,
        )  # [N, 4, 4]

        camtoworlds_all = torch.from_numpy(camtoworlds_all).float().to(device)
        K = torch.from_numpy(list(self.parser.Ks_dict.values())[0]).float().to(device)
        width, height = list(self.parser.imsize_dict.values())[0]

        # save to video
        video_dir = f"{cfg.result_dir}/videos"
        os.makedirs(video_dir, exist_ok=True)
        writer = imageio.get_writer(f"{video_dir}/{stage}_traj_{step}.mp4", fps=30)
        for i in tqdm.trange(len(camtoworlds_all), desc="Rendering trajectory"):
            camtoworlds = camtoworlds_all[i : i + 1]
            Ks = K[None]

            renders, _, _ = self.rasterize_splats(
                camtoworlds=camtoworlds,
                Ks=Ks,
                width=width,
                height=height,
                sh_degree=cfg.sh_degree,
                near_plane=cfg.near_plane,
                far_plane=cfg.far_plane,
                render_mode="RGB+ED",
            )  # [1, H, W, 4]
            colors = torch.clamp(renders[..., 0:3], 0.0, 1.0)  # [1, H, W, 3]
            depths = renders[..., 3:4]  # [1, H, W, 1]
            depths = (depths - depths.min()) / (depths.max() - depths.min())
            canvas_list = [colors, depths.repeat(1, 1, 1, 3)]

            # write images
            # canvas = torch.cat(canvas_list, dim=2).squeeze(0).cpu().numpy()
            canvas = canvas_list[0].squeeze(0).cpu().numpy()
            canvas = (canvas * 255).astype(np.uint8)
            writer.append_data(canvas)
        writer.close()
        print(f"Video saved to {video_dir}/{stage}_traj_{step}.mp4")

    @torch.no_grad()
    def run_compression(self, step: int):
        """Entry for running compression."""
        print("Running compression...")
        world_rank = self.world_rank

        compress_dir = f"{cfg.result_dir}/compression/rank{world_rank}"

        if os.path.exists(compress_dir):
            shutil.rmtree(compress_dir)
        os.makedirs(compress_dir)

        self.run_param_distribution_vis(self.splats, save_dir=f"{cfg.result_dir}/visualization/raw")
        
        if isinstance(self.compression_method, PngCompression):
            self.compression_method.compress(compress_dir, self.splats)
        elif isinstance(self.compression_method, EntropyCodingCompression):
            self.compression_method.compress(compress_dir, self.splats, self.entropy_models)
        elif isinstance(self.compression_method, HevcCompression):
            self.compression_method.compress(compress_dir, self.splats)
        else:
            raise NotImplementedError(f"The compression method is not implemented yet.")

        # evaluate compression
        splats_c = self.compression_method.decompress(compress_dir)
        
        self.run_param_distribution_vis(splats_c, save_dir=f"{cfg.result_dir}/visualization/quant")

        for k in splats_c.keys():
            self.splats[k].data = splats_c[k].to(self.device)
        self.eval(step=step, stage="compress")
        self.render_traj(step=step, stage="compress")

    @torch.no_grad()
    def run_post_training_compression(self, step: int):
        """Use PostTrainingCompressor for post-training compression"""
        cfg = self.cfg
        
        # Check if post-training compression is enabled
        if not hasattr(self, 'pt_comp_config') or self.pt_comp_config is None:
            print("Post-training compression not configured, skipping...")
            return
            
        print("Running post-training compression...")
        
        # Use the configuration already adjusted during Runner initialization
        pt_cfg = copy.deepcopy(self.pt_comp_config)
        
        # Further dynamic adjustments (if needed)
        if pt_cfg.input_spec.input_type == "ckpt" and cfg.ckpt is not None:
            pt_cfg.input_spec.path = Path(cfg.ckpt[0])
            print(f"Using checkpoint as input: {pt_cfg.input_spec.path}")
        else:
            # If not loading from ckpt, save current splats as PLY
            ply_dir = f"{cfg.result_dir}/post_training_temp"
            os.makedirs(ply_dir, exist_ok=True)
            ply_path = f"{ply_dir}/splats_step{step}_rank{self.world_rank}.ply"
            save_ply(self.splats, ply_path)
            print(f"Saved splats to {ply_path}")
            
            # Update input_spec to point to the newly saved PLY file
            pt_cfg.input_spec = InputSpec(
                input_type="ply",
                path=Path(ply_path)
            )
        
        # Create compression output directory
        compress_dir = f"{cfg.result_dir}/post_training_compression/step{step}_rank{self.world_rank}"
        
        # Instantiate PostTrainingCompressor
        compressor = PostTrainingCompressor(
            config=pt_cfg,
            output_dir=Path(compress_dir)
        )
        
        # Run encoding
        print("Encoding splats with PostTrainingCompressor...")
        encode_result = compressor.encode()
        print(f"Encode completed. Output: {encode_result.payload_dir}")
        
        # Run decoding
        print("Decoding splats...")
        decode_result = compressor.decode()
        print(f"Decoded frame saved to: {decode_result.saved_path}")
        
        # Load decoded splats and update to self.splats
        splats_decoded = decode_result.frame
        for k in splats_decoded.keys():
            self.splats[k].data = splats_decoded[k].to(self.device)
        
        print("Updated runner splats with decoded results.")
        
        # Evaluate decoded results
        self.eval(step=step, stage="post_training_compress")
        self.render_traj(step=step, stage="post_training_compress")
        
        print("Post-training compression completed.")

    @torch.no_grad()
    def run_param_distribution_vis(self, param_dict: Dict[str, Tensor], save_dir: str):
        import matplotlib.pyplot as plt
        import matplotlib.ticker as ticker
        from matplotlib.colors import LinearSegmentedColormap

        def plot_distribution(value, param_name, save_dir):
            tensor_np = value.flatten().detach().cpu().numpy()
            min_val, max_val = tensor_np.min(), tensor_np.max()
            
            nice_blue = '#4878CF'  # Brighter blue
            
            plt.figure(figsize=(6, 4.5), dpi=100)
            
            # Use more bins for a smoother histogram
            n, bins, patches = plt.hist(tensor_np, bins=50, density=False, alpha=0.85, 
                                    color=nice_blue, edgecolor='none')
            
            # Add grid lines but place them behind the chart
            plt.grid(alpha=0.3, linestyle='--', axis='y')
            plt.gca().set_axisbelow(True)
            
            # Use scientific notation for y-axis ticks
            plt.gca().yaxis.set_major_formatter(ticker.ScalarFormatter(useMathText=True))
            plt.gca().ticklabel_format(style='sci', axis='y', scilimits=(0,0))
            
            # Improved annotations for minimum and maximum values, smaller size
            plt.annotate(f'Min: {min_val:.2f}', xy=(min_val, 0), xytext=(min_val, max(n) * 0.1),
                        arrowprops=dict(facecolor='green', width=1.5, headwidth=6, headlength=6, shrink=0.05), 
                        fontsize=8, color='darkgreen', weight='bold',
                        bbox=dict(boxstyle="round,pad=0.1", fc="white", ec="green", alpha=0.7))

            plt.annotate(f'Max: {max_val:.2f}', xy=(max_val, 0), xytext=(max_val, max(n) * 0.1),
                        arrowprops=dict(facecolor='red', width=1.5, headwidth=6, headlength=6, shrink=0.05), 
                        fontsize=8, color='darkred', weight='bold',
                        bbox=dict(boxstyle="round,pad=0.1", fc="white", ec="red", alpha=0.7))
            
            # Beautify title and labels
            plt.title(f'{param_name} Distribution')
            plt.xlabel('Value')
            plt.ylabel('Frequency')
            
            # Adjust x and y axis ranges to leave enough space for annotations
            plt.xlim(min_val - (max_val - min_val) * 0.05, max_val + (max_val - min_val) * 0.05)
            plt.ylim(0, max(n) * 1.2)
            
            plt.tight_layout()
            plt.savefig(os.path.join(save_dir, f'{param_name}.png'), dpi=120, bbox_inches='tight')
            plt.close()

        os.makedirs(save_dir, exist_ok=True)
        for param_name, value in param_dict.items():
            plot_distribution(value, param_name, save_dir)
        
        print(f"Histograms saved in '{save_dir}' directory.")
    
    def load_entropy_model_from_ckpt(self, ckpt: Dict, entropy_model_type: str):
        self.entropy_models = {}
        for name, value in ckpt.items():
            if "_entropy_model" in name:
                attr_name = name[:(len(name) - len("_entropy_model"))]
                num_ch = ckpt["splats"][attr_name].shape[-1]
                if entropy_model_type == "factorized_model":
                    # TODO
                    if attr_name == "scales" or attr_name == "sh0":
                        filters = (3, 3)
                    else:
                        filters = (3, 3, 3)
                    entropy_model = Entropy_factorized_optimized_refactor(channel=num_ch, filters=filters)

                elif entropy_model_type == "gaussian_model":
                    entropy_model = Entropy_gaussian(channel=num_ch)
                
                entropy_model.load_state_dict(value)
                self.entropy_models[attr_name] = entropy_model

    @torch.no_grad()
    def _viewer_render_fn(
        self, camera_state: nerfview.CameraState, img_wh: Tuple[int, int]
    ):
        """Callable function for the viewer."""
        W, H = img_wh
        c2w = camera_state.c2w
        K = camera_state.get_K(img_wh)
        c2w = torch.from_numpy(c2w).float().to(self.device)
        K = torch.from_numpy(K).float().to(self.device)

        render_colors, _, _ = self.rasterize_splats(
            camtoworlds=c2w[None],
            Ks=K[None],
            width=W,
            height=H,
            sh_degree=self.cfg.sh_degree,  # active all SH degrees
            radius_clip=3.0,  # skip GSs that have small image radius (in pixels)
        )  # [1, H, W, 3]
        return render_colors[0].cpu().numpy()
    
    @torch.no_grad()
    def save_params_into_ply_file(
        self
    ):
        """Save parameters of Gaussian Splats into .ply file"""
        ply_dir = f"{self.cfg.result_dir}/ply"
        os.makedirs(ply_dir, exist_ok=True)
        ply_file = ply_dir + "/splats.ply"
        save_ply(self.splats, ply_file)
        print(f"Saved parameters of splats into file: {ply_file}.")


def main(local_rank: int, world_rank, world_size: int, cfg: Config):
    if world_size > 1 and not cfg.disable_viewer:
        cfg.disable_viewer = True
        if world_rank == 0:
            print("Viewer is disabled in distributed training.")

    runner = Runner(local_rank, world_rank, world_size, cfg)

    if cfg.mode == "train":
        runner.train()
    elif cfg.mode == "compress":
        if cfg.ckpt is None:
            raise ValueError("ckpt parameter is required for compress mode")
        # Load checkpoints
        ckpts = [
            torch.load(file, map_location=runner.device, weights_only=True)
            for file in cfg.ckpt
        ]
        for k in runner.splats.keys():
            runner.splats[k].data = torch.cat([ckpt["splats"][k] for ckpt in ckpts])
        step = ckpts[0]["step"]
        # save ply
        runner.save_params_into_ply_file()
        # eval and render traj. of uncompressed splats
        # runner.eval(step=step)
        # runner.render_traj(step=step)

        runner.run_post_training_compression(step=step)
    else:
        raise ValueError(f"Unknown mode: {cfg.mode}")

    if not cfg.disable_viewer:
        print("Viewer running... Ctrl+C to exit.")
        time.sleep(1000000)


if __name__ == "__main__":
    """
    Usage:

    ```bash
    # Training mode (default)
    CUDA_VISIBLE_DEVICES=0 python simple_trainer.py default
    CUDA_VISIBLE_DEVICES=0 python simple_trainer.py default --mode train

    # Compression mode (requires checkpoint and post-training compression config)
    CUDA_VISIBLE_DEVICES=0 python simple_trainer.py default --mode compress --ckpt results/garden/ckpts/ckpt_30000_rank0.pt

    # Distributed training on 4 GPUs: Effectively 4x batch size so run 4x less steps.
    CUDA_VISIBLE_DEVICES=0,1,2,3 python simple_trainer.py default --steps_scaler 0.25

    """

    # Config objects we can choose between.
    # Each is a tuple of (CLI description, config object).
    configs = {
        "default": (
            "Gaussian splatting training using densification heuristics from the original paper.",
            Config(
                strategy=DefaultStrategy(verbose=True),
            ),
        ),
        "mcmc": (
            "Gaussian splatting training using densification from the paper '3D Gaussian Splatting as Markov Chain Monte Carlo'.",
            Config(
                init_opa=0.5,
                init_scale=0.1,
                opacity_reg=0.01,
                scale_reg=0.01,
                strategy=MCMCStrategy(verbose=True),
            ),
        ),
    }
    config_file_arg = pop_flag_value(CONFIG_FILE_FLAGS)
    save_config_arg = pop_flag_value(CONFIG_SAVE_FLAGS)

    external_updates = None
    if config_file_arg is not None:
        external_updates = load_config_updates(Path(config_file_arg))
    prepared_configs = prepare_presets(
        configs,
        external_updates,
        registry_handlers=_REGISTRY_HANDLERS,
    )

    cfg = tyro.extras.overridable_config_cli(prepared_configs)
    # synchronize_compression_config(cfg)  # Removed since CompressionConfig is no longer used
    cfg.adjust_steps(cfg.steps_scaler)

    snapshot_path = (
        Path(save_config_arg)
        if save_config_arg is not None
        else Path(cfg.result_dir) / "cfg_snapshot" / DEFAULT_CONFIG_SNAPSHOT
    )
    save_config_snapshot(
        cfg,
        snapshot_path,
        custom_serializers=_CONFIG_SERIALIZERS,
    )
    print(f"[config] snapshot saved to {snapshot_path}")

    # try import extra dependencies
    # if cfg.compression == "png":
    #     try:
    #         import plas
    #         import torchpq
    #     except:
    #         raise ImportError(
    #             "To use PNG compression, you need to install "
    #             "torchpq (instruction at https://github.com/DeMoriarty/TorchPQ?tab=readme-ov-file#install) "
    #             "and plas (via 'pip install git+https://github.com/fraunhoferhhi/PLAS.git') "
    #         )

    cli(main, cfg, verbose=True)
