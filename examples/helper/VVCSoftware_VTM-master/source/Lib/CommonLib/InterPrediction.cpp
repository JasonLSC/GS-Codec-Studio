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

/** \file     Prediction.cpp
    \brief    prediction class
*/

#include "InterPrediction.h"

#include "Buffer.h"
#include "UnitTools.h"
#include "MCTS.h"

#include <memory.h>
#include <algorithm>

//! \ingroup CommonLib
//! \{

const std::array<Mv, DMVR_AREA> InterPrediction::m_dmvrSearchOffsets = {
  Mv(-2, -2), Mv(-1, -2), Mv(0, -2), Mv(1, -2), Mv(2, -2), Mv(-2, -1), Mv(-1, -1), Mv(0, -1), Mv(1, -1),
  Mv(2, -1),  Mv(-2, 0),  Mv(-1, 0), Mv(0, 0),  Mv(1, 0),  Mv(2, 0),   Mv(-2, 1),  Mv(-1, 1), Mv(0, 1),
  Mv(1, 1),   Mv(2, 1),   Mv(-2, 2), Mv(-1, 2), Mv(0, 2),  Mv(1, 2),   Mv(2, 2)
};

// ====================================================================================================================
// Constructor / destructor / initialize
// ====================================================================================================================

InterPrediction::InterPrediction()
  : m_currChromaFormat(ChromaFormat::UNDEFINED)
  , m_maxCompIDToPred(MAX_NUM_COMPONENT)
  , m_pcRdCost(nullptr)
  , m_storedMv(nullptr)
  , m_gradX0(nullptr)
  , m_gradY0(nullptr)
  , m_gradX1(nullptr)
  , m_gradY1(nullptr)
  , m_IBCBufferWidth(0)
{
  dmvrEnableEncoderCheck = false;
  for( uint32_t ch = 0; ch < MAX_NUM_COMPONENT; ch++ )
  {
    for( uint32_t refList = 0; refList < NUM_REF_PIC_LIST_01; refList++ )
    {
      m_acYuvPred[refList][ch] = nullptr;
    }
  }

  for( uint32_t c = 0; c < MAX_NUM_COMPONENT; c++ )
  {
    for( uint32_t i = 0; i < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; i++ )
    {
      for( uint32_t j = 0; j < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; j++ )
      {
        m_filteredBlock[i][j][c] = nullptr;
      }

      m_filteredBlockTmp[i][c] = nullptr;
    }
  }

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    m_yuvPredTempDmvr[l] = nullptr;
    for (uint32_t ch = 0; ch < MAX_NUM_COMPONENT; ch++)
    {
      m_refSamplesDmvr[l][ch] = nullptr;
    }
  }
}

InterPrediction::~InterPrediction()
{
  destroy();
}

void InterPrediction::destroy()
{
  for( uint32_t i = 0; i < NUM_REF_PIC_LIST_01; i++ )
  {
    for( uint32_t c = 0; c < MAX_NUM_COMPONENT; c++ )
    {
      xFree( m_acYuvPred[i][c] );
      m_acYuvPred[i][c] = nullptr;
    }
  }

  for( uint32_t c = 0; c < MAX_NUM_COMPONENT; c++ )
  {
    for( uint32_t i = 0; i < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; i++ )
    {
      for( uint32_t j = 0; j < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; j++ )
      {
        xFree( m_filteredBlock[i][j][c] );
        m_filteredBlock[i][j][c] = nullptr;
      }

      xFree( m_filteredBlockTmp[i][c] );
      m_filteredBlockTmp[i][c] = nullptr;
    }
  }

  m_geoPartBuf[0].destroy();
  m_geoPartBuf[1].destroy();
  m_colorTransResiBuf[0].destroy();
  m_colorTransResiBuf[1].destroy();
  m_colorTransResiBuf[2].destroy();

  if (m_storedMv != nullptr)
  {
    delete[]m_storedMv;
    m_storedMv = nullptr;
  }

  xFree(m_gradX0);   m_gradX0 = nullptr;
  xFree(m_gradY0);   m_gradY0 = nullptr;
  xFree(m_gradX1);   m_gradX1 = nullptr;
  xFree(m_gradY1);   m_gradY1 = nullptr;

  xFree(m_filteredBlockTmpRPR);
  m_filteredBlockTmpRPR = nullptr;

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    xFree(m_yuvPredTempDmvr[l]);
    m_yuvPredTempDmvr[l] = nullptr;

    for (uint32_t ch = 0; ch < MAX_NUM_COMPONENT; ch++)
    {
      xFree(m_refSamplesDmvr[l][ch]);
      m_refSamplesDmvr[l][ch] = nullptr;
    }
  }
  m_IBCBuffer.destroy();
}

