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

/**
 \file     EncSampleAdaptiveOffset.cpp
 \brief       estimation part of sample adaptive offset class
 */
#include "EncSampleAdaptiveOffset.h"

#include "CommonLib/UnitTools.h"
#include "CommonLib/dtrace_codingstruct.h"
#include "CommonLib/dtrace_buffer.h"
#include "CommonLib/CodingStructure.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

//! \ingroup EncoderLib
//! \{


#define SAOCtx(c) SubCtx( Ctx::Sao, c )


//! rounding with IBDI
inline double xRoundIbdi2(int bitDepth, double x)
{
#if FULL_NBIT
  return ((x) >= 0 ? ((int)((x) + 0.5)) : ((int)((x) -0.5)));
#else
  if (DISTORTION_PRECISION_ADJUSTMENT(bitDepth) == 0)
  {
    return ((x) >= 0 ? ((int)((x) + 0.5)) : ((int)((x) -0.5)));
  }
  else
  {
    return ((x) > 0) ? (int)(((int)(x) + (1 << (DISTORTION_PRECISION_ADJUSTMENT(bitDepth) - 1)))
                             / (1 << DISTORTION_PRECISION_ADJUSTMENT(bitDepth)))
                     : ((int)(((int)(x) - (1 << (DISTORTION_PRECISION_ADJUSTMENT(bitDepth) - 1)))
                              / (1 << DISTORTION_PRECISION_ADJUSTMENT(bitDepth))));
  }
#endif
}

inline double xRoundIbdi(int bitDepth, double x)
{
  return (bitDepth > 8 ? xRoundIbdi2(bitDepth, (x)) : ((x)>=0 ? ((int)((x)+0.5)) : ((int)((x)-0.5)))) ;
}

EncSampleAdaptiveOffset::EncSampleAdaptiveOffset() { ::memset(m_saoDisabledRate, 0, sizeof(m_saoDisabledRate)); }

EncSampleAdaptiveOffset::~EncSampleAdaptiveOffset()
{
  destroyEncData();
}

void EncSampleAdaptiveOffset::createEncData(bool isPreDBFSamplesUsed, uint32_t numCTUsPic)
{
  //statistics
  const uint32_t sizeInCtus = numCTUsPic;
  m_statData.resize( sizeInCtus );

  for(uint32_t i=0; i< sizeInCtus; i++)
  {
    m_statData[i] = new StatDataArray[MAX_NUM_COMPONENT];
  }

  if(isPreDBFSamplesUsed)
  {
    m_preDBFstatData.resize( sizeInCtus );
    for(uint32_t i=0; i< sizeInCtus; i++)
    {
      m_preDBFstatData[i] = new StatDataArray[MAX_NUM_COMPONENT];
    }
  }

  for (const auto typeIdc: { SAOModeNewTypes::EO_0, SAOModeNewTypes::EO_90, SAOModeNewTypes::EO_135,
                             SAOModeNewTypes::EO_45, SAOModeNewTypes::BO })
  {
    m_skipLinesR[COMPONENT_Y ][typeIdc]= 5;
    m_skipLinesR[COMPONENT_Cb][typeIdc]= m_skipLinesR[COMPONENT_Cr][typeIdc]= 3;

    m_skipLinesB[COMPONENT_Y ][typeIdc]= 4;
    m_skipLinesB[COMPONENT_Cb][typeIdc]= m_skipLinesB[COMPONENT_Cr][typeIdc]= 2;

    if(isPreDBFSamplesUsed)
    {
      switch (typeIdc)
      {
      case SAOModeNewTypes::EO_0:
        m_skipLinesR[COMPONENT_Y][typeIdc]  = 5;
        m_skipLinesR[COMPONENT_Cb][typeIdc] = m_skipLinesR[COMPONENT_Cr][typeIdc] = 3;

        m_skipLinesB[COMPONENT_Y][typeIdc]  = 3;
        m_skipLinesB[COMPONENT_Cb][typeIdc] = m_skipLinesB[COMPONENT_Cr][typeIdc] = 1;
        break;

      case SAOModeNewTypes::EO_90:
        m_skipLinesR[COMPONENT_Y][typeIdc]  = 4;
        m_skipLinesR[COMPONENT_Cb][typeIdc] = m_skipLinesR[COMPONENT_Cr][typeIdc] = 2;

        m_skipLinesB[COMPONENT_Y][typeIdc]  = 4;
        m_skipLinesB[COMPONENT_Cb][typeIdc] = m_skipLinesB[COMPONENT_Cr][typeIdc] = 2;
        break;

      case SAOModeNewTypes::EO_135:
      case SAOModeNewTypes::EO_45:
        m_skipLinesR[COMPONENT_Y][typeIdc]  = 5;
        m_skipLinesR[COMPONENT_Cb][typeIdc] = m_skipLinesR[COMPONENT_Cr][typeIdc] = 3;

        m_skipLinesB[COMPONENT_Y][typeIdc]  = 4;
        m_skipLinesB[COMPONENT_Cb][typeIdc] = m_skipLinesB[COMPONENT_Cr][typeIdc] = 2;
        break;

      case SAOModeNewTypes::BO:
        m_skipLinesR[COMPONENT_Y][typeIdc]  = 4;
        m_skipLinesR[COMPONENT_Cb][typeIdc] = m_skipLinesR[COMPONENT_Cr][typeIdc] = 2;

        m_skipLinesB[COMPONENT_Y][typeIdc]  = 3;
        m_skipLinesB[COMPONENT_Cb][typeIdc] = m_skipLinesB[COMPONENT_Cr][typeIdc] = 1;
        break;

      default:
        THROW("Not a supported type");
        break;
      }
    }
  }
}

void EncSampleAdaptiveOffset::destroyEncData()
{
  for(uint32_t i=0; i< m_statData.size(); i++)
  {
    delete[] m_statData[i];
  }
  m_statData.clear();


  for(int i=0; i< m_preDBFstatData.size(); i++)
  {
    delete[] m_preDBFstatData[i];
  }
  m_preDBFstatData.clear();
}

void EncSampleAdaptiveOffset::initCABACEstimator(CABACEncoder *cabacEncoder, CtxPool *ctxPool, Slice *pcSlice)
{
  m_CABACEstimator = cabacEncoder->getCABACEstimator( pcSlice->getSPS() );
  m_ctxPool        = ctxPool;
  m_CABACEstimator->initCtxModels( *pcSlice );
  m_CABACEstimator->resetBits();
}

void EncSampleAdaptiveOffset::SAOProcess(CodingStructure &cs, bool *sliceEnabled, const double *lambdas,
#if ENABLE_QPA
                                         const double lambdaChromaWeight,
#endif
                                         const bool testSAODisableAtPictureLevel, const double saoEncodingRate,
                                         const double saoEncodingRateChroma, const bool isPreDBFSamplesUsed,
                                         bool isGreedyMergeEncoding, bool usingTrueOrg)
{
  PelUnitBuf org = usingTrueOrg ? cs.getTrueOrgBuf() : cs.getOrgBuf();
  PelUnitBuf res = cs.getRecoBuf();
  PelUnitBuf src = m_tempBuf;
  memcpy(m_lambda, lambdas, sizeof(m_lambda));

  src.copyFrom(res);

  //collect statistics
  getStatistics(m_statData, org, src, cs);
  if(isPreDBFSamplesUsed)
  {
    addPreDBFStatistics(m_statData);
  }

  //slice on/off
  decidePicParams(*cs.slice, sliceEnabled, saoEncodingRate, saoEncodingRateChroma);

  //block on/off
  std::vector<SAOBlkParam> reconParams(cs.pcv->sizeInCtus);
  decideBlkParams(cs, sliceEnabled, m_statData, src, res, &reconParams[0], cs.picture->getSAO(),
                  testSAODisableAtPictureLevel,
#if ENABLE_QPA
                  lambdaChromaWeight,
#endif
                  saoEncodingRate, saoEncodingRateChroma, isGreedyMergeEncoding);

  DTRACE_UPDATE(g_trace_ctx, (std::make_pair("poc", cs.slice->getPOC())));
  DTRACE_PIC_COMP(D_REC_CB_LUMA_SAO, cs, cs.getRecoBuf(), COMPONENT_Y);
  DTRACE_PIC_COMP(D_REC_CB_CHROMA_SAO, cs, cs.getRecoBuf(), COMPONENT_Cb);
  DTRACE_PIC_COMP(D_REC_CB_CHROMA_SAO, cs, cs.getRecoBuf(), COMPONENT_Cr);

  DTRACE    ( g_trace_ctx, D_CRC, "SAO" );
  DTRACE_CRC( g_trace_ctx, D_CRC, cs, cs.getRecoBuf() );
}

