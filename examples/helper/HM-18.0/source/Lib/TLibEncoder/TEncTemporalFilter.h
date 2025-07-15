/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2022, ITU/ISO/IEC
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

/** \file     TEncTemporalFilter.h
    \brief    TEncTemporalFilter class (header)
 */

#ifndef __TEMPORAL_FILTER__
#define __TEMPORAL_FILTER__
#include "TLibCommon/TComPicYuv.h"
#include "Utilities/TVideoIOYuv.h"
#include <sstream>
#include <map>
#include <deque>

 //! \ingroup EncoderLib
 //! \{

struct MotionVector
{
  Int x, y;
  Int error;
#if JVET_V0056_MCTF || JVET_Y0077_BIM
  Int noise;
  MotionVector() : x(0), y(0), error(INT_LEAST32_MAX), noise(0) {}
#else
  MotionVector() : x(0), y(0), error(INT_LEAST32_MAX) {}
#endif
  void set(Int nx, Int ny, Int ne) { x = nx; y = ny; error = ne; }
};

template <class T>
struct Array2D
{
private:
  UInt m_width, m_height;
  std::vector< T > v;
public:
  Array2D() : m_width(0), m_height(0), v() { }
  Array2D(UInt width, UInt height, const T& value=T()) : m_width(0), m_height(0), v() { allocate(width, height, value); }

#if JVET_Y0077_BIM
  UInt w() const { return m_width;  }
  UInt h() const { return m_height; }
#endif

  Void allocate(UInt width, UInt height, const T& value=T())
  {
    m_width=width;
    m_height=height;
    v.resize(std::size_t(m_width*m_height), value);
  }

  T& get(UInt x, UInt y)
  {
    assert(x<m_width && y<m_height);
    return v[y*m_width+x];
  }

  const T& get(UInt x, UInt y) const
  {
    assert(x<m_width && y<m_height);
    return v[y*m_width+x];
  }
};

struct TemporalFilterSourcePicInfo
{
  TemporalFilterSourcePicInfo() : picBuffer(), mvs(), origOffset(0) { }
  TComPicYuv            picBuffer;
  Array2D<MotionVector> mvs;
  Int                   origOffset;
};

// ====================================================================================================================
// Class definition
// ====================================================================================================================

class TEncTemporalFilter
{
public:
   TEncTemporalFilter();
  ~TEncTemporalFilter() {}

  void init(const Int frameSkip,
            const Int inputBitDepth[MAX_NUM_CHANNEL_TYPE],
            const Int MSBExtendedBitDepth[MAX_NUM_CHANNEL_TYPE], const Int InternalBitDepth[MAX_NUM_CHANNEL_TYPE],
            const Int width,
            const Int height,
            const Int *pad,
            const Int frames,
            const Bool Rec709,
            const std::string &filename,
            const ChromaFormat inputChromaFormatIDC,
            const InputColourSpaceConversion colorSpaceConv,
            const Int qp,
            const Int GOPSize,
            const std::map<Int, Double> &temporalFilterStrengths,
            const Int pastRefs,
            const Int futureRefs,
            const Int firstValidFrame,
#if !JVET_Y0077_BIM
            const Int lastValidFrame);
#else
            const Int lastValidFrame,
            const Bool mctfEnabled,
            std::map<Int, Int*> *adaptQPmap,
            const Bool bimEnabled);
#endif

  Bool filter(TComPicYuv *orgPic, Int frame);

private:
  // Private static member variables
  static const Double s_chromaFactor;
  static const Double s_sigmaMultiplier;
  static const Double s_sigmaZeroPoint;
  static const Int s_motionVectorFactor;
  static const Int s_padding;
  static const Int s_interpolationFilter[16][8];
#if JVET_V0056_MCTF
  static const Double s_refStrengths[2][4];
#else
  static const Double s_refStrengths[2][2];
#endif
#if JVET_Y0077_BIM
  static const Int s_cuTreeThresh[4];
#endif

  // Private member variables
  Int m_FrameSkip;
  std::string m_inputFileName;
  Int m_inputBitDepth[MAX_NUM_CHANNEL_TYPE];
  Int m_MSBExtendedBitDepth[MAX_NUM_CHANNEL_TYPE];
  Int m_internalBitDepth[MAX_NUM_CHANNEL_TYPE];
  ChromaFormat m_chromaFormatIDC;
  Int m_sourceWidth;
  Int m_sourceHeight;
  Int m_QP;
  Int m_GOPSize;
  std::map<Int, Double> m_temporalFilterStrengths;
  Int m_sourcePadding[2];
  Int m_framesToBeEncoded;
  Bool m_bClipInputVideoToRec709Range;
  InputColourSpaceConversion m_inputColourSpaceConvert;
  Int m_pastRefs;
  Int m_futureRefs;
  Int m_firstValidFrame;
  Int m_lastValidFrame;
#if JVET_Y0077_BIM
  Bool m_mctfEnabled;
  Bool m_bimEnabled;
  Int m_numCTU;
  std::map<Int, Int*> *m_ctuAdaptQP;
#endif

  // Private functions
  Void subsampleLuma(const TComPicYuv &input, TComPicYuv &output, const Int factor = 2) const;
  Int motionErrorLuma(const TComPicYuv &orig, const TComPicYuv &buffer, const Int x, const Int y, Int dx, Int dy, const Int bs, const Int besterror = 8 * 8 * 1024 * 1024) const;
  Void motionEstimationLuma(Array2D<MotionVector> &mvs, const TComPicYuv &orig, const TComPicYuv &buffer, const Int bs,
      const Array2D<MotionVector> *previous=0, const Int factor = 1, const Bool doubleRes = false) const;
  Void motionEstimation(Array2D<MotionVector> &mvs, const TComPicYuv &orgPic, const TComPicYuv &buffer, const TComPicYuv &origSubsampled2, const TComPicYuv &origSubsampled4) const;

#if JVET_V0056_MCTF
  Void bilateralFilter(const TComPicYuv &orgPic, std::deque<TemporalFilterSourcePicInfo> &srcFrameInfo, TComPicYuv &newOrgPic, Double overallStrength) const;
#else
  Void bilateralFilter(const TComPicYuv &orgPic, const std::deque<TemporalFilterSourcePicInfo> &srcFrameInfo, TComPicYuv &newOrgPic, Double overallStrength) const;
#endif
  Void applyMotion(const Array2D<MotionVector> &mvs, const TComPicYuv &input, TComPicYuv &output) const;
}; // END CLASS DEFINITION TEncTemporalFilter

//! \}

#endif // __TEMPORAL_FILTER__
