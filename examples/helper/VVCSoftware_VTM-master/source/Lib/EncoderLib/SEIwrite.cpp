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

#include "CommonLib/BitStream.h"
#include "CommonLib/SEI.h"
#include "CommonLib/Slice.h"
#include "CommonLib/Picture.h"
#include "CommonLib/dtrace_next.h"
#include "SEIwrite.h"

//! \ingroup EncoderLib
//! \{

void SEIWriter::xWriteSEIpayloadData(OutputBitstream &bs, const SEI &sei, HRD &hrd, const uint32_t temporalId,
                                     int SEIPrefixIndicationIdx)
{
  const SEIBufferingPeriod *bp = nullptr;
  switch (sei.payloadType())
  {
  case SEI::PayloadType::USER_DATA_UNREGISTERED:
    xWriteSEIuserDataUnregistered(reinterpret_cast<const SEIUserDataUnregistered&>(sei));
    break;
  case SEI::PayloadType::DECODING_UNIT_INFO:
    bp = hrd.getBufferingPeriodSEI();
    CHECK (bp == nullptr, "Buffering Period need to be initialized in HRD to allow writing of Decoding Unit Information SEI");
    xWriteSEIDecodingUnitInfo(*static_cast<const SEIDecodingUnitInfo*>(& sei), *bp, temporalId);
    break;
  case SEI::PayloadType::SCALABLE_NESTING:
    xWriteSEIScalableNesting(bs, *static_cast<const SEIScalableNesting*>(&sei));
    break;
  case SEI::PayloadType::DECODED_PICTURE_HASH:
    xWriteSEIDecodedPictureHash(*static_cast<const SEIDecodedPictureHash*>(&sei));
    break;
  case SEI::PayloadType::BUFFERING_PERIOD:
    xWriteSEIBufferingPeriod(*static_cast<const SEIBufferingPeriod*>(&sei));
    hrd.setBufferingPeriodSEI(static_cast<const SEIBufferingPeriod*>(&sei));
    break;
  case SEI::PayloadType::PICTURE_TIMING:
    bp = hrd.getBufferingPeriodSEI();
    CHECK(bp == nullptr, "Buffering Period need to be initialized in HRD to allow writing of Picture Timing SEI");
    xWriteSEIPictureTiming(*static_cast<const SEIPictureTiming *>(&sei), *bp, temporalId);
    break;
  case SEI::PayloadType::FRAME_FIELD_INFO:
    xWriteSEIFrameFieldInfo(*static_cast<const SEIFrameFieldInfo *>(&sei));
    break;
  case SEI::PayloadType::DEPENDENT_RAP_INDICATION:
    xWriteSEIDependentRAPIndication(*static_cast<const SEIDependentRAPIndication *>(&sei));
    break;
  case SEI::PayloadType::EXTENDED_DRAP_INDICATION:
    xWriteSEIEdrapIndication(*static_cast<const SEIExtendedDrapIndication *>(&sei));
    break;
  case SEI::PayloadType::FRAME_PACKING:
    xWriteSEIFramePacking(*static_cast<const SEIFramePacking *>(&sei), SEIPrefixIndicationIdx);
    break;
#if GREEN_METADATA_SEI_ENABLED
  case SEI::PayloadType::GREEN_METADATA:
    xWriteSEIGreenMetadataInfo(*static_cast<const SEIGreenMetadataInfo *>(&sei));
    break;
#endif
  case SEI::PayloadType::DISPLAY_ORIENTATION:
    xWriteSEIDisplayOrientation(*static_cast<const SEIDisplayOrientation *>(&sei));
    break;
  case SEI::PayloadType::PARAMETER_SETS_INCLUSION_INDICATION:
    xWriteSEIParameterSetsInclusionIndication(*static_cast<const SEIParameterSetsInclusionIndication *>(&sei));
    break;
  case SEI::PayloadType::MASTERING_DISPLAY_COLOUR_VOLUME:
    xWriteSEIMasteringDisplayColourVolume(*static_cast<const SEIMasteringDisplayColourVolume *>(&sei));
    break;
  case SEI::PayloadType::ALTERNATIVE_TRANSFER_CHARACTERISTICS:
    xWriteSEIAlternativeTransferCharacteristics(*static_cast<const SEIAlternativeTransferCharacteristics *>(&sei));
    break;
  case SEI::PayloadType::EQUIRECTANGULAR_PROJECTION:
    xWriteSEIEquirectangularProjection(*static_cast<const SEIEquirectangularProjection *>(&sei),
                                       SEIPrefixIndicationIdx);
    break;
  case SEI::PayloadType::SPHERE_ROTATION:
    xWriteSEISphereRotation(*static_cast<const SEISphereRotation *>(&sei), SEIPrefixIndicationIdx);
    break;
  case SEI::PayloadType::OMNI_VIEWPORT:
    xWriteSEIOmniViewport(*static_cast<const SEIOmniViewport *>(&sei));
    break;
  case SEI::PayloadType::REGION_WISE_PACKING:
    xWriteSEIRegionWisePacking(*static_cast<const SEIRegionWisePacking *>(&sei), SEIPrefixIndicationIdx);
    break;
  case SEI::PayloadType::GENERALIZED_CUBEMAP_PROJECTION:
    xWriteSEIGeneralizedCubemapProjection(*static_cast<const SEIGeneralizedCubemapProjection *>(&sei),
                                          SEIPrefixIndicationIdx);
    break;
  case SEI::PayloadType::SCALABILITY_DIMENSION_INFO:
    xWriteSEIScalabilityDimensionInfo(*static_cast<const SEIScalabilityDimensionInfo *>(&sei));
    break;
  case SEI::PayloadType::MULTIVIEW_ACQUISITION_INFO:
    xWriteSEIMultiviewAcquisitionInfo(*static_cast<const SEIMultiviewAcquisitionInfo *>(&sei));
    break;
  case SEI::PayloadType::MULTIVIEW_VIEW_POSITION:
    xWriteSEIMultiviewViewPosition(*static_cast<const SEIMultiviewViewPosition *>(&sei));
    break;
  case SEI::PayloadType::ALPHA_CHANNEL_INFO:
    xWriteSEIAlphaChannelInfo(*static_cast<const SEIAlphaChannelInfo *>(&sei));
    break;
  case SEI::PayloadType::DEPTH_REPRESENTATION_INFO:
    xWriteSEIDepthRepresentationInfo(*static_cast<const SEIDepthRepresentationInfo *>(&sei));
    break;
  case SEI::PayloadType::USER_DATA_REGISTERED_ITU_T_T35:
    xWriteSEIUserDataRegistered(*static_cast<const SEIUserDataRegistered *>(&sei));
    break;
  case SEI::PayloadType::FILM_GRAIN_CHARACTERISTICS:
    xWriteSEIFilmGrainCharacteristics(*static_cast<const SEIFilmGrainCharacteristics *>(&sei));
    break;
  case SEI::PayloadType::CONTENT_LIGHT_LEVEL_INFO:
    xWriteSEIContentLightLevelInfo(*static_cast<const SEIContentLightLevelInfo *>(&sei));
    break;
  case SEI::PayloadType::AMBIENT_VIEWING_ENVIRONMENT:
    xWriteSEIAmbientViewingEnvironment(*static_cast<const SEIAmbientViewingEnvironment *>(&sei));
    break;
  case SEI::PayloadType::CONTENT_COLOUR_VOLUME:
    xWriteSEIContentColourVolume(*static_cast<const SEIContentColourVolume *>(&sei));
    break;
  case SEI::PayloadType::COLOUR_TRANSFORM_INFO:
    xWriteSEIColourTransformInfo(*static_cast<const SEIColourTransformInfo *>(&sei));
    break;
  case SEI::PayloadType::SUBPICTURE_LEVEL_INFO:
    xWriteSEISubpictureLevelInfo(*static_cast<const SEISubpictureLevelInfo*>(&sei));
    break;
  case SEI::PayloadType::SAMPLE_ASPECT_RATIO_INFO:
    xWriteSEISampleAspectRatioInfo(*static_cast<const SEISampleAspectRatioInfo *>(&sei));
    break;
  case SEI::PayloadType::PHASE_INDICATION:
    xWriteSEIPhaseIndication(*static_cast<const SEIPhaseIndication *>(&sei));
    break;
  case SEI::PayloadType::ANNOTATED_REGIONS:
    xWriteSEIAnnotatedRegions(*static_cast<const SEIAnnotatedRegions *>(&sei));
    break;
  case SEI::PayloadType::OBJECT_MASK_INFO:
    xWriteSEIObjectMaskInfos(*static_cast<const SEIObjectMaskInfos*>(&sei));
    break;
  case SEI::PayloadType::SEI_MANIFEST:
    CHECK((SEIPrefixIndicationIdx), "wrong SEI prefix indication message");
    xWriteSEISEIManifest(*static_cast<const SEIManifest *>(&sei));
    break;
  case SEI::PayloadType::SEI_PREFIX_INDICATION:
    CHECK((SEIPrefixIndicationIdx), "wrong SEI prefix indication message");
    xWriteSEISEIPrefixIndication(bs, *static_cast<const SEIPrefixIndication *>(&sei), hrd, temporalId);
    break;
  case SEI::PayloadType::CONSTRAINED_RASL_ENCODING:
    xWriteSEIConstrainedRaslIndication(*static_cast<const SEIConstrainedRaslIndication *>(&sei));
    break;
  case SEI::PayloadType::SHUTTER_INTERVAL_INFO:
    xWriteSEIShutterInterval(*static_cast<const SEIShutterIntervalInfo *>(&sei));
    break;
  case SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_CHARACTERISTICS:
    xWriteSEINeuralNetworkPostFilterCharacteristics(
      *static_cast<const SEINeuralNetworkPostFilterCharacteristics *>(&sei));
    break;
  case SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_ACTIVATION:
    xWriteSEINeuralNetworkPostFilterActivation(*static_cast<const SEINeuralNetworkPostFilterActivation *>(&sei));
    break;
  case SEI::PayloadType::SEI_PROCESSING_ORDER:
    xWriteSEIProcessingOrder(bs, *static_cast<const SEIProcessingOrderInfo*>(&sei));
    break;
  case SEI::PayloadType::SEI_PROCESSING_ORDER_NESTING:
    xWriteSEIProcessingOrderNesting(bs, *static_cast<const SEIProcessingOrderNesting*>(&sei));
    break;
  case SEI::PayloadType::POST_FILTER_HINT:
    xWriteSEIPostFilterHint(*static_cast<const SEIPostFilterHint *>(&sei));
    break;
  case SEI::PayloadType::ENCODER_OPTIMIZATION_INFO:
    xWriteSEIEncoderOptimizationInfo(*static_cast<const SEIEncoderOptimizationInfo *>(&sei));
    break;
  case SEI::PayloadType::SOURCE_PICTURE_TIMING_INFO:
    xWriteSEISourcePictureTimingInfo(*static_cast<const SEISourcePictureTimingInfo*>(&sei));
    break;
  case SEI::PayloadType::MODALITY_INFORMATION:
    xWriteSEIModalityInfo(*static_cast<const SEIModalityInfo *>(&sei));
    break;
  case SEI::PayloadType::TEXT_DESCRIPTION:
    xWriteSEITextDescription(*static_cast<const SEITextDescription*>(&sei));
    break;
  case SEI::PayloadType::GENERATIVE_FACE_VIDEO:
    xWriteSEIGenerativeFaceVideo(*static_cast<const SEIGenerativeFaceVideo*>(&sei));
    break;
  case SEI::PayloadType::GENERATIVE_FACE_VIDEO_ENHANCEMENT:
    xWriteSEIGenerativeFaceVideoEnhancement(*static_cast<const SEIGenerativeFaceVideoEnhancement*>(&sei));
    break;

#if JVET_AJ0151_DSC_SEI
  case SEI::PayloadType::DIGITALLY_SIGNED_CONTENT_INITIALIZATION:
    xWriteSEIDigitallySignedContentInitialization(*static_cast<const SEIDigitallySignedContentInitialization *>(&sei));
    break;
  case SEI::PayloadType::DIGITALLY_SIGNED_CONTENT_SELECTION:
    xWriteSEIDigitallySignedContentSelection(*static_cast<const SEIDigitallySignedContentSelection *>(&sei));
    break;
  case SEI::PayloadType::DIGITALLY_SIGNED_CONTENT_VERIFICATION:
    xWriteSEIDigitallySignedContentVerification(*static_cast<const SEIDigitallySignedContentVerification *>(&sei));
    break;
#endif
#if JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
  case SEI::PayloadType::AI_USAGE_RESTRICTIONS:
    xWriteSEIAIUsageRestrictions(*static_cast<const SEIAIUsageRestrictions *>(&sei));
    break;
#endif 
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  case SEI::PayloadType::PACKED_REGIONS_INFO:
    xWriteSEIPackedRegionsInfo(*static_cast<const SEIPackedRegionsInfo*>(&sei));
    break;
#endif
  default:
    THROW("Trying to write unhandled SEI message");
    break;
  }
  if (SEIPrefixIndicationIdx)
  {
    return;
  }
  xWriteByteAlign();
}

/**
 * marshal all SEI messages in provided list into one bitstream bs
 */
void SEIWriter::writeSEImessages(OutputBitstream& bs, const SEIMessages &seiList, HRD &hrd, bool isNested, const uint32_t temporalId)
{
#if ENABLE_TRACING
  if (g_HLSTraceEnable)
    xTraceSEIHeader();
#endif

  OutputBitstream bs_count;

  for (SEIMessages::const_iterator sei=seiList.begin(); sei!=seiList.end(); sei++)
  {
    // calculate how large the payload data is
    // TODO: this would be far nicer if it used vectored buffers
    bs_count.clear();
    setBitstream(&bs_count);

#if ENABLE_TRACING
    bool traceEnable = g_HLSTraceEnable;
    g_HLSTraceEnable = false;
#endif
    xWriteSEIpayloadData(bs_count, **sei, hrd, temporalId);
#if ENABLE_TRACING
    g_HLSTraceEnable = traceEnable;
#endif
    uint32_t payload_data_num_bits = bs_count.getNumberOfWrittenBits();
    CHECK(0 != payload_data_num_bits % 8, "Invalid number of payload data bits");

    setBitstream(&bs);
    uint32_t payloadType = to_underlying((*sei)->payloadType());
    for (; payloadType >= 0xff; payloadType -= 0xff)
    {
      xWriteCode(0xff, 8, "payload_type");
    }
    xWriteCode(payloadType, 8, "payload_type");

    uint32_t payloadSize = payload_data_num_bits/8;
    for (; payloadSize >= 0xff; payloadSize -= 0xff)
    {
      xWriteCode(0xff, 8, "payload_size");
    }
    xWriteCode(payloadSize, 8, "payload_size");

    /* payloadData */
#if ENABLE_TRACING
    if (g_HLSTraceEnable)
    {
      xTraceSEIMessageType((*sei)->payloadType());
    }
#endif

    xWriteSEIpayloadData(bs, **sei, hrd, temporalId);
  }
  if (!isNested)
  {
    xWriteRbspTrailingBits();
  }
}

/**
 * marshal a user_data_unregistered SEI message sei, storing the marshalled
 * representation in bitstream bs.
 */
void SEIWriter::xWriteSEIuserDataUnregistered(const SEIUserDataUnregistered& sei)
{
  for (uint32_t i = 0; i < sei.uuid.size(); i++)
  {
    xWriteCode(sei.uuid[i], 8, "uuid_iso_iec_11578[i]");
  }

  for (uint32_t i = 0; i < sei.data.size(); i++)
  {
    xWriteCode(sei.data[i], 8, "user_data_payload_byte");
  }
}

/**
 * marshal a decoded picture hash SEI message, storing the marshalled
 * representation in bitstream bs.
 */
void SEIWriter::xWriteSEIDecodedPictureHash(const SEIDecodedPictureHash& sei)
{
  const char *traceString="\0";
  switch (sei.method)
  {
  case HashType::MD5:
    traceString = "picture_md5";
    break;
  case HashType::CRC:
    traceString = "picture_crc";
    break;
  case HashType::CHECKSUM:
    traceString = "picture_checksum";
    break;
  default:
    THROW("Unknown hash type");
    break;
  }

  if (traceString != 0) //use of this variable is needed to avoid a compiler error with G++ 4.6.1
  {
    xWriteCode(to_underlying(sei.method), 8, "dph_sei_hash_type");
    xWriteCode(sei.singleCompFlag, 1, "dph_sei_single_component_flag");
    xWriteCode(0, 7, "dph_sei_reserved_zero_7bits");
    for(uint32_t i=0; i<uint32_t(sei.m_pictureHash.hash.size()); i++)
    {
      xWriteCode(sei.m_pictureHash.hash[i], 8, traceString);
    }
  }
}

void SEIWriter::xWriteSEIDecodingUnitInfo(const SEIDecodingUnitInfo& dui, const SEIBufferingPeriod& bp,
                                          const uint32_t temporalId)
{
  xWriteUvlc(dui.decodingUnitIdx, "dui_decoding_unit_idx");
  if (!bp.duCpbParamsInPicTimingSei)
  {
    for (int i = temporalId; i < bp.maxSublayers; i++)
    {
      if (i < bp.maxSublayers - 1)
      {
        xWriteFlag(dui.hasSublayerDelays[i] ? 1 : 0, "dui_sublayer_delays_present_flag[i]");
      }
      if (dui.hasSublayerDelays[i])
      {
        xWriteCode(dui.duCpbRemovalDelayIncrement[i], bp.duCpbRemovalDelayIncrementLength,
                   "dui_du_cpb_removal_delay_increment[i]");
      }
    }
  }
  if (!bp.duDpbParamsInPicTimingSei)
  {
    xWriteFlag(dui.hasDpbOutputDuDelay, "dui_dpb_output_du_delay_present_flag");
  }

  if (dui.hasDpbOutputDuDelay)
  {
    xWriteCode(dui.dpbOutputDuDelay, bp.dpbOutputDelayDuLength, "dui_dpb_output_du_delay");
  }
}

void SEIWriter::xWriteSEIBufferingPeriod(const SEIBufferingPeriod& sei)
{
  CHECK(!sei.hasHrdParams[HrdType::NAL] && !sei.hasHrdParams[HrdType::VCL],
        "at least one of bp_nal_hrd_params_present_flag and bp_vcl_hrd_params_present_flag must be true");
  xWriteFlag(sei.hasHrdParams[HrdType::NAL], "bp_nal_hrd_params_present_flag");
  xWriteFlag(sei.hasHrdParams[HrdType::VCL], "bp_vcl_hrd_params_present_flag");

  CHECK(sei.cpbInitialRemovalDelayLength < 1, "sei.cpbInitialRemovalDelayLength must be > 0");
  xWriteCode(sei.cpbInitialRemovalDelayLength - 1, 5, "bp_cpb_initial_removal_delay_length_minus1");

  CHECK(sei.cpbRemovalDelayLength < 1, "sei.cpbRemovalDelayLength must be > 0");
  xWriteCode(sei.cpbRemovalDelayLength - 1, 5, "bp_cpb_removal_delay_length_minus1");

  CHECK(sei.dpbOutputDelayLength < 1, "sei.dpbOutputDelayLength must be > 0");
  xWriteCode(sei.dpbOutputDelayLength - 1, 5, "bp_dpb_output_delay_length_minus1");

  xWriteFlag(sei.hasDuHrdParams, "bp_du_hrd_params_present_flag");
  if (sei.hasDuHrdParams)
  {
    CHECK(sei.duCpbRemovalDelayIncrementLength < 1, "sei.duCpbRemovalDelayIncrementLength must be > 0");
    xWriteCode(sei.duCpbRemovalDelayIncrementLength - 1, 5, "bp_du_cpb_removal_delay_increment_length_minus1");

    CHECK(sei.dpbOutputDelayDuLength < 1, "sei.dpbOutputDelayDuLength must be > 0");
    xWriteCode(sei.dpbOutputDelayDuLength - 1, 5, "bp_dpb_output_delay_du_length_minus1");

    xWriteFlag(sei.duCpbParamsInPicTimingSei, "bp_du_cpb_params_in_pic_timing_sei_flag");
    xWriteFlag(sei.duDpbParamsInPicTimingSei, "bp_du_dpb_params_in_pic_timing_sei_flag");
  }

  xWriteFlag(sei.concatenation, "bp_concatenation_flag");
  xWriteFlag(sei.hasAdditionalConcatenationInfo, "bp_additional_concatenation_info_present_flag");
  if (sei.hasAdditionalConcatenationInfo)
  {
    xWriteCode(sei.maxInitialRemovalDelayForConcatenation, sei.cpbInitialRemovalDelayLength,
               "bp_max_initial_removal_delay_for_concatenation");
  }

  CHECK(sei.cpbRemovalDelayDelta < 1, "sei.cpbRemovalDelayDelta must be > 0");
  xWriteCode(sei.cpbRemovalDelayDelta - 1, sei.cpbRemovalDelayLength, "bp_cpb_removal_delay_delta_minus1");

  CHECK(sei.maxSublayers < 1, "bp_max_sublayers must be > 0");
  xWriteCode(sei.maxSublayers - 1, 3, "bp_max_sublayers_minus1");
  if (sei.maxSublayers > 1)
  {
    xWriteFlag(sei.hasCpbRemovalDelayDeltas(), "bp_cpb_removal_delay_deltas_present_flag");
  }

  if (sei.hasCpbRemovalDelayDeltas())
  {
    xWriteUvlc(sei.numCpbRemovalDelayDeltas() - 1, "bp_num_cpb_removal_delay_deltas_minus1");
    for (int i = 0; i < sei.numCpbRemovalDelayDeltas(); i++)
    {
      xWriteCode(sei.cpbRemovalDelayDeltaVals[i], sei.cpbRemovalDelayLength, "bp_cpb_removal_delay_delta_val[i]");
    }
  }
  CHECK(sei.cpbCount < 1, "sei.cpbCount must be > 0");
  xWriteUvlc(sei.cpbCount - 1, "bp_cpb_cnt_minus1");
  if (sei.maxSublayers - 1 > 0)
  {
    xWriteFlag(sei.hasSublayerInitialCpbRemovalDelay, "bp_sublayer_initial_cpb_removal_delay_present_flag");
  }
  for (int i = (sei.hasSublayerInitialCpbRemovalDelay ? 0 : sei.maxSublayers - 1); i < sei.maxSublayers; i++)
  {
    for (auto hrdType: { HrdType::NAL, HrdType::VCL })
    {
      if (sei.hasHrdParams[hrdType])
      {
        for (int j = 0; j < sei.cpbCount; j++)
        {
          xWriteCode(sei.initialCpbRemoval[hrdType][i][j].delay, sei.cpbInitialRemovalDelayLength,
                     hrdType == HrdType::NAL ? "bp_nal_initial_cpb_removal_delay[i][j]"
                                             : "bp_vcl_initial_cpb_removal_delay[i][j]");
          xWriteCode(sei.initialCpbRemoval[hrdType][i][j].offset, sei.cpbInitialRemovalDelayLength,
                     hrdType == HrdType::NAL ? "bp_nal_initial_cpb_removal_offset[i][j]"
                                             : "bp_vcl_initial_cpb_removal_offset[i][j]");

          if (sei.hasDuHrdParams)
          {
            xWriteCode(sei.initialAltCpbRemoval[hrdType][i][j].delay, sei.cpbInitialRemovalDelayLength,
                       hrdType == HrdType::NAL ? "bp_nal_alt_initial_cpb_removal_delay[i][j]"
                                               : "bp_vcl_alt_initial_cpb_removal_delay[i][j]");
            xWriteCode(sei.initialAltCpbRemoval[hrdType][i][j].offset, sei.cpbInitialRemovalDelayLength,
                       hrdType == HrdType::NAL ? "bp_nal_alt_initial_cpb_removal_offset[i][j]"
                                               : "bp_vcl_alt_initial_cpb_removal_offset[i][j]");
          }
        }
      }
    }
  }
  if (sei.maxSublayers - 1 > 0)
  {
    xWriteFlag(sei.hasSublayerDpbOutputOffsets, "bp_sublayer_dpb_output_offsets_present_flag");
  }

  if (sei.hasSublayerDpbOutputOffsets)
  {
    for (int i = 0; i < sei.maxSublayers - 1; i++)
    {
      xWriteUvlc(sei.dpbOutputTidOffset[i], "bp_dpb_output_tid_offset[i]");
    }
  }
  xWriteFlag(sei.hasAltCpbParams, "bp_alt_cpb_params_present_flag");
  if (sei.hasAltCpbParams)
  {
    xWriteFlag(sei.useAltCpbParams, "bp_use_alt_cpb_params_flag");
  }
}

