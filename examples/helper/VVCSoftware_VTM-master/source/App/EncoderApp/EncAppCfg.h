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

/** \file     EncAppCfg.h
    \brief    Handle encoder configuration parameters (header)
*/

#ifndef __ENCAPPCFG__
#define __ENCAPPCFG__

#include "CommonLib/CommonDef.h"
#include "EncoderLib/EncCfgParam.h"

#include <map>
template <class T1, class T2>
static inline std::istream& operator >> (std::istream &in, std::map<T1, T2> &map);

#include "Utilities/program_options_lite.h"

#include "EncoderLib/EncCfg.h"
#if EXTENSION_360_VIDEO
#include "AppEncHelper360/TExt360AppEncCfg.h"
#endif

#if JVET_O0756_CALCULATE_HDRMETRICS
#include "HDRLib/inc/DistortionMetric.H"
#ifdef UNDEFINED
#undef UNDEFINED
#endif
#endif
namespace po = ProgramOptionsLite;

#include <sstream>
#include <vector>
#include <optional>

//! \ingroup EncoderApp
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// encoder configuration class
class EncAppCfg
{
protected:
  // file I/O
  std::string m_inputFileName;                                ///< source file name
  std::string m_bitstreamFileName;                            ///< output bitstream file
  std::string m_reconFileName;                                ///< output reconstruction file

  // Lambda modifiers
  double    m_adLambdaModifier[ MAX_TLAYER ];                 ///< Lambda modifier array for each temporal layer
  std::vector<double> m_adIntraLambdaModifier;                ///< Lambda modifier for Intra pictures, one for each temporal layer. If size>temporalLayer, then use [temporalLayer], else if size>0, use [size()-1], else use m_adLambdaModifier.
  double    m_dIntraQpFactor;                                 ///< Intra Q Factor. If negative, use a default equation: 0.57*(1.0 - Clip3( 0.0, 0.5, 0.05*(double)(isField ? (GopSize-1)/2 : GopSize-1) ))

  // source specification
  Fraction      m_frameRate;
  uint32_t      m_frameSkip;                                      ///< number of skipped frames from the beginning
  uint32_t      m_temporalSubsampleRatio;                         ///< temporal subsample ratio, 2 means code every two frames
  int       m_sourceWidth;                                   ///< source width in pixel
  int       m_sourceHeight;                                  ///< source height in pixel (when interlaced = field height)
  double    m_sourceScalingRatioHor;                          ////< source scaling ratio Horizontal
  double    m_sourceScalingRatioVer;                          ////< source scaling ratio Vertical
  int       m_sourceWidthBeforeScale;                         ///< source width in pixel before applying source scaling ratio Horizontal 
  int       m_sourceHeightBeforeScale;                        ///< source height in pixel before applying source scaling ratio Vertical (when interlaced = field height)
#if EXTENSION_360_VIDEO
  int       m_inputFileWidth;                                 ///< width of image in input file  (this is equivalent to sourceWidth,  if sourceWidth  is not subsequently altered due to padding)
  int       m_inputFileHeight;                                ///< height of image in input file (this is equivalent to sourceHeight, if sourceHeight is not subsequently altered due to padding)
#endif
  int       m_iSourceHeightOrg;                               ///< original source height in pixel (when interlaced = frame height)

  bool      m_isField;                                        ///< enable field coding
  bool      m_isTopFieldFirst;
  bool      m_efficientFieldIRAPEnabled;   ///< enable an efficient field IRAP structure.
  bool      m_harmonizeGopFirstFieldCoupleEnabled;

  int       m_conformanceWindowMode;
  int       m_confWinLeft;
  int       m_confWinRight;
  int       m_confWinTop;
  int       m_confWinBottom;
  bool      m_explicitScalingWindowEnabled;
  int       m_scalWinLeft;
  int       m_scalWinRight;
  int       m_scalWinTop;
  int       m_scalWinBottom;
  int       m_sourcePadding[2];                                       ///< number of padded pixels for width and height
  int       m_firstValidFrame;
  int       m_lastValidFrame;
  int       m_framesToBeEncoded;                              ///< number of encoded frames
  bool      m_AccessUnitDelimiter;                            ///< add Access Unit Delimiter NAL units
  bool      m_enablePictureHeaderInSliceHeader;               ///< Enable Picture Header in Slice Header

  InputColourSpaceConversion m_inputColourSpaceConvert;       ///< colour space conversion to apply to input video
  bool      m_snrInternalColourSpace;                       ///< if true, then no colour space conversion is applied for snr calculation, otherwise inverse of input is applied.
  bool      m_outputInternalColourSpace;                    ///< if true, then no colour space conversion is applied for reconstructed video, otherwise inverse of input is applied.
  ChromaFormat               m_inputChromaFormatIDC;

  bool      m_printMSEBasedSequencePSNR;
  bool      m_printHexPsnr;
  bool      m_printFrameMSE;
  bool      m_printSequenceMSE;
  bool      m_printMSSSIM;
  bool      m_printWPSNR;
  bool      m_printHighPrecEncTime = false;
  bool      m_cabacZeroWordPaddingEnabled;
  bool      m_clipInputVideoToRec709Range;
  bool      m_clipOutputVideoToRec709Range;
  bool      m_packedYUVMode;                                  ///< If true, output 10-bit and 12-bit YUV data as 5-byte and 3-byte (respectively) packed YUV data

  bool      m_gciPresentFlag;
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

  // profile/level
  Profile::Name m_profile;
  Level::Tier   m_levelTier;
  Level::Name   m_level;
  bool          m_frameOnlyConstraintFlag;
  bool          m_multiLayerEnabledFlag;
  std::vector<uint32_t>  m_subProfile;
  uint8_t      m_numSubProfile;

