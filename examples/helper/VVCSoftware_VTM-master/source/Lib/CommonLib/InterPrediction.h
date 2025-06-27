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

/** \file     InterPrediction.h
    \brief    inter prediction class (header)
*/

#ifndef __INTERPREDICTION__
#define __INTERPREDICTION__


// Include files
#include "InterpolationFilter.h"
#include "WeightPrediction.h"

#include "Buffer.h"
#include "Unit.h"
#include "Picture.h"

#include "RdCost.h"
// forward declaration
class Mv;

//! \ingroup CommonLib
//! \{


// ====================================================================================================================
// Class definition
// ====================================================================================================================

class MergeCtx
{
public:
  MergeCtx() : numValidMergeCand(0), hasMergedCandList(false) {}
  ~MergeCtx() {}
public:
  MvField mvFieldNeighbours[MRG_MAX_NUM_CANDS][2];
#if GDR_ENABLED
  // note : check if source of mv and mv itself is valid
  bool     mvSolid[MRG_MAX_NUM_CANDS][2];
  bool     mvValid[MRG_MAX_NUM_CANDS][2];
  Position mvPos[MRG_MAX_NUM_CANDS][2];
  MvpType  mvType[MRG_MAX_NUM_CANDS][2];
#endif
  uint8_t       bcwIdx[MRG_MAX_NUM_CANDS];
  unsigned char interDirNeighbours[ MRG_MAX_NUM_CANDS      ];
  int           numValidMergeCand;
  bool          hasMergedCandList;

  MotionBuf     subPuMvpMiBuf;
  MvField       mmvdBaseMv[MmvdIdx::BASE_MV_NUM][2];
#if GDR_ENABLED
  bool mmvdSolid[MmvdIdx::BASE_MV_NUM][2];
  bool mmvdValid[MmvdIdx::BASE_MV_NUM][2];
#endif
  void          setMmvdMergeCandiInfo(PredictionUnit &pu, MmvdIdx candIdx);
  void          getMmvdDeltaMv(const Slice& slice, const MmvdIdx candIdx, Mv deltaMv[NUM_REF_PIC_LIST_01]) const;
  bool          mmvdUseAltHpelIf[MmvdIdx::BASE_MV_NUM];
  bool          useAltHpelIf      [ MRG_MAX_NUM_CANDS ];
  void setMergeInfo( PredictionUnit& pu, int candIdx ) const;
};

class AffineMergeCtx
{
public:
  AffineMergeCtx() : numValidMergeCand(0)
  {
    for (int i = 0; i < AFFINE_MRG_MAX_NUM_CANDS; i++)
    {
      affineType[i] = AffineModel::_4_PARAMS;
    }
  }
  ~AffineMergeCtx() {}
public:
  std::array<MvField[2], AFFINE_MAX_NUM_CP> mvFieldNeighbours[AFFINE_MRG_MAX_NUM_CANDS];
#if GDR_ENABLED
  std::array<bool[2], AFFINE_MAX_NUM_CP> mvSolid[AFFINE_MRG_MAX_NUM_CANDS];
  std::array<bool[2], AFFINE_MAX_NUM_CP> mvValid[AFFINE_MRG_MAX_NUM_CANDS];

  bool isSolid(const int idx, const int l)
  {
    bool solid = true;
    for (auto &c: mvSolid[idx])
    {
      solid &= c[l];
    }
    return solid;
  }

  bool isValid(const int idx, const int l)
  {
    bool valid = true;
    for (auto &c: mvValid[idx])
    {
      valid &= c[l];
    }
    return valid;
  }
#endif
  unsigned char interDirNeighbours[AFFINE_MRG_MAX_NUM_CANDS];
  AffineModel   affineType[AFFINE_MRG_MAX_NUM_CANDS];
  uint8_t       bcwIdx[AFFINE_MRG_MAX_NUM_CANDS];
  int           numValidMergeCand;
  int           maxNumMergeCand;

