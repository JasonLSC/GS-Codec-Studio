from __future__ import annotations

from typing import Tuple

import math
import torch

from ...configs import QuantFieldConfig
from .types import QuantFieldStats
from .registry import register_quantizer


def quantize_tensor(
    tensor: torch.Tensor, config: QuantFieldConfig
) -> Tuple[torch.Tensor, QuantFieldStats, torch.Tensor | None, torch.Tensor | None]:
    working = tensor.clone().float()
    flat = working.reshape(working.shape[0], -1)
    if flat.numel() == 0:
        empty_labels = torch.empty((flat.shape[0], 1), dtype=torch.int32, device=tensor.device)
        stats = QuantFieldStats(
            bitwidth=config.bitwidth,
            channels=flat.shape[1],
            method="vector",
            tensor_shape=list(empty_labels.shape),
            original_shape=list(tensor.shape),
            codebook=[],
        )
        return working.to(tensor.dtype), stats, empty_labels, None

    try:
        from torchpq.clustering import KMeans
    except ImportError as exc:
        raise ImportError(
            "Vector quantization requires torchpq. Install with `pip install torchpq`."
        ) from exc

    num_points = flat.shape[0]
    feature_dim = flat.shape[1]
    max_clusters = num_points
    if config.vector_clusters is not None:
        max_clusters = min(max_clusters, max(1, config.vector_clusters))
    if config.vector_bits is not None and config.vector_bits >= 0:
        max_clusters = min(max_clusters, 1 << config.vector_bits)
    if max_clusters <= 1:
        # fall back to scalar handled by caller; keep behavior consistent
        raise ValueError("Vector quantization requires at least 2 clusters; adjust config.")

    mask = flat.abs().sum(dim=1) > 0
    valid_count = int(mask.sum().item())
    if valid_count == 0:
        int_tensor = torch.zeros((num_points, 1), dtype=torch.int32, device=tensor.device)
        stats = QuantFieldStats(
            bitwidth=config.bitwidth,
            channels=feature_dim,
            method="vector",
            codebook=[],
            tensor_shape=list(int_tensor.shape),
            original_shape=list(tensor.shape),
            mask=mask.to(torch.int32).cpu().tolist(),
        )
        return working.to(tensor.dtype), stats, int_tensor, None

    clusters = min(max_clusters, valid_count)
    if clusters <= 1:
        raise ValueError("Vector quantization resulted in a single cluster; adjust config or data.")

    valid_flat = flat[mask]
    data = valid_flat.detach().to(torch.float32).permute(1, 0).contiguous()
    kmeans = KMeans(n_clusters=clusters, distance="manhattan", verbose=True)
    labels_valid = kmeans.fit(data).to(torch.int64)
    centroids = kmeans.centroids.permute(1, 0).to(tensor.device, tensor.dtype)

    actual_clusters = centroids.shape[0]
    if actual_clusters <= 1:
        raise ValueError("Vector quantization produced a single centroid.")

    labels_full = torch.zeros(num_points, dtype=torch.int64, device=tensor.device)
    labels_full[mask] = labels_valid.to(tensor.device)

    reconstructed_flat = torch.zeros_like(flat)
    reconstructed_flat[mask] = centroids[labels_full[mask]]
    reconstructed_flat[~mask] = flat[~mask]
    reconstructed = reconstructed_flat.reshape_as(working).to(tensor.dtype)

    required_bitwidth = max(config.bitwidth, math.ceil(math.log2(actual_clusters)))
    int_tensor = labels_full.to(torch.int32).view(-1, 1)

    quant_bits = 8
    quant_levels = (1 << quant_bits) - 1
    cb_min = centroids.min(dim=0).values
    cb_max = centroids.max(dim=0).values
    cb_range = cb_max - cb_min
    cb_range = torch.where(cb_range < 1e-8, torch.ones_like(cb_range), cb_range)
    norm_centroids = (centroids - cb_min) / cb_range
    quantized_codebook = torch.clamp(torch.round(norm_centroids * quant_levels), 0, quant_levels).to(torch.uint8)

    stats = QuantFieldStats(
        bitwidth=required_bitwidth,
        channels=feature_dim,
        method="vector",
        codebook=centroids.cpu().tolist(),
        tensor_shape=list(int_tensor.shape),
        original_shape=list(tensor.shape),
        mask=mask.to(torch.int32).cpu().tolist(),
        codebook_bits=quant_bits,
        codebook_min_vals=cb_min.cpu().tolist(),
        codebook_max_vals=cb_max.cpu().tolist(),
        codebook_shape=list(centroids.shape),
    )
    return reconstructed, stats, int_tensor, quantized_codebook


def dequantize_tensor(tensor: torch.Tensor, stats: QuantFieldStats) -> torch.Tensor:
    if stats.codebook is None:
        raise ValueError("Vector quantization stats missing codebook.")
    labels = tensor.reshape(-1).to(torch.long)
    codebook = torch.tensor(stats.codebook, device=tensor.device, dtype=tensor.dtype)
    if codebook.shape[0] == 0:
        target_shape = tuple(stats.original_shape) if stats.original_shape is not None else tensor.shape
        return torch.zeros(target_shape, dtype=tensor.dtype, device=tensor.device)
    if stats.mask is not None:
        mask_values = torch.tensor(stats.mask, device=tensor.device, dtype=torch.bool)
    else:
        mask_values = torch.ones(labels.shape[0], dtype=torch.bool, device=tensor.device)
    output_flat = torch.zeros(labels.shape[0], codebook.shape[1], device=tensor.device, dtype=codebook.dtype)
    if mask_values.any():
        valid_labels = torch.clamp(labels[mask_values], 0, codebook.shape[0] - 1)
        output_flat[mask_values] = codebook[valid_labels]
    target_shape = tuple(stats.original_shape) if stats.original_shape is not None else (tensor.shape[0], stats.channels)
    return output_flat.reshape(target_shape).to(tensor.dtype)


@register_quantizer("vector")
class VectorQuantizer:
    def quantize_field(
        self, tensor: torch.Tensor, config: QuantFieldConfig
    ) -> Tuple[torch.Tensor, QuantFieldStats, torch.Tensor | None, torch.Tensor | None]:
        return quantize_tensor(tensor, config)

    def dequantize_field(self, tensor: torch.Tensor, stats: QuantFieldStats) -> torch.Tensor:
        return dequantize_tensor(tensor, stats)


