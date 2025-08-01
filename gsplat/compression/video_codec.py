# video_codec.py

import logging
import os
import shlex
import subprocess
from pathlib import Path
from typing import Any, Dict, List, Literal

# Paths for different video codec executables and their configurations.
VIDEO_CODEC_PATHS = {
    "vvenc": {
        "encoder": "helper/vvenc/bin/release-static/vvencapp",
        "decoder": "helper/vvdec/bin/release-static/vvdecapp",
    },
    "vtm": {
        "encoder": "helper/VVCSoftware_VTM-master/bin/EncoderAppStatic",
        "decoder": "helper/VVCSoftware_VTM-master/bin/DecoderAppStatic",
        "intra_config_path": "helper/VVCSoftware_VTM-master/cfg/encoder_intra_vtm.cfg",
        "gopsize16_config_path": "helper/VVCSoftware_VTM-master/cfg/encoder_randomaccess_vtm_gop16.cfg",
        "lossless_444_config_path": "helper/VVCSoftware_VTM-master/cfg/lossless/lossless444.cfg",
        "lossless_config_path": "helper/VVCSoftware_VTM-master/cfg/lossless/lossless.cfg",
    },
    "hm": {
        "encoder": "helper/HM-18.0/bin/TAppEncoderStatic",
        "decoder": "helper/HM-18.0/bin/TAppDecoderStatic",
        "intra_lossy_config_path": "helper/hm_cfg/intra_yuv444p.cfg",
        "intra_lossless_config_path": "helper/hm_cfg/intra_lossless_yuv444p.cfg",
        "inter_lossy_config_path": "helper/hm_cfg/inter_yuv444p.cfg",
        "inter_lossless_config_path": "helper/hm_cfg/inter_lossless_yuv444p.cfg",
    },
    "ffmpeg": {
        "encoder": "ffmpeg",
        "decoder": "ffmpeg",
    },
}

# Standard logging configuration.
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s'
)


