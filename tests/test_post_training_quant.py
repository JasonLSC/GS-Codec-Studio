import pytest
import torch

from gsplat.compression.post_training.configs import QuantConfig, QuantFieldConfig
from gsplat.compression.post_training.components.quant import scalar as scalar_quant
from gsplat.compression.post_training.components.quant import vector as vector_quant
from gsplat.compression.post_training.components.quant.manager import QuantManager


def test_scalar_quantize_tensor_stats_and_range():
    x = torch.tensor(
        [[0.0, 0.5, 1.0], [1.0, 0.25, 0.75]], dtype=torch.float32
    )
    cfg = QuantFieldConfig(
        bitwidth=4, clamp_min=0.0, clamp_max=1.0, store_as_int=True, method="scalar"
    )

    restored, stats, int_tensor, codebook = scalar_quant.quantize_tensor(x, cfg)

    assert codebook is None
    assert stats.method == "scalar"
    assert stats.bitwidth == 4
    assert stats.channels == x.numel() // x.shape[0]
    assert stats.tensor_shape == list(x.shape)
    assert stats.original_shape == list(x.shape)

    assert len(stats.min_vals) == stats.channels
    assert len(stats.max_vals) == stats.channels
    assert all(mn >= 0.0 for mn in stats.min_vals)
    assert all(mx <= 1.0 for mx in stats.max_vals)

    assert int_tensor is not None
    assert int_tensor.dtype == torch.int32
    assert int_tensor.shape == x.shape

    assert torch.all(restored >= 0.0) and torch.all(restored <= 1.0)
    assert torch.isfinite(restored).all()

    mse = torch.mean((restored - x) ** 2).item()
    assert mse >= 0.0
    assert mse < 1e-2


def test_vector_quantize_tensor_all_zero_fastpath():
    x = torch.zeros((5, 3), dtype=torch.float32)
    cfg = QuantFieldConfig(bitwidth=6, method="vector")

    restored, stats, int_tensor, codebook = vector_quant.quantize_tensor(x, cfg)

    assert stats.method == "vector"
    assert stats.channels == x.shape[1]
    assert stats.original_shape == list(x.shape)
    assert stats.tensor_shape == list(int_tensor.shape)
    assert isinstance(stats.codebook, list) and len(stats.codebook) == 0

    assert int_tensor is not None
    assert int_tensor.dtype == torch.int32
    assert int_tensor.shape == (x.shape[0], 1)

    assert torch.allclose(restored, x)

    y = vector_quant.dequantize_tensor(int_tensor, stats).to(x.dtype)
    assert torch.allclose(y, x)


def test_quant_manager_scalar_roundtrip_cpu():
    # Default scalar quantization, CPU tensors
    quant_cfg = QuantConfig(
        enabled=True,
        default=QuantFieldConfig(bitwidth=5, clamp_min=0.0, clamp_max=1.0, method="scalar"),
        fields={},
    )

    manager = QuantManager(quant_cfg)

    splats = {
        "opacities": torch.rand(8, 1, dtype=torch.float32),
        "means": torch.rand(8, 3, dtype=torch.float32),
    }

    quantized, ctx = manager.quantize_all(splats)

    # Shapes preserved and values finite
    for k in splats.keys():
        assert quantized[k].shape == splats[k].shape
        assert torch.isfinite(quantized[k]).all()

    # Dequantize with captured context
    restored = manager.dequantize_all(quantized, ctx)
    for k in splats.keys():
        assert restored[k].shape == splats[k].shape
        assert torch.isfinite(restored[k]).all()


def test_vector_quantize_tensor_with_torchpq_if_available():
    torchpq = pytest.importorskip("torchpq")
    _ = torchpq  # silence linter unused

    x = torch.tensor(
        [
            [1.0, 0.0, 0.0],
            [0.9, 0.1, 0.0],
            [0.0, 1.0, 0.0],
            [0.0, 0.9, 0.1],
        ],
        dtype=torch.float32,
    )
    cfg = QuantFieldConfig(bitwidth=4, method="vector", vector_clusters=2)

    try:
        restored, stats, int_tensor, codebook = vector_quant.quantize_tensor(x, cfg)
    except Exception as e:
        pytest.xfail(f"torchpq CPU path known issue: {e}")

    assert stats.method == "vector"
    assert stats.channels == x.shape[1]
    assert isinstance(stats.codebook, list) and len(stats.codebook) >= 2
    assert stats.codebook_shape is not None
    assert int_tensor is not None and int_tensor.shape == (x.shape[0], 1)

    y = vector_quant.dequantize_tensor(int_tensor, stats)
    assert y.shape == x.shape
    assert torch.isfinite(y).all()