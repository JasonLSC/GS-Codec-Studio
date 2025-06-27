/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2025, ITU/ISO/IEC
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

/** \file     DecAppCfg.cpp
    \brief    Decoder configuration class
*/

#include <cstdio>
#include <cstring>
#include <string>
#include "DecAppCfg.h"
#include "Utilities/program_options_lite.h"
#include "Utilities/VideoIOYuv.h"
#include "CommonLib/ChromaFormat.h"
#include "CommonLib/dtrace_next.h"

namespace po = ProgramOptionsLite;

//! \ingroup DecoderApp
//! \{

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/** \param argc number of arguments
    \param argv array of arguments
 */
bool DecAppCfg::parseCfg( int argc, char* argv[] )
{
  bool do_help = false;
  std::string cfg_TargetDecLayerIdSetFile;
  std::string outputColourSpaceConvert;
  int warnUnknowParameter = 0;
#if ENABLE_TRACING
  std::string sTracingRule;
  std::string sTracingFile;
  bool   bTracingChannelsList = false;
#endif
#if ENABLE_SIMD_OPT
  std::string ignore;
#endif
  po::Options opts;

  // clang-format off
  opts.addOptions()
  ("help",                      do_help,                               false,      "this help text")
  ("BitstreamFile,b",           m_bitstreamFileName,                   std::string(""), "bitstream input file name")
  ("ReconFile,o",               m_reconFileName,                       std::string(""), "reconstructed YUV output file name\n")
  ("OplFile,-opl",              m_oplFilename,                         std::string(""), "opl-file name without extension for conformance testing\n")

#if ENABLE_SIMD_OPT
  ("SIMD",                      ignore,                                std::string(""), "SIMD extension to use (SCALAR, SSE41, SSE42, AVX, AVX2, AVX512), default: the highest supported extension\n")
#endif
  ("WarnUnknowParameter,w",     warnUnknowParameter,                   0,          "warn for unknown configuration parameters instead of failing")
  ("SkipFrames,s",              m_iSkipFrame,                          0,          "number of frames to skip before random access")
  ("OutputBitDepth,d",          m_outputBitDepth[ChannelType::LUMA],   0,          "bit depth of YUV output luma component (default: use 0 for native depth)")
  ("OutputBitDepthC,d",         m_outputBitDepth[ChannelType::CHROMA], 0,          "bit depth of YUV output chroma component (default: use luma output bit-depth)")
  ("OutputColourSpaceConvert",  outputColourSpaceConvert,              std::string(""), "Colour space conversion to apply to input 444 video. Permitted values are (empty string=UNCHANGED) " + getListOfColourSpaceConverts(false))
  ("MaxTemporalLayer,t",        m_maxTemporalLayer,                    TL_UNDEFINED, "Maximum Temporal Layer to be decoded. -1 to decode all layers")
  ("TargetOutputLayerSet,p",    m_targetOlsIdx,                        500,        "Target output layer set index")
  ("SEIShutterIntervalPostFilename,-sii", m_shutterIntervalPostFileName, std::string(""), "Post Filtering with Shutter Interval SEI. If empty, no filtering is applied (ignore SEI message)\n")
  ("SEIDecodedPictureHash,-dph", m_decodedPictureHashSEIEnabled,       1,          "Control handling of decoded picture hash SEI messages\n"
                                                                                   "\t1: check hash in SEI messages if available in the bitstream\n"
                                                                                   "\t0: ignore SEI message")
  ("SEINoDisplay",              m_decodedNoDisplaySEIEnabled,          true,       "Control handling of decoded no display SEI messages")
  ("TarDecLayerIdSetFile,l",    cfg_TargetDecLayerIdSetFile,           std::string(""), "targetDecLayerIdSet file name. The file should include white space separated LayerId values to be decoded. Omitting the option or a value of -1 in the file decodes all layers.")
  ("SEIColourRemappingInfoFilename", m_colourRemapSEIFileName,         std::string(""), "Colour Remapping YUV output file name. If empty, no remapping is applied (ignore SEI message)\n")
  ("SEICTIFilename",            m_SEICTIFileName,                      std::string(""), "CTI YUV output file name. If empty, no Colour Transform is applied (ignore SEI message)\n")
  ("SEIFGSFilename",            m_SEIFGSFileName,                      std::string(""), "FGS YUV output file name. If empty, no film grain is applied (ignore SEI message)\n")
  ("SEIAnnotatedRegionsInfoFilename", m_annotatedRegionsSEIFileName,   std::string(""), "Annotated regions output file name. If empty, no object information will be saved (ignore SEI message)\n")
  ("SEIObjectMaskInfosFilename", m_objectMaskInfoSEIFileName,          std::string(""), "Object mask information output file name. If empty, no object mask information will be saved (ignore SEI message)\n")
  ("OutputDecodedSEIMessagesFilename", m_outputDecodedSEIMessagesFilename, std::string(""), "When non empty, output decoded SEI messages to the indicated file. If file is '-', then output to stdout\n")
#if JVET_S0257_DUMP_360SEI_MESSAGE
  ("360DumpFile",               m_outputDecoded360SEIMessagesFilename, std::string(""), "When non empty, output decoded 360 SEI messages to the indicated file.\n")
#endif
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  ("SEIPackedRegionsInfoFilename", m_packedRegionsInfoSEIFileName,     std::string(""), "Packed regions info output file name. If empty, no object information will be saved\n")
#endif
  ("ClipOutputVideoToRec709Range",      m_clipOutputVideoToRec709Range,  false,   "If true then clip output video to the Rec. 709 Range on saving")
  ("PYUV",                      m_packedYUVMode,                       false,      "If true then output 10-bit and 12-bit YUV data as 5-byte and 3-byte (respectively) packed YUV data. Ignored for interlaced output.")
#if ENABLE_TRACING
  ("TraceChannelsList",         bTracingChannelsList,                  false,      "List all available tracing channels")
  ("TraceRule",                 sTracingRule,                          std::string(""), "Tracing rule (ex: \"D_CABAC:poc==8\" or \"D_REC_CB_LUMA:poc==8\")")
  ("TraceFile",                 sTracingFile,                          std::string(""), "Tracing file")
#endif
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  ("CacheCfg",                  m_cacheCfgFile,                        std::string(""), "CacheCfg File")
#endif
#if RExt__DECODER_DEBUG_STATISTICS
  ("Stats",                     m_statMode,                            3,          "Control decoder debugging statistic output mode\n"
                                                                                   "\t0: disable statistic\n"
                                                                                   "\t1: enable bit statistic\n"
                                                                                   "\t2: enable tool statistic\n"
                                                                                   "\t3: enable bit and tool statistic\n")
#endif
#if GREEN_METADATA_SEI_ENABLED
  ("GMFA", m_GMFA, false, "Write output file for the Green-Metadata analyzer for decoder complexity metrics (JVET-P0085)\n")
  ("GMFAFile", m_GMFAFile, std::string(""), "File for the Green Metadata Bit Stream Feature Analyzer output (JVET-P0085)\n")
  ("GMFAFramewise", m_GMFAFramewise, false, "Output of frame-wise Green Metadata Bit Stream Feature Analyzer files\n")
#endif
  ("MCTSCheck",                m_mctsCheck,                            false,      "If enabled, the decoder checks for violations of mc_exact_sample_value_match_flag in Temporal MCTS ")
  ("targetSubPicIdx",          m_targetSubPicIdx,                      0,          "Specify which subpicture shall be written to output, using subpic index, 0: disabled, subpicIdx=m_targetSubPicIdx-1 \n" )
  ("UpscaledOutput",           m_upscaledOutput,                       0,          "Output upscaled (2), decoded but in full resolution buffer (1) or decoded cropped (0, default) picture for RPR" )
  ("UpscaledOutputWidth",      m_upscaledOutputWidth,                  0,          "Forced upscaled output width (override SPS)" )
  ("UpscaledOutputHeight",     m_upscaledOutputHeight,                 0,          "Forced upscaled output height (override SPS)" )
  ("UpscaleFilterForDisplay",  m_upscaleFilterForDisplay,              1,          "Filters used for upscaling reconstruction to full resolution (2: ECM 12 - tap luma and 6 - tap chroma MC filters, 1 : Alternative 12 - tap luma and 6 - tap chroma filters, 0 : VVC 8 - tap luma and 4 - tap chroma MC filters)")
#if JVET_AJ0151_DSC_SEI
  ("KeyStoreDir",              m_keyStoreDir,            std::string("keystore/pub"),    "Directory for locally stored public keys for verifying digitally signed content")
  ("TrustStoreDir",            m_trustStoreDir,          std::string("keystore/ca"),     "Directory for locally stored trusted CA certificates")
#endif
#if GDR_LEAK_TEST
  ("RandomAccessPos",           m_gdrPocRandomAccess,                  0,          "POC of GDR Random access picture\n")
#endif // GDR_LEAK_TEST
    ;
  // clang-format on

  po::setDefaults(opts);
  po::ErrorReporter err;
  const std::list<const char *> &argv_unhandled = po::scanArgv(opts, argc, (const char **) argv, err);

  for (std::list<const char *>::const_iterator it = argv_unhandled.begin(); it != argv_unhandled.end(); it++)
  {
    msg( ERROR, "Unhandled argument ignored: `%s'\n", *it);
  }

  if (argc == 1 || do_help)
  {
    po::doHelp(std::cout, opts);
    return false;
  }

  if (err.is_errored)
  {
    if (!warnUnknowParameter)
    {
      /* errors have already been reported to stderr */
      return false;
    }
  }

#if ENABLE_TRACING
  g_trace_ctx = tracing_init( sTracingFile, sTracingRule );
  if( bTracingChannelsList && g_trace_ctx )
  {
    std::string sChannelsList;
    g_trace_ctx->getChannelsList( sChannelsList );
    msg( INFO, "\nAvailable tracing channels:\n\n%s\n", sChannelsList.c_str() );
  }
#endif

  g_mctsDecCheckEnabled = m_mctsCheck;
  // Chroma output bit-depth
  if (m_outputBitDepth[ChannelType::LUMA] != 0 && m_outputBitDepth[ChannelType::CHROMA] == 0)
  {
    m_outputBitDepth[ChannelType::CHROMA] = m_outputBitDepth[ChannelType::LUMA];
  }

  m_outputColourSpaceConvert = stringToInputColourSpaceConvert(outputColourSpaceConvert, false);
  if (m_outputColourSpaceConvert>=NUMBER_INPUT_COLOUR_SPACE_CONVERSIONS)
  {
    msg( ERROR, "Bad output colour space conversion string\n");
    return false;
  }

  if (m_bitstreamFileName.empty())
  {
    msg( ERROR, "No input file specified, aborting\n");
    return false;
  }

  if ( !cfg_TargetDecLayerIdSetFile.empty() )
  {
    FILE* targetDecLayerIdSetFile = fopen ( cfg_TargetDecLayerIdSetFile.c_str(), "r" );
    if ( targetDecLayerIdSetFile )
    {
      bool isLayerIdZeroIncluded = false;
      while ( !feof(targetDecLayerIdSetFile) )
      {
        int layerIdParsed = 0;
        if ( fscanf( targetDecLayerIdSetFile, "%d ", &layerIdParsed ) != 1 )
        {
          if ( m_targetDecLayerIdSet.size() == 0 )
          {
            msg( ERROR, "No LayerId could be parsed in file %s. Decoding all LayerIds as default.\n", cfg_TargetDecLayerIdSetFile.c_str() );
          }
          break;
        }
        if ( layerIdParsed  == -1 ) // The file includes a -1, which means all LayerIds are to be decoded.
        {
          m_targetDecLayerIdSet.clear(); // Empty set means decoding all layers.
          break;
        }
        if ( layerIdParsed < 0 || layerIdParsed >= MAX_NUM_LAYER_IDS )
        {
          msg( ERROR, "Warning! Parsed LayerId %d is not within allowed range [0,%d]. Ignoring this value.\n", layerIdParsed, MAX_NUM_LAYER_IDS-1 );
        }
        else
        {
          isLayerIdZeroIncluded = layerIdParsed == 0 ? true : isLayerIdZeroIncluded;
          m_targetDecLayerIdSet.push_back ( layerIdParsed );
        }
      }
      fclose (targetDecLayerIdSetFile);
      if ( m_targetDecLayerIdSet.size() > 0 && !isLayerIdZeroIncluded )
      {
        msg( ERROR, "TargetDecLayerIdSet must contain LayerId=0, aborting" );
        return false;
      }
    }
    else
    {
      msg( ERROR, "File %s could not be opened. Using all LayerIds as default.\n", cfg_TargetDecLayerIdSetFile.c_str() );
    }
  }

  m_mTidExternalSet = m_maxTemporalLayer != TL_UNDEFINED;
  if (!m_mTidExternalSet)
  {
    m_maxTemporalLayer = TL_INFINITY;
  }

  if ( m_targetOlsIdx != 500)
  {
    m_tOlsIdxTidExternalSet = true;
  }
  else
  {
    m_targetOlsIdx = -1;
  }

  return true;
}

