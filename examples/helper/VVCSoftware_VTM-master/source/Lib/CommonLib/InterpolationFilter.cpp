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
 * \brief Implementation of InterpolationFilter class
 */

// ====================================================================================================================
// Includes
// ====================================================================================================================

#include "Rom.h"
#include "InterpolationFilter.h"
#include "ChromaFormat.h"

#if JVET_J0090_MEMORY_BANDWITH_MEASURE
CacheModel* InterpolationFilter::m_cacheModel;
#endif
//! \ingroup CommonLib
//! \{

// ====================================================================================================================
// Tables
// ====================================================================================================================
// clang-format off
const TFilterCoeff InterpolationFilter::m_affineLumaFilter[LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS][NTAPS_LUMA] =
{
  { 0,   0,  64,   0,   0, 0, 0, 0 },
  { 1,  -3,  63,   4,  -2, 1, 0, 0 },
  { 1,  -5,  62,   8,  -3, 1, 0, 0 },
  { 2,  -8,  60,  13,  -4, 1, 0, 0 },

  { 3, -10,  58,  17,  -5, 1, 0, 0 },
  { 3, -11,  52,  26,  -8, 2, 0, 0 },
  { 2,  -9,  47,  31, -10, 3, 0, 0 },
  { 3, -11,  45,  34, -10, 3, 0, 0 },

  { 3, -11,  40,  40, -11, 3, 0, 0 },
  { 3, -10,  34,  45, -11, 3, 0, 0 },
  { 3, -10,  31,  47,  -9, 2, 0, 0 },
  { 2,  -8,  26,  52, -11, 3, 0, 0 },

  { 1,  -5,  17,  58, -10, 3, 0, 0 },
  { 1,  -4,  13,  60,  -8, 2, 0, 0 },
  { 1,  -3,   8,  62,  -5, 1, 0, 0 },
  { 1,  -2,   4,  63,  -3, 1, 0, 0 }
};
// clang-format on

const TFilterCoeff InterpolationFilter::m_lumaFilter[LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS][NTAPS_LUMA] =
{
  {  0, 0,   0, 64,  0,   0,  0,  0 },
  {  0, 1,  -3, 63,  4,  -2,  1,  0 },
  { -1, 2,  -5, 62,  8,  -3,  1,  0 },
  { -1, 3,  -8, 60, 13,  -4,  1,  0 },
  { -1, 4, -10, 58, 17,  -5,  1,  0 },
  { -1, 4, -11, 52, 26,  -8,  3, -1 },
  { -1, 3,  -9, 47, 31, -10,  4, -1 },
  { -1, 4, -11, 45, 34, -10,  4, -1 },
  { -1, 4, -11, 40, 40, -11,  4, -1 },
  { -1, 4, -10, 34, 45, -11,  4, -1 },
  { -1, 4, -10, 31, 47,  -9,  3, -1 },
  { -1, 3,  -8, 26, 52, -11,  4, -1 },
  {  0, 1,  -5, 17, 58, -10,  4, -1 },
  {  0, 1,  -4, 13, 60,  -8,  3, -1 },
  {  0, 1,  -3,  8, 62,  -5,  2, -1 },
  {  0, 1,  -2,  4, 63,  -3,  1,  0 }
};

// 1.5x
const TFilterCoeff InterpolationFilter::m_lumaFilterRPR1[LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS][NTAPS_LUMA] =
{
  { -1, -5, 17, 42, 17, -5, -1,  0 },
  {  0, -5, 15, 41, 19, -5, -1,  0 },
  {  0, -5, 13, 40, 21, -4, -1,  0 },
  {  0, -5, 11, 39, 24, -4, -2,  1 },
  {  0, -5,  9, 38, 26, -3, -2,  1 },
  {  0, -5,  7, 38, 28, -2, -3,  1 },
  {  1, -5,  5, 36, 30, -1, -3,  1 },
  {  1, -4,  3, 35, 32,  0, -4,  1 },
  {  1, -4,  2, 33, 33,  2, -4,  1 },
  {  1, -4,  0, 32, 35,  3, -4,  1 },
  {  1, -3, -1, 30, 36,  5, -5,  1 },
  {  1, -3, -2, 28, 38,  7, -5,  0 },
  {  1, -2, -3, 26, 38,  9, -5,  0 },
  {  1, -2, -4, 24, 39, 11, -5,  0 },
  {  0, -1, -4, 21, 40, 13, -5,  0 },
  {  0, -1, -5, 19, 41, 15, -5,  0 }
};

