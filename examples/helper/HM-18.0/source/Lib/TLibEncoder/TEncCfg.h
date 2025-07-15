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

/** \file     TEncCfg.h
    \brief    encoder configuration class (header)
*/

#ifndef __TENCCFG__
#define __TENCCFG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TLibCommon/CommonDef.h"
#include "TLibCommon/TComSlice.h"
#if JVET_T0050_ANNOTATED_REGIONS_SEI
#include "TLibCommon/SEI.h"
#endif
#include <assert.h>

struct GOPEntry
{
  Int m_POC;
  Int m_QPOffset;
  Double m_QPOffsetModelOffset;
  Double m_QPOffsetModelScale;
  Int m_CbQPoffset;
  Int m_CrQPoffset;
  Double m_QPFactor;
  Int m_tcOffsetDiv2;
  Int m_betaOffsetDiv2;
  Int m_temporalId;
  Bool m_refPic;
  Int m_numRefPicsActive;
  SChar m_sliceType;
  Int m_numRefPics;
  Int m_referencePics[MAX_NUM_REF_PICS];
  Int m_usedByCurrPic[MAX_NUM_REF_PICS];
  Int m_interRPSPrediction;
  Int m_deltaRPS;
  Int m_numRefIdc;
  Int m_refIdc[MAX_NUM_REF_PICS+1];
  Bool m_isEncoded;
  GOPEntry()
  : m_POC(-1)
  , m_QPOffset(0)
  , m_QPOffsetModelOffset(0)
  , m_QPOffsetModelScale(0)
  , m_CbQPoffset(0)
  , m_CrQPoffset(0)
  , m_QPFactor(0)
  , m_tcOffsetDiv2(0)
  , m_betaOffsetDiv2(0)
  , m_temporalId(0)
  , m_refPic(false)
  , m_numRefPicsActive(0)
  , m_sliceType('P')
  , m_numRefPics(0)
  , m_interRPSPrediction(false)
  , m_deltaRPS(0)
  , m_numRefIdc(0)
  , m_isEncoded(false)
  {
    ::memset( m_referencePics, 0, sizeof(m_referencePics) );
    ::memset( m_usedByCurrPic, 0, sizeof(m_usedByCurrPic) );
    ::memset( m_refIdc,        0, sizeof(m_refIdc) );
  }
};

std::istringstream &operator>>(std::istringstream &in, GOPEntry &entry);     //input
//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// encoder configuration class
class TEncCfg
{
public:

struct TEncSEIKneeFunctionInformation
{
  struct KneePointPair
  {
    Int inputKneePoint;
    Int outputKneePoint;
  };

  Int       m_kneeFunctionId;
  Bool      m_kneeFunctionCancelFlag;
  Bool      m_kneeFunctionPersistenceFlag;
  Int       m_inputDRange;
  Int       m_inputDispLuminance;
  Int       m_outputDRange;
  Int       m_outputDispLuminance;
  std::vector<KneePointPair> m_kneeSEIKneePointPairs;
};

#if JVET_T0050_ANNOTATED_REGIONS_SEI
  std::map<UInt, SEIAnnotatedRegions::AnnotatedRegionObject> m_arObjects;
#endif

protected:
  //==== File I/O ========
  Int       m_iFrameRate;
  Int       m_FrameSkip;
  UInt      m_temporalSubsampleRatio;
  Int       m_iSourceWidth;
  Int       m_iSourceHeight;
  Window    m_conformanceWindow;
  Int       m_framesToBeEncoded;
  Double    m_adLambdaModifier[ MAX_TLAYER ];
  std::vector<Double> m_adIntraLambdaModifier;
  Double    m_dIntraQpFactor;                                 ///< Intra Q Factor. If negative, use a default equation: 0.57*(1.0 - Clip3( 0.0, 0.5, 0.05*(Double)(isField ? (GopSize-1)/2 : GopSize-1) ))

  Bool      m_printMSEBasedSequencePSNR;
  Bool      m_printHexPsnr;
  Bool      m_printFrameMSE;
  Bool      m_printSequenceMSE;
  Bool      m_printMSSSIM;
  Bool      m_bXPSNREnableFlag;
  Double    m_dXPSNRWeight[MAX_NUM_COMPONENT];
  Bool      m_cabacZeroWordPaddingEnabled;
#if SHUTTER_INTERVAL_SEI_PROCESSING
  Bool      m_ShutterFilterEnable;                          ///< enable Pre-Filtering with Shutter Interval SEI
#endif

  /* profile & level */
  Profile::Name m_profile;
  Level::Tier   m_levelTier;
  Level::Name   m_level;
  Bool m_progressiveSourceFlag;
  Bool m_interlacedSourceFlag;
  Bool m_nonPackedConstraintFlag;
  Bool m_frameOnlyConstraintFlag;
  UInt              m_bitDepthConstraintValue;
  ChromaFormat      m_chromaFormatConstraintValue;
  Bool              m_intraConstraintFlag;
  Bool              m_onePictureOnlyConstraintFlag;
  Bool              m_lowerBitRateConstraintFlag;

  //====== Coding Structure ========
  UInt      m_uiIntraPeriod;                    // TODO: make this an Int - it can be -1!
  UInt      m_uiDecodingRefreshType;            ///< the type of decoding refresh employed for the random access.
  Bool      m_bReWriteParamSetsFlag;
  Int       m_iGOPSize;
  GOPEntry  m_GOPList[MAX_GOP];
  Int       m_extraRPSs;
  Int       m_maxDecPicBuffering[MAX_TLAYER];
  Int       m_numReorderPics[MAX_TLAYER];

  Int       m_iQP;                              //  if (AdaptiveQP == OFF)
  Int       m_intraQPOffset;                    ///< QP offset for intra slice (integer)
  Int       m_lambdaFromQPEnable;               ///< enable lambda derivation from QP
  Int       m_sourcePadding[2];

  Bool      m_AccessUnitDelimiter;               ///< add Access Unit Delimiter NAL units

  Int       m_iMaxRefPicNum;                     ///< this is used to mimic the sliding mechanism used by the decoder
                                                 // TODO: We need to have a common sliding mechanism used by both the encoder and decoder

  Int       m_maxTempLayer;                      ///< Max temporal layer
  Bool      m_useAMP;
  UInt      m_maxCUWidth;
  UInt      m_maxCUHeight;
  UInt      m_maxTotalCUDepth;
  UInt      m_log2DiffMaxMinCodingBlockSize;

  //======= Transform =============
  UInt      m_uiQuadtreeTULog2MaxSize;
  UInt      m_uiQuadtreeTULog2MinSize;
  UInt      m_uiQuadtreeTUMaxDepthInter;
  UInt      m_uiQuadtreeTUMaxDepthIntra;

  //====== Loop/Deblock Filter ========
  Bool      m_bLoopFilterDisable;
  Bool      m_loopFilterOffsetInPPS;
  Int       m_loopFilterBetaOffsetDiv2;
  Int       m_loopFilterTcOffsetDiv2;
  Int       m_deblockingFilterMetric;
  Bool      m_bUseSAO;
  Bool      m_bTestSAODisableAtPictureLevel;
  Double    m_saoEncodingRate;       // When non-0 SAO early picture termination is enabled for luma and chroma
  Double    m_saoEncodingRateChroma; // The SAO early picture termination rate to use for chroma (when m_SaoEncodingRate is >0). If <=0, use results for luma.
  Int       m_maxNumOffsetsPerPic;
  Bool      m_saoCtuBoundary;
  Bool      m_resetEncoderStateAfterIRAP;

  //====== Motion search ========
  Bool      m_bDisableIntraPUsInInterSlices;
  MESearchMethod m_motionEstimationSearchMethod;
  Int       m_iSearchRange;                     //  0:Full frame
  Int       m_bipredSearchRange;
  Bool      m_bClipForBiPredMeEnabled;
  Bool      m_bFastMEAssumingSmootherMVEnabled;
  Int       m_minSearchWindow;
  Bool      m_bRestrictMESampling;

  //====== Quality control ========
  Int       m_iMaxDeltaQP;                      //  Max. absolute delta QP (1:default)
  Int       m_iMaxCuDQPDepth;                   //  Max. depth for a minimum CuDQP (0:default)
  Int       m_diffCuChromaQpOffsetDepth;        ///< If negative, then do not apply chroma qp offsets.

  Int       m_chromaCbQpOffset;                 //  Chroma Cb QP Offset (0:default)
  Int       m_chromaCrQpOffset;                 //  Chroma Cr Qp Offset (0:default)
  WCGChromaQPControl m_wcgChromaQpControl;                    ///< Wide-colour-gamut chroma QP control.
  UInt      m_sliceChromaQpOffsetPeriodicity;                 ///< Used in conjunction with Slice Cb/Cr QpOffsetIntraOrPeriodic. Use 0 (default) to disable periodic nature.
  Int       m_sliceChromaQpOffsetIntraOrPeriodic[2/*Cb,Cr*/]; ///< Chroma Cb QP Offset at slice level for I slice or for periodic inter slices as defined by SliceChromaQPOffsetPeriodicity. Replaces offset in the GOP table.

  ChromaFormat m_chromaFormatIDC;

#if ADAPTIVE_QP_SELECTION
  Bool      m_bUseAdaptQpSelect;
#endif
  Bool      m_extendedPrecisionProcessingFlag;
  Bool      m_highPrecisionOffsetsEnabledFlag;
  Bool      m_bUseAdaptiveQP;
  Int       m_iQPAdaptationRange;

  //====== Tool list ========
  Int       m_bitDepth[MAX_NUM_CHANNEL_TYPE];
#if JVET_X0048_X0103_FILM_GRAIN
  Int       m_bitDepthInput[MAX_NUM_CHANNEL_TYPE];
#endif
  Bool      m_bUseASR;
  Bool      m_bUseHADME;
  Bool      m_useRDOQ;
  Bool      m_useRDOQTS;
  Bool      m_useSelectiveRDOQ;
  UInt      m_rdPenalty;
  FastInterSearchMode m_fastInterSearchMode;
  Bool      m_bUseEarlyCU;
  Bool      m_useFastDecisionForMerge;
  Bool      m_bUseCbfFastMode;
  Bool      m_useEarlySkipDetection;
  Bool      m_crossComponentPredictionEnabledFlag;
  Bool      m_reconBasedCrossCPredictionEstimate;
  UInt      m_log2SaoOffsetScale[MAX_NUM_CHANNEL_TYPE];
  Bool      m_useTransformSkip;
  Bool      m_useTransformSkipFast;
  UInt      m_log2MaxTransformSkipBlockSize;
  Bool      m_transformSkipRotationEnabledFlag;
  Bool      m_transformSkipContextEnabledFlag;
  Bool      m_persistentRiceAdaptationEnabledFlag;
  Bool      m_cabacBypassAlignmentEnabledFlag;
  Bool      m_rdpcmEnabledFlag[NUMBER_OF_RDPCM_SIGNALLING_MODES];
  LumaLevelToDeltaQPMapping m_lumaLevelToDeltaQPMapping; ///< mapping from luma level to delta QP.
  Int*      m_aidQP;
  UInt      m_uiDeltaQpRD;
  Bool      m_bFastDeltaQP;
#if JVET_V0078
  Bool      m_bSmoothQPReductionEnable;
  Double    m_dSmoothQPReductionThreshold;
  Double    m_dSmoothQPReductionModelScale;
  Double    m_dSmoothQPReductionModelOffset;
  Int       m_iSmoothQPReductionLimit;
  Int       m_iSmoothQPReductionPeriodicity;
#endif

  Bool      m_bUseConstrainedIntraPred;
  Bool      m_bFastUDIUseMPMEnabled;
  Bool      m_bFastMEForGenBLowDelayEnabled;
  Bool      m_bUseBLambdaForNonKeyLowDelayPictures;
  Bool      m_usePCM;
  Int       m_PCMBitDepth[MAX_NUM_CHANNEL_TYPE];
  UInt      m_pcmLog2MaxSize;
  UInt      m_uiPCMLog2MinSize;
  //====== Slice ========
  SliceConstraint m_sliceMode;
  Int       m_sliceArgument;
  //====== Dependent Slice ========
  SliceConstraint m_sliceSegmentMode;
  Int       m_sliceSegmentArgument;
  Bool      m_bLFCrossSliceBoundaryFlag;

  Bool      m_bPCMInputBitDepthFlag;
  Bool      m_bPCMFilterDisableFlag;
  Bool      m_intraSmoothingDisabledFlag;
  Bool      m_loopFilterAcrossTilesEnabledFlag;
  Bool      m_tileUniformSpacingFlag;
  Int       m_iNumColumnsMinus1;
  Int       m_iNumRowsMinus1;
  std::vector<Int> m_tileColumnWidth;
  std::vector<Int> m_tileRowHeight;

  Bool      m_entropyCodingSyncEnabledFlag;

