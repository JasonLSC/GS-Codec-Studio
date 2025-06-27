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

/** \file     UnitTool.h
 *  \brief    defines operations for basic units
 */

#ifndef __UNITTOOLS__
#define __UNITTOOLS__

#include "Unit.h"
#include "UnitPartitioner.h"
#include "ContextModelling.h"
#include "InterPrediction.h"

// CS tools
namespace CS
{
  UnitArea getArea                    ( const CodingStructure &cs, const UnitArea &area, const ChannelType chType );
  bool   isDualITree                  ( const CodingStructure &cs );
  void   setRefinedMotionField(CodingStructure &cs);
}   // namespace CS

// CU tools
namespace CU
{
  static inline bool isIntra(const CodingUnit &cu)
  {
    return cu.predMode == MODE_INTRA;
  }

  static inline bool isInter(const CodingUnit &cu)
  {
    return cu.predMode == MODE_INTER;
  }

  static inline bool isIBC(const CodingUnit &cu)
  {
    return cu.predMode == MODE_IBC;
  }

  static inline bool isPLT(const CodingUnit &cu)
  {
    return cu.predMode == MODE_PLT;
  }

  bool isSameCtu                      (const CodingUnit &cu, const CodingUnit &cu2);
  bool isSameSlice                    (const CodingUnit &cu, const CodingUnit &cu2);
  bool isSameTile                     (const CodingUnit &cu, const CodingUnit &cu2);
  bool isSameSliceAndTile             (const CodingUnit &cu, const CodingUnit &cu2);
  bool isSameSubPic                   (const CodingUnit &cu, const CodingUnit &cu2);
  bool isLastSubCUOfCtu               (const CodingUnit &cu);
  uint32_t getCtuAddr                     (const CodingUnit &cu);
  int  predictQP                      (const CodingUnit& cu, const int prevQP );

  uint32_t getNumPUs                      (const CodingUnit& cu);
  void addPUs                         (      CodingUnit& cu);

  void saveMotionForHmvp(const CodingUnit &cu);

  PartSplit getSplitAtDepth           (const CodingUnit& cu, const unsigned depth);
  ModeType  getModeTypeAtDepth        (const CodingUnit& cu, const unsigned depth);

  uint32_t getNumNonZeroCoeffNonTsCorner8x8( const CodingUnit& cu, const bool lumaFlag = true, const bool chromaFlag = true );
  bool  isPredRegDiffFromTB(const CodingUnit& cu, const ComponentID compID);
  bool  isFirstTBInPredReg(const CodingUnit& cu, const ComponentID compID, const CompArea &area);
  bool  isMinWidthPredEnabledForBlkSize(const int w, const int h);
  void  adjustPredArea(CompArea &area);
  bool  isBcwIdxCoded                 (const CodingUnit& cu);
  uint8_t  getValidBcwIdx(const CodingUnit &cu);
  bool bdpcmAllowed                   (const CodingUnit& cu, const ComponentID compID);
  bool isMTSAllowed                   (const CodingUnit& cu, const ComponentID compID);


  bool      divideTuInRows            ( const CodingUnit &cu );
  PartSplit getISPType                ( const CodingUnit &cu,                         const ComponentID compID );
  bool      isISPLast                 ( const CodingUnit &cu, const CompArea &tuArea, const ComponentID compID );
  bool      isISPFirst                ( const CodingUnit &cu, const CompArea &tuArea, const ComponentID compID );
  bool      canUseISP                 ( const CodingUnit &cu,                         const ComponentID compID );
  bool      canUseISP                 ( const int width, const int height, const int maxTrSize = MAX_TB_SIZEY );
  bool      canUseLfnstWithISP(const CompArea &cuArea, ISPType ispSplitType);
  bool      canUseLfnstWithISP        ( const CodingUnit& cu, const ChannelType chType );
  uint32_t  getISPSplitDim            ( const int width, const int height, const PartSplit ispType );
  bool      allLumaCBFsAreZero        ( const CodingUnit& cu );

  inline bool canUseIbc(const UnitArea& a) { return a.lwidth() <= IBC_MAX_CU_SIZE && a.lheight() <= IBC_MAX_CU_SIZE; }

