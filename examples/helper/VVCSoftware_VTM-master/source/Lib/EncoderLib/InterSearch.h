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

/** \file     InterSearch.h
    \brief    inter search class (header)
 */

#ifndef __INTERSEARCH__
#define __INTERSEARCH__

// Include files
#include "CABACWriter.h"
#include "EncCfg.h"

#include "CommonLib/MotionInfo.h"
#include "CommonLib/InterPrediction.h"
#include "CommonLib/TrQuant.h"
#include "CommonLib/Unit.h"
#include "CommonLib/UnitPartitioner.h"
#include "CommonLib/RdCost.h"

#include "CommonLib/AffineGradientSearch.h"
#include "CommonLib/IbcHashMap.h"
#include "CommonLib/Hash.h"
#include <unordered_map>
#include <vector>
#include "EncReshape.h"
//! \ingroup EncoderLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

static constexpr uint32_t MAX_NUM_REF_LIST_ADAPT_SR = NUM_REF_PIC_LIST_01;
static constexpr uint32_t MAX_IDX_ADAPT_SR          = MAX_NUM_REF;
static constexpr uint32_t NUM_MV_PREDICTORS         = 3;
struct BlkRecord
{
  std::unordered_map<Mv, Distortion> bvRecord;
};
class EncModeCtrl;

struct AffineMVInfo
{
  RefSetArray<Mv[3]> affMVs;
  int x, y, w, h;
};

#if GDR_ENABLED
struct AffineMVInfoSolid
{
  RefSetArray<bool[3]> affMVsSolid;
};
#endif
struct BlkUniMvInfo
{
  RefSetArray<Mv> uniMvs;
  int x, y, w, h;
};

typedef struct
{
  Mv acMvAffine4Para[2][3];
  Mv acMvAffine6Para[2][3];
  int16_t affine4ParaRefIdx[2];
  int16_t affine6ParaRefIdx[2];
  Distortion hevcCost[3];
  Distortion affineCost[3];
  bool affine4ParaAvail;
  bool affine6ParaAvail;

#if GDR_ENABLED
  bool acMvAffine4ParaSolid[2][3];
  bool acMvAffine6ParaSolid[2][3];
#endif
} EncAffineMotion;

/// encoder search class
class InterSearch : public InterPrediction, AffineGradientSearch
{
private:
  EncModeCtrl     *m_modeCtrl;

  PelStorage      m_tmpPredStorage              [NUM_REF_PIC_LIST_01];
  PelStorage      m_tmpStorageCtu;
  PelStorage      m_tmpAffiStorage;
  Pel*            m_tmpAffiError;
  int*            m_tmpAffiDeri[2];

  CodingStructure ****m_pSplitCS;
  CodingStructure ****m_pFullCS;

  CodingStructure **m_pSaveCS;

  ClpRng          m_lumaClpRng;
  uint32_t        m_estWeightIdxBits[BCW_NUM];
  BcwMotionParam  m_uniMotions;
  bool            m_affineModeSelected;
  std::unordered_map< Position, std::unordered_map< Size, BlkRecord> > m_ctuRecord;
  AffineMVInfo       *m_affMVList;
#if GDR_ENABLED
  AffineMVInfoSolid  *m_affMVListSolid;
#endif
  int             m_affMVListIdx;
  int             m_affMVListSize;
  int             m_affMVListMaxSize;
  BlkUniMvInfo*   m_uniMvList;
  int             m_uniMvListIdx;
  int             m_uniMvListSize;
  int             m_uniMvListMaxSize;
  Distortion      m_hevcCost;
#if GDR_ENABLED
  bool            m_hevcCostOk;
#endif
  EncAffineMotion m_affineMotion;
  static_vector<Mv, IBC_NUM_CANDIDATES> m_defaultCachedBvs;
protected:
  // interface to option
  EncCfg*         m_pcEncCfg;

  // interface to classes
  TrQuant*        m_pcTrQuant;
  EncReshape*     m_pcReshape;

  // ME parameters
  int             m_searchRange;
  int             m_bipredSearchRange; // Search range for bi-prediction
  MESearchMethod  m_motionEstimationSearchMethod;
  int             m_adaptSR[MAX_NUM_REF_LIST_ADAPT_SR][MAX_IDX_ADAPT_SR];

