
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Any, List

import logging
import warnings
import math
import numpy as np
import torch
import imageio.v2 as imageio

from .configs import CodecConfig
from .stages import QuantizationContext
from .io import TensorDict


logger = logging.getLogger(__name__)


# Deprecation notice: concrete codecs are migrating to gsplat.compression.post_training.codecs
warnings.warn(
    "post_training.codec will only host the registry; concrete codecs are provided in post_training.codecs",
    DeprecationWarning,
    stacklevel=1,
)

# Ensure extended codecs are imported and registered (overriding registrations below)
try:
    from . import codecs as _codecs  # noqa: F401
except Exception:
    pass


CODEC_REGISTRY: Dict[str, type] = {}


def register_codec(name: str, cls: type) -> None:
    CODEC_REGISTRY[name] = cls


def build_codec(config: CodecConfig):
    name = config.name.lower()
    if name not in CODEC_REGISTRY:
        raise KeyError(f"Unsupported codec '{config.name}'. Available: {list(CODEC_REGISTRY)}")
    return CODEC_REGISTRY[name](config)


@dataclass
class CodecArtifacts:
    name: str
    files: Dict[str, Dict[str, Any]]

    def to_meta(self) -> Dict[str, Any]:
        return {"name": self.name, "files": self.files}

    @classmethod
    def from_meta(cls, meta: Dict[str, Any]) -> "CodecArtifacts":
        return cls(name=meta["name"], files=meta.get("files", {}))


class NPZDebugCodec:
    def __init__(self, config: CodecConfig) -> None:
        self.config = config

    def encode(self, frame_dir: Path, splats: TensorDict, quant_ctx: QuantizationContext) -> CodecArtifacts:
        frame_dir.mkdir(parents=True, exist_ok=True)
        files: Dict[str, Dict[str, Any]] = {}
        for name, tensor in splats.items():
            int_tensor = quant_ctx.int_values.get(name)
            if int_tensor is not None:
                array = int_tensor.detach().cpu().numpy()
                file_type = "npz_int"
            else:
                array = tensor.detach().cpu().numpy()
                file_type = "npz_float"
            field_cfg = quant_ctx.field_configs.get(name)
            logger.info("NPZ codec encoding field '%s'; config=%s, type=%s", name, field_cfg, file_type)
            out_path = frame_dir / f"{name}.npz"
            np.savez_compressed(out_path, data=array)
            files[name] = {
                "type": file_type,
                "file": out_path.name,
                "shape": list(tensor.shape),
                "dtype": str(array.dtype),
            }
        return CodecArtifacts(name="npz_debug", files=files)

    def decode(self, frame_dir: Path, artifacts: CodecArtifacts, quant_ctx: QuantizationContext) -> TensorDict:
        restored: TensorDict = {}
        for name, meta in artifacts.files.items():
            file_type = meta.get("type")
            if file_type not in {"npz_int", "npz_float"}:
                raise ValueError(f"NPZ codec received unsupported file type '{file_type}' for '{name}'.")
            data = np.load(frame_dir / meta["file"])["data"]
            restored[name] = torch.from_numpy(data.astype(np.float32))
        return restored


