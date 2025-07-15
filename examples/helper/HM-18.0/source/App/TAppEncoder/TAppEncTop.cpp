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

/** \file     TAppEncTop.cpp
    \brief    Encoder application class
*/

#include <list>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <iomanip>

#include "TAppEncTop.h"
#include "TLibEncoder/TEncTemporalFilter.h"
#include "TLibEncoder/AnnexBwrite.h"

#if EXTENSION_360_VIDEO
#include "TAppEncHelper360/TExt360AppEncTop.h"
#endif

using namespace std;

//! \ingroup TAppEncoder
//! \{

// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================

TAppEncTop::TAppEncTop()
{
  m_iFrameRcvd = 0;
  m_totalBytes = 0;
  m_essentialBytes = 0;
}

TAppEncTop::~TAppEncTop()
{
}

Void TAppEncTop::xInitLibCfg()
{
  TComVPS vps;

  vps.setMaxTLayers                                               ( m_maxTempLayer );
  if (m_maxTempLayer == 1)
  {
    vps.setTemporalNestingFlag(true);
  }
  vps.setMaxLayers                                                ( 1 );
  for(Int i = 0; i < MAX_TLAYER; i++)
  {
    vps.setNumReorderPics                                         ( m_numReorderPics[i], i );
    vps.setMaxDecPicBuffering                                     ( m_maxDecPicBuffering[i], i );
  }
  m_cTEncTop.setVPS(&vps);

  m_cTEncTop.setProfile                                           ( m_profile);
  m_cTEncTop.setLevel                                             ( m_levelTier, m_level);
  m_cTEncTop.setProgressiveSourceFlag                             ( m_progressiveSourceFlag);
  m_cTEncTop.setInterlacedSourceFlag                              ( m_interlacedSourceFlag);
  m_cTEncTop.setNonPackedConstraintFlag                           ( m_nonPackedConstraintFlag);
  m_cTEncTop.setFrameOnlyConstraintFlag                           ( m_frameOnlyConstraintFlag);
  m_cTEncTop.setBitDepthConstraintValue                           ( m_bitDepthConstraint );
  m_cTEncTop.setChromaFormatConstraintValue                       ( m_chromaFormatConstraint );
  m_cTEncTop.setIntraConstraintFlag                               ( m_intraConstraintFlag );
  m_cTEncTop.setOnePictureOnlyConstraintFlag                      ( m_onePictureOnlyConstraintFlag );
  m_cTEncTop.setLowerBitRateConstraintFlag                        ( m_lowerBitRateConstraintFlag );

  m_cTEncTop.setPrintMSEBasedSequencePSNR                         ( m_printMSEBasedSequencePSNR);
  m_cTEncTop.setPrintHexPsnr                                      ( m_printHexPsnr);
  m_cTEncTop.setPrintFrameMSE                                     ( m_printFrameMSE);
  m_cTEncTop.setPrintSequenceMSE                                  ( m_printSequenceMSE);
  m_cTEncTop.setPrintMSSSIM                                       ( m_printMSSSIM );

  m_cTEncTop.setXPSNREnableFlag                                   ( m_bXPSNREnableFlag);
  for (Int id = 0 ; id < MAX_NUM_COMPONENT; id++)
  {
    m_cTEncTop.setXPSNRWeight                                     ( m_dXPSNRWeight[id], ComponentID(id));
  }

#if SHUTTER_INTERVAL_SEI_PROCESSING
  m_cTEncTop.setShutterFilterFlag                                 ( m_ShutterFilterEnable );
#endif

  m_cTEncTop.setCabacZeroWordPaddingEnabled                       ( m_cabacZeroWordPaddingEnabled );

  m_cTEncTop.setFrameRate                                         ( m_iFrameRate );
  m_cTEncTop.setFrameSkip                                         ( m_FrameSkip );
  m_cTEncTop.setTemporalSubsampleRatio                            ( m_temporalSubsampleRatio );
  m_cTEncTop.setSourceWidth                                       ( m_sourceWidth );
  m_cTEncTop.setSourceHeight                                      ( m_sourceHeight );
  m_cTEncTop.setConformanceWindow                                 ( m_confWinLeft, m_confWinRight, m_confWinTop, m_confWinBottom );
  m_cTEncTop.setFramesToBeEncoded                                 ( m_framesToBeEncoded );

  //====== Coding Structure ========
  m_cTEncTop.setIntraPeriod                                       ( m_iIntraPeriod );
  m_cTEncTop.setDecodingRefreshType                               ( m_iDecodingRefreshType );
  m_cTEncTop.setGOPSize                                           ( m_iGOPSize );
  m_cTEncTop.setReWriteParamSetsFlag                              ( m_bReWriteParamSetsFlag );
  m_cTEncTop.setGopList                                           ( m_GOPList );
  m_cTEncTop.setExtraRPSs                                         ( m_extraRPSs );
  for(Int i = 0; i < MAX_TLAYER; i++)
  {
    m_cTEncTop.setNumReorderPics                                  ( m_numReorderPics[i], i );
    m_cTEncTop.setMaxDecPicBuffering                              ( m_maxDecPicBuffering[i], i );
  }
  for( UInt uiLoop = 0; uiLoop < MAX_TLAYER; ++uiLoop )
  {
    m_cTEncTop.setLambdaModifier                                  ( uiLoop, m_adLambdaModifier[ uiLoop ] );
  }
  m_cTEncTop.setIntraLambdaModifier                               ( m_adIntraLambdaModifier );
  m_cTEncTop.setIntraQpFactor                                     ( m_dIntraQpFactor );

  m_cTEncTop.setQP                                                ( m_iQP );

  m_cTEncTop.setIntraQPOffset                                     ( m_intraQPOffset );
  m_cTEncTop.setLambdaFromQPEnable                                ( m_lambdaFromQPEnable );
  m_cTEncTop.setSourcePadding                                     ( m_sourcePadding );

  m_cTEncTop.setAccessUnitDelimiter                               ( m_AccessUnitDelimiter );

  m_cTEncTop.setMaxTempLayer                                      ( m_maxTempLayer );
  m_cTEncTop.setUseAMP( m_enableAMP );

  //===== Slice ========

  //====== Loop/Deblock Filter ========
  m_cTEncTop.setLoopFilterDisable                                 ( m_bLoopFilterDisable       );
  m_cTEncTop.setLoopFilterOffsetInPPS                             ( m_loopFilterOffsetInPPS );
  m_cTEncTop.setLoopFilterBetaOffset                              ( m_loopFilterBetaOffsetDiv2  );
  m_cTEncTop.setLoopFilterTcOffset                                ( m_loopFilterTcOffsetDiv2    );
  m_cTEncTop.setDeblockingFilterMetric                            ( m_deblockingFilterMetric );

  //====== Motion search ========
  m_cTEncTop.setDisableIntraPUsInInterSlices                      ( m_bDisableIntraPUsInInterSlices );
  m_cTEncTop.setMotionEstimationSearchMethod                      ( m_motionEstimationSearchMethod  );
  m_cTEncTop.setSearchRange                                       ( m_iSearchRange );
  m_cTEncTop.setBipredSearchRange                                 ( m_bipredSearchRange );
  m_cTEncTop.setClipForBiPredMeEnabled                            ( m_bClipForBiPredMeEnabled );
  m_cTEncTop.setFastMEAssumingSmootherMVEnabled                   ( m_bFastMEAssumingSmootherMVEnabled );
  m_cTEncTop.setMinSearchWindow                                   ( m_minSearchWindow );
  m_cTEncTop.setRestrictMESampling                                ( m_bRestrictMESampling );

  //====== Quality control ========
  m_cTEncTop.setMaxDeltaQP                                        ( m_iMaxDeltaQP  );
  m_cTEncTop.setMaxCuDQPDepth                                     ( m_iMaxCuDQPDepth  );
  m_cTEncTop.setDiffCuChromaQpOffsetDepth                         ( m_diffCuChromaQpOffsetDepth );
  m_cTEncTop.setChromaCbQpOffset                                  ( m_cbQpOffset     );
  m_cTEncTop.setChromaCrQpOffset                                  ( m_crQpOffset  );
  m_cTEncTop.setWCGChromaQpControl                                ( m_wcgChromaQpControl );
  m_cTEncTop.setSliceChromaOffsetQpIntraOrPeriodic                ( m_sliceChromaQpOffsetPeriodicity, m_sliceChromaQpOffsetIntraOrPeriodic );
  m_cTEncTop.setChromaFormatIdc                                   ( m_chromaFormatIDC  );

#if ADAPTIVE_QP_SELECTION
  m_cTEncTop.setUseAdaptQpSelect                                  ( m_bUseAdaptQpSelect   );
#endif
#if  JVET_V0078
  m_cTEncTop.setSmoothQPReductionEnable                           (m_bSmoothQPReductionEnable);
  m_cTEncTop.setSmoothQPReductionThreshold                        (m_dSmoothQPReductionThreshold);
  m_cTEncTop.setSmoothQPReductionModelScale                       (m_dSmoothQPReductionModelScale);
  m_cTEncTop.setSmoothQPReductionModelOffset                      (m_dSmoothQPReductionModelOffset);
  m_cTEncTop.setSmoothQPReductionLimit                            (m_iSmoothQPReductionLimit);
  m_cTEncTop.setSmoothQPReductionPeriodicity                      (m_iSmoothQPReductionPeriodicity);
#endif

  m_cTEncTop.setUseAdaptiveQP                                     ( m_bUseAdaptiveQP  );
  m_cTEncTop.setQPAdaptationRange                                 ( m_iQPAdaptationRange );
  m_cTEncTop.setExtendedPrecisionProcessingFlag                   ( m_extendedPrecisionProcessingFlag );
  m_cTEncTop.setHighPrecisionOffsetsEnabledFlag                   ( m_highPrecisionOffsetsEnabledFlag );

  m_cTEncTop.setWeightedPredictionMethod( m_weightedPredictionMethod );

  //====== Tool list ========
  m_cTEncTop.setLumaLevelToDeltaQPControls                        ( m_lumaLevelToDeltaQPMapping );
  m_cTEncTop.setDeltaQpRD( (m_costMode==COST_LOSSLESS_CODING) ? 0 : m_uiDeltaQpRD );
  m_cTEncTop.setFastDeltaQp                                       ( m_bFastDeltaQP  );
  m_cTEncTop.setUseASR                                            ( m_bUseASR      );
  m_cTEncTop.setUseHADME                                          ( m_bUseHADME    );
  m_cTEncTop.setdQPs                                              ( m_aidQP        );
  m_cTEncTop.setUseRDOQ                                           ( m_useRDOQ     );
  m_cTEncTop.setUseRDOQTS                                         ( m_useRDOQTS   );
  m_cTEncTop.setUseSelectiveRDOQ                                  ( m_useSelectiveRDOQ );
  m_cTEncTop.setRDpenalty                                         ( m_rdPenalty );
  m_cTEncTop.setMaxCUWidth                                        ( m_uiMaxCUWidth );
  m_cTEncTop.setMaxCUHeight                                       ( m_uiMaxCUHeight );
  m_cTEncTop.setMaxTotalCUDepth                                   ( m_uiMaxTotalCUDepth );
  m_cTEncTop.setLog2DiffMaxMinCodingBlockSize                     ( m_uiLog2DiffMaxMinCodingBlockSize );
  m_cTEncTop.setQuadtreeTULog2MaxSize                             ( m_uiQuadtreeTULog2MaxSize );
  m_cTEncTop.setQuadtreeTULog2MinSize                             ( m_uiQuadtreeTULog2MinSize );
  m_cTEncTop.setQuadtreeTUMaxDepthInter                           ( m_uiQuadtreeTUMaxDepthInter );
  m_cTEncTop.setQuadtreeTUMaxDepthIntra                           ( m_uiQuadtreeTUMaxDepthIntra );
  m_cTEncTop.setFastInterSearchMode                               ( m_fastInterSearchMode );
  m_cTEncTop.setUseEarlyCU                                        ( m_bUseEarlyCU  );
  m_cTEncTop.setUseFastDecisionForMerge                           ( m_useFastDecisionForMerge  );
  m_cTEncTop.setUseCbfFastMode                                    ( m_bUseCbfFastMode  );
  m_cTEncTop.setUseEarlySkipDetection                             ( m_useEarlySkipDetection );
  m_cTEncTop.setCrossComponentPredictionEnabledFlag               ( m_crossComponentPredictionEnabledFlag );
  m_cTEncTop.setUseReconBasedCrossCPredictionEstimate             ( m_reconBasedCrossCPredictionEstimate );
  m_cTEncTop.setLog2SaoOffsetScale                                ( CHANNEL_TYPE_LUMA  , m_log2SaoOffsetScale[CHANNEL_TYPE_LUMA]   );
  m_cTEncTop.setLog2SaoOffsetScale                                ( CHANNEL_TYPE_CHROMA, m_log2SaoOffsetScale[CHANNEL_TYPE_CHROMA] );
  m_cTEncTop.setUseTransformSkip                                  ( m_useTransformSkip      );
  m_cTEncTop.setUseTransformSkipFast                              ( m_useTransformSkipFast  );
  m_cTEncTop.setTransformSkipRotationEnabledFlag                  ( m_transformSkipRotationEnabledFlag );
  m_cTEncTop.setTransformSkipContextEnabledFlag                   ( m_transformSkipContextEnabledFlag   );
  m_cTEncTop.setPersistentRiceAdaptationEnabledFlag               ( m_persistentRiceAdaptationEnabledFlag );
  m_cTEncTop.setCabacBypassAlignmentEnabledFlag                   ( m_cabacBypassAlignmentEnabledFlag );
  m_cTEncTop.setLog2MaxTransformSkipBlockSize                     ( m_log2MaxTransformSkipBlockSize  );
  for (UInt signallingModeIndex = 0; signallingModeIndex < NUMBER_OF_RDPCM_SIGNALLING_MODES; signallingModeIndex++)
  {
    m_cTEncTop.setRdpcmEnabledFlag                                ( RDPCMSignallingMode(signallingModeIndex), m_rdpcmEnabledFlag[signallingModeIndex]);
  }
  m_cTEncTop.setUseConstrainedIntraPred                           ( m_bUseConstrainedIntraPred );
  m_cTEncTop.setFastUDIUseMPMEnabled                              ( m_bFastUDIUseMPMEnabled );
  m_cTEncTop.setFastMEForGenBLowDelayEnabled                      ( m_bFastMEForGenBLowDelayEnabled );
  m_cTEncTop.setUseBLambdaForNonKeyLowDelayPictures               ( m_bUseBLambdaForNonKeyLowDelayPictures );
  m_cTEncTop.setPCMLog2MinSize                                    ( m_uiPCMLog2MinSize);
  m_cTEncTop.setUsePCM                                            ( m_usePCM );

  // set internal bit-depth and constants
  for (UInt channelType = 0; channelType < MAX_NUM_CHANNEL_TYPE; channelType++)
  {
    m_cTEncTop.setBitDepth((ChannelType)channelType, m_internalBitDepth[channelType]);
    m_cTEncTop.setPCMBitDepth((ChannelType)channelType, m_bPCMInputBitDepthFlag ? m_MSBExtendedBitDepth[channelType] : m_internalBitDepth[channelType]);
#if JVET_X0048_X0103_FILM_GRAIN
    m_cTEncTop.setBitDepthInput((ChannelType)channelType, m_inputBitDepth[channelType]);
#endif
  }

  m_cTEncTop.setPCMLog2MaxSize                                    ( m_pcmLog2MaxSize);
  m_cTEncTop.setMaxNumMergeCand                                   ( m_maxNumMergeCand );


  //====== Weighted Prediction ========
  m_cTEncTop.setUseWP                                             ( m_useWeightedPred     );
  m_cTEncTop.setWPBiPred                                          ( m_useWeightedBiPred   );

  //====== Parallel Merge Estimation ========
  m_cTEncTop.setLog2ParallelMergeLevelMinus2                      ( m_log2ParallelMergeLevel - 2 );

  //====== Slice ========
  m_cTEncTop.setSliceMode                                         ( m_sliceMode );
  m_cTEncTop.setSliceArgument                                     ( m_sliceArgument );

  //====== Dependent Slice ========
  m_cTEncTop.setSliceSegmentMode                                  ( m_sliceSegmentMode );
  m_cTEncTop.setSliceSegmentArgument                              ( m_sliceSegmentArgument );

  if(m_sliceMode == NO_SLICES )
  {
    m_bLFCrossSliceBoundaryFlag = true;
  }
  m_cTEncTop.setLFCrossSliceBoundaryFlag                          ( m_bLFCrossSliceBoundaryFlag );
  m_cTEncTop.setUseSAO                                            ( m_bUseSAO );
  m_cTEncTop.setTestSAODisableAtPictureLevel                      ( m_bTestSAODisableAtPictureLevel );
  m_cTEncTop.setSaoEncodingRate                                   ( m_saoEncodingRate );
  m_cTEncTop.setSaoEncodingRateChroma                             ( m_saoEncodingRateChroma );
  m_cTEncTop.setMaxNumOffsetsPerPic                               ( m_maxNumOffsetsPerPic);

  m_cTEncTop.setSaoCtuBoundary                                    ( m_saoCtuBoundary);
  m_cTEncTop.setResetEncoderStateAfterIRAP                        ( m_resetEncoderStateAfterIRAP );
  m_cTEncTop.setPCMInputBitDepthFlag                              ( m_bPCMInputBitDepthFlag);
  m_cTEncTop.setPCMFilterDisableFlag                              ( m_bPCMFilterDisableFlag);

  m_cTEncTop.setIntraSmoothingDisabledFlag                        (!m_enableIntraReferenceSmoothing );
  m_cTEncTop.setDecodedPictureHashSEIType                         ( m_decodedPictureHashSEIType );
  m_cTEncTop.setRecoveryPointSEIEnabled                           ( m_recoveryPointSEIEnabled );
  m_cTEncTop.setBufferingPeriodSEIEnabled                         ( m_bufferingPeriodSEIEnabled );
  m_cTEncTop.setPictureTimingSEIEnabled                           ( m_pictureTimingSEIEnabled );
  m_cTEncTop.setToneMappingInfoSEIEnabled                         ( m_toneMappingInfoSEIEnabled );
  m_cTEncTop.setTMISEIToneMapId                                   ( m_toneMapId );
  m_cTEncTop.setTMISEIToneMapCancelFlag                           ( m_toneMapCancelFlag );
  m_cTEncTop.setTMISEIToneMapPersistenceFlag                      ( m_toneMapPersistenceFlag );
  m_cTEncTop.setTMISEICodedDataBitDepth                           ( m_toneMapCodedDataBitDepth );
  m_cTEncTop.setTMISEITargetBitDepth                              ( m_toneMapTargetBitDepth );
  m_cTEncTop.setTMISEIModelID                                     ( m_toneMapModelId );
  m_cTEncTop.setTMISEIMinValue                                    ( m_toneMapMinValue );
  m_cTEncTop.setTMISEIMaxValue                                    ( m_toneMapMaxValue );
  m_cTEncTop.setTMISEISigmoidMidpoint                             ( m_sigmoidMidpoint );
  m_cTEncTop.setTMISEISigmoidWidth                                ( m_sigmoidWidth );
  m_cTEncTop.setTMISEIStartOfCodedInterva                         ( m_startOfCodedInterval );
  m_cTEncTop.setTMISEINumPivots                                   ( m_numPivots );
  m_cTEncTop.setTMISEICodedPivotValue                             ( m_codedPivotValue );
  m_cTEncTop.setTMISEITargetPivotValue                            ( m_targetPivotValue );
  m_cTEncTop.setTMISEICameraIsoSpeedIdc                           ( m_cameraIsoSpeedIdc );
  m_cTEncTop.setTMISEICameraIsoSpeedValue                         ( m_cameraIsoSpeedValue );
  m_cTEncTop.setTMISEIExposureIndexIdc                            ( m_exposureIndexIdc );
  m_cTEncTop.setTMISEIExposureIndexValue                          ( m_exposureIndexValue );
  m_cTEncTop.setTMISEIExposureCompensationValueSignFlag           ( m_exposureCompensationValueSignFlag );
  m_cTEncTop.setTMISEIExposureCompensationValueNumerator          ( m_exposureCompensationValueNumerator );
  m_cTEncTop.setTMISEIExposureCompensationValueDenomIdc           ( m_exposureCompensationValueDenomIdc );
  m_cTEncTop.setTMISEIRefScreenLuminanceWhite                     ( m_refScreenLuminanceWhite );
  m_cTEncTop.setTMISEIExtendedRangeWhiteLevel                     ( m_extendedRangeWhiteLevel );
  m_cTEncTop.setTMISEINominalBlackLevelLumaCodeValue              ( m_nominalBlackLevelLumaCodeValue );
  m_cTEncTop.setTMISEINominalWhiteLevelLumaCodeValue              ( m_nominalWhiteLevelLumaCodeValue );
  m_cTEncTop.setTMISEIExtendedWhiteLevelLumaCodeValue             ( m_extendedWhiteLevelLumaCodeValue );
  m_cTEncTop.setChromaResamplingFilterHintEnabled                 ( m_chromaResamplingFilterSEIenabled );
  m_cTEncTop.setChromaResamplingHorFilterIdc                      ( m_chromaResamplingHorFilterIdc );
  m_cTEncTop.setChromaResamplingVerFilterIdc                      ( m_chromaResamplingVerFilterIdc );
  m_cTEncTop.setFramePackingArrangementSEIEnabled                 ( m_framePackingSEIEnabled );
  m_cTEncTop.setFramePackingArrangementSEIType                    ( m_framePackingSEIType );
  m_cTEncTop.setFramePackingArrangementSEIId                      ( m_framePackingSEIId );
  m_cTEncTop.setFramePackingArrangementSEIQuincunx                ( m_framePackingSEIQuincunx );
  m_cTEncTop.setFramePackingArrangementSEIInterpretation          ( m_framePackingSEIInterpretation );
  m_cTEncTop.setSegmentedRectFramePackingArrangementSEIEnabled    ( m_segmentedRectFramePackingSEIEnabled );
  m_cTEncTop.setSegmentedRectFramePackingArrangementSEICancel     ( m_segmentedRectFramePackingSEICancel );
  m_cTEncTop.setSegmentedRectFramePackingArrangementSEIType       ( m_segmentedRectFramePackingSEIType );
  m_cTEncTop.setSegmentedRectFramePackingArrangementSEIPersistence( m_segmentedRectFramePackingSEIPersistence );
  m_cTEncTop.setDisplayOrientationSEIAngle                        ( m_displayOrientationSEIAngle );
  m_cTEncTop.setTemporalLevel0IndexSEIEnabled                     ( m_temporalLevel0IndexSEIEnabled );
  m_cTEncTop.setGradualDecodingRefreshInfoEnabled                 ( m_gradualDecodingRefreshInfoEnabled );
  m_cTEncTop.setNoDisplaySEITLayer                                ( m_noDisplaySEITLayer );
  m_cTEncTop.setDecodingUnitInfoSEIEnabled                        ( m_decodingUnitInfoSEIEnabled );
  m_cTEncTop.setSOPDescriptionSEIEnabled                          ( m_SOPDescriptionSEIEnabled );
  m_cTEncTop.setScalableNestingSEIEnabled                         ( m_scalableNestingSEIEnabled );
  m_cTEncTop.setTMCTSSEIEnabled                                   ( m_tmctsSEIEnabled );
#if MCTS_ENC_CHECK
  m_cTEncTop.setTMCTSSEITileConstraint                            ( m_tmctsSEITileConstraint );
#endif
#if MCTS_EXTRACTION
  m_cTEncTop.setTMCTSExtractionSEIEnabled                         ( m_tmctsExtractionSEIEnabled);
#endif
  m_cTEncTop.setTimeCodeSEIEnabled                                ( m_timeCodeSEIEnabled );
  m_cTEncTop.setNumberOfTimeSets                                  ( m_timeCodeSEINumTs );
  for(Int i = 0; i < m_timeCodeSEINumTs; i++)
  {
    m_cTEncTop.setTimeSet(m_timeSetArray[i], i);
  }
  m_cTEncTop.setKneeSEIEnabled                                    ( m_kneeSEIEnabled );
  m_cTEncTop.setKneeFunctionInformationSEI                        ( m_kneeFunctionInformationSEI );
  m_cTEncTop.setCcvSEIEnabled                                     (m_ccvSEIEnabled);
  m_cTEncTop.setCcvSEICancelFlag                                  (m_ccvSEICancelFlag);
  m_cTEncTop.setCcvSEIPersistenceFlag                             (m_ccvSEIPersistenceFlag);
  
  m_cTEncTop.setCcvSEIEnabled                                     (m_ccvSEIEnabled);
  m_cTEncTop.setCcvSEICancelFlag                                  (m_ccvSEICancelFlag);
  m_cTEncTop.setCcvSEIPersistenceFlag                             (m_ccvSEIPersistenceFlag);
  m_cTEncTop.setCcvSEIPrimariesPresentFlag                        (m_ccvSEIPrimariesPresentFlag);
  m_cTEncTop.setCcvSEIMinLuminanceValuePresentFlag                (m_ccvSEIMinLuminanceValuePresentFlag);
  m_cTEncTop.setCcvSEIMaxLuminanceValuePresentFlag                (m_ccvSEIMaxLuminanceValuePresentFlag);
  m_cTEncTop.setCcvSEIAvgLuminanceValuePresentFlag                (m_ccvSEIAvgLuminanceValuePresentFlag);
  for(Int i = 0; i < MAX_NUM_COMPONENT; i++) {
    m_cTEncTop.setCcvSEIPrimariesX                                (m_ccvSEIPrimariesX[i], i);
    m_cTEncTop.setCcvSEIPrimariesY                                (m_ccvSEIPrimariesY[i], i);
  }
  m_cTEncTop.setCcvSEIMinLuminanceValue                           (m_ccvSEIMinLuminanceValue);
  m_cTEncTop.setCcvSEIMaxLuminanceValue                           (m_ccvSEIMaxLuminanceValue);
  m_cTEncTop.setCcvSEIAvgLuminanceValue                           (m_ccvSEIAvgLuminanceValue);
  m_cTEncTop.setErpSEIEnabled                                     ( m_erpSEIEnabled );           
  m_cTEncTop.setErpSEICancelFlag                                  ( m_erpSEICancelFlag );        
  m_cTEncTop.setErpSEIPersistenceFlag                             ( m_erpSEIPersistenceFlag );   
  m_cTEncTop.setErpSEIGuardBandFlag                               ( m_erpSEIGuardBandFlag );     
  m_cTEncTop.setErpSEIGuardBandType                               ( m_erpSEIGuardBandType );     
  m_cTEncTop.setErpSEILeftGuardBandWidth                          ( m_erpSEILeftGuardBandWidth );
  m_cTEncTop.setErpSEIRightGuardBandWidth                         ( m_erpSEIRightGuardBandWidth );
  m_cTEncTop.setSphereRotationSEIEnabled                          ( m_sphereRotationSEIEnabled );
  m_cTEncTop.setSphereRotationSEICancelFlag                       ( m_sphereRotationSEICancelFlag );
  m_cTEncTop.setSphereRotationSEIPersistenceFlag                  ( m_sphereRotationSEIPersistenceFlag );
  m_cTEncTop.setSphereRotationSEIYaw                              ( m_sphereRotationSEIYaw );
  m_cTEncTop.setSphereRotationSEIPitch                            ( m_sphereRotationSEIPitch );
  m_cTEncTop.setSphereRotationSEIRoll                             ( m_sphereRotationSEIRoll );
  m_cTEncTop.setOmniViewportSEIEnabled                            ( m_omniViewportSEIEnabled );          
  m_cTEncTop.setOmniViewportSEIId                                 ( m_omniViewportSEIId );               
  m_cTEncTop.setOmniViewportSEICancelFlag                         ( m_omniViewportSEICancelFlag );       
  m_cTEncTop.setOmniViewportSEIPersistenceFlag                    ( m_omniViewportSEIPersistenceFlag );  
  m_cTEncTop.setOmniViewportSEICntMinus1                          ( m_omniViewportSEICntMinus1 );        
  m_cTEncTop.setOmniViewportSEIAzimuthCentre                      ( m_omniViewportSEIAzimuthCentre );    
  m_cTEncTop.setOmniViewportSEIElevationCentre                    ( m_omniViewportSEIElevationCentre );  
  m_cTEncTop.setOmniViewportSEITiltCentre                         ( m_omniViewportSEITiltCentre );       
  m_cTEncTop.setOmniViewportSEIHorRange                           ( m_omniViewportSEIHorRange );         
  m_cTEncTop.setOmniViewportSEIVerRange                           ( m_omniViewportSEIVerRange );         
  m_cTEncTop.setGopBasedTemporalFilterEnabled                     ( m_gopBasedTemporalFilterEnabled );
#if JVET_Y0077_BIM
  m_cTEncTop.setBIM                                               ( m_bimEnabled );
#endif
  m_cTEncTop.setCmpSEIEnabled                                     (m_cmpSEIEnabled);
  m_cTEncTop.setCmpSEICmpCancelFlag                               (m_cmpSEICmpCancelFlag);
  m_cTEncTop.setCmpSEICmpPersistenceFlag                          (m_cmpSEICmpPersistenceFlag);
  m_cTEncTop.setRwpSEIEnabled                                     (m_rwpSEIEnabled);
  m_cTEncTop.setRwpSEIRwpCancelFlag                               (m_rwpSEIRwpCancelFlag);
  m_cTEncTop.setRwpSEIRwpPersistenceFlag                          (m_rwpSEIRwpPersistenceFlag);
  m_cTEncTop.setRwpSEIConstituentPictureMatchingFlag              (m_rwpSEIConstituentPictureMatchingFlag);
  m_cTEncTop.setRwpSEINumPackedRegions                            (m_rwpSEINumPackedRegions);
  m_cTEncTop.setRwpSEIProjPictureWidth                            (m_rwpSEIProjPictureWidth);
  m_cTEncTop.setRwpSEIProjPictureHeight                           (m_rwpSEIProjPictureHeight);
  m_cTEncTop.setRwpSEIPackedPictureWidth                          (m_rwpSEIPackedPictureWidth);
  m_cTEncTop.setRwpSEIPackedPictureHeight                         (m_rwpSEIPackedPictureHeight);
  m_cTEncTop.setRwpSEIRwpTransformType                            (m_rwpSEIRwpTransformType);
  m_cTEncTop.setRwpSEIRwpGuardBandFlag                            (m_rwpSEIRwpGuardBandFlag);
  m_cTEncTop.setRwpSEIProjRegionWidth                             (m_rwpSEIProjRegionWidth);
  m_cTEncTop.setRwpSEIProjRegionHeight                            (m_rwpSEIProjRegionHeight);
  m_cTEncTop.setRwpSEIRwpSEIProjRegionTop                         (m_rwpSEIRwpSEIProjRegionTop);
  m_cTEncTop.setRwpSEIProjRegionLeft                              (m_rwpSEIProjRegionLeft);
  m_cTEncTop.setRwpSEIPackedRegionWidth                           (m_rwpSEIPackedRegionWidth);
  m_cTEncTop.setRwpSEIPackedRegionHeight                          (m_rwpSEIPackedRegionHeight);
  m_cTEncTop.setRwpSEIPackedRegionTop                             (m_rwpSEIPackedRegionTop);
  m_cTEncTop.setRwpSEIPackedRegionLeft                            (m_rwpSEIPackedRegionLeft);
  m_cTEncTop.setRwpSEIRwpLeftGuardBandWidth                       (m_rwpSEIRwpLeftGuardBandWidth);
  m_cTEncTop.setRwpSEIRwpRightGuardBandWidth                      (m_rwpSEIRwpRightGuardBandWidth);
  m_cTEncTop.setRwpSEIRwpTopGuardBandHeight                       (m_rwpSEIRwpTopGuardBandHeight);
  m_cTEncTop.setRwpSEIRwpBottomGuardBandHeight                    (m_rwpSEIRwpBottomGuardBandHeight);
  m_cTEncTop.setRwpSEIRwpGuardBandNotUsedForPredFlag              (m_rwpSEIRwpGuardBandNotUsedForPredFlag);
  m_cTEncTop.setRwpSEIRwpGuardBandType                            (m_rwpSEIRwpGuardBandType);
#if SHUTTER_INTERVAL_SEI_MESSAGE
  m_cTEncTop.setSiiSEIEnabled                                     (m_siiSEIEnabled);
  m_cTEncTop.setSiiSEINumUnitsInShutterInterval                   (m_siiSEINumUnitsInShutterInterval);
  m_cTEncTop.setSiiSEITimeScale                                   (m_siiSEITimeScale);
  m_cTEncTop.setSiiSEISubLayerNumUnitsInSI                        (m_siiSEISubLayerNumUnitsInSI);
#endif
#if SEI_ENCODER_CONTROL
// film grain charcteristics
  m_cTEncTop.setFilmGrainCharactersticsSEIEnabled                 (m_fgcSEIEnabled);
  m_cTEncTop.setFilmGrainCharactersticsSEICancelFlag              (m_fgcSEICancelFlag);
  m_cTEncTop.setFilmGrainCharactersticsSEIPersistenceFlag         (m_fgcSEIPersistenceFlag);
  m_cTEncTop.setFilmGrainCharactersticsSEIModelID                 ((UChar)m_fgcSEIModelID);
  m_cTEncTop.setFilmGrainCharactersticsSEISepColourDescPresent    (m_fgcSEISepColourDescPresentFlag);
  m_cTEncTop.setFilmGrainCharactersticsSEIBlendingModeID          ((UChar)m_fgcSEIBlendingModeID);
  m_cTEncTop.setFilmGrainCharactersticsSEILog2ScaleFactor         ((UChar)m_fgcSEILog2ScaleFactor);
#if JVET_X0048_X0103_FILM_GRAIN
  m_cTEncTop.setFilmGrainAnalysisEnabled                          (m_fgcSEIAnalysisEnabled);
  m_cTEncTop.setFilmGrainExternalMask                             (m_fgcSEIExternalMask);
  m_cTEncTop.setFilmGrainExternalDenoised                         (m_fgcSEIExternalDenoised);
  m_cTEncTop.setFilmGrainCharactersticsSEIPerPictureSEI           (m_fgcSEIPerPictureSEI);
#endif
  for (Int i = 0; i < MAX_NUM_COMPONENT; i++) {
    m_cTEncTop.setFGCSEICompModelPresent                          (m_fgcSEICompModelPresent[i], i);
#if JVET_X0048_X0103_FILM_GRAIN	
    if (m_fgcSEICompModelPresent[i])
    {
      m_cTEncTop.setFGCSEINumIntensityIntervalMinus1              ((UChar)m_fgcSEINumIntensityIntervalMinus1[i], i);
      m_cTEncTop.setFGCSEINumModelValuesMinus1                    ((UChar)m_fgcSEINumModelValuesMinus1[i], i);
      for (UInt j = 0; j <= m_fgcSEINumIntensityIntervalMinus1[i]; j++)
      {
        m_cTEncTop.setFGCSEIIntensityIntervalLowerBound           ((UChar)m_fgcSEIIntensityIntervalLowerBound[i][j], i, j);
        m_cTEncTop.setFGCSEIIntensityIntervalUpperBound           ((UChar)m_fgcSEIIntensityIntervalUpperBound[i][j], i, j);
        for (UInt k = 0; k <= m_fgcSEINumModelValuesMinus1[i]; k++)
        {
          m_cTEncTop.setFGCSEICompModelValue                      (m_fgcSEICompModelValue[i][j][k], i, j, k);
        }
      }
    }
#endif
  }
// content light level
  m_cTEncTop.setCLLSEIEnabled                                     (m_cllSEIEnabled);
  m_cTEncTop.setCLLSEIMaxContentLightLevel                        ((UShort)m_cllSEIMaxContentLevel);
  m_cTEncTop.setCLLSEIMaxPicAvgLightLevel                         ((UShort)m_cllSEIMaxPicAvgLevel);
// ambient viewing enviornment
  m_cTEncTop.setAmbientViewingEnvironmentSEIEnabled               (m_aveSEIEnabled);
  m_cTEncTop.setAmbientViewingEnvironmentSEIIlluminance           (m_aveSEIAmbientIlluminance);
  m_cTEncTop.setAmbientViewingEnvironmentSEIAmbientLightX         ((UShort)m_aveSEIAmbientLightX);
  m_cTEncTop.setAmbientViewingEnvironmentSEIAmbientLightY         ((UShort)m_aveSEIAmbientLightY);
#endif
  if (m_fisheyeVIdeoInfoSEIEnabled)
  {
    m_cTEncTop.setFviSEIEnabled(m_fisheyeVideoInfoSEI);
  }
  else
  {
    m_cTEncTop.setFviSEIDisabled();
  }
  m_cTEncTop.setColourRemapInfoSEIFileRoot                        ( m_colourRemapSEIFileRoot );
  m_cTEncTop.setMasteringDisplaySEI                               ( m_masteringDisplay );
  m_cTEncTop.setSEIAlternativeTransferCharacteristicsSEIEnable    ( m_preferredTransferCharacteristics>=0     );
  m_cTEncTop.setSEIPreferredTransferCharacteristics               ( UChar(m_preferredTransferCharacteristics) );
  m_cTEncTop.setSEIGreenMetadataInfoSEIEnable                     ( m_greenMetadataType > 0 );
  m_cTEncTop.setSEIGreenMetadataType                              ( UChar(m_greenMetadataType) );
  m_cTEncTop.setSEIXSDMetricType                                  ( UChar(m_xsdMetricType) );
  m_cTEncTop.setRegionalNestingSEIFileRoot                        ( m_regionalNestingSEIFileRoot );
  m_cTEncTop.setAnnotatedRegionSEIFileRoot                        (m_arSEIFileRoot);
  m_cTEncTop.setTileUniformSpacingFlag                            ( m_tileUniformSpacingFlag );
  m_cTEncTop.setNumColumnsMinus1                                  ( m_numTileColumnsMinus1 );
  m_cTEncTop.setNumRowsMinus1                                     ( m_numTileRowsMinus1 );
  if(!m_tileUniformSpacingFlag)
  {
    m_cTEncTop.setColumnWidth                                     ( m_tileColumnWidth );
    m_cTEncTop.setRowHeight                                       ( m_tileRowHeight );
  }
  m_cTEncTop.xCheckGSParameters();
  Int uiTilesCount = (m_numTileRowsMinus1+1) * (m_numTileColumnsMinus1+1);
  if(uiTilesCount == 1)
  {
    m_bLFCrossTileBoundaryFlag = true;
  }
  m_cTEncTop.setLFCrossTileBoundaryFlag                           ( m_bLFCrossTileBoundaryFlag );
  m_cTEncTop.setEntropyCodingSyncEnabledFlag                      ( m_entropyCodingSyncEnabledFlag );
  m_cTEncTop.setTMVPModeId                                        ( m_TMVPModeId );
  m_cTEncTop.setUseScalingListId                                  ( m_useScalingListId  );
  m_cTEncTop.setScalingListFileName                               ( m_scalingListFileName );
  m_cTEncTop.setSignDataHidingEnabledFlag                         ( m_signDataHidingEnabledFlag);
  m_cTEncTop.setUseRateCtrl                                       ( m_RCEnableRateControl );
  m_cTEncTop.setTargetBitrate                                     ( m_RCTargetBitrate );
  m_cTEncTop.setKeepHierBit                                       ( m_RCKeepHierarchicalBit );
  m_cTEncTop.setLCULevelRC                                        ( m_RCLCULevelRC );
  m_cTEncTop.setUseLCUSeparateModel                               ( m_RCUseLCUSeparateModel );
  m_cTEncTop.setInitialQP                                         ( m_RCInitialQP );
  m_cTEncTop.setForceIntraQP                                      ( m_RCForceIntraQP );
  m_cTEncTop.setCpbSaturationEnabled                              ( m_RCCpbSaturationEnabled );
  m_cTEncTop.setCpbSize                                           ( m_RCCpbSize );
  m_cTEncTop.setInitialCpbFullness                                ( m_RCInitialCpbFullness );
  m_cTEncTop.setTransquantBypassEnabledFlag                       ( m_TransquantBypassEnabledFlag );
  m_cTEncTop.setCUTransquantBypassFlagForceValue                  ( m_CUTransquantBypassFlagForce );
  m_cTEncTop.setCostMode                                          ( m_costMode );
  m_cTEncTop.setUseRecalculateQPAccordingToLambda                 ( m_recalculateQPAccordingToLambda );
  m_cTEncTop.setUseStrongIntraSmoothing                           ( m_useStrongIntraSmoothing );
  m_cTEncTop.setActiveParameterSetsSEIEnabled                     ( m_activeParameterSetsSEIEnabled );
  m_cTEncTop.setVuiParametersPresentFlag                          ( m_vuiParametersPresentFlag );
  m_cTEncTop.setAspectRatioInfoPresentFlag                        ( m_aspectRatioInfoPresentFlag);
  m_cTEncTop.setAspectRatioIdc                                    ( m_aspectRatioIdc );
  m_cTEncTop.setSarWidth                                          ( m_sarWidth );
  m_cTEncTop.setSarHeight                                         ( m_sarHeight );
  m_cTEncTop.setOverscanInfoPresentFlag                           ( m_overscanInfoPresentFlag );
  m_cTEncTop.setOverscanAppropriateFlag                           ( m_overscanAppropriateFlag );
  m_cTEncTop.setVideoSignalTypePresentFlag                        ( m_videoSignalTypePresentFlag );
  m_cTEncTop.setVideoFormat                                       ( m_videoFormat );
  m_cTEncTop.setVideoFullRangeFlag                                ( m_videoFullRangeFlag );
  m_cTEncTop.setColourDescriptionPresentFlag                      ( m_colourDescriptionPresentFlag );
  m_cTEncTop.setColourPrimaries                                   ( m_colourPrimaries );
  m_cTEncTop.setTransferCharacteristics                           ( m_transferCharacteristics );
  m_cTEncTop.setMatrixCoefficients                                ( m_matrixCoefficients );
  m_cTEncTop.setChromaLocInfoPresentFlag                          ( m_chromaLocInfoPresentFlag );
  m_cTEncTop.setChromaSampleLocTypeTopField                       ( m_chromaSampleLocTypeTopField );
  m_cTEncTop.setChromaSampleLocTypeBottomField                    ( m_chromaSampleLocTypeBottomField );
  m_cTEncTop.setNeutralChromaIndicationFlag                       ( m_neutralChromaIndicationFlag );
  m_cTEncTop.setDefaultDisplayWindow                              ( m_defDispWinLeftOffset, m_defDispWinRightOffset, m_defDispWinTopOffset, m_defDispWinBottomOffset );
  m_cTEncTop.setFrameFieldInfoPresentFlag                         ( m_frameFieldInfoPresentFlag );
  m_cTEncTop.setPocProportionalToTimingFlag                       ( m_pocProportionalToTimingFlag );
  m_cTEncTop.setNumTicksPocDiffOneMinus1                          ( m_numTicksPocDiffOneMinus1    );
  m_cTEncTop.setBitstreamRestrictionFlag                          ( m_bitstreamRestrictionFlag );
  m_cTEncTop.setTilesFixedStructureFlag                           ( m_tilesFixedStructureFlag );
  m_cTEncTop.setMotionVectorsOverPicBoundariesFlag                ( m_motionVectorsOverPicBoundariesFlag );
  m_cTEncTop.setMinSpatialSegmentationIdc                         ( m_minSpatialSegmentationIdc );
  m_cTEncTop.setMaxBytesPerPicDenom                               ( m_maxBytesPerPicDenom );
  m_cTEncTop.setMaxBitsPerMinCuDenom                              ( m_maxBitsPerMinCuDenom );
  m_cTEncTop.setLog2MaxMvLengthHorizontal                         ( m_log2MaxMvLengthHorizontal );
  m_cTEncTop.setLog2MaxMvLengthVertical                           ( m_log2MaxMvLengthVertical );
  m_cTEncTop.setEfficientFieldIRAPEnabled                         ( m_bEfficientFieldIRAPEnabled );
  m_cTEncTop.setHarmonizeGopFirstFieldCoupleEnabled               ( m_bHarmonizeGopFirstFieldCoupleEnabled );

  m_cTEncTop.setSummaryOutFilename                                ( m_summaryOutFilename );
  m_cTEncTop.setSummaryPicFilenameBase                            ( m_summaryPicFilenameBase );
  m_cTEncTop.setSummaryVerboseness                                ( m_summaryVerboseness );

#if JCTVC_AD0021_SEI_MANIFEST
  m_cTEncTop.setSEIManifestSEIEnabled(m_SEIManifestSEIEnabled);
#endif
#if JCTVC_AD0021_SEI_PREFIX_INDICATION
  m_cTEncTop.setSEIPrefixIndicationSEIEnabled(m_SEIPrefixIndicationSEIEnabled);
#endif
}

