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

/** \file     EncLib.cpp
    \brief    encoder class
*/
#include "EncLib.h"

#include "EncModeCtrl.h"
#include "AQp.h"
#include "EncCu.h"

#include "CommonLib/Picture.h"
#include "CommonLib/CommonDef.h"
#include "CommonLib/ChromaFormat.h"
#include "EncLibCommon.h"
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
#include "CommonLib/SEIPackedRegionsInfoProcess.h"
#endif
#include "CommonLib/ProfileTierLevel.h"

//! \ingroup EncoderLib
//! \{

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

EncLib::EncLib(EncLibCommon *encLibCommon)
  : m_cListPic(encLibCommon->getPictureBuffer())
#if JVET_AJ0151_DSC_SEI && JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  , m_cGOPEncoder(encLibCommon->getDscSubstreamManager())
#endif
  , m_spsMap(encLibCommon->getSpsMap())
  , m_ppsMap(encLibCommon->getPpsMap())
  , m_apsMaps(encLibCommon->getApsMaps())
  , m_AUWriterIf(nullptr)
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  , m_cacheModel()
#endif
  , m_lmcsAPS(nullptr)
  , m_scalinglistAPS(nullptr)
  , m_doPlt(true)
  , m_vps(encLibCommon->getVPS())
  , m_layerDecPicBuffering(encLibCommon->getDecPicBuffering())
{
  m_pocLast          = -1;
  m_receivedPicCount = 0;
  m_codedPicCount    = 0;

  m_maxRefPicNum = 0;

#if ENABLE_SIMD_OPT_BUFFER
  g_pelBufOP.initPelBufOpsX86();
#endif

#if JVET_O0756_CALCULATE_HDRMETRICS
  m_metricTime = std::chrono::milliseconds(0);
#endif

  memset(m_apss, 0, sizeof(m_apss));

  m_layerId = NOT_VALID;
  m_picIdInGOP = NOT_VALID;
  m_gopRprPpsId = 0;

  m_encType = ENC_FULL;
}

EncLib::~EncLib()
{
}

void EncLib::create( const int layerId )
{
  m_layerId = layerId;
  m_pocLast = m_compositeRefEnabled ? -2 : -1;
  // create processing unit classes
  m_cGOPEncoder.        create( );
  m_cCuEncoder.         create( this );
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  m_cInterSearch.cacheAssign( &m_cacheModel );
#endif

  m_deblockingFilter.create(floorLog2(m_maxCUWidth) - MIN_CU_LOG2);

  if (!m_deblockingFilterDisable && m_encDbOpt)
  {
    m_deblockingFilter.initEncPicYuvBuffer(m_chromaFormatIdc, Size(getSourceWidth(), getSourceHeight()),
                                           getMaxCUWidth());
  }

  if (m_lmcsEnabled)
  {
    m_cReshaper.createEnc(getSourceWidth(), getSourceHeight(), m_maxCUWidth, m_maxCUHeight,
                          m_bitDepth[ChannelType::LUMA]);
  }
  if (m_rcEnableRateControl)
  {
    Fraction frameRate = m_frameRate;
    frameRate.den *= m_temporalSubsampleRatio;
    m_cRateCtrl.init(m_framesToBeEncoded, m_rcTargetBitrate, frameRate, m_gopSize, m_intraPeriod, m_sourceWidth,
                     m_sourceHeight, m_maxCUWidth, m_maxCUHeight, getBitDepth(ChannelType::LUMA),
                     m_rcKeepHierarchicalBit, m_rcUseCtuSeparateModel, m_GOPList);
  }

}

void EncLib::destroy ()
{
  // destroy processing unit classes
  m_cGOPEncoder.        destroy();
  m_cSliceEncoder.      destroy();
  m_cCuEncoder.         destroy();
  if( m_alf )
  {
    m_cEncALF.destroy();
  }
  m_cEncSAO.            destroyEncData();
  m_cEncSAO.            destroy();
  m_deblockingFilter.   destroy();
  m_cRateCtrl.          destroy();
  m_cReshaper.          destroy();
  m_cInterSearch.       destroy();
  m_cIntraSearch.destroy();
}

void EncLib::init(AUWriterIf *auWriterIf)
{
  m_AUWriterIf = auWriterIf;

  SPS &sps0 = *(m_spsMap.allocatePS( m_vps->getGeneralLayerIdx( m_layerId ) )); // NOTE: implementations that use more than 1 SPS need to be aware of activation issues.
  PPS &pps0 = *( m_ppsMap.allocatePS( m_vps->getGeneralLayerIdx( m_layerId ) ) );

  if (getAvoidIntraInDepLayer() && getNumRefLayers(m_vps->getGeneralLayerIdx( getLayerId())) > 0)
  {
    setIDRRefParamListPresent(true);
  }
  // initialize SPS
  xInitSPS( sps0 );

  for( int i = 0; i < MAX_TLAYER; i++ )
  {
    m_layerDecPicBuffering[m_layerId * MAX_TLAYER + i] = m_maxDecPicBuffering[i];
  }

  xInitVPS( sps0 );
  xInitOPI(m_opi);
  xInitDCI(m_dci, sps0);

  if (getUseCompositeRef() || getDependentRAPIndicationSEIEnabled())
  {
    sps0.setLongTermRefsPresent(true);
  }

  if (m_rcCpbSaturationEnabled)
  {
    m_cRateCtrl.initHrdParam(sps0.getGeneralHrdParameters(), sps0.getOlsHrdParameters(), m_frameRate,
                             m_rcInitialCpbFullness);
  }
  m_cRdCost.setCostMode ( m_costMode );

  // initialize PPS
  pps0.setPicWidthInLumaSamples( m_sourceWidth );
  pps0.setPicHeightInLumaSamples( m_sourceHeight );
  if (pps0.getPicWidthInLumaSamples() == sps0.getMaxPicWidthInLumaSamples() && pps0.getPicHeightInLumaSamples() == sps0.getMaxPicHeightInLumaSamples())
  {
    pps0.setConformanceWindow( sps0.getConformanceWindow() );
    pps0.setConformanceWindowFlag( false );
  }
  else
  {
    pps0.setConformanceWindow( m_conformanceWindow );
    pps0.setConformanceWindowFlag(!m_conformanceWindow.isZero());
  }
  if (m_explicitScalingWindowEnabled)
  {
    pps0.setExplicitScalingWindowFlag(true);
    pps0.setScalingWindow(m_scalingWindow);
  }
  if (!pps0.getExplicitScalingWindowFlag())
  {
    pps0.setScalingWindow(pps0.getConformanceWindow());
  }
  xInitPPS(pps0, sps0);
  // initialize APS
  xInitRPL(sps0);

  if (m_resChangeInClvsEnabled)
  {
    PPS& pps = *(m_ppsMap.allocatePS(ENC_PPS_ID_RPR + m_layerId));
    const Window& inputScalingWindow = pps0.getScalingWindow();
    int scaledWidth = int( ( pps0.getPicWidthInLumaSamples() - SPS::getWinUnitX( sps0.getChromaFormatIdc() ) * ( inputScalingWindow.getWindowLeftOffset() + inputScalingWindow.getWindowRightOffset() ) ) / m_scalingRatioHor );
    int minSizeUnit = std::max(8, 1 << sps0.getLog2MinCodingBlockSize());
    int temp = scaledWidth / minSizeUnit;
    int width = ( scaledWidth - ( temp * minSizeUnit) > 0 ? temp + 1 : temp ) * minSizeUnit;

    int scaledHeight = int( ( pps0.getPicHeightInLumaSamples() - SPS::getWinUnitY( sps0.getChromaFormatIdc() ) * ( inputScalingWindow.getWindowTopOffset() + inputScalingWindow.getWindowBottomOffset() ) ) / m_scalingRatioVer );
    temp = scaledHeight / minSizeUnit;
    int height = ( scaledHeight - ( temp * minSizeUnit) > 0 ? temp + 1 : temp ) * minSizeUnit;

    pps.setPicWidthInLumaSamples( width );
    pps.setPicHeightInLumaSamples( height );
    pps.setSliceChromaQpFlag(true);
    Window conformanceWindow(0, (width - scaledWidth) / SPS::getWinUnitX(sps0.getChromaFormatIdc()), 0,
                             (height - scaledHeight) / SPS::getWinUnitY(sps0.getChromaFormatIdc()));
    if (pps.getPicWidthInLumaSamples() == sps0.getMaxPicWidthInLumaSamples() && pps.getPicHeightInLumaSamples() == sps0.getMaxPicHeightInLumaSamples())
    {
      pps.setConformanceWindow( sps0.getConformanceWindow() );
      pps.setConformanceWindowFlag( false );
    }
    else
    {
      pps.setConformanceWindow( conformanceWindow );
      pps.setConformanceWindowFlag(!pps.getConformanceWindow().isZero());
    }

    Window scalingWindow(0, (width - scaledWidth) / SPS::getWinUnitX(sps0.getChromaFormatIdc()), 0,
                         (height - scaledHeight) / SPS::getWinUnitY(sps0.getChromaFormatIdc()));
    pps.setScalingWindow( scalingWindow );
    pps.setExplicitScalingWindowFlag(!scalingWindow.isZero());

    //register the width/height of the current pic into reference SPS
    if (!sps0.getPPSValidFlag(pps.getPPSId()))
    {
      sps0.setPPSValidFlag(pps.getPPSId(), true);
      sps0.setScalingWindowSizeInPPS(pps.getPPSId(), scaledWidth, scaledHeight);
    }
    int curSeqMaxPicWidthY = sps0.getMaxPicWidthInLumaSamples();    // sps_pic_width_max_in_luma_samples
    int curSeqMaxPicHeightY = sps0.getMaxPicHeightInLumaSamples();  // sps_pic_height_max_in_luma_samples
    int curPicWidthY = width;                                       // pps_pic_width_in_luma_samples
    int curPicHeightY = height;                                     // pps_pic_height_in_luma_samples
    int max8MinCbSizeY = std::max((int)8, (1 << sps0.getLog2MinCodingBlockSize())); // Max(8, MinCbSizeY)
    //Warning message of potential scaling window size violation
    for (int i = 0; i < MAX_NUM_PPS; i++)
    {
      if (sps0.getPPSValidFlag(i))
      {
        if ((scaledWidth * curSeqMaxPicWidthY) < sps0.getScalingWindowSizeInPPS(i).width * (curPicWidthY - max8MinCbSizeY))
        {
          printf("Potential violation: (curScaledWIdth * curSeqMaxPicWidthY) should be greater than or equal to refScaledWidth * (curPicWidthY - max(8, MinCbSizeY)\n");
        }
        if ((scaledHeight * curSeqMaxPicHeightY) < sps0.getScalingWindowSizeInPPS(i).height * (curPicHeightY - max8MinCbSizeY))
        {
          printf("Potential violation: (curScaledHeight * curSeqMaxPicHeightY) should be greater than or equal to refScaledHeight * (curPicHeightY - max(8, MinCbSizeY)\n");
        }
      }
    }

    // disable picture partitioning for scaled RPR pictures (slice/tile config only provided for the original resolution)
    m_noPicPartitionFlag = true;

    xInitPPS( pps, sps0 ); // will allocate memory for and initialize pps.pcv inside

    if( pps.getWrapAroundEnabledFlag() )
    {
      const int minCbSizeY = 1 << sps0.getLog2MinCodingBlockSize();
      pps.setPicWidthMinusWrapAroundOffset      ((pps.getPicWidthInLumaSamples()/minCbSizeY) - (m_wrapAroundOffset * pps.getPicWidthInLumaSamples() / pps0.getPicWidthInLumaSamples() / minCbSizeY) );
      pps.setWrapAroundOffset                   (minCbSizeY * (pps.getPicWidthInLumaSamples() / minCbSizeY - pps.getPicWidthMinusWrapAroundOffset()));

    }
    else
    {
      pps.setPicWidthMinusWrapAroundOffset      (0);
      pps.setWrapAroundOffset                   ( 0 );
    }
  }
  if (m_resChangeInClvsEnabled && ((m_gopBasedRPREnabledFlag && (m_iQP >= getGOPBasedRPRQPThreshold())) || m_rprFunctionalityTestingEnabledFlag))
  {
    PPS& pps = *(m_ppsMap.allocatePS(ENC_PPS_ID_RPR2 + m_layerId));
    Window& inputScalingWindow = pps0.getScalingWindow();
    int scaledWidth = int((pps0.getPicWidthInLumaSamples() - SPS::getWinUnitX(sps0.getChromaFormatIdc()) * (inputScalingWindow.getWindowLeftOffset() + inputScalingWindow.getWindowRightOffset())) / m_scalingRatioHor2);
    int minSizeUnit = std::max(8, 1 << sps0.getLog2MinCodingBlockSize());
    int temp = scaledWidth / minSizeUnit;
    int width = (scaledWidth - (temp * minSizeUnit) > 0 ? temp + 1 : temp) * minSizeUnit;

    int scaledHeight = int((pps0.getPicHeightInLumaSamples() - SPS::getWinUnitY(sps0.getChromaFormatIdc()) * (inputScalingWindow.getWindowTopOffset() + inputScalingWindow.getWindowBottomOffset())) / m_scalingRatioVer2);
    temp = scaledHeight / minSizeUnit;
    int height = (scaledHeight - (temp * minSizeUnit) > 0 ? temp + 1 : temp) * minSizeUnit;

    pps.setPicWidthInLumaSamples(width);
    pps.setPicHeightInLumaSamples(height);
    pps.setSliceChromaQpFlag(true);

    Window conformanceWindow(0, (width - scaledWidth) / SPS::getWinUnitX(sps0.getChromaFormatIdc()), 0,
                             (height - scaledHeight) / SPS::getWinUnitY(sps0.getChromaFormatIdc()));
    if (pps.getPicWidthInLumaSamples() == sps0.getMaxPicWidthInLumaSamples() && pps.getPicHeightInLumaSamples() == sps0.getMaxPicHeightInLumaSamples())
    {
      pps.setConformanceWindow(sps0.getConformanceWindow());
      pps.setConformanceWindowFlag(false);
    }
    else
    {
      pps.setConformanceWindow(conformanceWindow);
      pps.setConformanceWindowFlag(!pps.getConformanceWindow().isZero());
    }

    Window scalingWindow(0, (width - scaledWidth) / SPS::getWinUnitX(sps0.getChromaFormatIdc()), 0,
                         (height - scaledHeight) / SPS::getWinUnitY(sps0.getChromaFormatIdc()));
    pps.setScalingWindow(scalingWindow);

    //register the width/height of the current pic into reference SPS
    if (!sps0.getPPSValidFlag(pps.getPPSId()))
    {
      sps0.setPPSValidFlag(pps.getPPSId(), true);
      sps0.setScalingWindowSizeInPPS(pps.getPPSId(), scaledWidth, scaledHeight);
    }
    int curSeqMaxPicWidthY = sps0.getMaxPicWidthInLumaSamples();    // sps_pic_width_max_in_luma_samples
    int curSeqMaxPicHeightY = sps0.getMaxPicHeightInLumaSamples();  // sps_pic_height_max_in_luma_samples
    int curPicWidthY = width;                                       // pps_pic_width_in_luma_samples
    int curPicHeightY = height;                                     // pps_pic_height_in_luma_samples
    int max8MinCbSizeY = std::max((int)8, (1 << sps0.getLog2MinCodingBlockSize())); // Max(8, MinCbSizeY)
                                                                                    //Warning message of potential scaling window size violation
    for (int i = 0; i < MAX_NUM_PPS; i++)
    {
      if (sps0.getPPSValidFlag(i))
      {
        if ((scaledWidth * curSeqMaxPicWidthY) < sps0.getScalingWindowSizeInPPS(i).width * (curPicWidthY - max8MinCbSizeY))
          printf("Potential violation: (curScaledWIdth * curSeqMaxPicWidthY) should be greater than or equal to refScaledWidth * (curPicWidthY - max(8, MinCbSizeY)\n");
        if ((scaledHeight * curSeqMaxPicHeightY) < sps0.getScalingWindowSizeInPPS(i).height * (curPicHeightY - max8MinCbSizeY))
          printf("Potential violation: (curScaledHeight * curSeqMaxPicHeightY) should be greater than or equal to refScaledHeight * (curPicHeightY - max(8, MinCbSizeY)\n");
      }
    }

    // disable picture partitioning for scaled RPR pictures (slice/tile config only provided for the original resolution)
    m_noPicPartitionFlag = true;

    xInitPPS(pps, sps0); // will allocate memory for and initialize pps.pcv inside

    if (pps.getWrapAroundEnabledFlag())
    {
      int minCbSizeY = (1 << sps0.getLog2MinCodingBlockSize());
      pps.setPicWidthMinusWrapAroundOffset((pps.getPicWidthInLumaSamples() / minCbSizeY) - (m_wrapAroundOffset * pps.getPicWidthInLumaSamples() / pps0.getPicWidthInLumaSamples() / minCbSizeY));
      pps.setWrapAroundOffset(minCbSizeY * (pps.getPicWidthInLumaSamples() / minCbSizeY - pps.getPicWidthMinusWrapAroundOffset()));

    }
    else
    {
      pps.setPicWidthMinusWrapAroundOffset(0);
      pps.setWrapAroundOffset(0);
    }
  }
  if (m_resChangeInClvsEnabled && ((m_gopBasedRPREnabledFlag && (m_iQP >= getGOPBasedRPRQPThreshold())) || m_rprFunctionalityTestingEnabledFlag))
  {
    PPS& pps = *(m_ppsMap.allocatePS(ENC_PPS_ID_RPR3 + m_layerId));
    Window& inputScalingWindow = pps0.getScalingWindow();
    int scaledWidth = int((pps0.getPicWidthInLumaSamples() - SPS::getWinUnitX(sps0.getChromaFormatIdc()) * (inputScalingWindow.getWindowLeftOffset() + inputScalingWindow.getWindowRightOffset())) / m_scalingRatioHor3);
    int minSizeUnit = std::max(8, 1 << sps0.getLog2MinCodingBlockSize());
    int temp = scaledWidth / minSizeUnit;
    int width = (scaledWidth - (temp * minSizeUnit) > 0 ? temp + 1 : temp) * minSizeUnit;

    int scaledHeight = int((pps0.getPicHeightInLumaSamples() - SPS::getWinUnitY(sps0.getChromaFormatIdc()) * (inputScalingWindow.getWindowTopOffset() + inputScalingWindow.getWindowBottomOffset())) / m_scalingRatioVer3);
    temp = scaledHeight / minSizeUnit;
    int height = (scaledHeight - (temp * minSizeUnit) > 0 ? temp + 1 : temp) * minSizeUnit;

    pps.setPicWidthInLumaSamples(width);
    pps.setPicHeightInLumaSamples(height);
    pps.setSliceChromaQpFlag(true);

    Window conformanceWindow(0, (width - scaledWidth) / SPS::getWinUnitX(sps0.getChromaFormatIdc()), 0,
                             (height - scaledHeight) / SPS::getWinUnitY(sps0.getChromaFormatIdc()));
    if (pps.getPicWidthInLumaSamples() == sps0.getMaxPicWidthInLumaSamples() && pps.getPicHeightInLumaSamples() == sps0.getMaxPicHeightInLumaSamples())
    {
      pps.setConformanceWindow(sps0.getConformanceWindow());
      pps.setConformanceWindowFlag(false);
    }
    else
    {
      pps.setConformanceWindow(conformanceWindow);
      pps.setConformanceWindowFlag(!pps.getConformanceWindow().isZero());
    }

    Window scalingWindow(0, (width - scaledWidth) / SPS::getWinUnitX(sps0.getChromaFormatIdc()), 0,
                         (height - scaledHeight) / SPS::getWinUnitY(sps0.getChromaFormatIdc()));
    pps.setScalingWindow(scalingWindow);

    //register the width/height of the current pic into reference SPS
    if (!sps0.getPPSValidFlag(pps.getPPSId()))
    {
      sps0.setPPSValidFlag(pps.getPPSId(), true);
      sps0.setScalingWindowSizeInPPS(pps.getPPSId(), scaledWidth, scaledHeight);
    }
    int curSeqMaxPicWidthY = sps0.getMaxPicWidthInLumaSamples();    // sps_pic_width_max_in_luma_samples
    int curSeqMaxPicHeightY = sps0.getMaxPicHeightInLumaSamples();  // sps_pic_height_max_in_luma_samples
    int curPicWidthY = width;                                       // pps_pic_width_in_luma_samples
    int curPicHeightY = height;                                     // pps_pic_height_in_luma_samples
    int max8MinCbSizeY = std::max((int)8, (1 << sps0.getLog2MinCodingBlockSize())); // Max(8, MinCbSizeY)
                                                                                    //Warning message of potential scaling window size violation
    for (int i = 0; i < MAX_NUM_PPS; i++)
    {
      if (sps0.getPPSValidFlag(i))
      {
        if ((scaledWidth * curSeqMaxPicWidthY) < sps0.getScalingWindowSizeInPPS(i).width * (curPicWidthY - max8MinCbSizeY))
          printf("Potential violation: (curScaledWIdth * curSeqMaxPicWidthY) should be greater than or equal to refScaledWidth * (curPicWidthY - max(8, MinCbSizeY)\n");
        if ((scaledHeight * curSeqMaxPicHeightY) < sps0.getScalingWindowSizeInPPS(i).height * (curPicHeightY - max8MinCbSizeY))
          printf("Potential violation: (curScaledHeight * curSeqMaxPicHeightY) should be greater than or equal to refScaledHeight * (curPicHeightY - max(8, MinCbSizeY)\n");
      }
    }

    // disable picture partitioning for scaled RPR pictures (slice/tile config only provided for the original resolution)
    m_noPicPartitionFlag = true;

    xInitPPS(pps, sps0); // will allocate memory for and initialize pps.pcv inside

    if (pps.getWrapAroundEnabledFlag())
    {
      const int minCbSizeY = 1 << sps0.getLog2MinCodingBlockSize();
      pps.setPicWidthMinusWrapAroundOffset((pps.getPicWidthInLumaSamples() / minCbSizeY) - (m_wrapAroundOffset * pps.getPicWidthInLumaSamples() / pps0.getPicWidthInLumaSamples() / minCbSizeY));
      pps.setWrapAroundOffset(minCbSizeY * (pps.getPicWidthInLumaSamples() / minCbSizeY - pps.getPicWidthMinusWrapAroundOffset()));

    }
    else
    {
      pps.setPicWidthMinusWrapAroundOffset(0);
      pps.setWrapAroundOffset(0);
    }
  }

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  int packedRegionsWidth = 0;
  int packedRegionsHeight = 0;
  if (m_priSEIEnabled)
  {
    CHECK(m_resChangeInClvsEnabled, "Cannot use RPR resolution change with packed regions info SEI");
    PPS& pps = *(m_ppsMap.allocatePS(ENC_PPS_ID_RPR + m_layerId));  // Reuse RPR pps for packed regions info SEI
    int minSizeUnit = std::max(8, 1 << sps0.getLog2MinCodingBlockSize());
    uint32_t priUnitSize = 1 << m_priSEILog2UnitSize;
    for (uint32_t i = 0; i <= m_priSEINumRegionsMinus1; i++)
    {
      packedRegionsWidth = std::max(packedRegionsWidth, (int)(m_priSEIRegionTopLeftInUnitsX[i] + (m_priSEIRegionWidthInUnitsMinus1[i] + 1) * priUnitSize));
      packedRegionsHeight = std::max(packedRegionsHeight, (int)(m_priSEIRegionTopLeftInUnitsY[i] + (m_priSEIRegionHeightInUnitsMinus1[i] + 1) * priUnitSize));
    }
    packedRegionsWidth = (packedRegionsWidth + (minSizeUnit - 1)) & ~(minSizeUnit - 1);
    packedRegionsHeight = (packedRegionsHeight + (minSizeUnit - 1)) & ~(minSizeUnit - 1);
    pps.setPicWidthInLumaSamples(packedRegionsWidth);
    pps.setPicHeightInLumaSamples(packedRegionsHeight);
    pps.setSliceChromaQpFlag(true);

    //register the width/height of the current pic into reference SPS
    if (!sps0.getPPSValidFlag(pps.getPPSId()))
    {
      sps0.setPPSValidFlag(pps.getPPSId(), true);
      sps0.setScalingWindowSizeInPPS(pps.getPPSId(), packedRegionsWidth, packedRegionsHeight);
    }

    // disable picture partitioning for scaled RPR pictures (slice/tile config only provided for the original resolution)
    m_noPicPartitionFlag = true;

    xInitPPS(pps, sps0); // will allocate memory for and initialize pps.pcv inside

    if (pps.getWrapAroundEnabledFlag())
    {
      const int minCbSizeY = 1 << sps0.getLog2MinCodingBlockSize();
      pps.setPicWidthMinusWrapAroundOffset((pps.getPicWidthInLumaSamples() / minCbSizeY) - (m_wrapAroundOffset * pps.getPicWidthInLumaSamples() / pps0.getPicWidthInLumaSamples() / minCbSizeY));
      pps.setWrapAroundOffset(minCbSizeY * (pps.getPicWidthInLumaSamples() / minCbSizeY - pps.getPicWidthMinusWrapAroundOffset()));

    }
    else
    {
      pps.setPicWidthMinusWrapAroundOffset(0);
      pps.setWrapAroundOffset(0);
    }
  }
#endif

#if ER_CHROMA_QP_WCG_PPS
  if (m_wcgChromaQpControl.isEnabled())
  {
    PPS &pps1=*(m_ppsMap.allocatePS(1));
    xInitPPS(pps1, sps0);
  }
#endif
  if (getUseCompositeRef())
  {
    PPS &pps2 = *(m_ppsMap.allocatePS(2));
    xInitPPS(pps2, sps0);
    xInitPPSforLT(pps2);
  }
  if( this->m_rprRASLtoolSwitch && m_wrapAround )
  {
    PPS &pps4 = *(m_ppsMap.allocatePS(4));
    pps4.setPicWidthInLumaSamples( pps0.getPicWidthInLumaSamples() );
    pps4.setPicHeightInLumaSamples( pps0.getPicHeightInLumaSamples() );
    xInitPPS(pps4, sps0);
    pps4.setWrapAroundEnabledFlag( false );
    pps4.setPicWidthMinusWrapAroundOffset( 0 );
    pps4.setWrapAroundOffset             ( 0 );
  }
  xInitPicHeader(m_picHeader, sps0, pps0);

  // initialize processing unit classes
  m_cGOPEncoder.  init( this );
  m_cSliceEncoder.init( this, sps0 );
  m_cCuEncoder.   init( this, sps0 );

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  if (m_priSEIEnabled)
  {
    SEIPackedRegionsInfo sei;
    m_cGOPEncoder.getSEIEncoder().initSEIPackedRegionsInfo(&sei);
    m_priProcess.init(sei, sps0, packedRegionsWidth, packedRegionsHeight);
  }
#endif

  // initialize transform & quantization class
  m_cTrQuant.init( nullptr,
                   1 << m_log2MaxTbSize,
                   m_useRDOQ,
                   m_useRDOQTS,
                   m_useSelectiveRDOQ,
                   true
  );

  // initialize encoder search class
  CABACWriter* cabacEstimator = m_CABACEncoder.getCABACEstimator(&sps0);
  m_cIntraSearch.init(this, &m_cTrQuant, &m_cRdCost, cabacEstimator, getCtxCache(), m_maxCUWidth, m_maxCUHeight,
                      floorLog2(m_maxCUWidth) - m_log2MinCUSize, &m_cReshaper, sps0.getBitDepth(ChannelType::LUMA));
  m_cInterSearch.init(this, &m_cTrQuant, m_searchRange, m_bipredSearchRange, m_motionEstimationSearchMethod,
                      getUseCompositeRef(), m_maxCUWidth, m_maxCUHeight, floorLog2(m_maxCUWidth) - m_log2MinCUSize,
                      &m_cRdCost, cabacEstimator, getCtxCache(), &m_cReshaper);

  // link temporary buffets from intra search with inter search to avoid unneccessary memory overhead
  m_cInterSearch.setTempBuffers( m_cIntraSearch.getSplitCSBuf(), m_cIntraSearch.getFullCSBuf(), m_cIntraSearch.getSaveCSBuf() );

  m_maxRefPicNum = 0;

  if (getUseScalingListId() != SCALING_LIST_OFF)
  {
    CHECK(!sps0.getScalingListFlag(), "sps_explicit_scaling_list_enabled_flag should be ON if ScalingList is not OFF");
    const int scalingListApsId = std::min<int>(MAX_NUM_APS(ApsType::SCALING_LIST) - 1, getVPS() == nullptr ?
      0 : getVPS()->getGeneralLayerIdx(m_layerId));
    APS* scalingListAps = m_apsMaps[ApsType::SCALING_LIST].allocatePS(scalingListApsId);
    scalingListAps->setAPSType(ApsType::SCALING_LIST);
    scalingListAps->setLayerId(m_layerId);
    xInitScalingLists(sps0, scalingListAps);
  }
  else
  {
    xInitScalingLists(sps0, nullptr);
  }
  if (getUseCompositeRef())
  {
    Picture *picBg = new Picture;
    picBg->create(sps0.getWrapAroundEnabledFlag(), sps0.getChromaFormatIdc(), Size(pps0.getPicWidthInLumaSamples(), pps0.getPicHeightInLumaSamples()),
                  sps0.getMaxCUWidth(), sps0.getMaxCUWidth() + 16, false, m_layerId,
                  getGopBasedTemporalFilterEnabled());
    picBg->getRecoBuf().fill(0);
    picBg->finalInit( m_vps, sps0, pps0, &m_picHeader, m_apss, m_lmcsAPS, m_scalinglistAPS );
    picBg->allocateNewSlice();
    picBg->createSpliceIdx(pps0.pcv->sizeInCtus);
    m_cGOPEncoder.setPicBg(picBg);
    Picture *picOrig = new Picture;
    picOrig->create(sps0.getWrapAroundEnabledFlag(), sps0.getChromaFormatIdc(), Size(pps0.getPicWidthInLumaSamples(), pps0.getPicHeightInLumaSamples()),
                    sps0.getMaxCUWidth(), sps0.getMaxCUWidth() + 16, false, m_layerId,
                    getGopBasedTemporalFilterEnabled());
    picOrig->getOrigBuf().fill(0);
    m_cGOPEncoder.setPicOrig(picOrig);
  }
}

