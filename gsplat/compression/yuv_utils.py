import os
import subprocess
from typing import Literal

import torch
import torch.nn.functional as F
from torch import Tensor


def _save_tensor_to_yuv(video: torch.Tensor,
                        yuv_file_path: str,
                        chroma_subsampling: Literal["420", "444", "400"] = "420"):
    '''
    Save a tensor to a YUV file in planar format.

    Args:
        video (torch.Tensor): video in the shape of [T, H, W, 3], assumed to be uint8 and contain YUV data.
                              For 420, the U and V channels might conceptually represent subsampling
                              (e.g., via `chroma_downsampling` function), but still have HxW resolution.
        compress_dir (str): directory to save the YUV file
        param_name (str): name of the parameter used for the filename
        chroma_subsampling (str): chroma subsampling format ("420" or "444"), which determines the output file structure.
    '''
    n_frames = int(video.size(0))
    H = int(video.size(1))
    W = int(video.size(2))

    # Ensure tensor is uint8 and on CPU
    if video.dtype != torch.uint8:
        # If the input is float [0, 1], scale and convert. Otherwise, assume it's compatible.
        if video.dtype.is_floating_point:
             print(f"Warning: Input tensor for _save_tensor_to_yuv is float, converting to uint8 assuming range [0, 1].")
             video = (video.clamp(0, 1) * 255).round().to(torch.uint8)
        else:
             print(f"Warning: Input tensor for _save_tensor_to_yuv is not uint8 ({video.dtype}), attempting direct conversion.")
             video = video.to(torch.uint8) # Potential data loss or wrap-around

    video_np = video.cpu().numpy() # Shape: (T, H, W, 3)

    with open(yuv_file_path, 'wb') as f:
        for t in range(n_frames):
            frame = video_np[t] # Shape: (H, W, 3)
            y_plane = frame[..., 0] # Shape: (H, W)
            # Write Y plane (always HxW)
            f.write(y_plane.tobytes())
            if chroma_subsampling == "400":
                # For YUV400, we only write the Y plane, U and V are not present
                continue
            u_plane = frame[..., 1] # Shape: (H, W)
            v_plane = frame[..., 2] # Shape: (H, W)

            if chroma_subsampling == "444":
                # Write U plane (HxW)
                f.write(u_plane.tobytes())
                # Write V plane (HxW)
                f.write(v_plane.tobytes())
            elif chroma_subsampling == "420":
                # Perform actual 2x2 subsampling for U and V before writing
                # Simple subsampling: take the top-left pixel of each 2x2 block
                u_subsampled = u_plane[::2, ::2] # Shape: (H/2, W/2)
                v_subsampled = v_plane[::2, ::2] # Shape: (H/2, W/2)
                # Write U plane ((H/2)x(W/2))
                f.write(u_subsampled.tobytes())
                # Write V plane ((H/2)x(W/2))
                f.write(v_subsampled.tobytes())
            else:
                raise ValueError(f"Unsupported chroma subsampling format: {chroma_subsampling}")

    print(f"Saved YUV file to: {yuv_file_path} with format {chroma_subsampling}")

