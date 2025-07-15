/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2020, ITU/ISO/IEC
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

using namespace std;
namespace po = df::program_options_lite;

#if JVET_X0048_X0103_FILM_GRAIN
//! \ingroup SEIFilmGrainApp
//! \{

template <class T>
static inline istream& operator >> (std::istream &in, SMultiValueInput<T> &values)
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

template <class T>
istream& SMultiValueInput<T>::readValues(std::istream &in)
{
  values.clear();
  string str;
  while (!in.eof())
  {
    string tmp; in >> tmp; str += " " + tmp;
  }
  if (!str.empty())
  {
    const TChar *pStr=str.c_str();
    // soak up any whitespace
    for (; isspace(*pStr); pStr++);

    while (*pStr != 0)
    {
      Bool bSuccess=true;
      T val = readValue(pStr, bSuccess);
      if (!bSuccess)
      {
        in.setstate(ios::failbit);
        break;
      }

      if (maxNumValuesIncl != 0 && values.size() >= maxNumValuesIncl)
      {
        in.setstate(ios::failbit);
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
    in.setstate(ios::failbit);
  }
  return in;
}

template <class T1, class T2>
static inline istream& operator >> (std::istream &in, std::map<T1, T2> &map)
{
  T1 key;
  T2 value;
  try
  {
    in >> key;
    in >> value;
  }
  catch (...)
  {
    in.setstate(ios::failbit);
  }

  map[key] = value;
  return in;
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/** \param argc number of arguments
    \param argv array of arguments
 */
Bool SEIFilmGrainAppCfg::parseCfg( Int argc, TChar* argv[] )
{
  Bool do_help = false;
  Int warnUnknowParameter = 0;

  // default values used for FGC SEI parameter parsing
  SMultiValueInput<UInt>  cfg_FgcSEIIntensityIntervalLowerBoundComp[MAX_NUM_COMPONENT]={SMultiValueInput<UInt> (0, 255, 0, 256), SMultiValueInput<UInt> (0, 255, 0, 256), SMultiValueInput<UInt> (0, 255, 0, 256)};
  SMultiValueInput<UInt>  cfg_FgcSEIIntensityIntervalUpperBoundComp[MAX_NUM_COMPONENT]={SMultiValueInput<UInt> (0, 255, 0, 256), SMultiValueInput<UInt> (0, 255, 0, 256), SMultiValueInput<UInt> (0, 255, 0, 256)};
  SMultiValueInput<UInt>  cfg_FgcSEICompModelValueComp[MAX_NUM_COMPONENT]={SMultiValueInput<UInt> (0, 65535, 0, 256 * 6), SMultiValueInput<UInt> (0, 65535, 0, 256 * 6), SMultiValueInput<UInt> (0, 65535, 0, 256 * 6)};

  po::Options opts;
  opts.addOptions()

  ("help",                      do_help,                               false,      "this help text")
  ("c",                         po::parseConfigFile,                               "film grain configuration file name")
  ("BitstreamFileIn,b",         m_bitstreamFileNameIn,                 string(""), "bitstream input file name")
  ("BitstreamFileOut,o",        m_bitstreamFileNameOut,                string(""), "bitstream output file name")
  ("SEIFilmGrainOption",        m_seiFilmGrainOption,                  0,          "process FGC SEI option (0:disable, 1:remove, 2:insert, 3:change)" )
  ("SEIFilmGrainPrint",         m_seiFilmGrainPrint,                   false,      "print output film grain characteristics SEI message (1:enable)")
// film grain characteristics SEI
  ("SEIFGCEnabled",                                   m_fgcSEIEnabled,                                   false, "Control generation of the film grain characteristics SEI message")
  ("SEIFGCAnalysisEnabled",                           m_fgcSEIAnalysisEnabled,                           false, "Control adaptive film grain parameter estimation - film grain analysis")
  ("SEIFGCCancelFlag",                                m_fgcSEICancelFlag,                                false, "Specifies the persistence of any previous film grain characteristics SEI message in output order.")
  ("SEIFGCPersistenceFlag",                           m_fgcSEIPersistenceFlag,                           false, "Specifies the persistence of the film grain characteristics SEI message for the current layer.")
  ("SEIFGCPerPictureSEI",                             m_fgcSEIPerPictureSEI,                             false, "Film Grain SEI is added for each picture as speciffied in RDD5 to ensure bit accurate synthesis in tricky mode")
  ("SEIFGCModelID",                                   m_fgcSEIModelID,                                      0u, "Specifies the film grain simulation model. 0: frequency filtering; 1: auto-regression.")
  ("SEIFGCSepColourDescPresentFlag",                  m_fgcSEISepColourDescPresentFlag,                  false, "Specifies the presence of a distinct colour space description for the film grain characteristics specified in the SEI message.")
  ("SEIFGCBlendingModeID",                            m_fgcSEIBlendingModeID,                               0u, "Specifies the blending mode used to blend the simulated film grain with the decoded images. 0: additive; 1: multiplicative.")
  ("SEIFGCLog2ScaleFactor",                           m_fgcSEILog2ScaleFactor,                              2u, "Specifies a scale factor used in the film grain characterization equations.")
  ("SEIFGCCompModelPresentComp0",                     m_fgcSEICompModelPresent[0],                       false, "Specifies the presence of film grain modelling on colour component 0.")
  ("SEIFGCCompModelPresentComp1",                     m_fgcSEICompModelPresent[1],                       false, "Specifies the presence of film grain modelling on colour component 1.")
  ("SEIFGCCompModelPresentComp2",                     m_fgcSEICompModelPresent[2],                       false, "Specifies the presence of film grain modelling on colour component 2.")
  ("SEIFGCNumIntensityIntervalMinus1Comp0",           m_fgcSEINumIntensityIntervalMinus1[0],                0u, "Specifies the number of intensity intervals minus1 on colour component 0.")
  ("SEIFGCNumIntensityIntervalMinus1Comp1",           m_fgcSEINumIntensityIntervalMinus1[1],                0u, "Specifies the number of intensity intervals minus1 on colour component 1.")
  ("SEIFGCNumIntensityIntervalMinus1Comp2",           m_fgcSEINumIntensityIntervalMinus1[2],                0u, "Specifies the number of intensity intervals minus1 on colour component 2.")
  ("SEIFGCNumModelValuesMinus1Comp0",                 m_fgcSEINumModelValuesMinus1[0],                      0u, "Specifies the number of component model values minus1 on colour component 0.")
  ("SEIFGCNumModelValuesMinus1Comp1",                 m_fgcSEINumModelValuesMinus1[1],                      0u, "Specifies the number of component model values minus1 on colour component 1.")
  ("SEIFGCNumModelValuesMinus1Comp2",                 m_fgcSEINumModelValuesMinus1[2],                      0u, "Specifies the number of component model values minus1 on colour component 2.")
  ("SEIFGCIntensityIntervalLowerBoundComp0", cfg_FgcSEIIntensityIntervalLowerBoundComp[0], cfg_FgcSEIIntensityIntervalLowerBoundComp[0], "Specifies the lower bound for the intensity intervals on colour component 0.")
  ("SEIFGCIntensityIntervalLowerBoundComp1", cfg_FgcSEIIntensityIntervalLowerBoundComp[1], cfg_FgcSEIIntensityIntervalLowerBoundComp[1], "Specifies the lower bound for the intensity intervals on colour component 1.")
  ("SEIFGCIntensityIntervalLowerBoundComp2", cfg_FgcSEIIntensityIntervalLowerBoundComp[2], cfg_FgcSEIIntensityIntervalLowerBoundComp[2], "Specifies the lower bound for the intensity intervals on colour component 2.")
  ("SEIFGCIntensityIntervalUpperBoundComp0", cfg_FgcSEIIntensityIntervalUpperBoundComp[0], cfg_FgcSEIIntensityIntervalUpperBoundComp[0], "Specifies the upper bound for the intensity intervals on colour component 0.")
  ("SEIFGCIntensityIntervalUpperBoundComp1", cfg_FgcSEIIntensityIntervalUpperBoundComp[1], cfg_FgcSEIIntensityIntervalUpperBoundComp[1], "Specifies the upper bound for the intensity intervals on colour component 1.")
  ("SEIFGCIntensityIntervalUpperBoundComp2", cfg_FgcSEIIntensityIntervalUpperBoundComp[2], cfg_FgcSEIIntensityIntervalUpperBoundComp[2], "Specifies the upper bound for the intensity intervals on colour component 2.")
  ("SEIFGCCompModelValuesComp0", cfg_FgcSEICompModelValueComp[0], cfg_FgcSEICompModelValueComp[0], "Specifies the component model values on colour component 0.")
  ("SEIFGCCompModelValuesComp1", cfg_FgcSEICompModelValueComp[1], cfg_FgcSEICompModelValueComp[1], "Specifies the component model values on colour component 1.")
  ("SEIFGCCompModelValuesComp2", cfg_FgcSEICompModelValueComp[2], cfg_FgcSEICompModelValueComp[2], "Specifies the component model values on colour component 2.")

  ("WarnUnknowParameter,w",     warnUnknowParameter,                   0,          "warn for unknown configuration parameters instead of failing")
  ;

  po::setDefaults(opts);
  po::ErrorReporter err;
  const list<const TChar*>& argv_unhandled = po::scanArgv(opts, argc, (const TChar**)argv, err);

  for (list<const TChar*>::const_iterator it = argv_unhandled.begin(); it != argv_unhandled.end(); it++)
  {
    std::cerr << "Unhandled argument ignored: "<< *it << std::endl;
  }

  if (argc == 1 || do_help)
  {
    po::doHelp(cout, opts);
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
      fprintf(stderr, "*************************************************************************\n");
      fprintf(stderr, "* WARNING: SEIFGCAnalysisEnabled needs to be set to 0! *\n");
      fprintf(stderr, "*************************************************************************\n");
      m_fgcSEIAnalysisEnabled = false;
    }
    if (!m_fgcSEIPerPictureSEI && !m_fgcSEIPersistenceFlag) {
      fprintf(stderr, "*************************************************************************\n");
      fprintf(stderr, "* WARNING: SEIPerPictureSEI is set to 0, SEIPersistenceFlag needs to be set to 1! *\n");
      fprintf(stderr, "*************************************************************************\n");
      m_fgcSEIPersistenceFlag = true;
    }
    else if (m_fgcSEIPerPictureSEI && m_fgcSEIPersistenceFlag) {
      fprintf(stderr, "*************************************************************************\n");
      fprintf(stderr, "* WARNING: SEIPerPictureSEI is set to 1, SEIPersistenceFlag needs to be set to 0! *\n");
      fprintf(stderr, "*************************************************************************\n");
      m_fgcSEIPersistenceFlag = false;
    }

    UInt numModelCtr;
    for (UInt c = 0; c <= 2; c++)
    {
      if (m_fgcSEICompModelPresent[c])
      {
        numModelCtr = 0;
        for (UInt i = 0; i <= m_fgcSEINumIntensityIntervalMinus1[c]; i++)
        {
          m_fgcSEIIntensityIntervalLowerBound[c][i] = UChar((cfg_FgcSEIIntensityIntervalLowerBoundComp[c].values.size() > i) ? cfg_FgcSEIIntensityIntervalLowerBoundComp[c].values[i] : 0);
          m_fgcSEIIntensityIntervalUpperBound[c][i] = UChar((cfg_FgcSEIIntensityIntervalUpperBoundComp[c].values.size() > i) ? cfg_FgcSEIIntensityIntervalUpperBoundComp[c].values[i] : 0);
          for (UInt j = 0; j <= m_fgcSEINumModelValuesMinus1[c]; j++)
          {
            m_fgcSEICompModelValue[c][i][j] = UInt((cfg_FgcSEICompModelValueComp[c].values.size() > numModelCtr) ? cfg_FgcSEICompModelValueComp[c].values[numModelCtr] : 0);
            numModelCtr++;
          }
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
}

//! \}
#endif