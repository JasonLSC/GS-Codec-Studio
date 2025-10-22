import os
import re
import subprocess
from pathlib import Path
from typing import Optional, Tuple

from PIL import Image
import torch
import torchvision.transforms as transforms
from torchmetrics.image.lpip import LearnedPerceptualImagePatchSimilarity
from torch.utils.data import Dataset, DataLoader

def run_QMIV_metric(render_YUV_filename: Path, ref_YUV_filename: Path, 
                    render_start_frame: int = 0, ref_start_frame: int = 0,
                    saved_log_file: Optional[Path] = None, 
                    resolution: str = "2048x1088", pix_fmt: str = "yuv420p10le",
                    frame_num: int = 16):
    """
    Run QMIV quality metrics on YUV files in both YUV and RGB domains.
    
    This function executes QMIV tool twice:
    1. First in YUV domain to calculate YUV-based metrics
    2. Then in RGB domain with BT.709 color space conversion
    
    Args:
        render_YUV_filename (Path): Path to the rendered YUV file to be evaluated
        ref_YUV_filename (Path): Path to the reference YUV file
        render_start_frame (int): Index of the start frame of rendered video, usually as 0.
        ref_start_frame (int): Index of the start frame of reference video, not always as 0.
        saved_log_file (Optional[Path], optional): Path where the QMIV log will be saved. 
            If None, uses render file's stem + ".txt". Defaults to None.
        resolution (str, optional): Resolution of input videos in format "widthxheight". 
            Defaults to "2048x1088".
        pix_fmt (str, optional): Pixel format of input videos. 
            Defaults to "yuv420p10le".
        frame_num (int, optional): Number of frames.
            Defaults to 16.
    
    Returns:
        dict: Dictionary containing the following metrics:
            - "RGB_PSNR": PSNR value in RGB domain
            - "YUV_PSNR": PSNR value in YUV domain
            - "YUV_SSIM": SSIM value in YUV domain
            - "YUV_IVSSIM": IVSSIM value
    
    Note:
        - Requires QMIV executable in the current directory
        - Processes 65 frames as specified in the -nf parameter
        - Uses BT.709 color space for RGB conversion
        - Automatically overwrites existing log file if it exists
    """
        
    if saved_log_file is None:
        saved_log_file = render_YUV_filename.stem + ".txt"

    if os.path.exists(saved_log_file):
        os.remove(saved_log_file)
    
    # YUV domain
    QMIV_cmd = [
        "./helper/mpeg_gsc/QMIV",
        "-i0", render_YUV_filename,
        "-i1", ref_YUV_filename,
        "-s0", f"{render_start_frame}",
        "-s1", f"{ref_start_frame}",
        "-ps", resolution,
        "-pf", pix_fmt,
        "-nf", f"{frame_num}",
        "-r", saved_log_file,
        "-ml", "All"
    ]

    subprocess.run(QMIV_cmd, capture_output=True, text=True)
    # TODO:specify start frame of ref video
    # RGB domain
    QMIV_cmd = [
        "./helper/mpeg_gsc/QMIV",
        "-i0", render_YUV_filename,
        "-i1", ref_YUV_filename,
        "-s0", f"{render_start_frame}",
        "-s1", f"{ref_start_frame}",
        "-ps", resolution,
        "-pf", pix_fmt,
        "-nf", f"{frame_num}",
        "-csi", "YCbCr_BT709", "-csm", "RGB", "-cwa", "1:1:1:0", "-cws", "1:1:1:0",
        "-r", saved_log_file,
        "-ml", "All"
    ]

    subprocess.run(QMIV_cmd, capture_output=True, text=True)

    with open(saved_log_file, 'r') as f:
        content = f.read()

    yuv_psnr = float(re.search(r'PSNR\s+-YCbCr\s+(\d+\.\d+)', content).group(1))
    yuv_ssim = float(re.search(r'SSIM\s+-YCbCr\s+(\d+\.\d+)', content).group(1))
    yuv_ivssim = float(re.search(r'IVSSIM\s+(\d+\.\d+)', content).group(1))
    rgb_psnr = float(re.search(r'PSNR\s+-RGB\s+(\d+\.\d+)', content).group(1))

    ret_dict = {
        "RGB_PSNR": rgb_psnr,
        "YUV_PSNR": yuv_psnr,
        "YUV_SSIM": yuv_ssim,
        "YUV_IVSSIM": yuv_ivssim,
    }

    return ret_dict

