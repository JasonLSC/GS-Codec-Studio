"""Utilities and helpers for post-training compression."""

from .io import (
    load_ply_file,
    load_ply_sequence,
    save_ply_file,
    save_ply_sequence,
    load_ckpt_file,
)
from .configs import (
    CodecConfig,
    InputSpec,
    MappingConfig,
    PTCompConfig,
    PreprocessConfig,
    PruningConfig,
    PruningHook,
    QuantConfig,
    QuantFieldConfig,
)
from .stages import (
    PruningStepRecord,
    PruningContext,
    MappingContext,
)
from .components import Pruner, PrePostProcessor, Mapper, BaseCodec
from .codec import build_codec, CodecArtifacts
from .codecs import PNGCodec  # ensure extended PNG codec is imported and registered
from .legacy_png import LegacyPNGCodec
from .orchestrator import (
    PostTrainingCompressor,
    CompressionResult,
    DecodeResult,
)

# Import codecs package to activate codec registrations (png, npz_debug, etc.)
from . import codecs as _codecs  # noqa: F401

__all__ = [
    # IO
    "load_ply_file",
    "load_ply_sequence",
    "save_ply_file",
    "save_ply_sequence",
    "load_ckpt_file",
    # Configs
    "CodecConfig",
    "InputSpec",
    "MappingConfig",
    "PTCompConfig",
    "PreprocessConfig",
    "PruningConfig",
    "PruningHook",
    "QuantConfig",
    "QuantFieldConfig",
    "MappingContext",
    "PruningContext",
    # Codec
    "build_codec",
    "CodecArtifacts",
    "PNGCodec",
    "LegacyPNGCodec",
    # Stages
    "PruningStepRecord",
    # Orchestrator
    "PostTrainingCompressor",
    "CompressionResult",
    "DecodeResult",
    # Components
    "Pruner",
    "PrePostProcessor",
    "Mapper",
    "BaseCodec",
]
