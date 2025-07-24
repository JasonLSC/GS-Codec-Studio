import json
import argparse
import os
from typing import Tuple, Optional, List, Any, Dict

import torch
import torch.nn.functional as F

# Assuming these are in your utils.py and datasets/colmap.py
from utils import load_ply, save_ply
from datasets.colmap import Dataset, GSCDataset, Parser
from gsplat.rendering import rasterization


def _quantize_tensor(
    params: torch.Tensor, bit_depth: int = 8
) -> Tuple[torch.Tensor, torch.Tensor, torch.Tensor, List[int]]:
    """Quantizes a tensor to a specified bit depth using per-channel min-max normalization."""
    # Calculate the minimum and maximum value for each channel
    mins = torch.amin(params, dim=tuple(range(params.ndim - 1)), keepdim=True)
    maxs = torch.amax(params, dim=tuple(range(params.ndim - 1)), keepdim=True)
    scale = maxs - mins
    scale[scale == 0] = 1.0  # Prevent division by zero

    params_norm = (params - mins) / scale

    max_val = (2**bit_depth) - 1
    uint_type = f"uint{bit_depth}" if bit_depth == 8 else "int32"  # PyTorch does not have uint16
    video = (params_norm * max_val).round().to(getattr(torch, uint_type))

    # Squeeze for easier serialization
    mins = mins.squeeze(tuple(range(params.ndim - 1)))
    maxs = maxs.squeeze(tuple(range(params.ndim - 1)))
    original_shape = list(params.shape)
    return video, mins, maxs, original_shape

def _dequantize_tensor(video: torch.Tensor, meta: Dict[str, Any]) -> torch.Tensor:
    """Dequantizes a video tensor using per-channel metadata."""
    # Restore the min and max for each channel
    mins = torch.tensor(meta["mins"], device=video.device).view(*([1] * (video.ndim - 1)), -1)
    maxs = torch.tensor(meta["maxs"], device=video.device).view(*([1] * (video.ndim - 1)), -1)

    bit_depth = meta.get("bit_depth", 8)
    max_val = (2**bit_depth) - 1

    params_norm = video.to(torch.float32) / max_val
    params = params_norm * (maxs - mins) + mins

    params = params.reshape(meta["shape"])
    dtype_str = str(meta["dtype"])
    if 'torch.' not in dtype_str:
        dtype_str = f'torch.{dtype_str}'

    params = params.to(dtype=eval(dtype_str))
    return params

def _save_metadata(meta: Dict[str, Any], metadata_path: str):
    with open(metadata_path, "w") as f:
        json.dump(meta, f)

def _load_metadata(metadata_path: str):
    with open(metadata_path, "r") as f:
        meta = json.load(f)
    return meta

def preprocess_gs_ply(ply_path: str, metadata_path: str, output_path: str):
    """
    Preprocess the GS PLY file.

    Args:
        ply_path: The path to the GS PLY file.
        metadata_path: The path to the metadata file.
        output_path: The path to the output GS PLY file.
    
    Returns:
        dict: A dictionary containing Gaussian splats obtained before preprocessing.
    """
    splats = load_ply(ply_path)

    splats["quats"] = F.normalize(splats["quats"], dim=-1)

    quantized_splats = {}
    meta = {}
    for key, value in splats.items():
        bit_depth = 16 if key == "means" else 8
        q_value, mins, maxs, shape = _quantize_tensor(value, bit_depth)

        quantized_splats[key] = q_value
        meta[key] = {
            "shape": shape,
            "dtype": str(value.dtype),
            "mins": mins.tolist(),
            "maxs": maxs.tolist(),
            "bit_depth": bit_depth
        }
    
    save_ply(quantized_splats, output_path)
    _save_metadata(meta, metadata_path)

    return splats

def postprocess_gs_ply(ply_path: str, metadata_path: str, output_path: str):
    """
    Postprocess the GS PLY file.

    Args:
        ply_path: The path to the GS PLY file.
        metadata_path: The path to the metadata file for dequantization.
        output_path: The path to the output GS PLY file.
    
    Returns:
        dict: A dictionary containing Gaussian splats obtained after postprocessing.
    """
    splats = load_ply(ply_path)
    meta = _load_metadata(metadata_path)

    dequantized_splats = {}
    for key, value in splats.items():
        dequantized_splats[key] = _dequantize_tensor(value, meta[key])

    save_ply(dequantized_splats, output_path)

    return dequantized_splats

