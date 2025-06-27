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

/** \file     EncCu.h
    \brief    Coding Unit (CU) encoder class (header)
*/

#ifndef __ENCCU__
#define __ENCCU__

// Include files
#include "CommonLib/CommonDef.h"
#include "CommonLib/IntraPrediction.h"
#include "CommonLib/InterPrediction.h"
#include "CommonLib/TrQuant.h"
#include "CommonLib/Unit.h"
#include "CommonLib/UnitPartitioner.h"
#include "CommonLib/IbcHashMap.h"
#include "CommonLib/DeblockingFilter.h"

#include "DecoderLib/DecCu.h"

#include "CABACWriter.h"
#include "IntraSearch.h"
#include "InterSearch.h"
#include "RateCtrl.h"
#include "EncModeCtrl.h"
//! \ingroup EncoderLib
//! \{

class EncLib;
class HLSWriter;
class EncSlice;
class EncGOP;

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// CU encoder class
struct GeoMergeCombo
{
  int splitDir;
  MergeIdxPair mergeIdx;
  double cost;
  GeoMergeCombo() : splitDir(0), mergeIdx{ 0, 0 }, cost(0.0){};
  GeoMergeCombo(int _splitDir, const MergeIdxPair &idx, double _cost)
    : splitDir(_splitDir), mergeIdx(idx), cost(_cost){};
};

class GeoComboCostList
{
public:
  GeoComboCostList() {};
  ~GeoComboCostList() {};
  std::vector<GeoMergeCombo> list;

  void sortByCost()
  {
    std::stable_sort(list.begin(), list.end(),
                     [](const GeoMergeCombo &a, const GeoMergeCombo &b) { return a.cost < b.cost; });
  };
};

class FastGeoCostList
{
  int m_maxNumGeoCand{ 0 };

  using CostArray = double[GEO_NUM_PARTITION_MODE][2];

  CostArray *m_singleDistList{ nullptr };

public:
  FastGeoCostList() {}
  ~FastGeoCostList()
  {
    delete[] m_singleDistList;
    m_singleDistList = nullptr;
  }

  void init(int maxNumGeoCand)
  {
    if (m_maxNumGeoCand != maxNumGeoCand)
    {
      delete[] m_singleDistList;
      m_singleDistList = nullptr;

      CHECK(maxNumGeoCand > MRG_MAX_NUM_CANDS, "Too many candidates");
      m_singleDistList = new CostArray[maxNumGeoCand];
      m_maxNumGeoCand  = maxNumGeoCand;
    }
  }

  void insert(int geoIdx, int partIdx, int mergeIdx, double cost)
  {
    CHECKD(geoIdx >= GEO_NUM_PARTITION_MODE, "geoIdx is too large");
    CHECKD(mergeIdx >= m_maxNumGeoCand, "mergeIdx is too large");
    CHECKD(partIdx >= 2, "partIdx is too large");

    m_singleDistList[mergeIdx][geoIdx][partIdx] = cost;
  }

  double getCost(const int splitDir, const MergeIdxPair &mergeCand)
  {
    return m_singleDistList[mergeCand[0]][splitDir][0] + m_singleDistList[mergeCand[1]][splitDir][1];
  }
};

class MergeItem
{
private:
  PelStorage m_pelStorage;
  std::vector<MotionInfo> m_mvStorage;

public:
  enum class MergeItemType
  {
    REGULAR,
    SBTMVP,
    AFFINE,
    MMVD,
    CIIP,
    GPM,
    IBC,
    NUM,
  };

  double        cost;
  std::array<MvField[2], AFFINE_MAX_NUM_CP> mvField;
  int           mergeIdx;
  uint8_t       bcwIdx;
  uint8_t       interDir;
  bool          useAltHpelIf;
  bool          useInterLayerRef;
  AffineModel   affineType;

  bool          noResidual;
  bool          noBdofRefine;

  bool          lumaPredReady;
  bool          chromaPredReady;

  MergeItemType mergeItemType;
  MotionBuf     mvBuf;

#if GDR_ENABLED
  bool          mvSolid[2];
  bool          mvValid[2];
#endif

  MergeItem();
  ~MergeItem();

