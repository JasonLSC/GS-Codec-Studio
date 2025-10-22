from __future__ import annotations

# Trigger registration of built-in quantizers via side-effect imports.
from . import scalar as _scalar  # noqa: F401
from . import vector as _vector  # noqa: F401

# Re-export registry APIs for consumers.
from .registry import get_registry, register_quantizer  # noqa: F401


