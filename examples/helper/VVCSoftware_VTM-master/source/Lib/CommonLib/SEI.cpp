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

/** \file     SEI.cpp
    \brief    helper functions for SEI handling
*/

#include "CommonDef.h"
#include "SEI.h"

#include "dtrace_next.h"

#if ENABLE_TRACING
void xTraceSEIHeader()
{
  DTRACE( g_trace_ctx, D_HEADER, "=========== SEI message ===========\n" );
}

void xTraceSEIMessageType( SEI::PayloadType payloadType )
{
  DTRACE( g_trace_ctx, D_HEADER, "=========== %s SEI message ===========\n", SEI::getSEIMessageString( payloadType ) );
}
#endif

SEIMessages getSeisByType(const SEIMessages &seiList, SEI::PayloadType seiType)
{
  SEIMessages result;

  for (SEIMessages::const_iterator it=seiList.begin(); it!=seiList.end(); it++)
  {
    if ((*it)->payloadType() == seiType)
    {
      result.push_back(*it);
    }
  }
  return result;
}

SEIMessages extractSeisByType(SEIMessages &seiList, SEI::PayloadType seiType)
{
  SEIMessages result;

  SEIMessages::iterator it=seiList.begin();
  while ( it!=seiList.end() )
  {
    if ((*it)->payloadType() == seiType)
    {
      result.push_back(*it);
      it = seiList.erase(it);
    }
    else
    {
      it++;
    }
  }
  return result;
}


void deleteSEIs (SEIMessages &seiList)
{
  for (SEIMessages::iterator it=seiList.begin(); it!=seiList.end(); it++)
  {
    delete (*it);
  }
  seiList.clear();
}

void SEIBufferingPeriod::copyTo(SEIBufferingPeriod& target) const { target = *this; }

bool SEIScalabilityDimensionInfo::isSDISameContent(SEIScalabilityDimensionInfo* sdiB)
{
  if (!sdiB)
  {
    return false;
  }
  if (m_sdiNumViews != sdiB->m_sdiNumViews)
  {
    return false;
  }
  if (m_sdiMaxLayersMinus1 != sdiB->m_sdiMaxLayersMinus1)
  {
    return false;
  }
  if (m_sdiMultiviewInfoFlag != sdiB->m_sdiMultiviewInfoFlag)
  {
    return false;
  }
  if (m_sdiAuxiliaryInfoFlag != sdiB->m_sdiAuxiliaryInfoFlag)
  {
    return false;
  }
  if (m_sdiMultiviewInfoFlag || m_sdiAuxiliaryInfoFlag)
  {
    if (m_sdiMultiviewInfoFlag)
    {
      if (m_sdiViewIdLenMinus1 != sdiB->m_sdiViewIdLenMinus1)
      {
        return false;
      }
    }
    for (int i = 0; i <= m_sdiMaxLayersMinus1; i++)
    {
      if (m_sdiMultiviewInfoFlag)
      {
        if (m_sdiViewIdVal[i] != sdiB->m_sdiViewIdVal[i])
        {
          return false;
        }
      }
      if (m_sdiAuxiliaryInfoFlag)
      {
        if (m_sdiAuxId[i] != sdiB->m_sdiAuxId[i])
        {
          return false;
        }
        if (m_sdiAuxId[i] > 0)
        {
          if (m_sdiNumAssociatedPrimaryLayersMinus1[i] != sdiB->m_sdiNumAssociatedPrimaryLayersMinus1[i])
          {
            return false;
          }
          for (int j = 0; j <= m_sdiNumAssociatedPrimaryLayersMinus1[i]; j++)
          {
            if (m_sdiAssociatedPrimaryLayerIdx[i][j] != sdiB->m_sdiAssociatedPrimaryLayerIdx[i][j])
            {
              return false;
            }
          }
        }
      }
    }
  }
  return true;
}

uint32_t SEIMultiviewAcquisitionInfo::getMantissaFocalLengthXLen( int i ) const
{
  return xGetSyntaxElementLen( m_maiExponentFocalLengthX[i], m_maiPrecFocalLength, m_maiMantissaFocalLengthX[ i ] );
}

uint32_t SEIMultiviewAcquisitionInfo::getMantissaFocalLengthYLen( int i ) const
{
  return xGetSyntaxElementLen( m_maiExponentFocalLengthY[i], m_maiPrecFocalLength, m_maiMantissaFocalLengthY[ i ]  );
}


uint32_t SEIMultiviewAcquisitionInfo::getMantissaPrincipalPointXLen( int i ) const
{
  return xGetSyntaxElementLen( m_maiExponentPrincipalPointX[i], m_maiPrecPrincipalPoint, m_maiMantissaPrincipalPointX[ i ]  );
}

uint32_t SEIMultiviewAcquisitionInfo::getMantissaPrincipalPointYLen( int i ) const
{
  return xGetSyntaxElementLen( m_maiExponentPrincipalPointY[i], m_maiPrecPrincipalPoint, m_maiMantissaPrincipalPointY[ i ] );
}

uint32_t SEIMultiviewAcquisitionInfo::getMantissaSkewFactorLen( int i ) const
{
  return xGetSyntaxElementLen( m_maiExponentSkewFactor[ i ], m_maiPrecSkewFactor, m_maiMantissaSkewFactor[ i ] );
}

uint32_t SEIMultiviewAcquisitionInfo::getMantissaRLen( int i, int j, int k ) const
{
  return xGetSyntaxElementLen( m_maiExponentR[ i ][ j ][ k ], m_maiPrecRotationParam, m_maiMantissaR[ i ][ j] [ k ] );
}

uint32_t SEIMultiviewAcquisitionInfo::getMantissaTLen( int i, int j ) const
{
  return xGetSyntaxElementLen( m_maiExponentT[ i ][ j ], m_maiPrecTranslationParam, m_maiMantissaT[ i ][ j ] );
}
uint32_t SEIMultiviewAcquisitionInfo::xGetSyntaxElementLen( int expo, int prec, int val ) const
{
  uint32_t len;
  if( expo == 0 )
  {
    len = std::max(0, prec - 30 );
  }
  else
  {
    len = std::max( 0, expo + prec - 31 );
  }

  assert( val >= 0 );
  const uint64_t MAX_VAL = (1llu << len) - 1;
  CHECK( val > MAX_VAL, "Value is too big" );
  return len;
}

bool SEIMultiviewAcquisitionInfo::isMAISameContent(SEIMultiviewAcquisitionInfo *maiB)
{
  if (!maiB)
  {
    return false;
  }
  if (m_maiIntrinsicParamFlag != maiB->m_maiIntrinsicParamFlag)
  {
    return false;
  }
  if (m_maiExtrinsicParamFlag != maiB->m_maiExtrinsicParamFlag)
  {
    return false;
  }
  if (m_maiNumViewsMinus1 != maiB->m_maiNumViewsMinus1)
  {
    return false;
  }
  if (m_maiIntrinsicParamFlag)
  {
    if (m_maiIntrinsicParamsEqualFlag != maiB->m_maiIntrinsicParamsEqualFlag)
    {
      return false;
    }
    if (m_maiPrecFocalLength != maiB->m_maiPrecFocalLength)
    {
      return false;
    }
    if (m_maiPrecPrincipalPoint != maiB->m_maiPrecPrincipalPoint)
    {
      return false;
    }
    if (m_maiPrecSkewFactor != maiB->m_maiPrecSkewFactor)
    {
      return false;
    }
    for (int i = 0; i <= (m_maiIntrinsicParamsEqualFlag ? 0 : m_maiNumViewsMinus1); i++)
    {
      if (m_maiSignFocalLengthX[i] != maiB->m_maiSignFocalLengthX[i])
      {
        return false;
      }
      if (m_maiExponentFocalLengthX[i] != maiB->m_maiExponentFocalLengthX[i])
      {
        return false;
      }
      if (m_maiMantissaFocalLengthX[i] != maiB->m_maiMantissaFocalLengthX[i])
      {
        return false;
      }
      if (m_maiSignFocalLengthY[i] != maiB->m_maiSignFocalLengthY[i])
      {
        return false;
      }
      if (m_maiExponentFocalLengthY[i] != maiB->m_maiExponentFocalLengthY[i])
      {
        return false;
      }
      if (m_maiMantissaFocalLengthY[i] != maiB->m_maiMantissaFocalLengthY[i])
      {
        return false;
      }
      if (m_maiSignPrincipalPointX[i] != maiB->m_maiSignPrincipalPointX[i])
      {
        return false;
      }
      if (m_maiExponentPrincipalPointX[i] != maiB->m_maiExponentPrincipalPointX[i])
      {
        return false;
      }
      if (m_maiMantissaPrincipalPointX[i] != maiB->m_maiMantissaPrincipalPointX[i])
      {
        return false;
      }
      if (m_maiSignPrincipalPointY[i] != maiB->m_maiSignPrincipalPointY[i])
      {
        return false;
      }
      if (m_maiExponentPrincipalPointY[i] != maiB->m_maiExponentPrincipalPointY[i])
      {
        return false;
      }
      if (m_maiMantissaPrincipalPointY[i] != maiB->m_maiMantissaPrincipalPointY[i])
      {
        return false;
      }
      if (m_maiSignSkewFactor[i] != maiB->m_maiSignSkewFactor[i])
      {
        return false;
      }
      if (m_maiExponentSkewFactor[i] != maiB->m_maiExponentSkewFactor[i])
      {
        return false;
      }
      if (m_maiMantissaSkewFactor[i] != maiB->m_maiMantissaSkewFactor[i])
      {
        return false;
      }
    }
  }
  if (m_maiExtrinsicParamFlag)
  {
    if (m_maiPrecRotationParam != maiB->m_maiPrecRotationParam)
    {
      return false;
    }
    if (m_maiPrecTranslationParam != maiB->m_maiPrecTranslationParam)
    {
      return false;
    }
    for (int i = 0; i <= m_maiNumViewsMinus1; i++)
    {
      for (int j = 0; j < 3; j++)
      {
        for (int k = 0; k < 3; k++)
        {
          if (m_maiSignR[i][j][k] != maiB->m_maiSignR[i][j][k])
          {
            return false;
          }
          if (m_maiExponentR[i][j][k] != maiB->m_maiExponentR[i][j][k])
          {
            return false;
          }
          if (m_maiMantissaR[i][j][k] != maiB->m_maiMantissaR[i][j][k])
          {
            return false;
          }
        }
        if (m_maiSignT[i][j] != maiB->m_maiSignT[i][j])
        {
          return false;
        }
        if (m_maiExponentT[i][j] != maiB->m_maiExponentT[i][j])
        {
          return false;
        }
        if (m_maiMantissaT[i][j] != maiB->m_maiMantissaT[i][j])
        {
          return false;
        }
      }
    }
  }
  return true;
}

