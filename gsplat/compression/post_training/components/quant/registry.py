from __future__ import annotations

from typing import Dict, Type

from .base import QuantizerBase


_REGISTRY: Dict[str, Type[QuantizerBase]] = {}


def register_quantizer(name: str):
    """Class decorator to register a quantizer implementation by name."""

    def deco(cls: Type[QuantizerBase]):
        _REGISTRY[name] = cls
        return cls

    return deco


def get_registry() -> Dict[str, Type[QuantizerBase]]:
    """Return a copy of the quantizer registry mapping name -> class."""

    return dict(_REGISTRY)


