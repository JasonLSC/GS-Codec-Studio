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

 /** \file     TAppMctsExtTop.h
  \brief    MCTS Extractor application class (header)
  */

#ifndef __TAPPMCTSEXTTOP__
#define __TAPPMCTSEXTTOP__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TLibDecoder/TDecTop.h"
#include "TLibEncoder/TEncTop.h"
#include "TLibCommon/AccessUnit.h"

#if MCTS_EXTRACTION
#include "TAppMctsExtCfg.h"

  //! \ingroup TAppMctsExt
  //! \{

  // ====================================================================================================================
  // Class definition
  // ====================================================================================================================

  /// MCTS Extraction application class
class TAppMctsExtTop : public TAppMctsExtCfg
{
private:
  // class interface
  TDecTop                         m_cTDecTop;                     ///< decoder class
  TComSlice*                      m_pcSlice;                      ///< slice header class
  TDecEntropy                     m_cEntropyDecoder;              ///< entropy decoder class
  TDecCavlc                       m_cCavlcDecoder;                ///< CAVLC decoder class
  TEncEntropy                     m_cEntropyCoder;                ///< entropy encoder class
  TEncCavlc                       m_cCavlcCoder;                  ///< CAVLC encoder class
  MctsExtractorTComPicSym         m_cSlicePicSym;

  Bool                            m_mctsExtractionInfoPresent;    ///< indicates whether MCTS extraction info for the traget mcts idx has been found in the bitstream

public:
  TAppMctsExtTop();
  virtual ~TAppMctsExtTop() {};


  Void  create(); ///< create internal members
  Void  destroy(); ///< destroy internal members
  Void  extract(); ///< main extracting function

protected:
  Void  xCreateMctsExtLib(); ///< create internal classes
  Void  xDestroyMctsExtLib(); ///< destroy internal classes
  Void  xInitMctsExtLib(); ///< initialize decoder class

  Void  xExtractSuitableParameterSets(SEIMessages SEIMctsSEIs, SEIMessages SEIMctsEisSEIs, AccessUnit &accessUnit); ///< search suitable EIS and extract parameter sets into AU
  Bool  xIsNaluWithinMCTSSet(Int MCTS_id); ///< check whether given Nalu belongs to MCTS SET
  Void  xInputToOutputSliceNaluConversion(InputNALUnit &inNalu, OutputNALUnit &outNalu, TComSlice* curSlice); ///< input to output nalu conversion
  Void  xWriteOutput(std::ostream& bitstreamFile, const AccessUnit accessUnit); ///< write AU into output bitstream
};

//! \}

#endif
#endif