void InterPrediction::init(RdCost* pcRdCost, ChromaFormat chromaFormatIdc, const int ctuSize)
{
  m_pcRdCost = pcRdCost;


  // if it has been initialised before, but the chroma format has changed, release the memory and start again.
  if (m_acYuvPred[REF_PIC_LIST_0][COMPONENT_Y] != nullptr && m_currChromaFormat != chromaFormatIdc)
  {
    destroy();
  }

  m_currChromaFormat = chromaFormatIdc;
  if( m_acYuvPred[REF_PIC_LIST_0][COMPONENT_Y] == nullptr ) // check if first is null (in which case, nothing initialised yet)
  {
    for( uint32_t c = 0; c < MAX_NUM_COMPONENT; c++ )
    {
      int extWidth  = MAX_CU_SIZE + std::max(2 * BIO_EXTEND_SIZE + 18, DMVR_SPAN + 15);
      int extHeight = MAX_CU_SIZE + std::max(2 * BIO_EXTEND_SIZE + 3, DMVR_SPAN);

      for( uint32_t i = 0; i < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; i++ )
      {
        m_filteredBlockTmp[i][c] = ( Pel* ) xMalloc( Pel, ( extWidth + 4 ) * ( extHeight + 7 + 4 ) );

        for( uint32_t j = 0; j < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; j++ )
        {
          m_filteredBlock[i][j][c] = ( Pel* ) xMalloc( Pel, extWidth * extHeight );
        }
      }

      // new structure
      for( uint32_t i = 0; i < NUM_REF_PIC_LIST_01; i++ )
      {
        m_acYuvPred[i][c] = ( Pel* ) xMalloc( Pel, MAX_CU_SIZE * MAX_CU_SIZE );
      }
    }

    m_geoPartBuf[0].create(UnitArea(chromaFormatIdc, Area(0, 0, MAX_CU_SIZE, MAX_CU_SIZE)));
    m_geoPartBuf[1].create(UnitArea(chromaFormatIdc, Area(0, 0, MAX_CU_SIZE, MAX_CU_SIZE)));
    m_colorTransResiBuf[0].create(UnitArea(chromaFormatIdc, Area(0, 0, MAX_CU_SIZE, MAX_CU_SIZE)));
    m_colorTransResiBuf[1].create(UnitArea(chromaFormatIdc, Area(0, 0, MAX_CU_SIZE, MAX_CU_SIZE)));
    m_colorTransResiBuf[2].create(UnitArea(chromaFormatIdc, Area(0, 0, MAX_CU_SIZE, MAX_CU_SIZE)));

    m_gradX0 = (Pel*)xMalloc(Pel, BIO_TEMP_BUFFER_SIZE);
    m_gradY0 = (Pel*)xMalloc(Pel, BIO_TEMP_BUFFER_SIZE);
    m_gradX1 = (Pel*)xMalloc(Pel, BIO_TEMP_BUFFER_SIZE);
    m_gradY1 = (Pel*)xMalloc(Pel, BIO_TEMP_BUFFER_SIZE);

    m_filteredBlockTmpRPR = (Pel *) xMalloc(Pel, TMP_RPR_WIDTH * TMP_RPR_HEIGHT);
  }

  if (m_yuvPredTempDmvr[REF_PIC_LIST_0] == nullptr)
  {
    constexpr size_t w0 = DMVR_SUBCU_WIDTH + DMVR_SPAN - 1;
    constexpr size_t h0 = DMVR_SUBCU_HEIGHT + DMVR_SPAN - 1;
    constexpr size_t w1 = DMVR_SUBCU_WIDTH + DMVR_SPAN - 1 + NTAPS_LUMA;
    constexpr size_t h1 = DMVR_SUBCU_HEIGHT + DMVR_SPAN - 1 + NTAPS_LUMA;

    for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
    {
      m_yuvPredTempDmvr[l] = (Pel *) xMalloc(Pel, w0 * h0);

      for (uint32_t ch = 0; ch < MAX_NUM_COMPONENT; ch++)
      {
        m_refSamplesDmvr[l][ch] = (Pel *) xMalloc(Pel, w1 * h1);
      }
    }
  }
#if !JVET_J0090_MEMORY_BANDWITH_MEASURE
  m_if.initInterpolationFilter( true );
#endif

  if (m_storedMv == nullptr)
  {
    m_storedMv = new Mv[MVBUFFER_SIZE*MVBUFFER_SIZE];
  }
  if (m_IBCBufferWidth != IBC_BUFFER_SIZE / ctuSize)
  {
    m_IBCBuffer.destroy();
  }
  if (m_IBCBuffer.bufs.empty())
  {
    m_IBCBufferWidth = IBC_BUFFER_SIZE / ctuSize;
    m_IBCBuffer.create(UnitArea(chromaFormatIdc, Area(0, 0, m_IBCBufferWidth, ctuSize)));
  }
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

bool InterPrediction::xCheckIdenticalMotion( const PredictionUnit &pu )
{
  const Slice &slice = *pu.cs->slice;

  if( slice.isInterB() && !pu.cs->pps->getWPBiPred() )
  {
    if( pu.refIdx[0] >= 0 && pu.refIdx[1] >= 0 )
    {
      const Picture *refPicL0 = slice.getRefPic(REF_PIC_LIST_0, pu.refIdx[0]);
      const Picture *refPicL1 = slice.getRefPic(REF_PIC_LIST_1, pu.refIdx[1]);

      if (refPicL0 == refPicL1)
      {
        if( !pu.cu->affine )
        {
          if( pu.mv[0] == pu.mv[1] )
          {
            return true;
          }
        }
        else
        {
          if (pu.mvAffi[0][0] == pu.mvAffi[1][0] && pu.mvAffi[0][1] == pu.mvAffi[1][1]
              && (pu.cu->affineType == AffineModel::_4_PARAMS || pu.mvAffi[0][2] == pu.mvAffi[1][2]))
          {
            return true;
          }
        }
      }
    }
  }

  return false;
}

void InterPrediction::xSubPuMC(PredictionUnit &pu, PelUnitBuf &predBuf, const RefPicList &eRefPicList, const bool luma,
                               const bool chroma)
{
  // compute the location of the current PU
  Position puPos    = pu.lumaPos();
  Size puSize       = pu.lumaSize();

  int numPartLine, numPartCol, puHeight, puWidth;
  {
    numPartLine = std::max(puSize.width >> ATMVP_SUB_BLOCK_SIZE, 1u);
    numPartCol = std::max(puSize.height >> ATMVP_SUB_BLOCK_SIZE, 1u);
    puHeight = numPartCol == 1 ? puSize.height : 1 << ATMVP_SUB_BLOCK_SIZE;
    puWidth = numPartLine == 1 ? puSize.width : 1 << ATMVP_SUB_BLOCK_SIZE;
  }

  PredictionUnit subPu;

  subPu.cs        = pu.cs;
  subPu.cu        = pu.cu;
  subPu.mergeType = MergeType::DEFAULT_N;

  bool isAffine = pu.cu->affine;
  subPu.cu->affine = false;

  // join sub-pus containing the same motion
  bool verMC = puSize.height > puSize.width;
  int  fstStart = (!verMC ? puPos.y : puPos.x);
  int  secStart = (!verMC ? puPos.x : puPos.y);
  int  fstEnd = (!verMC ? puPos.y + puSize.height : puPos.x + puSize.width);
  int  secEnd = (!verMC ? puPos.x + puSize.width : puPos.y + puSize.height);
  int  fstStep = (!verMC ? puHeight : puWidth);
  int  secStep = (!verMC ? puWidth : puHeight);

  bool scaled = pu.cu->slice->getRefPic( REF_PIC_LIST_0, 0 )->isRefScaled( pu.cs->sps, pu.cs->pps ) || ( pu.cs->slice->getSliceType() == B_SLICE ? pu.cu->slice->getRefPic( REF_PIC_LIST_1, 0 )->isRefScaled( pu.cs->sps, pu.cs->pps ) : false );

  for (int fstDim = fstStart; fstDim < fstEnd; fstDim += fstStep)
  {
    for (int secDim = secStart; secDim < secEnd; secDim += secStep)
    {
      int x = !verMC ? secDim : fstDim;
      int y = !verMC ? fstDim : secDim;
      const MotionInfo &curMi = pu.getMotionInfo(Position{ x, y });

      int length = secStep;
      int later  = secDim + secStep;

      while (later < secEnd)
      {
        const MotionInfo &laterMi = !verMC ? pu.getMotionInfo(Position{ later, fstDim }) : pu.getMotionInfo(Position{ fstDim, later });
        if (!scaled && laterMi == curMi)
        {
          length += secStep;
        }
        else
        {
          break;
        }
        later += secStep;
      }
      int dx = !verMC ? length : puWidth;
      int dy = !verMC ? puHeight : length;

      subPu.UnitArea::operator=(UnitArea(pu.chromaFormat, Area(x, y, dx, dy)));
      subPu = curMi;
      PelUnitBuf subPredBuf = predBuf.subBuf(UnitAreaRelative(pu, subPu));
      subPu.mmvdEncOptMode  = 0;
      motionCompensation(subPu, subPredBuf, eRefPicList, luma, chroma, nullptr, true);
      secDim = later - secStep;
    }
  }

  pu.cu->affine = isAffine;
}

void InterPrediction::xSubPuBio(PredictionUnit &pu, PelUnitBuf &predBuf, const RefPicList &eRefPicList,
                                PelUnitBuf *yuvDstTmp)
{
  // compute the location of the current PU
  Position puPos = pu.lumaPos();
  Size puSize = pu.lumaSize();

#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  JVET_J0090_SET_CACHE_ENABLE(true);
  int mvShift = (MV_FRACTIONAL_BITS_INTERNAL);
  for (int k = 0; k < NUM_REF_PIC_LIST_01; k++)
  {
    RefPicList refId = (RefPicList)k;
    const Picture* refPic = pu.cu->slice->getRefPic(refId, pu.refIdx[refId]);
    for (int compID = 0; compID < MAX_NUM_COMPONENT; compID++)
    {
      Mv cMv = pu.mv[refId];
      int mvshiftTemp = mvShift + getComponentScaleX((ComponentID)compID, pu.chromaFormat);
      int filtersize = (compID == (COMPONENT_Y)) ? NTAPS_LUMA : NTAPS_CHROMA;
      cMv += Mv(-(((filtersize >> 1) - 1) << mvshiftTemp), -(((filtersize >> 1) - 1) << mvshiftTemp));
      bool wrapRef = false;
      if ( pu.cu->slice->getRefPic(refId, pu.refIdx[refId])->isWrapAroundEnabled(pu.cs->sps, pu.cs->pps ) )
      {
        wrapRef = wrapClipMv(cMv, pu.blocks[0].pos(), pu.blocks[0].size(), pu.cs->sps, pu.cs->pps);
      }
      else
      {
        clipMv(cMv, pu.lumaPos(), pu.lumaSize(), *pu.cs->sps, *pu.cs->pps);
      }

      int width = predBuf.bufs[compID].width + (filtersize - 1);
      int height = predBuf.bufs[compID].height + (filtersize - 1);

      CPelBuf refBuf;
      Position recOffset = pu.blocks[compID].pos().offset(cMv.getHor() >> mvshiftTemp, cMv.getVer() >> mvshiftTemp);
      refBuf = refPic->getRecoBuf(CompArea((ComponentID)compID, pu.chromaFormat, recOffset, pu.blocks[compID].size()), wrapRef);

      JVET_J0090_SET_REF_PICTURE(refPic, (ComponentID)compID);
      for (int row = 0; row < height; row++)
      {
        for (int col = 0; col < width; col++)
        {
          JVET_J0090_CACHE_ACCESS(((Pel *)refBuf.buf) + row * refBuf.stride + col, __FILE__, __LINE__);
        }
      }
    }
  }
  JVET_J0090_SET_CACHE_ENABLE(false);
#endif
  PredictionUnit subPu;

  subPu.cs             = pu.cs;
  subPu.cu             = pu.cu;
  subPu.mergeType      = pu.mergeType;
  subPu.mmvdMergeFlag  = pu.mmvdMergeFlag;
  subPu.mmvdEncOptMode = pu.mmvdEncOptMode;
  subPu.mergeFlag      = pu.mergeFlag;
  subPu.ciipFlag       = pu.ciipFlag;
  subPu.refIdx[0]      = pu.refIdx[0];
  subPu.refIdx[1]      = pu.refIdx[1];

  int  fstStart = puPos.y;
  int  secStart = puPos.x;
  int  fstEnd = puPos.y + puSize.height;
  int  secEnd = puPos.x + puSize.width;
  int  fstStep = std::min((int)MAX_BDOF_APPLICATION_REGION, (int)puSize.height);
  int  secStep = std::min((int)MAX_BDOF_APPLICATION_REGION, (int)puSize.width);
  for (int fstDim = fstStart; fstDim < fstEnd; fstDim += fstStep)
  {
    for (int secDim = secStart; secDim < secEnd; secDim += secStep)
    {
      int x = secDim;
      int y = fstDim;
      int dx = secStep;
      int dy = fstStep;

      const MotionInfo &curMi = pu.getMotionInfo(Position{ x, y });

      subPu.UnitArea::operator=(UnitArea(pu.chromaFormat, Area(x, y, dx, dy)));
      subPu = curMi;
      PelUnitBuf subPredBuf = predBuf.subBuf(UnitAreaRelative(pu, subPu));

      if (yuvDstTmp)
      {
        PelUnitBuf subPredBufTmp = yuvDstTmp->subBuf(UnitAreaRelative(pu, subPu));
        motionCompensation(subPu, subPredBuf, eRefPicList, true, true, &subPredBufTmp, false);
      }
      else
      {
        motionCompensation(subPu, subPredBuf, eRefPicList);
      }
    }
  }
  JVET_J0090_SET_CACHE_ENABLE(true);
}

void InterPrediction::xPredInterUni(const PredictionUnit &pu, const RefPicList &eRefPicList, PelUnitBuf &pcYuvPred,
                                    const bool bi, const bool bioApplied, const bool luma, const bool chroma)
{
  const SPS &sps = *pu.cs->sps;

  int  refIdx = pu.refIdx[eRefPicList];
  Mv mv[3];
  bool isIBC = false;
  CHECK( !CU::isIBC( *pu.cu ) && pu.lwidth() == 4 && pu.lheight() == 4, "invalid 4x4 inter blocks" );
  if (CU::isIBC(*pu.cu))
  {
    isIBC = true;
  }
  if( pu.cu->affine )
  {
    CHECK(refIdx < 0, "refIdx incorrect.");

    mv[0] = pu.mvAffi[eRefPicList][0];
    mv[1] = pu.mvAffi[eRefPicList][1];
    mv[2] = pu.mvAffi[eRefPicList][2];
  }
  else
  {
    mv[0] = pu.mv[eRefPicList];
  }

  if( !pu.cu->affine )
  {
    if (!isIBC && pu.cu->slice->getRefPic(eRefPicList, refIdx)->isRefScaled(pu.cs->sps, pu.cs->pps) == false)
    {
      if( !pu.cs->pps->getWrapAroundEnabledFlag() )
      {
        clipMv( mv[0], pu.cu->lumaPos(), pu.cu->lumaSize(), sps, *pu.cs->pps );
      }
    }
  }

  for( uint32_t comp = COMPONENT_Y; comp < pcYuvPred.bufs.size() && comp <= m_maxCompIDToPred; comp++ )
  {
    const ComponentID compID = ComponentID( comp );
    if (compID == COMPONENT_Y && !luma)
    {
      continue;
    }
    if (compID != COMPONENT_Y && !chroma)
    {
      continue;
    }
    if ( pu.cu->affine )
    {
      CHECK(bioApplied, "BIO is not allowed with affine");
      bool genChromaMv = (!luma && chroma && compID == COMPONENT_Cb);
      xPredAffineBlk(compID, pu, pu.cu->slice->getRefPic(eRefPicList, refIdx)->unscaledPic, mv, pcYuvPred, bi,
                     pu.cu->slice->clpRng(compID), genChromaMv, pu.cu->slice->getScalingRatio(eRefPicList, refIdx));
    }
    else
    {
      if (isIBC)
      {
        xPredInterBlk(compID, pu, pu.cu->slice->getPic(), mv[0], pcYuvPred, bi, pu.cu->slice->clpRng(compID),
                      bioApplied, isIBC, eRefPicList);
      }
      else
      {
        xPredInterBlk(compID, pu, pu.cu->slice->getRefPic(eRefPicList, refIdx)->unscaledPic, mv[0], pcYuvPred, bi,
                      pu.cu->slice->clpRng(compID), bioApplied, isIBC,
                      pu.cu->slice->getScalingRatio(eRefPicList, refIdx), false, nullptr, 0,
                      isLuma(compID) && bioApplied ? m_filteredBlockTmp[2 + eRefPicList][compID] : nullptr);
      }
    }
  }
}

void InterPrediction::xPredInterBi(PredictionUnit &pu, PelUnitBuf &pcYuvPred, const bool luma, const bool chroma,
                                   PelUnitBuf *yuvPredTmp, const bool isSubPu)
{
  const PPS   &pps   = *pu.cs->pps;
  const Slice &slice = *pu.cs->slice;

  const int refIdx0 = pu.refIdx[REF_PIC_LIST_0];
  const int refIdx1 = pu.refIdx[REF_PIC_LIST_1];

  CHECK(!pu.cu->affine && refIdx0 >= 0 && refIdx1 >= 0 && pu.lwidth() + pu.lheight() == 12,
        "invalid 4x8/8x4 bi-predicted blocks");

  bool bioApplied = false;
  if (pu.cs->sps->getBDOFEnabledFlag() && !pu.cs->picHeader->getBdofDisabledFlag())
  {
    if (pu.cu->affine || isSubPu)
    {
      bioApplied = false;
    }
    else
    {
      bioApplied = PU::isSimpleSymmetricBiPred(pu) && PU::dmvrBdofSizeCheck(pu) && !pu.ciipFlag && !pu.cu->smvdMode;
    }
  }
  if (pu.mmvdEncOptMode == 2 && pu.mmvdMergeFlag)
  {
    bioApplied = false;
  }
  if (!luma)
  {
    bioApplied = false;
  }

  bool dmvrApplied = !isSubPu && PU::checkDMVRCondition(pu);

  bool refIsScaled = ( refIdx0 < 0 ? false : pu.cu->slice->getRefPic( REF_PIC_LIST_0, refIdx0 )->isRefScaled( pu.cs->sps, pu.cs->pps ) ) ||
                     ( refIdx1 < 0 ? false : pu.cu->slice->getRefPic( REF_PIC_LIST_1, refIdx1 )->isRefScaled( pu.cs->sps, pu.cs->pps ) );
  dmvrApplied = dmvrApplied && !refIsScaled;
  bioApplied = bioApplied && !refIsScaled;

  for (uint32_t refList = 0; refList < NUM_REF_PIC_LIST_01; refList++)
  {
    if( pu.refIdx[refList] < 0)
    {
      continue;
    }

    RefPicList eRefPicList = (refList ? REF_PIC_LIST_1 : REF_PIC_LIST_0);

    CHECK(CU::isIBC(*pu.cu) && eRefPicList != REF_PIC_LIST_0, "Invalid interdir for ibc mode");
    CHECK(CU::isIBC(*pu.cu) && pu.refIdx[refList] != IBC_REF_IDX, "Invalid reference index for ibc mode");
    CHECK((CU::isInter(*pu.cu) && pu.refIdx[refList] >= slice.getNumRefIdx(eRefPicList)), "Invalid reference index");

    PelUnitBuf pcMbBuf =
      (!isChromaEnabled(pu.chromaFormat) ? PelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[refList][0], pcYuvPred.Y()))
                                         : PelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[refList][0], pcYuvPred.Y()),
                                                      PelBuf(m_acYuvPred[refList][1], pcYuvPred.Cb()),
                                                      PelBuf(m_acYuvPred[refList][2], pcYuvPred.Cr())));

    if (pu.refIdx[0] >= 0 && pu.refIdx[1] >= 0)
    {
      if (dmvrApplied)
      {
        if (yuvPredTmp)
        {
          xPredInterUni(pu, eRefPicList, pcMbBuf, true, false, luma, chroma);
        }
        continue;
      }
      xPredInterUni(pu, eRefPicList, pcMbBuf, true, bioApplied, luma, chroma);
    }
    else
    {
      if( ( (pps.getUseWP() && slice.getSliceType() == P_SLICE) || (pps.getWPBiPred() && slice.getSliceType() == B_SLICE) ) )
      {
        xPredInterUni(pu, eRefPicList, pcMbBuf, true, bioApplied, luma, chroma);
      }
      else
      {
        xPredInterUni(pu, eRefPicList, pcMbBuf, pu.cu->geoFlag, bioApplied, luma, chroma);
      }
    }
  }
  CPelUnitBuf srcPred0 =
    (!isChromaEnabled(pu.chromaFormat)
       ? CPelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[0][0], pcYuvPred.Y()))
       : CPelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[0][0], pcYuvPred.Y()),
                     PelBuf(m_acYuvPred[0][1], pcYuvPred.Cb()), PelBuf(m_acYuvPred[0][2], pcYuvPred.Cr())));
  CPelUnitBuf srcPred1 =
    (!isChromaEnabled(pu.chromaFormat)
       ? CPelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[1][0], pcYuvPred.Y()))
       : CPelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[1][0], pcYuvPred.Y()),
                     PelBuf(m_acYuvPred[1][1], pcYuvPred.Cb()), PelBuf(m_acYuvPred[1][2], pcYuvPred.Cr())));
  const bool lumaOnly   = luma && !chroma;
  const bool chromaOnly = !luma && chroma;
  if (!pu.cu->geoFlag && (!dmvrApplied) && (!bioApplied) && pps.getWPBiPred() && slice.getSliceType() == B_SLICE
      && pu.cu->bcwIdx == BCW_DEFAULT)
  {
    xWeightedPredictionBi( pu, srcPred0, srcPred1, pcYuvPred, m_maxCompIDToPred, lumaOnly, chromaOnly );
    if (yuvPredTmp)
    {
      yuvPredTmp->copyFrom(pcYuvPred);
    }
  }
  else if( !pu.cu->geoFlag && pps.getUseWP() && slice.getSliceType() == P_SLICE )
  {
    xWeightedPredictionUni( pu, srcPred0, REF_PIC_LIST_0, pcYuvPred, -1, m_maxCompIDToPred, lumaOnly, chromaOnly );
    if (yuvPredTmp)
    {
      yuvPredTmp->copyFrom(pcYuvPred);
    }
  }
  else
  {
    if (dmvrApplied)
    {
      if (yuvPredTmp)
      {
        yuvPredTmp->addAvg(srcPred0, srcPred1, slice.clpRngs(), false);
      }
      xProcessDMVR(pu, pcYuvPred, slice.clpRngs(), bioApplied);
    }
    else
    {
      xWeightedAverage(pu, srcPred0, srcPred1, pcYuvPred, slice.getSPS()->getBitDepths(), slice.clpRngs(), bioApplied,
                       lumaOnly, chromaOnly, yuvPredTmp);
    }
  }
}