def _encode_yuv_to_video_stream(
    yuv_file_path: str,
    output_video_path: str,
    width: int,
    height: int,
    pix_fmt: Literal["yuv420p", "yuv444p"],
    qp: int,
    use_all_intra: bool,
    use_chroma_qp_offset: bool = False
) -> None:
    """
    Encodes a raw YUV file to HEVC (libx265) using FFmpeg.

    Args:
        yuv_file_path (str): Path to the input raw YUV file.
        output_video_path (str): Path for the output HEVC video file (.mp4 or .265).
        width (int): Width of the video frames.
        height (int): Height of the video frames.
        pix_fmt (Literal["yuv420p", "yuv444p"]): Pixel format of the input YUV file.
        qp (int): Quantization Parameter for HEVC encoding.
        use_all_intra (bool): Whether to force all frames to be I-frames.

    Raises:
        RuntimeError: If FFmpeg execution fails.
        FileNotFoundError: If ffmpeg command is not found.
    """
    video_size = f"{width}x{height}"
    print(f"Encoding {os.path.basename(yuv_file_path)} to {os.path.basename(output_video_path)} "
          f"with QP={qp}, Intra={use_all_intra}, Format={pix_fmt}, Size={video_size}")

    # Build x265 parameters string
    x265_params_list = [f"qp={qp}"]
    if use_all_intra:
        # Enforce I-frames only with keyint=1 and scenecut=0
        x265_params_list.append("keyint=1:min-keyint=1:scenecut=0")
    if use_chroma_qp_offset:
        # Use a fixed chroma QP offset of 6 for all frames
        x265_params_list.append("cbqpoffs=10:crqpoffs=10")
    else:
        x265_params_list.append("cbqpoffs=1:crqpoffs=1")  
    x265_params = ":".join(x265_params_list)

    # Construct the ffmpeg command
    cmd = [
        "ffmpeg",
        "-f", "rawvideo",            # Input format is raw video
        "-pix_fmt", pix_fmt,         # Input pixel format (e.g., yuv420p)
        "-s:v", video_size,          # Input size (WidthxHeight)
        "-i", yuv_file_path,         # Input YUV file path
        "-c:v", "libx265",           # Codec to use for video encoding
        "-x265-params", x265_params, # Pass parameters to libx265
        "-y",                        # Overwrite output file if it exists
        output_video_path            # Path for the output encoded video file
        ]

    cmd_str = " ".join(cmd) # Create a string representation for logging
    print(f"Executing ffmpeg command: {cmd_str}")

    try:
        # Run the ffmpeg command
        # check=True raises CalledProcessError on non-zero exit code
        # capture_output=True captures stdout and stderr
        # text=True decodes stdout/stderr as text
        # encoding/errors handle potential decoding issues with ffmpeg output
        result = subprocess.run(cmd, check=True, capture_output=True, text=True, encoding='utf-8', errors='ignore')
        # Print stderr as it often contains useful encoding progress and statistics
        print(f"FFmpeg stderr:\n{result.stderr}")
        print(f"Successfully encoded to {output_video_path}")

    except subprocess.CalledProcessError as e:
        # Handle errors during ffmpeg execution
        print(f"Error during ffmpeg execution for {os.path.basename(yuv_file_path)}:")
        print(f"Command: {' '.join(e.cmd)}")
        print(f"Return code: {e.returncode}")
        # Decode stdout/stderr safely in case they are bytes
        stdout = e.stdout.decode('utf-8', errors='ignore') if isinstance(e.stdout, bytes) else e.stdout
        stderr = e.stderr.decode('utf-8', errors='ignore') if isinstance(e.stderr, bytes) else e.stderr
        print(f"Stdout: {stdout}")
        print(f"Stderr: {stderr}")
        # Re-raise as a RuntimeError for the calling function to handle
        raise RuntimeError(f"FFmpeg encoding failed for {os.path.basename(yuv_file_path)}") from e
    except FileNotFoundError:
        # Handle case where ffmpeg executable is not found
        print("Error: ffmpeg command not found. Make sure ffmpeg is installed and in your system's PATH.")
        raise

