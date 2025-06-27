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

/** \file     IntraSearch.h
    \brief    intra search class (header)
*/

#ifndef __INTRASEARCH__
#define __INTRASEARCH__

// Include files

#include "CABACWriter.h"
#include "EncCfg.h"

#include "CommonLib/IntraPrediction.h"
#include "CommonLib/TrQuant.h"
#include "CommonLib/Unit.h"
#include "CommonLib/RdCost.h"
#include "EncReshape.h"

//! \ingroup EncoderLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================
class EncModeCtrl;

enum PLTScanMode
{
  PLT_SCAN_HORTRAV = 0,
  PLT_SCAN_VERTRAV = 1,
  NUM_PLT_SCAN = 2
};
class SortingElement
{
public:
  SortingElement() {
    cnt[0] = cnt[1] = cnt[2] = cnt[3] = 0;
    shift[0] = shift[1] = shift[2] = 0;
    lastCnt[0] = lastCnt[1] = lastCnt[2] = 0;
    data[0] = data[1] = data[2] = 0;
    sumData[0] = sumData[1] = sumData[2] = 0;
  }
  uint32_t  getCnt(int idx) const         { return cnt[idx]; }
  void      setCnt(uint32_t val, int idx) { cnt[idx] = val; }
  int       getSumData (int id) const   { return sumData[id]; }

  void resetAll(ComponentID compBegin, uint32_t numComp)
  {
    shift[0] = shift[1] = shift[2] = 0;
    lastCnt[0] = lastCnt[1] = lastCnt[2] = 0;
    for (int ch = compBegin; ch < (compBegin + numComp); ch++)
    {
      data[ch] = 0;
      sumData[ch] = 0;
    }
  }
  void setAll(uint32_t* ui, ComponentID compBegin, uint32_t numComp)
  {
    for (int ch = compBegin; ch < (compBegin + numComp); ch++)
    {
      data[ch] = ui[ch];
    }
  }
  bool almostEqualData(SortingElement element, int errorLimit, const BitDepths& bitDepths, ComponentID compBegin, uint32_t numComp, bool lossless)
  {
    bool almostEqual = true;
    for (int comp = compBegin; comp < (compBegin + numComp); comp++)
    {
      if (lossless)
      {
        if ((std::abs(data[comp] - element.data[comp])) > errorLimit)
        {
          almostEqual = false;
          break;
        }
      }
      else
      {
      uint32_t absError = 0;
      if (isChroma((ComponentID) comp))
      {
        absError += int(double(std::abs(data[comp] - element.data[comp])) * PLT_CHROMA_WEIGHTING)
                    >> (bitDepths[ChannelType::CHROMA] - PLT_ENCBITDEPTH);
      }
      else
      {
        absError += (std::abs(data[comp] - element.data[comp])) >> (bitDepths[ChannelType::LUMA] - PLT_ENCBITDEPTH);
      }
      if (absError > errorLimit)
      {
        almostEqual = false;
        break;
      }
      }
    }
    return almostEqual;
  }
  uint32_t getSAD(SortingElement element, const BitDepths &bitDepths, ComponentID compBegin, uint32_t numComp, bool lossless)
  {
    uint32_t sumAd = 0;
    for (int comp = compBegin; comp < (compBegin + numComp); comp++)
    {
      ChannelType chType = (comp > 0) ? ChannelType::CHROMA : ChannelType::LUMA;
      if (lossless)
      {
        sumAd += (std::abs(data[comp] - element.data[comp]));
      }
      else
      {
        sumAd += (std::abs(data[comp] - element.data[comp]) >> (bitDepths[chType] - PLT_ENCBITDEPTH));
      }
    }
    return sumAd;
  }
  void copyDataFrom(SortingElement element, ComponentID compBegin, uint32_t numComp)
  {
    for (int comp = compBegin; comp < (compBegin + numComp); comp++)
    {
      data[comp] = element.data[comp];
      sumData[comp] = data[comp];
      shift[comp] = 0;
      lastCnt[comp] = 1;
    }
  }
  void copyAllFrom(SortingElement element, ComponentID compBegin, uint32_t numComp)
  {
    copyDataFrom(element, compBegin, numComp);
    for (int comp = compBegin; comp < (compBegin + numComp); comp++)
    {
      sumData[comp] = element.sumData[comp];
      cnt[comp]     = element.cnt[comp];
      shift[comp]   = element.shift[comp];
      lastCnt[comp] = element.lastCnt[comp];
    }
    cnt[MAX_NUM_COMPONENT] = element.cnt[MAX_NUM_COMPONENT];
  }
  void addElement(const SortingElement& element, ComponentID compBegin, uint32_t numComp)
  {
    for (int i = compBegin; i<(compBegin + numComp); i++)
    {
      sumData[i] += element.data[i];
      cnt[i]++;
      if( cnt[i] > 1 && cnt[i] == 2 * lastCnt[i] )
      {
        uint32_t rnd = 1 << shift[i];
        shift[i]++;
        data[i] = (sumData[i] + rnd) >> shift[i];
        lastCnt[i] = cnt[i];
      }
    }
  }
private:
  uint32_t cnt[MAX_NUM_COMPONENT+1];
  int shift[3], lastCnt[3], data[3], sumData[3];
};
/// encoder search class
class IntraSearch : public IntraPrediction
{
private:
  EncModeCtrl    *m_modeCtrl;
  Pel*            m_pSharedPredTransformSkip[MAX_NUM_TBLOCKS];

