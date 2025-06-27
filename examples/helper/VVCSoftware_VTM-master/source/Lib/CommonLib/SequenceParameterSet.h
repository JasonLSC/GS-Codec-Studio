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

#pragma once

#include <map>
#include "HRD.h"
#include "ProfileTierLevel.h"
#include "ChromaFormat.h"

class VUI
{
private:
  bool m_progressiveSourceFlag;
  bool m_interlacedSourceFlag;
  bool m_nonPackedFlag;
  bool m_nonProjectedFlag;
  bool m_aspectRatioInfoPresentFlag;
  bool m_aspectRatioConstantFlag;
  int  m_aspectRatioIdc;
  int  m_sarWidth;
  int  m_sarHeight;
  bool m_overscanInfoPresentFlag;
  bool m_overscanAppropriateFlag;
  bool m_colourDescriptionPresentFlag;
  int  m_colourPrimaries;
  int  m_transferCharacteristics;
  int  m_matrixCoefficients;
  bool m_videoFullRangeFlag;
  bool             m_chromaLocInfoPresentFlag       = false;
  Chroma420LocType m_chromaSampleLocTypeTopField    = Chroma420LocType::UNSPECIFIED;
  Chroma420LocType m_chromaSampleLocTypeBottomField = Chroma420LocType::UNSPECIFIED;
  Chroma420LocType m_chromaSampleLocType            = Chroma420LocType::UNSPECIFIED;

public:
  VUI()
    : m_progressiveSourceFlag(false)   // Default values as documented in VVC D10 are used
    , m_interlacedSourceFlag(false)
    , m_nonPackedFlag(false)
    , m_nonProjectedFlag(false)
    , m_aspectRatioInfoPresentFlag(false)
    , m_aspectRatioConstantFlag(false)
    , m_aspectRatioIdc(0)
    , m_sarWidth(0)
    , m_sarHeight(0)
    , m_overscanInfoPresentFlag(false)
    , m_overscanAppropriateFlag(false)
    , m_colourDescriptionPresentFlag(false)
    , m_colourPrimaries(2)
    , m_transferCharacteristics(2)
    , m_matrixCoefficients(2)
    , m_videoFullRangeFlag(false)
  {}

  virtual ~VUI() {}

  bool getAspectRatioInfoPresentFlag() const     { return m_aspectRatioInfoPresentFlag; }

  void setAspectRatioInfoPresentFlag(bool i)     { m_aspectRatioInfoPresentFlag = i; }
  bool getAspectRatioConstantFlag() const        { return m_aspectRatioConstantFlag; }

  void setAspectRatioConstantFlag(bool b)        { m_aspectRatioConstantFlag = b; }
  int  getAspectRatioIdc() const                 { return m_aspectRatioIdc; }

  void setAspectRatioIdc(int i)                  { m_aspectRatioIdc = i;}

  int  getSarWidth() const                       { return m_sarWidth; }
  void setSarWidth(int i)                        { m_sarWidth = i; }

  int  getSarHeight() const                      { return m_sarHeight; }
  void setSarHeight(int i)                       { m_sarHeight = i; }

  bool getColourDescriptionPresentFlag() const   { return m_colourDescriptionPresentFlag; }
  void setColourDescriptionPresentFlag(bool i)   { m_colourDescriptionPresentFlag = i; }

  int  getColourPrimaries() const                { return m_colourPrimaries; }
  void setColourPrimaries(int i)                 { m_colourPrimaries = i; }

  int  getTransferCharacteristics() const        { return m_transferCharacteristics;     }
  void setTransferCharacteristics(int i)         { m_transferCharacteristics = i;        }

  int  getMatrixCoefficients() const             { return m_matrixCoefficients; }
  void setMatrixCoefficients(int i)              { m_matrixCoefficients = i; }

  bool getProgressiveSourceFlag() const          { return m_progressiveSourceFlag; }
  void setProgressiveSourceFlag(bool b)          { m_progressiveSourceFlag = b; }

  bool getInterlacedSourceFlag() const           { return m_interlacedSourceFlag; }
  void setInterlacedSourceFlag(bool b)           { m_interlacedSourceFlag = b; }

  bool getNonPackedFlag() const                  { return m_nonPackedFlag; }
  void setNonPackedFlag(bool b)                  { m_nonPackedFlag = b; }

  bool getNonProjectedFlag() const               { return m_nonProjectedFlag; }
  void setNonProjectedFlag(bool b)               { m_nonProjectedFlag = b; }

  bool getChromaLocInfoPresentFlag() const       { return m_chromaLocInfoPresentFlag; }
  void setChromaLocInfoPresentFlag(bool i)       { m_chromaLocInfoPresentFlag = i; }

  Chroma420LocType getChromaSampleLocTypeTopField() const { return m_chromaSampleLocTypeTopField; }
  void             setChromaSampleLocTypeTopField(Chroma420LocType val) { m_chromaSampleLocTypeTopField = val; }

  Chroma420LocType getChromaSampleLocTypeBottomField() const { return m_chromaSampleLocTypeBottomField; }
  void             setChromaSampleLocTypeBottomField(Chroma420LocType val) { m_chromaSampleLocTypeBottomField = val; }

  Chroma420LocType getChromaSampleLocType() const { return m_chromaSampleLocType; }
  void             setChromaSampleLocType(Chroma420LocType val) { m_chromaSampleLocType = val; }

  bool getOverscanInfoPresentFlag() const        { return m_overscanInfoPresentFlag; }
  void setOverscanInfoPresentFlag(bool i)        { m_overscanInfoPresentFlag = i; }

  bool getOverscanAppropriateFlag() const        { return m_overscanAppropriateFlag; }
  void setOverscanAppropriateFlag(bool i)        { m_overscanAppropriateFlag = i; }

  bool getVideoFullRangeFlag() const             { return m_videoFullRangeFlag; }
  void setVideoFullRangeFlag(bool i)             { m_videoFullRangeFlag = i; }

};

class ReferencePictureList
{
private:
  int   m_numberOfShorttermPictures;
  int   m_numberOfLongtermPictures;
  int   m_isLongtermRefPic[MAX_NUM_REF_PICS];
  int   m_refPicIdentifier[MAX_NUM_REF_PICS];  //This can be delta POC for STRP or POC LSB for LTRP
  int   m_POC[MAX_NUM_REF_PICS];
  int   m_numberOfActivePictures;
  bool  m_deltaPocMSBPresentFlag[MAX_NUM_REF_PICS];
  int   m_deltaPOCMSBCycleLT[MAX_NUM_REF_PICS];
  bool  m_ltrpInSliceHeaderFlag;
  bool  m_interLayerPresentFlag;
  bool  m_isInterLayerRefPic[MAX_NUM_REF_PICS];
  int   m_interLayerRefPicIdx[MAX_NUM_REF_PICS];
  int   m_numberOfInterLayerPictures;

public:
  ReferencePictureList(const bool interLayerPicPresentFlag = false);
  virtual ~ReferencePictureList();

  void setRefPicIdentifier(int idx, int identifier, bool isLongterm, bool isInterLayerRefPic, int interLayerIdx);
  int  getRefPicIdentifier(int idx) const;
  bool isRefPicLongterm(int idx) const;

  void setNumberOfShorttermPictures(int n) { m_numberOfShorttermPictures = n; }
  int  getNumberOfShorttermPictures() const { return m_numberOfShorttermPictures; }

  void setNumberOfLongtermPictures(int n) { m_numberOfLongtermPictures = n; }
  int  getNumberOfLongtermPictures() const { return m_numberOfLongtermPictures; }

  void setLtrpInSliceHeaderFlag(bool flag) { m_ltrpInSliceHeaderFlag = flag; }
  bool getLtrpInSliceHeaderFlag() const { return m_ltrpInSliceHeaderFlag; }

