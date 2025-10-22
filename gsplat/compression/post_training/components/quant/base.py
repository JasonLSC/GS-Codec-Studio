from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Tuple

from ...io import TensorDict
from ...stages import QuantizationContext


class QuantizerBase(ABC):
    @abstractmethod
    def quantize(self, splats: TensorDict) -> Tuple[TensorDict, QuantizationContext]:
        """Quantize input splats and return quantized tensors with context."""

    @abstractmethod
    def dequantize(self, splats: TensorDict, context: QuantizationContext) -> TensorDict:
        """Reconstruct floating tensors from quantized tensors and context."""


