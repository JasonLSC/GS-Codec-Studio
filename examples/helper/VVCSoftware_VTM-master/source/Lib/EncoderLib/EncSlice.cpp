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

/** \file     EncSlice.cpp
    \brief    slice encoder class
*/

#include "EncSlice.h"

#include "EncLib.h"
#include "CommonLib/UnitTools.h"
#include "CommonLib/Picture.h"
#if K0149_BLOCK_STATISTICS
#include "CommonLib/dtrace_blockstatistics.h"
#endif


#include <math.h>

//! \ingroup EncoderLib
//! \{

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

EncSlice::EncSlice()
 : m_encCABACTableIdx(I_SLICE)
#if ENABLE_QPA
 , m_adaptedLumaQP(-1)
#endif
{
}

EncSlice::~EncSlice()
{
  destroy();
}

void EncSlice::create(int width, int height, ChromaFormat chromaFormat, uint32_t iMaxCUWidth, uint32_t iMaxCUHeight,
                      uint8_t uhTotalDepth)
{
}

void EncSlice::destroy()
{
  // free lambda and QP arrays
  m_vdRdPicLambda.clear();
  m_vdRdPicQp.clear();
  m_viRdPicQp.clear();

  if (m_pcCfg->getDPF())
  {
    m_lambdaWeight.clear();
    if (m_pixelRecDis)
    {
      for (int i = 0; i < m_maxPicHeight; i++)
      {
        delete[] m_pixelRecDis[i];
        m_pixelRecDis[i] = nullptr;
      }
      delete[] m_pixelRecDis;
      m_pixelRecDis = nullptr;
    }
    if (m_pixelPredErr)
    {
      for (int i = 0; i < m_maxPicHeight; i++)
      {
        delete[] m_pixelPredErr[i];
        m_pixelPredErr[i] = nullptr;
      }
      delete[] m_pixelPredErr;
      m_pixelPredErr = nullptr;
    }
  }
}

void EncSlice::init( EncLib* pcEncLib, const SPS& sps )
{
  m_pcCfg             = pcEncLib;
  m_pcLib             = pcEncLib;
  m_pcListPic         = pcEncLib->getListPic();

  m_pcGOPEncoder      = pcEncLib->getGOPEncoder();
  m_pcCuEncoder       = pcEncLib->getCuEncoder();
  m_pcInterSearch     = pcEncLib->getInterSearch();
  m_CABACWriter       = pcEncLib->getCABACEncoder()->getCABACWriter   (&sps);
  m_CABACEstimator    = pcEncLib->getCABACEncoder()->getCABACEstimator(&sps);
  m_pcTrQuant         = pcEncLib->getTrQuant();
  m_pcRdCost          = pcEncLib->getRdCost();

  // create lambda and QP arrays
  m_vdRdPicLambda.resize(m_pcCfg->getDeltaQpRD() * 2 + 1 );
  m_vdRdPicQp.resize(    m_pcCfg->getDeltaQpRD() * 2 + 1 );
  m_viRdPicQp.resize(    m_pcCfg->getDeltaQpRD() * 2 + 1 );
  m_pcRateCtrl        = pcEncLib->getRateCtrl();

  if (m_pcCfg->getDPF())
  {
    m_maxPicHeight = sps.getMaxPicHeightInLumaSamples();
    m_maxPicWidth = sps.getMaxPicWidthInLumaSamples();
    m_pixelPredErr = new int*[m_maxPicHeight];
    m_pixelRecDis = new int*[m_maxPicHeight];
    for (int i = 0; i < m_maxPicHeight; i++)
    {
      m_pixelPredErr[i] = new int[m_maxPicWidth];
      m_pixelRecDis[i] = new int[m_maxPicWidth];
    }
  }
}

void EncSlice::setUpLambda(Slice *slice, const double dLambda, int qp)
{
  m_pcRdCost->resetStore();
  m_pcTrQuant->resetStore();
  // store lambda
  m_pcRdCost ->setLambda( dLambda, slice->getSPS()->getBitDepths() );

  // for RDO
  // in RdCost there is only one lambda because the luma and chroma bits are not separated, instead we weight the distortion of chroma.
  double dLambdas[MAX_NUM_COMPONENT] = { dLambda };
  for( uint32_t compIdx = 1; compIdx < MAX_NUM_COMPONENT; compIdx++ )
  {
    const ComponentID compID = ComponentID( compIdx );
    int chromaQPOffset       = slice->getPPS()->getQpOffset( compID ) + slice->getSliceChromaQpDelta( compID );
    int               qpc                  = slice->getSPS()->getMappedChromaQpValue(compID, qp) + chromaQPOffset;
    double tmpWeight = pow(2.0, (qp - qpc) / 3.0);   // takes into account of the chroma qp mapping and chroma qp Offset
    if (slice->getDepQuantEnabledFlag())
    {
      tmpWeight *= ( m_pcCfg->getGOPSize() >= 8 ? pow( 2.0, 0.1/3.0 ) : pow( 2.0, 0.2/3.0 ) );  // increase chroma weight for dependent quantization (in order to reduce bit rate shift from chroma to luma)
    }
    m_pcRdCost->setDistortionWeight( compID, tmpWeight );
    dLambdas[compIdx] = dLambda / tmpWeight;
  }

#if RDOQ_CHROMA_LAMBDA
  // for RDOQ
  m_pcTrQuant->setLambdas( dLambdas );
#else
  m_pcTrQuant->setLambda( dLambda );
#endif

  // for SAO
  slice->setLambdas( dLambdas );
}

#if ENABLE_QPA

static inline int apprI3Log2 (const double d) // rounded 3*log2(d)
{
  return d < 1.5e-13 ? -128 : int (floor (3.0 * log (d) / log (2.0) + 0.5));
}

static inline int lumaDQPOffset (const uint32_t avgLumaValue, const int bitDepth)
{
  return (1 - int ((3 * uint64_t (avgLumaValue * avgLumaValue)) >> uint64_t (2 * bitDepth - 1)));
}

static void filterAndCalculateAverageEnergies(const Pel *pSrc, const ptrdiff_t srcStride, double &hpEner,
                                              const int height, const int width,
                                              const uint32_t bitDepth /* luma bit-depth (4-16) */)
{
  uint64_t saAct = 0;

  // skip first row as there may be a black border frame
  pSrc += srcStride;
  // center rows
  for (int y = 1; y < height - 1; y++)
  {
    // skip column as there may be a black border frame

    for (int x = 1; x < width - 1; x++)   // and columns
    {
      const int f = 12 * pSrc[x] - 2 * (pSrc[x - 1] + pSrc[x + 1] + pSrc[x - srcStride] + pSrc[x + srcStride])
                    - pSrc[x - 1 - srcStride] - pSrc[x + 1 - srcStride] - pSrc[x - 1 + srcStride]
                    - pSrc[x + 1 + srcStride];
      saAct += abs (f);
    }
    // skip column as there may be a black border frame
    pSrc += srcStride;
  }
  // skip last row as there may be a black border frame

  hpEner = double(saAct) / double((width - 2) * (height - 2));

  // lower limit, compensate for highpass amplification
  if (hpEner < double(1 << (bitDepth - 4)))
  {
    hpEner = double(1 << (bitDepth - 4));
  }
}

#ifndef GLOBAL_AVERAGING
  #define GLOBAL_AVERAGING 1 // "global" averaging of a_k across a set instead of one picture
#endif

#if GLOBAL_AVERAGING
static double getAveragePictureEnergy(const CPelBuf picOrig, const uint32_t bitDepth)
{
  const double hpEnerPic =
    16.0 * sqrt((3840.0 * 2160.0) / double(picOrig.width * picOrig.height)) * double(1 << (2 * bitDepth - 10));

  return sqrt (hpEnerPic); // square-root of a_pic value
}
#endif

static int getGlaringColorQPOffset (Picture* const pcPic, const int ctuAddr, Slice* const pcSlice,
                                    const int bitDepth,   uint32_t &avgLumaValue)
{
  const PreCalcValues& pcv  = *pcPic->cs->pcv;
  const ChromaFormat chrFmt = pcPic->chromaFormat;
  const uint32_t       chrWidth   = pcv.maxCUWidth >> getChannelTypeScaleX(ChannelType::CHROMA, chrFmt);
  const uint32_t       chrHeight  = pcv.maxCUHeight >> getChannelTypeScaleY(ChannelType::CHROMA, chrFmt);
  const int      midLevel   = 1 << (bitDepth - 1);
  int chrValue = MAX_INT;
  avgLumaValue = (pcSlice != nullptr) ? 0 : (uint32_t)pcPic->getOrigBuf().Y().computeAvg();

  if (ctuAddr >= 0) // luma
  {
    avgLumaValue = (uint32_t)pcPic->m_iOffsetCtu[ctuAddr];
  }
  else if (pcSlice != nullptr)
  {
    for (uint32_t ctuIdx = 0; ctuIdx < pcSlice->getNumCtuInSlice(); ctuIdx++)
    {
      uint32_t ctuRsAddr = pcSlice->getCtuAddrInSlice( ctuIdx );
      avgLumaValue += pcPic->m_iOffsetCtu[ctuRsAddr];
    }
    avgLumaValue = (avgLumaValue + (pcSlice->getNumCtuInSlice() >> 1)) / pcSlice->getNumCtuInSlice();
  }

  for (uint32_t comp = COMPONENT_Cb; comp < MAX_NUM_COMPONENT; comp++)
  {
    const ComponentID compID = (ComponentID)comp;
    int avgCompValue;

    if (ctuAddr >= 0) // chroma
    {
      const CompArea chrArea = clipArea (CompArea (compID, chrFmt, Area ((ctuAddr % pcv.widthInCtus) * chrWidth, (ctuAddr / pcv.widthInCtus) * chrHeight, chrWidth, chrHeight)), pcPic->block (compID));

      avgCompValue = pcPic->getOrigBuf (chrArea).computeAvg();
    }
    else
    {
      avgCompValue = pcPic->getOrigBuf(pcPic->block(compID)).computeAvg();
    }

    if (chrValue > avgCompValue)
    {
      chrValue = avgCompValue;   // minimum of the DC offsets
    }
  }
  CHECK (chrValue < 0, "DC offset cannot be negative!");

  chrValue = (int)avgLumaValue - chrValue;

  if (chrValue > midLevel)
  {
    return apprI3Log2(double(chrValue * chrValue) / double(midLevel * midLevel));
  }

  return 0;
}

static int applyQPAdaptationChroma (Picture* const pcPic, Slice* const pcSlice, EncCfg* const pcEncCfg, const int sliceQP)
{
  const int bitDepth                  = pcSlice->getSPS()->getBitDepth(ChannelType::LUMA);   // overall image bit-depth
  double hpEner[MAX_NUM_COMPONENT] = {0.0, 0.0, 0.0};
  int    optSliceChromaQpOffset[2] = {0, 0};
  int    savedLumaQP               = -1;
  uint32_t meanLuma                = MAX_UINT;

  for (uint32_t comp = 0; comp < getNumberValidComponents (pcPic->chromaFormat); comp++)
  {
    const ComponentID compID = (ComponentID)comp;
    const CPelBuf    picOrig = pcPic->getOrigBuf (pcPic->block (compID));

    filterAndCalculateAverageEnergies (picOrig.buf,    picOrig.stride, hpEner[comp],
                                       picOrig.height, picOrig.width,  bitDepth - (isChroma (compID) ? 1 : 0));
    if (isChroma (compID))
    {
      const int  adaptChromaQPOffset = 2.0 * hpEner[comp] <= hpEner[0] ? 0 : apprI3Log2 (2.0 * hpEner[comp] / hpEner[0]);

      if (savedLumaQP < 0)
      {
#if GLOBAL_AVERAGING
        int     averageAdaptedLumaQP = Clip3 (0, MAX_QP, sliceQP + apprI3Log2 (hpEner[0] / getAveragePictureEnergy (pcPic->getOrigBuf().Y(), bitDepth)));
#else
        int     averageAdaptedLumaQP = Clip3 (0, MAX_QP, sliceQP); // mean slice QP
#endif

        averageAdaptedLumaQP += getGlaringColorQPOffset (pcPic, -1 /*ctuRsAddr*/, nullptr /*pcSlice*/, bitDepth, meanLuma);

        if (averageAdaptedLumaQP > MAX_QP
#if SHARP_LUMA_DELTA_QP
            && (pcEncCfg->getLumaLevelToDeltaQPMapping().mode != LUMALVL_TO_DQP_NUM_MODES)
#endif
            ) averageAdaptedLumaQP = MAX_QP;
#if SHARP_LUMA_DELTA_QP

        // change mean picture QP index based on picture's average luma value (Sharp)
        if (pcEncCfg->getLumaLevelToDeltaQPMapping().mode == LUMALVL_TO_DQP_NUM_MODES)
        {
          if (meanLuma == MAX_UINT)
          {
            meanLuma = pcPic->getOrigBuf().Y().computeAvg();
          }

          averageAdaptedLumaQP = Clip3 (0, MAX_QP, averageAdaptedLumaQP + lumaDQPOffset (meanLuma, bitDepth));
        }
#endif

        savedLumaQP = averageAdaptedLumaQP;
      } // savedLumaQP < 0

      const int lumaChromaMappingDQP = savedLumaQP - pcSlice->getSPS()->getMappedChromaQpValue(compID, savedLumaQP);

      optSliceChromaQpOffset[comp-1] = std::min (3 + lumaChromaMappingDQP, adaptChromaQPOffset + lumaChromaMappingDQP);
    }
  }

  pcEncCfg->setSliceChromaOffsetQpIntraOrPeriodic (pcEncCfg->getSliceChromaOffsetQpPeriodicity(), optSliceChromaQpOffset);

  return savedLumaQP;
}

#endif // ENABLE_QPA

/**
 - non-referenced frame marking
 - QP computation based on temporal structure
 - lambda computation based on QP
 - set temporal layer ID and the parameter sets
 .
 \param pcPic         picture class
 \param pocLast       POC of last picture
 \param pocCurr       current POC
 \param numPicRcvd   number of received pictures
 \param gopId        POC offset for hierarchical structure
 \param rpcSlice      slice header class
 \param isField       true for field coding
 */