Void TAppEncTop::xCreateLib()
{
  // Video I/O
  m_cTVideoIOYuvInputFile.open( m_inputFileName,     false, m_inputBitDepth, m_MSBExtendedBitDepth, m_internalBitDepth );  // read  mode
  m_cTVideoIOYuvInputFile.skipFrames(m_FrameSkip, m_inputFileWidth, m_inputFileHeight, m_InputChromaFormatIDC);

  if (!m_reconFileName.empty())
  {
    m_cTVideoIOYuvReconFile.open(m_reconFileName, true, m_outputBitDepth, m_outputBitDepth, m_internalBitDepth);  // write mode
  }
#if SHUTTER_INTERVAL_SEI_PROCESSING
  if (m_ShutterFilterEnable && !m_shutterIntervalPreFileName.empty())
  {
    m_cTVideoIOYuvSIIPreFile.open(m_shutterIntervalPreFileName, true, m_outputBitDepth, m_outputBitDepth, m_internalBitDepth);  // write mode
  }
#endif

  // Neo Decoder
  m_cTEncTop.create();
}

Void TAppEncTop::xDestroyLib()
{
  // Video I/O
  m_cTVideoIOYuvInputFile.close();
  m_cTVideoIOYuvReconFile.close();
#if SHUTTER_INTERVAL_SEI_PROCESSING
  if (m_ShutterFilterEnable && !m_shutterIntervalPreFileName.empty())
  {
    m_cTVideoIOYuvSIIPreFile.close();
  }
#endif

  // Neo Decoder
  m_cTEncTop.destroy();
}

