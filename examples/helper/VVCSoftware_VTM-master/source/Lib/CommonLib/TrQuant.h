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

/** \file     TrQuant.h
    \brief    transform and quantization class (header)
*/

#ifndef __TRQUANT__
#define __TRQUANT__

#include "CommonDef.h"
#include "Unit.h"
#include "ChromaFormat.h"
#include "Contexts.h"
#include "ContextModelling.h"

#include "UnitPartitioner.h"
#include "Quant.h"

#include "DepQuant.h"
//! \ingroup CommonLib
//! \{

typedef void FwdTrans(const TCoeff*, TCoeff*, int, int, int, int);
typedef void InvTrans(const TCoeff*, TCoeff*, int, int, int, int, const TCoeff, const TCoeff);

// ====================================================================================================================
// Class definition
// ====================================================================================================================

using CbfMaskList = static_vector<int, 3>;
using TrModeList  = static_vector<TrMode, NUM_TRAFO_MODES_MTS>;

/// transform and quantization class
class TrQuant
{
public:
  TrQuant();
  ~TrQuant();

  // initialize class
  void init      (
                    const Quant* otherQuant,
                    const uint32_t uiMaxTrSize,
                    const bool bUseRDOQ,
                    const bool bUseRDOQTS,
                    const bool useSelectiveRDOQ,
                    const bool bEnc
  );
  void getTrTypes(const TransformUnit tu, const ComponentID compID, TransType &trTypeHor, TransType &trTypeVer);

  void fwdLfnstNxN( TCoeff* src, TCoeff* dst, const uint32_t mode, const uint32_t index, const uint32_t size, int zeroOutSize );
  void invLfnstNxN( TCoeff* src, TCoeff* dst, const uint32_t mode, const uint32_t index, const uint32_t size, int zeroOutSize, const int maxLog2TrDynamicRange );

  uint32_t getLFNSTIntraMode( int wideAngPredMode );
  bool     getTransposeFlag ( uint32_t intraMode  );

protected:
#ifdef TARGET_SIMD_X86
  void                         initX86();
  template<X86_VEXT vext> void _initX86();
#endif

  EnumArray<std::array<FwdTrans*, NUM_TRANSFORM_MATRIX_SIZES>, TransType> m_fwdTx;
  EnumArray<std::array<InvTrans*, NUM_TRANSFORM_MATRIX_SIZES>, TransType> m_invTx;

  void xFwdLfnst( const TransformUnit &tu, const ComponentID compID, const bool loadTr = false );
  void xInvLfnst( const TransformUnit &tu, const ComponentID compID );

public:

  void invTransformNxN  (TransformUnit &tu, const ComponentID &compID, PelBuf &pResi, const QpParam &cQPs);
  void transformNxN(TransformUnit &tu, const ComponentID &compID, const QpParam &cQP, TrModeList &trModes,
                    const int maxCand);
  void transformNxN(TransformUnit &tu, const ComponentID &compID, const QpParam &cQP, TCoeff &absSum, const Ctx &ctx,
                    const bool loadTr = false);

  void transformSkipQuantOneSample(TransformUnit &tu, const ComponentID &compID, const TCoeff &resiDiff, TCoeff &coeff,    const uint32_t &uiPos, const QpParam &cQP, const bool bUseHalfRoundingPoint);
  void invTrSkipDeQuantOneSample  (TransformUnit &tu, const ComponentID &compID, const TCoeff &pcCoeff,  Pel &reconSample, const uint32_t &uiPos, const QpParam &cQP);

  void                        invTransformICT     ( const TransformUnit &tu, PelBuf &resCb, PelBuf &resCr );
  std::pair<int64_t,int64_t>  fwdTransformICT     ( const TransformUnit &tu, const PelBuf &resCb, const PelBuf &resCr, PelBuf& resC1, PelBuf& resC2, int jointCbCr = -1 );

  void selectICTCandidates(const TransformUnit &tu, CompStorage *resCb, CompStorage *resCr,
                           CbfMaskList &cbfMasksToTest);

#if RDOQ_CHROMA_LAMBDA
  void   setLambdas  ( const double lambdas[MAX_NUM_COMPONENT] )   { m_quant->setLambdas( lambdas ); }
  void   selectLambda( const ComponentID compIdx )                 { m_quant->selectLambda( compIdx ); }
  void   getLambdas  ( double (&lambdas)[MAX_NUM_COMPONENT]) const { m_quant->getLambdas( lambdas ); }
#endif
  void   setLambda   ( const double dLambda )                      { m_quant->setLambda( dLambda ); }
  double getLambda   () const                                      { return m_quant->getLambda(); }

  DepQuant* getQuant() { return m_quant; }
  void   lambdaAdjustColorTrans(bool forward) { m_quant->lambdaAdjustColorTrans(forward); }
  void   resetStore() { m_quant->resetStore(); }

protected:
  TCoeff   m_tempCoeff[MAX_TB_SIZEY * MAX_TB_SIZEY];

private:
  DepQuant *m_quant;          //!< Quantizer

  EnumArray<TCoeff[MAX_TB_SIZEY * MAX_TB_SIZEY], MtsType> m_mtsCoeffs;

  TCoeff   m_tempInMatrix [ 48 ];
  TCoeff   m_tempOutMatrix[ 48 ];
  static const int maxAbsIctMode = 3;
  void                      (*m_invICTMem[1+2*maxAbsIctMode])(PelBuf&,PelBuf&);
  std::pair<int64_t,int64_t>(*m_fwdICTMem[1+2*maxAbsIctMode])(const PelBuf&,const PelBuf&,PelBuf&,PelBuf&);
  void                      (**m_invICT)(PelBuf&,PelBuf&);
  std::pair<int64_t,int64_t>(**m_fwdICT)(const PelBuf&,const PelBuf&,PelBuf&,PelBuf&);


  // forward Transform
  void xT               (const TransformUnit &tu, const ComponentID &compID, const CPelBuf &resi, CoeffBuf &dstCoeff, const int width, const int height);

  // skipping Transform
  void xTransformSkip   (const TransformUnit &tu, const ComponentID &compID, const CPelBuf &resi, TCoeff* psCoeff);

  // quantization
  void xQuant(TransformUnit &tu, const ComponentID &compID, const CCoeffBuf &pSrc, TCoeff &absSum, const QpParam &cQP,
              const Ctx &ctx);

  // dequantization
  void xDeQuant( const TransformUnit &tu,
                       CoeffBuf      &dstCoeff,
                 const ComponentID   &compID,
                 const QpParam       &cQP      );

  // inverse transform
  void xIT     ( const TransformUnit &tu, const ComponentID &compID, const CCoeffBuf &pCoeff, PelBuf &pResidual );

  // inverse skipping transform
  void xITransformSkip(
                 const CCoeffBuf     &plCoef,
                       PelBuf        &pResidual,
                 const TransformUnit &tu,
                 const ComponentID   &component);

  void xGetCoeffEnergy(
                       TransformUnit  &tu,
                 const ComponentID    &compID,
                 const CoeffBuf       &coeffs,
                       double*        diagRatio,
                       double*        horVerRatio );
};// END CLASS DEFINITION TrQuant

//! \}

#endif // __TRQUANT__