void EncSlice::initEncSlice(Picture *pcPic, const int pocLast, const int pocCurr, const int gopId, Slice *&rpcSlice,
                            const bool isField, bool isEncodeLtRef, int layerId, NalUnitType nalType)
{
  double dQP;
  double dLambda;
  PicHeader *picHeader = pcPic->cs->picHeader;
  pcPic->cs->resetPrevPLT(pcPic->cs->prevPLT);

  rpcSlice = pcPic->slices[0];
  rpcSlice->setSliceBits(0);
  rpcSlice->setPic( pcPic );
  rpcSlice->setPicHeader( picHeader );
  rpcSlice->initSlice();
  rpcSlice->setNalUnitLayerId(layerId);

  int multipleFactor = m_pcCfg->getUseCompositeRef() ? 2 : 1;
  if (m_pcCfg->getUseCompositeRef() && isEncodeLtRef)
  {
    picHeader->setPicOutputFlag(false);
  }
  else
  {
    picHeader->setPicOutputFlag(true);
  }
  rpcSlice->setPOC( pocCurr );

  if( m_pcCfg->getCostMode() != COST_LOSSLESS_CODING )
  {
    rpcSlice->setDepQuantEnabledFlag( m_pcCfg->getDepQuantEnabledFlag() );
    rpcSlice->setSignDataHidingEnabledFlag( m_pcCfg->getSignDataHidingEnabledFlag() );
    rpcSlice->setTSResidualCodingDisabledFlag( false );

    CHECK( (m_pcCfg->getDepQuantEnabledFlag() || m_pcCfg->getSignDataHidingEnabledFlag() )
           && rpcSlice->getTSResidualCodingDisabledFlag() , "TSRC cannot be bypassed if either DQ or SDH are enabled at slice level.");
  }
  else
  {
    rpcSlice->setDepQuantEnabledFlag( false ); //should be disabled for lossless
    rpcSlice->setSignDataHidingEnabledFlag( false ); //should be disabled for lossless
    if( m_pcCfg->getTSRCdisableLL() )
    {
      rpcSlice->setTSResidualCodingDisabledFlag( true );
    }
  }

#if SHARP_LUMA_DELTA_QP
  pcPic->fieldPic = isField;
  m_gopID         = gopId;
#endif

  // depth computation based on GOP size
  int hierPredLayerIdx;
  {
    int poc = rpcSlice->getPOC();
    if(isField)
    {
      poc = (poc/2) % (m_pcCfg->getGOPSize()/2);
    }
    else
    {
      poc = poc % (m_pcCfg->getGOPSize() * multipleFactor);
    }

    if ( poc == 0 )
    {
      hierPredLayerIdx = 0;
    }
    else
    {
      int step = m_pcCfg->getGOPSize() * multipleFactor;
      hierPredLayerIdx = 0;
      for( int i=step>>1; i>=1; i>>=1 )
      {
        for (int j = i; j<(m_pcCfg->getGOPSize() * multipleFactor); j += step)
        {
          if ( j == poc )
          {
            i=0;
            break;
          }
        }
        step >>= 1;
        hierPredLayerIdx++;
      }
    }

    if(m_pcCfg->getHarmonizeGopFirstFieldCoupleEnabled() && poc != 0)
    {
      if (isField && ((rpcSlice->getPOC() % 2) == 1))
      {
        hierPredLayerIdx++;
      }
    }
  }

  // slice type
  SliceType eSliceType;

  eSliceType=B_SLICE;
  const bool useIlRef = m_pcCfg->getAvoidIntraInDepLayer() && rpcSlice->getPic()->cs->vps && m_pcCfg->getNumRefLayers(rpcSlice->getPic()->cs->vps->getGeneralLayerIdx(layerId));
  if (m_pcCfg->getIntraPeriod() > 0 )
  {
    if(!(isField && pocLast == 1) || !m_pcCfg->getEfficientFieldIRAPEnabled())
    {
      if(m_pcCfg->getDecodingRefreshType() == 3)
      {
        eSliceType = (pocLast == 0 || pocCurr % (m_pcCfg->getIntraPeriod() * multipleFactor) == 0 || m_pcGOPEncoder->getGOPSize() == 0) && (!useIlRef) ? I_SLICE : eSliceType;
#if GDR_ENABLED
        if (m_pcCfg->getGdrEnabled() && (pocCurr >= m_pcCfg->getGdrPocStart()) && ((pocCurr - m_pcCfg->getGdrPocStart()) % m_pcCfg->getGdrPeriod() == 0))
        {
          eSliceType = B_SLICE;
        }
#endif
      }
      else
      {
        eSliceType = (pocLast == 0 || (pocCurr - (isField ? 1 : 0)) % (m_pcCfg->getIntraPeriod() * multipleFactor) == 0 || m_pcGOPEncoder->getGOPSize() == 0) && (!useIlRef) ? I_SLICE : eSliceType;
#if GDR_ENABLED
        if (m_pcCfg->getGdrEnabled() && (pocCurr >= m_pcCfg->getGdrPocStart()) && ((pocCurr - m_pcCfg->getGdrPocStart()) % m_pcCfg->getGdrPeriod() == 0))
        {
          eSliceType = B_SLICE;
        }
#endif
      }
    }
  }
  else
  {
    eSliceType = (pocLast == 0 || pocCurr == 0 || m_pcGOPEncoder->getGOPSize() == 0) && (!useIlRef) ? I_SLICE : eSliceType;
  }

  rpcSlice->setHierPredLayerIdx(hierPredLayerIdx);
  rpcSlice->setSliceType    ( eSliceType );

  // ------------------------------------------------------------------------------------------------------------------
  // Non-referenced frame marking
  // ------------------------------------------------------------------------------------------------------------------

  pcPic->referenced = true;

  // ------------------------------------------------------------------------------------------------------------------
  // QP setting
  // ------------------------------------------------------------------------------------------------------------------

  rpcSlice->setNalUnitType(nalType);
  dQP = m_pcCfg->getQPForPicture(gopId, rpcSlice);

  // ------------------------------------------------------------------------------------------------------------------
  // Lambda computation
  // ------------------------------------------------------------------------------------------------------------------

  const int temporalId = m_pcCfg->getGOPEntry(gopId).m_temporalId;
#if !SHARP_LUMA_DELTA_QP
  const std::vector<double> &intraLambdaModifiers=m_pcCfg->getIntraLambdaModifier();
#endif
  int    qp;
  double dOrigQP = dQP;

  // pre-compute lambda and QP values for all possible QP candidates
  for ( int iDQpIdx = 0; iDQpIdx < 2 * m_pcCfg->getDeltaQpRD() + 1; iDQpIdx++ )
  {
    // compute QP value
    dQP = dOrigQP + ((iDQpIdx+1)>>1)*(iDQpIdx%2 ? -1 : 1);
    // compute lambda value
#if SHARP_LUMA_DELTA_QP
    dLambda = calculateLambda(rpcSlice, gopId, dQP, dQP, qp);
#else
    dLambda = initializeLambda(rpcSlice, gopId, int(dQP + 0.5), dQP);
    qp      = Clip3(-rpcSlice->getSPS()->getQpBDOffset(ChannelType::LUMA), MAX_QP, int(dQP + 0.5));
#endif

    m_vdRdPicLambda[iDQpIdx] = dLambda;
    m_vdRdPicQp    [iDQpIdx] = dQP;
    m_viRdPicQp[iDQpIdx]     = qp;
  }

  // obtain dQP = 0 case
  dLambda = m_vdRdPicLambda[0];
  dQP     = m_vdRdPicQp    [0];
  qp      = m_viRdPicQp[0];


#if W0038_CQP_ADJ
 #if ENABLE_QPA
  m_adaptedLumaQP = -1;

  if ((m_pcCfg->getUsePerceptQPA() || m_pcCfg->getSliceChromaOffsetQpPeriodicity() > 0) && !m_pcCfg->getUseRateCtrl() && rpcSlice->getPPS()->getSliceChromaQpFlag() &&
      (rpcSlice->isIntra() || (m_pcCfg->getSliceChromaOffsetQpPeriodicity() > 0 && (rpcSlice->getPOC() % m_pcCfg->getSliceChromaOffsetQpPeriodicity()) == 0)))
  {
    m_adaptedLumaQP = applyQPAdaptationChroma(pcPic, rpcSlice, m_pcCfg, qp);
  }
 #endif
  if(rpcSlice->getPPS()->getSliceChromaQpFlag())
  {
    const bool bUseIntraOrPeriodicOffset = (rpcSlice->isIntra() && !rpcSlice->getSPS()->getIBCFlag()) || (m_pcCfg->getSliceChromaOffsetQpPeriodicity() > 0 && (rpcSlice->getPOC() % m_pcCfg->getSliceChromaOffsetQpPeriodicity()) == 0);
    int        cbQP   = bUseIntraOrPeriodicOffset ? m_pcCfg->getSliceChromaOffsetQpIntraOrPeriodic(false)
                                                  : m_pcCfg->getGOPEntry(gopId).m_CbQPoffset;
    int        crQP   = bUseIntraOrPeriodicOffset ? m_pcCfg->getSliceChromaOffsetQpIntraOrPeriodic(true)
                                                  : m_pcCfg->getGOPEntry(gopId).m_CrQPoffset;
    // adjust chroma QP such that it corresponds to the luma QP change when encoding in reduced resolution
    if (m_pcCfg->getGOPBasedRPREnabledFlag() || m_pcCfg->getRprFunctionalityTestingEnabledFlag())
    {
      auto mappedQpDelta = [&](ComponentID c, int qpOffset) -> int {
        int lumaQpBefore = Clip3(-rpcSlice->getSPS()->getQpBDOffset(ChannelType::LUMA), MAX_QP, qp - qpOffset);
        const int mappedQpBefore = rpcSlice->getSPS()->getMappedChromaQpValue(c, lumaQpBefore);
        const int mappedQpAfter = rpcSlice->getSPS()->getMappedChromaQpValue(c, qp);
        return mappedQpBefore - mappedQpAfter + qpOffset;
      };
      if (m_pcCfg->getRprFunctionalityTestingEnabledFlag())
      {
        int currPoc = rpcSlice->getPOC() + m_pcCfg->getFrameSkip();
        int rprSegment = m_pcCfg->getRprSwitchingSegment(currPoc);
        cbQP += mappedQpDelta(COMPONENT_Cb, m_pcCfg->getRprSwitchingQPOffsetOrderList(rprSegment));
        crQP += mappedQpDelta(COMPONENT_Cr, m_pcCfg->getRprSwitchingQPOffsetOrderList(rprSegment));
      }
      else
      {
        if (rpcSlice->getPPS()->getPPSId() == (ENC_PPS_ID_RPR + rpcSlice->getNalUnitLayerId())) // ScalingRatioHor/ScalingRatioVer
        {
          cbQP += mappedQpDelta(COMPONENT_Cb, m_pcCfg->getQpOffsetChromaRPR());
          crQP += mappedQpDelta(COMPONENT_Cr, m_pcCfg->getQpOffsetChromaRPR());
        }
        if (rpcSlice->getPPS()->getPPSId() == (ENC_PPS_ID_RPR2 + rpcSlice->getNalUnitLayerId())) // ScalingRatioHor/ScalingRatioVer
        {
          cbQP += mappedQpDelta(COMPONENT_Cb, m_pcCfg->getQpOffsetChromaRPR2());
          crQP += mappedQpDelta(COMPONENT_Cr, m_pcCfg->getQpOffsetChromaRPR2());
        }
        if (rpcSlice->getPPS()->getPPSId() == (ENC_PPS_ID_RPR3 + rpcSlice->getNalUnitLayerId())) // ScalingRatioHor/ScalingRatioVer
        {
          cbQP += mappedQpDelta(COMPONENT_Cb, m_pcCfg->getQpOffsetChromaRPR3());
          crQP += mappedQpDelta(COMPONENT_Cr, m_pcCfg->getQpOffsetChromaRPR3());
        }
      }
    }
    int cbCrQP = (cbQP + crQP) >> 1; // use floor of average chroma QP offset for joint-Cb/Cr coding

    cbQP = Clip3( -12, 12, cbQP + rpcSlice->getPPS()->getQpOffset(COMPONENT_Cb) ) - rpcSlice->getPPS()->getQpOffset(COMPONENT_Cb);
    crQP = Clip3( -12, 12, crQP + rpcSlice->getPPS()->getQpOffset(COMPONENT_Cr) ) - rpcSlice->getPPS()->getQpOffset(COMPONENT_Cr);
    rpcSlice->setSliceChromaQpDelta(COMPONENT_Cb, Clip3( -12, 12, cbQP));
    CHECK(!(rpcSlice->getSliceChromaQpDelta(COMPONENT_Cb)+rpcSlice->getPPS()->getQpOffset(COMPONENT_Cb)<=12 && rpcSlice->getSliceChromaQpDelta(COMPONENT_Cb)+rpcSlice->getPPS()->getQpOffset(COMPONENT_Cb)>=-12), "Unspecified error");
    rpcSlice->setSliceChromaQpDelta(COMPONENT_Cr, Clip3( -12, 12, crQP));
    CHECK(!(rpcSlice->getSliceChromaQpDelta(COMPONENT_Cr)+rpcSlice->getPPS()->getQpOffset(COMPONENT_Cr)<=12 && rpcSlice->getSliceChromaQpDelta(COMPONENT_Cr)+rpcSlice->getPPS()->getQpOffset(COMPONENT_Cr)>=-12), "Unspecified error");
    if (rpcSlice->getSPS()->getJointCbCrEnabledFlag())
    {
      cbCrQP = Clip3(-12, 12, cbCrQP + rpcSlice->getPPS()->getQpOffset(JOINT_CbCr)) - rpcSlice->getPPS()->getQpOffset(JOINT_CbCr);
      rpcSlice->setSliceChromaQpDelta(JOINT_CbCr, Clip3( -12, 12, cbCrQP ));
    }
  }
  else
  {
    rpcSlice->setSliceChromaQpDelta( COMPONENT_Cb, 0 );
    rpcSlice->setSliceChromaQpDelta( COMPONENT_Cr, 0 );
    rpcSlice->setSliceChromaQpDelta( JOINT_CbCr, 0 );
  }
#endif


#if RDOQ_CHROMA_LAMBDA
  m_pcRdCost->setDistortionWeight (COMPONENT_Y, 1.0); // no chroma weighting for luma
#endif
  setUpLambda(rpcSlice, dLambda, qp);
  if (m_pcCfg->getDPF())
  {
    m_lambda = dLambda;
    m_qpCtu = qp;
  }

#if WCG_EXT
  // cost = Distortion + Lambda*R,
  // when QP is adjusted by luma, distortion is changed, so we have to adjust lambda to match the distortion, then the cost function becomes
  // costA = Distortion + AdjustedLambda * R          -- currently, costA is still used when calculating intermediate cost of using SAD, HAD, resisual etc.
  // an alternative way is to weight the distortion to before the luma QP adjustment, then the cost function becomes
  // costB = weightedDistortion + Lambda * R          -- currently, costB is used to calculat final cost, and when DF_FUNC is DF_DEFAULT
  m_pcRdCost->saveUnadjustedLambda();
#endif

  if (m_pcCfg->getFastMEForGenBLowDelayEnabled())
  {
    // restore original slice type

    if (m_pcCfg->getIntraPeriod() > 0 )
    {
      if(!(isField && pocLast == 1) || !m_pcCfg->getEfficientFieldIRAPEnabled())
      {
        if(m_pcCfg->getDecodingRefreshType() == 3)
        {
          eSliceType = (pocLast == 0 || pocCurr % (m_pcCfg->getIntraPeriod() * multipleFactor) == 0 || m_pcGOPEncoder->getGOPSize() == 0) && (!useIlRef) ? I_SLICE : eSliceType;
#if GDR_ENABLED
          if (m_pcCfg->getGdrEnabled() && (pocCurr >= m_pcCfg->getGdrPocStart()) && ((pocCurr - m_pcCfg->getGdrPocStart()) % m_pcCfg->getGdrPeriod() == 0))
          {
            eSliceType = B_SLICE;
          }
#endif
        }
        else
        {
          eSliceType = (pocLast == 0 || (pocCurr - (isField ? 1 : 0)) % (m_pcCfg->getIntraPeriod() * multipleFactor) == 0 || m_pcGOPEncoder->getGOPSize() == 0) && (!useIlRef) ? I_SLICE : eSliceType;
#if GDR_ENABLED
          if (m_pcCfg->getGdrEnabled() && (pocCurr >= m_pcCfg->getGdrPocStart()) && ((pocCurr - m_pcCfg->getGdrPocStart()) % m_pcCfg->getGdrPeriod() == 0))
          {
            eSliceType = B_SLICE;
          }
#endif
        }
      }
    }
    else
    {
      eSliceType = (pocLast == 0 || pocCurr == 0 || m_pcGOPEncoder->getGOPSize() == 0) && (!useIlRef) ? I_SLICE : eSliceType;
    }

    rpcSlice->setSliceType        ( eSliceType );
  }

  if (m_pcCfg->getUseRecalculateQPAccordingToLambda())
  {
    dQP = xGetQPValueAccordingToLambda( dLambda );
    qp  = Clip3(-rpcSlice->getSPS()->getQpBDOffset(ChannelType::LUMA), MAX_QP, (int) floor(dQP + 0.5));
  }

  rpcSlice->setSliceQp(qp);
  rpcSlice->setSliceQpDelta      ( 0 );
  pcPic->setLossyQPValue(qp);
  if ((!rpcSlice->getTSResidualCodingDisabledFlag()) && ( rpcSlice->getSPS()->getSpsRangeExtension().getTSRCRicePresentFlag() ))
  {
    rpcSlice->setTsrcIndex(Clip3(MIN_TSRC_RICE, MAX_TSRC_RICE, (int) ((19 - qp) / 6)) - 1);
  }
#if !W0038_CQP_ADJ
  rpcSlice->setSliceChromaQpDelta( COMPONENT_Cb, 0 );
  rpcSlice->setSliceChromaQpDelta( COMPONENT_Cr, 0 );
  rpcSlice->setSliceChromaQpDelta( JOINT_CbCr,   0 );
#endif
  rpcSlice->setUseChromaQpAdj( rpcSlice->getPPS()->getCuChromaQpOffsetListEnabledFlag() && m_pcCfg->getCuChromaQpOffsetEnabled() );
  rpcSlice->setNumRefIdx(REF_PIC_LIST_0, m_pcCfg->getRPLEntry(0, gopId).m_numRefPicsActive);
  rpcSlice->setNumRefIdx(REF_PIC_LIST_1, m_pcCfg->getRPLEntry(1, gopId).m_numRefPicsActive);

  if ( m_pcCfg->getDeblockingFilterMetric() )
  {
    rpcSlice->setDeblockingFilterOverrideFlag(true);
    rpcSlice->setDeblockingFilterDisable(false);
    rpcSlice->setDeblockingFilterBetaOffsetDiv2( 0 );
    rpcSlice->setDeblockingFilterTcOffsetDiv2( 0 );
    rpcSlice->setDeblockingFilterCbBetaOffsetDiv2( 0 );
    rpcSlice->setDeblockingFilterCbTcOffsetDiv2( 0 );
    rpcSlice->setDeblockingFilterCrBetaOffsetDiv2( 0 );
    rpcSlice->setDeblockingFilterCrTcOffsetDiv2( 0 );
  }
  else if (rpcSlice->getPPS()->getDeblockingFilterControlPresentFlag())
  {
    rpcSlice->setDeblockingFilterOverrideFlag(rpcSlice->getPPS()->getDeblockingFilterOverrideEnabledFlag() && !rpcSlice->getPPS()->getPPSDeblockingFilterDisabledFlag());
    rpcSlice->setDeblockingFilterDisable( rpcSlice->getPPS()->getPPSDeblockingFilterDisabledFlag() );
    if ( !rpcSlice->getDeblockingFilterDisable())
    {
      if ( rpcSlice->getDeblockingFilterOverrideFlag() && eSliceType!=I_SLICE)
      {
        rpcSlice->setDeblockingFilterBetaOffsetDiv2(m_pcCfg->getGOPEntry(gopId).m_betaOffsetDiv2
                                                    + m_pcCfg->getDeblockingFilterBetaOffset());
        rpcSlice->setDeblockingFilterTcOffsetDiv2(m_pcCfg->getGOPEntry(gopId).m_tcOffsetDiv2
                                                  + m_pcCfg->getDeblockingFilterTcOffset());
        if( rpcSlice->getPPS()->getPPSChromaToolFlag() )
        {
          rpcSlice->setDeblockingFilterCbBetaOffsetDiv2(m_pcCfg->getGOPEntry(gopId).m_CbBetaOffsetDiv2
                                                        + m_pcCfg->getDeblockingFilterCbBetaOffset());
          rpcSlice->setDeblockingFilterCbTcOffsetDiv2(m_pcCfg->getGOPEntry(gopId).m_CbTcOffsetDiv2
                                                      + m_pcCfg->getDeblockingFilterCbTcOffset());
          rpcSlice->setDeblockingFilterCrBetaOffsetDiv2(m_pcCfg->getGOPEntry(gopId).m_CrBetaOffsetDiv2
                                                        + m_pcCfg->getDeblockingFilterCrBetaOffset());
          rpcSlice->setDeblockingFilterCrTcOffsetDiv2(m_pcCfg->getGOPEntry(gopId).m_CrTcOffsetDiv2
                                                      + m_pcCfg->getDeblockingFilterCrTcOffset());
        }
        else
        {
          rpcSlice->setDeblockingFilterCbBetaOffsetDiv2( rpcSlice->getDeblockingFilterBetaOffsetDiv2() );
          rpcSlice->setDeblockingFilterCbTcOffsetDiv2  ( rpcSlice->getDeblockingFilterTcOffsetDiv2() );
          rpcSlice->setDeblockingFilterCrBetaOffsetDiv2( rpcSlice->getDeblockingFilterBetaOffsetDiv2() );
          rpcSlice->setDeblockingFilterCrTcOffsetDiv2  ( rpcSlice->getDeblockingFilterTcOffsetDiv2() );
        }
      }
      else
      {
        rpcSlice->setDeblockingFilterBetaOffsetDiv2  ( m_pcCfg->getDeblockingFilterBetaOffset() );
        rpcSlice->setDeblockingFilterTcOffsetDiv2    ( m_pcCfg->getDeblockingFilterTcOffset() );
        rpcSlice->setDeblockingFilterCbBetaOffsetDiv2( m_pcCfg->getDeblockingFilterCbBetaOffset() );
        rpcSlice->setDeblockingFilterCbTcOffsetDiv2  ( m_pcCfg->getDeblockingFilterCbTcOffset() );
        rpcSlice->setDeblockingFilterCrBetaOffsetDiv2( m_pcCfg->getDeblockingFilterCrBetaOffset() );
        rpcSlice->setDeblockingFilterCrTcOffsetDiv2  ( m_pcCfg->getDeblockingFilterCrTcOffset() );
      }
    }
  }
  else
  {
    rpcSlice->setDeblockingFilterOverrideFlag( false );
    rpcSlice->setDeblockingFilterDisable( false );
    rpcSlice->setDeblockingFilterBetaOffsetDiv2( 0 );
    rpcSlice->setDeblockingFilterTcOffsetDiv2( 0 );
    rpcSlice->setDeblockingFilterCbBetaOffsetDiv2( 0 );
    rpcSlice->setDeblockingFilterCbTcOffsetDiv2( 0 );
    rpcSlice->setDeblockingFilterCrBetaOffsetDiv2( 0 );
    rpcSlice->setDeblockingFilterCrTcOffsetDiv2( 0 );
  }

  pcPic->temporalId =  temporalId;
  if(eSliceType==I_SLICE)
  {
    pcPic->temporalId = 0;
  }
  rpcSlice->setTLayer( pcPic->temporalId );

  rpcSlice->setDisableSATDForRD(false);

  if( ( m_pcCfg->getIBCHashSearch() && m_pcCfg->getIBCMode() ) || m_pcCfg->getAllowDisFracMMVD() )
  {
    m_pcCuEncoder->getIbcHashMap().destroy();
    m_pcCuEncoder->getIbcHashMap().init( pcPic->cs->pps->getPicWidthInLumaSamples(), pcPic->cs->pps->getPicHeightInLumaSamples() );
  }
#if GDR_ENABLED
  if (m_pcCfg->getGdrEnabled())
  {
    int gdrPocStart = m_pcCuEncoder->getEncCfg()->getGdrPocStart();
    int gdrPeriod = m_pcCuEncoder->getEncCfg()->getGdrPeriod();
    int gdrInterval = m_pcCuEncoder->getEncCfg()->getGdrInterval();

    int picWidth = rpcSlice->getPPS()->getPicWidthInLumaSamples();

    int curPoc = rpcSlice->getPOC();
    int gdrPoc = (curPoc - gdrPocStart) % gdrPeriod;

    pcPic->cs->picHeader->setGdrPicFlag(false);
    pcPic->cs->picHeader->setRecoveryPocCnt(0);

    pcPic->gdrParam.inGdrInterval = false;

    pcPic->cs->picHeader->setVirtualBoundariesPresentFlag(false);

    int  offset = (curPoc < gdrPocStart) ? 0 : (((curPoc - gdrPocStart) / gdrPeriod) * gdrPeriod);
    int  actualGdrStart = gdrPocStart + offset;
    int  actualGdrInterval = std::min(gdrInterval, (int) (pcPic->getPicWidthInLumaSamples() / 8));
    int  recoveryPocCnt = actualGdrInterval - 1;
    int  recoveryPicPoc = actualGdrStart + recoveryPocCnt;

    bool isInGdrInterval = (curPoc >= actualGdrStart) && (curPoc < recoveryPicPoc);
    bool isOutGdrInterval = !isInGdrInterval;
    bool isGdrPic = (actualGdrStart == curPoc);

#if GDR_ENC_TRACE
    printf("\n");
    printf("-poc:%d gdrPocStart:%d actualGdrStart:%d actualGdrInterval:%d actualGdrEndPoc:%d\n", rpcSlice->getPOC(), gdrPocStart, actualGdrStart, actualGdrInterval, recoveryPicPoc - 1);
#endif

    // for none gdr period pictures
    if ((curPoc < gdrPocStart) || isOutGdrInterval)
    {
      pcPic->gdrParam.inGdrInterval = false;
      pcPic->gdrParam.verBoundary = -1;
      pcPic->cs->picHeader->setVirtualBoundariesPresentFlag(false);

      pcPic->cs->picHeader->setNumHorVirtualBoundaries(0);
      pcPic->cs->picHeader->setNumVerVirtualBoundaries(0);

#if GDR_ENC_TRACE
      printf("-poc:%d no virtual boundary\n", rpcSlice->getPOC());
#endif
    }
    // for gdr inteval pictures
    else
    {
      if (curPoc == recoveryPicPoc)
      {
        pcPic->gdrParam.inGdrInterval = false;
      }
      else
      {
        pcPic->gdrParam.inGdrInterval = true;
      }

      pcPic->cs->picHeader->setVirtualBoundariesPresentFlag(true);

      if (isGdrPic)
      {
        pcPic->cs->picHeader->setGdrOrIrapPicFlag(true);
        pcPic->cs->picHeader->setGdrPicFlag(true);

        pcPic->cs->picHeader->setRecoveryPocCnt(recoveryPocCnt);
        m_pcGOPEncoder->setLastGdrIntervalPoc(recoveryPicPoc - 1);
      }

      pcPic->cs->picHeader->setNumHorVirtualBoundaries(0);
      pcPic->cs->picHeader->setNumVerVirtualBoundaries(1);

      int begGdrX;
      int endGdrX;
      int m1, m2, n1;

      double dd = (picWidth / (double)gdrInterval);
      int mm = (int)((picWidth / (double)gdrInterval) + 0.49999);
      m1 = ((mm + 7) >> 3) << 3;
      m2 = ((mm + 0) >> 3) << 3;

      if (dd > mm && m1 == m2)
      {
        m1 = m1 + 8;
      }

      n1 = (picWidth - m2 * gdrInterval) / 8;

      if (gdrPoc < n1)
      {
        begGdrX = m1 * gdrPoc;
        endGdrX = begGdrX + m1;
      }
      else
      {
        begGdrX = m1 * n1 + m2 * (gdrPoc - n1);
        endGdrX = begGdrX + m2;
        if (picWidth <= begGdrX)
        {
          begGdrX = picWidth;
          endGdrX = picWidth;
        }
      }

      pcPic->cs->picHeader->setVirtualBoundariesPosX(endGdrX, 0);
      pcPic->gdrParam.verBoundary = endGdrX;

#if GDR_ENC_TRACE
      printf("\n");
      printf("-poc:%d beg:%d end:%d\n", rpcSlice->getPOC(), begGdrX, endGdrX);
#endif
    }
  }
#endif

  if (rpcSlice->getSPS()->getSpsRangeExtension().getRrcRiceExtensionEnableFlag())
  {
    int bitDepth  = rpcSlice->getSPS()->getBitDepth(ChannelType::LUMA);
    int baseLevel = (bitDepth > 12) ? (rpcSlice->isIntra() ? 5 : 2 * 5) : (rpcSlice->isIntra() ? 2 * 5 : 3 * 5);
    rpcSlice->setRiceBaseLevel(baseLevel);
  }
  else
  {
    rpcSlice->setRiceBaseLevel(4);
  }
}

