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
import math

import matplotlib.pyplot as plt
import matplotlib.colors as mcolors

@dataclass
class SeqHevcCompressionFullSort:
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
    n_clusters: int = 1024
    debug: bool = False
    use_all_intra: bool = False

    attribute_codec_registry: InitVar[Optional[Dict[str, str]]] = None

    compress_fn_map: Dict[str, Callable] = field(default_factory=lambda: {
        "means": _compress_video_hevc_16bit,
        "scales": _compress_video_hevc,
        "quats": _compress_quats_video_hevc,  
        "opacities": _compress_video_hevc,
        "sh0": _compress_video_hevc,
        "shN": _compress_shN_video_hevc,
        # "shN": _compress_masked_kmeans,
    })

    decompress_fn_map: Dict[str, Callable] = field(default_factory=lambda: {
        "means": _decompress_video_hevc_16bit,
        "scales": _decompress_video_hevc,
        "quats": _decompress_quats_video_hevc,  
        "opacities": _decompress_video_hevc,
        "sh0": _decompress_video_hevc,
        "shN": _decompress_shN_video_hevc,
        # "shN": _decompress_masked_kmeans,
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
        # 1) normalize quats
        self.splats_videos["quats"] = F.normalize(self.splats_videos["quats"], dim=-1)

        meta = {}

        for name, tensor in self.splats_videos.items():

            fn = self._get_compress_fn(name)

            if name == "shN":
                # ────────────────────────────────────────
                # a) 원본 5D shape 기록
                orig_shape_shN   = list(tensor.shape)                    # [T, H, W, 15, 3]
                orig_shape_quats = list(self.splats_videos["quats"].shape)  # [T, H, W, 4]

                # b) flatten
                shN_flat   = tensor.reshape(-1, orig_shape_shN[-2], orig_shape_shN[-1])    # [N,15,3]
                quats_flat = self.splats_videos["quats"].reshape(-1, orig_shape_quats[-1])  # [N,4]

                # c) joint kmeans 호출
                info = fn(
                    compress_dir,
                    name,
                    shN=shN_flat,
                    quats=quats_flat,
                    n_clusters=self.n_clusters,
                    verbose=self.verbose
                )

                # d) **원본** shape 으로 덮어쓰기
                info["shape_shN"]   = orig_shape_shN
                info["shape_quats"] = orig_shape_quats

                meta[name] = info
                # ────────────────────────────────────────

            elif name == "quats":
                # joint clustering 에 포함됐으니까 skip
                fn(compress_dir, name, None)

            else:
                # 나머지 파라미터들 (means, scales, sh0, opacities 등)
                meta[name] = fn(
                    compress_dir,
                    name,
                    tensor,
                    n_sidelen=int(self.splats_videos["means"].size(1)),
                    qp=self.qp.get(name, 0),
                    debug=self.debug,
                    use_all_intra=self.use_all_intra
                )

        # meta.json 쓰기
        with open(os.path.join(compress_dir, "meta.json"), "w") as f:
            json.dump(meta, f)


    def decompress(self, compress_dir: str) -> Dict[str, Tensor]:
        meta = json.load(open(os.path.join(compress_dir, "meta.json")))
        out = {}
        
        # 각 속성 복원
        for name, m in meta.items():
            fn = self._get_decompress_fn(name)
            out[name] = fn(compress_dir, name, m)
            
            if name == "quats":
                # 쿼터니언 정규화 (안전장치)
                out[name] = F.normalize(out[name], dim=-1)
        
        return out

    
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
        """
        클러스터를 정사각형 블록으로 배치하고 클러스터 평균값으로 패딩하면서 가우시안 개수를 보존
        """
        # 1) 기본 데이터 준비
        seq_attr_dict = self.splats_list_to_attribute_seq(splats_list)
        padded_splats_videos = self.pad_attr_seq(seq_attr_dict)
        
        # 원본 가우시안 개수 저장 (복원 시 정확히 이만큼만 사용하기 위함)
        original_n_gaussians = seq_attr_dict["means"].shape[1]  # T, N, ...에서 N이 원본 가우시안 개수
        print(f"Original number of gaussians: {original_n_gaussians}")
        
        T, N = padded_splats_videos["means"].shape[:2]
        device = padded_splats_videos["means"].device
        
        if "shN" in padded_splats_videos and "quats" in padded_splats_videos and self.use_sort:
            print("Creating space-efficient 2D cluster packing with square grid output...")
            
            try:
                # 특성 벡터 생성
                shN_data = padded_splats_videos["shN"][0]
                quats_data = F.normalize(padded_splats_videos["quats"][0], dim=-1)
                
                x_sh = shN_data.reshape(N, -1)
                x_q = quats_data
                
                # 특성 정규화
                x_sh = F.normalize(x_sh, dim=-1)
                x_q = F.normalize(x_q, dim=-1)
                
                # 결합 특성 벡터
                x = torch.cat([x_sh, x_q], dim=1)
                x = torch.nan_to_num(x, nan=0.0, posinf=1.0, neginf=0.0)
                
                # K-means 클러스터링
                from torchpq.clustering import KMeans
                
                n_effective_clusters = self.n_clusters
                print(f"Using {n_effective_clusters} clusters")
                
                kmeans = KMeans(n_clusters=n_effective_clusters, distance="cosine", verbose=self.verbose)
                labels = kmeans.fit(x.t().contiguous()).cpu()
                
                # 클러스터별 인덱스 그룹화
                clusters = {}
                for i in range(len(labels)):
                    c = int(labels[i].item())
                    if c not in clusters:
                        clusters[c] = []
                    clusters[c].append(i)
                
                # 각 클러스터 정사각형 크기 계산 (최소 크기 1x1 보장)
                cluster_sizes = {}
                min_elements = float('inf')
                max_elements = 0
                empty_clusters = []
                
                for c, indices in clusters.items():
                    if len(indices) == 0:
                        empty_clusters.append(c)
                        cluster_sizes[c] = 1  # 빈 클러스터도 최소 1x1
                        continue
                        
                    min_elements = min(min_elements, len(indices))
                    max_elements = max(max_elements, len(indices))
                    
                    # 정사각형 한 변의 길이 계산 (제곱근 올림)
                    side = max(1, int(np.ceil(np.sqrt(len(indices)))))
                    cluster_sizes[c] = side
                
                if min_elements == float('inf'):
                    min_elements = 0
                    
                print(f"Cluster element counts - Min: {min_elements}, Max: {max_elements}")
                
                # 클러스터를 크기 기준으로 정렬 (큰 것부터)
                sorted_clusters = sorted([(c, cluster_sizes[c], len(clusters.get(c, []))) 
                                        for c in clusters.keys() if c not in empty_clusters],
                                    key=lambda x: x[1], reverse=True)
                
                # ===== 빈 패킹(Bin Packing) 알고리즘 적용 =====
                
                print("Using bin packing for efficient cluster layout...")
                
                # 초기 그리드 크기 예상 (나중에 필요시 확장)
                estimated_width = int(np.ceil(np.sqrt(N)) * 1.2)  # 여유 있게 시작
                
                # 빈 패킹 상태 관리
                # skyline 알고리즘 사용: 각 x 위치에서의 현재 높이 추적
                skyline = [0] * estimated_width
                max_height = 0
                max_width = 0
                
                # 클러스터 위치 저장
                cluster_positions = {}  # {cluster_id: (y_start, x_start, side)}
                
                # 각 클러스터를 배치
                for c, side, size in sorted_clusters:
                    # 이 클러스터에 가장 적합한 x 위치 찾기 (first-fit decreasing)
                    best_x = 0
                    best_height = float('inf')
                    
                    # 각 가능한 x 위치 검사
                    for x in range(estimated_width - side + 1):
                        # 이 위치의 현재 높이(이 영역에서 가장 높은 지점)
                        current_height = max(skyline[x:x+side])
                        
                        # 더 낮은 위치를 찾았다면 업데이트
                        if current_height < best_height:
                            best_height = current_height
                            best_x = x
                            
                            # 충분히 낮은 위치를 찾았다면 조기 종료
                            if current_height == 0:
                                break
                    
                    # 배치할 공간이 부족하면 그리드 확장
                    if best_x + side > estimated_width:
                        new_width = best_x + side + 10  # 여유공간 추가
                        skyline.extend([0] * (new_width - estimated_width))
                        estimated_width = new_width
                    
                    # 클러스터 배치 위치 저장
                    start_x = best_x
                    start_y = best_height
                    cluster_positions[c] = (start_y, start_x, side)
                    
                    # 스카이라인 업데이트
                    for x in range(start_x, start_x + side):
                        if x < estimated_width:
                            skyline[x] = start_y + side
                    
                    # 최대 사용 영역 업데이트
                    max_height = max(max_height, start_y + side)
                    max_width = max(max_width, start_x + side)
                    
                    print(f"Cluster {c}: Placed at ({start_x},{start_y}) with side {side} - Elements: {size}")
                
                # 빈 클러스터 처리 (필요한 경우)
                for c in empty_clusters:
                    # 빈 공간 찾기
                    for x in range(estimated_width):
                        if skyline[x] == 0:
                            # 빈 클러스터를 1x1로 배치
                            cluster_positions[c] = (0, x, 1)
                            skyline[x] = 1
                            max_width = max(max_width, x + 1)
                            max_height = max(max_height, 1)
                            print(f"Empty cluster {c}: Placed at ({x},0) with size 1x1")
                            break
                
                # 최종 그리드 크기를 정사각형으로 만듦
                grid_side = max(max_width, max_height)
                grid_width = grid_side
                grid_height = grid_side
                
                print(f"Final square grid size: {grid_side}x{grid_side}")
                
                # ===== 인덱스 매핑 생성 =====
                print("Step 1: Creating index mapping...")
                
                # 인덱스 맵핑 준비 (원본 -> 새 위치)
                index_mapping = torch.full((N,), -1, dtype=torch.long, device=device)
                
                # 클러스터별 요소 배치 위치 계산
                mapped_count = 0
                for c, (start_y, start_x, side) in cluster_positions.items():
                    indices = clusters.get(c, [])
                    
                    # 클러스터 요소 인덱스 맵핑 생성
                    idx = 0
                    for y in range(side):
                        for x in range(side):
                            grid_y = start_y + y
                            grid_x = start_x + x
                            
                            if 0 <= grid_y < grid_height and 0 <= grid_x < grid_width:
                                flat_idx = grid_y * grid_width + grid_x
                                
                                if idx < len(indices):
                                    # 실제 인덱스 맵핑
                                    orig_idx = indices[idx]
                                    index_mapping[orig_idx] = flat_idx
                                    idx += 1
                                    mapped_count += 1
                
                print(f"Successfully mapped {mapped_count}/{N} elements")
                
                # 매핑되지 않은 요소 처리
                unmapped = (index_mapping == -1).sum().item()
                if unmapped > 0:
                    print(f"Handling {unmapped} unmapped elements...")
                    unmapped_indices = torch.where(index_mapping == -1)[0]
                    
                    # 사용 가능한 위치 찾기
                    all_positions = set(range(grid_height * grid_width))
                    used_positions = set(index_mapping[index_mapping >= 0].cpu().numpy())
                    free_positions = list(all_positions - used_positions)
                    
                    # 매핑되지 않은 요소 할당
                    for i, idx in enumerate(unmapped_indices):
                        if i < len(free_positions):
                            index_mapping[idx] = free_positions[i]
                        else:
                            print(f"Error: No space for element {idx}")
                            index_mapping[idx] = 0
                
                # ===== 역매핑 생성 =====
                print("Step 2: Creating reverse mapping...")
                
                # 역매핑 생성 (새 위치 -> 원본)
                max_pos = grid_height * grid_width
                reverse_mapping = torch.full((max_pos,), -1, dtype=torch.long, device=device)
                
                for orig_idx, new_pos in enumerate(index_mapping):
                    if 0 <= new_pos < max_pos:
                        reverse_mapping[new_pos] = orig_idx
                
                # ===== 클러스터별 평균값 계산 =====
                print("Step 3: Computing cluster averages for improved padding...")
                
                # 1) 각 클러스터별 평균값 계산
                cluster_averages = {}
                first_frame_data = {attr: tensor[0] for attr, tensor in padded_splats_videos.items()}
                
                for c, indices in clusters.items():
                    if not indices:  # 빈 클러스터는 건너뜀
                        continue
                    
                    # 클러스터별 평균값 계산
                    cluster_avg = {}
                    for attr_name, data in first_frame_data.items():
                        # 클러스터에 속한 요소들의 데이터
                        cluster_elements = data[indices]
                        
                        # 평균 계산
                        if attr_name == "opacities":
                            # 투명도는 작은 값(-5)으로 패딩하는 것이 좋음
                            cluster_avg[attr_name] = torch.tensor(-5.0, device=device)
                        elif attr_name == "scales":
                            # 스케일은 작은 값(-10)으로 패딩
                            cluster_avg[attr_name] = torch.tensor(-10.0, device=device)
                        else:
                            # 나머지는 평균값으로
                            cluster_avg[attr_name] = cluster_elements.mean(dim=0)
                    
                    cluster_averages[c] = cluster_avg
                
                # ===== 패딩 처리 - 클러스터 평균값 사용 =====
                print("Step 4: Filling padding with cluster averages...")
                
                # 2) 각 클러스터의 패딩 위치 찾기
                padding_positions = {}  # {cluster_id: [positions]}
                
                for c, (start_y, start_x, side) in cluster_positions.items():
                    padding_positions[c] = []
                    indices = clusters.get(c, [])
                    idx = len(indices)  # 실제 데이터 이후부터는 패딩
                    
                    # 클러스터 영역 순회
                    for y in range(side):
                        for x in range(side):
                            # 클러스터 내 위치 계산
                            pos_idx = y * side + x
                            if pos_idx >= idx:  # 실제 데이터 이후는 패딩
                                grid_y = start_y + y
                                grid_x = start_x + x
                                
                                if 0 <= grid_y < grid_height and 0 <= grid_x < grid_width:
                                    flat_idx = grid_y * grid_width + grid_x
                                    padding_positions.setdefault(c, []).append(flat_idx)
                
                # 빈공간이 클러스터 영역 밖에 있을 수도 있음 (이를 처리)
                empty_positions = torch.where(reverse_mapping == -1)[0]
                if len(empty_positions) > 0:
                    print(f"Handling {len(empty_positions)} extra empty positions outside clusters...")
                    
                    # 가장 가까운 클러스터 찾기 (간단한 구현: 첫번째 클러스터에 할당)
                    first_cluster = next(iter(cluster_averages.keys())) if cluster_averages else None
                    if first_cluster is not None:
                        for pos in empty_positions:
                            padding_positions.setdefault("outside", []).append(pos.item())
                
                # ===== 재배치된 텐서 생성 =====
                print("Step 5: Creating reorganized tensors...")
                
                self.splats_videos = {}
                
                # 각 속성별로 처리
                for attr_name, tensor in padded_splats_videos.items():
                    print(f"Processing {attr_name}...")
                    shape = tensor.shape
                    rest_dims = shape[2:]  # 속성 차원
                    
                    # 새 텐서 준비
                    new_tensor = []
                    
                    # 프레임별 처리
                    for t in range(T):
                        # 원본 프레임 가져오기
                        frame_flat = tensor[t]  # [N, ...]
                        
                        # 새 형태로 재배치 (1D 먼저 생성)
                        frame_2d = torch.zeros((grid_height * grid_width, *rest_dims), 
                                            dtype=frame_flat.dtype, device=device)
                        
                        # 1. 유효한 데이터 먼저 할당
                        valid_mask = reverse_mapping >= 0
                        valid_indices = reverse_mapping[valid_mask]
                        frame_2d[valid_mask] = frame_flat[valid_indices]
                        
                        # 2. 클러스터별 패딩 위치에 평균값 할당
                        for c, positions in padding_positions.items():
                            if c == "outside":
                                # 클러스터 외부 패딩은 첫번째 클러스터 평균값 사용
                                first_cluster = next(iter(cluster_averages.keys())) if cluster_averages else None
                                if first_cluster is not None and positions:
                                    avg_value = cluster_averages[first_cluster][attr_name]
                                    for pos in positions:
                                        frame_2d[pos] = avg_value
                            elif c in cluster_averages and positions:
                                # 해당 클러스터 평균값으로 패딩
                                avg_value = cluster_averages[c][attr_name]
                                for pos in positions:
                                    frame_2d[pos] = avg_value
                        
                        # 2D 형태로 reshape
                        frame_2d = frame_2d.reshape(grid_height, grid_width, *rest_dims)
                        new_tensor.append(frame_2d.unsqueeze(0))
                    
                    # 모든 프레임 결합
                    self.splats_videos[attr_name] = torch.cat(new_tensor, dim=0)
                    print(f"Completed {attr_name} with shape {self.splats_videos[attr_name].shape}")
                
                # 매핑 정보 저장 (가우시안 갯수 정확하게 복원하기 위해 필수)
                self.cluster_grid_info = {
                    "grid_height": int(grid_height),
                    "grid_width": int(grid_width),
                    "grid_side": int(grid_side),
                    "cluster_positions": {str(k): [int(v[0]), int(v[1]), int(v[2])] 
                                        for k, v in cluster_positions.items()},
                    "cluster_sizes": {str(k): int(cluster_sizes.get(k, 1)) for k in clusters.keys()},
                    "cluster_elements": {str(k): len(clusters.get(k, [])) for k in clusters.keys()},
                    "total_elements_mapped": mapped_count,
                    "original_n_gaussians": original_n_gaussians  # 원본 가우시안 개수 저장
                }
                
                # 압축 코드와의 호환성을 위해 n_sidelen 정보 추가
                self.grid_height = grid_height
                self.grid_width = grid_width
                
            except Exception as e:
                import traceback
                print(f"Bin packing layout failed: {e}")
                print(traceback.format_exc())
                print("Using default layout")
                
                # 기본 정사각형 그리드로 변환
                n_sidelen = int(np.ceil(np.sqrt(N)))
                self.splats_videos = {}
                for attr_name, padded_splats_video in padded_splats_videos.items():
                    ori_shape = list(padded_splats_video.shape)
                    new_shape = [ori_shape[0]] + [n_sidelen, n_sidelen] + ori_shape[2:]
                    try:
                        self.splats_videos[attr_name] = padded_splats_video.reshape(new_shape)
                    except RuntimeError as e:
                        print(f"Error reshaping {attr_name}: {e}")
                        self.splats_videos[attr_name] = padded_splats_video
                
                # 매핑 정보 저장 (기본 레이아웃도 원본 개수 저장)
                self.cluster_grid_info = {
                    "grid_height": n_sidelen,
                    "grid_width": n_sidelen,
                    "original_n_gaussians": original_n_gaussians
                }
                self.grid_height = n_sidelen
                self.grid_width = n_sidelen
        else:
            # 기본 정사각형 그리드로 변환
            n_sidelen = int(np.ceil(np.sqrt(N)))
            self.splats_videos = {}
            for attr_name, padded_splats_video in padded_splats_videos.items():
                ori_shape = list(padded_splats_video.shape)
                new_shape = [ori_shape[0]] + [n_sidelen, n_sidelen] + ori_shape[2:]
                try:
                    self.splats_videos[attr_name] = padded_splats_video.reshape(new_shape)
                except RuntimeError as e:
                    print(f"Error reshaping {attr_name}: {e}")
                    self.splats_videos[attr_name] = padded_splats_video
            
            # 매핑 정보 저장 (기본 레이아웃도 원본 개수 저장)
            self.cluster_grid_info = {
                "grid_height": n_sidelen,
                "grid_width": n_sidelen,
                "original_n_gaussians": original_n_gaussians
            }
            self.grid_height = n_sidelen
            self.grid_width = n_sidelen
        
        # 쿼터니언 정규화
        if "quats" in self.splats_videos:
            self.splats_videos["quats"] = F.normalize(self.splats_videos["quats"], dim=-1)
        
        return self.splats_videos


    def deorganize(self, splats_videos: Dict[str, Tensor]) -> List[Dict]:
        """
        클러스터 평균값으로 채워진 2D 그리드에서 원본 가우시안만 복원
        """
        # 2D 그리드 -> 1D 변환 
        flat_splats_videos = {}
        for attr_name, splats_video in splats_videos.items():
            shape = splats_video.shape
            if len(shape) >= 3:  # 3차원 이상 텐서
                T, H, W = shape[0], shape[1], shape[2]
                rest_shape = shape[3:]
                # 2D -> 1D 변환
                flat_splats_videos[attr_name] = splats_video.reshape(T, H * W, *rest_shape).contiguous()
            else:
                # 이미 평탄화된 텐서
                flat_splats_videos[attr_name] = splats_video
        
        # 기본 정보 가져오기
        T = flat_splats_videos["means"].shape[0]
        total_grid_size = flat_splats_videos["means"].shape[1]
        device = flat_splats_videos["means"].device
        
        # 원본 가우시안 개수 (저장된 값 또는 기본값)
        original_n_gaussians = self.cluster_grid_info.get("original_n_gaussians", total_grid_size)
        print(f"Restoring original {original_n_gaussians} gaussians from grid of size {total_grid_size}")
        
        # 클러스터 그리드 정보 사용
        if hasattr(self, 'cluster_grid_info') and isinstance(self.cluster_grid_info, dict):
            print("Using cluster grid information for precise data restoration...")
            
            # 클러스터 위치 정보와 요소 수 가져오기
            cluster_positions = {}
            for c_str, pos in self.cluster_grid_info.get("cluster_positions", {}).items():
                c = int(c_str)
                if len(pos) >= 3:
                    cluster_positions[c] = (pos[0], pos[1], pos[2])
            
            # 클러스터별 요소 수 
            cluster_elements = self.cluster_grid_info.get("cluster_elements", {})
            
            # 유효 데이터 위치 추적 (패딩 제외)
            valid_positions = []
            grid_width = self.cluster_grid_info.get("grid_width", int(np.sqrt(total_grid_size)))
            
            # 각 클러스터마다 실제 데이터 위치 계산
            for c_str, count in cluster_elements.items():
                c = int(c_str)
                if c not in cluster_positions or count <= 0:
                    continue
                    
                # 클러스터 위치 정보
                start_y, start_x, side = cluster_positions[c]
                element_count = 0
                
                # 클러스터 내 요소 순회
                for y in range(side):
                    for x in range(side):
                        if element_count >= count:
                            break  # 이 클러스터의 실제 요소를 모두 찾음
                        
                        grid_y = start_y + y
                        grid_x = start_x + x
                        
                        if 0 <= grid_y < self.cluster_grid_info.get("grid_height", int(np.sqrt(total_grid_size))) and \
                        0 <= grid_x < grid_width:
                            flat_idx = grid_y * grid_width + grid_x
                            if flat_idx < total_grid_size:
                                valid_positions.append(flat_idx)
                                element_count += 1
                    
                    if element_count >= count:
                        break
            
            # 유효 위치 정렬 (일관성 유지)
            valid_positions.sort()
            
            # 실제 가우시안 수가 원본과 일치하는지 확인
            if len(valid_positions) != original_n_gaussians:
                print(f"Warning: Identified {len(valid_positions)} valid positions, " 
                    f"but original had {original_n_gaussians} gaussians")
                
                # 유효 위치 수 조정 (원본 개수에 맞게)
                if len(valid_positions) > original_n_gaussians:
                    # 초과분 제거
                    valid_positions = valid_positions[:original_n_gaussians]
                else:
                    # 부족분 처리 (복제 또는 기본값으로 채우기)
                    while len(valid_positions) < original_n_gaussians:
                        # 순환하여 채우기
                        idx_to_add = valid_positions[len(valid_positions) % len(valid_positions)] if valid_positions else 0
                        valid_positions.append(idx_to_add)
            
            print(f"Using {len(valid_positions)} valid positions for restoring original gaussians")
            
            # 유효 위치 텐서 생성
            valid_positions_tensor = torch.tensor(valid_positions, dtype=torch.long, device=device)
            
            # 유효 데이터만 추출
            reordered_data = {}
            for attr_name, tensor in flat_splats_videos.items():
                reordered = []
                for t in range(T):
                    # 유효 위치의 데이터만 선택
                    frame = tensor[t]
                    valid_data = torch.index_select(frame, 0, valid_positions_tensor)
                    reordered.append(valid_data.unsqueeze(0))
                reordered_data[attr_name] = torch.cat(reordered, dim=0)
        
        else:
            # 이전 방식: 단순히 앞에서부터 원본 개수만큼 선택
            print(f"No specific mapping info - using first {original_n_gaussians} elements")
            reordered_data = {}
            for attr_name, tensor in flat_splats_videos.items():
                reordered = []
                for t in range(T):
                    # 앞에서부터 원본 개수만큼만 가져옴
                    valid_data = tensor[t, :original_n_gaussians]
                    reordered.append(valid_data.unsqueeze(0))
                reordered_data[attr_name] = torch.cat(reordered, dim=0)
        
        # 결과 리스트 생성
        result = []
        for t in range(T):
            frame_dict = {}
            for attr_name, tensor in reordered_data.items():
                frame_dict[attr_name] = tensor[t]
            
            # 쿼터니언 정규화
            if "quats" in frame_dict:
                frame_dict["quats"] = F.normalize(frame_dict["quats"], dim=-1)
            
            result.append(frame_dict)
        
        return result



    def compress(self, compress_dir: str) -> None:
        # 쿼터니언 정규화 (안전장치)
        if "quats" in self.splats_videos:
            self.splats_videos["quats"] = F.normalize(self.splats_videos["quats"], dim=-1)
        
        meta = {}
        
        # 압축 수행
        for name, tensor in self.splats_videos.items():
            fn = self._get_compress_fn(name)
            
            if name == "shN":
                # SH 계수 비디오 압축
                meta[name] = fn(
                    compress_dir,
                    name,
                    tensor,
                    n_sidelen=tensor.size(1),  # 이미 2D 그리드로 변환된 상태
                    qp=self.qp.get("shN", {"sh1": 20, "sh2": 24, "sh3": 28}),
                    debug=self.debug,
                    use_all_intra=self.use_all_intra
                )
                
                # 클러스터링 사용 여부 저장
                if hasattr(self, 'cluster_info'):
                    meta[name]["clustered"] = True
                    
            elif name == "quats":
                # 쿼터니언 비디오 압축
                meta[name] = fn(
                    compress_dir,
                    name,
                    tensor,
                    n_sidelen=tensor.size(1),
                    qp=self.qp.get("quats", 4),
                    debug=self.debug,
                    use_all_intra=self.use_all_intra
                )
                
            else:
                # 다른 파라미터들은 기존대로 압축
                meta[name] = fn(
                    compress_dir,
                    name,
                    tensor,
                    n_sidelen=tensor.size(1),
                    qp=self.qp.get(name, 0),
                    debug=self.debug,
                    use_all_intra=self.use_all_intra
                )
        
        # 메타데이터 저장
        with open(os.path.join(compress_dir, "meta.json"), "w") as f:
            json.dump(meta, f)

    

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
    shN: Tensor = None,
    quats: Tensor = None,
    n_clusters: int = 32768,
    quantization: int = 8,
    verbose: bool = True,
    **kwargs
) -> Dict[str, Any]:
    """K-means 클러스터링으로 shN과 quats를 압축"""
    try:
        from torchpq.clustering import KMeans
    except:
        raise ImportError(
            "Please install torchpq with 'pip install torchpq' to use K-means clustering"
        )

    # shN이 None이거나 비어있는 경우
    if shN is None or shN.numel() == 0:
        meta = {"shape": [], "dtype": "float32"}
        return meta
    
    # 원본 형태 저장
    orig_shape_shN = shN.shape
    
    # 5D 텐서([T, H, W, 15, 3])를 3D 텐서([N, 15, 3])로 변환
    # 여기서 N = T*H*W
    N = shN.shape[0] * shN.shape[1] * shN.shape[2]
    shN_reshaped = shN.reshape(N, shN.shape[3], shN.shape[4])
    
    # 마스크 생성 - 유효한 데이터만 선택 (모든 값이 0이 아닌 경우)
    mask = (shN_reshaped.abs().sum(dim=-1).sum(dim=-1) > 0)  # [N]
    mask_flat = mask.cpu().numpy().astype(bool)
    n = len(mask_flat)
    n_bytes = (n + 7) // 8
    bits = np.packbits(mask_flat)[:n_bytes]
    bits.tofile(os.path.join(compress_dir, f"mask.bin"))
    
    # 유효한 shN 데이터만 선택
    masked_shN = shN_reshaped[mask]  # [M, 15, 3] where M < N
    
    # 유효한 데이터가 충분히 있는지 확인
    if len(masked_shN) == 0:
        print("Warning: No valid data found in shN. Creating dummy data.")
        meta = {
            "shape": list(orig_shape_shN),
            "dtype": str(shN.dtype).split(".")[1],
            "mask_bits": n,
            "mask_byte": n_bytes
        }
        return meta
    
    # shN을 특성 벡터로 변환 (펼치기)
    x_sh = masked_shN.reshape(masked_shN.shape[0], -1)  # [M, 45]
    
    # 쿼터니언이 있는 경우 함께 처리
    if quats is not None:
        # 쿼터니언도 동일하게 reshape
        quats_reshaped = quats.reshape(N, quats.shape[-1])  # [N, 4]
        masked_quats = quats_reshaped[mask]  # [M, 4]
        
        # 쿼터니언 정규화
        masked_quats = F.normalize(masked_quats, dim=-1)
        
        # shN과 쿼터니언 결합
        x = torch.cat([x_sh, masked_quats], dim=1)  # [M, 49]
    else:
        x = x_sh  # [M, 45]
    
    # 특성 정규화
    mins = torch.min(x, dim=0)[0]
    maxs = torch.max(x, dim=0)[0]
    
    # 0으로 나누기 방지
    range_vals = maxs - mins
    range_vals[range_vals == 0] = 1.0
    
    x_norm = (x - mins) / range_vals
    
    # 클러스터 수 조정
    effective_clusters = min(n_clusters, x_norm.shape[0])
    if effective_clusters < n_clusters and verbose:
        print(f"Warning: reducing clusters from {n_clusters} to {effective_clusters}")
    
    # K-means 클러스터링 수행
    kmeans = KMeans(n_clusters=effective_clusters, distance="manhattan", verbose=verbose)
    x_t = x_norm.t().contiguous()  # torchpq는 [D, N] 형태 필요
    labels = kmeans.fit(x_t)
    labels = labels.detach().cpu().numpy()
    
    # 센트로이드 변환 및 양자화
    centroids = kmeans.centroids.permute(1, 0)  # [K, D]
    centroids_norm = (centroids * (2**quantization - 1)).round().cpu().numpy().astype(np.uint8)  
    labels = labels.astype(np.uint16)  
    
    # 결과 저장  
    np.savez_compressed(  
        os.path.join(compress_dir, f"{param_name}.npz"),  
        centroids=centroids_norm,  
        labels=labels  
    )  
    
    # 메타데이터 구성
    meta = {
        "shape": list(orig_shape_shN),
        "dtype": str(shN.dtype).split(".")[1],
        "mins": mins.tolist(),
        "maxs": maxs.tolist(),
        "quantization": quantization,
        "mask_bits": n,
        "mask_byte": n_bytes,
        "combined": quats is not None
    }
    
    return meta


def _decompress_masked_kmeans(
    compress_dir: str, param_name: str, meta: Dict[str, Any], **kwargs
) -> Tensor:
    """Decompress parameters from K-means compression."""
    # 원본 shape 가져오기
    shape = meta.get("shape", [])
    if not np.all(shape):
        return torch.zeros(0, dtype=torch.float32)
    
    # 마스크 복원
    bits_loaded = np.fromfile(os.path.join(compress_dir, 'mask.bin'), dtype=np.uint8)
    mask_restored = np.unpackbits(bits_loaded)[:meta["mask_bits"]].astype(bool)
    mask = torch.from_numpy(mask_restored)
    
    # NPZ 로드
    npz_dict = np.load(os.path.join(compress_dir, f"{param_name}.npz"))
    centroids_quant = npz_dict["centroids"]
    labels = npz_dict["labels"].astype(np.int32)
    
    # 센트로이드 역정규화 - 여기서 데이터 타입을 명시적으로 float32로 지정
    centroids_norm = centroids_quant / (2 ** meta["quantization"] - 1)
    centroids_norm = torch.tensor(centroids_norm, dtype=torch.float32)
    mins = torch.tensor(meta["mins"], dtype=torch.float32)
    maxs = torch.tensor(meta["maxs"], dtype=torch.float32)
    centroids = centroids_norm * (maxs - mins) + mins
    
    # 클러스터 인덱스로 값 복원
    params_flat = centroids[labels]
    
    # combined 모드인 경우 (shN + quats)
    combined = meta.get("combined", False)
    if combined:
        # 피처 벡터 분리
        sh_dim = 15 * 3
        sh_values = params_flat[:, :sh_dim]
        
        # 원본 shape에서 필요한 값 계산
        T, H, W = shape[0], shape[1], shape[2]
        N = T * H * W
        
        # shN 복원 (데이터 타입 명시)
        null_out = torch.zeros([N, 15, 3], dtype=torch.float32)
        null_out[mask] = sh_values.reshape(-1, 15, 3)
        
        # 원래 5D 모양으로 복원
        return null_out.reshape(shape)
    else:
        # 일반 모드 (shN만)
        sh_values = params_flat
        
        # 5D shape 계산
        T, H, W = shape[0], shape[1], shape[2]
        N = T * H * W
        
        # 데이터 타입 명시적 지정
        null_out = torch.zeros([N, 15, 3], dtype=torch.float32)
        null_out[mask] = sh_values.reshape(-1, 15, 3)
        
        # 원래 5D 모양으로 복원
        return null_out.reshape(shape)
