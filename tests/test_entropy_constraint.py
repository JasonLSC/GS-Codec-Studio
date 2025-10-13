import pytest
import torch

from gsplat.compression_simulation.config import EntropyConfig
from gsplat.compression_simulation.entropy import EntropyConstraint


def _build_config(model_type: str) -> EntropyConfig:
    cfg = EntropyConfig()
    cfg.enabled = True
    cfg.model_type = model_type
    cfg.steps = {"scales": 0, "quats": -1, "opacities": -1, "sh0": -1, "means": -1, "shN": -1}
    return cfg


def test_entropy_constraint_factorized_forward():
    device = torch.device("cuda")
    config = _build_config("factorized_model")
    constraint = EntropyConstraint(config, device=device)

    tensor = torch.randn(16, 3, device=device)
    q_step = torch.tensor(0.05, device=device)
    result = constraint.evaluate("scales", tensor, q_step, step=1)

    assert result.loss is not None
    assert result.bits is not None
    assert result.bits.shape[0] == tensor.shape[0]


def test_entropy_constraint_gaussian_forward():
    if not torch.cuda.is_available():
        pytest.skip("gaussian entropy requires CUDA")
    device = torch.device("cuda")
    config = _build_config("gaussian_model")
    constraint = EntropyConstraint(config, device=device)

    tensor = torch.randn(32, 3, device=device)
    q_step = torch.tensor(0.1, device=device)
    means = torch.randn(32, 3, device=device)

    result = constraint.evaluate(
        "scales",
        tensor,
        q_step,
        step=1,
        meta={"means": means},
    )

    assert result.loss is not None
    assert result.bits is not None
    assert result.bits.shape[0] == tensor.shape[0]