  MergeCtx     *mrgCtx;
  MergeType     mergeType[AFFINE_MRG_MAX_NUM_CANDS];
};

class InterPrediction : public WeightPrediction
{
private:



protected:
  InterpolationFilter  m_if;

  Pel*                 m_acYuvPred            [NUM_REF_PIC_LIST_01][MAX_NUM_COMPONENT];
  Pel*                 m_filteredBlock        [LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL][LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL][MAX_NUM_COMPONENT];
  Pel*                 m_filteredBlockTmp     [LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL][MAX_NUM_COMPONENT];

  static constexpr int TMP_RPR_WIDTH  = MAX_CU_SIZE + 16;
  static constexpr int TMP_RPR_HEIGHT = MAX_CU_SIZE * MAX_SCALING_RATIO + 16;

  Pel *m_filteredBlockTmpRPR;

  ChromaFormat         m_currChromaFormat;

  ComponentID          m_maxCompIDToPred;      ///< tells the predictor to only process the components up to (inklusive) this one - useful to skip chroma components during RD-search

  RdCost*              m_pcRdCost;

  PelStorage           m_geoPartBuf[2];

  static constexpr int MVBUFFER_SIZE = MAX_CU_SIZE / MIN_PU_SIZE;

  Mv*                  m_storedMv;
  // buffers for initial prediction that is used to calculate DMVR refinement
  Pel   *m_yuvPredTempDmvr[NUM_REF_PIC_LIST_01];
  PelBuf m_dmvrInitialPred[NUM_REF_PIC_LIST_01];

  // buffers for padded data, initially filled by xDmvrPrefetch(), padded by xDmvrPad()
  Pel       *m_refSamplesDmvr[NUM_REF_PIC_LIST_01][MAX_NUM_COMPONENT];
  PelUnitBuf m_yuvRefBufDmvr[NUM_REF_PIC_LIST_01];

  static const std::array<Mv, DMVR_AREA> m_dmvrSearchOffsets;

  static constexpr int AFFINE_SUBBLOCK_WIDTH_EXT  = AFFINE_SUBBLOCK_SIZE + 2 * PROF_BORDER_EXT_W;
  static constexpr int AFFINE_SUBBLOCK_HEIGHT_EXT = AFFINE_SUBBLOCK_SIZE + 2 * PROF_BORDER_EXT_H;

  Pel m_gradBuf[2][AFFINE_SUBBLOCK_WIDTH_EXT * AFFINE_SUBBLOCK_HEIGHT_EXT];

  // PROF skip flags for encoder speedup
  bool m_skipProf{ false };
  bool m_skipProfCond{ false };
  bool m_biPredSearchAffine{ false };

  Pel*                 m_gradX0;
  Pel*                 m_gradY0;
  Pel*                 m_gradX1;
  Pel                 *m_gradY1;

  int                  m_IBCBufferWidth;
  PelStorage           m_IBCBuffer;
  void xIntraBlockCopy          (PredictionUnit &pu, PelUnitBuf &predBuf, const ComponentID compID);
  int             rightShiftMSB(int numer, int    denom);
  void            applyBiOptFlow(const PredictionUnit &pu, const CPelUnitBuf &yuvSrc0, const CPelUnitBuf &yuvSrc1, const int &refIdx0, const int &refIdx1, PelUnitBuf &yuvDst, const BitDepths &clipBitDepths);
  void xPredInterUni(const PredictionUnit &pu, const RefPicList &eRefPicList, PelUnitBuf &pcYuvPred, const bool bi,
                     const bool bioApplied, const bool luma, const bool chroma);
  void xPredInterBi(PredictionUnit &pu, PelUnitBuf &pcYuvPred, bool luma, bool chroma, PelUnitBuf *yuvPredTmp,
                    bool isSubPu);