  void setNumberOfInterLayerPictures(int numberOfIlrp) { m_numberOfInterLayerPictures = numberOfIlrp; }
  int  getNumberOfInterLayerPictures() const { return m_numberOfInterLayerPictures; }

  int  getNumRefEntries() const { return m_numberOfShorttermPictures + m_numberOfLongtermPictures + m_numberOfInterLayerPictures; }

  void setPOC(int idx, int POC);
  int  getPOC(int idx) const;

  void setNumberOfActivePictures(int numberOfLtrp);
  int  getNumberOfActivePictures() const;

  int  getDeltaPocMSBCycleLT(int i) const { return m_deltaPOCMSBCycleLT[i]; }
  void setDeltaPocMSBCycleLT(int i, int x) { m_deltaPOCMSBCycleLT[i] = x; }
  bool getDeltaPocMSBPresentFlag(int i) const { return m_deltaPocMSBPresentFlag[i]; }
  void setDeltaPocMSBPresentFlag(int i, bool x) { m_deltaPocMSBPresentFlag[i] = x; }

  void printRefPicInfo() const;

  bool getInterLayerPresentFlag()                   const { return m_interLayerPresentFlag; }
  void setInterLayerPresentFlag( bool b )                 { m_interLayerPresentFlag = b; }
  bool isInterLayerRefPic( int idx )                const { return m_isInterLayerRefPic[idx]; }
  int  getInterLayerRefPicIdx( int idx )            const { return m_interLayerRefPicIdx[idx]; }
  void setInterLayerRefPicIdx( int idx, int layerIdc )    { m_interLayerRefPicIdx[idx] = layerIdc; }
};

// Reference Picture List set class
class RPLList
{
private:
  std::vector<ReferencePictureList> m_referencePictureLists;

public:
  RPLList() {}
  virtual ~RPLList() { }

  void                           create(int numberOfEntries) { m_referencePictureLists.resize(numberOfEntries); }
  void                           destroy() { }


  ReferencePictureList*          getReferencePictureList(int referencePictureListIdx) { return &m_referencePictureLists[referencePictureListIdx]; }
  const ReferencePictureList*    getReferencePictureList(int referencePictureListIdx) const { return &m_referencePictureLists[referencePictureListIdx]; }

  int                            getNumberOfReferencePictureLists() const { return int(m_referencePictureLists.size()); }
};

class Window
{
private:
  int m_leftOffset   = 0;
  int m_rightOffset  = 0;
  int m_topOffset    = 0;
  int m_bottomOffset = 0;

public:
  Window() = default;
  Window(int offsetLeft, int offsetRight, int offsetTop, int offsetBottom)
    : m_leftOffset(offsetLeft), m_rightOffset(offsetRight), m_topOffset(offsetTop), m_bottomOffset(offsetBottom)
  {}
  Window(const Window& w) = default;

  bool operator==(const Window& w) const
  {
    return m_leftOffset == w.m_leftOffset && m_rightOffset == w.m_rightOffset && m_topOffset == w.m_topOffset
           && m_bottomOffset == w.m_bottomOffset;
  }

  bool operator!=(const Window& w) const { return !(*this == w); }

  bool isZero() const { return m_leftOffset == 0 && m_rightOffset == 0 && m_topOffset == 0 && m_bottomOffset == 0; }

  int getWindowLeftOffset() const { return m_leftOffset; }
  int getWindowRightOffset() const { return m_rightOffset; }
  int getWindowTopOffset() const { return m_topOffset; }
  int getWindowBottomOffset() const { return m_bottomOffset; }

  void setWindowLeftOffset(int val) { m_leftOffset = val; }
  void setWindowRightOffset(int val) { m_rightOffset = val; }
  void setWindowTopOffset(int val) { m_topOffset = val; }
  void setWindowBottomOffset(int val) { m_bottomOffset = val; }
};

struct ChromaQpMappingTableParams
{
  int               m_qpBdOffset;
  bool              m_sameCQPTableForAllChromaFlag;
  int               m_numQpTables;
  int               m_qpTableStartMinus26[MAX_NUM_CQP_MAPPING_TABLES];
  int               m_numPtsInCQPTableMinus1[MAX_NUM_CQP_MAPPING_TABLES];
  std::vector<int>  m_deltaQpInValMinus1[MAX_NUM_CQP_MAPPING_TABLES];
  std::vector<int>  m_deltaQpOutVal[MAX_NUM_CQP_MAPPING_TABLES];

  ChromaQpMappingTableParams()
  {
    m_qpBdOffset = 12;
    m_sameCQPTableForAllChromaFlag = true;
    m_numQpTables = 1;
    m_numPtsInCQPTableMinus1[0] = 0;
    m_qpTableStartMinus26[0] = 0;
    m_deltaQpInValMinus1[0] = { 0 };
    m_deltaQpOutVal[0] = { 0 };
  }

  void setSameCQPTableForAllChromaFlag(bool b)                             { m_sameCQPTableForAllChromaFlag = b; }
  bool getSameCQPTableForAllChromaFlag()                             const { return m_sameCQPTableForAllChromaFlag; }
  void setNumQpTables(int n)                                               { m_numQpTables = n; }
  int  getNumQpTables()                                              const { return m_numQpTables; }
  void setQpTableStartMinus26(int tableIdx, int n)                         { m_qpTableStartMinus26[tableIdx] = n; }
  int  getQpTableStartMinus26(int tableIdx)                          const { return m_qpTableStartMinus26[tableIdx]; }
  void setNumPtsInCQPTableMinus1(int tableIdx, int n)                      { m_numPtsInCQPTableMinus1[tableIdx] = n; }
  int  getNumPtsInCQPTableMinus1(int tableIdx)                       const { return m_numPtsInCQPTableMinus1[tableIdx]; }
  void setDeltaQpInValMinus1(int tableIdx, std::vector<int> &inVals)       { m_deltaQpInValMinus1[tableIdx] = inVals; }
  void setDeltaQpInValMinus1(int tableIdx, int idx, int n)                 { m_deltaQpInValMinus1[tableIdx][idx] = n; }
  int  getDeltaQpInValMinus1(int tableIdx, int idx)                  const { return m_deltaQpInValMinus1[tableIdx][idx]; }
  void setDeltaQpOutVal(int tableIdx, std::vector<int> &outVals)           { m_deltaQpOutVal[tableIdx] = outVals; }
  void setDeltaQpOutVal(int tableIdx, int idx, int n)                      { m_deltaQpOutVal[tableIdx][idx] = n; }
  int  getDeltaQpOutVal(int tableIdx, int idx)                       const { return m_deltaQpOutVal[tableIdx][idx]; }
};

struct ChromaQpMappingTable : ChromaQpMappingTableParams
{
  std::map<int, int> m_chromaQpMappingTables[MAX_NUM_CQP_MAPPING_TABLES];

  int  getMappedChromaQpValue(ComponentID compID, const int qpVal)  const { return m_chromaQpMappingTables[m_sameCQPTableForAllChromaFlag ? 0 : (int)compID - 1].at(qpVal); }
  void deriveChromaQPMappingTables();
  void setParams(const ChromaQpMappingTableParams &params, const int qpBdOffset);
};

// SPS RExt class
class SPSRExt // Names aligned to text specification
{
private:
  bool m_transformSkipRotationEnabledFlag;
  bool m_transformSkipContextEnabledFlag;
  bool m_extendedPrecisionProcessingFlag;
  bool m_tsrcRicePresentFlag;
  bool m_highPrecisionOffsetsEnabledFlag;
  bool m_rrcRiceExtensionEnableFlag;
  bool m_persistentRiceAdaptationEnabledFlag;
  bool m_reverseLastSigCoeffEnabledFlag;
  bool m_cabacBypassAlignmentEnabledFlag;

public:
  SPSRExt();

