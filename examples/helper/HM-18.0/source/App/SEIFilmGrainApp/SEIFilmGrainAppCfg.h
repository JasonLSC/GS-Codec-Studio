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

/** \file     SEIFilmGrainAppCfg.h
    \brief    Decoder configuration class (header)
*/

#ifndef __SEIFILMGRAINAPPCFG__
#define __SEIFILMGRAINAPPCFG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TLibCommon/CommonDef.h"
#include <vector>

#if JVET_X0048_X0103_FILM_GRAIN
//! \ingroup SEIFilmGrainApp
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// Decoder configuration class
class SEIFilmGrainAppCfg
{
protected:
  std::string   m_bitstreamFileNameIn;                ///< output bitstream file name
  std::string   m_bitstreamFileNameOut;               ///< input bitstream file name
  Int           m_seiFilmGrainOption;                 ///< process FGC SEI option: 0:disable, 1:remove, 2:insert, 3:change
  Bool          m_seiFilmGrainPrint;                  ///< print output FGC SEI message. 1:enable
  // film grain characterstics sei
  Bool          m_fgcSEIEnabled;
  Bool          m_fgcSEIAnalysisEnabled;
  Bool          m_fgcSEICancelFlag;
  Bool          m_fgcSEIPersistenceFlag;
  Bool          m_fgcSEIPerPictureSEI;
  UInt          m_fgcSEIModelID;
  Bool          m_fgcSEISepColourDescPresentFlag;
  UInt          m_fgcSEIBlendingModeID;
  UInt          m_fgcSEILog2ScaleFactor;
  Bool          m_fgcSEICompModelPresent[MAX_NUM_COMPONENT];
  UInt          m_fgcSEINumModelValuesMinus1[MAX_NUM_COMPONENT];
  UInt          m_fgcSEINumIntensityIntervalMinus1[MAX_NUM_COMPONENT];
  UInt          m_fgcSEIIntensityIntervalLowerBound[MAX_NUM_COMPONENT][FG_MAX_NUM_INTENSITIES];
  UInt          m_fgcSEIIntensityIntervalUpperBound[MAX_NUM_COMPONENT][FG_MAX_NUM_INTENSITIES];
  UInt          m_fgcSEICompModelValue[MAX_NUM_COMPONENT][FG_MAX_NUM_INTENSITIES][FG_MAX_NUM_MODEL_VALUES];

public:
  SEIFilmGrainAppCfg();
  virtual ~SEIFilmGrainAppCfg();

  Bool  parseCfg        ( Int argc, TChar* argv[] );   ///< initialize option class from configuration
};

//! \}
#endif

#endif  // __SEIFILMGRAINAPPCFG__


