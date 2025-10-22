from __future__ import annotations

from abc import ABC, abstractmethod
from pathlib import Path
from typing import Dict

from ...io import TensorDict
from ...stages import QuantizationContext
from ...codec import CodecArtifacts


class CodecBase(ABC):
    @abstractmethod
    def encode(self, frame_dir: Path, splats: TensorDict, quant_ctx: QuantizationContext) -> CodecArtifacts:
        """Encode tensor dict into artifacts on disk and return metadata."""

    @abstractmethod
    def decode(self, frame_dir: Path, artifacts: CodecArtifacts, quant_ctx: QuantizationContext) -> TensorDict:
        """Decode artifacts back into quantized-domain tensors."""