def run_QMIV_metric_for_pngs(
    render_png_filename: Path, 
    ref_png_filename: Path, 
    render_start_frame: int = 0, 
    ref_start_frame: int = 0,
    saved_log_file: Optional[Path] = None, 
    resolution: str = "2048x1088",
    pix_fmt: str = "yuv420p10le",
    frame_num: int = 16
):
    """
    Compute QMIV quality metrics for PNG image sequences in both RGB and YUV domains.

    This function runs the QMIV tool twice:
    1. In the RGB domain to compute RGB-based PSNR.
    2. In the YUV domain (with BT.601 color space) to compute YUV-based PSNR, SSIM, and IVSSIM.

    Args:
        render_png_filename (Path): Path pattern to the rendered PNG sequence to be evaluated.
        ref_png_filename (Path): Path pattern to the reference PNG sequence.
        render_start_frame (int, optional): Start frame index for the rendered sequence. Default is 0.
        ref_start_frame (int, optional): Start frame index for the reference sequence. Default is 0.
        saved_log_file (Optional[Path], optional): Path to save the QMIV log file. If None, uses the stem of the render file. Default is None.
        resolution (str, optional): Resolution of the input images, e.g., "1920x1080". Default is "2048x1088".
        pix_fmt (str, optional): Pixel format (not used for PNG). Default is "yuv420p10le".
        frame_num (int, optional): Number of frames to process. Default is 16.

    Returns:
        dict: Dictionary with the following keys:
            - "RGB_PSNR": PSNR value in the RGB domain (float)
            - "YUV_PSNR": PSNR value in the YUV domain (float)
            - "YUV_SSIM": SSIM value in the YUV domain (float)
            - "YUV_IVSSIM": IVSSIM value in the YUV domain (float)

    Notes:
        - Requires the QMIV executable to be available in the specified directory.
        - The function will overwrite the log file if it already exists.
        - The function expects PNG sequences with consistent naming and frame count.
    """
        
    if saved_log_file is None:
        saved_log_file = render_png_filename.stem + ".txt"

    if os.path.exists(saved_log_file):
        os.remove(saved_log_file)
    
    # RGB domain
    QMIV_cmd = [
        "./helper/mpeg_gsc/QMIV",
        "-i0", render_png_filename,
        "-i1", ref_png_filename,
        "-ff", "PNG",
        "-ps", resolution,
        "-csi", "RGB", "-csm", "RGB", "-cwa", "1:1:1:0", "-cws", "1:1:1:0",
        "-ml", "PSNR",
        "-nth", "16", "-v", "2",
        "-r", saved_log_file
    ]

    result = subprocess.run(QMIV_cmd, capture_output=True, text=True)
    if result.stderr:
        print(result.stderr)
    # YCbCr domain
    QMIV_cmd = [
        "./helper/mpeg_gsc/QMIV",
        "-i0", render_png_filename,
        "-i1", ref_png_filename,
        "-ff", "PNG",
        "-ps", resolution,
        "-csi", "RGB", "-csm", "YCbCr_BT601",
        "-ml", "PSNR, SSIM, IVSSIM",
        "-nth", "16", "-v", "2",
        "-r", saved_log_file
    ]

    result = subprocess.run(QMIV_cmd, capture_output=True, text=True)
    if result.stderr:
        print(result.stderr)

    with open(saved_log_file, 'r') as f:
        content = f.read()

    yuv_psnr = float(re.search(r'PSNR\s+-YCbCr\s+(\d+\.\d+)', content).group(1))
    yuv_ssim = float(re.search(r'SSIM\s+-YCbCr\s+(\d+\.\d+)', content).group(1))
    yuv_ivssim = float(re.search(r'IVSSIM\s+(\d+\.\d+)', content).group(1))
    rgb_psnr = float(re.search(r'PSNR\s+-RGB\s+(\d+\.\d+)', content).group(1))

    ret_dict = {
        "RGB_PSNR": rgb_psnr,
        "YUV_PSNR": yuv_psnr,
        "YUV_SSIM": yuv_ssim,
        "YUV_IVSSIM": yuv_ivssim,
    }

    return ret_dict

class ImageSequenceDataset(Dataset):
    def __init__(self, file_pattern: str):
        """
        Dataset for loading image sequences
        
        Args:
            file_pattern: Complete file path pattern, e.g. "results/.../val_frame{:03d}_testv000.png"
        """
        path = Path(file_pattern).parent
        glob_pattern = format_to_glob_pattern(Path(file_pattern).name)
        self.files = sorted(path.glob(glob_pattern))
        
        self.transform = transforms.Compose([
            transforms.ToTensor(),  # Convert PIL image to tensor and normalize to [0,1]
        ])
    
    def __len__(self):
        return len(self.files)
    
    def __getitem__(self, idx):
        image = Image.open(self.files[idx])
        return self.transform(image)

