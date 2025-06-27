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
 * \file
 * \brief Declaration of AffineGradientSearch class
 */

#ifndef __AFFINEGRADIENTSEARCH__
#define __AFFINEGRADIENTSEARCH__

#include "CommonDef.h"

//! \ingroup CommonLib
//! \{

class AffineGradientSearch
{
public:
  void (*m_HorizontalSobelFilter)(Pel *const pPred, const ptrdiff_t predStride, int *const pDerivate,
                                  const ptrdiff_t derivateBufStride, const int width, const int height);

  void (*m_VerticalSobelFilter)(Pel *const pPred, const ptrdiff_t predStride, int *const pDerivate,
                                const ptrdiff_t derivateBufStride, const int width, const int height);

  void (*m_EqualCoeffComputer)(Pel *pResidue, ptrdiff_t residueStride, int **ppDerivate, ptrdiff_t derivateBufStride,
                               int64_t (*pEqualCoeff)[7], int width, int height, bool b6Param);

  static void xHorizontalSobelFilter(Pel *const pPred, const ptrdiff_t predStride, int *const pDerivate,
                                     const ptrdiff_t derivateBufStride, const int width, const int height);

  static void xVerticalSobelFilter(Pel *const pPred, const ptrdiff_t predStride, int *const pDerivate,
                                   const ptrdiff_t derivateBufStride, const int width, const int height);

  static void xEqualCoeffComputer(Pel *pResidue, ptrdiff_t residueStride, int **ppDerivate, ptrdiff_t derivateBufStride,
                                  int64_t (*pEqualCoeff)[7], int width, int height, bool b6Param);

  AffineGradientSearch();
  ~AffineGradientSearch() {}

#ifdef TARGET_SIMD_X86
  void initAffineGradientSearchX86();
  template <X86_VEXT vext>
  void _initAffineGradientSearchX86();
#endif
};

//! \}

#endif
