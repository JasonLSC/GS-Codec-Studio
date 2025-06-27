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

/** \file     AdaptiveLoopFilter.h
    \brief    adaptive loop filter class (header)
*/

#ifndef __ADAPTIVELOOPFILTER__
#define __ADAPTIVELOOPFILTER__

#include "CommonDef.h"

#include "Unit.h"
#include "UnitTools.h"

struct AlfClassifier
{
  AlfClassifier() {}
  AlfClassifier( uint8_t cIdx, uint8_t tIdx )
    : classIdx( cIdx ), transposeIdx( tIdx )
  {
  }

  uint8_t classIdx;
  uint8_t transposeIdx;
};

enum Direction
{
  HOR,
  VER,
  DIAG0,
  DIAG1,
  NUM_DIRECTIONS
};

class AdaptiveLoopFilter
{
public:
  static constexpr int COEFF_SCALE_BITS = 7;   // 8-bit signed values

  static inline Pel clipALF(const Pel clip, const Pel ref, const Pel val0, const Pel val1)
  {
    return Clip3<Pel>(-clip, +clip, val0-ref) + Clip3<Pel>(-clip, +clip, val1-ref);
  }

  static const EnumArray<int, ChannelType> ALF_NUM_CLIP_VALS;
  static constexpr int MAX_ALF_NUM_CLIP_VALS                   = 4;

  static constexpr int   m_CLASSIFICATION_BLK_SIZE = 32;   // non-normative, local buffer size

  AdaptiveLoopFilter();
  virtual ~AdaptiveLoopFilter() {}
  void reconstructCoeffAPSs(CodingStructure& cs, bool luma, bool chroma, bool isRdo);
  void reconstructCoeff(AlfParam& alfParam, ChannelType channel, const bool isRdo, const bool isRedo = false);
  void ALFProcess(CodingStructure& cs);
  void        create(const int picWidth, const int picHeight, const ChromaFormat format, const int maxCUWidth,
                     const int maxCUHeight, const int maxCUDepth, const BitDepths &inputBitDepth);
  void destroy();
  static void deriveClassificationBlk(AlfClassifier **classifier, int **laplacian[NUM_DIRECTIONS],
                                      const CPelBuf &srcLuma, const Area &blkDst, const Area &blk, const int shift,
                                      const int vbCTUHeight, int vbPos);
  void deriveClassification( AlfClassifier** classifier, const CPelBuf& srcLuma, const Area& blkDst, const Area& blk );
  template<AlfFilterType filtTypeCcAlf>
  static void filterBlkCcAlf(const PelBuf& dstBuf, const CPelUnitBuf& recSrc, const Area& blkDst, const Area& blkSrc,
                             const ComponentID compId, const AlfCoeff* filterCoeff, const ClpRngs& clpRngs,
                             CodingStructure& cs, int vbCTUHeight, int vbPos);

  template<AlfFilterType filtType> static void filterBlk(AlfClassifier** classifier, const PelUnitBuf& recDst,
                                                         const CPelUnitBuf& recSrc, const Area& blkDst, const Area& blk,
                                                         const ComponentID compId, const AlfCoeff* filterSet,
                                                         const Pel* fClipSet, const ClpRng& clpRng, CodingStructure& cs,
                                                         const int vbCTUHeight, int vbPos);
  void (*m_deriveClassificationBlk)(AlfClassifier **classifier, int **laplacian[NUM_DIRECTIONS], const CPelBuf &srcLuma,
                                    const Area &blkDst, const Area &blk, const int shift, const int vbCTUHeight,
                                    int vbPos);

  void (*m_filterCcAlf)(const PelBuf& dstBuf, const CPelUnitBuf& recSrc, const Area& blkDst, const Area& blkSrc,
                        const ComponentID compId, const AlfCoeff* filterCoeff, const ClpRngs& clpRngs,
                        CodingStructure& cs, int vbCTUHeight, int vbPos);
  void applyCcAlfFilter(CodingStructure& cs, ComponentID compID, const PelBuf& dstBuf, const PelUnitBuf& recYuvExt,
                        uint8_t*       filterControl,
                        const AlfCoeff filterSet[MAX_NUM_CC_ALF_FILTERS][MAX_NUM_CC_ALF_CHROMA_COEFF],
                        const int      selectedFilterIdx);
  CcAlfFilterParam &getCcAlfFilterParam() { return m_ccAlfFilterParam; }
  uint8_t* getCcAlfControlIdc(const ComponentID compID)   { return m_ccAlfFilterControl[compID-1]; }
  void (*m_filter5x5Blk)(AlfClassifier** classifier, const PelUnitBuf& recDst, const CPelUnitBuf& recSrc,
                         const Area& blkDst, const Area& blk, const ComponentID compId, const AlfCoeff* filterSet,
                         const Pel* fClipSet, const ClpRng& clpRng, CodingStructure& cs, const int vbCTUHeight,
                         int vbPos);
  void (*m_filter7x7Blk)(AlfClassifier** classifier, const PelUnitBuf& recDst, const CPelUnitBuf& recSrc,
                         const Area& blkDst, const Area& blk, const ComponentID compId, const AlfCoeff* filterSet,
                         const Pel* fClipSet, const ClpRng& clpRng, CodingStructure& cs, const int vbCTUHeight,
                         int vbPos);

#ifdef TARGET_SIMD_X86
  void initAdaptiveLoopFilterX86();
  template <X86_VEXT vext>
  void _initAdaptiveLoopFilterX86();
#endif

protected:
  bool isCrossedByVirtualBoundaries( const CodingStructure& cs, const int xPos, const int yPos, const int width, const int height, bool& clipTop, bool& clipBottom, bool& clipLeft, bool& clipRight, int& numHorVirBndry, int& numVerVirBndry, int horVirBndryPos[], int verVirBndryPos[], int& rasterSliceAlfPad );