Void TAppEncTop::xInitLib(Bool isFieldCoding)
{
  m_cTEncTop.init(isFieldCoding);
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/**
 - create internal class
 - initialize internal variable
 - until the end of input YUV file, call encoding function in TEncTop class
 - delete allocated buffers
 - destroy internal class
 .
 */
Void TAppEncTop::encode()
{
  fstream bitstreamFile(m_bitstreamFileName.c_str(), fstream::binary | fstream::out);
  if (!bitstreamFile)
  {
    fprintf(stderr, "\nfailed to open bitstream file `%s' for writing\n", m_bitstreamFileName.c_str());
    exit(EXIT_FAILURE);
  }

  TComPicYuv*       pcPicYuvOrg = new TComPicYuv;
  TComPicYuv*       pcPicYuvRec = NULL;

#if JVET_X0048_X0103_FILM_GRAIN
  TComPicYuv* m_filteredOrgPicForFG;
  if (m_fgcSEIAnalysisEnabled && m_fgcSEIExternalDenoised.empty())
  {
    m_filteredOrgPicForFG = new TComPicYuv;
    m_filteredOrgPicForFG->create(m_sourceWidth, m_sourceHeight, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true);
  }
  else
  {
    m_filteredOrgPicForFG = NULL;
  }
#endif

  // initialize internal class & member variables
  xInitLibCfg();
  xCreateLib();
  xInitLib(m_isField);

  printChromaFormat();

  // main encoder loop
  Int   iNumEncoded = 0;
  Bool  bEos = false;

  const InputColourSpaceConversion ipCSC  =  m_inputColourSpaceConvert;
  const InputColourSpaceConversion snrCSC = (!m_snrInternalColourSpace) ? m_inputColourSpaceConvert : IPCOLOURSPACE_UNCHANGED;

  list<AccessUnit> outputAccessUnits; ///< list of access units to write out.  is populated by the encoding process

  TComPicYuv cPicYuvTrueOrg;

  // allocate original YUV buffer
  if( m_isField )
  {
    pcPicYuvOrg->create  ( m_sourceWidth, m_sourceHeightOrg, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true );
    cPicYuvTrueOrg.create(m_sourceWidth, m_sourceHeightOrg, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true);
  }
  else
  {
    pcPicYuvOrg->create  ( m_sourceWidth, m_sourceHeight, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true );
    cPicYuvTrueOrg.create(m_sourceWidth, m_sourceHeight, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true );
  }

#if EXTENSION_360_VIDEO
  TExt360AppEncTop           ext360(*this, m_cTEncTop.getGOPEncoder()->getExt360Data(), *(m_cTEncTop.getGOPEncoder()), *pcPicYuvOrg);
#endif
  TEncTemporalFilter temporalFilter;
#if JVET_Y0077_BIM
  if ( m_gopBasedTemporalFilterEnabled || m_bimEnabled )
#else
  if (m_gopBasedTemporalFilterEnabled)
#endif
  {
    temporalFilter.init(m_FrameSkip, m_inputBitDepth, m_MSBExtendedBitDepth, m_internalBitDepth, m_sourceWidth, m_sourceHeight,
      m_sourcePadding, m_framesToBeEncoded, m_bClipInputVideoToRec709Range, m_inputFileName, m_chromaFormatIDC,
      m_inputColourSpaceConvert, m_iQP, m_iGOPSize, m_gopBasedTemporalFilterStrengths,
      m_gopBasedTemporalFilterPastRefs, m_gopBasedTemporalFilterFutureRefs,
#if !JVET_Y0077_BIM
      m_firstValidFrame, m_lastValidFrame);
#else
      m_firstValidFrame, m_lastValidFrame,
      m_gopBasedTemporalFilterEnabled, m_cTEncTop.getAdaptQPmap(), m_bimEnabled);
#endif
  }
#if JVET_X0048_X0103_FILM_GRAIN
  TEncTemporalFilter m_temporalFilterForFG;
  if ( m_fgcSEIAnalysisEnabled && m_fgcSEIExternalDenoised.empty() )
  {
    int  filteredFrame                 = 0;
    if ( m_iIntraPeriod < 1 )
      filteredFrame = 2 * m_iFrameRate;
    else
      filteredFrame = m_iIntraPeriod;

    map<int, double> filteredFramesAndStrengths = { { filteredFrame, 1.5 } };   // TODO: adjust MCTF and MCTF strenght

    m_temporalFilterForFG.init(m_FrameSkip, m_inputBitDepth, m_MSBExtendedBitDepth, m_internalBitDepth, m_sourceWidth, m_sourceHeight,
      m_sourcePadding, m_framesToBeEncoded, m_bClipInputVideoToRec709Range, m_inputFileName, m_chromaFormatIDC,
      m_inputColourSpaceConvert, m_iQP, m_iGOPSize, filteredFramesAndStrengths,
      m_gopBasedTemporalFilterPastRefs, m_gopBasedTemporalFilterFutureRefs,
#if !JVET_Y0077_BIM
      m_firstValidFrame, m_lastValidFrame);
#else
      m_firstValidFrame, m_lastValidFrame,
      m_gopBasedTemporalFilterEnabled, m_cTEncTop.getAdaptQPmap(), m_bimEnabled);
#endif
  }
