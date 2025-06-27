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

/** \file     SEIFilmGrainAppCfg.cpp
    \brief    Decoder configuration class
*/

#include <cstdio>
#include <cstring>
#include <string>
#include "SEIFilmGrainAppCfg.h"
#include "Utilities/program_options_lite.h"
#if ENABLE_TRACING
#include "CommonLib/dtrace_next.h"
#endif

namespace po = ProgramOptionsLite;

//! \ingroup SEIFilmGrainApp
//! \{

template<class T> static inline std::istream &operator>>(std::istream &in, SMultiValueInput<T> &values)
{
  return values.readValues(in);
}

template <class T>
T SMultiValueInput<T>::readValue(const char *&pStr, bool &bSuccess)
{
  T val = T();
  std::string s(pStr);
  std::replace(s.begin(), s.end(), ',', ' '); // make comma separated into space separated
  std::istringstream iss(s);
  iss >> val;
  bSuccess = !iss.fail() // check nothing has gone wrong
    && !(val<minValIncl || val>maxValIncl) // check value is within range
    && (int)iss.tellg() != 0 // check we've actually read something
    && (iss.eof() || iss.peek() == ' '); // check next character is a space, or eof
  pStr += (iss.eof() ? s.size() : (std::size_t)iss.tellg());
  return val;
}

