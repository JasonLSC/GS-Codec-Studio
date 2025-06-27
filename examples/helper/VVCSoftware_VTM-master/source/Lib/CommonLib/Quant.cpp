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

/** \file     Quant.cpp
    \brief    transform and quantization class
*/

#include "Quant.h"

#include "UnitTools.h"
#include "ContextModelling.h"
#include "CodingStructure.h"

#include "dtrace_buffer.h"

#include <stdlib.h>
#include <limits>
#include <memory.h>



//! \ingroup CommonLib
//! \{

// ====================================================================================================================
// Constants
// ====================================================================================================================


// ====================================================================================================================
// QpParam constructor
// ====================================================================================================================

QpParam::QpParam(const int           qpy,
                 const ComponentID   compID,
                 const int           qpBdOffset,
                 const int           internalMinusInputBitDepth,
                 const int           chromaQPOffset,
                 const ChromaFormat  chFmt,
                 const int           dqp
              ,  const SPS           *sps
              , const bool           applyACTQpoffset
)
{
  int baseQp;
  if (isLuma(compID))
  {
    baseQp = qpy + qpBdOffset;
  }
  else
  {
    int qpi = Clip3(-qpBdOffset, MAX_QP, qpy);
    baseQp = sps->getMappedChromaQpValue(compID, qpi);
    baseQp = Clip3(-qpBdOffset, MAX_QP, baseQp + chromaQPOffset) + qpBdOffset;
  }

  if (applyACTQpoffset)
  {
    baseQp += DELTA_QP_ACT[compID];
  }
  baseQp = Clip3( 0, MAX_QP+qpBdOffset, baseQp + dqp );

  qps[0] = baseQp;
  pers[0]=baseQp/6;
  rems[0]=baseQp%6;

  int baseQpTS = baseQp;
  baseQpTS = std::max(baseQpTS, 4 + 6 * internalMinusInputBitDepth);

  qps[1]  = baseQpTS;
  pers[1] = baseQpTS / 6;
  rems[1] = baseQpTS % 6;
}

QpParam::QpParam(const TransformUnit& tu, const ComponentID &compIDX, const int QP /*= -MAX_INT*/, const bool allowACTQpoffset /*= true*/)
{
  int chromaQpOffset = 0;
  ComponentID compID = MAP_CHROMA(compIDX);

  if (isChroma(compID))
  {
    const bool useJQP = ( abs(TU::getICTMode(tu)) == 2 );

    chromaQpOffset += tu.cs->pps->getQpOffset            ( useJQP ? JOINT_CbCr : compID );
    chromaQpOffset += tu.cu->slice->getSliceChromaQpDelta( useJQP ? JOINT_CbCr : compID );

    chromaQpOffset += tu.cs->pps->getChromaQpOffsetListEntry( tu.cu->chromaQpAdj ).u.offset[int( useJQP ? JOINT_CbCr : compID ) - 1];
  }

  int dqp = 0;

  const bool useJQP = isChroma(compID) && (abs(TU::getICTMode(tu)) == 2);
  bool applyACTQpoffset = tu.cu->colorTransform && allowACTQpoffset;
  *this = QpParam(QP <= -MAX_INT ? tu.cu->qp : QP, useJQP ? JOINT_CbCr : compID, tu.cs->sps->getQpBDOffset(toChannelType(compID)), tu.cs->sps->getInternalMinusInputBitDepth(toChannelType(compID)), chromaQpOffset, tu.chromaFormat, dqp, tu.cs->sps, applyACTQpoffset);
}


// ====================================================================================================================
// Quant class member functions
// ====================================================================================================================

Quant::Quant( const Quant* other )
{
  xInitScalingList( other );
}

Quant::~Quant()
{
  xDestroyScalingList();
}

void invResDPCM( const TransformUnit &tu, const ComponentID &compID, CoeffBuf &dstBuf )
{
  const CompArea &rect = tu.blocks[compID];
  const int      wdt = rect.width;
  const int      hgt = rect.height;
  const CCoeffBuf coeffs = tu.getCoeffs(compID);

  const int      maxLog2TrDynamicRange = tu.cs->sps->getMaxLog2TrDynamicRange(toChannelType(compID));
  const TCoeff   inputMinimum   = -(1 << maxLog2TrDynamicRange);
  const TCoeff   inputMaximum   =  (1 << maxLog2TrDynamicRange) - 1;

  const TCoeff* coef = &coeffs.buf[0];
  TCoeff* dst = &dstBuf.buf[0];

  if (tu.cu->getBdpcmMode(compID) == BdpcmMode::HOR)
  {
    for( int y = 0; y < hgt; y++ )
    {
      dst[0] = coef[0];
      for( int x = 1; x < wdt; x++ )
      {
        dst[x] = Clip3(inputMinimum, inputMaximum, dst[x - 1] + coef[x]);
      }
      coef += coeffs.stride;
      dst += dstBuf.stride;
    }
  }
  else
  {
    for( int x = 0; x < wdt; x++ )
    {
      dst[x] = coef[x];
    }
    for( int y = 0; y < hgt - 1; y++ )
    {
      for( int x = 0; x < wdt; x++ )
      {
        dst[dstBuf.stride + x] = Clip3(inputMinimum, inputMaximum, dst[x] + coef[coeffs.stride + x]);
      }
      coef += coeffs.stride;
      dst += dstBuf.stride;
    }
  }
}

void fwdResDPCM( TransformUnit &tu, const ComponentID &compID )
{
  const CompArea &rect = tu.blocks[compID];
  const int      wdt = rect.width;
  const int      hgt = rect.height;
  CoeffBuf       coeffs = tu.getCoeffs(compID);

  TCoeff* coef = &coeffs.buf[0];

  if (tu.cu->getBdpcmMode(compID) == BdpcmMode::HOR)
  {
    for( int y = 0; y < hgt; y++ )
    {
      for( int x = wdt - 1; x > 0; x-- )
      {
        coef[x] -= coef[x - 1];
      }
      coef += coeffs.stride;
    }
  }
  else
  {
    coef += coeffs.stride * (hgt - 1);
    for( int y = 0; y < hgt - 1; y++ )
    {
      for ( int x = 0; x < wdt; x++ )
      {
        coef[x] -= coef[x - coeffs.stride];
      }
      coef -= coeffs.stride;
    }
  }
}

