import json
import os
import subprocess
from dataclasses import dataclass, field, InitVar
import glob
import time
import shutil
from typing import Any, Callable, Dict, List, Optional, Union, Literal

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

@dataclass
class SeqHevcCompression:
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
        "quats": _compress_quats_video_hevc,
        "opacities": _compress_video_hevc,
        "sh0": _compress_video_hevc,
        "shN": _compress_shN_video_hevc
        # "shN": _compress_masked_kmeans,
    })
    decompress_fn_map: Dict[str, Callable] = field(default_factory=lambda: {
        "means": _decompress_video_hevc_16bit,
        "scales": _decompress_video_hevc,
        "quats": _decompress_quats_video_hevc,
        "opacities": _decompress_video_hevc,
        "sh0": _decompress_video_hevc,
        "shN": _decompress_shN_video_hevc
        # "shN": _decompress_masked_kmeans,
    })

    transform_attributes: Dict[str, bool] = field(default_factory=lambda: {
        "means": False,
        "opacities": False,
        "scales": False,
        "quats": True,
        "sh0": True,
        "shN": True
    })

    def __post_init__(self, attribute_codec_registry):
        if attribute_codec_registry:
            available_functions = {
                "_compress_video_hevc_16bit": _compress_video_hevc_16bit,
                "_compress_video_hevc": _compress_video_hevc,
                "_compress_quats_video_hevc": _compress_quats_video_hevc,
                "_compress_shN_video_hevc": _compress_shN_video_hevc,
                # "_compress_masked_kmeans": _compress_masked_kmeans,

                "_decompress_video_hevc_16bit": _decompress_video_hevc_16bit,
                "_decompress_video_hevc": _decompress_video_hevc,
                "_decompress_quats_video_hevc": _decompress_quats_video_hevc,
                "_decompress_shN_video_hevc": _decompress_shN_video_hevc,
                "_decompress_video_hevc_opencv": _decompress_video_hevc_opencv,
                "_decompress_video_hevc_16bit_opencv": _decompress_video_hevc_16bit_opencv,
                "_decompress_quats_video_hevc_opencv": _decompress_quats_video_hevc_opencv,
                "_decompress_shN_video_hevc_opencv": _decompress_shN_video_hevc_opencv,
                # "_decompress_masked_kmeans": _decompress_masked_kmeans,
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
    
    def param_transform(self, splats_list: List[Dict], transform_attributes: Dict[str, bool]) -> List[Dict]:
        """Transform the splats parameters to new domain
        e.g.  
        for SH coefficients, transform them from RGB domain to YCbCr domain
        for quats:
            1) normalize to unit quaternion
            2) transform to Euler angles or other equivalent representations (Optional, depends on kwargs)
    
        Args:                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              
            splats_list (List[Dict]): splats parameters of all frames
            transform_attributes (Dict[str, bool]): indicate attributes whether to transform domain
        """
        transformed_splats_list = []
        
        # Define the RGB to YCbCr transformation matrix
        # Using BT.709 (Rec. 709) standard, which is the most widely used for HD content
        # Reference: ITU-R BT.709-6 (06/2015)
        # 
        # Key characteristics:
        # - Designed for HD and Full HD content (1080p)
        # - Uses D65 white point
        # - Assumes RGB values are in range [0,1]
        # - Output Y in range [0,1], Cb/Cr in range [-0.5,0.5]
        #
        # Matrix explanation:
        # [Y]   [ 0.299   0.587   0.114 ] [R]   # Luma (Y) coefficients sum to 1
        # [Cb] = [-0.169  -0.331   0.5  ] [G]   # Cb (B-Y) coefficients sum to 0
        # [Cr]   [ 0.5    -0.419  -0.081] [B]   # Cr (R-Y) coefficients sum to 0
        #
        # Note: For different standards, use:
        # - BT.601 (SD content): Different coefficients for Y
        #   [0.299, 0.587, 0.114] - Same as BT.709 for historical reasons
        # - BT.2020 (4K/8K content): 
        #   [0.2627, 0.6780, 0.0593] for Y

        rgb_to_ycbcr = torch.tensor([
            [ 0.299,    0.587,    0.114  ],  # Y  = 0.299R  + 0.587G  + 0.114B
            [-0.169,   -0.331,    0.5    ],  # Cb = -0.169R - 0.331G  + 0.5B
            [ 0.5,     -0.419,   -0.081  ]   # Cr = 0.5R    - 0.419G  - 0.081B
        ], dtype=torch.float32)
        
        for splats in splats_list:
            transformed_splats = {}
            
            # Process all keys in the splats dictionary
            for key, value in splats.items():
                # Handle quaternions - normalize to unit quaternions
                if key == "quats" and transform_attributes[key]:
                    transformed_splats[key] = F.normalize(value, dim=-1)
                
                # Handle SH coefficients - transform from RGB to YCbCr
                elif key in ["sh0", "shN"] and transform_attributes[key]:
                    # Get the RGB values
                    rgb_data = value
                    
                    # Determine the tensor shape to handle both sh0 and shN correctly
                    original_shape = rgb_data.shape
                    last_dim = original_shape[-1]
                    
                    # Ensure we have RGB in the last dimension
                    if last_dim == 3:
                        # Reshape to make the RGB dimension the last dimension and all others flattened
                        rgb_flat = rgb_data.reshape(-1, 3)
                        
                        # Convert RGB -> YCbCr using matrix multiplication
                        # (N, 3) × (3, 3)T = (N, 3)
                        rgb_to_ycbcr = rgb_to_ycbcr.to(rgb_flat.device, rgb_flat.dtype)
                        ycbcr_flat = torch.matmul(rgb_flat, rgb_to_ycbcr.t())
                        
                        # Reshape back to original shape
                        ycbcr_data = ycbcr_flat.reshape(original_shape)
                        
                        # Store transformed data
                        transformed_splats[key] = ycbcr_data
                    else:
                        # If not RGB data, keep as is
                        transformed_splats[key] = value
                
                # For all other attributes, keep as is
                else:
                    transformed_splats[key] = value
            
            transformed_splats_list.append(transformed_splats)
        
        return transformed_splats_list
    
    def param_inverse_transform(self, splats_list: List[Dict], transform_attributes: Dict[str, bool]) -> List[Dict]:
        """Inverse transform the splats parameters to original domain
        
        Args:
            splats_list (List[Dict]): splats parameters of all frames
            transform_attributes (Dict[str, bool]): indicate attributes whether to transform domain
        """
        inverse_transformed_splats_list = []
        
        # Define the YCbCr to RGB transformation matrix
        # Using BT.709 inverse transformation matrix
        # This matrix is the inverse of the RGB to YCbCr matrix
        # 
        # Matrix explanation:
        # [R]   [1.0      0.0       1.402   ] [Y]    # Y contributes to all RGB
        # [G] = [1.0     -0.344136 -0.714136] [Cb]   # Cb affects G and B
        # [B]   [1.0      1.772     0.0     ] [Cr]   # Cr affects R and G
        #
        # The coefficients are derived to ensure:
        # 1. Perfect reconstruction (lossless conversion)
        # 2. Proper handling of the Cb/Cr ranges [-0.5,0.5]
        # 3. Maintains the BT.709 color primaries

        # TODO: check if this is correct?
        ycbcr_to_rgb = torch.tensor([
            [1.0,     0.0,      1.402   ],  # R = Y + 0.0Cb + 1.402Cr
            [1.0,    -0.344136, -0.714136],  # G = Y - 0.344136Cb - 0.714136Cr
            [1.0,     1.772,    0.0     ]   # B = Y + 1.772Cb + 0.0Cr
        ], dtype=torch.float32)
        
        for splats in splats_list:
            inverse_transformed_splats = {}
            
            # Process all keys in the splats dictionary
            for key, value in splats.items():
                # Handle SH coefficients - transform from YCbCr back to RGB
                if key in ["sh0", "shN"] and transform_attributes[key]:
                    # Get the YCbCr values
                    ycbcr_data = value
                    
                    # Determine the tensor shape to handle both sh0 and shN correctly
                    original_shape = ycbcr_data.shape
                    last_dim = original_shape[-1]
                    
                    # Ensure we have YCbCr in the last dimension
                    if last_dim == 3:
                        # Reshape to make the YCbCr dimension the last dimension and all others flattened
                        ycbcr_flat = ycbcr_data.reshape(-1, 3)
                        
                        # Convert YCbCr -> RGB using matrix multiplication
                        # (N, 3) × (3, 3)T = (N, 3)
                        ycbcr_to_rgb = ycbcr_to_rgb.to(ycbcr_flat.device, ycbcr_flat.dtype)
                        rgb_flat = torch.matmul(ycbcr_flat, ycbcr_to_rgb.t())
                        
                        # Reshape back to original shape
                        rgb_data = rgb_flat.reshape(original_shape)
                        
                        # Store inverse transformed data
                        inverse_transformed_splats[key] = rgb_data
                    else:
                        # If not YCbCr data, keep as is
                        inverse_transformed_splats[key] = value
                
                # For all other attributes, keep as is (quaternions already normalized)
                else:
                    inverse_transformed_splats[key] = value
            
            inverse_transformed_splats_list.append(inverse_transformed_splats)
        
        return inverse_transformed_splats_list
    
    def compress(self, splats_videos: Dict[str, Tensor], compress_dir: str, gop_id: int = 0) -> None:
        """Run compression

        Args:
            splats_videos (Dict[str, Tensor]): dictionary of splats videos in Tensor format
            compress_dir (str): directory to save compressed files
            gop_id (int): gop id
        """

        # Param-specific preprocessing
        # splats["means"] = log_transform(splats["means"])
        # splats_videos["quats"] = F.normalize(splats_videos["quats"], dim=-1) # already normalized in the reorganize function

        meta = {}
        for param_name in splats_videos.keys():
            compress_fn = self._get_compress_fn(param_name)
            kwargs = {
                "n_sidelen": int(splats_videos["means"].size(1)),
                "qp": self.qp[param_name],
                "use_all_intra": self.use_all_intra,
                "debug": self.debug,
                "gop_id": gop_id
            }
            meta[param_name] = compress_fn(
                compress_dir, param_name, splats_videos[param_name], **kwargs
            )

        with open(os.path.join(compress_dir, f"gop{gop_id}_meta.json"), "w") as f:
            json.dump(meta, f)

    def decompress(self, compress_dir: str, gop_id: int = 0) -> Dict[str, Tensor]:
        """Run decompression

        Args:
            compress_dir (str): directory that contains compressed files

        Returns:
            Dict[str, Tensor]: decompressed Gaussian splats videos
        """
        with open(os.path.join(compress_dir, f"gop{gop_id}_meta.json"), "r") as f:
            meta = json.load(f)

        splats = {}
        decoding_times = {}
        # Record the start time of decompression
        total_start_time = time.time()

        for param_name, param_meta in meta.items():
            start_time = time.time()

            decompress_fn = self._get_decompress_fn(param_name)
            splats[param_name] = decompress_fn(compress_dir, param_name, param_meta)

            decoding_time = time.time() - start_time
            decoding_times[param_name] = decoding_time
            if self.verbose:
                print(f"Decoding time of {param_name} is: {decoding_time} s")
        
        # Record the end time of decompression
        total_end_time = time.time()
        total_decoding_time = total_end_time - total_start_time
        print(f"Total decoding time is: {total_decoding_time} s")

        decoding_stats_path = os.path.join(os.path.dirname(compress_dir), "stats", "decoding_times.json")
        with open(decoding_stats_path, "w") as fp:
            # Add total time to the decoding time dictionary
            decoding_times["total"] = total_decoding_time
            json.dump(decoding_times, fp, indent=4)

        # Param-specific postprocessing
        # splats["means"] = inverse_log_transform(splats["means"])
        return splats
    
    def sort_with_frame_index(self, splats_list: List[Dict], frame_id: int = 0) -> Tensor:
        """Organize the list of splats into several sequences of attributs

        Args:

        """
        splats_to_be_sorted = splats_list[frame_id]

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
    
    def reorganize(self, splats_list: List[Dict], gop_id: int = 0) -> Dict[str, Tensor]:
        '''
        Organize the list of splats into a dictionary of splats videos.
        Args:
            splats_list (List[Dict]): list of splats
            gop_id (int): gop id

        Returns:
            Dict[str, Tensor]: dictionary of splats videos in Tensor format
        '''
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
        splats_videos = {}
        for attr_name, padded_splats_video in padded_splats_videos.items(): 
            ori_shape = list(padded_splats_video.shape)
            new_shape = [ori_shape[0]] + [n_sidelen, n_sidelen] + ori_shape[2:]
            splats_videos[attr_name] = padded_splats_video.reshape(new_shape)

            print(attr_name, padded_splats_video.shape)

        # proprocessing on splats_videos
        splats_videos["quats"] = F.normalize(splats_videos["quats"], dim=-1)

        return splats_videos

    def deorganize(self, splats_videos_c: Dict[str, Tensor]) -> List[Dict]:
        '''
        Deorganize the dictionary of splats videos into a list of splats.
        Args:
            splats_videos_c (Dict[str, Tensor]): dictionary of splats videos in Tensor format

        Returns:
            List[Dict]: list of splats
        '''
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
    

def _pad_n_splats(splats: Dict[str, Tensor], n_pad: int) -> Dict[str, Tensor]:
    for k, v in splats.items():
        pad_shape = list(v.shape)
        pad_shape[0] += n_pad
        padded_v = torch.zeros(pad_shape, dtype=v.dtype, device=v.device)
        padded_v[:v.shape[0]] = v
        splats[k] = padded_v
    return splats

# def _ndarray_to_YUV_file(video: np.ndarray, 
#                          compress_dir: str, 
#                          param_name: str, 
#                          chroma_subsampling: Literal["420", "444"] = "420"):
#     '''
#     Transform a nd.arrray which is a video in the shape of [T, H, W, 3] to YUV format

#     Args:
#         video (np.ndarray): video in the shape of [T, H, W, 3]
#         compress_dir (str): directory to save the YUV file
#         param_name (str): name of the parameter
#         chroma_subsampling (str): chroma subsampling format
#     '''
#     if chroma_subsampling == "420":
#         # do chroma subsampling
        

# def _YUV_file_to_ndarray(video_yuv: np.ndarray, 
#                          compress_dir: str, 
#                          param_name: str, 
#                          chroma_subsampling: Literal["420", "444"] = "420"):
#     '''
#     Load a YUV file and transform it to a nd.array which is a video in the shape of [T, H, W, 3]

#     Args:
#         video_yuv (np.ndarray): video in the shape of [T, H, W, 3]
#         compress_dir (str): directory to save the YUV file
#         param_name (str): name of the parameter
#         chroma_subsampling (str): chroma subsampling format
#     '''



def _compress_video_hevc(
        compress_dir: str, 
        param_name: str, 
        params: Tensor, 
        n_sidelen: int, 
        qp: int = 10, 
        debug: bool = False,
        use_all_intra: bool = False,
        gop_id: int = 0
) -> Dict[str, Any]:
    import imageio.v2 as imageio
    n_frames = int(params.size(0))

    grid = params.reshape((n_frames, n_sidelen, n_sidelen, -1))
    mins = torch.amin(grid, dim=(0, 1, 2))
    maxs = torch.amax(grid, dim=(0, 1, 2))
    grid_norm = (grid - mins) / (maxs - mins)
    video_norm = grid_norm.detach().cpu().numpy()

    video = (video_norm * (2**8 - 1)).round().astype(np.uint8)
    # save the Tensor as a numpy file for decoder-side check
    if video.shape[-1] != 3:
        video = video[..., 0]
    if debug:
        np.save(os.path.join(compress_dir, f"{param_name}.npy"), video)

    # save each frame
    # if len(video.shape) == 2:  
    #     imageio.imwrite(os.path.join(compress_dir, f"{param_name}_frame000.png"), video)
    # else:  
    for i in range(n_frames):
        imageio.imwrite(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_frame{i:03d}.png"), video[i])
    
    # run ffmpeg libx265 to compress PNG file
    file_extension = ".265" if debug else ".mp4"
    video_file = os.path.join(compress_dir, f"gop{gop_id}_{param_name}.{file_extension[1:]}")

    print(f"QP value of {param_name} is: {qp}")
    pix_fmt = "-pix_fmt gray" if param_name == "opacities" else ""
    intra_params = ":keyint=1:min-keyint=1:scenecut=0" if use_all_intra else ""
    cmd = f"ffmpeg -i {compress_dir}/gop{gop_id}_{param_name}_frame%03d.png -c:v libx265 {pix_fmt} -x265-params \"qp={qp}{intra_params}\" {video_file}"

    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)

    # remove png files
    png_files = sorted(glob.glob(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_frame*.png")))
    for png_file in png_files:
        os.remove(png_file)
    
    meta = {
        "shape": list(params.shape),
        "dtype": str(params.dtype).split(".")[1],
        "mins": mins.tolist(),
        "maxs": maxs.tolist(),
        "file_extension": file_extension,
    }
    return meta

def _decompress_video_hevc(compress_dir: str, param_name: str, meta: Dict[str, Any], gop_id: int = 0):   
    import imageio.v2 as imageio
    import time
    
    file_extension = meta["file_extension"]
    
    # Record the time when video reading starts
    video_read_start = time.time()
    
    reader = imageio.get_reader(os.path.join(compress_dir, f"{param_name}.{file_extension[1:]}"), format='FFMPEG')

    frames = []
    for i, frame in enumerate(reader):
        frames.append(frame)
    
    video = np.stack(frames, axis=0)
    if param_name == "opacities":
        video = video[..., 0]
    
    # Record the time when video reading ends
    video_read_end = time.time()
    video_read_time = video_read_end - video_read_start
    print(f"Time to get {param_name} video: {video_read_time:.4f} seconds")
    
    # Record the time when video processing starts
    process_start = time.time()
    
    video_norm = video / (2**8 - 1)

    grid_norm = torch.tensor(video_norm)
    mins = torch.tensor(meta["mins"])
    maxs = torch.tensor(meta["maxs"])
    grid = grid_norm * (maxs - mins) + mins

    params = grid.reshape(meta["shape"])
    params = params.to(dtype=getattr(torch, meta["dtype"]))
    
    # Record the time when video processing ends
    process_end = time.time()
    process_time = process_end - process_start
    print(f"Time to convert {param_name} video to params: {process_time:.4f} seconds")
    
    return params

def _compress_video_hevc_16bit(
        compress_dir: str, 
        param_name: str, 
        params: Tensor, 
        n_sidelen: int, 
        qp: int = 10, 
        debug: bool = False,
        use_all_intra: bool = False,
        gop_id: int = 0
) -> Dict[str, Any]:
    import imageio.v2 as imageio
    n_frames = int(params.size(0))

    grid = params.reshape((n_frames, n_sidelen, n_sidelen, -1))
    mins = torch.amin(grid, dim=(0, 1, 2))
    maxs = torch.amax(grid, dim=(0, 1, 2))
    grid_norm = (grid - mins) / (maxs - mins)
    video_norm = grid_norm.detach().cpu().numpy()

    video = (video_norm * (2**16 - 1)).round().astype(np.uint16)
    if debug:
        np.save(os.path.join(compress_dir, f"{param_name}.npy"), video,)

    video_l = video & 0xFF
    video_u = (video >> 8) & 0xFF

    # save each frame
    for i in range(len(video)):
        imageio.imwrite(
            os.path.join(compress_dir, f"gop{gop_id}_{param_name}_l_frame{i:03d}.png"), video_l[i].astype(np.uint8)
        )
        imageio.imwrite(
            os.path.join(compress_dir, f"gop{gop_id}_{param_name}_u_frame{i:03d}.png"), video_u[i].astype(np.uint8)
        )
    
    for byte_select in ['l', 'u']:
        file_extension = ".265" if debug else ".mp4"
        video_file = os.path.join(compress_dir, f"gop{gop_id}_{param_name}_{byte_select}.{file_extension[1:]}")

        # old
        intra_params = ":keyint=1:min-keyint=1:scenecut=0" if use_all_intra else ""
        cmd = f"ffmpeg -i {compress_dir}/gop{gop_id}_{param_name}_{byte_select}_frame%03d.png -c:v libx265 -x265-params \"lossless=1:preset=veryslow{intra_params}\" {video_file}"
        
        # new
        # intra_params = "-intra" if use_all_intra else ""
        # cmd = f"ffmpeg -i {compress_dir}/{param_name}_{byte_select}_frame%03d.png -c:v libx265 {intra_params} -x265-params \"lossless=1:preset=veryslow\" {video_file}"

        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)

    # remove png files
    if not debug:
        png_files = sorted(glob.glob(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_*.png")))
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
        compress_dir: str, param_name: str, meta: Dict[str, Any], gop_id: int = 0
) -> Tensor:
    import imageio.v2 as imageio

    file_extension = meta["file_extension"]
    reader_l = imageio.get_reader(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_l.{file_extension[1:]}"), format='FFMPEG')
    reader_u = imageio.get_reader(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_u.{file_extension[1:]}"), format='FFMPEG')

    frames = []    
    for i, (frame_l, frame_u) in enumerate(zip(reader_l, reader_u)):
        frame_u = frame_u.astype(np.uint16)
        frame = (frame_u << 8) + frame_l
        frames.append(frame)
    
    video = np.stack(frames, axis=0)

    # report the PSNR between reconstructed videos and original videos
    # raw_video = np.load(os.path.join(compress_dir, f"{param_name}.npy"))
    # cal_psnr = lambda x, y: float('inf') if (d := np.mean((x-y)**2)) == 0 else 20*np.log10(65535) - 10*np.log10(d)
    # print(f"PSNR of \"{param_name}\" map after video coding: {cal_psnr(raw_video, video)} dB")
    # os.remove(os.path.join(compress_dir, f"{param_name}.npy"))

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
        use_all_intra: bool = True,
        gop_id: int = 0
) -> Dict[str, Any]:
    import imageio.v2 as imageio
    n_frames = int(params.size(0))

    grid = params.reshape((n_frames, n_sidelen, n_sidelen, -1))
    mins = torch.amin(grid, dim=(0, 1, 2))
    maxs = torch.amax(grid, dim=(0, 1, 2))
    grid_norm = (grid - mins) / (maxs - mins)
    video_norm = grid_norm.detach().cpu().numpy()

    video = (video_norm * (2**8 - 1)).round().astype(np.uint8)
    
    if debug:
        np.save(os.path.join(compress_dir, f"{param_name}.npy"), video,)

    video_w = video[..., 0] # [T, H, W]
    video_xyz = video[..., 1:] # [T, H, W, 3]

    for i in range(len(video)):
        imageio.imwrite(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_w_frame{i:03d}.png"), video_w[i])
        imageio.imwrite(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_xyz_frame{i:03d}.png"), video_xyz[i])

    # run ffmpeg libx265 to compress PNG file
    file_extension = ".265" if debug else ".mp4"
    intra_params = ":keyint=1:min-keyint=1:scenecut=0" if use_all_intra else ""

    video_file = os.path.join(compress_dir, f"gop{gop_id}_{param_name}_w.{file_extension[1:]}")
    print(f"QP value of {param_name}_w is: {qp}")
    cmd = f"ffmpeg -i {compress_dir}/gop{gop_id}_{param_name}_w_frame%03d.png -c:v libx265 -pix_fmt gray -x265-params \"qp={qp}{intra_params}\" {video_file}"
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)

    video_file = os.path.join(compress_dir, f"gop{gop_id}_{param_name}_xyz.{file_extension[1:]}")
    print(f"QP value of {param_name}_xyz is: {qp}")
    cmd = f"ffmpeg -i {compress_dir}/gop{gop_id}_{param_name}_xyz_frame%03d.png -c:v libx265 -x265-params \"qp={qp}{intra_params}\" {video_file}"
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)

    # remove png files
    if not debug:
        png_files = sorted(glob.glob(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_*.png")))
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
        compress_dir: str, param_name: str, meta: Dict[str, Any], gop_id: int = 0
):
    import imageio.v2 as imageio

    file_extension = meta["file_extension"]
    reader_w = imageio.get_reader(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_w.{file_extension[1:]}"), format='FFMPEG')
    reader_xyz = imageio.get_reader(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_xyz.{file_extension[1:]}"), format='FFMPEG')

    frames = []
    for frame_w, frame_xyz in zip(reader_w, reader_xyz):
        frame = np.concatenate([frame_w[..., 0:1], frame_xyz], axis=-1)
        frames.append(frame)

    video = np.stack(frames, axis=0)

    # report the PSNR between reconstructed videos and original videos
    # raw_video = np.load(os.path.join(compress_dir, f"{param_name}.npy"))
    # cal_psnr = lambda x, y: float('inf') if (d := np.mean((x-y)**2)) == 0 else 20*np.log10(255) - 10*np.log10(d)
    # print(f"PSNR of \"{param_name}\" map after video coding: {cal_psnr(raw_video, video)} dB")
    # os.remove(os.path.join(compress_dir, f"{param_name}.npy"))   

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
        use_all_intra: bool = False,
        gop_id: int = 0
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
            imageio.imwrite(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_{shN_name}_frame{f_id:03d}.png"), image)
    
    file_extension = ".265" if debug else ".mp4"
    intra_params = ":keyint=1:min-keyint=1:scenecut=0" if use_all_intra else ""
    for shN_id, shN_name in enumerate(shN_name_list):
        print(f"QP value of {shN_name} is: {qp[shN_name[0:3]]}")
        video_file = os.path.join(compress_dir, f"gop{gop_id}_{param_name}_{shN_name}.{file_extension[1:]}")
        cmd = f"ffmpeg -i {compress_dir}/gop{gop_id}_{param_name}_{shN_name}_frame%03d.png -c:v libx265 -x265-params \"qp={qp[shN_name[0:3]]}{intra_params}\" {video_file}"
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    
    # remove png files
    if not debug:
        png_files = sorted(glob.glob(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_*.png")))
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
        compress_dir: str, param_name: str, meta: Dict[str, Any], gop_id: int = 0
):
    import imageio.v2 as imageio

    shN_name_list = []
    for degree in range(1,4):
        for level in range(-degree, degree+1):
            shN_name_list.append(f"sh{degree}_{level}")

    file_extension = meta["file_extension"]

    shN_reader_list = []
    for shN_name in shN_name_list:
        shN_reader_list.append(imageio.get_reader(os.path.join(compress_dir, f"gop{gop_id}_{param_name}_{shN_name}.{file_extension[1:]}"), format='FFMPEG'))

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
    compress_dir: str, param_name: str, params: Tensor, gop_id: int = 0, **kwargs
) -> Dict[str, Any]:
    """Compress parameters with numpy's NPZ compression."""
    npz_dict = {"arr": params.detach().cpu().numpy()}
    save_fp = os.path.join(compress_dir, f"gop{gop_id}_{param_name}.npz")
    os.makedirs(os.path.dirname(save_fp), exist_ok=True)
    np.savez_compressed(save_fp, **npz_dict)
    meta = {
        "shape": params.shape,
        "dtype": str(params.dtype).split(".")[1],
    }
    return meta


def _decompress_npz(compress_dir: str, param_name: str, meta: Dict[str, Any], gop_id: int = 0) -> Tensor:
    """Decompress parameters with numpy's NPZ compression."""
    arr = np.load(os.path.join(compress_dir, f"gop{gop_id}_{param_name}.npz"))["arr"]
    params = torch.tensor(arr)
    params = params.reshape(meta["shape"])
    params = params.to(dtype=getattr(torch, meta["dtype"]))
    return params

def decode_video_opencv(video_path: str, param_name: str) -> np.ndarray:
    """Decode video using OpenCV and handle color channels based on parameter name."""

    import cv2 # Import OpenCV here to make the dependency optional unless these functions are used
    # Record the time when video reading starts
    video_read_start = time.time()

    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        raise IOError(f"Cannot open video file: {video_path}")

    frames = []
    while True:
        ret, frame = cap.read()
        if not ret:
            break

        # Key: If not a grayscale parameter (opacities), convert BGR to RGB.
        if param_name != "opacities" and frame.shape[-1] == 3:
             frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        elif param_name == "opacities" and frame.shape[-1] == 3: # Grayscale but read as 3 channels.
             frame = frame[..., 0] # Take only one channel.
        # If already single-channel grayscale, no processing needed.

        frames.append(frame)

    cap.release()

    if not frames:
        raise ValueError(f"No frames read from video: {video_path}")

    video = np.stack(frames, axis=0)

    # If opacities is single-channel, remove the last dimension.
    if param_name == "opacities" and video.ndim > 3 and video.shape[-1] == 1:
         video = video.squeeze(-1)

    # Record the time when video reading ends
    video_read_end = time.time()
    video_read_time = video_read_end - video_read_start
    print(f"Time to get {param_name} video (OpenCV, RGB corrected): {video_read_time:.4f} seconds")

    return video

def _decompress_video_hevc_opencv(compress_dir: str, param_name: str, meta: Dict[str, Any]):
    """Decompress HEVC video using OpenCV (alternative to imageio version).

    Note: Requires opencv-python (pip install opencv-python).
    """

    file_extension = meta["file_extension"]
    video_path = os.path.join(compress_dir, f"{param_name}.{file_extension[1:]}")

    # Decode video using OpenCV (includes RGB correction)
    video = decode_video_opencv(video_path, param_name)

    # Record the time when video processing starts
    process_start = time.time()

    video_norm = video / (2**8 - 1)

    grid_norm = torch.tensor(video_norm, dtype=torch.float32) # Ensure correct dtype.
    mins = torch.tensor(meta["mins"], dtype=torch.float32)
    maxs = torch.tensor(meta["maxs"], dtype=torch.float32)

    # Handle potential dimension mismatch for mins and maxs (e.g., grayscale).
    if grid_norm.dim() == 3 and mins.dim() == 1 and grid_norm.shape[-1] != mins.shape[0]:
        # Assuming grayscale case, mins/maxs have only one value.
        mins = mins.view(1)
        maxs = maxs.view(1)
    elif grid_norm.dim() == 4 and mins.dim() == 1 and grid_norm.shape[-1] == mins.shape[0]:
         # Normal multi-channel case, adjust mins/maxs shape for broadcasting.
        mins = mins.view(1, 1, 1, -1)
        maxs = maxs.view(1, 1, 1, -1)
    elif grid_norm.dim() == 3 and mins.dim() == 0: # Grayscale and mins/maxs are scalars.
        pass # No need to adjust shape.
    elif grid_norm.dim() == 4 and mins.dim() == 0: # Multi-channel and mins/maxs are scalars? (Unlikely, but handle it).
         pass # No need to adjust shape.

    grid = grid_norm * (maxs - mins) + mins

    # Ensure the shape after reshape matches meta['shape'].
    target_shape = meta["shape"]
    try:
        params = grid.reshape(target_shape)
    except RuntimeError as e:
         print(f"Warning: Shape mismatch during reshape. Grid shape: {grid.shape}, Target shape: {target_shape}. Error: {e}")
         # Try a more compatible reshape (T, H, W, C) -> (T, N, C) or (T, H, W) -> (T, N).
         if len(target_shape) == 3 and grid.dim() == 4: # T, N, C vs T, H, W, C
             params = grid.reshape(target_shape[0], -1, target_shape[2])
         elif len(target_shape) == 2 and grid.dim() == 3: # T, N vs T, H, W
             params = grid.reshape(target_shape[0], -1)
         else: # Cannot match, keep grid shape or raise error.
             print("Error: Cannot automatically reconcile shapes.")
             raise e # Or return grid.

    params = params.to(dtype=getattr(torch, meta["dtype"]))

    # Record the time when video processing ends
    process_end = time.time()
    process_time = process_end - process_start
    print(f"Time to convert {param_name} video to params (OpenCV): {process_time:.4f} seconds")

    return params

def _decompress_video_hevc_16bit_opencv(
        compress_dir: str, param_name: str, meta: Dict[str, Any]
) -> Tensor:
    """Decompress 16-bit HEVC video using OpenCV by combining two 8-bit streams.

    Note: Requires opencv-python. Reads '_l' and '_u' streams.
    """
    file_extension = meta["file_extension"]
    video_path_l = os.path.join(compress_dir, f"{param_name}_l.{file_extension[1:]}")
    video_path_u = os.path.join(compress_dir, f"{param_name}_u.{file_extension[1:]}")

    # Decode LSB and MSB videos using OpenCV helper
    video_l = decode_video_opencv(video_path_l, f"{param_name}_l") # Pass param_name hint
    video_u = decode_video_opencv(video_path_u, f"{param_name}_u") # Pass param_name hint

    # Ensure videos have the same length
    if len(video_l) != len(video_u):
        raise ValueError(f"Length mismatch between {param_name}_l ({len(video_l)}) and {param_name}_u ({len(video_u)}) videos.")

    # Reconstruct 16-bit video
    video_u_16 = video_u.astype(np.uint16)
    video = (video_u_16 << 8) | video_l.astype(np.uint16) # Combine LSB and MSB

    # --- Rest of the processing is similar to the imageio version ---
    process_start = time.time()

    video_norm = video / (2**16 - 1)

    grid_norm = torch.tensor(video_norm, dtype=torch.float32)
    mins = torch.tensor(meta["mins"], dtype=torch.float32)
    maxs = torch.tensor(meta["maxs"], dtype=torch.float32)

    # Adjust mins/maxs shape for broadcasting
    if grid_norm.dim() == 4 and mins.dim() == 1:
        mins = mins.view(1, 1, 1, -1)
        maxs = maxs.view(1, 1, 1, -1)
    elif grid_norm.dim() == 3 and mins.dim() == 0: # scalar mins/maxs
         pass

    grid = grid_norm * (maxs - mins) + mins

    target_shape = meta["shape"]
    try:
        params = grid.reshape(target_shape)
    except RuntimeError as e:
        print(f"Warning: Shape mismatch during reshape. Grid shape: {grid.shape}, Target shape: {target_shape}. Error: {e}")
        # Fallback reshape logic (same as before)
        if len(target_shape) == 3 and grid.dim() == 4:
            params = grid.reshape(target_shape[0], -1, target_shape[2])
        elif len(target_shape) == 2 and grid.dim() == 3:
            params = grid.reshape(target_shape[0], -1)
        else:
            print("Error: Cannot automatically reconcile shapes.")
            raise e

    params = params.to(dtype=getattr(torch, meta["dtype"]))

    process_end = time.time()
    process_time = process_end - process_start
    print(f"Time to convert {param_name} 16bit video to params (OpenCV): {process_time:.4f} seconds")

    return params

def _decompress_quats_video_hevc_opencv(
        compress_dir: str, param_name: str, meta: Dict[str, Any]
) -> Tensor:
    """Decompress quaternion HEVC video using OpenCV.

    Note: Requires opencv-python. Reads '_w' (grayscale) and '_xyz' (color) streams.
    """
    file_extension = meta["file_extension"]
    video_path_w = os.path.join(compress_dir, f"{param_name}_w.{file_extension[1:]}")
    video_path_xyz = os.path.join(compress_dir, f"{param_name}_xyz.{file_extension[1:]}")

    # Decode videos using OpenCV helper
    video_w = decode_video_opencv(video_path_w, "opacities") # Hint as grayscale
    video_xyz = decode_video_opencv(video_path_xyz, f"{param_name}_xyz") # Hint as color

    # Ensure videos have the same length
    if len(video_w) != len(video_xyz):
        raise ValueError(f"Length mismatch between {param_name}_w ({len(video_w)}) and {param_name}_xyz ({len(video_xyz)}) videos.")

    # Combine w and xyz components
    if video_w.ndim == 3: # T, H, W
        video_w = video_w[..., np.newaxis] # Add channel dim

    video = np.concatenate([video_w, video_xyz], axis=-1)

    # --- Rest of the processing is similar to the imageio version ---
    process_start = time.time()

    video_norm = video / (2**8 - 1)

    grid_norm = torch.tensor(video_norm, dtype=torch.float32)
    mins = torch.tensor(meta["mins"], dtype=torch.float32)
    maxs = torch.tensor(meta["maxs"], dtype=torch.float32)

    # Adjust mins/maxs shape for broadcasting
    if grid_norm.dim() == 4 and mins.dim() == 1:
         mins = mins.view(1, 1, 1, -1)
         maxs = maxs.view(1, 1, 1, -1)
    elif grid_norm.dim() == 4 and mins.dim() == 0: # scalar mins/maxs
        pass

    grid = grid_norm * (maxs - mins) + mins

    target_shape = meta["shape"]
    try:
        params = grid.reshape(target_shape)
    except RuntimeError as e:
        print(f"Warning: Shape mismatch during reshape. Grid shape: {grid.shape}, Target shape: {target_shape}. Error: {e}")
        # Fallback reshape logic
        if len(target_shape) == 3 and grid.dim() == 4:
            params = grid.reshape(target_shape[0], -1, target_shape[2])
        elif len(target_shape) == 2 and grid.dim() == 3:
             params = grid.reshape(target_shape[0], -1)
        else:
            print("Error: Cannot automatically reconcile shapes.")
            raise e

    params = params.to(dtype=getattr(torch, meta["dtype"]))

    process_end = time.time()
    process_time = process_end - process_start
    print(f"Time to convert {param_name} video to params (OpenCV): {process_time:.4f} seconds")

    return params

def _decompress_shN_video_hevc_opencv(
        compress_dir: str, param_name: str, meta: Dict[str, Any]
) -> Tensor:
    """Decompress SH coefficient HEVC videos using OpenCV.

    Note: Requires opencv-python. Reads multiple streams named like 'shN_sh1_-1'.
    """
    shN_name_list = []
    for degree in range(1, 4):
        for level in range(-degree, degree + 1):
            shN_name_list.append(f"sh{degree}_{level}")

    file_extension = meta["file_extension"]

    shN_video_list = []
    process_start_all_decodes = time.time()
    for shN_name in shN_name_list:
        video_path = os.path.join(compress_dir, f"{param_name}_{shN_name}.{file_extension[1:]}")
        # Decode each SH component video using OpenCV helper
        shN_video = decode_video_opencv(video_path, f"{param_name}_{shN_name}")
        shN_video_list.append(shN_video)
    process_end_all_decodes = time.time()
    print(f"Time to decode all {len(shN_name_list)} SH videos (OpenCV): {process_end_all_decodes - process_start_all_decodes:.4f} seconds")


    # Check consistency and Stack videos along the SH dimension (axis=3)
    if not shN_video_list:
        raise ValueError("No SH videos decoded.")
    first_video_shape = shN_video_list[0].shape
    for i, vid in enumerate(shN_video_list[1:], 1):
        if vid.shape[0] != first_video_shape[0]: # Check frame count
             raise ValueError(f"Frame count mismatch in SH videos: {shN_name_list[0]} ({first_video_shape[0]}) vs {shN_name_list[i]} ({vid.shape[0]}) ")

    shN_videos = np.stack(shN_video_list, axis=3) # [T, H, W, N_sh, C]

    # --- Rest of the processing is similar to the imageio version ---
    process_start_conversion = time.time()

    shN_norm = shN_videos / (2**8 - 1)

    grid_norm = torch.tensor(shN_norm, dtype=torch.float32)
    mins = torch.tensor(meta["mins"], dtype=torch.float32) # Shape [N_sh, C]
    maxs = torch.tensor(meta["maxs"], dtype=torch.float32) # Shape [N_sh, C]

    # Adjust mins/maxs shape for broadcasting: [N_sh, C] -> [1, 1, 1, N_sh, C]
    if grid_norm.dim() == 5 and mins.dim() == 2:
         mins = mins.view(1, 1, 1, mins.shape[0], mins.shape[1])
         maxs = maxs.view(1, 1, 1, maxs.shape[0], maxs.shape[1])
    elif grid_norm.dim()==5 and mins.dim() == 0: # scalar mins/maxs
        pass

    grid = grid_norm * (maxs - mins) + mins # [T, H, W, N_sh, C]

    target_shape = meta["shape"] # Expected: [T, N, N_sh, C] where N = H*W
    try:
        # Reshape [T, H, W, N_sh, C] -> [T, H*W, N_sh, C]
        params = grid.reshape(target_shape[0], -1, target_shape[2], target_shape[3])
        # Verify shape matches exactly
        if list(params.shape) != target_shape:
             print(f"Warning: Reshaped shape {list(params.shape)} doesn't exactly match target {target_shape}. Final reshape might be needed.")
             params = params.reshape(target_shape) # Force final reshape

    except RuntimeError as e:
        print(f"Error: Shape mismatch during reshape. Grid shape: {grid.shape}, Target shape: {target_shape}. Error: {e}")
        raise e

    params = params.to(dtype=getattr(torch, meta["dtype"]))

    process_end_conversion = time.time()
    process_time = process_end_conversion - process_start_conversion
    print(f"Time to convert {param_name} video to params (OpenCV): {process_time:.4f} seconds")

    return params