void EncSampleAdaptiveOffset::getPreDBFStatistics( CodingStructure& cs, bool usingTrueOrg )
{
  PelUnitBuf org = usingTrueOrg ? cs.getTrueOrgBuf() : cs.getOrgBuf();
  PelUnitBuf rec = cs.getRecoBuf();
  getStatistics(m_preDBFstatData, org, rec, cs, true);
}

void EncSampleAdaptiveOffset::addPreDBFStatistics(std::vector<StatDataArray *> &blkStats)
{
  const uint32_t numCTUsPic = (uint32_t)blkStats.size();
  for(uint32_t n=0; n< numCTUsPic; n++)
  {
    for(uint32_t compIdx=0; compIdx < MAX_NUM_COMPONENT; compIdx++)
    {
      for (const auto typeIdc: { SAOModeNewTypes::EO_0, SAOModeNewTypes::EO_90, SAOModeNewTypes::EO_135,
                                 SAOModeNewTypes::EO_45, SAOModeNewTypes::BO })
      {
        blkStats[n][compIdx][typeIdc] += m_preDBFstatData[n][compIdx][typeIdc];
      }
    }
  }
}

void EncSampleAdaptiveOffset::getStatistics(std::vector<StatDataArray *> &blkStats, PelUnitBuf &orgYuv,
                                            PelUnitBuf &srcYuv, CodingStructure &cs, bool isCalculatePreDeblockSamples)
{
  bool isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail, isAboveLeftAvail, isAboveRightAvail;

  const PreCalcValues& pcv = *cs.pcv;
  const int numberOfComponents = getNumberValidComponents(pcv.chrFormat);

  size_t lineBufferSize = pcv.maxCUWidth + 1;
  if (m_signLineBuf1.size() != lineBufferSize)
  {
    m_signLineBuf1.resize(lineBufferSize);
    m_signLineBuf2.resize(lineBufferSize);
  }

  int ctuRsAddr = 0;
  for( uint32_t yPos = 0; yPos < pcv.lumaHeight; yPos += pcv.maxCUHeight )
  {
    for( uint32_t xPos = 0; xPos < pcv.lumaWidth; xPos += pcv.maxCUWidth )
    {
      const uint32_t width  = (xPos + pcv.maxCUWidth  > pcv.lumaWidth)  ? (pcv.lumaWidth - xPos)  : pcv.maxCUWidth;
      const uint32_t height = (yPos + pcv.maxCUHeight > pcv.lumaHeight) ? (pcv.lumaHeight - yPos) : pcv.maxCUHeight;
      const UnitArea area( cs.area.chromaFormat, Area(xPos , yPos, width, height) );

      deriveLoopFilterBoundaryAvailability(cs, area.Y(), isLeftAvail, isAboveAvail, isAboveLeftAvail);

      //NOTE: The number of skipped lines during gathering CTU statistics depends on the slice boundary availabilities.
      //For simplicity, here only picture boundaries are considered.

      isRightAvail      = (xPos + pcv.maxCUWidth  < pcv.lumaWidth );
      isBelowAvail      = (yPos + pcv.maxCUHeight < pcv.lumaHeight);
      isAboveRightAvail = ((yPos > 0) && (isRightAvail));

      int numHorVirBndry = 0, numVerVirBndry = 0;
      int horVirBndryPos[] = { -1,-1,-1 };
      int verVirBndryPos[] = { -1,-1,-1 };
      int horVirBndryPosComp[] = { -1,-1,-1 };
      int verVirBndryPosComp[] = { -1,-1,-1 };
      bool isCtuCrossedByVirtualBoundaries = isCrossedByVirtualBoundaries(xPos, yPos, width, height, numHorVirBndry, numVerVirBndry, horVirBndryPos, verVirBndryPos, cs.picHeader );

      for(int compIdx = 0; compIdx < numberOfComponents; compIdx++)
      {
        const ComponentID compID = ComponentID(compIdx);
        const CompArea& compArea = area.block( compID );

        ptrdiff_t srcStride  = srcYuv.get(compID).stride;
        Pel* srcBlk     = srcYuv.get(compID).bufAt( compArea );

        ptrdiff_t orgStride  = orgYuv.get(compID).stride;
        Pel* orgBlk     = orgYuv.get(compID).bufAt( compArea );

        for (int i = 0; i < numHorVirBndry; i++)
        {
          horVirBndryPosComp[i] = (horVirBndryPos[i] >> ::getComponentScaleY(compID, area.chromaFormat)) - compArea.y;
        }
        for (int i = 0; i < numVerVirBndry; i++)
        {
          verVirBndryPosComp[i] = (verVirBndryPos[i] >> ::getComponentScaleX(compID, area.chromaFormat)) - compArea.x;
        }

        getBlkStats(compID, cs.sps->getBitDepth(toChannelType(compID)), blkStats[ctuRsAddr][compID], srcBlk, orgBlk,
                    srcStride, orgStride, compArea.width, compArea.height, isLeftAvail, isRightAvail, isAboveAvail,
                    isBelowAvail, isAboveLeftAvail, isAboveRightAvail, isCalculatePreDeblockSamples,
                    isCtuCrossedByVirtualBoundaries, horVirBndryPosComp, verVirBndryPosComp, numHorVirBndry,
                    numVerVirBndry);
      }
      ctuRsAddr++;
    }
  }
}

void EncSampleAdaptiveOffset::decidePicParams(const Slice& slice, bool* sliceEnabled, const double saoEncodingRate, const double saoEncodingRateChroma)
{
  if ( slice.getPendingRasInit() )
  { // reset
    for (int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++)
    {
      for (int tempLayer = 1; tempLayer < MAX_TLAYER; tempLayer++)
      {
        m_saoDisabledRate[compIdx][tempLayer] = 0.0;
      }
    }
  }

  const int hierPredLayerIdx = slice.getHierPredLayerIdx();

  //decide sliceEnabled[compIdx]
  const int numberOfComponents = m_numberOfComponents;
  for (int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++)
  {
    sliceEnabled[compIdx] = false;
  }

  for (int compIdx = 0; compIdx < numberOfComponents; compIdx++)
  {
    // reset flags & counters
    sliceEnabled[compIdx] = true;

    if (saoEncodingRate>0.0)
    {
      if (saoEncodingRateChroma>0.0)
      {
        // decide slice-level on/off based on previous results
        if (hierPredLayerIdx > 0
            && (m_saoDisabledRate[compIdx][hierPredLayerIdx - 1]
                > ((compIdx == COMPONENT_Y) ? saoEncodingRate : saoEncodingRateChroma)))
        {
          sliceEnabled[compIdx] = false;
        }
      }
      else
      {
        // decide slice-level on/off based on previous results
        if (hierPredLayerIdx > 0 && (m_saoDisabledRate[COMPONENT_Y][0] > saoEncodingRate))
        {
          sliceEnabled[compIdx] = false;
        }
      }
    }
  }
}

int64_t EncSampleAdaptiveOffset::getDistortion(const int channelBitDepth, SAOModeNewTypes typeIdc, int typeAuxInfo,
                                               int *invQuantOffset, SAOStatData &statData)
{
  int64_t dist = 0;

  const int shift = 2 * DISTORTION_PRECISION_ADJUSTMENT(channelBitDepth);

  switch(typeIdc)
  {
  case SAOModeNewTypes::EO_0:
  case SAOModeNewTypes::EO_90:
  case SAOModeNewTypes::EO_135:
  case SAOModeNewTypes::EO_45:
    for (int offsetIdx = 0; offsetIdx < NUM_SAO_EO_CLASSES; offsetIdx++)
    {
      dist += estSaoDist(statData.count[offsetIdx], invQuantOffset[offsetIdx], statData.diff[offsetIdx], shift);
    }
    break;
  case SAOModeNewTypes::BO:
    for (int offsetIdx = typeAuxInfo; offsetIdx < typeAuxInfo + 4; offsetIdx++)
    {
      const int bandIdx = offsetIdx % NUM_SAO_BO_CLASSES;
      dist += estSaoDist(statData.count[bandIdx], invQuantOffset[bandIdx], statData.diff[bandIdx], shift);
    }
    break;
  default:
    THROW("Not a supported type");
    break;
  }

  return dist;
}

