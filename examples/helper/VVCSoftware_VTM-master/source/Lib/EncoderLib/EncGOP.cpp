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

/** \file     EncGOP.cpp
    \brief    GOP encoder class
*/

#include <list>
#include <algorithm>
#include <functional>

#include "EncLib.h"
#include "EncGOP.h"
#include "Analyze.h"
#include "libmd5/MD5.h"
#include "CommonLib/SEI.h"
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
#include "CommonLib/SEIPackedRegionsInfoProcess.h"
#endif
#include "CommonLib/NAL.h"
#include "NALwrite.h"

#include <math.h>
#include <deque>
#include <chrono>
#include <cinttypes>

#include "CommonLib/UnitTools.h"
#include "CommonLib/dtrace_codingstruct.h"
#include "CommonLib/dtrace_buffer.h"
#include "CommonLib/ProfileTierLevel.h"

#include "DecoderLib/DecLib.h"

//! \ingroup EncoderLib
//! \{

// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================
#if JVET_AJ0151_DSC_SEI && JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
EncGOP::EncGOP(DscSubstreamManager* dscSubstreamManager) : m_dscSubstreamManager(*dscSubstreamManager)
#else
EncGOP::EncGOP()
#endif
{
  m_iLastIDR            = 0;
  m_iGopSize            = 0;
  m_numPicsCoded                       = 0;
  m_first                              = true;
  m_latestDRAPPOC       = MAX_INT;
  m_latestEDRAPPOC      = MAX_INT;
  m_latestEdrapLeadingPicDecodableFlag = false;
  m_lastRasPoc          = MAX_INT;
  ::memset(m_riceBit, 0, 8 * 2 * sizeof(unsigned));
  ::memset(m_preQP, MAX_INT, 2 * sizeof(int));
  m_preIPOC             = 0;

  m_pcCfg               = nullptr;
  m_pcSliceEncoder      = nullptr;
  m_pcListPic           = nullptr;
  m_HLSWriter           = nullptr;
  m_seqFirst            = true;
  m_audIrapOrGdrAuFlag  = false;

  m_refreshPending       = 0;
  m_pocCRA              = 0;
  m_numLongTermRefPicSPS = 0;
  ::memset(m_ltRefPicPocLsbSps, 0, sizeof(m_ltRefPicPocLsbSps));
  ::memset(m_ltRefPicUsedByCurrPicFlag, 0, sizeof(m_ltRefPicUsedByCurrPicFlag));
  ::memset(m_lastBPSEI, 0, sizeof(m_lastBPSEI));
  m_rapWithLeading      = false;
  m_bufferingPeriodSEIPresentInAU = false;
  for (int i = 0; i < MAX_VPS_LAYERS; i++)
  {
    m_associatedIRAPType[i] = NAL_UNIT_CODED_SLICE_IDR_N_LP;
  }
  ::memset(m_associatedIRAPPOC, 0, sizeof(m_associatedIRAPPOC));
  m_pcDeblockingTempPicYuv = nullptr;
  m_pcRefLayerRescaledPicYuv = nullptr;

#if JVET_O0756_CALCULATE_HDRMETRICS
  m_ppcFrameOrg             = nullptr;
  m_ppcFrameRec             = nullptr;

  m_pcConvertFormat         = nullptr;
  m_pcConvertIQuantize      = nullptr;
  m_pcColorTransform        = nullptr;
  m_pcDistortionDeltaE      = nullptr;
  m_pcTransferFct           = nullptr;

  m_pcColorTransformParams  = nullptr;
  m_pcFrameFormat           = nullptr;

  m_metricTime = std::chrono::milliseconds(0);
#endif

  m_initAMaxBt = true;
  m_bgPOC = -1;

  m_picBg   = nullptr;
  m_picOrig = nullptr;

  m_isEncodedLTRef = false;
  m_isUseLTRef = false;
  m_isPrepareLTRef = true;
  m_lastLTRefPoc = 0;
  m_cntRightBottom      = 0;
  m_cntRightBottomIntra = 0;
}

EncGOP::~EncGOP()
{
  if( !m_pcCfg->getDecodeBitstream(0).empty() || !m_pcCfg->getDecodeBitstream(1).empty() )
  {
    // reset potential decoder resources
    tryDecodePicture(nullptr, 0, std::string(""), -1);
  }
#if JVET_O0756_CALCULATE_HDRMETRICS
  delete [] m_ppcFrameOrg;
  delete [] m_ppcFrameRec;

  m_ppcFrameOrg = m_ppcFrameRec = nullptr;

  delete m_pcConvertFormat;
  delete m_pcConvertIQuantize;
  delete m_pcColorTransform;
  delete m_pcDistortionDeltaE;
  delete m_pcTransferFct;
  delete m_pcColorTransformParams;
  delete m_pcFrameFormat;

  m_pcConvertFormat         = nullptr;
  m_pcConvertIQuantize      = nullptr;
  m_pcColorTransform        = nullptr;
  m_pcDistortionDeltaE      = nullptr;
  m_pcTransferFct           = nullptr;
  m_pcColorTransformParams  = nullptr;
  m_pcFrameFormat           = nullptr;
#endif
}

/** Create list to contain pointers to CTU start addresses of slice.
 */
void  EncGOP::create()
{
}

void  EncGOP::destroy()
{
  if (m_pcDeblockingTempPicYuv)
  {
    m_pcDeblockingTempPicYuv->destroy();
    delete m_pcDeblockingTempPicYuv;
    m_pcDeblockingTempPicYuv = nullptr;
  }
  if (m_picBg)
  {
    m_picBg->destroy();
    delete m_picBg;
    m_picBg = nullptr;
  }
  if (m_picOrig)
  {
    m_picOrig->destroy();
    delete m_picOrig;
    m_picOrig = nullptr;
  }
  if (m_pcCfg->getFilmGrainAnalysisEnabled())
  {
    m_fgAnalyzer.destroy();
  }
  if (m_pcRefLayerRescaledPicYuv)
  {
    m_pcRefLayerRescaledPicYuv->destroy();
    delete m_pcRefLayerRescaledPicYuv;
    m_pcRefLayerRescaledPicYuv= nullptr;
  }

}

void EncGOP::init ( EncLib* pcEncLib )
{
  m_pcEncLib     = pcEncLib;
  m_pcCfg                = pcEncLib;
  m_seiEncoder.init(m_pcCfg, pcEncLib, this);
  m_pcSliceEncoder       = pcEncLib->getSliceEncoder();
  m_pcListPic            = pcEncLib->getListPic();
  m_HLSWriter            = pcEncLib->getHLSWriter();
  m_pcLoopFilter         = pcEncLib->getDeblockingFilter();
  m_pcSAO                = pcEncLib->getSAO();
  m_pcALF                = pcEncLib->getALF();
  m_pcRateCtrl           = pcEncLib->getRateCtrl();
  ::memset(m_lastBPSEI, 0, sizeof(m_lastBPSEI));
  ::memset(m_totalCoded, 0, sizeof(m_totalCoded));
  m_HRD                = pcEncLib->getHRD();
  m_AUWriterIf = pcEncLib->getAUWriterIf();

  if (m_pcCfg->getFilmGrainAnalysisEnabled())
  {
    m_fgAnalyzer.init(m_pcCfg->getSourceWidth(), m_pcCfg->getSourceHeight(), m_pcCfg->getSourcePadding(0),
                      m_pcCfg->getSourcePadding(1), IPCOLOURSPACE_UNCHANGED, false, m_pcCfg->getChromaFormatIdc(),
                      m_pcCfg->getInputBitDepth(), m_pcCfg->getBitDepth(), m_pcCfg->getFrameSkip(),
                      m_pcCfg->getFGCSEICompModelPresent(), m_pcCfg->getFilmGrainExternalMask(),
                      m_pcCfg->getFilmGrainExternalDenoised());
  }

#if WCG_EXT
  if (m_pcCfg->getLmcs())
  {
    pcEncLib->getRdCost()->setReshapeInfo(m_pcCfg->getReshapeSignalType(), m_pcCfg->getBitDepth(ChannelType::LUMA));
    pcEncLib->getRdCost()->initLumaLevelToWeightTableReshape();
  }
  else if (m_pcCfg->getLumaLevelToDeltaQPMapping().mode)
  {
    pcEncLib->getRdCost()->setReshapeInfo(RESHAPE_SIGNAL_PQ, m_pcCfg->getBitDepth(ChannelType::LUMA));
    pcEncLib->getRdCost()->initLumaLevelToWeightTableReshape();
  }
  else if (m_pcCfg->getPrintWPSNR())
  {
    pcEncLib->getRdCost()->initLumaLevelToWeightTable(m_pcCfg->getBitDepth(ChannelType::LUMA));
  }

  const bool alfWSSD = m_pcCfg->getLmcs() && m_pcCfg->getReshapeSignalType() == RESHAPE_SIGNAL_PQ;
  pcEncLib->getALF()->setAlfWSSD(alfWSSD);
  if (alfWSSD)
  {
    pcEncLib->getALF()->setLumaLevelWeightTable(pcEncLib->getRdCost()->getLumaLevelWeightTable());
  }
#endif
  m_pcReshaper = pcEncLib->getReshaper();

#if JVET_O0756_CALCULATE_HDRMETRICS
  const bool calculateHdrMetrics = m_pcEncLib->getCalculateHdrMetrics();
  if(calculateHdrMetrics)
  {
    //allocate frame buffers and initialize class members
    const int chainNumber = 5;

    m_ppcFrameOrg = new hdrtoolslib::Frame* [chainNumber];
    m_ppcFrameRec = new hdrtoolslib::Frame* [chainNumber];

    double* whitePointDeltaE = new double[hdrtoolslib::NB_REF_WHITE];
    for (int i=0; i<hdrtoolslib::NB_REF_WHITE; i++)
    {
      whitePointDeltaE[i] = m_pcCfg->getWhitePointDeltaE(i);
    }

    double maxSampleValue                       = m_pcCfg->getMaxSampleValue();
    hdrtoolslib::SampleRange sampleRange        = m_pcCfg->getSampleRange();
    hdrtoolslib::ChromaFormat chFmt             = hdrtoolslib::ChromaFormat(m_pcCfg->getChromaFormatIdc());
    int                          bitDepth          = m_pcCfg->getBitDepth(ChannelType::LUMA);
    hdrtoolslib::ColorPrimaries colorPrimaries  = m_pcCfg->getColorPrimaries();
    bool enableTFunctionLUT                     = m_pcCfg->getEnableTFunctionLUT();
    hdrtoolslib::ChromaLocation* chromaLocation = new hdrtoolslib::ChromaLocation[2];
    for (int i=0; i<2; i++)
    {
      chromaLocation[i] = m_pcCfg->getChromaLocation(i);
    }
    int chromaUpFilter  = m_pcCfg->getChromaUPFilter();
    int cropOffsetLeft   = m_pcCfg->getCropOffsetLeft();
    int cropOffsetTop    = m_pcCfg->getCropOffsetTop();
    int cropOffsetRight  = m_pcCfg->getCropOffsetRight();
    int cropOffsetBottom = m_pcCfg->getCropOffsetBottom();

    const int width  = m_pcCfg->getSourceWidth() - cropOffsetLeft + cropOffsetRight;
    const int height = m_pcCfg->getSourceHeight() - cropOffsetTop + cropOffsetBottom;

    m_ppcFrameOrg[0] = new hdrtoolslib::Frame(width, height, false, hdrtoolslib::CM_YCbCr, colorPrimaries, chFmt, sampleRange, bitDepth, false, hdrtoolslib::TF_PQ, 0);
    m_ppcFrameRec[0] = new hdrtoolslib::Frame(width, height, false, hdrtoolslib::CM_YCbCr, colorPrimaries, chFmt, sampleRange, bitDepth, false, hdrtoolslib::TF_PQ, 0);

    m_ppcFrameOrg[1] = new hdrtoolslib::Frame(m_ppcFrameOrg[0]->m_width[hdrtoolslib::Y_COMP], m_ppcFrameOrg[0]->m_height[hdrtoolslib::Y_COMP], false, hdrtoolslib::CM_YCbCr, colorPrimaries, hdrtoolslib::CF_444, sampleRange, bitDepth, false, hdrtoolslib::TF_PQ, 0);
    m_ppcFrameRec[1] = new hdrtoolslib::Frame(m_ppcFrameRec[0]->m_width[hdrtoolslib::Y_COMP], m_ppcFrameRec[0]->m_height[hdrtoolslib::Y_COMP], false, hdrtoolslib::CM_YCbCr, colorPrimaries, hdrtoolslib::CF_444, sampleRange, bitDepth, false, hdrtoolslib::TF_PQ, 0);                                // 420 to 444 conversion

    m_ppcFrameOrg[2] =  new hdrtoolslib::Frame(m_ppcFrameOrg[0]->m_width[hdrtoolslib::Y_COMP], m_ppcFrameOrg[0]->m_height[hdrtoolslib::Y_COMP], true, hdrtoolslib::CM_YCbCr, colorPrimaries, hdrtoolslib::CF_444, hdrtoolslib::SR_UNKNOWN, 32, false, hdrtoolslib::TF_PQ, 0);
    m_ppcFrameRec[2] =  new hdrtoolslib::Frame(m_ppcFrameRec[0]->m_width[hdrtoolslib::Y_COMP], m_ppcFrameRec[0]->m_height[hdrtoolslib::Y_COMP], true, hdrtoolslib::CM_YCbCr, colorPrimaries, hdrtoolslib::CF_444, hdrtoolslib::SR_UNKNOWN, 32, false, hdrtoolslib::TF_PQ, 0);                                // 444 to Float conversion

    m_ppcFrameOrg[3] = new hdrtoolslib::Frame(m_ppcFrameOrg[0]->m_width[hdrtoolslib::Y_COMP], m_ppcFrameOrg[0]->m_height[hdrtoolslib::Y_COMP], true, hdrtoolslib::CM_RGB, hdrtoolslib::CP_2020, hdrtoolslib::CF_444, hdrtoolslib::SR_UNKNOWN, 32, false, hdrtoolslib::TF_PQ, 0);
    m_ppcFrameRec[3] = new hdrtoolslib::Frame(m_ppcFrameRec[0]->m_width[hdrtoolslib::Y_COMP], m_ppcFrameRec[0]->m_height[hdrtoolslib::Y_COMP], true, hdrtoolslib::CM_RGB, hdrtoolslib::CP_2020, hdrtoolslib::CF_444, hdrtoolslib::SR_UNKNOWN, 32, false, hdrtoolslib::TF_PQ, 0);                                // YCbCr to RGB conversion

    m_ppcFrameOrg[4] = new hdrtoolslib::Frame(m_ppcFrameOrg[0]->m_width[hdrtoolslib::Y_COMP], m_ppcFrameOrg[0]->m_height[hdrtoolslib::Y_COMP], true, hdrtoolslib::CM_RGB, hdrtoolslib::CP_2020, hdrtoolslib::CF_444, hdrtoolslib::SR_UNKNOWN, 32, false, hdrtoolslib::TF_NULL, 0);
    m_ppcFrameRec[4] = new hdrtoolslib::Frame(m_ppcFrameRec[0]->m_width[hdrtoolslib::Y_COMP], m_ppcFrameRec[0]->m_height[hdrtoolslib::Y_COMP], true, hdrtoolslib::CM_RGB, hdrtoolslib::CP_2020, hdrtoolslib::CF_444, hdrtoolslib::SR_UNKNOWN, 32, false, hdrtoolslib::TF_NULL, 0);                                // Inverse Transfer Function

    m_pcFrameFormat                   = new hdrtoolslib::FrameFormat();
    m_pcFrameFormat->m_isFloat        = true;
    m_pcFrameFormat->m_chromaFormat   = hdrtoolslib::CF_UNKNOWN;
    m_pcFrameFormat->m_colorSpace     = hdrtoolslib::CM_RGB;
    m_pcFrameFormat->m_colorPrimaries = hdrtoolslib::CP_2020;
    m_pcFrameFormat->m_sampleRange    = hdrtoolslib::SR_UNKNOWN;

    m_pcConvertFormat     = hdrtoolslib::ConvertColorFormat::create(width, height, chFmt, hdrtoolslib::CF_444, chromaUpFilter, chromaLocation, chromaLocation);
    m_pcConvertIQuantize  = hdrtoolslib::Convert::create(&m_ppcFrameOrg[1]->m_format, &m_ppcFrameOrg[2]->m_format);
    m_pcColorTransform    = hdrtoolslib::ColorTransform::create(m_ppcFrameOrg[2]->m_colorSpace, m_ppcFrameOrg[2]->m_colorPrimaries, m_ppcFrameOrg[3]->m_colorSpace, m_ppcFrameOrg[3]->m_colorPrimaries, true, 1);
    m_pcDistortionDeltaE  = new hdrtoolslib::DistortionMetricDeltaE(m_pcFrameFormat, false, maxSampleValue, whitePointDeltaE, 1);
    m_pcTransferFct       = hdrtoolslib::TransferFunction::create(hdrtoolslib::TF_PQ, true, (float) maxSampleValue, 0, 0.0, 1.0, enableTFunctionLUT);
  }
#endif
#if GDR_ENABLED
  m_lastGdrIntervalPoc = -1;
#endif
}

#if JVET_AJ0151_DSC_SEI
void EncGOP::xAddToSubstream(int substreamId, OutputNALUnit &nalu)
{
  // exit, if digitally signed content is not active
  if (!m_pcCfg->getDigitallySignedContentSEICfg().enabled)
  {
    return;
  }
  std::ostringstream rbspPayload;

  // add emultaion prevention
  writeNaluWithHeader(rbspPayload, nalu);

  // store NAL unit
  m_dscSubstreamManager.addToSubstream(substreamId, rbspPayload.str().c_str(), rbspPayload.str().size());
}
#endif

int EncGOP::xWriteOPI (AccessUnit &accessUnit, const OPI *opi)
{
  OutputNALUnit nalu(NAL_UNIT_OPI);
  m_HLSWriter->setBitstream(&nalu.m_bitstream);
  CHECK( nalu.m_temporalId, "The value of TemporalId of OPI NAL units shall be equal to 0" );
  m_HLSWriter->codeOPI( opi );
  accessUnit.push_back(new NALUnitEBSP(nalu));
  return (int)(accessUnit.back()->m_nalUnitData.str().size()) * 8;
}

int EncGOP::xWriteVPS (AccessUnit &accessUnit, const VPS *vps)
{
  OutputNALUnit nalu(NAL_UNIT_VPS);
  m_HLSWriter->setBitstream(&nalu.m_bitstream);
  CHECK( nalu.m_temporalId, "The value of TemporalId of VPS NAL units shall be equal to 0" );
  m_HLSWriter->codeVPS( vps );
#if JVET_AJ0151_DSC_SEI
  xAddToSubstream(0, nalu);
#endif
  accessUnit.push_back(new NALUnitEBSP(nalu));
  return (int)(accessUnit.back()->m_nalUnitData.str().size()) * 8;
}

int EncGOP::xWriteDCI(AccessUnit& accessUnit, const DCI* dci)
{
  OutputNALUnit nalu(NAL_UNIT_DCI);
  m_HLSWriter->setBitstream(&nalu.m_bitstream);
  CHECK(nalu.m_temporalId, "The value of TemporalId of DCI NAL units shall be equal to 0");
  m_HLSWriter->codeDCI(dci);
  accessUnit.push_back(new NALUnitEBSP(nalu));
  return (int)(accessUnit.back()->m_nalUnitData.str().size()) * 8;
}

int EncGOP::xWriteSPS( AccessUnit &accessUnit, const SPS *sps, const int layerId )
{
  OutputNALUnit nalu(NAL_UNIT_SPS);
  m_HLSWriter->setBitstream(&nalu.m_bitstream);
  nalu.m_nuhLayerId = layerId;
  CHECK( nalu.m_temporalId, "The value of TemporalId of SPS NAL units shall be equal to 0" );
  m_HLSWriter->codeSPS( sps );
#if JVET_AJ0151_DSC_SEI
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  xAddToSubstream(m_dscSubstreamId, nalu);
#else
  xAddToSubstream(0, nalu);
#endif
#endif
  accessUnit.push_back(new NALUnitEBSP(nalu));
  return (int) (accessUnit.back()->m_nalUnitData.str().size()) * 8;
}

int EncGOP::xWritePPS( AccessUnit &accessUnit, const PPS *pps, const int layerId )
{
  OutputNALUnit nalu(NAL_UNIT_PPS);
  m_HLSWriter->setBitstream(&nalu.m_bitstream);
  nalu.m_nuhLayerId = layerId;
  nalu.m_temporalId = accessUnit.temporalId;
  CHECK( nalu.m_temporalId < accessUnit.temporalId, "TemporalId shall be greater than or equal to the TemporalId of the layer access unit containing the NAL unit" );
  m_HLSWriter->codePPS( pps );
#if JVET_AJ0151_DSC_SEI
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  xAddToSubstream(m_dscSubstreamId, nalu);
#else
  xAddToSubstream(0, nalu);
#endif
#endif
  accessUnit.push_back(new NALUnitEBSP(nalu));
  return (int)(accessUnit.back()->m_nalUnitData.str().size()) * 8;
}

int EncGOP::xWriteAPS( AccessUnit &accessUnit, APS *aps, const int layerId, const bool isPrefixNUT )
{
  OutputNALUnit nalu( isPrefixNUT ? NAL_UNIT_PREFIX_APS : NAL_UNIT_SUFFIX_APS );
  m_HLSWriter->setBitstream(&nalu.m_bitstream);
  nalu.m_nuhLayerId = layerId;
  nalu.m_temporalId = aps->getTemporalId();
  aps->setLayerId( layerId );
  CHECK( nalu.m_temporalId < accessUnit.temporalId, "TemporalId shall be greater than or equal to the TemporalId of the layer access unit containing the NAL unit" );

#if GDR_ENC_TRACE
  if (aps)
  {
    printf("-aps ty:%d id:%d\n", to_underlying(aps->getAPSType()), aps->getAPSId());
  }
#endif

  m_HLSWriter->codeAPS(aps);
#if JVET_AJ0151_DSC_SEI
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  xAddToSubstream(m_dscSubstreamId, nalu);
#else
  xAddToSubstream(0, nalu);
#endif
#endif
  accessUnit.push_back(new NALUnitEBSP(nalu));
  return (int)(accessUnit.back()->m_nalUnitData.str().size()) * 8;
}

int EncGOP::xWriteParameterSets(AccessUnit &accessUnit, Slice *slice, const bool bSeqFirst, const int layerIdx, bool newPPS)
{
  int actualTotalBits = 0;

  if( bSeqFirst )
  {
    if (layerIdx == 0)
    {
      if (m_pcCfg->getOPIEnabled())
      {
        actualTotalBits += xWriteOPI(accessUnit, m_pcEncLib->getOPI());
      }
      if (m_pcCfg->getDCIEnabled())
      {
        actualTotalBits += xWriteDCI(accessUnit, m_pcEncLib->getDCI());
      }
      if (slice->getSPS()->getVPSId() != 0)
      {
        actualTotalBits += xWriteVPS(accessUnit, m_pcEncLib->getVPS());
      }
    }
    if( m_pcEncLib->SPSNeedsWriting( slice->getSPS()->getSPSId() ) ) // Note this assumes that all changes to the SPS are made at the EncLib level prior to picture creation (EncLib::xGetNewPicBuffer).
    {
      CHECK( !( bSeqFirst ), "Unspecified error" ); // Implementations that use more than 1 SPS need to be aware of activation issues.
      actualTotalBits += xWriteSPS( accessUnit, slice->getSPS(), m_pcEncLib->getLayerId() );
    }
  }

  if( newPPS ) // Note this assumes that all changes to the PPS are made at the EncLib level prior to picture creation (EncLib::xGetNewPicBuffer).
  {
    if (m_pcEncLib->getRprPopulatePPSatIntraFlag())
    {
      if (slice->isIntra())
      {
        actualTotalBits += xWritePPS(accessUnit, slice->getPPS(), m_pcEncLib->getLayerId());
        for (int nr = 0; nr < NUM_RPR_PPS; nr++)
        {
          if (slice->getPPS()->getPPSId() != RPR_PPS_ID[nr])
          {
            const PPS* pPPS = m_pcEncLib->getPPS(RPR_PPS_ID[nr]);
            actualTotalBits += xWritePPS(accessUnit, pPPS, m_pcEncLib->getLayerId());
          }
        }
      }
      else
      {
        bool isRprPPS = false;
        for (int nr = 0; nr < NUM_RPR_PPS; nr++)
        {
          if (slice->getPPS()->getPPSId() == RPR_PPS_ID[nr])
          {
            isRprPPS = true;
          }
        }
        if (!isRprPPS)
        {
          const PPS* pPPS = m_pcEncLib->getPPS(0);
          actualTotalBits += xWritePPS(accessUnit, pPPS, m_pcEncLib->getLayerId());
        }
      }
    }
    else
    {
      actualTotalBits += xWritePPS(accessUnit, slice->getPPS(), m_pcEncLib->getLayerId());
    }
  }

  return actualTotalBits;
}

int EncGOP::xWritePicHeader( AccessUnit &accessUnit, PicHeader *picHeader )
{
  OutputNALUnit nalu(NAL_UNIT_PH);
  m_HLSWriter->setBitstream(&nalu.m_bitstream);
  nalu.m_temporalId = accessUnit.temporalId;
  nalu.m_nuhLayerId = m_pcEncLib->getLayerId();
  m_HLSWriter->codePictureHeader( picHeader, true );
#if JVET_AJ0151_DSC_SEI
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  xAddToSubstream(m_dscSubstreamId, nalu);
#else
  xAddToSubstream(0, nalu);
#endif
#endif
  accessUnit.push_back(new NALUnitEBSP(nalu));
  return (int)(accessUnit.back()->m_nalUnitData.str().size()) * 8;
}

void EncGOP::xWriteAccessUnitDelimiter (AccessUnit &accessUnit, Slice *slice)
{
  AUDWriter audWriter;
  OutputNALUnit nalu(NAL_UNIT_ACCESS_UNIT_DELIMITER);
  nalu.m_temporalId = slice->getTLayer();
  const int vpsId   = slice->getSPS()->getVPSId();
  if (vpsId == 0)
  {
    nalu.m_nuhLayerId = 0;
  }
  else
  {
    nalu.m_nuhLayerId = slice->getVPS()->getLayerId(0);
  }
  CHECK( nalu.m_temporalId != accessUnit.temporalId, "TemporalId shall be equal to the TemporalId of the AU containing the NAL unit" );
  const int picType = slice->isIntra() ? 0 : (slice->isInterP() ? 1 : 2);
  audWriter.codeAUD(nalu.m_bitstream, m_audIrapOrGdrAuFlag, picType);
  accessUnit.push_front(new NALUnitEBSP(nalu));
}

void EncGOP::xWriteFillerData (AccessUnit &accessUnit, Slice *slice, uint32_t &fdSize)
{
  FDWriter fdWriter;
  OutputNALUnit nalu(NAL_UNIT_FD);
  nalu.m_temporalId = slice->getTLayer();
  const int vpsId   = slice->getSPS()->getVPSId();
  if (vpsId == 0)
  {
    nalu.m_nuhLayerId = 0;
  }
  else
  {
    nalu.m_nuhLayerId = slice->getVPS()->getLayerId(0);
  }
  CHECK( nalu.m_temporalId != accessUnit.temporalId, "TemporalId shall be equal to the TemporalId of the AU containing the NAL unit" );
  fdWriter.codeFD(nalu.m_bitstream, fdSize);
  accessUnit.push_back(new NALUnitEBSP(nalu));
}

// write SEI list into one NAL unit and add it to the Access unit at auPos
void EncGOP::xWriteSEI (NalUnitType naluType, SEIMessages& seiMessages, AccessUnit &accessUnit, AccessUnit::iterator &auPos, int temporalId)
{
  // don't do anything, if we get an empty list
  if (seiMessages.empty())
  {
    return;
  }
  OutputNALUnit nalu( naluType, m_pcEncLib->getLayerId(), temporalId );
  m_seiWriter.writeSEImessages(nalu.m_bitstream, seiMessages, *m_HRD, false, temporalId);
  auPos = accessUnit.insert(auPos, new NALUnitEBSP(nalu));
  auPos++;
}

void EncGOP::xWriteSEISeparately (NalUnitType naluType, SEIMessages& seiMessages, AccessUnit &accessUnit, AccessUnit::iterator &auPos, int temporalId)
{
  // don't do anything, if we get an empty list
  if (seiMessages.empty())
  {
    return;
  }

  for (SEIMessages::const_iterator sei = seiMessages.begin(); sei!=seiMessages.end(); sei++ )
  {
    SEIMessages tmpMessages;
    tmpMessages.push_back(*sei);
    OutputNALUnit nalu( naluType, m_pcEncLib->getLayerId(), temporalId );
    m_seiWriter.writeSEImessages(nalu.m_bitstream, tmpMessages, *m_HRD, false, temporalId);
    auPos = accessUnit.insert(auPos, new NALUnitEBSP(nalu));
    auPos++;
  }
}

void EncGOP::xClearSEIs(SEIMessages& seiMessages, bool deleteMessages)
{
  if (deleteMessages)
  {
    deleteSEIs(seiMessages);
  }
  else
  {
    seiMessages.clear();
  }
}

// write SEI messages as separate NAL units ordered
void EncGOP::xWriteLeadingSEIOrdered (SEIMessages& seiMessages, SEIMessages& duInfoSeiMessages, AccessUnit &accessUnit, int temporalId, bool testWrite)
{
  AccessUnit::iterator itNalu = accessUnit.begin();

#if JVET_AJ0151_DSC_SEI
  SEIMessages currentMessages;
  SEIMessages localMessages = seiMessages;

#if ENABLE_TRACING
  g_HLSTraceEnable = !testWrite;
#endif

  // skip over AUD or any Trailing SEIs, which belong to previous AU
  while ((itNalu != accessUnit.end()) &&
    ((*itNalu)->m_nalUnitType == NAL_UNIT_ACCESS_UNIT_DELIMITER
     || (*itNalu)->m_nalUnitType == NAL_UNIT_SUFFIX_SEI
     ))
  {
    itNalu++;
  }
  if (m_pcCfg->getDigitallySignedContentSEICfg().enabled)
  {
    currentMessages = extractSeisByType(localMessages, SEI::PayloadType::DIGITALLY_SIGNED_CONTENT_INITIALIZATION);
    CHECK( (currentMessages.size() > 1), "too many Digitally Signed Content Initialization SEI messages found");
    xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId);
    xClearSEIs(currentMessages, !testWrite);

    currentMessages = extractSeisByType(localMessages, SEI::PayloadType::DIGITALLY_SIGNED_CONTENT_SELECTION);
    CHECK( (currentMessages.size() > 1), "too many Digitally Signed Content Selection SEI messages found");
    xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId);
    xClearSEIs(currentMessages, !testWrite);
  }
#endif

  while ((itNalu != accessUnit.end()) &&
    ((*itNalu)->m_nalUnitType == NAL_UNIT_ACCESS_UNIT_DELIMITER
      || (*itNalu)->m_nalUnitType == NAL_UNIT_OPI
      || (*itNalu)->m_nalUnitType == NAL_UNIT_VPS
      || (*itNalu)->m_nalUnitType == NAL_UNIT_DCI
      || (*itNalu)->m_nalUnitType == NAL_UNIT_SPS
      || (*itNalu)->m_nalUnitType == NAL_UNIT_PPS
      ))
  {
    itNalu++;
  }

#if !JVET_AJ0151_DSC_SEI
  SEIMessages localMessages = seiMessages;
  SEIMessages currentMessages;

#if ENABLE_TRACING
  g_HLSTraceEnable = !testWrite;
#endif
#endif
  // The case that a specific SEI is not present is handled in xWriteSEI (empty list)

  // When SEI Manifest SEI message is present in an SEI NAL unit, the SEI Manifest SEI message shall be the first SEI
  // message in the SEI NAL unit (D3.45 in ISO/IEC 23008-2).
  if (m_pcCfg->getSEIManifestSEIEnabled())
  {
    currentMessages = extractSeisByType(localMessages, SEI::PayloadType::SEI_MANIFEST);
    CHECK(!(currentMessages.size() <= 1), "Unspecified error");
    xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId);
    xClearSEIs(currentMessages, !testWrite);
  }
  if (m_pcCfg->getSEIPrefixIndicationSEIEnabled())
  {
    //There may be multiple SEI prefix indication messages at the same time
    currentMessages = extractSeisByType(localMessages, SEI::PayloadType::SEI_PREFIX_INDICATION);
    xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId);
    xClearSEIs(currentMessages, !testWrite);
  }

  // Buffering period SEI must always be following active parameter sets
  currentMessages = extractSeisByType(localMessages, SEI::PayloadType::BUFFERING_PERIOD);
  CHECK(!(currentMessages.size() <= 1), "Unspecified error");
  xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId);
  xClearSEIs(currentMessages, !testWrite);

  // Picture timing SEI must always be following buffering period
  // Note: When general_same_pic_timing_in_all_ols_flag is equal to 1, PT SEI messages are required
  //       to be placed into separate NAL units. The code below conforms to the constraint even if
  //       general_same_pic_timing_in_all_ols_flag is equal to 0
  currentMessages = extractSeisByType(localMessages, SEI::PayloadType::PICTURE_TIMING);
  CHECK(!(currentMessages.size() <= 1), "Unspecified error");
  xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId);
  xClearSEIs(currentMessages, !testWrite);

  // Decoding unit info SEI must always be following picture timing
  if (!duInfoSeiMessages.empty())
  {
    currentMessages.push_back(duInfoSeiMessages.front());
    if (!testWrite)
    {
      duInfoSeiMessages.pop_front();
    }
    xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId);
    xClearSEIs(currentMessages, !testWrite);
  }

  if (m_pcCfg->getScalableNestingSEIEnabled())
  {
    // Scalable nesting SEI must always be the following DU info
    currentMessages = extractSeisByType(localMessages, SEI::PayloadType::SCALABLE_NESTING);
    xWriteSEISeparately(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId);
    xClearSEIs(currentMessages, !testWrite);
  }


  // And finally everything else one by one
  xWriteSEISeparately(NAL_UNIT_PREFIX_SEI, localMessages, accessUnit, itNalu, temporalId);
  xClearSEIs(localMessages, !testWrite);

  if (!testWrite)
  {
    seiMessages.clear();
  }
}

void EncGOP::xWriteLeadingSEIMessages (SEIMessages& seiMessages, SEIMessages& duInfoSeiMessages, AccessUnit &accessUnit, int temporalId, const SPS *sps, std::deque<DUData> &duData)
{
  AccessUnit testAU;
  SEIMessages picTimingSEIs = getSeisByType(seiMessages, SEI::PayloadType::PICTURE_TIMING);
  CHECK(!(picTimingSEIs.size() < 2), "Unspecified error");
  auto pt = picTimingSEIs.empty() ? nullptr : (SEIPictureTiming*) picTimingSEIs.front();

  // test writing
  xWriteLeadingSEIOrdered(seiMessages, duInfoSeiMessages, testAU, temporalId, true);
  // update Timing and DU info SEI
  xUpdateDuData(testAU, duData);
  xUpdateTimingSEI(pt, duData, sps);
  xUpdateDuInfoSEI(duInfoSeiMessages, pt);
  // actual writing
  xWriteLeadingSEIOrdered(seiMessages, duInfoSeiMessages, accessUnit, temporalId, false);

  // testAU will automatically be cleaned up when losing scope
}

void EncGOP::xWriteTrailingSEIMessages (SEIMessages& seiMessages, AccessUnit &accessUnit, int temporalId)
{
  // Note: using accessUnit.end() works only as long as this function is called after slice coding and before EOS/EOB NAL units
  AccessUnit::iterator pos = accessUnit.end();
  xWriteSEISeparately(NAL_UNIT_SUFFIX_SEI, seiMessages, accessUnit, pos, temporalId);
  deleteSEIs(seiMessages);
}

void EncGOP::xWriteDuSEIMessages (SEIMessages& duInfoSeiMessages, AccessUnit &accessUnit, int temporalId, std::deque<DUData> &duData)
{
  if (m_pcCfg->getDecodingUnitInfoSEIEnabled() && m_HRD->getBufferingPeriodSEI()->duCpbParamsInPicTimingSei)
  {
    int naluIdx = 0;
    AccessUnit::iterator nalu = accessUnit.begin();

    // skip over first DU, we have a DU info SEI there already
    while (naluIdx < duData[0].accumNalsDU && nalu!=accessUnit.end())
    {
      naluIdx++;
      nalu++;
    }

    SEIMessages::iterator duSEI = duInfoSeiMessages.begin();
    // loop over remaining DUs
    for (int duIdx = 1; duIdx < duData.size(); duIdx++)
    {
      CHECK(duSEI == duInfoSeiMessages.end(), "Number of generated SEIs should match number of DUs");

      // write the next SEI
      SEIMessages tmpSEI;
      tmpSEI.push_back(*duSEI);
      xWriteSEI(NAL_UNIT_PREFIX_SEI, tmpSEI, accessUnit, nalu, temporalId);
      // nalu points to the position after the SEI, so we have to increase the index as well
      naluIdx++;
      while ((naluIdx < duData[duIdx].accumNalsDU) && nalu!=accessUnit.end())
      {
        naluIdx++;
        nalu++;
      }
      duSEI++;
    }
  }
  deleteSEIs(duInfoSeiMessages);
}


void EncGOP::xCreateIRAPLeadingSEIMessages (SEIMessages& seiMessages, const SPS *sps, const PPS *pps)
{
  OutputNALUnit nalu(NAL_UNIT_PREFIX_SEI);

  if(m_pcCfg->getFramePackingArrangementSEIEnabled())
  {
    SEIFramePacking *sei = new SEIFramePacking;
    m_seiEncoder.initSEIFramePacking(sei, m_numPicsCoded);
    seiMessages.push_back(sei);
  }

  if (m_pcCfg->getParameterSetsInclusionIndicationSEIEnabled())
  {
    SEIParameterSetsInclusionIndication* sei = new SEIParameterSetsInclusionIndication;
    m_seiEncoder.initSEIParameterSetsInclusionIndication(sei);
    seiMessages.push_back(sei);
  }

  if(m_pcCfg->getSEIAlternativeTransferCharacteristicsSEIEnable())
  {
    SEIAlternativeTransferCharacteristics *seiAlternativeTransferCharacteristics = new SEIAlternativeTransferCharacteristics;
    m_seiEncoder.initSEIAlternativeTransferCharacteristics(seiAlternativeTransferCharacteristics);
    seiMessages.push_back(seiAlternativeTransferCharacteristics);
  }
  if (m_pcCfg->getErpSEIEnabled())
  {
    SEIEquirectangularProjection *sei = new SEIEquirectangularProjection;
    m_seiEncoder.initSEIErp(sei);
    seiMessages.push_back(sei);
  }

  if (m_pcCfg->getSphereRotationSEIEnabled())
  {
    SEISphereRotation *sei = new SEISphereRotation;
    m_seiEncoder.initSEISphereRotation(sei);
    seiMessages.push_back(sei);
  }

  if (m_pcCfg->getOmniViewportSEIEnabled())
  {
    SEIOmniViewport *sei = new SEIOmniViewport;
    m_seiEncoder.initSEIOmniViewport(sei);
    seiMessages.push_back(sei);
  }
  if (m_pcCfg->getRwpSEIEnabled())
  {
    SEIRegionWisePacking *seiRegionWisePacking = new SEIRegionWisePacking;
    m_seiEncoder.initSEIRegionWisePacking(seiRegionWisePacking);
    seiMessages.push_back(seiRegionWisePacking);
  }
  if (m_pcCfg->getGcmpSEIEnabled())
  {
    SEIGeneralizedCubemapProjection *sei = new SEIGeneralizedCubemapProjection;
    m_seiEncoder.initSEIGcmp(sei);
    seiMessages.push_back(sei);
  }
  if (m_pcCfg->getSubpicureLevelInfoSEICfg().m_enabled)
  {
    auto sli = new SEISubpictureLevelInfo;
    m_seiEncoder.initSEISubpictureLevelInfo(sli, sps);
    seiMessages.push_back(sli);
  }
  if (m_pcCfg->getSampleAspectRatioInfoSEIEnabled())
  {
    SEISampleAspectRatioInfo *seiSampleAspectRatioInfo = new SEISampleAspectRatioInfo;
    m_seiEncoder.initSEISampleAspectRatioInfo(seiSampleAspectRatioInfo);
    seiMessages.push_back(seiSampleAspectRatioInfo);
  }
  // film grain
  if (m_pcCfg->getFilmGrainCharactersticsSEIEnabled() && !m_pcCfg->getFilmGrainCharactersticsSEIPerPictureSEI())
  {
    SEIFilmGrainCharacteristics *sei = new SEIFilmGrainCharacteristics;
    m_seiEncoder.initSEIFilmGrainCharacteristics(sei);
    if (m_pcCfg->getFilmGrainAnalysisEnabled())
    {
      sei->m_log2ScaleFactor = m_fgAnalyzer.getLog2scaleFactor();
      for (int compIdx = 0; compIdx < getNumberValidComponents(m_pcCfg->getChromaFormatIdc()); compIdx++)
      {
        if (sei->m_compModel[compIdx].presentFlag)
        {   // higher importance of presentFlag is from cfg file
          sei->m_compModel[compIdx] = m_fgAnalyzer.getCompModel(compIdx);
        }
      }
    }
    seiMessages.push_back(sei);
  }

  // mastering display colour volume
  if (m_pcCfg->getMasteringDisplaySEI().colourVolumeSEIEnabled)
  {
    SEIMasteringDisplayColourVolume *sei = new SEIMasteringDisplayColourVolume;
    m_seiEncoder.initSEIMasteringDisplayColourVolume(sei);
    seiMessages.push_back(sei);
  }

  // content light level
  if (m_pcCfg->getCLLSEIEnabled())
  {
    SEIContentLightLevelInfo *seiCLL = new SEIContentLightLevelInfo;
    m_seiEncoder.initSEIContentLightLevel(seiCLL);
    seiMessages.push_back(seiCLL);
  }

  // ambient viewing environment
  if (m_pcCfg->getAmbientViewingEnvironmentSEIEnabled())
  {
    SEIAmbientViewingEnvironment *seiAVE = new SEIAmbientViewingEnvironment;
    m_seiEncoder.initSEIAmbientViewingEnvironment(seiAVE);
    seiMessages.push_back(seiAVE);
  }

  // content colour volume
  if (m_pcCfg->getCcvSEIEnabled())
  {
    SEIContentColourVolume *seiContentColourVolume = new SEIContentColourVolume;
    m_seiEncoder.initSEIContentColourVolume(seiContentColourVolume);
    seiMessages.push_back(seiContentColourVolume);
  }

  if (m_pcCfg->getSdiSEIEnabled())
  {
    SEIScalabilityDimensionInfo *seiScalabilityDimensionInfo = new SEIScalabilityDimensionInfo;
    m_seiEncoder.initSEIScalabilityDimensionInfo(seiScalabilityDimensionInfo);
    seiMessages.push_back(seiScalabilityDimensionInfo);
  }
  // multiview acquisition information
  if (m_pcCfg->getMaiSEIEnabled())
  {
    SEIMultiviewAcquisitionInfo *seiMultiviewAcquisitionInfo = new SEIMultiviewAcquisitionInfo;
    m_seiEncoder.initSEIMultiviewAcquisitionInfo(seiMultiviewAcquisitionInfo);
    seiMessages.push_back(seiMultiviewAcquisitionInfo);
  }
  // multiview view position
  if (m_pcCfg->getMvpSEIEnabled())
  {
    SEIMultiviewViewPosition *seiMultiviewViewPosition = new SEIMultiviewViewPosition;
    m_seiEncoder.initSEIMultiviewViewPosition(seiMultiviewViewPosition);
    seiMessages.push_back(seiMultiviewViewPosition);
  }
  // alpha channel information
  if (m_pcCfg->getAciSEIEnabled())
  {
    SEIAlphaChannelInfo *seiAlphaChannelInfo = new SEIAlphaChannelInfo;
    m_seiEncoder.initSEIAlphaChannelInfo(seiAlphaChannelInfo);
    seiMessages.push_back(seiAlphaChannelInfo);
  }
  // depth representation information
  if (m_pcCfg->getDriSEIEnabled())
  {
    SEIDepthRepresentationInfo *seiDepthRepresentationInfo = new SEIDepthRepresentationInfo;
    m_seiEncoder.initSEIDepthRepresentationInfo(seiDepthRepresentationInfo);
    seiMessages.push_back(seiDepthRepresentationInfo);
  }
  // colour transform information
  if (m_pcCfg->getCtiSEIEnabled())
  {
    SEIColourTransformInfo* seiCTI = new SEIColourTransformInfo;
    m_seiEncoder.initSEIColourTransformInfo(seiCTI);
    seiMessages.push_back(seiCTI);
  }

  // Make sure that sei_manifest and sei_prefix are the last two initialized sei_msg, otherwise it will cause these two
  // Sei messages to not be able to enter all SEI messages
  if (m_pcCfg->getSEIManifestSEIEnabled())
  {
    SEIManifest *seiSEIManifest = new SEIManifest;
    m_seiEncoder.initSEISEIManifest(seiSEIManifest, seiMessages);
    seiMessages.push_back(seiSEIManifest);
  }
  if (m_pcCfg->getSEIPrefixIndicationSEIEnabled())
  {
    int numSeiPrefixMsg = 0;
    for (auto &it: seiMessages)
    {
      if (it->payloadType() == SEI::PayloadType::SEI_MANIFEST)
      {
        break;
      }
      numSeiPrefixMsg++;
    }
    for (auto &it: seiMessages)
    {
      if (numSeiPrefixMsg == 0 || it->payloadType() == SEI::PayloadType::SEI_MANIFEST)
      {
        break;
      }
      SEIPrefixIndication *seiSEIPrefixIndication = new SEIPrefixIndication;
      m_seiEncoder.initSEISEIPrefixIndication(seiSEIPrefixIndication, it);
      seiMessages.push_back(seiSEIPrefixIndication);
      numSeiPrefixMsg--;
    }
  }

  if (m_pcCfg->getConstrainedRaslencoding())
  {
    SEIConstrainedRaslIndication* seiConstrainedRasl = new SEIConstrainedRaslIndication;
    seiMessages.push_back(seiConstrainedRasl);
  }
  if (m_pcCfg->getSiiSEIEnabled())
  {
    SEIShutterIntervalInfo *seiShutterInterval = new SEIShutterIntervalInfo;
    m_seiEncoder.initSEIShutterIntervalInfo(seiShutterInterval);
    seiMessages.push_back(seiShutterInterval);
  }
  if (m_pcCfg->getSptiSEIEnabled())
  {
    SEISourcePictureTimingInfo* seiSourcePictureTimingInfo = new SEISourcePictureTimingInfo(sps->getMaxTLayers() - 1);
    m_seiEncoder.initSEISourcePictureTimingInfo(seiSourcePictureTimingInfo);
    seiMessages.push_back(seiSourcePictureTimingInfo);
  }
  if (m_pcCfg->getNNPostFilterSEICharacteristicsEnabled() && !m_pcCfg->getNNPostFilterSEICharacteristicsUseSuffixSEI())
  {
    xCreateNNPostFilterCharacteristicsSEIMessages(seiMessages);
  }
  if (m_pcCfg->getPoSEIEnabled())
  {
    SEIProcessingOrderInfo *seiProcessingOrder = new SEIProcessingOrderInfo;
    SEIProcessingOrderNesting *seiProcessingOrderNesting = new SEIProcessingOrderNesting;
    m_seiEncoder.initSEIProcessingOrderInfo(seiProcessingOrder, seiProcessingOrderNesting);
    seiMessages.push_back(seiProcessingOrder);
    seiMessages.push_back(seiProcessingOrderNesting);
  }
#if JVET_AJ0151_DSC_SEI
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  if (m_pcCfg->getDigitallySignedContentSEICfg().enabled && m_dscSubstreamId == 0)
#else
  if (m_pcCfg->getDigitallySignedContentSEICfg().enabled)
#endif
  {
    SEIDigitallySignedContentInitialization *sei = new SEIDigitallySignedContentInitialization;
    m_seiEncoder.initSEIDigitallySignedContentInitialization(sei);
    seiMessages.push_back(sei);
  }
#endif
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  if (m_pcCfg->getPriSEIEnabled())
  {
    SEIPackedRegionsInfo *seiPackedRegionsInfo = new SEIPackedRegionsInfo;
    m_seiEncoder.initSEIPackedRegionsInfo(seiPackedRegionsInfo);
    seiMessages.push_back(seiPackedRegionsInfo);
  }
#endif
}

void EncGOP::xCreatePerPictureSEIMessages (int picInGOP, SEIMessages& seiMessages, SEIMessages& nestedSeiMessages, Slice *slice)
{
  if ((m_pcCfg->getBufferingPeriodSEIEnabled()) && (slice->isIRAP() || slice->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR) &&
    slice->getNalUnitLayerId()==slice->getVPS()->getLayerId(0) &&
  (slice->getSPS()->getGeneralHrdParametersPresentFlag()))
  {
    auto                bufferingPeriodSEI = new SEIBufferingPeriod;
    const bool          noLeadingPictures =
      slice->getNalUnitType() != NAL_UNIT_CODED_SLICE_IDR_W_RADL && slice->getNalUnitType() != NAL_UNIT_CODED_SLICE_CRA;
    m_seiEncoder.initSEIBufferingPeriod(bufferingPeriodSEI,noLeadingPictures);
    m_HRD->setBufferingPeriodSEI(bufferingPeriodSEI);
    seiMessages.push_back(bufferingPeriodSEI);
    m_bufferingPeriodSEIPresentInAU = true;

    if (m_pcCfg->getScalableNestingSEIEnabled())
    {
      auto bufferingPeriodSEIcopy = new SEIBufferingPeriod;
      bufferingPeriodSEI->copyTo(*bufferingPeriodSEIcopy);
      nestedSeiMessages.push_back(bufferingPeriodSEIcopy);
    }
  }

  if (m_pcEncLib->getDependentRAPIndicationSEIEnabled() && slice->isDRAP())
  {
    SEIDependentRAPIndication *dependentRAPIndicationSEI = new SEIDependentRAPIndication();
    m_seiEncoder.initSEIDependentRAPIndication(dependentRAPIndicationSEI);
    seiMessages.push_back(dependentRAPIndicationSEI);
  }

  if (m_pcEncLib->getEdrapIndicationSEIEnabled() && slice->getEdrapRapId() > 0)
  {
    SEIExtendedDrapIndication *seiExtendedDrapIndication = new SEIExtendedDrapIndication();
    m_seiEncoder.initSEIExtendedDrapIndication(seiExtendedDrapIndication);
    // update EDRAP SEI message according to the reference lists of the slice
    seiExtendedDrapIndication->m_edrapIndicationRapIdMinus1 = slice->getEdrapRapId() - 1;
    seiExtendedDrapIndication->m_edrapIndicationLeadingPicturesDecodableFlag = slice->getLatestEdrapLeadingPicDecodableFlag();
    seiExtendedDrapIndication->m_edrapIndicationNumRefRapPicsMinus1 = slice->getEdrapNumRefRapPics() - 1;
    seiExtendedDrapIndication->m_edrapIndicationRefRapId.resize(seiExtendedDrapIndication->m_edrapIndicationNumRefRapPicsMinus1 + 1);
    for (int i = 0; i <= seiExtendedDrapIndication->m_edrapIndicationNumRefRapPicsMinus1; i++)
    {
      seiExtendedDrapIndication->m_edrapIndicationRefRapId[i] = slice->getEdrapRefRapId(i);
    }
    seiMessages.push_back(seiExtendedDrapIndication);
  }

  // insert one Annotated Region SEI for the picture (if the file exists)
  if (!m_pcCfg->getAnnotatedRegionSEIFileRoot().empty())
  {
    SEIAnnotatedRegions *seiAnnotatedRegions = new SEIAnnotatedRegions();
    const bool success = m_seiEncoder.initSEIAnnotatedRegions(seiAnnotatedRegions, slice->getPOC());

    if (success)
    {
      seiMessages.push_back(seiAnnotatedRegions);
    }
    else
    {
      delete seiAnnotatedRegions;
    }
  }

  if (!m_pcCfg->getObjectMaskInfoSEIFileRoot().empty())
  {
    // CHECK(!m_pcCfg->getSdiSEIEnabled(), "SDI-SEI has not enabled. (OMI-SEI depends on SDI-SEI)");
    SEIObjectMaskInfos* seiObjectMaskInfo = new SEIObjectMaskInfos();
    const bool          success           = m_seiEncoder.initSEIObjectMaskInfos(seiObjectMaskInfo, slice->getPOC());

    if (success)
    {
      seiMessages.push_back(seiObjectMaskInfo);
    }
    else
    {
      delete seiObjectMaskInfo;
    }
  }

  if (m_pcCfg->getFilmGrainCharactersticsSEIEnabled() && m_pcCfg->getFilmGrainCharactersticsSEIPerPictureSEI())
  {
    SEIFilmGrainCharacteristics *fgcSEI = new SEIFilmGrainCharacteristics;
    m_seiEncoder.initSEIFilmGrainCharacteristics(fgcSEI);
    if (m_pcCfg->getFilmGrainAnalysisEnabled())
    {
      fgcSEI->m_log2ScaleFactor = m_fgAnalyzer.getLog2scaleFactor();
      for (int compIdx = 0; compIdx < getNumberValidComponents(m_pcCfg->getChromaFormatIdc()); compIdx++)
      {
        if (fgcSEI->m_compModel[compIdx].presentFlag)
        {   // higher importance of presentFlag is from cfg file
          fgcSEI->m_compModel[compIdx] = m_fgAnalyzer.getCompModel(compIdx);
        }
      }
    }
    seiMessages.push_back(fgcSEI);
  }

  if (m_pcCfg->getEOISEIEnabled())
  {
    SEIEncoderOptimizationInfo *eoiSEI = new SEIEncoderOptimizationInfo;
    m_seiEncoder.initSEIEncoderOptimizationInfo(eoiSEI);
    seiMessages.push_back(eoiSEI);
  }


  // modality information SEI
  if (m_pcCfg->getMiSEIEnabled())
  {
    SEIModalityInfo* seiMI = new SEIModalityInfo;
    m_seiEncoder.initSEIModalityInfo(seiMI);
    seiMessages.push_back(seiMI);
  }

  if (m_pcCfg->getNnPostFilterSEIActivationEnabled() && !m_pcCfg->getNnPostFilterSEIActivationUseSuffixSEI())
  {
    xCreateNNPostFilterActivationSEIMessage(seiMessages, slice);
  }

  if (m_pcCfg->getPostFilterHintSEIEnabled())
  {
    SEIPostFilterHint *postFilterHintSEI = new SEIPostFilterHint;

    m_seiEncoder.initSEIPostFilterHint(postFilterHintSEI);
    seiMessages.push_back(postFilterHintSEI);
  }
#if JVET_AJ0151_DSC_SEI

  if (m_pcCfg->getDigitallySignedContentSEICfg().enabled)
  {
    SEIDigitallySignedContentSelection *sei = new SEIDigitallySignedContentSelection;
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
    m_seiEncoder.initSEIDigitallySignedContentSelection(sei, m_dscSubstreamId);
#else
    m_seiEncoder.initSEIDigitallySignedContentSelection(sei, 0);
#endif
    seiMessages.push_back(sei);
  }
#endif
#if JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
  if (m_pcCfg->getAURSEIEnabled())
  {
    SEIAIUsageRestrictions *aurSEI = new SEIAIUsageRestrictions;
    m_seiEncoder.initSEIAIUsageRestrictions(aurSEI);
    seiMessages.push_back(aurSEI);
  }
#endif 
}

void EncGOP::xCreateGenerativeFaceVideoSEIMessages(SEIMessages& seiMessages)
{
  for (int frameIndex = 0; frameIndex < m_pcCfg->getGenerativeFaceVideoSEINumber(); frameIndex++)
  {
    SEIGenerativeFaceVideo *seiGenerativeFaceVideo = new SEIGenerativeFaceVideo;
    m_seiEncoder.initSEIGenerativeFaceVideo(seiGenerativeFaceVideo, frameIndex);
    seiMessages.push_back(seiGenerativeFaceVideo);
  }
}

void EncGOP::xCreateGenerativeFaceVideoEnhancementSEIMessages(SEIMessages& seiMessages)
{
  for (int frameIndex = 0; frameIndex < m_pcCfg->getGenerativeFaceVideoEnhancementSEINumber(); frameIndex++)
  {
    SEIGenerativeFaceVideoEnhancement *seiGenerativeFaceVideoEnhancement = new SEIGenerativeFaceVideoEnhancement;
    m_seiEncoder.initSEIGenerativeFaceVideoEnhancement(seiGenerativeFaceVideoEnhancement, frameIndex);
    seiMessages.push_back(seiGenerativeFaceVideoEnhancement);
  }
}

void EncGOP::xCreateNNPostFilterCharacteristicsSEIMessages(SEIMessages& seiMessages)
{
  for (int i = 0; i < m_pcCfg->getNNPostFilterSEICharacteristicsNumFilters(); i++)
  {
    SEINeuralNetworkPostFilterCharacteristics *seiNNPostFilterCharacteristics = new SEINeuralNetworkPostFilterCharacteristics;
    m_seiEncoder.initSEINeuralNetworkPostFilterCharacteristics(seiNNPostFilterCharacteristics, i);
    seiMessages.push_back(seiNNPostFilterCharacteristics);
  }
}

void EncGOP::xCreateNNPostFilterActivationSEIMessage(SEIMessages& seiMessages, Slice* slice)
{
  SEINeuralNetworkPostFilterActivation *nnpfActivationSEI = new SEINeuralNetworkPostFilterActivation;
  m_seiEncoder.initSEINeuralNetworkPostFilterActivation(nnpfActivationSEI);
  CHECK(!slice->getPicHeader()->getPicOutputFlag(), "NNPFA SEI Message cannot be associated with picture with ph_pic_output_flag equal to 0")
  seiMessages.push_back(nnpfActivationSEI);
}

void EncGOP::xCreatePhaseIndicationSEIMessages(SEIMessages& seiMessages, Slice* slice, int ppsId)
{
  if (m_pcCfg->getPhaseIndicationSEIEnabledFullResolution() && ppsId == 0)
  {
    SEIPhaseIndication* seiPhaseIndication = new SEIPhaseIndication;
    m_seiEncoder.initSEIPhaseIndication(seiPhaseIndication, ppsId);
    seiMessages.push_back(seiPhaseIndication);
  }
  else if (m_pcCfg->getPhaseIndicationSEIEnabledReducedResolution() && ppsId == ENC_PPS_ID_RPR)
  {
    SEIPhaseIndication* seiPhaseIndication = new SEIPhaseIndication;
    m_seiEncoder.initSEIPhaseIndication(seiPhaseIndication, ppsId);
    seiMessages.push_back(seiPhaseIndication);
  }
}

void EncGOP::xCreateScalableNestingSEI(SEIMessages& seiMessages, SEIMessages& nestedSeiMessages, const std::vector<int> &targetOLSs, const std::vector<int> &targetLayers, const std::vector<uint16_t>& subpicIDs, uint16_t maxSubpicIdInPic)
{
  SEIMessages tmpMessages;
  while (!nestedSeiMessages.empty())
  {
    SEI* sei = nestedSeiMessages.front();
    nestedSeiMessages.pop_front();
    tmpMessages.push_back(sei);
    auto sn = new SEIScalableNesting();
    m_seiEncoder.initSEIScalableNesting(sn, tmpMessages, targetOLSs, targetLayers, subpicIDs, maxSubpicIdInPic);
    seiMessages.push_back(sn);
    tmpMessages.clear();
  }
}


void EncGOP::xCreateFrameFieldInfoSEI  (SEIMessages& seiMessages, Slice *slice, bool isField)
{
  if (m_pcCfg->getFrameFieldInfoSEIEnabled())
  {
    SEIFrameFieldInfo *frameFieldInfoSEI = new SEIFrameFieldInfo();

    // encode only very basic information. if more feature are supported, this should be moved to SEIEncoder
    frameFieldInfoSEI->m_fieldPicFlag = isField;
    if (isField)
    {
      frameFieldInfoSEI->m_bottomFieldFlag = !slice->getPic()->topField;
    }
    seiMessages.push_back(frameFieldInfoSEI);
  }
}

void EncGOP::xCreatePictureTimingSEI(int irapGopId, SEIMessages &seiMessages, SEIMessages &nestedSeiMessages,
                                     SEIMessages &duInfoSeiMessages, Slice *slice, bool isField,
                                     std::deque<DUData> &duData)
{
  // Picture timing depends on buffering period. When either of those is not disabled,
  // initialization would fail. Needs more cleanup after DU timing is integrated.
  if (!(m_pcCfg->getPictureTimingSEIEnabled() && m_pcCfg->getBufferingPeriodSEIEnabled()))
  {
    return;
  }

  const GeneralHrdParams *hrd = slice->getSPS()->getGeneralHrdParameters();

  // update decoding unit parameters
  if ((m_pcCfg->getPictureTimingSEIEnabled() || m_pcCfg->getDecodingUnitInfoSEIEnabled()) && slice->getNalUnitLayerId() == slice->getVPS()->getLayerId(0))
  {
    auto pt = new SEIPictureTiming();

    // DU parameters
    if( hrd->getGeneralDecodingUnitHrdParamsPresentFlag() )
    {
      pt->setNumDecodingUnits(duData.size());
      pt->duCommonCpbRemovalDelay = false;
    }
    const uint32_t cpbRemovalDelayLength = m_HRD->getBufferingPeriodSEI()->cpbRemovalDelayLength;
    const uint32_t cpbMaxRemovalDelay    = 1 << cpbRemovalDelayLength;
    const uint32_t maxNumSubLayers = slice->getSPS()->getMaxTLayers();
    const int      cpbRemovalDelay       = m_totalCoded[maxNumSubLayers - 1] - m_lastBPSEI[maxNumSubLayers - 1];
    CHECK(cpbRemovalDelay > cpbMaxRemovalDelay,
          " cpbRemovalDelayLength too small for cpbRemovalDelay[pt_max_sub_layers_minus1] at picture timing SEI ");
    pt->cpbRemovalDelay[maxNumSubLayers - 1] = std::max<int>(1, cpbRemovalDelay);
    const uint32_t temporalId = slice->getTLayer();
    if (maxNumSubLayers == 1)
    {
      pt->hasSublayerDelays[0] = true;
    }
    for( int i = temporalId ; i < maxNumSubLayers - 1 ; i ++ )
    {
      int indexWithinGOP = (m_totalCoded[maxNumSubLayers - 1] - m_lastBPSEI[maxNumSubLayers - 1]) % m_pcCfg->getGOPSize();
      pt->hasSublayerDelays[i] = true;
      if( ((m_rapWithLeading == true) && (indexWithinGOP == 0)) || (m_totalCoded[maxNumSubLayers - 1] == 0) || m_bufferingPeriodSEIPresentInAU || (slice->getPOC() + m_pcCfg->getGOPSize()) > m_pcCfg->getFramesToBeEncoded() )
      {
        pt->cpbRemovalDelayDeltaEnabled[i] = false;
      }
      else
      {
        pt->cpbRemovalDelayDeltaEnabled[i] = m_HRD->getBufferingPeriodSEI()->hasCpbRemovalDelayDeltas();
      }
      if (pt->cpbRemovalDelayDeltaEnabled[i])
      {
        if( m_rapWithLeading == false )
        {
          switch (m_pcCfg->getGOPSize())
          {
            case 8:
            {
              if((indexWithinGOP == 1 && i == 2))
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 0;
              }
              else if((indexWithinGOP == 2 && i == 2) || (indexWithinGOP == 6 && i == 2))
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 1;
              }
              else if((indexWithinGOP == 1 && i == 1) || (indexWithinGOP == 3 && i == 2))
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 2;
              }
              else if(indexWithinGOP == 2 && i == 1)
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 3;
              }
              else if(indexWithinGOP == 1 && i == 0)
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 4;
              }
              else
              {
                THROW("cpbRemovalDelayDeltaIdx not applicable for the sub-layer and GOP size");
              }
            }
              break;
            case 16:
            {
              if((indexWithinGOP == 1 && i == 3))
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 0;
              }
              else if((indexWithinGOP == 2 && i == 3) || (indexWithinGOP == 10 && i == 3) || (indexWithinGOP == 14 && i == 3))
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 1;
              }
              else if((indexWithinGOP == 1 && i == 2) || (indexWithinGOP == 3 && i == 3) || (indexWithinGOP == 7 && i == 3) || (indexWithinGOP == 11 && i == 3))
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 2;
              }
              else if(indexWithinGOP == 4 && i == 3)
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 3;
              }
              else if((indexWithinGOP == 2 && i == 2) || (indexWithinGOP == 10 && i == 2))
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 4;
              }
              else if(indexWithinGOP == 1 && i == 1)
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 5;
              }
              else if(indexWithinGOP == 3 && i == 2)
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 6;
              }
              else if(indexWithinGOP == 2 && i == 1)
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 7;
              }
              else if(indexWithinGOP == 1 && i == 0)
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 8;
              }
              else
              {
                THROW("cpbRemovalDelayDeltaIdx not applicable for the sub-layer and GOP size");
              }
            }
              break;
            default:
            {
              THROW("cpbRemovalDelayDeltaIdx not supported for the current GOP size");
            }
              break;
          }
        }
        else
        {
          switch (m_pcCfg->getGOPSize())
          {
            case 8:
            {
              if((indexWithinGOP == 1 && i == 2) || (indexWithinGOP == 5 && i == 2))
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 0;
              }
              else if(indexWithinGOP == 2 && i == 2)
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 1;
              }
              else if(indexWithinGOP == 1 && i == 1)
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 2;
              }
              else
              {
                THROW("cpbRemovalDelayDeltaIdx not applicable for the sub-layer and GOP size");
              }
            }
              break;
            case 16:
            {
              if((indexWithinGOP == 1 && i == 3) || (indexWithinGOP == 9 && i == 3) || (indexWithinGOP == 13 && i == 3))
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 0;
              }
              else if((indexWithinGOP == 2 && i == 3) || (indexWithinGOP == 6 && i == 3) || (indexWithinGOP == 10 && i == 3))
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 1;
              }
              else if((indexWithinGOP == 1 && i == 2) || (indexWithinGOP == 9 && i == 2) || (indexWithinGOP == 3 && i == 3))
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 2;
              }
              else if(indexWithinGOP == 2 && i == 2)
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 3;
              }
              else if(indexWithinGOP == 1 && i == 1)
              {
                pt->cpbRemovalDelayDeltaIdx[i] = 4;
              }
              else
              {
                THROW("cpbRemovalDelayDeltaIdx not applicable for the sub-layer and GOP size");
              }
            }
              break;
            default:
            {
              THROW("cpbRemovalDelayDeltaIdx not applicable for the sub-layer and GOP size");
            }
              break;
          }
        }
      }
      else
      {
        const int scaledDistToBuffPeriod = (m_totalCoded[i] - m_lastBPSEI[i]) * (1 << (maxNumSubLayers - 1 - i));
        CHECK(scaledDistToBuffPeriod > cpbMaxRemovalDelay,
              " cpbRemovalDelayLength too small for cpbRemovalDelay[i] at picture timing SEI ");
        pt->cpbRemovalDelay[i] = std::max<int>(1, scaledDistToBuffPeriod);
      }
    }
    pt->dpbOutputDelay = slice->getSPS()->getMaxNumReorderPics(slice->getSPS()->getMaxTLayers() - 1) + slice->getPOC()
                         - m_totalCoded[maxNumSubLayers - 1];
    if (m_pcCfg->getEfficientFieldIRAPEnabled() && irapGopId > 0 && irapGopId < m_iGopSize)
    {
      // if pictures have been swapped there is likely one more picture delay on their tid. Very rough approximation
      pt->dpbOutputDelay++;
    }
    int factor = hrd->getTickDivisorMinus2() + 2;
    pt->dpbOutputDuDelay = factor * pt->dpbOutputDelay;
    if (m_bufferingPeriodSEIPresentInAU)
    {
      for( int i = temporalId ; i < maxNumSubLayers ; i ++ )
      {
        m_lastBPSEI[i] = m_totalCoded[i];
      }
      if( (slice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL)||(slice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA) )
      {
        m_rapWithLeading = true;
      }
    }


    if( m_pcCfg->getPictureTimingSEIEnabled() )
    {
      seiMessages.push_back(pt);

      if (m_pcCfg->getScalableNestingSEIEnabled() && !m_pcCfg->getSamePicTimingInAllOLS())
      {
        auto ptCopy = new SEIPictureTiming(*pt);
        nestedSeiMessages.push_back(ptCopy);
      }
    }

    if( m_pcCfg->getDecodingUnitInfoSEIEnabled() && hrd->getGeneralDecodingUnitHrdParamsPresentFlag() )
    {
      for (int i = 0; i < pt->getNumDecodingUnits(); i++)
      {
        SEIDecodingUnitInfo* dui        = new SEIDecodingUnitInfo();
        dui->decodingUnitIdx            = i;
        dui->duCpbRemovalDelayIncrement = pt->duCpbRemovalDelayIncrement[i];
        dui->hasDpbOutputDuDelay        = false;

        duInfoSeiMessages.push_back(dui);
      }
    }

    if (!m_pcCfg->getPictureTimingSEIEnabled() && pt)
    {
      delete pt;
    }
  }
}

void EncGOP::xUpdateDuData(AccessUnit &testAU, std::deque<DUData> &duData)
{
  if (duData.empty())
  {
    return;
  }
  // fix first
  uint32_t numNalUnits = (uint32_t)testAU.size();
  uint32_t numRBSPBytes = 0;
  for (AccessUnit::const_iterator it = testAU.begin(); it != testAU.end(); it++)
  {
    numRBSPBytes += uint32_t((*it)->m_nalUnitData.str().size());
  }
  duData[0].accumBitsDU += 8 * numRBSPBytes;
  duData[0].accumNalsDU += numNalUnits;

  // adapt cumulative sums for all following DUs
  // and add one DU info SEI, if enabled
  for (int i=1; i<duData.size(); i++)
  {
    if (m_pcCfg->getDecodingUnitInfoSEIEnabled())
    {
      numNalUnits  += 1;
      numRBSPBytes += 8 * 5;
    }
    duData[i].accumBitsDU += numRBSPBytes; // probably around 5 bytes
    duData[i].accumNalsDU += numNalUnits;
  }

  // The last DU may have a trailing SEI
  if (m_pcCfg->getDecodedPictureHashSEIType() != HashType::NONE)
  {
    duData.back().accumBitsDU += 8 * 20;   // probably around 20 bytes - should be further adjusted, e.g. by type
    duData.back().accumNalsDU += 1;
  }

}
void EncGOP::xUpdateTimingSEI(SEIPictureTiming* pt, std::deque<DUData>& duData, const SPS* sps)
{
  if (!pt)
  {
    return;
  }
  const GeneralHrdParams *hrd = sps->getGeneralHrdParameters();
  if( hrd->getGeneralDecodingUnitHrdParamsPresentFlag() )
  {
    int i;
    uint64_t ui64Tmp;
    uint32_t uiPrev = 0;
    uint32_t numDus                     = pt->getNumDecodingUnits();
    auto&    duCpbRemovalDelayIncrement = pt->duCpbRemovalDelayIncrement;
    uint32_t maxDiff = ( hrd->getTickDivisorMinus2() + 2 ) - 1;
    const int maxNumSubLayers            = sps->getMaxTLayers();

    pt->hasSublayerDelays.fill(false);

    for (i = 0; i < numDus; i++)
    {
      pt->numNalusInDu[i] = (i == 0) ? duData[i].accumNalsDU : duData[i].accumNalsDU - duData[i - 1].accumNalsDU;
    }

    if (numDus == 1)
    {
      duCpbRemovalDelayIncrement[0][maxNumSubLayers - 1] = 0; /* don't care */
    }
    else
    {
      duCpbRemovalDelayIncrement[numDus - 1][maxNumSubLayers - 1] = 1; /* by definition */
      uint32_t tmp = 0;
      uint32_t accum = 0;

      for (i = (numDus - 2); i >= 0; i--)
      {
        ui64Tmp =
          (((duData[numDus - 1].accumBitsDU - duData[i].accumBitsDU)
            * (sps->getGeneralHrdParameters()->getTimeScale() / sps->getGeneralHrdParameters()->getNumUnitsInTick())
            * (hrd->getTickDivisorMinus2() + 2))
           / (m_pcCfg->getTargetBitrate()));
        if( (uint32_t)ui64Tmp > maxDiff )
        {
          tmp ++;
        }
      }
      uiPrev = 0;

      uint32_t flag = 0;
      for (i = (numDus - 2); i >= 0; i--)
      {
        flag = 0;
        ui64Tmp =
          (((duData[numDus - 1].accumBitsDU - duData[i].accumBitsDU)
            * (sps->getGeneralHrdParameters()->getTimeScale() / sps->getGeneralHrdParameters()->getNumUnitsInTick())
            * (hrd->getTickDivisorMinus2() + 2))
           / (m_pcCfg->getTargetBitrate()));

        if( (uint32_t)ui64Tmp > maxDiff )
        {
          if(uiPrev >= maxDiff - tmp)
          {
            ui64Tmp = uiPrev + 1;
            flag = 1;
          }
          else
          {
            ui64Tmp = maxDiff - tmp + 1;
          }
        }

        const bool cond = uiPrev >= ui64Tmp;

        duCpbRemovalDelayIncrement[i][maxNumSubLayers - 1] = cond ? 1 : (uint32_t) ui64Tmp - uiPrev;
        if (!cond && tmp > 0 && flag == 1)
        {
          tmp --;
        }
        accum += duCpbRemovalDelayIncrement[i][maxNumSubLayers - 1];
        uiPrev = accum;
      }
    }
  }
}

void EncGOP::xUpdateDuInfoSEI(SEIMessages& duInfoSeiMessages, SEIPictureTiming* pt)
{
  if (pt == nullptr)
  {
    return;
  }

  int i = 0;
  for (auto sei: duInfoSeiMessages)
  {
    SEIDecodingUnitInfo* dui        = (SEIDecodingUnitInfo*) sei;
    dui->decodingUnitIdx            = i;
    dui->hasSublayerDelays          = pt->hasSublayerDelays;
    dui->duCpbRemovalDelayIncrement = pt->duCpbRemovalDelayIncrement[i];
    dui->hasDpbOutputDuDelay        = false;
    i++;
  }
}

static void
validateMinCrRequirements(const ProfileTierLevelFeatures &plt, std::size_t numBytesInVclNalUnits, const Picture *pPic, const EncCfg *pCfg)
{
  //  numBytesInVclNalUnits shall be less than or equal to
  //     FormatCapabilityFactor * MaxLumaSr * framePeriod / MinCr,
  //     ( = FormatCapabilityFactor * MaxLumaSr / (MinCr * frameRate),
  if (plt.getTierLevelFeatures() && plt.getProfileFeatures() && plt.getTierLevelFeatures()->level!=Level::LEVEL15_5)
  {
    const uint32_t formatCapabilityFactorx1000 = plt.getProfileFeatures()->formatCapabilityFactorx1000;
    const uint64_t maxLumaSr = plt.getTierLevelFeatures()->maxLumaSr;
    const double   frameRate                   = pCfg->getFrameRate().getFloatVal();
    const double   minCr = plt.getMinCr();
    const double   denominator                 = minCr * frameRate * 1000;
    if (denominator!=0)
    {
      const double   threshold =(formatCapabilityFactorx1000 * maxLumaSr) / (denominator);

      if (numBytesInVclNalUnits > threshold)
      {
        msg( WARNING, "WARNING: Encoded stream does not meet MinCr requirements numBytesInVclNalUnits (%.0f) must be <= %.0f. Try increasing Qp, tier or level\n",
                      (double) numBytesInVclNalUnits, threshold );
      }
    }
  }
}

static void validateMinCrRequirements(const ProfileTierLevelFeatures& plt, std::size_t numBytesInVclNalUnits,
                                      const Slice* pSlice, const EncCfg* pCfg, const SEISubpictureLevelInfo& sli,
                                      const int subPicIdx, const int layerId)
{
  if (plt.getTierLevelFeatures() && plt.getProfileFeatures())
  {
    if (plt.getTier() == Level::Tier::MAIN)
    {
      const uint32_t formatCapabilityFactorx1000 = plt.getProfileFeatures()->formatCapabilityFactorx1000;
      const uint64_t maxLumaSr = plt.getTierLevelFeatures()->maxLumaSr;
      const double   denomx1000x256 = 256 * plt.getMinCr() * pCfg->getFrameRate().getFloatVal() * 1000 * 256;

      for (int i = 0; i < sli.numRefLevels(); i++)
      {
        const Level::Name level = sli.refLevelIdc(i, layerId);
        if (level != Level::LEVEL15_5)
        {
          const int nonSubpicLayersFraction = sli.nonSubpicLayerFraction(i, layerId);
          const int refLevelFraction        = sli.refLevelFraction(i, subPicIdx, layerId)
                                       + 1;   // m_refLevelFraction is actually sli_ref_level_fraction_minus1
          const uint32_t olsRefLevelFractionx256 = nonSubpicLayersFraction * 256 + (256 - nonSubpicLayersFraction) * refLevelFraction;

          const double   threshold = formatCapabilityFactorx1000 * maxLumaSr * olsRefLevelFractionx256 / denomx1000x256;

          if (numBytesInVclNalUnits > threshold)
          {
            msg( WARNING, "WARNING: Encoded stream for sub-picture %d does not meet MinCr requirements numBytesInVclNalUnits (%.0f) must be <= %.0f. Try increasing Qp, tier or level\n",
                      subPicIdx, (double) numBytesInVclNalUnits, threshold );
          }
        }
      }
    }
  }
}

static std::size_t
cabac_zero_word_padding(const Slice *const pcSlice,
                        const Picture *const pcPic,
                        const std::size_t binCountsInNalUnits,
                        const std::size_t numBytesInVclNalUnits,
                        const std::size_t numZeroWordsAlreadyInserted,
                              std::ostringstream &nalUnitData,
                        const bool cabacZeroWordPaddingEnabled,
                        const ProfileTierLevelFeatures &plt)
{
  const SPS &sps=*(pcSlice->getSPS());
  const ChromaFormat format = sps.getChromaFormatIdc();
  const int log2subWidthCxsubHeightC = (::getComponentScaleX(COMPONENT_Cb, format)+::getComponentScaleY(COMPONENT_Cb, format));
  const int minCuWidth  = 1 << pcSlice->getSPS()->getLog2MinCodingBlockSize();
  const int minCuHeight = 1 << pcSlice->getSPS()->getLog2MinCodingBlockSize();
  const int paddedWidth = ( ( pcSlice->getPPS()->getPicWidthInLumaSamples() + minCuWidth - 1 ) / minCuWidth ) * minCuWidth;
  const int paddedHeight = ( ( pcSlice->getPPS()->getPicHeightInLumaSamples() + minCuHeight - 1 ) / minCuHeight ) * minCuHeight;
  const int rawBits =
    paddedWidth * paddedHeight
    * (sps.getBitDepth(ChannelType::LUMA) + ((2 * sps.getBitDepth(ChannelType::CHROMA)) >> log2subWidthCxsubHeightC));
  const int vclByteScaleFactor_x3 = ( 32 + 4 * (plt.getTier()==Level::HIGH ? 1 : 0) );
  const std::size_t threshold = (vclByteScaleFactor_x3*numBytesInVclNalUnits/3) + (rawBits/32);
  // "The value of BinCountsInPicNalUnits shall be less than or equal to vclByteScaleFactor * NumBytesInPicVclNalUnits     + ( RawMinCuBits * PicSizeInMinCbsY ) / 32."
  //               binCountsInNalUnits                  <=               vclByteScaleFactor_x3 * numBytesInVclNalUnits / 3 +   rawBits / 32.
  // If it is currently not, then add cabac_zero_words to increase numBytesInVclNalUnits.
  if (binCountsInNalUnits >= threshold)
  {
    // need to add additional cabac zero words (each one accounts for 3 bytes (=00 00 03)) to increase numBytesInVclNalUnits
    const std::size_t targetNumBytesInVclNalUnits = ((binCountsInNalUnits - (rawBits/32))*3+vclByteScaleFactor_x3-1)/vclByteScaleFactor_x3;

    if (targetNumBytesInVclNalUnits>numBytesInVclNalUnits) // It should be!
    {
      const std::size_t numberOfAdditionalBytesNeeded= std::max<std::size_t>(0, targetNumBytesInVclNalUnits - numBytesInVclNalUnits - numZeroWordsAlreadyInserted * 3);
      const std::size_t numberOfAdditionalCabacZeroWords=(numberOfAdditionalBytesNeeded+2)/3;
      const std::size_t numberOfAdditionalCabacZeroBytes=numberOfAdditionalCabacZeroWords*3;
      if(numberOfAdditionalCabacZeroBytes > 0)
      {
        if (cabacZeroWordPaddingEnabled)
        {
          std::vector<uint8_t> zeroBytesPadding(numberOfAdditionalCabacZeroBytes, uint8_t(0));
          for(std::size_t i=0; i<numberOfAdditionalCabacZeroWords; i++)
          {
            zeroBytesPadding[i*3+2]=3;  // 00 00 03
          }
          nalUnitData.write(reinterpret_cast<const char*>(&(zeroBytesPadding[0])), numberOfAdditionalCabacZeroBytes);
          msg( NOTICE, "Adding %d bytes of padding\n", uint32_t( numberOfAdditionalCabacZeroWords * 3 ) );
        }
        else
        {
          msg( NOTICE, "Standard would normally require adding %d bytes of padding\n", uint32_t( numberOfAdditionalCabacZeroWords * 3 ) );
        }
      }
      return numberOfAdditionalCabacZeroWords;
    }
  }
  return 0;
}

class EfficientFieldIRAPMapping
{
  private:
    int  irapGopId;
    bool IRAPtoReorder;
    bool swapIRAPForward;

  public:
    EfficientFieldIRAPMapping() : irapGopId(-1), IRAPtoReorder(false), swapIRAPForward(false) {}

    void initialize(const bool isField, const int gopSize, const int POCLast, const int numPicRcvd, const int lastIDR, EncGOP *pEncGop, EncCfg *pCfg);

    int adjustGOPid(const int gopID);
    int restoreGOPid(const int gopID);
    int GetIRAPGOPid() const { return irapGopId; }
};

void EfficientFieldIRAPMapping::initialize(const bool isField, const int gopSize, const int POCLast, const int numPicRcvd, const int lastIDR, EncGOP *pEncGop, EncCfg *pCfg )
{
  if(isField)
  {
    int pocCurr;
    for (int gopId = 0; gopId < gopSize; gopId++)
    {
      // determine actual POC
      if(POCLast == 0) //case first frame or first top field
      {
        pocCurr=0;
      }
      else if(POCLast == 1 && isField) //case first bottom field, just like the first frame, the poc computation is not right anymore, we set the right value
      {
        pocCurr = 1;
      }
      else
      {
        pocCurr = POCLast - numPicRcvd + pCfg->getGOPEntry(gopId).m_POC - isField;
      }

      // check if POC corresponds to IRAP
      NalUnitType tmpUnitType = pEncGop->getNalUnitType(pocCurr, lastIDR, isField);
      if (tmpUnitType >= NAL_UNIT_CODED_SLICE_IDR_W_RADL && tmpUnitType <= NAL_UNIT_CODED_SLICE_CRA) // if picture is an IRAP
      {
        if (pocCurr % 2 == 0 && gopId < gopSize - 1
            && pCfg->getGOPEntry(gopId).m_POC == pCfg->getGOPEntry(gopId + 1).m_POC - 1)
        { // if top field and following picture in enc order is associated bottom field
          irapGopId       = gopId;
          IRAPtoReorder = true;
          swapIRAPForward = true;
          break;
        }
        if (pocCurr % 2 != 0 && gopId > 0 && pCfg->getGOPEntry(gopId).m_POC == pCfg->getGOPEntry(gopId - 1).m_POC + 1)
        {
          // if picture is an IRAP remember to process it first
          irapGopId       = gopId;
          IRAPtoReorder = true;
          swapIRAPForward = false;
          break;
        }
      }
    }
  }
}

int EfficientFieldIRAPMapping::adjustGOPid(const int gopId)
{
  if(IRAPtoReorder)
  {
    if(swapIRAPForward)
    {
      if (gopId == irapGopId)
      {
        return irapGopId + 1;
      }
      else if (gopId == irapGopId + 1)
      {
        return irapGopId;
      }
    }
    else
    {
      if (gopId == irapGopId - 1)
      {
        return irapGopId;
      }
      else if (gopId == irapGopId)
      {
        return irapGopId - 1;
      }
    }
  }
  return gopId;
}

int EfficientFieldIRAPMapping::restoreGOPid(const int gopId)
{
  if(IRAPtoReorder)
  {
    if(swapIRAPForward)
    {
      if (gopId == irapGopId)
      {
        IRAPtoReorder = false;
        return irapGopId + 1;
      }
      else if (gopId == irapGopId + 1)
      {
        return gopId - 1;
      }
    }
    else
    {
      if (gopId == irapGopId)
      {
        return irapGopId - 1;
      }
      else if (gopId == irapGopId - 1)
      {
        IRAPtoReorder = false;
        return irapGopId;
      }
    }
  }
  return gopId;
}


static void
printHash(const HashType hashType, const std::string &digestStr)
{
  const char *decodedPictureHashModeName;
  switch (hashType)
  {
  case HashType::MD5:
    decodedPictureHashModeName = "MD5";
    break;
  case HashType::CRC:
    decodedPictureHashModeName = "CRC";
    break;
  case HashType::CHECKSUM:
    decodedPictureHashModeName = "Checksum";
    break;
  default:
    decodedPictureHashModeName = nullptr;
    break;
  }
  if (decodedPictureHashModeName != nullptr)
  {
    if (digestStr.empty())
    {
      msg( NOTICE, " [%s:%s]", decodedPictureHashModeName, "?");
    }
    else
    {
      msg( NOTICE, " [%s:%s]", decodedPictureHashModeName, digestStr.c_str());
    }
  }
}

bool isPicEncoded( int targetPoc, int curPoc, int curTLayer, int gopSize, int intraPeriod )
{
  const int tarGop = targetPoc / gopSize;
  const int curGop = curPoc / gopSize;

  if( tarGop + 1 == curGop )
  {
    // part of next GOP only for tl0 pics
    return curTLayer == 0;
  }

  const int tarIFr = (targetPoc / intraPeriod) * intraPeriod;
  const int curIFr = (curPoc / intraPeriod) * intraPeriod;

  if( curIFr != tarIFr )
  {
    return false;
  }

  int  tarId = targetPoc - tarGop * gopSize;

  if( tarGop > curGop )
  {
    return ( tarId == 0 ) ? ( 0 == curTLayer ) : ( 1 >= curTLayer );
  }

  if( tarGop + 1 < curGop )
  {
    return false;
  }

  int  curId = curPoc - curGop * gopSize;
  int  tarTL = 0;

  while( tarId != 0 )
  {
    gopSize /= 2;
    if( tarId >= gopSize )
    {
      tarId -= gopSize;
      if( curId != 0 ) curId -= gopSize;
    }
    else if( curId == gopSize )
    {
      curId = 0;
    }
    tarTL++;
  }

  return curTLayer <= tarTL && curId == 0;
}

void trySkipOrDecodePicture(bool &decPic, bool &encPic, const EncCfg &cfg, Picture *pcPic,
                            EnumArray<ParameterSetMap<APS>, ApsType> *apsMap)
{
  // check if we should decode a leading bitstream
  if( !cfg.getDecodeBitstream( 0 ).empty() )
  {
    static bool firstTime = true;
    static bool doDecode1stPart[MAX_VPS_LAYERS] ; /* TODO: MT */
    const int layerIdx = (pcPic->cs->vps == nullptr) ? 0 : pcPic->cs->vps->getGeneralLayerIdx(pcPic->layerId);
    if (firstTime)
    {
      firstTime = false;
      std::fill_n(doDecode1stPart, MAX_VPS_LAYERS, true);
    }
    if(doDecode1stPart[layerIdx])
    {
      if( cfg.getForceDecodeBitstream1() )
      {
        if( (doDecode1stPart[layerIdx] = tryDecodePicture( pcPic, pcPic->getPOC(), cfg.getDecodeBitstream( 0 ), layerIdx, apsMap, false ) ) )
        {
          decPic = doDecode1stPart[layerIdx];
        }
      }
      else
      {
        // update decode decision
        bool dbgCTU = cfg.getDebugCTU() != -1 && cfg.getSwitchPOC() == pcPic->getPOC();

        if( (doDecode1stPart[layerIdx] = ( cfg.getSwitchPOC() != pcPic->getPOC() ) || dbgCTU ) && (doDecode1stPart[layerIdx] = tryDecodePicture( pcPic, pcPic->getPOC(), cfg.getDecodeBitstream( 0 ), layerIdx, apsMap, false, cfg.getDebugCTU(), cfg.getSwitchPOC() ) ) )
        {
          if( dbgCTU )
          {
            encPic = true;
            decPic = false;
            doDecode1stPart[layerIdx] = false;

            return;
          }
          decPic = doDecode1stPart[layerIdx];
          return;
        }
        else if( pcPic->getPOC() )
        {
          // reset decoder if used and not required any further
          tryDecodePicture(nullptr, 0, std::string(""), layerIdx);
        }
      }
    }

    encPic |= cfg.getForceDecodeBitstream1() && !decPic;
    if (cfg.getForceDecodeBitstream1())
    {
      return;
    }
  }


  // check if we should decode a trailing bitstream
  if( ! cfg.getDecodeBitstream(1).empty() )
  {
    const int  iNextKeyPOC    = (1+cfg.getSwitchPOC()  / cfg.getGOPSize())     *cfg.getGOPSize();
    const int  iNextIntraPOC  = (1+(cfg.getSwitchPOC() / cfg.getIntraPeriod()))*cfg.getIntraPeriod();
    const int  iRestartIntraPOC   = iNextIntraPOC + (((iNextKeyPOC == iNextIntraPOC) && cfg.getSwitchDQP() ) ? cfg.getIntraPeriod() : 0);

    bool bDecode2ndPart = (pcPic->getPOC() >= iRestartIntraPOC);
    int expectedPoc = pcPic->getPOC();
    Slice slice0;
    const int layerIdx = (pcPic->cs->vps == nullptr) ? 0 : pcPic->cs->vps->getGeneralLayerIdx(pcPic->layerId);
    if ( cfg.getBs2ModPOCAndType() )
    {
      expectedPoc = pcPic->getPOC() - iRestartIntraPOC;
      slice0.copySliceInfo( pcPic->slices[ 0 ], false );
    }
    if( bDecode2ndPart && (bDecode2ndPart = tryDecodePicture( pcPic, expectedPoc, cfg.getDecodeBitstream(1), layerIdx, apsMap, true )) )
    {
      decPic = bDecode2ndPart;
      if ( cfg.getBs2ModPOCAndType() )
      {
        for( int i = 0; i < pcPic->slices.size(); i++ )
        {
          pcPic->slices[ i ]->setPOC              ( slice0.getPOC()            );
          if ( pcPic->slices[ i ]->getNalUnitType() != slice0.getNalUnitType()
              && pcPic->slices[ i ]->getIdrPicFlag()
              && slice0.getRapPicFlag()
              && slice0.isIntra() )
          {
            // patch IDR-slice to CRA-Intra-slice
            pcPic->slices[ i ]->setNalUnitType    ( slice0.getNalUnitType()    );
            pcPic->slices[ i ]->setLastIDR        ( slice0.getLastIDR()        );
            if ( pcPic->cs->picHeader->getEnableTMVPFlag() )
            {
              pcPic->slices[ i ]->setColFromL0Flag( slice0.getColFromL0Flag()  );
              pcPic->slices[ i ]->setColRefIdx    ( slice0.getColRefIdx()      );
            }
          }
        }
      }
      return;
    }
  }

  // leave here if we do not use forward to poc
  if( ! cfg.useFastForwardToPOC() )
  {
    // let's encode
    encPic   = true;
    return;
  }

  // this is the forward to poc section
  static bool bHitFastForwardPOC = false; /* TODO: MT */
  if( bHitFastForwardPOC || isPicEncoded( cfg.getFastForwardToPOC(), pcPic->getPOC(), pcPic->temporalId, cfg.getGOPSize(), cfg.getIntraPeriod() ) )
  {
    bHitFastForwardPOC |= cfg.getFastForwardToPOC() == pcPic->getPOC(); // once we hit the poc we continue encoding

    if( bHitFastForwardPOC && cfg.getStopAfterFFtoPOC() && cfg.getFastForwardToPOC() != pcPic->getPOC() )
    {
      return;
    }

    //except if FastForwardtoPOC is meant to be a SwitchPOC in thist case drop all preceding pictures
    if( bHitFastForwardPOC && ( cfg.getSwitchPOC() == cfg.getFastForwardToPOC() ) && ( cfg.getFastForwardToPOC() > pcPic->getPOC() ) )
    {
      return;
    }
    // let's encode
    encPic   = true;
  }
}

void EncGOP::xPicInitHashME( Picture *pic, const PPS *pps, PicList &rcListPic )
{
  if (!m_modeCtrl->getUseHashME())
  {
    return;
  }

  PicList::iterator iterPic = rcListPic.begin();
  while (iterPic != rcListPic.end())
  {
    Picture* refPic = *(iterPic++);

    if (refPic->reconstructed && refPic->referenced && refPic->poc != pic->poc && refPic->layerId == pic->layerId)
    {
      bool validPOC = ((refPic->getPOC() == m_modeCtrl->getUseHashMEPOCToCheck()) && !m_modeCtrl->getUseHashMEPOCChecked());
      if (!refPic->getHashMap()->isInitial() || validPOC)
      {
        if (validPOC)
        {
          m_modeCtrl->setUseHashMEPOCChecked(true);
          Pel* picSrc = refPic->getOrigBuf().get(COMPONENT_Y).buf;
          ptrdiff_t stridePic = refPic->getOrigBuf().get(COMPONENT_Y).stride;
          int picWidth = refPic->lwidth();
          int picHeight = refPic->lheight();
          int blockSize = 4;
          int allNum = 0;
          int simpleNum = 0;
          for (int j = 0; j <= picHeight - blockSize; j += blockSize)
          {
            for (int i = 0; i <= picWidth - blockSize; i += blockSize)
            {
              Pel* curBlock = picSrc + j * stridePic + i;
              bool isHorSame = true;
              for (int m = 0; m < blockSize&&isHorSame; m++)
              {
                for (int n = 1; n < blockSize&&isHorSame; n++)
                {
                  if (curBlock[m*stridePic] != curBlock[m*stridePic + n])
                  {
                    isHorSame = false;
                  }
                }
              }
              bool isVerSame = true;
              for (int m = 1; m < blockSize&&isVerSame; m++)
              {
                for (int n = 0; n < blockSize&&isVerSame; n++)
                {
                  if (curBlock[n] != curBlock[m*stridePic + n])
                  {
                    isVerSame = false;
                  }
                }
              }
              allNum++;
              if (isHorSame || isVerSame)
              {
                simpleNum++;
              }
            }
          }

          if (simpleNum < 0.3*allNum)
          {
            m_modeCtrl->setUseHashME(false);
            break;
          }
        }
        refPic->addPictureToHashMapForInter();
      }
    }
  }
}

void EncGOP::xPicInitRateControl(int &estimatedBits, int gopId, double &lambda, Picture *pic, Slice *slice)
{
  if ( !m_pcCfg->getUseRateCtrl() ) // TODO: does this work with multiple slices and slice-segments?
  {
    return;
  }
  int frameLevel = m_pcRateCtrl->getRCSeq()->getGOPID2Level( gopId );
  if ( pic->slices[0]->isIRAP() )
  {
    frameLevel = 0;
  }
  m_pcRateCtrl->initRCPic( frameLevel );
  estimatedBits = m_pcRateCtrl->getRCPic()->getTargetBits();

  if (m_pcRateCtrl->getCpbSaturationEnabled() && frameLevel != 0)
  {
    int estimatedCpbFullness = m_pcRateCtrl->getCpbState() + m_pcRateCtrl->getBufferingRate();

    // prevent overflow
    if (estimatedCpbFullness - estimatedBits > (int)(m_pcRateCtrl->getCpbSize()*0.9f))
    {
      estimatedBits = estimatedCpbFullness - (int)(m_pcRateCtrl->getCpbSize()*0.9f);
    }

    estimatedCpbFullness -= m_pcRateCtrl->getBufferingRate();
    // prevent underflow
    if (estimatedCpbFullness - estimatedBits < m_pcRateCtrl->getRCPic()->getLowerBound())
    {
      estimatedBits = std::max(200, estimatedCpbFullness - m_pcRateCtrl->getRCPic()->getLowerBound());
    }

    m_pcRateCtrl->getRCPic()->setTargetBits(estimatedBits);
  }

  int sliceQP = m_pcCfg->getInitialQP();
  if ( ( slice->getPOC() == 0 && m_pcCfg->getInitialQP() > 0 ) || ( frameLevel == 0 && m_pcCfg->getForceIntraQP() ) ) // QP is specified
  {
    int    NumberBFrames = ( m_pcCfg->getGOPSize() - 1 );
    double dLambda_scale = 1.0 - Clip3( 0.0, 0.5, 0.05*(double)NumberBFrames );
    double dQPFactor     = 0.57*dLambda_scale;
    int    SHIFT_QP      = 12;
    int    bitdepth_luma_qp_scale = 6
                                 * (slice->getSPS()->getBitDepth(ChannelType::LUMA) - 8
                                    - DISTORTION_PRECISION_ADJUSTMENT(slice->getSPS()->getBitDepth(ChannelType::LUMA)));
    double qp_temp = (double) sliceQP + bitdepth_luma_qp_scale - SHIFT_QP;
    lambda = dQPFactor*pow( 2.0, qp_temp/3.0 );
  }
  else if ( frameLevel == 0 )   // intra case, but use the model
  {
    m_pcSliceEncoder->calCostPictureI(pic);
    if ( m_pcCfg->getIntraPeriod() != 1 )   // do not refine allocated bits for all intra case
    {
      int bits = m_pcRateCtrl->getRCSeq()->getLeftAverageBits();
      bits = m_pcRateCtrl->getRCPic()->getRefineBitsForIntra( bits );

      if (m_pcRateCtrl->getCpbSaturationEnabled() )
      {
        int estimatedCpbFullness = m_pcRateCtrl->getCpbState() + m_pcRateCtrl->getBufferingRate();

        // prevent overflow
        if (estimatedCpbFullness - bits > (int)(m_pcRateCtrl->getCpbSize()*0.9f))
        {
          bits = estimatedCpbFullness - (int)(m_pcRateCtrl->getCpbSize()*0.9f);
        }

        estimatedCpbFullness -= m_pcRateCtrl->getBufferingRate();
        // prevent underflow
        if (estimatedCpbFullness - bits < m_pcRateCtrl->getRCPic()->getLowerBound())
        {
          bits = estimatedCpbFullness - m_pcRateCtrl->getRCPic()->getLowerBound();
        }
      }

      if ( bits < 200 )
      {
        bits = 200;
      }
      m_pcRateCtrl->getRCPic()->setTargetBits( bits );
    }

    std::list<EncRCPic *> listPreviousPicture = m_pcRateCtrl->getPicList();
    m_pcRateCtrl->getRCPic()->getLCUInitTargetBits();
    lambda  = m_pcRateCtrl->getRCPic()->estimatePicLambda( listPreviousPicture, slice->isIRAP());
    sliceQP = m_pcRateCtrl->getRCPic()->estimatePicQP( lambda, listPreviousPicture );
  }
  else    // normal case
  {
    std::list<EncRCPic *> listPreviousPicture = m_pcRateCtrl->getPicList();
    lambda  = m_pcRateCtrl->getRCPic()->estimatePicLambda( listPreviousPicture, slice->isIRAP());
    sliceQP = m_pcRateCtrl->getRCPic()->estimatePicQP( lambda, listPreviousPicture );
  }

  sliceQP = Clip3(-slice->getSPS()->getQpBDOffset(ChannelType::LUMA), MAX_QP, sliceQP);
  m_pcRateCtrl->getRCPic()->setPicEstQP( sliceQP );

  m_pcSliceEncoder->resetQP( pic, sliceQP, lambda );
}

void EncGOP::xPicInitLMCS(Picture *pic, PicHeader *picHeader, Slice *slice)
{
  if (slice->getSPS()->getUseLmcs())
  {
    const SliceType realSliceType = slice->getSliceType();
    SliceType condSliceType = realSliceType;

    if (condSliceType != I_SLICE && slice->getNalUnitLayerId() > 0 && (slice->getNalUnitType()>= NAL_UNIT_CODED_SLICE_IDR_W_RADL && slice->getNalUnitType()<= NAL_UNIT_CODED_SLICE_CRA))
    {
      condSliceType = I_SLICE;
    }
    m_pcReshaper->getReshapeCW()->rspTid = slice->getTLayer() + (slice->isIntra() ? 0 : 1);
    m_pcReshaper->getReshapeCW()->rspSliceQP = slice->getSliceQp();

    m_pcReshaper->setSrcReshaped(false);
    m_pcReshaper->setRecReshaped(true);

    m_pcReshaper->getSliceReshaperInfo().chrResScalingOffset = m_pcCfg->getReshapeCSoffset();

    if (m_pcCfg->getReshapeSignalType() == RESHAPE_SIGNAL_PQ)
    {
      m_pcReshaper->preAnalyzerHDR(pic, condSliceType, m_pcCfg->getReshapeCW(), m_pcCfg->getDualITree());
    }
    else if (m_pcCfg->getReshapeSignalType() == RESHAPE_SIGNAL_SDR || m_pcCfg->getReshapeSignalType() == RESHAPE_SIGNAL_HLG)
    {
      m_pcReshaper->preAnalyzerLMCS(pic, m_pcCfg->getReshapeSignalType(), condSliceType, m_pcCfg->getReshapeCW());
    }
    else
    {
      THROW("Reshaper for other signal currently not defined!");
    }

    if (condSliceType == I_SLICE )
    {
      if (m_pcCfg->getReshapeSignalType() == RESHAPE_SIGNAL_PQ)
      {
        m_pcReshaper->initLUTfromdQPModel();
        m_pcEncLib->getRdCost()->updateReshapeLumaLevelToWeightTableChromaMD(m_pcReshaper->getInvLUT());
      }
      else if (m_pcCfg->getReshapeSignalType() == RESHAPE_SIGNAL_SDR || m_pcCfg->getReshapeSignalType() == RESHAPE_SIGNAL_HLG)
      {
        if (m_pcReshaper->getReshapeFlag())
        {
          m_pcReshaper->constructReshaperLMCS();
          m_pcEncLib->getRdCost()->updateReshapeLumaLevelToWeightTable(m_pcReshaper->getSliceReshaperInfo(), m_pcReshaper->getWeightTable(), m_pcReshaper->getCWeight());
        }
      }
      else
      {
        THROW("Reshaper for other signal currently not defined!");
      }

      m_pcReshaper->setCTUFlag(false);
      if (realSliceType != condSliceType)
      {
        m_pcReshaper->setCTUFlag(true);
      }
    }
    else
    {
      if (!m_pcReshaper->getReshapeFlag())
      {
        m_pcReshaper->setCTUFlag(false);
      }
      else
      {
        m_pcReshaper->setCTUFlag(true);
      }

      m_pcReshaper->getSliceReshaperInfo().setSliceReshapeModelPresentFlag(false);

      if (m_pcCfg->getReshapeSignalType() == RESHAPE_SIGNAL_PQ)
      {
        m_pcEncLib->getRdCost()->restoreReshapeLumaLevelToWeightTable();
      }
      else if (m_pcCfg->getReshapeSignalType() == RESHAPE_SIGNAL_SDR || m_pcCfg->getReshapeSignalType() == RESHAPE_SIGNAL_HLG)
      {
        int modIP = pic->getPOC() - pic->getPOC() / m_pcCfg->getReshapeCW().rspFpsToIp * m_pcCfg->getReshapeCW().rspFpsToIp;
#if GDR_ENABLED
        if (slice->getSPS()->getGDREnabledFlag() && slice->isInterGDR())
        {
          modIP = 0;
        }
#endif
        if (m_pcReshaper->getReshapeFlag() && m_pcCfg->getReshapeCW().updateCtrl == 2 && modIP == 0)
        {
          m_pcReshaper->getSliceReshaperInfo().setSliceReshapeModelPresentFlag(true);
          m_pcReshaper->constructReshaperLMCS();
          m_pcEncLib->getRdCost()->updateReshapeLumaLevelToWeightTable(m_pcReshaper->getSliceReshaperInfo(), m_pcReshaper->getWeightTable(), m_pcReshaper->getCWeight());
        }
      }
      else
      {
        THROW("Reshaper for other signal currently not defined!");
      }
    }

    //set all necessary information in LMCS APS and picture header
    picHeader->setLmcsEnabledFlag(m_pcReshaper->getSliceReshaperInfo().getUseSliceReshaper());
    slice->setLmcsEnabledFlag(m_pcReshaper->getSliceReshaperInfo().getUseSliceReshaper());
    picHeader->setLmcsChromaResidualScaleFlag(m_pcReshaper->getSliceReshaperInfo().getSliceReshapeChromaAdj() == 1);

#if GDR_ENABLED
    if (slice->getSPS()->getGDREnabledFlag() && slice->getPic()->gdrParam.inGdrInterval)
    {
      picHeader->setLmcsChromaResidualScaleFlag(false);
    }
#endif

    if (m_pcReshaper->getSliceReshaperInfo().getSliceReshapeModelPresentFlag())
    {
      int apsId = std::min<int>( 3, m_pcEncLib->getVPS() == nullptr ? 0 : m_pcEncLib->getVPS()->getGeneralLayerIdx( m_pcEncLib->getLayerId() ) );
      picHeader->setLmcsAPSId(apsId);
      APS* lmcsAPS = picHeader->getLmcsAPS();
      if (lmcsAPS == nullptr)
      {
        ParameterSetMap<APS> *apsMap = m_pcEncLib->getApsMap(ApsType::LMCS);
        lmcsAPS                      = apsMap->getPS(apsId);
        if (lmcsAPS == nullptr)
        {
          lmcsAPS = apsMap->allocatePS(apsId);
          lmcsAPS->setAPSId(apsId);
          lmcsAPS->setAPSType(ApsType::LMCS);
        }
        picHeader->setLmcsAPS(lmcsAPS);
      }
      //m_pcReshaper->copySliceReshaperInfo(lmcsAPS->getReshaperAPSInfo(), m_pcReshaper->getSliceReshaperInfo());
      SliceReshapeInfo& tInfo = lmcsAPS->getReshaperAPSInfo();
      SliceReshapeInfo& sInfo = m_pcReshaper->getSliceReshaperInfo();
      tInfo.reshaperModelMaxBinIdx = sInfo.reshaperModelMaxBinIdx;
      tInfo.reshaperModelMinBinIdx = sInfo.reshaperModelMinBinIdx;
      memcpy(tInfo.reshaperModelBinCWDelta, sInfo.reshaperModelBinCWDelta, sizeof(int)*(PIC_CODE_CW_BINS));
      tInfo.maxNbitsNeededDeltaCW = sInfo.maxNbitsNeededDeltaCW;
      tInfo.chrResScalingOffset = sInfo.chrResScalingOffset;
      m_pcEncLib->getApsMap(ApsType::LMCS)->setChangedFlag(lmcsAPS->getAPSId());
    }


    if (picHeader->getLmcsEnabledFlag())
    {
      const int apsId = std::min<int>(
        3, m_pcEncLib->getVPS() == nullptr ? 0 : m_pcEncLib->getVPS()->getGeneralLayerIdx(m_pcEncLib->getLayerId()));
      picHeader->setLmcsAPSId(apsId);
    }
  }
  else
  {
    m_pcReshaper->setCTUFlag(false);
  }
}

void EncGOP::computeSignalling(Picture* pcPic, Slice* pcSlice) const
{
  bool deriveETSRC = (!pcSlice->getTSResidualCodingDisabledFlag() && pcSlice->getSPS()->getSpsRangeExtension().getTSRCRicePresentFlag());
  bool deriveRLSCP = pcSlice->getSPS()->getSpsRangeExtension().getReverseLastSigCoeffEnabledFlag();

  if (deriveETSRC || deriveRLSCP)
  {
    int total = 0;
    int ignored = 0;
    uint32_t freq[128] = {};
    static const int offsetRLSCP = 15; // Equivalent to 2.5 bits
    for (ComponentID compID = COMPONENT_Y;
         compID <= (!isChromaEnabled(pcPic->chromaFormat) ? COMPONENT_Y : COMPONENT_Cr);
         compID = ComponentID(compID + 1))
    {
      int bitDepth = pcPic->cs->sps->getBitDepth(toChannelType(compID));
      int qpBase = offsetRLSCP + 4 - (bitDepth - 8) * 6;
      int qpOffs = pcSlice->getSliceQp() - qpBase;

      const CPelBuf buffer = pcPic->getOrigBuf(compID);
      const ptrdiff_t stride = buffer.stride;
      const int height = buffer.height;
      const int width = buffer.width;
      total += (height - 1) * (width - 1);

      const Pel* buf = buffer.buf;
      for (int h = 1; h < height; h++)
      {
        const Pel* above = buf;
        buf += stride;
        for (int w = 1; w < width; w++)
        {
          Pel residual = std::min(std::abs(buf[w] - buf[w - 1]), std::abs(buf[w] - above[w]));
          if (residual > 0)
          {
            int resLevel = (int)std::round(6 * std::log2(residual)) - qpOffs;
            freq[Clip3(0, 127, resLevel)]++;
          }
          else
          {
            ignored++;
          }
        }
      }
    }

    if (deriveRLSCP)
    {
      pcSlice->setReverseLastSigCoeffFlag((freq[0] + ignored) < total / 2);
    }

    if (deriveETSRC)
    {
      total -= ignored;
      int target[3] = { total / 6, total / 3, total / 2 };
      int win[3];

      int winCount = 0;
      int totalFreq = 0;
      for (int i = 0; i < 128 && winCount < 3; i++)
      {
        totalFreq += freq[i];
        while (totalFreq >= target[winCount] && winCount < 3)
        {
           win[winCount++] = i;
        }
      }

      int winCentre = ((win[0] + win[1] * 2 + win[2]) / 4) - offsetRLSCP;
      int tsrcIndex = Clip3<int>(0, 7, winCentre / 6);
      if (ignored > total)
      {
        tsrcIndex = std::min(tsrcIndex, std::max(0, pcPic->cs->sps->getBitDepth(ChannelType::LUMA) - 9));
      }
      pcSlice->setTsrcIndex(tsrcIndex);
    }
  }
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================
void EncGOP::compressGOP(int pocLast, int numPicRcvd, PicList &rcListPic, std::list<PelUnitBuf *> &rcListPicYuvRecOut,
                         bool isField, bool isTff, const InputColourSpaceConversion snr_conversion,
                         const bool printFrameMSE, const bool printMSSSIM, bool isEncodeLtRef, const int picIdInGOP)
{
  // TODO: Split this function up.

  Picture   *pcPic     = nullptr;
  PicHeader *picHeader = nullptr;

  Slice*      pcSlice;
  OutputBitstream  *pcBitstreamRedirect;
  pcBitstreamRedirect = new OutputBitstream;
  AccessUnit::iterator  itLocationToPushSliceHeaderNALU; // used to store location where NALU containing slice header is to be inserted
  Picture* scaledRefPic[MAX_NUM_REF] = {};

  xInitGOP(pocLast, numPicRcvd, isField, isEncodeLtRef);

  m_numPicsCoded = 0;
  SEIMessages leadingSeiMessages;
  SEIMessages nestedSeiMessages;
  SEIMessages duInfoSeiMessages;
  SEIMessages trailingSeiMessages;
  std::deque<DUData> duData;

  EfficientFieldIRAPMapping effFieldIRAPMap;
  if (m_pcCfg->getEfficientFieldIRAPEnabled())
  {
    effFieldIRAPMap.initialize(isField, m_iGopSize, pocLast, numPicRcvd, m_iLastIDR, this, m_pcCfg);
  }

  if( isField && picIdInGOP == 0 )
  {
    for (int gopId = 0; gopId < std::max(2, m_iGopSize); gopId++)
    {
      m_pcCfg->setEncodedFlag(gopId, false);
    }
  }
  for (int gopId = picIdInGOP; gopId <= picIdInGOP; gopId++)
  {
    // reset flag indicating whether pictures have been encoded
    m_pcCfg->setEncodedFlag(gopId, false);
    if (m_pcCfg->getEfficientFieldIRAPEnabled())
    {
      gopId = effFieldIRAPMap.adjustGOPid(gopId);
    }

    //-- For time output for each slice
    auto beforeTime = std::chrono::steady_clock::now();


    /////////////////////////////////////////////////////////////////////////////////////////////////// Initial to start encoding
    int timeOffset;
    int pocCurr;
    int multipleFactor = m_pcCfg->getUseCompositeRef() ? 2 : 1;

    if (pocLast == 0)   // case first frame or first top field
    {
      pocCurr=0;
      timeOffset = isField ? (1 - multipleFactor) : multipleFactor;
    }
    else if (pocLast == 1 && isField)   // case first bottom field, just like the first frame, the poc computation is
                                        // not right anymore, we set the right value
    {
      pocCurr = 1;
      timeOffset = multipleFactor + 1;
    }
    else
    {
      pocCurr = pocLast - numPicRcvd * multipleFactor + m_pcCfg->getGOPEntry(gopId).m_POC
                - ((isField && m_iGopSize > 1) ? 1 : 0);
      timeOffset = m_pcCfg->getGOPEntry(gopId).m_POC;
    }

    if (m_pcCfg->getUseCompositeRef() && isEncodeLtRef)
    {
      pocCurr++;
      timeOffset--;
    }
    if (pocCurr / multipleFactor >= m_pcCfg->getFramesToBeEncoded())
    {
      if (m_pcCfg->getEfficientFieldIRAPEnabled())
      {
        gopId = effFieldIRAPMap.restoreGOPid(gopId);
      }
      continue;
    }

    if( getNalUnitType(pocCurr, m_iLastIDR, isField) == NAL_UNIT_CODED_SLICE_IDR_W_RADL || getNalUnitType(pocCurr, m_iLastIDR, isField) == NAL_UNIT_CODED_SLICE_IDR_N_LP )
    {
      m_iLastIDR = pocCurr;
    }

    // start a new access unit: create an entry in the list of output access units
    AccessUnit accessUnit;
    accessUnit.temporalId = m_pcCfg->getGOPEntry(gopId).m_temporalId;
    xGetBuffer(rcListPic, rcListPicYuvRecOut, numPicRcvd, timeOffset, pcPic, pocCurr, isField);
    picHeader = pcPic->cs->picHeader;
    picHeader->setSPSId( pcPic->cs->pps->getSPSId() );
    if( getNalUnitType(pocCurr, m_iLastIDR, isField) == NAL_UNIT_CODED_SLICE_RASL && m_pcCfg->getRprRASLtoolSwitch() && m_pcCfg->getUseWrapAround() )
    {
      picHeader->setPPSId( 4 );
      pcPic->cs->pps = m_pcEncLib->getPPS( 4 );
    }
    else
    {
      picHeader->setPPSId( pcPic->cs->pps->getPPSId() );
    }
    picHeader->setSplitConsOverrideFlag(false);
    // initial two flags to be false
    picHeader->setPicInterSliceAllowedFlag(false);
    picHeader->setPicIntraSliceAllowedFlag(false);
#if ER_CHROMA_QP_WCG_PPS
    // th this is a hot fix for the choma qp control
    if( m_pcEncLib->getWCGChromaQPControl().isEnabled() && m_pcEncLib->getSwitchPOC() != -1 )
    {
      static int usePPS = 0; /* TODO: MT */
      if( pocCurr == m_pcEncLib->getSwitchPOC() )
      {
        usePPS = 1;
      }
      const PPS *pPPS = m_pcEncLib->getPPS(usePPS);
      // replace the pps with a more appropriated one
      pcPic->cs->pps = pPPS;
    }
#endif

#if JVET_AJ0151_DSC_SEI && JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
    if (m_pcCfg->getDigitallySignedContentSEICfg().enabled)
    {
      m_dscSubstreamId = m_pcEncLib->getVPS() == nullptr ? 0 : m_pcEncLib->getVPS()->getGeneralLayerIdx(m_pcEncLib->getLayerId());
    }
#endif

    // create objects based on the picture size
    const int picWidth = pcPic->cs->pps->getPicWidthInLumaSamples();
    const int picHeight = pcPic->cs->pps->getPicHeightInLumaSamples();
    const int maxCUWidth = pcPic->cs->sps->getMaxCUWidth();
    const int maxCUHeight = pcPic->cs->sps->getMaxCUHeight();
    const ChromaFormat chromaFormatIdc = pcPic->cs->sps->getChromaFormatIdc();
    const int maxTotalCUDepth = floorLog2(maxCUWidth) - pcPic->cs->sps->getLog2MinCodingBlockSize();

    m_pcSliceEncoder->create(picWidth, picHeight, chromaFormatIdc, maxCUWidth, maxCUHeight, maxTotalCUDepth);

    const bool isCurrentFrameFiltered = m_pcCfg->getGopBasedTemporalFilterEnabled() || m_pcCfg->getBIM();
    const bool isFgFiltered = m_pcCfg->getFilmGrainAnalysisEnabled() && m_pcCfg->getFilmGrainExternalDenoised().empty();
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
    pcPic->createTempBuffers(pcPic->cs->pps->pcv->maxCUWidth, isCurrentFrameFiltered, m_pcEncLib->isResChangeInClvsEnabled() || m_pcEncLib->getPriSEIEnabled(), false, isFgFiltered);
#else
    pcPic->createTempBuffers(pcPic->cs->pps->pcv->maxCUWidth, isCurrentFrameFiltered, m_pcEncLib->isResChangeInClvsEnabled(), false, isFgFiltered);
#endif
    pcPic->getTrueOrigBuf().copyFrom(pcPic->getOrigBuf());
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
    if (m_pcEncLib->isResChangeInClvsEnabled() || m_pcEncLib->getPriSEIEnabled())
#else
    if (m_pcEncLib->isResChangeInClvsEnabled())
#endif
    {
      pcPic->M_BUFS(0, PIC_TRUE_ORIGINAL_INPUT).copyFrom(pcPic->M_BUFS(0, PIC_ORIGINAL_INPUT));
    }
    if (isFgFiltered)
    {
      pcPic->M_BUFS(0, PIC_FILTERED_ORIGINAL_FG).copyFrom(pcPic->M_BUFS(0, PIC_ORIGINAL));
      m_pcEncLib->getTemporalFilterForFG().filter(&pcPic->M_BUFS(0, PIC_FILTERED_ORIGINAL_FG), pocCurr);
    }
    if (isCurrentFrameFiltered)
    {
      if (m_pcEncLib->isResChangeInClvsEnabled())
      {
        m_pcEncLib->getTemporalFilter().filter(&pcPic->M_BUFS(0, PIC_ORIGINAL_INPUT), pocCurr);

        const Window& curScalingWindow = pcPic->getScalingWindow();
        const SPS& sps = *pcPic->cs->sps;
        const int curPicWidth = pcPic->M_BUFS(0, PIC_ORIGINAL).Y().width - SPS::getWinUnitX(sps.getChromaFormatIdc()) * (curScalingWindow.getWindowLeftOffset() + curScalingWindow.getWindowRightOffset());
        const int curPicHeight = pcPic->M_BUFS(0, PIC_ORIGINAL).Y().height - SPS::getWinUnitY(sps.getChromaFormatIdc()) * (curScalingWindow.getWindowTopOffset() + curScalingWindow.getWindowBottomOffset());
        const PPS* pps = m_pcEncLib->getPPS(0);
        const Window& refScalingWindow = pps->getScalingWindow();
        const int refPicWidth = pcPic->M_BUFS(0, PIC_ORIGINAL_INPUT).Y().width - SPS::getWinUnitX(sps.getChromaFormatIdc()) * (refScalingWindow.getWindowLeftOffset() + refScalingWindow.getWindowRightOffset());
        const int refPicHeight = pcPic->M_BUFS(0, PIC_ORIGINAL_INPUT).Y().height - SPS::getWinUnitY(sps.getChromaFormatIdc()) * (refScalingWindow.getWindowTopOffset() + refScalingWindow.getWindowBottomOffset());
        const int xScale = ((refPicWidth << ScalingRatio::BITS) + (curPicWidth >> 1)) / curPicWidth;
        const int yScale = ((refPicHeight << ScalingRatio::BITS) + (curPicHeight >> 1)) / curPicHeight;
        ScalingRatio scalingRatio = {xScale, yScale};
        Picture::rescalePicture(scalingRatio, pcPic->M_BUFS(0, PIC_ORIGINAL_INPUT), curScalingWindow, pcPic->M_BUFS(0, PIC_ORIGINAL), pps->getScalingWindow(), chromaFormatIdc, sps.getBitDepths(), true, true,
          sps.getHorCollocatedChromaFlag(), sps.getVerCollocatedChromaFlag());
      }
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
      else if (m_pcEncLib->getPriSEIEnabled())
      {
        m_pcEncLib->getTemporalFilter().filter(&pcPic->M_BUFS(0, PIC_ORIGINAL_INPUT), pocCurr);
        m_pcEncLib->getPriProcess().packRegions(pcPic->M_BUFS(0, PIC_ORIGINAL_INPUT),  m_pcEncLib->getLayerId(), pcPic->M_BUFS(0, PIC_ORIGINAL), *pcPic->cs->sps);
      }
#endif
      else
      {
        m_pcEncLib->getTemporalFilter().filter(&pcPic->M_BUFS(0, PIC_ORIGINAL), pocCurr);
      }
      pcPic->getFilteredOrigBuf().copyFrom(pcPic->getOrigBuf());
    }

    pcPic->cs->createTemporaryCsData((bool)pcPic->cs->sps->getPLTMode());

    //  Slice data initialization
    pcPic->clearSliceBuffer();
    pcPic->allocateNewSlice();
    m_pcSliceEncoder->setSliceSegmentIdx(0);

    const NalUnitType naluType = getNalUnitType(pocCurr, m_iLastIDR, isField);
    pcPic->setPictureType(naluType);
    m_pcSliceEncoder->initEncSlice(pcPic, pocLast, pocCurr, gopId, pcSlice, isField, isEncodeLtRef,
                                   m_pcEncLib->getLayerId(), naluType);

    DTRACE_UPDATE( g_trace_ctx, ( std::make_pair( "poc", pocCurr ) ) );
    DTRACE_UPDATE( g_trace_ctx, ( std::make_pair( "final", 0 ) ) );

#if !SHARP_LUMA_DELTA_QP
    //Set Frame/Field coding
    pcPic->fieldPic = isField;
#endif

    pcSlice->setLastIDR(m_iLastIDR);
    pcSlice->setIndependentSliceIdx(0);

    if (pcSlice->getSliceType() == B_SLICE && m_pcCfg->getGOPEntry(gopId).m_sliceType == 'P')
    {
      pcSlice->setSliceType(P_SLICE);
    }
    if (pcSlice->getSliceType() == B_SLICE && m_pcCfg->getGOPEntry(gopId).m_sliceType == 'I')
    {
      pcSlice->setSliceType(I_SLICE);
    }
    pcSlice->setTLayer(m_pcCfg->getGOPEntry(gopId).m_temporalId);
#if GDR_ENABLED
    if (m_pcCfg->getGdrEnabled() && pocCurr >= m_pcCfg->getGdrPocStart() && ((pocCurr - m_pcCfg->getGdrPocStart()) % m_pcCfg->getGdrPeriod() == 0))
    {
      pcSlice->setSliceType(B_SLICE);
    }

    // note : first picture is GDR(I_SLICE)
    if (m_pcCfg->getGdrEnabled() && pocCurr == 0)
    {
      pcSlice->setSliceType(I_SLICE);
    }
#endif

    // Set the nal unit type
    pcSlice->setNalUnitType(getNalUnitType(pocCurr, m_iLastIDR, isField));
    // set two flags according to slice type presented in the picture
    if (pcSlice->getSliceType() != I_SLICE)
    {
      picHeader->setPicInterSliceAllowedFlag(true);
    }
    if (pcSlice->getSliceType() == I_SLICE)
    {
      picHeader->setPicIntraSliceAllowedFlag(true);
    }
    picHeader->setGdrOrIrapPicFlag(picHeader->getGdrPicFlag() || pcSlice->isIRAP());

    if (m_pcCfg->getEfficientFieldIRAPEnabled())
    {
      if ( pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA)  // IRAP picture
      {
        m_associatedIRAPType[pcPic->layerId] = pcSlice->getNalUnitType();
        m_associatedIRAPPOC[pcPic->layerId] = pocCurr;
        if (m_pcEncLib->getEdrapIndicationSEIEnabled())
        {
          m_latestEDRAPPOC = MAX_INT;
          pcPic->setEdrapRapId(0);
        }
      }
      pcSlice->setAssociatedIRAPType(m_associatedIRAPType[pcPic->layerId]);
      pcSlice->setAssociatedIRAPPOC(m_associatedIRAPPOC[pcPic->layerId]);
    }

    pcSlice->decodingRefreshMarking(m_pocCRA, m_refreshPending, rcListPic, m_pcCfg->getEfficientFieldIRAPEnabled());
    if (m_pcCfg->getUseCompositeRef() && isEncodeLtRef)
    {
      setUseLTRef(true);
      setPrepareLTRef(false);
      setNewestBgPOC(pocCurr);
      setLastLTRefPoc(pocCurr);
    }
    else if (m_pcCfg->getUseCompositeRef() && getLastLTRefPoc() >= 0 && getEncodedLTRef() == false
             && !getPicBg()->getSpliceFull() && pocCurr - getLastLTRefPoc() > m_pcCfg->getFrameRate().getFloatVal() * 2)
    {
      setUseLTRef(false);
      setPrepareLTRef(false);
      setEncodedLTRef(true);
      setNewestBgPOC(-1);
      setLastLTRefPoc(-1);
    }

    if (m_pcCfg->getUseCompositeRef() && m_picBg->getSpliceFull() && getUseLTRef())
    {
      m_pcEncLib->selectReferencePictureList(pcSlice, pocCurr, gopId, m_bgPOC);
    }
    else
    {
      m_pcEncLib->selectReferencePictureList(pcSlice, pocCurr, gopId, -1);
    }
    if (!m_pcCfg->getEfficientFieldIRAPEnabled())
    {
      if ( pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA)  // IRAP picture
      {
        m_associatedIRAPType[pcPic->layerId] = pcSlice->getNalUnitType();
        m_associatedIRAPPOC[pcPic->layerId] = pocCurr;
        if (m_pcEncLib->getEdrapIndicationSEIEnabled())
        {
          m_latestEDRAPPOC = MAX_INT;
          pcPic->setEdrapRapId(0);
        }
      }
      pcSlice->setAssociatedIRAPType(m_associatedIRAPType[pcPic->layerId]);
      pcSlice->setAssociatedIRAPPOC(m_associatedIRAPPOC[pcPic->layerId]);
    }

    pcSlice->setEnableDRAPSEI(m_pcEncLib->getDependentRAPIndicationSEIEnabled());
    if (m_pcEncLib->getDependentRAPIndicationSEIEnabled())
    {
      // Only mark the picture as DRAP if all of the following applies:
      //  1) DRAP indication SEI messages are enabled
      //  2) The current picture is not an intra picture
      //  3) The current picture is in the DRAP period
      //  4) The current picture is a trailing picture
      pcSlice->setDRAP(m_pcEncLib->getDependentRAPIndicationSEIEnabled() && m_pcEncLib->getDrapPeriod() > 0 && !pcSlice->isIntra() &&
              pocCurr % m_pcEncLib->getDrapPeriod() == 0 && pocCurr > pcSlice->getAssociatedIRAPPOC());

      if (pcSlice->isDRAP())
      {
        int pocCycle = 1 << (pcSlice->getSPS()->getBitsForPOC());
        int deltaPOC = pocCurr > pcSlice->getAssociatedIRAPPOC() ? pocCurr - pcSlice->getAssociatedIRAPPOC() : pocCurr - ( pcSlice->getAssociatedIRAPPOC() & (pocCycle -1) );
        CHECK(deltaPOC > (pocCycle >> 1), "Use a greater value for POC wraparound to enable a POC distance between IRAP and DRAP of " << deltaPOC << ".");
        m_latestDRAPPOC = pocCurr;
        pcSlice->setTLayer(0); // Force DRAP picture to have temporal layer 0
      }
      pcSlice->setLatestDRAPPOC(m_latestDRAPPOC);
      pcSlice->setUseLTforDRAP(false); // When set, sets the associated IRAP as long-term in RPL0 at slice level, unless the associated IRAP is already included in RPL0 or RPL1 defined in SPS

      PicList::iterator iterPic = rcListPic.begin();
      Picture *rpcPic;
      while (iterPic != rcListPic.end())
      {
        rpcPic = *(iterPic++);
        if ( pcSlice->isDRAP() && rpcPic->getPOC() != pocCurr )
        {
          rpcPic->precedingDRAP = true;
        }
        else if ( !pcSlice->isDRAP() && rpcPic->getPOC() == pocCurr )
        {
          rpcPic->precedingDRAP = false;
        }
      }
    }

    pcSlice->setEnableEdrapSEI(m_pcEncLib->getEdrapIndicationSEIEnabled());
    if (m_pcEncLib->getEdrapIndicationSEIEnabled())
    {
      // Only mark the picture as Extended DRAP if all of the following applies:
      //  1) Extended DRAP indication SEI messages are enabled
      //  2) The current picture is not an intra picture
      //  3) The current picture is in the EDRAP period
      //  4) The current picture is a trailing picture
      if (m_pcEncLib->getEdrapIndicationSEIEnabled() && m_pcEncLib->getEdrapPeriod() > 0 && !pcSlice->isIntra() &&
          pocCurr % m_pcEncLib->getEdrapPeriod() == 0 && pocCurr > pcSlice->getAssociatedIRAPPOC())
      {
        pcSlice->setEdrapRapId(pocCurr / m_pcEncLib->getEdrapPeriod());
        pcSlice->getPic()->setEdrapRapId(pocCurr / m_pcEncLib->getEdrapPeriod());
      }

      if (pcSlice->getEdrapRapId() > 0)
      {
        m_latestEDRAPPOC = pocCurr;
        m_latestEdrapLeadingPicDecodableFlag = false;
        pcSlice->setTLayer(0); // Force Extended DRAP picture to have temporal layer 0
        msg( NOTICE, "Force the temporal sublayer identifier of the EDRAP picture equal to 0.\n");
      }
      pcSlice->setLatestEDRAPPOC(m_latestEDRAPPOC);
      pcSlice->setLatestEdrapLeadingPicDecodableFlag(m_latestEdrapLeadingPicDecodableFlag);
      pcSlice->setUseLTforEdrap(false); // When set, sets the associated IRAP/EDRAP as long-term in RPL0 at slice level, unless the associated IRAP/EDRAP is already included in RPL0 or RPL1 defined in SPS

      PicList::iterator iterPic = rcListPic.begin();
      Picture *rpcPic;
      while (iterPic != rcListPic.end())
      {
        rpcPic = *(iterPic++);
        if ( pcSlice->getEdrapRapId() > 0 && rpcPic->getPOC() != pocCurr && rpcPic->getPOC() >= pcSlice->getAssociatedIRAPPOC() )
        {
          if (rpcPic->getEdrapRapId() >= 0 && rpcPic->getPOC() % m_pcEncLib->getEdrapPeriod() == 0)
          {
            bool refExists = false;
            for (int i = 0; i < pcSlice->getEdrapNumRefRapPics(); i++)
            {
              if (pcSlice->getEdrapRefRapId(i) == rpcPic->getEdrapRapId())
              {
                refExists = true;
              }
            }
            if (!refExists)
            {
              pcSlice->addEdrapRefRapIds(rpcPic->getPOC() / m_pcEncLib->getEdrapPeriod());
              pcSlice->setEdrapNumRefRapPics(pcSlice->getEdrapNumRefRapPics() + 1);
            }
          }
        }
      }
    }

    if (pcSlice->checkThatAllRefPicsAreAvailable(rcListPic, pcSlice->getRpl(REF_PIC_LIST_0), 0, false) != 0
        || pcSlice->checkThatAllRefPicsAreAvailable(rcListPic, pcSlice->getRpl(REF_PIC_LIST_1), 1, false) != 0
        || (m_pcEncLib->getDependentRAPIndicationSEIEnabled() && !pcSlice->isIRAP()
            && (pcSlice->isDRAP()
                || !pcSlice->isPOCInRefPicList(pcSlice->getRpl(REF_PIC_LIST_0), pcSlice->getAssociatedIRAPPOC())))
        || (m_pcEncLib->getEdrapIndicationSEIEnabled() && !pcSlice->isIRAP()
            && (pcSlice->getEdrapRapId() > 0
                || !pcSlice->isPOCInRefPicList(pcSlice->getRpl(REF_PIC_LIST_0), pcSlice->getAssociatedIRAPPOC())))
        || (((pcSlice->isIRAP() && m_pcEncLib->getAvoidIntraInDepLayer())
             || (!pcSlice->isIRAP() && m_pcEncLib->getRplOfDepLayerInSh()))
            && pcSlice->getPic()->cs->vps
            && m_pcEncLib->getNumRefLayers(pcSlice->getPic()->cs->vps->getGeneralLayerIdx(m_pcEncLib->getLayerId()))))
    {
      xCreateExplicitReferencePictureSetFromReference(pcSlice, rcListPic, pcSlice->getRpl(REF_PIC_LIST_0),
                                                      pcSlice->getRpl(REF_PIC_LIST_1));
    }

    pcSlice->applyReferencePictureListBasedMarking(rcListPic, pcSlice->getRpl(REF_PIC_LIST_0),
                                                   pcSlice->getRpl(REF_PIC_LIST_1), pcSlice->getPic()->layerId,
                                                   *(pcSlice->getPPS()));

    if (pcSlice->getTLayer() > 0 && !pcSlice->isLeadingPic())
    {
      if (pcSlice->isStepwiseTemporalLayerSwitchingPointCandidate(rcListPic))
      {
        bool isSTSA=true;

        for (int ii = 0; ii < m_pcCfg->getGOPSize() && isSTSA; ii++)
        {
          int lTid = m_pcCfg->getRPLEntry(0, ii).m_temporalId;

          if (lTid == pcSlice->getTLayer())
          {
            for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
            {
              const ReferencePictureList *rpl = m_pcEncLib->getRplOfDepLayerInSh()
                                                  ? m_pcEncLib->getRplList(l)->getReferencePictureList(ii)
                                                  : pcSlice->getSPS()->getRplList(l)->getReferencePictureList(ii);
              for (int jj = 0; jj < pcSlice->getRpl(l)->getNumberOfActivePictures(); jj++)
              {
                // What about long-term and inter-layer?
                int tPoc = pcSlice->getPOC() + rpl->getRefPicIdentifier(jj);
                for (int kk = 0; kk < m_pcCfg->getGOPSize(); kk++)
                {
                  if (m_pcCfg->getRPLEntry(0, kk).m_POC == tPoc)
                  {
                    int tTid = m_pcCfg->getRPLEntry(0, kk).m_temporalId;
                    if (tTid >= pcSlice->getTLayer())
                    {
                      isSTSA = false;
                      break;
                    }
                  }
                }
              }
            }
          }
        }
        if (isSTSA)
        {
          pcSlice->setNalUnitType(NAL_UNIT_CODED_SLICE_STSA);
        }
      }
    }

    if (m_pcCfg->getUseCompositeRef() && getUseLTRef() && (pocCurr > getLastLTRefPoc()))
    {
      pcSlice->setNumRefIdx(REF_PIC_LIST_0, (pcSlice->isIntra())
                                              ? 0
                                              : std::min(m_pcCfg->getRPLEntry(0, gopId).m_numRefPicsActive + 1,
                                                    pcSlice->getRpl(REF_PIC_LIST_0)->getNumberOfActivePictures()));
      pcSlice->setNumRefIdx(REF_PIC_LIST_1, (!pcSlice->isInterB())
                                              ? 0
                                              : std::min(m_pcCfg->getRPLEntry(1, gopId).m_numRefPicsActive + 1,
                                                    pcSlice->getRpl(REF_PIC_LIST_1)->getNumberOfActivePictures()));
    }
    else
    {
      pcSlice->setNumRefIdx(REF_PIC_LIST_0,
                            (pcSlice->isIntra()) ? 0 : pcSlice->getRpl(REF_PIC_LIST_0)->getNumberOfActivePictures());
      pcSlice->setNumRefIdx(REF_PIC_LIST_1,
                            (!pcSlice->isInterB()) ? 0 : pcSlice->getRpl(REF_PIC_LIST_1)->getNumberOfActivePictures());
    }
    if (m_pcCfg->getUseCompositeRef() && getPrepareLTRef())
    {
      arrangeCompositeReference(pcSlice, rcListPic, pocCurr);
    }
    //  Set reference list
    pcSlice->constructRefPicList(rcListPic);

    // store sub-picture numbers, sizes, and locations with a picture
    pcSlice->getPic()->subPictures.clear();

    for( int subPicIdx = 0; subPicIdx < pcPic->cs->pps->getNumSubPics(); subPicIdx++ )
    {
      pcSlice->getPic()->subPictures.push_back( pcPic->cs->pps->getSubPic( subPicIdx ) );
    }

    const VPS* vps = pcPic->cs->vps;
    int layerIdx = vps == nullptr ? 0 : vps->getGeneralLayerIdx(pcPic->layerId);
    if (vps && !vps->getIndependentLayerFlag(layerIdx) && pcPic->cs->pps->getNumSubPics() > 1)
    {
      CU::checkConformanceILRP(pcSlice);
    }

    if (m_modeCtrl->getUseHashMEPOCChecked())
    {
      if ((m_modeCtrl->getUseHashMEPOCToCheck() != m_modeCtrl->getUseHashMENextPOCToCheck()) && !m_modeCtrl->getUseHashME())
      {
        // if first intra disables hashME also check second intra
        m_modeCtrl->setUseHashMEPOCChecked(false);
        m_modeCtrl->setUseHashMEPOCToCheck(m_modeCtrl->getUseHashMENextPOCToCheck());
        m_modeCtrl->setUseHashME(m_pcCfg->getUseHashMECfgEnable()); // initialize hashME for next intra picture
      }

      if (pcPic->getPOC() > m_modeCtrl->getUseHashMENextPOCToCheck())
      {
        if (m_modeCtrl->getUseHashMEPOCToCheck() != m_modeCtrl->getUseHashMENextPOCToCheck())
        {
          // now can we move the new intra poc in slot 2 to the active slot
          m_modeCtrl->setUseHashMEPOCToCheck(m_modeCtrl->getUseHashMENextPOCToCheck());
          m_modeCtrl->setUseHashMEPOCChecked(false);
          m_modeCtrl->setUseHashME(m_pcCfg->getUseHashMECfgEnable()); // initialize hashME for next intra picture
        }
      }
    }
    if (pcSlice->isIRAP())
    {
      // in-case the previous intra not has been checked we need to put the new intra poc in another slot
      m_modeCtrl->setUseHashMENextPOCToCheck(pcSlice->getPOC());
    }
    xPicInitHashME( pcPic, pcSlice->getPPS(), rcListPic );

    if (m_pcCfg->getUseAMaxBT())
    {
      const SliceType sliceType = pcSlice->getSliceType();
      const SPS      *sps       = pcSlice->getSPS();

      if (!pcSlice->isIRAP())
      {
        const int hierPredLayerIdx = std::min<int>(pcSlice->getHierPredLayerIdx(), (int) m_blkStat.size() - 1);

        if (m_initAMaxBt && pcSlice->getPOC() > m_prevISlicePoc)
        {
          m_blkStat.fill({ 0, 0 });
          m_initAMaxBt = false;
        }

        if (hierPredLayerIdx >= 0 && m_blkStat[hierPredLayerIdx].count != 0)
        {
          picHeader->setSplitConsOverrideFlag(true);

          const double avgBlkSize = (double) m_blkStat[hierPredLayerIdx].area / m_blkStat[hierPredLayerIdx].count;

          unsigned newMaxBtSize;
          if (avgBlkSize < AMAXBT_TH32 * AMAXBT_TH32)
          {
            newMaxBtSize = 32;
          }
          else if (avgBlkSize < AMAXBT_TH64 * AMAXBT_TH64)
          {
            newMaxBtSize = 64;
          }
          else
          {
            newMaxBtSize = 128;
          }
          newMaxBtSize = Clip3(picHeader->getMinQTSize(sliceType), sps->getCTUSize(), newMaxBtSize);
          picHeader->setMaxBTSize(1, newMaxBtSize);

          m_blkStat[hierPredLayerIdx] = { 0, 0 };
        }
      }
      else
      {
        if (m_initAMaxBt)
        {
          m_blkStat.fill({ 0, 0 });
        }

        m_prevISlicePoc = pcSlice->getPOC();
        m_initAMaxBt    = true;
      }

      bool identicalToSps = true;

      if (identicalToSps && picHeader->getPicInterSliceAllowedFlag())
      {
        identicalToSps = picHeader->getMinQTSize(sliceType) == sps->getMinQTSize(sliceType)
                         && picHeader->getMaxMTTHierarchyDepth(sliceType) == sps->getMaxMTTHierarchyDepth()
                         && picHeader->getMaxBTSize(sliceType) == sps->getMaxBTSize()
                         && picHeader->getMaxTTSize(sliceType) == sps->getMaxTTSize();
      }

      if (identicalToSps && picHeader->getPicIntraSliceAllowedFlag())
      {
        identicalToSps = picHeader->getMinQTSize(I_SLICE) == sps->getMinQTSize(I_SLICE)
                         && picHeader->getMaxMTTHierarchyDepth(I_SLICE) == sps->getMaxMTTHierarchyDepthI()
                         && picHeader->getMaxBTSize(I_SLICE) == sps->getMaxBTSizeI()
                         && picHeader->getMaxTTSize(I_SLICE) == sps->getMaxTTSizeI();

        if (identicalToSps && sps->getUseDualITree())
        {
          identicalToSps =
            picHeader->getMinQTSize(I_SLICE, ChannelType::CHROMA) == sps->getMinQTSize(I_SLICE, ChannelType::CHROMA)
            && picHeader->getMaxMTTHierarchyDepth(I_SLICE, ChannelType::CHROMA) == sps->getMaxMTTHierarchyDepthIChroma()
            && picHeader->getMaxBTSize(I_SLICE, ChannelType::CHROMA) == sps->getMaxBTSizeIChroma()
            && picHeader->getMaxTTSize(I_SLICE, ChannelType::CHROMA) == sps->getMaxTTSizeIChroma();
        }
      }

      if (identicalToSps)
      {
        picHeader->setSplitConsOverrideFlag(false);
      }
    }

    //  Slice info. refinement
    if ( (pcSlice->getSliceType() == B_SLICE) && (pcSlice->getNumRefIdx(REF_PIC_LIST_1) == 0) )
    {
      pcSlice->setSliceType ( P_SLICE );
    }

    xUpdateRasInit( pcSlice );

    if (pcSlice->getPendingRasInit() || pcSlice->isIRAP())
    {
      // this ensures that independently encoded bitstream chunks can be combined to bit-equal
      pcSlice->setEncCABACTableIdx( pcSlice->getSliceType() );
    }
    else
    {
      pcSlice->setEncCABACTableIdx( m_pcSliceEncoder->getEncCABACTableIdx() );
    }

    if (pcSlice->getSliceType() == B_SLICE)
    {
      bool lowDelay  = true;
      int  currPoc   = pcSlice->getPOC();
      int  refIdx    = 0;

      for (refIdx = 0; refIdx < pcSlice->getNumRefIdx(REF_PIC_LIST_0) && lowDelay; refIdx++)
      {
        if (pcSlice->getRefPic(REF_PIC_LIST_0, refIdx)->getPOC() > currPoc)
        {
          lowDelay = false;
        }
      }
      for (refIdx = 0; refIdx < pcSlice->getNumRefIdx(REF_PIC_LIST_1) && lowDelay; refIdx++)
      {
        if (pcSlice->getRefPic(REF_PIC_LIST_1, refIdx)->getPOC() > currPoc)
        {
          lowDelay = false;
        }
      }

      pcSlice->setCheckLDC(lowDelay);
    }
    else
    {
      pcSlice->setCheckLDC(true);
    }


    //-------------------------------------------------------------
    pcSlice->setRefPOCList();

    pcSlice->setList1IdxToList0Idx();

    switch (m_pcEncLib->getTMVPModeId())
    {
    case 2:
      // disable TMVP for first picture in SOP (i.e. forward B)
      // Note: pcSlice->getColFromL0Flag() is assumed to be always 0 and getcolRefIdx() is always 0.
      picHeader->setEnableTMVPFlag(gopId != 0);
      break;
    case 1:
      picHeader->setEnableTMVPFlag(true);
      break;
    default:
      picHeader->setEnableTMVPFlag(false);
      break;
    }

    // disable TMVP when current picture is the only ref picture
    if (pcSlice->isIRAP() && pcSlice->getSPS()->getIBCFlag())
    {
      picHeader->setEnableTMVPFlag(false);
    }

    if( pcSlice->getSliceType() != I_SLICE && picHeader->getEnableTMVPFlag() )
    {
      int colRefIdxL0 = -1, colRefIdxL1 = -1;

      for( int refIdx = 0; refIdx < pcSlice->getNumRefIdx( REF_PIC_LIST_0 ); refIdx++ )
      {
        CHECK( pcSlice->getRefPic( REF_PIC_LIST_0, refIdx )->unscaledPic == nullptr, "unscaledPic is not set for L0 reference picture" );

        if( pcSlice->getRefPic( REF_PIC_LIST_0, refIdx )->isRefScaled(pcSlice->getSPS(), pcSlice->getPPS() ) == false )
        {
          colRefIdxL0 = refIdx;
          break;
        }
      }

      if( pcSlice->getSliceType() == B_SLICE )
      {
        for( int refIdx = 0; refIdx < pcSlice->getNumRefIdx( REF_PIC_LIST_1 ); refIdx++ )
        {
          CHECK( pcSlice->getRefPic( REF_PIC_LIST_1, refIdx )->unscaledPic == nullptr, "unscaledPic is not set for L1 reference picture" );

          if( pcSlice->getRefPic( REF_PIC_LIST_1, refIdx )->isRefScaled(pcSlice->getSPS(), pcSlice->getPPS() ) == false )
          {
            colRefIdxL1 = refIdx;
            break;
          }
        }
      }

      if( colRefIdxL0 >= 0 && colRefIdxL1 >= 0 )
      {
        const Picture *refPicL0 = pcSlice->getRefPic( REF_PIC_LIST_0, colRefIdxL0 );
        if( !refPicL0->slices.size() )
        {
          refPicL0 = refPicL0->unscaledPic;
        }

        const Picture *refPicL1 = pcSlice->getRefPic( REF_PIC_LIST_1, colRefIdxL1 );
        if( !refPicL1->slices.size() )
        {
          refPicL1 = refPicL1->unscaledPic;
        }

        CHECK( !refPicL0->slices.size(), "Wrong L0 reference picture" );
        CHECK( !refPicL1->slices.size(), "Wrong L1 reference picture" );

        const uint32_t uiColFromL0 = refPicL0->slices[0]->getSliceQp() > refPicL1->slices[0]->getSliceQp();
        picHeader->setPicColFromL0Flag( uiColFromL0 );
        pcSlice->setColFromL0Flag( uiColFromL0 );
        pcSlice->setColRefIdx( uiColFromL0 ? colRefIdxL0 : colRefIdxL1 );
        picHeader->setColRefIdx( uiColFromL0 ? colRefIdxL0 : colRefIdxL1 );
      }
      else if( colRefIdxL0 < 0 && colRefIdxL1 >= 0 )
      {
        picHeader->setPicColFromL0Flag( false );
        pcSlice->setColFromL0Flag( false );
        pcSlice->setColRefIdx( colRefIdxL1 );
        picHeader->setColRefIdx( colRefIdxL1 );
      }
      else if( colRefIdxL0 >= 0 && colRefIdxL1 < 0 )
      {
        picHeader->setPicColFromL0Flag( true );
        pcSlice->setColFromL0Flag( true );
        pcSlice->setColRefIdx( colRefIdxL0 );
        picHeader->setColRefIdx( colRefIdxL0 );
      }
      else
      {
        picHeader->setEnableTMVPFlag( false );
      }
    }

    if (!pcSlice->getSPS()->getUseAffine())
    {
      picHeader->setMaxNumAffineMergeCand((pcSlice->getSPS()->getSbTMVPEnabledFlag() && picHeader->getEnableTMVPFlag()) ? 1 : 0);
    }

    pcSlice->scaleRefPicList( scaledRefPic, pcPic->cs->picHeader, m_pcEncLib->getApss(), picHeader->getLmcsAPS(), picHeader->getScalingListAPS(), false );

    // set adaptive search range for non-intra-slices
    if (m_pcCfg->getUseASR() && !pcSlice->isIntra())
    {
      m_pcSliceEncoder->setSearchRange(pcSlice);
    }

    bool identicalListsInSliceB = false;
    if (pcSlice->getSliceType() == B_SLICE)
    {
      if (pcSlice->getNumRefIdx(REF_PIC_LIST_0) == pcSlice->getNumRefIdx(REF_PIC_LIST_1))
      {
        identicalListsInSliceB = true;
        for (int i = 0; i < pcSlice->getNumRefIdx(REF_PIC_LIST_1); i++)
        {
          if (pcSlice->getRefPOC(REF_PIC_LIST_1, i) != pcSlice->getRefPOC(REF_PIC_LIST_0, i))
          {
            identicalListsInSliceB = false;
            break;
          }
        }
      }
    }
    picHeader->setMvdL1ZeroFlag(identicalListsInSliceB);

    pcSlice->setMeetBiPredT(false);
    if (pcSlice->getSPS()->getUseSMVD() && !pcSlice->getCheckLDC() && !picHeader->getMvdL1ZeroFlag())
    {
      int currPOC = pcSlice->getPOC();

      int forwardPOC = currPOC;
      int backwardPOC = currPOC;
      int refIdx0 = -1, refIdx1 = -1;

      // search nearest forward POC in List 0
      for (int ref = 0; ref < pcSlice->getNumRefIdx(REF_PIC_LIST_0); ref++)
      {
        int poc = pcSlice->getRefPic( REF_PIC_LIST_0, ref )->getPOC();
        const bool isRefLongTerm = pcSlice->getRefPic(REF_PIC_LIST_0, ref)->longTerm;
        if ( poc < currPOC && (poc > forwardPOC || refIdx0 == -1) && !isRefLongTerm )
        {
          forwardPOC = poc;
          refIdx0 = ref;
        }
      }

      // search nearest backward POC in List 1
      for (int ref = 0; ref < pcSlice->getNumRefIdx(REF_PIC_LIST_1); ref++)
      {
        int poc = pcSlice->getRefPic( REF_PIC_LIST_1, ref )->getPOC();
        const bool isRefLongTerm = pcSlice->getRefPic(REF_PIC_LIST_1, ref)->longTerm;
        if ( poc > currPOC && (poc < backwardPOC || refIdx1 == -1) && !isRefLongTerm )
        {
          backwardPOC = poc;
          refIdx1 = ref;
        }
      }

      if ( !(forwardPOC < currPOC && backwardPOC > currPOC) )
      {
        forwardPOC = currPOC;
        backwardPOC = currPOC;
        refIdx0 = -1;
        refIdx1 = -1;

        // search nearest backward POC in List 0
        for (int ref = 0; ref < pcSlice->getNumRefIdx(REF_PIC_LIST_0); ref++)
        {
          int poc = pcSlice->getRefPic( REF_PIC_LIST_0, ref )->getPOC();
          const bool isRefLongTerm = pcSlice->getRefPic(REF_PIC_LIST_0, ref)->longTerm;
          if ( poc > currPOC && (poc < backwardPOC || refIdx0 == -1) && !isRefLongTerm )
          {
            backwardPOC = poc;
            refIdx0 = ref;
          }
        }

        // search nearest forward POC in List 1
        for (int ref = 0; ref < pcSlice->getNumRefIdx(REF_PIC_LIST_1); ref++)
        {
          int poc = pcSlice->getRefPic( REF_PIC_LIST_1, ref )->getPOC();
          const bool isRefLongTerm = pcSlice->getRefPic(REF_PIC_LIST_1, ref)->longTerm;
          if ( poc < currPOC && (poc > forwardPOC || refIdx1 == -1) && !isRefLongTerm )
          {
            forwardPOC = poc;
            refIdx1 = ref;
          }
        }
      }

      if ( forwardPOC < currPOC && backwardPOC > currPOC )
      {
        pcSlice->setBiDirPred( true, refIdx0, refIdx1 );
        constexpr int affineMeTBiPred = 1;
        pcSlice->setMeetBiPredT(abs(forwardPOC - currPOC) <= affineMeTBiPred);
      }
      else
      {
        pcSlice->setBiDirPred( false, -1, -1 );
      }
    }
    else
    {
      pcSlice->setBiDirPred( false, -1, -1 );
    }

    if( pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL && m_pcCfg->getRprRASLtoolSwitch() )
    {
      pcSlice->setDisableLmChromaCheck( true );
      picHeader->setDmvrDisabledFlag( true );
      xUpdateRPRtmvp( picHeader, pcSlice );
      CHECK( pcSlice->getPPS()->getWrapAroundEnabledFlag(), "pps_ref_wraparound_enabled_flag should be 0 with constrained RASL encoding" );
    }

    double lambda            = 0.0;
    int actualHeadBits       = 0;
    int actualTotalBits      = 0;
    int estimatedBits        = 0;
    int tmpBitsBeforeWriting = 0;

    xPicInitRateControl(estimatedBits, gopId, lambda, pcPic, pcSlice);

    uint32_t numSliceSegments = 1;

    pcSlice->setDefaultClpRng(*pcSlice->getSPS());

    // Allocate some coders, now the number of tiles are known.
    const uint32_t numberOfCtusInFrame = pcPic->cs->pcv->sizeInCtus;
    const int numSubstreamsColumns = pcSlice->getPPS()->getNumTileColumns();
    const int numSubstreamRows     = pcSlice->getSPS()->getEntropyCodingSyncEnabledFlag() ? pcPic->cs->pcv->heightInCtus : (pcSlice->getPPS()->getNumTileRows());
    const int numSubstreams        = std::max<int> (numSubstreamRows * numSubstreamsColumns, (int) pcPic->cs->pps->getNumSlicesInPic());
    std::vector<OutputBitstream> substreamsOut(numSubstreams);

#if ENABLE_QPA
    pcPic->m_uEnerHpCtu.resize (numberOfCtusInFrame);
    pcPic->m_iOffsetCtu.resize (numberOfCtusInFrame);
#if ENABLE_QPA_SUB_CTU
    if (pcSlice->getPPS()->getUseDQP() && pcSlice->getCuQpDeltaSubdiv() > 0)
    {
      const PreCalcValues &pcv = *pcPic->cs->pcv;
      const unsigned   mtsLog2 = (unsigned)floorLog2(std::min (pcPic->cs->sps->getMaxTbSize(), pcv.maxCUWidth));
      pcPic->m_subCtuQP.resize ((pcv.maxCUWidth >> mtsLog2) * (pcv.maxCUHeight >> mtsLog2));
    }
#endif
#endif
    if (pcSlice->getSPS()->getSAOEnabledFlag())
    {
      pcPic->resizeSAO( numberOfCtusInFrame, 0 );
      pcPic->resizeSAO( numberOfCtusInFrame, 1 );
    }

    // it is used for signalling during CTU mode decision, i.e. before ALF processing
    if( pcSlice->getSPS()->getALFEnabledFlag() )
    {
      pcPic->resizeAlfData(numberOfCtusInFrame);
    }

    bool decPic = false;
    bool encPic = false;
    // test if we can skip the picture entirely or decode instead of encoding
    trySkipOrDecodePicture(decPic, encPic, *m_pcCfg, pcPic, m_pcEncLib->getApsMaps());

    pcPic->cs->slice = pcSlice; // please keep this
#if ENABLE_QPA
    if (pcSlice->getPPS()->getSliceChromaQpFlag() && CS::isDualITree (*pcSlice->getPic()->cs) && !m_pcCfg->getUsePerceptQPA() && (m_pcCfg->getSliceChromaOffsetQpPeriodicity() == 0))
#else
    if (pcSlice->getPPS()->getSliceChromaQpFlag() && CS::isDualITree (*pcSlice->getPic()->cs))
#endif
    {
      bool isRprPPS = false;
      for (int nr = 0; nr < NUM_RPR_PPS; nr++)
      {
        if ((pcSlice->getPPS()->getPPSId() == RPR_PPS_ID[nr]) && (RPR_PPS_ID[nr] != 0))
        {
          isRprPPS = true;
        }
      }
      if (!isRprPPS)
      {
        // overwrite chroma qp offset for dual tree
        pcSlice->setSliceChromaQpDelta(COMPONENT_Cb, m_pcCfg->getChromaCbQpOffsetDualTree());
        pcSlice->setSliceChromaQpDelta(COMPONENT_Cr, m_pcCfg->getChromaCrQpOffsetDualTree());
        if (pcSlice->getSPS()->getJointCbCrEnabledFlag())
        {
          pcSlice->setSliceChromaQpDelta(JOINT_CbCr, m_pcCfg->getChromaCbCrQpOffsetDualTree());
        }
        m_pcSliceEncoder->setUpLambda(pcSlice, pcSlice->getLambdas()[0], pcSlice->getSliceQp());
      }
    }

    xPicInitLMCS(pcPic, picHeader, pcSlice);

    if( pcSlice->getSPS()->getScalingListFlag() && m_pcCfg->getUseScalingListId() == SCALING_LIST_FILE_READ )
    {
      picHeader->setExplicitScalingListEnabledFlag( true );
      pcSlice->setExplicitScalingListUsed( true );
      if (picHeader->getScalingListAPS() == nullptr)
      {
        const int apsId = std::min<int>(MAX_NUM_APS(ApsType::SCALING_LIST) - 1, m_pcEncLib->getVPS() == nullptr ?
          0 : m_pcEncLib->getVPS()->getGeneralLayerIdx(m_pcEncLib->getLayerId()));
        APS* scalingListAPS = m_pcEncLib->getApsMap(ApsType::SCALING_LIST)->getPS(apsId);
        CHECK(scalingListAPS == nullptr, "scalingListAPS id not found");
        picHeader->setScalingListAPS(scalingListAPS);
      }
    }

    pcPic->cs->picHeader->setPic(pcPic);
    pcPic->cs->picHeader->setValid();
    if(pcPic->cs->sps->getFpelMmvdEnabledFlag())
    {
      // cannot set ph_fpel_mmvd_enabled_flag at slice level - need new picture-level version of checkDisFracMmvd algorithm?
      // m_pcSliceEncoder->checkDisFracMmvd( pcPic, 0, numberOfCtusInFrame );
      const bool useIntegerMVD = (pcPic->lwidth() * pcPic->lheight() > 1920 * 1080);
      pcPic->cs->picHeader->setDisFracMMVD( useIntegerMVD );
    }
    if (pcSlice->getSPS()->getJointCbCrEnabledFlag())
    {
      if (m_pcCfg->getConstantJointCbCrSignFlag())
      {
        pcPic->cs->picHeader->setJointCbCrSignFlag(false);
      }
      else
      {
        m_pcSliceEncoder->setJointCbCrModes(*pcPic->cs, Position(0, 0), pcPic->cs->area.lumaSize());
      }
    }
    if (!pcSlice->getSPS()->getSpsRangeExtension().getReverseLastSigCoeffEnabledFlag() || pcSlice->getSliceQp() > 12)
    {
      pcSlice->setReverseLastSigCoeffFlag(false);
    }
    else
    {
      /*for RA serial and parallel alignment start*/
      if (m_pcCfg->getIntraPeriod() > 1)
      {
        if (pcSlice->isIntra())
        {
          m_cntRightBottom = 0;
        }
        if ((pocCurr % m_pcCfg->getIntraPeriod()) <= m_pcCfg->getGOPSize() && gopId == 0 && !pcSlice->isIntra())
        {
          m_cntRightBottom = m_cntRightBottomIntra;
        }
      }
      /*for RA serial and parallel alignment end*/
      pcSlice->setReverseLastSigCoeffFlag(m_cntRightBottom >= 0);
    }

    if( encPic )
    // now compress (trial encode) the various slice segments (slices, and dependent slices)
    {
      DTRACE_UPDATE( g_trace_ctx, ( std::make_pair( "poc", pocCurr ) ) );
      const std::vector<uint16_t> sliceLosslessArray = *(m_pcCfg->getSliceLosslessArray());
      bool mixedLossyLossless = m_pcCfg->getMixedLossyLossless();
      if (m_pcCfg->getCostMode() == COST_LOSSLESS_CODING)
      {
        pcPic->fillSliceLossyLosslessArray(sliceLosslessArray, mixedLossyLossless);
      }

      for(uint32_t sliceIdx = 0; sliceIdx < pcPic->cs->pps->getNumSlicesInPic(); sliceIdx++ )
      {
        pcSlice->setSliceMap( pcPic->cs->pps->getSliceMap( sliceIdx ) );
        if (pcSlice->getSPS()->getSpsRangeExtension().getTSRCRicePresentFlag() && (pcPic->cs->pps->getNumSlicesInPic() == 1))
        {
          if (!pcSlice->isIntra())
          {
            int nextRice = 1;

            if (m_preIPOC < pocCurr)
            {
              for (int idx = 0; idx < MAX_TSRC_RICE; idx++)
              {
                m_riceBit[idx][0] = m_riceBit[idx][1];
              }
              m_preQP[0] = m_preQP[1];
              m_preIPOC = MAX_INT;
            }

            if (m_preQP[0] != pcSlice->getSliceQp())
            {
              m_riceBit[pcSlice->getTsrcIndex()][0] = (int) (m_riceBit[pcSlice->getTsrcIndex()][0] * 9 / 10);
            }

            for (int idx = 2; idx < 9; idx++)
            {
              if (m_riceBit[idx - 2][0] > m_riceBit[idx - 1][0])
              {
                nextRice = idx;
              }
              else
              {
                m_riceBit[idx - 1][0] = m_riceBit[idx - 2][0];
              }
              m_riceBit[idx - 2][0] = 0;
            }
            m_riceBit[7][0] = 0;
            pcSlice->setTsrcIndex(nextRice - 1);
          }
          else
          {
            m_preIPOC = pocCurr;
            m_preQP[0] = MAX_INT;
            m_preQP[1] = pcSlice->getSliceQp();
            for (int idx = 0; idx < MAX_TSRC_RICE; idx++)
            {
              m_riceBit[idx][0] = 0;
            }
          }
          for (int idx = 0; idx < MAX_TSRC_RICE; idx++)
          {
            pcSlice->setRiceBit(idx, m_riceBit[idx][0]);
          }
        }
        if( pcPic->cs->pps->getRectSliceFlag() )
        {
          Position firstCtu;
          firstCtu.x = pcSlice->getFirstCtuRsAddrInSlice() % pcPic->cs->pps->getPicWidthInCtu();
          firstCtu.y = pcSlice->getFirstCtuRsAddrInSlice() / pcPic->cs->pps->getPicWidthInCtu();
          int subPicIdx = NOT_VALID;
          for( int sp = 0; sp < pcPic->cs->pps->getNumSubPics(); sp++ )
          {
            if( pcPic->cs->pps->getSubPic( sp ).containsCtu( firstCtu ) )
            {
              subPicIdx = sp;
              break;
            }
          }
          CHECK( subPicIdx == NOT_VALID, "Sub-picture was not found" );

          pcSlice->setSliceSubPicId( pcPic->cs->pps->getSubPic( subPicIdx ).getSubPicID() );
        }
        if (pcPic->cs->sps->getUseLmcs())
        {
          pcSlice->setLmcsEnabledFlag(picHeader->getLmcsEnabledFlag());
          if (pcSlice->getSliceType() == I_SLICE)
          {
            //reshape original signal
            if(m_pcCfg->getGopBasedTemporalFilterEnabled())
            {
              pcPic->getOrigBuf().copyFrom(pcPic->getFilteredOrigBuf());
            }
            else
            {
              pcPic->getOrigBuf().copyFrom(pcPic->getTrueOrigBuf());
            }

            if (pcSlice->getLmcsEnabledFlag())
            {
              pcPic->getOrigBuf(COMPONENT_Y).rspSignal(m_pcReshaper->getFwdLUT());
              m_pcReshaper->setSrcReshaped(true);
              m_pcReshaper->setRecReshaped(true);
            }
            else
            {
              m_pcReshaper->setSrcReshaped(false);
              m_pcReshaper->setRecReshaped(false);
            }
          }
        }

        bool isLossless = false;
        if (m_pcCfg->getCostMode() == COST_LOSSLESS_CODING)
        {
          isLossless = pcPic->losslessSlice(sliceIdx);
        }
        m_pcSliceEncoder->setLosslessSlice(pcPic, isLossless);

        if( pcSlice->getSliceType() != I_SLICE && pcSlice->getRefPic( REF_PIC_LIST_0, 0 )->subPictures.size() > 1 )
        {
          clipMv = clipMvInSubpic;
          m_pcEncLib->getInterSearch()->setClipMvInSubPic(true);
        }
        else
        {
          clipMv = clipMvInPic;
          m_pcEncLib->getInterSearch()->setClipMvInSubPic(false);
        }

        if (pcSlice->isIntra() && (pocLast == 0 || m_pcCfg->getIntraPeriod() > 1))
        {
          computeSignalling(pcPic, pcSlice);
        }
        m_pcSliceEncoder->precompressSlice( pcPic );
#if GREEN_METADATA_SEI_ENABLED
        pcPic->setFeatureCounter(m_featureCounter);
        if(m_pcEncLib->getGMFAFramewise())
        {
          FeatureCounterStruct m_featureCounterFrameReference;
          m_featureCounterFrameReference = m_featureCounter;
        }
#endif
        m_pcSliceEncoder->compressSlice   ( pcPic, false, false);
#if GREEN_METADATA_SEI_ENABLED
        m_featureCounter = pcPic->getFeatureCounter();
#endif

        if(sliceIdx < pcPic->cs->pps->getNumSlicesInPic() - 1)
        {
          uint32_t independentSliceIdx = pcSlice->getIndependentSliceIdx();
          pcPic->allocateNewSlice();
          m_pcSliceEncoder->setSliceSegmentIdx(numSliceSegments);
          // prepare for next slice
          pcSlice = pcPic->slices[numSliceSegments];
          CHECK(!(pcSlice->getPPS() != 0), "Unspecified error");
          pcSlice->copySliceInfo(pcPic->slices[numSliceSegments - 1]);
          pcSlice->setSliceBits(0);
          independentSliceIdx++;
          pcSlice->setIndependentSliceIdx(independentSliceIdx);
          numSliceSegments++;
        }
      }
#if GREEN_METADATA_SEI_ENABLED
      m_featureCounter.baseQP[pcPic->getLossyQPValue()] ++;
      if (m_featureCounter.isYUV420 == -1)
      {
        m_featureCounter.isYUV400 = pcSlice->getSPS()->getChromaFormatIdc() == ChromaFormat::_400 ? 1 : 0;
        m_featureCounter.isYUV420 = pcSlice->getSPS()->getChromaFormatIdc() == ChromaFormat::_420 ? 1 : 0;
        m_featureCounter.isYUV422 = pcSlice->getSPS()->getChromaFormatIdc() == ChromaFormat::_422 ? 1 : 0;
        m_featureCounter.isYUV444 = pcSlice->getSPS()->getChromaFormatIdc() == ChromaFormat::_444 ? 1 : 0;
      }
  
      if (m_featureCounter.is8bit == -1)
      {
        m_featureCounter.is8bit  = (pcSlice->getSPS()->getBitDepth(ChannelType::LUMA) == 8) ? 1 : 0;
        m_featureCounter.is10bit = (pcSlice->getSPS()->getBitDepth(ChannelType::LUMA) == 10) ? 1 : 0;
        m_featureCounter.is12bit = (pcSlice->getSPS()->getBitDepth(ChannelType::LUMA) == 12) ? 1 : 0;
      }
  
  
      if (pcSlice->getSliceType() == B_SLICE)
      {
        m_featureCounter.bSlices++;
      }
      else if (pcSlice->getSliceType()== P_SLICE)
      {
        m_featureCounter.pSlices++;
      }
      else
      {
        m_featureCounter.iSlices++;
      }
  
      if (m_featureCounter.width == -1)
      {
        m_featureCounter.width = pcPic->getPicWidthInLumaSamples();
      }
  
      if (m_featureCounter.height == -1)
      {
        m_featureCounter.height = pcPic->getPicHeightInLumaSamples();
      }
#endif
      duData.clear();

      CodingStructure& cs = *pcPic->cs;
      pcSlice = pcPic->slices[0];

      if (cs.sps->getUseLmcs() && m_pcReshaper->getSliceReshaperInfo().getUseSliceReshaper())
      {
        picHeader->setLmcsEnabledFlag(true);
#if GDR_ENABLED
        if (cs.sps->getGDREnabledFlag() && pcPic->gdrParam.inGdrInterval)
        {
          picHeader->setLmcsChromaResidualScaleFlag(false);
        }
#endif
        int apsId = std::min<int>(3, m_pcEncLib->getVPS() == nullptr ? 0 : m_pcEncLib->getVPS()->getGeneralLayerIdx(m_pcEncLib->getLayerId()));
        picHeader->setLmcsAPSId(apsId);

        const PreCalcValues& pcv = *cs.pcv;
        for (uint32_t yPos = 0; yPos < pcv.lumaHeight; yPos += pcv.maxCUHeight)
        {
          for (uint32_t xPos = 0; xPos < pcv.lumaWidth; xPos += pcv.maxCUWidth)
          {
            const CodingUnit *cu = cs.getCU(Position(xPos, yPos), ChannelType::LUMA);
            if (cu->slice->getLmcsEnabledFlag())
            {
              const uint32_t width = (xPos + pcv.maxCUWidth > pcv.lumaWidth) ? (pcv.lumaWidth - xPos) : pcv.maxCUWidth;
              const uint32_t height = (yPos + pcv.maxCUHeight > pcv.lumaHeight) ? (pcv.lumaHeight - yPos) : pcv.maxCUHeight;
              const UnitArea area(cs.area.chromaFormat, Area(xPos, yPos, width, height));
              cs.getRecoBuf(area).get(COMPONENT_Y).rspSignal(m_pcReshaper->getInvLUT());
            }
          }
        }
        m_pcReshaper->setRecReshaped(false);

        if(m_pcCfg->getGopBasedTemporalFilterEnabled())
        {
          pcPic->getOrigBuf().copyFrom(pcPic->getFilteredOrigBuf());
        }
        else
        {
          pcPic->getOrigBuf().copyFrom(pcPic->getTrueOrigBuf());
        }
      }

      // create SAO object based on the picture size
      if( pcSlice->getSPS()->getSAOEnabledFlag() )
      {
        const uint32_t widthInCtus = ( picWidth + maxCUWidth - 1 ) / maxCUWidth;
        const uint32_t heightInCtus = ( picHeight + maxCUHeight - 1 ) / maxCUHeight;
        const uint32_t numCtuInFrame = widthInCtus * heightInCtus;
        const uint32_t log2SaoOffsetScaleLuma =
          (uint32_t) std::max(0, pcSlice->getSPS()->getBitDepth(ChannelType::LUMA) - MAX_SAO_TRUNCATED_BITDEPTH);
        const uint32_t log2SaoOffsetScaleChroma =
          (uint32_t) std::max(0, pcSlice->getSPS()->getBitDepth(ChannelType::CHROMA) - MAX_SAO_TRUNCATED_BITDEPTH);

        m_pcSAO->create(picWidth, picHeight, chromaFormatIdc, maxCUWidth, maxCUHeight, maxTotalCUDepth,
                        log2SaoOffsetScaleLuma, log2SaoOffsetScaleChroma);
        m_pcSAO->destroyEncData();
        m_pcSAO->createEncData( m_pcCfg->getSaoCtuBoundary(), numCtuInFrame );
        m_pcSAO->setReshaper( m_pcReshaper );
      }

      // SAO parameter estimation using non-deblocked pixels for CTU bottom and right boundary areas
      if( pcSlice->getSPS()->getSAOEnabledFlag() && m_pcCfg->getSaoCtuBoundary() )
      {
        m_pcSAO->getPreDBFStatistics( cs, m_pcCfg->getSaoTrueOrg() );
      }

      //-- Loop filter
      if ( m_pcCfg->getDeblockingFilterMetric() )
      {
        if ( m_pcCfg->getDeblockingFilterMetric()==2 )
        {
          applyDeblockingFilterParameterSelection(pcPic, numSliceSegments, gopId);
        }
        else
        {
          applyDeblockingFilterMetric(pcPic);
        }
      }
      if (m_pcCfg->getCostMode() == COST_LOSSLESS_CODING)
      {
        for (int s = 0; s < numSliceSegments; s++)
        {
          if (pcPic->slices[s]->isLossless())
          {
            pcPic->slices[s]->setDeblockingFilterDisable(true);
          }
        }
      }
#if GREEN_METADATA_SEI_ENABLED
      cs.m_featureCounter.resetBoundaryStrengths();
#endif
      m_pcLoopFilter->deblockingFilterPic( cs );
#if GREEN_METADATA_SEI_ENABLED
      m_featureCounter.addBoundaryStrengths(cs.m_featureCounter);
#endif

      CS::setRefinedMotionField(cs);

      if( pcSlice->getSPS()->getSAOEnabledFlag() )
      {
#if GREEN_METADATA_SEI_ENABLED
        cs.m_featureCounter.resetSAO();
#endif
        bool sliceEnabled[MAX_NUM_COMPONENT];
        m_pcSAO->initCABACEstimator( m_pcEncLib->getCABACEncoder(), m_pcEncLib->getCtxCache(), pcSlice );
        m_pcSAO->SAOProcess( cs, sliceEnabled, pcSlice->getLambdas(),
#if ENABLE_QPA
                             (m_pcCfg->getUsePerceptQPA() && !m_pcCfg->getUseRateCtrl() && pcSlice->getPPS()->getUseDQP() ? m_pcEncLib->getRdCost ()->getChromaWeight() : 0.0),
#endif
                             m_pcCfg->getTestSAODisableAtPictureLevel(), m_pcCfg->getSaoEncodingRate(), m_pcCfg->getSaoEncodingRateChroma(), m_pcCfg->getSaoCtuBoundary(), m_pcCfg->getSaoGreedyMergeEnc(), m_pcCfg->getSaoTrueOrg() );
        //assign SAO slice header
        for (int s = 0; s < numSliceSegments; s++)
        {
          if (pcPic->slices[s]->isLossless() && m_pcCfg->getCostMode() == COST_LOSSLESS_CODING)
          {
            pcPic->slices[s]->setSaoEnabledFlag(ChannelType::LUMA, false);
            pcPic->slices[s]->setSaoEnabledFlag(ChannelType::CHROMA, false);
          }
          else
          {
            pcPic->slices[s]->setSaoEnabledFlag(ChannelType::LUMA, sliceEnabled[COMPONENT_Y]);
            CHECK(!(sliceEnabled[COMPONENT_Cb] == sliceEnabled[COMPONENT_Cr]), "Unspecified error");
            pcPic->slices[s]->setSaoEnabledFlag(ChannelType::CHROMA, sliceEnabled[COMPONENT_Cb]);
          }
        }
#if GREEN_METADATA_SEI_ENABLED
        m_featureCounter.addSAO(cs.m_featureCounter);
#endif
      }

      if( pcSlice->getSPS()->getALFEnabledFlag() )
      {
        m_pcALF->destroy();
        m_pcALF->create(m_pcCfg, picWidth, picHeight, chromaFormatIdc, maxCUWidth, maxCUHeight, maxTotalCUDepth,
                        m_pcCfg->getBitDepth(), m_pcCfg->getInputBitDepth());

        for (int s = 0; s < numSliceSegments; s++)
        {
          pcPic->slices[s]->setAlfEnabledFlag(COMPONENT_Y, false);
        }
        m_pcALF->initCABACEstimator(m_pcEncLib->getCABACEncoder(), m_pcEncLib->getCtxCache(), pcSlice,
                                    m_pcEncLib->getApsMap(ApsType::ALF));
#if GREEN_METADATA_SEI_ENABLED
        cs.m_featureCounter.resetALF();
#endif
        m_pcALF->ALFProcess(cs, pcSlice->getLambdas()
#if ENABLE_QPA
                                  ,
                            (m_pcCfg->getUsePerceptQPA() && !m_pcCfg->getUseRateCtrl() && pcSlice->getPPS()->getUseDQP()
                               ? m_pcEncLib->getRdCost()->getChromaWeight()
                               : 0.0)
#endif
                              ,
                            pcPic, numSliceSegments);
#if GREEN_METADATA_SEI_ENABLED
        m_featureCounter.addALF(cs.m_featureCounter);
#endif
        //assign ALF slice header
        for (int s = 0; s < numSliceSegments; s++)
        {
           //For the first slice, even if it is lossless, slice level ALF is not disabled and ALF-APS is signaled so that the later lossy slices can use APS of the first slice.
           //However, if the first slice is lossless, the ALF process is disabled for all of the CTUs ( m_ctuEnableFlag == 0) of that slice which is implemented in the function void EncAdaptiveLoopFilter::ALFProcess.

          if (pcPic->slices[s]->isLossless() && s && m_pcCfg->getCostMode() == COST_LOSSLESS_CODING)
          {
            pcPic->slices[s]->setAlfEnabledFlag(COMPONENT_Y, false);
            pcPic->slices[s]->setAlfEnabledFlag(COMPONENT_Cb, false);
            pcPic->slices[s]->setAlfEnabledFlag(COMPONENT_Cr, false);
          }
          else
          {
            pcPic->slices[s]->setAlfEnabledFlag(COMPONENT_Y, cs.slice->getAlfEnabledFlag(COMPONENT_Y));
            pcPic->slices[s]->setAlfEnabledFlag(COMPONENT_Cb, cs.slice->getAlfEnabledFlag(COMPONENT_Cb));
            pcPic->slices[s]->setAlfEnabledFlag(COMPONENT_Cr, cs.slice->getAlfEnabledFlag(COMPONENT_Cr));

          }
          if (pcPic->slices[s]->getAlfEnabledFlag(COMPONENT_Y))
          {
            pcPic->slices[s]->setNumAlfApsIdsLuma(cs.slice->getNumAlfApsIdsLuma());
            pcPic->slices[s]->setAlfApsIdsLuma(cs.slice->getAlfApsIdsLuma());
          }
          else
          {
            pcPic->slices[s]->setNumAlfApsIdsLuma(0);
          }
          pcPic->slices[s]->setAlfAPSs(cs.slice->getAlfAPSs());
          pcPic->slices[s]->setAlfApsIdChroma(cs.slice->getAlfApsIdChroma());
          pcPic->slices[s]->setCcAlfCbApsId(cs.slice->getCcAlfCbApsId());
          pcPic->slices[s]->setCcAlfCrApsId(cs.slice->getCcAlfCrApsId());
          pcPic->slices[s]->m_ccAlfFilterParam      = m_pcALF->getCcAlfFilterParam();
          pcPic->slices[s]->m_ccAlfFilterControl[0] = m_pcALF->getCcAlfControlIdc(COMPONENT_Cb);
          pcPic->slices[s]->m_ccAlfFilterControl[1] = m_pcALF->getCcAlfControlIdc(COMPONENT_Cr);
        }
      }
      else if (cs.slice->getPendingRasInit() || cs.slice->isIDRorBLA())
      {
        m_pcALF->setApsIdStart(m_pcCfg->getALFAPSIDShift() + m_pcCfg->getMaxNumALFAPS());
      }
      DTRACE_UPDATE( g_trace_ctx, ( std::make_pair( "final", 1 ) ) );
      if (m_pcCfg->getUseCompositeRef() && getPrepareLTRef())
      {
        updateCompositeReference(pcSlice, rcListPic, pocCurr);
      }
    }
    else // skip enc picture
    {
      pcSlice->setSliceQpBase( pcSlice->getSliceQp() );

#if ENABLE_QPA
      if (m_pcCfg->getUsePerceptQPA() && !m_pcCfg->getUseRateCtrl() && pcSlice->getPPS()->getUseDQP())
      {
        const double picLambda = pcSlice->getLambdas()[0];

        for (uint32_t ctuRsAddr = 0; ctuRsAddr < numberOfCtusInFrame; ctuRsAddr++)
        {
          pcPic->m_uEnerHpCtu[ctuRsAddr] = picLambda;  // initialize to slice lambda (just for safety)
        }
      }
#endif
      if( pcSlice->getSPS()->getSAOEnabledFlag() )
      {
        m_pcSAO->disabledRate( *pcPic->cs, pcPic->getSAO(1), m_pcCfg->getSaoEncodingRate(), m_pcCfg->getSaoEncodingRateChroma());
      }
      if (pcSlice->getSPS()->getALFEnabledFlag() && (pcSlice->getAlfEnabledFlag(COMPONENT_Y) || pcSlice->getCcAlfCbEnabledFlag() || pcSlice->getCcAlfCrEnabledFlag()))
      {
        // IRAP AU: reset APS map
        {
          if (pcSlice->getPendingRasInit() || pcSlice->isIDRorBLA())
          {
            // We have to reset all APS on IRAP, but in not encoding case we have to keep the parsed APS of current slice
            // Get active ALF APSs from picture/slice header
            const AlfApsList &sliceApsIdsLuma = pcSlice->getAlfApsIdsLuma();

            m_pcALF->setApsIdStart(m_pcCfg->getALFAPSIDShift() + m_pcCfg->getMaxNumALFAPS());

            ParameterSetMap<APS> *apsMap = m_pcEncLib->getApsMap(ApsType::ALF);
            apsMap->clearActive();

           for (int apsId = m_pcCfg->getALFAPSIDShift(); apsId < m_pcCfg->getALFAPSIDShift() + m_pcCfg->getMaxNumALFAPS(); apsId++)
           {
             APS *aps = apsMap->getPS(apsId);
             if (aps)
             {
               // Check if this APS is currently the active one (used in current slice)
               bool activeAps      = false;
               bool activeApsCcAlf = false;
               // Luma
               for (int i = 0; i < sliceApsIdsLuma.size(); i++)
               {
                 if (aps->getAPSId() == sliceApsIdsLuma[i])
                 {
                   activeAps = true;
                   break;
                 }
               }
               // Chroma
               activeAps |= aps->getAPSId() == pcSlice->getAlfApsIdChroma();
               // CC-ALF
               activeApsCcAlf |= pcSlice->getCcAlfCbEnabledFlag() && aps->getAPSId() == pcSlice->getCcAlfCbApsId();
               activeApsCcAlf |= pcSlice->getCcAlfCrEnabledFlag() && aps->getAPSId() == pcSlice->getCcAlfCrApsId();
               if (!activeAps && !activeApsCcAlf)
               {
                 apsMap->clearChangedFlag(apsId);
               }
               if (!activeAps)
               {
                 aps->getAlfAPSParam().reset();
               }
               if (!activeApsCcAlf)
               {
                 aps->getCcAlfAPSParam().reset();
               }
             }
            }
          }
        }

        // Assign tne correct APS to slice and emulate the setting of ALF start APS ID
        int changedApsId = -1;
        for (int apsId = m_pcCfg->getALFAPSIDShift() + m_pcCfg->getMaxNumALFAPS() - 1; apsId >= m_pcCfg->getALFAPSIDShift(); apsId--)
        {
          ParameterSetMap<APS> *apsMap = m_pcEncLib->getApsMap(ApsType::ALF);
          APS                  *aps    = apsMap->getPS(apsId);
          if( aps )
          {
            // In slice, replace the old APS (from decoder map) with the APS from encoder map due to later checks while bitstream writing
            if( pcSlice->getAlfAPSs() && pcSlice->getAlfAPSs()[apsId] )
            {
              pcSlice->getAlfAPSs()[apsId] = aps;
            }
            if (apsMap->getChangedFlag(apsId))
            {
              changedApsId = apsId;
            }
          }
        }
        if( changedApsId >= 0 )
        {
          m_pcALF->setApsIdStart( changedApsId );
        }
      }
    }

    pcSlice->freeScaledRefPicList( scaledRefPic );

    if (m_pcCfg->getUseAMaxBT() && !pcSlice->isIntra())
    {
      const int hierPredLayerIdx = std::min<int>(pcSlice->getHierPredLayerIdx(), (int) m_blkStat.size() - 1);

      for (const CodingUnit *cu: pcPic->cs->cus)
      {
        m_blkStat[hierPredLayerIdx].area += cu->Y().area();
        m_blkStat[hierPredLayerIdx].count++;
      }
    }

    if (m_pcCfg->getFilmGrainAnalysisEnabled())
    {
      int  filteredFrame    = m_pcCfg->getIntraPeriod() < 1
                                ? static_cast<int>(2 * m_pcCfg->getFrameRate().getFloatVal() + 0.5)
                                : m_pcCfg->getIntraPeriod();
      bool readyToAnalyze   = pcPic->getPOC() % filteredFrame
                                ? false
                                : true;   // either it is mctf denoising or external source for film grain analysis. note:
                                          // if mctf is used, it is different from mctf for encoding.
      if (readyToAnalyze)
      {
        m_fgAnalyzer.initBufs(pcPic);
        m_fgAnalyzer.estimate_grain(pcPic);
      }
    }

    if( encPic || decPic )
    {
      pcSlice = pcPic->slices[0];

      /////////////////////////////////////////////////////////////////////////////////////////////////// File writing

      // write various parameter sets
#if GDR_ENABLED // Note : insert SPS/PPS at every GDR picture
      bool writePS = m_seqFirst || (m_pcCfg->getReWriteParamSets() && (pcSlice->isIRAP())) || pcSlice->isInterGDR();
#else
      bool writePS = m_seqFirst || (m_pcCfg->getReWriteParamSets() && (pcSlice->isIRAP()));
#endif
      if (writePS)
      {
        m_pcEncLib->setParamSetChanged(pcSlice->getSPS()->getSPSId(), pcSlice->getPPS()->getPPSId());
      }
#if JVET_AJ0151_DSC_SEI

#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
      // Before writing the NAL units of an RAP, write trailing TWC verification SEIs of previous picture when single layer is used. Multi-layer case is handled along with last picture handling.
      const bool writeDSCverification = !m_seqFirst && pcSlice->isIRAP() && (m_pcEncLib->getVPS() == nullptr || m_pcEncLib->getVPS()->getMaxLayers() == 1);
#else
      // Before writing the NAL units of an RAP, write trailing TWC verification SEIs of previous picture
      const bool writeDSCverification = !m_seqFirst && pcSlice->isIRAP();
#endif
      if (writeDSCverification)
      {
        if (m_pcCfg->getDigitallySignedContentSEICfg().enabled)
        {
          SEIMessages twcSeiMessages;
          std::vector<uint8_t> signature;
          m_dscSubstreamManager.signSubstream(0, signature);
          SEIDigitallySignedContentVerification *sei = new SEIDigitallySignedContentVerification;
          m_seiEncoder.initSEIDigitallySignedContentVerification(sei, 0, signature);
          twcSeiMessages.push_back(sei);
          xWriteTrailingSEIMessages(twcSeiMessages, accessUnit, m_prevPicTemporalId);
        }
      }
      m_prevPicTemporalId = pcSlice->getTLayer();
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
      if (writePS && m_pcCfg->getDigitallySignedContentSEICfg().enabled && m_dscSubstreamId == 0)
#else
      if (writePS && m_pcCfg->getDigitallySignedContentSEICfg().enabled)
#endif
      {
        std::array<uint8_t,16> contentUuid = {
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        };
        const EncCfgParam::CfgSEIDigitallySignedContent &dscCfg = m_pcCfg->getDigitallySignedContentSEICfg();
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
        m_dscSubstreamManager.initDscSubstreamManager(dscCfg.numVerificationSubstreams, dscCfg.hashMethod, dscCfg.publicKeyUri , false, contentUuid, dscCfg.refSubstreamFlag, dscCfg.implicitAssociationModeFlag);
#else
        m_dscSubstreamManager.initDscSubstreamManager(dscCfg.numVerificationSubstreams, dscCfg.hashMethod, dscCfg.publicKeyUri , false, contentUuid, dscCfg.refSubstreamFlag);
#endif
#else
        m_dscSubstreamManager.initDscSubstreamManager(1, dscCfg.hashMethod, dscCfg.publicKeyUri , false, contentUuid);
#endif
        m_dscSubstreamManager.initSignature(dscCfg.privateKeyFile);
      }
#endif

      int layerIdx = m_pcEncLib->getVPS() == nullptr ? 0 : m_pcEncLib->getVPS()->getGeneralLayerIdx( m_pcEncLib->getLayerId() );

      // it is assumed that layerIdx equal to 0 is always present
      m_audIrapOrGdrAuFlag = pcSlice->getPicHeader()->getGdrPicFlag() || (pcSlice->isIRAP() && !pcSlice->getPPS()->getMixedNaluTypesInPicFlag());
      if ((( m_pcEncLib->getVPS()->getMaxLayers() > 1 && m_audIrapOrGdrAuFlag) || m_pcCfg->getAccessUnitDelimiter()) && !layerIdx )
      {
        xWriteAccessUnitDelimiter(accessUnit, pcSlice);
      }

      // it is assumed that layerIdx equal to 0 is always present
      bool newPPS = m_pcEncLib->PPSNeedsWriting(pcSlice->getPPS()->getPPSId());
      if (m_pcEncLib->getRprFunctionalityTestingEnabledFlag() || m_pcEncLib->getGOPBasedRPREnabledFlag())
      {
        if (newPPS)
        {
          m_pcEncLib->setRprPPSCodedAfterIntra(m_pcEncLib->getRprResolutionIndex(pcSlice->getPPS()->getPPSId()), true);
        }
        // here a PPS needs to be encoded for an inter picture if PPS is different from any RPR PPS written after and including the intra
        if ((m_pcEncLib->getRprFunctionalityTestingEnabledFlag() && (pcSlice->getPOC() % m_pcEncLib->getRprSwitchingSegmentSize()) == 0) ||
          (m_pcEncLib->getGOPBasedRPREnabledFlag() && (pcSlice->getPOC() % m_pcEncLib->getGOPSize()) == 0))
        {
          if (pcSlice->isIntra())
          {
            for (int nr = 0; nr < NUM_RPR_PPS; nr++)
            {
              // at intra all PPS coded after Intra is reset except the current one
              if (pcSlice->getPPS()->getPPSId() != RPR_PPS_ID[nr])
              {
                m_pcEncLib->setRprPPSCodedAfterIntra(m_pcEncLib->getRprResolutionIndex(RPR_PPS_ID[nr]), false);
              }
            }
          }
          else
          {
            if (!m_pcEncLib->getRprPPSCodedAfterIntra(m_pcEncLib->getRprResolutionIndex(pcSlice->getPPS()->getPPSId())))
            {
              // here a forced coding of a pps is enabled
              newPPS = true;
              m_pcEncLib->setRprPPSCodedAfterIntra(m_pcEncLib->getRprResolutionIndex(pcSlice->getPPS()->getPPSId()), true);
            }
          }
        }
      }
      actualTotalBits += xWriteParameterSets(accessUnit, pcSlice, writePS, layerIdx, newPPS);

      if (writePS)
      {
        // create prefix SEI messages at the beginning of the sequence
        CHECK(!(leadingSeiMessages.empty()), "Unspecified error");
        xCreateIRAPLeadingSEIMessages(leadingSeiMessages, pcSlice->getSPS(), pcSlice->getPPS());

        m_seqFirst = false;
      }

      if (writePS && m_pcCfg->getNNPostFilterSEICharacteristicsEnabled() && m_pcCfg->getNNPostFilterSEICharacteristicsUseSuffixSEI())
      {
        // create NNPostFilterSEICharacteristics SEI as suffix SEI
        xCreateNNPostFilterCharacteristicsSEIMessages(trailingSeiMessages);
      }
      if (writePS && m_pcCfg->getGenerativeFaceVideoSEIEnabled())
      {
        xCreateGenerativeFaceVideoSEIMessages(trailingSeiMessages);
      }
      if (writePS && m_pcCfg->getGenerativeFaceVideoEnhancementSEIEnabled())
      {
        xCreateGenerativeFaceVideoEnhancementSEIMessages(trailingSeiMessages);
      }

      //send LMCS APS when LMCSModel is updated. It can be updated even current slice does not enable reshaper.
      //For example, in RA, update is on intra slice, but intra slice may not use reshaper
      if (pcSlice->getSPS()->getUseLmcs())
      {
        //only 1 LMCS data for 1 picture
        int apsId = picHeader->getLmcsAPSId();

        ParameterSetMap<APS> *apsMapLmcs = m_pcEncLib->getApsMap(ApsType::LMCS);

        APS *aps = apsId >= 0 ? apsMapLmcs->getPS(apsId) : nullptr;

        bool writeAPS = aps && apsMapLmcs->getChangedFlag(apsId);
#if GDR_ENABLED // note : insert APS at every GDR picture
        if (aps)
        {
          writeAPS |= pcSlice->isInterGDR();
        }
#endif
        if (writeAPS)
        {
          aps->chromaPresentFlag = isChromaEnabled(pcSlice->getSPS()->getChromaFormatIdc());
          actualTotalBits += xWriteAPS( accessUnit, aps, m_pcEncLib->getLayerId(), true );
          apsMapLmcs->clearChangedFlag(apsId);
#if GDR_ENABLED
          if (!pcSlice->isInterGDR())
          {
            CHECK(aps != picHeader->getLmcsAPS(), "Wrong LMCS APS pointer in compressGOP");
          }
#else
          CHECK(aps != picHeader->getLmcsAPS(), "Wrong LMCS APS pointer in compressGOP");
#endif
        }
      }

      // only 1 SCALING LIST data for 1 picture
      if( pcSlice->getSPS()->getScalingListFlag() && ( m_pcCfg->getUseScalingListId() == SCALING_LIST_FILE_READ ) )
      {
        const int             apsId    = picHeader->getScalingListAPSId();
        ParameterSetMap<APS> *apsMapSl = m_pcEncLib->getApsMap(ApsType::SCALING_LIST);
        APS                  *aps      = apsMapSl->getPS(apsId);
        bool                  writeAPS = aps && apsMapSl->getChangedFlag(apsId);
#if GDR_ENABLED // note : insert APS at every GDR picture
        if (aps && apsId >= 0)
        {
          writeAPS |= pcSlice->isInterGDR();
        }
#endif
        if( writeAPS )
        {
          aps->chromaPresentFlag = isChromaEnabled(pcSlice->getSPS()->getChromaFormatIdc());
          actualTotalBits += xWriteAPS( accessUnit, aps, m_pcEncLib->getLayerId(), true );
          apsMapSl->clearChangedFlag(apsId);
#if GDR_ENABLED
          if (!pcSlice->isInterGDR())
          {
            CHECK(aps != picHeader->getScalingListAPS(), "Wrong SCALING LIST APS pointer in compressGOP");
          }
#else
          CHECK( aps != picHeader->getScalingListAPS(), "Wrong SCALING LIST APS pointer in compressGOP" );
#endif
        }
      }

      if (pcSlice->getSPS()->getALFEnabledFlag() && (pcSlice->getAlfEnabledFlag(COMPONENT_Y) || pcSlice->getCcAlfCbEnabledFlag() || pcSlice->getCcAlfCrEnabledFlag()))
      {
        for (int apsId = m_pcCfg->getALFAPSIDShift(); apsId < m_pcCfg->getALFAPSIDShift() + m_pcCfg->getMaxNumALFAPS(); apsId++)
        {
          ParameterSetMap<APS> *apsMapAlf = m_pcEncLib->getApsMap(ApsType::ALF);

          APS *aps      = apsMapAlf->getPS(apsId);
          bool writeAPS = aps && apsMapAlf->getChangedFlag(apsId);
          if (!aps && pcSlice->getAlfAPSs() && pcSlice->getAlfAPSs()[apsId])
          {
            writeAPS = true;
            aps = pcSlice->getAlfAPSs()[apsId]; // use asp from slice header
            *apsMapAlf->allocatePS(apsId) = *aps;                         // allocate and cpy
            m_pcALF->setApsIdStart( apsId );
          }
          else if (pcSlice->getCcAlfCbEnabledFlag() && !aps && apsId == pcSlice->getCcAlfCbApsId())
          {
            writeAPS = true;
            aps      = apsMapAlf->getPS(pcSlice->getCcAlfCbApsId());
          }
          else if (pcSlice->getCcAlfCrEnabledFlag() && !aps && apsId == pcSlice->getCcAlfCrApsId())
          {
            writeAPS = true;
            aps      = apsMapAlf->getPS(pcSlice->getCcAlfCrApsId());
          }
#if GDR_ENABLED // note : insert APS at every GDR picture
          if (aps && apsId >= 0)
          {
            writeAPS |= (pcSlice->isInterGDR());
          }
#endif
          if (writeAPS )
          {
            aps->chromaPresentFlag = isChromaEnabled(pcSlice->getSPS()->getChromaFormatIdc());
            actualTotalBits += xWriteAPS( accessUnit, aps, m_pcEncLib->getLayerId(), true );
            apsMapAlf->clearChangedFlag(apsId);
#if GDR_ENABLED
            if (!pcSlice->isInterGDR())
            {
              CHECK(aps != pcSlice->getAlfAPSs()[apsId] && apsId != pcSlice->getCcAlfCbApsId() && apsId != pcSlice->getCcAlfCrApsId(), "Wrong APS pointer in compressGOP");
            }
#else
            CHECK(aps != pcSlice->getAlfAPSs()[apsId] && apsId != pcSlice->getCcAlfCbApsId() && apsId != pcSlice->getCcAlfCrApsId(), "Wrong APS pointer in compressGOP");
#endif
          }
        }
      }

      // reset presence of BP SEI indication
      m_bufferingPeriodSEIPresentInAU = false;
      // create prefix SEI associated with a picture
      xCreatePerPictureSEIMessages(gopId, leadingSeiMessages, nestedSeiMessages, pcSlice);

      if (m_pcCfg->getNnPostFilterSEIActivationEnabled() && m_pcCfg->getNnPostFilterSEIActivationUseSuffixSEI())
      {
        // create NeuralNetworkPostFilterActivation SEI as suffix SEI
        xCreateNNPostFilterActivationSEIMessage(trailingSeiMessages, pcSlice);
      }

      if (newPPS)
      {
        xCreatePhaseIndicationSEIMessages(leadingSeiMessages, pcSlice, pcSlice->getPPS()->getPPSId());
      }
      // pcSlice is currently slice 0.
      std::size_t binCountsInNalUnits   = 0; // For implementation of cabac_zero_word stuffing (section 7.4.3.10)
      std::size_t numBytesInVclNalUnits = 0; // For implementation of cabac_zero_word stuffing (section 7.4.3.10)
      std::size_t sumZeroWords          = 0; // sum of cabac_zero_word inserted per sub-picture
      std::vector<EncBitstreamParams> subPicStats (pcPic->cs->pps->getNumSubPics());

      for(uint32_t sliceSegmentIdxCount = 0; sliceSegmentIdxCount < pcPic->cs->pps->getNumSlicesInPic(); sliceSegmentIdxCount++ )
      {
        pcSlice = pcPic->slices[sliceSegmentIdxCount];
        if(sliceSegmentIdxCount > 0 && pcSlice->getSliceType()!= I_SLICE)
        {
          pcSlice->checkColRefIdx(sliceSegmentIdxCount, pcPic);
        }
        m_pcSliceEncoder->setSliceSegmentIdx(sliceSegmentIdxCount);

        *pcSlice->getRpl(REF_PIC_LIST_0) = *pcPic->slices[0]->getRpl(REF_PIC_LIST_0);
        *pcSlice->getRpl(REF_PIC_LIST_1) = *pcPic->slices[0]->getRpl(REF_PIC_LIST_1);
        pcSlice->setRplIdx(REF_PIC_LIST_0, pcPic->slices[0]->getRplIdx(REF_PIC_LIST_0));
        pcSlice->setRplIdx(REF_PIC_LIST_1, pcPic->slices[0]->getRplIdx(REF_PIC_LIST_1));

        picHeader->setNoOutputBeforeRecoveryFlag( false );
        if (pcSlice->isIRAP())
        {
          if (pcSlice->getNalUnitType() >= NAL_UNIT_CODED_SLICE_IDR_W_RADL && pcSlice->getNalUnitType() <= NAL_UNIT_CODED_SLICE_IDR_N_LP)
          {
            picHeader->setNoOutputBeforeRecoveryFlag( true );
          }
          //the inference for NoOutputPriorPicsFlag
          // KJS: This cannot happen at the encoder
          if (!m_first && (pcSlice->isIRAP() || pcSlice->getNalUnitType() >= NAL_UNIT_CODED_SLICE_GDR)
              && picHeader->getNoOutputBeforeRecoveryFlag())
          {
            if (pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA || pcSlice->getNalUnitType() >= NAL_UNIT_CODED_SLICE_GDR)
            {
              pcSlice->setNoOutputOfPriorPicsFlag(true);
            }
          }
        }

        // code picture header before first slice
        if(sliceSegmentIdxCount == 0)
        {
          // code RPL in picture header or slice headers
          if( !m_pcCfg->getSliceLevelRpl() && (!pcSlice->getIdrPicFlag() || pcSlice->getSPS()->getIDRRefParamListPresent()) )
          {
            picHeader->setRplIdx(REF_PIC_LIST_0, pcSlice->getRplIdx(REF_PIC_LIST_0));
            picHeader->setRplIdx(REF_PIC_LIST_1, pcSlice->getRplIdx(REF_PIC_LIST_1));
            *picHeader->getRpl(REF_PIC_LIST_0) = *pcSlice->getRpl(REF_PIC_LIST_0);
            *picHeader->getRpl(REF_PIC_LIST_1) = *pcSlice->getRpl(REF_PIC_LIST_1);
          }

          // code DBLK in picture header or slice headers
          if( !m_pcCfg->getSliceLevelDblk() )
          {
            picHeader->setDeblockingFilterOverrideFlag   ( pcSlice->getDeblockingFilterOverrideFlag()   );
            picHeader->setDeblockingFilterDisable        ( pcSlice->getDeblockingFilterDisable()        );
            picHeader->setDeblockingFilterBetaOffsetDiv2 ( pcSlice->getDeblockingFilterBetaOffsetDiv2() );
            picHeader->setDeblockingFilterTcOffsetDiv2   ( pcSlice->getDeblockingFilterTcOffsetDiv2()   );
            picHeader->setDeblockingFilterCbBetaOffsetDiv2( pcSlice->getDeblockingFilterCbBetaOffsetDiv2() );
            picHeader->setDeblockingFilterCbTcOffsetDiv2  ( pcSlice->getDeblockingFilterCbTcOffsetDiv2() );
            picHeader->setDeblockingFilterCrBetaOffsetDiv2( pcSlice->getDeblockingFilterCrBetaOffsetDiv2() );
            picHeader->setDeblockingFilterCrTcOffsetDiv2  ( pcSlice->getDeblockingFilterCrTcOffsetDiv2() );
          }

          if (!m_pcCfg->getSliceLevelDeltaQp())
          {
            picHeader->setQpDelta(pcSlice->getSliceQp() - (pcSlice->getPPS()->getPicInitQPMinus26() + QP26));
          }

          // code SAO parameters in picture header or slice headers
          if( !m_pcCfg->getSliceLevelSao() )
          {
            picHeader->setSaoEnabledFlag(ChannelType::LUMA, pcSlice->getSaoEnabledFlag(ChannelType::LUMA));
            picHeader->setSaoEnabledFlag(ChannelType::CHROMA, pcSlice->getSaoEnabledFlag(ChannelType::CHROMA));
          }

          // code ALF parameters in picture header or slice headers
          if( !m_pcCfg->getSliceLevelAlf() )
          {
            picHeader->setAlfEnabledFlag(COMPONENT_Y,  pcSlice->getAlfEnabledFlag(COMPONENT_Y ) );
            picHeader->setAlfEnabledFlag(COMPONENT_Cb, pcSlice->getAlfEnabledFlag(COMPONENT_Cb) );
            picHeader->setAlfEnabledFlag(COMPONENT_Cr, pcSlice->getAlfEnabledFlag(COMPONENT_Cr) );
            picHeader->setNumAlfApsIdsLuma(pcSlice->getNumAlfApsIdsLuma());
            picHeader->setAlfApsIdsLuma(pcSlice->getAlfApsIdsLuma());
            picHeader->setAlfApsIdChroma(pcSlice->getAlfApsIdChroma());
            picHeader->setCcAlfEnabledFlag(COMPONENT_Cb, pcSlice->getCcAlfCbEnabledFlag());
            picHeader->setCcAlfEnabledFlag(COMPONENT_Cr, pcSlice->getCcAlfCrEnabledFlag());
            picHeader->setCcAlfCbApsId(pcSlice->getCcAlfCbApsId());
            picHeader->setCcAlfCrApsId(pcSlice->getCcAlfCrApsId());
          }

          // code WP parameters in picture header or slice headers
          if (!m_pcCfg->getSliceLevelWp())
          {
            picHeader->setWpScaling(pcSlice->getWpScalingAll());
            picHeader->setNumWeights(REF_PIC_LIST_0, pcSlice->getNumRefIdx(REF_PIC_LIST_0));
            picHeader->setNumWeights(REF_PIC_LIST_1, pcSlice->getNumRefIdx(REF_PIC_LIST_1));
          }

          pcPic->cs->picHeader->setPic(pcPic);
          pcPic->cs->picHeader->setValid();
          if (pcPic->cs->pps->getNumSlicesInPic() > 1 || !m_pcCfg->getEnablePictureHeaderInSliceHeader())
          {
            pcSlice->setPictureHeaderInSliceHeader(false);
            actualTotalBits += xWritePicHeader(accessUnit, pcPic->cs->picHeader);
#if GDR_ENC_TRACE
            printf("-gdr_pic_flag:%d\n", picHeader->getGdrPicFlag());
            printf("-recovery_poc_cnt:%d\n", picHeader->getRecoveryPocCnt());
            printf("-InGdrInterval:%d\n", pcPic->gdrParam.inGdrInterval);
            printf("-pic_lmcs_enabled_flag:%d\n", picHeader->getLmcsEnabledFlag() ? 1 : 0);
            printf("-pic_chroma_residual_scale_flag:%d\n", picHeader->getLmcsChromaResidualScaleFlag() ? 1 : 0);
#endif
          }
          else
          {
            pcSlice->setPictureHeaderInSliceHeader(true);
          }
          if (pcSlice->getSPS()->getProfileTierLevel()->getConstraintInfo()->getPicHeaderInSliceHeaderConstraintFlag())
          {
            CHECK(pcSlice->getPictureHeaderInSliceHeader() == false, "PH shall be present in SH, when pic_header_in_slice_header_constraint_flag is equal to 1");
          }

#if JVET_AL0056_EOI_SEI_QUANT_CONSTRAINT
          if (m_pcCfg->getEOISEIEnabled() && !m_pcCfg->getEOISEICancelFlag() && (m_pcCfg->getEOISEIType() & EOI_OptimizationType::OBJECT_BASED_OPTIMIZATION) && (m_pcCfg->getEOISEIObjectBasedIdc() & EOI_OBJECT_BASED::COARSER_QUANTIZATION))
          {
            if (m_pcCfg->getEOISEIPicQuantObjectFlag())
            {
              CHECK(m_pcCfg->getEOISEIQuantThresholdDelta() < 1 || m_pcCfg->getEOISEIQuantThresholdDelta() > (63 - pcSlice->getSliceQp()), "When eoi_pic_quant_object_flag is equal to 1, the value of eoi_quant_threshold_delta shall be in the range of 1 to 63 - PicQuant, inclusive");
            }
            else
            {
              CHECK(m_pcCfg->getEOISEIQuantThresholdDelta() < 1 || m_pcCfg->getEOISEIQuantThresholdDelta() > (pcSlice->getSPS()->getQpBDOffset(ChannelType::LUMA) + pcSlice->getSliceQp()), "When eoi_pic_quant_object_flag is equal to 0, the value of eoi_quant_threshold_delta shall be in the range of 1 to QpBdOffset + PicQuant, inclusive");
            }
          }
#endif
        }
        pcSlice->setPicHeader( pcPic->cs->picHeader );
        pcSlice->setNalUnitLayerId( m_pcEncLib->getLayerId() );

        for ( uint32_t ui = 0 ; ui < numSubstreams; ui++ )
        {
          substreamsOut[ui].clear();
        }

        /* start slice NALunit */
        OutputNALUnit nalu( pcSlice->getNalUnitType(), m_pcEncLib->getLayerId(), pcSlice->getTLayer() );
        m_HLSWriter->setBitstream(&nalu.m_bitstream);

        tmpBitsBeforeWriting = m_HLSWriter->getNumberOfWrittenBits();
        m_HLSWriter->codeSliceHeader( pcSlice );
        actualHeadBits += ( m_HLSWriter->getNumberOfWrittenBits() - tmpBitsBeforeWriting );

        pcSlice->setFinalized(true);

        pcSlice->resetNumberOfSubstream( );
        pcSlice->setNumSubstream( pcSlice->getSPS(), pcSlice->getPPS() );
        pcSlice->clearSubstreamSizes(  );
        const int subpicIdx = pcPic->cs->pps->getSubPicIdxFromSubPicId(pcSlice->getSliceSubPicId());
        {
          uint32_t numBinsCoded = 0;
          m_pcSliceEncoder->encodeSlice(pcPic, &(substreamsOut[0]), numBinsCoded);
          binCountsInNalUnits+=numBinsCoded;
          subPicStats[subpicIdx].numBinsWritten += numBinsCoded;
        }
        if (pcSlice->getSPS()->getSpsRangeExtension().getTSRCRicePresentFlag() && (pcPic->cs->pps->getNumSlicesInPic() == 1))
        {
          if (pcSlice->getSliceType() == I_SLICE)
          {
            for (int idx = 0; idx < MAX_TSRC_RICE; idx++)
            {
              m_riceBit[idx][1] = pcSlice->getRiceBit(idx);
            }
          }
          for (int idx = 0; idx < MAX_TSRC_RICE; idx++)
          {
            m_riceBit[idx][0] = pcSlice->getRiceBit(idx);
          }
          m_preQP[0] = pcSlice->getSliceQp();
        }
        {
          // Construct the final bitstream by concatenating substreams.
          // The final bitstream is either nalu.m_bitstream or pcBitstreamRedirect;
          // Complete the slice header info.
          m_HLSWriter->setBitstream(&nalu.m_bitstream);
          m_HLSWriter->codeTilesWPPEntryPoint( pcSlice );

          // Append substreams...
          OutputBitstream *pcOut = pcBitstreamRedirect;
          const int numSubstreamsToCode = pcSlice->getNumberOfSubstream() + 1;

          for ( uint32_t ui = 0 ; ui < numSubstreamsToCode; ui++ )
          {
            pcOut->addSubstream(&(substreamsOut[ui]));
          }
        }

        // If current NALU is the first NALU of slice (containing slice header) and more NALUs exist (due to multiple dependent slices) then buffer it.
        // If current NALU is the last NALU of slice and a NALU was buffered, then (a) Write current NALU (b) Update an write buffered NALU at approproate location in NALU list.
        bool naluAlignedWrittenToList =
          false;   // used to ensure current NALU is not written more than once to the NALU list.
        xAttachSliceDataToNalUnit(nalu, pcBitstreamRedirect);
#if JVET_AJ0151_DSC_SEI
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
        xAddToSubstream(m_dscSubstreamId, nalu);
#else
        xAddToSubstream(0, nalu);
#endif
#endif
        accessUnit.push_back(new NALUnitEBSP(nalu));
        actualTotalBits += uint32_t(accessUnit.back()->m_nalUnitData.str().size()) * 8;
        numBytesInVclNalUnits += (std::size_t)(accessUnit.back()->m_nalUnitData.str().size());
        subPicStats[subpicIdx].numBytesInVclNalUnits += (std::size_t)(accessUnit.back()->m_nalUnitData.str().size());
        naluAlignedWrittenToList = true;

        if (!naluAlignedWrittenToList)
        {
          nalu.m_bitstream.writeAlignZero();
          accessUnit.push_back(new NALUnitEBSP(nalu));
        }

        if( ( m_pcCfg->getPictureTimingSEIEnabled() || m_pcCfg->getDecodingUnitInfoSEIEnabled() ) &&
        ((pcSlice->getSPS()->getGeneralHrdParameters()->getGeneralNalHrdParametersPresentFlag())
          || (pcSlice->getSPS()->getGeneralHrdParameters()->getGeneralVclHrdParametersPresentFlag())) &&
          (pcSlice->getSPS()->getGeneralHrdParameters()->getGeneralDecodingUnitHrdParamsPresentFlag()))
        {
          uint32_t numNalus = 0;
          uint32_t numRBSPBytes = 0;
          for (AccessUnit::const_iterator it = accessUnit.begin(); it != accessUnit.end(); it++)
          {
            numRBSPBytes += uint32_t((*it)->m_nalUnitData.str().size());
            numNalus ++;
          }
          duData.push_back(DUData());
          duData.back().accumBitsDU = ( numRBSPBytes << 3 );
          duData.back().accumNalsDU = numNalus;
        }
        if (pcSlice->isLastSliceInSubpic())
        {
          // Check picture level encoding constraints/requirements
          ProfileTierLevelFeatures profileTierLevelFeatures;
          profileTierLevelFeatures.extractPTLInformation(*(pcSlice->getSPS()));
          const SEIMessages &subPictureLevelInfoSEIs =
            getSeisByType(leadingSeiMessages, SEI::PayloadType::SUBPICTURE_LEVEL_INFO);
          if (!subPictureLevelInfoSEIs.empty())
          {
            const SEISubpictureLevelInfo& sli =
              static_cast<const SEISubpictureLevelInfo&>(*subPictureLevelInfoSEIs.front());
            validateMinCrRequirements(profileTierLevelFeatures, subPicStats[subpicIdx].numBytesInVclNalUnits, pcSlice,
                                      m_pcCfg, sli, subpicIdx, m_pcEncLib->getLayerId());
          }
          sumZeroWords += cabac_zero_word_padding(pcSlice, pcPic, subPicStats[subpicIdx].numBinsWritten, subPicStats[subpicIdx].numBytesInVclNalUnits, 0,
                                                  accessUnit.back()->m_nalUnitData, m_pcCfg->getCabacZeroWordPaddingEnabled(), profileTierLevelFeatures);
        }
      } // end iteration over slices

      {
        // Check picture level encoding constraints/requirements
        ProfileTierLevelFeatures profileTierLevelFeatures;
        profileTierLevelFeatures.extractPTLInformation(*(pcSlice->getSPS()));
        validateMinCrRequirements(profileTierLevelFeatures, numBytesInVclNalUnits, pcPic, m_pcCfg);
        // cabac_zero_words processing
        cabac_zero_word_padding(pcSlice, pcPic, binCountsInNalUnits, numBytesInVclNalUnits, sumZeroWords, accessUnit.back()->m_nalUnitData, m_pcCfg->getCabacZeroWordPaddingEnabled(), profileTierLevelFeatures);
      }

      //-- For time output for each slice
      auto elapsed = std::chrono::steady_clock::now() - beforeTime;
      auto encTime = std::chrono::duration_cast<std::chrono::milliseconds>( elapsed ).count()/1000.0;

      std::string digestStr;
#if GDR_ENABLED
      // note : generate hash sei only for non-gdr pictures
      bool genHash = !(m_pcCfg->getGdrNoHash() && pcSlice->getPic()->gdrParam.inGdrInterval);
      if (m_pcCfg->getDecodedPictureHashSEIType() != HashType::NONE && genHash)
#else
      if (m_pcCfg->getDecodedPictureHashSEIType() != HashType::NONE)
#endif
      {
        SEIDecodedPictureHash *decodedPictureHashSei = new SEIDecodedPictureHash();
        PelUnitBuf recoBuf = pcPic->cs->getRecoBuf();
        m_seiEncoder.initDecodedPictureHashSEI(decodedPictureHashSei, recoBuf, digestStr, pcSlice->getSPS()->getBitDepths());
        trailingSeiMessages.push_back(decodedPictureHashSei);
      }
      // create per-subpicture decoded picture hash SEI messages, if more than one subpicture is enabled
      const PPS* pps = pcPic->cs->pps;
      const int numSubpics = pps->getNumSubPics();
      std::string subPicDigest;
      if (numSubpics > 1 && m_pcCfg->getSubpicDecodedPictureHashType() != HashType::NONE)
      {
        std::vector<uint16_t> subPicIdsInPic;
        xGetSubpicIdsInPic(subPicIdsInPic, pcPic->cs->sps, pps);
        uint16_t maxSubpicIdInPic = subPicIdsInPic.size() == 0 ? 0 : *std::max_element(subPicIdsInPic.begin(), subPicIdsInPic.end());
        for (int subPicIdx = 0; subPicIdx < numSubpics; subPicIdx++)
        {
          const SubPic& subpic = pps->getSubPic(subPicIdx);
          const UnitArea area = UnitArea(pcSlice->getSPS()->getChromaFormatIdc(), Area(subpic.getSubPicLeft(), subpic.getSubPicTop(), subpic.getSubPicWidthInLumaSample(), subpic.getSubPicHeightInLumaSample()));
          PelUnitBuf recoBuf = pcPic->cs->getRecoBuf(area);
          SEIDecodedPictureHash *decodedPictureHashSEI = new SEIDecodedPictureHash();
          m_seiEncoder.initDecodedPictureHashSEI(decodedPictureHashSEI, recoBuf, subPicDigest, pcSlice->getSPS()->getBitDepths());
          SEIMessages nestedSEI;
          nestedSEI.push_back(decodedPictureHashSEI);
          const std::vector<uint16_t> subPicIds = { (uint16_t)subpic.getSubPicID() };
          std::vector<int> targetOLS;
          std::vector<int> targetLayers = {pcPic->layerId};
          xCreateScalableNestingSEI(trailingSeiMessages, nestedSEI, targetOLS, targetLayers, subPicIds, maxSubpicIdInPic);
        }
      }

      m_pcCfg->setEncodedFlag(gopId, true);

      double PSNR_Y;
      xCalculateAddPSNRs(isField, isTff, gopId, pcPic, accessUnit, rcListPic, encTime, snr_conversion, printFrameMSE,
                         printMSSSIM, &PSNR_Y, isEncodeLtRef);
#if GREEN_METADATA_SEI_ENABLED
      this->setFeatureCounter(m_featureCounter);
      m_SEIGreenQualityMetrics.psnr = PSNR_Y;
      if (m_pcCfg->getSEIGreenMetadataInfoSEIEnable())
      {
        SEIGreenMetadataInfo* seiGreenMetadataInfo = new SEIGreenMetadataInfo;
        seiGreenMetadataInfo->m_greenMetadataType = m_pcCfg->getSEIGreenMetadataType();
        seiGreenMetadataInfo->m_numPictures = m_pcCfg->getSEIGreenMetadataPeriodNumPictures();
        seiGreenMetadataInfo->m_periodType = m_pcCfg->getSEIGreenMetadataPeriodType();
        seiGreenMetadataInfo->m_numSeconds = m_pcCfg->getSEIGreenMetadataPeriodNumSeconds();
        seiGreenMetadataInfo->m_greenMetadataGranularityType = m_pcCfg->getSEIGreenMetadataGranularityType();
        seiGreenMetadataInfo->m_greenMetadataExtendedRepresentation = m_pcCfg->getSEIGreenMetadataExtendedRepresentation();
        int64_t codedFrames = m_featureCounter.iSlices + m_featureCounter.bSlices + m_featureCounter.pSlices;
        int     numberFrames =
          static_cast<int>(seiGreenMetadataInfo->m_numSeconds * m_pcCfg->getFrameRate().getFloatVal() + 0.5);

        if (seiGreenMetadataInfo->m_greenMetadataType == 0)
        {
          switch (m_pcCfg->getSEIGreenMetadataPeriodType()) // Period type
          {
          case 0: //0x00 complexity metrics are applicable to a single picture
            seiGreenMetadataInfo->m_numPictures = m_pcCfg->getSEIGreenMetadataPeriodNumPictures();
            xCalculateGreenComplexityMetrics(m_featureCounter, m_featureCounterReference, seiGreenMetadataInfo);
            m_seiEncoder.initSEIGreenMetadataInfo(seiGreenMetadataInfo,  m_featureCounter, m_SEIGreenQualityMetrics,m_SEIGreenComplexityMetrics);
            leadingSeiMessages.push_back(seiGreenMetadataInfo);
            m_featureCounterReference = m_featureCounter;
            break;
          case 1: //0x01 complexity metrics are applicable to all pictures in decoding order, up to (but not including) the picture containing the next I slice
            if (codedFrames == m_pcCfg->getFramesToBeEncoded() || codedFrames == 1)
            {
              xCalculateGreenComplexityMetrics(m_featureCounter, m_featureCounterReference, seiGreenMetadataInfo);
              m_seiEncoder.initSEIGreenMetadataInfo(seiGreenMetadataInfo,  m_featureCounter, m_SEIGreenQualityMetrics,m_SEIGreenComplexityMetrics);
              leadingSeiMessages.push_back(seiGreenMetadataInfo);
              m_featureCounterReference = m_featureCounter;
            }
            break;
          case 2: //0x02 complexity metrics are applicable over a specified time interval in seconds
            seiGreenMetadataInfo->m_numSeconds = m_pcCfg->getSEIGreenMetadataPeriodNumSeconds();
            if( ((codedFrames% numberFrames) == 0) || (codedFrames == m_pcCfg->getFramesToBeEncoded()))
            {
              seiGreenMetadataInfo->m_numSeconds = int(floor(codedFrames / m_pcCfg->getFrameRate().getFloatVal()));
              xCalculateGreenComplexityMetrics(m_featureCounter, m_featureCounterReference, seiGreenMetadataInfo);
              m_seiEncoder.initSEIGreenMetadataInfo(seiGreenMetadataInfo,  m_featureCounter, m_SEIGreenQualityMetrics,m_SEIGreenComplexityMetrics);
              leadingSeiMessages.push_back(seiGreenMetadataInfo);
              m_featureCounterReference = m_featureCounter;
            }
            break;
          case 3: //0x03 complexity metrics are applicable over a specified number of pictures counted in decoding order
            seiGreenMetadataInfo->m_numPictures = m_pcCfg->getSEIGreenMetadataPeriodNumPictures();
            if( ((codedFrames%(seiGreenMetadataInfo->m_numPictures)) == 0) || (codedFrames == m_pcCfg->getFramesToBeEncoded()))
            {
              xCalculateGreenComplexityMetrics(m_featureCounter, m_featureCounterReference, seiGreenMetadataInfo);
              m_seiEncoder.initSEIGreenMetadataInfo(seiGreenMetadataInfo,  m_featureCounter, m_SEIGreenQualityMetrics,m_SEIGreenComplexityMetrics);
              leadingSeiMessages.push_back(seiGreenMetadataInfo);
              m_featureCounterReference = m_featureCounter;
            }
            break;
          case 4: //0x04 complexity metrics are applicable to a single picture with slice or tile granularity
          case 5: //0x05 complexity metrics are applicable to a single picture with subpicture granularity
          case 6: //0x06 complexity metrics are applicable to all pictures in decoding order, up to (but not including) the picture containing the next I slice with subpicture granularity
          case 7: //0x07 complexity metrics are applicable over a specified time interval in seconds with subpicture granularity
          case 8: //0x08 complexity metrics are applicable over a specified number of pictures counted in decoding order with subpicture granularity
          default: //0x05-0xFF reserved
            break;
          }
        }
        else if (seiGreenMetadataInfo->m_greenMetadataType == 1) // Quality metric signaling
        {
          m_seiEncoder.initSEIGreenMetadataInfo(seiGreenMetadataInfo, m_featureCounter, m_SEIGreenQualityMetrics, m_SEIGreenComplexityMetrics);
          leadingSeiMessages.push_back(seiGreenMetadataInfo);
        }
#if GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464
        else if (seiGreenMetadataInfo->m_greenMetadataType == 2)   // Attenuation map signaling
        {
          m_seiEncoder.initSEIGreenMetadataInfo(seiGreenMetadataInfo, m_featureCounter, m_SEIGreenQualityMetrics,
                                                m_SEIGreenComplexityMetrics);
          leadingSeiMessages.push_back(seiGreenMetadataInfo);
        }
#endif 
      }
#endif
      
      xWriteTrailingSEIMessages(trailingSeiMessages, accessUnit, pcSlice->getTLayer());
#if JVET_AJ0151_DSC_SEI

      m_totalPicsCoded++;
      const int skip = m_pcCfg->getFrameSkip() ? m_pcCfg->getFrameSkip() : 1;
      const int lastPic =(m_pcCfg->getFramesToBeEncoded() / skip) - 1;
      const bool isLastPicture = ( m_totalPicsCoded > lastPic);
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
      bool writeLayerWiseDSCV = false;
      if (m_pcEncLib->getVPS() != NULL && m_pcEncLib->getVPS()->getMaxLayers() > 1)
      {
        CHECK(m_pcCfg->getFieldSeqFlag() || m_pcCfg->getUseCompositeRef(), "Field coding and composite ref should be disabled when multi-layer Digitally Signed Content Verification SEI is enabled in this encoder implementation");
        int pocNext;
        if (m_totalPicsCoded == 0)
        {
          pocNext = m_pcCfg->getGOPEntry(0).m_POC;
        }
        else if (gopId + 1 < m_iGopSize)
        {
          pocNext = pocLast - numPicRcvd + m_pcCfg->getGOPEntry(gopId + 1).m_POC;
        }
        else
        {
          pocNext = pocLast - numPicRcvd + m_iGopSize + m_pcCfg->getGOPEntry(0).m_POC;
        }
        writeLayerWiseDSCV = getNalUnitType(pocNext, pocLast, false) >= NAL_UNIT_CODED_SLICE_IDR_W_RADL && getNalUnitType(pocNext, pocLast, false) <= NAL_UNIT_CODED_SLICE_CRA;
      }
      if (isLastPicture || writeLayerWiseDSCV)
#else
      if (isLastPicture)
#endif
      {
        if (m_pcCfg->getDigitallySignedContentSEICfg().enabled)
        {
          SEIMessages twcSeiMessages;
          std::vector<uint8_t> signature;
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
          m_dscSubstreamManager.signSubstream(m_dscSubstreamId, signature);
#else
          m_dscSubstreamManager.signSubstream(0, signature);
#endif
          SEIDigitallySignedContentVerification *sei = new SEIDigitallySignedContentVerification;
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
          m_seiEncoder.initSEIDigitallySignedContentVerification(sei, m_dscSubstreamId, signature);
#else
          m_seiEncoder.initSEIDigitallySignedContentVerification(sei, 0, signature);
#endif
          twcSeiMessages.push_back(sei);
          xWriteTrailingSEIMessages(twcSeiMessages, accessUnit, pcSlice->getTLayer());
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
          if (isLastPicture && m_dscSubstreamId == (m_pcCfg->getDigitallySignedContentSEICfg().numVerificationSubstreams - 1))
          {
            m_dscSubstreamManager.uninitDscSubstreamManager();
          }
#else
          m_dscSubstreamManager.uninitDscSubstreamManager();
#endif
        }
      }
#endif

#if GDR_ENABLED
      if (!(m_pcCfg->getGdrNoHash() && pcSlice->getPic()->gdrParam.inGdrInterval))
      {
        printHash(m_pcCfg->getDecodedPictureHashSEIType(), digestStr);
      }
#else
      printHash(m_pcCfg->getDecodedPictureHashSEIType(), digestStr);
#endif

      if ( m_pcCfg->getUseRateCtrl() )
      {
        double avgQP     = m_pcRateCtrl->getRCPic()->calAverageQP();
        double avgLambda = m_pcRateCtrl->getRCPic()->calAverageLambda();
        if ( avgLambda < 0.0 )
        {
          avgLambda = lambda;
        }

        m_pcRateCtrl->getRCPic()->updateAfterPicture( actualHeadBits, actualTotalBits, avgQP, avgLambda, pcSlice->isIRAP());
        m_pcRateCtrl->getRCPic()->addToPictureLsit( m_pcRateCtrl->getPicList() );

        m_pcRateCtrl->getRCSeq()->updateAfterPic( actualTotalBits );
        if ( !pcSlice->isIRAP() )
        {
          m_pcRateCtrl->getRCGOP()->updateAfterPicture( actualTotalBits );
        }
        else    // for intra picture, the estimated bits are used to update the current status in the GOP
        {
          m_pcRateCtrl->getRCGOP()->updateAfterPicture( estimatedBits );
        }
        if (m_pcRateCtrl->getCpbSaturationEnabled())
        {
          m_pcRateCtrl->updateCpbState(actualTotalBits);
          msg( NOTICE, " [CPB %6d bits]", m_pcRateCtrl->getCpbState() );
        }
      }
      xCreateFrameFieldInfoSEI( leadingSeiMessages, pcSlice, isField );
      xCreatePictureTimingSEI( m_pcCfg->getEfficientFieldIRAPEnabled() ? effFieldIRAPMap.GetIRAPGOPid() : 0, leadingSeiMessages, nestedSeiMessages, duInfoSeiMessages, pcSlice, isField, duData );

      if (m_pcCfg->getScalableNestingSEIEnabled())
      {
        const SPS* sps = pcSlice->getSPS();
        const PPS* pps = pcSlice->getPPS();

        std::vector<uint16_t> subpicIDs;
        xGetSubpicIdsInPic(subpicIDs, sps, pps);
        uint16_t maxSubpicIdInPic = subpicIDs.size() == 0 ? 0 : *std::max_element(subpicIDs.begin(), subpicIDs.end());
        // Note (KJS): Using targetOLS = 0, 1 is as random as encapsulating the same SEIs in scalable nesting.
        //             This can just be seen as example regarding how to write scalable nesting, not what to write.
        std::vector<int> targetOLS = {0, 1};
        std::vector<int> targetLayers;
        xCreateScalableNestingSEI(leadingSeiMessages, nestedSeiMessages, targetOLS, targetLayers, subpicIDs, maxSubpicIdInPic);
      }

      SEIMessages seiMessages = getSeisByType(leadingSeiMessages, SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_CHARACTERISTICS);
      for (auto it = seiMessages.cbegin(); it != seiMessages.cend(); it++)
      {
        pcPic->SEIs.push_back(new SEINeuralNetworkPostFilterCharacteristics(*(SEINeuralNetworkPostFilterCharacteristics*) *it));
      }

      seiMessages = getSeisByType(trailingSeiMessages, SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_CHARACTERISTICS);
      for (auto it = seiMessages.cbegin(); it != seiMessages.cend(); it++)
      {
        pcPic->SEIs.push_back(new SEINeuralNetworkPostFilterCharacteristics(*(SEINeuralNetworkPostFilterCharacteristics*) *it));
      }

      seiMessages = getSeisByType(leadingSeiMessages, SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_ACTIVATION);
      for (auto it = seiMessages.cbegin(); it != seiMessages.cend(); it++)
      {
        pcPic->SEIs.push_back(new SEINeuralNetworkPostFilterActivation(*(SEINeuralNetworkPostFilterActivation*) *it));
      }

      seiMessages = getSeisByType(trailingSeiMessages, SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_ACTIVATION);
      for (auto it = seiMessages.cbegin(); it != seiMessages.cend(); it++)
      {
        pcPic->SEIs.push_back(new SEINeuralNetworkPostFilterActivation(*(SEINeuralNetworkPostFilterActivation*) *it));
      }

      seiMessages = getSeisByType(leadingSeiMessages, SEI::PayloadType::FRAME_PACKING);
      for (auto it = seiMessages.cbegin(); it != seiMessages.cend(); it++)
      {
        pcPic->SEIs.push_back(new SEIFramePacking(*(SEIFramePacking*) *it));
      }

      xWriteLeadingSEIMessages( leadingSeiMessages, duInfoSeiMessages, accessUnit, pcSlice->getTLayer(), pcSlice->getSPS(), duData );
      xWriteDuSEIMessages( duInfoSeiMessages, accessUnit, pcSlice->getTLayer(), duData );

      m_AUWriterIf->outputAU( accessUnit );

      msg( NOTICE, "\n" );
      fflush( stdout );
    }

    m_cntRightBottom = pcSlice->getCntRightBottom();
    if (m_pcCfg->getIntraPeriod() > 1 && pcSlice->isIntra())
    {
      m_cntRightBottomIntra = m_cntRightBottom;
    }

    DTRACE_UPDATE( g_trace_ctx, ( std::make_pair( "final", 0 ) ) );

    pcPic->reconstructed = true;
    m_first              = false;
    m_numPicsCoded++;
    if (!(m_pcCfg->getUseCompositeRef() && isEncodeLtRef))
    {
      for( int i = pcSlice->getTLayer() ; i < pcSlice->getSPS()->getMaxTLayers() ; i ++ )
      {
        m_totalCoded[i]++;
      }
    }
    /* logging: insert a newline at end of picture period */

    if (m_pcCfg->getEfficientFieldIRAPEnabled())
    {
      gopId = effFieldIRAPMap.restoreGOPid(gopId);
    }

    pcPic->destroyTempBuffers();
    pcPic->cs->destroyTemporaryCsData();
  }   // gopId-loop

  delete pcBitstreamRedirect;

  CHECK(m_numPicsCoded > 1, "Unspecified error");
}

void EncGOP::printOutSummary(uint32_t numAllPicCoded, bool isField, const bool printMSEBasedSNR,
                             const bool printSequenceMSE, const bool printMSSSIM, const bool printHexPsnr,
                             const bool printRprPsnr, const BitDepths &bitDepths, int layerId)
{
#if ENABLE_QPA
  const bool    useWPSNR = m_pcEncLib->getUseWPSNR();
#endif
#if WCG_WPSNR
  const bool    useLumaWPSNR = m_pcEncLib->getPrintWPSNR();
#endif

  if( m_pcCfg->getDecodeBitstream(0).empty() && m_pcCfg->getDecodeBitstream(1).empty() && !m_pcCfg->useFastForwardToPOC() )
  {
    CHECK(!(numAllPicCoded == m_gcAnalyzeAll.getNumPic()), "Unspecified error");
  }

  Fraction picRate = m_pcCfg->getFrameRate();
  picRate.num *= isField ? 2 : 1;
  picRate.den *= m_pcCfg->getTemporalSubsampleRatio();

  m_gcAnalyzeAll.setFrameRate(picRate);
  m_gcAnalyzeI.setFrameRate(picRate);
  m_gcAnalyzeP.setFrameRate(picRate);
  m_gcAnalyzeB.setFrameRate(picRate);
#if WCG_WPSNR
  if (useLumaWPSNR)
  {
    m_gcAnalyzeWPSNR.setFrameRate(picRate);
  }
#endif

  const ChromaFormat chFmt = m_pcCfg->getChromaFormatIdc();

  //-- all
  msg( INFO, "\n" );
  msg( DETAILS,"\nSUMMARY --------------------------------------------------------\n" );
#if JVET_O0756_CALCULATE_HDRMETRICS
  const bool calculateHdrMetrics = m_pcEncLib->getCalculateHdrMetrics();
#else
  const bool calculateHdrMetrics = false;
#endif

  std::string header,metrics;
  std::string id="a";
  id += layerId == 0 ? " " : std::to_string(layerId);
  m_gcAnalyzeAll.printOut(header, metrics, id, chFmt, printMSEBasedSNR, printSequenceMSE, printMSSSIM, printHexPsnr,
                          printRprPsnr, bitDepths, useWPSNR, calculateHdrMetrics);
  if( g_verbosity >= INFO ) std::cout<<header<<'\n'<<metrics<<std::endl;

  id="i";
  id += layerId == 0 ? " " : std::to_string(layerId);
  m_gcAnalyzeI.printOut(header, metrics, id, chFmt, printMSEBasedSNR, printSequenceMSE, printMSSSIM, printHexPsnr,
                        printRprPsnr, bitDepths, false, false);
  if( g_verbosity >= DETAILS ) std::cout<< "\n\nI Slices--------------------------------------------------------\n"<<header<<'\n'<<metrics<<std::endl;

  id="p";
  id += layerId == 0 ? " " : std::to_string(layerId);
  m_gcAnalyzeP.printOut(header, metrics, id, chFmt, printMSEBasedSNR, printSequenceMSE, printMSSSIM, printHexPsnr,
                        printRprPsnr, bitDepths, false, false);
  if( g_verbosity >= DETAILS ) std::cout<<"\n\nP Slices--------------------------------------------------------\n"<<header<<'\n'<<metrics<<std::endl;

  id="b";
  id += layerId == 0 ? " " : std::to_string(layerId);
  m_gcAnalyzeB.printOut(header, metrics, id, chFmt, printMSEBasedSNR, printSequenceMSE, printMSSSIM, printHexPsnr,
                        printRprPsnr, bitDepths, false, false);
  if( g_verbosity >= DETAILS ) std::cout<<"\n\nB Slices--------------------------------------------------------\n"<<header<<'\n'<<metrics<<std::endl;

#if WCG_WPSNR
  if (useLumaWPSNR)
  {
    id="w";
    id += layerId == 0 ? " " : std::to_string(layerId);
    m_gcAnalyzeWPSNR.printOut(header, metrics, id, chFmt, printMSEBasedSNR, printSequenceMSE, printMSSSIM, printHexPsnr,
                              printRprPsnr, bitDepths, useLumaWPSNR, false);
    if( g_verbosity >= DETAILS ) std::cout<<"\nWPSNR SUMMARY --------------------------------------------------------\n"<<header<<'\n'<<metrics<<std::endl;

  }
#endif


  if (!m_pcCfg->getSummaryOutFilename().empty())
  {
    m_gcAnalyzeAll.printSummary(chFmt, printSequenceMSE, printHexPsnr, bitDepths, m_pcCfg->getSummaryOutFilename());
  }

  if (!m_pcCfg->getSummaryPicFilenameBase().empty())
  {
    m_gcAnalyzeI.printSummary(chFmt, printSequenceMSE, printHexPsnr, bitDepths, m_pcCfg->getSummaryPicFilenameBase()+"I.txt");
    m_gcAnalyzeP.printSummary(chFmt, printSequenceMSE, printHexPsnr, bitDepths, m_pcCfg->getSummaryPicFilenameBase()+"P.txt");
    m_gcAnalyzeB.printSummary(chFmt, printSequenceMSE, printHexPsnr, bitDepths, m_pcCfg->getSummaryPicFilenameBase()+"B.txt");
  }

#if WCG_WPSNR
  if (!m_pcCfg->getSummaryOutFilename().empty() && useLumaWPSNR)
  {
    m_gcAnalyzeWPSNR.printSummary(chFmt, printSequenceMSE, printHexPsnr, bitDepths, m_pcCfg->getSummaryOutFilename());
  }
#endif
  if(isField)
  {
    //-- interlaced summary
    Fraction frameRate = m_pcCfg->getFrameRate();
    frameRate.den *= m_pcCfg->getTemporalSubsampleRatio();
    m_gcAnalyzeAllField.setFrameRate(frameRate);
    m_gcAnalyzeAllField.setBits(m_gcAnalyzeAll.getBits());
    // prior to the above statement, the interlace analyser does not contain the correct total number of bits.
    id="a";
    id += layerId == 0 ? " " : std::to_string(layerId);
    m_gcAnalyzeAllField.printOut(header, metrics, id, chFmt, printMSEBasedSNR, printSequenceMSE, printMSSSIM,
                                 printHexPsnr, printRprPsnr, bitDepths, useWPSNR, false);
    if (g_verbosity >= DETAILS)
    {
      std::cout << "\n\nSUMMARY INTERLACED ---------------------------------------------\n"
                << header << '\n'
                << metrics << std::endl;
    }
    if (!m_pcCfg->getSummaryOutFilename().empty())
    {
      m_gcAnalyzeAllField.printSummary(chFmt, printSequenceMSE, printHexPsnr, bitDepths,
                                       m_pcCfg->getSummaryOutFilename());
#if WCG_WPSNR
      if (useLumaWPSNR)
      {
        m_gcAnalyzeWPSNR.printSummary(chFmt, printSequenceMSE, printHexPsnr, bitDepths, m_pcCfg->getSummaryOutFilename());
      }
#endif
    }
  }

  msg( DETAILS,"\nRVM: %.3lf\n", xCalculateRVM() );
}

uint64_t EncGOP::preLoopFilterPicAndCalcDist( Picture* pcPic )
{
  CodingStructure& cs = *pcPic->cs;
  m_pcLoopFilter->deblockingFilterPic( cs );

  const CPelUnitBuf picOrg = pcPic->getRecoBuf();
  const CPelUnitBuf picRec = cs.getRecoBuf();

  uint64_t dist = 0;
  for( uint32_t comp = 0; comp < (uint32_t)picRec.bufs.size(); comp++)
  {
    const ComponentID compID = ComponentID(comp);
    const uint32_t rshift = 2 * DISTORTION_PRECISION_ADJUSTMENT(cs.sps->getBitDepth(toChannelType(compID)));
#if ENABLE_QPA
    CHECK( rshift >= 8, "shifts greater than 7 are not supported." );
#endif
    dist += xFindDistortionPlane(picOrg.get(compID), picRec.get(compID), rshift);
  }
  return dist;
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================
void EncGOP::xInitGOP(int pocLast, int numPicRcvd, bool isField, bool isEncodeLtRef)
{
  CHECK(!(numPicRcvd > 0), "Unspecified error");
  //  Exception for the first frames
  if ((isField && (pocLast == 0 || pocLast == 1)) || (!isField && (pocLast == 0)) || isEncodeLtRef)
  {
    m_iGopSize    = 1;
  }
  else
  {
    m_iGopSize    = m_pcCfg->getGOPSize();
  }
  CHECK(!(m_iGopSize > 0), "Unspecified error");

  return;
}

void EncGOP::xGetBuffer(PicList &rcListPic, std::list<PelUnitBuf *> &rcListPicYuvRecOut, int numPicRcvd, int timeOffset,
                        Picture *&rpcPic, int pocCurr, bool isField)
{
  int i;
  //  Rec. output
  std::list<PelUnitBuf*>::iterator     iterPicYuvRec = rcListPicYuvRecOut.end();

  if (isField && pocCurr > 1 && m_iGopSize!=1)
  {
    timeOffset--;
  }

  int multipleFactor = m_pcCfg->getUseCompositeRef() ? 2 : 1;
  for (i = 0; i < (numPicRcvd * multipleFactor - timeOffset + 1); i += multipleFactor)
  {
    iterPicYuvRec--;
  }

  //  Current pic.
  PicList::iterator        iterPic       = rcListPic.begin();
  while (iterPic != rcListPic.end())
  {
    rpcPic = *(iterPic);
    if( rpcPic->getPOC() == pocCurr && rpcPic->layerId == m_pcEncLib->getLayerId() )
    {
      break;
    }
    iterPic++;
  }

  CHECK(!(rpcPic != nullptr), "Unspecified error");
  CHECK(!(rpcPic->getPOC() == pocCurr), "Unspecified error");

  (**iterPicYuvRec) = rpcPic->getRecoBuf();
  return;
}

void EncGOP::xGetSubpicIdsInPic(std::vector<uint16_t>& subpicIDs, const SPS* sps, const PPS* pps)
{
  subpicIDs.clear();

  if (sps->getSubPicInfoPresentFlag())
  {
    if(sps->getSubPicIdMappingExplicitlySignalledFlag())
    {
      if(sps->getSubPicIdMappingPresentFlag())
      {
        subpicIDs = sps->getSubPicIds();
      }
      else
      {
        subpicIDs = pps->getSubPicIds();
      }
    }
    else
    {
      const int numSubPics = sps->getNumSubPics();
      subpicIDs.resize(numSubPics);
      for (int i = 0 ; i < numSubPics; i++)
      {
        subpicIDs[i] = (uint16_t) i;
      }
    }
  }
}

#if ENABLE_QPA

#ifndef BETA
  #define BETA 0.5 // value between 0.0 and 1; use 0.0 to obtain traditional PSNR
#endif

static inline double calcWeightedSquaredError(const CPelBuf& org,        const CPelBuf& rec,
                                              double &sumAct,            const uint32_t bitDepth,
                                              const uint32_t imageWidth, const uint32_t imageHeight,
                                              const uint32_t offsetX,    const uint32_t offsetY,
                                              int blockWidth,            int blockHeight)
{
  const ptrdiff_t O    = org.stride;
  const ptrdiff_t R    = rec.stride;
  const Pel   *o = org.bufAt(offsetX, offsetY);
  const Pel   *r = rec.bufAt(offsetX, offsetY);
  const int yAct = offsetY > 0 ? 0 : 1;
  const int xAct = offsetX > 0 ? 0 : 1;

  if (offsetY + (uint32_t)blockHeight > imageHeight) blockHeight = imageHeight - offsetY;
  if (offsetX + (uint32_t)blockWidth  > imageWidth ) blockWidth  = imageWidth  - offsetX;

  const int hAct = offsetY + (uint32_t)blockHeight < imageHeight ? blockHeight : blockHeight - 1;
  const int wAct = offsetX + (uint32_t)blockWidth  < imageWidth  ? blockWidth  : blockWidth  - 1;
  uint64_t ssErr = 0; // sum of squared diffs
  uint64_t saAct = 0; // sum of abs. activity
  double msAct;
  int x, y;

  // calculate image differences and activity
  for (y = 0; y < blockHeight; y++)  // error
  {
    for (x = 0; x < blockWidth; x++)
    {
      const     int64_t iDiff = (int64_t)o[y*O + x] - (int64_t)r[y*R + x];
      ssErr += uint64_t(iDiff * iDiff);
    }
  }
  if (wAct <= xAct || hAct <= yAct)
  {
    return (double) ssErr;
  }

  for (y = yAct; y < hAct; y++)   // activity
  {
    for (x = xAct; x < wAct; x++)
    {
      const int f = 12 * (int)o[y*O + x] - 2 * ((int)o[y*O + x-1] + (int)o[y*O + x+1] + (int)o[(y-1)*O + x] + (int)o[(y+1)*O + x])
                       - (int)o[(y-1)*O + x-1] - (int)o[(y-1)*O + x+1] - (int)o[(y+1)*O + x-1] - (int)o[(y+1)*O + x+1];
      saAct += abs(f);
    }
  }

  // calculate weight (mean squared activity)
  msAct = (double)saAct / (double(wAct - xAct) * double(hAct - yAct));

  // lower limit, accounts for high-pass gain
  if (msAct < double(1 << (bitDepth - 4)))
  {
    msAct = double(1 << (bitDepth - 4));
  }

  msAct *= msAct; // because ssErr is squared

  sumAct += msAct; // includes high-pass gain

  // calculate activity weighted error square
  return (double)ssErr * pow(msAct, -1.0 * BETA);
}
#endif // ENABLE_QPA

uint64_t EncGOP::xFindDistortionPlane(const CPelBuf& pic0, const CPelBuf& pic1, const uint32_t rshift
#if ENABLE_QPA
                                    , const uint32_t chromaShiftHor /*= 0*/, const uint32_t chromaShiftVer /*= 0*/
#endif
                                      )
{
  uint64_t     totalDiff;
  const  Pel*  pSrc0 = pic0.bufAt(0, 0);
  const  Pel*  pSrc1 = pic1.bufAt(0, 0);

  CHECK(pic0.width  != pic1.width , "Unspecified error");
  CHECK(pic0.height != pic1.height, "Unspecified error");

  if( rshift > 0 )
  {
#if ENABLE_QPA
    const   uint32_t  BD = rshift;      // image bit-depth
    if (BD >= 8)
    {
      const uint32_t   W = pic0.width;  // image width
      const uint32_t   H = pic0.height; // image height
      const double     R = double(W * H) / (1920.0 * 1080.0);
      const uint32_t   B = Clip3<uint32_t>(0, 128 >> chromaShiftVer, 4 * uint32_t(16.0 * sqrt(R) + 0.5)); // WPSNR block size in integer multiple of 4 (for SIMD, = 64 at full-HD)

      uint32_t x, y;

      if (B < 4) // image is too small to use WPSNR, resort to traditional PSNR
      {
        totalDiff = 0;
        for (y = 0; y < H; y++)
        {
          for (x = 0; x < W; x++)
          {
            const           int64_t iDiff = (int64_t)pSrc0[x] - (int64_t)pSrc1[x];
            totalDiff += uint64_t(iDiff * iDiff);
          }
          pSrc0 += pic0.stride;
          pSrc1 += pic1.stride;
        }
        return totalDiff;
      }

      double wmse = 0.0, sumAct = 0.0; // compute activity normalized SNR value

      for (y = 0; y < H; y += B)
      {
        for (x = 0; x < W; x += B)
        {
          wmse += calcWeightedSquaredError(pic1,   pic0,
                                           sumAct, BD,
                                           W,      H,
                                           x,      y,
                                           B,      B);
        }
      }

      // integer weighted distortion
      sumAct = 16.0 * sqrt ((3840.0 * 2160.0) / double((W << chromaShiftHor) * (H << chromaShiftVer))) * double(1 << (2 * BD - 10));

      return (wmse <= 0.0) ? 0 : uint64_t(wmse * pow(sumAct, BETA) + 0.5);
    }
#endif // ENABLE_QPA
    totalDiff = 0;
    for (int y = 0; y < pic0.height; y++)
    {
      for (int x = 0; x < pic0.width; x++)
      {
        Intermediate_Int temp = pSrc0[x] - pSrc1[x];
        totalDiff += uint64_t((temp * temp) >> rshift);
      }
      pSrc0 += pic0.stride;
      pSrc1 += pic1.stride;
    }
  }
  else
  {
    totalDiff = 0;
    for (int y = 0; y < pic0.height; y++)
    {
      for (int x = 0; x < pic0.width; x++)
      {
        Intermediate_Int temp = pSrc0[x] - pSrc1[x];
        totalDiff += uint64_t(temp * temp);
      }
      pSrc0 += pic0.stride;
      pSrc1 += pic1.stride;
    }
  }

  return totalDiff;
}
#if WCG_WPSNR
double EncGOP::xFindDistortionPlaneWPSNR(const CPelBuf& pic0, const CPelBuf& pic1, const uint32_t rshift, const CPelBuf& picLuma0,
  ComponentID compID, const ChromaFormat chfmt    )
{
  const bool    useLumaWPSNR = m_pcEncLib->getPrintWPSNR();
  if (!useLumaWPSNR)
  {
    return 0;
  }

  double       totalDiffWpsnr;
  const  Pel*  pSrc0 = pic0.bufAt(0, 0);
  const  Pel*  pSrc1 = pic1.bufAt(0, 0);
  const  Pel*  pSrcLuma = picLuma0.bufAt(0, 0);
  CHECK(pic0.width  != pic1.width , "Unspecified error");
  CHECK(pic0.height != pic1.height, "Unspecified error");

  if( rshift > 0 )
  {
    totalDiffWpsnr = 0;
    for (int y = 0; y < pic0.height; y++)
    {
      for (int x = 0; x < pic0.width; x++)
      {
        Intermediate_Int temp = pSrc0[x] - pSrc1[x];
        double dW = m_pcEncLib->getRdCost()->getWPSNRLumaLevelWeight(pSrcLuma[(x << getComponentScaleX(compID, chfmt))]);
        totalDiffWpsnr += ((dW * (double) temp * (double) temp)) * (double) (1 >> rshift);
      }
      pSrc0 += pic0.stride;
      pSrc1 += pic1.stride;
      pSrcLuma += picLuma0.stride << getComponentScaleY(compID, chfmt);
    }
  }
  else
  {
    totalDiffWpsnr = 0;
    for (int y = 0; y < pic0.height; y++)
    {
      for (int x = 0; x < pic0.width; x++)
      {
        Intermediate_Int temp = pSrc0[x] - pSrc1[x];
        double dW = m_pcEncLib->getRdCost()->getWPSNRLumaLevelWeight(pSrcLuma[x << getComponentScaleX(compID, chfmt)]);
        totalDiffWpsnr += dW * (double) temp * (double) temp;
      }
      pSrc0 += pic0.stride;
      pSrc1 += pic1.stride;
      pSrcLuma += picLuma0.stride << getComponentScaleY(compID, chfmt);
    }
  }

  return totalDiffWpsnr;
}
#endif

void EncGOP::xCalculateAddPSNRs(const bool isField, const bool isFieldTopFieldFirst, const int gopId, Picture *pcPic,
                                const AccessUnit &accessUnit, PicList &rcListPic, const double dEncTime,
                                const InputColourSpaceConversion snr_conversion, const bool printFrameMSE,
                                const bool printMSSSIM, double *PSNR_Y, bool isEncodeLtRef)
{
  xCalculateAddPSNR(pcPic, pcPic->getRecoBuf(), accessUnit, (double)dEncTime, snr_conversion,
    printFrameMSE, printMSSSIM, PSNR_Y, isEncodeLtRef);

  //In case of field coding, compute the interlaced PSNR for both fields
  if(isField)
  {
    bool bothFieldsAreEncoded = false;
    int correspondingFieldPOC = pcPic->getPOC();
    int  currentPicGOPPoc      = m_pcCfg->getGOPEntry(gopId).m_POC;
    if(pcPic->getPOC() == 0)
    {
      // particular case for POC 0 and 1.
      // If they are not encoded first and separately from other pictures, we need to change this
      // POC 0 is always encoded first then POC 1 is encoded
      bothFieldsAreEncoded = false;
    }
    else if(pcPic->getPOC() == 1)
    {
      // if we are at POC 1, POC 0 has been encoded for sure
      correspondingFieldPOC = 0;
      bothFieldsAreEncoded = true;
    }
    else
    {
      if(pcPic->getPOC()%2 == 1)
      {
        correspondingFieldPOC -= 1; // all odd POC are associated with the preceding even POC (e.g poc 1 is associated to poc 0)
        currentPicGOPPoc      -= 1;
      }
      else
      {
        correspondingFieldPOC += 1; // all even POC are associated with the following odd POC (e.g poc 0 is associated to poc 1)
        currentPicGOPPoc      += 1;
      }
      for(int i = 0; i < m_iGopSize; i ++)
      {
        if(m_pcCfg->getGOPEntry(i).m_POC == currentPicGOPPoc)
        {
          bothFieldsAreEncoded = m_pcCfg->getGOPEntry(i).m_isEncoded;
          break;
        }
      }
    }

    if(bothFieldsAreEncoded)
    {
      //get complementary top field
      PicList::iterator   iterPic = rcListPic.begin();
      while ((*iterPic)->getPOC() != correspondingFieldPOC)
      {
        iterPic ++;
      }
      Picture* correspondingFieldPic = *(iterPic);

      if ((pcPic->topField && isFieldTopFieldFirst) || (!pcPic->topField && !isFieldTopFieldFirst))
      {
        xCalculateInterlacedAddPSNR(pcPic, correspondingFieldPic, pcPic->getRecoBuf(),
          correspondingFieldPic->getRecoBuf(), snr_conversion, printFrameMSE, printMSSSIM,
          PSNR_Y, isEncodeLtRef);
      }
      else
      {
        xCalculateInterlacedAddPSNR(correspondingFieldPic, pcPic, correspondingFieldPic->getRecoBuf(),
          pcPic->getRecoBuf(), snr_conversion, printFrameMSE, printMSSSIM, PSNR_Y, isEncodeLtRef);
      }
    }
  }
}

void EncGOP::xCalculateAddPSNR(Picture* pcPic, PelUnitBuf cPicD, const AccessUnit& accessUnit,
  double dEncTime, const InputColourSpaceConversion conversion, const bool printFrameMSE, const bool printMSSSIM,
  double* PSNR_Y, bool isEncodeLtRef)
{
  const SPS&         sps = *pcPic->cs->sps;
  const CPelUnitBuf& pic = cPicD;
  CHECK(!(conversion == IPCOLOURSPACE_UNCHANGED), "Unspecified error");
//  const CPelUnitBuf& org = (conversion != IPCOLOURSPACE_UNCHANGED) ? pcPic->getPicYuvTrueOrg()->getBuf() : pcPic->getPicYuvOrg()->getBuf();
  const CPelUnitBuf& org = (sps.getUseLmcs() || m_pcCfg->getGopBasedTemporalFilterEnabled()) ? pcPic->getTrueOrigBuf() : pcPic->getOrigBuf();
#if ENABLE_QPA
  const bool    useWPSNR = m_pcEncLib->getUseWPSNR();
#endif
  double  dPSNR[MAX_NUM_COMPONENT];
  double msssim[MAX_NUM_COMPONENT] = {0.0};
#if WCG_WPSNR
  const bool    useLumaWPSNR = m_pcEncLib->getPrintWPSNR();
  double  dPSNRWeighted[MAX_NUM_COMPONENT];
  double  MSEyuvframeWeighted[MAX_NUM_COMPONENT];
#endif
  double  upscaledPSNR[MAX_NUM_COMPONENT];
  double  upscaledMsssim[MAX_NUM_COMPONENT];
  for(int i=0; i<MAX_NUM_COMPONENT; i++)
  {
    dPSNR[i]=0.0;
#if WCG_WPSNR
    dPSNRWeighted[i]=0.0;
    MSEyuvframeWeighted[i] = 0.0;
#endif
    upscaledPSNR[i] = 0.0;
  }
#if JVET_O0756_CALCULATE_HDRMETRICS
  double deltaE[hdrtoolslib::NB_REF_WHITE];
  double psnrL[hdrtoolslib::NB_REF_WHITE];
  for (int i=0; i<hdrtoolslib::NB_REF_WHITE; i++)
  {
    deltaE[i] = 0.0;
    psnrL[i] = 0.0;
  }
#endif

  PelStorage interm;

  if (conversion != IPCOLOURSPACE_UNCHANGED)
  {
    interm.create(pic.chromaFormat, Area(Position(), pic.Y()));
    VideoIOYuv::colourSpaceConvert(pic, interm, conversion, false);
  }

  const CPelUnitBuf& picC = (conversion == IPCOLOURSPACE_UNCHANGED) ? pic : interm;

  //===== calculate PSNR =====
  double             mseYuvFrame[MAX_NUM_COMPONENT] = { 0, 0, 0 };
  const ChromaFormat formatD = pic.chromaFormat;
  const ChromaFormat format  = sps.getChromaFormatIdc();

  const bool bPicIsField     = pcPic->fieldPic;
  const Slice*  pcSlice      = pcPic->slices[0];

  PelStorage upscaledRec;

  if (m_pcEncLib->isResChangeInClvsEnabled())
  {
    const CPelBuf& upscaledOrg = (sps.getUseLmcs() || m_pcCfg->getGopBasedTemporalFilterEnabled()) ? pcPic->M_BUFS( 0, PIC_TRUE_ORIGINAL_INPUT).get( COMPONENT_Y ) : pcPic->M_BUFS( 0, PIC_ORIGINAL_INPUT).get( COMPONENT_Y );
    upscaledRec.create( pic.chromaFormat, Area( Position(), upscaledOrg ) );

    ScalingRatio scalingRatio;
    // it is assumed that full resolution picture PPS has ppsId 0
    const PPS* pps = m_pcEncLib->getPPS(pcPic->layerId);

    CU::getRprScaling(&sps, pps, pcPic, scalingRatio);

    bool rescaleForDisplay = true;
    Picture::rescalePicture(scalingRatio, picC, pcPic->getScalingWindow(), upscaledRec, pps->getScalingWindow(), format, sps.getBitDepths(), false, false, sps.getHorCollocatedChromaFlag(), sps.getVerCollocatedChromaFlag(), rescaleForDisplay, m_pcCfg->getUpscaleFilerForDisplay());
  }

  Picture* picRefLayer = nullptr;
  if (m_pcEncLib->isRefLayerMetricsEnabled())
  {
    const VPS* vps = pcPic->cs->vps;
    if (vps && m_pcEncLib->getNumRefLayers(vps->getGeneralLayerIdx(pcPic->layerId)) > 0)
    {
      int layerIdx = vps->getGeneralLayerIdx(pcPic->layerId);
      int refLayerId = vps->getLayerId(vps->getDirectRefLayerIdx(layerIdx,0));

      for (Picture* p: *m_pcEncLib->getListPic())
      {
        if (p->layerId == refLayerId  && p->poc == pcPic->poc)
        {
          picRefLayer = p;
          break;
        }
      }
      if (picRefLayer) 
      {
        const CPelUnitBuf& pub1 = org;
        const CPelUnitBuf& pub0 = picRefLayer->getRecoBuf();
        const Window&      wScaling0 = picRefLayer->getScalingWindow();
        const Window&      wScaling1 = pcPic->getScalingWindow();
        int w0 = pub0.get(COMPONENT_Y).width - SPS::getWinUnitX( sps.getChromaFormatIdc() ) * ( wScaling0.getWindowLeftOffset() + wScaling0.getWindowRightOffset() );
        int h0 = pub0.get(COMPONENT_Y).height - SPS::getWinUnitY( sps.getChromaFormatIdc() ) * ( wScaling0.getWindowTopOffset()  + wScaling0.getWindowBottomOffset() );
        int w1 = pub1.get(COMPONENT_Y).width - SPS::getWinUnitX( sps.getChromaFormatIdc() ) * ( wScaling1.getWindowLeftOffset() + wScaling1.getWindowRightOffset() );
        int h1 = pub1.get(COMPONENT_Y).height - SPS::getWinUnitY( sps.getChromaFormatIdc() ) * ( wScaling1.getWindowTopOffset()  + wScaling1.getWindowBottomOffset() );
        int xScale = ((w0 << ScalingRatio::BITS) + (w1 >> 1)) / w1;
        int yScale = ((h0 << ScalingRatio::BITS) + ( h1 >> 1 )) / h1;
        ScalingRatio scalingRatio = { xScale, yScale };          
      
        if (m_pcRefLayerRescaledPicYuv == nullptr)
        {
          m_pcRefLayerRescaledPicYuv = new PelStorage();
          m_pcRefLayerRescaledPicYuv->create(pub1.chromaFormat, Area(Position(), pub1.get(COMPONENT_Y)));
        }

        Picture::rescalePicture( scalingRatio, pub0, wScaling0, *m_pcRefLayerRescaledPicYuv, wScaling1, format, sps.getBitDepths(), false, false, sps.getHorCollocatedChromaFlag(), sps.getVerCollocatedChromaFlag() );
        m_pcEncLib->setRefLayerRescaledAvailable(true);
      }
    }
  }

  for (int comp = 0; comp < ::getNumberValidComponents(formatD); comp++)
  {
    const ComponentID compID = ComponentID(comp);
    const CPelBuf&    p = picC.get(compID);
    const CPelBuf&    o = org.get(compID);

    CHECK(!( p.width  == o.width), "Unspecified error");
    CHECK(!( p.height == o.height), "Unspecified error");

    int padX = m_pcEncLib->getSourcePadding( 0 );
    int padY = m_pcEncLib->getSourcePadding( 1 );

    // when RPR is enabled, picture padding is picture specific due to possible different picture resoluitons, however only full resolution padding is stored in EncLib
    // get per picture padding from the conformance window, in this case if conformance window is set not equal to the padding then PSNR results may be inaccurate
    if (m_pcEncLib->isResChangeInClvsEnabled())
    {
      const Window& conf = pcPic->getConformanceWindow();
      padX = conf.getWindowRightOffset() * SPS::getWinUnitX( format );
      padY = conf.getWindowBottomOffset() * SPS::getWinUnitY( format );
    }

    const uint32_t width = p.width - ( padX >> ::getComponentScaleX( compID, format ) );
    const uint32_t height = p.height - ( padY >> ( !!bPicIsField + ::getComponentScaleY( compID, format ) ) );

    // create new buffers with correct dimensions
    const CPelBuf recPB(p.bufAt(0, 0), p.stride, width, height);
    const CPelBuf orgPB(o.bufAt(0, 0), o.stride, width, height);
    const uint32_t    bitDepth = sps.getBitDepth(toChannelType(compID));
#if ENABLE_QPA
    const uint64_t ssdTemp =
      xFindDistortionPlane(recPB, orgPB, useWPSNR ? bitDepth : 0, ::getComponentScaleX(compID, format),
                           ::getComponentScaleY(compID, format));
#else
    const uint64_t ssdTemp = xFindDistortionPlane(recPB, orgPB, 0);
#endif
    const uint32_t maxval = 255 << (bitDepth - 8);
    const uint32_t size   = width * height;
    const double fRefValue = (double)maxval * maxval * size;
    dPSNR[comp]              = ssdTemp ? 10.0 * log10(fRefValue / (double) ssdTemp) : 999.99;
    mseYuvFrame[comp]        = (double) ssdTemp / size;
    if(printMSSSIM)
    {
      msssim[comp] = xCalculateMSSSIM (o.bufAt(0, 0), o.stride, p.bufAt(0, 0), p.stride, width, height, bitDepth);
    }
#if WCG_WPSNR
    const double uiSSDtempWeighted = xFindDistortionPlaneWPSNR(recPB, orgPB, 0, org.get(COMPONENT_Y), compID, format);
    if (useLumaWPSNR)
    {
      dPSNRWeighted[comp] = uiSSDtempWeighted ? 10.0 * log10(fRefValue / (double)uiSSDtempWeighted) : 999.99;
      MSEyuvframeWeighted[comp] = (double)uiSSDtempWeighted / size;
    }
#endif


    if (m_pcEncLib->isResChangeInClvsEnabled())
    {
      const CPelBuf& upscaledOrg = (sps.getUseLmcs() || m_pcCfg->getGopBasedTemporalFilterEnabled()) ? pcPic->M_BUFS( 0, PIC_TRUE_ORIGINAL_INPUT).get( compID ) : pcPic->M_BUFS( 0, PIC_ORIGINAL_INPUT).get( compID );

      const uint32_t upscaledWidth = upscaledOrg.width - ( m_pcEncLib->getSourcePadding( 0 ) >> ::getComponentScaleX( compID, format ) );
      const uint32_t upscaledHeight = upscaledOrg.height - ( m_pcEncLib->getSourcePadding( 1 ) >> ( !!bPicIsField + ::getComponentScaleY( compID, format ) ) );

      // create new buffers with correct dimensions
      const CPelBuf upscaledRecPB( upscaledRec.get( compID ).bufAt( 0, 0 ), upscaledRec.get( compID ).stride, upscaledWidth, upscaledHeight );
      const CPelBuf upscaledOrgPB( upscaledOrg.bufAt( 0, 0 ), upscaledOrg.stride, upscaledWidth, upscaledHeight );

#if ENABLE_QPA
      const uint64_t upscaledSSD = xFindDistortionPlane( upscaledRecPB, upscaledOrgPB, useWPSNR ? bitDepth : 0, ::getComponentScaleX( compID, format ) );
#else
      const uint64_t scaledSSD = xFindDistortionPlane( upsacledRecPB, upsacledOrgPB, 0 );
#endif

      upscaledPSNR[comp] = upscaledSSD ? 10.0 * log10( (double)maxval * maxval * upscaledWidth * upscaledHeight / (double)upscaledSSD ) : 999.99;
      upscaledMsssim[comp] = xCalculateMSSSIM (upscaledOrgPB.bufAt(0, 0), upscaledOrgPB.stride, upscaledRecPB.bufAt(0, 0), upscaledRecPB.stride, upscaledWidth, upscaledHeight, bitDepth);
    }
    else if (picRefLayer)
    {
      const CPelBuf& p = m_pcRefLayerRescaledPicYuv->get(compID);
      const CPelBuf& o = org.get(compID);
#if ENABLE_QPA
      const uint64_t upscaledSSD = xFindDistortionPlane(p, o, useWPSNR ? bitDepth : 0, ::getComponentScaleX(compID, format), ::getComponentScaleY(compID, format));
#else
      const uint64_t upscaledSSD = xFindDistortionPlane(p, o, 0);
#endif
      upscaledPSNR[comp] = upscaledSSD ? 10.0 * log10((double) fRefValue / (double) upscaledSSD) : 999.99;
      if (printMSSSIM)
      {
        const uint32_t upscaledWidth = o.width - ( m_pcEncLib->getSourcePadding( 0 ) >> ::getComponentScaleX( compID, format ) );
        const uint32_t upscaledHeight = o.height - ( m_pcEncLib->getSourcePadding( 1 ) >> ( !!bPicIsField + ::getComponentScaleY( compID, format ) ) );
        upscaledMsssim[comp] = xCalculateMSSSIM(o.bufAt(0, 0), o.stride, p.bufAt(0, 0), p.stride, upscaledWidth, upscaledHeight, bitDepth);
      }
    }
  }

#if EXTENSION_360_VIDEO
  m_ext360.calculatePSNRs(pcPic);
#endif

#if JVET_O0756_CALCULATE_HDRMETRICS
  const bool calculateHdrMetrics = m_pcEncLib->getCalculateHdrMetrics();
  if (calculateHdrMetrics)
  {
    auto beforeTime = std::chrono::steady_clock::now();
    xCalculateHDRMetrics(pcPic, deltaE, psnrL);
    auto elapsed = std::chrono::steady_clock::now() - beforeTime;
    m_metricTime += elapsed;
  }
#endif

  /* calculate the size of the access unit, excluding:
   *  - any AnnexB contributions (start_code_prefix, zero_byte, etc.,)
   *  - SEI NAL units
   */
  uint32_t numRBSPBytes = 0;
  for (AccessUnit::const_iterator it = accessUnit.begin(); it != accessUnit.end(); it++)
  {
    uint32_t numRBSPBytes_nal = uint32_t((*it)->m_nalUnitData.str().size());
    if (m_pcCfg->getSummaryVerboseness() > 0)
    {
      msg( NOTICE, "*** %6s numBytesInNALunit: %u\n", nalUnitTypeToString((*it)->m_nalUnitType), numRBSPBytes_nal);
    }
    if( ( *it )->m_nalUnitType != NAL_UNIT_PREFIX_SEI && ( *it )->m_nalUnitType != NAL_UNIT_SUFFIX_SEI )
    {
      numRBSPBytes += numRBSPBytes_nal;
      if (it == accessUnit.begin() || (*it)->m_nalUnitType == NAL_UNIT_OPI || (*it)->m_nalUnitType == NAL_UNIT_VPS || (*it)->m_nalUnitType == NAL_UNIT_DCI || (*it)->m_nalUnitType == NAL_UNIT_SPS || (*it)->m_nalUnitType == NAL_UNIT_PPS || (*it)->m_nalUnitType == NAL_UNIT_PREFIX_APS || (*it)->m_nalUnitType == NAL_UNIT_SUFFIX_APS)
      {
        numRBSPBytes += 4;
      }
      else
      {
        numRBSPBytes += 3;
      }
    }
  }

  uint32_t uibits = numRBSPBytes * 8;
  m_rvm.push_back(uibits);

  //===== add PSNR =====
  m_gcAnalyzeAll.addResult(dPSNR, (double) uibits, mseYuvFrame, upscaledPSNR, msssim, upscaledMsssim, isEncodeLtRef);
#if EXTENSION_360_VIDEO
  m_ext360.addResult(m_gcAnalyzeAll);
#endif
#if JVET_O0756_CALCULATE_HDRMETRICS
  if (calculateHdrMetrics)
  {
    m_gcAnalyzeAll.addHDRMetricsResult(deltaE, psnrL);
  }
#endif
  if (pcSlice->isIntra())
  {
    m_gcAnalyzeI.addResult(dPSNR, (double) uibits, mseYuvFrame, upscaledPSNR, msssim, upscaledMsssim, isEncodeLtRef);
    *PSNR_Y = dPSNR[COMPONENT_Y];
#if EXTENSION_360_VIDEO
    m_ext360.addResult(m_gcAnalyzeI);
#endif
#if JVET_O0756_CALCULATE_HDRMETRICS
    if (calculateHdrMetrics)
    {
      m_gcAnalyzeI.addHDRMetricsResult(deltaE, psnrL);
    }
#endif
  }
  if (pcSlice->isInterP())
  {
    m_gcAnalyzeP.addResult(dPSNR, (double) uibits, mseYuvFrame, upscaledPSNR, msssim, upscaledMsssim, isEncodeLtRef);
    *PSNR_Y = dPSNR[COMPONENT_Y];
#if EXTENSION_360_VIDEO
    m_ext360.addResult(m_gcAnalyzeP);
#endif
#if JVET_O0756_CALCULATE_HDRMETRICS
    if (calculateHdrMetrics)
    {
      m_gcAnalyzeP.addHDRMetricsResult(deltaE, psnrL);
    }
#endif
  }
  if (pcSlice->isInterB())
  {
    m_gcAnalyzeB.addResult(dPSNR, (double) uibits, mseYuvFrame, upscaledPSNR, msssim, upscaledMsssim, isEncodeLtRef);
    *PSNR_Y = dPSNR[COMPONENT_Y];
#if EXTENSION_360_VIDEO
    m_ext360.addResult(m_gcAnalyzeB);
#endif
#if JVET_O0756_CALCULATE_HDRMETRICS
    if (calculateHdrMetrics)
    {
      m_gcAnalyzeB.addHDRMetricsResult(deltaE, psnrL);
    }
#endif
  }
#if WCG_WPSNR
  if (useLumaWPSNR)
  {
    m_gcAnalyzeWPSNR.addResult( dPSNRWeighted, (double)uibits, MSEyuvframeWeighted, upscaledPSNR, msssim, upscaledMsssim, isEncodeLtRef );
  }
#endif

  char c = (pcSlice->isIntra() ? 'I' : pcSlice->isInterP() ? 'P' : 'B');
  if (! pcPic->referenced)
  {
    c += 32;
  }
  if (m_pcCfg->getDependentRAPIndicationSEIEnabled() && pcSlice->isDRAP())
  {
    c = 'D';
  }
  if (m_pcCfg->getEdrapIndicationSEIEnabled() && pcSlice->getEdrapRapId() > 0)
  {
    c = 'E';
  }

  if( g_verbosity >= NOTICE )
  {
    msg( NOTICE, "POC %4d LId: %2d TId: %1d ( %s, %c-SLICE, QP %d ) %10d bits",
         pcSlice->getPOC(),
         pcSlice->getPic()->layerId,
         pcSlice->getTLayer(),
         nalUnitTypeToString(pcSlice->getNalUnitType()),
         c,
         pcSlice->getSliceQp(),
         uibits );

    msg( NOTICE, " [Y %6.4lf dB    U %6.4lf dB    V %6.4lf dB]", dPSNR[COMPONENT_Y], dPSNR[COMPONENT_Cb], dPSNR[COMPONENT_Cr] );

#if EXTENSION_360_VIDEO
    m_ext360.printPerPOCInfo(NOTICE);
#endif

    if (m_pcEncLib->getPrintHexPsnr())
    {
      uint64_t xPsnr[MAX_NUM_COMPONENT];
      for (int i = 0; i < MAX_NUM_COMPONENT; i++)
      {
        std::copy(reinterpret_cast<uint8_t *>(&dPSNR[i]), reinterpret_cast<uint8_t *>(&dPSNR[i]) + sizeof(dPSNR[i]),
                  reinterpret_cast<uint8_t *>(&xPsnr[i]));
      }
      msg(NOTICE, " [xY %16" PRIx64 " xU %16" PRIx64 " xV %16" PRIx64 "]", xPsnr[COMPONENT_Y], xPsnr[COMPONENT_Cb], xPsnr[COMPONENT_Cr]);

#if EXTENSION_360_VIDEO
      m_ext360.printPerPOCInfo(NOTICE, true);
#endif
    }
    if (printMSSSIM)
    {
      msg( NOTICE, " [MS-SSIM Y %1.6lf    U %1.6lf    V %1.6lf]", msssim[COMPONENT_Y], msssim[COMPONENT_Cb], msssim[COMPONENT_Cr] );
    }

    if( printFrameMSE )
    {
      msg(NOTICE, " [Y MSE %6.4lf  U MSE %6.4lf  V MSE %6.4lf]", mseYuvFrame[COMPONENT_Y], mseYuvFrame[COMPONENT_Cb],
          mseYuvFrame[COMPONENT_Cr]);
    }
#if WCG_WPSNR
    if (useLumaWPSNR)
    {
      msg(NOTICE, " [WY %6.4lf dB    WU %6.4lf dB    WV %6.4lf dB]", dPSNRWeighted[COMPONENT_Y], dPSNRWeighted[COMPONENT_Cb], dPSNRWeighted[COMPONENT_Cr]);

      if (m_pcEncLib->getPrintHexPsnr())
      {
        uint64_t xPsnrWeighted[MAX_NUM_COMPONENT];
        for (int i = 0; i < MAX_NUM_COMPONENT; i++)
        {
          std::copy(reinterpret_cast<uint8_t *>(&dPSNRWeighted[i]),
                    reinterpret_cast<uint8_t *>(&dPSNRWeighted[i]) + sizeof(dPSNRWeighted[i]),
                    reinterpret_cast<uint8_t *>(&xPsnrWeighted[i]));
        }
        msg(NOTICE, " [xWY %16" PRIx64 " xWU %16" PRIx64 " xWV %16" PRIx64 "]", xPsnrWeighted[COMPONENT_Y], xPsnrWeighted[COMPONENT_Cb], xPsnrWeighted[COMPONENT_Cr]);
      }
    }
#endif
#if JVET_O0756_CALCULATE_HDRMETRICS
    if(calculateHdrMetrics)
    {
      for (int i=0; i<1; i++)
      {
        msg(NOTICE, " [DeltaE%d %6.4lf dB]", (int)m_pcCfg->getWhitePointDeltaE(i), deltaE[i]);
        if (m_pcEncLib->getPrintHexPsnr())
        {
          int64_t xdeltaE[MAX_NUM_COMPONENT];
          for (int i = 0; i < 1; i++)
          {
            std::copy_n(reinterpret_cast<uint8_t*>(&deltaE[i]), sizeof(deltaE[i]),
                        reinterpret_cast<uint8_t*>(&xdeltaE[i]));
          }
          msg(NOTICE, " [xDeltaE%d %16" PRIx64 "]", (int)m_pcCfg->getWhitePointDeltaE(i), xdeltaE[0]);
        }
      }
      for (int i=0; i<1; i++)
      {
        msg(NOTICE, " [PSNRL%d %6.4lf dB]", (int)m_pcCfg->getWhitePointDeltaE(i), psnrL[i]);

        if (m_pcEncLib->getPrintHexPsnr())
        {
          int64_t xpsnrL[MAX_NUM_COMPONENT];
          for (int i = 0; i < 1; i++)
          {
            std::copy_n(reinterpret_cast<uint8_t*>(&psnrL[i]), sizeof(psnrL[i]),
                        reinterpret_cast<uint8_t*>(&xpsnrL[i]));
          }

          msg(NOTICE, " [xPSNRL%d %16" PRIx64 "]", (int) m_pcCfg->getWhitePointDeltaE(i), xpsnrL[0]);
        }
      }
    }
#endif
    msg(NOTICE, m_pcCfg->getPrintHighPrecEncTime() ? " [ET %6.3f ]" : " [ET %5.0f ]", dEncTime);

    // msg( SOME, " [WP %d]", pcSlice->getUseWeightedPrediction());

    for (int refList = 0; refList < 2; refList++)
    {
      msg(NOTICE, " [L%d", refList);
      for (int refIndex = 0; refIndex < pcSlice->getNumRefIdx(RefPicList(refList)); refIndex++)
      {
        const ScalingRatio &scaleRatio = pcSlice->getScalingRatio(RefPicList(refList), refIndex);

        if (pcPic->cs->picHeader->getEnableTMVPFlag() && pcSlice->getColFromL0Flag() == bool(1 - refList)
            && pcSlice->getColRefIdx() == refIndex)
        {
          if (scaleRatio != SCALE_1X)
          {
            msg(NOTICE, " %dc(%1.2lfx, %1.2lfx)", pcSlice->getRefPOC(RefPicList(refList), refIndex),
                double(scaleRatio.x) / (1 << ScalingRatio::BITS), double(scaleRatio.y) / (1 << ScalingRatio::BITS));
          }
          else
          {
            msg(NOTICE, " %dc", pcSlice->getRefPOC(RefPicList(refList), refIndex));
          }
        }
        else
        {
          if (scaleRatio != SCALE_1X)
          {
            msg(NOTICE, " %d(%1.2lfx, %1.2lfx)", pcSlice->getRefPOC(RefPicList(refList), refIndex),
                double(scaleRatio.x) / (1 << ScalingRatio::BITS), double(scaleRatio.y) / (1 << ScalingRatio::BITS));
          }
          else
          {
            msg(NOTICE, " %d", pcSlice->getRefPOC(RefPicList(refList), refIndex));
          }
        }

        if (pcSlice->getRefPOC(RefPicList(refList), refIndex) == pcSlice->getPOC())
        {
          msg(NOTICE, ".%d", pcSlice->getRefPic(RefPicList(refList), refIndex)->layerId);
        }
      }
      msg( NOTICE, "]" );
    }
    if (m_pcEncLib->isResChangeInClvsEnabled() || m_pcEncLib->isRefLayerRescaledAvailable())
    {
      msg( NOTICE, " [Y2 %6.4lf dB  U2 %6.4lf dB  V2 %6.4lf dB]", upscaledPSNR[COMPONENT_Y], upscaledPSNR[COMPONENT_Cb], upscaledPSNR[COMPONENT_Cr] );
      if (printMSSSIM)
      {
        msg( NOTICE, " MS-SSIM2: [Y %1.6lf  U %1.6lf  V %1.6lf ]", upscaledMsssim[COMPONENT_Y], upscaledMsssim[COMPONENT_Cb], upscaledMsssim[COMPONENT_Cr] );
      }
    }
  }
  else if( g_verbosity >= INFO )
  {
    std::cout << "\r\t" << pcSlice->getPOC();
    std::cout.flush();
  }
#if GREEN_METADATA_SEI_ENABLED
  m_SEIGreenQualityMetrics.ssim = msssim[0];
  m_SEIGreenQualityMetrics.wpsnr = dPSNR[0];
#endif
}

#if GREEN_METADATA_SEI_ENABLED
void EncGOP::xCalculateGreenComplexityMetrics( FeatureCounterStruct featureCounter, FeatureCounterStruct featureCounterReference, SEIGreenMetadataInfo* seiGreenMetadataInfo)
{
  double chromaFormatMultiplier = 0;
  
  if (featureCounter.isYUV400 == 1)
  {
    chromaFormatMultiplier = 1;
  }
  else if (featureCounter.isYUV420)
  {
    chromaFormatMultiplier = 1.5;
  }
  else if (featureCounter.isYUV422)
  {
    chromaFormatMultiplier = 2;
  }
  else if (featureCounter.isYUV444)
  {
    chromaFormatMultiplier = 3;
  }
  
  // Initialize
  int64_t totalNum4BlocksPic = 0;
  int64_t totalNum4BlocksInPeriod = 0;
  int64_t maxNumDeblockingInstances = 0;
  
  double numNonZeroBlocks = 0;
  double numNonZero4_8_16_Blocks = 0;
  double numNonZero32_64_128_Blocks = 0;
  double numNonZero256_512_1024_Blocks = 0;
  double numNonZero2048_4096_Blocks = 0;
  double numIntraPredictedBlocks = 0;
  double numBiAndGpmPredictedBlocks = 0;
  double numBDOFPredictedBlocks = 0;
  double numDeblockingInstances = 0;
  double numSaoFilteredBlocks = 0;
  double numAlfFilteredBlocks = 0;
  // Calculate difference
  FeatureCounterStruct featureCounterDifference;
  
  featureCounterDifference.iSlices  = featureCounter.iSlices - featureCounterReference.iSlices;
  featureCounterDifference.bSlices  = featureCounter.bSlices - featureCounterReference.bSlices;
  featureCounterDifference.pSlices  = featureCounter.pSlices - featureCounterReference.pSlices;
  featureCounterDifference.nrOfCoeff = featureCounter.nrOfCoeff - featureCounterReference.nrOfCoeff;
  featureCounterDifference.biPredPel = featureCounter.biPredPel - featureCounterReference.biPredPel;
  featureCounterDifference.boundaryStrength[0] = featureCounter.boundaryStrength[0] - featureCounterReference.boundaryStrength[0];
  featureCounterDifference.boundaryStrength[1] = featureCounter.boundaryStrength[1] - featureCounterReference.boundaryStrength[1];
  featureCounterDifference.boundaryStrength[2] = featureCounter.boundaryStrength[2] - featureCounterReference.boundaryStrength[2];
  featureCounterDifference.saoLumaEO = featureCounter.saoLumaEO - featureCounterReference.saoLumaEO;
  featureCounterDifference.saoLumaBO = featureCounter.saoLumaBO - featureCounterReference.saoLumaBO;
  featureCounterDifference.saoChromaEO = featureCounter.saoChromaEO - featureCounterReference.saoChromaEO;
  featureCounterDifference.saoChromaBO = featureCounter.saoChromaBO - featureCounterReference.saoChromaBO;
  featureCounterDifference.saoLumaPels = featureCounter.saoLumaPels - featureCounterReference.saoLumaPels;
  featureCounterDifference.saoChromaPels = featureCounter.saoChromaPels - featureCounterReference.saoChromaPels;
  featureCounterDifference.alfLumaType7 = featureCounter.alfLumaType7 - featureCounterReference.alfLumaType7;
  featureCounterDifference.alfChromaType5 = featureCounter.alfChromaType5 - featureCounterReference.alfChromaType5;
  featureCounterDifference.alfLumaPels = featureCounter.alfLumaPels - featureCounterReference.alfLumaPels;
  featureCounterDifference.alfChromaPels = featureCounter.alfChromaPels - featureCounterReference.alfChromaPels;
  
  
  for (int i = 0; i < MAX_CU_DEPTH+1; i++)
  {
    for (int j = 0; j < MAX_CU_DEPTH+1; j++)
    {
      featureCounterDifference.transformBlocks[i][j] = featureCounter.transformBlocks[i][j] - featureCounterReference.transformBlocks[i][j];
      featureCounterDifference.intraBlockSizes[i][j] = featureCounter.intraBlockSizes[i][j] - featureCounterReference.intraBlockSizes[i][j];
      featureCounterDifference.geo[i][j] = featureCounter.geo[i][j] - featureCounterReference.geo[i][j];
      featureCounterDifference.bdofBlocks[i][j] = featureCounter.bdofBlocks[i][j] - featureCounterReference.bdofBlocks[i][j];
    }
  }
  
  
  //Calculate complexity metrics
  totalNum4BlocksPic = int(chromaFormatMultiplier * featureCounter.width * featureCounter.height / 4);
  totalNum4BlocksInPeriod = int((featureCounterDifference.iSlices + featureCounterDifference.pSlices + featureCounterDifference.bSlices) * totalNum4BlocksPic);
  maxNumDeblockingInstances = int(chromaFormatMultiplier * totalNum4BlocksInPeriod - 2 * (featureCounter.width + featureCounter.height) * 2);
  
  for (int i = 0; i < MAX_CU_DEPTH+1; i++)
  {
    for(int j = 0; j < MAX_CU_DEPTH+1; j++)
    {
      double numberOfPels = pow(2,i) * pow(2,j);
      numNonZeroBlocks += double(featureCounterDifference.transformBlocks[i][j] * numberOfPels / 4);
      numIntraPredictedBlocks += double(featureCounterDifference.intraBlockSizes[i][j] * numberOfPels / 4);
      
      if (numberOfPels == 4 || numberOfPels == 8 || numberOfPels == 16)
      {
        numNonZero4_8_16_Blocks += double(featureCounterDifference.transformBlocks[i][j] * numberOfPels / 4);
      }
      
      if (numberOfPels == 32 || numberOfPels == 64 || numberOfPels == 128)
      {
        numNonZero32_64_128_Blocks += double(featureCounterDifference.transformBlocks[i][j] * numberOfPels / 4);
      }
      
      if (numberOfPels == 256 || numberOfPels == 512 || numberOfPels == 1024)
      {
        numNonZero256_512_1024_Blocks += double(featureCounterDifference.transformBlocks[i][j] * numberOfPels / 4);
      }
      
      if (numberOfPels == 2048 || numberOfPels == 4096 )
      {
        numNonZero2048_4096_Blocks += double(featureCounterDifference.transformBlocks[i][j] * numberOfPels / 4);
      }
      numBDOFPredictedBlocks += double(featureCounterDifference.bdofBlocks[i][j] * numberOfPels / 4);
      numBiAndGpmPredictedBlocks += double(featureCounterDifference.geo[i][j] * numberOfPels / 4);
    }
  }
  
  numBiAndGpmPredictedBlocks += double(featureCounterDifference.biPredPel/4);
  numDeblockingInstances = double(featureCounterDifference.boundaryStrength[0] + featureCounterDifference.boundaryStrength[1] + featureCounterDifference.boundaryStrength[2]);
  numSaoFilteredBlocks   = double(featureCounterDifference.saoLumaPels + featureCounterDifference.saoChromaPels)/4;
  numAlfFilteredBlocks   = double(featureCounterDifference.alfLumaPels + featureCounterDifference.alfChromaPels)/4;
  
  seiGreenMetadataInfo->m_greenComplexityMetrics.portionNonZeroBlocksArea = int(floor( 255.0 * numNonZeroBlocks / totalNum4BlocksInPeriod));
  seiGreenMetadataInfo->m_greenComplexityMetrics.portionNonZero_4_8_16BlocksArea = int(floor(255.0 * numNonZero4_8_16_Blocks / totalNum4BlocksInPeriod));
  seiGreenMetadataInfo->m_greenComplexityMetrics.portionNonZero_32_64_128BlocksArea = int(floor( 255.0 * numNonZero32_64_128_Blocks / totalNum4BlocksInPeriod));
  seiGreenMetadataInfo->m_greenComplexityMetrics.portionNonZero_256_512_1024BlocksArea = int(floor( 255.0 * numNonZero256_512_1024_Blocks / totalNum4BlocksInPeriod));
  seiGreenMetadataInfo->m_greenComplexityMetrics.portionNonZero_2048_4096BlocksArea = int(floor( 255.0 * numNonZero2048_4096_Blocks / totalNum4BlocksInPeriod));
  if (numNonZeroBlocks != 0)
  {
    seiGreenMetadataInfo->m_greenComplexityMetrics.portionNonZeroTransformCoefficientsArea =
      int(floor(255.0 * featureCounterDifference.nrOfCoeff / (4 *numNonZeroBlocks)));
  }
  
  seiGreenMetadataInfo->m_greenComplexityMetrics.portionIntraPredictedBlocksArea = int(floor( 255.0 * numIntraPredictedBlocks / totalNum4BlocksInPeriod));
  seiGreenMetadataInfo->m_greenComplexityMetrics.portionBiAndGpmPredictedBlocksArea = int(floor( 255.0 * numBiAndGpmPredictedBlocks / totalNum4BlocksInPeriod));
  seiGreenMetadataInfo->m_greenComplexityMetrics.portionBdofBlocksArea  = int(floor( 255.0 * numBDOFPredictedBlocks / totalNum4BlocksInPeriod));
  seiGreenMetadataInfo->m_greenComplexityMetrics.portionDeblockingInstances = int(floor( 255.0 * numDeblockingInstances / maxNumDeblockingInstances));
  seiGreenMetadataInfo->m_greenComplexityMetrics.portionSaoInstances = int(floor( 255.0 * numSaoFilteredBlocks / totalNum4BlocksInPeriod));
  seiGreenMetadataInfo->m_greenComplexityMetrics.portionAlfInstances = int(floor( 255.0 * numAlfFilteredBlocks / totalNum4BlocksInPeriod));
  seiGreenMetadataInfo->m_numPictures = int(featureCounterDifference.iSlices + featureCounterDifference.bSlices +featureCounterDifference.pSlices);
  seiGreenMetadataInfo->m_numSeconds =
    int(floor(seiGreenMetadataInfo->m_numPictures / m_pcCfg->getFrameRate().getFloatVal()));
}
#endif

double EncGOP::xCalculateMSSSIM(const Pel *org, const ptrdiff_t orgStride, const Pel *rec, const ptrdiff_t recStride,
                                const int width, const int height, const uint32_t bitDepth)
{
  const int MAX_MSSSIM_SCALE  = 5;
  const int WEIGHTING_MID_TAP = 5;
  const int WEIGHTING_SIZE    = WEIGHTING_MID_TAP*2+1;

  uint32_t maxScale;

  // For low resolution videos determine number of scales
  if (width < 22 || height < 22)
  {
    maxScale = 1;
  }
  else if (width < 44 || height < 44)
  {
    maxScale = 2;
  }
  else if (width < 88 || height < 88)
  {
    maxScale = 3;
  }
  else if (width < 176 || height < 176)
  {
    maxScale = 4;
  }
  else
  {
    maxScale = 5;
  }

  assert(maxScale>0 && maxScale<=MAX_MSSSIM_SCALE);

  //Normalized Gaussian mask design, 11*11, s.d. 1.5
  double weights[WEIGHTING_SIZE][WEIGHTING_SIZE];
  double coeffSum=0.0;
  for(int y=0; y<WEIGHTING_SIZE; y++)
  {
    for(int x=0; x<WEIGHTING_SIZE; x++)
    {
      weights[y][x] =
        exp(-((y - WEIGHTING_MID_TAP) * (y - WEIGHTING_MID_TAP) + (x - WEIGHTING_MID_TAP) * (x - WEIGHTING_MID_TAP))
            / (WEIGHTING_MID_TAP - 0.5));
      coeffSum += weights[y][x];
    }
  }

  for(int y=0; y<WEIGHTING_SIZE; y++)
  {
    for(int x=0; x<WEIGHTING_SIZE; x++)
    {
      weights[y][x] /=coeffSum;
    }
  }

  //Resolution based weights
  const double exponentWeights[MAX_MSSSIM_SCALE][MAX_MSSSIM_SCALE] = {{1.0,    0,      0,      0,      0     },
                                                                      {0.1356, 0.8644, 0,      0,      0     },
                                                                      {0.0711, 0.4530, 0.4760, 0,      0     },
                                                                      {0.0517, 0.3295, 0.3462, 0.2726, 0     },
                                                                      {0.0448, 0.2856, 0.3001, 0.2363, 0.1333}};

  //Downsampling of data:
  std::vector<double> original[MAX_MSSSIM_SCALE];
  std::vector<double> recon[MAX_MSSSIM_SCALE];

  for(uint32_t scale=0; scale<maxScale; scale++)
  {
    const int scaledHeight = height >> scale;
    const int scaledWidth  = width  >> scale;
    original[scale].resize(scaledHeight*scaledWidth, double(0));
    recon[scale].resize(scaledHeight*scaledWidth, double(0));
  }

  // Initial [0] arrays to be a copy of the source data (but stored in array "double", not Pel array).
  for(int y=0; y<height; y++)
  {
    for(int x=0; x<width; x++)
    {
      original[0][y*width+x] = org[y*orgStride+x];
      recon[0][   y*width+x] = rec[y*recStride+x];
    }
  }

  // Set up other arrays to be average value of each 2x2 sample.
  for(uint32_t scale=1; scale<maxScale; scale++)
  {
    const int scaledHeight = height >> scale;
    const int scaledWidth  = width  >> scale;
    for(int y=0; y<scaledHeight; y++)
    {
      for(int x=0; x<scaledWidth; x++)
      {
        original[scale][y*scaledWidth+x]= (original[scale-1][ 2*y   *(2*scaledWidth)+2*x  ] +
                                           original[scale-1][ 2*y   *(2*scaledWidth)+2*x+1] +
                                           original[scale-1][(2*y+1)*(2*scaledWidth)+2*x  ] +
                                           original[scale-1][(2*y+1)*(2*scaledWidth)+2*x+1]) / 4.0;
        recon[scale][y*scaledWidth+x]=    (   recon[scale-1][ 2*y   *(2*scaledWidth)+2*x  ] +
                                              recon[scale-1][ 2*y   *(2*scaledWidth)+2*x+1] +
                                              recon[scale-1][(2*y+1)*(2*scaledWidth)+2*x  ] +
                                              recon[scale-1][(2*y+1)*(2*scaledWidth)+2*x+1]) / 4.0;
      }
    }
  }

  // Calculate MS-SSIM:
  const uint32_t   maxValue  = (1<<bitDepth)-1;
  const double c1        = (0.01*maxValue)*(0.01*maxValue);
  const double c2        = (0.03*maxValue)*(0.03*maxValue);

  double finalMSSSIM = 1.0;

  for(uint32_t scale=0; scale<maxScale; scale++)
  {
    const int scaledHeight    = height >> scale;
    const int scaledWidth     = width  >> scale;
    const int blocksPerRow    = scaledWidth-WEIGHTING_SIZE+1;
    const int blocksPerColumn = scaledHeight-WEIGHTING_SIZE+1;
    const int totalBlocks     = blocksPerRow*blocksPerColumn;

    double meanSSIM= 0.0;

    for(int blockIndexY=0; blockIndexY<blocksPerColumn; blockIndexY++)
    {
      for(int blockIndexX=0; blockIndexX<blocksPerRow; blockIndexX++)
      {
        double muOrg          =0.0;
        double muRec          =0.0;
        double muOrigSqr      =0.0;
        double muRecSqr       =0.0;
        double muOrigMultRec  =0.0;

        for(int y=0; y<WEIGHTING_SIZE; y++)
        {
          for(int x=0;x<WEIGHTING_SIZE; x++)
          {
            const double gaussianWeight=weights[y][x];
            const int    sampleOffset=(blockIndexY+y)*scaledWidth+(blockIndexX+x);
            const double orgPel=original[scale][sampleOffset];
            const double recPel=   recon[scale][sampleOffset];

            muOrg        +=orgPel*       gaussianWeight;
            muRec        +=recPel*       gaussianWeight;
            muOrigSqr    +=orgPel*orgPel*gaussianWeight;
            muRecSqr     +=recPel*recPel*gaussianWeight;
            muOrigMultRec+=orgPel*recPel*gaussianWeight;
          }
        }

        const double sigmaSqrOrig = muOrigSqr    -(muOrg*muOrg);
        const double sigmaSqrRec  = muRecSqr     -(muRec*muRec);
        const double sigmaOrigRec = muOrigMultRec-(muOrg*muRec);

        double blockSSIMVal = ((2.0*sigmaOrigRec + c2)/(sigmaSqrOrig+sigmaSqrRec + c2));
        if(scale == maxScale-1)
        {
          blockSSIMVal*=(2.0*muOrg*muRec + c1)/(muOrg*muOrg+muRec*muRec + c1);
        }

        meanSSIM += blockSSIMVal;
      }
    }

    meanSSIM /=totalBlocks;

    finalMSSSIM *= pow(meanSSIM, exponentWeights[maxScale-1][scale]);
  }

  return finalMSSSIM;
}

#if JVET_O0756_CALCULATE_HDRMETRICS
void EncGOP::xCalculateHDRMetrics( Picture* pcPic, double deltaE[hdrtoolslib::NB_REF_WHITE], double psnrL[hdrtoolslib::NB_REF_WHITE])
{
  copyBuftoFrame(pcPic);

  ChromaFormat chFmt =  pcPic->chromaFormat;

  if (chFmt != ChromaFormat::_444)
  {
    m_pcConvertFormat->process(m_ppcFrameOrg[1], m_ppcFrameOrg[0]);
    m_pcConvertFormat->process(m_ppcFrameRec[1], m_ppcFrameRec[0]);
  }

  m_pcConvertIQuantize->process(m_ppcFrameOrg[2], m_ppcFrameOrg[1]);
  m_pcConvertIQuantize->process(m_ppcFrameRec[2], m_ppcFrameRec[1]);

  m_pcColorTransform->process(m_ppcFrameOrg[3], m_ppcFrameOrg[2]);
  m_pcColorTransform->process(m_ppcFrameRec[3], m_ppcFrameRec[2]);

  m_pcTransferFct->forward(m_ppcFrameOrg[4], m_ppcFrameOrg[3]);
  m_pcTransferFct->forward(m_ppcFrameRec[4], m_ppcFrameRec[3]);

  // Calculate the Metrics
  m_pcDistortionDeltaE->computeMetric(m_ppcFrameOrg[4], m_ppcFrameRec[4]);

  *deltaE = m_pcDistortionDeltaE->getDeltaE();
  *psnrL  = m_pcDistortionDeltaE->getPsnrL();
}

void EncGOP::copyBuftoFrame( Picture* pcPic )
{
  int cropOffsetLeft   = m_pcCfg->getCropOffsetLeft();
  int cropOffsetTop    = m_pcCfg->getCropOffsetTop();
  int cropOffsetRight  = m_pcCfg->getCropOffsetRight();
  int cropOffsetBottom = m_pcCfg->getCropOffsetBottom();

  int height = pcPic->getTrueOrigBuf(COMPONENT_Y).height - cropOffsetLeft + cropOffsetRight;
  int width  = pcPic->getTrueOrigBuf(COMPONENT_Y).width - cropOffsetTop + cropOffsetBottom;

  ChromaFormat chFmt =  pcPic->chromaFormat;

  Pel *pOrg = pcPic->getTrueOrigBuf(COMPONENT_Y).buf;
  Pel* pRec = pcPic->getRecoBuf(COMPONENT_Y).buf;

  uint16_t* yOrg = m_ppcFrameOrg[0]->m_ui16Comp[hdrtoolslib::Y_COMP];
  uint16_t* yRec = m_ppcFrameRec[0]->m_ui16Comp[hdrtoolslib::Y_COMP];
  uint16_t* uOrg = m_ppcFrameOrg[0]->m_ui16Comp[hdrtoolslib::Cb_COMP];
  uint16_t* uRec = m_ppcFrameRec[0]->m_ui16Comp[hdrtoolslib::Cb_COMP];
  uint16_t* vOrg = m_ppcFrameOrg[0]->m_ui16Comp[hdrtoolslib::Cr_COMP];
  uint16_t* vRec = m_ppcFrameRec[0]->m_ui16Comp[hdrtoolslib::Cr_COMP];

  if (chFmt == ChromaFormat::_444)
  {
    yOrg = m_ppcFrameOrg[1]->m_ui16Comp[hdrtoolslib::Y_COMP];
    yRec = m_ppcFrameRec[1]->m_ui16Comp[hdrtoolslib::Y_COMP];
    uOrg = m_ppcFrameOrg[1]->m_ui16Comp[hdrtoolslib::Cb_COMP];
    uRec = m_ppcFrameRec[1]->m_ui16Comp[hdrtoolslib::Cb_COMP];
    vOrg = m_ppcFrameOrg[1]->m_ui16Comp[hdrtoolslib::Cr_COMP];
    vRec = m_ppcFrameRec[1]->m_ui16Comp[hdrtoolslib::Cr_COMP];
  }

  for (int i = 0; i < height; i++)
  {
    for (int j = 0; j < width; j++)
    {
      yOrg[i * width + j] = static_cast<uint16_t>(pOrg[(i + cropOffsetTop) * pcPic->getTrueOrigBuf(COMPONENT_Y).stride + j + cropOffsetLeft]);
      yRec[i*width + j] = static_cast<uint16_t>(pRec[(i + cropOffsetTop) * pcPic->getRecoBuf(COMPONENT_Y).stride + j + cropOffsetLeft]);
    }
  }

  if (chFmt != ChromaFormat::_444)
  {
    height >>= 1;
    width  >>= 1;
    cropOffsetLeft >>= 1;
    cropOffsetTop >>= 1;
  }

  pOrg = pcPic->getTrueOrigBuf(COMPONENT_Cb).buf;
  pRec = pcPic->getRecoBuf(COMPONENT_Cb).buf;

  for (int i = 0; i < height; i++)
  {
    for (int j = 0; j < width; j++)
    {
      uOrg[i * width + j] = static_cast<uint16_t>(pOrg[(i + cropOffsetTop) * pcPic->getTrueOrigBuf(COMPONENT_Cb).stride + j + cropOffsetLeft]);
      uRec[i*width + j] = static_cast<uint16_t>(pRec[(i + cropOffsetTop) * pcPic->getRecoBuf(COMPONENT_Cb).stride + j + cropOffsetLeft]);
    }
  }

  pOrg = pcPic->getTrueOrigBuf(COMPONENT_Cr).buf;
  pRec = pcPic->getRecoBuf(COMPONENT_Cr).buf;

  for (int i = 0; i < height; i++)
  {
    for (int j = 0; j < width; j++)
    {
      vOrg[i * width + j] = static_cast<uint16_t>(pOrg[(i + cropOffsetTop) * pcPic->getTrueOrigBuf(COMPONENT_Cr).stride + j + cropOffsetLeft]);
      vRec[i*width + j] = static_cast<uint16_t>(pRec[(i + cropOffsetTop) * pcPic->getRecoBuf(COMPONENT_Cr).stride + j + cropOffsetLeft]);
    }
  }
}
#endif

void EncGOP::xCalculateInterlacedAddPSNR( Picture* pcPicOrgFirstField, Picture* pcPicOrgSecondField,
                                          PelUnitBuf cPicRecFirstField, PelUnitBuf cPicRecSecondField,
                                          const InputColourSpaceConversion conversion, const bool printFrameMSE,
                                          const bool printMSSSIM, double* PSNR_Y, bool isEncodeLtRef)
{
  const SPS &sps = *pcPicOrgFirstField->cs->sps;
  const ChromaFormat format = sps.getChromaFormatIdc();
  double  dPSNR[MAX_NUM_COMPONENT];
  Picture    *apcPicOrgFields[2] = {pcPicOrgFirstField, pcPicOrgSecondField};
  PelUnitBuf acPicRecFields[2]   = {cPicRecFirstField, cPicRecSecondField};
#if ENABLE_QPA
  const bool    useWPSNR = m_pcEncLib->getUseWPSNR();
#endif
  for(int i=0; i<MAX_NUM_COMPONENT; i++)
  {
    dPSNR[i]=0.0;
  }

  PelStorage cscd[2 /* first/second field */];
  if (conversion!=IPCOLOURSPACE_UNCHANGED)
  {
    for(uint32_t fieldNum=0; fieldNum<2; fieldNum++)
    {
      PelUnitBuf& reconField= (acPicRecFields[fieldNum]);
      cscd[fieldNum].create( reconField.chromaFormat, Area( Position(), reconField.Y()) );
      VideoIOYuv::colourSpaceConvert(reconField, cscd[fieldNum], conversion, false);
      acPicRecFields[fieldNum]=cscd[fieldNum];
    }
  }

  //===== calculate PSNR =====
  double mseYuvFrame[MAX_NUM_COMPONENT] = { 0, 0, 0 };
  double msssim[MAX_NUM_COMPONENT] = {0.0};

  CHECK(!(acPicRecFields[0].chromaFormat==acPicRecFields[1].chromaFormat), "Unspecified error");
  const uint32_t numValidComponents = ::getNumberValidComponents( acPicRecFields[0].chromaFormat );

  for (int chan = 0; chan < numValidComponents; chan++)
  {
    const ComponentID ch=ComponentID(chan);
    CHECK(!(acPicRecFields[0].get(ch).width==acPicRecFields[1].get(ch).width), "Unspecified error");
    CHECK(!(acPicRecFields[0].get(ch).height==acPicRecFields[0].get(ch).height), "Unspecified error");

    uint64_t       ssdTemp = 0;
    const uint32_t width    = acPicRecFields[0].get(ch).width - (m_pcEncLib->getSourcePadding(0) >> ::getComponentScaleX(ch, format));
    const uint32_t height   = acPicRecFields[0].get(ch).height - ((m_pcEncLib->getSourcePadding(1) >> 1) >> ::getComponentScaleY(ch, format));
    const uint32_t bitDepth = sps.getBitDepth(toChannelType(ch));

    double sumOverFieldsMSSSIM = 0;
    for(uint32_t fieldNum=0; fieldNum<2; fieldNum++)
    {
      CHECK(!(conversion == IPCOLOURSPACE_UNCHANGED), "Unspecified error");
#if ENABLE_QPA
      ssdTemp += xFindDistortionPlane(acPicRecFields[fieldNum].get(ch), apcPicOrgFields[fieldNum]->getOrigBuf().get(ch),
                                      useWPSNR ? bitDepth : 0, ::getComponentScaleX(ch, format),
                                      ::getComponentScaleY(ch, format));
#else
      ssdTemp +=
        xFindDistortionPlane(acPicRecFields[fieldNum].get(ch), apcPicOrgFields[fieldNum]->getOrigBuf().get(ch), 0);
#endif
      if(printMSSSIM)
      {
        CPelBuf o = apcPicOrgFields[fieldNum]->getOrigBuf().get(ch);
        CPelBuf p = acPicRecFields[fieldNum].get(ch);
        sumOverFieldsMSSSIM += xCalculateMSSSIM(o.bufAt(0, 0), o.stride, p.bufAt(0, 0), p.stride, width, height, bitDepth);
      }
    }
    if (printMSSSIM)
    {
      msssim[ch] = sumOverFieldsMSSSIM / 2;
    }
    const uint32_t maxval = 255 << (bitDepth - 8);
    const uint32_t size   = width * height * 2;
    const double fRefValue = (double)maxval * maxval * size;
    dPSNR[ch]                = ssdTemp ? 10.0 * log10(fRefValue / (double) ssdTemp) : 999.99;
    mseYuvFrame[ch]          = (double) ssdTemp / size;
  }

  uint32_t uibits = 0; // the number of bits for the pair is not calculated here - instead the overall total is used elsewhere.

  //===== add PSNR =====
  m_gcAnalyzeAllField.addResult(dPSNR, (double) uibits, mseYuvFrame, mseYuvFrame, msssim, msssim, isEncodeLtRef);

  *PSNR_Y = dPSNR[COMPONENT_Y];

  msg( INFO, "\n                                      Interlaced frame %d: [Y %6.4lf dB    U %6.4lf dB    V %6.4lf dB]", pcPicOrgSecondField->getPOC()/2, dPSNR[COMPONENT_Y], dPSNR[COMPONENT_Cb], dPSNR[COMPONENT_Cr] );
  if (printMSSSIM)
  {
    printf(" [MS-SSIM Y %1.6lf    U %1.6lf    V %1.6lf]", msssim[COMPONENT_Y], msssim[COMPONENT_Cb], msssim[COMPONENT_Cr] );
  }
  if (printFrameMSE)
  {
    msg(DETAILS, " [Y MSE %6.4lf  U MSE %6.4lf  V MSE %6.4lf]", mseYuvFrame[COMPONENT_Y], mseYuvFrame[COMPONENT_Cb],
        mseYuvFrame[COMPONENT_Cr]);
  }

  for(uint32_t fieldNum=0; fieldNum<2; fieldNum++)
  {
    cscd[fieldNum].destroy();
  }
}

/** Function for deciding the nal_unit_type.
 * \param pocCurr POC of the current picture
 * \param lastIDR  POC of the last IDR picture
 * \param isField  true to indicate field coding
 * \returns the NAL unit type of the picture
 * This function checks the configuration and returns the appropriate nal_unit_type for the picture.
 */
NalUnitType EncGOP::getNalUnitType(int pocCurr, int lastIDR, bool isField)
{
#if GDR_ENABLED
  if (m_pcCfg->getGdrEnabled() && m_pcCfg->getDecodingRefreshType() == 3 && (pocCurr >= m_pcCfg->getGdrPocStart()))
  {
    int m = pocCurr - m_pcCfg->getGdrPocStart();
    int n = m_pcCfg->getGdrPeriod();
    if (m % n == 0)
    {
      return NAL_UNIT_CODED_SLICE_GDR;
    }
  }
#endif

  if (pocCurr == 0)
  {
    return NAL_UNIT_CODED_SLICE_IDR_N_LP;
  }

  if (m_pcCfg->getEfficientFieldIRAPEnabled() && isField && pocCurr == (m_pcCfg->getUseCompositeRef() ? 2: 1))
  {
    // to avoid the picture becoming an IRAP
    return NAL_UNIT_CODED_SLICE_TRAIL;
  }

  if (m_pcCfg->getDecodingRefreshType() != 3 && (pocCurr - isField) % (m_pcCfg->getIntraPeriod() * (m_pcCfg->getUseCompositeRef() ? 2 : 1)) == 0)
  {
    if (m_pcCfg->getDecodingRefreshType() == 1)
    {
      return NAL_UNIT_CODED_SLICE_CRA;
    }
    else if (m_pcCfg->getDecodingRefreshType() == 2)
    {
      SPS *sps = m_pcEncLib->getSPS(m_pcEncLib->getLayerId());
      if (sps != nullptr)
      {
        const int maxTLayer = sps->getMaxTLayers() - 1;
        return sps->getMaxNumReorderPics(maxTLayer) > 0 ? NAL_UNIT_CODED_SLICE_IDR_W_RADL : NAL_UNIT_CODED_SLICE_IDR_N_LP;
      }
      else
      {
        return NAL_UNIT_CODED_SLICE_IDR_W_RADL;
      }
    }
  }
  if(m_pocCRA>0)
  {
    if(pocCurr<m_pocCRA)
    {
      // All leading pictures are being marked as TFD pictures here since current encoder uses all
      // reference pictures while encoding leading pictures. An encoder can ensure that a leading
      // picture can be still decodable when random accessing to a CRA/CRANT/BLA/BLANT picture by
      // controlling the reference pictures used for encoding that leading picture. Such a leading
      // picture need not be marked as a TFD picture.
      return NAL_UNIT_CODED_SLICE_RASL;
    }
  }
  if (lastIDR>0)
  {
    if (pocCurr < lastIDR)
    {
      return NAL_UNIT_CODED_SLICE_RADL;
    }
  }
#if GDR_ENABLED
  if (m_pcCfg->getGdrEnabled() && pocCurr >= m_pcCfg->getGdrPocStart() && ((pocCurr - m_pcCfg->getGdrPocStart()) % m_pcCfg->getGdrPeriod() == 0))
  {
    return NAL_UNIT_CODED_SLICE_GDR;
  }
  else
  {
    return NAL_UNIT_CODED_SLICE_TRAIL;
  }
#else
  return NAL_UNIT_CODED_SLICE_TRAIL;
#endif
}

void EncGOP::xUpdateRasInit(Slice* slice)
{
  slice->setPendingRasInit( false );
  if ( slice->getPOC() > m_lastRasPoc )
  {
    m_lastRasPoc = MAX_INT;
    slice->setPendingRasInit( true );
  }
  if ( slice->isIRAP() )
  {
    m_lastRasPoc = slice->getPOC();
  }
}

void EncGOP::xUpdateRPRtmvp( PicHeader* pcPicHeader, Slice* pcSlice )
{
  if( pcPicHeader->getEnableTMVPFlag() )
  {
    int colRefIdxL0 = -1, colRefIdxL1 = -1;

    for( int refIdx = 0; refIdx < pcSlice->getNumRefIdx( REF_PIC_LIST_0 ); refIdx++ )
    {
      if( !( pcSlice->getRefPic( REF_PIC_LIST_0, refIdx )->slices[0]->getNalUnitType() != NAL_UNIT_CODED_SLICE_RASL &&
            pcSlice->getRefPic( REF_PIC_LIST_0, refIdx )->poc <= m_pocCRA ) )
      {
        colRefIdxL0 = refIdx;
        break;
      }
    }

    for( int refIdx = 0; refIdx < pcSlice->getNumRefIdx( REF_PIC_LIST_1 ); refIdx++ )
    {
      if( !( pcSlice->getRefPic( REF_PIC_LIST_1, refIdx )->slices[0]->getNalUnitType() != NAL_UNIT_CODED_SLICE_RASL &&
            pcSlice->getRefPic( REF_PIC_LIST_1, refIdx )->poc <= m_pocCRA ) )
      {
        colRefIdxL1 = refIdx;
        break;
      }
    }

    if( colRefIdxL0 >= 0 && colRefIdxL1 >= 0 )
    {
      const Picture *refPicL0 = pcSlice->getRefPic( REF_PIC_LIST_0, colRefIdxL0 );
      const Picture *refPicL1 = pcSlice->getRefPic( REF_PIC_LIST_1, colRefIdxL1 );

      CHECK( !refPicL0->slices.size(), "Wrong L0 reference picture" );
      CHECK( !refPicL1->slices.size(), "Wrong L1 reference picture" );

      const uint32_t colFromL0 = refPicL0->slices[0]->getSliceQp() > refPicL1->slices[0]->getSliceQp();
      pcPicHeader->setPicColFromL0Flag( colFromL0 );
      pcSlice->setColFromL0Flag(colFromL0);
      pcSlice->setColRefIdx( colFromL0 ? colRefIdxL0 : colRefIdxL1 );
      pcPicHeader->setColRefIdx( colFromL0 ? colRefIdxL0 : colRefIdxL1 );
    }
    else if( colRefIdxL0 < 0 && colRefIdxL1 >= 0 )
    {
      pcPicHeader->setPicColFromL0Flag( false );
      pcSlice->setColFromL0Flag( false );
      pcSlice->setColRefIdx( colRefIdxL1 );
      pcPicHeader->setColRefIdx( colRefIdxL1 );
    }
    else if( colRefIdxL0 >= 0 && colRefIdxL1 < 0 )
    {
      pcPicHeader->setPicColFromL0Flag( true );
      pcSlice->setColFromL0Flag( true );
      pcSlice->setColRefIdx( colRefIdxL0 );
      pcPicHeader->setColRefIdx( colRefIdxL0 );
    }
    else
    {
      pcPicHeader->setEnableTMVPFlag( false );
    }
  }
}

double EncGOP::xCalculateRVM()
{
  double dRVM = 0;

  if( m_pcCfg->getGOPSize() == 1 && m_pcCfg->getIntraPeriod() != 1 && m_pcCfg->getFramesToBeEncoded() > RVM_VCEGAM10_M * 2 )
  {
    // calculate RVM only for lowdelay configurations

    size_t n = m_rvm.size();

    std::vector<double> vRL(n);
    std::vector<double> vB(n);

    int i;
    double dRavg = 0 , dBavg = 0;
    vB[RVM_VCEGAM10_M] = 0;
    for (i = RVM_VCEGAM10_M + 1; i < n - RVM_VCEGAM10_M + 1; i++)
    {
      vRL[i] = 0;
      for( int j = i - RVM_VCEGAM10_M ; j <= i + RVM_VCEGAM10_M - 1 ; j++ )
      {
        vRL[i] += m_rvm[j];
      }
      vRL[i] /= ( 2 * RVM_VCEGAM10_M );
      vB[i] = vB[i - 1] + m_rvm[i] - vRL[i];
      dRavg += m_rvm[i];
      dBavg += vB[i];
    }

    dRavg /= (n - 2 * RVM_VCEGAM10_M);
    dBavg /= (n - 2 * RVM_VCEGAM10_M);

    double dSigamB = 0;
    for (i = RVM_VCEGAM10_M + 1; i < n - RVM_VCEGAM10_M + 1; i++)
    {
      double tmp = vB[i] - dBavg;
      dSigamB += tmp * tmp;
    }
    dSigamB = sqrt(dSigamB / (n - 2 * RVM_VCEGAM10_M));

    double f = sqrt( 12.0 * ( RVM_VCEGAM10_M - 1 ) / ( RVM_VCEGAM10_M + 1 ) );

    dRVM = dSigamB / dRavg * f;
  }

  return( dRVM );
}

/** Attaches the input bitstream to the stream in the output NAL unit
    Updates rNalu to contain concatenated bitstream. rpcBitstreamRedirect is cleared at the end of this function call.
 *  \param codedSliceData contains the coded slice data (bitstream) to be concatenated to rNalu
 *  \param rNalu          target NAL unit
 */
void EncGOP::xAttachSliceDataToNalUnit (OutputNALUnit& rNalu, OutputBitstream* codedSliceData)
{
  // Byte-align
  rNalu.m_bitstream.writeByteAlignment();   // Slice header byte-alignment

  // Perform bitstream concatenation
  if (codedSliceData->getNumberOfWrittenBits() > 0)
  {
    rNalu.m_bitstream.addSubstream(codedSliceData);
  }
  codedSliceData->clear();
}


void EncGOP::arrangeCompositeReference(Slice* pcSlice, PicList& rcListPic, int pocCurr)
{
  Picture             *curPic  = nullptr;
  PicList::iterator  iterPic = rcListPic.begin();
  const PreCalcValues *pcv = pcSlice->getPPS()->pcv;
  m_bgPOC = pocCurr + 1;
  if (m_picBg->getSpliceFull())
  {
    return;
  }
  while (iterPic != rcListPic.end())
  {
    curPic = *(iterPic++);
    if (curPic->getPOC() == pocCurr)
    {
      break;
    }
  }
  if (pcSlice->isIRAP())
  {
    return;
  }

  int width = pcv->lumaWidth;
  int height = pcv->lumaHeight;
  ptrdiff_t stride        = curPic->getOrigBuf().get(COMPONENT_Y).stride;
  ptrdiff_t cStride       = curPic->getOrigBuf().get(COMPONENT_Cb).stride;
  Pel* curLumaAddr = curPic->getOrigBuf().get(COMPONENT_Y).buf;
  Pel* curCbAddr = curPic->getOrigBuf().get(COMPONENT_Cb).buf;
  Pel* curCrAddr = curPic->getOrigBuf().get(COMPONENT_Cr).buf;
  Pel* bgOrgLumaAddr = m_picOrig->getOrigBuf().get(COMPONENT_Y).buf;
  Pel* bgOrgCbAddr = m_picOrig->getOrigBuf().get(COMPONENT_Cb).buf;
  Pel* bgOrgCrAddr = m_picOrig->getOrigBuf().get(COMPONENT_Cr).buf;
  int cuMaxWidth = pcv->maxCUWidth;
  int cuMaxHeight = pcv->maxCUHeight;
  int maxReplace = (pcv->sizeInCtus) / 2;
  maxReplace = maxReplace < 1 ? 1 : maxReplace;
  struct CostStr
  {
    double cost;
    int ctuIdx;
  };
  CostStr* minCtuCost = new CostStr[maxReplace];
  for (int i = 0; i < maxReplace; i++)
  {
    minCtuCost[i].cost = 1e10;
    minCtuCost[i].ctuIdx = -1;
  }
  int bitIncrementY  = pcSlice->getSPS()->getBitDepth(ChannelType::LUMA) - 8;
  int bitIncrementUV = pcSlice->getSPS()->getBitDepth(ChannelType::CHROMA) - 8;
  for (int y = 0; y < height; y += cuMaxHeight)
  {
    for (int x = 0; x < width; x += cuMaxWidth)
    {
      double lcuDist = 0.0;
      double lcuDistCb = 0.0;
      double lcuDistCr = 0.0;
      int    realPixelCnt = 0;
      double lcuCost = 1e10;
      int largeDist = 0;

      for (int tmpy = 0; tmpy < cuMaxHeight; tmpy++)
      {
        if (y + tmpy >= height)
        {
          break;
        }
        for (int tmpx = 0; tmpx < cuMaxWidth; tmpx++)
        {
          if (x + tmpx >= width)
          {
            break;
          }

          realPixelCnt++;
          lcuDist += abs(curLumaAddr[(y + tmpy)*stride + x + tmpx] - bgOrgLumaAddr[(y + tmpy)*stride + x + tmpx]);
          if (abs(curLumaAddr[(y + tmpy)*stride + x + tmpx] - bgOrgLumaAddr[(y + tmpy)*stride + x + tmpx]) >(20 << bitIncrementY))
          {
            largeDist++;
          }

          if (tmpy % 2 == 0 && tmpx % 2 == 0)
          {
            lcuDistCb += abs(curCbAddr[(y + tmpy) / 2 * cStride + (x + tmpx) / 2] - bgOrgCbAddr[(y + tmpy) / 2 * cStride + (x + tmpx) / 2]);
            lcuDistCr += abs(curCrAddr[(y + tmpy) / 2 * cStride + (x + tmpx) / 2] - bgOrgCrAddr[(y + tmpy) / 2 * cStride + (x + tmpx) / 2]);
          }
        }
      }

      //Test the vertical or horizontal edge for background patches candidates
      int yInLCU = y / cuMaxHeight;
      int xInLCU = x / cuMaxWidth;
      int iLCUIdx = yInLCU * pcv->widthInCtus + xInLCU;
      if ((largeDist / (double)realPixelCnt < 0.01 &&lcuDist / realPixelCnt < (3.5 * (1 << bitIncrementY)) && lcuDistCb / realPixelCnt < (0.5 * (1 << bitIncrementUV)) && lcuDistCr / realPixelCnt < (0.5 * (1 << bitIncrementUV)) && m_picBg->getSpliceIdx(iLCUIdx) == 0))
      {
        lcuCost = lcuDist / realPixelCnt + lcuDistCb / realPixelCnt + lcuDistCr / realPixelCnt;
        //obtain the maxReplace smallest cost
        //1) find the largest cost in the maxReplace candidates
        for (int i = 0; i < maxReplace - 1; i++)
        {
          if (minCtuCost[i].cost > minCtuCost[i + 1].cost)
          {
            std::swap(minCtuCost[i].cost, minCtuCost[i + 1].cost);
            std::swap(minCtuCost[i].ctuIdx, minCtuCost[i + 1].ctuIdx);
          }
        }
        // 2) compare the current cost with the largest cost
        if (lcuCost < minCtuCost[maxReplace - 1].cost)
        {
          minCtuCost[maxReplace - 1].cost = lcuCost;
          minCtuCost[maxReplace - 1].ctuIdx = iLCUIdx;
        }
      }
    }
  }

  // modify QP for background CTU
  for (int i = 0; i < maxReplace; i++)
  {
    if (minCtuCost[i].ctuIdx != -1)
    {
      m_picBg->setSpliceIdx(minCtuCost[i].ctuIdx, pocCurr);
    }
  }

  delete[]minCtuCost;
}

void EncGOP::updateCompositeReference(Slice* pcSlice, PicList& rcListPic, int pocCurr)
{
  Picture             *curPic  = nullptr;
  const PreCalcValues *pcv = pcSlice->getPPS()->pcv;
  PicList::iterator  iterPic = rcListPic.begin();
  iterPic = rcListPic.begin();
  while (iterPic != rcListPic.end())
  {
    curPic = *(iterPic++);
    if (curPic->getPOC() == pocCurr)
    {
      break;
    }
  }
  assert(curPic->getPOC() == pocCurr);

  int width = pcv->lumaWidth;
  int height = pcv->lumaHeight;
  ptrdiff_t stride  = curPic->getRecoBuf().get(COMPONENT_Y).stride;
  ptrdiff_t cStride = curPic->getRecoBuf().get(COMPONENT_Cb).stride;

  Pel* bgLumaAddr = m_picBg->getRecoBuf().get(COMPONENT_Y).buf;
  Pel* bgCbAddr = m_picBg->getRecoBuf().get(COMPONENT_Cb).buf;
  Pel* bgCrAddr = m_picBg->getRecoBuf().get(COMPONENT_Cr).buf;
  Pel* curLumaAddr = curPic->getRecoBuf().get(COMPONENT_Y).buf;
  Pel* curCbAddr = curPic->getRecoBuf().get(COMPONENT_Cb).buf;
  Pel* curCrAddr = curPic->getRecoBuf().get(COMPONENT_Cr).buf;

  int maxCuWidth = pcv->maxCUWidth;
  int maxCuHeight = pcv->maxCUHeight;

  // Update background reference
  if (pcSlice->isIRAP())//(pocCurr == 0)
  {
    curPic->extendPicBorder( pcSlice->getSPS(), pcSlice->getPPS() );
    curPic->setBorderExtension(true);

    m_picBg->getRecoBuf().copyFrom(curPic->getRecoBuf());
    m_picOrig->getOrigBuf().copyFrom(curPic->getOrigBuf());
  }
  else
  {
    //cout << "update B" << pocCurr << endl;
    for (int y = 0; y < height; y += maxCuHeight)
    {
      for (int x = 0; x < width; x += maxCuWidth)
      {
        if (m_picBg->getSpliceIdx((y / maxCuHeight)*pcv->widthInCtus + x / maxCuWidth) == pocCurr)
        {
          for (int tmpy = 0; tmpy < maxCuHeight; tmpy++)
          {
            if (y + tmpy >= height)
            {
              break;
            }
            for (int tmpx = 0; tmpx < maxCuWidth; tmpx++)
            {
              if (x + tmpx >= width)
              {
                break;
              }
              bgLumaAddr[(y + tmpy)*stride + x + tmpx] = curLumaAddr[(y + tmpy)*stride + x + tmpx];
              if (tmpy % 2 == 0 && tmpx % 2 == 0)
              {
                bgCbAddr[(y + tmpy) / 2 * cStride + (x + tmpx) / 2] = curCbAddr[(y + tmpy) / 2 * cStride + (x + tmpx) / 2];
                bgCrAddr[(y + tmpy) / 2 * cStride + (x + tmpx) / 2] = curCrAddr[(y + tmpy) / 2 * cStride + (x + tmpx) / 2];
              }
            }
          }
        }
      }
    }
    m_picBg->setBorderExtension(false);
    m_picBg->extendPicBorder(pcSlice->getSPS(), pcSlice->getPPS());
    m_picBg->setBorderExtension(true);

    curPic->extendPicBorder(pcSlice->getSPS(), pcSlice->getPPS());
    curPic->setBorderExtension(true);
    m_picOrig->getOrigBuf().copyFrom(curPic->getOrigBuf());

    m_picBg->setBorderExtension(false);
    m_picBg->extendPicBorder(pcSlice->getSPS(), pcSlice->getPPS());
    m_picBg->setBorderExtension(true);
  }
}

void EncGOP::applyDeblockingFilterMetric(Picture *pic)
{
  CPelBuf pelBuf = pic->getRecoBuf().get(COMPONENT_Y);

  const Pel      *rec       = pelBuf.buf;
  const ptrdiff_t stride    = pelBuf.stride;
  const uint32_t  picWidth  = pelBuf.width;
  const uint32_t  picHeight = pelBuf.height;

  const Pel   *tempRec    = rec;
  const Slice *firstSlice = pic->slices.front();

  const uint32_t log2maxTB       = firstSlice->getSPS()->getLog2MaxTbSize();
  const uint32_t maxTBsize = (1<<log2maxTB);
  const uint32_t minBlockArtSize = 8;
  const uint32_t noCol = (picWidth>>log2maxTB);
  const uint32_t noRows = (picHeight>>log2maxTB);
  CHECK(!(noCol > 1), "Unspecified error");
  CHECK(!(noRows > 1), "Unspecified error");
  std::vector<uint64_t> colSAD(noCol,  uint64_t(0));
  std::vector<uint64_t> rowSAD(noRows, uint64_t(0));
  uint32_t colIdx = 0;
  uint32_t rowIdx = 0;
  Pel p0, p1, p2, q0, q1, q2;

  const int qp            = firstSlice->getSliceQp();
  const int bitDepthLuma  = firstSlice->getSPS()->getBitDepth(ChannelType::LUMA);
  const int bitdepthScale = 1 << (bitDepthLuma - 8);
  const int beta          = DeblockingFilter::getBeta(qp) * bitdepthScale;
  const int thr2 = (beta>>2);
  const int thr1 = 2*bitdepthScale;
  uint32_t a = 0;

  if (maxTBsize > minBlockArtSize)
  {
    // Analyze vertical artifact edges
    for(int c = maxTBsize; c < picWidth; c += maxTBsize)
    {
      for(int r = 0; r < picHeight; r++)
      {
        p2 = rec[c - 3];
        p1 = rec[c - 2];
        p0 = rec[c - 1];
        q0 = rec[c];
        q1 = rec[c + 1];
        q2 = rec[c + 2];
        a = ((abs(p2-(p1<<1)+p0)+abs(q0-(q1<<1)+q2))<<1);
        if ( thr1 < a && a < thr2)
        {
          colSAD[colIdx] += abs(p0 - q0);
        }
        rec += stride;
      }
      colIdx++;
      rec = tempRec;
    }

    // Analyze horizontal artifact edges
    for(int r = maxTBsize; r < picHeight; r += maxTBsize)
    {
      for(int c = 0; c < picWidth; c++)
      {
        p2 = rec[c + (r - 3) * stride];
        p1 = rec[c + (r - 2) * stride];
        p0 = rec[c + (r - 1) * stride];
        q0 = rec[c + r * stride];
        q1 = rec[c + (r + 1) * stride];
        q2 = rec[c + (r + 2) * stride];
        a = ((abs(p2-(p1<<1)+p0)+abs(q0-(q1<<1)+q2))<<1);
        if (thr1 < a && a < thr2)
        {
          rowSAD[rowIdx] += abs(p0 - q0);
        }
      }
      rowIdx++;
    }
  }

  uint64_t colSADsum = 0;
  uint64_t rowSADsum = 0;
  for(int c = 0; c < noCol-1; c++)
  {
    colSADsum += colSAD[c];
  }
  for(int r = 0; r < noRows-1; r++)
  {
    rowSADsum += rowSAD[r];
  }

  colSADsum <<= 10;
  rowSADsum <<= 10;
  colSADsum /= (noCol-1);
  colSADsum /= picHeight;
  rowSADsum /= (noRows-1);
  rowSADsum /= picWidth;

  uint64_t avgSAD = ((colSADsum + rowSADsum)>>1);
  avgSAD >>= (bitDepthLuma-8);

  if ( avgSAD > 2048 )
  {
    avgSAD >>= 9;
    int offset = Clip3(2,6,(int)avgSAD);
    for (Slice *slice: pic->slices)
    {
      slice->setDeblockingFilterOverrideFlag(true);
      slice->setDeblockingFilterDisable(false);
      slice->setDeblockingFilterBetaOffsetDiv2(offset);
      slice->setDeblockingFilterTcOffsetDiv2(offset);
      slice->setDeblockingFilterCbBetaOffsetDiv2(offset);
      slice->setDeblockingFilterCbTcOffsetDiv2(offset);
      slice->setDeblockingFilterCrBetaOffsetDiv2(offset);
      slice->setDeblockingFilterCrTcOffsetDiv2(offset);
    }
  }
  else
  {
    const PPS *pps = firstSlice->getPPS();

    for (Slice *slice: pic->slices)
    {
      slice->setDeblockingFilterOverrideFlag(false);
      slice->setDeblockingFilterDisable(pps->getPPSDeblockingFilterDisabledFlag());
      slice->setDeblockingFilterBetaOffsetDiv2(pps->getDeblockingFilterBetaOffsetDiv2());
      slice->setDeblockingFilterTcOffsetDiv2(pps->getDeblockingFilterTcOffsetDiv2());
      slice->setDeblockingFilterCbBetaOffsetDiv2(pps->getDeblockingFilterCbBetaOffsetDiv2());
      slice->setDeblockingFilterCbTcOffsetDiv2(pps->getDeblockingFilterCbTcOffsetDiv2());
      slice->setDeblockingFilterCrBetaOffsetDiv2(pps->getDeblockingFilterCrBetaOffsetDiv2());
      slice->setDeblockingFilterCrTcOffsetDiv2(pps->getDeblockingFilterCrTcOffsetDiv2());
    }
  }
}

void EncGOP::applyDeblockingFilterParameterSelection( Picture* pcPic, const uint32_t numSlices, const int gopID )
{
  constexpr int MAX_BETA_OFFSET = 3;
  constexpr int MIN_BETA_OFFSET = -3;
  constexpr int MAX_TC_OFFSET   = 3;
  constexpr int MIN_TC_OFFSET   = -3;

  PelUnitBuf reco = pcPic->getRecoBuf();

  const int currQualityLayer = !pcPic->slices[0]->isIRAP() ? m_pcCfg->getGOPEntry(gopID).m_temporalId + 1 : 0;
  CHECK(currQualityLayer >= MAX_ENCODER_DEBLOCKING_QUALITY_LAYERS, "currQualityLayer is too large");

  CodingStructure& cs = *pcPic->cs;

  if (!m_pcDeblockingTempPicYuv)
  {
    m_pcDeblockingTempPicYuv = new PelStorage;
    m_pcDeblockingTempPicYuv->create( cs.area );

    for (auto &p: m_deblockParam)
    {
      p.available = false;
    }
  }

  //preserve current reconstruction
  m_pcDeblockingTempPicYuv->copyFrom ( reco );

  auto &deblockParam = m_deblockParam[currQualityLayer];

  const bool hasBetaTc = deblockParam.available && !deblockParam.disabled;

  const int maxBetaOffsetDiv2 =
    hasBetaTc ? Clip3(MIN_BETA_OFFSET, MAX_BETA_OFFSET, deblockParam.betaOffsetDiv2 + 1) : MAX_BETA_OFFSET;
  const int minBetaOffsetDiv2 =
    hasBetaTc ? Clip3(MIN_BETA_OFFSET, MAX_BETA_OFFSET, deblockParam.betaOffsetDiv2 - 1) : MIN_BETA_OFFSET;

  const int maxTcOffsetDiv2 =
    hasBetaTc ? Clip3(MIN_TC_OFFSET, MAX_TC_OFFSET, deblockParam.tcOffsetDiv2 + 2) : MAX_TC_OFFSET;
  const int minTcOffsetDiv2 =
    hasBetaTc ? Clip3(MIN_TC_OFFSET, MAX_TC_OFFSET, deblockParam.tcOffsetDiv2 - 2) : MIN_TC_OFFSET;

  uint64_t distBetaPrevious      = std::numeric_limits<uint64_t>::max();
  uint64_t distMin               = std::numeric_limits<uint64_t>::max();

  bool dbFilterDisabledBest = true;
  int  betaOffsetDiv2Best   = 0;
  int  tcOffsetDiv2Best     = 0;

  for (int betaOffsetDiv2 = maxBetaOffsetDiv2; betaOffsetDiv2 >= minBetaOffsetDiv2; betaOffsetDiv2--)
  {
    uint64_t distTcMin = std::numeric_limits<uint64_t>::max();

    for (int tcOffsetDiv2 = maxTcOffsetDiv2; tcOffsetDiv2 >= minTcOffsetDiv2; tcOffsetDiv2--)
    {
      for (int i = 0; i < numSlices; i++)
      {
        Slice *slice = pcPic->slices[i];

        slice->setDeblockingFilterOverrideFlag(true);
        slice->setDeblockingFilterDisable(false);
        slice->setDeblockingFilterBetaOffsetDiv2(betaOffsetDiv2);
        slice->setDeblockingFilterTcOffsetDiv2(tcOffsetDiv2);
        slice->setDeblockingFilterCbBetaOffsetDiv2(betaOffsetDiv2);
        slice->setDeblockingFilterCbTcOffsetDiv2(tcOffsetDiv2);
        slice->setDeblockingFilterCrBetaOffsetDiv2(betaOffsetDiv2);
        slice->setDeblockingFilterCrTcOffsetDiv2(tcOffsetDiv2);
      }

      // restore reconstruction
      reco.copyFrom( *m_pcDeblockingTempPicYuv );

      const uint64_t dist = preLoopFilterPicAndCalcDist( pcPic );

      if (dist < distMin)
      {
        distMin              = dist;
        dbFilterDisabledBest = false;
        betaOffsetDiv2Best   = betaOffsetDiv2;
        tcOffsetDiv2Best     = tcOffsetDiv2;
      }

      if (dist < distTcMin)
      {
        distTcMin = dist;
      }
      else if (tcOffsetDiv2 < -2)
      {
        break;
      }
    }

    if (betaOffsetDiv2 < -1 && distTcMin >= distBetaPrevious)
    {
      break;
    }
    distBetaPrevious = distTcMin;
  }

  // update
  deblockParam.available      = true;
  deblockParam.disabled       = dbFilterDisabledBest;
  deblockParam.betaOffsetDiv2 = betaOffsetDiv2Best;
  deblockParam.tcOffsetDiv2   = tcOffsetDiv2Best;

  // restore reconstruction
  reco.copyFrom( *m_pcDeblockingTempPicYuv );

  const PPS *pps = pcPic->slices.front()->getPPS();
  if (dbFilterDisabledBest)
  {
    for (int i = 0; i < numSlices; i++)
    {
      Slice *slice = pcPic->slices[i];

      slice->setDeblockingFilterOverrideFlag(!pps->getPPSDeblockingFilterDisabledFlag());
      slice->setDeblockingFilterDisable(true);
    }
  }
  else if (!pps->getPPSDeblockingFilterDisabledFlag() && betaOffsetDiv2Best == pps->getDeblockingFilterBetaOffsetDiv2()
           && tcOffsetDiv2Best == pps->getDeblockingFilterTcOffsetDiv2())
  {
    for (int i = 0; i < numSlices; i++)
    {
      Slice *slice = pcPic->slices[i];

      slice->setDeblockingFilterOverrideFlag(false);
      slice->setDeblockingFilterDisable(false);
      slice->setDeblockingFilterBetaOffsetDiv2(pps->getDeblockingFilterBetaOffsetDiv2());
      slice->setDeblockingFilterTcOffsetDiv2(pps->getDeblockingFilterTcOffsetDiv2());
      slice->setDeblockingFilterCbBetaOffsetDiv2(pps->getDeblockingFilterBetaOffsetDiv2());
      slice->setDeblockingFilterCbTcOffsetDiv2(pps->getDeblockingFilterTcOffsetDiv2());
      slice->setDeblockingFilterCrBetaOffsetDiv2(pps->getDeblockingFilterBetaOffsetDiv2());
      slice->setDeblockingFilterCrTcOffsetDiv2(pps->getDeblockingFilterTcOffsetDiv2());
    }
  }
  else
  {
    for (int i = 0; i < numSlices; i++)
    {
      Slice *slice = pcPic->slices[i];

      slice->setDeblockingFilterOverrideFlag(true);
      slice->setDeblockingFilterDisable(false);
      slice->setDeblockingFilterBetaOffsetDiv2(betaOffsetDiv2Best);
      slice->setDeblockingFilterTcOffsetDiv2(tcOffsetDiv2Best);
      slice->setDeblockingFilterCbBetaOffsetDiv2(betaOffsetDiv2Best);
      slice->setDeblockingFilterCbTcOffsetDiv2(tcOffsetDiv2Best);
      slice->setDeblockingFilterCrBetaOffsetDiv2(betaOffsetDiv2Best);
      slice->setDeblockingFilterCrTcOffsetDiv2(tcOffsetDiv2Best);
    }
  }
}

bool EncGOP::xCheckMaxTidILRefPics(int layerIdx, Picture* refPic, bool currentPicIsIRAP)
{
  const VPS* vps = refPic->cs->vps;
  const int  refLayerIdx          = vps == nullptr ? 0 : vps->getGeneralLayerIdx(refPic->layerId);
  const int maxTidILRefPicsPlus1 = vps->getMaxTidIlRefPicsPlus1(layerIdx, refLayerIdx);

  // -1 means not set
  if (maxTidILRefPicsPlus1 < 0)
  {
    return true;
  }

  // 0 allows only IRAP pictures to use inter-layer prediction
  if (maxTidILRefPicsPlus1 == 0)
  {
    return currentPicIsIRAP;
  }

  // all other cases filter by temporalID
  return ( refPic->temporalId < maxTidILRefPicsPlus1 );
}

void EncGOP::xCreateExplicitReferencePictureSetFromReference( Slice* slice, PicList& rcListPic, const ReferencePictureList *rpl0, const ReferencePictureList *rpl1 )
{
  const int pocCycle = 1 << slice->getSPS()->getBitsForPOC();

  const bool interLayerPresent = slice->getSPS()->getInterLayerPresentFlag();

  Picture   *curPic   = slice->getPic();
  const VPS *vps      = curPic->cs->vps;
  int        layerIdx = vps->getGeneralLayerIdx(curPic->layerId);

  const bool isIntraLayerPredAllowed =
    (vps->getIndependentLayerFlag(layerIdx) || vps->getPredDirection(slice->getTLayer()) != 1)
    && (!slice->isIRAP() || (m_pcEncLib->getAvoidIntraInDepLayer() && layerIdx != 0));
  const bool isInterLayerPredAllowed =
    !vps->getIndependentLayerFlag(layerIdx) && vps->getPredDirection(slice->getTLayer()) != 2;

  ReferencePictureList localRpl[NUM_REF_PIC_LIST_01] = { ReferencePictureList(interLayerPresent),
                                                         ReferencePictureList(interLayerPresent) };

  uint32_t numStrp[NUM_REF_PIC_LIST_01] = { 0, 0 };
  uint32_t numLtrp[NUM_REF_PIC_LIST_01] = { 0, 0 };
  uint32_t numIlrp[NUM_REF_PIC_LIST_01] = { 0, 0 };
  uint32_t num[NUM_REF_PIC_LIST_01]     = { 0, 0 };

  static_vector<int, MAX_NUM_REF_PICS> higherTLayerRefs[NUM_REF_PIC_LIST_01];
  static_vector<int, MAX_NUM_REF_PICS> inactiveRefs[NUM_REF_PIC_LIST_01];

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const ReferencePictureList *rpl = l == REF_PIC_LIST_0 ? rpl0 : rpl1;

    if (isIntraLayerPredAllowed)
    {
      for (int ii = 0; ii < rpl->getNumRefEntries(); ii++)
      {
        if (!rpl->isInterLayerRefPic(ii))
        {
          for (const auto &pic: rcListPic)
          {
            if (pic->layerId == curPic->layerId && pic->referenced
                && !slice->isPocRestrictedByDRAP(pic->getPOC(), pic->precedingDRAP)
                && !slice->isPocRestrictedByEdrap(pic->getPOC()))
            {
              const bool isAvailable = !rpl->isRefPicLongterm(ii)
                                         ? pic->getPOC() == slice->getPOC() + rpl->getRefPicIdentifier(ii)
                                         : (pic->getPOC() & (pocCycle - 1)) == rpl->getRefPicIdentifier(ii);
              if (isAvailable)
              {
                if (slice->isIRAP())
                {
                  inactiveRefs[l].push_back(ii);
                }
                else if (pic->temporalId > curPic->temporalId)
                {
                  higherTLayerRefs[l].push_back(ii);
                }
                else if (num[l] >= rpl->getNumberOfActivePictures() - rpl->getNumberOfInterLayerPictures()
                         && layerIdx != 0 && vps != nullptr && !vps->getAllIndependentLayersFlag()
                         && isInterLayerPredAllowed && !m_pcEncLib->getExplicitILRP())
                {
                  inactiveRefs[l].push_back(ii);
                }

                else
                {
                  localRpl[l].setRefPicIdentifier(num[l], rpl->getRefPicIdentifier(ii), rpl->isRefPicLongterm(ii),
                                                  false, NOT_VALID);
                  num[l]++;
                  numStrp[l] += rpl->isRefPicLongterm(ii) ? 0 : 1;
                  numLtrp[l] += rpl->isRefPicLongterm(ii) && !rpl->isInterLayerRefPic(ii) ? 1 : 0;
                }
                break;
              }
            }
          }
        }
        else if (m_pcEncLib->getExplicitILRP() && layerIdx != 0 && vps != nullptr && !vps->getAllIndependentLayersFlag() && isInterLayerPredAllowed)
        {
          for (const auto &pic: rcListPic)
          {
            int refLayerIdx = vps->getGeneralLayerIdx(pic->layerId);
            if (refLayerIdx == vps->getDirectRefLayerIdx(layerIdx, rpl->getInterLayerRefPicIdx(ii)) && pic->referenced && pic->getPOC() == curPic->getPOC()
                && vps->getDirectRefLayerFlag(layerIdx, refLayerIdx) && xCheckMaxTidILRefPics(layerIdx, pic, slice->isIRAP()))
            {
              localRpl[l].setRefPicIdentifier(num[l], 0, true, true, vps->getInterLayerRefIdc(layerIdx, refLayerIdx));
              num[l]++;
              numIlrp[l]++;
            }
          }
        }
      }
    }

    // AvoidIntraInDepLayer IRAPs in dependent layers are replaced with inter slices but may have empty L0 list (e.g. if no IL ref specified in config)
    // In this case implicitly add an inter-layer ref to avoid inter slices with no ref
    if (m_pcEncLib->getExplicitILRP())
    {
      if(l==REF_PIC_LIST_0 && num[l]==0 && (slice->isIRAP() && m_pcEncLib->getAvoidIntraInDepLayer())
        && m_pcEncLib->getNumRefLayers(layerIdx) && !vps->getAllIndependentLayersFlag())
      {
        for (const auto &pic: rcListPic)
        {
          int refLayerIdx = vps->getGeneralLayerIdx(pic->layerId);
          if (pic->referenced && pic->getPOC() == curPic->getPOC() && vps->getDirectRefLayerFlag(layerIdx, refLayerIdx)
              && xCheckMaxTidILRefPics(layerIdx, pic, slice->isIRAP()))
          {
            localRpl[l].setRefPicIdentifier(num[l], 0, true, true, vps->getInterLayerRefIdc(layerIdx, refLayerIdx));
            num[l]++;
            numIlrp[l]++;
            msg(WARNING, "WARNING: inter slice at POC %d and LId %d has an empty L0 list => Automatically adding inter-layer reference from LId %d\n", slice->getPOC(), curPic->layerId, pic->layerId);
            break;//only add 1 Inter-layer ref pic automatically.
          }
        }
      }
    }
    else
    {
      // inter-layer reference pictures are added to the end of the reference picture list
      if (layerIdx != 0 && vps != nullptr && !vps->getAllIndependentLayersFlag() && isInterLayerPredAllowed)
      {
        for (const auto &pic: rcListPic)
        {
          int refLayerIdx = vps->getGeneralLayerIdx(pic->layerId);
          if (pic->referenced && pic->getPOC() == curPic->getPOC() && vps->getDirectRefLayerFlag(layerIdx, refLayerIdx)
              && xCheckMaxTidILRefPics(layerIdx, pic, slice->isIRAP()))
          {
            localRpl[l].setRefPicIdentifier(num[l], 0, true, true, vps->getInterLayerRefIdc(layerIdx, refLayerIdx));
            num[l]++;
            numIlrp[l]++;
          }
        }
      }
    }
  }

  uint32_t numPrev[NUM_REF_PIC_LIST_01] = { num[REF_PIC_LIST_0], num[REF_PIC_LIST_1] };

  // Copy from other list if we have fewer than active ref pics

  bool isDisallowMixedRefPic = slice->getSPS()->getAllActiveRplEntriesHasSameSignFlag();

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const ReferencePictureList* rpl = l == REF_PIC_LIST_0 ? rpl0 : rpl1;
    const auto                  k   = l == REF_PIC_LIST_0 ? REF_PIC_LIST_1 : REF_PIC_LIST_0;

    int numOfNeedToFill = rpl->getNumberOfActivePictures() - num[l];

    for (int ii = 0; numOfNeedToFill > 0 && ii < numPrev[k]; ii++)
    {
      const int  identifier   = localRpl[k].getRefPicIdentifier(ii);
      const bool isLongTerm   = localRpl[k].isRefPicLongterm(ii);
      const bool isInterLayer = localRpl[k].isInterLayerRefPic(ii);
      if (m_pcEncLib->getExplicitILRP() && isInterLayer)
      {
        continue;//do not implicitly add inter layer refs from other list if explicitILRP is enabled.
      }

      // Make sure this copy is not already present
      bool canIncludeThis = true;
      for (int jj = 0; jj < num[l]; jj++)
      {
        if (identifier == localRpl[l].getRefPicIdentifier(jj) && isLongTerm == localRpl[l].isRefPicLongterm(jj)
            && isInterLayer == localRpl[l].isInterLayerRefPic(jj))
        {
          canIncludeThis = false;
          break;
        }

        if (isDisallowMixedRefPic && !isLongTerm && !localRpl[l].isRefPicLongterm(jj))
        {
          const bool sameSign = (identifier ^ localRpl[l].getRefPicIdentifier(jj)) >= 0;
          if (!sameSign)
          {
            canIncludeThis = false;
            break;
          }
        }
      }
      if (canIncludeThis)
      {
        localRpl[l].setRefPicIdentifier(num[l], identifier, isLongTerm, isInterLayer,
                                        localRpl[k].getInterLayerRefPicIdx(ii));
        num[l]++;
        numStrp[l] += isLongTerm ? 0 : 1;
        numLtrp[l] += isLongTerm && !isInterLayer ? 1 : 0;
        numIlrp[l] += isInterLayer ? 1 : 0;
        numOfNeedToFill--;
      }
    }
  }

  const uint32_t numValidRefs[NUM_REF_PIC_LIST_01] = { num[REF_PIC_LIST_0], num[REF_PIC_LIST_1] };

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const ReferencePictureList* rpl = l == REF_PIC_LIST_0 ? rpl0 : rpl1;

    // now add inactive refs
    for (const int i: inactiveRefs[l])
    {
      localRpl[l].setRefPicIdentifier(num[l], rpl->getRefPicIdentifier(i), rpl->isRefPicLongterm(i), false, NOT_VALID);
      num[l]++;
      numStrp[l] += rpl->isRefPicLongterm(i) ? 0 : 1;
      numLtrp[l] += rpl->isRefPicLongterm(i) && !rpl->isInterLayerRefPic(i) ? 1 : 0;
    }

    if (slice->getEnableDRAPSEI() && l == REF_PIC_LIST_0)
    {
      localRpl[l].setNumberOfShorttermPictures(numStrp[l]);
      localRpl[l].setNumberOfLongtermPictures(numLtrp[l]);
      localRpl[l].setNumberOfInterLayerPictures(numIlrp[l]);

      if (!slice->isIRAP() && !slice->isPOCInRefPicList(&localRpl[l], slice->getAssociatedIRAPPOC()))
      {
        if (slice->getUseLTforDRAP() && !slice->isPOCInRefPicList(rpl1, slice->getAssociatedIRAPPOC()))
        {
          // Adding associated IRAP as longterm picture
          localRpl[l].setRefPicIdentifier(num[l], slice->getAssociatedIRAPPOC(), true, false, 0);
          num[l]++;
          numLtrp[l]++;
        }
        else
        {
          // Adding associated IRAP as shortterm picture
          localRpl[l].setRefPicIdentifier(num[l], slice->getAssociatedIRAPPOC() - slice->getPOC(), false, false, 0);
          num[l]++;
          numStrp[l]++;
        }
      }
    }
    if (slice->getEnableEdrapSEI() && l == REF_PIC_LIST_0)
    {
      localRpl[l].setNumberOfShorttermPictures(numStrp[l]);
      localRpl[l].setNumberOfLongtermPictures(numLtrp[l]);
      localRpl[l].setNumberOfInterLayerPictures(numIlrp[l]);

      for (int i = 0; i < slice->getEdrapNumRefRapPics(); i++)
      {
        int refPoc = slice->getEdrapRefRapId(i) == 0 ? slice->getAssociatedIRAPPOC()
                                                     : slice->getEdrapRefRapId(i) * m_pcEncLib->getEdrapPeriod();
        if (slice->isPOCInRefPicList(&localRpl[l], refPoc))
        {
          continue;
        }
        if (slice->getUseLTforEdrap() && !slice->isPOCInRefPicList(rpl1, refPoc))
        {
          // Added as longterm picture
          localRpl[l].setRefPicIdentifier(num[l], refPoc, true, false, 0);
          num[l]++;
          numLtrp[l]++;
        }
        else
        {
          // Added as shortterm picture
          localRpl[l].setRefPicIdentifier(num[l], refPoc - slice->getPOC(), false, false, 0);
          num[l]++;
          numStrp[l]++;
        }
      }
    }

    // now add higher TId refs
    for (const int i: higherTLayerRefs[l])
    {
      localRpl[l].setRefPicIdentifier(num[l], rpl->getRefPicIdentifier(i), rpl->isRefPicLongterm(i), false, NOT_VALID);
      num[l]++;
      numStrp[l] += rpl->isRefPicLongterm(i) ? 0 : 1;
      numLtrp[l] += rpl->isRefPicLongterm(i) && !rpl->isInterLayerRefPic(i) ? 1 : 0;
    }
  }

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const ReferencePictureList *rpl = l == REF_PIC_LIST_0 ? rpl0 : rpl1;

    localRpl[l].setNumberOfLongtermPictures(numLtrp[l]);
    localRpl[l].setNumberOfShorttermPictures(numStrp[l]);
    localRpl[l].setNumberOfInterLayerPictures(numIlrp[l]);
    localRpl[l].setNumberOfActivePictures(std::min<int>(numValidRefs[l], rpl->getNumberOfActivePictures() + (rpl->getNumberOfInterLayerPictures() > 0? 0: numIlrp[l])));
    localRpl[l].setLtrpInSliceHeaderFlag(true);
    slice->setRplIdx(l, -1);
    *slice->getRpl(l) = localRpl[l];
  }

  // Ensure that all pictures in the RefRapIds are included in a reference list.
  for (int i = 0; i < slice->getEdrapNumRefRapPics(); i++)
  {
    int refPoc = slice->getEdrapRefRapId(i) == 0 ? slice->getAssociatedIRAPPOC() : slice->getEdrapRefRapId(i) * m_pcEncLib->getEdrapPeriod();
    if (!slice->isPOCInRefPicList(&localRpl[REF_PIC_LIST_0], refPoc)
        && !slice->isPOCInRefPicList(&localRpl[REF_PIC_LIST_1], refPoc))
    {
      slice->deleteEdrapRefRapIds(i);
    }
  }

}

//! \}