SEIManifest::SEIManifestDescription SEIManifest::getSEIMessageDescription(const PayloadType payloadType)
{
  std::vector<PayloadType> necessary = { PayloadType::FRAME_PACKING, PayloadType::EQUIRECTANGULAR_PROJECTION,
                                         PayloadType::GENERALIZED_CUBEMAP_PROJECTION, PayloadType::SPHERE_ROTATION,
                                         PayloadType::REGION_WISE_PACKING };

  std::vector<PayloadType> undetermined = { PayloadType::USER_DATA_REGISTERED_ITU_T_T35,
                                            PayloadType::USER_DATA_UNREGISTERED };

  for (auto pt : necessary) 
  {
    if (payloadType == pt) 
    {
      return NECESSARY_SEI_MESSAGE;
    }
  }
  for (auto pt: undetermined)
  {
    if (payloadType == pt) 
    {
      return UNDETERMINED_SEI_MESSAGE;
    } 
  }
  return UNNECESSARY_SEI_MESSAGE;
}

uint8_t SEIPrefixIndication::getNumsOfSeiPrefixIndications(const SEI *sei)
{
  PayloadType payloadType = sei->payloadType();
  CHECK((payloadType == PayloadType::SEI_MANIFEST), "SEI_SPI should not include SEI_manfest");
  CHECK((payloadType == PayloadType::SEI_PREFIX_INDICATION), "SEI_SPI should not include itself");

  //Unable to determine how many indicators are needed, it will be determined in xWriteSEIPrefixIndication() return 3
  std::vector<PayloadType> indicationN = { PayloadType::REGION_WISE_PACKING };
  // Need two indicators to finish writing the SEI prefix indication message(return 2)
  std::vector<PayloadType> indication2 = { PayloadType::SPHERE_ROTATION };

  for (auto pt: indicationN)
  {
    if (payloadType == pt) 
    {
      return 3;
    }
  }
  for (auto pt: indication2)
  {
    if (payloadType == pt)
    {
      return 2;
    }
  }
  return 1;
}

bool SEIMultiviewViewPosition::isMVPSameContent(SEIMultiviewViewPosition *mvpB)
{
  if (!mvpB)
  {
    return false;
  }
  if (m_mvpNumViewsMinus1 != mvpB->m_mvpNumViewsMinus1)
  {
    return false;
  }
  for (int i = 0; i <= m_mvpNumViewsMinus1; i++)
  {
    if (m_mvpViewPosition[i] != mvpB->m_mvpViewPosition[i])
    {
      return false;
    }
  }
  return true;
}

// Static member
static const std::map<SEI::PayloadType, const char *> payloadTypeStrings = {
  { SEI::PayloadType::BUFFERING_PERIOD, "Buffering period" },
  { SEI::PayloadType::PICTURE_TIMING, "Picture timing" },
  { SEI::PayloadType::FILLER_PAYLOAD, "Filler payload" },
  { SEI::PayloadType::USER_DATA_REGISTERED_ITU_T_T35, "User data registered" },
  { SEI::PayloadType::USER_DATA_UNREGISTERED, "User data unregistered" },
  { SEI::PayloadType::FILM_GRAIN_CHARACTERISTICS, "Film grain characteristics" },
  { SEI::PayloadType::FRAME_PACKING, "Frame packing arrangement" },
  { SEI::PayloadType::DISPLAY_ORIENTATION, "Display orientation" },
  { SEI::PayloadType::GREEN_METADATA, "Green metadata" },
  { SEI::PayloadType::PARAMETER_SETS_INCLUSION_INDICATION, "Parameter sets inclusion indication" },
  { SEI::PayloadType::DECODING_UNIT_INFO, "Decoding unit information" },
  { SEI::PayloadType::SCALABLE_NESTING, "Scalable nesting" },
  { SEI::PayloadType::DECODED_PICTURE_HASH, "Decoded picture hash" },
  { SEI::PayloadType::DEPENDENT_RAP_INDICATION, "Dependent RAP indication" },
  { SEI::PayloadType::MASTERING_DISPLAY_COLOUR_VOLUME, "Mastering display colour volume" },
  { SEI::PayloadType::ALTERNATIVE_TRANSFER_CHARACTERISTICS, "Alternative transfer characteristics" },
  { SEI::PayloadType::CONTENT_LIGHT_LEVEL_INFO, "Content light level information" },
  { SEI::PayloadType::AMBIENT_VIEWING_ENVIRONMENT, "Ambient viewing environment" },
  { SEI::PayloadType::CONTENT_COLOUR_VOLUME, "Content colour volume" },
  { SEI::PayloadType::COLOUR_TRANSFORM_INFO, "Colour transform information" },
  { SEI::PayloadType::EQUIRECTANGULAR_PROJECTION, "Equirectangular projection" },
  { SEI::PayloadType::SPHERE_ROTATION, "Sphere rotation" },
  { SEI::PayloadType::REGION_WISE_PACKING, "Region wise packing information" },
  { SEI::PayloadType::OMNI_VIEWPORT, "Omni viewport" },
  { SEI::PayloadType::GENERALIZED_CUBEMAP_PROJECTION, "Generalized cubemap projection" },
  { SEI::PayloadType::ALPHA_CHANNEL_INFO, "Alpha channel information" },
  { SEI::PayloadType::DEPTH_REPRESENTATION_INFO, "Depth representation information" },
  { SEI::PayloadType::MULTIVIEW_ACQUISITION_INFO, "Multiview acquisition information" },
  { SEI::PayloadType::MULTIVIEW_VIEW_POSITION, "Multiview view position" },
  { SEI::PayloadType::SAMPLE_ASPECT_RATIO_INFO, "Sample aspect ratio information" },
  { SEI::PayloadType::SUBPICTURE_LEVEL_INFO, "Subpicture level information" },
  { SEI::PayloadType::ANNOTATED_REGIONS, "Annotated Region" },
  { SEI::PayloadType::SCALABILITY_DIMENSION_INFO, "Scalability dimension information" },
  { SEI::PayloadType::EXTENDED_DRAP_INDICATION, "Extended DRAP indication" },
  { SEI::PayloadType::SEI_MANIFEST, "SEI manifest" },
  { SEI::PayloadType::SEI_PREFIX_INDICATION, "SEI prefix indication" },
  { SEI::PayloadType::CONSTRAINED_RASL_ENCODING, "Constrained RASL encoding" },
  { SEI::PayloadType::VDI_SEI_ENVELOPE, "Video decoding interface SEI envelope" },
  { SEI::PayloadType::SHUTTER_INTERVAL_INFO, "Shutter interval information" },
  { SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_CHARACTERISTICS, "Neural network post-filter characteristics" },
  { SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_ACTIVATION, "Neural network post-filter activation" },
  { SEI::PayloadType::PHASE_INDICATION, "Phase Indication" },
  { SEI::PayloadType::SEI_PROCESSING_ORDER, "SEI messages Processing order" },
  { SEI::PayloadType::SOURCE_PICTURE_TIMING_INFO, "Source picture timing info" },
  { SEI::PayloadType::MODALITY_INFORMATION, "Modality information" },
  { SEI::PayloadType::DIGITALLY_SIGNED_CONTENT_INITIALIZATION, "Digitally Signed Content Initialization" },
  { SEI::PayloadType::DIGITALLY_SIGNED_CONTENT_SELECTION, "Digitally Signed Content Selection" },
  { SEI::PayloadType::DIGITALLY_SIGNED_CONTENT_VERIFICATION, "Digitally Signed Content Verification" },
  { SEI::PayloadType::GENERATIVE_FACE_VIDEO, "Generative face video" },
  { SEI::PayloadType::GENERATIVE_FACE_VIDEO_ENHANCEMENT, "Generative face video enhancement" },
#if JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
  { SEI::PayloadType::AI_USAGE_RESTRICTIONS, "AI usage restrictions" },
#endif
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  { SEI::PayloadType::PACKED_REGIONS_INFO, "Packed regions info" },
#endif
};

const char *SEI::getSEIMessageString(SEI::PayloadType payloadType)
{
  auto p = payloadTypeStrings.find(payloadType);
  if (p != payloadTypeStrings.end())
  {
    return p->second;
  }
  else
  {
    return "Unknown";
  }
}