int EncSampleAdaptiveOffset::estIterOffset(SAOModeNewTypes typeIdx, double lambda, int offsetInput, int64_t count,
                                           int64_t diffSum, int shift, int bitIncrease, int64_t &bestDist,
                                           double &bestCost, int offsetTh)
{
  int iterOffset, tempOffset;
  int64_t tempDist, tempRate;
  double tempCost, tempMinCost;
  int offsetOutput = 0;
  iterOffset = offsetInput;
  // Assuming sending quantized value 0 results in zero offset and sending the value zero needs 1 bit. entropy coder can be used to measure the exact rate here.
  tempMinCost = lambda;
  while (iterOffset != 0)
  {
    // Calculate the bits required for signaling the offset
    tempRate = (typeIdx == SAOModeNewTypes::BO) ? (abs((int) iterOffset) + 2) : (abs((int) iterOffset) + 1);
    if (abs((int)iterOffset)==offsetTh) //inclusive
    {
      tempRate --;
    }
    // Do the dequantization before distortion calculation
    tempOffset  = iterOffset << bitIncrease;
    tempDist    = estSaoDist( count, tempOffset, diffSum, shift);
    tempCost    = ((double)tempDist + lambda * (double) tempRate);
    if(tempCost < tempMinCost)
    {
      tempMinCost = tempCost;
      offsetOutput = iterOffset;
      bestDist = tempDist;
      bestCost = tempCost;
    }
    iterOffset = (iterOffset > 0) ? (iterOffset-1):(iterOffset+1);
  }
  return offsetOutput;
}

void EncSampleAdaptiveOffset::deriveOffsets(ComponentID compIdx, const int channelBitDepth, SAOModeNewTypes typeIdc,
                                            SAOStatData &statData, int *quantOffsets, int &typeAuxInfo)
{
  int bitDepth = channelBitDepth;
  int shift = 2 * DISTORTION_PRECISION_ADJUSTMENT(bitDepth);
  int offsetTh = SampleAdaptiveOffset::getMaxOffsetQVal(channelBitDepth);  //inclusive

  ::memset(quantOffsets, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);

  //derive initial offsets
  int numClasses = (typeIdc == SAOModeNewTypes::BO) ? ((int) NUM_SAO_BO_CLASSES) : ((int) NUM_SAO_EO_CLASSES);
  for(int classIdx=0; classIdx< numClasses; classIdx++)
  {
    if ((typeIdc != SAOModeNewTypes::BO) && (classIdx == SAO_CLASS_EO_PLAIN))
    {
      continue; //offset will be zero
    }

    if(statData.count[classIdx] == 0)
    {
      continue; //offset will be zero
    }

    quantOffsets[classIdx] =
      (int) xRoundIbdi(bitDepth, (double)(statData.diff[classIdx] << DISTORTION_PRECISION_ADJUSTMENT(bitDepth))
                                   / (double)(statData.count[classIdx] << m_offsetStepLog2[compIdx]));
    quantOffsets[classIdx] = Clip3(-offsetTh, offsetTh, quantOffsets[classIdx]);
  }

  // adjust offsets
  switch(typeIdc)
  {
  case SAOModeNewTypes::EO_0:
  case SAOModeNewTypes::EO_90:
  case SAOModeNewTypes::EO_135:
  case SAOModeNewTypes::EO_45:
  {
    int64_t classDist;
    double  classCost;
    for (int classIdx = 0; classIdx < NUM_SAO_EO_CLASSES; classIdx++)
    {
      if (classIdx == SAO_CLASS_EO_FULL_VALLEY && quantOffsets[classIdx] < 0)
      {
        quantOffsets[classIdx] = 0;
      }
      if (classIdx == SAO_CLASS_EO_HALF_VALLEY && quantOffsets[classIdx] < 0)
      {
        quantOffsets[classIdx] = 0;
      }
      if (classIdx == SAO_CLASS_EO_HALF_PEAK && quantOffsets[classIdx] > 0)
      {
        quantOffsets[classIdx] = 0;
      }
      if (classIdx == SAO_CLASS_EO_FULL_PEAK && quantOffsets[classIdx] > 0)
      {
        quantOffsets[classIdx] = 0;
      }

      if (quantOffsets[classIdx] != 0)   // iterative adjustment only when derived offset is not zero
      {
        quantOffsets[classIdx] =
          estIterOffset(typeIdc, m_lambda[compIdx], quantOffsets[classIdx], statData.count[classIdx],
                        statData.diff[classIdx], shift, m_offsetStepLog2[compIdx], classDist, classCost, offsetTh);
      }
    }

    typeAuxInfo = 0;
  }
      break;
      case SAOModeNewTypes::BO:
      {
        int64_t  distBOClasses[NUM_SAO_BO_CLASSES];
        double costBOClasses[NUM_SAO_BO_CLASSES];
        ::memset(distBOClasses, 0, sizeof(int64_t)*NUM_SAO_BO_CLASSES);
        for(int classIdx=0; classIdx< NUM_SAO_BO_CLASSES; classIdx++)
        {
          costBOClasses[classIdx]= m_lambda[compIdx];
          if( quantOffsets[classIdx] != 0 ) //iterative adjustment only when derived offset is not zero
          {
            quantOffsets[classIdx] = estIterOffset(
              typeIdc, m_lambda[compIdx], quantOffsets[classIdx], statData.count[classIdx], statData.diff[classIdx],
              shift, m_offsetStepLog2[compIdx], distBOClasses[classIdx], costBOClasses[classIdx], offsetTh);
          }
        }

        //decide the starting band index
        double minCost = MAX_DOUBLE, cost;
        for(int band=0; band< NUM_SAO_BO_CLASSES- 4+ 1; band++)
        {
          cost  = costBOClasses[band  ];
          cost += costBOClasses[band+1];
          cost += costBOClasses[band+2];
          cost += costBOClasses[band+3];

          if(cost < minCost)
          {
            minCost = cost;
            typeAuxInfo = band;
          }
        }
        //clear those unused classes
        int clearQuantOffset[NUM_SAO_BO_CLASSES];
        ::memset(clearQuantOffset, 0, sizeof(int)*NUM_SAO_BO_CLASSES);
        for(int i=0; i< 4; i++)
        {
          int band = (typeAuxInfo+i)%NUM_SAO_BO_CLASSES;
          clearQuantOffset[band] = quantOffsets[band];
        }
        ::memcpy(quantOffsets, clearQuantOffset, sizeof(int)*NUM_SAO_BO_CLASSES);
      }
      break;
    default:
      {
        THROW("Not a supported type");
      }
  }
}