  uint32_t          m_bitDepthConstraint;
  ChromaFormat  m_chromaFormatConstraint;
  bool          m_onePictureOnlyConstraintFlag;
  bool          m_intraOnlyConstraintFlag;
  bool          m_nonPackedConstraintFlag;
  bool          m_nonProjectedConstraintFlag;
  bool          m_noRprConstraintFlag;
  bool          m_noResChangeInClvsConstraintFlag;
  bool          m_oneTilePerPicConstraintFlag;
  bool          m_picHeaderInSliceHeaderConstraintFlag;
  bool          m_oneSlicePerPicConstraintFlag;
  bool          m_noIdrRplConstraintFlag;
  bool          m_noRectSliceConstraintFlag;
  bool          m_oneSlicePerSubpicConstraintFlag;
  bool          m_noSubpicInfoConstraintFlag;
  // coding structure
  int m_intraPeriod;   ///< period of I-slice (random access period)
#if GDR_ENABLED
  bool      m_gdrEnabled;
  int       m_gdrPocStart;
  int       m_gdrPeriod;
  int       m_gdrInterval;
  bool      m_gdrNoHash;
#endif
  int       m_intraRefreshType;                               ///< random access type
  int       m_gopSize;                                        ///< GOP size of hierarchical structure
  int       m_drapPeriod;                                     ///< period of dependent RAP pictures
  int       m_edrapPeriod;                                    ///< period of extended dependent RAP pictures
  bool      m_rewriteParamSets;                              ///< Flag to enable rewriting of parameter sets at random access points
  RPLEntry  m_RPLList0[MAX_GOP];                               ///< the RPL entries from the config file
  RPLEntry  m_RPLList1[MAX_GOP];                               ///< the RPL entries from the config file
  bool      m_idrRefParamList;                                ///< indicates if reference picture list syntax elements are present in slice headers of IDR pictures
  GOPEntry  m_GOPList[MAX_GOP];                               ///< the coding structure entries from the config file
  int       m_maxNumReorderPics[MAX_TLAYER];                  ///< total number of reorder pictures
  int       m_maxDecPicBuffering[MAX_TLAYER];                 ///< total number of pictures in the decoded picture buffer
  bool      m_reconBasedCrossCPredictionEstimate;             ///< causes the alpha calculation in encoder search to be based on the decoded residual rather than the pre-transform encoder-side residual
  bool      m_useTransformSkip;                               ///< flag for enabling intra transform skipping
  bool      m_useTransformSkipFast;                           ///< flag for enabling fast intra transform skipping
  bool      m_useBDPCM;
  uint32_t      m_log2MaxTransformSkipBlockSize;                  ///< transform-skip maximum size (minimum of 2)
  bool      m_transformSkipRotationEnabledFlag;               ///< control flag for transform-skip/transquant-bypass residual rotation
  bool      m_transformSkipContextEnabledFlag;                ///< control flag for transform-skip/transquant-bypass single significance map context
  bool      m_rrcRiceExtensionEnableFlag;                        ///< control flag for enabling extension of the Golomb-Rice parameter derivation for RRC
  bool      m_persistentRiceAdaptationEnabledFlag;            ///< control flag for Golomb-Rice parameter adaptation over each slice
  bool      m_cabacBypassAlignmentEnabledFlag;
  bool      m_ISP;
  bool      m_useFastISP;                                    ///< flag for enabling fast methods for ISP
  int       m_fastAdaptCostPredMode;                         ///< mode for cost prediction, 0..2
  bool      m_disableFastDecisionTT;                         ///< flag for disabling fast decision for TT from BT

  // coding quality
  std::optional<uint32_t> m_qpIncrementAtSourceFrame;   // Optional source frame number at which all subsequent frames
                                                        // are to use an increased internal QP.

  int       m_iQP;                                            ///< QP value of key-picture (integer)
  int       m_qpRefAdj;
  bool      m_useIdentityTableForNon420Chroma;

  ChromaQpMappingTableParams m_chromaQpMappingTableParams;
  int       m_intraQPOffset;                                  ///< QP offset for intra slice (integer)
  bool      m_lambdaFromQPEnable;                             ///< enable flag for QP:lambda fix
  double    m_lambdaScaleTowardsNextQP;                       ///< scale lambda towards lambda of next QP
  std::string m_dQPFileName;                                  ///< QP offset for each slice (initialized from external file)

  FrameDeltaQps m_frameDeltaQps;   // array of frame delta QP values

  int       m_iMaxDeltaQP;                                    ///< max. |delta QP|
  uint32_t  m_uiDeltaQpRD;                                    ///< dQP range for multi-pass slice QP optimization
  int       m_cuQpDeltaSubdiv;                                ///< Maximum subdiv for CU luma Qp adjustment (0:default)
  int       m_cuChromaQpOffsetSubdiv;                         ///< If negative, then do not apply chroma qp offsets.
  std::vector<ChromaQpAdj> m_cuChromaQpOffsetList;            ///< Local chroma QP offsets list (to be signalled in PPS)
  bool      m_cuChromaQpOffsetEnabled;                        ///< Enable local chroma QP offsets (slice level flag)
  bool      m_bFastDeltaQP;                                   ///< Fast Delta QP (false:default)

  int       m_cbQpOffset;                                     ///< Chroma Cb QP Offset (0:default)
  int       m_crQpOffset;                                     ///< Chroma Cr QP Offset (0:default)
  int       m_cbQpOffsetDualTree;                             ///< Chroma Cb QP Offset for dual tree (overwrite m_cbQpOffset for dual tree)
  int       m_crQpOffsetDualTree;                             ///< Chroma Cr QP Offset for dual tree (overwrite m_crQpOffset for dual tree)
  int       m_cbCrQpOffset;                                   ///< QP Offset for joint Cb-Cr mode
  int       m_cbCrQpOffsetDualTree;                           ///< QP Offset for joint Cb-Cr mode (overwrite m_cbCrQpOffset for dual tree)
#if ER_CHROMA_QP_WCG_PPS
  WCGChromaQPControl m_wcgChromaQpControl;                    ///< Wide-colour-gamut chroma QP control.
#endif
#if W0038_CQP_ADJ
  uint32_t      m_sliceChromaQpOffsetPeriodicity;                 ///< Used in conjunction with Slice Cb/Cr QpOffsetIntraOrPeriodic. Use 0 (default) to disable periodic nature.
  int       m_sliceChromaQpOffsetIntraOrPeriodic[2/*Cb,Cr*/]; ///< Chroma Cb QP Offset at slice level for I slice or for periodic inter slices as defined by SliceChromaQPOffsetPeriodicity. Replaces offset in the GOP table.
#endif
#if SHARP_LUMA_DELTA_QP
  LumaLevelToDeltaQPMapping m_lumaLevelToDeltaQPMapping;      ///< mapping from luma level to Delta QP.
#endif
  SEIMasteringDisplay m_masteringDisplay;
  bool      m_smoothQPReductionEnable;
  double    m_smoothQPReductionThresholdIntra;
  double    m_smoothQPReductionModelScaleIntra;
  double    m_smoothQPReductionModelOffsetIntra;
  int       m_smoothQPReductionLimitIntra;
  double    m_smoothQPReductionThresholdInter;
  double    m_smoothQPReductionModelScaleInter;
  double    m_smoothQPReductionModelOffsetInter;
  int       m_smoothQPReductionLimitInter;
  int       m_smoothQPReductionPeriodicity;