def run_LPIPS_for_pngs(
    render_png_filename: Path, 
    ref_png_filename: Path, 
    lpips_calculator: LearnedPerceptualImagePatchSimilarity,
):
    device = lpips_calculator.device
    
    # Create datasets
    render_dataset = ImageSequenceDataset(str(render_png_filename))
    ref_dataset = ImageSequenceDataset(str(ref_png_filename))
    
    # Create dataloaders with multiple workers and prefetching
    batch_size = 2  # Adjust based on GPU memory
    num_workers = 4  # Adjust based on CPU cores
    
    render_loader = DataLoader(
        render_dataset, 
        batch_size=batch_size, 
        shuffle=False,  # Keep order for sequence
        num_workers=num_workers,
        pin_memory=True,  # Faster GPU transfer
        prefetch_factor=2  # Prefetch batches
    )
    
    ref_loader = DataLoader(
        ref_dataset, 
        batch_size=batch_size, 
        shuffle=False,
        num_workers=num_workers,
        pin_memory=True,
        prefetch_factor=2
    )
    
    lpips_values_list = []
    
    with torch.no_grad():
        for render_batch, ref_batch in zip(render_loader, ref_loader):
            # Move to device (faster with pin_memory=True)
            render_batch = render_batch.to(device, non_blocking=True)
            ref_batch = ref_batch.to(device, non_blocking=True)
            
            # Calculate LPIPS
            try:
                lpips_values = lpips_calculator(render_batch, ref_batch)
            except Exception as e:
                print(f"Error in LPIPS calculation: {e}")
                lpips_values = torch.tensor(float('nan'), device=device)
            
            # Convert to Python float and append
            lpips_values_list.append(lpips_values.item())
            
            # Optional: clear cache if memory is tight
            torch.cuda.empty_cache()
    
    # Calculate mean
    # If any value is NaN, the mean will be NaN automatically
    lpips_mean = sum(lpips_values_list) / len(lpips_values_list)
    
    return {"LPIPS": lpips_mean}

def format_to_glob_pattern(format_str):
    return re.sub(r'\{:0\d+d\}', '*', format_str)

def load_image_sequence_to_tensors(file_pattern: str):
    """
    Load image sequence and convert to tensor sequence
    
    Args:
        file_pattern: Complete file path pattern, e.g. "results/mpeg151/video_anchor/bartender/rp0/renders/val_frame{:03d}_testv000.png"
    
    Returns:
        torch.Tensor: A tensor in (N,C,H,W) format, with values normalized to [0,1]
    """
    # Convert to Path object and get matching files
    path = Path(file_pattern).parent  # Get parent directory
    glob_pattern = format_to_glob_pattern(Path(file_pattern).name)  # Convert only the filename part to glob pattern
    files = sorted(path.glob(glob_pattern))
    
    # Define preprocessing transform
    transform = transforms.Compose([
        transforms.ToTensor(),  # Convert PIL image to tensor and normalize to [0,1]
    ])
    
    # Load images and convert to tensors
    tensors = [transform(Image.open(file)) for file in files]
    
    # Stack tensors along a new dimension
    return torch.stack(tensors, dim=0)  # (N,3,H,W)

def convert_mp4_to_yuv(input_mp4: Path) -> Tuple[bool, Path]:
    """
    Convert MP4 to YUV using ffmpeg
    
    Args:
        input_mp4: Input MP4 filename
        
    Returns:
        bool: True if conversion successful, False otherwise
    """
    # Check if input file exists
    if not os.path.exists(input_mp4):
        print(f"Error: Input file {input_mp4} does not exist")
        return False, None

    # Generate output filename by replacing extension
    output_yuv = input_mp4.with_suffix('.yuv')

    # Check if output file exists
    if os.path.exists(output_yuv):
        os.remove(output_yuv)

    # Construct ffmpeg command
    cmd = [
        'ffmpeg', '-i', str(input_mp4),
        '-vf', 'scale=in_range=pc:in_color_matrix=bt709:out_range=pc:out_color_matrix=bt709',
        '-pix_fmt', 'yuv420p10le',
        '-colorspace', 'bt709',
        '-color_primaries', 'bt709', 
        '-color_trc', 'bt709',
        '-color_range', 'pc',
        '-sws_flags', 'lanczos+bitexact+full_chroma_int+full_chroma_inp',
        '-f', 'rawvideo',
        str(output_yuv)
    ]
    
    try:
        # Execute ffmpeg command
        subprocess.run(cmd, check=True, capture_output=True)
        print(f"Successfully converted {input_mp4} to {output_yuv}")
        return True, output_yuv
    except subprocess.CalledProcessError as e:
        print(f"Error during conversion: {e.stderr.decode()}")
        return False, None
    except Exception as e:
        print(f"Unexpected error: {str(e)}")
        return False, None

DATASET_INFOS = {
    "CBA": {
        "start_frame": 0,
        "resolution": "2048x1088"
    },
    "Bartender": {
        "start_frame": 50,
        "resolution": "1920x1080"
    },
    "Choreo_Dark": {
        "start_frame": 30,
    },
    "Cinema": {
        "start_frame": 235,
    },
}

if __name__ == "__main__":
    SCENE = "CBA"

    success, output_path = convert_mp4_to_yuv(Path('../temp/val_step29999_testv0.mp4'))
    if success:
        results_dict = run_QMIV_metric(output_path,
                            Path("../temp/v07_texture_2048x1088_yuv420p10le.yuv"),
                            render_start_frame=0,
                            ref_start_frame=DATASET_INFOS[SCENE]["start_frame"],
                            resolution=DATASET_INFOS[SCENE]["resolution"]
                        )
    print(results_dict)