void EncSampleAdaptiveOffset::deriveModeNewRDO(const BitDepths &bitDepths, int ctuRsAddr, MergeBlkParams &mergeList,
                                               bool *sliceEnabled, std::vector<StatDataArray *> &blkStats,
                                               SAOBlkParam &modeParam, double &modeNormCost)
{
  double minCost, cost;
  uint64_t previousFracBits;
  const int numberOfComponents = m_numberOfComponents;

  int64_t dist[MAX_NUM_COMPONENT], modeDist[MAX_NUM_COMPONENT];
  SAOOffset testOffset[MAX_NUM_COMPONENT];
  int invQuantOffset[MAX_NUM_SAO_CLASSES];
  for(int comp=0; comp < MAX_NUM_COMPONENT; comp++)
  {
    modeDist[comp] = 0;
  }

  //pre-encode merge flags
  modeParam[COMPONENT_Y].modeIdc = SAOMode::OFF;
  const TempCtx ctxStartBlk(m_ctxPool, SAOCtx(m_CABACEstimator->getCtx()));
  m_CABACEstimator->sao_block_params(modeParam, bitDepths, sliceEnabled,
                                     (mergeList[SAOModeMergeTypes::LEFT] != nullptr),
                                     (mergeList[SAOModeMergeTypes::ABOVE] != nullptr), true);
  const TempCtx ctxStartLuma(m_ctxPool, SAOCtx(m_CABACEstimator->getCtx()));
  TempCtx       ctxBestLuma(m_ctxPool);

  //------ luma --------//
  {
    const ComponentID compIdx = COMPONENT_Y;
    //"off" case as initial cost
    modeParam[compIdx].modeIdc = SAOMode::OFF;
    m_CABACEstimator->resetBits();
    m_CABACEstimator->sao_offset_params(modeParam[compIdx], compIdx, sliceEnabled[compIdx],
                                        bitDepths[ChannelType::LUMA]);
    modeDist[compIdx] = 0;
    minCost           = m_lambda[compIdx] * (FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits());
    ctxBestLuma = SAOCtx( m_CABACEstimator->getCtx() );
    if(sliceEnabled[compIdx])
    {
      for (const auto typeIdc: { SAOModeNewTypes::EO_0, SAOModeNewTypes::EO_90, SAOModeNewTypes::EO_135,
                                 SAOModeNewTypes::EO_45, SAOModeNewTypes::BO })
      {
        testOffset[compIdx].modeIdc         = SAOMode::NEW;
        testOffset[compIdx].typeIdc.newType = typeIdc;

        //derive coded offset
        deriveOffsets(compIdx, bitDepths[ChannelType::LUMA], typeIdc, blkStats[ctuRsAddr][compIdx][typeIdc],
                      testOffset[compIdx].offset, testOffset[compIdx].typeAuxInfo);

        //inversed quantized offsets
        invertQuantOffsets(compIdx, typeIdc, testOffset[compIdx].typeAuxInfo, invQuantOffset, testOffset[compIdx].offset);

        //get distortion
        dist[compIdx] =
          getDistortion(bitDepths[ChannelType::LUMA], testOffset[compIdx].typeIdc.newType,
                        testOffset[compIdx].typeAuxInfo, invQuantOffset, blkStats[ctuRsAddr][compIdx][typeIdc]);

        //get rate
        m_CABACEstimator->getCtx() = SAOCtx( ctxStartLuma );
        m_CABACEstimator->resetBits();
        m_CABACEstimator->sao_offset_params(testOffset[compIdx], compIdx, sliceEnabled[compIdx],
                                            bitDepths[ChannelType::LUMA]);
        double rate = FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();
        cost = (double)dist[compIdx] + m_lambda[compIdx]*rate;
        if(cost < minCost)
        {
          minCost = cost;
          modeDist[compIdx] = dist[compIdx];
          modeParam[compIdx]= testOffset[compIdx];
          ctxBestLuma = SAOCtx( m_CABACEstimator->getCtx() );
        }
      }
    }
    m_CABACEstimator->getCtx() = SAOCtx( ctxBestLuma );
  }

  //------ chroma --------//
//"off" case as initial cost
  cost = 0;
  previousFracBits = 0;
  m_CABACEstimator->resetBits();
  for(uint32_t componentIndex = COMPONENT_Cb; componentIndex < numberOfComponents; componentIndex++)
  {
    const ComponentID component = ComponentID(componentIndex);

    modeParam[component].modeIdc = SAOMode::OFF;
    modeDist [component]         = 0;
    m_CABACEstimator->sao_offset_params(modeParam[component], component, sliceEnabled[component],
                                        bitDepths[ChannelType::CHROMA]);
    const uint64_t currentFracBits = m_CABACEstimator->getEstFracBits();
    cost += m_lambda[component] * FRAC_BITS_SCALE * (currentFracBits - previousFracBits);
    previousFracBits = currentFracBits;
  }

  minCost = cost;

  //doesn't need to store cabac status here since the whole CTU parameters will be re-encoded at the end of this function

  for (const auto typeIdc: { SAOModeNewTypes::EO_0, SAOModeNewTypes::EO_90, SAOModeNewTypes::EO_135,
                             SAOModeNewTypes::EO_45, SAOModeNewTypes::BO })
  {
    m_CABACEstimator->getCtx() = SAOCtx( ctxBestLuma );
    m_CABACEstimator->resetBits();
    previousFracBits = 0;
    cost = 0;

    for(uint32_t componentIndex = COMPONENT_Cb; componentIndex < numberOfComponents; componentIndex++)
    {
      const ComponentID component = ComponentID(componentIndex);
      if(!sliceEnabled[component])
      {
        testOffset[component].modeIdc = SAOMode::OFF;
        dist[component]= 0;
        continue;
      }
      testOffset[component].modeIdc         = SAOMode::NEW;
      testOffset[component].typeIdc.newType = typeIdc;

      //derive offset & get distortion
      deriveOffsets(component, bitDepths[ChannelType::CHROMA], typeIdc, blkStats[ctuRsAddr][component][typeIdc],
                    testOffset[component].offset, testOffset[component].typeAuxInfo);
      invertQuantOffsets(component, typeIdc, testOffset[component].typeAuxInfo, invQuantOffset, testOffset[component].offset);
      dist[component] = getDistortion(bitDepths[ChannelType::CHROMA], typeIdc, testOffset[component].typeAuxInfo,
                                      invQuantOffset, blkStats[ctuRsAddr][component][typeIdc]);
      m_CABACEstimator->sao_offset_params(testOffset[component], component, sliceEnabled[component],
                                          bitDepths[ChannelType::CHROMA]);
      const uint64_t currentFracBits = m_CABACEstimator->getEstFracBits();
      cost += dist[component] + (m_lambda[component] * FRAC_BITS_SCALE * (currentFracBits - previousFracBits));
      previousFracBits = currentFracBits;
    }

    if(cost < minCost)
    {
      minCost = cost;
      for(uint32_t componentIndex = COMPONENT_Cb; componentIndex < numberOfComponents; componentIndex++)
      {
        modeDist[componentIndex]  = dist[componentIndex];
        modeParam[componentIndex] = testOffset[componentIndex];
      }
    }

  } // SAO_TYPE loop

  //----- re-gen rate & normalized cost----//
  modeNormCost = 0;
  for(uint32_t componentIndex = COMPONENT_Y; componentIndex < numberOfComponents; componentIndex++)
  {
    modeNormCost += (double)modeDist[componentIndex] / m_lambda[componentIndex];
  }

  m_CABACEstimator->getCtx() = SAOCtx( ctxStartBlk );
  m_CABACEstimator->resetBits();
  m_CABACEstimator->sao_block_params(modeParam, bitDepths, sliceEnabled,
                                     (mergeList[SAOModeMergeTypes::LEFT] != nullptr),
                                     (mergeList[SAOModeMergeTypes::ABOVE] != nullptr), false);
  modeNormCost += FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();
}

void EncSampleAdaptiveOffset::deriveModeMergeRDO(const BitDepths &bitDepths, int ctuRsAddr, MergeBlkParams &mergeList,
                                                 bool *sliceEnabled, std::vector<StatDataArray *> &blkStats,
                                                 SAOBlkParam &modeParam, double &modeNormCost)
{
  modeNormCost = MAX_DOUBLE;

  double cost;
  SAOBlkParam testBlkParam;
  const int numberOfComponents = m_numberOfComponents;

  const TempCtx ctxStart(m_ctxPool, SAOCtx(m_CABACEstimator->getCtx()));
  TempCtx       ctxBest(m_ctxPool);

  for (const auto mergeType: { SAOModeMergeTypes::LEFT, SAOModeMergeTypes::ABOVE })
  {
    if (mergeList[mergeType] == nullptr)
    {
      continue;
    }

    testBlkParam = *(mergeList[mergeType]);
    //normalized distortion
    double normDist=0;
    for(int compIdx = 0; compIdx < numberOfComponents; compIdx++)
    {
      testBlkParam[compIdx].modeIdc           = SAOMode::MERGE;
      testBlkParam[compIdx].typeIdc.mergeType = mergeType;

      SAOOffset& mergedOffsetParam = (*(mergeList[mergeType]))[compIdx];

      if (mergedOffsetParam.modeIdc != SAOMode::OFF)
      {
        //offsets have been reconstructed. Don't call inversed quantization function.
        normDist +=
          (((double) getDistortion(bitDepths[toChannelType(ComponentID(compIdx))], mergedOffsetParam.typeIdc.newType,
                                   mergedOffsetParam.typeAuxInfo, mergedOffsetParam.offset,
                                   blkStats[ctuRsAddr][compIdx][mergedOffsetParam.typeIdc.newType]))
           / m_lambda[compIdx]);
      }
    }

    //rate
    m_CABACEstimator->getCtx() = SAOCtx( ctxStart );
    m_CABACEstimator->resetBits();
    m_CABACEstimator->sao_block_params(testBlkParam, bitDepths, sliceEnabled,
                                       (mergeList[SAOModeMergeTypes::LEFT] != nullptr),
                                       (mergeList[SAOModeMergeTypes::ABOVE] != nullptr), false);
    double rate = FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();
    cost = normDist+rate;

    if(cost < modeNormCost)
    {
      modeNormCost = cost;
      modeParam    = testBlkParam;
      ctxBest      = SAOCtx( m_CABACEstimator->getCtx() );
    }
  }
  if( modeNormCost < MAX_DOUBLE )
  {
    m_CABACEstimator->getCtx() = SAOCtx( ctxBest );
  }
}

