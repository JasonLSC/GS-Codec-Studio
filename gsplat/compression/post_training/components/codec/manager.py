from __future__ import annotations

from pathlib import Path
from typing import Tuple

from ...configs import CodecConfig
from ...io import TensorDict
from ...stages import QuantizationContext
from ...codec import build_codec, CodecArtifacts


class CodecManager:
    def __init__(self, config: CodecConfig) -> None:
        self.config = config
        # For now, a single codec configured globally. Attribute-level routing can be added later.
        self._codec = build_codec(config)

    def encode_all(self, frame_dir: Path, splats: TensorDict, quant_ctx: QuantizationContext) -> CodecArtifacts:
        return self._codec.encode(frame_dir, splats, quant_ctx)

    def decode_all(self, frame_dir: Path, artifacts: CodecArtifacts, quant_ctx: QuantizationContext) -> TensorDict:
        return self._codec.decode(frame_dir, artifacts, quant_ctx)