// 2x
const TFilterCoeff InterpolationFilter::m_lumaFilterRPR2[LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS][NTAPS_LUMA] =
{
  { -4,  2, 20, 28, 20,  2, -4,  0 },
  { -4,  0, 19, 29, 21,  5, -4, -2 },
  { -4, -1, 18, 29, 22,  6, -4, -2 },
  { -4, -1, 16, 29, 23,  7, -4, -2 },
  { -4, -1, 16, 28, 24,  7, -4, -2 },
  { -4, -1, 14, 28, 25,  8, -4, -2 },
  { -3, -3, 14, 27, 26,  9, -3, -3 },
  { -3, -1, 12, 28, 25, 10, -4, -3 },
  { -3, -3, 11, 27, 27, 11, -3, -3 },
  { -3, -4, 10, 25, 28, 12, -1, -3 },
  { -3, -3,  9, 26, 27, 14, -3, -3 },
  { -2, -4,  8, 25, 28, 14, -1, -4 },
  { -2, -4,  7, 24, 28, 16, -1, -4 },
  { -2, -4,  7, 23, 29, 16, -1, -4 },
  { -2, -4,  6, 22, 29, 18, -1, -4 },
  { -2, -4,  5, 21, 29, 19,  0, -4 }
};

// clang-format off
// 1.5x
const TFilterCoeff InterpolationFilter::m_affineLumaFilterRPR1[LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS][NTAPS_LUMA] =
{
  {  -6, 17, 42, 17, -5, -1,  0, 0 },
  {  -5, 15, 41, 19, -5, -1,  0, 0 },
  {  -5, 13, 40, 21, -4, -1,  0, 0 },
  {  -5, 11, 39, 24, -4, -1,  0, 0 },
  {  -5,  9, 38, 26, -3, -1,  0, 0 },
  {  -5,  7, 38, 28, -2, -2,  0, 0 },
  {  -4,  5, 36, 30, -1, -2,  0, 0 },
  {  -3,  3, 35, 32,  0, -3,  0, 0 },
  {  -3,  2, 33, 33,  2, -3,  0, 0 },
  {  -3,  0, 32, 35,  3, -3,  0, 0 },
  {  -2, -1, 30, 36,  5, -4,  0, 0 },
  {  -2, -2, 28, 38,  7, -5,  0, 0 },
  {  -1, -3, 26, 38,  9, -5,  0, 0 },
  {  -1, -4, 24, 39, 11, -5,  0, 0 },
  {  -1, -4, 21, 40, 13, -5,  0, 0 },
  {  -1, -5, 19, 41, 15, -5,  0, 0 }
};

// 2x
const TFilterCoeff InterpolationFilter::m_affineLumaFilterRPR2[LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS][NTAPS_LUMA] =
{
  {  -2, 20, 28, 20,  2, -4,  0, 0 },
  {  -4, 19, 29, 21,  5, -6,  0, 0 },
  {  -5, 18, 29, 22,  6, -6,  0, 0 },
  {  -5, 16, 29, 23,  7, -6,  0, 0 },
  {  -5, 16, 28, 24,  7, -6,  0, 0 },
  {  -5, 14, 28, 25,  8, -6,  0, 0 },
  {  -6, 14, 27, 26,  9, -6,  0, 0 },
  {  -4, 12, 28, 25, 10, -7,  0, 0 },
  {  -6, 11, 27, 27, 11, -6,  0, 0 },
  {  -7, 10, 25, 28, 12, -4,  0, 0 },
  {  -6,  9, 26, 27, 14, -6,  0, 0 },
  {  -6,  8, 25, 28, 14, -5,  0, 0 },
  {  -6,  7, 24, 28, 16, -5,  0, 0 },
  {  -6,  7, 23, 29, 16, -5,  0, 0 },
  {  -6,  6, 22, 29, 18, -5,  0, 0 },
  {  -6,  5, 21, 29, 19, -4,  0, 0 }
};
// clang-format on

