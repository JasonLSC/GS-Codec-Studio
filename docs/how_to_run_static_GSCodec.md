# How to Run static GSCodec

We ships with ready-to-run training and codec scripts for three static datasets:

- Tanks_and_Temples (TT)
- MipNeRF360 (Mip)
- Deep Blending (DB)

Each dataset has a matching script bundle under `examples/scripts/static_exps/<dataset>`. The remainder of this guide walks through the TT scripts; workflows for Mip and DB follow the same patterns.

## Key Scripts for Tanks and Temples

- `train_single_ratepoint.sh`: Runs both training and compression for all TT scenes at one RD λ value (default scenes are `train` and `truck`).
- `train_multi_ratepoint.sh`: Calls the single-rate-point script sequentially for multiple RD λ values.
- `compression_only_single_ratepoint.sh`: Skips training and re-runs only the compression phase for all scenes at one RD λ, assuming checkpoints already exist.
- `compression_only_multi_ratepoint.sh`: Iterates over multiple RD λ values, invoking the compression-only single-rate script each time.
- `compression_only_single_scene.sh`: Helper script to compress a single scene (useful for debugging or re-running incomplete jobs).

All scripts call `examples/simple_trainer.py mcmc` with the default configuration `examples/configs/mcmc_comp_sim.yaml`.

## Prerequisites

- Place TT scene data under `examples/data/tandt/<scene_name>` (default scenes: `train`, `truck`).
- Ensure the desired GPUs are listed in each script's `GPU_LIST` (defaults to `GPU_LIST=(1 2)`).
- Adjust `SCENE_LIST`, configuration paths, or RD λ defaults inside the scripts if your setup differs.

## Train + Compress a Single Rate Point

```bash
bash examples/scripts/static_exps/tt/train_single_ratepoint.sh 0.01
```

- Omitting the argument falls back to `RD_LAMBDA=0.01`.
- Training runs first and typically writes a checkpoint at step 29 999 inside `ckpts/`.
- The script immediately launches the compression pass (`--mode compress`) using that checkpoint.

## Batch Multiple Rate Points

```bash
# Use the script's default list (0.002, 0.006, 0.01)
bash examples/scripts/static_exps/tt/train_multi_ratepoint.sh

# Provide a custom list of RD λ values
bash examples/scripts/static_exps/tt/train_multi_ratepoint.sh 0.001 0.005 0.02
```

The script stops on the first failing rate point and prints the result directory once all rate points succeed.

## Compression-Only Workflows

Use these helpers when training is already complete and you want to regenerate compressed assets or metrics from existing checkpoints.

```bash
# Compress every scene for one RD λ
dash examples/scripts/static_exps/tt/compression_only_single_ratepoint.sh 0.01

# Compress multiple RD λ values sequentially
dash examples/scripts/static_exps/tt/compression_only_multi_ratepoint.sh 0.001 0.005 0.02

# Compress a single scene (e.g., debugging)
dash examples/scripts/static_exps/tt/compression_only_single_scene.sh truck 0.01
```

Before running these scripts, confirm that `results/tt_mcmc_comp_sim/rd_<lambda>/<scene>/ckpts/ckpt_29999_rank0.pt` (or the checkpoint referenced in the script) already exists. Each script verifies the checkpoint path and aborts if it is missing.

## Result Directory Structure

All outputs default to `results/tt_mcmc_comp_sim`. A typical layout for `RD_LAMBDA=0.001` looks like this:

```
results/tt_mcmc_comp_sim/
 ├─ rd_0.001/
 │   ├─ train/
 │   │   ├─ cfg.yml                  # Final runtime configuration
 │   │   ├─ cfg_snapshot/            # Config snapshot with CLI overrides
 │   │   ├─ ckpts/                   # Training checkpoints (e.g. ckpt_29999_rank0.pt)
 │   │   ├─ ply/                     # Exported point clouds or Gaussian splats
 │   │   ├─ post_training_compression/ # Bitstream, decoded assets, metrics
 │   │   ├─ renders/                 # Rendered validation views
 │   │   ├─ stats/                   # Training/compression statistics (JSON, CSV)
 │   │   ├─ tb/                      # TensorBoard logs
 │   │   └─ videos/                  # Rendered videos or GIF previews
 │   └─ truck/
 │       └─ ...                      # Same structure as the train scene
 └─ rd_0.005/
     └─ ...
```

Each RD λ receives its own `rd_<lambda>` folder. Scene subdirectories store every artifact from both training and compression phases, enabling reproducible analysis.

## Adapting to Other Datasets

- MipNeRF360 scripts live in `examples/scripts/static_exps/mip`.
- Deep Blending scripts live in `examples/scripts/static_exps/db`.
- Command signatures, environment variables, and output layouts mirror the TT scripts; adjust dataset paths and result directory prefixes accordingly.

