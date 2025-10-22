from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, Mapping, Optional

import torch
from torch import Tensor

from .ada_mask import AnnealingMask
from .config import MaskConfig


@dataclass
class MaskResult:
    value: Tensor
    loss: Optional[Tensor] = None
    metrics: Dict[str, Any] = field(default_factory=dict)


class AdaptiveMaskBase:
    def __init__(self, config: MaskConfig):
        self.config = config

    def maybe_update(self, step: int, splats: Mapping[str, Tensor]) -> None:
        """Observe the current splats before running compression."""

    def apply(self, tensor: Tensor, step: int) -> MaskResult:
        """Return the masked tensor along with optional loss/metrics."""
        return MaskResult(value=tensor)

    def step_optimizer(self, step: int) -> None:
        """Hook for stepping any optimizers the mask may own."""

    def state_dict(self) -> Dict[str, Any]:
        return {}

    def load_state_dict(self, state: Mapping[str, Any]) -> None:
        if state:
            raise ValueError("AdaptiveMaskBase.load_state_dict received unexpected state")

    def get_binary_mask(self) -> Optional[Tensor]:
        return None


class NullAdaptiveMask(AdaptiveMaskBase):
    def __init__(self) -> None:
        super().__init__(MaskConfig())

    def maybe_update(self, step: int, splats: Mapping[str, Tensor]) -> None:
        return


class LearnableAdaptiveMask(AdaptiveMaskBase):
    def __init__(self, config: MaskConfig, device: torch.device) -> None:
        super().__init__(config)
        self._device = device
        self._module: Optional[AnnealingMask] = None
        self._optimizer: Optional[torch.optim.Optimizer] = None
        self._active_size: int = 0
        self._last_ratio: float = 0.0

    def _build_module(self, size: int, device: torch.device) -> None:
        settings = self.config.learnable
        self._module = AnnealingMask(
            input_shape=[size, 1, 1],
            device=device,
            total_iters=settings.total_iters,
            start_temp=settings.start_temp,
            end_temp=settings.end_temp,
            annealing_start_iter=self.config.start_step,
            target_sparsity=settings.target_sparsity,
        )
        self._optimizer = torch.optim.Adam(
            [{"params": self._module.parameters(), "lr": settings.lr}]
        )
        self._active_size = size

    def _expand_if_needed(self, size: int) -> None:
        assert self._module is not None
        if size <= self._module.mask_logits.shape[0]:
            self._active_size = size
            return
        old_logits = self._module.mask_logits.detach()
        device = old_logits.device
        new_logits = torch.zeros((size, 1, 1), device=device, dtype=old_logits.dtype)
        new_logits[: old_logits.shape[0]] = old_logits
        self._module.mask_logits = torch.nn.Parameter(new_logits)
        settings = self.config.learnable
        self._optimizer = torch.optim.Adam(
            [{"params": self._module.parameters(), "lr": settings.lr}]
        )
        self._active_size = size

    def _ensure_module(self, tensor: Tensor) -> None:
        size = tensor.shape[0]
        device = tensor.device
        if self._module is None:
            self._build_module(size, device)
        else:
            if self._module.mask_logits.device != device:
                self._module = self._module.to(device)
                settings = self.config.learnable
                self._optimizer = torch.optim.Adam(
                    [{"params": self._module.parameters(), "lr": settings.lr}]
                )
            self._expand_if_needed(size)

    def maybe_update(self, step: int, splats: Mapping[str, Tensor]) -> None:
        shn = splats.get("shN")
        if shn is None:
            return
        self._ensure_module(shn)
        with torch.no_grad():
            mask = (shn != 0).any(dim=-1).any(dim=-1)
            self._last_ratio = mask.float().mean().item()

    def apply(self, tensor: Tensor, step: int) -> MaskResult:
        self._ensure_module(tensor)
        modulus = self._module
        assert modulus is not None
        metrics: Dict[str, Any] = {"mask_strategy": "learnable", "mask_ratio": self._last_ratio}
        if step <= self.config.start_step:
            return MaskResult(value=tensor, metrics=metrics)
        masked = modulus(tensor, step)
        current_ratio = modulus.get_mask_ratio().item()
        self._last_ratio = current_ratio
        metrics["mask_ratio"] = current_ratio
        raw_loss = modulus.get_sparsity_loss()
        loss = None if self.config.regularization_weight == 0.0 else raw_loss * self.config.regularization_weight
        return MaskResult(value=masked, loss=loss, metrics=metrics)

    def step_optimizer(self, step: int) -> None:
        if step <= self.config.start_step:
            return
        if self._optimizer is None:
            return
        self._optimizer.step()
        self._optimizer.zero_grad(set_to_none=True)

    def state_dict(self) -> Dict[str, Any]:
        if self._module is None:
            return {}
        return {
            "module": self._module.state_dict(),
            "optimizer": self._optimizer.state_dict() if self._optimizer is not None else None,
            "active_size": self._active_size,
        }

    def load_state_dict(self, state: Mapping[str, Any]) -> None:
        if not state:
            return
        active_size = int(state.get("active_size", 0))
        if self._module is None and active_size > 0:
            self._build_module(active_size, self._device)
        if self._module is not None and "module" in state:
            self._module.load_state_dict(state["module"])
        if self._optimizer is not None and state.get("optimizer") is not None:
            self._optimizer.load_state_dict(state["optimizer"])

    def get_binary_mask(self) -> Optional[Tensor]:
        if self._module is None:
            return None
        mask = self._module.get_binary_mask()
        if mask is None:
            return None
        if self._active_size and mask.shape[0] != self._active_size:
            return mask[: self._active_size]
        return mask