const TFilterCoeff InterpolationFilter::m_lumaAltHpelIFilter[NTAPS_LUMA] = {  0, 3, 9, 20, 20, 9, 3, 0 };
const TFilterCoeff InterpolationFilter::m_chromaFilter[CHROMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS][NTAPS_CHROMA] =
{
  {  0, 64,  0,  0 },
  { -1, 63,  2,  0 },
  { -2, 62,  4,  0 },
  { -2, 60,  7, -1 },
  { -2, 58, 10, -2 },
  { -3, 57, 12, -2 },
  { -4, 56, 14, -2 },
  { -4, 55, 15, -2 },
  { -4, 54, 16, -2 },
  { -5, 53, 18, -2 },
  { -6, 52, 20, -2 },
  { -6, 49, 24, -3 },
  { -6, 46, 28, -4 },
  { -5, 44, 29, -4 },
  { -4, 42, 30, -4 },
  { -4, 39, 33, -4 },
  { -4, 36, 36, -4 },
  { -4, 33, 39, -4 },
  { -4, 30, 42, -4 },
  { -4, 29, 44, -5 },
  { -4, 28, 46, -6 },
  { -3, 24, 49, -6 },
  { -2, 20, 52, -6 },
  { -2, 18, 53, -5 },
  { -2, 16, 54, -4 },
  { -2, 15, 55, -4 },
  { -2, 14, 56, -4 },
  { -2, 12, 57, -3 },
  { -2, 10, 58, -2 },
  { -1,  7, 60, -2 },
  {  0,  4, 62, -2 },
  {  0,  2, 63, -1 },
};

//1.5x
const TFilterCoeff InterpolationFilter::m_chromaFilterRPR1[CHROMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS][NTAPS_CHROMA] =
{
  { 12, 40, 12,  0 },
  { 11, 40, 13,  0 },
  { 10, 40, 15, -1 },
  {  9, 40, 16, -1 },
  {  8, 40, 17, -1 },
  {  8, 39, 18, -1 },
  {  7, 39, 19, -1 },
  {  6, 38, 21, -1 },
  {  5, 38, 22, -1 },
  {  4, 38, 23, -1 },
  {  4, 37, 24, -1 },
  {  3, 36, 25,  0 },
  {  3, 35, 26,  0 },
  {  2, 34, 28,  0 },
  {  2, 33, 29,  0 },
  {  1, 33, 30,  0 },
  {  1, 31, 31,  1 },
  {  0, 30, 33,  1 },
  {  0, 29, 33,  2 },
  {  0, 28, 34,  2 },
  {  0, 26, 35,  3 },
  {  0, 25, 36,  3 },
  { -1, 24, 37,  4 },
  { -1, 23, 38,  4 },
  { -1, 22, 38,  5 },
  { -1, 21, 38,  6 },
  { -1, 19, 39,  7 },
  { -1, 18, 39,  8 },
  { -1, 17, 40,  8 },
  { -1, 16, 40,  9 },
  { -1, 15, 40, 10 },
  {  0, 13, 40, 11 },
};

//2x
const TFilterCoeff InterpolationFilter::m_chromaFilterRPR2[CHROMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS][NTAPS_CHROMA] =
{
  { 17, 30, 17,  0 },
  { 17, 30, 18, -1 },
  { 16, 30, 18,  0 },
  { 16, 30, 18,  0 },
  { 15, 30, 18,  1 },
  { 14, 30, 18,  2 },
  { 13, 29, 19,  3 },
  { 13, 29, 19,  3 },
  { 12, 29, 20,  3 },
  { 11, 28, 21,  4 },
  { 10, 28, 22,  4 },
  { 10, 27, 22,  5 },
  {  9, 27, 23,  5 },
  {  9, 26, 24,  5 },
  {  8, 26, 24,  6 },
  {  7, 26, 25,  6 },
  {  7, 25, 25,  7 },
  {  6, 25, 26,  7 },
  {  6, 24, 26,  8 },
  {  5, 24, 26,  9 },
  {  5, 23, 27,  9 },
  {  5, 22, 27, 10 },
  {  4, 22, 28, 10 },
  {  4, 21, 28, 11 },
  {  3, 20, 29, 12 },
  {  3, 19, 29, 13 },
  {  3, 19, 29, 13 },
  {  2, 18, 30, 14 },
  {  1, 18, 30, 15 },
  {  0, 18, 30, 16 },
  {  0, 18, 30, 16 },
  { -1, 18, 30, 17 }
};

const TFilterCoeff InterpolationFilter::m_bilinearFilterPrec4[LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS][NTAPS_BILINEAR] =
{
  { 16,  0, },
  { 15,  1, },
  { 14,  2, },
  { 13, 3, },
  { 12, 4, },
  { 11, 5, },
  { 10, 6, },
  { 9, 7, },
  { 8, 8, },
  { 7, 9, },
  { 6, 10, },
  { 5, 11, },
  { 4, 12, },
  { 3, 13, },
  { 2, 14, },
  { 1, 15, }
};
// ====================================================================================================================
// Private member functions
// ====================================================================================================================

