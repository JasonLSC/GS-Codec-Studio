import json
import os
import subprocess
from dataclasses import dataclass, field, InitVar
import glob
import shutil
from typing import Any, Callable, Dict, List, Optional, Union

import numpy as np
from sympy import im
import torch
import torch.nn.functional as F
from torch import Tensor
from torch.nn import Module

from gsplat import compression
from gsplat.compression.outlier_filter import filter_splats
from gsplat.compression.sort import sort_splats
from gsplat.utils import inverse_log_transform, log_transform
import copy


@dataclass
class SeqHevcCompressionRot:
    """Uses quantization and sorting to compress splats into mp4 files via libx265
      and uses K-means clustering to compress the spherical harmonic coefficents.

    .. warning::
        This class requires the `imageio <https://pypi.org/project/imageio/>`_,
        `plas <https://github.com/fraunhoferhhi/PLAS.git>`_
        and `torchpq <https://github.com/DeMoriarty/TorchPQ?tab=readme-ov-file#install>`_ packages to be installed.

    .. warning::
        This class might throw away a few lowest opacities splats if the number of
        splats is not a square number.

    .. note::
        The splats parameters are expected to be pre-activation values. It expects
        the following fields in the splats dictionary: "means", "scales", "quats",
        "opacities", "sh0", "shN". More fields can be added to the dictionary, but
        they will only be compressed using NPZ compression.

    References:
        - `Compact 3D Scene Representation via Self-Organizing Gaussian Grids <https://arxiv.org/abs/2312.13299>`_
        - `Making Gaussian Splats more smaller <https://aras-p.info/blog/2023/09/27/Making-Gaussian-Splats-more-smaller/>`_

    Args:
        use_sort (bool, optional): Whether to sort splats before compression. Defaults to True.
        verbose (bool, optional): Whether to print verbose information. Default to True.
    """

    use_sort: bool = True
    verbose: bool = True
    qp: Dict[str, Union[int, Dict[str, Any]]] = field(default_factory=lambda: {
        "means": -1,
        "opacities": 4,
        "quats": 4,
        "shift_quats": -1,
        "shift_scales": -1,
        "shift_sh0": -1,
        "shift_shN": -1,
        "scales": 4,
        "sh0": 16,
        "shN":{
            "sh1": 20,
            "sh2": 24,
            "sh3": 28
        }
    })
    n_clusters: int = 32768
    debug: bool = False
    use_all_intra: bool = False

    attribute_codec_registry: InitVar[Optional[Dict[str, str]]] = None

    compress_fn_map: Dict[str, Callable] = field(default_factory=lambda: {
        "means": _compress_video_hevc_16bit,
        "scales": _compress_video_hevc,
        "quats": _compress_kmeans,
        "opacities": _compress_video_hevc,
        "sh0": _compress_video_hevc,
        # "shN": _compress_shN_video_hevc
        "shN": _compress_masked_kmeans,
    })
    decompress_fn_map: Dict[str, Callable] = field(default_factory=lambda: {
        "means": _decompress_video_hevc_16bit,
        "scales": _decompress_video_hevc,
        "quats": _decompress_kmeans,
        "opacities": _decompress_video_hevc,
        "sh0": _decompress_video_hevc,
        # "shN": _decompress_shN_video_hevc
        "shN": _decompress_masked_kmeans,
    })

    def __post_init__(self, attribute_codec_registry):
        if attribute_codec_registry:
            available_functions = {
                "_compress_video_hevc_16bit": _compress_video_hevc_16bit,
                "_compress_video_hevc": _compress_video_hevc,
                "_compress_quats_video_hevc": _compress_quats_video_hevc,
                "_compress_shN_video_hevc": _compress_shN_video_hevc,
                "_compress_masked_kmeans": _compress_masked_kmeans,
                "_compress_kmeans": _compress_kmeans,

                "_decompress_video_hevc_16bit": _decompress_video_hevc_16bit,
                "_decompress_video_hevc": _decompress_video_hevc,
                "_decompress_quats_video_hevc": _decompress_quats_video_hevc,
                "_decompress_shN_video_hevc": _decompress_shN_video_hevc,
                "_decompress_masked_kmeans": _decompress_masked_kmeans,
                "_decompress_kmeans": _decompress_kmeans,
            }

            for attr_name, attr_codec in attribute_codec_registry.items(): # go through the registry
                if attr_name in self.compress_fn_map and "encode" in attr_codec:
                    if attr_codec["encode"] in available_functions:
                        self.compress_fn_map[attr_name] = available_functions[attr_codec["encode"]]
                    else:
                        print(f"Warning: Unknown func: {attr_codec['encode']}")

                if attr_name in self.decompress_fn_map and "decode" in attr_codec:
                    if attr_codec["decode"] in available_functions:
                        self.decompress_fn_map[attr_name] = available_functions[attr_codec["decode"]]
                    else:
                        print(f"Warning: Unknown func: {attr_codec['decode']}")
            self.pad_info = [] 

    def _get_compress_fn(self, param_name: str) -> Callable:
        if param_name in self.compress_fn_map:
            return self.compress_fn_map[param_name]
        else:
            return _compress_npz

    def _get_decompress_fn(self, param_name: str) -> Callable:
        if param_name in self.decompress_fn_map:
            return self.decompress_fn_map[param_name]
        else:
            return _decompress_npz
    
    def compress(self, compress_dir: str) -> None:
        """Run compression

        Args:
            compress_dir (str): directory to save compressed files
        """

        # Param-specific preprocessing
        # splats["means"] = log_transform(splats["means"])
        self.splats_videos["quats"] = F.normalize(self.splats_videos["quats"], dim=-1)

        # ignore_keys = {k for k in self.splats_videos if k.startswith("shift")}  # <<<< 무시할 키 추가!

        meta = {}
        for param_name in self.splats_videos.keys():
            compress_fn = self._get_compress_fn(param_name)

            # --- shN 전용 플래트닝 + masked_kmeans 압축 ---
            if param_name == "shN" and compress_fn.__name__ in ["_compress_kmeans", "_compress_masked_kmeans"]:
                # 1) 원본 5D shape 저장
                orig_shape = list(self.splats_videos["shN"].shape)  # [T, H, W, K, C]
                # 2) (T*H*W, K, C) 로 flatten
                flat = self.splats_videos["shN"].reshape(-1, orig_shape[-2], orig_shape[-1])
                # 3) KMeans 압축 호출

                meta_shN = compress_fn(
                    compress_dir,
                    param_name,
                    flat,
                    n_clusters=self.n_clusters,
                    verbose=self.verbose
                )
                # 4) 나중에 복원할 수 있도록 원래 shape 기록
                meta_shN["orig_shape"] = orig_shape
                meta[param_name] = meta_shN

            elif param_name == "quats" and compress_fn.__name__ in ["_compress_kmeans", "_compress_masked_kmeans"]:
                orig_shape = list(self.splats_videos["quats"].shape)  # [T, H, W, 4]
                flat = self.splats_videos["quats"].reshape(-1, orig_shape[-1])  # [N, 4]

                meta_quats = compress_fn(
                    compress_dir,
                    param_name,
                    flat,
                    n_clusters=self.n_clusters,
                    verbose=self.verbose
                )
                meta_quats["orig_shape"] = orig_shape
                meta[param_name] = meta_quats

            # --- 나머지 파라미터들은 기존 방식 ---
            else:
                kwargs = {
                    "n_sidelen": int(self.splats_videos["means"].size(1)),
                    "qp": self.qp[param_name],
                    "use_all_intra": self.use_all_intra,
                    "debug": self.debug,
                }
                if compress_fn.__name__ in ["_compress_kmeans", "_compress_masked_kmeans"]:
                    kwargs["quantization"] = kwargs.pop("qp")
                    kwargs["n_clusters"] = self.n_clusters
                    kwargs["verbose"] = self.verbose

                meta[param_name] = compress_fn(
                    compress_dir, param_name, self.splats_videos[param_name], **kwargs
                )
                
        with open(os.path.join(compress_dir, "meta.json"), "w") as f:
            json.dump(meta, f)

    def decompress(self, compress_dir: str) -> Dict[str, Tensor]:
        """Run decompression

        Args:
            compress_dir (str): directory that contains compressed files

        Returns:
            Dict[str, Tensor]: decompressed Gaussian splats
        """
        with open(os.path.join(compress_dir, "meta.json"), "r") as f:
            meta = json.load(f)

        splats = {}
        for param_name, param_meta in meta.items():
            decompress_fn = self._get_decompress_fn(param_name)

            # --- shN 전용: flat → 5D 복원 ---
            if param_name == "shN" and "orig_shape" in param_meta:
                # 1) flat 텐서 복원 (shape: [N_flat, K, C])
                flat = decompress_fn(compress_dir, param_name, param_meta)
                # 2) 원본 shape으로 reshape
                T, H, W, K, C = param_meta["orig_shape"]
                splats[param_name] = flat.reshape(T, H, W, K, C)

            elif param_name == "quats" and "orig_shape" in param_meta:
                flat = decompress_fn(compress_dir, param_name, param_meta)
                splats[param_name] = flat.reshape(param_meta["orig_shape"])
                
            # --- 나머지 파라미터들은 기본 복원 ---
            else:
                splats[param_name] = decompress_fn(compress_dir, param_name, param_meta)
        return splats

    
    def sort_with_frame_index(self, splats_list: List[Dict], frame_id: int = 0) -> Tensor:
        """Organize the list of splats into several sequences of attributs

        Args:

        """
        splats_to_be_sorted = copy.deepcopy(splats_list[frame_id])

        n_gs = len(splats_to_be_sorted["means"])
        n_sidelen = int(np.ceil(n_gs**0.5))
        n_pad = n_sidelen**2 - n_gs
        if n_pad != 0:
            # splats = _crop_n_splats(splats, n_crop)
            splats_to_be_sorted = _pad_n_splats(splats_to_be_sorted, n_pad)
            print(
                f"Warning: Number of Gaussians was not square. Padded {n_pad} Gaussians."
            )
        
        _, sorted_indices = sort_splats(splats_to_be_sorted, return_indices=True, sort_with_shN=False)
        print(f"Finsh the sorting with frame {frame_id}.")

        return sorted_indices
    
    def splats_list_to_attribute_seq(self, splats_list: List[Dict]) -> Dict[str, Tensor]:
        sample_splat = splats_list[0]
        attribute_names = list(sample_splat.keys())

        splats_sequences = {}
        for attr_name in attribute_names:
            attr_seq = [splat[attr_name] for splat in splats_list if attr_name in splat]
            attr_seq = torch.stack(attr_seq, dim=0)

            splats_sequences[attr_name] = attr_seq
        
        return splats_sequences
    

    # def splats_list_to_attribute_seq(self, splats_list: List[Dict]) -> Dict[str, Tensor]:
    #     sample_splat = splats_list[0]
    #     attribute_names = list(sample_splat.keys())

    #     splats_sequences = {}
    #     for attr_name in attribute_names:
    #         # 각 프레임의 텐서 리스트를 가져옴
    #         tensors = [splat[attr_name] for splat in splats_list]

    #         # 가장 긴 길이에 맞춰 패딩
    #         max_len = max(t.shape[0] for t in tensors)
             
    #         padded_tensors = []
    #         for t in tensors:
    #             pad_len = max_len - t.shape[0]
    #             self.pad_info.append(pad_len)
    #             if pad_len > 0:
    #                 # 제일 앞 차원을 기준으로 0 padding
    #                 pad_shape = list(t.shape)
    #                 pad_shape[0] = pad_len
    #                 pad_tensor = torch.zeros(pad_shape, dtype=t.dtype, device=t.device)
    #                 t = torch.cat([t, pad_tensor], dim=0)
    #             padded_tensors.append(t)

    #         # Stack after padding
    #         splats_sequences[attr_name] = torch.stack(padded_tensors, dim=0)

    #     return splats_sequences

    
    def pad_attr_seq(self, splats_videos: Dict[str, Tensor]) -> Dict[str, Tensor]:
        n_gs = splats_videos["means"].size(1)
        n_sidelen = int(np.ceil(n_gs**0.5))
        n_pad = n_sidelen**2 - n_gs
        if n_pad != 0:
            print(
                f"Warning: Number of Gaussians was not square. Padded {n_pad} Gaussians."
            )
            for attr_name, splats_video in splats_videos.items():
                pad_shape = list(splats_video.shape)
                pad_shape[1] = n_pad
                if attr_name == "opacities":
                    pad_splats_video = -5 * torch.ones(pad_shape, dtype=splats_video.dtype, device=splats_video.device)
                elif attr_name == "scales":
                    pad_splats_video = -10 * torch.ones(pad_shape, dtype=splats_video.dtype, device=splats_video.device)
                else:
                    pad_splats_video = torch.zeros(pad_shape, dtype=splats_video.dtype, device=splats_video.device)

                splats_videos[attr_name] = torch.cat([splats_video, pad_splats_video], dim=1)
        
        return splats_videos
    
    def reorganize(self, splats_list: List[Dict]) -> Dict[str, Tensor]:
        # splat list to sequence of attributes
        seq_attr_dict = self.splats_list_to_attribute_seq(splats_list)
        # pad
        padded_splats_videos = self.pad_attr_seq(seq_attr_dict)
        
        # random access
        if not self.use_all_intra:
            if self.use_sort:
                ## get padded first splats
                ## sort and get indices
                sorted_indices = self.sort_with_frame_index(splats_list)

                ## use indices to sort the sequences of attributes
                for attr_name, padded_splats_video in padded_splats_videos.items():
                    padded_splats_videos[attr_name] = padded_splats_video[:, sorted_indices, ...]
        else: # all intra
            if self.use_sort:
                for fr_id, _ in enumerate(splats_list):
                    sorted_indices = self.sort_with_frame_index(splats_list, fr_id)

                    for attr_name, padded_splats_video in padded_splats_videos.items():
                        padded_splats_video[fr_id] = padded_splats_video[fr_id][sorted_indices, ...]

        # reshape to 2d sequences
        n_gs = padded_splats_videos["means"].size(1)
        n_sidelen = int(n_gs**0.5)
        self.splats_videos = {}
        for attr_name, padded_splats_video in padded_splats_videos.items(): 
            ori_shape = list(padded_splats_video.shape)
            new_shape = [ori_shape[0]] + [n_sidelen, n_sidelen] + ori_shape[2:]
            self.splats_videos[attr_name] = padded_splats_video.reshape(new_shape)

        # proprocessing on splats_videos
        self.splats_videos["quats"] = F.normalize(self.splats_videos["quats"], dim=-1)

        return self.splats_videos

    def deorganize(self, splats_videos_c: Dict[str, Tensor]) -> List[Dict]:
        flattened_splats_videos = {}
        for attr_name, splats_video in splats_videos_c.items():
            ori_shape = list(splats_video.shape)
            new_shape = [ori_shape[0], ori_shape[1] * ori_shape[2]] + ori_shape[3:]

            flattened_splats_videos[attr_name] = splats_video.reshape(new_shape)

        n_frames = flattened_splats_videos["means"].size(0)
        splats_list = []

        for frame_idx in range(n_frames):
            splat_dict = {}
            for attr_name, attr_seq in flattened_splats_videos.items():
                splat_dict[attr_name] = attr_seq[frame_idx, ...]
            
            splats_list.append(splat_dict)
        
        return splats_list
    
    def shift_sh_coefficients(self, splats: Dict[str, torch.Tensor]) -> None:
        shift_info = {}

        for attr_name in ("sh0", "shN"):
            if attr_name not in splats:
                continue
            
            tensor = splats[attr_name]  # [N, K, C]
            if tensor.dim() == 2:
                tensor = tensor.unsqueeze(1)  # [N, 1, C]

            N, K, C = tensor.shape
            tensor_rolled = []
            shifts_all = []

            for c in range(C):
                channel_tensor = tensor[:, :, c]  # [N, K]
                abs_channel_tensor = channel_tensor.abs()

                max_indices = torch.argmax(abs_channel_tensor, dim=-1)  # [N]
                shifts_all.append(max_indices)

                rolled_channel = torch.stack([
                    torch.roll(channel_tensor[i], -max_indices[i].item(), dims=0)
                    for i in range(N)
                ], dim=0)  # [N, K]

                tensor_rolled.append(rolled_channel.unsqueeze(-1))

            shifted_tensor = torch.cat(tensor_rolled, dim=-1)  # [N, K, C]
            splats[attr_name] = shifted_tensor

            shift_info[attr_name] = torch.stack(shifts_all, dim=-1).float()  # [N, C]
            

        # shift 정보를 저장
        if "sh0" in shift_info:
            splats["shift_sh0"] = shift_info["sh0"]
        if "shN" in shift_info:
            splats["shift_shN"] = shift_info["shN"]

    def inverse_shift_sh_coefficients(self, splats: Dict[str, torch.Tensor]) -> None:
        for attr_name in ("sh0", "shN"):
            shift_key = f"shift_{attr_name}"
            if attr_name not in splats or shift_key not in splats:
                continue

            tensor = splats[attr_name]  # [N, K, C]
            shifts = splats[shift_key].long()  # [N, C]

            N, K, C = tensor.shape
            restored = []

            for c in range(C):
                channel_tensor = tensor[:, :, c]  # [N, K]
                restored_channel = torch.stack([
                    torch.roll(channel_tensor[i], shifts[i, c].item(), dims=0)
                    for i in range(N)
                ], dim=0)
                restored.append(restored_channel.unsqueeze(-1))

            restored_tensor = torch.cat(restored, dim=-1)  # [N, K, C]
            splats[attr_name] = restored_tensor.float()


    def shift_scales(self, splats: Dict[str, torch.Tensor]) -> None:
        if "scales" not in splats:
            return

        scales = splats["scales"]  # [N, 3]
        N = scales.shape[0]

        # RGB 기준으로 가장 큰 값이 앞으로 오도록 roll
        max_indices = torch.argmax(scales.abs(), dim=1)  # [N]
        shifted_scales = torch.stack([
            torch.roll(scales[i], -max_indices[i].item(), dims=0) for i in range(N)
        ], dim=0)  # [N, 3]

        splats["scales"] = shifted_scales
        splats["shift_scales"] = max_indices.float()  # float으로 저장



    def inverse_shift_scales(self, splats: Dict[str, torch.Tensor]) -> None:
        if "scales" not in splats or "shift_scales" not in splats:
            return

        scales = splats["scales"]
        shifts = splats["shift_scales"].long()
        N = scales.shape[0]

        restored_scales = torch.stack([
            torch.roll(scales[i], shifts[i].item(), dims=0) for i in range(N)
        ], dim=0)

        splats["scales"] = restored_scales.float()


    
    def shift_quats(self, splats: Dict[str, torch.Tensor]) -> None:
        if "quats" not in splats:
            return

        quats = splats["quats"]  # [N, 4]
        N = quats.shape[0]

        max_indices = torch.argmax(quats.abs(), dim=1)
        shifted_quats = torch.stack([
            torch.roll(quats[i], -max_indices[i].item(), dims=0) for i in range(N)
        ], dim=0)

        splats["quats"] = shifted_quats
        splats["shift_quats"] = max_indices.float()


    
    def inverse_shift_quats(self, splats: Dict[str, torch.Tensor]) -> None:
        if "quats" not in splats or "shift_quats" not in splats:
            print("No quats or shift_quats found in splats.")
            return

        quats = splats["quats"]  # [N, 4]
        shifts = splats["shift_quats"].long()  # [N]

        N = quats.shape[0]
        restored_quats = torch.stack([
            torch.roll(quats[i], shifts[i].item(), dims=0) for i in range(N)
        ], dim=0)

        splats["quats"] = restored_quats.float()

    def compute_stats(self, tensor: torch.Tensor, name: str = "param") -> Dict[str, Any]:
        """
        통계량(분산, 최소, 최대)을 채널 별로 계산
        """
        # 마지막 차원을 기준으로 통계 계산
        N, K, C = tensor.shape  # 예: N=567724, K=15 (SH 개수), C=3 (RGB)
        flattened = tensor.reshape(-1, C)  # [N*K, C]
        stats = {}

        stats = {}
        for k in range(K):
            for c in range(C):
                data = tensor[:, k, c]
                var_val = torch.var(data).item()
                min_val = torch.min(data).item()
                max_val = torch.max(data).item()

                key = f"{name}_sh{k}_rgb{c}"
                stats[key] = {
                    "var": var_val,
                    "min": min_val,
                    "max": max_val
                }

                print(f"[{key}] var: {var_val:.6f}, min: {min_val:.6f}, max: {max_val:.6f}")

        print("\n")


