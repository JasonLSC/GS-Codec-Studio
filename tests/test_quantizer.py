import torch

from gsplat.compression_simulation.config import AttributeQuantizerConfig
from gsplat.compression_simulation.quantizer import build_quantizer, DifferentiableQuantizer


def test_quantizer_disabled_passthrough():
    cfg = AttributeQuantizerConfig(enabled=False)
    quantizer = build_quantizer("means", cfg)
    assert quantizer is None

    cfg2 = AttributeQuantizerConfig(enabled=True, bitwidth=None)
    quantizer2 = build_quantizer("means", cfg2)
    assert quantizer2 is None


def test_noise_quantization_outputs_q_step_and_stats():
    cfg = AttributeQuantizerConfig(
        enabled=True,
        bitwidth=6,
        clamp_range=(-1.0, 1.0),
        warmup_steps=None,
        mode="noise",
    )
    quantizer = DifferentiableQuantizer("scales", cfg)

    x = torch.linspace(-2, 2, steps=5, requires_grad=True)
    result = quantizer.quantize(x, step=100)

    assert torch.is_tensor(result.q_step)
    expected_q_step = torch.tensor((1.0 - (-1.0)) / (2 ** 6 - 1), dtype=x.dtype)
    assert torch.allclose(result.q_step.cpu(), expected_q_step)
    assert "bitwidth" in result.metadata
    assert result.metadata["bitwidth"].item() == 6
    assert torch.all(result.value <= 1.0) and torch.all(result.value >= -1.0)

    # Gradient should pass through (noise path uses straight-through)
    loss = result.value.sum()
    loss.backward()
    assert x.grad is not None


def test_round_quantization_matches_levels():
    cfg = AttributeQuantizerConfig(
        enabled=True,
        bitwidth=4,
        clamp_range=(0.0, 1.0),
        warmup_steps=None,
        mode="round",
    )
    quantizer = DifferentiableQuantizer("opacities", cfg)

    x = torch.tensor([0.0, 0.1, 0.5, 0.9, 1.0], dtype=torch.float32, requires_grad=True)
    result = quantizer.quantize(x, step=0)

    q_step = (1.0 - 0.0) / (2 ** 4 - 1)
    expected = torch.round(x / q_step) * q_step
    assert torch.allclose(result.value.detach(), expected)
    assert torch.allclose(result.q_step.cpu(), torch.tensor(q_step))

    grad = torch.autograd.grad(result.value.sum(), x, retain_graph=False)[0]
    assert torch.allclose(grad, torch.ones_like(grad))


def test_warmup_bitwidth_used_before_threshold():
    cfg = AttributeQuantizerConfig(
        enabled=True,
        bitwidth=6,
        clamp_range=(-2.0, 2.0),
        warmup_steps=5,
        warmup_bitwidth=4,
        mode="round",
    )
    quantizer = DifferentiableQuantizer("sh0", cfg)

    x = torch.zeros(3)
    result_pre = quantizer.quantize(x, step=0)
    pre_step = (2.0 - (-2.0)) / (2 ** 4 - 1)
    assert torch.allclose(result_pre.q_step.cpu(), torch.tensor(pre_step))

    result_post = quantizer.quantize(x, step=10)
    post_step = (2.0 - (-2.0)) / (2 ** 6 - 1)
    assert torch.allclose(result_post.q_step.cpu(), torch.tensor(post_step))
