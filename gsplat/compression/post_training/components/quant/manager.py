from __future__ import annotations

from typing import Dict, Tuple

from ...configs import QuantConfig, QuantFieldConfig
from ...io import TensorDict
from .types import QuantizationContext
from . import get_registry


class QuantManager:
    def __init__(self, config: QuantConfig) -> None:
        self.config = config
        # Build method -> singleton instance from registry
        self._by_method: Dict[str, object] = {name: cls() for name, cls in get_registry().items()}
        if "scalar" not in self._by_method:
            raise RuntimeError("No 'scalar' quantizer registered.")

        # Default quantizer comes from default.method
        default_method = getattr(self.config.default, "method", "scalar")
        self._default_quant = self._by_method.get(default_method, self._by_method["scalar"])

        # Pre-bind field -> instance for explicitly configured fields
        self._by_field: Dict[str, object] = {}
        for name, fc in (self.config.fields or {}).items():
            cfg: QuantFieldConfig = fc if isinstance(fc, QuantFieldConfig) else self.config.field_config(name)
            if not cfg.enabled:
                continue
            self._by_field[name] = self._by_method.get(cfg.method, self._by_method["scalar"])

    def quantize_all(self, splats: TensorDict) -> Tuple[TensorDict, QuantizationContext]:
        if not self.config.enabled:
            return {k: v.clone() for k, v in splats.items()}, QuantizationContext()

        quantized: TensorDict = {k: v.clone() for k, v in splats.items()}
        context = QuantizationContext()
        for name, tensor in splats.items():
            field_cfg: QuantFieldConfig = self.config.field_config(name)
            if not field_cfg.enabled:
                continue
            quantizer = self._by_field.get(name, self._default_quant)
            q_tensor, stats, int_tensor, codebook_tensor = quantizer.quantize_field(tensor, field_cfg)
            quantized[name] = q_tensor
            context.register(name, stats, int_tensor, field_cfg, codebook_tensor)
        return quantized, context

    def dequantize_all(self, splats: TensorDict, context: QuantizationContext) -> TensorDict:
        if not context.field_stats:
            return {k: v.clone() for k, v in splats.items()}
        restored: TensorDict = {k: v.clone() for k, v in splats.items()}
        for name, stats in context.field_stats.items():
            if name not in restored:
                continue
            quantizer = self._by_method.get(getattr(stats, "method", "scalar"), self._by_method["scalar"])
            restored[name] = quantizer.dequantize_field(restored[name], stats)
        return restored