  XuPool m_unitPool;

  CodingStructure ****m_pSplitCS;
  CodingStructure ****m_pFullCS;

  CodingStructure ***m_pTempCS;
  CodingStructure ***m_pBestCS;

  CodingStructure **m_pSaveCS;

  bool            m_saveCuCostInSCIPU;
  uint8_t         m_numCuInSCIPU;
  Area            m_cuAreaInSCIPU[NUM_INTER_CU_INFO_SAVE];
  double          m_cuCostInSCIPU[NUM_INTER_CU_INFO_SAVE];

  struct ModeInfo
  {
    bool     mipFlg; // CU::mipFlag
    bool     mipTrFlg; // PU::mipTransposedFlag
    uint8_t  mRefId;   // PU::multiRefIdx
    ISPType  ispMod;   // CU::ispMode
    uint32_t modeId;   // PU::intraDir[ChannelType::LUMA]

    ModeInfo() : mipFlg(false), mipTrFlg(false), mRefId(0), ispMod(ISPType::NONE), modeId(0) {}
    ModeInfo(const bool mipf, const bool miptf, const int mrid, const ISPType ispm, const uint32_t mode)
      : mipFlg(mipf), mipTrFlg(miptf), mRefId(mrid), ispMod(ispm), modeId(mode)
    {
    }
    bool operator==(const ModeInfo cmp) const { return (mipFlg == cmp.mipFlg && mipTrFlg == cmp.mipTrFlg && mRefId == cmp.mRefId && ispMod == cmp.ispMod && modeId == cmp.modeId); }
  };

  struct ModeInfoWithCost : public ModeInfo
  {
    double rdCost;
    ModeInfoWithCost() : ModeInfo(), rdCost(MAX_DOUBLE) {}
    ModeInfoWithCost(const bool mipf, const bool miptf, const int mrid, const ISPType ispm, const uint32_t mode,
                     double cost)
      : ModeInfo(mipf, miptf, mrid, ispm, mode), rdCost(cost)
    {
    }
    bool operator==(const ModeInfoWithCost cmp) const { return (mipFlg == cmp.mipFlg && mipTrFlg == cmp.mipTrFlg && mRefId == cmp.mRefId && ispMod == cmp.ispMod && modeId == cmp.modeId && rdCost == cmp.rdCost); }
    static bool compare(const ModeInfoWithCost &a, const ModeInfoWithCost &b) { return a.rdCost < b.rdCost; }
  };

  struct ISPTestedModeInfo
  {
    int    numCompSubParts;
    double rdCost;

    ISPTestedModeInfo() {}

    void setMode(int numParts, double cost)
    {
      numCompSubParts = numParts;
      rdCost = cost;
    }

    void clear()
    {
      numCompSubParts = -1;
      rdCost = MAX_DOUBLE;
    }
  };

  struct ISPTestedModesInfo
  {
    EnumArray<ISPTestedModeInfo, ISPType> intraMode[NUM_LUMA_MODE];