// To minimize the distortion only. No rate is considered.
void Quant::xSignBitHidingHDQ( TCoeff* pQCoef, const TCoeff* pCoef, TCoeff* deltaU, const CoeffCodingContext& cctx, const int maxLog2TrDynamicRange )
{
  const uint32_t width     = cctx.width();
  const uint32_t height    = cctx.height();
  const uint32_t groupSize = 1 << cctx.log2CGSize();

  const TCoeff entropyCodingMinimum = -(1 << maxLog2TrDynamicRange);
  const TCoeff entropyCodingMaximum =  (1 << maxLog2TrDynamicRange) - 1;

  int lastCG = -1;
  int absSum = 0 ;
  int n ;

  for( int subSet = (width*height-1) >> cctx.log2CGSize(); subSet >= 0; subSet-- )
  {
    int  subPos = subSet << cctx.log2CGSize();
    int  firstNZPosInCG=groupSize , lastNZPosInCG=-1 ;
    absSum = 0 ;

    for(n = groupSize-1; n >= 0; --n )
    {
      if( pQCoef[ cctx.blockPos( n + subPos ) ] )
      {
        lastNZPosInCG = n;
        break;
      }
    }

    for(n = 0; n <groupSize; n++ )
    {
      if( pQCoef[ cctx.blockPos( n + subPos ) ] )
      {
        firstNZPosInCG = n;
        break;
      }
    }

    for(n = firstNZPosInCG; n <=lastNZPosInCG; n++ )
    {
      absSum += int(pQCoef[ cctx.blockPos( n + subPos ) ]);
    }

    if(lastNZPosInCG>=0 && lastCG==-1)
    {
      lastCG = 1 ;
    }

    if( lastNZPosInCG-firstNZPosInCG>=SBH_THRESHOLD )
    {
      uint32_t signbit = (pQCoef[cctx.blockPos(subPos+firstNZPosInCG)]>0?0:1) ;
      if( signbit!=(absSum&0x1) )  //compare signbit with sum_parity
      {
        TCoeff curCost    = std::numeric_limits<TCoeff>::max();
        TCoeff minCostInc = std::numeric_limits<TCoeff>::max();
        int minPos =-1, finalChange=0, curChange=0;

        for( n = (lastCG==1?lastNZPosInCG:groupSize-1) ; n >= 0; --n )
        {
          uint32_t blkPos   = cctx.blockPos( n+subPos );
          if(pQCoef[ blkPos ] != 0 )
          {
            if(deltaU[blkPos]>0)
            {
              curCost = - deltaU[blkPos];
              curChange=1 ;
            }
            else
            {
              //curChange =-1;
              if(n==firstNZPosInCG && abs(pQCoef[blkPos])==1)
              {
                curCost = std::numeric_limits<TCoeff>::max();
              }
              else
              {
                curCost = deltaU[blkPos];
                curChange =-1;
              }
            }
          }
          else
          {
            if(n<firstNZPosInCG)
            {
              uint32_t thisSignBit = (pCoef[blkPos]>=0?0:1);
              if(thisSignBit != signbit )
              {
                curCost = std::numeric_limits<TCoeff>::max();
              }
              else
              {
                curCost = - (deltaU[blkPos])  ;
                curChange = 1 ;
              }
            }
            else
            {
              curCost = - (deltaU[blkPos])  ;
              curChange = 1 ;
            }
          }

          if( curCost<minCostInc)
          {
            minCostInc = curCost ;
            finalChange = curChange ;
            minPos = blkPos ;
          }
        } //CG loop

        if(pQCoef[minPos] == entropyCodingMaximum || pQCoef[minPos] == entropyCodingMinimum)
        {
          finalChange = -1;
        }

        if(pCoef[minPos]>=0)
        {
          pQCoef[minPos] += finalChange ;
        }
        else
        {
          pQCoef[minPos] -= finalChange ;
        }
      } // Hide
    }
    if(lastCG==1)
    {
      lastCG=0 ;
    }
  } // TU loop

  return;
}

void Quant::dequant(const TransformUnit &tu,
                             CoeffBuf      &dstCoeff,
                       const ComponentID   &compID,
                       const QpParam       &cQP)
{
  const SPS            *sps                = tu.cs->sps;
  const CompArea       &area               = tu.blocks[compID];
  const uint32_t            uiWidth            = area.width;
  const uint32_t            uiHeight           = area.height;
        TCoeff   *const piCoef             = dstCoeff.buf;
  const uint32_t            numSamplesInBlock  = uiWidth * uiHeight;
  const int             maxLog2TrDynamicRange = sps->getMaxLog2TrDynamicRange(toChannelType(compID));
  const TCoeff          transformMinimum   = -(1 << maxLog2TrDynamicRange);
  const TCoeff          transformMaximum   =  (1 << maxLog2TrDynamicRange) - 1;

  const bool isTransformSkip = tu.mtsIdx[compID] == MtsType::SKIP;

  const bool            disableSMForLFNST = tu.cs->slice->getExplicitScalingListUsed() ? tu.cs->slice->getSPS()->getDisableScalingMatrixForLfnstBlks() : false;
  const bool            isLfnstApplied = tu.cu->lfnstIdx > 0 && (tu.cu->isSepTree() ? true : isLuma(compID));
  const bool            disableSMForACT = tu.cs->slice->getSPS()->getScalingMatrixForAlternativeColourSpaceDisabledFlag() && (tu.cs->slice->getSPS()->getScalingMatrixDesignatedColourSpaceFlag() == tu.cu->colorTransform);
  const bool            enableScalingLists = getUseScalingList(uiWidth, uiHeight, isTransformSkip, isLfnstApplied, disableSMForLFNST, disableSMForACT);
  const int             scalingListType    = getScalingListType(tu.cu->predMode, compID);
  const int             channelBitDepth    = sps->getBitDepth(toChannelType(compID));

  const TCoeff          *coef;
  if (tu.cu->getBdpcmMode(compID) != BdpcmMode::NONE)
  {
    invResDPCM( tu, compID, dstCoeff );
    coef = piCoef;
  }
  else
  {
    coef = tu.getCoeffs(compID).buf;
  }
  const TCoeff          *const piQCoef = coef;
  CHECK(scalingListType >= SCALING_LIST_NUM, "Invalid scaling list");
  CHECK(uiWidth > m_uiMaxTrSize, "Unsupported transformation size");

  // Represents scaling through forward transform
  const bool clipTransformShiftTo0 =
    tu.mtsIdx[compID] != MtsType::SKIP && sps->getSpsRangeExtension().getExtendedPrecisionProcessingFlag();
  const int  originalTransformShift = getTransformShift(channelBitDepth, area.size(), maxLog2TrDynamicRange);
  const bool needSqrtAdjustment     = TU::needsBlockSizeTrafoScale( tu, compID );
  const int  iTransformShift =
    (clipTransformShiftTo0 ? std::max<int>(0, originalTransformShift) : originalTransformShift)
    + (needSqrtAdjustment ? -1 : 0);

  const int qpPer = cQP.per(isTransformSkip);
  const int qpRem = cQP.rem(isTransformSkip);

  const int rightShift = (IQUANT_SHIFT - ((isTransformSkip ? 0 : iTransformShift) + qpPer))
                         + (enableScalingLists ? LOG2_SCALING_LIST_NEUTRAL_VALUE : 0);

  if(enableScalingLists)
  {
    //from the dequantization equation:
    //iCoeffQ                         = ((Intermediate_Int(clipQCoef) * piDequantCoef[deQuantIdx]) + iAdd ) >> rightShift
    //(sizeof(Intermediate_Int) * 8)  =              inputBitDepth    +    dequantCoefBits                   - rightShift
    const uint32_t             dequantCoefBits     = 1 + IQUANT_SHIFT + SCALING_LIST_BITS;
    const uint32_t             targetInputBitDepth = std::min<uint32_t>((maxLog2TrDynamicRange + 1), (((sizeof(Intermediate_Int) * 8) + rightShift) - dequantCoefBits));

    const Intermediate_Int inputMinimum        = -(1 << (targetInputBitDepth - 1));
    const Intermediate_Int inputMaximum        =  (1 << (targetInputBitDepth - 1)) - 1;

    const uint32_t uiLog2TrWidth  = floorLog2(uiWidth);
    const uint32_t uiLog2TrHeight = floorLog2(uiHeight);
    int           *piDequantCoef  = getDequantCoeff(scalingListType, qpRem, uiLog2TrWidth, uiLog2TrHeight);

    if(rightShift > 0)
    {
      const Intermediate_Int iAdd = (Intermediate_Int) 1 << (rightShift - 1);

      for( int n = 0; n < numSamplesInBlock; n++ )
      {
        const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, piQCoef[n]));
        const Intermediate_Int iCoeffQ   = ((Intermediate_Int(clipQCoef) * piDequantCoef[n]) + iAdd ) >> rightShift;

        piCoef[n] = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
      }
    }
    else
    {
      const int leftShift = -rightShift;

      for( int n = 0; n < numSamplesInBlock; n++ )
      {
        const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, piQCoef[n]));
        const Intermediate_Int iCoeffQ   = (Intermediate_Int(clipQCoef) * piDequantCoef[n]) << leftShift;

        piCoef[n] = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
      }
    }
  }
  else
  {
    const int scale     = g_invQuantScales[needSqrtAdjustment ? 1 : 0][qpRem];
    const int scaleBits = ( IQUANT_SHIFT + 1 );

    //from the dequantisation equation:
    //iCoeffQ                         = Intermediate_Int((int64_t(clipQCoef) * scale + iAdd) >> rightShift);
    //(sizeof(Intermediate_Int) * 8)  =                    inputBitDepth   + scaleBits      - rightShift
    const uint32_t             targetInputBitDepth = std::min<uint32_t>((maxLog2TrDynamicRange + 1), (((sizeof(Intermediate_Int) * 8) + rightShift) - scaleBits));
    const Intermediate_Int inputMinimum        = -(1 << (targetInputBitDepth - 1));
    const Intermediate_Int inputMaximum        =  (1 << (targetInputBitDepth - 1)) - 1;

    if (rightShift > 0)
    {
      const Intermediate_Int iAdd = (Intermediate_Int) 1 << (rightShift - 1);

      for( int n = 0; n < numSamplesInBlock; n++ )
      {
        const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, piQCoef[n]));
        const Intermediate_Int iCoeffQ   = (Intermediate_Int(clipQCoef) * scale + iAdd) >> rightShift;

        piCoef[n] = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
      }
    }
    else
    {
      const int leftShift = -rightShift;

      for( int n = 0; n < numSamplesInBlock; n++ )
      {
        const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, piQCoef[n]));
        const Intermediate_Int iCoeffQ   = (Intermediate_Int(clipQCoef) * scale) * (Intermediate_Int(1) << leftShift);

        piCoef[n] = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
      }
    }
  }
}