template<class T> std::istream &SMultiValueInput<T>::readValues(std::istream &in)
{
  values.clear();
  std::string str;
  while (!in.eof())
  {
    std::string tmp;
    in >> tmp;
    str += " " + tmp;
  }
  if (!str.empty())
  {
    const char *pStr = str.c_str();
    // soak up any whitespace
    for (; isspace(*pStr); pStr++);

    while (*pStr != 0)
    {
      bool bSuccess = true;
      T val = readValue(pStr, bSuccess);
      if (!bSuccess)
      {
        in.setstate(std::ios::failbit);
        break;
      }

      if (maxNumValuesIncl != 0 && values.size() >= maxNumValuesIncl)
      {
        in.setstate(std::ios::failbit);
        break;
      }
      values.push_back(val);
      // soak up any whitespace and up to 1 comma.
      for (; isspace(*pStr); pStr++);
      if (*pStr == ',')
      {
        pStr++;
      }
      for (; isspace(*pStr); pStr++);
    }
  }
  if (values.size() < minNumValuesIncl)
  {
    in.setstate(std::ios::failbit);
  }
  return in;
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/** \param argc number of arguments
    \param argv array of arguments
 */
bool SEIFilmGrainAppCfg::parseCfg( int argc, char* argv[] )
{
#if ENABLE_TRACING
  bool printTracingChannelsList;
  std::string tracingFile;
  std::string tracingRule;
#endif

  bool do_help = false;
  int warnUnknowParameter = 0;

  SMultiValueInput<uint32_t>    cfg_FgcSEIIntensityIntervalLowerBoundComp0(0, 255, 0, 256);
  SMultiValueInput<uint32_t>    cfg_FgcSEIIntensityIntervalLowerBoundComp1(0, 255, 0, 256);
  SMultiValueInput<uint32_t>    cfg_FgcSEIIntensityIntervalLowerBoundComp2(0, 255, 0, 256);
  SMultiValueInput<uint32_t>    cfg_FgcSEIIntensityIntervalUpperBoundComp0(0, 255, 0, 256);
  SMultiValueInput<uint32_t>    cfg_FgcSEIIntensityIntervalUpperBoundComp1(0, 255, 0, 256);
  SMultiValueInput<uint32_t>    cfg_FgcSEIIntensityIntervalUpperBoundComp2(0, 255, 0, 256);
  SMultiValueInput<int32_t>     cfg_FgcSEICompModelValueComp0(-32768, 32767, 0, 256 * 6);
  SMultiValueInput<int32_t>     cfg_FgcSEICompModelValueComp1(-32768, 32767, 0, 256 * 6);
  SMultiValueInput<int32_t>     cfg_FgcSEICompModelValueComp2(-32768, 32767, 0, 256 * 6);

  po::Options opts;

  // clang-format off
  opts.addOptions()
  ("help",                      do_help,                               false,      "this help text")
  ("c",                         po::parseConfigFile,                               "film grain configuration file name")
  ("BitstreamFileIn,b",         m_bitstreamFileNameIn,                 std::string(""), "bitstream input file name")
  ("BitstreamFileOut,o",        m_bitstreamFileNameOut,                std::string(""), "bitstream output file name")
  ("SEIFilmGrainOption",        m_seiFilmGrainOption,                  0,          "process FGC SEI option (0:disable, 1:remove, 2:insert, 3:change)" )
  ("SEIFilmGrainPrint",         m_seiFilmGrainPrint,                   false,      "print output film grain characteristics SEI message (1:enable)")
// film grain characteristics SEI
  ("SEIFGCEnabled",                                   m_fgcSEIEnabled,                                   false, "Control generation of the film grain characteristics SEI message")
  ("SEIFGCAnalysisEnabled",                           m_fgcSEIAnalysisEnabled,                           false, "Control adaptive film grain parameter estimation - film grain analysis")
  ("SEIFGCCancelFlag",                                m_fgcSEICancelFlag,                                 true, "Specifies the persistence of any previous film grain characteristics SEI message in output order.")
  ("SEIFGCPersistenceFlag",                           m_fgcSEIPersistenceFlag,                           false, "Specifies the persistence of the film grain characteristics SEI message for the current layer.")
  ("SEIFGCPerPictureSEI",                             m_fgcSEIPerPictureSEI,                             false, "Film Grain SEI is added for each picture as speciffied in RDD5 to ensure bit accurate synthesis in tricky mode")
  ("SEIFGCModelID",                                   m_fgcSEIModelID,                                      0u, "Specifies the film grain simulation model. 0: frequency filtering; 1: auto-regression.")
  ("SEIFGCSepColourDescPresentFlag",                  m_fgcSEISepColourDescPresentFlag,                  false, "Specifies the presence of a distinct colour space description for the film grain characteristics specified in the SEI message.")
  ("SEIFGCBlendingModeID",                            m_fgcSEIBlendingModeID,                               0u, "Specifies the blending mode used to blend the simulated film grain with the decoded images. 0: additive; 1: multiplicative.")
  ("SEIFGCLog2ScaleFactor",                           m_fgcSEILog2ScaleFactor,                              0u, "Specifies a scale factor used in the film grain characterization equations.")
  ("SEIFGCCompModelPresentComp0",                     m_fgcSEICompModelPresent[0],                       false, "Specifies the presence of film grain modelling on colour component 0.")
  ("SEIFGCCompModelPresentComp1",                     m_fgcSEICompModelPresent[1],                       false, "Specifies the presence of film grain modelling on colour component 1.")
  ("SEIFGCCompModelPresentComp2",                     m_fgcSEICompModelPresent[2],                       false, "Specifies the presence of film grain modelling on colour component 2.")
  ("SEIFGCNumIntensityIntervalMinus1Comp0",           m_fgcSEINumIntensityIntervalMinus1[0],                0u, "Specifies the number of intensity intervals minus1 on colour component 0.")
  ("SEIFGCNumIntensityIntervalMinus1Comp1",           m_fgcSEINumIntensityIntervalMinus1[1],                0u, "Specifies the number of intensity intervals minus1 on colour component 1.")
  ("SEIFGCNumIntensityIntervalMinus1Comp2",           m_fgcSEINumIntensityIntervalMinus1[2],                0u, "Specifies the number of intensity intervals minus1 on colour component 2.")
  ("SEIFGCNumModelValuesMinus1Comp0",                 m_fgcSEINumModelValuesMinus1[0],                      0u, "Specifies the number of component model values minus1 on colour component 0.")
  ("SEIFGCNumModelValuesMinus1Comp1",                 m_fgcSEINumModelValuesMinus1[1],                      0u, "Specifies the number of component model values minus1 on colour component 1.")
  ("SEIFGCNumModelValuesMinus1Comp2",                 m_fgcSEINumModelValuesMinus1[2],                      0u, "Specifies the number of component model values minus1 on colour component 2.")
  ("SEIFGCIntensityIntervalLowerBoundComp0", cfg_FgcSEIIntensityIntervalLowerBoundComp0, cfg_FgcSEIIntensityIntervalLowerBoundComp0, "Specifies the lower bound for the intensity intervals on colour component 0.")
  ("SEIFGCIntensityIntervalLowerBoundComp1", cfg_FgcSEIIntensityIntervalLowerBoundComp1, cfg_FgcSEIIntensityIntervalLowerBoundComp1, "Specifies the lower bound for the intensity intervals on colour component 1.")
  ("SEIFGCIntensityIntervalLowerBoundComp2", cfg_FgcSEIIntensityIntervalLowerBoundComp2, cfg_FgcSEIIntensityIntervalLowerBoundComp2, "Specifies the lower bound for the intensity intervals on colour component 2.")
  ("SEIFGCIntensityIntervalUpperBoundComp0", cfg_FgcSEIIntensityIntervalUpperBoundComp0, cfg_FgcSEIIntensityIntervalUpperBoundComp0, "Specifies the upper bound for the intensity intervals on colour component 0.")
  ("SEIFGCIntensityIntervalUpperBoundComp1", cfg_FgcSEIIntensityIntervalUpperBoundComp1, cfg_FgcSEIIntensityIntervalUpperBoundComp1, "Specifies the upper bound for the intensity intervals on colour component 1.")
  ("SEIFGCIntensityIntervalUpperBoundComp2", cfg_FgcSEIIntensityIntervalUpperBoundComp2, cfg_FgcSEIIntensityIntervalUpperBoundComp2, "Specifies the upper bound for the intensity intervals on colour component 2.")
  ("SEIFGCCompModelValuesComp0",             cfg_FgcSEICompModelValueComp0,              cfg_FgcSEICompModelValueComp0,              "Specifies the component model values on colour component 0.")
  ("SEIFGCCompModelValuesComp1",             cfg_FgcSEICompModelValueComp1,              cfg_FgcSEICompModelValueComp1,              "Specifies the component model values on colour component 1.")
  ("SEIFGCCompModelValuesComp2",             cfg_FgcSEICompModelValueComp2,              cfg_FgcSEICompModelValueComp2,              "Specifies the component model values on colour component 2.")

#if ENABLE_TRACING
  ("TraceChannelsList",         printTracingChannelsList,              false,      "List all available tracing channels" )
  ("TraceRule",                 tracingRule,                           std::string(""), "Tracing rule (ex: \"D_CABAC:poc==8\" or \"D_REC_CB_LUMA:poc==8\")" )
  ("TraceFile",                 tracingFile,                           std::string(""), "Tracing file" )
#endif
  ("WarnUnknowParameter,w",     warnUnknowParameter,                   0,          "warn for unknown configuration parameters instead of failing")
  ;
  // clang-format on

  po::setDefaults(opts);
  po::ErrorReporter err;
  const std::list<const char *> &argv_unhandled = po::scanArgv(opts, argc, (const char **) argv, err);

  for (std::list<const char *>::const_iterator it = argv_unhandled.begin(); it != argv_unhandled.end(); it++)
  {
    std::cerr << "Unhandled argument ignored: "<< *it << std::endl;
  }

  if (argc == 1 || do_help)
  {
    po::doHelp(std::cout, opts);
    return false;
  }

#if ENABLE_TRACING
  g_trace_ctx = tracing_init(tracingFile, tracingRule);
  if (printTracingChannelsList && g_trace_ctx)
  {
    std::string channelsList;
    g_trace_ctx->getChannelsList(channelsList);
    msg(INFO, "\nAvailable tracing channels:\n\n%s\n", channelsList.c_str());
  }
  DTRACE_UPDATE(g_trace_ctx, std::make_pair("final", 1));
#endif

  if (err.is_errored)
  {
    if (!warnUnknowParameter)
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

  // set sei film grain parameters.
  if (m_fgcSEIEnabled)
  {
    if (m_fgcSEIAnalysisEnabled) {
      msg(WARNING, "*************************************************************************\n");
      msg(WARNING, "* WARNING: SEIFGCAnalysisEnabled needs to be set to 0! *\n");
      msg(WARNING, "*************************************************************************\n");
      m_fgcSEIAnalysisEnabled = false;
    }
    if (!m_fgcSEIPerPictureSEI && !m_fgcSEIPersistenceFlag) {
      msg(WARNING, "*************************************************************************\n");
      msg(WARNING, "* WARNING: SEIPerPictureSEI is set to 0, SEIPersistenceFlag needs to be set to 1! *\n");
      msg(WARNING, "*************************************************************************\n");
      m_fgcSEIPersistenceFlag = true;
    }
    else if (m_fgcSEIPerPictureSEI && m_fgcSEIPersistenceFlag) {
      msg(WARNING, "*************************************************************************\n");
      msg(WARNING, "* WARNING: SEIPerPictureSEI is set to 1, SEIPersistenceFlag needs to be set to 0! *\n");
      msg(WARNING, "*************************************************************************\n");
      m_fgcSEIPersistenceFlag = false;
    }
    m_fgcSEILog2ScaleFactor = m_fgcSEILog2ScaleFactor ? m_fgcSEILog2ScaleFactor : 2;

    uint32_t numModelCtr;
    if (m_fgcSEICompModelPresent[0])
    {
      numModelCtr = 0;
      for (uint8_t i = 0; i <= m_fgcSEINumIntensityIntervalMinus1[0]; i++)
      {
        m_fgcSEIIntensityIntervalLowerBound[0][i] = uint32_t((cfg_FgcSEIIntensityIntervalLowerBoundComp0.values.size() > i) ? cfg_FgcSEIIntensityIntervalLowerBoundComp0.values[i] : 10);
        m_fgcSEIIntensityIntervalUpperBound[0][i] = uint32_t((cfg_FgcSEIIntensityIntervalUpperBoundComp0.values.size() > i) ? cfg_FgcSEIIntensityIntervalUpperBoundComp0.values[i] : 250);
        for (uint8_t j = 0; j <= m_fgcSEINumModelValuesMinus1[0]; j++)
        {
          m_fgcSEICompModelValue[0][i][j] = int32_t((cfg_FgcSEICompModelValueComp0.values.size() > numModelCtr) ? cfg_FgcSEICompModelValueComp0.values[numModelCtr] : 24);
          numModelCtr++;
        }
      }
    }
    if (m_fgcSEICompModelPresent[1])
    {
      numModelCtr = 0;
      for (uint8_t i = 0; i <= m_fgcSEINumIntensityIntervalMinus1[1]; i++)
      {
        m_fgcSEIIntensityIntervalLowerBound[1][i] = uint32_t((cfg_FgcSEIIntensityIntervalLowerBoundComp1.values.size() > i) ? cfg_FgcSEIIntensityIntervalLowerBoundComp1.values[i] : 60);
        m_fgcSEIIntensityIntervalUpperBound[1][i] = uint32_t((cfg_FgcSEIIntensityIntervalUpperBoundComp1.values.size() > i) ? cfg_FgcSEIIntensityIntervalUpperBoundComp1.values[i] : 200);

        for (uint8_t j = 0; j <= m_fgcSEINumModelValuesMinus1[1]; j++)
        {
          m_fgcSEICompModelValue[1][i][j] = int32_t((cfg_FgcSEICompModelValueComp1.values.size() > numModelCtr) ? cfg_FgcSEICompModelValueComp1.values[numModelCtr] : 16);
          numModelCtr++;
        }
      }
    }
    if (m_fgcSEICompModelPresent[2])
    {
      numModelCtr = 0;
      for (uint8_t i = 0; i <= m_fgcSEINumIntensityIntervalMinus1[2]; i++)
      {
        m_fgcSEIIntensityIntervalLowerBound[2][i] = uint32_t((cfg_FgcSEIIntensityIntervalLowerBoundComp2.values.size() > i) ? cfg_FgcSEIIntensityIntervalLowerBoundComp2.values[i] : 60);
        m_fgcSEIIntensityIntervalUpperBound[2][i] = uint32_t((cfg_FgcSEIIntensityIntervalUpperBoundComp2.values.size() > i) ? cfg_FgcSEIIntensityIntervalUpperBoundComp2.values[i] : 250);

        for (uint8_t j = 0; j <= m_fgcSEINumModelValuesMinus1[2]; j++)
        {
          m_fgcSEICompModelValue[2][i][j] = int32_t((cfg_FgcSEICompModelValueComp2.values.size() > numModelCtr) ? cfg_FgcSEICompModelValueComp2.values[numModelCtr] : 12);
          numModelCtr++;
        }
      }
    }
  }

  return true;
}

SEIFilmGrainAppCfg::SEIFilmGrainAppCfg()
: m_bitstreamFileNameIn()
, m_bitstreamFileNameOut()
, m_seiFilmGrainOption()
, m_seiFilmGrainPrint(false)
{
}

SEIFilmGrainAppCfg::~SEIFilmGrainAppCfg()
{
#if ENABLE_TRACING
  tracing_uninit(g_trace_ctx);
#endif
}

//! \}
