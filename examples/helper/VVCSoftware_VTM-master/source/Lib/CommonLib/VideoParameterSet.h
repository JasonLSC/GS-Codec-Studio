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

#include <vector>
#include "HRD.h"
#include "ProfileTierLevel.h"

struct DpbParameters
{
  int maxDecPicBuffering[MAX_TLAYER]      = { 0 };
  int maxNumReorderPics[MAX_TLAYER]       = { 0 };
  int maxLatencyIncreasePlus1[MAX_TLAYER] = { 0 };
};

class VPS
{
private:
  int                   m_vpsId;
  uint32_t              m_maxLayers;

  uint32_t              m_vpsMaxSubLayers;
  uint32_t              m_vpsLayerId[MAX_VPS_LAYERS];
  bool                  m_vpsDefaultPtlDpbHrdMaxTidFlag;
  bool                  m_vpsAllIndependentLayersFlag;
  uint32_t              m_vpsCfgPredDirection[MAX_VPS_SUBLAYERS];
  bool                  m_vpsIndependentLayerFlag[MAX_VPS_LAYERS];
  bool                  m_vpsDirectRefLayerFlag[MAX_VPS_LAYERS][MAX_VPS_LAYERS];
  std::vector<std::vector<uint32_t>> m_vpsMaxTidIlRefPicsPlus1;
  bool                  m_vpsEachLayerIsAnOlsFlag;
  uint32_t              m_vpsOlsModeIdc;
  uint32_t              m_vpsNumOutputLayerSets;
  bool                  m_vpsOlsOutputLayerFlag[MAX_NUM_OLSS][MAX_VPS_LAYERS];
  uint32_t              m_directRefLayerIdx[MAX_VPS_LAYERS][MAX_VPS_LAYERS];
  uint32_t              m_generalLayerIdx[MAX_VPS_LAYERS];

  bool                  m_ptPresentFlag[MAX_NUM_OLSS];
  uint32_t              m_ptlMaxTemporalId[MAX_NUM_OLSS];
  std::vector<ProfileTierLevel> m_vpsProfileTierLevel;
  uint32_t              m_olsPtlIdx[MAX_NUM_OLSS];

  // stores index ( ilrp_idx within 0 .. NumDirectRefLayers ) of the dependent reference layers
  uint32_t              m_interLayerRefIdx[MAX_VPS_LAYERS][MAX_VPS_LAYERS];
  bool                  m_vpsExtensionFlag;
  bool                  m_vpsGeneralHrdParamsPresentFlag;
  bool                  m_vpsSublayerCpbParamsPresentFlag;
  uint32_t              m_numOlsTimingHrdParamsMinus1;
  uint32_t              m_hrdMaxTid[MAX_NUM_OLSS];
  uint32_t              m_olsTimingHrdIdx[MAX_NUM_OLSS];
  GeneralHrdParams      m_generalHrdParams;
  std::vector<Size>             m_olsDpbPicSize;
  std::vector<int>              m_olsDpbParamsIdx;
  std::vector<std::vector<int>> m_outputLayerIdInOls;
  std::vector<std::vector<int>> m_numSubLayersInLayerInOLS;

  // mapping from multi-layer OLS index to OLS index. Initialized in deriveOutputLayerSets()
  // m_multiLayerOlsIdxToOlsIdx[n] is the OLSidx of the n-th multi-layer OLS.
  std::vector<int> m_multiLayerOlsIdxToOlsIdx;

public:
  std::vector<std::vector<OlsHrdParams>> m_olsHrdParams;

  int                           m_totalNumOLSs;
  int                           m_numMultiLayeredOlss;
  uint32_t                      m_multiLayerOlsIdx[MAX_NUM_OLSS];
  int                           m_numDpbParams;
  std::vector<DpbParameters>    m_dpbParameters;
  bool                          m_sublayerDpbParamsPresentFlag;
  std::vector<int>              m_dpbMaxTemporalId;
  std::vector<int>              m_targetOutputLayerIdSet;          // set of LayerIds to be outputted
  std::vector<int>              m_targetLayerIdSet;                // set of LayerIds to be included in the sub-bitstream extraction process.
  int                           m_targetOlsIdx;
  std::vector<int>              m_numOutputLayersInOls;
  std::vector<int>              m_numLayersInOls;
  std::vector<std::vector<int>> m_layerIdInOls;
  std::vector<ChromaFormat>     m_olsDpbChromaFormatIdc;
  std::vector<int>              m_olsDpbBitDepthMinus8;

public:
  VPS();
  virtual ~VPS() {};

