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

/** \file     DecLib.h
    \brief    decoder class (header)
*/

#ifndef __DECLIB__
#define __DECLIB__

#include "DecSlice.h"
#include "CABACReader.h"
#include "VLCReader.h"
#include "SEIread.h"
#include "CacheModel.h"

#include "CommonLib/CommonDef.h"
#include "CommonLib/Picture.h"
#include "CommonLib/TrQuant.h"
#include "CommonLib/InterPrediction.h"
#include "CommonLib/IntraPrediction.h"
#include "CommonLib/DeblockingFilter.h"
#include "CommonLib/AdaptiveLoopFilter.h"
#include "CommonLib/SEI.h"
#include "CommonLib/Unit.h"
#include "CommonLib/Reshape.h"
#include "CommonLib/SEINeuralNetworkPostFiltering.h"
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
#include "CommonLib/SEIPackedRegionsInfoProcess.h"
#endif

#if JVET_AJ0151_DSC_SEI
#include "SEIDigitallySignedContent.h"
#endif

#if JVET_AJ0151_DSC_SEI
struct binNalUnit
{
  NalUnitType nalUnitType = NAL_UNIT_INVALID;
  size_t  length = 0;
  uint8_t *data  = nullptr;
};
#endif

class InputNALUnit;

//! \ingroup DecoderLib
//! \{

bool tryDecodePicture(Picture *pcPic, const int expectedPoc, const std::string &bitstreamFileName,
                      const int layerIdx, EnumArray<ParameterSetMap<APS>, ApsType> *apsMap = nullptr, 
                      bool bDecodeUntilPocFound = false, int debugCTU = -1, int debugPOC = -1);
// Class definition
// ====================================================================================================================

/// decoder class
class DecLib
{
private:
  int                     m_maxRefPicNum;
  bool m_isFirstGeneralHrd;
  GeneralHrdParams        m_prevGeneralHrdParams;

  int                     m_prevGDRInSameLayerPOC[MAX_VPS_LAYERS]; ///< POC number of the latest GDR picture
  int                     m_prevGDRInSameLayerRecoveryPOC[MAX_VPS_LAYERS]; ///< Recovery POC number of the latest GDR picture
  NalUnitType             m_associatedIRAPType[MAX_VPS_LAYERS]; ///< NAL unit type of the previous IRAP picture
  int                     m_pocCRA[MAX_VPS_LAYERS];             ///< POC number of the previous CRA picture
  CheckCRAFlags           m_checkCRAFlags[MAX_VPS_LAYERS];
  int                     m_latestDRAPPOC;
  int                     m_latestEDRAPPOC;
  bool                    m_latestEDRAPIndicationLeadingPicturesDecodableFlag;
  int                     m_associatedIRAPDecodingOrderNumber[MAX_VPS_LAYERS]; ///< Decoding order number of the previous IRAP picture
  int                     m_decodingOrderCounter;
  int                     m_puCounter;
  bool                    m_seiInclusionFlag;
  int                     m_prevGDRSubpicPOC[MAX_VPS_LAYERS][MAX_NUM_SUB_PICS];
  int                     m_prevIRAPSubpicPOC[MAX_VPS_LAYERS][MAX_NUM_SUB_PICS];
  NalUnitType             m_prevIRAPSubpicType[MAX_VPS_LAYERS][MAX_NUM_SUB_PICS];
  int                     m_prevIRAPSubpicDecOrderNo[MAX_VPS_LAYERS][MAX_NUM_SUB_PICS];
  int                     m_pocRandomAccess;   ///< POC number of the random access point (the first IDR or CRA picture)
  int                     m_lastRasPoc;
  bool                    m_prevEOS[MAX_VPS_LAYERS];

  PicList                 m_cListPic;         //  Dynamic buffer
  ParameterSetManager     m_parameterSetManager;  // storage for parameter sets
  PicHeader               m_picHeader;            // picture header
  Slice*                  m_apcSlicePilot;


  SEIMessages             m_SEIs; ///< List of SEI messages that have been received before the first slice and between slices, excluding prefix SEIs...
  SEIScalabilityDimensionInfo* m_sdiSEIInFirstAU;
  SEIMultiviewAcquisitionInfo* m_maiSEIInFirstAU;
  SEIMultiviewViewPosition*    m_mvpSEIInFirstAU;