  HashType  m_decodedPictureHashSEIType;
  Bool      m_bufferingPeriodSEIEnabled;
  Bool      m_pictureTimingSEIEnabled;
  Bool      m_recoveryPointSEIEnabled;
  Bool      m_toneMappingInfoSEIEnabled;
  Int       m_toneMapId;
  Bool      m_toneMapCancelFlag;
  Bool      m_toneMapPersistenceFlag;
  Int       m_codedDataBitDepth;
  Int       m_targetBitDepth;
  Int       m_modelId;
  Int       m_minValue;
  Int       m_maxValue;
  Int       m_sigmoidMidpoint;
  Int       m_sigmoidWidth;
  Int       m_numPivots;
  Int       m_cameraIsoSpeedIdc;
  Int       m_cameraIsoSpeedValue;
  Int       m_exposureIndexIdc;
  Int       m_exposureIndexValue;
  Bool      m_exposureCompensationValueSignFlag;
  Int       m_exposureCompensationValueNumerator;
  Int       m_exposureCompensationValueDenomIdc;
  Int       m_refScreenLuminanceWhite;
  Int       m_extendedRangeWhiteLevel;
  Int       m_nominalBlackLevelLumaCodeValue;
  Int       m_nominalWhiteLevelLumaCodeValue;
  Int       m_extendedWhiteLevelLumaCodeValue;
  Int*      m_startOfCodedInterval;
  Int*      m_codedPivotValue;
  Int*      m_targetPivotValue;
  Bool      m_framePackingSEIEnabled;
  Int       m_framePackingSEIType;
  Int       m_framePackingSEIId;
  Int       m_framePackingSEIQuincunx;
  Int       m_framePackingSEIInterpretation;
  Bool      m_segmentedRectFramePackingSEIEnabled;
  Bool      m_segmentedRectFramePackingSEICancel;
  Int       m_segmentedRectFramePackingSEIType;
  Bool      m_segmentedRectFramePackingSEIPersistence;
  Int       m_displayOrientationSEIAngle;
  Bool      m_temporalLevel0IndexSEIEnabled;
  Bool      m_gradualDecodingRefreshInfoEnabled;
  Int       m_noDisplaySEITLayer;
  Bool      m_decodingUnitInfoSEIEnabled;
  Bool      m_SOPDescriptionSEIEnabled;
  Bool      m_scalableNestingSEIEnabled;
  Bool      m_tmctsSEIEnabled;
#if MCTS_ENC_CHECK
  Bool      m_tmctsSEITileConstraint;
#endif
#if MCTS_EXTRACTION
  Bool      m_tmctsExtractionSEIEnabled;
#endif
  Bool      m_timeCodeSEIEnabled;
  Int       m_timeCodeSEINumTs;
  TComSEITimeSet   m_timeSetArray[MAX_TIMECODE_SEI_SETS];
  Bool      m_kneeSEIEnabled;
  TEncSEIKneeFunctionInformation m_kneeFunctionInformationSEI;
  std::string m_colourRemapSEIFileRoot;          ///< SEI Colour Remapping File (initialized from external file)
  TComSEIMasteringDisplay m_masteringDisplay;
  Bool      m_alternativeTransferCharacteristicsSEIEnabled;
  UChar     m_preferredTransferCharacteristics;
  Bool      m_greenMetadataInfoSEIEnabled;
  UChar     m_greenMetadataType;
  UChar     m_xsdMetricType;
  Bool      m_ccvSEIEnabled;
  Bool      m_ccvSEICancelFlag;
  Bool      m_ccvSEIPersistenceFlag;
  Bool      m_ccvSEIPrimariesPresentFlag;
  Bool      m_ccvSEIMinLuminanceValuePresentFlag;
  Bool      m_ccvSEIMaxLuminanceValuePresentFlag;
  Bool      m_ccvSEIAvgLuminanceValuePresentFlag;
  Double    m_ccvSEIPrimariesX[MAX_NUM_COMPONENT]; 
  Double    m_ccvSEIPrimariesY[MAX_NUM_COMPONENT];
  Double    m_ccvSEIMinLuminanceValue;
  Double    m_ccvSEIMaxLuminanceValue;
  Double    m_ccvSEIAvgLuminanceValue;
  Bool      m_erpSEIEnabled;          
  Bool      m_erpSEICancelFlag;
  Bool      m_erpSEIPersistenceFlag;
  Bool      m_erpSEIGuardBandFlag;
  UInt      m_erpSEIGuardBandType;
  UInt      m_erpSEILeftGuardBandWidth;
  UInt      m_erpSEIRightGuardBandWidth;
  Bool      m_sphereRotationSEIEnabled;          
  Bool      m_sphereRotationSEICancelFlag;
  Bool      m_sphereRotationSEIPersistenceFlag;
  Int       m_sphereRotationSEIYaw;
  Int       m_sphereRotationSEIPitch;
  Int       m_sphereRotationSEIRoll;
  Bool      m_omniViewportSEIEnabled;          
  UInt      m_omniViewportSEIId;
  Bool      m_omniViewportSEICancelFlag;
  Bool      m_omniViewportSEIPersistenceFlag;
  UInt      m_omniViewportSEICntMinus1;
  std::vector<Int>  m_omniViewportSEIAzimuthCentre;
  std::vector<Int>  m_omniViewportSEIElevationCentre;
  std::vector<Int>  m_omniViewportSEITiltCentre;
  std::vector<UInt> m_omniViewportSEIHorRange;
  std::vector<UInt> m_omniViewportSEIVerRange; 
  Bool      m_gopBasedTemporalFilterEnabled;
#if JVET_Y0077_BIM
  Bool                  m_bimEnabled;
  std::map<Int, Int*>   m_adaptQPmap;
#endif
  Bool                  m_cmpSEIEnabled;
  Bool                  m_cmpSEICmpCancelFlag;
  Bool                  m_cmpSEICmpPersistenceFlag;
  Bool                  m_rwpSEIEnabled;
  Bool                  m_rwpSEIRwpCancelFlag;
  Bool                  m_rwpSEIRwpPersistenceFlag;
  Bool                  m_rwpSEIConstituentPictureMatchingFlag;
  Int                   m_rwpSEINumPackedRegions;
  Int                   m_rwpSEIProjPictureWidth;
  Int                   m_rwpSEIProjPictureHeight;
  Int                   m_rwpSEIPackedPictureWidth;
  Int                   m_rwpSEIPackedPictureHeight;
  std::vector<UChar>    m_rwpSEIRwpTransformType;
  std::vector<Bool>     m_rwpSEIRwpGuardBandFlag;
  std::vector<UInt>     m_rwpSEIProjRegionWidth;
  std::vector<UInt>     m_rwpSEIProjRegionHeight;
  std::vector<UInt>     m_rwpSEIRwpSEIProjRegionTop;
  std::vector<UInt>     m_rwpSEIProjRegionLeft;
  std::vector<UShort>   m_rwpSEIPackedRegionWidth;
  std::vector<UShort>   m_rwpSEIPackedRegionHeight;
  std::vector<UShort>   m_rwpSEIPackedRegionTop;
  std::vector<UShort>   m_rwpSEIPackedRegionLeft;
  std::vector<UChar>    m_rwpSEIRwpLeftGuardBandWidth;
  std::vector<UChar>    m_rwpSEIRwpRightGuardBandWidth;
  std::vector<UChar>    m_rwpSEIRwpTopGuardBandHeight;
  std::vector<UChar>    m_rwpSEIRwpBottomGuardBandHeight;
  std::vector<Bool>     m_rwpSEIRwpGuardBandNotUsedForPredFlag;
  std::vector<UChar>    m_rwpSEIRwpGuardBandType;
  std::string           m_arSEIFileRoot;  // Annotated region SEI - initialized from external file
  Bool                    m_fviSEIEnabled;
  TComSEIFisheyeVideoInfo m_fisheyeVideoInfo;
  std::string m_regionalNestingSEIFileRoot;  // Regional nesting SEI - initialized from external file
#if SHUTTER_INTERVAL_SEI_MESSAGE
  Bool                    m_siiSEIEnabled;
  UInt                    m_siiSEINumUnitsInShutterInterval;
  UInt                    m_siiSEITimeScale;
  std::vector<UInt>       m_siiSEISubLayerNumUnitsInSI;
#endif
#if SEI_ENCODER_CONTROL
  // film grain characterstics sei
  Bool      m_fgcSEIEnabled;
  Bool      m_fgcSEICancelFlag;
  Bool      m_fgcSEIPersistenceFlag;
  UChar     m_fgcSEIModelID;
  Bool      m_fgcSEISepColourDescPresentFlag;
  UChar     m_fgcSEIBlendingModeID;
  UChar     m_fgcSEILog2ScaleFactor;
  Bool      m_fgcSEICompModelPresent[MAX_NUM_COMPONENT];
#if JVET_X0048_X0103_FILM_GRAIN
  Bool      m_fgcSEIAnalysisEnabled;
  std::string m_fgcSEIExternalMask;
  std::string m_fgcSEIExternalDenoised;
  Bool      m_fgcSEIPerPictureSEI;
  UChar     m_fgcSEINumIntensityIntervalMinus1[MAX_NUM_COMPONENT];
  UChar     m_fgcSEINumModelValuesMinus1[MAX_NUM_COMPONENT];
  UChar     m_fgcSEIIntensityIntervalLowerBound[MAX_NUM_COMPONENT][FG_MAX_NUM_INTENSITIES];
  UChar     m_fgcSEIIntensityIntervalUpperBound[MAX_NUM_COMPONENT][FG_MAX_NUM_INTENSITIES];
  UInt      m_fgcSEICompModelValue[MAX_NUM_COMPONENT][FG_MAX_NUM_INTENSITIES][FG_MAX_NUM_MODEL_VALUES];
#endif
  // content light level SEI
  Bool      m_cllSEIEnabled;
  UShort    m_cllSEIMaxContentLevel;
  UShort    m_cllSEIMaxPicAvgLevel;
  // ambient viewing environment sei
  Bool      m_aveSEIEnabled;
  UInt      m_aveSEIAmbientIlluminance;
  UShort    m_aveSEIAmbientLightX;
  UShort    m_aveSEIAmbientLightY;
  #endif
  //====== Weighted Prediction ========
  Bool      m_useWeightedPred;       //< Use of Weighting Prediction (P_SLICE)
  Bool      m_useWeightedBiPred;    //< Use of Bi-directional Weighting Prediction (B_SLICE)
  WeightedPredictionMethod m_weightedPredictionMethod;
  UInt      m_log2ParallelMergeLevelMinus2;       ///< Parallel merge estimation region
  UInt      m_maxNumMergeCand;                    ///< Maximum number of merge candidates
  ScalingListMode m_useScalingListId;             ///< Using quantization matrix i.e. 0=off, 1=default, 2=file.
  std::string m_scalingListFileName;              ///< quantization matrix file name
  Int       m_TMVPModeId;
  Bool      m_SignDataHidingEnabledFlag;
  Bool      m_RCEnableRateControl;
  Int       m_RCTargetBitrate;
  Int       m_RCKeepHierarchicalBit;
  Bool      m_RCLCULevelRC;
  Bool      m_RCUseLCUSeparateModel;
  Int       m_RCInitialQP;
  Bool      m_RCForceIntraQP;
  Bool      m_RCCpbSaturationEnabled;
  UInt      m_RCCpbSize;
  Double    m_RCInitialCpbFullness;
  Bool      m_TransquantBypassEnabledFlag;                    ///< transquant_bypass_enabled_flag setting in PPS.
  Bool      m_CUTransquantBypassFlagForce;                    ///< if transquant_bypass_enabled_flag, then, if true, all CU transquant bypass flags will be set to true.

  CostMode  m_costMode;                                       ///< The cost function to use, primarily when considering lossless coding.

  TComVPS   m_cVPS;
  Bool      m_recalculateQPAccordingToLambda;                 ///< recalculate QP value according to the lambda value
  Int       m_activeParameterSetsSEIEnabled;                  ///< enable active parameter set SEI message
  Bool      m_vuiParametersPresentFlag;                       ///< enable generation of VUI parameters
  Bool      m_aspectRatioInfoPresentFlag;                     ///< Signals whether aspect_ratio_idc is present
  Bool      m_chromaResamplingFilterHintEnabled;              ///< Signals whether chroma sampling filter hint data is present
  Int       m_chromaResamplingHorFilterIdc;                   ///< Specifies the Index of filter to use
  Int       m_chromaResamplingVerFilterIdc;                   ///< Specifies the Index of filter to use
  Int       m_aspectRatioIdc;                                 ///< aspect_ratio_idc
  Int       m_sarWidth;                                       ///< horizontal size of the sample aspect ratio
  Int       m_sarHeight;                                      ///< vertical size of the sample aspect ratio
  Bool      m_overscanInfoPresentFlag;                        ///< Signals whether overscan_appropriate_flag is present
  Bool      m_overscanAppropriateFlag;                        ///< Indicates whether conformant decoded pictures are suitable for display using overscan
  Bool      m_videoSignalTypePresentFlag;                     ///< Signals whether video_format, video_full_range_flag, and colour_description_present_flag are present
  Int       m_videoFormat;                                    ///< Indicates representation of pictures
  Bool      m_videoFullRangeFlag;                             ///< Indicates the black level and range of luma and chroma signals
  Bool      m_colourDescriptionPresentFlag;                   ///< Signals whether colour_primaries, transfer_characteristics and matrix_coefficients are present
  Int       m_colourPrimaries;                                ///< Indicates chromaticity coordinates of the source primaries
  Int       m_transferCharacteristics;                        ///< Indicates the opto-electronic transfer characteristics of the source
  Int       m_matrixCoefficients;                             ///< Describes the matrix coefficients used in deriving luma and chroma from RGB primaries
  Bool      m_chromaLocInfoPresentFlag;                       ///< Signals whether chroma_sample_loc_type_top_field and chroma_sample_loc_type_bottom_field are present
  Int       m_chromaSampleLocTypeTopField;                    ///< Specifies the location of chroma samples for top field
  Int       m_chromaSampleLocTypeBottomField;                 ///< Specifies the location of chroma samples for bottom field
  Bool      m_neutralChromaIndicationFlag;                    ///< Indicates that the value of all decoded chroma samples is equal to 1<<(BitDepthCr-1)
  Window    m_defaultDisplayWindow;                           ///< Represents the default display window parameters
  Bool      m_frameFieldInfoPresentFlag;                      ///< Indicates that pic_struct and other field coding related values are present in picture timing SEI messages
  Bool      m_pocProportionalToTimingFlag;                    ///< Indicates that the POC value is proportional to the output time w.r.t. first picture in CVS
  Int       m_numTicksPocDiffOneMinus1;                       ///< Number of ticks minus 1 that for a POC difference of one
  Bool      m_bitstreamRestrictionFlag;                       ///< Signals whether bitstream restriction parameters are present
  Bool      m_tilesFixedStructureFlag;                        ///< Indicates that each active picture parameter set has the same values of the syntax elements related to tiles
  Bool      m_motionVectorsOverPicBoundariesFlag;             ///< Indicates that no samples outside the picture boundaries are used for inter prediction
  Int       m_minSpatialSegmentationIdc;                      ///< Indicates the maximum size of the spatial segments in the pictures in the coded video sequence
  Int       m_maxBytesPerPicDenom;                            ///< Indicates a number of bytes not exceeded by the sum of the sizes of the VCL NAL units associated with any coded picture
  Int       m_maxBitsPerMinCuDenom;                           ///< Indicates an upper bound for the number of bits of coding_unit() data
  Int       m_log2MaxMvLengthHorizontal;                      ///< Indicate the maximum absolute value of a decoded horizontal MV component in quarter-pel luma units
  Int       m_log2MaxMvLengthVertical;                        ///< Indicate the maximum absolute value of a decoded vertical MV component in quarter-pel luma units

  Bool      m_useStrongIntraSmoothing;                        ///< enable the use of strong intra smoothing (bi_linear interpolation) for 32x32 blocks when reference samples are flat.
  Bool      m_bEfficientFieldIRAPEnabled;                     ///< enable to code fields in a specific, potentially more efficient, order.
  Bool      m_bHarmonizeGopFirstFieldCoupleEnabled;