void InterPrediction::xPredInterBlk(const ComponentID compID, const PredictionUnit &pu, const Picture *refPic,
                                    const Mv &_mv, PelUnitBuf &dstPic, const bool bi, const ClpRng &clpRng,
                                    const bool bioApplied, bool isIBC, const ScalingRatio scalingRatio,
                                    bool bilinearMC, Pel *srcPadBuf, ptrdiff_t srcPadStride, Pel *bdofDstBuf)
{
  JVET_J0090_SET_REF_PICTURE( refPic, compID );
  const ChromaFormat  chFmt = pu.chromaFormat;
  const bool          rndRes = !bi;

  int shiftHor = MV_FRACTIONAL_BITS_INTERNAL + ::getComponentScaleX(compID, chFmt);
  int shiftVer = MV_FRACTIONAL_BITS_INTERNAL + ::getComponentScaleY(compID, chFmt);

  bool  wrapRef = false;
  Mv    mv(_mv);
  if( !isIBC && refPic->isWrapAroundEnabled(pu.cs->sps, pu.cs->pps ) )
  {
    wrapRef = wrapClipMv( mv, pu.blocks[0].pos(), pu.blocks[0].size(), pu.cs->sps, pu.cs->pps );
  }

  bool useAltHpelIf = pu.cu->imv == IMV_HPEL;

  if (!isIBC
      && xPredInterBlkRPR(scalingRatio, *pu.cs->sps, *pu.cs->pps, CompArea(compID, chFmt, pu.blocks[compID], dstPic.bufs[compID]),
                          refPic, mv, dstPic.bufs[compID].buf, dstPic.bufs[compID].stride, bi, wrapRef, clpRng,
                          InterpolationFilter::Filter::DEFAULT, useAltHpelIf))
  {
    CHECK( bilinearMC, "DMVR should be disabled with RPR" );
    CHECK( bioApplied, "BDOF should be disabled with RPR" );
  }
  else
  {
    int xFrac, yFrac;
    if (isIBC)
    {
      xFrac = yFrac = 0;
      JVET_J0090_SET_CACHE_ENABLE(false);
    }
    else if (isLuma(compID))
    {
      xFrac = mv.hor & 15;
      yFrac = mv.ver & 15;
    }
    else
    {
      xFrac = mv.hor * (1 << (1 - ::getComponentScaleX(compID, chFmt))) & 31;
      yFrac = mv.ver * (1 << (1 - ::getComponentScaleY(compID, chFmt))) & 31;
    }

    PelBuf & dstBuf = dstPic.bufs[compID];
    const unsigned width  = dstBuf.width;
    const unsigned height = dstBuf.height;

    CPelBuf refBuf;
    {
      Position offset = pu.blocks[compID].pos().offset(mv.getHor() >> shiftHor, mv.getVer() >> shiftVer);
      refBuf          = refPic->getRecoBuf(CompArea(compID, chFmt, offset, pu.blocks[compID].size()), wrapRef);
    }

    if (nullptr != srcPadBuf)
    {
      refBuf.buf    = srcPadBuf;
      refBuf.stride = srcPadStride;
    }
    // backup data
    Pel *backupDstBufPtr    = dstBuf.buf;
    ptrdiff_t backupDstBufStride = dstBuf.stride;

    if (nullptr != bdofDstBuf)
    {
      // change MC output
      dstBuf.stride = width + 2 * BIO_EXTEND_SIZE + 2;
      dstBuf.buf    = bdofDstBuf + 2 * dstBuf.stride + 2;
    }

    const auto filterIdx =
      bilinearMC ? InterpolationFilter::Filter::DMVR
                 : (useAltHpelIf ? InterpolationFilter::Filter::HALFPEL_ALT : InterpolationFilter::Filter::DEFAULT);

    if (yFrac == 0)
    {
      m_if.filterHor(compID, refBuf.buf, refBuf.stride, dstBuf.buf, dstBuf.stride, width, height, xFrac, rndRes, clpRng,
                     filterIdx);
    }
    else if (xFrac == 0)
    {
      m_if.filterVer(compID, refBuf.buf, refBuf.stride, dstBuf.buf, dstBuf.stride, width, height, yFrac, true, rndRes,
                     clpRng, filterIdx);
    }
    else
    {
      PelBuf tmpBuf = PelBuf(m_filteredBlockTmp[0][compID], dstBuf.stride, pu.blocks[compID]);

      const int filterSize = bilinearMC ? NTAPS_BILINEAR : (isLuma(compID) ? NTAPS_LUMA : NTAPS_CHROMA);
      const int margin     = (filterSize >> 1) - 1;

      m_if.filterHor(compID, refBuf.bufAt(0, -margin), refBuf.stride, tmpBuf.buf, tmpBuf.stride, width,
                     height + filterSize - 1, xFrac, false, clpRng, filterIdx);
      JVET_J0090_SET_CACHE_ENABLE(false);
      m_if.filterVer(compID, tmpBuf.bufAt(0, margin), tmpBuf.stride, dstBuf.buf, dstBuf.stride, width, height, yFrac,
                     false, rndRes, clpRng, filterIdx);
    }
    // Enabled only in non-DMVR-non-BDOF process, In DMVR process, srcPadStride is always non-zero
    JVET_J0090_SET_CACHE_ENABLE(srcPadStride == 0 && !bioApplied);

    if (bioApplied && compID == COMPONENT_Y)
    {
      const int shift = IF_INTERNAL_FRAC_BITS(clpRng.bd);
      int        xOffset = (xFrac < 8) ? 1 : 0;
      int        yOffset = (yFrac < 8) ? 1 : 0;
      const Pel *refPel  = refBuf.buf - yOffset * refBuf.stride - xOffset;
      Pel       *dstPel  = bdofDstBuf + dstBuf.stride + 1;
      for (int w = 0; w < width + 2; w++)
      {
        Pel val   = leftShift_round(refPel[w], shift);
        dstPel[w] = val - (Pel) IF_INTERNAL_OFFS;
      }

      refPel = refBuf.buf + (1 - yOffset) * refBuf.stride - xOffset;
      dstPel = bdofDstBuf + 2 * dstBuf.stride + 1;
      for (int h = 0; h < height; h++)
      {
        const ptrdiff_t idxLeft  = 0;
        const ptrdiff_t idxRight = width + 2 * BIO_EXTEND_SIZE - 1;

        dstPel[idxLeft]  = leftShift_round(refPel[idxLeft], shift) - (Pel) IF_INTERNAL_OFFS;
        dstPel[idxRight] = leftShift_round(refPel[idxRight], shift) - (Pel) IF_INTERNAL_OFFS;

        refPel += refBuf.stride;
        dstPel += dstBuf.stride;
      }

      refPel = refBuf.buf + (height + 1 - yOffset) * refBuf.stride - xOffset;
      dstPel = bdofDstBuf + (height + 2) * dstBuf.stride + 1;
      for (int w = 0; w < width + 2; w++)
      {
        Pel val   = leftShift_round(refPel[w], shift);
        dstPel[w] = val - (Pel) IF_INTERNAL_OFFS;
      }

      // restore data
      dstBuf.buf    = backupDstBufPtr;
      dstBuf.stride = backupDstBufStride;
    }
  }
}

bool InterPrediction::isSubblockVectorSpreadOverLimit( int a, int b, int c, int d, int predType )
{
  int s4 = ( 4 << 11 );
  int filterTap = 6;

  if ( predType == 3 )
  {
    int refBlkWidth  = std::max( std::max( 0, 4 * a + s4 ), std::max( 4 * c, 4 * a + 4 * c + s4 ) ) - std::min( std::min( 0, 4 * a + s4 ), std::min( 4 * c, 4 * a + 4 * c + s4 ) );
    int refBlkHeight = std::max( std::max( 0, 4 * b ), std::max( 4 * d + s4, 4 * b + 4 * d + s4 ) ) - std::min( std::min( 0, 4 * b ), std::min( 4 * d + s4, 4 * b + 4 * d + s4 ) );
    refBlkWidth  = ( refBlkWidth >> 11 ) + filterTap + 3;
    refBlkHeight = ( refBlkHeight >> 11 ) + filterTap + 3;

    if ( refBlkWidth * refBlkHeight > ( filterTap + 9 ) * ( filterTap + 9 ) )
    {
      return true;
    }
  }
  else
  {
    int refBlkWidth  = std::max( 0, 4 * a + s4 ) - std::min( 0, 4 * a + s4 );
    int refBlkHeight = std::max( 0, 4 * b ) - std::min( 0, 4 * b );
    refBlkWidth  = ( refBlkWidth >> 11 ) + filterTap + 3;
    refBlkHeight = ( refBlkHeight >> 11 ) + filterTap + 3;
    if ( refBlkWidth * refBlkHeight > ( filterTap + 9 ) * ( filterTap + 5 ) )
    {
      return true;
    }

    refBlkWidth  = std::max( 0, 4 * c ) - std::min( 0, 4 * c );
    refBlkHeight = std::max( 0, 4 * d + s4 ) - std::min( 0, 4 * d + s4 );
    refBlkWidth  = ( refBlkWidth >> 11 ) + filterTap + 3;
    refBlkHeight = ( refBlkHeight >> 11 ) + filterTap + 3;
    if ( refBlkWidth * refBlkHeight > ( filterTap + 5 ) * ( filterTap + 9 ) )
    {
      return true;
    }
  }
  return false;
}

#if GDR_ENABLED
bool InterPrediction::xPredAffineBlk(const ComponentID &compID, const PredictionUnit &pu, const Picture *refPic,
                                     const Mv *_mv, PelUnitBuf &dstPic, const bool bi, const ClpRng &clpRng,
                                     bool genChromaMv, const ScalingRatio scalingRatio)
#else
void InterPrediction::xPredAffineBlk(const ComponentID &compID, const PredictionUnit &pu, const Picture *refPic,
                                     const Mv *_mv, PelUnitBuf &dstPic, const bool bi, const ClpRng &clpRng,
                                     bool genChromaMv, const ScalingRatio scalingRatio)
#endif
{
  JVET_J0090_SET_REF_PICTURE(refPic, compID);

  const ChromaFormat &chFmt = pu.chromaFormat;

  const CodingStructure &cs  = *pu.cs;
  const SPS &            sps = *cs.sps;
  const PPS &            pps = *cs.pps;
  const PicHeader &      ph  = *cs.picHeader;

#if GDR_ENABLED
  bool allOk = true;
  const bool isEncodeGdrClean = sps.getGDREnabledFlag() && cs.pcv->isEncoder
                                && ((cs.picture->gdrParam.inGdrInterval && cs.isClean(pu.Y().topRight(), ChannelType::LUMA))
                                    || cs.picture->gdrParam.verBoundary == -1);
  const int pux = pu.lx();
  const int puy = pu.ly();
#endif

  const int widthLuma  = pu.Y().width;
  const int heightLuma = pu.Y().height;

  const int sbWidth  = AFFINE_SUBBLOCK_SIZE;
  const int sbHeight = AFFINE_SUBBLOCK_SIZE;

  const bool isRefScaled = refPic->isRefScaled(&sps, &pps);

  const int dstExtW = (sbWidth + PROF_BORDER_EXT_W * 2 + 7) & ~7;
  const int dstExtH = sbHeight + PROF_BORDER_EXT_H * 2;
  PelBuf    dstExtBuf(m_filteredBlockTmp[1][compID], dstExtW, dstExtH);

  const int refExtH = dstExtH + MAX_FILTER_SIZE - 1;
  PelBuf    tmpBuf  = PelBuf(m_filteredBlockTmp[0][compID], dstExtW, refExtH);

  PelBuf &dstBuf = dstPic.bufs[compID];

  const bool wrapAroundEnabled = refPic->isWrapAroundEnabled(&sps, &pps);

  int dmvHorX = 0;
  int dmvHorY = 0;
  int dmvVerX = 0;
  int dmvVerY = 0;

  constexpr int PREC = MAX_CU_DEPTH;

  bool enableProfTmp = compID == COMPONENT_Y && !ph.getProfDisabledFlag() && !isRefScaled && !m_skipProf;

  if (compID == COMPONENT_Y || genChromaMv || chFmt == ChromaFormat::_444)
  {
    const Mv &mvLT = _mv[0];
    const Mv &mvRT = _mv[1];
    const Mv &mvLB = _mv[2];

    dmvHorX = (mvRT - mvLT).getHor() * (1 << (PREC - floorLog2(widthLuma)));
    dmvHorY = (mvRT - mvLT).getVer() * (1 << (PREC - floorLog2(widthLuma)));
    if (pu.cu->affineType == AffineModel::_6_PARAMS)
    {
      dmvVerX = (mvLB - mvLT).getHor() * (1 << (PREC - floorLog2(heightLuma)));
      dmvVerY = (mvLB - mvLT).getVer() * (1 << (PREC - floorLog2(heightLuma)));
    }
    else
    {
      dmvVerX = -dmvHorY;
      dmvVerY = dmvHorX;
    }

    if (dmvHorX == 0 && dmvHorY == 0 && dmvVerX == 0 && dmvVerY == 0)
    {
      enableProfTmp = false;
    }

    const bool largeMvGradient = isSubblockVectorSpreadOverLimit(dmvHorX, dmvHorY, dmvVerX, dmvVerY, pu.interDir);
    if (largeMvGradient)
    {
      enableProfTmp = false;
    }

    const int baseHor = mvLT.getHor() * (1 << PREC);
    const int baseVer = mvLT.getVer() * (1 << PREC);

    for (int h = 0; h < heightLuma; h += sbHeight)
    {
      for (int w = 0; w < widthLuma; w += sbWidth)
      {
        const int weightHor = largeMvGradient ? widthLuma >> 1 : (sbWidth >> 1) + w;
        const int weightVer = largeMvGradient ? heightLuma >> 1 : (sbHeight >> 1) + h;

        Mv tmpMv;

        tmpMv.hor = baseHor + dmvHorX * weightHor + dmvVerX * weightVer;
        tmpMv.ver = baseVer + dmvHorY * weightHor + dmvVerY * weightVer;

        tmpMv >>= PREC - 4 + MV_FRACTIONAL_BITS_INTERNAL;
        tmpMv.clipToStorageBitDepth();

        m_storedMv[h / AFFINE_SUBBLOCK_SIZE * MVBUFFER_SIZE + w / AFFINE_SUBBLOCK_SIZE] = tmpMv;
      }
    }

    if (enableProfTmp && m_skipProfCond)
    {
      const int profThres = (m_biPredSearchAffine ? 2 : 1) << PREC;

      if (abs(dmvHorX) <= profThres && abs(dmvHorY) <= profThres && abs(dmvVerX) <= profThres
          && abs(dmvVerY) <= profThres)
      {
        // Skip PROF during search when the adjustment is expected to be small
        enableProfTmp = false;
      }
    }
  }

  const bool enableProf = enableProfTmp;
  const bool isLast     = !enableProf && !bi;

  int dMvScaleHor[AFFINE_SUBBLOCK_SIZE * AFFINE_SUBBLOCK_SIZE];
  int dMvScaleVer[AFFINE_SUBBLOCK_SIZE * AFFINE_SUBBLOCK_SIZE];

  if (enableProf)
  {
    for (int y = 0; y < sbHeight; y++)
    {
      for (int x = 0; x < sbWidth; x++)
      {
        const int wx = 2 * x - (sbWidth - 1);
        const int wy = 2 * y - (sbHeight - 1);

        dMvScaleHor[y * sbWidth + x] = wx * dmvHorX + wy * dmvVerX;
        dMvScaleVer[y * sbWidth + x] = wx * dmvHorY + wy * dmvVerY;
      }
    }

    // NOTE: the shift value is 7 and not 8 as in section 8.5.5.9 of the spec because
    // the values dMvScaleHor/dMvScaleVer are half of diffMvLX (which are always even
    // in equations 876 and 877)
    const int mvShift  = 7;
    const int dmvLimit = (1 << 5) - 1;

    const int sz = sbWidth * sbHeight;

    if (!g_pelBufOP.roundIntVector)
    {
      for (int idx = 0; idx < sz; idx++)
      {
        Mv tmpMv(dMvScaleHor[idx], dMvScaleVer[idx]);
        tmpMv >>= 7;
        dMvScaleHor[idx] = Clip3(-dmvLimit, dmvLimit, tmpMv.getHor());
        dMvScaleVer[idx] = Clip3(-dmvLimit, dmvLimit, tmpMv.getVer());
      }
    }
    else
    {
      g_pelBufOP.roundIntVector(dMvScaleHor, sz, mvShift, dmvLimit);
      g_pelBufOP.roundIntVector(dMvScaleVer, sz, mvShift, dmvLimit);
    }
  }

  // get prediction block by block
  const int scaleX = ::getComponentScaleX(compID, chFmt);
  const int scaleY = ::getComponentScaleY(compID, chFmt);

  const int width  = widthLuma >> scaleX;
  const int height = heightLuma >> scaleY;

  CHECK(sbWidth > width, "Subblock width > block width");
  CHECK(sbHeight > height, "Subblock height > block height");

  for (int h = 0; h < height; h += sbHeight)
  {
    for (int w = 0; w < width; w += sbWidth)
    {
      Mv curMv;

      const int hLuma = h << scaleY;
      const int wLuma = w << scaleX;

      const ptrdiff_t idx = hLuma / AFFINE_SUBBLOCK_SIZE * MVBUFFER_SIZE + wLuma / AFFINE_SUBBLOCK_SIZE;

      if (compID == COMPONENT_Y || chFmt == ChromaFormat::_444)
      {
        curMv = m_storedMv[idx];
      }
      else
      {
        curMv = m_storedMv[idx] + m_storedMv[idx + scaleY * MVBUFFER_SIZE + scaleX];
        curMv >>= 1;
      }

      bool wrapRef = false;

      if (wrapAroundEnabled)
      {
        wrapRef = wrapClipMv(curMv, Position(pu.Y().x + wLuma, pu.Y().y + hLuma),
                             Size(sbWidth << scaleX, sbHeight << scaleY), &sps, &pps);
      }
      else if (!isRefScaled)
      {
        clipMv(curMv, pu.lumaPos(), pu.lumaSize(), sps, pps);
      }

#if GDR_ENABLED
      if (isEncodeGdrClean)
      {
        const Position subPuPos = Position(pux + ((w + sbWidth) << scaleX), puy + ((h + sbHeight) << scaleY));

        const bool puClean = cs.isClean(subPuPos, curMv, refPic);

        allOk = allOk && puClean;
      }
#endif

      const auto filterIdx = InterpolationFilter::Filter::AFFINE;

      if( isRefScaled )
      {
        CHECK(enableProf, "PROF should be disabled with RPR");
        xPredInterBlkRPR(scalingRatio, sps, pps,
                         CompArea(compID, chFmt, pu.blocks[compID].offset(w, h), Size(sbWidth, sbHeight)), refPic,
                         curMv, dstBuf.buf + w + h * dstBuf.stride, dstBuf.stride, bi, wrapRef, clpRng, filterIdx);
      }
      else
      {
        // get the MV in high precision
        int xFrac, yFrac, xInt, yInt;

        if (isLuma(compID))
        {
          xInt  = curMv.getHor() >> MV_FRAC_BITS_LUMA;
          xFrac = curMv.getHor() & MV_FRAC_MASK_LUMA;
          yInt  = curMv.getVer() >> MV_FRAC_BITS_LUMA;
          yFrac = curMv.getVer() & MV_FRAC_MASK_LUMA;
        }
        else
        {
          xInt  = curMv.getHor() * (1 << (1 - scaleX)) >> MV_FRAC_BITS_CHROMA;
          xFrac = curMv.getHor() * (1 << (1 - scaleX)) & MV_FRAC_MASK_CHROMA;
          yInt  = curMv.getVer() * (1 << (1 - scaleY)) >> MV_FRAC_BITS_CHROMA;
          yFrac = curMv.getVer() * (1 << (1 - scaleY)) & MV_FRAC_MASK_CHROMA;
        }

        const CPelBuf refBuf = refPic->getRecoBuf(
          CompArea(compID, chFmt, pu.blocks[compID].offset(xInt + w, yInt + h), pu.blocks[compID]), wrapRef);

        const Pel *ref       = refBuf.buf;
        const ptrdiff_t refStride = refBuf.stride;

        Pel *dst =
          enableProf ? dstExtBuf.bufAt(PROF_BORDER_EXT_W, PROF_BORDER_EXT_H) : dstBuf.buf + w + h * dstBuf.stride;
        const ptrdiff_t dstStride = enableProf ? dstExtBuf.stride : dstBuf.stride;

        if (yFrac == 0)
        {
          m_if.filterHor(compID, ref, refStride, dst, dstStride, sbWidth, sbHeight, xFrac, isLast, clpRng, filterIdx);
        }
        else if (xFrac == 0)
        {
          m_if.filterVer(compID, ref, refStride, dst, dstStride, sbWidth, sbHeight, yFrac, true, isLast, clpRng,
                         filterIdx);
        }
        else
        {
          const int filterSize = isLuma(compID) ? NTAPS_LUMA_AFFINE : NTAPS_CHROMA_AFFINE;
          const int rowsAbove  = (filterSize - 1) >> 1;

          m_if.filterHor(compID, ref - rowsAbove * refStride, refStride, tmpBuf.buf, tmpBuf.stride, sbWidth,
                         sbHeight + filterSize - 1, xFrac, false, clpRng, filterIdx);
          JVET_J0090_SET_CACHE_ENABLE(false);
          m_if.filterVer(compID, tmpBuf.buf + rowsAbove * tmpBuf.stride, tmpBuf.stride, dst, dstStride, sbWidth,
                         sbHeight, yFrac, false, isLast, clpRng, filterIdx);
          JVET_J0090_SET_CACHE_ENABLE(true);
        }

        if (enableProf)
        {
          const int shift = IF_INTERNAL_FRAC_BITS(clpRng.bd);
          CHECKD(shift < 0, "shift must be positive");
          const int xOffset = xFrac >> (MV_FRAC_BITS_LUMA - 1);
          const int yOffset = yFrac >> (MV_FRAC_BITS_LUMA - 1);

          // NOTE: corners don't need to be padded
          const Pel *refPel = ref + yOffset * refStride + xOffset;
          Pel *      dstPel = dst;

          for (ptrdiff_t x = 0; x < sbWidth; x++)
          {
            const ptrdiff_t refOffset = sbHeight * refStride;
            const ptrdiff_t dstOffset = sbHeight * dstStride;

            dstPel[x - dstStride] = (refPel[x - refStride] << shift) - IF_INTERNAL_OFFS;
            dstPel[x + dstOffset] = (refPel[x + refOffset] << shift) - IF_INTERNAL_OFFS;
          }

          for (int y = 0; y < sbHeight; y++, refPel += refStride, dstPel += dstStride)
          {
            dstPel[-1]      = (refPel[-1] << shift) - IF_INTERNAL_OFFS;
            dstPel[sbWidth] = (refPel[sbWidth] << shift) - IF_INTERNAL_OFFS;
          }

          const ptrdiff_t strideGrad = AFFINE_SUBBLOCK_WIDTH_EXT;

          g_pelBufOP.profGradFilter(dstExtBuf.buf, dstExtBuf.stride, AFFINE_SUBBLOCK_WIDTH_EXT,
                                    AFFINE_SUBBLOCK_HEIGHT_EXT, strideGrad, m_gradBuf[0], m_gradBuf[1], clpRng.bd);

          const Pel offset = (1 << shift >> 1) + IF_INTERNAL_OFFS;

          Pel *src  = dst;
          Pel *gX   = m_gradBuf[0] + PROF_BORDER_EXT_H * strideGrad + PROF_BORDER_EXT_W;
          Pel *gY   = m_gradBuf[1] + PROF_BORDER_EXT_H * strideGrad + PROF_BORDER_EXT_W;
          Pel *dstY = dstBuf.bufAt(w, h);

          g_pelBufOP.applyPROF(dstY, dstBuf.stride, src, dstExtBuf.stride, sbWidth, sbHeight, gX, gY, strideGrad,
                               dMvScaleHor, dMvScaleVer, sbWidth, bi, shift, offset, clpRng);
        }
      }
    }
  }
#if GDR_ENABLED
  return allOk;
#endif
}