void SEIWriter::xWriteSEIPictureTiming(const SEIPictureTiming& pt, const SEIBufferingPeriod& bp,
                                       const uint32_t temporalId)
{
  CHECK(bp.maxSublayers < 1, "There must be at least one sublayer");
  xWriteCode(pt.cpbRemovalDelay[bp.maxSublayers - 1] - 1, bp.cpbRemovalDelayLength,
             "pt_cpb_removal_delay_minus1[bp_max_sub_layers_minus1]");
  for (int i = temporalId; i < bp.maxSublayers - 1; i++)
  {
    xWriteFlag(pt.hasSublayerDelays[i] ? 1 : 0, "pt_sublayer_delays_present_flag[i]");
    if (pt.hasSublayerDelays[i])
    {
      if (bp.hasCpbRemovalDelayDeltas())
      {
        xWriteFlag(pt.cpbRemovalDelayDeltaEnabled[i], "pt_cpb_removal_delay_delta_enabled_flag[i]");
      }
      if (pt.cpbRemovalDelayDeltaEnabled[i])
      {
        if (bp.numCpbRemovalDelayDeltas() > 1)
        {
          xWriteCode(pt.cpbRemovalDelayDeltaIdx[i], ceilLog2(bp.numCpbRemovalDelayDeltas()),
                     "pt_cpb_removal_delay_delta_idx[i]");
        }
      }
      else
      {
        CHECK(pt.cpbRemovalDelay[i] < 1, "CPB removal delay must be at least 1");
        xWriteCode(pt.cpbRemovalDelay[i] - 1, bp.cpbRemovalDelayLength, "pt_cpb_removal_delay_minus1[i]");
      }
    }
  }
  xWriteCode(pt.dpbOutputDelay, bp.dpbOutputDelayLength, "pt_dpb_output_delay");
  if (bp.hasAltCpbParams)
  {
    xWriteFlag(pt.hasAltTimingInfo, "pt_cpb_alt_timing_info_present_flag");
    if (pt.hasAltTimingInfo)
    {
      for (auto hrdType: { HrdType::NAL, HrdType::VCL })
      {
        if (bp.hasHrdParams[hrdType])
        {
          for (int i = (bp.hasSublayerInitialCpbRemovalDelay ? 0 : bp.maxSublayers - 1); i < bp.maxSublayers; ++i)
          {
            for (int j = 0; j < bp.cpbCount; j++)
            {
              xWriteCode(pt.initialAltCpbRemovalDelta[hrdType][i][j].delay, bp.cpbInitialRemovalDelayLength,
                         hrdType == HrdType::NAL ? "pt_nal_cpb_alt_initial_removal_delay_delta[ i ][ j ]"
                                                 : "pt_vcl_cpb_alt_initial_removal_delay_delta[ i ][ j ]");
              xWriteCode(pt.initialAltCpbRemovalDelta[hrdType][i][j].offset, bp.cpbInitialRemovalDelayLength,
                         hrdType == HrdType::NAL ? "pt_nal_cpb_alt_initial_removal_offset_delta[ i ][ j ]"
                                                 : "pt_vcl_cpb_alt_initial_removal_offset_delta[ i ][ j ]");
            }
            xWriteCode(pt.cpbDelayOffset[hrdType][i], bp.cpbRemovalDelayLength,
                       hrdType == HrdType::NAL ? "pt_nal_cpb_delay_offset[ i ]" : "pt_vcl_cpb_delay_offset[ i ]");
            xWriteCode(pt.dpbDelayOffset[hrdType][i], bp.dpbOutputDelayLength,
                       hrdType == HrdType::NAL ? "pt_nal_dpb_delay_offset[ i ]" : "pt_vcl_dpb_delay_offset[ i ]");
          }
        }
      }
    }
  }

  if (bp.hasDuHrdParams && bp.duDpbParamsInPicTimingSei)
  {
    xWriteCode(pt.dpbOutputDuDelay, bp.dpbOutputDelayDuLength, "pt_dpb_output_du_delay");
  }
  if (bp.hasDuHrdParams && bp.duCpbParamsInPicTimingSei)
  {
    CHECK(pt.getNumDecodingUnits() < 1, "there must be at least one DU");
    xWriteUvlc(pt.getNumDecodingUnits() - 1, "pt_num_decoding_units_minus1");
    if (pt.getNumDecodingUnits() > 1)
    {
      xWriteFlag(pt.duCommonCpbRemovalDelay, "pt_du_common_cpb_removal_delay_flag");
      if (pt.duCommonCpbRemovalDelay)
      {
        for (int i = temporalId; i < bp.maxSublayers; i++)
        {
          if (pt.hasSublayerDelays[i])
          {
            CHECK(pt.duCommonCpbRemovalDelayIncrement[i] < 1, "duCommonCpbRemovalDelayIncrement[i] must be at least 1");
            xWriteCode(pt.duCommonCpbRemovalDelayIncrement[i] - 1, bp.duCpbRemovalDelayIncrementLength,
                       "pt_du_common_cpb_removal_delay_increment_minus1[i]");
          }
        }
      }
      for (int i = 0; i < pt.getNumDecodingUnits(); i++)
      {
        CHECK(pt.numNalusInDu[i] < 1, "numNalusInDu[i] must be at least 1");
        xWriteUvlc(pt.numNalusInDu[i] - 1, "pt_num_nalus_in_du_minus1[i]");
        if (!pt.duCommonCpbRemovalDelay && i < pt.getNumDecodingUnits() - 1)
        {
          for (int j = temporalId; j < bp.maxSublayers; j++)
          {
            if (pt.hasSublayerDelays[j])
            {
              CHECK(pt.duCpbRemovalDelayIncrement[i][j] < 1, "duCpbRemovalDelayIncrement must be at least 1");
              xWriteCode(pt.duCpbRemovalDelayIncrement[i][j] - 1, bp.duCpbRemovalDelayIncrementLength,
                         "pt_du_cpb_removal_delay_increment_minus1[i][j]");
            }
          }
        }
      }
    }
  }

  if (bp.hasAdditionalConcatenationInfo)
  {
    xWriteFlag(pt.delayForConcatenationEnsured ? 1 : 0, "pt_delay_for_concatenation_ensured_flag");
  }

  CHECK(pt.displayElementalPeriods < 1, "displayElementalPeriods must be at least 1");
  xWriteCode(pt.displayElementalPeriods - 1, 8, "pt_display_elemental_periods_minus1");
}

void SEIWriter::xWriteSEIFrameFieldInfo(const SEIFrameFieldInfo& sei)
{
  xWriteFlag( sei.m_fieldPicFlag ? 1 : 0,                    "ffi_field_pic_flag" );
  if (sei.m_fieldPicFlag)
  {
    xWriteFlag( sei.m_bottomFieldFlag ? 1 : 0,               "ffi_bottom_field_flag" );
    xWriteFlag( sei.m_pairingIndicatedFlag ? 1 : 0,          "ffi_pairing_indicated_flag" );
    if (sei.m_pairingIndicatedFlag)
    {
      xWriteFlag( sei.m_pairedWithNextFieldFlag ? 1 : 0,     "ffi_paired_with_next_field_flag" );
    }
  }
  else
  {
    xWriteFlag( sei.m_displayFieldsFromFrameFlag ? 1 : 0,     "ffi_display_fields_from_frame_flag" );
    if (sei.m_displayFieldsFromFrameFlag)
    {
      xWriteFlag( sei.m_topFieldFirstFlag ? 1 : 0,            "ffi_top_field_first_flag" );
    }
    xWriteCode( sei.m_displayElementalPeriodsMinus1, 8,       "ffi_display_elemental_periods_minus1" );
  }
  xWriteCode( sei.m_sourceScanType, 2,                        "ffi_source_scan_type" );
  xWriteFlag( sei.m_duplicateFlag ? 1 : 0,                    "ffi_duplicate_flag" );
}

void SEIWriter::xWriteSEIDependentRAPIndication(const SEIDependentRAPIndication& /*sei*/)
{
  // intentionally empty
}

void SEIWriter::xWriteSEIEdrapIndication(const SEIExtendedDrapIndication& sei)
{
  xWriteCode( sei.m_edrapIndicationRapIdMinus1, 16,        "edrap_rap_id_minsu1" );
  xWriteFlag( sei.m_edrapIndicationLeadingPicturesDecodableFlag ? 1 : 0, "edrap_leading_pictures_decodable_flag" );
  xWriteCode( sei.m_edrapIndicationReservedZero12Bits, 12, "edrap_reserved_zero_12bits" );
  xWriteCode( sei.m_edrapIndicationNumRefRapPicsMinus1, 3, "edrap_num_ref_rap_pics_minus1" );
  for (int i = 0; i <= sei.m_edrapIndicationNumRefRapPicsMinus1; i++)
  {
    xWriteCode( sei.m_edrapIndicationRefRapId[i], 16, "edrap_ref_rap_id[i]" );
  }
}

void SEIWriter::xWriteSEIScalableNesting(OutputBitstream& bs, const SEIScalableNesting& sn)
{
  CHECK(sn.nestedSeis.size() < 1, "There must be at lease one SEI message nested in the scalable nesting SEI.")

  xWriteFlag(!sn.olsIdx.empty() ? 1 : 0, "sn_ols_flag");
  xWriteFlag(!sn.subpicId.empty() ? 1 : 0, "sn_subpic_flag");
  if (!sn.olsIdx.empty())
  {
    xWriteUvlc((uint32_t) sn.olsIdx.size() - 1, "sn_num_olss_minus1");
    for (uint32_t i = 0; i < sn.olsIdx.size(); i++)
    {
      const uint32_t pred = i == 0 ? 0 : sn.olsIdx[i - 1] + 1;
      CHECK(sn.olsIdx[i] < pred, "sn_ols_idx_delta_minus1 cannot be negative");
      xWriteUvlc(sn.olsIdx[i] - pred, "sn_ols_idx_delta_minus1[i]");
    }
  }
  else
  {
    xWriteFlag(sn.allLayersFlag() ? 1 : 0, "sn_all_layers_flag");
    if (!sn.allLayersFlag())
    {
      xWriteUvlc((uint32_t) sn.layerId.size() - 1, "sn_num_layers_minus1");
      for (uint32_t i = 1; i < sn.layerId.size(); i++)
      {
        xWriteCode(sn.layerId[i], 6, "sn_layer_id");
      }
    }
  }
  if (!sn.subpicId.empty())
  {
    xWriteUvlc((uint32_t) sn.subpicId.size() - 1, "sn_num_subpics_minus1");
    CHECK(sn.subpicIdLen <= 1, "subpicIdLen must be at least 1");
    xWriteUvlc(sn.subpicIdLen - 1, "sn_subpic_id_len_minus1");
    for (uint32_t i = 0; i < sn.subpicId.size(); i++)
    {
      xWriteCode(sn.subpicId[i], sn.subpicIdLen, "sn_subpic_id[i]");
    }
  }

  xWriteUvlc((uint32_t) sn.nestedSeis.size() - 1, "sn_num_seis_minus1");

  // byte alignment
  while (m_pcBitIf->getNumberOfWrittenBits() % 8 != 0)
  {
    xWriteFlag(0, "sn_zero_bit");
  }

  SEIMessages bufferingPeriod = getSeisByType(sn.nestedSeis, SEI::PayloadType::BUFFERING_PERIOD);
  if (!bufferingPeriod.empty())
  {
    SEIBufferingPeriod *bp = (SEIBufferingPeriod*)bufferingPeriod.front();
    m_nestingHrd.setBufferingPeriodSEI(bp);
  }

  // write nested SEI messages
  writeSEImessages(bs, sn.nestedSeis, m_nestingHrd, true, 0);
}

void SEIWriter::xWriteSEIFramePacking(const SEIFramePacking &sei, int SEIPrefixIndicationIdx)
{
  if (SEIPrefixIndicationIdx)
  {
    int numBits = 0;
    numBits += 2 * floorLog2(sei.m_arrangementId + 1) + 1;
    if (!sei.m_arrangementCancelFlag) 
    {
      numBits += 9;
    }
    else
    {
      numBits += 2;
    }
    xWriteCode(numBits - 1, 16, "num_bits_in_prefix_indication_minus1");
  }

  xWriteUvlc( sei.m_arrangementId,                  "fp_arrangement_id" );
  xWriteFlag( sei.m_arrangementCancelFlag,          "fp_arrangement_cancel_flag" );

  if( sei.m_arrangementCancelFlag == 0 )
  {
    xWriteCode( sei.m_arrangementType, 7,           "fp_arrangement_type" );
    if (SEIPrefixIndicationIdx)
    {
      return;
    }
    xWriteFlag( sei.m_quincunxSamplingFlag,         "fp_quincunx_sampling_flag" );
    xWriteCode( sei.m_contentInterpretationType, 6, "fp_content_interpretation_type" );
    xWriteFlag( sei.m_spatialFlippingFlag,          "fp_spatial_flipping_flag" );
    xWriteFlag( sei.m_frame0FlippedFlag,            "fp_frame0_flipped_flag" );
    xWriteFlag( sei.m_fieldViewsFlag,               "fp_field_views_flag" );
    xWriteFlag( sei.m_currentFrameIsFrame0Flag,     "fp_current_frame_is_frame0_flag" );

    xWriteFlag( sei.m_frame0SelfContainedFlag,      "fp_frame0_self_contained_flag" );
    xWriteFlag( sei.m_frame1SelfContainedFlag,      "fp_frame1_self_contained_flag" );

    if(sei.m_quincunxSamplingFlag == 0 && sei.m_arrangementType != 5)
    {
      xWriteCode( sei.m_frame0GridPositionX, 4,     "fp_frame0_grid_position_x" );
      xWriteCode( sei.m_frame0GridPositionY, 4,     "fp_frame0_grid_position_y" );
      xWriteCode( sei.m_frame1GridPositionX, 4,     "fp_frame1_grid_position_x" );
      xWriteCode( sei.m_frame1GridPositionY, 4,     "fp_frame1_grid_position_y" );
    }

    xWriteCode( sei.m_arrangementReservedByte, 8,   "fp_arrangement_reserved_byte" );
    xWriteFlag( sei.m_arrangementPersistenceFlag,   "fp_arrangement_persistence_flag" );
  }

  xWriteFlag( sei.m_upsampledAspectRatio,           "fp_upsampled_aspect_ratio_flag" );
}


void SEIWriter::xWriteSEIDisplayOrientation(const SEIDisplayOrientation& sei)
{
  xWriteFlag(sei.m_doCancelFlag, "display_orientation_cancel_flag");

  if (sei.m_doCancelFlag == 0)
  {
    xWriteFlag(sei.m_doPersistenceFlag, "display_orientation_persistence_flag");
    xWriteCode(sei.m_doTransformType, 3, "display_orientation_transform_type");
    xWriteCode(0, 3, "display_orientation_reserved_zero_3bits");
  }
}

void SEIWriter::xWriteSEIParameterSetsInclusionIndication(const SEIParameterSetsInclusionIndication& sei)
{
  xWriteFlag(sei.m_selfContainedClvsFlag, "psii_self_contained_clvs_flag");
}

void SEIWriter::xWriteSEIMasteringDisplayColourVolume(const SEIMasteringDisplayColourVolume& sei)
{
  xWriteCode( sei.values.primaries[0][0],  16,  "mdcv_display_primaries_x[0]" );
  xWriteCode( sei.values.primaries[0][1],  16,  "mdcv_display_primaries_y[0]" );

  xWriteCode( sei.values.primaries[1][0],  16,  "mdcv_display_primaries_x[1]" );
  xWriteCode( sei.values.primaries[1][1],  16,  "mdcv_display_primaries_y[1]" );

  xWriteCode( sei.values.primaries[2][0],  16,  "mdcv_display_primaries_x[2]" );
  xWriteCode( sei.values.primaries[2][1],  16,  "mdcv_display_primaries_y[2]" );

  xWriteCode( sei.values.whitePoint[0],    16,  "mdcv_white_point_x" );
  xWriteCode( sei.values.whitePoint[1],    16,  "mdcv_white_point_y" );

  xWriteCode( sei.values.maxLuminance,     32,  "mdcv_max_display_mastering_luminance" );
  xWriteCode( sei.values.minLuminance,     32,  "mdcv_min_display_mastering_luminance" );
}

void SEIWriter::xWriteSEISEIManifest(const SEIManifest &sei)
{
  xWriteCode(sei.m_manifestNumSeiMsgTypes, 16, "manifest_num_sei_msg_types");
  for (int i = 0; i < sei.m_manifestNumSeiMsgTypes; i++)
  {
    xWriteCode(to_underlying(sei.m_manifestSeiPayloadType[i]), 16, "manifest_sei_payload_types");
    xWriteCode(sei.m_manifestSeiDescription[i], 8, "manifest_sei_description");
  }
}

//SEI prefix indication
void SEIWriter::xWriteSEISEIPrefixIndication(OutputBitstream &bs, const SEIPrefixIndication &sei, HRD &hrd, const uint32_t temporalId)
{
  xWriteCode(to_underlying(sei.m_prefixSeiPayloadType), 16, "prefix_sei_payload_type");
  int idx = sei.m_numSeiPrefixIndicationsMinus1 + 1;
  //If num_sei_prefix_indication cannot be determined during initialization, then determine when writing prefix databits
  if (idx <= 1) 
  {
    xWriteCode(sei.m_numSeiPrefixIndicationsMinus1, 8, "num_sei_prefix_indications_minus1"); 
  }
  // By writing SEI prefix indication recursively, you only need to pass in SEIPrefixIndicationIdx in the corresponding
  // function and add the SEI prefix syntax elements. At present, only part of SEI can be written in SEI prefix
  // indication. If it needs to be added later, the corresponding databit should be determined
  xWriteSEIpayloadData(bs, *static_cast<const SEI *>(sei.m_payload), hrd, temporalId, idx);
  xWriteSEIPrefixIndicationByteAlign();
}

void SEIWriter::xWriteSEIPrefixIndicationByteAlign() {
  while (m_pcBitIf->getNumberOfWrittenBits() % 8 != 0)
  {
    xWriteFlag(1, "byte_alignment_bit_equal_to_one");
  } 
}
// ~SEI prefix indication