  std::string m_summaryOutFilename;                           ///< filename to use for producing summary output file.
  std::string m_summaryPicFilenameBase;                       ///< Base filename to use for producing summary picture output files. The actual filenames used will have I.txt, P.txt and B.txt appended.
  UInt        m_summaryVerboseness;                           ///< Specifies the level of the verboseness of the text output.

#if JCTVC_AD0021_SEI_MANIFEST
  Bool        m_SEIManifestSEIEnabled;
#endif
#if JCTVC_AD0021_SEI_PREFIX_INDICATION
  Bool        m_SEIPrefixIndicationSEIEnabled;
#endif

public:
  TEncCfg()
  : m_tileColumnWidth()
  , m_tileRowHeight()
  {
    m_PCMBitDepth[CHANNEL_TYPE_LUMA]=8;
    m_PCMBitDepth[CHANNEL_TYPE_CHROMA]=8;
  }

  virtual ~TEncCfg()
  {}

  Void setProfile(Profile::Name profile) { m_profile = profile; }
  Void setLevel(Level::Tier tier, Level::Name level) { m_levelTier = tier; m_level = level; }

  Void      setFrameRate                    ( Int   i )      { m_iFrameRate = i; }
  Void      setFrameSkip                    ( UInt  i )      { m_FrameSkip = i; }
  Void      setTemporalSubsampleRatio       ( UInt  i )      { m_temporalSubsampleRatio = i; }
  Void      setSourceWidth                  ( Int   i )      { m_iSourceWidth = i; }
  Void      setSourceHeight                 ( Int   i )      { m_iSourceHeight = i; }

  Window   &getConformanceWindow()                           { return m_conformanceWindow; }
  Void      setConformanceWindow (Int confLeft, Int confRight, Int confTop, Int confBottom ) { m_conformanceWindow.setWindow (confLeft, confRight, confTop, confBottom); }

  Void      setFramesToBeEncoded            ( Int   i )      { m_framesToBeEncoded = i; }

  Bool      getPrintMSEBasedSequencePSNR    ()         const { return m_printMSEBasedSequencePSNR;  }
  Void      setPrintMSEBasedSequencePSNR    (Bool value)     { m_printMSEBasedSequencePSNR = value; }

  Bool      getPrintHexPsnr                 ()         const { return m_printHexPsnr;               }
  Void      setPrintHexPsnr                 (Bool value)     { m_printHexPsnr = value;              }

  Bool      getPrintFrameMSE                ()         const { return m_printFrameMSE;              }
  Void      setPrintFrameMSE                (Bool value)     { m_printFrameMSE = value;             }

  Bool      getPrintSequenceMSE             ()         const { return m_printSequenceMSE;           }
  Void      setPrintSequenceMSE             (Bool value)     { m_printSequenceMSE = value;          }

  Bool      getPrintMSSSIM                  ()         const { return m_printMSSSIM;               }
  Void      setPrintMSSSIM                  (Bool value)     { m_printMSSSIM = value;              }

  Bool      getXPSNREnableFlag              () const                     { return m_bXPSNREnableFlag;}
  Double    getXPSNRWeight                  (const ComponentID id) const { return m_dXPSNRWeight[id];}

  Void      setXPSNREnableFlag              ( Bool  i )      { m_bXPSNREnableFlag = i; }
  Void      setXPSNRWeight                  ( Double dValue, ComponentID id) { m_dXPSNRWeight[id] = dValue;}

  Bool      getCabacZeroWordPaddingEnabled()           const { return m_cabacZeroWordPaddingEnabled;  }
  Void      setCabacZeroWordPaddingEnabled(Bool value)       { m_cabacZeroWordPaddingEnabled = value; }

#if SHUTTER_INTERVAL_SEI_PROCESSING
  Bool      getShutterFilterFlag()              const { return m_ShutterFilterEnable; }
  Void      setShutterFilterFlag(Bool value)    { m_ShutterFilterEnable = value; }
#endif

  //====== Coding Structure ========
  Void      setIntraPeriod                  ( Int   i )      { m_uiIntraPeriod = (UInt)i; }
  Void      setDecodingRefreshType          ( Int   i )      { m_uiDecodingRefreshType = (UInt)i; }
  Void      setReWriteParamSetsFlag         ( Bool  b )      { m_bReWriteParamSetsFlag = b; }
  Void      setGOPSize                      ( Int   i )      { m_iGOPSize = i; }
  Void      setGopList                      ( const GOPEntry GOPList[MAX_GOP] ) {  for ( Int i = 0; i < MAX_GOP; i++ ) m_GOPList[i] = GOPList[i]; }
  Void      setExtraRPSs                    ( Int   i )      { m_extraRPSs = i; }
  const GOPEntry &getGOPEntry               ( Int   i ) const { return m_GOPList[i]; }
  Void      setEncodedFlag                  ( Int  i, Bool value )  { m_GOPList[i].m_isEncoded = value; }
  Void      setMaxDecPicBuffering           ( UInt u, UInt tlayer ) { m_maxDecPicBuffering[tlayer] = u;    }
  Void      setNumReorderPics               ( Int  i, UInt tlayer ) { m_numReorderPics[tlayer] = i;    }

  Void      setQP                           ( Int   i )      { m_iQP = i; }
  Void      setIntraQPOffset                ( Int   i )         { m_intraQPOffset = i; }
  Void      setLambdaFromQPEnable           ( Bool  b )         { m_lambdaFromQPEnable = b; }
  Void      setSourcePadding                ( Int*  padding )   { for ( Int i = 0; i < 2; i++ ) m_sourcePadding[i] = padding[i]; }

  Int       getMaxRefPicNum                 ()                              { return m_iMaxRefPicNum;           }
  Void      setMaxRefPicNum                 ( Int iMaxRefPicNum )           { m_iMaxRefPicNum = iMaxRefPicNum;  }

  Int       getMaxTempLayer                 ()                              { return m_maxTempLayer;              } 
  Void      setMaxTempLayer                 ( Int maxTempLayer )            { m_maxTempLayer = maxTempLayer;      }
  Void      setMaxCUWidth                   ( UInt  u )      { m_maxCUWidth  = u; }
  Void      setMaxCUHeight                  ( UInt  u )      { m_maxCUHeight = u; }
  Void      setMaxTotalCUDepth              ( UInt  u )      { m_maxTotalCUDepth = u; }
  Void      setLog2DiffMaxMinCodingBlockSize( UInt  u )      { m_log2DiffMaxMinCodingBlockSize = u; }

  //======== Transform =============
  Void      setQuadtreeTULog2MaxSize        ( UInt  u )      { m_uiQuadtreeTULog2MaxSize = u; }
  Void      setQuadtreeTULog2MinSize        ( UInt  u )      { m_uiQuadtreeTULog2MinSize = u; }
  Void      setQuadtreeTUMaxDepthInter      ( UInt  u )      { m_uiQuadtreeTUMaxDepthInter = u; }
  Void      setQuadtreeTUMaxDepthIntra      ( UInt  u )      { m_uiQuadtreeTUMaxDepthIntra = u; }

  Void setUseAMP( Bool b ) { m_useAMP = b; }

  //====== Loop/Deblock Filter ========
  Void      setLoopFilterDisable            ( Bool  b )      { m_bLoopFilterDisable        = b; }
  Void      setLoopFilterOffsetInPPS        ( Bool  b )      { m_loopFilterOffsetInPPS     = b; }
  Void      setLoopFilterBetaOffset         ( Int   i )      { m_loopFilterBetaOffsetDiv2  = i; }
  Void      setLoopFilterTcOffset           ( Int   i )      { m_loopFilterTcOffsetDiv2    = i; }
  Void      setDeblockingFilterMetric       ( Int   i )      { m_deblockingFilterMetric    = i; }

  //====== Motion search ========
  Void      setDisableIntraPUsInInterSlices ( Bool  b )      { m_bDisableIntraPUsInInterSlices = b; }
  Void      setMotionEstimationSearchMethod ( MESearchMethod e ) { m_motionEstimationSearchMethod = e; }
  Void      setSearchRange                  ( Int   i )      { m_iSearchRange = i; }
  Void      setBipredSearchRange            ( Int   i )      { m_bipredSearchRange = i; }
  Void      setClipForBiPredMeEnabled       ( Bool  b )      { m_bClipForBiPredMeEnabled = b; }
  Void      setFastMEAssumingSmootherMVEnabled ( Bool b )    { m_bFastMEAssumingSmootherMVEnabled = b; }
  Void      setMinSearchWindow              ( Int   i )      { m_minSearchWindow = i; }
  Void      setRestrictMESampling           ( Bool  b )      { m_bRestrictMESampling = b; }

  //====== Quality control ========
  Void      setMaxDeltaQP                   ( Int   i )      { m_iMaxDeltaQP = i; }
  Void      setMaxCuDQPDepth                ( Int   i )      { m_iMaxCuDQPDepth = i; }

  Int       getDiffCuChromaQpOffsetDepth    ()         const { return m_diffCuChromaQpOffsetDepth;  }
  Void      setDiffCuChromaQpOffsetDepth    (Int value)      { m_diffCuChromaQpOffsetDepth = value; }

  Void      setChromaCbQpOffset             ( Int   i )      { m_chromaCbQpOffset = i; }
  Void      setChromaCrQpOffset             ( Int   i )      { m_chromaCrQpOffset = i; }
  Void      setWCGChromaQpControl           ( const WCGChromaQPControl &ctrl )     { m_wcgChromaQpControl = ctrl; }
  const WCGChromaQPControl &getWCGChromaQPControl () const { return m_wcgChromaQpControl; }
  Void      setSliceChromaOffsetQpIntraOrPeriodic( UInt periodicity, Int sliceChromaQpOffsetIntraOrPeriodic[2]) { m_sliceChromaQpOffsetPeriodicity = periodicity; memcpy(m_sliceChromaQpOffsetIntraOrPeriodic, sliceChromaQpOffsetIntraOrPeriodic, sizeof(m_sliceChromaQpOffsetIntraOrPeriodic)); }
  Int       getSliceChromaOffsetQpIntraOrPeriodic( Bool bIsCr) const                                            { return m_sliceChromaQpOffsetIntraOrPeriodic[bIsCr?1:0]; }
  UInt      getSliceChromaOffsetQpPeriodicity() const                                                           { return m_sliceChromaQpOffsetPeriodicity; }

  Void      setChromaFormatIdc              ( ChromaFormat cf ) { m_chromaFormatIDC = cf; }
  ChromaFormat  getChromaFormatIdc          ( )              { return m_chromaFormatIDC; }

  Void      setLumaLevelToDeltaQPControls( const LumaLevelToDeltaQPMapping &lumaLevelToDeltaQPMapping ) { m_lumaLevelToDeltaQPMapping=lumaLevelToDeltaQPMapping; }
  const LumaLevelToDeltaQPMapping& getLumaLevelToDeltaQPMapping() const { return m_lumaLevelToDeltaQPMapping; }

#if ADAPTIVE_QP_SELECTION
  Void      setUseAdaptQpSelect             ( Bool   i ) { m_bUseAdaptQpSelect    = i; }
  Bool      getUseAdaptQpSelect             ()           { return   m_bUseAdaptQpSelect; }
#endif

#if JVET_V0078
  Bool      getSmoothQPReductionEnable       () const        { return m_bSmoothQPReductionEnable; }
  void      setSmoothQPReductionEnable       (Bool value)    { m_bSmoothQPReductionEnable = value; }
  Double    getSmoothQPReductionThreshold    () const        { return m_dSmoothQPReductionThreshold; }
  void      setSmoothQPReductionThreshold    (Double value)  { m_dSmoothQPReductionThreshold = value; }
  Double    getSmoothQPReductionModelScale   () const        { return m_dSmoothQPReductionModelScale; }
  void      setSmoothQPReductionModelScale   (Double value)  { m_dSmoothQPReductionModelScale = value; }
  Double    getSmoothQPReductionModelOffset  () const        { return m_dSmoothQPReductionModelOffset; }
  void      setSmoothQPReductionModelOffset  (Double value)  { m_dSmoothQPReductionModelOffset = value; }
  Int       getSmoothQPReductionLimit        ()              const { return m_iSmoothQPReductionLimit; }
  void      setSmoothQPReductionLimit        (Int value)     { m_iSmoothQPReductionLimit = value; }
  Int       getSmoothQPReductionPeriodicity  ()  const       { return m_iSmoothQPReductionPeriodicity; }
  void      setSmoothQPReductionPeriodicity  (Int value)     { m_iSmoothQPReductionPeriodicity = value; }
#endif

  Bool      getExtendedPrecisionProcessingFlag         ()         const { return m_extendedPrecisionProcessingFlag;  }
  Void      setExtendedPrecisionProcessingFlag         (Bool value)     { m_extendedPrecisionProcessingFlag = value; }

  Bool      getHighPrecisionOffsetsEnabledFlag() const { return m_highPrecisionOffsetsEnabledFlag; }
  Void      setHighPrecisionOffsetsEnabledFlag(Bool value) { m_highPrecisionOffsetsEnabledFlag = value; }

  Void      setUseAdaptiveQP                ( Bool  b )      { m_bUseAdaptiveQP = b; }
  Void      setQPAdaptationRange            ( Int   i )      { m_iQPAdaptationRange = i; }

  //====== Sequence ========
  Int       getFrameRate                    ()      { return  m_iFrameRate; }
  UInt      getFrameSkip                    ()      { return  m_FrameSkip; }
  UInt      getTemporalSubsampleRatio       ()      { return  m_temporalSubsampleRatio; }
  Int       getSourceWidth                  ()      { return  m_iSourceWidth; }
  Int       getSourceHeight                 ()      { return  m_iSourceHeight; }
  Int       getFramesToBeEncoded            ()      { return  m_framesToBeEncoded; }
  
  //====== Lambda Modifiers ========
  Void      setLambdaModifier               ( UInt uiIndex, Double dValue ) { m_adLambdaModifier[ uiIndex ] = dValue; }
  Double    getLambdaModifier               ( UInt uiIndex )          const { return m_adLambdaModifier[ uiIndex ]; }
  Void      setIntraLambdaModifier          ( const std::vector<Double> &dValue )               { m_adIntraLambdaModifier = dValue;       }
  const std::vector<Double>& getIntraLambdaModifier()                        const { return m_adIntraLambdaModifier;         }
  Void      setIntraQpFactor                ( Double dValue )               { m_dIntraQpFactor = dValue;              }
  Double    getIntraQpFactor                ()                        const { return m_dIntraQpFactor;                }