  int  getVPSId() const                                    { return m_vpsId; }
  void setVPSId(int i)                                     { m_vpsId = i; }

  uint32_t          getMaxLayers() const                   { return m_maxLayers; }
  void              setMaxLayers(uint32_t l)               { m_maxLayers = l; }

  uint32_t          getMaxSubLayers() const                { return m_vpsMaxSubLayers; }
  void              setMaxSubLayers(uint32_t value)        { m_vpsMaxSubLayers = value; }
  bool              getDefaultPtlDpbHrdMaxTidFlag() const  { return m_vpsDefaultPtlDpbHrdMaxTidFlag; }
  void              setDefaultPtlDpbHrdMaxTidFlag(bool t)  { m_vpsDefaultPtlDpbHrdMaxTidFlag = t; }

  uint32_t          getLayerId(uint32_t layerIdx) const             { return m_vpsLayerId[layerIdx]; }
  void              setLayerId(uint32_t layerIdx, uint32_t layerId) { m_vpsLayerId[layerIdx] = layerId; }

  bool              getAllIndependentLayersFlag() const { return m_vpsAllIndependentLayersFlag; }
  void              setAllIndependentLayersFlag(bool t) { m_vpsAllIndependentLayersFlag = t; }
  uint32_t          getPredDirection(uint32_t tmplayer) const       { return m_vpsCfgPredDirection[tmplayer]; }
  void              setPredDirection(uint32_t tmplayer, uint32_t t) { m_vpsCfgPredDirection[tmplayer] = t; }

  bool              getIndependentLayerFlag(uint32_t layerIdx) const   { return m_vpsIndependentLayerFlag[layerIdx]; }
  void              setIndependentLayerFlag(uint32_t layerIdx, bool t) { m_vpsIndependentLayerFlag[layerIdx] = t; }

  uint32_t getMaxTidIlRefPicsPlus1(const uint32_t layerIdx, const uint32_t refLayerIdx) const
  {
    CHECK(layerIdx >= m_vpsMaxTidIlRefPicsPlus1.size(), "layerIdx out of bounds");
    CHECK(refLayerIdx >= m_vpsMaxTidIlRefPicsPlus1[layerIdx].size(), "refLayerIdx out of bounds");
    return m_vpsMaxTidIlRefPicsPlus1[layerIdx][refLayerIdx];
  }
  void setMaxTidIlRefPicsPlus1(const uint32_t layerIdx, const uint32_t refLayerIdx, const uint32_t i)
  {
    CHECK(layerIdx >= m_vpsMaxTidIlRefPicsPlus1.size(), "layerIdx out of bounds");
    CHECK(refLayerIdx >= m_vpsMaxTidIlRefPicsPlus1[layerIdx].size(), "refLayerIdx out of bounds");
    m_vpsMaxTidIlRefPicsPlus1[layerIdx][refLayerIdx] = i;
  }

  void              setMaxTidIlRefPicsPlus1(std::vector<std::vector<uint32_t>> i) { m_vpsMaxTidIlRefPicsPlus1 = i; }

  bool              getDirectRefLayerFlag(uint32_t layerIdx, uint32_t refLayerIdx) const { return m_vpsDirectRefLayerFlag[layerIdx][refLayerIdx]; }
  void              setDirectRefLayerFlag(uint32_t layerIdx, uint32_t refLayerIdx, bool t) { m_vpsDirectRefLayerFlag[layerIdx][refLayerIdx] = t; }

  uint32_t          getDirectRefLayerIdx( uint32_t layerIdx, uint32_t refLayerIdc ) const { return m_directRefLayerIdx[layerIdx][refLayerIdc]; }
  void              setDirectRefLayerIdx( uint32_t layerIdx, uint32_t refLayerIdc, uint32_t refLayerIdx ) { m_directRefLayerIdx[layerIdx][refLayerIdc] = refLayerIdx; }