#endif
  while ( !bEos )
  {
    // get buffers
    xGetBuffer(pcPicYuvRec);

    // read input YUV file
#if EXTENSION_360_VIDEO
    if (ext360.isEnabled())
    {
      ext360.read(m_cTVideoIOYuvInputFile, *pcPicYuvOrg, cPicYuvTrueOrg, ipCSC);
    }
    else
    {
      m_cTVideoIOYuvInputFile.read( pcPicYuvOrg, &cPicYuvTrueOrg, ipCSC, m_sourcePadding, m_InputChromaFormatIDC, m_bClipInputVideoToRec709Range );
    }
#else
    m_cTVideoIOYuvInputFile.read( pcPicYuvOrg, &cPicYuvTrueOrg, ipCSC, m_sourcePadding, m_InputChromaFormatIDC, m_bClipInputVideoToRec709Range );
#endif

#if JVET_X0048_X0103_FILM_GRAIN
    if (m_fgcSEIAnalysisEnabled && m_fgcSEIExternalDenoised.empty())
    {
      pcPicYuvOrg->copyToPic(m_filteredOrgPicForFG);
      m_temporalFilterForFG.filter(m_filteredOrgPicForFG, m_iFrameRcvd);
    }
#endif

#if JVET_Y0077_BIM
    if ( m_gopBasedTemporalFilterEnabled || m_bimEnabled )
#else
    if (m_gopBasedTemporalFilterEnabled)
#endif
    {
      temporalFilter.filter(pcPicYuvOrg, m_iFrameRcvd);
    }

    // increase number of received frames
    m_iFrameRcvd++;

    bEos = (m_isField && (m_iFrameRcvd == (m_framesToBeEncoded >> 1) )) || ( !m_isField && (m_iFrameRcvd == m_framesToBeEncoded) );

    Bool flush = 0;
    // if end of file (which is only detected on a read failure) flush the encoder of any queued pictures
    if (m_cTVideoIOYuvInputFile.isEof())
    {
      flush = true;
      bEos = true;
      m_iFrameRcvd--;
      m_cTEncTop.setFramesToBeEncoded(m_iFrameRcvd);
    }

    // call encoding function for one frame
    if ( m_isField )
    {
      m_cTEncTop.encode( bEos, flush ? 0 : pcPicYuvOrg, flush ? 0 : &cPicYuvTrueOrg, ipCSC, snrCSC, m_cListPicYuvRec, outputAccessUnits, iNumEncoded, m_isTopFieldFirst );
    }
    else
    {
#if JVET_X0048_X0103_FILM_GRAIN
      m_cTEncTop.encode( bEos, flush ? 0 : pcPicYuvOrg, flush ? 0 : &cPicYuvTrueOrg, flush ? 0 : m_filteredOrgPicForFG, ipCSC, snrCSC, m_cListPicYuvRec, outputAccessUnits, iNumEncoded);
#else
      m_cTEncTop.encode( bEos, flush ? 0 : pcPicYuvOrg, flush ? 0 : &cPicYuvTrueOrg, ipCSC, snrCSC, m_cListPicYuvRec, outputAccessUnits, iNumEncoded );
#endif
    }

#if SHUTTER_INTERVAL_SEI_PROCESSING
    if (m_ShutterFilterEnable && !m_shutterIntervalPreFileName.empty())
    {
      m_cTVideoIOYuvSIIPreFile.write(pcPicYuvOrg, ipCSC, m_confWinLeft, m_confWinRight, m_confWinTop, m_confWinBottom,
        NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range);
    }
#endif

    // write bistream to file if necessary
    if ( iNumEncoded > 0 )
    {
      xWriteOutput(bitstreamFile, iNumEncoded, outputAccessUnits);
      outputAccessUnits.clear();
    }
    // temporally skip frames
    if( m_temporalSubsampleRatio > 1 )
    {
      m_cTVideoIOYuvInputFile.skipFrames(m_temporalSubsampleRatio-1, m_inputFileWidth, m_inputFileHeight, m_InputChromaFormatIDC);
    }
  }

  m_cTEncTop.printSummary(m_isField);

  // delete original YUV buffer
  pcPicYuvOrg->destroy();
  delete pcPicYuvOrg;
  pcPicYuvOrg = NULL;