  bool      m_bUseAdaptiveQP;                                 ///< Flag for enabling QP adaptation based on a psycho-visual model
  int       m_iQPAdaptationRange;                             ///< dQP range by QP adaptation
#if ENABLE_QPA
  bool      m_bUsePerceptQPA;                                 ///< Flag to enable perceptually motivated input-adaptive QP modification
  bool      m_bUseWPSNR;                                      ///< Flag to output perceptually weighted peak SNR (WPSNR) instead of PSNR
#endif
  int       m_maxTempLayer;                                   ///< Max temporal layer
  bool      m_isLowDelay;

  // coding unit (CU) definition
  unsigned              m_ctuSize;
  bool m_subPicInfoPresentFlag;
  unsigned m_numSubPics;
  bool m_subPicSameSizeFlag;
  std::vector<uint32_t> m_subPicCtuTopLeftX;
  std::vector<uint32_t> m_subPicCtuTopLeftY;
  std::vector<uint32_t> m_subPicWidth;
  std::vector<uint32_t> m_subPicHeight;
  std::vector<bool>     m_subPicTreatedAsPicFlag;
  std::vector<bool>     m_loopFilterAcrossSubpicEnabledFlag;
  bool m_subPicIdMappingExplicitlySignalledFlag;
  bool m_subPicIdMappingInSpsFlag;
  unsigned m_subPicIdLen;
  std::vector<uint16_t> m_subPicId;
  bool      m_SplitConsOverrideEnabledFlag;
  unsigned              m_minQt[3];   // 0: I slice luma; 1: P/B slice; 2: I slice chroma
  unsigned  m_uiMaxMTTHierarchyDepth;
  unsigned  m_uiMaxMTTHierarchyDepthI;
  unsigned  m_uiMaxMTTHierarchyDepthIChroma;
  unsigned              m_maxBt[3];
  unsigned              m_maxTt[3];
  int       m_ttFastSkip;
  double    m_ttFastSkipThr;
  bool      m_dualTree;
  bool      m_LFNST;
  bool      m_useFastLFNST;
  bool      m_sbTmvpEnableFlag;
  bool      m_Affine;
  bool      m_AffineType;
  bool      m_adaptBypassAffineMe;
  bool      m_PROF;
  bool      m_BIO;
  int       m_LMChroma;
  int                   m_horCollocatedChromaFlag;
  int                   m_verCollocatedChromaFlag;

  int       m_mtsMode;                                        ///< XZ: Multiple Transform Set
  int       m_MTSIntraMaxCand;                                ///< XZ: Number of additional candidates to test
  int       m_MTSInterMaxCand;                                ///< XZ: Number of additional candidates to test
  int       m_mtsImplicitIntra;

  bool      m_SBT;                                            ///< Sub-Block Transform for inter blocks
  int       m_SBTFast64WidthTh;
  bool      m_SMVD;
  bool      m_compositeRefEnabled;
  bool      m_bcw;
  bool      m_BcwFast;
  bool      m_LadfEnabed;
  int              m_ladfNumIntervals;
  std::vector<int> m_ladfQpOffset;
  int              m_ladfIntervalLowerBound[MAX_LADF_INTERVALS];

  bool      m_ciip;
  bool      m_Geo;
  bool      m_HashME;
  bool      m_allowDisFracMMVD;
  bool      m_AffineAmvr;
  bool      m_AffineAmvrEncOpt;
  bool      m_AffineAmvp;
  bool      m_DMVR;
  bool      m_MMVD;
  int       m_MmvdDisNum;
  bool      m_rgbFormat;
  bool      m_useColorTrans;
  unsigned  m_PLTMode;
  bool      m_jointCbCrMode;
  bool      m_useChromaTS;
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

  bool      m_wrapAround;
  unsigned  m_wrapAroundOffset;

  // ADD_NEW_TOOL : (encoder app) add tool enabling flags and associated parameters here
  bool      m_virtualBoundariesEnabledFlag;
  bool      m_virtualBoundariesPresentFlag;
  unsigned  m_numVerVirtualBoundaries;
  unsigned  m_numHorVirtualBoundaries;
  std::vector<unsigned> m_virtualBoundariesPosX;
  std::vector<unsigned> m_virtualBoundariesPosY;
  bool      m_lmcsEnabled;
  uint32_t  m_reshapeSignalType;
  uint32_t  m_intraCMD;
  ReshapeCW m_reshapeCW;
  int       m_updateCtrl;
  int       m_adpOption;
  uint32_t  m_initialCW;
  int       m_CSoffset;
  bool      m_encDbOpt;
  bool      m_encALFOpt;
  unsigned              m_maxCuWidth;                                      ///< max. CU width in pixel
  unsigned              m_maxCuHeight;                                     ///< max. CU height in pixel
  unsigned m_log2MinCuSize;                                   ///< min. CU size log2

  bool      m_useMttSkip;
  bool      m_useFastLCTU;
  bool      m_usePbIntraFast;
  bool      m_useAMaxBT;
  bool      m_useFastMrg;
  int       m_maxMergeRdCandNumTotal;
  int       m_mergeRdCandQuotaRegular;
  int       m_mergeRdCandQuotaRegularSmallBlk;
  int       m_mergeRdCandQuotaSubBlk;
  int       m_mergeRdCandQuotaCiip;
  int       m_mergeRdCandQuotaGpm;
  bool      m_e0023FastEnc;
  bool      m_contentBasedFastQtbt;
  bool      m_useNonLinearAlfLuma;
  bool      m_useNonLinearAlfChroma;
  unsigned  m_maxNumAlfAlternativesChroma;
  bool      m_MRL;
  bool      m_MIP;
  bool      m_useFastMIP;
  int       m_fastLocalDualTreeMode;

  int       m_log2MaxTbSize;
  // coding tools (bit-depth)
  BitDepths m_inputBitDepth;         // bit depth of input file
  BitDepths m_outputBitDepth;        // bit depth of output file
  BitDepths m_msbExtendedBitDepth;   // bit depth of input samples after MSB extension
  BitDepths m_internalBitDepth;      // bit depth codec operates at (input/output files will be converted)
  bool      m_extendedPrecisionProcessingFlag;
  bool      m_tsrcRicePresentFlag;
  bool      m_reverseLastSigCoeffEnabledFlag;
  bool      m_highPrecisionOffsetsEnabledFlag;

  //coding tools (chroma format)
  ChromaFormat m_chromaFormatIdc;