void SEIWriter::xWriteSEIAnnotatedRegions(const SEIAnnotatedRegions &sei)
{
  xWriteFlag(sei.m_hdr.m_cancelFlag, "ar_cancel_flag");
  if (!sei.m_hdr.m_cancelFlag)
  {
    xWriteFlag(sei.m_hdr.m_notOptimizedForViewingFlag, "ar_not_optimized_for_viewing_flag");
    xWriteFlag(sei.m_hdr.m_trueMotionFlag, "ar_true_motion_flag");
    xWriteFlag(sei.m_hdr.m_occludedObjectFlag, "ar_occluded_object_flag");
    xWriteFlag(sei.m_hdr.m_partialObjectFlagPresentFlag, "ar_partial_object_flag_present_flag");
    xWriteFlag(sei.m_hdr.m_objectLabelPresentFlag, "ar_object_label_present_flag");
    xWriteFlag(sei.m_hdr.m_objectConfidenceInfoPresentFlag, "ar_object_confidence_info_present_flag");
    if (sei.m_hdr.m_objectConfidenceInfoPresentFlag)
    {
      assert(sei.m_hdr.m_objectConfidenceLength <= 16 && sei.m_hdr.m_objectConfidenceLength>0);
      xWriteCode((sei.m_hdr.m_objectConfidenceLength - 1), 4, "ar_object_confidence_length_minus_1");
    }
    if (sei.m_hdr.m_objectLabelPresentFlag)
    {
      xWriteFlag(sei.m_hdr.m_objectLabelLanguagePresentFlag, "ar_object_label_language_present_flag");
      if (sei.m_hdr.m_objectLabelLanguagePresentFlag)
      {
        xWriteByteAlign();
        assert(sei.m_hdr.m_annotatedRegionsObjectLabelLang.size()<256);
        for (uint32_t j = 0; j < sei.m_hdr.m_annotatedRegionsObjectLabelLang.size(); j++)
        {
          char ch = sei.m_hdr.m_annotatedRegionsObjectLabelLang[j];
          xWriteCode(ch, 8, "ar_object_label_language");
        }
        xWriteCode('\0', 8, "ar_label_language");
      }
      xWriteUvlc((uint32_t)sei.m_annotatedLabels.size(), "ar_num_label_updates");
      assert(sei.m_annotatedLabels.size()<256);
      for(auto it=sei.m_annotatedLabels.begin(); it!=sei.m_annotatedLabels.end(); it++)
      {
        assert(it->first < 256);
        xWriteUvlc(it->first, "ar_label_idx[]");
        const SEIAnnotatedRegions::AnnotatedRegionLabel &ar=it->second;
        xWriteFlag(!ar.labelValid, "ar_label_cancel_flag");
        if (ar.labelValid)
        {
          xWriteByteAlign();
          assert(ar.label.size()<256);
          for (uint32_t j = 0; j < ar.label.size(); j++)
          {
            char ch = ar.label[j];
            xWriteCode(ch, 8, "ar_label[]");
          }
          xWriteCode('\0', 8, "ar_label[]");
        }
      }
    }

    xWriteUvlc((uint32_t)sei.m_annotatedRegions.size(), "ar_num_object_updates");
    assert(sei.m_annotatedRegions.size()<256);
    for (auto it=sei.m_annotatedRegions.begin(); it!=sei.m_annotatedRegions.end(); it++)
    {
      const SEIAnnotatedRegions::AnnotatedRegionObject &ar = it->second;
      xWriteUvlc(it->first, "ar_object_idx");
      xWriteFlag(ar.objectCancelFlag, "ar_object_cancel_flag");
      if (!ar.objectCancelFlag)
      {
        if (sei.m_hdr.m_objectLabelPresentFlag)
        {
          xWriteFlag(ar.objectLabelValid, "ar_object_label_update_flag");
          if (ar.objectLabelValid)
          {
            assert(ar.objLabelIdx<256);
            xWriteUvlc(ar.objLabelIdx, "ar_object_label_idx");
          }
        }
        xWriteFlag(ar.boundingBoxValid, "ar_object_bounding_box_update_flag");
        if (ar.boundingBoxValid)
        {
          xWriteFlag(ar.boundingBoxCancelFlag, "ar_bounding_box_cancel_flag");
          if (!ar.boundingBoxCancelFlag)
          {
            xWriteCode(ar.boundingBoxTop,   16, "ar_bounding_box_top");
            xWriteCode(ar.boundingBoxLeft,  16, "ar_bounding_box_left");
            xWriteCode(ar.boundingBoxWidth, 16, "ar_bounding_box_width");
            xWriteCode(ar.boundingBoxHeight,16, "ar_bounding_box_height");
            if (sei.m_hdr.m_partialObjectFlagPresentFlag)
            {
              xWriteUvlc(ar.partialObjectFlag, "ar_partial_object_flag");
            }
            if (sei.m_hdr.m_objectConfidenceInfoPresentFlag)
            {
              assert(ar.objectConfidence < (1<<sei.m_hdr.m_objectConfidenceLength));
              xWriteCode(ar.objectConfidence, sei.m_hdr.m_objectConfidenceLength, "ar_object_confidence");
            }
          }
        }
      }
    }
  }
}

void SEIWriter::xWriteSEIObjectMaskInfos(const SEIObjectMaskInfos& sei)
{
  xWriteFlag(sei.m_hdr.m_cancelFlag, "omi_cancel_flag");
  if (!sei.m_hdr.m_cancelFlag)
  {
    xWriteFlag(sei.m_hdr.m_persistenceFlag, "omi_persistence_flag");
    xWriteUvlc((uint32_t) sei.m_hdr.m_numAuxPicLayerMinus1, "omi_num_aux_pic_layer_minus1");
    xWriteUvlc((uint32_t) sei.m_hdr.m_maskIdLengthMinus1, "omi_mask_id_length_minus1");
    xWriteUvlc((uint32_t) sei.m_hdr.m_maskSampleValueLengthMinus8, "omi_mask_sample_value_length_minus8");
    xWriteFlag(sei.m_hdr.m_maskConfidenceInfoPresentFlag, "omi_mask_confidence_info_present_flag");
    if (sei.m_hdr.m_maskConfidenceInfoPresentFlag)
    {
      CHECK((sei.m_hdr.m_maskConfidenceLengthMinus1 > 15 || sei.m_hdr.m_maskConfidenceLengthMinus1 < 0), "The range of omi_mask_confidence_length_minus1 must be [0, 15]");
      xWriteCode((sei.m_hdr.m_maskConfidenceLengthMinus1), 4, "omi_mask_confidence_length_minus1");
    }
    xWriteFlag(sei.m_hdr.m_maskDepthInfoPresentFlag, "omi_mask_depth_info_present_flag");
    if (sei.m_hdr.m_maskDepthInfoPresentFlag)
    {
      CHECK((sei.m_hdr.m_maskDepthLengthMinus1 > 15 || sei.m_hdr.m_maskDepthLengthMinus1 < 0), "The range of omi_mask_depth_length_minus1 must be [0, 15]");
      xWriteCode((sei.m_hdr.m_maskDepthLengthMinus1), 4, "omi_mask_depth_length_minus1");
    }
    xWriteFlag(sei.m_hdr.m_maskLabelInfoPresentFlag, "omi_mask_label_info_present_flag");
    if (sei.m_hdr.m_maskLabelInfoPresentFlag)
    {
      xWriteFlag(sei.m_hdr.m_maskLabelLanguagePresentFlag, "omi_mask_label_language_present_flag");
      if (sei.m_hdr.m_maskLabelLanguagePresentFlag)
      {
        while (!isByteAligned())
        {
          xWriteFlag(0, "omi_bit_equal_to_zero");
        }
        CHECK(sei.m_hdr.m_maskLabelLanguage.size() > 255, "label oversize");
        for (uint32_t m = 0; m < sei.m_hdr.m_maskLabelLanguage.size(); m++)
        {
          char ch = sei.m_hdr.m_maskLabelLanguage[m];
          xWriteCode(ch, 8, "omi_mask_lable_language");
        }
        xWriteCode('\0', 8, "omi_mask_lable_language");
      }
    }

    uint32_t maskCnt = 0;
    for (uint32_t i = 0; i <= sei.m_hdr.m_numAuxPicLayerMinus1; i++)
    {
      xWriteFlag(sei.m_maskPicUpdateFlag[i], "omi_mask_pic_update_flag[i]");
      if (sei.m_maskPicUpdateFlag[i])
      {
        xWriteUvlc((uint32_t) sei.m_numMaskInPic[i], "omi_num_mask_in_pic[i]");
        for (uint32_t j = 0; j < sei.m_numMaskInPic[i]; j++)
        {
          xWriteCode(sei.m_objectMaskInfos[maskCnt].maskId, sei.m_hdr.m_maskIdLengthMinus1 + 1, "omi_mask_id[i][j]");
              xWriteFlag(sei.m_objectMaskInfos[maskCnt].maskNew, "omi_mask_id_new_object_flag[i][j]");
          xWriteCode(sei.m_objectMaskInfos[maskCnt].auxSampleValue, sei.m_hdr.m_maskSampleValueLengthMinus8 + 8, "omi_aux_sample_value[i][j]");
          xWriteFlag(sei.m_objectMaskInfos[maskCnt].maskBoundingBoxPresentFlag, "omi_mask_bounding_box_present_flag[i][j]");
          if (sei.m_objectMaskInfos[maskCnt].maskBoundingBoxPresentFlag)
          {
            xWriteCode((uint32_t) sei.m_objectMaskInfos[maskCnt].maskTop, 16, "omi_mask_top[i][j]");
            xWriteCode((uint32_t) sei.m_objectMaskInfos[maskCnt].maskLeft, 16, "omi_mask_left[i][j]");
#if JVET_AL0067_OMI_SEI_CONSTRAINTS
            xWriteCode((uint32_t) (sei.m_objectMaskInfos[maskCnt].maskWidth - 1), 16, "omi_mask_width_minus1[i][j]");
            xWriteCode((uint32_t) (sei.m_objectMaskInfos[maskCnt].maskHeight - 1), 16, "omi_mask_height_minus1[i][j]");
#else
            xWriteCode((uint32_t) sei.m_objectMaskInfos[maskCnt].maskWidth, 16, "omi_mask_width[i][j]");
            xWriteCode((uint32_t) sei.m_objectMaskInfos[maskCnt].maskHeight, 16, "omi_mask_height[i][j]");
#endif
          }
          if (sei.m_hdr.m_maskConfidenceInfoPresentFlag)
          {
            xWriteCode(sei.m_objectMaskInfos[maskCnt].maskConfidence, sei.m_hdr.m_maskConfidenceLengthMinus1 + 1, "omi_mask_confidence[i][j]");
          }
          if (sei.m_hdr.m_maskDepthInfoPresentFlag)
          {
            xWriteCode(sei.m_objectMaskInfos[maskCnt].maskDepth, sei.m_hdr.m_maskDepthLengthMinus1 + 1, "omi_mask_depth[i][j]");
          }
          while (!isByteAligned())
          {
            xWriteFlag(0, "omi_bit_equal_to_zero");
          }
          if (sei.m_hdr.m_maskLabelInfoPresentFlag)
          {
            CHECK(sei.m_objectMaskInfos[maskCnt].maskLabel.size() > 255, "label oversize");
            for (uint32_t m = 0; m < sei.m_objectMaskInfos[maskCnt].maskLabel.size(); m++)
            {
              char ch = sei.m_objectMaskInfos[maskCnt].maskLabel[m];
              xWriteCode(ch, 8, "omi_mask_label");
            }
            xWriteCode('\0', 8, "omi_mask_label");
          }
          maskCnt++;
        }
      }
    }
  }
}

void SEIWriter::xWriteByteAlign()
{
  if( m_pcBitIf->getNumberOfWrittenBits() % 8 != 0)
  {
    xWriteFlag( 1, "payload_bit_equal_to_one" );
    while( m_pcBitIf->getNumberOfWrittenBits() % 8 != 0 )
    {
      xWriteFlag( 0, "payload_bit_equal_to_zero" );
    }
  }
}

void SEIWriter::xWriteSEIAlternativeTransferCharacteristics(const SEIAlternativeTransferCharacteristics& sei)
{
  xWriteCode(sei.m_preferredTransferCharacteristics, 8, "preferred_transfer_characteristics");
}

void SEIWriter::xWriteSEIEquirectangularProjection(const SEIEquirectangularProjection &sei, int SEIPrefixIndicationIdx)
{
  if (SEIPrefixIndicationIdx)
  {
    int numBits = 5;
    if (sei.m_erpGuardBandFlag) 
    {
      numBits += 19;
    }   
    xWriteCode(numBits - 1, 16, "num_bits_in_prefix_indication_minus1");
  }

  xWriteFlag( sei.m_erpCancelFlag, "erp_cancel_flag" );
  if( !sei.m_erpCancelFlag )
  {
    xWriteFlag( sei.m_erpPersistenceFlag, "erp_persistence_flag" );
    xWriteFlag( sei.m_erpGuardBandFlag,   "erp_guard_band_flag" );
    xWriteCode( 0, 2, "erp_reserved_zero_2bits" );
    if ( sei.m_erpGuardBandFlag == 1)
    {
      xWriteCode( sei.m_erpGuardBandType,       3, "erp_guard_band_type" );
      xWriteCode( sei.m_erpLeftGuardBandWidth,  8, "erp_left_guard_band_width" );
      xWriteCode( sei.m_erpRightGuardBandWidth, 8, "erp_right_guard_band_width" );
    }
  }
}

void SEIWriter::xWriteSEISphereRotation(const SEISphereRotation &sei, int SEIPrefixIndicationIdx)
{
  if (SEIPrefixIndicationIdx)
  {
    if (sei.m_sphereRotationCancelFlag)
    {
      xWriteCode(0,                                 8, "num_sei_prefix_indications_minus1");
    }
    else
    {
      xWriteCode(1,                                 8, "num_sei_prefix_indications_minus1");
    }
    int numBits = 8;
    xWriteCode(numBits - 1,                        16, "num_bits_in_prefix_indication_minus1");
  }
  xWriteFlag( sei.m_sphereRotationCancelFlag,           "sphere_rotation_cancel_flag" );
  if( !sei.m_sphereRotationCancelFlag )
  {
    xWriteFlag( sei.m_sphereRotationPersistenceFlag,    "sphere_rotation_persistence_flag" );
    xWriteCode( 0,                                   6, "sphere_rotation_reserved_zero_6bits" );
    if (SEIPrefixIndicationIdx >= 2)
    {
      xWriteSEIPrefixIndicationByteAlign();
      int numBits2 = 8 + 32 + 32 + 32;
      xWriteCode(numBits2 - 1,                      16, "num_bits_in_prefix_indication_minus1");
      xWriteFlag(sei.m_sphereRotationCancelFlag, "sphere_rotation_cancel_flag");
      xWriteFlag(sei.m_sphereRotationPersistenceFlag, "sphere_rotation_persistence_flag");
      xWriteCode(0, 6, "sphere_rotation_reserved_zero_6bits");
    }
    xWriteSCode(sei.m_sphereRotationYaw,            32, "sphere_rotation_yaw" );
    xWriteSCode(sei.m_sphereRotationPitch,          32, "sphere_rotation_pitch" );
    xWriteSCode(sei.m_sphereRotationRoll,           32, "sphere_rotation_roll" );
  }
}

void SEIWriter::xWriteSEIOmniViewport(const SEIOmniViewport &sei)
{
  xWriteCode( sei.m_omniViewportId,     10,        "omni_viewport_id" );
  xWriteFlag( sei.m_omniViewportCancelFlag,        "omni_viewport_cancel_flag" );
  if ( !sei.m_omniViewportCancelFlag )
  {
    xWriteFlag( sei.m_omniViewportPersistenceFlag, "omni_viewport_persistence_flag" );
    const uint32_t numRegions = (uint32_t) sei.m_omniViewportRegions.size();
    xWriteCode( numRegions - 1, 4,                 "omni_viewport_cnt_minus1" );
    for(uint32_t region=0; region<numRegions; region++)
    {
      const SEIOmniViewport::OmniViewport &viewport=sei.m_omniViewportRegions[region];
      xWriteSCode( viewport.azimuthCentre,     32, "omni_viewport_azimuth_centre"   );
      xWriteSCode( viewport.elevationCentre,   32, "omni_viewport_elevation_centre" );
      xWriteSCode( viewport.tiltCentre,        32, "omni_viewport_tilt_center" );
      xWriteCode( viewport.horRange,           32, "omni_viewport_hor_range[i]" );
      xWriteCode( viewport.verRange,           32, "omni_viewport_ver_range[i]" );
    }
  }
}

void SEIWriter::xWriteSEIRegionWisePacking(const SEIRegionWisePacking &sei, int SEIPrefixIndicationIdx)
{
  if (SEIPrefixIndicationIdx)
  {
    xWriteCode(0, 8, "num_sei_prefix_indications_minus1");
    int numBits = 1;
    if (!sei.m_rwpCancelFlag)
    {
      numBits += 111;
    }
    xWriteCode(numBits - 1,                                       16,       "num_bits_in_prefix_indication_minus1");
  }
  xWriteFlag( sei.m_rwpCancelFlag,                                           "rwp_cancel_flag" );
  if(!sei.m_rwpCancelFlag)
  {
    xWriteFlag( sei.m_rwpPersistenceFlag,                                    "rwp_persistence_flag" );
    xWriteFlag( sei.m_constituentPictureMatchingFlag,                        "rwp_constituent_picture_matching_flag" );
    xWriteCode( 0, 5,                                                        "rwp_reserved_zero_5bits" );
    xWriteCode( (uint32_t)sei.m_numPackedRegions,                 8,         "rwp_num_packed_regions" );
    xWriteCode( (uint32_t)sei.m_projPictureWidth,                 32,        "rwp_proj_picture_width" );
    xWriteCode( (uint32_t)sei.m_projPictureHeight,                32,        "rwp_proj_picture_height" );
    xWriteCode( (uint32_t)sei.m_packedPictureWidth,               16,        "rwp_packed_picture_width" );
    xWriteCode( (uint32_t)sei.m_packedPictureHeight,              16,        "rwp_packed_picture_height" );
    if (SEIPrefixIndicationIdx)
    {
      // don't write full message
      return;
    }
    for( int i=0; i < sei.m_numPackedRegions; i++ )
    {
      xWriteCode( 0, 4,                                                      "rwp_reserved_zero_4bits" );
      xWriteCode( (uint32_t)sei.m_rwpTransformType[i],            3,         "rwp_transform_type" );
      xWriteFlag( sei.m_rwpGuardBandFlag[i],                                 "rwp_guard_band_flag" );
      xWriteCode( (uint32_t)sei.m_projRegionWidth[i],             32,        "rwp_proj_region_width" );
      xWriteCode( (uint32_t)sei.m_projRegionHeight[i],            32,        "rwp_proj_region_height" );
      xWriteCode( (uint32_t)sei.m_rwpProjRegionTop[i],            32,        "rwp_proj_region_top" );
      xWriteCode( (uint32_t)sei.m_projRegionLeft[i],              32,        "rwp_proj_region_left" );
      xWriteCode( (uint32_t)sei.m_packedRegionWidth[i],           16,        "rwp_packed_region_width" );
      xWriteCode( (uint32_t)sei.m_packedRegionHeight[i],          16,        "rwp_packed_region_height" );
      xWriteCode( (uint32_t)sei.m_packedRegionTop[i],             16,        "rwp_packed_region_top" );
      xWriteCode( (uint32_t)sei.m_packedRegionLeft[i],            16,        "rwp_packed_region_left" );
      if( sei.m_rwpGuardBandFlag[i] )
      {
        xWriteCode( (uint32_t)sei.m_rwpLeftGuardBandWidth[i],     8,         "rwp_left_guard_band_width");
        xWriteCode( (uint32_t)sei.m_rwpRightGuardBandWidth[i],    8,         "rwp_right_guard_band_width");
        xWriteCode( (uint32_t)sei.m_rwpTopGuardBandHeight[i],     8,         "rwp_top_guard_band_height");
        xWriteCode( (uint32_t)sei. m_rwpBottomGuardBandHeight[i], 8,         "rwp_bottom_guard_band_height");
        xWriteFlag( sei.m_rwpGuardBandNotUsedForPredFlag[i],                 "rwp_guard_band_not_used_for_pred_flag" );
        for( int j=0; j < 4; j++ )
        {
          xWriteCode( (uint32_t)sei.m_rwpGuardBandType[i*4 + j],  3,         "rwp_guard_band_type");
        }
        xWriteCode( 0, 3,                                                    "rwp_guard_band_reserved_zero_3bits" );
      }
    }
  }
}

void SEIWriter::xWriteSEIGeneralizedCubemapProjection(const SEIGeneralizedCubemapProjection &sei,
                                                      int                                    SEIPrefixIndicationIdx)
{
  if (SEIPrefixIndicationIdx)
  {
    int numBits = 1;
    if (!sei.m_gcmpCancelFlag) 
    {
      numBits += 6;
    }
    xWriteCode(numBits - 1, 16, "num_bits_in_prefix_indication_minus1");
  }

  xWriteFlag( sei.m_gcmpCancelFlag,                           "gcmp_cancel_flag" );
  if (!sei.m_gcmpCancelFlag)
  {
    xWriteFlag( sei.m_gcmpPersistenceFlag,                    "gcmp_persistence_flag" );
    xWriteCode( sei.m_gcmpPackingType,                     3, "gcmp_packing_type" );
    xWriteCode( sei.m_gcmpMappingFunctionType,             2, "gcmp_mapping_function_type" );
    if (SEIPrefixIndicationIdx == 1)
    {
      return;
    }
    int numFace = sei.m_gcmpPackingType == 4 || sei.m_gcmpPackingType == 5 ? 5 : 6;
    for (int i = 0; i < numFace; i++)
    {
      xWriteCode( sei.m_gcmpFaceIndex[i],                  3, "gcmp_face_index" );
      xWriteCode( sei.m_gcmpFaceRotation[i],               2, "gcmp_face_rotation" );
      if (sei.m_gcmpMappingFunctionType == 2)
      {
        xWriteCode( sei.m_gcmpFunctionCoeffU[i],           7, "gcmp_function_coeff_u" );
        xWriteFlag( sei.m_gcmpFunctionUAffectedByVFlag[i],    "gcmp_function_u_affected_by_v_flag" );
        xWriteCode( sei.m_gcmpFunctionCoeffV[i],           7, "gcmp_function_coeff_v" );
        xWriteFlag( sei.m_gcmpFunctionVAffectedByUFlag[i],    "gcmp_function_v_affected_by_u_flag" );
      }
    }
    xWriteFlag( sei.m_gcmpGuardBandFlag,                      "gcmp_guard_band_flag" );
    if (sei.m_gcmpGuardBandFlag)
    {
      xWriteCode( sei.m_gcmpGuardBandType,                 3, "gcmp_guard_band_type" );
      xWriteFlag( sei.m_gcmpGuardBandBoundaryExteriorFlag,    "gcmp_guard_band_boundary_exterior_flag" );
      xWriteCode( sei.m_gcmpGuardBandSamplesMinus1,        4, "gcmp_guard_band_samples_minus1" );
    }
  }
}

void SEIWriter::xWriteSEIScalabilityDimensionInfo(const SEIScalabilityDimensionInfo &sei)
{
  xWriteCode(sei.m_sdiMaxLayersMinus1, 6,                           "sdi_max_layers_minus1");
  xWriteFlag(sei.m_sdiMultiviewInfoFlag,                            "sdi_multiview_info_flag");
  xWriteFlag(sei.m_sdiAuxiliaryInfoFlag,                            "sdi_auxiliary_info_flag");
  if (sei.m_sdiMultiviewInfoFlag || sei.m_sdiAuxiliaryInfoFlag)
  {
    if (sei.m_sdiMultiviewInfoFlag)
    {
      xWriteCode(sei.m_sdiViewIdLenMinus1, 4,                              "sdi_view_id_len_minus1");
    }
    for (int i = 0; i <= sei.m_sdiMaxLayersMinus1; i++)
    {
      xWriteCode(sei.m_sdiLayerId[i], 6,                                         "sdi_layer_id");
      if (sei.m_sdiMultiviewInfoFlag)
      {
        xWriteCode(sei.m_sdiViewIdVal[i], sei.m_sdiViewIdLenMinus1 + 1,       "sdi_view_id_val");
      }
      if (sei.m_sdiAuxiliaryInfoFlag)
      {
        xWriteCode(sei.m_sdiAuxId[i], 8,                           "sdi_aux_id");
        if (sei.m_sdiAuxId[i] > 0)
        {
          xWriteCode(sei.m_sdiNumAssociatedPrimaryLayersMinus1[i], 6,          "sdi_num_associated_primary_layers_minus1");
          for (int j = 0; j <= sei.m_sdiNumAssociatedPrimaryLayersMinus1[i]; j++)
          {
            xWriteCode(sei.m_sdiAssociatedPrimaryLayerIdx[i][j], 6,               "sdi_associated_primary_layer_idx");
          }
        }
      }
    }
  }
}