  // RD computation
  CABACWriter*    m_CABACEstimator;
  CtxPool        *m_ctxPool;
  DistParam       m_cDistParam;

  RefPicList      m_currRefPicList;
  int             m_currRefPicIndex;
  bool            m_skipFracME;

  RefSetArray<int>                         m_numHashMVStoreds;
  RefSetArray<Mv[Hash::NUM_LOG_BLK_SIZES]> m_hashMVStoreds;

  // Misc.
  Pel            *m_pTempPel;

  // AMVP cost computation
  uint32_t            m_auiMVPIdxCost               [AMVP_MAX_NUM_CANDS+1][AMVP_MAX_NUM_CANDS+1]; //th array bounds

  RefSetArray<Mv> m_integerMv2Nx2N;

  bool            m_isInitialized;

  static_vector<Mv, 2 * IBC_NUM_CANDIDATES> m_acBVs;
  bool            m_useCompositeRef;
  Distortion      m_estMinDistSbt[NUMBER_SBT_MODE + 1]; // estimated minimum SSE value of the PU if using a SBT mode
  uint8_t         m_sbtRdoOrder[NUMBER_SBT_MODE];       // order of SBT mode in RDO
  bool            m_skipSbtAll;                         // to skip all SBT modes for the current PU
  uint8_t         m_histBestSbt;                        // historical best SBT mode for PU of certain SSE values
  MtsType         m_histBestMtsIdx;                     // historical best MTS idx  for PU of certain SSE values
  bool            m_clipMvInSubPic;

public:
  InterSearch();
  virtual ~InterSearch();

  void init(EncCfg *pcEncCfg, TrQuant *pcTrQuant, int searchRange, int bipredSearchRange,
            MESearchMethod motionEstimationSearchMethod, bool useCompositeRef, const uint32_t maxCUWidth,
            const uint32_t maxCUHeight, const uint32_t maxTotalCUDepth, RdCost *pcRdCost, CABACWriter *CABACEstimator,
            CtxPool *ctxPool, EncReshape *m_pcReshape);

  void destroy                      ();

  void       calcMinDistSbt         ( CodingStructure &cs, const CodingUnit& cu, const uint8_t sbtAllowed );
  uint8_t    skipSbtByRDCost        ( int width, int height, int mtDepth, uint8_t sbtIdx, uint8_t sbtPos, double bestCost, Distortion distSbtOff, double costSbtOff, bool rootCbfSbtOff );
  bool       getSkipSbtAll          ()                 { return m_skipSbtAll; }
  void       setSkipSbtAll          ( bool skipAll )   { m_skipSbtAll = skipAll; }
  uint8_t    getSbtRdoOrder         ( uint8_t idx )    { assert( m_sbtRdoOrder[idx] < NUMBER_SBT_MODE ); assert( (uint32_t)( m_estMinDistSbt[m_sbtRdoOrder[idx]] >> 2 ) < ( MAX_UINT >> 1 ) ); return m_sbtRdoOrder[idx]; }
  Distortion getEstDistSbt          ( uint8_t sbtMode) { return m_estMinDistSbt[sbtMode]; }
  void       initTuAnalyzer         ()                 { m_estMinDistSbt[NUMBER_SBT_MODE] = std::numeric_limits<uint64_t>::max(); m_skipSbtAll = false; }
  void       setHistBestTrs(uint8_t sbtInfo, MtsType mtsIdx)
  {
    m_histBestSbt    = sbtInfo;
    m_histBestMtsIdx = mtsIdx;
  }
  void       initSbtRdoOrder        ( uint8_t sbtMode ) { m_sbtRdoOrder[0] = sbtMode; m_estMinDistSbt[0] = m_estMinDistSbt[sbtMode]; }