double EncSlice::initializeLambda(const Slice *slice, const int gopId, const int refQP, const double dQP)
{
  const int                  bitDepthLuma         = slice->getSPS()->getBitDepth(ChannelType::LUMA);
  const int   bitDepthShift = 6 * (bitDepthLuma - 8 - DISTORTION_PRECISION_ADJUSTMENT(bitDepthLuma)) - 12;
  const int   numberBFrames = m_pcCfg->getGOPSize() - 1;
  const SliceType sliceType = slice->getSliceType();
  const int                  temporalId           = m_pcCfg->getGOPEntry(gopId).m_temporalId;
  const std::vector<double> &intraLambdaModifiers = m_pcCfg->getIntraLambdaModifier();
  // case #1: I or P slices (key-frame)
  double dQPFactor = m_pcCfg->getGOPEntry(gopId).m_QPFactor;
  double dLambda, lambdaModifier;

  if (sliceType == I_SLICE)
  {
    if ((m_pcCfg->getIntraQpFactor() >= 0.0) && (m_pcCfg->getGOPEntry(gopId).m_sliceType != I_SLICE))
    {
      dQPFactor = m_pcCfg->getIntraQpFactor();
    }
    else
    {
      if (m_pcCfg->getLambdaFromQPEnable())
      {
        dQPFactor = 0.57;
      }
      else
      {
        dQPFactor =
          0.57 * (1.0 - Clip3(0.0, 0.5, 0.05 * double(slice->getPic()->fieldPic ? numberBFrames >> 1 : numberBFrames)));
      }
    }
  }
  else if (m_pcCfg->getLambdaFromQPEnable())
  {
    dQPFactor = 0.57;
  }

  dLambda = dQPFactor * pow(2.0, (dQP + m_pcCfg->getLambdaScaleTowardsNextQP() + bitDepthShift) / 3.0);
  if (slice->getHierPredLayerIdx() > 0 && !m_pcCfg->getLambdaFromQPEnable())
  {
    dLambda *= Clip3(2.0, 4.0, ((refQP + bitDepthShift) / 6.0));
  }
  // if Hadamard is used in motion estimation process
  if (!m_pcCfg->getUseHADME() && (sliceType != I_SLICE))
  {
    dLambda *= 0.95;
  }
  if ((sliceType != I_SLICE) || intraLambdaModifiers.empty())
  {
    lambdaModifier = m_pcCfg->getLambdaModifier(temporalId);
  }
  else
  {
    lambdaModifier = intraLambdaModifiers[temporalId < intraLambdaModifiers.size() ? temporalId : intraLambdaModifiers.size() - 1];
  }
  dLambda *= lambdaModifier;

  return dLambda;
}