void Quant::init( uint32_t uiMaxTrSize,
                  bool bUseRDOQ,
                  bool bUseRDOQTS,
                  bool useSelectiveRDOQ
                  )
{

  // TODO: pass to init() a single variable containing (quantization) flags,
  //       instead of variables that don't have to do with this class

  m_uiMaxTrSize  = uiMaxTrSize;
  m_useRDOQ      = bUseRDOQ;
  m_useRDOQTS    = bUseRDOQTS;
  m_useSelectiveRDOQ     = useSelectiveRDOQ;
  m_resetStore = true;
}

void Quant::setScalingList(ScalingList *scalingList, const int maxLog2TrDynamicRange[MAX_NUM_CHANNEL_TYPE], const BitDepths &bitDepths)
{
  const int minimumQp = 0;
  const int maximumQp = SCALING_LIST_REM_NUM;

  int scalingListId = 0;
  int recScalingListId = 0;
  for(uint32_t size = SCALING_LIST_FIRST_CODED; size <= SCALING_LIST_LAST_CODED; size++) //2x2->64x64
  {
    for(uint32_t list = 0; list < SCALING_LIST_NUM; list++)
    {
      if (size == SCALING_LIST_2x2 && list < 4)   // skip 2x2 luma
      {
        continue;
      }
      scalingListId = g_scalingListId[size][list];
      if (scalingList->getChromaScalingListPresentFlag() || scalingList->isLumaScalingList(scalingListId))
      {
        for(int qp = minimumQp; qp < maximumQp; qp++)
        {
          xSetScalingListEnc(scalingList, list, size, qp, scalingListId);
          xSetScalingListDec(*scalingList, list, size, qp, scalingListId);
        }
      }
      else // chroma QMs in 400
      {
        scalingList->processDefaultMatrix(scalingListId);
      }
    }
  }
  //based on square result and apply downsample technology
  for (uint32_t sizew = 0; sizew <= SCALING_LIST_LAST_CODED; sizew++) //7
  {
    for (uint32_t sizeh = 0; sizeh <= SCALING_LIST_LAST_CODED; sizeh++) //7
    {
      if (sizew == sizeh || (sizew == SCALING_LIST_1x1 && sizeh < SCALING_LIST_4x4)
          || (sizeh == SCALING_LIST_1x1 && sizew < SCALING_LIST_4x4))
      {
        continue;
      }
      for (uint32_t list = 0; list < SCALING_LIST_NUM; list++) //9
      {
        int largerSide = (sizew > sizeh) ? sizew : sizeh;
        if (largerSide < SCALING_LIST_4x4) printf("Rectangle Error !\n");
        recScalingListId = g_scalingListId[largerSide][list];
        for (int qp = minimumQp; qp < maximumQp; qp++)
        {
          xSetRecScalingListEnc(scalingList, list, sizew, sizeh, qp, recScalingListId);
          xSetRecScalingListDec(*scalingList, list, sizew, sizeh, qp, recScalingListId);
        }
      }
    }
  }
}
/** set quantized matrix coefficient for decode
 * \param scalingList quantized matrix address
 * \param format      chroma format
 */
void Quant::setScalingListDec(const ScalingList &scalingList)
{
  const int minimumQp = 0;
  const int maximumQp = SCALING_LIST_REM_NUM;

  int scalingListId = 0;
  int recScalingListId = 0;
  for (uint32_t size = SCALING_LIST_FIRST_CODED; size <= SCALING_LIST_LAST_CODED; size++)
  {
    for(uint32_t list = 0; list < SCALING_LIST_NUM; list++)
    {
      if (size == SCALING_LIST_2x2 && list < 4)   // skip 2x2 luma
      {
        continue;
      }
      scalingListId = g_scalingListId[size][list];
      for(int qp = minimumQp; qp < maximumQp; qp++)
      {
        xSetScalingListDec(scalingList, list, size, qp, scalingListId);
      }
    }
  }
  //based on square result and apply downsample technology
  //based on square result and apply downsample technology
  for (uint32_t sizew = 0; sizew <= SCALING_LIST_LAST_CODED; sizew++) //7
  {
    for (uint32_t sizeh = 0; sizeh <= SCALING_LIST_LAST_CODED; sizeh++) //7
    {
      if (sizew == sizeh || (sizew == SCALING_LIST_1x1 && sizeh < SCALING_LIST_4x4)
          || (sizeh == SCALING_LIST_1x1 && sizew < SCALING_LIST_4x4))
      {
        continue;
      }
      for (uint32_t list = 0; list < SCALING_LIST_NUM; list++) //9
      {
        int largerSide = (sizew > sizeh) ? sizew : sizeh;
        if (largerSide < SCALING_LIST_4x4) printf("Rectangle Error !\n");
        recScalingListId = g_scalingListId[largerSide][list];
        for (int qp = minimumQp; qp < maximumQp; qp++)
        {
          xSetRecScalingListDec(scalingList, list, sizew, sizeh, qp, recScalingListId);
        }
      }
    }
  }
}