  void setTempBuffers               (CodingStructure ****pSlitCS, CodingStructure ****pFullCS, CodingStructure **pSaveCS );
  void resetCtuRecord               ()             { m_ctuRecord.clear(); }
  void setAffineModeSelected        ( bool flag) { m_affineModeSelected = flag; }
  void resetAffineMVList() { m_affMVListIdx = 0; m_affMVListSize = 0; }
#if GDR_ENABLED
  void savePrevAffMVInfo(int idx, AffineMVInfo &tmpMVInfo, AffineMVInfoSolid &tmpMVInfoSolid, bool& isSaved)
#else
  void savePrevAffMVInfo(int idx, AffineMVInfo &tmpMVInfo, bool& isSaved)
#endif
  {
    if (m_affMVListSize > idx)
    {
      tmpMVInfo = m_affMVList[(m_affMVListIdx - 1 - idx + m_affMVListMaxSize) % m_affMVListMaxSize];
#if GDR_ENABLED
      tmpMVInfoSolid = m_affMVListSolid[(m_affMVListIdx - 1 - idx + m_affMVListMaxSize) % m_affMVListMaxSize];
#endif
      isSaved = true;
    }
    else
      isSaved = false;
  }
#if GDR_ENABLED
  void addAffMVInfo(AffineMVInfo &tmpMVInfo, AffineMVInfoSolid &tmpMVInfoSolid)
#else
  void addAffMVInfo(AffineMVInfo &tmpMVInfo)
#endif
  {
    int j = 0;
    AffineMVInfo *prevInfo = nullptr;
#if GDR_ENABLED
    AffineMVInfoSolid *prevInfoSolid = nullptr;
#endif
    for (; j < m_affMVListSize; j++)
    {
      prevInfo = m_affMVList + ((m_affMVListIdx - j - 1 + m_affMVListMaxSize) % (m_affMVListMaxSize));
#if GDR_ENABLED
      prevInfoSolid = m_affMVListSolid + ((m_affMVListIdx - j - 1 + m_affMVListMaxSize) % (m_affMVListMaxSize));
#endif
      if ((tmpMVInfo.x == prevInfo->x) && (tmpMVInfo.y == prevInfo->y) && (tmpMVInfo.w == prevInfo->w) && (tmpMVInfo.h == prevInfo->h))
      {
        break;
      }
    }
#if GDR_ENABLED
    if (j < m_affMVListSize)
    {
      *prevInfo = tmpMVInfo;
      *prevInfoSolid = tmpMVInfoSolid;
    }
#else
    if (j < m_affMVListSize)
    {
      *prevInfo = tmpMVInfo;
    }
#endif
    else
    {
      m_affMVList[m_affMVListIdx] = tmpMVInfo;
#if GDR_ENABLED
      m_affMVListSolid[m_affMVListIdx] = tmpMVInfoSolid;
#endif
      m_affMVListIdx = (m_affMVListIdx + 1) % m_affMVListMaxSize;
      m_affMVListSize = std::min(m_affMVListSize + 1, m_affMVListMaxSize);
    }
  }
  void resetUniMvList() { m_uniMvListIdx = 0; m_uniMvListSize = 0; }
  void insertUniMvCands(CompArea blkArea, RefSetArray<Mv> &cMvTemp)
  {
    BlkUniMvInfo* curMvInfo = m_uniMvList + m_uniMvListIdx;
    int j = 0;
    for (; j < m_uniMvListSize; j++)
    {
      BlkUniMvInfo* prevMvInfo = m_uniMvList + ((m_uniMvListIdx - 1 - j + m_uniMvListMaxSize) % (m_uniMvListMaxSize));
      if ((blkArea.x == prevMvInfo->x) && (blkArea.y == prevMvInfo->y) && (blkArea.width == prevMvInfo->w) && (blkArea.height == prevMvInfo->h))
      {
        break;
      }
    }

    if (j < m_uniMvListSize)
    {
      curMvInfo = m_uniMvList + ((m_uniMvListIdx - 1 - j + m_uniMvListMaxSize) % (m_uniMvListMaxSize));
    }

    ::memcpy(curMvInfo->uniMvs, cMvTemp, sizeof(cMvTemp));
    if (j == m_uniMvListSize)  // new element
    {
      curMvInfo->x = blkArea.x;
      curMvInfo->y = blkArea.y;
      curMvInfo->w = blkArea.width;
      curMvInfo->h = blkArea.height;
      m_uniMvListSize = std::min(m_uniMvListSize + 1, m_uniMvListMaxSize);
      m_uniMvListIdx = (m_uniMvListIdx + 1) % (m_uniMvListMaxSize);
    }
  }
  void savePrevUniMvInfo(CompArea blkArea, BlkUniMvInfo &tmpUniMvInfo, bool& isUniMvInfoSaved)
  {
    int j = 0;
    BlkUniMvInfo* curUniMvInfo = nullptr;
    for (; j < m_uniMvListSize; j++)
    {
      curUniMvInfo = m_uniMvList + ((m_uniMvListIdx - 1 - j + m_uniMvListMaxSize) % (m_uniMvListMaxSize));
      if ((blkArea.x == curUniMvInfo->x) && (blkArea.y == curUniMvInfo->y) && (blkArea.width == curUniMvInfo->w) && (blkArea.height == curUniMvInfo->h))
      {
        break;
      }
    }

    if (j < m_uniMvListSize)
    {
      isUniMvInfoSaved = true;
      tmpUniMvInfo = *curUniMvInfo;
    }
  }
  void addUniMvInfo(BlkUniMvInfo &tmpUniMVInfo)
  {
    int j = 0;
    BlkUniMvInfo* prevUniMvInfo = nullptr;
    for (; j < m_uniMvListSize; j++)
    {
      prevUniMvInfo = m_uniMvList + ((m_uniMvListIdx - 1 - j + m_uniMvListMaxSize) % (m_uniMvListMaxSize));
      if ((tmpUniMVInfo.x == prevUniMvInfo->x) && (tmpUniMVInfo.y == prevUniMvInfo->y) && (tmpUniMVInfo.w == prevUniMvInfo->w) && (tmpUniMVInfo.h == prevUniMvInfo->h))
      {
        break;
      }
    }
    if (j < m_uniMvListSize)
    {
      *prevUniMvInfo = tmpUniMVInfo;
    }
    else
    {
      m_uniMvList[m_uniMvListIdx] = tmpUniMVInfo;
      m_uniMvListIdx = (m_uniMvListIdx + 1) % m_uniMvListMaxSize;
      m_uniMvListSize = std::min(m_uniMvListSize + 1, m_uniMvListMaxSize);
    }
  }
  void resetSavedAffineMotion();

#if GDR_ENABLED
  void storeAffineMotion(Mv acAffineMv[2][3], bool acAffineMvSolid[2][3], int8_t affineRefIdx[2],
                         AffineModel affineType, int bcwIdx);
#else
  void storeAffineMotion(Mv acAffineMv[2][3], int8_t affineRefIdx[2], AffineModel affineType, int bcwIdx);
#endif