  //==== Coding Structure ========
  UInt      getIntraPeriod                  ()      { return  m_uiIntraPeriod; }
  UInt      getDecodingRefreshType          ()      { return  m_uiDecodingRefreshType; }
  Bool      getReWriteParamSetsFlag         ()      { return m_bReWriteParamSetsFlag; }
  Int       getGOPSize                      ()      { return  m_iGOPSize; }
  Int       getMaxDecPicBuffering           (UInt tlayer) { return m_maxDecPicBuffering[tlayer]; }
  Int       getNumReorderPics               (UInt tlayer) { return m_numReorderPics[tlayer]; }
  Int       getIntraQPOffset                () const    { return  m_intraQPOffset; }
  Int       getLambdaFromQPEnable           () const    { return  m_lambdaFromQPEnable; }
protected:
  Int       getBaseQP                       () const { return  m_iQP; } // public should use getQPForPicture.
public:
  Int       getQPForPicture                 (const UInt gopIndex, const TComSlice *pSlice) const; // Function actually defined in TEncTop.cpp
  Int       getSourcePadding                ( Int i )  const  { assert (i < 2 ); return  m_sourcePadding[i]; }

  Bool      getAccessUnitDelimiter() const  { return m_AccessUnitDelimiter; }
  Void      setAccessUnitDelimiter(Bool val){ m_AccessUnitDelimiter = val; }

  //======== Transform =============
  UInt      getQuadtreeTULog2MaxSize        ()      const { return m_uiQuadtreeTULog2MaxSize; }
  UInt      getQuadtreeTULog2MinSize        ()      const { return m_uiQuadtreeTULog2MinSize; }
  UInt      getQuadtreeTUMaxDepthInter      ()      const { return m_uiQuadtreeTUMaxDepthInter; }
  UInt      getQuadtreeTUMaxDepthIntra      ()      const { return m_uiQuadtreeTUMaxDepthIntra; }

  //==== Loop/Deblock Filter ========
  Bool      getLoopFilterDisable            ()      { return  m_bLoopFilterDisable;       }
  Bool      getLoopFilterOffsetInPPS        ()      { return m_loopFilterOffsetInPPS; }
  Int       getLoopFilterBetaOffset         ()      { return m_loopFilterBetaOffsetDiv2; }
  Int       getLoopFilterTcOffset           ()      { return m_loopFilterTcOffsetDiv2; }
  Int       getDeblockingFilterMetric       ()      { return m_deblockingFilterMetric; }

  //==== Motion search ========
  Bool      getDisableIntraPUsInInterSlices    () const { return m_bDisableIntraPUsInInterSlices; }
  MESearchMethod getMotionEstimationSearchMethod ( ) const { return m_motionEstimationSearchMethod; }
  Int       getSearchRange                     () const { return m_iSearchRange; }
  Bool      getClipForBiPredMeEnabled          () const { return m_bClipForBiPredMeEnabled; }
  Bool      getFastMEAssumingSmootherMVEnabled () const { return m_bFastMEAssumingSmootherMVEnabled; }
  Int       getMinSearchWindow                 () const { return m_minSearchWindow; }
  Bool      getRestrictMESampling              () const { return m_bRestrictMESampling; }

  //==== Quality control ========
  Int       getMaxDeltaQP                   () const { return  m_iMaxDeltaQP; }
  Int       getMaxCuDQPDepth                () const { return  m_iMaxCuDQPDepth; }
  Bool      getUseAdaptiveQP                () const { return  m_bUseAdaptiveQP; }
  Int       getQPAdaptationRange            () const { return  m_iQPAdaptationRange; }
#if JVET_X0048_X0103_FILM_GRAIN
  int       getBitDepth(const ChannelType chType) const { return m_bitDepth[chType]; }
  int*      getBitDepth() { return m_bitDepth; }
  int       getBitDepthInput(const ChannelType chType) const { return m_bitDepthInput[chType]; }
  int*      getBitDepthInput() { return m_bitDepthInput; }
  Void      setBitDepthInput(const ChannelType chType, Int internalBitDepthForChannel) { m_bitDepthInput[chType] = internalBitDepthForChannel; }
#endif

  //==== Tool list ========
  Void      setBitDepth( const ChannelType chType, Int internalBitDepthForChannel ) { m_bitDepth[chType] = internalBitDepthForChannel; }
  Void      setUseASR                       ( Bool  b )     { m_bUseASR     = b; }
  Void      setUseHADME                     ( Bool  b )     { m_bUseHADME   = b; }
  Void      setUseRDOQ                      ( Bool  b )     { m_useRDOQ    = b; }
  Void      setUseRDOQTS                    ( Bool  b )     { m_useRDOQTS  = b; }
  Void      setUseSelectiveRDOQ             ( Bool b )      { m_useSelectiveRDOQ = b; }
  Void      setRDpenalty                    ( UInt  u )     { m_rdPenalty  = u; }
  Void      setFastInterSearchMode          ( FastInterSearchMode m ) { m_fastInterSearchMode = m; }
  Void      setUseEarlyCU                   ( Bool  b )     { m_bUseEarlyCU = b; }
  Void      setUseFastDecisionForMerge      ( Bool  b )     { m_useFastDecisionForMerge = b; }
  Void      setUseCbfFastMode               ( Bool  b )     { m_bUseCbfFastMode = b; }
  Void      setUseEarlySkipDetection        ( Bool  b )     { m_useEarlySkipDetection = b; }
  Void      setUseConstrainedIntraPred      ( Bool  b )     { m_bUseConstrainedIntraPred = b; }
  Void      setFastUDIUseMPMEnabled         ( Bool  b )     { m_bFastUDIUseMPMEnabled = b; }
  Void      setFastMEForGenBLowDelayEnabled ( Bool  b )     { m_bFastMEForGenBLowDelayEnabled = b; }
  Void      setUseBLambdaForNonKeyLowDelayPictures ( Bool b ) { m_bUseBLambdaForNonKeyLowDelayPictures = b; }

  Void      setPCMInputBitDepthFlag         ( Bool  b )     { m_bPCMInputBitDepthFlag = b; }
  Void      setPCMFilterDisableFlag         ( Bool  b )     {  m_bPCMFilterDisableFlag = b; }
  Void      setUsePCM                       ( Bool  b )     {  m_usePCM = b;               }
  Void      setPCMBitDepth( const ChannelType chType, Int pcmBitDepthForChannel ) { m_PCMBitDepth[chType] = pcmBitDepthForChannel; }
  Void      setPCMLog2MaxSize               ( UInt u )      { m_pcmLog2MaxSize = u;      }
  Void      setPCMLog2MinSize               ( UInt u )     { m_uiPCMLog2MinSize = u;      }
  Void      setdQPs                         ( Int*  p )     { m_aidQP       = p; }
  Void      setDeltaQpRD                    ( UInt  u )     {m_uiDeltaQpRD  = u; }
  Void      setFastDeltaQp                  ( Bool  b )     {m_bFastDeltaQP = b; }
  Bool      getUseASR                       ()      { return m_bUseASR;     }
  Bool      getUseHADME                     ()      { return m_bUseHADME;   }
  Bool      getUseRDOQ                      ()      { return m_useRDOQ;    }
  Bool      getUseRDOQTS                    ()      { return m_useRDOQTS;  }
  Bool      getUseSelectiveRDOQ             ()      { return m_useSelectiveRDOQ; }
  Int       getRDpenalty                    ()      { return m_rdPenalty;  }
  FastInterSearchMode getFastInterSearchMode() const{ return m_fastInterSearchMode;  }
  Bool      getUseEarlyCU                   ()      { return m_bUseEarlyCU; }
  Bool      getUseFastDecisionForMerge      ()      { return m_useFastDecisionForMerge; }
  Bool      getUseCbfFastMode               ()      { return m_bUseCbfFastMode; }
  Bool      getUseEarlySkipDetection        ()      { return m_useEarlySkipDetection; }
  Bool      getUseConstrainedIntraPred      ()      { return m_bUseConstrainedIntraPred; }
  Bool      getFastUDIUseMPMEnabled         ()      { return m_bFastUDIUseMPMEnabled; }
  Bool      getFastMEForGenBLowDelayEnabled ()      { return m_bFastMEForGenBLowDelayEnabled; }
  Bool      getUseBLambdaForNonKeyLowDelayPictures () { return m_bUseBLambdaForNonKeyLowDelayPictures; }
  Bool      getPCMInputBitDepthFlag         ()      { return m_bPCMInputBitDepthFlag;   }
  Bool      getPCMFilterDisableFlag         ()      { return m_bPCMFilterDisableFlag;   }
  Bool      getUsePCM                       ()      { return m_usePCM;                 }
  UInt      getPCMLog2MaxSize               ()      { return m_pcmLog2MaxSize;  }
  UInt      getPCMLog2MinSize               ()      { return  m_uiPCMLog2MinSize;  }

  Bool      getCrossComponentPredictionEnabledFlag     ()                const { return m_crossComponentPredictionEnabledFlag;   }
  Void      setCrossComponentPredictionEnabledFlag     (const Bool value)      { m_crossComponentPredictionEnabledFlag = value;  }
  Bool      getUseReconBasedCrossCPredictionEstimate ()                const { return m_reconBasedCrossCPredictionEstimate;  }
  Void      setUseReconBasedCrossCPredictionEstimate (const Bool value)      { m_reconBasedCrossCPredictionEstimate = value; }
  Void      setLog2SaoOffsetScale(ChannelType type, UInt uiBitShift)         { m_log2SaoOffsetScale[type] = uiBitShift; }

  Bool getUseTransformSkip                             ()      { return m_useTransformSkip;        }
  Void setUseTransformSkip                             ( Bool b ) { m_useTransformSkip  = b;       }
  Bool getTransformSkipRotationEnabledFlag             ()            const { return m_transformSkipRotationEnabledFlag;  }
  Void setTransformSkipRotationEnabledFlag             (const Bool value)  { m_transformSkipRotationEnabledFlag = value; }
  Bool getTransformSkipContextEnabledFlag              ()            const { return m_transformSkipContextEnabledFlag;  }
  Void setTransformSkipContextEnabledFlag              (const Bool value)  { m_transformSkipContextEnabledFlag = value; }
  Bool getPersistentRiceAdaptationEnabledFlag          ()                 const { return m_persistentRiceAdaptationEnabledFlag;  }
  Void setPersistentRiceAdaptationEnabledFlag          (const Bool value)       { m_persistentRiceAdaptationEnabledFlag = value; }
  Bool getCabacBypassAlignmentEnabledFlag              ()       const      { return m_cabacBypassAlignmentEnabledFlag;  }
  Void setCabacBypassAlignmentEnabledFlag              (const Bool value)  { m_cabacBypassAlignmentEnabledFlag = value; }
  Bool getRdpcmEnabledFlag                             (const RDPCMSignallingMode signallingMode)        const      { return m_rdpcmEnabledFlag[signallingMode];  }
  Void setRdpcmEnabledFlag                             (const RDPCMSignallingMode signallingMode, const Bool value) { m_rdpcmEnabledFlag[signallingMode] = value; }
  Bool getUseTransformSkipFast                         ()      { return m_useTransformSkipFast;    }
  Void setUseTransformSkipFast                         ( Bool b ) { m_useTransformSkipFast  = b;   }
  UInt getLog2MaxTransformSkipBlockSize                () const      { return m_log2MaxTransformSkipBlockSize;     }
  Void setLog2MaxTransformSkipBlockSize                ( UInt u )    { m_log2MaxTransformSkipBlockSize  = u;       }
  Bool getIntraSmoothingDisabledFlag               ()      const { return m_intraSmoothingDisabledFlag; }
  Void setIntraSmoothingDisabledFlag               (Bool bValue) { m_intraSmoothingDisabledFlag=bValue; }

  const Int* getdQPs                        () const { return m_aidQP;       }
  UInt      getDeltaQpRD                    () const { return m_uiDeltaQpRD; }
  Bool      getFastDeltaQp                  () const { return m_bFastDeltaQP; }

  //====== Slice ========
  Void  setSliceMode                   ( SliceConstraint  i )        { m_sliceMode = i;              }
  Void  setSliceArgument               ( Int  i )                    { m_sliceArgument = i;          }
  SliceConstraint getSliceMode         () const                      { return m_sliceMode;           }
  Int   getSliceArgument               ()                            { return m_sliceArgument;       }
  //====== Dependent Slice ========
  Void  setSliceSegmentMode            ( SliceConstraint  i )        { m_sliceSegmentMode = i;       }
  Void  setSliceSegmentArgument        ( Int  i )                    { m_sliceSegmentArgument = i;   }
  SliceConstraint getSliceSegmentMode  () const                      { return m_sliceSegmentMode;    }
  Int   getSliceSegmentArgument        ()                            { return m_sliceSegmentArgument;}
  Void      setLFCrossSliceBoundaryFlag     ( Bool   bValue  )       { m_bLFCrossSliceBoundaryFlag = bValue; }
  Bool      getLFCrossSliceBoundaryFlag     ()                       { return m_bLFCrossSliceBoundaryFlag;   }

  Void      setUseSAO                  (Bool bVal)                   { m_bUseSAO = bVal; }
  Bool      getUseSAO                  ()                            { return m_bUseSAO; }
  Void  setTestSAODisableAtPictureLevel (Bool bVal)                  { m_bTestSAODisableAtPictureLevel = bVal; }
  Bool  getTestSAODisableAtPictureLevel ( ) const                    { return m_bTestSAODisableAtPictureLevel; }

