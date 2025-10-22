from __future__ import annotations

import glob
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple, Union

import numpy as np
import torch
from plyfile import PlyData, PlyElement

TensorDict = Dict[str, torch.Tensor]


@dataclass
class PlySequence:
    paths: List[Path]
    frames: List[TensorDict]

    @property
    def frame_num(self) -> int:
        return len(self.frames)


def _ensure_path(value: Union[str, Path]) -> Path:
    path = Path(value)
    if not path.exists():
        raise FileNotFoundError(f"File does not exist: {path}")
    return path


def _infer_sh_dims(total: int) -> Tuple[int, int]:
    if total == 0:
        return 0, 0
    if total % 3 != 0:
        raise ValueError(f"Expected SH channel size to be divisible by 3, got {total}")
    return total // 3, 3


def load_ply_file(path: Union[str, Path]) -> TensorDict:
    ply = PlyData.read(str(_ensure_path(path)))
    vertices = ply["vertex"]

    means = np.stack((vertices["x"], vertices["y"], vertices["z"]), axis=1)
    sh0_size = len([p for p in vertices.properties if p.name.startswith("f_dc_")])
    shN_size = len([p for p in vertices.properties if p.name.startswith("f_rest_")])
    opacity = vertices["opacity"].astype(np.float32)

    sh0 = np.zeros((means.shape[0], sh0_size), dtype=np.float32)
    for idx in range(sh0_size):
        sh0[:, idx] = vertices[f"f_dc_{idx}"]

    shN = np.zeros((means.shape[0], shN_size), dtype=np.float32)
    for idx in range(shN_size):
        shN[:, idx] = vertices[f"f_rest_{idx}"]

    scale_size = len([p for p in vertices.properties if p.name.startswith("scale_")])
    scales = np.zeros((means.shape[0], scale_size), dtype=np.float32)
    for idx in range(scale_size):
        scales[:, idx] = vertices[f"scale_{idx}"]

    quat_size = len([p for p in vertices.properties if p.name.startswith("rot_")])
    quats = np.zeros((means.shape[0], quat_size), dtype=np.float32)
    for idx in range(quat_size):
        quats[:, idx] = vertices[f"rot_{idx}"]

    sh0_d1, sh0_d2 = _infer_sh_dims(sh0_size)
    shN_d1, shN_d2 = _infer_sh_dims(shN_size)

    sh0_tensor = torch.from_numpy(sh0.reshape(-1, sh0_d2, sh0_d1).transpose(0, 2, 1)) if sh0_size else torch.empty((means.shape[0], 0, 0))
    shN_tensor = torch.from_numpy(shN.reshape(-1, shN_d2, shN_d1).transpose(0, 2, 1)) if shN_size else torch.empty((means.shape[0], 0, 0))

    return {
        "means": torch.from_numpy(means.astype(np.float32)),
        "sh0": sh0_tensor,
        "shN": shN_tensor,
        "opacities": torch.from_numpy(opacity),
        "scales": torch.from_numpy(scales),
        "quats": torch.from_numpy(quats),
    }


def save_ply_file(splats: TensorDict, path: Union[str, Path]) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)

    means = splats["means"].cpu().numpy()
    sh0 = splats["sh0"].cpu().numpy()
    shN = splats["shN"].cpu().numpy()
    opacities = splats["opacities"].cpu().numpy()
    scales = splats["scales"].cpu().numpy()
    quats = splats["quats"].cpu().numpy()

    normals = np.zeros_like(means)
    sh0_flat = sh0.transpose(0, 2, 1).reshape(means.shape[0], -1)
    shN_flat = shN.transpose(0, 2, 1).reshape(means.shape[0], -1)

    attributes = ["x", "y", "z", "nx", "ny", "nz"]
    attributes += [f"f_dc_{i}" for i in range(sh0_flat.shape[1])]
    attributes += [f"f_rest_{i}" for i in range(shN_flat.shape[1])]
    attributes.append("opacity")
    attributes += [f"scale_{i}" for i in range(scales.shape[1])]
    attributes += [f"rot_{i}" for i in range(quats.shape[1])]
    dtype_full = [(attr, "f4") for attr in attributes]

    concat = np.concatenate(
        (means, normals, sh0_flat, shN_flat, opacities[:, None], scales, quats), axis=1
    )
    elements = np.empty(means.shape[0], dtype=dtype_full)
    elements[:] = list(map(tuple, concat))
    vertex_element = PlyElement.describe(elements, "vertex")
    PlyData([vertex_element]).write(str(path))


def load_ply_sequence(pattern: str, frame_num: int = 1) -> PlySequence:
    paths = sorted(Path(p) for p in glob.glob(pattern))
    if not paths:
        raise FileNotFoundError(f"No PLY files matched pattern: {pattern}")
    if frame_num <= 0:
        raise ValueError("frame_num must be positive")
    selected = paths[:frame_num]
    frames = [load_ply_file(p) for p in selected]
    return PlySequence(paths=list(selected), frames=frames)


def save_ply_sequence(frames: Sequence[TensorDict], directory: Union[str, Path], stem: str = "frame") -> List[Path]:
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    saved_paths: List[Path] = []
    for idx, splats in enumerate(frames):
        out_path = directory / f"{stem}{idx:03d}.ply"
        save_ply_file(splats, out_path)
        saved_paths.append(out_path)
    return saved_paths


def load_ckpt_file(path: Union[str, Path]) -> TensorDict:
    payload = torch.load(str(_ensure_path(path)), map_location="cpu", weights_only=False)
    if "splats" not in payload:
        raise KeyError("Checkpoint missing 'splats' entry")
    splats_state = payload["splats"]
    return {k: torch.as_tensor(v) for k, v in splats_state.items()}
