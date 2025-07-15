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

/**
 \file     SEIread.cpp
 \brief    reading functionality for SEI messages
 */

#include "TLibCommon/CommonDef.h"
#include "TLibCommon/TComBitStream.h"
#include "TLibCommon/SEI.h"
#include "TLibCommon/TComSlice.h"
#include "SyntaxElementParser.h"
#include "SEIread.h"
#include "TLibCommon/TComPicYuv.h"
#include <iomanip>


//! \ingroup TLibDecoder
//! \{


#if ENC_DEC_TRACE
Void  xTraceSEIHeader()
{
  fprintf( g_hTrace, "=========== SEI message ===========\n");
}

Void  xTraceSEIMessageType(SEI::PayloadType payloadType)
{
  fprintf( g_hTrace, "=========== %s SEI message ===========\n", SEI::getSEIMessageString(payloadType));
}
#endif

Void SEIReader::sei_read_scode(std::ostream *pOS, UInt uiLength, Int& ruiCode, const TChar *pSymbolName)
{
  READ_SCODE(uiLength, ruiCode, pSymbolName);
  if (pOS)
  {
    (*pOS) << "  " << std::setw(55) << pSymbolName << ": " << ruiCode << "\n";
  }
}

Void SEIReader::sei_read_code(std::ostream *pOS, UInt uiLength, UInt& ruiCode, const TChar *pSymbolName)
{
  READ_CODE(uiLength, ruiCode, pSymbolName);
  if (pOS)
  {
    (*pOS) << "  " << std::setw(55) << pSymbolName << ": " << ruiCode << "\n";
  }
}

Void SEIReader::sei_read_uvlc(std::ostream *pOS, UInt& ruiCode, const TChar *pSymbolName)
{
  READ_UVLC(ruiCode, pSymbolName);
  if (pOS)
  {
    (*pOS) << "  " << std::setw(55) << pSymbolName << ": " << ruiCode << "\n";
  }
}

Void SEIReader::sei_read_svlc(std::ostream *pOS, Int& ruiCode, const TChar *pSymbolName)
{
  READ_SVLC(ruiCode, pSymbolName);
  if (pOS)
  {
    (*pOS) << "  " << std::setw(55) << pSymbolName << ": " << ruiCode << "\n";
  }
}

Void SEIReader::sei_read_flag(std::ostream *pOS, UInt& ruiCode, const TChar *pSymbolName)
{
  READ_FLAG(ruiCode, pSymbolName);
  if (pOS)
  {
    (*pOS) << "  " << std::setw(55) << pSymbolName << ": " << (ruiCode?1:0) << "\n";
  }
}

static inline Void output_sei_message_header(SEI &sei, std::ostream *pDecodedMessageOutputStream, UInt payloadSize)
{
  if (pDecodedMessageOutputStream)
  {
    std::string seiMessageHdr(SEI::getSEIMessageString(sei.payloadType())); seiMessageHdr+=" SEI message";
    (*pDecodedMessageOutputStream) << std::setfill('-') << std::setw((int)seiMessageHdr.size()) << "-" << std::setfill(' ') << "\n" << seiMessageHdr << " (" << payloadSize << " bytes)"<< "\n";
  }
}

#undef READ_SCODE
#undef READ_CODE
#undef READ_SVLC
#undef READ_UVLC
#undef READ_FLAG


/**
 * unmarshal a single SEI message from bitstream bs
 */