SEIShutterIntervalInfo::SEIShutterIntervalInfo(const SEIShutterIntervalInfo& sei)
{
  m_siiEnabled = sei.m_siiEnabled;
  m_siiNumUnitsInShutterInterval = sei.m_siiNumUnitsInShutterInterval;
  m_siiTimeScale = sei.m_siiTimeScale;
  m_siiMaxSubLayersMinus1 = sei.m_siiMaxSubLayersMinus1;
  m_siiFixedSIwithinCLVS = sei.m_siiFixedSIwithinCLVS;
  m_siiSubLayerNumUnitsInSI = sei.m_siiSubLayerNumUnitsInSI;
}
SEISourcePictureTimingInfo::SEISourcePictureTimingInfo(const SEISourcePictureTimingInfo& sei)
{
  m_sptiSEIEnabled                         = sei.m_sptiSEIEnabled;
  m_sptiSourceTimingEqualsOutputTimingFlag = sei.m_sptiSourceTimingEqualsOutputTimingFlag;
  m_sptiSourceType                         = sei.m_sptiSourceType;
  m_sptiTimeScale                          = sei.m_sptiTimeScale;
  m_sptiNumUnitsInElementalInterval        = sei.m_sptiNumUnitsInElementalInterval;
  m_sptiDirectionFlag                      = sei.m_sptiDirectionFlag;
}

SEIProcessingOrderInfo::SEIProcessingOrderInfo(const SEIProcessingOrderInfo& sei)
{
  m_posEnabled = sei.m_posEnabled;
  m_posId = sei.m_posId;
  m_posForHumanViewingIdc = sei.m_posForHumanViewingIdc;
  m_posForMachineAnalysisIdc = sei.m_posForMachineAnalysisIdc;
  m_posNumMinus2 = sei.m_posNumMinus2;
  m_posBreadthFirstFlag = sei.m_posBreadthFirstFlag;
  m_posWrappingFlag = sei.m_posWrappingFlag;
  m_posImportanceFlag = sei.m_posImportanceFlag;
  m_posProcessingDegreeFlag = sei.m_posProcessingDegreeFlag;
  m_posPrefixFlag = sei.m_posPrefixFlag;
  m_posPayloadType = sei.m_posPayloadType;
  m_posProcessingOrder = sei.m_posProcessingOrder;
  m_posPrefixByte = sei.m_posPrefixByte;
#if JVET_AJ0105_SPO_COMPLEXITY_INFO
  m_posComplexityInfoPresentFlag = sei.m_posComplexityInfoPresentFlag;
  m_posParameterTypeIdc = sei.m_posParameterTypeIdc;
  m_posLog2ParameterBitLengthMinus3 = sei.m_posLog2ParameterBitLengthMinus3;
  m_posNumParametersIdc = sei.m_posNumParametersIdc;
  m_posNumKmacOperationIdc = sei.m_posNumKmacOperationIdc;
  m_posTotalKilobyteSize = sei.m_posTotalKilobyteSize;
#endif
}

SEIProcessingOrderNesting::SEIProcessingOrderNesting(const SEIProcessingOrderNesting& sei)
{
  m_ponTargetPoId = sei.m_ponTargetPoId;
  m_ponNumSeisMinus1 = sei.m_ponNumSeisMinus1;
  m_ponProcessingOrder = sei.m_ponProcessingOrder;
  m_ponWrapSeiMessages = sei.m_ponWrapSeiMessages;
  m_ponPayloadType = sei.m_ponPayloadType;
}

SEIEquirectangularProjection::SEIEquirectangularProjection(const SEIEquirectangularProjection& sei)
{
  m_erpCancelFlag = sei.m_erpCancelFlag;
  m_erpPersistenceFlag = sei.m_erpPersistenceFlag;
  m_erpGuardBandFlag = sei.m_erpGuardBandFlag;
  m_erpGuardBandType = sei.m_erpGuardBandType;
  m_erpLeftGuardBandWidth = sei.m_erpLeftGuardBandWidth;
  m_erpRightGuardBandWidth = sei.m_erpRightGuardBandWidth;
}

SEISphereRotation::SEISphereRotation(const SEISphereRotation& sei)
{
  m_sphereRotationCancelFlag = sei.m_sphereRotationCancelFlag;
  m_sphereRotationPersistenceFlag = sei.m_sphereRotationPersistenceFlag;
  m_sphereRotationYaw = sei.m_sphereRotationYaw;
  m_sphereRotationPitch = sei.m_sphereRotationPitch;
  m_sphereRotationRoll = sei.m_sphereRotationRoll;
}

SEIOmniViewport::SEIOmniViewport(const SEIOmniViewport& sei)
{
  m_omniViewportId = sei.m_omniViewportId;
  m_omniViewportCancelFlag = sei.m_omniViewportCancelFlag;
  m_omniViewportPersistenceFlag = sei.m_omniViewportPersistenceFlag;
  m_omniViewportCntMinus1 = sei.m_omniViewportCntMinus1;
  m_omniViewportRegions = sei.m_omniViewportRegions;
  for(unsigned region = 0; region < m_omniViewportRegions.size(); region++)
  {
    m_omniViewportRegions[region].azimuthCentre = sei.m_omniViewportRegions[region].azimuthCentre;
    m_omniViewportRegions[region].elevationCentre = sei.m_omniViewportRegions[region].elevationCentre;
    m_omniViewportRegions[region].tiltCentre = sei.m_omniViewportRegions[region].tiltCentre;
    m_omniViewportRegions[region].horRange = sei.m_omniViewportRegions[region].horRange;
    m_omniViewportRegions[region].verRange = sei.m_omniViewportRegions[region].verRange;
  }
}

SEIRegionWisePacking::SEIRegionWisePacking(const SEIRegionWisePacking& sei)
{
  m_rwpCancelFlag = sei.m_rwpCancelFlag;
  m_rwpPersistenceFlag = sei.m_rwpPersistenceFlag;
  m_constituentPictureMatchingFlag = sei.m_constituentPictureMatchingFlag;
  m_numPackedRegions = sei.m_numPackedRegions;
  m_projPictureWidth = sei.m_projPictureWidth;
  m_projPictureHeight = sei.m_projPictureHeight;
  m_packedPictureWidth = sei.m_packedPictureWidth;
  m_packedPictureHeight = sei.m_packedPictureHeight;
  m_rwpTransformType = sei.m_rwpTransformType;
  m_rwpGuardBandFlag = sei.m_rwpGuardBandFlag;
  m_projRegionWidth = sei.m_projRegionWidth;
  m_projRegionHeight = sei.m_projRegionHeight;
  m_rwpProjRegionTop = sei.m_rwpProjRegionTop;
  m_projRegionLeft = sei.m_projRegionLeft;
  m_packedRegionWidth = sei.m_packedRegionWidth;
  m_packedRegionHeight = sei.m_packedRegionHeight;
  m_packedRegionTop = sei.m_packedRegionTop;
  m_packedRegionLeft = sei.m_packedRegionLeft;
  m_rwpLeftGuardBandWidth = sei.m_rwpLeftGuardBandWidth;
  m_rwpRightGuardBandWidth = sei.m_rwpRightGuardBandWidth;
  m_rwpTopGuardBandHeight = sei.m_rwpTopGuardBandHeight;
  m_rwpBottomGuardBandHeight = sei.m_rwpBottomGuardBandHeight;
  m_rwpGuardBandNotUsedForPredFlag = sei.m_rwpGuardBandNotUsedForPredFlag;
  m_rwpGuardBandType = sei.m_rwpGuardBandType;
}

SEIGeneralizedCubemapProjection::SEIGeneralizedCubemapProjection(const SEIGeneralizedCubemapProjection& sei)
{
  m_gcmpCancelFlag = sei.m_gcmpCancelFlag;
  m_gcmpPersistenceFlag = sei.m_gcmpPersistenceFlag;
  m_gcmpPackingType = sei.m_gcmpPackingType;
  m_gcmpMappingFunctionType = sei.m_gcmpMappingFunctionType;
  m_gcmpFaceIndex = sei.m_gcmpFaceIndex;
  m_gcmpFaceRotation = sei.m_gcmpFaceRotation;
  m_gcmpFunctionCoeffU = sei.m_gcmpFunctionCoeffU;
  m_gcmpFunctionUAffectedByVFlag = sei.m_gcmpFunctionUAffectedByVFlag;
  m_gcmpFunctionCoeffV = sei.m_gcmpFunctionCoeffV;
  m_gcmpFunctionVAffectedByUFlag = sei.m_gcmpFunctionVAffectedByUFlag;
  m_gcmpGuardBandFlag = sei.m_gcmpGuardBandFlag;
  m_gcmpGuardBandType = sei.m_gcmpGuardBandType;
  m_gcmpGuardBandBoundaryExteriorFlag = sei.m_gcmpGuardBandBoundaryExteriorFlag;
  m_gcmpGuardBandSamplesMinus1 = sei.m_gcmpGuardBandSamplesMinus1;
}

SEIScalabilityDimensionInfo::SEIScalabilityDimensionInfo(const SEIScalabilityDimensionInfo& sei)
{
  m_sdiNumViews = sei.m_sdiNumViews;
  m_sdiMaxLayersMinus1 = sei.m_sdiMaxLayersMinus1;
  m_sdiMultiviewInfoFlag = sei.m_sdiMultiviewInfoFlag;
  m_sdiAuxiliaryInfoFlag = sei.m_sdiAuxiliaryInfoFlag;
  m_sdiViewIdLenMinus1 = sei.m_sdiViewIdLenMinus1;
  m_sdiLayerId = sei.m_sdiLayerId;
  m_sdiViewIdVal = sei.m_sdiViewIdVal;
  m_sdiAuxId = sei.m_sdiAuxId;
  m_sdiNumAssociatedPrimaryLayersMinus1 = sei.m_sdiNumAssociatedPrimaryLayersMinus1;
  m_sdiAssociatedPrimaryLayerIdx = sei.m_sdiAssociatedPrimaryLayerIdx;
}

