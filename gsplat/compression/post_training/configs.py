from __future__ import annotations

from dataclasses import dataclass, field
import logging
from pathlib import Path
from typing import Any, Callable, Dict, Literal, Optional, Sequence, Union

import torch
logger = logging.getLogger(__name__)


@dataclass
class InputSpec:
    """Specification describing how to locate post-training splats."""

    input_type: Literal["ply", "ckpt"]
    path: Union[str, Path]

    def __post_init__(self) -> None:
        if self.input_type not in {"ply", "ckpt"}:
            raise ValueError(f"Unsupported input type: {self.input_type}")
        # Convert string to Path if needed
        if isinstance(self.path, str):
            self.path = Path(self.path)

    def __str__(self) -> str:
        return f"InputSpec(input_type={self.input_type}, path={self.path})"

    def to_dict(self) -> dict:
        """Convert to dictionary for YAML serialization."""
        return {
            "input_type": self.input_type,
            "path": str(self.path)
        }

    def __getstate__(self):
        """Support for pickle serialization."""
        return {
            "input_type": self.input_type,
            "path": str(self.path)
        }

    def __setstate__(self, state):
        """Support for pickle deserialization."""
        self.input_type = state["input_type"]
        self.path = Path(state["path"])


@dataclass
class PruningHook:
    """Callable wrapper used for custom pruning steps."""

    name: str
    fn: Callable[[Any], Any]
    description: Optional[str] = None


@dataclass
class PruningConfig:
    """Configuration for pruning stage."""

    enabled: bool = False
    use_outlier_filter: bool = True
    opacity_threshold: Optional[float] = None
    max_points: Optional[int] = None
    opacity_activation: Literal["sigmoid", "linear"] = "sigmoid"
    scale_keep_ratio: Optional[float] = None
    scale_metric: Literal["volume", "max"] = "volume"
    custom_hooks: Sequence[PruningHook] = field(default_factory=tuple)


@dataclass
class MappingConfig:
    """Configuration for reordering / mapping stage."""

    strategy: Literal["none", "morton", "plas"] = "morton"
    sort_with_shN: bool = False
    verbose: bool = True
    enabled: bool = True
    ensure_square: bool = True
    trim_score: Literal["opacity_mean", "opacity_sum"] = "opacity_mean"


@dataclass
class QuantFieldConfig:
    """Field-specific quantization parameters."""

    bitwidth: int = 8
    clamp_min: Optional[float] = None
    clamp_max: Optional[float] = None
    enabled: bool = True
    mode: Literal["uniform", "log_uniform"] = "uniform"
    symmetric: bool = False
    store_as_int: bool = True
    dtype: Optional[Union[str, torch.dtype]] = None
    method: Literal["scalar", "vector"] = "scalar"
    vector_clusters: Optional[int] = None
    vector_bits: Optional[int] = None
    vector_n_init: int = 10
    vector_max_iter: int = 300
    vector_random_state: Optional[int] = None
    vector_sample_size: int = 100000

    def __post_init__(self) -> None:
        # Convert string dtype to torch.dtype if needed
        if isinstance(self.dtype, str):
            if self.dtype.lower() == "float32":
                self.dtype = torch.float32
            elif self.dtype.lower() == "float16":
                self.dtype = torch.float16
            elif self.dtype.lower() == "int8":
                self.dtype = torch.int8
            elif self.dtype.lower() == "int16":
                self.dtype = torch.int16
            elif self.dtype.lower() == "int32":
                self.dtype = torch.int32
            elif self.dtype.lower() == "int64":
                self.dtype = torch.int64
            else:
                raise ValueError(f"Unsupported dtype string: {self.dtype}")

    def levels(self) -> int:
        if self.bitwidth <= 0:
            raise ValueError("bitwidth must be positive.")
        return (1 << self.bitwidth) - 1


@dataclass
class QuantConfig:
    """Configuration for quantization stage."""

    enabled: bool = True
    default: QuantFieldConfig = field(default_factory=QuantFieldConfig)
    fields: Dict[str, QuantFieldConfig] = field(default_factory=dict)

    def field_config(self, name: str) -> QuantFieldConfig:
        return self.fields.get(name, self.default)


@dataclass
class CodecConfig:
    """Configuration wrapper for codec instantiation."""

    name: Literal["png", "entropy", "hevc", "seq_hevc", "seq_yuv", "stg"] = "png"
    params: Dict[str, Any] = field(default_factory=dict)


@dataclass
class PreprocessConfig:
    """Configuration for preprocessing transforms (log transform, quat normalization, etc.)."""

    enabled: bool = True
    apply_means_log_transform: bool = True
    apply_quat_normalize: bool = True


@dataclass
class PTCompConfig:
    """Top-level configuration passed into the orchestrator."""

    input_spec: InputSpec = field(default_factory=lambda: InputSpec(
        input_type="ckpt",
        path="/tmp/placeholder.pt"
    ))
    codec: CodecConfig = field(default_factory=CodecConfig)
    preprocess: PreprocessConfig = field(default_factory=PreprocessConfig)
    pruning: PruningConfig = field(default_factory=PruningConfig)
    mapping: MappingConfig = field(default_factory=MappingConfig)
    quant: QuantConfig = field(default_factory=QuantConfig)

    def __post_init__(self):
        """Ensure nested objects are proper dataclass instances and validate config cross-dependencies."""
        # 转换quant.fields中的字典为QuantFieldConfig实例
        if hasattr(self.quant, 'fields') and isinstance(self.quant.fields, dict):
            converted_fields = {}
            for name, field_config in self.quant.fields.items():
                if isinstance(field_config, dict):
                    # 处理dtype字段的字符串转换
                    if 'dtype' in field_config and isinstance(field_config['dtype'], str):
                        # 在QuantFieldConfig的__post_init__中处理dtype转换
                        converted_fields[name] = QuantFieldConfig(**field_config)
                    else:
                        converted_fields[name] = QuantFieldConfig(**field_config)
                else:
                    converted_fields[name] = field_config
            self.quant.fields = converted_fields

        # Cross-validation: if codec is PNG and any field uses vector quantization,
        # ensure vq_storage policy is provided per field; otherwise warn about default.
        try:
            if isinstance(self.codec, CodecConfig) and str(self.codec.name).lower() == "png":
                vq_policy = {}
                if isinstance(self.codec.params, dict):
                    vq_policy = self.codec.params.get("vq_storage", {}) or {}
                for field_name, field_cfg in getattr(self.quant, "fields", {}).items():
                    method = getattr(field_cfg, "method", "scalar")
                    if method == "vector" and field_name not in vq_policy:
                        logger.warning(
                            "Field '%s' uses vector quantization, but no vq_storage policy is set. "
                            "Will use default 'masked_npz_both'. For best results, explicitly set: "
                            "codec.params.vq_storage.%s = 'masked_npz_both'",
                            field_name,
                            field_name,
                        )
        except Exception:
            # Do not block construction on validation issues; log-only.
            logger.exception("Config validation for PNG VQ storage policy failed.")