void EncSampleAdaptiveOffset::decideBlkParams(CodingStructure &cs, bool *sliceEnabled,
                                              std::vector<StatDataArray *> &blkStats, PelUnitBuf &srcYuv,
                                              PelUnitBuf &resYuv, SAOBlkParam *reconParams, SAOBlkParam *codedParams,
                                              const bool testSAODisableAtPictureLevel,
#if ENABLE_QPA
                                              const double chromaWeight,
#endif
                                              const double saoEncodingRate, const double saoEncodingRateChroma,
                                              const bool isGreedymergeEncoding)

{
  const PreCalcValues& pcv = *cs.pcv;
  bool allBlksDisabled = true;
  const uint32_t numberOfComponents = m_numberOfComponents;
  for(uint32_t compId = COMPONENT_Y; compId < numberOfComponents; compId++)
  {
    if (sliceEnabled[compId])
    {
      allBlksDisabled = false;
    }
  }

  const TempCtx ctxPicStart(m_ctxPool, SAOCtx(m_CABACEstimator->getCtx()));

  SAOBlkParam modeParam;
  double minCost, modeCost;

  double minCost2 = 0;
  std::vector<StatDataArray *> groupBlkStat;
  if (isGreedymergeEncoding)
  {
    groupBlkStat.resize(cs.pcv->sizeInCtus);
    for (uint32_t k = 0; k < cs.pcv->sizeInCtus; k++)
    {
      groupBlkStat[k] = new StatDataArray[MAX_NUM_COMPONENT];
    }
  }
  SAOBlkParam  testBlkParam;
  SAOBlkParam  groupParam;

  MergeBlkParams tempMergeList;
  MergeBlkParams startingMergeList;
  tempMergeList.fill(nullptr);
  startingMergeList.fill(nullptr);

  int     mergeCtuAddr = 1; //Ctu to be merged
  int     groupSize = 1;
  double  cost[2]      = { 0, 0 };
  TempCtx ctxBeforeMerge(m_ctxPool);
  TempCtx ctxAfterMerge(m_ctxPool);

  double totalCost = 0;   // Used if testSAODisableAtPictureLevel==true

  int ctuRsAddr = 0;
#if ENABLE_QPA
  CHECK ((chromaWeight > 0.0) && (cs.slice->getFirstCtuRsAddrInSlice() != 0), "incompatible start CTU address, must be 0");
#endif

  for( uint32_t yPos = 0; yPos < pcv.lumaHeight; yPos += pcv.maxCUHeight )
  {
    for (uint32_t xPos = 0; xPos < pcv.lumaWidth; xPos += pcv.maxCUWidth, ctuRsAddr++)
    {
      if(allBlksDisabled)
      {
        codedParams[ctuRsAddr].reset();
        continue;
      }

      const uint32_t width  = (xPos + pcv.maxCUWidth > pcv.lumaWidth) ? (pcv.lumaWidth - xPos) : pcv.maxCUWidth;
      const uint32_t height = (yPos + pcv.maxCUHeight > pcv.lumaHeight) ? (pcv.lumaHeight - yPos) : pcv.maxCUHeight;
      const UnitArea area(pcv.chrFormat, Area(xPos, yPos, width, height));

      const TempCtx ctxStart(m_ctxPool, SAOCtx(m_CABACEstimator->getCtx()));
      TempCtx       ctxBest(m_ctxPool);

      if (ctuRsAddr == mergeCtuAddr - 1)
      {
        ctxBeforeMerge = SAOCtx(m_CABACEstimator->getCtx());
      }

      //get merge list
      MergeBlkParams mergeList;
      getMergeList(cs, ctuRsAddr, reconParams, mergeList);

      minCost = MAX_DOUBLE;
#if ENABLE_QPA
      if (chromaWeight > 0.0) // temporarily adopt local (CTU-wise) lambdas from QPA
      {
        for (int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++)
        {
          m_lambda[compIdx] = isLuma((ComponentID) compIdx) ? cs.picture->m_uEnerHpCtu[ctuRsAddr]
                                                            : cs.picture->m_uEnerHpCtu[ctuRsAddr] / chromaWeight;
        }
      }
#endif
      bool firstMode = true;
      for (const auto mode: { SAOMode::NEW, SAOMode::MERGE })
      {
        if (!firstMode)
        {
          m_CABACEstimator->getCtx() = SAOCtx( ctxStart );
        }
        firstMode = false;

        switch (mode)
        {
        case SAOMode::NEW:
          deriveModeNewRDO(cs.sps->getBitDepths(), ctuRsAddr, mergeList, sliceEnabled, blkStats, modeParam, modeCost);
          break;
        case SAOMode::MERGE:
          deriveModeMergeRDO(cs.sps->getBitDepths(), ctuRsAddr, mergeList, sliceEnabled, blkStats, modeParam, modeCost);
          break;
        default:
          THROW("Invalid SAO mode");
          break;
        }

        if (modeCost < minCost)
        {
          minCost                = modeCost;
          codedParams[ctuRsAddr] = modeParam;
          ctxBest                = SAOCtx( m_CABACEstimator->getCtx() );
        }
      }

      if (!isGreedymergeEncoding)
      {
        totalCost += minCost;
      }

      m_CABACEstimator->getCtx() = SAOCtx( ctxBest );

      //apply reconstructed offsets
      reconParams[ctuRsAddr] = codedParams[ctuRsAddr];
      reconstructBlkSAOParam(reconParams[ctuRsAddr], mergeList);

      if (isGreedymergeEncoding)
      {
        if (ctuRsAddr == mergeCtuAddr - 1)
        {
          cost[0]   = minCost;   // previous
          groupSize = 1;
          getMergeList(cs, ctuRsAddr, reconParams, startingMergeList);
        }
        else if (ctuRsAddr == mergeCtuAddr)
        {
          cost[1]  = minCost;
          minCost2 = MAX_DOUBLE;
          for (int tmp = groupSize; tmp >= 0; tmp--)
          {
            for (int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++)
            {
              for (const auto i: { SAOModeNewTypes::EO_0, SAOModeNewTypes::EO_90, SAOModeNewTypes::EO_135,
                                   SAOModeNewTypes::EO_45, SAOModeNewTypes::BO })
              {
                for (int j = 0; j < MAX_NUM_SAO_CLASSES; j++)
                {
                  if (tmp == groupSize)
                  {
                    groupBlkStat[ctuRsAddr][compIdx][i].count[j] = blkStats[ctuRsAddr - tmp][compIdx][i].count[j];
                    groupBlkStat[ctuRsAddr][compIdx][i].diff[j] = blkStats[ctuRsAddr - tmp][compIdx][i].diff[j];
                  }
                  else
                  {
                    groupBlkStat[ctuRsAddr][compIdx][i].count[j] += blkStats[ctuRsAddr - tmp][compIdx][i].count[j];
                    groupBlkStat[ctuRsAddr][compIdx][i].diff[j] += blkStats[ctuRsAddr - tmp][compIdx][i].diff[j];
                  }
                }
              }
            }
          }

          // Derive new offset for grouped CTUs
          m_CABACEstimator->getCtx() = SAOCtx(ctxBeforeMerge);
          deriveModeNewRDO(cs.sps->getBitDepths(), ctuRsAddr, startingMergeList, sliceEnabled, groupBlkStat, modeParam, modeCost);

          //rate for mergeLeft CTB
          testBlkParam[COMPONENT_Y].modeIdc           = SAOMode::MERGE;
          testBlkParam[COMPONENT_Y].typeIdc.mergeType = SAOModeMergeTypes::LEFT;
          m_CABACEstimator->resetBits();
          m_CABACEstimator->sao_block_params(testBlkParam, cs.sps->getBitDepths(), sliceEnabled, true, false, true);
          double rate = FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();
          modeCost += rate * groupSize;
          if (modeCost < minCost2)
          {
            groupParam = modeParam;
            minCost2 = modeCost;
            ctxAfterMerge = SAOCtx(m_CABACEstimator->getCtx());
          }

          // Test merge mode for grouped CTUs
          m_CABACEstimator->getCtx() = SAOCtx(ctxStart);
          deriveModeMergeRDO(cs.sps->getBitDepths(), ctuRsAddr, startingMergeList, sliceEnabled, groupBlkStat, modeParam, modeCost);
          modeCost += rate * groupSize;
          if (modeCost < minCost2)
          {
            minCost2 = modeCost;
            groupParam = modeParam;
            ctxAfterMerge = SAOCtx(m_CABACEstimator->getCtx());
          }

          totalCost += cost[0];
          totalCost += cost[1];

          if ((cost[0] + cost[1]) > minCost2)   // merge current CTU
          {
            //original merge all
            totalCost                          = totalCost - cost[0] - cost[1] + minCost2;
            codedParams[ctuRsAddr - groupSize] = groupParam;
            for (int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++)
            {
              codedParams[ctuRsAddr][compIdx].modeIdc           = SAOMode::MERGE;
              codedParams[ctuRsAddr][compIdx].typeIdc.mergeType = SAOModeMergeTypes::LEFT;
            }
            for (int i = groupSize; i >= 0; i--) //change previous results
            {
              reconParams[ctuRsAddr - i] = codedParams[ctuRsAddr - i];
              getMergeList(cs, ctuRsAddr - i, reconParams, tempMergeList);
              reconstructBlkSAOParam(reconParams[ctuRsAddr - i], tempMergeList);
            }

            mergeCtuAddr += 1;
            if (mergeCtuAddr % pcv.widthInCtus == 0) //reaching the end of a row
            {
              mergeCtuAddr += 1;
            }
            else //next CTU can be merged with current group
            {
              cost[0] = minCost2;
              groupSize += 1;
            }
            m_CABACEstimator->getCtx() = SAOCtx(ctxAfterMerge);
          }
          else // don't merge current CTU
          {
            mergeCtuAddr += 1;
            // Current block will be the starting block for successive operations
            cost[0] = cost[1];
            getMergeList(cs, ctuRsAddr, reconParams, startingMergeList);
            groupSize = 1;
            m_CABACEstimator->getCtx() = SAOCtx(ctxStart);
            ctxBeforeMerge = SAOCtx(m_CABACEstimator->getCtx());
            m_CABACEstimator->getCtx() = SAOCtx(ctxBest);
            if (mergeCtuAddr% pcv.widthInCtus == 0) //reaching the end of a row
            {
              mergeCtuAddr += 1;
            }
          }   // else, if(cost[0] + cost[1] > minCost2)
        }//else if (ctuRsAddr == mergeCtuAddr)
      }
      else
      {
        offsetCTU(area, srcYuv, resYuv, reconParams[ctuRsAddr], cs);
      }
    }   // ctuRsAddr
  }

#if ENABLE_QPA
  // restore global lambdas (might be unnecessary)
  if (chromaWeight > 0.0)
  {
    memcpy(m_lambda, cs.slice->getLambdas(), sizeof(m_lambda));
  }
#endif
  //reconstruct
  if (isGreedymergeEncoding)
  {
    ctuRsAddr = 0;
    for (uint32_t yPos = 0; yPos < pcv.lumaHeight; yPos += pcv.maxCUHeight)
    {
      for (uint32_t xPos = 0; xPos < pcv.lumaWidth; xPos += pcv.maxCUWidth)
      {
        const uint32_t width = (xPos + pcv.maxCUWidth > pcv.lumaWidth) ? (pcv.lumaWidth - xPos) : pcv.maxCUWidth;
        const uint32_t height = (yPos + pcv.maxCUHeight > pcv.lumaHeight) ? (pcv.lumaHeight - yPos) : pcv.maxCUHeight;

        const UnitArea area(pcv.chrFormat, Area(xPos, yPos, width, height));

        offsetCTU(area, srcYuv, resYuv, reconParams[ctuRsAddr], cs);
        ctuRsAddr++;
      }
    }
    //delete memory
    for (uint32_t i = 0; i< groupBlkStat.size(); i++)
    {
      delete[] groupBlkStat[i];
    }
    groupBlkStat.clear();
  }
  if (!allBlksDisabled && (totalCost >= 0)
      && testSAODisableAtPictureLevel)   // SAO has not beneficial in this case - disable it
  {
    for( ctuRsAddr = 0; ctuRsAddr < pcv.sizeInCtus; ctuRsAddr++)
    {
      codedParams[ctuRsAddr].reset();
    }

    for (uint32_t componentIndex = 0; componentIndex < MAX_NUM_COMPONENT; componentIndex++)
    {
      sliceEnabled[componentIndex] = false;
    }
    m_CABACEstimator->getCtx() = SAOCtx(ctxPicStart);

    resYuv.copyFrom(srcYuv);
  }

  disabledRate(cs, reconParams, saoEncodingRate, saoEncodingRateChroma);
}

