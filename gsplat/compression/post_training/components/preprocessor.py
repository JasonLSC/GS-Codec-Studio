from __future__ import annotations

import torch.nn.functional as F

from ..configs import PreprocessConfig
from ..io import TensorDict
from gsplat.utils import log_transform, inverse_log_transform


class PrePostProcessor:
    def __init__(self, config: PreprocessConfig | None = None) -> None:
        self.config = config or PreprocessConfig()

    def pre_quantization_transform(self, splats: TensorDict) -> TensorDict:
        if not self.config.enabled:
            return {k: v.clone() for k, v in splats.items()}
        transformed = {k: v.clone() for k, v in splats.items()}
        if self.config.apply_means_log_transform and "means" in transformed:
            transformed["means"] = log_transform(transformed["means"])
        if self.config.apply_quat_normalize and "quats" in transformed:
            transformed["quats"] = F.normalize(transformed["quats"], dim=-1)
        return transformed

    def post_quantization_inverse_transform(self, splats: TensorDict) -> TensorDict:
        if not self.config.enabled:
            return {k: v.clone() for k, v in splats.items()}
        restored = {k: v.clone() for k, v in splats.items()}
        if self.config.apply_means_log_transform and "means" in restored:
            restored["means"] = inverse_log_transform(restored["means"])
        # if self.config.apply_quat_normalize and "quats" in restored:
        #     restored["quats"] = F.normalize(restored["quats"], dim=-1)
        return restored


