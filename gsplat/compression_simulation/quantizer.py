from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Optional, Tuple

import torch
from torch import Tensor

from .config import AttributeQuantizerConfig


@dataclass
class QuantizeResult:
    value: Tensor
    q_step: Optional[Tensor]
    metadata: Dict[str, Tensor]


class DifferentiableQuantizer:
    def __init__(self, name: str, config: AttributeQuantizerConfig):
        self.name = name
        self.config = config

    def quantize(self, tensor: Tensor, step: int) -> QuantizeResult:
        if not self.config.enabled or self.config.bitwidth is None:
            return QuantizeResult(value=tensor, q_step=None, metadata={})

        bitwidth = self._select_bitwidth(step)
        if bitwidth is None:
            return QuantizeResult(value=tensor, q_step=None, metadata={})

        clamp_range = self.config.clamp_range
        if clamp_range is None:
            raise ValueError(f"Quantizer '{self.name}' requires clamp_range when enabled")

        lower, upper = clamp_range
        if upper <= lower:
            raise ValueError(f"Quantizer '{self.name}' has invalid clamp range {clamp_range}")

        q_step = self._create_q_step(tensor, lower, upper, bitwidth)

        mode = self.config.mode
        if mode == "noise":
            value = self._apply_noise_quantization(tensor, lower, upper, q_step)
        elif mode == "round":
            value = self._apply_round_quantization(tensor, lower, upper, bitwidth)
        else:
            raise ValueError(f"Unsupported quantizer mode '{mode}' for attribute '{self.name}'")

        return QuantizeResult(
            value=value,
            q_step=q_step,
            metadata={"bitwidth": torch.tensor(bitwidth, device=tensor.device, dtype=tensor.dtype)},
        )

    def _select_bitwidth(self, step: int) -> Optional[int]:
        bitwidth = self.config.bitwidth
        if bitwidth is None:
            return None
        warmup_steps = self.config.warmup_steps
        if warmup_steps is not None and step < warmup_steps:
            return self.config.warmup_bitwidth or bitwidth
        return bitwidth

    def _create_q_step(self, tensor: Tensor, lower: float, upper: float, bitwidth: int) -> Tensor:
        denom = (2 ** bitwidth) - 1
        step_value = (upper - lower) / denom
        return torch.as_tensor(step_value, dtype=tensor.dtype, device=tensor.device)

    def _apply_noise_quantization(self, tensor: Tensor, lower: float, upper: float, q_step: Tensor) -> Tensor:
        clamped = tensor.clamp(lower, upper)
        noise = torch.empty_like(tensor).uniform_(-0.5, 0.5)
        quantized = clamped + noise * q_step
        return quantized.clamp(lower, upper)

    def _apply_round_quantization(self, tensor: Tensor, lower: float, upper: float, bitwidth: int) -> Tensor:
        return _RoundSTE.apply(tensor, lower, upper, bitwidth)


class _RoundSTE(torch.autograd.Function):
    @staticmethod
    def forward(ctx, input: Tensor, lower: float, upper: float, bitwidth: int) -> Tensor:
        clamped = input.clamp(lower, upper)
        denom = (2 ** bitwidth) - 1
        q_step = (upper - lower) / denom
        levels = torch.round((clamped - lower) / q_step)
        return levels * q_step + lower

    @staticmethod
    def backward(ctx, grad_output: Tensor) -> Tuple[Tensor, None, None, None]:
        return grad_output, None, None, None


def build_quantizer(name: str, config: AttributeQuantizerConfig) -> Optional[DifferentiableQuantizer]:
    if not config.enabled or config.bitwidth is None:
        return None
    return DifferentiableQuantizer(name=name, config=config)
