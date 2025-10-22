from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, Mapping, MutableMapping, Optional

import torch
from torch import Tensor
from torch.nn import ParameterDict

from .config import CompSimConfig
from .mask import AdaptiveMaskFactory
from .quantizer import build_quantizer
from .entropy import EntropyConstraint


def _as_tensor_dict(data: Mapping[str, Tensor]) -> Dict[str, Tensor]:
    return {key: value for key, value in data.items()}


def _maybe_to_tensor_dict(
    splats: Mapping[str, Tensor] | ParameterDict,
) -> Dict[str, Tensor]:
    if isinstance(splats, ParameterDict):
        return {key: param for key, param in splats.items()}
    return _as_tensor_dict(splats)


@dataclass
class SimulationResult:
    splats: Dict[str, Tensor]
    loss_terms: Dict[str, Tensor] = field(default_factory=dict)
    metrics: Dict[str, Any] = field(default_factory=dict)


class CompressionSimulationBase:
    """Common interface for compression simulation modules."""

    def __init__(self, config: CompSimConfig, device: Optional[torch.device] = None):
        self.config = config
        self.device = device

    def run(
        self, splats: Mapping[str, Tensor] | ParameterDict, step: int
    ) -> SimulationResult:
        raise NotImplementedError

    def step_optimizers(self, step: int) -> None:
        """Hook for entropy/mask optimizers to perform a step."""
        pass

    def state_dict(self) -> Dict[str, Any]:
        return {}

    def load_state_dict(self, state: MutableMapping[str, Any]) -> None:
        if state:
            raise ValueError("CompressionSimulationBase.load_state_dict received unexpected state")

    def get_mask_binary(self) -> Optional[Tensor]:
        return None

    def get_entropy_models(self) -> Dict[str, Any]:
        return {}


class NullCompressionSimulation(CompressionSimulationBase):
    """No-op simulation used when compression simulation is disabled."""

    def __init__(self, device: Optional[torch.device] = None):
        super().__init__(CompSimConfig(enabled=False), device=device)

    def run(
        self, splats: Mapping[str, Tensor] | ParameterDict, step: int
    ) -> SimulationResult:
        tensor_dict = _maybe_to_tensor_dict(splats)
        return SimulationResult(splats=dict(tensor_dict))

    def step_optimizers(self, step: int) -> None:
        return

    def state_dict(self) -> Dict[str, Any]:
        return {}

    def load_state_dict(self, state: MutableMapping[str, Any]) -> None:
        if state:
            raise ValueError("NullCompressionSimulation received unexpected state during load")


class DefaultCompressionSimulation(CompressionSimulationBase):
    """Compression simulation built on top of the refactored runtime components."""

    def __init__(self, config: CompSimConfig, device: Optional[torch.device] = None):
        super().__init__(config, device=device)
        entropy_device = device if device is not None else torch.device("cpu")
        self._entropy = EntropyConstraint(config.entropy, entropy_device)
        self._mask = AdaptiveMaskFactory.create(config.mask, device)
        self._quantizers: Dict[str, Any] = {}
        for name, attr_cfg in config.quantizer.attributes.items():
            quantizer = build_quantizer(name, attr_cfg)
            if quantizer is not None:
                self._quantizers[name] = quantizer

    def run(
        self, splats: Mapping[str, Tensor] | ParameterDict, step: int
    ) -> SimulationResult:
        tensor_dict = _maybe_to_tensor_dict(splats)
        self._entropy.maybe_update(step, tensor_dict)
        self._mask.maybe_update(step, tensor_dict)

        result_splats: Dict[str, Tensor] = {}
        loss_terms: Dict[str, Tensor] = {}
        metrics: Dict[str, Any] = {}
        entropy_bits: Dict[str, Tensor] = {}

        means_tensor = tensor_dict.get("means")

        for name, tensor in tensor_dict.items():
            quantizer = self._quantizers.get(name)
            q_step = None
            value = tensor
            if quantizer is not None:
                quant_result = quantizer.quantize(tensor, step)
                value = quant_result.value
                q_step = quant_result.q_step
                bitwidth = quant_result.metadata.get("bitwidth") if quant_result.metadata else None
                if bitwidth is not None:
                    metrics[f"quantizer/{name}_bitwidth"] = float(bitwidth.item())
            result_splats[name] = value

            entropy_tensor = value
            transform_bits = lambda x: x
            if name == "sh0":
                # Entropy model expects sh0 in shape [N, 3]; squeeze the singleton SH dim and restore afterward.
                entropy_tensor = value.squeeze(1)
                transform_bits = lambda x: x.unsqueeze(1) if x is not None else None
            elif name == "opacities":
                # Legacy entropy model operates on column vectors; temporarily add the feature dim then drop it again.
                entropy_tensor = value.unsqueeze(1)
                transform_bits = lambda x: x.squeeze(1) if x is not None else None

            entropy_result = self._entropy.evaluate(
                attr=name,
                tensor=entropy_tensor,
                q_step=q_step,
                step=step,
                meta={"means": means_tensor} if means_tensor is not None else None,
            )
            if entropy_result.bits is not None:
                entropy_bits[name] = transform_bits(entropy_result.bits)
            if entropy_result.loss is not None:
                loss_terms[f"entropy/{name}"] = entropy_result.loss
            if entropy_result.metrics:
                metrics.update(entropy_result.metrics)

        shn = result_splats.get("shN")
        if shn is not None:
            mask_result = self._mask.apply(shn, step)
            result_splats["shN"] = mask_result.value
            if mask_result.loss is not None:
                mask_loss = loss_terms.get("mask")
                loss_terms["mask"] = mask_result.loss if mask_loss is None else mask_loss + mask_result.loss
            if mask_result.metrics:
                metrics.update(mask_result.metrics)

        if entropy_bits:
            metrics["entropy_bits"] = entropy_bits

        return SimulationResult(splats=result_splats, loss_terms=loss_terms, metrics=metrics)

    def step_optimizers(self, step: int) -> None:
        if not self.config.enabled:
            return
        self._entropy.step_optimizers(step)
        self._mask.step_optimizer(step)

    def state_dict(self) -> Dict[str, Any]:
        state: Dict[str, Any] = {}
        entropy_state = self._entropy.state_dict()
        if entropy_state:
            state["entropy"] = entropy_state
        mask_state = self._mask.state_dict()
        if mask_state:
            state["mask"] = mask_state
        return state

    def load_state_dict(self, state: MutableMapping[str, Any]) -> None:
        if not state:
            return
        entropy_state = state.get("entropy")
        if entropy_state:
            self._entropy.load_state_dict(entropy_state)
        mask_state = state.get("mask")
        if mask_state:
            self._mask.load_state_dict(mask_state)

    def get_mask_binary(self) -> Optional[Tensor]:
        return self._mask.get_binary_mask()

    def get_entropy_models(self) -> Dict[str, Any]:
        return dict(self._entropy.models)