DecAppCfg::DecAppCfg()
  : m_bitstreamFileName()
  , m_reconFileName()
  , m_oplFilename()

  , m_iSkipFrame(0)
  // m_outputBitDepth array initialised below
  , m_outputColourSpaceConvert(IPCOLOURSPACE_UNCHANGED)
  , m_targetOlsIdx(0)
  , m_tOlsIdxTidExternalSet(false)
  , m_decodedPictureHashSEIEnabled(0)
  , m_decodedNoDisplaySEIEnabled(false)
  , m_colourRemapSEIFileName()
  , m_SEICTIFileName()
  , m_SEIFGSFileName()
  , m_annotatedRegionsSEIFileName()
  , m_objectMaskInfoSEIFileName()
  , m_targetDecLayerIdSet()
  , m_outputDecodedSEIMessagesFilename()
#if JVET_S0257_DUMP_360SEI_MESSAGE
  , m_outputDecoded360SEIMessagesFilename()
#endif
  , m_clipOutputVideoToRec709Range(false)
  , m_packedYUVMode(false)
  , m_statMode(0)
  , m_mctsCheck(false)
{
  m_outputBitDepth.fill(0);
}

DecAppCfg::~DecAppCfg()
{
#if ENABLE_TRACING
  tracing_uninit( g_trace_ctx );
#endif
}

//! \}