  PUTraverser traversePUs             (      CodingUnit& cu);
  TUTraverser traverseTUs             (      CodingUnit& cu);
  cPUTraverser traversePUs            (const CodingUnit& cu);
  cTUTraverser traverseTUs            (const CodingUnit& cu);

  bool  hasSubCUNonZeroMVd            (const CodingUnit& cu);
  bool  hasSubCUNonZeroAffineMVd      ( const CodingUnit& cu );

  uint8_t getSbtInfo                  (uint8_t idx, uint8_t pos);
  uint8_t getSbtIdx                   (const uint8_t sbtInfo);
  uint8_t getSbtPos                   (const uint8_t sbtInfo);
  uint8_t getSbtMode                  (const uint8_t sbtIdx, const uint8_t sbtPos);
  uint8_t getSbtIdxFromSbtMode        (const uint8_t sbtMode);
  uint8_t getSbtPosFromSbtMode        (const uint8_t sbtMode);
  uint8_t targetSbtAllowed            (uint8_t idx, uint8_t sbtAllowed);
  uint8_t numSbtModeRdo               (uint8_t sbtAllowed);
  bool    isSbtMode                   (const uint8_t sbtInfo);
  bool    isSameSbtSize               (const uint8_t sbtInfo1, const uint8_t sbtInfo2);
  bool    getRprScaling(const SPS *sps, const PPS *curPPS, Picture *refPic, ScalingRatio &scalingRatio);
  void    checkConformanceILRP        (Slice *slice);
}
// PU tools
namespace PU
{
  int      getLMSymbolList(const PredictionUnit &pu, int *modeList);
  int      getIntraMPMs(const PredictionUnit &pu, unsigned *mpm);
  bool     isMIP(const PredictionUnit &pu, const ChannelType chType = ChannelType::LUMA);
  bool     isDMChromaMIP(const PredictionUnit &pu);
  uint32_t getIntraDirLuma(const PredictionUnit &pu);
  void     getIntraChromaCandModes(const PredictionUnit &pu, unsigned modeList[NUM_CHROMA_MODE]);
  uint32_t getFinalIntraMode(const PredictionUnit &pu, const ChannelType &chType);
  uint32_t getCoLocatedIntraLumaMode(const PredictionUnit &pu);
  int      getWideAngle(const TransformUnit &tu, const uint32_t dirMode, const ComponentID compID);

  const PredictionUnit &getCoLocatedLumaPU(const PredictionUnit &pu);