void EncSampleAdaptiveOffset::disabledRate(CodingStructure &cs, SAOBlkParam *reconParams, const double saoEncodingRate,
                                           const double saoEncodingRateChroma)
{
  if (saoEncodingRate > 0.0)
  {
    const PreCalcValues& pcv = *cs.pcv;

    const uint32_t numberOfComponents = getNumberValidComponents( cs.picture->chromaFormat );

    const int hierPredLayerIdx = cs.slice->getHierPredLayerIdx();

    int numCtusForSAOOff[MAX_NUM_COMPONENT];

    for (int compIdx = 0; compIdx < numberOfComponents; compIdx++)
    {
      numCtusForSAOOff[compIdx] = 0;
      for( int ctuRsAddr=0; ctuRsAddr< pcv.sizeInCtus; ctuRsAddr++)
      {
        if (reconParams[ctuRsAddr][compIdx].modeIdc == SAOMode::OFF)
        {
          numCtusForSAOOff[compIdx]++;
        }
      }
    }
    if (saoEncodingRateChroma > 0.0)
    {
      for (int compIdx = 0; compIdx < numberOfComponents; compIdx++)
      {
        m_saoDisabledRate[compIdx][hierPredLayerIdx] = (double) numCtusForSAOOff[compIdx] / (double) pcv.sizeInCtus;
      }
    }
    else if (hierPredLayerIdx == 0)
    {
      m_saoDisabledRate[COMPONENT_Y][0] =
        (double) (numCtusForSAOOff[COMPONENT_Y] + numCtusForSAOOff[COMPONENT_Cb] + numCtusForSAOOff[COMPONENT_Cr])
        / (double) (pcv.sizeInCtus * 3);
    }
  }
}