  void xPredInterBlk(const ComponentID compID, const PredictionUnit &pu, const Picture *refPic, const Mv &_mv,
                     PelUnitBuf &dstPic, bool bi, const ClpRng &clpRng, bool bioApplied, bool isIBC,
                     const ScalingRatio scalingRatio, bool bilinearMC, Pel *srcPadBuf, ptrdiff_t srcPadStride,
                     Pel *bdofDstBuf);
  void xPredInterBlk(const ComponentID compID, const PredictionUnit &pu, const Picture *refPic, const Mv &_mv,
                     PelUnitBuf &dstPic, bool bi, const ClpRng &clpRng, bool bioApplied, bool isIBC, RefPicList l)
  {
    xPredInterBlk(compID, pu, refPic, _mv, dstPic, bi, clpRng, bioApplied, isIBC, SCALE_1X, false, nullptr, 0,
                  isLuma(compID) && bioApplied ? m_filteredBlockTmp[2 + l][compID] : nullptr);
  }

  void xAddBIOAvg4(const Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride, Pel *dst,
                   ptrdiff_t dstStride, const Pel *gradX0, const Pel *gradX1, const Pel *gradY0, const Pel *gradY1,
                   ptrdiff_t gradStride, int width, int height, int tmpx, int tmpy, int shift, int offset,
                   const ClpRng &clpRng);
  void xBioGradFilter(Pel *pSrc, ptrdiff_t srcStride, int width, int height, ptrdiff_t gradStride, Pel *gradX,
                      Pel *gradY, int bitDepth);
  void xCalcBIOPar              (const Pel* srcY0Temp, const Pel* srcY1Temp, const Pel* gradX0, const Pel* gradX1, const Pel* gradY0, const Pel* gradY1, int* dotProductTemp1, int* dotProductTemp2, int* dotProductTemp3, int* dotProductTemp5, int* dotProductTemp6, const int src0Stride, const int src1Stride, const int gradStride, const int widthG, const int heightG, int bitDepth);
  void xCalcBlkGradient         (int sx, int sy, int    *arraysGx2, int     *arraysGxGy, int     *arraysGxdI, int     *arraysGy2, int     *arraysGydI, int     &sGx2, int     &sGy2, int     &sGxGy, int     &sGxdI, int     &sGydI, int width, int height, int unitSize);
  void xWeightedAverage(const PredictionUnit &pu, const CPelUnitBuf &pcYuvSrc0, const CPelUnitBuf &pcYuvSrc1,
                        PelUnitBuf &pcYuvDst, const BitDepths &clipBitDepths, const ClpRngs &clpRngs, bool bioApplied,
                        bool lumaOnly, bool chromaOnly, PelUnitBuf *yuvDstTmp);
#if GDR_ENABLED
  bool xPredAffineBlk(const ComponentID &compID, const PredictionUnit &pu, const Picture *refPic, const Mv *_mv,
                      PelUnitBuf &dstPic, const bool bi, const ClpRng &clpRng, const bool genChromaMv = false,
                      const ScalingRatio = SCALE_1X);
#else
  void xPredAffineBlk(const ComponentID &compID, const PredictionUnit &pu, const Picture *refPic, const Mv *_mv,
                      PelUnitBuf &dstPic, const bool bi, const ClpRng &clpRng, const bool genChromaMv = false,
                      const ScalingRatio = SCALE_1X);
#endif

  static bool xCheckIdenticalMotion( const PredictionUnit& pu );

  void xSubPuMC(PredictionUnit &pu, PelUnitBuf &predBuf, const RefPicList &eRefPicList, bool luma, bool chroma);
  void xSubPuBio(PredictionUnit &pu, PelUnitBuf &predBuf, const RefPicList &eRefPicList, PelUnitBuf *yuvDstTmp);
  void destroy();