void SEIWriter::xWriteSEIMultiviewAcquisitionInfo(const SEIMultiviewAcquisitionInfo& sei)
{
  xWriteFlag( ( sei.m_maiIntrinsicParamFlag ? 1 : 0 ), "intrinsic_param_flag" );
  xWriteFlag( ( sei.m_maiExtrinsicParamFlag ? 1 : 0 ), "extrinsic_param_flag" );
  xWriteUvlc(   sei.m_maiNumViewsMinus1               , "num_views_minus1"           );
  if( sei.m_maiIntrinsicParamFlag )
  {
    xWriteFlag( ( sei.m_maiIntrinsicParamsEqualFlag ? 1 : 0 ), "intrinsic_params_equal_flag" );
    xWriteUvlc(   sei.m_maiPrecFocalLength                   , "prec_focal_length"           );
    xWriteUvlc(   sei.m_maiPrecPrincipalPoint                , "prec_principal_point"        );
    xWriteUvlc(   sei.m_maiPrecSkewFactor                    , "prec_skew_factor"            );

    for( int i = 0; i  <=  ( sei.m_maiIntrinsicParamsEqualFlag ? 0 : sei.m_maiNumViewsMinus1 ); i++ )
    {
      xWriteFlag( ( sei.m_maiSignFocalLengthX       [i] ? 1 : 0 ),                                         "sign_focal_length_x"        );
      xWriteCode(   sei.m_maiExponentFocalLengthX   [i]          , 6                                  ,    "exponent_focal_length_x"    );
      xWriteCode(   sei.m_maiMantissaFocalLengthX   [i]          , sei.getMantissaFocalLengthXLen( i ),    "mantissa_focal_length_x"    );
      xWriteFlag( ( sei.m_maiSignFocalLengthY       [i] ? 1 : 0 ),                                         "sign_focal_length_y"        );
      xWriteCode(   sei.m_maiExponentFocalLengthY   [i]          , 6                                  ,    "exponent_focal_length_y"    );
      xWriteCode(   sei.m_maiMantissaFocalLengthY   [i]          , sei.getMantissaFocalLengthYLen( i ),    "mantissa_focal_length_y"    );
      xWriteFlag( ( sei.m_maiSignPrincipalPointX    [i] ? 1 : 0 ),                                         "sign_principal_point_x"     );
      xWriteCode(   sei.m_maiExponentPrincipalPointX[i]          , 6,                                      "exponent_principal_point_x" );
      xWriteCode(   sei.m_maiMantissaPrincipalPointX[i]          , sei.getMantissaPrincipalPointXLen( i ), "mantissa_principal_point_x" );
      xWriteFlag( ( sei.m_maiSignPrincipalPointY    [i] ? 1 : 0 ),                                         "sign_principal_point_y"     );
      xWriteCode(   sei.m_maiExponentPrincipalPointY[i]          , 6,                                      "exponent_principal_point_y" );
      xWriteCode(   sei.m_maiMantissaPrincipalPointY[i]          , sei.getMantissaPrincipalPointYLen( i ), "mantissa_principal_point_y" );
      xWriteFlag( ( sei.m_maiSignSkewFactor         [i] ? 1 : 0 ),                                         "sign_skew_factor"           );
      xWriteCode(   sei.m_maiExponentSkewFactor     [i]          , 6,                                      "exponent_skew_factor"       );
      xWriteCode(   sei.m_maiMantissaSkewFactor     [i]          , sei.getMantissaSkewFactorLen( i )  ,    "mantissa_skew_factor"       );
    }
  }
  if( sei.m_maiExtrinsicParamFlag )
  {
    xWriteUvlc( sei.m_maiPrecRotationParam   , "prec_rotation_param"    );
    xWriteUvlc( sei.m_maiPrecTranslationParam, "prec_translation_param" );
    for( int i = 0; i  <=  sei.m_maiNumViewsMinus1; i++ )
    {
      for( int j = 0; j  <=  2; j++ )  /* row */
      {
        for( int k = 0; k  <=  2; k++ )  /* column */
        {
          xWriteFlag( ( sei.m_maiSignR    [i][j][k] ? 1 : 0 ),                                "sign_r"     );
          xWriteCode(   sei.m_maiExponentR[i][j][k]          , 6,                             "exponent_r" );
          xWriteCode(   sei.m_maiMantissaR[i][j][k]          , sei.getMantissaRLen( i,j,k ) , "mantissa_r" );
        }
        xWriteFlag( ( sei.m_maiSignT    [i][j] ? 1 : 0 ),                          "sign_t"     );
        xWriteCode(   sei.m_maiExponentT[i][j]          , 6,                       "exponent_t" );
        xWriteCode(   sei.m_maiMantissaT[i][j]          , sei.getMantissaTLen( i,j ),"mantissa_t" );
      }
    }
  }
};

void SEIWriter::xWriteSEIMultiviewViewPosition(const SEIMultiviewViewPosition& sei)
{
  xWriteUvlc(sei.m_mvpNumViewsMinus1, "num_views_minus1");
  for (int i = 0; i <= sei.m_mvpNumViewsMinus1; i++)
  {
    xWriteUvlc(sei.m_mvpViewPosition[i], "view_position");
  }
};

void SEIWriter::xWriteSEIAlphaChannelInfo( const SEIAlphaChannelInfo& sei)
{
  xWriteFlag( ( sei.m_aciCancelFlag ? 1 : 0 ), "alpha_channel_cancel_flag" );
  if( !sei.m_aciCancelFlag )
  {
    xWriteCode( sei.m_aciUseIdc, 3, "alpha_channel_use_idc" );
    xWriteCode( sei.m_aciBitDepthMinus8, 3, "alpha_channel_bit_depth_minus8" );
    xWriteCode( sei.m_aciTransparentValue, sei.m_aciBitDepthMinus8+9, "alpha_transparent_value" );
    xWriteCode( sei.m_aciOpaqueValue, sei.m_aciBitDepthMinus8+9, "alpha_opaque_value" );
    xWriteFlag( ( sei.m_aciIncrFlag ? 1 : 0 ), "alpha_channel_incr_flag" );
    xWriteFlag( ( sei.m_aciClipFlag ? 1 : 0 ), "alpha_channel_clip_flag" );
    if( sei.m_aciClipFlag )
    {
      xWriteFlag( ( sei.m_aciClipTypeFlag ? 1 : 0 ), "alpha_channel_clip_type_flag" );
    }
  }
};

void SEIWriter::xWriteSEIDepthRepresentationInfo( const SEIDepthRepresentationInfo& sei)
{
  xWriteFlag( ( sei.m_driZNearFlag ? 1 : 0 ), "z_near_flag" );
  xWriteFlag( ( sei.m_driZFarFlag ? 1 : 0 ), "z_far_flag" );
  xWriteFlag( ( sei.m_driDMinFlag ? 1 : 0 ), "d_min_flag" );
  xWriteFlag( ( sei.m_driDMaxFlag ? 1 : 0 ), "d_max_flag" );
  xWriteUvlc( sei.m_driDepthRepresentationType, "depth_representation_type" );
  if( sei.m_driDMinFlag || sei.m_driDMaxFlag )
  {
    xWriteUvlc( sei.m_driDisparityRefViewId, "disparity_ref_view_id" );
  }
  if( sei.m_driZNearFlag )
  {
    xWriteSEIDepthRepInfoElement(sei.m_driZNear);
  }
  if( sei.m_driZFarFlag )
  {
    xWriteSEIDepthRepInfoElement(sei.m_driZFar);
  }
  if( sei.m_driDMinFlag )
  {
    xWriteSEIDepthRepInfoElement(sei.m_driDMin);
  }
  if( sei.m_driDMaxFlag )
  {
    xWriteSEIDepthRepInfoElement(sei.m_driDMax);
  }

  if (sei.m_driDepthRepresentationType == 3)
  {
    xWriteUvlc( sei.m_driDepthNonlinearRepresentationNumMinus1, "depth_nonlinear_representation_num_minus1" );
    for( int i = 1; i  <=  sei.m_driDepthNonlinearRepresentationNumMinus1 + 1; i++ )
    {
      xWriteUvlc(sei.m_driDepthNonlinearRepresentationModel.at(i - 1),"depth_nonlinear_representation_model[ i ]");
    }
  }
}

void SEIWriter::xWriteSEIDepthRepInfoElement( double f )
{
  uint32_t x_sign, x_exp, x_mantissa,x_mantissa_len;
  if (f < 0)
  {
    f = f * (-1);
    x_sign = 1;
  }
  else
  {
    x_sign = 0;
  }
  int exponent=0;
  if(f >= 1)
  {
    while(f>=2)
    {
      exponent++;
      f = f/2;
    }
  }
  else
  {
    while (f<1)
    {
      exponent++;
      f = f*2;
    }
    exponent=-exponent;
  }

  int i;
  f = f -1;
  double s = 1;
  char s_mantissa[32];
  double thr=1.0/(4.0*(1<<30));

  if (f>=thr)
  {
    for(i=0;i<32;i++)
    {
      s /= 2;
      if(f>=s)
      {
        f = f-s;
        s_mantissa[i]=1;

        if (f<thr)
          break;
      }
      else
      {
        s_mantissa[i]=0;
      }
    }

    if (i<32)
      x_mantissa_len=i+1;
    else
      x_mantissa_len=32;

    x_mantissa=0;

    for(i=0;i<x_mantissa_len;i++)
    {
      if (s_mantissa[i]==1)
        x_mantissa += (1u)<<(x_mantissa_len-1-i) ;
    }

  }
  else
  {
    x_mantissa=0;
    x_mantissa_len=1;
  }

  assert(exponent>=-31 && exponent<= (1<<7)-32);
  x_exp=exponent+31;

  xWriteFlag( x_sign,                          "da_sign_flag" );
  xWriteCode( x_exp, 7 ,                       "da_exponent" );
  xWriteCode( x_mantissa_len-1, 5 ,            "da_mantissa_len_minus1" );
  xWriteCode( x_mantissa, x_mantissa_len ,     "da_mantissa" );
};

void SEIWriter::xWriteSEISubpictureLevelInfo(const SEISubpictureLevelInfo& sli)
{
  CHECK(sli.numRefLevels() < 1, "SEISubpictureLevelInfo: numRefLevels must be greater than zero");
  xWriteCode(sli.numRefLevels() - 1, 3, "sli_num_ref_levels_minus1");
  xWriteFlag(sli.cbrConstraint ? 1 : 0, "sli_cbr_constraint_flag");
  xWriteFlag(sli.explicitFractionPresentFlag() ? 1 : 0, "sli_explicit_fraction_present_flag");
  if (sli.explicitFractionPresentFlag())
  {
    xWriteUvlc(sli.numSubpics() - 1, "sli_num_subpics_minus1");
    xWriteCode(sli.maxSublayers() - 1, 3, "sli_max_sublayers_minus1");
    xWriteFlag(sli.hasSublayerInfo ? 1 : 0, "sli_sublayer_info_present_flag");
    while (!isByteAligned())
    {
      xWriteFlag(0, "sli_alignment_zero_bit");
    }
  }

  for (int k = sli.hasSublayerInfo ? 0 : sli.maxSublayers() - 1; k < sli.maxSublayers(); k++)
  {
    for (int i = 0; i < sli.numRefLevels(); i++)
    {
      xWriteCode(sli.nonSubpicLayerFraction(i, k), 8, "sli_non_subpic_layers_fraction[i][k]");
      xWriteCode(sli.refLevelIdc(i, k), 8, "sli_ref_level_idc[i][k]");
      if (sli.explicitFractionPresentFlag())
      {
        for (int j = 0; j < sli.numSubpics(); j++)
        {
          xWriteCode(sli.refLevelFraction(i, j, k), 8, "sli_ref_level_fraction_minus1[i][j][k]");
        }
      }
    }
  }
}

void SEIWriter::xWriteSEISampleAspectRatioInfo(const SEISampleAspectRatioInfo &sei)
{
  xWriteFlag( sei.m_sariCancelFlag,                                           "sari_cancel_flag" );
  if(!sei.m_sariCancelFlag)
  {
    xWriteFlag( sei.m_sariPersistenceFlag,                                    "sari_persistence_flag" );
    xWriteCode( (uint32_t)sei.m_sariAspectRatioIdc, 8,                        "sari_aspect_ratio_idc");
    if (sei.m_sariAspectRatioIdc == 255)
    {
      xWriteCode( (uint32_t)sei.m_sariSarWidth, 16,                           "sari_sar_width");
      xWriteCode( (uint32_t)sei.m_sariSarHeight, 16,                          "sari_sar_height");
    }
  }
}

void SEIWriter::xWriteSEIPhaseIndication(const SEIPhaseIndication& sei)
{
  xWriteCode((uint32_t)sei.m_horPhaseNum, 8, "hor_phase_num");
  xWriteCode((uint32_t)sei.m_horPhaseDenMinus1, 8, "hor_phase_den_minus1");
  xWriteCode((uint32_t)sei.m_verPhaseNum, 8, "ver_phase_num");
  xWriteCode((uint32_t)sei.m_verPhaseDenMinus1, 8, "ver_phase_den_minus1");
}

void SEIWriter::xWriteSEIUserDataRegistered(const SEIUserDataRegistered &sei)
{
  xWriteCode((sei.m_ituCountryCode>255) ? 0xff : sei.m_ituCountryCode, 8, "itu_t_t35_country_code");
  if (sei.m_ituCountryCode >= 255)
  {
    assert(sei.m_ituCountryCode < 255 + 256);
    xWriteCode(sei.m_ituCountryCode - 255, 8, "itu_t_t35_country_code_extension_byte");
  }
  for (uint32_t i = 0; i<sei.m_userData.size(); i++)
  {
    xWriteCode(sei.m_userData[i], 8, "itu_t_t35_payload_byte");
  }
}

void SEIWriter::xWriteSEIFilmGrainCharacteristics(const SEIFilmGrainCharacteristics &sei)
{
  xWriteFlag(sei.m_filmGrainCharacteristicsCancelFlag,        "fg_characteristics_cancel_flag");
  if (!sei.m_filmGrainCharacteristicsCancelFlag)
  {
    xWriteCode(sei.m_filmGrainModelId, 2,                     "fg_model_id");
    xWriteFlag(sei.m_separateColourDescriptionPresentFlag,    "fg_separate_colour_description_present_flag");
    if (sei.m_separateColourDescriptionPresentFlag)
    {
      xWriteCode(sei.m_filmGrainBitDepthLumaMinus8, 3,        "fg_bit_depth_luma_minus8");
      xWriteCode(sei.m_filmGrainBitDepthChromaMinus8, 3,      "fg_bit_depth_chroma_minus8");
      xWriteFlag(sei.m_filmGrainFullRangeFlag,                "fg_full_range_flag");
      xWriteCode(sei.m_filmGrainColourPrimaries, 8,           "fg_colour_primaries");
      xWriteCode(sei.m_filmGrainTransferCharacteristics, 8,   "fg_transfer_characteristics");
#if JVET_AL0301_MATRIXCOEFFS_CONSTRAINTS
      CHECK((sei.m_filmGrainMatrixCoeffs == 0 || sei.m_filmGrainMatrixCoeffs == 16 || sei.m_filmGrainMatrixCoeffs == 17) && !(sei.m_filmGrainBitDepthLumaMinus8 == sei.m_filmGrainBitDepthChromaMinus8),
        "fg_matrix_coeffs shall not be equal to 0, 16, or 17 unless fg_bit_depth_luma_minus8 is equal to fg_bit_depth_chroma_minus8")
      CHECK(sei.m_filmGrainMatrixCoeffs == 8 && !(sei.m_filmGrainBitDepthLumaMinus8 == sei.m_filmGrainBitDepthChromaMinus8 || sei.m_filmGrainBitDepthLumaMinus8 + 1 == sei.m_filmGrainBitDepthChromaMinus8),
        "fg_matrix_coeffs shall not be equal to 8 unless fg_bit_depth_chroma_minus8 is equal to fg_bit_depth_luma_minus8 or fg_bit_depth_luma_minus8 + 1")
#endif
      xWriteCode(sei.m_filmGrainMatrixCoeffs, 8,              "fg_matrix_coeffs");
    }
    xWriteCode(sei.m_blendingModeId, 2,                       "fg_blending_mode_id");
    xWriteCode(sei.m_log2ScaleFactor, 4,                      "fg_log2_scale_factor");
    for (int c = 0; c<3; c++)
    {
      const SEIFilmGrainCharacteristics::CompModel &cm = sei.m_compModel[c];
      const uint32_t numIntensityIntervals = (uint32_t) cm.numIntensityIntervals;
      const uint32_t numModelValues = cm.numModelValues;
      xWriteFlag(sei.m_compModel[c].presentFlag && numIntensityIntervals>0 && numModelValues>0, "fg_comp_model_present_flag[c]");
    }
    for (uint32_t c = 0; c<3; c++)
    {
      const SEIFilmGrainCharacteristics::CompModel &cm = sei.m_compModel[c];
      const uint32_t numIntensityIntervals = (uint32_t) cm.numIntensityIntervals;
      const uint32_t numModelValues = cm.numModelValues;
      if (cm.presentFlag && numIntensityIntervals>0 && numModelValues>0)
      {
        assert(numIntensityIntervals <= 256);
        assert(numModelValues <= 256);
        xWriteCode(numIntensityIntervals - 1, 8,              "fg_num_intensity_intervals_minus1[c]");
        xWriteCode(numModelValues - 1, 3,                     "fg_num_model_values_minus1[c]");
        for (uint32_t interval = 0; interval<numIntensityIntervals; interval++)
        {
          const SEIFilmGrainCharacteristics::CompModelIntensityValues &cmiv = cm.intensityValues[interval];
          xWriteCode(cmiv.intensityIntervalLowerBound, 8,     "fg_intensity_interval_lower_bound[c][i]");
          xWriteCode(cmiv.intensityIntervalUpperBound, 8,     "fg_intensity_interval_upper_bound[c][i]");
          for (uint32_t j = 0; j<cm.numModelValues; j++)
          {
            xWriteSvlc(cmiv.compModelValue[j],                "fg_comp_model_value[c][i]");
          }
        }
      }
    } // for c
    xWriteFlag(sei.m_filmGrainCharacteristicsPersistenceFlag, "fg_characteristics_persistence_flag");
  } // cancel flag
#if JVET_AL0339_FGS_SEI_SPATIAL_RESOLUTION
  if (sei.m_spatialResolutionPresentFlag)
  {
    // SEI payload extension bits
    xWriteFlag(sei.m_spatialResolutionPresentFlag,            "fg_spatialresolution_present_flag");
    xWriteUvlc(sei.m_picWidthInLumaSamples,                   "fg_pic_width_in_luma_samples");
    xWriteUvlc(sei.m_picHeightInLumaSamples,                  "fg_pic_height_in_luma_samples");
    xWriteFlag(1,                                             "payload_bit_equal_to_one");
    while (!isByteAligned())
    {
      xWriteFlag(0,                                           "payload_bit_equal_to_zero");
    }
  }
#endif
}

void SEIWriter::xWriteSEIContentLightLevelInfo(const SEIContentLightLevelInfo& sei)
{
  xWriteCode( sei.m_maxContentLightLevel,    16, "clli_max_content_light_level"     );
  xWriteCode( sei.m_maxPicAverageLightLevel, 16, "clli_max_pic_average_light_level" );
}

void SEIWriter::xWriteSEIAmbientViewingEnvironment(const SEIAmbientViewingEnvironment& sei)
{
  xWriteCode(sei.m_ambientIlluminance, 32, "ambient_illuminance" );
  xWriteCode(sei.m_ambientLightX,      16, "ambient_light_x" );
  xWriteCode(sei.m_ambientLightY,      16, "ambient_light_y" );
}

void SEIWriter::xWriteSEIContentColourVolume(const SEIContentColourVolume &sei)
{
  xWriteFlag(sei.m_ccvCancelFlag, "ccv_cancel_flag");
  if (!sei.m_ccvCancelFlag)
  {
    xWriteFlag(sei.m_ccvPersistenceFlag, "ccv_persistence_flag");
    xWriteFlag(sei.m_ccvPrimariesPresentFlag, "ccv_primaries_present_flag");
    xWriteFlag(sei.m_ccvMinLuminanceValuePresentFlag, "ccv_min_luminance_value_present_flag");
    xWriteFlag(sei.m_ccvMaxLuminanceValuePresentFlag, "ccv_max_luminance_value_present_flag");
    xWriteFlag(sei.m_ccvAvgLuminanceValuePresentFlag, "ccv_avg_luminance_value_present_flag");

    if (sei.m_ccvPrimariesPresentFlag == true)
    {
      for (int i = 0; i < MAX_NUM_COMPONENT; i++)
      {
        xWriteSCode((int32_t)sei.m_ccvPrimariesX[i], 32, "ccv_primaries_x[i]");
        xWriteSCode((int32_t)sei.m_ccvPrimariesY[i], 32, "ccv_primaries_y[i]");
      }
    }

    if (sei.m_ccvMinLuminanceValuePresentFlag == true)
    {
      xWriteCode((uint32_t)sei.m_ccvMinLuminanceValue, 32, "ccv_min_luminance_value");
    }
    if (sei.m_ccvMaxLuminanceValuePresentFlag == true)
    {
      xWriteCode((uint32_t)sei.m_ccvMaxLuminanceValue, 32, "ccv_max_luminance_value");
    }
    if (sei.m_ccvAvgLuminanceValuePresentFlag == true)
    {
      xWriteCode((uint32_t)sei.m_ccvAvgLuminanceValue, 32, "ccv_avg_luminance_value");
    }
  }
}