  bool settingsDifferFromDefaults() const
  {
    return getTransformSkipRotationEnabledFlag()
        || getTransformSkipContextEnabledFlag()
        || getExtendedPrecisionProcessingFlag()
        || getTSRCRicePresentFlag()
        || getHighPrecisionOffsetsEnabledFlag()
        || getRrcRiceExtensionEnableFlag()
        || getPersistentRiceAdaptationEnabledFlag()
        || getReverseLastSigCoeffEnabledFlag()
        || getCabacBypassAlignmentEnabledFlag();
  }


  bool getTransformSkipRotationEnabledFlag() const        { return m_transformSkipRotationEnabledFlag;     }
  void setTransformSkipRotationEnabledFlag(bool value)    { m_transformSkipRotationEnabledFlag = value;    }

  bool getTransformSkipContextEnabledFlag() const         { return m_transformSkipContextEnabledFlag;      }
  void setTransformSkipContextEnabledFlag(bool value)     { m_transformSkipContextEnabledFlag = value;     }

  bool getExtendedPrecisionProcessingFlag() const         { return m_extendedPrecisionProcessingFlag;      }
  void setExtendedPrecisionProcessingFlag(bool value)     { m_extendedPrecisionProcessingFlag = value;     }

  bool getTSRCRicePresentFlag() const                     { return m_tsrcRicePresentFlag;                  }
  void setTSRCRicePresentFlag(bool b)                     { m_tsrcRicePresentFlag = b;                     }

  bool getHighPrecisionOffsetsEnabledFlag() const         { return m_highPrecisionOffsetsEnabledFlag;      }
  void setHighPrecisionOffsetsEnabledFlag(bool value)     { m_highPrecisionOffsetsEnabledFlag = value;     }

  bool getRrcRiceExtensionEnableFlag() const              { return m_rrcRiceExtensionEnableFlag; }
  void setRrcRiceExtensionEnableFlag(bool value)          { m_rrcRiceExtensionEnableFlag = value; }

  bool getPersistentRiceAdaptationEnabledFlag() const     { return m_persistentRiceAdaptationEnabledFlag;  }
  void setPersistentRiceAdaptationEnabledFlag(bool value) { m_persistentRiceAdaptationEnabledFlag = value; }

  bool getReverseLastSigCoeffEnabledFlag() const          { return m_reverseLastSigCoeffEnabledFlag;       }
  void setReverseLastSigCoeffEnabledFlag(bool value)      { m_reverseLastSigCoeffEnabledFlag = value;      }

  bool getCabacBypassAlignmentEnabledFlag() const         { return m_cabacBypassAlignmentEnabledFlag;      }
  void setCabacBypassAlignmentEnabledFlag(bool value)     { m_cabacBypassAlignmentEnabledFlag = value;     }
};


// SPS class
class SPS
{
private:
  int               m_spsId;
  int               m_vpsId;
  int               m_layerId;
  bool              m_affineAmvrEnabledFlag;
  bool              m_DMVR;
  bool              m_MMVD;
  bool              m_SBT;
  bool              m_ISP;
  ChromaFormat      m_chromaFormatIdc = ChromaFormat::_420;

  uint32_t          m_maxSubLayers;           // maximum number of temporal layers

  bool              m_ptlDpbHrdParamsPresentFlag;
  bool              m_subLayerDpbParamsFlag;

  // Structure
  uint32_t              m_maxWidthInLumaSamples;
  uint32_t              m_maxHeightInLumaSamples;
  Window                m_conformanceWindow;

  bool                  m_subPicInfoPresentFlag;             // indicates the presence of sub-picture info
  uint32_t              m_numSubPics;                        // number of sub-pictures used
  bool                  m_independentSubPicsFlag;
  bool                  m_subPicSameSizeFlag;
  std::vector<uint32_t> m_subPicCtuTopLeftX;
  std::vector<uint32_t> m_subPicCtuTopLeftY;
  std::vector<uint32_t> m_subPicWidth;
  std::vector<uint32_t> m_subPicHeight;
  std::vector<bool>     m_subPicTreatedAsPicFlag;
  std::vector<bool>     m_loopFilterAcrossSubpicEnabledFlag;
  bool                  m_subPicIdMappingExplicitlySignalledFlag;
  bool                  m_subPicIdMappingPresentFlag;
  uint32_t              m_subPicIdLen;                       // sub-picture ID length in bits
  std::vector<uint16_t> m_subPicId;                          // sub-picture ID for each sub-picture in the sequence

  int         m_log2MinCodingBlockSize;
  unsigned    m_CTUSize;
  unsigned    m_partitionOverrideEnalbed;   // enable partition constraints override function
  unsigned    m_minQT[3];                   // 0: I slice luma; 1: P/B slice; 2: I slice chroma
  unsigned    m_maxMTTHierarchyDepth[3];
  unsigned    m_maxBTSize[3];
  unsigned    m_maxTTSize[3];
  bool        m_idrRefParamList;
  unsigned    m_dualITree;
  uint32_t    m_maxCuWidth;
  uint32_t    m_maxCuHeight;

  RPLList  m_rplList[NUM_REF_PIC_LIST_01];
  uint32_t m_numRpl[NUM_REF_PIC_LIST_01];

  bool              m_rpl1CopyFromRpl0Flag;
  bool              m_rpl1IdxPresentFlag;
  bool              m_allRplEntriesHasSameSignFlag;
  bool              m_longTermRefsPresent;
  bool              m_temporalMvpEnabledFlag;
  int               m_maxNumReorderPics[MAX_TLAYER];

  // Tool list

  bool              m_transformSkipEnabledFlag;
  int               m_log2MaxTransformSkipBlockSize;
  bool              m_bdpcmEnabledFlag;
  bool              m_jointCbCrEnabledFlag;
  // Parameter
  BitDepths         m_bitDepths;
  bool              m_entropyCodingSyncEnabledFlag;     // Flag for enabling WPP
  bool              m_entryPointPresentFlag;            // Flag for indicating the presence of entry points
  int               m_qpBDOffset[MAX_NUM_CHANNEL_TYPE];
  BitDepths         m_internalMinusInputBitDepth;       //  max(0, internal bitdepth - input bitdepth)

  bool              m_sbtmvpEnabledFlag;
  bool              m_bdofEnabledFlag;
  bool              m_fpelMmvdEnabledFlag;
  bool              m_bdofControlPresentInPhFlag;
  bool              m_dmvrControlPresentInPhFlag;
  bool              m_profControlPresentInPhFlag;
  uint32_t          m_bitsForPoc;
  bool              m_pocMsbCycleFlag;
  uint32_t          m_pocMsbCycleLen;
  int               m_numExtraPHBytes;
  int               m_numExtraSHBytes;

  std::vector<bool> m_extraPHBitPresentFlag;
  std::vector<bool> m_extraSHBitPresentFlag;
  uint32_t          m_numLongTermRefPicSPS;
  uint32_t          m_ltRefPicPocLsbSps[MAX_NUM_LONG_TERM_REF_PICS];
  bool              m_usedByCurrPicLtSPSFlag[MAX_NUM_LONG_TERM_REF_PICS];
  uint32_t          m_log2MaxTbSize;
  bool              m_useWeightPred;                  // Use of Weighting Prediction (P_SLICE)
  bool              m_useWeightedBiPred;              // Use of Weighting Bi-Prediction (B_SLICE)

  bool              m_saoEnabledFlag;

  bool              m_temporalIdNestingFlag;          // temporal_id_nesting_flag

