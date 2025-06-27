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

/** \file     EncCfg.h
    \brief    encoder configuration class (header)
*/

#ifndef __ENCCFG__
#define __ENCCFG__

#pragma once

#include "CommonLib/CommonDef.h"
#include "CommonLib/Slice.h"

#include "CommonLib/Unit.h"

#include "EncCfgParam.h"

#if JVET_O0756_CALCULATE_HDRMETRICS
#include "HDRLib/inc/DistortionMetric.H"
#ifdef UNDEFINED
#undef UNDEFINED
#endif
#endif

struct GOPEntry
{
  int m_POC;
  int m_QPOffset;
  double m_QPOffsetModelOffset;
  double m_QPOffsetModelScale;
#if W0038_CQP_ADJ
  int m_CbQPoffset;
  int m_CrQPoffset;
#endif
  double m_QPFactor;
  int m_tcOffsetDiv2;
  int m_betaOffsetDiv2;
  int m_CbTcOffsetDiv2;
  int m_CbBetaOffsetDiv2;
  int m_CrTcOffsetDiv2;
  int m_CrBetaOffsetDiv2;
  int m_temporalId;
  bool m_refPic;
  int8_t m_sliceType;
  int m_numRefPicsActive0;
  int m_numRefPics0;
  int m_deltaRefPics0[MAX_NUM_REF_PICS];
  int m_numRefPicsActive1;
  int m_numRefPics1;
  int m_deltaRefPics1[MAX_NUM_REF_PICS];
  bool m_isEncoded;
  bool   m_ltrpInSliceHeaderFlag;
  int m_layerRef0[MAX_VPS_LAYERS];
  int m_layerRef1[MAX_VPS_LAYERS];
  GOPEntry()
    : m_POC(-1)
    , m_QPOffset(0)
    , m_QPOffsetModelOffset(0)
    , m_QPOffsetModelScale(0)
#if W0038_CQP_ADJ
    , m_CbQPoffset(0)
    , m_CrQPoffset(0)
#endif
    , m_QPFactor(0)
    , m_tcOffsetDiv2(0)
    , m_betaOffsetDiv2(0)
    , m_CbTcOffsetDiv2(0)
    , m_CbBetaOffsetDiv2(0)
    , m_CrTcOffsetDiv2(0)
    , m_CrBetaOffsetDiv2(0)
    , m_temporalId(0)
    , m_refPic(false)
    , m_sliceType('P')
    , m_numRefPicsActive0(0)
    , m_numRefPics0(0)
    , m_numRefPicsActive1(0)
    , m_numRefPics1(0)
    , m_isEncoded(false)
    , m_ltrpInSliceHeaderFlag(false)
  {
    ::memset(m_deltaRefPics0, 0, sizeof(m_deltaRefPics0));
    ::memset(m_deltaRefPics1, 0, sizeof(m_deltaRefPics1));
    ::memset(m_layerRef0, -1, sizeof(m_layerRef0));
    ::memset(m_layerRef1, -1, sizeof(m_layerRef1));
  }
};

struct RPLEntry
{
  int m_POC;
  int m_temporalId;
  bool m_refPic;
  int m_numRefPicsActive;
  int8_t m_sliceType;
  int m_numRefPics;
  int m_deltaRefPics[MAX_NUM_REF_PICS];
  bool m_isEncoded;
  bool   m_ltrpInSliceHeaderFlag;
  int m_layerRef[MAX_NUM_REF_PICS];
  RPLEntry()
    : m_POC(-1)
    , m_temporalId(0)
    , m_refPic(false)
    , m_numRefPicsActive(0)
    , m_sliceType('P')
    , m_numRefPics(0)
    , m_isEncoded(false)
    , m_ltrpInSliceHeaderFlag(false)
  {
    ::memset(m_deltaRefPics, 0, sizeof(m_deltaRefPics));
    ::memset(m_layerRef, -1, sizeof(m_layerRef));
  }
};

std::istringstream &operator>>(std::istringstream &in, GOPEntry &entry);     //input


//! \ingroup EncoderLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

using FrameDeltaQps = std::vector<int>;

/// encoder configuration class
class EncCfg
{
protected:
  //==== File I/O ========
  Fraction  m_frameRate;
  int       m_frameSkip;
  uint32_t  m_temporalSubsampleRatio;
  int       m_sourceWidth;
  int       m_sourceHeight;
  Window    m_conformanceWindow;
  bool      m_explicitScalingWindowEnabled;
  Window    m_scalingWindow;
  int       m_sourcePadding[2];
  int       m_framesToBeEncoded;
  int       m_firstValidFrame;
  int       m_lastValidFrame;

  double    m_adLambdaModifier[ MAX_TLAYER ];
  std::vector<double> m_adIntraLambdaModifier;
  double    m_dIntraQpFactor;                                 ///< Intra Q Factor. If negative, use a default equation: 0.57*(1.0 - Clip3( 0.0, 0.5, 0.05*(double)(isField ? (GopSize-1)/2 : GopSize-1) ))

  bool      m_printMSEBasedSequencePSNR;
  bool      m_printHexPsnr;
  bool      m_printFrameMSE;
  bool      m_printSequenceMSE;
  bool      m_printMSSSIM;
  bool      m_printWPSNR;
  bool      m_printHighPrecEncTime = false;
  bool      m_cabacZeroWordPaddingEnabled;
  bool      m_ShutterFilterEnable;                          ///< enable Pre-Filtering with Shutter Interval SEI
  int       m_SII_BlendingRatio;

  bool      m_gciPresentFlag;
  bool      m_onePictureOnlyConstraintFlag;
  bool      m_bIntraOnlyConstraintFlag;
  uint32_t  m_maxBitDepthConstraintIdc;
  ChromaFormat m_maxChromaFormatConstraintIdc;
  bool      m_allLayersIndependentConstraintFlag;
  bool      m_noMrlConstraintFlag;
  bool      m_noIspConstraintFlag;
  bool      m_noMipConstraintFlag;
  bool      m_noLfnstConstraintFlag;
  bool      m_noMmvdConstraintFlag;
  bool      m_noSmvdConstraintFlag;
  bool      m_noProfConstraintFlag;
  bool      m_noPaletteConstraintFlag;
  bool      m_noActConstraintFlag;
  bool      m_noLmcsConstraintFlag;
  bool      m_noExplicitScaleListConstraintFlag;
  bool      m_noVirtualBoundaryConstraintFlag;
  bool      m_noMttConstraintFlag;
  bool      m_noChromaQpOffsetConstraintFlag;
  bool      m_noQtbttDualTreeIntraConstraintFlag;
  int       m_maxLog2CtuSizeConstraintIdc;
  bool      m_noPartitionConstraintsOverrideConstraintFlag;
  bool      m_noSaoConstraintFlag;
  bool      m_noAlfConstraintFlag;
  bool      m_noCCAlfConstraintFlag;
  bool      m_noWeightedPredictionConstraintFlag;
  bool      m_noRefWraparoundConstraintFlag;
  bool      m_noTemporalMvpConstraintFlag;
  bool      m_noSbtmvpConstraintFlag;
  bool      m_noAmvrConstraintFlag;
  bool      m_noBdofConstraintFlag;
  bool      m_noDmvrConstraintFlag;
  bool      m_noCclmConstraintFlag;
  bool      m_noMtsConstraintFlag;
  bool      m_noSbtConstraintFlag;
  bool      m_noAffineMotionConstraintFlag;
  bool      m_noBcwConstraintFlag;
  bool      m_noIbcConstraintFlag;
  bool      m_noCiipConstraintFlag;
  bool      m_noGeoConstraintFlag;
  bool      m_noLadfConstraintFlag;
  bool      m_noTransformSkipConstraintFlag;
  bool      m_noLumaTransformSize64ConstraintFlag;
  bool      m_noBDPCMConstraintFlag;
  bool      m_noJointCbCrConstraintFlag;
  bool      m_noCuQpDeltaConstraintFlag;
  bool      m_noDepQuantConstraintFlag;
  bool      m_noSignDataHidingConstraintFlag;
  bool      m_noTrailConstraintFlag;
  bool      m_noStsaConstraintFlag;
  bool      m_noRaslConstraintFlag;
  bool      m_noRadlConstraintFlag;
  bool      m_noIdrConstraintFlag;
  bool      m_noCraConstraintFlag;
  bool      m_noGdrConstraintFlag;
  bool      m_noApsConstraintFlag;
  bool      m_allRapPicturesFlag;
  bool      m_noExtendedPrecisionProcessingConstraintFlag;
  bool      m_noTsResidualCodingRiceConstraintFlag;
  bool      m_noRrcRiceExtensionConstraintFlag;
  bool      m_noPersistentRiceAdaptationConstraintFlag;
  bool      m_noReverseLastSigCoeffConstraintFlag;

  /* profile & level */
  Profile::Name m_profile;
  Level::Tier   m_tier;
  Level::Name   m_level;
  bool m_frameOnlyConstraintFlag;
  bool m_multiLayerEnabledFlag;
  std::vector<uint32_t>      m_subProfile;
  uint8_t       m_numSubProfile;
  bool m_nonPackedConstraintFlag;
  bool m_nonProjectedConstraintFlag;
  bool m_noRprConstraintFlag;
  bool m_noResChangeInClvsConstraintFlag;
  bool m_oneTilePerPicConstraintFlag;
  bool m_picHeaderInSliceHeaderConstraintFlag;
  bool m_oneSlicePerPicConstraintFlag;
  bool m_noIdrRplConstraintFlag;
  bool m_noRectSliceConstraintFlag;
  bool m_oneSlicePerSubpicConstraintFlag;
  bool m_noSubpicInfoConstraintFlag;
  bool m_intraOnlyConstraintFlag;

  //====== Coding Structure ========
  int       m_intraPeriod;                        // needs to be signed to allow '-1' for no intra period
  uint32_t  m_decodingRefreshType;            ///< the type of decoding refresh employed for the random access.
  bool      m_rewriteParamSets;
  bool      m_idrRefParamList;
  int       m_gopSize;
  RPLEntry  m_RPLList0[MAX_GOP];
  RPLEntry  m_RPLList1[MAX_GOP];
  int       m_numRPLList0;
  int       m_numRPLList1;
  GOPEntry  m_GOPList[MAX_GOP];
  int       m_maxDecPicBuffering[MAX_TLAYER];
  int       m_maxNumReorderPics[MAX_TLAYER];
  int       m_drapPeriod;
  int       m_edrapPeriod;

  int       m_iQP;                              //  if (AdaptiveQP == OFF)
  int       m_qpRefAdj;

  ChromaQpMappingTableParams m_chromaQpMappingTableParams;
  int       m_intraQPOffset;                    ///< QP offset for intra slice (integer)
  int       m_lambdaFromQPEnable;               ///< enable lambda derivation from QP
  double    m_lambdaScaleTowardsNextQP;         ///< scale lambda towards lambda of next QP
  bool      m_AccessUnitDelimiter;               ///< add Access Unit Delimiter NAL units
  bool      m_enablePictureHeaderInSliceHeader;  ///< Enable Picture Header in Slice Header

  int m_maxRefPicNum;   ///< this is used to mimic the sliding mechanism used by the decoder
                        // TODO: We need to have a common sliding mechanism used by both the encoder and decoder

  int       m_maxTempLayer;                      ///< Max temporal layer
  bool      m_isLowDelay;
  unsigned  m_CTUSize;
  bool                  m_subPicInfoPresentFlag;
  uint32_t              m_numSubPics;
  bool                  m_subPicSameSizeFlag;
  std::vector<uint32_t> m_subPicCtuTopLeftX;
  std::vector<uint32_t> m_subPicCtuTopLeftY;
  std::vector<uint32_t> m_subPicWidth;
  std::vector<uint32_t> m_subPicHeight;
  std::vector<bool>     m_subPicTreatedAsPicFlag;
  std::vector<bool>     m_loopFilterAcrossSubpicEnabledFlag;
  bool                  m_subPicIdMappingExplicitlySignalledFlag;
  bool                  m_subPicIdMappingInSpsFlag;
  unsigned              m_subPicIdLen;
  std::vector<uint16_t> m_subPicId;
#if GDR_ENABLED
  bool      m_gdrEnabled;
  unsigned  m_gdrPocStart;
  unsigned  m_gdrPeriod;
  int       m_gdrInterval;
  bool      m_gdrNoHash;
#endif
  bool      m_useSplitConsOverride;
  unsigned  m_minQt[3];   // 0: I slice; 1: P/B slice, 2: I slice chroma
  unsigned  m_maxBt[3];   // 0: I slice; 1: P/B slice, 2: I slice chroma
  unsigned  m_maxTt[3];   // 0: I slice; 1: P/B slice, 2: I slice chroma
  unsigned  m_uiMaxMTTHierarchyDepth;
  unsigned  m_uiMaxMTTHierarchyDepthI;
  unsigned  m_uiMaxMTTHierarchyDepthIChroma;
  int       m_ttFastSkip;
  double    m_ttFastSkipThr;
  bool      m_dualITree;
  unsigned  m_maxCUWidth;
  unsigned  m_maxCUHeight;
  unsigned m_log2MinCUSize;

  int       m_LMChroma;
  bool      m_horCollocatedChromaFlag;
  bool      m_verCollocatedChromaFlag;
  int       m_explicitMtsIntra;
  int       m_explicitMtsInter;
  int       m_MTSIntraMaxCand;
  int       m_MTSInterMaxCand;
  int       m_implicitMtsIntra;
  bool      m_SBT;                                ///< Sub-Block Transform for inter blocks
  int       m_SBTFast64WidthTh;                   ///< Enable size-64 SBT in encoder RDO check for HD and above sequences

  bool      m_LFNST;
  bool      m_useFastLFNST;
  bool      m_sbTmvpEnableFlag;
  bool      m_Affine;
  bool      m_AffineType;
  bool      m_adaptBypassAffineMe;
  bool      m_PROF;
  bool      m_BIO;

  bool      m_SMVD;
  bool      m_compositeRefEnabled;        //composite reference
  bool      m_bcw;
  bool      m_BcwFast;
  bool      m_ladfEnabled;
  int       m_ladfNumIntervals;
  int       m_ladfQpOffset[MAX_LADF_INTERVALS];
  int       m_ladfIntervalLowerBound[MAX_LADF_INTERVALS];

  bool      m_ciip;
  bool      m_Geo;
  bool      m_allowDisFracMMVD;
  bool      m_AffineAmvr;
  bool      m_useHashMeInCurrentIntraPeriod;
  bool      m_HashMECfgEnable;
  bool      m_AffineAmvrEncOpt;
  bool      m_AffineAmvp;
  bool      m_DMVR;
  bool      m_MMVD;
  int       m_MmvdDisNum;
  bool      m_rgbFormat;
  bool      m_useColorTrans;
  unsigned  m_PLTMode;
  bool      m_jointCbCrMode;
  unsigned  m_IBCMode;
  unsigned  m_IBCLocalSearchRangeX;
  unsigned  m_IBCLocalSearchRangeY;
  unsigned  m_IBCHashSearch;
  unsigned  m_IBCHashSearchMaxCand;
  unsigned  m_IBCHashSearchRange4SmallBlk;
  unsigned  m_IBCFastMethod;
  bool      m_dmvrEncSelect;
  int       m_dmvrEncSelectBaseQpTh;
  bool      m_dmvrEncSelectDisableHighestTemporalLayer;
  int       m_dmvrDisableTemporalLayers;

  bool      m_wrapAround;
  unsigned  m_wrapAroundOffset;

  // ADD_NEW_TOOL : (encoder lib) add tool enabling flags and associated parameters here
  bool      m_virtualBoundariesEnabledFlag;
  bool      m_virtualBoundariesPresentFlag;
  unsigned  m_numVerVirtualBoundaries;
  unsigned  m_numHorVirtualBoundaries;
  unsigned  m_virtualBoundariesPosX[3];
  unsigned  m_virtualBoundariesPosY[3];
  bool      m_lmcsEnabled;
  unsigned  m_reshapeSignalType;
  unsigned  m_intraCMD;
  ReshapeCW m_reshapeCW;
  int       m_CSoffset;
  bool      m_encDbOpt;
  bool      m_encALFOpt;
  bool      m_useFastLCTU;
  bool      m_useFastMrg;
  int       m_maxMergeRdCandNumTotal;
  int       m_mergeRdCandQuotaRegular;
  int       m_mergeRdCandQuotaRegularSmallBlk;
  int       m_mergeRdCandQuotaSubBlk;
  int       m_mergeRdCandQuotaCiip;
  int       m_mergeRdCandQuotaGpm;
  bool      m_usePbIntraFast;
  bool      m_useAMaxBT;
  bool      m_e0023FastEnc;
  bool      m_contentBasedFastQtbt;
  bool      m_useNonLinearAlfLuma;
  bool      m_useNonLinearAlfChroma;
  unsigned  m_maxNumAlfAlternativesChroma;
  bool      m_MRL;
  bool      m_MIP;
  bool      m_useFastMIP;
  int       m_fastLocalDualTreeMode;
  int       m_fastAdaptCostPredMode;
  bool      m_disableFastDecisionTT;
  uint32_t  m_log2MaxTbSize;
  bool      m_useMttSkip;

  //====== Loop/Deblock Filter ========
  bool      m_deblockingFilterDisable;
  bool      m_deblockingFilterOffsetInPPS;
  int       m_deblockingFilterBetaOffsetDiv2;
  int       m_deblockingFilterTcOffsetDiv2;
  int       m_deblockingFilterCbBetaOffsetDiv2;
  int       m_deblockingFilterCbTcOffsetDiv2;
  int       m_deblockingFilterCrBetaOffsetDiv2;
  int       m_deblockingFilterCrTcOffsetDiv2;
  int       m_deblockingFilterMetric;
  bool      m_useSao;
  bool      m_saoTrueOrg;
  bool      m_bTestSAODisableAtPictureLevel;
  double    m_saoEncodingRate;       // When non-0 SAO early picture termination is enabled for luma and chroma
  double    m_saoEncodingRateChroma; // The SAO early picture termination rate to use for chroma (when m_SaoEncodingRate is >0). If <=0, use results for luma.
  int       m_maxNumOffsetsPerPic;
  bool      m_saoCtuBoundary;

  bool      m_saoGreedyMergeEnc;
  //====== Motion search ========
  bool      m_bDisableIntraPUsInInterSlices;
  MESearchMethod m_motionEstimationSearchMethod;
  int            m_searchRange;   //  0:Full frame
  int       m_bipredSearchRange;
  bool      m_bClipForBiPredMeEnabled;
  bool      m_bFastMEAssumingSmootherMVEnabled;
  int       m_minSearchWindow;
  bool      m_bRestrictMESampling;

  //====== Quality control ========
  int       m_iMaxDeltaQP;                      //  Max. absolute delta QP (1:default)
  int       m_cuQpDeltaSubdiv;                  //  Max. subdivision level for a CuDQP (0:default)
  unsigned  m_cuChromaQpOffsetSubdiv;           ///< Max. subdivision level for a chroma QP adjustment (0:default)
  bool      m_cuChromaQpOffsetEnabled;          ///< Local chroma QP offset enable flag
  std::vector<ChromaQpAdj> m_cuChromaQpOffsetList; ///< Local chroma QP offsets list (to be signalled in PPS)

  int       m_chromaCbQpOffset;                 //  Chroma Cb QP Offset (0:default)
  int       m_chromaCrQpOffset;                 //  Chroma Cr Qp Offset (0:default)
  int       m_chromaCbQpOffsetDualTree;         //  Chroma Cb QP Offset for dual tree
  int       m_chromaCrQpOffsetDualTree;         //  Chroma Cr Qp Offset for dual tree
  int       m_chromaCbCrQpOffset;               //  QP Offset for the joint Cb-Cr mode
  int       m_chromaCbCrQpOffsetDualTree;       //  QP Offset for the joint Cb-Cr mode in dual tree
#if ER_CHROMA_QP_WCG_PPS
  WCGChromaQPControl m_wcgChromaQpControl;                    ///< Wide-colour-gamut chroma QP control.
#endif
#if W0038_CQP_ADJ
  uint32_t      m_sliceChromaQpOffsetPeriodicity;                 ///< Used in conjunction with Slice Cb/Cr QpOffsetIntraOrPeriodic. Use 0 (default) to disable periodic nature.
  int       m_sliceChromaQpOffsetIntraOrPeriodic[2/*Cb,Cr*/]; ///< Chroma Cb QP Offset at slice level for I slice or for periodic inter slices as defined by SliceChromaQPOffsetPeriodicity. Replaces offset in the GOP table.
#endif

  ChromaFormat m_chromaFormatIdc;

  bool      m_extendedPrecisionProcessingFlag;
  bool      m_tsrcRicePresentFlag;
  bool      m_reverseLastSigCoeffEnabledFlag;
  bool      m_highPrecisionOffsetsEnabledFlag;
  bool      m_bUseAdaptiveQP;
  int       m_iQPAdaptationRange;
#if ENABLE_QPA
  bool      m_bUsePerceptQPA;
  bool      m_bUseWPSNR;
#endif

  //====== Tool list ========
  BitDepths m_inputBitDepth;   // bit-depth of input file
  BitDepths m_bitDepth;

  bool      m_bUseASR;
  bool      m_bUseHADME;
  bool      m_useRDOQ;
  bool      m_useRDOQTS;
  bool      m_useSelectiveRDOQ;
  FastInterSearchMode m_fastInterSearchMode;
  bool      m_bUseEarlyCU;
  bool      m_useFastDecisionForMerge;
  bool      m_useEarlySkipDetection;
  bool      m_reconBasedCrossCPredictionEstimate;
  bool      m_useTransformSkip;
  bool      m_useTransformSkipFast;
  bool      m_useChromaTS;
  bool      m_useBDPCM;
  uint32_t      m_log2MaxTransformSkipBlockSize;
  bool      m_transformSkipRotationEnabledFlag;
  bool      m_transformSkipContextEnabledFlag;
  bool      m_rrcRiceExtensionEnableFlag;
  bool      m_persistentRiceAdaptationEnabledFlag;
  bool      m_cabacBypassAlignmentEnabledFlag;
#if SHARP_LUMA_DELTA_QP
  LumaLevelToDeltaQPMapping m_lumaLevelToDeltaQPMapping; ///< mapping from luma level to delta QP.
#endif
  bool      m_smoothQPReductionEnable;
  int       m_smoothQPReductionPeriodicity;
  double    m_smoothQPReductionThresholdIntra;
  double    m_smoothQPReductionModelScaleIntra;
  double    m_smoothQPReductionModelOffsetIntra;
  int       m_smoothQPReductionLimitIntra;
  double    m_smoothQPReductionThresholdInter;
  double    m_smoothQPReductionModelScaleInter;
  double    m_smoothQPReductionModelOffsetInter;
  int       m_smoothQPReductionLimitInter;

  FrameDeltaQps m_frameDeltaQps;

  uint32_t      m_uiDeltaQpRD;
  bool      m_bFastDeltaQP;
  bool      m_ISP;
  bool      m_useFastISP;

  bool      m_bFastUDIUseMPMEnabled;
  bool      m_bFastMEForGenBLowDelayEnabled;
  bool      m_gopBasedTemporalFilterEnabled;
  bool      m_bimEnabled;
  std::map<int, int*> m_adaptQPmap;
  bool      m_noPicPartitionFlag;                             ///< no picture partitioning flag (single tile, single slice)
  bool      m_mixedLossyLossless;                             ///< enable mixed lossy/lossless coding

  bool      m_dpfEnabled;
  int       m_dpfKeyLen;
  int       m_dpfNonkeyLen;

  std::vector<uint16_t> m_sliceLosslessArray;                      ///< Slice lossless array
  std::vector<uint32_t> m_tileColumnWidth;                    ///< tile column widths in units of CTUs (last column width will be repeated uniformly to cover any remaining picture width)
  std::vector<uint32_t> m_tileRowHeight;                      ///< tile row heights in units of CTUs (last row height will be repeated uniformly to cover any remaining picture height)
  bool      m_rectSliceFlag;                                  ///< indicates if using rectangular or raster-scan slices
  uint32_t  m_numSlicesInPic;                                 ///< number of rectangular slices in the picture (raster-scan slice specified at slice level)
  bool      m_tileIdxDeltaPresentFlag;                        ///< rectangular slice tile index delta present flag
  std::vector<RectSlice> m_rectSlices;                        ///< list of rectanglar slice syntax parameters
  std::vector<uint32_t> m_rasterSliceSize;                    ///< raster-scan slice sizes in units of tiles
  bool      m_bLFCrossTileBoundaryFlag;                       ///< 1: filter across tile boundaries  0: do not filter across tile boundaries
  bool      m_bLFCrossSliceBoundaryFlag;                      ///< 1: filter across slice boundaries 0: do not filter across slice boundaries

  //====== Sub-picture and Slices ========
  bool      m_singleSlicePerSubPicFlag;
  bool      m_entropyCodingSyncEnabledFlag;
  bool      m_entryPointPresentFlag;                           ///< flag for the presence of entry points

  HashType  m_decodedPictureHashSEIType;
  HashType  m_subpicDecodedPictureHashType;
  bool      m_bufferingPeriodSEIEnabled;
  bool      m_pictureTimingSEIEnabled;
  bool      m_frameFieldInfoSEIEnabled;
  bool      m_dependentRAPIndicationSEIEnabled;
  bool      m_edrapIndicationSEIEnabled;
  bool      m_framePackingSEIEnabled;
  int       m_framePackingSEIType;
  int       m_framePackingSEIId;
  int       m_framePackingSEIQuincunx;
  int       m_framePackingSEIInterpretation;
  bool      m_doSEIEnabled;
  bool      m_doSEICancelFlag;
  bool      m_doSEIPersistenceFlag;
  int       m_doSEITransformType;
  bool      m_parameterSetsInclusionIndicationSEIEnabled;
#if GREEN_METADATA_SEI_ENABLED
  bool      m_greenMetadataInfoSEIEnabled;
  int      m_greenMetadataType;
  int      m_greenMetadataGranularityType;
  int      m_greenMetadataExtendedRepresentation;
  int      m_greenMetadataPeriodType;
  int      m_greenMetadataPeriodNumSeconds;
  int      m_greenMetadataPeriodNumPictures;
  //Metrics for quality recovery after low-power encoding
  int      m_xsdNumberMetrics;
  bool     m_xsdMetricTypePSNR;
  bool     m_xsdMetricTypeSSIM;
  bool     m_xsdMetricTypeVMAF;
  bool     m_xsdMetricTypeWPSNR;
  bool     m_xsdMetricTypeWSPSNR;
  bool     m_xsdMetricTypeEstimatedEnergy;
#if GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464
  uint32_t                          m_greenMetadataAMIFlags;
  uint8_t                           m_greenMetadataAMIDisplayModel;
  uint8_t                           m_greenMetadataAMIApproximationModel;
  uint8_t                           m_greenMetadataAMIMapNumber;
  std::vector<uint8_t>              m_greenMetadataAMILayerId;
  std::vector<uint8_t>              m_greenMetadataAMIOlsNumber;
  std::vector<std::vector<uint8_t>> m_greenMetadataAMIOlsId;
  std::vector<uint8_t>              m_greenMetadataAMIEnergyReductionRate;
  std::vector<uint8_t>              m_greenMetadataAMIVideoQualityMetricType;
  std::vector<uint16_t>             m_greenMetadataAMIVideoQualityLevel;
  std::vector<uint8_t>              m_greenMetadataAMIMaxValue;
  std::vector<uint8_t>              m_greenMetadataAMIAttenuationUseIdc;
  std::vector<uint8_t>              m_greenMetadataAMIAttenuationCompIdc;
  std::vector<bool>                 m_greenMetadataAMIPreprocessingFlag;
  std::vector<uint8_t>              m_greenMetadataAMIPreprocessingTypeIdc;
  std::vector<uint8_t>              m_greenMetadataAMIPreprocessingScaleIdc;
  std::vector<uint8_t>              m_greenMetadataAMIBacklightScalingIdc;
#endif
#endif
  bool      m_selfContainedClvsFlag;
  bool      m_bpDeltasGOPStructure;
  bool      m_decodingUnitInfoSEIEnabled;

  bool      m_scalableNestingSEIEnabled;

  bool      m_erpSEIEnabled;
  bool      m_erpSEICancelFlag;
  bool      m_erpSEIPersistenceFlag;
  bool      m_erpSEIGuardBandFlag;
  uint32_t  m_erpSEIGuardBandType;
  uint32_t  m_erpSEILeftGuardBandWidth;
  uint32_t  m_erpSEIRightGuardBandWidth;
  bool      m_sphereRotationSEIEnabled;
  bool      m_sphereRotationSEICancelFlag;
  bool      m_sphereRotationSEIPersistenceFlag;
  int       m_sphereRotationSEIYaw;
  int       m_sphereRotationSEIPitch;
  int       m_sphereRotationSEIRoll;
  bool      m_omniViewportSEIEnabled;
  uint32_t  m_omniViewportSEIId;
  bool      m_omniViewportSEICancelFlag;
  bool      m_omniViewportSEIPersistenceFlag;
  uint32_t  m_omniViewportSEICntMinus1;
  std::vector<int>      m_omniViewportSEIAzimuthCentre;
  std::vector<int>      m_omniViewportSEIElevationCentre;
  std::vector<int>      m_omniViewportSEITiltCentre;
  std::vector<uint32_t> m_omniViewportSEIHorRange;
  std::vector<uint32_t> m_omniViewportSEIVerRange;
  bool                  m_rwpSEIEnabled;
  bool                  m_rwpSEIRwpCancelFlag;
  bool                  m_rwpSEIRwpPersistenceFlag;
  bool                  m_rwpSEIConstituentPictureMatchingFlag;
  int                   m_rwpSEINumPackedRegions;
  int                   m_rwpSEIProjPictureWidth;
  int                   m_rwpSEIProjPictureHeight;
  int                   m_rwpSEIPackedPictureWidth;
  int                   m_rwpSEIPackedPictureHeight;
  std::vector<uint8_t>  m_rwpSEIRwpTransformType;
  std::vector<bool>     m_rwpSEIRwpGuardBandFlag;
  std::vector<uint32_t> m_rwpSEIProjRegionWidth;
  std::vector<uint32_t> m_rwpSEIProjRegionHeight;
  std::vector<uint32_t> m_rwpSEIRwpSEIProjRegionTop;
  std::vector<uint32_t> m_rwpSEIProjRegionLeft;
  std::vector<uint16_t> m_rwpSEIPackedRegionWidth;
  std::vector<uint16_t> m_rwpSEIPackedRegionHeight;
  std::vector<uint16_t> m_rwpSEIPackedRegionTop;
  std::vector<uint16_t> m_rwpSEIPackedRegionLeft;
  std::vector<uint8_t>  m_rwpSEIRwpLeftGuardBandWidth;
  std::vector<uint8_t>  m_rwpSEIRwpRightGuardBandWidth;
  std::vector<uint8_t>  m_rwpSEIRwpTopGuardBandHeight;
  std::vector<uint8_t>  m_rwpSEIRwpBottomGuardBandHeight;
  std::vector<bool>     m_rwpSEIRwpGuardBandNotUsedForPredFlag;
  std::vector<uint8_t>  m_rwpSEIRwpGuardBandType;
  bool                 m_gcmpSEIEnabled;
  bool                 m_gcmpSEICancelFlag;
  bool                 m_gcmpSEIPersistenceFlag;
  uint8_t              m_gcmpSEIPackingType;
  uint8_t              m_gcmpSEIMappingFunctionType;
  std::vector<uint8_t> m_gcmpSEIFaceIndex;
  std::vector<uint8_t> m_gcmpSEIFaceRotation;
  std::vector<double>  m_gcmpSEIFunctionCoeffU;
  std::vector<bool>    m_gcmpSEIFunctionUAffectedByVFlag;
  std::vector<double>  m_gcmpSEIFunctionCoeffV;
  std::vector<bool>    m_gcmpSEIFunctionVAffectedByUFlag;
  bool                 m_gcmpSEIGuardBandFlag;
  uint8_t              m_gcmpSEIGuardBandType;
  bool                 m_gcmpSEIGuardBandBoundaryExteriorFlag;
  uint8_t              m_gcmpSEIGuardBandSamplesMinus1;
  EncCfgParam::CfgSEISubpictureLevel m_cfgSubpictureLevelInfoSEI;
#if JVET_AJ0151_DSC_SEI
  EncCfgParam::CfgSEIDigitallySignedContent m_cfgDigitallySignedContentSEI;
#endif
  bool                  m_sampleAspectRatioInfoSEIEnabled;
  bool                  m_sariCancelFlag;
  bool                  m_sariPersistenceFlag;
  int                   m_sariAspectRatioIdc;
  int                   m_sariSarWidth;
  int                   m_sariSarHeight;
  bool                  m_phaseIndicationSEIEnabledFullResolution;
  int                   m_horPhaseNumFullResolution;
  int                   m_horPhaseDenMinus1FullResolution;
  int                   m_verPhaseNumFullResolution;
  int                   m_verPhaseDenMinus1FullResolution;
  bool                  m_phaseIndicationSEIEnabledReducedResolution;
  int                   m_horPhaseNumReducedResolution;
  int                   m_horPhaseDenMinus1ReducedResolution;
  int                   m_verPhaseNumReducedResolution;
  int                   m_verPhaseDenMinus1ReducedResolution;
  bool      m_MCTSEncConstraint;
  SEIMasteringDisplay m_masteringDisplay;
  bool      m_alternativeTransferCharacteristicsSEIEnabled;
  uint8_t     m_preferredTransferCharacteristics;

  bool                    m_siiSEIEnabled;
  uint32_t                m_siiSEINumUnitsInShutterInterval;
  uint32_t                m_siiSEITimeScale;
  std::vector<uint32_t>   m_siiSEISubLayerNumUnitsInSI;
  bool     m_sptiSEIEnabled;
  bool     m_sptiSourceTimingEqualsOutputTimingFlag;
  uint32_t m_sptiSourceType;
  uint32_t m_sptiTimeScale;
  uint32_t m_sptiNumUnitsInElementalInterval;
  bool     m_sptiDirectionFlag;

  bool                    m_nnPostFilterSEICharacteristicsEnabled;
  bool                    m_nnPostFilterSEICharacteristicsUseSuffixSEI;
  int                     m_nnPostFilterSEICharacteristicsNumFilters;
  uint32_t                m_nnPostFilterSEICharacteristicsId[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsModeIdc[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsPropertyPresentFlag[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsBaseFlag[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsPurpose[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsOutSubCFlag[MAX_NUM_NN_POST_FILTERS];
  ChromaFormat            m_nnPostFilterSEICharacteristicsOutColourFormatIdc[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsChromaLocInfoPresentFlag[MAX_NUM_NN_POST_FILTERS];
  Chroma420LocType        m_nnPostFilterSEICharacteristicsChromaSampleLocTypeFrame[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsPicWidthNumeratorMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsPicWidthDenominatorMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsPicHeightNumeratorMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsPicHeightDenominatorMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsInpTensorBitDepthLumaMinus8[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsInpTensorBitDepthChromaMinus8[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsOutTensorBitDepthLumaMinus8[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsOutTensorBitDepthChromaMinus8[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsComponentLastFlag[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsInpFormatIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsAuxInpIdc[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsSepColDescriptionFlag[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsFullRangeFlag[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsColPrimaries[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsTransCharacteristics[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsMatrixCoeffs[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsInpOrderIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsOutFormatIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsOutOrderIdc[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsConstantPatchSizeFlag[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsPatchWidthMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsPatchHeightMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsExtendedPatchWidthCdDeltaMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsExtendedPatchHeightCdDeltaMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsOverlap[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsPaddingType[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsLumaPadding[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsCrPadding[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsCbPadding[MAX_NUM_NN_POST_FILTERS];
  std::string             m_nnPostFilterSEICharacteristicsPayloadFilename[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsComplexityInfoPresentFlag[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsApplicationPurposeTagUriPresentFlag[MAX_NUM_NN_POST_FILTERS];
  std::string             m_nnPostFilterSEICharacteristicsApplicationPurposeTagUri[MAX_NUM_NN_POST_FILTERS];
#if NNPFC_SCAN_TYPE_IDC
  uint32_t                m_nnPostFilterSEICharacteristicsScanTypeIdc[MAX_NUM_NN_POST_FILTERS];
#endif
  uint32_t                m_nnPostFilterSEICharacteristicsForHumanViewingIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsForMachineAnalysisIdc[MAX_NUM_NN_POST_FILTERS];
  std::string             m_nnPostFilterSEICharacteristicsUriTag[MAX_NUM_NN_POST_FILTERS];
  std::string             m_nnPostFilterSEICharacteristicsUri[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsParameterTypeIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsLog2ParameterBitLengthMinus3[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsNumParametersIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsNumKmacOperationsIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsTotalKilobyteSize[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsNumberInputDecodedPicturesMinus1[MAX_NUM_NN_POST_FILTERS];
  std::vector<uint32_t>   m_nnPostFilterSEICharacteristicsNumberInterpolatedPictures[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsNumberExtrapolatedPicturesMinus1[MAX_NUM_NN_POST_FILTERS];
  int32_t                 m_nnPostFilterSEICharacteristicsSpatialExtrapolationLeftOffset[MAX_NUM_NN_POST_FILTERS];
  int32_t                 m_nnPostFilterSEICharacteristicsSpatialExtrapolationRightOffset[MAX_NUM_NN_POST_FILTERS];
  int32_t                 m_nnPostFilterSEICharacteristicsSpatialExtrapolationTopOffset[MAX_NUM_NN_POST_FILTERS];
  int32_t                 m_nnPostFilterSEICharacteristicsSpatialExtrapolationBottomOffset[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsInbandPromptFlag[MAX_NUM_NN_POST_FILTERS];
  std::string             m_nnPostFilterSEICharacteristicsPrompt[MAX_NUM_NN_POST_FILTERS];
  std::vector<bool>       m_nnPostFilterSEICharacteristicsInputPicOutputFlag[MAX_NUM_NN_POST_FILTERS];
  bool                    m_nnPostFilterSEICharacteristicsAbsentInputPicZeroFlag[MAX_NUM_NN_POST_FILTERS];
#if JVET_AK0326_NNPF_SEED
  bool                    m_nnPostFilterSEICharacteristicsInbandSeedFlag[MAX_NUM_NN_POST_FILTERS];
  uint32_t                m_nnPostFilterSEICharacteristicsSeed[MAX_NUM_NN_POST_FILTERS];
#endif

  bool                    m_nnPostFilterSEIActivationEnabled;
  bool                    m_nnPostFilterSEIActivationUseSuffixSEI;
  uint32_t                m_nnPostFilterSEIActivationTargetId;
  bool                    m_nnPostFilterSEIActivationCancelFlag;
  bool                    m_nnPostFilterSEIActivationTargetBaseFlag;
  bool                    m_nnPostFilterSEIActivationNoPrevCLVSFlag;
  bool                    m_nnPostFilterSEIActivationNoFollCLVSFlag;
  bool                    m_nnPostFilterSEIActivationPersistenceFlag;
  std::vector<bool>       m_nnPostFilterSEIActivationOutputflag;
#if JVET_AJ0104_NNPFA_PROMPT_UPDATE
  bool                    m_nnPostFilterSEIActivationPromptUpdateFlag;
  std::string             m_nnPostFilterSEIActivationPrompt;
#endif
#if JVET_AK0326_NNPF_SEED
  bool                    m_nnPostFilterSEIActivationSeedUpdateFlag;
  uint32_t                m_nnPostFilterSEIActivationSeed;
#endif
#if JVET_AJ0114_NNPFA_NUM_PIC_SHIFT
#if JVET_AL0075_NNPFA_SELECTED_INPUT_FLAG
  bool                    m_nnPostFilterSEIActivationSelectedInputFlag;
#endif
  uint32_t                m_nnPostFilterSEIActivationNumInputPicShift;
#endif

  // Encoder Optimization Information SEI
  bool  m_eoiSEIEnabled;
  bool  m_eoiSEICancelFlag;
  bool  m_eoiSEIPersistenceFlag;
  uint32_t  m_eoiSEIForHumanViewingIdc;
  uint32_t  m_eoiSEIForMachineAnalysisIdc;
  uint32_t m_eoiSEIType;
  uint32_t m_eoiSEIObjectBasedIdc;
  uint32_t m_eoiSEIQuantThresholdDelta;
  bool     m_eoiSEIPicQuantObjectFlag;
  bool m_eoiSEITemporalResamplingTypeFlag;
  uint32_t m_eoiSEINumIntPics;
#if JVET_AJ0183_EOI_SEI_SRC_PIC_FLAG
  bool     m_eoiSEISrcPicFlag; ;
#endif
  bool     m_eoiSEIOrigPicDimensionsFlag;
#if JVET_AL0123_AL0310_EOI
  uint32_t m_eoiSEIOrigPicWidthMinus1;
  uint32_t m_eoiSEIOrigPicHeightMinus1;
  uint32_t m_eoiSEISpatialHorResamplingTypeIdc;
  uint32_t m_eoiSEISpatialVerResamplingTypeIdc;
#else
  uint32_t m_eoiSEIOrigPicWidth;
  uint32_t m_eoiSEIOrigPicHeight;
  bool m_eoiSEISpatialResamplingTypeFlag;
#endif
  uint32_t m_eoiSEIPrivacyProtectionTypeIdc;
  uint32_t m_eoiSEIPrivacyProtectedInfoType;

  // Modality Information SEI
  bool        m_miSEIEnabled;
  bool        m_miCancelFlag;
  bool        m_miPersistenceFlag;
  int         m_miModalityType; 
  bool        m_miSpectrumRangePresentFlag;
  int         m_miMinWavelengthMantissa; 
  int         m_miMinWavelengthExponentPlus15; 
  int         m_miMaxWavelengthMantissa; 
  int         m_miMaxWavelengthExponentPlus15; 

  // film grain characterstics sei
  bool      m_fgcSEIEnabled;
  bool      m_fgcSEICancelFlag;
  bool      m_fgcSEIPersistenceFlag;
  uint8_t   m_fgcSEIModelID;
  bool      m_fgcSEISepColourDescPresentFlag;
  uint8_t   m_fgcSEIBlendingModeID;
  uint8_t   m_fgcSEILog2ScaleFactor;
  bool      m_fgcSEICompModelPresent[MAX_NUM_COMPONENT];
  bool      m_fgcSEIAnalysisEnabled;
  std::string m_fgcSEIExternalMask;
  std::string m_fgcSEIExternalDenoised;
  int       m_fgcSEITemporalFilterPastRefs;
  int       m_fgcSEITemporalFilterFutureRefs;
  std::map<int, double> m_fgcSEITemporalFilterStrengths;
  bool      m_fgcSEIPerPictureSEI;
  uint8_t   m_fgcSEINumModelValuesMinus1          [MAX_NUM_COMPONENT];
  uint8_t   m_fgcSEINumIntensityIntervalMinus1    [MAX_NUM_COMPONENT];
  uint8_t   m_fgcSEIIntensityIntervalLowerBound   [MAX_NUM_COMPONENT][MAX_NUM_INTENSITIES];
  uint8_t   m_fgcSEIIntensityIntervalUpperBound   [MAX_NUM_COMPONENT][MAX_NUM_INTENSITIES];
  uint32_t  m_fgcSEICompModelValue                [MAX_NUM_COMPONENT][MAX_NUM_INTENSITIES][MAX_NUM_MODEL_VALUES];
#if JVET_AL0339_FGS_SEI_SPATIAL_RESOLUTION
  bool      m_fgcSEISpatialResolutionPresentFlag;
  int       m_fgcSEIPicWidthInLumaSamples;
  int       m_fgcSEIPicHeightInLumaSamples;
#endif
// cll SEI
  bool      m_cllSEIEnabled;
  uint16_t  m_cllSEIMaxContentLevel;
  uint16_t  m_cllSEIMaxPicAvgLevel;
// ave sei
  bool      m_aveSEIEnabled;
  uint32_t  m_aveSEIAmbientIlluminance;
  uint16_t  m_aveSEIAmbientLightX;
  uint16_t  m_aveSEIAmbientLightY;
  // colour tranform information sei
  bool      m_ctiSEIEnabled;
  uint32_t  m_ctiSEIId;
  bool      m_ctiSEISignalInfoFlag;
  bool      m_ctiSEIFullRangeFlag;
  uint32_t  m_ctiSEIPrimaries;
  uint32_t  m_ctiSEITransferFunction;
  uint32_t  m_ctiSEIMatrixCoefs;
  bool      m_ctiSEICrossComponentFlag;
  bool      m_ctiSEICrossComponentInferred;
  uint32_t  m_ctiSEINumberChromaLut;
  int       m_ctiSEIChromaOffset;
  LutModel  m_ctiSEILut[MAX_NUM_COMPONENT];
// ccv sei
  bool      m_ccvSEIEnabled;
  bool      m_ccvSEICancelFlag;
  bool      m_ccvSEIPersistenceFlag;
  bool      m_ccvSEIPrimariesPresentFlag;
  bool      m_ccvSEIMinLuminanceValuePresentFlag;
  bool      m_ccvSEIMaxLuminanceValuePresentFlag;
  bool      m_ccvSEIAvgLuminanceValuePresentFlag;
  double    m_ccvSEIPrimariesX[MAX_NUM_COMPONENT];
  double    m_ccvSEIPrimariesY[MAX_NUM_COMPONENT];
  double    m_ccvSEIMinLuminanceValue;
  double    m_ccvSEIMaxLuminanceValue;
  double    m_ccvSEIAvgLuminanceValue;
  // sdi sei
  bool              m_sdiSEIEnabled;
  int               m_sdiSEIMaxLayersMinus1;
  bool              m_sdiSEIMultiviewInfoFlag;
  bool              m_sdiSEIAuxiliaryInfoFlag;
  int               m_sdiSEIViewIdLenMinus1;
  std::vector<uint32_t>  m_sdiSEILayerId;
  std::vector<uint32_t>  m_sdiSEIViewIdVal;
  std::vector<uint32_t>  m_sdiSEIAuxId;
  std::vector<uint32_t>  m_sdiSEINumAssociatedPrimaryLayersMinus1;
  std::vector<uint32_t>  m_sdiSEIAssociatedPrimaryLayerIdx;
  // mai sei
  bool              m_maiSEIEnabled;
  bool              m_maiSEIIntrinsicParamFlag;
  bool              m_maiSEIExtrinsicParamFlag;
  int               m_maiSEINumViewsMinus1;
  bool              m_maiSEIIntrinsicParamsEqualFlag;
  int               m_maiSEIPrecFocalLength;
  int               m_maiSEIPrecPrincipalPoint;
  int               m_maiSEIPrecSkewFactor;
  std::vector<bool> m_maiSEISignFocalLengthX;
  std::vector<uint32_t>  m_maiSEIExponentFocalLengthX;
  std::vector<uint32_t>  m_maiSEIMantissaFocalLengthX;
  std::vector<bool> m_maiSEISignFocalLengthY;
  std::vector<uint32_t>  m_maiSEIExponentFocalLengthY;
  std::vector<uint32_t>  m_maiSEIMantissaFocalLengthY;
  std::vector<bool> m_maiSEISignPrincipalPointX;
  std::vector<uint32_t>  m_maiSEIExponentPrincipalPointX;
  std::vector<uint32_t>  m_maiSEIMantissaPrincipalPointX;
  std::vector<bool> m_maiSEISignPrincipalPointY;
  std::vector<uint32_t>  m_maiSEIExponentPrincipalPointY;
  std::vector<uint32_t>  m_maiSEIMantissaPrincipalPointY;
  std::vector<bool> m_maiSEISignSkewFactor;
  std::vector<uint32_t>  m_maiSEIExponentSkewFactor;
  std::vector<uint32_t>  m_maiSEIMantissaSkewFactor;
  int               m_maiSEIPrecRotationParam;
  int               m_maiSEIPrecTranslationParam;
  // mvp sei
  bool              m_mvpSEIEnabled;
  int               m_mvpSEINumViewsMinus1;
  std::vector<uint32_t>  m_mvpSEIViewPosition;
  // aci sei
  bool      m_aciSEIEnabled;
  bool      m_aciSEICancelFlag;
  int       m_aciSEIUseIdc;
  int       m_aciSEIBitDepthMinus8;
  int       m_aciSEITransparentValue;
  int       m_aciSEIOpaqueValue;
  bool      m_aciSEIIncrFlag;
  bool      m_aciSEIClipFlag;
  bool      m_aciSEIClipTypeFlag;
  // dri sei
  bool      m_driSEIEnabled;
  bool      m_driSEIZNearFlag;
  bool      m_driSEIZFarFlag;
  bool      m_driSEIDMinFlag;
  bool      m_driSEIDMaxFlag;
  double    m_driSEIZNear;
  double    m_driSEIZFar;
  double    m_driSEIDMin;
  double    m_driSEIDMax;
  int       m_driSEIDepthRepresentationType;
  int       m_driSEIDisparityRefViewId;
  int       m_driSEINonlinearNumMinus1;
  std::vector<uint32_t> m_driSEINonlinearModel;
  std::string           m_arSEIFileRoot;  // Annotated region SEI - initialized from external file
  std::string           m_omiSEIFileRoot;  // Object mask information SEI - initialized from external file

  bool m_SEIManifestSEIEnabled;
  bool m_SEIPrefixIndicationSEIEnabled;
  //SEI message processing order
  bool                  m_poSEIEnabled;
  uint32_t              m_poSEIId;
  uint32_t              m_poSEIForHumanViewingIdc;
  uint32_t              m_poSEIForMachineAnalysisIdc;
  uint32_t              m_poSEINumMinus2;
  bool                  m_poSEIBreadthFirstFlag;
  std::vector<bool>     m_poSEIWrappingFlag;
  std::vector<bool>     m_poSEIImportanceFlag;
  std::vector<bool>     m_poSEIProcessingDegreeFlag;
  std::vector<bool>     m_poSEIPrefixFlag;
  std::vector<uint16_t> m_poSEIPayloadType;
  std::vector<uint16_t>  m_poSEIProcessingOrder;
  std::vector<uint16_t> m_poSEINumOfPrefixBits;
#if JVET_AJ0105_SPO_COMPLEXITY_INFO
  bool                  m_poSEIComplexityInfoPresentFlag;
  uint32_t              m_poSEIParameterTypeIdc;
  uint32_t              m_poSEILog2ParameterBitLengthMinus3;
  uint32_t              m_poSEINumParametersIdc;
  uint32_t              m_poSEINumKmacOperationIdc;
  uint32_t              m_poSEITotalKilobyteSize;
#endif
  std::vector<std::vector<uint8_t>>  m_poSEIPrefixByte;
  bool                 m_postFilterHintSEIEnabled;
  bool                 m_postFilterHintSEICancelFlag;
  bool                 m_postFilterHintSEIPersistenceFlag;
  uint32_t             m_postFilterHintSEISizeY;
  uint32_t             m_postFilterHintSEISizeX;
  uint32_t             m_postFilterHintSEIType;
  bool                 m_postFilterHintSEIChromaCoeffPresentFlag;
  std::vector<int32_t> m_postFilterHintValues;
  uint16_t                 m_textDescriptionSEIId;
  bool                     m_textSEICancelFlag;
  bool                     m_textSEIIDCancelFlag;
  bool                     m_textSEIPersistenceFlag;
  uint8_t                  m_textSEIDescriptionPurpose;
  uint8_t                  m_textSEINumStringsMinus1;
  std::vector<std::string> m_textSEIDescriptionStringLang;
  std::vector<std::string> m_textSEIDescriptionString;

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  bool     m_priSEIEnabled;
  bool     m_priSEICancelFlag;
  bool     m_priSEIPersistenceFlag;
  uint32_t m_priSEINumRegionsMinus1;
#if JVET_AL0324_AL0070_PRI_SEI
  bool     m_priSEIMultilayerFlag;
#endif
  bool     m_priSEIUseMaxDimensionsFlag;
  uint32_t m_priSEILog2UnitSize;
  uint32_t m_priSEIRegionSizeLenMinus1;
  bool     m_priSEIRegionIdPresentFlag;
  bool     m_priSEITargetPicParamsPresentFlag;
  uint32_t m_priSEITargetPicWidthMinus1;
  uint32_t m_priSEITargetPicHeightMinus1;
  uint32_t m_priSEINumResamplingRatiosMinus1;
  std::vector<uint32_t> m_priSEIResamplingWidthNumMinus1;
  std::vector<uint32_t> m_priSEIResamplingWidthDenomMinus1;
  std::vector<bool>     m_priSEIFixedAspectRatioFlag;
  std::vector<uint32_t> m_priSEIResamplingHeightNumMinus1;
  std::vector<uint32_t> m_priSEIResamplingHeightDenomMinus1;
  std::vector<uint32_t> m_priSEIRegionId;
#if JVET_AL0324_AL0070_PRI_SEI
  std::vector<uint32_t> m_priSEIRegionLayerId;
  std::vector<bool>     m_priSEIRegionIsALayerFlag;
#endif
  std::vector<uint32_t> m_priSEIRegionTopLeftInUnitsX;
  std::vector<uint32_t> m_priSEIRegionTopLeftInUnitsY;
  std::vector<uint32_t> m_priSEIRegionWidthInUnitsMinus1;
  std::vector<uint32_t> m_priSEIRegionHeightInUnitsMinus1;
  std::vector<uint32_t> m_priSEIResamplingRatioIdx;
#if JVET_AL0324_AL0070_PRI_SEI
  std::vector<uint32_t> m_priSEITargetRegionTopLeftInUnitsX;
  std::vector<uint32_t> m_priSEITargetRegionTopLeftInUnitsY;
#else
  std::vector<uint32_t> m_priSEITargetRegionTopLeftX;
  std::vector<uint32_t> m_priSEITargetRegionTopLeftY;
  bool                  m_priSEIMultilayerFlag;
  std::vector<uint32_t> m_priSEIRegionLayerId;
  std::vector<bool>     m_priSEIRegionIsALayerFlag;
#endif
#endif

  bool      m_constrainedRaslEncoding;

#if  JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
  bool  m_aurSEIEnabled;
  bool  m_aurSEICancelFlag;
  bool  m_aurSEIPersistenceFlag;
  uint32_t  m_aurSEINumRestrictionsMinus1;
  std::vector<uint32_t>  m_aurSEIRestrictions;
  std::vector<bool>  m_aurSEIContextPresentFlag;
  std::vector<uint32_t>  m_aurSEIContext;
#endif

  //====== Weighted Prediction ========
  bool      m_useWeightedPred;       //< Use of Weighting Prediction (P_SLICE)
  bool      m_useWeightedBiPred;    //< Use of Bi-directional Weighting Prediction (B_SLICE)
  WeightedPredictionMethod m_weightedPredictionMethod;
  uint32_t      m_log2ParallelMergeLevelMinus2;       ///< Parallel merge estimation region
  uint32_t      m_maxNumMergeCand;                    ///< Maximum number of merge candidates
  uint32_t      m_maxNumAffineMergeCand;              ///< Maximum number of affine merge candidates
  uint32_t      m_maxNumGeoCand;
  uint32_t      m_maxNumIBCMergeCand;                 ///< Max number of IBC merge candidates
  ScalingListMode m_useScalingListId;             ///< Using quantization matrix i.e. 0=off, 1=default, 2=file.
  std::string m_scalingListFileName;              ///< quantization matrix file name

  bool      m_disableScalingMatrixForAlternativeColourSpace;
  bool      m_scalingMatrixDesignatedColourSpace;
  bool      m_sliceLevelRpl;                      ///< code reference picture lists in slice headers rather than picture header
  bool      m_sliceLevelDblk;                     ///< code deblocking filter parameters in slice headers rather than picture header
  bool      m_sliceLevelSao;                      ///< code SAO parameters in slice headers rather than picture header
  bool      m_sliceLevelAlf;                      ///< code ALF parameters in slice headers rather than picture header
  bool      m_sliceLevelWp;                       ///< code weighted prediction parameters in slice headers rather than picture header
  bool      m_sliceLevelDeltaQp;                  ///< code delta in slice headers rather than picture header
  bool      m_disableScalingMatrixForLfnstBlks;
  int       m_TMVPModeId;
  bool      m_constantSliceHeaderParamsEnabledFlag;
  int       m_PPSDepQuantEnabledIdc;
  int       m_PPSRefPicListSPSIdc0;
  int       m_PPSRefPicListSPSIdc1;
  int       m_PPSMvdL1ZeroIdc;
  int       m_PPSCollocatedFromL0Idc;
  uint32_t  m_PPSSixMinusMaxNumMergeCandPlus1;
  uint32_t  m_PPSMaxNumMergeCandMinusMaxNumGeoCandPlus1;
  bool      m_DepQuantEnabledFlag;
  bool      m_SignDataHidingEnabledFlag;
  bool      m_rcEnableRateControl = false;
  int       m_rcTargetBitrate;
  int       m_rcKeepHierarchicalBit;
  bool      m_rcCtuLevelRateControl;
  bool      m_rcUseCtuSeparateModel;
  int       m_rcInitialQp;
  bool      m_rcForceIntraQp;
  bool      m_rcCpbSaturationEnabled = false;
  uint32_t  m_rcCpbSize;
  double    m_rcInitialCpbFullness;
  CostMode  m_costMode;                                       ///< The cost function to use, primarily when considering lossless coding.
  bool      m_TSRCdisableLL;                                  ///< Disable TSRC for lossless

  OPI       m_opi;
  bool      m_OPIEnabled;                                     ///< enable Operating Point Information (OPI)
  bool      m_rplOfDepLayerInSh;

  DCI       m_dci;
  bool      m_DCIEnabled;                                     ///< enable Decoding Capability Information (DCI)

  bool      m_recalculateQPAccordingToLambda;                 ///< recalculate QP value according to the lambda value
  bool      m_hrdParametersPresentFlag;                       ///< enable generation of HRD parameters
  bool      m_vuiParametersPresentFlag;                       ///< enable generation of VUI parameters
  bool      m_samePicTimingInAllOLS;                          ///< same picture timing SEI message is used in all OLS
  bool      m_aspectRatioInfoPresentFlag;                     ///< Signals whether aspect_ratio_idc is present
  int       m_aspectRatioIdc;                                 ///< aspect_ratio_idc
  int       m_sarWidth;                                       ///< horizontal size of the sample aspect ratio
  int       m_sarHeight;                                      ///< vertical size of the sample aspect ratio
  bool      m_colourDescriptionPresentFlag;                   ///< Signals whether colour_primaries, transfer_characteristics and matrix_coefficients are present
  int       m_colourPrimaries;                                ///< Indicates chromaticity coordinates of the source primaries
  int       m_transferCharacteristics;                        ///< Indicates the opto-electronic transfer characteristics of the source
  int       m_matrixCoefficients;                             ///< Describes the matrix coefficients used in deriving luma and chroma from RGB primaries
  bool      m_progressiveSourceFlag;                          ///< Indicates if the content is progressive
  bool      m_interlacedSourceFlag;                           ///< Indicates if the content is interlaced
  bool      m_chromaLocInfoPresentFlag;                       ///< Signals whether chroma_sample_loc_type_top_field and chroma_sample_loc_type_bottom_field are present
  Chroma420LocType m_chromaSampleLocTypeTopField;      // Specifies the location of chroma samples for top field
  Chroma420LocType m_chromaSampleLocTypeBottomField;   // Specifies the location of chroma samples for bottom field
  Chroma420LocType m_chromaSampleLocType;   // Specifies the location of chroma samples for progressive content
  bool      m_overscanInfoPresentFlag;                        ///< Signals whether overscan_appropriate_flag is present
  bool      m_overscanAppropriateFlag;                        ///< Indicates whether conformant decoded pictures are suitable for display using overscan
  bool      m_videoFullRangeFlag;                             ///< Indicates the black level and range of luma and chroma signals

  bool m_fieldSeqFlag;
  bool m_efficientFieldIRAPEnabled;   /// enable to code fields in a specific, potentially more efficient, order.
  bool m_harmonizeGopFirstFieldCoupleEnabled;

  std::string m_summaryOutFilename;                           ///< filename to use for producing summary output file.
  std::string m_summaryPicFilenameBase;                       ///< Base filename to use for producing summary picture output files. The actual filenames used will have I.txt, P.txt and B.txt appended.
  uint32_t        m_summaryVerboseness;                           ///< Specifies the level of the verboseness of the text output.
  int       m_ImvMode;
  int       m_Imv4PelFast;
  std::string m_decodeBitstreams[2];                          ///< filename for decode bitstreams.
  bool        m_forceDecodeBitstream1;                        ///< guess what it means
  int         m_switchPOC;                                    ///< dbg poc.
  int         m_switchDQP;                                    ///< dqp applied to  switchPOC and subsequent pictures.
  int         m_fastForwardToPOC;                             ///<
  bool        m_stopAfterFFtoPOC;                             ///<
  int         m_debugCTU;                                     ///< dbg ctu
  bool        m_bs2ModPOCAndType;

  EncCfgParam::CfgVPSParameters m_cfgVPSParameters;

  int         m_maxNumAlfAps{ ALF_CTB_MAX_NUM_APS };
  int         m_alfapsIDShift{ 0 };
  bool        m_constantJointCbCrSignFlag;
  bool        m_alf;                                          ///< Adaptive Loop Filter
  bool        m_alfTrueOrg;
  double      m_alfStrengthLuma;
  bool        m_alfAllowPredefinedFilters;
  double      m_ccalfStrength;
  double      m_alfStrengthChroma;
  double      m_alfStrengthTargetLuma;
  double      m_alfStrengthTargetChroma;
  double      m_ccalfStrengthTarget;
  bool        m_ccalf;
  int         m_ccalfQpThreshold;
#if JVET_O0756_CALCULATE_HDRMETRICS
  double                       m_whitePointDeltaE[hdrtoolslib::NB_REF_WHITE];
  double                       m_maxSampleValue;
  hdrtoolslib::SampleRange     m_sampleRange;
  hdrtoolslib::ColorPrimaries  m_colorPrimaries;
  bool                         m_enableTFunctionLUT;
  hdrtoolslib::ChromaLocation  m_chromaLocation[2];
  int                          m_chromaUPFilter;
  int                          m_cropOffsetLeft;
  int                          m_cropOffsetTop;
  int                          m_cropOffsetRight;
  int                          m_cropOffsetBottom;
  bool                         m_calculateHdrMetrics;
#endif
  double      m_scalingRatioHor;
  double      m_scalingRatioVer;
  bool        m_gopBasedRPREnabledFlag;
  int         m_gopBasedRPRQPThreshold;
  double      m_scalingRatioHor2;
  double      m_scalingRatioVer2;
  double      m_scalingRatioHor3;
  double      m_scalingRatioVer3;
  double      m_psnrThresholdRPR;
  double      m_psnrThresholdRPR2;
  double      m_psnrThresholdRPR3;
  int         m_qpOffsetRPR;
  int         m_qpOffsetRPR2;
  int         m_qpOffsetRPR3;
  int         m_qpOffsetChromaRPR;
  int         m_qpOffsetChromaRPR2;
  int         m_qpOffsetChromaRPR3;
  int         m_rprSwitchingResolutionOrderList[MAX_RPR_SWITCHING_ORDER_LIST_SIZE];
  int         m_rprSwitchingQPOffsetOrderList[MAX_RPR_SWITCHING_ORDER_LIST_SIZE];
  int         m_rprSwitchingListSize;
  bool        m_rprFunctionalityTestingEnabledFlag;
  bool        m_rprPopulatePPSatIntraFlag;
  int         m_rprSwitchingSegmentSize;
  double      m_rprSwitchingTime;
  bool        m_rprEnabledFlag;
  bool        m_resChangeInClvsEnabled;
  int         m_switchPocPeriod;
  int         m_upscaledOutput;
  int         m_upscaleFilterForDisplay;
  int         m_numRefLayers[MAX_VPS_LAYERS];
  bool        m_avoidIntraInDepLayer;
  bool        m_craAPSreset;
  bool        m_rprRASLtoolSwitch;
  bool        m_refLayerMetricsEnabled;
  bool        m_explicitILRP;
  bool        m_encILOpt;
  double      m_encILOptLambdaModifier;
  bool                                 m_generativeFaceVideoEnabled;
  uint32_t                             m_generativeFaceVideoSEINumber;
  bool                                 m_generativeFaceVideoSEIBasePicFlag;
  bool                                 m_generativeFaceVideoSEINNPresentFlag;
  uint32_t                             m_generativeFaceVideoSEINNModeIdc;
  std::string                          m_generativeFaceVideoSEINNTagURI;
  std::string                          m_generativeFaceVideoSEINNURI;
  bool                                 m_generativeFaceVideoSEIChromaKeyInfoPresentFlag;
  std::vector<bool>                    m_generativeFaceVideoSEIChromaKeyValuePresentFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEIChromaKeyValue;
  std::vector<bool>                    m_generativeFaceVideoSEIChromaKeyThrPresentFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEIChromaKeyThrValue;
  std::vector<bool>                    m_generativeFaceVideoSEIDrivePicFusionFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEIId;
  std::vector<uint32_t>                m_generativeFaceVideoSEICnt;
  std::vector<bool>                    m_generativeFaceVideoSEILowConfidenceFaceParameterFlag;
  std::vector<bool>                    m_generativeFaceVideoSEICoordinatePresentFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEICoordinateQuantizationFactor;
  std::vector<bool>                    m_generativeFaceVideoSEICoordinatePredFlag;
  std::vector<bool>                    m_generativeFaceVideoSEI3DCoordinateFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEICoordinatePointNum;
  std::vector<std::vector<double>>     m_generativeFaceVideoSEICoordinateXTesonr;
  std::vector<std::vector<double>>     m_generativeFaceVideoSEICoordinateYTesonr;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIZCoordinateMaxValue;
  std::vector<std::vector<double>>     m_generativeFaceVideoSEICoordinateZTesonr;
  std::vector<bool>                    m_generativeFaceVideoSEIMatrixPresentFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEIMatrixElementPrecisionFactor;
  std::vector<bool>                    m_generativeFaceVideoSEIMatrixPredFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEINumMatrixType;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIMatrixTypeIdx;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIMatrix3DSpaceFlag;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEINumMatricestoNumKpsFlag;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEINumMatricesInfo;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEINumMatrices;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIMatrixWidth;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIMatrixHeight;
  std::vector<std::vector<std::vector<std::vector<std::vector<double>>>>>   m_generativeFaceVideoSEIMatrixElement;
  std::string                          m_generativeFaceVideoSEIPayloadFilename;
  bool                                 m_generativeFaceVideoEnhancementEnabled;
  uint32_t                             m_generativeFaceVideoEnhancementSEINumber;
  bool                                 m_generativeFaceVideoEnhancementSEIBasePicFlag;
  bool                                 m_generativeFaceVideoEnhancementSEINNPresentFlag;
  uint32_t                             m_generativeFaceVideoEnhancementSEINNModeIdc;
  std::string                          m_generativeFaceVideoEnhancementSEINNTagURI;
  std::string                          m_generativeFaceVideoEnhancementSEINNURI;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIId;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIGFVId;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIGFVCnt;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor;
  std::vector<bool>                    m_generativeFaceVideoEnhancementSEIMatrixPresentFlag;
  std::vector<bool>                    m_generativeFaceVideoEnhancementSEIMatrixPredFlag;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEINumMatrices;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoEnhancementSEIMatrixWidth;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoEnhancementSEIMatrixHeight;
  std::vector<std::vector<std::vector<std::vector<double>>>>   m_generativeFaceVideoEnhancementSEIMatrixElement;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIPupilPresentIdx;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor;
  std::vector<double>                  m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX;
  std::vector<double>                  m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY;
  std::vector<double>                  m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX;
  std::vector<double>                  m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY;
  std::string                          m_generativeFaceVideoEnhancementSEIPayloadFilename;

public:
  EncCfg()
  {
  }

  virtual ~EncCfg()
  {}
  std::map<uint32_t, SEIAnnotatedRegions::AnnotatedRegionObject> m_arObjects;
  void setProfile(Profile::Name profile) { m_profile = profile; }
  void setTierLevel(Level::Tier tier, Level::Name level) { m_tier = tier; m_level = level; }
  bool      getFrameOnlyConstraintFlag() const { return m_frameOnlyConstraintFlag; }
  void      setFrameOnlyConstraintFlag(bool b) { m_frameOnlyConstraintFlag = b;    }
  bool      getMultiLayerEnabledFlag() const   { return m_multiLayerEnabledFlag;   }
  void      setMultiLayerEnabledFlag(bool b)   { m_multiLayerEnabledFlag = b;      }
  void setNumSubProfile( uint8_t numSubProfile) { m_numSubProfile = numSubProfile; m_subProfile.resize(m_numSubProfile); }
  void setSubProfile( int i, uint32_t subProfile) { m_subProfile[i] = subProfile; }

  bool      getOnePictureOnlyConstraintFlag() const                        { return m_onePictureOnlyConstraintFlag; }
  void      setOnePictureOnlyConstraintFlag(bool b)                        { m_onePictureOnlyConstraintFlag=b; }

  bool      getIntraOnlyConstraintFlag() const { return m_bIntraOnlyConstraintFlag; }
  void      setIntraOnlyConstraintFlag(bool val) { m_bIntraOnlyConstraintFlag = val; }
  uint32_t  getMaxBitDepthConstraintIdc() const { return m_maxBitDepthConstraintIdc; }
  void      setMaxBitDepthConstraintIdc(uint32_t u) { m_maxBitDepthConstraintIdc = u; }
  ChromaFormat  getMaxChromaFormatConstraintIdc() const { return m_maxChromaFormatConstraintIdc; }
  void          setMaxChromaFormatConstraintIdc(ChromaFormat cf) { m_maxChromaFormatConstraintIdc = cf; }
  bool          getGciPresentFlag() const { return m_gciPresentFlag; }
  void          setGciPresentFlag(bool b) { m_gciPresentFlag = b; }
  bool          getAllLayersIndependentConstraintFlag() const { return m_allLayersIndependentConstraintFlag; }
  void          setAllLayersIndependentConstraintFlag(bool val) { m_allLayersIndependentConstraintFlag = val; }
  bool          getNoMrlConstraintFlag() const { return m_noMrlConstraintFlag; }
  void          setNoMrlConstraintFlag(bool val) { m_noMrlConstraintFlag = val; }
  bool          getNoIspConstraintFlag() const { return m_noIspConstraintFlag; }
  void          setNoIspConstraintFlag(bool val) { m_noIspConstraintFlag = val; }
  bool          getNoMipConstraintFlag() const { return m_noMipConstraintFlag; }
  void          setNoMipConstraintFlag(bool val) { m_noMipConstraintFlag = val; }
  bool          getNoLfnstConstraintFlag() const { return m_noLfnstConstraintFlag; }
  void          setNoLfnstConstraintFlag(bool val) { m_noLfnstConstraintFlag = val; }
  bool          getNoMmvdConstraintFlag() const { return m_noMmvdConstraintFlag; }
  void          setNoMmvdConstraintFlag(bool val) { m_noMmvdConstraintFlag = val; }
  bool          getNoSmvdConstraintFlag() const { return m_noSmvdConstraintFlag; }
  void          setNoSmvdConstraintFlag(bool val) { m_noSmvdConstraintFlag = val; }
  bool          getNoProfConstraintFlag() const { return m_noProfConstraintFlag; }
  void          setNoProfConstraintFlag(bool val) { m_noProfConstraintFlag = val; }
  bool          getNoPaletteConstraintFlag() const { return m_noPaletteConstraintFlag; }
  void          setNoPaletteConstraintFlag(bool val) { m_noPaletteConstraintFlag = val; }
  bool          getNoActConstraintFlag() const { return m_noActConstraintFlag; }
  void          setNoActConstraintFlag(bool val) { m_noActConstraintFlag = val; }
  bool          getNoLmcsConstraintFlag() const { return m_noLmcsConstraintFlag; }
  void          setNoLmcsConstraintFlag(bool val) { m_noLmcsConstraintFlag = val; }
  bool          getNoExplicitScaleListConstraintFlag() const { return m_noExplicitScaleListConstraintFlag; }
  void          setNoExplicitScaleListConstraintFlag(bool val) { m_noExplicitScaleListConstraintFlag = val; }
  bool          getNoVirtualBoundaryConstraintFlag() const { return m_noVirtualBoundaryConstraintFlag; }
  void          setNoVirtualBoundaryConstraintFlag(bool val) { m_noVirtualBoundaryConstraintFlag = val; }
  bool          getNoMttConstraintFlag() const { return m_noMttConstraintFlag; }
  void          setNoMttConstraintFlag(bool val) { m_noMttConstraintFlag = val; }
  bool      getNoChromaQpOffsetConstraintFlag() const { return m_noChromaQpOffsetConstraintFlag; }
  void      setNoChromaQpOffsetConstraintFlag(bool bVal) { m_noChromaQpOffsetConstraintFlag = bVal; }
  bool      getNoQtbttDualTreeIntraConstraintFlag() const { return m_noQtbttDualTreeIntraConstraintFlag; }
  void      setNoQtbttDualTreeIntraConstraintFlag(bool val) { m_noQtbttDualTreeIntraConstraintFlag = val; }
  int       getMaxLog2CtuSizeConstraintIdc() const { return m_maxLog2CtuSizeConstraintIdc; }
  void      setMaxLog2CtuSizeConstraintIdc(int u) { m_maxLog2CtuSizeConstraintIdc = u; }
  bool      getNoPartitionConstraintsOverrideConstraintFlag() const { return m_noPartitionConstraintsOverrideConstraintFlag; }
  void      setNoPartitionConstraintsOverrideConstraintFlag(bool val) { m_noPartitionConstraintsOverrideConstraintFlag = val; }
  bool      getNoSaoConstraintFlag() const { return m_noSaoConstraintFlag; }
  void      setNoSaoConstraintFlag(bool val) { m_noSaoConstraintFlag = val; }
  bool      getNoAlfConstraintFlag() const { return m_noAlfConstraintFlag; }
  void      setNoAlfConstraintFlag(bool val) { m_noAlfConstraintFlag = val; }
  bool      getNoCCAlfConstraintFlag() const { return m_noCCAlfConstraintFlag; }
  void      setNoCCAlfConstraintFlag(bool val) { m_noCCAlfConstraintFlag = val; }
  bool      getWeightedPredictionConstraintFlag() const { return m_noWeightedPredictionConstraintFlag; }
  void      setNoWeightedPredictionConstraintFlag(bool val) { m_noWeightedPredictionConstraintFlag = val; }
  bool      getNoRefWraparoundConstraintFlag() const { return m_noRefWraparoundConstraintFlag; }
  void      setNoRefWraparoundConstraintFlag(bool val) { m_noRefWraparoundConstraintFlag = val; }
  bool      getNoTemporalMvpConstraintFlag() const { return m_noTemporalMvpConstraintFlag; }
  void      setNoTemporalMvpConstraintFlag(bool val) { m_noTemporalMvpConstraintFlag = val; }
  bool      getNoSbtmvpConstraintFlag() const { return m_noSbtmvpConstraintFlag; }
  void      setNoSbtmvpConstraintFlag(bool val) { m_noSbtmvpConstraintFlag = val; }
  bool      getNoAmvrConstraintFlag() const { return m_noAmvrConstraintFlag; }
  void      setNoAmvrConstraintFlag(bool val) { m_noAmvrConstraintFlag = val; }
  bool      getNoBdofConstraintFlag() const { return m_noBdofConstraintFlag; }
  void      setNoBdofConstraintFlag(bool val) { m_noBdofConstraintFlag = val; }
  bool      getNoDmvrConstraintFlag() const { return m_noDmvrConstraintFlag; }
  void      setNoDmvrConstraintFlag(bool val) { m_noDmvrConstraintFlag = val; }
  bool      getNoCclmConstraintFlag() const { return m_noCclmConstraintFlag; }
  void      setNoCclmConstraintFlag(bool val) { m_noCclmConstraintFlag = val; }
  bool      getNoMtsConstraintFlag() const { return m_noMtsConstraintFlag; }
  void      setNoMtsConstraintFlag(bool val) { m_noMtsConstraintFlag = val; }
  bool      getNoSbtConstraintFlag() const { return m_noSbtConstraintFlag; }
  void      setNoSbtConstraintFlag(bool val) { m_noSbtConstraintFlag = val; }
  bool      getNoAffineMotionConstraintFlag() const { return m_noAffineMotionConstraintFlag; }
  void      setNoAffineMotionConstraintFlag(bool val) { m_noAffineMotionConstraintFlag = val; }
  bool      getNoBcwConstraintFlag() const { return m_noBcwConstraintFlag; }
  void      setNoBcwConstraintFlag(bool val) { m_noBcwConstraintFlag = val; }
  bool      getNoIbcConstraintFlag() const { return m_noIbcConstraintFlag; }
  void      setNoIbcConstraintFlag(bool val) { m_noIbcConstraintFlag = val; }
  bool      getNoCiipConstraintFlag() const { return m_noCiipConstraintFlag; }
  void      setNoCiipConstraintFlag(bool val) { m_noCiipConstraintFlag = val; }
  bool      getNoGeoConstraintFlag() const { return m_noGeoConstraintFlag; }
  void      setNoGeoConstraintFlag(bool val) { m_noGeoConstraintFlag = val; }
  bool      getNoLadfConstraintFlag() const { return m_noLadfConstraintFlag; }
  void      setNoLadfConstraintFlag(bool val) { m_noLadfConstraintFlag = val; }
  bool      getNoTransformSkipConstraintFlag() const { return m_noTransformSkipConstraintFlag; }
  void      setNoTransformSkipConstraintFlag(bool val) { m_noTransformSkipConstraintFlag = val; }
  bool      getNoLumaTransformSize64ConstraintFlag() const { return m_noLumaTransformSize64ConstraintFlag; }
  void      setNoLumaTransformSize64ConstraintFlag(bool val) { m_noLumaTransformSize64ConstraintFlag = val; }
  bool      getNoBDPCMConstraintFlag() const { return m_noBDPCMConstraintFlag; }
  void      setNoBDPCMConstraintFlag(bool val) { m_noBDPCMConstraintFlag = val; }
  bool      getNoJointCbCrConstraintFlag() const { return m_noJointCbCrConstraintFlag; }
  void      setNoJointCbCrConstraintFlag(bool val) { m_noJointCbCrConstraintFlag = val; }
  bool      getNoCuQpDeltaConstraintFlag() const { return m_noCuQpDeltaConstraintFlag; }
  void      setNoCuQpDeltaConstraintFlag(bool val) { m_noCuQpDeltaConstraintFlag = val; }
  bool      getNoDepQuantConstraintFlag() const { return m_noDepQuantConstraintFlag; }
  void      setNoDepQuantConstraintFlag(bool val) { m_noDepQuantConstraintFlag = val; }
  bool      getNoSignDataHidingConstraintFlag() const { return m_noSignDataHidingConstraintFlag; }
  void      setNoSignDataHidingConstraintFlag(bool val) { m_noSignDataHidingConstraintFlag = val; }
  bool      getNoTrailConstraintFlag() const { return m_noTrailConstraintFlag; }
  void      setNoTrailConstraintFlag(bool val) { m_noTrailConstraintFlag = val; }
  bool      getNoStsaConstraintFlag() const { return m_noStsaConstraintFlag; }
  void      setNoStsaConstraintFlag(bool val) { m_noStsaConstraintFlag = val; }
  bool      getNoRaslConstraintFlag() const { return m_noRaslConstraintFlag; }
  void      setNoRaslConstraintFlag(bool val) { m_noRaslConstraintFlag = val; }
  bool      getNoRadlConstraintFlag() const { return m_noRadlConstraintFlag; }
  void      setNoRadlConstraintFlag(bool val) { m_noRadlConstraintFlag = val; }
  bool      getNoIdrConstraintFlag() const { return m_noIdrConstraintFlag; }
  void      setNoIdrConstraintFlag(bool val) { m_noIdrConstraintFlag = val; }
  bool      getNoCraConstraintFlag() const { return m_noCraConstraintFlag; }
  void      setNoCraConstraintFlag(bool val) { m_noCraConstraintFlag = val; }
  bool      getNoGdrConstraintFlag() const { return m_noGdrConstraintFlag; }
  void      setNoGdrConstraintFlag(bool val) { m_noGdrConstraintFlag = val; }
  bool      getNoApsConstraintFlag() const { return m_noApsConstraintFlag; }
  void      setNoApsConstraintFlag(bool val) { m_noApsConstraintFlag = val; }
  bool      getAllRapPicturesFlag() const { return m_allRapPicturesFlag; }
  void      setAllRapPicturesFlag(bool val) { m_allRapPicturesFlag = val; }
  bool      getNoExtendedPrecisionProcessingConstraintFlag() const { return m_noExtendedPrecisionProcessingConstraintFlag; }
  void      setNoExtendedPrecisionProcessingConstraintFlag(bool val) { m_noExtendedPrecisionProcessingConstraintFlag = val; }
  bool      getNoTsResidualCodingRiceConstraintFlag() const { return m_noTsResidualCodingRiceConstraintFlag; }
  void      setNoTsResidualCodingRiceConstraintFlag(bool val) { m_noTsResidualCodingRiceConstraintFlag = val; }
  bool      getNoRrcRiceExtensionConstraintFlag() const { return m_noRrcRiceExtensionConstraintFlag; }
  void      setNoRrcRiceExtensionConstraintFlag(bool val) { m_noRrcRiceExtensionConstraintFlag = val; }
  bool      getNoPersistentRiceAdaptationConstraintFlag() const { return m_noPersistentRiceAdaptationConstraintFlag; }
  void      setNoPersistentRiceAdaptationConstraintFlag(bool val) { m_noPersistentRiceAdaptationConstraintFlag = val; }
  bool      getNoReverseLastSigCoeffConstraintFlag() const { return m_noReverseLastSigCoeffConstraintFlag; }
  void      setNoReverseLastSigCoeffConstraintFlag(bool val) { m_noReverseLastSigCoeffConstraintFlag = val; }

  void      setFrameRate(const Fraction& fr) { m_frameRate = fr; }
  void      setFrameSkip(uint32_t i) { m_frameSkip = i; }
  void      setTemporalSubsampleRatio       ( uint32_t  i )      { m_temporalSubsampleRatio = i; }
  void      setSourceWidth                  ( int   i )      { m_sourceWidth = i; }
  void      setSourceHeight                 ( int   i )      { m_sourceHeight = i; }

  Window   &getConformanceWindow()                           { return m_conformanceWindow; }
  void      setConformanceWindow(int confLeft, int confRight, int confTop, int confBottom)
  {
    m_conformanceWindow = Window(confLeft, confRight, confTop, confBottom);
  }
  void      setExplicitScalingWindowEnabled(bool enabled)    { m_explicitScalingWindowEnabled = enabled; }
  void      setScalingWindow(int scalingLeft, int scalingRight, int scalingTop, int scalingBottom)
  {
    m_scalingWindow = Window(scalingLeft, scalingRight, scalingTop, scalingBottom);
  }

  void      setFramesToBeEncoded            ( int   i )      { m_framesToBeEncoded = i; }

  void setValidFrames(const int first, const int last)
  {
    m_firstValidFrame = first;
    m_lastValidFrame  = last;
  }

  bool      getPrintMSEBasedSequencePSNR    ()         const { return m_printMSEBasedSequencePSNR;  }
  void      setPrintMSEBasedSequencePSNR    (bool value)     { m_printMSEBasedSequencePSNR = value; }

  bool      getPrintHexPsnr                 ()         const { return m_printHexPsnr;               }
  void      setPrintHexPsnr                 (bool value)     { m_printHexPsnr = value;              }

  bool      getPrintFrameMSE                ()         const { return m_printFrameMSE;              }
  void      setPrintFrameMSE                (bool value)     { m_printFrameMSE = value;             }

  bool      getPrintSequenceMSE             ()         const { return m_printSequenceMSE;           }
  void      setPrintSequenceMSE             (bool value)     { m_printSequenceMSE = value;          }

  bool      getPrintMSSSIM                  ()         const { return m_printMSSSIM;               }
  void      setPrintMSSSIM                  (bool value)     { m_printMSSSIM = value;              }

  bool      getPrintWPSNR                   ()         const { return m_printWPSNR;               }
  void      setPrintWPSNR                   (bool value)     { m_printWPSNR = value;              }

  bool getPrintHighPrecEncTime() const { return m_printHighPrecEncTime; }
  void setPrintHightPrecEncTime(bool val) { m_printHighPrecEncTime = val; }

  bool      getCabacZeroWordPaddingEnabled()           const { return m_cabacZeroWordPaddingEnabled;  }
  void      setCabacZeroWordPaddingEnabled(bool value)       { m_cabacZeroWordPaddingEnabled = value; }

  bool      getShutterFilterFlag()              const { return m_ShutterFilterEnable; }
  void      setShutterFilterFlag(bool value) { m_ShutterFilterEnable = value; }

  int       getBlendingRatioSII()             const { return m_SII_BlendingRatio; }
  void      setBlendingRatioSII(int value) { m_SII_BlendingRatio = value; }

  //====== Coding Structure ========
  void      setIntraPeriod                  (int   i)        { m_intraPeriod = i;                   }
  void      setDecodingRefreshType          ( int   i )      { m_decodingRefreshType = (uint32_t)i; }
  void      setReWriteParamSets             ( bool  b )      { m_rewriteParamSets = b; }
  void      setIDRRefParamListPresent       ( bool  b )      { m_idrRefParamList  = b; }
  bool      getIDRRefParamListPresent       ()        const  { return m_idrRefParamList; }
  void            setGOPSize(int i) { m_gopSize = i; }
  void      setGopList(const GOPEntry GOPList[MAX_GOP]) { for (int i = 0; i < MAX_GOP; i++) m_GOPList[i] = GOPList[i]; }
  const GOPEntry &getGOPEntry               ( int   i ) const { return m_GOPList[i]; }

  int getNumFramesInTemporalLayer(const int tId) const
  {
    int n = 0;
    for (int i = 0; i < m_gopSize; i++)
    {
      n += tId >= m_GOPList[i].m_temporalId ? 1 : 0;
    }
    return n;
  }

  void      setRPLList0(const RPLEntry RPLList[MAX_GOP])
  {
    m_numRPLList0 = 0;
    for (int i = 0; i < MAX_GOP; i++)
    {
      m_RPLList0[i] = RPLList[i];
      if (m_RPLList0[i].m_POC != -1) m_numRPLList0++;
    }
  }
  void      setRPLList1(const RPLEntry RPLList[MAX_GOP])
  {
    m_numRPLList1 = 0;
    for (int i = 0; i < MAX_GOP; i++)
    {
      m_RPLList1[i] = RPLList[i];
      if (m_RPLList1[i].m_POC != -1) m_numRPLList1++;
    }
  }
  const RPLEntry &getRPLEntry(int L01, int idx) const { return (L01 == 0) ? m_RPLList0[idx] : m_RPLList1[idx]; }
  int       getRPLCandidateSize(int L01) const { return  (L01 == 0) ? m_numRPLList0 : m_numRPLList1; }
  void      setEncodedFlag(uint32_t  i, bool value) { m_RPLList0[i].m_isEncoded = value; m_RPLList1[i].m_isEncoded = value; m_GOPList[i].m_isEncoded = value; }
  void      setMaxDecPicBuffering           ( uint32_t u, uint32_t tlayer ) { m_maxDecPicBuffering[tlayer] = u;    }
  void      setMaxNumReorderPics            ( int  i, uint32_t tlayer ) { m_maxNumReorderPics[tlayer] = i;    }
  void      setDrapPeriod                   (int drapPeriod) { m_drapPeriod = drapPeriod; }
  void      setEdrapPeriod                  (int edrapPeriod) { m_edrapPeriod = edrapPeriod; }

  void      setBaseQP                       ( int   i )      { m_iQP = i; }
  void      setQpRefAdj(int deltaQp) { m_qpRefAdj = deltaQp; }
  void      setIntraQPOffset                ( int   i )         { m_intraQPOffset = i; }
  void      setLambdaFromQPEnable           ( bool  b )         { m_lambdaFromQPEnable = b; }
  void      setChromaQpMappingTableParams   (const ChromaQpMappingTableParams &params) { m_chromaQpMappingTableParams = params; }

  void      setSourcePadding                ( int*  padding)                { for ( int i = 0; i < 2; i++ ) m_sourcePadding[i] = padding[i]; }

  int getMaxRefPicNum()
  {
    return m_maxRefPicNum;
  }
  void setMaxRefPicNum(int maxRefPicNum)
  {
    m_maxRefPicNum = maxRefPicNum;
  }

  int       getMaxTempLayer                 ()                              { return m_maxTempLayer;              }
  void      setMaxTempLayer                 ( int maxTempLayer )            { m_maxTempLayer = maxTempLayer;      }

  bool      getIsLowDelay                   ()                              { return m_isLowDelay;       }
  void      setIsLowDelay                   ( bool isLowDelay )             { m_isLowDelay = isLowDelay; }

  void      setCTUSize                      ( unsigned  u )      { m_CTUSize  = u; }
  void      setMinQTSizes(unsigned *minQT)
  {
    m_minQt[0] = minQT[0];
    m_minQt[1] = minQT[1];
    m_minQt[2] = minQT[2];
  }
  void setMaxBTSizes(unsigned *maxBT)
  {
    m_maxBt[0] = maxBT[0];
    m_maxBt[1] = maxBT[1];
    m_maxBt[2] = maxBT[2];
  }
  void setMaxTTSizes(unsigned *maxTT)
  {
    m_maxTt[0] = maxTT[0];
    m_maxTt[1] = maxTT[1];
    m_maxTt[2] = maxTT[2];
  }
#if GDR_ENABLED
  void      setGdrEnabled(bool b)       { m_gdrEnabled  = b; }
  void      setGdrPeriod(unsigned u)    { m_gdrPeriod   = u; }
  void      setGdrPocStart(unsigned u)  { m_gdrPocStart = u; }
  void      setGdrInterval(int i)
  {
    m_gdrInterval = i;
  }
  void setGdrNoHash(bool b)
  {
    m_gdrNoHash = b;
  }

  bool      getGdrEnabled()             { return m_gdrEnabled;  }
  unsigned  getGdrPeriod()              { return m_gdrPeriod;   }
  unsigned  getGdrPocStart()            { return m_gdrPocStart; }
  int       getGdrInterval()
  {
    return m_gdrInterval;
  }
  bool getGdrNoHash()
  {
    return m_gdrNoHash;
  }
#endif

  void      setMaxMTTHierarchyDepth         ( unsigned uiMaxMTTHierarchyDepth, unsigned uiMaxMTTHierarchyDepthI, unsigned uiMaxMTTHierarchyDepthIChroma )
                                                             { m_uiMaxMTTHierarchyDepth = uiMaxMTTHierarchyDepth; m_uiMaxMTTHierarchyDepthI = uiMaxMTTHierarchyDepthI; m_uiMaxMTTHierarchyDepthIChroma = uiMaxMTTHierarchyDepthIChroma; }
  unsigned  getMaxMTTHierarchyDepth         ()         const { return m_uiMaxMTTHierarchyDepth; }
  unsigned  getMaxMTTHierarchyDepthI        ()         const { return m_uiMaxMTTHierarchyDepthI; }
  unsigned  getMaxMTTHierarchyDepthIChroma  ()         const { return m_uiMaxMTTHierarchyDepthIChroma; }
  int       getCTUSize                      ()         const { return m_CTUSize; }
  void      setUseSplitConsOverride         (bool  n)        { m_useSplitConsOverride = n; }
  bool      getUseSplitConsOverride         ()         const { return m_useSplitConsOverride; }
  void      setFastTTskip                   (int val)        { m_ttFastSkip = val; }
  int       getFastTTskip                   ()         const { return m_ttFastSkip; }
  void      setFastTTskipThr                (double val)     { m_ttFastSkipThr = val; }
  double    getFastTTskipThr                ()         const { return m_ttFastSkipThr; }
  void      setDualITree                    ( bool b )       { m_dualITree = b; }
  bool      getDualITree                    ()         const { return m_dualITree; }
  void      setSubPicInfoPresentFlag                        (bool b)                    { m_subPicInfoPresentFlag = b; }
  void      setNumSubPics                               ( uint32_t u )              { CHECK( u >= MAX_NUM_SUB_PICS, "Maximum number of subpictures exceeded" );
                                                                                      m_numSubPics = u;
                                                                                      m_subPicCtuTopLeftX.resize(m_numSubPics);
                                                                                      m_subPicCtuTopLeftY.resize(m_numSubPics);
                                                                                      m_subPicWidth.resize(m_numSubPics);
                                                                                      m_subPicHeight.resize(m_numSubPics);
                                                                                      m_subPicTreatedAsPicFlag.resize(m_numSubPics);
                                                                                      m_loopFilterAcrossSubpicEnabledFlag.resize(m_numSubPics);
                                                                                      m_subPicId.resize(m_numSubPics);
                                                                                    }
  void      setSubPicSameSizeFlag                       (bool b)                    { m_subPicSameSizeFlag = b; }
  void      setSubPicCtuTopLeftX                        (uint32_t u, int i)         { m_subPicCtuTopLeftX[i] = u; }
  void      setSubPicCtuTopLeftY                        (uint32_t u, int i)         { m_subPicCtuTopLeftY[i] = u; }
  void      setSubPicWidth                              (uint32_t u, int i)         { m_subPicWidth[i] = u; }
  void      setSubPicHeight                             (uint32_t u, int i)         { m_subPicHeight[i] = u; }
  void      setSubPicTreatedAsPicFlag                   (bool b, int i)             { m_subPicTreatedAsPicFlag[i] = b; }
  void      setLoopFilterAcrossSubpicEnabledFlag        (bool b, int i)             { m_loopFilterAcrossSubpicEnabledFlag[i] = b; }
  void      setSubPicCtuTopLeftX                        (const std::vector<uint32_t> &v)   { CHECK(v.size() != (m_subPicSameSizeFlag ? 0 : m_numSubPics), "number of vector entries must be equal to numSubPics(subPicSameSize=0) or 0(subPicSameSize=1)"); m_subPicCtuTopLeftX = v; }
  void      setSubPicCtuTopLeftY                        (const std::vector<uint32_t> &v)   { CHECK(v.size() != (m_subPicSameSizeFlag ? 0 : m_numSubPics), "number of vector entries must be equal to numSubPics(subPicSameSize=0) or 0(subPicSameSize=1)"); m_subPicCtuTopLeftY = v; }
  void      setSubPicWidth                              (const std::vector<uint32_t> &v)   { CHECK(v.size() != (m_subPicSameSizeFlag ? 1 : m_numSubPics), "number of vector entries must be equal to numSubPics(subPicSameSize=0) or 1(subPicSameSize=1)"); m_subPicWidth = v; }
  void      setSubPicHeight                             (const std::vector<uint32_t> &v)   { CHECK(v.size() != (m_subPicSameSizeFlag ? 1 : m_numSubPics), "number of vector entries must be equal to numSubPics(subPicSameSize=0) or 1(subPicSameSize=1)"); m_subPicHeight = v; }
  void      setSubPicTreatedAsPicFlag                   (const std::vector<bool> &v)       { CHECK(v.size()!=m_numSubPics, "number of vector entries must be equal to numSubPics") ;m_subPicTreatedAsPicFlag = v; }
  void      setLoopFilterAcrossSubpicEnabledFlag        (const std::vector<bool> &v)       { CHECK(v.size()!=m_numSubPics, "number of vector entries must be equal to numSubPics") ;m_loopFilterAcrossSubpicEnabledFlag = v; }

  void      setSubPicIdMappingExplicitlySignalledFlag   (bool b)                    { m_subPicIdMappingExplicitlySignalledFlag = b; }
  void      setSubPicIdMappingInSpsFlag                 (bool b)                    { m_subPicIdMappingInSpsFlag = b; }
  void      setSubPicIdLen                              (uint32_t u)                { m_subPicIdLen = u; }
  void      setSubPicId                                 (uint32_t b, int i)         { m_subPicId[i] = b; }
  void      setSubPicId                                 (const std::vector<uint16_t> &v)   { CHECK(v.size()!=m_numSubPics, "number of vector entries must be equal to numSubPics"); m_subPicId = v; }

  bool      getSubPicInfoPresentFlag                    ()                          { return m_subPicInfoPresentFlag; }
  bool      getSubPicSameSizeFlag                       ()                          { return m_subPicSameSizeFlag; }
  uint32_t  getNumSubPics                               ()                          { return m_numSubPics; }
  uint32_t  getSubPicCtuTopLeftX                        (int i)                     { return m_subPicCtuTopLeftX[i]; }
  uint32_t  getSubPicCtuTopLeftY                        (int i)                     { return m_subPicCtuTopLeftY[i]; }
  uint32_t  getSubPicWidth                              (int i)                     { return m_subPicWidth[i]; }
  uint32_t  getSubPicHeight                             (int i)                     { return m_subPicHeight[i]; }
  bool      getSubPicTreatedAsPicFlag                   (int i)                     { return m_subPicTreatedAsPicFlag[i]; }
  uint32_t  getLoopFilterAcrossSubpicEnabledFlag        (int i)                     { return m_loopFilterAcrossSubpicEnabledFlag[i]; }
  bool      getSubPicIdMappingExplicitlySignalledFlag   ()                          { return m_subPicIdMappingExplicitlySignalledFlag; }
  bool      getSubPicIdMappingInSpsFlag                 ()                          { return m_subPicIdMappingInSpsFlag; }
  uint32_t  getSubPicIdLen                              ()                          { return m_subPicIdLen; }
  uint32_t  getSubPicId                                 (int i)                     { return m_subPicId[i]; }
  void      setLFNST                        ( bool b )       { m_LFNST = b; }
  bool      getLFNST()                                 const { return m_LFNST; }
  void      setUseFastLFNST                 ( bool b )       { m_useFastLFNST = b; }
  bool      getUseFastLFNST()                          const { return m_useFastLFNST; }

  void      setUseLMChroma                  ( int n )        { m_LMChroma = n; }
  int       getUseLMChroma()                           const { return m_LMChroma; }
  void      setHorCollocatedChromaFlag( bool b )             { m_horCollocatedChromaFlag = b; }
  bool      getHorCollocatedChromaFlag()               const { return m_horCollocatedChromaFlag; }
  void      setVerCollocatedChromaFlag( bool b )             { m_verCollocatedChromaFlag = b; }
  bool      getVerCollocatedChromaFlag()               const { return m_verCollocatedChromaFlag; }

  void setSbTmvpEnabledFlag(bool val) { m_sbTmvpEnableFlag = val; }

  void      setAffine                       ( bool b )       { m_Affine = b; }
  bool      getAffine                       ()         const { return m_Affine; }
  void      setAffineType( bool b )                          { m_AffineType = b; }
  bool      getAffineType()                            const { return m_AffineType; }
  void      setAdaptBypassAffineMe(bool b)                   { m_adaptBypassAffineMe = b;}
  bool      getAdaptBypassAffineMe()                   const { return m_adaptBypassAffineMe; }
  void      setPROF                         (bool b)         { m_PROF = b; }
  bool      getPROF                         ()         const { return m_PROF; }
  void      setBIO(bool b)                                   { m_BIO = b; }
  bool      getBIO()                                   const { return m_BIO; }

  void      setMTSIntraMaxCand              ( unsigned u )   { m_MTSIntraMaxCand = u; }
  unsigned  getMTSIntraMaxCand              ()         const { return m_MTSIntraMaxCand; }
  void      setMTSInterMaxCand              ( unsigned u )   { m_MTSInterMaxCand = u; }
  unsigned  getMTSInterMaxCand              ()         const { return m_MTSInterMaxCand; }
  void      setExplicitMtsIntraEnabled(bool b) { m_explicitMtsIntra = b; }
  void      setExplicitMtsInterEnabled(bool b) { m_explicitMtsInter = b; }
  void      setImplicitMtsIntraEnabled(bool b) { m_implicitMtsIntra = b; }
  void      setUseSBT                       ( bool b )       { m_SBT = b; }
  bool      getUseSBT                       ()         const { return m_SBT; }

  void      setSBTFast64WidthTh             ( int  b )       { m_SBTFast64WidthTh = b; }
  int       getSBTFast64WidthTh             ()         const { return m_SBTFast64WidthTh; }

  void      setUseCompositeRef              (bool b)         { m_compositeRefEnabled = b; }
  bool      getUseCompositeRef              ()         const { return m_compositeRefEnabled; }
  void      setUseSMVD                      ( bool b )       { m_SMVD = b; }
  bool      getUseSMVD                      ()         const { return m_SMVD; }
  void      setUseBcw                       ( bool b )       { m_bcw = b; }
  bool      getUseBcw                       ()         const { return m_bcw; }
  void      setUseBcwFast                   ( uint32_t b )   { m_BcwFast = b; }
  bool      getUseBcwFast                   ()         const { return m_BcwFast; }

  void setUseLadf(bool b) { m_ladfEnabled = b; }
  bool getUseLadf() const { return m_ladfEnabled; }
  void setLadfNumIntervals(int i) { m_ladfNumIntervals = i; }
  int  getLadfNumIntervals() const { return m_ladfNumIntervals; }
  void setLadfQpOffset(int value, int idx) { m_ladfQpOffset[idx] = value; }
  int  getLadfQpOffset(int idx) const { return m_ladfQpOffset[idx]; }
  void setLadfIntervalLowerBound(int value, int idx) { m_ladfIntervalLowerBound[idx] = value; }
  int  getLadfIntervalLowerBound(int idx) const { return m_ladfIntervalLowerBound[idx]; }

  void      setUseCiip                   ( bool b )       { m_ciip = b; }
  bool      getUseCiip                   ()         const { return m_ciip; }
  void      setUseGeo                       ( bool b )       { m_Geo = b; }
  bool      getUseGeo                       ()         const { return m_Geo; }
  void      setAllowDisFracMMVD             ( bool b )       { m_allowDisFracMMVD = b;    }
  bool      getAllowDisFracMMVD             ()         const { return m_allowDisFracMMVD; }
  void      setUseHashMECfgEnable           (bool b) { m_HashMECfgEnable = b; }
  bool      getUseHashMECfgEnable           ()         const { return m_HashMECfgEnable; }
  void      setUseAffineAmvr                ( bool b )       { m_AffineAmvr = b;    }
  bool      getUseAffineAmvr                ()         const { return m_AffineAmvr; }
  void      setUseAffineAmvrEncOpt          ( bool b )       { m_AffineAmvrEncOpt = b;    }
  bool      getUseAffineAmvrEncOpt          ()         const { return m_AffineAmvrEncOpt; }
  void      setUseAffineAmvp                ( bool b )       { m_AffineAmvp = b;    }
  bool      getUseAffineAmvp                ()         const { return m_AffineAmvp; }
  void      setDMVR                      ( bool b )       { m_DMVR = b; }
  bool      getDMVR                      ()         const { return m_DMVR; }
  void      setMMVD                         (bool b)         { m_MMVD = b;    }
  bool      getMMVD                         ()         const { return m_MMVD; }
  void      setMmvdDisNum                   ( int b )        { m_MmvdDisNum = b; }
  int       getMmvdDisNum                   ()         const { return m_MmvdDisNum; }
  void      setRGBFormatFlag(bool value) { m_rgbFormat = value; }
  bool      getRGBFormatFlag()                         const { return m_rgbFormat; }
  void      setUseColorTrans(bool value) { m_useColorTrans = value; }
  bool      getUseColorTrans()                         const { return m_useColorTrans; }
  void      setPLTMode                   ( unsigned n)    { m_PLTMode = n; }
  unsigned  getPLTMode                   ()         const { return m_PLTMode; }
  void      setJointCbCr(bool b) { m_jointCbCrMode = b; }
  bool      getJointCbCr() const { return m_jointCbCrMode; }
  void      setIBCMode                      (unsigned n)     { m_IBCMode = n; }
  unsigned  getIBCMode                      ()         const { return m_IBCMode; }
  void      setIBCLocalSearchRangeX         (unsigned n)     { m_IBCLocalSearchRangeX = n; }
  unsigned  getIBCLocalSearchRangeX         ()         const { return m_IBCLocalSearchRangeX; }
  void      setIBCLocalSearchRangeY         (unsigned n)     { m_IBCLocalSearchRangeY = n; }
  unsigned  getIBCLocalSearchRangeY         ()         const { return m_IBCLocalSearchRangeY; }
  void      setIBCHashSearch                (unsigned n)     { m_IBCHashSearch = n; }
  unsigned  getIBCHashSearch                ()         const { return m_IBCHashSearch; }
  void      setIBCHashSearchMaxCand         (unsigned n)     { m_IBCHashSearchMaxCand = n; }
  unsigned  getIBCHashSearchMaxCand         ()         const { return m_IBCHashSearchMaxCand; }
  void      setIBCHashSearchRange4SmallBlk  (unsigned n)     { m_IBCHashSearchRange4SmallBlk = n; }
  unsigned  getIBCHashSearchRange4SmallBlk  ()         const { return m_IBCHashSearchRange4SmallBlk; }
  void      setIBCFastMethod                (unsigned n)     { m_IBCFastMethod = n; }
  unsigned  getIBCFastMethod                ()         const { return m_IBCFastMethod; }
  void      setDMVREncMvSelection(bool b) { m_dmvrEncSelect = b; }
  bool      getDMVREncMvSelection()         const { return m_dmvrEncSelect; }
  void      setDMVREncMvSelectDisableHighestTemporalLayer(bool b) { m_dmvrEncSelectDisableHighestTemporalLayer = b; }
  int       getDMVREncMvSelectDisableHighestTemporalLayer()         const { return m_dmvrEncSelectDisableHighestTemporalLayer; }

  void      setUseWrapAround                ( bool b )       { m_wrapAround = b; }
  bool      getUseWrapAround                ()         const { return m_wrapAround; }
  void      setWrapAroundOffset             ( unsigned u )   { m_wrapAroundOffset = u; }
  unsigned  getWrapAroundOffset             ()         const { return m_wrapAroundOffset; }

  // ADD_NEW_TOOL : (encoder lib) add access functions here
  void      setVirtualBoundariesEnabledFlag( bool b ) { m_virtualBoundariesEnabledFlag = b; }
  bool      getVirtualBoundariesEnabledFlag() const { return m_virtualBoundariesEnabledFlag; }
  void      setVirtualBoundariesPresentFlag( bool b ) { m_virtualBoundariesPresentFlag = b; }
  bool      getVirtualBoundariesPresentFlag() const { return m_virtualBoundariesPresentFlag; }
  void      setNumVerVirtualBoundaries      ( unsigned u )   { m_numVerVirtualBoundaries = u; }
  unsigned  getNumVerVirtualBoundaries      ()         const { return m_numVerVirtualBoundaries; }
  void      setNumHorVirtualBoundaries      ( unsigned u )   { m_numHorVirtualBoundaries = u; }
  unsigned  getNumHorVirtualBoundaries      ()         const { return m_numHorVirtualBoundaries; }
  void      setVirtualBoundariesPosX        ( unsigned u, unsigned idx ) { m_virtualBoundariesPosX[idx] = u; }
  unsigned  getVirtualBoundariesPosX        ( unsigned idx ) const { return m_virtualBoundariesPosX[idx]; }
  void      setVirtualBoundariesPosY        ( unsigned u, unsigned idx ) { m_virtualBoundariesPosY[idx] = u; }
  unsigned  getVirtualBoundariesPosY        ( unsigned idx ) const { return m_virtualBoundariesPosY[idx]; }
  void      setUseISP                       ( bool b )       { m_ISP = b; }
  bool      getUseISP                       ()         const { return m_ISP; }
  void      setLmcs                         ( bool b )                   { m_lmcsEnabled = b; }
  bool      getLmcs                         () const                     { return m_lmcsEnabled; }
  void      setReshapeSignalType            ( uint32_t signalType )      { m_reshapeSignalType = signalType; }
  uint32_t  getReshapeSignalType            () const                     { return m_reshapeSignalType; }
  void      setReshapeIntraCMD              (uint32_t intraCMD)          { m_intraCMD = intraCMD; }
  uint32_t  getReshapeIntraCMD              ()                           { return m_intraCMD; }
  void      setReshapeCW                    (const ReshapeCW &reshapeCW) { m_reshapeCW = reshapeCW; }
  const ReshapeCW& getReshapeCW             ()                           { return m_reshapeCW; }
  void      setReshapeCSoffset              (int CSoffset)          { m_CSoffset = CSoffset; }
  int       getReshapeCSoffset              ()                      { return m_CSoffset; }
  void      setMaxCUWidth                   ( uint32_t  u )      { m_maxCUWidth  = u; }
  uint32_t      getMaxCUWidth                   () const         { return m_maxCUWidth; }
  void      setMaxCUHeight                  ( uint32_t  u )      { m_maxCUHeight = u; }
  uint32_t      getMaxCUHeight                  () const         { return m_maxCUHeight; }
  void      setLog2MinCodingBlockSize       ( int n )        { m_log2MinCUSize = n;   }
  int       getLog2MinCodingBlockSize       () const         { return m_log2MinCUSize;}
  void      setUseEncDbOpt                  ( bool  n )          { m_encDbOpt = n; }
  bool      getUseEncDbOpt                  () const             { return m_encDbOpt; }
  void      setUseAlfLambdaOpt              (bool n)         { m_encALFOpt = n; }
  bool      getUseAlfLambdaOpt              () const         { return m_encALFOpt; }
  
  void      setUseFastLCTU                  ( bool  n )      { m_useFastLCTU = n; }
  bool      getUseFastLCTU                  () const         { return m_useFastLCTU; }
  void      setUseFastMerge                 ( bool  n )      { m_useFastMrg = n; }
  bool      getUseFastMerge                 () const         { return m_useFastMrg; }
  void      setMaxMergeRdCandNumTotal       ( int n )        { m_maxMergeRdCandNumTotal = n;}
  int       getMaxMergeRdCandNumTotal       () const         { return m_maxMergeRdCandNumTotal;}
  void      setMergeRdCandQuotaRegular      ( int n )        { m_mergeRdCandQuotaRegular = n;}
  int       getMergeRdCandQuotaRegular      () const         { return m_mergeRdCandQuotaRegular;}
  void      setMergeRdCandQuotaRegularSmallBlk( int n )      { m_mergeRdCandQuotaRegularSmallBlk = n;}
  int       getMergeRdCandQuotaRegularSmallBlk() const       { return m_mergeRdCandQuotaRegularSmallBlk;}
  void      setMergeRdCandQuotaSubBlk       ( int n )        { m_mergeRdCandQuotaSubBlk = n;}
  int       getMergeRdCandQuotaSubBlk       () const         { return m_mergeRdCandQuotaSubBlk;}
  void      setMergeRdCandQuotaCiip         ( int n )        { m_mergeRdCandQuotaCiip = n;}
  int       getMergeRdCandQuotaCiip         () const         { return m_mergeRdCandQuotaCiip;}
  void      setMergeRdCandQuotaGpm          ( int n )        { m_mergeRdCandQuotaGpm = n;}
  int       getMergeRdCandQuotaGpm          () const         { return m_mergeRdCandQuotaGpm;}
  void      setUsePbIntraFast               ( bool  n )      { m_usePbIntraFast = n; }
  bool      getUsePbIntraFast               () const         { return m_usePbIntraFast; }
  void      setUseAMaxBT                    ( bool  n )      { m_useAMaxBT = n; }
  bool      getUseAMaxBT                    () const         { return m_useAMaxBT; }

  void      setUseE0023FastEnc              ( bool b )       { m_e0023FastEnc = b; }
  bool      getUseE0023FastEnc              () const         { return m_e0023FastEnc; }
  void      setUseContentBasedFastQtbt      ( bool b )       { m_contentBasedFastQtbt = b; }
  bool      getUseContentBasedFastQtbt      () const         { return m_contentBasedFastQtbt; }
  void      setUseNonLinearAlfLuma          ( bool b )       { m_useNonLinearAlfLuma = b; }
  bool      getUseNonLinearAlfLuma          () const         { return m_useNonLinearAlfLuma; }
  void      setUseNonLinearAlfChroma        ( bool b )       { m_useNonLinearAlfChroma = b; }
  bool      getUseNonLinearAlfChroma        () const         { return m_useNonLinearAlfChroma; }
  void      setMaxNumAlfAlternativesChroma  ( uint32_t u )   { m_maxNumAlfAlternativesChroma = u; }
  uint32_t  getMaxNumAlfAlternativesChroma  () const         { return m_maxNumAlfAlternativesChroma; }
  void      setUseMRL                       ( bool b )       { m_MRL = b; }
  bool      getUseMRL                       () const         { return m_MRL; }
  void      setUseMIP                       ( bool b )       { m_MIP = b; }
  bool      getUseMIP                       () const         { return m_MIP; }
  void      setUseFastMIP                   ( bool b )       { m_useFastMIP = b; }
  bool      getUseFastMIP                   () const         { return m_useFastMIP; }
  void      setFastLocalDualTreeMode        ( int i )        { m_fastLocalDualTreeMode = i; }
  int       getFastLocalDualTreeMode        () const         { return m_fastLocalDualTreeMode; }
  void      setFastAdaptCostPredMode        (int i)          { m_fastAdaptCostPredMode = i; }
  int       getFastAdaptCostPredMode        () const         { return m_fastAdaptCostPredMode; }
  void      setDisableFastDecisionTT        (bool i)         { m_disableFastDecisionTT = i; }
  bool      getDisableFastDecisionTT        () const         { return m_disableFastDecisionTT; }

  void      setUseMttSkip                   (bool i)         { m_useMttSkip = i; }
  bool      getUseMttSkip                   () const         { return m_useMttSkip; }
 
  void      setLog2MaxTbSize                ( uint32_t  u )   { m_log2MaxTbSize = u; }

  //====== Loop/Deblock Filter ========
  void      setDeblockingFilterDisable      ( bool  b )      { m_deblockingFilterDisable           = b; }
  void      setDeblockingFilterOffsetInPPS  ( bool  b )      { m_deblockingFilterOffsetInPPS       = b; }
  void      setDeblockingFilterBetaOffset   ( int   i )      { m_deblockingFilterBetaOffsetDiv2    = i; }
  void      setDeblockingFilterTcOffset     ( int   i )      { m_deblockingFilterTcOffsetDiv2      = i; }
  void      setDeblockingFilterCbBetaOffset ( int   i )      { m_deblockingFilterCbBetaOffsetDiv2  = i; }
  void      setDeblockingFilterCbTcOffset   ( int   i )      { m_deblockingFilterCbTcOffsetDiv2    = i; }
  void      setDeblockingFilterCrBetaOffset ( int   i )      { m_deblockingFilterCrBetaOffsetDiv2  = i; }
  void      setDeblockingFilterCrTcOffset   ( int   i )      { m_deblockingFilterCrTcOffsetDiv2    = i; }
  void      setDeblockingFilterMetric       ( int   i )      { m_deblockingFilterMetric = i; }
  //====== Motion search ========
  void      setDisableIntraPUsInInterSlices ( bool  b )      { m_bDisableIntraPUsInInterSlices = b; }
  void      setMotionEstimationSearchMethod ( MESearchMethod e ) { m_motionEstimationSearchMethod = e; }
  void      setSearchRange(int i)
  {
    m_searchRange = i;
  }
  void      setBipredSearchRange            ( int   i )      { m_bipredSearchRange = i; }
  void      setClipForBiPredMeEnabled       ( bool  b )      { m_bClipForBiPredMeEnabled = b; }
  void      setFastMEAssumingSmootherMVEnabled ( bool b )    { m_bFastMEAssumingSmootherMVEnabled = b; }
  void      setMinSearchWindow              ( int   i )      { m_minSearchWindow = i; }
  void      setRestrictMESampling           ( bool  b )      { m_bRestrictMESampling = b; }

  //====== Quality control ========
  void      setMaxDeltaQP                   ( int   i )      { m_iMaxDeltaQP = i; }
  void      setCuQpDeltaSubdiv              ( int   i )      { m_cuQpDeltaSubdiv = i; }
  unsigned  getCuChromaQpOffsetSubdiv       ()         const { return m_cuChromaQpOffsetSubdiv;  }
  void      setCuChromaQpOffsetSubdiv       ( unsigned value ) { m_cuChromaQpOffsetSubdiv = value; }
  bool      getCuChromaQpOffsetEnabled      ()         const { return m_cuChromaQpOffsetEnabled;  }
  void      setCuChromaQpOffsetEnabled      ( bool value )   { m_cuChromaQpOffsetEnabled = value; }
  void      setCuChromaQpOffsetList         (const std::vector<ChromaQpAdj> &list) { m_cuChromaQpOffsetList = list; }

  void      setChromaCbQpOffset             ( int   i )      { m_chromaCbQpOffset = i; }
  void      setChromaCrQpOffset             ( int   i )      { m_chromaCrQpOffset = i; }
  void      setChromaCbQpOffsetDualTree     ( int   i )      { m_chromaCbQpOffsetDualTree = i; }
  void      setChromaCrQpOffsetDualTree     ( int   i )      { m_chromaCrQpOffsetDualTree = i; }
  int       getChromaCbQpOffsetDualTree     ()         const { return m_chromaCbQpOffsetDualTree; }
  int       getChromaCrQpOffsetDualTree     ()         const { return m_chromaCrQpOffsetDualTree; }
  void      setChromaCbCrQpOffset           ( int   i )      { m_chromaCbCrQpOffset = i; }
  void      setChromaCbCrQpOffsetDualTree   ( int   i )      { m_chromaCbCrQpOffsetDualTree = i; }
  int       getChromaCbCrQpOffsetDualTree   ()         const { return m_chromaCbCrQpOffsetDualTree; }
#if ER_CHROMA_QP_WCG_PPS
  void      setWCGChromaQpControl           ( const WCGChromaQPControl &ctrl )     { m_wcgChromaQpControl = ctrl; }
  const WCGChromaQPControl &getWCGChromaQPControl () const { return m_wcgChromaQpControl; }
#endif
#if W0038_CQP_ADJ
  void      setSliceChromaOffsetQpIntraOrPeriodic( uint32_t periodicity, int sliceChromaQpOffsetIntraOrPeriodic[2]) { m_sliceChromaQpOffsetPeriodicity = periodicity; memcpy(m_sliceChromaQpOffsetIntraOrPeriodic, sliceChromaQpOffsetIntraOrPeriodic, sizeof(m_sliceChromaQpOffsetIntraOrPeriodic)); }
  int       getSliceChromaOffsetQpIntraOrPeriodic( bool bIsCr) const                                            { return m_sliceChromaQpOffsetIntraOrPeriodic[bIsCr?1:0]; }
  uint32_t      getSliceChromaOffsetQpPeriodicity() const                                                           { return m_sliceChromaQpOffsetPeriodicity; }
#endif

  void         setChromaFormatIdc(ChromaFormat cf) { m_chromaFormatIdc = cf; }
  ChromaFormat getChromaFormatIdc() const { return m_chromaFormatIdc; }

#if SHARP_LUMA_DELTA_QP
  void      setLumaLevelToDeltaQPControls( const LumaLevelToDeltaQPMapping &lumaLevelToDeltaQPMapping ) { m_lumaLevelToDeltaQPMapping=lumaLevelToDeltaQPMapping; }
  const LumaLevelToDeltaQPMapping& getLumaLevelToDeltaQPMapping() const { return m_lumaLevelToDeltaQPMapping; }
#endif
  bool      getSmoothQPReductionEnable()                  const { return m_smoothQPReductionEnable; }
  void      setSmoothQPReductionEnable(bool value)        { m_smoothQPReductionEnable = value; }
  int       getSmoothQPReductionPeriodicity()                 const { return m_smoothQPReductionPeriodicity; }
  void      setSmoothQPReductionPeriodicity(int value)        { m_smoothQPReductionPeriodicity = value; }
  double    getSmoothQPReductionThresholdIntra()              const { return m_smoothQPReductionThresholdIntra; }
  void      setSmoothQPReductionThresholdIntra(double value)  { m_smoothQPReductionThresholdIntra = value; }
  double    getSmoothQPReductionModelScaleIntra()              const { return m_smoothQPReductionModelScaleIntra; }
  void      setSmoothQPReductionModelScaleIntra(double value) { m_smoothQPReductionModelScaleIntra = value; }
  double    getSmoothQPReductionModelOffsetIntra()             const { return m_smoothQPReductionModelOffsetIntra; }
  void      setSmoothQPReductionModelOffsetIntra(double value) { m_smoothQPReductionModelOffsetIntra = value; }
  int       getSmoothQPReductionLimitIntra()                   const { return m_smoothQPReductionLimitIntra; }
  void      setSmoothQPReductionLimitIntra(int value)          { m_smoothQPReductionLimitIntra = value; }
  double    getSmoothQPReductionThresholdInter()               const { return m_smoothQPReductionThresholdInter; }
  void      setSmoothQPReductionThresholdInter(double value)   { m_smoothQPReductionThresholdInter = value; }
  double    getSmoothQPReductionModelScaleInter()              const { return m_smoothQPReductionModelScaleInter; }
  void      setSmoothQPReductionModelScaleInter(double value) { m_smoothQPReductionModelScaleInter = value; }
  double    getSmoothQPReductionModelOffsetInter()             const { return m_smoothQPReductionModelOffsetInter; }
  void      setSmoothQPReductionModelOffsetInter(double value) { m_smoothQPReductionModelOffsetInter = value; }
  int       getSmoothQPReductionLimitInter()                   const { return m_smoothQPReductionLimitInter; }
  void      setSmoothQPReductionLimitInter(int value)          { m_smoothQPReductionLimitInter = value; }
  bool      getExtendedPrecisionProcessingFlag         ()         const { return m_extendedPrecisionProcessingFlag;  }
  void      setExtendedPrecisionProcessingFlag         (bool value)     { m_extendedPrecisionProcessingFlag = value; }
  bool      getTSRCRicePresentFlag         ()         const { return m_tsrcRicePresentFlag;  }
  void      setTSRCRicePresentFlag         (bool value)     { m_tsrcRicePresentFlag = value; }
  bool      getReverseLastSigCoeffEnabledFlag         ()         const { return m_reverseLastSigCoeffEnabledFlag;  }
  void      setReverseLastSigCoeffEnabledFlag         (bool value)     { m_reverseLastSigCoeffEnabledFlag = value; }
  bool      getHighPrecisionOffsetsEnabledFlag() const { return m_highPrecisionOffsetsEnabledFlag; }
  void      setHighPrecisionOffsetsEnabledFlag(bool value) { m_highPrecisionOffsetsEnabledFlag = value; }

  void      setUseAdaptiveQP                ( bool  b )      { m_bUseAdaptiveQP = b; }
  void      setQPAdaptationRange            ( int   i )      { m_iQPAdaptationRange = i; }
#if ENABLE_QPA
  void      setUsePerceptQPA                ( const bool b ) { m_bUsePerceptQPA = b; }
  void      setUseWPSNR                     ( const bool b ) { m_bUseWPSNR = b; }
#endif

  //====== Sequence ========
  const Fraction& getFrameRate() const { return m_frameRate; }
  uint32_t      getFrameSkip() const { return m_frameSkip; }
  uint32_t      getTemporalSubsampleRatio       () const     { return  m_temporalSubsampleRatio; }
  int       getSourceWidth                  () const     { return  m_sourceWidth; }
  int       getSourceHeight                 () const     { return  m_sourceHeight; }
  int       getFramesToBeEncoded            () const     { return  m_framesToBeEncoded; }

  //====== Lambda Modifiers ========
  void      setLambdaModifier               ( uint32_t uiIndex, double dValue ) { m_adLambdaModifier[ uiIndex ] = dValue; }
  double    getLambdaModifier               ( uint32_t uiIndex )          const { return m_adLambdaModifier[ uiIndex ]; }
  void      setIntraLambdaModifier          ( const std::vector<double> &dValue )               { m_adIntraLambdaModifier = dValue;       }
  const std::vector<double>& getIntraLambdaModifier()                        const { return m_adIntraLambdaModifier;         }
  void      setIntraQpFactor                ( double dValue )               { m_dIntraQpFactor = dValue;              }
  double    getIntraQpFactor                ()                        const { return m_dIntraQpFactor;                }
  void      setLambdaScaleTowardsNextQP     (double  scale) { m_lambdaScaleTowardsNextQP = scale; }
  double    getLambdaScaleTowardsNextQP     () const        { return m_lambdaScaleTowardsNextQP; }

  //==== Coding Structure ========
  int       getIntraPeriod                  () const     { return  m_intraPeriod; }
  uint32_t  getDecodingRefreshType          () const     { return  m_decodingRefreshType; }
  bool      getReWriteParamSets             ()  const    { return m_rewriteParamSets; }
  int       getGOPSize() const { return m_gopSize; }
  int       getMaxDecPicBuffering           (uint32_t tlayer) { return m_maxDecPicBuffering[tlayer]; }
  int       getMaxNumReorderPics            (uint32_t tlayer) { return m_maxNumReorderPics[tlayer]; }
  int       getDrapPeriod                   ()     { return m_drapPeriod; }
  int       getEdrapPeriod                  ()     { return m_edrapPeriod; }
  int       getIntraQPOffset                () const    { return  m_intraQPOffset; }
  int       getLambdaFromQPEnable           () const    { return  m_lambdaFromQPEnable; }
public:
  int       getBaseQP                       () const { return  m_iQP; } // public should use getQPForPicture.
  int       getQpRefAdj() const { return m_qpRefAdj; }
  int       getQPForPicture                 (const uint32_t gopIndex, const Slice *pSlice) const; // Function actually defined in EncLib.cpp
  int       getSourcePadding                ( int i ) { CHECK(i >= 2, "Invalid index"); return  m_sourcePadding[i]; }

  bool      getAccessUnitDelimiter() const  { return m_AccessUnitDelimiter; }
  void      setAccessUnitDelimiter(bool val){ m_AccessUnitDelimiter = val; }
  bool      getEnablePictureHeaderInSliceHeader() const { return m_enablePictureHeaderInSliceHeader; }
  void      setEnablePictureHeaderInSliceHeader(bool val) { m_enablePictureHeaderInSliceHeader = val; }

  //==== Loop/Deblock Filter ========
  bool      getDeblockingFilterDisable            ()      { return m_deblockingFilterDisable;          }
  bool      getDeblockingFilterOffsetInPPS        ()      { return m_deblockingFilterOffsetInPPS;      }
  int       getDeblockingFilterBetaOffset         ()      { return m_deblockingFilterBetaOffsetDiv2;   }
  int       getDeblockingFilterTcOffset           ()      { return m_deblockingFilterTcOffsetDiv2;     }
  int       getDeblockingFilterCbBetaOffset       ()      { return m_deblockingFilterCbBetaOffsetDiv2; }
  int       getDeblockingFilterCbTcOffset         ()      { return m_deblockingFilterCbTcOffsetDiv2;   }
  int       getDeblockingFilterCrBetaOffset       ()      { return m_deblockingFilterCrBetaOffsetDiv2; }
  int       getDeblockingFilterCrTcOffset         ()      { return m_deblockingFilterCrTcOffsetDiv2;   }
  int       getDeblockingFilterMetric       ()      { return m_deblockingFilterMetric; }

  //==== Motion search ========
  bool      getDisableIntraPUsInInterSlices    () const { return m_bDisableIntraPUsInInterSlices; }
  MESearchMethod getMotionEstimationSearchMethod ( ) const { return m_motionEstimationSearchMethod; }
  int       getSearchRange                     () const { return m_searchRange; }
  bool      getClipForBiPredMeEnabled          () const { return m_bClipForBiPredMeEnabled; }
  bool      getFastMEAssumingSmootherMVEnabled () const { return m_bFastMEAssumingSmootherMVEnabled; }
  int       getMinSearchWindow                 () const { return m_minSearchWindow; }
  bool      getRestrictMESampling              () const { return m_bRestrictMESampling; }

  //==== Quality control ========
  int       getMaxDeltaQP                   () const { return m_iMaxDeltaQP; }
  int       getCuQpDeltaSubdiv              () const { return m_cuQpDeltaSubdiv; }
  bool      getUseAdaptiveQP                () const { return m_bUseAdaptiveQP; }
  int       getQPAdaptationRange            () const { return m_iQPAdaptationRange; }
#if ENABLE_QPA
  bool      getUsePerceptQPA                () const { return m_bUsePerceptQPA; }
  bool      getUseWPSNR                     () const { return m_bUseWPSNR; }
#endif

  //==== Tool list ========
  void setBitDepth(const ChannelType chType, int internalBitDepthForChannel)
  {
    m_bitDepth[chType] = internalBitDepthForChannel;
  }
  void setInputBitDepth(const ChannelType chType, int internalBitDepthForChannel)
  {
    m_inputBitDepth[chType] = internalBitDepthForChannel;
  }
  BitDepths &getInputBitDepth() { return m_inputBitDepth; }
  void      setUseASR                       ( bool  b )     { m_bUseASR     = b; }
  void      setUseHADME                     ( bool  b )     { m_bUseHADME   = b; }
  void      setUseRDOQ                      ( bool  b )     { m_useRDOQ    = b; }
  void      setUseRDOQTS                    ( bool  b )     { m_useRDOQTS  = b; }
  void      setUseSelectiveRDOQ             ( bool b )      { m_useSelectiveRDOQ = b; }
  void      setFastInterSearchMode          ( FastInterSearchMode m ) { m_fastInterSearchMode = m; }
  void      setUseEarlyCU                   ( bool  b )     { m_bUseEarlyCU = b; }
  void      setUseFastDecisionForMerge      ( bool  b )     { m_useFastDecisionForMerge = b; }
  void      setUseEarlySkipDetection        ( bool  b )     { m_useEarlySkipDetection = b; }
  void      setFastUDIUseMPMEnabled         ( bool  b )     { m_bFastUDIUseMPMEnabled = b; }
  void      setFastMEForGenBLowDelayEnabled ( bool  b )     { m_bFastMEForGenBLowDelayEnabled = b; }

  void                 setdQPs(FrameDeltaQps &v) { m_frameDeltaQps = v; }
  const FrameDeltaQps &getdQPs() const { return m_frameDeltaQps; }

  void      setDeltaQpRD                    ( uint32_t  u )     {m_uiDeltaQpRD  = u; }
  void      setFastDeltaQp                  ( bool  b )     {m_bFastDeltaQP = b; }
  int                 getBitDepth(const ChannelType chType) const { return m_bitDepth[chType]; }
  BitDepths          &getBitDepth() { return m_bitDepth; }
  bool      getUseASR                       ()      { return m_bUseASR;     }
  bool      getUseHADME                     ()      { return m_bUseHADME;   }
  bool      getUseRDOQ                      ()      { return m_useRDOQ;    }
  bool      getUseRDOQTS                    ()      { return m_useRDOQTS;  }
  bool      getUseSelectiveRDOQ             ()      { return m_useSelectiveRDOQ; }
  FastInterSearchMode getFastInterSearchMode() const{ return m_fastInterSearchMode;  }
  bool      getUseEarlyCU                   () const{ return m_bUseEarlyCU; }
  bool      getUseFastDecisionForMerge      () const{ return m_useFastDecisionForMerge; }
  bool      getUseEarlySkipDetection        () const{ return m_useEarlySkipDetection; }
  bool      getFastUDIUseMPMEnabled         ()      { return m_bFastUDIUseMPMEnabled; }
  bool      getFastMEForGenBLowDelayEnabled ()      { return m_bFastMEForGenBLowDelayEnabled; }

  void setGopBasedTemporalFilterEnabled(const bool b) { m_gopBasedTemporalFilterEnabled = b; }
  bool getGopBasedTemporalFilterEnabled() const { return m_gopBasedTemporalFilterEnabled; }
  void      setBIM                          (bool flag)               { m_bimEnabled = flag; }
  bool      getBIM                          ()                        { return m_bimEnabled; }
  void      setAdaptQPmap                   (std::map<int, int*> map) { m_adaptQPmap = map; }
  int*      getAdaptQPmap                   (int poc)                 { return m_adaptQPmap[poc]; }
  std::map<int, int*> *getAdaptQPmap        ()                        { return &m_adaptQPmap; }

  void      setDPF                          (bool flag)               { m_dpfEnabled = flag; }
  void      setDPFKeyLen                    (int i)                   { m_dpfKeyLen = i; }
  void      setDPFNonkeyLen                 (int i)                   { m_dpfNonkeyLen = i; }
  bool      getDPF()                                            const { return m_dpfEnabled; }
  int       getDPFKeyLen()                                      const { return m_dpfKeyLen; }
  int       getDPFNonkeyLen()                                   const { return m_dpfNonkeyLen; }

  bool      getUseReconBasedCrossCPredictionEstimate ()                const { return m_reconBasedCrossCPredictionEstimate;  }
  void      setUseReconBasedCrossCPredictionEstimate (const bool value)      { m_reconBasedCrossCPredictionEstimate = value; }

  bool getUseTransformSkip                             ()      { return m_useTransformSkip;        }
  void setUseTransformSkip                             ( bool b ) { m_useTransformSkip  = b;       }
  bool getTransformSkipRotationEnabledFlag             ()            const { return m_transformSkipRotationEnabledFlag;  }
  void setTransformSkipRotationEnabledFlag             (const bool value)  { m_transformSkipRotationEnabledFlag = value; }
  bool getTransformSkipContextEnabledFlag              ()            const { return m_transformSkipContextEnabledFlag;  }
  void setTransformSkipContextEnabledFlag              (const bool value)  { m_transformSkipContextEnabledFlag = value; }
  bool getUseChromaTS                                  ()       { return m_useChromaTS; }
  void setUseChromaTS                                  (bool b) { m_useChromaTS = b; }
  bool getUseBDPCM                                     ()         { return m_useBDPCM; }
  void setUseBDPCM                                     ( bool b ) { m_useBDPCM  = b;   }
  bool     getUseJointCbCr() { return m_jointCbCrMode; }
  void     setUseJointCbCr(bool b) { m_jointCbCrMode = b; }
  bool getRrcRiceExtensionEnableFlag()                 const { return m_rrcRiceExtensionEnableFlag; }
  void setRrcRiceExtensionEnableFlag(const bool value) { m_rrcRiceExtensionEnableFlag = value; }
  bool getPersistentRiceAdaptationEnabledFlag          ()                 const { return m_persistentRiceAdaptationEnabledFlag;  }
  void setPersistentRiceAdaptationEnabledFlag          (const bool value)       { m_persistentRiceAdaptationEnabledFlag = value; }
  bool getCabacBypassAlignmentEnabledFlag              ()       const      { return m_cabacBypassAlignmentEnabledFlag;  }
  void setCabacBypassAlignmentEnabledFlag              (const bool value)  { m_cabacBypassAlignmentEnabledFlag = value; }
  bool getUseTransformSkipFast                         ()      { return m_useTransformSkipFast;    }
  void setUseTransformSkipFast                         ( bool b ) { m_useTransformSkipFast  = b;   }
  uint32_t getLog2MaxTransformSkipBlockSize                () const      { return m_log2MaxTransformSkipBlockSize;     }
  void setLog2MaxTransformSkipBlockSize                ( uint32_t u )    { m_log2MaxTransformSkipBlockSize  = u;       }
  bool getUseFastISP                                   () const   { return m_useFastISP;    }
  void setUseFastISP                                   ( bool b ) { m_useFastISP  = b;   }

  uint32_t      getDeltaQpRD                    () const { return m_uiDeltaQpRD; }
  bool      getFastDeltaQp                  () const { return m_bFastDeltaQP; }
  void      setMixedLossyLossless(bool b) { m_mixedLossyLossless = b; }
  bool      getMixedLossyLossless()       { return m_mixedLossyLossless; }
  void      setSliceLosslessArray(std::vector<uint16_t> sliceLosslessArray) { m_sliceLosslessArray = sliceLosslessArray; }
  const     std::vector<uint16_t>*   getSliceLosslessArray() const { return &m_sliceLosslessArray; }
  //====== Tiles and Slices ========
  void      setNoPicPartitionFlag( bool b )                                { m_noPicPartitionFlag = b;              }
  bool      getNoPicPartitionFlag()                                        { return m_noPicPartitionFlag;           }
  void      setTileColWidths( std::vector<uint32_t> tileColWidths )        { m_tileColumnWidth = tileColWidths;     }
  const     std::vector<uint32_t>*   getTileColWidths() const              { return &m_tileColumnWidth;             }
  void      setTileRowHeights( std::vector<uint32_t> tileRowHeights )      { m_tileRowHeight = tileRowHeights;      }
  const     std::vector<uint32_t>*   getTileRowHeights() const             { return &m_tileRowHeight;               }
  void      setRectSliceFlag( bool b )                                     { m_rectSliceFlag = b;                   }
  bool      getRectSliceFlag()                                             { return m_rectSliceFlag;                }
  void      setNumSlicesInPic( uint32_t u )                                { m_numSlicesInPic = u;                  }
  uint32_t  getNumSlicesInPic()                                            { return m_numSlicesInPic;               }
  void      setTileIdxDeltaPresentFlag( bool b )                           { m_tileIdxDeltaPresentFlag = b;         }
  bool      getTileIdxDeltaPresentFlag()                                   { return m_tileIdxDeltaPresentFlag;      }
  void      setRectSlices( std::vector<RectSlice> rectSlices )             { m_rectSlices = rectSlices;             }
  const     std::vector<RectSlice>*   getRectSlices() const                { return &m_rectSlices;                  }
  void      setRasterSliceSizes( std::vector<uint32_t> rasterSliceSizes )  { m_rasterSliceSize = rasterSliceSizes;  }
  const     std::vector<uint32_t>*   getRasterSliceSizes() const           { return &m_rasterSliceSize;             }
  void      setLFCrossTileBoundaryFlag( bool b )                           { m_bLFCrossTileBoundaryFlag = b;        }
  bool      getLFCrossTileBoundaryFlag()                                   { return m_bLFCrossTileBoundaryFlag;     }
  void      setLFCrossSliceBoundaryFlag( bool b )                          { m_bLFCrossSliceBoundaryFlag = b;       }
  bool      getLFCrossSliceBoundaryFlag()                                  { return m_bLFCrossSliceBoundaryFlag;    }
  //====== Sub-picture and Slices ========
  void      setSingleSlicePerSubPicFlagFlag( bool b )                { m_singleSlicePerSubPicFlag = b;    }
  bool      getSingleSlicePerSubPicFlagFlag( )                       { return m_singleSlicePerSubPicFlag;    }
  void      setUseSAO(bool val) { m_useSao = val; }
  bool      getUseSAO() { return m_useSao; }
  void      setSaoTrueOrg              (bool b)                      { m_saoTrueOrg = b; }
  bool      getSaoTrueOrg              () const                      { return m_saoTrueOrg; }
  void  setTestSAODisableAtPictureLevel (bool bVal)                  { m_bTestSAODisableAtPictureLevel = bVal; }
  bool  getTestSAODisableAtPictureLevel ( ) const                    { return m_bTestSAODisableAtPictureLevel; }

  void   setSaoEncodingRate(double v)                                { m_saoEncodingRate = v; }
  double getSaoEncodingRate() const                                  { return m_saoEncodingRate; }
  void   setSaoEncodingRateChroma(double v)                          { m_saoEncodingRateChroma = v; }
  double getSaoEncodingRateChroma() const                            { return m_saoEncodingRateChroma; }
  void  setMaxNumOffsetsPerPic                   (int iVal)          { m_maxNumOffsetsPerPic = iVal; }
  int   getMaxNumOffsetsPerPic                   ()                  { return m_maxNumOffsetsPerPic; }
  void  setSaoCtuBoundary              (bool val)                    { m_saoCtuBoundary = val; }
  bool  getSaoCtuBoundary              ()                            { return m_saoCtuBoundary; }

  void  setSaoGreedyMergeEnc           (bool val)                    { m_saoGreedyMergeEnc = val; }
  bool  getSaoGreedyMergeEnc           ()                            { return m_saoGreedyMergeEnc; }
  void  setEntropyCodingSyncEnabledFlag(bool b)                      { m_entropyCodingSyncEnabledFlag = b; }
  bool  getEntropyCodingSyncEnabledFlag() const                      { return m_entropyCodingSyncEnabledFlag; }
  void  setEntryPointPresentFlag(bool b)                             { m_entryPointPresentFlag = b; }
  void  setDecodedPictureHashSEIType(HashType m)                     { m_decodedPictureHashSEIType = m; }
  HashType getDecodedPictureHashSEIType() const                      { return m_decodedPictureHashSEIType; }
  void  setSubpicDecodedPictureHashType(HashType m)                  { m_subpicDecodedPictureHashType = m; }
  HashType getSubpicDecodedPictureHashType() const                   { return m_subpicDecodedPictureHashType; }

  void     setSiiSEIEnabled(bool b) { m_siiSEIEnabled = b; }
  bool     getSiiSEIEnabled() { return m_siiSEIEnabled; }
  void     setSiiSEINumUnitsInShutterInterval(uint32_t value) { m_siiSEINumUnitsInShutterInterval = value; }
  uint32_t getSiiSEINumUnitsInShutterInterval() { return m_siiSEINumUnitsInShutterInterval; }
  void     setSiiSEITimeScale(uint32_t value) { m_siiSEITimeScale = value; }
  uint32_t getSiiSEITimeScale() { return m_siiSEITimeScale; }
  uint32_t getSiiSEIMaxSubLayersMinus1() { return uint32_t(std::max(1u, uint32_t(m_siiSEISubLayerNumUnitsInSI.size())) - 1); }
  bool     getSiiSEIFixedSIwithinCLVS() { return m_siiSEISubLayerNumUnitsInSI.empty(); }
  void     setSiiSEISubLayerNumUnitsInSI(const std::vector<uint32_t>& b) { m_siiSEISubLayerNumUnitsInSI = b; }
  uint32_t getSiiSEISubLayerNumUnitsInSI(uint32_t idx) const { return m_siiSEISubLayerNumUnitsInSI[idx]; }

  void     setSptiSEIEnabled(bool b) { m_sptiSEIEnabled = b; }
  bool     getSptiSEIEnabled() { return m_sptiSEIEnabled; }
  void     setmSptiSEISourceTimingEqualsOutputTimingFlag(bool b) { m_sptiSourceTimingEqualsOutputTimingFlag = b; }
  bool     getmSptiSEISourceTimingEqualsOutputTimingFlag() { return m_sptiSourceTimingEqualsOutputTimingFlag; }
  void     setmSptiSEISourceType(uint32_t b) { m_sptiSourceType = b; }
  uint32_t getmSptiSEISourceType() { return m_sptiSourceType; }
  void     setmSptiSEITimeScale(uint32_t b) { m_sptiTimeScale = b; }
  uint32_t getmSptiSEITimeScale() { return m_sptiTimeScale; }
  void     setmSptiSEINumUnitsInElementalInterval(uint32_t b) { m_sptiNumUnitsInElementalInterval = b; }
  uint32_t getmSptiSEINumUnitsInElementalInterval() { return m_sptiNumUnitsInElementalInterval; }
  void     setmSptiSEIDirectionFlag(bool b) { m_sptiDirectionFlag = b; }
  uint32_t getmSptiSEIDirectionFlag() { return m_sptiDirectionFlag; }

  void        setNNPostFilterSEICharacteristicsEnabled(bool enabledFlag)                                                { m_nnPostFilterSEICharacteristicsEnabled = enabledFlag; }
  bool        getNNPostFilterSEICharacteristicsEnabled() const                                                          { return m_nnPostFilterSEICharacteristicsEnabled; }
  void        setNNPostFilterSEICharacteristicsUseSuffixSEI(bool suffixFlag)                                            { m_nnPostFilterSEICharacteristicsUseSuffixSEI = suffixFlag; }
  bool        getNNPostFilterSEICharacteristicsUseSuffixSEI() const                                                     { return m_nnPostFilterSEICharacteristicsUseSuffixSEI; }
  void        setNNPostFilterSEICharacteristicsNumFilters(int numFilters)                                               { m_nnPostFilterSEICharacteristicsNumFilters = numFilters; }
  int         getNNPostFilterSEICharacteristicsNumFilters() const                                                       { return m_nnPostFilterSEICharacteristicsNumFilters; }
  void        setNNPostFilterSEICharacteristicsId(uint32_t id, int filterIdx)                                           { m_nnPostFilterSEICharacteristicsId[filterIdx] = id; }
  uint32_t    getNNPostFilterSEICharacteristicsId(int filterIdx) const                                                  { return m_nnPostFilterSEICharacteristicsId[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsModeIdc(uint32_t idc, int filterIdx)                                     { m_nnPostFilterSEICharacteristicsModeIdc[filterIdx] = idc; }
  uint32_t    getNNPostFilterSEICharacteristicsModeIdc(int filterIdx) const                                             { return m_nnPostFilterSEICharacteristicsModeIdc[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsPropertyPresentFlag(bool propertyPresentFlag, int filterIdx)   { m_nnPostFilterSEICharacteristicsPropertyPresentFlag[filterIdx] = propertyPresentFlag; }
  bool        getNNPostFilterSEICharacteristicsPropertyPresentFlag(int filterIdx) const                            { return m_nnPostFilterSEICharacteristicsPropertyPresentFlag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsBaseFlag(bool baseFlag, int filterIdx)                                   { m_nnPostFilterSEICharacteristicsBaseFlag[filterIdx] = baseFlag; }
  bool        getNNPostFilterSEICharacteristicsBaseFlag(int filterIdx) const                                            { return m_nnPostFilterSEICharacteristicsBaseFlag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsPurpose(uint32_t purpose, int filterIdx)                                 { m_nnPostFilterSEICharacteristicsPurpose[filterIdx] = purpose; }
  uint32_t    getNNPostFilterSEICharacteristicsPurpose(int filterIdx) const                                             { return m_nnPostFilterSEICharacteristicsPurpose[filterIdx]; }

  void        setNNPostFilterSEICharacteristicsOutSubCFlag(bool SubCFlag, int filterIdx) { m_nnPostFilterSEICharacteristicsOutSubCFlag[filterIdx] = SubCFlag; }
  bool        getNNPostFilterSEICharacteristicsOutSubCFlag(int filterIdx) const { return m_nnPostFilterSEICharacteristicsOutSubCFlag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsOutColourFormatIdc(ChromaFormat outColourFormatIdc, int filterIdx)     { m_nnPostFilterSEICharacteristicsOutColourFormatIdc[filterIdx] = outColourFormatIdc; }
  ChromaFormat getNNPostFilterSEICharacteristicsOutColourFormatIdc(int filterIdx) const                               { return m_nnPostFilterSEICharacteristicsOutColourFormatIdc[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsPicWidthNumeratorMinus1(uint32_t widthNumMinus1, int filterIdx)           { m_nnPostFilterSEICharacteristicsPicWidthNumeratorMinus1[filterIdx] = widthNumMinus1; }
  uint32_t    getNNPostFilterSEICharacteristicsPicWidthNumeratorMinus1(int filterIdx) const                              { return m_nnPostFilterSEICharacteristicsPicWidthNumeratorMinus1[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsPicWidthDenominatorMinus1(uint32_t widthDenomMinus1, int filterIdx)       { m_nnPostFilterSEICharacteristicsPicWidthDenominatorMinus1[filterIdx] = widthDenomMinus1; }
  uint32_t    getNNPostFilterSEICharacteristicsPicWidthDenominatorMinus1(int filterIdx) const                            { return m_nnPostFilterSEICharacteristicsPicWidthDenominatorMinus1[filterIdx]; }

  void        setNNPostFilterSEICharacteristicsPicHeightNumeratorMinus1(uint32_t heightNumMinus1, int filterIdx)           { m_nnPostFilterSEICharacteristicsPicHeightNumeratorMinus1[filterIdx] = heightNumMinus1; }
  uint32_t    getNNPostFilterSEICharacteristicsPicHeightNumeratorMinus1(int filterIdx) const                              { return m_nnPostFilterSEICharacteristicsPicHeightNumeratorMinus1[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsPicHeightDenominatorMinus1(uint32_t heightDenomMinus1, int filterIdx)       { m_nnPostFilterSEICharacteristicsPicHeightDenominatorMinus1[filterIdx] = heightDenomMinus1; }
  uint32_t    getNNPostFilterSEICharacteristicsPicHeightDenominatorMinus1(int filterIdx) const                            { return m_nnPostFilterSEICharacteristicsPicHeightDenominatorMinus1[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsInpTensorBitDepthLumaMinus8(uint32_t inpTensorBitDepthLumaMinus8, int filterIdx)     { m_nnPostFilterSEICharacteristicsInpTensorBitDepthLumaMinus8[filterIdx] = inpTensorBitDepthLumaMinus8; }
  uint32_t    getNNPostFilterSEICharacteristicsInpTensorBitDepthLumaMinus8(int filterIdx) const                                     { return m_nnPostFilterSEICharacteristicsInpTensorBitDepthLumaMinus8[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsInpTensorBitDepthChromaMinus8(uint32_t inpTensorBitDepthChromaMinus8, int filterIdx) { m_nnPostFilterSEICharacteristicsInpTensorBitDepthChromaMinus8[filterIdx] = inpTensorBitDepthChromaMinus8; }
  uint32_t    getNNPostFilterSEICharacteristicsInpTensorBitDepthChromaMinus8(int filterIdx) const                                   { return m_nnPostFilterSEICharacteristicsInpTensorBitDepthChromaMinus8[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsOutTensorBitDepthLumaMinus8(uint32_t outTensorBitDepthLumaMinus8, int filterIdx)     { m_nnPostFilterSEICharacteristicsOutTensorBitDepthLumaMinus8[filterIdx] = outTensorBitDepthLumaMinus8; }
  uint32_t    getNNPostFilterSEICharacteristicsOutTensorBitDepthLumaMinus8(int filterIdx) const                                     { return m_nnPostFilterSEICharacteristicsOutTensorBitDepthLumaMinus8[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsOutTensorBitDepthChromaMinus8(uint32_t outTensorBitDepthChromaMinus8, int filterIdx) { m_nnPostFilterSEICharacteristicsOutTensorBitDepthChromaMinus8[filterIdx] = outTensorBitDepthChromaMinus8; }
  uint32_t    getNNPostFilterSEICharacteristicsOutTensorBitDepthChromaMinus8(int filterIdx) const                                   { return m_nnPostFilterSEICharacteristicsOutTensorBitDepthChromaMinus8[filterIdx]; }
  void setNNPostFilterSEICharacteristicsAuxInpIdc(uint32_t auxInpIdc, int filterIdx)
  {
    m_nnPostFilterSEICharacteristicsAuxInpIdc[filterIdx] = auxInpIdc;
  }
  uint32_t getNNPostFilterSEICharacteristicsAuxInpIdc(int filterIdx) const
  {
    return m_nnPostFilterSEICharacteristicsAuxInpIdc[filterIdx];
  }
  void setNNPostFilterSEICharacteristicsSepColDescriptionFlag(bool sepColDescriptionFlag, int filterIdx)
  {
    m_nnPostFilterSEICharacteristicsSepColDescriptionFlag[filterIdx] = sepColDescriptionFlag;
  }
  bool getNNPostFilterSEICharacteristicsSepColDescriptionFlag(int filterIdx) const
  {
    return m_nnPostFilterSEICharacteristicsSepColDescriptionFlag[filterIdx];
  }
  void setNNPostFilterSEICharacteristicsFullRangeFlag(bool fullRangeFlag, int filterIdx)
  {
    m_nnPostFilterSEICharacteristicsFullRangeFlag[filterIdx] = fullRangeFlag;
  }
  bool getNNPostFilterSEICharacteristicsFullRangeFlag(int filterIdx) const
  {
    return m_nnPostFilterSEICharacteristicsFullRangeFlag[filterIdx];
  }
  void        setNNPostFilterSEICharacteristicsColPrimaries(uint32_t colPrimaries, int filterIdx)                       { m_nnPostFilterSEICharacteristicsColPrimaries[filterIdx] = colPrimaries; }
  uint32_t    getNNPostFilterSEICharacteristicsColPrimaries(int filterIdx) const                                        { return m_nnPostFilterSEICharacteristicsColPrimaries[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsTransCharacteristics(uint32_t transCharacteristics, int filterIdx)       { m_nnPostFilterSEICharacteristicsTransCharacteristics[filterIdx] = transCharacteristics; }
  uint32_t    getNNPostFilterSEICharacteristicsTransCharacteristics(int filterIdx) const                                { return m_nnPostFilterSEICharacteristicsTransCharacteristics[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsMatrixCoeffs(uint32_t matrixCoeffs, int filterIdx)                       { m_nnPostFilterSEICharacteristicsMatrixCoeffs[filterIdx] = matrixCoeffs; }
  uint32_t    getNNPostFilterSEICharacteristicsMatrixCoeffs(int filterIdx) const                                        { return m_nnPostFilterSEICharacteristicsMatrixCoeffs[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsComponentLastFlag(bool componentLastFlag, int filterIdx)                 { m_nnPostFilterSEICharacteristicsComponentLastFlag[filterIdx] = componentLastFlag; }
  bool        getNNPostFilterSEICharacteristicsComponentLastFlag(int filterIdx) const                                   { return m_nnPostFilterSEICharacteristicsComponentLastFlag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsInpFormatIdc(uint32_t inpFormatIdc, int filterIdx)                       { m_nnPostFilterSEICharacteristicsInpFormatIdc[filterIdx] = inpFormatIdc; }
  uint32_t    getNNPostFilterSEICharacteristicsInpFormatIdc(int filterIdx) const                                        { return m_nnPostFilterSEICharacteristicsInpFormatIdc[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsInpOrderIdc(uint32_t inpOrderIdc, int filterIdx)                         { m_nnPostFilterSEICharacteristicsInpOrderIdc[filterIdx] = inpOrderIdc; }
  uint32_t    getNNPostFilterSEICharacteristicsInpOrderIdc(int filterIdx) const                                         { return m_nnPostFilterSEICharacteristicsInpOrderIdc[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsOutFormatIdc(uint32_t outFormatIdc, int filterIdx)                       { m_nnPostFilterSEICharacteristicsOutFormatIdc[filterIdx] = outFormatIdc; }
  uint32_t    getNNPostFilterSEICharacteristicsOutFormatIdc(int filterIdx) const                                        { return m_nnPostFilterSEICharacteristicsOutFormatIdc[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsOutOrderIdc(uint32_t outOrderIdc, int filterIdx)                         { m_nnPostFilterSEICharacteristicsOutOrderIdc[filterIdx] = outOrderIdc; }
  uint32_t    getNNPostFilterSEICharacteristicsOutOrderIdc(int filterIdx) const                                         { return m_nnPostFilterSEICharacteristicsOutOrderIdc[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsChromaLocInfoPresentFlag(bool chromaLocInfoPresentFlag, int filterIdx)   { m_nnPostFilterSEICharacteristicsChromaLocInfoPresentFlag[filterIdx] = chromaLocInfoPresentFlag; }
  bool        getNNPostFilterSEICharacteristicsChromaLocInfoPresentFlag(int filterIdx) const                            { return m_nnPostFilterSEICharacteristicsChromaLocInfoPresentFlag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsChromaSampleLocTypeFrame(Chroma420LocType chromaSampleLocTypeFrame, int filterIdx) {m_nnPostFilterSEICharacteristicsChromaSampleLocTypeFrame[filterIdx] = chromaSampleLocTypeFrame;}
  Chroma420LocType getNNPostFilterSEICharacteristicsChromaSampleLocTypeFrame(int filterIdx) const {return  m_nnPostFilterSEICharacteristicsChromaSampleLocTypeFrame[filterIdx];}
  void        setNNPostFilterSEICharacteristicsConstantPatchSizeFlag(bool constantPatchSizeFlag, int filterIdx)         { m_nnPostFilterSEICharacteristicsConstantPatchSizeFlag[filterIdx] = constantPatchSizeFlag; }
  bool        getNNPostFilterSEICharacteristicsConstantPatchSizeFlag(int filterIdx) const                               { return m_nnPostFilterSEICharacteristicsConstantPatchSizeFlag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsPatchWidthMinus1(uint32_t patchWidthMinus1, int filterIdx)               { m_nnPostFilterSEICharacteristicsPatchWidthMinus1[filterIdx] = patchWidthMinus1; }
  uint32_t    getNNPostFilterSEICharacteristicsPatchWidthMinus1(int filterIdx) const                                    { return m_nnPostFilterSEICharacteristicsPatchWidthMinus1[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsPatchHeightMinus1(uint32_t patchHeightMinus1, int filterIdx)             { m_nnPostFilterSEICharacteristicsPatchHeightMinus1[filterIdx] = patchHeightMinus1; }
  void        setNNPostFilterSEICharacteristicsExtendedPatchWidthCdDeltaMinus1(uint32_t extendedPatchWidthCdDeltaMinus1, int filterIdx)   { m_nnPostFilterSEICharacteristicsExtendedPatchWidthCdDeltaMinus1[filterIdx] = extendedPatchWidthCdDeltaMinus1; }
  uint32_t    getNNPostFilterSEICharacteristicsExtendedPatchWidthCdDeltaMinus1(int filterIdx) const                                       { return m_nnPostFilterSEICharacteristicsExtendedPatchWidthCdDeltaMinus1[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsExtendedPatchHeightCdDeltaMinus1(uint32_t extendedPatchHeightCdDeltaMinus1, int filterIdx) { m_nnPostFilterSEICharacteristicsExtendedPatchHeightCdDeltaMinus1[filterIdx] = extendedPatchHeightCdDeltaMinus1; }
  uint32_t    getNNPostFilterSEICharacteristicsExtendedPatchHeightCdDeltaMinus1(int filterIdx) const                                      { return m_nnPostFilterSEICharacteristicsExtendedPatchHeightCdDeltaMinus1[filterIdx]; }
  uint32_t    getNNPostFilterSEICharacteristicsPatchHeightMinus1(int filterIdx) const                                   { return m_nnPostFilterSEICharacteristicsPatchHeightMinus1[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsOverlap(uint32_t overlap, int filterIdx)                                 { m_nnPostFilterSEICharacteristicsOverlap[filterIdx] = overlap; }
  uint32_t    getNNPostFilterSEICharacteristicsOverlap(int filterIdx) const                                             { return m_nnPostFilterSEICharacteristicsOverlap[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsPaddingType(uint32_t paddingType, int filterIdx)                         { m_nnPostFilterSEICharacteristicsPaddingType[filterIdx] = paddingType; }
  uint32_t    getNNPostFilterSEICharacteristicsPaddingType(int filterIdx) const                                         { return m_nnPostFilterSEICharacteristicsPaddingType[filterIdx]; }

  void        setNNPostFilterSEICharacteristicsLumaPadding(uint32_t lumaPadding, int filterIdx) { m_nnPostFilterSEICharacteristicsLumaPadding[filterIdx] = lumaPadding; }
  uint32_t    getNNPostFilterSEICharacteristicsLumaPadding(int filterIdx) const { return m_nnPostFilterSEICharacteristicsLumaPadding[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsCbPadding(uint32_t cbPadding, int filterIdx) { m_nnPostFilterSEICharacteristicsCbPadding[filterIdx] = cbPadding; }
  uint32_t    getNNPostFilterSEICharacteristicsCbPadding(int filterIdx) const { return m_nnPostFilterSEICharacteristicsCbPadding[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsCrPadding(uint32_t crPadding, int filterIdx) { m_nnPostFilterSEICharacteristicsCrPadding[filterIdx] = crPadding; }
  uint32_t    getNNPostFilterSEICharacteristicsCrPadding(int filterIdx) const { return m_nnPostFilterSEICharacteristicsCrPadding[filterIdx]; }

  void        setNNPostFilterSEICharacteristicsComplexityInfoPresentFlag(bool complexityInfoPresentFlag, int filterIdx) { m_nnPostFilterSEICharacteristicsComplexityInfoPresentFlag[filterIdx] = complexityInfoPresentFlag; }
  bool        getNNPostFilterSEICharacteristicsComplexityInfoPresentFlag(int filterIdx) const                           { return m_nnPostFilterSEICharacteristicsComplexityInfoPresentFlag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsApplicationPurposeTagUriPresentFlag(bool applicationPurposeTagUriPresentFlag, int filterIdx) { m_nnPostFilterSEICharacteristicsApplicationPurposeTagUriPresentFlag[filterIdx] = applicationPurposeTagUriPresentFlag; }
  bool        getNNPostFilterSEICharacteristicsApplicationPurposeTagUriPresentFlag(int filterIdx) const                 { return m_nnPostFilterSEICharacteristicsApplicationPurposeTagUriPresentFlag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsApplicationPurposeTagUri(std::string applicationPurposeTagUri, int filterIdx) { m_nnPostFilterSEICharacteristicsApplicationPurposeTagUri[filterIdx] = applicationPurposeTagUri; }
  std::string getNNPostFilterSEICharacteristicsApplicationPurposeTagUri(int filterIdx) const                             { return m_nnPostFilterSEICharacteristicsApplicationPurposeTagUri[filterIdx]; }
#if NNPFC_SCAN_TYPE_IDC
  void        setNNPostFilterSEICharacteristicsScanTypeIdc(uint32_t scanTypeIdc, int filterIdx)                         { m_nnPostFilterSEICharacteristicsScanTypeIdc[filterIdx] = scanTypeIdc; }
  uint32_t    getNNPostFilterSEICharacteristicsScanTypeIdc(int filterIdx) const                                         { return m_nnPostFilterSEICharacteristicsScanTypeIdc[filterIdx]; }
#endif
  void        setNNPostFilterSEICharacteristicsForHumanViewingIdc(uint32_t forHumanViewingIdc, int filterIdx)           { m_nnPostFilterSEICharacteristicsForHumanViewingIdc[filterIdx] = forHumanViewingIdc; }
  uint32_t    getNNPostFilterSEICharacteristicsForHumanViewingIdc(int filterIdx) const                                  { return m_nnPostFilterSEICharacteristicsForHumanViewingIdc[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsForMachineAnalysisIdc(uint32_t forMachineAnalysisIdc, int filterIdx)     { m_nnPostFilterSEICharacteristicsForMachineAnalysisIdc[filterIdx] = forMachineAnalysisIdc; }
  uint32_t    getNNPostFilterSEICharacteristicsForMachineAnalysisIdc(int filterIdx) const                               { return m_nnPostFilterSEICharacteristicsForMachineAnalysisIdc[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsUriTag(std::string uriTag, int filterIdx)                                { m_nnPostFilterSEICharacteristicsUriTag[filterIdx] = uriTag; }
  std::string getNNPostFilterSEICharacteristicsUriTag(int filterIdx) const                                              { return m_nnPostFilterSEICharacteristicsUriTag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsUri(std::string uri, int filterIdx)                                      { m_nnPostFilterSEICharacteristicsUri[filterIdx] = uri; }
  std::string getNNPostFilterSEICharacteristicsUri(int filterIdx) const                                                 { return m_nnPostFilterSEICharacteristicsUri[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsParameterTypeIdc(uint32_t parameterTypeIdc, int filterIdx) { m_nnPostFilterSEICharacteristicsParameterTypeIdc[filterIdx] = parameterTypeIdc; }
  uint32_t    getNNPostFilterSEICharacteristicsParameterTypeIdc(int filterIdx) const { return m_nnPostFilterSEICharacteristicsParameterTypeIdc[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsLog2ParameterBitLengthMinus3 (uint32_t log2ParameterBitLengthMinus3 , int filterIdx) { m_nnPostFilterSEICharacteristicsLog2ParameterBitLengthMinus3[filterIdx] = log2ParameterBitLengthMinus3 ; }
  uint32_t    getNNPostFilterSEICharacteristicsLog2ParameterBitLengthMinus3 (int filterIdx) const                       { return m_nnPostFilterSEICharacteristicsLog2ParameterBitLengthMinus3[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsNumParametersIdc  (uint32_t numParametersIdc  , int filterIdx)           { m_nnPostFilterSEICharacteristicsNumParametersIdc[filterIdx] = numParametersIdc  ; }
  uint32_t    getNNPostFilterSEICharacteristicsNumParametersIdc  (int filterIdx) const                                  { return m_nnPostFilterSEICharacteristicsNumParametersIdc[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsNumKmacOperationsIdc(uint32_t numKmacOperationsIdc   , int filterIdx)    { m_nnPostFilterSEICharacteristicsNumKmacOperationsIdc[filterIdx] = numKmacOperationsIdc   ; }
  uint32_t    getNNPostFilterSEICharacteristicsNumKmacOperationsIdc(int filterIdx) const                                { return m_nnPostFilterSEICharacteristicsNumKmacOperationsIdc[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsTotalKilobyteSize(uint32_t totalKilobyteSize, int filterIdx)             { m_nnPostFilterSEICharacteristicsTotalKilobyteSize[filterIdx] = totalKilobyteSize; }
  uint32_t    getNNPostFilterSEICharacteristicsTotalKilobyteSize(int filterIdx) const                                   { return m_nnPostFilterSEICharacteristicsTotalKilobyteSize[filterIdx]; }

  void        setNNPostFilterSEICharacteristicsPayloadFilename(std::string payloadFilename, int filterIdx)              { m_nnPostFilterSEICharacteristicsPayloadFilename[filterIdx] = payloadFilename; }
  std::string getNNPostFilterSEICharacteristicsPayloadFilename(int filterIdx) const                                     { return m_nnPostFilterSEICharacteristicsPayloadFilename[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsNumberInputDecodedPicturesMinus1(uint32_t value, int filterIdx)          { m_nnPostFilterSEICharacteristicsNumberInputDecodedPicturesMinus1[filterIdx] = value; }
  uint32_t    getNNPostFilterSEICharacteristicsNumberInputDecodedPicturesMinus1(int filterIdx) const                    { return m_nnPostFilterSEICharacteristicsNumberInputDecodedPicturesMinus1[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsNumberInterpolatedPictures(std::vector<uint32_t> value, int filterIdx)   { m_nnPostFilterSEICharacteristicsNumberInterpolatedPictures[filterIdx] = value; }
  const       std::vector<uint32_t>& getNNPostFilterSEICharacteristicsNumberInterpolatedPictures(int filterIdx)         { return m_nnPostFilterSEICharacteristicsNumberInterpolatedPictures[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsNumberExtrapolatedPicturesMinus1(uint32_t value, int filterIdx)          { m_nnPostFilterSEICharacteristicsNumberExtrapolatedPicturesMinus1[filterIdx] = value; }
  uint32_t    getNNPostFilterSEICharacteristicsNumberExtrapolatedPicturesMinus1(int filterIdx)                          { return m_nnPostFilterSEICharacteristicsNumberExtrapolatedPicturesMinus1[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsSpatialExtrapolationLeftOffset(int32_t value, int filterIdx)             { m_nnPostFilterSEICharacteristicsSpatialExtrapolationLeftOffset[filterIdx] = value; }
  int32_t     getNNPostFilterSEICharacteristicsSpatialExtrapolationLeftOffset(int filterIdx)                            { return m_nnPostFilterSEICharacteristicsSpatialExtrapolationLeftOffset[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsSpatialExtrapolationRightOffset(int32_t value, int filterIdx)            { m_nnPostFilterSEICharacteristicsSpatialExtrapolationRightOffset[filterIdx] = value; }
  int32_t     getNNPostFilterSEICharacteristicsSpatialExtrapolationRightOffset(int filterIdx)                           { return m_nnPostFilterSEICharacteristicsSpatialExtrapolationRightOffset[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsSpatialExtrapolationTopOffset(int32_t value, int filterIdx)              { m_nnPostFilterSEICharacteristicsSpatialExtrapolationTopOffset[filterIdx] = value; }
  int32_t     getNNPostFilterSEICharacteristicsSpatialExtrapolationTopOffset(int filterIdx)                             { return m_nnPostFilterSEICharacteristicsSpatialExtrapolationTopOffset[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsSpatialExtrapolationBottomOffset(int32_t value, int filterIdx)           { m_nnPostFilterSEICharacteristicsSpatialExtrapolationBottomOffset[filterIdx] = value; }
  int32_t     getNNPostFilterSEICharacteristicsSpatialExtrapolationBottomOffset(int filterIdx)                          { return m_nnPostFilterSEICharacteristicsSpatialExtrapolationBottomOffset[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsInbandPromptFlag(bool promptPresentFlag, int filterIdx)                  { m_nnPostFilterSEICharacteristicsInbandPromptFlag[filterIdx] = promptPresentFlag; }
  bool        getNNPostFilterSEICharacteristicsInbandPromptFlag(int filterIdx) const                                    { return m_nnPostFilterSEICharacteristicsInbandPromptFlag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsPrompt(std::string prompt, int filterIdx)                                { m_nnPostFilterSEICharacteristicsPrompt[filterIdx] = prompt; }
  std::string getNNPostFilterSEICharacteristicsPrompt(int filterIdx) const                                              { return m_nnPostFilterSEICharacteristicsPrompt[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsInputPicOutputFlag(std::vector<bool> value, int filterIdx)   { m_nnPostFilterSEICharacteristicsInputPicOutputFlag[filterIdx] = value; }
  const       std::vector<bool>& getNNPostFilterSEICharacteristicsInputPicOutputFlag(int filterIdx)         { return m_nnPostFilterSEICharacteristicsInputPicOutputFlag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsAbsentInputPicZeroFlag(bool absentInputPicZeroFlag, int filterIdx)       { m_nnPostFilterSEICharacteristicsAbsentInputPicZeroFlag[filterIdx] = absentInputPicZeroFlag; }
  bool        getNNPostFilterSEICharacteristicsAbsentInputPicZeroFlag(int filterIdx) const                              { return m_nnPostFilterSEICharacteristicsAbsentInputPicZeroFlag[filterIdx]; }
#if JVET_AK0326_NNPF_SEED
  void        setNNPostFilterSEICharacteristicsInbandSeedFlag(bool inbandSeedFlag, int filterIdx)                       { m_nnPostFilterSEICharacteristicsInbandSeedFlag[filterIdx] = inbandSeedFlag; }
  bool        getNNPostFilterSEICharacteristicsInbandSeedFlag(int filterIdx) const                                      { return m_nnPostFilterSEICharacteristicsInbandSeedFlag[filterIdx]; }
  void        setNNPostFilterSEICharacteristicsSeed(uint32_t seed, int filterIdx)                                       { m_nnPostFilterSEICharacteristicsSeed[filterIdx] = seed; }
  uint32_t    getNNPostFilterSEICharacteristicsSeed(int filterIdx) const                                                { return m_nnPostFilterSEICharacteristicsSeed[filterIdx]; }
#endif
  void        setNnPostFilterSEIActivationEnabled(bool enabledFlag)                                                     { m_nnPostFilterSEIActivationEnabled = enabledFlag; }
  bool        getNnPostFilterSEIActivationEnabled() const                                                               { return m_nnPostFilterSEIActivationEnabled; }
  void        setNnPostFilterSEIActivationUseSuffixSEI(bool suffixFlag)                                                 { m_nnPostFilterSEIActivationUseSuffixSEI = suffixFlag; }
  bool        getNnPostFilterSEIActivationUseSuffixSEI() const                                                          { return m_nnPostFilterSEIActivationUseSuffixSEI; }
  void        setNnPostFilterSEIActivationTargetId(uint32_t targetId)                                                   { m_nnPostFilterSEIActivationTargetId = targetId; }
  uint32_t    getNnPostFilterSEIActivationTargetId() const                                                              { return m_nnPostFilterSEIActivationTargetId; }
  void setNnPostFilterSEIActivationCancelFlag(bool cancelFlag)                                                          { m_nnPostFilterSEIActivationCancelFlag = cancelFlag; }
  bool getNnPostFilterSEIActivationCancelFlag() const                                                                   { return m_nnPostFilterSEIActivationCancelFlag;}
  void setNnPostFilterSEIActivationTargetBaseFlag(bool targetBaseFlag)                                                  { m_nnPostFilterSEIActivationTargetBaseFlag = targetBaseFlag; }
  bool getNnPostFilterSEIActivationTargetBaseFlag() const                                                               { return m_nnPostFilterSEIActivationTargetBaseFlag;}
  void setNnPostFilterSEIActivationNoPrevCLVSFlag(bool noPrevCLVSFlag)                                                  { m_nnPostFilterSEIActivationNoPrevCLVSFlag = noPrevCLVSFlag; }
  bool getNnPostFilterSEIActivationNoPrevCLVSFlag() const                                                               { return m_nnPostFilterSEIActivationNoPrevCLVSFlag;}
  void setNnPostFilterSEIActivationNoFollCLVSFlag(bool noFollCLVSFlag)                                                  { m_nnPostFilterSEIActivationNoFollCLVSFlag = noFollCLVSFlag; }
  bool getNnPostFilterSEIActivationNoFollCLVSFlag() const                                                               { return m_nnPostFilterSEIActivationNoFollCLVSFlag;}
  void setNnPostFilterSEIActivationPersistenceFlag(bool persistenceFlag)                                                          { m_nnPostFilterSEIActivationPersistenceFlag = persistenceFlag; }
  bool getNnPostFilterSEIActivationPersistenceFlag() const                                                                   { return m_nnPostFilterSEIActivationPersistenceFlag;}
  uint32_t    getNnPostFilterSEIActivationNumOutputEntries() const                                                      { return (uint32_t)m_nnPostFilterSEIActivationOutputflag.size(); }
  void        setNnPostFilterSEIActivationOutputFlag(std::vector<bool> value)                                           { m_nnPostFilterSEIActivationOutputflag = value; }
  const       std::vector<bool>& getNnPostFilterSEIActivationOutputFlag() const                                         { return m_nnPostFilterSEIActivationOutputflag; }
#if JVET_AJ0104_NNPFA_PROMPT_UPDATE
  void        setNnPostFilterSEIActivationPromptUpdateFlag(bool  promptUpdateFlag)                                      { m_nnPostFilterSEIActivationPromptUpdateFlag = promptUpdateFlag; }
  bool        getNnPostFilterSEIActivationPromptUpdateFlag() const                                                      { return m_nnPostFilterSEIActivationPromptUpdateFlag; }
  void        setNnPostFilterSEIActivationPrompt(std::string prompt)                                                    { m_nnPostFilterSEIActivationPrompt = prompt; }
  std::string getNnPostFilterSEIActivationPrompt() const                                                                { return m_nnPostFilterSEIActivationPrompt; }
#endif
#if JVET_AK0326_NNPF_SEED
  void        setNnPostFilterSEIActivationSeedUpdateFlag(bool seedUpdateFlag)                                           { m_nnPostFilterSEIActivationSeedUpdateFlag = seedUpdateFlag; }
  bool        getNnPostFilterSEIActivationSeedUpdateFlag() const                                                        { return m_nnPostFilterSEIActivationSeedUpdateFlag; }
  void        setNnPostFilterSEIActivationSeed(uint32_t seed)                                                           { m_nnPostFilterSEIActivationSeed = seed; }
  uint32_t    getNnPostFilterSEIActivationSeed() const                                                                  { return m_nnPostFilterSEIActivationSeed; }
#endif
#if JVET_AJ0114_NNPFA_NUM_PIC_SHIFT
#if JVET_AL0075_NNPFA_SELECTED_INPUT_FLAG
  void        setNnPostFilterSEIActivationSelectedInputFlag(uint32_t selectedInputFlag)                                 { m_nnPostFilterSEIActivationSelectedInputFlag = selectedInputFlag; }
  bool        getNnPostFilterSEIActivationSelectedInputFlag() const                                                     { return m_nnPostFilterSEIActivationSelectedInputFlag; }
#endif 
  void        setNnPostFilterSEIActivationNumInputPicShift(uint32_t numInputPicShift)                                   { m_nnPostFilterSEIActivationNumInputPicShift = numInputPicShift; }
  uint32_t    getNnPostFilterSEIActivationNumInputPicShift() const                                                      { return m_nnPostFilterSEIActivationNumInputPicShift; }
#endif 

  void  setBufferingPeriodSEIEnabled(bool b)                         { m_bufferingPeriodSEIEnabled = b; }
  bool  getBufferingPeriodSEIEnabled() const                         { return m_bufferingPeriodSEIEnabled; }
  void  setPictureTimingSEIEnabled(bool b)                           { m_pictureTimingSEIEnabled = b; }
  bool  getPictureTimingSEIEnabled() const                           { return m_pictureTimingSEIEnabled; }
  void  setFrameFieldInfoSEIEnabled(bool b)                           { m_frameFieldInfoSEIEnabled = b; }
  bool  getFrameFieldInfoSEIEnabled() const                           { return m_frameFieldInfoSEIEnabled; }
  void  setDependentRAPIndicationSEIEnabled(bool b)                  { m_dependentRAPIndicationSEIEnabled = b; }
  int   getDependentRAPIndicationSEIEnabled() const                  { return m_dependentRAPIndicationSEIEnabled; }
  void  setEdrapIndicationSEIEnabled(bool b)                         { m_edrapIndicationSEIEnabled = b; }
  int   getEdrapIndicationSEIEnabled() const                         { return m_edrapIndicationSEIEnabled; }
  void  setFramePackingArrangementSEIEnabled(bool b)                 { m_framePackingSEIEnabled = b; }
  bool  getFramePackingArrangementSEIEnabled() const                 { return m_framePackingSEIEnabled; }
  void  setFramePackingArrangementSEIType(int b)                     { m_framePackingSEIType = b; }
  int   getFramePackingArrangementSEIType()                          { return m_framePackingSEIType; }
  void  setFramePackingArrangementSEIId(int b)                       { m_framePackingSEIId = b; }
  int   getFramePackingArrangementSEIId()                            { return m_framePackingSEIId; }
  void  setFramePackingArrangementSEIQuincunx(int b)                 { m_framePackingSEIQuincunx = b; }
  int   getFramePackingArrangementSEIQuincunx()                      { return m_framePackingSEIQuincunx; }
  void  setFramePackingArrangementSEIInterpretation(int b)           { m_framePackingSEIInterpretation = b; }
  int   getFramePackingArrangementSEIInterpretation()                { return m_framePackingSEIInterpretation; }
  void  setDoSEIEnabled(bool b)                                      { m_doSEIEnabled = b; }
  bool  getDoSEIEnabled() const                                      { return m_doSEIEnabled; }
  void  setDoSEICancelFlag(bool b)                                   { m_doSEICancelFlag = b; }
  bool  getDoSEICancelFlag()                                         { return m_doSEICancelFlag; }
  void  setDoSEIPersistenceFlag(bool b)                              { m_doSEIPersistenceFlag = b; }
  bool  getDoSEIPersistenceFlag()                                    { return m_doSEIPersistenceFlag; }
  void  setDoSEITransformType(const int type)                        { m_doSEITransformType = type; }
  int   getDOSEITransformType() const                                { return m_doSEITransformType; }
  void  setParameterSetsInclusionIndicationSEIEnabled(bool b)        { m_parameterSetsInclusionIndicationSEIEnabled = b; }
  bool  getParameterSetsInclusionIndicationSEIEnabled() const        { return m_parameterSetsInclusionIndicationSEIEnabled; }
#if GREEN_METADATA_SEI_ENABLED
  void setSEIGreenMetadataInfoSEIEnable(int b)                       { (b >= 0) ? m_greenMetadataInfoSEIEnabled = 1 : m_greenMetadataInfoSEIEnabled =0;}
  bool getSEIGreenMetadataInfoSEIEnable()                            { return m_greenMetadataInfoSEIEnabled;}
  void setSEIGreenMetadataType(int b)                                { m_greenMetadataType = b;}
  int getSEIGreenMetadataType()                                      { return m_greenMetadataType;}
  int getSEIGreenMetadataGranularityType()                           { return m_greenMetadataGranularityType;}
  void setSEIGreenMetadataGranularityType(int b)                     { m_greenMetadataGranularityType = b;}
  int getSEIGreenMetadataExtendedRepresentation()                    { return m_greenMetadataExtendedRepresentation;}
  void setSEIGreenMetadataExtendedRepresentation(int b)              { m_greenMetadataExtendedRepresentation = b;}
  void setSEIGreenMetadataPeriodType(int b)                          { m_greenMetadataPeriodType = b;}
  int getSEIGreenMetadataPeriodType()                                { return m_greenMetadataPeriodType;}
  void setSEIGreenMetadataPeriodNumSeconds(int b)                    {m_greenMetadataPeriodNumSeconds = b;}
  int getSEIGreenMetadataPeriodNumSeconds()                          {return m_greenMetadataPeriodNumSeconds;}
  void setSEIGreenMetadataPeriodNumPictures(int b)                   {m_greenMetadataPeriodNumPictures = b;}
  int getSEIGreenMetadataPeriodNumPictures()                         {return m_greenMetadataPeriodNumPictures;}
  void setSEIXSDNumberMetrics(int b)                                  { m_xsdNumberMetrics = b;}
  int  getSEIXSDNumberMetrics()                                      { return m_xsdNumberMetrics;}
  void setSEIXSDMetricTypePSNR(bool b)                                { m_xsdMetricTypePSNR = b;}
  bool getSEIXSDMetricTypePSNR()                                     { return m_xsdMetricTypePSNR;}
  void setSEIXSDMetricTypeSSIM(bool b)                                { m_xsdMetricTypeSSIM = b;}
  bool getSEIXSDMetricTypeSSIM()                                     { return m_xsdMetricTypeSSIM;}
  void setSEIXSDMetricTypeWPSNR(bool b)                               { m_xsdMetricTypeWPSNR = b;}
  bool getSEIXSDMetricTypeWPSNR()                                    { return m_xsdMetricTypeWPSNR;}
  void setSEIXSDMetricTypeWSPSNR(bool b)                              { m_xsdMetricTypeWSPSNR = b;}
  bool getSEIXSDMetricTypeWSPSNR()                                   { return m_xsdMetricTypeWSPSNR;}
#if GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464
  void           setSEIGreenMetadataAMIFlags(uint32_t b) { m_greenMetadataAMIFlags = b; }
  uint32_t       getSEIGreenMetadataAMIFlags() const { return m_greenMetadataAMIFlags; }
  void           setSEIGreenMetadataAMIDisplayModel(uint32_t b) { m_greenMetadataAMIDisplayModel = b; }
  const uint32_t getSEIGreenMetadataAMIDisplayModel() const { return m_greenMetadataAMIDisplayModel; }
  void           setSEIGreenMetadataAMIApproximationModel(uint32_t b) { m_greenMetadataAMIApproximationModel = b; }
  const uint32_t getSEIGreenMetadataAMIApproximationModel() const { return m_greenMetadataAMIApproximationModel; }
  void           setSEIGreenMetadataAMIMapNumber(uint32_t b) { m_greenMetadataAMIMapNumber = b; }
  const uint32_t getSEIGreenMetadataAMIMapNumber() const { return m_greenMetadataAMIMapNumber; }

  void                        setSEIGreenMetadataAMILayerId(std::vector<uint8_t>& b) { m_greenMetadataAMILayerId = b; }
  const std::vector<uint8_t>& getSEIGreenMetadataAMILayerId() const { return m_greenMetadataAMILayerId; }

  void setSEIGreenMetadataAMIOlsNumber(std::vector<uint8_t>& b) { m_greenMetadataAMIOlsNumber = b; }
  const std::vector<uint8_t>& getSEIGreenMetadataAMIOlsNumber() const { return m_greenMetadataAMIOlsNumber; }

  void setSEIGreenMetadataAMIOlsId(std::vector<std::vector<uint8_t>>& b) { m_greenMetadataAMIOlsId = b; }
  const std::vector<std::vector<uint8_t>>& getSEIGreenMetadataAMIOlsId() const { return m_greenMetadataAMIOlsId; }

  void setSEIGreenMetadataAMIEnergyReductionRate(std::vector<uint8_t>& b) { m_greenMetadataAMIEnergyReductionRate = b; }
  const std::vector<uint8_t>& getSEIGreenMetadataAMIEnergyReductionRate() const
  {
    return m_greenMetadataAMIEnergyReductionRate;
  }

  void setSEIGreenMetadataAMIVideoQualityMetricType(std::vector<uint8_t>& b)
  {
    m_greenMetadataAMIVideoQualityMetricType = b;
  }
  const std::vector<uint8_t>& getSEIGreenMetadataAMIVideoQualityMetricType() const
  {
    return m_greenMetadataAMIVideoQualityMetricType;
  }

  void setSEIGreenMetadataAMIVideoQualityLevel(std::vector<uint16_t>& b) { m_greenMetadataAMIVideoQualityLevel = b; }
  const std::vector<uint16_t>& getSEIGreenMetadataAMIVideoQualityLevel() const
  {
    return m_greenMetadataAMIVideoQualityLevel;
  }

  void setSEIGreenMetadataAMIMaxValue(std::vector<uint8_t>& b) { m_greenMetadataAMIMaxValue = b; }
  const std::vector<uint8_t>& getSEIGreenMetadataAMIMaxValue() const { return m_greenMetadataAMIMaxValue; }

  void setSEIGreenMetadataAMIAttenuationUseIdc(std::vector<uint8_t>& b) { m_greenMetadataAMIAttenuationUseIdc = b; }
  const std::vector<uint8_t>& getSEIGreenMetadataAMIAttenuationUseIdc() const
  {
    return m_greenMetadataAMIAttenuationUseIdc;
  }

  void setSEIGreenMetadataAMIAttenuationCompIdc(std::vector<uint8_t>& b) { m_greenMetadataAMIAttenuationCompIdc = b; }
  const std::vector<uint8_t>& getSEIGreenMetadataAMIAttenuationCompIdc() const
  {
    return m_greenMetadataAMIAttenuationCompIdc;
  }

  void setSEIGreenMetadataAMIPreprocessingFlag(std::vector<bool>& b) { m_greenMetadataAMIPreprocessingFlag = b; }
  const std::vector<bool>& getSEIGreenMetadataAMIPreprocessingFlag() const
  {
    return m_greenMetadataAMIPreprocessingFlag;
  }

  void setSEIGreenMetadataAMIPreprocessingTypeIdc(std::vector<uint8_t>& b)
  {
    m_greenMetadataAMIPreprocessingTypeIdc = b;
  }
  const std::vector<uint8_t>& getSEIGreenMetadataAMIPreprocessingTypeIdc() const
  {
    return m_greenMetadataAMIPreprocessingTypeIdc;
  }

  void setSEIGreenMetadataAMIPreprocessingScaleIdc(std::vector<uint8_t>& b)
  {
    m_greenMetadataAMIPreprocessingScaleIdc = b;
  }
  const std::vector<uint8_t>& getSEIGreenMetadataAMIPreprocessingScaleIdc() const
  {
    return m_greenMetadataAMIPreprocessingScaleIdc;
  }

  void setSEIGreenMetadataAMIBacklightScalingIdc(std::vector<uint8_t>& b) { m_greenMetadataAMIBacklightScalingIdc = b; }
  const std::vector<uint8_t>& getSEIGreenMetadataAMIBacklightScalingIdc() const
  {
    return m_greenMetadataAMIBacklightScalingIdc;
  }
#endif
#endif
  void  setSelfContainedClvsFlag(bool b)                             { m_selfContainedClvsFlag = b; }
  int   getSelfContainedClvsFlag()                                   { return m_selfContainedClvsFlag; }
  void  setBpDeltasGOPStructure(bool b)                              { m_bpDeltasGOPStructure = b;    }
  bool  getBpDeltasGOPStructure() const                              { return m_bpDeltasGOPStructure; }
  void  setDecodingUnitInfoSEIEnabled(bool b)                        { m_decodingUnitInfoSEIEnabled = b;    }
  bool  getDecodingUnitInfoSEIEnabled() const                        { return m_decodingUnitInfoSEIEnabled; }
  void  setScalableNestingSEIEnabled(bool b)                         { m_scalableNestingSEIEnabled = b; }
  bool  getScalableNestingSEIEnabled() const                         { return m_scalableNestingSEIEnabled; }

  void  setErpSEIEnabled(bool b)                                     { m_erpSEIEnabled = b; }
  bool  getErpSEIEnabled()                                           { return m_erpSEIEnabled; }
  void  setErpSEICancelFlag(bool b)                                  { m_erpSEICancelFlag = b; }
  bool  getErpSEICancelFlag()                                        { return m_erpSEICancelFlag; }
  void  setErpSEIPersistenceFlag(bool b)                             { m_erpSEIPersistenceFlag = b; }
  bool  getErpSEIPersistenceFlag()                                   { return m_erpSEIPersistenceFlag; }
  void  setErpSEIGuardBandFlag(bool b)                               { m_erpSEIGuardBandFlag = b; }
  bool  getErpSEIGuardBandFlag()                                     { return m_erpSEIGuardBandFlag; }
  void  setErpSEIGuardBandType(uint32_t b)                           { m_erpSEIGuardBandType = b; }
  uint32_t  getErpSEIGuardBandType()                                 { return m_erpSEIGuardBandType; }
  void  setErpSEILeftGuardBandWidth(uint32_t b)                      { m_erpSEILeftGuardBandWidth = b; }
  uint32_t  getErpSEILeftGuardBandWidth()                            { return m_erpSEILeftGuardBandWidth; }
  void  setErpSEIRightGuardBandWidth(uint32_t b)                     { m_erpSEIRightGuardBandWidth = b; }
  uint32_t  getErpSEIRightGuardBandWidth()                           { return m_erpSEIRightGuardBandWidth; }
  void  setSphereRotationSEIEnabled(bool b)                          { m_sphereRotationSEIEnabled = b; }
  bool  getSphereRotationSEIEnabled()                                { return m_sphereRotationSEIEnabled; }
  void  setSphereRotationSEICancelFlag(bool b)                       { m_sphereRotationSEICancelFlag = b; }
  bool  getSphereRotationSEICancelFlag()                             { return m_sphereRotationSEICancelFlag; }
  void  setSphereRotationSEIPersistenceFlag(bool b)                  { m_sphereRotationSEIPersistenceFlag = b; }
  bool  getSphereRotationSEIPersistenceFlag()                        { return m_sphereRotationSEIPersistenceFlag; }
  void  setSphereRotationSEIYaw(int b)                               { m_sphereRotationSEIYaw = b; }
  int   getSphereRotationSEIYaw()                                    { return m_sphereRotationSEIYaw; }
  void  setSphereRotationSEIPitch(int b)                             { m_sphereRotationSEIPitch = b; }
  int   getSphereRotationSEIPitch()                                  { return m_sphereRotationSEIPitch; }
  void  setSphereRotationSEIRoll(int b)                              { m_sphereRotationSEIRoll = b; }
  int   getSphereRotationSEIRoll()                                   { return m_sphereRotationSEIRoll; }
  void  setOmniViewportSEIEnabled(bool b)                            { m_omniViewportSEIEnabled = b; }
  bool  getOmniViewportSEIEnabled()                                  { return m_omniViewportSEIEnabled; }
  void  setOmniViewportSEIId(uint32_t b)                             { m_omniViewportSEIId = b; }
  uint32_t  getOmniViewportSEIId()                                   { return m_omniViewportSEIId; }
  void  setOmniViewportSEICancelFlag(bool b)                         { m_omniViewportSEICancelFlag = b; }
  bool  getOmniViewportSEICancelFlag()                               { return m_omniViewportSEICancelFlag; }
  void  setOmniViewportSEIPersistenceFlag(bool b)                    { m_omniViewportSEIPersistenceFlag = b; }
  bool  getOmniViewportSEIPersistenceFlag()                          { return m_omniViewportSEIPersistenceFlag; }
  void  setOmniViewportSEICntMinus1(uint32_t b)                      { m_omniViewportSEICntMinus1 = b; }
  uint32_t  getOmniViewportSEICntMinus1()                            { return m_omniViewportSEICntMinus1; }
  void  setOmniViewportSEIAzimuthCentre(const std::vector<int>& vi)  { m_omniViewportSEIAzimuthCentre = vi; }
  int   getOmniViewportSEIAzimuthCentre(int idx)                     { return m_omniViewportSEIAzimuthCentre[idx]; }
  void  setOmniViewportSEIElevationCentre(const std::vector<int>& vi){ m_omniViewportSEIElevationCentre = vi; }
  int   getOmniViewportSEIElevationCentre(int idx)                   { return m_omniViewportSEIElevationCentre[idx]; }
  void  setOmniViewportSEITiltCentre(const std::vector<int>& vi)     { m_omniViewportSEITiltCentre = vi; }
  int   getOmniViewportSEITiltCentre(int idx)                        { return m_omniViewportSEITiltCentre[idx]; }
  void  setOmniViewportSEIHorRange(const std::vector<uint32_t>& vi)  { m_omniViewportSEIHorRange = vi; }
  uint32_t  getOmniViewportSEIHorRange(int idx)                      { return m_omniViewportSEIHorRange[idx]; }
  void  setOmniViewportSEIVerRange(const std::vector<uint32_t>& vi)  { m_omniViewportSEIVerRange = vi; }
  uint32_t  getOmniViewportSEIVerRange(int idx)                      { return m_omniViewportSEIVerRange[idx]; }
  void  setAnnotatedRegionSEIFileRoot(const std::string &s)          { m_arSEIFileRoot = s; m_arObjects.clear();}
  const std::string &getAnnotatedRegionSEIFileRoot() const           { return m_arSEIFileRoot; }
  void  setObjectMaskInfoSEIFileRoot(const std::string& s)           { m_omiSEIFileRoot = s; }
  const std::string& getObjectMaskInfoSEIFileRoot() const            { return m_omiSEIFileRoot; }
  void     setRwpSEIEnabled(bool b)                                                                     { m_rwpSEIEnabled = b; }
  bool     getRwpSEIEnabled()                                                                           { return m_rwpSEIEnabled; }
  void     setRwpSEIRwpCancelFlag(bool b)                                                               { m_rwpSEIRwpCancelFlag = b; }
  bool     getRwpSEIRwpCancelFlag()                                                                     { return m_rwpSEIRwpCancelFlag; }
  void     setRwpSEIRwpPersistenceFlag (bool b)                                                         { m_rwpSEIRwpPersistenceFlag = b; }
  bool     getRwpSEIRwpPersistenceFlag ()                                                               { return m_rwpSEIRwpPersistenceFlag; }
  void     setRwpSEIConstituentPictureMatchingFlag (bool b)                                             { m_rwpSEIConstituentPictureMatchingFlag = b; }
  bool     getRwpSEIConstituentPictureMatchingFlag ()                                                   { return m_rwpSEIConstituentPictureMatchingFlag; }
  void     setRwpSEINumPackedRegions (int value)                                                        { m_rwpSEINumPackedRegions = value; }
  int      getRwpSEINumPackedRegions ()                                                                 { return m_rwpSEINumPackedRegions; }
  void     setRwpSEIProjPictureWidth (int value)                                                        { m_rwpSEIProjPictureWidth = value; }
  int      getRwpSEIProjPictureWidth ()                                                                 { return m_rwpSEIProjPictureWidth; }
  void     setRwpSEIProjPictureHeight (int value)                                                       { m_rwpSEIProjPictureHeight = value; }
  int      getRwpSEIProjPictureHeight ()                                                                { return m_rwpSEIProjPictureHeight; }
  void     setRwpSEIPackedPictureWidth (int value)                                                      { m_rwpSEIPackedPictureWidth = value; }
  int      getRwpSEIPackedPictureWidth ()                                                               { return m_rwpSEIPackedPictureWidth; }
  void     setRwpSEIPackedPictureHeight (int value)                                                     { m_rwpSEIPackedPictureHeight = value; }
  int      getRwpSEIPackedPictureHeight ()                                                              { return m_rwpSEIPackedPictureHeight; }
  void     setRwpSEIRwpTransformType(const std::vector<uint8_t>& rwpTransformType)                          { m_rwpSEIRwpTransformType =rwpTransformType; }
  uint8_t  getRwpSEIRwpTransformType(uint32_t idx) const                                                    { return m_rwpSEIRwpTransformType[idx]; }
  void     setRwpSEIRwpGuardBandFlag(const std::vector<bool>& rwpGuardBandFlag)                             { m_rwpSEIRwpGuardBandFlag = rwpGuardBandFlag; }
  bool     getRwpSEIRwpGuardBandFlag(uint32_t idx) const                                                    { return m_rwpSEIRwpGuardBandFlag[idx]; }
  void     setRwpSEIProjRegionWidth(const std::vector<uint32_t>& projRegionWidth)                           { m_rwpSEIProjRegionWidth = projRegionWidth; }
  uint32_t getRwpSEIProjRegionWidth(uint32_t idx) const                                                     { return m_rwpSEIProjRegionWidth[idx]; }
  void     setRwpSEIProjRegionHeight(const std::vector<uint32_t>& projRegionHeight)                         { m_rwpSEIProjRegionHeight = projRegionHeight; }
  uint32_t getRwpSEIProjRegionHeight(uint32_t idx) const                                                    { return m_rwpSEIProjRegionHeight[idx]; }
  void     setRwpSEIRwpSEIProjRegionTop(const std::vector<uint32_t>& projRegionTop)                         { m_rwpSEIRwpSEIProjRegionTop = projRegionTop; }
  uint32_t getRwpSEIRwpSEIProjRegionTop(uint32_t idx) const                                                 { return m_rwpSEIRwpSEIProjRegionTop[idx]; }
  void     setRwpSEIProjRegionLeft(const std::vector<uint32_t>& projRegionLeft)                             { m_rwpSEIProjRegionLeft = projRegionLeft; }
  uint32_t getRwpSEIProjRegionLeft(uint32_t idx) const                                                      { return m_rwpSEIProjRegionLeft[idx]; }
  void     setRwpSEIPackedRegionWidth(const std::vector<uint16_t>& packedRegionWidth)                       { m_rwpSEIPackedRegionWidth  = packedRegionWidth; }
  uint16_t getRwpSEIPackedRegionWidth(uint32_t idx) const                                                   { return m_rwpSEIPackedRegionWidth[idx]; }
  void     setRwpSEIPackedRegionHeight(const std::vector<uint16_t>& packedRegionHeight)                     { m_rwpSEIPackedRegionHeight = packedRegionHeight; }
  uint16_t getRwpSEIPackedRegionHeight(uint32_t idx) const                                                  { return m_rwpSEIPackedRegionHeight[idx]; }
  void     setRwpSEIPackedRegionTop(const std::vector<uint16_t>& packedRegionTop)                           { m_rwpSEIPackedRegionTop = packedRegionTop; }
  uint16_t getRwpSEIPackedRegionTop(uint32_t idx) const                                                     { return m_rwpSEIPackedRegionTop[idx]; }
  void     setRwpSEIPackedRegionLeft(const std::vector<uint16_t>& packedRegionLeft)                         { m_rwpSEIPackedRegionLeft = packedRegionLeft; }
  uint16_t getRwpSEIPackedRegionLeft(uint32_t idx) const                                                    { return m_rwpSEIPackedRegionLeft[idx]; }
  void     setRwpSEIRwpLeftGuardBandWidth(const std::vector<uint8_t>& rwpLeftGuardBandWidth)                { m_rwpSEIRwpLeftGuardBandWidth = rwpLeftGuardBandWidth; }
  uint8_t  getRwpSEIRwpLeftGuardBandWidth(uint32_t idx) const                                               { return m_rwpSEIRwpLeftGuardBandWidth[idx]; }
  void     setRwpSEIRwpRightGuardBandWidth(const std::vector<uint8_t>& rwpRightGuardBandWidth)              { m_rwpSEIRwpRightGuardBandWidth = rwpRightGuardBandWidth; }
  uint8_t  getRwpSEIRwpRightGuardBandWidth(uint32_t idx) const                                              { return m_rwpSEIRwpRightGuardBandWidth[idx]; }
  void     setRwpSEIRwpTopGuardBandHeight(const std::vector<uint8_t>& rwpTopGuardBandHeight)                { m_rwpSEIRwpTopGuardBandHeight = rwpTopGuardBandHeight; }
  uint8_t  getRwpSEIRwpTopGuardBandHeight(uint32_t idx) const                                               { return m_rwpSEIRwpTopGuardBandHeight[idx]; }
  void     setRwpSEIRwpBottomGuardBandHeight(const std::vector<uint8_t>& rwpBottomGuardBandHeight)          { m_rwpSEIRwpBottomGuardBandHeight = rwpBottomGuardBandHeight; }
  uint8_t  getRwpSEIRwpBottomGuardBandHeight(uint32_t idx) const                                            { return m_rwpSEIRwpBottomGuardBandHeight[idx]; }
  void     setRwpSEIRwpGuardBandNotUsedForPredFlag(const std::vector<bool>& rwpGuardBandNotUsedForPredFlag) { m_rwpSEIRwpGuardBandNotUsedForPredFlag = rwpGuardBandNotUsedForPredFlag; }
  bool     getRwpSEIRwpGuardBandNotUsedForPredFlag(uint32_t idx) const                                      { return m_rwpSEIRwpGuardBandNotUsedForPredFlag[idx]; }
  void     setRwpSEIRwpGuardBandType(const std::vector<uint8_t>& rwpGuardBandType)                          { m_rwpSEIRwpGuardBandType = rwpGuardBandType; }
  uint8_t  getRwpSEIRwpGuardBandType(uint32_t idx) const                                                    { return m_rwpSEIRwpGuardBandType[idx]; }
  void    setGcmpSEIEnabled(bool b)                                                                 { m_gcmpSEIEnabled = b; }
  bool    getGcmpSEIEnabled()                                                                       { return m_gcmpSEIEnabled; }
  void    setGcmpSEICancelFlag(bool b)                                                              { m_gcmpSEICancelFlag = b; }
  bool    getGcmpSEICancelFlag()                                                                    { return m_gcmpSEICancelFlag; }
  void    setGcmpSEIPersistenceFlag(bool b)                                                         { m_gcmpSEIPersistenceFlag = b; }
  bool    getGcmpSEIPersistenceFlag()                                                               { return m_gcmpSEIPersistenceFlag; }
  void    setGcmpSEIPackingType(uint8_t u)                                                          { m_gcmpSEIPackingType = u; }
  uint8_t getGcmpSEIPackingType()                                                                   { return m_gcmpSEIPackingType; }
  void    setGcmpSEIMappingFunctionType(uint8_t u)                                                  { m_gcmpSEIMappingFunctionType = u; }
  uint8_t getGcmpSEIMappingFunctionType()                                                           { return m_gcmpSEIMappingFunctionType; }
  void    setGcmpSEIFaceIndex(const std::vector<uint8_t>& gcmpFaceIndex)                            { m_gcmpSEIFaceIndex = gcmpFaceIndex; }
  uint8_t getGcmpSEIFaceIndex(int idx) const                                                        { return m_gcmpSEIFaceIndex[idx]; }
  void    setGcmpSEIFaceRotation(const std::vector<uint8_t>& gcmpFaceRotation)                      { m_gcmpSEIFaceRotation = gcmpFaceRotation; }
  uint8_t getGcmpSEIFaceRotation(int idx) const                                                     { return m_gcmpSEIFaceRotation[idx]; }
  void    setGcmpSEIFunctionCoeffU(const std::vector<double>& gcmpFunctionCoeffU)                   { m_gcmpSEIFunctionCoeffU = gcmpFunctionCoeffU; }
  double  getGcmpSEIFunctionCoeffU(int idx) const                                                   { return m_gcmpSEIFunctionCoeffU[idx]; }
  void    setGcmpSEIFunctionUAffectedByVFlag(const std::vector<bool>& gcmpFunctionUAffectedByVFlag) { m_gcmpSEIFunctionUAffectedByVFlag = gcmpFunctionUAffectedByVFlag; }
  bool    getGcmpSEIFunctionUAffectedByVFlag(int idx) const                                         { return m_gcmpSEIFunctionUAffectedByVFlag[idx]; }
  void    setGcmpSEIFunctionCoeffV(const std::vector<double>& gcmpFunctionCoeffV)                   { m_gcmpSEIFunctionCoeffV = gcmpFunctionCoeffV; }
  double  getGcmpSEIFunctionCoeffV(int idx) const                                                   { return m_gcmpSEIFunctionCoeffV[idx]; }
  void    setGcmpSEIFunctionVAffectedByUFlag(const std::vector<bool>& gcmpFunctionVAffectedByUFlag) { m_gcmpSEIFunctionVAffectedByUFlag = gcmpFunctionVAffectedByUFlag; }
  bool    getGcmpSEIFunctionVAffectedByUFlag(int idx) const                                         { return m_gcmpSEIFunctionVAffectedByUFlag[idx]; }
  void    setGcmpSEIGuardBandFlag(bool b)                                                           { m_gcmpSEIGuardBandFlag = b; }
  bool    getGcmpSEIGuardBandFlag()                                                                 { return m_gcmpSEIGuardBandFlag; }
  void    setGcmpSEIGuardBandType(uint8_t u)                                                        { m_gcmpSEIGuardBandType = u; }
  uint8_t getGcmpSEIGuardBandType()                                                                 { return m_gcmpSEIGuardBandType; }
  void    setGcmpSEIGuardBandBoundaryExteriorFlag(bool b)                                           { m_gcmpSEIGuardBandBoundaryExteriorFlag = b; }
  bool    getGcmpSEIGuardBandBoundaryExteriorFlag()                                                 { return m_gcmpSEIGuardBandBoundaryExteriorFlag; }
  void    setGcmpSEIGuardBandSamplesMinus1( uint8_t u )                                             { m_gcmpSEIGuardBandSamplesMinus1 = u; }
  uint8_t getGcmpSEIGuardBandSamplesMinus1()                                                        { return m_gcmpSEIGuardBandSamplesMinus1; }
  const EncCfgParam::CfgSEISubpictureLevel &getSubpicureLevelInfoSEICfg() const
  {
    return m_cfgSubpictureLevelInfoSEI;
  }
  void setSubpicureLevelInfoSEICfg(const EncCfgParam::CfgSEISubpictureLevel &cfg)
  {
    m_cfgSubpictureLevelInfoSEI = cfg;
  }
#if JVET_AJ0151_DSC_SEI
  const EncCfgParam::CfgSEIDigitallySignedContent &getDigitallySignedContentSEICfg() const
  {
    return m_cfgDigitallySignedContentSEI;
  }
  void setDigitallySignedContentSEICfg(const EncCfgParam::CfgSEIDigitallySignedContent &cfg)
  {
    m_cfgDigitallySignedContentSEI = cfg;
  }
#endif
  bool     getSampleAspectRatioInfoSEIEnabled() const                                                       { return m_sampleAspectRatioInfoSEIEnabled; }
  void     setSampleAspectRatioInfoSEIEnabled(const bool val)                                               { m_sampleAspectRatioInfoSEIEnabled = val; }
  bool     getSariCancelFlag() const                                                                        { return m_sariCancelFlag; }
  void     setSariCancelFlag(const bool val)                                                                { m_sariCancelFlag = val; }
  bool     getSariPersistenceFlag() const                                                                   { return m_sariPersistenceFlag; }
  void     setSariPersistenceFlag(const bool val)                                                           { m_sariPersistenceFlag = val; }
  int      getSariAspectRatioIdc() const                                                                    { return m_sariAspectRatioIdc; }
  void     setSariAspectRatioIdc(const int val)                                                             { m_sariAspectRatioIdc = val; }
  int      getSariSarWidth() const                                                                          { return m_sariSarWidth; }
  void     setSariSarWidth(const int val)                                                                   { m_sariSarWidth = val; }
  int      getSariSarHeight() const                                                                         { return m_sariSarHeight; }
  void     setSariSarHeight(const int val)                                                                  { m_sariSarHeight = val; }
  bool     getPhaseIndicationSEIEnabledFullResolution() const                                               { return m_phaseIndicationSEIEnabledFullResolution; }
  void     setPhaseIndicationSEIEnabledFullResolution(const bool val)                                       { m_phaseIndicationSEIEnabledFullResolution = val; }
  int      getHorPhaseNumFullResolution() const                                                             { return m_horPhaseNumFullResolution; }
  void     setHorPhaseNumFullResolution(const int val)                                                      { m_horPhaseNumFullResolution = val; }
  int      getHorPhaseDenMinus1FullResolution() const                                                       { return m_horPhaseDenMinus1FullResolution; }
  void     setHorPhaseDenMinus1FullResolution(const int val)                                                { m_horPhaseDenMinus1FullResolution = val; }
  int      getVerPhaseNumFullResolution() const                                                             { return m_verPhaseNumFullResolution; }
  void     setVerPhaseNumFullResolution(const int   val)                                                    { m_verPhaseNumFullResolution = val; }
  int      getVerPhaseDenMinus1FullResolution() const                                                       { return m_verPhaseDenMinus1FullResolution; }
  void     setVerPhaseDenMinus1FullResolution(const int val)                                                { m_verPhaseDenMinus1FullResolution = val; }
  bool     getPhaseIndicationSEIEnabledReducedResolution() const                                            { return m_phaseIndicationSEIEnabledReducedResolution; }
  void     setPhaseIndicationSEIEnabledReducedResolution(const bool val)                                    { m_phaseIndicationSEIEnabledReducedResolution = val; }
  int      getHorPhaseNumReducedResolution() const                                                          { return m_horPhaseNumReducedResolution; }
  void     setHorPhaseNumReducedResolution(const int val)                                                   { m_horPhaseNumReducedResolution = val; }
  int      getHorPhaseDenMinus1ReducedResolution() const                                                    { return m_horPhaseDenMinus1ReducedResolution; }
  void     setHorPhaseDenMinus1ReducedResolution(const int val)                                             { m_horPhaseDenMinus1ReducedResolution = val; }
  int      getVerPhaseNumReducedResolution() const                                                          { return m_verPhaseNumReducedResolution; }
  void     setVerPhaseNumReducedResolution(const int   val)                                                 { m_verPhaseNumReducedResolution = val; }
  int      getVerPhaseDenMinus1ReducedResolution() const                                                    { return m_verPhaseDenMinus1ReducedResolution; }
  void     setVerPhaseDenMinus1ReducedResolution(const int val)                                             { m_verPhaseDenMinus1ReducedResolution = val; }
  void  setMCTSEncConstraint(bool b)                                 { m_MCTSEncConstraint = b; }
  bool  getMCTSEncConstraint()                                       { return m_MCTSEncConstraint; }
  void  setMasteringDisplaySEI(const SEIMasteringDisplay &src)       { m_masteringDisplay = src; }
  void  setSEIAlternativeTransferCharacteristicsSEIEnable( bool b)   { m_alternativeTransferCharacteristicsSEIEnabled = b;    }
  bool  getSEIAlternativeTransferCharacteristicsSEIEnable( ) const   { return m_alternativeTransferCharacteristicsSEIEnabled; }
  void  setSEIPreferredTransferCharacteristics(uint8_t v)              { m_preferredTransferCharacteristics = v;    }
  uint8_t getSEIPreferredTransferCharacteristics() const               { return m_preferredTransferCharacteristics; }
  const SEIMasteringDisplay &getMasteringDisplaySEI() const          { return m_masteringDisplay; }
  void setEOISEIEnabled(bool enabledFlag) { m_eoiSEIEnabled = enabledFlag; }
  bool getEOISEIEnabled() const { return m_eoiSEIEnabled; }
  void setEOISEICancelFlag(bool cancelFlag) { m_eoiSEICancelFlag = cancelFlag; }
  bool getEOISEICancelFlag() const { return m_eoiSEICancelFlag; }
  void setEOISEIPersistenceFlag(bool persistenceFlag) { m_eoiSEIPersistenceFlag = persistenceFlag; }
  bool getEOISEIPersistenceFlag() const { return m_eoiSEIPersistenceFlag; }
  void setEOISEIForHumanViewingIdc(uint32_t forHumanViewingIdc) { m_eoiSEIForHumanViewingIdc = forHumanViewingIdc; }
  uint32_t getEOISEIForHumanViewingIdc() const { return m_eoiSEIForHumanViewingIdc; }
  void setEOISEIForMachineAnalysisIdc(uint32_t forMachineAnalysisIdc) { m_eoiSEIForMachineAnalysisIdc = forMachineAnalysisIdc; }
  uint32_t getEOISEIForMachineAnalysisIdc() const { return m_eoiSEIForMachineAnalysisIdc; }
  void setEOISEIType(uint32_t eoiType) { m_eoiSEIType = eoiType; }
  uint32_t getEOISEIType() const { return m_eoiSEIType; }
  void setEOISEIObjectBasedIdc(uint32_t objectBasedIdc) { m_eoiSEIObjectBasedIdc = objectBasedIdc; }
  uint32_t getEOISEIObjectBasedIdc() const { return m_eoiSEIObjectBasedIdc; }
  void setEOISEIQuantThresholdDelta(uint32_t quantThresholdDelta) { m_eoiSEIQuantThresholdDelta = quantThresholdDelta; }
  uint32_t getEOISEIQuantThresholdDelta() const { return m_eoiSEIQuantThresholdDelta; }
  void setEOISEIPicQuantObjectFlag(bool picQuantObjectFlag) { m_eoiSEIPicQuantObjectFlag = picQuantObjectFlag; }
  bool getEOISEIPicQuantObjectFlag() const { return m_eoiSEIPicQuantObjectFlag; }
  void setEOISEITemporalResamplingTypeFlag(bool temporalResamplingTypeFlag) { m_eoiSEITemporalResamplingTypeFlag = temporalResamplingTypeFlag; }
  bool getEOISEITemporalResamplingTypeFlag() const { return m_eoiSEITemporalResamplingTypeFlag; }
#if JVET_AJ0183_EOI_SEI_SRC_PIC_FLAG
  void setEOISEISrcPicFlag(bool srcPicFlag) { m_eoiSEISrcPicFlag = srcPicFlag; }
  bool getEOISEISrcPicFlag() const { return m_eoiSEISrcPicFlag; }
#endif
  void setEOISEINumIntPics(uint32_t numIntPics) { m_eoiSEINumIntPics = numIntPics; }
  uint32_t getEOISEINumIntPics() const { return m_eoiSEINumIntPics; }
  void setEOISEIOrigPicDimensionsFlag(bool origPicDimensionsFlag) { m_eoiSEIOrigPicDimensionsFlag = origPicDimensionsFlag; }
  bool getEOISEIOrigPicDimensionsFlag() { return m_eoiSEIOrigPicDimensionsFlag; }
#if JVET_AL0123_AL0310_EOI
  void setEOISEIOrigPicWidthMinus1(uint32_t origPicWidthMinus1) { m_eoiSEIOrigPicWidthMinus1 = origPicWidthMinus1; }
  uint32_t getEOISEIOrigPicWidthMinus1() { return m_eoiSEIOrigPicWidthMinus1; }
  void     setEOISEIOrigPicHeightMinus1(uint32_t origPicHeightMinus1) { m_eoiSEIOrigPicHeightMinus1 = origPicHeightMinus1; }
  uint32_t getEOISEIOrigPicHeightMinus1() { return m_eoiSEIOrigPicHeightMinus1; }
  void setEOISEISpatialHorResamplingTypeIdc(uint32_t spatialHorResamplingTypeIdc) { m_eoiSEISpatialHorResamplingTypeIdc = spatialHorResamplingTypeIdc; }
  uint32_t getEOISEISpatialHorResamplingTypeIdc() const { return m_eoiSEISpatialHorResamplingTypeIdc; }
  void setEOISEISpatialVerResamplingTypeIdc(uint32_t spatialVerResamplingTypeIdc) { m_eoiSEISpatialVerResamplingTypeIdc = spatialVerResamplingTypeIdc; }
  uint32_t getEOISEISpatialVerResamplingTypeIdc() const { return m_eoiSEISpatialVerResamplingTypeIdc; }
#else
  void setEOISEIOrigPicWidth(uint32_t origPicWidth) { m_eoiSEIOrigPicWidth = origPicWidth; }
  uint32_t getEOISEIOrigPicWidth() { return m_eoiSEIOrigPicWidth; }
  void     setEOISEIOrigPicHeight(uint32_t origPicHeight) { m_eoiSEIOrigPicHeight = origPicHeight; }
  uint32_t getEOISEIOrigPicHeight() { return m_eoiSEIOrigPicHeight; }
  void setEOISEISpatialResamplingTypeFlag(bool spatialResamplingTypeFlag) { m_eoiSEISpatialResamplingTypeFlag = spatialResamplingTypeFlag; }
  bool getEOISEISpatialResamplingTypeFlag() const { return m_eoiSEISpatialResamplingTypeFlag; }
#endif
  void setEOISEIPrivacyProtectionTypeIdc(uint32_t privacyProtectionTypeIdc) { m_eoiSEIPrivacyProtectionTypeIdc = privacyProtectionTypeIdc; }
  uint32_t getEOISEIPrivacyProtectionTypeIdc() const { return m_eoiSEIPrivacyProtectionTypeIdc; }
  void setEOISEIPrivacyProtectedInfoType(uint32_t privacyProtectedInfoType) { m_eoiSEIPrivacyProtectedInfoType = privacyProtectedInfoType; }
  uint32_t getEOISEIPrivacyProtectedInfoType() const { return m_eoiSEIPrivacyProtectedInfoType; }

  //Modality Information SEI 
  void     setMiSEIEnabled(bool b) { m_miSEIEnabled = b; }
  bool     getMiSEIEnabled()                                                                              { return m_miSEIEnabled; }
  void     setMiCancelFlag(const bool val) { m_miCancelFlag = val; }
  bool     getMiCancelFlag() const                                                                        { return m_miCancelFlag; }
  void     setMiPersistenceFlag(const bool val) { m_miPersistenceFlag = val; }
  bool     getMiPersistenceFlag() const                                                                   { return m_miPersistenceFlag; }
  void     setMiModalityType(const int val) { m_miModalityType = val; }
  int      getMiModalityType() const                                                                      { return m_miModalityType; }
  void     setMiSpectrumRangePresentFlag(const bool val) { m_miSpectrumRangePresentFlag = val; }
  bool     getMiSpectrumRangePresentFlag() const                                                          { return m_miSpectrumRangePresentFlag; }
  void     setMiMinWavelengthMantissa(const int val) { m_miMinWavelengthMantissa = val; }
  int      getMiMinWavelengthMantissa() const                                                             { return m_miMinWavelengthMantissa; }
  void     setMiMinWavelengthExponentPlus15(const int val) { m_miMinWavelengthExponentPlus15 = val; }
  int      getMiMinWavelengthExponentPlus15() const                                                       { return m_miMinWavelengthExponentPlus15; }
  void     setMiMaxWavelengthMantissa(const int val) { m_miMaxWavelengthMantissa = val; }
  int      getMiMaxWavelengthMantissa() const                                                             { return m_miMaxWavelengthMantissa; }
  void     setMiMaxWavelengthExponentPlus15(const int val) { m_miMaxWavelengthExponentPlus15 = val; }
  int      getMiMaxWavelengthExponentPlus15() const                                                       { return m_miMaxWavelengthExponentPlus15; }

  // film grain SEI
  void  setFilmGrainCharactersticsSEIEnabled (bool b)                { m_fgcSEIEnabled = b; }
  bool  getFilmGrainCharactersticsSEIEnabled()                       { return m_fgcSEIEnabled; }
  void  setFilmGrainCharactersticsSEICancelFlag(bool b)              { m_fgcSEICancelFlag = b; }
  bool  getFilmGrainCharactersticsSEICancelFlag()                    { return m_fgcSEICancelFlag; }
  void  setFilmGrainCharactersticsSEIPersistenceFlag(bool b)         { m_fgcSEIPersistenceFlag = b; }
  bool  getFilmGrainCharactersticsSEIPersistenceFlag()               { return m_fgcSEIPersistenceFlag; }
  void  setFilmGrainCharactersticsSEIModelID(uint8_t v )             { m_fgcSEIModelID = v; }
  uint8_t getFilmGrainCharactersticsSEIModelID()                     { return m_fgcSEIModelID; }
  void  setFilmGrainCharactersticsSEISepColourDescPresent(bool b)    { m_fgcSEISepColourDescPresentFlag = b; }
  bool  getFilmGrainCharactersticsSEISepColourDescPresent()          { return m_fgcSEISepColourDescPresentFlag; }
  void  setFilmGrainCharactersticsSEIBlendingModeID(uint8_t v )      { m_fgcSEIBlendingModeID = v; }
  uint8_t getFilmGrainCharactersticsSEIBlendingModeID()              { return m_fgcSEIBlendingModeID; }
  void  setFilmGrainCharactersticsSEILog2ScaleFactor(uint8_t v )     { m_fgcSEILog2ScaleFactor = v; }
  uint8_t getFilmGrainCharactersticsSEILog2ScaleFactor()             { return m_fgcSEILog2ScaleFactor; }
  void  setFGCSEICompModelPresent(bool b, int index)                 { m_fgcSEICompModelPresent[index] = b; }
  bool  getFGCSEICompModelPresent(int index)                         { return m_fgcSEICompModelPresent[index]; }
  bool*     getFGCSEICompModelPresent                 ()                        { return m_fgcSEICompModelPresent; }
  void      setFilmGrainAnalysisEnabled               (bool b)                  { m_fgcSEIAnalysisEnabled = b; }
  bool      getFilmGrainAnalysisEnabled               ()                        { return m_fgcSEIAnalysisEnabled; }
  void        setFilmGrainExternalMask(std::string s) { m_fgcSEIExternalMask = s; }
  void        setFilmGrainExternalDenoised(std::string s) { m_fgcSEIExternalDenoised = s; }
  std::string getFilmGrainExternalMask() { return m_fgcSEIExternalMask; }
  std::string getFilmGrainExternalDenoised() { return m_fgcSEIExternalDenoised; }
  void        setFilmGrainTemporalFilterPastRefs(int v)                         { m_fgcSEITemporalFilterPastRefs = v; }
  void        setFilmGrainTemporalFilterFutureRefs(int v)                       { m_fgcSEITemporalFilterFutureRefs = v; }
  void        setFilmGrainTemporalFilterStrengths(std::map<int, double> v)      { m_fgcSEITemporalFilterStrengths = v; }
  int         getFilmGrainTemporalFilterPastRefs()                              { return m_fgcSEITemporalFilterPastRefs; };
  int         getFilmGrainTemporalFilterFutureRef()                             { return m_fgcSEITemporalFilterFutureRefs; };
  std::map<int, double> getFilmGrainTemporalFilterStrengths()                   { return m_fgcSEITemporalFilterStrengths; };
  void      setFilmGrainCharactersticsSEIPerPictureSEI(bool b)                  { m_fgcSEIPerPictureSEI = b; }
  bool      getFilmGrainCharactersticsSEIPerPictureSEI()                        { return m_fgcSEIPerPictureSEI; }
  void      setFGCSEINumIntensityIntervalMinus1 (uint8_t v, int index)          { m_fgcSEINumIntensityIntervalMinus1[index] = v; }
  uint8_t   getFGCSEINumIntensityIntervalMinus1 (int index)                     { return m_fgcSEINumIntensityIntervalMinus1[index]; }
  void      setFGCSEINumModelValuesMinus1       (uint8_t v, int index)          { m_fgcSEINumModelValuesMinus1[index] = v; }
  uint8_t   getFGCSEINumModelValuesMinus1       (int index)                     { return m_fgcSEINumModelValuesMinus1[index]; }
  void      setFGCSEIIntensityIntervalLowerBound(uint8_t v, int index, int ctr) { m_fgcSEIIntensityIntervalLowerBound[index][ctr] = v; }
  uint8_t   getFGCSEIIntensityIntervalLowerBound(int index, int ctr)            { return m_fgcSEIIntensityIntervalLowerBound[index][ctr]; }
  void      setFGCSEIIntensityIntervalUpperBound(uint8_t v, int index, int ctr) { m_fgcSEIIntensityIntervalUpperBound[index][ctr] = v; }
  uint8_t   getFGCSEIIntensityIntervalUpperBound(int index, int ctr)            { return m_fgcSEIIntensityIntervalUpperBound[index][ctr]; }
  void      setFGCSEICompModelValue             (uint32_t v, int index, int ctr, int modelCtr)  { m_fgcSEICompModelValue[index][ctr][modelCtr] = v; }
  uint32_t  getFGCSEICompModelValue             (int index, int ctr, int modelCtr)              { return m_fgcSEICompModelValue[index][ctr][modelCtr]; }
#if JVET_AL0339_FGS_SEI_SPATIAL_RESOLUTION
  void      setFGCSEISpatialResolutionPresentFlag(bool b)                       { m_fgcSEISpatialResolutionPresentFlag = b; }
  bool      getFGCSEISpatialResolutionPresentFlag()                             { return m_fgcSEISpatialResolutionPresentFlag; }
  void      setFGCSEIPicWidthInLumaSamples(int w)                               { m_fgcSEIPicWidthInLumaSamples = w; }
  int       getFGCSEIPicWidthInLumaSamples()                                    { return m_fgcSEIPicWidthInLumaSamples; }
  void      setFGCSEIPicHeightInLumaSamples(int w)                              { m_fgcSEIPicHeightInLumaSamples = w; }
  int       getFGCSEIPicHeightInLumaSamples()                                   { return m_fgcSEIPicHeightInLumaSamples; }
#endif
  // cll SEI
  void  setCLLSEIEnabled(bool b)                                     { m_cllSEIEnabled = b; }
  bool  getCLLSEIEnabled()                                           { return m_cllSEIEnabled; }
  void  setCLLSEIMaxContentLightLevel (uint16_t v)                   { m_cllSEIMaxContentLevel = v; }
  uint16_t getCLLSEIMaxContentLightLevel()                           { return m_cllSEIMaxContentLevel; }
  void  setCLLSEIMaxPicAvgLightLevel(uint16_t v)                     { m_cllSEIMaxPicAvgLevel = v; }
  uint16_t getCLLSEIMaxPicAvgLightLevel()                            { return m_cllSEIMaxPicAvgLevel; }
  // ave SEI
  void  setAmbientViewingEnvironmentSEIEnabled (bool b)              { m_aveSEIEnabled = b; }
  bool  getAmbientViewingEnvironmentSEIEnabled ()                    { return m_aveSEIEnabled; }
  void  setAmbientViewingEnvironmentSEIIlluminance( uint32_t v )     { m_aveSEIAmbientIlluminance = v; }
  uint32_t getAmbientViewingEnvironmentSEIIlluminance()              { return m_aveSEIAmbientIlluminance; }
  void  setAmbientViewingEnvironmentSEIAmbientLightX( uint16_t v )   { m_aveSEIAmbientLightX = v; }
  uint16_t getAmbientViewingEnvironmentSEIAmbientLightX()            { return m_aveSEIAmbientLightX; }
  void  setAmbientViewingEnvironmentSEIAmbientLightY( uint16_t v )   { m_aveSEIAmbientLightY = v; }
  uint16_t getAmbientViewingEnvironmentSEIAmbientLightY()            { return m_aveSEIAmbientLightY; }
  // colour tranform information sei
  void      setCtiSEIEnabled(bool b) { m_ctiSEIEnabled = b; }
  bool      getCtiSEIEnabled() { return m_ctiSEIEnabled; }
  void      setCtiSEIId(uint32_t b) { m_ctiSEIId = b; }
  uint32_t  getCtiSEIId() { return m_ctiSEIId; }
  void      setCtiSEISignalInfoFlag(bool b) { m_ctiSEISignalInfoFlag = b; }
  bool      getCtiSEISignalInfoFlag() { return m_ctiSEISignalInfoFlag; }
  void      setCtiSEIFullRangeFlag(bool b) { m_ctiSEIFullRangeFlag = b; }
  bool      getCtiSEIFullRangeFlag() { return m_ctiSEIFullRangeFlag; }
  uint32_t  getCtiSEIPrimaries() { return m_ctiSEIPrimaries; }
  void      setCtiSEIPrimaries(uint32_t v) { m_ctiSEIPrimaries = v; }
  uint32_t  getCtiSEITransferFunction() { return m_ctiSEITransferFunction; }
  void      setCtiSEITransferFunction(uint32_t v) { m_ctiSEITransferFunction = v; }
  uint32_t  getCtiSEIMatrixCoefs() { return m_ctiSEIMatrixCoefs; }
  void      setCtiSEIMatrixCoefs(uint32_t v) { m_ctiSEIMatrixCoefs = v; }
  void      setCtiSEICrossComponentFlag(bool b) { m_ctiSEICrossComponentFlag = b; }
  bool      getCtiSEICrossComponentFlag() { return m_ctiSEICrossComponentFlag; }
  void      setCtiSEICrossComponentInferred(bool b) { m_ctiSEICrossComponentInferred = b; }
  bool      getCtiSEICrossComponentInferred() { return m_ctiSEICrossComponentInferred; }
  uint32_t  getCtiSEINbChromaLut() { return m_ctiSEINumberChromaLut; }
  void      setCtiSEINbChromaLut(uint32_t v) { m_ctiSEINumberChromaLut = v; }
  int       getCtiSEIChromaOffset() { return m_ctiSEIChromaOffset; }
  void      setCtiSEIChromaOffset(int v) { m_ctiSEIChromaOffset = v; }
  const LutModel&  getCtiSEILut(int idx) const { return m_ctiSEILut[idx]; }
  void      setCtiSEILut(LutModel& cmp, int idx) { m_ctiSEILut[idx] = cmp; }
  // ccv SEI
  void     setCcvSEIEnabled(bool b)                                  { m_ccvSEIEnabled = b; }
  bool     getCcvSEIEnabled()                                        { return m_ccvSEIEnabled; }
  void     setCcvSEICancelFlag(bool b)                               { m_ccvSEICancelFlag = b; }
  bool     getCcvSEICancelFlag()                                     { return m_ccvSEICancelFlag; }
  void     setCcvSEIPersistenceFlag(bool b)                          { m_ccvSEIPersistenceFlag = b; }
  bool     getCcvSEIPersistenceFlag()                                { return m_ccvSEIPersistenceFlag; }
  void     setCcvSEIPrimariesPresentFlag(bool b)                     { m_ccvSEIPrimariesPresentFlag = b; }
  bool     getCcvSEIPrimariesPresentFlag()                           { return m_ccvSEIPrimariesPresentFlag; }
  void     setCcvSEIMinLuminanceValuePresentFlag(bool b)             { m_ccvSEIMinLuminanceValuePresentFlag = b; }
  bool     getCcvSEIMinLuminanceValuePresentFlag()                   { return m_ccvSEIMinLuminanceValuePresentFlag; }
  void     setCcvSEIMaxLuminanceValuePresentFlag(bool b)             { m_ccvSEIMaxLuminanceValuePresentFlag = b; }
  bool     getCcvSEIMaxLuminanceValuePresentFlag()                   { return m_ccvSEIMaxLuminanceValuePresentFlag; }
  void     setCcvSEIAvgLuminanceValuePresentFlag(bool b)             { m_ccvSEIAvgLuminanceValuePresentFlag = b; }
  bool     getCcvSEIAvgLuminanceValuePresentFlag()                   { return m_ccvSEIAvgLuminanceValuePresentFlag; }
  void     setCcvSEIPrimariesX(double dValue, int index)             { m_ccvSEIPrimariesX[index] = dValue; }
  double   getCcvSEIPrimariesX(int index)                            { return m_ccvSEIPrimariesX[index]; }
  void     setCcvSEIPrimariesY(double dValue, int index)             { m_ccvSEIPrimariesY[index] = dValue; }
  double   getCcvSEIPrimariesY(int index)                            { return m_ccvSEIPrimariesY[index]; }
  void     setCcvSEIMinLuminanceValue  (double dValue)               { m_ccvSEIMinLuminanceValue = dValue; }
  double   getCcvSEIMinLuminanceValue  ()                            { return m_ccvSEIMinLuminanceValue;  }
  void     setCcvSEIMaxLuminanceValue  (double dValue)               { m_ccvSEIMaxLuminanceValue = dValue; }
  double   getCcvSEIMaxLuminanceValue  ()                            { return m_ccvSEIMaxLuminanceValue;  }
  void     setCcvSEIAvgLuminanceValue  (double dValue)               { m_ccvSEIAvgLuminanceValue = dValue; }
  double   getCcvSEIAvgLuminanceValue  ()                            { return m_ccvSEIAvgLuminanceValue;  }
  // scalability dimension information SEI
  void     setSdiSEIEnabled(bool b)                                  { m_sdiSEIEnabled = b; }
  bool     getSdiSEIEnabled() const                                  { return m_sdiSEIEnabled; }
  void     setSdiSEIMaxLayersMinus1(int i)                           { m_sdiSEIMaxLayersMinus1 = i; }
  int      getSdiSEIMaxLayersMinus1() const                          { return m_sdiSEIMaxLayersMinus1; }
  void     setSdiSEIMultiviewInfoFlag(bool b)                        { m_sdiSEIMultiviewInfoFlag = b; }
  bool     getSdiSEIMultiviewInfoFlag() const                        { return m_sdiSEIMultiviewInfoFlag; }
  void     setSdiSEIAuxiliaryInfoFlag(bool b)                        { m_sdiSEIAuxiliaryInfoFlag = b; }
  bool     getSdiSEIAuxiliaryInfoFlag() const                        { return m_sdiSEIAuxiliaryInfoFlag; }
  void     setSdiSEIViewIdLenMinus1(int i)                           { m_sdiSEIViewIdLenMinus1 = i; }
  int      getSdiSEIViewIdLenMinus1() const                          { return m_sdiSEIViewIdLenMinus1; }
  void     setSdiSEILayerId(const std::vector<uint32_t>& sdiSEILayerId)   { m_sdiSEILayerId = sdiSEILayerId; }
  uint32_t getSdiSEILayerId(int idx) const                           { return m_sdiSEILayerId[idx]; }
  void     setSdiSEIViewIdVal(const std::vector<uint32_t>& sdiSEIViewIdVal)   { m_sdiSEIViewIdVal = sdiSEIViewIdVal; }
  uint32_t getSdiSEIViewIdVal(int idx) const                         { return m_sdiSEIViewIdVal[idx]; }
  void     setSdiSEIAuxId(const std::vector<uint32_t>& sdiSEIAuxId)       { m_sdiSEIAuxId = sdiSEIAuxId; }
  uint32_t getSdiSEIAuxId(int idx) const                             { return m_sdiSEIAuxId[idx]; }
  void     setSdiSEINumAssociatedPrimaryLayersMinus1(const std::vector<uint32_t>& sdiSEINumAssociatedPrimaryLayersMinus1)   { m_sdiSEINumAssociatedPrimaryLayersMinus1 = sdiSEINumAssociatedPrimaryLayersMinus1; }
  uint32_t getSdiSEINumAssociatedPrimaryLayersMinus1(int idx) const  { return m_sdiSEINumAssociatedPrimaryLayersMinus1[idx]; }
  void     setSdiSEIAssociatedPrimaryLayerIdx(const std::vector<uint32_t>& sdiSEIAssociatedPrimaryLayerIdx) { m_sdiSEIAssociatedPrimaryLayerIdx = sdiSEIAssociatedPrimaryLayerIdx; }
  uint32_t getSdiSEIAssociatedPrimaryLayerIdx(int idx) const         { return m_sdiSEIAssociatedPrimaryLayerIdx[idx]; }
  // multiview acquisition information SEI
  void     setMaiSEIEnabled(bool b)                                  { m_maiSEIEnabled = b; }
  bool     getMaiSEIEnabled() const                                  { return m_maiSEIEnabled; }
  void     setMaiSEIIntrinsicParamFlag(bool b)                       { m_maiSEIIntrinsicParamFlag = b; }
  bool     getMaiSEIIntrinsicParamFlag() const                       { return m_maiSEIIntrinsicParamFlag; }
  void     setMaiSEIExtrinsicParamFlag(bool b)                       { m_maiSEIExtrinsicParamFlag = b; }
  bool     getMaiSEIExtrinsicParamFlag() const                       { return m_maiSEIExtrinsicParamFlag; }
  void     setMaiSEINumViewsMinus1(int i)                            { m_maiSEINumViewsMinus1 = i; }
  int      getMaiSEINumViewsMinus1() const                           { return m_maiSEINumViewsMinus1; }
  void     setMaiSEIIntrinsicParamsEqualFlag(bool b)                 { m_maiSEIIntrinsicParamsEqualFlag = b; }
  bool     getMaiSEIIntrinsicParamsEqualFlag() const                 { return m_maiSEIIntrinsicParamsEqualFlag; }
  void     setMaiSEIPrecFocalLength(int i)                           { m_maiSEIPrecFocalLength= i; }
  int      getMaiSEIPrecFocalLength() const                          { return m_maiSEIPrecFocalLength; }
  void     setMaiSEIPrecPrincipalPoint(int i)                        { m_maiSEIPrecPrincipalPoint = i; }
  int      getMaiSEIPrecPrincipalPoint() const                       { return m_maiSEIPrecPrincipalPoint; }
  void     setMaiSEIPrecSkewFactor(int i)                            { m_maiSEIPrecSkewFactor = i; }
  int      getMaiSEIPrecSkewFactor() const                           { return m_maiSEIPrecSkewFactor; }
  void     setMaiSEISignFocalLengthX(const std::vector<bool>& maiSEISignFocalLengthX) { m_maiSEISignFocalLengthX = maiSEISignFocalLengthX; }
  bool     getMaiSEISignFocalLengthX(int idx) const                  { return m_maiSEISignFocalLengthX[idx]; }
  void     setMaiSEIExponentFocalLengthX(const std::vector<uint32_t>& maiSEIExponentFocalLengthX) { m_maiSEIExponentFocalLengthX = maiSEIExponentFocalLengthX; }
  uint32_t      getMaiSEIExponentFocalLengthX(int idx) const              { return m_maiSEIExponentFocalLengthX[idx]; }
  void     setMaiSEIMantissaFocalLengthX(const std::vector<uint32_t>& maiSEIMantissaFocalLengthX) { m_maiSEIMantissaFocalLengthX = maiSEIMantissaFocalLengthX; }
  uint32_t      getMaiSEIMantissaFocalLengthX(int idx) const              { return m_maiSEIMantissaFocalLengthX[idx]; }
  void     setMaiSEISignFocalLengthY(const std::vector<bool>& maiSEISignFocalLengthY) { m_maiSEISignFocalLengthY = maiSEISignFocalLengthY; }
  bool     getMaiSEISignFocalLengthY(int idx) const                  { return m_maiSEISignFocalLengthY[idx]; }
  void     setMaiSEIExponentFocalLengthY(const std::vector<uint32_t>& maiSEIExponentFocalLengthY) { m_maiSEIExponentFocalLengthY = maiSEIExponentFocalLengthY; }
  uint32_t      getMaiSEIExponentFocalLengthY(int idx) const              { return m_maiSEIExponentFocalLengthY[idx]; }
  void     setMaiSEIMantissaFocalLengthY(const std::vector<uint32_t>& maiSEIMantissaFocalLengthY) { m_maiSEIMantissaFocalLengthY = maiSEIMantissaFocalLengthY; }
  uint32_t      getMaiSEIMantissaFocalLengthY(int idx) const              { return m_maiSEIMantissaFocalLengthY[idx]; }
  void     setMaiSEISignPrincipalPointX(const std::vector<bool>& maiSEISignPrincipalPointX) { m_maiSEISignPrincipalPointX = maiSEISignPrincipalPointX; }
  bool     getMaiSEISignPrincipalPointX(int idx) const               { return m_maiSEISignPrincipalPointX[idx]; }
  void     setMaiSEIExponentPrincipalPointX(const std::vector<uint32_t>& maiSEIExponentPrincipalPointX) { m_maiSEIExponentPrincipalPointX = maiSEIExponentPrincipalPointX; }
  uint32_t      getMaiSEIExponentPrincipalPointX(int idx) const           { return m_maiSEIExponentPrincipalPointX[idx]; }
  void     setMaiSEIMantissaPrincipalPointX(const std::vector<uint32_t>& maiSEIMantissaPrincipalPointX) { m_maiSEIMantissaPrincipalPointX = maiSEIMantissaPrincipalPointX; }
  uint32_t      getMaiSEIMantissaPrincipalPointX(int idx) const           { return m_maiSEIMantissaPrincipalPointX[idx]; }
  void     setMaiSEISignPrincipalPointY(const std::vector<bool>& maiSEISignPrincipalPointY) { m_maiSEISignPrincipalPointY = maiSEISignPrincipalPointY; }
  bool     getMaiSEISignPrincipalPointY(int idx) const               { return m_maiSEISignPrincipalPointY[idx]; }
  void     setMaiSEIExponentPrincipalPointY(const std::vector<uint32_t>& maiSEIExponentPrincipalPointY) { m_maiSEIExponentPrincipalPointY = maiSEIExponentPrincipalPointY; }
  uint32_t      getMaiSEIExponentPrincipalPointY(int idx) const           { return m_maiSEIExponentPrincipalPointY[idx]; }
  void     setMaiSEIMantissaPrincipalPointY(const std::vector<uint32_t>& maiSEIMantissaPrincipalPointY) { m_maiSEIMantissaPrincipalPointY = maiSEIMantissaPrincipalPointY; }
  uint32_t      getMaiSEIMantissaPrincipalPointY(int idx) const           { return m_maiSEIMantissaPrincipalPointY[idx]; }
  void     setMaiSEISignSkewFactor(const std::vector<bool>& maiSEISignSkewFactor) { m_maiSEISignSkewFactor = maiSEISignSkewFactor; }
  bool     getMaiSEISignSkewFactor(int idx) const                    { return m_maiSEISignSkewFactor[idx]; }
  void     setMaiSEIExponentSkewFactor(const std::vector<uint32_t>& maiSEIExponentSkewFactor) { m_maiSEIExponentSkewFactor = maiSEIExponentSkewFactor; }
  uint32_t      getMaiSEIExponentSkewFactor(int idx) const                { return m_maiSEIExponentSkewFactor[idx]; }
  void     setMaiSEIMantissaSkewFactor(const std::vector<uint32_t>& maiSEIMantissaSkewFactor) { m_maiSEIMantissaSkewFactor = maiSEIMantissaSkewFactor; }
  uint32_t      getMaiSEIMantissaSkewFactor(int idx) const                { return m_maiSEIMantissaSkewFactor[idx]; }
  void     setMaiSEIPrecRotationParam(int i)                         { m_maiSEIPrecRotationParam = i; }
  int      getMaiSEIPrecRotationParam() const                        { return m_maiSEIPrecRotationParam; }
  void     setMaiSEIPrecTranslationParam(int i)                      { m_maiSEIPrecTranslationParam = i; }
  int      getMaiSEIPrecTranslationParam() const                     { return m_maiSEIPrecTranslationParam; }
  // multiview view position SEI
  void     setMvpSEIEnabled(bool b) { m_mvpSEIEnabled = b; }
  bool     getMvpSEIEnabled() const { return m_mvpSEIEnabled; }
  void     setMvpSEINumViewsMinus1(int i) { m_mvpSEINumViewsMinus1 = i; }
  int      getMvpSEINumViewsMinus1() const { return m_mvpSEINumViewsMinus1; }
  void     setMvpSEIViewPosition(const std::vector<uint32_t>& mvpSEIViewPosition) { m_mvpSEIViewPosition = mvpSEIViewPosition; }
  uint32_t      getMvpSEIViewPosition(int idx) const { return m_mvpSEIViewPosition[idx]; }
  // alpha channel information SEI
  void     setAciSEIEnabled(bool b)                                  { m_aciSEIEnabled = b; }
  bool     getAciSEIEnabled() const                                  { return m_aciSEIEnabled; }
  void     setAciSEICancelFlag(bool b)                               { m_aciSEICancelFlag = b; }
  bool     getAciSEICancelFlag() const                               { return m_aciSEICancelFlag; }
  void     setAciSEIUseIdc(int value)                                { m_aciSEIUseIdc = value; }
  int      getAciSEIUseIdc() const                                   { return m_aciSEIUseIdc; }
  void     setAciSEIBitDepthMinus8(int value)                        { m_aciSEIBitDepthMinus8 = value; }
  int      getAciSEIBitDepthMinus8() const                           { return m_aciSEIBitDepthMinus8; }
  void     setAciSEITransparentValue(int value)                      { m_aciSEITransparentValue = value; }
  int      getAciSEITransparentValue() const                         { return m_aciSEITransparentValue; }
  void     setAciSEIOpaqueValue(int value)                           { m_aciSEIOpaqueValue = value; }
  int      getAciSEIOpaqueValue() const                              { return m_aciSEIOpaqueValue; }
  void     setAciSEIIncrFlag(bool b)                                 { m_aciSEIIncrFlag = b; }
  bool     getAciSEIIncrFlag() const                                 { return m_aciSEIIncrFlag; }
  void     setAciSEIClipFlag(bool b)                                 { m_aciSEIClipFlag = b; }
  bool     getAciSEIClipFlag() const                                 { return m_aciSEIClipFlag; }
  void     setAciSEIClipTypeFlag(bool b)                             { m_aciSEIClipTypeFlag = b; }
  bool     getAciSEIClipTypeFlag() const                             { return m_aciSEIClipTypeFlag; }
  // depth representation information SEI
  void     setDriSEIEnabled(bool b)                                  { m_driSEIEnabled = b; }
  bool     getDriSEIEnabled() const                                  { return m_driSEIEnabled; }
  void     setDriSEIZNearFlag(bool b)                                { m_driSEIZNearFlag = b; }
  bool     getDriSEIZNearFlag() const                                { return m_driSEIZNearFlag; }
  void     setDriSEIZFarFlag(bool b)                                 { m_driSEIZFarFlag = b; }
  bool     getDriSEIZFarFlag() const                                 { return m_driSEIZFarFlag; }
  void     setDriSEIDMinFlag(bool b)                                 { m_driSEIDMinFlag = b; }
  bool     getDriSEIDMinFlag() const                                 { return m_driSEIDMinFlag; }
  void     setDriSEIDMaxFlag(bool b)                                 { m_driSEIDMaxFlag = b; }
  bool     getDriSEIDMaxFlag() const                                 { return m_driSEIDMaxFlag; }
  void     setDriSEIZNear(double d)                                  { m_driSEIZNear = d; }
  double   getDriSEIZNear() const                                    { return m_driSEIZNear; }
  void     setDriSEIZFar(double d)                                   { m_driSEIZFar = d; }
  double   getDriSEIZFar() const                                     { return m_driSEIZFar; }
  void     setDriSEIDMin(double d)                                   { m_driSEIDMin = d; }
  double   getDriSEIDMin() const                                     { return m_driSEIDMin; }
  void     setDriSEIDMax(double d)                                   { m_driSEIDMax = d; }
  double   getDriSEIDMax() const                                     { return m_driSEIDMax; }
  void     setDriSEIDepthRepresentationType(int i)                   { m_driSEIDepthRepresentationType = i; }
  int      getDriSEIDepthRepresentationType() const                  { return m_driSEIDepthRepresentationType; }
  void     setDriSEIDisparityRefViewId(int i)                        { m_driSEIDisparityRefViewId = i; }
  int      getDriSEIDisparityRefViewId() const                       { return m_driSEIDisparityRefViewId; }
  void     setDriSEINonlinearNumMinus1(int i)                        { m_driSEINonlinearNumMinus1 = i; }
  int      getDriSEINonlinearNumMinus1() const                       { return m_driSEINonlinearNumMinus1; }
  void     setDriSEINonlinearModel(const std::vector<uint32_t>& driSEINonLinearModel) { m_driSEINonlinearModel = driSEINonLinearModel; }
  uint32_t getDriSEINonlinearModel(int idx) const                                                    { return m_driSEINonlinearModel[idx]; }
 
  //SEI manifest
  void setSEIManifestSEIEnabled(bool b) { m_SEIManifestSEIEnabled = b; }
  bool getSEIManifestSEIEnabled() { return m_SEIManifestSEIEnabled; }
  //SEI prefix indication
  void setSEIPrefixIndicationSEIEnabled(bool b) { m_SEIPrefixIndicationSEIEnabled = b; }
  bool getSEIPrefixIndicationSEIEnabled() { return m_SEIPrefixIndicationSEIEnabled; }
  
  void     setConstrainedRaslencoding(bool b)                        { m_constrainedRaslEncoding = b; }
  bool     getConstrainedRaslencoding()                              { return m_constrainedRaslEncoding; }
  void     setCraAPSreset(bool b)                                    { m_craAPSreset = b; }
  bool     getCraAPSreset()                                    const { return m_craAPSreset; }
  void     setRprRASLtoolSwitch(bool b)                              { m_rprRASLtoolSwitch = b; }
  bool     getRprRASLtoolSwitch()                                    { return m_rprRASLtoolSwitch; }

  //SEI messages processing order
  void     setPoSEIEnabled(bool b)                                   { m_poSEIEnabled = b; }
  bool     getPoSEIEnabled()                                         { return m_poSEIEnabled; }
  void     setPoSEIId(uint32_t i)                                    { m_poSEIId = i; }
  uint32_t getPoSEIId()                                              { return m_poSEIId; }
  void     setPoSEIForHumanViewingIdc(uint32_t i)                    { m_poSEIForHumanViewingIdc = i; }
  uint32_t getPoSEIForHumanViewingIdc()                              { return m_poSEIForHumanViewingIdc; }
  void     setPoSEIForMachineAnalysisIdc(uint32_t i)                 { m_poSEIForMachineAnalysisIdc = i; }
  uint32_t getPoSEIForMachineAnalysisIdc()                           { return m_poSEIForMachineAnalysisIdc; }
  void     setPoSEINumMinus2(uint32_t i)                             { m_poSEINumMinus2 = i; }
  uint32_t getPoSEINumMinus2()                                       { return m_poSEINumMinus2; }
  void     setPoSEIBreadthFirstFlag(bool b)                          { m_poSEIBreadthFirstFlag = b; }
  bool     getPoSEIBreadthFirstFlag()                                { return m_poSEIBreadthFirstFlag; }
  void     setPoSEIWrappingFlag(const std::vector<bool>& b)          { m_poSEIWrappingFlag = b; }
  bool     getPoSEIWrappingFlag(uint16_t idx)                  const { return m_poSEIWrappingFlag[idx]; }
  void     setPoSEIImportanceFlag(const std::vector<bool>& b)        { m_poSEIImportanceFlag = b; }
  bool     getPoSEIImportanceFlag(uint16_t idx)                const { return m_poSEIImportanceFlag[idx]; }
  void     setPoSEIProcessingDegreeFlag(const std::vector<bool>& b)  { m_poSEIProcessingDegreeFlag = b; }
  bool     getPoSEIProcessingDegreeFlag(uint16_t idx)          const { return m_poSEIProcessingDegreeFlag[idx]; }
  void     setPoSEIPrefixFlag(const std::vector<bool>& b)           { m_poSEIPrefixFlag = b; }
  bool     getPoSEIPrefixFlag(uint16_t idx)                   const { return m_poSEIPrefixFlag[idx]; }
  void     setPoSEIPayloadType(const std::vector<uint16_t>& b)       { m_poSEIPayloadType = b; }
  uint16_t getPoSEIPayloadType(uint16_t idx)                   const { return m_poSEIPayloadType[idx]; }
  void     setPoSEIProcessingOrder(const std::vector<uint16_t>& b) { m_poSEIProcessingOrder = b; }
  uint16_t  getPoSEIProcessingOrder(uint16_t idx)              const { return m_poSEIProcessingOrder[idx]; }
  void     setPoSEINumOfPrefixBits(const std::vector<uint16_t>& b)   { m_poSEINumOfPrefixBits = b; }
  uint16_t getPoSEINumOfPrefixBits(uint16_t idx)               const { return m_poSEINumOfPrefixBits[idx]; }
  uint32_t getPoSEIPayloadTypeSize()                           const { return (uint32_t)m_poSEIPayloadType.size(); }
  void     setPoSEIPrefixByte(const std::vector<std::vector<uint8_t>>& b) { m_poSEIPrefixByte = b; }
#if JVET_AJ0105_SPO_COMPLEXITY_INFO
  void     setPoSEIComplexityInfoPresentFlag(bool b)                 { m_poSEIComplexityInfoPresentFlag = b; }
  bool     getPoSEIComplexityInfoPresentFlag()                       { return m_poSEIComplexityInfoPresentFlag; }
  void     setPoSEIParameterTypeIdc(uint32_t b)                      { m_poSEIParameterTypeIdc = b; }
  uint32_t getPoSEIParameterTypeIdc()                                { return m_poSEIParameterTypeIdc; }
  void     setPoSEILog2ParameterBitLengthMinus3(uint32_t b)          { m_poSEILog2ParameterBitLengthMinus3 = b; }
  uint32_t getPoSEILog2ParameterBitLengthMinus3()                   { return m_poSEILog2ParameterBitLengthMinus3; }
  void     setPoSEINumParametersIdc(uint32_t b)                      { m_poSEINumParametersIdc = b; }
  uint32_t getPoSEINumParametersIdc()                                { return m_poSEINumParametersIdc; }
  void     setPoSEINumKmacOperationIdc(uint32_t b)                   { m_poSEINumKmacOperationIdc = b; }
  uint32_t getPoSEINumKmacOperationIdc()                             { return m_poSEINumKmacOperationIdc; }
  void     setPoSEITotalKilobyteSize(uint32_t b)                     { m_poSEITotalKilobyteSize = b; }
  uint32_t getPoSEITotalKilobyteSize()                               { return m_poSEITotalKilobyteSize; }
#endif
  std::vector<uint8_t>  getPoSEIPrefixByte(uint16_t idx)       const { return m_poSEIPrefixByte[idx]; }
  void     setPostFilterHintSEIEnabled(bool b) { m_postFilterHintSEIEnabled = b; }
  bool     getPostFilterHintSEIEnabled() { return m_postFilterHintSEIEnabled; }
  void     setPostFilterHintSEICancelFlag(bool b) { m_postFilterHintSEICancelFlag = b; }
  bool     getPostFilterHintSEICancelFlag() { return m_postFilterHintSEICancelFlag; }
  void     setPostFilterHintSEIPersistenceFlag(bool b) { m_postFilterHintSEIPersistenceFlag = b; }
  bool     getPostFilterHintSEIPersistenceFlag() { return m_postFilterHintSEIPersistenceFlag; }
  void     setPostFilterHintSEISizeY(uint32_t i) { m_postFilterHintSEISizeY = i; }
  uint32_t getPostFilterHintSEISizeY() { return m_postFilterHintSEISizeY; }
  void     setPostFilterHintSEISizeX(uint32_t i) { m_postFilterHintSEISizeX = i; }
  uint32_t getPostFilterHintSEISizeX() { return m_postFilterHintSEISizeX; }
  void     setPostFilterHintSEIType(uint32_t i) { m_postFilterHintSEIType = i; }
  uint32_t getPostFilterHintSEIType() { return m_postFilterHintSEIType; }
  void     setPostFilterHintSEIChromaCoeffPresentFlag(bool b) { m_postFilterHintSEIChromaCoeffPresentFlag = b; }
  bool     getPostFilterHintSEIChromaCoeffPresentFlag() { return m_postFilterHintSEIChromaCoeffPresentFlag; }
  void     setPostFilterHintSEIValues(const std::vector<int32_t> &b) { m_postFilterHintValues = b; }
  int32_t  getPostFilterHintSEIValues(int32_t idx) const { return m_postFilterHintValues[idx]; }
  void         setTextDescriptionSEIId(const uint16_t i) {m_textDescriptionSEIId = i;}
  uint32_t     getTextDescriptionSEIId() {return m_textDescriptionSEIId;}
  void         setTextSEICancelFlag(bool b) {m_textSEICancelFlag = b;}
  bool         getTextSEICancelFlag() {return m_textSEICancelFlag;}
  void         setTextSEIIDCancelFlag(bool b) { m_textSEIIDCancelFlag = b; }
  bool         getTextSEIIDCancelFlag() { return m_textSEIIDCancelFlag; }
  void         setTextSEIPersistenceFlag(bool b) {m_textSEIPersistenceFlag = b;}
  bool         getTextSEIPersistenceFlag() {return m_textSEIPersistenceFlag;}
  void         setTextSEIPurpose(uint8_t i) {m_textSEIDescriptionPurpose = i;}
  uint32_t     getTextSEIPurpose() {return m_textSEIDescriptionPurpose;}
  void         setTextSEINumStringsMinus1(uint8_t i) {m_textSEINumStringsMinus1 = i;}
  uint32_t     getTextSEINumStringsMinus1() { return m_textSEINumStringsMinus1;}
  void         setTextSEIDescriptionStringLang(const std::vector<std::string> b) {m_textSEIDescriptionStringLang = b;}
  std::string  getTextSEIDescriptionStringLang(int idx) const {return m_textSEIDescriptionStringLang[idx];}
  void         setTextSEIDescriptionString(const std::vector<std::string> b) {m_textSEIDescriptionString = b;}
  std::string  getTextSEIDescriptionString(int idx) const {return m_textSEIDescriptionString[idx];}
#if  JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
  void         setAURSEIEnabled(bool b) { m_aurSEIEnabled = b; }
  bool         getAURSEIEnabled() const { return m_aurSEIEnabled; }
  void         setAURSEICancelFlag(bool b) { m_aurSEICancelFlag = b; }
  bool         getAURSEICancelFlag() const { return m_aurSEICancelFlag; }
  void         setAURSEIPersistenceFlag(bool b) { m_aurSEIPersistenceFlag = b; }
  bool         getAURSEIPersistenceFlag() const { return m_aurSEIPersistenceFlag; }
  void         setAURSEINumRestrictionsMinus1(uint32_t b) { m_aurSEINumRestrictionsMinus1 = b; }
  uint32_t     getAURSEINumRestrictionsMinus1() { return m_aurSEINumRestrictionsMinus1; }
  void         setAURSEIRestrictions(std::vector<uint32_t> b) { m_aurSEIRestrictions = b; }
  uint32_t     getAURSEIRestrictions(uint32_t idx) const { return m_aurSEIRestrictions[idx]; }
  void         setAURSEIContextPresentFlag(std::vector<bool> b) { m_aurSEIContextPresentFlag = b; }
  bool         getAURSEIContextPresentFlag(uint32_t idx) const { return m_aurSEIContextPresentFlag[idx]; }
  void         setAURSEIContext(std::vector<uint32_t> b) { m_aurSEIContext = b; }
  uint32_t     getAURSEIContext(uint32_t idx) const { return m_aurSEIContext[idx]; }
#endif

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  void     setPriSEIEnabled(bool b)                                       { m_priSEIEnabled = b; }
  bool     getPriSEIEnabled()                                             { return m_priSEIEnabled; }
  void     setPriSEICancelFlag(bool b)                                    { m_priSEICancelFlag = b; }
  bool     getPriSEICancelFlag()                                          { return m_priSEICancelFlag; }
  void     setPriSEIPersistenceFlag(bool b)                               { m_priSEIPersistenceFlag = b; }
  bool     getPriSEIPersistenceFlag()                                     { return m_priSEIPersistenceFlag; }
  void     setPriSEINumRegionsMinus1(uint32_t i)                          { m_priSEINumRegionsMinus1 = i; }
  uint32_t getPriSEINumRegionsMinus1()                                    { return m_priSEINumRegionsMinus1; }
#if JVET_AL0324_AL0070_PRI_SEI
  void     setPriSEIMultilayerFlag(bool b)                                { m_priSEIMultilayerFlag = b; }
  bool     getPriSEIMultilayerFlag()                                      { return m_priSEIMultilayerFlag; }
#endif
  void     setPriSEIUseMaxDimensionsFlag(bool b)                          { m_priSEIUseMaxDimensionsFlag = b; }
  bool     getPriSEIUseMaxDimensionsFlag()                                { return m_priSEIUseMaxDimensionsFlag; }
  void     setPriSEILog2UnitSize(uint32_t i)                              { m_priSEILog2UnitSize = i; }
  uint32_t getPriSEILog2UnitSize()                                        { return m_priSEILog2UnitSize; }
  void     setPriSEIRegionSizeLenMinus1(uint32_t i)                       { m_priSEIRegionSizeLenMinus1 = i; }
  uint32_t getPriSEIRegionSizeLenMinus1()                                 { return m_priSEIRegionSizeLenMinus1; }
  void     setPriSEIRegionIdPresentFlag(bool b)                           { m_priSEIRegionIdPresentFlag = b; }
  bool     getPriSEIRegionIdPresentFlag()                                 { return m_priSEIRegionIdPresentFlag; }
  void     setPriSEITargetPicParamsPresentFlag(bool b)                    { m_priSEITargetPicParamsPresentFlag = b; }
  bool     getPriSEITargetPicParamsPresentFlag()                          { return m_priSEITargetPicParamsPresentFlag; }
  void     setPriSEITargetPicWidthMinus1(uint32_t i)                      { m_priSEITargetPicWidthMinus1 = i; }
  uint32_t getPriSEITargetPicWidthMinus1()                                { return m_priSEITargetPicWidthMinus1; }
  void     setPriSEITargetPicHeightMinus1(uint32_t i)                     { m_priSEITargetPicHeightMinus1 = i; }
  uint32_t getPriSEITargetPicHeightMinus1()                               { return m_priSEITargetPicHeightMinus1; }
  void     setPriSEINumResamplingRatiosMinus1(uint32_t i)                 { m_priSEINumResamplingRatiosMinus1 = i; }
  uint32_t getPriSEINumResamplingRatiosMinus1()                           { return m_priSEINumResamplingRatiosMinus1; }
  void     setPriSEIResamplingWidthNumMinus1(std::vector<uint32_t>& b)    { m_priSEIResamplingWidthNumMinus1 = b; }
  uint32_t getPriSEIResamplingWidthNumMinus1(int i)                       { return m_priSEIResamplingWidthNumMinus1[i]; }
  void     setPriSEIResamplingWidthDenomMinus1(std::vector<uint32_t>& b)  { m_priSEIResamplingWidthDenomMinus1 = b; }
  uint32_t getPriSEIResamplingWidthDenomMinus1(int i)                     { return m_priSEIResamplingWidthDenomMinus1[i]; }
  void     setPriSEIFixedAspectRatioFlag(std::vector<bool>& b)            { m_priSEIFixedAspectRatioFlag = b; }
  bool     getPriSEIFixedAspectRatioFlag(int i)                           { return m_priSEIFixedAspectRatioFlag[i]; }
  void     setPriSEIResamplingHeightNumMinus1(std::vector<uint32_t>& b)   { m_priSEIResamplingHeightNumMinus1 = b; }
  uint32_t getPriSEIResamplingHeightNumMinus1(int i)                      { return m_priSEIResamplingHeightNumMinus1[i]; }
  void     setPriSEIResamplingHeightDenomMinus1(std::vector<uint32_t>& b) { m_priSEIResamplingHeightDenomMinus1 = b; }
  uint32_t getPriSEIResamplingHeightDenomMinus1(int i)                    { return m_priSEIResamplingHeightDenomMinus1[i]; }
  void     setPriSEIRegionId(std::vector<uint32_t>& b)                    { m_priSEIRegionId = b; }
  uint32_t getPriSEIRegionId(int i)                                       { return m_priSEIRegionId[i]; }
#if JVET_AL0324_AL0070_PRI_SEI
  void     setPriSEIRegionLayerId(std::vector<uint32_t>& b)               { m_priSEIRegionLayerId = b; }
  uint32_t getPriSEIRegionLayerId(int i)                                  { return m_priSEIRegionLayerId[i]; }
  void     setPriSEIRegionIsALayerFlag(std::vector<bool>& b)              { m_priSEIRegionIsALayerFlag = b; }
  uint32_t getPriSEIRegionIsALayerFlag(int i)                             { return m_priSEIRegionIsALayerFlag[i]; }
#endif
  void     setPriSEIRegionTopLeftInUnitsX(std::vector<uint32_t>& b)       { m_priSEIRegionTopLeftInUnitsX = b; }
  uint32_t getPriSEIRegionTopLeftInUnitsX(int i)                          { return m_priSEIRegionTopLeftInUnitsX[i]; }
  void     setPriSEIRegionTopLeftInUnitsY(std::vector<uint32_t>& b)       { m_priSEIRegionTopLeftInUnitsY = b; }
  uint32_t getPriSEIRegionTopLeftInUnitsY(int i)                          { return m_priSEIRegionTopLeftInUnitsY[i]; }
  void     setPriSEIRegionWidthInUnitsMinus1(std::vector<uint32_t>& b)    { m_priSEIRegionWidthInUnitsMinus1 = b; }
  uint32_t getPriSEIRegionWidthInUnitsMinus1(int i)                       { return m_priSEIRegionWidthInUnitsMinus1[i]; }
  void     setPriSEIRegionHeightInUnitsMinus1(std::vector<uint32_t>& b)   { m_priSEIRegionHeightInUnitsMinus1 = b; }
  uint32_t getPriSEIRegionHeightInUnitsMinus1(int i)                      { return m_priSEIRegionHeightInUnitsMinus1[i]; }
  void     setPriSEIResamplingRatioIdx(std::vector<uint32_t>& b)          { m_priSEIResamplingRatioIdx = b; }
  uint32_t getPriSEIResamplingRatioIdx(int i)                             { return m_priSEIResamplingRatioIdx[i]; }
#if JVET_AL0324_AL0070_PRI_SEI
  void     setPriSEITargetRegionTopLeftInUnitsX(std::vector<uint32_t>& b) { m_priSEITargetRegionTopLeftInUnitsX = b; }
  uint32_t getPriSEITargetRegionTopLeftInUnitsX(int i) { return m_priSEITargetRegionTopLeftInUnitsX[i]; }
  void     setPriSEITargetRegionTopLeftInUnitsY(std::vector<uint32_t>& b) { m_priSEITargetRegionTopLeftInUnitsY = b; }
  uint32_t getPriSEITargetRegionTopLeftInUnitsY(int i) { return m_priSEITargetRegionTopLeftInUnitsY[i]; }
#else
  void     setPriSEITargetRegionTopLeftX(std::vector<uint32_t>& b)        { m_priSEITargetRegionTopLeftX = b; }
  uint32_t getPriSEITargetRegionTopLeftX(int i)                           { return m_priSEITargetRegionTopLeftX[i]; }
  void     setPriSEITargetRegionTopLeftY(std::vector<uint32_t>& b)        { m_priSEITargetRegionTopLeftY = b; }
  uint32_t getPriSEITargetRegionTopLeftY(int i)                           { return m_priSEITargetRegionTopLeftY[i]; }
  void     setPriSEIMultilayerFlag(bool b)                                { m_priSEIMultilayerFlag = b; }
  bool     getPriSEIMultilayerFlag()                                      { return m_priSEIMultilayerFlag; }
  void     setPriSEIRegionLayerId(std::vector<uint32_t>& b)               { m_priSEIRegionLayerId = b; }
  uint32_t getPriSEIRegionLayerId(int i)                                  { return m_priSEIRegionLayerId[i]; }
  void     setPriSEIRegionIsALayerFlag(std::vector<bool>& b)              { m_priSEIRegionIsALayerFlag = b; }
  bool     getPriSEIRegionIsALayerFlag(int i)                             { return m_priSEIRegionIsALayerFlag[i]; }
#endif
#endif

  void         setUseWP               ( bool b )                     { m_useWeightedPred   = b;    }
  void         setWPBiPred            ( bool b )                     { m_useWeightedBiPred = b;    }
  bool         getUseWP               ()                             { return m_useWeightedPred;   }
  bool         getWPBiPred            ()                             { return m_useWeightedBiPred; }
  void         setLog2ParallelMergeLevelMinus2(uint32_t u)           { m_log2ParallelMergeLevelMinus2 = u; }
  uint32_t     getLog2ParallelMergeLevelMinus2()                     { return m_log2ParallelMergeLevelMinus2; }
  void         setMaxNumMergeCand                ( uint32_t u )          { m_maxNumMergeCand = u;      }
  uint32_t         getMaxNumMergeCand                ()                  { return m_maxNumMergeCand;   }
  void         setMaxNumAffineMergeCand          ( uint32_t u )      { m_maxNumAffineMergeCand = u;    }
  uint32_t     getMaxNumAffineMergeCand          ()                  { return m_maxNumAffineMergeCand; }
  void         setMaxNumGeoCand                  ( uint32_t u )      { m_maxNumGeoCand = u;    }
  uint32_t     getMaxNumGeoCand                  ()                  { return m_maxNumGeoCand; }
  void         setMaxNumIBCMergeCand             ( uint32_t u )      { m_maxNumIBCMergeCand = u; }
  uint32_t     getMaxNumIBCMergeCand             ()                  { return m_maxNumIBCMergeCand; }
  void         setUseScalingListId    ( ScalingListMode u )          { m_useScalingListId       = u;   }
  ScalingListMode getUseScalingListId    ()                          { return m_useScalingListId;      }
  void         setScalingListFileName       ( const std::string &s ) { m_scalingListFileName = s;      }
  const std::string& getScalingListFileName () const                 { return m_scalingListFileName;   }
  void         setDisableScalingMatrixForAlternativeColourSpace(bool b) { m_disableScalingMatrixForAlternativeColourSpace = b; }
  bool         getDisableScalingMatrixForAlternativeColourSpace()    { return m_disableScalingMatrixForAlternativeColourSpace; }
  void         setScalingMatrixDesignatedColourSpace (bool b)        { m_scalingMatrixDesignatedColourSpace = b; }
  bool         getScalingMatrixDesignatedColourSpace ()              { return m_scalingMatrixDesignatedColourSpace; }
  void         setSliceLevelRpl  ( bool b )                          { m_sliceLevelRpl = b;     }
  bool         getSliceLevelRpl  ()                                  { return m_sliceLevelRpl;  }
  void         setSliceLevelDblk ( bool b )                          { m_sliceLevelDblk = b;    }
  bool         getSliceLevelDblk ()                                  { return m_sliceLevelDblk; }
  void         setSliceLevelSao  ( bool b )                          { m_sliceLevelSao = b;     }
  bool         getSliceLevelSao  ()                                  { return m_sliceLevelSao;  }
  void         setSliceLevelAlf  ( bool b )                          { m_sliceLevelAlf = b;     }
  bool         getSliceLevelAlf  ()                                  { return m_sliceLevelAlf;  }
  void         setSliceLevelWp(bool b)                               { m_sliceLevelWp = b;      }
  bool         getSliceLevelWp()                                     { return m_sliceLevelWp;   }
  void         setSliceLevelDeltaQp(bool b)                          { m_sliceLevelDeltaQp = b; }
  bool         getSliceLevelDeltaQp()                                { return m_sliceLevelDeltaQp; }
  void         setDisableScalingMatrixForLfnstBlks(bool u) { m_disableScalingMatrixForLfnstBlks = u; }
  bool         getDisableScalingMatrixForLfnstBlks() const          { return m_disableScalingMatrixForLfnstBlks; }
  void         setTMVPModeId ( int  u )                              { m_TMVPModeId = u;    }
  int          getTMVPModeId ()                                      { return m_TMVPModeId; }
  WeightedPredictionMethod getWeightedPredictionMethod() const       { return m_weightedPredictionMethod; }
  void         setWeightedPredictionMethod( WeightedPredictionMethod m ) { m_weightedPredictionMethod = m; }
  void         setDepQuantEnabledFlag( bool b )                      { m_DepQuantEnabledFlag = b;    }
  bool         getDepQuantEnabledFlag()                              { return m_DepQuantEnabledFlag; }
  void         setSignDataHidingEnabledFlag( bool b )                { m_SignDataHidingEnabledFlag = b;    }
  bool         getSignDataHidingEnabledFlag()                        { return m_SignDataHidingEnabledFlag; }

  bool getUseRateCtrl() const { return m_rcEnableRateControl; }
  void setUseRateCtrl(bool b) { m_rcEnableRateControl = b; }
  int  getTargetBitrate() const { return m_rcTargetBitrate; }
  void setTargetBitrate(int bitrate) { m_rcTargetBitrate = bitrate; }
  int  getKeepHierBit() const { return m_rcKeepHierarchicalBit; }
  void setKeepHierBit(int i) { m_rcKeepHierarchicalBit = i; }
  bool getLCULevelRC() const { return m_rcCtuLevelRateControl; }
  void setLCULevelRC(bool b) { m_rcCtuLevelRateControl = b; }
  bool getUseLCUSeparateModel() const { return m_rcUseCtuSeparateModel; }
  void setUseLCUSeparateModel(bool b) { m_rcUseCtuSeparateModel = b; }
  int  getInitialQP() const { return m_rcInitialQp; }
  void setInitialQP(int QP) { m_rcInitialQp = QP; }
  bool getForceIntraQP() const { return m_rcForceIntraQp; }
  void setForceIntraQP(bool b) { m_rcForceIntraQp = b; }

  bool         getCpbSaturationEnabled() { return m_rcCpbSaturationEnabled; }
  void         setCpbSaturationEnabled(bool b) { m_rcCpbSaturationEnabled = b; }
  uint32_t     getCpbSize() { return m_rcCpbSize; }
  void         setCpbSize(uint32_t ui) { m_rcCpbSize = ui; }
  double       getInitialCpbFullness() { return m_rcInitialCpbFullness; }
  void         setInitialCpbFullness(double f) { m_rcInitialCpbFullness = f; }
  CostMode     getCostMode( ) const                                  { return m_costMode; }
  void         setCostMode(CostMode m )                              { m_costMode = m; }
  bool         getTSRCdisableLL       ()                             { return m_TSRCdisableLL;         }
  void         setTSRCdisableLL       ( bool b )                     { m_TSRCdisableLL = b;            }

  void         setOPI(OPI *p)                                        { m_opi = *p; }
  OPI*         getOPI()                                              { return &m_opi; }

  void         setDCI(DCI *p)                                        { m_dci = *p; }
  DCI*         getDCI()                                              { return &m_dci; }
  void         setUseRecalculateQPAccordingToLambda (bool b)         { m_recalculateQPAccordingToLambda = b;    }
  bool         getUseRecalculateQPAccordingToLambda ()               { return m_recalculateQPAccordingToLambda; }

  void setFieldSeqFlag(const bool b) { m_fieldSeqFlag = b; }
  bool getFieldSeqFlag() const { return m_fieldSeqFlag; }

  void setEfficientFieldIRAPEnabled(const bool b) { m_efficientFieldIRAPEnabled = b; }
  bool getEfficientFieldIRAPEnabled() const { return m_efficientFieldIRAPEnabled; }

  void setHarmonizeGopFirstFieldCoupleEnabled(const bool b) { m_harmonizeGopFirstFieldCoupleEnabled = b; }
  bool getHarmonizeGopFirstFieldCoupleEnabled() const { return m_harmonizeGopFirstFieldCoupleEnabled; }

  bool         getOPIEnabled()                      { return m_OPIEnabled; }
  void         setOPIEnabled(bool i)                { m_OPIEnabled = i; }
  void         setHtidPlus1(int HTid)               { m_opi.setHtidInfoPresentFlag(true); m_opi.setOpiHtidPlus1(HTid); }
  void         setTargetOlsIdx(int TOlsIdx)         { m_opi.setOlsInfoPresentFlag(true); m_opi.setOpiOlsIdx(TOlsIdx); }
  void         setRplOfDepLayerInSh(bool val)       { m_rplOfDepLayerInSh = val; }
  bool         getRplOfDepLayerInSh()         const { return m_rplOfDepLayerInSh; }

  bool         getDCIEnabled()                      { return m_DCIEnabled; }
  void         setDCIEnabled(bool i)                { m_DCIEnabled = i; }
  bool         getHrdParametersPresentFlag()                         { return m_hrdParametersPresentFlag; }
  void         setHrdParametersPresentFlag(bool i)                   { m_hrdParametersPresentFlag = i; }
  bool         getVuiParametersPresentFlag()                         { return m_vuiParametersPresentFlag; }
  void         setVuiParametersPresentFlag(bool i)                   { m_vuiParametersPresentFlag = i; }
  bool         getSamePicTimingInAllOLS() const                      { return m_samePicTimingInAllOLS; }
  void         setSamePicTimingInAllOLS(bool b)                      { m_samePicTimingInAllOLS = b; }
  bool         getAspectRatioInfoPresentFlag()                       { return m_aspectRatioInfoPresentFlag; }
  void         setAspectRatioInfoPresentFlag(bool i)                 { m_aspectRatioInfoPresentFlag = i; }
  int          getAspectRatioIdc()                                   { return m_aspectRatioIdc; }
  void         setAspectRatioIdc(int i)                              { m_aspectRatioIdc = i; }
  int          getSarWidth()                                         { return m_sarWidth; }
  void         setSarWidth(int i)                                    { m_sarWidth = i; }
  int          getSarHeight()                                        { return m_sarHeight; }
  void         setSarHeight(int i)                                   { m_sarHeight = i; }
  bool         getColourDescriptionPresentFlag()                     { return m_colourDescriptionPresentFlag; }
  void         setColourDescriptionPresentFlag(bool i)               { m_colourDescriptionPresentFlag = i; }
  int          getColourPrimaries()                                  { return m_colourPrimaries; }
  void         setColourPrimaries(int i)                             { m_colourPrimaries = i; }
  int          getTransferCharacteristics()                          { return m_transferCharacteristics; }
  void         setTransferCharacteristics(int i)                     { m_transferCharacteristics = i; }
  int          getMatrixCoefficients()                               { return m_matrixCoefficients; }
  void         setMatrixCoefficients(int i)                          { m_matrixCoefficients = i; }
  bool         getChromaLocInfoPresentFlag()                         { return m_chromaLocInfoPresentFlag; }
  void         setChromaLocInfoPresentFlag(bool i)                   { m_chromaLocInfoPresentFlag = i; }
  Chroma420LocType getChromaSampleLocTypeTopField() { return m_chromaSampleLocTypeTopField; }
  void             setChromaSampleLocTypeTopField(Chroma420LocType val) { m_chromaSampleLocTypeTopField = val; }
  Chroma420LocType getChromaSampleLocTypeBottomField() { return m_chromaSampleLocTypeBottomField; }
  void             setChromaSampleLocTypeBottomField(Chroma420LocType val) { m_chromaSampleLocTypeBottomField = val; }
  Chroma420LocType getChromaSampleLocType() { return m_chromaSampleLocType; }
  void             setChromaSampleLocType(Chroma420LocType val) { m_chromaSampleLocType = val; }
  bool         getOverscanInfoPresentFlag()                          { return m_overscanInfoPresentFlag; }
  void         setOverscanInfoPresentFlag(bool i)                    { m_overscanInfoPresentFlag = i; }
  bool         getOverscanAppropriateFlag()                          { return m_overscanAppropriateFlag; }
  void         setOverscanAppropriateFlag(bool i)                    { m_overscanAppropriateFlag = i; }
  bool         getVideoFullRangeFlag()                               { return m_videoFullRangeFlag; }
  void         setVideoFullRangeFlag(bool i)                         { m_videoFullRangeFlag = i; }

  bool         getProgressiveSourceFlag() const                      { return m_progressiveSourceFlag; }
  void         setProgressiveSourceFlag(bool b)                      { m_progressiveSourceFlag = b; }

  bool         getInterlacedSourceFlag() const                       { return m_interlacedSourceFlag; }
  void         setInterlacedSourceFlag(bool b)                       { m_interlacedSourceFlag = b; }

  bool         getNonPackedConstraintFlag() const                    { return m_nonPackedConstraintFlag; }
  void         setNonPackedConstraintFlag(bool b)                    { m_nonPackedConstraintFlag = b; }

  bool         getNonProjectedConstraintFlag() const                 { return m_nonProjectedConstraintFlag; }
  void         setNonProjectedConstraintFlag(bool b)                 { m_nonProjectedConstraintFlag = b; }

  bool         getNoRprConstraintFlag() const                        { return m_noRprConstraintFlag; }
  void         setNoRprConstraintFlag(bool b)                        { m_noRprConstraintFlag = b; }

  bool         getNoResChangeInClvsConstraintFlag() const            { return m_noResChangeInClvsConstraintFlag; }
  void         setNoResChangeInClvsConstraintFlag(bool b)            { m_noResChangeInClvsConstraintFlag = b; }

  bool         getOneTilePerPicConstraintFlag() const                { return m_oneTilePerPicConstraintFlag; }
  void         setOneTilePerPicConstraintFlag(bool b)                { m_oneTilePerPicConstraintFlag = b; }

  bool         getPicHeaderInSliceHeaderConstraintFlag() const { return m_picHeaderInSliceHeaderConstraintFlag; }
  void         setPicHeaderInSliceHeaderConstraintFlag(bool b) { m_picHeaderInSliceHeaderConstraintFlag = b; }

  bool         getOneSlicePerPicConstraintFlag() const               { return m_oneSlicePerPicConstraintFlag; }
  void         setOneSlicePerPicConstraintFlag(bool b)               { m_oneSlicePerPicConstraintFlag = b; }

  bool         getNoIdrRplConstraintFlag() const                     { return m_noIdrRplConstraintFlag; }
  void         setNoIdrRplConstraintFlag(bool b)                     { m_noIdrRplConstraintFlag = b; }

  bool         getNoRectSliceConstraintFlag() const                  { return m_noRectSliceConstraintFlag; }
  void         setNoRectSliceConstraintFlag(bool b)                  { m_noRectSliceConstraintFlag = b; }

  bool         getOneSlicePerSubpicConstraintFlag() const            { return m_oneSlicePerSubpicConstraintFlag; }
  void         setOneSlicePerSubpicConstraintFlag(bool b)            { m_oneSlicePerSubpicConstraintFlag = b; }

  bool         getNoSubpicInfoConstraintFlag() const                 { return m_noSubpicInfoConstraintFlag; }
  void         setNoSubpicInfoConstraintFlag(bool b)                 { m_noSubpicInfoConstraintFlag = b; }

  void         setSummaryOutFilename(const std::string &s)           { m_summaryOutFilename = s; }
  const std::string& getSummaryOutFilename() const                   { return m_summaryOutFilename; }
  void         setSummaryPicFilenameBase(const std::string &s)       { m_summaryPicFilenameBase = s; }
  const std::string& getSummaryPicFilenameBase() const               { return m_summaryPicFilenameBase; }

  void         setSummaryVerboseness(uint32_t v)                         { m_summaryVerboseness = v; }
  uint32_t         getSummaryVerboseness( ) const                        { return m_summaryVerboseness; }
  void         setIMV(int n)                                         { m_ImvMode = n; }
  int          getIMV() const                                        { return m_ImvMode; }
  void         setIMV4PelFast(int n)                                 { m_Imv4PelFast = n; }
  int          getIMV4PelFast() const                                { return m_Imv4PelFast; }
  void         setDecodeBitstream( int i, const std::string& s )     { m_decodeBitstreams[i] = s; }
  const std::string& getDecodeBitstream( int i )               const { return m_decodeBitstreams[i]; }
  bool         getForceDecodeBitstream1()                      const { return m_forceDecodeBitstream1; }
  void         setForceDecodeBitstream1( bool b )                    { m_forceDecodeBitstream1 = b; }
  void         setSwitchPOC( int i )                                 { m_switchPOC = i; }
  int          getSwitchPOC()                                  const { return m_switchPOC; }
  void         setSwitchDQP( int i )                                 { m_switchDQP = i; }
  int          getSwitchDQP()                                  const { return m_switchDQP; }
  void         setFastForwardToPOC( int i )                          { m_fastForwardToPOC = i; }
  int          getFastForwardToPOC()                           const { return m_fastForwardToPOC; }
  bool         useFastForwardToPOC()                           const { return m_fastForwardToPOC >= 0; }
  void         setStopAfterFFtoPOC( bool b )                         { m_stopAfterFFtoPOC = b; }
  bool         getStopAfterFFtoPOC()                           const { return m_stopAfterFFtoPOC; }
  void         setBs2ModPOCAndType( bool b )                         { m_bs2ModPOCAndType = b; }
  bool         getBs2ModPOCAndType()                           const { return m_bs2ModPOCAndType; }
  void         setDebugCTU( int i )                                  { m_debugCTU = i; }
  int          getDebugCTU()                                   const { return m_debugCTU; }

  void         setMaxNumALFAPS(int n)                                 { m_maxNumAlfAps = n; }
  int          getMaxNumALFAPS()                                const { return m_maxNumAlfAps; }
  void         setALFAPSIDShift(int n)                                { m_alfapsIDShift = n; }
  int          getALFAPSIDShift()                               const { return m_alfapsIDShift; }
  void         setConstantJointCbCrSignFlag(bool b)                   { m_constantJointCbCrSignFlag = b; }
  bool         getConstantJointCbCrSignFlag()                   const { return m_constantJointCbCrSignFlag; }

  void         setUseALF( bool b ) { m_alf = b; }
  bool         getUseALF()                                      const { return m_alf; }
  void         setAlfTrueOrg( bool b )                                { m_alfTrueOrg = b; }
  bool         getAlfTrueOrg()                                  const { return m_alfTrueOrg; }
  void         setALFStrengthLuma(double s)                     { m_alfStrengthLuma = s; }
  double       getALFStrengthLuma()                             const { return m_alfStrengthLuma; }
  void         setALFAllowPredefinedFilters(bool b)             { m_alfAllowPredefinedFilters = b; }
  bool         getALFAllowPredefinedFilters()                   const { return m_alfAllowPredefinedFilters; }
  void         setCCALFStrength(double s)                       { m_ccalfStrength = s; }
  double       getCCALFStrength()                               const { return m_ccalfStrength; }
  void         setALFStrengthChroma(double s)                  { m_alfStrengthChroma = s; }
  double       getALFStrengthChroma()                          const { return m_alfStrengthChroma; }
  void         setALFStrengthTargetLuma(double s)              { m_alfStrengthTargetLuma = s; }
  double       getALFStrengthTargetLuma()                      const { return m_alfStrengthTargetLuma; }
  void         setALFStrengthTargetChroma(double s)            { m_alfStrengthTargetChroma = s; }
  double       getALFStrengthTargetChroma()                    const { return m_alfStrengthTargetChroma; }
  void         setCCALFStrengthTarget(double s)                { m_ccalfStrengthTarget = s; }
  double       getCCALFStrengthTarget()                        const { return m_ccalfStrengthTarget; }
  void         setUseCCALF( bool b )                                  { m_ccalf = b; }
  bool         getUseCCALF()                                    const { return m_ccalf; }
  void         setCCALFQpThreshold( int b )                           { m_ccalfQpThreshold = b; }
  int          getCCALFQpThreshold()                            const { return m_ccalfQpThreshold; }
#if JVET_O0756_CALCULATE_HDRMETRICS
  void        setWhitePointDeltaE( uint32_t index, double value )     { m_whitePointDeltaE[ index ] = value; }
  double      getWhitePointDeltaE( uint32_t index )             const { return m_whitePointDeltaE[ index ]; }
  void        setMaxSampleValue(double value)                         { m_maxSampleValue = value;}
  double      getMaxSampleValue()                               const { return m_maxSampleValue;}
  void        setSampleRange(int value)                               { m_sampleRange = static_cast<hdrtoolslib::SampleRange>(value);}
  hdrtoolslib::SampleRange getSampleRange()                     const { return m_sampleRange;}
  void        setColorPrimaries(int value)                            { m_colorPrimaries = static_cast<hdrtoolslib::ColorPrimaries>(value);}
  hdrtoolslib::ColorPrimaries getColorPrimaries()               const { return m_colorPrimaries;}
  void        setEnableTFunctionLUT(bool value)                       { m_enableTFunctionLUT = value;}
  bool        getEnableTFunctionLUT()                           const { return m_enableTFunctionLUT;}
  void        setChromaLocation(uint32_t index, int value)            { m_chromaLocation[ index ] = static_cast<hdrtoolslib::ChromaLocation>(value);}
  hdrtoolslib::ChromaLocation getChromaLocation(uint32_t index) const { return m_chromaLocation[index];}
  void        setChromaUPFilter(int value)                            { m_chromaUPFilter = value;}
  int         getChromaUPFilter()                               const { return m_chromaUPFilter;}
  void        setCropOffsetLeft(int value)                            { m_cropOffsetLeft = value;}
  int         getCropOffsetLeft()                               const { return m_cropOffsetLeft;}
  void        setCropOffsetTop(int value)                             { m_cropOffsetTop = value;}
  int         getCropOffsetTop()                                const { return m_cropOffsetTop;}
  void        setCropOffsetRight(int value)                           { m_cropOffsetRight = value;}
  int         getCropOffsetRight()                              const { return m_cropOffsetRight;}
  void        setCropOffsetBottom(int value)                          { m_cropOffsetBottom = value;}
  int         getCropOffsetBottom()                             const { return m_cropOffsetBottom;}
  void        setCalculateHdrMetrics(bool value)                      { m_calculateHdrMetrics = value;}
  bool        getCalculateHdrMetrics()                          const { return m_calculateHdrMetrics;}
#endif
  void        setRprEnabled(bool b)                                   { m_rprEnabledFlag = b; }
  bool        isRprEnabled()                                    const { return m_rprEnabledFlag; }
  void        setScalingRatio( double hor, double ver )              { m_scalingRatioHor = hor, m_scalingRatioVer = ver;  }
  void        setGOPBasedRPREnabledFlag(bool b)                      { m_gopBasedRPREnabledFlag = b; }
  bool        getGOPBasedRPREnabledFlag()                            const { return m_gopBasedRPREnabledFlag; }
  void        setGOPBasedRPRQPThreshold(int qp)                      { m_gopBasedRPRQPThreshold = qp; }
  int         getGOPBasedRPRQPThreshold()                            const { return m_gopBasedRPRQPThreshold; }
  void        setScalingRatio2(double hor, double ver)               { m_scalingRatioHor2 = hor, m_scalingRatioVer2 = ver; }
  void        setScalingRatio3(double hor, double ver)               { m_scalingRatioHor3 = hor, m_scalingRatioVer3 = ver; }
  void        setPsnrThresholdRPR(double psnr, double psnr2, double psnr3) { m_psnrThresholdRPR = psnr, m_psnrThresholdRPR2 = psnr2, m_psnrThresholdRPR3 = psnr3; }
  void        setQpOffsetRPR(int qpOffset, int qpOffset2, int qpOffset3)   { m_qpOffsetRPR = qpOffset, m_qpOffsetRPR2 = qpOffset2, m_qpOffsetRPR3 = qpOffset3; }
  int         getQpOffsetRPR()                                       const { return m_qpOffsetRPR; }
  int         getQpOffsetRPR2()                                      const { return m_qpOffsetRPR2; }
  int         getQpOffsetRPR3()                                      const { return m_qpOffsetRPR3; }
  void        setQpOffsetChromaRPR(int qpOffsetChroma, int qpOffsetChroma2, int qpOffsetChroma3) { m_qpOffsetChromaRPR = qpOffsetChroma, m_qpOffsetChromaRPR2 = qpOffsetChroma2, m_qpOffsetChromaRPR3 = qpOffsetChroma3; }
  int         getQpOffsetChromaRPR()                                  const { return m_qpOffsetChromaRPR; }
  int         getQpOffsetChromaRPR2()                                 const { return m_qpOffsetChromaRPR2; }
  int         getQpOffsetChromaRPR3()                                 const { return m_qpOffsetChromaRPR3; }
  bool      getRprFunctionalityTestingEnabledFlag()                   const { return m_rprFunctionalityTestingEnabledFlag; }
  void      setRprFunctionalityTestingEnabledFlag(bool flag)          { m_rprFunctionalityTestingEnabledFlag = flag; }
  bool      getRprPopulatePPSatIntraFlag()                            const { return m_rprPopulatePPSatIntraFlag; }
  void      setRprPopulatePPSatIntraFlag(bool flag)                   { m_rprPopulatePPSatIntraFlag = flag; }
  int       getRprSwitchingSegmentSize()                              const { return m_rprSwitchingSegmentSize; }
  void      setRprSwitchingSegmentSize(int size)                      { m_rprSwitchingSegmentSize = size; }
  int       getRprSwitchingListSize()                                 const { return m_rprSwitchingListSize; }
  void      setRprSwitchingListSize(int size)                         { m_rprSwitchingListSize = size; }
  double    getRprSwitchingTime()                                     const { return m_rprSwitchingTime; }
  void      setRprSwitchingTime(int size)                             { m_rprSwitchingTime = size; }
  void      setRprSwitchingResolutionOrderList(int value, int idx)    { m_rprSwitchingResolutionOrderList[idx] = value; }
  int       getRprSwitchingResolutionOrderList(int idx)               const { return m_rprSwitchingResolutionOrderList[idx]; }
  void      setRprSwitchingQPOffsetOrderList(int value, int idx)      { m_rprSwitchingQPOffsetOrderList[idx] = value; }
  int       getRprSwitchingQPOffsetOrderList(int idx)                 const { return m_rprSwitchingQPOffsetOrderList[idx]; }
  int       getRprSwitchingSegment(int currPoc)                       const { return (currPoc / m_rprSwitchingSegmentSize % m_rprSwitchingListSize); }
  int       getRprSwitchingPPSID(int rprSegment)                      const { return RPR_PPS_ID[m_rprSwitchingResolutionOrderList[rprSegment]];}
  int       getRprResolutionIndex(int ppsId)                          const { int num = -1;
                                                                              for (int nr = 0; nr < NUM_RPR_PPS; nr++)
                                                                              {
                                                                                if (RPR_PPS_ID[nr] == ppsId)
                                                                                {
                                                                                  num = nr;
                                                                                }
                                                                              }
                                                                              return num;
                                                                             }
  void        setResChangeInClvsEnabled(bool b)                      { m_resChangeInClvsEnabled = b; }
  bool        isResChangeInClvsEnabled()                        const { return m_resChangeInClvsEnabled; }
  void        setRefLayerMetricsEnabled(bool b)                      { m_refLayerMetricsEnabled = b; } 
  bool        isRefLayerMetricsEnabled()                       const { return m_refLayerMetricsEnabled;}
  void        setSwitchPocPeriod( int p )                            { m_switchPocPeriod = p;}
  void        setUpscaledOutput( int b )                             { m_upscaledOutput = b; }
  int         getUpscaledOutput()                              const { return m_upscaledOutput; }
  void        setUpscaleFilerForDisplay(int b)                       { m_upscaleFilterForDisplay = b; }
  int         getUpscaleFilerForDisplay()                      const { return m_upscaleFilterForDisplay; }

  void        setNumRefLayers( int* numRefLayers )                   { std::memcpy( m_numRefLayers, numRefLayers, sizeof( m_numRefLayers ) ); }
  int         getNumRefLayers( int layerIdx )                  const { return m_numRefLayers[layerIdx];  }

  void        setAvoidIntraInDepLayer(bool b)                        { m_avoidIntraInDepLayer = b; }
  bool        getAvoidIntraInDepLayer()                        const { return m_avoidIntraInDepLayer; }
  void        setExplicitILRP(bool b)                        { m_explicitILRP = b; }
  bool        getExplicitILRP()                        const { return m_explicitILRP; }
  void        setEncILOpt(bool b)                            { m_encILOpt = b; }
  bool        getEncILOpt()                            const { return m_encILOpt; }
  void        setEncILOptLambdaModifier(double dValue)       { m_encILOptLambdaModifier = dValue; }
  double      getEncILOptLambdaModifier()              const { return m_encILOptLambdaModifier; }
  void              setGenerativeFaceVideoSEIEnabled(bool enabledFlag)                    { m_generativeFaceVideoEnabled = enabledFlag; }
  bool              getGenerativeFaceVideoSEIEnabled()                              const { return m_generativeFaceVideoEnabled; }
  void              setGenerativeFaceVideoSEINumber(uint32_t number)                      { m_generativeFaceVideoSEINumber = number; }
  uint32_t          getGenerativeFaceVideoSEINumber()                               const { return m_generativeFaceVideoSEINumber; }
  void              setGenerativeFaceVideoSEIBasePicFlag(bool BasePicFlag)                { m_generativeFaceVideoSEIBasePicFlag = BasePicFlag; }
  bool              getGenerativeFaceVideoSEIBasePicFlag()                          const { return m_generativeFaceVideoSEIBasePicFlag; }
  void              setGenerativeFaceVideoSEINNPresentFlag(bool NNPresentFlag)            { m_generativeFaceVideoSEINNPresentFlag = NNPresentFlag; }
  bool              getGenerativeFaceVideoSEINNPresentFlag()                        const { return m_generativeFaceVideoSEINNPresentFlag; }
  void              setGenerativeFaceVideoSEINNModeIdc(uint32_t NNModeIdc)                { m_generativeFaceVideoSEINNModeIdc = NNModeIdc; }
  uint32_t          getGenerativeFaceVideoSEINNModeIdc()                            const { return m_generativeFaceVideoSEINNModeIdc; }
  void              setGenerativeFaceVideoSEINNTagURI(const std::string &NNTagURI)        { m_generativeFaceVideoSEINNTagURI = NNTagURI; }
  const std::string getGenerativeFaceVideoSEINNTagURI()                             const { return m_generativeFaceVideoSEINNTagURI; }
  void              setGenerativeFaceVideoSEINNURI(const std::string &NNURI)              { m_generativeFaceVideoSEINNURI = NNURI; }
  const std::string getGenerativeFaceVideoSEINNURI()                                const { return m_generativeFaceVideoSEINNURI; }
  void              setGenerativeFaceVideoSEIChromaKeyInfoPresentFlag(bool ChromaKeyInfoPresentFlag)                        { m_generativeFaceVideoSEIChromaKeyInfoPresentFlag = ChromaKeyInfoPresentFlag; }
  bool              getGenerativeFaceVideoSEIChromaKeyInfoPresentFlag()                                               const { return m_generativeFaceVideoSEIChromaKeyInfoPresentFlag; }
  void              setGenerativeFaceVideoSEIChromaKeyValuePresentFlag(const std::vector<bool>& ChromaKeyValuePresentFlag)  { m_generativeFaceVideoSEIChromaKeyValuePresentFlag = ChromaKeyValuePresentFlag; }
  bool              getGenerativeFaceVideoSEIChromaKeyValuePresentFlag(int c)                                         const { return m_generativeFaceVideoSEIChromaKeyValuePresentFlag[c]; }
  void              setGenerativeFaceVideoSEIChromaKeyValue(const std::vector<uint32_t>& ChromaKeyValue)                    { m_generativeFaceVideoSEIChromaKeyValue = ChromaKeyValue; }
  uint32_t          getGenerativeFaceVideoSEIChromaKeyValue(uint32_t c)                                               const { return m_generativeFaceVideoSEIChromaKeyValue[c]; }  
  void              setGenerativeFaceVideoSEIChromaKeyThrPresentFlag(const std::vector<bool>& ChromaKeyThrPresentFlag)      { m_generativeFaceVideoSEIChromaKeyThrPresentFlag = ChromaKeyThrPresentFlag; }
  bool              getGenerativeFaceVideoSEIChromaKeyThrPresentFlag(int i)                                           const { return m_generativeFaceVideoSEIChromaKeyThrPresentFlag[i]; }
  void              setGenerativeFaceVideoSEIChromaKeyThrValue(const std::vector<uint32_t>& ChromaKeyThrValue)              { m_generativeFaceVideoSEIChromaKeyThrValue = ChromaKeyThrValue; }
  uint32_t          getGenerativeFaceVideoSEIChromaKeyThrValue(uint32_t i)                                            const { return m_generativeFaceVideoSEIChromaKeyThrValue[i]; }
  void              setGenerativeFaceVideoSEIDrivePicFusionFlag(const std::vector<bool>& DrivePicFusionFlag)  { m_generativeFaceVideoSEIDrivePicFusionFlag = DrivePicFusionFlag; }
  bool              getGenerativeFaceVideoSEIDrivePicFusionFlag(int idx)            const { return m_generativeFaceVideoSEIDrivePicFusionFlag[idx]; }
  void              setGenerativeFaceVideoSEIId(const std::vector<uint32_t>& id)          { m_generativeFaceVideoSEIId = id; }
  uint32_t          getGenerativeFaceVideoSEIId(int idx)                            const { return m_generativeFaceVideoSEIId[idx]; }
  void              setGenerativeFaceVideoSEICnt(const std::vector<uint32_t>& cnt)        { m_generativeFaceVideoSEICnt = cnt; }
  uint32_t          getGenerativeFaceVideoSEICnt(int idx)                           const { return m_generativeFaceVideoSEICnt[idx]; }
  void              setGenerativeFaceVideoSEILowConfidenceFaceParameterFlag(const std::vector<bool>& lowconfidence)             { m_generativeFaceVideoSEILowConfidenceFaceParameterFlag = lowconfidence; }
  bool              getGenerativeFaceVideoSEILowConfidenceFaceParameterFlag(int idx)                                      const { return m_generativeFaceVideoSEILowConfidenceFaceParameterFlag[idx]; }
  void              setGenerativeFaceVideoSEICoordinatePresentFlag(const std::vector<bool>& coordinatepresentFlag)              { m_generativeFaceVideoSEICoordinatePresentFlag = coordinatepresentFlag; }
  bool              getGenerativeFaceVideoSEICoordinatePresentFlag(int idx)                                               const { return m_generativeFaceVideoSEICoordinatePresentFlag[idx]; }
  void              setGenerativeFaceVideoSEICoordinateQuantizationFactor(const std::vector<uint32_t>& cqf)                     { m_generativeFaceVideoSEICoordinateQuantizationFactor = cqf; }
  uint32_t          getGenerativeFaceVideoSEICoordinateQuantizationFactor(int idx)                                        const { return m_generativeFaceVideoSEICoordinateQuantizationFactor[idx]; }
  void              setGenerativeFaceVideoSEICoordinatePredFlag(const std::vector<bool>& CoordinatePredFlag)                    { m_generativeFaceVideoSEICoordinatePredFlag = CoordinatePredFlag; }
  bool              getGenerativeFaceVideoSEICoordinatePredFlag(int idx)                                                  const { return m_generativeFaceVideoSEICoordinatePredFlag[idx]; }
  void              setGenerativeFaceVideoSEI3DCoordinateFlag(const std::vector<bool>& Coordinate3dFlag)                        { m_generativeFaceVideoSEI3DCoordinateFlag = Coordinate3dFlag; }
  bool              getGenerativeFaceVideoSEI3DCoordinateFlag(int idx)                                                    const { return m_generativeFaceVideoSEI3DCoordinateFlag[idx]; }
  void              setGenerativeFaceVideoSEICoordinatePointNum(const std::vector<uint32_t>& CoordinatePointNum)                { m_generativeFaceVideoSEICoordinatePointNum = CoordinatePointNum; }
  uint32_t          getGenerativeFaceVideoSEICoordinatePointNum(int idx)                                                  const { return m_generativeFaceVideoSEICoordinatePointNum[idx]; }
  void              setGenerativeFaceVideoSEICoordinateXTesonr(const std::vector<std::vector<double>>& Xcoordinate)             { m_generativeFaceVideoSEICoordinateXTesonr = Xcoordinate; }
  double            getGenerativeFaceVideoSEICoordinateXTesonr(int i, int j)                                              const { return m_generativeFaceVideoSEICoordinateXTesonr[i][j]; }
  void              setGenerativeFaceVideoSEICoordinateYTesonr(const std::vector<std::vector<double>>& Ycoordinate)             { m_generativeFaceVideoSEICoordinateYTesonr = Ycoordinate; }
  double            getGenerativeFaceVideoSEICoordinateYTesonr(int i, int j)                                              const { return m_generativeFaceVideoSEICoordinateYTesonr[i][j]; }
  void              setGenerativeFaceVideoSEIZCoordinateMaxValue(const std::vector<std::vector<uint32_t>>& coordinateZMaxValue) { m_generativeFaceVideoSEIZCoordinateMaxValue = coordinateZMaxValue; }
  uint32_t          getGenerativeFaceVideoSEIZCoordinateMaxValue(int i, int j)                                            const { return m_generativeFaceVideoSEIZCoordinateMaxValue[i][j]; }
  void              setGenerativeFaceVideoSEICoordinateZTesonr(const std::vector<std::vector<double>>& Zcoordinate)             { m_generativeFaceVideoSEICoordinateZTesonr = Zcoordinate; }
  double            getGenerativeFaceVideoSEICoordinateZTesonr(int i, int j)                                              const { return m_generativeFaceVideoSEICoordinateZTesonr[i][j]; }
  void              setGenerativeFaceVideoSEIMatrixPresentFlag(const std::vector<bool>& matrixpresentFlag)                                               { m_generativeFaceVideoSEIMatrixPresentFlag = matrixpresentFlag; }
  bool              getGenerativeFaceVideoSEIMatrixPresentFlag(int idx)                                                                            const { return m_generativeFaceVideoSEIMatrixPresentFlag[idx]; }
  void              setGenerativeFaceVideoSEIMatrixElementPrecisionFactor(const std::vector<uint32_t>& mpf)                                              { m_generativeFaceVideoSEIMatrixElementPrecisionFactor = mpf; }
  uint32_t          getGenerativeFaceVideoSEIMatrixElementPrecisionFactor(int idx)                                                                 const { return m_generativeFaceVideoSEIMatrixElementPrecisionFactor[idx]; }
  void              setGenerativeFaceVideoSEINumMatrixType(const std::vector<uint32_t>& nummatrixtype)                                                   { m_generativeFaceVideoSEINumMatrixType = nummatrixtype; }
  uint32_t          getGenerativeFaceVideoSEINumMatrixType(int idx)                                                                                const { return m_generativeFaceVideoSEINumMatrixType[idx]; }
  void              setGenerativeFaceVideoSEIMatrixTypeIdx(const std::vector<std::vector<uint32_t>>& matrixTypeIdx)                                      { m_generativeFaceVideoSEIMatrixTypeIdx = matrixTypeIdx; }
  uint32_t          getGenerativeFaceVideoSEIMatrixTypeIdx(int idx,int idy)                                                                        const { return m_generativeFaceVideoSEIMatrixTypeIdx[idx][idy]; }
  void              setGenerativeFaceVideoSEIMatrix3DSpaceFlag(const std::vector<std::vector<uint32_t>>& matrix3DSpaceFlag)                              { m_generativeFaceVideoSEIMatrix3DSpaceFlag = matrix3DSpaceFlag; }
  uint32_t          getGenerativeFaceVideoSEIMatrix3DSpaceFlag(int idx, int idy)                                                                   const { return m_generativeFaceVideoSEIMatrix3DSpaceFlag[idx][idy]; }
  void              setGenerativeFaceVideoSEINumMatrices(const std::vector<std::vector<uint32_t>>& numMatrices)                                          { m_generativeFaceVideoSEINumMatrices = numMatrices; }
  uint32_t          getGenerativeFaceVideoSEINumMatrices(int idx, int idy)                                                                         const { return m_generativeFaceVideoSEINumMatrices[idx][idy]; }
  void              setGenerativeFaceVideoSEIMatrixWidth(const std::vector<std::vector<uint32_t>>& matrixWidth)                                          { m_generativeFaceVideoSEIMatrixWidth = matrixWidth; }
  uint32_t          getGenerativeFaceVideoSEIMatrixWidth(int idx, int idy)                                                                         const { return m_generativeFaceVideoSEIMatrixWidth[idx][idy]; }
  void              setGenerativeFaceVideoSEIMatrixHeight(const std::vector<std::vector<uint32_t>>& matrixHeight)                                        { m_generativeFaceVideoSEIMatrixHeight = matrixHeight; }
  uint32_t          getGenerativeFaceVideoSEIMatrixHeight(int idx, int idy)                                                                        const { return m_generativeFaceVideoSEIMatrixHeight[idx][idy]; }
  void              setGenerativeFaceVideoSEIMatrixElement(const std::vector<std::vector<std::vector<std::vector<std::vector<double>>>>>& matrixElement) { m_generativeFaceVideoSEIMatrixElement = matrixElement; }
  double            getGenerativeFaceVideoSEIMatrixElement(int i, int j, int k, int l, int m)                                                      const { return m_generativeFaceVideoSEIMatrixElement[i][j][k][l][m]; }
  void              setGenerativeFaceVideoSEIMatrixPredFlag(const std::vector<bool>& matrixpredFlag)                                                      { m_generativeFaceVideoSEIMatrixPredFlag = matrixpredFlag; }
  bool              getGenerativeFaceVideoSEIMatrixPredFlag(int idx)                                                                                const { return m_generativeFaceVideoSEIMatrixPredFlag[idx]; }
  void              setGenerativeFaceVideoSEINumMatricestoNumKpsFlag(const std::vector<std::vector<uint32_t>>& numflag)                                   { m_generativeFaceVideoSEINumMatricestoNumKpsFlag = numflag; }
  uint32_t          getGenerativeFaceVideoSEINumMatricestoNumKpsFlag(int idx,int idy)                                                               const { return m_generativeFaceVideoSEINumMatricestoNumKpsFlag[idx][idy]; }
  void              setGenerativeFaceVideoSEINumMatricesInfo(const std::vector<std::vector<uint32_t>>& nummatricesinfo)                                   { m_generativeFaceVideoSEINumMatricesInfo = nummatricesinfo; }
  uint32_t          getGenerativeFaceVideoSEINumMatricesInfo(int idx,int idy)                                                                       const { return m_generativeFaceVideoSEINumMatricesInfo[idx][idy]; }
  void              setGenerativeFaceVideoSEIPayloadFilename(const std::string &payloadFilename)                                                          { m_generativeFaceVideoSEIPayloadFilename = payloadFilename; }
  const std::string getGenerativeFaceVideoSEIPayloadFilename()                                                                                      const { return m_generativeFaceVideoSEIPayloadFilename; }
  void              setGenerativeFaceVideoEnhancementSEIEnabled(bool enabledFlag)                          { m_generativeFaceVideoEnhancementEnabled = enabledFlag; }
  bool              getGenerativeFaceVideoEnhancementSEIEnabled()                                    const { return m_generativeFaceVideoEnhancementEnabled; }
  void              setGenerativeFaceVideoEnhancementSEINumber(uint32_t number)                            { m_generativeFaceVideoEnhancementSEINumber = number; }
  uint32_t          getGenerativeFaceVideoEnhancementSEINumber()                                     const { return m_generativeFaceVideoEnhancementSEINumber; }
  void              setGenerativeFaceVideoEnhancementSEIBasePicFlag(bool BasePicFlag)                      { m_generativeFaceVideoEnhancementSEIBasePicFlag = BasePicFlag; }
  bool              getGenerativeFaceVideoEnhancementSEIBasePicFlag()                                const { return m_generativeFaceVideoEnhancementSEIBasePicFlag; }
  void              setGenerativeFaceVideoEnhancementSEINNPresentFlag(bool NNPresentFlag)                  { m_generativeFaceVideoEnhancementSEINNPresentFlag = NNPresentFlag; }
  bool              getGenerativeFaceVideoEnhancementSEINNPresentFlag()                              const { return m_generativeFaceVideoEnhancementSEINNPresentFlag; }
  void              setGenerativeFaceVideoEnhancementSEINNModeIdc(uint32_t NNModeIdc)                      { m_generativeFaceVideoEnhancementSEINNModeIdc = NNModeIdc; }
  uint32_t          getGenerativeFaceVideoEnhancementSEINNModeIdc()                                  const { return m_generativeFaceVideoEnhancementSEINNModeIdc; }
  void              setGenerativeFaceVideoEnhancementSEINNTagURI(const std::string &NNTagURI)              { m_generativeFaceVideoEnhancementSEINNTagURI = NNTagURI; }
  const std::string getGenerativeFaceVideoEnhancementSEINNTagURI()                                   const { return m_generativeFaceVideoEnhancementSEINNTagURI; }
  void              setGenerativeFaceVideoEnhancementSEINNURI(const std::string &NNURI)                    { m_generativeFaceVideoEnhancementSEINNURI = NNURI; }
  const std::string getGenerativeFaceVideoEnhancementSEINNURI()                                      const { return m_generativeFaceVideoEnhancementSEINNURI; }
  void              setGenerativeFaceVideoEnhancementSEIId(const std::vector<uint32_t>& id)                { m_generativeFaceVideoEnhancementSEIId = id; }
  uint32_t          getGenerativeFaceVideoEnhancementSEIId(int idx)                                  const { return m_generativeFaceVideoEnhancementSEIId[idx]; }
  void              setGenerativeFaceVideoEnhancementSEIGFVId(const std::vector<uint32_t>& gfvid)          { m_generativeFaceVideoEnhancementSEIGFVId = gfvid; }
  uint32_t          getGenerativeFaceVideoEnhancementSEIGFVId(int idx)                               const { return m_generativeFaceVideoEnhancementSEIGFVId[idx]; }
  void              setGenerativeFaceVideoEnhancementSEIGFVCnt(const std::vector<uint32_t>& cnt)           { m_generativeFaceVideoEnhancementSEIGFVCnt = cnt; }
  uint32_t          getGenerativeFaceVideoEnhancementSEIGFVCnt(int idx)                              const { return m_generativeFaceVideoEnhancementSEIGFVCnt[idx]; }
  void              setGenerativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor(const std::vector<uint32_t>& mpf)                                              { m_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor = mpf; }
  uint32_t          getGenerativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor(int idx)                                                                 const { return m_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor[idx]; }
  void              setGenerativeFaceVideoEnhancementSEIMatrixPresentFlag(const std::vector<bool>& mpf)                                                             { m_generativeFaceVideoEnhancementSEIMatrixPresentFlag = mpf; }
  bool              getGenerativeFaceVideoEnhancementSEIMatrixPresentFlag(int idx)                                                                            const { return m_generativeFaceVideoEnhancementSEIMatrixPresentFlag[idx]; }
  void              setGenerativeFaceVideoEnhancementSEIMatrixPredFlag(const std::vector<bool>& mpf)                                                                { m_generativeFaceVideoEnhancementSEIMatrixPredFlag = mpf; }
  bool              getGenerativeFaceVideoEnhancementSEIMatrixPredFlag(int idx)                                                                               const { return m_generativeFaceVideoEnhancementSEIMatrixPredFlag[idx]; }
  void              setGenerativeFaceVideoEnhancementSEINumMatrices(const std::vector<uint32_t>& numMatrices)                                                       { m_generativeFaceVideoEnhancementSEINumMatrices = numMatrices; }
  uint32_t          getGenerativeFaceVideoEnhancementSEINumMatrices(int idx)                                                                                  const { return m_generativeFaceVideoEnhancementSEINumMatrices[idx]; }
  void              setGenerativeFaceVideoEnhancementSEIMatrixWidth(const std::vector<std::vector<uint32_t>>& matrixWidth)                                          { m_generativeFaceVideoEnhancementSEIMatrixWidth = matrixWidth; }
  uint32_t          getGenerativeFaceVideoEnhancementSEIMatrixWidth(int idx, int idj)                                                                         const { return m_generativeFaceVideoEnhancementSEIMatrixWidth[idx][idj]; }
  void              setGenerativeFaceVideoEnhancementSEIMatrixHeight(const std::vector<std::vector<uint32_t>>& matrixHeight)                                        { m_generativeFaceVideoEnhancementSEIMatrixHeight = matrixHeight; }
  uint32_t          getGenerativeFaceVideoEnhancementSEIMatrixHeight(int idx, int idj)                                                                        const { return m_generativeFaceVideoEnhancementSEIMatrixHeight[idx][idj]; }
  void              setGenerativeFaceVideoEnhancementSEIMatrixElement(const std::vector<std::vector<std::vector<std::vector<double>>>>& matrixElement)              { m_generativeFaceVideoEnhancementSEIMatrixElement = matrixElement; }
  double            getGenerativeFaceVideoEnhancementSEIMatrixElement(int i, int j, int k, int l)                                                             const { return m_generativeFaceVideoEnhancementSEIMatrixElement[i][j][k][l]; }
  void              setGenerativeFaceVideoEnhancementSEIPayloadFilename(const std::string &payloadFilename)                                                         { m_generativeFaceVideoEnhancementSEIPayloadFilename = payloadFilename; }
  const std::string getGenerativeFaceVideoEnhancementSEIPayloadFilename()                                                                                     const { return m_generativeFaceVideoEnhancementSEIPayloadFilename; }
  void              setGenerativeFaceVideoEnhancementSEIPupilPresentIdx(const std::vector<uint32_t>& pupilPresentIdx)                                               { m_generativeFaceVideoEnhancementSEIPupilPresentIdx = pupilPresentIdx; }
  uint32_t          getGenerativeFaceVideoEnhancementSEIPupilPresentIdx(int idx)                                                                              const { return m_generativeFaceVideoEnhancementSEIPupilPresentIdx[idx]; }
  void              setGenerativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor(const std::vector<uint32_t>& pupilCoordinatePrecisionFactor)                 { m_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor = pupilCoordinatePrecisionFactor; }
  uint32_t          getGenerativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor(int idx)                                                               const { return m_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor[idx]; }
  void              setGenerativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX(const std::vector<double>& pupilLeftEyeCoordinateX)                                 { m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX = pupilLeftEyeCoordinateX; }
  double            getGenerativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX(int idx)                                                                      const { return m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX[idx]; }
  void              setGenerativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY(const std::vector<double>& pupilLeftEyeCoordinateY)                                 { m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY = pupilLeftEyeCoordinateY; }
  double            getGenerativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY(int idx)                                                                      const { return m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY[idx]; }
  void              setGenerativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX(const std::vector<double>& pupilRightEyeCoordinateX)                               { m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX = pupilRightEyeCoordinateX; }
  double            getGenerativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX(int idx)                                                                     const { return m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX[idx]; }
  void              setGenerativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY(const std::vector<double>& pupilRightEyeCoordinateY)                               { m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY = pupilRightEyeCoordinateY; }
  double            getGenerativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY(int idx)                                                                     const { return m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY[idx]; }

  const EncCfgParam::CfgVPSParameters &getVPSParameters() const
  {
    return m_cfgVPSParameters;
  }
  void setVPSParameters(const EncCfgParam::CfgVPSParameters &cfg)
  {
    m_cfgVPSParameters = cfg;
  }
};

//! \}

#endif // !defined(AFX_TENCCFG_H__6B99B797_F4DA_4E46_8E78_7656339A6C41__INCLUDED_)