  void          create(ChromaFormat chromaFormat, const Area& area);
  void          importMergeInfo(const MergeCtx& mergeCtx, int _mergeIdx, MergeItemType _mergeItemType, PredictionUnit& pu);
  void          importMergeInfo(const AffineMergeCtx& mergeCtx, int _mergeIdx, MergeItemType _mergeItemType, const UnitArea& unitArea);
  bool          exportMergeInfo(PredictionUnit& pu, bool forceNoResidual);
  PelUnitBuf    getPredBuf(const UnitArea& unitArea) { return m_pelStorage.getBuf(unitArea); }
  MotionBuf     getMvBuf(const UnitArea& unitArea) { return MotionBuf(m_mvStorage.data(), g_miScaling.scale(unitArea.lumaSize())); }

  static int getGpmUnfiedIndex(int splitDir, const MergeIdxPair& geoMergeIdx)
  {
    return (splitDir << 8) | (geoMergeIdx[0] << 4) | geoMergeIdx[1];
  }
  static void updateGpmIdx(int mergeIdx, uint8_t& splitDir, MergeIdxPair& geoMergeIdx)
  {
    splitDir = (mergeIdx >> 8) & 0xFF;
    geoMergeIdx[0] = (mergeIdx >> 4) & 0xF;
    geoMergeIdx[1] = mergeIdx & 0xF;
  }
};

class MergeItemList
{
private:
  Pool<MergeItem> m_mergeItemPool;
  std::vector<MergeItem *> m_list;
  size_t m_maxTrackingNum = 0;
  std::vector<MergeItem *> m_backupList;
  size_t m_maxTrackingBackupNum = 0;
  int m_numInterLayers = 0;
  ChromaFormat  m_chromaFormat;
  Area m_ctuArea;

public:
  MergeItemList();
  ~MergeItemList();

  void          init(size_t maxSize, ChromaFormat chromaFormat, int ctuWidth, int ctuHeight);
  MergeItem*    allocateNewMergeItem();
  void          insertMergeItemToList(MergeItem* p, bool trackInterLayerCand=false);
  void          resetList(size_t maxTrackingNum);
  MergeItem*    getMergeItemInList(size_t index);
  size_t        size() { return m_list.size(); }

  void          insertMergeItemToBackupList(MergeItem* p);
  void          resetBackupList(size_t maxTrackingBackupNum);
  void          insertBackupItemsToList();
  void          swapItems(int index_first, int index_second) { std::swap(m_list[index_first], m_list[index_second]); }
  int           getNumInterLayerItems() { return m_numInterLayers; }

};

