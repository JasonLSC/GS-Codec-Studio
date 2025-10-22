from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, Mapping, Optional, Tuple, Literal


ATTRIBUTE_NAMES = (
    "means",
    "scales",
    "quats",
    "opacities",
    "sh0",
    "shN",
)


def _default_entropy_steps() -> Dict[str, int]:
    return {
        "means": -1,
        "quats": 10_000,
        "scales": 10_000,
        "opacities": 10_000,
        "sh0": 20_000,
        "shN": 10_000,
    }


def _default_bitwidths() -> Dict[str, Optional[int]]:
    return {
        "means": None,
        "scales": 8,
        "quats": 8,
        "opacities": 8,
        "sh0": 8,
        "shN": None,
    }


def _default_clamp_ranges() -> Dict[str, Optional[Tuple[float, float]]]:
    return {
        "means": None,
        "scales": (-10.0, 2.0),
        "quats": (-1.0, 1.0),
        "opacities": (-15.0, 15.0),
        "sh0": (-2.0, 4.0),
        "shN": None,
    }


@dataclass
class AttributeQuantizerConfig:
    """Quantization controls for a single attribute."""

    enabled: bool = True
    bitwidth: Optional[int] = None
    clamp_range: Optional[Tuple[float, float]] = None
    warmup_steps: Optional[int] = 10_000
    warmup_bitwidth: Optional[int] = 8
    mode: str = "noise"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "enabled": self.enabled,
            "bitwidth": self.bitwidth,
            "clamp_range": list(self.clamp_range) if self.clamp_range is not None else None,
            "warmup_steps": self.warmup_steps,
            "warmup_bitwidth": self.warmup_bitwidth,
            "mode": self.mode,
        }


def _default_attribute_configs() -> Dict[str, AttributeQuantizerConfig]:
    bitwidths = _default_bitwidths()
    clamps = _default_clamp_ranges()
    configs: Dict[str, AttributeQuantizerConfig] = {}
    for name in ATTRIBUTE_NAMES:
        configs[name] = AttributeQuantizerConfig(
            enabled=name != "means",
            bitwidth=bitwidths[name],
            clamp_range=clamps[name],
            warmup_steps=10_000 if name in {"scales", "quats", "sh0"} else None,
        )
    return configs


@dataclass
class QuantizerConfig:
    """Configuration for differentiable quantization."""

    attributes: Dict[str, AttributeQuantizerConfig] = field(
        default_factory=_default_attribute_configs
    )

    def copy_for_attribute_overrides(
        self, overrides: Optional[Mapping[str, Mapping[str, Any]]]
    ) -> "QuantizerConfig":
        if not overrides:
            return self
        updated = {
            name: AttributeQuantizerConfig(
                enabled=config.enabled,
                bitwidth=config.bitwidth,
                clamp_range=config.clamp_range,
                warmup_steps=config.warmup_steps,
                warmup_bitwidth=config.warmup_bitwidth,
                mode=config.mode,
            )
            for name, config in self.attributes.items()
        }
        for name, cfg in overrides.items():
            if name not in updated:
                continue
            attr_cfg = updated[name]
            for key, value in cfg.items():
                if not hasattr(attr_cfg, key):
                    raise KeyError(f"Unknown quantizer setting '{name}.{key}'")
                setattr(attr_cfg, key, value)
        return QuantizerConfig(attributes=updated)


@dataclass
class EntropyConfig:
    """Configuration for entropy constraint stage."""

    enabled: bool = False
    model_type: Literal["factorized_model", "gaussian_model"] = "factorized_model"
    steps: Dict[str, int] = field(default_factory=_default_entropy_steps)
    factorized_lr: float = 1e-4
    gaussian_lr: float = 5e-3
    scheduler_gamma: float = 0.01
    rd_lambda: float = 0.01  # Rate-distortion trade-off parameter

    def ensure_all_attributes(self) -> None:
        for name in ATTRIBUTE_NAMES:
            if name not in self.steps:
                self.steps[name] = -1