void InterPrediction::applyBiOptFlow(const PredictionUnit &pu, const CPelUnitBuf &yuvSrc0, const CPelUnitBuf &yuvSrc1, const int &refIdx0, const int &refIdx1, PelUnitBuf &yuvDst, const BitDepths &clipBitDepths)
{
  const int     height = yuvDst.Y().height;
  const int     width = yuvDst.Y().width;
  int           heightG = height + 2 * BIO_EXTEND_SIZE;
  int           widthG = width + 2 * BIO_EXTEND_SIZE;
  int           offsetPos = widthG*BIO_EXTEND_SIZE + BIO_EXTEND_SIZE;

  Pel*          gradX0 = m_gradX0;
  Pel*          gradX1 = m_gradX1;
  Pel*          gradY0 = m_gradY0;
  Pel*          gradY1 = m_gradY1;

  const ptrdiff_t stridePredMC = widthG + 2;

  const Pel *srcY0 = m_filteredBlockTmp[2][COMPONENT_Y] + stridePredMC + 1;
  const Pel *srcY1 = m_filteredBlockTmp[3][COMPONENT_Y] + stridePredMC + 1;

  const ptrdiff_t src0Stride = stridePredMC;
  const ptrdiff_t src1Stride = stridePredMC;

  Pel*          dstY = yuvDst.Y().buf;
  const ptrdiff_t dstStride = yuvDst.Y().stride;
  const Pel*    srcY0Temp = srcY0;
  const Pel*    srcY1Temp = srcY1;

  for (int refList = 0; refList < NUM_REF_PIC_LIST_01; refList++)
  {
    Pel* dstTempPtr = m_filteredBlockTmp[2 + refList][COMPONENT_Y] + stridePredMC + 1;
    Pel* gradY = (refList == 0) ? m_gradY0 : m_gradY1;
    Pel* gradX = (refList == 0) ? m_gradX0 : m_gradX1;

    xBioGradFilter(dstTempPtr, stridePredMC, widthG, heightG, widthG, gradX, gradY,
                   clipBitDepths[toChannelType(COMPONENT_Y)]);
    Pel* padStr = m_filteredBlockTmp[2 + refList][COMPONENT_Y] + 2 * stridePredMC + 2;
    for (int y = 0; y< height; y++)
    {
      padStr[-1] = padStr[0];
      padStr[width] = padStr[width - 1];
      padStr += stridePredMC;
    }

    padStr = m_filteredBlockTmp[2 + refList][COMPONENT_Y] + 2 * stridePredMC + 1;
    ::memcpy(padStr - stridePredMC, padStr, sizeof(Pel)*(widthG));
    ::memcpy(padStr + height*stridePredMC, padStr + (height - 1)*stridePredMC, sizeof(Pel)*(widthG));
  }

  const ClpRng& clpRng = pu.cu->cs->slice->clpRng(COMPONENT_Y);
  const int     bitDepth = clipBitDepths[toChannelType(COMPONENT_Y)];
  const int   shiftNum = IF_INTERNAL_FRAC_BITS(bitDepth) + 1;
  const int   offset = (1 << (shiftNum - 1)) + 2 * IF_INTERNAL_OFFS;
  const int   limit = ( 1 << 4 ) - 1;

  int xUnit = (width >> 2);
  int yUnit = (height >> 2);

  Pel *dstY0 = dstY;
  gradX0 = m_gradX0; gradX1 = m_gradX1;
  gradY0 = m_gradY0; gradY1 = m_gradY1;

  for (int yu = 0; yu < yUnit; yu++)
  {
    for (int xu = 0; xu < xUnit; xu++)
    {
      int tmpx = 0, tmpy = 0;
      int sumAbsGX = 0, sumAbsGY = 0, sumDIX = 0, sumDIY = 0;
      int sumSignGY_GX = 0;

      Pel* pGradX0Tmp = m_gradX0 + (xu << 2) + (yu << 2) * widthG;
      Pel* pGradX1Tmp = m_gradX1 + (xu << 2) + (yu << 2) * widthG;
      Pel* pGradY0Tmp = m_gradY0 + (xu << 2) + (yu << 2) * widthG;
      Pel* pGradY1Tmp = m_gradY1 + (xu << 2) + (yu << 2) * widthG;
      const Pel* SrcY1Tmp = srcY1 + (xu << 2) + (yu << 2) * src1Stride;
      const Pel* SrcY0Tmp = srcY0 + (xu << 2) + (yu << 2) * src0Stride;

      g_pelBufOP.calcBIOSums(SrcY0Tmp, SrcY1Tmp, pGradX0Tmp, pGradX1Tmp, pGradY0Tmp, pGradY1Tmp, xu, yu, src0Stride, src1Stride, widthG, bitDepth, &sumAbsGX, &sumAbsGY, &sumDIX, &sumDIY, &sumSignGY_GX);
      tmpx = (sumAbsGX == 0 ? 0 : rightShiftMSB(4 * sumDIX, sumAbsGX));
      tmpx = Clip3(-limit, limit, tmpx);

      const int tmpData = sumSignGY_GX * tmpx >> 1;

      tmpy = (sumAbsGY == 0 ? 0 : rightShiftMSB((4 * sumDIY - tmpData), sumAbsGY));
      tmpy = Clip3(-limit, limit, tmpy);

      srcY0Temp = srcY0 + (stridePredMC + 1) + ((yu*src0Stride + xu) << 2);
      srcY1Temp = srcY1 + (stridePredMC + 1) + ((yu*src0Stride + xu) << 2);
      gradX0 = m_gradX0 + offsetPos + ((yu*widthG + xu) << 2);
      gradX1 = m_gradX1 + offsetPos + ((yu*widthG + xu) << 2);
      gradY0 = m_gradY0 + offsetPos + ((yu*widthG + xu) << 2);
      gradY1 = m_gradY1 + offsetPos + ((yu*widthG + xu) << 2);

      dstY0 = dstY + ((yu*dstStride + xu) << 2);
      xAddBIOAvg4(srcY0Temp, src0Stride, srcY1Temp, src1Stride, dstY0, dstStride, gradX0, gradX1, gradY0, gradY1, widthG, (1 << 2), (1 << 2), (int)tmpx, (int)tmpy, shiftNum, offset, clpRng);
    }  // xu
  }  // yu
}

void InterPrediction::xAddBIOAvg4(const Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride,
                                  Pel *dst, ptrdiff_t dstStride, const Pel *gradX0, const Pel *gradX1,
                                  const Pel *gradY0, const Pel *gradY1, ptrdiff_t gradStride, int width, int height,
                                  int tmpx, int tmpy, int shift, int offset, const ClpRng &clpRng)
{
  g_pelBufOP.addBIOAvg4(src0, src0Stride, src1, src1Stride, dst, dstStride, gradX0, gradX1, gradY0, gradY1, gradStride, width, height, tmpx, tmpy, shift, offset, clpRng);
}

void InterPrediction::xBioGradFilter(Pel *pSrc, ptrdiff_t srcStride, int width, int height, ptrdiff_t gradStride,
                                     Pel *gradX, Pel *gradY, int bitDepth)
{
  g_pelBufOP.bioGradFilter(pSrc, srcStride, width, height, gradStride, gradX, gradY, bitDepth);
}

void InterPrediction::xCalcBIOPar(const Pel* srcY0Temp, const Pel* srcY1Temp, const Pel* gradX0, const Pel* gradX1, const Pel* gradY0, const Pel* gradY1, int* dotProductTemp1, int* dotProductTemp2, int* dotProductTemp3, int* dotProductTemp5, int* dotProductTemp6, const int src0Stride, const int src1Stride, const int gradStride, const int widthG, const int heightG, int bitDepth)
{
  g_pelBufOP.calcBIOPar(srcY0Temp, srcY1Temp, gradX0, gradX1, gradY0, gradY1, dotProductTemp1, dotProductTemp2, dotProductTemp3, dotProductTemp5, dotProductTemp6, src0Stride, src1Stride, gradStride, widthG, heightG, bitDepth);
}

void InterPrediction::xCalcBlkGradient(int sx, int sy, int    *arraysGx2, int     *arraysGxGy, int     *arraysGxdI, int     *arraysGy2, int     *arraysGydI, int     &sGx2, int     &sGy2, int     &sGxGy, int     &sGxdI, int     &sGydI, int width, int height, int unitSize)
{
  g_pelBufOP.calcBlkGradient(sx, sy, arraysGx2, arraysGxGy, arraysGxdI, arraysGy2, arraysGydI, sGx2, sGy2, sGxGy, sGxdI, sGydI, width, height, unitSize);
}