  MotionInfo      m_SubPuMiBuf[(MAX_CU_SIZE * MAX_CU_SIZE) >> (MIN_CU_LOG2 << 1)];
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  CacheModel      *m_cacheModel;
#endif
  PelStorage       m_colorTransResiBuf[3];  // 0-org; 1-act; 2-tmp

public:
  InterPrediction();
  virtual ~InterPrediction();

  void init(RdCost* pcRdCost, ChromaFormat chromaFormatIdc, const int ctuSize);

  // inter
  void motionCompensation(PredictionUnit &pu, PelUnitBuf &predBuf, RefPicList eRefPicList, bool luma, bool chroma,
                          PelUnitBuf *predBufWOBIO, bool isSubPu);

  void motionCompensation(PredictionUnit &pu, PelUnitBuf &predBuf, RefPicList eRefPicList)
  {
    motionCompensation(pu, predBuf, eRefPicList, true, true, nullptr, false);
  }

  void motionCompensatePu(PredictionUnit &pu, RefPicList eRefPicList, bool luma, bool chroma);
  void motionCompensateCu(CodingUnit &cu, RefPicList eRefPicList, bool luma, bool chroma);

  void    motionCompensationGeo(CodingUnit &cu, MergeCtx &GeoMrgCtx);
  void    weightedGeoBlk(PredictionUnit &pu, const uint8_t splitDir, ChannelType channel, PelUnitBuf &predDst,
                         PelUnitBuf &predSrc0, PelUnitBuf &predSrc1);

  // DMVR related definitions
  using DmvrDist = int32_t;

  static constexpr DmvrDist UNDEFINED_DMVR_DIST = -1;
  bool     dmvrEnableEncoderCheck;
  void     xDmvrSetEncoderCheckFlag(bool enableFlag) { dmvrEnableEncoderCheck = enableFlag; }
  bool     xDmvrGetEncoderCheckFlag() { return dmvrEnableEncoderCheck; }
  void     xDmvrPrefetch(const PredictionUnit &pu, bool forLuma);
  void     xDmvrPad(const PredictionUnit &pu);
  void     xDmvrFinalMc(const PredictionUnit &pu, PelUnitBuf yuvSrc[NUM_REF_PIC_LIST_01], bool applyBdof,
                        const Mv startMV[NUM_REF_PIC_LIST_01], bool blockMoved);
  void     xDmvrIntegerRefine(int bd, DmvrDist &minCost, Mv &deltaMv, DmvrDist *sadPtr, int width, int height);
  Mv       xDmvrSubpelRefine(const DmvrDist *sadPtr);
  DmvrDist xDmvrCost(int bitDepth, const Mv &mvd, int width, int height);
  void     xDmvrInitialMc(const PredictionUnit &pu, const ClpRngs &clpRngs);
  void     xProcessDMVR(PredictionUnit &pu, PelUnitBuf &pcYuvDst, const ClpRngs &clpRngs, const bool applyBdof);

#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  void    cacheAssign( CacheModel *cache );
#endif
  static bool isSubblockVectorSpreadOverLimit( int a, int b, int c, int d, int predType );

  void xFillIBCBuffer(CodingUnit &cu);
  void resetIBCBuffer(const ChromaFormat chromaFormatIdc, const int ctuSize);
  void resetVPDUforIBC(const ChromaFormat chromaFormatIdc, const int ctuSize, const int vSize, const int xPos,
                       const int yPos);
  bool isLumaBvValid(const int ctuSize, const int xCb, const int yCb, const int width, const int height, const int xBv, const int yBv);

  bool xPredInterBlkRPR(const ScalingRatio scalingRatio, const SPS& sps, const PPS& pps, const CompArea& blk,
                        const Picture *refPic, const Mv &mv, Pel *dst, const ptrdiff_t dstStride, const bool bi,
                        const bool wrapRef, const ClpRng &clpRng, const InterpolationFilter::Filter filterIndex,
                        const bool useAltHpelIf = false);
};

//! \}

#endif // __INTERPREDICTION__
