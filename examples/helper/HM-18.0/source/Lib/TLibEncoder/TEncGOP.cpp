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

/** \file     TEncGOP.cpp
    \brief    GOP encoder class
*/

#include <list>
#include <algorithm>
#include <functional>
#include <cinttypes>

#include "TEncTop.h"
#include "TEncGOP.h"
#include "TEncAnalyze.h"
#include "libmd5/MD5.h"
#include "TLibCommon/SEI.h"
#include "TLibCommon/NAL.h"
#include "NALwrite.h"
#include <time.h>
#include <math.h>

#include <deque>
using namespace std;

//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================
Int getLSB(Int poc, Int maxLSB)
{
  if (poc >= 0)
  {
    return poc % maxLSB;
  }
  else
  {
    return (maxLSB - ((-poc) % maxLSB)) % maxLSB;
  }
}

TEncGOP::TEncGOP()
{
  m_iLastIDR            = 0;
  m_RASPOCforResetEncoder = MAX_INT;

  m_iGopSize            = 0;
  m_iNumPicCoded        = 0; //Niko
  m_bFirst              = true;
  m_iLastRecoveryPicPOC = 0;

  m_pcCfg               = NULL;
  m_pcSliceEncoder      = NULL;
  m_pcListPic           = NULL;

  m_pcEntropyCoder      = NULL;
  m_pcCavlcCoder        = NULL;
  m_pcSbacCoder         = NULL;
  m_pcBinCABAC          = NULL;

  m_bSeqFirst           = true;

  m_bRefreshPending     = 0;
  m_pocCRA            = 0;
  m_numLongTermRefPicSPS = 0;
  ::memset(m_ltRefPicPocLsbSps, 0, sizeof(m_ltRefPicPocLsbSps));
  ::memset(m_ltRefPicUsedByCurrPicFlag, 0, sizeof(m_ltRefPicUsedByCurrPicFlag));
  m_lastBPSEI         = 0;
  m_bufferingPeriodSEIPresentInAU = false;
  m_associatedIRAPType = NAL_UNIT_CODED_SLICE_IDR_N_LP;
  m_associatedIRAPPOC  = 0;
  m_pcDeblockingTempPicYuv = NULL;
}

TEncGOP::~TEncGOP()
{
}

/** Create list to contain pointers to CTU start addresses of slice.
 */
Void  TEncGOP::create()
{
  m_bLongtermTestPictureHasBeenCoded = 0;
  m_bLongtermTestPictureHasBeenCoded2 = 0;
}

Void  TEncGOP::destroy()
{
  if (m_pcDeblockingTempPicYuv)
  {
    m_pcDeblockingTempPicYuv->destroy();
    delete m_pcDeblockingTempPicYuv;
    m_pcDeblockingTempPicYuv = NULL;
  }
#if JVET_X0048_X0103_FILM_GRAIN
  if (m_pcCfg->getFilmGrainAnalysisEnabled())
  {
    m_FGAnalyser.destroy();
  }
#endif
}

Void TEncGOP::init ( TEncTop* pcTEncTop )
{
  m_pcEncTop     = pcTEncTop;
  m_pcCfg                = pcTEncTop;
  m_seiEncoder.init(m_pcCfg, pcTEncTop, this);
  m_pcSliceEncoder       = pcTEncTop->getSliceEncoder();
  m_pcListPic            = pcTEncTop->getListPic();

  m_pcEntropyCoder       = pcTEncTop->getEntropyCoder();
  m_pcCavlcCoder         = pcTEncTop->getCavlcCoder();
  m_pcSbacCoder          = pcTEncTop->getSbacCoder();
  m_pcBinCABAC           = pcTEncTop->getBinCABAC();
  m_pcLoopFilter         = pcTEncTop->getLoopFilter();

  m_pcSAO                = pcTEncTop->getSAO();
  m_pcRateCtrl           = pcTEncTop->getRateCtrl();
  m_lastBPSEI          = 0;
  m_totalCoded         = 0;
#if JVET_X0048_X0103_FILM_GRAIN
  if (m_pcCfg->getFilmGrainAnalysisEnabled())
  {
      m_FGAnalyser.init(m_pcCfg->getSourceWidth(), m_pcCfg->getSourceHeight(), m_pcCfg->getSourcePadding(0),
          m_pcCfg->getSourcePadding(1), IPCOLOURSPACE_UNCHANGED, false, m_pcCfg->getChromaFormatIdc(),
          *(BitDepths*)m_pcCfg->getBitDepthInput(), *(BitDepths*)m_pcCfg->getBitDepth(),
          m_pcCfg->getFrameSkip(), m_pcCfg->getFGCSEICompModelPresent(),
          m_pcCfg->getFilmGrainExternalMask(), m_pcCfg->getFilmGrainExternalDenoised());

  }
#endif

}

#if MCTS_EXTRACTION
Void TEncGOP::generateVPS_RBSP(TComBitIf* rbsp, const TComVPS *vps)
{
  m_pcEntropyCoder->setBitstream(rbsp);
  m_pcEntropyCoder->encodeVPS(vps);
}

Void TEncGOP::generateSPS_RBSP(TComBitIf* rbsp, const TComSPS *sps)
{
  m_pcEntropyCoder->setBitstream(rbsp);
  m_pcEntropyCoder->encodeSPS(sps);
}

Void TEncGOP::generatePPS_RBSP(TComBitIf* rbsp, const TComPPS *pps)
{
  m_pcEntropyCoder->setBitstream(rbsp);
  m_pcEntropyCoder->encodePPS(pps);
}
#endif

Int TEncGOP::xWriteVPS (AccessUnit &accessUnit, const TComVPS *vps)
{
  OutputNALUnit nalu(NAL_UNIT_VPS);
  m_pcEntropyCoder->setBitstream(&nalu.m_Bitstream);
  m_pcEntropyCoder->encodeVPS(vps);
  accessUnit.push_back(new NALUnitEBSP(nalu));
  return (Int)(accessUnit.back()->m_nalUnitData.str().size()) * 8;
}

Int TEncGOP::xWriteSPS (AccessUnit &accessUnit, const TComSPS *sps)
{
  OutputNALUnit nalu(NAL_UNIT_SPS);
  m_pcEntropyCoder->setBitstream(&nalu.m_Bitstream);
  m_pcEntropyCoder->encodeSPS(sps);
  accessUnit.push_back(new NALUnitEBSP(nalu));
  return (Int)(accessUnit.back()->m_nalUnitData.str().size()) * 8;

}

Int TEncGOP::xWritePPS (AccessUnit &accessUnit, const TComPPS *pps)
{
  OutputNALUnit nalu(NAL_UNIT_PPS);
  m_pcEntropyCoder->setBitstream(&nalu.m_Bitstream);
  m_pcEntropyCoder->encodePPS(pps);
  accessUnit.push_back(new NALUnitEBSP(nalu));
  return (Int)(accessUnit.back()->m_nalUnitData.str().size()) * 8;
}


Int TEncGOP::xWriteParameterSets (AccessUnit &accessUnit, TComSlice *slice, const Bool bSeqFirst)
{
  Int actualTotalBits = 0;

  if (bSeqFirst)
  {
    actualTotalBits += xWriteVPS(accessUnit, m_pcEncTop->getVPS());
  }
  if (m_pcEncTop->SPSNeedsWriting(slice->getSPS()->getSPSId())) // Note this assumes that all changes to the SPS are made at the TEncTop level prior to picture creation (TEncTop::xGetNewPicBuffer).
  {
    assert(bSeqFirst); // Implementations that use more than 1 SPS need to be aware of activation issues.
    actualTotalBits += xWriteSPS(accessUnit, slice->getSPS());
  }
  if (m_pcEncTop->PPSNeedsWriting(slice->getPPS()->getPPSId())) // Note this assumes that all changes to the PPS are made at the TEncTop level prior to picture creation (TEncTop::xGetNewPicBuffer).
  {
    actualTotalBits += xWritePPS(accessUnit, slice->getPPS());
  }

  return actualTotalBits;
}

Void TEncGOP::xWriteAccessUnitDelimiter (AccessUnit &accessUnit, TComSlice *slice)
{
  AUDWriter audWriter;
  OutputNALUnit nalu(NAL_UNIT_ACCESS_UNIT_DELIMITER);

  Int picType = slice->isIntra() ? 0 : (slice->isInterP() ? 1 : 2);

  audWriter.codeAUD(nalu.m_Bitstream, picType);
  accessUnit.push_front(new NALUnitEBSP(nalu));
}

// write SEI list into one NAL unit and add it to the Access unit at auPos
Void TEncGOP::xWriteSEI (NalUnitType naluType, SEIMessages& seiMessages, AccessUnit &accessUnit, AccessUnit::iterator &auPos, Int temporalId, const TComSPS *sps)
{
  // don't do anything, if we get an empty list
  if (seiMessages.empty())
  {
    return;
  }
  OutputNALUnit nalu(naluType, temporalId);
  m_seiWriter.writeSEImessages(nalu.m_Bitstream, seiMessages, sps, false);
  auPos = accessUnit.insert(auPos, new NALUnitEBSP(nalu));
  auPos++;
}

Void TEncGOP::xWriteSEISeparately (NalUnitType naluType, SEIMessages& seiMessages, AccessUnit &accessUnit, AccessUnit::iterator &auPos, Int temporalId, const TComSPS *sps)
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
    OutputNALUnit nalu(naluType, temporalId);
    m_seiWriter.writeSEImessages(nalu.m_Bitstream, tmpMessages, sps, false);
    auPos = accessUnit.insert(auPos, new NALUnitEBSP(nalu));
    auPos++;
  }
}

Void TEncGOP::xClearSEIs(SEIMessages& seiMessages, Bool deleteMessages)
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
Void TEncGOP::xWriteLeadingSEIOrdered (SEIMessages& seiMessages, SEIMessages& duInfoSeiMessages, AccessUnit &accessUnit, Int temporalId, const TComSPS *sps, Bool testWrite)
{
  AccessUnit::iterator itNalu = accessUnit.begin();

  while ( (itNalu!=accessUnit.end())&&
    ( (*itNalu)->m_nalUnitType==NAL_UNIT_ACCESS_UNIT_DELIMITER 
    || (*itNalu)->m_nalUnitType==NAL_UNIT_VPS
    || (*itNalu)->m_nalUnitType==NAL_UNIT_SPS
    || (*itNalu)->m_nalUnitType==NAL_UNIT_PPS
    ))
  {
    itNalu++;
  }

  SEIMessages localMessages = seiMessages;
  SEIMessages currentMessages;
  
#if ENC_DEC_TRACE
  g_HLSTraceEnable = !testWrite;
#endif
  // The case that a specific SEI is not present is handled in xWriteSEI (empty list)
#if JCTVC_AD0021_SEI_MANIFEST
  // When SEI Manifest SEI message is present in an SEI NAL unit, the SEI Manifest SEI message shall be the first SEI message in the SEI NAL unit (D3.45 in ISO/IEC 23008-2).
  if (m_pcCfg->getSEIManifestSEIEnabled())
  {
    currentMessages = extractSeisByType(localMessages, SEI::SEI_MANIFEST);
    assert(currentMessages.size() <= 1);
    xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId, sps);
    xClearSEIs(currentMessages, !testWrite);
  }
#endif
#if JCTVC_AD0021_SEI_PREFIX_INDICATION
  // When SEI Manifest SEI message is present in an SEI NAL unit, the SEI Manifest SEI message shall be the first SEI message in the SEI NAL unit (D3.45 in ISO/IEC 23008-2).
  if (m_pcCfg->getSEIPrefixIndicationSEIEnabled())
  {
    currentMessages = extractSeisByType(localMessages, SEI::SEI_PREFIX_INDICATION);
    xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId, sps);
    xClearSEIs(currentMessages, !testWrite);
  }
#endif

  // Active parameter sets SEI must always be the first SEI
  currentMessages = extractSeisByType(localMessages, SEI::ACTIVE_PARAMETER_SETS);
  assert (currentMessages.size() <= 1);
  xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId, sps);
  xClearSEIs(currentMessages, !testWrite);
  
  // Buffering period SEI must always be following active parameter sets
  currentMessages = extractSeisByType(localMessages, SEI::BUFFERING_PERIOD);
  assert (currentMessages.size() <= 1);
  xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId, sps);
  xClearSEIs(currentMessages, !testWrite);

  // Picture timing SEI must always be following buffering period
  currentMessages = extractSeisByType(localMessages, SEI::PICTURE_TIMING);
  assert (currentMessages.size() <= 1);
  xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId, sps);
  xClearSEIs(currentMessages, !testWrite);

  // Decoding unit info SEI must always be following picture timing
  if (!duInfoSeiMessages.empty())
  {
    currentMessages.push_back(duInfoSeiMessages.front());
    if (!testWrite)
    {
      duInfoSeiMessages.pop_front();
    }
    xWriteSEI(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId, sps);
    xClearSEIs(currentMessages, !testWrite);
  }

  // Scalable nesting SEI must always be the following DU info
  currentMessages = extractSeisByType(localMessages, SEI::SCALABLE_NESTING);
  xWriteSEISeparately(NAL_UNIT_PREFIX_SEI, currentMessages, accessUnit, itNalu, temporalId, sps);
  xClearSEIs(currentMessages, !testWrite);

  // And finally everything else one by one
  xWriteSEISeparately(NAL_UNIT_PREFIX_SEI, localMessages, accessUnit, itNalu, temporalId, sps);
  xClearSEIs(localMessages, !testWrite);

  if (!testWrite)
  {
    seiMessages.clear();
  }
}


Void TEncGOP::xWriteLeadingSEIMessages (SEIMessages& seiMessages, SEIMessages& duInfoSeiMessages, AccessUnit &accessUnit, Int temporalId, const TComSPS *sps, std::deque<DUData> &duData)
{
  AccessUnit testAU;
  SEIMessages picTimingSEIs = getSeisByType(seiMessages, SEI::PICTURE_TIMING);
  assert (picTimingSEIs.size() < 2);
  SEIPictureTiming * picTiming = picTimingSEIs.empty() ? NULL : (SEIPictureTiming*) picTimingSEIs.front();

  // test writing
  xWriteLeadingSEIOrdered(seiMessages, duInfoSeiMessages, testAU, temporalId, sps, true);
  // update Timing and DU info SEI
  xUpdateDuData(testAU, duData);
  xUpdateTimingSEI(picTiming, duData, sps);
  xUpdateDuInfoSEI(duInfoSeiMessages, picTiming);
  // actual writing
  xWriteLeadingSEIOrdered(seiMessages, duInfoSeiMessages, accessUnit, temporalId, sps, false);

  // testAU will automatically be cleaned up when losing scope
}

Void TEncGOP::xWriteTrailingSEIMessages (SEIMessages& seiMessages, AccessUnit &accessUnit, Int temporalId, const TComSPS *sps)
{
  // Note: using accessUnit.end() works only as long as this function is called after slice coding and before EOS/EOB NAL units
  AccessUnit::iterator pos = accessUnit.end();
  xWriteSEISeparately(NAL_UNIT_SUFFIX_SEI, seiMessages, accessUnit, pos, temporalId, sps);
  deleteSEIs(seiMessages);
}

