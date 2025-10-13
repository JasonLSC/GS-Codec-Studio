# 配置系统使用说明

本说明聚焦于 `examples/simple_trainer.py` 入口新增的配置文件能力，帮助在开发阶段快速复现实验配置。

## 读取外部配置

- 通过 `--config <path>`（或同义标志 `--config-path`、`-c`）加载 YAML 文件。  
- YAML 中仅需写入与默认值不同的字段；未出现的字段继续使用 dataclass 默认值。  
- 可以与命令行参数叠加使用：先读取默认值，再合并 YAML，最后由 CLI 覆盖。

示例：

```bash
python examples/simple_trainer.py default \
  --config configs/entropy_finetune.yaml \
  --rd_lambda 0.02
```

## 保存配置快照

- 启动后，脚本会自动将最终的配置写入 `result_dir/config_snapshot.yaml`。  
- 也可使用 `--save-config <path>`（别名 `--config-save`）手动指定输出位置。
- 快照文件包含本次实验使用的全部参数；下次运行只需 `--config <snapshot>` 即可复现。

示例：

```bash
python examples/simple_trainer.py default \
  --config configs/entropy_finetune.yaml \
  --save-config logs/run_001.yaml

# 复现
python examples/simple_trainer.py default --config logs/run_001.yaml
```

> 提示：策略（`strategy`）字段会以 `{type: DefaultStrategy, params: {...}}` 形式写入 YAML，方便直接调整或复用。


## Adaptive Mask 重构设计（进行中）

- **目标**：将自适应 SHN mask 重构为独立子模块，和可微分量化、熵约束保持同等抽象层级；Trainer 与 orchestrator 仅通过统一接口消费 mask 输出/损失/指标。
- **配置结构**：
  ```python
  @dataclass
  class LearnableMaskSettings:
      start_temp: float = 5.0
      end_temp: float = 0.1
      total_iters: int = 30_000
      target_sparsity: float = 0.2
      lr: float = 1e-2

  @dataclass
  class GradientMaskSettings:
      grad_threshold: float = 2e-3  # 小于该阈值的梯度会被清零

  @dataclass
  class MaskConfig:
      enabled: bool = False
      strategy: Optional[str] = "learnable"
      start_step: int = 10_000
      learnable: LearnableMaskSettings = field(default_factory=LearnableMaskSettings)
      gradient: GradientMaskSettings = field(default_factory=GradientMaskSettings)
  ```
  YAML 示例：
  ```yaml
  compression_sim_cfg:
    mask:
      enabled: true
      strategy: gradient
      start_step: 8000
      gradient:
        grad_threshold: 0.003
  ```
- **模块划分**：新增 `gsplat/compression_simulation/mask.py`，定义 `MaskResult`、`AdaptiveMaskBase` 接口；`AdaptiveMaskFactory` 根据策略返回 `LearnableAdaptiveMask`（封装 `AnnealingMask`）、`GradientAdaptiveMask`（梯度阈值裁剪）或 `NullAdaptiveMask`。
- **Gradient 策略**：
  - `maybe_update` 统计 SHN 非零比例，供日志与阈值计算。
  - `apply` 在 `step > start_step` 时注册一次 `tensor.register_hook`，返回 `MaskResult(value=tensor, metrics={"mask_ratio": ..., "mask_grad_threshold": ...})`。
  - Hook 逻辑：
    ```python
    def grad_hook(grad):
        shn = tensor.detach()
        zero_mask = (shn == 0).all(dim=-1).all(dim=-1)
        grad_norm = grad.flatten(2).norm(p=2, dim=-1)
        mask = ~(zero_mask & (grad_norm < cfg.gradient.grad_threshold))
        while mask.ndim < grad.ndim:
            mask = mask.unsqueeze(-1)
        return grad * mask
    ```
- **Orchestrator 接入**：`CompressionSimulation.run()` 中调用 `mask.maybe_update(step, splats)`；对 `shN` 应用 `mask.apply` 并汇总 `loss_terms` 与 `metrics`；`step_optimizers`/`state_dict` 同步 mask 状态。
- **Trainer/日志**：训练循环只消费 `SimulationResult.loss_terms.get("mask")` 与 `metrics["mask_ratio"]`；导出 PLY 时通过 mask 模块暴露的 `get_binary_mask()` 获取最终掩码。
- **测试计划**：新增 `tests/test_adaptive_mask.py` 覆盖 learnable（温度调度、loss、优化器）、gradient（阈值裁剪、指标）与 null（passthrough）。