  // coding tool (SAO)
  bool      m_useSao;
  bool      m_saoTrueOrg;
  bool      m_bTestSAODisableAtPictureLevel;
  double    m_saoEncodingRate;                                ///< When >0 SAO early picture termination is enabled for luma and chroma
  double    m_saoEncodingRateChroma;                          ///< The SAO early picture termination rate to use for chroma (when m_SaoEncodingRate is >0). If <=0, use results for luma.
  int       m_maxNumOffsetsPerPic;                            ///< SAO maximun number of offset per picture
  bool      m_saoCtuBoundary;                                 ///< SAO parameter estimation using non-deblocked pixels for CTU bottom and right boundary areas
  bool      m_saoGreedyMergeEnc;                              ///< SAO greedy merge encoding algorithm
  // coding tools (loop filter)
  bool      m_deblockingFilterDisable;                        ///< flag for using deblocking filter
  bool      m_deblockingFilterOffsetInPPS;                    ///< offset for deblocking filter in 0 = slice header, 1 = PPS
  int       m_deblockingFilterBetaOffsetDiv2;                 ///< beta offset for deblocking filter
  int       m_deblockingFilterTcOffsetDiv2;                   ///< tc offset for deblocking filter
  int       m_deblockingFilterCbBetaOffsetDiv2;               ///< beta offset for Cb deblocking filter
  int       m_deblockingFilterCbTcOffsetDiv2;                 ///< tc offset for Cb deblocking filter
  int       m_deblockingFilterCrBetaOffsetDiv2;               ///< beta offset for Cr deblocking filter
  int       m_deblockingFilterCrTcOffsetDiv2;                 ///< tc offset for Cr deblocking filter
  int       m_deblockingFilterMetric;                         ///< blockiness metric in encoder

  // coding tools (encoder-only parameters)
  bool      m_bUseASR;                                        ///< flag for using adaptive motion search range
  bool      m_bUseHADME;                                      ///< flag for using HAD in sub-pel ME
  bool      m_useRDOQ;                                       ///< flag for using RD optimized quantization
  bool      m_useRDOQTS;                                     ///< flag for using RD optimized quantization for transform skip
  bool      m_useSelectiveRDOQ;                               ///< flag for using selective RDOQ
  bool      m_bDisableIntraPUsInInterSlices;                  ///< Flag for disabling intra predicted PUs in inter slices.
  MESearchMethod m_motionEstimationSearchMethod;
  bool      m_bRestrictMESampling;                            ///< Restrict sampling for the Selective ME
  int       m_iSearchRange;                                   ///< ME search range
  int       m_bipredSearchRange;                              ///< ME search range for bipred refinement
  int       m_minSearchWindow;                                ///< ME minimum search window size for the Adaptive Window ME
  bool      m_bClipForBiPredMeEnabled;                        ///< Enables clipping for Bi-Pred ME.
  bool      m_bFastMEAssumingSmootherMVEnabled;               ///< Enables fast ME assuming a smoother MV.
  FastInterSearchMode m_fastInterSearchMode;                  ///< Parameter that controls fast encoder settings
  bool      m_bUseEarlyCU;                                    ///< flag for using Early CU setting
  bool      m_useFastDecisionForMerge;                        ///< flag for using Fast Decision Merge RD-Cost
  bool      m_useEarlySkipDetection;                          ///< flag for using Early SKIP Detection
  bool      m_picPartitionFlag;                               ///< enable picture partitioning (0: single tile, single slice, 1: multiple tiles/slices can be used)
  bool      m_mixedLossyLossless;                             ///< enable mixed lossy/lossless coding
  std::vector<uint16_t> m_sliceLosslessArray;                 ///< Slice lossless array
  std::vector<uint32_t> m_tileColumnWidth;                    ///< tile column widths in units of CTUs (last column width will be repeated uniformly to cover any remaining picture width)
  std::vector<uint32_t> m_tileRowHeight;                      ///< tile row heights in units of CTUs (last row height will be repeated uniformly to cover any remaining picture height)
  bool      m_rasterSliceFlag;                                ///< indicates if using raster-scan or rectangular slices (0: rectangular, 1: raster-scan)
  std::vector<uint32_t> m_rectSlicePos;                       ///< rectangular slice positions (pairs of top-left CTU address followed by bottom-right CTU address)
  int       m_rectSliceFixedWidth;                            ///< fixed rectangular slice width in units of tiles (0: disable this feature and use RectSlicePositions instead)
  int       m_rectSliceFixedHeight;                           ///< fixed rectangular slice height in units of tiles (0: disable this feature and use RectSlicePositions instead)
  std::vector<uint32_t> m_rasterSliceSize;                    ///< raster-scan slice sizes in units of tiles (last size will be repeated uniformly to cover any remaining tiles in the picture)
  bool      m_disableLFCrossTileBoundaryFlag;                 ///< 0: filter across tile boundaries  1: do not filter across tile boundaries
  bool      m_disableLFCrossSliceBoundaryFlag;                ///< 0: filter across slice boundaries 1: do not filter across slice boundaries
  uint32_t  m_numSlicesInPic;                                 ///< derived number of rectangular slices in the picture (raster-scan slice specified at slice level)
  bool      m_tileIdxDeltaPresentFlag;                        ///< derived tile index delta present flag
  std::vector<RectSlice> m_rectSlices;                        ///< derived list of rectangular slice signalling parameters
  uint32_t  m_numTileCols;                                    ///< derived number of tile columns
  uint32_t  m_numTileRows;                                    ///< derived number of tile rows
  bool      m_singleSlicePerSubPicFlag;
  bool      m_entropyCodingSyncEnabledFlag;
  bool      m_entryPointPresentFlag;                          ///< flag for the presence of entry points

  bool      m_bFastUDIUseMPMEnabled;
  bool      m_bFastMEForGenBLowDelayEnabled;

  HashType  m_decodedPictureHashSEIType;                      ///< Checksum mode for decoded picture hash SEI message
  HashType  m_subpicDecodedPictureHashType;
  bool      m_bufferingPeriodSEIEnabled;
  bool      m_pictureTimingSEIEnabled;
  bool      m_bpDeltasGOPStructure;
  bool      m_decodingUnitInfoSEIEnabled;
  bool      m_scalableNestingSEIEnabled;
  bool      m_frameFieldInfoSEIEnabled;
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
  int       m_selfContainedClvsFlag;
  int       m_preferredTransferCharacteristics;

  bool  m_eoiSEIEnabled;
  bool  m_eoiSEICancelFlag;
  bool  m_eoiSEIPersistenceFlag;
  uint32_t m_eoiSEIForHumanViewingIdc;
  uint32_t m_eoiSEIForMachineAnalysisIdc;
  uint32_t m_eoiSEIType;
  uint32_t m_eoiSEIObjectBasedIdc;
  uint32_t m_eoiSEIQuantThresholdDelta;
  bool     m_eoiSEIPicQuantObjectFlag;
  bool m_eoiSEITemporalResamplingTypeFlag;
  uint32_t m_eoiSEINumIntPics;
#if JVET_AJ0183_EOI_SEI_SRC_PIC_FLAG
  bool     m_eoiSEISrcPicFlag;;
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

