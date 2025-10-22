from __future__ import annotations

from typing import Tuple

import torch

from ...configs import QuantFieldConfig
from .types import QuantFieldStats
from .registry import register_quantizer


def quantize_tensor(
    tensor: torch.Tensor, config: QuantFieldConfig
) -> Tuple[torch.Tensor, QuantFieldStats, torch.Tensor | None, torch.Tensor | None]:
    working = tensor.clone().float()
    if config.clamp_min is not None or config.clamp_max is not None:
        lower = config.clamp_min if config.clamp_min is not None else float("-inf")
        upper = config.clamp_max if config.clamp_max is not None else float("inf")
        working = torch.clamp(working, min=lower, max=upper)
    flat = working.reshape(working.shape[0], -1)
    min_vals = flat.min(dim=0).values
    max_vals = flat.max(dim=0).values
    scale = max_vals - min_vals
    zero_mask = scale < 1e-8
    scale = torch.where(zero_mask, torch.ones_like(scale), scale)
    normalized = (flat - min_vals) / scale
    normalized = torch.clamp(normalized, 0.0, 1.0)
    levels = config.levels()
    ints = torch.round(normalized * levels)
    ints = torch.clamp(ints, 0, levels)
    reconstructed_flat = ints / levels * scale + min_vals
    reconstructed_flat = torch.where(zero_mask, min_vals, reconstructed_flat)
    restored = reconstructed_flat.reshape_as(working).to(tensor.dtype)
    int_tensor = None
    if config.store_as_int:
        int_tensor = ints.reshape_as(working).to(torch.int32)
    stats = QuantFieldStats(
        bitwidth=config.bitwidth,
        min_vals=min_vals.tolist(),
        max_vals=max_vals.tolist(),
        clamp_min=config.clamp_min,
        clamp_max=config.clamp_max,
        channels=flat.shape[1],
        method="scalar",
        tensor_shape=list(tensor.shape),
        original_shape=list(tensor.shape),
    )
    return restored, stats, int_tensor, None


def dequantize_tensor(
    tensor: torch.Tensor, stats: QuantFieldStats
) -> torch.Tensor:
    # Reconstruct floating tensor from stored integer-like tensor, per-channel ranges
    working = tensor.float()
    flat = working.reshape(working.shape[0], -1)
    min_vals = torch.tensor(stats.min_vals, device=flat.device, dtype=flat.dtype)
    max_vals = torch.tensor(stats.max_vals, device=flat.device, dtype=flat.dtype)
    scale = max_vals - min_vals
    zero_mask = scale < 1e-8
    scale = torch.where(zero_mask, torch.ones_like(scale), scale)
    levels = (1 << stats.bitwidth) - 1
    normalized = torch.clamp(flat, 0.0, levels) / levels
    restored_flat = normalized * scale + min_vals
    restored_flat = torch.where(zero_mask, min_vals, restored_flat)
    return restored_flat.reshape_as(working).to(tensor.dtype)


@register_quantizer("scalar")
class ScalarQuantizer:
    def quantize_field(
        self, tensor: torch.Tensor, config: QuantFieldConfig
    ) -> Tuple[torch.Tensor, QuantFieldStats, torch.Tensor | None, torch.Tensor | None]:
        return quantize_tensor(tensor, config)

    def dequantize_field(self, tensor: torch.Tensor, stats: QuantFieldStats) -> torch.Tensor:
        return dequantize_tensor(tensor, stats)