def rasterize_splats(
        cfg, # Added cfg to match the original function, assuming it's available in the class
        camtoworlds: torch.Tensor,
        Ks: torch.Tensor,
        width: int,
        height: int,
        masks: Optional[torch.Tensor] = None,
        splats: Optional[torch.nn.ParameterDict] = None,
        **kwargs,
    ) -> Tuple[torch.Tensor, torch.Tensor, Dict]:
        if splats is not None:
            means = splats["means"] # [N, 3]
            quats = splats["quats"] # [N, 4]
            scales = torch.exp(splats["scales"])  # [N, 3]
            opacities = torch.sigmoid(splats["opacities"])  # [N,]
            sh0, shN = splats["sh0"], splats["shN"]
        else:
            raise NotImplementedError(f"Should pass splats dict.")
    
        colors = torch.cat([sh0, shN], 1)  # [N, K, 3]

        # Assuming cfg.antialiased is defined where this function is typically called
        rasterize_mode = "antialiased" if cfg.antialiased else "classic"
        render_colors, render_alphas, info = rasterization(
            means=means,
            quats=quats,
            scales=scales,
            opacities=opacities,
            colors=colors,
            viewmats=torch.linalg.inv(camtoworlds),  # [C, 4, 4]
            Ks=Ks,  # [C, 3, 3]
            width=width,
            height=height,
            packed=False,
            absgrad=False,
            sparse_grad=False,
            rasterize_mode=rasterize_mode,
            distributed=False,
            camera_model="pinhole",
            **kwargs,
        )
        if masks is not None:
            render_colors[~masks] = 0
        return render_colors, render_alphas, info

def calculate_psnr(img1: torch.Tensor, img2: torch.Tensor, max_pixel: float = 1.0) -> float:
    """Calculate PSNR between two images."""
    mse = torch.mean((img1 - img2) ** 2)
    if mse == 0:
        return float('inf')  # If MSE is zero, PSNR is infinite
    psnr = 10 * torch.log10((max_pixel ** 2) / mse)
    return psnr.item()

def test_ply_quantization(raw_ply_path: str, dequantized_ply_path: str, colmap_path: str):
    """
    Test for the quantization error of the PLY file.
    """
    raw_splats = load_ply(raw_ply_path)
    dequantized_splats = load_ply(dequantized_ply_path)
    splats_dict = {
        "raw": raw_splats,
        "dequantized": dequantized_splats
    }

    # Load colmap poses
    parser = Parser(
        data_dir=colmap_path,
        factor=1,
        normalize=False,
        test_every=1,
    )
    valset = GSCDataset(
        parser,
        split="val",
        test_view_ids="all"
    )

    valloader = torch.utils.data.DataLoader(
        valset, batch_size=1, shuffle=False, num_workers=1
    )

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

    render_results = {}
    
    # Create a dummy cfg object for rasterize_splats
    class Cfg:
        def __init__(self, antialiased: bool = False):
            self.antialiased = antialiased
    dummy_cfg = Cfg(antialiased=True) # You can set this based on your needs

    for name, splats in splats_dict.items():
        # Ensure splats are on the correct device for rasterization
        splats_on_device = {k: v.to(device) for k, v in splats.items()}


        render_results[name] = []
        for v_id, data in enumerate(valloader):
            camtoworlds = data["camtoworld"].to(device)
            Ks = data["K"].to(device)
            pixels = data["image"].to(device) / 255.0
            masks = data["mask"].to(device) if "mask" in data else None
            height, width = pixels.shape[1:3]

            colors, _, _ = rasterize_splats(
                cfg=dummy_cfg, # Pass the dummy cfg object
                camtoworlds=camtoworlds,
                Ks=Ks,
                width=width,
                height=height,
                sh_degree=3,
                near_plane=0.01,
                far_plane=1e10,
                masks=masks,
                splats=splats_on_device # must need
            )  # [1, H, W, 3]

            render_results[name].append(colors.squeeze(0)) # Squeeze the batch dimension

    # Stack the list of tensors into a single tensor
    render_results = {k: torch.cat(v, 0) for k, v in render_results.items()}
    psnr_value = calculate_psnr(render_results["raw"], render_results["dequantized"])
    print(f"PSNR between the rendered images from raw and dequantized: {psnr_value:.2f} dB")


