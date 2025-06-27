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

/**
 \file     EncSampleAdaptiveOffset.h
 \brief    estimation part of sample adaptive offset class (header)
 */

#ifndef __ENCSAMPLEADAPTIVEOFFSET__
#define __ENCSAMPLEADAPTIVEOFFSET__

#include "CommonLib/SampleAdaptiveOffset.h"

#include "CABACWriter.h"

//! \ingroup EncoderLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

struct SAOStatData //data structure for SAO statistics
{
  int64_t diff[MAX_NUM_SAO_CLASSES];
  int64_t count[MAX_NUM_SAO_CLASSES];

  SAOStatData(){}
  ~SAOStatData(){}
  void reset()
  {
    ::memset(diff, 0, sizeof(int64_t)*MAX_NUM_SAO_CLASSES);
    ::memset(count, 0, sizeof(int64_t)*MAX_NUM_SAO_CLASSES);
  }
  const SAOStatData& operator=(const SAOStatData& src)
  {
    ::memcpy(diff, src.diff, sizeof(int64_t)*MAX_NUM_SAO_CLASSES);
    ::memcpy(count, src.count, sizeof(int64_t)*MAX_NUM_SAO_CLASSES);
    return *this;
  }
  const SAOStatData& operator+= (const SAOStatData& src)
  {
    for(int i=0; i< MAX_NUM_SAO_CLASSES; i++)
    {
      diff[i] += src.diff[i];
      count[i] += src.count[i];
    }
    return *this;
  }
};

class EncSampleAdaptiveOffset : public SampleAdaptiveOffset
{
  using StatDataArray = EnumArray<SAOStatData, SAOModeNewTypes>;

public:
  EncSampleAdaptiveOffset();
  virtual ~EncSampleAdaptiveOffset();

  //interface
  void createEncData(bool isPreDBFSamplesUsed, uint32_t numCTUsPic);
  void destroyEncData();
  void initCABACEstimator(CABACEncoder *cabacEncoder, CtxPool *ctxPool, Slice *pcSlice);
  void SAOProcess(CodingStructure &cs, bool *sliceEnabled, const double *lambdas,
#if ENABLE_QPA
                  const double lambdaChromaWeight,
#endif
                  const bool testSAODisableAtPictureLevel, const double saoEncodingRate,
                  const double saoEncodingRateChroma, const bool isPreDBFSamplesUsed, bool isGreedyMergeEncoding,
                  bool usingTrueOrg);

  void disabledRate(CodingStructure &cs, SAOBlkParam *reconParams, const double saoEncodingRate,
                    const double saoEncodingRateChroma);
  void getPreDBFStatistics(CodingStructure &cs, bool usingTrueOrg);

private:   // methods
  void deriveLoopFilterBoundaryAvailability(CodingStructure &cs, const Position &pos, bool &isLeftAvail,
                                            bool &isAboveAvail, bool &isAboveLeftAvail) const;
  void getStatistics(std::vector<StatDataArray *> &blkStats, PelUnitBuf &orgYuv, PelUnitBuf &srcYuv,
                     CodingStructure &cs, bool isCalculatePreDeblockSamples = false);
  void decidePicParams(const Slice& slice, bool* sliceEnabled, const double saoEncodingRate, const double saoEncodingRateChroma);
  void decideBlkParams(CodingStructure &cs, bool *sliceEnabled, std::vector<StatDataArray *> &blkStats,
                       PelUnitBuf &srcYuv, PelUnitBuf &resYuv, SAOBlkParam *reconParams, SAOBlkParam *codedParams,
                       const bool testSAODisableAtPictureLevel,
#if ENABLE_QPA
                       const double chromaWeight,
#endif
                       const double saoEncodingRate, const double saoEncodingRateChroma,
                       const bool isGreedymergeEncoding);
  void    getBlkStats(const ComponentID compIdx, const int channelBitDepth, StatDataArray &statsDataTypes, Pel *srcBlk,
                      Pel *orgBlk, ptrdiff_t srcStride, ptrdiff_t orgStride, int width, int height, bool isLeftAvail,
                      bool isRightAvail, bool isAboveAvail, bool isBelowAvail, bool isAboveLeftAvail,
                      bool isAboveRightAvail, bool isCalculatePreDeblockSamples, bool isCtuCrossedByVirtualBoundaries,
                      int horVirBndryPos[], int verVirBndryPos[], int numHorVirBndry, int numVerVirBndry);
  void    deriveModeNewRDO(const BitDepths &bitDepths, int ctuRsAddr, MergeBlkParams &mergeList, bool *sliceEnabled,
                           std::vector<StatDataArray *> &blkStats, SAOBlkParam &modeParam, double &modeNormCost);
  void    deriveModeMergeRDO(const BitDepths &bitDepths, int ctuRsAddr, MergeBlkParams &mergeList, bool *sliceEnabled,
                             std::vector<StatDataArray *> &blkStats, SAOBlkParam &modeParam, double &modeNormCost);
  int64_t getDistortion(const int channelBitDepth, SAOModeNewTypes typeIdc, int typeAuxInfo, int *offsetVal,
                        SAOStatData &statData);
  void    deriveOffsets(ComponentID compIdx, const int channelBitDepth, SAOModeNewTypes typeIdc, SAOStatData &statData,
                        int *quantOffsets, int &typeAuxInfo);

  int64_t estSaoDist(int64_t count, int64_t offset, int64_t diffSum, int shift)
  {
    return (count * offset * offset - diffSum * offset * 2) >> shift;
  }

  int  estIterOffset(SAOModeNewTypes typeIdx, double lambda, int offsetInput, int64_t count, int64_t diffSum, int shift,
                     int bitIncrease, int64_t &bestDist, double &bestCost, int offsetTh);
  void addPreDBFStatistics(std::vector<StatDataArray *> &blkStats);

private: //members
  //for RDO
  CABACWriter *m_CABACEstimator{ nullptr };
  CtxPool     *m_ctxPool{ nullptr };
  double       m_lambda[MAX_NUM_COMPONENT];

  //statistics
  std::vector<StatDataArray *> m_statData;   //[ctu][comp][classes]
  std::vector<StatDataArray *> m_preDBFstatData;

  double m_saoDisabledRate[MAX_NUM_COMPONENT][MAX_TLAYER];

  EnumArray<int, SAOModeNewTypes> m_skipLinesR[MAX_NUM_COMPONENT];
  EnumArray<int, SAOModeNewTypes> m_skipLinesB[MAX_NUM_COMPONENT];
};


//! \}

#endif