SEIMultiviewAcquisitionInfo::SEIMultiviewAcquisitionInfo(const SEIMultiviewAcquisitionInfo& sei)
{
  m_maiIntrinsicParamFlag = sei.m_maiIntrinsicParamFlag;
  m_maiExtrinsicParamFlag = sei.m_maiExtrinsicParamFlag;
  m_maiNumViewsMinus1 = sei.m_maiNumViewsMinus1;
  m_maiIntrinsicParamsEqualFlag = sei.m_maiIntrinsicParamsEqualFlag;
  m_maiPrecFocalLength = sei.m_maiPrecFocalLength;
  m_maiPrecPrincipalPoint = sei.m_maiPrecPrincipalPoint;
  m_maiPrecSkewFactor = sei.m_maiPrecSkewFactor;
  m_maiSignFocalLengthX = sei.m_maiSignFocalLengthX;
  m_maiExponentFocalLengthX = sei.m_maiExponentFocalLengthX;
  m_maiMantissaFocalLengthX = sei.m_maiMantissaFocalLengthX;
  m_maiSignFocalLengthY = sei.m_maiSignFocalLengthY;
  m_maiExponentFocalLengthY = sei.m_maiExponentFocalLengthY;
  m_maiMantissaFocalLengthY = sei.m_maiMantissaFocalLengthY;
  m_maiSignPrincipalPointX = sei.m_maiSignPrincipalPointX;
  m_maiExponentPrincipalPointX = sei.m_maiExponentPrincipalPointX;
  m_maiMantissaPrincipalPointX = sei.m_maiMantissaPrincipalPointX;
  m_maiSignPrincipalPointY = sei.m_maiSignPrincipalPointY;
  m_maiExponentPrincipalPointY = sei.m_maiExponentPrincipalPointY;
  m_maiMantissaPrincipalPointY = sei.m_maiMantissaPrincipalPointY;
  m_maiSignSkewFactor = sei.m_maiSignSkewFactor;
  m_maiExponentSkewFactor = sei.m_maiExponentSkewFactor;
  m_maiMantissaSkewFactor = sei.m_maiMantissaSkewFactor;
  m_maiPrecRotationParam = sei.m_maiPrecRotationParam;
  m_maiPrecTranslationParam = sei.m_maiPrecTranslationParam;
  m_maiSignR = sei.m_maiSignR;
  m_maiExponentR = sei.m_maiExponentR;
  m_maiMantissaR = sei.m_maiMantissaR;
  m_maiSignT = sei.m_maiSignT;
  m_maiExponentT = sei.m_maiExponentT;
  m_maiMantissaT = sei.m_maiMantissaT;
}

SEIMultiviewViewPosition::SEIMultiviewViewPosition(const SEIMultiviewViewPosition& sei)
{
  m_mvpNumViewsMinus1 = sei.m_mvpNumViewsMinus1;
  m_mvpViewPosition = sei.m_mvpViewPosition;
}

SEIAlphaChannelInfo::SEIAlphaChannelInfo(const SEIAlphaChannelInfo& sei)
{
  m_aciCancelFlag = sei.m_aciCancelFlag;
  m_aciUseIdc = sei.m_aciUseIdc;
  m_aciBitDepthMinus8 = sei.m_aciBitDepthMinus8;
  m_aciTransparentValue = sei.m_aciTransparentValue;
  m_aciOpaqueValue = sei.m_aciOpaqueValue;
  m_aciIncrFlag = sei.m_aciIncrFlag;
  m_aciClipFlag = sei.m_aciClipFlag;
  m_aciClipTypeFlag = sei.m_aciClipTypeFlag;
}

SEIDepthRepresentationInfo::SEIDepthRepresentationInfo(const SEIDepthRepresentationInfo& sei)
{
  m_driZNearFlag = sei.m_driZNearFlag;
  m_driZFarFlag = sei.m_driZFarFlag;
  m_driDMinFlag = sei.m_driDMinFlag;
  m_driDMaxFlag = sei.m_driDMaxFlag;
  m_driZNear = sei.m_driZNear;
  m_driZFar = sei.m_driZFar;
  m_driDMin = sei.m_driDMin;
  m_driDMax = sei.m_driDMax;
  m_driDepthRepresentationType = sei.m_driDepthRepresentationType;
  m_driDisparityRefViewId = sei.m_driDisparityRefViewId;
  m_driDepthNonlinearRepresentationNumMinus1 = sei.m_driDepthNonlinearRepresentationNumMinus1;
  m_driDepthNonlinearRepresentationModel = sei.m_driDepthNonlinearRepresentationModel;
}

SEISampleAspectRatioInfo::SEISampleAspectRatioInfo(const SEISampleAspectRatioInfo& sei)
{
  m_sariCancelFlag = sei.m_sariCancelFlag;
  m_sariPersistenceFlag = sei.m_sariPersistenceFlag;
  m_sariAspectRatioIdc = sei.m_sariAspectRatioIdc;
  m_sariSarWidth = sei.m_sariSarWidth;
  m_sariSarHeight = sei.m_sariSarHeight;
}

SEIPhaseIndication::SEIPhaseIndication(const SEIPhaseIndication& sei)
{
  m_horPhaseNum = sei.m_horPhaseNum;
  m_horPhaseDenMinus1 = sei.m_horPhaseDenMinus1;
  m_verPhaseNum = sei.m_verPhaseNum;
  m_verPhaseDenMinus1 = sei.m_verPhaseDenMinus1;
}

SEIDecodedPictureHash::SEIDecodedPictureHash(const SEIDecodedPictureHash& sei)
{
  method = sei.method;
  singleCompFlag = sei.singleCompFlag;
  m_pictureHash = sei.m_pictureHash;
}

SEIBufferingPeriod::SEIBufferingPeriod(const SEIBufferingPeriod& sei) = default;

SEIPictureTiming::SEIPictureTiming(const SEIPictureTiming& sei) = default;

SEIDecodingUnitInfo::SEIDecodingUnitInfo(const SEIDecodingUnitInfo& sei) = default;

SEIFrameFieldInfo::SEIFrameFieldInfo(const SEIFrameFieldInfo& sei)
{
  m_fieldPicFlag = sei.m_fieldPicFlag;
  m_bottomFieldFlag = sei.m_bottomFieldFlag;
  m_pairingIndicatedFlag = sei.m_pairingIndicatedFlag;
  m_pairedWithNextFieldFlag = sei.m_pairedWithNextFieldFlag;
  m_displayFieldsFromFrameFlag = sei.m_displayFieldsFromFrameFlag;
  m_topFieldFirstFlag = sei.m_topFieldFirstFlag;
  m_displayElementalPeriodsMinus1 = sei.m_displayElementalPeriodsMinus1;
  m_sourceScanType = sei.m_sourceScanType;
  m_duplicateFlag = sei.m_duplicateFlag;
}

SEIFramePacking::SEIFramePacking(const SEIFramePacking& sei)
{
  m_arrangementId = sei.m_arrangementId;
  m_arrangementCancelFlag = sei.m_arrangementCancelFlag;
  m_arrangementType = sei.m_arrangementType;
  m_quincunxSamplingFlag = sei.m_quincunxSamplingFlag;
  m_contentInterpretationType = sei.m_contentInterpretationType;
  m_spatialFlippingFlag = sei.m_spatialFlippingFlag;
  m_frame0FlippedFlag = sei.m_frame0FlippedFlag;
  m_fieldViewsFlag = sei.m_fieldViewsFlag;
  m_currentFrameIsFrame0Flag = sei.m_currentFrameIsFrame0Flag;
  m_frame0SelfContainedFlag = sei.m_frame0SelfContainedFlag;
  m_frame1SelfContainedFlag = sei.m_frame1SelfContainedFlag;
  m_frame0GridPositionX = sei.m_frame0GridPositionX;
  m_frame0GridPositionY = sei.m_frame0GridPositionY;
  m_frame1GridPositionX = sei.m_frame1GridPositionX;
  m_frame1GridPositionY = sei.m_frame1GridPositionY;
  m_arrangementReservedByte = sei.m_arrangementReservedByte;
  m_arrangementPersistenceFlag = sei.m_arrangementPersistenceFlag;
  m_upsampledAspectRatio = sei.m_upsampledAspectRatio;
}

SEIDisplayOrientation::SEIDisplayOrientation(const SEIDisplayOrientation& sei)
{
  m_doCancelFlag = sei.m_doCancelFlag;
  m_doPersistenceFlag = sei.m_doPersistenceFlag;
  m_doTransformType = sei.m_doTransformType;
}