def _pad_n_splats(splats: Dict[str, Tensor], n_pad: int) -> Dict[str, Tensor]:
    for k, v in splats.items():
        pad_shape = list(v.shape)
        pad_shape[0] += n_pad
        padded_v = torch.zeros(pad_shape, dtype=v.dtype, device=v.device)
        padded_v[:v.shape[0]] = v
        splats[k] = padded_v
    return splats

def _compress_video_hevc(
        compress_dir: str, 
        param_name: str, 
        params: Tensor, 
        n_sidelen: int, 
        qp: int = 10, 
        debug: bool = False,
        use_all_intra: bool = False
) -> Dict[str, Any]:
    import imageio.v2 as imageio
    n_frames = int(params.size(0))

    grid = params.reshape((n_frames, n_sidelen, n_sidelen, -1))
    mins = torch.amin(grid, dim=(0, 1, 2))
    maxs = torch.amax(grid, dim=(0, 1, 2))
    grid_norm = (grid - mins) / (maxs - mins)
    video_norm = grid_norm.detach().cpu().numpy()

    video = (video_norm * (2**8 - 1)).round().astype(np.uint8)
    if video.shape[-1] != 3:
        video = video[..., 0]
    np.save(os.path.join(compress_dir, f"{param_name}.npy"), video)

    # save each frame
    # if len(video.shape) == 2:  
    #     imageio.imwrite(os.path.join(compress_dir, f"{param_name}_frame000.png"), video)
    # else:  
    for i in range(n_frames):
        imageio.imwrite(os.path.join(compress_dir, f"{param_name}_frame{i:03d}.png"), video[i])
    
    # run ffmpeg libx265 to compress PNG file
    file_extension = ".265" if debug else ".mp4"
    video_file = os.path.join(compress_dir, f"{param_name}.{file_extension[1:]}")

    print(f"QP value of {param_name} is: {qp}")
    pix_fmt = "-pix_fmt gray" if param_name == "opacities" else ""
    intra_params = ":keyint=1:min-keyint=1:scenecut=0" if use_all_intra else ""
    cmd = f"ffmpeg -i {compress_dir}/{param_name}_frame%03d.png -c:v libx265 {pix_fmt} -x265-params \"qp={qp}{intra_params}\" {video_file}"

    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)

    # remove png files
    png_files = sorted(glob.glob(os.path.join(compress_dir, f"{param_name}_frame*.png")))
    for png_file in png_files:
        os.remove(png_file)
    
    meta = {
        "shape": list(params.shape),
        "dtype": str(params.dtype).split(".")[1],
        "mins": mins.tolist(),
        "maxs": maxs.tolist(),
        "file_extension": file_extension
    }
    return meta

