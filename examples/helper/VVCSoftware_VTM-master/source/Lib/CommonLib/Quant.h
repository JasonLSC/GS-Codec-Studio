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

/** \file     Quant.h
    \brief    base quantization class (header)
*/

#ifndef __QUANT__
#define __QUANT__

#include "CommonDef.h"
#include "Unit.h"
#include "ChromaFormat.h"
#include "Contexts.h"
#include "ContextModelling.h"

#include "UnitPartitioner.h"

//! \ingroup CommonLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================
struct TrQuantParams
{
  int     rightShift;
  int     qScale;
};

/// QP struct
class QpParam
{
public:
  int qps[2];
  int pers[2];
  int rems[2];

private:

  QpParam(const int           qpy,
          const ComponentID   compID,
          const int           qpBdOffset,
          const int           internalMinusInputBitDepth,
          const int           chromaQPOffset,
          const ChromaFormat  chFmt,
          const int           dqp
        , const SPS           *sps
        , const bool          applyACTQpoffset
  );

public:

  QpParam(const TransformUnit& tu, const ComponentID &compID, const int QP = -MAX_INT, const bool allowACTQpoffset = true);

  int Qp(const bool ts) const { return qps[ts ? 1 : 0]; }
  int per( const bool ts ) const { return pers[ts?1:0]; }
  int rem( const bool ts ) const { return rems[ts?1:0]; }

}; // END STRUCT DEFINITION QpParam

/// transform and quantization class
class Quant
{
public:
  Quant( const Quant* other );
  virtual ~Quant();

  // initialize class
  virtual void init( uint32_t uiMaxTrSize,
                     bool useRDOQ = false,
                     bool useRDOQTS = false,
                     bool useSelectiveRDOQ = false
                     );

public:

  void   transformSkipQuantOneSample(TransformUnit &tu, const ComponentID &compID, const TCoeff &resiDiff, TCoeff &coeff,    const uint32_t &uiPos, const QpParam &cQP, const bool bUseHalfRoundingPoint);
  void   invTrSkipDeQuantOneSample  (TransformUnit &tu, const ComponentID &compID, const TCoeff &pcCoeff,  Pel &reconSample, const uint32_t &uiPos, const QpParam &cQP);

#if RDOQ_CHROMA_LAMBDA
  void   setLambdas              ( const double lambdas[MAX_NUM_COMPONENT] )   { for (uint32_t component = 0; component < MAX_NUM_COMPONENT; component++) m_lambdas[component] = lambdas[component]; }
  void   selectLambda            ( const ComponentID compIdx )                 { m_dLambda = m_lambdas[ MAP_CHROMA(compIdx) ]; }
  void   getLambdas              ( double (&lambdas)[MAX_NUM_COMPONENT]) const { for (uint32_t component = 0; component < MAX_NUM_COMPONENT; component++) lambdas[component] = m_lambdas[component]; }
#endif
  void   setLambda               ( const double dLambda )                      { m_dLambda = dLambda; }
  double getLambda               () const                                      { return m_dLambda; }
  void   lambdaAdjustColorTrans(bool forward);
  void   resetStore() { m_resetStore = true; }

  int* getQuantCoeff             ( uint32_t list, int qp, uint32_t sizeX, uint32_t sizeY ) { return m_quantCoef            [sizeX][sizeY][list][qp]; };  //!< get Quant Coefficent
  int* getDequantCoeff           ( uint32_t list, int qp, uint32_t sizeX, uint32_t sizeY ) { return m_dequantCoef          [sizeX][sizeY][list][qp]; };  //!< get DeQuant Coefficent

  void setUseScalingList         ( bool bUseScalingList){ m_scalingListEnabledFlag = bUseScalingList; };
  bool getUseScalingList(const uint32_t width, const uint32_t height, const bool isTransformSkip, const bool lfnstApplied, const bool disableScalingMatrixForLFNSTBlks, const bool disableSMforACT)
  {
    return (m_scalingListEnabledFlag && !isTransformSkip && (!lfnstApplied || !disableScalingMatrixForLFNSTBlks) && !disableSMforACT);
  }
  void setScalingListDec         ( const ScalingList &scalingList);
  void processScalingListEnc     ( int *coeff, int *quantcoeff, int qpMod6, uint32_t height, uint32_t width, uint32_t ratio, int sizuNum, uint32_t dc);
  void processScalingListDec     ( const int *coeff, int *dequantcoeff, int qpMod6, uint32_t height, uint32_t width, uint32_t ratio, int sizuNum, uint32_t dc);

  virtual void setFlatScalingList( const int maxLog2TrDynamicRange[MAX_NUM_CHANNEL_TYPE], const BitDepths &bitDepths);
  virtual void setScalingList    ( ScalingList *scalingList, const int maxLog2TrDynamicRange[MAX_NUM_CHANNEL_TYPE], const BitDepths &bitDepths);

  // quantization
  virtual void quant(TransformUnit &tu, const ComponentID &compID, const CCoeffBuf &pSrc, TCoeff &absSum,
                     const QpParam &cQP, const Ctx &ctx);
  // de-quantization
  virtual void dequant           ( const TransformUnit &tu, CoeffBuf &dstCoeff, const ComponentID &compID, const QpParam &cQP );

protected:

  bool xNeedRDOQ                 ( TransformUnit &tu, const ComponentID &compID, const CCoeffBuf &pSrc, const QpParam &cQP );

  double   m_dLambda;
  uint32_t     m_uiMaxTrSize;
  bool     m_useRDOQ;
  bool     m_useRDOQTS;
  bool     m_useSelectiveRDOQ;
private:
  void xInitScalingList   ( const Quant* other );
  void xDestroyScalingList();
  void xSetFlatScalingList( uint32_t list, uint32_t sizeX, uint32_t sizeY, int qp );
  void xSetScalingListEnc(ScalingList *scalingList, uint32_t list, uint32_t size, int qp, uint32_t scalingListId);
  void xSetScalingListDec(const ScalingList &scalingList, uint32_t list, uint32_t size, int qp, uint32_t scalingListId);
  void xSetRecScalingListEnc(ScalingList *scalingList, uint32_t list, uint32_t sizew, uint32_t sizeh, int qp, uint32_t scalingListId);
  void xSetRecScalingListDec(const ScalingList &scalingList, uint32_t list, uint32_t sizew, uint32_t sizeh, int qp, uint32_t scalingListId);
private:
  void xSignBitHidingHDQ  (TCoeff* pQCoef, const TCoeff* pCoef, TCoeff* deltaU, const CoeffCodingContext& cctx, const int maxLog2TrDynamicRange);

private:
#if RDOQ_CHROMA_LAMBDA
  double   m_lambdas[MAX_NUM_COMPONENT];
#endif
  double   m_lambdasStore[2][MAX_NUM_COMPONENT];  // 0-org; 1-act
  bool     m_resetStore;
  bool     m_scalingListEnabledFlag;
  bool     m_isScalingListOwner;

  int      *m_quantCoef            [SCALING_LIST_SIZE_NUM][SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][SCALING_LIST_REM_NUM]; ///< array of quantization matrix coefficient 4x4
  int      *m_dequantCoef          [SCALING_LIST_SIZE_NUM][SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][SCALING_LIST_REM_NUM]; ///< array of dequantization matrix coefficient 4x4

  int      m_pairCheck;
};// END CLASS DEFINITION Quant


//! \}

#endif // __QUANT__