    EnumArray<static_vector<int, FAST_UDI_MAX_RDMODE_NUM>, ISPType> testedModes;

    EnumArray<bool, ISPType>   modeHasBeenTested[NUM_LUMA_MODE];
    EnumArray<bool, ISPType>   splitIsFinished;
    EnumArray<int, ISPType>    numTotalParts;
    EnumArray<int, ISPType>    bestMode;
    EnumArray<int, ISPType>    numTestedModes;
    EnumArray<int, ISPType>    candIndexInList;
    EnumArray<double, ISPType> bestCost;

    int     bestModeSoFar;
    ISPType bestSplitSoFar;
    int     numOrigModesToTest;

    // set a tested mode results
    void setModeResults(const ISPType splitType, const int modeIdx, int numCompletedParts, double rdCost,
                        double currentBestCost)
    {
      const int maxNumParts = numTotalParts[splitType];
      intraMode[modeIdx][splitType].setMode(numCompletedParts, numCompletedParts == maxNumParts ? rdCost : MAX_DOUBLE);
      testedModes[splitType].push_back(modeIdx);
      numTestedModes[splitType]++;
      modeHasBeenTested[modeIdx][splitType] = true;
      if (numCompletedParts == maxNumParts && rdCost < bestCost[splitType])   // best mode update
      {
        bestMode[splitType] = modeIdx;
        bestCost[splitType] = rdCost;
      }
      if (numCompletedParts == maxNumParts && rdCost < currentBestCost)   // best mode update
      {
        bestModeSoFar  = modeIdx;
        bestSplitSoFar = splitType;
      }
    }

    int getNumCompletedSubParts(const ISPType splitType, const int modeIdx)
    {
      CHECKD(splitType != ISPType::HOR && splitType != ISPType::VER, "The split type is invalid!");
      CHECKD(modeIdx < 0 || modeIdx > (NUM_LUMA_MODE - 1), "The modeIdx is invalid");
      return modeHasBeenTested[modeIdx][splitType] ? intraMode[modeIdx][splitType].numCompSubParts : -1;
    }

    double getRDCost(const ISPType splitType, const int modeIdx)
    {
      CHECKD(splitType != ISPType::HOR && splitType != ISPType::VER, "The split type is invalid!");
      return modeHasBeenTested[modeIdx][splitType] ? intraMode[modeIdx][splitType].rdCost : MAX_DOUBLE;
    }

    // get a tested intra mode index
    int getTestedIntraMode(const ISPType splitType, const int pos)
    {
      CHECKD(splitType != ISPType::HOR && splitType != ISPType::VER, "The split type is invalid!");
      return pos < testedModes[splitType].size() ? testedModes[splitType].at(pos) : NOMODE_IDX;
    }

    // set everything to default values
    void clear()
    {
      for (const auto splitIdx: { ISPType::HOR, ISPType::VER })
      {
        numTestedModes [splitIdx] = 0;
        candIndexInList[splitIdx] = 0;
        numTotalParts  [splitIdx] = 0;
        splitIsFinished[splitIdx] = false;
        testedModes    [splitIdx].clear();
        bestCost       [splitIdx] = MAX_DOUBLE;
        bestMode[splitIdx]        = NOMODE_IDX;
      }
      bestModeSoFar      = NOMODE_IDX;
      bestSplitSoFar     = ISPType::NONE;
      numOrigModesToTest = -1;
      for (int i = 0; i < NUM_LUMA_MODE; i++)
      {
        modeHasBeenTested[i].fill(false);
      }
    }

    void clearISPModeInfo(int idx)
    {
      intraMode[idx][ISPType::HOR].clear();
      intraMode[idx][ISPType::VER].clear();
    }

    void init(const int numTotalPartsHor, const int numTotalPartsVer)
    {
      clear();
      numTotalParts[ISPType::HOR]   = numTotalPartsHor;
      numTotalParts[ISPType::VER]   = numTotalPartsVer;
      splitIsFinished[ISPType::HOR] = (numTotalParts[ISPType::HOR] == 0);
      splitIsFinished[ISPType::VER] = (numTotalParts[ISPType::VER] == 0);
    }
  };

