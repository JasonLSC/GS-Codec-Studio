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

 /** \file     EncReshape.h
     \brief    encoder reshaping header and class (header)
 */

#ifndef __ENCRESHAPE__
#define __ENCRESHAPE__

#pragma once

#include "CommonLib/Reshape.h"

//! \ingroup EncoderLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================
struct SeqInfo
{
  double binVar[PIC_ANALYZE_CW_BINS];
  double binHist[PIC_ANALYZE_CW_BINS];
  double normVar[PIC_ANALYZE_CW_BINS];
  int    nonZeroCnt;
  double weightVar;
  double weightNorm;
  double minBinVar;
  double maxBinVar;
  double meanBinVar;
  double ratioStdU;
  double ratioStdV;
};

class EncReshape : public Reshape
{
private:
  bool                    m_srcReshaped;
  int                     m_picWidth;
  int                     m_picHeight;
  uint32_t                m_maxCUWidth;
  uint32_t                m_maxCUHeight;
  uint32_t                m_widthInCtus;
  uint32_t                m_heightInCtus;
  uint32_t                m_numCtuInFrame;
  bool                    m_exceedSTD;
  std::vector<uint32_t>   m_binImportance;
  int                     m_tcase;
  int                     m_rateAdpMode;
  bool                    m_useAdpCW;
  uint16_t                m_initCWAnalyze;
  ReshapeCW               m_reshapeCW;
  Pel                     m_cwLumaWeight[PIC_CODE_CW_BINS];
  double                  m_chromaWeight;
  int                     m_chromaAdj;
  int                     m_binNum;
  SeqInfo                 m_srcSeqStats;
  SeqInfo                 m_rspSeqStats;
public:

  EncReshape();
  ~EncReshape();

  void createEnc( int picWidth, int picHeight, uint32_t maxCUWidth, uint32_t maxCUHeight, int bitDepth);
  void destroy();

  bool getSrcReshaped() { return m_srcReshaped; }
  void setSrcReshaped(bool b) { m_srcReshaped = b; }
  void initSeqStats(SeqInfo &stats);
  void calcSeqStats(Picture *pcPic, SeqInfo &stats);
  void preAnalyzerLMCS(Picture *pcPic, const uint32_t signalType, const SliceType sliceType, const ReshapeCW& reshapeCW);
  void preAnalyzerHDR(Picture *pcPic, const SliceType sliceType, const ReshapeCW& reshapeCW, bool isDualT);
  void bubbleSortDsd(double *array, int * idx, int n);
  void swap(int *xp, int *yp) { int temp = *xp;  *xp = *yp;  *yp = temp; }
  void swap(double *xp, double *yp) { double temp = *xp;  *xp = *yp;  *yp = temp; }
  void cwPerturbation(int startBinIdx, int endBinIdx, uint16_t maxCW);
  void cwReduction(int startBinIdx, int endBinIdx);
  void deriveReshapeParametersSDR(bool *intraAdp, bool *interAdp);
  void deriveReshapeParameters(double *array, int start, int end, ReshapeCW respCW, double &alpha, double &beta);
  void initLUTfromdQPModel();
  void constructReshaperLMCS();
  ReshapeCW * getReshapeCW() { return &m_reshapeCW; }
  Pel * getWeightTable() { return m_cwLumaWeight; }
  double getCWeight() { return m_chromaWeight; }
  void adjustLmcsPivot();
};// END CLASS DEFINITION EncReshape

//! \}
#endif
