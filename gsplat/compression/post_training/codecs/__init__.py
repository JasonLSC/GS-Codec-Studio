from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List

import numpy as np
import torch
import imageio.v2 as imageio

from ..codec import CodecArtifacts, register_codec
from ..configs import CodecConfig
from ..stages import QuantizationContext
from ..io import TensorDict
from ..codec import PNGCodec as BasePNGCodec  # reuse base implementation


@dataclass
class VQStoragePolicy:
    # per-field storage policy for vector-quantized attributes
    # values: "png_labels_npz_codebook" (default), "npz_both"
    mapping: Dict[str, str]

    @staticmethod
    def from_params(params: Dict[str, Any] | None) -> "VQStoragePolicy":
        mapping: Dict[str, str] = {}
        if params and isinstance(params.get("vq_storage"), dict):
            mapping = dict(params["vq_storage"])  # shallow copy
        return VQStoragePolicy(mapping=mapping)

    def get(self, field: str, default: str = "masked_npz_both") -> str:
        return self.mapping.get(field, default)


class PNGCodec(BasePNGCodec):
    """Extended PNG codec with VQ storage policy for vector-quantized fields.

    - masked_npz_both: (默认) 通过NPZ存储紧凑的labels、codebook和mask
    - npz_both: 通过NPZ存储完整的labels和codebook
    - png_labels_npz_codebook: 已弃用 - codebook未持久化
    """

    def __init__(self, config: CodecConfig) -> None:
        super().__init__(config)
        self._vq_policy = VQStoragePolicy.from_params(config.params)

    def encode(self, frame_dir: Path, splats: TensorDict, quant_ctx: QuantizationContext) -> CodecArtifacts:
        # Intercept vector-quantized fields that request npz_both; others use base class behavior
        frame_dir.mkdir(parents=True, exist_ok=True)

        # We'll build artifacts manually to allow per-field control
        files: Dict[str, Dict[str, Any]] = {}

        for name, tensor in splats.items():
            stats = quant_ctx.field_stats.get(name)
            if stats is None:
                raise KeyError(f"Quantization stats missing for field '{name}'")

            # If vector with npz_both policy, write npz labels
            method = getattr(stats, "method", "scalar")
            policy = self._vq_policy.get(name)
            int_tensor = quant_ctx.int_values.get(name)

            if method == "vector" and policy == "npz_both":
                if int_tensor is None:
                    raise ValueError(
                        f"Field '{name}' vector quantization requires store_as_int=True to persist labels."
                    )
                out_path = frame_dir / f"{name}.npz"
                np.savez_compressed(out_path, data=int_tensor.detach().cpu().numpy())
                files[name] = {
                    "type": "npz_int",
                    "file": out_path.name,
                    "shape": list(tensor.shape),
                    "dtype": "int32",
                    # carry basic metadata similar to base PNG codec for symmetry
                    "channels": int(int_tensor.reshape(int_tensor.shape[0], -1).shape[1]),
                }
                continue

            # Support masked VQ: store compact labels and codebook via NPZ (skip PNG entirely)
            if method == "vector" and policy == "masked_npz_both":
                if int_tensor is None:
                    raise ValueError(
                        f"Field '{name}' vector quantization requires store_as_int=True to persist labels."
                    )
                # labels: only keep valid entries according to mask (must be provided in stats)
                if stats.mask is None:
                    raise ValueError(f"Masked VQ for '{name}' requires stats.mask to be present.")
                mask_np = np.asarray(stats.mask, dtype=np.bool_)
                labels_full = int_tensor.reshape(-1).detach().cpu().numpy()
                labels_valid = labels_full[mask_np].astype(np.uint16, copy=False)
                labels_path = frame_dir / f"{name}_labels_compact.npz"
                np.savez_compressed(labels_path, labels=labels_valid)
                # write codebook npz (quantized uint8 + dequant params) under frame_dir
                codebook_tensor = quant_ctx.codebooks.get(name)
                if codebook_tensor is None:
                    raise ValueError(f"Masked VQ for '{name}' requires quantized codebook in quant_ctx.codebooks")
                centroids_q = codebook_tensor.detach().cpu().numpy().astype(np.uint8)
                cb_bits = int(getattr(stats, "codebook_bits", 8) or 8)
                cb_mins = np.asarray(getattr(stats, "codebook_min_vals", []), dtype=np.float32)
                cb_maxs = np.asarray(getattr(stats, "codebook_max_vals", []), dtype=np.float32)
                codebook_path = frame_dir / f"{name}_codebook.npz"
                np.savez_compressed(codebook_path, centroids=centroids_q, mins=cb_mins, maxs=cb_maxs, bits=np.array(cb_bits, dtype=np.uint8))
                # write mask npz (bit-packed) under frame_dir
                packed = np.packbits(mask_np.astype(np.uint8), bitorder="little")
                mask_path = frame_dir / f"{name}_mask.npz"
                np.savez_compressed(mask_path, bits=packed, len=np.array(mask_np.size, dtype=np.int64))
                files[name] = {
                    "type": "npz_masked_vq",
                    "labels_file": labels_path.name,
                    "codebook_file": codebook_path.name,
                    "mask_file": mask_path.name,
                    "shape": list(tensor.shape),
                    "method": method,
                }
                continue

            # fallback to base PNG behavior for scalar fields only
            # Vector fields should use masked_npz_both or npz_both strategies to persist codebook
            if method == "vector":
                raise ValueError(
                    f"Field '{name}' uses vector quantization but reached PNG fallback. "
                    f"This indicates a bug in storage policy routing. "
                    f"Vector fields must use 'masked_npz_both' or 'npz_both' strategies."
                )
            
            # We reuse the parent's encode for this single field by duplicating essential logic here (to avoid re-encoding twice)
            # The following block is adapted from BasePNGCodec.encode with minimal changes
            n_points = tensor.shape[0]
            sidelen = int(np.sqrt(n_points))
            if sidelen * sidelen != n_points:
                raise ValueError(f"PNG codec requires perfect-square length after mapping, got {n_points} for '{name}'.")

            flattened = (int_tensor if int_tensor is not None else tensor).reshape(n_points, -1)
            channels = flattened.shape[1]

            if method == "scalar" and name == "means" and stats.bitwidth > 8:
                # 16-bit split
                levels = (1 << stats.bitwidth) - 1
                clamped = torch.clamp(flattened, 0, levels)
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
                    "shape": list(stats.tensor_shape or tensor.shape),
                    "original_shape": list(stats.original_shape or tensor.shape),
                    "sidelen": sidelen,
                    "channels": channels,
                    "bitwidth": stats.bitwidth,
                    "method": method,
                }
                continue

            # generic PNG path
            bitwidth = int(stats.bitwidth)
            levels = (1 << bitwidth) - 1
            clamped = torch.clamp(flattened, 0, levels)
            dtype = np.uint8 if bitwidth <= 8 else np.uint16
            image = clamped.detach().cpu().numpy().astype(dtype).reshape(sidelen, sidelen, channels)

            if method == "scalar" and name == "shN":
                # group into 3 channels per file to keep PNG RGB packing efficient
                chunk_files: List[str] = []
                per = 3
                num_chunks = int(np.ceil(channels / per))
                for idx in range(num_chunks):
                    start = idx * per
                    end = min((idx + 1) * per, channels)
                    chunk = image[..., start:end]
                    if chunk.shape[2] < per:
                        pad_width = ((0, 0), (0, 0), (0, per - chunk.shape[2]))
                        chunk = np.pad(chunk, pad_width, mode="constant")
                    fname = f"{name}_{idx:03d}.png"
                    imageio.imwrite(frame_dir / fname, chunk)
                    chunk_files.append(fname)
                files[name] = {
                    "type": "png_group",
                    "files": chunk_files,
                    "shape": list(stats.tensor_shape or tensor.shape),
                    "original_shape": list(stats.original_shape or tensor.shape),
                    "sidelen": sidelen,
                    "channels": channels,
                    "channels_per_file": per,
                    "bitwidth": bitwidth,
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
                "shape": list(stats.tensor_shape or tensor.shape),
                "original_shape": list(stats.original_shape or tensor.shape),
                "sidelen": sidelen,
                "channels": channels,
                "bitwidth": bitwidth,
                "dtype": np.dtype(dtype).name,
                "method": method,
            }

        return CodecArtifacts(name=self.config.name, files=files)

    def decode(self, frame_dir: Path, artifacts: CodecArtifacts, quant_ctx: QuantizationContext) -> TensorDict:
        restored: TensorDict = {}

        for name, meta in artifacts.files.items():
            mtype = meta.get("type")
            if mtype == "npz_int":
                arr = np.load(frame_dir / meta["file"]) ["data"]
                restored[name] = torch.from_numpy(arr.astype(np.float32))
                continue
            if mtype == "npz_masked_vq":
                labels_file = meta.get("labels_file")
                if not labels_file:
                    raise ValueError(f"Masked VQ entry for '{name}' missing labels_file")
                labels_valid = np.load(frame_dir / labels_file)["labels"].astype(np.int32)
                # read mask npz
                mask_file = meta.get("mask_file")
                if not mask_file:
                    raise ValueError(f"Masked VQ entry for '{name}' missing mask_file")
                mnpz = np.load(frame_dir / mask_file)
                packed = mnpz["bits"]
                total = int(mnpz["len"]) if "len" in mnpz else None
                mask_np = np.unpackbits(packed, bitorder="little").astype(np.bool_)
                if total is not None:
                    mask_np = mask_np[:total]
                full = np.zeros(mask_np.shape[0], dtype=np.int32)
                full[mask_np] = labels_valid
                restored[name] = torch.from_numpy(full[:, None].astype(np.float32))
                # populate quant_ctx stats.codebook from codebook npz
                cb_file = meta.get("codebook_file")
                if not cb_file:
                    raise ValueError(f"Masked VQ entry for '{name}' missing codebook_file")
                cnpz = np.load(frame_dir / cb_file)
                centroids_q = cnpz["centroids"].astype(np.uint8)
                cb_bits = int(cnpz["bits"]) if "bits" in cnpz else 8
                cb_mins = cnpz["mins"].astype(np.float32)
                cb_maxs = cnpz["maxs"].astype(np.float32)
                # dequantize codebook to float for dequantization stage
                levels = (1 << cb_bits) - 1
                rng = cb_maxs - cb_mins
                rng[rng < 1e-8] = 1.0
                codebook = (centroids_q.astype(np.float32) / levels) * rng + cb_mins
                # ensure stats exists
                stats = quant_ctx.field_stats.get(name)
                if stats is not None:
                    stats.codebook = codebook.tolist()
                    stats.codebook_bits = cb_bits
                    stats.codebook_min_vals = cb_mins.tolist()
                    stats.codebook_max_vals = cb_maxs.tolist()
                    stats.codebook_shape = list(centroids_q.shape)
                    stats.mask = mask_np.astype(np.int32).tolist()
                continue

            # fallback to base behaviors similar to original PNG codec
            sidelen = int(meta["sidelen"])
            channels = int(meta["channels"])
            data_shape = list(meta.get("shape", []))
            original_shape = list(meta.get("original_shape", data_shape))
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
                arr = imageio.imread(frame_dir / files_list[0])
                if arr.ndim == 2:
                    arr = arr[..., None]
                flat = arr.reshape(sidelen * sidelen, channels).astype(np.float32)
                restored[name] = torch.from_numpy(flat).reshape(data_shape)
                continue

            raise ValueError(f"Unsupported codec file type '{meta.get('type')}' for field '{name}'")

        return restored


# Override the default 'png' codec with extended version supporting VQ policies
register_codec("png", PNGCodec)


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
            data = np.load(frame_dir / meta["file"]) ["data"]
            restored[name] = torch.from_numpy(data.astype(np.float32))
        return restored


register_codec("npz_debug", NPZDebugCodec)