  bool              m_scalingListEnabledFlag;
  bool              m_depQuantEnabledFlag;            // dependent quantization enabled flag
  bool              m_signDataHidingEnabledFlag;      // sign data hiding enabled flag
  bool              m_virtualBoundariesEnabledFlag;   // Enable virtual boundaries tool
  bool              m_virtualBoundariesPresentFlag;   // disable loop filtering across virtual boundaries
  unsigned          m_numVerVirtualBoundaries;        // number of vertical virtual boundaries
  unsigned          m_numHorVirtualBoundaries;        // number of horizontal virtual boundaries
  unsigned          m_virtualBoundariesPosX[3];       // horizontal position of each vertical virtual boundary
  unsigned          m_virtualBoundariesPosY[3];       // vertical position of each horizontal virtual boundary
  uint32_t          m_maxDecPicBuffering[MAX_TLAYER];
  uint32_t          m_maxLatencyIncreasePlus1[MAX_TLAYER];

  bool              m_generalHrdParametersPresentFlag;
  GeneralHrdParams  m_generalHrdParams;
  OlsHrdParams      m_olsHrdParams[8];

  bool              m_fieldSeqFlag;
  bool              m_vuiParametersPresentFlag;
  unsigned          m_vuiPayloadSize;
  VUI               m_vuiParameters;

  SPSRExt           m_spsRangeExtension;

  ProfileTierLevel  m_profileTierLevel;

  bool              m_alfEnabledFlag;
  bool              m_ccalfEnabledFlag;
  bool              m_alfOptEnabledFlag;
  bool              m_wrapAroundEnabledFlag;
  bool              m_ibcFlag;
  bool              m_useColorTrans;
  unsigned          m_PLTMode;

  bool              m_lmcsEnabled;
  bool              m_AMVREnabledFlag;
  bool              m_LMChroma;
  bool              m_horCollocatedChromaFlag;
  bool              m_verCollocatedChromaFlag;
  bool              m_mtsEnabled{ false };
  bool              m_explicitMtsIntra{ false };
  bool              m_explicitMtsInter{ false };
  bool              m_LFNST;
  bool              m_SMVD;
  bool              m_Affine;
  bool              m_AffineType;
  bool              m_PROF;
  bool              m_bcw;
  bool              m_ciip;
  bool              m_Geo;

  bool              m_ladfEnabled;
  int               m_ladfNumIntervals;
  int               m_ladfQpOffset[MAX_LADF_INTERVALS];
  int               m_ladfIntervalLowerBound[MAX_LADF_INTERVALS];

  bool              m_MRL;
  bool              m_MIP;

  ChromaQpMappingTable m_chromaQpMappingTable;

  bool              m_GDREnabledFlag;
  bool              m_SubLayerCbpParametersPresentFlag;

  bool              m_rprEnabledFlag;
  bool              m_resChangeInClvsEnabledFlag;
  bool              m_interLayerPresentFlag;
  bool              m_gopBasedRPREnabledFlag;
  uint32_t          m_log2ParallelMergeLevelMinus2;
  bool              m_ppsValidFlag[MAX_NUM_PPS];
  Size              m_scalingWindowSizeInPPS[MAX_NUM_PPS];
  uint32_t          m_maxNumMergeCand;
  uint32_t          m_maxNumAffineMergeCand;
  uint32_t          m_maxNumIBCMergeCand;
  uint32_t          m_maxNumGeoCand;
  bool              m_scalingMatrixAlternativeColourSpaceDisabledFlag;
  bool              m_scalingMatrixDesignatedColourSpaceFlag;

  bool              m_disableScalingMatrixForLfnstBlks;

public:

  SPS();
  virtual ~SPS();

  int  getSPSId() const { return m_spsId; }
  void setSPSId(int val) { m_spsId = val; }

  int  getVPSId() const { return m_vpsId; }
  void setVPSId(int val) { m_vpsId = val; }

  void          setLayerId( int i )                                   { m_layerId = i; }
  int           getLayerId() const                                    { return m_layerId; }
  ChromaFormat  getChromaFormatIdc () const                           { return m_chromaFormatIdc; }
  void          setChromaFormatIdc (ChromaFormat i)                   { m_chromaFormatIdc = i; }

  // structure
  void          setMaxPicWidthInLumaSamples( uint32_t u )             { m_maxWidthInLumaSamples = u; }
  uint32_t      getMaxPicWidthInLumaSamples() const                   { return  m_maxWidthInLumaSamples; }
  void          setMaxPicHeightInLumaSamples( uint32_t u )            { m_maxHeightInLumaSamples = u; }
  uint32_t      getMaxPicHeightInLumaSamples() const                  { return  m_maxHeightInLumaSamples; }
  Window&       getConformanceWindow()                                { return  m_conformanceWindow; }
  const Window& getConformanceWindow() const                          { return  m_conformanceWindow; }
  void          setConformanceWindow(const Window& conformanceWindow) { m_conformanceWindow = conformanceWindow; }

  void      setSubPicInfoPresentFlag(bool b)                    { m_subPicInfoPresentFlag = b;            }
  bool      getSubPicInfoPresentFlag() const                    { return m_subPicInfoPresentFlag;         }
  void      setNumSubPics( uint32_t u );
  void      setIndependentSubPicsFlag(bool b)                   { m_independentSubPicsFlag = b;                   }
  bool      getIndependentSubPicsFlag() const                   { return m_independentSubPicsFlag;                }
  void      setSubPicSameSizeFlag(bool b)                       { m_subPicSameSizeFlag = b;                       }
  bool      getSubPicSameSizeFlag() const                       { return m_subPicSameSizeFlag;                    }
  uint32_t  getNumSubPics() const                               { return  m_numSubPics;                           }
  void      setSubPicCtuTopLeftX(int i, uint32_t u)             { m_subPicCtuTopLeftX[i] = u;                     }
  uint32_t  getSubPicCtuTopLeftX(int i) const                   { return  m_subPicCtuTopLeftX[i];                 }
  void      setSubPicCtuTopLeftY(int i, uint32_t u)             { m_subPicCtuTopLeftY[i] = u;                     }
  uint32_t  getSubPicCtuTopLeftY(int i) const                   { return  m_subPicCtuTopLeftY[i];                 }
  void      setSubPicWidth(int i, uint32_t u)                   { m_subPicWidth[i] = u;                           }
  uint32_t  getSubPicWidth(int i) const                         { return  m_subPicWidth[i];                       }
  void      setSubPicHeight(int i, uint32_t u)                  { m_subPicHeight[i] = u;                          }
  uint32_t  getSubPicHeight(int i) const                        { return  m_subPicHeight[i];                      }
  void      setSubPicTreatedAsPicFlag(int i, bool u)            { m_subPicTreatedAsPicFlag[i] = u;                }
  bool      getSubPicTreatedAsPicFlag(int i) const              { return  m_subPicTreatedAsPicFlag[i];            }
  void      setLoopFilterAcrossSubpicEnabledFlag(int i, bool u) { m_loopFilterAcrossSubpicEnabledFlag[i] = u;     }
  bool      getLoopFilterAcrossSubpicEnabledFlag(int i) const   { return  m_loopFilterAcrossSubpicEnabledFlag[i]; }