SEIGreenMetadataInfo::SEIGreenMetadataInfo(const SEIGreenMetadataInfo& sei)
{
  m_greenMetadataType = sei.m_greenMetadataType;
  m_xsdSubpicNumberMinus1 = sei.m_xsdSubpicNumberMinus1;
#if GREEN_METADATA_SEI_ENABLED
  m_xsdSubPicIdc = sei.m_xsdSubPicIdc;
#endif
  m_xsdMetricValuePSNR = sei.m_xsdMetricValuePSNR;
  m_xsdMetricValueSSIM = sei.m_xsdMetricValueSSIM;
  m_xsdMetricValueWPSNR = sei.m_xsdMetricValueWPSNR;
  m_xsdMetricValueWSPSNR = sei.m_xsdMetricValueWSPSNR;
  m_xsdMetricTypePSNR = sei.m_xsdMetricTypePSNR;
  m_xsdMetricTypeSSIM = sei.m_xsdMetricTypeSSIM;
  m_xsdMetricTypeWPSNR = sei.m_xsdMetricTypeWPSNR;
  m_xsdMetricTypeWSPSNR = sei.m_xsdMetricTypeWSPSNR;
  m_greenMetadataGranularityType = sei.m_greenMetadataGranularityType;
  m_greenMetadataExtendedRepresentation = sei.m_greenMetadataExtendedRepresentation;
  m_periodType = sei.m_periodType;
  m_numPictures = sei.m_numPictures;
  m_numSeconds = sei.m_numSeconds;
  m_greenComplexityMetrics.portionNonZeroBlocksArea = sei.m_greenComplexityMetrics.portionNonZeroBlocksArea;
  m_greenComplexityMetrics.portionNonZero_4_8_16BlocksArea = sei.m_greenComplexityMetrics.portionNonZero_4_8_16BlocksArea;
  m_greenComplexityMetrics.portionNonZero_32_64_128BlocksArea = sei.m_greenComplexityMetrics.portionNonZero_32_64_128BlocksArea;
  m_greenComplexityMetrics.portionNonZero_256_512_1024BlocksArea = sei.m_greenComplexityMetrics.portionNonZero_256_512_1024BlocksArea;
  m_greenComplexityMetrics.portionNonZero_2048_4096BlocksArea = sei.m_greenComplexityMetrics.portionNonZero_2048_4096BlocksArea;
  m_greenComplexityMetrics.portionNonZeroTransformCoefficientsArea = sei.m_greenComplexityMetrics.portionNonZeroTransformCoefficientsArea;
  m_greenComplexityMetrics.portionIntraPredictedBlocksArea = sei.m_greenComplexityMetrics.portionIntraPredictedBlocksArea;
  m_greenComplexityMetrics.portionBdofBlocksArea = sei.m_greenComplexityMetrics.portionBdofBlocksArea;
  m_greenComplexityMetrics.portionBiAndGpmPredictedBlocksArea = sei.m_greenComplexityMetrics.portionBiAndGpmPredictedBlocksArea;
  m_greenComplexityMetrics.portionDeblockingInstances = sei.m_greenComplexityMetrics.portionDeblockingInstances;
  m_greenComplexityMetrics.portionSaoInstances = sei.m_greenComplexityMetrics.portionSaoInstances;
  m_greenComplexityMetrics.portionAlfInstances = sei.m_greenComplexityMetrics.portionAlfInstances;
#if GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464
  m_greenMetadataAMIFlags                  = sei.m_greenMetadataAMIFlags;
  m_greenMetadataAMIDisplayModel           = sei.m_greenMetadataAMIDisplayModel;
  m_greenMetadataAMIApproximationModel     = sei.m_greenMetadataAMIApproximationModel;
  m_greenMetadataAMIMapNumber              = sei.m_greenMetadataAMIMapNumber;
  m_greenMetadataAMILayerId                = sei.m_greenMetadataAMILayerId;
  m_greenMetadataAMIOlsNumber              = sei.m_greenMetadataAMIOlsNumber;
  m_greenMetadataAMIOlsId                  = sei.m_greenMetadataAMIOlsId;
  m_greenMetadataAMIEnergyReductionRate    = sei.m_greenMetadataAMIEnergyReductionRate;
  m_greenMetadataAMIVideoQualityMetricType = sei.m_greenMetadataAMIVideoQualityMetricType;
  m_greenMetadataAMIVideoQualityLevel      = sei.m_greenMetadataAMIVideoQualityLevel;
  m_greenMetadataAMIMaxValue               = sei.m_greenMetadataAMIMaxValue;
  m_greenMetadataAMIAttenuationUseIdc      = sei.m_greenMetadataAMIAttenuationUseIdc;
  m_greenMetadataAMIAttenuationCompIdc     = sei.m_greenMetadataAMIAttenuationCompIdc;
  m_greenMetadataAMIPreprocessingFlag      = sei.m_greenMetadataAMIPreprocessingFlag;
  m_greenMetadataAMIPreprocessingTypeIdc   = sei.m_greenMetadataAMIPreprocessingTypeIdc;
  m_greenMetadataAMIPreprocessingScaleIdc  = sei.m_greenMetadataAMIPreprocessingScaleIdc;
  m_greenMetadataAMIBacklightScalingIdc    = sei.m_greenMetadataAMIBacklightScalingIdc;
#endif
}

SEIParameterSetsInclusionIndication::SEIParameterSetsInclusionIndication(const SEIParameterSetsInclusionIndication& sei)
{
  m_selfContainedClvsFlag = sei.m_selfContainedClvsFlag;
}

SEIMasteringDisplayColourVolume::SEIMasteringDisplayColourVolume(const SEIMasteringDisplayColourVolume& sei)
{
  values.colourVolumeSEIEnabled = sei.values.colourVolumeSEIEnabled;
  values.maxLuminance = sei.values.maxLuminance;
  values.minLuminance = sei.values.minLuminance;
  std::memcpy(values.primaries, sei.values.primaries, sizeof(sei.values.primaries));
  std::memcpy(values.whitePoint, sei.values.whitePoint, sizeof(sei.values.whitePoint));
}

SEIScalableNesting::SEIScalableNesting(const SEIScalableNesting& sei) = default;

SEIAlternativeTransferCharacteristics::SEIAlternativeTransferCharacteristics(const SEIAlternativeTransferCharacteristics& sei)
{
  m_preferredTransferCharacteristics = sei.m_preferredTransferCharacteristics;
}

SEIUserDataRegistered::SEIUserDataRegistered(const SEIUserDataRegistered& sei)
{
  m_ituCountryCode = sei.m_ituCountryCode;
  m_userData = sei.m_userData;
}

SEIFilmGrainCharacteristics::SEIFilmGrainCharacteristics(const SEIFilmGrainCharacteristics& sei)
{
  m_filmGrainCharacteristicsCancelFlag = sei.m_filmGrainCharacteristicsCancelFlag;
  m_filmGrainModelId = sei.m_filmGrainModelId;
  m_separateColourDescriptionPresentFlag = sei.m_separateColourDescriptionPresentFlag;
  m_filmGrainBitDepthLumaMinus8 = sei.m_filmGrainBitDepthLumaMinus8;
  m_filmGrainBitDepthChromaMinus8 = sei.m_filmGrainBitDepthChromaMinus8;
  m_filmGrainFullRangeFlag = sei.m_filmGrainFullRangeFlag;
  m_filmGrainColourPrimaries = sei.m_filmGrainColourPrimaries;
  m_filmGrainTransferCharacteristics = sei.m_filmGrainTransferCharacteristics;
  m_filmGrainMatrixCoeffs = sei.m_filmGrainMatrixCoeffs;
  m_blendingModeId = sei.m_blendingModeId;
  m_log2ScaleFactor = sei.m_log2ScaleFactor;
  for (int i = 0; i < MAX_NUM_COMPONENT; i++)
  {
    m_compModel[i].presentFlag = sei.m_compModel[i].presentFlag;
    m_compModel[i].numModelValues = sei.m_compModel[i].numModelValues;
    m_compModel[i].numIntensityIntervals = sei.m_compModel[i].numIntensityIntervals;
    m_compModel[i].intensityValues = sei.m_compModel[i].intensityValues;
  }
  m_filmGrainCharacteristicsPersistenceFlag = sei.m_filmGrainCharacteristicsPersistenceFlag;
#if JVET_AL0339_FGS_SEI_SPATIAL_RESOLUTION
  m_spatialResolutionPresentFlag = sei.m_spatialResolutionPresentFlag;
  m_picWidthInLumaSamples = sei.m_picWidthInLumaSamples;
  m_picHeightInLumaSamples = sei.m_picHeightInLumaSamples;
#endif
}

SEIContentLightLevelInfo::SEIContentLightLevelInfo(const SEIContentLightLevelInfo& sei)
{
  m_maxContentLightLevel = sei.m_maxContentLightLevel;
  m_maxPicAverageLightLevel = sei.m_maxPicAverageLightLevel;
}

SEIAmbientViewingEnvironment::SEIAmbientViewingEnvironment(const SEIAmbientViewingEnvironment& sei)
{
  m_ambientIlluminance = sei.m_ambientIlluminance;
  m_ambientLightX = sei.m_ambientLightX;
  m_ambientLightY = sei.m_ambientLightY;
}

SEIColourTransformInfo::SEIColourTransformInfo(const SEIColourTransformInfo& sei)
{
  m_id = sei.m_id;
  m_signalInfoFlag = sei.m_signalInfoFlag;
  m_fullRangeFlag = sei.m_fullRangeFlag;
  m_primaries = sei.m_primaries;
  m_transferFunction = sei.m_transferFunction;
  m_matrixCoefs = sei.m_matrixCoefs;
  m_crossComponentFlag = sei.m_crossComponentFlag;
  m_crossComponentInferred = sei.m_crossComponentInferred;
  m_numberChromaLutMinus1 = sei.m_numberChromaLutMinus1;
  m_chromaOffset = sei.m_chromaOffset;
  m_bitdepth = sei.m_bitdepth;
  m_log2NumberOfPointsPerLut = sei.m_log2NumberOfPointsPerLut;
  for (int i = 0; i < MAX_NUM_COMPONENT; i++)
  {
    m_lut[i].presentFlag = sei.m_lut[i].presentFlag;
    m_lut[i].numLutValues = sei.m_lut[i].numLutValues;
    m_lut[i].lutValues = sei.m_lut[i].lutValues;
  }
}