def parse_args():
    parser = argparse.ArgumentParser(description="GS PLY processing script")

    # Add arguments for preprocess, postprocess, and evaluate as optional flags
    parser.add_argument("--preprocess", action="store_true", help="Perform preprocessing of the PLY file.")
    parser.add_argument("--postprocess", action="store_true", help="Perform postprocessing (dequantization) of the PLY file.")
    parser.add_argument("--evaluate", action="store_true", help="Perform evaluation of quantization error.")

    parser.add_argument("--raw_ply_path", type=str, help="Path to the raw input PLY file.")
    parser.add_argument("--exp_dir", type=str, required=True, help="Directory to save output files.")
    parser.add_argument("--colmap_path", type=str, help="Path to the COLMAP directory for evaluation poses (required if evaluate is true).")

    # Arguments for configurable filenames
    parser.add_argument("--metadata_filename", type=str, default="metadata.json",
                        help="Filename for the metadata JSON file. Default is 'metadata.json'.")
    parser.add_argument("--quantized_ply_filename", type=str, default="quantized.ply",
                        help="Filename for the quantized PLY output. Default is 'quantized.ply'.")
    parser.add_argument("--dequantized_ply_filename", type=str, default="dequantized.ply",
                        help="Filename for the dequantized PLY output. Default is 'dequantized.ply'.")

    args = parser.parse_args()

    # Conditional check for --colmap_path
    if args.evaluate and not args.colmap_path:
        parser.error("--colmap_path is required when --evaluate is set.")

    return args

if __name__ == "__main__":
    args = parse_args()

    if args.exp_dir:
        os.makedirs(args.exp_dir, exist_ok=True)

    # Use the configured filenames
    metadata_path = os.path.join(args.exp_dir, args.metadata_filename)
    quantized_ply_path = os.path.join(args.exp_dir, args.quantized_ply_filename)
    dequantized_ply_path = os.path.join(args.exp_dir, args.dequantized_ply_filename)

    if args.preprocess:
        print(f"Running preprocessing...")
        print(f"  Input PLY: {args.raw_ply_path}")
        print(f"  Output directory: {args.exp_dir}")
        print(f"  Metadata filename: {args.metadata_filename}")
        print(f"  Quantized PLY filename: {args.quantized_ply_filename}")
        preprocess_gs_ply(args.raw_ply_path, metadata_path, quantized_ply_path)
        print(f"  Saved quantized PLY to: {quantized_ply_path}")
        print(f"  Saved metadata to: {metadata_path}")

    if args.postprocess:
        print(f"Running postprocessing...")
        print(f"  Input (quantized) PLY: {quantized_ply_path}")
        print(f"  Input metadata: {metadata_path}")
        print(f"  Output directory: {args.exp_dir}")
        print(f"  Dequantized PLY filename: {args.dequantized_ply_filename}")
        postprocess_gs_ply(quantized_ply_path, metadata_path, dequantized_ply_path)
        print(f"  Saved dequantized PLY to: {dequantized_ply_path}")

    # Automatic evaluation if both preprocess and postprocess were run
    if args.preprocess and args.postprocess:
        print(f"\nBoth preprocessing and postprocessing completed. Running automatic evaluation...")
        print(f"  Raw PLY: {args.raw_ply_path}")
        print(f"  Dequantized PLY: {dequantized_ply_path}")
        print(f"  COLMAP data: {args.colmap_path}")
        test_ply_quantization(args.raw_ply_path, dequantized_ply_path, args.colmap_path)
    elif args.evaluate: # Only run explicit evaluate if not automatically triggered
        print(f"\nRunning explicit evaluation...")
        print(f"  Raw PLY: {args.raw_ply_path}")
        print(f"  Dequantized PLY: {dequantized_ply_path}")
        print(f"  COLMAP data: {args.colmap_path}")
        test_ply_quantization(args.raw_ply_path, dequantized_ply_path, args.colmap_path)