void InterPrediction::xWeightedAverage(const PredictionUnit &pu, const CPelUnitBuf &pcYuvSrc0,
                                       const CPelUnitBuf &pcYuvSrc1, PelUnitBuf &pcYuvDst,
                                       const BitDepths &clipBitDepths, const ClpRngs &clpRngs, const bool bioApplied,
                                       bool lumaOnly, bool chromaOnly, PelUnitBuf *yuvDstTmp)
{
  CHECK( (chromaOnly && lumaOnly), "should not happen" );

  const int refIdx0 = pu.refIdx[0];
  const int refIdx1 = pu.refIdx[1];

  if (refIdx0 >= 0 && refIdx1 >= 0)
  {
    if (pu.cu->bcwIdx != BCW_DEFAULT && (yuvDstTmp || !pu.ciipFlag))
    {
      CHECK(bioApplied, "Bcw is disallowed with BIO");
      pcYuvDst.addWeightedAvg(pcYuvSrc0, pcYuvSrc1, clpRngs, pu.cu->bcwIdx, chromaOnly, lumaOnly);
      if (yuvDstTmp)
        yuvDstTmp->addAvg(pcYuvSrc0, pcYuvSrc1, clpRngs, chromaOnly, lumaOnly);
      return;
    }
    if (bioApplied)
    {
      const int  src0Stride = pu.lwidth() + 2 * BIO_EXTEND_SIZE + 2;
      const int  src1Stride = pu.lwidth() + 2 * BIO_EXTEND_SIZE + 2;
      const Pel* pSrcY0 = m_filteredBlockTmp[2][COMPONENT_Y] + 2 * src0Stride + 2;
      const Pel* pSrcY1 = m_filteredBlockTmp[3][COMPONENT_Y] + 2 * src1Stride + 2;

      bool bioEnabled = true;
      if (bioEnabled)
      {
        applyBiOptFlow(pu, pcYuvSrc0, pcYuvSrc1, refIdx0, refIdx1, pcYuvDst, clipBitDepths);
        if (yuvDstTmp)
          yuvDstTmp->bufs[0].addAvg(CPelBuf(pSrcY0, src0Stride, pu.lumaSize()), CPelBuf(pSrcY1, src1Stride, pu.lumaSize()), clpRngs.comp[0]);
      }
      else
      {
        pcYuvDst.bufs[0].addAvg(CPelBuf(pSrcY0, src0Stride, pu.lumaSize()), CPelBuf(pSrcY1, src1Stride, pu.lumaSize()), clpRngs.comp[0]);
        if (yuvDstTmp)
          yuvDstTmp->bufs[0].copyFrom(pcYuvDst.bufs[0]);
      }
    }
    if (!bioApplied && (lumaOnly || chromaOnly))
    {
      pcYuvDst.addAvg(pcYuvSrc0, pcYuvSrc1, clpRngs, chromaOnly, lumaOnly);
    }
    else
    {
      pcYuvDst.addAvg(pcYuvSrc0, pcYuvSrc1, clpRngs, bioApplied);
    }
    if (yuvDstTmp)
    {
      if (bioApplied)
      {
        if (isChromaEnabled(yuvDstTmp->chromaFormat))
        {
          yuvDstTmp->bufs[1].copyFrom(pcYuvDst.bufs[1]);
          yuvDstTmp->bufs[2].copyFrom(pcYuvDst.bufs[2]);
        }
      }
      else
      {
        yuvDstTmp->copyFrom(pcYuvDst, lumaOnly, chromaOnly);
      }
    }
  }
  else if (refIdx0 >= 0 && refIdx1 < 0)
  {
    if( pu.cu->geoFlag )
    {
      pcYuvDst.copyFrom( pcYuvSrc0 );
    }
    else
    {
      pcYuvDst.copyClip( pcYuvSrc0, clpRngs, lumaOnly, chromaOnly );
    }
    if (yuvDstTmp)
    {
      yuvDstTmp->copyFrom( pcYuvDst, lumaOnly, chromaOnly );
    }
  }
  else if (refIdx0 < 0 && refIdx1 >= 0)
  {
    if( pu.cu->geoFlag )
    {
      pcYuvDst.copyFrom( pcYuvSrc1 );
    }
    else
    {
      pcYuvDst.copyClip( pcYuvSrc1, clpRngs, lumaOnly, chromaOnly );
    }
    if (yuvDstTmp)
    {
      yuvDstTmp->copyFrom(pcYuvDst, lumaOnly, chromaOnly);
    }
  }
}

void InterPrediction::motionCompensation(PredictionUnit &pu, PelUnitBuf &predBuf, const RefPicList eRefPicList,
                                         const bool luma, const bool chroma, PelUnitBuf *predBufWOBIO,
                                         const bool isSubPu)
{
  // Note: there appears to be an interaction with weighted prediction that
  // makes the code follow different paths if chroma is on or off (in the encoder).
  // Therefore for 4:0:0, "chroma" is not changed to false.
  CHECK(predBufWOBIO && pu.ciipFlag, "the case should not happen!");

  if (!pu.cs->pcv->isEncoder)
  {
    if (CU::isIBC(*pu.cu))
    {
      CHECK(!luma, "IBC only for Chroma is not allowed.");
      xIntraBlockCopy(pu, predBuf, COMPONENT_Y);
      if (chroma && isChromaEnabled(pu.chromaFormat))
      {
        xIntraBlockCopy(pu, predBuf, COMPONENT_Cb);
        xIntraBlockCopy(pu, predBuf, COMPONENT_Cr);
      }
      return;
    }
  }
  // dual tree handling for IBC as the only ref
  if ((!luma || !chroma) && eRefPicList == REF_PIC_LIST_0)
  {
    xPredInterUni(pu, eRefPicList, predBuf, false, false, luma, chroma);
    return;
  }
  // else, go with regular MC below
        CodingStructure &cs = *pu.cs;
  const PPS &pps            = *cs.pps;
  const SliceType sliceType =  cs.slice->getSliceType();

  if( eRefPicList != REF_PIC_LIST_X )
  {
    CHECK(predBufWOBIO != nullptr, "the case should not happen!");
    if (!CU::isIBC(*pu.cu) && ((sliceType == P_SLICE && pps.getUseWP()) || (sliceType == B_SLICE && pps.getWPBiPred())))
    {
      xPredInterUni(pu, eRefPicList, predBuf, true, false, luma, chroma);
      xWeightedPredictionUni(pu, predBuf, eRefPicList, predBuf, -1, m_maxCompIDToPred, (luma && !chroma),
                             (!luma && chroma));
    }
    else
    {
      xPredInterUni(pu, eRefPicList, predBuf, false, false, luma, chroma);
    }
  }
  else
  {
    const int refIdx0 = pu.refIdx[REF_PIC_LIST_0];
    const int refIdx1 = pu.refIdx[REF_PIC_LIST_1];
    CHECK(!pu.cu->affine && refIdx0 >= 0 && refIdx1 >= 0 && pu.lwidth() + pu.lheight() == 12,
          "invalid 4x8/8x4 bi-predicted blocks");

    bool bioApplied = false;
    if (pu.cs->sps->getBDOFEnabledFlag() && !pu.cs->picHeader->getBdofDisabledFlag())
    {
      if (pu.cu->affine || isSubPu)
      {
        bioApplied = false;
      }
      else
      {
        bioApplied = PU::isSimpleSymmetricBiPred(pu) && PU::dmvrBdofSizeCheck(pu) && !pu.ciipFlag && !pu.cu->smvdMode;
      }

      if (pu.mmvdEncOptMode == 2 && pu.mmvdMergeFlag)
      {
        bioApplied = false;
      }
    }

    bool refIsScaled = ( refIdx0 < 0 ? false : pu.cu->slice->getRefPic( REF_PIC_LIST_0, refIdx0 )->isRefScaled( pu.cs->sps, pu.cs->pps ) ) ||
                       ( refIdx1 < 0 ? false : pu.cu->slice->getRefPic( REF_PIC_LIST_1, refIdx1 )->isRefScaled( pu.cs->sps, pu.cs->pps ) );
    bioApplied = refIsScaled ? false : bioApplied;
    bool dmvrApplied = !isSubPu && PU::checkDMVRCondition(pu);
    if ((pu.lumaSize().width > MAX_BDOF_APPLICATION_REGION || pu.lumaSize().height > MAX_BDOF_APPLICATION_REGION)
        && pu.mergeType != MergeType::SUBPU_ATMVP && (bioApplied && !dmvrApplied))
    {
      xSubPuBio(pu, predBuf, eRefPicList, predBufWOBIO);
    }
    else
    {
      if (pu.mergeType != MergeType::DEFAULT_N && pu.mergeType != MergeType::IBC)
      {
        CHECK(predBufWOBIO != nullptr, "the case should not happen!");
        xSubPuMC(pu, predBuf, eRefPicList, luma, chroma);
      }
      else if (xCheckIdenticalMotion(pu))
      {
        xPredInterUni(pu, REF_PIC_LIST_0, predBuf, false, false, luma, chroma);
        if (predBufWOBIO)
        {
          predBufWOBIO->copyFrom(predBuf, (luma && !chroma), (chroma && !luma));
        }
      }
      else
      {
        xPredInterBi(pu, predBuf, luma, chroma, predBufWOBIO, isSubPu);
      }
    }
  }
  return;
}

void InterPrediction::motionCompensateCu(CodingUnit &cu, const RefPicList eRefPicList, const bool luma,
                                         const bool chroma)
{
  for( auto &pu : CU::traversePUs( cu ) )
  {
    PelUnitBuf predBuf = cu.cs->getPredBuf(pu);
    motionCompensation(pu, predBuf, eRefPicList, luma, chroma, nullptr, false);
  }
}

void InterPrediction::motionCompensatePu(PredictionUnit &pu, const RefPicList eRefPicList, const bool luma,
                                         const bool chroma)
{
  PelUnitBuf predBuf = pu.cs->getPredBuf( pu );
  motionCompensation(pu, predBuf, eRefPicList, luma, chroma, nullptr, false);
}

int InterPrediction::rightShiftMSB(int numer, int denom)
{
  return numer >> floorLog2(denom);
}

void InterPrediction::motionCompensationGeo( CodingUnit &cu, MergeCtx &geoMrgCtx )
{
  const uint8_t splitDir = cu.firstPU->geoSplitDir;

  for( auto &pu : CU::traversePUs( cu ) )
  {
    const UnitArea localUnitArea(cu.cs->area.chromaFormat, Area(0, 0, pu.lwidth(), pu.lheight()));

    PelUnitBuf predBuf = cu.cs->getPredBuf(pu);
    PelUnitBuf tmpGeoBuf[2];

    for (int i = 0; i < 2; i++)
    {
      tmpGeoBuf[i] = m_geoPartBuf[i].getBuf(localUnitArea);

      geoMrgCtx.setMergeInfo(pu, cu.firstPU->geoMergeIdx[i]);
      PU::spanMotionInfo(pu);
      // TODO: check 4:0:0 interaction with weighted prediction.
      motionCompensation(pu, tmpGeoBuf[i], REF_PIC_LIST_X, true, isChromaEnabled(pu.chromaFormat), nullptr, false);
      if (g_mctsDecCheckEnabled && !MCTSHelper::checkMvBufferForMCTSConstraint(pu, true))
      {
        printf("DECODER_GEO_PU: pu motion vector across tile boundaries (%d,%d,%d,%d)\n", pu.lx(), pu.ly(), pu.lwidth(),
               pu.lheight());
      }
    }

    weightedGeoBlk(pu, splitDir, ChannelType::LUMA, predBuf, tmpGeoBuf[0], tmpGeoBuf[1]);
    if (isChromaEnabled(pu.chromaFormat))
    {
      weightedGeoBlk(pu, splitDir, ChannelType::CHROMA, predBuf, tmpGeoBuf[0], tmpGeoBuf[1]);
    }
  }
}

void InterPrediction::weightedGeoBlk(PredictionUnit &pu, const uint8_t splitDir, const ChannelType channel,
                                     PelUnitBuf &predDst, PelUnitBuf &predSrc0, PelUnitBuf &predSrc1)
{
  if (isLuma(channel))
  {
    m_if.weightedGeoBlk( pu, pu.lumaSize().width, pu.lumaSize().height, COMPONENT_Y, splitDir, predDst, predSrc0, predSrc1 );
  }
  else
  {
    m_if.weightedGeoBlk( pu, pu.chromaSize().width, pu.chromaSize().height, COMPONENT_Cb, splitDir, predDst, predSrc0, predSrc1 );
    m_if.weightedGeoBlk( pu, pu.chromaSize().width, pu.chromaSize().height, COMPONENT_Cr, splitDir, predDst, predSrc0, predSrc1 );
  }
}

void InterPrediction::xDmvrPrefetch(const PredictionUnit &pu, const bool forLuma)
{
  const int mvShift = MV_FRACTIONAL_BITS_INTERNAL;
  const int start   = forLuma ? 0 : 1;
  const int end     = forLuma ? 1 : MAX_NUM_COMPONENT;

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const Picture *refPic = pu.cu->slice->getRefPic(l, pu.refIdx[l])->unscaledPic;

    for (int compID = start; compID < end; compID++)
    {
      PelBuf &dstBuf = m_yuvRefBufDmvr[l].bufs[compID];

      dstBuf.stride = dstBuf.width + DMVR_SPAN - 1 + NTAPS_LUMA;

      const int filtersize     = isLuma((ComponentID) compID) ? NTAPS_LUMA : NTAPS_CHROMA;
      const int width          = dstBuf.width + filtersize - 1;
      const int height         = dstBuf.height + filtersize - 1;
      const int mvshiftTempHor = mvShift + getComponentScaleX((ComponentID) compID, pu.chromaFormat);
      const int mvshiftTempVer = mvShift + getComponentScaleY((ComponentID) compID, pu.chromaFormat);
      const int halfFilterSize = (filtersize >> 1) - 1;

      Mv cMv = pu.mv[l] - Mv(halfFilterSize << mvshiftTempHor, halfFilterSize << mvshiftTempVer);

      bool wrapRef = false;
      if (refPic->isWrapAroundEnabled(pu.cs->sps, pu.cs->pps))
      {
        wrapRef = wrapClipMv(cMv, pu.blocks[0].pos(), pu.blocks[0].size(), pu.cs->sps, pu.cs->pps);
      }
      else
      {
        clipMv(cMv, pu.lumaPos(), pu.lumaSize(), *pu.cs->sps, *pu.cs->pps);
      }
      /* Pre-fetch similar to HEVC*/
      {
        Position recOffset =
          pu.blocks[compID].pos().offset(cMv.getHor() >> mvshiftTempHor, cMv.getVer() >> mvshiftTempVer);
        CPelBuf refBuf = refPic->getRecoBuf(
          CompArea((ComponentID) compID, pu.chromaFormat, recOffset, pu.blocks[compID].size()), wrapRef);
        g_pelBufOP.copyBuffer(refBuf.buf, refBuf.stride, dstBuf.bufAt(DMVR_RANGE, DMVR_RANGE), dstBuf.stride, width,
                              height);
      }
    }
  }
}

void InterPrediction::xDmvrPad(const PredictionUnit &pu)
{
  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    for (int compID = 0; compID < getNumberValidComponents(pu.chromaFormat); compID++)
    {
      PelBuf &buf = m_yuvRefBufDmvr[l].bufs[compID];

      const int filtersize = isLuma(ComponentID(compID)) ? NTAPS_LUMA : NTAPS_CHROMA;
      const int width      = buf.width + filtersize - 1;
      const int height     = buf.height + filtersize - 1;
      // using larger padsize for 4:2:2
      const int padsize = DMVR_RANGE >> getComponentScaleY((ComponentID) compID, pu.chromaFormat);
      g_pelBufOP.padding(buf.bufAt(DMVR_RANGE, DMVR_RANGE), buf.stride, width, height, padsize);
    }
  }
}

constexpr InterPrediction::DmvrDist InterPrediction::UNDEFINED_DMVR_DIST;

void InterPrediction::xDmvrIntegerRefine(int bd, DmvrDist &minCost, Mv &deltaMv, DmvrDist *sadPtr, int width,
                                         int height)
{
  for (const auto &mvd: m_dmvrSearchOffsets)
  {
    const int32_t sadOffset = mvd.ver * DMVR_SPAN + mvd.hor;

    if (sadPtr[sadOffset] == UNDEFINED_DMVR_DIST)
    {
      sadPtr[sadOffset] = xDmvrCost(bd, mvd, width, height);
    }
    if (sadPtr[sadOffset] < minCost)
    {
      minCost = sadPtr[sadOffset];
      deltaMv = mvd;
    }
  }
}

