# How to Extend GSCodec Studio

GSCodec Studio is a modular framework that lets developers select different components through configuration and plug new modules into the pipeline. Two representative subsystems are Training Time Compression Simulation and Post Training Compression. The rest of this document walks through both subsystems to illustrate how to extend GSCodec Studio in practice.

## Training Time Compression Simulation
**a) Module overview:** `DefaultCompressionSimulation` in `gsplat/compression_simulation/runtime.py` stitches together three component families:
- `DifferentiableQuantizer` performs differentiable quantization for attributes such as `means`, `scales`, `quats`, and `opacities` (see `gsplat/compression_simulation/quantizer.py`). Both noise-injection and straight-through round modes are available.
- `EntropyConstraint` estimates bits and accumulates rate–distortion loss after the configured steps (implementation in `gsplat/compression_simulation/entropy.py`).
- `AdaptiveMaskFactory` applies optional SH-band sparsity masks using either `learnable` or `gradient` strategies (see `gsplat/compression_simulation/mask.py`).
During `DefaultCompressionSimulation.run()`, the framework reads the `CompSimConfig` definition (`gsplat/compression_simulation/config.py`), invokes quantization, entropy, and masking sequentially, and collects auxiliary losses and metrics so that training can account for compression artefacts early on.

**b) Submodule activation and configuration:** `CompSimConfig` consolidates the switches for quantization, entropy, and adaptive masking:
- `quantizer.attributes` exposes per-field settings such as `bitwidth`, `clamp_range`, and `mode`.
- `entropy.enabled` and `entropy.steps` decide when entropy models should run, while `model_type` toggles between `factorized_model` and `gaussian_model`.
- `mask.enabled` and `mask.strategy` control SH-band sparsity and the iteration at which masking starts.
Training scripts usually inject this structure when constructing the trainer. The snippet below enables quantization and entropy for `scales` and `quats`, and activates the learnable mask after 12k steps:

```python
from gsplat.compression_simulation.config import (  # code location
    CompSimConfig, QuantizerConfig, AttributeQuantizerConfig,
    EntropyConfig, MaskConfig,
)

comp_sim_cfg = CompSimConfig(
    enabled=True,
    quantizer=QuantizerConfig(attributes={
        "scales": AttributeQuantizerConfig(bitwidth=8, clamp_range=(-10.0, 2.0), mode="round"),
        "quats": AttributeQuantizerConfig(bitwidth=8, clamp_range=(-1.0, 1.0)),
    }),
    entropy=EntropyConfig(
        enabled=True,
        model_type="factorized_model",
        steps={"scales": 10000, "quats": 8000},
    ),
    mask=MaskConfig(enabled=True, strategy="learnable", start_step=12000),
)
# After attaching `comp_sim_cfg` to the trainer, `DefaultCompressionSimulation.run()` will automatically activate the configured submodules.
```

- Quantization: subclass `DifferentiableQuantizer` in `gsplat/compression_simulation/quantizer.py` and override `_quantizers` in a custom simulator.
- Entropy models: extend `EntropyConstraint` to support extra `model_type` values or inject additional bit estimators.
- Masking: implement a new `AdaptiveMaskBase` subclass and register the strategy inside `AdaptiveMaskFactory.create()`.
The pseudo code below injects a custom quantizer for the `colors` field by deriving from the default simulator:

```python
from gsplat.compression_simulation.runtime import DefaultCompressionSimulation
from gsplat.compression_simulation.quantizer import DifferentiableQuantizer, QuantizeResult

class ColorClippingQuantizer(DifferentiableQuantizer):
    def quantize(self, tensor, step) -> QuantizeResult:
        clipped = tensor.clamp(-6.0, 6.0)
        return QuantizeResult(value=clipped, q_step=None, metadata={})

class CustomCompressionSimulation(DefaultCompressionSimulation):
    def __init__(self, config, device=None):
        super().__init__(config, device=device)
        colors_cfg = config.quantizer.attributes.get("colors")
        if colors_cfg is not None:
            self._quantizers["colors"] = ColorClippingQuantizer("colors", colors_cfg)

# Replace the default simulator with CustomCompressionSimulation at the training entry point to load the new module.
```
Extensions for entropy models or masking strategies follow the same pattern: implement a new class → register it in the factory or in a derived simulator → enable it via configuration to control when it starts.

