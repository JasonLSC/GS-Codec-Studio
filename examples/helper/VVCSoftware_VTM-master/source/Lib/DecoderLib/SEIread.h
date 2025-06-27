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
 \file     SEIread.h
 \brief    reading funtionality for SEI messages
 */

#ifndef __SEIREAD__
#define __SEIREAD__

#pragma once

#include <fstream>
//! \ingroup DecoderLib
//! \{

#include "CommonLib/SEI.h"

#include "CommonLib/SEIDigitallySignedContent.h"

class InputBitstream;

class SEIReader: public VLCReader
{
public:
  SEIReader() {};
  virtual ~SEIReader() {};
  SEIMessages::iterator parseSEImessage(InputBitstream* bs, SEIMessages& seis, const NalUnitType nalUnitType, const uint32_t nuh_layer_id, const uint32_t temporalId,const VPS *vps, const SPS *sps, HRD &hrd, std::ostream *pDecodedMessageOutputStream);
  void parseAndExtractSEIScalableNesting(InputBitstream *bs, const NalUnitType nalUnitType, const uint32_t nuh_layer_id,
                                         const VPS *vps, const SPS *sps, HRD &hrd, uint32_t payloadSize,
                                         std::vector<SeiPayload> *seiList);
  void getSEIDecodingUnitInfoDuiIdx(InputBitstream* bs, const uint32_t nuh_layer_id, HRD& hrd, uint32_t payloadSize,
                                    int& duiIdx);
  bool nnpfcProcessed;
  std::vector<int> nnpfcValues;
  
protected:
  bool xCheckNnpfcSeiMsg                      (uint32_t seiId,                        bool baseFlag,                            const std::vector<int> nnpfcValueList);
  bool xCheckNnpfcUpdatePresentSeiMsg         (uint32_t seiId,                        const std::vector<int> nnpfcValueList);
  bool xReadSEImessage                        (SEIMessages& seis, const NalUnitType nalUnitType, const uint32_t nuh_layer_id, const uint32_t temporalId, const VPS *vps, const SPS *sps, HRD &hrd, std::ostream *pDecodedMessageOutputStream);
  void xParseSEIFillerPayload                 (SEIFillerPayload &sei,                 uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIuserDataUnregistered(SEIUserDataUnregistered& sei, uint32_t payloadSize,
                                     std::ostream* pDecodedMessageOutputStream);
  void xParseSEIDecodingUnitInfo(SEIDecodingUnitInfo& dui, uint32_t payloadSize, const SEIBufferingPeriod& bp,
                                 const uint32_t temporalId, std::ostream* pDecodedMessageOutputStream);
  void xParseSEIDecodedPictureHash            (SEIDecodedPictureHash& sei,            uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIBufferingPeriod(SEIBufferingPeriod& bp, uint32_t payloadSize,
                                std::ostream* pDecodedMessageOutputStream);
  void xParseSEIPictureTiming(SEIPictureTiming& pt, uint32_t payloadSize, const uint32_t temporalId,
                              const SEIBufferingPeriod& bp, std::ostream* pDecodedMessageOutputStream);
  void xParseSEIScalableNesting(SEIScalableNesting& sn, const NalUnitType nalUnitType, const uint32_t nuhLayerId,
                                uint32_t payloadSize, const VPS* vps, const SPS* sps, HRD& hrd,
                                std::ostream* decodedMessageOutputStream, std::vector<SeiPayload>* seiList);
  void xCheckScalableNestingConstraints(const SEIScalableNesting& sn, const NalUnitType nalUnitType,
                                        const GeneralHrdParams* generalHrd);
  void xParseSEIFrameFieldinfo                (SEIFrameFieldInfo& sei,                uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
  void xParseSEIGreenMetadataInfo             (SEIGreenMetadataInfo& sei,             uint32_t payLoadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIDependentRAPIndication        (SEIDependentRAPIndication& sei,        uint32_t payLoadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIFramePacking                  (SEIFramePacking& sei,                  uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIDisplayOrientation            (SEIDisplayOrientation& sei,            uint32_t payloadSize,                     std::ostream* pDecodedMessageOutputStream);
  void xParseSEIParameterSetsInclusionIndication(SEIParameterSetsInclusionIndication& sei, uint32_t payloadSize,                std::ostream* pDecodedMessageOutputStream);
  void xParseSEIMasteringDisplayColourVolume  (SEIMasteringDisplayColourVolume& sei,  uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIAnnotatedRegions              (SEIAnnotatedRegions& sei,              uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIObjectMaskInfos               (SEIObjectMaskInfos& sei,               uint32_t payloadSize,                     std::ostream* pDecodedMessageOutputStream);
  void xParseSEIAlternativeTransferCharacteristics(SEIAlternativeTransferCharacteristics& sei,              uint32_t payLoadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIEncoderOptimizationInfo(SEIEncoderOptimizationInfo& sei, uint32_t payloadSize, std::ostream* pDecodedMessageOutputStream);
  void xParseSEIModalityInfo                  (SEIModalityInfo& sei,                  uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIEquirectangularProjection     (SEIEquirectangularProjection &sei,     uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEISphereRotation                (SEISphereRotation &sei,                uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIOmniViewport                  (SEIOmniViewport& sei,                  uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIRegionWisePacking             (SEIRegionWisePacking& sei,             uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIGeneralizedCubemapProjection  (SEIGeneralizedCubemapProjection &sei,  uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIScalabilityDimensionInfo      (SEIScalabilityDimensionInfo& sei,      uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
  void xParseSEIMultiviewAcquisitionInfo      (SEIMultiviewAcquisitionInfo& sei,      uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
  void xParseSEIMultiviewViewPosition         (SEIMultiviewViewPosition& sei,         uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
  void xParseSEIAlphaChannelInfo              (SEIAlphaChannelInfo& sei,              uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIDepthRepresentationInfo       (SEIDepthRepresentationInfo& sei,       uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
  void xParseSEIDepthRepInfoElement           (double &f,std::ostream *pDecodedMessageOutputStream);
  void xParseSEISubpictureLevelInfo(SEISubpictureLevelInfo& sli, uint32_t payloadSize,
                                    std::ostream* pDecodedMessageOutputStream);
  void xParseSEISampleAspectRatioInfo         (SEISampleAspectRatioInfo& sei,         uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIUserDataRegistered            (SEIUserDataRegistered& sei,            uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIFilmGrainCharacteristics      (SEIFilmGrainCharacteristics& sei,      uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIContentLightLevelInfo         (SEIContentLightLevelInfo& sei,         uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIAmbientViewingEnvironment     (SEIAmbientViewingEnvironment& sei,     uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIContentColourVolume           (SEIContentColourVolume& sei,           uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIExtendedDrapIndication        (SEIExtendedDrapIndication& sei,        uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIColourTransformInfo           (SEIColourTransformInfo& sei, uint32_t payloadSize, std::ostream* pDecodedMessageOutputStream);
  void xParseSEISEIManifest                   (SEIManifest& sei,                      uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEISEIPrefixIndication           (SEIPrefixIndication& sei,              uint32_t payloadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIConstrainedRaslIndication     (SEIConstrainedRaslIndication& sei,     uint32_t payLoadSize,                     std::ostream *pDecodedMessageOutputStream);
  void xParseSEIShutterInterval(SEIShutterIntervalInfo& sei, uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
  void xParseSEINNPostFilterCharacteristics(SEINeuralNetworkPostFilterCharacteristics& sei, uint32_t payloadSize, const SPS* sps, std::ostream* pDecodedMessageOutputStream);
  void xParseSEINNPostFilterActivation(SEINeuralNetworkPostFilterActivation& sei, uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
  void xParseSEIPhaseIndication(SEIPhaseIndication& sei, uint32_t payloadSize, std::ostream* pDecodedMessageOutputStream);
  void xParseSEIProcessingOrder(SEIProcessingOrderInfo& sei, const NalUnitType nalUnitType, const uint32_t nuhLayerId, uint32_t payloadSize, const VPS* vps, const SPS* sps, HRD& hrd, std::ostream* decodedMessageOutputStream);
  void xParseSEIProcessingOrderNesting(SEIProcessingOrderNesting& sei, const NalUnitType nalUnitType, const uint32_t nuhLayerId, uint32_t payloadSize, const VPS* vps, const SPS* sps, HRD& hrd, std::ostream* decodedMessageOutputStream);
  void xParseSEIPostFilterHint(SEIPostFilterHint &sei, uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
  void xParseSEISourcePictureTimingInfo(SEISourcePictureTimingInfo& sei, uint32_t payloadSize,
                                        std::ostream* pDecodedMessageOutputStream);
  void xParseSEITextDescription(SEITextDescription &sei, uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
  void xParseSEIDigitallySignedContentInitialization(SEIDigitallySignedContentInitialization &sei, uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
  void xParseSEIDigitallySignedContentSelection     (SEIDigitallySignedContentSelection &sei, uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
  void xParseSEIDigitallySignedContentVerification  (SEIDigitallySignedContentVerification &sei, uint32_t payloadSize, std::ostream *pDecodedMessageOutputStream);
#if  JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
  void xParseSEIAIUsageRestrictions                 (SEIAIUsageRestrictions& sei, uint32_t payloadSize, std::ostream* pDecodedMessageOutputStream);
#endif
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  void xParsePackedRegionsInfo(SEIPackedRegionsInfo &sei, const uint32_t nuhLayerId, uint32_t payLoadSize, std::ostream *pDecodedMessageOutputStream);
#endif

  void sei_read_scode(std::ostream *pOS, uint32_t length, int& code, const char *pSymbolName);
  void sei_read_code(std::ostream *pOS, uint32_t length, uint32_t &ruiCode, const char *pSymbolName);
  void sei_read_uvlc(std::ostream *pOS,                uint32_t& ruiCode, const char *pSymbolName);
  void sei_read_svlc(std::ostream *pOS,                int&  ruiCode, const char *pSymbolName);
  void sei_read_flag(std::ostream *pOS,                uint32_t& ruiCode, const char *pSymbolName);
  void sei_read_string(std::ostream* os, std::string& code, const char* symbolName);
#if JVET_AL0339_FGS_SEI_SPATIAL_RESOLUTION
  bool xPayloadExtensionPresent();
#endif

protected:
  HRD m_nestedHrd;
  void xParseSEIGenerativeFaceVideo(SEIGenerativeFaceVideo& sei, uint32_t payloadSize,std::ostream* pDecodedMessageOutputStream);
  uint32_t                         baseCoordinateQuantizationFactor;
  uint32_t                                                   basdCoordinatePointNum;
  bool                                                       base3DCoordinateFlag;
  uint32_t                                                   baseCoordinateZMaxValue;
  std::vector<double>                                        prevCoordinateX;
  std::vector<double>                                        prevCoordinateY;
  std::vector<double>                                        prevCoordinateZ;
  std::vector<double>                                        baseCoordinateX;
  std::vector<double>                                        baseCoordinateY;
  std::vector<double>                                        baseCoordinateZ;
  uint32_t                                                   baseMatrixElementPrecisionFactor;
  uint32_t                                                   baseNumMatrixType;
  std::vector<uint32_t>                                      baseNumMatrices;
  std::vector<uint32_t>                                      baseMatrixWidth;
  std::vector<uint32_t>                                      baseMatrixHeight;
  std::vector<std::vector<std::vector<std::vector<double>>>> baseMatrix;
  std::vector<std::vector<std::vector<std::vector<double>>>> prevMatrix;

  void xParseSEIGenerativeFaceVideoEnhancement(SEIGenerativeFaceVideoEnhancement & sei, uint32_t payloadSize,std::ostream* pDecodedMessageOutputStream);
  double xParseSEIPupilCoordinate(std::ostream *pOS, double refCoordinate, int precisionFactor, const char* eye, const char* axis);
  uint32_t                                                   gfveBaseMatrixElementPrecisionFactor;
  uint32_t                                                   gfveBaseNumMatrices;
  std::vector<uint32_t>                                      gfveBaseMatrixWidth;
  std::vector<uint32_t>                                      gfveBaseMatrixHeight;
  std::vector<std::vector<std::vector<double>>>              gfveBaseMatrix;
  std::vector<std::vector<std::vector<double>>>              gfvePrevMatrix;
  uint32_t                                                   gfveBasePupilCoordinatePrecisionFactor;
  double                                                     prevGfveLeftPupilCoordinateX;
  double                                                     prevGfveLeftPupilCoordinateY;
  double                                                     prevGfveRightPupilCoordinateX;
  double                                                     prevGfveRightPupilCoordinateY;
  double                                                     baseGfveLeftPupilCoordinateX;
  double                                                     baseGfveLeftPupilCoordinateY;
  double                                                     baseGfveRightPupilCoordinateX;
  double                                                     baseGfveRightPupilCoordinateY;
  bool                                                       checkBasePicPupilPresentIdx = false;
};

#if JVET_S0257_DUMP_360SEI_MESSAGE
class SeiCfgFileDump
{
public:
  SeiCfgFileDump()
  : m_360SEIMessageDumped(false)
  {};
  virtual ~SeiCfgFileDump() {};

  void write360SeiDump (std::string decoded360MessageFileName, SEIMessages& seis, const SPS* sps);

protected:
  void xDumpSEIEquirectangularProjection     (SEIEquirectangularProjection &sei, const SPS* sps, std::string decoded360MessageFileName);
  void xDumpSEIGeneralizedCubemapProjection  (SEIGeneralizedCubemapProjection &sei, const SPS* sps, std::string decoded360MessageFileName);

  bool m_360SEIMessageDumped;

};


#endif

//! \}

#endif