void InterPrediction::xDmvrFinalMc(const PredictionUnit &pu, PelUnitBuf yuvSrc[NUM_REF_PIC_LIST_01],
                                   const bool applyBdof, const Mv mergeMV[NUM_REF_PIC_LIST_01], const bool blockMoved)
{
  /*always high precision MVs are used*/
  int mvShift = MV_FRACTIONAL_BITS_INTERNAL;

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    Mv cMv = pu.mv[l];

    const Picture *refPic = pu.cu->slice->getRefPic(l, pu.refIdx[l])->unscaledPic;

    Mv cMvClipped = cMv;
    if( !pu.cs->pps->getWrapAroundEnabledFlag() )
    {
      clipMv( cMvClipped, pu.lumaPos(), pu.lumaSize(), *pu.cs->sps, *pu.cs->pps );
    }

    Mv startMv = mergeMV[l];

    if (g_mctsDecCheckEnabled && !MCTSHelper::checkMvForMCTSConstraint(pu, startMv, MvPrecision::INTERNAL))
    {
      const Area& tileArea = pu.cs->picture->mctsInfo.getTileArea();
      printf( "Attempt an access over tile boundary at block %d,%d %d,%d with MV %d,%d (in Tile TL: %d,%d BR: %d,%d)\n",
        pu.lx(), pu.ly(), pu.lwidth(), pu.lheight(), startMv.getHor(), startMv.getVer(), tileArea.topLeft().x, tileArea.topLeft().y, tileArea.bottomRight().x, tileArea.bottomRight().y );
      THROW( "MCTS constraint failed!" );
    }
    for (int co = 0; co < getNumberValidComponents(pu.chromaFormat); co++)
    {
      const auto compId = ComponentID(co);

      Pel *srcBufPtr    = nullptr;
      ptrdiff_t srcBufStride = 0;

      // when blockMoved is false, only luma data has been prefetched
      if (blockMoved || isLuma(compId))
      {
        const int mvshiftTempHor = mvShift + getComponentScaleX(compId, pu.chromaFormat);
        const int mvshiftTempVer = mvShift + getComponentScaleY(compId, pu.chromaFormat);
        const int leftPixelExtra = ((isLuma(compId) ? NTAPS_LUMA : NTAPS_CHROMA) >> 1) - 1;
        const int deltaIntMvX    = (cMv.getHor() >> mvshiftTempHor) - (startMv.getHor() >> mvshiftTempHor);
        const int deltaIntMvY    = (cMv.getVer() >> mvshiftTempVer) - (startMv.getVer() >> mvshiftTempVer);

        CHECK(abs(deltaIntMvX) > DMVR_RANGE || abs(deltaIntMvY) > DMVR_RANGE, "not expected DMVR movement");

        PelBuf &srcBuf = m_yuvRefBufDmvr[l].bufs[compId];

        srcBufPtr = srcBuf.bufAt(DMVR_RANGE + leftPixelExtra + deltaIntMvX, DMVR_RANGE + leftPixelExtra + deltaIntMvY);
        srcBufStride = srcBuf.stride;
      }

      JVET_J0090_SET_CACHE_ENABLE(false);
      xPredInterBlk(compId, pu, refPic, cMvClipped, yuvSrc[l], true, pu.cs->slice->getClpRngs().comp[compId], applyBdof,
                    false, pu.cu->slice->getScalingRatio(l, pu.refIdx[l]), false, srcBufPtr, srcBufStride,
                    isLuma(compId) && applyBdof ? m_filteredBlockTmp[2 + l][compId] : nullptr);
      JVET_J0090_SET_CACHE_ENABLE(false);
    }
  }
}

auto InterPrediction::xDmvrCost(int bitDepth, const Mv &mvd, int width, int height) -> DmvrDist
{
  const Pel *p0 = m_dmvrInitialPred[REF_PIC_LIST_0].bufAt(DMVR_RANGE + mvd.hor, DMVR_RANGE + mvd.ver);
  const Pel *p1 = m_dmvrInitialPred[REF_PIC_LIST_1].bufAt(DMVR_RANGE - mvd.hor, DMVR_RANGE - mvd.ver);

  const ptrdiff_t s0 = m_dmvrInitialPred[REF_PIC_LIST_0].stride;
  const ptrdiff_t s1 = m_dmvrInitialPred[REF_PIC_LIST_1].stride;

  DistParam cDistParam;
  cDistParam.applyWeight = false;
  cDistParam.useMR = false;
  m_pcRdCost->setDistParam(cDistParam, p0, p1, s0, s1, bitDepth, COMPONENT_Y, width, height, 1);
  return DmvrDist(cDistParam.distFunc(cDistParam) >> 1);
}

Mv InterPrediction::xDmvrSubpelRefine(const DmvrDist *sadPtr)
{
  auto getRefinement = [](DmvrDist s0, DmvrDist s1, DmvrDist s2) -> int
  {
    CHECK(s0 > s1 || s0 > s2, "s0 should be smallest value");

    const DmvrDist den = s1 + s2 - 2 * s0;

    if (0 != den)
    {
      if (s1 != s0 && s2 != s0)
      {
        const DmvrDist num  = ((s1 - s2) << MV_FRACTIONAL_BITS_INTERNAL) >> 1;
        DmvrDist       rem  = abs(num);
        const bool     sign = num < 0;

        int q = 0;

        for (int i = 2; i >= 0; i--)
        {
          const DmvrDist div = den << i;

          if (rem >= div)
          {
            rem -= div;
            q += 1 << i;
          }
        }

        return sign ? -q : q;
      }
      else
      {
        constexpr int HALF_PEL = 1 << MV_FRACTIONAL_BITS_INTERNAL >> 1;
        return s1 == s0 ? -HALF_PEL : HALF_PEL;
      }
    }

    return 0;
  };

  return Mv(getRefinement(sadPtr[0], sadPtr[-1], sadPtr[1]),
            getRefinement(sadPtr[0], sadPtr[-DMVR_SPAN], sadPtr[DMVR_SPAN]));
}

void InterPrediction::xDmvrInitialMc(const PredictionUnit &pu, const ClpRngs &clpRngs)
{
  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    Mv mergeMv = pu.mv[l];
    if (!pu.cs->pps->getWrapAroundEnabledFlag())
    {
      clipMv(mergeMv, pu.lumaPos(), pu.lumaSize(), *pu.cs->sps, *pu.cs->pps);
    }

    const int leftPixelExtra = (NTAPS_LUMA >> 1) - 1;
    Pel      *srcBufPtr      = m_yuvRefBufDmvr[l].bufs[COMPONENT_Y].bufAt(leftPixelExtra, leftPixelExtra);
    const ptrdiff_t srcBufStride   = m_yuvRefBufDmvr[l].bufs[COMPONENT_Y].stride;

    PelUnitBuf dstBuf(ChromaFormat::_400, m_dmvrInitialPred[l]);

    xPredInterBlk(COMPONENT_Y, pu, pu.cu->slice->getRefPic(l, pu.refIdx[l])->unscaledPic, mergeMv, dstBuf, true,
                  clpRngs.comp[COMPONENT_Y], false, false, pu.cu->slice->getScalingRatio(l, pu.refIdx[l]), true,
                  srcBufPtr, srcBufStride, nullptr);
  }
}

static constexpr int ACTIVITY_TH[MAX_QP + 1] = {
  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  5,  5,  6,  6,
  7,  7,  8,  9,  9,  10, 10, 11, 12, 13, 13, 14, 15, 16, 17, 18, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 29, 30, 31, 32, 33, 34, 36, 36, 36, 36, 37, 37, 37, 37, 37, 37,
};

void InterPrediction::xProcessDMVR(PredictionUnit &pu, PelUnitBuf &pcYuvDst, const ClpRngs &clpRngs,
                                   const bool applyBdof)
{
  /*use merge MV as starting MV*/
  const Mv mergeMv[] = { pu.mv[REF_PIC_LIST_0], pu.mv[REF_PIC_LIST_1] };

  const int dy = std::min<int>(pu.lumaSize().height, DMVR_SUBCU_HEIGHT);
  const int dx = std::min<int>(pu.lumaSize().width, DMVR_SUBCU_WIDTH);

  const Position puPos = pu.lumaPos();

  int bd = pu.cs->slice->getClpRngs().comp[COMPONENT_Y].bd;

#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  JVET_J0090_SET_CACHE_ENABLE(true);
  /*Always High Precision*/
  int mvShift = MV_FRACTIONAL_BITS_INTERNAL;

  for (int k = 0; k < NUM_REF_PIC_LIST_01; k++)
  {
    RefPicList refId = (RefPicList)k;
    const Picture* refPic = pu.cu->slice->getRefPic(refId, pu.refIdx[refId]);
    for (int compID = 0; compID < MAX_NUM_COMPONENT; compID++)
    {
      Mv cMv = pu.mv[refId];
      int mvshiftTemp = mvShift + getComponentScaleX((ComponentID)compID, pu.chromaFormat);
      int filtersize = (compID == (COMPONENT_Y)) ? NTAPS_LUMA : NTAPS_CHROMA;
      cMv += Mv(-(((filtersize >> 1) - 1) << mvshiftTemp), -(((filtersize >> 1) - 1) << mvshiftTemp));
      bool wrapRef = false;
      if ( pu.cs->pps->getWrapAroundEnabledFlag() )
      {
        wrapRef = wrapClipMv(cMv, pu.blocks[0].pos(), pu.blocks[0].size(), pu.cs->sps, pu.cs->pps);
      }
      else
      {
        clipMv(cMv, pu.lumaPos(), pu.lumaSize(), *pu.cs->sps, *pu.cs->pps);
      }

      int width = pcYuvDst.bufs[compID].width + (filtersize - 1);
      int height = pcYuvDst.bufs[compID].height + (filtersize - 1);

      CPelBuf refBuf;
      Position recOffset = pu.blocks[compID].pos().offset(cMv.getHor() >> mvshiftTemp, cMv.getVer() >> mvshiftTemp);
      refBuf = refPic->getRecoBuf(CompArea((ComponentID)compID, pu.chromaFormat, recOffset, pu.blocks[compID].size()), wrapRef);

      JVET_J0090_SET_REF_PICTURE(refPic, (ComponentID)compID);
      for (int row = 0; row < height; row++)
      {
        for (int col = 0; col < width; col++)
        {
          JVET_J0090_CACHE_ACCESS(((Pel *)refBuf.buf) + row * refBuf.stride + col, __FILE__, __LINE__);
        }
      }
    }
  }
  JVET_J0090_SET_CACHE_ENABLE(false);
#endif

  PredictionUnit  subPu = pu;
  subPu.UnitArea::operator=(UnitArea(pu.chromaFormat, Area(puPos.x, puPos.y, dx, dy)));

  PelUnitBuf srcPred[NUM_REF_PIC_LIST_01];

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    // point mc buffer to centre point to avoid multiplication to reach each iteration to the begining
    m_dmvrInitialPred[l] = PelBuf(m_yuvPredTempDmvr[l], dx + DMVR_SPAN - 1, dy + DMVR_SPAN - 1);

    m_yuvRefBufDmvr[l] = !isChromaEnabled(pu.chromaFormat)
                           ? PelUnitBuf(pu.chromaFormat, PelBuf(m_refSamplesDmvr[l][COMPONENT_Y], pcYuvDst.Y()))
                           : PelUnitBuf(pu.chromaFormat, PelBuf(m_refSamplesDmvr[l][COMPONENT_Y], pcYuvDst.Y()),
                                        PelBuf(m_refSamplesDmvr[l][COMPONENT_Cb], pcYuvDst.Cb()),
                                        PelBuf(m_refSamplesDmvr[l][COMPONENT_Cr], pcYuvDst.Cr()));
    m_yuvRefBufDmvr[l] = m_yuvRefBufDmvr[l].subBuf(UnitAreaRelative(pu, subPu));
    // stride is set in xDmvrPrefetch()

    srcPred[l] = !isChromaEnabled(pu.chromaFormat)
                   ? PelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[l][COMPONENT_Y], pcYuvDst.Y()))
                   : PelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[l][COMPONENT_Y], pcYuvDst.Y()),
                                PelBuf(m_acYuvPred[l][COMPONENT_Cb], pcYuvDst.Cb()),
                                PelBuf(m_acYuvPred[l][COMPONENT_Cr], pcYuvDst.Cr()));
    srcPred[l] = srcPred[l].subBuf(UnitAreaRelative(pu, subPu));
  }

  bool      riskArtifact          = false;
  const int widthInSubPu          = pu.lumaSize().width / DMVR_SUBCU_WIDTH;
  const int spatActivityThreshold = ACTIVITY_TH[std::max<int>(0, pu.cu->qp)];

  // subblock boundary activity threshold
  // corresponds to encoding in 1/2 reduced resolution when GOP based RPR is used
  const int boundaryDiffThreshold =
    pu.cu->slice->getPPS()->getPPSId() == (ENC_PPS_ID_RPR + pu.cu->slice->getNalUnitLayerId()) && pu.cu->slice->getSPS()->getGOPBasedRPREnabledFlag() ? 5 : 10;

  int subPuIdx = 0;

  for (int yStart = 0; yStart < pu.lumaSize().height; yStart += dy)
  {
    for (int xStart = 0; xStart < pu.lumaSize().width; xStart += dx)
    {
      const int x = puPos.x + xStart;
      const int y = puPos.y + yStart;

      subPu.UnitArea::operator=(UnitArea(pu.chromaFormat, Area(x, y, dx, dy)));
      subPu.mv[REF_PIC_LIST_0] = mergeMv[REF_PIC_LIST_0];
      subPu.mv[REF_PIC_LIST_1] = mergeMv[REF_PIC_LIST_1];

      xDmvrPrefetch(subPu, true);

      xDmvrInitialMc(subPu, clpRngs);

      bool checkDmvr = xDmvrGetEncoderCheckFlag() && xStart != 0 && yStart != 0;
      if (checkDmvr)
      {
        checkDmvr = false;

        CHECK(dx != DMVR_SUBCU_WIDTH, "bad subblock width");
        CHECK(dy != DMVR_SUBCU_HEIGHT, "bad subblock height");

        for (int listIdx = 0; listIdx < NUM_REF_PIC_LIST_01; listIdx++)
        {
          int blkSumAct = 0;

          const ptrdiff_t blkStride = m_dmvrInitialPred[listIdx].stride;
          const Pel*      org       = m_dmvrInitialPred[listIdx].buf;

          for (int row = 1; row < DMVR_SUBCU_HEIGHT; row++)
          {
            for (int col = 1; col < DMVR_SUBCU_WIDTH; col++)
            {
              blkSumAct += std::abs(org[row * blkStride + col] - org[row * blkStride + col - 1]);
              blkSumAct += std::abs(org[row * blkStride + col] - org[row * blkStride + col - blkStride]);
            }
          }

          if (blkSumAct / ((DMVR_SUBCU_WIDTH - 1) * (DMVR_SUBCU_HEIGHT - 1)) < spatActivityThreshold)
          {
            checkDmvr = true;
            break;
          }
        }
      }

      std::array<DmvrDist, DMVR_AREA> sads;
      sads.fill(UNDEFINED_DMVR_DIST);
      DmvrDist *sadPtr = &sads[sads.size() / 2];

      DmvrDist minCost = xDmvrCost(clpRngs.comp[COMPONENT_Y].bd, Mv(), dx, dy);
      minCost -= minCost >> 2;
      sadPtr[0] = minCost;

      const bool significantCost = minCost >= dx * dy;

      Mv deltaMv;
      if (significantCost)
      {
        xDmvrIntegerRefine(bd, minCost, deltaMv, sadPtr, dx, dy);
        sadPtr += deltaMv.ver * DMVR_SPAN + deltaMv.hor;
      }

      const bool applyBdofSubPu = applyBdof && minCost >= 2 * dx * dy;

      const bool doSubpelRefine =
        significantCost && deltaMv.getAbsHor() != DMVR_RANGE && deltaMv.getAbsVer() != DMVR_RANGE;

      deltaMv.changePrecision(MvPrecision::ONE, MvPrecision::INTERNAL);

      if (doSubpelRefine)
      {
        deltaMv += xDmvrSubpelRefine(sadPtr);
      }

      const bool blockMoved = deltaMv != Mv();

      if (blockMoved)
      {
        if (isChromaEnabled(pu.chromaFormat))
        {
          xDmvrPrefetch(subPu, false);
        }
        xDmvrPad(subPu);
      }

      pu.mvdL0SubPu[subPuIdx] = deltaMv;
      subPu.mv[REF_PIC_LIST_0] += deltaMv;
      subPu.mv[REF_PIC_LIST_1] -= deltaMv;

      subPu.mv[REF_PIC_LIST_0].clipToStorageBitDepth();
      subPu.mv[REF_PIC_LIST_1].clipToStorageBitDepth();

      xDmvrFinalMc(subPu, srcPred, applyBdofSubPu, mergeMv, blockMoved);

      PelUnitBuf subPredBuf = pcYuvDst.subBuf(UnitAreaRelative(pu, subPu));

      xWeightedAverage(subPu, srcPred[REF_PIC_LIST_0], srcPred[REF_PIC_LIST_1], subPredBuf,
                       subPu.cu->slice->getSPS()->getBitDepths(), subPu.cu->slice->clpRngs(), applyBdofSubPu, false,
                       false, nullptr);
      if (checkDmvr)
      {
        checkDmvr = false;

        const ptrdiff_t blkStride = pcYuvDst.bufs[COMPONENT_Y].stride;
        const Pel*      org       = subPredBuf.bufs[COMPONENT_Y].buf;

        // check boundary diff above
        int blkSumAct = 0;

        for (int col = 0; col < DMVR_SUBCU_WIDTH; col++)
        {
          blkSumAct += std::abs(org[col] - org[col - blkStride]);
        }
        checkDmvr = blkSumAct / DMVR_SUBCU_WIDTH > boundaryDiffThreshold;

        if (!checkDmvr)
        {
          // check boundary diff left
          blkSumAct = 0;

          for (int row = 0; row < DMVR_SUBCU_HEIGHT; row++)
          {
            blkSumAct += std::abs(org[row * blkStride] - org[row * blkStride - 1]);
          }
          checkDmvr = blkSumAct / DMVR_SUBCU_HEIGHT > boundaryDiffThreshold;
        }

        if (checkDmvr)
        {
          constexpr int MV_THRESHOLD = 28;   // subblock motion difference threshold

          if (abs(pu.mvdL0SubPu[subPuIdx].hor - pu.mvdL0SubPu[subPuIdx - 1].hor) >= MV_THRESHOLD
              || abs(pu.mvdL0SubPu[subPuIdx].ver - pu.mvdL0SubPu[subPuIdx - 1].ver) >= MV_THRESHOLD
              || abs(pu.mvdL0SubPu[subPuIdx].hor - pu.mvdL0SubPu[subPuIdx - widthInSubPu].hor) >= MV_THRESHOLD
              || abs(pu.mvdL0SubPu[subPuIdx].ver - pu.mvdL0SubPu[subPuIdx - widthInSubPu].ver) >= MV_THRESHOLD)
          {
            riskArtifact = true;
          }
        }
      }
      subPuIdx++;
    }
  }
  if (xDmvrGetEncoderCheckFlag())
  {
    pu.dmvrImpreciseMv = riskArtifact;
  }
  JVET_J0090_SET_CACHE_ENABLE(true);
}