  void      setSubPicCtuTopLeftX                 (const std::vector<uint32_t> &v)   { CHECK(v.size()!=m_numSubPics, "number of vector entries must be equal to numSubPics") ;m_subPicCtuTopLeftX = v; }
  void      setSubPicCtuTopLeftY                 (const std::vector<uint32_t> &v)   { CHECK(v.size()!=m_numSubPics, "number of vector entries must be equal to numSubPics") ;m_subPicCtuTopLeftY = v; }
  void      setSubPicWidth                       (const std::vector<uint32_t> &v)   { CHECK(v.size()!=m_numSubPics, "number of vector entries must be equal to numSubPics") ;m_subPicWidth = v; }
  void      setSubPicHeight                      (const std::vector<uint32_t> &v)   { CHECK(v.size()!=m_numSubPics, "number of vector entries must be equal to numSubPics") ;m_subPicHeight = v; }
  void      setSubPicTreatedAsPicFlag            (const std::vector<bool> &v)       { CHECK(v.size()!=m_numSubPics, "number of vector entries must be equal to numSubPics") ;m_subPicTreatedAsPicFlag = v; }
  void      setLoopFilterAcrossSubpicEnabledFlag (const std::vector<bool> &v)       { CHECK(v.size()!=m_numSubPics, "number of vector entries must be equal to numSubPics") ;m_loopFilterAcrossSubpicEnabledFlag = v; }

  bool      getDisableScalingMatrixForLfnstBlks() const { return m_disableScalingMatrixForLfnstBlks; }
  void      setDisableScalingMatrixForLfnstBlks(bool b) { m_disableScalingMatrixForLfnstBlks = b; }

  void      setSubPicIdMappingExplicitlySignalledFlag( bool b ) { m_subPicIdMappingExplicitlySignalledFlag = b; }
  bool      getSubPicIdMappingExplicitlySignalledFlag() const   { return m_subPicIdMappingExplicitlySignalledFlag; }
  void      setSubPicIdMappingPresentFlag( bool b )             { m_subPicIdMappingPresentFlag = b; }
  bool      getSubPicIdMappingPresentFlag() const               { return  m_subPicIdMappingPresentFlag; }
  void      setSubPicIdLen(uint32_t u)                          { m_subPicIdLen = u; }
  uint32_t  getSubPicIdLen() const                              { return  m_subPicIdLen; }
  void      setSubPicId(int i, uint16_t u)                      { m_subPicId[i] = u; }
  uint16_t  getSubPicId(int i) const                            { return  m_subPicId[i]; }

  void  setSubPicId(const std::vector<uint16_t> &v)             { CHECK(v.size()!=m_numSubPics, "number of vector entries must be equal to numSubPics") ; m_subPicId = v; }
  const std::vector<uint16_t> getSubPicIds() const              { return  m_subPicId; }

  uint32_t  getNumLongTermRefPicSPS() const                     { return m_numLongTermRefPicSPS; }
  void      setNumLongTermRefPicSPS(uint32_t val)               { m_numLongTermRefPicSPS = val; }

  uint32_t  getLtRefPicPocLsbSps(uint32_t index) const          { CHECK( index >= MAX_NUM_LONG_TERM_REF_PICS, "Index exceeds boundary" ); return m_ltRefPicPocLsbSps[index]; }
  void      setLtRefPicPocLsbSps(uint32_t index, uint32_t val)  { CHECK( index >= MAX_NUM_LONG_TERM_REF_PICS, "Index exceeds boundary" ); m_ltRefPicPocLsbSps[index] = val;  }

  bool      getUsedByCurrPicLtSPSFlag(int i) const              { CHECK( i >= MAX_NUM_LONG_TERM_REF_PICS, "Index exceeds boundary" ); return m_usedByCurrPicLtSPSFlag[i];    }
  void      setUsedByCurrPicLtSPSFlag(int i, bool x)            { CHECK( i >= MAX_NUM_LONG_TERM_REF_PICS, "Index exceeds boundary" ); m_usedByCurrPicLtSPSFlag[i] = x;       }

  int       getLog2MinCodingBlockSize() const                   { return m_log2MinCodingBlockSize; }
  void      setLog2MinCodingBlockSize(int val)                  { m_log2MinCodingBlockSize = val; }
  void      setCTUSize(unsigned ctuSize)                        { m_CTUSize = ctuSize; }
  unsigned  getCTUSize() const                                  { return  m_CTUSize; }
  void      setSplitConsOverrideEnabledFlag(bool b)             { m_partitionOverrideEnalbed = b; }
  bool      getSplitConsOverrideEnabledFlag() const             { return m_partitionOverrideEnalbed; }

  void setMinQTSizes(unsigned*   minQT)
  {
    m_minQT[0] = minQT[0];
    m_minQT[1] = minQT[1];
    m_minQT[2] = minQT[2];
  }
  unsigned getMinQTSize(SliceType slicetype, ChannelType chType = ChannelType::LUMA) const
  {
    return slicetype == I_SLICE ? (isLuma(chType) ? m_minQT[0] : m_minQT[2]) : m_minQT[1];
  }
  void setMaxMTTHierarchyDepth(unsigned maxMTTHierarchyDepth,
                               unsigned maxMTTHierarchyDepthI,
                               unsigned maxMTTHierarchyDepthIChroma)
  { m_maxMTTHierarchyDepth[1] = maxMTTHierarchyDepth;
    m_maxMTTHierarchyDepth[0] = maxMTTHierarchyDepthI;
    m_maxMTTHierarchyDepth[2] = maxMTTHierarchyDepthIChroma;
  }

  unsigned getMaxMTTHierarchyDepth() const        { return m_maxMTTHierarchyDepth[1]; }
  unsigned getMaxMTTHierarchyDepthI() const       { return m_maxMTTHierarchyDepth[0]; }
  unsigned getMaxMTTHierarchyDepthIChroma() const { return m_maxMTTHierarchyDepth[2]; }

  void setMaxBTSize(unsigned maxBTSize,
                    unsigned maxBTSizeI,
                    unsigned maxBTSizeC)
  { m_maxBTSize[1] = maxBTSize;
    m_maxBTSize[0] = maxBTSizeI;
    m_maxBTSize[2] = maxBTSizeC;
  }

  unsigned getMaxBTSize() const        { return m_maxBTSize[1]; }
  unsigned getMaxBTSizeI() const       { return m_maxBTSize[0]; }
  unsigned getMaxBTSizeIChroma() const { return m_maxBTSize[2]; }

  void setMaxTTSize(unsigned maxTTSize,
                    unsigned maxTTSizeI,
                    unsigned maxTTSizeC)
  { m_maxTTSize[1] = maxTTSize;
    m_maxTTSize[0] = maxTTSizeI;
    m_maxTTSize[2] = maxTTSizeC;
  }

  unsigned  getMaxTTSize() const             { return m_maxTTSize[1]; }
  unsigned  getMaxTTSizeI() const            { return m_maxTTSize[0]; }
  unsigned  getMaxTTSizeIChroma() const      { return m_maxTTSize[2]; }
  unsigned* getMinQTSizes() const            { return (unsigned *)m_minQT;                }
  unsigned* getMaxMTTHierarchyDepths() const { return (unsigned *)m_maxMTTHierarchyDepth; }
  unsigned* getMaxBTSizes() const            { return (unsigned *)m_maxBTSize;            }
  unsigned* getMaxTTSizes() const            { return (unsigned *)m_maxTTSize;            }

  void      setIDRRefParamListPresent(bool b) { m_idrRefParamList = b; }
  bool      getIDRRefParamListPresent() const { return m_idrRefParamList; }

  void      setUseDualITree(bool b) { m_dualITree = b; }
  bool      getUseDualITree() const { return m_dualITree; }

  void      setMaxCUWidth(uint32_t u)  { m_maxCuWidth = u; }
  uint32_t  getMaxCUWidth() const      { return m_maxCuWidth; }
  void      setMaxCUHeight(uint32_t u) { m_maxCuHeight = u; }
  uint32_t  getMaxCUHeight() const     { return m_maxCuHeight; }

  bool      getTransformSkipEnabledFlag() const          { return m_transformSkipEnabledFlag; }
  void      setTransformSkipEnabledFlag( bool b )        { m_transformSkipEnabledFlag = b; }
  uint32_t  getLog2MaxTransformSkipBlockSize() const     { return m_log2MaxTransformSkipBlockSize; }
  void      setLog2MaxTransformSkipBlockSize(uint32_t u) { m_log2MaxTransformSkipBlockSize = u; }