#if JVET_X0048_X0103_FILM_GRAIN
  if (m_fgcSEIAnalysisEnabled && m_fgcSEIExternalDenoised.empty())
  {
    m_filteredOrgPicForFG->destroy();
    delete m_filteredOrgPicForFG;
    m_filteredOrgPicForFG = NULL;
  }
#endif

  // delete used buffers in encoder class
  m_cTEncTop.deletePicBuffer();
  cPicYuvTrueOrg.destroy();

  // delete buffers & classes
  xDeleteBuffer();
  xDestroyLib();

  printRateSummary();

  return;
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

/**
 - application has picture buffer list with size of GOP
 - picture buffer list acts as ring buffer
 - end of the list has the latest picture
 .
 */
Void TAppEncTop::xGetBuffer( TComPicYuv*& rpcPicYuvRec)
{
  assert( m_iGOPSize > 0 );

  // org. buffer
  if ( m_cListPicYuvRec.size() >= (UInt)m_iGOPSize ) // buffer will be 1 element longer when using field coding, to maintain first field whilst processing second.
  {
    rpcPicYuvRec = m_cListPicYuvRec.popFront();

  }
  else
  {
    rpcPicYuvRec = new TComPicYuv;

    rpcPicYuvRec->create( m_sourceWidth, m_sourceHeight, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true );

  }
  m_cListPicYuvRec.pushBack( rpcPicYuvRec );
}