  bool isValidBv(PredictionUnit& pu, int xPos, int yPos, int width, int height, int picWidth, int picHeight, int xBv,
                 int yBv, int ctuSize);
  void setClipMvInSubPic(bool flag) { m_clipMvInSubPic = flag; }
protected:

  /// sub-function for motion vector refinement used in fractional-pel accuracy
#if GDR_ENABLED
  Distortion xPatternRefinement(const PredictionUnit &pu, RefPicList eRefPicList, int refIdx,
                                const CPelBuf *pcPatternKey, Mv baseRefMv, int iFrac, Mv &rcMvFrac,
                                bool bAllowUseOfHadamard, bool &rbCleanCandExist);
#else
  Distortion  xPatternRefinement    ( const CPelBuf* pcPatternKey, Mv baseRefMv, int iFrac, Mv& rcMvFrac, bool bAllowUseOfHadamard );
#endif

   typedef struct
   {
     int left;
     int right;
     int top;
     int bottom;
   }SearchRange;

  typedef struct
  {
    SearchRange searchRange;
    const CPelBuf* pcPatternKey;
    const Pel*  piRefY;
    ptrdiff_t       iRefStride;
    int         iBestX;
    int         iBestY;
    uint32_t        uiBestRound;
    uint32_t        uiBestDistance;
    Distortion  uiBestSad;
    uint8_t       ucPointNr;
    int         subShiftMode;
    unsigned    imvShift;
    bool        useAltHpelIf;
    bool        inCtuSearch;
    bool        zeroMV;
  } IntTZSearchStruct;

  // sub-functions for ME
  inline void xTZSearchHelp         ( IntTZSearchStruct& rcStruct, const int iSearchX, const int iSearchY, const uint8_t ucPointNr, const uint32_t uiDistance );
  inline void xTZ2PointSearch       ( IntTZSearchStruct& rcStruct );
  inline void xTZ8PointSquareSearch ( IntTZSearchStruct& rcStruct, const int iStartX, const int iStartY, const int iDist );
  inline void xTZ8PointDiamondSearch( IntTZSearchStruct& rcStruct, const int iStartX, const int iStartY, const int iDist, const bool bCheckCornersAtDist1 );