#if JVET_J0090_MEMORY_BANDWITH_MEASURE
void InterPrediction::cacheAssign( CacheModel *cache )
{
  m_cacheModel = cache;
  m_if.cacheAssign( cache );
  m_if.initInterpolationFilter( !cache->isCacheEnable() );
}
#endif

void InterPrediction::xFillIBCBuffer(CodingUnit &cu)
{
  for (auto &currPU : CU::traverseTUs(cu))
  {
    for (const CompArea &area : currPU.blocks)
    {
      if (!area.valid())
      {
        continue;
      }

      const unsigned int lcuWidth = cu.cs->slice->getSPS()->getMaxCUWidth();
      const int shiftSampleHor = ::getComponentScaleX(area.compID, cu.chromaFormat);
      const int shiftSampleVer = ::getComponentScaleY(area.compID, cu.chromaFormat);
      const int ctuSizeLog2Ver = floorLog2(lcuWidth) - shiftSampleVer;
      const int pux = area.x & ((m_IBCBufferWidth >> shiftSampleHor) - 1);
      const int puy = area.y & (( 1 << ctuSizeLog2Ver ) - 1);
      const CompArea dstArea = CompArea(area.compID, cu.chromaFormat, Position(pux, puy), Size(area.width, area.height));
      CPelBuf srcBuf = cu.cs->getRecoBuf(area);
      PelBuf dstBuf = m_IBCBuffer.getBuf(dstArea);

      dstBuf.copyFrom(srcBuf);
    }
  }
}

void InterPrediction::xIntraBlockCopy(PredictionUnit &pu, PelUnitBuf &predBuf, const ComponentID compID)
{
  const unsigned int lcuWidth = pu.cs->slice->getSPS()->getMaxCUWidth();
  const int shiftSampleHor = ::getComponentScaleX(compID, pu.chromaFormat);
  const int shiftSampleVer = ::getComponentScaleY(compID, pu.chromaFormat);
  const int ctuSizeLog2Ver = floorLog2(lcuWidth) - shiftSampleVer;
  pu.bv = pu.mv[REF_PIC_LIST_0];
  pu.bv.changePrecision(MvPrecision::INTERNAL, MvPrecision::ONE);
  int refx, refy;
  if (compID == COMPONENT_Y)
  {
    refx = pu.Y().x + pu.bv.hor;
    refy = pu.Y().y + pu.bv.ver;
  }
  else
  {//Cb or Cr
    refx = pu.Cb().x + (pu.bv.hor >> shiftSampleHor);
    refy = pu.Cb().y + (pu.bv.ver >> shiftSampleVer);
  }
  refx &= ((m_IBCBufferWidth >> shiftSampleHor) - 1);
  refy &= ((1 << ctuSizeLog2Ver) - 1);

  if (refx + predBuf.bufs[compID].width <= (m_IBCBufferWidth >> shiftSampleHor))
  {
    const CompArea srcArea = CompArea(compID, pu.chromaFormat, Position(refx, refy), Size(predBuf.bufs[compID].width, predBuf.bufs[compID].height));
    const CPelBuf refBuf = m_IBCBuffer.getBuf(srcArea);
    predBuf.bufs[compID].copyFrom(refBuf);
  }
  else
  {//wrap around
    int width = (m_IBCBufferWidth >> shiftSampleHor) - refx;
    CompArea srcArea = CompArea(compID, pu.chromaFormat, Position(refx, refy), Size(width, predBuf.bufs[compID].height));
    CPelBuf srcBuf = m_IBCBuffer.getBuf(srcArea);
    PelBuf dstBuf = PelBuf(predBuf.bufs[compID].bufAt(Position(0, 0)), predBuf.bufs[compID].stride, Size(width, predBuf.bufs[compID].height));
    dstBuf.copyFrom(srcBuf);

    width = refx + predBuf.bufs[compID].width - (m_IBCBufferWidth >> shiftSampleHor);
    srcArea = CompArea(compID, pu.chromaFormat, Position(0, refy), Size(width, predBuf.bufs[compID].height));
    srcBuf = m_IBCBuffer.getBuf(srcArea);
    dstBuf = PelBuf(predBuf.bufs[compID].bufAt(Position((m_IBCBufferWidth >> shiftSampleHor) - refx, 0)), predBuf.bufs[compID].stride, Size(width, predBuf.bufs[compID].height));
    dstBuf.copyFrom(srcBuf);
  }
}

void InterPrediction::resetIBCBuffer(const ChromaFormat chromaFormatIdc, const int ctuSize)
{
  const UnitArea area = UnitArea(chromaFormatIdc, Area(0, 0, m_IBCBufferWidth, ctuSize));
  m_IBCBuffer.getBuf(area).fill(-1);
}

void InterPrediction::resetVPDUforIBC(const ChromaFormat chromaFormatIdc, const int ctuSize, const int vSize,
                                      const int xPos, const int yPos)
{
  const UnitArea area =
    UnitArea(chromaFormatIdc, Area(xPos & (m_IBCBufferWidth - 1), yPos & (ctuSize - 1), vSize, vSize));
  m_IBCBuffer.getBuf(area).fill(-1);
}

bool InterPrediction::isLumaBvValid(const int ctuSize, const int xCb, const int yCb, const int width, const int height, const int xBv, const int yBv)
{
  if(((yCb + yBv) & (ctuSize - 1)) + height > ctuSize)
  {
    return false;
  }
  int refTLx = xCb + xBv;
  int refTLy = (yCb + yBv) & (ctuSize - 1);
  PelBuf buf = m_IBCBuffer.Y();
  for(int x = 0; x < width; x += 4)
  {
    for(int y = 0; y < height; y += 4)
    {
      if(buf.at((x + refTLx) & (m_IBCBufferWidth - 1), y + refTLy) == -1) return false;
      if(buf.at((x + 3 + refTLx) & (m_IBCBufferWidth - 1), y + refTLy) == -1) return false;
      if(buf.at((x + refTLx) & (m_IBCBufferWidth - 1), y + 3 + refTLy) == -1) return false;
      if(buf.at((x + 3 + refTLx) & (m_IBCBufferWidth - 1), y + 3 + refTLy) == -1) return false;
    }
  }
  return true;
}

bool InterPrediction::xPredInterBlkRPR(const ScalingRatio scalingRatio, const SPS &sps, const PPS &pps, const CompArea &blk,
                                       const Picture *refPic, const Mv &mv, Pel *dst, const ptrdiff_t dstStride,
                                       const bool bi, const bool wrapRef, const ClpRng &clpRng,
                                       const InterpolationFilter::Filter filterIndex, const bool useAltHpelIf)
{
  const ChromaFormat  chFmt = blk.chromaFormat;
  const ComponentID compID = blk.compID;
  const bool          rndRes = !bi;

  int shiftHor = MV_FRACTIONAL_BITS_INTERNAL + (isLuma(compID) ? 0 : 1);
  int shiftVer = MV_FRACTIONAL_BITS_INTERNAL + (isLuma(compID) ? 0 : 1);

  int width = blk.width;
  int height = blk.height;
  CPelBuf refBuf;

  const bool scaled = refPic->isRefScaled( &sps, &pps );

  if( scaled )
  {
    int row, col;
    int refPicWidth = refPic->getPicWidthInLumaSamples();
    int refPicHeight = refPic->getPicHeightInLumaSamples();

    InterpolationFilter::Filter xFilter       = filterIndex;
    InterpolationFilter::Filter yFilter       = filterIndex;

    const int rprThreshold1 = (1 << ScalingRatio::BITS) * 5 / 4;
    const int rprThreshold2 = (1 << ScalingRatio::BITS) * 7 / 4;

    if (filterIndex == InterpolationFilter::Filter::DEFAULT || !isLuma(compID))
    {
      if (scalingRatio.x > rprThreshold2)
      {
        xFilter = InterpolationFilter::Filter::RPR2;
      }
      else if (scalingRatio.x > rprThreshold1)
      {
        xFilter = InterpolationFilter::Filter::RPR1;
      }

      if (scalingRatio.y > rprThreshold2)
      {
        yFilter = InterpolationFilter::Filter::RPR2;
      }
      else if (scalingRatio.y > rprThreshold1)
      {
        yFilter = InterpolationFilter::Filter::RPR1;
      }
    }
    else if (filterIndex == InterpolationFilter::Filter::AFFINE)
    {
      if (scalingRatio.x > rprThreshold2)
      {
        xFilter = InterpolationFilter::Filter::AFFINE_RPR2;
      }
      else if (scalingRatio.x > rprThreshold1)
      {
        xFilter = InterpolationFilter::Filter::AFFINE_RPR1;
      }

      if (scalingRatio.y > rprThreshold2)
      {
        yFilter = InterpolationFilter::Filter::AFFINE_RPR2;
      }
      else if (scalingRatio.y > rprThreshold1)
      {
        yFilter = InterpolationFilter::Filter::AFFINE_RPR1;
      }
    }

    if (useAltHpelIf)
    {
      if (xFilter == InterpolationFilter::Filter::DEFAULT && scalingRatio.x == SCALE_1X.x)
      {
        xFilter = InterpolationFilter::Filter::HALFPEL_ALT;
      }
      if (yFilter == InterpolationFilter::Filter::DEFAULT && scalingRatio.y == SCALE_1X.y)
      {
        yFilter = InterpolationFilter::Filter::HALFPEL_ALT;
      }
    }
    const int posShift = ScalingRatio::BITS - 4;

    const int stepX = (scalingRatio.x + 8) >> 4;
    const int stepY = (scalingRatio.y + 8) >> 4;

    const int offX = 1 << (posShift - shiftHor - 1);
    const int offY = 1 << (posShift - shiftVer - 1);

    const uint32_t scaleX = ::getComponentScaleX(compID, chFmt);
    const uint32_t scaleY = ::getComponentScaleY(compID, chFmt);

    const int64_t posX =
      ((blk.pos().x << scaleX) - (pps.getScalingWindow().getWindowLeftOffset() * SPS::getWinUnitX(chFmt))) >> scaleX;
    const int64_t posY =
      ((blk.pos().y << scaleY) - (pps.getScalingWindow().getWindowTopOffset() * SPS::getWinUnitY(chFmt))) >> scaleY;

    int addX =
      isLuma(compID) ? 0 : int(1 - sps.getHorCollocatedChromaFlag()) * 8 * (scalingRatio.x - SCALE_1X.x);
    int addY =
      isLuma(compID) ? 0 : int(1 - sps.getVerCollocatedChromaFlag()) * 8 * (scalingRatio.y - SCALE_1X.y);

    int boundLeft   = 0;
    int boundRight  = refPicWidth >> scaleX;
    int boundTop    = 0;
    int boundBottom = refPicHeight >> scaleY;
    if( refPic->subPictures.size() > 1 )
    {
      const SubPic& curSubPic = pps.getSubPicFromPos(blk.lumaPos());
      if( curSubPic.getTreatedAsPicFlag() )
      {
        boundLeft   = curSubPic.getSubPicLeft() >> scaleX;
        boundRight  = curSubPic.getSubPicRight() >> scaleX;
        boundTop    = curSubPic.getSubPicTop() >> scaleY;
        boundBottom = curSubPic.getSubPicBottom() >> scaleY;
      }
    }

    int64_t x0Int;
    int64_t y0Int;

    x0Int = ((posX << (4 + scaleX)) + mv.getHor()) * (int64_t) scalingRatio.x + addX;
    x0Int = sgn2(x0Int) * ((abs(x0Int) + (1ull << (7 + scaleX))) >> (8 + scaleX))
            + ((refPic->getScalingWindow().getWindowLeftOffset() * SPS::getWinUnitX(chFmt)) << ((posShift - scaleX)));

    y0Int = ((posY << (4 + scaleY)) + mv.getVer()) * (int64_t) scalingRatio.y + addY;
    y0Int = sgn2(y0Int) * ((abs(y0Int) + (1ull << (7 + scaleY))) >> (8 + scaleY))
            + ((refPic->getScalingWindow().getWindowTopOffset() * SPS::getWinUnitY(chFmt)) << ((posShift - scaleY)));

    const int extSize = isLuma( compID ) ? 1 : 2;
    int vFilterSize = isLuma( compID ) ? NTAPS_LUMA : NTAPS_CHROMA;

    int yInt0 = ( (int32_t)y0Int + offY ) >> posShift;
    yInt0 = std::min( std::max( boundTop - (NTAPS_LUMA / 2), yInt0 ), boundBottom + (NTAPS_LUMA / 2) );

    int xInt0 = ( (int32_t)x0Int + offX ) >> posShift;
    xInt0 = std::min( std::max( boundLeft - (NTAPS_LUMA / 2), xInt0 ), boundRight + (NTAPS_LUMA / 2) );

    int refHeight = ((((int32_t)y0Int + (height-1) * stepY) + offY ) >> posShift) - ((((int32_t)y0Int + 0 * stepY) + offY ) >> posShift) + 1;
    refHeight = std::max<int>( 1, refHeight );

    CHECK(TMP_RPR_HEIGHT < refHeight + vFilterSize - 1 + extSize,
          "Buffer is not large enough, increase MAX_SCALING_RATIO");

    int tmpStride = width;
    int xInt = 0, yInt = 0;

    for( col = 0; col < width; col++ )
    {
      int posX = (int32_t)x0Int + col * stepX;
      xInt = ( posX + offX ) >> posShift;
      xInt = std::min( std::max( boundLeft - (NTAPS_LUMA / 2), xInt ), boundRight + (NTAPS_LUMA / 2) );
      int xFrac = ( ( posX + offX ) >> ( posShift - shiftHor ) ) & ( ( 1 << shiftHor ) - 1 );

      CHECK( xInt0 > xInt, "Wrong horizontal starting point" );

      Position offset = Position( xInt, yInt0 );
      refBuf = refPic->getRecoBuf( CompArea( compID, chFmt, offset, Size( 1, refHeight ) ), wrapRef );

      Pel *const tempBuf = m_filteredBlockTmpRPR + col;

      m_if.filterHor(compID, (Pel *) refBuf.buf - ((vFilterSize >> 1) - 1) * refBuf.stride, refBuf.stride, tempBuf,
                     tmpStride, 1, refHeight + vFilterSize - 1 + extSize, xFrac, false, clpRng, xFilter);
    }

    for( row = 0; row < height; row++ )
    {
      int posY = (int32_t)y0Int + row * stepY;
      yInt = ( posY + offY ) >> posShift;
      yInt = std::min( std::max( boundTop - (NTAPS_LUMA / 2), yInt ), boundBottom + (NTAPS_LUMA / 2) );
      int yFrac = ( ( posY + offY ) >> ( posShift - shiftVer ) ) & ( ( 1 << shiftVer ) - 1 );

      CHECK( yInt0 > yInt, "Wrong vertical starting point" );

      const Pel *const tempBuf = m_filteredBlockTmpRPR + (yInt - yInt0) * tmpStride;

      JVET_J0090_SET_CACHE_ENABLE( false );
      m_if.filterVer(compID, tempBuf + ((vFilterSize >> 1) - 1) * tmpStride, tmpStride, dst + row * dstStride,
                     dstStride, width, 1, yFrac, false, rndRes, clpRng, yFilter);
      JVET_J0090_SET_CACHE_ENABLE( true );
    }
  }

  return scaled;
}