Void TAppEncTop::xDeleteBuffer( )
{
  TComList<TComPicYuv*>::iterator iterPicYuvRec  = m_cListPicYuvRec.begin();

  Int iSize = Int( m_cListPicYuvRec.size() );

  for ( Int i = 0; i < iSize; i++ )
  {
    TComPicYuv*  pcPicYuvRec  = *(iterPicYuvRec++);
    pcPicYuvRec->destroy();
    delete pcPicYuvRec; pcPicYuvRec = NULL;
  }

}

/** 
  Write access units to output file.
  \param bitstreamFile  target bitstream file
  \param iNumEncoded    number of encoded frames
  \param accessUnits    list of access units to be written
 */
Void TAppEncTop::xWriteOutput(std::ostream& bitstreamFile, Int iNumEncoded, const std::list<AccessUnit>& accessUnits)
{
  const InputColourSpaceConversion ipCSC = (!m_outputInternalColourSpace) ? m_inputColourSpaceConvert : IPCOLOURSPACE_UNCHANGED;

  if (m_isField)
  {
    //Reinterlace fields
    Int i;
    TComList<TComPicYuv*>::iterator iterPicYuvRec = m_cListPicYuvRec.end();
    list<AccessUnit>::const_iterator iterBitstream = accessUnits.begin();

    for ( i = 0; i < iNumEncoded; i++ )
    {
      --iterPicYuvRec;
    }

    for ( i = 0; i < iNumEncoded/2; i++ )
    {
      TComPicYuv*  pcPicYuvRecTop  = *(iterPicYuvRec++);
      TComPicYuv*  pcPicYuvRecBottom  = *(iterPicYuvRec++);

      if (!m_reconFileName.empty())
      {
        m_cTVideoIOYuvReconFile.write( pcPicYuvRecTop, pcPicYuvRecBottom, ipCSC, m_confWinLeft, m_confWinRight, m_confWinTop, m_confWinBottom, NUM_CHROMA_FORMAT, m_isTopFieldFirst );
      }

      const AccessUnit& auTop = *(iterBitstream++);
      const vector<UInt>& statsTop = writeAnnexB(bitstreamFile, auTop);
      rateStatsAccum(auTop, statsTop);

      const AccessUnit& auBottom = *(iterBitstream++);
      const vector<UInt>& statsBottom = writeAnnexB(bitstreamFile, auBottom);
      rateStatsAccum(auBottom, statsBottom);
    }
  }
  else
  {
    Int i;

    TComList<TComPicYuv*>::iterator iterPicYuvRec = m_cListPicYuvRec.end();
    list<AccessUnit>::const_iterator iterBitstream = accessUnits.begin();

    for ( i = 0; i < iNumEncoded; i++ )
    {
      --iterPicYuvRec;
    }

    for ( i = 0; i < iNumEncoded; i++ )
    {
      TComPicYuv*  pcPicYuvRec  = *(iterPicYuvRec++);
      if (!m_reconFileName.empty())
      {
        m_cTVideoIOYuvReconFile.write( pcPicYuvRec, ipCSC, m_confWinLeft, m_confWinRight, m_confWinTop, m_confWinBottom,
            NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range  );
      }

      const AccessUnit& au = *(iterBitstream++);
      const vector<UInt>& stats = writeAnnexB(bitstreamFile, au);
      rateStatsAccum(au, stats);
    }
  }
}