  Void   setSaoEncodingRate(Double v)                                { m_saoEncodingRate = v; }
  Double getSaoEncodingRate() const                                  { return m_saoEncodingRate; }
  Void   setSaoEncodingRateChroma(Double v)                          { m_saoEncodingRateChroma = v; }
  Double getSaoEncodingRateChroma() const                            { return m_saoEncodingRateChroma; }
  Void  setMaxNumOffsetsPerPic                   (Int iVal)          { m_maxNumOffsetsPerPic = iVal; }
  Int   getMaxNumOffsetsPerPic                   ()                  { return m_maxNumOffsetsPerPic; }
  Void  setSaoCtuBoundary              (Bool val)                    { m_saoCtuBoundary = val; }
  Bool  getSaoCtuBoundary              ()                            { return m_saoCtuBoundary; }
  Void  setResetEncoderStateAfterIRAP(Bool b)                        { m_resetEncoderStateAfterIRAP = b; }
  Bool  getResetEncoderStateAfterIRAP() const                        { return m_resetEncoderStateAfterIRAP; }
  Void  setLFCrossTileBoundaryFlag               ( Bool   val  )     { m_loopFilterAcrossTilesEnabledFlag = val; }
  Bool  getLFCrossTileBoundaryFlag               ()                  { return m_loopFilterAcrossTilesEnabledFlag;   }
  Void  setTileUniformSpacingFlag      ( Bool b )                    { m_tileUniformSpacingFlag = b; }
  Bool  getTileUniformSpacingFlag      ()                            { return m_tileUniformSpacingFlag; }
  Void  setNumColumnsMinus1            ( Int i )                     { m_iNumColumnsMinus1 = i; }
  Int   getNumColumnsMinus1            ()                            { return m_iNumColumnsMinus1; }
  Void  setColumnWidth ( const std::vector<Int>& columnWidth )       { m_tileColumnWidth = columnWidth; }
  UInt  getColumnWidth                 ( UInt columnIdx )            { return m_tileColumnWidth[columnIdx]; }
  Void  setNumRowsMinus1               ( Int i )                     { m_iNumRowsMinus1 = i; }
  Int   getNumRowsMinus1               ()                            { return m_iNumRowsMinus1; }
  Void  setRowHeight ( const std::vector<Int>& rowHeight)            { m_tileRowHeight = rowHeight; }
  UInt  getRowHeight                   ( UInt rowIdx )               { return m_tileRowHeight[rowIdx]; }
  Void  xCheckGSParameters();
  Void  setEntropyCodingSyncEnabledFlag(Bool b)                      { m_entropyCodingSyncEnabledFlag = b; }
  Bool  getEntropyCodingSyncEnabledFlag() const                      { return m_entropyCodingSyncEnabledFlag; }
  Void  setDecodedPictureHashSEIType(HashType m)                     { m_decodedPictureHashSEIType = m; }
  HashType getDecodedPictureHashSEIType() const                      { return m_decodedPictureHashSEIType; }
  Void  setBufferingPeriodSEIEnabled(Bool b)                         { m_bufferingPeriodSEIEnabled = b; }
  Bool  getBufferingPeriodSEIEnabled() const                         { return m_bufferingPeriodSEIEnabled; }
  Void  setPictureTimingSEIEnabled(Bool b)                           { m_pictureTimingSEIEnabled = b; }
  Bool  getPictureTimingSEIEnabled() const                           { return m_pictureTimingSEIEnabled; }
  Void  setRecoveryPointSEIEnabled(Bool b)                           { m_recoveryPointSEIEnabled = b; }
  Bool  getRecoveryPointSEIEnabled() const                           { return m_recoveryPointSEIEnabled; }
  Void  setToneMappingInfoSEIEnabled(Bool b)                         { m_toneMappingInfoSEIEnabled = b;  }
  Bool  getToneMappingInfoSEIEnabled()                               { return m_toneMappingInfoSEIEnabled;  }
  Void  setTMISEIToneMapId(Int b)                                    { m_toneMapId = b;  }
  Int   getTMISEIToneMapId()                                         { return m_toneMapId;  }
  Void  setTMISEIToneMapCancelFlag(Bool b)                           { m_toneMapCancelFlag=b;  }
  Bool  getTMISEIToneMapCancelFlag()                                 { return m_toneMapCancelFlag;  }
  Void  setTMISEIToneMapPersistenceFlag(Bool b)                      { m_toneMapPersistenceFlag = b;  }
  Bool   getTMISEIToneMapPersistenceFlag()                           { return m_toneMapPersistenceFlag;  }
  Void  setTMISEICodedDataBitDepth(Int b)                            { m_codedDataBitDepth = b;  }
  Int   getTMISEICodedDataBitDepth()                                 { return m_codedDataBitDepth;  }
  Void  setTMISEITargetBitDepth(Int b)                               { m_targetBitDepth = b;  }
  Int   getTMISEITargetBitDepth()                                    { return m_targetBitDepth;  }
  Void  setTMISEIModelID(Int b)                                      { m_modelId = b;  }
  Int   getTMISEIModelID()                                           { return m_modelId;  }
  Void  setTMISEIMinValue(Int b)                                     { m_minValue = b;  }
  Int   getTMISEIMinValue()                                          { return m_minValue;  }
  Void  setTMISEIMaxValue(Int b)                                     { m_maxValue = b;  }
  Int   getTMISEIMaxValue()                                          { return m_maxValue;  }
  Void  setTMISEISigmoidMidpoint(Int b)                              { m_sigmoidMidpoint = b;  }
  Int   getTMISEISigmoidMidpoint()                                   { return m_sigmoidMidpoint;  }
  Void  setTMISEISigmoidWidth(Int b)                                 { m_sigmoidWidth = b;  }
  Int   getTMISEISigmoidWidth()                                      { return m_sigmoidWidth;  }
  Void  setTMISEIStartOfCodedInterva( Int*  p )                      { m_startOfCodedInterval = p;  }
  Int*  getTMISEIStartOfCodedInterva()                               { return m_startOfCodedInterval;  }
  Void  setTMISEINumPivots(Int b)                                    { m_numPivots = b;  }
  Int   getTMISEINumPivots()                                         { return m_numPivots;  }
  Void  setTMISEICodedPivotValue( Int*  p )                          { m_codedPivotValue = p;  }
  Int*  getTMISEICodedPivotValue()                                   { return m_codedPivotValue;  }
  Void  setTMISEITargetPivotValue( Int*  p )                         { m_targetPivotValue = p;  }
  Int*  getTMISEITargetPivotValue()                                  { return m_targetPivotValue;  }
  Void  setTMISEICameraIsoSpeedIdc(Int b)                            { m_cameraIsoSpeedIdc = b;  }
  Int   getTMISEICameraIsoSpeedIdc()                                 { return m_cameraIsoSpeedIdc;  }
  Void  setTMISEICameraIsoSpeedValue(Int b)                          { m_cameraIsoSpeedValue = b;  }
  Int   getTMISEICameraIsoSpeedValue()                               { return m_cameraIsoSpeedValue;  }
  Void  setTMISEIExposureIndexIdc(Int b)                             { m_exposureIndexIdc = b;  }
  Int   getTMISEIExposurIndexIdc()                                   { return m_exposureIndexIdc;  }
  Void  setTMISEIExposureIndexValue(Int b)                           { m_exposureIndexValue = b;  }
  Int   getTMISEIExposurIndexValue()                                 { return m_exposureIndexValue;  }
  Void  setTMISEIExposureCompensationValueSignFlag(Bool b)           { m_exposureCompensationValueSignFlag = b;  }
  Bool  getTMISEIExposureCompensationValueSignFlag()                 { return m_exposureCompensationValueSignFlag;  }
  Void  setTMISEIExposureCompensationValueNumerator(Int b)           { m_exposureCompensationValueNumerator = b;  }
  Int   getTMISEIExposureCompensationValueNumerator()                { return m_exposureCompensationValueNumerator;  }
  Void  setTMISEIExposureCompensationValueDenomIdc(Int b)            { m_exposureCompensationValueDenomIdc =b;  }
  Int   getTMISEIExposureCompensationValueDenomIdc()                 { return m_exposureCompensationValueDenomIdc;  }
  Void  setTMISEIRefScreenLuminanceWhite(Int b)                      { m_refScreenLuminanceWhite = b;  }
  Int   getTMISEIRefScreenLuminanceWhite()                           { return m_refScreenLuminanceWhite;  }
  Void  setTMISEIExtendedRangeWhiteLevel(Int b)                      { m_extendedRangeWhiteLevel = b;  }
  Int   getTMISEIExtendedRangeWhiteLevel()                           { return m_extendedRangeWhiteLevel;  }
  Void  setTMISEINominalBlackLevelLumaCodeValue(Int b)               { m_nominalBlackLevelLumaCodeValue = b;  }
  Int   getTMISEINominalBlackLevelLumaCodeValue()                    { return m_nominalBlackLevelLumaCodeValue;  }
  Void  setTMISEINominalWhiteLevelLumaCodeValue(Int b)               { m_nominalWhiteLevelLumaCodeValue = b;  }
  Int   getTMISEINominalWhiteLevelLumaCodeValue()                    { return m_nominalWhiteLevelLumaCodeValue;  }
  Void  setTMISEIExtendedWhiteLevelLumaCodeValue(Int b)              { m_extendedWhiteLevelLumaCodeValue =b;  }
  Int   getTMISEIExtendedWhiteLevelLumaCodeValue()                   { return m_extendedWhiteLevelLumaCodeValue;  }
  Void  setFramePackingArrangementSEIEnabled(Bool b)                 { m_framePackingSEIEnabled = b; }
  Bool  getFramePackingArrangementSEIEnabled() const                 { return m_framePackingSEIEnabled; }
  Void  setFramePackingArrangementSEIType(Int b)                     { m_framePackingSEIType = b; }
  Int   getFramePackingArrangementSEIType()                          { return m_framePackingSEIType; }
  Void  setFramePackingArrangementSEIId(Int b)                       { m_framePackingSEIId = b; }
  Int   getFramePackingArrangementSEIId()                            { return m_framePackingSEIId; }
  Void  setFramePackingArrangementSEIQuincunx(Int b)                 { m_framePackingSEIQuincunx = b; }
  Int   getFramePackingArrangementSEIQuincunx()                      { return m_framePackingSEIQuincunx; }
  Void  setFramePackingArrangementSEIInterpretation(Int b)           { m_framePackingSEIInterpretation = b; }
  Int   getFramePackingArrangementSEIInterpretation()                { return m_framePackingSEIInterpretation; }
  Void  setSegmentedRectFramePackingArrangementSEIEnabled(Bool b)    { m_segmentedRectFramePackingSEIEnabled = b; }
  Bool  getSegmentedRectFramePackingArrangementSEIEnabled() const    { return m_segmentedRectFramePackingSEIEnabled; }
  Void  setSegmentedRectFramePackingArrangementSEICancel(Int b)      { m_segmentedRectFramePackingSEICancel = b; }
  Int   getSegmentedRectFramePackingArrangementSEICancel()           { return m_segmentedRectFramePackingSEICancel; }
  Void  setSegmentedRectFramePackingArrangementSEIType(Int b)        { m_segmentedRectFramePackingSEIType = b; }
  Int   getSegmentedRectFramePackingArrangementSEIType()             { return m_segmentedRectFramePackingSEIType; }
  Void  setSegmentedRectFramePackingArrangementSEIPersistence(Int b) { m_segmentedRectFramePackingSEIPersistence = b; }
  Int   getSegmentedRectFramePackingArrangementSEIPersistence()      { return m_segmentedRectFramePackingSEIPersistence; }
  Void  setDisplayOrientationSEIAngle(Int b)                         { m_displayOrientationSEIAngle = b; }
  Int   getDisplayOrientationSEIAngle()                              { return m_displayOrientationSEIAngle; }
  Void  setTemporalLevel0IndexSEIEnabled(Bool b)                     { m_temporalLevel0IndexSEIEnabled = b; }
  Bool  getTemporalLevel0IndexSEIEnabled() const                     { return m_temporalLevel0IndexSEIEnabled; }
  Void  setGradualDecodingRefreshInfoEnabled(Bool b)                 { m_gradualDecodingRefreshInfoEnabled = b;    }
  Bool  getGradualDecodingRefreshInfoEnabled() const                 { return m_gradualDecodingRefreshInfoEnabled; }
  Void  setNoDisplaySEITLayer(Int b)                                 { m_noDisplaySEITLayer = b;    }
  Int   getNoDisplaySEITLayer()                                      { return m_noDisplaySEITLayer; }
  Void  setDecodingUnitInfoSEIEnabled(Bool b)                        { m_decodingUnitInfoSEIEnabled = b;    }
  Bool  getDecodingUnitInfoSEIEnabled() const                        { return m_decodingUnitInfoSEIEnabled; }
  Void  setSOPDescriptionSEIEnabled(Bool b)                          { m_SOPDescriptionSEIEnabled = b; }
  Bool  getSOPDescriptionSEIEnabled() const                          { return m_SOPDescriptionSEIEnabled; }
  Void  setScalableNestingSEIEnabled(Bool b)                         { m_scalableNestingSEIEnabled = b; }
  Bool  getScalableNestingSEIEnabled() const                         { return m_scalableNestingSEIEnabled; }
  Void  setTMCTSSEIEnabled(Bool b)                                   { m_tmctsSEIEnabled = b; }
  Bool  getTMCTSSEIEnabled()                                         { return m_tmctsSEIEnabled; }
#if MCTS_ENC_CHECK
  Void  setTMCTSSEITileConstraint(Bool b)                            { m_tmctsSEITileConstraint = b; }
  Bool  getTMCTSSEITileConstraint()                                  { return m_tmctsSEITileConstraint; }
#endif
#if MCTS_EXTRACTION
  Void  setTMCTSExtractionSEIEnabled(Bool b)                         { m_tmctsExtractionSEIEnabled = b; }
  Bool  getTMCTSExtractionSEIEnabled() const                         { return m_tmctsExtractionSEIEnabled; }
#endif
  Void  setTimeCodeSEIEnabled(Bool b)                                { m_timeCodeSEIEnabled = b; }
  Bool  getTimeCodeSEIEnabled()                                      { return m_timeCodeSEIEnabled; }
  Void  setNumberOfTimeSets(Int value)                               { m_timeCodeSEINumTs = value; }
  Int   getNumberOfTimesets()                                        { return m_timeCodeSEINumTs; }
  Void  setTimeSet(TComSEITimeSet element, Int index)                { m_timeSetArray[index] = element; }
  TComSEITimeSet &getTimeSet(Int index)                              { return m_timeSetArray[index]; }
  const TComSEITimeSet &getTimeSet(Int index) const                  { return m_timeSetArray[index]; }
  Void  setKneeSEIEnabled(Int b)                                     { m_kneeSEIEnabled = b; }
  Bool  getKneeSEIEnabled()                                          { return m_kneeSEIEnabled; }
  Void  setKneeFunctionInformationSEI(const TEncSEIKneeFunctionInformation &seiknee) { m_kneeFunctionInformationSEI = seiknee; }
  const TEncSEIKneeFunctionInformation &getKneeFunctionInformationSEI() const        { return m_kneeFunctionInformationSEI; }