InterpolationFilter::InterpolationFilter()
{
  m_filterHor[_8_TAPS][0][0] = filter<8, false, false, false, false>;
  m_filterHor[_8_TAPS][0][1] = filter<8, false, false, true, false>;
  m_filterHor[_8_TAPS][1][0] = filter<8, false, true, false, false>;
  m_filterHor[_8_TAPS][1][1] = filter<8, false, true, true, false>;

  m_filterHor[_4_TAPS][0][0] = filter<4, false, false, false, false>;
  m_filterHor[_4_TAPS][0][1] = filter<4, false, false, true, false>;
  m_filterHor[_4_TAPS][1][0] = filter<4, false, true, false, false>;
  m_filterHor[_4_TAPS][1][1] = filter<4, false, true, true, false>;

  m_filterHor[_2_TAPS_DMVR][0][0] = filter<2, false, false, false, true>;
  m_filterHor[_2_TAPS_DMVR][0][1] = filter<2, false, false, true, true>;
  m_filterHor[_2_TAPS_DMVR][1][0] = filter<2, false, true, false, true>;
  m_filterHor[_2_TAPS_DMVR][1][1] = filter<2, false, true, true, true>;

  m_filterHor[_6_TAPS][0][0] = filter<6, false, false, false, false>;
  m_filterHor[_6_TAPS][0][1] = filter<6, false, false, true, false>;
  m_filterHor[_6_TAPS][1][0] = filter<6, false, true, false, false>;
  m_filterHor[_6_TAPS][1][1] = filter<6, false, true, true, false>;

  m_filterVer[_8_TAPS][0][0] = filter<8, true, false, false, false>;
  m_filterVer[_8_TAPS][0][1] = filter<8, true, false, true, false>;
  m_filterVer[_8_TAPS][1][0] = filter<8, true, true, false, false>;
  m_filterVer[_8_TAPS][1][1] = filter<8, true, true, true, false>;

  m_filterVer[_4_TAPS][0][0] = filter<4, true, false, false, false>;
  m_filterVer[_4_TAPS][0][1] = filter<4, true, false, true, false>;
  m_filterVer[_4_TAPS][1][0] = filter<4, true, true, false, false>;
  m_filterVer[_4_TAPS][1][1] = filter<4, true, true, true, false>;

  m_filterVer[_2_TAPS_DMVR][0][0] = filter<2, true, false, false, true>;
  m_filterVer[_2_TAPS_DMVR][0][1] = filter<2, true, false, true, true>;
  m_filterVer[_2_TAPS_DMVR][1][0] = filter<2, true, true, false, true>;
  m_filterVer[_2_TAPS_DMVR][1][1] = filter<2, true, true, true, true>;

  m_filterVer[_6_TAPS][0][0] = filter<6, true, false, false, false>;
  m_filterVer[_6_TAPS][0][1] = filter<6, true, false, true, false>;
  m_filterVer[_6_TAPS][1][0] = filter<6, true, true, false, false>;
  m_filterVer[_6_TAPS][1][1] = filter<6, true, true, true, false>;

  m_filterCopy[0][0]   = filterCopy<false, false>;
  m_filterCopy[0][1]   = filterCopy<false, true>;
  m_filterCopy[1][0]   = filterCopy<true, false>;
  m_filterCopy[1][1]   = filterCopy<true, true>;

  m_weightedGeoBlk = xWeightedGeoBlk;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// !!! NOTE !!!
//
//  This is the scalar version of the function.
//  If you change the functionality here, consider to switch off the SIMD implementation of this function.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<bool isFirst, bool isLast>
void InterpolationFilter::filterCopy(const ClpRng &clpRng, const Pel *src, const ptrdiff_t srcStride, Pel *dst,
                                     const ptrdiff_t dstStride, int width, int height, bool biMCForDMVR)
{
  int row, col;

  if ( isFirst == isLast )
  {
    for (row = 0; row < height; row++)
    {
      for (col = 0; col < width; col++)
      {
        dst[col] = src[col];
        JVET_J0090_CACHE_ACCESS( &src[col], __FILE__, __LINE__ );
      }

      src += srcStride;
      dst += dstStride;
    }
  }
  else if ( isFirst )
  {
    const int shift = IF_INTERNAL_FRAC_BITS(clpRng.bd);

    if (biMCForDMVR)
    {
      int shift10BitOut, offset;
      if ((clpRng.bd - IF_INTERNAL_PREC_BILINEAR) > 0)
      {
        shift10BitOut = (clpRng.bd - IF_INTERNAL_PREC_BILINEAR);
        offset = (1 << (shift10BitOut - 1));
        for (row = 0; row < height; row++)
        {
          for (col = 0; col < width; col++)
          {
            dst[col] = (src[col] + offset) >> shift10BitOut;
          }
          src += srcStride;
          dst += dstStride;
        }
      }
      else
      {
        shift10BitOut = (IF_INTERNAL_PREC_BILINEAR - clpRng.bd);
        for (row = 0; row < height; row++)
        {
          for (col = 0; col < width; col++)
          {
            dst[col] = src[col] << shift10BitOut;
          }
          src += srcStride;
          dst += dstStride;
        }
      }
    }
    else
    for (row = 0; row < height; row++)
    {
      for (col = 0; col < width; col++)
      {
        Pel val = leftShift_round(src[col], shift);
        dst[col] = val - (Pel)IF_INTERNAL_OFFS;
        JVET_J0090_CACHE_ACCESS( &src[col], __FILE__, __LINE__ );
      }

      src += srcStride;
      dst += dstStride;
    }
  }
  else
  {
    const int shift = IF_INTERNAL_FRAC_BITS(clpRng.bd);

    if (biMCForDMVR)
    {
      int shift10BitOut, offset;
      if ((clpRng.bd - IF_INTERNAL_PREC_BILINEAR) > 0)
      {
        shift10BitOut = (clpRng.bd - IF_INTERNAL_PREC_BILINEAR);
        offset = (1 << (shift10BitOut - 1));
        for (row = 0; row < height; row++)
        {
          for (col = 0; col < width; col++)
          {
            dst[col] = (src[col] + offset) >> shift10BitOut;
          }
          src += srcStride;
          dst += dstStride;
        }
      }
      else
      {
        shift10BitOut = (IF_INTERNAL_PREC_BILINEAR - clpRng.bd);
        for (row = 0; row < height; row++)
        {
          for (col = 0; col < width; col++)
          {
            dst[col] = src[col] << shift10BitOut;
          }
          src += srcStride;
          dst += dstStride;
        }
      }
    }
    else
    {
      for (row = 0; row < height; row++)
      {
        for (col = 0; col < width; col++)
        {
          Pel val = src[col];
          val     = rightShift_round((val + IF_INTERNAL_OFFS), shift);

          dst[col] = ClipPel(val, clpRng);
          JVET_J0090_CACHE_ACCESS(&src[col], __FILE__, __LINE__);
        }

        src += srcStride;
        dst += dstStride;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// !!! NOTE !!!
//
//  This is the scalar version of the function.
//  If you change the functionality here, consider to switch off the SIMD implementation of this function.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<int N, bool isVertical, bool isFirst, bool isLast, bool biMCForDMVR>
void InterpolationFilter::filter(const ClpRng &clpRng, Pel const *src, const ptrdiff_t srcStride, Pel *dst,
                                 const ptrdiff_t dstStride, int width, int height, TFilterCoeff const *coeff)
{
  int row, col;

  Pel c[8];
  c[0] = coeff[0];
  c[1] = coeff[1];
  if ( N >= 4 )
  {
    c[2] = coeff[2];
    c[3] = coeff[3];
  }
  if ( N >= 6 )
  {
    c[4] = coeff[4];
    c[5] = coeff[5];
  }
  if ( N == 8 )
  {
    c[6] = coeff[6];
    c[7] = coeff[7];
  }

  const ptrdiff_t cStride = (isVertical) ? srcStride : 1;
  src -= ( N/2 - 1 ) * cStride;

  int offset;
  int headRoom = IF_INTERNAL_FRAC_BITS(clpRng.bd);
  int shift    = IF_FILTER_PREC;
  // with the current settings (IF_INTERNAL_PREC = 14 and IF_FILTER_PREC = 6), though headroom can be
  // negative for bit depths greater than 14, shift will remain non-negative for bit depths of 8->20
  CHECK(shift < 0, "Negative shift");

  if ( isLast )
  {
    shift += (isFirst) ? 0 : headRoom;
    offset = 1 << (shift - 1);
    offset += (isFirst) ? 0 : IF_INTERNAL_OFFS << IF_FILTER_PREC;
  }
  else
  {
    shift -= (isFirst) ? headRoom : 0;
    offset = (isFirst) ? -IF_INTERNAL_OFFS << shift : 0;
  }

  if (biMCForDMVR)
  {
    if( isFirst )
    {
      shift = IF_FILTER_PREC_BILINEAR - (IF_INTERNAL_PREC_BILINEAR - clpRng.bd);
      offset = 1 << (shift - 1);
    }
    else
    {
      shift = 4;
      offset = 1 << (shift - 1);
    }
  }
  for (row = 0; row < height; row++)
  {
    for (col = 0; col < width; col++)
    {
      int sum;

      sum  = src[ col + 0 * cStride] * c[0];
      sum += src[ col + 1 * cStride] * c[1];
      JVET_J0090_CACHE_ACCESS( &src[ col + 0 * cStride], __FILE__, __LINE__ );
      JVET_J0090_CACHE_ACCESS( &src[ col + 1 * cStride], __FILE__, __LINE__ );
      if ( N >= 4 )
      {
        sum += src[ col + 2 * cStride] * c[2];
        sum += src[ col + 3 * cStride] * c[3];
        JVET_J0090_CACHE_ACCESS( &src[ col + 2 * cStride], __FILE__, __LINE__ );
        JVET_J0090_CACHE_ACCESS( &src[ col + 3 * cStride], __FILE__, __LINE__ );
      }
      if ( N >= 6 )
      {
        sum += src[ col + 4 * cStride] * c[4];
        sum += src[ col + 5 * cStride] * c[5];
        JVET_J0090_CACHE_ACCESS( &src[ col + 4 * cStride], __FILE__, __LINE__ );
        JVET_J0090_CACHE_ACCESS( &src[ col + 5 * cStride], __FILE__, __LINE__ );
      }
      if ( N == 8 )
      {
        sum += src[ col + 6 * cStride] * c[6];
        sum += src[ col + 7 * cStride] * c[7];
        JVET_J0090_CACHE_ACCESS( &src[ col + 6 * cStride], __FILE__, __LINE__ );
        JVET_J0090_CACHE_ACCESS( &src[ col + 7 * cStride], __FILE__, __LINE__ );
      }

      Pel val = ( sum + offset ) >> shift;
      if ( isLast )
      {
        val = ClipPel( val, clpRng );
      }
      dst[col] = val;
    }

    src += srcStride;
    dst += dstStride;
  }
}

template<int N, bool biMCForDMVR>
void InterpolationFilter::filterHor(const ClpRng &clpRng, Pel const *src, const ptrdiff_t srcStride, Pel *dst,
                                    const ptrdiff_t dstStride, int width, int height, bool isLast,
                                    TFilterCoeff const *coeff)
{
  constexpr int IDX = tapToIdx(N, biMCForDMVR);
  static_assert(IDX < NUM_TAP_MODES, "Unsupported tap count");
  m_filterHor[IDX][1][isLast](clpRng, src, srcStride, dst, dstStride, width, height, coeff);
}

template<int N, bool biMCForDMVR>
void InterpolationFilter::filterVer(const ClpRng &clpRng, Pel const *src, const ptrdiff_t srcStride, Pel *dst,
                                    const ptrdiff_t dstStride, int width, int height, bool isFirst, bool isLast,
                                    TFilterCoeff const *coeff)
{
  constexpr int IDX = tapToIdx(N, biMCForDMVR);
  static_assert(IDX < NUM_TAP_MODES, "Unsupported tap count");
  m_filterVer[IDX][isFirst][isLast](clpRng, src, srcStride, dst, dstStride, width, height, coeff);
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

void InterpolationFilter::filterHor(const ComponentID compID, Pel const *src, const ptrdiff_t srcStride, Pel *dst,
                                    const ptrdiff_t dstStride, int width, int height, int frac, bool isLast,
                                    const ClpRng &clpRng, Filter nFilterIdx)
{
  if (frac == 0 && nFilterIdx <= Filter::AFFINE)
  {
    m_filterCopy[true][isLast](clpRng, src, srcStride, dst, dstStride, width, height, nFilterIdx == Filter::DMVR);
  }
  else if( isLuma( compID ) )
  {
    CHECK( frac < 0 || frac >= LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS, "Invalid fraction" );
    if (nFilterIdx == Filter::DMVR)
    {
      filterHor<NTAPS_BILINEAR, true>(clpRng, src, srcStride, dst, dstStride, width, height, isLast,
                                      m_bilinearFilterPrec4[frac]);
    }
    else if (nFilterIdx == Filter::AFFINE)
    {
      filterHor<NTAPS_LUMA_AFFINE, false>(clpRng, src, srcStride, dst, dstStride, width, height, isLast,
                                          m_affineLumaFilter[frac]);
    }
    else if (nFilterIdx == Filter::RPR1)
    {
      filterHor<NTAPS_LUMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isLast,
                                   m_lumaFilterRPR1[frac]);
    }
    else if (nFilterIdx == Filter::RPR2)
    {
      filterHor<NTAPS_LUMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isLast,
                                   m_lumaFilterRPR2[frac]);
    }
    else if (nFilterIdx == Filter::AFFINE_RPR1)
    {
      filterHor<NTAPS_LUMA_AFFINE, false>(clpRng, src, srcStride, dst, dstStride, width, height, isLast,
                                          m_affineLumaFilterRPR1[frac]);
    }
    else if (nFilterIdx == Filter::AFFINE_RPR2)
    {
      filterHor<NTAPS_LUMA_AFFINE, false>(clpRng, src, srcStride, dst, dstStride, width, height, isLast,
                                          m_affineLumaFilterRPR2[frac]);
    }
    else if (frac == LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS / 2 && nFilterIdx == Filter::HALFPEL_ALT)
    {
      filterHor<NTAPS_LUMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isLast, m_lumaAltHpelIFilter);
    }
    else
    {
      filterHor<NTAPS_LUMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isLast, m_lumaFilter[frac]);
    }
  }
  else
  {
    CHECK(frac < 0 || frac >= CHROMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS, "Invalid fraction");
    if (nFilterIdx == Filter::RPR1)
    {
      filterHor<NTAPS_CHROMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isLast,
                                     m_chromaFilterRPR1[frac]);
    }
    else if (nFilterIdx == Filter::RPR2)
    {
      filterHor<NTAPS_CHROMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isLast,
                                     m_chromaFilterRPR2[frac]);
    }
    else
    {
      filterHor<NTAPS_CHROMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isLast,
                                     m_chromaFilter[frac]);
    }
  }
}

