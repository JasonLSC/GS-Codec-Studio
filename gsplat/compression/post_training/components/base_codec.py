from __future__ import annotations

from abc import ABC, abstractmethod
from pathlib import Path
from typing import Dict

from ..configs import CodecConfig
from ..stages import QuantizationContext, TensorDict
from ..codec import CodecArtifacts  # reuse existing artifacts container


class BaseCodec(ABC):
    def __init__(self, config: CodecConfig) -> None:
        self.config = config

    @abstractmethod
    def encode(self, frame_dir: Path, splats: TensorDict, quant_ctx: QuantizationContext) -> CodecArtifacts:
        """Encode quantized splats to files and return artifact metadata."""
        raise NotImplementedError

    @abstractmethod
    def decode(self, frame_dir: Path, artifacts: CodecArtifacts, quant_ctx: QuantizationContext) -> TensorDict:
        """Decode artifacts back into tensors (usually integer labels/values)."""
        raise NotImplementedError