  void getInterMergeCandidates(const PredictionUnit &pu, MergeCtx &mrgCtx, int mmvdList, const int &mrgCandIdx = -1);
  void getIBCMergeCandidates(const PredictionUnit& pu, MergeCtx& mrgCtx, const int mrgCandIdx);
  void getInterMMVDMergeCandidates(const PredictionUnit &pu, MergeCtx &mrgCtx);
  int getDistScaleFactor(const int &currPOC, const int &currRefPOC, const int &colPOC, const int &colRefPOC);
  bool isDiffMER                      (const Position &pos1, const Position &pos2, const unsigned plevel);
  bool getColocatedMVP                (const PredictionUnit &pu, const RefPicList &eRefPicList, const Position &pos, Mv& rcMv, const int &refIdx, bool sbFlag);
  void fillMvpCand                    (      PredictionUnit &pu, const RefPicList &eRefPicList, const int &refIdx, AMVPInfo &amvpInfo );
  void fillIBCMvpCand                 (PredictionUnit &pu, AMVPInfo &amvpInfo);
  void fillAffineMvpCand              (      PredictionUnit &pu, const RefPicList &eRefPicList, const int &refIdx, AffineAMVPInfo &affiAMVPInfo);
  bool addMVPCandUnscaled(const PredictionUnit &pu, const RefPicList &eRefPicList, const int &refIdx,
                          const Position &pos, const MvpDir &eDir, AMVPInfo &amvpInfo);
#if GDR_ENABLED
  void xInheritedAffineMv(const PredictionUnit &pu, const PredictionUnit *puNeighbour, RefPicList eRefPicList,
                          Mv rcMv[3], bool rcMvSolid[3], MvpType rcMvType[3], Position rcMvPos[3]);
#endif
  void xInheritedAffineMv             ( const PredictionUnit &pu, const PredictionUnit* puNeighbour, RefPicList eRefPicList, Mv rcMv[3] );
  bool addMergeHmvpCand(const CodingStructure &cs, MergeCtx &mrgCtx, const int &mrgCandIdx,
                        const uint32_t maxNumMergeCandMin1, int &cnt, const bool isAvailableA1,
                        const MotionInfo &miLeft, const bool isAvailableB1, const MotionInfo &miAbove,
                        const bool ibcFlag, const bool isGt4x4
#if GDR_ENABLED
                        ,
                        const PredictionUnit &pu, bool &allCandSolidInAbove
#endif
  );
  void addAMVPHMVPCand                (const PredictionUnit &pu, const RefPicList eRefPicList, const Picture* currRefPic, AMVPInfo &info);
  bool addAffineMVPCandUnscaled       ( const PredictionUnit &pu, const RefPicList &refPicList, const int &refIdx, const Position &pos, const MvpDir &dir, AffineAMVPInfo &affiAmvpInfo );
  bool isBipredRestriction            (const PredictionUnit &pu);
  void spanMotionInfo                 (      PredictionUnit &pu, const MergeCtx &mrgCtx = MergeCtx() );
  void spanMotionInfo                 (      PredictionUnit &pu, const MotionBuf& subPuMvpMiBuf);
  void applyImv(PredictionUnit &pu, MergeCtx &mrgCtx, InterPrediction *interPred = nullptr);
#if GDR_ENABLED
  void getAffineControlPointCand(const PredictionUnit& pu, MotionInfo mi[4], bool isAvailable[4], int verIdx[4], int8_t bcwIdx, int modelIdx, int verNum, AffineMergeCtx& affMrgCtx, bool isEncodeGdrClean, bool modelSolid[6]);
#else
  void getAffineControlPointCand(const PredictionUnit &pu, MotionInfo mi[4], bool isAvailable[4], int verIdx[4], int8_t bcwIdx, int modelIdx, int verNum, AffineMergeCtx& affMrgCtx);
#endif
  void getAffineMergeCand( const PredictionUnit &pu, AffineMergeCtx& affMrgCtx, const int mrgCandIdx = -1 );
  void setAllAffineMvField(PredictionUnit &pu, std::array<MvField[2], AFFINE_MAX_NUM_CP> &mvField, RefPicList eRefList);
  void setAllAffineMv                 (      PredictionUnit &pu, Mv affLT, Mv affRT, Mv affLB, RefPicList eRefList, bool clipCPMVs = false );
  bool getInterMergeSubPuMvpCand(const PredictionUnit &pu, MergeCtx &mrgCtx, const int count, int mmvdList);
  bool getInterMergeSubPuRecurCand(const PredictionUnit &pu, MergeCtx &mrgCtx, const int count);
  bool isSimpleSymmetricBiPred(const PredictionUnit &pu);
  void restrictBiPredMergeCandsOne    (PredictionUnit &pu);

  bool isLMCMode                      (                          unsigned mode);
  bool isLMCModeEnabled(const PredictionUnit &pu, unsigned mode);
  void getGeoMergeCandidates          (const PredictionUnit &pu, MergeCtx &GeoMrgCtx);
  void spanGeoMotionInfo(PredictionUnit &pu, const MergeCtx &GeoMrgCtx, const uint8_t splitDir,
                         const MergeIdxPair &candIdx);
  bool addNeighborMv  (const Mv& currMv, static_vector<Mv, IBC_NUM_CANDIDATES>& neighborMvs);
  void getIbcMVPsEncOnly(PredictionUnit &pu, static_vector<Mv, IBC_NUM_CANDIDATES>& mvPred);
  bool getDerivedBV(PredictionUnit &pu, const Mv& currentMv, Mv& derivedMv);
  bool checkDMVRCondition(const PredictionUnit& pu);
  void getNeighborAffineInfo(const PredictionUnit& pu, int& numNeighborAvai, int& numNeighborAffine);