  Distortion xGetInterPredictionError( PredictionUnit& pu, PelUnitBuf& origBuf, const RefPicList &eRefPicList = REF_PIC_LIST_X );

public:
  /// encoder estimation - inter prediction (non-skip)

  void setModeCtrl( EncModeCtrl *modeCtrl ) { m_modeCtrl = modeCtrl;}

  void predInterSearch(CodingUnit& cu, Partitioner& partitioner );

  /// set ME search range
  void setAdaptiveSearchRange(int dir, int refIdx, int searchRange)
  {
    CHECK(dir >= MAX_NUM_REF_LIST_ADAPT_SR || refIdx >= int(MAX_IDX_ADAPT_SR), "Invalid index");
    m_adaptSR[dir][refIdx] = searchRange;
  }
  bool  predIBCSearch           ( CodingUnit& cu, Partitioner& partitioner, const int localSearchRangeX, const int localSearchRangeY, IbcHashMap& ibcHashMap);
  void  xIntraPatternSearch         ( PredictionUnit& pu, IntTZSearchStruct&  cStruct, Mv& rcMv, Distortion&  ruiCost, Mv* cMvSrchRngLT, Mv* cMvSrchRngRB, Mv* pcMvPred);
  void  xSetIntraSearchRange        ( PredictionUnit& pu, int iRoiWidth, int iRoiHeight, const int localSearchRangeX, const int localSearchRangeY, Mv& rcMvSrchRngLT, Mv& rcMvSrchRngRB);
  void  resetIbcSearch()
  {
    m_defaultCachedBvs.clear();
  }
  void  xIBCEstimation   ( PredictionUnit& pu, PelUnitBuf& origBuf, Mv     *pcMvPred, Mv     &rcMv, Distortion &ruiCost, const int localSearchRangeX, const int localSearchRangeY);
  void  xIBCSearchMVCandUpdate  ( Distortion  uiSad, int x, int y, Distortion* uiSadBestCand, static_vector<Mv, CHROMA_REFINEMENT_CANDIDATES>& cMVCand);
  int   xIBCSearchMVChromaRefine( PredictionUnit& pu, int iRoiWidth, int iRoiHeight, int cuPelX, int cuPelY, Distortion* uiSadBestCand, static_vector<Mv, CHROMA_REFINEMENT_CANDIDATES>& cMVCand);
  void addToSortList(std::list<BlockHash>& listBlockHash, std::list<int>& listCost, int cost, const BlockHash& blockHash);
  bool predInterHashSearch(CodingUnit& cu, Partitioner& partitioner, bool& isPerfectMatch);
  bool xHashInterEstimation(PredictionUnit& pu, RefPicList& bestRefPicList, int& bestRefIndex, Mv& bestMv, Mv& bestMvd, int& bestMVPIndex, bool& isPerfectMatch);
  bool xRectHashInterEstimation(PredictionUnit& pu, RefPicList& bestRefPicList, int& bestRefIndex, Mv& bestMv, Mv& bestMvd, int& bestMVPIndex, bool& isPerfectMatch);
  void selectRectangleMatchesInter(const MapIterator& itBegin, int count, std::list<BlockHash>& listBlockHash, const BlockHash& currBlockHash, int width, int height, int idxNonSimple, unsigned int* &hashValues, int baseNum, int picWidth, int picHeight, bool isHorizontal, uint16_t* curHashPic);
  void selectMatchesInter(const MapIterator& itBegin, int count, std::list<BlockHash>& vecBlockHash, const BlockHash& currBlockHash);
protected:

  // -------------------------------------------------------------------------------------------------------------------
  // Inter search (AMP)
  // -------------------------------------------------------------------------------------------------------------------