void EncLib::xInitScalingLists( SPS &sps, APS *aps )
{
  // Initialise scaling lists
  // The encoder will only use the SPS scaling lists. The PPS will never be marked present.
  const int maxLog2TrDynamicRange[MAX_NUM_CHANNEL_TYPE] = { sps.getMaxLog2TrDynamicRange(ChannelType::LUMA),
                                                            sps.getMaxLog2TrDynamicRange(ChannelType::CHROMA) };

  Quant* quant = getTrQuant()->getQuant();

  if(getUseScalingListId() == SCALING_LIST_OFF)
  {
    quant->setFlatScalingList(maxLog2TrDynamicRange, sps.getBitDepths());
    quant->setUseScalingList(false);
  }
  else if(getUseScalingListId() == SCALING_LIST_DEFAULT)
  {
    CHECK(aps == nullptr, "aps should not be nullptr if getUseScalingListId() != SCALING_LIST_OFF");
    aps->getScalingList().setDefaultScalingList ();
    quant->setScalingList( &( aps->getScalingList() ), maxLog2TrDynamicRange, sps.getBitDepths() );
    quant->setUseScalingList(true);
  }
  else if(getUseScalingListId() == SCALING_LIST_FILE_READ)
  {
    CHECK(aps == nullptr, "aps should not be nullptr if getUseScalingListId() != SCALING_LIST_OFF");
    aps->getScalingList().setDefaultScalingList();
    CHECK( aps->getScalingList().xParseScalingList( getScalingListFileName() ), "Error Parsing Scaling List Input File" );
    aps->getScalingList().checkDcOfMatrix();
    if (!aps->getScalingList().isNotDefaultScalingList())
    {
      setUseScalingListId( SCALING_LIST_DEFAULT );
    }
    aps->getScalingList().setChromaScalingListPresentFlag(isChromaEnabled(sps.getChromaFormatIdc()));
    quant->setScalingList( &( aps->getScalingList() ), maxLog2TrDynamicRange, sps.getBitDepths() );
    quant->setUseScalingList(true);

    sps.setDisableScalingMatrixForLfnstBlks(getDisableScalingMatrixForLfnstBlks());
  }
  else
  {
    THROW("error : ScalingList == " << getUseScalingListId() << " not supported\n");
  }

  if( getUseScalingListId() == SCALING_LIST_FILE_READ )
  {
    // Prepare delta's:
    for (uint32_t scalingListId = 0; scalingListId < 28; scalingListId++)
    {
      if (aps->getScalingList().getChromaScalingListPresentFlag()||aps->getScalingList().isLumaScalingList(scalingListId))
      {
        aps->getScalingList().checkPredMode(scalingListId);
      }
    }
  }
}

