from __future__ import annotations

from typing import Tuple

import torch

from ..configs import PruningConfig
from ..io import TensorDict
from ..stages import PruningContext, PruningStepRecord
from gsplat.compression.outlier_filter import filter_splats


class Pruner:
    def __init__(self, config: PruningConfig) -> None:
        self.config = config

    def prune(self, splats: TensorDict) -> Tuple[TensorDict, PruningContext]:
        if not self.config.enabled:
            cloned = {k: v.clone() for k, v in splats.items()}
            total = len(cloned["opacities"]) if cloned else 0
            return cloned, PruningContext(enabled=False, kept=total, removed=0)

        total_before = len(splats["opacities"]) if splats else 0
        steps: list[PruningStepRecord] = []

        current = {k: v.clone() for k, v in splats.items()}

        if self.config.use_outlier_filter:
            before = len(current["opacities"]) if current else 0
            mask, _ = filter_splats(current)
            kept_now = len(current["opacities"]) if current else 0
            steps.append(
                PruningStepRecord(
                    name="outlier_filter",
                    removed=before - kept_now,
                    kept=kept_now,
                    metadata={"kept_ratio": float(kept_now / before) if before > 0 else 0.0},
                )
            )

        if self.config.opacity_threshold is not None:
            before = len(current["opacities"]) if current else 0
            values = current["opacities"]
            if self.config.opacity_activation == "sigmoid":
                values = torch.sigmoid(values)
            mask = values >= self.config.opacity_threshold
            current = {k: v[mask] for k, v in current.items()}
            kept_now = len(current["opacities"]) if current else 0
            steps.append(
                PruningStepRecord(
                    name="opacity_threshold",
                    removed=before - kept_now,
                    kept=kept_now,
                    metadata={
                        "threshold": self.config.opacity_threshold,
                        "activation": self.config.opacity_activation,
                    },
                )
            )

        if self.config.max_points is not None and self.config.max_points > 0:
            before = len(current["opacities"]) if current else 0
            if before > self.config.max_points:
                opacities = current["opacities"]
                topk = torch.topk(opacities, self.config.max_points, sorted=False).indices
                current = {k: v[topk] for k, v in current.items()}
            kept_now = len(current["opacities"]) if current else 0
            steps.append(
                PruningStepRecord(
                    name="topk",
                    removed=before - kept_now,
                    kept=kept_now,
                    metadata={"max_points": self.config.max_points},
                )
            )

        if self.config.scale_keep_ratio is not None:
            ratio = float(max(min(self.config.scale_keep_ratio, 1.0), 0.0))
            before = len(current["opacities"]) if current else 0
            if 0.0 < ratio < 1.0 and before > 0:
                keep = max(1, int(before * ratio))
                scales = current.get("scales")
                if scales is None:
                    raise KeyError("Scale-based pruning requires 'scales' field in splats.")
                if self.config.scale_metric == "max":
                    metric = scales.abs().max(dim=-1).values
                elif self.config.scale_metric == "volume":
                    metric = scales.abs().prod(dim=-1)
                else:
                    raise ValueError(f"Unknown scale metric {self.config.scale_metric}")
                topk = torch.topk(metric, keep, sorted=False).indices
                current = {k: v[topk] for k, v in current.items()}
            kept_now = len(current["opacities"]) if current else 0
            steps.append(
                PruningStepRecord(
                    name="scale_ratio",
                    removed=before - kept_now,
                    kept=kept_now,
                    metadata={
                        "ratio": ratio,
                        "metric": self.config.scale_metric,
                    },
                )
            )

        for hook in self.config.custom_hooks:
            before = len(current["opacities"]) if current else 0
            result = hook.fn({k: v.clone() for k, v in current.items()})
            if not isinstance(result, dict) or "opacities" not in result:
                raise TypeError(f"Custom pruning hook '{hook.name}' must return a TensorDict with 'opacities'.")
            current = result
            kept_now = len(current["opacities"]) if current else 0
            steps.append(
                PruningStepRecord(
                    name=f"custom:{hook.name}",
                    removed=before - kept_now,
                    kept=kept_now,
                    metadata={"description": hook.description},
                )
            )

        kept = len(current["opacities"]) if current else 0
        removed = total_before - kept
        context = PruningContext(
            enabled=True,
            kept=kept,
            removed=removed,
            opacity_threshold=self.config.opacity_threshold,
            max_points=self.config.max_points,
            steps=steps,
        )
        return current, context