  EnumArray<static_vector<ModeInfo, FAST_UDI_MAX_RDMODE_NUM>, ISPType> m_ispCandList;
  static_vector<ModeInfoWithCost, FAST_UDI_MAX_RDMODE_NUM> m_regIntraRDListWithCosts;

  ISPTestedModesInfo m_ispTestedModes[NUM_LFNST_NUM_PER_SET];
  int m_curIspLfnstIdx;

  //cost variables for the EMT algorithm and new modes list
  double     m_bestModeCostStore[ NUM_LFNST_NUM_PER_SET ];                                    // RD cost of the best mode for each PU using DCT2
  bool       m_bestModeCostValid[ NUM_LFNST_NUM_PER_SET ];
  double     m_modeCostStore[ NUM_LFNST_NUM_PER_SET ][ NUM_LUMA_MODE ];                   // RD cost of each mode for each PU using DCT2
  ModeInfo   m_savedRdModeList[ NUM_LFNST_NUM_PER_SET ][ NUM_LUMA_MODE ];
  int32_t    m_savedNumRdModes[ NUM_LFNST_NUM_PER_SET ];

  ModeInfo  m_savedRdModeFirstColorSpace[4 * NUM_LFNST_NUM_PER_SET * 2][FAST_UDI_MAX_RDMODE_NUM];
  BdpcmMode m_savedBDPCMModeFirstColorSpace[4 * NUM_LFNST_NUM_PER_SET * 2][FAST_UDI_MAX_RDMODE_NUM];
  double    m_savedRdCostFirstColorSpace[4 * NUM_LFNST_NUM_PER_SET * 2][FAST_UDI_MAX_RDMODE_NUM];
  int       m_numSavedRdModeFirstColorSpace[4 * NUM_LFNST_NUM_PER_SET * 2];
  int       m_savedRdModeIdx;

  static_vector<ModeInfo, FAST_UDI_MAX_RDMODE_NUM> m_savedRdModeListLFNST;
  static_vector<ModeInfo, FAST_UDI_MAX_RDMODE_NUM> m_savedHadModeListLFNST;
  uint32_t                                         m_savedNumRdModesLFNST;
  static_vector<double, FAST_UDI_MAX_RDMODE_NUM>   m_savedModeCostLFNST;
  static_vector<double, FAST_UDI_MAX_RDMODE_NUM>   m_savedHadListLFNST;

  PelStorage      m_tmpStorageCtu;
  PelStorage      m_colorTransResiBuf;

  std::vector<TransformUnit *> m_orgTUs;

protected:
  // interface to option
  EncCfg*         m_pcEncCfg;

  // interface to classes
  TrQuant*        m_pcTrQuant;
  RdCost*         m_pcRdCost;
  EncReshape*     m_pcReshape;

  // RD computation
  CABACWriter*    m_CABACEstimator;
  CtxPool        *m_ctxPool;

  bool            m_isInitialized;
  bool            m_bestEscape;
  double*         m_indexError[MAXPLTSIZE + 1];
  uint8_t*        m_minErrorIndexMap; // store the best index in terms of distortion for each pixel
  uint8_t         m_indexMapRDOQ   [2][NUM_TRELLIS_STATE][2 * MAX_CU_BLKSIZE_PLT];
  PLTRunMode      m_runMapRDOQ[2][NUM_TRELLIS_STATE][2 * MAX_CU_BLKSIZE_PLT];
  uint8_t*        m_statePtRDOQ    [NUM_TRELLIS_STATE];
  PLTRunMode      m_prevRunTypeRDOQ[2][NUM_TRELLIS_STATE];
  int             m_prevRunPosRDOQ [2][NUM_TRELLIS_STATE];
  double          m_stateCostRDOQ  [2][NUM_TRELLIS_STATE];
public:

  IntraSearch();
  ~IntraSearch();

  void init(EncCfg *pcEncCfg, TrQuant *pcTrQuant, RdCost *pcRdCost, CABACWriter *CABACEstimator, CtxPool *ctxPool,
            const uint32_t maxCUWidth, const uint32_t maxCUHeight, const uint32_t maxTotalCUDepth,
            EncReshape *m_pcReshape, const unsigned bitDepthY);