void InterpolationFilter::filterVer(const ComponentID compID, Pel const *src, const ptrdiff_t srcStride, Pel *dst,
                                    const ptrdiff_t dstStride, int width, int height, int frac, bool isFirst,
                                    bool isLast, const ClpRng &clpRng, Filter nFilterIdx)
{
  if (frac == 0 && nFilterIdx <= Filter::AFFINE)
  {
    m_filterCopy[isFirst][isLast](clpRng, src, srcStride, dst, dstStride, width, height, nFilterIdx == Filter::DMVR);
  }
  else if( isLuma( compID ) )
  {
    CHECK( frac < 0 || frac >= LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS, "Invalid fraction" );
    if (nFilterIdx == Filter::DMVR)
    {
      filterVer<NTAPS_BILINEAR, true>(clpRng, src, srcStride, dst, dstStride, width, height, isFirst, isLast,
                                      m_bilinearFilterPrec4[frac]);
    }
    else if (nFilterIdx == Filter::AFFINE)
    {
      filterVer<NTAPS_LUMA_AFFINE, false>(clpRng, src, srcStride, dst, dstStride, width, height, isFirst, isLast,
                                          m_affineLumaFilter[frac]);
    }
    else if (nFilterIdx == Filter::RPR1)
    {
      filterVer<NTAPS_LUMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isFirst, isLast,
                                   m_lumaFilterRPR1[frac]);
    }
    else if (nFilterIdx == Filter::RPR2)
    {
      filterVer<NTAPS_LUMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isFirst, isLast,
                                   m_lumaFilterRPR2[frac]);
    }
    else if (nFilterIdx == Filter::AFFINE_RPR1)
    {
      filterVer<NTAPS_LUMA_AFFINE, false>(clpRng, src, srcStride, dst, dstStride, width, height, isFirst, isLast,
                                          m_affineLumaFilterRPR1[frac]);
    }
    else if (nFilterIdx == Filter::AFFINE_RPR2)
    {
      filterVer<NTAPS_LUMA_AFFINE, false>(clpRng, src, srcStride, dst, dstStride, width, height, isFirst, isLast,
                                          m_affineLumaFilterRPR2[frac]);
    }
    else if (frac == 8 && nFilterIdx == Filter::HALFPEL_ALT)
    {
      filterVer<NTAPS_LUMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isFirst, isLast,
                                   m_lumaAltHpelIFilter);
    }
    else
    {
      filterVer<NTAPS_LUMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isFirst, isLast,
                                   m_lumaFilter[frac]);
    }
  }
  else
  {
    CHECK(frac < 0 || frac >= CHROMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS, "Invalid fraction");
    if (nFilterIdx == Filter::RPR1)
    {
      filterVer<NTAPS_CHROMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isFirst, isLast,
                                     m_chromaFilterRPR1[frac]);
    }
    else if (nFilterIdx == Filter::RPR2)
    {
      filterVer<NTAPS_CHROMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isFirst, isLast,
                                     m_chromaFilterRPR2[frac]);
    }
    else
    {
      filterVer<NTAPS_CHROMA, false>(clpRng, src, srcStride, dst, dstStride, width, height, isFirst, isLast,
                                     m_chromaFilter[frac]);
    }
  }
}