#if SHARP_LUMA_DELTA_QP || ENABLE_QPA_SUB_CTU
double EncSlice::calculateLambda(const Slice *slice,
                                 const int    gopId,   // entry in the GOP table
                                 const double refQP,   // initial slice-level QP
                                 const double dQP,     // initial double-precision QP
                                 int &        qp)              // returned integer QP.
{
  double dLambda = initializeLambda(slice, gopId, int(refQP + 0.5), dQP);
  qp             = Clip3(-slice->getSPS()->getQpBDOffset(ChannelType::LUMA), MAX_QP, int(dQP + 0.5));

  if (slice->getDepQuantEnabledFlag())
  {
    dLambda *= pow( 2.0, 0.25/3.0 ); // slight lambda adjustment for dependent quantization (due to different slope of quantizer)
  }

  // NOTE: the lambda modifiers that are sometimes applied later might be best always applied in here.
  return dLambda;
}
#endif

void EncSlice::resetQP( Picture* pic, int sliceQP, double lambda )
{
  Slice* slice = pic->slices[0];

  // store lambda
  slice->setSliceQp( sliceQP );
#if RDOQ_CHROMA_LAMBDA
  m_pcRdCost->setDistortionWeight (COMPONENT_Y, 1.0); // no chroma weighting for luma
#endif
  setUpLambda(slice, lambda, sliceQP);
#if WCG_EXT
  if (!m_pcCfg->getLumaLevelToDeltaQPMapping().isEnabled())
  {
    m_pcRdCost->saveUnadjustedLambda();
  }
#endif
}

#if ENABLE_QPA
static bool applyQPAdaptation (Picture* const pcPic,       Slice* const pcSlice,        const PreCalcValues& pcv,
                               const bool useSharpLumaDQP,
                               const bool useFrameWiseQPA, const int previouslyAdaptedLumaQP = -1)
{
  const int  bitDepth        = pcSlice->getSPS()->getBitDepth(ChannelType::LUMA);
  const int  iQPIndex    = pcSlice->getSliceQp(); // initial QP index for current slice, used in following loops
  bool   sliceQPModified = false;
  uint32_t   meanLuma    = MAX_UINT;
  double     hpEnerAvg   = 0.0;

#if GLOBAL_AVERAGING
  if (!useFrameWiseQPA || previouslyAdaptedLumaQP < 0)  // mean visual activity value and luma value in each CTU
#endif
  {
    for (uint32_t ctuIdx = 0; ctuIdx < pcSlice->getNumCtuInSlice(); ctuIdx++)
    {
      uint32_t ctuRsAddr = pcSlice->getCtuAddrInSlice( ctuIdx );
      const Position pos ((ctuRsAddr % pcv.widthInCtus) * pcv.maxCUWidth, (ctuRsAddr / pcv.widthInCtus) * pcv.maxCUHeight);
      const CompArea ctuArea    = clipArea (CompArea (COMPONENT_Y, pcPic->chromaFormat, Area (pos.x, pos.y, pcv.maxCUWidth, pcv.maxCUHeight)), pcPic->Y());
      const CompArea fltArea    = clipArea (CompArea (COMPONENT_Y, pcPic->chromaFormat, Area (pos.x > 0 ? pos.x - 1 : 0, pos.y > 0 ? pos.y - 1 : 0, pcv.maxCUWidth + (pos.x > 0 ? 2 : 1), pcv.maxCUHeight + (pos.y > 0 ? 2 : 1))), pcPic->Y());
      const CPelBuf  picOrig    = pcPic->getOrigBuf (fltArea);
      double hpEner = 0.0;

      filterAndCalculateAverageEnergies (picOrig.buf,    picOrig.stride, hpEner,
                                         picOrig.height, picOrig.width,  bitDepth);
      hpEnerAvg += hpEner;
      pcPic->m_uEnerHpCtu[ctuRsAddr] = hpEner;
      pcPic->m_iOffsetCtu[ctuRsAddr] = pcPic->getOrigBuf (ctuArea).computeAvg();
    }

    hpEnerAvg /= double (pcSlice->getNumCtuInSlice());
  }
#if GLOBAL_AVERAGING
  const double hpEnerPic = 1.0 / getAveragePictureEnergy (pcPic->getOrigBuf().Y(), bitDepth);  // inverse, speed
#else
  const double hpEnerPic = 1.0 / hpEnerAvg; // speedup: multiply instead of divide in loop below; 1.0 for tuning
#endif

  if (useFrameWiseQPA || (iQPIndex >= MAX_QP))
  {
    int iQPFixed = (previouslyAdaptedLumaQP < 0) ? Clip3 (0, MAX_QP, iQPIndex + apprI3Log2 (hpEnerAvg * hpEnerPic)) : previouslyAdaptedLumaQP;

    if (isChromaEnabled (pcPic->chromaFormat) && (iQPIndex < MAX_QP) && (previouslyAdaptedLumaQP < 0))
    {
      iQPFixed += getGlaringColorQPOffset (pcPic, -1 /*ctuRsAddr*/, pcSlice, bitDepth, meanLuma);

      if (iQPFixed > MAX_QP
#if SHARP_LUMA_DELTA_QP
          && !useSharpLumaDQP
#endif
      )
      {
        iQPFixed = MAX_QP;
      }
    }
#if SHARP_LUMA_DELTA_QP

    // change new fixed QP based on average CTU luma value (Sharp)
    if (useSharpLumaDQP && (iQPIndex < MAX_QP) && (previouslyAdaptedLumaQP < 0))
    {
      if (meanLuma == MAX_UINT) // collect picture mean luma value
      {
        meanLuma = 0;

        for (uint32_t ctuIdx = 0; ctuIdx < pcSlice->getNumCtuInSlice(); ctuIdx++)
        {
          uint32_t ctuRsAddr = pcSlice->getCtuAddrInSlice( ctuIdx );

          meanLuma += pcPic->m_iOffsetCtu[ctuRsAddr];  // CTU mean
        }
        meanLuma = (meanLuma + (pcSlice->getNumCtuInSlice() >> 1)) / pcSlice->getNumCtuInSlice();
      }
      iQPFixed = Clip3 (0, MAX_QP, iQPFixed + lumaDQPOffset (meanLuma, bitDepth));
    }
#endif

    if (iQPIndex >= MAX_QP)
    {
      iQPFixed = MAX_QP;
    }
    else if (iQPFixed != iQPIndex)
    {
      const double* oldLambdas = pcSlice->getLambdas();
      const double  corrFactor = pow (2.0, double(iQPFixed - iQPIndex) / 3.0);
      const double  newLambdas[MAX_NUM_COMPONENT] = {oldLambdas[0] * corrFactor, oldLambdas[1] * corrFactor, oldLambdas[2] * corrFactor};

      CHECK (iQPIndex != pcSlice->getSliceQpBase(), "Invalid slice QP!");
      pcSlice->setLambdas (newLambdas);
      pcSlice->setSliceQp (iQPFixed); // update the slice/base QPs
      pcSlice->setSliceQpBase (iQPFixed);

      sliceQPModified = true;
    }

    for (uint32_t ctuIdx = 0; ctuIdx < pcSlice->getNumCtuInSlice(); ctuIdx++)
    {
      uint32_t ctuRsAddr = pcSlice->getCtuAddrInSlice( ctuIdx );

      pcPic->m_iOffsetCtu[ctuRsAddr] = (Pel)iQPFixed; // fixed QPs
    }
  }
  else // CTU-wise QPA
  {
    for (uint32_t ctuIdx = 0; ctuIdx < pcSlice->getNumCtuInSlice(); ctuIdx++)
    {
      uint32_t ctuRsAddr = pcSlice->getCtuAddrInSlice( ctuIdx );

      int iQPAdapt = Clip3 (0, MAX_QP, iQPIndex + apprI3Log2 (pcPic->m_uEnerHpCtu[ctuRsAddr] * hpEnerPic));

      if (pcv.widthInCtus > 1) // try to enforce CTU SNR greater than zero dB
      {
        meanLuma = (uint32_t)pcPic->m_iOffsetCtu[ctuRsAddr];

        if (isChromaEnabled (pcPic->chromaFormat))
        {
          iQPAdapt += getGlaringColorQPOffset (pcPic, (int)ctuRsAddr, nullptr, bitDepth, meanLuma);

          if (iQPAdapt > MAX_QP
#if SHARP_LUMA_DELTA_QP
              && !useSharpLumaDQP
#endif
          )
          {
            iQPAdapt = MAX_QP;
          }
          CHECK (meanLuma != (uint32_t)pcPic->m_iOffsetCtu[ctuRsAddr], "luma DC offsets don't match");
        }
#if SHARP_LUMA_DELTA_QP

        // change adaptive QP based on mean CTU luma value (Sharp)
        if (useSharpLumaDQP)
        {
 #if ENABLE_QPA_SUB_CTU
          pcPic->m_uEnerHpCtu[ctuRsAddr] = (double)meanLuma; // for sub-CTU QPA
 #endif
          iQPAdapt = Clip3 (0, MAX_QP, iQPAdapt + lumaDQPOffset (meanLuma, bitDepth));
        }

#endif
        const uint32_t uRefScale  = g_invQuantScales[0][iQPAdapt % 6] << ((iQPAdapt / 6) + bitDepth - 4);
        const CompArea subArea    = clipArea (CompArea (COMPONENT_Y, pcPic->chromaFormat, Area ((ctuRsAddr % pcv.widthInCtus) * pcv.maxCUWidth, (ctuRsAddr / pcv.widthInCtus) * pcv.maxCUHeight, pcv.maxCUWidth, pcv.maxCUHeight)), pcPic->Y());
        const Pel*     pSrc       = pcPic->getOrigBuf (subArea).buf;
        const ptrdiff_t srcStride  = pcPic->getOrigBuf(subArea).stride;
        const SizeType srcHeight  = pcPic->getOrigBuf(subArea).height;
        const SizeType srcWidth   = pcPic->getOrigBuf(subArea).width;
        uint32_t uAbsDCless = 0;

        // compute sum of absolute DC-less (high-pass) luma values
        for (SizeType h = 0; h < srcHeight; h++)
        {
          for (SizeType w = 0; w < srcWidth; w++)
          {
            uAbsDCless += (uint32_t)abs (pSrc[w] - (Pel)meanLuma);
          }
          pSrc += srcStride;
        }

        if (srcHeight >= 64 || srcWidth >= 64)   // normalization
        {
          const uint64_t blockSize = uint64_t(srcWidth * srcHeight);

          uAbsDCless = uint32_t((uint64_t(uAbsDCless) * 64*64 + (blockSize >> 1)) / blockSize);
        }

        if (uAbsDCless < 64 * 64)
        {
          uAbsDCless = 64 * 64;   // limit to 1
        }

        // reduce QP index if CTU would be fully quantized to zero
        if (uAbsDCless < uRefScale)
        {
          const int limit  = std::min (0, ((iQPIndex + 4) >> 3) - 6);
          const int redVal = std::max (limit, apprI3Log2 ((double)uAbsDCless / (double)uRefScale));

          iQPAdapt = std::max (0, iQPAdapt + redVal);
        }
      }

      pcPic->m_iOffsetCtu[ctuRsAddr] = (Pel)iQPAdapt; // adapted QPs

#if ENABLE_QPA_SUB_CTU
      if (pcv.widthInCtus > 1 && pcSlice->getCuQpDeltaSubdiv() == 0)  // reduce local DQP rate peaks
#elif ENABLE_QPA_SUB_CTU
      if (pcv.widthInCtus > 1 && pcSlice->getPPS()->getMaxCuDQPDepth() == 0)  // reduce local DQP rate peaks
#else
      if (pcv.widthInCtus > 1) // try to reduce local bitrate peaks via minimum smoothing of the adapted QPs
#endif
      {
        iQPAdapt = ctuRsAddr % pcv.widthInCtus; // horizontal offset
        if (iQPAdapt == 0)
        {
          iQPAdapt = (ctuRsAddr > 1) ? pcPic->m_iOffsetCtu[ctuRsAddr - 2] : 0;
        }
        else // iQPAdapt >= 1
        {
          iQPAdapt = (iQPAdapt > 1) ? std::min (pcPic->m_iOffsetCtu[ctuRsAddr - 2], pcPic->m_iOffsetCtu[ctuRsAddr]) : pcPic->m_iOffsetCtu[ctuRsAddr];
        }
        if (ctuRsAddr > pcv.widthInCtus)
        {
          iQPAdapt = std::min (iQPAdapt, (int)pcPic->m_iOffsetCtu[ctuRsAddr - 1 - pcv.widthInCtus]);
        }
        if ((ctuRsAddr > 0) && (pcPic->m_iOffsetCtu[ctuRsAddr - 1] < (Pel)iQPAdapt))
        {
          pcPic->m_iOffsetCtu[ctuRsAddr - 1] = (Pel)iQPAdapt;
        }
        if ((ctuIdx == pcSlice->getNumCtuInSlice() - 1) && (ctuRsAddr > pcv.widthInCtus)) // last CTU in the given slice
        {
          iQPAdapt = std::min (pcPic->m_iOffsetCtu[ctuRsAddr - 1], pcPic->m_iOffsetCtu[ctuRsAddr - pcv.widthInCtus]);
          if (pcPic->m_iOffsetCtu[ctuRsAddr] < (Pel)iQPAdapt)
          {
            pcPic->m_iOffsetCtu[ctuRsAddr] = (Pel)iQPAdapt;
          }
        }
      }
    } // end iteration over all CTUs in current slice
  }

  return sliceQPModified;
}

