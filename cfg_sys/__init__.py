"""Configuration system utilities for GSCodec Studio.

This package centralizes helpers that manage experiment configurations,
allowing multiple entry points to share a consistent set of behaviors for
parsing overrides, instantiating modules, and recording snapshots.
"""

from .config_manager import (
    CONFIG_FILE_FLAGS,
    CONFIG_SAVE_FLAGS,
    DEFAULT_CONFIG_SNAPSHOT,
    apply_updates,
    build_from_registry,
    coerce_value,
    load_config_updates,
    pop_flag_value,
    prepare_presets,
    save_config_snapshot,
    serialize_config_value,
    synchronize_compression_config,
)

__all__ = [
    "CONFIG_FILE_FLAGS",
    "CONFIG_SAVE_FLAGS",
    "DEFAULT_CONFIG_SNAPSHOT",
    "apply_updates",
    "build_from_registry",
    "coerce_value",
    "load_config_updates",
    "pop_flag_value",
    "prepare_presets",
    "save_config_snapshot",
    "serialize_config_value",
    "synchronize_compression_config",
]