class LegacyCompressionSimulationAdapter(CompressionSimulationBase):
    """Adapter that wraps the legacy CompressionSimulation implementation."""

    def __init__(self, legacy_obj, config: CompSimConfig, device: Optional[torch.device] = None):
        super().__init__(config, device=device)
        self._legacy = legacy_obj
        self._mask = AdaptiveMaskFactory.create(config.mask, device)
        if hasattr(self._legacy, 'shN_ada_mask_opt'):
            self._legacy.shN_ada_mask_opt = False

    def run(self, splats: Mapping[str, Tensor] | ParameterDict, step: int) -> SimulationResult:
        self._mask.maybe_update(step, splats)
        quantized_splats, metrics = self._legacy.simulate_compression(splats, step)
        result = SimulationResult(splats={k: v for k, v in quantized_splats.items()})
        loss_terms: Dict[str, Tensor] = {}
        metrics_map: Dict[str, Any] = {}
        if isinstance(metrics, Mapping):
            metrics_map['entropy_bits'] = metrics
        shn = result.splats.get('shN')
        if shn is not None:
            mask_result = self._mask.apply(shn, step)
            result.splats['shN'] = mask_result.value
            if mask_result.loss is not None:
                loss_terms['mask'] = mask_result.loss
            metrics_map.update(mask_result.metrics)
        if loss_terms:
            result.loss_terms.update(loss_terms)
        if metrics_map:
            result.metrics.update(metrics_map)
        return result

    def step_optimizers(self, step: int) -> None:
        if not self.config.enabled:
            return

        if self.config.entropy.enabled:
            optimizers = getattr(self._legacy, 'entropy_model_optimizers', None)
            if optimizers:
                for optimizer in optimizers.values():
                    if optimizer is not None:
                        optimizer.step()
                        optimizer.zero_grad(set_to_none=True)
            schedulers = getattr(self._legacy, 'entropy_model_schedulers', None)
            if schedulers:
                for name, scheduler in schedulers.items():
                    if scheduler is not None and step > self.config.entropy.steps.get(name, -1):
                        scheduler.step()

        self._mask.step_optimizer(step)

    def state_dict(self) -> Dict[str, Any]:
        state: Dict[str, Any] = {}
        models = getattr(self._legacy, 'entropy_models', None)
        if models:
            state['entropy'] = {k: v.state_dict() for k, v in models.items() if hasattr(v, 'state_dict')}
        mask_state = self._mask.state_dict()
        if mask_state:
            state['mask'] = mask_state
        return state

    def load_state_dict(self, state: MutableMapping[str, Any]) -> None:
        if not state:
            return
        entropy_state = state.get('entropy')
        models = getattr(self._legacy, 'entropy_models', None)
        if entropy_state and models:
            for key, model in models.items():
                if model is not None and key in entropy_state and hasattr(model, 'load_state_dict'):
                    model.load_state_dict(entropy_state[key])
        mask_state = state.get('mask')
        if mask_state is not None:
            self._mask.load_state_dict(mask_state)

    def get_mask_binary(self) -> Optional[Tensor]:
        return self._mask.get_binary_mask()

    def get_entropy_models(self) -> Dict[str, Any]:
        models = getattr(self._legacy, "entropy_models", None)
        if models is None:
            return {}
        return {k: v for k, v in models.items() if v is not None}