#if ENABLE_QPA_SUB_CTU
static int applyQPAdaptationSubCtu (CodingStructure &cs, const UnitArea ctuArea, const uint32_t ctuAddr, const bool useSharpLumaDQP)
{
  const PreCalcValues &pcv = *cs.pcv;
  const Picture     *pcPic = cs.picture;
  const int            bitDepth     = cs.slice->getSPS()->getBitDepth(ChannelType::LUMA);   // overall image bit-depth
  const int   adaptedCtuQP = pcPic ? pcPic->m_iOffsetCtu[ctuAddr] : cs.slice->getSliceQpBase();

  if (!pcPic || cs.slice->getCuQpDeltaSubdiv() == 0)
  {
    return adaptedCtuQP;
  }

  for (unsigned addr = 0; addr < cs.picture->m_subCtuQP.size(); addr++)
  {
    cs.picture->m_subCtuQP[addr] = (int8_t)adaptedCtuQP;
  }
  if (cs.slice->getSliceQp() < MAX_QP && pcv.widthInCtus > 1)
  {
#if SHARP_LUMA_DELTA_QP
    const int   lumaCtuDQP = useSharpLumaDQP ? lumaDQPOffset ((uint32_t)pcPic->m_uEnerHpCtu[ctuAddr], bitDepth) : 0;
#endif
    const unsigned     mts = std::min (cs.sps->getMaxTbSize(), pcv.maxCUWidth);
    const unsigned mtsLog2 = (unsigned)floorLog2(mts);
    const unsigned  stride = pcv.maxCUWidth >> mtsLog2;
    unsigned numAct = 0;    // number of block activities
    double   sumAct = 0.0; // sum of all block activities
    double   subAct[16];   // individual block activities
#if SHARP_LUMA_DELTA_QP
    uint32_t subMLV[16];   // individual mean luma values
#endif

    CHECK (mts * 4 < pcv.maxCUWidth || mts * 4 < pcv.maxCUHeight, "max. transform size is too small for given CTU size");

    for (unsigned h = 0; h < (pcv.maxCUHeight >> mtsLog2); h++)
    {
      for (unsigned w = 0; w < stride; w++)
      {
        const unsigned addr    = w + h * stride;
        const PosType  x       = ctuArea.lx() + w * mts;
        const PosType  y       = ctuArea.ly() + h * mts;
        const CompArea fltArea = clipArea (CompArea (COMPONENT_Y, pcPic->chromaFormat, Area (x > 0 ? x - 1 : 0, y > 0 ? y - 1 : 0, mts + (x > 0 ? 2 : 1), mts + (y > 0 ? 2 : 1))), pcPic->Y());
        const CPelBuf  picOrig = pcPic->getOrigBuf (fltArea);

        if (x >= pcPic->lwidth() || y >= pcPic->lheight())
        {
          continue;
        }
        filterAndCalculateAverageEnergies (picOrig.buf,    picOrig.stride, subAct[addr],
                                           picOrig.height, picOrig.width,  bitDepth);
        numAct++;
        sumAct += subAct[addr];
#if SHARP_LUMA_DELTA_QP

        if (useSharpLumaDQP)
        {
          const CompArea subArea = clipArea (CompArea (COMPONENT_Y, pcPic->chromaFormat, Area (x, y, mts, mts)), pcPic->Y());

          subMLV[addr] = pcPic->getOrigBuf (subArea).computeAvg();
        }
#endif
      }
    }
    if (sumAct <= 0.0)
    {
      return adaptedCtuQP;
    }

    sumAct = double(numAct) / sumAct; // 1.0 / (average CTU activity)

    for (unsigned h = 0; h < (pcv.maxCUHeight >> mtsLog2); h++)
    {
      for (unsigned w = 0; w < stride; w++)
      {
        const unsigned addr = w + h * stride;

        if (ctuArea.lx() + w * mts >= pcPic->lwidth() || ctuArea.ly() + h * mts >= pcPic->lheight())
        {
          continue;
        }
        cs.picture->m_subCtuQP[addr] = (int8_t)Clip3 (0, MAX_QP, adaptedCtuQP + apprI3Log2 (subAct[addr] * sumAct));
#if SHARP_LUMA_DELTA_QP

        // change adapted QP based on mean sub-CTU luma value (Sharp)
        if (useSharpLumaDQP)
        {
          cs.picture->m_subCtuQP[addr] = (int8_t)Clip3 (0, MAX_QP, (int)cs.picture->m_subCtuQP[addr] - lumaCtuDQP + lumaDQPOffset (subMLV[addr], bitDepth));
        }
#endif
      }
    }
  }

  return adaptedCtuQP;
}
#endif // ENABLE_QPA_SUB_CTU
#endif // ENABLE_QPA

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