  uint32_t          getInterLayerRefIdc( uint32_t layerIdx, uint32_t refLayerIdx ) const { return m_interLayerRefIdx[layerIdx][refLayerIdx]; }
  void              setInterLayerRefIdc( uint32_t layerIdx, uint32_t refLayerIdx, uint32_t refLayerIdc ) { m_interLayerRefIdx[layerIdx][refLayerIdx] = refLayerIdc; }

  uint32_t          getGeneralLayerIdx(uint32_t layerId) const { return m_generalLayerIdx[layerId]; }
  void              setGeneralLayerIdx(uint32_t layerId, uint32_t layerIdc) { m_generalLayerIdx[layerId] = layerIdc; }

  bool              getEachLayerIsAnOlsFlag() const { return m_vpsEachLayerIsAnOlsFlag; }
  void              setEachLayerIsAnOlsFlag(bool t) { m_vpsEachLayerIsAnOlsFlag = t; }

  uint32_t          getOlsModeIdc() const { return m_vpsOlsModeIdc; }
  void              setOlsModeIdc(uint32_t t) { m_vpsOlsModeIdc = t; }

  uint32_t          getNumOutputLayerSets() const { return m_vpsNumOutputLayerSets; }
  void              setNumOutputLayerSets(uint32_t t) { m_vpsNumOutputLayerSets = t; }

  bool              getOlsOutputLayerFlag(uint32_t ols, uint32_t layer) const { return m_vpsOlsOutputLayerFlag[ols][layer]; }
  void              setOlsOutputLayerFlag(uint32_t ols, uint32_t layer, bool t) { m_vpsOlsOutputLayerFlag[ols][layer] = t; }

  uint32_t          getNumPtls() const                                   { return (uint32_t) m_vpsProfileTierLevel.size(); }
  void              setNumPtls(uint32_t val)                             { m_vpsProfileTierLevel.resize(val); }

  bool              getPtPresentFlag(int idx) const                      { return m_ptPresentFlag[idx]; }
  void              setPtPresentFlag(int idx, bool val)                  { m_ptPresentFlag[idx] = val; }

  uint32_t          getPtlMaxTemporalId(int idx) const                   { return m_ptlMaxTemporalId[idx]; }
  void              setPtlMaxTemporalId(int idx, uint32_t val)           { m_ptlMaxTemporalId[idx] = val; }

  void setProfileTierLevel(int idx, const ProfileTierLevel &ptl)         { m_vpsProfileTierLevel[idx] = ptl; }
  const ProfileTierLevel &getProfileTierLevel(int idx) const             { return m_vpsProfileTierLevel[idx]; }

  uint32_t          getOlsPtlIdx(int idx) const                          { return m_olsPtlIdx[idx]; }
  void              setOlsPtlIdx(int idx, uint32_t val)                  { m_olsPtlIdx[idx] = val; }

  bool              getVPSExtensionFlag() const                          { return m_vpsExtensionFlag; }
  void              setVPSExtensionFlag(bool t)                          { m_vpsExtensionFlag = t; }
  bool              getVPSGeneralHrdParamsPresentFlag() const            { return m_vpsGeneralHrdParamsPresentFlag; }
  void              setVPSGeneralHrdParamsPresentFlag(bool t)            { m_vpsGeneralHrdParamsPresentFlag = t; }
  bool              getVPSSublayerCpbParamsPresentFlag() const           { return m_vpsSublayerCpbParamsPresentFlag; }
  void              setVPSSublayerCpbParamsPresentFlag(bool t)           { m_vpsSublayerCpbParamsPresentFlag = t; }
  uint32_t          getNumOlsTimingHrdParamsMinus1() const               { return m_numOlsTimingHrdParamsMinus1; }
  void              setNumOlsTimingHrdParamsMinus1(uint32_t val)         { m_numOlsTimingHrdParamsMinus1 = val; }
  uint32_t          getHrdMaxTid(int olsIdx) const                       { return m_hrdMaxTid[olsIdx]; }
  void              setHrdMaxTid(int olsIdx, uint32_t val)               { m_hrdMaxTid[olsIdx] = val; }
  uint32_t          getOlsTimingHrdIdx(int olsIdx) const                 { return m_olsTimingHrdIdx[olsIdx]; }
  void              setOlsTimingHrdIdx(int olsIdx, uint32_t val)         { m_olsTimingHrdIdx[olsIdx] = val; }