void MergeCtx::setMergeInfo( PredictionUnit& pu, int candIdx ) const
{
  CHECK( candIdx >= numValidMergeCand, "Merge candidate does not exist" );
  pu.regularMergeFlag        = !(pu.ciipFlag || pu.cu->geoFlag);
  pu.mergeFlag               = true;
  pu.mmvdMergeFlag = false;
  pu.interDir                = interDirNeighbours[candIdx];
  pu.cu->imv = (!pu.cu->geoFlag && useAltHpelIf[candIdx]) ? IMV_HPEL : 0;
  pu.mergeIdx                = candIdx;
  pu.mergeType               = CU::isIBC(*pu.cu) ? MergeType::IBC : MergeType::DEFAULT_N;

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    pu.mv[l]     = mvFieldNeighbours[candIdx][l].mv;
    pu.mvd[l]    = Mv();
    pu.refIdx[l] = mvFieldNeighbours[candIdx][l].refIdx;
    pu.mvpIdx[l] = NOT_VALID;
    pu.mvpNum[l] = NOT_VALID;
  }
#if GDR_ENABLED
  CodingStructure &cs = *pu.cs;
  const bool       isEncodeGdrClean =
    cs.sps->getGDREnabledFlag() && cs.pcv->isEncoder
    && ((cs.picture->gdrParam.inGdrInterval && cs.isClean(pu.Y().topRight(), ChannelType::LUMA))
        || (cs.picture->gdrParam.verBoundary == -1));

  if (isEncodeGdrClean)
  {
    for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
    {
      Mv mv = pu.mv[l];

      int refIdx = pu.refIdx[l];

      pu.mvSolid[l] = mvSolid[candIdx][l];
      pu.mvValid[l] = cs.isClean(pu.Y().topRight(), mv, l, refIdx);
    }
  }
#endif

  if (CU::isIBC(*pu.cu))
  {
    pu.bv = pu.mv[REF_PIC_LIST_0];
    pu.bv.changePrecision(MvPrecision::INTERNAL, MvPrecision::ONE);   // used for only integer resolution
    pu.cu->imv = pu.cu->imv == IMV_HPEL ? 0 : pu.cu->imv;
  }
  pu.cu->bcwIdx = (interDirNeighbours[candIdx] == 3) ? bcwIdx[candIdx] : BCW_DEFAULT;

  PU::restrictBiPredMergeCandsOne(pu);
  pu.mmvdEncOptMode = 0;
}

void MergeCtx::getMmvdDeltaMv(const Slice& slice, const MmvdIdx candIdx, Mv deltaMv[NUM_REF_PIC_LIST_01]) const
{
  const int mvdBaseIdx = candIdx.pos.baseIdx;
  const int mvdStep = candIdx.pos.step;
  const int mvdPosition = candIdx.pos.position;

  int offset = 1 << (mvdStep + MV_FRACTIONAL_BITS_DIFF);
  if (slice.getPicHeader()->getDisFracMMVD())
  {
    offset <<= 2;
  }
  const int refList0 = mmvdBaseMv[mvdBaseIdx][REF_PIC_LIST_0].refIdx;
  const int refList1 = mmvdBaseMv[mvdBaseIdx][REF_PIC_LIST_1].refIdx;

  const Mv dMvTable[4] = { Mv(offset,0), Mv(-offset,0), Mv(0, offset), Mv(0, -offset) };
  if ((refList0 != -1) && (refList1 != -1))
  {
    const int poc0 = slice.getRefPOC(REF_PIC_LIST_0, refList0);
    const int poc1 = slice.getRefPOC(REF_PIC_LIST_1, refList1);
    const int currPoc = slice.getPOC();
    deltaMv[0] = dMvTable[mvdPosition];

    if ((poc0 - currPoc) == (poc1 - currPoc))
    {
      deltaMv[1] = deltaMv[0];
    }
    else if (abs(poc1 - currPoc) > abs(poc0 - currPoc))
    {
      const int scale = PU::getDistScaleFactor(currPoc, poc0, currPoc, poc1);
      deltaMv[1] = deltaMv[0];
      const bool isL0RefLongTerm = slice.getRefPic(REF_PIC_LIST_0, refList0)->longTerm;
      const bool isL1RefLongTerm = slice.getRefPic(REF_PIC_LIST_1, refList1)->longTerm;
      if (isL0RefLongTerm || isL1RefLongTerm)
      {
        if ((poc1 - currPoc)*(poc0 - currPoc) > 0)
        {
          deltaMv[0] = deltaMv[1];
        }
        else
        {
          deltaMv[0].set(-1 * deltaMv[1].getHor(), -1 * deltaMv[1].getVer());
        }
      }
      else
      {
        deltaMv[0] = deltaMv[1].getScaledMv(scale);
      }
    }
    else
    {
      const int scale = PU::getDistScaleFactor(currPoc, poc1, currPoc, poc0);
      const bool isL0RefLongTerm = slice.getRefPic(REF_PIC_LIST_0, refList0)->longTerm;
      const bool isL1RefLongTerm = slice.getRefPic(REF_PIC_LIST_1, refList1)->longTerm;
      if (isL0RefLongTerm || isL1RefLongTerm)
      {
        if ((poc1 - currPoc)*(poc0 - currPoc) > 0)
        {
          deltaMv[1] = deltaMv[0];
        }
        else
        {
          deltaMv[1].set(-1 * deltaMv[0].getHor(), -1 * deltaMv[0].getVer());
        }
      }
      else
      {
        deltaMv[1] = deltaMv[0].getScaledMv(scale);
      }
    }
  }
  else if (refList0 != -1)
  {
    deltaMv[0] = dMvTable[mvdPosition];
  }
  else if (refList1 != -1)
  {
    deltaMv[1] = dMvTable[mvdPosition];
  }
}

void MergeCtx::setMmvdMergeCandiInfo(PredictionUnit &pu, const MmvdIdx candIdx)
{
  Mv tempMv[NUM_REF_PIC_LIST_01];

#if GDR_ENABLED
  const CodingStructure &cs = *pu.cs;
  const bool             isEncodeGdrClean =
    cs.sps->getGDREnabledFlag() && cs.pcv->isEncoder
    && ((cs.picture->gdrParam.inGdrInterval && cs.isClean(pu.Y().topRight(), ChannelType::LUMA))
        || (cs.picture->gdrParam.verBoundary == -1));
#endif

  getMmvdDeltaMv(*pu.cs->slice, candIdx, tempMv);
  const int mvdBaseIdx  = candIdx.pos.baseIdx;

  const int refList0 = mmvdBaseMv[mvdBaseIdx][0].refIdx;
  const int refList1 = mmvdBaseMv[mvdBaseIdx][1].refIdx;

  if ((refList0 != -1) && (refList1 != -1))
  {
    pu.interDir = 3;
    pu.mv[REF_PIC_LIST_0]     = mmvdBaseMv[mvdBaseIdx][0].mv + tempMv[0];
    pu.refIdx[REF_PIC_LIST_0] = refList0;
    pu.mv[REF_PIC_LIST_1]     = mmvdBaseMv[mvdBaseIdx][1].mv + tempMv[1];
    pu.refIdx[REF_PIC_LIST_1] = refList1;
#if GDR_ENABLED
    if (isEncodeGdrClean)
    {
      Mv mv0 = pu.mv[REF_PIC_LIST_0];
      Mv mv1 = pu.mv[REF_PIC_LIST_1];

      int refIdx0 = pu.refIdx[REF_PIC_LIST_0];
      int refIdx1 = pu.refIdx[REF_PIC_LIST_1];

      mmvdValid[mvdBaseIdx][0] = cs.isClean(pu.Y().topRight(), mv0, REF_PIC_LIST_0, refIdx0);
      mmvdValid[mvdBaseIdx][1] = cs.isClean(pu.Y().topRight(), mv1, REF_PIC_LIST_1, refIdx1);

      pu.mvSolid[REF_PIC_LIST_0] = mmvdSolid[mvdBaseIdx][0];
      pu.mvSolid[REF_PIC_LIST_1] = mmvdSolid[mvdBaseIdx][1];

      pu.mvValid[REF_PIC_LIST_0] = mmvdValid[mvdBaseIdx][0];
      pu.mvValid[REF_PIC_LIST_1] = mmvdValid[mvdBaseIdx][1];
    }
#endif
  }
  else if (refList0 != -1)
  {
    pu.interDir = 1;
    pu.mv[REF_PIC_LIST_0]     = mmvdBaseMv[mvdBaseIdx][0].mv + tempMv[0];
    pu.refIdx[REF_PIC_LIST_0] = refList0;
    pu.mv[REF_PIC_LIST_1] = Mv(0, 0);
    pu.refIdx[REF_PIC_LIST_1] = -1;

#if GDR_ENABLED
    if (isEncodeGdrClean)
    {
      Mv mv0 = pu.mv[REF_PIC_LIST_0];
      //Mv mv1 = pu.mv[REF_PIC_LIST_1];

      int refIdx0 = pu.refIdx[REF_PIC_LIST_0];
      //int refIdx1 = pu.refIdx[REF_PIC_LIST_1];

      pu.mvSolid[REF_PIC_LIST_0] = mmvdSolid[mvdBaseIdx][0];
      pu.mvSolid[REF_PIC_LIST_1] = true;

      mmvdValid[mvdBaseIdx][0] = cs.isClean(pu.Y().topRight(), mv0, REF_PIC_LIST_0, refIdx0);
      mmvdValid[mvdBaseIdx][1] = true;

      pu.mvValid[REF_PIC_LIST_0] = mmvdValid[mvdBaseIdx][0];
      pu.mvValid[REF_PIC_LIST_1] = true;
    }
#endif
  }
  else if (refList1 != -1)
  {
    pu.interDir = 2;
    pu.mv[REF_PIC_LIST_0] = Mv(0, 0);
    pu.refIdx[REF_PIC_LIST_0] = -1;
    pu.mv[REF_PIC_LIST_1]     = mmvdBaseMv[mvdBaseIdx][1].mv + tempMv[1];
    pu.refIdx[REF_PIC_LIST_1] = refList1;
#if GDR_ENABLED
    if (isEncodeGdrClean)
    {
      // Mv mv0 = pu.mv[REF_PIC_LIST_0];
      Mv mv1 = pu.mv[REF_PIC_LIST_1];

      // int refIdx0 = pu.refIdx[REF_PIC_LIST_0];
      int refIdx1 = pu.refIdx[REF_PIC_LIST_1];

      mmvdValid[mvdBaseIdx][0] = true;
      mmvdValid[mvdBaseIdx][1] = cs.isClean(pu.Y().topRight(), mv1, REF_PIC_LIST_1, refIdx1);

      pu.mvSolid[REF_PIC_LIST_0] = true;
      pu.mvSolid[REF_PIC_LIST_1] = mmvdSolid[mvdBaseIdx][1];

      pu.mvValid[REF_PIC_LIST_0] = true;
      pu.mvValid[REF_PIC_LIST_1] = mmvdValid[mvdBaseIdx][1];
    }
#endif
  }

  pu.mmvdMergeFlag    = true;
  pu.mmvdMergeIdx     = candIdx;
  pu.mergeFlag        = true;
  pu.regularMergeFlag = true;
  pu.mergeIdx         = candIdx.val;
  pu.mergeType        = MergeType::DEFAULT_N;

  pu.mvd[REF_PIC_LIST_0] = Mv();
  pu.mvd[REF_PIC_LIST_1] = Mv();
  pu.mvpIdx[REF_PIC_LIST_0] = NOT_VALID;
  pu.mvpIdx[REF_PIC_LIST_1] = NOT_VALID;
  pu.mvpNum[REF_PIC_LIST_0] = NOT_VALID;
  pu.mvpNum[REF_PIC_LIST_1] = NOT_VALID;
  pu.cu->imv                = mmvdUseAltHpelIf[mvdBaseIdx] ? IMV_HPEL : 0;

  pu.cu->bcwIdx = (interDirNeighbours[mvdBaseIdx] == 3) ? bcwIdx[mvdBaseIdx] : BCW_DEFAULT;

  for (int refList = 0; refList < 2; refList++)
  {
    if (pu.refIdx[refList] >= 0)
    {
      pu.mv[refList].clipToStorageBitDepth();
    }
  }

  PU::restrictBiPredMergeCandsOne(pu);
}