void Quant::xSetScalingListEnc(ScalingList *scalingList, uint32_t listId, uint32_t sizeId, int qp, uint32_t scalingListId)
{
  uint32_t width  = g_scalingListSizeX[sizeId];
  uint32_t height = g_scalingListSizeX[sizeId];
  uint32_t ratio  = g_scalingListSizeX[sizeId]/std::min(MAX_MATRIX_SIZE_NUM,(int)g_scalingListSizeX[sizeId]);
  int *quantcoeff;
  int *coeff = scalingList->getScalingListAddress(scalingListId);
  quantcoeff  = getQuantCoeff(listId, qp, sizeId, sizeId);

  const bool blockIsNotPowerOf4 = ((floorLog2(width) + floorLog2(height)) & 1) == 1;
  int quantScales = g_quantScales[blockIsNotPowerOf4?1:0][qp];

  processScalingListEnc(coeff,
                        quantcoeff,
                        (quantScales << LOG2_SCALING_LIST_NEUTRAL_VALUE),
                        height, width, ratio,
                        std::min(MAX_MATRIX_SIZE_NUM, (int)g_scalingListSizeX[sizeId]),
                        scalingList->getScalingListDC(scalingListId));
}

void Quant::xSetScalingListDec(const ScalingList &scalingList, uint32_t listId, uint32_t sizeId, int qp, uint32_t scalingListId)
{
  uint32_t width  = g_scalingListSizeX[sizeId];
  uint32_t height = g_scalingListSizeX[sizeId];
  uint32_t ratio  = g_scalingListSizeX[sizeId]/std::min(MAX_MATRIX_SIZE_NUM,(int)g_scalingListSizeX[sizeId]);
  int *dequantcoeff;
  const int *coeff = scalingList.getScalingListAddress(scalingListId);

  dequantcoeff = getDequantCoeff(listId, qp, sizeId, sizeId);

  const bool blockIsNotPowerOf4 = ((floorLog2(width) + floorLog2(height)) & 1) == 1;
  int invQuantScale = g_invQuantScales[blockIsNotPowerOf4?1:0][qp];

  processScalingListDec(coeff,
                        dequantcoeff,
                        invQuantScale,
                        height, width, ratio,
                        std::min(MAX_MATRIX_SIZE_NUM, (int)g_scalingListSizeX[sizeId]),
                        scalingList.getScalingListDC(scalingListId));
}

void Quant::xSetRecScalingListEnc(ScalingList *scalingList, uint32_t listId, uint32_t sizeIdw, uint32_t sizeIdh, int qp, uint32_t scalingListId)
{
  if (sizeIdw == sizeIdh) return;

  uint32_t width = g_scalingListSizeX[sizeIdw];
  uint32_t height = g_scalingListSizeX[sizeIdh];
  uint32_t largeSideId = (sizeIdw > sizeIdh) ? sizeIdw : sizeIdh;  //16
  int *quantcoeff;
  int *coeff = scalingList->getScalingListAddress(scalingListId);//4x4, 8x8
  quantcoeff = getQuantCoeff(listId, qp, sizeIdw, sizeIdh);//final quantCoeff (downsample)
  const bool blockIsNotPowerOf4 = ((floorLog2(width) + floorLog2(height)) & 1) == 1;
  int quantScales = g_quantScales[blockIsNotPowerOf4?1:0][qp];

  processScalingListEnc(coeff,
    quantcoeff,
    (quantScales << LOG2_SCALING_LIST_NEUTRAL_VALUE),
    height, width,
    ((largeSideId>3) ? 2 : 1),
    ((largeSideId >= 3) ? 8 : 4),
    scalingList->getScalingListDC(scalingListId));
}

void Quant::xSetRecScalingListDec(const ScalingList &scalingList, uint32_t listId, uint32_t sizeIdw, uint32_t sizeIdh, int qp, uint32_t scalingListId)
{
  if (sizeIdw == sizeIdh)
  {
    return;
  }
  uint32_t width = g_scalingListSizeX[sizeIdw];
  uint32_t height = g_scalingListSizeX[sizeIdh];
  uint32_t largeSideId = (sizeIdw > sizeIdh) ? sizeIdw : sizeIdh;  //16

  const int *coeff = scalingList.getScalingListAddress(scalingListId);
  int *dequantcoeff;
  dequantcoeff = getDequantCoeff(listId, qp, sizeIdw, sizeIdh);
  const bool blockIsNotPowerOf4 = ((floorLog2(width) + floorLog2(height)) & 1) == 1;
  int invQuantScale = g_invQuantScales[blockIsNotPowerOf4 ? 1 : 0][qp];
  processScalingListDec(coeff,
                        dequantcoeff,
                        invQuantScale,
                        height, width, (largeSideId>3) ? 2 : 1,
                        (largeSideId >= 3 ? 8 : 4),
                        scalingList.getScalingListDC(scalingListId));
}
/** set flat matrix value to quantized coefficient
 */
void Quant::setFlatScalingList(const int maxLog2TrDynamicRange[MAX_NUM_CHANNEL_TYPE], const BitDepths &bitDepths)
{
  const int minimumQp = 0;
  const int maximumQp = SCALING_LIST_REM_NUM;

  for(uint32_t sizeX = 0; sizeX < SCALING_LIST_SIZE_NUM; sizeX++)
  {
    for(uint32_t sizeY = 0; sizeY < SCALING_LIST_SIZE_NUM; sizeY++)
    {
      for(uint32_t list = 0; list < SCALING_LIST_NUM; list++)
      {
        for(int qp = minimumQp; qp < maximumQp; qp++)
        {
          xSetFlatScalingList( list, sizeX, sizeY, qp );
        }
      }
    }
  }
}

/** set flat matrix value to quantized coefficient
 * \param list List ID
 * \param size size index
 * \param qp Quantization parameter
 * \param format chroma format
 */