  // functional classes
  IntraPrediction         m_cIntraPred;
  InterPrediction         m_cInterPred;
  TrQuant                 m_cTrQuant;
  DecSlice                m_cSliceDecoder;
  TrQuant                 m_cTrQuantScalingList;
  DecCu                   m_cCuDecoder;
  HLSyntaxReader          m_HLSReader;
  CABACDecoder            m_CABACDecoder;
  SEIReader               m_seiReader;
#if JVET_S0257_DUMP_360SEI_MESSAGE
  SeiCfgFileDump          m_seiCfgDump;
#endif
  DeblockingFilter        m_deblockingFilter;
  SampleAdaptiveOffset    m_cSAO;
  AdaptiveLoopFilter      m_cALF;
  Reshape                 m_cReshaper;                        ///< reshaper class
  HRD                     m_HRD;
  // decoder side RD cost computation
  RdCost                  m_cRdCost;                      ///< RD cost computation class
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  CacheModel              m_cacheModel;
#endif
  bool isRandomAccessSkipPicture(int& iSkipFrame, int& iPOCLastDisplay, bool mixedNaluInPicFlag, uint32_t layerId);
  Picture*                m_pcPic;
  uint32_t                m_uiSliceSegmentIdx;
  uint32_t                m_prevLayerID;
  int                     m_prevPOC;
  int                     m_prevPicPOC;
  int                     m_prevTid0POC;
  bool                    m_bFirstSliceInPicture;
  bool                    m_firstPictureInSequence;
  SEIFilmGrainSynthesizer m_grainCharacteristic;
  PelStorage              m_grainBuf;
  SEIColourTransformApply m_colourTranfParams;
  PelStorage              m_invColourTransfBuf;
  bool                    m_firstSliceInSequence[MAX_VPS_LAYERS];
  bool                    m_firstSliceInBitstream;
  bool                    m_isFirstAuInCvs;
  bool                    m_accessUnitEos[MAX_VPS_LAYERS];
  bool                    m_prevSliceSkipped;
  int                     m_skippedPOC;
  uint32_t                m_skippedLayerID;
  int                     m_lastPOCNoOutputPriorPics;
  bool                    m_isNoOutputPriorPics;
  bool                    m_lastNoOutputBeforeRecoveryFlag[MAX_VPS_LAYERS];    //value of variable NoOutputBeforeRecoveryFlag of the assocated CRA/GDR pic
  int                     m_sliceLmcsApsId;         //value of LmcsApsId, constraint is same id for all slices in one picture
  std::ostream           *m_pDecodedSEIOutputStream;
  uint32_t                m_audIrapOrGdrAuFlag;
#if JVET_S0257_DUMP_360SEI_MESSAGE
  std::string             m_decoded360SeiDumpFileName;
#endif

  int                     m_decodedPictureHashSEIEnabled;  ///< Checksum(3)/CRC(2)/MD5(1)/disable(0) acting on decoded picture hash SEI message
  uint32_t                m_numberOfChecksumErrorsDetected;

  bool                    m_warningMessageSkipPicture;

  std::list<InputNALUnit*> m_prefixSEINALUs; /// Buffered up prefix SEI NAL Units.
  bool                                m_ShutterFilterEnable;          ///< enable Post-processing with Shutter Interval SEI
  int                     m_debugPOC;
  int                     m_debugCTU;

  struct AccessUnitInfo
  {
    NalUnitType     m_nalUnitType; ///< nal_unit_type
    uint32_t        m_temporalId;  ///< temporal_id
    uint32_t        m_nuhLayerId;  ///< nuh_layer_id
  };
  std::vector<AccessUnitInfo> m_accessUnitNals;
  struct AccessUnitPicInfo
  {
    NalUnitType     m_nalUnitType; ///< nal_unit_type
    uint32_t        m_temporalId;  ///< temporal_id
    uint32_t        m_nuhLayerId;  ///< nuh_layer_id
    int             m_POC;
  };
  std::vector<AccessUnitPicInfo> m_accessUnitPicInfo;
  std::vector<AccessUnitPicInfo> m_firstAccessUnitPicInfo;
  struct AccessUnitNestedSliSeiInfo
  {
    bool m_nestedSliPresent;
    uint32_t m_numOlssNestedSli;
    uint32_t m_olsIdxNestedSLI[MAX_NUM_OLSS];
  };
  std::vector<AccessUnitNestedSliSeiInfo> m_accessUnitNestedSliSeiInfo;
  int m_accessUnitSpsNumSubpic[MAX_VPS_LAYERS];
  struct NalUnitInfo
  {
    NalUnitType     m_nalUnitType; ///< nal_unit_type
    uint32_t        m_nuhLayerId;  ///< nuh_layer_id
    uint32_t        m_firstCTUinSlice; /// the first CTU in slice, specified with raster scan order ctu address
    int             m_POC;             /// the picture order
  };
  std::vector<NalUnitInfo> m_nalUnitInfo[MAX_VPS_LAYERS];
  EnumArray<std::vector<int>, ApsType> m_accessUnitApsNals;
  std::vector<int> m_accessUnitSeiTids;
  std::vector<bool> m_accessUnitNoOutputPriorPicFlags;