class VideoCodec:
    """
    A generic command-line wrapper for various video codecs.

    This class provides a unified interface for different codecs (e.g., vvenc,
    VTM, HM, ffmpeg) by automatically generating the correct command-line
    arguments based on the input configuration dictionary.
    """

    def __init__(self,
                 video_codec_type: Literal['vvenc', 'vtm', 'hm', 'ffmpeg'],
                 encoder_path: str = None,
                 decoder_path: str = None):
        """
        Initializes the video codec wrapper.

        Args:
            video_codec_type: The type of video codec to use.
            encoder_path (optional): Path to the encoder executable.
            decoder_path (optional): Path to the decoder executable.
        """
        self.video_codec_type = video_codec_type.lower()
        if self.video_codec_type not in VIDEO_CODEC_PATHS:
            raise ValueError(f"Unsupported video codec type: {self.video_codec_type}. "
                             f"Supported types: {', '.join(VIDEO_CODEC_PATHS.keys())}")

        codec_paths = VIDEO_CODEC_PATHS[self.video_codec_type]
        self.encoder_path = Path(encoder_path or codec_paths['encoder'])
        self.decoder_path = Path(decoder_path or codec_paths['decoder'])
        self.intra_lossy_config_path = codec_paths.get('intra_lossy_config_path')
        self.inter_lossy_config_path = codec_paths.get('inter_lossy_config_path')
        self.intra_lossless_config_path = codec_paths.get('intra_lossless_config_path')
        self.inter_lossless_config_path = codec_paths.get('inter_lossless_config_path')

        self._check_executables()

    def _check_executables(self):
        """Ensures encoder and decoder executables exist and are accessible."""
        if self.video_codec_type == 'ffmpeg':
            # FFmpeg is assumed to be in the system PATH.
            return

        if not self.encoder_path.is_file():
            raise FileNotFoundError(f"Encoder not found: {self.encoder_path}")
        if not self.decoder_path.is_file():
            raise FileNotFoundError(f"Decoder not found: {self.decoder_path}")

        # This check is more useful on Linux/macOS.
        # On Windows, os.access(..., os.X_OK) may return True if the file exists.
        if not os.access(self.encoder_path, os.X_OK):
            logging.warning(f"Encoder may not be executable: {self.encoder_path}")
        if not os.access(self.decoder_path, os.X_OK):
            logging.warning(f"Decoder may not be executable: {self.decoder_path}")

    def _run_command(self, cmd: List[str], process_name: str):
        """
        Executes a subprocess command and handles logging and errors.

        Args:
            cmd: The command to execute as a list of strings.
            process_name: A descriptive name for the process (e.g., "Encoding").
        """
        logging.info(f"{process_name} with {self.video_codec_type.upper()}...")
        logging.debug(f"Executing command: {shlex.join(cmd)}")

        try:
            result = subprocess.run(cmd, check=True, capture_output=True, text=True, encoding='utf-8')
            logging.debug(f"Process stdout: {result.stdout}")
            if result.stderr:
                logging.warning(f"Process stderr: {result.stderr}")
            logging.info(f"{process_name} completed successfully.")
            return result
        except subprocess.CalledProcessError as e:
            logging.error(f"{process_name} failed.")
            logging.error(f"Return code: {e.returncode}")
            logging.error(f"Command: {shlex.join(e.cmd)}")
            logging.error(f"Stderr: {e.stderr}")
            logging.error(f"Stdout: {e.stdout}")
            raise

    # --- Command Builders: Encoder ---

    def _build_encode_cmd(self, input_path: Path, output_path: Path, params: Dict[str, Any]) -> List[str]:
        """Dispatches to the appropriate command builder based on codec type."""
        builders = {
            "vvenc": self._build_vvenc_encode_cmd,
            "vtm": self._build_vtm_encode_cmd,
            "hm": self._build_hm_encode_cmd,
            "ffmpeg": self._build_ffmpeg_encode_cmd,
        }
        return builders[self.video_codec_type](input_path, output_path, params)

    def _build_vvenc_encode_cmd(self, i_path: Path, o_path: Path, p: Dict[str, Any]) -> List[str]:
        """Builds the encoding command for vvenc (vvencapp)."""
        for key in ['width', 'height', 'qp', 'pix_fmt']:
            if key not in p:
                raise ValueError(f"Missing required parameter for vvenc: '{key}'")

        pix_fmt_map = {'yuv420p': '420', 'yuv422p': '422', 'yuv444p': '444', 'yuv400p': '400'}
        chroma_format = pix_fmt_map.get(p['pix_fmt'])
        if not chroma_format:
            raise ValueError(f"Unsupported pix_fmt for vvenc: {p['pix_fmt']}")

        cmd = [
            str(self.encoder_path),
            '-i', str(i_path),
            '-b', str(o_path),
            '--Size', f"{p['width']}x{p['height']}",
            '--qp', str(p['qp']),
            '--InputChromaFormat', chroma_format,
            '--preset', p.get('preset', 'medium'),
        ]
        if p.get('all_intra', False):
            cmd.extend(['--IntraPeriod', '1'])
        if 'extra_params' in p:
            cmd.extend(p['extra_params'])
        return cmd

    def _build_vtm_encode_cmd(self, i_path: Path, o_path: Path, p: Dict[str, Any]) -> List[str]:
        """Builds the encoding command for VTM (EncoderApp)."""
        for key in ['width', 'height', 'qp', 'pix_fmt', 'frame_num']:
            if key not in p:
                raise ValueError(f"Missing required parameter for VTM: '{key}'")

        pix_fmt_map = {'yuv420p': '420', 'yuv422p': '422', 'yuv444p': '444', 'yuv400p': '400'}
        chroma_format = pix_fmt_map.get(p['pix_fmt'])
        if not chroma_format:
            raise ValueError(f"Unsupported pix_fmt for VTM: {p['pix_fmt']}")

        config_path = self.intra_lossy_config_path if p.get('use_all_intra') else self.inter_lossy_config_path

        cmd = [
            str(self.encoder_path),
            '-c', config_path,
            '-i', str(i_path),
            '-b', str(o_path),
            '-wdt', str(p['width']),
            '-hgt', str(p['height']),
            '-q', str(p['qp']),
            '-cf', chroma_format,
            '-fr', '30',
            '-f', str(p['frame_num']),
        ]
        if p.get('all_intra', False):
            cmd.extend(['-ip', '1'])
        if 'extra_params' in p:
            cmd.extend(p['extra_params'])
        return cmd

    def _build_hm_encode_cmd(self, i_path: Path, o_path: Path, p: Dict[str, Any]) -> List[str]:
        """Builds the encoding command for HM (TAppEncoder)."""
        for key in ['width', 'height', 'qp', 'pix_fmt', 'frame_num']:
            if key not in p:
                raise ValueError(f"Missing required parameter for HM: '{key}'")

        pix_fmt_map = {'yuv420p': '420', 'yuv422p': '422', 'yuv444p': '444', 'yuv400p': '400'}
        chroma_format = pix_fmt_map.get(p['pix_fmt'])
        if not chroma_format:
            raise ValueError(f"Unsupported pix_fmt for HM: {p['pix_fmt']}")

        # Use lossless config if qp < 0
        if p['qp'] < 0:
            config_path = self.intra_lossless_config_path if p.get('use_all_intra') else self.inter_lossless_config_path
            qp_value = 4
        else:
            config_path = self.intra_lossy_config_path if p.get('use_all_intra') else self.inter_lossy_config_path
            qp_value = p['qp']

        cmd = [
            str(self.encoder_path),
            '-c', config_path,
            '-i', str(i_path),
            '-b', str(o_path),
            '-wdt', str(p['width']),
            '-hgt', str(p['height']),
            '-q', str(qp_value),
            '-cf', chroma_format, # output chroma format
            '-fr', '30',
            '-f', str(p['frame_num']),
            f'--InputChromaFormat={chroma_format}', # input chroma format
        ]
        if p.get('all_intra', False):
            cmd.extend(['--IntraPeriod', '1'])
        if 'extra_params' in p:
            cmd.extend(p['extra_params'])
        return cmd

    def _build_ffmpeg_encode_cmd(self, i_path: Path, o_path: Path, p: Dict[str, Any]) -> List[str]:
        """Builds the encoding command for FFmpeg (e.g., using libx265)."""
        for key in ['width', 'height', 'qp', 'pix_fmt']:
            if key not in p:
                raise ValueError(f"Missing required parameter for ffmpeg: '{key}'")

        pix_fmt_map = {'yuv420p': 'yuv420p', 'yuv422p': 'yuv422p', 'yuv444p': 'yuv444p', 'yuv400p': 'gray'}
        chroma_format = pix_fmt_map.get(p['pix_fmt'])
        if not chroma_format:
            raise ValueError(f"Unsupported pix_fmt for ffmpeg: {p['pix_fmt']}")
        # Use qp or lossless mode
        qp_params = f"qp={p['qp']}" if p['qp'] > 0 else 'lossless=1'
        cmd = [
            str(self.encoder_path),
            '-y',  # Overwrite output file if it exists
            '-f', 'rawvideo',
            '-pix_fmt', chroma_format,
            '-s:v', f"{p['width']}x{p['height']}",
            '-r', '30',  # Frame rate
            '-i', str(i_path),
            '-c:v', 'libx265',
            '-x265-params', qp_params,
        ]
        if p.get('use_all_intra', False):
            cmd.extend(['-g', '1'])  # GOP size of 1 means all-intra
        
        cmd.append(str(o_path))
        return cmd

    # --- Command Builders: Decoder ---

    def _build_decode_cmd(self, input_path: Path, output_path: Path, params: Dict[str, Any]) -> List[str]:
        """Dispatches to the appropriate decoder command builder."""
        if self.video_codec_type == 'ffmpeg':
            return self._build_ffmpeg_decode_cmd(input_path, output_path, params)
        else:
            cmd = [
                str(self.decoder_path),
                '-b', str(input_path),
                '-o', str(output_path),
            ]
            if params and 'extra_params' in params:
                cmd.extend(params['extra_params'])
            return cmd

    def _build_ffmpeg_decode_cmd(self, i_path: Path, o_path: Path, p: Dict[str, Any]) -> List[str]:
        """Builds the decoding command for FFmpeg."""
        pix_fmt_map = {'yuv420p': 'yuv420p', 'yuv422p': 'yuv422p', 'yuv444p': 'yuv444p', 'yuv400p': 'gray'}
        # Default to a common format if not specified
        chroma_format = pix_fmt_map.get(p.get('pix_fmt', 'yuv420p'))

        cmd = [
            str(self.decoder_path),
            '-y',
            '-i', str(i_path),
            '-pix_fmt', chroma_format,
            '-f', 'rawvideo',
            str(o_path),
        ]
        return cmd

    # --- Public API ---

    def encode(self,
               input_yuv: Path,
               output_bitstream: Path,
               config_params: Dict[str, Any]):
        """
        Encodes a YUV file to a bitstream using the specified configuration.

        Args:
            input_yuv: Path to the input YUV file.
            output_bitstream: Path for the output bitstream file.
            config_params: A dictionary of encoding parameters. Common keys:
                'width' (int), 'height' (int), 'qp' (int),
                'pix_fmt' (str: 'yuv420p', etc.), 'frame_num' (int),
                'all_intra' (bool), 'extra_params' (List[str], optional).
        """
        cmd = self._build_encode_cmd(input_yuv, output_bitstream, config_params)
        self._run_command(cmd, f"Encoding {input_yuv.name}")

    def decode(self,
               input_bitstream: Path,
               output_yuv: Path,
               config_params: Dict[str, Any] = None):
        """
        Decodes a bitstream file to a YUV file.

        Args:
            input_bitstream: Path to the input bitstream file.
            output_yuv: Path for the output YUV file.
            config_params (optional): A dictionary of decoding parameters.
                Mainly used by ffmpeg to specify output format.
        """
        if config_params is None:
            config_params = {}
        cmd = self._build_decode_cmd(input_bitstream, output_yuv, config_params)
        self._run_command(cmd, f"Decoding {input_bitstream.name}")