  bool      m_miSEIEnabled;
  bool      m_miCancelFlag;
  bool      m_miPersistenceFlag;
  int       m_miModalityType; 
  bool      m_miSpectrumRangePresentFlag;
  int       m_miMinWavelengthMantissa; 
  int       m_miMinWavelengthExponentPlus15; 
  int       m_miMaxWavelengthMantissa; 
  int       m_miMaxWavelengthExponentPlus15; 

  // film grain characterstics sei
  bool      m_fgcSEIEnabled;
  bool      m_fgcSEICancelFlag;
  bool      m_fgcSEIPersistenceFlag;
  uint32_t  m_fgcSEIModelID;
  bool      m_fgcSEISepColourDescPresentFlag;
  uint32_t  m_fgcSEIBlendingModeID;
  uint32_t  m_fgcSEILog2ScaleFactor;
  bool      m_fgcSEICompModelPresent[MAX_NUM_COMPONENT];
  bool      m_fgcSEIAnalysisEnabled;
  std::string m_fgcSEIExternalMask;
  std::string m_fgcSEIExternalDenoised;
  int       m_fgcSEITemporalFilterPastRefs;
  int       m_fgcSEITemporalFilterFutureRefs;
  std::map<int, double> m_fgcSEITemporalFilterStrengths;
  bool      m_fgcSEIPerPictureSEI;
  uint32_t  m_fgcSEINumModelValuesMinus1          [MAX_NUM_COMPONENT];
  uint32_t  m_fgcSEINumIntensityIntervalMinus1    [MAX_NUM_COMPONENT];
  uint32_t  m_fgcSEIIntensityIntervalLowerBound   [MAX_NUM_COMPONENT][MAX_NUM_INTENSITIES];
  uint32_t  m_fgcSEIIntensityIntervalUpperBound   [MAX_NUM_COMPONENT][MAX_NUM_INTENSITIES];
  uint32_t  m_fgcSEICompModelValue                [MAX_NUM_COMPONENT][MAX_NUM_INTENSITIES][MAX_NUM_MODEL_VALUES];
  // content light level SEI
  bool      m_cllSEIEnabled;
  uint32_t  m_cllSEIMaxContentLevel;
  uint32_t  m_cllSEIMaxPicAvgLevel;
  // ambient viewing environment sei
  bool      m_aveSEIEnabled;
  uint32_t  m_aveSEIAmbientIlluminance;
  uint32_t  m_aveSEIAmbientLightX;
  uint32_t  m_aveSEIAmbientLightY;
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
  // content colour volume sei
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
  // scalability dimension information sei
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
  // multiview acquisition information sei
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
  std::vector<bool>      m_maiSEISignFocalLengthY;
  std::vector<uint32_t>  m_maiSEIExponentFocalLengthY;
  std::vector<uint32_t>  m_maiSEIMantissaFocalLengthY;
  std::vector<bool>      m_maiSEISignPrincipalPointX;
  std::vector<uint32_t>  m_maiSEIExponentPrincipalPointX;
  std::vector<uint32_t>  m_maiSEIMantissaPrincipalPointX;
  std::vector<bool>      m_maiSEISignPrincipalPointY;
  std::vector<uint32_t>  m_maiSEIExponentPrincipalPointY;
  std::vector<uint32_t>  m_maiSEIMantissaPrincipalPointY;
  std::vector<bool>      m_maiSEISignSkewFactor;
  std::vector<uint32_t>  m_maiSEIExponentSkewFactor;
  std::vector<uint32_t>  m_maiSEIMantissaSkewFactor;
  int               m_maiSEIPrecRotationParam;
  int               m_maiSEIPrecTranslationParam;
  // multiview acquisition information sei
  bool              m_mvpSEIEnabled;
  int               m_mvpSEINumViewsMinus1;
  std::vector<uint32_t>  m_mvpSEIViewPosition;
  // alpha channel information sei
  bool      m_aciSEIEnabled;
  bool      m_aciSEICancelFlag;
  int       m_aciSEIUseIdc;
  int       m_aciSEIBitDepthMinus8;
  int       m_aciSEITransparentValue;
  int       m_aciSEIOpaqueValue;
  bool      m_aciSEIIncrFlag;
  bool      m_aciSEIClipFlag;
  bool      m_aciSEIClipTypeFlag;
  // depth representation information sei
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
  std::string           m_arSEIFileRoot;  // Annotated region SEI - initialized from external file
  std::string           m_omiSEIFileRoot;  // Object mask information SEI - initialized from external file
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
  uint32_t             m_gcmpSEIPackingType;
  uint32_t             m_gcmpSEIMappingFunctionType;
  std::vector<uint8_t> m_gcmpSEIFaceIndex;
  std::vector<uint8_t> m_gcmpSEIFaceRotation;
  std::vector<double>  m_gcmpSEIFunctionCoeffU;
  std::vector<bool>    m_gcmpSEIFunctionUAffectedByVFlag;
  std::vector<double>  m_gcmpSEIFunctionCoeffV;
  std::vector<bool>    m_gcmpSEIFunctionVAffectedByUFlag;
  bool                 m_gcmpSEIGuardBandFlag;
  uint32_t             m_gcmpSEIGuardBandType;
  bool                 m_gcmpSEIGuardBandBoundaryExteriorFlag;
  uint32_t             m_gcmpSEIGuardBandSamplesMinus1;

  EncCfgParam::CfgSEISubpictureLevel m_cfgSubpictureLevelInfoSEI;
#if JVET_AJ0151_DSC_SEI
  EncCfgParam::CfgSEIDigitallySignedContent m_cfgDigitallySignedContentSEI;
#endif