Void SEIReader::parseSEImessage(TComInputBitstream* bs, SEIMessages& seis, const NalUnitType nalUnitType, const TComSPS *sps, std::ostream *pDecodedMessageOutputStream)
{
  setBitstream(bs);

  assert(!m_pcBitstream->getNumBitsUntilByteAligned());
  do
  {
    if(nalUnitType == NAL_UNIT_PREFIX_SEI)
    {
      xReadSEImessage(seis, nalUnitType, sps, pDecodedMessageOutputStream, SEI::prefix_sei_messages, std::string("prefix SEI"));
    }
    else if (nalUnitType == NAL_UNIT_SUFFIX_SEI)
    {
      xReadSEImessage(seis, nalUnitType, sps, pDecodedMessageOutputStream, SEI::suffix_sei_messages, std::string("suffix SEI"));
    }
    else
    {
      std::cerr << "Unsupported SEI NAL unit type '" << nalUnitType << "'" << std::endl;
      exit(EXIT_FAILURE);
    }

    /* SEI messages are an integer number of bytes, something has failed
    * in the parsing if bitstream not byte-aligned */
    assert(!m_pcBitstream->getNumBitsUntilByteAligned());
  }
  while (m_pcBitstream->getNumBitsLeft() > 8);

  xReadRbspTrailingBits();
}
Void SEIReader::xReadSEIPayloadData(Int const payloadType, Int const payloadSize, SEI *&sei, const NalUnitType nalUnitType, const TComSPS *sps, 
  std::ostream *pDecodedMessageOutputStream, std::string const &typeName)
{
  switch(payloadType)
  {
    case SEI::BUFFERING_PERIOD:
      if (!sps)
      {
        printf ("Warning: Found Buffering period SEI message, but no active SPS is available. Ignoring.");
      }
      else
      {
        sei = new SEIBufferingPeriod;
        xParseSEIBufferingPeriod((SEIBufferingPeriod&) *sei, payloadSize, sps, pDecodedMessageOutputStream);
      }
      break;
    case SEI::PICTURE_TIMING:
      if (!sps)
      {
        printf ("Warning: Found Picture timing SEI message, but no active SPS is available. Ignoring.");
      }
      else
      {
        sei = new SEIPictureTiming;
        xParseSEIPictureTiming((SEIPictureTiming&)*sei, payloadSize, sps, pDecodedMessageOutputStream);
      }
      break;
    case SEI::PAN_SCAN_RECT:
      sei = new SEIPanScanRect;
      xParseSEIPanScanRect((SEIPanScanRect&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::FILLER_PAYLOAD:
      sei = new SEIFillerPayload;
      xParseSEIFillerPayload((SEIFillerPayload&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::USER_DATA_REGISTERED_ITU_T_T35:
      sei = new SEIUserDataRegistered;
      xParseSEIUserDataRegistered((SEIUserDataRegistered&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::USER_DATA_UNREGISTERED:
      sei = new SEIUserDataUnregistered;
      xParseSEIUserDataUnregistered((SEIUserDataUnregistered&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::RECOVERY_POINT:
      sei = new SEIRecoveryPoint;
      xParseSEIRecoveryPoint((SEIRecoveryPoint&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::SCENE_INFO:
      sei = new SEISceneInfo;
      xParseSEISceneInfo((SEISceneInfo&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::PICTURE_SNAPSHOT:
      sei = new SEIPictureSnapshot;
      xParseSEIPictureSnapshot((SEIPictureSnapshot&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::PROGRESSIVE_REFINEMENT_SEGMENT_START:
      sei = new SEIProgressiveRefinementSegmentStart;
      xParseSEIProgressiveRefinementSegmentStart((SEIProgressiveRefinementSegmentStart&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::PROGRESSIVE_REFINEMENT_SEGMENT_END:
      sei = new SEIProgressiveRefinementSegmentEnd;
      xParseSEIProgressiveRefinementSegmentEnd((SEIProgressiveRefinementSegmentEnd&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::FILM_GRAIN_CHARACTERISTICS:
      sei = new SEIFilmGrainCharacteristics;
      xParseSEIFilmGrainCharacteristics((SEIFilmGrainCharacteristics&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::POST_FILTER_HINT:
      if (!sps)
      {
        printf ("Warning: post filter hint SEI message, but no active SPS is available. Ignoring.");
      }
      else
      {
        sei = new SEIPostFilterHint;
        xParseSEIPostFilterHint((SEIPostFilterHint&) *sei, payloadSize, sps, pDecodedMessageOutputStream);
      }
      break;
    case SEI::TONE_MAPPING_INFO:
      sei = new SEIToneMappingInfo;
      xParseSEIToneMappingInfo((SEIToneMappingInfo&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::FRAME_PACKING:
      sei = new SEIFramePacking;
      xParseSEIFramePacking((SEIFramePacking&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::DISPLAY_ORIENTATION:
      sei = new SEIDisplayOrientation;
      xParseSEIDisplayOrientation((SEIDisplayOrientation&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::GREEN_METADATA:
      sei = new SEIGreenMetadataInfo;
      xParseSEIGreenMetadataInfo((SEIGreenMetadataInfo&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::SOP_DESCRIPTION:
      sei = new SEISOPDescription;
      xParseSEISOPDescription((SEISOPDescription&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::DECODED_PICTURE_HASH:
      sei = new SEIDecodedPictureHash;
      xParseSEIDecodedPictureHash((SEIDecodedPictureHash&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::ACTIVE_PARAMETER_SETS:
      sei = new SEIActiveParameterSets;
      xParseSEIActiveParameterSets((SEIActiveParameterSets&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::DECODING_UNIT_INFO:
      if (!sps)
      {
        printf ("Warning: Found Decoding unit SEI message, but no active SPS is available. Ignoring.");
      }
      else
      {
        sei = new SEIDecodingUnitInfo;
        xParseSEIDecodingUnitInfo((SEIDecodingUnitInfo&) *sei, payloadSize, sps, pDecodedMessageOutputStream);
      }
      break;
    case SEI::TEMPORAL_LEVEL0_INDEX:
      sei = new SEITemporalLevel0Index;
      xParseSEITemporalLevel0Index((SEITemporalLevel0Index&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::SCALABLE_NESTING:
      sei = new SEIScalableNesting;
      xParseSEIScalableNesting((SEIScalableNesting&) *sei, nalUnitType, payloadSize, sps, pDecodedMessageOutputStream);
      break;
    case SEI::REGION_REFRESH_INFO:
      sei = new SEIRegionRefreshInfo;
      xParseSEIRegionRefreshInfo((SEIRegionRefreshInfo&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::NO_DISPLAY:
      sei = new SEINoDisplay;
      xParseSEINoDisplay((SEINoDisplay&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::TIME_CODE:
      sei = new SEITimeCode;
      xParseSEITimeCode((SEITimeCode&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::MASTERING_DISPLAY_COLOUR_VOLUME:
      sei = new SEIMasteringDisplayColourVolume;
      xParseSEIMasteringDisplayColourVolume((SEIMasteringDisplayColourVolume&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::SEGM_RECT_FRAME_PACKING:
      sei = new SEISegmentedRectFramePacking;
      xParseSEISegmentedRectFramePacking((SEISegmentedRectFramePacking&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::TEMP_MOTION_CONSTRAINED_TILE_SETS:
      sei = new SEITempMotionConstrainedTileSets;
      xParseSEITempMotionConstraintsTileSets((SEITempMotionConstrainedTileSets&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
#if MCTS_EXTRACTION
    case SEI::MCTS_EXTRACTION_INFO_SET:
      sei = new SEIMCTSExtractionInfoSet;
      xParseSEIMCTSExtractionInfoSet((SEIMCTSExtractionInfoSet&)*sei, payloadSize, pDecodedMessageOutputStream);
      break;
#endif
    case SEI::CHROMA_RESAMPLING_FILTER_HINT:
      sei = new SEIChromaResamplingFilterHint;
      xParseSEIChromaResamplingFilterHint((SEIChromaResamplingFilterHint&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::KNEE_FUNCTION_INFO:
      sei = new SEIKneeFunctionInfo;
      xParseSEIKneeFunctionInfo((SEIKneeFunctionInfo&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::COLOUR_REMAPPING_INFO:
      sei = new SEIColourRemappingInfo;
      xParseSEIColourRemappingInfo((SEIColourRemappingInfo&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::DEINTERLACE_FIELD_IDENTIFICATION:
      sei = new SEIDeinterlaceFieldIdentification;
      xParseSEIDeinterlaceFieldIdentification((SEIDeinterlaceFieldIdentification&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::CONTENT_LIGHT_LEVEL_INFO:
      sei = new SEIContentLightLevelInfo;
      xParseSEIContentLightLevelInfo((SEIContentLightLevelInfo&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::DEPENDENT_RAP_INDICATION:
      sei = new SEIDependentRAPIndication;
      xParseSEIDependentRAPIndication((SEIDependentRAPIndication&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::CODED_REGION_COMPLETION:
      sei = new SEICodedRegionCompletion;
      xParseSEICodedRegionCompletion((SEICodedRegionCompletion&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::ALTERNATIVE_TRANSFER_CHARACTERISTICS:
      sei = new SEIAlternativeTransferCharacteristics;
      xParseSEIAlternativeTransferCharacteristics((SEIAlternativeTransferCharacteristics&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::AMBIENT_VIEWING_ENVIRONMENT:
      sei = new SEIAmbientViewingEnvironment;
      xParseSEIAmbientViewingEnvironment((SEIAmbientViewingEnvironment&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::CONTENT_COLOUR_VOLUME:
      sei = new SEIContentColourVolume;
      xParseSEIContentColourVolume((SEIContentColourVolume&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::EQUIRECTANGULAR_PROJECTION:
      sei = new SEIEquirectangularProjection;
      xParseSEIEquirectangularProjection((SEIEquirectangularProjection&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::SPHERE_ROTATION:
      sei = new SEISphereRotation;
      xParseSEISphereRotation((SEISphereRotation&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::OMNI_VIEWPORT:
      sei = new SEIOmniViewport;
      xParseSEIOmniViewport((SEIOmniViewport&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::CUBEMAP_PROJECTION:
      sei = new SEICubemapProjection;
      xParseSEICubemapProjection((SEICubemapProjection&)*sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::REGION_WISE_PACKING:
      sei = new SEIRegionWisePacking;
      xParseSEIRegionWisePacking((SEIRegionWisePacking&) *sei, payloadSize, pDecodedMessageOutputStream);
      break;

    case SEI::ANNOTATED_REGIONS:
      sei = new SEIAnnotatedRegions;
      xParseSEIAnnotatedRegions((SEIAnnotatedRegions&)*sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::FISHEYE_VIDEO_INFO:
      sei = new SEIFisheyeVideoInfo;
      xParseSEIFisheyeVideoInfo((SEIFisheyeVideoInfo&)*sei, payloadSize, pDecodedMessageOutputStream);
      break;
    case SEI::REGIONAL_NESTING:
      sei = new SEIRegionalNesting;
      xParseSEIRegionalNesting((SEIRegionalNesting&) *sei, payloadSize, sps, pDecodedMessageOutputStream);
      break;
#if SHUTTER_INTERVAL_SEI_MESSAGE
    case SEI::SHUTTER_INTERVAL_INFO:
      sei = new SEIShutterIntervalInfo;
      xParseSEIShutterInterval((SEIShutterIntervalInfo&)*sei, payloadSize, pDecodedMessageOutputStream);
      break;
#endif
#if JCTVC_AD0021_SEI_MANIFEST
    case SEI::SEI_MANIFEST:
      sei = new SEIManifest;
      xParseSEISEIManifest((SEIManifest&)*sei, payloadSize, pDecodedMessageOutputStream);
      break;
#endif
#if JCTVC_AD0021_SEI_PREFIX_INDICATION
    case SEI::SEI_PREFIX_INDICATION:
      sei = new SEIPrefixIndication;
      xParseSEISEIPrefixIndication((SEIPrefixIndication&)*sei, payloadSize, pDecodedMessageOutputStream);
      break;
#endif
    default:
      for (UInt i = 0; i < payloadSize; i++)
      {
        UInt seiByte;
        std::string msg = std::string("unknown ")+typeName+std::string(" payload byte");
        sei_read_code (NULL, 8, seiByte, msg.c_str());
      }
      printf ("Unknown prefix SEI message (payloadType = %d) was found!\n", payloadType);
      if (pDecodedMessageOutputStream)
      {
        (*pDecodedMessageOutputStream) << "Unknown "<< typeName << " message (payloadType = " << payloadType << ") was found!\n";
      }
      break;
    }
}

Void SEIReader::xReadSEImessage(SEIMessages& seis, const NalUnitType nalUnitType, const TComSPS *sps, std::ostream *pDecodedMessageOutputStream, const vector<SEI::PayloadType>& allowedSeiTypes, std::string const &typeName)
{
#if ENC_DEC_TRACE
  xTraceSEIHeader();
#endif
  Int payloadType = 0;
  UInt val = 0;

  do
  {
    sei_read_code(NULL, 8, val, "payload_type");
    payloadType += val;
  } while (val==0xFF);

  UInt payloadSize = 0;
  do
  {
    sei_read_code(NULL, 8, val, "payload_size");
    payloadSize += val;
  } while (val==0xFF);

#if ENC_DEC_TRACE
  xTraceSEIMessageType((SEI::PayloadType)payloadType);
#endif

  /* extract the payload for this single SEI message.
   * This allows greater safety in erroneous parsing of an SEI message
   * from affecting subsequent messages.
   * After parsing the payload, bs needs to be restored as the primary
   * bitstream.
   */
  TComInputBitstream *bs = getBitstream();
  setBitstream(bs->extractSubstream(payloadSize * 8));

  SEI *sei = NULL;

  if (std::find(allowedSeiTypes.begin(), allowedSeiTypes.begin(), payloadType) !=  allowedSeiTypes.end())
  {
    xReadSEIPayloadData(payloadType, payloadSize, sei, nalUnitType, sps, pDecodedMessageOutputStream, typeName);
  } 
  else
  {
    for (UInt i = 0; i < payloadSize; i++)
    {
      UInt seiByte;
      sei_read_code (NULL, 8, seiByte, "unknown SEI payload byte");
    }
    printf ("Unknown SEI message (payloadType = %d) was found!\n", payloadType);
    if (pDecodedMessageOutputStream)
    {
      (*pDecodedMessageOutputStream) << "Unknown SEI message (payloadType = " << payloadType << ") was found!\n";
    }
  }

  if (sei != NULL)
  {
    seis.push_back(sei);
  }

  /* By definition the underlying bitstream terminates in a byte-aligned manner.
   * 1. Extract all bar the last MIN(bitsremaining,nine) bits as reserved_payload_extension_data
   * 2. Examine the final 8 bits to determine the payload_bit_equal_to_one marker
   * 3. Extract the remainingreserved_payload_extension_data bits.
   *
   * If there are fewer than 9 bits available, extract them.
   */
  Int payloadBitsRemaining = getBitstream()->getNumBitsLeft();
  if (payloadBitsRemaining) /* more_data_in_payload() */
  {
    for (; payloadBitsRemaining > 9; payloadBitsRemaining--)
    {
      UInt reservedPayloadExtensionData;
      sei_read_code ( pDecodedMessageOutputStream, 1, reservedPayloadExtensionData, "reserved_payload_extension_data");
    }

    /* 2 */
    Int finalBits = getBitstream()->peekBits(payloadBitsRemaining);
    Int finalPayloadBits = 0;
    for (Int mask = 0xff; finalBits & (mask >> finalPayloadBits); finalPayloadBits++)
    {
      continue;
    }

    /* 3 */
    for (; payloadBitsRemaining > 9 - finalPayloadBits; payloadBitsRemaining--)
    {
      UInt reservedPayloadExtensionData;
      sei_read_flag ( 0, reservedPayloadExtensionData, "reserved_payload_extension_data");
    }

    UInt dummy;
    sei_read_flag( 0, dummy, "payload_bit_equal_to_one"); payloadBitsRemaining--;
    while (payloadBitsRemaining)
    {
      sei_read_flag( 0, dummy, "payload_bit_equal_to_zero"); payloadBitsRemaining--;
    }
  }

  /* restore primary bitstream for sei_message */
  delete getBitstream();
  setBitstream(bs);
}


Void SEIReader::xParseSEIBufferingPeriod(SEIBufferingPeriod& sei, UInt payloadSize, const TComSPS *sps, std::ostream *pDecodedMessageOutputStream)
{
  Int i, nalOrVcl;
  UInt code;

  const TComVUI *pVUI = sps->getVuiParameters();
  const TComHRD *pHRD = pVUI->getHrdParameters();

  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_uvlc( pDecodedMessageOutputStream, code, "bp_seq_parameter_set_id" );                         sei.m_bpSeqParameterSetId     = code;
  if( !pHRD->getSubPicCpbParamsPresentFlag() )
  {
    sei_read_flag( pDecodedMessageOutputStream, code, "irap_cpb_params_present_flag" );                   sei.m_rapCpbParamsPresentFlag = code;
  }
  if( sei.m_rapCpbParamsPresentFlag )
  {
    sei_read_code( pDecodedMessageOutputStream, pHRD->getCpbRemovalDelayLengthMinus1() + 1, code, "cpb_delay_offset" );      sei.m_cpbDelayOffset = code;
    sei_read_code( pDecodedMessageOutputStream, pHRD->getDpbOutputDelayLengthMinus1()  + 1, code, "dpb_delay_offset" );      sei.m_dpbDelayOffset = code;
  }

  //read splicing flag and cpb_removal_delay_delta
  sei_read_flag( pDecodedMessageOutputStream, code, "concatenation_flag");
  sei.m_concatenationFlag = code;
  sei_read_code( pDecodedMessageOutputStream, ( pHRD->getCpbRemovalDelayLengthMinus1() + 1 ), code, "au_cpb_removal_delay_delta_minus1" );
  sei.m_auCpbRemovalDelayDelta = code + 1;

  for( nalOrVcl = 0; nalOrVcl < 2; nalOrVcl ++ )
  {
    if( ( ( nalOrVcl == 0 ) && ( pHRD->getNalHrdParametersPresentFlag() ) ) ||
        ( ( nalOrVcl == 1 ) && ( pHRD->getVclHrdParametersPresentFlag() ) ) )
    {
      for( i = 0; i < ( pHRD->getCpbCntMinus1( 0 ) + 1 ); i ++ )
      {
        sei_read_code( pDecodedMessageOutputStream, ( pHRD->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, nalOrVcl?"vcl_initial_cpb_removal_delay":"nal_initial_cpb_removal_delay" );
        sei.m_initialCpbRemovalDelay[i][nalOrVcl] = code;
        sei_read_code( pDecodedMessageOutputStream, ( pHRD->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, nalOrVcl?"vcl_initial_cpb_removal_offset":"nal_initial_cpb_removal_offset" );
        sei.m_initialCpbRemovalDelayOffset[i][nalOrVcl] = code;
        if( pHRD->getSubPicCpbParamsPresentFlag() || sei.m_rapCpbParamsPresentFlag )
        {
          sei_read_code( pDecodedMessageOutputStream, ( pHRD->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, nalOrVcl?"vcl_initial_alt_cpb_removal_delay":"nal_initial_alt_cpb_removal_delay" );
          sei.m_initialAltCpbRemovalDelay[i][nalOrVcl] = code;
          sei_read_code( pDecodedMessageOutputStream, ( pHRD->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, nalOrVcl?"vcl_initial_alt_cpb_removal_offset":"nal_initial_alt_cpb_removal_offset" );
          sei.m_initialAltCpbRemovalDelayOffset[i][nalOrVcl] = code;
        }
      }
    }
  }
}


Void SEIReader::xParseSEIPictureTiming(SEIPictureTiming& sei, UInt payloadSize, const TComSPS *sps, std::ostream *pDecodedMessageOutputStream)
{
  Int i;
  UInt code;

  const TComVUI *vui = sps->getVuiParameters();
  const TComHRD *hrd = vui->getHrdParameters();
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  if( vui->getFrameFieldInfoPresentFlag() )
  {
    sei_read_code( pDecodedMessageOutputStream, 4, code, "pic_struct" );             sei.m_picStruct            = code;
    sei_read_code( pDecodedMessageOutputStream, 2, code, "source_scan_type" );       sei.m_sourceScanType       = code;
    sei_read_flag( pDecodedMessageOutputStream,    code, "duplicate_flag" );         sei.m_duplicateFlag        = (code == 1);
  }

  if( hrd->getCpbDpbDelaysPresentFlag())
  {
    sei_read_code( pDecodedMessageOutputStream, ( hrd->getCpbRemovalDelayLengthMinus1() + 1 ), code, "au_cpb_removal_delay_minus1" );
    sei.m_auCpbRemovalDelay = code + 1;
    sei_read_code( pDecodedMessageOutputStream, ( hrd->getDpbOutputDelayLengthMinus1() + 1 ), code, "pic_dpb_output_delay" );
    sei.m_picDpbOutputDelay = code;

    if(hrd->getSubPicCpbParamsPresentFlag())
    {
      sei_read_code( pDecodedMessageOutputStream, hrd->getDpbOutputDelayDuLengthMinus1()+1, code, "pic_dpb_output_du_delay" );
      sei.m_picDpbOutputDuDelay = code;
    }

    if( hrd->getSubPicCpbParamsPresentFlag() && hrd->getSubPicCpbParamsInPicTimingSEIFlag() )
    {
      sei_read_uvlc( pDecodedMessageOutputStream, code, "num_decoding_units_minus1");
      sei.m_numDecodingUnitsMinus1 = code;
      sei_read_flag( pDecodedMessageOutputStream, code, "du_common_cpb_removal_delay_flag" );
      sei.m_duCommonCpbRemovalDelayFlag = code;
      if( sei.m_duCommonCpbRemovalDelayFlag )
      {
        sei_read_code( pDecodedMessageOutputStream, ( hrd->getDuCpbRemovalDelayLengthMinus1() + 1 ), code, "du_common_cpb_removal_delay_increment_minus1" );
        sei.m_duCommonCpbRemovalDelayMinus1 = code;
      }
      sei.m_numNalusInDuMinus1.resize(sei.m_numDecodingUnitsMinus1 + 1 );
      sei.m_duCpbRemovalDelayMinus1.resize( sei.m_numDecodingUnitsMinus1 + 1 );

      for( i = 0; i <= sei.m_numDecodingUnitsMinus1; i ++ )
      {
        sei_read_uvlc( pDecodedMessageOutputStream, code, "num_nalus_in_du_minus1[i]");
        sei.m_numNalusInDuMinus1[ i ] = code;
        if( ( !sei.m_duCommonCpbRemovalDelayFlag ) && ( i < sei.m_numDecodingUnitsMinus1 ) )
        {
          sei_read_code( pDecodedMessageOutputStream, ( hrd->getDuCpbRemovalDelayLengthMinus1() + 1 ), code, "du_cpb_removal_delay_minus1[i]" );
          sei.m_duCpbRemovalDelayMinus1[ i ] = code;
        }
      }
    }
  }
}


Void SEIReader::xParseSEIPanScanRect(SEIPanScanRect& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_uvlc( pDecodedMessageOutputStream, code, "pan_scan_rect_id" );          sei.m_panScanRectId = code;
  sei_read_flag( pDecodedMessageOutputStream, code, "pan_scan_rect_cancel_flag" ); sei.m_panScanRectCancelFlag = code!=0;
  if (!sei.m_panScanRectCancelFlag)
  {
    UInt numRegions;
    sei_read_uvlc( pDecodedMessageOutputStream, numRegions, "pan_scan_cnt_minus1" ); numRegions++;
    sei.m_panScanRectRegions.resize(numRegions);
    for(UInt region=0; region<numRegions; region++)
    {
      SEIPanScanRect::PanScanRect &rect=sei.m_panScanRectRegions[region];
      Int  i;
      sei_read_svlc( pDecodedMessageOutputStream, i, "pan_scan_rect_left_offset[i]" );   rect.leftOffset   = i;
      sei_read_svlc( pDecodedMessageOutputStream, i, "pan_scan_rect_right_offset[i]" );  rect.rightOffset  = i;
      sei_read_svlc( pDecodedMessageOutputStream, i, "pan_scan_rect_top_offset[i]" );    rect.topOffset    = i;
      sei_read_svlc( pDecodedMessageOutputStream, i, "pan_scan_rect_bottom_offset[i]" ); rect.bottomOffset = i;
    }
    sei_read_flag( pDecodedMessageOutputStream, code, "pan_scan_rect_persistence_flag" ); sei.m_panScanRectPersistenceFlag = code!=0;
  }
  else
  {
    sei.m_panScanRectRegions.clear();
    sei.m_panScanRectPersistenceFlag=false;
  }
}


Void SEIReader::xParseSEIFillerPayload(SEIFillerPayload& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei.m_numFillerFFBytes = payloadSize;
  Bool allBytesWereFF=true;
  for(UInt k=0; k<payloadSize; k++)
  {
    UInt code;
    sei_read_code( NULL, 8, code, "ff_byte" );
    if (code!=0xff) allBytesWereFF=false;
  }
  if (pDecodedMessageOutputStream && !allBytesWereFF)
  {
    (*pDecodedMessageOutputStream) << "  not all filler payload bytes were 0xff\n";
  }
}


Void SEIReader::xParseSEIUserDataRegistered(SEIUserDataRegistered& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  UInt code;
  assert(payloadSize>0);
  sei_read_code( pDecodedMessageOutputStream, 8, code, "itu_t_t35_country_code" ); payloadSize--;
  if (code == 255)
  {
    assert(payloadSize>0);
    sei_read_code( pDecodedMessageOutputStream, 8, code, "itu_t_t35_country_code_extension_byte" ); payloadSize--;
    code+=255;
  }
  sei.m_ituCountryCode = code;
  sei.m_userData.resize(payloadSize);
  for (UInt i = 0; i < sei.m_userData.size(); i++)
  {
    sei_read_code( NULL, 8, code, "itu_t_t35_payload_byte" );
    sei.m_userData[i] = code;
  }
  if (pDecodedMessageOutputStream)
  {
    (*pDecodedMessageOutputStream) << "  itu_t_t35 payload size: " << sei.m_userData.size() << "\n";
  }
}


Void SEIReader::xParseSEIUserDataUnregistered(SEIUserDataUnregistered &sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  assert(payloadSize >= ISO_IEC_11578_LEN);
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  for (UInt i = 0; i < ISO_IEC_11578_LEN; i++)
  {
    sei_read_code( pDecodedMessageOutputStream, 8, val, "uuid_iso_iec_11578");
    sei.m_uuid_iso_iec_11578[i] = val;
  }

  sei.m_userData.resize(payloadSize - ISO_IEC_11578_LEN);
  for (UInt i = 0; i < sei.m_userData.size(); i++)
  {
    sei_read_code( NULL, 8, val, "user_data_payload_byte" );
    sei.m_userData[i] = val;
  }
  if (pDecodedMessageOutputStream)
  {
    (*pDecodedMessageOutputStream) << "  User data payload size: " << sei.m_userData.size() << "\n";
  }
}


Void SEIReader::xParseSEIRecoveryPoint(SEIRecoveryPoint& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  Int  iCode;
  UInt uiCode;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_svlc( pDecodedMessageOutputStream, iCode,  "recovery_poc_cnt" );      sei.m_recoveryPocCnt     = iCode;
  sei_read_flag( pDecodedMessageOutputStream, uiCode, "exact_matching_flag" );   sei.m_exactMatchingFlag  = uiCode;
  sei_read_flag( pDecodedMessageOutputStream, uiCode, "broken_link_flag" );      sei.m_brokenLinkFlag     = uiCode;
}


Void SEIReader::xParseSEISceneInfo(SEISceneInfo& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_flag( pDecodedMessageOutputStream, code, "scene_info_present_flag" ); sei.m_bSceneInfoPresentFlag = code!=0;
  if (sei.m_bSceneInfoPresentFlag)
  {
    sei_read_flag( pDecodedMessageOutputStream, code, "prev_scene_id_valid_flag" ); sei.m_bPrevSceneIdValidFlag = code!=0;
    sei_read_uvlc( pDecodedMessageOutputStream, code, "scene_id" );                 sei.m_sceneId = code;
    sei_read_uvlc( pDecodedMessageOutputStream, code, "scene_transition_type" );    sei.m_sceneTransitionType = code;
    if (sei.m_sceneTransitionType > 3)
    {
      sei_read_uvlc( pDecodedMessageOutputStream, code, "second_scene_id" );        sei.m_secondSceneId = code;
    }
    else
    {
      sei.m_secondSceneId = 0; // set to known value.
    }
  }
}


Void SEIReader::xParseSEIPictureSnapshot(SEIPictureSnapshot& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_uvlc( pDecodedMessageOutputStream, code, "snapshot_id" ); sei.m_snapshotId = code;
}


Void SEIReader::xParseSEIProgressiveRefinementSegmentStart(SEIProgressiveRefinementSegmentStart& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_uvlc( pDecodedMessageOutputStream, code, "progressive_refinement_id" ); sei.m_progressiveRefinementId = code;
  sei_read_uvlc( pDecodedMessageOutputStream, code, "pic_order_cnt_delta" );       sei.m_picOrderCntDelta = code;
}


Void SEIReader::xParseSEIProgressiveRefinementSegmentEnd(SEIProgressiveRefinementSegmentEnd& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_uvlc( pDecodedMessageOutputStream, code, "progressive_refinement_id" ); sei.m_progressiveRefinementId = code;
}


Void SEIReader::xParseSEIFilmGrainCharacteristics(SEIFilmGrainCharacteristics& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_flag( pDecodedMessageOutputStream, code, "film_grain_characteristics_cancel_flag" );     sei.m_filmGrainCharacteristicsCancelFlag = code!=0;
  if (!sei.m_filmGrainCharacteristicsCancelFlag)
  {
    sei_read_code( pDecodedMessageOutputStream, 2, code, "film_grain_model_id" );                   sei.m_filmGrainModelId = code;
    sei_read_flag( pDecodedMessageOutputStream,    code, "separate_colour_description_present_flag" ); sei.m_separateColourDescriptionPresentFlag = code!=0;
    if (sei.m_separateColourDescriptionPresentFlag)
    {
      sei_read_code( pDecodedMessageOutputStream, 3, code, "film_grain_bit_depth_luma_minus8" );    sei.m_filmGrainBitDepthLumaMinus8      = code;
      sei_read_code( pDecodedMessageOutputStream, 3, code, "film_grain_bit_depth_chroma_minus8" );  sei.m_filmGrainBitDepthChromaMinus8    = code;
      sei_read_flag( pDecodedMessageOutputStream,    code, "film_grain_full_range_flag" );          sei.m_filmGrainFullRangeFlag           = code!=0;
      sei_read_code( pDecodedMessageOutputStream, 8, code, "film_grain_colour_primaries" );         sei.m_filmGrainColourPrimaries         = code;
      sei_read_code( pDecodedMessageOutputStream, 8, code, "film_grain_transfer_characteristics" ); sei.m_filmGrainTransferCharacteristics = code;
      sei_read_code( pDecodedMessageOutputStream, 8, code, "film_grain_matrix_coeffs" );            sei.m_filmGrainMatrixCoeffs            = code;
    }
    sei_read_code( pDecodedMessageOutputStream, 2, code, "blending_mode_id" );                      sei.m_blendingModeId                   = code;
    sei_read_code( pDecodedMessageOutputStream, 4, code, "log2_scale_factor" );                     sei.m_log2ScaleFactor                  = code;
    for(Int c=0; c<3; c++)
    {
      sei_read_flag( pDecodedMessageOutputStream,    code, "comp_model_present_flag[c]" );          sei.m_compModel[c].bPresentFlag        = code!=0;
    }
    for(Int c=0; c<3; c++)
    {
      SEIFilmGrainCharacteristics::CompModel &cm=sei.m_compModel[c];
      if (cm.bPresentFlag)
      {
#if JVET_X0048_X0103_FILM_GRAIN
        sei_read_code( pDecodedMessageOutputStream, 8, code, "num_intensity_intervals_minus1[c]"); cm.numIntensityIntervals = code + 1;
#else
        UInt numIntensityIntervals;
        sei_read_code( pDecodedMessageOutputStream, 8, code, "num_intensity_intervals_minus1[c]" ); numIntensityIntervals = code+1;
#endif
        sei_read_code( pDecodedMessageOutputStream, 3, code, "num_model_values_minus1[c]" );        cm.numModelValues     = code+1;
#if JVET_X0048_X0103_FILM_GRAIN
        cm.intensityValues.resize(cm.numIntensityIntervals);
        for (UInt interval = 0; interval < cm.numIntensityIntervals; interval++)
#else
        cm.intensityValues.resize(numIntensityIntervals);
        for(UInt interval=0; interval<numIntensityIntervals; interval++)
#endif
        {
          SEIFilmGrainCharacteristics::CompModelIntensityValues &cmiv=cm.intensityValues[interval];
          sei_read_code( pDecodedMessageOutputStream, 8, code, "intensity_interval_lower_bound[c][i]" ); cmiv.intensityIntervalLowerBound=code;
          sei_read_code( pDecodedMessageOutputStream, 8, code, "intensity_interval_upper_bound[c][i]" ); cmiv.intensityIntervalUpperBound=code;
          cmiv.compModelValue.resize(cm.numModelValues);
          for(UInt j=0; j<cm.numModelValues; j++)
          {
            sei_read_svlc( pDecodedMessageOutputStream, cmiv.compModelValue[j], "comp_model_value[c][i]" );
          }
        }
      }
    } // for c
    sei_read_flag( pDecodedMessageOutputStream, code, "film_grain_characteristics_persistence_flag" ); sei.m_filmGrainCharacteristicsPersistenceFlag = code!=0;
  } // cancel flag
}


Void SEIReader::xParseSEIPostFilterHint(SEIPostFilterHint& sei, UInt payloadSize, const TComSPS *sps, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_uvlc( pDecodedMessageOutputStream,    code, "filter_hint_size_y" ); sei.m_filterHintSizeY = code;
  sei_read_uvlc( pDecodedMessageOutputStream,    code, "filter_hint_size_x" ); sei.m_filterHintSizeX = code;
  sei_read_code( pDecodedMessageOutputStream, 2, code, "filter_hint_type"   ); sei.m_filterHintType  = code;

  sei.m_bIsMonochrome = (sps->getChromaFormatIdc() == CHROMA_400);
  const UInt numChromaChannels = sei.m_bIsMonochrome ? 1:3;

  sei.m_filterHintValues.resize(numChromaChannels * sei.m_filterHintSizeX * sei.m_filterHintSizeY);
  for(std::size_t i=0; i<sei.m_filterHintValues.size(); i++)
  {
    Int v;
    sei_read_svlc( pDecodedMessageOutputStream, v, "filter_hint_value[][][]" ); sei.m_filterHintValues[i] = code;
  }
}


Void SEIReader::xParseSEIToneMappingInfo(SEIToneMappingInfo& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  Int i;
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_uvlc( pDecodedMessageOutputStream, val, "tone_map_id" );                         sei.m_toneMapId = val;
  sei_read_flag( pDecodedMessageOutputStream, val, "tone_map_cancel_flag" );                sei.m_toneMapCancelFlag = val;

  if ( !sei.m_toneMapCancelFlag )
  {
    sei_read_flag( pDecodedMessageOutputStream, val, "tone_map_persistence_flag" );         sei.m_toneMapPersistenceFlag = val;
    sei_read_code( pDecodedMessageOutputStream, 8, val, "coded_data_bit_depth" );           sei.m_codedDataBitDepth = val;
    sei_read_code( pDecodedMessageOutputStream, 8, val, "target_bit_depth" );               sei.m_targetBitDepth = val;
    sei_read_uvlc( pDecodedMessageOutputStream, val, "tone_map_model_id" );                 sei.m_modelId = val;
    switch(sei.m_modelId)
    {
    case 0:
      {
        sei_read_code( pDecodedMessageOutputStream, 32, val, "min_value" );                 sei.m_minValue = val;
        sei_read_code( pDecodedMessageOutputStream, 32, val, "max_value" );                 sei.m_maxValue = val;
        break;
      }
    case 1:
      {
        sei_read_code( pDecodedMessageOutputStream, 32, val, "sigmoid_midpoint" );          sei.m_sigmoidMidpoint = val;
        sei_read_code( pDecodedMessageOutputStream, 32, val, "sigmoid_width" );             sei.m_sigmoidWidth = val;
        break;
      }
    case 2:
      {
        UInt num = 1u << sei.m_targetBitDepth;
        sei.m_startOfCodedInterval.resize(num+1);
        for(i = 0; i < num; i++)
        {
          sei_read_code( pDecodedMessageOutputStream, ((( sei.m_codedDataBitDepth + 7 ) >> 3 ) << 3), val, "start_of_coded_interval[i]" );
          sei.m_startOfCodedInterval[i] = val;
        }
        sei.m_startOfCodedInterval[num] = 1u << sei.m_codedDataBitDepth;
        break;
      }
    case 3:
      {
        sei_read_code( pDecodedMessageOutputStream, 16, val,  "num_pivots" );                       sei.m_numPivots = val;
        sei.m_codedPivotValue.resize(sei.m_numPivots);
        sei.m_targetPivotValue.resize(sei.m_numPivots);
        for(i = 0; i < sei.m_numPivots; i++ )
        {
          sei_read_code( pDecodedMessageOutputStream, ((( sei.m_codedDataBitDepth + 7 ) >> 3 ) << 3), val, "coded_pivot_value[i]" );
          sei.m_codedPivotValue[i] = val;
          sei_read_code( pDecodedMessageOutputStream, ((( sei.m_targetBitDepth + 7 ) >> 3 ) << 3),    val, "target_pivot_value[i]" );
          sei.m_targetPivotValue[i] = val;
        }
        break;
      }
    case 4:
      {
        sei_read_code( pDecodedMessageOutputStream, 8, val, "camera_iso_speed_idc" );                     sei.m_cameraIsoSpeedIdc = val;
        if( sei.m_cameraIsoSpeedIdc == 255) //Extended_ISO
        {
          sei_read_code( pDecodedMessageOutputStream, 32,   val,   "camera_iso_speed_value" );            sei.m_cameraIsoSpeedValue = val;
        }
        sei_read_code( pDecodedMessageOutputStream, 8, val, "exposure_index_idc" );                       sei.m_exposureIndexIdc = val;
        if( sei.m_exposureIndexIdc == 255) //Extended_ISO
        {
          sei_read_code( pDecodedMessageOutputStream, 32,   val,   "exposure_index_value" );              sei.m_exposureIndexValue = val;
        }
        sei_read_flag( pDecodedMessageOutputStream, val, "exposure_compensation_value_sign_flag" );       sei.m_exposureCompensationValueSignFlag = val;
        sei_read_code( pDecodedMessageOutputStream, 16, val, "exposure_compensation_value_numerator" );   sei.m_exposureCompensationValueNumerator = val;
        sei_read_code( pDecodedMessageOutputStream, 16, val, "exposure_compensation_value_denom_idc" );   sei.m_exposureCompensationValueDenomIdc = val;
        sei_read_code( pDecodedMessageOutputStream, 32, val, "ref_screen_luminance_white" );              sei.m_refScreenLuminanceWhite = val;
        sei_read_code( pDecodedMessageOutputStream, 32, val, "extended_range_white_level" );              sei.m_extendedRangeWhiteLevel = val;
        sei_read_code( pDecodedMessageOutputStream, 16, val, "nominal_black_level_code_value" );          sei.m_nominalBlackLevelLumaCodeValue = val;
        sei_read_code( pDecodedMessageOutputStream, 16, val, "nominal_white_level_code_value" );          sei.m_nominalWhiteLevelLumaCodeValue= val;
        sei_read_code( pDecodedMessageOutputStream, 16, val, "extended_white_level_code_value" );         sei.m_extendedWhiteLevelLumaCodeValue = val;
        break;
      }
    default:
      {
        assert(!"Undefined SEIToneMapModelId");
        break;
      }
    }//switch model id
  }// if(!sei.m_toneMapCancelFlag)
}


Void SEIReader::xParseSEIFramePacking(SEIFramePacking& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_uvlc( pDecodedMessageOutputStream, val, "frame_packing_arrangement_id" );                 sei.m_arrangementId = val;
  sei_read_flag( pDecodedMessageOutputStream, val, "frame_packing_arrangement_cancel_flag" );        sei.m_arrangementCancelFlag = val;

  if ( !sei.m_arrangementCancelFlag )
  {
    sei_read_code( pDecodedMessageOutputStream, 7, val, "frame_packing_arrangement_type" );          sei.m_arrangementType = val;
    assert((sei.m_arrangementType > 2) && (sei.m_arrangementType < 6) );

    sei_read_flag( pDecodedMessageOutputStream, val, "quincunx_sampling_flag" );                     sei.m_quincunxSamplingFlag = val;

    sei_read_code( pDecodedMessageOutputStream, 6, val, "content_interpretation_type" );             sei.m_contentInterpretationType = val;
    sei_read_flag( pDecodedMessageOutputStream, val, "spatial_flipping_flag" );                      sei.m_spatialFlippingFlag = val;
    sei_read_flag( pDecodedMessageOutputStream, val, "frame0_flipped_flag" );                        sei.m_frame0FlippedFlag = val;
    sei_read_flag( pDecodedMessageOutputStream, val, "field_views_flag" );                           sei.m_fieldViewsFlag = val;
    sei_read_flag( pDecodedMessageOutputStream, val, "current_frame_is_frame0_flag" );               sei.m_currentFrameIsFrame0Flag = val;
    sei_read_flag( pDecodedMessageOutputStream, val, "frame0_self_contained_flag" );                 sei.m_frame0SelfContainedFlag = val;
    sei_read_flag( pDecodedMessageOutputStream, val, "frame1_self_contained_flag" );                 sei.m_frame1SelfContainedFlag = val;

    if ( sei.m_quincunxSamplingFlag == 0 && sei.m_arrangementType != 5)
    {
      sei_read_code( pDecodedMessageOutputStream, 4, val, "frame0_grid_position_x" );                sei.m_frame0GridPositionX = val;
      sei_read_code( pDecodedMessageOutputStream, 4, val, "frame0_grid_position_y" );                sei.m_frame0GridPositionY = val;
      sei_read_code( pDecodedMessageOutputStream, 4, val, "frame1_grid_position_x" );                sei.m_frame1GridPositionX = val;
      sei_read_code( pDecodedMessageOutputStream, 4, val, "frame1_grid_position_y" );                sei.m_frame1GridPositionY = val;
    }

    sei_read_code( pDecodedMessageOutputStream, 8, val, "frame_packing_arrangement_reserved_byte" );   sei.m_arrangementReservedByte = val;
    sei_read_flag( pDecodedMessageOutputStream, val,  "frame_packing_arrangement_persistence_flag" );  sei.m_arrangementPersistenceFlag = (val != 0);
  }
  sei_read_flag( pDecodedMessageOutputStream, val, "upsampled_aspect_ratio_flag" );                  sei.m_upsampledAspectRatio = val;
}


Void SEIReader::xParseSEIDisplayOrientation(SEIDisplayOrientation& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_flag( pDecodedMessageOutputStream, val,       "display_orientation_cancel_flag" );       sei.cancelFlag            = val;
  if( !sei.cancelFlag )
  {
    sei_read_flag( pDecodedMessageOutputStream, val,     "hor_flip" );                              sei.horFlip               = val;
    sei_read_flag( pDecodedMessageOutputStream, val,     "ver_flip" );                              sei.verFlip               = val;
    sei_read_code( pDecodedMessageOutputStream, 16, val, "anticlockwise_rotation" );                sei.anticlockwiseRotation = val;
    sei_read_flag( pDecodedMessageOutputStream, val,     "display_orientation_persistence_flag" );  sei.persistenceFlag       = val;
  }
}


Void SEIReader::xParseSEIGreenMetadataInfo(SEIGreenMetadataInfo& sei, UInt payloadSize, ostream* pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_code(pDecodedMessageOutputStream, 8, code, "green_metadata_type");
  sei.m_greenMetadataType = code;

  sei_read_code(pDecodedMessageOutputStream, 8, code, "xsd_metric_type");
  sei.m_xsdMetricType = code;

  sei_read_code(pDecodedMessageOutputStream, 16, code, "xsd_metric_value");
  sei.m_xsdMetricValue = code;
}


Void SEIReader::xParseSEISOPDescription(SEISOPDescription &sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  Int iCode;
  UInt uiCode;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_uvlc( pDecodedMessageOutputStream, uiCode,           "sop_seq_parameter_set_id"            ); sei.m_sopSeqParameterSetId = uiCode;
  sei_read_uvlc( pDecodedMessageOutputStream, uiCode,           "num_pics_in_sop_minus1"              ); sei.m_numPicsInSopMinus1 = uiCode;
  for (UInt i = 0; i <= sei.m_numPicsInSopMinus1; i++)
  {
    sei_read_code( pDecodedMessageOutputStream, 6, uiCode,                     "sop_vcl_nut[i]" );  sei.m_sopDescVclNaluType[i] = uiCode;
    sei_read_code( pDecodedMessageOutputStream, 3, sei.m_sopDescTemporalId[i], "sop_temporal_id[i]"   );  sei.m_sopDescTemporalId[i] = uiCode;
    if (sei.m_sopDescVclNaluType[i] != NAL_UNIT_CODED_SLICE_IDR_W_RADL && sei.m_sopDescVclNaluType[i] != NAL_UNIT_CODED_SLICE_IDR_N_LP)
    {
      sei_read_uvlc( pDecodedMessageOutputStream, sei.m_sopDescStRpsIdx[i],    "sop_short_term_rps_idx[i]"    ); sei.m_sopDescStRpsIdx[i] = uiCode;
    }
    if (i > 0)
    {
      sei_read_svlc( pDecodedMessageOutputStream, iCode,                       "sop_poc_delta[i]"     ); sei.m_sopDescPocDelta[i] = iCode;
    }
  }
}


Void SEIReader::xParseSEIActiveParameterSets(SEIActiveParameterSets& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_code( pDecodedMessageOutputStream, 4, val, "active_video_parameter_set_id");   sei.activeVPSId = val;
  sei_read_flag( pDecodedMessageOutputStream,    val, "self_contained_cvs_flag");         sei.m_selfContainedCvsFlag     = (val != 0);
  sei_read_flag( pDecodedMessageOutputStream,    val, "no_parameter_set_update_flag");    sei.m_noParameterSetUpdateFlag = (val != 0);
  sei_read_uvlc( pDecodedMessageOutputStream,    val, "num_sps_ids_minus1");              sei.numSpsIdsMinus1 = val;

  sei.activeSeqParameterSetId.resize(sei.numSpsIdsMinus1 + 1);
  for (Int i=0; i < (sei.numSpsIdsMinus1 + 1); i++)
  {
    sei_read_uvlc( pDecodedMessageOutputStream, val, "active_seq_parameter_set_id[i]");    sei.activeSeqParameterSetId[i] = val;
  }
}


Void SEIReader::xParseSEIDecodingUnitInfo(SEIDecodingUnitInfo& sei, UInt payloadSize, const TComSPS *sps, std::ostream *pDecodedMessageOutputStream)
{
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_uvlc( pDecodedMessageOutputStream, val, "decoding_unit_idx");
  sei.m_decodingUnitIdx = val;

  const TComVUI *vui = sps->getVuiParameters();
  if(vui->getHrdParameters()->getSubPicCpbParamsInPicTimingSEIFlag())
  {
    sei_read_code( pDecodedMessageOutputStream, ( vui->getHrdParameters()->getDuCpbRemovalDelayLengthMinus1() + 1 ), val, "du_spt_cpb_removal_delay_increment");
    sei.m_duSptCpbRemovalDelay = val;
  }
  else
  {
    sei.m_duSptCpbRemovalDelay = 0;
  }
  sei_read_flag( pDecodedMessageOutputStream, val, "dpb_output_du_delay_present_flag"); sei.m_dpbOutputDuDelayPresentFlag = (val != 0);
  if(sei.m_dpbOutputDuDelayPresentFlag)
  {
    sei_read_code( pDecodedMessageOutputStream, vui->getHrdParameters()->getDpbOutputDelayDuLengthMinus1() + 1, val, "pic_spt_dpb_output_du_delay");
    sei.m_picSptDpbOutputDuDelay = val;
  }
}


Void SEIReader::xParseSEITemporalLevel0Index(SEITemporalLevel0Index& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_code( pDecodedMessageOutputStream, 8, val, "temporal_sub_layer_zero_idx" );  sei.tl0Idx = val;
  sei_read_code( pDecodedMessageOutputStream, 8, val, "irap_pic_id" );  sei.rapIdx = val;
}


Void SEIReader::xParseSEIDecodedPictureHash(SEIDecodedPictureHash& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt bytesRead = 0;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  UInt val;
  sei_read_code( pDecodedMessageOutputStream, 8, val, "hash_type");
  sei.method = static_cast<HashType>(val); bytesRead++;

  const TChar *traceString="\0";
  switch (sei.method)
  {
    case HASHTYPE_MD5: traceString="picture_md5"; break;
    case HASHTYPE_CRC: traceString="picture_crc"; break;
    case HASHTYPE_CHECKSUM: traceString="picture_checksum"; break;
    default: assert(false); break;
  }

  if (pDecodedMessageOutputStream)
  {
    (*pDecodedMessageOutputStream) << "  " << std::setw(55) << traceString << ": " << std::hex << std::setfill('0');
  }

  sei.m_pictureHash.hash.clear();
  for(;bytesRead < payloadSize; bytesRead++)
  {
    sei_read_code( NULL, 8, val, traceString);
    sei.m_pictureHash.hash.push_back((UChar)val);
    if (pDecodedMessageOutputStream)
    {
      (*pDecodedMessageOutputStream) << std::setw(2) << val;
    }
  }

  if (pDecodedMessageOutputStream)
  {
    (*pDecodedMessageOutputStream) << std::dec << std::setfill(' ') << "\n";
  }
}


Void SEIReader::xParseSEIScalableNesting(SEIScalableNesting& sei, const NalUnitType nalUnitType, UInt payloadSize, const TComSPS *sps, std::ostream *pDecodedMessageOutputStream)
{
  UInt uiCode;
  SEIMessages seis;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_flag( pDecodedMessageOutputStream, uiCode,            "bitstream_subset_flag"         ); sei.m_bitStreamSubsetFlag = uiCode;
  sei_read_flag( pDecodedMessageOutputStream, uiCode,            "nesting_op_flag"               ); sei.m_nestingOpFlag = uiCode;
  if (sei.m_nestingOpFlag)
  {
    sei_read_flag( pDecodedMessageOutputStream, uiCode,            "default_op_flag"               ); sei.m_defaultOpFlag = uiCode;
    sei_read_uvlc( pDecodedMessageOutputStream, uiCode,            "nesting_num_ops_minus1"        ); sei.m_nestingNumOpsMinus1 = uiCode;
    for (UInt i = sei.m_defaultOpFlag; i <= sei.m_nestingNumOpsMinus1; i++)
    {
      sei_read_code( pDecodedMessageOutputStream, 3,        uiCode,  "nesting_max_temporal_id_plus1[i]"   ); sei.m_nestingMaxTemporalIdPlus1[i] = uiCode;
      sei_read_uvlc( pDecodedMessageOutputStream, uiCode,            "nesting_op_idx[i]"                  ); sei.m_nestingOpIdx[i] = uiCode;
    }
  }
  else
  {
    sei_read_flag( pDecodedMessageOutputStream, uiCode,            "all_layers_flag"               ); sei.m_allLayersFlag       = uiCode;
    if (!sei.m_allLayersFlag)
    {
      sei_read_code( pDecodedMessageOutputStream, 3,        uiCode,  "nesting_no_op_max_temporal_id_plus1"  ); sei.m_nestingNoOpMaxTemporalIdPlus1 = uiCode;
      sei_read_uvlc( pDecodedMessageOutputStream, uiCode,            "nesting_num_layers_minus1"            ); sei.m_nestingNumLayersMinus1        = uiCode;
      for (UInt i = 0; i <= sei.m_nestingNumLayersMinus1; i++)
      {
        sei_read_code( pDecodedMessageOutputStream, 6,           uiCode,     "nesting_layer_id[i]"      ); sei.m_nestingLayerId[i]   = uiCode;
      }
    }
  }

  // byte alignment
  while ( m_pcBitstream->getNumBitsRead() % 8 != 0 )
  {
    UInt code;
    sei_read_flag( pDecodedMessageOutputStream, code, "nesting_zero_bit" );
  }

  // read nested SEI messages
  do
  {
    if(nalUnitType == NAL_UNIT_PREFIX_SEI)
    {
      xReadSEImessage(sei.m_nestedSEIs, nalUnitType, sps, pDecodedMessageOutputStream, SEI::prefix_sei_messages, std::string("scalable nested SEI"));
    }
    else
    {
      xReadSEImessage(sei.m_nestedSEIs, nalUnitType, sps, pDecodedMessageOutputStream, SEI::suffix_sei_messages, std::string("scalable nested SEI"));
    }
  } while (m_pcBitstream->getNumBitsLeft() > 8);

  if (pDecodedMessageOutputStream)
  {
    (*pDecodedMessageOutputStream) << "End of scalable nesting SEI message\n";
  }
}


Void SEIReader::xParseSEIRegionRefreshInfo(SEIRegionRefreshInfo& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_flag( pDecodedMessageOutputStream, val, "refreshed_region_flag" ); sei.m_gdrForegroundFlag = val ? 1 : 0;
}


Void SEIReader::xParseSEINoDisplay(SEINoDisplay& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei.m_noDisplay = true;
}


Void SEIReader::xParseSEITimeCode(SEITimeCode& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_code( pDecodedMessageOutputStream, 2, code, "num_clock_ts"); sei.numClockTs = code;
  for(Int i = 0; i < sei.numClockTs; i++)
  {
    TComSEITimeSet currentTimeSet;
    sei_read_flag( pDecodedMessageOutputStream, code, "clock_time_stamp_flag[i]"); currentTimeSet.clockTimeStampFlag = code;
    if(currentTimeSet.clockTimeStampFlag)
    {
      sei_read_flag( pDecodedMessageOutputStream, code, "nuit_field_based_flag"); currentTimeSet.numUnitFieldBasedFlag = code;
      sei_read_code( pDecodedMessageOutputStream, 5, code, "counting_type"); currentTimeSet.countingType = code;
      sei_read_flag( pDecodedMessageOutputStream, code, "full_timestamp_flag"); currentTimeSet.fullTimeStampFlag = code;
      sei_read_flag( pDecodedMessageOutputStream, code, "discontinuity_flag"); currentTimeSet.discontinuityFlag = code;
      sei_read_flag( pDecodedMessageOutputStream, code, "cnt_dropped_flag"); currentTimeSet.cntDroppedFlag = code;
      sei_read_code( pDecodedMessageOutputStream, 9, code, "n_frames"); currentTimeSet.numberOfFrames = code;
      if(currentTimeSet.fullTimeStampFlag)
      {
        sei_read_code( pDecodedMessageOutputStream, 6, code, "seconds_value"); currentTimeSet.secondsValue = code;
        sei_read_code( pDecodedMessageOutputStream, 6, code, "minutes_value"); currentTimeSet.minutesValue = code;
        sei_read_code( pDecodedMessageOutputStream, 5, code, "hours_value"); currentTimeSet.hoursValue = code;
      }
      else
      {
        sei_read_flag( pDecodedMessageOutputStream, code, "seconds_flag"); currentTimeSet.secondsFlag = code;
        if(currentTimeSet.secondsFlag)
        {
          sei_read_code( pDecodedMessageOutputStream, 6, code, "seconds_value"); currentTimeSet.secondsValue = code;
          sei_read_flag( pDecodedMessageOutputStream, code, "minutes_flag"); currentTimeSet.minutesFlag = code;
          if(currentTimeSet.minutesFlag)
          {
            sei_read_code( pDecodedMessageOutputStream, 6, code, "minutes_value"); currentTimeSet.minutesValue = code;
            sei_read_flag( pDecodedMessageOutputStream, code, "hours_flag"); currentTimeSet.hoursFlag = code;
            if(currentTimeSet.hoursFlag)
            {
              sei_read_code( pDecodedMessageOutputStream, 5, code, "hours_value"); currentTimeSet.hoursValue = code;
            }
          }
        }
      }
      sei_read_code( pDecodedMessageOutputStream, 5, code, "time_offset_length"); currentTimeSet.timeOffsetLength = code;
      if(currentTimeSet.timeOffsetLength > 0)
      {
        sei_read_code( pDecodedMessageOutputStream, currentTimeSet.timeOffsetLength, code, "time_offset_value");
        if((code & (1 << (currentTimeSet.timeOffsetLength-1))) == 0)
        {
          currentTimeSet.timeOffsetValue = code;
        }
        else
        {
          code &= (1<< (currentTimeSet.timeOffsetLength-1)) - 1;
          currentTimeSet.timeOffsetValue = ~code + 1;
        }
      }
    }
    sei.timeSetArray[i] = currentTimeSet;
  }
}


Void SEIReader::xParseSEIMasteringDisplayColourVolume(SEIMasteringDisplayColourVolume& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_code( pDecodedMessageOutputStream, 16, code, "display_primaries_x[0]" ); sei.values.primaries[0][0] = code;
  sei_read_code( pDecodedMessageOutputStream, 16, code, "display_primaries_y[0]" ); sei.values.primaries[0][1] = code;

  sei_read_code( pDecodedMessageOutputStream, 16, code, "display_primaries_x[1]" ); sei.values.primaries[1][0] = code;
  sei_read_code( pDecodedMessageOutputStream, 16, code, "display_primaries_y[1]" ); sei.values.primaries[1][1] = code;

  sei_read_code( pDecodedMessageOutputStream, 16, code, "display_primaries_x[2]" ); sei.values.primaries[2][0] = code;
  sei_read_code( pDecodedMessageOutputStream, 16, code, "display_primaries_y[2]" ); sei.values.primaries[2][1] = code;


  sei_read_code( pDecodedMessageOutputStream, 16, code, "white_point_x" ); sei.values.whitePoint[0] = code;
  sei_read_code( pDecodedMessageOutputStream, 16, code, "white_point_y" ); sei.values.whitePoint[1] = code;

  sei_read_code( pDecodedMessageOutputStream, 32, code, "max_display_mastering_luminance" ); sei.values.maxLuminance = code;
  sei_read_code( pDecodedMessageOutputStream, 32, code, "min_display_mastering_luminance" ); sei.values.minLuminance = code;
}


Void SEIReader::xParseSEISegmentedRectFramePacking(SEISegmentedRectFramePacking& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_flag( pDecodedMessageOutputStream, val,       "segmented_rect_frame_packing_arrangement_cancel_flag" );       sei.m_arrangementCancelFlag            = val;
  if( !sei.m_arrangementCancelFlag )
  {
    sei_read_code( pDecodedMessageOutputStream, 2, val, "segmented_rect_content_interpretation_type" );                sei.m_contentInterpretationType = val;
    sei_read_flag( pDecodedMessageOutputStream, val,     "segmented_rect_frame_packing_arrangement_persistence" );                              sei.m_arrangementPersistenceFlag               = val;
  }
}


Void SEIReader::xParseSEITempMotionConstraintsTileSets(SEITempMotionConstrainedTileSets& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_flag( pDecodedMessageOutputStream, code, "mc_all_tiles_exact_sample_value_match_flag");  sei.m_mc_all_tiles_exact_sample_value_match_flag = (code != 0);
  sei_read_flag( pDecodedMessageOutputStream, code, "each_tile_one_tile_set_flag");                 sei.m_each_tile_one_tile_set_flag                = (code != 0);

  if(!sei.m_each_tile_one_tile_set_flag)
  {
    sei_read_flag( pDecodedMessageOutputStream, code, "limited_tile_set_display_flag");  sei.m_limited_tile_set_display_flag = (code != 0);
    sei_read_uvlc( pDecodedMessageOutputStream, code, "num_sets_in_message_minus1");     sei.setNumberOfTileSets(code + 1);

    if(sei.getNumberOfTileSets() != 0)
    {
      for(Int i = 0; i < sei.getNumberOfTileSets(); i++)
      {
        sei_read_uvlc( pDecodedMessageOutputStream, code, "mcts_id");  sei.tileSetData(i).m_mcts_id = code;

        if(sei.m_limited_tile_set_display_flag)
        {
          sei_read_flag( pDecodedMessageOutputStream, code, "display_tile_set_flag");  sei.tileSetData(i).m_display_tile_set_flag = (code != 1);
        }

        sei_read_uvlc( pDecodedMessageOutputStream, code, "num_tile_rects_in_set_minus1");  sei.tileSetData(i).setNumberOfTileRects(code + 1);

        for(Int j=0; j<sei.tileSetData(i).getNumberOfTileRects(); j++)
        {
          sei_read_uvlc( pDecodedMessageOutputStream, code, "top_left_tile_index");      sei.tileSetData(i).topLeftTileIndex(j)     = code;
          sei_read_uvlc( pDecodedMessageOutputStream, code, "bottom_right_tile_index");  sei.tileSetData(i).bottomRightTileIndex(j) = code;
        }

        if(!sei.m_mc_all_tiles_exact_sample_value_match_flag)
        {
          sei_read_flag( pDecodedMessageOutputStream, code, "exact_sample_value_match_flag");   sei.tileSetData(i).m_exact_sample_value_match_flag    = (code != 0);
        }
        sei_read_flag( pDecodedMessageOutputStream, code, "mcts_tier_level_idc_present_flag");  sei.tileSetData(i).m_mcts_tier_level_idc_present_flag = (code != 0);

        if(sei.tileSetData(i).m_mcts_tier_level_idc_present_flag)
        {
          sei_read_flag( pDecodedMessageOutputStream, code,    "mcts_tier_flag"); sei.tileSetData(i).m_mcts_tier_flag = (code != 0);
          sei_read_code( pDecodedMessageOutputStream, 8, code, "mcts_level_idc"); sei.tileSetData(i).m_mcts_level_idc =  code;
        }
      }
    }
  }
  else
  {
    sei_read_flag( pDecodedMessageOutputStream, code, "max_mcs_tier_level_idc_present_flag");  sei.m_max_mcs_tier_level_idc_present_flag = code;
    if(sei.m_max_mcs_tier_level_idc_present_flag)
    {
      sei_read_flag( pDecodedMessageOutputStream, code, "max_mcts_tier_flag");  sei.m_max_mcts_tier_flag = code;
      sei_read_code( pDecodedMessageOutputStream, 8, code, "max_mcts_level_idc"); sei.m_max_mcts_level_idc = code;
    }
  }
}

#if MCTS_EXTRACTION
Void SEIReader::xParseSEIMCTSExtractionInfoSet(SEIMCTSExtractionInfoSet& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  UInt numInfoSetsMinus1;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_uvlc(pDecodedMessageOutputStream, code, "num_sets_in_message_minus1"); numInfoSetsMinus1 = code;
  for (Int i = 0; i <= numInfoSetsMinus1; i++)
  {
    SEIMCTSExtractionInfoSet::MCTSExtractionInfo EIS;

    sei_read_uvlc(pDecodedMessageOutputStream, code, "num_mcts_sets_minus1[ i ]");
    EIS.m_idxOfMctsInSet.resize(code + 1);
    for (Int j = 0; j < EIS.m_idxOfMctsInSet.size(); j++)
    {
      sei_read_uvlc(pDecodedMessageOutputStream, code, "num_mcts_in_set_minus1[ i ][ j ]");
      EIS.m_idxOfMctsInSet[j].resize(code + 1);
      for (Int k = 0; k < EIS.m_idxOfMctsInSet[j].size(); k++)
      {
        sei_read_uvlc(pDecodedMessageOutputStream, code, "idx_of_mcts_in_set[ i ][ j ][ k ]"); EIS.m_idxOfMctsInSet[j][k] = code;
      }
    }
    sei_read_flag(pDecodedMessageOutputStream, code, "slice_reordering_enabled_flag[ i ]");  EIS.m_sliceReorderingEnabledFlag = (code != 0);
    if (EIS.m_sliceReorderingEnabledFlag) 
    {
      sei_read_uvlc(pDecodedMessageOutputStream, code, "num_slice_segments_minus1[ i ]");
      EIS.m_outputSliceSegmentAddress.resize(code + 1);
      for (Int j = 0; j < EIS.m_outputSliceSegmentAddress.size(); j++)
      {
        sei_read_uvlc(pDecodedMessageOutputStream, code, "output_slice_segment_address[ i ][ j ]"); EIS.m_outputSliceSegmentAddress[j] = code;
      }
    }

    sei_read_uvlc(pDecodedMessageOutputStream, code, "num_vps_in_info_set_minus1[i]");
    EIS.m_vpsRbspData.resize(code + 1);
    EIS.m_vpsRbspDataLength.resize(code + 1);
    for (Int j = 0; j < EIS.m_vpsRbspDataLength.size(); j++)
    {
      sei_read_uvlc(pDecodedMessageOutputStream, code, "vps_rbsp_data_length[i][j]"); EIS.m_vpsRbspDataLength[j] = code;
    }
    sei_read_uvlc(pDecodedMessageOutputStream, code, "num_sps_in_info_set_minus1[i]");
    EIS.m_spsRbspData.resize(code + 1);
    EIS.m_spsRbspDataLength.resize(code + 1);
    for (Int j = 0; j < EIS.m_spsRbspDataLength.size(); j++)
    {
      sei_read_uvlc(pDecodedMessageOutputStream, code, "sps_rbsp_data_length[i][j]"); EIS.m_spsRbspDataLength[j] = code;
    }
    sei_read_uvlc(pDecodedMessageOutputStream, code, "num_pps_in_info_set_minus1[i]");
    EIS.m_ppsRbspData.resize(code + 1);
    EIS.m_ppsNuhTemporalIdPlus1.resize(code + 1);
    EIS.m_ppsRbspDataLength.resize(code + 1);
    for (Int j = 0; j < EIS.m_ppsRbspDataLength.size(); j++)
    {
      sei_read_uvlc(pDecodedMessageOutputStream, code, "pps_nuh_temporal_id_plus1[i][j]"); EIS.m_ppsNuhTemporalIdPlus1[j] = code;
      sei_read_uvlc(pDecodedMessageOutputStream, code, "sps_rbsp_data_length[i][j]"); EIS.m_ppsRbspDataLength[j] = code;
    }

    // byte alignment
    while (m_pcBitstream->getNumBitsRead() % 8 != 0)
    {
      sei_read_flag(pDecodedMessageOutputStream, code, "mcts_alignment_bit_equal_to_zero");
    }

    for (Int j = 0; j < EIS.m_vpsRbspData.size(); j++)
    {
      EIS.m_vpsRbspData[j].resize(EIS.m_vpsRbspDataLength[j]);
      for (Int k = 0; k < EIS.m_vpsRbspDataLength[j]; k++) 
      {
        sei_read_code(pDecodedMessageOutputStream, 8, code, "vps_rbsp_data_byte[ i ][ j ][ k ]"); EIS.m_vpsRbspData[j][k] = code;
      }
    }
    for (Int j = 0; j < EIS.m_spsRbspData.size(); j++)
    {
      EIS.m_spsRbspData[j].resize(EIS.m_spsRbspDataLength[j]);
      for (Int k = 0; k < EIS.m_spsRbspDataLength[j]; k++) 
      {
        sei_read_code(pDecodedMessageOutputStream, 8, code, "sps_rbsp_data_byte[ i ][ j ][ k ]"); EIS.m_spsRbspData[j][k] = code;
      }
    }
    for (Int j = 0; j < EIS.m_ppsRbspData.size(); j++)
    {
      EIS.m_ppsRbspData[j].resize(EIS.m_ppsRbspDataLength[j]);
      for (Int k = 0; k < EIS.m_ppsRbspDataLength[j]; k++) 
      {
        sei_read_code(pDecodedMessageOutputStream, 8, code, "pps_rbsp_data_byte[ i ][ j ][ k ]"); EIS.m_ppsRbspData[j][k] = code;
      }
    }
    sei.m_MCTSExtractionInfoSets.push_back(EIS);
  }
}

#endif

Void SEIReader::xParseSEIChromaResamplingFilterHint(SEIChromaResamplingFilterHint& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt uiCode;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_code( pDecodedMessageOutputStream, 8, uiCode, "ver_chroma_filter_idc"); sei.m_verChromaFilterIdc = uiCode;
  sei_read_code( pDecodedMessageOutputStream, 8, uiCode, "hor_chroma_filter_idc"); sei.m_horChromaFilterIdc = uiCode;
  sei_read_flag( pDecodedMessageOutputStream, uiCode, "ver_filtering_field_processing_flag"); sei.m_verFilteringFieldProcessingFlag = uiCode;
  if(sei.m_verChromaFilterIdc == 1 || sei.m_horChromaFilterIdc == 1)
  {
    sei_read_uvlc( pDecodedMessageOutputStream, uiCode, "target_format_idc"); sei.m_targetFormatIdc = uiCode;
    if(sei.m_verChromaFilterIdc == 1)
    {
      UInt numVerticalFilters;
      sei_read_uvlc( pDecodedMessageOutputStream, numVerticalFilters, "num_vertical_filters"); sei.m_verFilterCoeff.resize(numVerticalFilters);
      if(numVerticalFilters > 0)
      {
        for(Int i = 0; i < numVerticalFilters; i++)
        {
          UInt verTapLengthMinus1;
          sei_read_uvlc( pDecodedMessageOutputStream, verTapLengthMinus1, "ver_tap_length_minus_1"); sei.m_verFilterCoeff[i].resize(verTapLengthMinus1+1);
          for(Int j = 0; j < (verTapLengthMinus1 + 1); j++)
          {
            sei_read_svlc( pDecodedMessageOutputStream, sei.m_verFilterCoeff[i][j], "ver_filter_coeff");
          }
        }
      }
    }
    if(sei.m_horChromaFilterIdc == 1)
    {
      UInt numHorizontalFilters;
      sei_read_uvlc( pDecodedMessageOutputStream, numHorizontalFilters, "num_horizontal_filters"); sei.m_horFilterCoeff.resize(numHorizontalFilters);
      if(numHorizontalFilters  > 0)
      {
        for(Int i = 0; i < numHorizontalFilters; i++)
        {
          UInt horTapLengthMinus1;
          sei_read_uvlc( pDecodedMessageOutputStream, horTapLengthMinus1, "hor_tap_length_minus_1"); sei.m_horFilterCoeff[i].resize(horTapLengthMinus1+1);
          for(Int j = 0; j < (horTapLengthMinus1 + 1); j++)
          {
            sei_read_svlc( pDecodedMessageOutputStream, sei.m_horFilterCoeff[i][j], "hor_filter_coeff");
          }
        }
      }
    }
  }
}


Void SEIReader::xParseSEIKneeFunctionInfo(SEIKneeFunctionInfo& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  Int i;
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_uvlc( pDecodedMessageOutputStream, val, "knee_function_id" );                   sei.m_kneeId = val;
  sei_read_flag( pDecodedMessageOutputStream, val, "knee_function_cancel_flag" );          sei.m_kneeCancelFlag = val;
  if ( !sei.m_kneeCancelFlag )
  {
    sei_read_flag( pDecodedMessageOutputStream, val, "knee_function_persistence_flag" );   sei.m_kneePersistenceFlag = val;
    sei_read_code( pDecodedMessageOutputStream, 32, val, "input_d_range" );                sei.m_kneeInputDrange = val;
    sei_read_code( pDecodedMessageOutputStream, 32, val, "input_disp_luminance" );         sei.m_kneeInputDispLuminance = val;
    sei_read_code( pDecodedMessageOutputStream, 32, val, "output_d_range" );               sei.m_kneeOutputDrange = val;
    sei_read_code( pDecodedMessageOutputStream, 32, val, "output_disp_luminance" );        sei.m_kneeOutputDispLuminance = val;
    sei_read_uvlc( pDecodedMessageOutputStream, val, "num_knee_points_minus1" );           sei.m_kneeNumKneePointsMinus1 = val;
    assert( sei.m_kneeNumKneePointsMinus1 > 0 );
    sei.m_kneeInputKneePoint.resize(sei.m_kneeNumKneePointsMinus1+1);
    sei.m_kneeOutputKneePoint.resize(sei.m_kneeNumKneePointsMinus1+1);
    for(i = 0; i <= sei.m_kneeNumKneePointsMinus1; i++ )
    {
      sei_read_code( pDecodedMessageOutputStream, 10, val, "input_knee_point" );           sei.m_kneeInputKneePoint[i] = val;
      sei_read_code( pDecodedMessageOutputStream, 10, val, "output_knee_point" );          sei.m_kneeOutputKneePoint[i] = val;
    }
  }
}

Void SEIReader::xParseSEIContentColourVolume(SEIContentColourVolume& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  Int i;
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_flag( pDecodedMessageOutputStream, val, "ccv_cancel_flag" );          sei.m_ccvCancelFlag = val;
  if ( !sei.m_ccvCancelFlag )
  {
    Int iVal;
    sei_read_flag( pDecodedMessageOutputStream, val, "ccv_persistence_flag" );   sei.m_ccvPersistenceFlag = val;
    sei_read_flag( pDecodedMessageOutputStream, val, "ccv_primaries_present_flag" );   sei.m_ccvPrimariesPresentFlag = val;
    sei_read_flag( pDecodedMessageOutputStream, val, "ccv_min_luminance_value_present_flag" );   sei.m_ccvMinLuminanceValuePresentFlag = val;
    sei_read_flag( pDecodedMessageOutputStream, val, "ccv_max_luminance_value_present_flag" );   sei.m_ccvMaxLuminanceValuePresentFlag = val;
    sei_read_flag( pDecodedMessageOutputStream, val, "ccv_avg_luminance_value_present_flag" );   sei.m_ccvAvgLuminanceValuePresentFlag = val;
    
    if (sei.m_ccvPrimariesPresentFlag) 
    {
      for (i = 0; i < MAX_NUM_COMPONENT; i++) 
      {
        sei_read_scode( pDecodedMessageOutputStream, 32, iVal, "ccv_primaries_x[i]" );          sei.m_ccvPrimariesX[i] = iVal;
        sei_read_scode( pDecodedMessageOutputStream, 32, iVal, "ccv_primaries_y[i]" );          sei.m_ccvPrimariesY[i] = iVal;
      }
    }
    if (sei.m_ccvMinLuminanceValuePresentFlag) 
    {
      sei_read_code( pDecodedMessageOutputStream, 32, val,     "ccv_min_luminance_value" );   sei.m_ccvMinLuminanceValue = val;
    }
    if (sei.m_ccvMaxLuminanceValuePresentFlag) 
    {
      sei_read_code( pDecodedMessageOutputStream, 32, val,     "ccv_max_luminance_value" );   sei.m_ccvMaxLuminanceValue = val;
    }
    if (sei.m_ccvAvgLuminanceValuePresentFlag) 
    {
      sei_read_code( pDecodedMessageOutputStream, 32, val,     "ccv_avg_luminance_value" );   sei.m_ccvAvgLuminanceValue = val;
    }
  }
}

#if SHUTTER_INTERVAL_SEI_MESSAGE
Void SEIReader::xParseSEIShutterInterval(SEIShutterIntervalInfo& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  Int i;
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_code(pDecodedMessageOutputStream, 32, val, "sii_time_scale");                      sei.m_siiTimeScale = val;
  sei_read_flag(pDecodedMessageOutputStream, val, "fixed_shutter_interval_within_clvs_flag"); sei.m_siiFixedSIwithinCLVS = val;
  if (sei.m_siiFixedSIwithinCLVS)
  {
    sei_read_code(pDecodedMessageOutputStream, 32, val, "sii_num_units_in_shutter_interval");   sei.m_siiNumUnitsInShutterInterval = val;
  }
  else
  {
    sei_read_code(pDecodedMessageOutputStream, 3,  val, "sii_max_sub_layers_minus1 ");          sei.m_siiMaxSubLayersMinus1 = val;
    sei.m_siiSubLayerNumUnitsInSI.resize(sei.m_siiMaxSubLayersMinus1 + 1);
    for (i = 0; i <= sei.m_siiMaxSubLayersMinus1; i++)
    {
      sei_read_code(pDecodedMessageOutputStream, 32, val, "sub_layer_num_units_in_shutter_interval[ i ]");
      sei.m_siiSubLayerNumUnitsInSI[i] = val;
    }
  }
}
#endif

Void SEIReader::xParseSEIEquirectangularProjection(SEIEquirectangularProjection& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt val;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_flag( pDecodedMessageOutputStream, val,       "erp_cancel_flag" );              sei.m_erpCancelFlag = val;
  if( !sei.m_erpCancelFlag )
  {
    sei_read_flag( pDecodedMessageOutputStream, val,      "erp_persistence_flag"    );     sei.m_erpPersistenceFlag   = val;
    sei_read_flag( pDecodedMessageOutputStream, val,      "erp_guard_band_flag"     );     sei.m_erpGuardBandFlag     = val;
    sei_read_code( pDecodedMessageOutputStream, 2, val,   "erp_reserved_zero_2bits" );
    if ( sei.m_erpGuardBandFlag == 1)
    {
      sei_read_code( pDecodedMessageOutputStream, 3, val,     "erp_guard_band_type"       );   sei.m_erpGuardBandType  = val;
      sei_read_code( pDecodedMessageOutputStream, 8, val,     "erp_left_guard_band_width" );   sei.m_erpLeftGuardBandWidth = val;
      sei_read_code( pDecodedMessageOutputStream, 8, val,     "erp_right_guard_band_width");   sei.m_erpRightGuardBandWidth = val;
    }
  }
}

Void SEIReader::xParseSEISphereRotation(SEISphereRotation& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt val;
  Int  sval;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_flag( pDecodedMessageOutputStream, val,       "sphere_rotation_cancel_flag" );              sei.m_sphereRotationCancelFlag = val;
  if( !sei.m_sphereRotationCancelFlag )
  {
    sei_read_flag ( pDecodedMessageOutputStream,      val,   "sphere_rotation_persistence_flag"    );     sei.m_sphereRotationPersistenceFlag = val;
    sei_read_code ( pDecodedMessageOutputStream, 6,   val,   "sphere_rotation_reserved_zero_6bits" );
    sei_read_scode( pDecodedMessageOutputStream, 32, sval,   "sphere_rotation_yaw"                 );     sei.m_sphereRotationYaw = sval;
    sei_read_scode( pDecodedMessageOutputStream, 32, sval,   "sphere_rotation_pitch"               );     sei.m_sphereRotationPitch = sval;
    sei_read_scode( pDecodedMessageOutputStream, 32, sval,   "sphere_rotation_roll"                );     sei.m_sphereRotationRoll = sval;
  }
}

Void SEIReader::xParseSEIOmniViewport(SEIOmniViewport& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt code;
  Int  scode;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei_read_code( pDecodedMessageOutputStream, 10, code, "omni_viewport_id"          ); sei.m_omniViewportId         = code;
  sei_read_flag( pDecodedMessageOutputStream,     code, "omni_viewport_cancel_flag" ); sei.m_omniViewportCancelFlag = code;
  if (!sei.m_omniViewportCancelFlag)
  {
    UInt numRegions;
    sei_read_flag( pDecodedMessageOutputStream,    code,       "omni_viewport_persistence_flag" ); sei.m_omniViewportPersistenceFlag = code;    
    sei_read_code( pDecodedMessageOutputStream, 4, numRegions, "omni_viewport_cnt_minus1"       ); numRegions++;
    sei.m_omniViewportRegions.resize(numRegions);
    for(UInt region=0; region<numRegions; region++)
    {
      SEIOmniViewport::OmniViewport &viewport = sei.m_omniViewportRegions[region];
      sei_read_scode( pDecodedMessageOutputStream, 32, scode, "omni_viewport_azimuth_centre"   );   viewport.azimuthCentre = scode;
      sei_read_scode( pDecodedMessageOutputStream, 32, scode, "omni_viewport_elevation_centre" );   viewport.elevationCentre = scode;
      sei_read_scode( pDecodedMessageOutputStream, 32, scode, "omni_viewport_tilt_centre"      );   viewport.tiltCentre = code;
      sei_read_code( pDecodedMessageOutputStream,  32, code, "omni_viewport_hor_range"         );   viewport.horRange        = code;
      sei_read_code( pDecodedMessageOutputStream,  32, code, "omni_viewport_ver_range"         );   viewport.verRange        = code;
    }    
  }
  else
  {
    sei.m_omniViewportRegions.clear();
    sei.m_omniViewportPersistenceFlag=false;
  }
}

Void SEIReader::xParseSEIAnnotatedRegions(SEIAnnotatedRegions& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  UInt val;

  sei_read_flag(pDecodedMessageOutputStream, val, "ar_cancel_flag");                                   sei.m_hdr.m_cancelFlag = val;
  if (!sei.m_hdr.m_cancelFlag)
  {
    sei_read_flag(pDecodedMessageOutputStream, val, "ar_not_optimized_for_viewing_flag");              sei.m_hdr.m_notOptimizedForViewingFlag = val;
    sei_read_flag(pDecodedMessageOutputStream, val, "ar_true_motion_flag");                            sei.m_hdr.m_trueMotionFlag = val;
    sei_read_flag(pDecodedMessageOutputStream, val, "ar_occluded_object_flag");                        sei.m_hdr.m_occludedObjectFlag = val; // must be constant
    sei_read_flag(pDecodedMessageOutputStream, val, "ar_partial_object_flag_present_flag");            sei.m_hdr.m_partialObjectFlagPresentFlag = val; // must be constant
    sei_read_flag(pDecodedMessageOutputStream, val, "ar_object_label_present_flag");                   sei.m_hdr.m_objectLabelPresentFlag = val;
    sei_read_flag(pDecodedMessageOutputStream, val, "ar_object_confidence_info_present_flag");         sei.m_hdr.m_objectConfidenceInfoPresentFlag = val; // must be constant
    if (sei.m_hdr.m_objectConfidenceInfoPresentFlag)
    {
      sei_read_code(pDecodedMessageOutputStream, 4, val, "ar_object_confidence_length_minus_1"); sei.m_hdr.m_objectConfidenceLength = (val + 1); // must be constant
    }
    if (sei.m_hdr.m_objectLabelPresentFlag)
    {
      sei_read_flag(pDecodedMessageOutputStream, val, "ar_object_label_language_present_flag");      sei.m_hdr.m_objectLabelLanguagePresentFlag = val;
      if (sei.m_hdr.m_objectLabelLanguagePresentFlag)
      {
        // byte alignment
        while (m_pcBitstream->getNumBitsRead() % 8 != 0)
        {
          UInt code;
          sei_read_flag(pDecodedMessageOutputStream, code, "ar_bit_equal_to_zero");
        }
        sei.m_hdr.m_annotatedRegionsObjectLabelLang.clear();
        do
        {
          sei_read_code(pDecodedMessageOutputStream, 8, val, "ar_label_language");
          if (val)
          {
            assert(sei.m_hdr.m_annotatedRegionsObjectLabelLang.size()<256);
            sei.m_hdr.m_annotatedRegionsObjectLabelLang.push_back((char)val);
          }
        } while (val != '\0');
      }

      UInt numLabelUpdates;
      sei_read_uvlc(pDecodedMessageOutputStream, numLabelUpdates, "ar_num_label_updates");
      assert(numLabelUpdates<256);
      sei.m_annotatedLabels.clear();
      sei.m_annotatedLabels.resize(numLabelUpdates);
      for (auto it=sei.m_annotatedLabels.begin(); it!=sei.m_annotatedLabels.end(); it++)
      {
        SEIAnnotatedRegions::AnnotatedRegionLabel &ar = it->second;
        sei_read_uvlc(pDecodedMessageOutputStream, val, "ar_label_idx[]");             it->first = val;
        assert(val<256);
        sei_read_flag(pDecodedMessageOutputStream, val, "ar_label_cancel_flag");       ar.labelValid = !val;
        if (ar.labelValid)
        {
          ar.label.clear();
          // byte alignment
          while (m_pcBitstream->getNumBitsRead() % 8 != 0)
          {
            UInt code;
            sei_read_flag(pDecodedMessageOutputStream, code, "ar_bit_equal_to_zero");
          }
          do
          {
            sei_read_code(pDecodedMessageOutputStream, 8, val, "ar_label[]");
            if (val)
            {
              assert(ar.label.size()<256);
              ar.label.push_back((char)val);
            }
          } while (val != '\0');
        }
      }
    }

    UInt numObjUpdates;
    sei_read_uvlc(pDecodedMessageOutputStream, numObjUpdates, "ar_num_object_updates");
    assert(numObjUpdates<256);
    sei.m_annotatedRegions.clear();
    sei.m_annotatedRegions.resize(numObjUpdates);
    for (auto it=sei.m_annotatedRegions.begin(); it!=sei.m_annotatedRegions.end(); it++)
    {
      sei_read_uvlc(pDecodedMessageOutputStream, val, "ar_object_idx"); it->first=val;
      assert(val<256);
      SEIAnnotatedRegions::AnnotatedRegionObject &ar = it->second;
      sei_read_flag(pDecodedMessageOutputStream, val, "ar_object_cancel_flag");                           ar.objectCancelFlag = val;
      ar.objectLabelValid=false;
      ar.boundingBoxValid=false;

      if (!ar.objectCancelFlag)
      {
        if (sei.m_hdr.m_objectLabelPresentFlag)
        {
          sei_read_flag(pDecodedMessageOutputStream, val, "ar_object_label_update_flag");             ar.objectLabelValid = val;
          if (ar.objectLabelValid)
          {
            sei_read_uvlc(pDecodedMessageOutputStream, val, "ar_object_label_idx");                      ar.objLabelIdx = val;
            assert(val<256);
          }
        }
        sei_read_flag(pDecodedMessageOutputStream, val, "ar_bounding_box_update_flag");              ar.boundingBoxValid = val;
        if (ar.boundingBoxValid)
        {
          sei_read_flag(pDecodedMessageOutputStream, val, "ar_bounding_box_cancel_flag");             ar.boundingBoxCancelFlag = val;
          if (!ar.boundingBoxCancelFlag)
          {
            sei_read_code(pDecodedMessageOutputStream, 16, val, "ar_bounding_box_top");                      ar.boundingBoxTop = val;
            sei_read_code(pDecodedMessageOutputStream, 16, val, "ar_bounding_box_left");                     ar.boundingBoxLeft = val;
            sei_read_code(pDecodedMessageOutputStream, 16, val, "ar_bounding_box_width");                    ar.boundingBoxWidth = val;
            sei_read_code(pDecodedMessageOutputStream, 16, val, "ar_bounding_box_height");                   ar.boundingBoxHeight = val;
            if (sei.m_hdr.m_partialObjectFlagPresentFlag)
            {
              sei_read_flag(pDecodedMessageOutputStream, val, "ar_partial_object_flag");                ar.partialObjectFlag = val;
            }
            if (sei.m_hdr.m_objectConfidenceInfoPresentFlag)
            {
              sei_read_code(pDecodedMessageOutputStream, sei.m_hdr.m_objectConfidenceLength, val, "ar_object_confidence"); ar.objectConfidence = val;
            }
          }
        }
      }
    }
  }
}

Void SEIReader::xParseSEICubemapProjection(SEICubemapProjection& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  UInt val;

  sei_read_flag(pDecodedMessageOutputStream, val, "cmp_cancel_flag");                      sei.m_cmpCancelFlag = val;
  if (!sei.m_cmpCancelFlag)
  {
    sei_read_flag(pDecodedMessageOutputStream, val, "cmp_persistence_flag");                 sei.m_cmpPersistenceFlag = val;
  }
}



Void SEIReader::xParseSEIRegionWisePacking(SEIRegionWisePacking& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  UInt val;

  sei_read_flag( pDecodedMessageOutputStream,           val,      "rwp_cancel_flag" );                      sei.m_rwpCancelFlag = val;
  if (!sei.m_rwpCancelFlag)
  {
    sei_read_flag( pDecodedMessageOutputStream,           val,    "rwp_persistence_flag" );                 sei.m_rwpPersistenceFlag = val;
    sei_read_flag( pDecodedMessageOutputStream,           val,    "constituent_picture_matching_flag" );    sei.m_constituentPictureMatchingFlag = val;
    sei_read_code( pDecodedMessageOutputStream,       5,  val,    "rwp_reserved_zero_5bits" );
    sei_read_code( pDecodedMessageOutputStream,       8,  val,    "num_packed_regions" );                   sei.m_numPackedRegions = val;
    sei_read_code( pDecodedMessageOutputStream,       32, val,    "proj_picture_width" );                   sei.m_projPictureWidth = val;
    sei_read_code( pDecodedMessageOutputStream,       32, val,    "proj_picture_height" );                  sei.m_projPictureHeight = val;
    sei_read_code( pDecodedMessageOutputStream,       16, val,    "packed_picture_width" );                 sei.m_packedPictureWidth = val;
    sei_read_code( pDecodedMessageOutputStream,       16, val,    "packed_picture_height" );                sei.m_packedPictureHeight = val;
    
    sei.m_rwpTransformType.resize(sei.m_numPackedRegions);
    sei.m_rwpGuardBandFlag.resize(sei.m_numPackedRegions);
    sei.m_projRegionWidth.resize(sei.m_numPackedRegions);
    sei.m_projRegionHeight.resize(sei.m_numPackedRegions);
    sei.m_rwpProjRegionTop.resize(sei.m_numPackedRegions);
    sei.m_projRegionLeft.resize(sei.m_numPackedRegions);
    sei.m_packedRegionWidth.resize(sei.m_numPackedRegions);
    sei.m_packedRegionHeight.resize(sei.m_numPackedRegions);
    sei.m_packedRegionTop.resize(sei.m_numPackedRegions);
    sei.m_packedRegionLeft.resize(sei.m_numPackedRegions);
    sei.m_rwpLeftGuardBandWidth.resize(sei.m_numPackedRegions);
    sei.m_rwpRightGuardBandWidth.resize(sei.m_numPackedRegions);
    sei.m_rwpTopGuardBandHeight.resize(sei.m_numPackedRegions);
    sei.m_rwpBottomGuardBandHeight.resize(sei.m_numPackedRegions);
    sei.m_rwpGuardBandNotUsedForPredFlag.resize(sei.m_numPackedRegions);
    sei.m_rwpGuardBandType.resize(4*sei.m_numPackedRegions);

    for( Int i=0; i < sei.m_numPackedRegions; i++ )
    {
      sei_read_code( pDecodedMessageOutputStream,     4,  val,    "rwp_reserved_zero_4bits" );
      sei_read_code( pDecodedMessageOutputStream,     3,  val,    "rwp_tTransform_type" );                  sei.m_rwpTransformType[i] = val;
      sei_read_flag( pDecodedMessageOutputStream,         val,    "rwp_guard_band_flag" );                  sei.m_rwpGuardBandFlag[i] = val;
      sei_read_code( pDecodedMessageOutputStream,     32, val,    "proj_region_width" );                    sei.m_projRegionWidth[i] = val;
      sei_read_code( pDecodedMessageOutputStream,     32, val,    "proj_region_height" );                   sei.m_projRegionHeight[i] = val;
      sei_read_code( pDecodedMessageOutputStream,     32, val,    "rwp_proj_regionTop" );                   sei.m_rwpProjRegionTop[i] = val;
      sei_read_code( pDecodedMessageOutputStream,     32, val,    "proj_region_left" );                     sei.m_projRegionLeft[i] = val;
      sei_read_code( pDecodedMessageOutputStream,     16, val,    "packed_region_width" );                  sei.m_packedRegionWidth[i] = val;
      sei_read_code( pDecodedMessageOutputStream,     16, val,    "packed_region_height" );                 sei.m_packedRegionHeight[i] = val;
      sei_read_code( pDecodedMessageOutputStream,     16, val,    "packed_region_top" );                    sei.m_packedRegionTop[i] = val;
      sei_read_code( pDecodedMessageOutputStream,     16, val,    "packed_region_left" );                   sei.m_packedRegionLeft[i] = val;
      if( sei.m_rwpGuardBandFlag[i] )
      {
        sei_read_code( pDecodedMessageOutputStream,   8,  val,    "rwp_left_guard_band_width" );            sei.m_rwpLeftGuardBandWidth[i] = val;
        sei_read_code( pDecodedMessageOutputStream,   8,  val,    "rwp_right_guard_band_width" );           sei.m_rwpRightGuardBandWidth[i] = val;
        sei_read_code( pDecodedMessageOutputStream,   8,  val,    "rwp_top_guard_band_height" );            sei.m_rwpTopGuardBandHeight[i]  = val;
        sei_read_code( pDecodedMessageOutputStream,   8,  val,    "rwp_bottom_guard_band_height" );         sei. m_rwpBottomGuardBandHeight[i]  = val;
        sei_read_flag( pDecodedMessageOutputStream,       val,    "rwp_guard_band_not_used_forPred_flag" ); sei.m_rwpGuardBandNotUsedForPredFlag[i] = val;
        for( Int j=0; j < 4; j++ )
        {
          sei_read_code( pDecodedMessageOutputStream, 3,  val,     "rwp_guard_band_type" ); sei.m_rwpGuardBandType[i*4 + j] = val;
        }
        sei_read_code( pDecodedMessageOutputStream,   3,  val,    "rwp_guard_band_reserved_zero_3bits" );
      }
    }
  }
}

Void SEIReader::xParseSEIFisheyeVideoInfo(SEIFisheyeVideoInfo& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt val;

  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  sei.values = TComSEIFisheyeVideoInfo();
  
  TComSEIFisheyeVideoInfo &info=sei.values;

  sei_read_flag(pDecodedMessageOutputStream, val, "fisheye_cancel_flag"); info.m_fisheyeCancelFlag = val;
  if (!info.m_fisheyeCancelFlag)
  {
    Int sval;

    sei_read_flag(pDecodedMessageOutputStream, val, "fisheye_persistence_flag");        info.m_fisheyePersistenceFlag = val;
    sei_read_code(pDecodedMessageOutputStream, 3, val, "fisheye_view_dimension_idc");   info.m_fisheyeViewDimensionIdc = val;
    sei_read_code(pDecodedMessageOutputStream, 3, val, "fisheye_reserved_zero_3bits");
    sei_read_code(pDecodedMessageOutputStream, 8, val, "fisheye_num_active_area_minus1");
    info.m_fisheyeActiveAreas.resize(val+1);

    for (std::size_t i = 0; i < info.m_fisheyeActiveAreas.size(); i++)
    {
      TComSEIFisheyeVideoInfo::ActiveAreaInfo &area=info.m_fisheyeActiveAreas[i];
      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_circular_region_centre_x[i]");  area.m_fisheyeCircularRegionCentreX = val;
      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_circular_region_centre_y[i]");  area.m_fisheyeCircularRegionCentreY = val;
      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_rect_region_top[i]");           area.m_fisheyeRectRegionTop = val;
      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_rect_region_left[i]");          area.m_fisheyeRectRegionLeft = val;
      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_rect_region_width[i]");         area.m_fisheyeRectRegionWidth = val;
      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_rect_region_Height[i]");        area.m_fisheyeRectRegionHeight = val;
      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_circular_region_radius[i]");    area.m_fisheyeCircularRegionRadius = val;
      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_scene_radius[i]");              area.m_fisheyeSceneRadius = val;

      sei_read_scode(pDecodedMessageOutputStream, 32, sval, "fisheye_camera_centre_azimuth[i]");   area.m_fisheyeCameraCentreAzimuth = sval;
      sei_read_scode(pDecodedMessageOutputStream, 32, sval, "fisheye_camera_centre_elevation[i]"); area.m_fisheyeCameraCentreElevation = sval;
      sei_read_scode(pDecodedMessageOutputStream, 32, sval, "fisheye_camera_centre_tilt[i]");      area.m_fisheyeCameraCentreTilt = sval;

      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_camera_centre_offset_x[i]");    area.m_fisheyeCameraCentreOffsetX = val;
      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_camera_centre_offset_y[i]");    area.m_fisheyeCameraCentreOffsetY = val;
      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_camera_centre_offset_z[i]");    area.m_fisheyeCameraCentreOffsetZ = val;
      sei_read_code(pDecodedMessageOutputStream, 32, val, "fisheye_field_of_view[i]");             area.m_fisheyeFieldOfView = val;
      sei_read_code(pDecodedMessageOutputStream, 16, val, "fisheye_num_polynomial_coeffs[i]");
      area.m_fisheyePolynomialCoeff.resize(val);

      for (std::size_t j = 0; j < area.m_fisheyePolynomialCoeff.size(); j++)
      {
        sei_read_scode(pDecodedMessageOutputStream, 32, sval, "fisheye_polynomial_coeff[i][j]");   area.m_fisheyePolynomialCoeff[j] = sval;
      }
    }
  }
}

Void SEIReader::xParseSEIColourRemappingInfo(SEIColourRemappingInfo& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream)
{
  UInt  uiVal;
  Int   iVal;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_uvlc( pDecodedMessageOutputStream, uiVal, "colour_remap_id" );          sei.m_colourRemapId = uiVal;
  sei_read_flag( pDecodedMessageOutputStream, uiVal, "colour_remap_cancel_flag" ); sei.m_colourRemapCancelFlag = uiVal;
  if( !sei.m_colourRemapCancelFlag ) 
  {
    sei_read_flag( pDecodedMessageOutputStream, uiVal, "colour_remap_persistence_flag" );                sei.m_colourRemapPersistenceFlag = uiVal;
    sei_read_flag( pDecodedMessageOutputStream, uiVal, "colour_remap_video_signal_info_present_flag" );  sei.m_colourRemapVideoSignalInfoPresentFlag = uiVal;
    if ( sei.m_colourRemapVideoSignalInfoPresentFlag )
    {
      sei_read_flag( pDecodedMessageOutputStream, uiVal,    "colour_remap_full_range_flag" );            sei.m_colourRemapFullRangeFlag = uiVal;
      sei_read_code( pDecodedMessageOutputStream, 8, uiVal, "colour_remap_primaries" );                  sei.m_colourRemapPrimaries = uiVal;
      sei_read_code( pDecodedMessageOutputStream, 8, uiVal, "colour_remap_transfer_function" );          sei.m_colourRemapTransferFunction = uiVal;
      sei_read_code( pDecodedMessageOutputStream, 8, uiVal, "colour_remap_matrix_coefficients" );        sei.m_colourRemapMatrixCoefficients = uiVal;
    }
    sei_read_code( pDecodedMessageOutputStream, 8, uiVal, "colour_remap_input_bit_depth" );              sei.m_colourRemapInputBitDepth = uiVal;
    sei_read_code( pDecodedMessageOutputStream, 8, uiVal, "colour_remap_bit_depth" );                    sei.m_colourRemapBitDepth = uiVal;
  
    for( Int c=0 ; c<3 ; c++ )
    {
      sei_read_code( pDecodedMessageOutputStream, 8, uiVal, "pre_lut_num_val_minus1[c]" ); sei.m_preLutNumValMinus1[c] = (uiVal==0) ? 1 : uiVal;
      sei.m_preLut[c].resize(sei.m_preLutNumValMinus1[c]+1);
      if( uiVal> 0 )
      {
        for ( Int i=0 ; i<=sei.m_preLutNumValMinus1[c] ; i++ )
        {
          sei_read_code( pDecodedMessageOutputStream, (( sei.m_colourRemapInputBitDepth   + 7 ) >> 3 ) << 3, uiVal, "pre_lut_coded_value[c][i]" );  sei.m_preLut[c][i].codedValue  = uiVal;
          sei_read_code( pDecodedMessageOutputStream, (( sei.m_colourRemapBitDepth + 7 ) >> 3 ) << 3, uiVal, "pre_lut_target_value[c][i]" ); sei.m_preLut[c][i].targetValue = uiVal;
        }
      }
      else // pre_lut_num_val_minus1[c] == 0
      {
        sei.m_preLut[c][0].codedValue  = 0;
        sei.m_preLut[c][0].targetValue = 0;
        sei.m_preLut[c][1].codedValue  = (1 << sei.m_colourRemapInputBitDepth) - 1 ;
        sei.m_preLut[c][1].targetValue = (1 << sei.m_colourRemapBitDepth) - 1 ;
      }
    }

    sei_read_flag( pDecodedMessageOutputStream, uiVal,      "colour_remap_matrix_present_flag" ); sei.m_colourRemapMatrixPresentFlag = uiVal;
    if( sei.m_colourRemapMatrixPresentFlag )
    {
      sei_read_code( pDecodedMessageOutputStream, 4, uiVal, "log2_matrix_denom" ); sei.m_log2MatrixDenom = uiVal;
      for ( Int c=0 ; c<3 ; c++ )
      {
        for ( Int i=0 ; i<3 ; i++ )
        {
          sei_read_svlc( pDecodedMessageOutputStream, iVal, "colour_remap_coeffs[c][i]" ); sei.m_colourRemapCoeffs[c][i] = iVal;
        }
      }
    }
    else // setting default matrix (I3)
    {
      sei.m_log2MatrixDenom = 10;
      for ( Int c=0 ; c<3 ; c++ )
      {
        for ( Int i=0 ; i<3 ; i++ )
        {
          sei.m_colourRemapCoeffs[c][i] = (c==i) << sei.m_log2MatrixDenom;
        }
      }
    }
    for( Int c=0 ; c<3 ; c++ )
    {
      sei_read_code( pDecodedMessageOutputStream, 8, uiVal, "post_lut_num_val_minus1[c]" ); sei.m_postLutNumValMinus1[c] = (uiVal==0) ? 1 : uiVal;
      sei.m_postLut[c].resize(sei.m_postLutNumValMinus1[c]+1);
      if( uiVal > 0 )
      {
        for ( Int i=0 ; i<=sei.m_postLutNumValMinus1[c] ; i++ )
        {
          sei_read_code( pDecodedMessageOutputStream, (( sei.m_colourRemapBitDepth + 7 ) >> 3 ) << 3, uiVal, "post_lut_coded_value[c][i]" );  sei.m_postLut[c][i].codedValue = uiVal;
          sei_read_code( pDecodedMessageOutputStream, (( sei.m_colourRemapBitDepth + 7 ) >> 3 ) << 3, uiVal, "post_lut_target_value[c][i]" ); sei.m_postLut[c][i].targetValue = uiVal;
        }
      }
      else
      {
        sei.m_postLut[c][0].codedValue  = 0;
        sei.m_postLut[c][0].targetValue = 0;
        sei.m_postLut[c][1].targetValue = (1 << sei.m_colourRemapBitDepth) - 1;
        sei.m_postLut[c][1].codedValue  = (1 << sei.m_colourRemapBitDepth) - 1;
      }
    }
  }
}


Void SEIReader::xParseSEIDeinterlaceFieldIdentification( SEIDeinterlaceFieldIdentification& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream )
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_flag(pDecodedMessageOutputStream, code, "deinterlaced_picture_source_parity_flag"); sei.m_deinterlacedPictureSourceParityFlag = code!=0;
}


Void SEIReader::xParseSEIContentLightLevelInfo( SEIContentLightLevelInfo& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream )
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_code(pDecodedMessageOutputStream, 16, code, "max_content_light_level");     sei.m_maxContentLightLevel    = code;
  sei_read_code(pDecodedMessageOutputStream, 16, code, "max_pic_average_light_level"); sei.m_maxPicAverageLightLevel = code;
}


Void SEIReader::xParseSEIDependentRAPIndication( SEIDependentRAPIndication& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream )
{
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
}


Void SEIReader::xParseSEICodedRegionCompletion( SEICodedRegionCompletion& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream )
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_uvlc(pDecodedMessageOutputStream, code, "next_segment_address"); sei.m_nextSegmentAddress= code;
  if (code)
  {
    sei_read_flag(pDecodedMessageOutputStream, code, "independent_slice_segment_flag"); sei.m_independentSliceSegmentFlag = code!=0;
  }
  else
  {
    sei.m_independentSliceSegmentFlag=false; // initialise to known value.
  }
}


Void SEIReader::xParseSEIAlternativeTransferCharacteristics(SEIAlternativeTransferCharacteristics& sei, UInt payloadSize, ostream* pDecodedMessageOutputStream)
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_code(pDecodedMessageOutputStream, 8, code, "preferred_transfer_characteristics"); sei.m_preferredTransferCharacteristics = code;
}


Void SEIReader::xParseSEIAmbientViewingEnvironment( SEIAmbientViewingEnvironment& sei, UInt payloadSize, std::ostream *pDecodedMessageOutputStream )
{
  UInt code;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);

  sei_read_code(pDecodedMessageOutputStream, 32, code, "ambient_illuminance"); sei.m_ambientIlluminance= code;
  sei_read_code(pDecodedMessageOutputStream, 16, code, "ambient_light_x");     sei.m_ambientLightX     = (UShort)code;
  sei_read_code(pDecodedMessageOutputStream, 16, code, "ambient_light_y");     sei.m_ambientLightY     = (UShort)code;
}
Void SEIReader::xParseSEIRegionalNesting( SEIRegionalNesting& sei, UInt payloadSize, const TComSPS *sps, std::ostream *pDecodedMessageOutputStream )
{
  UInt uiCode;
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  UInt numRegions, numSEIs;
  
  sei_read_code(pDecodedMessageOutputStream, 16, uiCode, "regional_nesting_id");
  sei_read_code(pDecodedMessageOutputStream,  8, uiCode, "regional_nesting_num_rect_regions"); numRegions = uiCode;

  sei.clearRegions();
  for(UInt i = 0; i < numRegions; i++)
  {
    RNSEIWindow region;
    Int lOffset, rOffset, tOffset, bOffset, regionId;
    sei_read_code(pDecodedMessageOutputStream,  8, uiCode, "regional_nesting_rect_region_id");      regionId = uiCode;
    sei_read_code(pDecodedMessageOutputStream, 16, uiCode, "regional_nesting_rect_left_offset");    lOffset  = uiCode;
    sei_read_code(pDecodedMessageOutputStream, 16, uiCode, "regional_nesting_rect_right_offset");   rOffset  = uiCode;
    sei_read_code(pDecodedMessageOutputStream, 16, uiCode, "regional_nesting_rect_top_offset");     tOffset  = uiCode;
    sei_read_code(pDecodedMessageOutputStream, 16, uiCode, "regional_nesting_rect_bottom_offset");  bOffset  = uiCode;
    region.setRegionId(regionId);
    region.setWindow(lOffset, rOffset, tOffset, bOffset);
    
    sei.addRegion(&region);
  }
  sei_read_code(pDecodedMessageOutputStream,  8, uiCode, "num_sei_messages_in_regional_nesting_minus1");      numSEIs = uiCode + 1;
  for(UInt i = 0; i < numSEIs; i++)
  {
    SEIRegionalNesting::SEIListOfIndices seiWithRegionIndices;
    UInt numRegionsForSEI;
    sei_read_code(pDecodedMessageOutputStream,  8, uiCode, "num_regions_for_sei_message[i]"); numRegionsForSEI = uiCode;
    for(UInt j = 0; j < numRegionsForSEI; j++) 
    {
      sei_read_code(pDecodedMessageOutputStream,  8, uiCode, "regional_nesting_sei_region_idx[i][j]");
      seiWithRegionIndices.m_listOfIndices.push_back(uiCode);
    }

    SEIMessages seiReg;
    xReadSEImessage(seiReg, NAL_UNIT_PREFIX_SEI, sps, pDecodedMessageOutputStream, SEI::regional_nesting_sei_messages, std::string(""));
    seiWithRegionIndices.m_seiMessage = seiReg.front();
    sei.addRegionalSEI( seiWithRegionIndices ) ;
  }
}

#if JCTVC_AD0021_SEI_MANIFEST
Void SEIReader::xParseSEISEIManifest(SEIManifest& sei, UInt payloadSize, std::ostream* pDecodedMessageOutputStream)
{
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  UInt val;
  sei_read_code(pDecodedMessageOutputStream, 16, val, "manifest_num_sei_msg_types");          sei.m_manifestNumSeiMsgTypes = val;

  if (sei.m_manifestNumSeiMsgTypes > 0)
  {
    sei.m_manifestSeiPayloadType.resize(sei.m_manifestNumSeiMsgTypes);
    sei.m_manifestSeiDescription.resize(sei.m_manifestNumSeiMsgTypes);
    for (Int i = 0; i < sei.m_manifestNumSeiMsgTypes; i++)
    {
      sei_read_code(pDecodedMessageOutputStream, 16, val, "manifest_sei_payload_types");           sei.m_manifestSeiPayloadType[i] = val;
      sei_read_code(pDecodedMessageOutputStream, 8, val, "manifest_sei_description");             sei.m_manifestSeiDescription[i] = val;
    }
  }
}
#endif

#if JCTVC_AD0021_SEI_PREFIX_INDICATION
Void SEIReader::xParseSEISEIPrefixIndication(SEIPrefixIndication& sei, UInt payloadSize, std::ostream* pDecodedMessageOutputStream)
{
  output_sei_message_header(sei, pDecodedMessageOutputStream, payloadSize);
  UInt val;
  UInt bitsRead = 0;
  sei_read_code(pDecodedMessageOutputStream, 16, val, "prefix_sei_payload_type");                sei.m_prefixSeiPayloadType = val;
  sei_read_code(pDecodedMessageOutputStream, 8, val, "num_sei_prefix_indications_minus1");      sei.m_numSeiPrefixIndicationsMinus1 = val;
  if (sei.m_numSeiPrefixIndicationsMinus1 >= 0)
  {
    sei.m_numBitsInPrefixIndicationMinus1.resize(sei.m_numSeiPrefixIndicationsMinus1 + 1);
    sei.m_seiPrefixDataBit.resize(sei.m_numSeiPrefixIndicationsMinus1 + 1);
    for (Int i = 0; i <= sei.m_numSeiPrefixIndicationsMinus1; i++)
    {
      sei_read_code(pDecodedMessageOutputStream, 16, val, "num_bits_in_prefix_indication_minus1");   sei.m_numBitsInPrefixIndicationMinus1[i] = val;
      sei.m_seiPrefixDataBit[i].resize(sei.m_numBitsInPrefixIndicationMinus1[i] + 1);
      for (Int j = 0; j <= sei.m_numBitsInPrefixIndicationMinus1[i]; j++)
      {
        sei_read_code(pDecodedMessageOutputStream, 1, val, "sei_prefix_data_bit");   sei.m_seiPrefixDataBit[i][j] = val;
        bitsRead += 1;
      }
      while (bitsRead % 8 != 0)
      {
        sei_read_code(pDecodedMessageOutputStream, 1, val, "byte_alignment_bit_equal_to_one");
        bitsRead += 1;
      }
    }
  }
}
#endif


//! \}
