"""Reusable configuration utilities for experiment entry points."""

from __future__ import annotations

import copy
import sys
from collections.abc import Mapping, MutableMapping, Sequence
from dataclasses import fields, is_dataclass
from inspect import isclass
from pathlib import Path
from typing import Any, Callable, Dict, Iterable, Optional, Tuple

import yaml

# Flag naming conventions shared across entry points.
CONFIG_FILE_FLAGS: Tuple[str, ...] = ("--config", "--config-path", "-c")
CONFIG_SAVE_FLAGS: Tuple[str, ...] = ("--save-config", "--config-save")
DEFAULT_CONFIG_SNAPSHOT = "config_snapshot.yaml"

# Type aliases for clarity.
Registry = Mapping[str, Callable[..., Any]]
ApplyFn = Callable[[Any, Dict[str, Any], str], None]
RegistryHandler = Callable[[Any, Optional[Any], str, ApplyFn], Any]
Serializer = Callable[[Any, Callable[[Any], Any]], Optional[Any]]


def pop_flag_value(flag_names: Sequence[str], argv: Optional[Sequence[str]] = None) -> Optional[str]:
    """Remove and return the value for the first matching flag in ``argv``."""

    argv_list = sys.argv if argv is None else list(argv)
    i = 1
    while i < len(argv_list):
        arg = argv_list[i]
        for flag in flag_names:
            if arg == flag:
                if i + 1 >= len(argv_list):
                    raise ValueError(f"Flag {flag} requires a value.")
                value = argv_list[i + 1]
                del argv_list[i : i + 2]
                if argv is None:
                    sys.argv[:] = argv_list
                return value
            if arg.startswith(f"{flag}="):
                value = arg.split("=", 1)[1]
                del argv_list[i]
                if argv is None:
                    sys.argv[:] = argv_list
                return value
        i += 1
    if argv is None:
        sys.argv[:] = argv_list
    return None


def load_config_updates(config_path: Path) -> Dict[str, Any]:
    """Load overrides from a YAML file."""

    with config_path.open("r") as f:
        data = yaml.safe_load(f) or {}
    if not isinstance(data, MutableMapping):
        raise ValueError(f"Config file {config_path} must contain a mapping at the top level.")
    return dict(data)


def coerce_value(reference: Any, value: Any) -> Any:
    """Best-effort coercion to keep tuple/list shapes consistent."""

    if isinstance(reference, tuple) and isinstance(value, list):
        return type(reference)(value)
    if isinstance(reference, tuple) and isinstance(value, tuple):
        return type(reference)(value)
    if isinstance(reference, list) and isinstance(value, tuple):
        return list(value)
    if isinstance(reference, list) and not isinstance(value, (list, tuple)):
        return [value]
    return value


def apply_updates(
    target: Any,
    updates: Dict[str, Any],
    path: str = "cfg",
    registry_handlers: Optional[Dict[str, RegistryHandler]] = None,
) -> None:
    """Recursively apply ``updates`` onto ``target``."""

    if not isinstance(updates, MutableMapping):
        raise ValueError(f"Expected mapping for updates at {path}, got {type(updates).__name__}")

    registry_handlers = registry_handlers or {}

    def _apply_nested(nested_target: Any, nested_updates: Dict[str, Any], nested_path: str) -> None:
        apply_updates(nested_target, nested_updates, nested_path, registry_handlers)

    if is_dataclass(target):
        field_lookup = {f.name: f for f in fields(target)}
        for key, value in updates.items():
            if key == "type":
                continue
            if key not in field_lookup:
                raise KeyError(f"Unknown configuration key '{path}.{key}'")

            current_value = getattr(target, key)
            next_path = f"{path}.{key}"

            if key in registry_handlers:
                handler = registry_handlers[key]
                new_value = handler(value, current_value, next_path, _apply_nested)
                setattr(target, key, new_value)
                continue

            if is_dataclass(current_value) and isinstance(value, MutableMapping):
                _apply_nested(current_value, dict(value), next_path)
                continue

            if isinstance(current_value, MutableMapping) and isinstance(value, MutableMapping):
                merged = copy.deepcopy(current_value)
                for sub_key, sub_value in value.items():
                    if (
                        sub_key in merged
                        and is_dataclass(merged[sub_key])
                        and isinstance(sub_value, MutableMapping)
                    ):
                        apply_updates(
                            merged[sub_key],
                            dict(sub_value),
                            f"{next_path}.{sub_key}",
                            registry_handlers,
                        )
                    else:
                        merged[sub_key] = sub_value
                setattr(target, key, merged)
                continue

            setattr(target, key, coerce_value(current_value, value))
        return

    if isinstance(target, MutableMapping):
        for key, value in updates.items():
            target[key] = value
        return

    raise ValueError(f"Unsupported target type '{type(target).__name__}' at {path}")