  Void     setCcvSEIEnabled(Bool b)                                  { m_ccvSEIEnabled = b; }
  Bool     getCcvSEIEnabled()                                        { return m_ccvSEIEnabled; }
  Void     setCcvSEICancelFlag(Bool b)                               { m_ccvSEICancelFlag = b; }
  Bool     getCcvSEICancelFlag()                                     { return m_ccvSEICancelFlag; }
  Void     setCcvSEIPersistenceFlag(Bool b)                          { m_ccvSEIPersistenceFlag = b; }
  Bool     getCcvSEIPersistenceFlag()                                { return m_ccvSEIPersistenceFlag; }
  Void     setCcvSEIPrimariesPresentFlag(Bool b)                     { m_ccvSEIPrimariesPresentFlag = b; }
  Bool     getCcvSEIPrimariesPresentFlag()                           { return m_ccvSEIPrimariesPresentFlag; }
  Void     setCcvSEIMinLuminanceValuePresentFlag(Bool b)             { m_ccvSEIMinLuminanceValuePresentFlag = b; }
  Bool     getCcvSEIMinLuminanceValuePresentFlag()                   { return m_ccvSEIMinLuminanceValuePresentFlag; }
  Void     setCcvSEIMaxLuminanceValuePresentFlag(Bool b)             { m_ccvSEIMaxLuminanceValuePresentFlag = b; }
  Bool     getCcvSEIMaxLuminanceValuePresentFlag()                   { return m_ccvSEIMaxLuminanceValuePresentFlag; }
  Void     setCcvSEIAvgLuminanceValuePresentFlag(Bool b)             { m_ccvSEIAvgLuminanceValuePresentFlag = b; }
  Bool     getCcvSEIAvgLuminanceValuePresentFlag()                   { return m_ccvSEIAvgLuminanceValuePresentFlag; }
  Void     setCcvSEIPrimariesX(Double dValue, Int index)             { m_ccvSEIPrimariesX[index] = dValue; }
  Double   getCcvSEIPrimariesX(Int index)                            { return m_ccvSEIPrimariesX[index]; }
  Void     setCcvSEIPrimariesY(Double dValue, Int index)             { m_ccvSEIPrimariesY[index] = dValue; }
  Double   getCcvSEIPrimariesY(Int index)                            { return m_ccvSEIPrimariesY[index]; }
  Void     setCcvSEIMinLuminanceValue  (Double dValue)               { m_ccvSEIMinLuminanceValue = dValue; }
  Double   getCcvSEIMinLuminanceValue  ()                            { return m_ccvSEIMinLuminanceValue;  }
  Void     setCcvSEIMaxLuminanceValue  (Double dValue)               { m_ccvSEIMaxLuminanceValue = dValue; }
  Double   getCcvSEIMaxLuminanceValue  ()                            { return m_ccvSEIMaxLuminanceValue;  }
  Void     setCcvSEIAvgLuminanceValue  (Double dValue)               { m_ccvSEIAvgLuminanceValue = dValue; }
  Double   getCcvSEIAvgLuminanceValue  ()                            { return m_ccvSEIAvgLuminanceValue;  }

