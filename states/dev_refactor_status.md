## 环境状态概览
- 当前分支：`dev_refactor`，源自 `codec_decouple...zju_gsc/codec_decouple`。
- 现存未跟踪文件：`third_party/plas/`（进入仓库前已存在，未改动）。
- `examples/simple_trainer.py` 引入新的配置系统工具：
  - 支持 `--config/--config-path/-c` 读入 YAML，仅覆盖差异字段。
  - CLI 参数仍旧通过 `tyro.extras.overridable_config_cli` 工作。
  - 每次运行后自动输出快照；默认写到 `result_dir/config_snapshot.yaml`，可用 `--save-config/--config-save` 指定位置。
  - 策略配置支持 `{type: Name, params: {...}}` 形式；快照也按此格式存储。
- 在 `dev_docs/config_usage.md` 记录了上述用法说明，便于团队查阅。

## 后续待办
1. 在写权限环境中跑一次轻量级校验（至少启动 CLI / dry-run）确认新配置流无报错。
2. 按既定方案重构 `CompressionSimulation`（子模块拆分、接口统一等）。
3. 根据需要扩展或调整文档，最后清理未使用的 import/函数（如 `gsplat.compression_simulation import simulation` 若仍无引用）。