/**
 *
 */
Void TAppEncTop::rateStatsAccum(const AccessUnit& au, const std::vector<UInt>& annexBsizes)
{
  AccessUnit::const_iterator it_au = au.begin();
  vector<UInt>::const_iterator it_stats = annexBsizes.begin();

  for (; it_au != au.end(); it_au++, it_stats++)
  {
    switch ((*it_au)->m_nalUnitType)
    {
    case NAL_UNIT_CODED_SLICE_TRAIL_R:
    case NAL_UNIT_CODED_SLICE_TRAIL_N:
    case NAL_UNIT_CODED_SLICE_TSA_R:
    case NAL_UNIT_CODED_SLICE_TSA_N:
    case NAL_UNIT_CODED_SLICE_STSA_R:
    case NAL_UNIT_CODED_SLICE_STSA_N:
    case NAL_UNIT_CODED_SLICE_BLA_W_LP:
    case NAL_UNIT_CODED_SLICE_BLA_W_RADL:
    case NAL_UNIT_CODED_SLICE_BLA_N_LP:
    case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
    case NAL_UNIT_CODED_SLICE_IDR_N_LP:
    case NAL_UNIT_CODED_SLICE_CRA:
    case NAL_UNIT_CODED_SLICE_RADL_N:
    case NAL_UNIT_CODED_SLICE_RADL_R:
    case NAL_UNIT_CODED_SLICE_RASL_N:
    case NAL_UNIT_CODED_SLICE_RASL_R:
    case NAL_UNIT_VPS:
    case NAL_UNIT_SPS:
    case NAL_UNIT_PPS:
      m_essentialBytes += *it_stats;
      break;
    default:
      break;
    }

    m_totalBytes += *it_stats;
  }
}

