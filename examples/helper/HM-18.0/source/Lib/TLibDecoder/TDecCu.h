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

/** \file     TDecCu.h
    \brief    CU decoder class (header)
*/

#ifndef __TDECCU__
#define __TDECCU__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TLibCommon/TComTrQuant.h"
#include "TDecEntropy.h"
#include "TDecConformance.h"

//! \ingroup TLibDecoder
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// CU decoder class
class TDecCu
{
private:
  UInt                m_uiMaxDepth;       ///< max. number of depth
  TComYuv**           m_ppcYuvResi;       ///< array of residual buffer
  TComYuv**           m_ppcYuvReco;       ///< array of prediction & reconstruction buffer
  TComDataCU**        m_ppcCU;            ///< CU data array

  // access channel
  TComTrQuant*        m_pcTrQuant;
  TComPrediction*     m_pcPrediction;
  TDecEntropy*        m_pcEntropyDecoder;
#if MCTS_ENC_CHECK
  TDecConformanceCheck* m_pConformanceCheck;
#endif

  Bool                m_bDecodeDQP;
  Bool                m_IsChromaQpAdjCoded;

public:
  TDecCu();
  virtual ~TDecCu();

  /// initialize access channels
#if MCTS_ENC_CHECK
  Void  init                    (TDecEntropy* pcEntropyDecoder, TComTrQuant* pcTrQuant, TComPrediction* pcPrediction, TDecConformanceCheck* pConformanceCheck);
#else
  Void  init                    ( TDecEntropy* pcEntropyDecoder, TComTrQuant* pcTrQuant, TComPrediction* pcPrediction );
#endif

  /// create internal buffers
  Void  create                  ( UInt uiMaxDepth, UInt uiMaxWidth, UInt uiMaxHeight, ChromaFormat chromaFormatIDC );

  /// destroy internal buffers
  Void  destroy                 ();

  /// decode Ctu information
  Void  decodeCtu               ( TComDataCU* pCtu, Bool &isLastCtuOfSliceSegment );

  /// reconstruct Ctu information
  Void  decompressCtu           ( TComDataCU* pCtu );

protected:

  Void xDecodeCU                ( TComDataCU* const pcCU, const UInt uiAbsPartIdx, const UInt uiDepth, Bool &isLastCtuOfSliceSegment);
  Void xFinishDecodeCU          ( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth, Bool &isLastCtuOfSliceSegment);
  Bool xDecodeSliceEnd          ( TComDataCU* pcCU, UInt uiAbsPartIdx );
  Void xDecompressCU            ( TComDataCU* pCtu, UInt uiAbsPartIdx, UInt uiDepth );

  Void xReconInter              ( TComDataCU* pcCU, UInt uiDepth );

  Void xReconIntraQT            ( TComDataCU* pcCU, UInt uiDepth );
  Void xIntraRecBlk             ( TComYuv* pcRecoYuv, TComYuv* pcPredYuv, TComYuv* pcResiYuv, const ComponentID component, TComTU &rTu );
  Void xIntraRecQT              ( TComYuv* pcRecoYuv, TComYuv* pcPredYuv, TComYuv* pcResiYuv, const ChannelType chType, TComTU &rTu );

  Void xReconPCM                ( TComDataCU* pcCU, UInt uiDepth );

  Void xDecodeInterTexture      ( TComDataCU* pcCU, UInt uiDepth );
  Void xDecodePCMTexture        ( TComDataCU* pcCU, const UInt uiPartIdx, const Pel *piPCM, Pel* piReco, const UInt uiStride, const UInt uiWidth, const UInt uiHeight, const ComponentID compID);

  Void xCopyToPic               ( TComDataCU* pcCU, TComPic* pcPic, UInt uiZorderIdx, UInt uiDepth );

  Bool getdQPFlag               ()                        { return m_bDecodeDQP;        }
  Void setdQPFlag               ( Bool b )                { m_bDecodeDQP = b;           }
  Bool getIsChromaQpAdjCoded    ()                        { return m_IsChromaQpAdjCoded;        }
  Void setIsChromaQpAdjCoded    ( Bool b )                { m_IsChromaQpAdjCoded = b;           }

  Void xFillPCMBuffer           (TComDataCU* pCU, UInt depth);
};

//! \}

#endif