  bool      getBDPCMEnabledFlag() const { return m_bdpcmEnabledFlag; }
  void      setBDPCMEnabledFlag(bool b) { m_bdpcmEnabledFlag = b; }

  void      setBitsForPOC(uint32_t val) { m_bitsForPoc = val; }
  uint32_t  getBitsForPOC() const       { return m_bitsForPoc; }

  void                    setPocMsbCycleFlag(bool b)                             { m_pocMsbCycleFlag = b; }
  bool                    getPocMsbCycleFlag() const                             { return m_pocMsbCycleFlag; }
  void                    setPocMsbCycleLen(uint32_t u)                          { m_pocMsbCycleLen = u; }
  uint32_t                getPocMsbCycleLen() const                              { return m_pocMsbCycleLen; }
  void                    setNumExtraPHBytes(int i)                              { m_numExtraPHBytes = i; }
  int                     getNumExtraPHBytes() const                             { return m_numExtraPHBytes; }
  void                    setNumExtraSHBytes(int i)                              { m_numExtraSHBytes = i; }
  int                     getNumExtraSHBytes() const                             { return m_numExtraSHBytes; }
  void                    setExtraPHBitPresentFlags(const std::vector<bool> &b)  { m_extraPHBitPresentFlag = b; }
  const std::vector<bool> getExtraPHBitPresentFlags() const                      { return m_extraPHBitPresentFlag; }
  void                    setExtraSHBitPresentFlags(const std::vector<bool> &b)  { m_extraSHBitPresentFlag = b; }
  const std::vector<bool> getExtraSHBitPresentFlags() const                      { return m_extraSHBitPresentFlag; }
  void                    setMaxNumReorderPics(int i, uint32_t tlayer)           { m_maxNumReorderPics[tlayer] = i; }
  int                     getMaxNumReorderPics(uint32_t tlayer) const            { return m_maxNumReorderPics[tlayer]; }
  void                    createRplList(RefPicList l, int numRPL);
  const RPLList          *getRplList(RefPicList l) const                         { return &m_rplList[l]; }
  RPLList                *getRplList(RefPicList l)                               { return &m_rplList[l]; }
  uint32_t                getNumRpl(RefPicList l) const                          { return m_numRpl[l]; }
  void                    setRPL1CopyFromRPL0Flag(bool isCopy)                   { m_rpl1CopyFromRpl0Flag = isCopy; }
  bool                    getRPL1CopyFromRPL0Flag() const                        { return m_rpl1CopyFromRpl0Flag; }
  bool                    getRPL1IdxPresentFlag() const                          { return m_rpl1IdxPresentFlag; }
  void                    setAllActiveRplEntriesHasSameSignFlag(bool isAllSame)  { m_allRplEntriesHasSameSignFlag = isAllSame; }
  bool                    getAllActiveRplEntriesHasSameSignFlag() const          { return m_allRplEntriesHasSameSignFlag; }

  void                    setLongTermRefsPresent(bool val) { m_longTermRefsPresent = val; }
  bool                    getLongTermRefsPresent() const   { return m_longTermRefsPresent; }

  void                    setSPSTemporalMVPEnabledFlag(bool val) { m_temporalMvpEnabledFlag = val; }
  bool                    getSPSTemporalMVPEnabledFlag() const   { return m_temporalMvpEnabledFlag; }

  void                    setLog2MaxTbSize( uint32_t u )                                                  { m_log2MaxTbSize = u;                                                 }
  uint32_t                getLog2MaxTbSize() const                                                        { return  m_log2MaxTbSize;                                             }
  uint32_t                getMaxTbSize() const                                                            { return  1 << m_log2MaxTbSize;                                        }
  // Bit-depth
  int                     getBitDepth(const ChannelType type) const { return m_bitDepths[type]; }
  void                    setBitDepth(const ChannelType type, int u) { m_bitDepths[type] = u; }
  const BitDepths&        getBitDepths() const                                                            { return m_bitDepths;                                                  }

  bool                    getEntropyCodingSyncEnabledFlag() const                                         { return m_entropyCodingSyncEnabledFlag;                               }
  void                    setEntropyCodingSyncEnabledFlag(bool val)                                       { m_entropyCodingSyncEnabledFlag = val;                                }
  bool                    getEntryPointsPresentFlag() const                                               { return m_entryPointPresentFlag;                                      }
  void                    setEntryPointsPresentFlag(bool val)                                             { m_entryPointPresentFlag = val;                                       }
  int                     getMaxLog2TrDynamicRange(ChannelType channelType) const
  {
    return getSpsRangeExtension().getExtendedPrecisionProcessingFlag()
    ? std::min<int>(20, int(m_bitDepths[channelType] + 6))
    : 15;
  }
  int  getQpBDOffset(ChannelType type) const { return m_qpBDOffset[int(type)]; }
  void setQpBDOffset(ChannelType type, int i) { m_qpBDOffset[int(type)] = i; }
  int  getInternalMinusInputBitDepth(ChannelType type) const { return m_internalMinusInputBitDepth[type]; }
  void setInternalMinusInputBitDepth(ChannelType type, int i) { m_internalMinusInputBitDepth[type] = i; }

  void                    setSAOEnabledFlag(bool bVal)      { m_saoEnabledFlag = bVal; }
  bool                    getSAOEnabledFlag() const         { return m_saoEnabledFlag; }

  void                    setALFEnabledFlag(bool val)       { m_alfEnabledFlag = val; }
  bool                    getALFEnabledFlag() const         { return m_alfEnabledFlag; }

  void                    setALFOptEnabledFlag( bool b )     { m_alfOptEnabledFlag = b; }
  bool                    getALFOptEnabledFlag() const       { return m_alfOptEnabledFlag; }

  void                    setCCALFEnabledFlag(bool val)     { m_ccalfEnabledFlag = val; }
  bool                    getCCALFEnabledFlag() const       { return m_ccalfEnabledFlag; }

  void                    setJointCbCrEnabledFlag(bool val) { m_jointCbCrEnabledFlag = val; }
  bool                    getJointCbCrEnabledFlag() const   { return m_jointCbCrEnabledFlag; }

  void                    setSbTMVPEnabledFlag(bool val)    { m_sbtmvpEnabledFlag = val; }
  bool                    getSbTMVPEnabledFlag() const      { return m_sbtmvpEnabledFlag; }

  void                    setBDOFEnabledFlag(bool b)        { m_bdofEnabledFlag = b; }
  bool                    getBDOFEnabledFlag() const        { return m_bdofEnabledFlag; }

  bool                    getFpelMmvdEnabledFlag() const    { return m_fpelMmvdEnabledFlag; }
  void                    setFpelMmvdEnabledFlag( bool b )  { m_fpelMmvdEnabledFlag = b;    }
  bool                    getUseDMVR()const                 { return m_DMVR; }
  void                    setUseDMVR(bool b)                { m_DMVR = b;    }
  bool                    getUseMMVD()const                 { return m_MMVD; }
  void                    setUseMMVD(bool b)                { m_MMVD = b;    }

  bool                    getBdofControlPresentInPhFlag() const  { return m_bdofControlPresentInPhFlag; }
  void                    setBdofControlPresentInPhFlag(bool b)  { m_bdofControlPresentInPhFlag = b; }

  bool                    getDmvrControlPresentInPhFlag() const  { return m_dmvrControlPresentInPhFlag; }
  void                    setDmvrControlPresentInPhFlag(bool b)  { m_dmvrControlPresentInPhFlag = b; }

