/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2022, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstdlib>

#include "TLibCommon/TComPicYuv.h"
#include "TLibVideoIO/TVideoIOYuv.h"
#include "TAppCommon/program_options_lite.h"

using namespace std;
namespace po = df::program_options_lite;

Int main(Int argc, const char** argv)
{
  Bool do_help;
  string filename_in, filename_out;
  UInt width, height;
  UInt bitdepth_in, bitdepth_out, chromaFormatRaw;
  UInt num_frames;
  UInt num_frames_skip;

  po::Options opts;
  opts.addOptions()
  ("help", do_help, false, "this help text")
  ("InputFile,i", filename_in, string(""), "input file to convert")
  ("OutputFile,o", filename_out, string(""), "output file")
  ("SourceWidth", width, 0u, "source picture width")
  ("SourceHeight", height, 0u, "source picture height")
  ("InputBitDepth", bitdepth_in, 8u, "bit-depth of input file")
  ("OutputBitDepth", bitdepth_out, 8u, "bit-depth of output file")
  ("ChromaFormat", chromaFormatRaw, 420u, "chroma format. 400, 420, 422 or 444 only")
  ("NumFrames", num_frames, 0xffffffffu, "number of frames to process")
  ("FrameSkip,-fs", num_frames_skip, 0u, "Number of frames to skip at start of input YUV")
  ;

  po::setDefaults(opts);
  po::scanArgv(opts, argc, argv);


  if (argc == 1 || do_help)
  {
    /* argc == 1: no options have been specified */
    po::doHelp(cout, opts);
    return EXIT_FAILURE;
  }

  ChromaFormat chromaFormatIDC=CHROMA_420;
  switch (chromaFormatRaw)
  {
    case 400: chromaFormatIDC=CHROMA_400; break;
    case 420: chromaFormatIDC=CHROMA_420; break;
    case 422: chromaFormatIDC=CHROMA_422; break;
    case 444: chromaFormatIDC=CHROMA_444; break;
    default:
      fprintf(stderr, "Bad chroma format string\n");
      return EXIT_FAILURE;
  }

  TVideoIOYuv input;
  TVideoIOYuv output;

  Int inputBitDepths [MAX_NUM_CHANNEL_TYPE];
  Int outputBitDepths[MAX_NUM_CHANNEL_TYPE];

  for (UInt channelTypeIndex = 0; channelTypeIndex < MAX_NUM_CHANNEL_TYPE; channelTypeIndex++)
  {
    inputBitDepths [channelTypeIndex] = bitdepth_in;
    outputBitDepths[channelTypeIndex] = bitdepth_out;
  }

  input.open((char*)filename_in.c_str(), false, inputBitDepths, inputBitDepths, outputBitDepths);
  output.open((char*)filename_out.c_str(), true, outputBitDepths, outputBitDepths, outputBitDepths);

  input.skipFrames(num_frames_skip, width, height, chromaFormatIDC);

  TComPicYuv frame;
  frame.createWithoutCUInfo( width, height, chromaFormatIDC);

  Int pad[2] = {0, 0};

  TComPicYuv cPicYuvTrueOrg;
  cPicYuvTrueOrg.createWithoutCUInfo( width, height, chromaFormatIDC );

  UInt num_frames_processed = 0;
  while (!input.isEof())
  {
    if (! input.read(&frame, &cPicYuvTrueOrg, IPCOLOURSPACE_UNCHANGED, pad))
    {
      break;
    }
#if 0
    Pel* img = frame.getAddr(COMPONENT_Y);
    for (Int y = 0; y < height; y++)
    {
      for (Int x = 0; x < height; x++)
      {
        img[x] = 0;
      }
      img += frame.getStride();
    }
    img = frame.getAddr(COMPONENT_Y);
    img[0] = 1;
#endif

    output.write(&frame, IPCOLOURSPACE_UNCHANGED);
    num_frames_processed++;
    if (num_frames_processed == num_frames)
    {
      break;
    }
  }

  input.close();
  output.close();
  cPicYuvTrueOrg.destroy();

  return EXIT_SUCCESS;
}