@dataclass
class LearnableMaskSettings:
    start_temp: float = 5.0
    end_temp: float = 0.1
    total_iters: int = 30_000
    target_sparsity: float = 0.2
    lr: float = 1e-2


@dataclass
class GradientMaskSettings:
    grad_threshold: float = 2e-3


@dataclass
class MaskConfig:
    """Configuration for adaptive mask stage."""

    enabled: bool = False
    strategy: Optional[str] = "learnable"
    start_step: int = 10_000
    regularization_weight: float = 1.0
    cap_max: Optional[int] = None
    learnable: LearnableMaskSettings = field(default_factory=LearnableMaskSettings)
    gradient: GradientMaskSettings = field(default_factory=GradientMaskSettings)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "enabled": self.enabled,
            "strategy": self.strategy,
            "start_step": self.start_step,
            "regularization_weight": self.regularization_weight,
            "cap_max": self.cap_max,
            "learnable": {
                "start_temp": self.learnable.start_temp,
                "end_temp": self.learnable.end_temp,
                "total_iters": self.learnable.total_iters,
                "target_sparsity": self.learnable.target_sparsity,
                "lr": self.learnable.lr,
            },
            "gradient": {
                "grad_threshold": self.gradient.grad_threshold,
            },
        }


@dataclass
class CompSimConfig:
    """Top-level configuration for compression simulation."""

    enabled: bool = False
    quantizer: QuantizerConfig = field(default_factory=QuantizerConfig)
    entropy: EntropyConfig = field(default_factory=EntropyConfig)
    mask: MaskConfig = field(default_factory=MaskConfig)

    @classmethod
    def from_trainer_config(cls, cfg: Any) -> "CompSimConfig":
        # New approach: directly use compression_sim_cfg if available
        if hasattr(cfg, "compression_sim_cfg"):
            return cfg.compression_sim_cfg
        
        # Legacy support for old field names (for backward compatibility with tests)
        enabled = bool(getattr(cfg, "compression_sim", False))

        entropy_cfg = EntropyConfig(
            enabled=bool(getattr(cfg, "entropy_model_opt", False)),
            model_type=getattr(cfg, "entropy_model_type", "factorized_model"),
            steps=dict(getattr(cfg, "entropy_steps", _default_entropy_steps())),
            rd_lambda=getattr(cfg, "rd_lambda", 0.01),
        )
        entropy_cfg.ensure_all_attributes()

        mask_cfg = MaskConfig(
            enabled=bool(getattr(cfg, "shN_ada_mask_opt", False)),
            strategy=getattr(cfg, "shN_ada_mask_strategy", "learnable"),
            start_step=getattr(cfg, "ada_mask_steps", 10_000),
            cap_max=getattr(getattr(cfg, "strategy", None), "cap_max", None),
        )
        grad_threshold = getattr(cfg, "shN_ada_mask_grad_threshold", None)
        if grad_threshold is not None:
            mask_cfg.gradient.grad_threshold = float(grad_threshold)

        quantizer_cfg = QuantizerConfig()

        return cls(
            enabled=enabled,
            quantizer=quantizer_cfg,
            entropy=entropy_cfg,
            mask=mask_cfg,
        )

    def to_dict(self) -> Dict[str, Any]:
        return {
            "enabled": self.enabled,
            "quantizer": {
                name: cfg.to_dict() for name, cfg in self.quantizer.attributes.items()
            },
            "entropy": {
                "enabled": self.entropy.enabled,
                "model_type": self.entropy.model_type,
                "steps": dict(self.entropy.steps),
                "factorized_lr": self.entropy.factorized_lr,
                "gaussian_lr": self.entropy.gaussian_lr,
                "scheduler_gamma": self.entropy.scheduler_gamma,
                "rd_lambda": self.entropy.rd_lambda,
            },
            "mask": self.mask.to_dict(),
        }