  void destroy                    ();

  CodingStructure****getSplitCSBuf() { return m_pSplitCS; }
  CodingStructure****getFullCSBuf () { return m_pFullCS; }
  CodingStructure  **getSaveCSBuf () { return m_pSaveCS; }

  void setModeCtrl                ( EncModeCtrl *modeCtrl ) { m_modeCtrl = modeCtrl; }

  bool getSaveCuCostInSCIPU       ()               { return m_saveCuCostInSCIPU; }
  void setSaveCuCostInSCIPU       ( bool b )       { m_saveCuCostInSCIPU = b;  }
  void setNumCuInSCIPU            ( uint8_t i )    { m_numCuInSCIPU = i; }
  void saveCuAreaCostInSCIPU      ( Area area, double cost );
  void initCuAreaCostInSCIPU      ();
  double findInterCUCost          ( CodingUnit &cu );

public:
  bool     estIntraPredLumaQT(CodingUnit &cu, Partitioner &pm, const double bestCostSoFar = MAX_DOUBLE,
                              bool mtsCheckRangeFlag = false, int mtsFirstCheckId = 0, int mtsLastCheckId = 0,
                              bool moreProbMTSIdxFirst = false, CodingStructure *bestCS = nullptr);
  void estIntraPredChromaQT       ( CodingUnit &cu, Partitioner& pm, const double maxCostAllowed = MAX_DOUBLE );
  void PLTSearch                  ( CodingStructure &cs, Partitioner& partitioner, ComponentID compBegin, uint32_t numComp);
  uint64_t xFracModeBitsIntra(PredictionUnit &pu, const uint32_t &mode, const ChannelType &compID);
  void invalidateBestModeCost     () { for( int i = 0; i < NUM_LFNST_NUM_PER_SET; i++ ) m_bestModeCostValid[ i ] = false; };

  void sortRdModeListFirstColorSpace(ModeInfo mode, double cost, BdpcmMode bdpcmMode, ModeInfo *rdModeList,
                                     double *rdCostList, BdpcmMode *bdpcmModeList, int &candNum);
  void invalidateBestRdModeFirstColorSpace();
  void setSavedRdModeIdx(int idx) { m_savedRdModeIdx = idx; }

#if GDR_ENABLED
  int  getNumTopRecons(PredictionUnit &pu, int lumaDirMode, bool isChroma);
  bool isValidIntraPredLuma(PredictionUnit &pu, int lumaDirMode);
  bool isValidIntraPredChroma(PredictionUnit &pu, int lumaDirMode, int chromaDirMode);
#endif
protected:

  // -------------------------------------------------------------------------------------------------------------------
  // T & Q & Q-1 & T-1
  // -------------------------------------------------------------------------------------------------------------------


  // -------------------------------------------------------------------------------------------------------------------
  // Intra search
  // -------------------------------------------------------------------------------------------------------------------

  void     xEncIntraHeader                         ( CodingStructure &cs, Partitioner& pm, const bool &luma, const bool &chroma, const int subTuIdx = -1 );
  void     xEncSubdivCbfQT                         ( CodingStructure &cs, Partitioner& pm, const bool &luma, const bool &chroma, const int subTuIdx = -1, const PartSplit ispType = TU_NO_ISP );
  uint64_t xGetIntraFracBitsQT                     ( CodingStructure &cs, Partitioner& pm, const bool &luma, const bool &chroma, const int subTuIdx = -1, const PartSplit ispType = TU_NO_ISP, CUCtx * cuCtx = nullptr  );
  uint64_t xGetIntraFracBitsQTSingleChromaComponent( CodingStructure &cs, Partitioner& pm, const ComponentID compID );

  uint64_t xGetIntraFracBitsQTChroma(TransformUnit& tu, const ComponentID &compID);
  void xEncCoeffQT                                 ( CodingStructure &cs, Partitioner& pm, const ComponentID compID, const int subTuIdx = -1, const PartSplit ispType = TU_NO_ISP, CUCtx * cuCtx = nullptr );