def _decompress_video_hevc(compress_dir: str, param_name: str, meta: Dict[str, Any]):
    import imageio.v2 as imageio

    file_extension = meta["file_extension"]
    reader = imageio.get_reader(os.path.join(compress_dir, f"{param_name}.{file_extension[1:]}"), format='FFMPEG')

    frames = []
    for i, frame in enumerate(reader):
        frames.append(frame)
    
    video = np.stack(frames, axis=0)
    if param_name == "opacities":
        video = video[..., 0]
    
    # report the PSNR between reconstructed videos and original videos
    raw_video = np.load(os.path.join(compress_dir, f"{param_name}.npy"))
    cal_psnr = lambda x, y: float('inf') if (d := np.mean((x-y)**2)) == 0 else 20*np.log10(255) - 10*np.log10(d)
    print(f"PSNR of \"{param_name}\" map after video coding: {cal_psnr(raw_video, video)} dB")
    os.remove(os.path.join(compress_dir, f"{param_name}.npy"))

    video_norm = video / (2**8 - 1)

    grid_norm = torch.tensor(video_norm)
    mins = torch.tensor(meta["mins"])
    maxs = torch.tensor(meta["maxs"])
    grid = grid_norm * (maxs - mins) + mins

    params = grid.reshape(meta["shape"])
    params = params.to(dtype=getattr(torch, meta["dtype"]))
    return params