Void TEncGOP::xWriteDuSEIMessages (SEIMessages& duInfoSeiMessages, AccessUnit &accessUnit, Int temporalId, const TComSPS *sps, std::deque<DUData> &duData)
{
  const TComHRD *hrd = sps->getVuiParameters()->getHrdParameters();

  if( m_pcCfg->getDecodingUnitInfoSEIEnabled() && hrd->getSubPicCpbParamsPresentFlag() )
  {
    Int naluIdx = 0;
    AccessUnit::iterator nalu = accessUnit.begin();

    // skip over first DU, we have a DU info SEI there already
    while (naluIdx < duData[0].accumNalsDU && nalu!=accessUnit.end())
    {
      naluIdx++;
      nalu++;
    }

    SEIMessages::iterator duSEI = duInfoSeiMessages.begin();
    // loop over remaining DUs
    for (Int duIdx = 1; duIdx < duData.size(); duIdx++)
    {
      if (duSEI == duInfoSeiMessages.end())
      {
        // if the number of generated SEIs matches the number of DUs, this should not happen
        assert (false);
        return;
      }
      // write the next SEI
      SEIMessages tmpSEI;
      tmpSEI.push_back(*duSEI);
      xWriteSEI(NAL_UNIT_PREFIX_SEI, tmpSEI, accessUnit, nalu, temporalId, sps);
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

#if MCTS_EXTRACTION
Void TEncGOP::xCreateIRAPLeadingSEIMessages(SEIMessages& seiMessages, const TComVPS *vps, const TComSPS *sps, const TComPPS *pps)
#else
Void TEncGOP::xCreateIRAPLeadingSEIMessages (SEIMessages& seiMessages, const TComSPS *sps, const TComPPS *pps)
#endif
{
  OutputNALUnit nalu(NAL_UNIT_PREFIX_SEI);

  if(m_pcCfg->getActiveParameterSetsSEIEnabled())
  {
    SEIActiveParameterSets *sei = new SEIActiveParameterSets;
    m_seiEncoder.initSEIActiveParameterSets (sei, m_pcCfg->getVPS(), sps);
    seiMessages.push_back(sei);
  }

  if(m_pcCfg->getFramePackingArrangementSEIEnabled())
  {
    SEIFramePacking *sei = new SEIFramePacking;
    m_seiEncoder.initSEIFramePacking (sei, m_iNumPicCoded);
    seiMessages.push_back(sei);
  }

  if(m_pcCfg->getSegmentedRectFramePackingArrangementSEIEnabled())
  {
    SEISegmentedRectFramePacking *sei = new SEISegmentedRectFramePacking;
    m_seiEncoder.initSEISegmentedRectFramePacking(sei);
    seiMessages.push_back(sei);
  }

  if (m_pcCfg->getDisplayOrientationSEIAngle())
  {
    SEIDisplayOrientation *sei = new SEIDisplayOrientation;
    m_seiEncoder.initSEIDisplayOrientation(sei);
    seiMessages.push_back(sei);
  }

  if(m_pcCfg->getToneMappingInfoSEIEnabled())
  {
    SEIToneMappingInfo *sei = new SEIToneMappingInfo;
    m_seiEncoder.initSEIToneMappingInfo (sei);
    seiMessages.push_back(sei);
  }

  if(m_pcCfg->getTMCTSSEIEnabled())
  {
    SEITempMotionConstrainedTileSets *sei = new SEITempMotionConstrainedTileSets;
    m_seiEncoder.initSEITempMotionConstrainedTileSets(sei, pps);
    seiMessages.push_back(sei);
  }
#if MCTS_EXTRACTION
  if (m_pcCfg->getTMCTSExtractionSEIEnabled())
  {
    SEIMCTSExtractionInfoSet *sei = new SEIMCTSExtractionInfoSet;
    m_seiEncoder.initSEIMCTSExtractionInfo(sei, vps, sps, pps);
    seiMessages.push_back(sei);
  }
#endif
  if(m_pcCfg->getTimeCodeSEIEnabled())
  {
    SEITimeCode *seiTimeCode = new SEITimeCode;
    m_seiEncoder.initSEITimeCode(seiTimeCode);
    seiMessages.push_back(seiTimeCode);
  }

  if(m_pcCfg->getKneeSEIEnabled())
  {
    SEIKneeFunctionInfo *sei = new SEIKneeFunctionInfo;
    m_seiEncoder.initSEIKneeFunctionInfo(sei);
    seiMessages.push_back(sei);
  }

  if (m_pcCfg->getCcvSEIEnabled())
  {
    SEIContentColourVolume *seiContentColourVolume = new SEIContentColourVolume;
    m_seiEncoder.initSEIContentColourVolume(seiContentColourVolume);
    seiMessages.push_back(seiContentColourVolume);
  }

#if SHUTTER_INTERVAL_SEI_MESSAGE
  if (m_pcCfg->getSiiSEIEnabled())
  {
    SEIShutterIntervalInfo *seiShutterInterval = new SEIShutterIntervalInfo;
    m_seiEncoder.initSEIShutterIntervalInfo(seiShutterInterval);
    seiMessages.push_back(seiShutterInterval);
  }
#endif

#if SEI_ENCODER_CONTROL
#if JVET_X0048_X0103_FILM_GRAIN
  if (m_pcCfg->getFilmGrainCharactersticsSEIEnabled() && !m_pcCfg->getFilmGrainCharactersticsSEIPerPictureSEI())
  {
    SEIFilmGrainCharacteristics* seiFGC = new SEIFilmGrainCharacteristics;
    m_seiEncoder.initSEIFilmGrainCharacteristics(seiFGC);
    if (m_pcCfg->getFilmGrainAnalysisEnabled())
    {
      seiFGC->m_log2ScaleFactor = m_FGAnalyser.getLog2scaleFactor();
      for (int compIdx = 0; compIdx < getNumberValidComponents(m_pcCfg->getChromaFormatIdc()); compIdx++)
      {
        if (seiFGC->m_compModel[compIdx].bPresentFlag)
        {   // higher importance of presentFlag is from cfg file
          seiFGC->m_compModel[compIdx] = m_FGAnalyser.getCompModel(compIdx);
        }
      }
    }
    seiMessages.push_back(seiFGC);
}
#else
  // film grain
  if (m_pcCfg->getFilmGrainCharactersticsSEIEnabled())
  {
    SEIFilmGrainCharacteristics *seiFGC = new SEIFilmGrainCharacteristics;
    m_seiEncoder.initSEIFilmGrainCharacteristics(seiFGC);
    seiMessages.push_back(seiFGC);
  }
#endif
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
#endif
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
  if (m_pcCfg->getCmpSEIEnabled())
  {
    SEICubemapProjection *seiCubemapProjection = new SEICubemapProjection;
    m_seiEncoder.initSEICubemapProjection(seiCubemapProjection);
    seiMessages.push_back(seiCubemapProjection);
  }
  if (m_pcCfg->getRwpSEIEnabled())
  {
    SEIRegionWisePacking *seiRegionWisePacking = new SEIRegionWisePacking;
    m_seiEncoder.initSEIRegionWisePacking(seiRegionWisePacking);
    seiMessages.push_back(seiRegionWisePacking);
  }
  if (m_pcCfg->getFviSEIEnabled())
  {
    SEIFisheyeVideoInfo *sei = new SEIFisheyeVideoInfo;
    m_seiEncoder.initSEIFisheyeVideoInfo(sei);
    seiMessages.push_back(sei);
  }
    
  if(m_pcCfg->getMasteringDisplaySEI().colourVolumeSEIEnabled)
  {
    const TComSEIMasteringDisplay &seiCfg=m_pcCfg->getMasteringDisplaySEI();
    SEIMasteringDisplayColourVolume *sei = new SEIMasteringDisplayColourVolume;
    sei->values = seiCfg;
    seiMessages.push_back(sei);
  }
  if(m_pcCfg->getChromaResamplingFilterHintEnabled())
  {
    SEIChromaResamplingFilterHint *seiChromaResamplingFilterHint = new SEIChromaResamplingFilterHint;
    m_seiEncoder.initSEIChromaResamplingFilterHint(seiChromaResamplingFilterHint, m_pcCfg->getChromaResamplingHorFilterIdc(), m_pcCfg->getChromaResamplingVerFilterIdc());
    seiMessages.push_back(seiChromaResamplingFilterHint);
  }
  if(m_pcCfg->getSEIAlternativeTransferCharacteristicsSEIEnable())
  {
    SEIAlternativeTransferCharacteristics *seiAlternativeTransferCharacteristics = new SEIAlternativeTransferCharacteristics;
    m_seiEncoder.initSEIAlternativeTransferCharacteristics(seiAlternativeTransferCharacteristics);
    seiMessages.push_back(seiAlternativeTransferCharacteristics);
  }
}

Void TEncGOP::xCreatePerPictureSEIMessages (Int picInGOP, SEIMessages& seiMessages, SEIMessages& nestedSeiMessages, TComSlice *slice)
{
  if( ( m_pcCfg->getBufferingPeriodSEIEnabled() ) && ( slice->getSliceType() == I_SLICE ) &&
    ( slice->getSPS()->getVuiParametersPresentFlag() ) &&
    ( ( slice->getSPS()->getVuiParameters()->getHrdParameters()->getNalHrdParametersPresentFlag() )
    || ( slice->getSPS()->getVuiParameters()->getHrdParameters()->getVclHrdParametersPresentFlag() ) ) )
  {
    SEIBufferingPeriod *bufferingPeriodSEI = new SEIBufferingPeriod();
    m_seiEncoder.initSEIBufferingPeriod(bufferingPeriodSEI, slice);
    seiMessages.push_back(bufferingPeriodSEI);
    m_bufferingPeriodSEIPresentInAU = true;

    if (m_pcCfg->getScalableNestingSEIEnabled())
    {
      SEIBufferingPeriod *bufferingPeriodSEIcopy = new SEIBufferingPeriod();
      bufferingPeriodSEI->copyTo(*bufferingPeriodSEIcopy);
      nestedSeiMessages.push_back(bufferingPeriodSEIcopy);
    }
  }

  if (picInGOP ==0 && m_pcCfg->getSOPDescriptionSEIEnabled() ) // write SOP description SEI (if enabled) at the beginning of GOP
  {
    SEISOPDescription* sopDescriptionSEI = new SEISOPDescription();
    m_seiEncoder.initSEISOPDescription(sopDescriptionSEI, slice, picInGOP, m_iLastIDR, m_iGopSize);
    seiMessages.push_back(sopDescriptionSEI);
  }

  if( ( m_pcEncTop->getRecoveryPointSEIEnabled() ) && ( slice->getSliceType() == I_SLICE ) )
  {
    if( m_pcEncTop->getGradualDecodingRefreshInfoEnabled() && !slice->getRapPicFlag() )
    {
      // Gradual decoding refresh SEI
      SEIRegionRefreshInfo *gradualDecodingRefreshInfoSEI = new SEIRegionRefreshInfo();
      gradualDecodingRefreshInfoSEI->m_gdrForegroundFlag = true; // Indicating all "foreground"
      seiMessages.push_back(gradualDecodingRefreshInfoSEI);
    }
    // Recovery point SEI
    SEIRecoveryPoint *recoveryPointSEI = new SEIRecoveryPoint();
    m_seiEncoder.initSEIRecoveryPoint(recoveryPointSEI, slice);
    seiMessages.push_back(recoveryPointSEI);
  }
  if (m_pcCfg->getTemporalLevel0IndexSEIEnabled())
  {
    SEITemporalLevel0Index *temporalLevel0IndexSEI = new SEITemporalLevel0Index();
    m_seiEncoder.initTemporalLevel0IndexSEI(temporalLevel0IndexSEI, slice);
    seiMessages.push_back(temporalLevel0IndexSEI);
  }

  if( m_pcEncTop->getNoDisplaySEITLayer() && ( slice->getTLayer() >= m_pcEncTop->getNoDisplaySEITLayer() ) )
  {
    SEINoDisplay *seiNoDisplay = new SEINoDisplay;
    seiNoDisplay->m_noDisplay = true;
    seiMessages.push_back(seiNoDisplay);
  }

  // insert one Colour Remapping Info SEI for the picture (if the file exists)
  if (!m_pcCfg->getColourRemapInfoSEIFileRoot().empty())
  {
    SEIColourRemappingInfo *seiColourRemappingInfo = new SEIColourRemappingInfo();
    const Bool success = m_seiEncoder.initSEIColourRemappingInfo(seiColourRemappingInfo, slice->getPOC() );

    if(success)
    {
      seiMessages.push_back(seiColourRemappingInfo);
    }
    else
    {
      delete seiColourRemappingInfo;
    }
  }

  // insert one Annotated Region SEI for the picture (if the file exists)
  if (!m_pcCfg->getAnnotatedRegionSEIFileRoot().empty())
  {
    SEIAnnotatedRegions *seiAnnotatedRegions = new SEIAnnotatedRegions();
    const Bool success = m_seiEncoder.initSEIAnnotatedRegions(seiAnnotatedRegions, slice->getPOC());

    if (success)
    {
      seiMessages.push_back(seiAnnotatedRegions);
    }
    else
    {
      delete seiAnnotatedRegions;
    }
  }
  // insert one Regional Nesting SEI for the picture (if the file exists)
  if (!m_pcCfg->getRegionalNestingSEIFileRoot().empty())
  {
    SEIRegionalNesting *seiRegionalNesting= new SEIRegionalNesting();
    const Bool success = m_seiEncoder.initSEIRegionalNesting(seiRegionalNesting, slice->getPOC() );

    if(success)
    {
      seiMessages.push_back(seiRegionalNesting);
    }
    else
    {
      delete seiRegionalNesting;
    }
  }
#if JVET_X0048_X0103_FILM_GRAIN
  // Film Grain Characteristics SEI insertion at at frame level
  if (m_pcCfg->getFilmGrainCharactersticsSEIEnabled() && m_pcCfg->getFilmGrainCharactersticsSEIPerPictureSEI())
  {
    SEIFilmGrainCharacteristics* fgcSEI = new SEIFilmGrainCharacteristics;
    m_seiEncoder.initSEIFilmGrainCharacteristics(fgcSEI);
    if (m_pcCfg->getFilmGrainAnalysisEnabled())
    {
      fgcSEI->m_log2ScaleFactor = m_FGAnalyser.getLog2scaleFactor();
      for (int compIdx = 0; compIdx < getNumberValidComponents(m_pcCfg->getChromaFormatIdc()); compIdx++)
      {
        if (fgcSEI->m_compModel[compIdx].bPresentFlag)
        {   // higher importance of presentFlag is from cfg file
          fgcSEI->m_compModel[compIdx] = m_FGAnalyser.getCompModel(compIdx);
        }
      }
    }
    seiMessages.push_back(fgcSEI);
  }
#endif

#if JCTVC_AD0021_SEI_MANIFEST
  // Make sure that sei_manifest and sei_prefix are the last two initialized sei_msg, otherwise it will cause these two
  // Sei messages to not be able to enter all SEI messages
  if (m_pcCfg->getSEIManifestSEIEnabled())
  {
    SEIManifest* seiSEIManifest = new SEIManifest;
    m_seiEncoder.initSEISEIManifest(seiSEIManifest, seiMessages);
    seiMessages.push_back(seiSEIManifest);
  }
#endif
#if JCTVC_AD0021_SEI_PREFIX_INDICATION
  if (m_pcCfg->getSEIPrefixIndicationSEIEnabled())
  {
    int NumOfSEIPrefixMsg = 0;
    for (auto& it : seiMessages)
    {
      if (it->payloadType() == SEI::SEI_MANIFEST)
      {
        break;
      }
      NumOfSEIPrefixMsg++;
    }
    for (auto& it : seiMessages)
    {
      if (NumOfSEIPrefixMsg == 0 || it->payloadType() == SEI::SEI_MANIFEST)
      {
        break;
      }
      SEIPrefixIndication* seiSEIPrefixIndication = new SEIPrefixIndication;
      m_seiEncoder.initSEISEIPrefixIndication(seiSEIPrefixIndication, it);
      seiMessages.push_back(seiSEIPrefixIndication);
      NumOfSEIPrefixMsg--;
    }
  }
#endif
}

Void TEncGOP::xCreateScalableNestingSEI (SEIMessages& seiMessages, SEIMessages& nestedSeiMessages)
{
  SEIMessages tmpMessages;
  while (!nestedSeiMessages.empty())
  {
    SEI* sei=nestedSeiMessages.front();
    nestedSeiMessages.pop_front();
    tmpMessages.push_back(sei);
    SEIScalableNesting *nestingSEI = new SEIScalableNesting();
    m_seiEncoder.initSEIScalableNesting(nestingSEI, tmpMessages);
    seiMessages.push_back(nestingSEI);
    tmpMessages.clear();
  }
}

Void TEncGOP::xCreatePictureTimingSEI  (Int IRAPGOPid, SEIMessages& seiMessages, SEIMessages& nestedSeiMessages, SEIMessages& duInfoSeiMessages, TComSlice *slice, Bool isField, std::deque<DUData> &duData)
{
  const TComVUI *vui = slice->getSPS()->getVuiParameters();
  const TComHRD *hrd = vui->getHrdParameters();

  // update decoding unit parameters
  if( ( m_pcCfg->getPictureTimingSEIEnabled() || m_pcCfg->getDecodingUnitInfoSEIEnabled() ) &&
    ( slice->getSPS()->getVuiParametersPresentFlag() ) &&
    (  hrd->getNalHrdParametersPresentFlag() || hrd->getVclHrdParametersPresentFlag() ) )
  {
    Int picSptDpbOutputDuDelay = 0;
    SEIPictureTiming *pictureTimingSEI = new SEIPictureTiming();

    // DU parameters
    if( hrd->getSubPicCpbParamsPresentFlag() )
    {
      UInt numDU = (UInt) duData.size();
      pictureTimingSEI->m_numDecodingUnitsMinus1     = ( numDU - 1 );
      pictureTimingSEI->m_duCommonCpbRemovalDelayFlag = false;
      pictureTimingSEI->m_numNalusInDuMinus1.resize( numDU );
      pictureTimingSEI->m_duCpbRemovalDelayMinus1.resize( numDU );
    }
    pictureTimingSEI->m_auCpbRemovalDelay = std::min<Int>(std::max<Int>(1, m_totalCoded - m_lastBPSEI), static_cast<Int>(pow(2, static_cast<Double>(hrd->getCpbRemovalDelayLengthMinus1()+1)))); // Syntax element signalled as minus, hence the .
    pictureTimingSEI->m_picDpbOutputDelay = slice->getSPS()->getNumReorderPics(slice->getSPS()->getMaxTLayers()-1) + slice->getPOC() - m_totalCoded;
    if(m_pcCfg->getEfficientFieldIRAPEnabled() && IRAPGOPid > 0 && IRAPGOPid < m_iGopSize)
    {
      // if pictures have been swapped there is likely one more picture delay on their tid. Very rough approximation
      pictureTimingSEI->m_picDpbOutputDelay ++;
    }
    Int factor = hrd->getTickDivisorMinus2() + 2;
    pictureTimingSEI->m_picDpbOutputDuDelay = factor * pictureTimingSEI->m_picDpbOutputDelay;
    if( m_pcCfg->getDecodingUnitInfoSEIEnabled() )
    {
      picSptDpbOutputDuDelay = factor * pictureTimingSEI->m_picDpbOutputDelay;
    }
    if (m_bufferingPeriodSEIPresentInAU)
    {
      m_lastBPSEI = m_totalCoded;
    }

    if( hrd->getSubPicCpbParamsPresentFlag() )
    {
      Int i;
      UInt64 ui64Tmp;
      UInt uiPrev = 0;
      UInt numDU = ( pictureTimingSEI->m_numDecodingUnitsMinus1 + 1 );
      std::vector<UInt> &rDuCpbRemovalDelayMinus1 = pictureTimingSEI->m_duCpbRemovalDelayMinus1;
      UInt maxDiff = ( hrd->getTickDivisorMinus2() + 2 ) - 1;

      for( i = 0; i < numDU; i ++ )
      {
        pictureTimingSEI->m_numNalusInDuMinus1[ i ]       = ( i == 0 ) ? ( duData[i].accumNalsDU - 1 ) : ( duData[i].accumNalsDU- duData[i-1].accumNalsDU - 1 );
      }

      if( numDU == 1 )
      {
        rDuCpbRemovalDelayMinus1[ 0 ] = 0; /* don't care */
      }
      else
      {
        rDuCpbRemovalDelayMinus1[ numDU - 1 ] = 0;/* by definition */
        UInt tmp = 0;
        UInt accum = 0;

        for( i = ( numDU - 2 ); i >= 0; i -- )
        {
          ui64Tmp = ( ( ( duData[numDU - 1].accumBitsDU  - duData[i].accumBitsDU ) * ( vui->getTimingInfo()->getTimeScale() / vui->getTimingInfo()->getNumUnitsInTick() ) * ( hrd->getTickDivisorMinus2() + 2 ) ) / ( m_pcCfg->getTargetBitrate() ) );
          if( (UInt)ui64Tmp > maxDiff )
          {
            tmp ++;
          }
        }
        uiPrev = 0;

        UInt flag = 0;
        for( i = ( numDU - 2 ); i >= 0; i -- )
        {
          flag = 0;
          ui64Tmp = ( ( ( duData[numDU - 1].accumBitsDU  - duData[i].accumBitsDU ) * ( vui->getTimingInfo()->getTimeScale() / vui->getTimingInfo()->getNumUnitsInTick() ) * ( hrd->getTickDivisorMinus2() + 2 ) ) / ( m_pcCfg->getTargetBitrate() ) );

          if( (UInt)ui64Tmp > maxDiff )
          {
            if(uiPrev >= maxDiff - tmp)
            {
              ui64Tmp = uiPrev + 1;
              flag = 1;
            }
            else                            ui64Tmp = maxDiff - tmp + 1;
          }
          rDuCpbRemovalDelayMinus1[ i ] = (UInt)ui64Tmp - uiPrev - 1;
          if( (Int)rDuCpbRemovalDelayMinus1[ i ] < 0 )
          {
            rDuCpbRemovalDelayMinus1[ i ] = 0;
          }
          else if (tmp > 0 && flag == 1)
          {
            tmp --;
          }
          accum += rDuCpbRemovalDelayMinus1[ i ] + 1;
          uiPrev = accum;
        }
      }
    }
    
    if( m_pcCfg->getPictureTimingSEIEnabled() )
    {
      pictureTimingSEI->m_picStruct = (isField && slice->getPic()->isTopField())? 1 : isField? 2 : 0;
      seiMessages.push_back(pictureTimingSEI);

      if ( m_pcCfg->getScalableNestingSEIEnabled() ) // put picture timing SEI into scalable nesting SEI
      {
        SEIPictureTiming *pictureTimingSEIcopy = new SEIPictureTiming();
        pictureTimingSEI->copyTo(*pictureTimingSEIcopy);
        nestedSeiMessages.push_back(pictureTimingSEIcopy);
      }
    }

    if( m_pcCfg->getDecodingUnitInfoSEIEnabled() && hrd->getSubPicCpbParamsPresentFlag() )
    {
      for( Int i = 0; i < ( pictureTimingSEI->m_numDecodingUnitsMinus1 + 1 ); i ++ )
      {
        SEIDecodingUnitInfo *duInfoSEI = new SEIDecodingUnitInfo();
        duInfoSEI->m_decodingUnitIdx = i;
        duInfoSEI->m_duSptCpbRemovalDelay = pictureTimingSEI->m_duCpbRemovalDelayMinus1[i] + 1;
        duInfoSEI->m_dpbOutputDuDelayPresentFlag = false;
        duInfoSEI->m_picSptDpbOutputDuDelay = picSptDpbOutputDuDelay;

        duInfoSeiMessages.push_back(duInfoSEI);
      }
    }

    if( !m_pcCfg->getPictureTimingSEIEnabled() && pictureTimingSEI )
    {
      delete pictureTimingSEI;
    }
  }
}

Void TEncGOP::xUpdateDuData(AccessUnit &testAU, std::deque<DUData> &duData)
{
  if (duData.empty())
  {
    return;
  }
  // fix first 
  UInt numNalUnits = (UInt)testAU.size();
  UInt numRBSPBytes = 0;
  for (AccessUnit::const_iterator it = testAU.begin(); it != testAU.end(); it++)
  {
    numRBSPBytes += UInt((*it)->m_nalUnitData.str().size());
  }
  duData[0].accumBitsDU += ( numRBSPBytes << 3 );
  duData[0].accumNalsDU += numNalUnits;

  // adapt cumulative sums for all following DUs
  // and add one DU info SEI, if enabled
  for (Int i=1; i<duData.size(); i++)
  {
    if (m_pcCfg->getDecodingUnitInfoSEIEnabled())
    {
      numNalUnits  += 1;
      numRBSPBytes += ( 5 << 3 );
    }
    duData[i].accumBitsDU += numRBSPBytes; // probably around 5 bytes
    duData[i].accumNalsDU += numNalUnits;
  }

  // The last DU may have a trailing SEI
  if (m_pcCfg->getDecodedPictureHashSEIType()!=HASHTYPE_NONE)
  {
    duData.back().accumBitsDU += ( 20 << 3 ); // probably around 20 bytes - should be further adjusted, e.g. by type
    duData.back().accumNalsDU += 1;
  }

}
Void TEncGOP::xUpdateTimingSEI(SEIPictureTiming *pictureTimingSEI, std::deque<DUData> &duData, const TComSPS *sps)
{
  if (!pictureTimingSEI)
  {
    return;
  }
  const TComVUI *vui = sps->getVuiParameters();
  const TComHRD *hrd = vui->getHrdParameters();
  if( hrd->getSubPicCpbParamsPresentFlag() )
  {
    Int i;
    UInt64 ui64Tmp;
    UInt uiPrev = 0;
    UInt numDU = ( pictureTimingSEI->m_numDecodingUnitsMinus1 + 1 );
    std::vector<UInt> &rDuCpbRemovalDelayMinus1 = pictureTimingSEI->m_duCpbRemovalDelayMinus1;
    UInt maxDiff = ( hrd->getTickDivisorMinus2() + 2 ) - 1;

    for( i = 0; i < numDU; i ++ )
    {
      pictureTimingSEI->m_numNalusInDuMinus1[ i ]       = ( i == 0 ) ? ( duData[i].accumNalsDU - 1 ) : ( duData[i].accumNalsDU- duData[i-1].accumNalsDU - 1 );
    }

    if( numDU == 1 )
    {
      rDuCpbRemovalDelayMinus1[ 0 ] = 0; /* don't care */
    }
    else
    {
      rDuCpbRemovalDelayMinus1[ numDU - 1 ] = 0;/* by definition */
      UInt tmp = 0;
      UInt accum = 0;

      for( i = ( numDU - 2 ); i >= 0; i -- )
      {
        ui64Tmp = ( ( ( duData[numDU - 1].accumBitsDU  - duData[i].accumBitsDU ) * ( vui->getTimingInfo()->getTimeScale() / vui->getTimingInfo()->getNumUnitsInTick() ) * ( hrd->getTickDivisorMinus2() + 2 ) ) / ( m_pcCfg->getTargetBitrate() ) );
        if( (UInt)ui64Tmp > maxDiff )
        {
          tmp ++;
        }
      }
      uiPrev = 0;

      UInt flag = 0;
      for( i = ( numDU - 2 ); i >= 0; i -- )
      {
        flag = 0;
        ui64Tmp = ( ( ( duData[numDU - 1].accumBitsDU  - duData[i].accumBitsDU ) * ( vui->getTimingInfo()->getTimeScale() / vui->getTimingInfo()->getNumUnitsInTick() ) * ( hrd->getTickDivisorMinus2() + 2 ) ) / ( m_pcCfg->getTargetBitrate() ) );

        if( (UInt)ui64Tmp > maxDiff )
        {
          if(uiPrev >= maxDiff - tmp)
          {
            ui64Tmp = uiPrev + 1;
            flag = 1;
          }
          else                            ui64Tmp = maxDiff - tmp + 1;
        }
        rDuCpbRemovalDelayMinus1[ i ] = (UInt)ui64Tmp - uiPrev - 1;
        if( (Int)rDuCpbRemovalDelayMinus1[ i ] < 0 )
        {
          rDuCpbRemovalDelayMinus1[ i ] = 0;
        }
        else if (tmp > 0 && flag == 1)
        {
          tmp --;
        }
        accum += rDuCpbRemovalDelayMinus1[ i ] + 1;
        uiPrev = accum;
      }
    }
  }
}
Void TEncGOP::xUpdateDuInfoSEI(SEIMessages &duInfoSeiMessages, SEIPictureTiming *pictureTimingSEI)
{
  if (duInfoSeiMessages.empty() || (pictureTimingSEI == NULL))
  {
    return;
  }

  Int i=0;

  for (SEIMessages::iterator du = duInfoSeiMessages.begin(); du!= duInfoSeiMessages.end(); du++)
  {
    SEIDecodingUnitInfo *duInfoSEI = (SEIDecodingUnitInfo*) (*du);
    duInfoSEI->m_decodingUnitIdx = i;
    duInfoSEI->m_duSptCpbRemovalDelay = pictureTimingSEI->m_duCpbRemovalDelayMinus1[i] + 1;
    duInfoSEI->m_dpbOutputDuDelayPresentFlag = false;
    i++;
  }
}

static Void
cabac_zero_word_padding(TComSlice *const pcSlice, TComPic *const pcPic, const std::size_t binCountsInNalUnits, const std::size_t numBytesInVclNalUnits, std::ostringstream &nalUnitData, const Bool cabacZeroWordPaddingEnabled)
{
  const TComSPS &sps=*(pcSlice->getSPS());
  const Int log2subWidthCxsubHeightC = (pcPic->getComponentScaleX(COMPONENT_Cb)+pcPic->getComponentScaleY(COMPONENT_Cb));
  const Int minCuWidth  = pcPic->getMinCUWidth();
  const Int minCuHeight = pcPic->getMinCUHeight();
  const Int paddedWidth = ((sps.getPicWidthInLumaSamples()  + minCuWidth  - 1) / minCuWidth) * minCuWidth;
  const Int paddedHeight= ((sps.getPicHeightInLumaSamples() + minCuHeight - 1) / minCuHeight) * minCuHeight;
  const Int rawBits = paddedWidth * paddedHeight *
                         (sps.getBitDepth(CHANNEL_TYPE_LUMA) + ((2*sps.getBitDepth(CHANNEL_TYPE_CHROMA))>>log2subWidthCxsubHeightC));
  const std::size_t threshold = (32/3)*numBytesInVclNalUnits + (rawBits/32);
  if (binCountsInNalUnits >= threshold)
  {
    // need to add additional cabac zero words (each one accounts for 3 bytes (=00 00 03)) to increase numBytesInVclNalUnits
    const std::size_t targetNumBytesInVclNalUnits = ((binCountsInNalUnits - (rawBits/32))*3+31)/32;

    if (targetNumBytesInVclNalUnits>numBytesInVclNalUnits) // It should be!
    {
      const std::size_t numberOfAdditionalBytesNeeded=targetNumBytesInVclNalUnits - numBytesInVclNalUnits;
      const std::size_t numberOfAdditionalCabacZeroWords=(numberOfAdditionalBytesNeeded+2)/3;
      const std::size_t numberOfAdditionalCabacZeroBytes=numberOfAdditionalCabacZeroWords*3;
      if (cabacZeroWordPaddingEnabled)
      {
        std::vector<UChar> zeroBytesPadding(numberOfAdditionalCabacZeroBytes, UChar(0));
        for(std::size_t i=0; i<numberOfAdditionalCabacZeroWords; i++)
        {
          zeroBytesPadding[i*3+2]=3;  // 00 00 03
        }
        nalUnitData.write(reinterpret_cast<const TChar*>(&(zeroBytesPadding[0])), numberOfAdditionalCabacZeroBytes);
        printf("Adding %d bytes of padding\n", UInt(numberOfAdditionalCabacZeroWords*3));
      }
      else
      {
        printf("Standard would normally require adding %d bytes of padding\n", UInt(numberOfAdditionalCabacZeroWords*3));
      }
    }
  }
}

class EfficientFieldIRAPMapping
{
  private:
    Int  IRAPGOPid;
    Bool IRAPtoReorder;
    Bool swapIRAPForward;

  public:
    EfficientFieldIRAPMapping() :
      IRAPGOPid(-1),
      IRAPtoReorder(false),
      swapIRAPForward(false)
    { }

    Void initialize(const Bool isField, const Int gopSize, const Int POCLast, const Int numPicRcvd, const Int lastIDR, TEncGOP *pEncGop, TEncCfg *pCfg);

    Int adjustGOPid(const Int gopID);
    Int restoreGOPid(const Int gopID);
    Int GetIRAPGOPid() const { return IRAPGOPid; }
};

Void EfficientFieldIRAPMapping::initialize(const Bool isField, const Int gopSize, const Int POCLast, const Int numPicRcvd, const Int lastIDR, TEncGOP *pEncGop, TEncCfg *pCfg )
{
  if(isField)
  {
    Int pocCurr;
    for ( Int iGOPid=0; iGOPid < gopSize; iGOPid++ )
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
        pocCurr = POCLast - numPicRcvd + pCfg->getGOPEntry(iGOPid).m_POC - isField;
      }

      // check if POC corresponds to IRAP
      NalUnitType tmpUnitType = pEncGop->getNalUnitType(pocCurr, lastIDR, isField);
      if(tmpUnitType >= NAL_UNIT_CODED_SLICE_BLA_W_LP && tmpUnitType <= NAL_UNIT_CODED_SLICE_CRA) // if picture is an IRAP
      {
        if(pocCurr%2 == 0 && iGOPid < gopSize-1 && pCfg->getGOPEntry(iGOPid).m_POC == pCfg->getGOPEntry(iGOPid+1).m_POC-1)
        { // if top field and following picture in enc order is associated bottom field
          IRAPGOPid = iGOPid;
          IRAPtoReorder = true;
          swapIRAPForward = true; 
          break;
        }
        if(pocCurr%2 != 0 && iGOPid > 0 && pCfg->getGOPEntry(iGOPid).m_POC == pCfg->getGOPEntry(iGOPid-1).m_POC+1)
        {
          // if picture is an IRAP remember to process it first
          IRAPGOPid = iGOPid;
          IRAPtoReorder = true;
          swapIRAPForward = false; 
          break;
        }
      }
    }
  }
}

Int EfficientFieldIRAPMapping::adjustGOPid(const Int GOPid)
{
  if(IRAPtoReorder)
  {
    if(swapIRAPForward)
    {
      if(GOPid == IRAPGOPid)
      {
        return IRAPGOPid +1;
      }
      else if(GOPid == IRAPGOPid +1)
      {
        return IRAPGOPid;
      }
    }
    else
    {
      if(GOPid == IRAPGOPid -1)
      {
        return IRAPGOPid;
      }
      else if(GOPid == IRAPGOPid)
      {
        return IRAPGOPid -1;
      }
    }
  }
  return GOPid;
}

Int EfficientFieldIRAPMapping::restoreGOPid(const Int GOPid)
{
  if(IRAPtoReorder)
  {
    if(swapIRAPForward)
    {
      if(GOPid == IRAPGOPid)
      {
        IRAPtoReorder = false;
        return IRAPGOPid +1;
      }
      else if(GOPid == IRAPGOPid +1)
      {
        return GOPid -1;
      }
    }
    else
    {
      if(GOPid == IRAPGOPid)
      {
        return IRAPGOPid -1;
      }
      else if(GOPid == IRAPGOPid -1)
      {
        IRAPtoReorder = false;
        return IRAPGOPid;
      }
    }
  }
  return GOPid;
}


static UInt calculateCollocatedFromL0Flag(const TComSlice *pSlice)
{
  const Int refIdx = 0; // Zero always assumed
  const TComPic *refPicL0 = pSlice->getRefPic(REF_PIC_LIST_0, refIdx);
  const TComPic *refPicL1 = pSlice->getRefPic(REF_PIC_LIST_1, refIdx);
  return refPicL0->getSlice(0)->getSliceQp() > refPicL1->getSlice(0)->getSliceQp();
}


static Void
printHash(const HashType hashType, const std::string &digestStr)
{
  const TChar *decodedPictureHashModeName;
  switch (hashType)
  {
    case HASHTYPE_MD5:
      decodedPictureHashModeName = "MD5";
      break;
    case HASHTYPE_CRC:
      decodedPictureHashModeName = "CRC";
      break;
    case HASHTYPE_CHECKSUM:
      decodedPictureHashModeName = "Checksum";
      break;
    default:
      decodedPictureHashModeName = NULL;
      break;
  }
  if (decodedPictureHashModeName != NULL)
  {
    if (digestStr.empty())
    {
      printf(" [%s:%s]", decodedPictureHashModeName, "?");
    }
    else
    {
      printf(" [%s:%s]", decodedPictureHashModeName, digestStr.c_str());
    }
  }
}


// ====================================================================================================================
// Public member functions
// ====================================================================================================================
Void TEncGOP::compressGOP( Int iPOCLast, Int iNumPicRcvd, TComList<TComPic*>& rcListPic,
                           TComList<TComPicYuv*>& rcListPicYuvRecOut, std::list<AccessUnit>& accessUnitsInGOP,
                           Bool isField, Bool isTff, const InputColourSpaceConversion ip_conversion, const InputColourSpaceConversion snr_conversion, const TEncAnalyze::OutputLogControl &outputLogCtrl )
{
  // TODO: Split this function up.

  TComPic*        pcPic = NULL;
  TComPicYuv*     pcPicYuvRecOut;
  TComSlice*      pcSlice;
  TComOutputBitstream  *pcBitstreamRedirect;
  pcBitstreamRedirect = new TComOutputBitstream;
  AccessUnit::iterator  itLocationToPushSliceHeaderNALU; // used to store location where NALU containing slice header is to be inserted

  xInitGOP( iPOCLast, iNumPicRcvd, isField );

  m_iNumPicCoded = 0;
  SEIMessages leadingSeiMessages;
  SEIMessages nestedSeiMessages;
  SEIMessages duInfoSeiMessages;
  SEIMessages trailingSeiMessages;
  std::deque<DUData> duData;
  SEIDecodingUnitInfo decodingUnitInfoSEI;

  EfficientFieldIRAPMapping effFieldIRAPMap;
  if (m_pcCfg->getEfficientFieldIRAPEnabled())
  {
    effFieldIRAPMap.initialize(isField, m_iGopSize, iPOCLast, iNumPicRcvd, m_iLastIDR, this, m_pcCfg);
  }

  // reset flag indicating whether pictures have been encoded
  for ( Int iGOPid=0; iGOPid < m_iGopSize; iGOPid++ )
  {
    m_pcCfg->setEncodedFlag(iGOPid, false);
  }

  for ( Int iGOPid=0; iGOPid < m_iGopSize; iGOPid++ )
  {
    if (m_pcCfg->getEfficientFieldIRAPEnabled())
    {
      iGOPid=effFieldIRAPMap.adjustGOPid(iGOPid);
    }

    //-- For time output for each slice
    clock_t iBeforeTime = clock();


    /////////////////////////////////////////////////////////////////////////////////////////////////// Initial to start encoding
    Int iTimeOffset;
    Int pocCurr;

    if(iPOCLast == 0) //case first frame or first top field
    {
      pocCurr=0;
      iTimeOffset = 1;
    }
    else if(iPOCLast == 1 && isField) //case first bottom field, just like the first frame, the poc computation is not right anymore, we set the right value
    {
      pocCurr = 1;
      iTimeOffset = 1;
    }
    else
    {
      pocCurr = iPOCLast - iNumPicRcvd + m_pcCfg->getGOPEntry(iGOPid).m_POC - ((isField && m_iGopSize>1) ? 1:0);
      iTimeOffset = m_pcCfg->getGOPEntry(iGOPid).m_POC;
    }

    if(pocCurr>=m_pcCfg->getFramesToBeEncoded())
    {
      if (m_pcCfg->getEfficientFieldIRAPEnabled())
      {
        iGOPid=effFieldIRAPMap.restoreGOPid(iGOPid);
      }
      continue;
    }

    if( getNalUnitType(pocCurr, m_iLastIDR, isField) == NAL_UNIT_CODED_SLICE_IDR_W_RADL || getNalUnitType(pocCurr, m_iLastIDR, isField) == NAL_UNIT_CODED_SLICE_IDR_N_LP )
    {
      m_iLastIDR = pocCurr;
    }
    // start a new access unit: create an entry in the list of output access units
    accessUnitsInGOP.push_back(AccessUnit());
    AccessUnit& accessUnit = accessUnitsInGOP.back();
    xGetBuffer( rcListPic, rcListPicYuvRecOut, iNumPicRcvd, iTimeOffset, pcPic, pcPicYuvRecOut, pocCurr, isField );

#if REDUCED_ENCODER_MEMORY
#if SHUTTER_INTERVAL_SEI_PROCESSING
    pcPic->prepareForReconstruction( m_pcCfg->getShutterFilterFlag() );
#else
    pcPic->prepareForReconstruction();
#endif

#endif
    //  Slice data initialization
    pcPic->clearSliceBuffer();
    pcPic->allocateNewSlice();
    m_pcSliceEncoder->setSliceIdx(0);
    pcPic->setCurrSliceIdx(0);

    m_pcSliceEncoder->initEncSlice ( pcPic, iPOCLast, pocCurr, iGOPid, pcSlice, isField );

    pcSlice->setLastIDR(m_iLastIDR);
    pcSlice->setSliceIdx(0);
    //set default slice level flag to the same as SPS level flag
    pcSlice->setLFCrossSliceBoundaryFlag(  pcSlice->getPPS()->getLoopFilterAcrossSlicesEnabledFlag()  );

    if(pcSlice->getSliceType()==B_SLICE&&m_pcCfg->getGOPEntry(iGOPid).m_sliceType=='P')
    {
      pcSlice->setSliceType(P_SLICE);
    }
    if(pcSlice->getSliceType()==B_SLICE&&m_pcCfg->getGOPEntry(iGOPid).m_sliceType=='I')
    {
      pcSlice->setSliceType(I_SLICE);
    }
    
    // Set the nal unit type
    pcSlice->setNalUnitType(getNalUnitType(pocCurr, m_iLastIDR, isField));
    if(pcSlice->getTemporalLayerNonReferenceFlag())
    {
      if (pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_TRAIL_R &&
          !(m_iGopSize == 1 && pcSlice->getSliceType() == I_SLICE))
        // Add this condition to avoid POC issues with encoder_intra_main.cfg configuration (see #1127 in bug tracker)
      {
        pcSlice->setNalUnitType(NAL_UNIT_CODED_SLICE_TRAIL_N);
      }
      if(pcSlice->getNalUnitType()==NAL_UNIT_CODED_SLICE_RADL_R)
      {
        pcSlice->setNalUnitType(NAL_UNIT_CODED_SLICE_RADL_N);
      }
      if(pcSlice->getNalUnitType()==NAL_UNIT_CODED_SLICE_RASL_R)
      {
        pcSlice->setNalUnitType(NAL_UNIT_CODED_SLICE_RASL_N);
      }
    }

    if (m_pcCfg->getEfficientFieldIRAPEnabled())
    {
      if ( pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA_W_LP
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA_W_RADL
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA_N_LP
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA )  // IRAP picture
      {
        m_associatedIRAPType = pcSlice->getNalUnitType();
        m_associatedIRAPPOC = pocCurr;
      }
      pcSlice->setAssociatedIRAPType(m_associatedIRAPType);
      pcSlice->setAssociatedIRAPPOC(m_associatedIRAPPOC);
    }
    // Do decoding refresh marking if any
    pcSlice->decodingRefreshMarking(m_pocCRA, m_bRefreshPending, rcListPic, m_pcCfg->getEfficientFieldIRAPEnabled());
    m_pcEncTop->selectReferencePictureSet(pcSlice, pocCurr, iGOPid);
    if (!m_pcCfg->getEfficientFieldIRAPEnabled())
    {
      if ( pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA_W_LP
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA_W_RADL
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA_N_LP
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP
        || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA )  // IRAP picture
      {
        m_associatedIRAPType = pcSlice->getNalUnitType();
        m_associatedIRAPPOC = pocCurr;
      }
      pcSlice->setAssociatedIRAPType(m_associatedIRAPType);
      pcSlice->setAssociatedIRAPPOC(m_associatedIRAPPOC);
    }

    if ((pcSlice->checkThatAllRefPicsAreAvailable(rcListPic, pcSlice->getRPS(), false, m_iLastRecoveryPicPOC, m_pcCfg->getDecodingRefreshType() == 3) != 0) || (pcSlice->isIRAP()) 
      || (m_pcCfg->getEfficientFieldIRAPEnabled() && isField && pcSlice->getAssociatedIRAPType() >= NAL_UNIT_CODED_SLICE_BLA_W_LP && pcSlice->getAssociatedIRAPType() <= NAL_UNIT_CODED_SLICE_CRA && pcSlice->getAssociatedIRAPPOC() == pcSlice->getPOC()+1)
      )
    {
      pcSlice->createExplicitReferencePictureSetFromReference(rcListPic, pcSlice->getRPS(), pcSlice->isIRAP(), m_iLastRecoveryPicPOC, m_pcCfg->getDecodingRefreshType() == 3, m_pcCfg->getEfficientFieldIRAPEnabled());
    }

    pcSlice->applyReferencePictureSet(rcListPic, pcSlice->getRPS());

    if(pcSlice->getTLayer() > 0 
      &&  !( pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RADL_N     // Check if not a leading picture
          || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RADL_R
          || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL_N
          || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL_R )
        )
    {
      if(pcSlice->isTemporalLayerSwitchingPoint(rcListPic) || pcSlice->getSPS()->getTemporalIdNestingFlag())
      {
        if(pcSlice->getTemporalLayerNonReferenceFlag())
        {
          pcSlice->setNalUnitType(NAL_UNIT_CODED_SLICE_TSA_N);
        }
        else
        {
          pcSlice->setNalUnitType(NAL_UNIT_CODED_SLICE_TSA_R);
        }
      }
      else if(pcSlice->isStepwiseTemporalLayerSwitchingPointCandidate(rcListPic))
      {
        Bool isSTSA=true;
        for(Int ii=iGOPid+1;(ii<m_pcCfg->getGOPSize() && isSTSA==true);ii++)
        {
          Int lTid= m_pcCfg->getGOPEntry(ii).m_temporalId;
          if(lTid==pcSlice->getTLayer())
          {
            const TComReferencePictureSet* nRPS = pcSlice->getSPS()->getRPSList()->getReferencePictureSet(ii);
            for(Int jj=0;jj<nRPS->getNumberOfPictures();jj++)
            {
              if(nRPS->getUsed(jj))
              {
                Int tPoc=m_pcCfg->getGOPEntry(ii).m_POC+nRPS->getDeltaPOC(jj);
                Int kk=0;
                for(kk=0;kk<m_pcCfg->getGOPSize();kk++)
                {
                  if(m_pcCfg->getGOPEntry(kk).m_POC==tPoc)
                  {
                    break;
                  }
                }
                Int tTid=m_pcCfg->getGOPEntry(kk).m_temporalId;
                if(tTid >= pcSlice->getTLayer())
                {
                  isSTSA=false;
                  break;
                }
              }
            }
          }
        }
        if(isSTSA==true)
        {
          if(pcSlice->getTemporalLayerNonReferenceFlag())
          {
            pcSlice->setNalUnitType(NAL_UNIT_CODED_SLICE_STSA_N);
          }
          else
          {
            pcSlice->setNalUnitType(NAL_UNIT_CODED_SLICE_STSA_R);
          }
        }
      }
    }
    arrangeLongtermPicturesInRPS(pcSlice, rcListPic);
    TComRefPicListModification* refPicListModification = pcSlice->getRefPicListModification();
    refPicListModification->setRefPicListModificationFlagL0(0);
    refPicListModification->setRefPicListModificationFlagL1(0);
    pcSlice->setNumRefIdx(REF_PIC_LIST_0,min(m_pcCfg->getGOPEntry(iGOPid).m_numRefPicsActive,pcSlice->getRPS()->getNumberOfPictures()));
    pcSlice->setNumRefIdx(REF_PIC_LIST_1,min(m_pcCfg->getGOPEntry(iGOPid).m_numRefPicsActive,pcSlice->getRPS()->getNumberOfPictures()));

    //  Set reference list
    pcSlice->setRefPicList ( rcListPic );

    //  Slice info. refinement
    if ( (pcSlice->getSliceType() == B_SLICE) && (pcSlice->getNumRefIdx(REF_PIC_LIST_1) == 0) )
    {
      pcSlice->setSliceType ( P_SLICE );
    }


    if (pcSlice->getPOC() > m_RASPOCforResetEncoder && m_pcCfg->getResetEncoderStateAfterIRAP())
    {
      // need to reset encoder decisions.
      m_pcSliceEncoder->resetEncoderDecisions();

      if (pcSlice->getSPS()->getUseSAO())
      {
        m_pcSAO->resetEncoderDecisions();
      }
      m_RASPOCforResetEncoder=MAX_INT;
    }
    if (pcSlice->isIRAP())
    {
      m_RASPOCforResetEncoder = pcSlice->getPOC();
    }

    pcSlice->setEncCABACTableIdx(m_pcSliceEncoder->getEncCABACTableIdx());
#if MCTS_EXTRACTION
    SliceType  encCABACTableIdx = pcSlice->getEncCABACTableIdx();
    Bool encCabacInitFlag = (pcSlice->getSliceType() != encCABACTableIdx && encCABACTableIdx != I_SLICE) ? true : false;
    pcSlice->setCabacInitFlag(encCabacInitFlag);
#endif

    if (pcSlice->getSliceType() == B_SLICE)
    {
      const UInt uiColFromL0 = calculateCollocatedFromL0Flag(pcSlice);
      pcSlice->setColFromL0Flag(uiColFromL0);
      Bool bLowDelay = true;
      Int  iCurrPOC  = pcSlice->getPOC();
      Int iRefIdx = 0;

      for (iRefIdx = 0; iRefIdx < pcSlice->getNumRefIdx(REF_PIC_LIST_0) && bLowDelay; iRefIdx++)
      {
        if ( pcSlice->getRefPic(REF_PIC_LIST_0, iRefIdx)->getPOC() > iCurrPOC )
        {
          bLowDelay = false;
        }
      }
      for (iRefIdx = 0; iRefIdx < pcSlice->getNumRefIdx(REF_PIC_LIST_1) && bLowDelay; iRefIdx++)
      {
        if ( pcSlice->getRefPic(REF_PIC_LIST_1, iRefIdx)->getPOC() > iCurrPOC )
        {
          bLowDelay = false;
        }
      }

      pcSlice->setCheckLDC(bLowDelay);
    }
    else
    {
      pcSlice->setCheckLDC(true);
    }


    //-------------------------------------------------------------
    pcSlice->setRefPOCList();

    pcSlice->setList1IdxToList0Idx();

    if (m_pcEncTop->getTMVPModeId() == 2)
    {
      if (iGOPid == 0) // first picture in SOP (i.e. forward B)
      {
        pcSlice->setEnableTMVPFlag(0);
      }
      else
      {
        // Note: pcSlice->getColFromL0Flag() is assumed to be always 0 and getcolRefIdx() is always 0.
        pcSlice->setEnableTMVPFlag(1);
      }
    }
    else if (m_pcEncTop->getTMVPModeId() == 1)
    {
      pcSlice->setEnableTMVPFlag(1);
    }
    else
    {
      pcSlice->setEnableTMVPFlag(0);
    }
    
    // set adaptive search range for non-intra-slices
    if (m_pcCfg->getUseASR() && pcSlice->getSliceType()!=I_SLICE)
    {
      m_pcSliceEncoder->setSearchRange(pcSlice);
    }

    Bool bGPBcheck=false;
    if ( pcSlice->getSliceType() == B_SLICE)
    {
      if ( pcSlice->getNumRefIdx(RefPicList( 0 ) ) == pcSlice->getNumRefIdx(RefPicList( 1 ) ) )
      {
        bGPBcheck=true;
        Int i;
        for ( i=0; i < pcSlice->getNumRefIdx(RefPicList( 1 ) ); i++ )
        {
          if ( pcSlice->getRefPOC(RefPicList(1), i) != pcSlice->getRefPOC(RefPicList(0), i) )
          {
            bGPBcheck=false;
            break;
          }
        }
      }
    }
    if(bGPBcheck)
    {
      pcSlice->setMvdL1ZeroFlag(true);
    }
    else
    {
      pcSlice->setMvdL1ZeroFlag(false);
    }


    Double lambda            = 0.0;
    Int actualHeadBits       = 0;
    Int actualTotalBits      = 0;
    Int estimatedBits        = 0;
    Int tmpBitsBeforeWriting = 0;
    if ( m_pcCfg->getUseRateCtrl() ) // TODO: does this work with multiple slices and slice-segments?
    {
      Int frameLevel = m_pcRateCtrl->getRCSeq()->getGOPID2Level( iGOPid );
      if ( pcPic->getSlice(0)->getSliceType() == I_SLICE )
      {
        frameLevel = 0;
      }
      m_pcRateCtrl->initRCPic( frameLevel );
      estimatedBits = m_pcRateCtrl->getRCPic()->getTargetBits();

      if (m_pcRateCtrl->getCpbSaturationEnabled() && frameLevel != 0)
      {
        Int estimatedCpbFullness = m_pcRateCtrl->getCpbState() + m_pcRateCtrl->getBufferingRate();

        // prevent overflow
        if (estimatedCpbFullness - estimatedBits > (Int)(m_pcRateCtrl->getCpbSize()*0.9f))
        {
          estimatedBits = estimatedCpbFullness - (Int)(m_pcRateCtrl->getCpbSize()*0.9f);
        }

        estimatedCpbFullness -= m_pcRateCtrl->getBufferingRate();
        // prevent underflow
        if (estimatedCpbFullness - estimatedBits < m_pcRateCtrl->getRCPic()->getLowerBound())
        {
          estimatedBits = max(200, estimatedCpbFullness - m_pcRateCtrl->getRCPic()->getLowerBound());
        }

        m_pcRateCtrl->getRCPic()->setTargetBits(estimatedBits);
      }

      Int sliceQP = m_pcCfg->getInitialQP();
      if ( ( pcSlice->getPOC() == 0 && m_pcCfg->getInitialQP() > 0 ) || ( frameLevel == 0 && m_pcCfg->getForceIntraQP() ) ) // QP is specified
      {
        Int    NumberBFrames = ( m_pcCfg->getGOPSize() - 1 );
        Double dLambda_scale = 1.0 - Clip3( 0.0, 0.5, 0.05*(Double)NumberBFrames );
        Double dQPFactor     = 0.57*dLambda_scale;
        Int    SHIFT_QP      = 12;
        Int    bitdepth_luma_qp_scale = 0;
        Double qp_temp = (Double) sliceQP + bitdepth_luma_qp_scale - SHIFT_QP;
        lambda = dQPFactor*pow( 2.0, qp_temp/3.0 );
      }
      else if ( frameLevel == 0 )   // intra case, but use the model
      {
        m_pcSliceEncoder->calCostSliceI(pcPic); // TODO: This only analyses the first slice segment - what about the others?

        if ( m_pcCfg->getIntraPeriod() != 1 )   // do not refine allocated bits for all intra case
        {
          Int bits = m_pcRateCtrl->getRCSeq()->getLeftAverageBits();
          bits = m_pcRateCtrl->getRCPic()->getRefineBitsForIntra( bits );

          if (m_pcRateCtrl->getCpbSaturationEnabled() )
          {
            Int estimatedCpbFullness = m_pcRateCtrl->getCpbState() + m_pcRateCtrl->getBufferingRate();

            // prevent overflow
            if (estimatedCpbFullness - bits > (Int)(m_pcRateCtrl->getCpbSize()*0.9f))
            {
              bits = estimatedCpbFullness - (Int)(m_pcRateCtrl->getCpbSize()*0.9f);
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

        list<TEncRCPic*> listPreviousPicture = m_pcRateCtrl->getPicList();
        m_pcRateCtrl->getRCPic()->getLCUInitTargetBits();
        lambda  = m_pcRateCtrl->getRCPic()->estimatePicLambda( listPreviousPicture, pcSlice->getSliceType());
        sliceQP = m_pcRateCtrl->getRCPic()->estimatePicQP( lambda, listPreviousPicture );
      }
      else    // normal case
      {
        list<TEncRCPic*> listPreviousPicture = m_pcRateCtrl->getPicList();
        lambda  = m_pcRateCtrl->getRCPic()->estimatePicLambda( listPreviousPicture, pcSlice->getSliceType());
        sliceQP = m_pcRateCtrl->getRCPic()->estimatePicQP( lambda, listPreviousPicture );
      }

      sliceQP = Clip3( -pcSlice->getSPS()->getQpBDOffset(CHANNEL_TYPE_LUMA), MAX_QP, sliceQP );
      m_pcRateCtrl->getRCPic()->setPicEstQP( sliceQP );

      m_pcSliceEncoder->resetQP( pcPic, sliceQP, lambda );
    }

    UInt uiNumSliceSegments = 1;

    // Allocate some coders, now the number of tiles are known.
    const Int numSubstreamsColumns = (pcSlice->getPPS()->getNumTileColumnsMinus1() + 1);
    const Int numSubstreamRows     = pcSlice->getPPS()->getEntropyCodingSyncEnabledFlag() ? pcPic->getFrameHeightInCtus() : (pcSlice->getPPS()->getNumTileRowsMinus1() + 1);
    const Int numSubstreams        = numSubstreamRows * numSubstreamsColumns;
    std::vector<TComOutputBitstream> substreamsOut(numSubstreams);

    // now compress (trial encode) the various slice segments (slices, and dependent slices)
    {
      const UInt numberOfCtusInFrame=pcPic->getPicSym()->getNumberOfCtusInFrame();
      pcSlice->setSliceCurStartCtuTsAddr( 0 );
      pcSlice->setSliceSegmentCurStartCtuTsAddr( 0 );

      for(UInt nextCtuTsAddr = 0; nextCtuTsAddr < numberOfCtusInFrame; )
      {
        m_pcSliceEncoder->precompressSlice( pcPic );
        m_pcSliceEncoder->compressSlice   ( pcPic, false, false );

        const UInt curSliceSegmentEnd = pcSlice->getSliceSegmentCurEndCtuTsAddr();
        if (curSliceSegmentEnd < numberOfCtusInFrame)
        {
          const Bool bNextSegmentIsDependentSlice=curSliceSegmentEnd<pcSlice->getSliceCurEndCtuTsAddr();
          const UInt sliceBits=pcSlice->getSliceBits();
          pcPic->allocateNewSlice();
          // prepare for next slice
          pcPic->setCurrSliceIdx                    ( uiNumSliceSegments );
          m_pcSliceEncoder->setSliceIdx             ( uiNumSliceSegments   );
          pcSlice = pcPic->getSlice                 ( uiNumSliceSegments   );
          assert(pcSlice->getPPS()!=0);
          pcSlice->copySliceInfo                    ( pcPic->getSlice(uiNumSliceSegments-1)  );
          pcSlice->setSliceIdx                      ( uiNumSliceSegments   );
          if (bNextSegmentIsDependentSlice)
          {
            pcSlice->setSliceBits(sliceBits);
          }
          else
          {
            pcSlice->setSliceCurStartCtuTsAddr      ( curSliceSegmentEnd );
            pcSlice->setSliceBits(0);
          }
          pcSlice->setDependentSliceSegmentFlag(bNextSegmentIsDependentSlice);
          pcSlice->setSliceSegmentCurStartCtuTsAddr ( curSliceSegmentEnd );
          // TODO: optimise cabac_init during compress slice to improve multi-slice operation
          // pcSlice->setEncCABACTableIdx(m_pcSliceEncoder->getEncCABACTableIdx());
          uiNumSliceSegments ++;
        }
        nextCtuTsAddr = curSliceSegmentEnd;
      }
    }

    duData.clear();
    pcSlice = pcPic->getSlice(0);

    // SAO parameter estimation using non-deblocked pixels for CTU bottom and right boundary areas
    if( pcSlice->getSPS()->getUseSAO() && m_pcCfg->getSaoCtuBoundary() )
    {
      m_pcSAO->getPreDBFStatistics(pcPic);
    }

    //-- Loop filter
    Bool bLFCrossTileBoundary = pcSlice->getPPS()->getLoopFilterAcrossTilesEnabledFlag();
    m_pcLoopFilter->setCfg(bLFCrossTileBoundary);
    if ( m_pcCfg->getDeblockingFilterMetric() )
    {
      if ( m_pcCfg->getDeblockingFilterMetric()==2 )
      {
        applyDeblockingFilterParameterSelection(pcPic, uiNumSliceSegments, iGOPid);
      }
      else
      {
        applyDeblockingFilterMetric(pcPic, uiNumSliceSegments);
      }
    }
    m_pcLoopFilter->loopFilterPic( pcPic );

#if JVET_X0048_X0103_FILM_GRAIN
    if (m_pcCfg->getFilmGrainAnalysisEnabled())
    {
      int  filteredFrame = m_pcCfg->getIntraPeriod() < 1 ? 2 * m_pcCfg->getFrameRate() : m_pcCfg->getIntraPeriod();
      bool ready_to_analyze = pcPic->getPOC() % filteredFrame ? false : true; // either it is mctf denoising or external source for film grain analysis. note: if mctf is used, it is different from mctf for encoding.
      if (ready_to_analyze)
      {
          m_FGAnalyser.initBufs(pcPic);
          m_FGAnalyser.estimate_grain(pcPic);
      }
    }
#endif

    /////////////////////////////////////////////////////////////////////////////////////////////////// File writing
    // Set entropy coder
    m_pcEntropyCoder->setEntropyCoder   ( m_pcCavlcCoder );

    // write various parameter sets
    //bool writePS = m_bSeqFirst || (m_pcCfg->getReWriteParamSetsFlag() && (pcPic->getSlice(0)->getSliceType() == I_SLICE));
    bool writePS = m_bSeqFirst || (m_pcCfg->getReWriteParamSetsFlag() && (pcSlice->isIRAP()));
    if (writePS)
    {
      m_pcEncTop->setParamSetChanged(pcSlice->getSPS()->getSPSId(), pcSlice->getPPS()->getPPSId());
    }
    actualTotalBits += xWriteParameterSets(accessUnit, pcSlice, writePS);

    if (writePS)
    {
      // create prefix SEI messages at the beginning of the sequence
      assert(leadingSeiMessages.empty());
#if MCTS_EXTRACTION
      xCreateIRAPLeadingSEIMessages(leadingSeiMessages, m_pcEncTop->getVPS(),  pcSlice->getSPS(), pcSlice->getPPS());
#else
      xCreateIRAPLeadingSEIMessages(leadingSeiMessages, pcSlice->getSPS(), pcSlice->getPPS());
#endif

      m_bSeqFirst = false;
    }
    if (m_pcCfg->getAccessUnitDelimiter())
    {
      xWriteAccessUnitDelimiter(accessUnit, pcSlice);
    }

    // reset presence of BP SEI indication
    m_bufferingPeriodSEIPresentInAU = false;
    // create prefix SEI associated with a picture
    xCreatePerPictureSEIMessages(iGOPid, leadingSeiMessages, nestedSeiMessages, pcSlice);

    /* use the main bitstream buffer for storing the marshalled picture */
    m_pcEntropyCoder->setBitstream(NULL);

    pcSlice = pcPic->getSlice(0);

    if (pcSlice->getSPS()->getUseSAO())
    {
      Bool sliceEnabled[MAX_NUM_COMPONENT];
      TComBitCounter tempBitCounter;
      tempBitCounter.resetBits();
      m_pcEncTop->getRDGoOnSbacCoder()->setBitstream(&tempBitCounter);
      m_pcSAO->initRDOCabacCoder(m_pcEncTop->getRDGoOnSbacCoder(), pcSlice);
      m_pcSAO->SAOProcess(pcPic, sliceEnabled, pcPic->getSlice(0)->getLambdas(),
                          m_pcCfg->getTestSAODisableAtPictureLevel(),
                          m_pcCfg->getSaoEncodingRate(),
                          m_pcCfg->getSaoEncodingRateChroma(),
                          m_pcCfg->getSaoCtuBoundary());
      m_pcSAO->PCMLFDisableProcess(pcPic);
      m_pcEncTop->getRDGoOnSbacCoder()->setBitstream(NULL);

      //assign SAO slice header
      for(Int s=0; s< uiNumSliceSegments; s++)
      {
        pcPic->getSlice(s)->setSaoEnabledFlag(CHANNEL_TYPE_LUMA, sliceEnabled[COMPONENT_Y]);
        assert(sliceEnabled[COMPONENT_Cb] == sliceEnabled[COMPONENT_Cr]);
        pcPic->getSlice(s)->setSaoEnabledFlag(CHANNEL_TYPE_CHROMA, sliceEnabled[COMPONENT_Cb]);
      }
    }

    // pcSlice is currently slice 0.
    std::size_t binCountsInNalUnits   = 0; // For implementation of cabac_zero_word stuffing (section 7.4.3.10)
    std::size_t numBytesInVclNalUnits = 0; // For implementation of cabac_zero_word stuffing (section 7.4.3.10)

    for( UInt sliceSegmentStartCtuTsAddr = 0, sliceIdxCount=0; sliceSegmentStartCtuTsAddr < pcPic->getPicSym()->getNumberOfCtusInFrame(); sliceIdxCount++, sliceSegmentStartCtuTsAddr=pcSlice->getSliceSegmentCurEndCtuTsAddr() )
    {
      pcSlice = pcPic->getSlice(sliceIdxCount);
      if(sliceIdxCount > 0 && pcSlice->getSliceType()!= I_SLICE)
      {
        pcSlice->checkColRefIdx(sliceIdxCount, pcPic);
      }
      pcPic->setCurrSliceIdx(sliceIdxCount);
      m_pcSliceEncoder->setSliceIdx(sliceIdxCount);

      pcSlice->setRPS(pcPic->getSlice(0)->getRPS());
      pcSlice->setRPSidx(pcPic->getSlice(0)->getRPSidx());

      for ( UInt ui = 0 ; ui < numSubstreams; ui++ )
      {
        substreamsOut[ui].clear();
      }

      m_pcEntropyCoder->setEntropyCoder   ( m_pcCavlcCoder );
      m_pcEntropyCoder->resetEntropy      ( pcSlice );
      /* start slice NALunit */
      OutputNALUnit nalu( pcSlice->getNalUnitType(), pcSlice->getTLayer() );
      m_pcEntropyCoder->setBitstream(&nalu.m_Bitstream);

      pcSlice->setNoRaslOutputFlag(false);
      if (pcSlice->isIRAP())
      {
        if (pcSlice->getNalUnitType() >= NAL_UNIT_CODED_SLICE_BLA_W_LP && pcSlice->getNalUnitType() <= NAL_UNIT_CODED_SLICE_IDR_N_LP)
        {
          pcSlice->setNoRaslOutputFlag(true);
        }
        //the inference for NoOutputPriorPicsFlag
        // KJS: This cannot happen at the encoder
        if (!m_bFirst && pcSlice->isIRAP() && pcSlice->getNoRaslOutputFlag())
        {
          if (pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA)
          {
            pcSlice->setNoOutputPriorPicsFlag(true);
          }
        }
      }

      pcSlice->setEncCABACTableIdx(m_pcSliceEncoder->getEncCABACTableIdx());
#if MCTS_EXTRACTION
      encCABACTableIdx = pcSlice->getEncCABACTableIdx();
      encCabacInitFlag = (pcSlice->getSliceType() != encCABACTableIdx && encCABACTableIdx != I_SLICE) ? true : false;
      pcSlice->setCabacInitFlag(encCabacInitFlag);
#endif
      tmpBitsBeforeWriting = m_pcEntropyCoder->getNumberOfWrittenBits();
      m_pcEntropyCoder->encodeSliceHeader(pcSlice);
      actualHeadBits += ( m_pcEntropyCoder->getNumberOfWrittenBits() - tmpBitsBeforeWriting );

      pcSlice->setFinalized(true);

      pcSlice->clearSubstreamSizes(  );
      {
        UInt numBinsCoded = 0;
        m_pcSliceEncoder->encodeSlice(pcPic, &(substreamsOut[0]), numBinsCoded);
        binCountsInNalUnits+=numBinsCoded;
      }

      {
        // Construct the final bitstream by concatenating substreams.
        // The final bitstream is either nalu.m_Bitstream or pcBitstreamRedirect;
        // Complete the slice header info.
        m_pcEntropyCoder->setEntropyCoder   ( m_pcCavlcCoder );
        m_pcEntropyCoder->setBitstream(&nalu.m_Bitstream);
        m_pcEntropyCoder->encodeTilesWPPEntryPoint( pcSlice );

        // Append substreams...
        TComOutputBitstream *pcOut = pcBitstreamRedirect;
        const Int numZeroSubstreamsAtStartOfSlice  = pcPic->getSubstreamForCtuAddr(pcSlice->getSliceSegmentCurStartCtuTsAddr(), false, pcSlice);
        const Int numSubstreamsToCode  = pcSlice->getNumberOfSubstreamSizes()+1;
        for ( UInt ui = 0 ; ui < numSubstreamsToCode; ui++ )
        {
          pcOut->addSubstream(&(substreamsOut[ui+numZeroSubstreamsAtStartOfSlice]));
        }
      }

      // If current NALU is the first NALU of slice (containing slice header) and more NALUs exist (due to multiple dependent slices) then buffer it.
      // If current NALU is the last NALU of slice and a NALU was buffered, then (a) Write current NALU (b) Update an write buffered NALU at approproate location in NALU list.
      Bool bNALUAlignedWrittenToList    = false; // used to ensure current NALU is not written more than once to the NALU list.
      xAttachSliceDataToNalUnit(nalu, pcBitstreamRedirect);
      accessUnit.push_back(new NALUnitEBSP(nalu));
      actualTotalBits += UInt(accessUnit.back()->m_nalUnitData.str().size()) * 8;
      numBytesInVclNalUnits += (std::size_t)(accessUnit.back()->m_nalUnitData.str().size());
      bNALUAlignedWrittenToList = true;

      if (!bNALUAlignedWrittenToList)
      {
        nalu.m_Bitstream.writeAlignZero();
        accessUnit.push_back(new NALUnitEBSP(nalu));
      }

      if( ( m_pcCfg->getPictureTimingSEIEnabled() || m_pcCfg->getDecodingUnitInfoSEIEnabled() ) &&
          ( pcSlice->getSPS()->getVuiParametersPresentFlag() ) &&
          ( ( pcSlice->getSPS()->getVuiParameters()->getHrdParameters()->getNalHrdParametersPresentFlag() )
         || ( pcSlice->getSPS()->getVuiParameters()->getHrdParameters()->getVclHrdParametersPresentFlag() ) ) &&
          ( pcSlice->getSPS()->getVuiParameters()->getHrdParameters()->getSubPicCpbParamsPresentFlag() ) )
      {
          UInt numNalus = 0;
        UInt numRBSPBytes = 0;
        for (AccessUnit::const_iterator it = accessUnit.begin(); it != accessUnit.end(); it++)
        {
          numRBSPBytes += UInt((*it)->m_nalUnitData.str().size());
          numNalus ++;
        }
        duData.push_back(DUData());
        duData.back().accumBitsDU = ( numRBSPBytes << 3 );
        duData.back().accumNalsDU = numNalus;
      }
    } // end iteration over slices

    // cabac_zero_words processing
    cabac_zero_word_padding(pcSlice, pcPic, binCountsInNalUnits, numBytesInVclNalUnits, accessUnit.back()->m_nalUnitData, m_pcCfg->getCabacZeroWordPaddingEnabled());

    pcPic->compressMotion();

    //-- For time output for each slice
    Double dEncTime = (Double)(clock()-iBeforeTime) / CLOCKS_PER_SEC;

    std::string digestStr;
    if (m_pcCfg->getDecodedPictureHashSEIType()!=HASHTYPE_NONE)
    {
      SEIDecodedPictureHash *decodedPictureHashSei = new SEIDecodedPictureHash();
      m_seiEncoder.initDecodedPictureHashSEI(decodedPictureHashSei, pcPic, digestStr, pcSlice->getSPS()->getBitDepths());
      trailingSeiMessages.push_back(decodedPictureHashSei);
    }

    m_pcCfg->setEncodedFlag(iGOPid, true);

    Double PSNR_Y;

    xCalculateAddPSNRs( isField, isTff, iGOPid, pcPic, accessUnit, rcListPic, dEncTime, ip_conversion, snr_conversion, outputLogCtrl, &PSNR_Y );
    
    // Only produce the Green Metadata SEI message with the last picture.
    if( m_pcCfg->getSEIGreenMetadataInfoSEIEnable() && pcSlice->getPOC() == ( m_pcCfg->getFramesToBeEncoded() - 1 )  )
    {
      SEIGreenMetadataInfo *seiGreenMetadataInfo = new SEIGreenMetadataInfo;
      m_seiEncoder.initSEIGreenMetadataInfo(seiGreenMetadataInfo, (UInt)(PSNR_Y * 100 + 0.5));
      trailingSeiMessages.push_back(seiGreenMetadataInfo);
    }
    
    xWriteTrailingSEIMessages(trailingSeiMessages, accessUnit, pcSlice->getTLayer(), pcSlice->getSPS());
    
    printHash(m_pcCfg->getDecodedPictureHashSEIType(), digestStr);

    if ( m_pcCfg->getUseRateCtrl() )
    {
      Double avgQP     = m_pcRateCtrl->getRCPic()->calAverageQP();
      Double avgLambda = m_pcRateCtrl->getRCPic()->calAverageLambda();
      if ( avgLambda < 0.0 )
      {
        avgLambda = lambda;
      }

      m_pcRateCtrl->getRCPic()->updateAfterPicture( actualHeadBits, actualTotalBits, avgQP, avgLambda, pcSlice->getSliceType());
      m_pcRateCtrl->getRCPic()->addToPictureLsit( m_pcRateCtrl->getPicList() );

      m_pcRateCtrl->getRCSeq()->updateAfterPic( actualTotalBits );
      if ( pcSlice->getSliceType() != I_SLICE )
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
        printf(" [CPB %6d bits]", m_pcRateCtrl->getCpbState());
      }
    }

    xCreatePictureTimingSEI(m_pcCfg->getEfficientFieldIRAPEnabled()?effFieldIRAPMap.GetIRAPGOPid():0, leadingSeiMessages, nestedSeiMessages, duInfoSeiMessages, pcSlice, isField, duData);
    if (m_pcCfg->getScalableNestingSEIEnabled())
    {
      xCreateScalableNestingSEI (leadingSeiMessages, nestedSeiMessages);
    }
    xWriteLeadingSEIMessages(leadingSeiMessages, duInfoSeiMessages, accessUnit, pcSlice->getTLayer(), pcSlice->getSPS(), duData);
    xWriteDuSEIMessages(duInfoSeiMessages, accessUnit, pcSlice->getTLayer(), pcSlice->getSPS(), duData);

    pcPic->getPicYuvRec()->copyToPic(pcPicYuvRecOut);

    pcPic->setReconMark   ( true );
    m_bFirst = false;
    m_iNumPicCoded++;
    m_totalCoded ++;
    /* logging: insert a newline at end of picture period */
    printf("\n");
    fflush(stdout);

    if (m_pcCfg->getEfficientFieldIRAPEnabled())
    {
      iGOPid=effFieldIRAPMap.restoreGOPid(iGOPid);
    }
#if REDUCED_ENCODER_MEMORY

    pcPic->releaseReconstructionIntermediateData();
    if (!isField) // don't release the source data for field-coding because the fields are dealt with in pairs. // TODO: release source data for interlace simulations.
    {
      pcPic->releaseEncoderSourceImageData();
    }

#endif
  } // iGOPid-loop

  delete pcBitstreamRedirect;

  assert ( (m_iNumPicCoded == iNumPicRcvd) );
}

Void TEncGOP::printOutSummary(UInt uiNumAllPicCoded, Bool isField, const TEncAnalyze::OutputLogControl &outputLogCtrl, const BitDepths &bitDepths)
{
  assert (uiNumAllPicCoded == m_gcAnalyzeAll.getNumPic());


  //--CFG_KDY
  const Int rateMultiplier=(isField?2:1);
  m_gcAnalyzeAll.setFrmRate( m_pcCfg->getFrameRate()*rateMultiplier / (Double)m_pcCfg->getTemporalSubsampleRatio());
  m_gcAnalyzeI.setFrmRate( m_pcCfg->getFrameRate()*rateMultiplier / (Double)m_pcCfg->getTemporalSubsampleRatio());
  m_gcAnalyzeP.setFrmRate( m_pcCfg->getFrameRate()*rateMultiplier / (Double)m_pcCfg->getTemporalSubsampleRatio());
  m_gcAnalyzeB.setFrmRate( m_pcCfg->getFrameRate()*rateMultiplier / (Double)m_pcCfg->getTemporalSubsampleRatio());
  const ChromaFormat chFmt = m_pcCfg->getChromaFormatIdc();

  //-- all
  printf( "\n\nSUMMARY --------------------------------------------------------\n" );
  m_gcAnalyzeAll.printOut('a', chFmt, outputLogCtrl, bitDepths);

  printf( "\n\nI Slices--------------------------------------------------------\n" );
  m_gcAnalyzeI.printOut('i', chFmt, outputLogCtrl, bitDepths);

  printf( "\n\nP Slices--------------------------------------------------------\n" );
  m_gcAnalyzeP.printOut('p', chFmt, outputLogCtrl, bitDepths);

  printf( "\n\nB Slices--------------------------------------------------------\n" );
  m_gcAnalyzeB.printOut('b', chFmt, outputLogCtrl, bitDepths);

  if (!m_pcCfg->getSummaryOutFilename().empty())
  {
    m_gcAnalyzeAll.printSummary(chFmt, outputLogCtrl, bitDepths, m_pcCfg->getSummaryOutFilename());
  }

  if (!m_pcCfg->getSummaryPicFilenameBase().empty())
  {
    m_gcAnalyzeI.printSummary(chFmt, outputLogCtrl, bitDepths, m_pcCfg->getSummaryPicFilenameBase()+"I.txt");
    m_gcAnalyzeP.printSummary(chFmt, outputLogCtrl, bitDepths, m_pcCfg->getSummaryPicFilenameBase()+"P.txt");
    m_gcAnalyzeB.printSummary(chFmt, outputLogCtrl, bitDepths, m_pcCfg->getSummaryPicFilenameBase()+"B.txt");
  }

  if(isField)
  {
    //-- interlaced summary
    m_gcAnalyzeAll_in.setFrmRate( m_pcCfg->getFrameRate() / (Double)m_pcCfg->getTemporalSubsampleRatio());
    m_gcAnalyzeAll_in.setBits(m_gcAnalyzeAll.getBits());
    // prior to the above statement, the interlace analyser does not contain the correct total number of bits.

    printf( "\n\nSUMMARY INTERLACED ---------------------------------------------\n" );
    m_gcAnalyzeAll_in.printOut('a', chFmt, outputLogCtrl, bitDepths);

    if (!m_pcCfg->getSummaryOutFilename().empty())
    {
      m_gcAnalyzeAll_in.printSummary(chFmt, outputLogCtrl, bitDepths, m_pcCfg->getSummaryOutFilename());
    }
  }

  printf("\nRVM: %.3lf\n" , xCalculateRVM());
}

Void TEncGOP::preLoopFilterPicAll( TComPic* pcPic, UInt64& ruiDist )
{
  Bool bCalcDist = false;
  m_pcLoopFilter->setCfg(m_pcCfg->getLFCrossTileBoundaryFlag());
  m_pcLoopFilter->loopFilterPic( pcPic );

  if (!bCalcDist)
  {
    ruiDist = xFindDistortionFrame(pcPic->getPicYuvOrg(), pcPic->getPicYuvRec(), pcPic->getPicSym()->getSPS().getBitDepths());
  }
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================


Void TEncGOP::xInitGOP( Int iPOCLast, Int iNumPicRcvd, Bool isField )
{
  assert( iNumPicRcvd > 0 );
  //  Exception for the first frames
  if ( ( isField && (iPOCLast == 0 || iPOCLast == 1) ) || (!isField  && (iPOCLast == 0))  )
  {
    m_iGopSize    = 1;
  }
  else
  {
    m_iGopSize    = m_pcCfg->getGOPSize();
  }
  assert (m_iGopSize > 0);

  return;
}


Void TEncGOP::xGetBuffer( TComList<TComPic*>&      rcListPic,
                         TComList<TComPicYuv*>&    rcListPicYuvRecOut,
                         Int                       iNumPicRcvd,
                         Int                       iTimeOffset,
                         TComPic*&                 rpcPic,
                         TComPicYuv*&              rpcPicYuvRecOut,
                         Int                       pocCurr,
                         Bool                      isField)
{
  Int i;
  //  Rec. output
  TComList<TComPicYuv*>::iterator     iterPicYuvRec = rcListPicYuvRecOut.end();

  if (isField && pocCurr > 1 && m_iGopSize!=1)
  {
    iTimeOffset--;
  }

  for ( i = 0; i < (iNumPicRcvd - iTimeOffset + 1); i++ )
  {
    iterPicYuvRec--;
  }

  rpcPicYuvRecOut = *(iterPicYuvRec);

  //  Current pic.
  TComList<TComPic*>::iterator        iterPic       = rcListPic.begin();
  while (iterPic != rcListPic.end())
  {
    rpcPic = *(iterPic);
    rpcPic->setCurrSliceIdx(0);
    if (rpcPic->getPOC() == pocCurr)
    {
      break;
    }
    iterPic++;
  }

  assert (rpcPic != NULL);
  assert (rpcPic->getPOC() == pocCurr);

  return;
}

UInt64 TEncGOP::xFindDistortionFrame (TComPicYuv* pcPic0, TComPicYuv* pcPic1, const BitDepths &bitDepths)
{
  UInt64  uiTotalDiff = 0;

  for(Int chan=0; chan<pcPic0 ->getNumberValidComponents(); chan++)
  {
    const ComponentID ch=ComponentID(chan);
    Pel*  pSrc0   = pcPic0 ->getAddr(ch);
    Pel*  pSrc1   = pcPic1 ->getAddr(ch);
    UInt  uiShift     = 2 * DISTORTION_PRECISION_ADJUSTMENT(bitDepths.recon[toChannelType(ch)]-8);

    const Int   iStride = pcPic0->getStride(ch);
    const Int   iWidth  = pcPic0->getWidth(ch);
    const Int   iHeight = pcPic0->getHeight(ch);

    for(Int y = 0; y < iHeight; y++ )
    {
      for(Int x = 0; x < iWidth; x++ )
      {
        Intermediate_Int iTemp = pSrc0[x] - pSrc1[x];
        uiTotalDiff += UInt64((iTemp*iTemp) >> uiShift);
      }
      pSrc0 += iStride;
      pSrc1 += iStride;
    }
  }

  return uiTotalDiff;
}

Void TEncGOP::xCalculateAddPSNRs( const Bool isField, const Bool isFieldTopFieldFirst, const Int iGOPid, TComPic* pcPic, const AccessUnit&accessUnit, TComList<TComPic*> &rcListPic, const Double dEncTime, const InputColourSpaceConversion ip_conversion, const InputColourSpaceConversion snr_conversion, const TEncAnalyze::OutputLogControl &outputLogCtrl, Double* PSNR_Y )
{
  xCalculateAddPSNR( pcPic, pcPic->getPicYuvRec(), accessUnit, dEncTime, ip_conversion, snr_conversion, outputLogCtrl, PSNR_Y );
  //In case of field coding, compute the interlaced PSNR for both fields
  if(isField)
  {
    Bool bothFieldsAreEncoded = false;
    Int correspondingFieldPOC = pcPic->getPOC();
    Int currentPicGOPPoc = m_pcCfg->getGOPEntry(iGOPid).m_POC;
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
      for(Int i = 0; i < m_iGopSize; i ++)
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
      TComList<TComPic*>::iterator   iterPic = rcListPic.begin();
      while ((*iterPic)->getPOC() != correspondingFieldPOC)
      {
        iterPic ++;
      }
      TComPic* correspondingFieldPic = *(iterPic);

      if( (pcPic->isTopField() && isFieldTopFieldFirst) || (!pcPic->isTopField() && !isFieldTopFieldFirst))
      {
        xCalculateInterlacedAddPSNR(pcPic, correspondingFieldPic, pcPic->getPicYuvRec(), correspondingFieldPic->getPicYuvRec(), snr_conversion, outputLogCtrl, PSNR_Y );
      }
      else
      {
        xCalculateInterlacedAddPSNR(correspondingFieldPic, pcPic, correspondingFieldPic->getPicYuvRec(), pcPic->getPicYuvRec(), snr_conversion, outputLogCtrl, PSNR_Y );
      }
    }
  }
}

Void TEncGOP::xCalculateAddPSNR( TComPic* pcPic, TComPicYuv* pcPicD, const AccessUnit& accessUnit, Double dEncTime, const InputColourSpaceConversion ip_conversion, const InputColourSpaceConversion snr_conversion, const TEncAnalyze::OutputLogControl &outputLogCtrl, Double* PSNR_Y )
{
  TEncAnalyze::ResultData result;

  // calculate colour space of reconstructed data

  TComPicYuv cscd;

  if (snr_conversion!=IPCOLOURSPACE_UNCHANGED)
  {
    cscd.createWithoutCUInfo(pcPicD->getWidth(COMPONENT_Y), pcPicD->getHeight(COMPONENT_Y), pcPicD->getChromaFormat() );
    TVideoIOYuv::ColourSpaceConvert(*pcPicD, cscd, snr_conversion, false);
  }
  TComPicYuv &picd=(snr_conversion==IPCOLOURSPACE_UNCHANGED)?*pcPicD : cscd;

  // calculate colour space of source data

  TComPicYuv tmpTrueOrgInInternalMap;
  const TComPicYuv *pOrgPicYuv=0;

  const bool bOrgDataHasBeenAdjusted=m_pcCfg->getGopBasedTemporalFilterEnabled();
  if (snr_conversion==IPCOLOURSPACE_UNCHANGED)
  {
    // Comparison in same colour space as internal.
    if (bOrgDataHasBeenAdjusted)
    {
      // have to map true original data into internal data format
      tmpTrueOrgInInternalMap.createWithoutCUInfo(pcPicD->getWidth(COMPONENT_Y), pcPicD->getHeight(COMPONENT_Y), pcPicD->getChromaFormat());
      TVideoIOYuv::ColourSpaceConvert(*pcPic->getPicYuvTrueOrg(), tmpTrueOrgInInternalMap, ip_conversion, true);
      pOrgPicYuv = &tmpTrueOrgInInternalMap;
    }
    else
    {
      // original data not corrupted, so just use original
      pOrgPicYuv = pcPic->getPicYuvOrg();
    }
  }
  else
  {
    // we are doing comparison in true original, which has not been corrupted.
    pOrgPicYuv = pcPic ->getPicYuvTrueOrg();
  }


  //===== calculate PSNR =====

  if (outputLogCtrl.printXPSNR && pcPicD->getChromaFormat() != CHROMA_400)
  {
    const Pel*   pOrg[MAX_NUM_COMPONENT];
    Double dWeightPel[MAX_NUM_COMPONENT];
    Int    iWeightSize[MAX_NUM_COMPONENT] = {1, 1, 1};
    const Pel*   pRec[MAX_NUM_COMPONENT];
    Int    iOrgStride[MAX_NUM_COMPONENT], iRecStride[MAX_NUM_COMPONENT];
    Int    iWidth[MAX_NUM_COMPONENT], iHeight[MAX_NUM_COMPONENT], iSize[MAX_NUM_COMPONENT];
    UInt64 uiSSDtemp[MAX_NUM_COMPONENT];
    UInt   uiShiftWidth[MAX_NUM_COMPONENT], uiShiftHeight[MAX_NUM_COMPONENT];
    Intermediate_Int iDiff;
    
    for(Int chan=0; chan<pcPicD->getNumberValidComponents(); chan++)
    {
      const ComponentID ch=ComponentID(chan);
      
      iOrgStride[ch]    = pOrgPicYuv->getStride(ch);
      iRecStride[ch]    = picd.getStride(ch);
      iWidth[ch]        = pcPicD->getWidth (ch) - (m_pcEncTop->getSourcePadding(0) >> pcPic->getComponentScaleX(ch));
      iHeight[ch]       = pcPicD->getHeight(ch) - ((m_pcEncTop->getSourcePadding(1) >> (pcPic->isField()?1:0)) >> pcPic->getComponentScaleY(ch));
      iSize[ch]         = iWidth[ch]*iHeight[ch];
      uiSSDtemp[ch]     = 0;
      uiShiftWidth[ch]  = (ch == COMPONENT_Y || pcPicD->getChromaFormat() == CHROMA_444) ? 0 : 1;
      uiShiftHeight[ch] = (ch == COMPONENT_Y || pcPicD->getChromaFormat() == CHROMA_444 || pcPicD->getChromaFormat() == CHROMA_422) ? 0 : 1;
      dWeightPel[ch]    = m_pcCfg->getXPSNRWeight(ch);
      pOrg[ch]          = pOrgPicYuv->getAddr(ch);
      pRec[ch]          = picd.getAddr(ch);
    }
    
    Double dSSDtemp=0;
    std::vector<Double> vecSSEChroma(iSize[COMPONENT_Cb], Double(0));
    for(Int y = 0, t= 0; y < iHeight[COMPONENT_Cb]; y++ )
    {
      for(Int x = 0; x < iWidth[COMPONENT_Cb]; x++, t++)
      {
        UInt64 uiSE_cb, uiSE_cr;
        iDiff = (Intermediate_Int)( (Intermediate_Int)pOrg[COMPONENT_Cb][x] - (Intermediate_Int)pRec[COMPONENT_Cb][x] );
        uiSE_cb = iDiff * iDiff;
        iDiff = (Intermediate_Int)( (Intermediate_Int)pOrg[COMPONENT_Cr][x] - (Intermediate_Int)pRec[COMPONENT_Cr][x] );
        uiSE_cr = iDiff * iDiff;
        uiSSDtemp[COMPONENT_Cb] += uiSE_cb;
        uiSSDtemp[COMPONENT_Cr] += uiSE_cr;
        vecSSEChroma[t] = dWeightPel[COMPONENT_Cb] * (Double) uiSE_cb + dWeightPel[COMPONENT_Cr] * (Double) uiSE_cr;
      }
      pOrg[COMPONENT_Cb]  += iOrgStride[COMPONENT_Cb];
      pRec[COMPONENT_Cb]  += iRecStride[COMPONENT_Cb];
      pOrg[COMPONENT_Cr]  += iOrgStride[COMPONENT_Cr];
      pRec[COMPONENT_Cr]  += iRecStride[COMPONENT_Cr];
    }
    
    for(Int y = 0; y < iHeight[COMPONENT_Y]; y++ )
    {
      UInt y_step_chroma = (y >> uiShiftHeight[COMPONENT_Cb]) * iWidth[COMPONENT_Cb];
      for(Int x = 0; x < iWidth[COMPONENT_Y]; x++)
      {
        UInt64 uiSE_y;
        UInt x_step_chroma = (x >> uiShiftWidth[COMPONENT_Cb]);
        iDiff   = (Intermediate_Int)( (Intermediate_Int)pOrg[COMPONENT_Y][x] - (Intermediate_Int)pRec[COMPONENT_Y][x] );
        uiSE_y  = iDiff * iDiff;
        uiSSDtemp[COMPONENT_Y] += uiSE_y;
        dSSDtemp += sqrt(dWeightPel[COMPONENT_Y] * (Double) uiSE_y + vecSSEChroma[y_step_chroma+x_step_chroma]);
      }
      pOrg[COMPONENT_Y]  += iOrgStride[COMPONENT_Y];
      pRec[COMPONENT_Y]  += iRecStride[COMPONENT_Y];
    }
    
    Double fWValue = 0;
    for( Int chan = 0; chan<pcPicD->getNumberValidComponents(); chan++)
    {
      const ComponentID ch=ComponentID(chan);
      const UInt maxval  = 255 << (pcPic->getPicSym()->getSPS().getBitDepth(toChannelType(ch)) - 8);
      const Double fRefValue = (Double) maxval * maxval * iSize[ch];
      result.psnr[ch]        = ( uiSSDtemp[ch] ? 10.0 * log10( fRefValue / (Double)uiSSDtemp[ch] ) : 999.99 );
      result.MSEyuvframe[ch]  = (Double)uiSSDtemp[ch]/(iSize[ch]);
      fWValue += (Double) iWeightSize[ch] * (Double) iSize[ch];
    }
    const Double maxval    = 255 << (pcPic->getPicSym()->getSPS().getBitDepth(toChannelType(COMPONENT_Y)) - 8);
    fWValue = Double( maxval * fWValue);
    result.xpsnr = dSSDtemp ? 20.0 * log10( fWValue / dSSDtemp) : 999.99;
  }
  else
  {
    for(Int chan=0; chan<pcPicD->getNumberValidComponents(); chan++)
    {
      const ComponentID ch=ComponentID(chan);
      const Pel*  pOrg       = pOrgPicYuv->getAddr(ch);
      const Int   iOrgStride = pOrgPicYuv->getStride(ch);
      Pel*  pRec             = picd.getAddr(ch);
      const Int   iRecStride = picd.getStride(ch);
      const Int   iWidth  = pcPicD->getWidth (ch) - (m_pcEncTop->getSourcePadding(0) >> pcPic->getComponentScaleX(ch));
      const Int   iHeight = pcPicD->getHeight(ch) - ((m_pcEncTop->getSourcePadding(1) >> (pcPic->isField()?1:0)) >> pcPic->getComponentScaleY(ch));

      Int   iSize   = iWidth*iHeight;

      UInt64 uiSSDtemp=0;
      for(Int y = 0; y < iHeight; y++ )
      {
        for(Int x = 0; x < iWidth; x++ )
        {
          Intermediate_Int iDiff = (Intermediate_Int)( pOrg[x] - pRec[x] );
          uiSSDtemp   += iDiff * iDiff;
        }
        pOrg += iOrgStride;
        pRec += iRecStride;
      }
      const Int maxval = 255 << (pcPic->getPicSym()->getSPS().getBitDepth(toChannelType(ch)) - 8);
      const Double fRefValue = (Double) maxval * maxval * iSize;
      result.psnr[ch]         = ( uiSSDtemp ? 10.0 * log10( fRefValue / (Double)uiSSDtemp ) : 999.99 );
      result.MSEyuvframe[ch]   = (Double)uiSSDtemp/(iSize);
    }
  }
#if EXTENSION_360_VIDEO
  m_ext360.calculatePSNRs(pcPic);
#endif

  //===== calculate MS-SSIM =====
  if (outputLogCtrl.printMSSSIM)
  {
    for(Int chan=0; chan<pcPicD->getNumberValidComponents(); chan++)
    {
      const ComponentID ch  = ComponentID(chan);
      const Pel*  pOrg      = pOrgPicYuv->getAddr(ch);
      const Int   orgStride = pOrgPicYuv->getStride(ch);
      const Pel*  pRec      = picd.getAddr(ch);
      const Int   recStride = picd.getStride(ch);
      const Int   width     = pcPicD->getWidth (ch) - (m_pcEncTop->getSourcePadding(0) >> pcPic->getComponentScaleX(ch));
      const Int   height    = pcPicD->getHeight(ch) - ((m_pcEncTop->getSourcePadding(1) >> (pcPic->isField()?1:0)) >> pcPic->getComponentScaleY(ch));
      const UInt  bitDepth  = pcPic->getPicSym()->getSPS().getBitDepth(toChannelType(ch));
 
      result.MSSSIM[ch] = xCalculateMSSSIM (pOrg, orgStride, pRec, recStride, width, height, bitDepth);
    }
  }

  /* calculate the size of the access unit, excluding:
   *  - SEI NAL units
   */
  UInt numRBSPBytes = 0;
  for (AccessUnit::const_iterator it = accessUnit.begin(); it != accessUnit.end(); it++)
  {
    UInt numRBSPBytes_nal = UInt((*it)->m_nalUnitData.str().size());
    if (m_pcCfg->getSummaryVerboseness() > 0)
    {
      printf("*** %6s numBytesInNALunit: %u\n", nalUnitTypeToString((*it)->m_nalUnitType), numRBSPBytes_nal);
    }
    if ((*it)->m_nalUnitType != NAL_UNIT_PREFIX_SEI && (*it)->m_nalUnitType != NAL_UNIT_SUFFIX_SEI)
    {
      numRBSPBytes += numRBSPBytes_nal;
      // add start code bytes (Annex B)
      if (it == accessUnit.begin() || (*it)->m_nalUnitType == NAL_UNIT_VPS || (*it)->m_nalUnitType == NAL_UNIT_SPS || (*it)->m_nalUnitType == NAL_UNIT_PPS)
      {
        numRBSPBytes += 4;
      }
      else
      {
        numRBSPBytes += 3;
      }
    }
  }

  UInt uibits = numRBSPBytes * 8;
  m_vRVM_RP.push_back( uibits );

  //===== add distortion metrics =====
  result.bits=(Double)uibits;
  m_gcAnalyzeAll.addResult (result);

#if EXTENSION_360_VIDEO
  m_ext360.addResult(m_gcAnalyzeAll);
#endif

  TComSlice*  pcSlice = pcPic->getSlice(0);
  if (pcSlice->isIntra())
  {
    m_gcAnalyzeI.addResult (result);
#if EXTENSION_360_VIDEO
    m_ext360.addResult(m_gcAnalyzeI);
#endif
    *PSNR_Y = result.psnr[COMPONENT_Y];
  }
  if (pcSlice->isInterP())
  {
    m_gcAnalyzeP.addResult (result);
#if EXTENSION_360_VIDEO
    m_ext360.addResult(m_gcAnalyzeP);
#endif
    *PSNR_Y = result.psnr[COMPONENT_Y];
  }
  if (pcSlice->isInterB())
  {
    m_gcAnalyzeB.addResult (result);
#if EXTENSION_360_VIDEO
    m_ext360.addResult(m_gcAnalyzeB);
#endif
    *PSNR_Y = result.psnr[COMPONENT_Y];
  }

  TChar c = (pcSlice->isIntra() ? 'I' : pcSlice->isInterP() ? 'P' : 'B');
  if (!pcSlice->isReferenced())
  {
    c += 32;
  }

#if ADAPTIVE_QP_SELECTION
  printf("POC %4d TId: %1d ( %c-SLICE, nQP %d QP %d ) %10d bits",
         pcSlice->getPOC(),
         pcSlice->getTLayer(),
         c,
         pcSlice->getSliceQpBase(),
         pcSlice->getSliceQp(),
         uibits );
#else
  printf("POC %4d TId: %1d ( %c-SLICE, QP %d ) %10d bits",
         pcSlice->getPOC()-pcSlice->getLastIDR(),
         pcSlice->getTLayer(),
         c,
         pcSlice->getSliceQp(),
         uibits );
#endif

  printf(" [Y %6.4lf dB    U %6.4lf dB    V %6.4lf dB]", result.psnr[COMPONENT_Y], result.psnr[COMPONENT_Cb], result.psnr[COMPONENT_Cr] );

  if (outputLogCtrl.printHexPerPOCPSNRs)
  {
    uint64_t xPsnr[MAX_NUM_COMPONENT];
    for (int i = 0; i < MAX_NUM_COMPONENT; i++)
    {
      copy(reinterpret_cast<uint8_t *>(&result.psnr[i]),
           reinterpret_cast<uint8_t *>(&result.psnr[i]) + sizeof(result.psnr[i]),
           reinterpret_cast<uint8_t *>(&xPsnr[i]));
    }
    printf(" [xY %16" PRIx64 " xU %16" PRIx64 " xv %16" PRIx64 "]", xPsnr[COMPONENT_Y], xPsnr[COMPONENT_Cb], xPsnr[COMPONENT_Cr]);
  }

  if (outputLogCtrl.printMSSSIM)
  {
    printf(" [MS-SSIM Y %1.6lf    U %1.6lf    V %1.6lf]", result.MSSSIM[COMPONENT_Y], result.MSSSIM[COMPONENT_Cb], result.MSSSIM[COMPONENT_Cr] );
  }  
  if (outputLogCtrl.printXPSNR)
  {
    printf(" [xPSNR %6.4lf dB]", result.xpsnr);
  }
  if (outputLogCtrl.printFrameMSE)
  {
    printf(" [Y MSE %6.4lf  U MSE %6.4lf  V MSE %6.4lf]", result.MSEyuvframe[COMPONENT_Y], result.MSEyuvframe[COMPONENT_Cb], result.MSEyuvframe[COMPONENT_Cr] );
  }
#if EXTENSION_360_VIDEO
  m_ext360.printPerPOCInfo();
#endif
  printf(" [ET %5.0f ]", dEncTime );

  // printf(" [WP %d]", pcSlice->getUseWeightedPrediction());

  for (Int iRefList = 0; iRefList < 2; iRefList++)
  {
    printf(" [L%d ", iRefList);
    for (Int iRefIndex = 0; iRefIndex < pcSlice->getNumRefIdx(RefPicList(iRefList)); iRefIndex++)
    {
      printf ("%d ", pcSlice->getRefPOC(RefPicList(iRefList), iRefIndex)-pcSlice->getLastIDR());
    }
    printf("]");
  }

  cscd.destroy();
}

Double TEncGOP::xCalculateMSSSIM (const Pel *pOrg, const Int orgStride, const Pel* pRec, const Int recStride, const Int width, const Int height, const UInt bitDepth)
{
  const Int MAX_MSSSIM_SCALE  = 5;
  const Int WEIGHTING_MID_TAP = 5;
  const Int WEIGHTING_SIZE    = WEIGHTING_MID_TAP*2+1;

  UInt maxScale;

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
  Double weights[WEIGHTING_SIZE][WEIGHTING_SIZE];
  {
    Double coeffSum=0.0;
    for(Int y=0; y<WEIGHTING_SIZE; y++)
    {
      for(Int x=0; x<WEIGHTING_SIZE; x++)
      {
        weights[y][x]=exp(-((y-WEIGHTING_MID_TAP)*(y-WEIGHTING_MID_TAP)+(x-WEIGHTING_MID_TAP)*(x-WEIGHTING_MID_TAP))/(WEIGHTING_MID_TAP-0.5));
        coeffSum +=weights[y][x];
      }
    }

    for(Int y=0; y<WEIGHTING_SIZE; y++)
    {
      for(Int x=0; x<WEIGHTING_SIZE; x++)
      {
        weights[y][x] /=coeffSum;
      }
    }
  }

  //Resolution based weights
  const Double exponentWeights[MAX_MSSSIM_SCALE][MAX_MSSSIM_SCALE] = {{1.0,    0,      0,      0,      0     },
                                                                      {0.1356, 0.8644, 0,      0,      0     },
                                                                      {0.0711, 0.4530, 0.4760, 0,      0     },
                                                                      {0.0517, 0.3295, 0.3462, 0.2726, 0     },
                                                                      {0.0448, 0.2856, 0.3001, 0.2363, 0.1333}};

  //Downsampling of data:
  std::vector<Double> original[MAX_MSSSIM_SCALE];
  std::vector<Double> recon[MAX_MSSSIM_SCALE];

  for(UInt scale=0; scale<maxScale; scale++)
  {
    const Int scaledHeight = height >> scale;
    const Int scaledWidth  = width  >> scale;
    original[scale].resize(scaledHeight*scaledWidth, Double(0));
    recon[scale].resize(scaledHeight*scaledWidth, Double(0));
  }

  // Initial [0] arrays to be a copy of the source data (but stored in array "Double", not Pel array).
  for(Int y=0; y<height; y++)
  {
    for(Int x=0; x<width; x++)
    {
      original[0][y*width+x] = pOrg[y*orgStride+x];
      recon[0][   y*width+x] = pRec[y*recStride+x];
    }
  }

  // Set up other arrays to be average value of each 2x2 sample.
  for(UInt scale=1; scale<maxScale; scale++)
  {
    const Int scaledHeight = height >> scale;
    const Int scaledWidth  = width  >> scale;
    for(Int y=0; y<scaledHeight; y++)
    {
      for(Int x=0; x<scaledWidth; x++)
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
  const UInt   maxValue  = (1<<bitDepth)-1;
  const Double c1        = (0.01*maxValue)*(0.01*maxValue);
  const Double c2        = (0.03*maxValue)*(0.03*maxValue);
  
  Double finalMSSSIM = 1.0;

  for(UInt scale=0; scale<maxScale; scale++)
  {
    const Int scaledHeight    = height >> scale;
    const Int scaledWidth     = width  >> scale;
    const Int blocksPerRow    = scaledWidth-WEIGHTING_SIZE+1;
    const Int blocksPerColumn = scaledHeight-WEIGHTING_SIZE+1;
    const Int totalBlocks     = blocksPerRow*blocksPerColumn;

    Double meanSSIM= 0.0;

    for(Int blockIndexY=0; blockIndexY<blocksPerColumn; blockIndexY++)
    {
      for(Int blockIndexX=0; blockIndexX<blocksPerRow; blockIndexX++)
      {
        Double muOrg          =0.0;
        Double muRec          =0.0;
        Double muOrigSqr      =0.0;
        Double muRecSqr       =0.0;
        Double muOrigMultRec  =0.0;

        for(Int y=0; y<WEIGHTING_SIZE; y++)
        {
          for(Int x=0;x<WEIGHTING_SIZE; x++)
          {
            const Double gaussianWeight=weights[y][x];
            const Int    sampleOffset=(blockIndexY+y)*scaledWidth+(blockIndexX+x);
            const Double orgPel=original[scale][sampleOffset];
            const Double recPel=   recon[scale][sampleOffset];

            muOrg        +=orgPel*       gaussianWeight;
            muRec        +=recPel*       gaussianWeight;
            muOrigSqr    +=orgPel*orgPel*gaussianWeight;
            muRecSqr     +=recPel*recPel*gaussianWeight;
            muOrigMultRec+=orgPel*recPel*gaussianWeight;
          }
        }

        const Double sigmaSqrOrig = muOrigSqr    -(muOrg*muOrg);
        const Double sigmaSqrRec  = muRecSqr     -(muRec*muRec);
        const Double sigmaOrigRec = muOrigMultRec-(muOrg*muRec);

        Double blockSSIMVal = ((2.0*sigmaOrigRec + c2)/(sigmaSqrOrig+sigmaSqrRec + c2));
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


Void TEncGOP::xCalculateInterlacedAddPSNR( TComPic* pcPicOrgFirstField, TComPic* pcPicOrgSecondField,
                                          TComPicYuv* pcPicRecFirstField, TComPicYuv* pcPicRecSecondField,
                                          const InputColourSpaceConversion conversion, const TEncAnalyze::OutputLogControl &outputLogCtrl, Double* PSNR_Y )
{
  TEncAnalyze::ResultData result;
  const TComSPS &sps=pcPicOrgFirstField->getPicSym()->getSPS();
  TComPic    *apcPicOrgFields[2]={pcPicOrgFirstField, pcPicOrgSecondField};
  TComPicYuv *apcPicRecFields[2]={pcPicRecFirstField, pcPicRecSecondField};

  TComPicYuv cscd[2 /* first/second field */];
  if (conversion!=IPCOLOURSPACE_UNCHANGED)
  {
    for(UInt fieldNum=0; fieldNum<2; fieldNum++)
    {
      TComPicYuv &reconField=*(apcPicRecFields[fieldNum]);
      cscd[fieldNum].createWithoutCUInfo(reconField.getWidth(COMPONENT_Y), reconField.getHeight(COMPONENT_Y), reconField.getChromaFormat() );
      TVideoIOYuv::ColourSpaceConvert(reconField, cscd[fieldNum], conversion, false);
      apcPicRecFields[fieldNum]=cscd+fieldNum;
    }
  }

  //===== calculate PSNR =====

  assert(apcPicRecFields[0]->getChromaFormat()==apcPicRecFields[1]->getChromaFormat());
  const UInt numValidComponents=apcPicRecFields[0]->getNumberValidComponents();
  const Bool   useTrueOrg = conversion != IPCOLOURSPACE_UNCHANGED || m_pcCfg->getGopBasedTemporalFilterEnabled();

  if (outputLogCtrl.printXPSNR && apcPicRecFields[0]->getChromaFormat() != CHROMA_400 && apcPicRecFields[1]->getChromaFormat() != CHROMA_400)
  {
    // For interlace images, we need to scan the two fields independently
    Pel*   pOrg[MAX_NUM_COMPONENT];
    Double dWeightPel[MAX_NUM_COMPONENT];
    Int    iWeightSize[MAX_NUM_COMPONENT] = {1, 1, 1};
    Pel*   pRec[MAX_NUM_COMPONENT];
    Int    iOrgStride[MAX_NUM_COMPONENT], iRecStride[MAX_NUM_COMPONENT];
    Int    iWidth[MAX_NUM_COMPONENT], iHeight[MAX_NUM_COMPONENT], iSize[MAX_NUM_COMPONENT];
    UInt64 uiSSDtemp[MAX_NUM_COMPONENT];
    UInt   uiShiftWidth[MAX_NUM_COMPONENT], uiShiftHeight[MAX_NUM_COMPONENT];
    Intermediate_Int iDiff;
    Double dSSDtemp = 0.0;
    Double fWValue  = 0.0;
    for(UInt fieldNum=0; fieldNum<2; fieldNum++)
    {
      TComPic *pcPic=apcPicOrgFields[fieldNum];
      TComPicYuv *pcPicD=apcPicRecFields[fieldNum];
      TComPicYuv *pOrgPicYuv = useTrueOrg ? pcPic ->getPicYuvTrueOrg() : pcPic ->getPicYuvOrg();

      for(Int chan=0; chan<pcPicD->getNumberValidComponents(); chan++)
      {
        const ComponentID ch=ComponentID(chan);
        iOrgStride[ch]    = pOrgPicYuv->getStride(ch);
        iRecStride[ch]    = pcPicD->getStride(ch);
        iWidth[ch]        = pcPicD->getWidth (ch) - (m_pcEncTop->getSourcePadding(0) >> pcPic->getComponentScaleX(ch));
        iHeight[ch]       = pcPicD->getHeight(ch) - ((m_pcEncTop->getSourcePadding(1) >> 1) >> pcPic->getComponentScaleY(ch));
        iSize[ch]         = iWidth[ch]*iHeight[ch];
        uiSSDtemp[ch]     = 0;
        uiShiftWidth[ch]  = (ch == COMPONENT_Y || pcPicD->getChromaFormat() == CHROMA_444) ? 0 : 1;
        uiShiftHeight[ch] = (ch == COMPONENT_Y || pcPicD->getChromaFormat() == CHROMA_444 || pcPicD->getChromaFormat() == CHROMA_422) ? 0 : 1;
        dWeightPel[ch]    = m_pcCfg->getXPSNRWeight(ch);
        pOrg[ch]          = pOrgPicYuv->getAddr(ch);
        pRec[ch]          = pcPicD->getAddr(ch);
      }
      std::vector<Double> vecSSEChroma(iSize[COMPONENT_Cb], Double(0));

      for(Int y = 0, t= 0; y < iHeight[COMPONENT_Cb]; y++ )
      {
        for(Int x = 0; x < iWidth[COMPONENT_Cb]; x++, t++)
        {
          UInt64 uiSE_cb, uiSE_cr;
          iDiff = (Intermediate_Int)( (Intermediate_Int)pOrg[COMPONENT_Cb][x] - (Intermediate_Int)pRec[COMPONENT_Cb][x] );
          uiSE_cb = iDiff * iDiff;
          iDiff = (Intermediate_Int)( (Intermediate_Int)pOrg[COMPONENT_Cr][x] - (Intermediate_Int)pRec[COMPONENT_Cr][x] );
          uiSE_cr = iDiff * iDiff;
          uiSSDtemp[COMPONENT_Cb] += uiSE_cb;
          uiSSDtemp[COMPONENT_Cr] += uiSE_cr;
          vecSSEChroma[t] = dWeightPel[COMPONENT_Cb] * (Double) uiSE_cb + dWeightPel[COMPONENT_Cr] * (Double) uiSE_cr;
        }
        pOrg[COMPONENT_Cb]  += iOrgStride[COMPONENT_Cb];
        pRec[COMPONENT_Cb]  += iRecStride[COMPONENT_Cb];
        pOrg[COMPONENT_Cr]  += iOrgStride[COMPONENT_Cr];
        pRec[COMPONENT_Cr]  += iRecStride[COMPONENT_Cr];
      }

      for(Int y = 0; y < iHeight[COMPONENT_Y]; y++ )
      {
        UInt y_step_chroma = (y >> uiShiftHeight[COMPONENT_Cb]) * iWidth[COMPONENT_Cb];
        for(Int x = 0; x < iWidth[COMPONENT_Y]; x++)
        {
          UInt64 uiSE_y;
          UInt x_step_chroma = (x >> uiShiftWidth[COMPONENT_Cb]);
          iDiff   = (Intermediate_Int)( (Intermediate_Int)pOrg[COMPONENT_Y][x] - (Intermediate_Int)pRec[COMPONENT_Y][x] );
          uiSE_y  = iDiff * iDiff;
          uiSSDtemp[COMPONENT_Y] += uiSE_y;
          dSSDtemp += sqrt(dWeightPel[COMPONENT_Y] * (Double) uiSE_y + vecSSEChroma[y_step_chroma+x_step_chroma]);
        }
        pOrg[COMPONENT_Y]  += iOrgStride[COMPONENT_Y];
        pRec[COMPONENT_Y]  += iRecStride[COMPONENT_Y];
      }

      for( Int chan = 0; chan<pcPicD->getNumberValidComponents(); chan++)
      {
        const ComponentID ch=ComponentID(chan);
        const UInt maxval  = 255 << (sps.getBitDepth(toChannelType(ch)) - 8);
        const Double fRefValue = (Double) maxval * maxval * iSize[ch];
        result.psnr[ch]        = ( uiSSDtemp[ch] ? 10.0 * log10( fRefValue / (Double)uiSSDtemp[ch] ) : 999.99 );
        result.MSEyuvframe[ch]  = (Double)uiSSDtemp[ch]/(iSize[ch]);
        fWValue += (Double) iWeightSize[ch] * (Double) iSize[ch];
      }
    }
    const Double maxval    = 255 << (sps.getBitDepth(toChannelType(COMPONENT_Y)) - 8);
    fWValue = Double( maxval * fWValue);
    result.xpsnr = dSSDtemp ? 20.0 * log10( fWValue / dSSDtemp) : 999.99;
  }
  else
  {
    for(Int chan=0; chan<numValidComponents; chan++)
    {
      const ComponentID ch=ComponentID(chan);
      assert(apcPicRecFields[0]->getWidth(ch)==apcPicRecFields[1]->getWidth(ch));
      assert(apcPicRecFields[0]->getHeight(ch)==apcPicRecFields[1]->getHeight(ch));

      UInt64 uiSSDtemp=0;
      const Int   iWidth  = apcPicRecFields[0]->getWidth (ch) - (m_pcEncTop->getSourcePadding(0) >> apcPicRecFields[0]->getComponentScaleX(ch));
      const Int   iHeight = apcPicRecFields[0]->getHeight(ch) - ((m_pcEncTop->getSourcePadding(1) >> 1) >> apcPicRecFields[0]->getComponentScaleY(ch));

      Int   iSize   = iWidth*iHeight;

      for(UInt fieldNum=0; fieldNum<2; fieldNum++)
      {
        TComPic *pcPic=apcPicOrgFields[fieldNum];
        TComPicYuv *pcPicD=apcPicRecFields[fieldNum];

        const Pel*  pOrg    = useTrueOrg ? pcPic ->getPicYuvTrueOrg()->getAddr(ch) : pcPic ->getPicYuvOrg()->getAddr(ch);
        Pel*  pRec    = pcPicD->getAddr(ch);
        const Int   iStride = pcPicD->getStride(ch);


        for(Int y = 0; y < iHeight; y++ )
        {
          for(Int x = 0; x < iWidth; x++ )
          {
            Intermediate_Int iDiff = (Intermediate_Int)( pOrg[x] - pRec[x] );
            uiSSDtemp   += iDiff * iDiff;
          }
          pOrg += iStride;
          pRec += iStride;
        }
      }
      const Int maxval = 255 << (sps.getBitDepth(toChannelType(ch)) - 8);
      const Double fRefValue = (Double) maxval * maxval * iSize*2;
      result.psnr[ch]         = ( uiSSDtemp ? 10.0 * log10( fRefValue / (Double)uiSSDtemp ) : 999.99 );
      result.MSEyuvframe[ch]   = (Double)uiSSDtemp/(iSize*2);
    }
  }
  //===== calculate MS-SSIM =====
  if (outputLogCtrl.printMSSSIM)
  {
    for(Int chan=0; chan<numValidComponents; chan++)
    {
      const ComponentID ch=ComponentID(chan);
      assert(apcPicRecFields[0]->getWidth(ch) ==apcPicRecFields[1]->getWidth(ch) );
      assert(apcPicRecFields[0]->getHeight(ch)==apcPicRecFields[1]->getHeight(ch));

      Double sumOverFieldsMSSSIM = 0.0;
      const Int   width  = apcPicRecFields[0]->getWidth (ch) - ( m_pcEncTop->getSourcePadding(0)       >> apcPicRecFields[0]->getComponentScaleX(ch));
      const Int   height = apcPicRecFields[0]->getHeight(ch) - ((m_pcEncTop->getSourcePadding(1) >> 1) >> apcPicRecFields[0]->getComponentScaleY(ch));

      for(UInt fieldNum=0; fieldNum<2; fieldNum++)
      {
        TComPic    *pcPic      = apcPicOrgFields[fieldNum];
        TComPicYuv *pcPicD     = apcPicRecFields[fieldNum];

        const Pel*  pOrg       = useTrueOrg ? pcPic ->getPicYuvTrueOrg()->getAddr(ch)   : pcPic ->getPicYuvOrg()->getAddr(ch);
        const Int   orgStride  = useTrueOrg ? pcPic ->getPicYuvTrueOrg()->getStride(ch) : pcPic ->getPicYuvOrg()->getStride(ch);
        Pel*        pRec       = pcPicD->getAddr(ch);
        const Int   recStride  = pcPicD->getStride(ch);
        const UInt  bitDepth   = sps.getBitDepth(toChannelType(ch));

        sumOverFieldsMSSSIM += xCalculateMSSSIM (pOrg, orgStride, pRec, recStride, width, height, bitDepth);
      }

      result.MSSSIM[ch] = sumOverFieldsMSSSIM/2;
    }
  }

  result.bits = 0; // the number of bits for the pair is not calculated here - instead the overall total is used elsewhere.

  //===== add PSNR =====
  m_gcAnalyzeAll_in.addResult (result);

  *PSNR_Y = result.psnr[COMPONENT_Y];

  printf("\n                                      Interlaced frame %d: [Y %6.4lf dB    U %6.4lf dB    V %6.4lf dB]", pcPicOrgSecondField->getPOC()/2 , result.psnr[COMPONENT_Y], result.psnr[COMPONENT_Cb], result.psnr[COMPONENT_Cr] );

  if (outputLogCtrl.printHexPerPOCPSNRs)
  {
    uint64_t xPsnr[MAX_NUM_COMPONENT];
    for (int i = 0; i < MAX_NUM_COMPONENT; i++)
    {
      copy(reinterpret_cast<uint8_t *>(&result.psnr[i]),
           reinterpret_cast<uint8_t *>(&result.psnr[i]) + sizeof(result.psnr[i]),
           reinterpret_cast<uint8_t *>(&xPsnr[i]));
    }
    printf(" [xY %16" PRIx64 " xU %16" PRIx64 " xv %16" PRIx64 "]", xPsnr[COMPONENT_Y], xPsnr[COMPONENT_Cb], xPsnr[COMPONENT_Cr]);
  }

  if (outputLogCtrl.printMSSSIM)
  {
    printf(" [MS-SSIM Y %1.6lf    U %1.6lf    V %1.6lf]", result.MSSSIM[COMPONENT_Y], result.MSSSIM[COMPONENT_Cb], result.MSSSIM[COMPONENT_Cr] );
  }
  if (outputLogCtrl.printFrameMSE)
  {
    printf(" [Y MSE %6.4lf  U MSE %6.4lf  V MSE %6.4lf]", result.MSEyuvframe[COMPONENT_Y], result.MSEyuvframe[COMPONENT_Cb], result.MSEyuvframe[COMPONENT_Cr] );
  }

  for(UInt fieldNum=0; fieldNum<2; fieldNum++)
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
NalUnitType TEncGOP::getNalUnitType(Int pocCurr, Int lastIDR, Bool isField)
{
  if (pocCurr == 0)
  {
    return NAL_UNIT_CODED_SLICE_IDR_W_RADL;
  }

  if(m_pcCfg->getEfficientFieldIRAPEnabled() && isField && pocCurr == 1)
  {
    // to avoid the picture becoming an IRAP
    return NAL_UNIT_CODED_SLICE_TRAIL_R;
  }

  if(m_pcCfg->getDecodingRefreshType() != 3 && (pocCurr - isField) % m_pcCfg->getIntraPeriod() == 0)
  {
    if (m_pcCfg->getDecodingRefreshType() == 1)
    {
      return NAL_UNIT_CODED_SLICE_CRA;
    }
    else if (m_pcCfg->getDecodingRefreshType() == 2)
    {
      return NAL_UNIT_CODED_SLICE_IDR_W_RADL;
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
      return NAL_UNIT_CODED_SLICE_RASL_R;
    }
  }
  if (lastIDR>0)
  {
    if (pocCurr < lastIDR)
    {
      return NAL_UNIT_CODED_SLICE_RADL_R;
    }
  }
  return NAL_UNIT_CODED_SLICE_TRAIL_R;
}

Double TEncGOP::xCalculateRVM()
{
  Double dRVM = 0;

  if( m_pcCfg->getGOPSize() == 1 && m_pcCfg->getIntraPeriod() != 1 && m_pcCfg->getFramesToBeEncoded() > RVM_VCEGAM10_M * 2 )
  {
    // calculate RVM only for lowdelay configurations
    std::vector<Double> vRL , vB;
    size_t N = m_vRVM_RP.size();
    vRL.resize( N );
    vB.resize( N );

    Int i;
    Double dRavg = 0 , dBavg = 0;
    vB[RVM_VCEGAM10_M] = 0;
    for( i = RVM_VCEGAM10_M + 1 ; i < N - RVM_VCEGAM10_M + 1 ; i++ )
    {
      vRL[i] = 0;
      for( Int j = i - RVM_VCEGAM10_M ; j <= i + RVM_VCEGAM10_M - 1 ; j++ )
      {
        vRL[i] += m_vRVM_RP[j];
      }
      vRL[i] /= ( 2 * RVM_VCEGAM10_M );
      vB[i] = vB[i-1] + m_vRVM_RP[i] - vRL[i];
      dRavg += m_vRVM_RP[i];
      dBavg += vB[i];
    }

    dRavg /= ( N - 2 * RVM_VCEGAM10_M );
    dBavg /= ( N - 2 * RVM_VCEGAM10_M );

    Double dSigamB = 0;
    for( i = RVM_VCEGAM10_M + 1 ; i < N - RVM_VCEGAM10_M + 1 ; i++ )
    {
      Double tmp = vB[i] - dBavg;
      dSigamB += tmp * tmp;
    }
    dSigamB = sqrt( dSigamB / ( N - 2 * RVM_VCEGAM10_M ) );

    Double f = sqrt( 12.0 * ( RVM_VCEGAM10_M - 1 ) / ( RVM_VCEGAM10_M + 1 ) );

    dRVM = dSigamB / dRavg * f;
  }

  return( dRVM );
}

/** Attaches the input bitstream to the stream in the output NAL unit
    Updates rNalu to contain concatenated bitstream. rpcBitstreamRedirect is cleared at the end of this function call.
 *  \param codedSliceData contains the coded slice data (bitstream) to be concatenated to rNalu
 *  \param rNalu          target NAL unit
 */
Void TEncGOP::xAttachSliceDataToNalUnit (OutputNALUnit& rNalu, TComOutputBitstream* codedSliceData)
{
  // Byte-align
  rNalu.m_Bitstream.writeByteAlignment();   // Slice header byte-alignment

  // Perform bitstream concatenation
  if (codedSliceData->getNumberOfWrittenBits() > 0)
  {
    rNalu.m_Bitstream.addSubstream(codedSliceData);
  }

  m_pcEntropyCoder->setBitstream(&rNalu.m_Bitstream);

  codedSliceData->clear();
}

// Function will arrange the long-term pictures in the decreasing order of poc_lsb_lt,
// and among the pictures with the same lsb, it arranges them in increasing delta_poc_msb_cycle_lt value
Void TEncGOP::arrangeLongtermPicturesInRPS(TComSlice *pcSlice, TComList<TComPic*>& rcListPic)
{
  if(pcSlice->getRPS()->getNumberOfLongtermPictures() == 0)
  {
    return;
  }
  // we can only modify the local RPS!
  assert (pcSlice->getRPSidx()==-1);
  TComReferencePictureSet *rps = pcSlice->getLocalRPS();

  // Arrange long-term reference pictures in the correct order of LSB and MSB,
  // and assign values for pocLSBLT and MSB present flag
  Int longtermPicsPoc[MAX_NUM_REF_PICS], longtermPicsLSB[MAX_NUM_REF_PICS], indices[MAX_NUM_REF_PICS];
  Int longtermPicsMSB[MAX_NUM_REF_PICS];
  Bool mSBPresentFlag[MAX_NUM_REF_PICS];
  ::memset(longtermPicsPoc, 0, sizeof(longtermPicsPoc));    // Store POC values of LTRP
  ::memset(longtermPicsLSB, 0, sizeof(longtermPicsLSB));    // Store POC LSB values of LTRP
  ::memset(longtermPicsMSB, 0, sizeof(longtermPicsMSB));    // Store POC LSB values of LTRP
  ::memset(indices        , 0, sizeof(indices));            // Indices to aid in tracking sorted LTRPs
  ::memset(mSBPresentFlag , 0, sizeof(mSBPresentFlag));     // Indicate if MSB needs to be present

  // Get the long-term reference pictures
  Int offset = rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures();
  Int i, ctr = 0;
  Int maxPicOrderCntLSB = 1 << pcSlice->getSPS()->getBitsForPOC();
  for(i = rps->getNumberOfPictures() - 1; i >= offset; i--, ctr++)
  {
    longtermPicsPoc[ctr] = rps->getPOC(i);                                  // LTRP POC
    longtermPicsLSB[ctr] = getLSB(longtermPicsPoc[ctr], maxPicOrderCntLSB); // LTRP POC LSB
    indices[ctr]      = i;
    longtermPicsMSB[ctr] = longtermPicsPoc[ctr] - longtermPicsLSB[ctr];
  }
  Int numLongPics = rps->getNumberOfLongtermPictures();
  assert(ctr == numLongPics);

  // Arrange pictures in decreasing order of MSB;
  for(i = 0; i < numLongPics; i++)
  {
    for(Int j = 0; j < numLongPics - 1; j++)
    {
      if(longtermPicsMSB[j] < longtermPicsMSB[j+1])
      {
        std::swap(longtermPicsPoc[j], longtermPicsPoc[j+1]);
        std::swap(longtermPicsLSB[j], longtermPicsLSB[j+1]);
        std::swap(longtermPicsMSB[j], longtermPicsMSB[j+1]);
        std::swap(indices[j]        , indices[j+1]        );
      }
    }
  }

  for(i = 0; i < numLongPics; i++)
  {
    // Check if MSB present flag should be enabled.
    // Check if the buffer contains any pictures that have the same LSB.
    TComList<TComPic*>::iterator  iterPic = rcListPic.begin();
    TComPic*                      pcPic;
    while ( iterPic != rcListPic.end() )
    {
      pcPic = *iterPic;
      if( (getLSB(pcPic->getPOC(), maxPicOrderCntLSB) == longtermPicsLSB[i])   &&     // Same LSB
                                      (pcPic->getSlice(0)->isReferenced())     &&    // Reference picture
                                        (pcPic->getPOC() != longtermPicsPoc[i])    )  // Not the LTRP itself
      {
        mSBPresentFlag[i] = true;
        break;
      }
      iterPic++;
    }
  }

  // tempArray for usedByCurr flag
  Bool tempArray[MAX_NUM_REF_PICS]; ::memset(tempArray, 0, sizeof(tempArray));
  for(i = 0; i < numLongPics; i++)
  {
    tempArray[i] = rps->getUsed(indices[i]);
  }
  // Now write the final values;
  ctr = 0;
  Int currMSB = 0, currLSB = 0;
  // currPicPoc = currMSB + currLSB
  currLSB = getLSB(pcSlice->getPOC(), maxPicOrderCntLSB);
  currMSB = pcSlice->getPOC() - currLSB;

  for(i = rps->getNumberOfPictures() - 1; i >= offset; i--, ctr++)
  {
    rps->setPOC                   (i, longtermPicsPoc[ctr]);
    rps->setDeltaPOC              (i, - pcSlice->getPOC() + longtermPicsPoc[ctr]);
    rps->setUsed                  (i, tempArray[ctr]);
    rps->setPocLSBLT              (i, longtermPicsLSB[ctr]);
    rps->setDeltaPocMSBCycleLT    (i, (currMSB - (longtermPicsPoc[ctr] - longtermPicsLSB[ctr])) / maxPicOrderCntLSB);
    rps->setDeltaPocMSBPresentFlag(i, mSBPresentFlag[ctr]);

    assert(rps->getDeltaPocMSBCycleLT(i) >= 0);   // Non-negative value
  }
  for(i = rps->getNumberOfPictures() - 1, ctr = 1; i >= offset; i--, ctr++)
  {
    for(Int j = rps->getNumberOfPictures() - 1 - ctr; j >= offset; j--)
    {
      // Here at the encoder we know that we have set the full POC value for the LTRPs, hence we
      // don't have to check the MSB present flag values for this constraint.
      assert( rps->getPOC(i) != rps->getPOC(j) ); // If assert fails, LTRP entry repeated in RPS!!!
    }
  }
}

Void TEncGOP::applyDeblockingFilterMetric( TComPic* pcPic, UInt uiNumSlices )
{
  TComPicYuv* pcPicYuvRec = pcPic->getPicYuvRec();
  Pel* Rec    = pcPicYuvRec->getAddr(COMPONENT_Y);
  Pel* tempRec = Rec;
  Int  stride = pcPicYuvRec->getStride(COMPONENT_Y);
  UInt log2maxTB = pcPic->getSlice(0)->getSPS()->getQuadtreeTULog2MaxSize();
  UInt maxTBsize = (1<<log2maxTB);
  const UInt minBlockArtSize = 8;
  const UInt picWidth = pcPicYuvRec->getWidth(COMPONENT_Y);
  const UInt picHeight = pcPicYuvRec->getHeight(COMPONENT_Y);
  const UInt noCol = (picWidth>>log2maxTB);
  const UInt noRows = (picHeight>>log2maxTB);
  assert(noCol > 1);
  assert(noRows > 1);
  std::vector<UInt64> colSAD(noCol,  UInt64(0));
  std::vector<UInt64> rowSAD(noRows, UInt64(0));
  UInt colIdx = 0;
  UInt rowIdx = 0;
  Pel p0, p1, p2, q0, q1, q2;

  Int qp = pcPic->getSlice(0)->getSliceQp();
  const Int bitDepthLuma=pcPic->getSlice(0)->getSPS()->getBitDepth(CHANNEL_TYPE_LUMA);
  Int bitdepthScale = 1 << (bitDepthLuma-8);
  Int beta = TComLoopFilter::getBeta( qp ) * bitdepthScale;
  const Int thr2 = (beta>>2);
  const Int thr1 = 2*bitdepthScale;
  UInt a = 0;

  if (maxTBsize > minBlockArtSize)
  {
    // Analyze vertical artifact edges
    for(Int c = maxTBsize; c < picWidth; c += maxTBsize)
    {
      for(Int r = 0; r < picHeight; r++)
      {
        p2 = Rec[c-3];
        p1 = Rec[c-2];
        p0 = Rec[c-1];
        q0 = Rec[c];
        q1 = Rec[c+1];
        q2 = Rec[c+2];
        a = ((abs(p2-(p1<<1)+p0)+abs(q0-(q1<<1)+q2))<<1);
        if ( thr1 < a && a < thr2)
        {
          colSAD[colIdx] += abs(p0 - q0);
        }
        Rec += stride;
      }
      colIdx++;
      Rec = tempRec;
    }

    // Analyze horizontal artifact edges
    for(Int r = maxTBsize; r < picHeight; r += maxTBsize)
    {
      for(Int c = 0; c < picWidth; c++)
      {
        p2 = Rec[c + (r-3)*stride];
        p1 = Rec[c + (r-2)*stride];
        p0 = Rec[c + (r-1)*stride];
        q0 = Rec[c + r*stride];
        q1 = Rec[c + (r+1)*stride];
        q2 = Rec[c + (r+2)*stride];
        a = ((abs(p2-(p1<<1)+p0)+abs(q0-(q1<<1)+q2))<<1);
        if (thr1 < a && a < thr2)
        {
          rowSAD[rowIdx] += abs(p0 - q0);
        }
      }
      rowIdx++;
    }
  }

  UInt64 colSADsum = 0;
  UInt64 rowSADsum = 0;
  for(Int c = 0; c < noCol-1; c++)
  {
    colSADsum += colSAD[c];
  }
  for(Int r = 0; r < noRows-1; r++)
  {
    rowSADsum += rowSAD[r];
  }

  colSADsum <<= 10;
  rowSADsum <<= 10;
  colSADsum /= (noCol-1);
  colSADsum /= picHeight;
  rowSADsum /= (noRows-1);
  rowSADsum /= picWidth;

  UInt64 avgSAD = ((colSADsum + rowSADsum)>>1);
  avgSAD >>= (bitDepthLuma-8);

  if ( avgSAD > 2048 )
  {
    avgSAD >>= 9;
    Int offset = Clip3(2,6,(Int)avgSAD);
    for (Int i=0; i<uiNumSlices; i++)
    {
      pcPic->getSlice(i)->setDeblockingFilterOverrideFlag(true);
      pcPic->getSlice(i)->setDeblockingFilterDisable(false);
      pcPic->getSlice(i)->setDeblockingFilterBetaOffsetDiv2( offset );
      pcPic->getSlice(i)->setDeblockingFilterTcOffsetDiv2( offset );
    }
  }
  else
  {
    for (Int i=0; i<uiNumSlices; i++)
    {
      pcPic->getSlice(i)->setDeblockingFilterOverrideFlag(false);
      pcPic->getSlice(i)->setDeblockingFilterDisable(        pcPic->getSlice(i)->getPPS()->getPPSDeblockingFilterDisabledFlag() );
      pcPic->getSlice(i)->setDeblockingFilterBetaOffsetDiv2( pcPic->getSlice(i)->getPPS()->getDeblockingFilterBetaOffsetDiv2() );
      pcPic->getSlice(i)->setDeblockingFilterTcOffsetDiv2(   pcPic->getSlice(i)->getPPS()->getDeblockingFilterTcOffsetDiv2()   );
    }
  }
}

Void TEncGOP::applyDeblockingFilterParameterSelection( TComPic* pcPic, const UInt numSlices, const Int gopID )
{
  enum DBFltParam
  {
    DBFLT_PARAM_AVAILABLE = 0,
    DBFLT_DISABLE_FLAG,
    DBFLT_BETA_OFFSETD2,
    DBFLT_TC_OFFSETD2,
    //NUM_DBFLT_PARAMS
  };
  const Int MAX_BETA_OFFSET = 3;
  const Int MIN_BETA_OFFSET = -3;
  const Int MAX_TC_OFFSET = 3;
  const Int MIN_TC_OFFSET = -3;

  TComPicYuv* pcPicYuvRec = pcPic->getPicYuvRec();
  TComPicYuv* pcPicYuvOrg = pcPic ->getPicYuvOrg();

  const Int currQualityLayer = (pcPic->getSlice(0)->getSliceType() != I_SLICE) ? m_pcCfg->getGOPEntry(gopID).m_temporalId+1 : 0;
  assert(currQualityLayer <MAX_ENCODER_DEBLOCKING_QUALITY_LAYERS);

  if(!m_pcDeblockingTempPicYuv)
  {
    m_pcDeblockingTempPicYuv         = new TComPicYuv;
    m_pcDeblockingTempPicYuv->create( m_pcEncTop->getSourceWidth(), m_pcEncTop->getSourceHeight(), m_pcEncTop->getChromaFormatIdc(),  pcPic->getSlice(0)->getSPS()->getMaxCUWidth(), pcPic->getSlice(0)->getSPS()->getMaxCUHeight(), pcPic->getSlice(0)->getSPS()->getMaxTotalCUDepth(),true );
    memset(m_DBParam, 0, sizeof(m_DBParam));
  }

  //preserve current reconstruction
  pcPicYuvRec->copyToPic(m_pcDeblockingTempPicYuv);

  const Bool bNoFiltering      = m_DBParam[currQualityLayer][DBFLT_PARAM_AVAILABLE] && m_DBParam[currQualityLayer][DBFLT_DISABLE_FLAG]==false /*&& pcPic->getTLayer()==0*/;
  const Int  maxBetaOffsetDiv2 = bNoFiltering? Clip3(MIN_BETA_OFFSET, MAX_BETA_OFFSET, m_DBParam[currQualityLayer][DBFLT_BETA_OFFSETD2]+1) : MAX_BETA_OFFSET;
  const Int  minBetaOffsetDiv2 = bNoFiltering? Clip3(MIN_BETA_OFFSET, MAX_BETA_OFFSET, m_DBParam[currQualityLayer][DBFLT_BETA_OFFSETD2]-1) : MIN_BETA_OFFSET;
  const Int  maxTcOffsetDiv2   = bNoFiltering? Clip3(MIN_TC_OFFSET, MAX_TC_OFFSET, m_DBParam[currQualityLayer][DBFLT_TC_OFFSETD2]+2)       : MAX_TC_OFFSET;
  const Int  minTcOffsetDiv2   = bNoFiltering? Clip3(MIN_TC_OFFSET, MAX_TC_OFFSET, m_DBParam[currQualityLayer][DBFLT_TC_OFFSETD2]-2)       : MIN_TC_OFFSET;

  UInt64 distBetaPrevious      = std::numeric_limits<UInt64>::max();
  UInt64 distMin               = std::numeric_limits<UInt64>::max();
  Bool   bDBFilterDisabledBest = true;
  Int    betaOffsetDiv2Best    = 0;
  Int    tcOffsetDiv2Best      = 0;

  for(Int betaOffsetDiv2=maxBetaOffsetDiv2; betaOffsetDiv2>=minBetaOffsetDiv2; betaOffsetDiv2--)
  {
    UInt64 distTcMin = std::numeric_limits<UInt64>::max();
    for(Int tcOffsetDiv2=maxTcOffsetDiv2; tcOffsetDiv2 >= minTcOffsetDiv2; tcOffsetDiv2--)
    {
      for (Int i=0; i<numSlices; i++)
      {
        pcPic->getSlice(i)->setDeblockingFilterOverrideFlag(true);
        pcPic->getSlice(i)->setDeblockingFilterDisable(false);
        pcPic->getSlice(i)->setDeblockingFilterBetaOffsetDiv2( betaOffsetDiv2 );
        pcPic->getSlice(i)->setDeblockingFilterTcOffsetDiv2( tcOffsetDiv2 );
      }
      m_pcDeblockingTempPicYuv->copyToPic(pcPicYuvRec); // restore reconstruction
      m_pcLoopFilter->loopFilterPic( pcPic );
      const UInt64 dist = xFindDistortionFrame(pcPicYuvOrg, pcPicYuvRec, pcPic->getPicSym()->getSPS().getBitDepths());
      if(dist < distMin)
      {
        distMin = dist;
        bDBFilterDisabledBest = false;
        betaOffsetDiv2Best  = betaOffsetDiv2;
        tcOffsetDiv2Best = tcOffsetDiv2;
      }
      if(dist < distTcMin)
      {
        distTcMin = dist;
      }
      else if(tcOffsetDiv2 <-2)
      {
        break;
      }
    }
    if(betaOffsetDiv2<-1 && distTcMin >= distBetaPrevious)
    {
      break;
    }
    distBetaPrevious = distTcMin;
  }

  //update:
  m_DBParam[currQualityLayer][DBFLT_PARAM_AVAILABLE] = 1;
  m_DBParam[currQualityLayer][DBFLT_DISABLE_FLAG]    = bDBFilterDisabledBest;
  m_DBParam[currQualityLayer][DBFLT_BETA_OFFSETD2]   = betaOffsetDiv2Best;
  m_DBParam[currQualityLayer][DBFLT_TC_OFFSETD2]     = tcOffsetDiv2Best;

  m_pcDeblockingTempPicYuv->copyToPic(pcPicYuvRec); //restore reconstruction

  if(bDBFilterDisabledBest)
  {
    for (Int i=0; i<numSlices; i++)
    {
      pcPic->getSlice(i)->setDeblockingFilterOverrideFlag(true);
      pcPic->getSlice(i)->setDeblockingFilterDisable(true);
    }
  }
  else if(betaOffsetDiv2Best ==pcPic->getSlice(0)->getPPS()->getDeblockingFilterBetaOffsetDiv2() &&  tcOffsetDiv2Best==pcPic->getSlice(0)->getPPS()->getDeblockingFilterTcOffsetDiv2())
  {
    for (Int i=0; i<numSlices; i++)
    {
      pcPic->getSlice(i)->setDeblockingFilterOverrideFlag(false);
      pcPic->getSlice(i)->setDeblockingFilterDisable(        pcPic->getSlice(i)->getPPS()->getPPSDeblockingFilterDisabledFlag() );
      pcPic->getSlice(i)->setDeblockingFilterBetaOffsetDiv2( pcPic->getSlice(i)->getPPS()->getDeblockingFilterBetaOffsetDiv2() );
      pcPic->getSlice(i)->setDeblockingFilterTcOffsetDiv2(   pcPic->getSlice(i)->getPPS()->getDeblockingFilterTcOffsetDiv2()   );
    }
  }
  else
  {
    for (Int i=0; i<numSlices; i++)
    {
      pcPic->getSlice(i)->setDeblockingFilterOverrideFlag(true);
      pcPic->getSlice(i)->setDeblockingFilterDisable( false );
      pcPic->getSlice(i)->setDeblockingFilterBetaOffsetDiv2(betaOffsetDiv2Best);
      pcPic->getSlice(i)->setDeblockingFilterTcOffsetDiv2(tcOffsetDiv2Best);
    }
  }
}

//! \}