  void xEstimateMvPredAMVP(PredictionUnit &pu, PelUnitBuf &origBuf, RefPicList eRefPicList, int refIdx, Mv &rcMvPred,
                           AMVPInfo &amvpInfo, bool bFilled = false, Distortion *puiDistBiP = nullptr);

#if GDR_ENABLED
  void xCheckBestMVP(PredictionUnit &pu, RefPicList eRefPicList, Mv cMv, Mv &rcMvPred, int &riMVPIdx,
                     AMVPInfo &amvpInfo, uint32_t &ruiBits, Distortion &ruiCost, const uint8_t imv);
#else
  void xCheckBestMVP(RefPicList eRefPicList, Mv cMv, Mv &rcMvPred, int &riMVPIdx, AMVPInfo &amvpInfo, uint32_t &ruiBits,
                     Distortion &ruiCost, const uint8_t imv);
#endif

  Distortion xGetTemplateCost(const PredictionUnit &pu, PelUnitBuf &origBuf, PelUnitBuf &predBuf, Mv cMvCand,
                              int mvpIdx, int mvpNum, RefPicList eRefPicList, int refIdx);
  uint32_t xCalcAffineMVBits      ( PredictionUnit& pu, Mv mvCand[3], Mv mvPred[3] );

  void xCopyAMVPInfo              ( AMVPInfo*   pSrc, AMVPInfo* pDst );
  uint32_t xGetMvpIdxBits(int idx, int num);
  void     xGetBlkBits(bool isPSlice, uint32_t blkBit[3]);

  // -------------------------------------------------------------------------------------------------------------------
  // motion estimation
  // -------------------------------------------------------------------------------------------------------------------

#if GDR_ENABLED
  void xMotionEstimation(PredictionUnit &pu, PelUnitBuf &origBuf, RefPicList eRefPicList, Mv &rcMvPred, int refIdxPred,
                         Mv &rcMv, bool &rcMvSolid, int &riMVPIdx, uint32_t &ruiBits, Distortion &ruiCost,
                         const AMVPInfo &amvpInfo, bool &rbCleanCandExist, bool bBi = false);
#else
  void xMotionEstimation(PredictionUnit &pu, PelUnitBuf &origBuf, RefPicList eRefPicList, Mv &rcMvPred, int refIdxPred,
                         Mv &rcMv, int &riMVPIdx, uint32_t &ruiBits, Distortion &ruiCost, const AMVPInfo &amvpInfo,
                         bool bBi = false);
#endif
  void xTZSearch(const PredictionUnit &pu, RefPicList eRefPicList, int refIdxPred, IntTZSearchStruct &cStruct, Mv &rcMv,
                 Distortion &ruiSAD, const Mv *const pIntegerMv2Nx2NPred, const bool bExtendedSettings,
                 const bool bFastSettings = false);

  void xTZSearchSelective(const PredictionUnit &pu, RefPicList eRefPicList, int refIdxPred, IntTZSearchStruct &cStruct,
                          Mv &rcMv, Distortion &ruiSAD, const Mv *const pIntegerMv2Nx2NPred);

  void xSetSearchRange(const PredictionUnit &pu, const Mv &cMvPred, const int iSrchRng, SearchRange &sr,
                       IntTZSearchStruct &cStruct
#if GDR_ENABLED
                       ,
                       RefPicList eRefPicList, int refIdx
#endif
  );

  void xPatternSearchFast(const PredictionUnit &pu, RefPicList eRefPicList, int refIdxPred, IntTZSearchStruct &cStruct,
                          Mv &rcMv, Distortion &ruiSAD, const Mv *const pIntegerMv2Nx2NPred);

  void xPatternSearch             ( IntTZSearchStruct&    cStruct,
                                    Mv&                   rcMv,
                                    Distortion&           ruiSAD
                                  );

  void xPatternSearchIntRefine(PredictionUnit &pu, IntTZSearchStruct &cStruct, Mv &rcMv, Mv &rcMvPred, int &riMVPIdx,
                               uint32_t &ruiBits, Distortion &ruiCost, const AMVPInfo &amvpInfo, double fWeight
#if GDR_ENABLED
                               ,
                               RefPicList eRefPicList, int refIdxPred, bool &rbCleanCandExist
#endif
  );

  void xPatternSearchFracDIF(const PredictionUnit &pu, RefPicList eRefPicList, int refIdx, IntTZSearchStruct &cStruct,
                             const Mv &rcMvInt, Mv &rcMvHalf, Mv &rcMvQter, Distortion &ruiCost
#if GDR_ENABLED
                             ,
                             bool &rbCleanCandExist
#endif
  );