  #if SHUTTER_INTERVAL_SEI_MESSAGE
  Void     setSiiSEIEnabled(Bool b)                                  { m_siiSEIEnabled = b; }
  Bool     getSiiSEIEnabled()                                        { return m_siiSEIEnabled; }
  Void     setSiiSEINumUnitsInShutterInterval(UInt value)            { m_siiSEINumUnitsInShutterInterval = value; }
  UInt     getSiiSEINumUnitsInShutterInterval()                      { return m_siiSEINumUnitsInShutterInterval; }
  Void     setSiiSEITimeScale(UInt value)                            { m_siiSEITimeScale = value; }
  UInt     getSiiSEITimeScale()                                      { return m_siiSEITimeScale; }
  UInt     getSiiSEIMaxSubLayersMinus1()                             { return UInt(std::max(1u, UInt(m_siiSEISubLayerNumUnitsInSI.size()))-1 ); }
  Bool     getSiiSEIFixedSIwithinCLVS()                              { return m_siiSEISubLayerNumUnitsInSI.empty(); }
  Void     setSiiSEISubLayerNumUnitsInSI(const std::vector<UInt>& b) { m_siiSEISubLayerNumUnitsInSI = b; }
  UInt     getSiiSEISubLayerNumUnitsInSI(UInt idx) const             { return m_siiSEISubLayerNumUnitsInSI[idx]; }
#endif
#if SEI_ENCODER_CONTROL
  // film grain SEI
  Void  setFilmGrainCharactersticsSEIEnabled (Bool b)                { m_fgcSEIEnabled = b; }
  Bool  getFilmGrainCharactersticsSEIEnabled()                       { return m_fgcSEIEnabled; }
  Void  setFilmGrainCharactersticsSEICancelFlag(Bool b)              { m_fgcSEICancelFlag = b; }
  Bool  getFilmGrainCharactersticsSEICancelFlag()                    { return m_fgcSEICancelFlag; }
  Void  setFilmGrainCharactersticsSEIPersistenceFlag(Bool b)         { m_fgcSEIPersistenceFlag = b; }
  Bool  getFilmGrainCharactersticsSEIPersistenceFlag()               { return m_fgcSEIPersistenceFlag; }
  Void  setFilmGrainCharactersticsSEIModelID(UChar v )               { m_fgcSEIModelID = v; }
  UChar getFilmGrainCharactersticsSEIModelID()                       { return m_fgcSEIModelID; }
  Void  setFilmGrainCharactersticsSEISepColourDescPresent(Bool b)    { m_fgcSEISepColourDescPresentFlag = b; }
  Bool  getFilmGrainCharactersticsSEISepColourDescPresent()          { return m_fgcSEISepColourDescPresentFlag; }
  Void  setFilmGrainCharactersticsSEIBlendingModeID(UChar v )        { m_fgcSEIBlendingModeID = v; }
  UChar getFilmGrainCharactersticsSEIBlendingModeID()                { return m_fgcSEIBlendingModeID; }
  Void  setFilmGrainCharactersticsSEILog2ScaleFactor(UChar v )       { m_fgcSEILog2ScaleFactor = v; }
  UChar getFilmGrainCharactersticsSEILog2ScaleFactor()               { return m_fgcSEILog2ScaleFactor; }
  Void  setFGCSEICompModelPresent(Bool b, Int index)                 { m_fgcSEICompModelPresent[index] = b; }
  Bool  getFGCSEICompModelPresent(Int index)                         { return m_fgcSEICompModelPresent[index]; }
#if JVET_X0048_X0103_FILM_GRAIN
  bool*   getFGCSEICompModelPresent                 ()               { return m_fgcSEICompModelPresent; }
  void    setFilmGrainAnalysisEnabled               (bool b)         { m_fgcSEIAnalysisEnabled = b; }
  bool    getFilmGrainAnalysisEnabled               ()               { return m_fgcSEIAnalysisEnabled; }
  void    setFilmGrainExternalMask(std::string s) { m_fgcSEIExternalMask = s; }
  void    setFilmGrainExternalDenoised(std::string s) { m_fgcSEIExternalDenoised = s; }
  std::string getFilmGrainExternalMask() { return m_fgcSEIExternalMask; }
  std::string getFilmGrainExternalDenoised() { return m_fgcSEIExternalDenoised; }
  void    setFilmGrainCharactersticsSEIPerPictureSEI(bool b)         { m_fgcSEIPerPictureSEI = b; }
  bool    getFilmGrainCharactersticsSEIPerPictureSEI()               { return m_fgcSEIPerPictureSEI; }
  Void    setFGCSEINumIntensityIntervalMinus1(UChar v, Int index) { m_fgcSEINumIntensityIntervalMinus1[index] = v; }
  UChar   getFGCSEINumIntensityIntervalMinus1(Int index) { return m_fgcSEINumIntensityIntervalMinus1[index]; }
  Void    setFGCSEINumModelValuesMinus1(UChar v, Int index) { m_fgcSEINumModelValuesMinus1[index] = v; }
  UChar   getFGCSEINumModelValuesMinus1(Int index) { return m_fgcSEINumModelValuesMinus1[index]; }
  Void    setFGCSEIIntensityIntervalLowerBound(UChar v, Int index, Int ctr) { m_fgcSEIIntensityIntervalLowerBound[index][ctr] = v; }
  UChar   getFGCSEIIntensityIntervalLowerBound(Int index, Int ctr) { return m_fgcSEIIntensityIntervalLowerBound[index][ctr]; }
  Void    setFGCSEIIntensityIntervalUpperBound(UChar v, Int index, Int ctr) { m_fgcSEIIntensityIntervalUpperBound[index][ctr] = v; }
  UChar   getFGCSEIIntensityIntervalUpperBound(Int index, Int ctr) { return m_fgcSEIIntensityIntervalUpperBound[index][ctr]; }
  Void    setFGCSEICompModelValue(UInt v, Int index, Int ctr, Int modelCtr) { m_fgcSEICompModelValue[index][ctr][modelCtr] = v; }
  UInt    getFGCSEICompModelValue(Int index, Int ctr, Int modelCtr) { return m_fgcSEICompModelValue[index][ctr][modelCtr]; }
#endif
  // cll SEI
  Void  setCLLSEIEnabled(Bool b)                                     { m_cllSEIEnabled = b; }
  Bool  getCLLSEIEnabled()                                           { return m_cllSEIEnabled; }
  Void  setCLLSEIMaxContentLightLevel (UShort v)                     { m_cllSEIMaxContentLevel = v; }
  UShort  getCLLSEIMaxContentLightLevel()                            { return m_cllSEIMaxContentLevel; }
  Void  setCLLSEIMaxPicAvgLightLevel(UShort v)                       { m_cllSEIMaxPicAvgLevel = v; }
  UShort  getCLLSEIMaxPicAvgLightLevel()                             { return m_cllSEIMaxPicAvgLevel; }
  // ave SEI
  Void  setAmbientViewingEnvironmentSEIEnabled (Bool b)              { m_aveSEIEnabled = b; }
  Bool  getAmbientViewingEnvironmentSEIEnabled ()                    { return m_aveSEIEnabled; }
  Void  setAmbientViewingEnvironmentSEIIlluminance(UInt v )          { m_aveSEIAmbientIlluminance = v; }
  UInt  getAmbientViewingEnvironmentSEIIlluminance()                 { return m_aveSEIAmbientIlluminance; }
  Void  setAmbientViewingEnvironmentSEIAmbientLightX(UShort v )      { m_aveSEIAmbientLightX = v; }
  UShort getAmbientViewingEnvironmentSEIAmbientLightX()              { return m_aveSEIAmbientLightX; }
  Void  setAmbientViewingEnvironmentSEIAmbientLightY(UShort v )      { m_aveSEIAmbientLightY = v; }
  UShort getAmbientViewingEnvironmentSEIAmbientLightY()              { return m_aveSEIAmbientLightY; }
#endif
  Void  setErpSEIEnabled(Bool b)                                     { m_erpSEIEnabled = b; }                                                         
  Bool  getErpSEIEnabled()                                           { return m_erpSEIEnabled; }
  Void  setErpSEICancelFlag(Bool b)                                  { m_erpSEICancelFlag = b; }                                                         
  Bool  getErpSEICancelFlag()                                        { return m_erpSEICancelFlag; }
  Void  setErpSEIPersistenceFlag(Bool b)                             { m_erpSEIPersistenceFlag = b; }                                                         
  Bool  getErpSEIPersistenceFlag()                                   { return m_erpSEIPersistenceFlag; }
  Void  setErpSEIGuardBandFlag(Bool b)                               { m_erpSEIGuardBandFlag = b; }                                                         
  Bool  getErpSEIGuardBandFlag()                                     { return m_erpSEIGuardBandFlag; }
  Void  setErpSEIGuardBandType(UInt b)                               { m_erpSEIGuardBandType = b; } 
  UInt  getErpSEIGuardBandType()                                     { return m_erpSEIGuardBandType; }  
  Void  setErpSEILeftGuardBandWidth(UInt b)                          { m_erpSEILeftGuardBandWidth = b; } 
  UInt  getErpSEILeftGuardBandWidth()                                { return m_erpSEILeftGuardBandWidth; }  
  Void  setErpSEIRightGuardBandWidth(UInt b)                         { m_erpSEIRightGuardBandWidth = b; } 
  UInt  getErpSEIRightGuardBandWidth()                               { return m_erpSEIRightGuardBandWidth; }      
  Void  setSphereRotationSEIEnabled(Bool b)                          { m_sphereRotationSEIEnabled = b; }                                                         
  Bool  getSphereRotationSEIEnabled()                                { return m_sphereRotationSEIEnabled; }
  Void  setSphereRotationSEICancelFlag(Bool b)                       { m_sphereRotationSEICancelFlag = b; }                                                         
  Bool  getSphereRotationSEICancelFlag()                             { return m_sphereRotationSEICancelFlag; }
  Void  setSphereRotationSEIPersistenceFlag(Bool b)                  { m_sphereRotationSEIPersistenceFlag = b; }
  Bool  getSphereRotationSEIPersistenceFlag()                        { return m_sphereRotationSEIPersistenceFlag; }
  Void  setSphereRotationSEIYaw(Int b)                               { m_sphereRotationSEIYaw = b; }
  Int   getSphereRotationSEIYaw()                                    { return m_sphereRotationSEIYaw; }
  Void  setSphereRotationSEIPitch(Int b)                             { m_sphereRotationSEIPitch = b; }
  Int   getSphereRotationSEIPitch()                                  { return m_sphereRotationSEIPitch; }
  Void  setSphereRotationSEIRoll(Int b)                              { m_sphereRotationSEIRoll = b; }
  Int   getSphereRotationSEIRoll()                                   { return m_sphereRotationSEIRoll; }
  Void  setOmniViewportSEIEnabled(Bool b)                            { m_omniViewportSEIEnabled = b; }
  Bool  getOmniViewportSEIEnabled()                                  { return m_omniViewportSEIEnabled; }
  Void  setOmniViewportSEIId(UInt b)                                 { m_omniViewportSEIId = b; }
  UInt  getOmniViewportSEIId()                                       { return m_omniViewportSEIId; }
  Void  setOmniViewportSEICancelFlag(Bool b)                         { m_omniViewportSEICancelFlag = b; }
  Bool  getOmniViewportSEICancelFlag()                               { return m_omniViewportSEICancelFlag; }
  Void  setOmniViewportSEIPersistenceFlag(Bool b)                    { m_omniViewportSEIPersistenceFlag = b; }
  Bool  getOmniViewportSEIPersistenceFlag()                          { return m_omniViewportSEIPersistenceFlag; }
  Void  setOmniViewportSEICntMinus1(UInt b)                          { m_omniViewportSEICntMinus1 = b; }
  UInt  getOmniViewportSEICntMinus1()                                { return m_omniViewportSEICntMinus1; }
  Void  setOmniViewportSEIAzimuthCentre(const std::vector<Int>& vi)  { m_omniViewportSEIAzimuthCentre = vi; }
  Int   getOmniViewportSEIAzimuthCentre(Int idx)                     { return m_omniViewportSEIAzimuthCentre[idx]; }
  Void  setOmniViewportSEIElevationCentre(const std::vector<Int>& vi){ m_omniViewportSEIElevationCentre = vi; }
  Int   getOmniViewportSEIElevationCentre(Int idx)                   { return m_omniViewportSEIElevationCentre[idx]; }
  Void  setOmniViewportSEITiltCentre(const std::vector<Int>& vi)     { m_omniViewportSEITiltCentre = vi; }
  Int   getOmniViewportSEITiltCentre(Int idx)                        { return m_omniViewportSEITiltCentre[idx]; }
  Void  setOmniViewportSEIHorRange(const std::vector<UInt>& vi)      { m_omniViewportSEIHorRange = vi; }
  UInt  getOmniViewportSEIHorRange(Int idx)                          { return m_omniViewportSEIHorRange[idx]; }
  Void  setOmniViewportSEIVerRange(const std::vector<UInt>& vi)      { m_omniViewportSEIVerRange = vi; } 
  UInt  getOmniViewportSEIVerRange(Int idx)                          { return m_omniViewportSEIVerRange[idx]; }
  Void  setGopBasedTemporalFilterEnabled(Bool flag)                  { m_gopBasedTemporalFilterEnabled = flag; }
  Bool  getGopBasedTemporalFilterEnabled() const                     { return m_gopBasedTemporalFilterEnabled; }
#if JVET_Y0077_BIM
  void  setBIM(Bool flag)                                            { m_bimEnabled = flag; }
  Bool  getBIM() const                                               { return m_bimEnabled; }
  void  setAdaptQPmap(std::map<Int, Int*> map)                       { m_adaptQPmap = map; }
  Int*  getAdaptQPmap(Int poc)                                       { return m_adaptQPmap[poc]; }
  std::map<Int, Int*> *getAdaptQPmap()                               { return &m_adaptQPmap; }
#endif
  Void     setCmpSEIEnabled(Bool b)                                  { m_cmpSEIEnabled = b; }
  Bool     getCmpSEIEnabled()                                        { return m_cmpSEIEnabled; }
  Void     setCmpSEICmpCancelFlag(Bool b)                            { m_cmpSEICmpCancelFlag = b; }
  Bool     getCmpSEICmpCancelFlag()                                  { return m_cmpSEICmpCancelFlag; }
  Void     setCmpSEICmpPersistenceFlag(Bool b)                       { m_cmpSEICmpPersistenceFlag = b; }
  Bool     getCmpSEICmpPersistenceFlag()                             { return m_cmpSEICmpPersistenceFlag; }
  Void     setRwpSEIEnabled(Bool b)                                                                     { m_rwpSEIEnabled = b; }
  Bool     getRwpSEIEnabled()                                                                           { return m_rwpSEIEnabled; }
  Void     setRwpSEIRwpCancelFlag(Bool b)                                                               { m_rwpSEIRwpCancelFlag = b; }
  Bool     getRwpSEIRwpCancelFlag()                                                                     { return m_rwpSEIRwpCancelFlag; }
  Void     setRwpSEIRwpPersistenceFlag (Bool b)                                                         { m_rwpSEIRwpPersistenceFlag = b; }
  Bool     getRwpSEIRwpPersistenceFlag ()                                                               { return m_rwpSEIRwpPersistenceFlag; }
  Void     setRwpSEIConstituentPictureMatchingFlag (Bool b)                                             { m_rwpSEIConstituentPictureMatchingFlag = b; }
  Bool     getRwpSEIConstituentPictureMatchingFlag ()                                                   { return m_rwpSEIConstituentPictureMatchingFlag; }
  Void     setRwpSEINumPackedRegions (Int value)                                                        { m_rwpSEINumPackedRegions = value; }
  Int      getRwpSEINumPackedRegions ()                                                                 { return m_rwpSEINumPackedRegions; }
  Void     setRwpSEIProjPictureWidth (Int value)                                                        { m_rwpSEIProjPictureWidth = value; }
  Int      getRwpSEIProjPictureWidth ()                                                                 { return m_rwpSEIProjPictureWidth; }
  Void     setRwpSEIProjPictureHeight (Int value)                                                       { m_rwpSEIProjPictureHeight = value; }
  Int      getRwpSEIProjPictureHeight ()                                                                { return m_rwpSEIProjPictureHeight; }
  Void     setRwpSEIPackedPictureWidth (Int value)                                                      { m_rwpSEIPackedPictureWidth = value; }
  Int      getRwpSEIPackedPictureWidth ()                                                               { return m_rwpSEIPackedPictureWidth; }
  Void     setRwpSEIPackedPictureHeight (Int value)                                                     { m_rwpSEIPackedPictureHeight = value; }
  Int      getRwpSEIPackedPictureHeight ()                                                              { return m_rwpSEIPackedPictureHeight; }
  Void     setRwpSEIRwpTransformType(const std::vector<UChar>& rwpTransformType)                        { m_rwpSEIRwpTransformType =rwpTransformType; }
  UChar    getRwpSEIRwpTransformType(UInt idx) const                                                    { return m_rwpSEIRwpTransformType[idx]; } 
  Void     setRwpSEIRwpGuardBandFlag(const std::vector<Bool>& rwpGuardBandFlag)                         { m_rwpSEIRwpGuardBandFlag = rwpGuardBandFlag; }
  Bool     getRwpSEIRwpGuardBandFlag(UInt idx) const                                                    { return m_rwpSEIRwpGuardBandFlag[idx]; }
  Void     setRwpSEIProjRegionWidth(const std::vector<UInt>& projRegionWidth)                           { m_rwpSEIProjRegionWidth = projRegionWidth; }
  UInt     getRwpSEIProjRegionWidth(UInt idx) const                                                     { return m_rwpSEIProjRegionWidth[idx]; } 
  Void     setRwpSEIProjRegionHeight(const std::vector<UInt>& projRegionHeight)                         { m_rwpSEIProjRegionHeight = projRegionHeight; } 
  UInt     getRwpSEIProjRegionHeight(UInt idx) const                                                    { return m_rwpSEIProjRegionHeight[idx]; } 
  Void     setRwpSEIRwpSEIProjRegionTop(const std::vector<UInt>& projRegionTop)                         { m_rwpSEIRwpSEIProjRegionTop = projRegionTop; }
  UInt     getRwpSEIRwpSEIProjRegionTop(UInt idx) const                                                 { return m_rwpSEIRwpSEIProjRegionTop[idx]; } 
  Void     setRwpSEIProjRegionLeft(const std::vector<UInt>& projRegionLeft)                             { m_rwpSEIProjRegionLeft = projRegionLeft; } 
  UInt     getRwpSEIProjRegionLeft(UInt idx) const                                                      { return m_rwpSEIProjRegionLeft[idx]; } 
  Void    setRwpSEIPackedRegionWidth(const std::vector<UShort>& packedRegionWidth)                      { m_rwpSEIPackedRegionWidth  = packedRegionWidth; }
  UShort  getRwpSEIPackedRegionWidth(UInt idx) const                                                    { return m_rwpSEIPackedRegionWidth[idx]; } 
  Void    setRwpSEIPackedRegionHeight(const std::vector<UShort>& packedRegionHeight)                    { m_rwpSEIPackedRegionHeight = packedRegionHeight; }
  UShort  getRwpSEIPackedRegionHeight(UInt idx) const                                                   { return m_rwpSEIPackedRegionHeight[idx]; } 
  Void    setRwpSEIPackedRegionTop(const std::vector<UShort>& packedRegionTop)                          { m_rwpSEIPackedRegionTop = packedRegionTop; }
  UShort  getRwpSEIPackedRegionTop(UInt idx) const                                                      { return m_rwpSEIPackedRegionTop[idx]; } 
  Void    setRwpSEIPackedRegionLeft(const std::vector<UShort>& packedRegionLeft)                        { m_rwpSEIPackedRegionLeft = packedRegionLeft; } 
  UShort  getRwpSEIPackedRegionLeft(UInt idx) const                                                     { return m_rwpSEIPackedRegionLeft[idx]; }
  Void    setRwpSEIRwpLeftGuardBandWidth(const std::vector<UChar>& rwpLeftGuardBandWidth)               { m_rwpSEIRwpLeftGuardBandWidth = rwpLeftGuardBandWidth; } 
  UChar   getRwpSEIRwpLeftGuardBandWidth(UInt idx) const                                                { return m_rwpSEIRwpLeftGuardBandWidth[idx]; }
  Void    setRwpSEIRwpRightGuardBandWidth(const std::vector<UChar>& rwpRightGuardBandWidth)             { m_rwpSEIRwpRightGuardBandWidth = rwpRightGuardBandWidth; } 
  UChar   getRwpSEIRwpRightGuardBandWidth(UInt idx) const                                               { return m_rwpSEIRwpRightGuardBandWidth[idx]; } 
  Void    setRwpSEIRwpTopGuardBandHeight(const std::vector<UChar>& rwpTopGuardBandHeight)               { m_rwpSEIRwpTopGuardBandHeight = rwpTopGuardBandHeight; } 
  UChar   getRwpSEIRwpTopGuardBandHeight(UInt idx) const                                                { return m_rwpSEIRwpTopGuardBandHeight[idx]; }
  Void    setRwpSEIRwpBottomGuardBandHeight(const std::vector<UChar>& rwpBottomGuardBandHeight)         { m_rwpSEIRwpBottomGuardBandHeight = rwpBottomGuardBandHeight; }
  UChar   getRwpSEIRwpBottomGuardBandHeight(UInt idx) const                                             { return m_rwpSEIRwpBottomGuardBandHeight[idx]; } 
  Void    setRwpSEIRwpGuardBandNotUsedForPredFlag(const std::vector<Bool>& rwpGuardBandNotUsedForPredFlag){ m_rwpSEIRwpGuardBandNotUsedForPredFlag = rwpGuardBandNotUsedForPredFlag; }
  Bool    getRwpSEIRwpGuardBandNotUsedForPredFlag(UInt idx) const                                         { return m_rwpSEIRwpGuardBandNotUsedForPredFlag[idx]; }
  Void    setRwpSEIRwpGuardBandType(const std::vector<UChar>& rwpGuardBandType)                           { m_rwpSEIRwpGuardBandType = rwpGuardBandType; }
  UChar   getRwpSEIRwpGuardBandType(UInt idx) const                                                       { return m_rwpSEIRwpGuardBandType[idx]; } 
  Void    setFviSEIDisabled()                                        { m_fviSEIEnabled = false; }
  Void    setFviSEIEnabled(const TComSEIFisheyeVideoInfo& fvi)       { m_fisheyeVideoInfo=fvi; m_fviSEIEnabled=true; }
  Bool    getFviSEIEnabled() const                                   { return m_fviSEIEnabled; }
  const TComSEIFisheyeVideoInfo& getFviSEIData() const               { return m_fisheyeVideoInfo; }
  Void  setColourRemapInfoSEIFileRoot( const std::string &s )        { m_colourRemapSEIFileRoot = s; }
  const std::string &getColourRemapInfoSEIFileRoot() const           { return m_colourRemapSEIFileRoot; }
  Void  setMasteringDisplaySEI(const TComSEIMasteringDisplay &src)   { m_masteringDisplay = src; }
  Void  setSEIAlternativeTransferCharacteristicsSEIEnable( Bool b)   { m_alternativeTransferCharacteristicsSEIEnabled = b;    }
  Bool  getSEIAlternativeTransferCharacteristicsSEIEnable( ) const   { return m_alternativeTransferCharacteristicsSEIEnabled; }
  Void  setSEIPreferredTransferCharacteristics(UChar v)              { m_preferredTransferCharacteristics = v;    }
  UChar getSEIPreferredTransferCharacteristics() const               { return m_preferredTransferCharacteristics; }
  Void  setSEIGreenMetadataInfoSEIEnable( Bool b)                    { m_greenMetadataInfoSEIEnabled = b;    }
  Bool  getSEIGreenMetadataInfoSEIEnable( ) const                    { return m_greenMetadataInfoSEIEnabled; }
  Void  setSEIGreenMetadataType(UChar v)                             { m_greenMetadataType = v;    }
  UChar getSEIGreenMetadataType() const                              { return m_greenMetadataType; }
  Void  setSEIXSDMetricType(UChar v)                                 { m_xsdMetricType = v;    }
  UChar getSEIXSDMetricType() const                                  { return m_xsdMetricType; }
  Void  setRegionalNestingSEIFileRoot( const std::string &s )        { m_regionalNestingSEIFileRoot = s; }
  const std::string &getRegionalNestingSEIFileRoot() const           { return m_regionalNestingSEIFileRoot; }
#if JVET_T0050_ANNOTATED_REGIONS_SEI
  Void  setAnnotatedRegionSEIFileRoot(const std::string &s)          { m_arSEIFileRoot = s; m_arObjects.clear(); }
#else
  Void  setAnnotatedRegionSEIFileRoot(const std::string &s)          { m_arSEIFileRoot = s; }
#endif
  const std::string &getAnnotatedRegionSEIFileRoot() const           { return m_arSEIFileRoot; }