def _compress_video_hevc_16bit(
        compress_dir: str, 
        param_name: str, 
        params: Tensor, 
        n_sidelen: int, 
        qp: int = 10, 
        debug: bool = False,
        use_all_intra: bool = False
) -> Dict[str, Any]:
    import imageio.v2 as imageio
    n_frames = int(params.size(0))

    grid = params.reshape((n_frames, n_sidelen, n_sidelen, -1))
    mins = torch.amin(grid, dim=(0, 1, 2))
    maxs = torch.amax(grid, dim=(0, 1, 2))
    grid_norm = (grid - mins) / (maxs - mins)
    video_norm = grid_norm.detach().cpu().numpy()

    video = (video_norm * (2**16 - 1)).round().astype(np.uint16)
    np.save(os.path.join(compress_dir, f"{param_name}.npy"), video,)

    video_l = video & 0xFF
    video_u = (video >> 8) & 0xFF

    # save each frame
    for i in range(len(video)):
        imageio.imwrite(
            os.path.join(compress_dir, f"{param_name}_l_frame{i:03d}.png"), video_l[i].astype(np.uint8)
        )
        imageio.imwrite(
            os.path.join(compress_dir, f"{param_name}_u_frame{i:03d}.png"), video_u[i].astype(np.uint8)
        )
    
    for byte_select in ['l', 'u']:
        file_extension = ".265" if debug else ".mp4"
        video_file = os.path.join(compress_dir, f"{param_name}_{byte_select}.{file_extension[1:]}")

        # old
        intra_params = ":keyint=1:min-keyint=1:scenecut=0" if use_all_intra else ""
        cmd = f"ffmpeg -i {compress_dir}/{param_name}_{byte_select}_frame%03d.png -c:v libx265 -x265-params \"lossless=1:preset=veryslow{intra_params}\" {video_file}"
        
        # new
        # intra_params = "-intra" if use_all_intra else ""
        # cmd = f"ffmpeg -i {compress_dir}/{param_name}_{byte_select}_frame%03d.png -c:v libx265 {intra_params} -x265-params \"lossless=1:preset=veryslow\" {video_file}"

        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)

    # remove png files
    if not debug:
        png_files = sorted(glob.glob(os.path.join(compress_dir, f"{param_name}_*.png")))
        for png_file in png_files:
            os.remove(png_file)

    meta = {
        "shape": list(params.shape),
        "dtype": str(params.dtype).split(".")[1],
        "mins": mins.tolist(),
        "maxs": maxs.tolist(),
        "file_extension": file_extension
    }
    return meta

