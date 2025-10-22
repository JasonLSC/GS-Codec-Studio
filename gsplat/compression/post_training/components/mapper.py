from __future__ import annotations

from typing import Tuple
import math
import torch

from ..configs import MappingConfig
from ..io import TensorDict
from ..stages import MappingContext, QuantizationContext
from gsplat.compression.sort import sort_splats, sort_splats_morton


class Mapper:
    def __init__(self, config: MappingConfig) -> None:
        self.config = config

    def _ensure_square_length(self, splats: TensorDict, quant_ctx: QuantizationContext) -> TensorDict:
        if not self.config.enabled or self.config.strategy == "none" or not self.config.ensure_square:
            return splats
        if not splats:
            return splats

        first_tensor = next(iter(splats.values()))
        n_points = first_tensor.shape[0]
        side = int(math.isqrt(n_points))
        target = side * side
        if target == n_points:
            return splats
        if target == 0:
            raise ValueError("Cannot map empty splats; need at least 1 point.")
        if "opacities" not in splats:
            raise KeyError("Square trimming requires 'opacities' field to compute importance scores.")

        opacities = splats["opacities"]
        flat = opacities.reshape(n_points, -1)
        if self.config.trim_score == "opacity_sum":
            scores = flat.sum(dim=1)
        else:
            scores = flat.mean(dim=1)
        keep = torch.topk(scores, target, largest=True).indices.sort()[0]

        trimmed: TensorDict = {name: tensor.index_select(0, keep) for name, tensor in splats.items()}

        # Sync quantization context shapes and cached int values
        for name, tensor in quant_ctx.int_values.items():
            quant_ctx.int_values[name] = tensor.index_select(0, keep)
        keep_cpu = keep.cpu()
        for name, stats in quant_ctx.field_stats.items():
            if stats.tensor_shape:
                shape = list(stats.tensor_shape)
                shape[0] = target
                stats.tensor_shape = shape
            if stats.original_shape:
                orig = list(stats.original_shape)
                orig[0] = target
                stats.original_shape = orig
            if stats.mask is not None:
                mask_tensor = torch.tensor(stats.mask, dtype=torch.bool)
                stats.mask = mask_tensor.index_select(0, keep_cpu).to(torch.int32).tolist()

        return trimmed

    def map(self, splats: TensorDict, quant_ctx: QuantizationContext | None = None) -> Tuple[TensorDict, MappingContext]:
        if not self.config.enabled or self.config.strategy == "none":
            return {k: v.clone() for k, v in splats.items()}, MappingContext(strategy="none", permutation=None)

        working = {k: v.clone() for k, v in splats.items()}
        if quant_ctx is None:
            quant_ctx = QuantizationContext()

        working = self._ensure_square_length(working, quant_ctx)

        if self.config.strategy == "morton":
            mapped, perm = sort_splats_morton(working, verbose=self.config.verbose, return_indices=True)
            # apply permutation to cached integer tensors used by codecs
            for name, int_tensor in list(quant_ctx.int_values.items()):
                if int_tensor.shape[0] == perm.shape[0]:
                    quant_ctx.int_values[name] = int_tensor[perm]
            # also permute any per-point masks stored in stats (for VQ)
            perm_cpu = perm.cpu()
            for fname, stats in quant_ctx.field_stats.items():
                if stats.mask is not None and len(stats.mask) == perm_cpu.shape[0]:
                    mask_tensor = torch.tensor(stats.mask, dtype=torch.int8)
                    stats.mask = mask_tensor[perm_cpu].to(torch.int32).tolist()
            return mapped, MappingContext(strategy="morton", permutation=perm)
        if self.config.strategy == "plas":
            mapped, perm = sort_splats(
                working,
                verbose=self.config.verbose,
                return_indices=True,
                sort_with_shN=self.config.sort_with_shN,
            )
            # apply permutation to cached integer tensors used by codecs
            for name, int_tensor in list(quant_ctx.int_values.items()):
                if int_tensor.shape[0] == perm.shape[0]:
                    quant_ctx.int_values[name] = int_tensor[perm]
            # also permute any per-point masks stored in stats (for VQ)
            perm_cpu = perm.cpu()
            for fname, stats in quant_ctx.field_stats.items():
                if stats.mask is not None and len(stats.mask) == perm_cpu.shape[0]:
                    mask_tensor = torch.tensor(stats.mask, dtype=torch.int8)
                    stats.mask = mask_tensor[perm_cpu].to(torch.int32).tolist()
            return mapped, MappingContext(strategy="plas", permutation=perm)
        return working, MappingContext(strategy=self.config.strategy, permutation=None)
