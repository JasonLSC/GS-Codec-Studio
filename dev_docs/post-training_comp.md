# Post-Training Compression 重构设想

## 目标
- 支持统一的后处理压缩入口，可消费训练得到的 splats（来源为 PLY 或 checkpoint）。
- 提供 encode / decode 两个核心函数：
  - `encode()`：从输入载入 splats，执行 pruning、quantization、3D→2D 映射、视频/图像编码，输出码流或压缩文件集合。
  - `decode()`：从码流恢复 splats，执行视频/图像解码、反量化等操作，可输出内存结构或保存为 PLY。
- 允许通过配置选择 codec 以及预处理策略，便于扩展新变种。

## 拟议架构
### 1. 顶层 Orchestrator（暂名 `PostTrainingCompressor`）
- 初始化参数：
  - `input_spec`: 指定输入类型（`ply` 或 `ckpt`）及路径。
  - `codec_config`: 指定编码方式（PNG / Entropy / HEVC / SeqYUV 等）及对应参数。
  - `preprocess_config`: 可选，控制 pruning、排序、属性变换等。
  - `quant_config`: 可选，控制按属性的量化位宽、截断范围等。
- 方法：
  - `encode()`：驱动全流程；返回压缩输出路径、元信息。
  - `decode(compressed_dir)`：读取码流、还原 splats；可返回字典或写入 PLY。

### 2. 数据加载层
- `load_ckpt(path)`：从 checkpoint 中提取 `splats`（means/scales/quats/...）。
- `load_ply_sequence(path | list)`：加载单帧或序列 PLY，返回统一的张量字典。
- 两者都转换成标准的 `Dict[str, Tensor]` 供后续模块使用。

### 3. 编码流水线模块
- `PruningStage`：离群点过滤、mask 过滤等，可配置开关。
- `QuantizationStage`：基于属性设置 bitwidth、clamp range，复用现有 `_compress_*` 逻辑。
- `MappingStage`：排序/映射策略（PLAS、morton、无排序）；负责 3D→2D 重排。
- `CodecStage`：调度 `gsplat/compression` 中的具体 codec 类。
- 元信息统一写入 `meta.json`（沿用现状）。

### 4. 解码流水线模块
- `CodecStage.decode`：调用 codec 的 `decompress`，获得属性张量。
- `DequantizationStage`：执行逆变换、逆量化；可复用 `inverse_log_transform` 等函数。
- 最终输出：
  - 内存中的 `Dict[str, Tensor]`，或
  - 调用 `save_ply` 写入磁盘。

### 5. 配置结构
使用 dataclass 组织配置，便于 YAML/CLI：
```python
@dataclass
class PTCompressionConfig:
    input_type: Literal["ply", "ckpt"]
    codec: Literal["png", "entropy", "hevc", "seq_hevc", "seq_yuv"]
    codec_params: Dict[str, Any] = field(default_factory=dict)
    preprocess: PreprocessConfig = field(default_factory=PreprocessConfig)
    quant: QuantConfig = field(default_factory=QuantConfig)
```
其中 `PreprocessConfig`、`QuantConfig` 再细分字段（是否过滤、排序策略、bitwidth 等）。

### 6. CLI/脚本整合
- 重写/新增脚本（如 `examples/benchmarks/post_train_compress.sh`）调用 `PostTrainingCompressor`。
- YAML/CLI 用同一套配置，避免脚本内硬编码。
- 脚本流程：解析配置 → `encode()` → `decode()` → 统计评估 → 输出路径。

## 顾及到的补充需求
- 模块内部独立提供 PLY / CKPT 的读写接口（与 encode/decode 解耦）。
- 支持读取单帧或序列 splats，保留 `is_sequence` 或 `num_frames` 标识。
- `encode()` 前和 `decode()` 后输出参数分布直方图（matplotlib），但不写入 meta 数据。
- 实现时参考现有 `gsplat/compression` 的操作顺序，避免与现有流程背离。

## `PostTrainingComp` 草图
```python
@dataclass
class PostTrainingComp:
    input_spec: InputSpec
    codec_config: CodecConfig
    preprocess_cfg: PreprocessConfig
    quant_cfg: QuantConfig
    output_dir: Path

    splats: Union[Dict[str, Tensor], List[Dict[str, Tensor]]] = field(init=False)
    is_sequence: bool = field(init=False)
    codec: BaseCodec = field(init=False)
    metadata: Dict[str, Any] = field(default_factory=dict)

    def load_inputs(self) -> None:
        if self.input_spec.type == "ply":
            self.splats = load_ply(self.input_spec.path, as_sequence=self.input_spec.as_sequence)
        elif self.input_spec.type == "ckpt":
            self.splats = load_ckpt(self.input_spec.path)
        self.is_sequence = isinstance(self.splats, list)
        self.metadata["num_frames"] = len(self.splats) if self.is_sequence else 1

    def encode(self) -> CompressionResult:
        self.load_inputs()
        self._plot_stats(self.splats, stage="before_encode")
        payload = self._run_encode_pipeline(self.splats)
        self._save_payload(payload)
        return CompressionResult(payload_path=..., metadata=self.metadata)

    def decode(self, payload_path: Path) -> DecodeResult:
        payload = self._load_payload(payload_path)
        decoded = self._run_decode_pipeline(payload)
        self._plot_stats(decoded, stage="after_decode")
        self._write_outputs(decoded)
        return DecodeResult(splats=decoded, saved_paths=...)

    def _run_encode_pipeline(self, splats):
        pruned = run_pruning(splats, self.preprocess_cfg)
        quantized = run_quantization(pruned, self.quant_cfg)
        mapped, mapping_ctx = run_mapping(quantized, self.preprocess_cfg.mapping)
        encoded = self.codec.encode(mapped, context=mapping_ctx)
        return {"encoded": encoded, "mapping_ctx": mapping_ctx}

    def _run_decode_pipeline(self, payload):
        decoded = self.codec.decode(payload["encoded"], context=payload.get("mapping_ctx"))
        unmapped = run_inverse_mapping(decoded, payload.get("mapping_ctx"))
        dequant = run_dequantization(unmapped, self.quant_cfg)
        restored = run_postprocess(dequant, self.preprocess_cfg)
        return restored

    def _plot_stats(self, splats, stage: str) -> None:
        # 遍历属性画直方图，保存到 output_dir/stage_* 下
```

- `InputSpec`、`CodecConfig`、`PreprocessConfig`、`QuantConfig` 等 dataclass 可进一步细化。
- `CompressionResult` / `DecodeResult` 用于统一返回路径、元数据。
- PLY/CKPT 的读写实现为独立的 util 函数。
- `BaseCodec` 为各 codec 的抽象基类，具体实现参考现有 PNG/Entropy/HEVC 等类。

## 未决问题
1. 是否需要一次性处理多个场景/帧并输出统一码流？
2. 统计文件命名、保存位置是否需要可配置？
3. 与现有脚本结合时，是否要提供默认 YAML 模板。

## 设计补充（2024-xx-xx）
- `Quantization` / `Dequantization` 阶段为必选流程，需始终执行。
- `Pruning`、`Mapping`、`Codec` 阶段可以按需求启用或跳过，可通过配置显式控制。
- `PostTrainingComp` 需提供 `save_decoded_splats(decoded, destination)` 助手，用于在 `decode()` 完成后将还原的 splats 持久化到 PLY / CKPT 等目标格式，避免重复实现写盘逻辑。