def _decompress_video_hevc_16bit(
        compress_dir: str, param_name: str, meta: Dict[str, Any]
) -> Tensor:
    import imageio.v2 as imageio

    file_extension = meta["file_extension"]
    reader_l = imageio.get_reader(os.path.join(compress_dir, f"{param_name}_l.{file_extension[1:]}"), format='FFMPEG')
    reader_u = imageio.get_reader(os.path.join(compress_dir, f"{param_name}_u.{file_extension[1:]}"), format='FFMPEG')

    frames = []    
    for i, (frame_l, frame_u) in enumerate(zip(reader_l, reader_u)):
        frame_u = frame_u.astype(np.uint16)
        frame = (frame_u << 8) + frame_l
        frames.append(frame)
    
    video = np.stack(frames, axis=0)

    # report the PSNR between reconstructed videos and original videos
    raw_video = np.load(os.path.join(compress_dir, f"{param_name}.npy"))
    cal_psnr = lambda x, y: float('inf') if (d := np.mean((x-y)**2)) == 0 else 20*np.log10(65535) - 10*np.log10(d)
    print(f"PSNR of \"{param_name}\" map after video coding: {cal_psnr(raw_video, video)} dB")
    os.remove(os.path.join(compress_dir, f"{param_name}.npy"))

    video_norm = video / (2**16 - 1)

    grid_norm = torch.tensor(video_norm)
    mins = torch.tensor(meta["mins"])
    maxs = torch.tensor(meta["maxs"])
    grid = grid_norm * (maxs - mins) + mins

    params = grid.reshape(meta["shape"])
    params = params.to(dtype=getattr(torch, meta["dtype"]))
    return params    