def _decode_video_stream_to_yuv(
        video_file: str,
        yuv_file: str,
        width: int,
        height: int,
        pix_fmt: str
) -> None:
    """
    Decodes a video file (e.g., HEVC encoded) to a raw YUV file using FFmpeg.

    Args:
        video_file (str): Path to the input video file (.mp4, .265, etc.).
        yuv_file (str): Path for the output raw YUV file.
        width (int): Expected width of the video frames (used for output size).
        height (int): Expected height of the video frames (used for output size).
        pix_fmt (str): Expected pixel format of the output YUV file (e.g., "yuv420p", "yuv444p").

    Raises:
        RuntimeError: If FFmpeg execution fails.
        FileNotFoundError: If ffmpeg command is not found.
    """
    video_size = f"{width}x{height}"
    print(f"Decoding {os.path.basename(video_file)} to {os.path.basename(yuv_file)} "
          f"with Format={pix_fmt}, Size={video_size}")

    # Construct the ffmpeg command for decoding
    cmd = [
        "ffmpeg",
        "-i", video_file,            # Input video file path
        "-f", "rawvideo",            # Output format is raw video
        "-pix_fmt", pix_fmt,         # Output pixel format (e.g., yuv420p)
        "-s:v", video_size,          # Output size (WidthxHeight) - important for raw output
        "-y",                        # Overwrite output file if it exists
        yuv_file                     # Path for the output raw YUV file
    ]

    cmd_str = " ".join(cmd) # Create a string representation for logging
    print(f"Executing ffmpeg command: {cmd_str}")

    try:
        # Run the ffmpeg command
        result = subprocess.run(cmd, check=True, capture_output=True, text=True, encoding='utf-8', errors='ignore')
        # Print stderr as it often contains useful decoding information or warnings
        print(f"FFmpeg stderr:\n{result.stderr}")
        print(f"Successfully decoded to {yuv_file}")

    except subprocess.CalledProcessError as e:
        # Handle errors during ffmpeg execution
        print(f"Error during ffmpeg execution for {os.path.basename(video_file)}:")
        print(f"Command: {' '.join(e.cmd)}")
        print(f"Return code: {e.returncode}")
        stdout = e.stdout.decode('utf-8', errors='ignore') if isinstance(e.stdout, bytes) else e.stdout
        stderr = e.stderr.decode('utf-8', errors='ignore') if isinstance(e.stderr, bytes) else e.stderr
        print(f"Stdout: {stdout}")
        print(f"Stderr: {stderr}")
        raise RuntimeError(f"FFmpeg decoding failed for {os.path.basename(video_file)}") from e
    except FileNotFoundError:
        # Handle case where ffmpeg executable is not found
        print("Error: ffmpeg command not found. Make sure ffmpeg is installed and in your system's PATH.")
        raise

