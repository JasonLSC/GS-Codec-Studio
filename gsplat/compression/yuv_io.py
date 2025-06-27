# yuv_io.py

import json
import logging
from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Literal, Tuple
from pathlib import Path

import torch
from torch import Tensor


from gsplat.compression.yuv_utils import _save_tensor_to_yuv, _load_yuv_to_tensor, chroma_downsampling

@dataclass
class YUVDataHandler:
    """
    Handles saving tensor data to YUV files with metadata, and loading it back.

    This class encapsulates the logic for different attribute types, such as standard
    quantization, 16-bit splitting, or multi-component handling.
    """
    yuv_dir: Path
    gop_id: int
    n_sidelen: int
    # This data is provided during compression. For decompression, it can be an empty dict.
    splats_video_data: Dict[str, Tensor] = field(default_factory=dict)

    def __post_init__(self):
        """Create the directory after initialization and set up method maps."""
        # Ensure the YUV directory exists
        if not self.yuv_dir.exists():
            logging.info(f"Error: YUV directory does not exist: {self.yuv_dir}.")
            raise FileNotFoundError(f"YUV directory does not exist: {self.yuv_dir}.")
        
        # Maps dispatch to the correct handler based on the attribute name.
        self._save_fn_map: Dict[str, Callable] = {
            "means": self._save_yuv_bit_split,
            "quats": self._save_yuv_4_components,
            "opacities": self._save_yuv_1_component,
            "shN": self._save_yuv_shN,
            "default": self._save_yuv_default, # Fallback for attributes like 'scales', 'sh0'
        }
        self._load_fn_map: Dict[str, Callable] = {
            "means": self._load_yuv_bit_split,
            "quats": self._load_yuv_4_components,
            "opacities": self._load_yuv_1_component,
            "shN": self._load_yuv_shN,
            "default": self._load_yuv_default,
        }

    #region Static Helper Methods for Quantization, etc.

    @staticmethod
    def _quantize_tensor(
        params: Tensor, n_sidelen: int, bit_depth: int = 8
    ) -> Tuple[Tensor, Tensor, Tensor, List[int]]:
        """Quantizes a tensor to a specified bit depth after reshaping and normalization."""
        n_frames = params.size(0)
        grid = params.reshape((n_frames, n_sidelen, n_sidelen, -1))
        
        mins = torch.amin(grid, dim=(0, 1, 2), keepdim=True)
        maxs = torch.amax(grid, dim=(0, 1, 2), keepdim=True)
        
        scale = maxs - mins
        scale[scale == 0] = 1.0  # Avoid division by zero

        grid_norm = (grid - mins) / scale
        
        max_val = (2**bit_depth) - 1
        uint_type = f"uint{bit_depth}" if bit_depth == 8 else "int32" # PyTorch lacks uint16
        video = (grid_norm * max_val).round().to(getattr(torch, uint_type))
        
        original_shape = list(params.shape)
        # Squeeze helper tensors for clean JSON serialization
        return video, mins.squeeze(), maxs.squeeze(), original_shape

    @staticmethod
    def _dequantize_tensor(video: Tensor, meta: Dict[str, Any]) -> Tensor:
        """Dequantizes a video tensor using metadata."""
        # Add a singleton dimension if needed for broadcasting
        mins = torch.tensor(meta["mins"], device=video.device).view(1, 1, 1, -1)
        maxs = torch.tensor(meta["maxs"], device=video.device).view(1, 1, 1, -1)
        
        bit_depth = meta.get("bit_depth", 8)
        max_val = (2**bit_depth) - 1

        grid_norm = video.to(torch.float32) / max_val
        grid = grid_norm * (maxs - mins) + mins
        
        params = grid.reshape(meta["shape"])
        dtype_str = str(meta["dtype"])
        if 'torch.' not in dtype_str:
            dtype_str = f'torch.{dtype_str}'
        
        params = params.to(dtype=eval(dtype_str))
        return params
    #endregion

    #region Main Public Methods
    def _get_paths(self, param_name: str, suffix: str = "") -> Tuple[Path, Path]:
        """Generates standard file paths for an attribute component."""
        sanitized_suffix = f"_{suffix}" if suffix else ""
        base_file_name = f"gop{self.gop_id}_{param_name}{sanitized_suffix}"
        yuv_path = self.yuv_dir / f"{base_file_name}.yuv"
        decoded_yuv_path = self.yuv_dir / f"{base_file_name}_decoded.yuv" 
        return yuv_path, decoded_yuv_path
    
    def save_all_to_yuv(self) -> Dict[str, Any]:
        """Saves all attributes in `splats_video_data` to their respective YUV files."""
        meta = {}
        for param_name, params in self.splats_video_data.items():
            logging.info(f"Saving '{param_name}' to YUV format...")
            
            save_fn = self._save_fn_map.get(param_name, self._save_fn_map["default"])
            param_meta = save_fn(params, param_name) # Pass `self` implicitly to the instance method
            meta[param_name] = param_meta
            
        return meta
            
    def load_all_from_yuv(self, meta: Dict[str, Any]) -> Dict[str, Tensor]:
        """Loads attributes specified by `param_names` from decoded YUV files."""
        loaded_data = {}
        for param_name, param_meta in meta.items():
            logging.info(f"Loading '{param_name}' from YUV format...")
            load_fn = self._load_fn_map.get(param_name, self._load_fn_map["default"])
            loaded_data[param_name] = load_fn(param_name, param_meta) # Pass `self` implicitly
    
        return loaded_data
    #endregion

    #region Specialized Handlers
    def _save_yuv_default(self, params: Tensor, param_name: str) -> Dict[str, Any]:
        """Default YUV saving with 8-bit quantization and 4:4:4 subsampling."""
        video, mins, maxs, shape = self._quantize_tensor(params, self.n_sidelen, bit_depth=8)
        yuv_path, _ = self._get_paths(param_name)
        # Default assumes 3 channels for color, adapt if needed
        pix_fmt = 'yuv444p'
        _save_tensor_to_yuv(video, str(yuv_path), '444')
        
        return {
            "shape": shape, "dtype": str(params.dtype),
            "mins": mins.tolist(), "maxs": maxs.tolist(), "pix_fmt": pix_fmt
        }

    def _load_yuv_default(self, param_name: str, meta: Dict[str, Any]) -> Tensor:
        """Default YUV loading."""
        _, decoded_yuv_path = self._get_paths(param_name)
        h, w = meta["shape"][1:3]
        pix_fmt_str = meta.get('pix_fmt', 'yuv444p') # Fallback to 444
        video = _load_yuv_to_tensor(str(decoded_yuv_path), h, w, pix_fmt_str)
        return self._dequantize_tensor(video, meta)

    def _save_yuv_1_component(self, params: Tensor, param_name: str) -> Dict[str, Any]:
        """Saves a single-component attribute (like opacity) as a 4:0:0 YUV."""
        video, mins, maxs, shape = self._quantize_tensor(params, self.n_sidelen)
        yuv_path, _ = self._get_paths(param_name)
        _save_tensor_to_yuv(video, str(yuv_path), '400')
        return {
            "shape": shape, "dtype": str(params.dtype),
            "mins": mins.tolist(), "maxs": maxs.tolist(), "pix_fmt": "yuv400p"
        }

    def _load_yuv_1_component(self, param_name: str, meta: Dict[str, Any]) -> Tensor:
        """Loads a single-component attribute."""
        # Reuses the default loader, as it's general enough
        return self._load_yuv_default(param_name, meta)

    def _save_yuv_4_components(self, params: Tensor, param_name: str) -> Dict[str, Any]:
        """Saves a 4-component attribute (like quaternions) into a 1-ch and a 3-ch YUV."""
        video, mins, maxs, shape = self._quantize_tensor(params, self.n_sidelen)
        
        video_w = video[..., 0:1] # Keep dimension for _save_tensor_to_yuv
        video_xyz = video[..., 1:4]

        yuv_w_path, _ = self._get_paths(param_name, "w")
        yuv_xyz_path, _ = self._get_paths(param_name, "xyz")

        _save_tensor_to_yuv(video_w, str(yuv_w_path), '400')
        _save_tensor_to_yuv(video_xyz, str(yuv_xyz_path), '444')
        
        return {
            "shape": shape, "dtype": str(params.dtype),
            "mins": mins.tolist(), "maxs": maxs.tolist(),
            "pix_fmt_w": "yuv400p", "pix_fmt_xyz": "yuv444p"
        }

    def _load_yuv_4_components(self, param_name: str, meta: Dict[str, Any]) -> Tensor:
        """Loads a 4-component attribute from two separate YUVs."""
        h, w = meta["shape"][1:3]
        _, decoded_w_path = self._get_paths(param_name, "w")
        _, decoded_xyz_path = self._get_paths(param_name, "xyz")
        
        video_w = _load_yuv_to_tensor(str(decoded_w_path), h, w, meta["pix_fmt_w"])
        video_xyz = _load_yuv_to_tensor(str(decoded_xyz_path), h, w, meta["pix_fmt_xyz"])

        video = torch.cat([video_w, video_xyz], dim=-1)
        return self._dequantize_tensor(video, meta)

    def _save_yuv_bit_split(self, params: Tensor, param_name: str) -> Dict[str, Any]:
        """Saves a tensor using 16-bit quantization, split into two 8-bit YUVs."""
        video, mins, maxs, shape = self._quantize_tensor(params, self.n_sidelen, bit_depth=16)
        video_uint16 = video.to(torch.int32) # Pytorch lacks uint16, use int32
        
        video_l = (video_uint16 & 0xFF).to(torch.uint8)
        video_u = ((video_uint16 >> 8) & 0xFF).to(torch.uint8)
        
        yuv_l_path, _ = self._get_paths(param_name, "l")
        yuv_u_path, _ = self._get_paths(param_name, "u")
        
        # Save both as 4:4:4 as they carry raw bit data, not color
        _save_tensor_to_yuv(video_l, str(yuv_l_path), '444')
        _save_tensor_to_yuv(video_u, str(yuv_u_path), '444')

        return {
            "shape": shape, "dtype": str(params.dtype),
            "mins": mins.tolist(), "maxs": maxs.tolist(),
            "bit_depth": 16, "pix_fmt": "yuv444p"
        }

    def _load_yuv_bit_split(self, param_name: str, meta: Dict[str, Any]) -> Tensor:
        """Loads a 16-bit tensor by combining two 8-bit YUVs."""
        h, w = meta["shape"][1:3]
        _, decoded_l_path = self._get_paths(param_name, "l")
        _, decoded_u_path = self._get_paths(param_name, "u")
        
        video_l = _load_yuv_to_tensor(str(decoded_l_path), h, w, meta["pix_fmt"])
        video_u = _load_yuv_to_tensor(str(decoded_u_path), h, w, meta["pix_fmt"])

        video_16bit = (video_u.to(torch.int32) << 8) | video_l.to(torch.int32)
        
        return self._dequantize_tensor(video_16bit, meta)
    
    def _get_sh_sub_names(self) -> List[str]:
        """
        Returns the standard names for spherical harmonic coefficients.

        This method generates a list of names for the 15 SH coefficients used in
        spherical harmonics, which are typically indexed from 0 to 14.
        """
        shN_name_list = []
        for degree in range(1,4):
            for level in range(-degree, degree+1):
                shN_name_list.append(f"sh{degree}_{level}")

        return shN_name_list
    
    def _save_yuv_shN(self, params: Tensor, param_name: str) -> Dict[str, Any]:
        """
        Saves spherical harmonic coefficients (degree > 0).

        This method quantizes the entire SH tensor at once to maintain relative
        magnitudes, then splits it into 15 individual 3-channel (4:4:4) YUV files,
        one for each SH coefficient. The metadata stores the shared quantization
        details and the list of sub-file names.
        """
        logging.info(f"Quantizing '{param_name}' tensor...")
        # Quantize the entire tensor at once to a shared range
        video, mins, maxs, shape = self._quantize_tensor(params, self.n_sidelen)
        video = video.reshape((video.shape[0], video.shape[1], video.shape[2], -1, 3))  # [T, H, W, C_idx, Channels]    
        sh_names = self._get_sh_sub_names()
        if video.shape[3] != len(sh_names):
            raise ValueError(
                f"Tensor dimension for '{param_name}' ({video.shape[3]}) does not match "
                f"the expected number of SH coefficients ({len(sh_names)})."
            )

        # Chroma subsampling format for this data type. 444 is best for non-visual data.
        pix_fmt = "yuv444p"
        chroma_format_str = "444"

        for i, sh_name in enumerate(sh_names):
            logging.debug(f"  -> Saving slice {i}: {sh_name}")
            sh_slice_video = video[..., i, :]

            # Get a unique path for this slice using a suffix
            yuv_path, _ = self._get_paths(param_name, suffix=sh_name)
            
            _save_tensor_to_yuv(sh_slice_video, str(yuv_path), chroma_format_str)

        logging.info(f"Finished saving all {len(sh_names)} slices for '{param_name}'.")

        # Metadata must include the list of names for reconstruction
        return {
            "shape": shape,
            "dtype": str(params.dtype),
            "mins": mins.tolist(),
            "maxs": maxs.tolist(),
            "pix_fmt": pix_fmt,
            "sh_names": sh_names, # Crucial for loading
        }

    def _load_yuv_shN(self, param_name: str, meta: Dict[str, Any]) -> Tensor:
        """
        Loads spherical harmonic coefficients from multiple YUV files.

        This method reads the list of SH sub-file names from the metadata, loads
        each corresponding decoded YUV file, stacks them back into a single tensor,
        and finally dequantizes the result using the shared quantization parameters.
        """
        sh_names = meta.get("sh_names")
        if not sh_names:
            raise KeyError(f"Metadata for '{param_name}' is missing the required 'sh_names' key.")

        h, w = meta["shape"][1:3]
        pix_fmt = meta.get("pix_fmt", "yuv444p")
        
        loaded_slices = []
        logging.info(f"Loading {len(sh_names)} slices for '{param_name}'...")

        for sh_name in sh_names:
            logging.debug(f"  -> Loading slice: {sh_name}")
            # Get the path to the DECODED YUV for this slice
            _, decoded_yuv_path = self._get_paths(param_name, suffix=sh_name)

            if not decoded_yuv_path.exists():
                raise FileNotFoundError(
                    f"Decoded YUV file for SH slice '{sh_name}' not found at: {decoded_yuv_path}"
                )

            slice_video = _load_yuv_to_tensor(str(decoded_yuv_path), h, w, pix_fmt)
            loaded_slices.append(slice_video)

        # Stack the slices along the dimension they were split from (dim=3: [T,H,W,C_idx,Channels])
        stacked_video = torch.stack(loaded_slices, dim=3)
        # view as [T, H, W, C_idx * Channels]
        stacked_video = stacked_video.reshape((stacked_video.shape[0], stacked_video.shape[1], stacked_video.shape[2], -1))
        logging.info("All slices loaded and stacked. Dequantizing...")

        # Use the master dequantization helper on the fully reconstructed tensor
        return self._dequantize_tensor(stacked_video, meta)