void EncLib::xInitPPSforLT(PPS& pps)
{
  pps.setOutputFlagPresentFlag(true);
  pps.setDeblockingFilterControlPresentFlag(true);
  pps.setPPSDeblockingFilterDisabledFlag(true);
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

void EncLib::deletePicBuffer()
{
  PicList::iterator iterPic = m_cListPic.begin();
  int               size    = int(m_cListPic.size());

  for (int i = 0; i < size; i++)
  {
    Picture* pcPic = *(iterPic++);

    pcPic->destroy();

    // get rid of the qpadaption layer
    while( pcPic->aqlayer.size() )
    {
      delete pcPic->aqlayer.back(); pcPic->aqlayer.pop_back();
    }

    delete pcPic;
    pcPic = nullptr;
  }

  m_cListPic.clear();
}

bool EncLib::encodePrep(bool flush, PelStorage *pcPicYuvOrg, const InputColourSpaceConversion snrCSC, 
  std::list<PelUnitBuf *> &rcListPicYuvRecOut, int &numEncoded, PelStorage** ppcPicYuvRPR)
{
  if (m_compositeRefEnabled && m_cGOPEncoder.getPicBg()->getSpliceFull() && m_pocLast >= 10 && m_receivedPicCount == 0
      && m_cGOPEncoder.getEncodedLTRef() == false)
  {
    Picture *picCurr = nullptr;
    xGetNewPicBuffer( rcListPicYuvRecOut, picCurr, 2 );
    const PPS *pps = m_ppsMap.getPS( 2 );
    const SPS *sps = m_spsMap.getPS( pps->getSPSId() );

    picCurr->M_BUFS( 0, PIC_ORIGINAL ).copyFrom( m_cGOPEncoder.getPicBg()->getRecoBuf() );
    picCurr->finalInit( m_vps, *sps, *pps, &m_picHeader, m_apss, m_lmcsAPS, m_scalinglistAPS );
    picCurr->poc = m_pocLast - 1;
    m_pocLast -= 2;

    if (getShutterFilterFlag())
    {
      int blendingRatio = getBlendingRatioSII();
      picCurr->xOutputPreFilteredPic(picCurr, &m_cListPic, blendingRatio, m_intraPeriod);
      picCurr->copyToPic(sps, &picCurr->m_bufs[PIC_ORIGINAL], pcPicYuvOrg);
    }

    if( getUseAdaptiveQP() )
    {
      AQpPreanalyzer::preanalyze( picCurr );
    }
    if (m_rcEnableRateControl)
    {
      m_cRateCtrl.initRCGOP(m_receivedPicCount);
    }

    m_cGOPEncoder.compressGOP(m_pocLast, m_receivedPicCount, m_cListPic, rcListPicYuvRecOut, false, false, snrCSC,
                              m_printFrameMSE, m_printMSSSIM, true, 0);

#if JVET_O0756_CALCULATE_HDRMETRICS
    m_metricTime = m_cGOPEncoder.getMetricTime();
#endif
#if GREEN_METADATA_SEI_ENABLED
    this->setFeatureCounter(m_cGOPEncoder.getFeatureCounter());
#endif
    m_cGOPEncoder.setEncodedLTRef( true );
    if (m_rcEnableRateControl)
    {
      m_cRateCtrl.destroyRCGOP();
    }

    numEncoded         = 0;
    m_receivedPicCount = 0;
  }

  //PROF_ACCUM_AND_START_NEW_SET( getProfilerPic(), P_GOP_LEVEL );
  if (pcPicYuvOrg != nullptr)
  {
    // get original YUV
    Picture *pcPicCurr = nullptr;

    int ppsID = -1; // Use default PPS ID
#if ER_CHROMA_QP_WCG_PPS
    if( getWCGChromaQPControl().isEnabled() )
    {
      ppsID = getdQPs()[m_pocLast / (m_compositeRefEnabled ? 2 : 1) + 1];
      ppsID += (getSwitchPOC() != -1 && (m_pocLast + 1 >= getSwitchPOC()) ? 1 : 0);
    }
#endif

    if (m_resChangeInClvsEnabled && m_gopBasedRPREnabledFlag && (m_iQP >= getGOPBasedRPRQPThreshold()))
    {
      const int poc = m_pocLast + (m_compositeRefEnabled ? 2 : 1);
      double upscaledPSNR = 0.0;
      if (poc % getGOPSize() == 0)
      {
        ScalingRatio downScalingRatio{ 32768, 32768 };
        ScalingRatio upScalingRatio{ 8192, 8192 };

        const PPS* orgPPS = m_ppsMap.getPS(0 + m_layerId);
        const SPS* orgSPS = m_spsMap.getPS(orgPPS->getSPSId());
        const ChromaFormat chFormatIdc = orgSPS->getChromaFormatIdc();

        const PPS* pTempPPS = m_ppsMap.getPS(ENC_PPS_ID_RPR + m_layerId);
        Picture::rescalePicture(downScalingRatio, *pcPicYuvOrg, orgPPS->getScalingWindow(), *ppcPicYuvRPR[1], pTempPPS->getScalingWindow(), chFormatIdc, orgSPS->getBitDepths(), true, true,
          orgSPS->getHorCollocatedChromaFlag(), orgSPS->getVerCollocatedChromaFlag());
        Picture::rescalePicture(upScalingRatio, *ppcPicYuvRPR[1], orgPPS->getScalingWindow(), *ppcPicYuvRPR[0], pTempPPS->getScalingWindow(), chFormatIdc, orgSPS->getBitDepths(), true, false,
          orgSPS->getHorCollocatedChromaFlag(), orgSPS->getVerCollocatedChromaFlag());

        // Calculate PSNR
        const  Pel* pSrc0 = pcPicYuvOrg->get(COMPONENT_Y).bufAt(0, 0);
        const  Pel* pSrc1 = ppcPicYuvRPR[0]->get(COMPONENT_Y).bufAt(0, 0);

        uint64_t totalDiff = 0;
        for (int y = 0; y < pcPicYuvOrg->get(COMPONENT_Y).height; y++)
        {
          for (int x = 0; x < pcPicYuvOrg->get(COMPONENT_Y).width; x++)
          {
            int diff = pSrc0[x] - pSrc1[x];
            totalDiff += uint64_t(diff) * uint64_t(diff);
          }
          pSrc0 += pcPicYuvOrg->get(COMPONENT_Y).stride;
          pSrc1 += ppcPicYuvRPR[0]->get(COMPONENT_Y).stride;
        }

        const uint32_t maxval = 255 << (orgSPS->getBitDepth(ChannelType::LUMA) - 8);
        upscaledPSNR = totalDiff ? 10.0 * log10((double)maxval * maxval * orgPPS->getPicWidthInLumaSamples() * orgPPS->getPicHeightInLumaSamples() / (double)totalDiff) : 999.99;
      }

      if (poc % getGOPSize() == 0)
      {
        const int qpBias = 37;
        if ((m_psnrThresholdRPR - (m_iQP - qpBias) * 0.5) < upscaledPSNR)
        {
          ppsID = ENC_PPS_ID_RPR + m_layerId;
        }
        else
        {
          if ((m_psnrThresholdRPR2 - (m_iQP - qpBias) * 0.5) < upscaledPSNR)
          {
            ppsID = ENC_PPS_ID_RPR2 + m_layerId;
          }
          else
          {
            if ((m_psnrThresholdRPR3 - (m_iQP - qpBias) * 0.5) < upscaledPSNR)
            {
              ppsID = ENC_PPS_ID_RPR3 + m_layerId;
            }
            else
            {
              ppsID = m_layerId;
            }
          }
        }
        m_gopRprPpsId = ppsID;
      }
      else
      {
        ppsID = m_gopRprPpsId;
      }
    }

    if (m_resChangeInClvsEnabled && m_rprFunctionalityTestingEnabledFlag)
    {
      const int poc = m_pocLast + (m_compositeRefEnabled ? 2 : 1);
      if (poc % m_rprSwitchingSegmentSize == 0)
      {
        ppsID = m_layerId;
        bool  applyRpr = false;
        int currPoc = poc + m_frameSkip;
        int rprSegment = getRprSwitchingSegment(currPoc);
        int thePPSID = getRprSwitchingPPSID(rprSegment);
        applyRpr = thePPSID != 0;
        if (applyRpr)
        {
          ppsID = thePPSID + m_layerId;
        }
        m_gopRprPpsId = ppsID;
      }
      else
      {
        ppsID = m_gopRprPpsId;
      }
    }
    if (m_resChangeInClvsEnabled && m_intraPeriod == -1 && !m_gopBasedRPREnabledFlag && !m_rprFunctionalityTestingEnabledFlag)
    {
      const int poc = m_pocLast + (m_compositeRefEnabled ? 2 : 1);

      if (poc / m_switchPocPeriod % 2)
      {
        ppsID = ENC_PPS_ID_RPR + m_layerId;
      }
      else
      {
        ppsID = m_layerId;
      }
    }
    if ((m_vps->getMaxLayers() > 1) && !((m_gopBasedRPREnabledFlag && (m_iQP >= getGOPBasedRPRQPThreshold())) || m_rprFunctionalityTestingEnabledFlag))
    {
      ppsID = m_vps->getGeneralLayerIdx( m_layerId );
    }

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
    if (m_priSEIEnabled)
    {
      ppsID = ENC_PPS_ID_RPR + m_layerId;
    }
#endif

    xGetNewPicBuffer( rcListPicYuvRecOut, pcPicCurr, ppsID );

    const PPS *pPPS = ( ppsID < 0 ) ? m_ppsMap.getFirstPS() : m_ppsMap.getPS( ppsID );
    const SPS *pSPS = m_spsMap.getPS( pPPS->getSPSId() );

    if (m_resChangeInClvsEnabled)
    {
      pcPicCurr->M_BUFS( 0, PIC_ORIGINAL_INPUT ).getBuf( COMPONENT_Y ).copyFrom( pcPicYuvOrg->getBuf( COMPONENT_Y ) );
      pcPicCurr->M_BUFS( 0, PIC_ORIGINAL_INPUT ).getBuf( COMPONENT_Cb ).copyFrom( pcPicYuvOrg->getBuf( COMPONENT_Cb ) );
      pcPicCurr->M_BUFS( 0, PIC_ORIGINAL_INPUT ).getBuf( COMPONENT_Cr ).copyFrom( pcPicYuvOrg->getBuf( COMPONENT_Cr ) );

      const ChromaFormat chromaFormatIdc = pSPS->getChromaFormatIdc();

      const PPS* refPPS = m_ppsMap.getPS(m_layerId);
      const Window& curScalingWindow = pPPS->getScalingWindow();

      const int curPicWidth = pPPS->getPicWidthInLumaSamples()
                              - SPS::getWinUnitX(pSPS->getChromaFormatIdc())
                                  * (curScalingWindow.getWindowLeftOffset() + curScalingWindow.getWindowRightOffset());
      const int curPicHeight = pPPS->getPicHeightInLumaSamples()
                               - SPS::getWinUnitY(pSPS->getChromaFormatIdc())
                                   * (curScalingWindow.getWindowTopOffset() + curScalingWindow.getWindowBottomOffset());

      const Window& refScalingWindow = refPPS->getScalingWindow();

      const int refPicWidth = refPPS->getPicWidthInLumaSamples()
                              - SPS::getWinUnitX(pSPS->getChromaFormatIdc())
                                  * (refScalingWindow.getWindowLeftOffset() + refScalingWindow.getWindowRightOffset());
      const int refPicHeight = refPPS->getPicHeightInLumaSamples()
                               - SPS::getWinUnitY(pSPS->getChromaFormatIdc())
                                   * (refScalingWindow.getWindowTopOffset() + refScalingWindow.getWindowBottomOffset());

      const int xScale = ((refPicWidth << ScalingRatio::BITS) + (curPicWidth >> 1)) / curPicWidth;
      const int yScale = ((refPicHeight << ScalingRatio::BITS) + (curPicHeight >> 1)) / curPicHeight;

      const ScalingRatio scalingRatio = { xScale, yScale };

      Picture::rescalePicture(scalingRatio, *pcPicYuvOrg, refPPS->getScalingWindow(), pcPicCurr->getOrigBuf(),
                              pPPS->getScalingWindow(), chromaFormatIdc, pSPS->getBitDepths(), true, true,
                              pSPS->getHorCollocatedChromaFlag(), pSPS->getVerCollocatedChromaFlag());
    }
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
    else if (m_priSEIEnabled)
    {
      pcPicCurr->M_BUFS( 0, PIC_ORIGINAL_INPUT ).getBuf( COMPONENT_Y ).copyFrom( pcPicYuvOrg->getBuf( COMPONENT_Y ) );
      pcPicCurr->M_BUFS( 0, PIC_ORIGINAL_INPUT ).getBuf( COMPONENT_Cb ).copyFrom( pcPicYuvOrg->getBuf( COMPONENT_Cb ) );
      pcPicCurr->M_BUFS( 0, PIC_ORIGINAL_INPUT ).getBuf( COMPONENT_Cr ).copyFrom( pcPicYuvOrg->getBuf( COMPONENT_Cr ) );

      PelUnitBuf dst = pcPicCurr->getOrigBuf();
      m_priProcess.packRegions(*pcPicYuvOrg, m_layerId, dst, *pSPS);
    }
#endif
    else
    {
      pcPicCurr->M_BUFS( 0, PIC_ORIGINAL ).swap( *pcPicYuvOrg );
    }
    pcPicCurr->finalInit( m_vps, *pSPS, *pPPS, &m_picHeader, m_apss, m_lmcsAPS, m_scalinglistAPS );

    pcPicCurr->poc = m_pocLast;

    if (getShutterFilterFlag())
    {
      int blendingRatio = getBlendingRatioSII();
      pcPicCurr->xOutputPreFilteredPic(pcPicCurr, &m_cListPic, blendingRatio, m_intraPeriod);
      pcPicCurr->copyToPic(pSPS, &pcPicCurr->m_bufs[PIC_ORIGINAL], pcPicYuvOrg);
    }

    // compute image characteristics
    if( getUseAdaptiveQP() )
    {
      AQpPreanalyzer::preanalyze( pcPicCurr );
    }
  }

  if ((m_receivedPicCount == 0)
      || (!flush && (m_pocLast != 0) && (m_receivedPicCount != m_gopSize) && (m_gopSize != 0)))
  {
    numEncoded = 0;
    return true;
  }

  if (m_rcEnableRateControl)
  {
    m_cRateCtrl.initRCGOP(m_receivedPicCount);
  }

  m_picIdInGOP = 0;

  return false;
}

/**
 - Application has picture buffer list with size of GOP + 1
 - Picture buffer list acts like as ring buffer
 - End of the list has the latest picture
 .
 \param   flush               cause encoder to encode a partial GOP
 \param   pcPicYuvOrg         original YUV picture
 \param   pcPicYuvTrueOrg
 \param   snrCSC
 \retval  rcListPicYuvRecOut  list of reconstruction YUV pictures
 \retval  accessUnitsOut      list of output access units
 \retval  numEncoded         number of encoded pictures
 */

bool EncLib::encode(const InputColourSpaceConversion snrCSC, std::list<PelUnitBuf *> &rcListPicYuvRecOut,
                    int &numEncoded)
{
  // compress GOP
  m_cGOPEncoder.compressGOP(m_pocLast, m_receivedPicCount, m_cListPic, rcListPicYuvRecOut, false, false, snrCSC,
                            m_printFrameMSE, m_printMSSSIM, false, m_picIdInGOP);

  m_picIdInGOP++;

  // go over all pictures in a GOP excluding the first IRAP
  if (m_picIdInGOP != m_gopSize && m_pocLast != 0)
  {
    return true;
  }

#if JVET_O0756_CALCULATE_HDRMETRICS
  m_metricTime = m_cGOPEncoder.getMetricTime();
#endif

  if (m_rcEnableRateControl)
  {
    m_cRateCtrl.destroyRCGOP();
  }

  numEncoded         = m_receivedPicCount;
  m_receivedPicCount = 0;
  m_codedPicCount += numEncoded;

  return false;
}

/**------------------------------------------------
 Separate interlaced frame into two fields
 -------------------------------------------------**/
void separateFields(Pel *org, Pel *dstField, ptrdiff_t stride, uint32_t width, uint32_t height, bool isTop)
{
  if (!isTop)
  {
    org += stride;
  }
  for (int y = 0; y < height>>1; y++)
  {
    for (int x = 0; x < width; x++)
    {
      dstField[x] = org[x];
    }

    dstField += stride;
    org += stride*2;
  }
}

bool EncLib::encodePrep(bool flush, PelStorage *pcPicYuvOrg, const InputColourSpaceConversion snrCSC,
                        std::list<PelUnitBuf *> &rcListPicYuvRecOut, int &numEncoded, bool isTff)
{
  numEncoded     = 0;
  bool keepDoing = true;

  for( int fieldNum = 0; fieldNum < 2; fieldNum++ )
  {
    if( pcPicYuvOrg )
    {
      /* -- field initialization -- */
      const bool isTopField = isTff == ( fieldNum == 0 );

      Picture *pcField;
      xGetNewPicBuffer( rcListPicYuvRecOut, pcField, -1 );

      for( uint32_t comp = 0; comp < ::getNumberValidComponents( pcPicYuvOrg->chromaFormat ); comp++ )
      {
        const ComponentID compID = ComponentID( comp );
        {
          PelBuf compBuf = pcPicYuvOrg->get( compID );
          separateFields( compBuf.buf,
            pcField->getOrigBuf().get( compID ).buf,
            compBuf.stride,
            compBuf.width,
            compBuf.height,
            isTopField );
        }
      }

      int ppsID = -1; // Use default PPS ID
      const PPS *pPPS = ( ppsID < 0 ) ? m_ppsMap.getFirstPS() : m_ppsMap.getPS( ppsID );
      const SPS *pSPS = m_spsMap.getPS( pPPS->getSPSId() );

      pcField->finalInit( m_vps, *pSPS, *pPPS, &m_picHeader, m_apss, m_lmcsAPS, m_scalinglistAPS );
      pcField->poc           = m_pocLast;
      pcField->reconstructed = false;

      pcField->setBorderExtension( false );// where is this normally?

      pcField->topField = isTopField;                  // interlaced requirement

      if (getShutterFilterFlag())
      {
        int blendingRatio = getBlendingRatioSII();
        pcField->xOutputPreFilteredPic(pcField, &m_cListPic, blendingRatio, m_intraPeriod);
        pcField->copyToPic(pSPS, &pcField->m_bufs[PIC_ORIGINAL], pcPicYuvOrg);
      }

      // compute image characteristics
      if( getUseAdaptiveQP() )
      {
        AQpPreanalyzer::preanalyze( pcField );
      }
    }
  }

  if (m_receivedPicCount && (flush || m_pocLast == 1 || m_receivedPicCount == m_gopSize))
  {
    m_picIdInGOP = 0;
    keepDoing = false;
  }

  return keepDoing;
}

bool EncLib::encode(const InputColourSpaceConversion snrCSC, std::list<PelUnitBuf *> &rcListPicYuvRecOut,
                    int &numEncoded, bool isTff)
{
  numEncoded = 0;

  for( int fieldNum = 0; fieldNum < 2; fieldNum++ )
  {
    m_pocLast = m_pocLast < 2 ? fieldNum : m_pocLast;

    // compress GOP
    m_cGOPEncoder.compressGOP(m_pocLast, m_pocLast < 2 ? m_pocLast + 1 : m_receivedPicCount, m_cListPic,
                              rcListPicYuvRecOut, true, isTff, snrCSC, m_printFrameMSE, m_printMSSSIM, false,
                              m_picIdInGOP);
#if JVET_O0756_CALCULATE_HDRMETRICS
    m_metricTime = m_cGOPEncoder.getMetricTime();
#endif

    m_picIdInGOP++;
  }

  // go over all pictures in a GOP excluding first top field and first bottom field
  if (m_picIdInGOP != m_gopSize && m_pocLast > 1)
  {
    return true;
  }

  numEncoded += m_receivedPicCount;
  m_codedPicCount += m_receivedPicCount;
  m_receivedPicCount = 0;

  return false;
}

void EncLib::applyNnPostFilter()
{
  if(m_cListPic.empty())
  {
    return;
  }
  m_nnPostFiltering.filterPictures(m_cListPic);
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

/**
 - Application has picture buffer list with size of GOP + 1
 - Picture buffer list acts like as ring buffer
 - End of the list has the latest picture
 .
 \retval rpcPic obtained picture buffer
 */
void EncLib::xGetNewPicBuffer ( std::list<PelUnitBuf*>& rcListPicYuvRecOut, Picture*& rpcPic, int ppsId )
{
  // rotate the output buffer
  rcListPicYuvRecOut.push_back( rcListPicYuvRecOut.front() ); rcListPicYuvRecOut.pop_front();

  rpcPic=0;

  // At this point, the SPS and PPS can be considered activated - they are copied to the new Pic.
  const PPS *pPPS=(ppsId<0) ? m_ppsMap.getFirstPS() : m_ppsMap.getPS(ppsId);
  CHECK(pPPS == nullptr, "PPS not found");
  const PPS &pps=*pPPS;

  const SPS *pSPS=m_spsMap.getPS(pps.getSPSId());
  CHECK(pSPS == nullptr, "SPS not found");
  const SPS &sps=*pSPS;

  Slice::sortPicList(m_cListPic);

  // use an entry in the buffered list if the maximum number that need buffering has been reached:
  int maxDecPicBuffering = ( m_vps == nullptr || m_vps->m_numLayersInOls[m_vps->m_targetOlsIdx] == 1 ) ? sps.getMaxDecPicBuffering( MAX_TLAYER - 1 ) : m_vps->getMaxDecPicBuffering( MAX_TLAYER - 1 );

  if (m_cListPic.size() >= (uint32_t) (m_gopSize + maxDecPicBuffering + 2))
  {
    PicList::iterator iterPic = m_cListPic.begin();
    int               size    = int(m_cListPic.size());
    for (int i = 0; i < size; i++)
    {
      rpcPic = *iterPic;
      if( !rpcPic->referenced && rpcPic->layerId == m_layerId )
      {
        break;
      }
      else
      {
        rpcPic = nullptr;
      }
      iterPic++;
    }

    // If PPS ID is the same, we will assume that it has not changed since it was last used
    // and return the old object.
    if( rpcPic && pps.getPPSId() != rpcPic->cs->pps->getPPSId() )
    {
      // the IDs differ - free up an entry in the list, and then create a new one, as with the case where the max buffering state has not been reached.
      rpcPic->destroy();
      delete rpcPic;
      m_cListPic.erase(iterPic);
      rpcPic=0;
    }
  }

  if (rpcPic==0)
  {
    rpcPic = new Picture;
    rpcPic->create(sps.getWrapAroundEnabledFlag(), sps.getChromaFormatIdc(), Size(pps.getPicWidthInLumaSamples(), pps.getPicHeightInLumaSamples()),
      sps.getMaxCUWidth(), sps.getMaxCUWidth() + PIC_MARGIN, false, m_layerId, getShutterFilterFlag());

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
    if (m_resChangeInClvsEnabled || m_priSEIEnabled)
#else
    if (m_resChangeInClvsEnabled)
#endif
    {
      int thePPS = m_layerId;
      const PPS& pps0 = *m_ppsMap.getPS(thePPS);
      rpcPic->M_BUFS(0, PIC_ORIGINAL_INPUT).create(sps.getChromaFormatIdc(), Area(Position(), Size(pps0.getPicWidthInLumaSamples(), pps0.getPicHeightInLumaSamples())));
    }
    if ( getUseAdaptiveQP() )
    {
      const uint32_t maxDqpLayer = m_picHeader.getCuQpDeltaSubdivIntra() / 2 + 1;
      rpcPic->aqlayer.resize(maxDqpLayer);
      for (uint32_t d = 0; d < maxDqpLayer; d++)
      {
        rpcPic->aqlayer[d] = new AQpLayer( pps.getPicWidthInLumaSamples(), pps.getPicHeightInLumaSamples(), sps.getMaxCUWidth() >> d, sps.getMaxCUHeight() >> d );
      }
    }

    m_cListPic.push_back( rpcPic );
  }

  rpcPic->setBorderExtension( false );
  rpcPic->reconstructed = false;
  rpcPic->referenced = true;
  rpcPic->getHashMap()->clearAll();

  m_pocLast += (m_compositeRefEnabled ? 2 : 1);
  m_receivedPicCount++;
}

void EncLib::xInitVPS( const SPS& sps )
{
  // The SPS must have already been set up.
  // set the VPS profile information.

  m_vps->m_olsHrdParams.clear();
  m_vps->m_olsHrdParams.resize(m_vps->getNumOlsTimingHrdParamsMinus1(), std::vector<OlsHrdParams>(m_vps->getMaxSubLayers()));
  m_vps->setMaxTidIlRefPicsPlus1(m_cfgVPSParameters.m_maxTidILRefPicsPlus1);
  m_vps->deriveOutputLayerSets();
  m_vps->deriveTargetOutputLayerSet( m_vps->m_targetOlsIdx );


  // number of the DPB parameters is set equal to the number of OLS containing multi layers
  if( !m_vps->getEachLayerIsAnOlsFlag() )
  {
    m_vps->m_numDpbParams = m_vps->getNumMultiLayeredOlss();
  }

  if( m_vps->m_dpbParameters.size() != m_vps->m_numDpbParams )
  {
    m_vps->m_dpbParameters.resize( m_vps->m_numDpbParams );
  }

  if( m_vps->m_dpbMaxTemporalId.size() != m_vps->m_numDpbParams )
  {
    m_vps->m_dpbMaxTemporalId.resize( m_vps->m_numDpbParams );
  }

  for( int olsIdx = 0, dpbIdx = 0; olsIdx < m_vps->m_numOutputLayersInOls.size(); olsIdx++ )
  {
    if ( m_vps->getNumLayersInOls(olsIdx) > 1 )
    {
      if( std::find( m_vps->m_layerIdInOls[olsIdx].begin(), m_vps->m_layerIdInOls[olsIdx].end(), m_layerId ) != m_vps->m_layerIdInOls[olsIdx].end() )
      {
        m_vps->setOlsDpbPicWidth( olsIdx, std::max<int>( sps.getMaxPicWidthInLumaSamples(), m_vps->getOlsDpbPicSize( olsIdx ).width ) );
        m_vps->setOlsDpbPicHeight( olsIdx, std::max<int>( sps.getMaxPicHeightInLumaSamples(), m_vps->getOlsDpbPicSize( olsIdx ).height ) );
        m_vps->setOlsDpbChromaFormatIdc(olsIdx,
                                        std::max(sps.getChromaFormatIdc(), m_vps->getOlsDpbChromaFormatIdc(olsIdx)));
        m_vps->setOlsDpbBitDepthMinus8(
          olsIdx, std::max<int>(sps.getBitDepth(ChannelType::LUMA) - 8, m_vps->getOlsDpbBitDepthMinus8(olsIdx)));
      }

      m_vps->setOlsDpbParamsIdx( olsIdx, dpbIdx );
      dpbIdx++;
    }
  }

  for( int i = 0; i < m_vps->m_numOutputLayersInOls.size(); i++ )
  {
    if ( m_vps->getNumLayersInOls(i) > 1 )
    {
      const int dpbIdx = m_vps->getOlsDpbParamsIdx(i);
      ProfileTierLevelFeatures profileTierLevelFeatures;
      if (m_vps->getProfileTierLevel(i).getLevelIdc() != Level::NONE 
        && m_vps->getProfileTierLevel(i).getProfileIdc() != Profile::NONE)
      {
        profileTierLevelFeatures.extractPTLInformation( m_vps->getProfileTierLevel(i) );
      }
      else
      {
        profileTierLevelFeatures.extractPTLInformation( sps );
      }

      if( m_vps->getMaxSubLayers() == 1 )
      {
        // When vps_max_sublayers_minus1 is equal to 0, the value of vps_dpb_max_tid[ dpbIdx ] is inferred to be equal to 0.
        m_vps->m_dpbMaxTemporalId[dpbIdx] = 0;
      }
      else
      {
        if( m_vps->getDefaultPtlDpbHrdMaxTidFlag() )
        {
          // When vps_max_sublayers_minus1 is greater than 0 and vps_all_layers_same_num_sublayers_flag is equal to 1, the value of vps_dpb_max_tid[ dpbIdx ] is inferred to be equal to vps_max_sublayers_minus1.
          m_vps->m_dpbMaxTemporalId[dpbIdx] = m_vps->getMaxSubLayers() - 1;
        }
        else
        {
          m_vps->m_dpbMaxTemporalId[dpbIdx] = m_vps->getMaxSubLayers() - 1;
        }
      }

      int decPicBuffering[MAX_TLAYER] = { 0 };

      for( int lIdx = 0; lIdx < m_vps->getNumLayersInOls( i ); lIdx++ )
      {
        for( int tId = 0; tId < MAX_TLAYER; tId++ )
        {
          decPicBuffering[tId] += m_layerDecPicBuffering[m_vps->getLayerIdInOls( i, lIdx ) * MAX_TLAYER + tId];
        }
      }

      for( int j = ( m_vps->m_sublayerDpbParamsPresentFlag ? 0 : m_vps->m_dpbMaxTemporalId[dpbIdx] ); j <= m_vps->m_dpbMaxTemporalId[dpbIdx]; j++ )
      {
        m_vps->m_dpbParameters[dpbIdx].maxDecPicBuffering[j] = decPicBuffering[j] > 0 ? decPicBuffering[j] : profileTierLevelFeatures.getMaxDpbSize( m_vps->getOlsDpbPicSize( i ).width * m_vps->getOlsDpbPicSize( i ).height );
        m_vps->m_dpbParameters[dpbIdx].maxNumReorderPics[j] = m_vps->m_dpbParameters[dpbIdx].maxDecPicBuffering[j] - 1;
        m_vps->m_dpbParameters[dpbIdx].maxLatencyIncreasePlus1[j] = 0;

        CHECK( m_vps->m_dpbParameters[dpbIdx].maxDecPicBuffering[j] > profileTierLevelFeatures.getMaxDpbSize( m_vps->getOlsDpbPicSize( i ).width * m_vps->getOlsDpbPicSize( i ).height ), "DPB size is not sufficient" );
      }

      for( int j = ( m_vps->m_sublayerDpbParamsPresentFlag ? m_vps->m_dpbMaxTemporalId[dpbIdx] : 0 ); j < m_vps->m_dpbMaxTemporalId[dpbIdx]; j++ )
      {
        // When dpb_max_dec_pic_buffering_minus1[ dpbIdx ] is not present for dpbIdx in the range of 0 to maxSubLayersMinus1 - 1, inclusive, due to subLayerInfoFlag being equal to 0, it is inferred to be equal to dpb_max_dec_pic_buffering_minus1[ maxSubLayersMinus1 ].
        m_vps->m_dpbParameters[dpbIdx].maxDecPicBuffering[j] = m_vps->m_dpbParameters[dpbIdx].maxDecPicBuffering[m_vps->m_dpbMaxTemporalId[dpbIdx]];

        // When dpb_max_num_reorder_pics[ dpbIdx ] is not present for dpbIdx in the range of 0 to maxSubLayersMinus1 - 1, inclusive, due to subLayerInfoFlag being equal to 0, it is inferred to be equal to dpb_max_num_reorder_pics[ maxSubLayersMinus1 ].
        m_vps->m_dpbParameters[dpbIdx].maxNumReorderPics[j] = m_vps->m_dpbParameters[dpbIdx].maxNumReorderPics[m_vps->m_dpbMaxTemporalId[dpbIdx]];

        // When dpb_max_latency_increase_plus1[ dpbIdx ] is not present for dpbIdx in the range of 0 to maxSubLayersMinus1 - 1, inclusive, due to subLayerInfoFlag being equal to 0, it is inferred to be equal to dpb_max_latency_increase_plus1[ maxSubLayersMinus1 ].
        m_vps->m_dpbParameters[dpbIdx].maxLatencyIncreasePlus1[j] = m_vps->m_dpbParameters[dpbIdx].maxLatencyIncreasePlus1[m_vps->m_dpbMaxTemporalId[dpbIdx]];
      }
    }
  }
  for (int i = 0; i < m_vps->getNumOutputLayerSets(); i++)
  {
    m_vps->setHrdMaxTid(i, m_vps->getMaxSubLayers() - 1);
  }

  m_vps->checkVPS();
}

void EncLib::xInitOPI(OPI& opi)
{
  if (m_OPIEnabled && m_vps)
  {
    if (!opi.getOlsInfoPresentFlag())
    {
      opi.setOpiOlsIdx(m_vps->deriveTargetOLSIdx());
      opi.setOlsInfoPresentFlag(true);
    }
    if (!opi.getHtidInfoPresentFlag())
    {
      opi.setOpiHtidPlus1(m_vps->getMaxTidinTOls(opi.getOpiOlsIdx()) + 1);
      opi.setHtidInfoPresentFlag(true);
    }
  }
}

void EncLib::xInitDCI(DCI& dci, const SPS& sps)
{
  dci.setMaxSubLayersMinus1(sps.getMaxTLayers() - 1);
  std::vector<ProfileTierLevel> ptls;
  ptls.push_back(*sps.getProfileTierLevel());
  dci.setProfileTierLevel(ptls);
}

void EncLib::xInitSPS( SPS& sps )
{
  ProfileTierLevel* profileTierLevel = sps.getProfileTierLevel();
  ConstraintInfo* cinfo = profileTierLevel->getConstraintInfo();

  cinfo->setGciPresentFlag(m_gciPresentFlag);
  cinfo->setNoRprConstraintFlag(m_noRprConstraintFlag);
  cinfo->setNoResChangeInClvsConstraintFlag(m_noResChangeInClvsConstraintFlag);
  cinfo->setOneTilePerPicConstraintFlag(m_oneTilePerPicConstraintFlag);
  cinfo->setPicHeaderInSliceHeaderConstraintFlag(m_picHeaderInSliceHeaderConstraintFlag);
  cinfo->setOneSlicePerPicConstraintFlag(m_oneSlicePerPicConstraintFlag);
  cinfo->setNoIdrRplConstraintFlag(m_noIdrRplConstraintFlag);
  cinfo->setNoRectSliceConstraintFlag(m_noRectSliceConstraintFlag);
  cinfo->setOneSlicePerSubpicConstraintFlag(m_oneSlicePerSubpicConstraintFlag);
  cinfo->setNoSubpicInfoConstraintFlag(m_noSubpicInfoConstraintFlag);
  cinfo->setOnePictureOnlyConstraintFlag(m_onePictureOnlyConstraintFlag);
  cinfo->setIntraOnlyConstraintFlag         (m_intraOnlyConstraintFlag);
  cinfo->setMaxBitDepthConstraintIdc    (m_maxBitDepthConstraintIdc);
  cinfo->setMaxChromaFormatConstraintIdc(m_maxChromaFormatConstraintIdc);
  cinfo->setAllLayersIndependentConstraintFlag (m_allLayersIndependentConstraintFlag);
  cinfo->setNoMrlConstraintFlag (m_noMrlConstraintFlag);
  cinfo->setNoIspConstraintFlag (m_noIspConstraintFlag);
  cinfo->setNoMipConstraintFlag (m_noMipConstraintFlag);
  cinfo->setNoLfnstConstraintFlag (m_noLfnstConstraintFlag);
  cinfo->setNoMmvdConstraintFlag (m_noMmvdConstraintFlag);
  cinfo->setNoSmvdConstraintFlag (m_noSmvdConstraintFlag);
  cinfo->setNoProfConstraintFlag (m_noProfConstraintFlag);
  cinfo->setNoPaletteConstraintFlag (m_noPaletteConstraintFlag);
  cinfo->setNoActConstraintFlag (m_noActConstraintFlag);
  cinfo->setNoLmcsConstraintFlag (m_noLmcsConstraintFlag);
  cinfo->setNoExplicitScaleListConstraintFlag(m_noExplicitScaleListConstraintFlag);
  cinfo->setNoVirtualBoundaryConstraintFlag(m_noVirtualBoundaryConstraintFlag);
  cinfo->setNoMttConstraintFlag(m_noMttConstraintFlag);
  cinfo->setNoChromaQpOffsetConstraintFlag(m_noChromaQpOffsetConstraintFlag);
  cinfo->setNoQtbttDualTreeIntraConstraintFlag(m_noQtbttDualTreeIntraConstraintFlag);
  cinfo->setNoPartitionConstraintsOverrideConstraintFlag(m_noPartitionConstraintsOverrideConstraintFlag);
  cinfo->setNoSaoConstraintFlag(m_noSaoConstraintFlag);
  cinfo->setNoAlfConstraintFlag(m_noAlfConstraintFlag);
  cinfo->setNoCCAlfConstraintFlag(m_noCCAlfConstraintFlag);
  cinfo->setNoWeightedPredictionConstraintFlag(m_noWeightedPredictionConstraintFlag);
  cinfo->setNoRefWraparoundConstraintFlag(m_noRefWraparoundConstraintFlag);
  cinfo->setNoTemporalMvpConstraintFlag(m_noTemporalMvpConstraintFlag);
  cinfo->setNoSbtmvpConstraintFlag(m_noSbtmvpConstraintFlag);
  cinfo->setNoAmvrConstraintFlag(m_noAmvrConstraintFlag);
  cinfo->setNoBdofConstraintFlag(m_noBdofConstraintFlag);
  cinfo->setNoDmvrConstraintFlag(m_noDmvrConstraintFlag);
  cinfo->setNoCclmConstraintFlag(m_noCclmConstraintFlag);
  cinfo->setNoMtsConstraintFlag(m_noMtsConstraintFlag);
  cinfo->setNoSbtConstraintFlag(m_noSbtConstraintFlag);
  cinfo->setNoAffineMotionConstraintFlag(m_noAffineMotionConstraintFlag);
  cinfo->setNoBcwConstraintFlag(m_noBcwConstraintFlag);
  cinfo->setNoIbcConstraintFlag(m_noIbcConstraintFlag);
  cinfo->setNoCiipConstraintFlag(m_noCiipConstraintFlag);
  cinfo->setNoGeoConstraintFlag(m_noGeoConstraintFlag);
  cinfo->setNoLadfConstraintFlag(m_noLadfConstraintFlag);
  cinfo->setNoTransformSkipConstraintFlag(m_noTransformSkipConstraintFlag);
  cinfo->setNoBDPCMConstraintFlag(m_noBDPCMConstraintFlag);
  cinfo->setNoJointCbCrConstraintFlag(m_noJointCbCrConstraintFlag);
  cinfo->setNoCuQpDeltaConstraintFlag(m_noCuQpDeltaConstraintFlag);
  cinfo->setNoDepQuantConstraintFlag(m_noDepQuantConstraintFlag);
  cinfo->setNoSignDataHidingConstraintFlag(m_noSignDataHidingConstraintFlag);
  cinfo->setNoTrailConstraintFlag(m_noTrailConstraintFlag);
  cinfo->setNoStsaConstraintFlag(m_noStsaConstraintFlag);
  cinfo->setNoRaslConstraintFlag(m_noRaslConstraintFlag);
  cinfo->setNoRadlConstraintFlag(m_noRadlConstraintFlag);
  cinfo->setNoIdrConstraintFlag(m_noIdrConstraintFlag);
  cinfo->setNoCraConstraintFlag(m_noCraConstraintFlag);
  cinfo->setNoGdrConstraintFlag(m_noGdrConstraintFlag);
  cinfo->setNoApsConstraintFlag(m_noApsConstraintFlag);
  cinfo->setAllRapPicturesFlag(m_allRapPicturesFlag);
  cinfo->setNoExtendedPrecisionProcessingConstraintFlag(m_noExtendedPrecisionProcessingConstraintFlag);
  cinfo->setNoTsResidualCodingRiceConstraintFlag(m_noTsResidualCodingRiceConstraintFlag);
  cinfo->setNoRrcRiceExtensionConstraintFlag(m_noRrcRiceExtensionConstraintFlag);
  cinfo->setNoPersistentRiceAdaptationConstraintFlag(m_noPersistentRiceAdaptationConstraintFlag);
  cinfo->setNoReverseLastSigCoeffConstraintFlag(m_noReverseLastSigCoeffConstraintFlag);

  profileTierLevel->setLevelIdc                    (m_level);
  profileTierLevel->setTierFlag                    (m_tier);
  profileTierLevel->setProfileIdc                  (m_profile);
  profileTierLevel->setFrameOnlyConstraintFlag     (m_frameOnlyConstraintFlag);
  profileTierLevel->setMultiLayerEnabledFlag       (m_multiLayerEnabledFlag);
  profileTierLevel->setNumSubProfile(m_numSubProfile);
  for (int k = 0; k < m_numSubProfile; k++)
  {
    profileTierLevel->setSubProfileIdc(k, m_subProfile[k]);
  }
  /* XXX: should Main be marked as compatible with still picture? */
  /* XXX: may be a good idea to refactor the above into a function
   * that chooses the actual compatibility based upon options */
  sps.setVPSId( m_vps->getVPSId() );

#if GDR_ENABLED
  if (m_gdrEnabled)
  {
    sps.setGDREnabledFlag(true);
  }
  else
  {
    sps.setGDREnabledFlag(false);
  }
#else
  sps.setGDREnabledFlag(false);
#endif

  sps.setMaxPicWidthInLumaSamples( m_sourceWidth );
  sps.setMaxPicHeightInLumaSamples( m_sourceHeight );
  bool scalingWindowResChanged = false;
  if (m_multiLayerEnabledFlag && m_vps->getMaxLayers() > 0)
  {
    const int minCuSize = std::max(8, 1 << m_log2MinCUSize);
    int currPicScaledWidth = m_sourceWidth - SPS::getWinUnitX(m_chromaFormatIdc) * (m_scalingWindow.getWindowLeftOffset() + m_scalingWindow.getWindowRightOffset());
    int currPicScaledHeight = m_sourceHeight - SPS::getWinUnitY( m_chromaFormatIdc) * (m_scalingWindow.getWindowTopOffset() + m_scalingWindow.getWindowBottomOffset());

    int refPicScaledWidth = currPicScaledWidth;
    int refPicScaledHeight = currPicScaledHeight;
    const int layerIdx = m_vps->getGeneralLayerIdx(m_layerId);
    if (getNumRefLayers(layerIdx) > 0)
    {
      for (int refLayerIdx = 0; refLayerIdx < layerIdx; refLayerIdx++)
      {
        if (m_vps->getDirectRefLayerFlag(layerIdx, refLayerIdx))
        {
          const PPS& refPPS = *getPPS(refLayerIdx);
          int scaledWidth = refPPS.getPicWidthInLumaSamples() - SPS::getWinUnitX(m_chromaFormatIdc) * (refPPS.getScalingWindow().getWindowLeftOffset() + refPPS.getScalingWindow().getWindowRightOffset());
          int scaledHeight = refPPS.getPicHeightInLumaSamples() - SPS::getWinUnitY( m_chromaFormatIdc) * (refPPS.getScalingWindow().getWindowTopOffset() + refPPS.getScalingWindow().getWindowBottomOffset());
          CHECK(currPicScaledWidth * 2 < scaledWidth, "CurrPicScalWinWidthL * 2 shall be greater than or equal to refPicScalWinWidthL");
          CHECK(currPicScaledHeight * 2 < scaledHeight, "CurrPicScalWinHeightL * 2 shall be greater than or equal to refPicScalWinHeightL");
          CHECK(currPicScaledWidth > scaledWidth * 8, "CurrPicScalWinWidthL shall be less than or equal to refPicScalWinWidthL * 8");
          CHECK(currPicScaledHeight > scaledHeight * 8, "CurrPicScalWinHeightL shall be less than or equal to refPicScalWinHeightL * 8");

          refPicScaledWidth = std::max(refPicScaledWidth, scaledWidth);
          refPicScaledHeight = std::max(refPicScaledHeight, scaledHeight);
        }
      }
    }

    int maxPicWidth = std::max(m_sourceWidth, ((m_sourceWidth - minCuSize) * refPicScaledWidth + currPicScaledWidth - 1) / currPicScaledWidth);
    int maxPicHeight = std::max(m_sourceHeight, ((m_sourceHeight - minCuSize) * refPicScaledHeight + currPicScaledHeight - 1) / currPicScaledHeight);
    if (maxPicWidth % minCuSize)
    {
      maxPicWidth = ((maxPicWidth / minCuSize) + 1) * minCuSize;
    }
    if (maxPicHeight % minCuSize)
    {
      maxPicHeight = ((maxPicHeight / minCuSize) + 1) * minCuSize;
    }
    if (maxPicWidth > m_sourceWidth || maxPicHeight > m_sourceHeight)
      scalingWindowResChanged = true;
    CHECK((currPicScaledWidth * maxPicWidth) < refPicScaledWidth * (m_sourceWidth - minCuSize),
          "(CurrPicScalWinWidthL * sps_pic_width_max_in_luma_samples) shall be greater than or equal to (refPicScalWinWidthL * (pps_pic_width_in_luma_samples - Max(8, MinCbSizeY)))");
    CHECK((currPicScaledHeight * maxPicHeight) < refPicScaledHeight * (m_sourceHeight - minCuSize),
          "(CurrPicScalWinHeightL * sps_pic_height_max_in_luma_samples) shall be greater than or equal to (refPicScalWinHeightL * (pps_pic_height_in_luma_samples -  Max(8, MinCbSizeY)))");
    sps.setMaxPicWidthInLumaSamples(maxPicWidth);
    sps.setMaxPicHeightInLumaSamples(maxPicHeight);
  }

  if (m_resChangeInClvsEnabled)
  {
    int maxPicWidth = std::max(m_sourceWidth, (int)((double)m_sourceWidth / m_scalingRatioHor + 0.5));
    int maxPicHeight = std::max(m_sourceHeight, (int)((double)m_sourceHeight / m_scalingRatioVer + 0.5));
    if (m_gopBasedRPREnabledFlag || m_rprFunctionalityTestingEnabledFlag)
    {
      maxPicWidth = std::max(maxPicWidth, (int)((double)m_sourceWidth / m_scalingRatioHor2 + 0.5));
      maxPicHeight = std::max(maxPicHeight, (int)((double)m_sourceHeight / m_scalingRatioVer2 + 0.5));
      maxPicWidth = std::max(maxPicWidth, (int)((double)m_sourceWidth / m_scalingRatioHor3 + 0.5));
      maxPicHeight = std::max(maxPicHeight, (int)((double)m_sourceHeight / m_scalingRatioVer3 + 0.5));
    }
    const int minCuSize = std::max(8, 1 << m_log2MinCUSize);
    if (maxPicWidth % minCuSize)
    {
      maxPicWidth += ((maxPicWidth / minCuSize) + 1) * minCuSize - maxPicWidth;
    }
    if (maxPicHeight % minCuSize)
    {
      maxPicHeight += ((maxPicHeight / minCuSize) + 1) * minCuSize - maxPicHeight;
    }
    sps.setMaxPicWidthInLumaSamples( maxPicWidth );
    sps.setMaxPicHeightInLumaSamples( maxPicHeight );
  }
  sps.setConformanceWindow( m_conformanceWindow );

  sps.setMaxCUWidth             ( m_maxCUWidth        );
  sps.setMaxCUHeight            ( m_maxCUHeight       );
  sps.setLog2MinCodingBlockSize ( m_log2MinCUSize );
  sps.setChromaFormatIdc(m_chromaFormatIdc);

  sps.setCTUSize                             ( m_CTUSize );
  sps.setSplitConsOverrideEnabledFlag        ( m_useSplitConsOverride );
  sps.setMinQTSizes(m_minQt);
  sps.setMaxMTTHierarchyDepth                ( m_uiMaxMTTHierarchyDepth, m_uiMaxMTTHierarchyDepthI, m_uiMaxMTTHierarchyDepthIChroma );
  sps.setMaxBTSize(m_maxBt[1], m_maxBt[0], m_maxBt[2]);
  sps.setMaxTTSize(m_maxTt[1], m_maxTt[0], m_maxTt[2]);
  sps.setIDRRefParamListPresent              ( m_idrRefParamList );
  sps.setUseDualITree                        ( m_dualITree );
  sps.setUseLFNST                            ( m_LFNST );
  sps.setSbTMVPEnabledFlag(m_sbTmvpEnableFlag);
  sps.setAMVREnabledFlag                ( m_ImvMode != IMV_OFF );
  sps.setBDOFEnabledFlag                    ( m_BIO );
  sps.setMaxNumMergeCand(getMaxNumMergeCand());
  sps.setMaxNumAffineMergeCand(getMaxNumAffineMergeCand());
  sps.setMaxNumIBCMergeCand(getMaxNumIBCMergeCand());
  sps.setMaxNumGeoCand(getMaxNumGeoCand());
  sps.setUseAffine             ( m_Affine );
  sps.setUseAffineType         ( m_AffineType );
  sps.setUsePROF               ( m_PROF );
  sps.setUseLMChroma           ( m_LMChroma ? true : false );
  sps.setHorCollocatedChromaFlag( m_horCollocatedChromaFlag );
  sps.setVerCollocatedChromaFlag( m_verCollocatedChromaFlag );
  sps.setMtsEnabled(m_explicitMtsIntra || m_explicitMtsInter || m_implicitMtsIntra);
  sps.setExplicitMtsIntraEnabled(m_explicitMtsIntra);
  sps.setExplicitMtsInterEnabled(m_explicitMtsInter);
  sps.setUseSBT                             ( m_SBT );
  sps.setUseSMVD                ( m_SMVD );
  sps.setUseBcw                ( m_bcw );
  sps.setLadfEnabled(m_ladfEnabled);
  if (m_ladfEnabled)
  {
    sps.setLadfNumIntervals(m_ladfNumIntervals);
    for (int k = 0; k < m_ladfNumIntervals; k++)
    {
      sps.setLadfQpOffset(m_ladfQpOffset[k], k);
      sps.setLadfIntervalLowerBound(m_ladfIntervalLowerBound[k], k);
    }
    CHECK(m_ladfIntervalLowerBound[0] != 0, "abnormal value set to LadfIntervalLowerBound[0]");
  }

  sps.setUseCiip            ( m_ciip );
  sps.setUseGeo                ( m_Geo );
  sps.setUseMMVD               ( m_MMVD );
  sps.setFpelMmvdEnabledFlag   (( m_MMVD ) ? m_allowDisFracMMVD : false);
  sps.setBdofControlPresentInPhFlag(m_BIO);
  sps.setDmvrControlPresentInPhFlag(m_DMVR);
  sps.setProfControlPresentInPhFlag(m_PROF);
  sps.setAffineAmvrEnabledFlag              ( m_AffineAmvr );
  sps.setUseDMVR                            ( m_DMVR );
  sps.setUseColorTrans(m_useColorTrans);
  sps.setPLTMode                            ( m_PLTMode);
  sps.setIBCFlag                            ( m_IBCMode != 0);
  sps.setWrapAroundEnabledFlag                      ( m_wrapAround );
  // ADD_NEW_TOOL : (encoder lib) set tool enabling flags and associated parameters here
  sps.setUseISP                             ( m_ISP );
  sps.setUseLmcs                            ( m_lmcsEnabled );
  sps.setUseMRL                ( m_MRL );
  sps.setUseMIP                ( m_MIP );
  CHECK(m_log2MinCUSize > std::min(6, floorLog2(sps.getMaxCUWidth())), "sps_log2_min_luma_coding_block_size_minus2 shall be in the range of 0 to min (4, log2_ctu_size - 2)");
  CHECK(m_uiMaxMTTHierarchyDepth > 2 * (floorLog2(sps.getCTUSize()) - sps.getLog2MinCodingBlockSize()), "sps_max_mtt_hierarchy_depth_inter_slice shall be in the range 0 to 2*(ctbLog2SizeY - log2MinCUSize)");
  CHECK(m_uiMaxMTTHierarchyDepthI > 2 * (floorLog2(sps.getCTUSize()) - sps.getLog2MinCodingBlockSize()), "sps_max_mtt_hierarchy_depth_intra_slice_luma shall be in the range 0 to 2*(ctbLog2SizeY - log2MinCUSize)");
  CHECK(m_uiMaxMTTHierarchyDepthIChroma > 2 * (floorLog2(sps.getCTUSize()) - sps.getLog2MinCodingBlockSize()), "sps_max_mtt_hierarchy_depth_intra_slice_chroma shall be in the range 0 to 2*(ctbLog2SizeY - log2MinCUSize)");

  sps.setTransformSkipEnabledFlag(m_useTransformSkip);
  sps.setLog2MaxTransformSkipBlockSize(m_log2MaxTransformSkipBlockSize);
  sps.setBDPCMEnabledFlag(m_useBDPCM);

  sps.setSPSTemporalMVPEnabledFlag((getTMVPModeId() == 2 || getTMVPModeId() == 1));

  sps.setLog2MaxTbSize   ( m_log2MaxTbSize );

  for (const auto channelType: { ChannelType::LUMA, ChannelType::CHROMA })
  {
    sps.setBitDepth(channelType, m_bitDepth[channelType]);
    sps.setQpBDOffset(channelType, (6 * (m_bitDepth[channelType] - 8)));
    sps.setInternalMinusInputBitDepth(channelType,
                                      std::max(0, (m_bitDepth[channelType] - m_inputBitDepth[channelType])));
  }

  sps.setEntropyCodingSyncEnabledFlag( m_entropyCodingSyncEnabledFlag );
  sps.setEntryPointsPresentFlag( m_entryPointPresentFlag );

  sps.setUseWP( m_useWeightedPred );
  sps.setUseWPBiPred( m_useWeightedBiPred );

  sps.setSAOEnabledFlag(m_useSao);
  sps.setJointCbCrEnabledFlag(m_jointCbCrMode);
  sps.setMaxTLayers( m_maxTempLayer );
  sps.setTemporalIdNestingFlag( ( m_maxTempLayer == 1 ) ? true : false );

  for (int i = 0; i < std::min(sps.getMaxTLayers(), (uint32_t) MAX_TLAYER); i++ )
  {
    sps.setMaxDecPicBuffering(m_maxDecPicBuffering[i], i);
    sps.setMaxNumReorderPics(m_maxNumReorderPics[i], i);
  }

  sps.setScalingListFlag ( (m_useScalingListId == SCALING_LIST_OFF) ? 0 : 1 );
  if (sps.getUseColorTrans() && sps.getScalingListFlag())
  {
    sps.setScalingMatrixForAlternativeColourSpaceDisabledFlag( m_disableScalingMatrixForAlternativeColourSpace );
  }
  else
  {
    sps.setScalingMatrixForAlternativeColourSpaceDisabledFlag( false );
  }
  if (sps.getScalingMatrixForAlternativeColourSpaceDisabledFlag())
  {
    sps.setScalingMatrixDesignatedColourSpaceFlag( m_scalingMatrixDesignatedColourSpace );
  }
  else
  {
    sps.setScalingMatrixDesignatedColourSpaceFlag( true );
  }
  sps.setALFEnabledFlag( m_alf );
  sps.setCCALFEnabledFlag( m_ccalf );
  sps.setALFOptEnabledFlag(m_encALFOpt);
  sps.setFieldSeqFlag(m_fieldSeqFlag);
  sps.setVuiParametersPresentFlag(getVuiParametersPresentFlag());

  if (sps.getVuiParametersPresentFlag())
  {
    VUI* pcVUI = sps.getVuiParameters();
    pcVUI->setAspectRatioInfoPresentFlag(getAspectRatioInfoPresentFlag());
    pcVUI->setAspectRatioConstantFlag(!getSampleAspectRatioInfoSEIEnabled());
    pcVUI->setAspectRatioIdc(getAspectRatioIdc());
    pcVUI->setSarWidth(getSarWidth());
    pcVUI->setSarHeight(getSarHeight());
    pcVUI->setColourDescriptionPresentFlag(getColourDescriptionPresentFlag());
    pcVUI->setColourPrimaries(getColourPrimaries());
    pcVUI->setTransferCharacteristics(getTransferCharacteristics());
    pcVUI->setMatrixCoefficients(getMatrixCoefficients());
    pcVUI->setProgressiveSourceFlag       (getProgressiveSourceFlag());
    pcVUI->setInterlacedSourceFlag        (getInterlacedSourceFlag());
    pcVUI->setNonPackedFlag               (getNonPackedConstraintFlag());
    pcVUI->setNonProjectedFlag            (getNonProjectedConstraintFlag());
    pcVUI->setChromaLocInfoPresentFlag(getChromaLocInfoPresentFlag());
    pcVUI->setChromaSampleLocTypeTopField(getChromaSampleLocTypeTopField());
    pcVUI->setChromaSampleLocTypeBottomField(getChromaSampleLocTypeBottomField());
    pcVUI->setChromaSampleLocType(getChromaSampleLocType());
    pcVUI->setOverscanInfoPresentFlag(getOverscanInfoPresentFlag());
    pcVUI->setOverscanAppropriateFlag(getOverscanAppropriateFlag());
    pcVUI->setVideoFullRangeFlag(getVideoFullRangeFlag());
  }

  sps.setNumLongTermRefPicSPS(NUM_LONG_TERM_REF_PIC_SPS);
  CHECK(!(NUM_LONG_TERM_REF_PIC_SPS <= MAX_NUM_LONG_TERM_REF_PICS), "Unspecified error");
  for (int k = 0; k < NUM_LONG_TERM_REF_PIC_SPS; k++)
  {
    sps.setLtRefPicPocLsbSps(k, 0);
    sps.setUsedByCurrPicLtSPSFlag(k, 0);
  }
  int numQpTables = m_chromaQpMappingTableParams.getSameCQPTableForAllChromaFlag() ? 1 : (sps.getJointCbCrEnabledFlag() ? 3 : 2);
  m_chromaQpMappingTableParams.setNumQpTables(numQpTables);
  sps.setChromaQpMappingTableFromParams(m_chromaQpMappingTableParams, sps.getQpBDOffset(ChannelType::CHROMA));
  sps.deriveChromaQPMappingTables();

  if (m_hrdParametersPresentFlag)
  {
    sps.setGeneralHrdParametersPresentFlag(true);
    xInitHrdParameters(sps);
  }
  if( getPictureTimingSEIEnabled() || getDecodingUnitInfoSEIEnabled() || getCpbSaturationEnabled() )
  {
    xInitHrdParameters(sps);
  }
  if( getBufferingPeriodSEIEnabled() || getPictureTimingSEIEnabled() || getDecodingUnitInfoSEIEnabled() )
  {
    sps.setGeneralHrdParametersPresentFlag(true);
  }

  // Set up SPS range extension settings
  sps.getSpsRangeExtension().setTransformSkipRotationEnabledFlag(m_transformSkipRotationEnabledFlag);
  sps.getSpsRangeExtension().setTransformSkipContextEnabledFlag(m_transformSkipContextEnabledFlag);
  sps.getSpsRangeExtension().setExtendedPrecisionProcessingFlag(m_extendedPrecisionProcessingFlag);
  sps.getSpsRangeExtension().setTSRCRicePresentFlag(m_tsrcRicePresentFlag);
  sps.getSpsRangeExtension().setHighPrecisionOffsetsEnabledFlag(m_highPrecisionOffsetsEnabledFlag);
  sps.getSpsRangeExtension().setRrcRiceExtensionEnableFlag(m_rrcRiceExtensionEnableFlag);
  sps.getSpsRangeExtension().setPersistentRiceAdaptationEnabledFlag(m_persistentRiceAdaptationEnabledFlag);
  sps.getSpsRangeExtension().setReverseLastSigCoeffEnabledFlag(m_reverseLastSigCoeffEnabledFlag);
  sps.getSpsRangeExtension().setCabacBypassAlignmentEnabledFlag(m_cabacBypassAlignmentEnabledFlag);

  sps.setSubPicInfoPresentFlag(m_subPicInfoPresentFlag);
  if (m_subPicInfoPresentFlag)
  {
    sps.setNumSubPics(m_numSubPics);
    sps.setSubPicSameSizeFlag(m_subPicSameSizeFlag);
    if (m_subPicSameSizeFlag)
    {
      uint32_t numSubpicCols = (m_sourceWidth + m_CTUSize - 1) / m_CTUSize / m_subPicWidth[0];
      for (unsigned int i = 0; i < m_numSubPics; i++)
      {
        sps.setSubPicCtuTopLeftX(i, (i % numSubpicCols) * m_subPicWidth[0]);
        sps.setSubPicCtuTopLeftY(i, (i / numSubpicCols) * m_subPicHeight[0]);
        sps.setSubPicWidth(i, m_subPicWidth[0]);
        sps.setSubPicHeight(i, m_subPicHeight[0]);
      }
    }
    else
    {
      sps.setSubPicCtuTopLeftX(m_subPicCtuTopLeftX);
      sps.setSubPicCtuTopLeftY(m_subPicCtuTopLeftY);
      sps.setSubPicWidth(m_subPicWidth);
      sps.setSubPicHeight(m_subPicHeight);
    }
    sps.setSubPicTreatedAsPicFlag(m_subPicTreatedAsPicFlag);
    sps.setLoopFilterAcrossSubpicEnabledFlag(m_loopFilterAcrossSubpicEnabledFlag);
    sps.setSubPicIdLen(m_subPicIdLen);
    sps.setSubPicIdMappingExplicitlySignalledFlag(m_subPicIdMappingExplicitlySignalledFlag);
    if (m_subPicIdMappingExplicitlySignalledFlag)
    {
      sps.setSubPicIdMappingPresentFlag(m_subPicIdMappingInSpsFlag);
      if (m_subPicIdMappingInSpsFlag)
      {
        sps.setSubPicId(m_subPicId);
      }
    }
  }
  else   //In that case, there is only one subpicture that contains the whole picture
  {
    sps.setNumSubPics(1);
    sps.setSubPicCtuTopLeftX(0, 0);
    sps.setSubPicCtuTopLeftY(0, 0);
    sps.setSubPicWidth(0, m_sourceWidth);
    sps.setSubPicHeight(0, m_sourceHeight);
    sps.setSubPicTreatedAsPicFlag(0, 1);
    sps.setLoopFilterAcrossSubpicEnabledFlag(0, 0);
    sps.setSubPicIdLen(0);
    sps.setSubPicIdMappingExplicitlySignalledFlag(false);
  }
  sps.setDepQuantEnabledFlag( m_DepQuantEnabledFlag );
  if (!sps.getDepQuantEnabledFlag())
  {
    sps.setSignDataHidingEnabledFlag( m_SignDataHidingEnabledFlag );
  }
  else
  {
    sps.setSignDataHidingEnabledFlag(false);
  }
  sps.setVirtualBoundariesEnabledFlag( m_virtualBoundariesEnabledFlag );
  if( sps.getVirtualBoundariesEnabledFlag() )
  {
    sps.setVirtualBoundariesPresentFlag( m_virtualBoundariesPresentFlag );
    CHECK( sps.getSubPicInfoPresentFlag() && sps.getVirtualBoundariesPresentFlag() != 1, "When subpicture signalling if present, the signalling of virtual boundaries, is present, shall be in the SPS" );
    sps.setNumVerVirtualBoundaries            ( m_numVerVirtualBoundaries );
    sps.setNumHorVirtualBoundaries            ( m_numHorVirtualBoundaries );
    for( unsigned int i = 0; i < m_numVerVirtualBoundaries; i++ )
    {
      sps.setVirtualBoundariesPosX            ( m_virtualBoundariesPosX[i], i );
    }
    for( unsigned int i = 0; i < m_numHorVirtualBoundaries; i++ )
    {
      sps.setVirtualBoundariesPosY            ( m_virtualBoundariesPosY[i], i );
    }
  }

  sps.setInterLayerPresentFlag( m_layerId > 0 && m_vps->getMaxLayers() > 1 && !m_vps->getAllIndependentLayersFlag() && !m_vps->getIndependentLayerFlag( m_vps->getGeneralLayerIdx( m_layerId ) ) );
  CHECK( m_vps->getIndependentLayerFlag( m_vps->getGeneralLayerIdx( m_layerId ) ) && sps.getInterLayerPresentFlag(), " When vps_independent_layer_flag[GeneralLayerIdx[nuh_layer_id ]]  is equal to 1, the value of inter_layer_ref_pics_present_flag shall be equal to 0." );

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  sps.setResChangeInClvsEnabledFlag(m_resChangeInClvsEnabled || m_constrainedRaslEncoding || scalingWindowResChanged || m_priSEIEnabled);
#else
  sps.setResChangeInClvsEnabledFlag(m_resChangeInClvsEnabled || m_constrainedRaslEncoding || scalingWindowResChanged);
#endif
  sps.setRprEnabledFlag(m_rprEnabledFlag || m_explicitScalingWindowEnabled || scalingWindowResChanged);
  sps.setGOPBasedRPREnabledFlag(m_gopBasedRPREnabledFlag);
  sps.setLog2ParallelMergeLevelMinus2( m_log2ParallelMergeLevelMinus2 );

  CHECK(sps.getResChangeInClvsEnabledFlag() && sps.getVirtualBoundariesPresentFlag(), "when the value of sps_res_change_in_clvs_allowed_flag is equal to 1, the value of sps_virtual_boundaries_present_flag shall be equal to 0");
}

void EncLib::xInitHrdParameters(SPS &sps)
{
  m_encHRD.initHRDParameters((EncCfg*) this);

  GeneralHrdParams *generalHrdParams = sps.getGeneralHrdParameters();
  *generalHrdParams = m_encHRD.getGeneralHrdParameters();

  OlsHrdParams *spsOlsHrdParams = sps.getOlsHrdParameters();
  for(int i = 0; i < MAX_TLAYER; i++)
  {
    *spsOlsHrdParams = m_encHRD.getOlsHrdParameters(i);
    spsOlsHrdParams++;
  }
}

void EncLib::xInitPPS(PPS &pps, const SPS &sps)
{
  // pps ID already initialised.
  pps.setSPSId(sps.getSPSId());

  pps.setNumSubPics(sps.getNumSubPics());
  pps.setSubPicIdMappingInPpsFlag(false);
  pps.setSubPicIdLen(sps.getSubPicIdLen());
  for(int picIdx=0; picIdx<pps.getNumSubPics(); picIdx++)
  {
    pps.setSubPicId(picIdx, sps.getSubPicId(picIdx));
  }
  bool useDeltaQp = getCuQpDeltaSubdiv() > 0;

  if (getMaxDeltaQP() != 0 || getUseAdaptiveQP())
  {
    useDeltaQp = true;
  }

#if SHARP_LUMA_DELTA_QP
  if ( getLumaLevelToDeltaQPMapping().isEnabled() )
  {
    useDeltaQp = true;
  }
#endif
  if (getSmoothQPReductionEnable())
  {
    useDeltaQp = true;
  }
#if ENABLE_QPA
  if (getUsePerceptQPA() && !useDeltaQp)
  {
    CHECK( m_cuQpDeltaSubdiv != 0, "max. delta-QP subdiv must be zero!" );
    useDeltaQp = (getBaseQP() < 38) && (getSourceWidth() > 512 || getSourceHeight() > 320);
  }
#endif
  if (m_bimEnabled)
  {
    useDeltaQp = true;
  }
  if (m_dpfEnabled)
  {
    useDeltaQp = true;
  }

  if (m_costMode==COST_SEQUENCE_LEVEL_LOSSLESS || m_costMode==COST_LOSSLESS_CODING)
  {
    useDeltaQp = false;
  }

  pps.setUseDQP(m_rcEnableRateControl || useDeltaQp);

  if ( m_cuChromaQpOffsetList.size() > 0 )
  {
    /* insert table entries from cfg parameters (NB, 0 should not be touched) */
    pps.clearChromaQpOffsetList();
    for (int i=0; i < m_cuChromaQpOffsetList.size(); i++)
    {
      pps.setChromaQpOffsetListEntry(i + 1, m_cuChromaQpOffsetList[i].u.comp.cbOffset,
                                     m_cuChromaQpOffsetList[i].u.comp.crOffset,
                                     m_cuChromaQpOffsetList[i].u.comp.jointCbCrOffset);
    }
  }
  else
  {
    pps.clearChromaQpOffsetList();
  }
  {
    const int refQp = getBaseQP() + getQpRefAdj();
    const int maxQp = MAX_QP;
    const int minQp = sps.getQpBDOffset(ChannelType::LUMA);

    pps.setPicInitQPMinus26(std::clamp(refQp, minQp, maxQp) - QP26);
  }

  if (!sps.getJointCbCrEnabledFlag() || !isChromaEnabled(getChromaFormatIdc()))
  {
    pps.setJointCbCrQpOffsetPresentFlag(false);
  }
  else
  {
    bool enable = (m_chromaCbCrQpOffset != 0);
    for (int i=0; i < m_cuChromaQpOffsetList.size(); i++)
    {
      enable |= (m_cuChromaQpOffsetList[i].u.comp.jointCbCrOffset != 0);
    }
    pps.setJointCbCrQpOffsetPresentFlag(enable);
  }

#if ER_CHROMA_QP_WCG_PPS
  if (getWCGChromaQPControl().isEnabled())
  {
    const int baseQp=m_iQP+pps.getPPSId();
    const double chromaQp = m_wcgChromaQpControl.chromaQpScale * baseQp + m_wcgChromaQpControl.chromaQpOffset;
    const double dcbQP = m_wcgChromaQpControl.chromaCbQpScale * chromaQp;
    const double dcrQP = m_wcgChromaQpControl.chromaCrQpScale * chromaQp;
    const int cbQP =(int)(dcbQP + ( dcbQP < 0 ? -0.5 : 0.5) );
    const int crQP =(int)(dcrQP + ( dcrQP < 0 ? -0.5 : 0.5) );
    pps.setQpOffset(COMPONENT_Cb, Clip3(-12, 12, std::min(0, cbQP) + m_chromaCbQpOffset));
    pps.setQpOffset(COMPONENT_Cr, Clip3(-12, 12, std::min(0, crQP) + m_chromaCrQpOffset));
    if(pps.getJointCbCrQpOffsetPresentFlag())
    {
      pps.setQpOffset(JOINT_CbCr, Clip3(-12, 12, (std::min(0, cbQP) + std::min(0, crQP)) / 2 + m_chromaCbCrQpOffset));
    }
    else
    {
      pps.setQpOffset(JOINT_CbCr, 0);
    }
  }
  else
  {
#endif
  pps.setQpOffset(COMPONENT_Cb, m_chromaCbQpOffset );
  pps.setQpOffset(COMPONENT_Cr, m_chromaCrQpOffset );
  if (pps.getJointCbCrQpOffsetPresentFlag())
  {
    pps.setQpOffset(JOINT_CbCr, m_chromaCbCrQpOffset);
  }
  else
  {
    pps.setQpOffset(JOINT_CbCr, 0);
  }
#if ER_CHROMA_QP_WCG_PPS
  }
#endif
#if W0038_CQP_ADJ
  bool chromaDeltaQpEnabled = false;
  {
    chromaDeltaQpEnabled = (m_sliceChromaQpOffsetIntraOrPeriodic[0] || m_sliceChromaQpOffsetIntraOrPeriodic[1]);
    if (!chromaDeltaQpEnabled)
    {
      for (int i = 0; i < m_gopSize; i++)
      {
        if( m_GOPList[i].m_CbQPoffset || m_GOPList[i].m_CrQPoffset )
        {
          chromaDeltaQpEnabled = true;
          break;
        }
      }
    }
  }
 #if ENABLE_QPA
  if ((getUsePerceptQPA() || getSliceChromaOffsetQpPeriodicity() > 0) && isChromaEnabled(getChromaFormatIdc()))
  {
    chromaDeltaQpEnabled = true;
  }
 #endif
  pps.setSliceChromaQpFlag(chromaDeltaQpEnabled);
#endif
  if (!pps.getSliceChromaQpFlag() && sps.getUseDualITree() && isChromaEnabled(getChromaFormatIdc()))
  {
    pps.setSliceChromaQpFlag(m_chromaCbQpOffsetDualTree != 0 || m_chromaCrQpOffsetDualTree != 0 || m_chromaCbCrQpOffsetDualTree != 0);
  }
  if (m_gopBasedRPREnabledFlag || m_rprFunctionalityTestingEnabledFlag)
  {
    bool isRprPPS = false;
    for (int nr = 0; nr < NUM_RPR_PPS; nr++)
    {
      if ((pps.getPPSId() == RPR_PPS_ID[nr]) && (RPR_PPS_ID[nr] != m_layerId))
      {
        isRprPPS = true;
      }
    }
    if (isRprPPS)
    {
      pps.setSliceChromaQpFlag(true);
    }
  }
  int minCbSizeY = (1 << sps.getLog2MinCodingBlockSize());
  pps.setWrapAroundEnabledFlag                ( m_wrapAround );
  if( m_wrapAround )
  {
    pps.setPicWidthMinusWrapAroundOffset      ((pps.getPicWidthInLumaSamples()/minCbSizeY) - (m_wrapAroundOffset / minCbSizeY));
    pps.setWrapAroundOffset                   (minCbSizeY *(pps.getPicWidthInLumaSamples() / minCbSizeY- pps.getPicWidthMinusWrapAroundOffset()));
  }
  else
  {
    pps.setPicWidthMinusWrapAroundOffset      ( 0 );
    pps.setWrapAroundOffset                   ( 0 );
  }
  CHECK( !sps.getWrapAroundEnabledFlag() && pps.getWrapAroundEnabledFlag(), "When sps_ref_wraparound_enabled_flag is equal to 0, the value of pps_ref_wraparound_enabled_flag shall be equal to 0.");
  CHECK( (((sps.getCTUSize() / minCbSizeY) + 1) > ((pps.getPicWidthInLumaSamples() / minCbSizeY) - 1)) && pps.getWrapAroundEnabledFlag(), "When the value of CtbSizeY / MinCbSizeY + 1 is greater than pps_pic_width_in_luma_samples / MinCbSizeY - 1, the value of pps_ref_wraparound_enabled_flag shall be equal to 0.");

  pps.setNoPicPartitionFlag( m_noPicPartitionFlag );
  if( m_noPicPartitionFlag == false )
  {
    pps.setLog2CtuSize( ceilLog2( sps.getCTUSize()) );
    pps.setNumExpTileColumns( (uint32_t) m_tileColumnWidth.size() );
    pps.setNumExpTileRows( (uint32_t) m_tileRowHeight.size() );
    pps.setTileColumnWidths( m_tileColumnWidth );
    pps.setTileRowHeights( m_tileRowHeight );
    pps.initTiles();
    pps.setRectSliceFlag( m_rectSliceFlag );
    if( m_rectSliceFlag )
    {
      pps.setSingleSlicePerSubPicFlag(m_singleSlicePerSubPicFlag);
      pps.setNumSlicesInPic( m_numSlicesInPic );
      pps.setTileIdxDeltaPresentFlag( m_tileIdxDeltaPresentFlag );
      pps.setRectSlices( m_rectSlices );
      pps.initRectSliceMap(&sps);
    }
    else
    {
      pps.initRasterSliceMap( m_rasterSliceSize );
    }
    pps.initSubPic(sps);
    pps.setLoopFilterAcrossTilesEnabledFlag( m_bLFCrossTileBoundaryFlag );
    pps.setLoopFilterAcrossSlicesEnabledFlag( m_bLFCrossSliceBoundaryFlag );
  }
  else
  {
    pps.setLog2CtuSize( ceilLog2( sps.getCTUSize()) );
    pps.setNumExpTileColumns(1);
    pps.setNumExpTileRows(1);
    pps.addTileColumnWidth( pps.getPicWidthInCtu( ) );
    pps.addTileRowHeight( pps.getPicHeightInCtu( ) );
    pps.initTiles();
    pps.setRectSliceFlag( 1 );
    pps.setNumSlicesInPic( 1 );
    pps.initRectSlices( );
    pps.setTileIdxDeltaPresentFlag( 0 );
    pps.setSliceTileIdx( 0, 0 );
    pps.initRectSliceMap( &sps );
    pps.initSubPic(sps);
    pps.setLoopFilterAcrossTilesEnabledFlag( true );
    pps.setLoopFilterAcrossSlicesEnabledFlag( true );
  }

  pps.setUseWP( m_useWeightedPred );
  pps.setWPBiPred( m_useWeightedBiPred );
  pps.setOutputFlagPresentFlag(false);

  if ( getDeblockingFilterMetric() )
  {
    pps.setDeblockingFilterOverrideEnabledFlag(true);
    pps.setPPSDeblockingFilterDisabledFlag(false);
  }
  else
  {
    pps.setDeblockingFilterOverrideEnabledFlag( !getDeblockingFilterOffsetInPPS() );
    pps.setPPSDeblockingFilterDisabledFlag( getDeblockingFilterDisable() );
  }

  if (! pps.getPPSDeblockingFilterDisabledFlag())
  {
    pps.setDeblockingFilterBetaOffsetDiv2  ( getDeblockingFilterBetaOffset() );
    pps.setDeblockingFilterTcOffsetDiv2    ( getDeblockingFilterTcOffset() );
    pps.setDeblockingFilterCbBetaOffsetDiv2( getDeblockingFilterCbBetaOffset() );
    pps.setDeblockingFilterCbTcOffsetDiv2  ( getDeblockingFilterCbTcOffset() );
    pps.setDeblockingFilterCrBetaOffsetDiv2( getDeblockingFilterCrBetaOffset() );
    pps.setDeblockingFilterCrTcOffsetDiv2  ( getDeblockingFilterCrTcOffset() );
  }
  else
  {
    pps.setDeblockingFilterBetaOffsetDiv2(0);
    pps.setDeblockingFilterTcOffsetDiv2(0);
    pps.setDeblockingFilterCbBetaOffsetDiv2(0);
    pps.setDeblockingFilterCbTcOffsetDiv2(0);
    pps.setDeblockingFilterCrBetaOffsetDiv2(0);
    pps.setDeblockingFilterCrTcOffsetDiv2(0);
  }

  // deblockingFilterControlPresentFlag is true if any of the settings differ from the inferred values:
  const bool deblockingFilterControlPresentFlag = pps.getDeblockingFilterOverrideEnabledFlag()   ||
                                                  pps.getPPSDeblockingFilterDisabledFlag()       ||
                                                  pps.getDeblockingFilterBetaOffsetDiv2() != 0   ||
                                                  pps.getDeblockingFilterTcOffsetDiv2() != 0     ||
                                                  pps.getDeblockingFilterCbBetaOffsetDiv2() != 0 ||
                                                  pps.getDeblockingFilterCbTcOffsetDiv2() != 0   ||
                                                  pps.getDeblockingFilterCrBetaOffsetDiv2() != 0 ||
                                                  pps.getDeblockingFilterCrTcOffsetDiv2() != 0;

  pps.setDeblockingFilterControlPresentFlag(deblockingFilterControlPresentFlag);

  pps.setCabacInitPresentFlag(CABAC_INIT_PRESENT_FLAG);
  pps.setLoopFilterAcrossSlicesEnabledFlag( m_bLFCrossSliceBoundaryFlag );

  bool chromaQPOffsetNotZero = false;
  if( pps.getQpOffset(COMPONENT_Cb) != 0 || pps.getQpOffset(COMPONENT_Cr) != 0 || pps.getJointCbCrQpOffsetPresentFlag() || pps.getSliceChromaQpFlag() || pps.getCuChromaQpOffsetListEnabledFlag() )
  {
    chromaQPOffsetNotZero = true;
  }
  bool chromaDbfOffsetNotSameAsLuma = true;
  if( pps.getDeblockingFilterCbBetaOffsetDiv2() == pps.getDeblockingFilterBetaOffsetDiv2() && pps.getDeblockingFilterCrBetaOffsetDiv2() == pps.getDeblockingFilterBetaOffsetDiv2()
     && pps.getDeblockingFilterCbTcOffsetDiv2() == pps.getDeblockingFilterTcOffsetDiv2() && pps.getDeblockingFilterCrTcOffsetDiv2() == pps.getDeblockingFilterTcOffsetDiv2() )
  {
    chromaDbfOffsetNotSameAsLuma = false;
  }
  if (isChromaEnabled(sps.getChromaFormatIdc()) && (chromaQPOffsetNotZero || chromaDbfOffsetNotSameAsLuma))
  {
    pps.setPPSChromaToolFlag(true);
  }
  else
  {
    pps.setPPSChromaToolFlag(false);
  }

  int histogram[MAX_NUM_REF + 1];
  for( int i = 0; i <= MAX_NUM_REF; i++ )
  {
    histogram[i]=0;
  }
  for( int i = 0; i < getGOPSize(); i++)
  {
    CHECK(!(getRPLEntry(0, i).m_numRefPicsActive >= 0 && getRPLEntry(0, i).m_numRefPicsActive <= MAX_NUM_REF), "Unspecified error");
    histogram[getRPLEntry(0, i).m_numRefPicsActive]++;
  }

  int maxHist=-1;
  int bestPos=0;
  for( int i = 0; i <= MAX_NUM_REF; i++ )
  {
    if(histogram[i]>maxHist)
    {
      maxHist=histogram[i];
      bestPos=i;
    }
  }
  CHECK(bestPos > 15, "Unspecified error");
  pps.setNumRefIdxDefaultActive(REF_PIC_LIST_0, bestPos);
  pps.setNumRefIdxDefaultActive(REF_PIC_LIST_1, bestPos);
  pps.setPictureHeaderExtensionPresentFlag(false);

  pps.setRplInfoInPhFlag(getSliceLevelRpl() ? false : true);
  pps.setDbfInfoInPhFlag(getSliceLevelDblk() ? false : true);
  pps.setSaoInfoInPhFlag(getSliceLevelSao() ? false : true);
  pps.setAlfInfoInPhFlag(getSliceLevelAlf() ? false : true);
  pps.setWpInfoInPhFlag(getSliceLevelWp() ? false : true);
  pps.setQpDeltaInfoInPhFlag(getSliceLevelDeltaQp() ? false : true);

  pps.pcv = new PreCalcValues( sps, pps, true );
  pps.setRpl1IdxPresentFlag(sps.getRPL1IdxPresentFlag());
}

void EncLib::xInitPicHeader(PicHeader &picHeader, const SPS &sps, const PPS &pps)
{
  int i;
  picHeader.initPicHeader();

  // parameter sets
  picHeader.setSPSId( sps.getSPSId() );
  picHeader.setPPSId( pps.getPPSId() );

  // merge list sizes
  picHeader.setMaxNumAffineMergeCand(getMaxNumAffineMergeCand());
  // copy partitioning constraints from SPS
  picHeader.setSplitConsOverrideFlag(false);
  picHeader.setMinQTSizes( sps.getMinQTSizes() );
  picHeader.setMaxMTTHierarchyDepths( sps.getMaxMTTHierarchyDepths() );
  picHeader.setMaxBTSizes( sps.getMaxBTSizes() );
  picHeader.setMaxTTSizes( sps.getMaxTTSizes() );

  bool useDeltaQp = getCuQpDeltaSubdiv() > 0;

  if( (getMaxDeltaQP() != 0 )|| getUseAdaptiveQP() )
  {
    useDeltaQp = true;
  }

#if SHARP_LUMA_DELTA_QP
  if( getLumaLevelToDeltaQPMapping().isEnabled() )
  {
    useDeltaQp = true;
  }
#endif
  if (getSmoothQPReductionEnable())
  {
    useDeltaQp = true;
  }
#if ENABLE_QPA
  if (getUsePerceptQPA() && !useDeltaQp)
  {
    CHECK( m_cuQpDeltaSubdiv != 0, "max. delta-QP subdiv must be zero!" );
    useDeltaQp = (getBaseQP() < 38) && (getSourceWidth() > 512 || getSourceHeight() > 320);
  }
#endif

  if( m_costMode==COST_SEQUENCE_LEVEL_LOSSLESS || m_costMode==COST_LOSSLESS_CODING )
  {
    useDeltaQp = false;
  }

  if (m_rcEnableRateControl)
  {
    picHeader.setCuQpDeltaSubdivIntra( 0 );
    picHeader.setCuQpDeltaSubdivInter( 0 );
  }
  else if (useDeltaQp)
  {
    picHeader.setCuQpDeltaSubdivIntra( m_cuQpDeltaSubdiv );
    picHeader.setCuQpDeltaSubdivInter( m_cuQpDeltaSubdiv );
  }
  else
  {
    picHeader.setCuQpDeltaSubdivIntra( 0 );
    picHeader.setCuQpDeltaSubdivInter( 0 );
  }

  picHeader.setCuChromaQpOffsetSubdivIntra(m_cuChromaQpOffsetSubdiv);
  picHeader.setCuChromaQpOffsetSubdivInter(m_cuChromaQpOffsetSubdiv);

  // virtual boundaries
  if( sps.getVirtualBoundariesEnabledFlag() )
  {
    picHeader.setVirtualBoundariesPresentFlag( sps.getVirtualBoundariesPresentFlag() );
    picHeader.setNumVerVirtualBoundaries(sps.getNumVerVirtualBoundaries());
    picHeader.setNumHorVirtualBoundaries(sps.getNumHorVirtualBoundaries());
    for (i = 0; i < 3; i++)
    {
      picHeader.setVirtualBoundariesPosX(sps.getVirtualBoundariesPosX(i), i);
      picHeader.setVirtualBoundariesPosY(sps.getVirtualBoundariesPosY(i), i);
    }
  }

#if GDR_ENABLED
  picHeader.setGdrOrIrapPicFlag(false);
#endif

  // gradual decoder refresh flag
  picHeader.setGdrPicFlag(false);

  // BDOF / DMVR / PROF
  picHeader.setBdofDisabledFlag(false);
  picHeader.setDmvrDisabledFlag(false);
  picHeader.setProfDisabledFlag(!sps.getUsePROF());
}

void EncLib::xInitAPS(APS &aps)
{
  //Do nothing now
}

void EncLib::xInitRPL(SPS &sps)
{
  const bool isFieldCoding = sps.getFieldSeqFlag();

  int numRPLCandidates = getRPLCandidateSize(0);
  // To allocate one additional memory for RPL of POC1 (first bottom field) which is not specified in cfg file
  int layerIdx = getVPS() == nullptr ? 0 : getVPS()->getGeneralLayerIdx(m_layerId);
  RPLList* rplLists[2];
  bool codeRplInSH = layerIdx > 0 && getRplOfDepLayerInSh() && getNumRefLayers(layerIdx) > 0 && (getAvoidIntraInDepLayer() || getIntraPeriod() > 1) ;
  setRplOfDepLayerInSh(codeRplInSH);
  if (codeRplInSH)
  {
    for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
    {
      sps.createRplList(l, 0);
      getRplList(l)->destroy();
      getRplList(l)->create(numRPLCandidates + (isFieldCoding ? 1 : 0));
      rplLists[l] = getRplList(l);
    }
  }
  else
  {
    for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
    {
      getRplList(l)->create(0);
      sps.createRplList(l, numRPLCandidates + (isFieldCoding ? 1 : 0));
      rplLists[l] = sps.getRplList(l);
    }
  }

  static_vector<int, MAX_VPS_LAYERS> refLayersIdx;
  if (layerIdx > 0 && !getRplOfDepLayerInSh())
  {
    if (getNumRefLayers(layerIdx) > 0)
    {
      for (int refLayerIdx = 0; refLayerIdx < layerIdx; refLayerIdx++)
      {
        if (getVPS()->getDirectRefLayerFlag(layerIdx, refLayerIdx))
        {
          refLayersIdx.push_back(refLayerIdx);
        }
      }
    }
  }

  for (int i = 0; i < 2; i++)
  {
    RPLList* rplList = rplLists[i];
    for (int j = 0; j < numRPLCandidates; j++)
    {
      const RPLEntry &ge = getRPLEntry(i, j);
      ReferencePictureList* rpl = rplList->getReferencePictureList(j);
      rpl->setNumberOfShorttermPictures(ge.m_numRefPics);
      rpl->setNumberOfLongtermPictures(0);   //Hardcoded as 0 for now. need to update this when implementing LTRP
      rpl->setNumberOfActivePictures(ge.m_numRefPicsActive);
      rpl->setLtrpInSliceHeaderFlag(ge.m_ltrpInSliceHeaderFlag);
      rpl->setInterLayerPresentFlag( sps.getInterLayerPresentFlag() );
      // inter-layer reference picture is not signaled in SPS RPL, SPS is shared currently
      rpl->setNumberOfInterLayerPictures( 0 );

      if (getExplicitILRP())
      {
        bool isIntraLayerPredAllowed = (getVPS() && !getRplOfDepLayerInSh()) ? ((m_intraPeriod < 0 && ge.m_POC != 0) || (ge.m_POC % m_intraPeriod) != 0) : true;
        bool isInterLayerPredAllowed = (getVPS() && !getRplOfDepLayerInSh()) ? (!getVPS()->getIndependentLayerFlag(layerIdx) && ((m_intraPeriod < 0 && ge.m_POC != 0) || ((ge.m_POC % m_intraPeriod) != 0) || (getAvoidIntraInDepLayer() && layerIdx))) : false;
        int numRefActive = 0;
        int validNumILRef = 0;
        for (int k = 0; k < ge.m_numRefPicsActive; k++)
        {
          if (ge.m_deltaRefPics[k]==0 && ge.m_layerRef[k]!=-1)
          {
            if(isInterLayerPredAllowed)
            {
              for (int refLayerIdx: refLayersIdx)
              {
                if (refLayerIdx == ge.m_layerRef[k])
                {
                  rpl->setRefPicIdentifier(numRefActive, 0, true, true, m_vps->getInterLayerRefIdc(layerIdx, refLayerIdx));
                  numRefActive++;
                  validNumILRef++;
                  break;
                }
              }
            }
          }
          else if(isIntraLayerPredAllowed)
          {
            rpl->setRefPicIdentifier(numRefActive, -ge.m_deltaRefPics[k], 0, false, 0);
            numRefActive++;
          }
        }
        rpl->setNumberOfInterLayerPictures(validNumILRef);
        rpl->setNumberOfActivePictures(numRefActive);
        //Add non active intra layer ref pics
        int numRefInactive = 0;
        int inactiveStart = isIntraLayerPredAllowed ? ge.m_numRefPicsActive : 0;
        for (int k = inactiveStart; k < ge.m_numRefPics; k++)
        {
          if (!(ge.m_deltaRefPics[k]==0 && ge.m_layerRef[k]!=-1))
          {
            rpl->setRefPicIdentifier(numRefActive+numRefInactive, -ge.m_deltaRefPics[k], 0, false, 0);
            numRefInactive++;
          }
        }
        rpl->setNumberOfShorttermPictures(numRefActive-validNumILRef+numRefInactive);
      }
      else
      {
        if (!getRplOfDepLayerInSh())
        {
          bool isIntraLayerPredAllowed = getVPS() ? ((getVPS()->getIndependentLayerFlag(layerIdx) || (getVPS()->getPredDirection(ge.m_temporalId) != 1)) && ((m_intraPeriod < 0 && ge.m_POC != 0) || (ge.m_POC % m_intraPeriod) != 0)) : true;
          bool isInterLayerPredAllowed = getVPS() ? (!getVPS()->getIndependentLayerFlag(layerIdx) && (getVPS()->getPredDirection(ge.m_temporalId) != 2) && ((m_intraPeriod < 0 && ge.m_POC != 0) || ((ge.m_POC % m_intraPeriod) != 0) || (getAvoidIntraInDepLayer() && layerIdx))) : false;
   
          int numRefActive = 0;
          if (isIntraLayerPredAllowed)
          {
            for (int k = 0; k < ge.m_numRefPicsActive; k++)
            {
              rpl->setRefPicIdentifier(k, -ge.m_deltaRefPics[k], 0, false, 0);
            }
            numRefActive = ge.m_numRefPicsActive;
          }
          int validNumILRef = 0;
          if (isInterLayerPredAllowed)
          {
            for (int refLayerIdx : refLayersIdx)
            {
              rpl->setRefPicIdentifier(numRefActive + validNumILRef, 0, true, true, m_vps->getInterLayerRefIdc(layerIdx, refLayerIdx));
              validNumILRef++;
            }
            rpl->setNumberOfInterLayerPictures(validNumILRef);
            rpl->setNumberOfActivePictures(numRefActive + validNumILRef);
          }
          for (int k = numRefActive; k < ge.m_numRefPics; k++)
          {
            rpl->setRefPicIdentifier(k + validNumILRef, -ge.m_deltaRefPics[k], 0, false, 0);
          }
        }
        else
        {
          for (int k = 0; k < ge.m_numRefPics; k++)
          {
            rpl->setRefPicIdentifier(k, -ge.m_deltaRefPics[k], 0, false, 0);
          }
        }
      }
    }
  }

  if (isFieldCoding)
  {
    // To set RPL of POC1 (first bottom field) which is not specified in cfg file
    for (int i = 0; i < 2; i++)
    {
      RPLList* rplList = rplLists[i];
      ReferencePictureList* rpl = rplList->getReferencePictureList(numRPLCandidates);
      rpl->setNumberOfShorttermPictures(1);
      rpl->setNumberOfLongtermPictures(0);
      rpl->setNumberOfActivePictures(1);
      rpl->setLtrpInSliceHeaderFlag(0);
      rpl->setRefPicIdentifier(0, -1, 0, false, 0);
      rpl->setPOC(0, 0);
    }
  }

  const int numRplsL0 = sps.getNumRpl(REF_PIC_LIST_0);
  const int numRplsL1 = sps.getNumRpl(REF_PIC_LIST_1);

  bool isRpl1CopiedFromRpl0 = numRplsL0 == numRplsL1;

  for (int i = 0; isRpl1CopiedFromRpl0 && i < numRplsL0; i++)
  {
    const int numEntriesL0 = sps.getRplList(REF_PIC_LIST_0)->getReferencePictureList(i)->getNumRefEntries();
    const int numEntriesL1 = sps.getRplList(REF_PIC_LIST_1)->getReferencePictureList(i)->getNumRefEntries();

    isRpl1CopiedFromRpl0 = numEntriesL0 == numEntriesL1;

    for (int j = 0; isRpl1CopiedFromRpl0 && j < numEntriesL0; j++)
    {
      const int entryL0 = sps.getRplList(REF_PIC_LIST_0)->getReferencePictureList(i)->getRefPicIdentifier(j);
      const int entryL1 = sps.getRplList(REF_PIC_LIST_1)->getReferencePictureList(i)->getRefPicIdentifier(j);

      isRpl1CopiedFromRpl0 = entryL0 == entryL1;
    }
  }

  sps.setRPL1CopyFromRPL0Flag(isRpl1CopiedFromRpl0);

  //Check if all delta POC of STRP in each RPL has the same sign
  //Check RPLL0 first
  const RPLList* rplList0 = rplLists[0];
  const RPLList* rplList1 = rplLists[1];

  bool isAllEntriesinRPLHasSameSignFlag = true;
  for (uint32_t ii = 0; isAllEntriesinRPLHasSameSignFlag && ii < rplList0->getNumberOfReferencePictureLists(); ii++)
  {
    bool isFirstEntry = true;
    bool prevSign       = true;
    int  prevIdentifier = 0;

    const ReferencePictureList* rpl = rplList0->getReferencePictureList(ii);
    for (uint32_t jj = 0; isAllEntriesinRPLHasSameSignFlag && jj < rpl->getNumberOfActivePictures(); jj++)
    {
      if (!rpl->isRefPicLongterm(jj))
      {
        const int identifier = rpl->getRefPicIdentifier(jj);
        const int delta      = identifier - prevIdentifier;
        if (delta != 0)
        {
          const bool currentSign = delta >= 0;
          if (!isFirstEntry && currentSign != prevSign)
          {
            isAllEntriesinRPLHasSameSignFlag = false;
          }
          prevIdentifier = identifier;
          prevSign       = currentSign;
          isFirstEntry   = false;
        }
      }
    }
  }
  //Check RPLL1. Skip it if it is already found out that this flag is not true for RPL0 or if RPL1 is the same as RPL0
  for (uint32_t ii = 0; isAllEntriesinRPLHasSameSignFlag && !sps.getRPL1CopyFromRPL0Flag() && ii < rplList1->getNumberOfReferencePictureLists(); ii++)
  {
    bool isFirstEntry = true;
    bool lastSign = true;

    const ReferencePictureList* rpl = rplList1->getReferencePictureList(ii);
    for (uint32_t jj = 0; isAllEntriesinRPLHasSameSignFlag && jj < rpl->getNumberOfActivePictures(); jj++)
    {
      if (!rpl->isRefPicLongterm(jj))
      {
        if (isFirstEntry)
        {
          lastSign     = rpl->getRefPicIdentifier(jj) >= 0;
          isFirstEntry = false;
        }
        else
        {
          const bool currentSign = rpl->getRefPicIdentifier(jj) - rpl->getRefPicIdentifier(jj - 1) >= 0;
          if (currentSign != lastSign)
          {
            isAllEntriesinRPLHasSameSignFlag = false;
          }
        }
      }
    }
  }
  sps.setAllActiveRplEntriesHasSameSignFlag(isAllEntriesinRPLHasSameSignFlag);
}

void EncLib::selectReferencePictureList(Slice *slice, int pocCurr, int gopId, int ltPoc)
{
  const bool isEncodeLtRef = (pocCurr == ltPoc);
  if (m_compositeRefEnabled && isEncodeLtRef)
  {
    pocCurr++;
  }

  const RPLList *rplLists[NUM_REF_PIC_LIST_01];
  bool codeRplInSH = getRplOfDepLayerInSh();
  int            RPLIdx      = gopId;
  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    if (codeRplInSH)
    {
      rplLists[l] = getRplList(l);
      slice->setRplIdx(l, -1);
    }
    else
    {
      rplLists[l] = slice->getSPS()->getRplList(l);
    }
  }

  int fullListNum    = m_gopSize;
  int partialListNum = getRPLCandidateSize(0) - m_gopSize;
  int extraNum = fullListNum;

  int rplPeriod = m_intraPeriod;
  if( rplPeriod < 0 )  //Need to check if it is low delay or RA but with no RAP
  {
    if (rplLists[0]->getReferencePictureList(1)->getRefPicIdentifier(0) * rplLists[1]->getReferencePictureList(1)->getRefPicIdentifier(0) < 0)
    {
      rplPeriod = m_gopSize * 2;
    }
  }

  if (m_isLowDelay)
  {
    const int currPOCsinceLastIDR = pocCurr - slice->getLastIDR();
    if (currPOCsinceLastIDR < (2 * m_gopSize + 2))
    {
      int candidateIdx = (currPOCsinceLastIDR + m_gopSize - 1 >= fullListNum + partialListNum)
                           ? gopId
                           : currPOCsinceLastIDR + m_gopSize - 1;
      RPLIdx = candidateIdx;
    }
    else
    {
      RPLIdx = (pocCurr % m_gopSize == 0) ? m_gopSize - 1 : pocCurr % m_gopSize - 1;
    }
    extraNum = fullListNum + partialListNum;
  }
  for (; extraNum < fullListNum + partialListNum; extraNum++)
  {
    if( rplPeriod > 0 )
    {
      int pocIndex = pocCurr % rplPeriod;
      if (pocIndex == 0)
      {
        pocIndex = rplPeriod;
      }
      if (pocIndex == m_RPLList0[extraNum].m_POC)
      {
        RPLIdx = extraNum;
        extraNum++;
      }
    }
  }

  if (slice->getPic()->fieldPic)
  {
    // To set RPL index of POC1 (first bottom field)
    if (pocCurr == 1)
    {
      slice->setRplIdx(REF_PIC_LIST_0, getRPLCandidateSize(0));
      slice->setRplIdx(REF_PIC_LIST_1, getRPLCandidateSize(0));
    }
    else if( rplPeriod < 0 )
    {
      // To set RPL indexes for LD
      int numRPLCandidates = getRPLCandidateSize(0);
      if (pocCurr < numRPLCandidates - m_gopSize + 2)
      {
        RPLIdx = pocCurr + m_gopSize - 2;
      }
      else
      {
        if (pocCurr % m_gopSize == 0)
        {
          RPLIdx = m_gopSize - 2;
        }
        else if (pocCurr % m_gopSize == 1)
        {
          RPLIdx = m_gopSize - 1;
        }
        else
        {
          RPLIdx = pocCurr % m_gopSize - 2;
        }
      }
    }
  }

  *slice->getRpl(REF_PIC_LIST_0) = *rplLists[0]->getReferencePictureList(RPLIdx);
  *slice->getRpl(REF_PIC_LIST_1) = *rplLists[1]->getReferencePictureList(RPLIdx);

  if (!codeRplInSH)
  {
    slice->setRplIdx(REF_PIC_LIST_0, RPLIdx);
    slice->setRplIdx(REF_PIC_LIST_1, RPLIdx);
  }
}

void EncLib::setParamSetChanged(int spsId, int ppsId)
{
  m_ppsMap.setChangedFlag(ppsId);
  m_spsMap.setChangedFlag(spsId);
}

bool EncLib::PPSNeedsWriting(int ppsId)
{
  const bool changed = m_ppsMap.getChangedFlag(ppsId);
  m_ppsMap.clearChangedFlag(ppsId);
  return changed;
}

bool EncLib::SPSNeedsWriting(int spsId)
{
  const bool changed = m_spsMap.getChangedFlag(spsId);
  m_spsMap.clearChangedFlag(spsId);
  return changed;
}

void EncLib::checkPltStats( Picture* pic )
{
  int totalArea = 0;
  int pltArea = 0;
  for (auto apu : pic->cs->pus)
  {
    for (int i = 0; i < MAX_NUM_TBLOCKS; ++i)
    {
      int puArea = apu->blocks[i].width * apu->blocks[i].height;
      if (apu->blocks[i].width > 0 && apu->blocks[i].height > 0)
      {
        totalArea += puArea;
        if (CU::isPLT(*apu->cu) || CU::isIBC(*apu->cu))
        {
          pltArea += puArea;
        }
        break;
      }

    }
  }
  m_doPlt = pltArea * PLT_FAST_RATIO >= totalArea;
}

int EncCfg::getQPForPicture(const uint32_t gopIndex, const Slice *pSlice) const
{
  const int lumaQpBDOffset = pSlice->getSPS()->getQpBDOffset(ChannelType::LUMA);
  int qp;

  if (getCostMode()==COST_LOSSLESS_CODING)
  {
    qp = getBaseQP();
  }
  else
  {
    const SliceType sliceType=pSlice->getSliceType();

    qp = getBaseQP();

    // switch at specific qp and keep this qp offset
    static int appliedSwitchDQQ = 0; /* TODO: MT */
    if( pSlice->getPOC() == getSwitchPOC() )
    {
      appliedSwitchDQQ = getSwitchDQP();
    }
    qp += appliedSwitchDQQ;

    const FrameDeltaQps &deltaQps = getdQPs();
    if (deltaQps.size() != 0)
    {
      qp += deltaQps[pSlice->getPOC() / (m_compositeRefEnabled ? 2 : 1)];
    }

    if(sliceType==I_SLICE)
    {
      qp += getIntraQPOffset();
    }
    else
    {
      if (pSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP || pSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA)
      {
        qp += getIntraQPOffset();
      }
      else
      {
        const GOPEntry &gopEntry=getGOPEntry(gopIndex);
        // adjust QP according to the QP offset for the GOP entry.
        qp +=gopEntry.m_QPOffset;

        // adjust QP according to QPOffsetModel for the GOP entry.
        double dqpOffset=qp*gopEntry.m_QPOffsetModelScale+gopEntry.m_QPOffsetModelOffset+0.5;
        int qpOffset = (int)floor(Clip3<double>(0.0, 3.0, dqpOffset));
        qp += qpOffset ;
      }
    }
    if (m_gopBasedRPREnabledFlag)
    {
      if (pSlice->getPPS()->getPPSId() == (ENC_PPS_ID_RPR + pSlice->getNalUnitLayerId()))
      {
        qp += EncCfg::m_qpOffsetRPR;
      }
      if (pSlice->getPPS()->getPPSId() == (ENC_PPS_ID_RPR2 + pSlice->getNalUnitLayerId()))
      {
        qp += EncCfg::m_qpOffsetRPR2;
      }
      if (pSlice->getPPS()->getPPSId() == (ENC_PPS_ID_RPR3 + pSlice->getNalUnitLayerId()))
      {
        qp += EncCfg::m_qpOffsetRPR3;
      }
    }
    if (!m_gopBasedRPREnabledFlag && m_rprFunctionalityTestingEnabledFlag)
    {
      int currPoc = pSlice->getPOC() + m_frameSkip;
      int rprSegment = EncCfg::getRprSwitchingSegment(currPoc);
      qp += EncCfg::m_rprSwitchingQPOffsetOrderList[rprSegment];
    }
  }
  qp = Clip3( -lumaQpBDOffset, MAX_QP, qp );
  return qp;
}


//! \}
