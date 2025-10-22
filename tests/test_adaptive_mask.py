import torch

from gsplat.compression_simulation.config import MaskConfig
from gsplat.compression_simulation.mask import AdaptiveMaskFactory


def _make_splats(tensor: torch.Tensor):
    return {"shN": tensor}


def test_learnable_mask_applies_and_returns_loss():
    cfg = MaskConfig(enabled=True)
    cfg.start_step = 0
    cfg.learnable.total_iters = 10
    cfg.learnable.start_temp = 1.0
    cfg.learnable.end_temp = 0.5
    cfg.learnable.lr = 1e-2

    mask = AdaptiveMaskFactory.create(cfg, device=torch.device("cpu"))
    shn = torch.rand(4, 2, 3, requires_grad=True)

    mask.maybe_update(step=0, splats=_make_splats(shn))
    result = mask.apply(shn, step=1)

    assert result.value.shape == shn.shape
    assert result.loss is not None
    assert "mask_ratio" in result.metrics

    mask.step_optimizer(step=1)
    binary = mask.get_binary_mask()
    assert binary is not None
    assert binary.shape[0] >= shn.shape[0]


def test_gradient_mask_zeroes_small_gradients():
    cfg = MaskConfig(enabled=True, strategy="gradient")
    cfg.start_step = 0
    cfg.gradient.grad_threshold = 1e-2

    mask = AdaptiveMaskFactory.create(cfg, device=None)
    param = torch.nn.Parameter(
        torch.tensor(
            [
                [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0]],
                [[1.0, 1.0, 1.0], [1.0, 1.0, 1.0]],
            ],
            dtype=torch.float32,
        )
    )

    mask.maybe_update(step=1, splats=_make_splats(param))
    mask.apply(param, step=1)

    weights = torch.tensor(
        [
            [[5e-5, 5e-5, 5e-5], [5e-5, 5e-5, 5e-5]],
            [[1.0, 1.0, 1.0], [1.0, 1.0, 1.0]],
        ],
        dtype=torch.float32,
    )
    (param * weights).sum().backward()

    assert torch.allclose(param.grad[0], torch.zeros_like(param.grad[0]))
    assert torch.all(param.grad[1] != 0)

    binary = mask.get_binary_mask()
    assert binary is not None
    assert binary.shape[0] == param.shape[0]


def test_disabled_mask_passthrough():
    cfg = MaskConfig(enabled=False)
    mask = AdaptiveMaskFactory.create(cfg, device=None)

    shn = torch.randn(3, 2, 3)
    result = mask.apply(shn, step=100)
    assert torch.allclose(result.value, shn)
    assert result.loss is None
    assert result.metrics == {}
    assert mask.get_binary_mask() is None