  // NAL unit type, layer ID, and SEI payloadType
  std::vector<std::tuple<NalUnitType, int, SEI::PayloadType>> m_accessUnitSeiPayLoadTypes;

  std::vector<NalUnitType> m_pictureUnitNals;
  std::list<InputNALUnit *> m_pictureSeiNalus;
  std::list<InputNALUnit *> m_suffixApsNalus;
  std::list<InputNALUnit*> m_accessUnitSeiNalus;

  OPI*                    m_opi;
  bool                    m_mTidExternalSet;
  bool                    m_mTidOpiSet;
  bool                    m_tOlsIdxTidExternalSet;
  bool                    m_tOlsIdxTidOpiSet;
  VPS*                    m_vps;
  int                     m_maxDecSubPicIdx;
  int                     m_maxDecSliceAddrInSubPic;
  int                     m_clsVPSid;

#if JVET_AJ0151_DSC_SEI
  std::string   m_keyStoreDir;
  std::string   m_trustStoreDir;
#endif

#if GDR_ENABLED
  int m_lastGdrPoc;
  int m_lastGdrRecoveryPocCnt;
#endif
  SEINeuralNetworkPostFiltering m_nnPostFiltering;
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  SEIPackedRegionsInfoProcess m_priProcess;
#endif

public:
  int                     m_targetSubPicIdx;

  DCI*                    m_dci;
  EnumArray<ParameterSetMap<APS>, ApsType> *m_apsMapEnc = nullptr;
#if GDR_LEAK_TEST
public:
  int                     m_gdrPocRandomAccess;
#endif // GDR_LEAK_TEST

#if JVET_AJ0151_DSC_SEI
  void xStoreNALUnitForSignature(InputNALUnit &nalu);
  void xProcessStoredNALUnitsForSignature(int substream_id);

  std::list<binNalUnit> m_signedContentNalUnitBuffer;
  DscSubstreamManager   m_dscSubstreamManager;
#endif

public:
  DecLib();
  virtual ~DecLib();

  void  create  ();
  void  destroy ();

  void  setDecodedPictureHashSEIEnabled(int enabled) { m_decodedPictureHashSEIEnabled=enabled; }

  void  init(
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
    const std::string& cacheCfgFileName
#endif
  );
  bool  decode(InputNALUnit& nalu, int& iSkipFrame, int& iPOCLastDisplay, int iTargetOlsIdx);
  void  deletePicBuffer();

  void  executeLoopFilters();
  void finishPicture(int &poc, PicList *&rpcListPic, MsgLevel msgl = INFO, bool associatedWithNewClvs = false);
  void  finishPictureLight(int& poc, PicList*& rpcListPic );
  void  checkNoOutputPriorPics (PicList* rpcListPic);
  void  checkNalUnitConstraints( uint32_t naluType );
  void  checkPicTypeAfterEos();
  void  updateAssociatedIRAP();
  void  updatePrevGDRInSameLayer();
  void  updatePrevIRAPAndGDRSubpic();
  bool  getGDRRecoveryPocReached()          { return ( m_pcPic->getPOC() >= m_prevGDRInSameLayerRecoveryPOC[m_pcPic->layerId] ); }
  void  setEosPresentInPu(bool eosPresent)  { m_pcPic->isEosPresentInPic = eosPresent; }

