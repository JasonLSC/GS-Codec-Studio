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

/** \file     MatrixIntraPrediction.h
\brief    matrix-based intra prediction class (header)
*/

#ifndef __MATRIXINTRAPPREDICTION__
#define __MATRIXINTRAPPREDICTION__


#include "Unit.h"

static constexpr int MIP_MAX_INPUT_SIZE             =  8;
static constexpr int MIP_MAX_REDUCED_OUTPUT_SAMPLES = 64;

class MatrixIntraPrediction
{
public:
  MatrixIntraPrediction();

  void prepareInputForPred(const CPelBuf &pSrc, const Area &block, const int bitDepth, const ComponentID compId);
  void predBlock(Pel *const result, const int modeIdx, const bool transpose, const int bitDepth,
                 const ComponentID compId);

  static int getNumModesMip(const Size &block);

private:
  enum class MipSizeId
  {
    S0 = 0,
    S1,
    S2,
    NUM
  };

  static MipSizeId getMipSizeId(const Size &block);

  ComponentID m_component;

  static_vector<Pel, MIP_MAX_INPUT_SIZE> m_reducedBoundary;             // downsampled             boundary of a block
  static_vector<Pel, MIP_MAX_INPUT_SIZE> m_reducedBoundaryTransposed;   // downsampled, transposed boundary of a block
  int                                    m_inputOffset;
  int                                    m_inputOffsetTransp;
  static_vector<Pel, MIP_MAX_WIDTH>      m_refSamplesTop;    // top  reference samples for upsampling
  static_vector<Pel, MIP_MAX_HEIGHT>     m_refSamplesLeft;   // left reference samples for upsampling

  Size         m_blockSize;
  MipSizeId    m_sizeId;
  int          m_reducedBdrySize;
  int          m_reducedPredSize;
  unsigned int m_upsmpFactorHor;
  unsigned int m_upsmpFactorVer;

  void initPredBlockParams(const Size &block);

  static void boundaryDownsampling1D(Pel *reducedDst, const Pel *const fullSrc, const SizeType srcLen,
                                     const SizeType dstLen);

  void        predictionUpsampling(Pel *const dst, const Pel *const src) const;
  static void predictionUpsampling1D(Pel *const dst, const Pel *const src, const Pel *const bndry,
                                     const SizeType srcSizeUpsmpDim, const SizeType srcSizeOrthDim,
                                     const SizeType srcStep, const SizeType srcStride, const SizeType dstStep,
                                     const SizeType dstStride, const SizeType bndryStep,
                                     const unsigned int upsmpFactor);

  const uint8_t *getMatrixData(const int modeIdx) const;

  void computeReducedPred(Pel *const result, const Pel *const input, const uint8_t *matrix, const bool transpose,
                          const int bitDepth);
  };

#endif //__MATRIXINTRAPPREDICTION__
