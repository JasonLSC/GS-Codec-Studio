# yuv_codec.py

import logging
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Any, Union, Optional, Literal

import numpy as np
import torch
import torch.nn.functional as F
from torch import Tensor

# ### <<< REFACTOR: Import the refactored classes ###
from .yuv_io import YUVDataHandler
from .video_codec import VideoCodec
from gsplat.compression.sort import sort_splats, sort_splats_morton
import json

# ### <<< REFACTOR: Define default configurations cleanly ###
# These can be customized and passed to SeqYUVCodec


DEFAULT_ATTRIBUTE_CONFIGS = {
    "means": {"qp": 0, "pix_fmt": "yuv444p"},
    "opacities": {"qp": 4, "pix_fmt": "yuv400p"},
    "quats": {
        "w": {"qp": 4, "pix_fmt": "yuv400p"},
        "xyz": {"qp": 4, "pix_fmt": "yuv444p"},
    },
    "scales": {"qp": 4, "pix_fmt": "yuv444p"},
    "sh0": {"qp": 16, "pix_fmt": "yuv444p"},
    "shN": {
        "sh1": {"qp": 20, "pix_fmt": "yuv444p"},
        "sh2": {"qp": 24, "pix_fmt": "yuv444p"},
        "sh3": {"qp": 28, "pix_fmt": "yuv444p"},
    },
    "default": {"qp": 22, "pix_fmt": "yuv444p"}, 
}
@dataclass
class SeqYUVCodec:
    """
    Orchestrates the compression and decompression of Gaussian Splatting sequences.

    This class takes a list of splat frames, reorganizes them into attribute videos,
    and then uses a YUVDataHandler to save them as raw YUV and a VideoCodec to 
    compress them into bitstreams.
    """
    
    # ### <<< REFACTOR: Use Dependency Injection for codec and data handler ###
    # Configuration for the process
    video_codec_type: str = "ffmpeg"  # or "hevc"
    
    verbose: bool = True
    debug: bool = False
    color_standard: str = "BT709"
    use_sort: bool = True
    sort_type: Literal["plas", "morton"] = "morton"
    use_all_intra: bool = False  # If True, all frames are intra-coded
    # ### <<< REFACTOR: A single, unified config dictionary is much cleaner ###
    attribute_configs: Optional[Dict[str, Dict[str, Any]]] = None
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
    transform_attributes: Dict[str, bool] = field(default_factory=lambda: {
        "means": False,
        "opacities": False,
        "scales": False,
        "quats": True,
        "sh0": True,
        "shN": True
    })

    chroma_subsampling: Dict[str, str] = field(default_factory=lambda: {
        "sh0": "444",
        "shN": "444"
    })
    use_chroma_qp_offset: Dict[str, bool]= field(default_factory=lambda: {
        "sh0": False,
        "shN": False
    })
    # Internal state
    # splats_videos: Dict[str, Tensor] = field(default_factory=dict, init=False)
    
    def __post_init__(self):
        """Post-initialization to set up default configurations."""
        if not self.attribute_configs:
            # Use default configurations if none provided
            self.attribute_configs = DEFAULT_ATTRIBUTE_CONFIGS
      
        # Ensure compress_dir exists
        #self.compress_dir.mkdir(parents=True, exist_ok=True)
    
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
        
        # Define the RGB to YCbCr transformation matrices for different standards
        
        # BT.709 (Rec. 709) standard, which is the most widely used for HD content
        # Reference: ITU-R BT.709-6 (06/2015)
        # 
        # Key characteristics:
        # - Designed for HD and Full HD content (1080p)
        # - Uses D65 white point
        # - Assumes RGB values are in range [0,1]
        # - Output Y in range [0,1], Cb/Cr in range [-0.5,0.5]
        bt709_rgb_to_ycbcr = torch.tensor([
            [ 0.299,    0.587,    0.114  ],  # Y  = 0.299R  + 0.587G  + 0.114B
            [-0.169,   -0.331,    0.5    ],  # Cb = -0.169R - 0.331G  + 0.5B
            [ 0.5,     -0.419,   -0.081  ]   # Cr = 0.5R    - 0.419G  - 0.081B
        ], dtype=torch.float32)
        
        # BT.470 (older standard for SD/analog TV content)
        # Reference: ITU-R BT.470
        # 
        # Key characteristics:
        # - Designed for SD content and analog TV broadcasts
        # - Uses different chroma scaling compared to BT.709
        # - Assumes RGB values are in range [0,1]
        # - Output Y in range [0,1], Cb/Cr in range [-0.5,0.5]
        bt470_rgb_to_ycbcr = torch.tensor([
            [ 0.299,    0.587,    0.114  ],  # Y  = 0.299R  + 0.587G  + 0.114B
            [-0.147,   -0.289,    0.436  ],  # Cb = -0.147R - 0.289G + 0.436B
            [ 0.615,   -0.515,   -0.100  ]   # Cr = 0.615R - 0.515G - 0.100B
        ], dtype=torch.float32)
        
        # Select the appropriate transformation matrix based on color standard
        if self.color_standard == "BT470":
            rgb_to_ycbcr = bt470_rgb_to_ycbcr
            if self.verbose:
                print("Using BT470 RGB to YCbCr transformation matrix")
        else:  # Default to BT709
            rgb_to_ycbcr = bt709_rgb_to_ycbcr
            if self.verbose:
                print("Using BT709 RGB to YCbCr transformation matrix")
        
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
        
        # Define the YCbCr to RGB transformation matrices for different standards
        
        # BT.709 inverse transformation matrix
        # This matrix is the inverse of the RGB to YCbCr matrix for BT.709
        bt709_ycbcr_to_rgb = torch.tensor([
            [1.0,     0.0,      1.402   ],  # R = Y + 0.0Cb + 1.402Cr
            [1.0,    -0.344136, -0.714136],  # G = Y - 0.344136Cb - 0.714136Cr
            [1.0,     1.772,    0.0     ]   # B = Y + 1.772Cb + 0.0Cr
        ], dtype=torch.float32)
        
        # BT.470 inverse transformation matrix
        # This matrix is the inverse of the RGB to YCbCr matrix for BT.470
        bt470_ycbcr_to_rgb = torch.tensor([
            [1.0,     0.0,      1.140  ],  # R = Y + 0.0Cb + 1.140Cr
            [1.0,    -0.394,   -0.581  ],  # G = Y - 0.394Cb - 0.581Cr
            [1.0,     2.032,    0.0    ]   # B = Y + 2.032Cb + 0.0Cr
        ], dtype=torch.float32)
        
        # Select the appropriate inverse transformation matrix based on color standard
        if self.color_standard == "BT470":
            ycbcr_to_rgb = bt470_ycbcr_to_rgb
            if self.verbose:
                print("Using BT470 YCbCr to RGB inverse transformation matrix")
        else:  # Default to BT709
            ycbcr_to_rgb = bt709_ycbcr_to_rgb
            if self.verbose:
                print("Using BT709 YCbCr to RGB inverse transformation matrix")
        
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
    
    def _pad_n_splats(self, splats: Dict[str, Tensor], n_pad: int) -> Dict[str, Tensor]:
        """Pad the splats to make them square
        Args:
            splats (Dict[str, Tensor]): Dictionary of splats with keys as attributes and values as tensors.
            n_pad (int): Number of splats to pad to make the total count square.
        Returns:
            Dict[str, Tensor]: Padded splats dictionary.
        """
        assert n_pad >= 0, "n_pad must be non-negative."
        if n_pad == 0:
            return splats
        for k, v in splats.items():
            pad_shape = list(v.shape)
            pad_shape[0] += n_pad
            padded_v = torch.zeros(pad_shape, dtype=v.dtype, device=v.device)
            padded_v[:v.shape[0]] = v
            splats[k] = padded_v
        return splats
    
    def _sort_with_frame_index(self, splats_list: List[Dict], n_pad: int, frame_id: int = 0) -> Tensor:
        """Organize the list of splats into several sequences of attributs

        Args:
            splats_list (List[Dict]): List of splat dictionaries, each containing attributes like means, quats, etc.
            frame_id (int): The index of the frame to sort. Defaults to 0.
        Returns:
            Tensor: Sorted indices of the splats for the specified frame.

        """
        splats_to_be_sorted = splats_list[frame_id]

        if n_pad != 0:
            # splats = _crop_n_splats(splats, n_crop)
            splats_to_be_sorted = self._pad_n_splats(splats_to_be_sorted, n_pad)
            print(
                f"Warning: Number of Gaussians was not square. Padded {n_pad} Gaussians."
            )
        if self.sort_type == "plas":
             _, sorted_indices = sort_splats(splats_to_be_sorted, return_indices=True, sort_with_shN=False)
        elif self.sort_type == "morton":
            _, sorted_indices = sort_splats_morton(splats_to_be_sorted, return_indices=True)
        else:
            raise ValueError(f"Unknown sort type: {self.sort_type}. Use 'plas' or 'morton'.")
        print(f"Finsh the sorting with frame {frame_id}.")

        return sorted_indices
    
    def _splats_list_to_attribute_seq(self, splats_list: List[Dict]) -> Dict[str, Tensor]:
        """Convert a list of splat dictionaries into a sequence of attributes.
        Args:
            splats_list (List[Dict]): List of splat dictionaries, each containing attributes like means, quats, etc.
        Returns:
            Dict[str, Tensor]: A dictionary where keys are attribute names and values are tensors of attributes
            stacked across the frames.
        """
        if not splats_list:
            logging.error("Input splats_list is empty. Cannot convert to attribute sequence.")
            return {}
        sample_splat = splats_list[0]
        attribute_names = list(sample_splat.keys())

        splats_sequences = {}
        for attr_name in attribute_names:
            attr_seq = [splat[attr_name] for splat in splats_list if attr_name in splat]
            attr_seq = torch.stack(attr_seq, dim=0)

            splats_sequences[attr_name] = attr_seq
        
        return splats_sequences

    def _pad_attr_seq(self, splats_videos: Dict[str, Tensor], n_pad : int) -> Dict[str, Tensor]:
        """Pad the attribute sequences to make them square.
        Args:
            splats_videos (Dict[str, Tensor]): Dictionary of attribute sequences, where keys are attribute names
            and values are tensors of attributes stacked across frames.
        Returns:
            Dict[str, Tensor]: Padded attribute sequences dictionary.
        """
        if not splats_videos:
            logging.error("Input splats_videos is empty. Cannot pad attribute sequences.")
            return {}
        if n_pad != 0:
            logging.warning(f"Padding {n_pad} Gaussians to make the number of Gaussians square.")
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
    
    def _reorganize(self, splats_list: List[Dict]) -> int:
        """Reorganize a list of splat dictionaries into a sequence of attributes.
        Args:
            splats_list (List[Dict]): List of splat dictionaries, each containing attributes like means, quats, etc.
        Returns:
            int: The side length of the padded square grid of Gaussians.
        """
        # splat list to sequence of attributes
        seq_attr_dict = self._splats_list_to_attribute_seq(splats_list)
        # pad
        
        n_gs = seq_attr_dict["means"].size(1)
        n_sidelen = int(np.ceil(n_gs**0.5))
        # n_sidelen should be a multiple of 8 for compression, HM require this
        if n_sidelen % 8 != 0:
            n_sidelen = ((n_sidelen // 8) + 1) * 8
            
        n_pad = n_sidelen**2 - n_gs
        padded_splats_videos = self._pad_attr_seq(seq_attr_dict, n_pad)
        
        # random access
        # if not self.use_all_intra:
            # logging.info("Using random access mode for compression.")
        if self.use_sort:
            # sort the splats with the first frame index
            sorted_indices = self._sort_with_frame_index(splats_list, n_pad)
            # use indices to sort the sequences of attributes
            for attr_name, padded_splats_video in padded_splats_videos.items():
                padded_splats_videos[attr_name] = padded_splats_video[:, sorted_indices, ...]
        # else: # all intra
        #     # logging.info("Using all intra mode for compression.")
        #     if self.use_sort:
        #         for fr_id, _ in enumerate(splats_list):
        #             sorted_indices = self._sort_with_frame_index(splats_list, n_pad, fr_id)

        #             for attr_name, padded_splats_video in padded_splats_videos.items():
        #                 padded_splats_video[fr_id] = padded_splats_video[fr_id][sorted_indices, ...]

        # reshape to 2d sequences
        self.splats_videos = {}
        for attr_name, padded_splats_video in padded_splats_videos.items(): 
            ori_shape = list(padded_splats_video.shape)
            new_shape = [ori_shape[0]] + [n_sidelen, n_sidelen] + ori_shape[2:]
            self.splats_videos[attr_name] = padded_splats_video.reshape(new_shape)

            print(attr_name, padded_splats_video.shape)

        # proprocessing on splats_videos
        self.splats_videos["quats"] = F.normalize(self.splats_videos["quats"], dim=-1)

        return n_sidelen

    def _deorganize(self, splats_videos_c: Dict[str, Tensor]) -> List[Dict]:
        """_summary_

        Args:
            splats_videos_c (Dict[str, Tensor]): _description_

        Returns:
            List[Dict]: _description_
        """
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
    
    def _save_metadata(self, compress_dir: Path, gop_id: int, meta: Dict[str, Any]) -> None:
        """Saves metadata to a JSON file in the specified directory."""
        meta_path = compress_dir / f"gop{gop_id}_metadata.json"
        with open(meta_path, 'w') as f:
            json.dump(meta, f, indent=4)
        logging.info(f"Metadata saved to {meta_path}")
        
    def _load_metadata(self, compress_dir: Path, gop_id: int) -> Dict[str, Any]:
        """Loads metadata from a JSON file in the specified directory."""
        meta_path = compress_dir / f"gop{gop_id}_metadata.json"
        if not meta_path.exists():
            raise FileNotFoundError(f"Metadata file not found: {meta_path}")
        
        with open(meta_path, 'r') as f:
            meta = json.load(f)
        logging.info(f"Metadata loaded from {meta_path}")
        return meta

    def _get_atribute_encode_configs(self, yuv_name: str, n_sidelen : int, frame_num : int, bit_depth : int=8) -> Dict[str, Any]:
        """Get the configuration parameters for a specific attribute based on its name."""
        common_config = {
            'frame_num': frame_num,
            'use_all_intra': self.use_all_intra,
            'width': n_sidelen,
            'height': n_sidelen,
            'bit_depth': bit_depth,
        }
        specific_config = {}
        if "means" in yuv_name:
            specific_config = self.attribute_configs["means"]
        elif "opacities" in yuv_name:
            specific_config = self.attribute_configs["opacities"]
        elif "quats" in yuv_name:
            if yuv_name.endswith("w"):
                specific_config = self.attribute_configs["quats"]["w"]
            elif yuv_name.endswith("xyz"):
                specific_config = self.attribute_configs["quats"]["xyz"]
        elif "scales" in yuv_name:
            specific_config = self.attribute_configs["scales"]
        elif "sh0" in yuv_name:
            specific_config = self.attribute_configs["sh0"]
        elif "shN" in yuv_name:
            sh_order = yuv_name.split("_")[-2]
            if sh_order in self.attribute_configs["shN"]:
                specific_config = self.attribute_configs["shN"][sh_order]
        else:
            logging.warning(f"Unknown attribute name {yuv_name}. Using default configuration.")
            specific_config = self.attribute_configs.get("default", {})
            
        # Combine common config with specific attribute config
        config_params = {**common_config, **specific_config}
        return config_params
        
    def compress(self, splats_list: List[Dict[str, Tensor]], compress_dir: Path, gop_id: int) -> None:
        """
        Compresses a list of splat frames.
        
        1. Reorganizes data into attribute-based videos.
        2. Saves each video as a raw YUV file using YUVDataHandler.
        3. Encodes each YUV file into a bitstream using VideoCodec.
        """
        if not splats_list:
            logging.error("Input splats_list is empty. Nothing to compress.")
            return

        logging.info("--- Starting Compression Process ---")
        
        # 1. Reorganize data
        n_sidelen = self._reorganize(splats_list)
        
        if isinstance(compress_dir, str):
            compress_dir = Path(compress_dir)
        if not compress_dir.exists():
            logging.info(f"Creating compression directory: {compress_dir}")
            compress_dir.mkdir(parents=True, exist_ok=True) 
        
        # 2. Setup YUV handler and save to YUV
        yuv_dir = compress_dir.parent / "yuv"
        if yuv_dir.exists():
            logging.info(f"Warning: YUV directory already exists: {yuv_dir}. Delete existing directory and recreate it.")
            import shutil
            shutil.rmtree(yuv_dir)  
        yuv_dir.mkdir(parents=True, exist_ok=False)  # Create new directory
        yuv_data_handler = YUVDataHandler(
            yuv_dir=yuv_dir, 
            gop_id=gop_id,
            n_sidelen=n_sidelen,
            splats_video_data=self.splats_videos
        )
        meta = yuv_data_handler.save_all_to_yuv()
        self._save_metadata(compress_dir, gop_id, meta)
        
        # 3. Encode each YUV to a bitstream
        video_codec = VideoCodec(
            video_codec_type=self.video_codec_type,
        )
        yuv_files = sorted([f for f in yuv_dir.glob("*.yuv")])
        if not yuv_files:
            logging.error(f"No YUV files found in {yuv_dir}. Cannot compress.")
            return
        for yuv_path in yuv_files:
            yuv_name = yuv_path.stem
            bitstream_path = compress_dir / f"{yuv_name}.mp4"
            config_params = self._get_atribute_encode_configs(yuv_name, n_sidelen, frame_num=len(splats_list))
            video_codec.encode(
                input_yuv=yuv_path,
                output_bitstream=bitstream_path,
                config_params=config_params
            )
        logging.info("--- Compression Process Finished ---")
        

    def decompress(self, compress_dir: Path, gop_id : int) -> List[Dict[str, Any]]:
        """
        Decompresses data from a compressed directory.

        1. Decodes each bitstream back to a YUV file.
        2. Loads data from YUV files using YUVDataHandler.
        3. Deorganizes attribute videos back into a list of splat frames.
        """
        logging.info("--- Starting Decompression Process ---")
        if isinstance(compress_dir, str):
            compress_dir = Path(compress_dir)
        yuv_dir =  compress_dir.parent / "yuv"
        if not yuv_dir.exists():
            raise FileNotFoundError(f"YUV directory not found: {yuv_dir}")

        # 1. Decode bitstreams to YUV
        # Find all metadata files to know which attributes to process

        meta = self._load_metadata(compress_dir, gop_id)
        
        video_codec = VideoCodec(
            video_codec_type=self.video_codec_type,
        )
        bin_files = [f for f in compress_dir.glob("*.mp4")] # sicheng: why mp4?
        if not bin_files:
            logging.error(f"No compressed files found in {compress_dir}. Cannot decompress.")
            return []
        for bin_path in bin_files:
            yuv_name = bin_path.stem
            yuv_path = yuv_dir / f"{yuv_name}_decoded.yuv"
            # In ffmpeg, it automatically use 10bit to encode videos, 
            # so we need to specify the pix_fmt at decoder side to truncate it to 8bit
            attr_type = yuv_name.split("_")[1]
            
            try:
                if attr_type == "quats":
                    yuv_suffix = yuv_name.split("_")[2]
                    pix_fmt = meta[attr_type][f"pix_fmt_{yuv_suffix}"]
                else:
                    pix_fmt = meta[attr_type]["pix_fmt"]
            except KeyError:
                logging.error(f"Pix fmt not found for {attr_type}. Using default pix_fmt.")
                pix_fmt = "yuv420p"

            if not yuv_path.exists():
                video_codec.decode(
                    input_bitstream=bin_path,
                    output_yuv=yuv_path,
                    config_params={"pix_fmt": pix_fmt}
                )
            else:
                logging.info(f"YUV file already exists: {yuv_path}. Skipping decoding.")
        # 2. Load from YUV and metadata
        yuv_data_handler = YUVDataHandler(
            yuv_dir=yuv_dir, 
            gop_id=gop_id,
            n_sidelen=None,
            splats_video_data=None
        )
        decompressed_videos = yuv_data_handler.load_all_from_yuv(meta)

        # 3. Deorganize back to splat list
        splats_list = self._deorganize(decompressed_videos)
        logging.info("--- Decompression Process Finished ---")
        return splats_list