  bool  getNoOutputPriorPicsFlag () const   { return m_isNoOutputPriorPics; }
  void  setNoOutputPriorPicsFlag (bool val) { m_isNoOutputPriorPics = val; }
  void  setFirstSliceInPicture (bool val)  { m_bFirstSliceInPicture = val; }
  bool  getFirstSliceInPicture () const  { return m_bFirstSliceInPicture; }
  bool  getFirstSliceInSequence(int layerId) const { return m_firstSliceInSequence[layerId]; }
  void  setFirstSliceInSequence(bool val, int layerId) { m_firstSliceInSequence[layerId] = val; }
  void  setDecodedSEIMessageOutputStream(std::ostream *pOpStream) { m_pDecodedSEIOutputStream = pOpStream; }
#if JVET_S0257_DUMP_360SEI_MESSAGE
  void  setDecoded360SEIMessageFileName(std::string &Dump360SeiFileName) { m_decoded360SeiDumpFileName = Dump360SeiFileName; }
#endif
  uint32_t  getNumberOfChecksumErrorsDetected() const { return m_numberOfChecksumErrorsDetected; }

#if GDR_ENABLED
  void setLastGdrPoc(int poc) { m_lastGdrPoc = poc;  }
  int  getLastGdrPoc()        { return m_lastGdrPoc; }
  void setLastGdrRecoveryPocCnt(int recoveryPocCnt) { m_lastGdrRecoveryPocCnt = recoveryPocCnt; }
  int  getLastGdrRecoveryPocCnt()                     { return m_lastGdrRecoveryPocCnt; }
#endif

  int  getDebugCTU( )               const { return m_debugCTU; }
  void setDebugCTU( int debugCTU )        { m_debugCTU = debugCTU; }
  int  getDebugPOC( )               const { return m_debugPOC; };
  void setDebugPOC( int debugPOC )        { m_debugPOC = debugPOC; };
  void resetAccessUnitNals()              { m_accessUnitNals.clear();    }
  void resetAccessUnitPicInfo()           { m_accessUnitPicInfo.clear(); }
  void resetAccessUnitApsNals()
  {
    for (auto &nals: m_accessUnitApsNals)
    {
      nals.clear();
    }
  }
  void resetAccessUnitSeiTids()           { m_accessUnitSeiTids.clear(); }
  void resetAudIrapOrGdrAuFlag()          { m_audIrapOrGdrAuFlag = false; }
  void resetAccessUnitEos()               { memset(m_accessUnitEos, false, sizeof(m_accessUnitEos)); }
  void checkTidLayerIdInAccessUnit();
  void resetAccessUnitSeiPayLoadTypes()   { m_accessUnitSeiPayLoadTypes.clear(); }
  void checkSEIInAccessUnit();
  void checkSeiContentInAccessUnit();
  void resetAccessUnitSeiNalus();
  void checkLayerIdIncludedInCvss();
  void CheckNoOutputPriorPicFlagsInAccessUnit();
  void resetAccessUnitNoOutputPriorPicFlags() { m_accessUnitNoOutputPriorPicFlags.clear(); }
  void checkMultiSubpicNum(int olsIdx);
  void resetAccessUnitNestedSliSeiInfo()  { m_accessUnitNestedSliSeiInfo.clear(); }
  void resetIsFirstAuInCvs();
  void checkSeiInPictureUnit();
  void resetPictureSeiNalus();
  void resetPrefixSeiNalus();
  bool isSliceNaluFirstInAU( bool newPicture, InputNALUnit &nalu );
  void processSuffixApsNalus();

  void checkAPSInPictureUnit();
  void resetPictureUnitNals() { m_pictureUnitNals.clear(); }

  const VPS* getVPS()                     { return m_vps; }
  void deriveTargetOutputLayerSet( const int targetOlsIdx ) { if( m_vps != nullptr ) m_vps->deriveTargetOutputLayerSet( targetOlsIdx ); }

  void  initScalingList()
  {
    m_cTrQuantScalingList.init(nullptr, MAX_TB_SIZEY, false, false, false, false);
  }

  void  setAPSMapEnc(EnumArray<ParameterSetMap<APS>, ApsType> *apsMap) { m_apsMapEnc = apsMap; }
  bool  isNewPicture( std::ifstream *bitstreamFile, class InputByteStream *bytestream );
  bool  isNewAccessUnit( bool newPicture, std::ifstream *bitstreamFile, class InputByteStream *bytestream );

