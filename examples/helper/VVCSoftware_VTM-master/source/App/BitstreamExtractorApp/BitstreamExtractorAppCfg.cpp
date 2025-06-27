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


#include <cstdio>
#include <cstring>
#include <string>
#include "CommonLib/CommonDef.h"
#include "BitstreamExtractorApp.h"
#include "Utilities/program_options_lite.h"
#if ENABLE_TRACING
#include "CommonLib/dtrace_next.h"
#endif

namespace po = ProgramOptionsLite;

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

// argc number of arguments
// argv array of arguments
 bool BitstreamExtractorAppCfg::parseCfg( int argc, char* argv[] )
{
#if ENABLE_TRACING
  bool printTracingChannelsList;
  std::string tracingFile;
  std::string tracingRule;
#endif

  bool printHelp = false;
  bool warnUnknownParameter = false;
  int  verbosity;

  po::Options opts;

  // clang-format off
  opts.addOptions()
  ("help",                      printHelp,                             false,      "This help text")
  ("BitstreamFileIn,b",         m_bitstreamFileNameIn,                 std::string(""), "Bitstream input file name")
  ("BitstreamFileOut,o",        m_bitstreamFileNameOut,                std::string(""), "bitstream output file name")
  ("MaxTemporalLayer,t",        m_maxTemporalLayer,                    -1,         "Maximum Temporal Layer to be decoded. -1 to decode all layers")
  ("TargetOutputLayerSet,p",    m_targetOlsIdx,                        -1,         "Target output layer set index")
  ("SubPicIdx,s",               m_subPicIdx,                           -1,         "Target subpic index for target output layers that containing multiple subpictures. -1 to decode all subpictures")

#if ENABLE_TRACING
  ("TraceChannelsList",         printTracingChannelsList,              false,        "List all available tracing channels" )
  ("TraceRule",                 tracingRule,                           std::string( "" ), "Tracing rule (ex: \"D_CABAC:poc==8\" or \"D_REC_CB_LUMA:poc==8\")" )
  ("TraceFile",                 tracingFile,                           std::string( "" ), "Tracing file" )
#endif

  ("Verbosity,v",               verbosity,                             (int)VERBOSE, "Specifies the level of the verboseness")
  ("WarnUnknowParameter,w",     warnUnknownParameter,                  false,        "Warn for unknown configuration parameters instead of failing")
  ;
  // clang-format on

  po::setDefaults(opts);
  po::ErrorReporter err;
  const std::list<const char *> &argv_unhandled = po::scanArgv(opts, argc, (const char **) argv, err);

  for (std::list<const char *>::const_iterator it = argv_unhandled.begin(); it != argv_unhandled.end(); it++)
  {
    std::cerr << "Unhandled argument ignored: "<< *it << std::endl;
  }

  if (argc == 1 || printHelp)
  {
    po::doHelp(std::cout, opts);
    return false;
  }

#if ENABLE_TRACING
  g_trace_ctx = tracing_init( tracingFile, tracingRule );
  if( printTracingChannelsList && g_trace_ctx )
  {
    std::string channelsList;
    g_trace_ctx->getChannelsList( channelsList );
    msg( INFO, "\nAvailable tracing channels:\n\n%s\n", channelsList.c_str() );
  }
  DTRACE_UPDATE( g_trace_ctx, std::make_pair( "final", 1 ) );
#endif

  g_verbosity = MsgLevel( verbosity );

  if (err.is_errored)
  {
    if (!warnUnknownParameter)
    {
      /* errors have already been reported to stderr */
      return false;
    }
  }


  if (m_bitstreamFileNameIn.empty())
  {
    std::cerr << "No input file specified, aborting" << std::endl;
    return false;
  }
  if (m_bitstreamFileNameOut.empty())
  {
    std::cerr << "No output file specified, aborting" << std::endl;
    return false;
  }


  return true;
}

BitstreamExtractorAppCfg::BitstreamExtractorAppCfg()
: m_bitstreamFileNameIn()
, m_bitstreamFileNameOut()
, m_maxTemporalLayer( 0 )
, m_targetOlsIdx( 0 )
, m_subPicIdx( -1 )
{
}

BitstreamExtractorAppCfg::~BitstreamExtractorAppCfg()
{
#if ENABLE_TRACING
  tracing_uninit( g_trace_ctx );
#endif
}