  void xPredAffineInterSearch(PredictionUnit &pu, PelUnitBuf &origBuf, int puIdx, uint32_t &lastMode,
                              Distortion &affineCost, RefSetArray<Mv> &hevcMv,
#if GDR_ENABLED
                              RefSetArray<bool> &hevcMvSolid,
#endif
                              RefSetArray<Mv[3]> &mvAffine4Para,
#if GDR_ENABLED
                              RefSetArray<bool[3]> &mvAffine4ParaSolid,
#endif
                              int refIdx4Para[NUM_REF_PIC_LIST_01], uint8_t bcwIdx = BCW_DEFAULT,
                              bool enforceBcwPred = false, uint32_t bcwIdxBits = 0);

#if GDR_ENABLED
  void xAffineMotionEstimation(PredictionUnit &pu, PelUnitBuf &origBuf, RefPicList eRefPicList, Mv acMvPred[3],
                               int refIdxPred, Mv acMv[3], bool acMvSolid[3], uint32_t &ruiBits, Distortion &ruiCost,
                               int &mvpIdx, const AffineAMVPInfo &aamvpi, bool &rbCleanCandExist, bool bBi = false);
#else
  void xAffineMotionEstimation(PredictionUnit &pu, PelUnitBuf &origBuf, RefPicList eRefPicList, Mv acMvPred[3],
                               int refIdxPred, Mv acMv[3], uint32_t &ruiBits, Distortion &ruiCost, int &mvpIdx,
                               const AffineAMVPInfo &aamvpi, bool bBi = false);
#endif

  void xEstimateAffineAMVP(PredictionUnit &pu, AffineAMVPInfo &affineAMVPInfo, PelUnitBuf &origBuf,
                           RefPicList eRefPicList, int refIdx, Mv acMvPred[3], Distortion *puiDistBiP);

#if GDR_ENABLED
  Distortion xGetAffineTemplateCost(PredictionUnit &pu, PelUnitBuf &origBuf, PelUnitBuf &predBuf, Mv acMvCand[3],
                                    int mvpIdx, int mvpNum, RefPicList eRefPicList, int refIdx, bool &rbOk);
#else
  Distortion xGetAffineTemplateCost(PredictionUnit &pu, PelUnitBuf &origBuf, PelUnitBuf &predBuf, Mv acMvCand[3],
                                    int mvpIdx, int mvpNum, RefPicList eRefPicList, int refIdx);
#endif

  void xCopyAffineAMVPInfo        ( AffineAMVPInfo& src, AffineAMVPInfo& dst );
  void xCheckBestAffineMVP        ( PredictionUnit &pu, AffineAMVPInfo &affineAMVPInfo, RefPicList eRefPicList, Mv acMv[3], Mv acMvPred[3], int& riMVPIdx, uint32_t& ruiBits, Distortion& ruiCost );