  OlsHrdParams*          getOlsHrdParameters(int olsIdx)                 { return &m_olsHrdParams[olsIdx][0]; }
  const OlsHrdParams*    getOlsHrdParameters(int olsIdx) const           { return &m_olsHrdParams[olsIdx][0]; }

  GeneralHrdParams*          getGeneralHrdParameters()                   { return &m_generalHrdParams; }
  const GeneralHrdParams*    getGeneralHrdParameters() const             { return &m_generalHrdParams; }

  int               getTargetOlsIdx()                                    { return m_targetOlsIdx; }
  void              setTargetOlsIdx(uint32_t t)                          { m_targetOlsIdx = t; }

  int               getMaxDecPicBuffering( int temporalId ) const        { return m_dpbParameters[m_olsDpbParamsIdx[m_targetOlsIdx]].maxDecPicBuffering[temporalId]; }
  int               getMaxNumReorderPics( int temporalId ) const         { return m_dpbParameters[m_olsDpbParamsIdx[m_targetOlsIdx]].maxNumReorderPics[temporalId]; }
  int               getTotalNumOLSs() const                              { return m_totalNumOLSs; }
  int               getNumMultiLayeredOlss() const                       { return m_numMultiLayeredOlss; }
  Size              getOlsDpbPicSize( int olsIdx ) const                 { return m_olsDpbPicSize[olsIdx]; }
  void              setOlsDpbPicSize( int olsIdx, Size size )            { m_olsDpbPicSize[olsIdx] = size; }
  void              setOlsDpbPicWidth( int olsIdx, int width )           { m_olsDpbPicSize[olsIdx].width = width; }
  void              setOlsDpbPicHeight( int olsIdx, int height )         { m_olsDpbPicSize[olsIdx].height = height; }
  ChromaFormat      getOlsDpbChromaFormatIdc(int olsIdx) const { return m_olsDpbChromaFormatIdc[olsIdx]; }
  int               getOlsDpbBitDepthMinus8(int olsIdx) const            { return m_olsDpbBitDepthMinus8[olsIdx]; }
  void              setOlsDpbChromaFormatIdc(int olsIdx, ChromaFormat cf) { m_olsDpbChromaFormatIdc[olsIdx] = cf; }
  void              setOlsDpbBitDepthMinus8(int olsIdx, int bitDepthMinus8) { m_olsDpbBitDepthMinus8[olsIdx] = bitDepthMinus8; }

  int               getOlsDpbParamsIdx( int olsIdx ) const               { return m_olsDpbParamsIdx[olsIdx]; }
  void              setOlsDpbParamsIdx( int olsIdx, int paramIdx )       { m_olsDpbParamsIdx[olsIdx] = paramIdx; }

  void              deriveOutputLayerSets();
  void              deriveTargetOutputLayerSet( int targetOlsIdx );
  int               deriveTargetOLSIdx();
  uint32_t          getMaxTidinTOls(int m_targetOlsIdx);

  void              checkVPS();

  void              setNumLayersInOls(int olsIdx, int numLayers)         { m_numLayersInOls[olsIdx]  = numLayers; }
  int               getNumLayersInOls(int olsIdx)      const             { return m_numLayersInOls[olsIdx]; }

  void              setLayerIdInOls  (int olsIdx, int layerIdx, int layerId) { m_layerIdInOls[olsIdx][layerIdx] = layerId; }
  uint32_t          getLayerIdInOls  (int olsIdx, int layerIdx) const        { return m_layerIdInOls[olsIdx][layerIdx]; }
  std::vector<int>  getLayerIdsInOls(int targetOlsIdx)                       { return m_layerIdInOls[targetOlsIdx]; }

  int               getNumSubLayersInLayerInOLS (int olsIdx, int layerIdx) const { return m_numSubLayersInLayerInOLS[olsIdx][layerIdx]   ; }
};

