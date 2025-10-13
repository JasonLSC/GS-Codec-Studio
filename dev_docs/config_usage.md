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

