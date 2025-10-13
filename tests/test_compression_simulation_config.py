import types

import torch
from torch import nn

from gsplat.compression_simulation import (
    CompSimConfig,
    NullCompressionSimulation,
    LegacyCompressionSimulationAdapter,
)


def test_comp_sim_config_from_trainer_defaults():
    trainer_cfg = types.SimpleNamespace()
    trainer_cfg.compression_sim = True
    trainer_cfg.entropy_model_opt = True
    trainer_cfg.entropy_model_type = "gaussian_model"
    trainer_cfg.entropy_steps = {"scales": 5000, "quats": 8000}
    trainer_cfg.shN_ada_mask_opt = True
    trainer_cfg.ada_mask_steps = 12_000
    trainer_cfg.shN_ada_mask_strategy = "learnable"
    trainer_cfg.strategy = types.SimpleNamespace(cap_max=123456)

    cfg = CompSimConfig.from_trainer_config(trainer_cfg)

    assert cfg.enabled is True
    assert cfg.entropy.enabled is True
    assert cfg.entropy.model_type == "gaussian_model"
    assert cfg.entropy.steps["means"] == -1
    assert cfg.entropy.steps["scales"] == 5000
    assert cfg.mask.enabled is True
    assert cfg.mask.start_step == 12_000
    assert cfg.mask.cap_max == 123456


def test_null_compression_simulation_passthrough():
    null_sim = NullCompressionSimulation()
    params = nn.ParameterDict({"means": nn.Parameter(torch.randn(2, 3))})

    result = null_sim.run(params, step=0)

    assert set(result.splats.keys()) == {"means"}
    assert torch.allclose(result.splats["means"], params["means"])
    assert result.loss_terms == {}
    assert result.metrics == {}

    null_sim.step_optimizers(step=0)
    null_sim.load_state_dict({})
    assert null_sim.state_dict() == {}

def test_legacy_adapter_run_provides_metrics():
    cfg = CompSimConfig(enabled=True)
    cfg.entropy.enabled = False
    legacy = types.SimpleNamespace(
        simulate_compression=lambda splats, step: ({"means": splats["means"] + 1}, {"means": torch.ones_like(splats["means"])}),
        entropy_model_optimizers={},
        entropy_model_schedulers={},
    )

    adapter = LegacyCompressionSimulationAdapter(legacy, cfg)
    params = {"means": torch.zeros(1, 3)}

    result = adapter.run(params, step=0)

    assert torch.allclose(result.splats["means"], torch.ones(1, 3))
    assert "entropy_bits" in result.metrics
    assert torch.equal(result.metrics["entropy_bits"]["means"], torch.ones(1, 3))

    adapter.step_optimizers(step=0)  # Should be a no-op without optimizers