  bool                  m_nnPostFilterSEICharacteristicsEnabled;
  bool                  m_nnPostFilterSEICharacteristicsUseSuffixSEI;
  int                   m_nnPostFilterSEICharacteristicsNumFilters;
  uint32_t              m_nnPostFilterSEICharacteristicsId[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsModeIdc[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsPropertyPresentFlag[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsBaseFlag[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsPurpose[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsOutSubCFlag[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsOutColourFormatIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsPicWidthNumerator[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsPicWidthDenominator[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsPicHeightNumerator[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsPicHeightDenominator[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsInpTensorBitDepthLumaMinus8[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsInpTensorBitDepthChromaMinus8[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsOutTensorBitDepthLumaMinus8[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsOutTensorBitDepthChromaMinus8[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsComponentLastFlag[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsInpFormatIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsAuxInpIdc[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsSepColDescriptionFlag[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsFullRangeFlag[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsColPrimaries[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsTransCharacteristics[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsMatrixCoeffs[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsInpOrderIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsOutFormatIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsOutOrderIdc[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsConstantPatchSizeFlag[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsChromaLocInfoPresentFlag[MAX_NUM_NN_POST_FILTERS];
  uint32_t               m_nnPostFilterSEICharacteristicsChromaSampleLocTypeFrame[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsPatchWidthMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsPatchHeightMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsExtendedPatchWidthCdDeltaMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsExtendedPatchHeightCdDeltaMinus1[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsOverlap[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsPaddingType[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsLumaPadding[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsCbPadding[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsCrPadding[MAX_NUM_NN_POST_FILTERS];
  std::string           m_nnPostFilterSEICharacteristicsPayloadFilename[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsComplexityInfoPresentFlag[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsApplicationPurposeTagUriPresentFlag[MAX_NUM_NN_POST_FILTERS];
  std::string           m_nnPostFilterSEICharacteristicsApplicationPurposeTagUri[MAX_NUM_NN_POST_FILTERS];
#if NNPFC_SCAN_TYPE_IDC
  uint32_t              m_nnPostFilterSEICharacteristicsScanTypeIdc[MAX_NUM_NN_POST_FILTERS];
#endif
  uint32_t              m_nnPostFilterSEICharacteristicsForHumanViewingIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsForMachineAnalysisIdc[MAX_NUM_NN_POST_FILTERS];
  std::string           m_nnPostFilterSEICharacteristicsUriTag[MAX_NUM_NN_POST_FILTERS];
  std::string           m_nnPostFilterSEICharacteristicsUri[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsParameterTypeIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsLog2ParameterBitLengthMinus3[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsNumParametersIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsNumKmacOperationsIdc[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsTotalKilobyteSize[MAX_NUM_NN_POST_FILTERS];

  bool                  m_nnPostFilterSEIActivationEnabled;
  bool                  m_nnPostFilterSEIActivationUseSuffixSEI;
  uint32_t              m_nnPostFilterSEIActivationTargetId;
  uint32_t              m_nnPostFilterSEICharacteristicsNumberInputDecodedPicturesMinus1[MAX_NUM_NN_POST_FILTERS];
  std::vector<uint32_t> m_nnPostFilterSEICharacteristicsNumberInterpolatedPictures[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsNumberExtrapolatedPicturesMinus1[MAX_NUM_NN_POST_FILTERS];
  int32_t               m_nnPostFilterSEICharacteristicsSpatialExtrapolationLeftOffset[MAX_NUM_NN_POST_FILTERS];
  int32_t               m_nnPostFilterSEICharacteristicsSpatialExtrapolationRightOffset[MAX_NUM_NN_POST_FILTERS];
  int32_t               m_nnPostFilterSEICharacteristicsSpatialExtrapolationTopOffset[MAX_NUM_NN_POST_FILTERS];
  int32_t               m_nnPostFilterSEICharacteristicsSpatialExtrapolationBottomOffset[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsInbandPromptFlag[MAX_NUM_NN_POST_FILTERS];
  std::string           m_nnPostFilterSEICharacteristicsPrompt[MAX_NUM_NN_POST_FILTERS];
  std::vector<bool>     m_nnPostFilterSEICharacteristicsInputPicOutputFlag[MAX_NUM_NN_POST_FILTERS];
  bool                  m_nnPostFilterSEICharacteristicsAbsentInputPicZeroFlag[MAX_NUM_NN_POST_FILTERS];
#if JVET_AK0326_NNPF_SEED
  bool                  m_nnPostFilterSEICharacteristicsInbandSeedFlag[MAX_NUM_NN_POST_FILTERS];
  uint32_t              m_nnPostFilterSEICharacteristicsSeed[MAX_NUM_NN_POST_FILTERS];
#endif
  bool                    m_nnPostFilterSEIActivationCancelFlag;
  bool                    m_nnPostFilterSEIActivationTargetBaseFlag;
  bool                    m_nnPostFilterSEIActivationNoPrevCLVSFlag;
  bool                    m_nnPostFilterSEIActivationNoFollCLVSFlag;
  bool                    m_nnPostFilterSEIActivationPersistenceFlag;
  std::vector<bool>       m_nnPostFilterSEIActivationOutputFlag;
#if JVET_AJ0104_NNPFA_PROMPT_UPDATE
  bool                  m_nnPostFilterSEIActivationPromptUpdateFlag;
  std::string           m_nnPostFilterSEIActivationPrompt;
#endif
#if JVET_AK0326_NNPF_SEED
  bool                  m_nnPostFilterSEIActivationSeedUpdateFlag;
  uint32_t              m_nnPostFilterSEIActivationSeed;
#endif
#if JVET_AJ0114_NNPFA_NUM_PIC_SHIFT
#if JVET_AL0075_NNPFA_SELECTED_INPUT_FLAG
  bool                  m_nnPostFilterSEIActivationSelectedInputFlag;
#endif
  uint32_t              m_nnPostFilterSEIActivationNumInputPicShift;
#endif 

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
  std::vector<uint16_t>  m_poSEINumOfPrefixBits;
  std::vector<std::vector<uint8_t>> m_poSEIPrefixByte;
#if JVET_AJ0105_SPO_COMPLEXITY_INFO
  bool                  m_poSEIComplexityInfoPresentFlag;
  uint32_t              m_poSEIParameterTypeIdc;
  uint32_t              m_poSEILog2ParameterBitLengthMinus3;
  uint32_t              m_poSEINumParametersIdc;
  uint32_t              m_poSEINumKmacOperationIdc;
  uint32_t              m_poSEITotalKilobyteSize;
#endif

  uint32_t                   m_SEITextDescriptionID;
  bool                       m_SEITextCancelFlag;
  bool                       m_SEITextPersistenceFlag;
  uint32_t                   m_SEITextDescriptionPurpose;
  uint32_t                   m_SEITextNumStringsMinus1;
  std::vector<std::string>   m_SEITextDescriptionStringLang;
  std::vector<std::string>   m_SEITextDescriptionString;

  bool                 m_postFilterHintSEIEnabled;
  bool                 m_postFilterHintSEICancelFlag;
  bool                 m_SEITextIDCancelFlag;
  bool                 m_postFilterHintSEIPersistenceFlag;
  uint32_t             m_postFilterHintSEISizeY;
  uint32_t             m_postFilterHintSEISizeX;
  uint32_t             m_postFilterHintSEIType;
  bool                 m_postFilterHintSEIChromaCoeffPresentFlag;
  std::vector<int32_t> m_postFilterHintValues;

  bool                  m_constrainedRaslEncoding;

  bool                  m_sampleAspectRatioInfoSEIEnabled;
  bool                  m_sariCancelFlag;
  bool                  m_sariPersistenceFlag;
  int                   m_sariAspectRatioIdc;
  int                   m_sariSarWidth;
  int                   m_sariSarHeight;

  bool      m_SEIManifestSEIEnabled;
  bool      m_SEIPrefixIndicationSEIEnabled;
  bool                  m_phaseIndicationSEIEnabledFullResolution;
  int                   m_piHorPhaseNumFullResolution;
  int                   m_piHorPhaseDenMinus1FullResolution;
  int                   m_piVerPhaseNumFullResolution;
  int                   m_piVerPhaseDenMinus1FullResolution;
  bool                  m_phaseIndicationSEIEnabledReducedResolution;
  int                   m_piHorPhaseNumReducedResolution;
  int                   m_piHorPhaseDenMinus1ReducedResolution;
  int                   m_piVerPhaseNumReducedResolution;
  int                   m_piVerPhaseDenMinus1ReducedResolution;

  bool      m_MCTSEncConstraint;

  // weighted prediction
  bool      m_useWeightedPred;                    ///< Use of weighted prediction in P slices
  bool      m_useWeightedBiPred;                  ///< Use of bi-directional weighted prediction in B slices
  WeightedPredictionMethod m_weightedPredictionMethod;

  uint32_t      m_log2ParallelMergeLevel;                         ///< Parallel merge estimation region
  uint32_t      m_maxNumMergeCand;                                ///< Max number of merge candidates
  uint32_t      m_maxNumAffineMergeCand;                          ///< Max number of affine merge candidates
  uint32_t      m_maxNumGeoCand;
  uint32_t      m_maxNumIBCMergeCand;                             ///< Max number of IBC merge candidates

  bool      m_sliceLevelRpl;                                      ///< code reference picture lists in slice headers rather than picture header
  bool      m_sliceLevelDblk;                                     ///< code deblocking filter parameters in slice headers rather than picture header
  bool      m_sliceLevelSao;                                      ///< code SAO parameters in slice headers rather than picture header
  bool      m_sliceLevelAlf;                                      ///< code ALF parameters in slice headers rather than picture header
  bool      m_sliceLevelWp;                                       ///< code weighted prediction parameters in slice headers rather than picture header
  bool      m_sliceLevelDeltaQp;                                  ///< code delta in slice headers rather than picture header

  int       m_TMVPModeId;
  bool      m_depQuantEnabledFlag;
  bool      m_signDataHidingEnabledFlag;

  // Rate control
  bool     m_rcEnableRateControl;      // enable rate control or not
  int      m_rcTargetBitrate;          // target bitrate when rate control is enabled
  int      m_rcKeepHierarchicalBit;    // 0: equal bit allocation; 1: fixed ratio bit allocation; 2: adaptive ratio bit
                                       // allocation
  bool     m_rcCtuLevelRateControl;    // true: CTU level rate control; false: picture level rate control
  bool     m_rcUseCtuSeparateModel;    // use separate R-lambda model at CTU level
  int      m_rcInitialQp;              // inital QP for rate control
  bool     m_rcForceIntraQp;           // force all intra picture to use initial QP or not
  bool     m_rcCpbSaturationEnabled;   // enable target bits saturation to avoid CPB overflow and underflow
  uint32_t m_rcCpbSize;                // CPB size
  double   m_rcInitialCpbFullness;     // initial CPB fullness

  ScalingListMode m_useScalingListId;                         ///< using quantization matrix
  std::string m_scalingListFileName;                          ///< quantization matrix file name
  bool      m_disableScalingMatrixForLfnstBlks;
  bool      m_disableScalingMatrixForAlternativeColourSpace;
  bool      m_scalingMatrixDesignatedColourSpace;
  CostMode  m_costMode;                                       ///< Cost mode to use
  bool      m_TSRCdisableLL;                                  ///< disable TSRC for lossless

  bool      m_recalculateQPAccordingToLambda;                 ///< recalculate QP value according to the lambda value

  bool      m_DCIEnabled;                                     ///< enable Decoding Capability Information (DCI)
  bool      m_hrdParametersPresentFlag;                       ///< enable generation of HRD parameters
  bool      m_vuiParametersPresentFlag;                       ///< enable generation of VUI parameters
  bool      m_writeVuiHrdFromY4m;                             ///< allow writing VUI and HRD information from input Y4M file
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
  int       m_ImvMode;                                        ///< imv mode
  int       m_Imv4PelFast;                                    ///< imv 4-Pel fast mode

  bool      m_siiSEIEnabled;
  uint32_t  m_siiSEINumUnitsInShutterInterval;
  uint32_t  m_siiSEITimeScale;
  std::vector<uint32_t>     m_siiSEISubLayerNumUnitsInSI;
  bool        m_ShutterFilterEnable;                          ///< enable Pre-Filtering with Shutter Interval SEI
  std::string m_shutterIntervalPreFileName;                   ///< output Pre-Filtering video
  int         m_SII_BlendingRatio;
  void        setBlendingRatioSII(int value) { m_SII_BlendingRatio = value; }
  bool     m_sptiSEIEnabled;
  bool     m_sptiSourceTimingEqualsOutputTimingFlag;
  uint32_t m_sptiSourceType;
  uint32_t m_sptiTimeScale;
  uint32_t m_sptiNumUnitsInElementalInterval;
  bool     m_sptiDirectionFlag;
#if GREEN_METADATA_SEI_ENABLED
public:
  std::string getGMFAFile ();
  bool getGMFAUsage();
protected:
  std::string   m_GMFAFile;
  bool          m_GMFA;
  
  int      m_greenMetadataType;
  int      m_greenMetadataExtendedRepresentation;
  int      m_greenMetadataGranularityType;
  int      m_greenMetadataPeriodType;
  int      m_greenMetadataPeriodNumSeconds;
  int      m_greenMetadataPeriodNumPictures;

  int      m_xsdNumberMetrics;
  bool     m_xsdMetricTypePSNR;
  bool     m_xsdMetricTypeSSIM;
  bool     m_xsdMetricTypeWPSNR;
  bool     m_xsdMetricTypeWSPSNR;

#if GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464
  uint32_t                          m_greenMetadataAMIFlags              = 0;
  bool                              m_greenMetadataAMICancelFlag         = false;
  bool                              m_greenMetadataAMIGlobalFlag         = false;
  bool                              m_greenMetadataAMIApproxFlag         = false;
  bool                              m_greenMetadataAMIPreprocFlag        = false;
  bool                              m_greenMetadataAMIQualityFlag        = false;
  bool                              m_greenMetadataAMIBacklightFlag      = false;
  uint32_t                          m_greenMetadataAMIDisplayModel       = 0;
  uint32_t                          m_greenMetadataAMIApproximationModel = 0;
  uint32_t                          m_greenMetadataAMIMapNumber          = 0;
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
  std::string m_summaryOutFilename;                           ///< filename to use for producing summary output file.
  std::string m_summaryPicFilenameBase;                       ///< Base filename to use for producing summary picture output files. The actual filenames used will have I.txt, P.txt and B.txt appended.
  uint32_t        m_summaryVerboseness;                           ///< Specifies the level of the verboseness of the text output.

  int         m_verbosity;

  std::string m_decodeBitstreams[2];                          ///< filename for decode bitstreams.
  int         m_debugCTU;
  int         m_switchPOC;                                    ///< dbg poc.
  int         m_switchDQP;                                    ///< switch DQP.
  int         m_fastForwardToPOC;                             ///< get to encoding the specified POC as soon as possible by skipping temporal layers irrelevant for the specified POC
  bool        m_stopAfterFFtoPOC;
  bool        m_bs2ModPOCAndType;
  bool        m_forceDecodeBitstream1;

  int         m_maxNumAlfAps{ ALF_CTB_MAX_NUM_APS };
  int         m_alfapsIDShift;
  int         m_constantJointCbCrSignFlag;
  bool        m_alf;                                       ///< Adaptive Loop Filter
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

  bool        m_rprEnabledFlag;
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
  bool        m_resChangeInClvsEnabled;
  bool        m_refMetricsEnabled;
  double      m_fractionOfFrames;                             ///< encode a fraction of the frames as specified in FramesToBeEncoded
  int         m_switchPocPeriod;
  int         m_upscaledOutput;                               ////< Output upscaled (2), decoded cropped but in full resolution buffer (1) or decoded cropped (0, default) picture for RPR.
  int         m_upscaledOutputWidth;
  int         m_upscaledOutputHeight;
  int         m_upscaleFilterForDisplay;
  bool        m_craAPSreset;
  bool        m_rprRASLtoolSwitch;
  bool        m_avoidIntraInDepLayer;

  bool                  m_gopBasedTemporalFilterEnabled;
  int                   m_gopBasedTemporalFilterPastRefs;
  int                   m_gopBasedTemporalFilterFutureRefs;
  std::map<int, double> m_gopBasedTemporalFilterStrengths;             ///< Filter strength per frame for the GOP-based Temporal Filter
  bool                  m_bimEnabled;
  bool                  m_dpfEnabled;
  int                   m_dpfKeyLen;
  int                   m_dpfNonkeyLen;

  int         m_maxLayers;
  int         m_targetOlsIdx;
  bool        m_OPIEnabled;                                     ///< enable Operating Point Information (OPI)
  int         m_maxTemporalLayer;
  int         m_layerId[MAX_VPS_LAYERS];
  int         m_maxSublayers;
  bool        m_defaultPtlDpbHrdMaxTidFlag;
  bool        m_allIndependentLayersFlag;
  std::string m_predDirectionArray;
  bool        m_explicitILRP;
  bool        m_encILOpt;
  double      m_encILOptLambdaModifier;

  int         m_numRefLayers[MAX_VPS_LAYERS];
  std::string m_refLayerIdxStr[MAX_VPS_LAYERS];
  bool        m_eachLayerIsAnOlsFlag;
  int         m_olsModeIdc;
  int         m_numOutputLayerSets;
  std::string m_olsOutputLayerStr[MAX_VPS_LAYERS];
  std::string m_maxTidILRefPicsPlus1Str[MAX_VPS_LAYERS];
  bool        m_rplOfDepLayerInSh;

  int         m_numPtlsInVps;
  int         m_ptPresentInPtl[MAX_NUM_OLSS];

  EncCfgParam::CfgVPSParameters m_cfgVPSParameters;
  Level::Name m_levelPtl[MAX_NUM_OLSS];
  int         m_olsPtlIdx[MAX_NUM_OLSS];

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
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEINumMatricestoNumKpsFlag;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEINumMatricesInfo;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIMatrix3DSpaceFlag;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEINumMatrices;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIMatrixWidth;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIMatrixHeight;
  std::vector < std::vector<std::vector<std::vector<std::vector<double>>>>>   m_generativeFaceVideoSEIMatrixElement;
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
  std::vector<bool>                    m_generativeFaceVideoEnhancementSEIMatrixPresentFlag;
  std::vector<bool>                    m_generativeFaceVideoEnhancementSEIMatrixPredFlag;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor;
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

#if EXTENSION_360_VIDEO
  TExt360AppEncCfg m_ext360;
  friend class TExt360AppEncCfg;
  friend class TExt360AppEncTop;
#endif

#if JVET_O0756_CONFIG_HDRMETRICS || JVET_O0756_CALCULATE_HDRMETRICS
#if JVET_O0756_CALCULATE_HDRMETRICS
  double      m_whitePointDeltaE[hdrtoolslib::NB_REF_WHITE];
#else
  double      m_whitePointDeltaE[3];
#endif
  double      m_maxSampleValue;
  int         m_sampleRange;
  int         m_colorPrimaries;
  bool        m_enableTFunctionLUT;
  int         m_chromaLocation;
  int         m_chromaUPFilter;
  int         m_cropOffsetLeft;
  int         m_cropOffsetTop;
  int         m_cropOffsetRight;
  int         m_cropOffsetBottom;
  bool        m_calculateHdrMetrics;
#endif

#if  JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
  bool        m_aurSEIEnabled;
  bool        m_aurSEICancelFlag;
  bool        m_aurSEIPersistenceFlag;
  uint32_t    m_aurSEINumRestrictionsMinus1;
  std::vector<uint32_t>  m_aurSEIRestrictions;
  std::vector<bool>      m_aurSEIContextPresentFlag;
  std::vector<uint32_t>  m_aurSEIContext;
#endif

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
  bool     m_priSEIMultilayerFlag;
  std::vector<uint32_t> m_priSEIRegionLayerId;
  std::vector<bool>     m_priSEIRegionIsALayerFlag;
#endif
#endif

  // internal member functions
  bool  xCheckParameter ();                                   ///< check validity of configuration values
  void  xPrintParameter ();                                   ///< print configuration values
  void  xPrintUsage     ();                                   ///< print usage
  bool  xHasNonZeroTemporalID();                             ///< check presence of constant temporal ID in GOP structure
  bool  xHasLeadingPicture();                                 ///< check presence of leading pictures in GOP structure
  int   xAutoDetermineProfile();                              ///< auto determine the profile to use given the other configuration settings. Returns 1 if erred. Can select profile 'NONE'
public:
  EncAppCfg();
  virtual ~EncAppCfg();

public:
  void  create    ();                                         ///< create option handling class
  void  destroy   ();                                         ///< destroy option handling class
  bool  parseCfg  ( int argc, char* argv[] );                ///< parse configuration file to fill member variables
};

//! \}

#endif // __ENCAPPCFG__