SEIContentColourVolume::SEIContentColourVolume(const SEIContentColourVolume& sei)
{
  m_ccvCancelFlag = sei.m_ccvCancelFlag;
  m_ccvPersistenceFlag = sei.m_ccvPersistenceFlag;
  m_ccvPrimariesPresentFlag = sei.m_ccvPrimariesPresentFlag;
  m_ccvMinLuminanceValuePresentFlag = sei.m_ccvMinLuminanceValuePresentFlag;
  m_ccvMaxLuminanceValuePresentFlag = sei.m_ccvMaxLuminanceValuePresentFlag;
  m_ccvAvgLuminanceValuePresentFlag = sei.m_ccvAvgLuminanceValuePresentFlag;
  std::memcpy(m_ccvPrimariesX, sei.m_ccvPrimariesX, sizeof(sei.m_ccvPrimariesX));
  std::memcpy(m_ccvPrimariesY, sei.m_ccvPrimariesY, sizeof(sei.m_ccvPrimariesY));
  m_ccvMinLuminanceValue = sei.m_ccvMinLuminanceValue;
  m_ccvMaxLuminanceValue = sei.m_ccvMaxLuminanceValue;
  m_ccvAvgLuminanceValue = sei.m_ccvAvgLuminanceValue;
}

SEISubpictureLevelInfo::SEISubpictureLevelInfo(const SEISubpictureLevelInfo& sli) = default;

SEIManifest::SEIManifest(const SEIManifest& sei)
{
  m_manifestNumSeiMsgTypes = sei.m_manifestNumSeiMsgTypes;
  m_manifestSeiPayloadType = sei.m_manifestSeiPayloadType;
  m_manifestSeiDescription = sei.m_manifestSeiDescription;
}

SEIPrefixIndication::SEIPrefixIndication(const SEIPrefixIndication& sei)
{
  m_prefixSeiPayloadType = sei.m_prefixSeiPayloadType;
  m_numSeiPrefixIndicationsMinus1 = sei.m_numSeiPrefixIndicationsMinus1;
  m_numBitsInPrefixIndicationMinus1 = sei.m_numBitsInPrefixIndicationMinus1;
  m_seiPrefixDataBit = sei.m_seiPrefixDataBit;
  m_payload = sei.m_payload;
}

SEIExtendedDrapIndication::SEIExtendedDrapIndication(const SEIExtendedDrapIndication& sei)
{
  m_edrapIndicationRapIdMinus1 = sei.m_edrapIndicationRapIdMinus1;
  m_edrapIndicationLeadingPicturesDecodableFlag = sei.m_edrapIndicationLeadingPicturesDecodableFlag;
  m_edrapIndicationReservedZero12Bits = sei.m_edrapIndicationReservedZero12Bits;
  m_edrapIndicationNumRefRapPicsMinus1 = sei.m_edrapIndicationNumRefRapPicsMinus1;
  m_edrapIndicationRefRapId = sei.m_edrapIndicationRefRapId;
}

SEINeuralNetworkPostFilterCharacteristics::SEINeuralNetworkPostFilterCharacteristics(
  const SEINeuralNetworkPostFilterCharacteristics& sei)
{
  m_id = sei.m_id;
  m_modeIdc = sei.m_modeIdc;
  m_propertyPresentFlag = sei.m_propertyPresentFlag;
  m_purpose = sei.m_purpose;
  m_outSubCFlag = sei.m_outSubCFlag;
  m_outSubWidthC = sei.m_outSubWidthC;
  m_outSubHeightC = sei.m_outSubHeightC;
  m_outColourFormatIdc = sei.m_outColourFormatIdc;
  m_picWidthNumeratorMinus1 = sei.m_picWidthNumeratorMinus1;
  m_picWidthDenominatorMinus1 = sei.m_picWidthDenominatorMinus1;
  m_picHeightNumeratorMinus1 = sei.m_picHeightNumeratorMinus1;
  m_picHeightDenominatorMinus1 = sei.m_picHeightDenominatorMinus1;
  m_picWidthInLumaSamples = sei.m_picWidthInLumaSamples;
  m_picHeightInLumaSamples = sei.m_picHeightInLumaSamples;
  m_inpTensorBitDepthLumaMinus8 = sei.m_inpTensorBitDepthLumaMinus8;
  m_inpTensorBitDepthChromaMinus8 = sei.m_inpTensorBitDepthChromaMinus8;
  m_outTensorBitDepthLumaMinus8 = sei.m_outTensorBitDepthLumaMinus8;
  m_outTensorBitDepthChromaMinus8 = sei.m_outTensorBitDepthChromaMinus8;
  m_componentLastFlag = sei.m_componentLastFlag;
  m_inpFormatIdc = sei.m_inpFormatIdc;
  m_auxInpIdc = sei.m_auxInpIdc;
  m_sepColDescriptionFlag = sei.m_sepColDescriptionFlag;
  m_fullRangeFlag = sei.m_fullRangeFlag;
  m_colPrimaries = sei.m_colPrimaries;
  m_transCharacteristics = sei.m_transCharacteristics;
  m_matrixCoeffs = sei.m_matrixCoeffs;
  m_inpOrderIdc = sei.m_inpOrderIdc;
  m_outFormatIdc = sei.m_outFormatIdc;
  m_outOrderIdc = sei.m_outOrderIdc;
  m_constantPatchSizeFlag = sei.m_constantPatchSizeFlag;
  m_patchWidthMinus1 = sei.m_patchWidthMinus1;
  m_patchHeightMinus1 = sei.m_patchHeightMinus1;
  m_extendedPatchWidthCdDeltaMinus1 = sei.m_extendedPatchWidthCdDeltaMinus1;
  m_extendedPatchHeightCdDeltaMinus1 = sei.m_extendedPatchHeightCdDeltaMinus1;
  m_overlap = sei.m_overlap;
  m_paddingType = sei.m_paddingType;
  m_lumaPadding = sei.m_lumaPadding;
  m_cbPadding = sei.m_cbPadding;
  m_crPadding = sei.m_crPadding;
  m_payloadLength = sei.m_payloadLength;
  m_payloadByte = sei.m_payloadByte ? new char(*sei.m_payloadByte) : nullptr;
  m_complexityInfoPresentFlag = sei.m_complexityInfoPresentFlag;
  m_applicationPurposeTagUriPresentFlag = sei.m_applicationPurposeTagUriPresentFlag;
  m_applicationPurposeTagUri = sei.m_applicationPurposeTagUri;
#if NNPFC_SCAN_TYPE_IDC
  m_scanTypeIdc = sei.m_scanTypeIdc;
#endif 
  m_forHumanViewingIdc = sei.m_forHumanViewingIdc;
  m_forMachineAnalysisIdc = sei.m_forMachineAnalysisIdc;
  m_uriTag = sei.m_uriTag;
  m_uri = sei.m_uri;
  m_parameterTypeIdc = sei.m_parameterTypeIdc;
  m_log2ParameterBitLengthMinus3 = sei.m_log2ParameterBitLengthMinus3;
  m_numParametersIdc = sei.m_numParametersIdc;
  m_numKmacOperationsIdc = sei.m_numKmacOperationsIdc;
  m_totalKilobyteSize = sei.m_totalKilobyteSize;
  m_numberInputDecodedPicturesMinus1 = sei.m_numberInputDecodedPicturesMinus1;
  m_numberInterpolatedPictures = sei.m_numberInterpolatedPictures;
  m_numberExtrapolatedPicturesMinus1 = sei.m_numberExtrapolatedPicturesMinus1;
  m_spatialExtrapolationLeftOffset = sei.m_spatialExtrapolationLeftOffset;
  m_spatialExtrapolationRightOffset = sei.m_spatialExtrapolationRightOffset;
  m_spatialExtrapolationTopOffset = sei.m_spatialExtrapolationTopOffset;
  m_spatialExtrapolationBottomOffset = sei.m_spatialExtrapolationBottomOffset;
  m_inbandPromptFlag = sei.m_inbandPromptFlag;
  m_prompt =  sei.m_prompt;
  m_inputPicOutputFlag = sei.m_inputPicOutputFlag;
#if JVET_AK0326_NNPF_SEED
  m_inbandSeedFlag = sei.m_inbandSeedFlag;
  m_seed = sei.m_seed;
#endif
}