class PNGCodec:
    def __init__(self, config: CodecConfig) -> None:
        self.config = config

    def encode(self, frame_dir: Path, splats: TensorDict, quant_ctx: QuantizationContext) -> CodecArtifacts:
        frame_dir.mkdir(parents=True, exist_ok=True)
        files: Dict[str, Dict[str, Any]] = {}

        for name, tensor in splats.items():
            stats = quant_ctx.field_stats.get(name)
            if stats is None:
                raise KeyError(f"Quantization stats missing for field '{name}'")

            method = getattr(stats, "method", "scalar")
            codebook_size = stats.codebook_shape[0] if getattr(stats, "codebook_shape", None) else 0
            logger.info(
                "PNG codec encoding field '%s'; method=%s bitwidth=%d channels=%d codebook_size=%d",
                name,
                method,
                stats.bitwidth,
                stats.channels,
                codebook_size,
            )
            int_tensor = quant_ctx.int_values.get(name)
            if int_tensor is None:
                raise ValueError(
                    f"Field '{name}' requires integer cache; set store_as_int=True in QuantFieldConfig."
                )

            n_points = int_tensor.shape[0]
            sidelen = int(math.isqrt(n_points))
            if sidelen * sidelen != n_points:
                raise ValueError(f"PNG codec requires perfect-square length after mapping, got {n_points} for '{name}'.")

            flattened = int_tensor.reshape(n_points, -1)
            channels = flattened.shape[1]

            if stats.bitwidth > 16:
                raise ValueError(f"PNG codec only supports up to 16-bit data, got {stats.bitwidth} for '{name}'.")

            # Skip fields with 0 channels (empty fields)
            if channels == 0:
                logger.info(f"Skipping field '{name}' with 0 channels")
                continue

            levels = (1 << stats.bitwidth) - 1
            clamped = torch.clamp(flattened, 0, levels)
            array = clamped.detach().cpu().numpy()

            data_shape = list(stats.tensor_shape or tensor.shape)

            if name == "means" and method == "scalar":
                array16 = array.astype(np.uint16)
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
                    "shape": data_shape,
                    "original_shape": list(stats.original_shape or tensor.shape),
                    "sidelen": sidelen,
                    "channels": channels,
                    "bitwidth": stats.bitwidth,
                    "method": method,
                }
                continue

            dtype = np.uint8 if stats.bitwidth <= 8 else np.uint16
            image = array.astype(dtype).reshape(sidelen, sidelen, channels)

            if method == "scalar" and name == "shN":
                chunk_files: List[str] = []
                chunk_size = 3
                num_chunks = math.ceil(channels / chunk_size)
                for idx in range(num_chunks):
                    start = idx * chunk_size
                    end = min((idx + 1) * chunk_size, channels)
                    chunk = image[..., start:end]
                    if chunk.shape[2] < chunk_size:
                        pad_width = ((0, 0), (0, 0), (0, chunk_size - chunk.shape[2]))
                        chunk = np.pad(chunk, pad_width, mode="constant")
                    fname = f"{name}_{idx:03d}.png"
                    imageio.imwrite(frame_dir / fname, chunk)
                    chunk_files.append(fname)
                files[name] = {
                    "type": "png_group",
                    "files": chunk_files,
                    "shape": data_shape,
                    "original_shape": list(stats.original_shape or tensor.shape),
                    "sidelen": sidelen,
                    "channels": channels,
                    "channels_per_file": chunk_size,
                    "bitwidth": stats.bitwidth,
                    "dtype": np.dtype(dtype).name,
                    "method": method,
                }
                continue

            fname = f"{name}.png"
            if channels == 1:
                imageio.imwrite(frame_dir / fname, image.reshape(sidelen, sidelen))
            else:
                imageio.imwrite(frame_dir / fname, image)

            files[name] = {
                "type": "png",
                "files": [fname],
                "shape": data_shape,
                "original_shape": list(stats.original_shape or tensor.shape),
                "sidelen": sidelen,
                "channels": channels,
                "bitwidth": stats.bitwidth,
                "dtype": np.dtype(dtype).name,
                "method": method,
            }

        return CodecArtifacts(name=self.config.name, files=files)

    def decode(self, frame_dir: Path, artifacts: CodecArtifacts, quant_ctx: QuantizationContext) -> TensorDict:
        restored: TensorDict = {}

        for name, meta in artifacts.files.items():
            sidelen = int(meta["sidelen"])
            channels = int(meta["channels"])
            data_shape = list(meta.get("shape", []))
            original_shape = list(meta.get("original_shape", data_shape))
            files_list = meta.get("files", [])
            mtype = meta.get("type", "png")
            method = meta.get("method", "scalar")

            if mtype == "png_split16":
                if len(files_list) != 2:
                    raise ValueError(f"Expected two PNG files for '{name}', got {len(files_list)}")
                low = imageio.imread(frame_dir / files_list[0])
                high = imageio.imread(frame_dir / files_list[1])
                if low.ndim == 2:
                    low = low[..., None]
                if high.ndim == 2:
                    high = high[..., None]
                combined = (high.astype(np.uint16) << 8) | low.astype(np.uint16)
                flat = combined.reshape(sidelen * sidelen, -1).astype(np.float32)
                restored[name] = torch.from_numpy(flat).reshape(data_shape)
                continue

            if mtype == "png_group":
                arrays = []
                for fname in files_list:
                    arr = imageio.imread(frame_dir / fname)
                    if arr.ndim == 2:
                        arr = arr[..., None]
                    arrays.append(arr)
                image = np.concatenate(arrays, axis=2)
                image = image[..., :channels]
                flat = image.reshape(sidelen * sidelen, channels).astype(np.float32)
                restored[name] = torch.from_numpy(flat).reshape(data_shape)
                continue

            if mtype == "png":
                if not files_list:
                    raise ValueError(f"No PNG files recorded for '{name}'")
                arr = imageio.imread(frame_dir / files_list[0])
                if arr.ndim == 2:
                    arr = arr[..., None]
                flat = arr.reshape(sidelen * sidelen, channels).astype(np.float32)
                restored[name] = torch.from_numpy(flat).reshape(data_shape)
                continue

            raise ValueError(f"Unsupported codec file type '{meta.get('type')}' for field '{name}'")

        return restored



register_codec("png", PNGCodec)


register_codec("npz_debug", NPZDebugCodec)
