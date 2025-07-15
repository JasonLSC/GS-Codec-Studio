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

#pragma once

#ifndef __SEIENCODER__
#define __SEIENCODER__

#include "TLibCommon/SEI.h"

// forward declarations
class TEncCfg;
class TEncTop;
class TEncGOP;


//! Initializes different SEI message types based on given encoder configuration parameters 
class SEIEncoder
{
public:
  SEIEncoder()
    :m_pcCfg(NULL)
    ,m_pcEncTop(NULL)
    ,m_pcEncGOP(NULL)
    ,m_tl0Idx(0)
    ,m_rapIdx(0)
    ,m_isInitialized(false)
  {};
  virtual ~SEIEncoder(){};

  Void init(TEncCfg* encCfg, TEncTop *encTop, TEncGOP *encGOP) 
  { 
    m_pcCfg = encCfg;
    m_pcEncGOP = encGOP;
    m_pcEncTop = encTop;
    m_isInitialized = true;
  };

  // leading SEIs
  Void initSEIActiveParameterSets (SEIActiveParameterSets *sei, const TComVPS *vps, const TComSPS *sps);
  Void initSEIFramePacking(SEIFramePacking *sei, Int currPicNum);
  Void initSEIDisplayOrientation(SEIDisplayOrientation *sei);
  Void initSEIToneMappingInfo(SEIToneMappingInfo *sei);
  Void initSEISOPDescription(SEISOPDescription *sei, TComSlice *slice, Int picInGOP, Int lastIdr, Int currGOPSize);
  Void initSEIBufferingPeriod(SEIBufferingPeriod *sei, TComSlice *slice);
  Void initSEIScalableNesting(SEIScalableNesting *sei, SEIMessages &nestedSEIs);
  Void initSEIRecoveryPoint(SEIRecoveryPoint *sei, TComSlice *slice);
  Void initSEISegmentedRectFramePacking(SEISegmentedRectFramePacking *sei);
  Void initSEITempMotionConstrainedTileSets (SEITempMotionConstrainedTileSets *sei, const TComPPS *pps);
#if MCTS_EXTRACTION
  Void initSEIMCTSExtractionInfo(SEIMCTSExtractionInfoSet *sei, const TComVPS *vps, const TComSPS *sps, const TComPPS *pps);
#endif
  Void initSEIKneeFunctionInfo(SEIKneeFunctionInfo *sei);
  Void initSEIContentColourVolume(SEIContentColourVolume *sei);
#if SHUTTER_INTERVAL_SEI_MESSAGE
  Void initSEIShutterIntervalInfo(SEIShutterIntervalInfo *sei);
#endif
#if SEI_ENCODER_CONTROL
  Void initSEIFilmGrainCharacteristics(SEIFilmGrainCharacteristics *sei);
  Void initSEIContentLightLevel(SEIContentLightLevelInfo *sei);
  Void initSEIAmbientViewingEnvironment(SEIAmbientViewingEnvironment *sei);
#endif
  Void initSEIErp(SEIEquirectangularProjection *sei);
  Void initSEISphereRotation(SEISphereRotation *sei);
  Void initSEIOmniViewport(SEIOmniViewport *sei);
  Void initSEICubemapProjection(SEICubemapProjection *sei);
  Void initSEIRegionWisePacking(SEIRegionWisePacking *sei);
  Void initSEIFisheyeVideoInfo(SEIFisheyeVideoInfo *sei);
  Void initSEIChromaResamplingFilterHint(SEIChromaResamplingFilterHint *sei, Int iHorFilterIndex, Int iVerFilterIndex);
  Void initSEITimeCode(SEITimeCode *sei);
  Bool initSEIColourRemappingInfo(SEIColourRemappingInfo *sei, Int currPOC); // returns true on success, false on failure.
  Void initSEIAlternativeTransferCharacteristics(SEIAlternativeTransferCharacteristics *sei);
  Void readToneMappingInfoSEI(std::istream &fic, SEIToneMappingInfo *seiToneMappingInfo , Bool &failed );
  Void readChromaResamplingFilterHintSEI(std::istream &fic, SEIChromaResamplingFilterHint *seiChromaResamplingFilterHint, Bool &failed );
  Void readKneeFunctionInfoSEI(std::istream &fic, SEIKneeFunctionInfo *seiKneeFunctionInfo, Bool &failed );
  Void readColourRemapSEI(std::istream &fic, SEIColourRemappingInfo *seiColorRemappingInfo, Bool &failed );
  Void readContentColourVolumeSEI(std::istream &fic, SEIContentColourVolume *seiContentColourVolume, Bool &failed );
  Bool initSEIRegionalNesting(SEIRegionalNesting *sei, Int currPOC); // returns true on success, false on failure.
  Void readRNSEIWindow(std::istream &fic, RNSEIWindowVec::iterator regionIter, Bool &failed );
  Bool initSEIAnnotatedRegions(SEIAnnotatedRegions *sei, Int currPOC);
  Void readAnnotatedRegionSEI(std::istream &fic, SEIAnnotatedRegions *seiAnnoRegion, Bool &failed);
#if JCTVC_AD0021_SEI_MANIFEST
  Void initSEISEIManifest(SEIManifest* seiSeiManifest, const SEIMessages& seiMessage);
#endif
#if JCTVC_AD0021_SEI_PREFIX_INDICATION
  Void initSEISEIPrefixIndication(SEIPrefixIndication* seiSeiPrefixIndications, const SEI* sei);
#endif
  // trailing SEIs
  Void initDecodedPictureHashSEI(SEIDecodedPictureHash *sei, TComPic *pcPic, std::string &rHashString, const BitDepths &bitDepths);
  Void initTemporalLevel0IndexSEI(SEITemporalLevel0Index *sei, TComSlice *slice);
  Void initSEIGreenMetadataInfo(SEIGreenMetadataInfo *sei, UInt u);

private:
  TEncCfg* m_pcCfg;
  TEncTop* m_pcEncTop;
  TEncGOP* m_pcEncGOP;

  // for temporal level 0 index SEI
  UInt m_tl0Idx;
  UInt m_rapIdx;

  Bool m_isInitialized;
};


//! \}

#endif // __SEIENCODER__