  bool                    getProfControlPresentInPhFlag() const  { return m_profControlPresentInPhFlag; }
  void                    setProfControlPresentInPhFlag(bool b)  { m_profControlPresentInPhFlag = b; }
  uint32_t                getMaxTLayers() const                  { return m_maxSubLayers; }
  void                    setMaxTLayers( uint32_t uiMaxTLayers ) { CHECK( uiMaxTLayers > MAX_TLAYER, "Invalid number T-layers" ); m_maxSubLayers = uiMaxTLayers; }

  bool                    getPtlDpbHrdParamsPresentFlag()  const { return m_ptlDpbHrdParamsPresentFlag; }
  void                    setPtlDpbHrdParamsPresentFlag(bool b)  { m_ptlDpbHrdParamsPresentFlag = b; }

  void                    setSubLayerDpbParamsFlag(bool val)     { m_subLayerDpbParamsFlag = val; }
  bool                    getSubLayerDpbParamsFlag() const       { return m_subLayerDpbParamsFlag; }

  bool                    getTemporalIdNestingFlag() const       { return m_temporalIdNestingFlag; }
  void                    setTemporalIdNestingFlag(bool value)   { m_temporalIdNestingFlag = value; }

  bool                    getScalingListFlag() const                         { return m_scalingListEnabledFlag; }
  void                    setScalingListFlag( bool b )                       { m_scalingListEnabledFlag  = b; }
  void                    setDepQuantEnabledFlag(bool b)                     { m_depQuantEnabledFlag = b; }
  bool                    getDepQuantEnabledFlag() const                     { return m_depQuantEnabledFlag; }
  void                    setSignDataHidingEnabledFlag(bool b)               { m_signDataHidingEnabledFlag = b; }
  bool                    getSignDataHidingEnabledFlag() const               { return m_signDataHidingEnabledFlag; }
  void                    setVirtualBoundariesEnabledFlag( bool b )          { m_virtualBoundariesEnabledFlag = b; }
  bool                    getVirtualBoundariesEnabledFlag() const            { return m_virtualBoundariesEnabledFlag; }
  void                    setVirtualBoundariesPresentFlag( bool b )          { m_virtualBoundariesPresentFlag = b; }
  bool                    getVirtualBoundariesPresentFlag() const            { return m_virtualBoundariesPresentFlag; }
  void                    setNumVerVirtualBoundaries(unsigned u)             { m_numVerVirtualBoundaries = u; }
  unsigned                getNumVerVirtualBoundaries() const                 { return m_numVerVirtualBoundaries; }
  void                    setNumHorVirtualBoundaries(unsigned u)             { m_numHorVirtualBoundaries = u; }
  unsigned                getNumHorVirtualBoundaries() const                 { return m_numHorVirtualBoundaries;                                    }
  void                    setVirtualBoundariesPosX(unsigned u, unsigned idx) { CHECK( idx >= 3, "vitrual boundary index exceeds valid range" ); m_virtualBoundariesPosX[idx] = u;    }
  unsigned                getVirtualBoundariesPosX(unsigned idx) const       { CHECK( idx >= 3, "vitrual boundary index exceeds valid range" ); return m_virtualBoundariesPosX[idx]; }
  void                    setVirtualBoundariesPosY(unsigned u, unsigned idx) { CHECK( idx >= 3, "vitrual boundary index exceeds valid range" ); m_virtualBoundariesPosY[idx] = u;    }
  unsigned                getVirtualBoundariesPosY(unsigned idx) const       { CHECK( idx >= 3, "vitrual boundary index exceeds valid range" ); return m_virtualBoundariesPosY[idx]; }

  uint32_t                getMaxDecPicBuffering(uint32_t tlayer) const         { return m_maxDecPicBuffering[tlayer]; }
  void                    setMaxDecPicBuffering(uint32_t val, uint32_t tlayer) { CHECK(tlayer >= MAX_TLAYER, "Invalid T-layer"); m_maxDecPicBuffering[tlayer] = val; }

  uint32_t                getMaxLatencyIncreasePlus1(uint32_t tlayer) const         { return m_maxLatencyIncreasePlus1[tlayer]; }
  void                    setMaxLatencyIncreasePlus1(uint32_t val, uint32_t tlayer) { m_maxLatencyIncreasePlus1[tlayer] = val; }

  uint32_t                getMaxNumMergeCand() const           { return m_maxNumMergeCand; }
  void                    setMaxNumMergeCand(uint32_t u)       { m_maxNumMergeCand = u; }
  uint32_t                getMaxNumAffineMergeCand() const     { return m_maxNumAffineMergeCand; }
  void                    setMaxNumAffineMergeCand(uint32_t u) { m_maxNumAffineMergeCand = u; }
  uint32_t                getMaxNumIBCMergeCand() const        { return m_maxNumIBCMergeCand; }
  void                    setMaxNumIBCMergeCand(uint32_t u)    { m_maxNumIBCMergeCand = u; }
  uint32_t                getMaxNumGeoCand() const             { return m_maxNumGeoCand; }
  void                    setMaxNumGeoCand(uint32_t u)         { m_maxNumGeoCand = u; }
  void                    setAffineAmvrEnabledFlag( bool val ) { m_affineAmvrEnabledFlag = val; }
  bool                    getAffineAmvrEnabledFlag() const     { return m_affineAmvrEnabledFlag; }

  bool                    getGeneralHrdParametersPresentFlag() const { return m_generalHrdParametersPresentFlag; }
  void                    setGeneralHrdParametersPresentFlag(bool b) { m_generalHrdParametersPresentFlag = b; }
  GeneralHrdParams*       getGeneralHrdParameters()                  { return &m_generalHrdParams; }
  const GeneralHrdParams* getGeneralHrdParameters() const            { return &m_generalHrdParams; }
  OlsHrdParams*           getOlsHrdParameters()                      { return &m_olsHrdParams[0]; }
  const OlsHrdParams*     getOlsHrdParameters() const                { return &m_olsHrdParams[0]; }

  bool                    getFieldSeqFlag() const             { return m_fieldSeqFlag; }
  void                    setFieldSeqFlag(bool i)             { m_fieldSeqFlag = i; }
  bool                    getVuiParametersPresentFlag() const { return m_vuiParametersPresentFlag; }
  void                    setVuiParametersPresentFlag(bool b) { m_vuiParametersPresentFlag = b; }
  unsigned                getVuiPayloadSize() const           { return m_vuiPayloadSize; }
  void                    setVuiPayloadSize(unsigned i)       { m_vuiPayloadSize = i; }
  VUI*                    getVuiParameters()                  { return &m_vuiParameters; }
  const VUI*              getVuiParameters() const            { return &m_vuiParameters; }
  const ProfileTierLevel* getProfileTierLevel() const         { return &m_profileTierLevel; }
  ProfileTierLevel*       getProfileTierLevel()               { return &m_profileTierLevel; }

  const SPSRExt&          getSpsRangeExtension() const        { return m_spsRangeExtension; }
  SPSRExt&                getSpsRangeExtension()              { return m_spsRangeExtension; }

  void      setWrapAroundEnabledFlag(bool b)    { m_wrapAroundEnabledFlag = b; }
  bool      getWrapAroundEnabledFlag() const    { return m_wrapAroundEnabledFlag; }
  void      setUseLmcs(bool b)                  { m_lmcsEnabled = b; }
  bool      getUseLmcs() const                  { return m_lmcsEnabled; }
  void      setIBCFlag(bool b)                  { m_ibcFlag = b; }
  bool      getIBCFlag() const                  { return m_ibcFlag; }
  void      setUseColorTrans(bool value)        { m_useColorTrans = value; }
  bool      getUseColorTrans() const            { return m_useColorTrans; }
  void      setPLTMode(unsigned PLTMode)        { m_PLTMode = PLTMode; }
  unsigned  getPLTMode() const                  { return m_PLTMode; }
  void      setUseSBT( bool b )                 { m_SBT = b; }
  bool      getUseSBT() const                   { return m_SBT; }
  void      setUseISP( bool b )                 { m_ISP = b; }
  bool      getUseISP() const                   { return m_ISP; }