**d) Testing and validation:**
- Unit tests: follow the assertion patterns in `tests/test_quantizer.py`, `tests/test_entropy_constraint.py`, and `tests/test_adaptive_mask.py` to cover custom quantizers or entropy models.
- Integration tests: extend `tests/test_compression_simulation_config.py`, build a `CompSimConfig`, invoke `DefaultCompressionSimulation.run()`, and validate tensor shapes, mask metrics, and auxiliary losses.
- Training regression: enable the new module inside your training script and monitor metrics such as `quantizer/*` or `entropy/*` to verify that bitwidths and estimated rates respond to configuration changes.
 
## Post Training Compression
**a) Module overview:** The single-frame post-training pipeline is orchestrated by `PostTrainingCompressor` in `gsplat/compression/post_training/orchestrator.py`. Its `encode()` method executes five sequential stages:
- `PrePostProcessor` (`components/preprocessor.py`) applies preprocessing such as log-domain transforms for `means` and quaternion normalization, and its inverse is used during decode.
- `Pruner` (`components/pruner.py`) performs outlier filtering, opacity thresholding, top-k selection, and scale-based pruning, all driven by configuration knobs.
- `QuantManager` (`components/quant/manager.py`) loads registered scalar or vector quantizers and records per-field statistics.
- `Mapper` (`components/mapper.py`) reorders splats using Morton or PLAS strategies and keeps the quantization context in sync.
- `CodecManager` (`components/codec/manager.py`) instantiates codecs such as PNG, HEVC, or STG according to `CodecConfig` and writes the artifacts to disk.
The orchestrator also emits `metadata_for_decoding.json` and `compression_info.json`, capturing the quantization and mapping metadata required for reproducible decoding.

**b) Submodule activation and configuration:** `PTCompConfig` is the top-level configuration for post-training compression (defined in `components/configs.py`). Its nested fields control the on/off toggles and behavior of each stage:
- `preprocess` toggles log transforms and quaternion normalization;
- `pruning` configures outlier filtering, opacity threshold, and the number of points to keep;
- `quant` specifies the default quantizer and per-field overrides, supporting registered `scalar` and `vector` quantizers;
- `mapping` decides whether to reorder points and whether to enforce square tiling;
- `codec` selects the final packaging/codec.
The YAML snippet below shows a common configuration:

```bash
preprocess:
  enabled: true
  apply_means_log_transform: true
  apply_quat_normalize: true

pruning:
  enabled: true
  use_outlier_filter: true
  opacity_threshold: 0.01
  max_points: 500000

quant:
  enabled: true
  default:
    method: "scalar"
    bitwidth: 8
  fields:
    shN:
      method: "vector"
      vector_clusters: 4096
      vector_sample_size: 50000

mapping:
  enabled: true
  strategy: "morton"
  ensure_square: true

codec:
  name: "png"
  params:
    vq_storage:
      shN: "masked_npz_both"
```
In Python, instantiate `PTCompConfig` and pass it to `PostTrainingCompressor(config, output_dir)` to load the configuration above.

**c) Extension points for new modules:** The post-training compression pipeline exposes several pluggable interfaces:
- Quantizers: register a new quantizer in `components/quant/registry.py`, implement the `quantize_field`/`dequantize_field` interfaces, and reference the new method name under `quant.fields`;
- Codecs: register a new `BaseCodec` implementation via `codec.build_codec()`, enabling new container formats;
- Mapping strategies: extend `Mapper.map()` with a new `strategy`, or implement a new ordering function.
The following example shows how to add a quantizer named `symmetric_scalar`:

```python
from gsplat.compression.post_training.components.quant.registry import register_quantizer
from gsplat.compression.post_training.components.quant.scalar import QuantFieldStats

@register_quantizer("symmetric_scalar")
class SymmetricScalarQuantizer:
    def quantize_field(self, tensor, config):
        # apply symmetric clipping then uniform quantization
        clipped = tensor.clamp(-config.clamp_max, config.clamp_max)
        # ... compute stats & integers
        return clipped, stats, ints, None

    def dequantize_field(self, tensor, stats):
        # reconstruct symmetric range
        return tensor.float()
```
After registration, set `quant.fields.means.method` to `symmetric_scalar` to enable it.

**d) Testing and validation:**
- Unit tests: see `tests/test_post_training_quant.py` and `tests/test_post_training_vq.py`. Add assertions for input–output consistency for the new quantizer or mapping strategy.
- Integration tests: craft a small `PTCompConfig`, call `PostTrainingCompressor.encode()` / `decode()`, and verify that the generated `metadata_for_decoding.json` contains the new fields and that compression is reversible.
- Benchmark scripts: run `examples/scripts/static_exps/tt/train_single_ratepoint.sh` or your own scenario. Inspect quantization summaries and final bitrates in `compression_info.json` to compare metrics before and after.