void Quant::xSetFlatScalingList(uint32_t list, uint32_t sizeX, uint32_t sizeY, int qp)
{
  uint32_t i,num = g_scalingListSizeX[sizeX]*g_scalingListSizeX[sizeY];
  int *quantcoeff;
  int *dequantcoeff;

  const bool blockIsNotPowerOf4 = ((floorLog2(g_scalingListSizeX[sizeX]) + floorLog2(g_scalingListSizeX[sizeY])) & 1) == 1;
  int quantScales    = g_quantScales   [blockIsNotPowerOf4?1:0][qp];
  int invQuantScales = g_invQuantScales[blockIsNotPowerOf4?1:0][qp] << 4;

  quantcoeff   = getQuantCoeff(list, qp, sizeX, sizeY);
  dequantcoeff = getDequantCoeff(list, qp, sizeX, sizeY);

  for(i=0;i<num;i++)
  {
    *quantcoeff++ = quantScales;
    *dequantcoeff++ = invQuantScales;
  }
}

/** set quantized matrix coefficient for encode
 * \param coeff quantaized matrix address
 * \param quantcoeff quantaized matrix address
 * \param quantScales Q(QP%6)
 * \param height height
 * \param width width
 * \param ratio ratio for upscale
 * \param sizuNum matrix size
 * \param dc dc parameter
 */
void Quant::processScalingListEnc( int *coeff, int *quantcoeff, int quantScales, uint32_t height, uint32_t width, uint32_t ratio, int sizuNum, uint32_t dc)
{
  if (height != width)
  {
    for (uint32_t j = 0; j<height; j++)
    {
      for (uint32_t i = 0; i<width; i++)
      {
        if (j >= MAX_NONZERO_TU_SIZE || i >= MAX_NONZERO_TU_SIZE)
        {
          quantcoeff[j*width + i] = 0;
          continue;
        }
        int ratioWH = (height>width) ? height / width : width / height;
        int ratioH = (height / sizuNum) ? (height / sizuNum) : (sizuNum / height); // 32/8 = 4
        int ratioW = (width / sizuNum) ? (width / sizuNum) : (sizuNum / width); //16/8 = 2 //sizeNum = 8/4
        if (height > width)
        {
          quantcoeff[j*width + i] = quantScales / coeff[sizuNum * (j / ratioH) + ((i * ratioWH) / ratioH)];
        }
        else //ratioH < ratioW
        {
          quantcoeff[j*width + i] = quantScales / coeff[sizuNum * ((j * ratioWH) / ratioW) + (i / ratioW)];
        }
        int largeOne = (width > height) ? width : height;
        if (largeOne>8)
        {
          quantcoeff[0] = quantScales / dc;
        }
      }
    }
    return;
  }
  for(uint32_t j=0;j<height;j++)
  {
    for(uint32_t i=0;i<width;i++)
    {
      quantcoeff[j*width + i] = quantScales / coeff[sizuNum * (j / ratio) + i / ratio];
    }
  }

  if(ratio > 1)
  {
    quantcoeff[0] = quantScales / dc;
  }
}

void Quant::processScalingListDec( const int *coeff, int *dequantcoeff, int invQuantScales, uint32_t height, uint32_t width, uint32_t ratio, int sizeNum, uint32_t dc)
{
  if (height != width)
  {
    int ratioWH = (height > width  ) ? (height / width  ) : (width   / height);
    int ratioH  = (height / sizeNum) ? (height / sizeNum) : (sizeNum / height);
    int ratioW  = (width  / sizeNum) ? (width  / sizeNum) : (sizeNum / width );
    if (height > width)
    {
      for (uint32_t j = 0; j < height; j++)
      {
        int coeffLineSep        = (j / ratioH) * sizeNum;
        int dequantCoeffLineSep = j * width;
        for (uint32_t i = 0; i < width; i++)
        {
          if (i >= MAX_NONZERO_TU_SIZE || j >= MAX_NONZERO_TU_SIZE)
          {
            dequantcoeff[dequantCoeffLineSep + i] = 0;
            continue;
          }
          dequantcoeff[dequantCoeffLineSep + i] = invQuantScales * coeff[coeffLineSep + ((i * ratioWH) / ratioH)];
        }
      }
    }
    else  //ratioH < ratioW
    {
      for (uint32_t j = 0; j < height; j++)
      {
        int coeffLineSep        = ((j * ratioWH) / ratioW) * sizeNum;
        int dequantCoeffLineSep = j * width;
        for (uint32_t i = 0; i < width; i++)
        {
          if (i >= MAX_NONZERO_TU_SIZE || j >= MAX_NONZERO_TU_SIZE)
          {
            dequantcoeff[dequantCoeffLineSep + i] = 0;
            continue;
          }
          dequantcoeff[dequantCoeffLineSep + i] = invQuantScales * coeff[coeffLineSep + (i / ratioW)];
        }
      }
    }
    int largeOne = (width > height) ? width : height;
    if (largeOne > 8)
    {
      dequantcoeff[0] = invQuantScales * dc;
    }
    return;
  }
  for (uint32_t j = 0; j<height; j++)
  {
    int coeffLineSep        = (j / ratio) * sizeNum;
    int dequantCoeffLineSep = j * width;
    for (uint32_t i = 0; i<width; i++)
    {
      dequantcoeff[dequantCoeffLineSep + i] = invQuantScales * coeff[coeffLineSep + i / ratio];
    }
  }

  if (ratio > 1)
  {
    dequantcoeff[0] = invQuantScales * dc;
  }
}

/** initialization process of scaling list array
 */
