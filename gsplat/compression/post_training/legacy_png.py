from __future__ import annotations

import json
import logging
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Any, Tuple

import numpy as np
import torch
import imageio.v2 as imageio
from sklearn.cluster import KMeans

from .codec import CodecArtifacts, register_codec
from .configs import CodecConfig
from .stages import QuantizationContext
from .io import TensorDict


@dataclass
class LegacyPNGCodecConfig:
    shN_clusters: int = 16384


logger = logging.getLogger(__name__)


class LegacyPNGCodec:
    def __init__(self, config: CodecConfig) -> None:
        params = config.params or {}
        self.config = LegacyPNGCodecConfig(**params)

    def encode(self, frame_dir: Path, splats: TensorDict, quant_ctx: QuantizationContext) -> CodecArtifacts:
        frame_dir.mkdir(parents=True, exist_ok=True)
        files: Dict[str, Dict[str, Any]] = {}

        for name, tensor in splats.items():
            stats = quant_ctx.field_stats.get(name)
            if stats is None:
                raise KeyError(f"Quantization stats missing for field '{name}'")

            field_cfg = quant_ctx.field_configs.get(name)
            logger.info("Legacy PNG codec encoding field '%s'; config=%s, stats=%s", name, field_cfg, stats)
            if getattr(stats, "method", "scalar") != "scalar":
                raise NotImplementedError(f"Legacy PNG codec does not support vector quantization for '{name}'.")
            if name == "shN":
                artifacts = self._encode_shN(frame_dir, tensor, stats)
                files[name] = artifacts
                continue

            int_tensor = quant_ctx.int_values.get(name)
            if int_tensor is None:
                raise ValueError(f"Field '{name}' requires integer cache; enable store_as_int in QuantConfig.")

            n_points = tensor.shape[0]
            sidelen = int(math.isqrt(n_points))
            if sidelen * sidelen != n_points:
                raise ValueError(f"PNG codec requires perfect-square length after mapping, got {n_points} for '{name}'.")

            flattened = int_tensor.reshape(n_points, -1)
            channels = flattened.shape[1]
            levels = (1 << stats.bitwidth) - 1
            clamped = torch.clamp(flattened, 0, levels)

            if name == "means":
                array16 = clamped.detach().cpu().numpy().astype(np.uint16)
                image16 = array16.reshape(sidelen, sidelen, channels)
                low = (image16 & 0xFF).astype(np.uint8)
                high = (image16 >> 8).astype(np.uint8)
                low_path = frame_dir / f"{name}_l.png"
                high_path = frame_dir / f"{name}_h.png"
                imageio.imwrite(low_path, low)
                imageio.imwrite(high_path, high)
                files[name] = {
                    "type": "png_split16",
                    "files": [low_path.name, high_path.name],
                    "shape": list(tensor.shape),
                    "sidelen": sidelen,
                    "channels": channels,
                    "bitwidth": stats.bitwidth,
                }
                continue

            dtype = np.uint8 if stats.bitwidth <= 8 else np.uint16
            image = clamped.detach().cpu().numpy().astype(dtype).reshape(sidelen, sidelen, channels)

            fname = f"{name}.png"
            if channels == 1:
                imageio.imwrite(frame_dir / fname, image.reshape(sidelen, sidelen))
            else:
                imageio.imwrite(frame_dir / fname, image)

            files[name] = {
                "type": "png",
                "files": [fname],
                "shape": list(tensor.shape),
                "sidelen": sidelen,
                "channels": channels,
                "bitwidth": stats.bitwidth,
                "dtype": np.dtype(dtype).name,
            }

        return CodecArtifacts(name="legacy_png", files=files)

    def _encode_shN(self, frame_dir: Path, tensor: torch.Tensor, stats) -> Dict[str, Any]:
        data = tensor.detach().cpu().numpy()
        n_points = data.shape[0]
        sidelen = int(math.isqrt(n_points))
        if sidelen * sidelen != n_points:
            raise ValueError(f"PNG codec requires perfect-square length after mapping, got {n_points} for 'shN'.")

        vectors = data.reshape(n_points, -1)
        mask = ~np.all(vectors == 0, axis=1)
        nonzero_vectors = vectors[mask]

        if len(nonzero_vectors) == 0:
            codebook = np.zeros((1, vectors.shape[1]), dtype=np.float32)
            assignments = np.zeros(n_points, dtype=np.int32)
        else:
            k = min(self.config.shN_clusters, len(nonzero_vectors))
            kmeans = KMeans(n_clusters=k, n_init="auto")
            kmeans.fit(nonzero_vectors)
            codebook = kmeans.cluster_centers_.astype(np.float32)
            default_codebook = np.zeros((1, vectors.shape[1]), dtype=np.float32)
            codebook = np.concatenate([default_codebook, codebook], axis=0)

            assignments = np.zeros(n_points, dtype=np.int32)
            nonzero_indices = np.where(mask)[0]
            assigned_clusters = kmeans.predict(nonzero_vectors) + 1
            assignments[nonzero_indices] = assigned_clusters

        channels = 1
        image = assignments.reshape(sidelen, sidelen).astype(np.uint16)
        fname = "shN_idx.png"
        imageio.imwrite(frame_dir / fname, image)

        codebook_path = frame_dir / "shN_codebook.npz"
        np.savez_compressed(codebook_path, codebook=codebook, mask=mask.astype(np.uint8))

        return {
            "type": "png_kmeans",
            "files": [fname],
            "codebook": codebook_path.name,
            "shape": list(tensor.shape),
            "sidelen": sidelen,
            "channels": tensor.shape[1] * tensor.shape[2],
            "bitwidth": stats.bitwidth,
        }

    def decode(self, frame_dir: Path, artifacts: CodecArtifacts, quant_ctx: QuantizationContext) -> TensorDict:
        restored: TensorDict = {}

        for name, meta in artifacts.files.items():
            sidelen = int(meta["sidelen"])
            channels = int(meta["channels"])
            original_shape = list(meta.get("shape", []))
            files_list = meta.get("files", [])
            mtype = meta.get("type", "png")

            if mtype == "png_split16":
                low = imageio.imread(frame_dir / files_list[0])
                high = imageio.imread(frame_dir / files_list[1])
                if low.ndim == 2:
                    low = low[..., None]
                if high.ndim == 2:
                    high = high[..., None]
                combined = (high.astype(np.uint16) << 8) | low.astype(np.uint16)
                flat = combined.reshape(sidelen * sidelen, -1).astype(np.float32)
                restored[name] = torch.from_numpy(flat).reshape(original_shape)
                continue

            if mtype == "png":
                arr = imageio.imread(frame_dir / files_list[0])
                if arr.ndim == 2:
                    arr = arr[..., None]
                flat = arr.reshape(sidelen * sidelen, channels).astype(np.float32)
                restored[name] = torch.from_numpy(flat).reshape(original_shape)
                continue

            if mtype == "png_kmeans":
                idx_img = imageio.imread(frame_dir / files_list[0])
                assignments = idx_img.reshape(-1).astype(np.int32)
                codebook_path = frame_dir / meta["codebook"]
                data = np.load(codebook_path)
                codebook = data["codebook"]
                mask = data["mask"].astype(bool)

                vectors = np.zeros((sidelen * sidelen, codebook.shape[1]), dtype=np.float32)
                nonzero_indices = np.where(assignments > 0)[0]
                clusters = assignments[nonzero_indices] - 1
                vectors[nonzero_indices] = codebook[clusters + 1]

                restored[name] = torch.from_numpy(vectors).reshape(original_shape)
                continue

            raise ValueError(f"Unsupported codec file type '{meta.get('type')}' for field '{name}'")

        return restored


register_codec("legacy_png", LegacyPNGCodec)