void SEIWriter::xWriteSEIColourTransformInfo(const SEIColourTransformInfo& sei)
{
  bool colourTransformCancelFlag = 0;
  bool colourTransformPersistenceFlag = 0;

  xWriteUvlc(sei.m_id, "colour_transform_id");
  xWriteFlag(colourTransformCancelFlag, "colour_transform_cancel_flag");

  if (colourTransformCancelFlag == 0)
  {
    xWriteFlag(colourTransformPersistenceFlag, "colour_transform_persistence_flag");
    xWriteFlag(sei.m_signalInfoFlag, "colour_transform_video_signal_info_present_flag");

    if (sei.m_signalInfoFlag)
    {
      xWriteFlag(sei.m_fullRangeFlag, "colour_transform_full_range_flag");
      xWriteCode(sei.m_primaries, 8, "colour_transform_primaries");
      xWriteCode(sei.m_transferFunction, 8, "colour_transform_transfer_function");
      xWriteCode(sei.m_matrixCoefs, 8, "colour_transform_matrix_coefficients");
    }
    xWriteCode(sei.m_bitdepth - 8, 4, "colour_transform_bit_depth_minus8");
    xWriteCode(sei.m_log2NumberOfPointsPerLut - 1, 3, "colour_transform_log2_number_of_points_per_lut_minus1");
    xWriteFlag(sei.m_crossComponentFlag, "colour_transform_cross_comp_flag");
    if (sei.m_crossComponentFlag)
    {
      xWriteFlag(sei.m_crossComponentInferred, "colour_transform_cross_comp_inferred");
    }

    uint16_t lutCodingLength = 2 + sei.m_bitdepth - sei.m_log2NumberOfPointsPerLut;
    for (uint32_t j = 0; j < sei.m_lut[0].numLutValues; j++)
    {
      xWriteCode(sei.m_lut[0].lutValues[j], lutCodingLength, "colour_transform_lut[0][i]");
    }
    if (sei.m_crossComponentFlag == 0 || sei.m_crossComponentInferred == 0)
    {
      xWriteFlag(sei.m_numberChromaLutMinus1, "colour_transform_number_chroma_lut_minus1");
      for (uint32_t j = 0; j < sei.m_lut[1].numLutValues; j++)
      {
        xWriteCode(sei.m_lut[1].lutValues[j], lutCodingLength, "colour_transform_lut[1][i]");
      }
      if (sei.m_numberChromaLutMinus1 == 1)
      {
        for (uint32_t j = 0; j < sei.m_lut[2].numLutValues; j++)
        {
          xWriteCode(sei.m_lut[2].lutValues[j], lutCodingLength, "colour_transform_lut[2][i]");
        }
      }
    }
    else
    {
      xWriteCode(sei.m_chromaOffset, lutCodingLength, "colour_transform_chroma_offset");
    }
  }
}

void SEIWriter::xWriteSEIShutterInterval(const SEIShutterIntervalInfo &sei)
{
  xWriteCode(sei.m_siiTimeScale, 32, "sii_time_scale");
  xWriteFlag(sei.m_siiFixedSIwithinCLVS, "fixed_shutter_interval_within_clvs_flag");
  if (sei.m_siiFixedSIwithinCLVS)
  {
    xWriteCode(sei.m_siiNumUnitsInShutterInterval, 32, "sii_num_units_in_shutter_interval");
  }
  else
  {
    xWriteCode(sei.m_siiMaxSubLayersMinus1, 3, "sii_max_sub_layers_minus1");
    for (unsigned i = 0; i <= sei.m_siiMaxSubLayersMinus1; i++)
    {
      xWriteCode(sei.m_siiSubLayerNumUnitsInSI[i], 32, "sub_layer_num_units_in_shutter_interval[ i ]");
    }
  }
}

void SEIWriter::xWriteSEIProcessingOrder(OutputBitstream& bs, const SEIProcessingOrderInfo& sei)
{
  CHECK(sei.m_posPayloadType.size() < 2, "An SEI processing order SEI message shall contain at least two pairs sei_payloadType[i] and sei_processingOrder[i]");
  SEIMessages wrapSEI;
  xWriteCode(sei.m_posId, 8, "po_sei_id");
  xWriteCode(sei.m_posForHumanViewingIdc, 2, "po_for_human_viewing_idc");
  xWriteCode(sei.m_posForMachineAnalysisIdc, 2, "po_for_machine_analysis_idc");
  xWriteCode(0, 4, "po_reserved_zero_4bits");
  xWriteCode(sei.m_posNumMinus2, 7, "po_num_sei_message_minus2");
  xWriteFlag(sei.m_posBreadthFirstFlag, "po_breadth_first_flag");
  for (uint32_t i = 0; i < ( sei.m_posNumMinus2 + 2 ); i++)
  {
    xWriteFlag(sei.m_posWrappingFlag[i], "po_sei_wrapping_flag[i]");
    xWriteFlag(sei.m_posImportanceFlag[i], "po_sei_importance_flag[i]");
    xWriteFlag(sei.m_posProcessingDegreeFlag[i], "po_sei_processing_degree_flag[i]");
      xWriteCode(sei.m_posPayloadType[i], 13, "po_sei_payload_type[i]");
      xWriteFlag(sei.m_posPrefixFlag[i], "po_sei_prefix_flag[i]");

    CHECK((i > 0) && (sei.m_posProcessingOrder[i] < sei.m_posProcessingOrder[i-1]) , "For i greater than 0, po_sei_processing_order[i] shall be greater than or equal to po_sei_processing_order[i-1]");
    xWriteCode(sei.m_posProcessingOrder[i], 8, "po_sei_processing_order[i]");
  }

  for (uint32_t i = 0; i < ( sei.m_posNumMinus2 + 2 ); i++)
  {
    if (sei.m_posPrefixFlag[i])
    {
      xWriteCode(sei.m_posNumBitsInPrefix[i], 8, "po_num_bits_in_prefix_indication_minus1[i]");
      for (uint32_t j = 0; j < sei.m_posNumBitsInPrefix[i]; j += 8)
      {
        uint32_t numBits = (sei.m_posNumBitsInPrefix[i] - j) < 8 ? (sei.m_posNumBitsInPrefix[i] - j) : 8;
        for (int k = (int)numBits - 1; k >= 0; k--)
        {
          xWriteCode((sei.m_posPrefixByte[i][j>>3] >> k) & 1, 1, "po_sei_prefix_data_bit[i][j]");
        }
      }
      while (!isByteAligned())
      {
        xWriteCode(1, 1, "po_byte_alignment_bit_equal_to_one");
      }
    }
  }
#if JVET_AJ0105_SPO_COMPLEXITY_INFO
  xWriteFlag(sei.m_posComplexityInfoPresentFlag, "po_complexity_info_present_flag");
  if (sei.m_posComplexityInfoPresentFlag)
  {
    xWriteCode(sei.m_posParameterTypeIdc, 2, "po_parameter_type_idc");
    if (sei.m_posParameterTypeIdc != 2)
    {
      xWriteCode(sei.m_posLog2ParameterBitLengthMinus3, 2, "po_log2_parameter_bit_length_minus3");
    }
    xWriteCode(sei.m_posNumParametersIdc, 6, "po_num_parameters_idc");
    xWriteUvlc(sei.m_posNumKmacOperationIdc, "po_num_kmac_operation_idc");
    xWriteUvlc(sei.m_posTotalKilobyteSize, "po_total_kilobyte_size");
  }
#endif
}

void SEIWriter::xWriteSEIProcessingOrderNesting(OutputBitstream& bs, const SEIProcessingOrderNesting& sei)
{
  SEIMessages wrapSEI;
  xWriteCode((uint32_t)(sei.m_ponTargetPoId.size() - 1), 8, "pon_num_po_ids_minus1");
  for (int i = 0; i < (int)sei.m_ponTargetPoId.size(); i++)
  {
    xWriteCode(sei.m_ponTargetPoId[i], 8, "pon_target_po_id[i]");
  }
  xWriteCode(sei.m_ponNumSeisMinus1, 8, "pon_num_seis_minus1");
  for (int i = 0; i <= sei.m_ponNumSeisMinus1; i++)
  {
    CHECK((i > 0) && (sei.m_ponProcessingOrder[i] < sei.m_ponProcessingOrder[i-1]) , "When i is greater than 0, pon_processing_order[i] shall be greater than or equal to pon_processing_order[i-1]");
    xWriteCode(sei.m_ponProcessingOrder[i], 8, "pon_processing_order[i]");
    wrapSEI = getSeisByType(sei.m_ponWrapSeiMessages, SEI::PayloadType(sei.m_ponPayloadType[i]));
    writeSEImessages(bs, wrapSEI, m_nestingHrd, true, 0);
  }
}

void SEIWriter::xWriteSEIConstrainedRaslIndication(const SEIConstrainedRaslIndication& /*sei*/)
{
  // intentionally empty
}

#if GREEN_METADATA_SEI_ENABLED
void SEIWriter::xWriteSEIGreenMetadataInfo(const SEIGreenMetadataInfo& sei)
{
  xWriteCode(sei.m_greenMetadataType, 8, "green_metadata_type");
  switch (sei.m_greenMetadataType)
  {
  case 0:
    xWriteCode(sei.m_periodType,4, "period_type");
    xWriteCode(sei.m_greenMetadataGranularityType,3, "granularity_type");
    xWriteCode(sei.m_greenMetadataExtendedRepresentation,1, "extended_representation_flag");
    
    if (sei.m_periodType == 2)
    {
      xWriteCode(sei.m_numSeconds, 16, "num_seconds");
    }
    else if (sei.m_periodType == 3)
    {
      xWriteCode(sei.m_numPictures, 16, "num_pictures");
    }
    
    if (sei.m_greenMetadataGranularityType == 0)
    {
      xWriteCode(sei.m_greenComplexityMetrics.portionNonZeroBlocksArea, 8, "portion_non_zero_blocks_area");
      xWriteCode(sei.m_greenComplexityMetrics.portionNonZeroTransformCoefficientsArea, 8, "portion_non_zero_transform_coefficients_area");
      xWriteCode(sei.m_greenComplexityMetrics.portionIntraPredictedBlocksArea, 8, "portion_intra_predicted_blocks_area");
      xWriteCode(sei.m_greenComplexityMetrics.portionDeblockingInstances, 8, "portion_deblocking_instances");
      xWriteCode(sei.m_greenComplexityMetrics.portionAlfInstances, 8, "portion_alf_instances");
      
      if(sei.m_greenMetadataExtendedRepresentation == 1)
      {
        if(sei.m_greenComplexityMetrics.portionNonZeroBlocksArea != 0)
        {
          xWriteCode(sei.m_greenComplexityMetrics.portionNonZero_4_8_16BlocksArea, 8, "portion_non_zero_4_8_16_blocks_area");
          xWriteCode(sei.m_greenComplexityMetrics.portionNonZero_32_64_128BlocksArea, 8, "portion_non_zero_32_64_128_blocks_area");
          xWriteCode(sei.m_greenComplexityMetrics.portionNonZero_256_512_1024BlocksArea, 8, "portion_non_zero_256_512_1024_blocks_area");
          xWriteCode(sei.m_greenComplexityMetrics.portionNonZero_2048_4096BlocksArea, 8, "portion_non_zero_2048_4096_blocks_area");
        }
        
        
        if(sei.m_greenComplexityMetrics.portionIntraPredictedBlocksArea < 255)
        {
          xWriteCode(sei.m_greenComplexityMetrics.portionBiAndGpmPredictedBlocksArea, 8,"portion_bi_and_gpm_predicted_blocks_area");
          xWriteCode(sei.m_greenComplexityMetrics.portionBdofBlocksArea, 8,"portion_bdof_blocks_area");
        }
        
        xWriteCode(sei.m_greenComplexityMetrics.portionSaoInstances, 8, "portion_sao_instances");
      }
    }
    
    break;
  case 1:
    {
      int xsdSubpicNumberMinus1 = 0;
      xWriteCode(xsdSubpicNumberMinus1, 16, "xsd_subpic_number_minus1");
      for (int i = 0; i <= xsdSubpicNumberMinus1; i++)
      {
        int xsdMetricNumberMinus1 = -1;
        xWriteCode(sei.m_xsdSubPicIdc, 16, "xsd_subpic_idc[i]");
        std::vector<int> xsdMetricArray;
        if (sei.m_xsdMetricTypePSNR)
        {
          xsdMetricNumberMinus1++;
          xsdMetricArray.push_back(0);
        }
        if (sei.m_xsdMetricTypeSSIM)
        {
          xsdMetricNumberMinus1++;
          xsdMetricArray.push_back(1);
        }

        if (sei.m_xsdMetricTypeWPSNR)
        {
          xsdMetricNumberMinus1++;
          xsdMetricArray.push_back(2);
        }

        if (sei.m_xsdMetricTypeWSPSNR)
        {
          xsdMetricNumberMinus1++;
          xsdMetricArray.push_back(3);
        }

        xWriteCode(xsdMetricNumberMinus1, 8, "xsd_metric_number_minus1[i]");
        for (int j = 0; j <= xsdMetricNumberMinus1; j++)
        {
          if (xsdMetricArray[j] == 0)
          {
            xWriteCode(0, 8, "xsd_metric_type[i][j]");
            xWriteCode(sei.m_xsdMetricValuePSNR, 16, "xsd_metric_value[i][j]");
          }
          else if (xsdMetricArray[j] == 1)
          {
            xWriteCode(1, 8, "xsd_metric_type[i][j]");
            xWriteCode(sei.m_xsdMetricValueSSIM, 16, "xsd_metric_value[i][j]");
          }
          else if (xsdMetricArray[j] == 2)
          {
            xWriteCode(3, 8, "xsd_metric_type[i][j]");
            xWriteCode(sei.m_xsdMetricValueWPSNR, 16, "xsd_metric_value[i][j]");
          }
          else if (xsdMetricArray[j] == 3)
          {
            xWriteCode(4, 8, "xsd_metric_type[i][j]");
            xWriteCode(sei.m_xsdMetricValueWSPSNR, 16, "xsd_metric_value[i][j]");
          }
        }
      }
      break;
    }
#if GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464
  case 2:
    {
      CHECK(sei.m_greenMetadataAMIFlags > 63, "green_metadata_ami_flags shall be in the range 0-63");
      xWriteCode(sei.m_greenMetadataAMIFlags, 8, "green_metadata_ami_flags");
      if ((sei.m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::CANCEL) != GREEN_METADATA_AMI_FLAGS::CANCEL)
      {
        CHECK(sei.m_greenMetadataAMIDisplayModel > 3, "green_metadata_ami_display_model shall be in the range 0-3");
        xWriteCode(sei.m_greenMetadataAMIDisplayModel, 4, "green_metadata_ami_display_model");
        if ((sei.m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::APPROX) == GREEN_METADATA_AMI_FLAGS::APPROX)
        {
          CHECK(sei.m_greenMetadataAMIApproximationModel > 4,
                "green_metadata_ami_approximation_model shall be in the range 0-4");
          xWriteCode(sei.m_greenMetadataAMIApproximationModel, 4, "green_metadata_ami_approximation_model");
        }
        CHECK(sei.m_greenMetadataAMIMapNumber > 7, "green_metadata_ami_map_number shall be in the range 0-7");
        xWriteCode(sei.m_greenMetadataAMIMapNumber, 3, "green_metadata_ami_map_number");

        for (int i = 0; i < sei.m_greenMetadataAMIMapNumber; i++)
        {
          CHECK(sei.m_greenMetadataAMILayerId[i] > 255, "green_metadata_ami_layer_id shall be in the range 0-255");
          xWriteCode(sei.m_greenMetadataAMILayerId[i], 8, "green_metadata_ami_layer_id[i]");
          CHECK(sei.m_greenMetadataAMIOlsNumber[i] > 15, "green_metadata_ami_ols_number shall be in the range 0-15");
          xWriteCode(sei.m_greenMetadataAMIOlsNumber[i], 4, "green_metadata_ami_ols_number[i]");
          for (int j = 0; j < sei.m_greenMetadataAMIOlsNumber[i]; j++)
          {
            CHECK(sei.m_greenMetadataAMIOlsId[i][j] > 255, "green_metadata_ami_ols_id shall be in the range 0-255");
            xWriteCode(sei.m_greenMetadataAMIOlsId[i][j], 8, "green_metadata_ami_ols_id[i][j]");
          }
          CHECK(sei.m_greenMetadataAMIEnergyReductionRate[i] > 31,
                "green_metadata_ami_energy_reduction_rate shall be in the range 0-31");
          xWriteCode(sei.m_greenMetadataAMIEnergyReductionRate[i], 5, "green_metadata_ami_energy_reduction_rate[i]");
          if ((sei.m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::QUALITY) == GREEN_METADATA_AMI_FLAGS::QUALITY)
          {
            CHECK(sei.m_greenMetadataAMIVideoQualityMetricType[i] > 3,
                  "green_metadata_ami_video_quality_metric_type shall be in the range 0-3");
            xWriteCode(sei.m_greenMetadataAMIVideoQualityMetricType[i], 3,
                       "green_metadata_ami_video_quality_metric_type[i]");
            CHECK(sei.m_greenMetadataAMIVideoQualityLevel[i] > 65535,
                  "green_metadata_ami_video_quality_level shall be in the range 0-65535");
            xWriteCode(sei.m_greenMetadataAMIVideoQualityLevel[i], 16, "green_metadata_ami_video_quality_level[i]");
          }
          CHECK(sei.m_greenMetadataAMIMaxValue[i] > 255, "green_metadata_ami_max_value shall be in the range 0-255");
          xWriteCode(sei.m_greenMetadataAMIMaxValue[i], 8, "green_metadata_ami_max_value[i]");
          if (((sei.m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::GLOBAL) != GREEN_METADATA_AMI_FLAGS::GLOBAL)
              || (i == 0))
          {
            CHECK(sei.m_greenMetadataAMIAttenuationUseIdc[i] > 2,
                  "green_metadata_ami_attenuation_use_idc shall be in the range 0-2");
            xWriteCode(sei.m_greenMetadataAMIAttenuationUseIdc[i], 4, "green_metadata_ami_attenuation_use_idc[i]");
            CHECK(sei.m_greenMetadataAMIAttenuationCompIdc[i] > 6,
                  "green_metadata_ami_attenuation_comp_idc shall be in the range 0-6");
            xWriteCode(sei.m_greenMetadataAMIAttenuationCompIdc[i], 4, "green_metadata_ami_attenuation_comp_idc[i]");
            if ((sei.m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::PREPROC) == GREEN_METADATA_AMI_FLAGS::PREPROC)
            {
              // CHECK( std::is_same_v< decltype((sei.m_greenMetadataAMIPreprocessingFlag[i])), bool > == true,
              //      "green_metadata_ami_preprocessing_flag shall be a boolean"); // CHD
              xWriteFlag(sei.m_greenMetadataAMIPreprocessingFlag[i], "green_metadata_ami_preprocessing_flag[i]");
              if (sei.m_greenMetadataAMIPreprocessingFlag[i])
              {
                CHECK(sei.m_greenMetadataAMIPreprocessingTypeIdc[i] > 3,
                      "green_metadata_ami_preprocessing_type shall be in the range 0-3");
                xWriteCode(sei.m_greenMetadataAMIPreprocessingTypeIdc[i], 2,
                           "green_metadata_ami_preprocessing_type_idc[i]");
              }
              CHECK(sei.m_greenMetadataAMIPreprocessingScaleIdc[i] > 2,
                    "green_metadata_ami_preprocessing_scale_idc shall be in the range 0-2");
              xWriteCode(sei.m_greenMetadataAMIPreprocessingScaleIdc[i], 4,
                         "green_metadata_ami_preprocessing_scale_idc[i]");
            }
            if ((sei.m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::BACKLIGHT)
                == GREEN_METADATA_AMI_FLAGS::BACKLIGHT)
            {
              CHECK(sei.m_greenMetadataAMIBacklightScalingIdc[i] > 1,
                    "green_metadata_ami_backlight_scaling_idc shall be in the range 0-1");
              xWriteCode(sei.m_greenMetadataAMIBacklightScalingIdc[i], 4,
                         "green_metadata_ami_backlight_scaling_idc[i]");
            }
          }
        }
        if (sei.m_greenMetadataAMIMapNumber == 0)
        {
          CHECK(sei.m_greenMetadataAMIEnergyReductionRate[0] > 31,
                "green_metadata_ami_energy_reduction_rate shall be in the range 0-31");
          xWriteCode(sei.m_greenMetadataAMIEnergyReductionRate[0], 5, "green_metadata_ami_energy_reduction_rate[0]");
          if ((sei.m_greenMetadataAMIFlags & GREEN_METADATA_AMI_FLAGS::QUALITY) == GREEN_METADATA_AMI_FLAGS::QUALITY)
          {
            CHECK(sei.m_greenMetadataAMIVideoQualityMetricType[0] > 3,
                  "green_metadata_ami_video_quality_metric_type shall be in the range 0-3");
            xWriteCode(sei.m_greenMetadataAMIVideoQualityMetricType[0], 3,
                       "green_metadata_ami_video_quality_metric_type[0]");
            CHECK(sei.m_greenMetadataAMIVideoQualityLevel[0] > 65535,
                  "green_metadata_ami_video_quality_level shall be in the range 0-65535");
            xWriteCode(sei.m_greenMetadataAMIVideoQualityLevel[0], 16, "green_metadata_ami_video_quality_level[0]");
          }
        }
      }
    }
    break;
#endif
  }
}
#endif