def _compress_quats_video_hevc(
        compress_dir: str, 
        param_name: str, 
        params: Tensor, 
        n_sidelen: int, 
        qp: int = 10, 
        debug: bool = False,
        use_all_intra: bool = True
) -> Dict[str, Any]:
    import imageio.v2 as imageio
    n_frames = int(params.size(0))

    grid = params.reshape((n_frames, n_sidelen, n_sidelen, -1))
    mins = torch.amin(grid, dim=(0, 1, 2))
    maxs = torch.amax(grid, dim=(0, 1, 2))
    grid_norm = (grid - mins) / (maxs - mins)
    video_norm = grid_norm.detach().cpu().numpy()

    video = (video_norm * (2**8 - 1)).round().astype(np.uint8)
    # video = video.squeeze()
    np.save(os.path.join(compress_dir, f"{param_name}.npy"), video,)

    video_w = video[..., 0] # [T, H, W]
    video_xyz = video[..., 1:] # [T, H, W, 3]

    for i in range(len(video)):
        imageio.imwrite(os.path.join(compress_dir, f"{param_name}_w_frame{i:03d}.png"), video_w[i])
        imageio.imwrite(os.path.join(compress_dir, f"{param_name}_xyz_frame{i:03d}.png"), video_xyz[i])

    # run ffmpeg libx265 to compress PNG file
    file_extension = ".265" if debug else ".mp4"
    intra_params = ":keyint=1:min-keyint=1:scenecut=0" if use_all_intra else ""

    video_file = os.path.join(compress_dir, f"{param_name}_w.{file_extension[1:]}")
    print(f"QP value of {param_name}_w is: {qp}")
    cmd = f"ffmpeg -i {compress_dir}/{param_name}_w_frame%03d.png -c:v libx265 -pix_fmt gray -x265-params \"qp={qp}{intra_params}\" {video_file}"
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)

    video_file = os.path.join(compress_dir, f"{param_name}_xyz.{file_extension[1:]}")
    print(f"QP value of {param_name}_xyz is: {qp}")
    cmd = f"ffmpeg -i {compress_dir}/{param_name}_xyz_frame%03d.png -c:v libx265 -x265-params \"qp={qp}{intra_params}\" {video_file}"
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)

    # remove png files
    if not debug:
        png_files = sorted(glob.glob(os.path.join(compress_dir, f"{param_name}_*.png")))
        for png_file in png_files:
            os.remove(png_file)
    
    meta = {
        "shape": list(params.shape),
        "dtype": str(params.dtype).split(".")[1],
        "mins": mins.tolist(),
        "maxs": maxs.tolist(),
        "file_extension": file_extension
    }
    return meta    

def _decompress_quats_video_hevc(
        compress_dir: str, param_name: str, meta: Dict[str, Any]
):
    import imageio.v2 as imageio

    file_extension = meta["file_extension"]
    reader_w = imageio.get_reader(os.path.join(compress_dir, f"{param_name}_w.{file_extension[1:]}"), format='FFMPEG')
    reader_xyz = imageio.get_reader(os.path.join(compress_dir, f"{param_name}_xyz.{file_extension[1:]}"), format='FFMPEG')

    frames = []
    for frame_w, frame_xyz in zip(reader_w, reader_xyz):
        frame = np.concatenate([frame_w[..., 0:1], frame_xyz], axis=-1)
        frames.append(frame)

    video = np.stack(frames, axis=0)

    # report the PSNR between reconstructed videos and original videos
    raw_video = np.load(os.path.join(compress_dir, f"{param_name}.npy"))
    cal_psnr = lambda x, y: float('inf') if (d := np.mean((x-y)**2)) == 0 else 20*np.log10(255) - 10*np.log10(d)
    print(f"PSNR of \"{param_name}\" map after video coding: {cal_psnr(raw_video, video)} dB")
    os.remove(os.path.join(compress_dir, f"{param_name}.npy"))   

    video_norm = video / (2**8 - 1)
    grid_norm = torch.tensor(video_norm)
    mins = torch.tensor(meta["mins"])
    maxs = torch.tensor(meta["maxs"])
    grid = grid_norm * (maxs - mins) + mins

    params = grid.reshape(meta["shape"])
    # ori_params = torch.load(os.path.join(compress_dir, "quats.ckpt"))

    params = params.to(dtype=getattr(torch, meta["dtype"]))
    return params    

