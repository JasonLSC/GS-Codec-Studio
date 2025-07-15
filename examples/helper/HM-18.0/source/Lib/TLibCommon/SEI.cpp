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

/** \file     SEI.cpp
    \brief    helper functions for SEI handling
*/

#include "CommonDef.h"
#include "SEI.h"
#include <iostream>

const std::vector<SEI::PayloadType> SEI::prefix_sei_messages({
  SEI::BUFFERING_PERIOD,
  SEI::PICTURE_TIMING,
  SEI::PAN_SCAN_RECT,
  SEI::FILLER_PAYLOAD,
  SEI::USER_DATA_REGISTERED_ITU_T_T35,
  SEI::USER_DATA_UNREGISTERED,
  SEI::RECOVERY_POINT,
  SEI::SCENE_INFO,
  SEI::PICTURE_SNAPSHOT,
  SEI::PROGRESSIVE_REFINEMENT_SEGMENT_START,
  SEI::PROGRESSIVE_REFINEMENT_SEGMENT_END,
  SEI::FILM_GRAIN_CHARACTERISTICS,
  SEI::POST_FILTER_HINT,
  SEI::TONE_MAPPING_INFO,
  SEI::FRAME_PACKING,
  SEI::DISPLAY_ORIENTATION,
  SEI::GREEN_METADATA,
  SEI::SOP_DESCRIPTION,
  SEI::ACTIVE_PARAMETER_SETS,
  SEI::DECODING_UNIT_INFO,
  SEI::TEMPORAL_LEVEL0_INDEX,
  SEI::SCALABLE_NESTING,
  SEI::REGION_REFRESH_INFO,
  SEI::NO_DISPLAY,
  SEI::TIME_CODE,
  SEI::MASTERING_DISPLAY_COLOUR_VOLUME,
  SEI::SEGM_RECT_FRAME_PACKING,
  SEI::TEMP_MOTION_CONSTRAINED_TILE_SETS,
  SEI::CHROMA_RESAMPLING_FILTER_HINT,
  SEI::KNEE_FUNCTION_INFO,
  SEI::COLOUR_REMAPPING_INFO,
  SEI::DEINTERLACE_FIELD_IDENTIFICATION,
  SEI::CONTENT_LIGHT_LEVEL_INFO,
  SEI::DEPENDENT_RAP_INDICATION,
  SEI::CODED_REGION_COMPLETION,
  SEI::ALTERNATIVE_TRANSFER_CHARACTERISTICS,
  SEI::AMBIENT_VIEWING_ENVIRONMENT
  , SEI::CONTENT_COLOUR_VOLUME
  , SEI::EQUIRECTANGULAR_PROJECTION
  , SEI::SPHERE_ROTATION
  , SEI::OMNI_VIEWPORT
  , SEI::CUBEMAP_PROJECTION
  , SEI::REGION_WISE_PACKING
  , SEI::FISHEYE_VIDEO_INFO
  , SEI::REGIONAL_NESTING
#if SHUTTER_INTERVAL_SEI_MESSAGE
  , SEI::SHUTTER_INTERVAL_INFO
#endif
});

const std::vector<SEI::PayloadType> SEI::suffix_sei_messages({
  SEI::FILLER_PAYLOAD,
  SEI::USER_DATA_REGISTERED_ITU_T_T35,
  SEI::USER_DATA_UNREGISTERED,
  SEI::PROGRESSIVE_REFINEMENT_SEGMENT_END,
  SEI::POST_FILTER_HINT,
  SEI::DECODED_PICTURE_HASH,
  SEI::CODED_REGION_COMPLETION,
});

const std::vector<SEI::PayloadType> SEI::regional_nesting_sei_messages({
  SEI::USER_DATA_REGISTERED_ITU_T_T35,
  SEI::USER_DATA_UNREGISTERED,
  SEI::FILM_GRAIN_CHARACTERISTICS,
  SEI::POST_FILTER_HINT,
  SEI::TONE_MAPPING_INFO,
  SEI::CHROMA_RESAMPLING_FILTER_HINT,
  SEI::KNEE_FUNCTION_INFO,
  SEI::COLOUR_REMAPPING_INFO,
  SEI::CONTENT_COLOUR_VOLUME,
});