void InterpolationFilter::weightedGeoBlk(const PredictionUnit &pu, const uint32_t width, const uint32_t height, const ComponentID compIdx, const uint8_t splitDir, PelUnitBuf& predDst, PelUnitBuf& predSrc0, PelUnitBuf& predSrc1)
{
  m_weightedGeoBlk(pu, width, height, compIdx, splitDir, predDst, predSrc0, predSrc1);
}

void InterpolationFilter::xWeightedGeoBlk(const PredictionUnit &pu, const uint32_t width, const uint32_t height, const ComponentID compIdx, const uint8_t splitDir, PelUnitBuf& predDst, PelUnitBuf& predSrc0, PelUnitBuf& predSrc1)
{
  Pel*    dst = predDst.get(compIdx).buf;
  Pel*    src0 = predSrc0.get(compIdx).buf;
  Pel*    src1 = predSrc1.get(compIdx).buf;
  ptrdiff_t strideDst  = predDst.get(compIdx).stride - width;
  ptrdiff_t strideSrc0 = predSrc0.get(compIdx).stride - width;
  ptrdiff_t strideSrc1 = predSrc1.get(compIdx).stride - width;

  const char    log2WeightBase = 3;
  const ClpRng  clipRng = pu.cu->slice->clpRngs().comp[compIdx];
  const int32_t clipbd = clipRng.bd;
  const int32_t shiftWeighted = IF_INTERNAL_FRAC_BITS(clipbd) + log2WeightBase;
  const int32_t offsetWeighted = (1 << (shiftWeighted - 1)) + (IF_INTERNAL_OFFS << log2WeightBase);
  const uint32_t scaleX = getComponentScaleX(compIdx, pu.chromaFormat);
  const uint32_t scaleY = getComponentScaleY(compIdx, pu.chromaFormat);

  const int angle = g_geoParams[splitDir].angleIdx;

  int16_t wIdx = floorLog2(pu.lwidth()) - GEO_MIN_CU_LOG2;
  int16_t hIdx = floorLog2(pu.lheight()) - GEO_MIN_CU_LOG2;
  int16_t stepX = 1 << scaleX;
  int16_t stepY = 0;
  int16_t* weight = nullptr;

  const int16_t *wOffset = g_weightOffset[splitDir][hIdx][wIdx];

  if (g_angle2mirror[angle] == 2)
  {
    stepY = -(int)((GEO_WEIGHT_MASK_SIZE << scaleY) + pu.lwidth());
    weight = &g_globalGeoWeights[g_angle2mask[angle]]
                                [(GEO_WEIGHT_MASK_SIZE - 1 - wOffset[1]) * GEO_WEIGHT_MASK_SIZE + wOffset[0]];
  }
  else if (g_angle2mirror[angle] == 1)
  {
    stepX = -1 << scaleX;
    stepY = (GEO_WEIGHT_MASK_SIZE << scaleY) + pu.lwidth();
    weight = &g_globalGeoWeights[g_angle2mask[angle]]
                                [wOffset[1] * GEO_WEIGHT_MASK_SIZE + (GEO_WEIGHT_MASK_SIZE - 1 - wOffset[0])];
  }
  else
  {
    stepY = (GEO_WEIGHT_MASK_SIZE << scaleY) - pu.lwidth();
    weight = &g_globalGeoWeights[g_angle2mask[angle]][wOffset[1] * GEO_WEIGHT_MASK_SIZE + wOffset[0]];
  }
  for( int y = 0; y < height; y++ )
  {
    for( int x = 0; x < width; x++ )
    {
      *dst++  = ClipPel(rightShift((*weight*(*src0++) + ((8 - *weight) * (*src1++)) + offsetWeighted), shiftWeighted), clipRng);
      weight += stepX;
    }
    dst    += strideDst;
    src0   += strideSrc0;
    src1   += strideSrc1;
    weight += stepY;
  }
}

void InterpolationFilter::initInterpolationFilter( bool enable )
{
#if ENABLE_SIMD_OPT_MCIF
#ifdef TARGET_SIMD_X86
  if ( enable )
  {
    initInterpolationFilterX86();
  }
#endif
#endif
}

//! \}