  CcAlfFilterParam       m_ccAlfFilterParam;
  uint8_t*               m_ccAlfFilterControl[2] = { nullptr };
  static const int             m_classToFilterMapping[ALF_NUM_FIXED_FILTER_SETS][MAX_NUM_ALF_CLASSES];
  static const AlfCoeff        m_fixedFilterSetCoeff[ALF_FIXED_FILTER_NUM][MAX_NUM_ALF_LUMA_COEFF];
  AlfCoeff m_fixedFilterSetCoeffDec[ALF_NUM_FIXED_FILTER_SETS][MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF];
  AlfCoeff m_coeffApsLuma[ALF_CTB_MAX_NUM_APS][MAX_NUM_ALF_LUMA_COEFF * MAX_NUM_ALF_CLASSES];
  AlfClipIdx m_clippApsLuma[ALF_CTB_MAX_NUM_APS][MAX_NUM_ALF_LUMA_COEFF * MAX_NUM_ALF_CLASSES];
  Pel                          m_clipValsApsLuma[ALF_CTB_MAX_NUM_APS][MAX_NUM_ALF_LUMA_COEFF * MAX_NUM_ALF_CLASSES];
  Pel                          m_clipDefault[MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF];
  bool                         m_created = false;
  AlfParam*                    m_alfParamChroma;
  EnumArray<std::array<Pel, MAX_ALF_NUM_CLIP_VALS>, ChannelType> m_alfClippingValues;
  std::vector<AlfFilterShape>  m_filterShapesCcAlf[2];
  EnumArray<std::vector<AlfFilterShape>, ChannelType>            m_filterShapes;
  AlfClassifier**              m_classifier;
  AlfCoeff                     m_coeffFinal[MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF];
  AlfClipIdx                   m_clippFinal[MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF];
  Pel                          m_clipValsFinal[MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF];
  AlfCoeff                     m_chromaCoeffFinal[ALF_MAX_NUM_ALTERNATIVES_CHROMA][MAX_NUM_ALF_CHROMA_COEFF];
  AlfClipIdx                   m_chromaClippFinal[ALF_MAX_NUM_ALTERNATIVES_CHROMA][MAX_NUM_ALF_CHROMA_COEFF];
  Pel                          m_chromaClipValsFinal[ALF_MAX_NUM_ALTERNATIVES_CHROMA][MAX_NUM_ALF_CHROMA_COEFF];
  int**                        m_laplacian[NUM_DIRECTIONS];
  int *                        m_laplacianPtr[NUM_DIRECTIONS][m_CLASSIFICATION_BLK_SIZE + 5];
  int m_laplacianData[NUM_DIRECTIONS][m_CLASSIFICATION_BLK_SIZE + 5][m_CLASSIFICATION_BLK_SIZE + 5];

  AlfMode *m_modes[MAX_NUM_COMPONENT];

  PelStorage                   m_tempBuf;
  PelStorage                   m_tempBuf2;
  BitDepths                    m_inputBitDepth;
  int                          m_picWidth;
  int                          m_picHeight;
  int                          m_maxCUWidth;
  int                          m_maxCUHeight;
  int                          m_maxCUDepth;
  int                          m_numCTUsInWidth;
  int                          m_numCTUsInHeight;
  int                          m_numCTUsInPic;
  int                          m_alfVBLumaPos;
  int                          m_alfVBChmaPos;
  int                          m_alfVBLumaCTUHeight;
  int                          m_alfVBChmaCTUHeight;
  ChromaFormat                 m_chromaFormat;
  ClpRngs                      m_clpRngs;

  const AlfCoeff* getCoeffVals(AlfMode m) const
  {
    return isAlfLumaFixed(m) ? m_fixedFilterSetCoeffDec[m - AlfMode::LUMA_FIXED0] : m_coeffApsLuma[m - AlfMode::LUMA0];
  }
  const Pel* getClipVals(AlfMode m) const
  {
    return isAlfLumaFixed(m) ? m_clipDefault : m_clipValsApsLuma[m - AlfMode::LUMA0];
  }
};

#endif