void SEIWriter::xWriteSEINeuralNetworkPostFilterCharacteristics(const SEINeuralNetworkPostFilterCharacteristics &sei)
{
  xWriteCode(sei.m_purpose, 16, "nnpfc_purpose");
  xWriteUvlc(sei.m_id, "nnpfc_id");
  xWriteFlag(sei.m_baseFlag, "nnpfc_base_flag");
  xWriteUvlc(sei.m_modeIdc, "nnpfc_mode_idc");
  if (sei.m_modeIdc == POST_FILTER_MODE::URI)
  {
    while (!isByteAligned())
    {
      xWriteFlag(0, "nnpfc_alignment_zero_bit");
    }
    xWriteString(sei.m_uriTag, "nnpfc_uri_tag");
    xWriteString(sei.m_uri, "nnpfc_uri");
  }
  xWriteFlag(sei.m_propertyPresentFlag, "nnpfc_property_present_flag");
  if (sei.m_propertyPresentFlag)
  {
    xWriteUvlc(sei.m_numberInputDecodedPicturesMinus1, "nnpfc_number_of_input_pictures_minus1");

    if (sei.m_numberInputDecodedPicturesMinus1 > 0)
    {
      for (int i = 0; i <= sei.m_numberInputDecodedPicturesMinus1; ++i)
      {
        xWriteFlag(sei.m_inputPicOutputFlag[i], "nnpfc_input_pic_filtering_flag");
      }
      xWriteFlag(sei.m_absentInputPicZeroFlag, "nnpfc_absent_input_pic_zero_flag");
    }

    if((sei.m_purpose & NNPC_PurposeType::CHROMA_UPSAMPLING) != 0)
    {
      xWriteFlag(sei.m_outSubCFlag, "nnpfc_out_sub_c_flag");
    }
    if((sei.m_purpose & NNPC_PurposeType::COLOURIZATION) != 0)
    {
      xWriteCode(uint32_t(sei.m_outColourFormatIdc), 2, "nnpfc_out_colour_format_idc");
    }
    if((sei.m_purpose & NNPC_PurposeType::RESOLUTION_UPSAMPLING) != 0)
    {
      xWriteUvlc(sei.m_picWidthNumeratorMinus1, "nnpfc_pic_width_num_minus1");
      xWriteUvlc(sei.m_picWidthDenominatorMinus1, "nnpfc_pic_width_denom_minus1");
      xWriteUvlc(sei.m_picHeightNumeratorMinus1, "nnpfc_pic_height_num_minus1");
      xWriteUvlc(sei.m_picHeightDenominatorMinus1, "nnpfc_pic_height_denom_minus1");
    }

    if((sei.m_purpose & NNPC_PurposeType::FRAME_RATE_UPSAMPLING) != 0)
    {
      for (int i = 0; i < sei.m_numberInputDecodedPicturesMinus1; ++i)
      {
        xWriteUvlc(sei.m_numberInterpolatedPictures[i], "nnpfc_interpolated_pictures");
      }
    }

    if((sei.m_purpose & NNPC_PurposeType::TEMPORAL_EXTRAPOLATION) != 0)
    {
      xWriteUvlc(sei.m_numberExtrapolatedPicturesMinus1, "nnpfc_extrapolated_pics_minus1");
    }

    if((sei.m_purpose & NNPC_PurposeType::SPATIAL_EXTRAPOLATION) != 0)
    {
      xWriteSvlc(sei.m_spatialExtrapolationLeftOffset, "nnpfc_spatial_extrapolation_left_offset");
      xWriteSvlc(sei.m_spatialExtrapolationRightOffset, "nnpfc_spatial_extrapolation_right_offset");
      xWriteSvlc(sei.m_spatialExtrapolationTopOffset, "nnpfc_spatial_extrapolation_top_offset");
      xWriteSvlc(sei.m_spatialExtrapolationBottomOffset, "nnpfc_spatial_extrapolation_right_offset");
    }

    xWriteFlag(sei.m_componentLastFlag, "nnpfc_component_last_flag");
    xWriteUvlc(sei.m_inpFormatIdc, "nnpfc_inp_format_idc");
    xWriteUvlc(sei.m_auxInpIdc, "nnpfc_auxiliary_inp_idc");
    if ((sei.m_auxInpIdc & 2) > 0)
    {
      xWriteFlag(sei.m_inbandPromptFlag, "nnpfc_inband_prompt_flag");
      if (sei.m_inbandPromptFlag)
      {
        while (!isByteAligned())
        {
          xWriteFlag(0, "nnpfc_alignment_zero_bit_c");
        }
        xWriteString(sei.m_prompt, "nnpfc_prompt");
      }
    }
#if JVET_AK0326_NNPF_SEED
    if ((sei.m_auxInpIdc & 4) > 0)
    {
      xWriteFlag(sei.m_inbandSeedFlag, "nnpfc_inband_seed_flag");
      if (sei.m_inbandSeedFlag)
      {
        xWriteCode(sei.m_seed, 16, "nnpfc_seed");
      }
    }
#endif
    xWriteUvlc(sei.m_inpOrderIdc, "nnpfc_inp_order_idc");
    if (sei.m_inpFormatIdc == 1)
    {
      if (sei.m_inpOrderIdc != 1)
      {
        xWriteUvlc(sei.m_inpTensorBitDepthLumaMinus8, "nnpfc_inp_tensor_luma_bitdepth_minus8");
      }
      if (sei.m_inpOrderIdc != 0)
      {
        xWriteUvlc(sei.m_inpTensorBitDepthChromaMinus8, "nnpfc_inp_tensor_chroma_bitdepth_minus8");
      }
    }
    xWriteUvlc(sei.m_outFormatIdc, "nnpfc_out_format_idc");
    xWriteUvlc(sei.m_outOrderIdc, "nnpfc_out_order_idc");
    if (sei.m_outFormatIdc == 1)
    {
      if (sei.m_outOrderIdc != 1)
      {
        xWriteUvlc(sei.m_outTensorBitDepthLumaMinus8, "nnpfc_out_tensor_luma_bitdepth_minus8");
      }
      if (sei.m_outOrderIdc != 0)
      {
        xWriteUvlc(sei.m_outTensorBitDepthChromaMinus8, "nnpfc_out_tensor_chroma_bitdepth_minus8");
      }
    }

    xWriteFlag(sei.m_sepColDescriptionFlag, "nnpfc_sep_col_desc_flag");

    if (sei.m_sepColDescriptionFlag)
    {
      xWriteCode(sei.m_colPrimaries, 8, "nnpfc_col_primaries");
      xWriteCode(sei.m_transCharacteristics, 8, "nnpfc_trans_characteristics");
      if (sei.m_outFormatIdc == 1)
      {
        xWriteCode(sei.m_matrixCoeffs, 8, "nnpfc_matrix_coeffs");
      }
    }
    if (sei.m_sepColDescriptionFlag && (sei.m_outFormatIdc == 1))
    {
      xWriteFlag(sei.m_fullRangeFlag, "nnpfc_full_range_flag");
    }
    
    if (sei.m_outOrderIdc != 0)
    {   
      xWriteFlag(sei.m_chromaLocInfoPresentFlag, "nnpfc_chroma_loc_info_present_flag");
    }

    if(sei.m_chromaLocInfoPresentFlag)
    {
      xWriteUvlc(to_underlying(sei.m_chromaSampleLocTypeFrame), "nnpfc_chroma_sample_loc_type_frame");
    }
    
    if((sei.m_purpose & NNPC_PurposeType::SPATIAL_EXTRAPOLATION) == 0)
    {
      xWriteUvlc(sei.m_overlap, "nnpfc_overlap");
      xWriteFlag(sei.m_constantPatchSizeFlag, "nnpfc_constant_patch_size_flag");
    }
    if (sei.m_constantPatchSizeFlag)
    {
      xWriteUvlc(sei.m_patchWidthMinus1, "nnpfc_patch_width_minus1");
      xWriteUvlc(sei.m_patchHeightMinus1, "nnpfc_patch_height_minus1");
    }
    else
    {
      xWriteUvlc(sei.m_extendedPatchWidthCdDeltaMinus1, "extended_nnpfc_patch_width_cd_delta_minus1");
      xWriteUvlc(sei.m_extendedPatchHeightCdDeltaMinus1, "extended_nnpfc_patch_height_cd_delta_minus1");
    }
    xWriteUvlc(sei.m_paddingType, "nnpfc_padding_type");
    if (sei.m_paddingType == NNPC_PaddingType::FIXED_PADDING)
    {
      if (sei.m_inpOrderIdc != 1)
      {
        xWriteUvlc(sei.m_lumaPadding, "nnpfc_luma_padding_val");
      }
      if (sei.m_inpOrderIdc != 0)
      {
        xWriteUvlc(sei.m_cbPadding, "nnpfc_cb_padding_val");
        xWriteUvlc(sei.m_crPadding, "nnpfc_cr_padding_val");
      }
    }

    xWriteFlag(sei.m_complexityInfoPresentFlag, "nnpfc_complexity_info_present_flag");
    if (sei.m_complexityInfoPresentFlag)
    {
      xWriteCode(sei.m_parameterTypeIdc, 2, "nnpfc_parameter_type_idc");
      if (sei.m_parameterTypeIdc != 2)
      {
        xWriteCode(sei.m_log2ParameterBitLengthMinus3, 2, "nnpfc_log2_parameter_bit_length_minus3");
      }
      xWriteCode(sei.m_numParametersIdc, 6, "nnpfc_num_parameters_idc");
      xWriteUvlc(sei.m_numKmacOperationsIdc, "nnpfc_num_kmac_operations_idc");
      xWriteUvlc(sei.m_totalKilobyteSize, "nnpfc_total_kilobyte_size");
    }
    uint32_t metadataExtensionNumBits = 0;
    if (sei.m_purpose == 0 || sei.m_forHumanViewingIdc != 0 || sei.m_forMachineAnalysisIdc != 0)
    {
      if (sei.m_purpose == 0)
      {
        metadataExtensionNumBits++;
        if (sei.m_applicationPurposeTagUriPresentFlag)
        {
          metadataExtensionNumBits +=  (static_cast<uint32_t>(sei.m_applicationPurposeTagUri.length() + 1) * 8);
        }
      }
      metadataExtensionNumBits += 4;  // nnpfc_for_human_viewing_idc and nnpfc_for_machine_analysis_idc bits
      xWriteUvlc(metadataExtensionNumBits, "nnpfc_metadata_extension_num_bits");
      if (sei.m_purpose == 0)
      {
        xWriteFlag(sei.m_applicationPurposeTagUriPresentFlag, "nnpfc_application_purpose_tag_uri_present_flag");
        if ( sei.m_applicationPurposeTagUriPresentFlag )
        {
#if JVET_AI0070_BYTE_ALIGNMENT
          while (!isByteAligned())
          {
            xWriteFlag(0, "nnpfc_metadata_alignment_zero_bit");
          }
#endif
          xWriteString(sei.m_applicationPurposeTagUri, "nnpfc_application_purpose_tag_uri"); 
        }
      }
#if NNPFC_SCAN_TYPE_IDC
      if((sei.m_purpose & NNPC_PurposeType::SPATIAL_EXTRAPOLATION) != 0 || (sei.m_purpose & NNPC_PurposeType::RESOLUTION_UPSAMPLING) != 0)
      {
        xWriteCode(sei.m_scanTypeIdc, 2, "nnpfc_scan_type_idc");
      }
#endif
      xWriteCode(sei.m_forHumanViewingIdc, 2, "nnpfc_for_human_viewing_idc");
      xWriteCode(sei.m_forMachineAnalysisIdc, 2, "nnpfc_for_machine_analysis_idc");
    }
    else
    {
      xWriteUvlc(metadataExtensionNumBits, "nnpfc_metadata_extension_num_bits");  
    }
  }
  if (sei.m_modeIdc == POST_FILTER_MODE::ISO_IEC_15938_17)
  {
    while (!isByteAligned())
    {
      xWriteFlag(0, "nnpfc_alignment_zero_bit");
    }
    for (long i = 0; i < sei.m_payloadLength; i++)
    {
      xWriteSCode(sei.m_payloadByte[i], 8, "nnpfc_payload_byte[i]");
    }
  }
}


void SEIWriter::xWriteSEINeuralNetworkPostFilterActivation(const SEINeuralNetworkPostFilterActivation &sei)
{
  xWriteUvlc(sei.m_targetId, "nnpfa_target_id");
  xWriteFlag(sei.m_cancelFlag, "nnpfa_cancel_flag");
  if(!sei.m_cancelFlag)
  {
    xWriteFlag(sei.m_persistenceFlag, "nnpfa_persistence_flag");
    xWriteFlag(sei.m_targetBaseFlag, "nnpfa_target_base_flag");
    xWriteFlag(sei.m_noPrevCLVSFlag, "nnpfa_no_prev_clvs_flag");
    if (sei.m_persistenceFlag)
    {
      xWriteFlag(sei.m_noFollCLVSFlag, "nnpfa_no_foll_clvs_flag");
    }
    xWriteUvlc((uint32_t)sei.m_outputFlag.size(), "nnpfa_num_output_entries");
    for (uint32_t i = 0; i < (uint32_t)sei.m_outputFlag.size(); i++)
    {
      xWriteFlag(sei.m_outputFlag[i], "nnpfa_output_flag");
    }
#if JVET_AJ0104_NNPFA_PROMPT_UPDATE  
    xWriteFlag(sei.m_promptUpdateFlag, "nnpfa_prompt_update_flag");
    if (sei.m_promptUpdateFlag)
    {
      while (!isByteAligned())
      {
        xWriteFlag(0, "nnpfa_alignment_zero_bit");
      }
      xWriteString(sei.m_prompt, "nnpfa_prompt");
    }
#endif
#if JVET_AK0326_NNPF_SEED
    xWriteFlag(sei.m_seedUpdateFlag, "nnpfa_seed_update_flag");
    if (sei.m_seedUpdateFlag)
    {
      xWriteCode(sei.m_seed, 16, "nnpfa_seed");
    }
#endif
#if JVET_AJ0114_NNPFA_NUM_PIC_SHIFT
#if JVET_AL0075_NNPFA_SELECTED_INPUT_FLAG
    xWriteFlag(sei.m_selectedInputFlag, "nnpfa_selected_input_flag");
    if (sei.m_selectedInputFlag)
    {
      xWriteUvlc((uint32_t)sei.m_numInputPicShift, "nnpfa_num_input_pic_shift");
    }
#else
    xWriteUvlc((uint32_t)sei.m_numInputPicShift, "nnpfa_num_input_pic_shift");
#endif
#endif 

  }
}

void SEIWriter::xWriteSEITextDescription(const SEITextDescription &sei)
{
  CHECK(sei.m_textDescriptionPurpose > 6, "txt_descr_purpose shall be in the range 0-6");
  xWriteCode(sei.m_textDescriptionPurpose, 8, "txt_descr_purpose");
  xWriteFlag(sei.m_textCancelFlag, "txt_cancel_flag");
  if (!sei.m_textCancelFlag) 
  {
    CHECK((sei.m_textDescriptionID < 1 || sei.m_textDescriptionID > 16383), "text description id must be in the range 1-16383");
    xWriteCode(sei.m_textDescriptionID, 13, "txt_descr_id");
    xWriteFlag(sei.m_textIDCancelFlag, "txt_id_cancel_flag");
    if (!sei.m_textIDCancelFlag) 
    {
      xWriteFlag(sei.m_textPersistenceFlag, "txt_persistence_flag");
      xWriteCode(sei.m_textNumStringsMinus1, 8, "txt_num_strings_minus1");
      for (int i=0; i<=sei.m_textNumStringsMinus1; i++)
      {
        CHECK(sei.m_textDescriptionStringLang[i].length() > 49, "The length of the text description language string must be in the range 0-49");
        xWriteString(sei.m_textDescriptionStringLang[i], "txt_descr_string_lang[i]");
        xWriteString(sei.m_textDescriptionString[i], "txt_descr_string[i]");
      }
    }
  }
}

void SEIWriter::xWriteSEIPostFilterHint(const SEIPostFilterHint &sei)
{
  xWriteFlag(sei.m_filterHintCancelFlag, "filter_hint_cancel_flag");
  if (sei.m_filterHintCancelFlag == false)
  {
    xWriteFlag(sei.m_filterHintPersistenceFlag, "filter_hint_persistence_flag");
    xWriteUvlc(sei.m_filterHintSizeY, "filter_hint_size_y");
    xWriteUvlc(sei.m_filterHintSizeX, "filter_hint_size_x");
    xWriteCode(sei.m_filterHintType, 2, "filter_hint_type");
    xWriteFlag(sei.m_filterHintChromaCoeffPresentFlag, "filter_hint_chroma_coeff_present_flag");

    CHECK(!(sei.m_filterHintValues.size() == ((sei.m_filterHintChromaCoeffPresentFlag ? 3 : 1) * sei.m_filterHintSizeX * sei.m_filterHintSizeY)), "The number of filter coefficient shall match the matrix size and considering whether filters for chroma is present of not");
    for (uint32_t i = 0; i < sei.m_filterHintValues.size(); i++)
    {
      xWriteSvlc(sei.m_filterHintValues[i], "filter_hint_value[][][]");
    }
  }
}
void SEIWriter::xWriteSEIEncoderOptimizationInfo(const SEIEncoderOptimizationInfo &sei)
{
  xWriteFlag(sei.m_cancelFlag, "eoi_cancel_flag");
  if (!sei.m_cancelFlag)
  {
    xWriteFlag(sei.m_persistenceFlag, "eoi_persistence_flag");
    xWriteCode(sei.m_forHumanViewingIdc,2, "eoi_for_human_viewing_idc");
    xWriteCode(sei.m_forMachineAnalysisIdc,2, "eoi_for_machine_analysis_idc");
    xWriteCode(0, 2, "eoi_reserved_zero_2bits");
    xWriteCode(sei.m_type, 16, "eoi_type");

    if ((sei.m_type & EOI_OptimizationType::OBJECT_BASED_OPTIMIZATION) != 0)
    {
      xWriteCode(sei.m_objectBasedIdc, 16, "eoi_object_based_idc");
      if (sei.m_objectBasedIdc & EOI_OBJECT_BASED::COARSER_QUANTIZATION)
      {
        xWriteUvlc(sei.m_quantThresholdDelta, "eoi_quant_threshold_delta");
        if (sei.m_quantThresholdDelta > 0)
        {
          xWriteFlag(sei.m_picQuantObjectFlag, "eoi_pic_quant_object_flag");
        }
      }
    }
    if ((sei.m_type & EOI_OptimizationType::TEMPORAL_RESAMPLING) != 0)
    {
      xWriteFlag(sei.m_temporalResamplingTypeFlag, "eoi_temporal_resampling_type_flag");
      xWriteUvlc(sei.m_numIntPics, "eoi_num_int_pics");
#if JVET_AJ0183_EOI_SEI_SRC_PIC_FLAG
      if (sei.m_temporalResamplingTypeFlag && sei.m_numIntPics > 0)
      {
        xWriteFlag(sei.m_srcPicFlag, "eoi_src_pic_flag");
      }
#endif
    }
    if ((sei.m_type & EOI_OptimizationType::SPATIAL_RESAMPLING) != 0)
    {
      xWriteFlag(sei.m_origPicDimensionsFlag, "eoi_orig_pic_dimensions_flag");
      if (sei.m_origPicDimensionsFlag)
      {
#if JVET_AL0123_AL0310_EOI
        xWriteCode(sei.m_origPicWidthMinus1, 16, "eoi_orig_pic_width_minus1");
        xWriteCode(sei.m_origPicHeightMinus1, 16, "eoi_orig_pic_height_minus1");
#else
        xWriteCode(sei.m_origPicWidth, 16, "eoi_orig_pic_width");
        xWriteCode(sei.m_origPicHeight, 16, "eoi_orig_pic_height");
#endif
      }
      else
      {
#if JVET_AL0123_AL0310_EOI
        CHECK(sei.m_spatialHorResamplingTypeIdc == 0 && sei.m_spatialVerResamplingTypeIdc == 0, "When eoi_spatial_hor_resampling_type_idc and eoi_spatial_ver_resampling_type_idc are present, their values shall not be both equal to 0.");
        xWriteCode(sei.m_spatialHorResamplingTypeIdc, 2, "eoi_spatial_hor_resampling_type_idc");
        xWriteCode(sei.m_spatialVerResamplingTypeIdc, 2, "eoi_spatial_ver_resampling_type_idc");
#else
        xWriteFlag(sei.m_spatialResamplingTypeFlag, "eoi_spatial_resampling_type_flag");
#endif
      }
    }
    if ((sei.m_type & EOI_OptimizationType::PRIVACY_PROTECTION_OPTIMIZATION) != 0)
    {
      xWriteCode(sei.m_privacyProtectionTypeIdc, 16, "eoi_privacy_protection_type_idc");
      xWriteCode(sei.m_privacyProtectedInfoType, 8, "eoi_privacy_protected_info_type");
    }
  }
}
void SEIWriter::xWriteSEISourcePictureTimingInfo(const SEISourcePictureTimingInfo& sei)
{
  xWriteFlag(sei.m_sptiCancelFlag, "spti_cancel_flag");

  if (!sei.m_sptiCancelFlag)
  {
    xWriteFlag(sei.m_sptiPersistenceFlag, "spti_persistance_flag");
    xWriteFlag(sei.m_sptiSourceTimingEqualsOutputTimingFlag, "spti_source_timing_equals_output_timing_flag");

    if (!sei.m_sptiSourceTimingEqualsOutputTimingFlag)
    {
      xWriteFlag(sei.m_sptiSourceTypePresentFlag, "spti_source_type_present_flag");

      if (sei.m_sptiSourceTypePresentFlag)
      {
        xWriteCode(sei.m_sptiSourceType, 16, "spti_source_type");
      }

      xWriteCode(sei.m_sptiTimeScale, 32, "spti_time_scale");
      xWriteCode(sei.m_sptiNumUnitsInElementalInterval, 32, "spti_num_units_in_elemental_interval");
      xWriteFlag(sei.m_sptiDirectionFlag, "spti_direction_flag");

      if (sei.m_sptiPersistenceFlag)
      {
        xWriteCode(sei.m_sptiMaxSublayersMinus1, 3, "spti_max_sublayers_minus_1");
      }

      int sptiMinTemporalSublayer = (sei.m_sptiPersistenceFlag ? 0 : sei.m_sptiMaxSublayersMinus1);

      for (int i = sptiMinTemporalSublayer; i <= sei.m_sptiMaxSublayersMinus1; i++)
      {
        xWriteUvlc(sei.m_sptiSublayerIntervalScaleFactor[i], "spti_sublayer_interval_scale_factor");
        xWriteFlag(sei.m_sptiSublayerSynthesizedPictureFlag[i], "spti_sublayer_synthesized_picture_flag");
      }
    }
  }
}

void SEIWriter::xWriteSEIModalityInfo(const SEIModalityInfo& sei)
{
  xWriteFlag( sei.m_miCancelFlag,                                            "mi_modality_info_cancel_flag" );
  if(!sei.m_miCancelFlag)
  {
    xWriteFlag( sei.m_miPersistenceFlag,                                     "mi_modality_info_persistence_flag" );
    xWriteCode( (uint32_t)sei.m_miModalityType, 5,                           "mi_modality_type");
    xWriteFlag( sei.m_miSpectrumRangePresentFlag,                            "mi_spectrum_range_present_flag" );
    if (sei.m_miSpectrumRangePresentFlag)
    {
      xWriteCode( (uint32_t)sei.m_miMinWavelengthMantissa, 11,               "mi_min_wavelength_mantissa ");
      xWriteCode( (uint32_t)sei.m_miMinWavelengthExponentPlus15, 5,          "mi_min_wavelength_exponent_plus15 ");
      xWriteCode( (uint32_t)sei.m_miMaxWavelengthMantissa, 11,               "mi_max_wavelength_mantissa ");
      xWriteCode( (uint32_t)sei.m_miMaxWavelengthExponentPlus15, 5,          "mi_max_wavelength_exponent_plus15 ");
    }
    xWriteUvlc(0, "mi_modality_type_extension_bits");   // mi_modality_type_extension_bits shall be equal to 0 in the current edition 
  }
}

