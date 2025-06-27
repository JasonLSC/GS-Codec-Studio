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

/** \file     EncAppCfg.cpp
    \brief    Handle encoder configuration parameters
*/

#include "EncAppCfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <string>
#include <fstream>
#include <limits>

#include "Utilities/program_options_lite.h"
#include "Utilities/VideoIOYuv.h"
#include "CommonLib/Rom.h"
#include "EncoderLib/RateCtrl.h"

#include "CommonLib/dtrace_next.h"
#include "CommonLib/ProfileTierLevel.h"

#define MACRO_TO_STRING_HELPER(val) #val
#define MACRO_TO_STRING(val) MACRO_TO_STRING_HELPER(val)

namespace po = ProgramOptionsLite;

enum ExtendedProfileName   // this is used for determining profile strings, where multiple profiles map to a single
                           // profile idc with various constraint flag combinations
{
  NONE,
  MAIN_10,
  MAIN_10_STILL_PICTURE,
  MAIN_10_444,
  MAIN_10_444_STILL_PICTURE,
  MULTILAYER_MAIN_10,
  MULTILAYER_MAIN_10_STILL_PICTURE,
  MULTILAYER_MAIN_10_444,
  MULTILAYER_MAIN_10_444_STILL_PICTURE,
  MAIN_12,
  MAIN_12_444,
  MAIN_16_444,
  MAIN_12_INTRA,
  MAIN_12_444_INTRA,
  MAIN_16_444_INTRA,
  MAIN_12_STILL_PICTURE,
  MAIN_12_444_STILL_PICTURE,
  MAIN_16_444_STILL_PICTURE,
  AUTO = -1
};

constexpr int TF_DEFAULT_REFS = 4;

//! \ingroup EncoderApp
//! \{

// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================

EncAppCfg::EncAppCfg()
: m_inputColourSpaceConvert(IPCOLOURSPACE_UNCHANGED)
, m_snrInternalColourSpace(false)
, m_outputInternalColourSpace(false)
, m_packedYUVMode(false)
#if EXTENSION_360_VIDEO
, m_ext360(*this)
#endif
{
}

EncAppCfg::~EncAppCfg()
{
#if ENABLE_TRACING
  tracing_uninit(g_trace_ctx);
  g_trace_ctx = nullptr;
#endif
}

void EncAppCfg::create()
{
}

void EncAppCfg::destroy()
{
}

std::istringstream &operator>>(std::istringstream &in, GOPEntry &entry)     //input
{
  in>>entry.m_sliceType;
  in>>entry.m_POC;
  in>>entry.m_QPOffset;
  in>>entry.m_QPOffsetModelOffset;
  in>>entry.m_QPOffsetModelScale;
#if W0038_CQP_ADJ
  in>>entry.m_CbQPoffset;
  in>>entry.m_CrQPoffset;
#endif
  in>>entry.m_QPFactor;
  in>>entry.m_tcOffsetDiv2;
  in>>entry.m_betaOffsetDiv2;
  in>>entry.m_CbTcOffsetDiv2;
  in>>entry.m_CbBetaOffsetDiv2;
  in>>entry.m_CrTcOffsetDiv2;
  in>>entry.m_CrBetaOffsetDiv2;
  in>>entry.m_temporalId;
  in >> entry.m_numRefPicsActive0;
  in >> entry.m_numRefPics0;

  char c;
  for (int i = 0; i < entry.m_numRefPics0; i++)
  {
    in >> entry.m_deltaRefPics0[i];
    if (in.rdbuf()->in_avail())
    {
      c = in.get();
      if (c == '.')
      {
        in >> entry.m_layerRef0[i];
      }
      else
      {
        in.unget();
      }
    }
  }
  in >> entry.m_numRefPicsActive1;
  in >> entry.m_numRefPics1;

  for (int i = 0; i < entry.m_numRefPics1; i++)
  {
    in >> entry.m_deltaRefPics1[i];
    if (in.rdbuf()->in_avail())
    {
      c = in.get();
      if (c == '.')
      {
        in >> entry.m_layerRef1[i];
      }
      else
      {
        in.unget();
      }
    }
  }

  return in;
}



bool confirmPara(bool bflag, const char* message);

static inline ChromaFormat numberToChromaFormat(const int val)
{
  switch (val)
  {
  case 400:
    return ChromaFormat::_400;
    break;
  case 420:
    return ChromaFormat::_420;
    break;
  case 422:
    return ChromaFormat::_422;
    break;
  case 444:
    return ChromaFormat::_444;
    break;
  default:
    return ChromaFormat::UNDEFINED;
  }
}

static const struct MapStrToProfile
{
  const char* str;
  Profile::Name value;
} strToProfile[] = {
  { "none", Profile::NONE },
  { "main_10", Profile::MAIN_10 },
  { "main_10_444", Profile::MAIN_10_444 },
  { "main_10_still_picture", Profile::MAIN_10_STILL_PICTURE },
  { "main_10_444_still_picture", Profile::MAIN_10_444_STILL_PICTURE },
  { "multilayer_main_10", Profile::MULTILAYER_MAIN_10 },
  { "multilayer_main_10_444", Profile::MULTILAYER_MAIN_10_444 },
  { "multilayer_main_10_still_picture", Profile::MULTILAYER_MAIN_10_STILL_PICTURE },
  { "multilayer_main_10_444_still_picture", Profile::MULTILAYER_MAIN_10_444_STILL_PICTURE },
  { "main_12", Profile::MAIN_12 },
  { "main_12_444", Profile::MAIN_12_444 },
  { "main_16_444", Profile::MAIN_16_444 },
  { "main_12_intra", Profile::MAIN_12_INTRA },
  { "main_12_444_intra", Profile::MAIN_12_444_INTRA },
  { "main_16_444_intra", Profile::MAIN_16_444_INTRA },
  { "main_12_still_picture", Profile::MAIN_12_STILL_PICTURE },
  { "main_12_444_still_picture", Profile::MAIN_12_444_STILL_PICTURE },
  { "main_16_444_still_picture", Profile::MAIN_16_444_STILL_PICTURE },
};

static const struct MapStrToExtendedProfile
{
  const char* str;
  ExtendedProfileName value;
} strToExtendedProfile[] = {
  { "none", NONE },
  { "main_10", MAIN_10 },
  { "main_10_444", MAIN_10_444 },
  { "main_10_still_picture", MAIN_10_STILL_PICTURE },
  { "main_10_444_still_picture", MAIN_10_444_STILL_PICTURE },
  { "multilayer_main_10", MULTILAYER_MAIN_10 },
  { "multilayer_main_10_444", MULTILAYER_MAIN_10_444 },
  { "multilayer_main_10_still_picture", MULTILAYER_MAIN_10_STILL_PICTURE },
  { "multilayer_main_10_444_still_picture", MULTILAYER_MAIN_10_444_STILL_PICTURE },
  { "main_12", MAIN_12 },
  { "main_12_444", MAIN_12_444 },
  { "main_16_444", MAIN_16_444 },
  { "main_12_intra", MAIN_12_INTRA },
  { "main_12_444_intra", MAIN_12_444_INTRA },
  { "main_16_444_intra", MAIN_16_444_INTRA },
  { "main_12_still_picture", MAIN_12_STILL_PICTURE },
  { "main_12_444_still_picture", MAIN_12_444_STILL_PICTURE },
  { "main_16_444_still_picture", MAIN_16_444_STILL_PICTURE },
  { "auto", AUTO },
};

static const struct MapStrToTier
{
  const char* str;
  Level::Tier value;
}
strToTier[] =
{
  {"main", Level::MAIN},
  {"high", Level::HIGH},
};

static const struct MapStrToLevel
{
  const char* str;
  Level::Name value;
}
strToLevel[] =
{
  {"none",Level::NONE},
  {"1",   Level::LEVEL1},
  {"2",   Level::LEVEL2},
  {"2.1", Level::LEVEL2_1},
  {"3",   Level::LEVEL3},
  {"3.1", Level::LEVEL3_1},
  {"4",   Level::LEVEL4},
  {"4.1", Level::LEVEL4_1},
  {"5",   Level::LEVEL5},
  {"5.1", Level::LEVEL5_1},
  {"5.2", Level::LEVEL5_2},
  {"6",   Level::LEVEL6},
  {"6.1", Level::LEVEL6_1},
  {"6.2", Level::LEVEL6_2},
  {"6.3", Level::LEVEL6_3},
  {"15.5", Level::LEVEL15_5},
};

uint32_t g_uiMaxCpbSize[2][28] =
{
  //            LEVEL1,          LEVEL2,  LEVEL2_1,      LEVEL3,  LEVEL3_1,       LEVEL4,   LEVEL4_1,       LEVEL5,    LEVEL5_1,  LEVEL5_2,     LEVEL6,    LEVEL6_1,  LEVEL6_2   LEVEL6_3
  { 0, 0, 0, 0, 350000, 0, 0, 0, 1500000, 3000000, 0, 0, 6000000, 10000000, 0, 0, 12000000, 20000000, 0, 0,  25000000,  40000000,  60000000, 0,  80000000, 120000000, 240000000,  240000000 },
  { 0, 0, 0, 0,      0, 0, 0, 0,       0,       0, 0, 0,       0,        0, 0, 0, 30000000, 50000000, 0, 0, 100000000, 160000000, 240000000, 0, 240000000, 480000000, 800000000, 1600000000 }
};

static const struct MapStrToCostMode
{
  const char* str;
  CostMode    value;
}
strToCostMode[] =
{
  {"lossy",                     COST_STANDARD_LOSSY},
  {"sequence_level_lossless",   COST_SEQUENCE_LEVEL_LOSSLESS},
  {"lossless",                  COST_LOSSLESS_CODING},
  {"mixed_lossless_lossy",      COST_MIXED_LOSSLESS_LOSSY_CODING}
};

static const struct MapStrToScalingListMode
{
  const char* str;
  ScalingListMode value;
}
strToScalingListMode[] =
{
  {"0",       SCALING_LIST_OFF},
  {"1",       SCALING_LIST_DEFAULT},
  {"2",       SCALING_LIST_FILE_READ},
  {"off",     SCALING_LIST_OFF},
  {"default", SCALING_LIST_DEFAULT},
  {"file",    SCALING_LIST_FILE_READ}
};

template<typename T, typename P>
static std::string enumToString(P map[], uint32_t mapLen, const T val)
{
  for (uint32_t i = 0; i < mapLen; i++)
  {
    if (val == map[i].value)
    {
      return map[i].str;
    }
  }
  return std::string();
}

template<typename T, typename P> static std::istream &readStrToEnum(P map[], uint32_t mapLen, std::istream &in, T &val)
{
  std::string str;
  in >> str;

  for (uint32_t i = 0; i < mapLen; i++)
  {
    if (str == map[i].str)
    {
      val = map[i].value;
      goto found;
    }
  }
  /* not found */
  in.setstate(std::ios::failbit);
found:
  return in;
}

//inline to prevent compiler warnings for "unused static function"

static inline std::istream &operator>>(std::istream &in, ExtendedProfileName &profile)
{
  return readStrToEnum(strToExtendedProfile, sizeof(strToExtendedProfile)/sizeof(*strToExtendedProfile), in, profile);
}

namespace Level
{
  static inline std::istream &operator>>(std::istream &in, Tier &tier)
  {
    return readStrToEnum(strToTier, sizeof(strToTier)/sizeof(*strToTier), in, tier);
  }

  static inline std::istream &operator>>(std::istream &in, Name &level)
  {
    return readStrToEnum(strToLevel, sizeof(strToLevel)/sizeof(*strToLevel), in, level);
  }
}

static inline std::istream &operator>>(std::istream &in, CostMode &mode)
{
  return readStrToEnum(strToCostMode, sizeof(strToCostMode)/sizeof(*strToCostMode), in, mode);
}

static inline std::istream &operator>>(std::istream &in, ScalingListMode &mode)
{
  return readStrToEnum(strToScalingListMode, sizeof(strToScalingListMode)/sizeof(*strToScalingListMode), in, mode);
}

template<class T> static inline std::istream &operator>>(std::istream &in, SMultiValueInput<T> &values)
{
  return values.readValues(in);
}

template <class T>
T SMultiValueInput<T>::readValue(const char *&pStr, bool &bSuccess)
{
  T val=T();
  std::string s(pStr);
  std::replace(s.begin(), s.end(), ',', ' '); // make comma separated into space separated
  std::istringstream iss(s);
  iss>>val;
  bSuccess=!iss.fail() // check nothing has gone wrong
                       && !(val<minValIncl || val>maxValIncl) // check value is within range
                       && (int)iss.tellg() !=  0 // check we've actually read something
                       && (iss.eof() || iss.peek()==' '); // check next character is a space, or eof
  pStr+= (iss.eof() ? s.size() : (std::size_t)iss.tellg());
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
    const char *pStr=str.c_str();
    // soak up any whitespace
    for(;isspace(*pStr);pStr++);

    while (*pStr != 0)
    {
      bool bSuccess=true;
      T val=readValue(pStr, bSuccess);
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
      for(;isspace(*pStr);pStr++);
      if (*pStr == ',')
      {
        pStr++;
      }
      for(;isspace(*pStr);pStr++);
    }
  }
  if (values.size() < minNumValuesIncl)
  {
    in.setstate(std::ios::failbit);
  }
  return in;
}

template<class T1, class T2> static inline std::istream &operator>>(std::istream &in, std::map<T1, T2> &map)
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
    in.setstate(std::ios::failbit);
  }

  map[key] = value;
  return in;
}



static uint32_t getMaxTileColsByLevel( Level::Name level )
{
  switch( level )
  {
    case Level::LEVEL1:
    case Level::LEVEL2:
    case Level::LEVEL2_1:
      return 1;
    case Level::LEVEL3:
      return 2;
    case Level::LEVEL3_1:
      return 3;
    case Level::LEVEL4:
    case Level::LEVEL4_1:
      return 5;
    case Level::LEVEL5:
    case Level::LEVEL5_1:
    case Level::LEVEL5_2:
      return 10;
    case Level::LEVEL6:
    case Level::LEVEL6_1:
    case Level::LEVEL6_2:
      return 20;
    case Level::LEVEL6_3:
      return 30;
    default:
      return MAX_TILE_COLS;
  }
}

static uint32_t getMaxTileRowsByLevel( Level::Name level )
{
  switch( level )
  {
    case Level::LEVEL1:
    case Level::LEVEL2:
    case Level::LEVEL2_1:
      return 1;
    case Level::LEVEL3:
      return 2;
    case Level::LEVEL3_1:
      return 3;
    case Level::LEVEL4:
    case Level::LEVEL4_1:
      return 5;
    case Level::LEVEL5:
    case Level::LEVEL5_1:
    case Level::LEVEL5_2:
      return 11;
    case Level::LEVEL6:
    case Level::LEVEL6_1:
    case Level::LEVEL6_2:
      return 22;
    case Level::LEVEL6_3:
      return 33;
    default:
      return MAX_TILES / MAX_TILE_COLS;
  }
}

static uint32_t getMaxSlicesByLevel( Level::Name level )
{
  switch( level )
  {
    case Level::LEVEL1:
    case Level::LEVEL2:
      return 16;
    case Level::LEVEL2_1:
      return 20;
    case Level::LEVEL3:
      return 30;
    case Level::LEVEL3_1:
      return 40;
    case Level::LEVEL4:
    case Level::LEVEL4_1:
      return 75;
    case Level::LEVEL5:
    case Level::LEVEL5_1:
    case Level::LEVEL5_2:
      return 200;
    case Level::LEVEL6:
    case Level::LEVEL6_1:
    case Level::LEVEL6_2:
      return 600;
    case Level::LEVEL6_3:
      return 1000;
    default:
      return MAX_SLICES;
  }
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/** \param  argc        number of arguments
    \param  argv        array of arguments
    \retval             true when success
 */
#ifdef _MSC_VER
// Disable optimizations to avoid long compile times
#pragma optimize( "", off )
#endif
bool EncAppCfg::parseCfg( int argc, char* argv[] )
{
  bool do_help = false;

  int tmpChromaFormat;
  int tmpInputChromaFormat;
  int tmpConstraintChromaFormat;
  int tmpMaxChromaFormatConstraintIdc;
  int tmpWeightedPredictionMethod;
  int tmpFastInterSearchMode;
  int tmpMotionEstimationSearchMethod;
  int tmpDecodedPictureHashSEIMappedType;
  int tmpSubpicDecodedPictureHashMappedType;

  std::string         inputColourSpaceConvert;
  std::string         inputPathPrefix;
  ExtendedProfileName extendedProfile;

  // Multi-value input fields:                                // minval, maxval (incl), min_entries, max_entries (incl) [, default values, number of default values]
  SMultiValueInput<uint32_t>  cfgTileColumnWidth              (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>  cfgTileRowHeight                (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>  cfgRectSlicePos                 (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>  cfgRasterSliceSize              (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<int>  cfg_startOfCodedInterval            (std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0, 1<<16);
  SMultiValueInput<int>  cfg_codedPivotValue                 (std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0, 1<<16);
  SMultiValueInput<int>  cfg_targetPivotValue                (std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0, 1<<16);


  SMultiValueInput<double> cfg_adIntraLambdaModifier         (0, std::numeric_limits<double>::max(), 0, MAX_TLAYER); ///< Lambda modifier for Intra pictures, one for each temporal layer. If size>temporalLayer, then use [temporalLayer], else if size>0, use [size()-1], else use m_adLambdaModifier.
  SMultiValueInput<uint16_t>  cfgSliceLosslessArray          (0, std::numeric_limits<uint16_t>::max(), 0, MAX_SLICES);
#if SHARP_LUMA_DELTA_QP
  const int defaultLumaLevelTodQp_QpChangePoints[]   =  {-3,  -2,  -1,   0,   1,   2,   3,   4,   5,   6};
  const int defaultLumaLevelTodQp_LumaChangePoints[] =  { 0, 301, 367, 434, 501, 567, 634, 701, 767, 834};
  SMultiValueInput<int>  cfg_lumaLeveltoDQPMappingQP         (-MAX_QP, MAX_QP,                    0, LUMA_LEVEL_TO_DQP_LUT_MAXSIZE, defaultLumaLevelTodQp_QpChangePoints,   sizeof(defaultLumaLevelTodQp_QpChangePoints  )/sizeof(int));
  SMultiValueInput<int>  cfg_lumaLeveltoDQPMappingLuma       (0, std::numeric_limits<int>::max(), 0, LUMA_LEVEL_TO_DQP_LUT_MAXSIZE, defaultLumaLevelTodQp_LumaChangePoints, sizeof(defaultLumaLevelTodQp_LumaChangePoints)/sizeof(int));
  uint32_t lumaLevelToDeltaQPMode;
#endif
  const int qpInVals[] = { 25, 33, 43 };                // qpInVal values used to derive the chroma QP mapping table used in VTM-5.0
  const int qpOutVals[] = { 25, 32, 37 };               // qpOutVal values used to derive the chroma QP mapping table used in VTM-5.0
  SMultiValueInput<int> cfg_qpInValCb                   (MIN_QP_VALUE_FOR_16_BIT, MAX_QP, 0, MAX_NUM_QP_VALUES, qpInVals, sizeof(qpInVals)/sizeof(int));
  SMultiValueInput<int> cfg_qpOutValCb                  (MIN_QP_VALUE_FOR_16_BIT, MAX_QP, 0, MAX_NUM_QP_VALUES, qpOutVals, sizeof(qpOutVals) / sizeof(int));
  const int zeroVector[] = { 0 };
  SMultiValueInput<int> cfg_qpInValCr                   (MIN_QP_VALUE_FOR_16_BIT, MAX_QP, 0, MAX_NUM_QP_VALUES, zeroVector, 1);
  SMultiValueInput<int> cfg_qpOutValCr                  (MIN_QP_VALUE_FOR_16_BIT, MAX_QP, 0, MAX_NUM_QP_VALUES, zeroVector, 1);
  SMultiValueInput<int> cfg_qpInValCbCr                 (MIN_QP_VALUE_FOR_16_BIT, MAX_QP, 0, MAX_NUM_QP_VALUES, zeroVector, 1);
  SMultiValueInput<int> cfg_qpOutValCbCr                (MIN_QP_VALUE_FOR_16_BIT, MAX_QP, 0, MAX_NUM_QP_VALUES, zeroVector, 1);
  const int cQpOffsets[] = { 6 };
  SMultiValueInput<int> cfg_cbQpOffsetList              (-12, 12, 0, 6, cQpOffsets, 0);
  SMultiValueInput<int> cfg_crQpOffsetList              (-12, 12, 0, 6, cQpOffsets, 0);
  SMultiValueInput<int> cfg_cbCrQpOffsetList            (-12, 12, 0, 6, cQpOffsets, 0);

  const uint32_t defaultInputKneeCodes[3]  = { 600, 800, 900 };
  const uint32_t defaultOutputKneeCodes[3] = { 100, 250, 450 };
  SMultiValueInput<uint32_t> cfg_kneeSEIInputKneePointValue      (1,  999, 0, 999, defaultInputKneeCodes,  sizeof(defaultInputKneeCodes )/sizeof(uint32_t));
  SMultiValueInput<uint32_t> cfg_kneeSEIOutputKneePointValue     (0, 1000, 0, 999, defaultOutputKneeCodes, sizeof(defaultOutputKneeCodes)/sizeof(uint32_t));
  const int defaultPrimaryCodes[6]     = { 0,50000, 0,0, 50000,0 };
  const int defaultWhitePointCode[2]   = { 16667, 16667 };

  SMultiValueInput<int>  cfg_DisplayPrimariesCode            (0, 50000, 6, 6, defaultPrimaryCodes,   sizeof(defaultPrimaryCodes  )/sizeof(int));
  SMultiValueInput<int>  cfg_DisplayWhitePointCode           (0, 50000, 2, 2, defaultWhitePointCode, sizeof(defaultWhitePointCode)/sizeof(int));

#if RExt__HIGH_BIT_DEPTH_SUPPORT
  SMultiValueInput<Pel>  cfg_SEICTILut0(0, ((1 << (2 + 16 - 1)) - 1), 0, MAX_CTI_LUT_SIZE + 1);
  SMultiValueInput<Pel>  cfg_SEICTILut1(0, ((1 << (2 + 16 - 1)) - 1), 0, MAX_CTI_LUT_SIZE + 1);
  SMultiValueInput<Pel>  cfg_SEICTILut2(0, ((1 << (2 + 16 - 1)) - 1), 0, MAX_CTI_LUT_SIZE + 1);
#else
  SMultiValueInput<Pel>  cfg_SEICTILut0(0, ((1 << (2 + 12 - 1)) - 1), 0, MAX_CTI_LUT_SIZE + 1);
  SMultiValueInput<Pel>  cfg_SEICTILut1(0, ((1 << (2 + 12 - 1)) - 1), 0, MAX_CTI_LUT_SIZE + 1);
  SMultiValueInput<Pel>  cfg_SEICTILut2(0, ((1 << (2 + 12 - 1)) - 1), 0, MAX_CTI_LUT_SIZE + 1);
#endif
  SMultiValueInput<bool> cfg_timeCodeSeiTimeStampFlag        (0,  1, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<bool> cfg_timeCodeSeiNumUnitFieldBasedFlag(0,  1, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<int>  cfg_timeCodeSeiCountingType         (0,  6, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<bool> cfg_timeCodeSeiFullTimeStampFlag    (0,  1, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<bool> cfg_timeCodeSeiDiscontinuityFlag    (0,  1, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<bool> cfg_timeCodeSeiCntDroppedFlag       (0,  1, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<int>  cfg_timeCodeSeiNumberOfFrames       (0,511, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<int>  cfg_timeCodeSeiSecondsValue         (0, 59, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<int>  cfg_timeCodeSeiMinutesValue         (0, 59, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<int>  cfg_timeCodeSeiHoursValue           (0, 23, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<bool> cfg_timeCodeSeiSecondsFlag          (0,  1, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<bool> cfg_timeCodeSeiMinutesFlag          (0,  1, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<bool> cfg_timeCodeSeiHoursFlag            (0,  1, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<int>  cfg_timeCodeSeiTimeOffsetLength     (0, 31, 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<int>  cfg_timeCodeSeiTimeOffsetValue      (std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0, MAX_TIMECODE_SEI_SETS);
  SMultiValueInput<int>      cfg_omniViewportSEIAzimuthCentre    (-11796480, 11796479, 0, 15);
  SMultiValueInput<int>      cfg_omniViewportSEIElevationCentre  ( -5898240,  5898240, 0, 15);
  SMultiValueInput<int>      cfg_omniViewportSEITiltCentre       (-11796480, 11796479, 0, 15);
  SMultiValueInput<uint32_t> cfg_omniViewportSEIHorRange         (        1, 23592960, 0, 15);
  SMultiValueInput<uint32_t> cfg_omniViewportSEIVerRange         (        1, 11796480, 0, 15);
  SMultiValueInput<uint32_t>   cfg_rwpSEIRwpTransformType                 (0, 7, 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<bool>       cfg_rwpSEIRwpGuardBandFlag                 (0, 1, 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIProjRegionWidth                  (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIProjRegionHeight                 (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIRwpSEIProjRegionTop              (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIProjRegionLeft                   (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIPackedRegionWidth                (0, std::numeric_limits<uint16_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIPackedRegionHeight               (0, std::numeric_limits<uint16_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIPackedRegionTop                  (0, std::numeric_limits<uint16_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIPackedRegionLeft                 (0, std::numeric_limits<uint16_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIRwpLeftGuardBandWidth            (0, std::numeric_limits<uint8_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIRwpRightGuardBandWidth           (0, std::numeric_limits<uint8_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIRwpTopGuardBandHeight            (0, std::numeric_limits<uint8_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIRwpBottomGuardBandHeight         (0, std::numeric_limits<uint8_t>::max(), 0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<bool>       cfg_rwpSEIRwpGuardBandNotUsedForPredFlag   (0, 1,   0, std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_rwpSEIRwpGuardBandType                 (0, 7,   0, 4*std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>   cfg_gcmpSEIFaceIndex                  (0, 5, 5, 6);
  SMultiValueInput<uint32_t>   cfg_gcmpSEIFaceRotation               (0, 3, 5, 6);
  SMultiValueInput<double>     cfg_gcmpSEIFunctionCoeffU             (0.0, 1.0, 5, 6);
  SMultiValueInput<uint32_t>   cfg_gcmpSEIFunctionUAffectedByVFlag   (0, 1, 5, 6);
  SMultiValueInput<double>     cfg_gcmpSEIFunctionCoeffV             (0.0, 1.0, 5, 6);
  SMultiValueInput<uint32_t>   cfg_gcmpSEIFunctionVAffectedByUFlag   (0, 1, 5, 6);
  SMultiValueInput<uint32_t>        cfg_sdiSEILayerId                  (0, 63, 0, 63);
  SMultiValueInput<uint32_t>        cfg_sdiSEIViewIdVal                (0, 63, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_sdiSEIAuxId                    (0, 255, 0, 63);
  SMultiValueInput<uint32_t>        cfg_sdiSEINumAssociatedPrimaryLayersMinus1 (0, 63, 0, 63);
  SMultiValueInput<uint32_t>        cfg_sdiSEIAssociatedPrimaryLayerIdx(0, 63, 0, 63);
  SMultiValueInput<bool>            cfg_maiSEISignFocalLengthX         (0, 1,   0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_maiSEIExponentFocalLengthX     (0, 63, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_maiSEIMantissaFocalLengthX     (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<bool>            cfg_maiSEISignFocalLengthY         (0, 1,   0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_maiSEIExponentFocalLengthY     (0, 63, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_maiSEIMantissaFocalLengthY     (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<bool>            cfg_maiSEISignPrincipalPointX      (0, 1,   0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_maiSEIExponentPrincipalPointX  (0, 63, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_maiSEIMantissaPrincipalPointX  (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<bool>            cfg_maiSEISignPrincipalPointY      (0, 1,   0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_maiSEIExponentPrincipalPointY  (0, 63, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_maiSEIMantissaPrincipalPointY  (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<bool>            cfg_maiSEISignSkewFactor           (0, 1,   0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_maiSEIExponentSkewFactor       (0, 63, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_maiSEIMantissaSkewFactor       (0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t>        cfg_mvpSEIViewPosition             (0, 63, 0, std::numeric_limits<uint32_t>::max());

  SMultiValueInput<uint32_t>        cfg_driSEINonlinearModel           (0, 31, 0, std::numeric_limits<uint32_t>::max());

  const int defaultLadfQpOffset[3] = { 1, 0, 1 };
  const int defaultLadfIntervalLowerBound[2] = { 350, 833 };

  SMultiValueInput<int> cfg_ladfQpOffset(-MAX_QP, MAX_QP, 2, MAX_LADF_INTERVALS, defaultLadfQpOffset, 3);
  SMultiValueInput<int> cfg_ladfIntervalLowerBound(0, std::numeric_limits<int>::max(), 1, MAX_LADF_INTERVALS - 1,
                                                   defaultLadfIntervalLowerBound, 2);

  SMultiValueInput<unsigned> cfg_virtualBoundariesPosX       (0, std::numeric_limits<uint32_t>::max(), 0, 3);
  SMultiValueInput<unsigned> cfg_virtualBoundariesPosY       (0, std::numeric_limits<uint32_t>::max(), 0, 3);
  const int defaultRprSwitchingResolutionOrderList[12] = { 1, 0, 2, 0, 3, 0, 1, 0, 2, 0, 3, 0 };
  const int defaultRprSwitchingQPOffsetOrderList[12] = { -2, 0, -4, 0, -6, 0, -2, 0, -4, 0, -6, 0 };
  SMultiValueInput<int>  cfg_rprSwitchingResolutionOrderList(0, 3, 0, MAX_RPR_SWITCHING_ORDER_LIST_SIZE, defaultRprSwitchingResolutionOrderList, 12);
  SMultiValueInput<int>  cfg_rprSwitchingQPOffsetOrderList(-MAX_QP, MAX_QP, 0, MAX_RPR_SWITCHING_ORDER_LIST_SIZE, defaultRprSwitchingQPOffsetOrderList, 12);
  SMultiValueInput<uint32_t>  cfg_SubProfile(0, std::numeric_limits<uint8_t>::max(), 0,
                                            std::numeric_limits<uint8_t>::max());
  SMultiValueInput<uint32_t>  cfg_subPicCtuTopLeftX(0, std::numeric_limits<uint32_t>::max(), 0, MAX_NUM_SUB_PICS);
  SMultiValueInput<uint32_t>  cfg_subPicCtuTopLeftY(0, std::numeric_limits<uint32_t>::max(), 0, MAX_NUM_SUB_PICS);
  SMultiValueInput<uint32_t>  cfg_subPicWidth(1, std::numeric_limits<uint32_t>::max(), 0, MAX_NUM_SUB_PICS);
  SMultiValueInput<uint32_t>  cfg_subPicHeight(1, std::numeric_limits<uint32_t>::max(), 0, MAX_NUM_SUB_PICS);
  SMultiValueInput<bool>      cfg_subPicTreatedAsPicFlag(0, 1, 0, MAX_NUM_SUB_PICS);
  SMultiValueInput<bool>      cfg_loopFilterAcrossSubpicEnabledFlag(0, 1, 0, MAX_NUM_SUB_PICS);
  SMultiValueInput<uint32_t>  cfg_subPicId(0, std::numeric_limits<uint16_t>::max(), 0, MAX_NUM_SUB_PICS);

  SMultiValueInput<int>       cfg_sliFractions(0, 255, 0, std::numeric_limits<int>::max());
  SMultiValueInput<int>       cfg_sliNonSubpicLayersFractions(0, 255, 0, std::numeric_limits<int>::max());

  SMultiValueInput<Level::Name>  cfg_sliRefLevels(Level::NONE, Level::LEVEL15_5, 0, 8 * MAX_VPS_SUBLAYERS);

  int warnUnknowParameter = 0;

  SMultiValueInput<uint32_t>   cfg_FgcSEIIntensityIntervalLowerBoundComp0 (0, 255, 0, 256);
  SMultiValueInput<uint32_t>   cfg_FgcSEIIntensityIntervalLowerBoundComp1 (0, 255, 0, 256);
  SMultiValueInput<uint32_t>   cfg_FgcSEIIntensityIntervalLowerBoundComp2 (0, 255, 0, 256);
  SMultiValueInput<uint32_t>   cfg_FgcSEIIntensityIntervalUpperBoundComp0 (0, 255, 0, 256);
  SMultiValueInput<uint32_t>   cfg_FgcSEIIntensityIntervalUpperBoundComp1 (0, 255, 0, 256);
  SMultiValueInput<uint32_t>   cfg_FgcSEIIntensityIntervalUpperBoundComp2 (0, 255, 0, 256);
  SMultiValueInput<uint32_t>   cfg_FgcSEICompModelValueComp0              (0, 65535,  0, 256 * 6);
  SMultiValueInput<uint32_t>   cfg_FgcSEICompModelValueComp1              (0, 65535,  0, 256 * 6);
  SMultiValueInput<uint32_t>   cfg_FgcSEICompModelValueComp2              (0, 65535,  0, 256 * 6);
  SMultiValueInput<unsigned>   cfg_siiSEIInputNumUnitsInSI(0, std::numeric_limits<uint32_t>::max(), 0, 7);
  SMultiValueInput<bool>       cfg_poSEIWrappingFlag(false, true, 0, 256);
  SMultiValueInput<uint16_t>   cfg_poSEIImportanceIdc(0, 3, 0, 256);
  SMultiValueInput<bool>       cfg_poSEIPrefixFlag(false, true, 0, 256);
  SMultiValueInput<uint16_t>   cfg_poSEIPayloadType(0, 32768, 0, 256 * 2);
  SMultiValueInput<uint16_t>   cfg_poSEIProcessingOrder(0, 65535, 0, 65536);

  SMultiValueInput<uint16_t>   cfg_poSEINumofPrefixBits(0, 255, 0, 256);
  SMultiValueInput<uint16_t>   cfg_poSEIPrefixByte     (0, 255, 0, 256);

  SMultiValueInput<int32_t> cfg_postFilterHintSEIValues(INT32_MIN + 1, INT32_MAX, 1 * 1 * 1, 15 * 15 * 3);

  std::vector<SMultiValueInput<uint32_t>>   cfg_nnPostFilterSEICharacteristicsInterpolatedPicturesList;
  std::vector<SMultiValueInput<bool>>   cfg_nnPostFilterSEICharacteristicsInputPicOutputFlagList;
  for (int i = 0; i < MAX_NUM_NN_POST_FILTERS; i++)
  {
    cfg_nnPostFilterSEICharacteristicsInterpolatedPicturesList.push_back(SMultiValueInput<uint32_t>(0, std::numeric_limits<uint32_t>::max(), 1, 0));
    cfg_nnPostFilterSEICharacteristicsInputPicOutputFlagList.push_back(SMultiValueInput<bool>(0, 1, 1, 0));
  }
  SMultiValueInput<bool>       cfg_nnPostFilterSEIActivationOutputFlagList(0, 1, 1, 0);

  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIId                                  (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEICnt                                 (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIDrivePicFusionFlag                  (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEILowConfidenceFaceParameterFlag      (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEICoordinatePresentFlag               (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEICoordinateQuantizationFactor        (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEICoordinatePredFlag                  (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEI3DCoordinateFlag                    (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEICoordinatePointNum                  (0, 2550, 0, 102400);
  SMultiValueInput<double>     cfg_generativeFaceVideoSEICoordinateXTesonr                   (-1.0, 1.0, 0, 102400);
  SMultiValueInput<double>     cfg_generativeFaceVideoSEICoordinateYTesonr                   (-1.0, 1.0, 0, 10240);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIZCoordinateMaxValue                 (0, 10240, 0, 10240);
  SMultiValueInput<double>     cfg_generativeFaceVideoSEICoordinateZTesonr                   (-10240.0, 10240.0, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIMatrixPresentFlag                   (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIMatrixElementPrecisionFactor        (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIMatrixPredFlag                      (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEINumMatrixType                       (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIMatrixTypeIdx                       (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIMatrix3DSpaceFlag                   (0, 1, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEINumMatrices                         (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIMatrixWidth                         (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIMatrixHeight                        (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEINumMatricestoNumKpsFlag             (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEINumMatricesInfo                     (0, 2550, 0, 102400);
  SMultiValueInput<double>     cfg_generativeFaceVideoSEIMatrixElement                       (-50960.0, 50960.0, 0, 5095000);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIChromaKeyValuePresentFlag           (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIChromaKeyValue                      (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIChromaKeyThrPresentFlag             (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoSEIChromaKeyThrValue                   (0, 2550, 0, 102400);  

  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoEnhancementSEIId                                  (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoEnhancementSEIGFVId                               (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoEnhancementSEIGFVCnt                              (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoEnhancementSEIMatrixPresentFlag                   (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoEnhancementSEIMatrixPredFlag                      (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor        (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoEnhancementSEINumMatrices                         (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoEnhancementSEIMatrixWidth                         (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoEnhancementSEIMatrixHeight                        (0, 2550, 0, 102400);
  SMultiValueInput<double>     cfg_generativeFaceVideoEnhancementSEIMatrixElement                       (-50960.0, 50960.0, 0, 5095000);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoEnhancementSEIPupilPresentIdx                     (0, 2550, 0, 102400);
  SMultiValueInput<uint32_t>   cfg_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor      (0, 2550, 0, 102400);
  SMultiValueInput<double>     cfg_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX             (-50960.0, 50960.0, 0, 5095000);
  SMultiValueInput<double>     cfg_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY             (-50960.0, 50960.0, 0, 5095000);
  SMultiValueInput<double>     cfg_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX            (-50960.0, 50960.0, 0, 5095000);
  SMultiValueInput<double>     cfg_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY            (-50960.0, 50960.0, 0, 5095000);

#if JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
  SMultiValueInput<uint32_t>   cfg_aurSEIRestrictions                                        (0, 2, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<bool>       cfg_aurSEIContextPresentFlag                                  (0, 1, 0, 4096);
  SMultiValueInput<uint32_t>   cfg_aurSEIContext                                             (0, 15, 0, std::numeric_limits<uint32_t>::max());
#endif

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  SMultiValueInput<uint32_t> cfg_priSEIResamplingWidthNumMinus1(0, 65535, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t> cfg_priSEIResamplingWidthDenomMinus1(0, 65535, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t> cfg_priSEIResamplingHeightNumMinus1(0, std::numeric_limits<uint32_t>::max() - 1, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t> cfg_priSEIResamplingHeightDenomMinus1(0, std::numeric_limits<uint32_t>::max() - 1, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t> cfg_priSEIRegionId(0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t> cfg_priSEIRegionTopLeftInUnitsX(0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t> cfg_priSEIRegionTopLeftInUnitsY(0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t> cfg_priSEIRegionWidthInUnitsMinus1(0, std::numeric_limits<uint32_t>::max() - 1, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t> cfg_priSEIRegionHeightInUnitsMinus1(0, std::numeric_limits<uint32_t>::max() - 1, 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t> cfg_priSEIResamplingRatioIdx(0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
#if JVET_AL0324_AL0070_PRI_SEI
  SMultiValueInput<uint32_t> cfg_priSEITargetRegionTopLeftInUnitsX(0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t> cfg_priSEITargetRegionTopLeftInUnitsY(0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
#else
  SMultiValueInput<uint32_t> cfg_priSEITargetRegionTopLeftX(0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<uint32_t> cfg_priSEITargetRegionTopLeftY(0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
#endif
  SMultiValueInput<uint32_t> cfg_priSEIRegionLayerId(0, std::numeric_limits<uint32_t>::max(), 0, std::numeric_limits<uint32_t>::max());
  SMultiValueInput<bool> cfg_priSEIRegionIsALayerFlag(0, 1, 0, std::numeric_limits<uint32_t>::max());
#endif

#if GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464
  CHECK(GREEN_METADATA_SEI_ENABLED == 0, "GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464 cannot be enabled when GREEN_METADATA_SEI_ENABLED is not enabled.");
#endif

#if GREEN_METADATA_SEI_ENABLED
#if GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464
  SMultiValueInput<uint16_t> cfg_greenMetadataAMILayerId(0, 255, 0, 256);
  SMultiValueInput<uint16_t> cfg_greenMetadataAMIOlsNumber(0, 15, 0, 16);
  SMultiValueInput<uint16_t> cfg_greenMetadataAMIOlsId(0, 4, 0, 5);
  SMultiValueInput<uint16_t> cfg_greenMetadataAMIEnergyReductionRate(0, 31, 0, 32);
  SMultiValueInput<uint16_t> cfg_greenMetadataAMIVideoQualityMetricType(0, 7, 0, 8);
  SMultiValueInput<double>   cfg_greenMetadataAMIVideoQualityLevel(0, 1.0, 0, 65536);
  SMultiValueInput<uint16_t> cfg_greenMetadataAMIMaxValue(0, 255, 0, 256);
  SMultiValueInput<uint16_t> cfg_greenMetadataAMIAttenuationUseIdc(0, 15, 0, 16);
  SMultiValueInput<uint16_t> cfg_greenMetadataAMIAttenuationCompIdc(0, 15, 0, 16);
  SMultiValueInput<bool>     cfg_greenMetadataAMIPreprocessingFlag(false, true, 0, 2);
  SMultiValueInput<uint16_t> cfg_greenMetadataAMIPreprocessingTypeIdc(0, 3, 0, 4);
  SMultiValueInput<uint16_t> cfg_greenMetadataAMIPreprocessingScaleIdc(0, 255, 0, 256);
  SMultiValueInput<uint16_t> cfg_greenMetadataAMIBacklightScalingIdc(0, 15, 0, 16);
#endif
#endif

#if ENABLE_TRACING
  std::string sTracingRule;
  std::string sTracingFile;
  bool   bTracingChannelsList = false;
#endif
#if ENABLE_SIMD_OPT
  std::string ignore;
#endif
  std::string frameRate;

  int chromaSampleLocType;
  int chromaSampleLocTypeTopField;
  int chromaSampleLocTypeBottomField;

  // clang-format off
  po::Options opts;
  opts.addOptions()
  ("help",                                            do_help,                                          false, "this help text")
  ("c",    po::parseConfigFile, "configuration file name")
  ("WarnUnknowParameter,w",                           warnUnknowParameter,                                  0, "warn for unknown configuration parameters instead of failing")
#if ENABLE_SIMD_OPT
  ("SIMD",                                            ignore,                                      std::string(""), "SIMD extension to use (SCALAR, SSE41, SSE42, AVX, AVX2, AVX512), default: the highest supported extension\n")
#endif
  // File, I/O and source parameters
  ("InputFile,i",                                     m_inputFileName,                             std::string(""), "Original YUV input file name")
  ("InputPathPrefix,-ipp",                            inputPathPrefix,                             std::string(""), "pathname to prepend to input filename")
  ("BitstreamFile,b",                                 m_bitstreamFileName,                         std::string(""), "Bitstream output file name")
  ("ReconFile,o",                                     m_reconFileName,                             std::string(""), "Reconstructed YUV output file name")
  ("SEIShutterIntervalPreFilename,-sii",              m_shutterIntervalPreFileName, std::string(""), "File name of Pre-Filtering video. If empty, not output video\n")
  ("SourceWidth,-wdt",                                m_sourceWidth,                                       0, "Source picture width")
  ("SourceHeight,-hgt",                               m_sourceHeight,                                      0, "Source picture height")
  ("SourceScalingRatioHor",                           m_sourceScalingRatioHor,                           1.0, "Source picture  horizontal scaling ratio")
  ("SourceScalingRatioVer",                           m_sourceScalingRatioVer,                           1.0, "Source picture vertical scaling ratio")
  ("InputBitDepth",                                   m_inputBitDepth[ChannelType::LUMA],                   8, "Bit-depth of input file")
  ("OutputBitDepth",                                  m_outputBitDepth[ChannelType::LUMA],                  0, "Bit-depth of output file (default:InternalBitDepth)")
  ("MSBExtendedBitDepth",                             m_msbExtendedBitDepth[ChannelType::LUMA],             0, "bit depth of luma component after addition of MSBs of value 0 (used for synthesising High Dynamic Range source material). (default:InputBitDepth)")
  ("InternalBitDepth",                                m_internalBitDepth[ChannelType::LUMA],                0, "Bit-depth the codec operates at. (default: MSBExtendedBitDepth). If different to MSBExtendedBitDepth, source data will be converted")
  ("InputBitDepthC",                                  m_inputBitDepth[ChannelType::CHROMA],                 0, "As per InputBitDepth but for chroma component. (default:InputBitDepth)")
  ("OutputBitDepthC",                                 m_outputBitDepth[ChannelType::CHROMA],                0, "As per OutputBitDepth but for chroma component. (default: use luma output bit-depth)")
  ("MSBExtendedBitDepthC",                            m_msbExtendedBitDepth[ChannelType::CHROMA],           0, "As per MSBExtendedBitDepth but for chroma component. (default:MSBExtendedBitDepth)")
  ("ExtendedPrecision",                               m_extendedPrecisionProcessingFlag,                false, "Increased internal accuracies to support high bit depths (not valid in V1 profiles)")
  ("TSRCRicePresent",                                 m_tsrcRicePresentFlag,                            false, "Indicate that TSRC Rice information is present in slice header (not valid in V1 profiles)")
  ("ReverseLastSigCoeff",                             m_reverseLastSigCoeffEnabledFlag,                 false, "enable reverse last significant coefficient postion in RRC (not valid in V1 profiles)")
  ("HighPrecisionPredictionWeighting",                m_highPrecisionOffsetsEnabledFlag,                false, "Use high precision option for weighted prediction (not valid in V1 profiles)")
  ("InputColourSpaceConvert",                         inputColourSpaceConvert,                     std::string(""), "Colour space conversion to apply to input video. Permitted values are (empty string=UNCHANGED) " + getListOfColourSpaceConverts(true))
  ("SNRInternalColourSpace",                          m_snrInternalColourSpace,                         false, "If true, then no colour space conversion is applied prior to SNR, otherwise inverse of input is applied.")
  ("OutputInternalColourSpace",                       m_outputInternalColourSpace,                      false, "If true, then no colour space conversion is applied for reconstructed video, otherwise inverse of input is applied.")
  ("InputChromaFormat",                               tmpInputChromaFormat,                               420, "InputChromaFormatIDC")
  ("MSEBasedSequencePSNR",                            m_printMSEBasedSequencePSNR,                      false, "0 (default) emit sequence PSNR only as a linear average of the frame PSNRs, 1 = also emit a sequence PSNR based on an average of the frame MSEs")
  ("PrintHexPSNR",                                    m_printHexPsnr,                                   false, "0 (default) don't emit hexadecimal PSNR for each frame, 1 = also emit hexadecimal PSNR values")
  ("PrintFrameMSE",                                   m_printFrameMSE,                                  false, "0 (default) emit only bit count and PSNRs for each frame, 1 = also emit MSE values")
  ("PrintSequenceMSE",                                m_printSequenceMSE,                               false, "0 (default) emit only bit rate and PSNRs for the whole sequence, 1 = also emit MSE values")
  ("PrintMSSSIM",                                     m_printMSSSIM,                                    false, "0 (default) do not print MS-SSIM scores, 1 = print MS-SSIM scores for each frame and for the whole sequence")
  ("PrintWPSNR",                                      m_printWPSNR,                                     false, "0 (default) do not print HDR-PQ based wPSNR, 1 = print HDR-PQ based wPSNR")
  ("PrintHighPrecEncTime",                            m_printHighPrecEncTime,                           false, "0 (default): print integer value of encoding time in seconds, 1: print floating-point value of encoding time")
  ("CabacZeroWordPaddingEnabled",                     m_cabacZeroWordPaddingEnabled,                     true, "0 do not add conforming cabac-zero-words to bit streams, 1 (default) = add cabac-zero-words as required")
  ("ChromaFormatIDC,-cf",                             tmpChromaFormat,                                      0, "ChromaFormatIDC (400|420|422|444 or set 0 (default) for same as InputChromaFormat)")
  ("ConformanceWindowMode",                           m_conformanceWindowMode,                              1, "Window conformance mode (0: no window, 1:automatic padding (default), 2:padding parameters specified, 3:conformance window parameters specified")
  ("HorizontalPadding,-pdx",                          m_sourcePadding[0],                                   0, "Horizontal source padding for conformance window mode 2")
  ("VerticalPadding,-pdy",                            m_sourcePadding[1],                                   0, "Vertical source padding for conformance window mode 2")
  ("ConfWinLeft",                                     m_confWinLeft,                                        0, "Left offset for window conformance mode 3")
  ("ConfWinRight",                                    m_confWinRight,                                       0, "Right offset for window conformance mode 3")
  ("ConfWinTop",                                      m_confWinTop,                                         0, "Top offset for window conformance mode 3")
  ("ConfWinBottom",                                   m_confWinBottom,                                      0, "Bottom offset for window conformance mode 3")
  ("ScalingWindow",                                   m_explicitScalingWindowEnabled,                   false, "Enable scaling window")
  ("ScalWinLeft,-swl",                                m_scalWinLeft,                                        0, "Left offset for scaling window")
  ("ScalWinRight,-swr",                               m_scalWinRight,                                       0, "Right offset for scaling window")
  ("ScalWinTop,-swt",                                 m_scalWinTop,                                         0, "Top offset for scaling window")
  ("ScalWinBottom,-swb",                              m_scalWinBottom,                                      0, "Bottom offset for scaling window")
  ("AccessUnitDelimiter",                             m_AccessUnitDelimiter,                            false, "Enable Access Unit Delimiter NALUs")
  ("EnablePictureHeaderInSliceHeader",                m_enablePictureHeaderInSliceHeader,                true, "Enable Picture Header in Slice Header")
  ("FrameRate,-fr",                                   frameRate,                            std::to_string(0), "Frame rate")
  ("FrameSkip,-fs",                                   m_frameSkip,                                         0u, "Number of frames to skip at start of input YUV")
  ("TemporalSubsampleRatio,-ts",                      m_temporalSubsampleRatio,                            1u, "Temporal sub-sample ratio when reading input YUV")
  ("FramesToBeEncoded,f",                             m_framesToBeEncoded,                                  0, "Number of frames to be encoded (default=all)")
  ("ClipInputVideoToRec709Range",                     m_clipInputVideoToRec709Range,                   false, "If true then clip input video to the Rec. 709 Range on loading when InternalBitDepth is less than MSBExtendedBitDepth")
  ("ClipOutputVideoToRec709Range",                    m_clipOutputVideoToRec709Range,                  false, "If true then clip output video to the Rec. 709 Range on saving when OutputBitDepth is less than InternalBitDepth")
  ("PYUV",                                            m_packedYUVMode,                                  false, "If true then output 10-bit and 12-bit YUV data as 5-byte and 3-byte (respectively) packed YUV data. Ignored for interlaced output.")
  ("SummaryOutFilename",                              m_summaryOutFilename,                          std::string(), "Filename to use for producing summary output file. If empty, do not produce a file.")
  ("SummaryPicFilenameBase",                          m_summaryPicFilenameBase,                      std::string(), "Base filename to use for producing summary picture output files. The actual filenames used will have I.txt, P.txt and B.txt appended. If empty, do not produce a file.")
  ("SummaryVerboseness",                              m_summaryVerboseness,                                0u, "Specifies the level of the verboseness of the text output")
  ("Verbosity,v",                                     m_verbosity,                               (int)VERBOSE, "Specifies the level of the verboseness")

#if JVET_O0756_CONFIG_HDRMETRICS || JVET_O0756_CALCULATE_HDRMETRICS
  ( "WhitePointDeltaE1",                              m_whitePointDeltaE[0],                            100.0, "1st reference white point value")
  ( "WhitePointDeltaE2",                              m_whitePointDeltaE[1],                           1000.0, "2nd reference white point value")
  ( "WhitePointDeltaE3",                              m_whitePointDeltaE[2],                           5000.0, "3rd reference white point value")
  ( "MaxSampleValue",                                 m_maxSampleValue,                               10000.0, "Maximum sample value for floats")
  ( "InputSampleRange",                               m_sampleRange,                                        0, "Sample Range")
  ( "InputColorPrimaries",                            m_colorPrimaries,                                     1, "Input Color Primaries")
  ( "EnableTFunctionLUT",                             m_enableTFunctionLUT,                             false, "Input Color Primaries")
  ( "ChromaLocation",                                 m_chromaLocation,                                     2, "Location of Chroma Samples")
  ( "ChromaUpsampleFilter",                           m_chromaUPFilter,                                     1, "420 to 444 conversion filters")
  ( "CropOffsetLeft",                                 m_cropOffsetLeft,                                     0, "Crop Offset Left position")
  ( "CropOffsetTop",                                  m_cropOffsetTop,                                      0, "Crop Offset Top position")
  ( "CropOffsetRight",                                m_cropOffsetRight,                                    0, "Crop Offset Right position")
  ( "CropOffsetBottom",                               m_cropOffsetBottom,                                   0, "Crop Offset Bottom position")
  ( "CalculateHdrMetrics",                            m_calculateHdrMetrics,                            false, "Enable HDR metric calculation")
#endif
#if GREEN_METADATA_SEI_ENABLED
  ("SEIGreenMetadataType",                            m_greenMetadataType,                                  -1, "Value for the green_metadata_type specifies the type of metadata that is present in the SEI message. -1: Green metadata disabled (default); 0: Decoder complexity metrics; 1: quality recovery after low-power encoding")
  ("SEIGreenMetadataGranularityType",                 m_greenMetadataGranularityType,                       -1, "Specifies the type of granularity for which the metadata are applicable. Only implemented for picture granularity. ")
  ("SEIGreenMetadataPeriodType",                      m_greenMetadataPeriodType,                             0, "Value for the Period Type incidacting over which amount of time the metadata have been calculated")
  ("SEIGreenMetadataPeriodTypeSeconds",               m_greenMetadataPeriodNumSeconds,                       1, "indicates the number of seconds over which the metadata are applicable when SEIGreenMetadataPeriodType is 2.")
  ("SEIGreenMetadataPeriodTypePictures",              m_greenMetadataPeriodNumPictures,                      1, "specifies the number of pictures, counted in decoding order, over which the metadata are applicable when SEIGreenMetadataPeriodType is 3.")
  ("SEIXSDMetricNumber",                              m_xsdNumberMetrics,                                    1, "Number of quality metrics.")
  ("SEIXSDMetricTypePSNR",                            m_xsdMetricTypePSNR,                               false, "Set to 'true' if PSNR shall be signalled. ")
  ("SEIXSDMetricTypeSSIM",                            m_xsdMetricTypeSSIM,                               false, "Set to 'true' if SSIM shall be signalled. ")
  ("SEIXSDMetricTypeWPSNR",                           m_xsdMetricTypeWPSNR,                              false, "Set to 'true' if WPSNR shall be signalled. ")
  ("SEIXSDMetricTypeWSPSNR",                          m_xsdMetricTypeWSPSNR,                             false, "Set to 'true' if WSSPSNR shall be signalled. ")
  ("SEIGreenMetadataExtendedRepresentation",          m_greenMetadataExtendedRepresentation,                 0, "Specifies whether reduced or extended set of complexity metrics is signelled. ")
  ("GMFA",                                            m_GMFA,                                            false, "Write output file for the Green-Metadata analyzer for decoder complexity metrics (JVET-P0085)\n")
  ("GMFAFile",                                        m_GMFAFile,                                   std::string(""), "File for the Green Metadata Bit Stream Feature Analyzer output (JVET-P0085)\n")
#if GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464
  ("SEIGreenMetadataAMIFlags",                        m_greenMetadataAMIFlags, (unsigned)GREEN_METADATA_AMI_FLAGS::CANCEL, "Specifies which parameters are needed to apply the attenuation map.")
  ("SEIGreenMetadataAMIDisplayModel",                 m_greenMetadataAMIDisplayModel,                       0u, "Specifies the display models for which the application of attenuation maps may be used.")
  ("SEIGreenMetadataAMIApproximationModel",           m_greenMetadataAMIApproximationModel,                 0u, "Specifies the model used to extrapolate the attenuation map samples for another energy reduction rate.")
  ("SEIGreenMetadataAMIMapNumber",                    m_greenMetadataAMIMapNumber,                          1u, "Specifies the number of auxiliary pictures of type AUX_ALPHA in the CVS. It corresponds to the number of attenuation maps.")
  ("SEIGreenMetadataAMILayerId",                      cfg_greenMetadataAMILayerId, cfg_greenMetadataAMILayerId,                               "Specifies the identifier of the decoded layer for the attenuation map of index i.")
  ("SEIGreenMetadataAMIOlsNumber",                    cfg_greenMetadataAMIOlsNumber, cfg_greenMetadataAMIOlsNumber,                           "Specifies the number of output layer sets to which the attenuation map of index i belongs.")
  ("SEIGreenMetadataAMIOlsId",                        cfg_greenMetadataAMIOlsId, cfg_greenMetadataAMIOlsId,                                   "Specifies the identifier of the output layer set of index j for the attenuation map of index i.")
  ("SEIGreenMetadataAMIEnergyReductionRate",          cfg_greenMetadataAMIEnergyReductionRate, cfg_greenMetadataAMIEnergyReductionRate,       "Specifies the expected energy savings rate when the video is displayed after applying the attenuation map of index i.")
  ("SEIGreenMetadataAMIVideoQualityMetricType",       cfg_greenMetadataAMIVideoQualityMetricType, cfg_greenMetadataAMIVideoQualityMetricType, "Specifies the quality metric which was considered to inform of the reduction of the video quality due to the application of the attenuation map of index i.")
  ("SEIGreenMetadataAMIVideoQualityLevel",            cfg_greenMetadataAMIVideoQualityLevel, cfg_greenMetadataAMIVideoQualityLevel,           "Indicates the expected video quality when the video is rendered on a display after application of the attenuation map of index i.")
  ("SEIGreenMetadataAMIMaxValue",                     cfg_greenMetadataAMIMaxValue, cfg_greenMetadataAMIMaxValue,                             "Indicates the maximum value of the attenuation map of index i.")
  ("SEIGreenMetadataAMIAttenuationUseIdc",            cfg_greenMetadataAMIAttenuationUseIdc, cfg_greenMetadataAMIAttenuationUseIdc,           "Specifies the use of the attenuation map sample values of the decoded auxiliary picture of index i")
  ("SEIGreenMetadataAMIAttenuationCompIdc",           cfg_greenMetadataAMIAttenuationCompIdc, cfg_greenMetadataAMIAttenuationCompIdc,         "Specifies on which colour component(s) of the associated primary picture the attenuation map of index i should be applied.")
  ("SEIGreenMetadataAMIPreprocessingFlag",            cfg_greenMetadataAMIPreprocessingFlag, cfg_greenMetadataAMIPreprocessingFlag,           "Specifies whether some pre-upsampling is to be used on the attenuation map of index i.")
  ("SEIGreenMetadataAMIPreprocessingTypeIdc",         cfg_greenMetadataAMIPreprocessingTypeIdc, cfg_greenMetadataAMIPreprocessingTypeIdc,     "Specifies the recommended type of the interpolation used to resample the attenuation map on index i.")
  ("SEIGreenMetadataAMIPreprocessingScaleIdc",        cfg_greenMetadataAMIPreprocessingScaleIdc, cfg_greenMetadataAMIPreprocessingScaleIdc,   "Specifies which scaling should be applied to the attenuation map of index i before applying it on the decoded picture.")
  ("SEIGreenMetadataAMIBacklightScalingIdc",          cfg_greenMetadataAMIBacklightScalingIdc, cfg_greenMetadataAMIBacklightScalingIdc,       "Specifies the process to compute the scaling factor of the backlight of transmissive pixel displays, derived from the attenuation map of index i.")
#endif
#endif
  //Field coding parameters
  ("FieldCoding",                                     m_isField,                                        false, "Signals if it's a field based coding")
  ("TopFieldFirst, Tff",                              m_isTopFieldFirst,                                false, "In case of field based coding, signals whether if it's a top field first or not")
  ("EfficientFieldIRAPEnabled",                       m_efficientFieldIRAPEnabled,                      true, "Enable to code fields in a specific, potentially more efficient, order.")
  ("HarmonizeGopFirstFieldCoupleEnabled",             m_harmonizeGopFirstFieldCoupleEnabled,            true, "Enables harmonization of Gop first field couple")

  // Profile and level
  ("Profile",                                         extendedProfile,              ExtendedProfileName::NONE, "Profile name to use for encoding. Use [multilayer_]main_10[_444][_still_picture], auto, or none")
  ("Level",                                           m_level,                                    Level::NONE, "Level limit to be used, eg 5.1, or none")
  ("Tier",                                            m_levelTier,                                Level::MAIN, "Tier to use for interpretation of --Level (main or high only)")
  ("FrameOnlyConstraintFlag",                         m_frameOnlyConstraintFlag,                        true, "Bitstream contains only frames")
  ("MultiLayerEnabledFlag",                           m_multiLayerEnabledFlag,                         false, "Bitstream might contain more than one layer")
  ("SubProfile",                                      cfg_SubProfile,                          cfg_SubProfile,  "Sub-profile idc")
  ("EnableDecodingCapabilityInformation",             m_DCIEnabled,                                     false, "Enables writing of Decoding Capability Information")
  ("MaxBitDepthConstraint",                           m_bitDepthConstraint,                                0u, "Bit depth to use for profile-constraint for RExt profiles. 0=automatically choose based upon other parameters")
  ("MaxChromaFormatConstraint",                       tmpConstraintChromaFormat,                            0, "Chroma-format to use for the profile-constraint for RExt profiles. 0=automatically choose based upon other parameters")

  ("GciPresentFlag",                                  m_gciPresentFlag,                                 false, "GCI field present")
  ("IntraOnlyConstraintFlag",                         m_intraOnlyConstraintFlag,                        false, "Value of intra_only_constraint_flag")
  ("AllLayersIndependentConstraintFlag",              m_allLayersIndependentConstraintFlag,             false, "Indicate that all layers are independent")
  ("OnePictureOnlyConstraintFlag",                    m_onePictureOnlyConstraintFlag,                   false, "Value of general_intra_constraint_flag. Can only be used for single frame encodings. Will be set to true for still picture profiles")
  ("MaxBitDepthConstraintIdc",                        m_maxBitDepthConstraintIdc,                          16u, "Indicate that sps_bitdepth_minus8 plus 8 shall be in the range of 0 to m_maxBitDepthConstraintIdc")
  ("MaxChromaFormatConstraintIdc",                    tmpMaxChromaFormatConstraintIdc,                        3, "Indicate that sps_chroma_format_idc shall be in the range of 0 to m_maxChromaFormatConstraintIdc")
  ("NoTrailConstraintFlag",                           m_noTrailConstraintFlag,                          false, "Indicate that TRAIL is deactivated")
  ("NoStsaConstraintFlag",                            m_noStsaConstraintFlag,                           false, "Indicate that STSA is deactivated")
  ("NoRaslConstraintFlag",                            m_noRaslConstraintFlag,                           false, "Indicate that RSAL is deactivated")
  ("NoRadlConstraintFlag",                            m_noRadlConstraintFlag,                           false, "Indicate that RADL is deactivated")
  ("NoIdrConstraintFlag",                             m_noIdrConstraintFlag,                            false, "Indicate that IDR is deactivated")
  ("NoCraConstraintFlag",                             m_noCraConstraintFlag,                            false, "Indicate that CRA is deactivated")
  ("NoGdrConstraintFlag",                             m_noGdrConstraintFlag,                            false, "Indicate that GDR is deactivated")
  ("NoApsConstraintFlag",                             m_noApsConstraintFlag,                            false, "Indicate that APS is deactivated")
  ("OneTilePerPicConstraintFlag",                     m_oneTilePerPicConstraintFlag,                    false, "Indicate that each picture shall contain only one tile")
  ("PicHeaderInSliceHeaderConstraintFlag",            m_picHeaderInSliceHeaderConstraintFlag,           false, "Indicate that picture header is present in slice header")
  ("OneSlicePerPicConstraintFlag",                    m_oneSlicePerPicConstraintFlag,                   false, "Indicate that each picture shall contain only one slice")
  ("NoIdrRplConstraintFlag",                          m_noIdrRplConstraintFlag,                         false, "Indicate that RPL is not present in SH of IDR slices")
  ("NoRectSliceConstraintFlag",                       m_noRectSliceConstraintFlag,                      false, "Indicate that rectagular slice is deactivated")
  ("OneSlicePerSubpicConstraintFlag",                 m_oneSlicePerSubpicConstraintFlag,                false, "Indicate that each subpicture shall contain only one slice")
  ("NoSubpicInfoConstraintFlag",                      m_noSubpicInfoConstraintFlag,                     false, "Indicate that subpicture information is not present")
  ("MaxLog2CtuSizeConstraintIdc",                     m_maxLog2CtuSizeConstraintIdc,                        8, "Indicate that Log2CtuSize shall be in the range of 0 to m_maxLog2CtuSizeConstraintIdc")
  ("NoPartitionConstraintsOverrideConstraintFlag",    m_noPartitionConstraintsOverrideConstraintFlag,   false, "Indicate that Partition Override is deactivated")
  ("MttConstraintFlag",                               m_noMttConstraintFlag,                            false, "Indicate that Mtt is deactivated")
  ("NoQtbttDualTreeIntraConstraintFlag",              m_noQtbttDualTreeIntraConstraintFlag,            false, "Indicate that Qtbtt DualTree Intra is deactivated")
  ("NoPaletteConstraintFlag",                         m_noPaletteConstraintFlag,                        false, "Indicate that PLT is deactivated")
  ("NoIbcConstraintFlag",                             m_noIbcConstraintFlag,                            false, "Indicate that IBC is deactivated")
  ("NoIspConstraintFlag",                             m_noIspConstraintFlag,                            false, "Indicate that ISP is deactivated")
  ("NoMrlConstraintFlag",                             m_noMrlConstraintFlag,                            false, "Indicate that MRL is deactivated")
  ("NoMipConstraintFlag",                             m_noMipConstraintFlag,                            false, "Indicate that MIP is deactivated")
  ("NoCclmConstraintFlag",                            m_noCclmConstraintFlag,                          false, "Indicate that CCLM is deactivated")
  ("NoRprConstraintFlag",                             m_noRprConstraintFlag,                            false, "Indicate that reference picture resampling is deactivated")
  ("NoResChangeInClvsConstraintFlag",                 m_noResChangeInClvsConstraintFlag,                false, "Indicate that the picture spatial resolution does not change within any CLVS referring to the SPS")
  ("WeightedPredictionConstraintFlag",                m_noWeightedPredictionConstraintFlag,             false, "Indicate that Weighted Prediction is deactivated")
  ("NoRefWraparoundConstraintFlag",                   m_noRefWraparoundConstraintFlag,                 false, "Indicate that Reference Wraparound is deactivated")
  ("NoTemporalMvpConstraintFlag",                     m_noTemporalMvpConstraintFlag,                   false, "Indicate that temporal MVP is deactivated")
  ("NoSbtmvpConstraintFlag",                          m_noSbtmvpConstraintFlag,                        false, "Indicate that SbTMVP is deactivated")
  ("NoAmvrConstraintFlag",                            m_noAmvrConstraintFlag,                          false, "Indicate that AMVR is deactivated")
  ("NoSmvdConstraintFlag",                            m_noSmvdConstraintFlag,                           false, "Indicate that SMVD is deactivated")
  ("NoBdofConstraintFlag",                            m_noBdofConstraintFlag,                          false, "Indicate that BIO is deactivated")
  ("NoDmvrConstraintFlag",                            m_noDmvrConstraintFlag,                           false, "Indicate that DMVR is deactivated")
  ("NoMmvdConstraintFlag",                            m_noMmvdConstraintFlag,                           false, "Indicate that MMVD is deactivated")
  ("NoAffineMotionConstraintFlag",                    m_noAffineMotionConstraintFlag,                  false, "Indicate that Affine is deactivated")
  ("NoProfConstraintFlag",                            m_noProfConstraintFlag,                           false, "Indicate that PROF is deactivated")
  ("NoBcwConstraintFlag",                             m_noBcwConstraintFlag,                           false, "Indicate that BCW is deactivated")
  ("NoCiipConstraintFlag",                            m_noCiipConstraintFlag,                          false, "Indicate that CIIP is deactivated")
  ("NoGpmConstraintFlag",                             m_noGeoConstraintFlag,                            false, "Indicate that GPM is deactivated")
  ("NoTransformSkipConstraintFlag",                   m_noTransformSkipConstraintFlag,                  false, "Indicate that Transform Skip is deactivated")
  ("NoLumaTransformSize64ConstraintFlag",             m_noLumaTransformSize64ConstraintFlag,            false, "Indicate that Luma Transform Size 64 is deactivated")
  ("NoBDPCMConstraintFlag",                           m_noBDPCMConstraintFlag,                          false, "Indicate that BDPCM is deactivated")
  ("NoMtsConstraintFlag",                             m_noMtsConstraintFlag,                           false, "Indicate that MTS is deactivated")
  ("NoLfnstConstraintFlag",                           m_noLfnstConstraintFlag,                          false, "Indicate that LFNST is deactivated")
  ("NoJointCbCrConstraintFlag",                       m_noJointCbCrConstraintFlag,                      false, "Indicate that JCCR is deactivated")
  ("NoSbtConstraintFlag",                             m_noSbtConstraintFlag,                            false, "Indicate that SBT is deactivated")
  ("NoActConstraintFlag",                             m_noActConstraintFlag,                            false, "Indicate that ACT is deactivated")
  ("NoExplicitScaleListConstraintFlag",               m_noExplicitScaleListConstraintFlag,              false, "Indicate that explicit scaling list is deactivated")
  ("NoChromaQpOffsetConstraintFlag",                  m_noChromaQpOffsetConstraintFlag,                 false, "Indicate that chroma qp offset is zero")
  ("NoDepQuantConstraintFlag",                        m_noDepQuantConstraintFlag,                      false, "Indicate that DQ is deactivated")
  ("NoSignDataHidingConstraintFlag",                  m_noSignDataHidingConstraintFlag,                false, "Indicate that SDH is deactivated")
  ("NoCuQpDeltaConstraintFlag",                       m_noCuQpDeltaConstraintFlag,                     false, "Indicate that CU QP delta is deactivated")
  ("NoSaoConstraintFlag",                             m_noSaoConstraintFlag,                           false, "Indicate that SAO is deactivated")
  ("NoAlfConstraintFlag",                             m_noAlfConstraintFlag,                           false, "Indicate that ALF is deactivated")
  ("NoCCAlfConstraintFlag",                           m_noCCAlfConstraintFlag,                          false, "Indicate that CCALF is deactivated")
  ("NoLmcsConstraintFlag",                            m_noLmcsConstraintFlag,                           false, "Indicate that LMCS is deactivated")
  ("NoLadfConstraintFlag",                            m_noLadfConstraintFlag,                          false, "Indicate that LADF is deactivated")
  ("NoVirtualBoundaryConstraintFlag",                 m_noVirtualBoundaryConstraintFlag,                false, "Indicate that virtual boundary is deactivated")
  ("AllRapPicturesFlag",                              m_allRapPicturesFlag,                             false, "Indicate that all pictures in OlsInScope are IRAP pictures or GDR pictures with ph_recovery_poc_cnt equal to 0")
  ("NoExtendedPrecisionProcessingConstraintFlag",     m_noExtendedPrecisionProcessingConstraintFlag,    false, "Indicate that ExtendedPrecision is deactivated")
  ("NoTsResidualCodingRiceConstraintFlag",            m_noTsResidualCodingRiceConstraintFlag,           false, "Indicate that TSRCRicePresent is deactivated")
  ("NoRrcRiceExtensionConstraintFlag",                m_noRrcRiceExtensionConstraintFlag,               false, "Indicate that ExtendedRiceRRC is deactivated")
  ("NoPersistentRiceAdaptationConstraintFlag",        m_noPersistentRiceAdaptationConstraintFlag,       false, "Indicate that GolombRiceParameterAdaptation is deactivated")
  ("NoReverseLastSigCoeffConstraintFlag",             m_noReverseLastSigCoeffConstraintFlag,            false, "Indicate that ReverseLastSigCoeff is deactivated")

  ("CTUSize",                                         m_ctuSize,                                       128u, "CTUSize (specifies the CTU size if QTBT is on) [default: 128]")
  ("Log2MinCuSize",                                   m_log2MinCuSize,                                     2u, "Log2 min CU size")
  ("SubPicInfoPresentFlag",                           m_subPicInfoPresentFlag,                          false, "equal to 1 specifies that subpicture parameters are present in in the SPS RBSP syntax")
  ("NumSubPics",                                      m_numSubPics,                                        0u, "specifies the number of subpictures")
  ("SubPicSameSizeFlag",                              m_subPicSameSizeFlag,                             false, "equal to 1 specifies that all subpictures in the CLVS have the same width specified by sps_subpic_width_minus1[ 0 ] and the same height specified by sps_subpic_height_minus1[ 0 ].")
  ("SubPicCtuTopLeftX",                               cfg_subPicCtuTopLeftX,            cfg_subPicCtuTopLeftX, "specifies horizontal position of top left CTU of i-th subpicture in unit of CtbSizeY")
  ("SubPicCtuTopLeftY",                               cfg_subPicCtuTopLeftY,            cfg_subPicCtuTopLeftY, "specifies vertical position of top left CTU of i-th subpicture in unit of CtbSizeY")
  ("SubPicWidth",                                     cfg_subPicWidth,                        cfg_subPicWidth, "specifies the width of the i-th subpicture in units of CtbSizeY")
  ("SubPicHeight",                                    cfg_subPicHeight,                      cfg_subPicHeight, "specifies the height of the i-th subpicture in units of CtbSizeY")
  ("SubPicTreatedAsPicFlag",                          cfg_subPicTreatedAsPicFlag,  cfg_subPicTreatedAsPicFlag, "equal to 1 specifies that the i-th subpicture of each coded picture in the CLVS is treated as a picture in the decoding process excluding in-loop filtering operations")
  ("LoopFilterAcrossSubpicEnabledFlag",               cfg_loopFilterAcrossSubpicEnabledFlag, cfg_loopFilterAcrossSubpicEnabledFlag, "equal to 1 specifies that in-loop filtering operations may be performed across the boundaries of the i-th subpicture in each coded picture in the CLVS")
  ("SubPicIdMappingExplicitlySignalledFlag",          m_subPicIdMappingExplicitlySignalledFlag,         false, "equal to 1 specifies that the subpicture ID mapping is explicitly signalled, either in the SPS or in the PPSs")
  ("SubPicIdMappingInSpsFlag",                        m_subPicIdMappingInSpsFlag,                       false, "equal to 1 specifies that subpicture ID mapping is signalled in the SPS")
  ("SubPicIdLen",                                     m_subPicIdLen,                                       0u, "specifies the number of bits used to represent the syntax element sps_subpic_id[ i ]. ")
  ("SubPicId",                                        cfg_subPicId,                              cfg_subPicId, "specifies that subpicture ID of the i-th subpicture")
  ("SingleSlicePerSubpic",                            m_singleSlicePerSubPicFlag,                       false, "Enables setting of a single slice per sub-picture (no explicit configuration required)")
  ("EnablePartitionConstraintsOverride",              m_SplitConsOverrideEnabledFlag,                    true, "Enable partition constraints override")
  ("MinQTISlice",                                     m_minQt[0],                                        8u, "MinQTISlice")
  ("MinQTLumaISlice",                                 m_minQt[0],                                        8u, "MinQTLumaISlice")
  ("MinQTChromaISliceInChromaSamples",                m_minQt[2],                                        4u, "MinQTChromaISliceInChromaSamples")
  ("MinQTNonISlice",                                  m_minQt[1],                                        8u, "MinQTNonISlice")
  ("MaxMTTHierarchyDepth",                            m_uiMaxMTTHierarchyDepth,                            3u, "MaxMTTHierarchyDepth")
  ("MaxMTTHierarchyDepthI",                           m_uiMaxMTTHierarchyDepthI,                           3u, "MaxMTTHierarchyDepthI")
  ("MaxMTTHierarchyDepthISliceL",                     m_uiMaxMTTHierarchyDepthI,                           3u, "MaxMTTHierarchyDepthISliceL")
  ("MaxMTTHierarchyDepthISliceC",                     m_uiMaxMTTHierarchyDepthIChroma,                     3u, "MaxMTTHierarchyDepthISliceC")
  ("MaxBTLumaISlice",                                 m_maxBt[0],                                       32u, "MaxBTLumaISlice")
  ("MaxBTChromaISlice",                               m_maxBt[2],                                       64u, "MaxBTChromaISlice")
  ("MaxBTNonISlice",                                  m_maxBt[1],                                      128u, "MaxBTNonISlice")
  ("MaxTTLumaISlice",                                 m_maxTt[0],                                       32u, "MaxTTLumaISlice")
  ("MaxTTChromaISlice",                               m_maxTt[2],                                       32u, "MaxTTChromaISlice")
  ("MaxTTNonISlice",                                  m_maxTt[1],                                       64u, "MaxTTNonISlice")
  ("TTFastSkip",                                      m_ttFastSkip,                                        31, "fast skip method for TT split partition")
  ("TTFastSkipThr",                                   m_ttFastSkipThr,                                  1.075, "Threshold value of fast skip method for TT split partition")
  ("DualITree",                                       m_dualTree,                                       false, "Use separate QTBT trees for intra slice luma and chroma channel types")
  ( "LFNST",                                          m_LFNST,                                          false, "Enable LFNST (0:off, 1:on)  [default: off]" )
  ( "FastLFNST",                                      m_useFastLFNST,                                   false, "Fast methods for LFNST" )
  ("SbTMVP",                                          m_sbTmvpEnableFlag,                               false, "Enable Subblock Temporal Motion Vector Prediction (0: off, 1: on) [default: off]")
  ("MMVD",                                            m_MMVD,                                            true, "Enable Merge mode with Motion Vector Difference (0:off, 1:on)  [default: 1]")
  ("Affine",                                          m_Affine,                                         false, "Enable affine prediction (0:off, 1:on)  [default: off]")
  ("AffineType",                                      m_AffineType,                                      true,  "Enable affine type prediction (0:off, 1:on)  [default: on]" )
  ("AdaptBypassAffineMe",                             m_adaptBypassAffineMe,                            false, "Adaptively bypass affine ME (0: off, 1:on, defaul: off]")
  ("PROF",                                            m_PROF,                                           false, "Enable Prediction refinement with optical flow for affine mode (0:off, 1:on)  [default: off]")
  ("BIO",                                             m_BIO,                                            false, "Enable bi-directional optical flow")
  ("IMV",                                             m_ImvMode,                                            1, "Adaptive MV precision Mode (IMV)\n"
                                                                                                               "\t0: disabled\n"
                                                                                                               "\t1: enabled (1/2-Pel, Full-Pel and 4-PEL)\n")
  ("IMV4PelFast",                                     m_Imv4PelFast,                                        1, "Fast 4-Pel Adaptive MV precision Mode 0:disabled, 1:enabled)  [default: 1]")
  ("LMChroma",                                        m_LMChroma,                                           1, " LMChroma prediction "
                                                                                                               "\t0:  Disable LMChroma\n"
                                                                                                               "\t1:  Enable LMChroma\n")
  ("HorCollocatedChroma",                             m_horCollocatedChromaFlag,                           -1, "Specifies location of a chroma sample relatively to the luma sample in horizontal direction in the reference picture resampling\n"
                                                                                                               "\t-1: set according to chroma location type (default)\n"
                                                                                                               "\t0:  horizontally shifted by 0.5 units of luma samples\n"
                                                                                                               "\t1:  collocated\n")
  ("VerCollocatedChroma",                             m_verCollocatedChromaFlag,                           -1, "Specifies location of a chroma sample relatively to the luma sample in vertical direction in the cross-component linear model intra prediction and the reference picture resampling\n"
                                                                                                               "\t-1: set according to chroma location type (default)\n"
                                                                                                               "\t0:  horizontally co-sited, vertically shifted by 0.5 units of luma samples\n"
                                                                                                               "\t1:  collocated\n")
  ("MTS",                                             m_mtsMode,                                            0, "Multiple Transform Set (MTS)\n"
    "\t0:  Disable MTS\n"
    "\t1:  Enable explicit Intra MTS\n"
    "\t2:  Enable implicit Intra and explicit Inter MTS\n"
    "\t3:  Enable explicit Intra and explicit Inter MTS\n"
    "\t4:  Enable implicit Intra MTS\n")
  ("MTSIntraMaxCand",                                 m_MTSIntraMaxCand,                                    3, "Number of additional candidates to test in encoder search for MTS in intra slices\n")
  ("MTSInterMaxCand",                                 m_MTSInterMaxCand,                                    4, "Number of additional candidates to test in encoder search for MTS in inter slices\n")
  ("MTSImplicit",                                     m_mtsImplicitIntra,                                   0, "Enable implicit Intra MTS (when MTS is 0)\n")
  ( "SBT",                                            m_SBT,                                            false, "Enable Sub-Block Transform for inter blocks\n" )
  ( "SBTFast64WidthTh",                               m_SBTFast64WidthTh,                                1920, "Picture width threshold for testing size-64 SBT in RDO (now for HD and above sequences)\n")
  ( "ISP",                                            m_ISP,                                            false, "Enable Intra Sub-Partitions\n" )
  ("SMVD",                                            m_SMVD,                                           false, "Enable Symmetric MVD\n")
  ("CompositeLTReference",                            m_compositeRefEnabled,                            false, "Enable Composite Long Term Reference Frame")
  ("BCW",                                             m_bcw,                                            false, "Enable Generalized Bi-prediction(Bcw)")
  ("BcwFast",                                         m_BcwFast,                                        false, "Fast methods for Generalized Bi-prediction(Bcw)\n")
  ("LADF",                                            m_LadfEnabed,                                     false, "Luma adaptive deblocking filter QP Offset(L0414)")
  ("LadfNumIntervals",                                m_ladfNumIntervals,                                   3, "LADF number of intervals (2-5, inclusive)")
  ("LadfQpOffset",                                    cfg_ladfQpOffset,                      cfg_ladfQpOffset, "LADF QP offset")
  ("LadfIntervalLowerBound",                          cfg_ladfIntervalLowerBound,  cfg_ladfIntervalLowerBound, "LADF lower bound for 2nd lowest interval")
  ("CIIP",                                            m_ciip,                                           false, "Enable CIIP mode")
  ("Geo",                                             m_Geo,                                            false, "Enable geometric partitioning mode (0:off, 1:on)")
  ("HashME",                                          m_HashME,                                         false, "Enable hash motion estimation (0:off, 1:on)")

  ("AllowDisFracMMVD",                                m_allowDisFracMMVD,                               false, "Disable fractional MVD in MMVD mode adaptively")
  ("AffineAmvr",                                      m_AffineAmvr,                                     false, "Eanble AMVR for affine inter mode")
  ("AffineAmvrEncOpt",                                m_AffineAmvrEncOpt,                               false, "Enable encoder optimization of affine AMVR")
  ("AffineAmvp",                                      m_AffineAmvp,                                      true, "Enable AMVP for affine inter mode")
  ("DMVR",                                            m_DMVR,                                           false, "Decoder-side Motion Vector Refinement")
  ("MmvdDisNum",                                      m_MmvdDisNum,                                     8,     "Number of MMVD Distance Entries")
  ("ColorTransform",                                  m_useColorTrans,                                  false, "Enable the color transform")
  ("PLT",                                             m_PLTMode,                                           0u, "PLTMode (0x1:enabled, 0x0:disabled)  [default: disabled]")
  ("JointCbCr",                                       m_jointCbCrMode,                                  false, "Enable joint coding of chroma residuals (JointCbCr, 0:off, 1:on)")
  ( "IBC",                                            m_IBCMode,                                           0u, "IBCMode (0x1:enabled, 0x0:disabled)  [default: disabled]")
  ( "IBCLocalSearchRangeX",                           m_IBCLocalSearchRangeX,                            128u, "Search range of IBC local search in x direction")
  ( "IBCLocalSearchRangeY",                           m_IBCLocalSearchRangeY,                            128u, "Search range of IBC local search in y direction")
  ( "IBCHashSearch",                                  m_IBCHashSearch,                                     1u, "Hash based IBC search")
  ( "IBCHashSearchMaxCand",                           m_IBCHashSearchMaxCand,                            256u, "Max candidates for hash based IBC search")
  ( "IBCHashSearchRange4SmallBlk",                    m_IBCHashSearchRange4SmallBlk,                     256u, "Small block search range in based IBC search")
  ( "IBCFastMethod",                                  m_IBCFastMethod,                                     6u, "Fast methods for IBC")
  ("DMVREncMvSelect",                                 m_dmvrEncSelect,                                  false, "Enable method for avoiding select MVs that are more likely to give subjective artifacts")
  ("DMVREncMvSelectBaseQpTh",                         m_dmvrEncSelectBaseQpTh,                             33, "Base QP Threshold for enabling the DMVR MV selection")
  ("DMVREncMvSelectDisableHighestTemporalLayer",      m_dmvrEncSelectDisableHighestTemporalLayer,        true, "Disable DMVR encoder control for highest temporal layer unless frame rate is less or equal to 30Hz")

  ("WrapAround",                                      m_wrapAround,                                     false, "Enable horizontal wrap-around motion compensation for inter prediction (0:off, 1:on)  [default: off]")
  ("WrapAroundOffset",                                m_wrapAroundOffset,                                  0u, "Offset in luma samples used for computing the horizontal wrap-around position")

  // ADD_NEW_TOOL : (encoder app) add parsing parameters here
  ( "VirtualBoundariesPresentInSPSFlag",              m_virtualBoundariesPresentFlag,                    true, "Virtual Boundary position information is signalled in SPS or PH (1:SPS, 0:PH)  [default: on]" )
  ("NumVerVirtualBoundaries",                         m_numVerVirtualBoundaries,                           0u, "Number of vertical virtual boundaries (0-3, inclusive)")
  ("NumHorVirtualBoundaries",                         m_numHorVirtualBoundaries,                           0u, "Number of horizontal virtual boundaries (0-3, inclusive)")
  ("VirtualBoundariesPosX",                           cfg_virtualBoundariesPosX,    cfg_virtualBoundariesPosX, "Locations of the vertical virtual boundaries in units of luma samples")
  ("VirtualBoundariesPosY",                           cfg_virtualBoundariesPosY,    cfg_virtualBoundariesPosY, "Locations of the horizontal virtual boundaries in units of luma samples")
  ("EncDbOpt",                                        m_encDbOpt,                                       false, "Encoder optimization with deblocking filter")
  ("AlfLambdaOpt",                                    m_encALFOpt,                                      false, "Encoder optimization with adaptive loop filter")
  ("LMCSEnable",                                      m_lmcsEnabled,                                    false, "Enable LMCS (luma mapping with chroma scaling")
  ("LMCSSignalType",                                  m_reshapeSignalType,                                 0u, "Input signal type: 0:SDR, 1:HDR-PQ, 2:HDR-HLG")
  ("LMCSUpdateCtrl",                                  m_updateCtrl,                                         0, "LMCS model update control: 0:RA, 1:AI, 2:LDB/LDP")
  ("LMCSAdpOption",                                   m_adpOption,                                          0, "LMCS adaptation options: 0:automatic(default),"
                                                                                                               "1: rsp both (CW66 for QP<=22), 2: rsp TID0 (for all QP),"
                                                                                                               "3: rsp inter(CW66 for QP<=22), 4: rsp inter(for all QP).")
  ("LMCSInitialCW",                                   m_initialCW,                                         0u, "LMCS initial total codeword (0~1023) when LMCSAdpOption > 0")
  ("LMCSOffset",                                      m_CSoffset,                                           0, "LMCS chroma residual scaling offset")
  ("IntraCMD",                                        m_intraCMD,                                          0u, "IntraChroma MD: 0: none, 1:fixed to default wPSNR weight")
  ("LCTUFast",                                        m_useFastLCTU,                                    false, "Fast methods for large CTU")
  ("FastMrg",                                         m_useFastMrg,                                     false, "Fast methods for inter merge")
  ("MaxMergeRdCandNumTotal",                          m_maxMergeRdCandNumTotal,                            15, "Max total number of merge candidates in full RD checking")
  ("MergeRdCandQuotaRegular",                         m_mergeRdCandQuotaRegular,            NUM_MRG_SATD_CAND, "Quota of regular merge candidates in full RD checking")
  ("MergeRdCandQuotaRegularSmallBlk",                 m_mergeRdCandQuotaRegularSmallBlk,    NUM_MRG_SATD_CAND, "Quota of regular merge candidates in full RD checking for blocks < 64 luma samples")
  ("MergeRdCandQuotaSubBlk",                          m_mergeRdCandQuotaSubBlk,         NUM_AFF_MRG_SATD_CAND, "Quota of sub-block merge candidates in full RD checking")
  ("MergeRdCandQuotaCiip",                            m_mergeRdCandQuotaCiip,                               1, "Quota of CIIP merge candidates in full RD checking")
  ("MergeRdCandQuotaGpm",                             m_mergeRdCandQuotaGpm,        GEO_MAX_TRY_WEIGHTED_SATD, "Quota of GPM merge candidates in full RD checking")
  ("PBIntraFast",                                     m_usePbIntraFast,                                 false, "Fast assertion if the intra mode is probable")
  ("AMaxBT",                                          m_useAMaxBT,                                      false, "Adaptive maximal BT-size")
  ("E0023FastEnc",                                    m_e0023FastEnc,                                    true, "Fast encoding setting for QTBT (proposal E0023)")
  ("MTTSkipping",                                     m_useMttSkip,                                     false, "MTT split modes early termination")
  ("ContentBasedFastQtbt",                            m_contentBasedFastQtbt,                           false, "Signal based QTBT speed-up")
  ("UseNonLinearAlfLuma",                             m_useNonLinearAlfLuma,                             true, "Non-linear adaptive loop filters for Luma Channel")
  ("UseNonLinearAlfChroma",                           m_useNonLinearAlfChroma,                           true, "Non-linear adaptive loop filters for Chroma Channels")
  ("MaxNumAlfAlternativesChroma",                     m_maxNumAlfAlternativesChroma,
                                                                    (unsigned)ALF_MAX_NUM_ALTERNATIVES_CHROMA, std::string("Maximum number of alternative Chroma filters (1-") + std::to_string(ALF_MAX_NUM_ALTERNATIVES_CHROMA) + std::string (", inclusive)") )
  ("MRL",                                             m_MRL,                                            false,  "Enable MRL (multiple reference line intra prediction)")
  ("MIP",                                             m_MIP,                                             true,  "Enable MIP (matrix-based intra prediction)")
  ("FastMIP",                                         m_useFastMIP,                                     false,  "Fast encoder search for MIP (matrix-based intra prediction)")
  ("FastLocalDualTreeMode",                           m_fastLocalDualTreeMode,                              0,  "Fast intra pass coding for local dual-tree in intra coding region, 0: off, 1: use threshold, 2: one intra mode only")
  ("SplitPredictAdaptMode",                           m_fastAdaptCostPredMode,                              0,  "Mode for split cost prediction, 0..2 (Default: 0)" )
  ("DisableFastTTfromBT",                             m_disableFastDecisionTT,                          false,  "Disable fast decision for TT from BT")
  // Unit definition parameters
  ("MaxCUWidth",                                      m_maxCuWidth,                                     64u)
  ("MaxCUHeight",                                     m_maxCuHeight,                                    64u)
  // todo: remove defaults from MaxCUSize
  ("MaxCUSize,s",                                     m_maxCuWidth,                                     64u, "Maximum CU size")
  ("MaxCUSize,s",                                     m_maxCuHeight,                                    64u, "Maximum CU size")

  ("Log2MaxTbSize",                                   m_log2MaxTbSize,                                      6, "Maximum transform block size in logarithm base 2 (Default: 6)")

  // Coding structure paramters
  ("IntraPeriod,-ip",                                 m_intraPeriod,                                      -1, "Intra period in frames, (-1: only first frame, -N: set to a multiple of N based on frame rate)")
#if GDR_ENABLED
  ("GdrEnabled",                                      m_gdrEnabled,                                     false, "GDR enabled")
  ("GdrPocStart",                                     m_gdrPocStart,                                       -1, "GDR poc start")
  ("GdrPeriod",                                       m_gdrPeriod,                                         -1, "Number of frames between GDR picture to the next GDR picture")
  ("GdrInterval",                                     m_gdrInterval,                                       -1, "Number of frames from GDR picture to the recovery point picture")
  ("GdrNoHash",                                       m_gdrNoHash,                                       true, "Do not generate decode picture hash SEI messages for GDR and recovering pictures")
#endif
  ("DecodingRefreshType,-dr",                         m_intraRefreshType,                                   0, "Intra refresh type (0:none 1:CRA 2:IDR 3:RecPointSEI)")
  ("GOPSize,g",                                       m_gopSize,                                           1, "GOP size of temporal structure")
  ("DRAPPeriod",                                      m_drapPeriod,                                         0, "DRAP period in frames (0: disable Dependent RAP indication SEI messages)")
  ("EDRAPPeriod",                                     m_edrapPeriod,                                        0, "EDRAP period in frames (0: disable Extended Dependent RAP indication SEI messages)")
  ("ReWriteParamSets",                                m_rewriteParamSets,                           false, "Enable rewriting of Parameter sets before every (intra) random access point")
  ("IDRRefParamList",                                 m_idrRefParamList,                            false, "Enable indication of reference picture list syntax elements in slice headers of IDR pictures")
  // motion search options
  ("DisableIntraInInter",                             m_bDisableIntraPUsInInterSlices,                  false, "Flag to disable intra PUs in inter slices")
  ("FastSearch",                                      tmpMotionEstimationSearchMethod,  to_underlying(MESearchMethod::DIAMOND), "0:Full search 1:Diamond 2:Selective 3:Enhanced Diamond")
  ("SearchRange,-sr",                                 m_iSearchRange,                                      96, "Motion search range")
  ("BipredSearchRange",                               m_bipredSearchRange,                                  4, "Motion search range for bipred refinement")
  ("MinSearchWindow",                                 m_minSearchWindow,                                    8, "Minimum motion search window size for the adaptive window ME")
  ("RestrictMESampling",                              m_bRestrictMESampling,                            false, "Restrict ME Sampling for selective inter motion search")
  ("ClipForBiPredMEEnabled",                          m_bClipForBiPredMeEnabled,                        false, "Enables clipping in the Bi-Pred ME. It is disabled to reduce encoder run-time")
  ("FastMEAssumingSmootherMVEnabled",                 m_bFastMEAssumingSmootherMVEnabled,                true, "Enables fast ME assuming a smoother MV.")

  ("HadamardME",                                      m_bUseHADME,                                       true, "Hadamard ME for fractional-pel")
  ("ASR",                                             m_bUseASR,                                        false, "Adaptive motion search range");
  opts.addOptions()

  // Mode decision parameters
  ("LambdaModifier0,-LM0",                            m_adLambdaModifier[ 0 ],                  ( double )1.0, "Lambda modifier for temporal layer 0. If LambdaModifierI is used, this will not affect intra pictures")
  ("LambdaModifier1,-LM1",                            m_adLambdaModifier[ 1 ],                  ( double )1.0, "Lambda modifier for temporal layer 1. If LambdaModifierI is used, this will not affect intra pictures")
  ("LambdaModifier2,-LM2",                            m_adLambdaModifier[ 2 ],                  ( double )1.0, "Lambda modifier for temporal layer 2. If LambdaModifierI is used, this will not affect intra pictures")
  ("LambdaModifier3,-LM3",                            m_adLambdaModifier[ 3 ],                  ( double )1.0, "Lambda modifier for temporal layer 3. If LambdaModifierI is used, this will not affect intra pictures")
  ("LambdaModifier4,-LM4",                            m_adLambdaModifier[ 4 ],                  ( double )1.0, "Lambda modifier for temporal layer 4. If LambdaModifierI is used, this will not affect intra pictures")
  ("LambdaModifier5,-LM5",                            m_adLambdaModifier[ 5 ],                  ( double )1.0, "Lambda modifier for temporal layer 5. If LambdaModifierI is used, this will not affect intra pictures")
  ("LambdaModifier6,-LM6",                            m_adLambdaModifier[ 6 ],                  ( double )1.0, "Lambda modifier for temporal layer 6. If LambdaModifierI is used, this will not affect intra pictures")
  ("LambdaModifierI,-LMI",                            cfg_adIntraLambdaModifier,    cfg_adIntraLambdaModifier, "Lambda modifiers for Intra pictures, comma separated, up to one the number of temporal layer. If entry for temporalLayer exists, then use it, else if some are specified, use the last, else use the standard LambdaModifiers.")
  ("IQPFactor,-IQF",                                  m_dIntraQpFactor,                                  -1.0, "Intra QP Factor for Lambda Computation. If negative, the default will scale lambda based on GOP size (unless LambdaFromQpEnable then IntraQPOffset is used instead)")
  ("LambdaScaleTowardsNextQP",                        m_lambdaScaleTowardsNextQP,                         0.0, "Scale lambda towards lambda for next integer QP. A negative number increase bitrate and a positive number decrease bitrate.")
  /* Quantization parameters */
  ("QP,q",                                            m_iQP,                                               30, "Qp value")
  ("PPSInitialQPOffset",                              m_qpRefAdj,                                           0, "Offset added to initial QP value coded in PPS")
  ("QPIncrementFrame,-qpif",                          m_qpIncrementAtSourceFrame,   std::optional<uint32_t>(), "If a source file frame number is specified, the internal QP will be incremented for all POCs associated with source frames >= frame number. If empty, do not increment.")
  ("IntraQPOffset",                                   m_intraQPOffset,                                      0, "Qp offset value for intra slice, typically determined based on GOP size")
  ("LambdaFromQpEnable",                              m_lambdaFromQPEnable,                             false, "Enable flag for derivation of lambda from QP")
  ("DeltaQpRD,-dqr",                                  m_uiDeltaQpRD,                                       0u, "max dQp offset for slice")
  ("MaxDeltaQP,d",                                    m_iMaxDeltaQP,                                        0, "max dQp offset for block")
  ("MaxCuDQPSubdiv,-dqd",                             m_cuQpDeltaSubdiv,                                    0, "Maximum subdiv for CU luma Qp adjustment")
  ("MaxCuChromaQpOffsetSubdiv",                       m_cuChromaQpOffsetSubdiv,                             0, "Maximum subdiv for CU chroma Qp adjustment")
  ("SliceCuChromaQpOffsetEnabled",                    m_cuChromaQpOffsetEnabled,                         true, "Enable local chroma QP offsets (slice level flag)")
  ("FastDeltaQP",                                     m_bFastDeltaQP,                                   false, "Fast Delta QP Algorithm")
#if SHARP_LUMA_DELTA_QP
  ("LumaLevelToDeltaQPMode",                          lumaLevelToDeltaQPMode,                              0u, "Luma based Delta QP 0(default): not used. 1: Based on CTU average, 2: Based on Max luma in CTU")
#if !WCG_EXT
  ("LumaLevelToDeltaQPMaxValWeight",                  m_lumaLevelToDeltaQPMapping.maxMethodWeight,        1.0, "Weight of block max luma val when LumaLevelToDeltaQPMode = 2")
#endif
  ("LumaLevelToDeltaQPMappingLuma",                   cfg_lumaLeveltoDQPMappingLuma,  cfg_lumaLeveltoDQPMappingLuma, "Luma to Delta QP Mapping - luma thresholds")
  ("LumaLevelToDeltaQPMappingDQP",                    cfg_lumaLeveltoDQPMappingQP,  cfg_lumaLeveltoDQPMappingQP, "Luma to Delta QP Mapping - DQP values")
#endif
  ("SmoothQPReductionEnable",                         m_smoothQPReductionEnable,                         false, "Enable QP reduction for smooth blocks according to: Clip3(SmoothQPReductionLimit, 0, SmoothQPReductionModelScale*baseQP+SmoothQPReductionModelOffset)")
  ("SmoothQPReductionPeriodicity",                    m_smoothQPReductionPeriodicity,                        0, "Periodicity parameter of the QP reduction model, 1: all frames, 0: only intra pictures, 2: every second frame, etc")
  ("SmoothQPReductionThresholdIntra",                 m_smoothQPReductionThresholdIntra,                   3.0, "Threshold parameter for smoothness for intra pictures (SmoothQPReductionThresholdIntra * number of samples in block)")
  ("SmoothQPReductionModelScaleIntra",                m_smoothQPReductionModelScaleIntra,                 -1.0, "Scale parameter of the QP reduction model for intra pictures ")
  ("SmoothQPReductionModelOffsetIntra",               m_smoothQPReductionModelOffsetIntra,                27.0, "Offset parameter of the QP reduction model for intra pictures ")
  ("SmoothQPReductionLimitIntra",                     m_smoothQPReductionLimitIntra,                       -16, "Threshold parameter for controlling maximum amount of QP reduction by the QP reduction model for intra pictures ")
  ("SmoothQPReductionThresholdInter",                 m_smoothQPReductionThresholdInter,                   3.0, "Threshold parameter for smoothness for inter pictures (SmoothQPReductionThresholdInter * number of samples in block)")
  ("SmoothQPReductionModelScaleInter",                m_smoothQPReductionModelScaleInter,                 -1.0, "Scale parameter of the QP reduction model for inter pictures")
  ("SmoothQPReductionModelOffsetInter",               m_smoothQPReductionModelOffsetInter,                27.0, "Offset parameter of the QP reduction model for inter pictures")
  ("SmoothQPReductionLimitInter",                     m_smoothQPReductionLimitInter,                        -4, "Threshold parameter for controlling maximum amount of QP reduction by the QP reduction model for inter pictures")
  ("BIM",                                             m_bimEnabled,                                      false, "Block Importance Mapping QP adaptation depending on estimated propagation of reference samples.")
  ("UseIdentityTableForNon420Chroma",                 m_useIdentityTableForNon420Chroma,                  true, "True: Indicates that 422/444 chroma uses identity chroma QP mapping tables; False: explicit Qp table may be specified in config")
  ("SameCQPTablesForAllChroma",                       m_chromaQpMappingTableParams.m_sameCQPTableForAllChromaFlag,                        true, "0: Different tables for Cb, Cr and joint Cb-Cr components, 1 (default): Same tables for all three chroma components")
  ("QpInValCb",                                       cfg_qpInValCb,                            cfg_qpInValCb, "Input coordinates for the QP table for Cb component")
  ("QpOutValCb",                                      cfg_qpOutValCb,                          cfg_qpOutValCb, "Output coordinates for the QP table for Cb component")
  ("QpInValCr",                                       cfg_qpInValCr,                            cfg_qpInValCr, "Input coordinates for the QP table for Cr component")
  ("QpOutValCr",                                      cfg_qpOutValCr,                          cfg_qpOutValCr, "Output coordinates for the QP table for Cr component")
  ("QpInValCbCr",                                     cfg_qpInValCbCr,                        cfg_qpInValCbCr, "Input coordinates for the QP table for joint Cb-Cr component")
  ("QpOutValCbCr",                                    cfg_qpOutValCbCr,                      cfg_qpOutValCbCr, "Output coordinates for the QP table for joint Cb-Cr component")
  ("CbQpOffset,-cbqpofs",                             m_cbQpOffset,                                         0, "Chroma Cb QP Offset")
  ("CrQpOffset,-crqpofs",                             m_crQpOffset,                                         0, "Chroma Cr QP Offset")
  ("CbQpOffsetDualTree",                              m_cbQpOffsetDualTree,                                 0, "Chroma Cb QP Offset for dual tree")
  ("CrQpOffsetDualTree",                              m_crQpOffsetDualTree,                                 0, "Chroma Cr QP Offset for dual tree")
  ("CbCrQpOffset,-cbcrqpofs",                         m_cbCrQpOffset,                                      -1, "QP Offset for joint Cb-Cr mode")
  ("CbCrQpOffsetDualTree",                            m_cbCrQpOffsetDualTree,                               0, "QP Offset for joint Cb-Cr mode in dual tree")
#if ER_CHROMA_QP_WCG_PPS
  ("WCGPPSEnable",                                    m_wcgChromaQpControl.enabled,                     false, "1: Enable the WCG PPS chroma modulation scheme. 0 (default) disabled")
  ("WCGPPSCbQpScale",                                 m_wcgChromaQpControl.chromaCbQpScale,               1.0, "WCG PPS Chroma Cb QP Scale")
  ("WCGPPSCrQpScale",                                 m_wcgChromaQpControl.chromaCrQpScale,               1.0, "WCG PPS Chroma Cr QP Scale")
  ("WCGPPSChromaQpScale",                             m_wcgChromaQpControl.chromaQpScale,                 0.0, "WCG PPS Chroma QP Scale")
  ("WCGPPSChromaQpOffset",                            m_wcgChromaQpControl.chromaQpOffset,                0.0, "WCG PPS Chroma QP Offset")
#endif
#if W0038_CQP_ADJ
  ("SliceChromaQPOffsetPeriodicity",                  m_sliceChromaQpOffsetPeriodicity,                    0u, "Used in conjunction with Slice Cb/Cr QpOffsetIntraOrPeriodic. Use 0 (default) to disable periodic nature.")
  ("SliceCbQpOffsetIntraOrPeriodic",                  m_sliceChromaQpOffsetIntraOrPeriodic[0],              0, "Chroma Cb QP Offset at slice level for I slice or for periodic inter slices as defined by SliceChromaQPOffsetPeriodicity. Replaces offset in the GOP table.")
  ("SliceCrQpOffsetIntraOrPeriodic",                  m_sliceChromaQpOffsetIntraOrPeriodic[1],              0, "Chroma Cr QP Offset at slice level for I slice or for periodic inter slices as defined by SliceChromaQPOffsetPeriodicity. Replaces offset in the GOP table.")
#endif
  ("CbQpOffsetList",                                  cfg_cbQpOffsetList,                  cfg_cbQpOffsetList, "Chroma Cb QP offset list for local adjustment")
  ("CrQpOffsetList",                                  cfg_crQpOffsetList,                  cfg_crQpOffsetList, "Chroma Cb QP offset list for local adjustment")
  ("CbCrQpOffsetList",                                cfg_cbCrQpOffsetList,              cfg_cbCrQpOffsetList, "Chroma joint Cb-Cr QP offset list for local adjustment")

  ("AdaptiveQP,-aq",                                  m_bUseAdaptiveQP,                                 false, "QP adaptation based on a psycho-visual model")
  ("MaxQPAdaptationRange,-aqr",                       m_iQPAdaptationRange,                                 6, "QP adaptation range")
#if ENABLE_QPA
  ("PerceptQPA,-qpa",                                 m_bUsePerceptQPA,                                 false, "perceptually motivated input-adaptive QP modification (default: 0 = off, ignored if -aq is set)")
  ("WPSNR,-wpsnr",                                    m_bUseWPSNR,                                      false, "output perceptually weighted peak SNR (WPSNR) instead of PSNR")
#endif
  ("dQPFile,m",                                       m_dQPFileName,                               std::string(""), "dQP file name")
  ("RDOQ",                                            m_useRDOQ,                                         true)
  ("RDOQTS",                                          m_useRDOQTS,                                       true)
  ("SelectiveRDOQ",                                   m_useSelectiveRDOQ,                               false, "Enable selective RDOQ")
  ("DPF",                                             m_dpfEnabled,                                     false, "Distortion Propagation Factor, CTU-Level Lagrange Multiplier and QP Adaptation")
  ("DPFKeyLength",                                    m_dpfKeyLen,                                   8, "DPF Propagation Length for key frames")
  ("DPFNonkeyLength",                                 m_dpfNonkeyLen,                                0, "DPF Propagation Length for non-key frames")

  // Deblocking filter parameters
  ("DeblockingFilterDisable",                         m_deblockingFilterDisable,                        false)
  ("DeblockingFilterOffsetInPPS",                     m_deblockingFilterOffsetInPPS,                     true)
  ("DeblockingFilterBetaOffset_div2",                 m_deblockingFilterBetaOffsetDiv2,                     0)
  ("DeblockingFilterTcOffset_div2",                   m_deblockingFilterTcOffsetDiv2,                       0)
  ("DeblockingFilterCbBetaOffset_div2",               m_deblockingFilterCbBetaOffsetDiv2,                   0)
  ("DeblockingFilterCbTcOffset_div2",                 m_deblockingFilterCbTcOffsetDiv2,                     0)
  ("DeblockingFilterCrBetaOffset_div2",               m_deblockingFilterCrBetaOffsetDiv2,                   0)
  ("DeblockingFilterCrTcOffset_div2",                 m_deblockingFilterCrTcOffsetDiv2,                     0)
  ("DeblockingFilterMetric",                          m_deblockingFilterMetric,                             0)
  // Coding tools
  ("ReconBasedCrossCPredictionEstimate",              m_reconBasedCrossCPredictionEstimate,             false, "When determining the alpha value for cross-component prediction, use the decoded residual rather than the pre-transform encoder-side residual")
  ("TransformSkip",                                   m_useTransformSkip,                               false, "Intra transform skipping")
  ("TransformSkipFast",                               m_useTransformSkipFast,                           false, "Fast encoder search for transform skipping, winner takes it all mode.")
  ("TransformSkipLog2MaxSize",                        m_log2MaxTransformSkipBlockSize,                     5U, "Specify transform-skip maximum size. Minimum 2, Maximum 5. (not valid in V1 profiles)")
  ("ChromaTS",                                        m_useChromaTS,                                    false, "Enable encoder search of chromaTS")
  ("BDPCM",                                           m_useBDPCM,                                       false, "BDPCM (0:off, 1:luma and chroma)")
  ("ISPFast",                                         m_useFastISP,                                     false, "Fast encoder search for ISP")
  ("ResidualRotation",                                m_transformSkipRotationEnabledFlag,               false, "Enable rotation of transform-skipped and transquant-bypassed TUs through 180 degrees prior to entropy coding (not valid in V1 profiles)")
  ("SingleSignificanceMapContext",                    m_transformSkipContextEnabledFlag,                false, "Enable, for transform-skipped and transquant-bypassed TUs, the selection of a single significance map context variable for all coefficients (not valid in V1 profiles)")
  ("ExtendedRiceRRC",                                 m_rrcRiceExtensionEnableFlag,                     false, "Enable the extention of the Golomb-Rice parameter derivation for RRC")
  ("GolombRiceParameterAdaptation",                   m_persistentRiceAdaptationEnabledFlag,            false, "Enable the adaptation of the Golomb-Rice parameter over the course of each slice")
  ("AlignCABACBeforeBypass",                          m_cabacBypassAlignmentEnabledFlag,                false, "Align the CABAC engine to a defined fraction of a bit prior to coding bypass data. Must be 1 in high bit rate profile, 0 otherwise")
  ("SAO",                                             m_useSao,                                         true, "Enable Sample Adaptive Offset")
  ("SaoTrueOrg",                                      m_saoTrueOrg,                                     false, "Using true original samples for SAO optimization when MCTF is enabled\n")
  ("TestSAODisableAtPictureLevel",                    m_bTestSAODisableAtPictureLevel,                  false, "Enables the testing of disabling SAO at the picture level after having analysed all blocks")
  ("SaoEncodingRate",                                 m_saoEncodingRate,                                 0.75, "When >0 SAO early picture termination is enabled for luma and chroma")
  ("SaoEncodingRateChroma",                           m_saoEncodingRateChroma,                            0.5, "The SAO early picture termination rate to use for chroma (when m_SaoEncodingRate is >0). If <=0, use results for luma")
  ("MaxNumOffsetsPerPic",                             m_maxNumOffsetsPerPic,                             2048, "Max number of SAO offset per picture (Default: 2048)")
  ("SAOLcuBoundary",                                  m_saoCtuBoundary,                                 false, "0: right/bottom CTU boundary areas skipped from SAO parameter estimation, 1: non-deblocked pixels are used for those areas")
  ("SAOGreedyEnc",                                    m_saoGreedyMergeEnc,                              false, "SAO greedy merge encoding algorithm")
  ("EnablePicPartitioning",                           m_picPartitionFlag,                               false, "Enable picture partitioning (0: single tile, single slice, 1: multiple tiles/slices can be used)")
  ("MixedLossyLossless",                              m_mixedLossyLossless,                                  false, "Enable encoder to encode mixed lossy/lossless coding ")
  ("SliceLosslessArray",                              cfgSliceLosslessArray, cfgSliceLosslessArray, " Lossless slice array Last lossless flag in the  list will be repeated uniformly to cover any remaining slice")
  ("TileColumnWidthArray",                            cfgTileColumnWidth,                  cfgTileColumnWidth, "Tile column widths in units of CTUs. Last column width in list will be repeated uniformly to cover any remaining picture width")
  ("TileRowHeightArray",                              cfgTileRowHeight,                      cfgTileRowHeight, "Tile row heights in units of CTUs. Last row height in list will be repeated uniformly to cover any remaining picture height")
  ("RasterScanSlices",                                m_rasterSliceFlag,                                false, "Indicates if using raster-scan or rectangular slices (0: rectangular, 1: raster-scan)")
  ("RectSlicePositions",                              cfgRectSlicePos,                        cfgRectSlicePos, "Rectangular slice positions. List containing pairs of top-left CTU RS address followed by bottom-right CTU RS address")
  ("RectSliceFixedWidth",                             m_rectSliceFixedWidth,                                0, "Fixed rectangular slice width in units of tiles (0: disable this feature and use RectSlicePositions instead)")
  ("RectSliceFixedHeight",                            m_rectSliceFixedHeight,                               0, "Fixed rectangular slice height in units of tiles (0: disable this feature and use RectSlicePositions instead)")
  ("RasterSliceSizes",                                cfgRasterSliceSize,                  cfgRasterSliceSize, "Raster-scan slice sizes in units of tiles. Last size in list will be repeated uniformly to cover any remaining tiles in the picture")
  ("DisableLoopFilterAcrossTiles",                    m_disableLFCrossTileBoundaryFlag,                 false, "Loop filtering applied across tile boundaries or not (0: filter across tile boundaries  1: do not filter across tile boundaries)")
  ("DisableLoopFilterAcrossSlices",                   m_disableLFCrossSliceBoundaryFlag,                false, "Loop filtering applied across slice boundaries or not (0: filter across slice boundaries 1: do not filter across slice boundaries)")
  ("FastUDIUseMPMEnabled",                            m_bFastUDIUseMPMEnabled,                           true, "If enabled, adapt intra direction search, accounting for MPM")
  ("FastMEForGenBLowDelayEnabled",                    m_bFastMEForGenBLowDelayEnabled,                   true, "If enabled use a fast ME for generalised B Low Delay slices")
  ("WeightedPredP,-wpP",                              m_useWeightedPred,                                false, "Use weighted prediction in P slices")
  ("WeightedPredB,-wpB",                              m_useWeightedBiPred,                              false, "Use weighted (bidirectional) prediction in B slices")
  ("WeightedPredMethod,-wpM",                         tmpWeightedPredictionMethod, int(WP_PER_PICTURE_WITH_SIMPLE_DC_COMBINED_COMPONENT), "Weighted prediction method")
  ("Log2ParallelMergeLevel",                          m_log2ParallelMergeLevel,                            2u, "Parallel merge estimation region")
  ("WaveFrontSynchro",                                m_entropyCodingSyncEnabledFlag,                   false, "0: entropy coding sync disabled; 1 entropy coding sync enabled")
  ("EntryPointsPresent",                              m_entryPointPresentFlag,                           true, "0: entry points is not present; 1 entry points may be present in slice header")
  ("ScalingList",                                     m_useScalingListId,                    SCALING_LIST_OFF, "0/off: no scaling list, 1/default: default scaling lists, 2/file: scaling lists specified in ScalingListFile")
  ("ScalingListFile",                                 m_scalingListFileName,                       std::string(""), "Scaling list file name. Use an empty string to produce help.")
  ("DisableScalingMatrixForLFNST",                    m_disableScalingMatrixForLfnstBlks,                true, "Disable scaling matrices, when enabled, for LFNST-coded blocks")
  ("DisableScalingMatrixForAlternativeColourSpace",   m_disableScalingMatrixForAlternativeColourSpace,  false, "Disable scaling matrices when the colour space is not equal to the designated colour space of scaling matrix")
  ("ScalingMatrixDesignatedColourSpace",              m_scalingMatrixDesignatedColourSpace,              true, "Indicates if the designated colour space of scaling matrices is equal to the original colour space")
  ("DepQuant",                                        m_depQuantEnabledFlag,                                          true, "Enable  dependent quantization (Default: 1)" )
  ("SignHideFlag,-SBH",                               m_signDataHidingEnabledFlag,                                    false,  "Enable sign hiding" )
  ("MaxNumMergeCand",                                 m_maxNumMergeCand,                                   5u, "Maximum number of merge candidates")
  ("MaxNumAffineMergeCand",                           m_maxNumAffineMergeCand,                             5u, "Maximum number of affine merge candidates")
  ("MaxNumGeoCand",                                   m_maxNumGeoCand,                                     5u, "Maximum number of geometric partitioning mode candidates")
  ("MaxNumIBCMergeCand",                              m_maxNumIBCMergeCand,                                6u, "Maximum number of IBC merge candidates")
    /* Misc. */
  ("SEIDecodedPictureHash,-dph",                      tmpDecodedPictureHashSEIMappedType,                   0, "Control generation of decode picture hash SEI messages\n"
                                                                                                               "\t3: checksum\n"
                                                                                                               "\t2: CRC\n"
                                                                                                               "\t1: use MD5\n"
                                                                                                               "\t0: disable")
  ("SubpicDecodedPictureHash",                        tmpSubpicDecodedPictureHashMappedType,                0, "Control generation of decode picture hash SEI messages for each subpicture\n"
                                                                                                               "\t3: checksum\n"
                                                                                                               "\t2: CRC\n"
                                                                                                               "\t1: use MD5\n"
                                                                                                               "\t0: disable")
  ("TMVPMode",                                        m_TMVPModeId,                                         1, "TMVP mode 0: TMVP disable for all slices. 1: TMVP enable for all slices (default) 2: TMVP enable for certain slices only")
  ("SliceLevelRpl",                                   m_sliceLevelRpl,                                   true, "Code reference picture lists in slice headers rather than picture header.")
  ("SliceLevelDblk",                                  m_sliceLevelDblk,                                  true, "Code deblocking filter parameters in slice headers rather than picture header.")
  ("SliceLevelSao",                                   m_sliceLevelSao,                                   true, "Code SAO parameters in slice headers rather than picture header.")
  ("SliceLevelAlf",                                   m_sliceLevelAlf,                                   true, "Code ALF parameters in slice headers rather than picture header.")
  ("SliceLevelWeightedPrediction",                    m_sliceLevelWp,                                    true, "Code weighted prediction parameters in slice headers rather than picture header.")
  ("SliceLevelDeltaQp",                               m_sliceLevelDeltaQp,                               true, "Code delta Qp in slice headers rather than picture header.")
  ("FEN",                                             tmpFastInterSearchMode,   int(FASTINTERSEARCH_DISABLED), "fast encoder setting")
  ("ECU",                                             m_bUseEarlyCU,                                    false, "Early CU setting")
  ("FDM",                                             m_useFastDecisionForMerge,                         true, "Fast decision for Merge RD Cost")
  ("ESD",                                             m_useEarlySkipDetection,                          false, "Early SKIP detection setting")
  ( "RateControl",                                    m_rcEnableRateControl,                            false, "Rate control: enable rate control" )
  ( "TargetBitrate",                                  m_rcTargetBitrate,                                    0, "Rate control: target bit-rate" )
  ( "KeepHierarchicalBit",                            m_rcKeepHierarchicalBit,                              0, "Rate control: 0: equal bit allocation; 1: fixed ratio bit allocation; 2: adaptive ratio bit allocation" )
  ( "LCULevelRateControl",                            m_rcCtuLevelRateControl,                                    true, "Rate control: true: CTU level RC; false: picture level RC" )
  ( "RCLCUSeparateModel",                             m_rcUseCtuSeparateModel,                           true, "Rate control: use CTU level separate R-lambda model" )
  ( "InitialQP",                                      m_rcInitialQp,                                        0, "Rate control: initial QP" )
  ( "RCForceIntraQP",                                 m_rcForceIntraQp,                                 false, "Rate control: force intra QP to be equal to initial QP" )
  ( "RCCpbSaturation",                                m_rcCpbSaturationEnabled,                         false, "Rate control: enable target bits saturation to avoid CPB overflow and underflow" )
  ( "RCCpbSize",                                      m_rcCpbSize,                                         0u, "Rate control: CPB size" )
  ( "RCInitialCpbFullness",                           m_rcInitialCpbFullness,                             0.9, "Rate control: initial CPB fullness" )
  ("CostMode",                                        m_costMode,                         COST_STANDARD_LOSSY, "Use alternative cost functions: choose between 'lossy', 'sequence_level_lossless', 'lossless' (which forces QP to " MACRO_TO_STRING(LOSSLESS_AND_MIXED_LOSSLESS_RD_COST_TEST_QP) ") and 'mixed_lossless_lossy' (which used QP'=" MACRO_TO_STRING(LOSSLESS_AND_MIXED_LOSSLESS_RD_COST_TEST_QP_PRIME) " for pre-estimates of transquant-bypass blocks).")
  ("TSRCdisableLL",                                   m_TSRCdisableLL,                                   true, "Disable TSRC for lossless coding" )
  ("RecalculateQPAccordingToLambda",                  m_recalculateQPAccordingToLambda,                 false, "Recalculate QP values according to lambda values. Do not suggest to be enabled in all intra case")
  ("HrdParametersPresent,-hrd",                       m_hrdParametersPresentFlag,                       false, "Enable generation of hrd_parameters()")
  ("VuiParametersPresent,-vui",                       m_vuiParametersPresentFlag,                       false, "Enable generation of vui_parameters()")
  ("WriteVuiHrdFromY4m",                              m_writeVuiHrdFromY4m,                              true, "Allow writing VUI and HRD information from input Y4M file")
  ("SamePicTimingInAllOLS",                           m_samePicTimingInAllOLS,                          true, "Indicates that the same picture timing SEI message is used in all OLS")
  ("AspectRatioInfoPresent",                          m_aspectRatioInfoPresentFlag,                     false, "Signals whether aspect_ratio_idc is present")
  ("AspectRatioIdc",                                  m_aspectRatioIdc,                                     0, "aspect_ratio_idc")
  ("SarWidth",                                        m_sarWidth,                                           0, "horizontal size of the sample aspect ratio")
  ("SarHeight",                                       m_sarHeight,                                          0, "vertical size of the sample aspect ratio")
  ("ColourDescriptionPresent",                        m_colourDescriptionPresentFlag,                   false, "Signals whether colour_primaries, transfer_characteristics and matrix_coefficients are present")
  ("ColourPrimaries",                                 m_colourPrimaries,                                    2, "Indicates chromaticity coordinates of the source primaries")
  ("TransferCharacteristics",                         m_transferCharacteristics,                            2, "Indicates the opto-electronic transfer characteristics of the source")
  ("MatrixCoefficients",                              m_matrixCoefficients,                                 2, "Describes the matrix coefficients used in deriving luma and chroma from RGB primaries")
  ("ProgressiveSource",                               m_progressiveSourceFlag,                          false, "Indicate that source is progressive")
  ("InterlacedSource",                                m_interlacedSourceFlag,                           false, "Indicate that source is interlaced")
  ("NonPackedSourceConstraintFlag",                   m_nonPackedConstraintFlag,                        false, "Indicate that source does not contain frame packing")
  ("NonProjectedConstraintFlag",                      m_nonProjectedConstraintFlag,                     false, "Indicate that the bitstream contains projection SEI messages")
  ("ChromaLocInfoPresent",                            m_chromaLocInfoPresentFlag,                       false, "Signals whether chroma_sample_loc_type_top_field and chroma_sample_loc_type_bottom_field are present")
  ("ChromaSampleLocTypeTopField",                     chromaSampleLocTypeTopField,    static_cast<int>(Chroma420LocType::UNSPECIFIED), "Specifies the location of chroma samples for top field")
  ("ChromaSampleLocTypeBottomField",                  chromaSampleLocTypeBottomField, static_cast<int>(Chroma420LocType::UNSPECIFIED), "Specifies the location of chroma samples for bottom field")
  ("ChromaSampleLocType",                             chromaSampleLocType,            static_cast<int>(Chroma420LocType::UNSPECIFIED), "Specifies the location of chroma samples for progressive content")
  ("OverscanInfoPresent",                             m_overscanInfoPresentFlag,                        false, "Indicates whether conformant decoded pictures are suitable for display using overscan\n")
  ("OverscanAppropriate",                             m_overscanAppropriateFlag,                        false, "Indicates whether conformant decoded pictures are suitable for display using overscan\n")
  ("VideoFullRange",                                  m_videoFullRangeFlag,                             false, "Indicates the black level and range of luma and chroma signals");
  opts.addOptions()
  ("SEIBufferingPeriod",                              m_bufferingPeriodSEIEnabled,                      false, "Control generation of buffering period SEI messages")
  ("SEIPictureTiming",                                m_pictureTimingSEIEnabled,                        false, "Control generation of picture timing SEI messages")
  ("SEIDecodingUnitInfo",                             m_decodingUnitInfoSEIEnabled,                     false, "Control generation of decoding unit information SEI message.")
  ("SEIScalableNesting",                              m_scalableNestingSEIEnabled,                      false, "Control generation of scalable nesting SEI messages")
  ("SEIFrameFieldInfo",                               m_frameFieldInfoSEIEnabled,                       false, "Control generation of frame field information SEI messages")
  ("SEIFramePacking",                                 m_framePackingSEIEnabled,                         false, "Control generation of frame packing SEI messages")
  ("SEIFramePackingType",                             m_framePackingSEIType,                                0, "Define frame packing arrangement\n"
                                                                                                               "\t3: side by side - frames are displayed horizontally\n"
                                                                                                               "\t4: top bottom - frames are displayed vertically\n"
                                                                                                               "\t5: frame alternation - one frame is alternated with the other")
  ("SEIFramePackingId",                               m_framePackingSEIId,                                  0, "Id of frame packing SEI message for a given session")
  ("SEIFramePackingQuincunx",                         m_framePackingSEIQuincunx,                            0, "Indicate the presence of a Quincunx type video frame")
  ("SEIFramePackingInterpretation",                   m_framePackingSEIInterpretation,                      0, "Indicate the interpretation of the frame pair\n"
                                                                                                               "\t0: unspecified\n"
                                                                                                               "\t1: stereo pair, frame0 represents left view\n"
                                                                                                               "\t2: stereo pair, frame0 represents right view")
  ("SEIDisplayOrientationEnabled",                    m_doSEIEnabled,                                   false, "Controls if display orientation packing SEI message enabled")
  ("SEIDisplayOrientationCancelFlag",                 m_doSEICancelFlag,                                 true, "Specifies the persistence of any previous display orientation SEI message in output order.")
  ("SEIDisplayOrientationPersistenceFlag",            m_doSEIPersistenceFlag,                           false, "Specifies the persistence of the display orientation packing SEI message for the current layer.")
  ("SEIDisplayOrientationTransformType",              m_doSEITransformType,                                 0, "specifies the rotation and mirroring to be applied to the picture.")
  ("SEIParameterSetsInclusionIndication",             m_parameterSetsInclusionIndicationSEIEnabled,      false, "Control generation of Parameter sets inclusion indication SEI messages")
  ("SEISelfContainedClvsFlag",                        m_selfContainedClvsFlag,                               0, "Self contained CLVS indication flag value")
  ("SEIMasteringDisplayColourVolume",                 m_masteringDisplay.colourVolumeSEIEnabled,         false, "Control generation of mastering display colour volume SEI messages")
  ("SEIMasteringDisplayMaxLuminance",                 m_masteringDisplay.maxLuminance,                  10000u, "Specifies the mastering display maximum luminance value in units of 1/10000 candela per square metre (32-bit code value)")
  ("SEIMasteringDisplayMinLuminance",                 m_masteringDisplay.minLuminance,                      0u, "Specifies the mastering display minimum luminance value in units of 1/10000 candela per square metre (32-bit code value)")
  ("SEIMasteringDisplayPrimaries",                    cfg_DisplayPrimariesCode,       cfg_DisplayPrimariesCode, "Mastering display primaries for all three colour planes in CIE xy coordinates in increments of 1/50000 (results in the ranges 0 to 50000 inclusive)")
  ("SEIMasteringDisplayWhitePoint",                   cfg_DisplayWhitePointCode,     cfg_DisplayWhitePointCode, "Mastering display white point CIE xy coordinates in normalised increments of 1/50000 (e.g. 0.333 = 16667)")
  ("SEIPreferredTransferCharacteristics",              m_preferredTransferCharacteristics,                   -1, "Value for the preferred_transfer_characteristics field of the Alternative transfer characteristics SEI which will override the corresponding entry in the VUI. If negative, do not produce the respective SEI message")

  ("SEIErpEnabled",                                   m_erpSEIEnabled,                                   false, "Control generation of equirectangular projection SEI messages")
("SEIErpCancelFlag", m_erpSEICancelFlag, true, "Indicate that equirectangular projection SEI message cancels the persistence or follows")
("SEIErpPersistenceFlag", m_erpSEIPersistenceFlag, false, "Specifies the persistence of the equirectangular projection SEI messages")
("SEIErpGuardBandFlag", m_erpSEIGuardBandFlag, false, "Indicate the existence of guard band areas in the constituent picture")
("SEIErpGuardBandType", m_erpSEIGuardBandType, 0u, "Indicate the type of the guard band")
("SEIErpLeftGuardBandWidth", m_erpSEILeftGuardBandWidth, 0u, "Indicate the width of the guard band on the left side of the constituent picture")
("SEIErpRightGuardBandWidth", m_erpSEIRightGuardBandWidth, 0u, "Indicate the width of the guard band on the right side of the constituent picture")
("SEISphereRotationEnabled", m_sphereRotationSEIEnabled, false, "Control generation of sphere rotation SEI messages")
("SEISphereRotationCancelFlag", m_sphereRotationSEICancelFlag, true, "Indicate that sphere rotation SEI message cancels the persistence or follows")
("SEISphereRotationPersistenceFlag", m_sphereRotationSEIPersistenceFlag, false, "Specifies the persistence of the sphere rotation SEI messages")
("SEISphereRotationYaw", m_sphereRotationSEIYaw, 0, "Specifies the value of the yaw rotation angle")
("SEISphereRotationPitch", m_sphereRotationSEIPitch, 0, "Specifies the value of the pitch rotation angle")
("SEISphereRotationRoll", m_sphereRotationSEIRoll, 0, "Specifies the value of the roll rotation angle")
("SEIOmniViewportEnabled", m_omniViewportSEIEnabled, false, "Control generation of omni viewport SEI messages")
("SEIOmniViewportId", m_omniViewportSEIId, 0u, "An identifying number that may be used to identify the purpose of the one or more recommended viewport regions")
("SEIOmniViewportCancelFlag", m_omniViewportSEICancelFlag, true, "Indicate that omni viewport SEI message cancels the persistence or follows")
("SEIOmniViewportPersistenceFlag", m_omniViewportSEIPersistenceFlag, false, "Specifies the persistence of the omni viewport SEI messages")
("SEIOmniViewportCntMinus1", m_omniViewportSEICntMinus1, 0u, "specifies the number of recommended viewport regions minus 1")
("SEIOmniViewportAzimuthCentre", cfg_omniViewportSEIAzimuthCentre, cfg_omniViewportSEIAzimuthCentre, "Indicate the centre of the i-th recommended viewport region")
("SEIOmniViewportElevationCentre", cfg_omniViewportSEIElevationCentre, cfg_omniViewportSEIElevationCentre, "Indicate the centre of the i-th recommended viewport region")
("SEIOmniViewportTiltCentre", cfg_omniViewportSEITiltCentre, cfg_omniViewportSEITiltCentre, "Indicates the tilt angle of the i-th recommended viewport region")
("SEIOmniViewportHorRange", cfg_omniViewportSEIHorRange, cfg_omniViewportSEIHorRange, "Indicates the azimuth range of the i-th recommended viewport region")
("SEIOmniViewportVerRange", cfg_omniViewportSEIVerRange, cfg_omniViewportSEIVerRange, "Indicates the elevation range of the i-th recommended viewport region")
("SEIRwpEnabled", m_rwpSEIEnabled, false, "Controls if region-wise packing SEI message enabled")
("SEIRwpCancelFlag", m_rwpSEIRwpCancelFlag, true, "Specifies the persistence of any previous region-wise packing SEI message in output order.")
("SEIRwpPersistenceFlag", m_rwpSEIRwpPersistenceFlag, false, "Specifies the persistence of the region-wise packing SEI message for the current layer.")
("SEIRwpConstituentPictureMatchingFlag", m_rwpSEIConstituentPictureMatchingFlag, false, "Specifies the information in the SEI message apply individually to each constituent picture or to the projected picture.")
("SEIRwpNumPackedRegions", m_rwpSEINumPackedRegions, 0, "specifies the number of packed regions when constituent picture matching flag is equal to 0.")
("SEIRwpProjPictureWidth", m_rwpSEIProjPictureWidth, 0, "Specifies the width of the projected picture.")
("SEIRwpProjPictureHeight", m_rwpSEIProjPictureHeight, 0, "Specifies the height of the projected picture.")
("SEIRwpPackedPictureWidth", m_rwpSEIPackedPictureWidth, 0, "specifies the width of the packed picture.")
("SEIRwpPackedPictureHeight", m_rwpSEIPackedPictureHeight, 0, "Specifies the height of the packed picture.")
("SEIRwpTransformType", cfg_rwpSEIRwpTransformType, cfg_rwpSEIRwpTransformType, "specifies the rotation and mirroring to be applied to the i-th packed region.")
("SEIRwpGuardBandFlag", cfg_rwpSEIRwpGuardBandFlag, cfg_rwpSEIRwpGuardBandFlag, "specifies the existence of guard band in the i-th packed region.")
("SEIRwpProjRegionWidth", cfg_rwpSEIProjRegionWidth, cfg_rwpSEIProjRegionWidth, "specifies the width of the i-th projected region.")
("SEIRwpProjRegionHeight", cfg_rwpSEIProjRegionHeight, cfg_rwpSEIProjRegionHeight, "specifies the height of the i-th projected region.")
("SEIRwpProjRegionTop", cfg_rwpSEIRwpSEIProjRegionTop, cfg_rwpSEIRwpSEIProjRegionTop, "specifies the top sample row of the i-th projected region.")
("SEIRwpProjRegionLeft", cfg_rwpSEIProjRegionLeft, cfg_rwpSEIProjRegionLeft, "specifies the left-most sample column of the i-th projected region.")
("SEIRwpPackedRegionWidth", cfg_rwpSEIPackedRegionWidth, cfg_rwpSEIPackedRegionWidth, "specifies the width of the i-th packed region.")
("SEIRwpPackedRegionHeight", cfg_rwpSEIPackedRegionHeight, cfg_rwpSEIPackedRegionHeight, "specifies the height of the i-th packed region.")
("SEIRwpPackedRegionTop", cfg_rwpSEIPackedRegionTop, cfg_rwpSEIPackedRegionTop, "specifies the top luma sample row of the i-th packed region.")
("SEIRwpPackedRegionLeft", cfg_rwpSEIPackedRegionLeft, cfg_rwpSEIPackedRegionLeft, "specifies the left-most luma sample column of the i-th packed region.")
("SEIRwpLeftGuardBandWidth", cfg_rwpSEIRwpLeftGuardBandWidth, cfg_rwpSEIRwpLeftGuardBandWidth, "specifies the width of the guard band on the left side of the i-th packed region.")
("SEIRwpRightGuardBandWidth", cfg_rwpSEIRwpRightGuardBandWidth, cfg_rwpSEIRwpRightGuardBandWidth, "specifies the width of the guard band on the right side of the i-th packed region.")
("SEIRwpTopGuardBandHeight", cfg_rwpSEIRwpTopGuardBandHeight, cfg_rwpSEIRwpTopGuardBandHeight, "specifies the height of the guard band above the i-th packed region.")
("SEIRwpBottomGuardBandHeight", cfg_rwpSEIRwpBottomGuardBandHeight, cfg_rwpSEIRwpBottomGuardBandHeight, "specifies the height of the guard band below the i-th packed region.")
("SEIRwpGuardBandNotUsedForPredFlag", cfg_rwpSEIRwpGuardBandNotUsedForPredFlag, cfg_rwpSEIRwpGuardBandNotUsedForPredFlag, "Specifies if the guard bands is used in the inter prediction process.")
("SEIRwpGuardBandType", cfg_rwpSEIRwpGuardBandType, cfg_rwpSEIRwpGuardBandType, "Specifies the type of the guard bands for the i-th packed region.")
("SEIGcmpEnabled", m_gcmpSEIEnabled, false, "Control generation of generalized cubemap projection SEI messages")
("SEIGcmpCancelFlag", m_gcmpSEICancelFlag, true, "Indicate that generalized cubemap projection SEI message cancels the persistence or follows")
("SEIGcmpPersistenceFlag", m_gcmpSEIPersistenceFlag, false, "Specifies the persistence of the generalized cubemap projection SEI messages")
("SEIGcmpPackingType", m_gcmpSEIPackingType, 0u, "Specifies the packing type")
("SEIGcmpMappingFunctionType", m_gcmpSEIMappingFunctionType, 0u, "Specifies the mapping function used to adjust the sample locations of the cubemap projection")
("SEIGcmpFaceIndex", cfg_gcmpSEIFaceIndex, cfg_gcmpSEIFaceIndex, "Specifies the face index for the i-th face")
("SEIGcmpFaceRotation", cfg_gcmpSEIFaceRotation, cfg_gcmpSEIFaceRotation, "Specifies the rotation to be applied to the i-th face")
("SEIGcmpFunctionCoeffU", cfg_gcmpSEIFunctionCoeffU, cfg_gcmpSEIFunctionCoeffU, "Specifies the coefficient used in the cubemap mapping function of the u-axis of the i-th face")
("SEIGcmpFunctionUAffectedByVFlag", cfg_gcmpSEIFunctionUAffectedByVFlag, cfg_gcmpSEIFunctionUAffectedByVFlag, "Specifies whether the cubemap mapping function of the u-axis refers to the v position of the sample location")
("SEIGcmpFunctionCoeffV", cfg_gcmpSEIFunctionCoeffV, cfg_gcmpSEIFunctionCoeffV, "Specifies the coefficient used in the cubemap mapping function of the v-axis of the i-th face")
("SEIGcmpFunctionVAffectedByUFlag", cfg_gcmpSEIFunctionVAffectedByUFlag, cfg_gcmpSEIFunctionVAffectedByUFlag, "Specifies whether the cubemap mapping function of the v-axis refers to the u position of the sample location")
("SEIGcmpGuardBandFlag", m_gcmpSEIGuardBandFlag, false, "Indicate the existence of guard band areas in the picture")
("SEIGcmpGuardBandType", m_gcmpSEIGuardBandType, 0u, "Indicate the type of the guard bands")
("SEIGcmpGuardBandBoundaryExteriorFlag", m_gcmpSEIGuardBandBoundaryExteriorFlag, false, "Indicate whether face boundaries contain guard bands")
("SEIGcmpGuardBandSamplesMinus1", m_gcmpSEIGuardBandSamplesMinus1, 0u, "Specifies the number of guard band samples minus1 used in the cubemap projected picture")
("SEISubpicLevelInfoEnabled", m_cfgSubpictureLevelInfoSEI.m_enabled, false, "Control generation of Subpicture Level Information SEI messages")
("SEISubpicLevelInfoRefLevels", cfg_sliRefLevels, cfg_sliRefLevels, "List of reference levels for Subpicture Level Information SEI messages")
("SEISubpicLevelInfoExplicitFraction", m_cfgSubpictureLevelInfoSEI.m_explicitFraction, false, "Enable sending of explicit fractions in Subpicture Level Information SEI messages")
("SEISubpicLevelInfoNumSubpics", m_cfgSubpictureLevelInfoSEI.m_numSubpictures, 1, "Number of subpictures for Subpicture Level Information SEI messages")
("SEIAnnotatedRegionsFileRoot,-ar", m_arSEIFileRoot, std::string(""), "Annotated region SEI parameters root file name (wo num ext); only the file name base is to be added. Underscore and POC would be automatically addded to . E.g. \"-ar ar\" will search for files ar_0.txt, ar_1.txt, ...")
("SEIObjectMaskFileRoot,-omi", m_omiSEIFileRoot, std::string(""), "Object mask information SEI parameters root file name (wo num ext); only the file name base is to be added. Underscore and POC would be automatically added to . E.g. \"-omi omi\" will search for files omi_0.txt, omi_1.txt, ...")
("SEISubpicLevelInfoMaxSublayers", m_cfgSubpictureLevelInfoSEI.m_sliMaxSublayers, 1, "Number of sublayers for Subpicture Level Information SEI messages")
("SEISubpicLevelInfoSublayerInfoPresentFlag", m_cfgSubpictureLevelInfoSEI.hasSublayerInfo, false, "Enable sending of level information for all sublayers in Subpicture Level Information SEI messages")
("SEISubpicLevelInfoRefLevelFractions", cfg_sliFractions, cfg_sliFractions, "List of subpicture level fractions for Subpicture Level Information SEI messages")
("SEISubpicLevelInfoNonSubpicLayersFractions", cfg_sliNonSubpicLayersFractions, cfg_sliNonSubpicLayersFractions, "List of level fractions for non-subpicture layers in Subpicture Level Information SEI messages")
("SEISampleAspectRatioInfo", m_sampleAspectRatioInfoSEIEnabled, false, "Control generation of Sample Aspect Ratio Information SEI messages")
("SEISARICancelFlag", m_sariCancelFlag, false, "Indicates that Sample Aspect Ratio Information SEI message cancels the persistence or follows")
("SEISARIPersistenceFlag", m_sariPersistenceFlag, true, "Specifies the persistence of the Sample Aspect Ratio Information SEI message")
("SEISARIAspectRatioIdc", m_sariAspectRatioIdc, 0, "Specifies the Sample Aspect Ratio IDC of Sample Aspect Ratio Information SEI messages")
("SEISARISarWidth", m_sariSarWidth, 0, "Specifies the Sample Aspect Ratio Width of Sample Aspect Ratio Information SEI messages, if extended SAR is chosen.")
("SEISARISarHeight", m_sariSarHeight, 0, "Specifies the Sample Aspect Ratio Height of Sample Aspect Ratio Information SEI messages, if extended SAR is chosen.")
("SEIPhaseIndicationFullResolution", m_phaseIndicationSEIEnabledFullResolution, false, "Control generation of Phase Indication SEI messages for full resolution pictures.")
("SEIPIHorPhaseNumFullResolution", m_piHorPhaseNumFullResolution, 0, "Specifies the Horizontal Phase Numerator of Phase Indication SEI messages for full resolution pictures.")
("SEIPIHorPhaseDenMinus1FullResolution", m_piHorPhaseDenMinus1FullResolution, 0, "Specifies the Horizontal Phase Denominator minus 1 of Phase Indication SEI messages for full resolution pictures.")
("SEIPIVerPhaseNumFullResolution", m_piVerPhaseNumFullResolution, 0, "Specifies the Vertical Phase Numerator of Phase Indication SEI messages for full resolution pictures.")
("SEIPIVerPhaseDenMinus1FullResolution", m_piVerPhaseDenMinus1FullResolution, 0, "Specifies the Vertical Phase Denominator minus 1 of Phase Indication SEI messages for full resolution pictures.")
("SEIPhaseIndicationReducedResolution", m_phaseIndicationSEIEnabledReducedResolution, false, "Control generation of Phase Indication SEI messages for reduced resolution pictures.")
("SEIPIHorPhaseNumReducedResolution", m_piHorPhaseNumReducedResolution, 0, "Specifies the Horizontal Phase Numerator of Phase Indication SEI messages for reduced resolution pictures.")
("SEIPIHorPhaseDenMinus1ReducedResolution", m_piHorPhaseDenMinus1ReducedResolution, 0, "Specifies the Horizontal Phase Denominator minus 1 of Phase Indication SEI messages for reduced resolution pictures.")
("SEIPIVerPhaseNumReducedResolution", m_piVerPhaseNumReducedResolution, 0, "Specifies the Vertical Phase Numerator of Phase Indication SEI messages for reduced resolution pictures.")
("SEIPIVerPhaseDenMinus1ReducedResolution", m_piVerPhaseDenMinus1ReducedResolution, 0, "Specifies the Vertical Phase Denominator minus 1 of Phase Indication SEI messages for reduced resolution pictures.")
("MCTSEncConstraint", m_MCTSEncConstraint, false, "For MCTS, constrain motion vectors at tile boundaries")
("SEIShutterIntervalEnabled", m_siiSEIEnabled, false, "Controls if shutter interval information SEI message is enabled")
("SEISiiTimeScale", m_siiSEITimeScale, 27000000u, "Specifies sii_time_scale")
("SEISiiInputNumUnitsInShutterInterval", cfg_siiSEIInputNumUnitsInSI, cfg_siiSEIInputNumUnitsInSI, "Specifies sub_layer_num_units_in_shutter_interval")

("SEISourcePictureTimingInfo", m_sptiSEIEnabled, false, "Controls if source picture timing information SEI message is enabled")
("SEISPTISourceTimingEqualsOutputTimingFlag", m_sptiSourceTimingEqualsOutputTimingFlag, true, "Indicates the timing of source pictures is the same as the timing of corresponding decoded output pictures")
("SEISPTISourceType", m_sptiSourceType, 0u, "Indicates the timing relationship between source pictures and corresponding decoded output pictures.")
("SEISPTITimeScale", m_sptiTimeScale, 27000000u, "Specifies the number of time units that pass in one second.")
("SEISPTINumUnitsInElementalInterval", m_sptiNumUnitsInElementalInterval, 1080000u, "Specifies the number of time units of a clock operating at the frequency spti_time_scale Hz that corresponds to the indicated elemental source picture interval of consecutive pictures in output order in the CLVS.")
("SEISPTIDirectionFlag", m_sptiDirectionFlag, false, "Indicates the direction of the signalled source picture intervals.")
#if JVET_AJ0151_DSC_SEI
("SEIDSCEnabled", m_cfgDigitallySignedContentSEI.enabled, false, "Control generation of Digitally Signed Content SEI messages")
("SEIDSCHashMethod", m_cfgDigitallySignedContentSEI.hashMethod, 0 , "Hash type to be used:\n"
                                                                     "\t0: SHA-1 (default)\n"
                                                                     "\t1: SHA-224\n"
                                                                     "\t2: SHA-256\n"
                                                                     "\t3: SHA-384\n"
                                                                     "\t4: SHA-512\n"
                                                                     "\t5: SHA-512/224\n"
                                                                     "\t6: SHA-512/256")
("SEIDSCSigningKeyFile", m_cfgDigitallySignedContentSEI.privateKeyFile, std::string("") , "(Private) signing key location for Digitally Signed Content SEI messages")
("SEIDSCVerificationKeyURI", m_cfgDigitallySignedContentSEI.publicKeyUri, std::string("") , "(Public) verification key URI for Digitally Signed Content SEI messages")
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
("SEIDSCImplicitAssociationModeFlag", m_cfgDigitallySignedContentSEI.implicitAssociationModeFlag, false , "Used to infer the verification substream ID")
#endif
("SEIDSCKeyIDEnabled", m_cfgDigitallySignedContentSEI.keyIdEnabled, false, "Enable using a key ID addition to URI of public key of Digitally Signed Content SEI messages")
("SEIDSCKeyID", m_cfgDigitallySignedContentSEI.keyId, 0 , "Public Key ID for Digitally Signed Content SEI messages (if enabled)")
#endif
#if ENABLE_TRACING
("TraceChannelsList", bTracingChannelsList, false, "List all available tracing channels")
("TraceRule", sTracingRule, std::string(""), "Tracing rule (ex: \"D_CABAC:poc==8\" or \"D_REC_CB_LUMA:poc==8\")")
("TraceFile", sTracingFile, std::string(""), "Tracing file")
#endif

("SEIEOIEnabled", m_eoiSEIEnabled, false, "Control use of the Encoder Optimization Information SEI")
("SEIEOICancelFlag", m_eoiSEICancelFlag, false, "Specifies that the persistence of the previous applied optimization")
("SEIEOIPersistenceFlag", m_eoiSEIPersistenceFlag, false, "Specifies the persistence of the optimization the current layer")
("SEIEOIForHumanViewingIdc", m_eoiSEIForHumanViewingIdc, 0u, "Indicates the level of optimization for human viewing")
("SEIEOIForMachineAnalysisIdc", m_eoiSEIForMachineAnalysisIdc, 0u, "Indicates the level of optimization for  machine analsysis")
("SEIEOIType", m_eoiSEIType, 0u, "Indicates the types of optimization method")
("SEIEOIObjectBasedIdc", m_eoiSEIObjectBasedIdc, 0u, "Indicates the type of object-based optimization")
("SEIEOIQuantThresholdDelta", m_eoiSEIQuantThresholdDelta, 0u, "Indicates the quantization parameter threshold determining areas classified to be outside the detected objects or to include one or more detected objects (0 = unknown or unspecified)")
("SEIEOIPicQuantObjectFlag", m_eoiSEIPicQuantObjectFlag, false, "Value of 1 indicates that areas with QP >= PicQuant + SEIEOIQuantThresholdDelta represent areas outside the detected objects. Value of 0 indicates that areas with QP <= PicQuant - SEIEOIQuantThresholdDelta represent areas that include objects")
("SEIEOITemporalResamplingTypeFlag", m_eoiSEITemporalResamplingTypeFlag, false, "specifies the type of the temporal resampling optimization.")
("SEIEOINumIntPics", m_eoiSEINumIntPics, 0u, "indicates that the count of pictures that the encoding system excluded or added between each pair of coded pictures in output order within the persistence of this SEI message is constant")
#if JVET_AJ0183_EOI_SEI_SRC_PIC_FLAG
("SEIEOISrcPicFlag", m_eoiSEISrcPicFlag, false, "Value of 1 specifies that the picture in the same access unit that contains the EOI SEI message is a source picture. Value of 0 provides no such indication.")
#endif
("SEIEOIOrigPicDimensionsFlag",m_eoiSEIOrigPicDimensionsFlag, false, "specifies if original source picture dimensions are present.")
#if JVET_AL0123_AL0310_EOI
("SEIEOIOrigPicWidthMinus1", m_eoiSEIOrigPicWidthMinus1, 0u, "indicates the width of the original source picture minus 1.")
("SEIEOIOrigPicHeightMinus1", m_eoiSEIOrigPicHeightMinus1, 0u, "indicates the height of the original source picture minus 1.")
("SEIEOISpatialHorResamplingTypeIdc", m_eoiSEISpatialHorResamplingTypeIdc, 0u, "specifies the type of the spatial horizontal resampling optimization.")
("SEIEOISpatialVerResamplingTypeIdc", m_eoiSEISpatialVerResamplingTypeIdc, 0u, "specifies the type of the spatial vertical resampling optimization.")
#else
("SEIEOIOrigPicWidth", m_eoiSEIOrigPicWidth, 0u, "indicates the width of the original source picture.")
("SEIEOIOrigPicHeight", m_eoiSEIOrigPicHeight, 0u, "indicates the height of the original source picture.")
("SEIEOISpatialResamplingTypeFlag", m_eoiSEISpatialResamplingTypeFlag, false, "specifies the type of the spatial resampling optimization.")
#endif
("SEIEOIPrivacyProtectionTypeIdc", m_eoiSEIPrivacyProtectionTypeIdc, 0u, "indicates the type of privacy protection optimization")
("SEIEOIPrivacyProtectedInfoType", m_eoiSEIPrivacyProtectedInfoType, 0u, "indicates the types of protected information")

// Modality Information SEI 
  ("SEIModalityInfoEnabled",                          m_miSEIEnabled,                                    false, "Control generation of Modality Information SEI messages")
  ("SEIMiCancelFlag",                                 m_miCancelFlag,                                    false, "Indicates that Modality Information SEI message cancels the persistence or follows")
  ("SEIMiPersistenceFlag",                            m_miPersistenceFlag,                                true, "Specifies the persistence of the Modality Information SEI message")
  ("SEIMiModalityType",                               m_miModalityType,                                      1, "Specifies the type of modality. 0: unspecified; 1: visible picture; 2: Infrared Picture; 3: Ultraviolet Picture")
  ("SEIMiSpectrumRangePresentFlag",                   m_miSpectrumRangePresentFlag,                      false, "Specifies the presence of the spectrum band of the optical radiation wavelength")
  ("SEIMiMinWavelengthMantissa",                      m_miMinWavelengthMantissa,                             0, "Specifies the mantissa part of the minimum wavelength indicating the spectral band of optical radiation")
  ("SEIMiMinWavelengthExponentPlus15",                m_miMinWavelengthExponentPlus15,                       0, "Specifies the exponent part of the minimum wavelength indicating the spectral band of optical radiation")
  ("SEIMiMaxWavelengthMantissa",                      m_miMaxWavelengthMantissa,                             0, "Specifies the mantissa part of the maximum wavelength indicating the spectral band of optical radiation")
  ("SEIMiMaxWavelengthExponentPlus15",                m_miMaxWavelengthExponentPlus15,                       0, "Specifies the exponent part of the maximum wavelength indicating the spectral band of optical radiation")


// film grain characteristics SEI
  ("SEIFGCEnabled",                                   m_fgcSEIEnabled,                                   false, "Control generation of the film grain characteristics SEI message")
  ("SEIFGCCancelFlag",                                m_fgcSEICancelFlag,                                 true, "Specifies the persistence of any previous film grain characteristics SEI message in output order.")
  ("SEIFGCPersistenceFlag",                           m_fgcSEIPersistenceFlag,                           false, "Specifies the persistence of the film grain characteristics SEI message for the current layer.")
  ("SEIFGCModelID",                                   m_fgcSEIModelID,                                      0u, "Specifies the film grain simulation model. 0: frequency filtering; 1: auto-regression.")
  ("SEIFGCSepColourDescPresentFlag",                  m_fgcSEISepColourDescPresentFlag,                  false, "Specifies the presence of a distinct colour space description for the film grain characteristics specified in the SEI message.")
  ("SEIFGCBlendingModeID",                            m_fgcSEIBlendingModeID,                               0u, "Specifies the blending mode used to blend the simulated film grain with the decoded images. 0: additive; 1: multiplicative.")
  ("SEIFGCLog2ScaleFactor",                           m_fgcSEILog2ScaleFactor,                              0u, "Specifies a scale factor used in the film grain characterization equations.")
  ("SEIFGCCompModelPresentComp0",                     m_fgcSEICompModelPresent[0],                       false, "Specifies the presence of film grain modelling on colour component 0.")
  ("SEIFGCCompModelPresentComp1",                     m_fgcSEICompModelPresent[1],                       false, "Specifies the presence of film grain modelling on colour component 1.")
  ("SEIFGCCompModelPresentComp2",                     m_fgcSEICompModelPresent[2],                       false, "Specifies the presence of film grain modelling on colour component 2.")
  ("SEIFGCAnalysisEnabled",                           m_fgcSEIAnalysisEnabled,                           false, "Control adaptive film grain parameter estimation - film grain analysis")
  ("SEIFGCExternalMask",                              m_fgcSEIExternalMask,                       std::string( "" ), "Read external file with mask for film grain analysis. If empty string, use internally calculated mask.")
  ("SEIFGCExternalDenoised",                          m_fgcSEIExternalDenoised,                   std::string( "" ), "Read external file with denoised sequence for film grain analysis. If empty string, use MCTF for denoising.")
  ("SEIFGCTemporalFilterPastRefs",                    m_fgcSEITemporalFilterPastRefs,          TF_DEFAULT_REFS, "Number of past references for temporal prefilter")
  ("SEIFGCTemporalFilterFutureRefs",                  m_fgcSEITemporalFilterFutureRefs,        TF_DEFAULT_REFS, "Number of future references for temporal prefilter")
  ("SEIFGCTemporalFilterStrengthFrame*",              m_fgcSEITemporalFilterStrengths, std::map<int, double>(), "Strength for every * frame in FGC-specific temporal filter, where * is an integer.")
  ("SEIFGCPerPictureSEI",                             m_fgcSEIPerPictureSEI,                             false, "Film Grain SEI is added for each picture as speciffied in RDD5 to ensure bit accurate synthesis in tricky mode")
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
// content light level SEI
  ("SEICLLEnabled",                                   m_cllSEIEnabled,                                   false, "Control generation of the content light level SEI message")
  ("SEICLLMaxContentLightLevel",                      m_cllSEIMaxContentLevel,                              0u, "When not equal to 0, specifies an upper bound on the maximum light level among all individual samples in a 4:4:4 representation "
                                                                                                                "of red, green, and blue colour primary intensities in the linear light domain for the pictures of the CLVS, "
                                                                                                                "in units of candelas per square metre.When equal to 0, no such upper bound is indicated.")
  ("SEICLLMaxPicAvgLightLevel",                       m_cllSEIMaxPicAvgLevel,                               0u, "When not equal to 0, specifies an upper bound on the maximum average light level among the samples in a 4:4:4 representation "
                                                                                                                "of red, green, and blue colour primary intensities in the linear light domain for any individual picture of the CLVS, "
                                                                                                                "in units of candelas per square metre.When equal to 0, no such upper bound is indicated.")
// ambient viewing environment SEI
  ("SEIAVEEnabled",                                   m_aveSEIEnabled,                                   false, "Control generation of the ambient viewing environment SEI message")
  ("SEIAVEAmbientIlluminance",                        m_aveSEIAmbientIlluminance,                      100000u, "Specifies the environmental illluminance of the ambient viewing environment in units of 1/10000 lux for the ambient viewing environment SEI message")
  ("SEIAVEAmbientLightX",                             m_aveSEIAmbientLightX,                            15635u, "Specifies the normalized x chromaticity coordinate of the environmental ambient light in the nominal viewing enviornment according to the CIE 1931 definition in units of 1/50000 lux for the ambient viewing enviornment SEI message")
  ("SEIAVEAmbientLightY",                             m_aveSEIAmbientLightY,                            16450u, "Specifies the normalized y chromaticity coordinate of the environmental ambient light in the nominal viewing enviornment according to the CIE 1931 definition in units of 1/50000 lux for the ambient viewing enviornment SEI message")
// colour tranform information SEI
  ("SEICTIEnabled",                                   m_ctiSEIEnabled,                                   false, "Control generation of the Colour transform information SEI message")
  ("SEICTIId",                                        m_ctiSEIId,                                           0u, "Id of the Colour transform information SEI message")
  ("SEICTISignalInfoFlag",                            m_ctiSEISignalInfoFlag,                            false, "indicates if signal information are present in the Colour transform information SEI message")
  ("SEICTIFullRangeFlag",                             m_ctiSEIFullRangeFlag,                             false, "specifies signal range after applying the Colour transform information SEI message")
  ("SEICTIPrimaries",                                 m_ctiSEIPrimaries,                                    0u, "indicates the signal primaries after applying the Colour transform information SEI message")
  ("SEICTITransferFunction",                          m_ctiSEITransferFunction,                             0u, "indicates the signal transfer function after applying the Colour transform information SEI message")
  ("SEICTIMatrixCoefs",                               m_ctiSEIMatrixCoefs,                                  0u, "indicates the signal matrix coefficients after applying the Colour transform information SEI message")
  ("SEICTICrossCompFlag",                             m_ctiSEICrossComponentFlag,                         true, "Specifies if cross-component transform mode is enabled in SEI CTI")
  ("SEICTICrossCompInferred",                         m_ctiSEICrossComponentInferred,                     true, "Specifies if cross-component transform LUT is inferred in SEI CTI")
  ("SEICTINbChromaLut",                               m_ctiSEINumberChromaLut,                              0u, "Specifies the number of chroma LUTs in SEI CTI")
  ("SEICTIChromaOffset",                              m_ctiSEIChromaOffset,                                  0, "Specifies the chroma offset of SEI CTI")
  ("SEICTILut0",                                      cfg_SEICTILut0,                           cfg_SEICTILut0, "slope values for component 0 of SEI CTI")
  ("SEICTILut1",                                      cfg_SEICTILut1,                           cfg_SEICTILut1, "slope values for component 1 of SEI CTI")
  ("SEICTILut2",                                      cfg_SEICTILut2,                           cfg_SEICTILut2, "slope values for component 2 of SEI CTI")
// content colour volume SEI
  ("SEICCVEnabled",                                   m_ccvSEIEnabled,                                   false, "Control generation of the Content Colour Volume SEI message")
  ("SEICCVCancelFlag",                                m_ccvSEICancelFlag,                                 true, "Specifies the persistence of any previous content colour volume SEI message in output order.")
  ("SEICCVPersistenceFlag",                           m_ccvSEIPersistenceFlag,                           false, "Specifies the persistence of the content colour volume SEI message for the current layer.")
  ("SEICCVPrimariesPresent",                          m_ccvSEIPrimariesPresentFlag,                       true, "Specifies whether the CCV primaries are present in the content colour volume SEI message.")
  ("m_ccvSEIPrimariesX0",                             m_ccvSEIPrimariesX[0],                             0.300, "Specifies the x coordinate of the first (green) primary for the content colour volume SEI message")
  ("m_ccvSEIPrimariesY0",                             m_ccvSEIPrimariesY[0],                             0.600, "Specifies the y coordinate of the first (green) primary for the content colour volume SEI message")
  ("m_ccvSEIPrimariesX1",                             m_ccvSEIPrimariesX[1],                             0.150, "Specifies the x coordinate of the second (blue) primary for the content colour volume SEI message")
  ("m_ccvSEIPrimariesY1",                             m_ccvSEIPrimariesY[1],                             0.060, "Specifies the y coordinate of the second (blue) primary for the content colour volume SEI message")
  ("m_ccvSEIPrimariesX2",                             m_ccvSEIPrimariesX[2],                             0.640, "Specifies the x coordinate of the third (red) primary for the content colour volume SEI message")
  ("m_ccvSEIPrimariesY2",                             m_ccvSEIPrimariesY[2],                             0.330, "Specifies the y coordinate of the third (red) primary for the content colour volume SEI message")
  ("SEICCVMinLuminanceValuePresent",                  m_ccvSEIMinLuminanceValuePresentFlag,               true, "Specifies whether the CCV min luminance value is present in the content colour volume SEI message")
  ("SEICCVMinLuminanceValue",                         m_ccvSEIMinLuminanceValue,                           0.0, "specifies the CCV min luminance value  in the content colour volume SEI message")
  ("SEICCVMaxLuminanceValuePresent",                  m_ccvSEIMaxLuminanceValuePresentFlag,               true, "Specifies whether the CCV max luminance value is present in the content colour volume SEI message")
  ("SEICCVMaxLuminanceValue",                         m_ccvSEIMaxLuminanceValue,                           0.1, "specifies the CCV max luminance value  in the content colour volume SEI message")
  ("SEICCVAvgLuminanceValuePresent",                  m_ccvSEIAvgLuminanceValuePresentFlag,               true, "Specifies whether the CCV avg luminance value is present in the content colour volume SEI message")
  ("SEICCVAvgLuminanceValue",                         m_ccvSEIAvgLuminanceValue,                          0.01, "specifies the CCV avg luminance value  in the content colour volume SEI message")
  // scalability dimension information SEI
  ("SEISDIEnabled",                                   m_sdiSEIEnabled,                          false, "Control generation of scalaibility dimension information SEI message")
  ("SEISDIMaxLayersMinus1",                           m_sdiSEIMaxLayersMinus1,                      0, "Specifies the maximum number of layers minus 1 in the current CVS")
  ("SEISDIMultiviewInfoFlag",                         m_sdiSEIMultiviewInfoFlag,                false, "Specifies the current CVS may have multiple views and the sdi_view_id_val[ ] syntax elements are present in the scalaibility dimension information SEI message")
  ("SEISDIAuxiliaryInfoFlag",                         m_sdiSEIAuxiliaryInfoFlag,                false, "Specifies that one or more layers in the current CVS may be auxiliary layers, which carry auxiliary information, and the sdi_aux_id[ ] syntax elements are present in the scalaibility dimension information SEI message")
  ("SEISDIViewIdLenMinus1",                           m_sdiSEIViewIdLenMinus1,                      0, "Specifies the length, in bits, of the sdi_view_id_val[ i ] syntax element minus 1 in the scalaibility dimension information SEI message")
  ("SEISDILayerId",                                   cfg_sdiSEILayerId,            cfg_sdiSEILayerId, "List of the layer identifiers that may be present in the scalaibility dimension information SEI message in the current CVS")
  ("SEISDIViewIdVal",                                 cfg_sdiSEIViewIdVal,        cfg_sdiSEIViewIdVal, "List of the view identifiers in the scalaibility dimension information SEI message")
  ("SEISDIAuxId",                                     cfg_sdiSEIAuxId,                cfg_sdiSEIAuxId, "List of the auxiliary identifiers in the scalaibility dimension information SEI message")
  ("SEISDINumAssociatedPrimaryLayersMinus1",          cfg_sdiSEINumAssociatedPrimaryLayersMinus1, cfg_sdiSEINumAssociatedPrimaryLayersMinus1, "List of the numbers of associated primary layers of i-th layer, which is an auxiliary layer.")
  ("SEISDIAssociatedPrimaryLayerIdx",                 cfg_sdiSEIAssociatedPrimaryLayerIdx, cfg_sdiSEIAssociatedPrimaryLayerIdx, "List of the layer index of the j-th associated primary layer of the i-th layer, which is an auxiliary layer. It is a 1-d list and the number of the associated primary layer of the i-th auxiliary layer is indicated by cfg_sdiSEINumAssociatedPrimaryLayersMinus1 and cfg_sdiSEIAuxId.")
  // multiview acquisition information SEI
  ("SEIMAIEnabled",                                   m_maiSEIEnabled,                                    false, "Control generation of multiview acquisition information SEI message")
  ("SEIMAIIntrinsicParamFlag",                        m_maiSEIIntrinsicParamFlag,                         false, "Specifies the presence of intrinsic camera parameters in the multiview acquisition information SEI message")
  ("SEIMAIExtrinsicParamFlag",                        m_maiSEIExtrinsicParamFlag,                         false, "Specifies the presence of extrinsic camera parameters in the multiview acquisition information SEI message")
  ("SEIMAINumViewsMinus1",                            m_maiSEINumViewsMinus1,                                 0, "Specifies the number of views minus 1 in the multiview acquisition information SEI message")
  ("SEIMAIIntrinsicParamsEqualFlag",                  m_maiSEIIntrinsicParamsEqualFlag,                   false, "Specifies the intrinsic camera parameters are equal for all cameras in the multiview acquisition information SEI message")
  ("SEIMAIPrecFocalLength",                           m_maiSEIPrecFocalLength,                                0, "Specifies the exponent of the maximum allowable truncation error for focal_length_x[i] and focal_length_y[i] in the multiview acquisition information SEI message")
  ("SEIMAIPrecPrincipalPoint",                        m_maiSEIPrecPrincipalPoint,                             0, "Specifies the exponent of the maximum allowable truncation error for principal_point_x[i] and principal_point_y[i] in the multiview acquisition information SEI message")
  ("SEIMAIPrecSkewFactor",                            m_maiSEIPrecSkewFactor,                                 0, "Specifies the exponent of the maximum allowable truncation error for skew factor in the multiview acquisition information SEI message")
  ("SEIMAISignFocalLengthX",                          cfg_maiSEISignFocalLengthX,    cfg_maiSEISignFocalLengthX, "List of the signs of the focal length of the camera in the horizontal direction in the multiview acquisition information SEI message")
  ("SEIMAIExponentFocalLengthX",                      cfg_maiSEIExponentFocalLengthX, cfg_maiSEIExponentFocalLengthX, "List of the exponent parts of the focal length of the camera in the horizontal direction. in the multiview acquisition information SEI message")
  ("SEIMAIMantissaFocalLengthX",                      cfg_maiSEIMantissaFocalLengthX, cfg_maiSEIMantissaFocalLengthX, "List of the mantissa parts of the focal length of the camera in the horizontal direction in the multiview acquisition information SEI message")
  ("SEIMAISignFocalLengthY",                          cfg_maiSEISignFocalLengthY,    cfg_maiSEISignFocalLengthY, "List of the signs of the focal length of the camera in the vertical direction in the multiview acquisition information SEI message")
  ("SEIMAIExponentFocalLengthY",                      cfg_maiSEIExponentFocalLengthY, cfg_maiSEIExponentFocalLengthY, "List of the exponent parts of the focal length of the camera in the vertical direction in the multiview acquisition information SEI message")
  ("SEIMAIMantissaFocalLengthY",                      cfg_maiSEIMantissaFocalLengthY, cfg_maiSEIMantissaFocalLengthY, "List of the mantissa parts of the focal length of the camera in the vertical direction in the multiview acquisition information SEI message")
  ("SEIMAISignPrincipalPointX",                       cfg_maiSEISignPrincipalPointX, cfg_maiSEISignPrincipalPointX, "List of the signs of the principal point of the camera in the horizontal direction in the multiview acquisition information SEI message")
  ("SEIMAIExponentPrincipalPointX",                   cfg_maiSEIExponentPrincipalPointX, cfg_maiSEIExponentPrincipalPointX, "List of the exponent parts of the principal point of the camera in the horizontal direction in the multiview acquisition information SEI message")
  ("SEIMAIMantissaPrincipalPointX",                   cfg_maiSEIMantissaPrincipalPointX, cfg_maiSEIMantissaPrincipalPointX, "List of the mantissa parts of the principal point of the camera in the horizontal direction in the multiview acquisition information SEI message")
  ("SEIMAISignPrincipalPointY",                       cfg_maiSEISignPrincipalPointY, cfg_maiSEISignPrincipalPointY, "List of the signs of the principal point of the camera in the vertical direction in the multiview acquisition information SEI message")
  ("SEIMAIExponentPrincipalPointY",                   cfg_maiSEIExponentPrincipalPointY, cfg_maiSEIExponentPrincipalPointY, "List of the exponent parts of the principal point of the camera in the vertical direction in the multiview acquisition information SEI message")
  ("SEIMAIMantissaPrincipalPointY",                   cfg_maiSEIMantissaPrincipalPointY, cfg_maiSEIMantissaPrincipalPointY, "List of the mantissa parts of the principal point of the camera in the vertical direction in the multiview acquisition information SEI message")
  ("SEIMAISignSkewFactor",                            cfg_maiSEISignSkewFactor,     cfg_maiSEISignSkewFactor, "List of the signs of the skew factor of the camera in the multiview acquisition information SEI message")
  ("SEIMAIExponentSkewFactor",                        cfg_maiSEIExponentSkewFactor, cfg_maiSEIExponentSkewFactor, "List of the exponent parts of the skew factor of the camera in the multiview acquisition information SEI message")
  ("SEIMAIMantissaSkewFactor",                        cfg_maiSEIMantissaSkewFactor, cfg_maiSEIMantissaSkewFactor, "List of the mantissa parts of the skew factor of the camera in the multiview acquisition information SEI message")
  ("SEIMAIPrecRotationParam",                         m_maiSEIPrecRotationParam,                            0, "Specifies the exponent of the maximum allowable truncation error for rotation in the multiview acquisition information SEI message")
  ("SEIMAIPrecTranslationParam",                      m_maiSEIPrecTranslationParam,                         0, "Specifies the exponent of the maximum allowable truncation error for translation in the multiview acquisition information SEI message")
// multiview view position SEI
  ("SEIMVPEnabled",                                   m_mvpSEIEnabled,                                  false, "Control generation of multiview view position SEI message")
  ("SEIMVPNumViewsMinus1",                            m_mvpSEINumViewsMinus1,                               0, "Specifies the number of views minus 1 in the multiview view postion SEI message")
  ("SEIMVPViewPosition",                              cfg_mvpSEIViewPosition,           cfg_mvpSEIViewPosition, "List of View Positions in the multiview view postion SEI message")
// alpha channel information SEI
  ("SEIACIEnabled",                                   m_aciSEIEnabled,                                   false, "Control generation of alpha channel information SEI message")
  ("SEIACICancelFlag",                                m_aciSEICancelFlag,                                false, "Specifies the persistence of any previous alpha channel information SEI message in output order")
  ("SEIACIUseIdc",                                    m_aciSEIUseIdc,                                        0, "Specifies the usage of the auxiliary picture in the alpha channel information SEI message")
  ("SEIACIBitDepthMinus8",                            m_aciSEIBitDepthMinus8,                                0, "Specifies the bit depth of the samples of the auxiliary picture in the alpha channel information SEI message")
  ("SEIACITransparentValue",                          m_aciSEITransparentValue,                              0, "Specifies the interpretation sample value of an auxiliary coded picture luma sample for which the associated luma and chroma samples of the primary coded picture are considered transparent for purposes of alpha blending in the alpha channel information SEI message")
  ("SEIACIOpaqueValue",                               m_aciSEIOpaqueValue,                                   0, "Specifies the interpretation sample value of an auxiliary coded picture luma sample for which the associated luma and chroma samples of the primary coded picture are considered opaque for purposes of alpha blending in the alpha channel information SEI message")
  ("SEIACIIncrFlag",                                  m_aciSEIIncrFlag,                                  false, "Specifies the interpretation sample value for each decoded auxiliary picture luma sample value is equal to the decoded auxiliary picture sample value for purposes of alpha blending in the alpha channel information SEI message")
  ("SEIACIClipFlag",                                  m_aciSEIClipFlag,                                  false, "Specifies whether clipping operation is applied in the alpha channel information SEI message")
  ("SEIACIClipTypeFlag",                              m_aciSEIClipTypeFlag,                              false, "Specifies the type of clipping operation in the alpha channel information SEI message")
  // depth representation information SEI
  ("SEIDRIEnabled",                                   m_driSEIEnabled,                                   false, "Control generation of depth representation information SEI message")
  ("SEIDRIZNearFlag",                                 m_driSEIZNearFlag,                                 false, "Specifies the presence of the nearest depth value in the depth representation information SEI message")
  ("SEIDRIZFarFlag",                                  m_driSEIZFarFlag,                                  false, "Specifies the presence of the farthest depth value in the depth representation information SEI message")
  ("SEIDRIDMinFlag",                                  m_driSEIDMinFlag,                                  false, "Specifies the presence of the minimum disparity value in the depth representation information SEI message")
  ("SEIDRIDMaxFlag",                                  m_driSEIDMaxFlag,                                  false, "Specifies the presence of the maximum disparity value in the depth representation information SEI message")
  ("SEIDRIZNear",                                     m_driSEIZNear,                                       0.0, "Specifies the nearest depth value in the depth representation information SEI message")
  ("SEIDRIZFar",                                      m_driSEIZFar,                                        0.0, "Specifies the farest depth value in the depth representation information SEI message")
  ("SEIDRIDMin",                                      m_driSEIDMin,                                        0.0, "Specifies the minimum disparity value in the depth representation information SEI message")
  ("SEIDRIDMax",                                      m_driSEIDMax,                                        0.0, "Specifies the maximum disparity value in the depth representation information SEI message")
  ("SEIDRIDepthRepresentationType",                   m_driSEIDepthRepresentationType,                       0, "Specifies the the representation definition of decoded luma samples of auxiliary pictures in the depth representation information SEI message")
  ("SEIDRIDisparityRefViewId",                        m_driSEIDisparityRefViewId,                            0, "Specifies the ViewId value against which the disparity values are derived in the depth representation information SEI message")
  ("SEIDRINonlinearNumMinus1",                        m_driSEINonlinearNumMinus1,                            0, "Specifies the number of piece-wise linear segments minus 2 for mapping of depth values to a scale that is uniformly quantized in terms of disparity  in the depth representation information SEI message")
  ("SEIDRINonlinearModel",                            cfg_driSEINonlinearModel,       cfg_driSEINonlinearModel, "List of the piece-wise linear segments for mapping of decoded luma sample values of an auxiliary picture to a scale that is uniformly quantized in terms of disparity in the depth representation information SEI message")
  ("SEIConstrainedRASL",                              m_constrainedRaslEncoding,                         false, "Control generation of constrained RASL encoding SEI message")
  
  //Processing order of SEI (pos)
  ("SEIPOEnabled",                                    m_poSEIEnabled,                                    false, "Specifies whether SEI processing order is applied or not")
  ("SEIPOId",                                         m_poSEIId,                                            0u, "Specifies the id of the SEI processing order SEI message")
  ("SEIPOForHumanViewingIdc",                         m_poSEIForHumanViewingIdc,                            0u, "Specifies the user viewing usage level of video resulting from processing chain specified by SPO SEI: optimal for human viewing (3), suitable (2), unsuitable (1), unknown (0, default)")
  ("SEIPOForMachineAnalysisIdc",                      m_poSEIForMachineAnalysisIdc,                         0u, "Specifies the machine analysis usage level of video resulting from processing chain specified by SPO SEI: optimal for machine analysis (3), suitable (2), unsuitable (1), unknown (0, default)")
  ("SEIPONumMinus2",                                  m_poSEINumMinus2,                                     0u, "Specifies the number of SEIs minus 2 in the SEI processing order SEI message")
  ("SEIPOBreadthFirstFlag",                           m_poSEIBreadthFirstFlag,                           false, "Specifies that breadth-first handling of processing chain is applied (1), or that either breadth-first or depth-first can be applied (0, default)")
  ("SEIPOWrappingFlag",                               cfg_poSEIWrappingFlag,             cfg_poSEIWrappingFlag, "Specifies whether a correspoding processing-order-nested SEI message exists or not")
  ("SEIPOImportanceIdc",                              cfg_poSEIImportanceIdc,           cfg_poSEIImportanceIdc, "Specifies degree of importance (0..3) for the SEI messages")
  ("SEIPOPrefixFlag",                                 cfg_poSEIPrefixFlag,                 cfg_poSEIPrefixFlag, "Specifies whether SEI message prefix is present or not")
  ("SEIPOPayLoadType",                                cfg_poSEIPayloadType,               cfg_poSEIPayloadType, "List of payloadType for processing")
  ("SEIPOProcessingOrder",                            cfg_poSEIProcessingOrder,       cfg_poSEIProcessingOrder, "List of payloadType processing order")
  ("SEIPONumofPrefixBits",                            cfg_poSEINumofPrefixBits,       cfg_poSEINumofPrefixBits, "List of number of prefix bits")
  ("SEIPOPrefixByte",                                 cfg_poSEIPrefixByte,                 cfg_poSEIPrefixByte, "List of prefix bytes")
#if JVET_AJ0105_SPO_COMPLEXITY_INFO
  ("SEIPOComplexityInfoPresentFlag",                  m_poSEIComplexityInfoPresentFlag,                  false, "Specifies whether complexity info is present (1) or not (0). (0, default)")
  ("SEIPOParameterTypeIdc",                           m_poSEIParameterTypeIdc         ,                     0u, "Specifies type of parameters of the NNPFs (0) only integer, (1) integer or floating point, (2) binary only, (3) reserved. (0, default)")
  ("SEIPOLog2ParameterBitLengthMinus3",               m_poSEILog2ParameterBitLengthMinus3,                  0u, "0, 1, 2, and 3 means the NNPFs do not use parameters of bit length greater than 8, 16, 32, and 64, respectively. (0, default)")
  ("SEIPONumParametersIdc",                           m_poSEINumParametersIdc,                              0u, "Specifies max number of parameters needed by NNPFs in the processing chain. (0, default)")
  ("SEIPONumKmacOperationIdcg",                       m_poSEINumKmacOperationIdc,                           0u, "When greater than 0 specifies that the max number of multiply-accumulate operations per sample of the NNPFs is less than or equal to po_num_kmac_operations_idc * 1000. ) means unknown. (0, default)")
  ("SEIPOTotalKilobyteSize",                          m_poSEITotalKilobyteSize,                             0u, "When greater than 0 specifies a total size in kilobytes required to store the uncompressed parameters for NNPFs. 0 means unknown. (0, default)")
#endif
  ("SEIPostFilterHintEnabled",                        m_postFilterHintSEIEnabled,                        false, "Control generation of post-filter Hint SEI message")
  ("SEIPostFilterHintCancelFlag",                     m_postFilterHintSEICancelFlag,                     false, "Specifies the persistence of any previous post-filter Hint SEI message in output order")
  ("SEIPostFilterHintPersistenceFlag",                m_postFilterHintSEIPersistenceFlag,                false, "Specifies the persistence of the post-filter Hint SEI message for the current layer")
  ("SEIPostFilterHintSizeY",                          m_postFilterHintSEISizeY,                             1u, "Specifies the vertical size of the post-filter coefficient or correlation array")
  ("SEIPostFilterHintSizeX",                          m_postFilterHintSEISizeX,                             1u, "Specifies the horizontal size of the post-filter coefficient or correlation array")
  ("SEIPostFilterHintType",                           m_postFilterHintSEIType,                              0u, "Specifies the type of the post-filter: 2D-FIR filter (0, default), 1D-FIR filters (1) or Cross-correlation matrix (0)")
  ("SEIPostFilterHintChromaCoeffPresentFlag",         m_postFilterHintSEIChromaCoeffPresentFlag,         false, "Specifies the presence of post-filter coefficients for chroma")
  ("SEIPostFilterHintValue",                          cfg_postFilterHintSEIValues, cfg_postFilterHintSEIValues, "Specifies post-filter coefficients or elements of a cross-correlation matrix")

  //SEI manifest
  ("SEISEIManifestEnabled",                           m_SEIManifestSEIEnabled,                           false, "Controls if SEI Manifest SEI messages enabled")
  //SEI prefix indication
  ("SEISEIPrefixIndicationEnabled",                   m_SEIPrefixIndicationSEIEnabled,                   false, "Controls if SEI Prefix Indications SEI messages enabled")

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  ("SEIPRIEnabled",                                   m_priSEIEnabled,                                   false, "Specifies whether packet regions info SEI is enabled")
  ("SEIPRICancelFlag",                                m_priSEICancelFlag,                                false, "Specifies the persistence of any previous packed regions info SEI message in output order")
  ("SEIPRIPersistenceFlag",                           m_priSEIPersistenceFlag,                            true, "Specifies the persistence of the packed regions info SEI message for the current layer")
  ("SEIPRINumRegionsMinus1",                          m_priSEINumRegionsMinus1,                             0u, "Specifies the number of regions minus 1 for which information is signalled")
  ("SEIPRIUseMaxDimensionsFlag",                      m_priSEIUseMaxDimensionsFlag,                      false, "Specifies that max pic dimensions are used in variable calculations")
  ("SEIPRILog2UnitSize",                              m_priSEILog2UnitSize,                                 0u, "Specifies a unit size used in variable calculations for the region parameters")
  ("SEIPRIRegionSizeLenMinus1",                       m_priSEIRegionSizeLenMinus1,                         12u, "Specifies the number of bits minus 1 used to signal region top left offsets and region dimensions")
  ("SEIPRIRegionIdPresentFlag",                       m_priSEIRegionIdPresentFlag,                       false, "Specifies whether region IDs are signalled")
  ("SEIPRITargetPicParamsPresentFlag",                m_priSEITargetPicParamsPresentFlag,                false, "Specifies whether pri_target_region_top_left_x[ i ], pri_target_region_top_left_y[ i ], pri_target_pic_width_minus1, and pri_target_pic_height_minus1 are signalled")
  ("SEIPRITargetPicWidthMinus1",                      m_priSEITargetPicWidthMinus1,                         0u, "Target output picture width minus 1")
  ("SEIPRITargetPicHeightMinus1",                     m_priSEITargetPicHeightMinus1,                        0u, "Target output picture height minus 1")
  ("SEIPRINumResamplingRatiosMinus1",                 m_priSEINumResamplingRatiosMinus1,                    0u, "Specifies the number resampling ratios minus 1 that are signalled")
  ("SEIPRIResamplingWidthNumMinus1",                  cfg_priSEIResamplingWidthNumMinus1, cfg_priSEIResamplingWidthNumMinus1, "Specifies a list of numerators minus 1 values for width resampling of the resampling ratio")
  ("SEIPRIResamplingWidthDenomMinus1",                cfg_priSEIResamplingWidthDenomMinus1, cfg_priSEIResamplingWidthDenomMinus1, "Specifies a list of denominators minus 1 values for width resampling of the resampling ratio")
  ("SEIPRIResamplingHeightNumMinus1",                 cfg_priSEIResamplingHeightNumMinus1, cfg_priSEIResamplingHeightNumMinus1, "Specifies a list of numerators minus 1 values for height resampling of the resampling ratio")
  ("SEIPRIResamplingHeightDenomMinus1",               cfg_priSEIResamplingHeightDenomMinus1, cfg_priSEIResamplingHeightDenomMinus1, "Specifies a list of denominators minus 1 values for height resampling of the resampling ratio")
  ("SEIPRIRegionId",                                  cfg_priSEIRegionId, cfg_priSEIRegionId,                   "Specifies a list of IDs for the regions")
  ("SEIPRIRegionTopLeftInUnitsX",                     cfg_priSEIRegionTopLeftInUnitsX, cfg_priSEIRegionTopLeftInUnitsX, "Specifies a list of horizontal top left positions for the regions")
  ("SEIPRIRegionTopLeftInUnitsY",                     cfg_priSEIRegionTopLeftInUnitsY, cfg_priSEIRegionTopLeftInUnitsY, "Specifies a list of vertical top left positions for the regions")
  ("SEIPRIRegionWidthInUnitsMinus1",                  cfg_priSEIRegionWidthInUnitsMinus1, cfg_priSEIRegionWidthInUnitsMinus1, "Specifies a list of widths minus 1 in units for the regions")
  ("SEIPRIRegionHeightInUnitsMinus1",                 cfg_priSEIRegionHeightInUnitsMinus1, cfg_priSEIRegionHeightInUnitsMinus1, "Specifies a list of heights minus 1 in units for the regions")
  ("SEIPRIResamplingRatioIdx",                        cfg_priSEIResamplingRatioIdx, cfg_priSEIResamplingRatioIdx, "Specifies a list of resampling ration indices for the regions")
#if JVET_AL0324_AL0070_PRI_SEI
  ("SEIPRITargetRegionTopLeftInUnitsX",               cfg_priSEITargetRegionTopLeftInUnitsX, cfg_priSEITargetRegionTopLeftInUnitsX, "Specifies a list of horizontal top left postions in units of priUnitSize luma samples for the regions in reconstructed target picture")
  ("SEIPRITargetRegionTopLeftInUnitsY",               cfg_priSEITargetRegionTopLeftInUnitsY, cfg_priSEITargetRegionTopLeftInUnitsY, "Specifies a list of vertical top left postions in units of priUnitSize luma samples for the regions in reconstructed target picture")
#else
  ("SEIPRITargetRegionTopLeftX",                      cfg_priSEITargetRegionTopLeftX, cfg_priSEITargetRegionTopLeftX, "Specifies a list of horizontal top left postions in units of luma samples for the regions in reconstructed target picture")
  ("SEIPRITargetRegionTopLeftY",                      cfg_priSEITargetRegionTopLeftY, cfg_priSEITargetRegionTopLeftY, "Specifies a list of vertical top left postions in units of luma samples for the regions in reconstructed target picture")
#endif
  ("SEIPRIMultilayerFlag",                            m_priSEIMultilayerFlag,                            false, "Specifies whether layer IDs are signalled")
  ("SEIPRIRegionLayerId",                             cfg_priSEIRegionLayerId,         cfg_priSEIRegionLayerId, "Specifies a list of the layer ids of the picture that the region information relate to")
  ("SEIPRIRegionIsALayerFlag",                        cfg_priSEIRegionIsALayerFlag, cfg_priSEIRegionIsALayerFlag, "Specifies a list of flags indicating for each region if the picture width and height in the layer are the same as the region's")
#endif

  ("DebugBitstream",                                  m_decodeBitstreams[0],             std::string( "" ), "Assume the frames up to POC DebugPOC will be the same as in this bitstream. Load those frames from the bitstream instead of encoding them." )
  ("DebugPOC",                                        m_switchPOC,                                 -1, "If DebugBitstream is present, load frames up to this POC from this bitstream. Starting with DebugPOC, return to normal encoding." )
  ("DecodeBitstream1",                                m_decodeBitstreams[0],             std::string( "" ), "Assume the frames up to POC DebugPOC will be the same as in this bitstream. Load those frames from the bitstream instead of encoding them." )
  ("DecodeBitstream2",                                m_decodeBitstreams[1],             std::string( "" ), "Assume the frames up to POC DebugPOC will be the same as in this bitstream. Load those frames from the bitstream instead of encoding them." )
  ("SwitchPOC",                                       m_switchPOC,                                 -1, "If DebugBitstream is present, load frames up to this POC from this bitstream. Starting with DebugPOC, return to normal encoding." )
  ("SwitchDQP",                                       m_switchDQP,                                  0, "delta QP applied to picture with switchPOC and subsequent pictures." )
  ("FastForwardToPOC",                                m_fastForwardToPOC,                          -1, "Get to encoding the specified POC as soon as possible by skipping temporal layers irrelevant for the specified POC." )
  ("StopAfterFFtoPOC",                                m_stopAfterFFtoPOC,                       false, "If using fast forward to POC, after the POC of interest has been hit, stop further encoding.")
  ("ForceDecodeBitstream1",                           m_forceDecodeBitstream1,                  false, "force decoding of bitstream 1 - use this only if you are realy sure about what you are doing ")
  ("DecodeBitstream2ModPOCAndType",                   m_bs2ModPOCAndType,                       false, "Modify POC and NALU-type of second input bitstream, to use second BS as closing I-slice")

  ("DebugCTU",                                        m_debugCTU,                                  -1, "If DebugBitstream is present, load frames up to this POC from this bitstream. Starting with DebugPOC-frame at CTUline containin debug CTU.")
  ("AlfTrueOrg",                                      m_alfTrueOrg,                              true, "Using true original samples for ALF optimization when MCTF is enabled\n")
  ( "ALF",                                             m_alf,                                    true, "Adaptive Loop Filter\n" )
  ("MaxNumALFAPS",                                    m_maxNumAlfAps,             ALF_CTB_MAX_NUM_APS, "Maximum number of ALF APSs" )
  ("AlfapsIDShift",                                   m_alfapsIDShift,                              0, "shift for ALF APSs" )
  ("ConstantJointCbCrSignFlag",                       m_constantJointCbCrSignFlag,              0, "Constant JointCbCr sign flag" )
  ("ALFStrengthLuma",                                  m_alfStrengthLuma,                         1.0, "Adaptive Loop Filter strength for luma. The parameter scales the magnitudes of the ALF filter coefficients for luma. Valid range is 0.0 <= ALFStrengthLuma <= 1.0")
  ("ALFAllowPredefinedFilters",                        m_alfAllowPredefinedFilters,              true, "Allow use of predefined filters for ALF")
  ("CCALFStrength",                                    m_ccalfStrength,                           1.0, "Cross-component Adaptive Loop Filter strength. The parameter scales the magnitudes of the CCALF filter coefficients. Valid range is 0.0 <= CCALFStrength <= 1.0")
  ("ALFStrengthChroma",                                m_alfStrengthChroma,                       1.0, "Adaptive Loop Filter strength for chroma. The parameter scales the magnitudes of the ALF filter coefficients for chroma. Valid range is 0.0 <= ALFStrengthChroma <= 1.0")
  ("ALFStrengthTargetLuma",                            m_alfStrengthTargetLuma,                   1.0, "Adaptive Loop Filter strength target for ALF luma filter optimization. The parameter scales the auto-correlation matrix E and the cross-correlation vector y for luma. Valid range is 0.0 <= ALFStrengthTargetLuma <= 1.0")
  ("ALFStrengthTargetChroma",                          m_alfStrengthTargetChroma,                 1.0, "Adaptive Loop Filter strength target for ALF chroma filter optimization. The parameter scales the auto-correlation matrix E and the cross-correlation vector y for chroma. Valid range is 0.0 <= ALFStrengthTargetChroma <= 1.0")
  ("CCALFStrengthTarget",                              m_ccalfStrengthTarget,                     1.0, "Cross-component Adaptive Loop Filter strength target for filter optimization. The parameter scales the auto-correlation matrix E and the cross-correlation vector y. Valid range is 0.0 <= CCALFStrengthTarget <= 1.0")
  ( "CCALF",                                           m_ccalf,                                  true, "Cross-component Adaptive Loop Filter" )
  ( "CCALFQpTh",                                       m_ccalfQpThreshold,                         37, "QP threshold above which encoder reduces CCALF usage")
  ( "RPR",                                            m_rprEnabledFlag,                          true, "Reference Sample Resolution" )
  ("ScalingRatioHor",                                 m_scalingRatioHor,                          1.0, "Scaling ratio in hor direction")
  ("ScalingRatioVer",                                 m_scalingRatioVer,                          1.0, "Scaling ratio in ver direction")
  ("GOPBasedRPR",                                     m_gopBasedRPREnabledFlag,                 false, "Enables decision to encode pictures in GOP in full resolution or one of three downscaled resolutions(default is 1/2, 2/3 and 4/5 in both dimensions)")
  ("GOPBasedRPRQPTh",                                 m_gopBasedRPRQPThreshold,                    32, "QP threshold parameter that determines which QP GOP-based RPR is invoked for given by QP >= GOPBasedRPRQPTh")
  ("ScalingRatioHor2",                                m_scalingRatioHor2,                         1.5, "Scaling ratio in hor direction for GOP based RPR (2/3)")
  ("ScalingRatioVer2",                                m_scalingRatioVer2,                         1.5, "Scaling ratio in ver direction for GOP based RPR (2/3)")
  ("ScalingRatioHor3",                                m_scalingRatioHor3,                        1.25, "Scaling ratio in hor direction for GOP based RPR (4/5)")
  ("ScalingRatioVer3",                                m_scalingRatioVer3,                        1.25, "Scaling ratio in ver direction for GOP based RPR (4/5)")
  ("PsnrThresholdRPR",                                m_psnrThresholdRPR,                        47.0, "PSNR threshold for GOP based RPR (1/2)")
  ("PsnrThresholdRPR2",                               m_psnrThresholdRPR2,                       44.0, "PSNR threshold for GOP based RPR (2/3)")
  ("PsnrThresholdRPR3",                               m_psnrThresholdRPR3,                       41.0, "PSNR threshold for GOP based RPR (4/5)")
  ("QpOffsetRPR",                                     m_qpOffsetRPR,                               -6, "QP offset for RPR (-6 for 1/2)")
  ("QpOffsetRPR2",                                    m_qpOffsetRPR2,                              -4, "QP offset for RPR2 (-4 for 2/3)")
  ("QpOffsetRPR3",                                    m_qpOffsetRPR3,                              -2, "QP offset for RPR3 (-2 for 4/5)")
  ("QpOffsetChromaRPR",                               m_qpOffsetChromaRPR,                         -6, "QP offset for RPR (-6 for 0.5x)")
  ("QpOffsetChromaRPR2",                              m_qpOffsetChromaRPR2,                        -4, "QP offset for RPR2 (-4 for 2/3x)")
  ("QpOffsetChromaRPR3",                              m_qpOffsetChromaRPR3,                        -2, "QP offset for RPR3 (-2 for 4/5x)")
  ("RPRFunctionalityTesting",                         m_rprFunctionalityTestingEnabledFlag,      false, "Enables RPR functionality testing")
  ("RPRSwitchingResolutionOrderList", cfg_rprSwitchingResolutionOrderList, cfg_rprSwitchingResolutionOrderList, "Order of resolutions for each segment in RPR functionality testing where 0,1,2,3 corresponds to full resolution,4/5,2/3 and 1/2")
  ("RPRSwitchingQPOffsetOrderList", cfg_rprSwitchingQPOffsetOrderList, cfg_rprSwitchingQPOffsetOrderList, "Order of QP offset for each segment in RPR functionality testing, where the QP is modified according to the given offset")
  ("RPRSwitchingSegmentSize",                         m_rprSwitchingSegmentSize,                    32, "Segment size with same resolution")
  ("RPRSwitchingTime",                                m_rprSwitchingTime,                          0.0, "Segment switching time in seconds, when non-zero it defines the segment size according to frame rate (a multiple of 8)")
  ("RPRPopulatePPSatIntra",                           m_rprPopulatePPSatIntraFlag,               false, "Populate all PPS which can be used in the sequence at the Intra, e.g. full-res, 4/5, 2/3 and 1/2")
  ( "FractionNumFrames",                              m_fractionOfFrames,                         1.0, "Encode a fraction of the specified in FramesToBeEncoded frames" )
  ( "SwitchPocPeriod",                                m_switchPocPeriod,                            0, "Switch POC period for RPR" )
  ( "UpscaledOutput",                                 m_upscaledOutput,                             0, "Output upscaled (2), decoded but in full resolution buffer (1) or decoded cropped (0, default) picture for RPR" )
  ("UpscaledOutputWidth",                             m_upscaledOutputWidth,                        0, "Forced upscaled output width (override SPS)" )
  ("UpscaledOutputHeight",                            m_upscaledOutputHeight,                       0, "Forced upscaled output height (override SPS)" )
  ("UpscaleFilterForDisplay",                         m_upscaleFilterForDisplay,                    1, "Filters used for upscaling reconstruction to full resolution (2: ECM 12-tap luma and 6-tap chroma MC filters, 1: Alternative 12-tap luma and 6-tap chroma filters, 0: VVC 8-tap luma and 4-tap chroma MC filters)")
  ( "MaxLayers",                                      m_maxLayers,                                  1, "Max number of layers" )
  ( "EnableOperatingPointInformation",                m_OPIEnabled,                             false, "Enables writing of Operating Point Information (OPI)" )
  ( "MaxTemporalLayer",                               m_maxTemporalLayer,                         500, "Maximum temporal layer to be signalled in OPI" )
  ( "TargetOutputLayerSet",                           m_targetOlsIdx,                             500, "Target output layer set index to be signalled in OPI" )
  ( "PrintRefLayerMetrics",                           m_refMetricsEnabled,                      false, "0 (default) do not print ref layer metrics, 1 = print ref layer metrics based on current layer source")
  ;
  opts.addOptions()
  ( "MaxSublayers",                                   m_maxSublayers,                               7, "Max number of Sublayers")
  ( "DefaultPtlDpbHrdMaxTidFlag",                     m_defaultPtlDpbHrdMaxTidFlag,              true, "specifies that the syntax elements vps_ptl_max_tid[ i ], vps_dpb_max_tid[ i ], and vps_hrd_max_tid[ i ] are not present and are inferred to be equal to the default value vps_max_sublayers_minus1")
  ( "AllIndependentLayersFlag",                       m_allIndependentLayersFlag,                true, "All layers are independent layer")
  ("AllowablePredDirection",                          m_predDirectionArray, std::string(""),                "prediction directions allowed for i-th temporal layer")
  ( "LayerId%d",                                      m_layerId,                    0, MAX_VPS_LAYERS, "Layer ID")
  ( "NumRefLayers%d",                                 m_numRefLayers,               0, MAX_VPS_LAYERS, "Number of direct reference layer index of i-th layer")
  ( "RefLayerIdx%d",                                  m_refLayerIdxStr,    std::string(""), MAX_VPS_LAYERS, "Reference layer index(es)")
  ( "EachLayerIsAnOlsFlag",                           m_eachLayerIsAnOlsFlag,                    true, "Each layer is an OLS layer flag")
  ( "OlsModeIdc",                                     m_olsModeIdc,                                 0, "Output layer set mode")
  ( "NumOutputLayerSets",                             m_numOutputLayerSets,                         1, "Number of output layer sets")
  ( "OlsOutputLayer%d",                               m_olsOutputLayerStr, std::string(""), MAX_VPS_LAYERS, "Output layer index of i-th OLS")
  ( "NumPTLsInVPS",                                   m_numPtlsInVps,                               1, "Number of profile_tier_level structures in VPS" )
  ( "PtPresentInPTL%d",                               m_ptPresentInPtl,               0, MAX_NUM_OLSS, "Profile/Tier present in i-th PTL")
  ( "AvoidIntraInDepLayers",                          m_avoidIntraInDepLayer,                    true, "Replaces I pictures in dependent layers with B pictures" )
  ( "MaxTidILRefPicsPlusOneLayerId%d",                m_maxTidILRefPicsPlus1Str, std::string(""), MAX_VPS_LAYERS, "Maximum temporal ID for inter-layer reference pictures plus 1 of i-th layer, 0 for IRAP only")
  ( "RPLofDepLayerInSH",                              m_rplOfDepLayerInSh,                      false, "define Reference picture lists in slice header instead of SPS for dependant layers")
  ( "ExplicitILRP",                                   m_explicitILRP,                           false, "Explicitly define Inter-Layer Reference pictures in GOP entry")
  ( "EncInterLayerOpt",                               m_encILOpt,                               false, "Enable encoder optimization to favor inter-layer predictions")
  ( "EncInterLayerOptLambdaModifier",                 m_encILOptLambdaModifier,         ( double )0.1, "lambda modifier for modified RD cost in Inter-Layer criterion")
    ;

  opts.addOptions()
    ("TemporalFilter",               m_gopBasedTemporalFilterEnabled,                     false, "Enable GOP based temporal filter. Disabled per default")
    ("TemporalFilterPastRefs",       m_gopBasedTemporalFilterPastRefs,          TF_DEFAULT_REFS, "Number of past references for temporal prefilter")
    ("TemporalFilterFutureRefs",     m_gopBasedTemporalFilterFutureRefs,        TF_DEFAULT_REFS, "Number of future references for temporal prefilter")
    ("FirstValidFrame",              m_firstValidFrame,                                       0, "First valid frame")
    ("LastValidFrame",               m_lastValidFrame,                                  MAX_INT, "Last valid frame")
    ("TemporalFilterStrengthFrame*", m_gopBasedTemporalFilterStrengths, std::map<int, double>(), "Strength for every * frame in GOP based temporal filter, where * is an integer."
                                                                                                                  " E.g. --TemporalFilterStrengthFrame8 0.95 will enable GOP based temporal filter at every 8th frame with strength 0.95");
  
  opts.addOptions()
    ("SEIGenerativeFaceVideoEnabled",                         m_generativeFaceVideoEnabled,                             false,                                                         "Control use of the Generative Face Video SEI on current picture")
    ("SEIGenerativeFaceVideoNumber",                          m_generativeFaceVideoSEINumber,                           0u,                                                            "Total number of Generative Face Video SEI to be carried")
    ("SEIGenerativeFaceVideoBasePicFlag",                     m_generativeFaceVideoSEIBasePicFlag,                      false,                                                         "Specifies whether to indicates the current decoded output picture corresponds to a base picture")
    ("SEIGenerativeFaceVideoNNPresentFlag",                   m_generativeFaceVideoSEINNPresentFlag,                    false,                                                         "indicates a neural network that may be used as a TranslatorNN( ) ")
    ("SEIGenerativeFaceVideoNNModeIdc",                       m_generativeFaceVideoSEINNModeIdc,                        0u,                                                            "specify a neural network that may be used as a TranslatorNN( )")
    ("SEIGenerativeFaceVideoNNTagURI",                        m_generativeFaceVideoSEINNTagURI,                         std::string(""),                                               "specify path to gfv_uri_tag")
    ("SEIGenerativeFaceVideoNNURI",                           m_generativeFaceVideoSEINNURI,                            std::string(""),                                               "specify path to gfv_uri")
    ("SEIGenerativeFaceVideoId",                              cfg_generativeFaceVideoSEIId,                             cfg_generativeFaceVideoSEIId,                                  "Target id of Generative Face Video SEI on current picture")
    ("SEIGenerativeFaceVideoCnt",                             cfg_generativeFaceVideoSEICnt,                            cfg_generativeFaceVideoSEICnt,                                 "Target cnt of Generative Face Video SEI on current picture")
    ("SEIGenerativeFaceVideoCoordinatePresentFlag",           cfg_generativeFaceVideoSEICoordinatePresentFlag,          cfg_generativeFaceVideoSEICoordinatePresentFlag,               "Specifies whether to carry coorinate parameter")
    ("SEIGenerativeFaceVideoLowConfidenceFaceParameterFlag",  cfg_generativeFaceVideoSEILowConfidenceFaceParameterFlag, cfg_generativeFaceVideoSEILowConfidenceFaceParameterFlag,      "Indicates the facial parameters have been derived with low confidence ")
    ("SEIGenerativeFaceVideoDrivePicFusionFlag",              cfg_generativeFaceVideoSEIDrivePicFusionFlag,             cfg_generativeFaceVideoSEIDrivePicFusionFlag,                  "Specifies whether to use DrivePicFusion function")
    ("SEIGenerativeFaceVideoCoordinateQuantizationFactor",    cfg_generativeFaceVideoSEICoordinateQuantizationFactor,   cfg_generativeFaceVideoSEICoordinateQuantizationFactor,        "Specifies the quantization factor to process the facial coordinate paramters")
    ("SEIGenerativeFaceVideoCoordinatePredFlag",              cfg_generativeFaceVideoSEICoordinatePredFlag,             cfg_generativeFaceVideoSEICoordinatePredFlag,                  "Specifies whether to use the difference operation to process data")
    ("SEIGenerativeFaceVideo3DCoordinateFlag",                cfg_generativeFaceVideoSEI3DCoordinateFlag,               cfg_generativeFaceVideoSEI3DCoordinateFlag,                    "Specifies whether to carry 3D coordinate-type paramters")
    ("SEIGenerativeFaceVideoCoordinatePointNum",              cfg_generativeFaceVideoSEICoordinatePointNum,             cfg_generativeFaceVideoSEICoordinatePointNum,                  "the number of facial coordinate parameter set")
    ("SEIGenerativeFaceVideoXCoordinate",                     cfg_generativeFaceVideoSEICoordinateXTesonr,              cfg_generativeFaceVideoSEICoordinateXTesonr,                   "the x-axis value for i_th coordinate")
    ("SEIGenerativeFaceVideoYCoordinate",                     cfg_generativeFaceVideoSEICoordinateYTesonr,              cfg_generativeFaceVideoSEICoordinateYTesonr,                   "the y-axis value for i_th coordinate")
    ("SEIGenerativeFaceVideoZCoordinateMaxValue",             cfg_generativeFaceVideoSEIZCoordinateMaxValue,            cfg_generativeFaceVideoSEIZCoordinateMaxValue,                 "the max value of z-axis coordinate parameter")
    ("SEIGenerativeFaceVideoZCoordinate",                     cfg_generativeFaceVideoSEICoordinateZTesonr,              cfg_generativeFaceVideoSEICoordinateZTesonr,                   "the z-axis value for i_th coordinate")
    ("SEIGenerativeFaceVideoMatrixPresentFlag",               cfg_generativeFaceVideoSEIMatrixPresentFlag,              cfg_generativeFaceVideoSEIMatrixPresentFlag,                   "Specifies whether to carry matrix parameter")
    ("SEIGenerativeFaceVideoMatrixElementPrecisionFactor",    cfg_generativeFaceVideoSEIMatrixElementPrecisionFactor,   cfg_generativeFaceVideoSEIMatrixElementPrecisionFactor,        "Specifies the precision factor to process the facial matrix paramters (decimal part)")
    ("SEIGenerativeFaceVideoNumMatrixType",                   cfg_generativeFaceVideoSEINumMatrixType,                  cfg_generativeFaceVideoSEINumMatrixType,                       "Specifies the number of used facial matrix type")
    ("SEIGenerativeFaceVideoMatrixTypeIdx",                   cfg_generativeFaceVideoSEIMatrixTypeIdx,                  cfg_generativeFaceVideoSEIMatrixTypeIdx,                       "an identifying number vector regarding which facial matrix may be used")
    ("SEIGenerativeFaceVideoMatrix3DSpaceFlag",               cfg_generativeFaceVideoSEIMatrix3DSpaceFlag,              cfg_generativeFaceVideoSEIMatrix3DSpaceFlag,                   "an identifying number vector regarding which facial matrix may be in 3d space")
    ("SEIGenerativeFaceVideoNumMatrices",                     cfg_generativeFaceVideoSEINumMatrices,                    cfg_generativeFaceVideoSEINumMatrices,                         "the number of matrices of the i-th matrix type")
    ("SEIGenerativeFaceVideoMatrixWidth",                     cfg_generativeFaceVideoSEIMatrixWidth,                    cfg_generativeFaceVideoSEIMatrixWidth,                         "the width of matrices of the i-th matrix type")
    ("SEIGenerativeFaceVideoMatrixHeight",                    cfg_generativeFaceVideoSEIMatrixHeight,                   cfg_generativeFaceVideoSEIMatrixHeight,                        "the height of matrices of the i-th matrix type")
    ("SEIGenerativeFaceVideoMatrixElement",                   cfg_generativeFaceVideoSEIMatrixElement,                  cfg_generativeFaceVideoSEIMatrixElement,                       "the value of the matrix element at position (k, l) of the j-th matrix of the i-th matrix type. ")
    ("SEIGenerativeFaceVideoMatrixPredFlag",                  cfg_generativeFaceVideoSEIMatrixPredFlag,                 cfg_generativeFaceVideoSEIMatrixPredFlag,                      "indicates whether to use difference operation for GFV matrix ")
    ("SEIGenerativeFaceVideoNumMatricestoNumKpsFlag",         cfg_generativeFaceVideoSEINumMatricestoNumKpsFlag,        cfg_generativeFaceVideoSEINumMatricestoNumKpsFlag,             "indicates whether  the number of matrices of the i-th matrix type is equal to gfv_num_kps_minus1 + 1")
    ("SEIGenerativeFaceVideoNumMatricesInfo",                 cfg_generativeFaceVideoSEINumMatricesInfo,                cfg_generativeFaceVideoSEINumMatricesInfo,                     "provides information to derive the number of the matrices of the i-th matrix type.")
    ("SEIGenerativeFaceVideoPayloadFilename",                 m_generativeFaceVideoSEIPayloadFilename,                  std::string(""),                                               "specify path to payloadfile")
    ("SEIGenerativeFaceVideoChromaKeyInfoPresentFlag",        m_generativeFaceVideoSEIChromaKeyInfoPresentFlag,         false,                                                         "Specifies the syntax elements gfv_chroma_key information")
    ("SEIGenerativeFaceVideoChromaKeyValuePresentFlag",       cfg_generativeFaceVideoSEIChromaKeyValuePresentFlag,      cfg_generativeFaceVideoSEIChromaKeyValuePresentFlag,           "indicates that the syntax element gfv_chroma_key_value[ c ] is present.")
    ("SEIGenerativeFaceVideoChromaKeyValue",                  cfg_generativeFaceVideoSEIChromaKeyValue,                 cfg_generativeFaceVideoSEIChromaKeyValue,                      "specifies the chroma key value corresponding to the c-th colour component")
    ("SEIGenerativeFaceVideoChromaKeyThrPresentFlag",         cfg_generativeFaceVideoSEIChromaKeyThrPresentFlag,        cfg_generativeFaceVideoSEIChromaKeyThrPresentFlag,             "indicates that the syntax element gfv_chroma_thr_value[ i ] is present")
    ("SEIGenerativeFaceVideoChromaKeyThrValue",               cfg_generativeFaceVideoSEIChromaKeyThrValue,              cfg_generativeFaceVideoSEIChromaKeyThrValue,                   "specifies the i-th chroma key threshold value");
  opts.addOptions()
    ("SEIGenerativeFaceVideoEnhancementEnabled",                         m_generativeFaceVideoEnhancementEnabled,                             false,                                                               "Control use of the Generative Face Video Enhancement SEI on current picture")
    ("SEIGenerativeFaceVideoEnhancementNumber",                          m_generativeFaceVideoEnhancementSEINumber,                           0u,                                                                  "Total number of Generative Face Video Enhancement SEI to be carried")  
    ("SEIGenerativeFaceVideoEnhancementBasePicFlag",                     m_generativeFaceVideoEnhancementSEIBasePicFlag,                      false,                                                               "Specifies whether to indicates the current decoded output picture corresponds to a base picture")
    ("SEIGenerativeFaceVideoEnhancementNNPresentFlag",                   m_generativeFaceVideoEnhancementSEINNPresentFlag,                    false,                                                               "indicates a neural network that may be used as a EnhancerNN( ) ")
    ("SEIGenerativeFaceVideoEnhancementNNModeIdc",                       m_generativeFaceVideoEnhancementSEINNModeIdc,                        0u,                                                                  "specify a neural network that may be used as a EnhancerNN( )")
    ("SEIGenerativeFaceVideoEnhancementNNTagURI",                        m_generativeFaceVideoEnhancementSEINNTagURI,                         std::string(""),                                                     "specify path to gfv_uri_tag")
    ("SEIGenerativeFaceVideoEnhancementNNURI",                           m_generativeFaceVideoEnhancementSEINNURI,                            std::string(""),                                                     "specify path to gfv_uri")    
    ("SEIGenerativeFaceVideoEnhancementId",                              cfg_generativeFaceVideoEnhancementSEIId,                             cfg_generativeFaceVideoEnhancementSEIId,                             "Target id of Generative Face Video Enhancement SEI on current picture")
    ("SEIGenerativeFaceVideoEnhancementGFVCnt",                          cfg_generativeFaceVideoEnhancementSEIGFVCnt,                         cfg_generativeFaceVideoEnhancementSEIGFVCnt,                         "Target cnt of Generative Face Video Enhancement SEI on current picture")
    ("SEIGenerativeFaceVideoEnhancementGFVId",                           cfg_generativeFaceVideoEnhancementSEIGFVId,                          cfg_generativeFaceVideoEnhancementSEIGFVId,                          "Target id of Generative Face Video Enhancement SEI on current picture")	  
    ("SEIGenerativeFaceVideoEnhancementMatrixPredFlag",                  cfg_generativeFaceVideoEnhancementSEIMatrixPredFlag,                 cfg_generativeFaceVideoEnhancementSEIMatrixPredFlag,                 "indicates whether to use difference operation for GFVE matrix ")
    ("SEIGenerativeFaceVideoEnhancementMatrixPresentFlag",               cfg_generativeFaceVideoEnhancementSEIMatrixPresentFlag,              cfg_generativeFaceVideoEnhancementSEIMatrixPresentFlag,              "Specifies whether to carry gfve matrix parameter ")  
    ("SEIGenerativeFaceVideoEnhancementMatrixElementPrecisionFactor",    cfg_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor,   cfg_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor,   "Specifies the precision factor to process the facial matrix paramters (decimal part)")
    ("SEIGenerativeFaceVideoEnhancementNumMatrices",                     cfg_generativeFaceVideoEnhancementSEINumMatrices,                    cfg_generativeFaceVideoEnhancementSEINumMatrices,                    "the number of matrices of the i-th matrix type")
    ("SEIGenerativeFaceVideoEnhancementMatrixWidth",                     cfg_generativeFaceVideoEnhancementSEIMatrixWidth,                    cfg_generativeFaceVideoEnhancementSEIMatrixWidth,                    "the width of matrices of the i-th matrix type")
    ("SEIGenerativeFaceVideoEnhancementMatrixHeight",                    cfg_generativeFaceVideoEnhancementSEIMatrixHeight,                   cfg_generativeFaceVideoEnhancementSEIMatrixHeight,                   "the height of matrices of the i-th matrix type")
    ("SEIGenerativeFaceVideoEnhancementMatrixElement",                   cfg_generativeFaceVideoEnhancementSEIMatrixElement,                  cfg_generativeFaceVideoEnhancementSEIMatrixElement,                  "the value of the matrix element at position (k, l) of the j-th matrix of the i-th matrix type. ")
    ("SEIGenerativeFaceVideoEnhancementPupilPresentIdx",                 cfg_generativeFaceVideoEnhancementSEIPupilPresentIdx,                cfg_generativeFaceVideoEnhancementSEIPupilPresentIdx,                "Indicate the pupil information for transmission")
    ("SEIGenerativeFaceVideoEnhancementPupilCoordinatePrecisionFactor",  cfg_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor, cfg_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor, "the quantization precision factor of pupil coordinates")
    ("SEIGenerativeFaceVideoEnhancementPupilLeftEyeCoordinateX",         cfg_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX,        cfg_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX,        "the X coordinate of the left eye pupil")
    ("SEIGenerativeFaceVideoEnhancementPupilLeftEyeCoordinateY",         cfg_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY,        cfg_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY,        "the Y coordinate of the left eye pupil")
    ("SEIGenerativeFaceVideoEnhancementPupilRightEyeCoordinateX",        cfg_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX,       cfg_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX,       "the X coordinate of the right eye pupil")
    ("SEIGenerativeFaceVideoEnhancementPupilRightEyeCoordinateY",        cfg_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY,       cfg_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY,       "the Y coordinate of the right eye pupil")
    ("SEIGenerativeFaceVideoEnhancementPayloadFilename",                 m_generativeFaceVideoEnhancementSEIPayloadFilename,                  std::string(""),                                                     "specify path to payloadfile") ;
  
#if JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
  opts.addOptions()
    ("SEIAUREnabled",                                         m_aurSEIEnabled,                                          false,                                                         "Control use of the AI usage restrictions SEI")
    ("SEIAURCancelFlag",                                      m_aurSEICancelFlag,                                       false,                                                         "Specifies the persistence of any previous AI usage restrictions SEI message in output order")
    ("SEIAURPersistenceFlag",                                 m_aurSEIPersistenceFlag,                                  false,                                                         "Specifies the persistence of the AI usage restrictions SEI message for the current layer.")
    ("SEIAURNumRestrictionsMinus1",                           m_aurSEINumRestrictionsMinus1,                            0u,                                                            "plus one specifies the number of restriction")
    ("SEIAURRestrictions",                                    cfg_aurSEIRestrictions,                                   cfg_aurSEIRestrictions,                                        "List of restrictions")
    ("SEIAURContextPresentFlag",                              cfg_aurSEIContextPresentFlag,                             cfg_aurSEIContextPresentFlag,                                  "List of flags indicating whether aur_context syntax elements are present")
    ("SEIAURContext",                                         cfg_aurSEIContext,                                        cfg_aurSEIContext,                                             "List of context");
#endif
  // clang-format on

#if EXTENSION_360_VIDEO
  TExt360AppEncCfg::TExt360AppEncCfgContext ext360CfgContext;
  m_ext360.addOptions(opts, ext360CfgContext);
#endif

  for(int i=1; i<MAX_GOP+1; i++)
  {
    std::ostringstream cOSS;
    cOSS<<"Frame"<<i;
    opts.addOptions()(cOSS.str(), m_GOPList[i-1], GOPEntry());
  }

  for(int i = 0; i < MAX_NUM_OLSS; i++)
  {
    std::ostringstream cOSS1;
    cOSS1<<"LevelPTL"<<i;
    opts.addOptions()(cOSS1.str(), m_levelPtl[i], Level::NONE);

    std::ostringstream cOSS2;
    cOSS2<<"OlsPTLIdx"<<i;
    opts.addOptions()(cOSS2.str(), m_olsPtlIdx[i], 0);
  }

  opts.addOptions()("SEINNPFCEnabled",  m_nnPostFilterSEICharacteristicsEnabled, false, "Control generation of the Neural Network Post Filter Characteristics SEI messages");
  opts.addOptions()("SEINNPFCUseSuffixSEI",  m_nnPostFilterSEICharacteristicsUseSuffixSEI, false, "Code NNPFC SEI either as suffix (1) or prefix (0) SEI message");
  opts.addOptions()( "SEINNPFCNumFilters",                                      m_nnPostFilterSEICharacteristicsNumFilters,                                  0, "Specifies the number of Neural Network Post Filter Characteristics SEI messages" );
  for (int i = 0; i < MAX_NUM_NN_POST_FILTERS; i++)
  {
    std::ostringstream id;
    id << "SEINNPFCId" << i;
    opts.addOptions()(id.str(), m_nnPostFilterSEICharacteristicsId[i], 0u, "Specifies the identifying number in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream modeIdc;
    modeIdc << "SEINNPFCModeIdc" << i;
    opts.addOptions()(modeIdc.str(), m_nnPostFilterSEICharacteristicsModeIdc[i], 0u, "Specifies the Neural Network Post Filter IDC in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream propertyPresentFlag;
    propertyPresentFlag << "SEINNPFCPropertyPresentFlag" << i;
    opts.addOptions()(propertyPresentFlag.str(), m_nnPostFilterSEICharacteristicsPropertyPresentFlag[i], false, "Specifies whether the filter purpose, input formatting, output formatting and complexity are present in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream nnpfcBaseFlag;
    nnpfcBaseFlag << "SEINNPFCBaseFlag" << i;
    opts.addOptions()(nnpfcBaseFlag.str(), m_nnPostFilterSEICharacteristicsBaseFlag[i], false, "Specifies whether the filter is a base filter or not");

    std::ostringstream purpose;
    purpose << "SEINNPFCPurpose" << i;
    opts.addOptions()(purpose.str(), m_nnPostFilterSEICharacteristicsPurpose[i], 0u, "Specifies the purpose in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream outSubWidthCFlag;
    outSubWidthCFlag << "SEINNPFCOutSubCFlag" << i;
    opts.addOptions()(outSubWidthCFlag.str(), m_nnPostFilterSEICharacteristicsOutSubCFlag[i], false, "Specifies output chroma format when upsampling");

    std::ostringstream outColourFormatIdc;
    outColourFormatIdc << "SEINNPFCOutColourFormatIdc" << i;
    opts.addOptions()(outColourFormatIdc.str(), m_nnPostFilterSEICharacteristicsOutColourFormatIdc[i], 1u, "Specifies output chroma format for colourization purpose");

    std::ostringstream picWidthNum;
    picWidthNum << "SEINNPFCPicWidthNumerator" << i;
    opts.addOptions()(picWidthNum.str(), m_nnPostFilterSEICharacteristicsPicWidthNumerator[i], 1u,
                      "Specifies the numerator of output picture width resulting from applying the Neural Network Post "
                      "Filter Characteristics SEI message");

    std::ostringstream picWidthDenom;
    picWidthDenom << "SEINNPFCPicWidthDenominator" << i;
    opts.addOptions()(picWidthDenom.str(), m_nnPostFilterSEICharacteristicsPicWidthDenominator[i], 1u,
                      "Specifies the denominator of output picture width resulting from applying the Neural Network "
                      "Post Filter Characteristics SEI message");

    std::ostringstream picHeightNum;
    picHeightNum << "SEINNPFCPicHeightNumerator" << i;
    opts.addOptions()(picHeightNum.str(), m_nnPostFilterSEICharacteristicsPicHeightNumerator[i], 1u,
                      "Specifies the numerator of output picture height resulting from applying the Neural Network "
                      "Post Filter Characteristics SEI message");

    std::ostringstream picHeightDenom;
    picHeightDenom << "SEINNPFCPicWidthDenominator" << i;
    opts.addOptions()(picHeightDenom.str(), m_nnPostFilterSEICharacteristicsPicHeightDenominator[i], 1u,
                      "Specifies the denominator of output picture height resulting from applying the Neural Network "
                      "Post Filter Characteristics SEI message");

    std::ostringstream inpTensorBitDepthLumaMinus8;
    inpTensorBitDepthLumaMinus8 << "SEINNPFCInpTensorBitDepthLumaMinusEight" << i;
    opts.addOptions()(inpTensorBitDepthLumaMinus8.str(), m_nnPostFilterSEICharacteristicsInpTensorBitDepthLumaMinus8[i], 0u, "Specifies the bit depth of the input tensor luma minus 8 in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream inpTensorBitDepthChromaMinus8;
    inpTensorBitDepthChromaMinus8 << "SEINNPFCInpTensorBitDepthChromaMinusEight" << i;
    opts.addOptions()(inpTensorBitDepthChromaMinus8.str(), m_nnPostFilterSEICharacteristicsInpTensorBitDepthChromaMinus8[i], 0u, "Specifies the bit depth of the input tensor chroma minus 8 in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream outTensorBitDepthLumaMinus8;
    outTensorBitDepthLumaMinus8 << "SEINNPFCOutTensorBitDepthLumaMinusEight" << i;
    opts.addOptions()(outTensorBitDepthLumaMinus8.str(), m_nnPostFilterSEICharacteristicsOutTensorBitDepthLumaMinus8[i], 0u, "Specifies the bit depth of the output tensor luma minus 8 in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream outTensorBitDepthChromaMinus8;
    outTensorBitDepthChromaMinus8 << "SEINNPFCOutTensorBitDepthChromaMinusEight" << i;
    opts.addOptions()(outTensorBitDepthChromaMinus8.str(), m_nnPostFilterSEICharacteristicsOutTensorBitDepthChromaMinus8[i], 0u, "Specifies the bit depth of the output tensor chroma minus 8 in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream componentLastFlag;
    componentLastFlag << "SEINNPFCComponentLastFlag" << i;
    opts.addOptions()(componentLastFlag.str(), m_nnPostFilterSEICharacteristicsComponentLastFlag[i], false, "Specifies the channel component is located in the last dimension for the Neural Network Post Filter Characteristics SEI message");


    std::ostringstream inpFormatIdc;
    inpFormatIdc << "SEINNPFCInpFormatIdc" << i;
    opts.addOptions()(inpFormatIdc.str(), m_nnPostFilterSEICharacteristicsInpFormatIdc[i], 0u, "Specifies the method of converting an input sample in the the Neural Network Post Filter Characteristics SEI message");
    std::ostringstream auxInpIdc;
    auxInpIdc << "SEINNPFCAuxInpIdc" << i;
    opts.addOptions()(auxInpIdc.str(), m_nnPostFilterSEICharacteristicsAuxInpIdc[i], 0u, "Specifies the auxillary input index in the Nueral Network Post Filter Characteristics SEI message");

    std::ostringstream sepColDescriptionFlag;
    sepColDescriptionFlag << "SEINNPFCSepColDescriptionFlag" << i;
    opts.addOptions()(sepColDescriptionFlag.str(), m_nnPostFilterSEICharacteristicsSepColDescriptionFlag[i], false, "Specifies the presence of seperate color descriptions in the Nueral Network Post Filter Characteristics SEI message");

    std::ostringstream fullRangeFlag;
    fullRangeFlag << "SEINNPFCFullRangeFlag" << i;
    opts.addOptions()(fullRangeFlag.str(), m_nnPostFilterSEICharacteristicsFullRangeFlag[i], false, "Specifies scaling and offset values applied in association with the matrix coefficients as specified by nnpfc_matrix_coeff.");
  
    std::ostringstream colPrimaries;
    colPrimaries << "SEINNPFCColPrimaries" << i;
    opts.addOptions()(colPrimaries.str(), m_nnPostFilterSEICharacteristicsColPrimaries[i], 0u, "Specifies color primaries in the Nueral Network Post Filter Characteristics SEI message");

    std::ostringstream transCharacteristics;
    transCharacteristics << "SEINNPFCTransCharacteristics" << i;
    opts.addOptions()(transCharacteristics.str(), m_nnPostFilterSEICharacteristicsTransCharacteristics[i], 0u, "Specifies Transfer Characteristics in the Nueral Network Post Filter Characteristics SEI message");

    std::ostringstream matrixCoeffs;
    matrixCoeffs << "SEINNPFCMatrixCoeffs" << i;
    opts.addOptions()(matrixCoeffs.str(), m_nnPostFilterSEICharacteristicsMatrixCoeffs[i], 0u, "Specifies color matrix coefficients in the Nueral Network Post Filter Characteristics SEI message");
    std::ostringstream inpOrderIdc;
    inpOrderIdc << "SEINNPFCInpOrderIdc" << i;
    opts.addOptions()(inpOrderIdc.str(), m_nnPostFilterSEICharacteristicsInpOrderIdc[i], 0u, "Specifies the method of ordering the input sample arrays in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream outFormatIdc;
    outFormatIdc << "SEINNPFCOutFormatIdc" << i;
    opts.addOptions()(outFormatIdc.str(), m_nnPostFilterSEICharacteristicsOutFormatIdc[i], 0u, "Specifies the method of converting an output sample in the the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream outOrderIdc;
    outOrderIdc << "SEINNPFCOutOrderIdc" << i;
    opts.addOptions()(outOrderIdc.str(), m_nnPostFilterSEICharacteristicsOutOrderIdc[i], 0u, "Specifies the method of ordering the output sample arrays in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream constantPatchSizeFlag;
    constantPatchSizeFlag << "SEINNPFCConstantPatchSizeFlag" << i;
    opts.addOptions()(constantPatchSizeFlag.str(), m_nnPostFilterSEICharacteristicsConstantPatchSizeFlag[i], false, "Specifies the patch size flag in the the Neural Network Post Filter Characteristics SEI message");
    
    std::ostringstream chromaLocInfoPresentFlag;
    chromaLocInfoPresentFlag << "SEINNPFCChromaLocInfoPresentFlag" << i;
    opts.addOptions()(chromaLocInfoPresentFlag.str(), m_nnPostFilterSEICharacteristicsChromaLocInfoPresentFlag[i], false, "Specifies the chroma location information flag in the the Neural Network Post Filter Characteristics SEI message");
    
    std::ostringstream chromaSampleLocTypeFrame;
    chromaSampleLocTypeFrame << "SEINNPFCChromaSampleLocTypeFrame" << i;
    opts.addOptions()(chromaSampleLocTypeFrame.str(), m_nnPostFilterSEICharacteristicsChromaSampleLocTypeFrame[i], 0u, "Specifies the method of ordering the output sample arrays in the Neural Network Post Filter Characteristics SEI message");
    
    std::ostringstream patchWidthMinus1;
    patchWidthMinus1 << "SEINNPFCPatchWidthMinus1" << i;
    opts.addOptions()(patchWidthMinus1.str(), m_nnPostFilterSEICharacteristicsPatchWidthMinus1[i], 0u, "Specifies the horizontal sample counts of a patch in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream patchHeightMinus1;
    patchHeightMinus1 << "SEINNPFCPatchHeightMinus1" << i;
    opts.addOptions()(patchHeightMinus1.str(), m_nnPostFilterSEICharacteristicsPatchHeightMinus1[i], 0u, "Specifies the vertical sample counts of a patch in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream extendedPatchWidthCdDeltaMinus1;
    extendedPatchWidthCdDeltaMinus1 << "SEINNPFCExtendedPatchWidthCdDeltaMinus1" << i;
    opts.addOptions()(extendedPatchWidthCdDeltaMinus1.str(), m_nnPostFilterSEICharacteristicsExtendedPatchWidthCdDeltaMinus1[i], 0u, "Specifies the extended horizontal sample counts of a patch in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream extendedPatchHeightCdDeltaMinus1;
    extendedPatchHeightCdDeltaMinus1 << "SEINNPFCExtendedPatchHeightCdDeltaMinus1" << i;
    opts.addOptions()(extendedPatchHeightCdDeltaMinus1.str(), m_nnPostFilterSEICharacteristicsExtendedPatchHeightCdDeltaMinus1[i], 0u, "Specifies the extended vertical sample counts of a patch in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream overlap;
    overlap << "SEINNPFCOverlap" << i;
    opts.addOptions()(overlap.str(), m_nnPostFilterSEICharacteristicsOverlap[i], 0u, "Specifies the overlap in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream paddingType;
    paddingType << "SEINNPFCPaddingType" << i;
    opts.addOptions()(paddingType.str(), m_nnPostFilterSEICharacteristicsPaddingType[i], 0u, "Specifies the process of padding when referencing sample locations outside the boundaries of the cropped decoded output picture ");

    std::ostringstream lumaPadding;
    lumaPadding << "SEINNPFCLumaPadding" << i;
    opts.addOptions()(lumaPadding.str(), m_nnPostFilterSEICharacteristicsLumaPadding[i], 0u, "Specifies the luma padding when when the padding type is fixed padding ");

    std::ostringstream crPadding;
    crPadding << "SEINNPFCCrPadding" << i;
    opts.addOptions()(crPadding.str(), m_nnPostFilterSEICharacteristicsCrPadding[i], 0u, "Specifies the Cr padding when when the padding type is fixed padding ");

    std::ostringstream cbPadding;
    cbPadding << "SEINNPFCCbPadding" << i;
    opts.addOptions()(cbPadding.str(), m_nnPostFilterSEICharacteristicsCbPadding[i], 0u, "Specifies the Cb padding when when the padding type is fixed padding ");

    std::ostringstream complexityInfoPresentFlag;
    complexityInfoPresentFlag << "SEINNPFCComplexityInfoPresentFlag" << i;
    opts.addOptions()(complexityInfoPresentFlag.str(), m_nnPostFilterSEICharacteristicsComplexityInfoPresentFlag[i], false, "Specifies the value of nnpfc_complexity_info_present_flag in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream applicationPurposeTagUriPresentFlag;
    applicationPurposeTagUriPresentFlag << "SEINNPFCApplicationPurposeTagUriPresentFlag" << i;
    opts.addOptions()(applicationPurposeTagUriPresentFlag.str(), m_nnPostFilterSEICharacteristicsApplicationPurposeTagUriPresentFlag[i], false, "equal to 1 indicates that the nnpfc_application_purpose_tag_uri syntax element is present in this NNPFC SEI message. nnpfc_application_purpose_tag_uri_present_flag equal to 0 indicates that the nnpfc_application_purpose_tag_uri syntax element is not present in this NNPFC SEI message.");

    std::ostringstream applicationPurposeTagUri;
    applicationPurposeTagUri << "SEINNPFCApplicationPurposeTagUri" << i;
    opts.addOptions()(applicationPurposeTagUri.str(), m_nnPostFilterSEICharacteristicsApplicationPurposeTagUri[i], std::string(""), "specifies a tag URI with syntax and semantics as specified in IETF RFC 4151 identifying the application determined purpose of the NNPF, when nnpfc_purpose is equal to 0.");

#if NNPFC_SCAN_TYPE_IDC
    std::ostringstream scanTypeIdc;
    scanTypeIdc << "SEINNPFCScanTypeIdc" << i;
    opts.addOptions()(scanTypeIdc.str(), m_nnPostFilterSEICharacteristicsScanTypeIdc[i], 0u, "the preferred display method for the pictures output by the NNPF: not overscan (2), overscan (1), unknown or unspecified (0).");
#endif

    std::ostringstream forHumanViewingIdc;
    forHumanViewingIdc << "SEINNPFCForHumanViewingIdc" << i;
    opts.addOptions()(forHumanViewingIdc.str(), m_nnPostFilterSEICharacteristicsForHumanViewingIdc[i], 0u, "Specifies the user viewing usage level of a neural-network post-filter: optimal for human viewing (3), suitable (2), unsuitable (1), unknown (0)");

    std::ostringstream forMachineAnalysis;
    forMachineAnalysis << "SEINNPFCForMachineAnalysisIdc" << i;
    opts.addOptions()(forMachineAnalysis.str(), m_nnPostFilterSEICharacteristicsForMachineAnalysisIdc[i], 0u, "Specifies the machine analysis usage level of a neural-network post-filter: optimal for machine analysis (3), suitable (2), unsuitable (1), unknown (0)");

    std::ostringstream uriTag;
    uriTag << "SEINNPFCUriTag" << i;
    opts.addOptions()(
      uriTag.str(), m_nnPostFilterSEICharacteristicsUriTag[i], std::string(""),
      "Specifies the neural network uri tag in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream uri;
    uri << "SEINNPFCUri" << i;
    opts.addOptions()(
      uri.str(), m_nnPostFilterSEICharacteristicsUri[i], std::string(""),
      "Specifies the neural network information uri in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream parameterTypeIdc;
    parameterTypeIdc << "SEINNPFCParameterTypeIdc" << i;
    opts.addOptions()(parameterTypeIdc.str(), m_nnPostFilterSEICharacteristicsParameterTypeIdc[i], 0u, "Specifies the data type of parameters in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream log2ParameterBitLengthMinus3;
    log2ParameterBitLengthMinus3 << "SEINNPFCLog2ParameterBitLengthMinus3" << i;
    opts.addOptions()(log2ParameterBitLengthMinus3.str(), m_nnPostFilterSEICharacteristicsLog2ParameterBitLengthMinus3[i], 0u, "Indicates that the neural network does not use parameter of bit length greater than 2^(N+3) bits");

    std::ostringstream numParametersIdc;
    numParametersIdc << "SEINNPFCNumParametersIdc" << i;
    opts.addOptions()(numParametersIdc.str(), m_nnPostFilterSEICharacteristicsNumParametersIdc[i], 0u, "Specifies the maximum number of parameters ((2048<<NumParametersIdc)-1) in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream numKmacOperationsIdc;
    numKmacOperationsIdc << "SEINNPFCNumKmacOperationsIdc" << i;
    opts.addOptions()(numKmacOperationsIdc.str(), m_nnPostFilterSEICharacteristicsNumKmacOperationsIdc[i], 0u, "Specifies the maximum number of operations (KMAC) per pixel in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream totalKilobyteSize; 
    totalKilobyteSize << "SEINNPFCTotalKilobyteSize" << i; 
    opts.addOptions()(totalKilobyteSize.str(), m_nnPostFilterSEICharacteristicsTotalKilobyteSize[i], 0u, "Indicates the total size in kilobytes required to store the uncompressed NN parameters in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream payloadFilename;
    payloadFilename << "SEINNPFCPayloadFilename" << i;
    opts.addOptions()(payloadFilename.str(), m_nnPostFilterSEICharacteristicsPayloadFilename[i], std::string(""),
                      "Specifies the NNR bitstream in the Neural Network Post Filter Characteristics SEI message");

    std::ostringstream numberDecodedInputPics;
    numberDecodedInputPics << "SEINNPFCNumberInputDecodedPicsMinusOne" << i;
    opts.addOptions()(numberDecodedInputPics.str(), m_nnPostFilterSEICharacteristicsNumberInputDecodedPicturesMinus1[i], 0u, "Specifies the number of decoded output pictures used as input for the post processing filter");
    std::ostringstream numberInterpolatedPics;
    numberInterpolatedPics << "SEINNPFCNumberInterpolatedPics" << i;
    opts.addOptions()(numberInterpolatedPics.str(), cfg_nnPostFilterSEICharacteristicsInterpolatedPicturesList[i], cfg_nnPostFilterSEICharacteristicsInterpolatedPicturesList[i], "Number of pictures to interpolate");
    std::ostringstream numberExtrapolatedPicturesMinus1;
    numberExtrapolatedPicturesMinus1 << "SEINNPFCNumberExtrapolatedPicsMinus1" << i; 
    opts.addOptions()(numberExtrapolatedPicturesMinus1.str(), m_nnPostFilterSEICharacteristicsNumberExtrapolatedPicturesMinus1[i], 0u, "Number of pictures to extrapolate");
    std::ostringstream spatialExtrapolationLeftOffset;
    spatialExtrapolationLeftOffset << "SEINNPFCSpatialExtrapolationLeftOffset" << i; 
    opts.addOptions()(spatialExtrapolationLeftOffset.str(), m_nnPostFilterSEICharacteristicsSpatialExtrapolationLeftOffset[i], 0, "Left offset of spatial extrapolation");

    std::ostringstream spatialExtrapolationRightOffset;
    spatialExtrapolationRightOffset << "SEINNPFCSpatialExtrapolationRightOffset" << i; 
    opts.addOptions()(spatialExtrapolationRightOffset.str(), m_nnPostFilterSEICharacteristicsSpatialExtrapolationRightOffset[i], 0, "Right offset of spatial extrapolation");

    std::ostringstream spatialExtrapolationTopOffset;
    spatialExtrapolationTopOffset << "SEINNPFCSpatialExtrapolationTopOffset" << i; 
    opts.addOptions()(spatialExtrapolationTopOffset.str(), m_nnPostFilterSEICharacteristicsSpatialExtrapolationTopOffset[i], 0, "Top offset of spatial extrapolation");

    std::ostringstream spatialExtrapolationBottomOffset;
    spatialExtrapolationBottomOffset << "SEINNPFCSpatialExtrapolationLeftOffset" << i; 
    opts.addOptions()(spatialExtrapolationBottomOffset.str(), m_nnPostFilterSEICharacteristicsSpatialExtrapolationBottomOffset[i], 0, "Bottom offset of spatial extrapolation");
    std::ostringstream inbandPromptFlag;
    inbandPromptFlag << "SEINNPFCInbandPromptFlag" << i;
    opts.addOptions()(inbandPromptFlag.str(), m_nnPostFilterSEICharacteristicsInbandPromptFlag[i], false, "equal to 1 specifies that nnpfc_prompt syntax element is present and nnpfc_alignment_zero_bit_c syntax element may be present. nnpfc_spatial_extrapolation_prompt_present_flag equal to 0 specifies that nnpfc_prompt syntax element and nnpfc_alignment_zero_bit_c syntax element are not present.");
    std::ostringstream prompt;
    prompt << "SEINNPFCPrompt" << i;
    opts.addOptions()(prompt.str(), m_nnPostFilterSEICharacteristicsPrompt[i], std::string(""), "specifies the text string prompt");
    std::ostringstream InputPicOutputFlag;
    InputPicOutputFlag << "SEINNPFCInputPicOutputFlag" << i;
    opts.addOptions()(InputPicOutputFlag.str(), cfg_nnPostFilterSEICharacteristicsInputPicOutputFlagList[i], cfg_nnPostFilterSEICharacteristicsInputPicOutputFlagList[i], "Indicates whether NNPF will generate a corresponding output picture for the input picture");
    std::ostringstream absentInputPicZeroFlag;
    absentInputPicZeroFlag << "SEINNPFCAbsentInputPicZeroFlag" << i;
    opts.addOptions()(absentInputPicZeroFlag.str(), m_nnPostFilterSEICharacteristicsAbsentInputPicZeroFlag[i], false, "Specifies the value of nnpfc_absent_input_pic_zero_flag in the Neural Network Post Filter Characteristics SEI message");
#if JVET_AK0326_NNPF_SEED
    std::ostringstream inbandSeedFlag;
    inbandSeedFlag << "SEINNPFCInbandSeedFlag" << i;
    opts.addOptions()(inbandSeedFlag.str(), m_nnPostFilterSEICharacteristicsInbandSeedFlag[i], false, "Specifies the value of nnpfc_inband_seed_flag in the Neural Network Post Filter Characteristics SEI message");
    std::ostringstream seed;
    seed << "SEINNPFCSeed" << i;
    opts.addOptions()(seed.str(), m_nnPostFilterSEICharacteristicsSeed[i], 0u, "Indicates the seed value used as input for an NNPF");
#endif

    opts.addOptions()("SEINNPostFilterActivationEnabled", m_nnPostFilterSEIActivationEnabled, false, "Control use of the Neural Network Post Filter SEI on current picture");
    opts.addOptions()("SEINNPostFilterActivationUseSuffixSEI",  m_nnPostFilterSEIActivationUseSuffixSEI, false, "Code NNPFA SEI either as suffix (1) or prefix (0) SEI message");
    opts.addOptions()("SEINNPostFilterActivationTargetId", m_nnPostFilterSEIActivationTargetId, 0u, "Target id of the Neural Network Post Filter on current picture");
    opts.addOptions()("SEINNPostFilterActivationCancelFlag", m_nnPostFilterSEIActivationCancelFlag, false, "Control use of the target neural network post filter established by any previous NNPFA SEI message");
    opts.addOptions()("SEINNPostFilterActivationTargetBaseFlag", m_nnPostFilterSEIActivationTargetBaseFlag, false, "Specifies that the target NNPF is the base NNPF");
    opts.addOptions()("SEINNPostFilterActivationNoPrevCLVSFlag", m_nnPostFilterSEIActivationNoPrevCLVSFlag, false, "Specifies whether input pictures cannot (1) or can (0) originate from a previous CLVS");
    opts.addOptions()("SEINNPostFilterActivationNoFollCLVSFlag", m_nnPostFilterSEIActivationNoFollCLVSFlag, false, "Specifies whether input pictures cannot (1) or can (0) originate from a following CLVS");
    opts.addOptions()("SEINNPostFilterActivationPersistenceFlag", m_nnPostFilterSEIActivationPersistenceFlag, false, "Specifies the persistence of the target neural-network post-processing filter for the current layer");
    opts.addOptions()("SEINNPostFilterActivationOutputFlag", cfg_nnPostFilterSEIActivationOutputFlagList, cfg_nnPostFilterSEIActivationOutputFlagList, "Specifies a list indicating whether the NNPF-generated picture that corresponds to the input picture having index InpIdx[i] is output or not");
#if JVET_AJ0104_NNPFA_PROMPT_UPDATE
    opts.addOptions()("SEINNPostFilterActivationPromptUpdateFlag", m_nnPostFilterSEIActivationPromptUpdateFlag, false, "Specifies that nnpfa_prompt syntax element is present and nnpfa_alignment_zero_bit syntax element may be present");
    opts.addOptions()("SEINNPostFilterActivationPrompt", m_nnPostFilterSEIActivationPrompt, std::string(""), "Specifies the text string prompt used as input for the target NNPF");
#endif
#if JVET_AK0326_NNPF_SEED
    opts.addOptions()("SEINNPostFilterActivationSeedUpdateFlag", m_nnPostFilterSEIActivationSeedUpdateFlag, false, "Specifies the value of nnpfa_seed_update_flag in the Neural Network Post Filter Activation SEI message");
    opts.addOptions()("SEINNPostFilterActivationSeed", m_nnPostFilterSEIActivationSeed, 0u, "Specifies the seed value used as input for the target NNPF");
#endif
#if JVET_AJ0114_NNPFA_NUM_PIC_SHIFT
#if JVET_AL0075_NNPFA_SELECTED_INPUT_FLAG
    opts.addOptions()("SEINNPostFilterActivationSelectedInputFlag", m_nnPostFilterSEIActivationSelectedInputFlag, false, "Specifies whether nnpfa_num_input_pic_shift is written to the bitstream");
#endif
    opts.addOptions()("SEINNPostFilterActivationNumInputPicShift", m_nnPostFilterSEIActivationNumInputPicShift, 0u, "specifies the number of input pictures shift in the list of candidate input pictures to get the final input pictures for the target NNPF");
#endif
  }

  opts.addOptions()("SEITextDescriptionID", m_SEITextDescriptionID, 1u, "Identifier value of this text description information SEI message, must be in the range 1-16383");
  opts.addOptions()("SEITextDescriptionCancelFlag", m_SEITextCancelFlag, true, "Cancels the persistence of any previous text description information SEI message with the same txt_descr_purpose");
  opts.addOptions()("SEITextDescriptionIDCancelFlag", m_SEITextIDCancelFlag, true, "Cancels the persistence of any previous text description information SEI message with the same txt_descr_id and the same txt_descr_purpose");
  opts.addOptions()("SEITextDescriptionPersistenceFlag", m_SEITextPersistenceFlag, true, "Specifies the persistence of the text information description message for the current layer");
  opts.addOptions()("SEITextDescriptionPurpose", m_SEITextDescriptionPurpose, 0u, "Indicates the purpose of the text description, must be in the range 0-6");
  opts.addOptions()("SEITextDescriptionsNumStringsMinus1", m_SEITextNumStringsMinus1, 0u, "Indicates the number of entries plus 1 for txt_descr_string_lang[ i ] and txt_descr_string[ i ]");
  m_SEITextDescriptionStringLang.resize(256);
  m_SEITextDescriptionString.resize(256);
  for (int i=0; i<256; i++)
  {
    std::ostringstream stringLang;
    stringLang << "SEITextDescriptionStringLang" << i;
    opts.addOptions()(stringLang.str(), m_SEITextDescriptionStringLang[0], std::string(""), "Specifies the i-th language of the txt_descr_string[ i ]");
    std::ostringstream stringDesc;
    stringDesc << "SEITextDescriptionString" << i;
    opts.addOptions()(stringDesc.str(), m_SEITextDescriptionString[0], std::string(""), "Specifies the i-th text description information string");
  }

  po::setDefaults(opts);
  po::ErrorReporter err;
  const std::list<const char *> &argv_unhandled = po::scanArgv(opts, argc, (const char **) argv, err);

  if (m_gopBasedRPREnabledFlag)
  {
    m_upscaledOutput = 2;
    if (m_scalingRatioHor == 1.0 && m_scalingRatioVer == 1.0)
    {
      m_scalingRatioHor = 2.0;
      m_scalingRatioVer = 2.0;
    }
  }
  m_resChangeInClvsEnabled = m_scalingRatioHor != 1.0 || m_scalingRatioVer != 1.0 || m_gopBasedRPREnabledFlag || m_rprFunctionalityTestingEnabledFlag;
  m_resChangeInClvsEnabled = m_resChangeInClvsEnabled && m_rprEnabledFlag;

  if( m_constrainedRaslEncoding )
  {
    m_craAPSreset            = true;
    m_rprRASLtoolSwitch      = true;
  }
  else
  {
    m_craAPSreset            = false;
    m_rprRASLtoolSwitch      = false;
  }

  const size_t columnPos = frameRate.find_first_of(':');

  m_frameRate.num = std::stoi(frameRate.substr(0, columnPos));
  m_frameRate.den = columnPos == std::string::npos ? 1 : std::stoi(frameRate.substr(columnPos + 1));

  if (m_intraPeriod < -1)
  {
    // Set IntraPeriod to a multiple of -m_intraPeriod according to frame rate of source
    // When setting to m_intraPeriod to -32, it is changed to appropriate value according to CTC:
    // Frame rate | IntraPeriod
    //     20     |     32
    //     24     |     32
    //     30     |     32
    //     50     |     64
    //     60     |     64
    //    100     |     96
    const int ipBase = -m_intraPeriod;
    m_intraPeriod    = std::max((m_frameRate.getIntValRound() + ipBase / 2) / ipBase, 1) * ipBase;
  }

  if( m_fractionOfFrames != 1.0 )
  {
    m_framesToBeEncoded = int( m_framesToBeEncoded * m_fractionOfFrames );
  }

  if (m_resChangeInClvsEnabled && !m_switchPocPeriod)
  {
    m_switchPocPeriod = m_frameRate.getIntValRound() / 2 / m_gopSize * m_gopSize;
  }

  //Check the given value of intra period and decoding refresh type. If intra period is -1, set decoding refresh type to be equal to 0. And vice versa
  if (m_intraPeriod == -1)
  {
    m_intraRefreshType = 0;
  }
  if (!m_intraRefreshType)
  {
    m_intraPeriod = -1;
  }

#if GDR_ENABLED
  if ( m_gdrEnabled )
  {
    m_intraRefreshType = 3;
    m_intraQPOffset = 0;
    for (int i = 1; i < m_gopSize; i++)
    {
      m_GOPList[i].m_POC = -1;
    }
    m_gopSize = 1;

    int8_t sliceType = m_GOPList[0].m_sliceType;

    m_GOPList[0].m_POC                 = 1;
    m_GOPList[0].m_QPOffset            = 0;
    m_GOPList[0].m_QPOffsetModelOffset = 0;
    m_GOPList[0].m_QPOffsetModelScale = 0;
    m_GOPList[0].m_CbQPoffset = 0;
    m_GOPList[0].m_CrQPoffset = 0;
    m_GOPList[0].m_QPFactor = 1.0;
    m_GOPList[0].m_tcOffsetDiv2 = 0;
    m_GOPList[0].m_betaOffsetDiv2 = 0;
    m_GOPList[0].m_CbTcOffsetDiv2 = 0;
    m_GOPList[0].m_CbBetaOffsetDiv2 = 0;
    m_GOPList[0].m_CrTcOffsetDiv2 = 0;
    m_GOPList[0].m_CrBetaOffsetDiv2 = 0;
    m_GOPList[0].m_temporalId = 0;

    m_GOPList[0].m_numRefPicsActive0 = 4;
    m_GOPList[0].m_numRefPics0 = 4;
    m_GOPList[0].m_deltaRefPics0[0] = 1;
    m_GOPList[0].m_deltaRefPics0[1] = 2;
    m_GOPList[0].m_deltaRefPics0[2] = 3;
    m_GOPList[0].m_deltaRefPics0[3] = 4;

    if (sliceType == 'B')
    {
      m_GOPList[0].m_numRefPicsActive1 = 4;
      m_GOPList[0].m_numRefPics1 = 4;
      m_GOPList[0].m_deltaRefPics1[0] = 1;
      m_GOPList[0].m_deltaRefPics1[1] = 2;
      m_GOPList[0].m_deltaRefPics1[2] = 3;
      m_GOPList[0].m_deltaRefPics1[3] = 4;
    }

    m_BIO  = false;
    m_DMVR = false;
    m_SMVD = false;

    if (m_gdrPeriod < 0)
    {
      m_gdrPeriod = m_frameRate.getIntValRound() * 2;
    }

    if (m_gdrInterval < 0)
    {
      m_gdrInterval = m_frameRate.getIntValRound();
    }

    if (m_gdrPocStart < 0)
    {
      m_gdrPocStart = m_gdrPeriod;
    }

    if (m_intraPeriod == -1)
    {
      m_frameRate = (m_frameRate.num == 0) ? Fraction{ 30, 1 } : m_frameRate;
      if (m_gdrPocStart % m_frameRate.getIntValRound() != 0)
      {
        m_intraPeriod = -1;
      }
      else
      {
        m_intraPeriod = m_gdrPeriod;
      }
    }
  }
#endif

  m_bpDeltasGOPStructure = false;
  if (m_gopSize == 16)
  {
    if ((m_GOPList[0].m_POC == 16 && m_GOPList[0].m_temporalId == 0 )
        && (m_GOPList[1].m_POC == 8 && m_GOPList[1].m_temporalId == 1 )
        && (m_GOPList[2].m_POC == 4 && m_GOPList[2].m_temporalId == 2 )
        && (m_GOPList[3].m_POC == 2 && m_GOPList[3].m_temporalId == 3 )
        && (m_GOPList[4].m_POC == 1 && m_GOPList[4].m_temporalId == 4 )
        && (m_GOPList[5].m_POC == 3 && m_GOPList[5].m_temporalId == 4 )
        && (m_GOPList[6].m_POC == 6 && m_GOPList[6].m_temporalId == 3 )
        && (m_GOPList[7].m_POC == 5 && m_GOPList[7].m_temporalId == 4 )
        && (m_GOPList[8].m_POC == 7 && m_GOPList[8].m_temporalId == 4 )
        && (m_GOPList[9].m_POC == 12 && m_GOPList[9].m_temporalId == 2 )
        && (m_GOPList[10].m_POC == 10 && m_GOPList[10].m_temporalId == 3 )
        && (m_GOPList[11].m_POC == 9 && m_GOPList[11].m_temporalId == 4 )
        && (m_GOPList[12].m_POC == 11 && m_GOPList[12].m_temporalId == 4 )
        && (m_GOPList[13].m_POC == 14 && m_GOPList[13].m_temporalId == 3 )
        && (m_GOPList[14].m_POC == 13 && m_GOPList[14].m_temporalId == 4 )
        && (m_GOPList[15].m_POC == 15 && m_GOPList[15].m_temporalId == 4 ))
    {
      m_bpDeltasGOPStructure = true;
    }
  }
  else if (m_gopSize == 8)
  {
    if ((m_GOPList[0].m_POC == 8 && m_GOPList[0].m_temporalId == 0 )
        && (m_GOPList[1].m_POC == 4 && m_GOPList[1].m_temporalId == 1 )
        && (m_GOPList[2].m_POC == 2 && m_GOPList[2].m_temporalId == 2 )
        && (m_GOPList[3].m_POC == 1 && m_GOPList[3].m_temporalId == 3 )
        && (m_GOPList[4].m_POC == 3 && m_GOPList[4].m_temporalId == 3 )
        && (m_GOPList[5].m_POC == 6 && m_GOPList[5].m_temporalId == 2 )
        && (m_GOPList[6].m_POC == 5 && m_GOPList[6].m_temporalId == 3 )
        && (m_GOPList[7].m_POC == 7 && m_GOPList[7].m_temporalId == 3 ))
    {
      m_bpDeltasGOPStructure = true;
    }
  }
  else
  {
    m_bpDeltasGOPStructure = false;
  }
  for (int i = 0; m_GOPList[i].m_POC != -1 && i < MAX_GOP + 1; i++)
  {
    m_RPLList0[i].m_POC = m_RPLList1[i].m_POC = m_GOPList[i].m_POC;
    m_RPLList0[i].m_temporalId = m_RPLList1[i].m_temporalId = m_GOPList[i].m_temporalId;
    m_RPLList0[i].m_refPic = m_RPLList1[i].m_refPic = m_GOPList[i].m_refPic;
    m_RPLList0[i].m_sliceType = m_RPLList1[i].m_sliceType = m_GOPList[i].m_sliceType;
    m_RPLList0[i].m_isEncoded = m_RPLList1[i].m_isEncoded = m_GOPList[i].m_isEncoded;

    m_RPLList0[i].m_numRefPicsActive = m_GOPList[i].m_numRefPicsActive0;
    m_RPLList1[i].m_numRefPicsActive = m_GOPList[i].m_numRefPicsActive1;
    m_RPLList0[i].m_numRefPics = m_GOPList[i].m_numRefPics0;
    m_RPLList1[i].m_numRefPics = m_GOPList[i].m_numRefPics1;
    m_RPLList0[i].m_ltrpInSliceHeaderFlag = m_GOPList[i].m_ltrpInSliceHeaderFlag;
    m_RPLList1[i].m_ltrpInSliceHeaderFlag = m_GOPList[i].m_ltrpInSliceHeaderFlag;
    for (int j = 0; j < m_GOPList[i].m_numRefPics0; j++)
      m_RPLList0[i].m_deltaRefPics[j] = m_GOPList[i].m_deltaRefPics0[j];
    for (int j = 0; j < m_GOPList[i].m_numRefPics1; j++)
      m_RPLList1[i].m_deltaRefPics[j] = m_GOPList[i].m_deltaRefPics1[j];
    for (int j = 0; j < MAX_NUM_REF_PICS; j++)
    {
      m_RPLList0[i].m_layerRef[j]  = m_GOPList[i].m_layerRef0[j];
      m_RPLList1[i].m_layerRef[j]  = m_GOPList[i].m_layerRef1[j];
    }
  }

  if (m_compositeRefEnabled)
  {
    for (int i = 0; i < m_gopSize; i++)
    {
      m_GOPList[i].m_POC *= 2;
      m_RPLList0[i].m_POC *= 2;
      m_RPLList1[i].m_POC *= 2;
      for (int j = 0; j < m_RPLList0[i].m_numRefPics; j++)
      {
        m_RPLList0[i].m_deltaRefPics[j] *= 2;
      }
      for (int j = 0; j < m_RPLList1[i].m_numRefPics; j++)
      {
        m_RPLList1[i].m_deltaRefPics[j] *= 2;
      }
    }
  }

  for (std::list<const char *>::const_iterator it = argv_unhandled.begin(); it != argv_unhandled.end(); it++)
  {
    msg( ERROR, "Unhandled argument ignored: `%s'\n", *it);
  }

  if (argc == 1 || do_help)
  {
    /* argc == 1: no options have been specified */
    po::doHelp(std::cout, opts);
    return false;
  }

  if (err.is_errored)
  {
    if (!warnUnknowParameter)
    {
      /* error report has already been printed on stderr */
      return false;
    }
  }

  g_verbosity = MsgLevel( m_verbosity );


  /*
   * Set any derived parameters
   */

  if ( m_sourceScalingRatioHor != 1.0 || m_sourceScalingRatioVer != 1.0 ) 
  {
    m_sourceWidthBeforeScale = m_sourceWidth;
    m_sourceHeightBeforeScale = m_sourceHeight;
    m_sourceWidth    = int(round(m_sourceWidth*m_sourceScalingRatioHor));
    m_sourceHeight   = int(round(m_sourceHeight*m_sourceScalingRatioVer));
  }
  else 
  {
    m_sourceWidthBeforeScale = 0;
    m_sourceHeightBeforeScale = 0;
  }
#if EXTENSION_360_VIDEO
  m_inputFileWidth = m_sourceWidth;
  m_inputFileHeight = m_sourceHeight;
  m_ext360.setMaxCUInfo(m_ctuSize, 1 << MIN_CU_LOG2);
#endif

  if (!inputPathPrefix.empty() && inputPathPrefix.back() != '/' && inputPathPrefix.back() != '\\' )
  {
    inputPathPrefix += "/";
  }
  m_inputFileName   = inputPathPrefix + m_inputFileName;

  if (m_firstValidFrame < 0)
  {
    m_firstValidFrame = m_frameSkip;
  }
  if (m_lastValidFrame < 0)
  {
    m_lastValidFrame = m_firstValidFrame + m_framesToBeEncoded - 1;
  }

  if( m_temporalSubsampleRatio < 1)
  {
    EXIT ( "Error: TemporalSubsampleRatio must be greater than 0" );
  }

  m_framesToBeEncoded = ( m_framesToBeEncoded + m_temporalSubsampleRatio - 1 ) / m_temporalSubsampleRatio;
  m_adIntraLambdaModifier = cfg_adIntraLambdaModifier.values;
  if(m_isField)
  {
    //Frame height
    m_iSourceHeightOrg = m_sourceHeight;
    //Field height
    m_sourceHeight = m_sourceHeight >> 1;
    //number of fields to encode
    m_framesToBeEncoded *= 2;
  }
  if ( m_subPicInfoPresentFlag )
  {
    CHECK( m_numSubPics > MAX_NUM_SUB_PICS || m_numSubPics < 1, "Number of subpicture must be within 1 to 2^16" )
    if (!m_subPicSameSizeFlag)
    {
      CHECK(cfg_subPicCtuTopLeftX.values.size() != m_numSubPics, "Number of SubPicCtuTopLeftX values must be equal to NumSubPics");
      CHECK(cfg_subPicCtuTopLeftY.values.size() != m_numSubPics, "Number of SubPicCtuTopLeftY values must be equal to NumSubPics");
      CHECK(cfg_subPicWidth.values.size() != m_numSubPics, "Number of SubPicWidth values must be equal to NumSubPics");
      CHECK(cfg_subPicHeight.values.size() != m_numSubPics, "Number of SubPicHeight values must be equal to NumSubPics");
    }
    else
    {
      CHECK(cfg_subPicCtuTopLeftX.values.size() != 0, "Number of SubPicCtuTopLeftX values must be equal to 0");
      CHECK(cfg_subPicCtuTopLeftY.values.size() != 0, "Number of SubPicCtuTopLeftY values must be equal to 0");
      CHECK(cfg_subPicWidth.values.size() != 1, "Number of SubPicWidth values must be equal to 1");
      CHECK(cfg_subPicHeight.values.size() != 1, "Number of SubPicHeight values must be equal to 1");
    }
    CHECK( cfg_subPicTreatedAsPicFlag.values.size() != m_numSubPics, "Number of SubPicTreatedAsPicFlag values must be equal to NumSubPics");
    CHECK( cfg_loopFilterAcrossSubpicEnabledFlag.values.size() != m_numSubPics, "Number of LoopFilterAcrossSubpicEnabledFlag values must be equal to NumSubPics");
    if (m_subPicIdMappingExplicitlySignalledFlag)
    {
      CHECK( cfg_subPicId.values.size() != m_numSubPics, "Number of SubPicId values must be equal to NumSubPics");
    }
    m_subPicCtuTopLeftX                 = cfg_subPicCtuTopLeftX.values;
    m_subPicCtuTopLeftY                 = cfg_subPicCtuTopLeftY.values;
    m_subPicWidth                       = cfg_subPicWidth.values;
    m_subPicHeight                      = cfg_subPicHeight.values;
    m_subPicTreatedAsPicFlag            = cfg_subPicTreatedAsPicFlag.values;
    m_loopFilterAcrossSubpicEnabledFlag = cfg_loopFilterAcrossSubpicEnabledFlag.values;
    if (m_subPicIdMappingExplicitlySignalledFlag)
    {
      for (int i=0; i < m_numSubPics; i++)
      {
        m_subPicId[i]                   = cfg_subPicId.values[i];
      }
    }
    uint32_t tmpWidthVal  = (m_sourceWidth + m_ctuSize - 1) / m_ctuSize;
    uint32_t tmpHeightVal = (m_sourceHeight + m_ctuSize - 1) / m_ctuSize;
    if (!m_subPicSameSizeFlag)
    {
      for (int i = 0; i < m_numSubPics; i++)
      {
        CHECK(m_subPicCtuTopLeftX[i] + m_subPicWidth[i] > tmpWidthVal, "Subpicture must not exceed picture boundary");
        CHECK(m_subPicCtuTopLeftY[i] + m_subPicHeight[i] > tmpHeightVal, "Subpicture must not exceed picture boundary");
      }
    }
    else
    {
      uint32_t numSubpicCols = tmpWidthVal / m_subPicWidth[0];
      CHECK(tmpWidthVal % m_subPicWidth[0] != 0, "sps_subpic_width_minus1[0] is invalid.");
      CHECK(tmpHeightVal % m_subPicHeight[0] != 0, "sps_subpic_height_minus1[0] is invalid.");
      CHECK(numSubpicCols * (tmpHeightVal / m_subPicHeight[0]) != m_numSubPics, "when sps_subpic_same_size_flag is equal to, sps_num_subpics_minus1 is invalid");
    }
    // automatically determine subpicture ID lenght in case it is not specified
    if (m_subPicIdLen == 0)
    {
      if (m_subPicIdMappingExplicitlySignalledFlag)
      {
        // use the heighest specified ID
        auto maxIdVal = std::max_element(m_subPicId.begin(),m_subPicId.end());
        m_subPicIdLen = ceilLog2(*maxIdVal);
      }
      else
      {
        // use the number of subpictures
        m_subPicIdLen = ceilLog2(m_numSubPics);
      }
    }

    CHECK( m_subPicIdLen > 16, "SubPicIdLen must not exceed 16 bits" );
    CHECK(m_resChangeInClvsEnabled, "resolution change in CLVS and subpictures cannot be enabled together");
  }

  if (m_virtualBoundariesPresentFlag)
  {
    if (m_sourceWidth <= 8)
      CHECK(m_numVerVirtualBoundaries != 0, "The number of vertical virtual boundaries shall be 0 when the picture width is less than or equal to 8");

    if (m_sourceHeight <= 8)
      CHECK(m_numHorVirtualBoundaries != 0, "The number of horizontal virtual boundaries shall be 0 when the picture height is less than or equal to 8");
  }

  if (m_cfgSubpictureLevelInfoSEI.m_enabled)
  {
    CHECK (m_numSubPics != m_cfgSubpictureLevelInfoSEI.m_numSubpictures, "NumSubPics must be equal to SEISubpicLevelInfoNumSubpics" );
    CHECK (m_cfgSubpictureLevelInfoSEI.m_sliMaxSublayers != m_maxSublayers, "SEISubpicLevelInfoMaxSublayers must be equal to vps_max_sublayers");
    if (m_cfgSubpictureLevelInfoSEI.hasSublayerInfo)
    {
      CHECK(cfg_sliRefLevels.values.size() < m_maxSublayers, "when sliSublayerInfoPresentFlag = 1, the number of reference levels must be greater than or equal to sublayers");
    }
    if (m_cfgSubpictureLevelInfoSEI.m_explicitFraction)
    {
      m_cfgSubpictureLevelInfoSEI.m_fractions = cfg_sliFractions.values;
      m_cfgSubpictureLevelInfoSEI.m_refLevels = cfg_sliRefLevels.values;
      if (m_cfgSubpictureLevelInfoSEI.hasSublayerInfo)
      {
        CHECK((int)cfg_sliRefLevels.values.size() / m_maxSublayers * m_cfgSubpictureLevelInfoSEI.m_numSubpictures * m_cfgSubpictureLevelInfoSEI.m_sliMaxSublayers != cfg_sliFractions.values.size(),
          "when sliSublayerInfoPresentFlag = 1, the number  of subpicture level fractions must be equal to the numer of subpictures times the number of reference levels times the number of sublayers");
      }
      else
      {
        CHECK((int)cfg_sliRefLevels.values.size() * m_cfgSubpictureLevelInfoSEI.m_numSubpictures != cfg_sliFractions.values.size(), "when sliSublayerInfoPresentFlag = 0, the number  of subpicture level fractions must be equal to the numer of subpictures times the number of reference levels");
      }
    }
    m_cfgSubpictureLevelInfoSEI.m_nonSubpicLayersFraction = cfg_sliNonSubpicLayersFractions.values;
    if (m_cfgSubpictureLevelInfoSEI.hasSublayerInfo)
    {
      CHECK((int)cfg_sliNonSubpicLayersFractions.values.size() != ( cfg_sliRefLevels.values.size() * m_cfgSubpictureLevelInfoSEI.m_numSubpictures ),
        "when sliSublayerInfoPresentFlag = 1, the number  of non-subpicture level fractions must be equal to the numer of reference levels times the number of sublayers");
    }
    else
    {
      CHECK((int)cfg_sliNonSubpicLayersFractions.values.size() != ( cfg_sliRefLevels.values.size() ),
        "when sliSublayerInfoPresentFlag = 0, the number  of non-subpicture level fractions must be equal to the numer of reference levels");
    }
  }

  if ((m_maxLayers>2) && m_gopBasedRPREnabledFlag)
  {
    msg(ERROR, "*************************************************************************\n");
    msg(ERROR, "* GOP based RPR is only implemented for max two layers *\n");
    msg(ERROR, "*************************************************************************\n");
    m_gopBasedRPREnabledFlag = false;
    m_scalingRatioHor = 1.0;
    m_scalingRatioVer = 1.0;
    m_resChangeInClvsEnabled = m_scalingRatioHor != 1.0 || m_scalingRatioVer != 1.0 || m_gopBasedRPREnabledFlag || m_rprFunctionalityTestingEnabledFlag;
  }
  if ((m_maxLayers > 2) && m_rprFunctionalityTestingEnabledFlag)
  {
    msg(ERROR, "*************************************************************************\n");
    msg(ERROR, "* RPR functionality testing is only implemented for max two layers *\n");
    msg(ERROR, "*************************************************************************\n");
    m_rprFunctionalityTestingEnabledFlag = false;
    m_resChangeInClvsEnabled = m_scalingRatioHor != 1.0 || m_scalingRatioVer != 1.0 || m_gopBasedRPREnabledFlag || m_rprFunctionalityTestingEnabledFlag;
  }

  if (m_costMode != COST_LOSSLESS_CODING && m_mixedLossyLossless)
  {
    m_mixedLossyLossless = 0;
    msg(WARNING, "*************************************************************************\n");
    msg(WARNING, "* Mixed lossy lossles coding cannot enable in lossy costMode *\n");
    msg(WARNING, "* Forcely disabled  m_mixedLossyLossless *\n");
    msg(WARNING, "*************************************************************************\n");
  }
  if (!m_mixedLossyLossless && cfgSliceLosslessArray.values.size() > 0)
  {
    msg(WARNING, "*************************************************************************\n");
    msg(WARNING, "* Mixed lossy lossles coding is not enabled *\n");
    msg(WARNING, "* ignoring the value of SliceLosslessArray *\n");
    msg(WARNING, "*************************************************************************\n");
  }

  if (m_costMode == COST_LOSSLESS_CODING && m_mixedLossyLossless)
  {
    m_sliceLosslessArray.resize(cfgSliceLosslessArray.values.size());
    for (uint32_t i = 0; i < cfgSliceLosslessArray.values.size(); i++)
    {
      m_sliceLosslessArray[i] = cfgSliceLosslessArray.values[i];
    }
  }

  if( m_picPartitionFlag )
  {
    // store tile column widths
    m_tileColumnWidth.resize(cfgTileColumnWidth.values.size());
    for(uint32_t i=0; i<cfgTileColumnWidth.values.size(); i++)
    {
      m_tileColumnWidth[i]=cfgTileColumnWidth.values[i];
    }

    // store tile row heights
    m_tileRowHeight.resize(cfgTileRowHeight.values.size());
    for(uint32_t i=0; i<cfgTileRowHeight.values.size(); i++)
    {
      m_tileRowHeight[i]=cfgTileRowHeight.values[i];
    }

    // store rectangular slice positions
    if( !m_rasterSliceFlag )
    {
      m_rectSlicePos.resize(cfgRectSlicePos.values.size());
      for(uint32_t i=0; i<cfgRectSlicePos.values.size(); i++)
      {
        m_rectSlicePos[i]=cfgRectSlicePos.values[i];
      }
    }

    // store raster-scan slice sizes
    else
    {
      m_rasterSliceSize.resize(cfgRasterSliceSize.values.size());
      for(uint32_t i=0; i<cfgRasterSliceSize.values.size(); i++)
      {
        m_rasterSliceSize[i]=cfgRasterSliceSize.values[i];
      }
    }
  }
  else
  {
    m_tileColumnWidth.clear();
    m_tileRowHeight.clear();
    m_rectSlicePos.clear();
    m_rasterSliceSize.clear();
    m_rectSliceFixedWidth = 0;
    m_rectSliceFixedHeight = 0;
  }

  m_numSubProfile = (uint8_t) cfg_SubProfile.values.size();
  m_subProfile.resize(m_numSubProfile);
  for (uint8_t i = 0; i < m_numSubProfile; ++i)
  {
    m_subProfile[i] = cfg_SubProfile.values[i];
  }
  /* rules for input, output and internal bitdepths as per help text */
  if (m_msbExtendedBitDepth[ChannelType::LUMA] == 0)
  {
    m_msbExtendedBitDepth[ChannelType::LUMA] = m_inputBitDepth[ChannelType::LUMA];
  }
  if (m_msbExtendedBitDepth[ChannelType::CHROMA] == 0)
  {
    m_msbExtendedBitDepth[ChannelType::CHROMA] = m_msbExtendedBitDepth[ChannelType::LUMA];
  }
  if (m_internalBitDepth[ChannelType::LUMA] == 0)
  {
    m_internalBitDepth[ChannelType::LUMA] = m_msbExtendedBitDepth[ChannelType::LUMA];
  }
  m_internalBitDepth[ChannelType::CHROMA] = m_internalBitDepth[ChannelType::LUMA];
  if (m_inputBitDepth[ChannelType::CHROMA] == 0)
  {
    m_inputBitDepth[ChannelType::CHROMA] = m_inputBitDepth[ChannelType::LUMA];
  }
  if (m_outputBitDepth[ChannelType::LUMA] == 0)
  {
    m_outputBitDepth[ChannelType::LUMA] = m_internalBitDepth[ChannelType::LUMA];
  }
  if (m_outputBitDepth[ChannelType::CHROMA] == 0)
  {
    m_outputBitDepth[ChannelType::CHROMA] = m_outputBitDepth[ChannelType::LUMA];
  }

  m_inputChromaFormatIDC = numberToChromaFormat(tmpInputChromaFormat);
  m_chromaFormatIdc = ((tmpChromaFormat == 0) ? (m_inputChromaFormatIDC) : (numberToChromaFormat(tmpChromaFormat)));
#if EXTENSION_360_VIDEO
  m_ext360.processOptions(ext360CfgContext);
#endif

  for (int i = 0; i < MAX_NUM_NN_POST_FILTERS; ++i)
  {
    m_nnPostFilterSEICharacteristicsNumberInterpolatedPictures[i] = cfg_nnPostFilterSEICharacteristicsInterpolatedPicturesList[i].values;
    if (m_nnPostFilterSEICharacteristicsNumberInterpolatedPictures[i].size() == 0)
    {
      m_nnPostFilterSEICharacteristicsNumberInterpolatedPictures[i].push_back(0);
    }

    for(int j=0; j<m_nnPostFilterSEICharacteristicsNumberInterpolatedPictures[i].size(); ++j)
    {
      CHECK(m_nnPostFilterSEICharacteristicsNumberInterpolatedPictures[i][j] > 63, "The value of nnpfc_interpolated_pics[i] shall be in the range of 0 to 63, inclusive");
    }
    CHECK(int(m_nnPostFilterSEICharacteristicsNumberInterpolatedPictures[i].size()) < int(m_nnPostFilterSEICharacteristicsNumberInputDecodedPicturesMinus1[i]) - 1, "Number Interpolated Pictures List must be greater than number of decoder pictures list");

    m_nnPostFilterSEICharacteristicsInputPicOutputFlag[i] = cfg_nnPostFilterSEICharacteristicsInputPicOutputFlagList[i].values;
    if (m_nnPostFilterSEICharacteristicsNumberInputDecodedPicturesMinus1[i] == 0)
    {
      if ((m_nnPostFilterSEICharacteristicsPurpose[i] & 0xff) != NNPC_PurposeType::TEMPORAL_EXTRAPOLATION)
      {
        m_nnPostFilterSEICharacteristicsInputPicOutputFlag[i] = {1};
      }
      else
      {
        m_nnPostFilterSEICharacteristicsInputPicOutputFlag[i] = {0};
      }
    }
    else
    {
      CHECK(int(m_nnPostFilterSEICharacteristicsInputPicOutputFlag[i].size()) < int(m_nnPostFilterSEICharacteristicsNumberInputDecodedPicturesMinus1[i]) + 1, "Number of input picture output flags cannot be less than number of input decoded pictures");
    }
  }

  m_nnPostFilterSEIActivationOutputFlag = cfg_nnPostFilterSEIActivationOutputFlagList.values;

  // TODO: check whether values are within valid range
  m_chromaSampleLocType            = static_cast<Chroma420LocType>(chromaSampleLocType);
  m_chromaSampleLocTypeTopField    = static_cast<Chroma420LocType>(chromaSampleLocTypeTopField);
  m_chromaSampleLocTypeBottomField = static_cast<Chroma420LocType>(chromaSampleLocTypeBottomField);

  if (isY4mFileExt(m_inputFileName))
  {
    int          width          = 0;
    int          height         = 0;
    Fraction     frameRate;
    int          inputBitDepth  = 0;
    ChromaFormat chromaFormat = ChromaFormat::_420;
    Chroma420LocType locType        = Chroma420LocType::UNSPECIFIED;

    VideoIOYuv   inputFile;
    inputFile.parseY4mFileHeader(m_inputFileName, width, height, frameRate, inputBitDepth, chromaFormat, locType);
    if (width != m_sourceWidth || height != m_sourceHeight || frameRate != m_frameRate
        || inputBitDepth != m_inputBitDepth[ChannelType::LUMA] || chromaFormat != m_chromaFormatIdc
        || locType != m_chromaSampleLocType)
    {
      msg(WARNING, "\nWarning: Y4M file info is different from input setting. Using the info from Y4M file\n");
      m_sourceWidth            = width;
      m_sourceHeight           = height;
      m_frameRate              = frameRate;
      m_inputBitDepth.fill(inputBitDepth);
      m_chromaFormatIdc        = chromaFormat;
      m_msbExtendedBitDepth    = m_inputBitDepth;
      if (m_writeVuiHrdFromY4m)
      {
        m_chromaSampleLocType    = locType;
      }
    }

    m_progressiveSourceFlag = true;   // TODO: update when processing of interlaced y4m files is supported
    if (m_chromaFormatIdc == ChromaFormat::_420 && m_chromaSampleLocType != Chroma420LocType::UNSPECIFIED
      && m_writeVuiHrdFromY4m)
    {
      m_chromaLocInfoPresentFlag = true;
      m_vuiParametersPresentFlag = true;
    }
  }

  CHECK( !( tmpWeightedPredictionMethod >= 0 && tmpWeightedPredictionMethod <= WP_PER_PICTURE_WITH_HISTOGRAM_AND_PER_COMPONENT_AND_CLIPPING_AND_EXTENSION ), "Error in cfg" );
  m_weightedPredictionMethod = WeightedPredictionMethod(tmpWeightedPredictionMethod);

  CHECK( tmpFastInterSearchMode<0 || tmpFastInterSearchMode>FASTINTERSEARCH_MODE3, "Error in cfg" );
  m_fastInterSearchMode = FastInterSearchMode(tmpFastInterSearchMode);

  CHECK(tmpMotionEstimationSearchMethod < to_underlying(MESearchMethod::FULL)
          || tmpMotionEstimationSearchMethod >= to_underlying(MESearchMethod::NUM),
        "Error in cfg");
  m_motionEstimationSearchMethod=MESearchMethod(tmpMotionEstimationSearchMethod);

  if (extendedProfile == ExtendedProfileName::AUTO)
  {
    if (xAutoDetermineProfile())
    {
      EXIT( "Unable to determine profile from configured settings");
    }
  }
  else
  {
    switch (extendedProfile)
    {
    case ExtendedProfileName::NONE: m_profile = Profile::NONE; break;
    case ExtendedProfileName::MAIN_10: m_profile = Profile::MAIN_10; break;
    case ExtendedProfileName::MAIN_10_444: m_profile = Profile::MAIN_10_444; break;
    case ExtendedProfileName::MAIN_10_STILL_PICTURE: m_profile = Profile::MAIN_10_STILL_PICTURE; break;
    case ExtendedProfileName::MAIN_10_444_STILL_PICTURE: m_profile = Profile::MAIN_10_444_STILL_PICTURE; break;
    case ExtendedProfileName::MULTILAYER_MAIN_10: m_profile = Profile::MULTILAYER_MAIN_10; break;
    case ExtendedProfileName::MULTILAYER_MAIN_10_444: m_profile = Profile::MULTILAYER_MAIN_10_444; break;
    case ExtendedProfileName::MULTILAYER_MAIN_10_STILL_PICTURE:
      m_profile = Profile::MULTILAYER_MAIN_10_STILL_PICTURE;
      break;
    case ExtendedProfileName::MULTILAYER_MAIN_10_444_STILL_PICTURE:
      m_profile = Profile::MULTILAYER_MAIN_10_444_STILL_PICTURE;
      break;
    case ExtendedProfileName::MAIN_12:
      m_profile = Profile::MAIN_12; break;
    case ExtendedProfileName::MAIN_12_444:
      m_profile = Profile::MAIN_12_444; break;
    case ExtendedProfileName::MAIN_16_444:
      m_profile = Profile::MAIN_16_444; break;
    case ExtendedProfileName::MAIN_12_INTRA:
      m_profile = Profile::MAIN_12_INTRA; break;
    case ExtendedProfileName::MAIN_12_444_INTRA:
      m_profile = Profile::MAIN_12_444_INTRA; break;
    case ExtendedProfileName::MAIN_16_444_INTRA:
      m_profile = Profile::MAIN_16_444_INTRA; break;
    case ExtendedProfileName::MAIN_12_STILL_PICTURE:
      m_profile = Profile::MAIN_12_STILL_PICTURE; break;
    case ExtendedProfileName::MAIN_12_444_STILL_PICTURE:
      m_profile = Profile::MAIN_12_444_STILL_PICTURE; break;
    case ExtendedProfileName::MAIN_16_444_STILL_PICTURE:
      m_profile = Profile::MAIN_16_444_STILL_PICTURE; break;
    default: EXIT("Unable to determine profile from configured settings"); break;
    }
  }

  {
    m_chromaFormatConstraint =
      (tmpConstraintChromaFormat == 0) ? m_chromaFormatIdc : numberToChromaFormat(tmpConstraintChromaFormat);
    m_maxChromaFormatConstraintIdc = static_cast<ChromaFormat>(tmpMaxChromaFormatConstraintIdc);

    if (m_bitDepthConstraint == 0)
    {
      if (m_profile != Profile::NONE)
      {
        const ProfileFeatures *features = ProfileFeatures::getProfileFeatures(m_profile);
        CHECK(features->profile != m_profile, "Profile not found");
        m_bitDepthConstraint = features->maxBitDepth;
      }
      else // m_profile == Profile::NONE
      {
        m_bitDepthConstraint = 16; // max value - unconstrained.
      }
    }
    CHECK(m_bitDepthConstraint < m_internalBitDepth[ChannelType::LUMA],
          "MaxBitDepthConstraint setting does not allow the specified luma bit depth to be coded.");
    CHECK(m_bitDepthConstraint < m_internalBitDepth[ChannelType::CHROMA],
          "MaxBitDepthConstraint setting does not allow the specified chroma bit depth to be coded.");
    CHECK(m_chromaFormatConstraint < m_chromaFormatIdc,
          "MaxChromaFormatConstraint setting does not allow the specified chroma format to be coded.");
    CHECK(m_chromaFormatConstraint >= ChromaFormat::NUM, "Bad value given for MaxChromaFormatConstraint setting.")
    CHECK(m_bitDepthConstraint < 8 || m_bitDepthConstraint>16, "MaxBitDepthConstraint setting must be in the range 8 to 16 (inclusive)");
  }

  m_inputColourSpaceConvert = stringToInputColourSpaceConvert(inputColourSpaceConvert, true);
  m_rgbFormat = m_inputColourSpaceConvert == IPCOLOURSPACE_RGBtoGBR && m_chromaFormatIdc == ChromaFormat::_444;
  if (m_profile == Profile::MAIN_12 || m_profile == Profile::MAIN_12_INTRA || m_profile == Profile::MAIN_12_STILL_PICTURE ||
      m_profile == Profile::MAIN_12_444 || m_profile == Profile::MAIN_12_444_INTRA || m_profile == Profile::MAIN_12_444_STILL_PICTURE ||
      m_profile == Profile::MAIN_16_444 || m_profile == Profile::MAIN_16_444_INTRA || m_profile == Profile::MAIN_16_444_STILL_PICTURE)
  {
    m_gciPresentFlag = true;
  }
  if (m_profile == Profile::MAIN_12_INTRA || m_profile == Profile::MAIN_12_444_INTRA || m_profile == Profile::MAIN_16_444_INTRA)
  {
    CHECK(m_intraPeriod != 1, "IntraPeriod setting must be 1 for Intra profiles")
  }
  if (m_profile == Profile::MULTILAYER_MAIN_10_STILL_PICTURE || m_profile == Profile::MAIN_10_STILL_PICTURE ||
      m_profile == Profile::MAIN_12_STILL_PICTURE || m_profile == Profile::MAIN_12_444_STILL_PICTURE || m_profile == Profile::MAIN_16_444_STILL_PICTURE)
  {
    CHECK(m_framesToBeEncoded != 1, "FramesToBeEncoded setting must be 1 for Still Picture profiles")
  }

  // Picture width and height must be multiples of 8 and minCuSize
  const int minResolutionMultiple = std::max(8, 1 << m_log2MinCuSize);

  switch (m_conformanceWindowMode)
  {
  case 0:
    {
      // no conformance or padding
      m_confWinLeft = m_confWinRight = m_confWinTop = m_confWinBottom = 0;
      m_sourcePadding[1] = m_sourcePadding[0] = 0;
      break;
    }
  case 1:
    {
      // automatic padding to minimum CU size
      if (m_sourceWidth % minResolutionMultiple)
      {
        m_sourcePadding[0] = m_confWinRight  = ((m_sourceWidth / minResolutionMultiple) + 1) * minResolutionMultiple - m_sourceWidth;
        m_sourceWidth  += m_confWinRight;
      }
      if (m_sourceHeight % minResolutionMultiple)
      {
        m_sourcePadding[1] = m_confWinBottom = ((m_sourceHeight / minResolutionMultiple) + 1) * minResolutionMultiple - m_sourceHeight;
        m_sourceHeight += m_confWinBottom;
        if ( m_isField )
        {
          m_iSourceHeightOrg += m_confWinBottom << 1;
          m_sourcePadding[1] = m_confWinBottom << 1;
        }
      }
      if (m_sourcePadding[0] % SPS::getWinUnitX(m_chromaFormatIdc) != 0)
      {
        EXIT( "Error: picture width is not an integer multiple of the specified chroma subsampling");
      }
      if (m_sourcePadding[1] % SPS::getWinUnitY(m_chromaFormatIdc) != 0)
      {
        EXIT( "Error: picture height is not an integer multiple of the specified chroma subsampling");
      }
      if (m_sourcePadding[0])
      {
        msg( INFO, "Info: Conformance window automatically enabled. Adding %i lumal pel horizontally\n", m_sourcePadding[0]);
      }
      if (m_sourcePadding[1])
      {
        msg( INFO, "Info: Conformance window automatically enabled. Adding %i lumal pel vertically\n", m_sourcePadding[1]);
      }
      break;
    }
  case 2:
    {
      //padding
      m_sourceWidth  += m_sourcePadding[0];
      m_sourceHeight += m_sourcePadding[1];
      m_confWinRight  = m_sourcePadding[0];
      m_confWinBottom = m_sourcePadding[1];
      break;
    }
  case 3:
    {
      // conformance
      if ((m_confWinLeft == 0) && (m_confWinRight == 0) && (m_confWinTop == 0) && (m_confWinBottom == 0))
      {
        msg( ERROR, "Warning: Conformance window enabled, but all conformance window parameters set to zero\n");
      }
      if ((m_sourcePadding[1] != 0) || (m_sourcePadding[0]!=0))
      {
        msg( ERROR, "Warning: Conformance window enabled, padding parameters will be ignored\n");
      }
      m_sourcePadding[1] = m_sourcePadding[0] = 0;
      break;
    }
  }
  CHECK(((m_sourceWidth% minResolutionMultiple) || (m_sourceHeight % minResolutionMultiple)), "Picture width or height (after padding) is not a multiple of 8 or minCuSize, please use ConformanceWindowMode=1 for automatic adjustment or ConformanceWindowMode=2 to specify padding manually!!");

  if( m_conformanceWindowMode > 0 && m_subPicInfoPresentFlag )
  {
    for(int i = 0; i < m_numSubPics; i++)
    {
      CHECK((m_subPicCtuTopLeftX[i] * m_ctuSize)
              >= (m_sourceWidth - m_confWinRight * SPS::getWinUnitX(m_chromaFormatIdc)),
            "No subpicture can be located completely outside of the conformance cropping window");
      CHECK(((m_subPicCtuTopLeftX[i] + m_subPicWidth[i]) * m_ctuSize)
              <= (m_confWinLeft * SPS::getWinUnitX(m_chromaFormatIdc)),
            "No subpicture can be located completely outside of the conformance cropping window");
      CHECK((m_subPicCtuTopLeftY[i] * m_ctuSize)
              >= (m_sourceHeight - m_confWinBottom * SPS::getWinUnitY(m_chromaFormatIdc)),
            "No subpicture can be located completely outside of the conformance cropping window");
      CHECK(((m_subPicCtuTopLeftY[i] + m_subPicHeight[i]) * m_ctuSize)
              <= (m_confWinTop * SPS::getWinUnitY(m_chromaFormatIdc)),
            "No subpicture can be located completely outside of the conformance cropping window");
    }
  }

  if (tmpDecodedPictureHashSEIMappedType < 0 || tmpDecodedPictureHashSEIMappedType > to_underlying(HashType::NUM))
  {
    EXIT( "Error: bad checksum mode");
  }
  // Need to map values to match those of the SEI message:
  if (tmpDecodedPictureHashSEIMappedType==0)
  {
    m_decodedPictureHashSEIType = HashType::NONE;
  }
  else
  {
    m_decodedPictureHashSEIType = static_cast<HashType>(tmpDecodedPictureHashSEIMappedType - 1);
  }
  // Need to map values to match those of the SEI message:
  if (tmpSubpicDecodedPictureHashMappedType==0)
  {
    m_subpicDecodedPictureHashType = HashType::NONE;
  }
  else
  {
    m_subpicDecodedPictureHashType = static_cast<HashType>(tmpSubpicDecodedPictureHashMappedType - 1);
  }
  // allocate slice-based dQP values
  m_frameDeltaQps.resize(m_framesToBeEncoded + m_gopSize + 1);
  std::fill(m_frameDeltaQps.begin(), m_frameDeltaQps.end(), 0);

  if (m_qpIncrementAtSourceFrame.has_value())
  {
    uint32_t switchingPOC = 0;
    if (m_qpIncrementAtSourceFrame.value() > m_frameSkip)
    {
      // if switch source frame (ssf) = 10, and frame skip (fs)=2 and temporal subsample ratio (tsr) =1, then
      //    for this simulation switch at POC 8 (=10-2).
      // if ssf=10, fs=2, tsr=2, then for this simulation, switch at POC 4 (=(10-2)/2): POC0=Src2, POC1=Src4, POC2=Src6, POC3=Src8, POC4=Src10
      switchingPOC = (m_qpIncrementAtSourceFrame.value() - m_frameSkip) / m_temporalSubsampleRatio;
    }
    for (uint32_t i = switchingPOC; i < m_frameDeltaQps.size(); i++)
    {
      m_frameDeltaQps[i] = 1;
    }
  }

#if SHARP_LUMA_DELTA_QP
  CHECK( lumaLevelToDeltaQPMode >= LUMALVL_TO_DQP_NUM_MODES, "Error in cfg" );

  m_lumaLevelToDeltaQPMapping.mode=LumaLevelToDQPMode(lumaLevelToDeltaQPMode);

  if (m_lumaLevelToDeltaQPMapping.mode)
  {
    CHECK(  cfg_lumaLeveltoDQPMappingLuma.values.size() != cfg_lumaLeveltoDQPMappingQP.values.size(), "Error in cfg" );
    m_lumaLevelToDeltaQPMapping.mapping.resize(cfg_lumaLeveltoDQPMappingLuma.values.size());
    for(uint32_t i=0; i<cfg_lumaLeveltoDQPMappingLuma.values.size(); i++)
    {
      m_lumaLevelToDeltaQPMapping.mapping[i]=std::pair<int,int>(cfg_lumaLeveltoDQPMappingLuma.values[i], cfg_lumaLeveltoDQPMappingQP.values[i]);
    }
  }
#endif

  CHECK(cfg_qpInValCb.values.size() != cfg_qpOutValCb.values.size(), "Chroma QP table for Cb is incomplete.");
  CHECK(cfg_qpInValCr.values.size() != cfg_qpOutValCr.values.size(), "Chroma QP table for Cr is incomplete.");
  CHECK(cfg_qpInValCbCr.values.size() != cfg_qpOutValCbCr.values.size(), "Chroma QP table for CbCr is incomplete.");
  if (m_useIdentityTableForNon420Chroma && m_chromaFormatIdc != ChromaFormat::_420)
  {
    m_chromaQpMappingTableParams.m_sameCQPTableForAllChromaFlag = true;

    cfg_qpInValCb.values    = { QP26 };
    cfg_qpInValCr.values    = { QP26 };
    cfg_qpInValCbCr.values  = { QP26 };
    cfg_qpOutValCb.values   = { QP26 };
    cfg_qpOutValCr.values   = { QP26 };
    cfg_qpOutValCbCr.values = { QP26 };
  }

  // Need to have at least 2 points in the set. Add second one if only one given
  if (cfg_qpInValCb.values.size() == 1)
  {
    cfg_qpInValCb.values.push_back(cfg_qpInValCb.values[0] + 1);
    cfg_qpOutValCb.values.push_back(cfg_qpOutValCb.values[0] + 1);
  }
  if (cfg_qpInValCr.values.size() == 1)
  {
    cfg_qpInValCr.values.push_back(cfg_qpInValCr.values[0] + 1);
    cfg_qpOutValCr.values.push_back(cfg_qpOutValCr.values[0] + 1);
  }
  if (cfg_qpInValCbCr.values.size() == 1)
  {
    cfg_qpInValCbCr.values.push_back(cfg_qpInValCbCr.values[0] + 1);
    cfg_qpOutValCbCr.values.push_back(cfg_qpOutValCbCr.values[0] + 1);
  }

  int qpBdOffsetC = 6 * (m_internalBitDepth[ChannelType::CHROMA] - 8);
  m_chromaQpMappingTableParams.m_deltaQpInValMinus1[0].resize(cfg_qpInValCb.values.size());
  m_chromaQpMappingTableParams.m_deltaQpOutVal[0].resize(cfg_qpOutValCb.values.size());
  m_chromaQpMappingTableParams.m_numPtsInCQPTableMinus1[0] = (int) cfg_qpOutValCb.values.size() - 2;
  m_chromaQpMappingTableParams.m_qpTableStartMinus26[0]    = -QP26 + cfg_qpInValCb.values[0];
  CHECK(m_chromaQpMappingTableParams.m_qpTableStartMinus26[0] < -QP26 - qpBdOffsetC
          || m_chromaQpMappingTableParams.m_qpTableStartMinus26[0] > 36,
        "qpTableStartMinus26[0] is out of valid range of -26 -qpBdOffsetC to 36, inclusive.")
  CHECK(cfg_qpInValCb.values[0] != cfg_qpOutValCb.values[0], "First qpInValCb value should be equal to first qpOutValCb value");
  for (int i = 0; i < cfg_qpInValCb.values.size() - 1; i++)
  {
    CHECK(cfg_qpInValCb.values[i] < -qpBdOffsetC || cfg_qpInValCb.values[i] > MAX_QP, "Some entries cfg_qpInValCb are out of valid range of -qpBdOffsetC to 63, inclusive.");
    CHECK(cfg_qpOutValCb.values[i] < -qpBdOffsetC || cfg_qpOutValCb.values[i] > MAX_QP, "Some entries cfg_qpOutValCb are out of valid range of -qpBdOffsetC to 63, inclusive.");
    m_chromaQpMappingTableParams.m_deltaQpInValMinus1[0][i] = cfg_qpInValCb.values[i + 1] - cfg_qpInValCb.values[i] - 1;
    m_chromaQpMappingTableParams.m_deltaQpOutVal[0][i] = cfg_qpOutValCb.values[i + 1] - cfg_qpOutValCb.values[i];
  }
  if (!m_chromaQpMappingTableParams.m_sameCQPTableForAllChromaFlag)
  {
    m_chromaQpMappingTableParams.m_deltaQpInValMinus1[1].resize(cfg_qpInValCr.values.size());
    m_chromaQpMappingTableParams.m_deltaQpOutVal[1].resize(cfg_qpOutValCr.values.size());
    m_chromaQpMappingTableParams.m_numPtsInCQPTableMinus1[1] = (int) cfg_qpOutValCr.values.size() - 2;
    m_chromaQpMappingTableParams.m_qpTableStartMinus26[1]    = -QP26 + cfg_qpInValCr.values[0];
    CHECK(m_chromaQpMappingTableParams.m_qpTableStartMinus26[1] < -QP26 - qpBdOffsetC
            || m_chromaQpMappingTableParams.m_qpTableStartMinus26[1] > 36,
          "qpTableStartMinus26[1] is out of valid range of -26 -qpBdOffsetC to 36, inclusive.")
    CHECK(cfg_qpInValCr.values[0] != cfg_qpOutValCr.values[0], "First qpInValCr value should be equal to first qpOutValCr value");
    for (int i = 0; i < cfg_qpInValCr.values.size() - 1; i++)
    {
      CHECK(cfg_qpInValCr.values[i] < -qpBdOffsetC || cfg_qpInValCr.values[i] > MAX_QP, "Some entries cfg_qpInValCr are out of valid range of -qpBdOffsetC to 63, inclusive.");
      CHECK(cfg_qpOutValCr.values[i] < -qpBdOffsetC || cfg_qpOutValCr.values[i] > MAX_QP, "Some entries cfg_qpOutValCr are out of valid range of -qpBdOffsetC to 63, inclusive.");
      m_chromaQpMappingTableParams.m_deltaQpInValMinus1[1][i] = cfg_qpInValCr.values[i + 1] - cfg_qpInValCr.values[i] - 1;
      m_chromaQpMappingTableParams.m_deltaQpOutVal[1][i] = cfg_qpOutValCr.values[i + 1] - cfg_qpOutValCr.values[i];
    }
    m_chromaQpMappingTableParams.m_deltaQpInValMinus1[2].resize(cfg_qpInValCbCr.values.size());
    m_chromaQpMappingTableParams.m_deltaQpOutVal[2].resize(cfg_qpOutValCbCr.values.size());
    m_chromaQpMappingTableParams.m_numPtsInCQPTableMinus1[2] = (int) cfg_qpOutValCbCr.values.size() - 2;
    m_chromaQpMappingTableParams.m_qpTableStartMinus26[2]    = -QP26 + cfg_qpInValCbCr.values[0];
    CHECK(m_chromaQpMappingTableParams.m_qpTableStartMinus26[2] < -QP26 - qpBdOffsetC
            || m_chromaQpMappingTableParams.m_qpTableStartMinus26[2] > 36,
          "qpTableStartMinus26[2] is out of valid range of -26 -qpBdOffsetC to 36, inclusive.")
    CHECK(cfg_qpInValCbCr.values[0] != cfg_qpInValCbCr.values[0], "First qpInValCbCr value should be equal to first qpOutValCbCr value");
    for (int i = 0; i < cfg_qpInValCbCr.values.size() - 1; i++)
    {
      CHECK(cfg_qpInValCbCr.values[i] < -qpBdOffsetC || cfg_qpInValCbCr.values[i] > MAX_QP, "Some entries cfg_qpInValCbCr are out of valid range of -qpBdOffsetC to 63, inclusive.");
      CHECK(cfg_qpOutValCbCr.values[i] < -qpBdOffsetC || cfg_qpOutValCbCr.values[i] > MAX_QP, "Some entries cfg_qpOutValCbCr are out of valid range of -qpBdOffsetC to 63, inclusive.");
      m_chromaQpMappingTableParams.m_deltaQpInValMinus1[2][i] = cfg_qpInValCbCr.values[i + 1] - cfg_qpInValCbCr.values[i] - 1;
      m_chromaQpMappingTableParams.m_deltaQpOutVal[2][i] = cfg_qpInValCbCr.values[i + 1] - cfg_qpInValCbCr.values[i];
    }
  }

  /* Local chroma QP offsets configuration */
  CHECK(m_cuChromaQpOffsetSubdiv < 0, "MaxCuChromaQpOffsetSubdiv shall be >= 0");
  CHECK(cfg_crQpOffsetList.values.size() != cfg_cbQpOffsetList.values.size(), "Chroma QP offset lists shall be the same size");
  CHECK(cfg_cbCrQpOffsetList.values.size() != cfg_cbQpOffsetList.values.size() && cfg_cbCrQpOffsetList.values.size() > 0, "Chroma QP offset list for joint CbCr shall be either the same size as Cb and Cr or empty");
  if (m_cuChromaQpOffsetSubdiv > 0 && !cfg_cbQpOffsetList.values.size())
  {
    msg(WARNING, "MaxCuChromaQpOffsetSubdiv has no effect when chroma QP offset lists are empty\n");
  }
  m_cuChromaQpOffsetList.resize(cfg_cbQpOffsetList.values.size());
  for (int i=0; i < cfg_cbQpOffsetList.values.size(); i++)
  {
    m_cuChromaQpOffsetList[i].u.comp.cbOffset = cfg_cbQpOffsetList.values[i];
    m_cuChromaQpOffsetList[i].u.comp.crOffset = cfg_crQpOffsetList.values[i];
    m_cuChromaQpOffsetList[i].u.comp.jointCbCrOffset =
      cfg_cbCrQpOffsetList.values.size() ? cfg_cbCrQpOffsetList.values[i] : 0;
  }
  if (m_rprFunctionalityTestingEnabledFlag)
  {
    m_upscaledOutput = 2;
    if (m_scalingRatioHor == 1.0 && m_scalingRatioVer == 1.0)
    {
      m_scalingRatioHor = 2.0;
      m_scalingRatioVer = 2.0;
    }
    CHECK(cfg_rprSwitchingResolutionOrderList.values.size() > MAX_RPR_SWITCHING_ORDER_LIST_SIZE, "Length of RPRSwitchingResolutionOrderList exceeds maximum length");
    CHECK(cfg_rprSwitchingQPOffsetOrderList.values.size() > MAX_RPR_SWITCHING_ORDER_LIST_SIZE, "Length of RPRSwitchingQPOffsetOrderList exceeds maximum length");
    CHECK(cfg_rprSwitchingResolutionOrderList.values.size() != cfg_rprSwitchingQPOffsetOrderList.values.size(), "RPRSwitchingResolutionOrderList and RPRSwitchingQPOffsetOrderList shall be the same size");
    m_rprSwitchingListSize = (int)cfg_rprSwitchingResolutionOrderList.values.size();
    for (int k = 0; k < m_rprSwitchingListSize; k++)
    {
      m_rprSwitchingResolutionOrderList[k] = cfg_rprSwitchingResolutionOrderList.values[k];
      m_rprSwitchingQPOffsetOrderList[k] = cfg_rprSwitchingQPOffsetOrderList.values[k];
    }
    if (m_rprSwitchingTime != 0.0)
    {
      const int segmentSize     = 8 * int(m_frameRate.getFloatVal() * m_rprSwitchingTime / 8 + 0.5);
      m_rprSwitchingSegmentSize = segmentSize;
    }
  }
  if ( m_LadfEnabed )
  {
    CHECK(m_ladfNumIntervals != cfg_ladfQpOffset.values.size(),
          "size of LadfQpOffset must be equal to LadfNumIntervals");
    CHECK(m_ladfNumIntervals - 1 != cfg_ladfIntervalLowerBound.values.size(),
          "size of LadfIntervalLowerBound must be equal to LadfNumIntervals - 1");
    m_ladfQpOffset              = cfg_ladfQpOffset.values;
    m_ladfIntervalLowerBound[0] = 0;
    for (int k = 1; k < m_ladfNumIntervals; k++)
    {
      m_ladfIntervalLowerBound[k] = cfg_ladfIntervalLowerBound.values[k - 1];
    }
  }

  if (m_chromaFormatIdc != ChromaFormat::_420)
  {
    if (m_horCollocatedChromaFlag != 1)
    {
      if (m_horCollocatedChromaFlag == 0)
      {
        msg(WARNING, "WARNING: HorCollocatedChroma forced to 1 (chroma format is not 4:2:0)\n");
      }
      m_horCollocatedChromaFlag = 1;
    }
    if (m_verCollocatedChromaFlag != 1)
    {
      if (m_verCollocatedChromaFlag == 0)
      {
        msg(WARNING, "WARNING: VerCollocatedChroma is forced to 1 (chroma format is not 4:2:0)\n");
      }
      m_verCollocatedChromaFlag = 1;
    }
  }
  else
  {
    if (m_horCollocatedChromaFlag == -1)
    {
      if (m_chromaSampleLocType != Chroma420LocType::UNSPECIFIED)
      {
        m_horCollocatedChromaFlag = m_chromaSampleLocType == Chroma420LocType::LEFT
                                        || m_chromaSampleLocType == Chroma420LocType::TOP_LEFT
                                        || m_chromaSampleLocType == Chroma420LocType::BOTTOM_LEFT
                                      ? 1
                                      : 0;
      }
      else
      {
        m_horCollocatedChromaFlag = 1;
      }
    }

    if (m_verCollocatedChromaFlag == -1)
    {
      if (m_chromaSampleLocType != Chroma420LocType::UNSPECIFIED)
      {
        m_verCollocatedChromaFlag =
          m_chromaSampleLocType == Chroma420LocType::TOP_LEFT || m_chromaSampleLocType == Chroma420LocType::TOP ? 1 : 0;
      }
      else
      {
        m_verCollocatedChromaFlag = 0;
      }
    }
  }

  CHECK(m_verCollocatedChromaFlag != 0 && m_verCollocatedChromaFlag != 1, "m_verCollocatedChromaFlag should be 0 or 1");
  CHECK(m_horCollocatedChromaFlag != 0 && m_horCollocatedChromaFlag != 1, "m_horCollocatedChromaFlag should be 0 or 1");

#if JVET_O0756_CONFIG_HDRMETRICS && !JVET_O0756_CALCULATE_HDRMETRICS
  if ( m_calculateHdrMetrics == true)
  {
    printf ("Warning: Configuration enables HDR metric calculations.  However, HDR metric support was not linked when compiling the VTM.\n");
    m_calculateHdrMetrics = false;
  }
#endif

#if GDR_ENABLED
  if (m_gdrEnabled)
  {
    m_virtualBoundariesEnabledFlag = 1;
    m_virtualBoundariesPresentFlag = 0;
  }
  else
  {
    m_virtualBoundariesEnabledFlag = 0;
  }
#else
  m_virtualBoundariesEnabledFlag = 0;
#endif

  if( m_numVerVirtualBoundaries > 0 || m_numHorVirtualBoundaries > 0 )
    m_virtualBoundariesEnabledFlag = 1;

  if( m_virtualBoundariesEnabledFlag )
  {
    CHECK( m_subPicInfoPresentFlag && m_virtualBoundariesPresentFlag != 1, "When subpicture signalling is present, the signalling of virtual boundaries, if present, shall be in the SPS" );

    if( m_virtualBoundariesPresentFlag )
    {
      CHECK( m_numVerVirtualBoundaries > 3, "Number of vertical virtual boundaries must be comprised between 0 and 3 included" );
      CHECK( m_numHorVirtualBoundaries > 3, "Number of horizontal virtual boundaries must be comprised between 0 and 3 included" );
      CHECK( m_numVerVirtualBoundaries != cfg_virtualBoundariesPosX.values.size(), "Size of VirtualBoundariesPosX must be equal to NumVerVirtualBoundaries");
      CHECK( m_numHorVirtualBoundaries != cfg_virtualBoundariesPosY.values.size(), "Size of VirtualBoundariesPosY must be equal to NumHorVirtualBoundaries");
      m_virtualBoundariesPosX = cfg_virtualBoundariesPosX.values;
      if (m_numVerVirtualBoundaries > 1)
      {
        sort(m_virtualBoundariesPosX.begin(), m_virtualBoundariesPosX.end());
      }
      for (unsigned i = 0; i < m_numVerVirtualBoundaries; i++)
      {
        CHECK( m_virtualBoundariesPosX[i] == 0 || m_virtualBoundariesPosX[i] >= m_sourceWidth, "The vertical virtual boundary must be within the picture" );
        CHECK( m_virtualBoundariesPosX[i] % 8, "The vertical virtual boundary must be a multiple of 8 luma samples" );
        if (i > 0)
        {
          CHECK(
            m_virtualBoundariesPosX[i] - m_virtualBoundariesPosX[i - 1] < m_ctuSize,
            "The distance between any two vertical virtual boundaries shall be greater than or equal to the CTU size");
        }
      }
      m_virtualBoundariesPosY = cfg_virtualBoundariesPosY.values;
      if (m_numHorVirtualBoundaries > 1)
      {
        sort(m_virtualBoundariesPosY.begin(), m_virtualBoundariesPosY.end());
      }
      for (unsigned i = 0; i < m_numHorVirtualBoundaries; i++)
      {
        CHECK( m_virtualBoundariesPosY[i] == 0 || m_virtualBoundariesPosY[i] >= m_sourceHeight, "The horizontal virtual boundary must be within the picture" );
        CHECK( m_virtualBoundariesPosY[i] % 8, "The horizontal virtual boundary must be a multiple of 8 luma samples" );
        if (i > 0)
        {
          CHECK(m_virtualBoundariesPosY[i] - m_virtualBoundariesPosY[i - 1] < m_ctuSize,
                "The distance between any two horizontal virtual boundaries shall be greater than or equal to the CTU "
                "size");
        }
      }
    }
  }

  if ( m_alf )
  {
    CHECK(m_maxNumAlfAlternativesChroma < 1 || m_maxNumAlfAlternativesChroma > ALF_MAX_NUM_ALTERNATIVES_CHROMA,
          std::string("The maximum number of ALF Chroma filter alternatives must be in the range (1-")
            + std::to_string(ALF_MAX_NUM_ALTERNATIVES_CHROMA) + std::string(", inclusive)"));
  }

  // reading external dQP description from file
  if ( !m_dQPFileName.empty() )
  {
    FILE* fpt=fopen( m_dQPFileName.c_str(), "r" );
    if ( fpt )
    {
      int val;
      int poc = 0;
      m_frameDeltaQps.clear();
      while (poc < m_framesToBeEncoded)
      {
        if (fscanf(fpt, "%d", &val) == EOF)
        {
          break;
        }
        m_frameDeltaQps.push_back(val);
        poc++;
      }
      fclose(fpt);
    }
  }

  if( m_masteringDisplay.colourVolumeSEIEnabled )
  {
    for(uint32_t idx=0; idx<6; idx++)
    {
      m_masteringDisplay.primaries[idx/2][idx%2] = uint16_t((cfg_DisplayPrimariesCode.values.size() > idx) ? cfg_DisplayPrimariesCode.values[idx] : 0);
    }
    for(uint32_t idx=0; idx<2; idx++)
    {
      m_masteringDisplay.whitePoint[idx] = uint16_t((cfg_DisplayWhitePointCode.values.size() > idx) ? cfg_DisplayWhitePointCode.values[idx] : 0);
    }
  }
#if GREEN_METADATA_SEI_ENABLED
#if GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464
  // Set SEI green metadata parameters
  m_greenMetadataAMICancelFlag =
    ((m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::CANCEL) == GREEN_METADATA_AMI_FLAGS::CANCEL) ? true : false;
  m_greenMetadataAMIGlobalFlag =
    ((m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::GLOBAL) == GREEN_METADATA_AMI_FLAGS::GLOBAL) ? true : false;
  m_greenMetadataAMIApproxFlag =
    ((m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::APPROX) == GREEN_METADATA_AMI_FLAGS::APPROX) ? true : false;
  m_greenMetadataAMIPreprocFlag =
    ((m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::PREPROC) == GREEN_METADATA_AMI_FLAGS::PREPROC) ? true : false;
  m_greenMetadataAMIQualityFlag =
    ((m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::QUALITY) == GREEN_METADATA_AMI_FLAGS::QUALITY) ? true : false;
  m_greenMetadataAMIBacklightFlag =
    ((m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::BACKLIGHT) == GREEN_METADATA_AMI_FLAGS::BACKLIGHT) ? true
                                                                                                             : false;

  if (!m_greenMetadataAMICancelFlag)
  {
    if (m_greenMetadataAMIMapNumber > 0)
    {
      CHECK(cfg_greenMetadataAMILayerId.values.size() != m_greenMetadataAMIMapNumber,
            "Number of AMI layer ids must be equal to AMI map number.");
      CHECK(cfg_greenMetadataAMIOlsNumber.values.size() != m_greenMetadataAMIMapNumber,
            "Number of AMI ols must be equal to AMI map number.");
      CHECK(cfg_greenMetadataAMIEnergyReductionRate.values.size() != m_greenMetadataAMIMapNumber,
            "Number of AMI energy reduction rates must be equal to AMI map number.");
      if (m_greenMetadataAMIQualityFlag)
      {
        CHECK(cfg_greenMetadataAMIVideoQualityMetricType.values.size() != m_greenMetadataAMIMapNumber,
              "Number of AMI quality types must be equal to AMI map number.");
        CHECK(cfg_greenMetadataAMIVideoQualityLevel.values.size() != m_greenMetadataAMIMapNumber,
              "Number of AMI quality levels must be equal to AMI map number.");
      }
      CHECK(cfg_greenMetadataAMIMaxValue.values.size() != m_greenMetadataAMIMapNumber,
            "Number of AMI max values must be equal to AMI map number.");
      if (!m_greenMetadataAMIGlobalFlag)
      {
        CHECK(cfg_greenMetadataAMIAttenuationUseIdc.values.size() != m_greenMetadataAMIMapNumber,
              "Number of AMI attenuation uses must be equal to AMI map number.");
        CHECK(cfg_greenMetadataAMIAttenuationCompIdc.values.size() != m_greenMetadataAMIMapNumber,
              "Number of AMI attenuation comp indices must be equal to AMI map number.");
        if (m_greenMetadataAMIPreprocFlag)
        {
          CHECK(cfg_greenMetadataAMIPreprocessingFlag.values.size() != m_greenMetadataAMIMapNumber,
                "Number of AMI preprocessing flags must be equal to AMI map number.");
          CHECK(cfg_greenMetadataAMIPreprocessingTypeIdc.values.size() != m_greenMetadataAMIMapNumber,
                "Number of AMI preprocessing types must be equal to AMI map number.");
          CHECK(cfg_greenMetadataAMIPreprocessingScaleIdc.values.size() != m_greenMetadataAMIMapNumber,
                "Number of AMI preprocessing scales must be equal to AMI map number.");
        }
        if (m_greenMetadataAMIBacklightFlag)
          CHECK(cfg_greenMetadataAMIBacklightScalingIdc.values.size() != m_greenMetadataAMIMapNumber,
                "Number of AMI backlight scalings must be equal to AMI map number.");
      }
      else
      {
        CHECK(cfg_greenMetadataAMIAttenuationUseIdc.values.size() != 1,
              "Number of AMI attenuation uses must be equal to 1.");
        CHECK(cfg_greenMetadataAMIAttenuationCompIdc.values.size() != 1,
              "Number of AMI attenuation comp indices must be equal to 1.");
        if (m_greenMetadataAMIPreprocFlag)
        {
          CHECK(cfg_greenMetadataAMIPreprocessingFlag.values.size() != 1,
                "Number of AMI preprocessing flags must be equal to 1.");
          CHECK(cfg_greenMetadataAMIPreprocessingTypeIdc.values.size() != 1,
                "Number of AMI preprocessing types must be equal to 1.");
          CHECK(cfg_greenMetadataAMIPreprocessingScaleIdc.values.size() != 1,
                "Number of AMI preprocessing scales must be equal to 1.");
        }
        if (m_greenMetadataAMIBacklightFlag)
          CHECK(cfg_greenMetadataAMIBacklightScalingIdc.values.size() != 1,
                "Number of AMI backlight scalings must be equal to 1.");
      }
      int totalOlsIds = 0;
      for (int i = 0; i < m_greenMetadataAMIMapNumber; i++)
      {
        totalOlsIds = totalOlsIds + cfg_greenMetadataAMIOlsNumber.values[i];
      }
      CHECK(cfg_greenMetadataAMIOlsId.values.size() > totalOlsIds, "Number of AMI ols ids is higher than expected.");
      CHECK(cfg_greenMetadataAMIOlsId.values.size() < totalOlsIds, "Number of AMI ols ids is lower than expected.");

      int totalSize = m_greenMetadataAMIGlobalFlag ? 1 : m_greenMetadataAMIMapNumber;

      m_greenMetadataAMILayerId.resize(m_greenMetadataAMIMapNumber);
      m_greenMetadataAMIOlsNumber.resize(m_greenMetadataAMIMapNumber);
      m_greenMetadataAMIOlsId.resize(m_greenMetadataAMIMapNumber);
      m_greenMetadataAMIEnergyReductionRate.resize(m_greenMetadataAMIMapNumber);
      if (m_greenMetadataAMIQualityFlag)
      {
        m_greenMetadataAMIVideoQualityMetricType.resize(m_greenMetadataAMIMapNumber);
        m_greenMetadataAMIVideoQualityLevel.resize(m_greenMetadataAMIMapNumber);
      }
      m_greenMetadataAMIMaxValue.resize(m_greenMetadataAMIMapNumber);
      m_greenMetadataAMIAttenuationUseIdc.resize(totalSize);
      m_greenMetadataAMIAttenuationCompIdc.resize(totalSize);
      if (m_greenMetadataAMIPreprocFlag)
      {
        m_greenMetadataAMIPreprocessingFlag.resize(totalSize);
        m_greenMetadataAMIPreprocessingTypeIdc.resize(totalSize);
        m_greenMetadataAMIPreprocessingScaleIdc.resize(totalSize);
      }
      if (m_greenMetadataAMIBacklightFlag)
        m_greenMetadataAMIBacklightScalingIdc.resize(totalSize);

      int index = 0;
      for (int i = 0; i < m_greenMetadataAMIMapNumber; i++)
      {
        m_greenMetadataAMILayerId[i]   = uint8_t(cfg_greenMetadataAMILayerId.values[i]);
        m_greenMetadataAMIOlsNumber[i] = uint8_t(cfg_greenMetadataAMIOlsNumber.values[i]);
        m_greenMetadataAMIOlsId.push_back(std::vector<uint8_t>());

        m_greenMetadataAMIOlsId[i].resize(m_greenMetadataAMIOlsNumber[i]);
        for (int j = 0; j < m_greenMetadataAMIOlsNumber[i]; j++)
        {
          m_greenMetadataAMIOlsId[i][j] = uint8_t(cfg_greenMetadataAMIOlsId.values[index + j]);
        }
        index = index + m_greenMetadataAMIOlsNumber[i];

        m_greenMetadataAMIEnergyReductionRate[i] = uint8_t(cfg_greenMetadataAMIEnergyReductionRate.values[i]);
        if (m_greenMetadataAMIQualityFlag)
        {
          m_greenMetadataAMIVideoQualityMetricType[i] = uint8_t(cfg_greenMetadataAMIVideoQualityMetricType.values[i]);
          m_greenMetadataAMIVideoQualityLevel[i]      = uint16_t(cfg_greenMetadataAMIVideoQualityLevel.values[i] * 100);
        }
        m_greenMetadataAMIMaxValue[i] = uint8_t(cfg_greenMetadataAMIMaxValue.values[i]);
      }
      for (int i = 0; i < totalSize; i++)
      {
        m_greenMetadataAMIAttenuationUseIdc[i]  = uint8_t(cfg_greenMetadataAMIAttenuationUseIdc.values[i]);
        m_greenMetadataAMIAttenuationCompIdc[i] = uint8_t(cfg_greenMetadataAMIAttenuationCompIdc.values[i]);
        if (m_greenMetadataAMIPreprocFlag)
        {
          m_greenMetadataAMIPreprocessingFlag[i]     = uint8_t(cfg_greenMetadataAMIPreprocessingFlag.values[i]);
          m_greenMetadataAMIPreprocessingTypeIdc[i]  = uint8_t(cfg_greenMetadataAMIPreprocessingTypeIdc.values[i]);
          m_greenMetadataAMIPreprocessingScaleIdc[i] = uint8_t(cfg_greenMetadataAMIPreprocessingScaleIdc.values[i]);
        }
        if (m_greenMetadataAMIBacklightFlag)
          m_greenMetadataAMIBacklightScalingIdc[i] = uint8_t(cfg_greenMetadataAMIBacklightScalingIdc.values[i]);
      }
    }
    else
    {
      m_greenMetadataAMIEnergyReductionRate.resize(1);
      m_greenMetadataAMIEnergyReductionRate[1] = uint8_t(cfg_greenMetadataAMIEnergyReductionRate.values[1]);

      if (m_greenMetadataAMIQualityFlag)
      {
        m_greenMetadataAMIVideoQualityMetricType.resize(1);
        m_greenMetadataAMIVideoQualityLevel.resize(1);
        m_greenMetadataAMIVideoQualityMetricType[1] = uint8_t(cfg_greenMetadataAMIVideoQualityMetricType.values[1]);
        m_greenMetadataAMIVideoQualityLevel[1]      = uint16_t(cfg_greenMetadataAMIVideoQualityLevel.values[1] * 100);
      }
    }
  }

#endif
#endif

  // set sei film grain parameters.
  CHECK(!m_fgcSEIEnabled && m_fgcSEIAnalysisEnabled, "FGC SEI must be enabled in order to perform film grain analysis!");
  if (m_fgcSEIEnabled)
  {
    if (m_intraPeriod < 1)
    {   // low delay configuration
      msg(WARNING, "*************************************************************************\n");
      msg(WARNING, "* WARNING: For low delay configuration, FGC SEI is inserted for first frame only!*\n");
      msg(WARNING, "*************************************************************************\n");
      m_fgcSEIPerPictureSEI   = false;
      m_fgcSEIPersistenceFlag = true;
    }
    else if (m_intraPeriod == 1)
    {   // all intra configuration
      msg(WARNING, "*************************************************************************\n");
      msg(WARNING, "* WARNING: For Intra Period = 1, FGC SEI is inserted per frame!*\n");
      msg(WARNING, "*************************************************************************\n");
      m_fgcSEIPerPictureSEI   = true;
      m_fgcSEIPersistenceFlag = false;
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
    if (m_fgcSEIAnalysisEnabled && m_fgcSEITemporalFilterStrengths.empty())
    {
      // By default: in random-acces = filter RAPs, in all-intra = filter every frame, otherwise = filter every 2s
      int filteredFrame = m_intraPeriod < 1 ? 2 * m_frameRate.getIntValRound() : m_intraPeriod;
      m_fgcSEITemporalFilterStrengths[filteredFrame] = 1.5;
    }
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
          m_fgcSEICompModelValue[0][i][j] = uint32_t((cfg_FgcSEICompModelValueComp0.values.size() > numModelCtr) ? cfg_FgcSEICompModelValueComp0.values[numModelCtr] : 24);
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
          m_fgcSEICompModelValue[1][i][j] = uint32_t((cfg_FgcSEICompModelValueComp1.values.size() > numModelCtr) ? cfg_FgcSEICompModelValueComp1.values[numModelCtr] : 16);
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
          m_fgcSEICompModelValue[2][i][j] = uint32_t((cfg_FgcSEICompModelValueComp2.values.size() > numModelCtr) ? cfg_FgcSEICompModelValueComp2.values[numModelCtr] : 12);
          numModelCtr++;
        }
      }
    }
    m_fgcSEILog2ScaleFactor = m_fgcSEILog2ScaleFactor ? m_fgcSEILog2ScaleFactor : 2;
  }
  if (m_ctiSEIEnabled)
  {
    CHECK(!m_ctiSEICrossComponentFlag && m_ctiSEICrossComponentInferred, "CTI CrossComponentFlag is 0, but CTI CrossComponentInferred is 1 (must be 0 for CrossComponentFlag 0)");
    CHECK(!m_ctiSEICrossComponentFlag && !m_ctiSEICrossComponentInferred && !m_ctiSEINumberChromaLut, "For CTI CrossComponentFlag = 0, CTI NumberChromaLut needs to be specified (1 or 2) ");
    CHECK(m_ctiSEICrossComponentFlag && !m_ctiSEICrossComponentInferred && !m_ctiSEINumberChromaLut, "For CTI CrossComponentFlag = 1 and CrossComponentInferred = 0, CTI NumberChromaLut needs to be specified (1 or 2) ");

    CHECK(cfg_SEICTILut0.values.empty(), "SEI CTI (SEICTIEnabled) but no LUT0 specified");
    m_ctiSEILut[0].presentFlag = true;
    m_ctiSEILut[0].numLutValues = (int)cfg_SEICTILut0.values.size();
    m_ctiSEILut[0].lutValues = cfg_SEICTILut0.values;

    if (!m_ctiSEICrossComponentFlag || (m_ctiSEICrossComponentFlag && !m_ctiSEICrossComponentInferred))
    {
      CHECK(cfg_SEICTILut1.values.empty(), "SEI CTI LUT1 not specified");
      m_ctiSEILut[1].presentFlag = true;
      m_ctiSEILut[1].numLutValues = (int)cfg_SEICTILut1.values.size();
      m_ctiSEILut[1].lutValues = cfg_SEICTILut1.values;

      if (m_ctiSEINumberChromaLut == 1)
      { // Cb lut the same as Cr lut
        m_ctiSEILut[2].presentFlag = true;
        m_ctiSEILut[2].numLutValues = m_ctiSEILut[1].numLutValues;
        m_ctiSEILut[2].lutValues = m_ctiSEILut[1].lutValues;
      }
      else if (m_ctiSEINumberChromaLut == 2)
      { // read from cfg
        CHECK(cfg_SEICTILut2.values.empty(), "SEI CTI LUT2 not specified");
        m_ctiSEILut[2].presentFlag = true;
        m_ctiSEILut[2].numLutValues = (int)cfg_SEICTILut2.values.size();
        m_ctiSEILut[2].lutValues = cfg_SEICTILut2.values;
      }
      else
      {
        CHECK(m_ctiSEINumberChromaLut < 1 && m_ctiSEINumberChromaLut > 2, "Number of chroma LUTs is missing or out of range!");
      }
    }
    //  check if lut size is power of 2
    for (int idx = 0; idx < MAX_NUM_COMPONENT; idx++)
    {
      int n = m_ctiSEILut[idx].numLutValues - 1;
      CHECK(n > 0 && (n & (n - 1)) != 0, "Size of LUT minus 1 should be power of 2!");
      CHECK(n > MAX_CTI_LUT_SIZE, "LUT size minus 1 is larger than MAX_CTI_LUT_SIZE (64)!");
    }
  }
  if ( m_omniViewportSEIEnabled && !m_omniViewportSEICancelFlag )
  {
    CHECK (!( m_omniViewportSEICntMinus1 >= 0 && m_omniViewportSEICntMinus1 < 16 ), "SEIOmniViewportCntMinus1 must be in the range of 0 to 16");
    m_omniViewportSEIAzimuthCentre.resize  (m_omniViewportSEICntMinus1+1);
    m_omniViewportSEIElevationCentre.resize(m_omniViewportSEICntMinus1+1);
    m_omniViewportSEITiltCentre.resize     (m_omniViewportSEICntMinus1+1);
    m_omniViewportSEIHorRange.resize       (m_omniViewportSEICntMinus1+1);
    m_omniViewportSEIVerRange.resize       (m_omniViewportSEICntMinus1+1);
    for(int i=0; i<(m_omniViewportSEICntMinus1+1); i++)
    {
      m_omniViewportSEIAzimuthCentre[i]   = cfg_omniViewportSEIAzimuthCentre  .values.size() > i ? cfg_omniViewportSEIAzimuthCentre  .values[i] : 0;
      m_omniViewportSEIElevationCentre[i] = cfg_omniViewportSEIElevationCentre.values.size() > i ? cfg_omniViewportSEIElevationCentre.values[i] : 0;
      m_omniViewportSEITiltCentre[i]      = cfg_omniViewportSEITiltCentre     .values.size() > i ? cfg_omniViewportSEITiltCentre     .values[i] : 0;
      m_omniViewportSEIHorRange[i]        = cfg_omniViewportSEIHorRange       .values.size() > i ? cfg_omniViewportSEIHorRange       .values[i] : 0;
      m_omniViewportSEIVerRange[i]        = cfg_omniViewportSEIVerRange       .values.size() > i ? cfg_omniViewportSEIVerRange       .values[i] : 0;
    }
  }

  if(!m_rwpSEIRwpCancelFlag && m_rwpSEIEnabled)
  {
    CHECK (!( m_rwpSEINumPackedRegions > 0 && m_rwpSEINumPackedRegions <= std::numeric_limits<uint8_t>::max() ), "SEIRwpNumPackedRegions must be in the range of 1 to 255");
    CHECK (!(cfg_rwpSEIRwpTransformType.values.size() == m_rwpSEINumPackedRegions), "Number of must SEIRwpTransformType values be equal to SEIRwpNumPackedRegions");
    CHECK (!(cfg_rwpSEIRwpGuardBandFlag.values.size() == m_rwpSEINumPackedRegions), "Number of must SEIRwpGuardBandFlag values must be equal to SEIRwpNumPackedRegions");
    CHECK (!(cfg_rwpSEIProjRegionWidth.values.size() == m_rwpSEINumPackedRegions), "Number of must SEIProjRegionWidth values must be equal to SEIRwpNumPackedRegions");
    CHECK (!(cfg_rwpSEIProjRegionHeight.values.size() == m_rwpSEINumPackedRegions), "Number of must SEIProjRegionHeight values must be equal to SEIRwpNumPackedRegions");
    CHECK (!(cfg_rwpSEIRwpSEIProjRegionTop.values.size() == m_rwpSEINumPackedRegions), "Number of must SEIRwpSEIProjRegionTop values must be equal to SEIRwpNumPackedRegions");
    CHECK (!(cfg_rwpSEIProjRegionLeft.values.size() == m_rwpSEINumPackedRegions), "Number of must SEIProjRegionLeft values must be equal to SEIRwpNumPackedRegions");
    CHECK (!(cfg_rwpSEIPackedRegionWidth.values.size() == m_rwpSEINumPackedRegions), "Number of must SEIPackedRegionWidth values must be equal to SEIRwpNumPackedRegions");
    CHECK (!(cfg_rwpSEIPackedRegionHeight.values.size() == m_rwpSEINumPackedRegions), "Number of must SEIPackedRegionHeight values must be equal to SEIRwpNumPackedRegions");
    CHECK (!(cfg_rwpSEIPackedRegionTop.values.size() == m_rwpSEINumPackedRegions), "Number of must SEIPackedRegionTop values must be equal to SEIRwpNumPackedRegions");
    CHECK (!(cfg_rwpSEIPackedRegionLeft.values.size() == m_rwpSEINumPackedRegions), "Number of must SEIPackedRegionLeft values must be equal to SEIRwpNumPackedRegions");

    m_rwpSEIRwpTransformType.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIRwpGuardBandFlag.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIProjRegionWidth.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIProjRegionHeight.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIRwpSEIProjRegionTop.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIProjRegionLeft.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIPackedRegionWidth.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIPackedRegionHeight.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIPackedRegionTop.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIPackedRegionLeft.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIRwpLeftGuardBandWidth.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIRwpRightGuardBandWidth.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIRwpTopGuardBandHeight.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIRwpBottomGuardBandHeight.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIRwpGuardBandNotUsedForPredFlag.resize(m_rwpSEINumPackedRegions);
    m_rwpSEIRwpGuardBandType.resize(4*m_rwpSEINumPackedRegions);
    for( int i=0; i < m_rwpSEINumPackedRegions; i++ )
    {
      m_rwpSEIRwpTransformType[i]                     = cfg_rwpSEIRwpTransformType.values[i];
      CHECK (!( m_rwpSEIRwpTransformType[i] >= 0 && m_rwpSEIRwpTransformType[i] <= 7 ), "SEIRwpTransformType must be in the range of 0 to 7");
      m_rwpSEIRwpGuardBandFlag[i]                     = cfg_rwpSEIRwpGuardBandFlag.values[i];
      m_rwpSEIProjRegionWidth[i]                      = cfg_rwpSEIProjRegionWidth.values[i];
      m_rwpSEIProjRegionHeight[i]                     = cfg_rwpSEIProjRegionHeight.values[i];
      m_rwpSEIRwpSEIProjRegionTop[i]                  = cfg_rwpSEIRwpSEIProjRegionTop.values[i];
      m_rwpSEIProjRegionLeft[i]                       = cfg_rwpSEIProjRegionLeft.values[i];
      m_rwpSEIPackedRegionWidth[i]                    = cfg_rwpSEIPackedRegionWidth.values[i];
      m_rwpSEIPackedRegionHeight[i]                   = cfg_rwpSEIPackedRegionHeight.values[i];
      m_rwpSEIPackedRegionTop[i]                      = cfg_rwpSEIPackedRegionTop.values[i];
      m_rwpSEIPackedRegionLeft[i]                     = cfg_rwpSEIPackedRegionLeft.values[i];
      if( m_rwpSEIRwpGuardBandFlag[i] )
      {
        m_rwpSEIRwpLeftGuardBandWidth[i]              =  cfg_rwpSEIRwpLeftGuardBandWidth.values[i];
        m_rwpSEIRwpRightGuardBandWidth[i]             =  cfg_rwpSEIRwpRightGuardBandWidth.values[i];
        m_rwpSEIRwpTopGuardBandHeight[i]              =  cfg_rwpSEIRwpTopGuardBandHeight.values[i];
        m_rwpSEIRwpBottomGuardBandHeight[i]           =  cfg_rwpSEIRwpBottomGuardBandHeight.values[i];
        CHECK (! ( m_rwpSEIRwpLeftGuardBandWidth[i] > 0 || m_rwpSEIRwpRightGuardBandWidth[i] > 0 || m_rwpSEIRwpTopGuardBandHeight[i] >0 || m_rwpSEIRwpBottomGuardBandHeight[i] >0 ), "At least one of the RWP guard band parameters mut be greater than zero");
        m_rwpSEIRwpGuardBandNotUsedForPredFlag[i]     =  cfg_rwpSEIRwpGuardBandNotUsedForPredFlag.values[i];
        for( int j=0; j < 4; j++ )
        {
          m_rwpSEIRwpGuardBandType[i*4 + j]           =  cfg_rwpSEIRwpGuardBandType.values[i*4 + j];
        }

      }
    }
  }
  if (m_gcmpSEIEnabled && !m_gcmpSEICancelFlag)
  {
    int numFace = m_gcmpSEIPackingType == 4 || m_gcmpSEIPackingType == 5 ? 5 : 6;
    CHECK (!(cfg_gcmpSEIFaceIndex.values.size()                  == numFace), "Number of SEIGcmpFaceIndex must be equal to 5 when SEIGcmpPackingType is equal to 4 or 5, otherwise, it must be equal to 6");
    CHECK (!(cfg_gcmpSEIFaceRotation.values.size()               == numFace), "Number of SEIGcmpFaceRotation must be equal to 5 when SEIGcmpPackingType is equal to 4 or 5, otherwise, it must be equal to 6");
    m_gcmpSEIFaceIndex.resize(numFace);
    m_gcmpSEIFaceRotation.resize(numFace);
    if (m_gcmpSEIMappingFunctionType == 2)
    {
      CHECK (!(cfg_gcmpSEIFunctionCoeffU.values.size()           == numFace), "Number of SEIGcmpFunctionCoeffU must be equal to 5 when SEIGcmpPackingType is equal to 4 or 5, otherwise, it must be equal to 6");
      CHECK (!(cfg_gcmpSEIFunctionUAffectedByVFlag.values.size() == numFace), "Number of SEIGcmpFunctionUAffectedByVFlag must be equal to 5 when SEIGcmpPackingType is equal to 4 or 5, otherwise, it must be equal to 6");
      CHECK (!(cfg_gcmpSEIFunctionCoeffV.values.size()           == numFace), "Number of SEIGcmpFunctionCoeffV must be equal to 5 when SEIGcmpPackingType is equal to 4 or 5, otherwise, it must be equal to 6");
      CHECK (!(cfg_gcmpSEIFunctionVAffectedByUFlag.values.size() == numFace), "Number of SEIGcmpFunctionVAffectedByUFlag must be equal to 5 when SEIGcmpPackingType is equal to 4 or 5, otherwise, it must be equal to 6");
      m_gcmpSEIFunctionCoeffU.resize(numFace);
      m_gcmpSEIFunctionUAffectedByVFlag.resize(numFace);
      m_gcmpSEIFunctionCoeffV.resize(numFace);
      m_gcmpSEIFunctionVAffectedByUFlag.resize(numFace);
    }
    for (int i = 0; i < numFace; i++)
    {
      m_gcmpSEIFaceIndex[i]                = cfg_gcmpSEIFaceIndex.values[i];
      m_gcmpSEIFaceRotation[i]             = cfg_gcmpSEIFaceRotation.values[i];
      if (m_gcmpSEIMappingFunctionType == 2)
      {
        m_gcmpSEIFunctionCoeffU[i]           = cfg_gcmpSEIFunctionCoeffU.values[i];
        m_gcmpSEIFunctionUAffectedByVFlag[i] = cfg_gcmpSEIFunctionUAffectedByVFlag.values[i];
        m_gcmpSEIFunctionCoeffV[i]           = cfg_gcmpSEIFunctionCoeffV.values[i];
        m_gcmpSEIFunctionVAffectedByUFlag[i] = cfg_gcmpSEIFunctionVAffectedByUFlag.values[i];
      }
    }
  }
  if ( m_sdiSEIEnabled )
  {
    if (m_sdiSEIMultiviewInfoFlag || m_sdiSEIAuxiliaryInfoFlag)
    {
      m_sdiSEILayerId.resize(m_sdiSEIMaxLayersMinus1 + 1);
      m_sdiSEIViewIdVal.resize(m_sdiSEIMaxLayersMinus1 + 1);
      m_sdiSEIAuxId.resize(m_sdiSEIMaxLayersMinus1 + 1);
      m_sdiSEINumAssociatedPrimaryLayersMinus1.resize(m_sdiSEIMaxLayersMinus1 + 1);
      uint32_t associatedPrimaryLayerIdxListCnt = 0;
      for (int i = 0; i <= m_sdiSEIMaxLayersMinus1; i++)
      {
        m_sdiSEILayerId[i] = cfg_sdiSEILayerId.values[i];
        if (m_sdiSEIMultiviewInfoFlag)
        {
          m_sdiSEIViewIdVal[i] = cfg_sdiSEIViewIdVal.values[i];
        }
        if (m_sdiSEIAuxiliaryInfoFlag)
        {
          m_sdiSEIAuxId[i] = cfg_sdiSEIAuxId.values[i];
          if (m_sdiSEIAuxId[i] > 0)
          {
            m_sdiSEINumAssociatedPrimaryLayersMinus1[i] = cfg_sdiSEINumAssociatedPrimaryLayersMinus1.values[i];
            m_sdiSEIAssociatedPrimaryLayerIdx.resize(associatedPrimaryLayerIdxListCnt + m_sdiSEINumAssociatedPrimaryLayersMinus1[i] + 1);
            for (uint32_t k = 0; k <= m_sdiSEINumAssociatedPrimaryLayersMinus1[i]; k++)
            {
              m_sdiSEIAssociatedPrimaryLayerIdx[associatedPrimaryLayerIdxListCnt + k] = cfg_sdiSEIAssociatedPrimaryLayerIdx.values[associatedPrimaryLayerIdxListCnt + k];
            }
            associatedPrimaryLayerIdxListCnt += (m_sdiSEINumAssociatedPrimaryLayersMinus1[i] + 1);
          }
        }
      }
    }
  }
  if ( m_maiSEIEnabled )
  {
    if (m_maiSEIIntrinsicParamFlag)
    {
      int numViews = m_maiSEIIntrinsicParamsEqualFlag ? 1 : m_maiSEINumViewsMinus1 + 1;
      m_maiSEISignFocalLengthX       .resize( numViews );
      m_maiSEIExponentFocalLengthX   .resize( numViews );
      m_maiSEIMantissaFocalLengthX   .resize( numViews );
      m_maiSEISignFocalLengthY       .resize( numViews );
      m_maiSEIExponentFocalLengthY   .resize( numViews );
      m_maiSEIMantissaFocalLengthY   .resize( numViews );
      m_maiSEISignPrincipalPointX    .resize( numViews );
      m_maiSEIExponentPrincipalPointX.resize( numViews );
      m_maiSEIMantissaPrincipalPointX.resize( numViews );
      m_maiSEISignPrincipalPointY    .resize( numViews );
      m_maiSEIExponentPrincipalPointY.resize( numViews );
      m_maiSEIMantissaPrincipalPointY.resize( numViews );
      m_maiSEISignSkewFactor         .resize( numViews );
      m_maiSEIExponentSkewFactor     .resize( numViews );
      m_maiSEIMantissaSkewFactor     .resize( numViews );
      for( int i = 0; i  <=  ( m_maiSEIIntrinsicParamsEqualFlag ? 0 : m_maiSEINumViewsMinus1 ); i++ )
      {
        m_maiSEISignFocalLengthX       [i] = cfg_maiSEISignFocalLengthX.values[i];
        m_maiSEIExponentFocalLengthX   [i] = cfg_maiSEIExponentFocalLengthX.values[i];
        m_maiSEIMantissaFocalLengthX   [i] = cfg_maiSEIMantissaFocalLengthX.values[i];
        m_maiSEISignFocalLengthY       [i] = cfg_maiSEISignFocalLengthY.values[i];
        m_maiSEIExponentFocalLengthY   [i] = cfg_maiSEIExponentFocalLengthY.values[i];
        m_maiSEIMantissaFocalLengthY   [i] = cfg_maiSEIMantissaFocalLengthY.values[i];
        m_maiSEISignPrincipalPointX    [i] = cfg_maiSEISignPrincipalPointX.values[i];
        m_maiSEIExponentPrincipalPointX[i] = cfg_maiSEIExponentPrincipalPointX.values[i];
        m_maiSEIMantissaPrincipalPointX[i] = cfg_maiSEIMantissaPrincipalPointX.values[i];
        m_maiSEISignPrincipalPointY    [i] = cfg_maiSEISignPrincipalPointY.values[i];
        m_maiSEIExponentPrincipalPointY[i] = cfg_maiSEIExponentPrincipalPointY.values[i];
        m_maiSEIMantissaPrincipalPointY[i] = cfg_maiSEIMantissaPrincipalPointY.values[i];
        m_maiSEISignSkewFactor         [i] = cfg_maiSEISignSkewFactor.values[i];
        m_maiSEIExponentSkewFactor     [i] = cfg_maiSEIExponentSkewFactor.values[i];
        m_maiSEIMantissaSkewFactor     [i] = cfg_maiSEIMantissaSkewFactor.values[i];
      }
    }
  }
  if (m_mvpSEIEnabled)
  {
    int numViews = m_mvpSEINumViewsMinus1 + 1;
    m_mvpSEIViewPosition.resize(numViews);
    for (int i = 0; i <= m_mvpSEINumViewsMinus1; i++)
    {
      m_mvpSEIViewPosition[i] = cfg_mvpSEIViewPosition.values[i];
    }
  }
  if ( m_driSEIEnabled )
  {
    m_driSEINonlinearModel.resize(m_driSEINonlinearNumMinus1+1);
    for(int i=0; i<(m_driSEINonlinearNumMinus1+1); i++)
    {
      m_driSEINonlinearModel[i]   = cfg_driSEINonlinearModel.values.size() > i ? cfg_driSEINonlinearModel.values[i] : 0;
    }
  }
  m_reshapeCW.binCW.resize(3);
  m_reshapeCW.rspFps     = m_frameRate.getIntValRound();
  m_reshapeCW.rspPicSize = m_sourceWidth*m_sourceHeight;
  m_reshapeCW.rspFpsToIp = std::max(16, 16 * (int) (round(m_frameRate.getFloatVal() / 16.0)));
  m_reshapeCW.rspBaseQP = m_iQP;
  m_reshapeCW.updateCtrl = m_updateCtrl;
  m_reshapeCW.adpOption = m_adpOption;
  m_reshapeCW.initialCW = m_initialCW;
#if ENABLE_TRACING
  g_trace_ctx = tracing_init(sTracingFile, sTracingRule);
  if( bTracingChannelsList && g_trace_ctx )
  {
    std::string sChannelsList;
    g_trace_ctx->getChannelsList( sChannelsList );
    msg( INFO, "\n Using tracing channels:\n\n%s\n", sChannelsList.c_str() );
  }
#endif

#if ENABLE_QPA
  if (m_bUsePerceptQPA && !m_bUseAdaptiveQP && m_dualTree && (m_cbQpOffsetDualTree != 0 || m_crQpOffsetDualTree != 0 || m_cbCrQpOffsetDualTree != 0))
  {
    msg( WARNING, "*************************************************************************\n" );
    msg( WARNING, "* WARNING: chroma QPA on, ignoring nonzero dual-tree chroma QP offsets! *\n" );
    msg( WARNING, "*************************************************************************\n" );
  }

#if ENABLE_QPA_SUB_CTU
  if ((m_iQP < 38) && m_bUsePerceptQPA && !m_bUseAdaptiveQP && (m_sourceWidth <= 2048) && (m_sourceHeight <= 1280)
#if WCG_EXT && ER_CHROMA_QP_WCG_PPS
      && (!m_wcgChromaQpControl.enabled)
#endif
      && ((1 << (m_log2MaxTbSize + 1)) == m_ctuSize) && (m_sourceWidth > 512 || m_sourceHeight > 320))
  {
    m_cuQpDeltaSubdiv = 2;
  }
#else
  if ((m_iQP < 38) && (m_gopSize > 4) && m_bUsePerceptQPA && !m_bUseAdaptiveQP && (m_sourceHeight <= 1280)
      && (m_sourceWidth <= 2048))
  {
    msg( WARNING, "*************************************************************************\n" );
    msg( WARNING, "* WARNING: QPA on with large CTU for <=HD sequences, limiting CTU size! *\n" );
    msg( WARNING, "*************************************************************************\n" );

    m_ctuSize = m_maxCuWidth;
    if ((1u << m_log2MaxTbSize) > m_ctuSize)
    {
      m_log2MaxTbSize--;
    }
  }
#endif
#endif // ENABLE_QPA


  m_ShutterFilterEnable = false;
  if (m_siiSEIEnabled)
  {
    assert(m_siiSEITimeScale >= 0 && m_siiSEITimeScale <= MAX_UINT);
    uint32_t sii_max_sub_layers = (uint32_t)cfg_siiSEIInputNumUnitsInSI.values.size();
    assert(sii_max_sub_layers > 0);
    if (sii_max_sub_layers > 1)
    {
      m_siiSEISubLayerNumUnitsInSI.resize(sii_max_sub_layers);
      for (int32_t i = 0; i < sii_max_sub_layers; i++)
      {
        m_siiSEISubLayerNumUnitsInSI[i] = cfg_siiSEIInputNumUnitsInSI.values[i];
        assert(m_siiSEISubLayerNumUnitsInSI[i] >= 0 && m_siiSEISubLayerNumUnitsInSI[i] <= MAX_UINT);
      }
    }
    else
    {
      m_siiSEINumUnitsInShutterInterval = cfg_siiSEIInputNumUnitsInSI.values[0];
      assert(m_siiSEINumUnitsInShutterInterval >= 0 && m_siiSEINumUnitsInShutterInterval <= MAX_UINT);
    }
    uint32_t siiMaxSubLayersMinus1 = sii_max_sub_layers - 1;
    int blending_ratio = (m_siiSEISubLayerNumUnitsInSI[0] / m_siiSEISubLayerNumUnitsInSI[siiMaxSubLayersMinus1]);

    if (sii_max_sub_layers > 1 && m_siiSEISubLayerNumUnitsInSI[0] ==
                                (blending_ratio * m_siiSEISubLayerNumUnitsInSI[siiMaxSubLayersMinus1]))
    {
      m_ShutterFilterEnable = true;
      double  fpsHFR        = m_frameRate.getFloatVal();
      int32_t i;
      bool    checkEqualValuesOfSFR = true;
      bool    checkSubLayerSI       = false;

      double shutterAngleFactor = (fpsHFR * ((double)(m_siiSEISubLayerNumUnitsInSI[siiMaxSubLayersMinus1])))/((double)m_siiSEITimeScale);

      // If shutterAngleFactor = 1 indicates that shutterAngle = 360
      // If shutterAngleFactor = 0.5 indicates that shutterAngle = 180
      // If shutterAngleFactor = 0.25 indicates that shutterAngle = 90

      if (shutterAngleFactor < 0.5)
      {
        for (int i = 0; i < siiMaxSubLayersMinus1; i++)
        {
          m_siiSEISubLayerNumUnitsInSI[i] = m_siiSEISubLayerNumUnitsInSI[siiMaxSubLayersMinus1];
        }
        m_ShutterFilterEnable = false;
        printf("Warning: For the shutterAngle = %d, the blending can't be applied\n", (int)(shutterAngleFactor * 360));
      }
      // supports only the case of SFR = HFR / 2
      if (m_siiSEISubLayerNumUnitsInSI[siiMaxSubLayersMinus1] < m_siiSEISubLayerNumUnitsInSI[siiMaxSubLayersMinus1 - 1])
      {
        checkSubLayerSI = true;
      }
      // check shutter interval for all sublayer remains same for LFR pictures
      for (i = 1; i < siiMaxSubLayersMinus1; i++)
      {
        if (m_siiSEISubLayerNumUnitsInSI[0] != m_siiSEISubLayerNumUnitsInSI[i])
        {
          checkEqualValuesOfSFR = false;
        }
      }
      if (checkSubLayerSI && checkEqualValuesOfSFR)
      {
        setBlendingRatioSII(blending_ratio);
      }
      else
      {
        m_ShutterFilterEnable = false;
      }
    }
    else
    {
      printf("Warning: SII-processing is applied for multiple shutter intervals and number of LFR units should be 2 times of number of HFR units\n");
    }
  }


  if (m_poSEIEnabled)
  {
    CHECK(cfg_poSEIPayloadType.values.size() <= 1, "there should be at least 2 SEIPOPayLoadType");
    CHECK(cfg_poSEIProcessingOrder.values.size() != cfg_poSEIPayloadType.values.size(), "the number of SEIPOPayLoadType should be equal to the number of SEIPOProcessingOrder");
    CHECK(cfg_poSEIPrefixFlag.values.size() <= 1, "there should be at least 2 SEIPOPrefixFlag");
    CHECK(cfg_poSEIPayloadType.values.size() != m_poSEINumMinus2 + 2, "the number of SEIPOPayLoadType should be equal to the number of SEI messages");
    CHECK(cfg_poSEIWrappingFlag.values.size() != m_poSEINumMinus2 + 2, "the number of SEIPOWrappingFlag should be equal to the number of SEI messages");
    CHECK(cfg_poSEIImportanceIdc.values.size() != m_poSEINumMinus2 + 2, "the number of SEIImportanceIdc should be equal to the number of SEI messages");
    m_poSEIWrappingFlag.resize((uint32_t)cfg_poSEIPayloadType.values.size());
    m_poSEIImportanceFlag.resize((uint32_t)cfg_poSEIPayloadType.values.size());
    m_poSEIProcessingDegreeFlag.resize((uint32_t)cfg_poSEIPayloadType.values.size());
    m_poSEIPrefixFlag.resize((uint32_t)cfg_poSEIPayloadType.values.size());
    m_poSEIPayloadType.resize((uint32_t) cfg_poSEIPayloadType.values.size());
    m_poSEIProcessingOrder.resize((uint32_t) cfg_poSEIPayloadType.values.size());
    m_poSEINumOfPrefixBits.resize((uint32_t) cfg_poSEINumofPrefixBits.values.size());
    m_poSEIPrefixByte.resize((uint32_t) cfg_poSEIPayloadType.values.size());
    uint16_t prefixByteIdx = 0;
    bool NNPFCFound = false;
    bool NNPFAFound = false;
    for (uint32_t i = 0; i < (m_poSEINumMinus2 + 2); i++)
    {
      m_poSEIPrefixFlag[i] =      cfg_poSEIPrefixFlag.values[i];
      m_poSEIWrappingFlag[i] = cfg_poSEIWrappingFlag.values[i];
      m_poSEIImportanceFlag[i] = (cfg_poSEIImportanceIdc.values[i] & 2) != 0;
      m_poSEIProcessingDegreeFlag[i] = (cfg_poSEIImportanceIdc.values[i] & 1) != 0;
      m_poSEIPayloadType[i]     = cfg_poSEIPayloadType.values[i];
      if (m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::MASTERING_DISPLAY_COLOUR_VOLUME ||
          m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::CONTENT_LIGHT_LEVEL_INFO ||
          m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::ALTERNATIVE_TRANSFER_CHARACTERISTICS ||
          m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::AMBIENT_VIEWING_ENVIRONMENT ||
          m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::MULTIVIEW_ACQUISITION_INFO ||
          m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::MULTIVIEW_VIEW_POSITION ||
          m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::SEI_MANIFEST ||
          m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::SEI_PREFIX_INDICATION ||
          m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::VDI_SEI_ENVELOPE ||
          m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::SEI_PROCESSING_ORDER
        )
      {
        CHECK(m_poSEIPrefixFlag[i] == true, "The value of po_sei_prefix_flag shall be equal to 0 when po_sei_payload_type is equal to 137, 144, 147, 148, 179, 180, 200, 201, 208, and 213");
      }
      NNPFCFound = NNPFCFound || (m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_CHARACTERISTICS);
      NNPFAFound = NNPFAFound || (m_poSEIPayloadType[i] == (uint16_t)SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_ACTIVATION);
      CHECK(!NNPFCFound && NNPFAFound, "NNPFA payload type found before NNPFC payload type in SPO SEI");
      m_poSEIProcessingOrder[i] = (uint16_t) cfg_poSEIProcessingOrder.values[i];
      if (m_poSEIPrefixFlag[i])
      {
        m_poSEINumOfPrefixBits[i] = cfg_poSEINumofPrefixBits.values[i];
        m_poSEIPrefixByte[i].resize((cfg_poSEINumofPrefixBits.values[i] + 7) >> 3);
        for (uint32_t j = 0; j < (uint32_t)m_poSEIPrefixByte[i].size(); j++)
        {
          m_poSEIPrefixByte[i][j] = (uint8_t) cfg_poSEIPrefixByte.values[prefixByteIdx++];
        }
      }
      else
      {
        cfg_poSEINumofPrefixBits.values[i] = 0;
        m_poSEINumOfPrefixBits[i] = 0;
      }
      // Error check, to avoid same PayloadType and same prefix bytes when present with different PayloadOrder
      for (uint32_t j = 0; j < i; j++)
      {
        if (m_poSEIPrefixFlag[i])
        {
            if ((m_poSEIPayloadType[j] == m_poSEIPayloadType[i]) && m_poSEIPrefixFlag[j])
            {
              auto numofPrefixBytes = std::min((cfg_poSEINumofPrefixBits.values[i] + 7) >> 3, (cfg_poSEINumofPrefixBits.values[j] + 7) >> 3);
              if (std::equal(m_poSEIPrefixByte[i].begin() + 1, m_poSEIPrefixByte[i].begin() + numofPrefixBytes - 1,
                             m_poSEIPrefixByte[j].begin()))
              {
                CHECK(m_poSEIProcessingOrder[j] != m_poSEIProcessingOrder[i], "multiple SEI messages with the same po_sei_payload_type and prefix content present when present shall have the same value of po_sei_processing_order");
              }
            }
        }
        else
        {
          if (m_poSEIPayloadType[j] == m_poSEIPayloadType[i])
          {
            CHECK(m_poSEIProcessingOrder[j] != m_poSEIProcessingOrder[i], "multiple SEI messages with the same po_sei_payload_type without prefix content shall have the same value of po_sei_processing_order");
          }
        }
      }
    }
    CHECK(NNPFCFound && !NNPFAFound, "When SPO SEI contains NNPFC payload type it shall also contain NNPFA payload type");
#if JVET_AL0075_NNPFA_SELECTED_INPUT_FLAG
    if (!NNPFAFound && m_nnPostFilterSEIActivationEnabled && m_nnPostFilterSEIActivationSelectedInputFlag)
    {
      m_nnPostFilterSEIActivationSelectedInputFlag = false;
      msg(WARNING, "\nWarning: Resetting SEINNPostFilterActivationSelectedInputFlag to 0 (it shall be zero when NNPFA SEI is not present in PON SEI)\n");
    }
#endif
    // The following code generares sub-chain indices for conformance checking.
    uint32_t numProcStgs = m_poSEINumMinus2 + 2;
    std::vector<uint32_t> seiTypeIdx;
    for (uint32_t j = 0; j < numProcStgs; j++)
    {
      seiTypeIdx.push_back(j);
    }
    uint32_t subChainFlag = 0;
    uint32_t subChainPrevIdx = 0;
    std::vector<uint32_t> subChainIdx(numProcStgs);
    for (uint32_t j = 0; j < numProcStgs; j++)
    {
      uint32_t idx = seiTypeIdx[j];
      if (m_poSEIImportanceFlag[idx] && m_poSEIProcessingDegreeFlag[idx])
      {
        if (subChainFlag == 0)
        {
          subChainIdx[j] = 0;
        }
        else
        {
          subChainIdx[j] = subChainPrevIdx;
        }
      }
      else if (!m_poSEIImportanceFlag[idx] && m_poSEIProcessingDegreeFlag[idx])
      {
        subChainIdx[j] = subChainFlag * subChainPrevIdx;
        CHECK(subChainIdx[j] == 0, "When pos_sei_importance_flag[idx] is equal to 0 and po_sei_processing_degree_flag[idx] is equal to 1, poSubChainIdx[j] shall be greater than 0")
        subChainFlag = 0;
      }
      else if (m_poSEIImportanceFlag[idx] && !m_poSEIProcessingDegreeFlag[idx])
      {
        subChainPrevIdx++;
        subChainIdx[j] = subChainPrevIdx;
        subChainFlag = 1;
      }
      else
      {
        subChainIdx[j] = subChainFlag * subChainPrevIdx;
      }
    }
  }
#if JVET_AL0075_NNPFA_SELECTED_INPUT_FLAG
  else if (m_nnPostFilterSEIActivationEnabled && m_nnPostFilterSEIActivationSelectedInputFlag)
  {
    m_nnPostFilterSEIActivationSelectedInputFlag = false;
    msg(WARNING, "\nWarning: Resetting SEINNPostFilterActivationSelectedInputFlag to 0 (it shall be zero when NNPFA SEI is not present in PON SEI)\n");
  }
#endif

  if (m_postFilterHintSEIEnabled)
  {
    CHECK(cfg_postFilterHintSEIValues.values.size() <= 0, "The number of filter coefficient shall be greater than zero");
    CHECK(!(cfg_postFilterHintSEIValues.values.size() == ((m_postFilterHintSEIChromaCoeffPresentFlag ? 3 : 1) * m_postFilterHintSEISizeY * m_postFilterHintSEISizeX)), "The number of filter coefficient shall match the matrix size and considering whether filters for chroma is present of not");
    m_postFilterHintValues.resize(cfg_postFilterHintSEIValues.values.size());

    for (uint32_t i = 0; i < m_postFilterHintValues.size(); i++)
    {
      m_postFilterHintValues[i] = cfg_postFilterHintSEIValues.values[i];
    }
  }
  if (m_generativeFaceVideoEnabled)
  {
    CHECK(cfg_generativeFaceVideoSEIId.values.size() != m_generativeFaceVideoSEINumber, "Number of GFV ID must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoSEICnt.values.size() != m_generativeFaceVideoSEINumber, "Number of GFV Cnt must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoSEIDrivePicFusionFlag.values.size() != m_generativeFaceVideoSEINumber, "Number of GFV DrivePicFusionFlag must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoSEILowConfidenceFaceParameterFlag.values.size() != m_generativeFaceVideoSEINumber, "Number of GFV LowConfidenceFaceParameterFlag must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoSEICoordinatePresentFlag.values.size() != m_generativeFaceVideoSEINumber, "Number of Coordinate PresentFlag must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoSEICoordinatePredFlag.values.size() != m_generativeFaceVideoSEINumber, "Number of CoordinatePredFlag must be equal to SEINumber");
    m_generativeFaceVideoSEIId.resize(m_generativeFaceVideoSEINumber);
    m_generativeFaceVideoSEICnt.resize(m_generativeFaceVideoSEINumber);
    m_generativeFaceVideoSEIDrivePicFusionFlag.resize(m_generativeFaceVideoSEINumber);
    m_generativeFaceVideoSEILowConfidenceFaceParameterFlag.resize(m_generativeFaceVideoSEINumber);
    m_generativeFaceVideoSEICoordinatePresentFlag.resize(m_generativeFaceVideoSEINumber);
    m_generativeFaceVideoSEICoordinateQuantizationFactor.resize(m_generativeFaceVideoSEINumber);
    m_generativeFaceVideoSEICoordinatePredFlag.resize(m_generativeFaceVideoSEINumber);
    m_generativeFaceVideoSEI3DCoordinateFlag.resize(m_generativeFaceVideoSEINumber);
    m_generativeFaceVideoSEICoordinatePointNum.resize(m_generativeFaceVideoSEINumber);
    CHECK(cfg_generativeFaceVideoSEIMatrixPresentFlag.values.size() != m_generativeFaceVideoSEINumber, "Number of Matrix PresentFlag must be equal to SEINumber");
    m_generativeFaceVideoSEIMatrixPresentFlag.resize(m_generativeFaceVideoSEINumber);
    m_generativeFaceVideoSEIMatrixPredFlag.resize(m_generativeFaceVideoSEINumber);
    m_generativeFaceVideoSEIMatrixElementPrecisionFactor.resize(m_generativeFaceVideoSEINumber);
    m_generativeFaceVideoSEINumMatrixType.resize(m_generativeFaceVideoSEINumber);
    int cooridinateBeforeSum = 0;
    int cooridinateZBeforeSum = 0;
    int matrixNumBeforeSum = 0;
    int matrixDimensionBeforeSum = 0;
    m_generativeFaceVideoSEIChromaKeyValuePresentFlag.resize(3);
    m_generativeFaceVideoSEIChromaKeyValue.resize(3);
    m_generativeFaceVideoSEIChromaKeyThrPresentFlag.resize(2);
    m_generativeFaceVideoSEIChromaKeyThrValue.resize(2);
    if(m_generativeFaceVideoSEIChromaKeyInfoPresentFlag)
    {
      for (uint32_t chromac = 0; chromac < 3; chromac++)
      {
        m_generativeFaceVideoSEIChromaKeyValuePresentFlag[chromac] = cfg_generativeFaceVideoSEIChromaKeyValuePresentFlag.values[chromac];
        m_generativeFaceVideoSEIChromaKeyValue[chromac] = cfg_generativeFaceVideoSEIChromaKeyValue.values[chromac];
      }
      for (uint32_t chromai = 0; chromai < 2; chromai++)
      {
        m_generativeFaceVideoSEIChromaKeyThrPresentFlag[chromai] = cfg_generativeFaceVideoSEIChromaKeyThrPresentFlag.values[chromai];
        m_generativeFaceVideoSEIChromaKeyThrValue[chromai] = cfg_generativeFaceVideoSEIChromaKeyThrValue.values[chromai];
      }
    }
    for (uint32_t frameIdx = 0; frameIdx < m_generativeFaceVideoSEINumber; frameIdx++) 
    {
      m_generativeFaceVideoSEIId[frameIdx] = cfg_generativeFaceVideoSEIId.values[frameIdx];
      m_generativeFaceVideoSEICnt[frameIdx] = cfg_generativeFaceVideoSEICnt.values[frameIdx];
      m_generativeFaceVideoSEIDrivePicFusionFlag[frameIdx] = cfg_generativeFaceVideoSEIDrivePicFusionFlag.values[frameIdx];
      m_generativeFaceVideoSEILowConfidenceFaceParameterFlag[frameIdx] = cfg_generativeFaceVideoSEILowConfidenceFaceParameterFlag.values[frameIdx];
      m_generativeFaceVideoSEICoordinatePresentFlag[frameIdx] = cfg_generativeFaceVideoSEICoordinatePresentFlag.values[frameIdx];
      CHECK(m_generativeFaceVideoSEICnt[frameIdx] && m_generativeFaceVideoSEIBasePicFlag && m_generativeFaceVideoSEIDrivePicFusionFlag[frameIdx],
            "When a GFV SEI message gfvSeiA with a particular gfv_id value has gfv_cnt greater than 0 and a GFV SEI message gfvSeiB with the same gfv_id value "
            "in the same picture unit has gfv_base_pic_flag equal to 1 (i.e., the current decoded picture is a base picture), the GFV SEI message gfvSeiA shall have gfv_drive_pic_fusion_flag equal to 0");
      m_generativeFaceVideoSEICoordinatePredFlag[frameIdx] = cfg_generativeFaceVideoSEICoordinatePredFlag.values[frameIdx];
      m_generativeFaceVideoSEIZCoordinateMaxValue.push_back(std::vector<uint32_t>());
      int basePicFlag = (frameIdx == 0) ? 1 : 0;
      if (basePicFlag || !m_generativeFaceVideoSEICoordinatePredFlag[frameIdx])
      {
        m_generativeFaceVideoSEICoordinateQuantizationFactor[frameIdx] = cfg_generativeFaceVideoSEICoordinateQuantizationFactor.values[frameIdx];
        m_generativeFaceVideoSEI3DCoordinateFlag[frameIdx] = cfg_generativeFaceVideoSEI3DCoordinateFlag.values[frameIdx];
        m_generativeFaceVideoSEICoordinatePointNum[frameIdx] = cfg_generativeFaceVideoSEICoordinatePointNum.values[frameIdx];
        if (m_generativeFaceVideoSEI3DCoordinateFlag[frameIdx] == 1)
        {
          m_generativeFaceVideoSEIZCoordinateMaxValue[frameIdx].push_back(cfg_generativeFaceVideoSEIZCoordinateMaxValue.values[frameIdx]);
        }
      }
      else
      {
        CHECK(m_generativeFaceVideoSEIId[frameIdx] != m_generativeFaceVideoSEIId[0], "Cannot find a reference SEI for key point prediction");
        m_generativeFaceVideoSEICoordinateQuantizationFactor[frameIdx] = m_generativeFaceVideoSEICoordinateQuantizationFactor[0];
        m_generativeFaceVideoSEI3DCoordinateFlag[frameIdx] = m_generativeFaceVideoSEI3DCoordinateFlag[0];
        m_generativeFaceVideoSEICoordinatePointNum[frameIdx] = m_generativeFaceVideoSEICoordinatePointNum[0];
        if (m_generativeFaceVideoSEI3DCoordinateFlag[frameIdx] == 1)
        {
          m_generativeFaceVideoSEIZCoordinateMaxValue[frameIdx].push_back(m_generativeFaceVideoSEIZCoordinateMaxValue[0][0]);
        }
      }
      m_generativeFaceVideoSEICoordinateXTesonr.push_back(std::vector<double>());
      m_generativeFaceVideoSEICoordinateYTesonr.push_back(std::vector<double>());
      m_generativeFaceVideoSEICoordinateZTesonr.push_back(std::vector<double>());
      m_generativeFaceVideoSEIMatrixElement.push_back(std::vector<std::vector<std::vector<std::vector<double>>>>());
      if (m_generativeFaceVideoSEICoordinatePresentFlag[frameIdx] == 1)
      {
        for (uint32_t coordinateId = 0; coordinateId < m_generativeFaceVideoSEICoordinatePointNum[frameIdx]; coordinateId++) 
        {
          m_generativeFaceVideoSEICoordinateXTesonr[frameIdx].push_back(cfg_generativeFaceVideoSEICoordinateXTesonr.values[cooridinateBeforeSum + coordinateId]);
          m_generativeFaceVideoSEICoordinateYTesonr[frameIdx].push_back(cfg_generativeFaceVideoSEICoordinateYTesonr.values[cooridinateBeforeSum + coordinateId]);
        }
        cooridinateBeforeSum = cooridinateBeforeSum + m_generativeFaceVideoSEICoordinatePointNum[frameIdx];
        if (m_generativeFaceVideoSEI3DCoordinateFlag[frameIdx] == 1) 
        {
          m_generativeFaceVideoSEIZCoordinateMaxValue[frameIdx].push_back(cfg_generativeFaceVideoSEIZCoordinateMaxValue.values[frameIdx]);
          for (uint32_t coordinateId = 0; coordinateId < m_generativeFaceVideoSEICoordinatePointNum[frameIdx]; coordinateId++) 
          {
            m_generativeFaceVideoSEICoordinateZTesonr[frameIdx].push_back(cfg_generativeFaceVideoSEICoordinateZTesonr.values[cooridinateZBeforeSum + coordinateId]);
          }
          cooridinateZBeforeSum = cooridinateZBeforeSum + m_generativeFaceVideoSEICoordinatePointNum[frameIdx];
        }
      }
      m_generativeFaceVideoSEIMatrixPresentFlag[frameIdx] = cfg_generativeFaceVideoSEIMatrixPresentFlag.values[frameIdx];
      m_generativeFaceVideoSEIMatrixPredFlag[frameIdx] = cfg_generativeFaceVideoSEIMatrixPredFlag.values[frameIdx];
      if (basePicFlag)
      {
        m_generativeFaceVideoSEIMatrixPredFlag[frameIdx] = 0;
      }
      if (basePicFlag || !m_generativeFaceVideoSEIMatrixPredFlag[frameIdx])
      {
        m_generativeFaceVideoSEIMatrixElementPrecisionFactor[frameIdx] = cfg_generativeFaceVideoSEIMatrixElementPrecisionFactor.values[frameIdx];
        m_generativeFaceVideoSEINumMatrixType[frameIdx] = cfg_generativeFaceVideoSEINumMatrixType.values[frameIdx];
      }
      else
      {
        CHECK(m_generativeFaceVideoSEIId[frameIdx] != m_generativeFaceVideoSEIId[0], "Cannot find a reference SEI for matrix prediction");
        m_generativeFaceVideoSEIMatrixElementPrecisionFactor[frameIdx] = m_generativeFaceVideoSEIMatrixElementPrecisionFactor[0];
        m_generativeFaceVideoSEINumMatrixType[frameIdx] = m_generativeFaceVideoSEINumMatrixType[0];
      }
      m_generativeFaceVideoSEIMatrixTypeIdx.push_back(std::vector<uint32_t>());
      m_generativeFaceVideoSEIMatrix3DSpaceFlag.push_back(std::vector<uint32_t>());
      m_generativeFaceVideoSEINumMatrices.push_back(std::vector<uint32_t>());
      m_generativeFaceVideoSEIMatrixWidth.push_back(std::vector<uint32_t>());
      m_generativeFaceVideoSEIMatrixHeight.push_back(std::vector<uint32_t>());
      m_generativeFaceVideoSEINumMatricestoNumKpsFlag.push_back(std::vector<uint32_t>());
      m_generativeFaceVideoSEINumMatricesInfo.push_back(std::vector<uint32_t>());
      if (m_generativeFaceVideoSEIMatrixPresentFlag[frameIdx] == 1) 
      {
        uint32_t matrixWidth=0;
        uint32_t matrixHeight=0;
        uint32_t numMatrices=0;
        for (uint32_t matrixId = 0; matrixId < m_generativeFaceVideoSEINumMatrixType[frameIdx]; matrixId++)
        {
          m_generativeFaceVideoSEIMatrixElement[frameIdx].push_back(std::vector<std::vector<std::vector<double>>>());
          if (basePicFlag || !m_generativeFaceVideoSEIMatrixPredFlag[frameIdx])
          {
            m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx].push_back(cfg_generativeFaceVideoSEIMatrixTypeIdx.values[matrixNumBeforeSum + matrixId]);
            m_generativeFaceVideoSEIMatrix3DSpaceFlag[frameIdx].push_back(cfg_generativeFaceVideoSEIMatrix3DSpaceFlag.values[matrixNumBeforeSum + matrixId]);
            m_generativeFaceVideoSEINumMatrices[frameIdx].push_back(cfg_generativeFaceVideoSEINumMatrices.values[matrixNumBeforeSum + matrixId]);
            m_generativeFaceVideoSEIMatrixWidth[frameIdx].push_back(cfg_generativeFaceVideoSEIMatrixWidth.values[matrixNumBeforeSum + matrixId]);
            m_generativeFaceVideoSEIMatrixHeight[frameIdx].push_back(cfg_generativeFaceVideoSEIMatrixHeight.values[matrixNumBeforeSum + matrixId]);
            m_generativeFaceVideoSEINumMatricestoNumKpsFlag[frameIdx].push_back(cfg_generativeFaceVideoSEINumMatricestoNumKpsFlag.values[matrixNumBeforeSum + matrixId]);
            m_generativeFaceVideoSEINumMatricesInfo[frameIdx].push_back(cfg_generativeFaceVideoSEINumMatricesInfo.values[matrixNumBeforeSum + matrixId]);
          }
          else
          {
            CHECK(m_generativeFaceVideoSEIId[frameIdx] != m_generativeFaceVideoSEIId[0], "Cannot find a reference SEI for matrix prediction")
            m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx].push_back(m_generativeFaceVideoSEIMatrixTypeIdx[0][matrixId]);
            m_generativeFaceVideoSEIMatrix3DSpaceFlag[frameIdx].push_back(m_generativeFaceVideoSEIMatrix3DSpaceFlag[0][matrixId]);
            m_generativeFaceVideoSEINumMatrices[frameIdx].push_back(m_generativeFaceVideoSEINumMatrices[0][matrixId]);
            m_generativeFaceVideoSEIMatrixWidth[frameIdx].push_back(m_generativeFaceVideoSEIMatrixWidth[0][matrixId]);
            m_generativeFaceVideoSEIMatrixHeight[frameIdx].push_back(m_generativeFaceVideoSEIMatrixHeight[0][matrixId]);
            m_generativeFaceVideoSEINumMatricestoNumKpsFlag[frameIdx].push_back(m_generativeFaceVideoSEINumMatricestoNumKpsFlag[0][matrixId]);
            m_generativeFaceVideoSEINumMatricesInfo[frameIdx].push_back(m_generativeFaceVideoSEINumMatricesInfo[0][matrixId]);
          }
          if (m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx][matrixId] == 0 || m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx][matrixId] == 1)
          {
            if (m_generativeFaceVideoSEI3DCoordinateFlag[frameIdx] == 1 || m_generativeFaceVideoSEIMatrix3DSpaceFlag[frameIdx][matrixId] == 1)
            {
              matrixWidth = 3;
              matrixHeight = 3;
            }
            else
            {
              matrixWidth = 2;
              matrixHeight = 2;
            }
            if (m_generativeFaceVideoSEICoordinatePresentFlag[frameIdx])
            {
              numMatrices = m_generativeFaceVideoSEINumMatricestoNumKpsFlag[frameIdx][matrixId] ? m_generativeFaceVideoSEICoordinatePointNum[frameIdx] : (m_generativeFaceVideoSEINumMatricesInfo[frameIdx][matrixId] < (m_generativeFaceVideoSEICoordinatePointNum[frameIdx] - 1) ? (m_generativeFaceVideoSEINumMatricesInfo[frameIdx][matrixId] + 1) : (m_generativeFaceVideoSEINumMatricesInfo[frameIdx][matrixId] + 2));
            }
            else
            {
              numMatrices = m_generativeFaceVideoSEINumMatricesInfo[frameIdx][matrixId] + 1;
            }
          }
          else if (m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx][matrixId] == 2 || m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx][matrixId] == 3 || m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx][matrixId] >= 7)
          {
            if (m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx][matrixId] >= 7)
            {
              numMatrices = m_generativeFaceVideoSEINumMatrices[frameIdx][matrixId];
            }
            else
            {
              numMatrices = 1;
            }
            matrixHeight = m_generativeFaceVideoSEIMatrixHeight[frameIdx][matrixId];
            matrixWidth = m_generativeFaceVideoSEIMatrixWidth[frameIdx][matrixId];
          }
          else if (m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx][matrixId] >= 4 && m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx][matrixId] <= 6)
          {
            if (m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx][matrixId] == 4)
            {
              if (m_generativeFaceVideoSEI3DCoordinateFlag[frameIdx] == 1 || m_generativeFaceVideoSEIMatrix3DSpaceFlag[frameIdx][matrixId] == 1)
              {
                matrixWidth = 3;
              }
              else
              {
                matrixWidth = 2;
              }
            }
            if (m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx][matrixId] == 5 || m_generativeFaceVideoSEIMatrixTypeIdx[frameIdx][matrixId] == 6)
            {
              matrixWidth = 1;
            }
            if (m_generativeFaceVideoSEI3DCoordinateFlag[frameIdx] == 1 || m_generativeFaceVideoSEIMatrix3DSpaceFlag[frameIdx][matrixId] == 1)
            {
              matrixHeight = 3;
            }
            else
            {
              matrixHeight = 2;
            }
            numMatrices = 1;
          }
          for (uint32_t j = 0; j < numMatrices; j++) 
          {
            m_generativeFaceVideoSEIMatrixElement[frameIdx][matrixId].push_back(std::vector< std::vector<double> >());
            for (uint32_t k = 0; k < matrixHeight; k++) 
            {
              m_generativeFaceVideoSEIMatrixElement[frameIdx][matrixId][j].push_back(std::vector<double>());
              for (uint32_t l = 0; l < matrixWidth; l++) 
              {
                 m_generativeFaceVideoSEIMatrixElement[frameIdx][matrixId][j][k].push_back(cfg_generativeFaceVideoSEIMatrixElement.values[matrixDimensionBeforeSum + j * matrixHeight * matrixWidth + k * matrixWidth + l]);
              }
            }
          }
          matrixDimensionBeforeSum = matrixDimensionBeforeSum + numMatrices * matrixHeight * matrixWidth;
        }
      }
      if (m_generativeFaceVideoSEINumMatrixType[frameIdx]==0) 
      {
        matrixNumBeforeSum = matrixNumBeforeSum + 1;
      }
      else 
      {
        matrixNumBeforeSum = matrixNumBeforeSum + m_generativeFaceVideoSEINumMatrixType[frameIdx];
      }
    }
  }

  if (m_generativeFaceVideoEnhancementEnabled)
  {
    CHECK(cfg_generativeFaceVideoEnhancementSEIId.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of GFVE ID must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEIGFVCnt.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of GFVE Cnt must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEIGFVId.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of GFVE ID must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of Matrix PrecisionFactor must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEIMatrixPresentFlag.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of Matrix PresentFlag must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEIMatrixPredFlag.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of Matrix PredFlag must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEINumMatrices.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of Matrix NumMatrices must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEIPupilPresentIdx.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of Pupil PresentIdx must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of GFVE GFVBasePicFlag must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of Pupil Pupil Coordinate PrecisionFactor must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of Pupil Left Eye CoordinateY must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of Pupil Right Eye CoordinateX must be equal to SEINumber");
    CHECK(cfg_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY.values.size() != m_generativeFaceVideoEnhancementSEINumber, "Number of Pupil Right Eye CoordinateY must be equal to SEINumber");
    m_generativeFaceVideoEnhancementSEIId.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEIGFVCnt.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEIGFVId.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEINumMatrices.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEIMatrixPresentFlag.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEIMatrixPredFlag.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEIPupilPresentIdx.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX.resize(m_generativeFaceVideoEnhancementSEINumber);
    m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY.resize(m_generativeFaceVideoEnhancementSEINumber);
    int matrixNumBeforeSum = 0;
    int matrixDimensionBeforeSum = 0;
    m_generativeFaceVideoEnhancementSEIMatrixElement.push_back(std::vector< std::vector< std::vector<double>> >());
    m_generativeFaceVideoEnhancementSEIMatrixElement.resize(m_generativeFaceVideoEnhancementSEINumber);
    for (uint32_t frameIdx = 0; frameIdx < m_generativeFaceVideoEnhancementSEINumber; frameIdx++) 
    {
      m_generativeFaceVideoEnhancementSEIId[frameIdx] = cfg_generativeFaceVideoEnhancementSEIId.values[frameIdx];
      m_generativeFaceVideoEnhancementSEIGFVCnt[frameIdx] = cfg_generativeFaceVideoEnhancementSEIGFVCnt.values[frameIdx];
      m_generativeFaceVideoEnhancementSEIGFVId[frameIdx] = cfg_generativeFaceVideoEnhancementSEIGFVId.values[frameIdx];
      m_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor[frameIdx] = cfg_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor.values[frameIdx];
      m_generativeFaceVideoEnhancementSEINumMatrices[frameIdx] = cfg_generativeFaceVideoEnhancementSEINumMatrices.values[frameIdx];
      m_generativeFaceVideoEnhancementSEIMatrixPresentFlag[frameIdx] = cfg_generativeFaceVideoEnhancementSEIMatrixPresentFlag.values[frameIdx];
      m_generativeFaceVideoEnhancementSEIMatrixPredFlag[frameIdx] = cfg_generativeFaceVideoEnhancementSEIMatrixPredFlag.values[frameIdx];     
      m_generativeFaceVideoEnhancementSEIPupilPresentIdx[frameIdx] = cfg_generativeFaceVideoEnhancementSEIPupilPresentIdx.values[frameIdx];
      m_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor[frameIdx] = cfg_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor.values[frameIdx];
      m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX[frameIdx] = cfg_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX.values[frameIdx];
      m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY[frameIdx] = cfg_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY.values[frameIdx];
      m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX[frameIdx] = cfg_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX.values[frameIdx];
      m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY[frameIdx] = cfg_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY.values[frameIdx];
      uint32_t numMatrices = m_generativeFaceVideoEnhancementSEINumMatrices[frameIdx];
      m_generativeFaceVideoEnhancementSEIMatrixWidth.push_back(std::vector<uint32_t>());
      m_generativeFaceVideoEnhancementSEIMatrixHeight.push_back(std::vector<uint32_t>());
      m_generativeFaceVideoEnhancementSEIMatrixElement[frameIdx].push_back(std::vector< std::vector<double> >());
      m_generativeFaceVideoEnhancementSEIMatrixElement[frameIdx].resize(numMatrices);
      for (uint32_t j = 0; j < numMatrices; j++) 
      {
        m_generativeFaceVideoEnhancementSEIMatrixWidth[frameIdx].push_back(cfg_generativeFaceVideoEnhancementSEIMatrixWidth.values[matrixNumBeforeSum+j]);
        m_generativeFaceVideoEnhancementSEIMatrixHeight[frameIdx].push_back(cfg_generativeFaceVideoEnhancementSEIMatrixHeight.values[matrixNumBeforeSum+j]);
        uint32_t matrixWidth =  m_generativeFaceVideoEnhancementSEIMatrixWidth[frameIdx][j];
        uint32_t matrixHeight = m_generativeFaceVideoEnhancementSEIMatrixHeight[frameIdx][j];  
        for (uint32_t k = 0; k < matrixHeight; k++) 
        {
          m_generativeFaceVideoEnhancementSEIMatrixElement[frameIdx][j].push_back(std::vector<double>());
          for (uint32_t l = 0; l < matrixWidth; l++) 
          {
            m_generativeFaceVideoEnhancementSEIMatrixElement[frameIdx][j][k].push_back(cfg_generativeFaceVideoEnhancementSEIMatrixElement.values[matrixDimensionBeforeSum + k * matrixWidth + l]);
          }
        }
        matrixDimensionBeforeSum = matrixDimensionBeforeSum +  matrixHeight * matrixWidth;
      }
      matrixNumBeforeSum=matrixNumBeforeSum+numMatrices;
    }
  }

#if JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
  if (m_aurSEIEnabled)
  {
    m_aurSEIRestrictions = cfg_aurSEIRestrictions.values;
    m_aurSEIContextPresentFlag = cfg_aurSEIContextPresentFlag.values;
    m_aurSEIContext = cfg_aurSEIContext.values;

  }
#endif 

  if( m_costMode == COST_LOSSLESS_CODING )
  {
    bool firstSliceLossless = false;
    if (m_mixedLossyLossless)
    {
      if (m_sliceLosslessArray.size() > 0)
      {
        for (uint32_t i = 0; i < m_sliceLosslessArray.size(); i++)
        {
          if (m_sliceLosslessArray[i] == 0)
          {
            firstSliceLossless = true;
            break;
          }
        }
      }
    }
    else
    {
      firstSliceLossless = true;
    }
    if (firstSliceLossless) // if first slice is lossless
      m_iQP = LOSSLESS_AND_MIXED_LOSSLESS_RD_COST_TEST_QP - ((m_internalBitDepth[ChannelType::LUMA] - 8) * 6);
  }

  m_maxCuWidth = m_maxCuHeight = m_ctuSize;

  CHECK(m_bimEnabled && m_dpfEnabled, "DPF is not compatible with BIM");
  CHECK(m_dpfEnabled && m_resChangeInClvsEnabled, "DPF is not compatible with resolution change in CLVS");

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  if (m_priSEIEnabled)
  {
    CHECK(m_priSEICancelFlag, "SEIPRICancelFlag must be 0 in this implementation");
    CHECK(!m_priSEIPersistenceFlag, "SEIPRIPersistenceFlag must be 1 in this implementation");
    CHECK(cfg_priSEIResamplingWidthNumMinus1.values.size() != (m_priSEINumResamplingRatiosMinus1 + 1), "Number of elements in SEIRPRIesamplingWidthNumMinus1 must be equal to SEIPRINumResamplingRatiosMinus1 + 1");
    CHECK(cfg_priSEIResamplingWidthDenomMinus1.values.size() != (m_priSEINumResamplingRatiosMinus1 + 1), "Number of elements in SEIPRIResamplingWidthNumMinus1 must be equal to SEIPRIResamplingWidthDenomMinus1 + 1");
    CHECK(cfg_priSEIResamplingHeightNumMinus1.values.size() != (m_priSEINumResamplingRatiosMinus1 + 1), "Number of elements in SEIPRIResamplingHeightNumMinus1 must be equal to SEIPRINumResamplingRatiosMinus1 + 1");
    CHECK(cfg_priSEIResamplingHeightDenomMinus1.values.size() != (m_priSEINumResamplingRatiosMinus1 + 1), "Number of elements in SEIPRIResamplingHeightNumMinus1 must be equal to SEIPRIResamplingWidthDenomMinus1 + 1");
    if (m_priSEINumResamplingRatiosMinus1 > 0)
    {
      m_priSEIResamplingWidthNumMinus1 = cfg_priSEIResamplingWidthNumMinus1.values;
      m_priSEIResamplingWidthDenomMinus1 = cfg_priSEIResamplingWidthDenomMinus1.values;
      m_priSEIResamplingHeightNumMinus1 = cfg_priSEIResamplingHeightNumMinus1.values;
      m_priSEIResamplingHeightDenomMinus1 = cfg_priSEIResamplingHeightDenomMinus1.values;
      m_priSEIFixedAspectRatioFlag.resize(m_priSEINumResamplingRatiosMinus1 + 1);
      for (uint32_t i = 1; i <= m_priSEINumResamplingRatiosMinus1; i++)
      {
        m_priSEIFixedAspectRatioFlag[i] =
          m_priSEIResamplingWidthNumMinus1[i] == m_priSEIResamplingHeightNumMinus1[i] &&
          m_priSEIResamplingWidthDenomMinus1[i] == m_priSEIResamplingHeightDenomMinus1[i];
      }
      // First entry has fixed values
      m_priSEIResamplingWidthNumMinus1[0] = 0;
      m_priSEIResamplingWidthDenomMinus1[0] = 0;
      m_priSEIFixedAspectRatioFlag[0] = true;
      m_priSEIResamplingHeightNumMinus1[0] = 0;
      m_priSEIResamplingHeightDenomMinus1[0] = 0;
    }
    else
    {
      m_priSEIResamplingWidthNumMinus1 = {0};
      m_priSEIResamplingWidthDenomMinus1 = {0};
      m_priSEIFixedAspectRatioFlag = {true};
      m_priSEIResamplingHeightNumMinus1 = {0};
      m_priSEIResamplingHeightDenomMinus1 = {0};
    }

    if (m_priSEIRegionIdPresentFlag)
    {
      CHECK(cfg_priSEIRegionId.values.size() != (m_priSEINumRegionsMinus1 + 1), "Number of elements in SEIPRIRegionId must be equal to SEIPRINumRegionsMinus1 + 1");
      std::vector<uint32_t> tmpVec = cfg_priSEIRegionId.values;
      std::sort(tmpVec.begin(), tmpVec.end());
      auto it = std::unique(tmpVec.begin(), tmpVec.end());
      CHECK(it != tmpVec.end(), "SEIRegionId values must be unique");
    }
    if (m_priSEIMultilayerFlag)
    {
      m_priSEIRegionLayerId = cfg_priSEIRegionLayerId.values;
      m_priSEIRegionIsALayerFlag = cfg_priSEIRegionIsALayerFlag.values;
    }
    CHECK(cfg_priSEIRegionTopLeftInUnitsX.values.size() != (m_priSEINumRegionsMinus1 + 1), "Number of elements in SEIPRIRegionTopLeftInUnitsX must be equal to SEIPRINumRegionsMinus1 + 1");
    CHECK(cfg_priSEIRegionTopLeftInUnitsY.values.size() != (m_priSEINumRegionsMinus1 + 1), "Number of elements in SEIPRIRegionTopLeftInUnitsY must be equal to SEIPRINumRegionsMinus1 + 1");
    CHECK(cfg_priSEIRegionWidthInUnitsMinus1.values.size() != (m_priSEINumRegionsMinus1 + 1), "Number of elements in SEIPRIRegionWidthInUnitsMinus1 must be equal to SEIPRINumRegionsMinus1 + 1");
    CHECK(cfg_priSEIRegionHeightInUnitsMinus1.values.size() != (m_priSEINumRegionsMinus1 + 1), "Number of elements in SEIPRIRegionHeightInUnitsMinus1 must be equal to SEIPRINumRegionsMinus1 + 1");
    if (m_priSEINumResamplingRatiosMinus1 > 0)
    {
      CHECK(cfg_priSEIResamplingRatioIdx.values.size() != (m_priSEINumRegionsMinus1 + 1), "Number of elements in SEIPRIResamplingRatioIdx must be equal to SEIPRINumRegionsMinus1 + 1");
    }
#if JVET_AL0324_AL0070_PRI_SEI
    if (m_priSEITargetPicParamsPresentFlag)
    {
      CHECK(cfg_priSEITargetRegionTopLeftInUnitsX.values.size() != (m_priSEINumRegionsMinus1 + 1), "Number of elements in SEIPRITargetRegionTopLeftInUnitsX must be equal to SEIPRINumRegionsMinus1 + 1");
      CHECK(cfg_priSEITargetRegionTopLeftInUnitsY.values.size() != (m_priSEINumRegionsMinus1 + 1), "Number of elements in SEIPRITargetRegionTopLeftInUnitsY must be equal to SEIPRINumRegionsMinus1 + 1");
    }
#else
    CHECK(cfg_priSEITargetRegionTopLeftX.values.size() != (m_priSEINumRegionsMinus1 + 1), "Number of elements in SEIPRITargetRegionTopLeftX must be equal to SEIPRINumRegionsMinus1 + 1");
    CHECK(cfg_priSEITargetRegionTopLeftY.values.size() != (m_priSEINumRegionsMinus1 + 1), "Number of elements in SEIPRITargetRegionTopLeftY must be equal to SEIPRINumRegionsMinus1 + 1");
#endif
    m_priSEIRegionId = cfg_priSEIRegionId.values;
    m_priSEIRegionTopLeftInUnitsX = cfg_priSEIRegionTopLeftInUnitsX.values;
    m_priSEIRegionTopLeftInUnitsY = cfg_priSEIRegionTopLeftInUnitsY.values;
    m_priSEIRegionWidthInUnitsMinus1 = cfg_priSEIRegionWidthInUnitsMinus1.values;
    m_priSEIRegionHeightInUnitsMinus1 = cfg_priSEIRegionHeightInUnitsMinus1.values;
    if (m_priSEINumResamplingRatiosMinus1 > 0)
    {
      m_priSEIResamplingRatioIdx = cfg_priSEIResamplingRatioIdx.values;
    }
    else
    {
      m_priSEIResamplingRatioIdx = {0};
    }
#if JVET_AL0324_AL0070_PRI_SEI
    m_priSEITargetRegionTopLeftInUnitsX = cfg_priSEITargetRegionTopLeftInUnitsX.values;
    m_priSEITargetRegionTopLeftInUnitsY = cfg_priSEITargetRegionTopLeftInUnitsY.values;
#else
    m_priSEITargetRegionTopLeftX = cfg_priSEITargetRegionTopLeftX.values;
    m_priSEITargetRegionTopLeftY = cfg_priSEITargetRegionTopLeftY.values;
#endif
  }
#endif

  // check validity of input parameters
  if( xCheckParameter() )
  {
    // return check failed
    return false;
  }

  // print-out parameters
  xPrintParameter();

  return true;
}
#ifdef _MSC_VER
// Restore optimizations
#pragma optimize( "", on )
#endif


// ====================================================================================================================
// Private member functions
// ====================================================================================================================

///< auto determine the profile to use given the other configuration settings. Returns 1 if erred. Can select profile 'NONE'

int EncAppCfg::xAutoDetermineProfile()
{
  const int maxBitDepth =
    std::max(m_internalBitDepth[ChannelType::LUMA], m_internalBitDepth[getLastChannel(m_chromaFormatIdc)]);
  m_profile=Profile::NONE;

  switch (m_chromaFormatIdc)
  {
  case ChromaFormat::_400:
  case ChromaFormat::_420:
    if (maxBitDepth <= 10)
    {
      if (m_level == Level::LEVEL15_5 && m_framesToBeEncoded == 1)
      {
        m_profile = m_maxLayers > 1 ? Profile::MULTILAYER_MAIN_10_STILL_PICTURE : Profile::MAIN_10_STILL_PICTURE;
      }
      else
      {
        m_profile = m_maxLayers > 1 ? Profile::MULTILAYER_MAIN_10 : Profile::MAIN_10;
      }
    }
    else if (maxBitDepth <= 12)
    {
      m_profile = (m_level == Level::LEVEL15_5 && m_framesToBeEncoded == 1) ? Profile::MAIN_12_STILL_PICTURE
                  : (m_intraPeriod == 1)                                    ? Profile::MAIN_12_INTRA
                                                                            : Profile::MAIN_12;
    }
    else if (maxBitDepth <= 16)
    {
      // Since there's no 16bit 420 profiles in VVC, we use 444 profiles.
      m_profile = (m_level == Level::LEVEL15_5 && m_framesToBeEncoded == 1) ? Profile::MAIN_16_444_STILL_PICTURE
                  : (m_intraPeriod == 1)                                    ? Profile::MAIN_16_444_INTRA
                                                                            : Profile::MAIN_16_444;
    }
    break;

  case ChromaFormat::_422:
  case ChromaFormat::_444:
    if (maxBitDepth <= 10)
    {
      if (m_level == Level::LEVEL15_5 && m_framesToBeEncoded == 1)
      {
        m_profile =
          m_maxLayers > 1 ? Profile::MULTILAYER_MAIN_10_444_STILL_PICTURE : Profile::MAIN_10_444_STILL_PICTURE;
      }
      else
      {
        m_profile = m_maxLayers > 1 ? Profile::MULTILAYER_MAIN_10_444 : Profile::MAIN_10_444;
      }
    }
    else if (maxBitDepth <= 12)
    {
      m_profile = (m_level == Level::LEVEL15_5 && m_framesToBeEncoded == 1) ? Profile::MAIN_12_444_STILL_PICTURE
                  : (m_intraPeriod == 1)                                    ? Profile::MAIN_12_444_INTRA
                                                                            : Profile::MAIN_12_444;
    }
    else if (maxBitDepth <= 16)
    {
      m_profile = (m_level == Level::LEVEL15_5 && m_framesToBeEncoded == 1) ? Profile::MAIN_16_444_STILL_PICTURE
                  : (m_intraPeriod == 1)                                    ? Profile::MAIN_16_444_INTRA
                                                                            : Profile::MAIN_16_444;
    }
    break;

  default: return 1;
  }
  if (m_profile == Profile::MAIN_12_INTRA || m_profile == Profile::MAIN_12_444_INTRA || m_profile == Profile::MAIN_16_444_INTRA ||
      m_profile == Profile::MAIN_12_STILL_PICTURE || m_profile == Profile::MAIN_12_444_STILL_PICTURE || m_profile == Profile::MAIN_16_444_STILL_PICTURE)
  {
    m_allRapPicturesFlag = 1;
  }
  return 0;
}

bool EncAppCfg::xCheckParameter()
{
  msg( NOTICE, "\n" );
  if (m_decodedPictureHashSEIType == HashType::NONE)
  {
    msg( DETAILS, "******************************************************************\n");
    msg( DETAILS, "** WARNING: --SEIDecodedPictureHash is now disabled by default. **\n");
    msg( DETAILS, "**          Automatic verification of decoded pictures by a     **\n");
    msg( DETAILS, "**          decoder requires this option to be enabled.         **\n");
    msg( DETAILS, "******************************************************************\n");
  }
  if( m_profile==Profile::NONE )
  {
    msg( DETAILS, "***************************************************************************\n");
    msg( DETAILS, "** WARNING: For conforming bitstreams a valid Profile value must be set! **\n");
    msg( DETAILS, "***************************************************************************\n");
  }
  if( m_level==Level::NONE )
  {
    msg( DETAILS, "***************************************************************************\n");
    msg( DETAILS, "** WARNING: For conforming bitstreams a valid Level value must be set!   **\n");
    msg( DETAILS, "***************************************************************************\n");
  }

  bool check_failed = false; /* abort if there is a fatal configuration problem */
#define xConfirmPara(a,b) check_failed |= confirmPara(a,b)

  xConfirmPara(m_alfapsIDShift < 0, "ALF APSs shift should be positive");
  xConfirmPara(m_alfapsIDShift + m_maxNumAlfAps > ALF_CTB_MAX_NUM_APS, "The number of ALF APSs should not be more than ALF_CTB_MAX_NUM_APS");

  if( m_depQuantEnabledFlag )
  {
    xConfirmPara( !m_useRDOQ || !m_useRDOQTS, "RDOQ and RDOQTS must be equal to 1 if dependent quantization is enabled" );
    xConfirmPara( m_signDataHidingEnabledFlag, "SignHideFlag must be equal to 0 if dependent quantization is enabled" );
  }

  if( m_wrapAround )
  {
    const int minCUSize = 1 << m_log2MinCuSize;
    xConfirmPara(m_wrapAroundOffset <= m_ctuSize + minCUSize,
                 "Wrap-around offset must be greater than CtbSizeY + MinCbSize");
    xConfirmPara(m_wrapAroundOffset > m_sourceWidth, "Wrap-around offset must not be greater than the source picture width");
    xConfirmPara( m_wrapAroundOffset % minCUSize != 0, "Wrap-around offset must be an integer multiple of the specified minimum CU size" );
  }


#if SHARP_LUMA_DELTA_QP && ENABLE_QPA
  xConfirmPara( m_bUsePerceptQPA && m_lumaLevelToDeltaQPMapping.mode >= 2, "QPA and SharpDeltaQP mode 2 cannot be used together" );
  if( m_bUsePerceptQPA && m_lumaLevelToDeltaQPMapping.mode == LUMALVL_TO_DQP_AVG_METHOD )
  {
    msg( WARNING, "*********************************************************************************\n" );
    msg( WARNING, "** WARNING: Applying custom luma-based QPA with activity-based perceptual QPA! **\n" );
    msg( WARNING, "*********************************************************************************\n" );

    m_lumaLevelToDeltaQPMapping.mode = LUMALVL_TO_DQP_NUM_MODES; // special QPA mode
  }
#endif


  xConfirmPara( m_useAMaxBT && !m_SplitConsOverrideEnabledFlag, "AMaxBt can only be used with PartitionConstriantsOverride enabled" );


  xConfirmPara(m_bitstreamFileName.empty(), "A bitstream file name must be specified (BitstreamFile)");
  xConfirmPara(m_internalBitDepth[ChannelType::CHROMA] != m_internalBitDepth[ChannelType::LUMA],
               "The internalBitDepth must be the same for luma and chroma");
  if (m_profile != Profile::NONE)
  {
    xConfirmPara(m_log2MaxTransformSkipBlockSize>=6, "Transform Skip Log2 Max Size must be less or equal to 5 for given profile.");
    xConfirmPara(m_transformSkipRotationEnabledFlag==true, "UseResidualRotation must not be enabled for given profile.");
    xConfirmPara(m_transformSkipContextEnabledFlag==true, "UseSingleSignificanceMapContext must not be enabled for given profile.");
    xConfirmPara(m_highPrecisionOffsetsEnabledFlag==true, "UseHighPrecisionPredictionWeighting must not be enabled for given profile.");
    xConfirmPara(m_cabacBypassAlignmentEnabledFlag, "AlignCABACBeforeBypass cannot be enabled for given profile.");
  }
  if (m_profile != Profile::NONE && m_profile != Profile::MAIN_12_444 && m_profile != Profile::MAIN_16_444 && m_profile != Profile::MAIN_12_444_INTRA && m_profile != Profile::MAIN_16_444_INTRA && m_profile != Profile::MAIN_12_444_STILL_PICTURE && m_profile != Profile::MAIN_12_444_STILL_PICTURE && m_profile != Profile::MAIN_16_444_STILL_PICTURE)
  {
    xConfirmPara(m_rrcRiceExtensionEnableFlag == true, "Extention of the Golomb-Rice parameter derivation for RRC must not be enabled for given profile.");
    xConfirmPara(m_persistentRiceAdaptationEnabledFlag==true, "GolombRiceParameterAdaption must not be enabled for given profile.");
    xConfirmPara(m_extendedPrecisionProcessingFlag==true, "UseExtendedPrecision must not be enabled for given profile.");
    xConfirmPara(m_tsrcRicePresentFlag == true, "TSRCRicePresent must not be enabled for given profile.");
    xConfirmPara(m_reverseLastSigCoeffEnabledFlag == true, "ReverseLastSigCoeff must not be enabled for given profile.");
  }


  // check range of parameters
  xConfirmPara(m_inputBitDepth[ChannelType::LUMA] < 8, "InputBitDepth must be at least 8");
  xConfirmPara(m_inputBitDepth[ChannelType::CHROMA] < 8, "InputBitDepthC must be at least 8");

  if ((m_internalBitDepth[ChannelType::LUMA] < m_inputBitDepth[ChannelType::LUMA])
      || (m_internalBitDepth[ChannelType::CHROMA] < m_inputBitDepth[ChannelType::CHROMA]))
  {
      msg(WARNING, "*****************************************************************************\n");
      msg(WARNING, "** WARNING: InternalBitDepth is set to the lower value than InputBitDepth! **\n");
      msg(WARNING, "**          min_qp_prime_ts_minus4 will be clipped to 0 at the low end!    **\n");
      msg(WARNING, "*****************************************************************************\n");
  }

#if !RExt__HIGH_BIT_DEPTH_SUPPORT
  if (m_extendedPrecisionProcessingFlag)
  {
    for (const auto bd: m_internalBitDepth)
    {
      xConfirmPara((bd > 8), "Model is not configured to support high enough internal accuracies - enable "
                             "RExt__HIGH_BIT_DEPTH_SUPPORT to use increased precision internal data types etc...");
    }
  }
  else
  {
    for (const auto bd: m_internalBitDepth)
    {
      xConfirmPara((bd > 12), "Model is not configured to support high enough internal accuracies - enable "
                              "RExt__HIGH_BIT_DEPTH_SUPPORT to use increased precision internal data types etc...");
    }
  }
#endif

  xConfirmPara((m_msbExtendedBitDepth[ChannelType::LUMA] < m_inputBitDepth[ChannelType::LUMA]),
               "MSB-extended bit depth for luma channel (--MSBExtendedBitDepth) must be greater than or equal to input "
               "bit depth for luma channel (--InputBitDepth)");
  xConfirmPara((m_msbExtendedBitDepth[ChannelType::CHROMA] < m_inputBitDepth[ChannelType::CHROMA]),
               "MSB-extended bit depth for chroma channel (--MSBExtendedBitDepthC) must be greater than or equal to "
               "input bit depth for chroma channel (--InputBitDepthC)");

  bool check_sps_range_extension_flag = m_extendedPrecisionProcessingFlag || m_rrcRiceExtensionEnableFlag
                                        || m_persistentRiceAdaptationEnabledFlag || m_tsrcRicePresentFlag;
  if (m_internalBitDepth[ChannelType::LUMA] <= 10)
  {
    xConfirmPara( (check_sps_range_extension_flag == 1) ,
                 "RExt tools (Extended Precision Processing, RRC Rice Extension, Persistent Rice Adaptation and TSRC Rice Extension) must be disabled for BitDepth is less than or equal to 10 (the value of sps_range_extension_flag shall be 0 when BitDepth is less than or equal to 10.)");
  }
  xConfirmPara(m_chromaFormatIdc >= ChromaFormat::NUM, "ChromaFormatIDC must be either 400, 420, 422 or 444");
  std::string sTempIPCSC="InputColourSpaceConvert must be empty, "+getListOfColourSpaceConverts(true);
  xConfirmPara( m_inputColourSpaceConvert >= NUMBER_INPUT_COLOUR_SPACE_CONVERSIONS,         sTempIPCSC.c_str() );
  xConfirmPara(m_inputChromaFormatIDC >= ChromaFormat::NUM, "InputChromaFormatIDC must be either 400, 420, 422 or 444");
  xConfirmPara(m_frameRate.getFloatVal() <= 0, "Frame rate cannot be 0 or less");
  xConfirmPara( m_framesToBeEncoded <= 0,                                                   "Total Number Of Frames encoded must be more than 0" );
  xConfirmPara( m_framesToBeEncoded < m_switchPOC,                                          "debug POC out of range" );

  xConfirmPara(m_gopSize < 1, "GOP Size must be greater or equal to 1");
  xConfirmPara(m_gopSize > 1 && m_gopSize % 2, "GOP Size must be a multiple of 2, if GOP Size is greater than 1");
  xConfirmPara((m_intraPeriod > 0 && m_intraPeriod < m_gopSize) || m_intraPeriod == 0,
               "Intra period must be more than GOP size, or -1 , not 0");
  xConfirmPara( m_drapPeriod < 0,                                                           "DRAP period must be greater or equal to 0" );
  xConfirmPara( m_edrapPeriod < 0,                                                          "EDRAP period must be greater or equal to 0" );
  xConfirmPara(m_intraRefreshType < 0 || m_intraRefreshType > 3,
               "Decoding Refresh Type must be comprised between 0 and 3 included");

  if (m_isField)
  {
    if (!m_frameFieldInfoSEIEnabled)
    {
      msg( WARNING, "*************************************************************************************\n");
      msg( WARNING, "** WARNING: Frame field information SEI should be enabled for field coding!        **\n");
      msg( WARNING, "*************************************************************************************\n");
    }
  }
  if ( m_pictureTimingSEIEnabled && (!m_bufferingPeriodSEIEnabled))
  {
    msg( WARNING, "****************************************************************************\n");
    msg( WARNING, "** WARNING: Picture Timing SEI requires Buffering Period SEI. Disabling.  **\n");
    msg( WARNING, "****************************************************************************\n");
    m_pictureTimingSEIEnabled = false;
  }

  xConfirmPara(m_bufferingPeriodSEIEnabled && m_rcCpbSize == 0,
               "RCCpbSize must be greater than zero, when buffering period SEI is enabled");

  xConfirmPara (m_log2MaxTransformSkipBlockSize < 2, "Transform Skip Log2 Max Size must be at least 2 (4x4)");

  xConfirmPara ( m_onePictureOnlyConstraintFlag && m_framesToBeEncoded!=1, "When onePictureOnlyConstraintFlag is true, the number of frames to be encoded must be 1" );
  if (m_profile != Profile::NONE)
  {
    const ProfileFeatures *features = ProfileFeatures::getProfileFeatures(m_profile);
    CHECK(features->profile != m_profile, "Profile not found");
    xConfirmPara(m_level == Level::LEVEL15_5 && !features->canUseLevel15p5, "Profile does not support level 15.5");
    xConfirmPara(m_level < Level::LEVEL4 && m_levelTier == Level::HIGH, "High tier not defined for levels below 4.");
  }

  xConfirmPara(m_iQP < -6 * (m_internalBitDepth[ChannelType::LUMA] - 8) || m_iQP > MAX_QP,
               "QP exceeds supported range (-QpBDOffsety to 63)");
  xConfirmPara( m_deblockingFilterMetric!=0 && (m_deblockingFilterDisable || m_deblockingFilterOffsetInPPS), "If DeblockingFilterMetric is non-zero then both LoopFilterDisable and LoopFilterOffsetInPPS must be 0");
  xConfirmPara( m_deblockingFilterBetaOffsetDiv2 < -12 || m_deblockingFilterBetaOffsetDiv2 > 12,          "Loop Filter Beta Offset div. 2 exceeds supported range (-12 to 12" );
  xConfirmPara( m_deblockingFilterTcOffsetDiv2 < -12 || m_deblockingFilterTcOffsetDiv2 > 12,              "Loop Filter Tc Offset div. 2 exceeds supported range (-12 to 12)" );
  xConfirmPara( m_deblockingFilterCbBetaOffsetDiv2 < -12 || m_deblockingFilterCbBetaOffsetDiv2 > 12,      "Loop Filter Beta Offset div. 2 exceeds supported range (-12 to 12" );
  xConfirmPara( m_deblockingFilterCbTcOffsetDiv2 < -12 || m_deblockingFilterCbTcOffsetDiv2 > 12,          "Loop Filter Tc Offset div. 2 exceeds supported range (-12 to 12)" );
  xConfirmPara( m_deblockingFilterCrBetaOffsetDiv2 < -12 || m_deblockingFilterCrBetaOffsetDiv2 > 12,      "Loop Filter Beta Offset div. 2 exceeds supported range (-12 to 12" );
  xConfirmPara( m_deblockingFilterCrTcOffsetDiv2 < -12 || m_deblockingFilterCrTcOffsetDiv2 > 12,          "Loop Filter Tc Offset div. 2 exceeds supported range (-12 to 12)" );
  xConfirmPara( m_iSearchRange < 0 ,                                                        "Search Range must be more than 0" );
  xConfirmPara( m_bipredSearchRange < 0 ,                                                   "Bi-prediction refinement search range must be more than 0" );
  xConfirmPara( m_minSearchWindow < 0,                                                      "Minimum motion search window size for the adaptive window ME must be greater than or equal to 0" );
  xConfirmPara( m_iMaxDeltaQP > MAX_DELTA_QP,                                               "Absolute Delta QP exceeds supported range (0 to 7)" );
#if ENABLE_QPA
  xConfirmPara( m_bUsePerceptQPA && m_uiDeltaQpRD > 0,                                      "Perceptual QPA cannot be used together with slice-level multiple-QP optimization" );
#endif
#if SHARP_LUMA_DELTA_QP
  xConfirmPara( m_lumaLevelToDeltaQPMapping.mode && m_uiDeltaQpRD > 0,                      "Luma-level-based Delta QP cannot be used together with slice level multiple-QP optimization\n" );
  xConfirmPara(m_lumaLevelToDeltaQPMapping.mode && m_rcEnableRateControl,
               "Luma-level-based Delta QP cannot be used together with rate control\n");
#endif
  if (m_lumaLevelToDeltaQPMapping.mode && m_lmcsEnabled)
  {
    msg(WARNING, "For HDR-PQ, LMCS should be used mutual-exclusively with Luma-level-based Delta QP. If use LMCS, turn lumaDQP off.\n");
    m_lumaLevelToDeltaQPMapping.mode = LUMALVL_TO_DQP_DISABLED;
  }
  if (!m_lmcsEnabled)
  {
    m_reshapeSignalType = RESHAPE_SIGNAL_NULL;
    m_intraCMD = 0;
  }
  if (m_lmcsEnabled && m_reshapeSignalType == RESHAPE_SIGNAL_PQ)
  {
    m_intraCMD = 1;
  }
  else if (m_lmcsEnabled && (m_reshapeSignalType == RESHAPE_SIGNAL_SDR || m_reshapeSignalType == RESHAPE_SIGNAL_HLG))
  {
    m_intraCMD = 0;
  }
  else
  {
    m_lmcsEnabled = false;
  }
  if (m_lmcsEnabled)
  {
    xConfirmPara(m_updateCtrl < 0, "Min. LMCS Update Control is 0");
    xConfirmPara(m_updateCtrl > 2, "Max. LMCS Update Control is 2");
    xConfirmPara(m_adpOption < 0, "Min. LMCS Adaptation Option is 0");
    xConfirmPara(m_adpOption > 4, "Max. LMCS Adaptation Option is 4");
    xConfirmPara(m_initialCW < 0, "Min. Initial Total Codeword is 0");
    xConfirmPara(m_initialCW > 1023, "Max. Initial Total Codeword is 1023");
    xConfirmPara(m_CSoffset < -7, "Min. LMCS Offset value is -7");
    xConfirmPara(m_CSoffset > 7, "Max. LMCS Offset value is 7");
    if (m_updateCtrl > 0 && m_adpOption > 2) { m_adpOption -= 2; }
  }

  if (m_ctiSEIEnabled)
  {
    xConfirmPara(m_ctiSEINumberChromaLut < 0 || m_ctiSEINumberChromaLut > 2, "CTI number of chroma LUTs is out of range");
  }
  xConfirmPara( m_cbQpOffset < -12,   "Min. Chroma Cb QP Offset is -12" );
  xConfirmPara( m_cbQpOffset >  12,   "Max. Chroma Cb QP Offset is  12" );
  xConfirmPara( m_crQpOffset < -12,   "Min. Chroma Cr QP Offset is -12" );
  xConfirmPara( m_crQpOffset >  12,   "Max. Chroma Cr QP Offset is  12" );
  xConfirmPara( m_cbQpOffsetDualTree < -12,   "Min. Chroma Cb QP Offset for dual tree is -12" );
  xConfirmPara( m_cbQpOffsetDualTree >  12,   "Max. Chroma Cb QP Offset for dual tree is  12" );
  xConfirmPara( m_crQpOffsetDualTree < -12,   "Min. Chroma Cr QP Offset for dual tree is -12" );
  xConfirmPara( m_crQpOffsetDualTree >  12,   "Max. Chroma Cr QP Offset for dual tree is  12" );
  if (m_dualTree && !isChromaEnabled(m_chromaFormatIdc))
  {
    msg( WARNING, "****************************************************************************\n");
    msg( WARNING, "** WARNING: --DualITree has been disabled because the chromaFormat is 400 **\n");
    msg( WARNING, "****************************************************************************\n");
    m_dualTree = false;
  }
  if (m_alf)
  {
    xConfirmPara(m_alfStrengthLuma < 0.0, "ALFStrengthLuma is less than 0. Valid range is 0.0 <= ALFStrengthLuma <= 1.0");
    xConfirmPara(m_alfStrengthLuma > 1.0, "ALFStrengthLuma is greater than 1. Valid range is 0.0 <= ALFStrengthLuma <= 1.0");
  }
  if (m_ccalf)
  {
    xConfirmPara(m_ccalfStrength < 0.0, "CCALFStrength is less than 0. Valid range is 0.0 <= CCALFStrength <= 1.0");
    xConfirmPara(m_ccalfStrength > 1.0, "CCALFStrength is greater than 1. Valid range is 0.0 <= CCALFStrength <= 1.0");
  }
  if (m_alf)
  {
    xConfirmPara(m_alfStrengthChroma < 0.0, "ALFStrengthChroma is less than 0. Valid range is 0.0 <= ALFStrengthChroma <= 1.0");
    xConfirmPara(m_alfStrengthChroma > 1.0, "ALFStrengthChroma is greater than 1. Valid range is 0.0 <= ALFStrengthChroma <= 1.0");
    xConfirmPara(m_alfStrengthTargetLuma < 0.0, "ALFStrengthTargetLuma is less than 0. Valid range is 0.0 <= ALFStrengthTargetLuma <= 1.0");
    xConfirmPara(m_alfStrengthTargetLuma > 1.0, "ALFStrengthTargetLuma is greater than 1. Valid range is 0.0 <= ALFStrengthTargetLuma <= 1.0");
    xConfirmPara(m_alfStrengthTargetChroma < 0.0, "ALFStrengthTargetChroma is less than 0. Valid range is 0.0 <= ALFStrengthTargetChroma <= 1.0");
    xConfirmPara(m_alfStrengthTargetChroma > 1.0, "ALFStrengthTargetChroma is greater than 1. Valid range is 0.0 <= ALFStrengthTargetChroma <= 1.0");
  }
  if (m_ccalf)
  {
    xConfirmPara(m_ccalfStrengthTarget < 0.0, "CCALFStrengthTarget is less than 0. Valid range is 0.0 <= CCALFStrengthTarget <= 1.0");
    xConfirmPara(m_ccalfStrengthTarget > 1.0, "CCALFStrengthTarget is greater than 1. Valid range is 0.0 <= CCALFStrengthTarget <= 1.0");
  }
  if (m_ccalf && !isChromaEnabled(m_chromaFormatIdc))
  {
    msg( WARNING, "****************************************************************************\n");
    msg( WARNING, "** WARNING: --CCALF has been disabled because the chromaFormat is 400     **\n");
    msg( WARNING, "****************************************************************************\n");
    m_ccalf = false;
  }
  if (m_jointCbCrMode && !isChromaEnabled(m_chromaFormatIdc))
  {
    msg( WARNING, "****************************************************************************\n");
    msg( WARNING, "** WARNING: --JointCbCr has been disabled because the chromaFormat is 400 **\n");
    msg( WARNING, "****************************************************************************\n");
    m_jointCbCrMode = false;
  }
  if (m_jointCbCrMode)
  {
    xConfirmPara( m_cbCrQpOffset < -12, "Min. Joint Cb-Cr QP Offset is -12");
    xConfirmPara( m_cbCrQpOffset >  12, "Max. Joint Cb-Cr QP Offset is  12");
    xConfirmPara( m_cbCrQpOffsetDualTree < -12, "Min. Joint Cb-Cr QP Offset for dual tree is -12");
    xConfirmPara( m_cbCrQpOffsetDualTree >  12, "Max. Joint Cb-Cr QP Offset for dual tree is  12");
  }
  xConfirmPara( m_iQPAdaptationRange <= 0,                                                  "QP Adaptation Range must be more than 0" );
  if (m_intraRefreshType == 2)
  {
    xConfirmPara(m_intraPeriod > 0 && m_intraPeriod <= m_gopSize,
                 "Intra period must be larger than GOP size for periodic IDR pictures");
  }
  xConfirmPara(m_maxCuWidth > MAX_CU_SIZE, "MaxCUWith exceeds predefined MAX_CU_SIZE limit");

  const int minCuSize = 1 << m_log2MinCuSize;
  xConfirmPara(m_minQt[0] > 64, "Min Luma QT size in I slices should be smaller than or equal to 64");
  xConfirmPara(m_minQt[1] > 64, "Min Luma QT size in non-I slices should be smaller than or equal to 64");
  xConfirmPara(m_maxBt[2] > 64, "Maximum BT size for chroma block in I slice should be smaller than or equal to 64");
  xConfirmPara(m_maxTt[0] > 64, "Maximum TT size for luma block in I slice should be smaller than or equal to 64");
  xConfirmPara(m_maxTt[1] > 64, "Maximum TT size for luma block in non-I slice should be smaller than or equal to 64");
  xConfirmPara(m_maxTt[2] > 64, "Maximum TT size for chroma block in I slice should be smaller than or equal to 64");
  xConfirmPara(m_minQt[0] < minCuSize, "Min Luma QT size in I slices should be larger than or equal to minCuSize");
  xConfirmPara(m_minQt[1] < minCuSize, "Min Luma QT size in non-I slices should be larger than or equal to minCuSize");
  xConfirmPara((m_sourceWidth % minCuSize ) || (m_sourceHeight % minCuSize),              "Picture width or height is not a multiple of minCuSize");
  const int minDiff =
    (int) floorLog2(m_minQt[2])
    - std::max(MIN_CU_LOG2, (int) m_log2MinCuSize - (int) getChannelTypeScaleX(ChannelType::CHROMA, m_chromaFormatIdc));
  xConfirmPara( minDiff < 0 ,                                                               "Min Chroma QT size in I slices is smaller than Min Luma CU size even considering color format");
  xConfirmPara((m_minQt[2] << (int) getChannelTypeScaleX(ChannelType::CHROMA, m_chromaFormatIdc))
                 > std::min(64, (int) m_ctuSize),
               "Min Chroma QT size in I slices should be smaller than or equal to CTB size or CB size after implicit "
               "split of CTB");
  xConfirmPara(m_ctuSize < 32, "CTUSize must be greater than or equal to 32");
  xConfirmPara(m_ctuSize > 128, "CTUSize must be less than or equal to 128");
  xConfirmPara(m_ctuSize != 32 && m_ctuSize != 64 && m_ctuSize != 128, "CTUSize must be a power of 2 (32, 64, or 128)");
  xConfirmPara(m_maxCuWidth < 16, "Maximum partition width size should be larger than or equal to 16");
  xConfirmPara(m_maxCuHeight < 16, "Maximum partition height size should be larger than or equal to 16");
  xConfirmPara(m_maxBt[0] < m_minQt[0],
               "Maximum BT size for luma block in I slice should be larger than minimum QT size");
  xConfirmPara(m_maxBt[0] > m_ctuSize,
               "Maximum BT size for luma block in I slice should be smaller than or equal to CTUSize");
  xConfirmPara(m_maxBt[1] < m_minQt[1],
               "Maximum BT size for luma block in non I slice should be larger than minimum QT size");
  xConfirmPara(m_maxBt[1] > m_ctuSize,
               "Maximum BT size for luma block in non I slice should be smaller than or equal to CTUSize");
  xConfirmPara(m_maxBt[2] < (m_minQt[2] << (int) getChannelTypeScaleX(ChannelType::CHROMA, m_chromaFormatIdc)),
               "Maximum BT size for chroma block in I slice should be larger than minimum QT size");
  xConfirmPara(m_maxBt[2] > m_ctuSize,
               "Maximum BT size for chroma block in I slice should be smaller than or equal to CTUSize");
  xConfirmPara(m_maxTt[0] < m_minQt[0],
               "Maximum TT size for luma block in I slice should be larger than minimum QT size");
  xConfirmPara(m_maxTt[0] > m_ctuSize,
               "Maximum TT size for luma block in I slice should be smaller than or equal to CTUSize");
  xConfirmPara(m_maxTt[1] < m_minQt[1],
               "Maximum TT size for luma block in non I slice should be larger than minimum QT size");
  xConfirmPara(m_maxTt[1] > m_ctuSize,
               "Maximum TT size for luma block in non I slice should be smaller than or equal to CTUSize");
  xConfirmPara(m_maxTt[2] < (m_minQt[2] << (int) getChannelTypeScaleX(ChannelType::CHROMA, m_chromaFormatIdc)),
               "Maximum TT size for chroma block in I slice should be larger than minimum QT size");
  xConfirmPara(m_maxTt[2] > m_ctuSize,
               "Maximum TT size for chroma block in I slice should be smaller than or equal to CTUSize");
  xConfirmPara( (m_sourceWidth  % (std::max(8u, m_log2MinCuSize))) != 0,                   "Resulting coded frame width must be a multiple of Max(8, the minimum CU size)");
  xConfirmPara( (m_sourceHeight % (std::max(8u, m_log2MinCuSize))) != 0,                   "Resulting coded frame height must be a multiple of Max(8, the minimum CU size)");
  if (m_uiMaxMTTHierarchyDepthI == 0)
  {
    xConfirmPara(m_maxBt[0] != m_minQt[0],
                 "MaxBTLumaISlice shall be equal to MinQTLumaISlice when MaxMTTHierarchyDepthISliceL is 0.");
    xConfirmPara(m_maxTt[0] != m_minQt[0],
                 "MaxTTLumaISlice shall be equal to MinQTLumaISlice when MaxMTTHierarchyDepthISliceL is 0.");
  }
  if (m_uiMaxMTTHierarchyDepthIChroma == 0)
  {
    xConfirmPara(m_maxBt[2] != (m_minQt[2] << (int) getChannelTypeScaleX(ChannelType::CHROMA, m_chromaFormatIdc)),
                 "MaxBTChromaISlice shall be equal to MinQTChromaISlice when MaxMTTHierarchyDepthISliceC is 0.");
    xConfirmPara(m_maxTt[2] != (m_minQt[2] << (int) getChannelTypeScaleX(ChannelType::CHROMA, m_chromaFormatIdc)),
                 "MaxTTChromaISlice shall be equal to MinQTChromaISlice when MaxMTTHierarchyDepthISliceC is 0.");
  }
  if (m_uiMaxMTTHierarchyDepth == 0)
  {
    xConfirmPara(m_maxBt[1] != m_minQt[1],
                 "MaxBTNonISlice shall be equal to MinQTNonISlice when MaxMTTHierarchyDepth is 0.");
    xConfirmPara(m_maxTt[1] != m_minQt[1],
                 "MaxTTNonISlice shall be equal to MinQTNonISlice when MaxMTTHierarchyDepth is 0.");
  }
  xConfirmPara( m_log2MaxTbSize > 6, "Log2MaxTbSize must be 6 or smaller." );
  xConfirmPara( m_log2MaxTbSize < 5,  "Log2MaxTbSize must be 5 or greater." );
  xConfirmPara( m_maxNumMergeCand < 1,  "MaxNumMergeCand must be 1 or greater.");
  xConfirmPara( m_maxNumMergeCand > MRG_MAX_NUM_CANDS, "MaxNumMergeCand must be no more than MRG_MAX_NUM_CANDS." );
  xConfirmPara( m_maxNumGeoCand > GEO_MAX_NUM_UNI_CANDS, "MaxNumGeoCand must be no more than GEO_MAX_NUM_UNI_CANDS." );
  xConfirmPara( m_maxNumGeoCand > m_maxNumMergeCand, "MaxNumGeoCand must be no more than MaxNumMergeCand." );
  xConfirmPara( 0 < m_maxNumGeoCand && m_maxNumGeoCand < 2, "MaxNumGeoCand must be no less than 2 unless MaxNumGeoCand is 0." );
  xConfirmPara( m_maxNumIBCMergeCand < 1, "MaxNumIBCMergeCand must be 1 or greater." );
  xConfirmPara( m_maxNumIBCMergeCand > IBC_MRG_MAX_NUM_CANDS, "MaxNumIBCMergeCand must be no more than IBC_MRG_MAX_NUM_CANDS." );
  xConfirmPara(m_maxNumAffineMergeCand < (m_sbTmvpEnableFlag ? 1 : 0),
               "MaxNumAffineMergeCand must be greater than 0 when SbTMVP is enabled");
  xConfirmPara( m_maxNumAffineMergeCand > AFFINE_MRG_MAX_NUM_CANDS, "MaxNumAffineMergeCand must be no more than AFFINE_MRG_MAX_NUM_CANDS." );
  constexpr int maxCandNum = NUM_MRG_SATD_CAND + 1 + NUM_AFF_MRG_SATD_CAND + GEO_MAX_TRY_WEIGHTED_SATD;
  // Note: maxCandNum=15 is an empirical value for the number of candidate in RD checking
  // Limit maximum value of MaxMergeRdCandNumTotal to maxCandNum. Larger values are not expected to be beneficial
  xConfirmPara( m_maxMergeRdCandNumTotal < 1 || m_maxMergeRdCandNumTotal > maxCandNum, 
    "MaxMergeRdCandNumTotal must be between 1 and 15, inclusive");
  xConfirmPara(m_mergeRdCandQuotaRegular < 0 || m_mergeRdCandQuotaRegular > maxCandNum
    || m_mergeRdCandQuotaRegularSmallBlk < 0 || m_mergeRdCandQuotaRegularSmallBlk > maxCandNum
    || m_mergeRdCandQuotaSubBlk < 0 || m_mergeRdCandQuotaSubBlk > maxCandNum
    || m_mergeRdCandQuotaCiip < 0 || m_mergeRdCandQuotaCiip > maxCandNum
    || m_mergeRdCandQuotaGpm < 0 || m_mergeRdCandQuotaGpm > maxCandNum,
    "MaxMergeRdCandNumReguar, MaxMergeRdCandNumReguarSmallBlk, MaxMergeRdCandNumSubBlk, MaxMergeRdCandNumCiip, and MaxMergeRdCandNumGpm must be between 0 and 15, inclusive");
  if ( m_Affine == 0 )
  {
    m_maxNumAffineMergeCand = m_sbTmvpEnableFlag ? 1 : 0;
    if (m_PROF) msg(WARNING, "PROF is forcefully disabled when Affine is off \n");
    m_PROF = false;
  }

  xConfirmPara(m_mtsMode < 0 || m_mtsMode > 4, "MTS must in the range 0..4");
  xConfirmPara( m_MTSIntraMaxCand < 0 || m_MTSIntraMaxCand > 5, "m_MTSIntraMaxCand must be greater than 0 and smaller than 6" );
  xConfirmPara( m_MTSInterMaxCand < 0 || m_MTSInterMaxCand > 5, "m_MTSInterMaxCand must be greater than 0 and smaller than 6" );
  xConfirmPara(m_mtsMode != 0 && m_mtsImplicitIntra != 0, "MTSImplicit may be enabled only when MTS is 0");

  if (m_useBDPCM)
  {
    xConfirmPara(!m_useTransformSkip, "BDPCM cannot be used when transform skip is disabled.");
  }

  if (m_tsrcRicePresentFlag)
  {
    xConfirmPara(!m_useTransformSkip, "TSRCRicePresent cannot be enabled when transform skip is disabled.");
  }

  if (!m_alf)
  {
    xConfirmPara( m_ccalf, "CCALF cannot be enabled when ALF is disabled" );
  }

  if (m_maxNumAlfAps == 0)
  {
    xConfirmPara(m_ccalf, "CCALF cannot be enabled when ALF APS is disabled");
  }

  xConfirmPara(m_sourceWidth % SPS::getWinUnitX(m_chromaFormatIdc) != 0,
               "Picture width must be an integer multiple of the specified chroma subsampling");
  xConfirmPara(m_sourceHeight % SPS::getWinUnitY(m_chromaFormatIdc) != 0,
               "Picture height must be an integer multiple of the specified chroma subsampling");

  xConfirmPara(m_sourcePadding[0] % SPS::getWinUnitX(m_chromaFormatIdc) != 0,
               "Horizontal padding must be an integer multiple of the specified chroma subsampling");
  xConfirmPara(m_sourcePadding[1] % SPS::getWinUnitY(m_chromaFormatIdc) != 0,
               "Vertical padding must be an integer multiple of the specified chroma subsampling");

  xConfirmPara(m_confWinLeft % SPS::getWinUnitX(m_chromaFormatIdc) != 0,
               "Left conformance window offset must be an integer multiple of the specified chroma subsampling");
  xConfirmPara(m_confWinRight % SPS::getWinUnitX(m_chromaFormatIdc) != 0,
               "Right conformance window offset must be an integer multiple of the specified chroma subsampling");
  xConfirmPara(m_confWinTop % SPS::getWinUnitY(m_chromaFormatIdc) != 0,
               "Top conformance window offset must be an integer multiple of the specified chroma subsampling");
  xConfirmPara(m_confWinBottom % SPS::getWinUnitY(m_chromaFormatIdc) != 0,
               "Bottom conformance window offset must be an integer multiple of the specified chroma subsampling");
  xConfirmPara(m_explicitScalingWindowEnabled && (m_scalingRatioHor != 1.0 || m_scalingRatioVer != 1.0 || m_gopBasedRPREnabledFlag), "ScalingWindow cannot be enabled when GOPBasedRPR is enabled");
  xConfirmPara(m_scalWinLeft    % SPS::getWinUnitX(m_chromaFormatIdc) != 0, "Left scaling window offset must be an integer multiple of the specified chroma subsampling");
  xConfirmPara(m_scalWinRight   % SPS::getWinUnitX(m_chromaFormatIdc) != 0, "Right scaling window offset must be an integer multiple of the specified chroma subsampling");
  xConfirmPara(m_scalWinTop     % SPS::getWinUnitY(m_chromaFormatIdc) != 0, "Top scaling window offset must be an integer multiple of the specified chroma subsampling");
  xConfirmPara(m_scalWinBottom  % SPS::getWinUnitY(m_chromaFormatIdc) != 0, "Bottom scaling window offset must be an integer multiple of the specified chroma subsampling");
  xConfirmPara((m_scalWinLeft < -m_sourceWidth * 15) || (m_scalWinLeft >= m_sourceWidth),
               "The values of SubWidthC * pps_scaling_win_left_offset shall be greater than or equal to -pps_pic_width_in_luma_samples * 15 and less than pps_pic_width_in_luma_samples");
  xConfirmPara((m_scalWinRight < -m_sourceWidth * 15) || (m_scalWinRight >= m_sourceWidth),
               "The values of SubWidthC * pps_scaling_win_right_offset shall be greater than or equal to -pps_pic_width_in_luma_samples * 15 and less than pps_pic_width_in_luma_samples");
  xConfirmPara((m_scalWinTop < -m_sourceHeight * 15) || (m_scalWinTop >= m_sourceHeight),
               "The values of SubHeightC * pps_scaling_win_top_offset shall be greater than or equal to -pps_pic_height_in_luma_samples * 15 and less than pps_pic_height_in_luma_samples");
  xConfirmPara((m_scalWinBottom < -m_sourceHeight * 15) || (m_scalWinBottom >= m_sourceHeight),
               "The values of SubHeightC * pps_scaling_win_bottom_offset shall be greater than or equal to -pps_pic_height_in_luma_samples * 15 and less than pps_pic_height_in_luma_samples");
  xConfirmPara(((m_scalWinLeft+m_scalWinRight) < -m_sourceWidth * 15) || ((m_scalWinLeft+m_scalWinRight) >= m_sourceWidth),
               "The values of SubWidthC * (pps_scaling_win_left_offset + pps_scaling_win_right_offset) shall be greater than or equal to -pps_pic_width_in_luma_samples * 15 and less than pps_pic_width_in_luma_samples");
  xConfirmPara(((m_scalWinTop+m_scalWinBottom) < -m_sourceHeight * 15) || ((m_scalWinTop+m_scalWinBottom) >= m_sourceHeight),
               "The values of SubHeightC * (pps_scaling_win_top_offset + pps_scaling_win_bottom_offset) shall be greater than or equal to -pps_pic_height_in_luma_samples * 15 and less than pps_pic_height_in_luma_samples");


  // max CU width and height should be power of 2
  uint32_t ui = m_maxCuWidth;
  while(ui)
  {
    ui >>= 1;
    if( (ui & 1) == 1)
    {
      xConfirmPara( ui != 1 , "Width should be 2^n");
    }
  }
  ui = m_maxCuHeight;
  while(ui)
  {
    ui >>= 1;
    if( (ui & 1) == 1)
    {
      xConfirmPara( ui != 1 , "Height should be 2^n");
    }
  }

  /* if this is an intra-only sequence, ie IntraPeriod=1, don't verify the GOP structure
   * This permits the ability to omit a GOP structure specification */
  if (m_intraPeriod == 1 && m_GOPList[0].m_POC == -1)
  {
    m_GOPList[0] = GOPEntry();
    m_GOPList[0].m_QPFactor = 1;
    m_GOPList[0].m_betaOffsetDiv2 = 0;
    m_GOPList[0].m_tcOffsetDiv2 = 0;
    m_GOPList[0].m_CbBetaOffsetDiv2 = 0;
    m_GOPList[0].m_CbTcOffsetDiv2 = 0;
    m_GOPList[0].m_CrBetaOffsetDiv2 = 0;
    m_GOPList[0].m_CrTcOffsetDiv2 = 0;
    m_GOPList[0].m_POC = 1;
    m_RPLList0[0] = RPLEntry();
    m_RPLList1[0] = RPLEntry();
    m_RPLList0[0].m_POC = m_RPLList1[0].m_POC = 1;
    m_RPLList0[0].m_numRefPicsActive = 4;
    m_GOPList[0].m_numRefPicsActive0 = 4;
  }
  else
  {
    xConfirmPara( m_intraOnlyConstraintFlag, "IntraOnlyConstraintFlag cannot be 1 for inter sequences");
  }

  int multipleFactor = m_compositeRefEnabled ? 2 : 1;
  bool verifiedGOP=false;
  bool errorGOP=false;
  int checkGOP=1;
  static_vector<int, MAX_NUM_REF_PICS + 1> refList;
  refList.push_back(0);
  if(m_isField)
  {
    refList.push_back(1);
  }
  bool isOK[MAX_GOP];
  for(int i=0; i<MAX_GOP; i++)
  {
    isOK[i]=false;
  }
  int numOK=0;
  xConfirmPara(m_intraPeriod >= 0 && (m_intraPeriod % m_gopSize != 0),
               "Intra period must be a multiple of GOPSize, or -1");

  for (int i = 0; i < m_gopSize; i++)
  {
    if (m_GOPList[i].m_POC == m_gopSize * multipleFactor)
    {
      xConfirmPara( m_GOPList[i].m_temporalId!=0 , "The last frame in each GOP must have temporal ID = 0 " );
    }
  }

  if ((m_intraPeriod != 1) && !m_deblockingFilterOffsetInPPS && (!m_deblockingFilterDisable))
  {
    for (int i = 0; i < m_gopSize; i++)
    {
      xConfirmPara( (m_GOPList[i].m_betaOffsetDiv2 + m_deblockingFilterBetaOffsetDiv2) < -12 || (m_GOPList[i].m_betaOffsetDiv2 + m_deblockingFilterBetaOffsetDiv2) > 12, "Loop Filter Beta Offset div. 2 for one of the GOP entries exceeds supported range (-12 to 12)" );
      xConfirmPara( (m_GOPList[i].m_tcOffsetDiv2 + m_deblockingFilterTcOffsetDiv2) < -12 || (m_GOPList[i].m_tcOffsetDiv2 + m_deblockingFilterTcOffsetDiv2) > 12, "Loop Filter Tc Offset div. 2 for one of the GOP entries exceeds supported range (-12 to 12)" );
      xConfirmPara( (m_GOPList[i].m_CbBetaOffsetDiv2 + m_deblockingFilterCbBetaOffsetDiv2) < -12 || (m_GOPList[i].m_CbBetaOffsetDiv2 + m_deblockingFilterCbBetaOffsetDiv2) > 12, "Loop Filter Beta Offset div. 2 for one of the GOP entries exceeds supported range (-12 to 12)" );
      xConfirmPara( (m_GOPList[i].m_CbTcOffsetDiv2 + m_deblockingFilterCbTcOffsetDiv2) < -12 || (m_GOPList[i].m_CbTcOffsetDiv2 + m_deblockingFilterCbTcOffsetDiv2) > 12, "Loop Filter Tc Offset div. 2 for one of the GOP entries exceeds supported range (-12 to 12)" );
      xConfirmPara( (m_GOPList[i].m_CrBetaOffsetDiv2 + m_deblockingFilterCrBetaOffsetDiv2) < -12 || (m_GOPList[i].m_CrBetaOffsetDiv2 + m_deblockingFilterCrBetaOffsetDiv2) > 12, "Loop Filter Beta Offset div. 2 for one of the GOP entries exceeds supported range (-12 to 12)" );
      xConfirmPara( (m_GOPList[i].m_CrTcOffsetDiv2 + m_deblockingFilterCrTcOffsetDiv2) < -12 || (m_GOPList[i].m_CrTcOffsetDiv2 + m_deblockingFilterCrTcOffsetDiv2) > 12, "Loop Filter Tc Offset div. 2 for one of the GOP entries exceeds supported range (-12 to 12)" );
    }
  }

#if W0038_CQP_ADJ
  for (int i = 0; i < m_gopSize; i++)
  {
    xConfirmPara( abs(m_GOPList[i].m_CbQPoffset               ) > 12, "Cb QP Offset for one of the GOP entries exceeds supported range (-12 to 12)" );
    xConfirmPara( abs(m_GOPList[i].m_CbQPoffset + m_cbQpOffset) > 12, "Cb QP Offset for one of the GOP entries, when combined with the PPS Cb offset, exceeds supported range (-12 to 12)" );
    xConfirmPara( abs(m_GOPList[i].m_CrQPoffset               ) > 12, "Cr QP Offset for one of the GOP entries exceeds supported range (-12 to 12)" );
    xConfirmPara( abs(m_GOPList[i].m_CrQPoffset + m_crQpOffset) > 12, "Cr QP Offset for one of the GOP entries, when combined with the PPS Cr offset, exceeds supported range (-12 to 12)" );
  }
  xConfirmPara( abs(m_sliceChromaQpOffsetIntraOrPeriodic[0]                 ) > 12, "Intra/periodic Cb QP Offset exceeds supported range (-12 to 12)" );
  xConfirmPara( abs(m_sliceChromaQpOffsetIntraOrPeriodic[0]  + m_cbQpOffset ) > 12, "Intra/periodic Cb QP Offset, when combined with the PPS Cb offset, exceeds supported range (-12 to 12)" );
  xConfirmPara( abs(m_sliceChromaQpOffsetIntraOrPeriodic[1]                 ) > 12, "Intra/periodic Cr QP Offset exceeds supported range (-12 to 12)" );
  xConfirmPara( abs(m_sliceChromaQpOffsetIntraOrPeriodic[1]  + m_crQpOffset ) > 12, "Intra/periodic Cr QP Offset, when combined with the PPS Cr offset, exceeds supported range (-12 to 12)" );
#endif

  xConfirmPara( m_maxSublayers < 1 || m_maxSublayers > 7, "MaxSublayers must be in range [1..7]" );
  xConfirmPara( m_explicitILRP && m_allIndependentLayersFlag, "AllIndependentLayersFlag cannot be 1 when ExplicitILRP is enabled" );
  if (m_explicitILRP)
  {
    for (int i = 0; i < m_predDirectionArray.size(); i++)
    {
      if (m_predDirectionArray[i] != ' ')
      {
        xConfirmPara( m_predDirectionArray[i] != '0', "AllowablePredDirection should be 0 for all temporal layers when ExplicitILRP is enabled" );
      }
    }
  }

  bool nonZeroDeltaPOC_ILRP = false;
  bool usingExplicit_ILRP = false;

  for (int i = 0; m_GOPList[i].m_POC != -1 && i < MAX_GOP; i++)
  {
    for (int j = 0; j < m_GOPList[i].m_numRefPics0; j++)
    {
      nonZeroDeltaPOC_ILRP |= m_RPLList0[i].m_layerRef[j] != -1 && m_RPLList0[i].m_deltaRefPics[j] != 0;
      nonZeroDeltaPOC_ILRP |= m_RPLList1[i].m_layerRef[j] != -1 && m_RPLList1[i].m_deltaRefPics[j] != 0;
      usingExplicit_ILRP |= m_RPLList0[i].m_layerRef[j] != -1 || m_RPLList1[i].m_layerRef[j] != -1;
    }
  }
  xConfirmPara(nonZeroDeltaPOC_ILRP, "For ExplicitILRP, Inter-Layer Reference pictures can only have same POC as current frame (delta POC=0)");
  if (!m_explicitILRP)
  {
    xConfirmPara(usingExplicit_ILRP, "Cannot specify inter-layer reference pictures in GOP entry when ExplicitILRP is disabled.");
  }

  xConfirmPara( m_fastLocalDualTreeMode < 0 || m_fastLocalDualTreeMode > 2, "FastLocalDualTreeMode must be in range [0..2]" );

  xConfirmPara( m_fastAdaptCostPredMode < 0 || m_fastAdaptCostPredMode > 2, "FastAdaptCostPredMode must be in range [0..2]" );

  int extraRPLs = 0;
  bool hasFutureRef = false;
  //start looping through frames in coding order until we can verify that the GOP structure is correct.
  while (!verifiedGOP && !errorGOP)
  {
    int       rplIdx = (checkGOP - 1) % m_gopSize;
    const int curPOC = ((checkGOP - 1) / m_gopSize) * m_gopSize * multipleFactor + m_RPLList0[rplIdx].m_POC;
    if (m_RPLList0[rplIdx].m_POC < 0 || m_RPLList1[rplIdx].m_POC < 0)
    {
      msg(WARNING, "\nError: found fewer Reference Picture Sets than GOPSize\n");
      errorGOP = true;
    }
    else
    {
      //check that all reference pictures are available, or have a POC < 0 meaning they might be available in the next GOP.
      bool beforeI = false;
      for (int i = 0; i < m_RPLList0[rplIdx].m_numRefPics; i++)
      {
        const int refPoc = curPOC - m_RPLList0[rplIdx].m_deltaRefPics[i];
        if (refPoc < 0)
        {
          beforeI = true;
        }
        else
        {
          bool found = false;
          for (const int poc: refList)
          {
            if (poc == refPoc)
            {
              found = true;
              for (int k = 0; k < m_gopSize; k++)
              {
                if (refPoc % (m_gopSize * multipleFactor) == m_RPLList0[k].m_POC % (m_gopSize * multipleFactor))
                {
                  // TODO: check whether equal test is correct
                  if (m_RPLList0[k].m_temporalId == m_RPLList0[rplIdx].m_temporalId)
                  {
                    m_RPLList0[k].m_refPic = true;
                  }
                }
              }
            }
            if (curPOC == refPoc && m_RPLList0[rplIdx].m_layerRef[i]!=-1)
            {
              found = true;
            }
          }
          if (!found)
          {
            msg(WARNING, "\nError: ref pic %d is not available for GOP frame %d\n",
                m_RPLList0[rplIdx].m_deltaRefPics[i], rplIdx + 1);
            errorGOP = true;
          }
        }
      }
      if (!beforeI && !errorGOP)
      {
        //all ref frames were present
        if (!isOK[rplIdx])
        {
          numOK++;
          isOK[rplIdx] = true;
          if (numOK == m_gopSize)
          {
            verifiedGOP = true;
          }
        }
      }
      else
      {
        const int newRplIdx = m_gopSize + extraRPLs;
        CHECK(newRplIdx >= MAX_GOP, "Too many RPLs");

        //create a new RPLEntry for this frame containing all the reference pictures that were available (POC > 0)
        m_RPLList0[newRplIdx] = m_RPLList0[rplIdx];
        m_RPLList1[newRplIdx] = m_RPLList1[rplIdx];
        int newRefs0 = 0;
        int newActiveRefs0    = 0;
        for (int i = 0; i < m_RPLList0[rplIdx].m_numRefPics; i++)
        {
          const int refPoc = curPOC - m_RPLList0[rplIdx].m_deltaRefPics[i];
          if (refPoc >= 0)
          {
            m_RPLList0[newRplIdx].m_deltaRefPics[newRefs0] = m_RPLList0[rplIdx].m_deltaRefPics[i];
            m_RPLList0[newRplIdx].m_layerRef[newRefs0] = m_RPLList0[rplIdx].m_layerRef[i];
            newRefs0++;
            newActiveRefs0 += i < m_RPLList0[rplIdx].m_numRefPicsActive ? 1 : 0;
          }
        }
        int numPrefActiveRefs0 = m_RPLList0[rplIdx].m_numRefPicsActive;

        int newRefs1 = 0;
        int newActiveRefs1 = 0;
        for (int i = 0; i < m_RPLList1[rplIdx].m_numRefPics; i++)
        {
          const int refPoc = curPOC - m_RPLList1[rplIdx].m_deltaRefPics[i];
          if (refPoc >= 0)
          {
            m_RPLList1[m_gopSize + extraRPLs].m_deltaRefPics[newRefs1] = m_RPLList1[rplIdx].m_deltaRefPics[i];
            m_RPLList1[m_gopSize + extraRPLs].m_layerRef[newRefs0] = m_RPLList1[rplIdx].m_layerRef[i];
            newRefs1++;
            newActiveRefs1 += i < m_RPLList1[rplIdx].m_numRefPicsActive ? 1 : 0;
          }
        }
        int numPrefActiveRefs1 = m_RPLList1[rplIdx].m_numRefPicsActive;

        for (int offset = -1; offset>-checkGOP; offset--)
        {
          //step backwards in coding order and include any extra available pictures we might find useful to replace the ones with POC < 0.
          int offGOP = (checkGOP - 1 + offset) % m_gopSize;
          int offPOC = ((checkGOP - 1 + offset) / m_gopSize) * (m_gopSize * multipleFactor) + m_RPLList0[offGOP].m_POC;
          if (offPOC >= 0 && m_RPLList0[offGOP].m_temporalId <= m_RPLList0[rplIdx].m_temporalId)
          {
            bool      newRef      = std::find(refList.begin(), refList.end(), offPOC) != refList.end();
            const int newDeltaPoc = curPOC - offPOC;
            for (int i = 0; i<newRefs0; i++)
            {
              if (m_RPLList0[newRplIdx].m_deltaRefPics[i] == newDeltaPoc)
              {
                newRef = false;
              }
            }
            if (newRef)
            {
              int insertPoint = newActiveRefs0;
              //this picture can be added, find appropriate place in list and insert it.
              if (m_RPLList0[offGOP].m_temporalId == m_RPLList0[rplIdx].m_temporalId)
              {
                m_RPLList0[offGOP].m_refPic = true;
              }
              for (int j = 0; j < newActiveRefs0; j++)
              {
                if (m_RPLList0[newRplIdx].m_deltaRefPics[j] > newDeltaPoc && newDeltaPoc > 0)
                {
                  insertPoint = j;
                  break;
                }
              }
              int prev = newDeltaPoc;
              int prevLayerRef = -1; // inserted picture should not be an inter-layer
              newRefs0++;
              newActiveRefs0++;
              for (int j = insertPoint; j < newRefs0; j++)
              {
                std::swap(prev, m_RPLList0[newRplIdx].m_deltaRefPics[j]);
                std::swap(prevLayerRef, m_RPLList0[newRplIdx].m_layerRef[j]);
              }
            }
          }
          if (newActiveRefs0 >= numPrefActiveRefs0)
          {
            break;
          }
        }

        for (int offset = -1; offset>-checkGOP; offset--)
        {
          //step backwards in coding order and include any extra available pictures we might find useful to replace the ones with POC < 0.
          int offGOP = (checkGOP - 1 + offset) % m_gopSize;
          int offPOC = ((checkGOP - 1 + offset) / m_gopSize) * (m_gopSize * multipleFactor) + m_RPLList1[offGOP].m_POC;
          if (offPOC >= 0 && m_RPLList1[offGOP].m_temporalId <= m_RPLList1[rplIdx].m_temporalId)
          {
            bool      newRef      = std::find(refList.begin(), refList.end(), offPOC) != refList.end();
            const int newDeltaPoc = curPOC - offPOC;
            for (int i = 0; i<newRefs1; i++)
            {
              if (m_RPLList1[newRplIdx].m_deltaRefPics[i] == newDeltaPoc)
              {
                newRef = false;
              }
            }
            if (newRef)
            {
              int insertPoint = newActiveRefs1;
              //this picture can be added, find appropriate place in list and insert it.
              if (m_RPLList1[offGOP].m_temporalId == m_RPLList1[rplIdx].m_temporalId)
              {
                m_RPLList1[offGOP].m_refPic = true;
              }
              for (int j = 0; j < newActiveRefs1; j++)
              {
                if (m_RPLList1[newRplIdx].m_deltaRefPics[j] > newDeltaPoc && newDeltaPoc > 0)
                {
                  insertPoint = j;
                  break;
                }
              }
              int prev = newDeltaPoc;
              int prevLayerRef = -1; // inserted picture should not be an inter-layer
              newRefs1++;
              newActiveRefs1++;
              for (int j = insertPoint; j < newRefs1; j++)
              {
                std::swap(prev, m_RPLList1[newRplIdx].m_deltaRefPics[j]);
                std::swap(prevLayerRef, m_RPLList1[newRplIdx].m_layerRef[j]);
              }
            }
          }
          if (newActiveRefs1 >= numPrefActiveRefs1)
          {
            break;
          }
        }

        m_RPLList0[newRplIdx].m_numRefPics       = newRefs0;
        m_RPLList0[newRplIdx].m_numRefPicsActive = newActiveRefs0;
        m_RPLList1[newRplIdx].m_numRefPics       = newRefs1;
        m_RPLList1[newRplIdx].m_numRefPicsActive = newActiveRefs1;

        rplIdx = newRplIdx;
        extraRPLs++;
      }

      refList.clear();
      for (int i = 0; i < m_RPLList0[rplIdx].m_numRefPics; i++)
      {
        const int refPoc = curPOC - m_RPLList0[rplIdx].m_deltaRefPics[i];
        hasFutureRef |= (m_RPLList0[rplIdx].m_deltaRefPics[i] < 0);
        if (refPoc >= 0)
        {
          refList.push_back(refPoc);
        }
      }
      for (int i = 0; i < m_RPLList1[rplIdx].m_numRefPics; i++)
      {
        const int refPoc = curPOC - m_RPLList1[rplIdx].m_deltaRefPics[i];
        hasFutureRef |= (m_RPLList1[rplIdx].m_deltaRefPics[i] < 0);
        if (refPoc >= 0)
        {
          if (std::find(refList.begin(), refList.end(), refPoc) == refList.end())
          {
            refList.push_back(refPoc);
          }
        }
      }
      refList.push_back(curPOC);
    }
    checkGOP++;
  }
  m_isLowDelay = !hasFutureRef && m_intraPeriod != 1;
  xConfirmPara(errorGOP, "Invalid GOP structure given");

  m_maxTempLayer = 1;

  for (int i = 0; i < m_gopSize; i++)
  {
    if(m_GOPList[i].m_temporalId >= m_maxTempLayer)
    {
      m_maxTempLayer = m_GOPList[i].m_temporalId+1;
    }
    xConfirmPara(m_GOPList[i].m_sliceType!='B' && m_GOPList[i].m_sliceType!='P' && m_GOPList[i].m_sliceType!='I', "Slice type must be equal to B or P or I");
  }
  for(int i=0; i<MAX_TLAYER; i++)
  {
    m_maxNumReorderPics[i] = 0;
    m_maxDecPicBuffering[i] = 1;
  }
  for (int i = 0; i < m_gopSize; i++)
  {

    int numRefPic = m_RPLList0[i].m_numRefPics;
    for (int tmp = 0; tmp < m_RPLList0[i].m_numRefPics; tmp++)
    {
      if (m_RPLList0[i].m_deltaRefPics[tmp]==0 && m_RPLList0[i].m_layerRef[tmp]!=-1)
      {
      	numRefPic--; //Inter-layer ref pic already in DPB for ref layer, do not count it for current layer.
      }
    }
    for (int tmp = 0; tmp < m_RPLList1[i].m_numRefPics; tmp++)
    {
      if (m_RPLList1[i].m_deltaRefPics[tmp]==0 && m_RPLList1[i].m_layerRef[tmp]!=-1) continue; //Inter-layer ref pic already in DPB for ref layer, do not count it for current layer.
      bool notSame = true;
      for (int jj = 0; notSame && jj < m_RPLList0[i].m_numRefPics; jj++)
      {
        if (m_RPLList1[i].m_deltaRefPics[tmp] == m_RPLList0[i].m_deltaRefPics[jj]) notSame = false;
      }
      if (notSame) numRefPic++;
    }
    if (numRefPic + 1 > m_maxDecPicBuffering[m_GOPList[i].m_temporalId])
    {
      m_maxDecPicBuffering[m_GOPList[i].m_temporalId] = numRefPic + 1;
    }
    int highestDecodingNumberWithLowerPOC = 0;
    for (int j = 0; j < m_gopSize; j++)
    {
      if(m_GOPList[j].m_POC <= m_GOPList[i].m_POC)
      {
        highestDecodingNumberWithLowerPOC = j;
      }
    }
    int numReorder = 0;
    for(int j=0; j<highestDecodingNumberWithLowerPOC; j++)
    {
      if(m_GOPList[j].m_temporalId <= m_GOPList[i].m_temporalId &&
        m_GOPList[j].m_POC > m_GOPList[i].m_POC)
      {
        numReorder++;
      }
    }
    if(numReorder > m_maxNumReorderPics[m_GOPList[i].m_temporalId])
    {
      m_maxNumReorderPics[m_GOPList[i].m_temporalId] = numReorder;
    }
  }

  for(int i=0; i<MAX_TLAYER-1; i++)
  {
    // a lower layer can not have higher value of m_maxNumReorderPics than a higher layer
    if(m_maxNumReorderPics[i+1] < m_maxNumReorderPics[i])
    {
      m_maxNumReorderPics[i+1] = m_maxNumReorderPics[i];
    }
    // the value of dpb_max_num_reorder_pics[ i ] shall be in the range of 0 to max_dec_pic_buffering[ i ] - 1, inclusive
    if(m_maxNumReorderPics[i] > m_maxDecPicBuffering[i] - 1)
    {
      m_maxDecPicBuffering[i] = m_maxNumReorderPics[i] + 1;
    }
    // a lower layer can not have higher value of m_maxDecPicBuffering than a higher layer
    if(m_maxDecPicBuffering[i+1] < m_maxDecPicBuffering[i])
    {
      m_maxDecPicBuffering[i+1] = m_maxDecPicBuffering[i];
    }
  }

  // the value of dpb_max_num_reorder_pics[ i ] shall be in the range of 0 to max_dec_pic_buffering[ i ] -  1, inclusive
  if(m_maxNumReorderPics[MAX_TLAYER-1] > m_maxDecPicBuffering[MAX_TLAYER-1] - 1)
  {
    m_maxDecPicBuffering[MAX_TLAYER-1] = m_maxNumReorderPics[MAX_TLAYER-1] + 1;
  }

  if( m_picPartitionFlag )
  {
    PPS pps;
    uint32_t colIdx, rowIdx;
    uint32_t remSize;

    pps.setPicWidthInLumaSamples( m_sourceWidth );
    pps.setPicHeightInLumaSamples( m_sourceHeight );
    pps.setLog2CtuSize(floorLog2(m_ctuSize));

    // set default tile column if not provided
    if( m_tileColumnWidth.size() == 0 )
    {
      m_tileColumnWidth.push_back( pps.getPicWidthInCtu() );
    }
    // set default tile row if not provided
    if( m_tileRowHeight.size() == 0 )
    {
      m_tileRowHeight.push_back( pps.getPicHeightInCtu() );
    }

    // remove any tile columns that can be specified implicitly
    while( m_tileColumnWidth.size() > 1 && m_tileColumnWidth.end()[-1] == m_tileColumnWidth.end()[-2] )
    {
      m_tileColumnWidth.pop_back();
    }

    // remove any tile rows that can be specified implicitly
    while( m_tileRowHeight.size() > 1 && m_tileRowHeight.end()[-1] == m_tileRowHeight.end()[-2] )
    {
      m_tileRowHeight.pop_back();
    }

    // setup tiles in temporary PPS structure
    remSize = pps.getPicWidthInCtu();
    for( colIdx=0; remSize > 0 && colIdx<m_tileColumnWidth.size(); colIdx++ )
    {
      xConfirmPara(m_tileColumnWidth[ colIdx ] == 0, "Tile column widths cannot be equal to 0");
      m_tileColumnWidth[ colIdx ] = std::min( remSize, m_tileColumnWidth[ colIdx ]);
      pps.addTileColumnWidth( m_tileColumnWidth[ colIdx ] );
      remSize -= m_tileColumnWidth[ colIdx ];
    }
    m_tileColumnWidth.resize( colIdx );
    pps.setNumExpTileColumns( (uint32_t)m_tileColumnWidth.size() );
    remSize = pps.getPicHeightInCtu();
    for( rowIdx=0; remSize > 0 && rowIdx<m_tileRowHeight.size(); rowIdx++ )
    {
      xConfirmPara(m_tileRowHeight[ rowIdx ] == 0, "Tile row heights cannot be equal to 0");
      m_tileRowHeight[ rowIdx ] = std::min( remSize, m_tileRowHeight[ rowIdx ]);
      pps.addTileRowHeight( m_tileRowHeight[ rowIdx ] );
      remSize -= m_tileRowHeight[ rowIdx ];
    }
    m_tileRowHeight.resize( rowIdx );
    pps.setNumExpTileRows( (uint32_t)m_tileRowHeight.size() );
    pps.initTiles();
    xConfirmPara(pps.getNumTileColumns() > getMaxTileColsByLevel( m_level ), "Number of tile columns exceeds maximum number allowed according to specified level");
    xConfirmPara(pps.getNumTileRows()    > getMaxTileRowsByLevel( m_level ), "Number of tile rows exceeds maximum number allowed according to specified level");
    m_numTileCols = pps.getNumTileColumns();
    m_numTileRows = pps.getNumTileRows();

    // rectangular slices
    if( !m_rasterSliceFlag )
    {
      if (!m_singleSlicePerSubPicFlag)
      {
        uint32_t sliceIdx;
        bool     needTileIdxDelta = false;

        // generate slice list for the simplified fixed-rectangular-slice-size config option
        if( m_rectSliceFixedWidth > 0 && m_rectSliceFixedHeight > 0 )
        {
          int tileIdx = 0;
          m_rectSlicePos.clear();
          while( tileIdx < pps.getNumTiles() )
          {
            uint32_t startTileX = tileIdx % pps.getNumTileColumns();
            uint32_t startTileY = tileIdx / pps.getNumTileColumns();
            uint32_t startCtuX  = pps.getTileColumnBd( startTileX );
            uint32_t startCtuY  = pps.getTileRowBd( startTileY );
            uint32_t stopCtuX   = (startTileX + m_rectSliceFixedWidth)  >= pps.getNumTileColumns() ? pps.getPicWidthInCtu() - 1  : pps.getTileColumnBd( startTileX + m_rectSliceFixedWidth ) - 1;
            uint32_t stopCtuY   = (startTileY + m_rectSliceFixedHeight) >= pps.getNumTileRows()    ? pps.getPicHeightInCtu() - 1 : pps.getTileRowBd( startTileY + m_rectSliceFixedHeight ) - 1;
            uint32_t stopTileX  = pps.ctuToTileCol( stopCtuX );
            uint32_t stopTileY  = pps.ctuToTileRow( stopCtuY );

            // add rectangular slice to list
            m_rectSlicePos.push_back( startCtuY * pps.getPicWidthInCtu() + startCtuX );
            m_rectSlicePos.push_back( stopCtuY  * pps.getPicWidthInCtu() + stopCtuX  );

            // get slice size in tiles
            uint32_t sliceWidth  = stopTileX - startTileX + 1;
            uint32_t sliceHeight = stopTileY - startTileY + 1;

            // move to next tile in raster scan order
            tileIdx += sliceWidth;
            if( tileIdx % pps.getNumTileColumns() == 0 )
            {
              tileIdx += (sliceHeight - 1) * pps.getNumTileColumns();
            }
          }
        }

        xConfirmPara( m_rectSlicePos.size() & 1, "Odd number of rectangular slice positions provided. Rectangular slice positions must be specified in pairs of (top-left / bottom-right) raster-scan CTU addresses.");

        // set default slice size if not provided
        if( m_rectSlicePos.size() == 0 )
        {
          m_rectSlicePos.push_back( 0 );
          m_rectSlicePos.push_back( pps.getPicWidthInCtu() * pps.getPicHeightInCtu() - 1 );
        }
        pps.setNumSlicesInPic( (uint32_t)(m_rectSlicePos.size() >> 1) );
        xConfirmPara(pps.getNumSlicesInPic() > getMaxSlicesByLevel( m_level ), "Number of rectangular slices exceeds maximum number allowed according to specified level");
        pps.initRectSlices();

        // set slice parameters from CTU addresses
        for( sliceIdx = 0; sliceIdx < pps.getNumSlicesInPic(); sliceIdx++ )
        {
          xConfirmPara( m_rectSlicePos[2*sliceIdx]     >= pps.getPicWidthInCtu() * pps.getPicHeightInCtu(), "Rectangular slice position exceeds total number of CTU in picture.");
          xConfirmPara( m_rectSlicePos[2*sliceIdx + 1] >= pps.getPicWidthInCtu() * pps.getPicHeightInCtu(), "Rectangular slice position exceeds total number of CTU in picture.");

          // map raster scan CTU address to X/Y position
          uint32_t startCtuX = m_rectSlicePos[2*sliceIdx]     % pps.getPicWidthInCtu();
          uint32_t startCtuY = m_rectSlicePos[2*sliceIdx]     / pps.getPicWidthInCtu();
          uint32_t stopCtuX  = m_rectSlicePos[2*sliceIdx + 1] % pps.getPicWidthInCtu();
          uint32_t stopCtuY  = m_rectSlicePos[2*sliceIdx + 1] / pps.getPicWidthInCtu();

          // get corresponding tile index
          uint32_t startTileX = pps.ctuToTileCol( startCtuX );
          uint32_t startTileY = pps.ctuToTileRow( startCtuY );
          uint32_t stopTileX  = pps.ctuToTileCol( stopCtuX );
          uint32_t stopTileY  = pps.ctuToTileRow( stopCtuY );
          uint32_t tileIdx    = startTileY * pps.getNumTileColumns() + startTileX;

          // get slice size in tiles
          uint32_t sliceWidth  = stopTileX - startTileX + 1;
          uint32_t sliceHeight = stopTileY - startTileY + 1;

          // check for slice / tile alignment
          xConfirmPara( startCtuX != pps.getTileColumnBd( startTileX ), "Rectangular slice position does not align with a left tile edge.");
          xConfirmPara( stopCtuX  != (pps.getTileColumnBd( stopTileX + 1 ) - 1), "Rectangular slice position does not align with a right tile edge.");
          if( sliceWidth > 1 || sliceHeight > 1 )
          {
            xConfirmPara( startCtuY != pps.getTileRowBd( startTileY ), "Rectangular slice position does not align with a top tile edge.");
            xConfirmPara( stopCtuY  != (pps.getTileRowBd( stopTileY + 1 ) - 1), "Rectangular slice position does not align with a bottom tile edge.");
          }

          // set slice size and tile index
          pps.setSliceWidthInTiles( sliceIdx, sliceWidth );
          pps.setSliceHeightInTiles( sliceIdx, sliceHeight );
          pps.setSliceTileIdx( sliceIdx, tileIdx );
          if( sliceIdx > 0 && !needTileIdxDelta )
          {
            uint32_t lastTileIdx = pps.getSliceTileIdx( sliceIdx-1 );
            lastTileIdx += pps.getSliceWidthInTiles( sliceIdx-1 );
            if( lastTileIdx % pps.getNumTileColumns() == 0)
            {
              lastTileIdx += (pps.getSliceHeightInTiles( sliceIdx-1 ) - 1) * pps.getNumTileColumns();
            }
            if( lastTileIdx != tileIdx )
            {
              needTileIdxDelta = true;
            }
          }

          // special case for multiple slices within a single tile
          if( sliceWidth == 1 && sliceHeight == 1 )
          {
            uint32_t firstSliceIdx = sliceIdx;
            uint32_t numSlicesInTile = 1;
            pps.setSliceHeightInCtu( sliceIdx, stopCtuY - startCtuY + 1 );

            while( sliceIdx < pps.getNumSlicesInPic()-1 )
            {
              uint32_t nextTileIdx;
              startCtuX   = m_rectSlicePos[2*(sliceIdx+1)]     % pps.getPicWidthInCtu();
              startCtuY   = m_rectSlicePos[2*(sliceIdx+1)]     / pps.getPicWidthInCtu();
              stopCtuX    = m_rectSlicePos[2*(sliceIdx+1) + 1] % pps.getPicWidthInCtu();
              stopCtuY    = m_rectSlicePos[2*(sliceIdx+1) + 1] / pps.getPicWidthInCtu();
              startTileX  = pps.ctuToTileCol( startCtuX );
              startTileY  = pps.ctuToTileRow( startCtuY );
              stopTileX   = pps.ctuToTileCol( stopCtuX );
              stopTileY   = pps.ctuToTileRow( stopCtuY );
              nextTileIdx = startTileY * pps.getNumTileColumns() + startTileX;
              sliceWidth  = stopTileX - startTileX + 1;
              sliceHeight = stopTileY - startTileY + 1;
              if(nextTileIdx != tileIdx || sliceWidth != 1 || sliceHeight != 1)
              {
                break;
              }
              numSlicesInTile++;
              sliceIdx++;
              pps.setSliceWidthInTiles( sliceIdx, 1 );
              pps.setSliceHeightInTiles( sliceIdx, 1 );
              pps.setSliceTileIdx( sliceIdx, tileIdx );
              pps.setSliceHeightInCtu( sliceIdx, stopCtuY - startCtuY + 1 );
            }
            pps.setNumSlicesInTile( firstSliceIdx, numSlicesInTile );
          }
        }
        pps.setTileIdxDeltaPresentFlag( needTileIdxDelta );
        m_tileIdxDeltaPresentFlag = needTileIdxDelta;

        // check rectangular slice mapping and full picture CTU coverage
        pps.initRectSliceMap(nullptr);

        // store rectangular slice parameters from temporary PPS structure
        m_numSlicesInPic = pps.getNumSlicesInPic();
        m_rectSlices.resize( pps.getNumSlicesInPic() );
        for( sliceIdx = 0; sliceIdx < pps.getNumSlicesInPic(); sliceIdx++ )
        {
          m_rectSlices[sliceIdx].setSliceWidthInTiles( pps.getSliceWidthInTiles(sliceIdx) );
          m_rectSlices[sliceIdx].setSliceHeightInTiles( pps.getSliceHeightInTiles(sliceIdx) );
          m_rectSlices[sliceIdx].setNumSlicesInTile( pps.getNumSlicesInTile(sliceIdx) );
          m_rectSlices[sliceIdx].setSliceHeightInCtu( pps.getSliceHeightInCtu(sliceIdx) );
          m_rectSlices[sliceIdx].setTileIdx( pps.getSliceTileIdx(sliceIdx) );
        }
      }
    }
    // raster-scan slices
    else
    {
      uint32_t listIdx = 0;
      uint32_t remTiles = pps.getNumTiles();

      // set default slice size if not provided
      if( m_rasterSliceSize.size() == 0 )
      {
        m_rasterSliceSize.push_back( remTiles );
      }

      // set raster slice sizes
      while( remTiles > 0 )
      {
        // truncate if size exceeds number of remaining tiles
        if( listIdx < m_rasterSliceSize.size() )
        {
          m_rasterSliceSize[listIdx] = std::min( remTiles, m_rasterSliceSize[listIdx] );
          remTiles -= m_rasterSliceSize[listIdx];
        }
        // replicate last size uniformly as needed to cover the remainder of the picture
        else
        {
          m_rasterSliceSize.push_back( std::min( remTiles, m_rasterSliceSize.back() ) );
          remTiles -= m_rasterSliceSize.back();
        }
        listIdx++;
      }
      // shrink list if too many sizes were provided
      m_rasterSliceSize.resize( listIdx );

      m_numSlicesInPic = (uint32_t)m_rasterSliceSize.size();
      xConfirmPara(m_rasterSliceSize.size() > getMaxSlicesByLevel( m_level ), "Number of raster-scan slices exceeds maximum number allowed according to specified level");
    }
  }
  else
  {
    m_numTileCols = 1;
    m_numTileRows = 1;
    m_numSlicesInPic = 1;
  }

  if ((m_MCTSEncConstraint) && (!m_disableLFCrossTileBoundaryFlag))
  {
    printf("Warning: Constrained Encoding for Motion Constrained Tile Sets (MCTS) is enabled. Disabling filtering across tile boundaries!\n");
    m_disableLFCrossTileBoundaryFlag = true;
  }
  if ((m_MCTSEncConstraint) && (m_TMVPModeId))
  {
    printf("Warning: Constrained Encoding for Motion Constrained Tile Sets (MCTS) is enabled. Disabling TMVP!\n");
    m_TMVPModeId = 0;
  }

  if ((m_MCTSEncConstraint) && ( m_alf ))
  {
    printf("Warning: Constrained Encoding for Motion Constrained Tile Sets (MCTS) is enabled. Disabling ALF!\n");
    m_alf = false;
  }
  if( ( m_MCTSEncConstraint ) && ( m_BIO ) )
  {
    printf( "Warning: Constrained Encoding for Motion Constrained Tile Sets (MCTS) is enabled. Disabling BIO!\n" );
    m_BIO = false;
  }

  xConfirmPara( m_sariAspectRatioIdc < 0 || m_sariAspectRatioIdc > 255, "SEISARISampleAspectRatioIdc must be in the range of 0 to 255");

  if (m_rcEnableRateControl)
  {
    if (m_rcForceIntraQp)
    {
      if (m_rcInitialQp == 0)
      {
        msg( WARNING, "\nInitial QP for rate control is not specified. Reset not to use force intra QP!" );
        m_rcForceIntraQp = false;
      }
    }
    xConfirmPara( m_uiDeltaQpRD > 0, "Rate control cannot be used together with slice level multiple-QP optimization!\n" );
    if (m_rcCpbSaturationEnabled && m_level != Level::NONE && m_profile != Profile::NONE)
    {
      uint32_t uiLevelIdx = (m_level / 16) * 4 + (uint32_t)((m_level % 16) / 3);
      xConfirmPara(m_rcCpbSize > g_uiMaxCpbSize[m_levelTier][uiLevelIdx],
                   "RCCpbSize should be smaller than or equal to Max CPB size according to tier and level");
      xConfirmPara(m_rcInitialCpbFullness > 1, "RCInitialCpbFullness should be smaller than or equal to 1");
    }
  }
  else
  {
    xConfirmPara(m_rcCpbSaturationEnabled != 0, "Target bits saturation cannot be processed without Rate control");
  }

  if (m_framePackingSEIEnabled)
  {
    xConfirmPara(m_framePackingSEIType < 3 || m_framePackingSEIType > 5 , "SEIFramePackingType must be in rage 3 to 5");
  }

  if (m_doSEIEnabled)
  {
    xConfirmPara(m_doSEITransformType < 0 || m_doSEITransformType > 7, "SEIDisplayOrientationTransformType must be in rage 0 to 7");
  }

  if( m_erpSEIEnabled && !m_erpSEICancelFlag )
  {
    xConfirmPara( m_erpSEIGuardBandType < 0 || m_erpSEIGuardBandType > 8, "SEIEquirectangularprojectionGuardBandType must be in the range of 0 to 7");
    xConfirmPara(
      (m_chromaFormatIdc == ChromaFormat::_420 || m_chromaFormatIdc == ChromaFormat::_422)
        && (m_erpSEILeftGuardBandWidth % 2 == 1),
      "SEIEquirectangularprojectionLeftGuardBandWidth must be an even number for 4:2:0 or 4:2:2 chroma format");
    xConfirmPara(
      (m_chromaFormatIdc == ChromaFormat::_420 || m_chromaFormatIdc == ChromaFormat::_422)
        && (m_erpSEIRightGuardBandWidth % 2 == 1),
      "SEIEquirectangularprojectionRightGuardBandWidth must be an even number for 4:2:0 or 4:2:2 chroma format");
  }

  if( m_sphereRotationSEIEnabled && !m_sphereRotationSEICancelFlag )
  {
    xConfirmPara( m_sphereRotationSEIYaw  < -(180<<16) || m_sphereRotationSEIYaw > (180<<16)-1, "SEISphereRotationYaw must be in the range of -11 796 480 to 11 796 479");
    xConfirmPara( m_sphereRotationSEIPitch < -(90<<16) || m_sphereRotationSEIYaw > (90<<16),    "SEISphereRotationPitch must be in the range of -5 898 240 to 5 898 240");
    xConfirmPara( m_sphereRotationSEIRoll < -(180<<16) || m_sphereRotationSEIYaw > (180<<16)-1, "SEISphereRotationRoll must be in the range of -11 796 480 to 11 796 479");
  }

  if ( m_omniViewportSEIEnabled && !m_omniViewportSEICancelFlag )
  {
    xConfirmPara( m_omniViewportSEIId < 0 || m_omniViewportSEIId > 1023, "SEIomniViewportId must be in the range of 0 to 1023");
    xConfirmPara( m_omniViewportSEICntMinus1 < 0 || m_omniViewportSEICntMinus1 > 15, "SEIomniViewportCntMinus1 must be in the range of 0 to 15");
    for ( uint32_t i=0; i<=m_omniViewportSEICntMinus1; i++ )
    {
      xConfirmPara( m_omniViewportSEIAzimuthCentre[i] < -(180<<16)  || m_omniViewportSEIAzimuthCentre[i] > (180<<16)-1, "SEIOmniViewportAzimuthCentre must be in the range of -11 796 480 to 11 796 479");
      xConfirmPara( m_omniViewportSEIElevationCentre[i] < -(90<<16) || m_omniViewportSEIElevationCentre[i] > (90<<16),  "SEIOmniViewportSEIElevationCentre must be in the range of -5 898 240 to 5 898 240");
      xConfirmPara( m_omniViewportSEITiltCentre[i] < -(180<<16)     || m_omniViewportSEITiltCentre[i] > (180<<16)-1,    "SEIOmniViewportTiltCentre must be in the range of -11 796 480 to 11 796 479");
      xConfirmPara( m_omniViewportSEIHorRange[i] < 1 || m_omniViewportSEIHorRange[i] > (360<<16), "SEIOmniViewportHorRange must be in the range of 1 to 360*2^16");
      xConfirmPara( m_omniViewportSEIVerRange[i] < 1 || m_omniViewportSEIVerRange[i] > (180<<16), "SEIOmniViewportVerRange must be in the range of 1 to 180*2^16");
    }
  }

  if (m_gcmpSEIEnabled && !m_gcmpSEICancelFlag)
  {
    xConfirmPara( m_gcmpSEIMappingFunctionType < 0 || m_gcmpSEIMappingFunctionType > 2, "SEIGcmpMappingFunctionType must be in the range of 0 to 2");
    int numFace = m_gcmpSEIPackingType == 4 || m_gcmpSEIPackingType == 5 ? 5 : 6;
    for ( int i = 0; i < numFace; i++ )
    {
      xConfirmPara( m_gcmpSEIFaceIndex[i] < 0 || m_gcmpSEIFaceIndex[i] > 5,       "SEIGcmpFaceIndex must be in the range of 0 to 5");
      xConfirmPara( m_gcmpSEIFaceRotation[i] < 0 || m_gcmpSEIFaceRotation[i] > 3, "SEIGcmpFaceRotation must be in the range of 0 to 3");
      if (m_gcmpSEIMappingFunctionType == 2)
      {
        xConfirmPara( m_gcmpSEIFunctionCoeffU[i] <= 0.0 || m_gcmpSEIFunctionCoeffU[i] > 1.0, "SEIGcmpFunctionCoeffU must be in the range (0, 1]");
        xConfirmPara( m_gcmpSEIFunctionCoeffV[i] <= 0.0 || m_gcmpSEIFunctionCoeffV[i] > 1.0, "SEIGcmpFunctionCoeffV must be in the range (0, 1]");
      }
      if (i != 2 && (m_gcmpSEIPackingType == 4 || m_gcmpSEIPackingType == 5))
      {
        if (m_gcmpSEIFaceIndex[2] == 0 || m_gcmpSEIFaceIndex[2] == 1)
        {
          xConfirmPara( m_gcmpSEIFaceIndex[i] == 0 || m_gcmpSEIFaceIndex[i] == 1, "SEIGcmpFaceIndex[i] must be in the range of 2 to 5 for i equal to 0, 1, 3, or 4 when SEIGcmpFaceIndex[2] is equal to 0 or 1");
          if (m_gcmpSEIPackingType == 4)
          {
            xConfirmPara( m_gcmpSEIFaceRotation[i] != 0 && m_gcmpSEIFaceRotation[i] != 2, "SEIGcmpFaceRotation[i] must be 0 or 2 for i equal to 0, 1, 3, or 4 when SEIGcmpFaceIndex[2] is equal to 0 or 1");
          }
          else
          {
            xConfirmPara( m_gcmpSEIFaceRotation[i] != 1 && m_gcmpSEIFaceRotation[i] != 3, "SEIGcmpFaceRotation[i] must be 1 or 3 for i equal to 0, 1, 3, or 4 when SEIGcmpFaceIndex[2] is equal to 0 or 1");
          }
        }
        else if (m_gcmpSEIFaceIndex[2] == 2 || m_gcmpSEIFaceIndex[2] == 3)
        {
          xConfirmPara( m_gcmpSEIFaceIndex[i] == 2 || m_gcmpSEIFaceIndex[i] == 3, "SEIGcmpFaceIndex[i] must be 0, 1, 4 or 5 for i equal to 0, 1, 3, or 4 when SEIGcmpFaceIndex[2] is equal to 2 or 3");
          if (m_gcmpSEIPackingType == 4)
          {
            if (m_gcmpSEIFaceIndex[i] == 1)
            {
              xConfirmPara( m_gcmpSEIFaceRotation[i] != 0 && m_gcmpSEIFaceRotation[i] != 2, "SEIGcmpFaceRotation[i] must be 0 or 2 when SEIGcmpFaceIndex[2] is equal to 2 or 3 and SEIGcmpFaceIndex[i] is equal to 1");
            }
            else
            {
              xConfirmPara( m_gcmpSEIFaceRotation[i] != 1 && m_gcmpSEIFaceRotation[i] != 3, "SEIGcmpFaceRotation[i] must be 1 or 3 when SEIGcmpFaceIndex[2] is equal to 2 or 3 and SEIGcmpFaceIndex[i] is equal to 0, 4 or 5");
            }
          }
          else
          {
            if (m_gcmpSEIFaceIndex[i] == 1)
            {
              xConfirmPara( m_gcmpSEIFaceRotation[i] != 1 && m_gcmpSEIFaceRotation[i] != 3, "SEIGcmpFaceRotation[i] must be 1 or 3 when SEIGcmpFaceIndex[2] is equal to 2 or 3 and SEIGcmpFaceIndex[i] is equal to 1");
            }
            else
            {
              xConfirmPara( m_gcmpSEIFaceRotation[i] != 0 && m_gcmpSEIFaceRotation[i] != 2, "SEIGcmpFaceRotation[i] must be 0 or 2 when SEIGcmpFaceIndex[2] is equal to 2 or 3 and SEIGcmpFaceIndex[i] is equal to 0, 4 or 5");
            }
          }
        }
        else if (m_gcmpSEIFaceIndex[2] == 4 || m_gcmpSEIFaceIndex[2] == 5)
        {
          xConfirmPara( m_gcmpSEIFaceIndex[i] == 4 || m_gcmpSEIFaceIndex[i] == 5, "SEIGcmpFaceIndex[i] must be in the range of 0 to 3 for i equal to 0, 1, 3, or 4 when SEIGcmpFaceIndex[2] is equal to 4 or 5");
          if (m_gcmpSEIPackingType == 4)
          {
            if (m_gcmpSEIFaceIndex[i] == 0)
            {
              xConfirmPara( m_gcmpSEIFaceRotation[i] != 0 && m_gcmpSEIFaceRotation[i] != 2, "SEIGcmpFaceRotation[i] must be 0 or 2 when SEIGcmpFaceIndex[2] is equal to 4 or 5 and SEIGcmpFaceIndex[i] is equal to 0");
            }
            else
            {
              xConfirmPara( m_gcmpSEIFaceRotation[i] != 1 && m_gcmpSEIFaceRotation[i] != 3, "SEIGcmpFaceRotation[i] must be 1 or 3 when SEIGcmpFaceIndex[2] is equal to 4 or 5 and SEIGcmpFaceIndex[i] is equal to 1, 2 or 3");
            }
          }
          else
          {
            if (m_gcmpSEIFaceIndex[i] == 0)
            {
              xConfirmPara( m_gcmpSEIFaceRotation[i] != 1 && m_gcmpSEIFaceRotation[i] != 3, "SEIGcmpFaceRotation[i] must be 1 or 3 when SEIGcmpFaceIndex[2] is equal to 4 or 5 and SEIGcmpFaceIndex[i] is equal to 0");
            }
            else
            {
              xConfirmPara( m_gcmpSEIFaceRotation[i] != 0 && m_gcmpSEIFaceRotation[i] != 2, "SEIGcmpFaceRotation[i] must be 0 or 2 when SEIGcmpFaceIndex[2] is equal to 4 or 5 and SEIGcmpFaceIndex[i] is equal to 1, 2 or 3");
            }
          }
        }
      }
    }
    if (m_gcmpSEIGuardBandFlag)
    {
      xConfirmPara( m_gcmpSEIGuardBandSamplesMinus1 < 0 || m_gcmpSEIGuardBandSamplesMinus1 > 15, "SEIGcmpGuardBandSamplesMinus1 must be in the range of 0 to 15");
    }
  }

  if (m_siiSEIEnabled && m_ShutterFilterEnable)
  {
    xConfirmPara(m_maxTempLayer == 1 || m_maxDecPicBuffering[0] == 1,"Shutter Interval SEI message processing is disabled for single TempLayer and single frame in DPB\n");
  }

  if (m_nnPostFilterSEICharacteristicsEnabled)
  {
    for (int i = 0; i < m_nnPostFilterSEICharacteristicsNumFilters; i++)
    {
      xConfirmPara(m_nnPostFilterSEICharacteristicsId[i] > MAX_NNPFC_ID, "SEINNPFCId must be in the range of 0 to 2^32-2");
      xConfirmPara(m_nnPostFilterSEICharacteristicsModeIdc[i] > 255, "SEINNPFCModeIdc must be in the range of 0 to 255");
      xConfirmPara(m_nnPostFilterSEICharacteristicsPurpose[i] > 127, "SEINNPFCPurpose must be in the range of 0 to 127");
      xConfirmPara(m_nnPostFilterSEICharacteristicsNumberInputDecodedPicturesMinus1[i] > 63, "SEINNPFCNumberInputDecodedPicturesMinus1 must be in the range of 0 to 63");
      xConfirmPara(m_nnPostFilterSEICharacteristicsNumberExtrapolatedPicturesMinus1[i] > 62, "SEINNPFCNumberExtrapolatedPicsMinus1 must be in the range of 0 to 62");
      xConfirmPara(m_nnPostFilterSEICharacteristicsSpatialExtrapolationLeftOffset[i] < -65536 || m_nnPostFilterSEICharacteristicsSpatialExtrapolationLeftOffset[i] > 65536,
                    "SEINNPFCCharacteristicSpatialExtrapolationLeftOffset must be in the range of -65536 to 65536");
      xConfirmPara(m_nnPostFilterSEICharacteristicsSpatialExtrapolationRightOffset[i] < -65536 || m_nnPostFilterSEICharacteristicsSpatialExtrapolationRightOffset[i] > 65536,
                    "SEINNPFCCharacteristicSpatialExtrapolationRightOffset must be in the range of -65536 to 65536");
      xConfirmPara(m_nnPostFilterSEICharacteristicsSpatialExtrapolationTopOffset[i] < -65536 || m_nnPostFilterSEICharacteristicsSpatialExtrapolationTopOffset[i] > 65536,
                    "SEINNPFCCharacteristicSpatialExtrapolationTopOffset must be in the range of -65536 to 65536");
      xConfirmPara(m_nnPostFilterSEICharacteristicsSpatialExtrapolationBottomOffset[i] < -65536 || m_nnPostFilterSEICharacteristicsSpatialExtrapolationBottomOffset[i] > 65536,
                    "SEINNPFCCharacteristicSpatialExtrapolationBottomOffset must be in the range of -65536 to 65536");
      xConfirmPara(m_nnPostFilterSEICharacteristicsInpTensorBitDepthLumaMinus8[i] > 24, "SEINNPFCInpTensorBitDepthLumaMinus8 must be in the range of 0 to 24");
      xConfirmPara(m_nnPostFilterSEICharacteristicsInpTensorBitDepthChromaMinus8[i] > 24, "SEINNPFCInpTensorBitDepthChromaMinus8 must be in the range of 0 to 24");
      xConfirmPara(m_nnPostFilterSEICharacteristicsOutTensorBitDepthLumaMinus8[i] > 24, "SEINNPFCOutTensorBitDepthLumaMinus8 must be in the range of 0 to 24");
      xConfirmPara(m_nnPostFilterSEICharacteristicsOutTensorBitDepthChromaMinus8[i] > 24, "SEINNPFCOutTensorBitDepthChromaMinus8 must be in the range of 0 to 24");
      xConfirmPara(m_nnPostFilterSEICharacteristicsInpFormatIdc[i] > 255, "SEINNPFCInpFormatIdc must be in the range of 0 to 255");
      xConfirmPara(m_nnPostFilterSEICharacteristicsInpOrderIdc[i] > 255, "SEINNPFCInpOrderIdc must be in the range of  0 to 255");
      xConfirmPara(m_nnPostFilterSEICharacteristicsColPrimaries[i] > 255, "m_nnPostFilterSEICharacteristicsColPrimaries must in the range 0 to 255");
      xConfirmPara(m_nnPostFilterSEICharacteristicsTransCharacteristics[i] > 255, "m_nnPostFilterSEICharacteristicsTransCharacteristics must in the range 0 to 255");
      xConfirmPara(m_nnPostFilterSEICharacteristicsMatrixCoeffs[i] > 255, "m_nnPostFilterSEICharacteristicsMatrixCoeffs must in the range 0 to 255");
      xConfirmPara(m_nnPostFilterSEICharacteristicsOutFormatIdc[i] > 255, "SEINNPFCOutFormatIdc must be in the range of 0 to 255");
      xConfirmPara(m_nnPostFilterSEICharacteristicsOutOrderIdc[i] > 255, "SEINNPFCOutOrderIdc must be in the range of 0 to 255");
      if (m_nnPostFilterSEICharacteristicsChromaLocInfoPresentFlag[i])
      {
        xConfirmPara(m_nnPostFilterSEICharacteristicsChromaSampleLocTypeFrame[i] > (uint32_t) Chroma420LocType::UNSPECIFIED,
                     "The value of nnpfc_chroma_sample_loc_type_frame shall be in the range of 0 to 6, inclusive");
      }
      xConfirmPara(m_nnPostFilterSEICharacteristicsPatchWidthMinus1[i] > 32766, "SEINNPFCPatchWidthMinus1 must be in the range of 0 to 32766");
      xConfirmPara(m_nnPostFilterSEICharacteristicsPatchHeightMinus1[i] > 32766, "SEINNPFCPatchHeightMinus1 must be in the range of 0 to 32766");
      xConfirmPara(m_nnPostFilterSEICharacteristicsOverlap[i] > 16383, "SEINNPFCOverlap must be in the range of 0 to 16383");
      xConfirmPara(m_nnPostFilterSEICharacteristicsPaddingType[i] > (1 << 4) - 1, "SEINNPostFilterPaddingType must be in the range of 0 to 2^4-1");
      xConfirmPara(m_nnPostFilterSEICharacteristicsLog2ParameterBitLengthMinus3[i] > 3, "SEINNPFCLog2ParameterBitLengthMinus3 must be in the range of 0 to 3");
      xConfirmPara(m_nnPostFilterSEICharacteristicsNumParametersIdc[i] > 52, "SEINNPFCNumParametersIdc must be in the range of 0 to 52");
      xConfirmPara(m_nnPostFilterSEICharacteristicsTotalKilobyteSize[i] > (uint32_t) (((uint64_t) 1 << 32) - 2), "SEINNPFCTotalKilobyteSize must be in the range of 0 to 2^32-2");
      xConfirmPara(m_nnPostFilterSEICharacteristicsNumKmacOperationsIdc[i] > (uint32_t) (((uint64_t) 1 << 32) - 2), "SEICharacteristicsNumKmacOperationsIdc must be in the range of 0 to 2^32-2");
      xConfirmPara(m_nnPostFilterSEICharacteristicsPicWidthNumerator[i] <= 0,
                   "Output picture width numerator cannot be equal to or less than 0");
      xConfirmPara(m_nnPostFilterSEICharacteristicsPicWidthDenominator[i] <= 0,
                   "Output picture width denominator cannot be equal to or less than 0");
      xConfirmPara(m_nnPostFilterSEICharacteristicsPicHeightNumerator[i] <= 0,
                   "Output picture height numerator cannot be equal to or less than 0");
      xConfirmPara(m_nnPostFilterSEICharacteristicsPicHeightDenominator[i] <= 0,
                   "Output picture height denominator cannot be equal to or less than 0");
      xConfirmPara(m_nnPostFilterSEICharacteristicsLumaPadding[i] > ((1 << m_inputBitDepth[ChannelType::LUMA]) - 1), "SEINNPFCLumaPadding must be in the range of 0 to 2^bitDepthLuma - 1");
      xConfirmPara(m_nnPostFilterSEICharacteristicsCbPadding[i] > ((1 << m_inputBitDepth[ChannelType::CHROMA]) - 1), "SEINNPFCLumaPadding must be in the range of 0 to 2^bitDepthChroma - 1");
      xConfirmPara(m_nnPostFilterSEICharacteristicsCrPadding[i] > ((1 << m_inputBitDepth[ChannelType::CHROMA]) - 1), "SEINNPFCLumaPadding must be in the range of 0 to 2^bitDepthChroma - 1");
#if NNPFC_SCAN_TYPE_IDC
      xConfirmPara(m_nnPostFilterSEICharacteristicsScanTypeIdc[i] > 2, "SEINNPFCScanTypeIdc must be in the range of 0 to 2");
#endif
      xConfirmPara(m_nnPostFilterSEICharacteristicsForHumanViewingIdc[i] > 3, "SEINNPFCForHumanViewingIdc must be in the range of 0 to 3");
      xConfirmPara(m_nnPostFilterSEICharacteristicsForMachineAnalysisIdc[i] > 3, "SEINNPFCForMachineAnalysisIdc must be in the range of 0 to 3");
    }
  }

  if (m_nnPostFilterSEIActivationEnabled)
  {
    xConfirmPara(m_nnPostFilterSEIActivationTargetId > MAX_NNPFA_ID, "SEINNPostFilterActivationTargetId must be in the range of 0 to 2^32-2");
#if JVET_AJ0104_NNPFA_PROMPT_UPDATE
    if (m_nnPostFilterSEIActivationPromptUpdateFlag)
    {
      xConfirmPara(m_nnPostFilterSEIActivationPrompt.empty(), "When present in the bitstream, SEINNPostFilterActivationPrompt shall not be a null string");
    }
#endif
  }

  if (m_phaseIndicationSEIEnabledFullResolution)
  {
    xConfirmPara(m_piHorPhaseNumFullResolution < 0, "m_piHorPhaseNumFullResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piHorPhaseDenMinus1FullResolution < 0, "m_piHorPhaseDenMinus1FullResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piVerPhaseNumFullResolution < 0, "m_piVerPhaseNumFullResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piVerPhaseDenMinus1FullResolution < 0, "m_piVerPhaseDenMinus1FullResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piHorPhaseDenMinus1FullResolution > 511, "m_piHorPhaseDenMinus1FullResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piHorPhaseNumFullResolution > m_piHorPhaseDenMinus1FullResolution + 1, "m_piHorPhaseNumFullResolution must be in the range of 0 to m_piHorPhaseDenMinus1FullResolution + 1, inclusive");
    xConfirmPara(m_piVerPhaseDenMinus1FullResolution > 511, "m_piVerPhaseDenMinus1FullResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piVerPhaseNumFullResolution > m_piVerPhaseDenMinus1FullResolution + 1, "m_piVerPhaseNumFullResolution must be in the range of 0 to m_piVerPhaseDenMinus1FullResolution + 1, inclusive");
  }
  if (m_phaseIndicationSEIEnabledReducedResolution)
  {
    xConfirmPara(m_piHorPhaseNumReducedResolution < 0, "m_piHorPhaseNumReducedResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piHorPhaseDenMinus1ReducedResolution < 0, "m_piHorPhaseDenMinus1ReducedResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piVerPhaseNumReducedResolution < 0, "m_piVerPhaseNumReducedResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piVerPhaseDenMinus1ReducedResolution < 0, "m_piVerPhaseDenMinus1ReducedResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piHorPhaseDenMinus1ReducedResolution > 511, "m_piHorPhaseDenMinus1ReducedResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piHorPhaseNumReducedResolution > m_piHorPhaseDenMinus1ReducedResolution + 1, "m_piHorPhaseNumReducedResolution must be in the range of 0 to m_piHorPhaseDenMinus1ReducedResolution + 1, inclusive");
    xConfirmPara(m_piVerPhaseDenMinus1ReducedResolution > 511, "m_piVerPhaseDenMinus1ReducedResolution must be in the range of 0 to 511, inclusive");
    xConfirmPara(m_piVerPhaseNumReducedResolution > m_piVerPhaseDenMinus1ReducedResolution + 1, "m_piVerPhaseNumReducedResolution must be in the range of 0 to m_piVerPhaseDenMinus1ReducedResolution + 1, inclusive");
  }

  xConfirmPara(m_SEITextDescriptionID < 1 && m_SEITextDescriptionID > 16383, "m_SEITextDescriptionID must be in the range of 1 to 16383, inclusive");
  xConfirmPara(m_SEITextDescriptionPurpose > 6, "m_SEITextDescriptionPurpose must be in the range of 0 to 6, inclusive");
  xConfirmPara(m_SEITextNumStringsMinus1 > 255, "m_SEITextNumStringsMinus1 must be in the range 0f 0 to 255, inclusive");

  xConfirmPara(m_log2ParallelMergeLevel < 2, "Log2ParallelMergeLevel should be larger than or equal to 2");
  xConfirmPara(m_log2ParallelMergeLevel > m_ctuSize, "Log2ParallelMergeLevel should be less than or equal to CTU size");
  xConfirmPara(m_preferredTransferCharacteristics > 255, "transfer_characteristics_idc should not be greater than 255.");
  xConfirmPara( unsigned(m_ImvMode) > 1, "ImvMode exceeds range (0 to 1)" );
  if (m_AffineAmvr)
  {
    xConfirmPara(!m_ImvMode, "AffineAmvr cannot be used when IMV is disabled.");
  }
  xConfirmPara( m_decodeBitstreams[0] == m_bitstreamFileName, "Debug bitstream and the output bitstream cannot be equal.\n" );
  xConfirmPara( m_decodeBitstreams[1] == m_bitstreamFileName, "Decode2 bitstream and the output bitstream cannot be equal.\n" );
  xConfirmPara(unsigned(m_LMChroma) > 1, "LMMode exceeds range (0 to 1)");
  if (m_gopBasedTemporalFilterEnabled)
  {
    xConfirmPara(m_temporalSubsampleRatio != 1, "GOP Based Temporal Filter only support Temporal sub-sample ratio 1");
    xConfirmPara(
      m_gopBasedTemporalFilterPastRefs <= 0 && m_gopBasedTemporalFilterFutureRefs <= 0,
      "Either TemporalFilterPastRefs or TemporalFilterFutureRefs must be larger than 0 when TemporalFilter is enabled");

    if ((m_gopBasedTemporalFilterPastRefs != 0 && m_gopBasedTemporalFilterPastRefs != TF_DEFAULT_REFS)
        || (m_gopBasedTemporalFilterFutureRefs != 0 && m_gopBasedTemporalFilterFutureRefs != TF_DEFAULT_REFS))
    {
      msg(WARNING, "Number of frames used for temporal prefilter is different from default.\n");
    }
  }
  if (m_bimEnabled)
  {
    xConfirmPara(m_temporalSubsampleRatio != 1, "Block Importance Mapping only support Temporal sub-sample ratio 1");
    xConfirmPara(
      m_gopBasedTemporalFilterPastRefs <= 0 && m_gopBasedTemporalFilterFutureRefs <= 0,
      "Either TemporalFilterPastRefs or TemporalFilterFutureRefs must be larger than 0 when Block Importance Mapping is enabled" );
  }
  xConfirmPara(m_dpfKeyLen < 0, "DPF Key Length must be greater than or equal to 0");
  xConfirmPara(m_dpfNonkeyLen < 0, "DPF Non-key Length must be greater than or equal to 0");
#if EXTENSION_360_VIDEO
  check_failed |= m_ext360.verifyParameters();
#endif

  xConfirmPara(m_useColorTrans && (m_log2MaxTbSize == 6), "Log2MaxTbSize must be less than 6 when ACT is enabled, otherwise ACT needs to be disabled");

  xConfirmPara(m_ctuSize <= 32 && (m_log2MaxTbSize == 6), "Log2MaxTbSize must be less than 6 when CTU size is 32");

#undef xConfirmPara
  return check_failed;
}

const char *profileToString(const Profile::Name profile)
{
  static const uint32_t numberOfProfiles = sizeof(strToProfile)/sizeof(*strToProfile);

  for (uint32_t profileIndex = 0; profileIndex < numberOfProfiles; profileIndex++)
  {
    if (strToProfile[profileIndex].value == profile)
    {
      return strToProfile[profileIndex].str;
    }
  }

  //if we get here, we didn't find this profile in the list - so there is an error
  EXIT( "ERROR: Unknown profile \"" << profile << "\" in profileToString" );
  return "";
}

void EncAppCfg::xPrintParameter()
{
  //msg( DETAILS, "\n" );
  msg( DETAILS, "Input          File                    : %s\n", m_inputFileName.c_str() );
  msg( DETAILS, "Bitstream      File                    : %s\n", m_bitstreamFileName.c_str() );
  msg( DETAILS, "Reconstruction File                    : %s\n", m_reconFileName.c_str() );
  if (m_ShutterFilterEnable && !m_shutterIntervalPreFileName.empty())
  {
    msg(DETAILS,"SII Pre-processed File                 : %s\n", m_shutterIntervalPreFileName.c_str());
  }
  msg(DETAILS, "Real     Format                        : %dx%d %gHz\n", m_sourceWidth - m_confWinLeft - m_confWinRight,
      m_sourceHeight - m_confWinTop - m_confWinBottom, m_frameRate.getFloatVal() / m_temporalSubsampleRatio);
  msg(DETAILS, "Internal Format                        : %dx%d %gHz\n", m_sourceWidth, m_sourceHeight,
      m_frameRate.getFloatVal() / m_temporalSubsampleRatio);
  msg( DETAILS, "Sequence PSNR output                   : %s\n", ( m_printMSEBasedSequencePSNR ? "Linear average, MSE-based" : "Linear average only" ) );
  msg( DETAILS, "Hexadecimal PSNR output                : %s\n", ( m_printHexPsnr ? "Enabled" : "Disabled" ) );
  msg( DETAILS, "Sequence MSE output                    : %s\n", ( m_printSequenceMSE ? "Enabled" : "Disabled" ) );
  msg( DETAILS, "Frame MSE output                       : %s\n", ( m_printFrameMSE ? "Enabled" : "Disabled" ) );
  msg( DETAILS, "MS-SSIM output                         : %s\n", ( m_printMSSSIM ? "Enabled" : "Disabled") );
  msg( DETAILS, "Cabac-zero-word-padding                : %s\n", ( m_cabacZeroWordPaddingEnabled ? "Enabled" : "Disabled" ) );
  if (m_isField)
  {
    msg( DETAILS, "Frame/Field                            : Field based coding\n" );
    msg(DETAILS, "Field index                            : %u - %d (%d fields)\n", m_frameSkip,
        m_frameSkip + m_framesToBeEncoded - 1, m_framesToBeEncoded);
    msg( DETAILS, "Field Order                            : %s field first\n", m_isTopFieldFirst ? "Top" : "Bottom" );

  }
  else
  {
    msg( DETAILS, "Frame/Field                            : Frame based coding\n" );
    msg(DETAILS, "Frame index                            : %u - %d (%d frames)\n", m_frameSkip,
        m_frameSkip + m_framesToBeEncoded - 1, m_framesToBeEncoded);
  }
  {
    msg( DETAILS, "Profile                                : %s\n", profileToString(m_profile) );
  }
  msg( DETAILS,"AllRapPicturesFlag                     : %d\n", m_allRapPicturesFlag );
  msg(DETAILS, "CTU size / min CU size                 : %d / %d \n", m_maxCuWidth, 1 << m_log2MinCuSize);

  msg(DETAILS, "subpicture info present flag           : %s\n", m_subPicInfoPresentFlag ? "Enabled" : "Disabled");
  if (m_subPicInfoPresentFlag)
  {
    msg(DETAILS, "number of subpictures                  : %d\n", m_numSubPics);
    msg(DETAILS, "subpicture size same flag              : %d\n", m_subPicSameSizeFlag);
    if (m_subPicSameSizeFlag)
    {
      msg(DETAILS, "[0]th subpicture size                  : [%d %d]\n", m_subPicWidth[0], m_subPicHeight[0]);
    }
    for (int i = 0; i < m_numSubPics; i++)
    {
      if (!m_subPicSameSizeFlag)
      {
        msg(DETAILS, "[%d]th subpicture location              : [%d %d]\n", i, m_subPicCtuTopLeftX[i],
            m_subPicCtuTopLeftY[i]);
        msg(DETAILS, "[%d]th subpicture size                  : [%d %d]\n", i, m_subPicWidth[i], m_subPicHeight[i]);
      }
      msg(DETAILS, "[%d]th subpicture treated as picture    : %d\n", i,
          m_subPicTreatedAsPicFlag[i] ? "Enabled" : "Disabled");
      msg(DETAILS, "loop filter across [%d]th subpicture    : %d\n", i,
          m_loopFilterAcrossSubpicEnabledFlag[i] ? "Enabled" : "Disabled");
    }
  }

  msg(DETAILS, "subpicture ID present flag             : %s\n",
      m_subPicIdMappingExplicitlySignalledFlag ? "Enabled" : "Disabled");
  if (m_subPicIdMappingExplicitlySignalledFlag)
  {
    msg(DETAILS, "subpicture ID signalling present flag  : %d\n", m_subPicIdMappingInSpsFlag);
    for (int i = 0; i < m_numSubPics; i++)
    {
      msg(DETAILS, "[%d]th subpictures ID length           : %d\n", i, m_subPicIdLen);
      msg(DETAILS, "[%d]th subpictures ID                  : %d\n", i, m_subPicId[i]);
    }
  }
  msg( DETAILS, "Max TB size                            : %d \n", 1 << m_log2MaxTbSize );
  msg( DETAILS, "Motion search range                    : %d\n", m_iSearchRange );
  msg(DETAILS, "Intra period                           : %d\n", m_intraPeriod);
  msg(DETAILS, "Decoding refresh type                  : %d\n", m_intraRefreshType);
  msg( DETAILS, "DRAP period                            : %d\n", m_drapPeriod );
  msg( DETAILS, "EDRAP period                           : %d\n", m_edrapPeriod );
  if (m_qpIncrementAtSourceFrame.has_value())
  {
    msg(DETAILS, "QP                                     : %d (incrementing internal QP at source frame %d)\n", m_iQP,
        m_qpIncrementAtSourceFrame.value());
  }
  else
  {
    msg( DETAILS, "QP                                     : %d\n", m_iQP);
  }
  msg( DETAILS, "Max dQP signaling subdiv               : %d\n", m_cuQpDeltaSubdiv);

  msg( DETAILS, "Cb QP Offset (dual tree)               : %d (%d)\n", m_cbQpOffset, m_cbQpOffsetDualTree);
  msg( DETAILS, "Cr QP Offset (dual tree)               : %d (%d)\n", m_crQpOffset, m_crQpOffsetDualTree);
  msg( DETAILS, "QP adaptation                          : %d (range=%d)\n", m_bUseAdaptiveQP, (m_bUseAdaptiveQP ? m_iQPAdaptationRange : 0) );
  msg(DETAILS, "GOP size                               : %d\n", m_gopSize);
  msg(DETAILS, "Input bit depth                        : (Y:%d, C:%d)\n", m_inputBitDepth[ChannelType::LUMA],
      m_inputBitDepth[ChannelType::CHROMA]);
  msg(DETAILS, "MSB-extended bit depth                 : (Y:%d, C:%d)\n", m_msbExtendedBitDepth[ChannelType::LUMA],
      m_msbExtendedBitDepth[ChannelType::CHROMA]);
  msg(DETAILS, "Internal bit depth                     : (Y:%d, C:%d)\n", m_internalBitDepth[ChannelType::LUMA],
      m_internalBitDepth[ChannelType::CHROMA]);
  if (m_cuChromaQpOffsetList.size() > 0)
  {
    msg( DETAILS, "Chroma QP offset list                  : (" );
    for (int i=0; i < m_cuChromaQpOffsetList.size(); i++)
    {
      msg(DETAILS, "%d %d %d%s", m_cuChromaQpOffsetList[i].u.comp.cbOffset, m_cuChromaQpOffsetList[i].u.comp.crOffset,
          m_cuChromaQpOffsetList[i].u.comp.jointCbCrOffset, (i + 1 < m_cuChromaQpOffsetList.size() ? ", " : ")\n"));
    }
    msg( DETAILS, "cu_chroma_qp_offset_subdiv             : %d\n", m_cuChromaQpOffsetSubdiv);
    msg( DETAILS, "cu_chroma_qp_offset_enabled_flag       : %s\n", (m_cuChromaQpOffsetEnabled ? "Enabled" : "Disabled") );
  }
  else
  {
    msg( DETAILS, "Chroma QP offset list                  : Disabled\n" );
  }
  msg( DETAILS, "extended_precision_processing_flag     : %s\n", (m_extendedPrecisionProcessingFlag         ? "Enabled" : "Disabled") );
  msg( DETAILS, "TSRC_Rice_present_flag                 : %s\n", (m_tsrcRicePresentFlag                     ? "Enabled" : "Disabled") );
  msg( DETAILS, "reverse_last_sig_coeff_enabled_flag    : %s\n", (m_reverseLastSigCoeffEnabledFlag          ? "Enabled" : "Disabled") );
  msg( DETAILS, "transform_skip_rotation_enabled_flag   : %s\n", (m_transformSkipRotationEnabledFlag        ? "Enabled" : "Disabled") );
  msg( DETAILS, "transform_skip_context_enabled_flag    : %s\n", (m_transformSkipContextEnabledFlag         ? "Enabled" : "Disabled") );
  msg( DETAILS, "high_precision_offsets_enabled_flag    : %s\n", (m_highPrecisionOffsetsEnabledFlag         ? "Enabled" : "Disabled") );
  msg( DETAILS, "rrc_rice_extension_flag                : %s\n", (m_rrcRiceExtensionEnableFlag                 ? "Enabled" : "Disabled") );
  msg( DETAILS, "persistent_rice_adaptation_enabled_flag: %s\n", (m_persistentRiceAdaptationEnabledFlag     ? "Enabled" : "Disabled") );
  msg( DETAILS, "cabac_bypass_alignment_enabled_flag    : %s\n", (m_cabacBypassAlignmentEnabledFlag         ? "Enabled" : "Disabled") );

  switch (m_costMode)
  {
    case COST_STANDARD_LOSSY:               msg( DETAILS, "Cost function:                         : Lossy coding (default)\n"); break;
    case COST_SEQUENCE_LEVEL_LOSSLESS:      msg( DETAILS, "Cost function:                         : Sequence_level_lossless coding\n"); break;
    case COST_LOSSLESS_CODING:              msg( DETAILS, "Cost function:                         : Lossless coding with fixed QP of %d\n", LOSSLESS_AND_MIXED_LOSSLESS_RD_COST_TEST_QP); break;
    case COST_MIXED_LOSSLESS_LOSSY_CODING:  msg( DETAILS, "Cost function:                         : Mixed_lossless_lossy coding with QP'=%d for lossless evaluation\n", LOSSLESS_AND_MIXED_LOSSLESS_RD_COST_TEST_QP_PRIME); break;
    default:                                msg( DETAILS, "Cost function:                         : Unknown\n"); break;
  }

  msg(DETAILS, "RateControl                            : %d\n", m_rcEnableRateControl);
  msg( DETAILS, "WeightedPredMethod                     : %d\n", int(m_weightedPredictionMethod));

  if (m_rcEnableRateControl)
  {
    msg(DETAILS, "TargetBitrate                          : %d\n", m_rcTargetBitrate);
    msg(DETAILS, "KeepHierarchicalBit                    : %d\n", m_rcKeepHierarchicalBit);
    msg(DETAILS, "LCULevelRC                             : %d\n", m_rcCtuLevelRateControl);
    msg(DETAILS, "UseLCUSeparateModel                    : %d\n", m_rcUseCtuSeparateModel);
    msg(DETAILS, "InitialQP                              : %d\n", m_rcInitialQp);
    msg(DETAILS, "ForceIntraQP                           : %d\n", m_rcForceIntraQp);
    msg(DETAILS, "CpbSaturation                          : %d\n", m_rcCpbSaturationEnabled);
    if (m_rcCpbSaturationEnabled)
    {
      msg(DETAILS, "CpbSize                                : %d\n", m_rcCpbSize);
      msg(DETAILS, "InitalCpbFullness                      : %.2f\n", m_rcInitialCpbFullness);
    }
  }

#if GDR_ENABLED
  msg(DETAILS, "GDREnabled                             : %d\n", m_gdrEnabled);

  if (m_gdrEnabled)
  {
    msg(DETAILS, "GDR Start                              : %d\n", m_gdrPocStart);
    msg(DETAILS, "GDR Interval                           : %d\n", m_gdrInterval);
    msg(DETAILS, "GDR Period                             : %d\n", m_gdrPeriod);
  }
#endif

  msg( DETAILS, "Max Num Merge Candidates               : %d\n", m_maxNumMergeCand );
  msg( DETAILS, "Max Num Affine Merge Candidates        : %d\n", m_maxNumAffineMergeCand );
  msg( DETAILS, "Max Num Geo Merge Candidates           : %d\n", m_maxNumGeoCand );
  msg( DETAILS, "Max Num IBC Merge Candidates           : %d\n", m_maxNumIBCMergeCand );
  msg( DETAILS, "\n");

  msg( VERBOSE, "TOOL CFG: ");
  msg(VERBOSE, "IBD:%d ",
      ((m_internalBitDepth[ChannelType::LUMA] > m_msbExtendedBitDepth[ChannelType::LUMA])
       || (m_internalBitDepth[ChannelType::CHROMA] > m_msbExtendedBitDepth[ChannelType::CHROMA])));
  msg( VERBOSE, "HAD:%d ", m_bUseHADME                          );
  msg( VERBOSE, "RDQ:%d ", m_useRDOQ                            );
  msg( VERBOSE, "RDQTS:%d ", m_useRDOQTS                        );
#if SHARP_LUMA_DELTA_QP
  msg( VERBOSE, "LQP:%d ", m_lumaLevelToDeltaQPMapping.mode     );
#endif
  msg( VERBOSE, "SQP:%d ", m_uiDeltaQpRD                        );
  msg( VERBOSE, "ASR:%d ", m_bUseASR                            );
  msg( VERBOSE, "MinSearchWindow:%d ", m_minSearchWindow        );
  msg( VERBOSE, "RestrictMESampling:%d ", m_bRestrictMESampling );
  msg( VERBOSE, "FEN:%d ", int(m_fastInterSearchMode)           );
  msg( VERBOSE, "ECU:%d ", m_bUseEarlyCU                        );
  msg( VERBOSE, "FDM:%d ", m_useFastDecisionForMerge            );
  msg( VERBOSE, "ESD:%d ", m_useEarlySkipDetection              );
  msg( VERBOSE, "TransformSkip:%d ",     m_useTransformSkip     );
  msg( VERBOSE, "TransformSkipFast:%d ", m_useTransformSkipFast );
  msg( VERBOSE, "TransformSkipLog2MaxSize:%d ", m_log2MaxTransformSkipBlockSize);
  msg(VERBOSE, "ChromaTS:%d ", m_useChromaTS);
  msg( VERBOSE, "BDPCM:%d ", m_useBDPCM                         );
  msg( VERBOSE, "Tiles: %dx%d ", m_numTileCols, m_numTileRows );
  msg( VERBOSE, "Slices: %d ", m_numSlicesInPic);
  msg( VERBOSE, "MCTS:%d ", m_MCTSEncConstraint );
  msg(VERBOSE, "SAO:%d ", (m_useSao) ? (1) : (0));
  msg( VERBOSE, "ALF:%d ", m_alf ? 1 : 0 );
  msg( VERBOSE, "CCALF:%d ", m_ccalf ? 1 : 0 );
  msg(VERBOSE, "MaxNumALFAPS %d ", m_maxNumAlfAps);
  msg(VERBOSE, "AlfapsIDShift %d ", m_alfapsIDShift);
  msg(VERBOSE, "ConstantJointCbCrSignFlag", m_constantJointCbCrSignFlag);
  msg( VERBOSE, "WPP:%d ", (int)m_useWeightedPred);
  msg( VERBOSE, "WPB:%d ", (int)m_useWeightedBiPred);
  msg( VERBOSE, "PME:%d ", m_log2ParallelMergeLevel);
  const int wavefrontSubstreams =
    m_entropyCodingSyncEnabledFlag ? (m_sourceHeight + m_maxCuHeight - 1) / m_maxCuHeight : 1;
  msg(VERBOSE, " WaveFrontSynchro:%d WaveFrontSubstreams:%d", m_entropyCodingSyncEnabledFlag ? 1 : 0,
      wavefrontSubstreams);
  msg( VERBOSE, " ScalingList:%d ", m_useScalingListId );
  msg( VERBOSE, "TMVPMode:%d ", m_TMVPModeId );
  msg( VERBOSE, " DQ:%d ", m_depQuantEnabledFlag);
  msg( VERBOSE, " SignBitHidingFlag:%d ", m_signDataHidingEnabledFlag);
  msg( VERBOSE, "RecalQP:%d ", m_recalculateQPAccordingToLambda ? 1 : 0 );

  {
    msg( VERBOSE, "\nTOOL CFG: " );
    msg( VERBOSE, "LFNST:%d ", m_LFNST );
    msg( VERBOSE, "MMVD:%d ", m_MMVD);
    msg( VERBOSE, "Affine:%d ", m_Affine );
    if ( m_Affine )
    {
      msg( VERBOSE, "AffineType:%d ", m_AffineType );
      msg( VERBOSE, "AdaptBypassAffineMe:%d ", m_adaptBypassAffineMe);
    }
    msg(VERBOSE, "PROF:%d ", m_PROF);
    msg(VERBOSE, "SbTMVP:%d ", m_sbTmvpEnableFlag);
    msg( VERBOSE, "DualITree:%d ", m_dualTree );
    msg( VERBOSE, "IMV:%d ", m_ImvMode );
    msg( VERBOSE, "BIO:%d ", m_BIO );
    msg( VERBOSE, "LMChroma:%d ", m_LMChroma );
    msg( VERBOSE, "HorCollocatedChroma:%d ", m_horCollocatedChromaFlag );
    msg( VERBOSE, "VerCollocatedChroma:%d ", m_verCollocatedChromaFlag );

    {
      std::string s;
      const int   m = m_mtsMode + 4 * m_mtsImplicitIntra;
      if (m != 0)
      {
        s = "(";
        s += (m & 1) != 0 ? "explicit intra" : "implicit intra";
        if (m & 2)
        {
          s += ", explicit inter";
        }
        s += ")";
      }
      msg(VERBOSE, "MTS:%d%s ", m != 0, s.c_str());
    }
    msg( VERBOSE, "SBT:%d ", m_SBT );
    msg( VERBOSE, "ISP:%d ", m_ISP );
    msg( VERBOSE, "SMVD:%d ", m_SMVD );
    msg( VERBOSE, "CompositeLTReference:%d ", m_compositeRefEnabled);
    msg( VERBOSE, "Bcw:%d ", m_bcw );
    msg( VERBOSE, "BcwFast:%d ", m_BcwFast );
    msg( VERBOSE, "LADF:%d ", m_LadfEnabed );
    msg(VERBOSE, "CIIP:%d ", m_ciip);
    msg( VERBOSE, "Geo:%d ", m_Geo );
    m_allowDisFracMMVD = m_MMVD ? m_allowDisFracMMVD : false;
    if ( m_MMVD )
      msg(VERBOSE, "AllowDisFracMMVD:%d ", m_allowDisFracMMVD);
    msg( VERBOSE, "AffineAmvr:%d ", m_AffineAmvr );
    m_AffineAmvrEncOpt = m_AffineAmvr ? m_AffineAmvrEncOpt : false;
    msg( VERBOSE, "AffineAmvrEncOpt:%d ", m_AffineAmvrEncOpt );
    msg(VERBOSE, "AffineAmvp:%d ", m_AffineAmvp);
    msg(VERBOSE, "DMVR:%d ", m_DMVR);
    msg(VERBOSE, "MmvdDisNum:%d ", m_MmvdDisNum);
    msg(VERBOSE, "JointCbCr:%d ", m_jointCbCrMode);
  }
  m_useColorTrans = m_chromaFormatIdc == ChromaFormat::_444 ? m_useColorTrans : false;
  msg(VERBOSE, "ACT:%d ", m_useColorTrans ? 1 : 0);
  msg(VERBOSE, "PLT:%d ", m_PLTMode);
  msg(VERBOSE, "IBC:%d ", m_IBCMode);
  msg( VERBOSE, "HashME:%d ", m_HashME );
  msg( VERBOSE, "WrapAround:%d ", m_wrapAround);
  if( m_wrapAround )
  {
    msg( VERBOSE, "WrapAroundOffset:%d ", m_wrapAroundOffset );
  }
  // ADD_NEW_TOOL (add some output indicating the usage of tools)
  msg( VERBOSE, "VirtualBoundariesEnabledFlag:%d ", m_virtualBoundariesEnabledFlag );
  msg( VERBOSE, "VirtualBoundariesPresentInSPSFlag:%d ", m_virtualBoundariesPresentFlag );
  if( m_virtualBoundariesPresentFlag )
  {
    msg(VERBOSE, "vertical virtual boundaries:[");
    for (unsigned i = 0; i < m_numVerVirtualBoundaries; i++)
    {
      msg(VERBOSE, " %d", m_virtualBoundariesPosX[i]);
    }
    msg(VERBOSE, " ] horizontal virtual boundaries:[");
    for (unsigned i = 0; i < m_numHorVirtualBoundaries; i++)
    {
      msg(VERBOSE, " %d", m_virtualBoundariesPosY[i]);
    }
    msg(VERBOSE, " ] ");
  }
    msg(VERBOSE, "Reshape:%d ", m_lmcsEnabled);
    if (m_lmcsEnabled)
    {
      msg(VERBOSE, "(Signal:%s ", m_reshapeSignalType == 0 ? "SDR" : (m_reshapeSignalType == 2 ? "HDR-HLG" : "HDR-PQ"));
      msg(VERBOSE, "Opt:%d", m_adpOption);
      if (m_adpOption > 0) { msg(VERBOSE, " CW:%d", m_initialCW); }
      msg(VERBOSE, " CSoffset:%d", m_CSoffset);
      msg(VERBOSE, ") ");
    }
    msg(VERBOSE, "MRL:%d ", m_MRL);
    msg(VERBOSE, "MIP:%d ", m_MIP);
    msg(VERBOSE, "EncDbOpt:%d ", m_encDbOpt);
    msg(VERBOSE, "AlfLambdaOpt:%d ", m_encALFOpt);
  msg( VERBOSE, "\nFAST TOOL CFG: " );
  msg( VERBOSE, "LCTUFast:%d ", m_useFastLCTU );
  msg( VERBOSE, "FastMrg:%d ", m_useFastMrg );
  msg( VERBOSE, "MaxMergeRdCandNumTotal:%d MergeRdCandQuotaRegular:%d MergeRdCandQuotaRegularSmallBlk:%d ", 
    m_maxMergeRdCandNumTotal, m_mergeRdCandQuotaRegular, m_mergeRdCandQuotaRegularSmallBlk);
  msg( VERBOSE, "MergeRdCandQuotaSubBlk:%d MergeRdCandQuotaCiip:%d MergeRdCandQuotaGpm:%d ",
    m_mergeRdCandQuotaSubBlk, m_mergeRdCandQuotaCiip, m_mergeRdCandQuotaGpm);
  msg( VERBOSE, "PBIntraFast:%d ", m_usePbIntraFast );
  if( m_ImvMode ) msg( VERBOSE, "IMV4PelFast:%d ", m_Imv4PelFast );
  if (m_mtsMode)
  {
    msg(VERBOSE, "MTSMaxCand: %1d(intra) %1d(inter) ", m_MTSIntraMaxCand, m_MTSInterMaxCand);
  }
  if( m_ISP ) msg( VERBOSE, "ISPFast:%d ", m_useFastISP );
  if( m_LFNST ) msg( VERBOSE, "FastLFNST:%d ", m_useFastLFNST );
  msg( VERBOSE, "AMaxBT:%d ", m_useAMaxBT );
  msg( VERBOSE, "E0023FastEnc:%d ", m_e0023FastEnc );
  msg( VERBOSE, "ContentBasedFastQtbt:%d ", m_contentBasedFastQtbt );
  msg( VERBOSE, "UseNonLinearAlfLuma:%d ", m_useNonLinearAlfLuma );
  msg( VERBOSE, "UseNonLinearAlfChroma:%d ", m_useNonLinearAlfChroma );
  msg( VERBOSE, "MaxNumAlfAlternativesChroma:%d ", m_maxNumAlfAlternativesChroma );
  if( m_MIP ) msg(VERBOSE, "FastMIP:%d ", m_useFastMIP);
  msg( VERBOSE, "TTFastSkip:%d ", m_ttFastSkip);
  msg( VERBOSE, "TTFastSkipThr:%.3f ", m_ttFastSkipThr);
  msg( VERBOSE, "FastLocalDualTree:%d ", m_fastLocalDualTreeMode );

  if (m_resChangeInClvsEnabled)
  {
    if (m_gopBasedRPREnabledFlag || m_rprFunctionalityTestingEnabledFlag)
    {
      msg(VERBOSE, "RPR:(%1.2lfx, %1.2lfx)|%d ", m_scalingRatioHor, m_scalingRatioVer, m_rprFunctionalityTestingEnabledFlag ? m_rprSwitchingSegmentSize : m_gopSize);
      msg(VERBOSE, "RPR2:(%1.2lfx, %1.2lfx)|%d ", m_scalingRatioHor2, m_scalingRatioVer2, m_rprFunctionalityTestingEnabledFlag ? m_rprSwitchingSegmentSize : m_gopSize);
      msg(VERBOSE, "RPR3:(%1.2lfx, %1.2lfx)|%d ", m_scalingRatioHor3, m_scalingRatioVer3, m_rprFunctionalityTestingEnabledFlag ? m_rprSwitchingSegmentSize : m_gopSize);
    }
    else
    {
      msg(VERBOSE, "RPR:(%1.2lfx, %1.2lfx)|%d ", m_scalingRatioHor, m_scalingRatioVer, m_switchPocPeriod);
    }
  }
  else
  {
    msg( VERBOSE, "RPR:%d ", 0 );
  }
  if (m_rplOfDepLayerInSh)
  {
    msg(VERBOSE, "RPLofDepLayerInSH:%d ", m_rplOfDepLayerInSh);
  }
  msg(VERBOSE, "TemporalFilter:%d/%d ", m_gopBasedTemporalFilterPastRefs, m_gopBasedTemporalFilterFutureRefs);
  msg(VERBOSE, "SEI CTI:%d ", m_ctiSEIEnabled);
  msg(VERBOSE, "BIM:%d ", m_bimEnabled);
  msg(VERBOSE, "SEI FGC:%d ", m_fgcSEIEnabled);

  msg(VERBOSE, "SEI processing Order:%d ", m_poSEIEnabled);

  msg(VERBOSE, "DPF:%d ", m_dpfEnabled);
  if (m_dpfEnabled)
  {
    msg(VERBOSE, "DPFKeyLength:%d ", m_dpfKeyLen);
    msg(VERBOSE, "DPFNonkeyLength:%d ", m_dpfNonkeyLen);
  }

#if EXTENSION_360_VIDEO
  m_ext360.outputConfigurationSummary();
#endif

  if( m_constrainedRaslEncoding )
  {
    msg(VERBOSE, "\n\nWarning: with SEIConstrainedRASL enabled, LMChroma estimation is skipped in RASL frames" );
    if( m_wrapAround )
    {
      msg(VERBOSE,   "\n         and wrap-around motion compensation is disabled in RASL frames" );
    }
  }

  msg( VERBOSE, "\n\n");

  msg( NOTICE, "\n");

  fflush( stdout );
}

bool EncAppCfg::xHasNonZeroTemporalID ()
{
  for (unsigned int i = 0; i < m_gopSize; i++)
  {
    if ( m_GOPList[i].m_temporalId != 0 )
    {
      return true;
    }
  }
  return false;
}

#if GREEN_METADATA_SEI_ENABLED
bool EncAppCfg::getGMFAUsage() {
  return m_GMFA;
}

std::string EncAppCfg::getGMFAFile (){
  return m_GMFAFile;
}

#endif

bool EncAppCfg::xHasLeadingPicture ()
{
  for (unsigned int i = 0; i < m_gopSize; i++)
  {
    for ( unsigned int j = 0; j < m_GOPList[i].m_numRefPics0; j++)
    {
      if ( m_GOPList[i].m_deltaRefPics0[j] < 0 )
      {
        return true;
      }
    }
    for ( unsigned int j = 0; j < m_GOPList[i].m_numRefPics1; j++)
    {
      if ( m_GOPList[i].m_deltaRefPics1[j] < 0 )
      {
        return true;
      }
    }
  }
  return false;
}


bool confirmPara(bool bflag, const char* message)
{
  if (!bflag)
  {
    return false;
  }

  msg( ERROR, "Error: %s\n",message);
  return true;
}



//! \}
