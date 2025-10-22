import torch
import pytest

pytest.importorskip("torchpq")

from gsplat.compression.post_training.components.quant.manager import QuantManager
from gsplat.compression.post_training.configs import QuantConfig, QuantFieldConfig


def test_vector_quantization_shn_basic():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    torch.manual_seed(0)
    data = torch.tensor(
        [
            [[1.0, 0.0, 0.0]],
            [[1.0, 0.0, 0.0]],
            [[1.0, 0.0, 0.0]],
            [[0.0, 1.0, 1.0]],
            [[0.0, 1.0, 1.0]],
            [[0.0, 0.0, 0.0]],
        ],
        device=device,
    )
    splats = {"shN": data.clone()}

    quant_cfg = QuantConfig(
        enabled=True,
        default=QuantFieldConfig(enabled=False),
        fields={
            "shN": QuantFieldConfig(
                method="vector",
                vector_clusters=2,
                store_as_int=True,
                bitwidth=8,
            )
        },
    )

    manager = QuantManager(quant_cfg)
    quantized, ctx = manager.quantize_all(splats)

    stats = ctx.field_stats["shN"]
    assert stats.method == "vector"
    assert stats.codebook is not None
    assert len(stats.codebook) == 2
    assert stats.mask is not None
    assert stats.codebook_bits == 8

    labels = ctx.int_values["shN"]
    assert labels.dtype == torch.int32
    assert "shN" in ctx.codebooks

    mask_tensor = torch.tensor(stats.mask, device=device, dtype=torch.bool)
    restored = manager.dequantize_all({"shN": labels.to(torch.float32)}, ctx)

    assert torch.allclose(
        restored["shN"][mask_tensor], quantized["shN"][mask_tensor], atol=1e-5
    )
    if (~mask_tensor).any():
        assert torch.allclose(
            restored["shN"][~mask_tensor],
            torch.zeros_like(restored["shN"][~mask_tensor]),
            atol=1e-6,
        )