  Distortion xGetSymmetricCost( PredictionUnit& pu, PelUnitBuf& origBuf, RefPicList eCurRefPicList, const MvField& cCurMvField, MvField& cTarMvField , int bcwIdx );

#if GDR_ENABLED
  Distortion xSymmeticRefineMvSearch(PredictionUnit &pu, PelUnitBuf &origBuf, Mv &rcMvCurPred, Mv &rcMvTarPred,
                                     RefPicList eRefPicList, MvField &rCurMvField, MvField &rTarMvField,
                                     Distortion uiMinCost, int searchPattern, int nSearchStepShift,
                                     uint32_t uiMaxSearchRounds, int bcwIdx, bool &rbOk);
#else
  Distortion xSymmeticRefineMvSearch( PredictionUnit& pu, PelUnitBuf& origBuf, Mv& rcMvCurPred, Mv& rcMvTarPred
    , RefPicList eRefPicList, MvField& rCurMvField, MvField& rTarMvField, Distortion uiMinCost, int searchPattern, int nSearchStepShift, uint32_t uiMaxSearchRounds , int bcwIdx );
#endif

#if GDR_ENABLED
  bool xSymmetricMotionEstimation( PredictionUnit& pu, PelUnitBuf& origBuf, Mv& rcMvCurPred, Mv& rcMvTarPred,
  RefPicList eRefPicList, MvField& rCurMvField, MvField& rTarMvField, Distortion& ruiCost, int bcwIdx, bool& ruiCostOk );
#else
  void xSymmetricMotionEstimation( PredictionUnit& pu, PelUnitBuf& origBuf, Mv& rcMvCurPred, Mv& rcMvTarPred, RefPicList eRefPicList, MvField& rCurMvField, MvField& rTarMvField, Distortion& ruiCost, int bcwIdx );
#endif

#if GDR_ENABLED
  bool xReadBufferedAffineUniMv(PredictionUnit &pu, RefPicList eRefPicList, int32_t refIdx, Mv acMvPred[3], Mv acMv[3],
                                bool acMvSolid[3], uint32_t &ruiBits, Distortion &ruiCost, int &mvpIdx,
                                const AffineAMVPInfo &aamvpi);
#else
  bool xReadBufferedAffineUniMv(PredictionUnit &pu, RefPicList eRefPicList, int32_t refIdx, Mv acMvPred[3], Mv acMv[3],
                                uint32_t &ruiBits, Distortion &ruiCost, int &mvpIdx, const AffineAMVPInfo &aamvpi);
#endif
  double xGetMEDistortionWeight   ( uint8_t bcwIdx, RefPicList eRefPicList);
#if GDR_ENABLED
  bool xReadBufferedUniMv(PredictionUnit &pu, RefPicList eRefPicList, int32_t refIdx, Mv &pcMvPred, Mv &rcMv,
                          bool &rcMvSolid, uint32_t &ruiBits, Distortion &ruiCost);
#else
  bool xReadBufferedUniMv(PredictionUnit &pu, RefPicList eRefPicList, int32_t refIdx, Mv &pcMvPred, Mv &rcMv,
                          uint32_t &ruiBits, Distortion &ruiCost);
#endif

  void xClipMv                    ( Mv& rcMv, const struct Position& pos, const struct Size& size, const class SPS& sps, const class PPS& pps );

public:
  void resetBufferedUniMotions    () { m_uniMotions.reset(); }
  uint32_t getWeightIdxBits       ( uint8_t bcwIdx ) { return m_estWeightIdxBits[bcwIdx]; }
  void initWeightIdxBits          ();
  void     symmvdCheckBestMvp(PredictionUnit &pu, PelUnitBuf &origBuf, Mv curMv, RefPicList curRefList,
                              RefSetArray<AMVPInfo> &amvpInfo, int32_t bcwIdx, Mv cMvPredSym[NUM_REF_PIC_LIST_01],
#if GDR_ENABLED
                          bool cMvPredSymSolid[NUM_REF_PIC_LIST_01],
#endif
                          int32_t mvpIdxSym[NUM_REF_PIC_LIST_01], Distortion &bestCost, bool skip = false);
protected:

  void xExtDIFUpSamplingH(CPelBuf* pcPattern, bool useAltHpelIf);
  void xExtDIFUpSamplingQ         ( CPelBuf* pcPatternKey, Mv halfPelRef );
  uint32_t xDetermineBestMvp      ( PredictionUnit& pu, Mv acMvTemp[3], int& mvpIdx, const AffineAMVPInfo& aamvpi );
  // -------------------------------------------------------------------------------------------------------------------
  // compute symbol bits
  // -------------------------------------------------------------------------------------------------------------------

  void setWpScalingDistParam(int refIdx, RefPicList eRefPicListCur, Slice *slice);

private:
  void  xxIBCHashSearch(PredictionUnit& pu, Mv* mvPred, int numMvPred, Mv &mv, int& idxMvPred, IbcHashMap& ibcHashMap);
public:

  void encodeResAndCalcRdInterCU  (CodingStructure &cs, Partitioner &partitioner, const bool &skipResidual
    , const bool luma = true, const bool chroma = true
  );
  void xEncodeInterResidualQT     (CodingStructure &cs, Partitioner &partitioner, const ComponentID &compID);
  void     xEstimateInterResidualQT(CodingStructure &cs, Partitioner &partitioner, Distortion *puiZeroDist = nullptr,
                                    const bool luma = true, const bool chroma = true, PelUnitBuf *orgResi = nullptr);
  uint64_t xGetSymbolFracBitsInter  (CodingStructure &cs, Partitioner &partitioner);
  uint64_t xCalcPuMeBits            (PredictionUnit& pu);

};// END CLASS DEFINITION EncSearch

//! \}

#endif // __ENCSEARCH__
