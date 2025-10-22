from __future__ import annotations

from dataclasses import dataclass, field
import logging
import warnings
import math
from typing import Any, Dict, List, Tuple

import torch

from gsplat.compression.outlier_filter import filter_splats
from gsplat.compression.sort import sort_splats, sort_splats_morton
from .configs import MappingConfig, PruningConfig, QuantConfig, PruningHook, QuantFieldConfig

# Type alias for tensor dictionaries
TensorDict = Dict[str, torch.Tensor]


@dataclass
class QuantFieldStats:
    """Statistics required to dequantize a single tensor field."""

    bitwidth: int
    min_vals: list[float] = field(default_factory=list)
    max_vals: list[float] = field(default_factory=list)
    clamp_min: float | None = None
    clamp_max: float | None = None
    channels: int = 0
    method: str = "scalar"
    codebook: list[list[float]] | None = None
    tensor_shape: list[int] | None = None
    original_shape: list[int] | None = None
    mask: list[int] | None = None
    codebook_bits: int | None = None
    codebook_min_vals: list[float] | None = None
    codebook_max_vals: list[float] | None = None
    codebook_shape: list[int] | None = None

    def to_dict(self, mode: str = "full") -> Dict[str, Any]:
        """Serialize to dictionary.

        Args:
            mode: "full" for complete info (decoding), "summary" for compression_info
        """
        if mode == "summary":
            result = {
                "bitwidth": self.bitwidth,
                "method": self.method,
                "channels": self.channels,
            }
            if self.method == "vector" and self.codebook_shape:
                result["codebook_size"] = self.codebook_shape[0]
            elif self.method == "scalar" and self.min_vals and self.max_vals:
                result["range"] = [[float(mn), float(mx)] for mn, mx in zip(self.min_vals, self.max_vals)]
            return result

        result = {
            "bitwidth": self.bitwidth,
            "method": self.method,
            "tensor_shape": self.tensor_shape,
            "original_shape": self.original_shape,
            "clamp_min": self.clamp_min,
            "clamp_max": self.clamp_max,
            "channels": self.channels,
            "min_vals": self.min_vals,
            "max_vals": self.max_vals,
        }
        if self.codebook_bits is not None:
            result["codebook_bits"] = self.codebook_bits
        if self.codebook_min_vals is not None:
            result["codebook_min_vals"] = self.codebook_min_vals
        if self.codebook_max_vals is not None:
            result["codebook_max_vals"] = self.codebook_max_vals
        if self.codebook_shape is not None:
            result["codebook_shape"] = self.codebook_shape
        if self.mask is not None:
            result["mask_present"] = True
            result["mask_len"] = len(self.mask)
        return result


@dataclass
class QuantizationContext:
    """Metadata storing quantization statistics for every field."""

    field_stats: Dict[str, QuantFieldStats] = field(default_factory=dict)
    int_values: Dict[str, torch.Tensor] = field(default_factory=dict)
    field_configs: Dict[str, QuantFieldConfig] = field(default_factory=dict)
    codebooks: Dict[str, torch.Tensor] = field(default_factory=dict)

    def register(
        self,
        name: str,
        stats: QuantFieldStats,
        int_tensor: torch.Tensor | None = None,
        config: QuantFieldConfig | None = None,
        codebook_tensor: torch.Tensor | None = None,
    ) -> None:
        self.field_stats[name] = stats
        if int_tensor is not None:
            self.int_values[name] = int_tensor
        if config is not None:
            self.field_configs[name] = config
        if codebook_tensor is not None:
            self.codebooks[name] = codebook_tensor

    def to_summary(self) -> Dict[str, Any]:
        """Create lightweight summary for compression_info.json."""
        return {
            "fields": {
                name: stats.to_dict(mode="summary")
                for name, stats in self.field_stats.items()
            }
        }


logger = logging.getLogger(__name__)


@dataclass
class PruningStepRecord:
    """Record describing a single pruning step."""

    name: str
    removed: int
    kept: int
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class PruningContext:
    """Metadata describing the result of the pruning stage."""

    enabled: bool
    kept: int
    removed: int
    opacity_threshold: float | None = None
    max_points: int | None = None
    steps: List[PruningStepRecord] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        """Serialize to dictionary for JSON storage."""
        result = {
            "enabled": self.enabled,
            "kept": self.kept,
            "removed": self.removed,
        }
        if self.enabled:
            result["compression_ratio"] = self.kept / (self.kept + self.removed) if (self.kept + self.removed) > 0 else 1.0
            if self.opacity_threshold is not None:
                result["opacity_threshold"] = self.opacity_threshold
            if self.max_points is not None:
                result["max_points"] = self.max_points
            if self.steps:
                result["steps"] = [
                    {
                        "name": step.name,
                        "removed": step.removed,
                        "kept": step.kept,
                        "metadata": step.metadata,
                    }
                    for step in self.steps
                ]
        return result


# Quantization-related types were moved to components/quant/types.py
# Keep QuantizationContext import above for backward compatibility of imports

@dataclass
class MappingContext:
    """Permutation metadata produced by the mapping stage."""

    strategy: str
    permutation: torch.Tensor | None = None

    def to_dict(self, include_permutation: bool = False) -> Dict[str, Any]:
        """Serialize to dictionary.
        
        Args:
            include_permutation: If True, save full permutation (large).
        """
        result = {"strategy": self.strategy}
        if self.permutation is not None:
            result["num_points"] = len(self.permutation)
            # Infer square shape (for morton/plas mapping)
            import math
            sidelen = int(math.isqrt(len(self.permutation)))
            if sidelen * sidelen == len(self.permutation):
                result["final_shape"] = [sidelen, sidelen]
            if include_permutation:
                result["permutation"] = self.permutation.cpu().tolist()
        return result


def clone_splats(splats: TensorDict) -> TensorDict:
    return {k: v.clone() for k, v in splats.items()}