bool SEINeuralNetworkPostFilterCharacteristics::operator == (const SEINeuralNetworkPostFilterCharacteristics& sei)
{
  bool result = 
  m_id == sei.m_id &&
  m_modeIdc == sei.m_modeIdc &&
  m_propertyPresentFlag == sei.m_propertyPresentFlag &&
  m_purpose == sei.m_purpose &&
  m_outSubCFlag == sei.m_outSubCFlag &&
  m_outSubWidthC == sei.m_outSubWidthC &&
  m_outSubHeightC == sei.m_outSubHeightC &&
  m_outColourFormatIdc == sei.m_outColourFormatIdc &&
  m_picWidthNumeratorMinus1 == sei.m_picWidthNumeratorMinus1 &&
  m_picWidthDenominatorMinus1 == sei.m_picWidthDenominatorMinus1 &&
  m_picHeightNumeratorMinus1 == sei.m_picHeightNumeratorMinus1 &&
  m_picHeightDenominatorMinus1 == sei.m_picHeightDenominatorMinus1 &&
  m_picWidthInLumaSamples == sei.m_picWidthInLumaSamples &&
  m_picHeightInLumaSamples == sei.m_picHeightInLumaSamples &&
  m_inpTensorBitDepthLumaMinus8 == sei.m_inpTensorBitDepthLumaMinus8 &&
  m_inpTensorBitDepthChromaMinus8 == sei.m_inpTensorBitDepthChromaMinus8 &&
  m_outTensorBitDepthLumaMinus8 == sei.m_outTensorBitDepthLumaMinus8 &&
  m_outTensorBitDepthChromaMinus8 == sei.m_outTensorBitDepthChromaMinus8 &&
  m_componentLastFlag == sei.m_componentLastFlag &&
  m_inpFormatIdc == sei.m_inpFormatIdc &&
  m_auxInpIdc == sei.m_auxInpIdc &&
  m_sepColDescriptionFlag == sei.m_sepColDescriptionFlag &&
  m_fullRangeFlag == sei.m_fullRangeFlag &&
  m_colPrimaries == sei.m_colPrimaries &&
  m_transCharacteristics == sei.m_transCharacteristics &&
  m_matrixCoeffs == sei.m_matrixCoeffs &&
  m_inpOrderIdc == sei.m_inpOrderIdc &&
  m_outFormatIdc == sei.m_outFormatIdc &&
  m_outOrderIdc == sei.m_outOrderIdc &&
  m_constantPatchSizeFlag == sei.m_constantPatchSizeFlag &&
  m_patchWidthMinus1 == sei.m_patchWidthMinus1 &&
  m_patchHeightMinus1 == sei.m_patchHeightMinus1 &&
  m_extendedPatchWidthCdDeltaMinus1 == sei.m_extendedPatchWidthCdDeltaMinus1 &&
  m_extendedPatchHeightCdDeltaMinus1 == sei.m_extendedPatchHeightCdDeltaMinus1 &&
  m_overlap == sei.m_overlap &&
  m_paddingType == sei.m_paddingType &&
  m_lumaPadding == sei.m_lumaPadding &&
  m_cbPadding == sei.m_cbPadding &&
  m_crPadding == sei.m_crPadding &&
  m_complexityInfoPresentFlag == sei.m_complexityInfoPresentFlag &&
  m_applicationPurposeTagUriPresentFlag == sei.m_applicationPurposeTagUriPresentFlag &&
  m_applicationPurposeTagUri == sei.m_applicationPurposeTagUri &&
  m_uriTag == sei.m_uriTag &&
  m_uri == sei.m_uri &&
  m_parameterTypeIdc == sei.m_parameterTypeIdc &&
  m_log2ParameterBitLengthMinus3 == sei.m_log2ParameterBitLengthMinus3 &&
  m_numParametersIdc == sei.m_numParametersIdc &&
  m_numKmacOperationsIdc == sei.m_numKmacOperationsIdc &&
  m_totalKilobyteSize == sei.m_totalKilobyteSize &&
  m_numberInputDecodedPicturesMinus1 == sei.m_numberInputDecodedPicturesMinus1 &&
  m_numberInterpolatedPictures == sei.m_numberInterpolatedPictures &&
  m_numberExtrapolatedPicturesMinus1 == sei.m_numberExtrapolatedPicturesMinus1 &&
  m_spatialExtrapolationLeftOffset == sei.m_spatialExtrapolationLeftOffset &&
  m_spatialExtrapolationRightOffset == sei.m_spatialExtrapolationRightOffset &&
  m_spatialExtrapolationTopOffset == sei.m_spatialExtrapolationTopOffset &&
  m_spatialExtrapolationBottomOffset == sei.m_spatialExtrapolationBottomOffset &&
#if NNPFC_SCAN_TYPE_IDC
  m_scanTypeIdc == sei.m_scanTypeIdc &&
#endif
  m_inbandPromptFlag == sei.m_inbandPromptFlag  &&
  m_prompt ==  sei.m_prompt  &&
  m_inputPicOutputFlag == sei.m_inputPicOutputFlag &&
#if JVET_AK0326_NNPF_SEED
  m_payloadLength == sei.m_payloadLength &&
  m_inbandSeedFlag == sei.m_inbandSeedFlag &&
  m_seed == sei.m_seed;
#else
  m_payloadLength == sei.m_payloadLength;
#endif

  if (m_payloadByte && sei.m_payloadByte && m_payloadLength == sei.m_payloadLength)
  {
    result &= !std::strncmp(m_payloadByte, sei.m_payloadByte, m_payloadLength);
  }
  else if ((m_payloadByte && !sei.m_payloadByte) || (!m_payloadByte && sei.m_payloadByte))
  {
    result = false;
  }

  return result;
}

SEINeuralNetworkPostFilterActivation::SEINeuralNetworkPostFilterActivation(
  const SEINeuralNetworkPostFilterActivation& sei)
{
  m_targetId = sei.m_targetId;
  m_cancelFlag = sei.m_cancelFlag;
  m_persistenceFlag = sei.m_persistenceFlag;
  m_targetBaseFlag = sei.m_targetBaseFlag;
  m_noPrevCLVSFlag = sei.m_noPrevCLVSFlag;
  m_noFollCLVSFlag = sei.m_noFollCLVSFlag;
  m_outputFlag = sei.m_outputFlag;
#if JVET_AJ0104_NNPFA_PROMPT_UPDATE
  m_promptUpdateFlag = sei.m_promptUpdateFlag;
  m_prompt = sei.m_prompt;
#endif
#if JVET_AK0326_NNPF_SEED
  m_seedUpdateFlag = sei.m_seedUpdateFlag;
  m_seed = sei.m_seed;
#endif
#if JVET_AJ0114_NNPFA_NUM_PIC_SHIFT
#if JVET_AL0075_NNPFA_SELECTED_INPUT_FLAG
  m_selectedInputFlag = sei.m_selectedInputFlag;
#endif
  m_numInputPicShift = sei.m_numInputPicShift;
#endif 
}

SEIPostFilterHint::SEIPostFilterHint(const SEIPostFilterHint& sei)
{
  m_filterHintCancelFlag = sei.m_filterHintCancelFlag;
  m_filterHintPersistenceFlag = sei.m_filterHintPersistenceFlag;
  m_filterHintSizeY = sei.m_filterHintSizeY;
  m_filterHintSizeX = sei.m_filterHintSizeX;
  m_filterHintType = sei.m_filterHintType;
  m_filterHintChromaCoeffPresentFlag = sei.m_filterHintChromaCoeffPresentFlag;
  m_filterHintValues = sei.m_filterHintValues;
}

  SEITextDescription::SEITextDescription(const SEITextDescription& sei)
  {
    m_textDescriptionID = sei.m_textDescriptionID;
    m_textCancelFlag = sei.m_textCancelFlag;
    m_textIDCancelFlag = sei.m_textIDCancelFlag;
    m_textPersistenceFlag = sei.m_textPersistenceFlag;
    m_textDescriptionPurpose = sei.m_textDescriptionPurpose;
    m_textNumStringsMinus1 = sei.m_textNumStringsMinus1;
    m_textDescriptionStringLang.resize(m_textNumStringsMinus1+1);
    m_textDescriptionString.resize(m_textNumStringsMinus1+1);
    for (int i=0; i<=m_textNumStringsMinus1; i++)
    {
      m_textDescriptionStringLang[i] = sei.m_textDescriptionStringLang[i];
      m_textDescriptionString[i] = sei.m_textDescriptionString[i];
    }
  }

SEINeuralNetworkPostFilterCharacteristics* getNnpfcWithGivenId(const SEIMessages &seiList, uint32_t id)
{
  for (auto sei : seiList)
  {
    auto *nnpfcSEI = (SEINeuralNetworkPostFilterCharacteristics *) sei;
    if (id == nnpfcSEI->m_id)
    {
      return nnpfcSEI;
    }
  }
  return nullptr;
}

SEINeuralNetworkPostFilterCharacteristics* getSuperResolutionNnpfc(const SEIMessages &seiList)
{
  for (auto sei: seiList)
  {
    auto *nnpfcSEI = (SEINeuralNetworkPostFilterCharacteristics *) sei;
    if ((nnpfcSEI->m_purpose & NNPC_PurposeType::RESOLUTION_UPSAMPLING) != 0)
    {
      return nnpfcSEI;
    }
  }
  return nullptr;
}

SEIEncoderOptimizationInfo::SEIEncoderOptimizationInfo(
  const SEIEncoderOptimizationInfo& sei)
{
  m_cancelFlag = sei.m_cancelFlag;
  m_persistenceFlag = sei.m_persistenceFlag;
  m_forHumanViewingIdc = sei.m_forHumanViewingIdc;
  m_forMachineAnalysisIdc = sei.m_forMachineAnalysisIdc;
  m_type = sei.m_type;
  m_objectBasedIdc = sei.m_objectBasedIdc;
  m_quantThresholdDelta = sei.m_quantThresholdDelta;
  m_picQuantObjectFlag = sei.m_picQuantObjectFlag;
  m_temporalResamplingTypeFlag = sei.m_temporalResamplingTypeFlag;
#if JVET_AJ0183_EOI_SEI_SRC_PIC_FLAG
  m_srcPicFlag = sei.m_srcPicFlag;
#endif
  m_numIntPics = sei.m_numIntPics;
  m_origPicDimensionsFlag = sei.m_origPicDimensionsFlag;
#if JVET_AL0123_AL0310_EOI
  m_origPicWidthMinus1 = sei.m_origPicWidthMinus1;
  m_origPicHeightMinus1 = sei.m_origPicHeightMinus1;
  m_spatialHorResamplingTypeIdc = sei.m_spatialHorResamplingTypeIdc;
  m_spatialVerResamplingTypeIdc = sei.m_spatialVerResamplingTypeIdc;
#else
  m_origPicWidth = sei.m_origPicWidth;
  m_origPicHeight = sei.m_origPicHeight;
  m_spatialResamplingTypeFlag = sei.m_spatialResamplingTypeFlag;
#endif
  m_privacyProtectionTypeIdc = sei.m_privacyProtectionTypeIdc;
  m_privacyProtectedInfoType = sei.m_privacyProtectedInfoType;

}