Void TAppEncTop::printRateSummary()
{
  Double time = (Double) m_iFrameRcvd / m_iFrameRate * m_temporalSubsampleRatio;
  printf("Bytes written to file: %u (%.3f kbps)\n", m_totalBytes, 0.008 * m_totalBytes / time);
  if (m_summaryVerboseness > 0)
  {
    printf("Bytes for SPS/PPS/Slice (Incl. Annex B): %u (%.3f kbps)\n", m_essentialBytes, 0.008 * m_essentialBytes / time);
  }
}

Void TAppEncTop::printChromaFormat()
{
  std::cout << std::setw(43) << "Input ChromaFormatIDC = ";
  switch (m_InputChromaFormatIDC)
  {
  case CHROMA_400:  std::cout << "  4:0:0"; break;
  case CHROMA_420:  std::cout << "  4:2:0"; break;
  case CHROMA_422:  std::cout << "  4:2:2"; break;
  case CHROMA_444:  std::cout << "  4:4:4"; break;
  default:
    std::cerr << "Invalid";
    exit(1);
  }
  std::cout << std::endl;

  std::cout << std::setw(43) << "Output (internal) ChromaFormatIDC = ";
  switch (m_cTEncTop.getChromaFormatIdc())
  {
  case CHROMA_400:  std::cout << "  4:0:0"; break;
  case CHROMA_420:  std::cout << "  4:2:0"; break;
  case CHROMA_422:  std::cout << "  4:2:2"; break;
  case CHROMA_444:  std::cout << "  4:4:4"; break;
  default:
    std::cerr << "Invalid";
    exit(1);
  }
  std::cout << "\n" << std::endl;
}

//! \}