void SEIWriter::xWriteSEIGenerativeFaceVideo(const SEIGenerativeFaceVideo &sei)
{
  uint32_t basePicFlag = 0;
  std::vector<double>  coordinateXRec;
  std::vector<double>  coordinateYRec;
  std::vector<double>  coordinateZRec;
  std::vector<std::vector<std::vector<std::vector<double>>>>   matrixElementRec;
  xWriteUvlc(sei.m_id, "gfv_id");
  xWriteUvlc(sei.m_cnt, "gfv_cnt");
  if (sei.m_cnt == 0)
  {
    xWriteFlag(sei.m_basePicFlag, "gfv_base_picture_flag");
    basePicFlag = sei.m_basePicFlag;
  }
  else
  {
    basePicFlag = 0;
  }
  if (basePicFlag == 1)
  {
    xWriteFlag(sei.m_nnPresentFlag, "gfv_nn_present_flag");
    if (sei.m_nnPresentFlag)
    {
      xWriteUvlc(sei.m_nnModeIdc, "gfv_mode_idc");
      if (sei.m_nnModeIdc == 1)
      {
        while (!isByteAligned())
        {
          xWriteFlag(0, "gfv_reserved_zero_bit_a");
        }
        xWriteString(sei.m_nnTagURI, "gfv_uri_tag");
        xWriteString(sei.m_nnURI, "gfv_uri");
      }
    }
    xWriteFlag(sei.m_chromaKeyInfoPresentFlag, "gfv_chroma_key_info_presentFlag");
    if (sei.m_chromaKeyInfoPresentFlag)
    {
      for (uint32_t chromac = 0; chromac < 3; chromac++)
      {
        xWriteFlag(sei.m_chromaKeyValuePresentFlag[chromac], "gfv_chroma_key_value_present_flag[c]");
        if (sei.m_chromaKeyValuePresentFlag[chromac])
        {
          xWriteCode(sei.m_chromaKeyValue[chromac], 8, "gfv_chroma_key_value[chromac]");
        }
      }
      for (uint32_t chromai = 0; chromai < 2; chromai++)
      {
        xWriteFlag(sei.m_chromaKeyThrPresentFlag[chromai], "gfv_chroma_key_thr_present_flag[i]");
        if (sei.m_chromaKeyThrPresentFlag[chromai])
        {
          xWriteUvlc(sei.m_chromaKeyThrValue[chromai], "gfv_chroma_key_thr_value[i]");
        }
      }
    }
  }
  else
  {
    xWriteFlag(sei.m_drivePicFusionFlag, "gfv_drive_picture_fusion_flag");
  }
  xWriteFlag(sei.m_lowConfidenceFaceParameterFlag, "gfv_low_confidence_face_parameter_flag");
  xWriteFlag(sei.m_coordinatePresentFlag, "gfv_coordinate_present_flag");
  if (sei.m_coordinatePresentFlag)
  {
    xWriteFlag(sei.m_coordinatePredFlag, "gfv_kps_pred_flag");
    if (basePicFlag || !sei.m_coordinatePredFlag)
    {
      uint32_t gfvCoordinatePrecisionFactorMinus1 = sei.m_coordinateQuantizationFactor - 1;
      CHECK(gfvCoordinatePrecisionFactorMinus1 < 0 || gfvCoordinatePrecisionFactorMinus1 > 31,"The value of gfv_coordinate_precision_factor_minus1 shall be in the range of 0 to 31, inclusive");
      xWriteUvlc(gfvCoordinatePrecisionFactorMinus1, "gfv_coordinate_precision_factor_minus1");
      uint32_t gfvNumKpsMinus1 = sei.m_coordinatePointNum - 1;
      xWriteUvlc(gfvNumKpsMinus1, "gfv_num_kps_minus1");
      xWriteFlag(sei.m_3DCoordinateFlag, "gfv_coordinate_z_present_flag");
      if (sei.m_3DCoordinateFlag == 1)
      {
        uint32_t gfvCoordinateZMaxValueMinus1 = sei.m_coordinateZMaxValue - 1;
        CHECK(gfvCoordinateZMaxValueMinus1 < 0 || gfvCoordinateZMaxValueMinus1 > (1<<16) - 1,"The value of gfv_coordinate_z_max_value_minus1 shall be in the range of 0 to 2^(16) - 1, inclusive");
        xWriteUvlc(gfvCoordinateZMaxValueMinus1, "gfv_coordinate_z_max_value_minus1");
      }
    }
    //X_coordinate_tensor && Y_coordinate_tensor  && Z_coordinate_tensor
    for (uint32_t i = 0; i < sei.m_coordinatePointNum; i++)
    {
      if (!sei.m_coordinatePredFlag)
      {
        // X_coordinate_tensor
        int curCoordinateXInt = (int)(sei.m_coordinateX[i] * (1 << sei.m_coordinateQuantizationFactor) + 0.5);
        int curCoordinateXIntAbs = abs(curCoordinateXInt);
        xWriteUvlc(curCoordinateXIntAbs, "gfv_coordinate_x_abs[ i ]");
        if (curCoordinateXIntAbs)
        {
          const int signflag = curCoordinateXInt <= 0;
          xWriteFlag(signflag, "gfv_coordinate_x_sign_flag[ i ]");
        }
        double coordinateXTensorAbsRec = ((double)curCoordinateXInt) / (1 << sei.m_coordinateQuantizationFactor);
        coordinateXRec.push_back(coordinateXTensorAbsRec);
        // Y_coordinate_tensor
        int curCoordinateYInt = (int)(sei.m_coordinateY[i] * (1 << sei.m_coordinateQuantizationFactor) + 0.5);
        int curCoordinateYIntAbs = abs(curCoordinateYInt);
        xWriteUvlc(curCoordinateYIntAbs, "gfv_coordinate_y_abs[ i ]");
        if (curCoordinateYIntAbs)
        {
          const int signflag = curCoordinateYInt <= 0;
          xWriteFlag(signflag, "gfv_coordinate_y_sign_flag[ i ]");
        }
        double coordinateYTensorAbsRec = ((double)curCoordinateYInt) / (1 << sei.m_coordinateQuantizationFactor);
        coordinateYRec.push_back(coordinateYTensorAbsRec);
        // Z_coordinate_tensor
        if (sei.m_3DCoordinateFlag == 1)
        {
          int curCoordinateZInt = (int)(sei.m_coordinateZ[i] * (1 << sei.m_coordinateQuantizationFactor) + 0.5);
          int curCoordinateZIntAbs = abs(curCoordinateZInt);
          xWriteUvlc(curCoordinateZIntAbs, "gfv_coordinate_z_abs[ i ]");
          if (curCoordinateZIntAbs)
          {
            const int signflag = curCoordinateZInt <= 0;
            xWriteFlag(signflag, "gfv_coordinate_z_sign_flag[ i ]");
          }
          double coordinateZTensorAbsRec = (((double)curCoordinateZInt / (1 << sei.m_coordinateQuantizationFactor)));
          coordinateZRec.push_back(coordinateZTensorAbsRec);
        }
      }
      else
      {
        //Inter-frame difference
        int curCoordinateXInt = (int)((sei.m_coordinateX[i] - (basePicFlag ? (i == 0 ? 0 : coordinateXRec[i - 1]) : (sei.m_cnt ==0 ? baseCoordinateXRec[i] : prevcoordinateXRec[i]))) * (1 << sei.m_coordinateQuantizationFactor) + 0.5);
        double coordinateXTensorAbsRec = ((double)curCoordinateXInt) / (1 << sei.m_coordinateQuantizationFactor);
        coordinateXRec.push_back(coordinateXTensorAbsRec + (basePicFlag ? (i == 0 ? 0 : coordinateXRec[i - 1]) : (sei.m_cnt == 0 ? baseCoordinateXRec[i] : prevcoordinateXRec[i])));
        int curCoordinateXIntAbs = abs(curCoordinateXInt);
        xWriteUvlc(curCoordinateXIntAbs, "gfv_coordinate_dx_abs[ i ]");
        if (curCoordinateXIntAbs)
        {
          const int signflag = curCoordinateXInt <= 0;
          xWriteFlag(signflag, "gfv_coordinate_dx_sign_flag[ i ]");
        }
        int curCoordinateYInt = (int)((sei.m_coordinateY[i] - (basePicFlag ? (i == 0 ? 0 : coordinateYRec[i - 1]) : (sei.m_cnt == 0 ? baseCoordinateYRec[i] : prevcoordinateYRec[i]))) * (1 << sei.m_coordinateQuantizationFactor) + 0.5);
        double coordinateYTensorAbsRec = ((double)curCoordinateYInt) / (1 << sei.m_coordinateQuantizationFactor);
        coordinateYRec.push_back(coordinateYTensorAbsRec + (basePicFlag ? (i == 0 ? 0 : coordinateYRec[i - 1]) : (sei.m_cnt == 0 ? baseCoordinateYRec[i] : prevcoordinateYRec[i])));
        int curCoordinateYIntAbs = abs(curCoordinateYInt);
        xWriteUvlc(curCoordinateYIntAbs, "gfv_coordinate_dy_abs[ i ]");
        if (curCoordinateYIntAbs)
        {
          const int signflag = curCoordinateYInt <= 0;
          xWriteFlag(signflag, "gfv_coordinate_dy_sign_flag[ i ]");
        }
        if (sei.m_3DCoordinateFlag == 1)
        {
          int curCoordinateZInt = (int)((sei.m_coordinateZ[i] - (basePicFlag ? (i == 0 ? 0 : coordinateZRec[i - 1]) : (sei.m_cnt == 0 ? baseCoordinateZRec[i] : prevcoordinateZRec[i]))) * (1 << sei.m_coordinateQuantizationFactor) + 0.5);
          double coordinateZTensorAbsRec = ((double)curCoordinateZInt / (1 << sei.m_coordinateQuantizationFactor));
          coordinateZRec.push_back(coordinateZTensorAbsRec + (basePicFlag ? (i == 0 ? 0 : coordinateZRec[i - 1]) : (sei.m_cnt == 0 ? baseCoordinateZRec[i] : prevcoordinateZRec[i])));
          int curCoordinateZIntAbs = abs(curCoordinateZInt);
          xWriteUvlc(curCoordinateZIntAbs, "gfv_coordinate_dz_abs[ i ]");
          if (curCoordinateZIntAbs)
          {
            const int signflag = curCoordinateZInt <= 0;
            xWriteFlag(signflag, "gfv_coordinate_dz_sign_flag[ i ]");
          }
        }
      }
    }
    if (doUpdateGFVcoordinate)
    {
      prevcoordinateXRec = coordinateXRec;
      prevcoordinateYRec = coordinateYRec;
      if (sei.m_3DCoordinateFlag == 1)
      {
        prevcoordinateZRec = coordinateZRec;
      }
      if (sei.m_basePicFlag)
      {
        baseCoordinateXRec = coordinateXRec;
        baseCoordinateYRec = coordinateYRec;
        if (sei.m_3DCoordinateFlag == 1)
        {
          baseCoordinateZRec = coordinateZRec;
        }
      }
      doUpdateGFVcoordinate = false;
    }
    else
    {
      doUpdateGFVcoordinate = true;
    }
  }
  // Matrix Parameters
  CHECK((!sei.m_coordinatePresentFlag) && (!sei.m_matrixPresentFlag), "When gfv_coordinate_present_flag is equal to 0, gfv_matrix_present_flag shall be equal to 1");
  xWriteFlag(sei.m_matrixPresentFlag, "gfv_matrix_present_flag");

  if (sei.m_matrixPresentFlag)
  {
    std::vector<uint32_t> matrixWidthVec;
    std::vector<uint32_t> matrixHeightVec;
    std::vector<uint32_t> numMatricesVec;
    if (!basePicFlag )
    {
      xWriteFlag(sei.m_matrixPredFlag, "gfv_matrix_pred_flag");
    }
    if ( !sei.m_matrixPredFlag)
    {
      uint32_t gfvMatrixElementPrecisionFactorMinus1 = sei.m_matrixElementPrecisionFactor - 1;
      CHECK(gfvMatrixElementPrecisionFactorMinus1 < 0 || gfvMatrixElementPrecisionFactorMinus1 > 31,"The value of gfv_matrix_element_precision_factor_minus1 shall be in the range of 0 to 31, inclusive");
      xWriteUvlc(gfvMatrixElementPrecisionFactorMinus1, "gfv_matrix_element_precision_factor_minus1");
      uint32_t gfvNumMatrixTypesMinus1=sei.m_numMatrixType - 1;
      xWriteUvlc(gfvNumMatrixTypesMinus1, "gfv_num_matrix_types_minus1");

      for (uint32_t matrixId = 0; matrixId < sei.m_numMatrixType; matrixId++)
      {
        CHECK(sei.m_matrixTypeIdx[matrixId] < 0 || sei.m_matrixTypeIdx[matrixId] > 63, "The value of gfv_matrix_type_idx shall be in the range of 0 to 63, inclusive");
        xWriteCode(sei.m_matrixTypeIdx[matrixId], 6, "gfv_matrix_type_idx");
        if (sei.m_matrixTypeIdx[matrixId] == 0 || sei.m_matrixTypeIdx[matrixId] == 1)
        {
          CHECK(sei.m_coordinatePresentFlag == 0, "coordinatePresentFlag shall be 1 when matrix type is 0 or 1");
          xWriteFlag(sei.m_numMatricestonumKpsFlag[matrixId], "gfv_num_matrices_equal_to_num_kps_flag");
          if (!sei.m_numMatricestonumKpsFlag[matrixId])
          {
            CHECK(sei.m_numMatricesInfo[matrixId] < 0 || sei.m_numMatricesInfo[matrixId] > (1 << 10) - 1, "The value of gfv_num_matrices_info shall be in the range of 0 to 63, inclusive");
            xWriteUvlc(sei.m_numMatricesInfo[matrixId], "gfv_num_matrices_info");
          }
        }
        else if (sei.m_matrixTypeIdx[matrixId] == 2 || sei.m_matrixTypeIdx[matrixId] == 3 || sei.m_matrixTypeIdx[matrixId] >= 7)
        {
          if (sei.m_matrixTypeIdx[matrixId] >= 7)
          {
            uint32_t gfvNumMatricesMinus1 = sei.m_numMatrices[matrixId] - 1;
            CHECK(gfvNumMatricesMinus1 < 0 || gfvNumMatricesMinus1 >(1 << 10) - 1, "The value of gfv_num_matrices_minus1 shall be in the range of 0 to 2^(10) - 1, inclusive");
            xWriteUvlc(gfvNumMatricesMinus1, "gfv_num_matrices_minus1");
          }
          uint32_t gfvMatrixWidthMinus1 = sei.m_matrixWidth[matrixId] - 1;
          xWriteUvlc(gfvMatrixWidthMinus1, "gfv_matrix_width_minus1");
          CHECK(gfvMatrixWidthMinus1 < 0 || gfvMatrixWidthMinus1 >(1 << 10) - 1, "The value of gfv_matrix_width_minus1 shall be in the range of 0 to 2^(10) - 1, inclusive");
          uint32_t gfvMatrixHeightMinus1 = sei.m_matrixHeight[matrixId] - 1;
          xWriteUvlc(gfvMatrixHeightMinus1, "gfv_matrix_height_minus1");
          CHECK(gfvMatrixHeightMinus1 < 0 || gfvMatrixHeightMinus1 >(1 << 10) - 1, "The value of gfv_matrix_height_minus1 shall be in the range of 0 to 2^(10) - 1, inclusive");
        }
        else if (sei.m_matrixTypeIdx[matrixId] >= 4 && sei.m_matrixTypeIdx[matrixId] <= 6)
        {
          if (!sei.m_coordinatePresentFlag)
          {
            xWriteFlag(sei.m_matrix3DSpaceFlag[matrixId], "gfv_Matrix3DSpaceFlag");
          }
        }
      }
    }
    if (sei.m_matrixPredFlag)
    {
      numMatricesVec = baseNumMatricesVec;
      matrixHeightVec = baseMatrixHeightVec;
      matrixWidthVec = baseMatrixWidthVec;
    }
    else
    {
      numMatricesVec=sei.m_numMatricesstore;
      matrixHeightVec=sei.m_matrixHeightstore;
      matrixWidthVec=sei.m_matrixWidthstore;
    }
    for (uint32_t matrixId = 0; matrixId < sei.m_numMatrixType; matrixId++)
    {
      matrixElementRec.push_back(std::vector<std::vector<std::vector<double>>>());
      for (uint32_t j = 0; j < numMatricesVec[matrixId]; j++)
      {
        matrixElementRec[matrixId].push_back(std::vector<std::vector<double>>());
        for (uint32_t k = 0; k < matrixHeightVec[matrixId]; k++)
        {
          matrixElementRec[matrixId][j].push_back(std::vector<double>());
          for (uint32_t l = 0; l < matrixWidthVec[matrixId]; l++)
          {
            if (!sei.m_matrixPredFlag)
            {
              double curMatrixElementAbs = fabs(sei.m_matrixElement[matrixId][j][k][l]);
              int curMatrixElementAbsInt = (int)(curMatrixElementAbs);
              xWriteUvlc(curMatrixElementAbsInt, "gfv_matrix_element_int");
              double curMatrixElementAbsDecimal = curMatrixElementAbs - curMatrixElementAbsInt;
              CHECK(curMatrixElementAbsDecimal < 0, "");
              int curMatrixElementAbsDecIntValue = Clip3(0, (1 << sei.m_matrixElementPrecisionFactor) - 1, (int)(curMatrixElementAbsDecimal * (1 << sei.m_matrixElementPrecisionFactor) + 0.5));
              xWriteCode(curMatrixElementAbsDecIntValue, sei.m_matrixElementPrecisionFactor, "gfv_matrix_element_dec");
              const int signflag = sei.m_matrixElement[matrixId][j][k][l] < 0;
              if (curMatrixElementAbsInt || curMatrixElementAbsDecIntValue)
              {
                xWriteFlag(signflag, "gfv_matrix_element_sign_flag");
              }
              double matrixElementAbsRec = (double)(curMatrixElementAbsInt + (((double)curMatrixElementAbsDecIntValue) / (1 << sei.m_matrixElementPrecisionFactor)));
              matrixElementRec[matrixId][j][k].push_back(signflag ? -matrixElementAbsRec : matrixElementAbsRec);
            
            }
            else
            {
              double curMatrixElementAbs = fabs(sei.m_matrixElement[matrixId][j][k][l] - (sei.m_cnt == 0 ? baseMatrixRec[matrixId][j][k][l] : prevMatrixRec[matrixId][j][k][l]));
              int curMatrixElementAbsInt = (int)curMatrixElementAbs;
              xWriteUvlc(curMatrixElementAbsInt, "gfv_matrix_delta_element_int");
              double curMatrixElementAbsDecimal = curMatrixElementAbs - curMatrixElementAbsInt;
              CHECK(curMatrixElementAbsDecimal < 0, "");
              int curMatrixElementAbsDecIntValue = (int)(curMatrixElementAbsDecimal* (1 << sei.m_matrixElementPrecisionFactor) + 0.5);
              xWriteUvlc(curMatrixElementAbsDecIntValue, "gfv_matrix_element_dec");
              const int signflag = (sei.m_matrixElement[matrixId][j][k][l] - (sei.m_cnt == 0 ? baseMatrixRec[matrixId][j][k][l] : prevMatrixRec[matrixId][j][k][l])) < 0;
              if (curMatrixElementAbsInt || curMatrixElementAbsDecIntValue)
              {
                xWriteFlag(signflag, "gfv_matrix_delta_element_sign_flag");
              }
              double matrixElementAbsRec = (double)(curMatrixElementAbsInt + (((double)curMatrixElementAbsDecIntValue) / (1 << sei.m_matrixElementPrecisionFactor)));
              matrixElementRec[matrixId][j][k].push_back((signflag ? -matrixElementAbsRec : matrixElementAbsRec) + (sei.m_cnt == 0 ? baseMatrixRec[matrixId][j][k][l] : prevMatrixRec[matrixId][j][k][l]));
            }
          }
        }
      }
    }
    if (doUpdateGFVmatrix)
    {
      prevMatrixRec = matrixElementRec;
      if (basePicFlag)
      {
        baseMatrixRec = matrixElementRec;
        baseNumMatricesVec = numMatricesVec;
        baseMatrixHeightVec = matrixHeightVec;
        baseMatrixWidthVec = matrixWidthVec;
      }
      doUpdateGFVmatrix = false;
    }
    else
    {
      doUpdateGFVmatrix = true;
    }
  }
  if (sei.m_nnPresentFlag)
  {
    if (sei.m_nnModeIdc == 0)
    {
      while (!isByteAligned())
      {
        xWriteFlag(0, "gfv_reserved_zero_bit_b");
      }
      for (long i = 0; i < sei.m_payloadLength; i++)
      {
        xWriteSCode(sei.m_payloadByte[i], 8, "gfv_nn_payload_byte[i]");
      }
    }
  }
}
void SEIWriter::xWriteSEIGenerativeFaceVideoEnhancement(const SEIGenerativeFaceVideoEnhancement &sei)
{
  uint32_t basePicFlag = 0;
  xWriteUvlc(sei.m_id, "gfve_id");
  xWriteUvlc(sei.m_gfvid, "gfve_gfv_id");
  xWriteUvlc(sei.m_gfvcnt, "gfve_gfv_cnt");

  if (sei.m_gfvcnt == 0)
  {
    xWriteFlag(sei.m_basePicFlag, "gfve_base_picture_flag");
    basePicFlag = sei.m_basePicFlag;
  }
  else
  {
    basePicFlag = 0;
  }
  if (basePicFlag)
  {
    xWriteFlag(sei.m_nnPresentFlag, "gfve_nnPresentFlag");
    if (sei.m_nnPresentFlag)
    {
      xWriteUvlc(sei.m_nnModeIdc, "gfve_mode_idc");
      if (sei.m_nnModeIdc == 1)
      {
        while (!isByteAligned())
        {
          xWriteFlag(0, "gfve_nn_alignment_zero_bit_a");
        }
        xWriteString(sei.m_nnTagURI, "gfve_uri_tag");
        xWriteString(sei.m_nnURI, "gfve_uri");
      }
    }
  }
  xWriteFlag(sei.m_matrixPresentFlag, "gfve_matrix_present_flag");
  if (sei.m_matrixPresentFlag)
  {
    std::vector<std::vector<std::vector<double>>>   gfveMatrixElementRec;
    uint32_t numMatrices = 0;
    uint32_t   matrixElementPrecisionFactor = 0;
    std::vector<uint32_t> matrixHeightVec;
    std::vector<uint32_t> matrixWidthVec;
    if (!basePicFlag )
    {
      xWriteFlag(sei.m_matrixPredFlag, "gfve_matrix_pred_flag");
    }
    if(!sei.m_matrixPredFlag)
    {
      uint32_t gfveMatrixElementPrecisionFactorMinus1 = sei.m_matrixElementPrecisionFactor - 1;
      CHECK(gfveMatrixElementPrecisionFactorMinus1 < 0 || gfveMatrixElementPrecisionFactorMinus1 > 31,"The value of gfve_matrix_element_precision_factor_minus1 shall be in the range of 0 to 31, inclusive");
      xWriteUvlc(gfveMatrixElementPrecisionFactorMinus1, "gfve_matrix_element_precision_factor_minus1");
      uint32_t gfveNumMatricesMinus1 = sei.m_numMatrices - 1;
      CHECK(gfveNumMatricesMinus1 < 0 || gfveNumMatricesMinus1 >(1 << 10) - 1, "The value of gfv_num_matrices_minus1 shall be in the range of 0 to 2^(10) - 1, inclusive");
      xWriteUvlc(gfveNumMatricesMinus1, "gfve_num_matrices_minus1");
      numMatrices = gfveNumMatricesMinus1 + 1;
      matrixElementPrecisionFactor = gfveMatrixElementPrecisionFactorMinus1 + 1;
      if (basePicFlag)
      {
        baseGfveNumMatrices= gfveNumMatricesMinus1+1;
        baseMatrixElementPrecisionFactor= gfveMatrixElementPrecisionFactorMinus1+1;
      }
      for (uint32_t j = 0; j <= gfveNumMatricesMinus1; j++)
      {
        uint32_t gfveMatrixHeightMinus1 = sei.m_matrixHeight[j] - 1;
        xWriteUvlc(gfveMatrixHeightMinus1, "gfve_matrix_height_minus1");
        uint32_t gfveMatrixWidthMinus1 = sei.m_matrixWidth[j] - 1;
        xWriteUvlc(gfveMatrixWidthMinus1, "gfve_matrix_width_minus1");
        matrixHeightVec.push_back(sei.m_matrixHeight[j]);
        matrixWidthVec.push_back(sei.m_matrixWidth[j]);
        if (basePicFlag && doUpdateGFVEmatrix)
        {
          baseGfveMatrixHeightVec.push_back(sei.m_matrixHeight[j]);
          baseGfveMatrixWidthVec.push_back(sei.m_matrixWidth[j]);
        }
      }
    }
    else
    {
      numMatrices = baseGfveNumMatrices;
      matrixElementPrecisionFactor = baseMatrixElementPrecisionFactor;
      matrixHeightVec = baseGfveMatrixHeightVec;
      matrixWidthVec = baseGfveMatrixWidthVec;
    }
    for (uint32_t j = 0; j < numMatrices; j++)
    {
      gfveMatrixElementRec.push_back(std::vector<std::vector<double>>());
      for (uint32_t k = 0; k < matrixHeightVec[j]; k++)
      {
        gfveMatrixElementRec[j].push_back(std::vector<double>());
        for (uint32_t l = 0; l < matrixWidthVec[j]; l++)
        {
          if(!sei.m_matrixPredFlag)
          {
            double curMatrixElementAbs = fabs(sei.m_matrixElement[j][k][l]);
            uint32_t curMatrixElementAbsInt = (int)(curMatrixElementAbs);
            CHECK(curMatrixElementAbsInt < 0 || curMatrixElementAbsInt > 4294967296 - 2, "The value of gfve_matrix_element_int shall be in the range of 0 to 2^(32) - 2, inclusive");
            xWriteUvlc(curMatrixElementAbsInt, "gfve_matrix_element_int");
            double curMatrixElementAbsDecimal = curMatrixElementAbs - curMatrixElementAbsInt;
            CHECK(curMatrixElementAbsDecimal < 0, "");
            int curMatrixElementAbsDecIntValue = Clip3(0, (1 << matrixElementPrecisionFactor) - 1, (int)(curMatrixElementAbsDecimal * (1 << matrixElementPrecisionFactor) + 0.5));
            xWriteCode(curMatrixElementAbsDecIntValue, matrixElementPrecisionFactor, "gfve_matrix_element_dec");
            const int signflag = sei.m_matrixElement[j][k][l] < 0;
            if (curMatrixElementAbsInt || curMatrixElementAbsDecIntValue)
            {
              xWriteFlag(signflag, "gfve_matrix_element_sign_flag");
            }
            double matrixElementAbsRec = (double)(curMatrixElementAbsInt + (((double)curMatrixElementAbsDecIntValue) / (1 << matrixElementPrecisionFactor)));
            gfveMatrixElementRec[j][k].push_back(signflag ? -matrixElementAbsRec : matrixElementAbsRec);
          }
          else
          {
            double curMatrixElementAbs = fabs(sei.m_matrixElement[j][k][l] - (sei.m_gfvcnt==0 ? baseGfveMatrixRec[j][k][l] : prevGfveMatrixRec[j][k][l]));
            uint32_t curMatrixElementAbsInt = (int)curMatrixElementAbs;
            CHECK(curMatrixElementAbsInt < 0 || curMatrixElementAbsInt > 4294967296 - 2, "The value of gfve_matrix_element_int shall be in the range of 0 to 2^(32) - 2, inclusive");
            xWriteUvlc(curMatrixElementAbsInt, "gfve_matrix_delta_element_int");
            double curMatrixElementAbsDecimal = curMatrixElementAbs - curMatrixElementAbsInt;
            CHECK(curMatrixElementAbsDecimal < 0, "");
            int curMatrixElementAbsDecIntValue = (int)(curMatrixElementAbsDecimal* (1 << matrixElementPrecisionFactor) + 0.5);
            xWriteCode(curMatrixElementAbsDecIntValue, matrixElementPrecisionFactor, "gfve_matrix_delta_element_dec");
            const int signflag = (sei.m_matrixElement[j][k][l] - (sei.m_gfvcnt == 0 ? baseGfveMatrixRec[j][k][l] : prevGfveMatrixRec[j][k][l])) < 0;
            if (curMatrixElementAbsInt || curMatrixElementAbsDecIntValue)
            {
              xWriteFlag(signflag, "gfve_matrix_delta_element_sign_flag");
            }
            double matrixElementAbsRec = (double)(curMatrixElementAbsInt + (((double)curMatrixElementAbsDecIntValue) / (1 << baseMatrixElementPrecisionFactor)));
            gfveMatrixElementRec[j][k].push_back((signflag ? -matrixElementAbsRec : matrixElementAbsRec) + (sei.m_gfvcnt == 0 ? baseGfveMatrixRec[j][k][l] : prevGfveMatrixRec[j][k][l]));
          }
        }
      }
    }
    if (doUpdateGFVEmatrix)
    {
      prevGfveMatrixRec = gfveMatrixElementRec;
      if (basePicFlag)
      {
        baseGfveMatrixRec = gfveMatrixElementRec;
      }
      doUpdateGFVEmatrix = false;
    }
    else
    {
      doUpdateGFVEmatrix = true;
    }
  }
  double gfveLeftPupilCoordinateXRec;
  double gfveLeftPupilCoordinateYRec;
  double gfveRightPupilCoordinateXRec;
  double gfveRightPupilCoordinateYRec;
  CHECK(sei.m_pupilPresentIdx < 0 || sei.m_pupilPresentIdx > 3, "The possible values of gfve_pupil_coordinate_present_idx are 0, 1, 2, and 3");
  xWriteCode(sei.m_pupilPresentIdx, 2, "gfve_pupil_coordinate_present_idx");
  if (sei.m_pupilPresentIdx != 0)
  {
    if (basePicFlag)
    {
      checkBasePicPupilPresentIdx = true;
      uint32_t gfvePupilCoordinatePrecisionFactorMinus1 = sei.m_pupilCoordinatePrecisionFactor - 1;
      CHECK(gfvePupilCoordinatePrecisionFactorMinus1 < 0 || gfvePupilCoordinatePrecisionFactorMinus1 > 31, "The value of gfve_matrix_element_precision_factor_minus1 shall be in the range of 0 to 31, inclusive");
      xWriteUvlc(gfvePupilCoordinatePrecisionFactorMinus1, "gfve_pupil_coordinate_precision_factor_minus1");
    }
    CHECK(!checkBasePicPupilPresentIdx, "The gfve_pupil_coordinate_present _idx for the first frame shall not be 0");
  }
  if (checkBasePicPupilPresentIdx)
  {
    double gfveLeftPupilCoordinateXRef = 0.0;
    double gfveLeftPupilCoordinateYRef = 0.0;
    double gfveRightPupilCoordinateXRef = 0.0;
    double gfveRightPupilCoordinateYRef = 0.0;
    if (sei.m_gfvcnt==0)
    {
      if (!basePicFlag)
      {
        gfveLeftPupilCoordinateXRef  = basegfveLeftPupilCoordinateX;
        gfveLeftPupilCoordinateYRef  = basegfveLeftPupilCoordinateY;
        gfveRightPupilCoordinateXRef = basegfveRightPupilCoordinateX;
        gfveRightPupilCoordinateYRef = basegfveRightPupilCoordinateY;
      }
    }
    else
    {
      gfveLeftPupilCoordinateXRef  = prevgfveLeftPupilCoordinateX;
      gfveLeftPupilCoordinateYRef  = prevgfveLeftPupilCoordinateY;
      gfveRightPupilCoordinateXRef = prevgfveRightPupilCoordinateX;
      gfveRightPupilCoordinateYRef = prevgfveRightPupilCoordinateY;
    }
    if (sei.m_pupilPresentIdx == 1 || sei.m_pupilPresentIdx == 3)
    {
      gfveLeftPupilCoordinateXRec = xWriteSEIPupilCoordinate(sei.m_pupilLeftEyeCoordinateX, gfveLeftPupilCoordinateXRef, sei.m_pupilCoordinatePrecisionFactor, "left", "x");
      gfveLeftPupilCoordinateYRec = xWriteSEIPupilCoordinate(sei.m_pupilLeftEyeCoordinateY, gfveLeftPupilCoordinateYRef, sei.m_pupilCoordinatePrecisionFactor, "left", "y");
    }
    else
    {
      gfveLeftPupilCoordinateXRec = gfveLeftPupilCoordinateXRef;
      gfveLeftPupilCoordinateYRec = gfveLeftPupilCoordinateYRef;
    }
    if (basePicFlag)
    {
      gfveRightPupilCoordinateXRef = gfveLeftPupilCoordinateXRec;
      gfveRightPupilCoordinateYRef = gfveLeftPupilCoordinateYRec;
    }
    if (2 == sei.m_pupilPresentIdx || 3 == sei.m_pupilPresentIdx)
    {
      gfveRightPupilCoordinateXRec = xWriteSEIPupilCoordinate(sei.m_pupilRightEyeCoordinateX, gfveRightPupilCoordinateXRef, sei.m_pupilCoordinatePrecisionFactor, "right", "x");
      gfveRightPupilCoordinateYRec = xWriteSEIPupilCoordinate(sei.m_pupilRightEyeCoordinateY, gfveRightPupilCoordinateYRef, sei.m_pupilCoordinatePrecisionFactor, "right", "y");
    }
    else
    {
      gfveRightPupilCoordinateXRec = gfveRightPupilCoordinateXRef;
      gfveRightPupilCoordinateYRec = gfveRightPupilCoordinateYRef;
    }
    if (doUpdateGFVPupilCoordinate)
    {
      if (basePicFlag)
      {
        basegfveLeftPupilCoordinateX = gfveLeftPupilCoordinateXRec;
        basegfveLeftPupilCoordinateY = gfveLeftPupilCoordinateYRec;
        basegfveRightPupilCoordinateX = gfveRightPupilCoordinateXRec;
        basegfveRightPupilCoordinateY = gfveRightPupilCoordinateYRec;
      }
      prevgfveLeftPupilCoordinateX  = gfveLeftPupilCoordinateXRec;
      prevgfveLeftPupilCoordinateY  = gfveLeftPupilCoordinateYRec;
      prevgfveRightPupilCoordinateX = gfveRightPupilCoordinateXRec;
      prevgfveRightPupilCoordinateY = gfveRightPupilCoordinateYRec;
      doUpdateGFVPupilCoordinate    = false;
    }
    else
    {
      doUpdateGFVPupilCoordinate = true;
    }
  }
  if (sei.m_nnPresentFlag)
  {
    if (sei.m_nnModeIdc == 0)
    {
      while (!isByteAligned())
      {
        xWriteFlag(0, "gfve_nn_alignment_zero_bit_b");
      }
      for (long i = 0; i < sei.m_payloadLength; i++)
      {
        xWriteSCode(sei.m_payloadByte[i], 8, "gfve_nn_payload_byte[i]");
      }
    }
  }
}
double SEIWriter::xWriteSEIPupilCoordinate(double coordinate, double refCoordinate, int precisionFactor, const char* eye, const char* axis)
{
  double deltaAbs = fabs(coordinate - refCoordinate);
  int absIntValue;
  absIntValue = static_cast<int>(deltaAbs * (1 << precisionFactor) + 0.5);

  CHECK(std::string(eye) != "left" && std::string(eye) != "right", "Invalid value for 'eye'. Allowed values are 'left' or 'right'.");
  CHECK(std::string(axis) != "x" && std::string(axis) != "y", "Invalid value for 'axis'. Allowed values are 'x' or 'y'.");
  std::string checkMessage = "The value of gfve_pupil_" + std::string(eye) + "_eye_d" + std::string(axis) + "_coordinate_abs shall be be 0 to 1 << (gfve_pupil_coordinate_precision_factor_minus1 + 2), inclusive";
  CHECK(absIntValue < 0 || absIntValue >(1 << (precisionFactor + 1)), checkMessage.c_str());

  std::string absSymbolName = "gfve_pupil_" + std::string(eye) + "_eye_d" + std::string(axis) + "_coordinate_abs";
  xWriteUvlc(absIntValue, absSymbolName.c_str());

  const int signFlag = (coordinate - refCoordinate < 0) ? 1 : 0;
  if (absIntValue)
  {
    std::string signSymbolName = "gfve_pupil_" + std::string(eye) + "_eye_d" + std::string(axis) + "_coordinate_sign_flag";
    xWriteFlag(signFlag, signSymbolName.c_str());
  }

  double deltaAbsRec = static_cast<double>(absIntValue) / (1 << precisionFactor);
  return (signFlag ? -deltaAbsRec : deltaAbsRec) + refCoordinate;
}