class EncCu
  : DecCu
{
private:
  bool m_bestModeUpdated;
  struct CtxPair
  {
    Ctx start;
    Ctx best;
  };

  std::vector<CtxPair>  m_ctxBuffer;
  CtxPair*              m_CurrCtx;
  CtxPool              *m_ctxPool;

  //  Data : encoder control
  int                   m_cuChromaQpOffsetIdxPlus1; // if 0, then cu_chroma_qp_offset_flag will be 0, otherwise cu_chroma_qp_offset_flag will be 1.

  XuPool m_unitPool;
  PelUnitBufPool m_pelUnitBufPool;

  CodingStructure    ***m_pTempCS;
  CodingStructure    ***m_pBestCS;
  CodingStructure    ***m_pTempCS2;
  CodingStructure    ***m_pBestCS2;
  //  Access channel
  EncCfg*               m_pcEncCfg;
  IntraSearch*          m_pcIntraSearch;
  InterSearch*          m_pcInterSearch;
  TrQuant*              m_pcTrQuant;
  RdCost*               m_pcRdCost;
  EncSlice*             m_pcSliceEncoder;
  DeblockingFilter*     m_deblockingFilter;
  EncGOP*               m_pcGOPEncoder;

  CABACWriter*          m_CABACEstimator;
  RateCtrl*             m_pcRateCtrl;
  IbcHashMap            m_ibcHashMap;
  EncModeCtrl          *m_modeCtrl;

  FastGeoCostList       m_geoCostList;
  double                m_AFFBestSATDCost;
  double                m_mergeBestSATDCost;
  MotionInfo            m_SubPuMiBuf      [( MAX_CU_SIZE * MAX_CU_SIZE ) >> ( MIN_CU_LOG2 << 1 )];

  int                   m_ctuIbcSearchRangeX;
  int                   m_ctuIbcSearchRangeY;

  std::array<int, 2>    m_bestBcwIdx;
  std::array<double, 2> m_bestBcwCost;

  static const MergeIdxPair m_geoModeTest[GEO_MAX_NUM_CANDS];

#if SHARP_LUMA_DELTA_QP || ENABLE_QPA_SUB_CTU
  void    updateLambda      ( Slice* slice, const int dQP,
 #if WCG_EXT && ER_CHROMA_QP_WCG_PPS
                              const bool useWCGChromaControl,
 #endif
                              const bool updateRdCostLambda );
#endif
  double                m_sbtCostSave[2];

  GeoComboCostList m_comboList;
  MergeItemList         m_mergeItemList;

public:
  /// copy parameters from encoder class
  void  init                ( EncLib* pcEncLib, const SPS& sps );

  void setDecCuReshaperInEncCU(EncReshape* pcReshape, ChromaFormat chromaFormatIdc)
  {
    initDecCuReshaper((Reshape*) pcReshape, chromaFormatIdc);
  }
  /// create internal buffers
  void  create              ( EncCfg* encCfg );

  /// destroy internal buffers
  void  destroy             ();

  /// CTU analysis function
  void compressCtu(CodingStructure &cs, const UnitArea &area, const unsigned ctuRsAddr,
                   const EnumArray<int, ChannelType> &prevQP, const EnumArray<int, ChannelType> &currQP);
  /// CTU encoding function
  int   updateCtuDataISlice ( const CPelBuf buf );

  EncModeCtrl* getModeCtrl  () { return m_modeCtrl; }


  void   setMergeBestSATDCost(double cost) { m_mergeBestSATDCost = cost; }
  double getMergeBestSATDCost()            { return m_mergeBestSATDCost; }
  void   setAFFBestSATDCost(double cost)   { m_AFFBestSATDCost = cost; }
  double getAFFBestSATDCost()              { return m_AFFBestSATDCost; }
  IbcHashMap& getIbcHashMap()              { return m_ibcHashMap;        }
  EncCfg*     getEncCfg()            const { return m_pcEncCfg;          }

  EncCu();
  ~EncCu();

protected:

  void xCalDebCost            ( CodingStructure &cs, Partitioner &partitioner, bool calDist = false );
  Distortion getDistortionDb  ( CodingStructure &cs, CPelBuf org, CPelBuf reco, ComponentID compID, const CompArea& compArea, bool afterDb );

  void xCompressCU            ( CodingStructure*& tempCS, CodingStructure*& bestCS, Partitioner& pm, double maxCostAllowed = MAX_DOUBLE );

  bool
    xCheckBestMode         ( CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &pm, const EncTestMode& encTestmode );

  void xCheckModeSplit        ( CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &pm, const EncTestMode& encTestMode, const ModeType modeTypeParent, bool &skipInterPass, double *splitRdCostBest);

  bool xCheckRDCostIntra(CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &pm, const EncTestMode& encTestMode, bool adaptiveColorTrans);

  void xCheckDQP              ( CodingStructure& cs, Partitioner& partitioner, bool bKeepCtx = false);
  void xCheckChromaQPOffset   ( CodingStructure& cs, Partitioner& partitioner);

  void xCheckRDCostHashInter  ( CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &pm, const EncTestMode& encTestMode );
  void xCheckRDCostInter      ( CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &pm, const EncTestMode& encTestMode );
  bool xCheckRDCostInterAmvr(CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &pm,
                             const EncTestMode &encTestMode, double &bestIntPelCost);
  void xEncodeDontSplit       ( CodingStructure &cs, Partitioner &partitioner);


  void xCheckRDCostUnifiedMerge ( CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &pm, const EncTestMode& encTestMode );

  void xEncodeInterResidual(CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &partitioner,
                            const EncTestMode &encTestMode, int residualPass = 0, bool *bestHasNonResi = nullptr,
                            double *equBcwCost = nullptr);
#if REUSE_CU_RESULTS
  void xReuseCachedResult     ( CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &Partitioner );
#endif
  bool xIsBcwSkip(const CodingUnit& cu)
  {
    if (cu.slice->getSliceType() != B_SLICE)
    {
      return true;
    }
    return((m_pcEncCfg->getBaseQP() > 32) && ((cu.slice->getTLayer() >= 4)
       || ((cu.refIdxBi[0] >= 0 && cu.refIdxBi[1] >= 0)
       && (abs(cu.slice->getPOC() - cu.slice->getRefPOC(REF_PIC_LIST_0, cu.refIdxBi[0])) == 1
       ||  abs(cu.slice->getPOC() - cu.slice->getRefPOC(REF_PIC_LIST_1, cu.refIdxBi[1])) == 1))));
  }
  void xCheckRDCostIBCMode    ( CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &pm, const EncTestMode& encTestMode );
  void xCheckRDCostIBCModeMerge2Nx2N( CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &partitioner, const EncTestMode& encTestMode );

  void xCheckPLT              ( CodingStructure *&tempCS, CodingStructure *&bestCS, Partitioner &partitioner, const EncTestMode& encTestMode );

  PredictionUnit* getPuForInterPrediction(CodingStructure* cs);
  unsigned int updateRdCheckingNum(double threshold, unsigned int numMergeSatdCand);

  void generateMergePrediction(const UnitArea& unitArea, MergeItem* mergeItem, PredictionUnit& pu, bool luma, bool chroma,
    PelUnitBuf& dstBuf, bool finalRd, bool forceNoResidual, PelUnitBuf* predBuf1, PelUnitBuf* predBuf2);
  double calcLumaCost4MergePrediction(const TempCtx& ctxStart, const PelUnitBuf& predBuf, double lambda, PredictionUnit& pu, DistParam& distParam);

  template <size_t N>
  void addRegularCandsToPruningList(const MergeCtx& mergeCtx, const UnitArea& localUnitArea, double sqrtLambdaForFirstPassIntra,
    const TempCtx& ctxStart, int numDmvrMvd, Mv dmvrL0Mvd[MRG_MAX_NUM_CANDS][MAX_NUM_SUBCU_DMVR], bool dmvrImpreciseMv[MRG_MAX_NUM_CANDS],
    PelUnitBufVector<N>& mrgPredBufNoCiip, PelUnitBufVector<N>& mrgPredBufNoMvRefine, DistParam& distParam, PredictionUnit* pu);
  template <size_t N>
  void addCiipCandsToPruningList(const MergeCtx& mergeCtx, const UnitArea& localUnitArea, double sqrtLambdaForFirstPassIntra,
    const TempCtx& ctxStart, PelUnitBufVector<N>& mrgPredBufNoCiip, PelUnitBufVector<N>& mrgPredBufNoMvRefine, DistParam& distParam, PredictionUnit* pu);
  void addMmvdCandsToPruningList(const MergeCtx& mergeCtx, const UnitArea& localUnitArea, double sqrtLambdaForFirstPassIntra,
    const TempCtx& ctxStart, DistParam& distParam, PredictionUnit* pu);
  void addAffineCandsToPruningList(AffineMergeCtx& affineMergeCtx, const UnitArea& localUnitArea, double sqrtLambdaForFirstPass,
    const TempCtx& ctxStart, DistParam& distParam, PredictionUnit* pu);
  template <size_t N>
  void addGpmCandsToPruningList(const MergeCtx& mergeCtx, const UnitArea& localUnitArea, double sqrtLambdaForFirstPass,
    const TempCtx& ctxStart, const GeoComboCostList& comboList, PelUnitBufVector<N>& geoBuffer, DistParam& distParamSAD2, PredictionUnit* pu);

  template<size_t N>
  bool prepareGpmComboList(const MergeCtx& mergeCtx, const UnitArea& localUnitArea, double sqrtLambdaForFirstPass,
    GeoComboCostList& comboList, PelUnitBufVector<N>& geoBuffer, PredictionUnit* pu);
  void checkEarlySkip(const CodingStructure* bestCS, const Partitioner &partitioner);

};

//! \}

#endif // __ENCMB__
