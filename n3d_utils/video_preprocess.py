import os
import subprocess
from pathlib import Path
import argparse

import cv2
import tqdm


def convert_mp4_to_png_sequence_via_ffmpeg(input_mp4, output_path):
    """
    Convert MP4 video to PNG sequence using ffmpeg
    
    Args:
        input_mp4 (str): Path to input MP4 file
        output_path (str): Directory path to save PNG sequence
    """
    os.makedirs(output_path, exist_ok=True)
    
    # Extract base name from MP4 file (remove extension)
    base_name = os.path.splitext(os.path.basename(input_mp4))[0]
    
    # Create output pattern for PNG sequence
    output_pattern = os.path.join(output_path, f'{base_name}_frame%03d.png')
    
    # FFmpeg command to convert MP4 to PNG sequence
    cmd = [
        'ffmpeg', '-hide_banner', '-loglevel', 'error',
        '-i', input_mp4,
        '-vf', 'scale=in_range=pc:in_color_matrix=bt709:out_range=pc',
        '-pix_fmt', 'rgb24',
        '-color_range', 'pc',
        '-sws_flags', 'lanczos+bitexact+full_chroma_int+full_chroma_inp',
        '-compression_level', '0',
        '-pred', 'none',
        output_pattern
    ]
    
    try:
        subprocess.run(cmd, check=True)
        print(f"Successfully converted {input_mp4} to PNG sequence in {output_path}")
    except subprocess.CalledProcessError as e:
        print(f"Error during conversion: {e}")


def convert_mp4_to_png_sequence_via_ffmpeg_with_resolution(input_mp4, output_path, resolution=None):
    """
    Convert MP4 video to PNG sequence using ffmpeg with optional resolution specification
    
    Args:
        input_mp4 (str): Path to input MP4 file
        output_path (str): Directory path to save PNG sequence
        resolution (str, optional): Output resolution (e.g., '1920x1080'). If None, keeps original resolution
    """
    os.makedirs(output_path, exist_ok=True)
    
    # Extract base name from MP4 file (remove extension)
    base_name = os.path.splitext(os.path.basename(input_mp4))[0]
    
    # Create output pattern for PNG sequence
    output_pattern = os.path.join(output_path, f'{base_name}_frame%03d.png')
    
    # Build FFmpeg command
    cmd = ['ffmpeg', '-hide_banner', '-loglevel', 'error', '-i', input_mp4]
    
    # Add resolution filter if specified
    if resolution:
        cmd.extend(['-vf', f'scale={resolution}:in_range=pc:in_color_matrix=bt709:out_range=pc'])
    else:
        cmd.extend(['-vf', 'scale=in_range=pc:in_color_matrix=bt709:out_range=pc'])
    
    # Add output options
    cmd.extend([
        '-pix_fmt', 'rgb24',
        '-color_range', 'pc',
        '-sws_flags', 'lanczos+bitexact+full_chroma_int+full_chroma_inp',
        '-compression_level', '0',
        '-pred', 'none',
        output_pattern
    ])
    
    try:
        subprocess.run(cmd, check=True)
        print(f"Successfully converted {input_mp4} to PNG sequence in {output_path}")
        if resolution:
            print(f"Output resolution: {resolution}")
    except subprocess.CalledProcessError as e:
        print(f"Error during conversion: {e}")

def extractframes(videopath: Path, startframe=0, endframe=300, downscale=1, save_subdir = '', ext='png'):
    output_dir = videopath.parent / save_subdir / videopath.stem
        
    if all((output_dir / f"{i}.{ext}").exists() for i in range(startframe, endframe)):
        print(f"Already extracted all the frames in {output_dir}")
        return

    cam = cv2.VideoCapture(str(videopath))
    cam.set(cv2.CAP_PROP_POS_FRAMES, startframe)

    output_dir.mkdir(parents=True, exist_ok=True)

    for i in range(startframe, endframe):
        success, frame = cam.read()
        if not success:
            print(f"Error reading frame {i}")
            break

        if downscale > 1:
            new_width, new_height = int(frame.shape[1] / downscale), int(frame.shape[0] / downscale)
            frame = cv2.resize(frame, (new_width, new_height), interpolation=cv2.INTER_AREA)

        cv2.imwrite(str(output_dir / f"{i}.{ext}"), frame)

    cam.release()


if __name__ == "__main__":
    # Example usage
    parser = argparse.ArgumentParser()
 
    # Required argument
    parser.add_argument("--videopath", required=True, type=str, 
                       help="Path to directory containing MP4 videos")
    
    # Optional arguments
    parser.add_argument("--startframe", default=0, type=int,
                       help="Starting frame number (default: 0)")
    parser.add_argument("--endframe", default=300, type=int,
                       help="Ending frame number (default: 300)")
    parser.add_argument("--downscale", default=1, type=int,
                       help="Downscale factor for frames (default: 1, no downscaling)")

    args = parser.parse_args()
    videopath = Path(args.videopath)
    startframe = args.startframe
    endframe = args.endframe
    downscale = args.downscale

    print(f"Parameters: startframe={startframe} - endframe={endframe} - downscale={downscale} - videopath={videopath}")
    
    # Parameter validation
    if startframe >= endframe:
        print("start frame must smaller than end frame")
        quit()
    if startframe < 0 or endframe > 300:
        print("frame must in range 0-300")
        quit()
    if not videopath.exists():
        print("path not exist")
        quit()
    ##### step1
    print("start extracting frames from videos")
    videoslist = sorted(videopath.glob("*.mp4"))
    for v in tqdm.tqdm(videoslist, desc="Extract frames from videos"):
        
        ### Use opencv to extract frames, taken from spacetime gaussian
        ### But it introduces minor color shift to images, when compared to mp4
        extractframes(v, downscale=downscale)
    
        ### Use ffmpeg to convert mp4 to png sequence
        # Basic conversion
        # convert_mp4_to_png_sequence_via_ffmpeg(input_file, output_dir)
        
        # Conversion with specific resolution
        # convert_mp4_to_png_sequence_via_ffmpeg_with_resolution(input_file, output_dir, "1920x1080") 