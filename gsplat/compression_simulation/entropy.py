from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Optional, Tuple

import torch
from torch import Tensor

from .config import EntropyConfig
from .entropy_model import (
    Entropy_factorized_optimized_refactor,
    Entropy_gaussian,
)


@dataclass
class EntropyResult:
    bits: Optional[Tensor]
    loss: Optional[Tensor]
    metrics: Dict[str, Any]


class EntropyConstraint:
    def __init__(self, config: EntropyConfig, device: torch.device):
        self.config = config
        self.device = device
        self.models: Dict[str, Any] = {}
        self.optimizers: Dict[str, torch.optim.Optimizer] = {}
        self.schedulers: Dict[str, Optional[torch.optim.lr_scheduler._LRScheduler]] = {}
        self.state: Dict[str, Dict[str, Any]] = {}

        if not config.enabled:
            self.model_type = None
            return

        model_type = config.model_type
        supported_attrs = {"scales", "quats", "sh0"} # "opacities"
        gaussian_supported = {"scales", "quats", "sh0"}

        for attr, step in config.steps.items():
            if step < 0:
                continue
            if model_type == "factorized_model":
                if attr not in supported_attrs:
                    continue
                model = Entropy_factorized_optimized_refactor(channel=_infer_channel(attr)).to(device)
                optimizer = torch.optim.Adam(
                    [{"params": p, "lr": config.factorized_lr, "name": n} for n, p in model.named_parameters()]
                )
                scheduler = None
            elif model_type == "gaussian_model":
                if attr not in gaussian_supported:
                    continue
                model = Entropy_gaussian(channel=_infer_channel(attr)).to(device)
                optimizer = torch.optim.Adam(
                    [
                        {"params": model.param_regressor.hash_grid.parameters(), "lr": config.gaussian_lr, "name": "hash_grid"},
                        {"params": model.param_regressor.mlp_regressor.parameters(), "lr": config.gaussian_lr, "name": "mlp_regressor"},
                    ]
                )
                decay_steps = max(1, 30000 - step)
                gamma = config.scheduler_gamma ** (1.0 / decay_steps)
                scheduler = torch.optim.lr_scheduler.ExponentialLR(optimizer, gamma=gamma)
            else:
                raise ValueError(f"Unsupported entropy model type '{model_type}'")

            name = _attr_key(attr)
            self.models[name] = model
            self.optimizers[name] = optimizer
            self.schedulers[name] = scheduler
            self.state[name] = {"step_threshold": step}

        self.model_type = model_type

    def maybe_update(self, step: int, splats: Dict[str, Tensor]) -> None:
        if not self.config.enabled or self.model_type != "gaussian_model":
            return

        means = splats.get("means")
        if means is None:
            return

        for name in self.models.keys():
            state = self.state[name]
            threshold = state["step_threshold"]
            if step == threshold:
                state["bbox"] = _compute_bbox(means)

            if step > threshold:
                bbox = state.get("bbox")
                if bbox is None:
                    bbox = _compute_bbox(means)
                    state["bbox"] = bbox
                state["sample_mask"] = _sample_mask(means, bbox, ratio=0.05)

    def evaluate(
        self,
        attr: str,
        tensor: Tensor,
        q_step: Optional[Tensor],
        step: int,
        meta: Optional[Dict[str, Any]] = None,
    ) -> EntropyResult:
        if not self.config.enabled:
            return EntropyResult(bits=None, loss=None, metrics={})

        key = _attr_key(attr)
        if key not in self.models:
            return EntropyResult(bits=None, loss=None, metrics={})

        state = self.state[key]
        if step <= state["step_threshold"]:
            return EntropyResult(bits=None, loss=None, metrics={})

        if q_step is not None:
            if q_step.device != tensor.device:
                q_step = q_step.to(tensor.device)
            if q_step.dim() == 0:
                q_step = q_step.view(1)

        model = self.models[key]
        if isinstance(model, Entropy_factorized_optimized_refactor):
            bits = model(tensor, Q=q_step)
        elif isinstance(model, Entropy_gaussian):
            means = meta.get("means") if meta else None
            if means is None:
                return EntropyResult(bits=None, loss=None, metrics={})
            sample_mask = state.get("sample_mask")
            if sample_mask is not None:
                if not sample_mask.any():
                    return EntropyResult(bits=None, loss=None, metrics={})
                selected = tensor[sample_mask]
                positions = means[sample_mask]
            else:
                selected = tensor
                positions = means
            bits = model(selected, Q=q_step, pos=positions)
        else:
            raise TypeError(f"Unsupported model instance for '{attr}'")

        if bits.numel() == 0:
            return EntropyResult(bits=None, loss=None, metrics={})

        loss = bits.mean()
        metrics = {f"entropy/{attr}_bits_mean": loss.detach().cpu().item()}
        return EntropyResult(bits=bits, loss=loss, metrics=metrics)

    def step_optimizers(self, step: int) -> None:
        if not self.config.enabled:
            return
        for name, optimizer in self.optimizers.items():
            threshold = self.state[name]["step_threshold"]
            if step <= threshold:
                continue
            optimizer.step()
            optimizer.zero_grad(set_to_none=True)
            scheduler = self.schedulers.get(name)
            if scheduler is not None:
                scheduler.step()

    def state_dict(self) -> Dict[str, Any]:
        if not self.config.enabled:
            return {}
        return {
            "models": {name: model.state_dict() for name, model in self.models.items()},
            "optimizers": {name: opt.state_dict() for name, opt in self.optimizers.items()},
            "schedulers": {name: sch.state_dict() if sch is not None else None for name, sch in self.schedulers.items()},
            "state": self.state,
            "model_type": self.model_type,
        }

    def load_state_dict(self, state: Dict[str, Any]) -> None:
        if not state:
            return
        for name, model_state in state.get("models", {}).items():
            if name in self.models:
                self.models[name].load_state_dict(model_state)
        for name, opt_state in state.get("optimizers", {}).items():
            if name in self.optimizers:
                self.optimizers[name].load_state_dict(opt_state)
        for name, sch_state in state.get("schedulers", {}).items():
            scheduler = self.schedulers.get(name)
            if scheduler is not None and sch_state is not None:
                scheduler.load_state_dict(sch_state)
        for name, data in state.get("state", {}).items():
            if name in self.state:
                self.state[name].update(data)


def _attr_key(attr: str) -> str:
    return attr


def _infer_channel(attr: str) -> int:
    if attr == "quats":
        return 4
    if attr == "opacities":
        return 1
    return 3


def _compute_bbox(means: Tensor, low: float = 0.01, high: float = 0.99) -> Tuple[Tensor, Tensor]:
    probs = torch.tensor([low, high], device=means.device)
    quantiles = torch.quantile(means, probs, dim=0)
    lower, upper = quantiles[0], quantiles[1]
    return lower, upper


def _sample_mask(means: Tensor, bbox: Tuple[Tensor, Tensor], ratio: float) -> Tensor:
    lower, upper = bbox
    inside = torch.all((means >= lower) & (means <= upper), dim=1)
    random_values = torch.rand_like(means[:, 0])
    return (random_values < ratio) & inside