class GradientAdaptiveMask(AdaptiveMaskBase):
    def __init__(self, config: MaskConfig):
        super().__init__(config)
        self._threshold = float(config.gradient.grad_threshold)
        self._start_step = config.start_step
        self._mask_ratio: float = 0.0
        self._handle: Optional[torch.utils.hooks.RemovableHandle] = None
        self._param_ref: Optional[Tensor] = None

    def _register_hook(self, tensor: Tensor) -> None:
        if self._handle is not None and self._param_ref is tensor:
            return
        if self._handle is not None:
            self._handle.remove()
        self._param_ref = tensor

        def hook(grad: Tensor) -> Tensor:
            if grad is None:
                return grad
            shn = tensor.detach()
            num_gaussians = shn.shape[0]
            gaussian_zero = (shn.reshape(num_gaussians, -1) == 0).all(dim=-1)
            grad_flat = grad.reshape(grad.shape[0], -1)
            indices = torch.arange(grad.shape[0], device=grad.device) % num_gaussians
            grad_norm = grad_flat.norm(p=2, dim=-1)
            inactive = gaussian_zero[indices] & (grad_norm < self._threshold)
            mask = (~inactive).to(grad.dtype)
            while mask.dim() < grad.dim():
                mask = mask.unsqueeze(-1)
            return grad * mask

        self._handle = tensor.register_hook(hook)

    def maybe_update(self, step: int, splats: Mapping[str, Tensor]) -> None:
        shn = splats.get("shN")
        if shn is None:
            return
        active = (shn != 0).any(dim=-1).any(dim=-1)
        self._mask_ratio = active.float().mean().item()
        if step > self._start_step:
            self._register_hook(shn)

    def apply(self, tensor: Tensor, step: int) -> MaskResult:
        metrics = {
            "mask_strategy": "gradient",
            "mask_ratio": self._mask_ratio,
            "mask_grad_threshold": self._threshold,
        }
        return MaskResult(value=tensor, metrics=metrics)

    def load_state_dict(self, state: Mapping[str, Any]) -> None:
        if state and "threshold" in state:
            self._threshold = float(state["threshold"])

    def state_dict(self) -> Dict[str, Any]:
        return {"threshold": self._threshold}

    def get_binary_mask(self) -> Optional[Tensor]:
        if self._param_ref is None:
            return None
        shn = self._param_ref.detach()
        mask = (shn != 0).any(dim=-1).any(dim=-1)
        return mask.float().view(-1, 1, 1)


class AdaptiveMaskFactory:
    @staticmethod
    def create(config: MaskConfig, device: Optional[torch.device]) -> AdaptiveMaskBase:
        if not config.enabled or config.strategy is None:
            return NullAdaptiveMask()
        if config.strategy == "learnable":
            if device is None:
                device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
            return LearnableAdaptiveMask(config, device=device)
        if config.strategy == "gradient":
            return GradientAdaptiveMask(config)
        raise ValueError(f"Unsupported mask strategy '{config.strategy}'")