def build_from_registry(
    value: Any,
    registry: Registry,
    current: Optional[Any] = None,
    *,
    apply_fn: Optional[ApplyFn] = None,
    path: str = "cfg",
) -> Any:
    """Instantiate or update an object using a registry mapping."""

    if any(isinstance(value, cls) for cls in registry.values() if isclass(cls)):
        return value

    if isinstance(value, str):
        if value not in registry:
            raise ValueError(f"Unsupported registry type '{value}' at {path}")
        factory = registry[value]
        return factory()

    if isinstance(value, MutableMapping):
        params = dict(value)
        type_name = params.pop("type", None)
        params_dict = params.pop("params", None)
        if params and params_dict is not None:
            params_dict.update(params)
        args = params_dict if params_dict is not None else params

        if type_name is None:
            if current is None:
                raise ValueError(f"Registry entry type must be specified at {path}")
            if args and apply_fn is not None:
                apply_fn(current, args, path)
            return current

        if type_name not in registry:
            raise ValueError(f"Unsupported registry type '{type_name}' at {path}")
        factory = registry[type_name]

        expected_type = factory if isclass(factory) else None
        if current is not None and expected_type is not None and isinstance(current, expected_type):
            if args and apply_fn is not None:
                apply_fn(current, args, f"{path}.{type_name}")
            return current

        if args:
            return factory(**args)
        return factory()

    raise ValueError(f"Unsupported registry specification at {path}: {value!r}")


def prepare_presets(
    presets: Mapping[str, Tuple[str, Any]],
    updates: Optional[Dict[str, Any]],
    *,
    registry_handlers: Optional[Dict[str, RegistryHandler]] = None,
) -> Dict[str, Tuple[str, Any]]:
    """Return deep-copied presets with overrides applied."""

    prepared: Dict[str, Tuple[str, Any]] = {}
    for name, (description, cfg) in presets.items():
        cfg_copy = copy.deepcopy(cfg)
        if updates:
            apply_updates(cfg_copy, dict(updates), registry_handlers=registry_handlers)
        prepared[name] = (description, cfg_copy)
    return prepared


def serialize_config_value(
    value: Any,
    *,
    custom_serializers: Optional[Iterable[Serializer]] = None,
) -> Any:
    """Recursively convert configuration values into YAML-friendly objects."""

    # 添加内置序列化器
    def path_serializer(obj: Any, _serialize: Callable) -> Any:
        from pathlib import Path
        if isinstance(obj, Path):
            return str(obj)
        return None
    
    def torch_dtype_serializer(obj: Any, _serialize: Callable) -> Any:
        try:
            import torch
            if isinstance(obj, torch.dtype):
                return str(obj)
        except ImportError:
            pass
        return None

    # 将内置序列化器添加到自定义序列化器列表前面
    serializers = [
        path_serializer,
        torch_dtype_serializer,
        *(custom_serializers or [])
    ]

    def _serialize(obj: Any) -> Any:
        for serializer in serializers:
            result = serializer(obj, _serialize)
            if result is not None:
                return result

        if is_dataclass(obj):
            return {f.name: _serialize(getattr(obj, f.name)) for f in fields(obj)}
        if isinstance(obj, Mapping):
            return {k: _serialize(v) for k, v in obj.items()}
        if isinstance(obj, (list, tuple)):
            return [_serialize(v) for v in obj]
        return obj

    return _serialize(value)


def save_config_snapshot(
    cfg: Any,
    destination: Path,
    *,
    custom_serializers: Optional[Iterable[Serializer]] = None,
) -> None:
    """Persist a configuration dataclass to ``destination`` as YAML."""

    destination.parent.mkdir(parents=True, exist_ok=True)
    serializable = {
        f.name: serialize_config_value(getattr(cfg, f.name), custom_serializers=custom_serializers)
        for f in fields(cfg)
    }
    with destination.open("w") as f:
        yaml.safe_dump(serializable, f, sort_keys=False)


def synchronize_compression_config(cfg: Any) -> None:
    """Keep compression-related configuration flags in sync."""

    comp_cfg = cfg.compression_sim_cfg

    if getattr(cfg, "compression_sim", False):
        comp_cfg.enabled = True
    else:
        cfg.compression_sim = comp_cfg.enabled

    if getattr(cfg, "entropy_model_opt", False):
        comp_cfg.entropy.enabled = True
    else:
        cfg.entropy_model_opt = comp_cfg.entropy.enabled

    comp_cfg.entropy.model_type = getattr(cfg, "entropy_model_type", None) or comp_cfg.entropy.model_type
    cfg.entropy_model_type = comp_cfg.entropy.model_type

    entropy_steps = getattr(cfg, "entropy_steps", None)
    if entropy_steps:
        comp_cfg.entropy.steps.update(entropy_steps)
    cfg.entropy_steps = comp_cfg.entropy.steps

    if getattr(cfg, "shN_ada_mask_opt", False):
        comp_cfg.mask.enabled = True
    else:
        cfg.shN_ada_mask_opt = comp_cfg.mask.enabled

    strategy = getattr(cfg, "shN_ada_mask_strategy", None)
    if strategy is not None:
        comp_cfg.mask.strategy = strategy
    elif comp_cfg.mask.strategy is not None:
        cfg.shN_ada_mask_strategy = comp_cfg.mask.strategy

    mask_steps = getattr(cfg, "ada_mask_steps", None)
    if mask_steps is not None:
        comp_cfg.mask.start_step = mask_steps
    elif comp_cfg.mask.start_step is not None:
        cfg.ada_mask_steps = comp_cfg.mask.start_step

    cfg.compression_sim_cfg = comp_cfg