  void      setAMVREnabledFlag(bool b)          { m_AMVREnabledFlag = b; }
  bool      getAMVREnabledFlag()const           { return m_AMVREnabledFlag; }
  void      setUseAffine(bool b)                { m_Affine = b; }
  bool      getUseAffine() const                { return m_Affine; }
  void      setUseAffineType(bool b)            { m_AffineType = b; }
  bool      getUseAffineType() const            { return m_AffineType; }
  void      setUsePROF(bool b)                  { m_PROF = b; }
  bool      getUsePROF() const                  { return m_PROF; }
  void      setUseLMChroma(bool b)              { m_LMChroma = b; }
  bool      getUseLMChroma() const              { return m_LMChroma; }
  void      setHorCollocatedChromaFlag(bool b)  { m_horCollocatedChromaFlag = b;    }
  bool      getHorCollocatedChromaFlag() const  { return m_horCollocatedChromaFlag; }
  void      setVerCollocatedChromaFlag(bool b)  { m_verCollocatedChromaFlag = b;    }
  bool      getVerCollocatedChromaFlag() const  { return m_verCollocatedChromaFlag; }
  bool      getCclmCollocatedChromaFlag() const { return m_verCollocatedChromaFlag; }
  void      setMtsEnabled(bool b)               { m_mtsEnabled = b; }
  bool      getMtsEnabled() const               { return m_mtsEnabled; }
  bool      getImplicitMTSIntraEnabled() const  { return m_mtsEnabled && !m_explicitMtsIntra; }
  void      setExplicitMtsIntraEnabled(bool b)  { m_explicitMtsIntra = b; }
  bool      getExplicitMtsIntraEnabled() const  { return m_explicitMtsIntra; }
  void      setExplicitMtsInterEnabled(bool b)  { m_explicitMtsInter = b; }
  bool      getExplicitMtsInterEnabled() const  { return m_explicitMtsInter; }
  void      setUseLFNST(bool b)                 { m_LFNST = b; }
  bool      getUseLFNST() const                 { return m_LFNST; }
  void      setUseSMVD(bool b)                  { m_SMVD = b; }
  bool      getUseSMVD() const                  { return m_SMVD; }
  void      setUseBcw(bool b)                   { m_bcw = b; }
  bool      getUseBcw() const                   { return m_bcw; }

  void      setLadfEnabled(bool b)              { m_ladfEnabled = b; }
  bool      getLadfEnabled() const              { return m_ladfEnabled; }
  void      setLadfNumIntervals(int i)          { m_ladfNumIntervals = i; }
  int       getLadfNumIntervals() const         { return m_ladfNumIntervals; }
  void      setLadfQpOffset(int value, int idx) { m_ladfQpOffset[idx] = value; }
  int       getLadfQpOffset(int idx) const      { return m_ladfQpOffset[idx]; }
  void      setLadfIntervalLowerBound(int value, int idx) { m_ladfIntervalLowerBound[idx] = value; }
  int       getLadfIntervalLowerBound(int idx) const { return m_ladfIntervalLowerBound[idx]; }

  void      setUseCiip(bool b)                  { m_ciip = b; }
  bool      getUseCiip() const                  { return m_ciip; }
  void      setUseGeo(bool b)                   { m_Geo = b; }
  bool      getUseGeo() const                   { return m_Geo; }
  void      setUseMRL(bool b)                   { m_MRL = b; }
  bool      getUseMRL() const                   { return m_MRL; }
  void      setUseMIP(bool b)                   { m_MIP = b; }
  bool      getUseMIP() const                   { return m_MIP; }

  bool      getUseWP() const                   { return m_useWeightPred; }
  bool      getUseWPBiPred() const             { return m_useWeightedBiPred; }
  void      setUseWP(bool b)                   { m_useWeightPred = b; }
  void      setUseWPBiPred(bool b)             { m_useWeightedBiPred = b; }

  void      setChromaQpMappingTableFromParams(const ChromaQpMappingTableParams &params, const int qpBdOffset)   { m_chromaQpMappingTable.setParams(params, qpBdOffset); }
  void      deriveChromaQPMappingTables()                                           { m_chromaQpMappingTable.deriveChromaQPMappingTables(); }
  const ChromaQpMappingTable& getChromaQpMappingTable()                   const     { return m_chromaQpMappingTable;}
  int       getMappedChromaQpValue(ComponentID compID, int qpVal)         const     { return m_chromaQpMappingTable.getMappedChromaQpValue(compID, qpVal); }

  void      setGDREnabledFlag(bool flag)                { m_GDREnabledFlag = flag; }
  bool      getGDREnabledFlag() const                   { return m_GDREnabledFlag; }
  void      setSubLayerParametersPresentFlag(bool flag) { m_SubLayerCbpParametersPresentFlag = flag; }
  bool      getSubLayerParametersPresentFlag() const    { return m_SubLayerCbpParametersPresentFlag;  }

  bool      getRprEnabledFlag()const                    { return m_rprEnabledFlag; }
  void      setRprEnabledFlag( bool flag )              { m_rprEnabledFlag = flag; }
  bool      getGOPBasedRPREnabledFlag() const           { return m_gopBasedRPREnabledFlag; }
  void      setGOPBasedRPREnabledFlag(bool flag)        { m_gopBasedRPREnabledFlag = flag; }
  bool      getInterLayerPresentFlag() const            { return m_interLayerPresentFlag; }
  void      setInterLayerPresentFlag( bool b )          { m_interLayerPresentFlag = b; }
  bool      getResChangeInClvsEnabledFlag() const       { return m_resChangeInClvsEnabledFlag; }
  void      setResChangeInClvsEnabledFlag(bool flag)    { m_resChangeInClvsEnabledFlag = flag; }

  uint32_t  getLog2ParallelMergeLevelMinus2() const            { return m_log2ParallelMergeLevelMinus2; }
  void      setLog2ParallelMergeLevelMinus2(uint32_t mrgLevel) { m_log2ParallelMergeLevelMinus2 = mrgLevel; }
  void      setPPSValidFlag(int i, bool b)                     { m_ppsValidFlag[i] = b; }
  bool      getPPSValidFlag(int i)                             { return m_ppsValidFlag[i]; }

  void        setScalingWindowSizeInPPS(int i, int scWidth, int scHeight) { m_scalingWindowSizeInPPS[i].width = scWidth; m_scalingWindowSizeInPPS[i].height = scHeight;}
  const Size& getScalingWindowSizeInPPS(int i)                            { return m_scalingWindowSizeInPPS[i]; }

  void      setScalingMatrixForAlternativeColourSpaceDisabledFlag(bool b) { m_scalingMatrixAlternativeColourSpaceDisabledFlag = b; }
  bool      getScalingMatrixForAlternativeColourSpaceDisabledFlag() const { return m_scalingMatrixAlternativeColourSpaceDisabledFlag; }
  void      setScalingMatrixDesignatedColourSpaceFlag(bool b)             { m_scalingMatrixDesignatedColourSpaceFlag = b; }
  bool      getScalingMatrixDesignatedColourSpaceFlag() const             { return m_scalingMatrixDesignatedColourSpaceFlag; }

  static int getWinUnitX(ChromaFormat cf) { return isChromaEnabled(cf) ? 1 << getChannelTypeScaleX(ChannelType::CHROMA, cf) : 1; }
  static int getWinUnitY(ChromaFormat cf) { return 1 << getChannelTypeScaleY(ChannelType::CHROMA, cf); }
};