def _compress_shN_video_hevc(
        compress_dir: str, 
        param_name: str, 
        params: Tensor, 
        n_sidelen: int, 
        qp: Dict[str, int], 
        debug: bool = False,
        use_all_intra: bool = False
) -> Dict[str, Any]:
    import imageio.v2 as imageio
    n_frames = int(params.size(0))

    shN_name_list = []
    for degree in range(1,4):
        for level in range(-degree, degree+1):
            shN_name_list.append(f"sh{degree}_{level}")

    grid = params # [T, H, W, 15, 3]
    mins = torch.amin(grid, dim=(0, 1, 2))
    maxs = torch.amax(grid, dim=(0, 1, 2))
    grid_norm = (grid - mins) / (maxs - mins)
    shN_norm = grid_norm.detach().cpu().numpy()

    shN_norm = (shN_norm * (2**8 - 1)).round().astype(np.uint8)
    # shN_norm = shN_norm.squeeze()

    for f_id in range(n_frames):
        for shN_id, shN_name in enumerate(shN_name_list):
            image = shN_norm[f_id,:,:,shN_id,:]
            imageio.imwrite(os.path.join(compress_dir, f"{param_name}_{shN_name}_frame{f_id:03d}.png"), image)
    
    file_extension = ".265" if debug else ".mp4"
    intra_params = ":keyint=1:min-keyint=1:scenecut=0" if use_all_intra else ""
    for shN_id, shN_name in enumerate(shN_name_list):
        print(f"QP value of {shN_name} is: {qp[shN_name[0:3]]}")
        video_file = os.path.join(compress_dir, f"{param_name}_{shN_name}.{file_extension[1:]}")
        cmd = f"ffmpeg -i {compress_dir}/{param_name}_{shN_name}_frame%03d.png -c:v libx265 -x265-params \"qp={qp[shN_name[0:3]]}{intra_params}\" {video_file}"
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    
    # remove png files
    if not debug:
        png_files = sorted(glob.glob(os.path.join(compress_dir, f"{param_name}_*.png")))
        for png_file in png_files:
            os.remove(png_file)
    
    meta = {
        "shape": list(params.shape),
        "dtype": str(params.dtype).split(".")[1],
        "mins": mins.tolist(),
        "maxs": maxs.tolist(),
        "file_extension": file_extension
    }
    return meta

def _decompress_shN_video_hevc(
        compress_dir: str, param_name: str, meta: Dict[str, Any]
):
    import imageio.v2 as imageio

    shN_name_list = []
    for degree in range(1,4):
        for level in range(-degree, degree+1):
            shN_name_list.append(f"sh{degree}_{level}")

    file_extension = meta["file_extension"]

    shN_reader_list = []
    for shN_name in shN_name_list:
        shN_reader_list.append(imageio.get_reader(os.path.join(compress_dir, f"{param_name}_{shN_name}.{file_extension[1:]}"), format='FFMPEG'))

    shN_video_list = []
    for shN_reader in shN_reader_list: # loop on shN components
        shN_frames = []
        for shN_frame in shN_reader: # loop on frames
            shN_frames.append(shN_frame)
        shN_video = np.stack(shN_frames, axis=0)
        shN_video_list.append(shN_video)
    shN_videos = np.stack(shN_video_list, axis=3)

    shN_norm = shN_videos / (2**8 -1)

    grid_norm = torch.tensor(shN_norm)
    mins = torch.tensor(meta["mins"])
    maxs = torch.tensor(meta["maxs"])
    grid = grid_norm * (maxs - mins) + mins

    params = grid.reshape(meta["shape"])
    params = params.to(dtype=getattr(torch, meta["dtype"]))
    return params 

def _compress_npz(
    compress_dir: str, param_name: str, params: Tensor, **kwargs
) -> Dict[str, Any]:
    """Compress parameters with numpy's NPZ compression."""
    npz_dict = {"arr": params.detach().cpu().numpy()}
    save_fp = os.path.join(compress_dir, f"{param_name}.npz")
    os.makedirs(os.path.dirname(save_fp), exist_ok=True)
    np.savez_compressed(save_fp, **npz_dict)
    meta = {
        "shape": params.shape,
        "dtype": str(params.dtype).split(".")[1],
    }
    return meta


def _decompress_npz(compress_dir: str, param_name: str, meta: Dict[str, Any]) -> Tensor:
    """Decompress parameters with numpy's NPZ compression."""
    arr = np.load(os.path.join(compress_dir, f"{param_name}.npz"))["arr"]
    params = torch.tensor(arr)
    params = params.reshape(meta["shape"])
    params = params.to(dtype=getattr(torch, meta["dtype"]))
    return params

def _compress_kmeans(
    compress_dir: str,
    param_name: str,
    params: Tensor,
    n_clusters: int = 65536,
    quantization: int = 8,
    verbose: bool = True,
    **kwargs,
) -> Dict[str, Any]:
    """Run K-means clustering on parameters and save centroids and labels to a npz file.

    .. warning::
        TorchPQ must installed to use K-means clustering.

    Args:
        compress_dir (str): compression directory
        param_name (str): parameter field name
        params (Tensor): parameters to compress
        n_clusters (int): number of K-means clusters
        quantization (int): number of bits in quantization
        verbose (bool, optional): Whether to print verbose information. Default to True.

    Returns:
        Dict[str, Any]: metadata
    """
    try:
        from torchpq.clustering import KMeans
    except:
        raise ImportError(
            "Please install torchpq with 'pip install torchpq' to use K-means clustering"
        )

    if torch.numel == 0:
        meta = {
            "shape": list(params.shape),
            "dtype": str(params.dtype).split(".")[1],
        }
        return meta
    
    x = params.reshape(params.shape[0], -1).permute(1, 0).contiguous()
    if n_clusters > x.shape[0]:
        if verbose:
            print(
                f"Warning: reducing n_clusters from {n_clusters} to {x.shape[0]} due to limited data"
            )
        n_clusters = x.shape[0]
    kmeans = KMeans(n_clusters=n_clusters, distance="manhattan", verbose=verbose)
    labels = kmeans.fit(x)
    labels = labels.detach().cpu().numpy()
    centroids = kmeans.centroids.permute(1, 0)

    mins = torch.min(centroids)
    maxs = torch.max(centroids)
    centroids_norm = (centroids - mins) / (maxs - mins)
    centroids_norm = centroids_norm.detach().cpu().numpy()
    centroids_quant = (
        (centroids_norm * (2**quantization - 1)).round().astype(np.uint8)
    )
    labels = labels.astype(np.uint16)

    npz_dict = {
        "centroids": centroids_quant,
        "labels": labels,
    }
    np.savez_compressed(os.path.join(compress_dir, f"{param_name}.npz"), **npz_dict)
    meta = {
        "shape": list(params.shape),
        "dtype": str(params.dtype).split(".")[1],
        "mins": mins.tolist(),
        "maxs": maxs.tolist(),
        "quantization": quantization,
    }
    return meta