  void xIntraCodingTUBlock(TransformUnit &tu, const ComponentID &compID, Distortion &dist,
                           const int &default0Save1Load2 = 0, uint32_t *numSig = nullptr, TrModeList *trModes = nullptr,
                           const bool loadTr = false);
  void xIntraCodingACTTUBlock(TransformUnit &tu, const ComponentID &compID, Distortion &dist,
                              TrModeList *trModes = nullptr, const bool loadTr = false);

  ChromaCbfs xRecurIntraChromaCodingQT( CodingStructure &cs, Partitioner& pm, const double bestCostSoFar = MAX_DOUBLE,                          const PartSplit ispType = TU_NO_ISP );
  bool       xRecurIntraCodingLumaQT  ( CodingStructure &cs, Partitioner& pm, bool mtsCheckRangeFlag = false, int mtsFirstCheckId = 0, int mtsLastCheckId = 0, bool moreProbMTSIdxFirst = false );
  bool       xRecurIntraCodingACTQT(CodingStructure &cs, Partitioner& pm, bool mtsCheckRangeFlag = false, int mtsFirstCheckId = 0, int mtsLastCheckId = 0, bool moreProbMTSIdxFirst = false);
  bool       xIntraCodingLumaISP      ( CodingStructure& cs, Partitioner& pm, const double bestCostSoFar = MAX_DOUBLE );

  template<typename T, size_t N>
  void reduceHadCandList(static_vector<T, N>& candModeList, static_vector<double, N>& candCostList, int& numModesForFullRD, const double thresholdHadCost, const double* mipHadCost, const PredictionUnit &pu, const bool fastMip);
  void   derivePLTLossy  (      CodingStructure& cs, Partitioner& partitioner, ComponentID compBegin, uint32_t numComp);
  void   calcPixelPred   (      CodingStructure& cs, Partitioner& partitioner, uint32_t    yPos,      uint32_t xPos,             ComponentID compBegin, uint32_t  numComp);
  void     preCalcPLTIndexRD      (CodingStructure& cs, Partitioner& partitioner, ComponentID compBegin, uint32_t numComp);
  void     calcPixelPredRD        (CodingStructure& cs, Partitioner& partitioner, Pel* orgBuf, Pel* pixelValue, Pel* recoValue, ComponentID compBegin, uint32_t numComp);
  void     deriveIndexMap(CodingStructure &cs, Partitioner &partitioner, ComponentID compBegin, uint32_t numComp,
                          PLTScanMode pltScanMode, double &cost, bool *idxExist);
  bool     deriveSubblockIndexMap(CodingStructure& cs, Partitioner& partitioner, ComponentID compBegin, PLTScanMode pltScanMode, int minSubPos, int maxSubPos, const BinFracBits& fracBitsPltRunType, const BinFracBits* fracBitsPltIndexINDEX, const BinFracBits* fracBitsPltIndexCOPY, const double minCost, bool useRotate);
  double   rateDistOptPLT(PLTRunMode RunType, uint8_t RunIndex, PLTRunMode prevRunType, uint8_t prevRunIndex,
                          uint8_t aboveRunIndex, PLTRunMode& prevCodedRunType, int& prevCodedRunPos, int scanPos,
                          uint32_t width, int dist, int indexMaxValue, const BinFracBits* IndexfracBits,
                          const BinFracBits& TypefracBits);
  uint32_t getTruncBinBits(uint32_t symbol, uint32_t numSymbols);
  uint32_t getEpExGolombNumBins   (uint32_t symbol, uint32_t count);

  void xGetNextISPMode                    ( ModeInfo& modeInfo, const ModeInfo* lastMode, const Size cuSize );
  bool xSortISPCandList(double bestCostSoFar, double bestNonISPCost, const ModeInfo &bestNonISPMode);
  void xSortISPCandListLFNST();
  void xFindAlreadyTestedNearbyIntraModes(int currentIntraMode, int &refLfnstIdx, std::array<int, 2> &similarModes,
                                          ISPType ispOption, int windowSize);
  bool updateISPStatusFromRelCU(double bestNonISPCostCurrCu, const ModeInfo &bestNonISPModeCurrCu,
                                int &bestISPModeInRelCU);
  void xFinishISPModes                    ( );
};// END CLASS DEFINITION EncSearch

//! \}

#endif // __ENCSEARCH__