  const TComSEIMasteringDisplay &getMasteringDisplaySEI() const      { return m_masteringDisplay; }
  Void         setUseWP               ( Bool b )                     { m_useWeightedPred   = b;    }
  Void         setWPBiPred            ( Bool b )                     { m_useWeightedBiPred = b;    }
  Bool         getUseWP               ()                             { return m_useWeightedPred;   }
  Bool         getWPBiPred            ()                             { return m_useWeightedBiPred; }
  Void         setLog2ParallelMergeLevelMinus2   ( UInt u )          { m_log2ParallelMergeLevelMinus2       = u;    }
  UInt         getLog2ParallelMergeLevelMinus2   ()                  { return m_log2ParallelMergeLevelMinus2;       }
  Void         setMaxNumMergeCand                ( UInt u )          { m_maxNumMergeCand = u;      }
  UInt         getMaxNumMergeCand                ()                  { return m_maxNumMergeCand;   }
  Void         setUseScalingListId    ( ScalingListMode u )          { m_useScalingListId       = u;   }
  ScalingListMode getUseScalingListId    ()                          { return m_useScalingListId;      }
  Void         setScalingListFileName       ( const std::string &s ) { m_scalingListFileName = s;      }
  const std::string& getScalingListFileName () const                 { return m_scalingListFileName;   }
  Void         setTMVPModeId ( Int  u )                              { m_TMVPModeId = u;    }
  Int          getTMVPModeId ()                                      { return m_TMVPModeId; }
  WeightedPredictionMethod getWeightedPredictionMethod() const       { return m_weightedPredictionMethod; }
  Void         setWeightedPredictionMethod( WeightedPredictionMethod m ) { m_weightedPredictionMethod = m; }
  Void         setSignDataHidingEnabledFlag( Bool b )                { m_SignDataHidingEnabledFlag = b;    }
  Bool         getSignDataHidingEnabledFlag()                        { return m_SignDataHidingEnabledFlag; }
  Bool         getUseRateCtrl         ()                             { return m_RCEnableRateControl;   }
  Void         setUseRateCtrl         ( Bool b )                     { m_RCEnableRateControl = b;      }
  Int          getTargetBitrate       ()                             { return m_RCTargetBitrate;       }
  Void         setTargetBitrate       ( Int bitrate )                { m_RCTargetBitrate  = bitrate;   }
  Int          getKeepHierBit         ()                             { return m_RCKeepHierarchicalBit; }
  Void         setKeepHierBit         ( Int i )                      { m_RCKeepHierarchicalBit = i;    }
  Bool         getLCULevelRC          ()                             { return m_RCLCULevelRC; }
  Void         setLCULevelRC          ( Bool b )                     { m_RCLCULevelRC = b; }
  Bool         getUseLCUSeparateModel ()                             { return m_RCUseLCUSeparateModel; }
  Void         setUseLCUSeparateModel ( Bool b )                     { m_RCUseLCUSeparateModel = b;    }
  Int          getInitialQP           ()                             { return m_RCInitialQP;           }
  Void         setInitialQP           ( Int QP )                     { m_RCInitialQP = QP;             }
  Bool         getForceIntraQP        ()                             { return m_RCForceIntraQP;        }
  Void         setForceIntraQP        ( Bool b )                     { m_RCForceIntraQP = b;           }
  Bool         getCpbSaturationEnabled()                             { return m_RCCpbSaturationEnabled;}
  Void         setCpbSaturationEnabled( Bool b )                     { m_RCCpbSaturationEnabled = b;   }
  UInt         getCpbSize             ()                             { return m_RCCpbSize;}
  Void         setCpbSize             ( UInt ui )                    { m_RCCpbSize = ui;   }
  Double       getInitialCpbFullness  ()                             { return m_RCInitialCpbFullness;  }
  Void         setInitialCpbFullness  (Double f)                     { m_RCInitialCpbFullness = f;     }
  Bool         getTransquantBypassEnabledFlag()                      { return m_TransquantBypassEnabledFlag; }
  Void         setTransquantBypassEnabledFlag(Bool flag)             { m_TransquantBypassEnabledFlag = flag; }
  Bool         getCUTransquantBypassFlagForceValue()                 { return m_CUTransquantBypassFlagForce; }
  Void         setCUTransquantBypassFlagForceValue(Bool flag)        { m_CUTransquantBypassFlagForce = flag; }
  CostMode     getCostMode( ) const                                  { return m_costMode; }
  Void         setCostMode(CostMode m )                              { m_costMode = m; }

  Void         setVPS(TComVPS *p)                                    { m_cVPS = *p; }
  TComVPS *    getVPS()                                              { return &m_cVPS; }
  Void         setUseRecalculateQPAccordingToLambda (Bool b)         { m_recalculateQPAccordingToLambda = b;    }
  Bool         getUseRecalculateQPAccordingToLambda ()               { return m_recalculateQPAccordingToLambda; }

  Void         setUseStrongIntraSmoothing ( Bool b )                 { m_useStrongIntraSmoothing = b;    }
  Bool         getUseStrongIntraSmoothing ()                         { return m_useStrongIntraSmoothing; }

  Void         setEfficientFieldIRAPEnabled( Bool b )                { m_bEfficientFieldIRAPEnabled = b; }
  Bool         getEfficientFieldIRAPEnabled( ) const                 { return m_bEfficientFieldIRAPEnabled; }

  Void         setHarmonizeGopFirstFieldCoupleEnabled( Bool b )      { m_bHarmonizeGopFirstFieldCoupleEnabled = b; }
  Bool         getHarmonizeGopFirstFieldCoupleEnabled( ) const       { return m_bHarmonizeGopFirstFieldCoupleEnabled; }

  Void         setActiveParameterSetsSEIEnabled ( Int b )            { m_activeParameterSetsSEIEnabled = b; }
  Int          getActiveParameterSetsSEIEnabled ()                   { return m_activeParameterSetsSEIEnabled; }
  Bool         getVuiParametersPresentFlag()                         { return m_vuiParametersPresentFlag; }
  Void         setVuiParametersPresentFlag(Bool i)                   { m_vuiParametersPresentFlag = i; }
  Bool         getAspectRatioInfoPresentFlag()                       { return m_aspectRatioInfoPresentFlag; }
  Void         setAspectRatioInfoPresentFlag(Bool i)                 { m_aspectRatioInfoPresentFlag = i; }
  Int          getAspectRatioIdc()                                   { return m_aspectRatioIdc; }
  Void         setAspectRatioIdc(Int i)                              { m_aspectRatioIdc = i; }
  Int          getSarWidth()                                         { return m_sarWidth; }
  Void         setSarWidth(Int i)                                    { m_sarWidth = i; }
  Int          getSarHeight()                                        { return m_sarHeight; }
  Void         setSarHeight(Int i)                                   { m_sarHeight = i; }
  Bool         getOverscanInfoPresentFlag()                          { return m_overscanInfoPresentFlag; }
  Void         setOverscanInfoPresentFlag(Bool i)                    { m_overscanInfoPresentFlag = i; }
  Bool         getOverscanAppropriateFlag()                          { return m_overscanAppropriateFlag; }
  Void         setOverscanAppropriateFlag(Bool i)                    { m_overscanAppropriateFlag = i; }
  Bool         getVideoSignalTypePresentFlag()                       { return m_videoSignalTypePresentFlag; }
  Void         setVideoSignalTypePresentFlag(Bool i)                 { m_videoSignalTypePresentFlag = i; }
  Int          getVideoFormat()                                      { return m_videoFormat; }
  Void         setVideoFormat(Int i)                                 { m_videoFormat = i; }
  Bool         getVideoFullRangeFlag()                               { return m_videoFullRangeFlag; }
  Void         setVideoFullRangeFlag(Bool i)                         { m_videoFullRangeFlag = i; }
  Bool         getColourDescriptionPresentFlag()                     { return m_colourDescriptionPresentFlag; }
  Void         setColourDescriptionPresentFlag(Bool i)               { m_colourDescriptionPresentFlag = i; }
  Int          getColourPrimaries()                                  { return m_colourPrimaries; }
  Void         setColourPrimaries(Int i)                             { m_colourPrimaries = i; }
  Int          getTransferCharacteristics()                          { return m_transferCharacteristics; }
  Void         setTransferCharacteristics(Int i)                     { m_transferCharacteristics = i; }
  Int          getMatrixCoefficients()                               { return m_matrixCoefficients; }
  Void         setMatrixCoefficients(Int i)                          { m_matrixCoefficients = i; }
  Bool         getChromaLocInfoPresentFlag()                         { return m_chromaLocInfoPresentFlag; }
  Void         setChromaLocInfoPresentFlag(Bool i)                   { m_chromaLocInfoPresentFlag = i; }
  Int          getChromaSampleLocTypeTopField()                      { return m_chromaSampleLocTypeTopField; }
  Void         setChromaSampleLocTypeTopField(Int i)                 { m_chromaSampleLocTypeTopField = i; }
  Int          getChromaSampleLocTypeBottomField()                   { return m_chromaSampleLocTypeBottomField; }
  Void         setChromaSampleLocTypeBottomField(Int i)              { m_chromaSampleLocTypeBottomField = i; }
  Bool         getNeutralChromaIndicationFlag()                      { return m_neutralChromaIndicationFlag; }
  Void         setNeutralChromaIndicationFlag(Bool i)                { m_neutralChromaIndicationFlag = i; }
  Window      &getDefaultDisplayWindow()                             { return m_defaultDisplayWindow; }
  Void         setDefaultDisplayWindow (Int offsetLeft, Int offsetRight, Int offsetTop, Int offsetBottom ) { m_defaultDisplayWindow.setWindow (offsetLeft, offsetRight, offsetTop, offsetBottom); }
  Bool         getFrameFieldInfoPresentFlag()                        { return m_frameFieldInfoPresentFlag; }
  Void         setFrameFieldInfoPresentFlag(Bool i)                  { m_frameFieldInfoPresentFlag = i; }
  Bool         getPocProportionalToTimingFlag()                      { return m_pocProportionalToTimingFlag; }
  Void         setPocProportionalToTimingFlag(Bool x)                { m_pocProportionalToTimingFlag = x;    }
  Int          getNumTicksPocDiffOneMinus1()                         { return m_numTicksPocDiffOneMinus1;    }
  Void         setNumTicksPocDiffOneMinus1(Int x)                    { m_numTicksPocDiffOneMinus1 = x;       }
  Bool         getBitstreamRestrictionFlag()                         { return m_bitstreamRestrictionFlag; }
  Void         setBitstreamRestrictionFlag(Bool i)                   { m_bitstreamRestrictionFlag = i; }
  Bool         getTilesFixedStructureFlag()                          { return m_tilesFixedStructureFlag; }
  Void         setTilesFixedStructureFlag(Bool i)                    { m_tilesFixedStructureFlag = i; }
  Bool         getMotionVectorsOverPicBoundariesFlag()               { return m_motionVectorsOverPicBoundariesFlag; }
  Void         setMotionVectorsOverPicBoundariesFlag(Bool i)         { m_motionVectorsOverPicBoundariesFlag = i; }
  Int          getMinSpatialSegmentationIdc()                        { return m_minSpatialSegmentationIdc; }
  Void         setMinSpatialSegmentationIdc(Int i)                   { m_minSpatialSegmentationIdc = i; }
  Int          getMaxBytesPerPicDenom()                              { return m_maxBytesPerPicDenom; }
  Void         setMaxBytesPerPicDenom(Int i)                         { m_maxBytesPerPicDenom = i; }
  Int          getMaxBitsPerMinCuDenom()                             { return m_maxBitsPerMinCuDenom; }
  Void         setMaxBitsPerMinCuDenom(Int i)                        { m_maxBitsPerMinCuDenom = i; }
  Int          getLog2MaxMvLengthHorizontal()                        { return m_log2MaxMvLengthHorizontal; }
  Void         setLog2MaxMvLengthHorizontal(Int i)                   { m_log2MaxMvLengthHorizontal = i; }
  Int          getLog2MaxMvLengthVertical()                          { return m_log2MaxMvLengthVertical; }
  Void         setLog2MaxMvLengthVertical(Int i)                     { m_log2MaxMvLengthVertical = i; }

  Bool         getProgressiveSourceFlag() const                      { return m_progressiveSourceFlag; }
  Void         setProgressiveSourceFlag(Bool b)                      { m_progressiveSourceFlag = b; }

  Bool         getInterlacedSourceFlag() const                       { return m_interlacedSourceFlag; }
  Void         setInterlacedSourceFlag(Bool b)                       { m_interlacedSourceFlag = b; }

  Bool         getNonPackedConstraintFlag() const                    { return m_nonPackedConstraintFlag; }
  Void         setNonPackedConstraintFlag(Bool b)                    { m_nonPackedConstraintFlag = b; }

  Bool         getFrameOnlyConstraintFlag() const                    { return m_frameOnlyConstraintFlag; }
  Void         setFrameOnlyConstraintFlag(Bool b)                    { m_frameOnlyConstraintFlag = b; }

  UInt         getBitDepthConstraintValue() const                    { return m_bitDepthConstraintValue; }
  Void         setBitDepthConstraintValue(UInt v)                    { m_bitDepthConstraintValue=v; }

  ChromaFormat getChromaFormatConstraintValue() const                { return m_chromaFormatConstraintValue; }
  Void         setChromaFormatConstraintValue(ChromaFormat v)        { m_chromaFormatConstraintValue=v; }

  Bool         getIntraConstraintFlag() const                        { return m_intraConstraintFlag; }
  Void         setIntraConstraintFlag(Bool b)                        { m_intraConstraintFlag=b; }

  Bool         getOnePictureOnlyConstraintFlag() const               { return m_onePictureOnlyConstraintFlag; }
  Void         setOnePictureOnlyConstraintFlag(Bool b)               { m_onePictureOnlyConstraintFlag=b; }

  Bool         getLowerBitRateConstraintFlag() const                 { return m_lowerBitRateConstraintFlag; }
  Void         setLowerBitRateConstraintFlag(Bool b)                 { m_lowerBitRateConstraintFlag=b; }

  Bool         getChromaResamplingFilterHintEnabled()                { return m_chromaResamplingFilterHintEnabled;}
  Void         setChromaResamplingFilterHintEnabled(Bool i)          { m_chromaResamplingFilterHintEnabled = i;}
  Int          getChromaResamplingHorFilterIdc()                     { return m_chromaResamplingHorFilterIdc;}
  Void         setChromaResamplingHorFilterIdc(Int i)                { m_chromaResamplingHorFilterIdc = i;}
  Int          getChromaResamplingVerFilterIdc()                     { return m_chromaResamplingVerFilterIdc;}
  Void         setChromaResamplingVerFilterIdc(Int i)                { m_chromaResamplingVerFilterIdc = i;}

  Void      setSummaryOutFilename(const std::string &s)              { m_summaryOutFilename = s; }
  const std::string& getSummaryOutFilename() const                   { return m_summaryOutFilename; }
  Void      setSummaryPicFilenameBase(const std::string &s)          { m_summaryPicFilenameBase = s; }
  const std::string& getSummaryPicFilenameBase() const               { return m_summaryPicFilenameBase; }

  Void      setSummaryVerboseness(UInt v)                            { m_summaryVerboseness = v; }
  UInt      getSummaryVerboseness( ) const                           { return m_summaryVerboseness; }

#if JCTVC_AD0021_SEI_MANIFEST
  Void     setSEIManifestSEIEnabled(Bool b) { m_SEIManifestSEIEnabled = b; }
  Bool     getSEIManifestSEIEnabled() { return m_SEIManifestSEIEnabled; }
#endif
#if JCTVC_AD0021_SEI_PREFIX_INDICATION
  Void     setSEIPrefixIndicationSEIEnabled(Bool b) { m_SEIPrefixIndicationSEIEnabled = b; }
  Bool     getSEIPrefixIndicationSEIEnabled() { return m_SEIPrefixIndicationSEIEnabled; }
#endif

};

//! \}

#endif // !defined(AFX_TENCCFG_H__6B99B797_F4DA_4E46_8E78_7656339A6C41__INCLUDED_)