def _load_yuv_to_tensor(
        yuv_file: str,
        height: int,
        width: int,
        pix_fmt: Literal["yuv420p", "yuv444p", "yuv400p"],
        bit_depth: int = 8 # The Default bit depth of the Decoded YUV file. 250705, sicheng: change 10 to 8. seems like the decoded pixfmt from ffmpeg is 8bit.
) -> Tensor:
    """
    Load a raw YUV file into a PyTorch tensor.

    Reads planar YUV data and reconstructs it into a tensor of shape [T, H, W, 3]
    with dtype uint8.
    For yuv420p, chroma channels (U, V) are upsampled using nearest neighbor
    interpolation to match the luma (Y) resolution, effectively replicating
    chroma values across 2x2 blocks.

    Args:
        yuv_file (str): Path to the input raw YUV file.
        height (int): Height of the video frames.
        width (int): Width of the video frames.
        pix_fmt (Literal["yuv420p", "yuv444p"]): Pixel format of the YUV file.

    Returns:
        Tensor: A tensor containing the video data in shape [T, H, W, 3] and dtype uint8.

    Raises:
        FileNotFoundError: If the yuv_file does not exist.
        ValueError: If the file size is not consistent with the dimensions and pix_fmt,
                    or if an unsupported pix_fmt is provided.
    """
    if bit_depth == 8:
        pix_bytes = 1  # 8-bit YUV
        _dtype = torch.uint8
    elif bit_depth == 10:
        pix_bytes = 2
        _dtype = torch.int16
    else:
        raise ValueError(f"Unsupported bit depth: {bit_depth}. Only 8 and 10 bits are supported.")
    
    if not os.path.exists(yuv_file):
        raise FileNotFoundError(f"YUV file not found: {yuv_file}")

    file_size = os.path.getsize(yuv_file)

    # Calculate the size of one frame in bytes
    if pix_fmt == "yuv444p":
        bytes_per_frame = pix_bytes * height * width * 3
        chroma_height, chroma_width = height, width
    elif pix_fmt == "yuv420p":
        # Y plane: H*W, U plane: (H/2)*(W/2), V plane: (H/2)*(W/2)
        bytes_per_frame = pix_bytes * int(height * width * 1.5)
        chroma_height, chroma_width = height // 2, width // 2
        if height % 2 != 0 or width % 2 != 0:
             print(f"Warning: Height ({height}) or Width ({width}) is odd for YUV420p. Chroma dimensions calculated using integer division.")
    elif pix_fmt == "yuv400p":
        # Y plane only, no chroma planes
        bytes_per_frame = pix_bytes * height * width
        chroma_height, chroma_width = 0, 0
    else:
        raise ValueError(f"Unsupported pix_fmt: {pix_fmt}")

    if bytes_per_frame == 0:
        raise ValueError("Calculated bytes_per_frame is zero. Check height/width.")

    if file_size == 0:
        print(f"Warning: YUV file is empty: {yuv_file}")
        return torch.empty((0, height, width, 3), dtype=_dtype)

    if file_size % bytes_per_frame != 0:
        raise ValueError(
            f"File size {file_size} is not a multiple of calculated frame size {bytes_per_frame} "
            f"for H={height}, W={width}, pix_fmt={pix_fmt}."
        )

    num_frames = file_size // bytes_per_frame
    print(f"Reading {num_frames} frames from {os.path.basename(yuv_file)}...")

    y_plane_size = pix_bytes * height * width
    chroma_plane_size = pix_bytes * chroma_height * chroma_width

    all_frames = []

    with open(yuv_file, 'rb') as f:
        for _ in range(num_frames):
            # Read Y plane
            y_plane_bytes = f.read(y_plane_size)
            if len(y_plane_bytes) != y_plane_size:
                 raise IOError(f"Failed to read full Y plane for frame {len(all_frames)}.")
            y_plane = torch.frombuffer(y_plane_bytes, dtype=_dtype).reshape((height, width))
            if pix_fmt == "yuv400p":
                # For YUV400, we only have the Y plane, so we can skip reading U and V planes
                frame_tensor = y_plane.unsqueeze(-1)  # Shape: (H, W, 1)
                all_frames.append(frame_tensor)
                continue
            # Read U plane
            u_plane_bytes = f.read(chroma_plane_size)
            if len(u_plane_bytes) != chroma_plane_size:
                 raise IOError(f"Failed to read full U plane for frame {len(all_frames)}.")
            u_plane = torch.frombuffer(u_plane_bytes, dtype=_dtype).reshape((chroma_height, chroma_width))

            # Read V plane
            v_plane_bytes = f.read(chroma_plane_size)
            if len(v_plane_bytes) != chroma_plane_size:
                 raise IOError(f"Failed to read full V plane for frame {len(all_frames)}.")
            v_plane = torch.frombuffer(v_plane_bytes, dtype=_dtype).reshape((chroma_height, chroma_width))

            # Upsample chroma if needed (for yuv420p)
            if pix_fmt == "yuv420p":
                # Add batch and channel dims: [H/2, W/2] -> [1, 1, H/2, W/2]
                u_plane_unsqueezed = u_plane.float().unsqueeze(0).unsqueeze(0)
                v_plane_unsqueezed = v_plane.float().unsqueeze(0).unsqueeze(0)

                # Interpolate to full resolution: [1, 1, H/2, W/2] -> [1, 1, H, W]
                u_upsampled = F.interpolate(u_plane_unsqueezed, size=(height, width), mode='nearest')
                v_upsampled = F.interpolate(v_plane_unsqueezed, size=(height, width), mode='nearest')

                # Remove batch and channel dims and convert back to uint8: [1, 1, H, W] -> [H, W]
                u_plane = u_upsampled.squeeze(0).squeeze(0).byte()
                v_plane = v_upsampled.squeeze(0).squeeze(0).byte()

            # Stack planes: (H, W), (H, W), (H, W) -> (H, W, 3)
            frame_tensor = torch.stack([y_plane, u_plane, v_plane], dim=-1)
            all_frames.append(frame_tensor)

    # Stack all frames: List[(H, W, 3)] -> [T, H, W, 3]
    video_tensor = torch.stack(all_frames, dim=0)
    if bit_depth == 10:
        # Convert to unit8 if the original data was in 10-bit format
        video_tensor = (video_tensor >> 2).to(torch.uint8)  # right shift by 2 to convert 10-bit to 8-bit
        # video_tensor = (video_tensor.float() * 255.0 / 1023.0).round().clamp(0, 255).to(torch.uint8)
    return video_tensor