SEIModalityInfo::SEIModalityInfo(const SEIModalityInfo& sei)
{
  m_miCancelFlag = sei.m_miCancelFlag;
  m_miPersistenceFlag = sei.m_miPersistenceFlag;
  m_miModalityType = sei.m_miModalityType;
  m_miSpectrumRangePresentFlag = sei.m_miSpectrumRangePresentFlag;
  m_miMinWavelengthMantissa = sei.m_miMinWavelengthMantissa;
  m_miMinWavelengthExponentPlus15 = sei.m_miMinWavelengthExponentPlus15;
  m_miMaxWavelengthMantissa = sei.m_miMaxWavelengthMantissa;
  m_miMaxWavelengthExponentPlus15 = sei.m_miMaxWavelengthExponentPlus15;
}

SEIGenerativeFaceVideo::SEIGenerativeFaceVideo(const SEIGenerativeFaceVideo & sei)
{
  m_number = sei.m_number;
  m_currentid = sei.m_currentid;
  m_id = sei.m_id;
  m_cnt = sei.m_cnt;
  m_basePicFlag = sei.m_basePicFlag;
  m_nnPresentFlag = sei.m_nnPresentFlag;
  m_nnModeIdc = sei.m_nnModeIdc;
  m_nnTagURI = sei.m_nnTagURI;
  m_nnURI = sei.m_nnURI;
  m_chromaKeyInfoPresentFlag = sei.m_chromaKeyInfoPresentFlag;
  m_chromaKeyValuePresentFlag = sei.m_chromaKeyValuePresentFlag;
  m_chromaKeyValue = sei.m_chromaKeyValue;
  m_chromaKeyThrPresentFlag = sei.m_chromaKeyThrPresentFlag;
  m_chromaKeyThrValue = sei.m_chromaKeyThrValue;
  m_drivePicFusionFlag = sei.m_drivePicFusionFlag;
  m_lowConfidenceFaceParameterFlag = sei.m_lowConfidenceFaceParameterFlag;
  m_coordinatePresentFlag = sei.m_coordinatePresentFlag;
  m_coordinateQuantizationFactor = sei.m_coordinateQuantizationFactor;
  m_coordinatePredFlag = sei.m_coordinatePredFlag;
  m_3DCoordinateFlag = sei.m_3DCoordinateFlag;
  m_coordinatePointNum = sei.m_coordinatePointNum;
  m_coordinateX = sei.m_coordinateX;
  m_coordinateY = sei.m_coordinateY;
  m_coordinateZMaxValue = sei.m_coordinateZMaxValue;
  m_coordinateZ = sei.m_coordinateZ;
  m_matrixPresentFlag = sei.m_matrixPresentFlag;
  m_matrixElementPrecisionFactor = sei.m_matrixElementPrecisionFactor;
  m_matrixPredFlag = sei.m_matrixPredFlag;
  m_numMatricestonumKpsFlag = sei.m_numMatricestonumKpsFlag;
  m_numMatricesInfo = sei.m_numMatricesInfo;
  m_numMatrixType = sei.m_numMatrixType;
  m_matrixTypeIdx = sei.m_matrixTypeIdx;
  m_matrix3DSpaceFlag = sei.m_matrix3DSpaceFlag;
  m_numMatrices = sei.m_numMatrices;
  m_matrixWidth = sei.m_matrixWidth;
  m_matrixHeight = sei.m_matrixHeight;
  m_matrixElement = sei.m_matrixElement;
  m_payloadFilename = sei.m_payloadFilename;
  m_payloadLength = sei.m_payloadLength;
  m_payloadByte = sei.m_payloadByte ? new char(*sei.m_payloadByte) : nullptr;
  m_numMatricesstore = sei.m_numMatricesstore;
  m_matrixWidthstore = sei.m_matrixWidthstore;
  m_matrixHeightstore = sei.m_matrixHeightstore;
}

#if  JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
SEIAIUsageRestrictions::SEIAIUsageRestrictions(
  const SEIAIUsageRestrictions& sei)
{
  m_cancelFlag = sei.m_cancelFlag;
  m_persistenceFlag = sei.m_persistenceFlag;
  m_numRestrictionsMinus1 = sei.m_numRestrictionsMinus1;
  m_restrictions = sei.m_restrictions;
  m_contextPresentFlag = sei.m_contextPresentFlag;
  m_context = sei.m_context;
}
#endif

SEIGenerativeFaceVideoEnhancement::SEIGenerativeFaceVideoEnhancement(const SEIGenerativeFaceVideoEnhancement & sei)
{
  m_number = sei.m_number;
  m_currentid = sei.m_currentid;
  m_id = sei.m_id;
  m_gfvcnt = sei.m_gfvcnt;
  m_gfvid = sei.m_gfvid;
  m_basePicFlag = sei.m_basePicFlag;
  m_nnPresentFlag = sei.m_nnPresentFlag;
  m_nnModeIdc = sei.m_nnModeIdc;
  m_nnTagURI = sei.m_nnTagURI;
  m_nnURI = sei.m_nnURI;
  m_matrixElementPrecisionFactor = sei.m_matrixElementPrecisionFactor;   
  m_matrixPresentFlag = sei.m_matrixPresentFlag;
  m_matrixPredFlag = sei.m_matrixPredFlag;
  m_numMatrices = sei.m_numMatrices;
  m_matrixWidth = sei.m_matrixWidth;
  m_matrixHeight = sei.m_matrixHeight;
  m_matrixElement = sei.m_matrixElement;
  m_payloadFilename = sei.m_payloadFilename;
  m_payloadLength = sei.m_payloadLength;
  m_payloadByte = sei.m_payloadByte ? new char(*sei.m_payloadByte) : nullptr;
  m_pupilPresentIdx = sei.m_pupilPresentIdx;
  m_pupilCoordinatePrecisionFactor = sei.m_pupilCoordinatePrecisionFactor;
  m_pupilLeftEyeCoordinateX = sei.m_pupilLeftEyeCoordinateX;
  m_pupilLeftEyeCoordinateY = sei.m_pupilLeftEyeCoordinateY;
  m_pupilRightEyeCoordinateX = sei.m_pupilRightEyeCoordinateX;
  m_pupilRightEyeCoordinateY = sei.m_pupilRightEyeCoordinateY;
}

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
SEIPackedRegionsInfo::SEIPackedRegionsInfo(SEIPackedRegionsInfo& sei)
{
  m_cancelFlag = sei.m_cancelFlag;
  m_persistenceFlag = sei.m_persistenceFlag;
  m_numRegionsMinus1 = sei.m_numRegionsMinus1;
#if JVET_AL0324_AL0070_PRI_SEI
  m_multilayerFlag = sei.m_multilayerFlag;
#endif
  m_useMaxDimensionsFlag = sei.m_useMaxDimensionsFlag;
  m_log2UnitSize = sei.m_log2UnitSize;
  m_regionSizeLenMinus1 = sei.m_regionSizeLenMinus1;
  m_regionIdPresentFlag = sei.m_regionIdPresentFlag;
  m_targetPicParamsPresentFlag = sei.m_targetPicParamsPresentFlag;
  m_targetPicWidthMinus1 = sei.m_targetPicWidthMinus1;
  m_targetPicHeightMinus1 = sei.m_targetPicHeightMinus1;
  m_numResamplingRatiosMinus1 = sei.m_numResamplingRatiosMinus1;
  m_resamplingWidthNumMinus1 = sei.m_resamplingWidthNumMinus1;
  m_resamplingWidthDenomMinus1 = sei.m_resamplingWidthDenomMinus1;
  m_fixedAspectRatioFlag = sei.m_fixedAspectRatioFlag;
  m_resamplingHeightNumMinus1 = sei.m_resamplingHeightNumMinus1;
  m_resamplingHeightDenomMinus1 = sei.m_resamplingHeightDenomMinus1;
  m_regionId = sei.m_regionId;
#if JVET_AL0324_AL0070_PRI_SEI
  m_regionLayerId = sei.m_regionLayerId;
  m_regionIsALayerFlag = sei.m_regionIsALayerFlag;
#endif
  m_regionTopLeftInUnitsX = sei.m_regionTopLeftInUnitsX;
  m_regionTopLeftInUnitsY = sei.m_regionTopLeftInUnitsY;
  m_regionWidthInUnitsMinus1 = sei.m_regionWidthInUnitsMinus1;
  m_regionHeightInUnitsMinus1 = sei.m_regionHeightInUnitsMinus1;
  m_resamplingRatioIdx = sei.m_resamplingRatioIdx;
#if JVET_AL0324_AL0070_PRI_SEI
  m_targetRegionTopLeftInUnitsX = sei.m_targetRegionTopLeftInUnitsX;
  m_targetRegionTopLeftInUnitsY = sei.m_targetRegionTopLeftInUnitsY;
#else
  m_targetRegionTopLeftX = sei.m_targetRegionTopLeftX;
  m_targetRegionTopLeftY = sei.m_targetRegionTopLeftY;
#endif
}
#endif