  bool      getHTidExternalSetFlag()               const { return m_mTidExternalSet; }
  void      setHTidExternalSetFlag(bool mTidExternalSet)  { m_mTidExternalSet = mTidExternalSet; }
  bool      getHTidOpiSetFlag()               const { return m_mTidOpiSet; }
  void      setHTidOpiSetFlag(bool mTidOpiSet)  { m_mTidOpiSet = mTidOpiSet; }
  bool      getTOlsIdxExternalFlag()               const { return m_tOlsIdxTidExternalSet; }
  void      setTOlsIdxExternalFlag (bool tOlsIdxExternalSet)  { m_tOlsIdxTidExternalSet = tOlsIdxExternalSet; }
  bool      getTOlsIdxOpiFlag()               const { return m_tOlsIdxTidOpiSet; }
  void      setTOlsIdxOpiFlag(bool tOlsIdxOpiSet)  { m_tOlsIdxTidOpiSet = tOlsIdxOpiSet; }
  const OPI* getOPI()                     { return m_opi; }

  bool      getMixedNaluTypesInPicFlag();
#if GREEN_METADATA_SEI_ENABLED
  FeatureCounterStruct m_featureCounter;
  bool m_GMFAFramewise;
  std::string   m_GMFAFile;
  void setFeatureCounter (FeatureCounterStruct b ) {m_featureCounter = b;}
  FeatureCounterStruct getFeatureCounter (){return m_featureCounter;}
  void setGMFAFile(std::string b){m_GMFAFile = b;}
  void setFeatureAnalysisFramewise(bool b){m_GMFAFramewise = b;}
#endif

  bool  getShutterFilterFlag()        const { return m_ShutterFilterEnable; }
  void  setShutterFilterFlag(bool value) { m_ShutterFilterEnable = value; }

  void applyNnPostFilter();
  void setPrevPicPOC(const int prevPicPoc) { m_prevPicPOC  = prevPicPoc;}

#if JVET_AJ0151_DSC_SEI
  void setKeyStoreParameters(const std::string &keyStoreDir, const std::string &trustStoreDir)
  {
    m_keyStoreDir = keyStoreDir;
    m_trustStoreDir = trustStoreDir;
  }
#endif
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  SEIPackedRegionsInfoProcess& getPriProcess() { return m_priProcess; }
#endif

protected:
  void  xUpdateRasInit(Slice* slice);

  Picture * xGetNewPicBuffer( const SPS &sps, const PPS &pps, const uint32_t temporalLayer, const int layerId );
  void  xCreateLostPicture( int iLostPOC, const int layerId );
  void  xCreateUnavailablePicture( const PPS *pps, const int iUnavailablePoc, const bool longTermFlag, const int temporalId, const int layerId, const bool interLayerRefPicFlag );
  void  checkParameterSetsInclusionSEIconstraints(const InputNALUnit nalu);
  void  xActivateParameterSets( const InputNALUnit nalu );
  void  xCheckParameterSetConstraints( const int layerId );
  void      xDecodePicHeader( InputNALUnit& nalu );
  bool      xDecodeSlice(InputNALUnit &nalu, int &iSkipFrame, int iPOCLastDisplay);
  void      xDecodeOPI( InputNALUnit& nalu );
  void      xDecodeVPS( InputNALUnit& nalu );
  void      xDecodeDCI( InputNALUnit& nalu );
  void      xDecodeSPS( InputNALUnit& nalu );
  void      xDecodePPS( InputNALUnit& nalu );
  void      xDecodeAPS(InputNALUnit& nalu);
  void      xUpdatePreviousTid0POC(Slice *pSlice)
  {
    if( (pSlice->getTLayer() == 0) && (pSlice->getNalUnitType() != NAL_UNIT_CODED_SLICE_RASL) && (pSlice->getNalUnitType() != NAL_UNIT_CODED_SLICE_RADL) && !pSlice->getPicHeader()->getNonReferencePictureFlag() )
    {
      m_prevTid0POC = pSlice->getPOC();
    }
  }
  void      xParsePrefixSEImessages();
  void      xParsePrefixSEIsForUnknownVCLNal();
  void      xCheckPrefixSEIMessages( SEIMessages& prefixSEIs );
  void      xCheckDUISEIMessages(SEIMessages &prefixSEIs);


  void  xCheckNalUnitConstraintFlags( const ConstraintInfo *cInfo, uint32_t naluType );
  void     xCheckMixedNalUnit(Slice* pcSlice, SPS *sps, InputNALUnit &nalu);
  uint32_t xGetNnpfaTargetId(uint8_t* payload, uint32_t payloadSize);
};// END CLASS DEFINITION DecLib


//! \}

#endif // __DECTOP__