def gaussian_downsample(input_tensor: torch.Tensor, sigma: float = 1.0, kernel_size: int = 5) -> torch.Tensor:
    """
    Apply Gaussian filtering followed by downsampling to reduce aliasing artifacts.
    
    Args:
        input_tensor (torch.Tensor): Input tensor with shape [B, C, H, W]
        sigma (float): Standard deviation of the Gaussian kernel, controls smoothing strength
        kernel_size (int): Size of the Gaussian kernel, should be odd
        
    Returns:
        torch.Tensor: Downsampled tensor with shape [B, C, H/2, W/2]
    """
    # Ensure kernel size is odd
    if kernel_size % 2 == 0:
        kernel_size += 1
        
    # Create Gaussian kernel
    device = input_tensor.device
    center = kernel_size // 2
    x = torch.arange(kernel_size, device=device) - center
    x_grid, y_grid = torch.meshgrid(x, x, indexing='ij')
    gaussian_kernel = torch.exp(-(x_grid.pow(2) + y_grid.pow(2)) / (2 * sigma**2))
    gaussian_kernel = gaussian_kernel / gaussian_kernel.sum()  # Normalize
    
    # Prepare kernel for each input channel
    channels = input_tensor.size(1)
    gaussian_kernel = gaussian_kernel.view(1, 1, kernel_size, kernel_size)
    gaussian_kernel = gaussian_kernel.repeat(channels, 1, 1, 1)
    
    # Apply group convolution to process all channels simultaneously
    pad_size = center
    input_padded = F.pad(input_tensor, (pad_size, pad_size, pad_size, pad_size), mode='reflect')
    filtered = F.conv2d(input_padded, gaussian_kernel, groups=channels)
    
    # Downsample: take every 2nd pixel
    downsampled = filtered[:, :, ::2, ::2]
    
    return downsampled


def chroma_downsampling(video: torch.Tensor) -> torch.Tensor:
    '''
    Chroma downsampling for YUV video from 4:4:4 to 4:2:0 format conceptually,
    while maintaining the original tensor shape.
    The U and V values within each 2x2 block will be the average
    of the original U and V values in that block.

    Args:
        video (torch.Tensor): Input YUV video tensor in either of these shapes:
                             - [T, H, W, 3] (YUV444 for regular video)
                             - [T, H, W, N, 3] (YUV444 for SH coefficients, where N is number of coefficients)
                             Assumed to be float type for pooling/interpolation.

    Returns:
        torch.Tensor: Output video tensor in the same shape as input,
                     where U and V channels represent 4:2:0 subsampling.
    '''
    assert video.shape[-1] == 3, "Input video must have 3 channels (YUV) in the last dimension"
    assert video.dim() in [4, 5], "Input video must have either 4 or 5 dimensions"

    # Store original shape
    original_shape = video.shape
    
    # Unify input processing to [B, H, W, 3] shape
    if video.dim() == 5:
        # [T, H, W, N, 3] -> [T*N, H, W, 3]
        B = original_shape[0] * original_shape[3]  # T * N
        video = video.permute(0, 3, 1, 2, 4).reshape(B, original_shape[1], original_shape[2], 3)
    
    # Separate Y, U, V channels
    y_channel = video[..., 0]  # Shape: [B, H, W]
    u_channel = video[..., 1]  # Shape: [B, H, W]
    v_channel = video[..., 2]  # Shape: [B, H, W]

    # Add channel dimension for chroma channels for pooling operations
    u_channel = u_channel.unsqueeze(1)  # [B, 1, H, W]
    v_channel = v_channel.unsqueeze(1)  # [B, 1, H, W]

    # Perform downsampling and upsampling on chroma channels
    for channel in (u_channel, v_channel):
        # Downsample: [B, 1, H, W] -> [B, 1, H/2, W/2]
        # channel_downsampled = F.avg_pool2d(channel, kernel_size=2, stride=2)
        
        # Use Gaussian filter + downsampling instead of simple average pooling
        channel_downsampled = gaussian_downsample(channel, sigma=1.0, kernel_size=5)
        
        # Upsample: [B, 1, H/2, W/2] -> [B, 1, H, W]
        channel.set_(F.interpolate(channel_downsampled, size=channel.shape[2:], mode='nearest'))

    # Remove channel dimension
    u_final = u_channel.squeeze(1)  # [B, H, W]
    v_final = v_channel.squeeze(1)  # [B, H, W]

    # Recombine channels
    output_video = torch.stack([y_channel, u_final, v_final], dim=-1)  # [B, H, W, 3]

    # Restore original shape if input was 5D
    if len(original_shape) == 5:
        T, H, W, N, _ = original_shape
        # [T*N, H, W, 3] -> [T, N, H, W, 3] -> [T, H, W, N, 3]
        output_video = output_video.reshape(T, N, H, W, 3).permute(0, 2, 3, 1, 4)

    return output_video