void Quant::xInitScalingList( const Quant* other )
{
  m_isScalingListOwner = other == nullptr;

  size_t numElements = 0;

  for (uint32_t sizeIdX = 0; sizeIdX < SCALING_LIST_SIZE_NUM; sizeIdX++)
  {
    for (uint32_t sizeIdY = 0; sizeIdY < SCALING_LIST_SIZE_NUM; sizeIdY++)
    {
      for (uint32_t qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
      {
        for (uint32_t listId = 0; listId < SCALING_LIST_NUM; listId++)
        {
          numElements += g_scalingListSizeX[sizeIdX] * g_scalingListSizeX[sizeIdY];
        }
      }
    }
  }

  if (m_isScalingListOwner)
  {
    m_quantCoef[0][0][0][0] = new int[2 * numElements];
  }

  size_t offset = 0;

  for(uint32_t sizeIdX = 0; sizeIdX < SCALING_LIST_SIZE_NUM; sizeIdX++)
  {
    for(uint32_t sizeIdY = 0; sizeIdY < SCALING_LIST_SIZE_NUM; sizeIdY++)
    {
      for(uint32_t qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
      {
        for(uint32_t listId = 0; listId < SCALING_LIST_NUM; listId++)
        {
          if( m_isScalingListOwner )
          {
            m_quantCoef[sizeIdX][sizeIdY][listId][qp] = m_quantCoef[0][0][0][0] + offset;
            offset += g_scalingListSizeX[sizeIdX] * g_scalingListSizeX[sizeIdY];
            m_dequantCoef[sizeIdX][sizeIdY][listId][qp] = m_quantCoef[0][0][0][0] + offset;
            offset += g_scalingListSizeX[sizeIdX] * g_scalingListSizeX[sizeIdY];
          }
          else
          {
            m_quantCoef   [sizeIdX][sizeIdY][listId][qp] = other->m_quantCoef   [sizeIdX][sizeIdY][listId][qp];
            m_dequantCoef [sizeIdX][sizeIdY][listId][qp] = other->m_dequantCoef [sizeIdX][sizeIdY][listId][qp];
          }
        } // listID loop
      }
    }
  }

  m_pairCheck = 0;
}

/** destroy quantization matrix array
 */
void Quant::xDestroyScalingList()
{
  if (!m_isScalingListOwner)
  {
    return;
  }

  delete[] m_quantCoef[0][0][0][0];
}

void Quant::quant(TransformUnit &tu, const ComponentID &compID, const CCoeffBuf &pSrc, TCoeff &absSum,
                  const QpParam &cQP, const Ctx &ctx)
{
  const SPS &sps            = *tu.cs->sps;
  const CompArea &rect      = tu.blocks[compID];
  const uint32_t uiWidth        = rect.width;
  const uint32_t uiHeight       = rect.height;
  const int channelBitDepth = sps.getBitDepth(toChannelType(compID));

  const CCoeffBuf &piCoef   = pSrc;
  CoeffBuf         piQCoef  = tu.getCoeffs(compID);

  const bool useTransformSkip      = tu.mtsIdx[compID] == MtsType::SKIP;
  const int  maxLog2TrDynamicRange = sps.getMaxLog2TrDynamicRange(toChannelType(compID));

  {
    CoeffCodingContext cctx(tu, compID, tu.cs->slice->getSignDataHidingEnabledFlag(), tu.cu->getBdpcmMode(compID));
    const TCoeff       entropyCodingMinimum = -(1 << maxLog2TrDynamicRange);
    const TCoeff       entropyCodingMaximum = (1 << maxLog2TrDynamicRange) - 1;

    TCoeff deltaU[MAX_TB_SIZEY * MAX_TB_SIZEY];
    int    scalingListType = getScalingListType(tu.cu->predMode, compID);
    CHECK(scalingListType >= SCALING_LIST_NUM, "Invalid scaling list");
    const uint32_t uiLog2TrWidth  = floorLog2(uiWidth);
    const uint32_t uiLog2TrHeight = floorLog2(uiHeight);
    int       *piQuantCoeff = getQuantCoeff(scalingListType, cQP.rem(useTransformSkip), uiLog2TrWidth, uiLog2TrHeight);
    const bool disableSMForLFNST = tu.cs->slice->getExplicitScalingListUsed()
                                     ? tu.cs->slice->getSPS()->getDisableScalingMatrixForLfnstBlks()
                                     : false;
    const bool isLfnstApplied    = tu.cu->lfnstIdx > 0 && (tu.cu->isSepTree() ? true : isLuma(compID));
    const bool disableSMForACT =
      tu.cs->slice->getSPS()->getScalingMatrixForAlternativeColourSpaceDisabledFlag()
      && (tu.cs->slice->getSPS()->getScalingMatrixDesignatedColourSpaceFlag() == tu.cu->colorTransform);
    const bool enableScalingLists =
      getUseScalingList(uiWidth, uiHeight, useTransformSkip, isLfnstApplied, disableSMForLFNST, disableSMForACT);

    // for blocks that where width*height != 4^N, the effective scaling applied during transformation cannot be
    // compensated by a bit-shift (the quantised result will be sqrt(2) * larger than required).
    // The quantScale table and shift is used to compensate for this.
    const bool needSqrtAdjustment             = TU::needsBlockSizeTrafoScale(tu, compID);
    const int  defaultQuantisationCoefficient = g_quantScales[needSqrtAdjustment ? 1 : 0][cQP.rem(useTransformSkip)];
    int        iTransformShift =
      getTransformShift(channelBitDepth, rect.size(), maxLog2TrDynamicRange) + (needSqrtAdjustment ? -1 : 0);

    if (useTransformSkip && sps.getSpsRangeExtension().getExtendedPrecisionProcessingFlag())
    {
      iTransformShift = std::max<int>(0, iTransformShift);
    }

    const int iQBits = QUANT_SHIFT + cQP.per(useTransformSkip) + (useTransformSkip ? 0 : iTransformShift);
    // QBits will be OK for any internal bit depth as the reduction in transform shift is balanced by an increase
    // in Qp_per due to QpBDOffset

    const int64_t iAdd   = int64_t(tu.cs->slice->isIRAP() ? 171 : 85) << int64_t(iQBits - 9);
    const int     qBits8 = iQBits - 8;

    const uint32_t lfnstIdx = tu.cu->lfnstIdx;
    const int      maxNumberOfCoeffs =
      lfnstIdx > 0 ? (((uiWidth == 4 && uiHeight == 4) || (uiWidth == 8 && uiHeight == 8)) ? 8 : 16) : piQCoef.area();
    memset(piQCoef.buf, 0, sizeof(TCoeff) * piQCoef.area());

    const ScanElement* scan = g_scanOrder[SCAN_GROUPED_4x4][CoeffScanType::DIAG][gp_sizeIdxInfo->idxFrom(uiWidth)][gp_sizeIdxInfo->idxFrom(uiHeight)];

    for (int uiScanPos = 0; uiScanPos < maxNumberOfCoeffs; uiScanPos++)
    {
      const int    uiBlockPos = scan[uiScanPos].idx;
      const TCoeff iLevel     = piCoef.buf[uiBlockPos];
      const TCoeff iSign      = (iLevel < 0 ? -1 : 1);

      const int64_t tmpLevel =
        (int64_t) abs(iLevel) * (enableScalingLists ? piQuantCoeff[uiBlockPos] : defaultQuantisationCoefficient);

      const TCoeff quantisedMagnitude = TCoeff((tmpLevel + iAdd) >> iQBits);
      deltaU[uiBlockPos]              = (TCoeff) ((tmpLevel - ((int64_t) quantisedMagnitude << iQBits)) >> qBits8);

      absSum += quantisedMagnitude;
      const TCoeff quantisedCoefficient = quantisedMagnitude * iSign;

      piQCoef.buf[uiBlockPos] = Clip3<TCoeff>(entropyCodingMinimum, entropyCodingMaximum, quantisedCoefficient);
    } // for n
    if (cctx.bdpcm() != BdpcmMode::NONE)
    {
      fwdResDPCM(tu, compID);
    }
    if (cctx.signHiding())
    {
      if (absSum >= 2)   // this prevents TUs with only one coefficient of value 1 from being tested
      {
        xSignBitHidingHDQ(piQCoef.buf, piCoef.buf, deltaU, cctx, maxLog2TrDynamicRange);
      }
    }
  }   // if RDOQ
  // return;
}

bool Quant::xNeedRDOQ(TransformUnit &tu, const ComponentID &compID, const CCoeffBuf &pSrc, const QpParam &cQP)
{
  const SPS &sps            = *tu.cs->sps;
  const CompArea &rect      = tu.blocks[compID];
  const uint32_t uiWidth        = rect.width;
  const uint32_t uiHeight       = rect.height;
  const int channelBitDepth = sps.getBitDepth(toChannelType(compID));

  const CCoeffBuf piCoef    = pSrc;

  const bool useTransformSkip      = tu.mtsIdx[compID] == MtsType::SKIP;
  const int  maxLog2TrDynamicRange = sps.getMaxLog2TrDynamicRange(toChannelType(compID));

  int scalingListType = getScalingListType(tu.cu->predMode, compID);
  CHECK(scalingListType >= SCALING_LIST_NUM, "Invalid scaling list");

  const uint32_t uiLog2TrWidth  = floorLog2(uiWidth);
  const uint32_t uiLog2TrHeight = floorLog2(uiHeight);
  int *piQuantCoeff         = getQuantCoeff(scalingListType, cQP.rem(useTransformSkip), uiLog2TrWidth, uiLog2TrHeight);

  const bool disableSMForLFNST = tu.cs->slice->getExplicitScalingListUsed() ? tu.cs->slice->getSPS()->getDisableScalingMatrixForLfnstBlks() : false;
  const bool isLfnstApplied = tu.cu->lfnstIdx > 0 && (tu.cu->isSepTree() ? true : isLuma(compID));
  const bool disableSMForACT = tu.cs->slice->getSPS()->getScalingMatrixForAlternativeColourSpaceDisabledFlag() && (tu.cs->slice->getSPS()->getScalingMatrixDesignatedColourSpaceFlag() == tu.cu->colorTransform);
  const bool enableScalingLists = getUseScalingList(uiWidth, uiHeight, (useTransformSkip != 0), isLfnstApplied, disableSMForLFNST, disableSMForACT);

  /* for 422 chroma blocks, the effective scaling applied during transformation is not a power of 2, hence it cannot be
    * implemented as a bit-shift (the quantised result will be sqrt(2) * larger than required). Alternatively, adjust the
    * uiLog2TrSize applied in iTransformShift, such that the result is 1/sqrt(2) the required result (i.e. smaller)
    * Then a QP+3 (sqrt(2)) or QP-3 (1/sqrt(2)) method could be used to get the required result
    */
  const bool needSqrtAdjustment= TU::needsBlockSizeTrafoScale( tu, compID );
  const int defaultQuantisationCoefficient    = g_quantScales[needSqrtAdjustment?1:0][cQP.rem(useTransformSkip)];
  int iTransformShift = getTransformShift(channelBitDepth, rect.size(), maxLog2TrDynamicRange) + (needSqrtAdjustment?-1:0);

  if (useTransformSkip && sps.getSpsRangeExtension().getExtendedPrecisionProcessingFlag())
  {
    iTransformShift = std::max<int>(0, iTransformShift);
  }


  const int iQBits = QUANT_SHIFT + cQP.per(useTransformSkip) + iTransformShift;
  assert(iQBits>=0);
  // QBits will be OK for any internal bit depth as the reduction in transform shift is balanced by an increase in Qp_per due to QpBDOffset

  // iAdd is different from the iAdd used in normal quantization
  const int64_t iAdd = int64_t(compID == COMPONENT_Y ? 171 : 256) << (iQBits - 9);

  for (int uiBlockPos = 0; uiBlockPos < rect.area(); uiBlockPos++)
  {
    const TCoeff iLevel   = piCoef.buf[uiBlockPos];
    const int64_t  tmpLevel = (int64_t)abs(iLevel) * (enableScalingLists ? piQuantCoeff[uiBlockPos] : defaultQuantisationCoefficient);
    const TCoeff quantisedMagnitude = TCoeff((tmpLevel + iAdd ) >> iQBits);

    if (quantisedMagnitude != 0)
    {
      return true;
    }
  } // for n
  return false;
}


void Quant::transformSkipQuantOneSample(TransformUnit &tu, const ComponentID &compID, const TCoeff &resiDiff, TCoeff &coeff, const uint32_t &uiPos, const QpParam &cQP, const bool bUseHalfRoundingPoint)
{
  const SPS           &sps = *tu.cs->sps;
  const CompArea      &rect                           = tu.blocks[compID];
  const uint32_t           uiWidth                        = rect.width;
  const uint32_t           uiHeight                       = rect.height;
  const int            maxLog2TrDynamicRange          = sps.getMaxLog2TrDynamicRange(toChannelType(compID));
  const int            channelBitDepth                = sps.getBitDepth(toChannelType(compID));
  const int            iTransformShift                = getTransformShift(channelBitDepth, rect.size(), maxLog2TrDynamicRange);
  const int            scalingListType                = getScalingListType(tu.cu->predMode, compID);

  const bool           disableSMForLFNST = tu.cs->slice->getExplicitScalingListUsed() ? tu.cs->slice->getSPS()->getDisableScalingMatrixForLfnstBlks() : false;
  const bool           isLfnstApplied = tu.cu->lfnstIdx > 0 && (tu.cu->isSepTree() ? true : isLuma(compID));
  const bool           disableSMForACT = tu.cs->slice->getSPS()->getScalingMatrixForAlternativeColourSpaceDisabledFlag() && (tu.cs->slice->getSPS()->getScalingMatrixDesignatedColourSpaceFlag() == tu.cu->colorTransform);
  const bool           enableScalingLists = getUseScalingList(uiWidth, uiHeight, true, isLfnstApplied, disableSMForLFNST, disableSMForACT);

  const bool useTransformSkip               = tu.mtsIdx[compID] == MtsType::SKIP;
  const int  defaultQuantisationCoefficient = g_quantScales[0][cQP.rem(useTransformSkip)];

  CHECK( scalingListType >= SCALING_LIST_NUM, "Invalid scaling list" );

  const uint32_t uiLog2TrWidth      = floorLog2(uiWidth);
  const uint32_t uiLog2TrHeight     = floorLog2(uiHeight);
  const int *const piQuantCoeff = getQuantCoeff(scalingListType, cQP.rem(useTransformSkip), uiLog2TrWidth, uiLog2TrHeight);

  /* for 422 chroma blocks, the effective scaling applied during transformation is not a power of 2, hence it cannot be
  * implemented as a bit-shift (the quantised result will be sqrt(2) * larger than required). Alternatively, adjust the
  * uiLog2TrSize applied in iTransformShift, such that the result is 1/sqrt(2) the required result (i.e. smaller)
  * Then a QP+3 (sqrt(2)) or QP-3 (1/sqrt(2)) method could be used to get the required result
  */
  const int iQBits = QUANT_SHIFT + cQP.per(useTransformSkip) + (useTransformSkip ? 0 : iTransformShift);
  // QBits will be OK for any internal bit depth as the reduction in transform shift is balanced by an increase in Qp_per due to QpBDOffset
  const int64_t iAdd = int64_t(bUseHalfRoundingPoint ? 256 : (tu.cs->slice->isIRAP() ? 171 : 85))
                       << int64_t(iQBits - 9);
  TCoeff transformedCoefficient;

  // transform-skip
  if (iTransformShift >= 0)
  {
    transformedCoefficient = resiDiff << iTransformShift;
  }
  else // for very high bit depths
  {
    const int iTrShiftNeg  = -iTransformShift;
    const int offset       = 1 << (iTrShiftNeg - 1);
    transformedCoefficient = ( resiDiff + offset ) >> iTrShiftNeg;
  }

  // quantization
  const TCoeff iSign = (transformedCoefficient < 0 ? -1: 1);

  const int quantisationCoefficient = enableScalingLists ? piQuantCoeff[uiPos] : defaultQuantisationCoefficient;

  const int64_t tmpLevel = (int64_t)abs(transformedCoefficient) * quantisationCoefficient;

  const TCoeff quantisedCoefficient = (TCoeff((tmpLevel + iAdd ) >> iQBits)) * iSign;

  const TCoeff entropyCodingMinimum = -(1 << maxLog2TrDynamicRange);
  const TCoeff entropyCodingMaximum =  (1 << maxLog2TrDynamicRange) - 1;
  coeff = Clip3<TCoeff>( entropyCodingMinimum, entropyCodingMaximum, quantisedCoefficient );
}

void Quant::invTrSkipDeQuantOneSample(TransformUnit &tu, const ComponentID &compID, const TCoeff &inSample, Pel &reconSample, const uint32_t &uiPos, const QpParam &cQP)
{
  const SPS           &sps                    = *tu.cs->sps;
  const CompArea      &rect                   = tu.blocks[compID];
  const uint32_t       uiWidth                = rect.width;
  const uint32_t       uiHeight               = rect.height;
  const int            qpPer                  = cQP.per(tu.mtsIdx[compID] == MtsType::SKIP);
  const int            qpRem                  = cQP.rem(tu.mtsIdx[compID] == MtsType::SKIP);
  const int            maxLog2TrDynamicRange  = sps.getMaxLog2TrDynamicRange(toChannelType(compID));
  const int            channelBitDepth        = sps.getBitDepth(toChannelType(compID));
  const int            iTransformShift        = getTransformShift(channelBitDepth, rect.size(), maxLog2TrDynamicRange);
  const int            scalingListType        = getScalingListType(tu.cu->predMode, compID);

  const bool           disableSMForLFNST = tu.cs->slice->getExplicitScalingListUsed() ? tu.cs->slice->getSPS()->getDisableScalingMatrixForLfnstBlks() : false;
  const bool           isLfnstApplied = tu.cu->lfnstIdx > 0 && (tu.cu->isSepTree() ? true : isLuma(compID));
  const bool           disableSMForACT = tu.cs->slice->getSPS()->getScalingMatrixForAlternativeColourSpaceDisabledFlag() && (tu.cs->slice->getSPS()->getScalingMatrixDesignatedColourSpaceFlag() == tu.cu->colorTransform);
  const bool           enableScalingLists = getUseScalingList(uiWidth, uiHeight, true, isLfnstApplied, disableSMForLFNST, disableSMForACT);
  CHECK(scalingListType >= SCALING_LIST_NUM, "Invalid scaling list");

  const bool isTransformSkip = tu.mtsIdx[compID] == MtsType::SKIP;
  const int  rightShift      = (IQUANT_SHIFT - ((isTransformSkip ? 0 : iTransformShift) + qpPer))
                         + (enableScalingLists ? LOG2_SCALING_LIST_NEUTRAL_VALUE : 0);

  const TCoeff transformMinimum = -(1 << maxLog2TrDynamicRange);
  const TCoeff transformMaximum =  (1 << maxLog2TrDynamicRange) - 1;

  // De-quantisation

  TCoeff dequantisedSample;

  if (enableScalingLists)
  {
    const uint32_t             dequantCoefBits = 1 + IQUANT_SHIFT + SCALING_LIST_BITS;
    const uint32_t             targetInputBitDepth = std::min<uint32_t>((maxLog2TrDynamicRange + 1), (((sizeof(Intermediate_Int) * 8) + rightShift) - dequantCoefBits));

    const Intermediate_Int inputMinimum = -(1 << (targetInputBitDepth - 1));
    const Intermediate_Int inputMaximum =  (1 << (targetInputBitDepth - 1)) - 1;

    const uint32_t uiLog2TrWidth  = floorLog2(uiWidth);
    const uint32_t uiLog2TrHeight = floorLog2(uiHeight);
    int           *piDequantCoef  = getDequantCoeff(scalingListType, qpRem, uiLog2TrWidth, uiLog2TrHeight);

    if (rightShift > 0)
    {
      const Intermediate_Int iAdd = (Intermediate_Int) 1 << (rightShift - 1);
      const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, inSample));
      const Intermediate_Int iCoeffQ = ((Intermediate_Int(clipQCoef) * piDequantCoef[uiPos]) + iAdd) >> rightShift;

      dequantisedSample = TCoeff(Clip3<Intermediate_Int>(transformMinimum, transformMaximum, iCoeffQ));
    }
    else
    {
      const int              leftShift = -rightShift;
      const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, inSample));
      const Intermediate_Int iCoeffQ = (Intermediate_Int(clipQCoef) * piDequantCoef[uiPos]) << leftShift;

      dequantisedSample = TCoeff(Clip3<Intermediate_Int>(transformMinimum, transformMaximum, iCoeffQ));
    }
  }
  else
  {
    const int scale     = g_invQuantScales[0][qpRem];
    const int scaleBits = (IQUANT_SHIFT + 1);

    const uint32_t             targetInputBitDepth = std::min<uint32_t>((maxLog2TrDynamicRange + 1), (((sizeof(Intermediate_Int) * 8) + rightShift) - scaleBits));
    const Intermediate_Int inputMinimum = -(1 << (targetInputBitDepth - 1));
    const Intermediate_Int inputMaximum = (1 << (targetInputBitDepth - 1)) - 1;

    if (rightShift > 0)
    {
      const Intermediate_Int iAdd = (Intermediate_Int) 1 << (rightShift - 1);
      const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, inSample));
      const Intermediate_Int iCoeffQ = (Intermediate_Int(clipQCoef) * scale + iAdd) >> rightShift;

      dequantisedSample = TCoeff(Clip3<Intermediate_Int>(transformMinimum, transformMaximum, iCoeffQ));
    }
    else
    {
      const int              leftShift = -rightShift;
      const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, inSample));
      const Intermediate_Int iCoeffQ = (Intermediate_Int(clipQCoef) * scale) << leftShift;

      dequantisedSample = TCoeff(Clip3<Intermediate_Int>(transformMinimum, transformMaximum, iCoeffQ));
    }
  }

  // Inverse transform-skip
  reconSample = Pel(dequantisedSample);
}

void Quant::lambdaAdjustColorTrans(bool forward)
{
  if (m_resetStore)
  {
    for (uint8_t component = 0; component < MAX_NUM_COMPONENT; component++)
    {
      ComponentID compID = (ComponentID)component;
      int       delta_QP = DELTA_QP_ACT[compID];
      double lamdbaAdjustRate = pow(2.0, delta_QP / 3.0);

      m_lambdasStore[0][component] = m_lambdas[component];
      m_lambdasStore[1][component] = m_lambdas[component] * lamdbaAdjustRate;
    }
    m_resetStore = false;
  }

  if (forward)
  {
    CHECK(m_pairCheck == 1, "lambda has been already adjusted");
    m_pairCheck = 1;
  }
  else
  {
    CHECK(m_pairCheck == 0, "lambda has not been adjusted");
    m_pairCheck = 0;
  }

  for (uint8_t component = 0; component < MAX_NUM_COMPONENT; component++)
  {
    m_lambdas[component] = m_lambdasStore[m_pairCheck][component];
  }
}
//! \}