  static inline bool dmvrBdofSizeCheck(const PredictionUnit &pu)
  {
    return pu.lheight() >= 8 && pu.lwidth() >= 8 && pu.lheight() * pu.lwidth() >= 128;
  }
}

// TU tools
namespace TU
{
  uint32_t getNumNonZeroCoeffsNonTSCorner8x8(const TransformUnit &tu, const bool hasLuma = true,
                                             const bool hasChroma = true);
  bool isNonTransformedResidualRotated(const TransformUnit &tu, const ComponentID &compID);
  bool getCbf                         (const TransformUnit &tu, const ComponentID &compID);
  bool getCbfAtDepth                  (const TransformUnit &tu, const ComponentID &compID, const unsigned &depth);
  void setCbfAtDepth                  (      TransformUnit &tu, const ComponentID &compID, const unsigned &depth, const bool &cbf);
  bool isTSAllowed                    (const TransformUnit &tu, const ComponentID  compID);

  bool needsSqrt2Scale                ( const TransformUnit &tu, const ComponentID &compID );
  bool needsBlockSizeTrafoScale       ( const TransformUnit &tu, const ComponentID &compID );
  TransformUnit* getPrevTU          ( const TransformUnit &tu, const ComponentID compID );
  bool           getPrevTuCbfAtDepth( const TransformUnit &tu, const ComponentID compID, const int trDepth );
  int            getICTMode         ( const TransformUnit &tu, int jointCbCr = -1 );
}

uint32_t getCtuAddr(const Position &pos, const PreCalcValues &pcv);
bool allowLfnstWithMip(const Size& block);
#if GREEN_METADATA_SEI_ENABLED
void writeGMFAOutput(FeatureCounterStruct& featureCounter, FeatureCounterStruct& featureCounterReference, std::string GMFAFile, bool lastFrame);
void featureToFile(std::ofstream& featureFile,int featureCounterReference[MAX_CU_DEPTH+1][MAX_CU_DEPTH+1], std::string featureName,bool calcDifference=false,int featureCounter[MAX_CU_DEPTH+1][MAX_CU_DEPTH+1]=NULL);
void countFeatures  (FeatureCounterStruct& featureCounterStruct, CodingStructure& cs, const UnitArea& ctuArea);
#endif
template<typename T, size_t N>
uint32_t updateCandList(T mode, double uiCost, static_vector<T, N> &candModeList,
                        static_vector<double, N> &candCostList, size_t uiFastCandNum = N, int *iserttPos = nullptr)
{
  CHECK( std::min( uiFastCandNum, candModeList.size() ) != std::min( uiFastCandNum, candCostList.size() ), "Sizes do not match!" );
  CHECK( uiFastCandNum > candModeList.capacity(), "The vector is to small to hold all the candidates!" );

  size_t i;
  size_t shift = 0;
  size_t currSize = std::min( uiFastCandNum, candCostList.size() );

  while( shift < uiFastCandNum && shift < currSize && uiCost < candCostList[currSize - 1 - shift] )
  {
    shift++;
  }

  if( candModeList.size() >= uiFastCandNum && shift != 0 )
  {
    for( i = 1; i < shift; i++ )
    {
      candModeList[currSize - i] = candModeList[currSize - 1 - i];
      candCostList[currSize - i] = candCostList[currSize - 1 - i];
    }
    candModeList[currSize - shift] = mode;
    candCostList[currSize - shift] = uiCost;
    if (iserttPos != nullptr)
    {
      *iserttPos = int(currSize - shift);
    }
    return 1;
  }
  else if( currSize < uiFastCandNum )
  {
    candModeList.insert(candModeList.end() - shift, mode);
    candCostList.insert( candCostList.end() - shift, uiCost );
    if (iserttPos != nullptr)
    {
      *iserttPos = int(candModeList.size() - shift - 1);
    }
    return 1;
  }
  if (iserttPos != nullptr)
  {
    *iserttPos = -1;
  }
  return 0;
}

#endif