SEIMessages getSeisByType(SEIMessages &seiList, SEI::PayloadType seiType)
{
  SEIMessages result;

  for (SEIMessages::iterator it=seiList.begin(); it!=seiList.end(); it++)
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


Void deleteSEIs (SEIMessages &seiList)
{
  for (SEIMessages::iterator it=seiList.begin(); it!=seiList.end(); it++)
  {
    delete (*it);
  }
  seiList.clear();
}

void SEIBufferingPeriod::copyTo (SEIBufferingPeriod& target)
{
  target.m_bpSeqParameterSetId = m_bpSeqParameterSetId;
  target.m_rapCpbParamsPresentFlag = m_rapCpbParamsPresentFlag;
  target.m_cpbDelayOffset = m_cpbDelayOffset;
  target.m_dpbDelayOffset = m_dpbDelayOffset;
  target.m_concatenationFlag = m_concatenationFlag;
  target.m_auCpbRemovalDelayDelta = m_auCpbRemovalDelayDelta;
  ::memcpy(target.m_initialCpbRemovalDelay, m_initialCpbRemovalDelay, sizeof(m_initialCpbRemovalDelay));
  ::memcpy(target.m_initialCpbRemovalDelayOffset, m_initialCpbRemovalDelayOffset, sizeof(m_initialCpbRemovalDelayOffset));
  ::memcpy(target.m_initialAltCpbRemovalDelay, m_initialAltCpbRemovalDelay, sizeof(m_initialAltCpbRemovalDelay));
  ::memcpy(target.m_initialAltCpbRemovalDelayOffset, m_initialAltCpbRemovalDelayOffset, sizeof(m_initialAltCpbRemovalDelayOffset));
}

void SEIPictureTiming::copyTo (SEIPictureTiming& target)
{
  target.m_picStruct = m_picStruct;
  target.m_sourceScanType = m_sourceScanType;
  target.m_duplicateFlag = m_duplicateFlag;

  target.m_auCpbRemovalDelay = m_auCpbRemovalDelay;
  target.m_picDpbOutputDelay = m_picDpbOutputDelay;
  target.m_picDpbOutputDuDelay = m_picDpbOutputDuDelay;
  target.m_numDecodingUnitsMinus1 = m_numDecodingUnitsMinus1;
  target.m_duCommonCpbRemovalDelayFlag = m_duCommonCpbRemovalDelayFlag;
  target.m_duCommonCpbRemovalDelayMinus1 = m_duCommonCpbRemovalDelayMinus1;

  target.m_numNalusInDuMinus1 = m_numNalusInDuMinus1;
  target.m_duCpbRemovalDelayMinus1 = m_duCpbRemovalDelayMinus1;
}

std::ostream& operator<<(std::ostream  &os, RNSEIWindow const &region)
{
  os << region.getRegionId() << " " << region.getWindowLeftOffset() <<
      region.getWindowRightOffset() << " " << region.getWindowTopOffset() << " "  <<
      region.getWindowBottomOffset() << "\n";
  return os;
}

SEIRegionalNesting::~SEIRegionalNesting()
{
  // Delete SEI messages
  for(Int i = 0; i < m_rnSeiMessages.size(); i++)
  {
    delete m_rnSeiMessages[i].m_seiMessage;
  }
}

Void SEIRegionalNesting::addRegionalSEI(RegionalSEI *regSEI)
{
  // Check if no conflict with region IDs of regions
  const RNSEIWindowVec newRegions = regSEI->getRegions();
  RNSEIWindowVec regionsToAdd;
  std::vector<UInt> listOfIndices;

  // Loop through regions to add
  for(RNSEIWindowVec::const_iterator iterNew = newRegions.begin();
            iterNew != newRegions.end(); iterNew++)
  {
    Bool addNewRegion = m_regions.empty() ? true : false;
    Bool foundRegion = false;
    // Loop through regions already present
    for(RNSEIWindowVec::const_iterator iterRef = m_regions.begin();
            !foundRegion && iterRef != m_regions.end(); iterRef++)
    {
      if( (*iterNew) == (*iterRef) )  // Check if same region present
      {
        listOfIndices.push_back((UInt)(iterRef - m_regions.begin()));  // Add index
        foundRegion = true;
      }
      else if( iterRef->checkSameID(*iterNew) )  // Check if there is a region ID class
      {
        // Two regions are different yet have same ID value; violates constraint
        std::cout << "Two different regions have the same ID; please check.\n";
        std::cout << "Region:" << (*iterNew) << "\n";
        std::cout << "Region:" << (*iterRef) << "\n";
        exit(1);
      }
    }        
    addNewRegion = !foundRegion;
    
    if(addNewRegion)
    {
      // Index is current size of the m_regions;
      listOfIndices.push_back((UInt)m_regions.size());
      m_regions.push_back((*iterNew));
    }
  }
  if(listOfIndices.empty())
  {
    std::cout << "Unable to add regions to the regional nesting SEI.\n";
    exit(1);
  }
  SEIListOfIndices seiWithListOfIndices(listOfIndices, regSEI->dissociateSEIObject());
  addRegionalSEI(seiWithListOfIndices);
}

#if JCTVC_AD0021_SEI_MANIFEST
SEIManifest::SEIManifestDescription SEIManifest::getSEIMessageDescription(const PayloadType payloadType)
{
  std::vector<PayloadType> necessary = { FRAME_PACKING, EQUIRECTANGULAR_PROJECTION  /*,GENERALIZED_CUBEMAP_PROJECTION*/
    ,SPHERE_ROTATION, REGION_WISE_PACKING };

  std::vector<PayloadType> undetermined = { USER_DATA_REGISTERED_ITU_T_T35, USER_DATA_UNREGISTERED };

  for (auto pt : necessary)
  {
    if (payloadType == pt)
    {
      return NECESSARY_SEI_MESSAGE;
    }
  }
  for (auto pt : undetermined)
  {
    if (payloadType == pt)
    {
      return UNDETERMINED_SEI_MESSAGE;
    }
  }
  return UNNECESSARY_SEI_MESSAGE;
}
#endif

#if JCTVC_AD0021_SEI_PREFIX_INDICATION
uint8_t SEIPrefixIndication::getNumsOfSeiPrefixIndications(const SEI* sei)
{
  PayloadType payloadType = sei->payloadType();

  //Unable to determine how many indicators are needed, it will be determined in xWriteSEIPrefixIndication() return 3
  std::vector<PayloadType> indicationN = { REGION_WISE_PACKING };
  // Need two indicators to finish writing the SEI prefix indication message(return 2)
  std::vector<PayloadType> indication2 = { SPHERE_ROTATION };

  for (auto pt : indicationN)
  {
    if (payloadType == pt)
    {
      return 3;
    }
  }
  for (auto pt : indication2)
  {
    if (payloadType == pt)
    {
      return 2;
    }
  }
  return 1;
}
#endif


// Static member
const TChar *SEI::getSEIMessageString(SEI::PayloadType payloadType)
{
  switch (payloadType)
  {
    case SEI::BUFFERING_PERIOD:                     return "Buffering period";
    case SEI::PICTURE_TIMING:                       return "Picture timing";
    case SEI::PAN_SCAN_RECT:                        return "Pan-scan rectangle";
    case SEI::FILLER_PAYLOAD:                       return "Filler payload";
    case SEI::USER_DATA_REGISTERED_ITU_T_T35:       return "User data registered";
    case SEI::USER_DATA_UNREGISTERED:               return "User data unregistered";
    case SEI::RECOVERY_POINT:                       return "Recovery point";
    case SEI::SCENE_INFO:                           return "Scene information";
    case SEI::PICTURE_SNAPSHOT:                     return "Picture snapshot";
    case SEI::PROGRESSIVE_REFINEMENT_SEGMENT_START: return "Progressive refinement segment start";
    case SEI::PROGRESSIVE_REFINEMENT_SEGMENT_END:   return "Progressive refinement segment end";
    case SEI::FILM_GRAIN_CHARACTERISTICS:           return "Film grain characteristics";
    case SEI::POST_FILTER_HINT:                     return "Post filter hint";
    case SEI::TONE_MAPPING_INFO:                    return "Tone mapping information";
    case SEI::KNEE_FUNCTION_INFO:                   return "Knee function information";
    case SEI::FRAME_PACKING:                        return "Frame packing arrangement";
    case SEI::DISPLAY_ORIENTATION:                  return "Display orientation";
    case SEI::GREEN_METADATA:                       return "Green metadata information";
    case SEI::SOP_DESCRIPTION:                      return "Structure of pictures information";
    case SEI::ACTIVE_PARAMETER_SETS:                return "Active parameter sets";
    case SEI::DECODING_UNIT_INFO:                   return "Decoding unit information";
    case SEI::TEMPORAL_LEVEL0_INDEX:                return "Temporal sub-layer zero index";
    case SEI::DECODED_PICTURE_HASH:                 return "Decoded picture hash";
    case SEI::SCALABLE_NESTING:                     return "Scalable nesting";
    case SEI::REGION_REFRESH_INFO:                  return "Region refresh information";
    case SEI::NO_DISPLAY:                           return "No display";
    case SEI::TIME_CODE:                            return "Time code";
    case SEI::MASTERING_DISPLAY_COLOUR_VOLUME:      return "Mastering display colour volume";
    case SEI::SEGM_RECT_FRAME_PACKING:              return "Segmented rectangular frame packing arrangement";
    case SEI::TEMP_MOTION_CONSTRAINED_TILE_SETS:    return "Temporal motion constrained tile sets";
    case SEI::CHROMA_RESAMPLING_FILTER_HINT:        return "Chroma sampling filter hint";
    case SEI::COLOUR_REMAPPING_INFO:                return "Colour remapping info";
    case SEI::DEINTERLACE_FIELD_IDENTIFICATION:     return "Deinterlace field identification";
    case SEI::CONTENT_LIGHT_LEVEL_INFO:             return "Content light level info";
    case SEI::DEPENDENT_RAP_INDICATION:             return "Dependent RAP indication";
    case SEI::CODED_REGION_COMPLETION:              return "Coded region completion";
    case SEI::ALTERNATIVE_TRANSFER_CHARACTERISTICS: return "Alternative transfer characteristics";
    case SEI::AMBIENT_VIEWING_ENVIRONMENT:          return "Ambient viewing environment";
    case SEI::CONTENT_COLOUR_VOLUME:                return "Content Colour Volume";
    case SEI::EQUIRECTANGULAR_PROJECTION:           return "Equirectangular projection";
    case SEI::SPHERE_ROTATION:                      return "Sphere rotation";
    case SEI::OMNI_VIEWPORT:                        return "Omni viewport";
    case SEI::CUBEMAP_PROJECTION:                  return "Cubemap projection";
    case SEI::REGION_WISE_PACKING:                  return "Region wise packing information";
    case SEI::FISHEYE_VIDEO_INFO:                   return "Fisheye video information";
    case SEI::REGIONAL_NESTING:                     return "Regional nesting";
#if MCTS_EXTRACTION
    case SEI::MCTS_EXTRACTION_INFO_SET:             return "MCTS extraction information";
#endif
#if JCTVC_AD0021_SEI_MANIFEST
    case SEI::SEI_MANIFEST:                         return "SEI manifest";
#endif
#if JCTVC_AD0021_SEI_PREFIX_INDICATION
    case SEI::SEI_PREFIX_INDICATION:                return "SEI prefix indication";
#endif
    case SEI::ANNOTATED_REGIONS:                    return "Annotated Region";
#if SHUTTER_INTERVAL_SEI_MESSAGE
    case SEI::SHUTTER_INTERVAL_INFO:                return "Shutter interval information";
#endif
    default:                                        return "Unknown";
  }
}
