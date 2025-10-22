
from __future__ import annotations

import json
from dataclasses import dataclass, field
import logging
from datetime import datetime
from pathlib import Path
from typing import Dict, Optional
# import math

import torch
import torch.nn.functional as F
import numpy as np

from gsplat.utils import log_transform, inverse_log_transform

from .configs import PTCompConfig
from .io import (
    TensorDict,
    load_ckpt_file,
    load_ply_file,
    save_ply_file,
)
from .stages import (
    MappingContext,
    PruningContext,
)
from .components.quant.types import QuantFieldStats, QuantizationContext
from .components import Pruner, PrePostProcessor, Mapper
from .components.quant.manager import QuantManager
from .components.codec.manager import CodecManager
from .codec import CodecArtifacts


@dataclass
class CompressionResult:
    payload_dir: Path
    frame_dir: Path
    metadata: Dict[str, object] = field(default_factory=dict)


@dataclass
class DecodeResult:
    frame: TensorDict
    saved_path: Path
    metadata: Dict[str, object] = field(default_factory=dict)


logger = logging.getLogger(__name__)


class PostTrainingCompressor:
    """Single-frame orchestrator for post-training compression."""

    def __init__(self, config: PTCompConfig, output_dir: Path) -> None:
        self.config = config
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

        self._frame: TensorDict | None = None
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

        # Components
        self.preprocessor = PrePostProcessor(config.preprocess)
        self.pruner = Pruner(config.pruning)
        self.quant_manager = QuantManager(config.quant)
        self.mapper = Mapper(config.mapping)
        self.codec_manager = CodecManager(config.codec)

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------
    def encode(self) -> CompressionResult:
        logger.info("Post-training encode started.")
        frame = self._load_input()
        logger.info("Input loaded; running preprocessing.")

        # Step 1: Preprocessing transforms (log transform, quat normalization)
        preprocessed = self.preprocessor.pre_quantization_transform(frame)
        logger.info("Applied preprocessing transforms.")
        
        # Step 2: Pruning
        pruned, pruning_ctx = self.pruner.prune(preprocessed)
        logger.info(
            "Pruning completed (enabled=%s, kept=%d, removed=%d).",
            pruning_ctx.enabled,
            pruning_ctx.kept,
            pruning_ctx.removed,
        )
        
        # Step 3: Quantization
        quantized, quant_ctx = self.quant_manager.quantize_all(pruned)
        logger.info("Quantization completed for %d fields.", len(quant_ctx.field_stats))
        
        # Step 4: Mapping
        mapped, mapping_ctx = self.mapper.map(quantized, quant_ctx)
        logger.info("Mapping completed with strategy=%s.", mapping_ctx.strategy)

        # Step 5: Codec encoding
        frame_dir = self._frame_dir()
        frame_dir.mkdir(parents=True, exist_ok=True)
        artifacts = self.codec_manager.encode_all(frame_dir, mapped, quant_ctx)
        logger.info("Codec encode completed with files=%s.", list(artifacts.files.keys()))
        
        # Save decoding metadata
        self._write_frame_decode_metadata(frame_dir, artifacts, quant_ctx)
        
        # Save compression info and get metadata for result
        compression_info_adata = self._save_compression_info(pruning_ctx, quant_ctx, mapping_ctx, artifacts)
        
        logger.info("Encode finished; results stored in %s.", self.output_dir)
        return CompressionResult(payload_dir=self.output_dir, frame_dir=frame_dir, metadata=compression_info_adata)

    def decode(self, payload_dir: Optional[Path] = None, save_path: Optional[Path] = None) -> DecodeResult:
        logger.info("Post-training decode started.")
        root = Path(payload_dir) if payload_dir is not None else self.output_dir
        if not root.exists():
            raise FileNotFoundError(f"Compressed directory does not exist: {root}")

        frame_dir = self._frame_dir(root)
        if not frame_dir.exists():
            raise FileNotFoundError(f"Frame directory missing: {frame_dir}")

        # read single-file manifest for decoding
        manifest_path = frame_dir / "metadata_for_decoding.json"
        if not manifest_path.exists():
            raise FileNotFoundError(f"Missing decoding manifest: {manifest_path}")
        with manifest_path.open("r") as f:
            manifest = json.load(f)
        codec_meta = manifest.get("codec")
        quant_section = manifest.get("quant", {})
        quant_ctx = self._quant_context_from_manifest(quant_section)
        artifacts = CodecArtifacts.from_meta(codec_meta)
        logger.info("Running codec decode with files=%s.", list(artifacts.files.keys()))

        decoded_quant = self.codec_manager.decode_all(frame_dir, artifacts, quant_ctx)
        restored = self.quant_manager.dequantize_all(decoded_quant, quant_ctx)
        restored = self.preprocessor.post_quantization_inverse_transform(restored)
        logger.info("Decode completed; writing outputs to disk.")

        target = Path(save_path) if save_path is not None else root / "decoded_frame.ply"
        target.parent.mkdir(parents=True, exist_ok=True)
        save_ply_file(restored, target)
        logger.info("Finished writing outputs to disk.")

        return DecodeResult(frame=restored, saved_path=target, metadata=manifest)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------
    def _write_frame_decode_metadata(self, frame_dir: Path, artifacts: CodecArtifacts, quant_ctx: QuantizationContext) -> None:
        """Write minimal metadata required for decoding."""
        manifest = {
            "codec": artifacts.to_meta(),
            "quant": {
                name: stats.to_dict(mode="full")
                for name, stats in quant_ctx.field_stats.items()
            },
        }
        with (frame_dir / "metadata_for_decoding.json").open("w") as f:
            json.dump(manifest, f, indent=2)

    def _save_compression_info(
        self,
        pruning_ctx: PruningContext,
        quant_ctx: QuantizationContext,
        mapping_ctx: MappingContext,
        artifacts: CodecArtifacts,
    ) -> Dict[str, object]:
        """Save lightweight compression metadata and return it."""
        info = {
            "input": {
                "type": self.config.input_spec.input_type,
                "path": str(self.config.input_spec.path),
            },
            "timestamp": datetime.utcnow().isoformat() + "Z",
            "pruning": pruning_ctx.to_dict(),
            "quantization": quant_ctx.to_summary(),
            "mapping": mapping_ctx.to_dict(include_permutation=False),
            "codec": {
                "name": artifacts.name,
                "files_count": len(artifacts.files),
                "fields": list(artifacts.files.keys()),
            },
            "config": {
                "pruning_enabled": self.config.pruning.enabled,
                "mapping_strategy": self.config.mapping.strategy,
                "codec_name": self.config.codec.name,
            },
        }
        
        info_path = self.output_dir / "compression_info.json"
        with info_path.open("w") as f:
            json.dump(info, f, indent=2)
        logger.info(f"Saved compression info to {info_path}")
        
        return info

    def _quant_context_from_manifest(self, quant_section: Dict[str, object]) -> QuantizationContext:
        context = QuantizationContext()
        if not isinstance(quant_section, dict):
            return context
        for name, entry in quant_section.items():
            if not isinstance(entry, dict):
                continue
            min_vals = [float(v) for v in entry.get("min_vals", [])]
            max_vals = [float(v) for v in entry.get("max_vals", [])]
            # ensure channels for scalar method
            channels = int(entry.get("channels", 0))
            if channels == 0 and len(min_vals) > 0:
                channels = len(min_vals)
            context.register(
                name,
                QuantFieldStats(
                    bitwidth=int(entry.get("bitwidth", 8)),
                    min_vals=min_vals,
                    max_vals=max_vals,
                    clamp_min=entry.get("clamp_min"),
                    clamp_max=entry.get("clamp_max"),
                    channels=channels,
                    method=str(entry.get("method", "scalar")),
                    tensor_shape=[int(v) for v in (entry.get("tensor_shape") or [])] or None,
                    original_shape=[int(v) for v in (entry.get("original_shape") or [])] or None,
                    codebook_bits=int(entry.get("codebook_bits", 8)) if entry.get("codebook_bits") is not None else None,
                    codebook_min_vals=[float(v) for v in entry.get("codebook_min_vals", [])] or None,
                    codebook_max_vals=[float(v) for v in entry.get("codebook_max_vals", [])] or None,
                    codebook_shape=[int(v) for v in (entry.get("codebook_shape") or [])] or None,
                ),
            )
        return context

    def _load_input(self) -> TensorDict:
        if self._frame is not None:
            return self._frame

        spec = self.config.input_spec
        if spec.input_type == "ply":
            frame = load_ply_file(spec.path)
        elif spec.input_type == "ckpt":
            frame = load_ckpt_file(spec.path)
        else:
            raise ValueError(f"Unsupported input type {spec.input_type}")

        if self.device.type == "cuda":
            frame = {k: v.to(self.device, non_blocking=True) for k, v in frame.items()}
        self._frame = frame
        return frame

    def _frame_dir(self, root: Optional[Path] = None) -> Path:
        base = root if root is not None else self.output_dir
        return base / "compressed_data"

    def _load_metadata(self, root: Path) -> Dict[str, object]:
        meta_path = root / "meta.json"
        if not meta_path.exists():
            raise FileNotFoundError(f"Missing meta.json under {root}")
        with meta_path.open("r") as f:
            metadata = json.load(f)
        self.metadata = metadata
        return metadata