#if JVET_AJ0151_DSC_SEI
void SEIWriter::xWriteSEIDigitallySignedContentInitialization(const SEIDigitallySignedContentInitialization &sei)
{
  xWriteCode(sei.dsciHashMethodType, 8, "dsci_hash_method_type");
  xWriteString(sei.dsciKeySourceUri, "dsci_key_source_uri");
  CHECK (sei.dsciNumVerificationSubstreams < 1, "Number of DSC verification substreams has to be greater than zero");
  xWriteUvlc(sei.dsciNumVerificationSubstreams - 1, "dsci_num_verification_substreams_minus1");
#if  JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  for (int i = 1; i < sei.dsciNumVerificationSubstreams; i++)
  {
    for (int j = 0; j < i; j++)
    {
      xWriteFlag(sei.dsciRefSubstreamFlag[i][j], "dsci_ref_substream_flag");
    }
  }
#endif
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
  xWriteFlag(sei.dsciVSSImplicitAssociationModeFlag, "dsci_vss_implicit_association_mode_flag");
#endif  
  xWriteUvlc(sei.dsciKeyRetrievalModeIdc, "dsci_key_retrieval_mode_idc");
  if (sei.dsciKeyRetrievalModeIdc == 1)
  {
    xWriteFlag(sei.dsciUseKeyRegisterIdxFlag, "dsci_use_key_register_idx_flag");
    if( sei.dsciUseKeyRegisterIdxFlag )
    {
      xWriteUvlc(sei.dsciKeyRegisterIdx, "dsci_key_register_idx");
    }
  }
  xWriteFlag(sei.dsciContentUuidPresentFlag, "dsci_content_uuid_present_flag");
  if (sei.dsciContentUuidPresentFlag)
  {
    for (int i=0; i<16; i++)
    {
      xWriteCode(sei.dsciContentUuid[i], 8, "dsci_content_uuid");
    }
  }
}

void SEIWriter::xWriteSEIDigitallySignedContentSelection(const SEIDigitallySignedContentSelection &sei)
{
  xWriteUvlc(sei.dscsVerificationSubstreamId, "dscs_verification_substream_id");
}

void SEIWriter::xWriteSEIDigitallySignedContentVerification(const SEIDigitallySignedContentVerification &sei)
{
  xWriteUvlc(sei.dscvVerificationSubstreamId, "dscv_verification_substream_id");
  CHECK (sei.dscvSignatureLengthInOctets < 1, "Length of signature has to be greater than zero");
  xWriteUvlc(sei.dscvSignatureLengthInOctets - 1, "dscv_signature_length_in_octets_minus1");
  CHECK (sei.dscvSignatureLengthInOctets != sei.dscvSignature.size(), "Signature length incosistent");
  for (int i=0; i< sei.dscvSignature.size(); i++)
  {
    xWriteCode(sei.dscvSignature[i], 8, "dscv_signature");
  }
}
#endif

#if JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
void SEIWriter::xWriteSEIAIUsageRestrictions(const SEIAIUsageRestrictions &sei)
{
  xWriteFlag(sei.m_cancelFlag, "aur_cancel_flag");
  if (!sei.m_cancelFlag)
  {
    xWriteFlag(sei.m_persistenceFlag, "aur_persistence_flag");
    xWriteUvlc(sei.m_numRestrictionsMinus1, "aur_num_restrictions_minus1");
    for (uint32_t i = 0; i <= sei.m_numRestrictionsMinus1; i++)
    {
      xWriteUvlc(sei.m_restrictions[i], "aur_restriction");
      xWriteFlag(sei.m_contextPresentFlag[i], "aur_context_present_flag");
      if (sei.m_contextPresentFlag[i])
      {
        xWriteUvlc(sei.m_context[i], "aur_context");
      }
    }
  }
}
#endif

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
void SEIWriter::xWriteSEIPackedRegionsInfo(const SEIPackedRegionsInfo& sei)
{
  xWriteFlag(sei.m_cancelFlag, "pri_cancel_flag");
  if (!sei.m_cancelFlag)
  {
    xWriteFlag(sei.m_persistenceFlag, "pri_persistence_flag");
    xWriteUvlc(sei.m_numRegionsMinus1, "pri_num_regions_minus1");
    xWriteFlag(sei.m_multilayerFlag, "pri_multilayer_flag");
    xWriteFlag(sei.m_useMaxDimensionsFlag, "pri_use_max_dimensions_flag");
    xWriteCode(sei.m_log2UnitSize, 4, "pri_log2_unit_size");
    xWriteCode(sei.m_regionSizeLenMinus1, 4, "pri_region_size_len_minus1");
    xWriteFlag(sei.m_regionIdPresentFlag, "pri_region_id_present_flag");
    xWriteFlag(sei.m_targetPicParamsPresentFlag, "pri_target_pic_params_present_flag");
    if (sei.m_targetPicParamsPresentFlag)
    {
      xWriteCode(sei.m_targetPicWidthMinus1, 16, "pri_target_pic_width_minus1");
      xWriteCode(sei.m_targetPicHeightMinus1, 16, "pri_target_pic_height_minus1");
    }
    xWriteUvlc(sei.m_numResamplingRatiosMinus1, "pri_num_resampling_ratios_minus1");
    for (uint32_t i = 1; i <= sei.m_numResamplingRatiosMinus1; i++)
    {
      xWriteUvlc(sei.m_resamplingWidthNumMinus1[i], "pri_resampling_width_num_minus1[i]");
      xWriteUvlc(sei.m_resamplingWidthDenomMinus1[i], "pri_resampling_width_denom_minus1[i]");
      xWriteFlag(sei.m_fixedAspectRatioFlag[i], "pri_fixed_aspect_ratio_flag[i]");
      if (!sei.m_fixedAspectRatioFlag[i])
      {
        xWriteUvlc(sei.m_resamplingHeightNumMinus1[i], "pri_resampling_height_num_minus1[i]");
        xWriteUvlc(sei.m_resamplingHeightDenomMinus1[i], "pri_resampling_height_denom_minus1[i]");
      }
    }
    for (uint32_t i = 0; i <= sei.m_numRegionsMinus1; i++)
    {
      if (sei.m_regionIdPresentFlag)
      {
        xWriteUvlc(sei.m_regionId[i], "pri_region_id[i]");
      }
      if (sei.m_multilayerFlag)
      {
        xWriteUvlc(sei.m_regionLayerId[i], "pri_region_layer_id[i]");
        xWriteFlag(sei.m_regionIsALayerFlag[i], "pri_region_is_a_layer_flag[i]");
      }
      if (!sei.m_regionIsALayerFlag[i])
      {
        xWriteCode(sei.m_regionTopLeftInUnitsX[i], sei.m_regionSizeLenMinus1 + 1, "pri_region_top_left_in_units_x[i]");
        xWriteCode(sei.m_regionTopLeftInUnitsY[i], sei.m_regionSizeLenMinus1 + 1, "pri_region_top_left_in_units_y[i]");
        xWriteCode(sei.m_regionWidthInUnitsMinus1[i], sei.m_regionSizeLenMinus1 + 1, "pri_region_width_in_units_minus1[i]");
        xWriteCode(sei.m_regionHeightInUnitsMinus1[i], sei.m_regionSizeLenMinus1 + 1, "pri_region_height_in_units_minus1[i]");
      }
      if (sei.m_numResamplingRatiosMinus1 > 0)
      {
        xWriteCode(sei.m_resamplingRatioIdx[i], ceilLog2(sei.m_numResamplingRatiosMinus1 + 1), "pri_resampling_ratio_idx[i]");
      }
      if (sei.m_targetPicParamsPresentFlag)
      {
#if JVET_AL0324_AL0070_PRI_SEI
        xWriteCode(sei.m_targetRegionTopLeftInUnitsX[i], sei.m_regionSizeLenMinus1 + 1, "pri_target_region_top_left_in_units_x[i]");
        xWriteCode(sei.m_targetRegionTopLeftInUnitsY[i], sei.m_regionSizeLenMinus1 + 1, "pri_target_region_top_left_in_units_y[i]");
#else
        xWriteCode(sei.m_targetRegionTopLeftX[i], sei.m_regionSizeLenMinus1 + 1, "pri_target_region_top_left_x[i]");
        xWriteCode(sei.m_targetRegionTopLeftY[i], sei.m_regionSizeLenMinus1 + 1, "pri_target_region_top_left_y[i]");
#endif
      }
    }
  }
}
#endif

//! \}
