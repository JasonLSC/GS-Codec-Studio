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

/** \file     DecCu.h
    \brief    CU decoder class (header)
*/

#ifndef __DECCU__
#define __DECCU__

#pragma once

#include "CABACReader.h"

#include "CommonLib/TrQuant.h"
#include "CommonLib/InterPrediction.h"
#include "CommonLib/IntraPrediction.h"
#include "CommonLib/Unit.h"
#include "CommonLib/Reshape.h"
//! \ingroup DecoderLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// CU decoder class
class DecCu
{
public:
  DecCu();
  virtual ~DecCu();

  /// initialize access channels
  void  init              ( TrQuant* pcTrQuant, IntraPrediction* pcIntra, InterPrediction* pcInter );

  /// destroy internal buffers
  void  decompressCtu     ( CodingStructure& cs, const UnitArea& ctuArea );
  Reshape*          m_pcReshape;
  Reshape* getReshape     () { return m_pcReshape; }
  void              initDecCuReshaper(Reshape* pcReshape, ChromaFormat chromaFormatIdc);
  void destoryDecCuReshaprBuf();

  /// reconstruct Ctu information
protected:
  void xIntraRecQT        ( CodingUnit&      cu, const ChannelType chType );
  void xIntraRecACTQT(CodingUnit&      cu);

  void xReconInter        ( CodingUnit&      cu );
  void xDecodeInterTexture( CodingUnit&      cu );
  void xReconIntraQT      ( CodingUnit&      cu );

  void xIntraRecBlk       ( TransformUnit&   tu, const ComponentID compID );
  void xIntraRecACTBlk(TransformUnit&   tu);
  void xDecodeInterTU     ( TransformUnit&   tu, const ComponentID compID );

  void               xDeriveCuMvs(CodingUnit &cu);
  void xReconPLT          ( CodingUnit&      cu,       ComponentID compBegin, uint32_t numComp );
  PelStorage        *m_tmpStorageCtu;

private:
  TrQuant*          m_pcTrQuant;
  IntraPrediction*  m_pcIntraPred;
  InterPrediction*  m_pcInterPred;


  MotionInfo        m_SubPuMiBuf[(MAX_CU_SIZE * MAX_CU_SIZE) >> (MIN_CU_LOG2 << 1)];

  MergeCtx          m_geoMrgCtx;
};

//! \}

#endif