def _decompress_kmeans(
    compress_dir: str, param_name: str, meta: Dict[str, Any], **kwargs
) -> Tensor:
    """Decompress parameters from K-means compression.

    Args:
        compress_dir (str): compression directory
        param_name (str): parameter field name
        meta (Dict[str, Any]): metadata

    Returns:
        Tensor: parameters
    """
    if not np.all(meta["shape"]):
        params = torch.zeros(meta["shape"], dtype=getattr(torch, meta["dtype"]))
        return meta

    npz_dict = np.load(os.path.join(compress_dir, f"{param_name}.npz"))
    centroids_quant = npz_dict["centroids"]
    labels = npz_dict["labels"].astype(np.int32) # uint16 -> int32

    centroids_norm = centroids_quant / (2 ** meta["quantization"] - 1)
    centroids_norm = torch.tensor(centroids_norm)
    mins = torch.tensor(meta["mins"])
    maxs = torch.tensor(meta["maxs"])
    centroids = centroids_norm * (maxs - mins) + mins

    params = centroids[labels]
    params = params.reshape(meta["shape"])
    params = params.to(dtype=getattr(torch, meta["dtype"]))
    return params


def _compress_masked_kmeans(
    compress_dir: str,
    param_name: str,
    params: Tensor,
    n_clusters: int = 32768, # 65536
    quantization: int = 8,
    verbose: bool = True,
    **kwargs,
) -> Dict[str, Any]:
    """Run K-means clustering on parameters and save centroids and labels to a npz file.

    .. warning::
        TorchPQ must installed to use K-means clustering.

    Args:
        compress_dir (str): compression directory
        param_name (str): parameter field name
        params (Tensor): parameters to compress
        n_clusters (int): number of K-means clusters
        quantization (int): number of bits in quantization
        verbose (bool, optional): Whether to print verbose information. Default to True.

    Returns:
        Dict[str, Any]: metadata
        
    """
    try:
        from torchpq.clustering import KMeans
    except:
        raise ImportError(
            "Please install torchpq with 'pip install torchpq' to use K-means clustering"
        )

    if torch.numel == 0:
        meta = {
            "shape": list(params.shape),
            "dtype": str(params.dtype).split(".")[1],
        }
        return meta
    
    # get mask and save mask
    mask = (params > 0).any(dim=1).any(dim=1).reshape(-1)
    mask_flat = mask.cpu().numpy().astype(bool)
    n = len(mask_flat)
    n_bytes = (n + 7) // 8  # 需要的字节数
    bits = np.packbits(mask_flat)[:n_bytes]  # 打包成bytes
    bits.tofile(os.path.join(compress_dir, f"mask.bin"))

    # select vaild shN
    masked_params = params[mask]
    x = masked_params.reshape(masked_params.shape[0], -1).permute(1, 0).contiguous()
    if n_clusters > x.shape[0]:
        if verbose:
            print(
                f"Warning: reducing n_clusters from {n_clusters} to {x.shape[0]} due to limited data"
            )
        n_clusters = x.shape[0]
    kmeans = KMeans(n_clusters=n_clusters, distance="manhattan", verbose=verbose)

    labels = kmeans.fit(x)
    labels = labels.detach().cpu().numpy()
    centroids = kmeans.centroids.permute(1, 0)

    mins = torch.min(centroids)
    maxs = torch.max(centroids)
    centroids_norm = (centroids - mins) / (maxs - mins)
    centroids_norm = centroids_norm.detach().cpu().numpy()
    centroids_quant = (
        (centroids_norm * (2**quantization - 1)).round().astype(np.uint8)
    )
    labels = labels.astype(np.uint16)
    npz_dict = {
        "centroids": centroids_quant,
        "labels": labels,
    }
    np.savez_compressed(os.path.join(compress_dir, f"{param_name}.npz"), **npz_dict)
    meta = {
        "shape": list(params.shape),
        "dtype": str(params.dtype).split(".")[1],
        "mins": mins.tolist(),
        "maxs": maxs.tolist(),
        "quantization": quantization,
        "mask_bits": n,
        "mask_byte": n_bytes
    }
    return meta


def _decompress_masked_kmeans(
    compress_dir: str, param_name: str, meta: Dict[str, Any], **kwargs
) -> Tensor:
    """Decompress parameters from K-means compression.

    Args:
        compress_dir (str): compression directory
        param_name (str): parameter field name
        meta (Dict[str, Any]): metadata

    Returns:
        Tensor: parameters
    """
    if not np.all(meta["shape"]):
        params = torch.zeros(meta["shape"], dtype=getattr(torch, meta["dtype"]))
        return meta
    
    # decode mask
    bits_loaded = np.fromfile(os.path.join(compress_dir, 'mask.bin'), dtype=np.uint8)
    mask_restored = np.unpackbits(bits_loaded)[:meta["mask_bits"]].astype(bool)
    mask = torch.from_numpy(mask_restored).reshape(meta["shape"][0])

    npz_dict = np.load(os.path.join(compress_dir, f"{param_name}.npz"))
    centroids_quant = npz_dict["centroids"]
    labels = npz_dict["labels"].astype(np.int32) # uint16 -> int32

    centroids_norm = centroids_quant / (2 ** meta["quantization"] - 1)
    centroids_norm = torch.tensor(centroids_norm)
    mins = torch.tensor(meta["mins"])
    maxs = torch.tensor(meta["maxs"])
    centroids = centroids_norm * (maxs - mins) + mins

    params = centroids[labels]
    null_params = torch.zeros(meta["shape"], dtype=params.dtype) # null tensor
    null_params[mask] = params.reshape([params.shape[0]] + meta["shape"][1:])
    params = null_params
    params = params.to(dtype=getattr(torch, meta["dtype"]))
    return params