void EncSampleAdaptiveOffset::getBlkStats(const ComponentID compIdx, const int channelBitDepth,
                                          StatDataArray &statsDataTypes, Pel *srcBlk, Pel *orgBlk, ptrdiff_t srcStride,
                                          ptrdiff_t orgStride, int width, int height, bool isLeftAvail,
                                          bool isRightAvail, bool isAboveAvail, bool isBelowAvail,
                                          bool isAboveLeftAvail, bool isAboveRightAvail,
                                          bool isCalculatePreDeblockSamples, bool isCtuCrossedByVirtualBoundaries,
                                          int horVirBndryPos[], int verVirBndryPos[], int numHorVirBndry,
                                          int numVerVirBndry)
{
  int x,y, startX, startY, endX, endY, edgeType, firstLineStartX, firstLineEndX;
  int8_t signLeft, signRight, signDown;
  int64_t *diff, *count;
  Pel *srcLine, *orgLine;
  const EnumArray<int, SAOModeNewTypes> &skipLinesR = m_skipLinesR[compIdx];
  const EnumArray<int, SAOModeNewTypes> &skipLinesB = m_skipLinesB[compIdx];

  for (const auto typeIdx: { SAOModeNewTypes::EO_0, SAOModeNewTypes::EO_90, SAOModeNewTypes::EO_135,
                             SAOModeNewTypes::EO_45, SAOModeNewTypes::BO })
  {
    SAOStatData &statsData = statsDataTypes[typeIdx];
    statsData.reset();

    srcLine = srcBlk;
    orgLine = orgBlk;
    diff    = statsData.diff;
    count   = statsData.count;
    switch(typeIdx)
    {
    case SAOModeNewTypes::EO_0:
    {
      diff += 2;
      count += 2;

      endY   = isBelowAvail ? (height - skipLinesB[typeIdx]) : height;
      startX = !isCalculatePreDeblockSamples ? (isLeftAvail ? 0 : 1)
                                             : (isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1));
      endX   = !isCalculatePreDeblockSamples ? (isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1))
                                             : (isRightAvail ? width : (width - 1));

      for (y = 0; y < endY; y++)
      {
        signLeft = (int8_t) sgn(srcLine[startX] - srcLine[startX - 1]);
        for (x = startX; x < endX; x++)
        {
          signRight = (int8_t) sgn(srcLine[x] - srcLine[x + 1]);
          if (isCtuCrossedByVirtualBoundaries
              && isProcessDisabled(x, y, numVerVirBndry, 0, verVirBndryPos, horVirBndryPos))
          {
            signLeft = -signRight;
            continue;
          }
          edgeType = signRight + signLeft;
          signLeft = -signRight;

          diff[edgeType] += (orgLine[x] - srcLine[x]);
          count[edgeType]++;
        }
        srcLine += srcStride;
        orgLine += orgStride;
      }
      if (isCalculatePreDeblockSamples)
      {
        if (isBelowAvail)
        {
          startX = isLeftAvail ? 0 : 1;
          endX   = isRightAvail ? width : (width - 1);

          for (y = 0; y < skipLinesB[typeIdx]; y++)
          {
            signLeft = (int8_t) sgn(srcLine[startX] - srcLine[startX - 1]);
            for (x = startX; x < endX; x++)
            {
              signRight = (int8_t) sgn(srcLine[x] - srcLine[x + 1]);
              if (isCtuCrossedByVirtualBoundaries
                  && isProcessDisabled(x, endY + y, numVerVirBndry, 0, verVirBndryPos, horVirBndryPos))
              {
                signLeft = -signRight;
                continue;
              }
              edgeType = signRight + signLeft;
              signLeft = -signRight;

              diff[edgeType] += (orgLine[x] - srcLine[x]);
              count[edgeType]++;
            }
            srcLine += srcStride;
            orgLine += orgStride;
          }
        }
      }
      break;
    }
    case SAOModeNewTypes::EO_90:
    {
      diff += 2;
      count += 2;
      int8_t *signUpLine = m_signLineBuf1.data();

      startX = (!isCalculatePreDeblockSamples) ? 0 : (isRightAvail ? (width - skipLinesR[typeIdx]) : width);
      startY = isAboveAvail ? 0 : 1;
      endX   = (!isCalculatePreDeblockSamples) ? (isRightAvail ? (width - skipLinesR[typeIdx]) : width) : width;
      endY   = isBelowAvail ? (height - skipLinesB[typeIdx]) : (height - 1);
      if (!isAboveAvail)
      {
        srcLine += srcStride;
        orgLine += orgStride;
      }

      Pel *srcLineAbove = srcLine - srcStride;
      for (x = startX; x < endX; x++)
      {
        signUpLine[x] = (int8_t) sgn(srcLine[x] - srcLineAbove[x]);
      }

      Pel *srcLineBelow;
      for (y = startY; y < endY; y++)
      {
        srcLineBelow = srcLine + srcStride;

        for (x = startX; x < endX; x++)
        {
          signDown = (int8_t) sgn(srcLine[x] - srcLineBelow[x]);
          if (isCtuCrossedByVirtualBoundaries
              && isProcessDisabled(x, y, 0, numHorVirBndry, verVirBndryPos, horVirBndryPos))
          {
            signUpLine[x] = -signDown;
            continue;
          }
          edgeType      = signDown + signUpLine[x];
          signUpLine[x] = -signDown;

          diff[edgeType] += (orgLine[x] - srcLine[x]);
          count[edgeType]++;
        }
        srcLine += srcStride;
        orgLine += orgStride;
      }
      if (isCalculatePreDeblockSamples)
      {
        if (isBelowAvail)
        {
          startX = 0;
          endX   = width;

          for (y = 0; y < skipLinesB[typeIdx]; y++)
          {
            srcLineBelow = srcLine + srcStride;
            srcLineAbove = srcLine - srcStride;

            for (x = startX; x < endX; x++)
            {
              if (isCtuCrossedByVirtualBoundaries
                  && isProcessDisabled(x, y + endY, 0, numHorVirBndry, verVirBndryPos, horVirBndryPos))
              {
                continue;
              }
              edgeType = sgn(srcLine[x] - srcLineBelow[x]) + sgn(srcLine[x] - srcLineAbove[x]);
              diff[edgeType] += (orgLine[x] - srcLine[x]);
              count[edgeType]++;
            }
            srcLine += srcStride;
            orgLine += orgStride;
          }
        }
      }
      break;
    }
    case SAOModeNewTypes::EO_135:
    {
      diff += 2;
      count += 2;
      int8_t *signTmpLine;

      int8_t *signUpLine   = m_signLineBuf1.data();
      int8_t *signDownLine = m_signLineBuf2.data();

      startX = (!isCalculatePreDeblockSamples) ? (isLeftAvail ? 0 : 1)
                                               : (isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1));

      endX = (!isCalculatePreDeblockSamples) ? (isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1))
                                             : (isRightAvail ? width : (width - 1));
      endY = isBelowAvail ? (height - skipLinesB[typeIdx]) : (height - 1);

      // prepare 2nd line's upper sign
      Pel *srcLineBelow = srcLine + srcStride;
      for (x = startX; x < endX + 1; x++)
      {
        signUpLine[x] = (int8_t) sgn(srcLineBelow[x] - srcLine[x - 1]);
      }

      // 1st line
      Pel *srcLineAbove = srcLine - srcStride;
      firstLineStartX   = (!isCalculatePreDeblockSamples) ? (isAboveLeftAvail ? 0 : 1) : startX;
      firstLineEndX     = (!isCalculatePreDeblockSamples) ? (isAboveAvail ? endX : 1) : endX;
      for (x = firstLineStartX; x < firstLineEndX; x++)
      {
        if (isCtuCrossedByVirtualBoundaries
            && isProcessDisabled(x, 0, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
        {
          continue;
        }
        edgeType = sgn(srcLine[x] - srcLineAbove[x - 1]) - signUpLine[x + 1];
        diff[edgeType] += (orgLine[x] - srcLine[x]);
        count[edgeType]++;
      }
      srcLine += srcStride;
      orgLine += orgStride;

      // middle lines
      for (y = 1; y < endY; y++)
      {
        srcLineBelow = srcLine + srcStride;

        for (x = startX; x < endX; x++)
        {
          signDown = (int8_t) sgn(srcLine[x] - srcLineBelow[x + 1]);
          if (isCtuCrossedByVirtualBoundaries
              && isProcessDisabled(x, y, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
          {
            signDownLine[x + 1] = -signDown;
            continue;
          }
          edgeType = signDown + signUpLine[x];
          diff [edgeType] += (orgLine[x] - srcLine[x]);
          count[edgeType] ++;

          signDownLine[x + 1] = -signDown;
        }
        signDownLine[startX] = (int8_t) sgn(srcLineBelow[startX] - srcLine[startX - 1]);

        signTmpLine  = signUpLine;
        signUpLine   = signDownLine;
        signDownLine = signTmpLine;

        srcLine += srcStride;
        orgLine += orgStride;
      }
      if (isCalculatePreDeblockSamples)
      {
        if (isBelowAvail)
        {
          startX = isLeftAvail ? 0 : 1;
          endX   = isRightAvail ? width : (width - 1);

          for (y = 0; y < skipLinesB[typeIdx]; y++)
          {
            srcLineBelow = srcLine + srcStride;
            srcLineAbove = srcLine - srcStride;

            for (x = startX; x < endX; x++)
            {
              if (isCtuCrossedByVirtualBoundaries
                  && isProcessDisabled(x, y + endY, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
              {
                continue;
              }
              edgeType = sgn(srcLine[x] - srcLineBelow[x + 1]) + sgn(srcLine[x] - srcLineAbove[x - 1]);
              diff[edgeType] += (orgLine[x] - srcLine[x]);
              count[edgeType]++;
            }
            srcLine += srcStride;
            orgLine += orgStride;
          }
        }
      }
      break;
    }
    case SAOModeNewTypes::EO_45:
    {
      diff += 2;
      count += 2;
      int8_t *signUpLine = m_signLineBuf1.data();

      startX = (!isCalculatePreDeblockSamples) ? (isLeftAvail ? 0 : 1)
                                               : (isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1));
      endX   = (!isCalculatePreDeblockSamples) ? (isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1))
                                               : (isRightAvail ? width : (width - 1));
      endY   = isBelowAvail ? (height - skipLinesB[typeIdx]) : (height - 1);

      // prepare 2nd line upper sign
      Pel *srcLineBelow = srcLine + srcStride;
      for (x = startX - 1; x < endX; x++)
      {
        signUpLine[x + 1] = (int8_t) sgn(srcLineBelow[x] - srcLine[x + 1]);
      }

      // first line
      Pel *srcLineAbove = srcLine - srcStride;

      firstLineStartX = !isCalculatePreDeblockSamples ? (isAboveAvail ? startX : endX) : startX;
      firstLineEndX   = !isCalculatePreDeblockSamples ? (!isRightAvail && isAboveRightAvail ? width : endX) : endX;

      for (x = firstLineStartX; x < firstLineEndX; x++)
      {
        if (isCtuCrossedByVirtualBoundaries
            && isProcessDisabled(x, 0, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
        {
          continue;
        }
        edgeType = sgn(srcLine[x] - srcLineAbove[x + 1]) - signUpLine[x];
        diff[edgeType] += (orgLine[x] - srcLine[x]);
        count[edgeType]++;
      }

      srcLine += srcStride;
      orgLine += orgStride;

      // middle lines
      for (y = 1; y < endY; y++)
      {
        srcLineBelow = srcLine + srcStride;

        for (x = startX; x < endX; x++)
        {
          signDown = (int8_t) sgn(srcLine[x] - srcLineBelow[x - 1]);
          if (isCtuCrossedByVirtualBoundaries
              && isProcessDisabled(x, y, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
          {
            signUpLine[x] = -signDown;
            continue;
          }
          edgeType = signDown + signUpLine[x + 1];

          diff [edgeType] += (orgLine[x] - srcLine[x]);
          count[edgeType]++;

          signUpLine[x] = -signDown;
        }
        signUpLine[endX] = (int8_t) sgn(srcLineBelow[endX - 1] - srcLine[endX]);
        srcLine += srcStride;
        orgLine += orgStride;
      }
      if (isCalculatePreDeblockSamples)
      {
        if (isBelowAvail)
        {
          startX = isLeftAvail ? 0 : 1;
          endX   = isRightAvail ? width : (width - 1);

          for (y = 0; y < skipLinesB[typeIdx]; y++)
          {
            srcLineBelow = srcLine + srcStride;
            srcLineAbove = srcLine - srcStride;

            for (x = startX; x < endX; x++)
            {
              if (isCtuCrossedByVirtualBoundaries
                  && isProcessDisabled(x, y + endY, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
              {
                continue;
              }
              edgeType = sgn(srcLine[x] - srcLineBelow[x - 1]) + sgn(srcLine[x] - srcLineAbove[x + 1]);
              diff[edgeType] += (orgLine[x] - srcLine[x]);
              count[edgeType]++;
            }
            srcLine += srcStride;
            orgLine += orgStride;
          }
        }
      }
      break;
    }
    case SAOModeNewTypes::BO:
    {
      startX = !isCalculatePreDeblockSamples ? 0 : (isRightAvail ? (width - skipLinesR[typeIdx]) : width);
      endX   = !isCalculatePreDeblockSamples ? (isRightAvail ? (width - skipLinesR[typeIdx]) : width) : width;
      endY   = isBelowAvail ? (height - skipLinesB[typeIdx]) : height;

      const int shiftBits = channelBitDepth - NUM_SAO_BO_CLASSES_LOG2;
      for (y = 0; y < endY; y++)
      {
        for (x = startX; x < endX; x++)
        {
          const int bandIdx = srcLine[x] >> shiftBits;
          diff[bandIdx] += (orgLine[x] - srcLine[x]);
          count[bandIdx]++;
        }
        srcLine += srcStride;
        orgLine += orgStride;
      }
      if (isCalculatePreDeblockSamples)
      {
        if (isBelowAvail)
        {
          startX = 0;
          endX   = width;

          for (y = 0; y < skipLinesB[typeIdx]; y++)
          {
            for (x = startX; x < endX; x++)
            {
              const int bandIdx = srcLine[x] >> shiftBits;
              diff[bandIdx] += (orgLine[x] - srcLine[x]);
              count[bandIdx]++;
            }
            srcLine += srcStride;
            orgLine += orgStride;
          }
        }
      }
      break;
    }
    default:
      THROW("Not a supported SAO type");
      break;
    }
  }
}

void EncSampleAdaptiveOffset::deriveLoopFilterBoundaryAvailability(CodingStructure &cs, const Position &pos,
                                                                   bool &isLeftAvail, bool &isAboveAvail,
                                                                   bool &isAboveLeftAvail) const
{
  bool isLoopFiltAcrossSlicePPS = cs.pps->getLoopFilterAcrossSlicesEnabledFlag();
  bool isLoopFiltAcrossTilePPS = cs.pps->getLoopFilterAcrossTilesEnabledFlag();

  const int width = cs.pcv->maxCUWidth;
  const int height = cs.pcv->maxCUHeight;
  const CodingUnit *cuCurr      = cs.getCU(pos, ChannelType::LUMA);
  const CodingUnit *cuLeft      = cs.getCU(pos.offset(-width, 0), ChannelType::LUMA);
  const CodingUnit *cuAbove     = cs.getCU(pos.offset(0, -height), ChannelType::LUMA);
  const CodingUnit *cuAboveLeft = cs.getCU(pos.offset(-width, -height), ChannelType::LUMA);

  if (!isLoopFiltAcrossSlicePPS)
  {
    isLeftAvail      = (cuLeft      == nullptr) ? false : CU::isSameSlice(*cuCurr, *cuLeft);
    isAboveAvail     = (cuAbove     == nullptr) ? false : CU::isSameSlice(*cuCurr, *cuAbove);
    isAboveLeftAvail = (cuAboveLeft == nullptr) ? false : CU::isSameSlice(*cuCurr, *cuAboveLeft);
  }
  else
  {
    isLeftAvail      = (cuLeft      != nullptr);
    isAboveAvail     = (cuAbove     != nullptr);
    isAboveLeftAvail = (cuAboveLeft != nullptr);
  }

  if (!isLoopFiltAcrossTilePPS)
  {
    isLeftAvail      = (!isLeftAvail)      ? false : CU::isSameTile(*cuCurr, *cuLeft);
    isAboveAvail     = (!isAboveAvail)     ? false : CU::isSameTile(*cuCurr, *cuAbove);
    isAboveLeftAvail = (!isAboveLeftAvail) ? false : CU::isSameTile(*cuCurr, *cuAboveLeft);
  }

  const SubPic& curSubPic = cs.pps->getSubPicFromCU(*cuCurr);
  if (!curSubPic.getloopFilterAcrossEnabledFlag())
  {
    isLeftAvail      = (!isLeftAvail)      ? false : CU::isSameSubPic(*cuCurr, *cuLeft);
    isAboveAvail     = (!isAboveAvail)     ? false : CU::isSameSubPic(*cuCurr, *cuAbove);
    isAboveLeftAvail = (!isAboveLeftAvail) ? false : CU::isSameSubPic(*cuCurr, *cuAboveLeft);
  }
}

//! \}
