# video_codec.py

import subprocess
import logging
from pathlib import Path
from typing import List, Literal, Any, Dict

# 使用大写字母表示常量，这是一种常见的Python风格
VIDEO_CODEC_PATHS = {
    "vvenc": {
        "encoder": "helper/vvenc/bin/release-static/vvencapp",
        "decoder": "helper/vvdec/bin/release-static/vvdecapp",
    },
    "vtm": {
        "encoder": "helper/VVCSoftware_VTM-master/bin/EncoderAppStatic",
        "decoder": "helper/VVCSoftware_VTM-master/bin/DecoderAppStatic",
        "intra_config_path": "helper/VVCSoftware_VTM-master/cfg/encoder_intra_vtm_bit8.cfg", 
        "gopsize16_config_path": "helper/VVCSoftware_VTM-master/cfg/encoder_randomaccess_vtm_bit8_gop16.cfg",
        "lossless_444_config_path": "helper/VVCSoftware_VTM-master/cfg/lossless/lossless444.cfg",
        "lossless_config_path": "helper/VVCSoftware_VTM-master/cfg/lossless/lossless.cfg",
    },
    "hm": {
        "encoder": "helper/HM-master/bin/TAppEncoderStatic",
        "decoder": "helper/HM-master/bin/TAppDecoderStatic",
        "intra_config_path": "helper/HM-master/cfg/encoder_intra_main_rext_bit8.cfg",  
        "gopsize16_config_path": "helper/HM-master/cfg/encoder_randomaccess_main_rext_bit8.cfg",
        "lossless_config_path": "helper/HM-master/cfg/lossless/lossless_bit8.cfg",
    },
    "ffmpeg": {
        "encoder": "ffmpeg",
        "decoder": "ffmpeg",
    },
}

# 日志配置保持不变，这是一个很好的实践
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s'
)