//! set adaptive search range based on poc difference
void EncSlice::setSearchRange( Slice* pcSlice )
{
  int currPoc = pcSlice->getPOC();
  int iRefPOC;
  int iGOPSize = m_pcCfg->getGOPSize();
  int offset      = (iGOPSize >> 1);
  int iMaxSR = m_pcCfg->getSearchRange();
  int iNumPredDir = pcSlice->isInterP() ? 1 : 2;

  for (int dir = 0; dir < iNumPredDir; dir++)
  {
    RefPicList e = (dir ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
    for (int refIdx = 0; refIdx < pcSlice->getNumRefIdx(e); refIdx++)
    {
      iRefPOC            = pcSlice->getRefPic(e, refIdx)->getPOC();
      int newSearchRange = Clip3(m_pcCfg->getMinSearchWindow(), iMaxSR,
                                 (iMaxSR * ADAPT_SR_SCALE * abs(currPoc - iRefPOC) + offset) / iGOPSize);
      m_pcInterSearch->setAdaptiveSearchRange(dir, refIdx, newSearchRange);
    }
  }
}

void EncSlice::setLosslessSlice(Picture* pcPic, bool islossless)
{
  Slice* slice = pcPic->slices[getSliceSegmentIdx()];
  slice->setLossless(islossless);

  if (m_pcCfg->getCostMode() == COST_LOSSLESS_CODING)
  {
    if (islossless)
    {
      int losslessQp =
        LOSSLESS_AND_MIXED_LOSSLESS_RD_COST_TEST_QP - ((slice->getSPS()->getBitDepth(ChannelType::LUMA) - 8) * 6);
      slice->setSliceQp(losslessQp); // update the slice/base QPs

      slice->setTSResidualCodingDisabledFlag(m_pcCfg->getTSRCdisableLL() ? true : false);
    }
    else
    {
      slice->setSliceQp(pcPic->getLossyQPValue());
      slice->setTSResidualCodingDisabledFlag(false);
    }
  }
}


/**
 Multi-loop slice encoding for different slice QP

 \param pcPic    picture class
 */
void EncSlice::precompressSlice( Picture* pcPic )
{
  if (m_pcCfg->getDPF())
  {
    setLambdaWeightByDPF(pcPic);
  }
  // if deltaQP RD is not used, simply return
  if ( m_pcCfg->getDeltaQpRD() == 0 )
  {
    return;
  }

  if ( m_pcCfg->getUseRateCtrl() )
  {
    THROW("\nMultiple QP optimization is not allowed when rate control is enabled." );
  }

  Slice* pcSlice        = pcPic->slices[getSliceSegmentIdx()];

  double     dPicRdCostBest = MAX_DOUBLE;
  uint32_t       uiQpIdxBest = 0;

  double dFrameLambda;
  int    SHIFT_QP = 12
                 + 6
                     * (pcSlice->getSPS()->getBitDepth(ChannelType::LUMA) - 8
                        - DISTORTION_PRECISION_ADJUSTMENT(pcSlice->getSPS()->getBitDepth(ChannelType::LUMA)));

  // set frame lambda
  if (m_pcCfg->getGOPSize() > 1)
  {
    dFrameLambda = 0.68 * pow (2, (m_viRdPicQp[0]  - SHIFT_QP) / 3.0) * (pcSlice->isInterB()? 2 : 1);
  }
  else
  {
    dFrameLambda = 0.68 * pow (2, (m_viRdPicQp[0] - SHIFT_QP) / 3.0);
  }

  // for each QP candidate
  for ( uint32_t uiQpIdx = 0; uiQpIdx < 2 * m_pcCfg->getDeltaQpRD() + 1; uiQpIdx++ )
  {
    pcSlice       ->setSliceQp             ( m_viRdPicQp    [uiQpIdx] );
    setUpLambda(pcSlice, m_vdRdPicLambda[uiQpIdx], m_viRdPicQp    [uiQpIdx]);

    // try compress
    compressSlice   ( pcPic, true, m_pcCfg->getFastDeltaQp());

    uint64_t uiPicDist        = m_uiPicDist; // Distortion, as calculated by compressSlice.
    // NOTE: This distortion is the chroma-weighted SSE distortion for the slice.
    //       Previously a standard SSE distortion was calculated (for the entire frame).
    //       Which is correct?
    // TODO: Update loop filter, SAO and distortion calculation to work on one slice only.
    // uiPicDist = m_pcGOPEncoder->preLoopFilterPicAndCalcDist( pcPic );
    // compute RD cost and choose the best
    double dPicRdCost = double( uiPicDist ) + dFrameLambda * double( m_uiPicTotalBits );

    if ( dPicRdCost < dPicRdCostBest )
    {
      uiQpIdxBest    = uiQpIdx;
      dPicRdCostBest = dPicRdCost;
    }
  }

  // set best values
  pcSlice       ->setSliceQp             ( m_viRdPicQp    [uiQpIdxBest] );
  setUpLambda(pcSlice, m_vdRdPicLambda[uiQpIdxBest], m_viRdPicQp    [uiQpIdxBest]);
}

void EncSlice::calCostSliceI(Picture* pcPic) // TODO: this only analyses the first slice segment. What about the others?
{
  double         iSumHadSlice      = 0;
  Slice * const  pcSlice           = pcPic->slices[getSliceSegmentIdx()];
  const PreCalcValues& pcv         = *pcPic->cs->pcv;
  const SPS     &sps               = *(pcSlice->getSPS());
  const int            shift             = sps.getBitDepth(ChannelType::LUMA) - 8;
  const int      offset            = (shift>0)?(1<<(shift-1)):0;

  for( uint32_t ctuIdx = 0; ctuIdx < pcSlice->getNumCtuInSlice(); ctuIdx++ )
  {
    uint32_t ctuRsAddr = pcSlice->getCtuAddrInSlice( ctuIdx );
    Position pos( (ctuRsAddr % pcv.widthInCtus) * pcv.maxCUWidth, (ctuRsAddr / pcv.widthInCtus) * pcv.maxCUHeight);

    const int height  = std::min( pcv.maxCUHeight, pcv.lumaHeight - pos.y );
    const int width   = std::min( pcv.maxCUWidth,  pcv.lumaWidth  - pos.x );
    const CompArea blk( COMPONENT_Y, pcv.chrFormat, pos, Size( width, height));
    int iSumHad = m_pcCuEncoder->updateCtuDataISlice( pcPic->getOrigBuf( blk ) );

    (m_pcRateCtrl->getRCPic()->getLCU(ctuRsAddr)).m_costIntra=(iSumHad+offset)>>shift;
    iSumHadSlice += (m_pcRateCtrl->getRCPic()->getLCU(ctuRsAddr)).m_costIntra;
  }
  m_pcRateCtrl->getRCPic()->setTotalIntraCost(iSumHadSlice);
}

void EncSlice::calCostPictureI(Picture* picture)
{
  double         sumHadPicture = 0;
  Slice * const  slice = picture->slices[getSliceSegmentIdx()];
  const PreCalcValues& pcv = *picture->cs->pcv;
  const SPS     &sps = *(slice->getSPS());
  const int            shift         = sps.getBitDepth(ChannelType::LUMA) - 8;
  const int      offset = (shift>0) ? (1 << (shift - 1)) : 0;

  for (uint32_t ctuIdx = 0; ctuIdx < picture->m_ctuNums; ctuIdx++)
  {
    Position pos((ctuIdx % pcv.widthInCtus) * pcv.maxCUWidth, (ctuIdx / pcv.widthInCtus) * pcv.maxCUHeight);

    const int height = std::min(pcv.maxCUHeight, pcv.lumaHeight - pos.y);
    const int width = std::min(pcv.maxCUWidth, pcv.lumaWidth - pos.x);
    const CompArea blk(COMPONENT_Y, pcv.chrFormat, pos, Size(width, height));
    int sumHad = m_pcCuEncoder->updateCtuDataISlice(picture->getOrigBuf(blk));

    (m_pcRateCtrl->getRCPic()->getLCU(ctuIdx)).m_costIntra = (sumHad + offset) >> shift;
    sumHadPicture += (m_pcRateCtrl->getRCPic()->getLCU(ctuIdx)).m_costIntra;
  }
  m_pcRateCtrl->getRCPic()->setTotalIntraCost(sumHadPicture);
}

/** \param pcPic   picture class
 */
void EncSlice::compressSlice( Picture* pcPic, const bool bCompressEntireSlice, const bool bFastDeltaQP )
{
  // if bCompressEntireSlice is true, then the entire slice (not slice segment) is compressed,
  //   effectively disabling the slice-segment-mode.

  Slice* const pcSlice    = pcPic->slices[getSliceSegmentIdx()];

  if (pcSlice->getSPS()->getSpsRangeExtension().getRrcRiceExtensionEnableFlag())
  {
    int bitDepth  = pcSlice->getSPS()->getBitDepth(ChannelType::LUMA);
    int baseLevel = (bitDepth > 12) ? (pcSlice->isIntra() ? 5 : 2 * 5 ) : (pcSlice->isIntra() ? 2 * 5 : 3 * 5);
    pcSlice->setRiceBaseLevel(baseLevel);
  }
  else
  {
    pcSlice->setRiceBaseLevel(4);
  }

  // initialize cost values - these are used by precompressSlice (they should be parameters).
  m_uiPicTotalBits  = 0;
  m_uiPicDist       = 0;

  pcSlice->setSliceQpBase( pcSlice->getSliceQp() );

  m_CABACEstimator->initCtxModels( *pcSlice );

  m_pcCuEncoder->getModeCtrl()->setFastDeltaQp(bFastDeltaQP);


  //------------------------------------------------------------------------------
  //  Weighted Prediction parameters estimation.
  //------------------------------------------------------------------------------
  // calculate AC/DC values for current picture
  if( pcSlice->getPPS()->getUseWP() || pcSlice->getPPS()->getWPBiPred() )
  {
    xCalcACDCParamSlice(pcSlice);
  }

  const bool bWp_explicit = (pcSlice->getSliceType()==P_SLICE && pcSlice->getPPS()->getUseWP()) || (pcSlice->getSliceType()==B_SLICE && pcSlice->getPPS()->getWPBiPred());

  if ( bWp_explicit )
  {

    xEstimateWPParamSlice( pcSlice, m_pcCfg->getWeightedPredictionMethod() );
    pcSlice->initWpScaling(pcSlice->getSPS());

    // check WP on/off
    xCheckWPEnable( pcSlice );
  }

  pcPic->m_prevQP.fill(pcSlice->getSliceQp());

  CHECK(pcPic->m_prevQP[ChannelType::LUMA] == std::numeric_limits<int>::max(), "Invalid previous QP");

  CodingStructure&  cs          = *pcPic->cs;
  cs.slice    = pcSlice;
  cs.pcv      = pcSlice->getPPS()->pcv;
  cs.fracBits = 0;

  if( pcSlice->getFirstCtuRsAddrInSlice() == 0 && ( pcSlice->getPOC() != m_pcCfg->getSwitchPOC() || -1 == m_pcCfg->getDebugCTU() ) )
  {
    cs.initStructData (pcSlice->getSliceQp());
  }

#if ENABLE_QPA
  if (m_pcCfg->getUsePerceptQPA() && !m_pcCfg->getUseRateCtrl())
  {
    if (applyQPAdaptation (pcPic, pcSlice, *cs.pcv, m_pcCfg->getLumaLevelToDeltaQPMapping().mode == LUMALVL_TO_DQP_NUM_MODES,
                           (m_pcCfg->getBaseQP() >= 38) || (m_pcCfg->getSourceWidth() <= 512 && m_pcCfg->getSourceHeight() <= 320), m_adaptedLumaQP))
    {
      m_CABACEstimator->initCtxModels (*pcSlice);
      pcPic->m_prevQP.fill(pcSlice->getSliceQp());
      if (pcSlice->getFirstCtuRsAddrInSlice() == 0)
      {
        cs.currQP.fill(pcSlice->getSliceQp());
      }
    }
  }
#endif // ENABLE_QPA

  bool checkPLTRatio = m_pcCfg->getIntraPeriod() != 1 && pcSlice->isIRAP();
  if (checkPLTRatio)
  {
    m_pcCuEncoder->getModeCtrl()->setPltEnc(true);
  }
  else
  {
    bool doPlt = m_pcLib->getPltEnc();
    m_pcCuEncoder->getModeCtrl()->setPltEnc(doPlt);
  }

#if K0149_BLOCK_STATISTICS
  const SPS *sps = pcSlice->getSPS();
  CHECK(sps == 0, "No SPS present");
  writeBlockStatisticsHeader(sps);
#endif
  m_pcInterSearch->resetAffineMVList();
  m_pcInterSearch->resetUniMvList();
  ::memset(g_isReusedUniMVsFilled, 0, sizeof(g_isReusedUniMVsFilled));
  encodeCtus( pcPic, bCompressEntireSlice, bFastDeltaQP, m_pcLib );
  if (checkPLTRatio)
  {
    m_pcLib->checkPltStats(pcPic);
  }
}

void EncSlice::checkDisFracMmvd( Picture* pcPic, uint32_t startCtuTsAddr, uint32_t boundingCtuTsAddr )
{
  CodingStructure&  cs            = *pcPic->cs;
  Slice* pcSlice                  = cs.slice;
  const PreCalcValues& pcv        = *cs.pcv;
  const uint32_t    widthInCtus   = pcv.widthInCtus;
  const uint32_t hashThreshold    = 20;
  uint32_t totalCtu               = 0;
  uint32_t hashRatio              = 0;

  if ( !pcSlice->getSPS()->getFpelMmvdEnabledFlag() )
  {
    return;
  }

  for ( uint32_t ctuIdx = 0; ctuIdx < pcSlice->getNumCtuInSlice(); ctuIdx++ )
  {
    const uint32_t ctuRsAddr = pcSlice->getCtuAddrInSlice( ctuIdx );
    const uint32_t ctuXPosInCtus        = ctuRsAddr % widthInCtus;
    const uint32_t ctuYPosInCtus        = ctuRsAddr / widthInCtus;

    const Position pos ( ctuXPosInCtus * pcv.maxCUWidth, ctuYPosInCtus * pcv.maxCUHeight );
    const UnitArea ctuArea( cs.area.chromaFormat, Area( pos.x, pos.y, pcv.maxCUWidth, pcv.maxCUHeight ) );

    hashRatio += m_pcCuEncoder->getIbcHashMap().getHashHitRatio( ctuArea.Y() );
    totalCtu++;
  }

  if ( hashRatio > totalCtu * hashThreshold )
  {
    pcPic->cs->picHeader->setDisFracMMVD( true );
  }
  if (!pcPic->cs->picHeader->getDisFracMMVD())
  {
    bool useIntegerMVD = (pcPic->lwidth()*pcPic->lheight() > 1920 * 1080);
    pcPic->cs->picHeader->setDisFracMMVD( useIntegerMVD );
  }
}

void EncSlice::setJointCbCrModes( CodingStructure& cs, const Position topLeftLuma, const Size sizeLuma )
{
  bool              sgnFlag = true;

  if( isChromaEnabled( cs.picture->chromaFormat) )
  {
    const CompArea  cbArea  = CompArea( COMPONENT_Cb, cs.picture->chromaFormat, Area(topLeftLuma,sizeLuma), true );
    const CompArea  crArea  = CompArea( COMPONENT_Cr, cs.picture->chromaFormat, Area(topLeftLuma,sizeLuma), true );
    const CPelBuf   orgCb   = cs.picture->getOrigBuf( cbArea );
    const CPelBuf   orgCr   = cs.picture->getOrigBuf( crArea );
    const int       x0      = ( cbArea.x > 0 ? 0 : 1 );
    const int       y0      = ( cbArea.y > 0 ? 0 : 1 );
    const int       x1      = ( cbArea.x + cbArea.width  < cs.picture->Cb().width  ? cbArea.width  : cbArea.width  - 1 );
    const int       y1      = ( cbArea.y + cbArea.height < cs.picture->Cb().height ? cbArea.height : cbArea.height - 1 );
    const ptrdiff_t cbs     = orgCb.stride;
    const ptrdiff_t crs     = orgCr.stride;
    const Pel*      pCb     = orgCb.buf + y0 * cbs;
    const Pel*      pCr     = orgCr.buf + y0 * crs;
    int64_t         sumCbCr = 0;

    // determine inter-chroma transform sign from correlation between high-pass filtered (i.e., zero-mean) Cb and Cr planes
    for( int y = y0; y < y1; y++, pCb += cbs, pCr += crs )
    {
      for( int x = x0; x < x1; x++ )
      {
        int cb = ( 12*(int)pCb[x] - 2*((int)pCb[x-1] + (int)pCb[x+1] + (int)pCb[x-cbs] + (int)pCb[x+cbs]) - ((int)pCb[x-1-cbs] + (int)pCb[x+1-cbs] + (int)pCb[x-1+cbs] + (int)pCb[x+1+cbs]) );
        int cr = ( 12*(int)pCr[x] - 2*((int)pCr[x-1] + (int)pCr[x+1] + (int)pCr[x-crs] + (int)pCr[x+crs]) - ((int)pCr[x-1-crs] + (int)pCr[x+1-crs] + (int)pCr[x-1+crs] + (int)pCr[x+1+crs]) );
        sumCbCr += cb*cr;
      }
    }

    sgnFlag = ( sumCbCr < 0 );
  }

  cs.picHeader->setJointCbCrSignFlag( sgnFlag );
}


void EncSlice::encodeCtus( Picture* pcPic, const bool bCompressEntireSlice, const bool bFastDeltaQP, EncLib* pEncLib )
{
  CodingStructure&  cs            = *pcPic->cs;
  Slice* pcSlice                  = cs.slice;
  const PreCalcValues& pcv        = *cs.pcv;
  const uint32_t        widthInCtus   = pcv.widthInCtus;
#if ENABLE_QPA
  const int iQPIndex              = pcSlice->getSliceQpBase();
#endif

  CABACWriter*    pCABACWriter    = pEncLib->getCABACEncoder()->getCABACEstimator( pcSlice->getSPS() );
  TrQuant*        pTrQuant        = pEncLib->getTrQuant();
  RdCost*         pRdCost         = pEncLib->getRdCost();
  EncCfg*         pCfg            = pEncLib;
  RateCtrl*       pRateCtrl       = pEncLib->getRateCtrl();
  pRdCost->setLosslessRDCost(pcSlice->isLossless());
#if RDOQ_CHROMA_LAMBDA
  pTrQuant    ->setLambdas( pcSlice->getLambdas() );
#else
  pTrQuant    ->setLambda ( pcSlice->getLambdas()[0] );
#endif
  pRdCost     ->setLambda ( pcSlice->getLambdas()[0], pcSlice->getSPS()->getBitDepths() );
#if WCG_EXT && ER_CHROMA_QP_WCG_PPS && ENABLE_QPA
  if (!pCfg->getWCGChromaQPControl().isEnabled() && pCfg->getUsePerceptQPA() && !pCfg->getUseRateCtrl())
  {
    pRdCost->saveUnadjustedLambda();
  }
#endif

  EnumArray<int, ChannelType> prevQP;
  EnumArray<int, ChannelType> currQP;

  prevQP.fill(pcSlice->getSliceQp());
  currQP.fill(pcSlice->getSliceQp());

  if ( pcSlice->getSPS()->getFpelMmvdEnabledFlag() ||
      (pcSlice->getSPS()->getIBCFlag() && m_pcCuEncoder->getEncCfg()->getIBCHashSearch()))
  {
    m_pcCuEncoder->getIbcHashMap().rebuildPicHashMap(cs.picture->getTrueOrigBuf());
    if (!m_pcCfg->getIsLowDelay())
    {
      int hashBlkHitPerc = m_pcCuEncoder->getIbcHashMap().calHashBlkMatchPerc(cs.area.Y());
      cs.slice->setDisableSATDForRD(hashBlkHitPerc > 59);
    }
    if ((pcSlice->getSPS()->getSpsRangeExtension().getTSRCRicePresentFlag())
        && (m_pcGOPEncoder->getPreQP() != pcSlice->getSliceQp()) && (pcPic->cs->pps->getNumSlicesInPic() == 1)
        && (pcSlice->getTsrcIndex() > 0) && (pcSlice->getSPS()->getBitDepth(ChannelType::LUMA) <= 12))
    {
      uint32_t totalCtu  = 0;
      uint32_t hashRatio = 0;
      for (uint32_t ctuIdx = 0; ctuIdx < pcSlice->getNumCtuInSlice(); ctuIdx++)
      {
        const uint32_t ctuRsAddr     = pcSlice->getCtuAddrInSlice(ctuIdx);
        const uint32_t ctuXPosInCtus = ctuRsAddr % widthInCtus;
        const uint32_t ctuYPosInCtus = ctuRsAddr / widthInCtus;
        const Position pos(ctuXPosInCtus * pcv.maxCUWidth, ctuYPosInCtus * pcv.maxCUHeight);
        const UnitArea ctuArea(cs.area.chromaFormat, Area(pos.x, pos.y, pcv.maxCUWidth, pcv.maxCUHeight));

        hashRatio += m_pcCuEncoder->getIbcHashMap().calHashBlkMatchPerc(cs.area.Y());
        totalCtu++;
      }
      if (totalCtu > 0)
      {
        if ((hashRatio < 4200) || (hashRatio < (41 * totalCtu)))
        {
          pcSlice->setTsrcIndex(0);
        }
      }
    }
  }

  // for every CTU in the slice
  for( uint32_t ctuIdx = 0; ctuIdx < pcSlice->getNumCtuInSlice(); ctuIdx++ )
  {
    const int32_t ctuRsAddr = pcSlice->getCtuAddrInSlice( ctuIdx );

    // update CABAC state
    const uint32_t ctuXPosInCtus        = ctuRsAddr % widthInCtus;
    const uint32_t ctuYPosInCtus        = ctuRsAddr / widthInCtus;

    const Position pos (ctuXPosInCtus * pcv.maxCUWidth, ctuYPosInCtus * pcv.maxCUHeight);
    const UnitArea ctuArea( cs.area.chromaFormat, Area( pos.x, pos.y, pcv.maxCUWidth, pcv.maxCUHeight ) );
    DTRACE_UPDATE( g_trace_ctx, std::make_pair( "ctu", ctuRsAddr ) );

    if( pCfg->getSwitchPOC() != pcPic->poc || -1 == pCfg->getDebugCTU() )
    {
      if ((cs.slice->getSliceType() != I_SLICE || cs.sps->getIBCFlag()) && cs.pps->ctuIsTileColBd(ctuXPosInCtus))
      {
        cs.motionLut.lut.resize(0);
        cs.motionLut.lutIbc.resize(0);
      }
    }

    const SubPic &curSubPic = pcSlice->getPPS()->getSubPicFromPos(pos);
    // padding/restore at slice level
    if (pcSlice->getPPS()->getNumSubPics() >= 2 && curSubPic.getTreatedAsPicFlag() && ctuIdx == 0)
    {
      int subPicX = (int)curSubPic.getSubPicLeft();
      int subPicY = (int)curSubPic.getSubPicTop();
      int subPicWidth = (int)curSubPic.getSubPicWidthInLumaSample();
      int subPicHeight = (int)curSubPic.getSubPicHeightInLumaSample();

      for (int rlist = REF_PIC_LIST_0; rlist < NUM_REF_PIC_LIST_01; rlist++)
      {
        int n = pcSlice->getNumRefIdx((RefPicList)rlist);
        for (int idx = 0; idx < n; idx++)
        {
          Picture *refPic = pcSlice->getRefPic((RefPicList)rlist, idx);

          if( !refPic->getSubPicSaved() && refPic->subPictures.size() > 1 )
          {
            refPic->saveSubPicBorder(refPic->getPOC(), subPicX, subPicY, subPicWidth, subPicHeight);
            refPic->extendSubPicBorder(refPic->getPOC(), subPicX, subPicY, subPicWidth, subPicHeight);
            refPic->setSubPicSaved(true);
          }
        }
      }
    }
    if (cs.pps->ctuIsTileColBd( ctuXPosInCtus ) && cs.pps->ctuIsTileRowBd( ctuYPosInCtus ))
    {
      pCABACWriter->initCtxModels( *pcSlice );
      cs.resetPrevPLT(cs.prevPLT);
      prevQP.fill(pcSlice->getSliceQp());
    }
    else if (cs.pps->ctuIsTileColBd( ctuXPosInCtus ) && pEncLib->getEntropyCodingSyncEnabledFlag())
    {
      // reset and then update contexts to the state at the end of the top CTU (if within current slice and tile).
      pCABACWriter->initCtxModels( *pcSlice );
      cs.resetPrevPLT(cs.prevPLT);
      if (cs.getCURestricted(pos.offset(0, -1), pos, pcSlice->getIndependentSliceIdx(), cs.pps->getTileIdx(pos),
                             ChannelType::LUMA))
      {
        // Top is available, we use it.
        pCABACWriter->getCtx() = pEncLib->m_entropyCodingSyncContextState;
        pCABACWriter->getCtx().riceStatReset(
          pcSlice->getSPS()->getBitDepth(ChannelType::LUMA),
          pcSlice->getSPS()->getSpsRangeExtension().getPersistentRiceAdaptationEnabledFlag());
        cs.setPrevPLT(pEncLib->m_palettePredictorSyncState);
      }
      prevQP.fill(pcSlice->getSliceQp());
    }


#if RDOQ_CHROMA_LAMBDA && ENABLE_QPA && !ENABLE_QPA_SUB_CTU
    double oldLambdaArray[MAX_NUM_COMPONENT] = {0.0};
#endif
    const double oldLambda = pRdCost->getLambda();
    if ( pCfg->getUseRateCtrl() )
    {
      int estQP        = pcSlice->getSliceQp();
      double estLambda = -1.0;
      double bpp       = -1.0;

      if( ( pcPic->slices[0]->isIRAP() && pCfg->getForceIntraQP() ) || !pCfg->getLCULevelRC() )
      {
        estQP = pcSlice->getSliceQp();
      }
      else
      {
        bpp = pRateCtrl->getRCPic()->getLCUTargetBpp(pcSlice->isIRAP());
        if ( pcPic->slices[0]->isIntra())
        {
          estLambda = pRateCtrl->getRCPic()->getLCUEstLambdaAndQP(bpp, pcSlice->getSliceQp(), &estQP);
        }
        else
        {
          estLambda = pRateCtrl->getRCPic()->getLCUEstLambda( bpp );
          estQP     = pRateCtrl->getRCPic()->getLCUEstQP    ( estLambda, pcSlice->getSliceQp() );
        }

        estQP = Clip3(-pcSlice->getSPS()->getQpBDOffset(ChannelType::LUMA), MAX_QP, estQP);

        pRdCost->setLambda(estLambda, pcSlice->getSPS()->getBitDepths());
#if WCG_EXT
        pRdCost->saveUnadjustedLambda();
#endif
        for (uint32_t compIdx = 1; compIdx < MAX_NUM_COMPONENT; compIdx++)
        {
          const ComponentID compID = ComponentID(compIdx);
          int chromaQPOffset = pcSlice->getPPS()->getQpOffset(compID) + pcSlice->getSliceChromaQpDelta(compID);
          int qpc = pcSlice->getSPS()->getMappedChromaQpValue(compID, estQP) + chromaQPOffset;
          double tmpWeight = pow(2.0, (estQP - qpc) / 3.0);  // takes into account of the chroma qp mapping and chroma qp Offset
          if (m_pcCfg->getDepQuantEnabledFlag())
          {
            tmpWeight *= (m_pcCfg->getGOPSize() >= 8 ? pow(2.0, 0.1 / 3.0) : pow(2.0, 0.2 / 3.0));  // increase chroma weight for dependent quantization (in order to reduce bit rate shift from chroma to luma)
          }
          m_pcRdCost->setDistortionWeight(compID, tmpWeight);
        }
#if RDOQ_CHROMA_LAMBDA
        const double lambdaArray[MAX_NUM_COMPONENT] = {estLambda / m_pcRdCost->getDistortionWeight (COMPONENT_Y),
                                                       estLambda / m_pcRdCost->getDistortionWeight (COMPONENT_Cb),
                                                       estLambda / m_pcRdCost->getDistortionWeight (COMPONENT_Cr)};
        pTrQuant->setLambdas( lambdaArray );
#else
        pTrQuant->setLambda( estLambda );
#endif
      }

      pRateCtrl->setRCQP( estQP );
    }
#if ENABLE_QPA
    else if (pCfg->getUsePerceptQPA() && pcSlice->getPPS()->getUseDQP())
    {
#if ENABLE_QPA_SUB_CTU
      const int adaptedQP    = applyQPAdaptationSubCtu (cs, ctuArea, ctuRsAddr, m_pcCfg->getLumaLevelToDeltaQPMapping().mode == LUMALVL_TO_DQP_NUM_MODES);
#else
      const int adaptedQP    = pcPic->m_iOffsetCtu[ctuRsAddr];
#endif
      const double newLambda = pcSlice->getLambdas()[0] * pow (2.0, double (adaptedQP - iQPIndex) / 3.0);
      pcPic->m_uEnerHpCtu[ctuRsAddr] = newLambda; // for ALF and SAO
#if !ENABLE_QPA_SUB_CTU
#if RDOQ_CHROMA_LAMBDA
      pTrQuant->getLambdas (oldLambdaArray); // save the old lambdas
      const double lambdaArray[MAX_NUM_COMPONENT] = {newLambda / m_pcRdCost->getDistortionWeight (COMPONENT_Y),
                                                     newLambda / m_pcRdCost->getDistortionWeight (COMPONENT_Cb),
                                                     newLambda / m_pcRdCost->getDistortionWeight (COMPONENT_Cr)};
      pTrQuant->setLambdas (lambdaArray);
#else
      pTrQuant->setLambda (newLambda);
#endif
      pRdCost->setLambda (newLambda, pcSlice->getSPS()->getBitDepths());
#endif
      currQP.fill(adaptedQP);
    }
#endif
    if (m_pcCfg->getDPF() && m_pcLib->getEncType() == ENC_FULL && !pcPic->slices[0]->isIntra())
    {
      setCTULambdaQpByWeight(ctuIdx, pTrQuant, pRdCost, pcSlice);
    }

    bool updateBcwCodingOrder = cs.slice->getSliceType() == B_SLICE && ctuIdx == 0;
    if( updateBcwCodingOrder )
    {
      resetBcwCodingOrder(false, cs);
      m_pcInterSearch->initWeightIdxBits();
    }
    if (pcSlice->getSPS()->getUseLmcs())
    {
      m_pcCuEncoder->setDecCuReshaperInEncCU(m_pcLib->getReshaper(), pcSlice->getSPS()->getChromaFormatIdc());
    }
    if( !cs.slice->isIntra() && pCfg->getMCTSEncConstraint() )
    {
      pcPic->mctsInfo.init( &cs, ctuRsAddr );
    }

    if (pCfg->getSwitchPOC() != pcPic->poc || ctuRsAddr >= pCfg->getDebugCTU())
    {
      m_pcCuEncoder->compressCtu(cs, ctuArea, ctuRsAddr, prevQP, currQP);
#if GREEN_METADATA_SEI_ENABLED
      FeatureCounterStruct m_featureCounter = pcPic->getFeatureCounter();
      countFeatures(m_featureCounter, cs,ctuArea);
      pcPic->setFeatureCounter(m_featureCounter);
#endif
    }
#if K0149_BLOCK_STATISTICS
    getAndStoreBlockStatistics(cs, ctuArea);
#endif

    pCABACWriter->resetBits();
    pCABACWriter->coding_tree_unit( cs, ctuArea, prevQP, ctuRsAddr, true, true );
    const int numberOfWrittenBits = int( pCABACWriter->getEstFracBits() >> SCALE_BITS );

    pcSlice->setSliceBits( ( uint32_t ) ( pcSlice->getSliceBits() + numberOfWrittenBits ) );

    // Store probabilities of first CTU in line into buffer - used only if wavefront-parallel-processing is enabled.
    if( cs.pps->ctuIsTileColBd( ctuXPosInCtus ) && pEncLib->getEntropyCodingSyncEnabledFlag() )
    {
      pEncLib->m_entropyCodingSyncContextState = pCABACWriter->getCtx();
      cs.storePrevPLT(pEncLib->m_palettePredictorSyncState);
    }

    if (m_pcCfg->getDPF() && m_pcLib->getEncType() == ENC_FULL && !pcPic->slices[0]->isIntra())
    {
      pRdCost->setLambda(oldLambda, pcSlice->getSPS()->getBitDepths());
    }

    int actualBits = int(cs.fracBits >> SCALE_BITS);
    actualBits    -= (int)m_uiPicTotalBits;
    if ( pCfg->getUseRateCtrl() )
    {
      int actualQP        = g_RCInvalidQPValue;
      double actualLambda = pRdCost->getLambda();
      int numberOfEffectivePixels    = 0;

      int numberOfSkipPixel = 0;
      for (auto &cu: cs.traverseCUs(ctuArea, ChannelType::LUMA))
      {
        numberOfSkipPixel += cu.skip*cu.lumaSize().area();
      }

      for (auto &cu: cs.traverseCUs(ctuArea, ChannelType::LUMA))
      {
        if( !cu.skip || cu.rootCbf )
        {
          numberOfEffectivePixels += cu.lumaSize().area();
          break;
        }
      }
      double skipRatio = (double)numberOfSkipPixel / ctuArea.lumaSize().area();
      CodingUnit *cu        = cs.getCU(ctuArea.lumaPos(), ChannelType::LUMA);

      if ( numberOfEffectivePixels == 0 )
      {
        actualQP = g_RCInvalidQPValue;
      }
      else
      {
        actualQP = cu->qp;
      }
      pRdCost->setLambda(oldLambda, pcSlice->getSPS()->getBitDepths());
      int estQP        = pcSlice->getSliceQp();
      for (uint32_t compIdx = 1; compIdx < MAX_NUM_COMPONENT; compIdx++)
      {
        const ComponentID compID = ComponentID(compIdx);
        int chromaQPOffset = pcSlice->getPPS()->getQpOffset(compID) + pcSlice->getSliceChromaQpDelta(compID);
        int qpc = pcSlice->getSPS()->getMappedChromaQpValue(compID, estQP) + chromaQPOffset;
        double tmpWeight = pow(2.0, (estQP - qpc) / 3.0);  // takes into account of the chroma qp mapping and chroma qp Offset
        if (m_pcCfg->getDepQuantEnabledFlag())
        {
          tmpWeight *= (m_pcCfg->getGOPSize() >= 8 ? pow(2.0, 0.1 / 3.0) : pow(2.0, 0.2 / 3.0));  // increase chroma weight for dependent quantization (in order to reduce bit rate shift from chroma to luma)
        }
        m_pcRdCost->setDistortionWeight(compID, tmpWeight);
      }
      pRateCtrl->getRCPic()->updateAfterCTU(pRateCtrl->getRCPic()->getLCUCoded(), actualBits, actualQP, actualLambda, skipRatio,
        pcSlice->isIRAP() ? 0 : pCfg->getLCULevelRC());
    }
#if ENABLE_QPA && !ENABLE_QPA_SUB_CTU
    else if (pCfg->getUsePerceptQPA() && pcSlice->getPPS()->getUseDQP())
    {
#if RDOQ_CHROMA_LAMBDA
      pTrQuant->setLambdas (oldLambdaArray);
#else
      pTrQuant->setLambda (oldLambda);
#endif
      pRdCost->setLambda (oldLambda, pcSlice->getSPS()->getBitDepths());
    }
#endif

    m_uiPicTotalBits += actualBits;
    m_uiPicDist       = cs.dist;
    // for last Ctu in the slice
    if (pcSlice->getPPS()->getNumSubPics() >= 2 && curSubPic.getTreatedAsPicFlag() && ctuIdx == (pcSlice->getNumCtuInSlice() - 1))
    {
      int subPicX = (int)curSubPic.getSubPicLeft();
      int subPicY = (int)curSubPic.getSubPicTop();
      int subPicWidth = (int)curSubPic.getSubPicWidthInLumaSample();
      int subPicHeight = (int)curSubPic.getSubPicHeightInLumaSample();

      for (int rlist = REF_PIC_LIST_0; rlist < NUM_REF_PIC_LIST_01; rlist++)
      {
        int n = pcSlice->getNumRefIdx((RefPicList)rlist);
        for (int idx = 0; idx < n; idx++)
        {
          Picture *refPic = pcSlice->getRefPic((RefPicList)rlist, idx);
          if (refPic->getSubPicSaved())
          {
            refPic->restoreSubPicBorder(refPic->getPOC(), subPicX, subPicY, subPicWidth, subPicHeight);
            refPic->setSubPicSaved(false);
          }
        }
      }
    }
    if (m_pcCfg->getDPF() && m_pcLib->getEncType() == ENC_PRE)
    {
      // merge clipped pred buffer
      const int width = pcPic->getOrigBuf().Y().width;
      const int height = pcPic->getOrigBuf().Y().height;
      const int clipWidth = std::min((int)pcv.maxCUWidth, width - pos.x);
      const int clipHeight = std::min((int)pcv.maxCUHeight, height - pos.y);
      const UnitArea clipArea(cs.area.chromaFormat, Area(pos.x, pos.y, clipWidth, clipHeight));
      PelUnitBuf dstBuf = m_pre.subBuf(clipArea);
      CPelUnitBuf srcBuf = pcPic->getPredBuf(clipArea);
      dstBuf.copyFrom(srcBuf, true);
    }
  }
}

void EncSlice::encodeSlice   ( Picture* pcPic, OutputBitstream* pcSubstreams, uint32_t &numBinsCoded )
{

  Slice *const pcSlice                 = pcPic->slices[getSliceSegmentIdx()];
  const bool wavefrontsEnabled         = pcSlice->getSPS()->getEntropyCodingSyncEnabledFlag();
  const bool entryPointsPresentFlag    = pcSlice->getSPS()->getEntryPointsPresentFlag();
  uint32_t substreamSize               = 0;
  pcSlice->resetNumberOfSubstream();


  // setup coding structure
  CodingStructure& cs = *pcPic->cs;
  cs.slice            = pcSlice;
  // initialise entropy coder for the slice
  m_CABACWriter->initCtxModels( *pcSlice );

  DTRACE( g_trace_ctx, D_HEADER, "=========== POC: %d ===========\n", pcSlice->getPOC() );

  pcPic->m_prevQP.fill(pcSlice->getSliceQp());

  const PreCalcValues& pcv = *cs.pcv;
  const uint32_t widthInCtus   = pcv.widthInCtus;
  uint32_t uiSubStrm = 0;

  // for every CTU in the slice...
  for( uint32_t ctuIdx = 0; ctuIdx < pcSlice->getNumCtuInSlice(); ctuIdx++ )
  {
    const uint32_t ctuRsAddr = pcSlice->getCtuAddrInSlice( ctuIdx );
    const uint32_t ctuXPosInCtus        = ctuRsAddr % widthInCtus;
    const uint32_t ctuYPosInCtus        = ctuRsAddr / widthInCtus;

    DTRACE_UPDATE( g_trace_ctx, std::make_pair( "ctu", ctuRsAddr ) );

    const Position pos (ctuXPosInCtus * pcv.maxCUWidth, ctuYPosInCtus * pcv.maxCUHeight);
    const UnitArea ctuArea (cs.area.chromaFormat, Area(pos.x, pos.y, pcv.maxCUWidth, pcv.maxCUHeight));
    m_CABACWriter->initBitstream( &pcSubstreams[uiSubStrm] );

    // set up CABAC contexts' state for this CTU
    if ( cs.pps->ctuIsTileColBd( ctuXPosInCtus ) && cs.pps->ctuIsTileRowBd( ctuYPosInCtus ) )
    {
      if (ctuIdx != 0) // if it is the first CTU, then the entropy coder has already been reset
      {
        numBinsCoded += m_CABACWriter->getNumBins();
        m_CABACWriter->initCtxModels( *pcSlice );
        cs.resetPrevPLT(cs.prevPLT);
      }
    }
    else if (cs.pps->ctuIsTileColBd( ctuXPosInCtus ) && wavefrontsEnabled)
    {
      // Synchronize cabac probabilities with upper CTU if it's available and at the start of a line.
      if (ctuIdx != 0) // if it is the first CTU, then the entropy coder has already been reset
      {
        numBinsCoded += m_CABACWriter->getNumBins();
        m_CABACWriter->initCtxModels( *pcSlice );
        cs.resetPrevPLT(cs.prevPLT);
      }
      if (cs.getCURestricted(pos.offset(0, -1), pos, pcSlice->getIndependentSliceIdx(), cs.pps->getTileIdx(pos),
                             ChannelType::LUMA))
      {
        // Top is available, so use it.
        m_CABACWriter->getCtx() = m_entropyCodingSyncContextState;
        m_CABACWriter->getCtx().riceStatReset(
          pcSlice->getSPS()->getBitDepth(ChannelType::LUMA),
          pcSlice->getSPS()->getSpsRangeExtension().getPersistentRiceAdaptationEnabledFlag());
        cs.setPrevPLT(m_palettePredictorSyncState);
      }
    }

    bool updateBcwCodingOrder = cs.slice->getSliceType() == B_SLICE && ctuIdx == 0;
    if( updateBcwCodingOrder )
    {
      resetBcwCodingOrder(false, cs);
    }

    m_CABACWriter->coding_tree_unit( cs, ctuArea, pcPic->m_prevQP, ctuRsAddr );

    // store probabilities of first CTU in line into buffer
    if( cs.pps->ctuIsTileColBd( ctuXPosInCtus ) && wavefrontsEnabled )
    {
      m_entropyCodingSyncContextState = m_CABACWriter->getCtx();
      cs.storePrevPLT(m_palettePredictorSyncState);
    }

    // terminate the sub-stream, if required (end of slice-segment, end of tile, end of wavefront-CTU-row):
    bool isLastCTUsinSlice = ctuIdx == pcSlice->getNumCtuInSlice()-1;
    bool isLastCTUinTile  = !isLastCTUsinSlice && cs.pps->getTileIdx( ctuRsAddr ) != cs.pps->getTileIdx( pcSlice->getCtuAddrInSlice( ctuIdx + 1 ) );
    bool isLastCTUinWPP    = !isLastCTUsinSlice && !isLastCTUinTile && wavefrontsEnabled && cs.pps->ctuIsTileColBd( pcSlice->getCtuAddrInSlice( ctuIdx + 1 ) % cs.pps->getPicWidthInCtu() );
    if (isLastCTUsinSlice || isLastCTUinTile || isLastCTUinWPP )         // this the the last CTU of the slice, tile, or WPP
    {
      m_CABACWriter->end_of_slice();  // end_of_slice_one_bit, end_of_tile_one_bit, or end_of_subset_one_bit

      // Byte-alignment in slice_data() when new tile
      pcSubstreams[uiSubStrm].writeByteAlignment();

      if (!isLastCTUsinSlice) //Byte alignment only when it is not the last substream in the slice
      {
        // write sub-stream size
        substreamSize += (pcSubstreams[uiSubStrm].getNumberOfWrittenBits() >> 3) + pcSubstreams[uiSubStrm].countStartCodeEmulations();
        pcSlice->increaseNumberOfSubstream();
        if( entryPointsPresentFlag )
        {
          pcSlice->addSubstreamSize(substreamSize);
          substreamSize = 0;
        }
      }
      uiSubStrm++;
    }
  } // CTU-loop


  if(pcSlice->getPPS()->getCabacInitPresentFlag())
  {
    m_encCABACTableIdx = m_CABACWriter->getCtxInitId( *pcSlice );
  }
  else
  {
    m_encCABACTableIdx = pcSlice->getSliceType();
  }
  numBinsCoded += m_CABACWriter->getNumBins();
}


double EncSlice::xGetQPValueAccordingToLambda ( double lambda )
{
  return 4.2005*log(lambda) + 13.7122;
}

void EncSlice::setCTULambdaQpByWeight(uint32_t ctuIdx, TrQuant* pTrQuant, RdCost* pRdCost, Slice* pcSlice)
{
  const double lambdaCTU = m_lambda * m_lambdaWeight[ctuIdx];
  const int    estQP = (int)(4.3281 * log(lambdaCTU) + 2.1829 + 0.499999);  // fitted QP-lambda relationship
  m_qpCtu = estQP;
  pRdCost->setLambda(lambdaCTU, pcSlice->getSPS()->getBitDepths());
#if WCG_EXT
  pRdCost->saveUnadjustedLambda();
#endif
  for (uint32_t compIdx = 1; compIdx < MAX_NUM_COMPONENT; compIdx++)
  {
    const ComponentID compID = ComponentID(compIdx);
    int    chromaQPOffset = pcSlice->getPPS()->getQpOffset(compID) + pcSlice->getSliceChromaQpDelta(compID);
    int    qpc = pcSlice->getSPS()->getMappedChromaQpValue(compID, estQP) + chromaQPOffset;
    double tmpWeight =
      pow(2.0, (estQP - qpc) / 3.0);   // takes into account of the chroma qp mapping and chroma qp Offset
    if (m_pcCfg->getDepQuantEnabledFlag())
    {
      tmpWeight *= (m_pcCfg->getGOPSize() >= 8 ? pow(2.0, 0.1 / 3.0) : pow(2.0, 0.2 / 3.0));   // increase chroma weight for dependent quantization (in order to reduce bit rate shift from chroma to luma)
    }
    m_pcRdCost->setDistortionWeight(compID, tmpWeight);
  }
#if RDOQ_CHROMA_LAMBDA
  const double lambdaArray[MAX_NUM_COMPONENT] = { lambdaCTU / m_pcRdCost->getDistortionWeight(COMPONENT_Y),
                                                  lambdaCTU / m_pcRdCost->getDistortionWeight(COMPONENT_Cb),
                                                  lambdaCTU / m_pcRdCost->getDistortionWeight(COMPONENT_Cr) };
  pTrQuant->setLambdas(lambdaArray);
#else
  pTrQuant->setLambda(estLambda);
#endif
}

void EncSlice::setLambdaWeightByDPF(Picture* pcPic)
{
  if (pcPic->slices[0]->getSliceType() == I_SLICE)
  {
    return;
  }

  const int curPOC = pcPic->getPOC();
  const int gopSize = m_pcCfg->getGOPSize();
  const int rPOC = curPOC % gopSize;
  const int numPropa = rPOC == 0 ? m_pcCfg->getDPFKeyLen() : m_pcCfg->getDPFNonkeyLen();

  const int width = pcPic->getOrigBuf().Y().width;
  const int height = pcPic->getOrigBuf().Y().height;
  const int sizeBlk = BLK_32;
  const int sizeCu = m_pcCfg->getCTUSize();
  const int numBlkWidth = (width + sizeBlk - 1) / sizeBlk;
  const int numBlkHeight = (height + sizeBlk - 1) / sizeBlk;
  const int numCuWidth = (width + sizeCu - 1) / sizeCu;
  const int numCuHeight = (height + sizeCu - 1) / sizeCu;
  const int numBlkPic = numBlkWidth * numBlkHeight;
  const int numCuPic = numCuWidth * numCuHeight;
  m_lambdaWeight.assign(numCuPic, 1.0);

  if (numPropa == 0)
  {
    return;
  }

  // fix QP for pre-encoding
  Slice* pcSlice = pcPic->slices[getSliceSegmentIdx()];
  double oldQP = m_pcCfg->getQPForPicture(0, pcSlice);
  int    qp;
  double lambda = calculateLambda(pcSlice, 0, oldQP, oldQP, qp);
  pcSlice->setSliceQp(qp);
  setUpLambda(pcSlice, lambda, qp);

  const auto chromaFormatIdc = m_pcCfg->getChromaFormatIdc();
  const auto area = Area(0, 0, width, height);
  m_pre.create(chromaFormatIdc, area, 0);

  m_pcLib->setEncType(ENC_PRE);
  compressSlice(pcPic, true, m_pcCfg->getFastDeltaQp());  // pre-encoding
  m_pcLib->setEncType(ENC_FULL);
  pcSlice->setSliceQp(m_qpCtu);
  setUpLambda(pcSlice, m_lambda, m_qpCtu);

  CodingStructure& cs = *pcPic->cs;
  const PreCalcValues& pcv = *cs.pcv;
  for (uint32_t yPos = 0; yPos < pcv.lumaHeight; yPos += pcv.maxCUHeight)
  {
    for (uint32_t xPos = 0; xPos < pcv.lumaWidth; xPos += pcv.maxCUWidth)
    {
      const CodingUnit* cu = cs.getCU(Position(xPos, yPos), ChannelType::LUMA);
      if (cu->slice->getLmcsEnabledFlag())
      {
        const uint32_t width = (xPos + pcv.maxCUWidth > pcv.lumaWidth) ? (pcv.lumaWidth - xPos) : pcv.maxCUWidth;
        const uint32_t height = (yPos + pcv.maxCUHeight > pcv.lumaHeight) ? (pcv.lumaHeight - yPos) : pcv.maxCUHeight;
        const UnitArea area(cs.area.chromaFormat, Area(xPos, yPos, width, height));
        cs.getRecoBuf(area).get(COMPONENT_Y).rspSignal(m_pcLib->getReshaper()->getInvLUT());
      }
    }
  }

  const CPelUnitBuf& org = pcPic->getTrueOrigBuf();
  const CPelUnitBuf& rec = pcPic->getRecoBuf();

  const CPelBuf& orgY = org.get(COMPONENT_Y);
  const CPelBuf& recY = rec.get(COMPONENT_Y);
  const CPelBuf& preY = m_pre.get(COMPONENT_Y);

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      Intermediate_Int err = *orgY.bufAt(x, y) - *preY.bufAt(x, y);
      Intermediate_Int dis = *orgY.bufAt(x, y) - *recY.bufAt(x, y);
      m_pixelPredErr[y][x] = err * err;
      m_pixelRecDis[y][x] = dis * dis;
    }
  }

  m_factorBlk.assign(numBlkPic, 0);
  int     pHeightX_Blk = 0;
  int     pWidthY_Blk = 0;

  for (int bIdx = 0; bIdx < numBlkPic; bIdx++)
  {
    int line = bIdx / numBlkWidth;
    int sumPixelRecDisBlk = 0;   // block coding distortion
    int sumPixelPredErrBlk = 0;  // block MCP error
    pHeightX_Blk = line;
    pWidthY_Blk = bIdx - line * numBlkWidth;
    for (int i = pHeightX_Blk * sizeBlk; i < std::min(pHeightX_Blk * sizeBlk + sizeBlk, height); i++)
    {
      for (int j = pWidthY_Blk * sizeBlk; j < std::min(pWidthY_Blk * sizeBlk + sizeBlk, width); j++)
      {
        sumPixelRecDisBlk += m_pixelRecDis[i][j];
        sumPixelPredErrBlk += m_pixelPredErr[i][j];
      }
    }
    if (sumPixelPredErrBlk < 1)
    {
      sumPixelPredErrBlk = 1;
    }
    double ki = (double)sumPixelRecDisBlk / (double)sumPixelPredErrBlk;  // block DPF
    if (ki > 1.0)
    {
      ki = 1.0;
    }
    double    kiLenth = 0;  // block DPF with distortion propagation length
    double    multiKi = 1;
    for (int i = 0; i < numPropa; i++)
    {
      multiKi = multiKi * ki;
      kiLenth = kiLenth + multiKi;
    }
    m_factorBlk[bIdx] = 1.0 / (1.0 + kiLenth);  // block weight
  }

  double sumFactorBlk = 0;
  for (int bIdx = 0; bIdx < numBlkPic; bIdx++)
  {
    sumFactorBlk = sumFactorBlk + m_factorBlk[bIdx];
  }
  double aveFactorBlk = sumFactorBlk / (double)numBlkPic;
  for (int bIdx = 0; bIdx < numBlkPic; bIdx++)
  {
    m_factorBlk[bIdx] = m_factorBlk[bIdx] / aveFactorBlk;  // block weight with dividing by mean normalization
  }
    
  const int numBlkCu = sizeCu / sizeBlk;
  for (int idxCu = 0; idxCu < numCuPic; idxCu++)
  {
    double sumFact = 0;
    int    k = 0;
    for (int i = 0; i < numBlkPic; i++)
    {
      if (idxCu == numCuWidth * (i / (numBlkWidth * numBlkCu)) + (i % numBlkWidth) / numBlkCu)
      {
        sumFact += m_factorBlk[i];
        k++;
      }
    }
    double tWeight = sumFact / (double)k;
    m_lambdaWeight[idxCu] = tWeight;  // CTU weight
  }

  m_factorBlk.clear();
  m_pre.destroy();
}

//! \}