class VideoCodec:
    """
    一个通用的命令行视频编解码器包装器。
    该类通过统一的接口处理不同的编解码器（如vvenc, VTM, HM, ffmpeg），
    并根据输入的配置字典自动生成正确的命令行参数。
    """

    def __init__(self,
                 video_codec_type: Literal['vvenc', 'vtm', 'hm', 'ffmpeg'],
                 encoder_path: str = None,
                 decoder_path: str = None):
        """
        初始化视频编解码器包装器。

        Args:
            video_codec_type: 要使用的视频编解码器类型。
            encoder_path: (可选) 编码器可执行文件的路径。
            decoder_path: (可选) 解码器可执行文件的路径。
        """
        self.video_codec_type = video_codec_type.lower()
        if self.video_codec_type not in VIDEO_CODEC_PATHS:
            raise ValueError(f"不支持的视频编解码器类型: {self.video_codec_type}. "
                             f"支持的类型: {', '.join(VIDEO_CODEC_PATHS.keys())}")

        # 使用 Path 对象进行路径操作，更加健壮
        self.encoder_path = Path(encoder_path or VIDEO_CODEC_PATHS[self.video_codec_type]['encoder'])
        self.decoder_path = Path(decoder_path or VIDEO_CODEC_PATHS[self.video_codec_type]['decoder'])
        self.intra_config_path = VIDEO_CODEC_PATHS[self.video_codec_type].get('intra_config_path', None)
        self.gopsize16_config_path = VIDEO_CODEC_PATHS[self.video_codec_type].get('gopsize16_config_path', None)
        self.lossless_444_config_path = VIDEO_CODEC_PATHS[self.video_codec_type].get('lossless_444_config_path', None)
        self.lossless_config_path = VIDEO_CODEC_PATHS[self.video_codec_type].get('lossless_config_path', None)
        self._check_executables()

    def _check_executables(self):
        """确保编码器和解码器可执行文件存在且可执行。"""
        if self.video_codec_type == 'ffmpeg':
            # FFmpeg 通常在系统路径中，直接使用 'ffmpeg' 命令即可
            return
        if not self.encoder_path.is_file():
            raise FileNotFoundError(f"编码器未找到: {self.encoder_path}")
        if not self.decoder_path.is_file():
            raise FileNotFoundError(f"解码器未找到: {self.decoder_path}")

        import os
        if not os.access(self.encoder_path, os.X_OK):
            # 在Windows上，只要文件存在，os.access(..., os.X_OK)就可能返回True。
            # is_file()检查已经足够，但这个检查在Linux/macOS上更有用。
            logging.warning(f"无法确认编码器的可执行权限: {self.encoder_path}")
        if not os.access(self.decoder_path, os.X_OK):
            logging.warning(f"无法确认解码器的可执行权限: {self.decoder_path}")

    def _run_command(self, cmd: List[str], process_name: str):
        """
        执行一个子进程命令，并处理日志和错误。

        Args:
            cmd (List[str]): 要执行的命令列表。
            process_name (str): 进程的描述名称（如 "Encoding", "Decoding"）。
        """
        logging.info(f"{process_name} with {self.video_codec_type.upper()}...")
        # 使用 shlex.join 来安全地将命令列表转换为可读的字符串，适合日志记录
        try:
            import shlex
            logging.debug(f"Executing command: {shlex.join(cmd)}")
        except ImportError:
            logging.debug(f"Executing command: {' '.join(cmd)}")

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
            logging.error(f"Command: {' '.join(e.cmd)}")
            logging.error(f"Stderr: {e.stderr}")
            logging.error(f"Stdout: {e.stdout}")
            raise

    # --- Command Builders: Encoder ---

    def _build_encode_cmd(self, input_path: Path, output_path: Path, params: Dict[str, Any]) -> List[str]:
        """根据编解码器类型分派到相应的命令构建函数。"""
        builders = {
            "vvenc": self._build_vvenc_encode_cmd,
            "vtm": self._build_vtm_encode_cmd,
            "hm": self._build_hm_encode_cmd,
            "ffmpeg": self._build_ffmpeg_encode_cmd,
        }
        return builders[self.video_codec_type](input_path, output_path, params)

    def _build_vvenc_encode_cmd(self, i_path: Path, o_path: Path, p: Dict[str, Any]) -> List[str]:
        """构建 vvenc (vvencapp) 的编码命令。"""
        # 强制要求width, height, qp, pix_fmt
        for key in ['width', 'height', 'qp', 'pix_fmt']:
            if key not in p:
                raise ValueError(f"vvenc 编码需要参数: '{key}'")
        
        # vvenc 使用 --InputChromaFormat 400/420/422/444
        pix_fmt_map = {'yuv420p': '420', 'yuv422p': '422', 'yuv444p': '444', 'yuv400p': '400'}
        chroma_format = pix_fmt_map.get(p['pix_fmt'])
        if not chroma_format:
            raise ValueError(f"不支持的 pix_fmt for vvenc: {p['pix_fmt']}")

        cmd = [
            str(self.encoder_path),
            '-i', str(i_path),
            '-b', str(o_path),
            '--Size', f"{p['width']}x{p['height']}",
            '--qp', str(p['qp']),
            '--InputChromaFormat', chroma_format,
            '--preset', p.get('preset', 'medium'), # 添加一个默认预设
        ]
        if p.get('all_intra', False):
            cmd.extend(['--IntraPeriod', '1'])
        
        # 允许传递其他原始参数
        if 'extra_params' in p:
            cmd.extend(p['extra_params'])
            
        return cmd

    def _build_vtm_encode_cmd(self, i_path: Path, o_path: Path, p: Dict[str, Any]) -> List[str]:
        """构建 VTM (EncoderApp) 的编码命令。"""
        for key in ['width', 'height', 'qp', 'pix_fmt']:
            if key not in p:
                raise ValueError(f"VTM 编码需要参数: '{key}'")

        pix_fmt_map = {'yuv420p': '420', 'yuv422p': '422', 'yuv444p': '444', 'yuv400p': '400'}
        chroma_format = pix_fmt_map.get(p['pix_fmt'])
        if not chroma_format:
            raise ValueError(f"不支持的 pix_fmt for VTM: {p['pix_fmt']}")

        if p.get('use_all_intra', False):
            config_path = self.intra_config_path 
        else:
            config_path = self.gopsize16_config_path
  
        cmd = [
            str(self.encoder_path),
            '-c', config_path,
            '-i', str(i_path),
            '-b', str(o_path),
            '-wdt', str(p['width']),
            '-hgt', str(p['height']),
            '-q', str(p['qp']),
            '-cf', chroma_format,
            '-fr', '30',  # 默认帧率为30
            '-f', '1',  # 输入格式为YUV
        ]
        if p.get('all_intra', False):
            cmd.extend(['-ip', '1'])
            
        if 'extra_params' in p:
            cmd.extend(p['extra_params'])

        return cmd

    def _build_hm_encode_cmd(self, i_path: Path, o_path: Path, p: Dict[str, Any]) -> List[str]:
        """构建 HM (TAppEncoder) 的编码命令。"""
        for key in ['width', 'height', 'qp', 'pix_fmt']:
            if key not in p:
                raise ValueError(f"HM 编码需要参数: '{key}'")
        
        pix_fmt_map = {'yuv420p': '420', 'yuv422p': '422', 'yuv444p': '444', 'yuv400p': '400'}
        chroma_format = pix_fmt_map.get(p['pix_fmt'])
        if not chroma_format:
            raise ValueError(f"不支持的 pix_fmt for HM: {p['pix_fmt']}")
        if p.get('use_all_intra', False):
            config_path = self.intra_config_path 
        else:
            config_path = self.gopsize16_config_path

        cmd = [
            str(self.encoder_path),
            '-c', config_path,
            '-i', str(i_path),
            '-b', str(o_path),
            '-wdt', str(p['width']),
            '-hgt', str(p['height']),
            '-q', str(p['qp']),
            '-cf', chroma_format, # InputChromaFormat
            '-fr', '30',  # 默认帧率为30
            '-f', '1',  # 输入格式为YUV
        ]
        if p.get('all_intra', False):
            cmd.extend(['--IntraPeriod', '1']) # HM 也使用 --IntraPeriod
        
        if 'extra_params' in p:
            cmd.extend(p['extra_params'])

        return cmd

    def _build_ffmpeg_encode_cmd(self, i_path: Path, o_path: Path, p: Dict[str, Any]) -> List[str]:
        """构建 FFmpeg 的编码命令 (以 libx265 为例)。"""
        for key in ['width', 'height', 'qp', 'pix_fmt', 'bit_depth']:
            if key not in p:
                raise ValueError(f"ffmpeg 编码需要参数: '{key}'")
        
        pix_fmt_map = {'yuv420p': 'yuv420p', 'yuv422p': 'yuv422p', 'yuv444p': 'yuv444p', 'yuv400p': 'gray'}
        chroma_format = pix_fmt_map.get(p['pix_fmt'])
        if not chroma_format:
            raise ValueError(f"不支持的 pix_fmt for ffmpeg: {p['pix_fmt']}")
        
        cmd = [
            str(self.encoder_path),
            '-y', # 自动覆盖输出文件
            '-f', 'rawvideo',
            '-pix_fmt', chroma_format,
            '-s', f"{p['width']}x{p['height']}",
            '-i', str(i_path),
            '-c:v', 'libx265', # 使用 libx265 编码器
            '-x265-params', f"\"qp={p['qp']}\"",
            #'-x265-params', f"\"qp={p['qp']}:output-depth={p['bit_depth']}\"",
        ]

        if p.get('all_intra', False):
            cmd.extend(['-g', '1']) # -g 1 (GOP size of 1) means all-intra
            
        cmd.append(str(o_path))
        return cmd

    # --- Command Builders: Decoder ---

    def _build_decode_cmd(self, input_path: Path, output_path: Path, params: Dict[str, Any]) -> List[str]:
        """根据编解码器类型分派到相应的解码命令构建函数。"""
        # 对于标准解码器，通常只需要输入和输出。FFmpeg可能需要额外参数。
        if self.video_codec_type == 'ffmpeg':
             return self._build_ffmpeg_decode_cmd(input_path, output_path, params)
        else:
            cmd = [
                str(self.decoder_path),
                '-b', str(input_path),
                '-o', str(output_path),
            ]
            # 允许传递其他原始参数，尽管解码器通常不需要
            if params and 'extra_params' in params:
                cmd.extend(params['extra_params'])
            return cmd

    def _build_ffmpeg_decode_cmd(self, i_path: Path, o_path: Path, p: Dict[str, Any]) -> List[str]:
        """构建 FFmpeg 的解码命令。"""
        pix_fmt_map = {'yuv420p': 'yuv420p', 'yuv422p': 'yuv422p', 'yuv444p': 'yuv444p', 'yuv400p': 'gray'}
        chroma_format = pix_fmt_map.get(p['pix_fmt'])
        if not chroma_format:
            raise ValueError(f"不支持的 pix_fmt for ffmpeg: {p['pix_fmt']}")
        cmd = [
            str(self.decoder_path),
            '-y',
            '-i', str(i_path),      # 输入码流
            '-f', 'rawvideo',       # 输出为 YUV
            '-pix_fmt', chroma_format, # 指定输出像素格式
            str(o_path),
        ]
        return cmd

    # --- Public API ---

    def encode(self,
               input_yuv: Path,
               output_bitstream: Path,
               config_params: Dict[str, Any]):
        """
        使用指定的配置将 YUV 文件编码为码流。

        Args:
            input_yuv: 输入的 YUV 文件路径。
            output_bitstream: 输出的码流文件路径。
            config_params: 包含编码参数的字典。通用键包括:
                'width' (int), 'height' (int), 'qp' (int), 
                'pix_fmt' (str: 'yuv420p', 'yuv400p', ...),
                'all_intra' (bool),
                'config_path' (Path): (VTM/HM 必需) 编码器配置文件路径。
                'extra_params' (List[str]): (可选) 附加的原始命令行参数。
        """
        cmd = self._build_encode_cmd(input_yuv, output_bitstream, config_params)
        self._run_command(cmd, f"Encoding {input_yuv.name}")

    def decode(self,
               input_bitstream: Path,
               output_yuv: Path,
               config_params: Dict[str, Any] = None):
        """
        将码流文件解码为 YUV 文件。

        Args:
            input_bitstream: 输入的码流文件路径。
            output_yuv: 输出的 YUV 文件路径。
            config_params: (可选) 解码参数字典，主要用于ffmpeg指定输出格式。
        """
        if config_params is None:
            config_params = {}
        cmd = self._build_decode_cmd(input_bitstream, output_yuv, config_params)
        self._run_command(cmd, f"Decoding {input_bitstream.name}")