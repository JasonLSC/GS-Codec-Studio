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

// ====================================================================================================================
// Includes
// ====================================================================================================================

#include "CommonDefX86.h"
#include "../AffineGradientSearch.h"

#ifdef TARGET_SIMD_X86

#if defined _MSC_VER
#include <tmmintrin.h>
#else
#include <immintrin.h>
#endif

static inline void sobelPadTopBottom(int32_t* dst, const ptrdiff_t dstStride, const int width, const int height)
{
  int* firstRow = dst;
  std::copy_n(firstRow + dstStride, width, firstRow);

  int* lastRow = dst + (height - 1) * dstStride;
  std::copy_n(lastRow - dstStride, width, lastRow);
}

template<X86_VEXT vext, bool EDGES>
static inline void simdHorizontalSobelFilter4(const Pel* src, const ptrdiff_t srcStride, int32_t* dst,
                                              const ptrdiff_t dstStride, const int widthOrCol, const int height)
{
  // 'widthOrCol' contains 'width' if 'EDGES' is true, and 'col' otherwise

  const __m128i s = EDGES ? _mm_setr_epi8(0, 1, 4, 5, 0, 1, 4, 5, 10, 11, 14, 15, 10, 11, 14, 15)
                          : _mm_setr_epi8(2, 3, 6, 7, 4, 5, 8, 9, 6, 7, 10, 11, 8, 9, 12, 13);
  const __m128i m = _mm_setr_epi16(-1, 1, -1, 1, -1, 1, -1, 1);

  __m128i x[4];   // source rows

  for (int k = 0; k < 2; k++)
  {
    if constexpr (EDGES)
    {
      __m128i l = _mm_loadl_epi64((const __m128i*) (src + k * srcStride));
      __m128i r = _mm_loadl_epi64((const __m128i*) (src + k * srcStride + widthOrCol - 4));
      x[k]      = _mm_unpacklo_epi64(l, r);
    }
    else
    {
      x[k] = _mm_loadu_si128((const __m128i*) (src + k * srcStride + widthOrCol - 2));
    }

    x[k] = _mm_shuffle_epi8(x[k], s);
    x[k] = _mm_madd_epi16(x[k], m);
  }

  for (int row = 1; row < height - 1; row += 2)
  {
    for (int k = 2; k < 4; k++)
    {
      if constexpr (EDGES)
      {
        __m128i l = _mm_loadl_epi64((const __m128i*) (src + (row + k - 1) * srcStride));
        __m128i r = _mm_loadl_epi64((const __m128i*) (src + (row + k - 1) * srcStride + widthOrCol - 4));
        x[k]      = _mm_unpacklo_epi64(l, r);
      }
      else
      {
        x[k] = _mm_loadu_si128((const __m128i*) (src + (row + k - 1) * srcStride + widthOrCol - 2));
      }

      x[k] = _mm_shuffle_epi8(x[k], s);
      x[k] = _mm_madd_epi16(x[k], m);
    }

    __m128i sum[3];
    for (int i = 0; i < 3; i++)
    {
      sum[i] = _mm_add_epi32(x[i], x[i + 1]);
    }

    for (int k = 0; k < 2; k++)
    {
      __m128i r = _mm_add_epi32(sum[k], sum[k + 1]);
      if constexpr (EDGES)
      {
        _mm_storel_epi64((__m128i*) (dst + (row + k) * dstStride), r);
        _mm_storel_epi64((__m128i*) (dst + (row + k) * dstStride + widthOrCol - 2), _mm_unpackhi_epi64(r, r));
      }
      else
      {
        _mm_storeu_si128((__m128i*) (dst + (row + k) * dstStride + widthOrCol), r);
      }

      x[k] = x[k + 2];
    }
  }
}

template<X86_VEXT vext> static void simdHorizontalSobelFilter(Pel* const src, const ptrdiff_t srcStride, int* const dst,
                                                              const ptrdiff_t dstStride, const int width,
                                                              const int height)
{
  CHECK(height % 2 != 0, "height must be even");
  CHECK(width % 4 != 0, "width must be a multiple of 4");

  simdHorizontalSobelFilter4<vext, true>(src, srcStride, dst, dstStride, width, height);

  for (int col = 2; col < width - 2; col += 4)
  {
    simdHorizontalSobelFilter4<vext, false>(src, srcStride, dst, dstStride, col, height);
  }

  sobelPadTopBottom(dst, dstStride, width, height);
}

template<X86_VEXT vext, bool EDGES>
static inline void simdVerticalSobelFilter4(const Pel* src, const ptrdiff_t srcStride, int32_t* dst,
                                            const ptrdiff_t dstStride, const int widthOrCol, const int height)
{
  // 'widthOrCol' contains 'width' if 'EDGES' is true, and 'col' otherwise

  const __m128i s0 = EDGES ? _mm_setr_epi8(0, 1, 2, 3, 0, 1, 2, 3, 12, 13, 14, 15, 12, 13, 14, 15)
                           : _mm_setr_epi8(2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 10, 11);
  const __m128i s1 = EDGES ? _mm_setr_epi8(2, 3, 4, 5, 2, 3, 4, 5, 10, 11, 12, 13, 10, 11, 12, 13)
                           : _mm_setr_epi8(4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 10, 11, 10, 11, 12, 13);
  const __m128i m  = _mm_set1_epi16(1);

  __m128i x[4];   // source rows

  for (int k = 0; k < 2; k++)
  {
    if constexpr (EDGES)
    {
      __m128i l = _mm_loadl_epi64((const __m128i*) (src + k * srcStride));
      __m128i r = _mm_loadl_epi64((const __m128i*) (src + k * srcStride + widthOrCol - 4));
      x[k]      = _mm_unpacklo_epi64(l, r);
    }
    else
    {
      x[k] = _mm_loadu_si128((const __m128i*) (src + k * srcStride + widthOrCol - 2));
    }

    __m128i tmp = x[k];

    tmp  = _mm_shuffle_epi8(tmp, s0);
    tmp  = _mm_madd_epi16(tmp, m);
    x[k] = _mm_shuffle_epi8(x[k], s1);
    x[k] = _mm_madd_epi16(x[k], m);
    x[k] = _mm_add_epi32(x[k], tmp);
  }

  for (int row = 1; row < height - 1; row += 2)
  {
    for (int k = 2; k < 4; k++)
    {
      if constexpr (EDGES)
      {
        __m128i l = _mm_loadl_epi64((const __m128i*) (src + (row + k - 1) * srcStride));
        __m128i r = _mm_loadl_epi64((const __m128i*) (src + (row + k - 1) * srcStride + widthOrCol - 4));
        x[k]      = _mm_unpacklo_epi64(l, r);
      }
      else
      {
        x[k] = _mm_loadu_si128((const __m128i*) (src + (row + k - 1) * srcStride + widthOrCol - 2));
      }

      __m128i tmp = x[k];

      tmp  = _mm_shuffle_epi8(tmp, s0);
      tmp  = _mm_madd_epi16(tmp, m);
      x[k] = _mm_shuffle_epi8(x[k], s1);
      x[k] = _mm_madd_epi16(x[k], m);
      x[k] = _mm_add_epi32(x[k], tmp);
    }

    for (int k = 0; k < 2; k++)
    {
      __m128i r = _mm_sub_epi32(x[k + 2], x[k]);
      if constexpr (EDGES)
      {
        _mm_storel_epi64((__m128i*) (dst + (row + k) * dstStride), r);
        _mm_storel_epi64((__m128i*) (dst + (row + k) * dstStride + widthOrCol - 2), _mm_unpackhi_epi64(r, r));
      }
      else
      {
        _mm_storeu_si128((__m128i*) (dst + (row + k) * dstStride + widthOrCol), r);
      }

      x[k] = x[k + 2];
    }
  }
}

template<X86_VEXT vext> static void simdVerticalSobelFilter(Pel* const src, const ptrdiff_t srcStride, int* const dst,
                                                            const ptrdiff_t dstStride, const int width,
                                                            const int height)
{
  CHECK(height % 2 != 0, "height must be even");
  CHECK(width % 2 != 0, "width must be even");

  simdVerticalSobelFilter4<vext, true>(src, srcStride, dst, dstStride, width, height);

  for (int col = 2; col < width - 2; col += 4)
  {
    simdVerticalSobelFilter4<vext, false>(src, srcStride, dst, dstStride, col, height);
  }

  sobelPadTopBottom(dst, dstStride, width, height);
}

static inline void calcEqualCoeff8Pxls(const __m128i x[2], const __m128i y[2], __m128i* dst)
{
  __m128i sum = _mm_setzero_si128();

  sum = _mm_add_epi64(sum, _mm_mul_epi32(x[0], y[0]));
  sum = _mm_add_epi64(sum, _mm_mul_epi32(_mm_srli_si128(x[0], 4), _mm_srli_si128(y[0], 4)));
  sum = _mm_add_epi64(sum, _mm_mul_epi32(x[1], y[1]));
  sum = _mm_add_epi64(sum, _mm_mul_epi32(_mm_srli_si128(x[1], 4), _mm_srli_si128(y[1], 4)));

  __m128i accum = _mm_loadl_epi64(dst);
  accum         = _mm_add_epi64(accum, sum);
  accum         = _mm_add_epi64(accum, _mm_shuffle_epi32(accum, _MM_SHUFFLE(1, 0, 3, 2)));
  _mm_storel_epi64(dst, accum);
}

#if USE_AVX2
static inline void calcEqualCoeff16Pxls(const __m256i x[2], const __m256i y[2], __m128i* dst)
{
  __m256i sum = _mm256_setzero_si256();

  sum = _mm256_add_epi64(sum, _mm256_mul_epi32(x[0], y[0]));
  sum = _mm256_add_epi64(sum, _mm256_mul_epi32(_mm256_srli_si256(x[0], 4), _mm256_srli_si256(y[0], 4)));
  sum = _mm256_add_epi64(sum, _mm256_mul_epi32(x[1], y[1]));
  sum = _mm256_add_epi64(sum, _mm256_mul_epi32(_mm256_srli_si256(x[1], 4), _mm256_srli_si256(y[1], 4)));

  __m128i accum = _mm_loadl_epi64(dst);
  accum         = _mm_add_epi64(accum, _mm256_castsi256_si128(sum));
  accum         = _mm_add_epi64(accum, _mm256_extracti128_si256(sum, 1));
  accum         = _mm_add_epi64(accum, _mm_shuffle_epi32(accum, _MM_SHUFFLE(1, 0, 3, 2)));
  _mm_storel_epi64(dst, accum);
}
#endif

template<X86_VEXT vext> static void simdEqualCoeffComputer(Pel* target, ptrdiff_t targetStride, int** grad,
                                                           ptrdiff_t gradStride, int64_t (*cov)[7], int width,
                                                           int height, bool b6Param)
{
  const int n = b6Param ? 6 : 4;
  CHECK(targetStride != gradStride, "stride mismatch");

  const int* gradHor = grad[0];
  const int* gradVer = grad[1];

#if USE_AVX2
  if constexpr (vext >= AVX2)
  {
    for (int j = 0; j < height; j += 2)
    {
      const __m256i mmIndxJ = _mm256_set1_epi32(j | 2);

      for (int k = 0; k < width; k += 8)
      {
        const ptrdiff_t idx1    = j * gradStride + k;
        const __m256i   mmIndxK = _mm256_inserti128_si256(_mm256_set1_epi32(k + 2), _mm_set1_epi32(k + 6), 1);

        __m256i mmC[6 + 1][2];

        for (int k = 0; k < 2; k++)
        {
          mmC[0][k] = _mm256_loadu_si256((const __m256i*) &gradHor[idx1 + k * gradStride]);
          mmC[2][k] = _mm256_loadu_si256((const __m256i*) &gradVer[idx1 + k * gradStride]);
          mmC[1][k] = _mm256_mullo_epi32(mmIndxK, mmC[0][k]);
          mmC[3][k] = _mm256_mullo_epi32(mmIndxK, mmC[2][k]);
          mmC[4][k] = _mm256_mullo_epi32(mmIndxJ, mmC[0][k]);
          mmC[5][k] = _mm256_mullo_epi32(mmIndxJ, mmC[2][k]);

          if (!b6Param)
          {
            mmC[1][k] = _mm256_add_epi32(mmC[1][k], mmC[5][k]);
            mmC[3][k] = _mm256_sub_epi32(mmC[4][k], mmC[3][k]);
          }

          // Residue
          if constexpr (sizeof(Pel) == 2)
          {
            mmC[n][k] = _mm256_cvtepi16_epi32(_mm_loadu_si128((const __m128i*) &target[idx1 + k * gradStride]));
          }
          else if constexpr (sizeof(Pel) == 4)
          {
            mmC[n][k] = _mm256_loadu_si256((const __m256i*) &target[idx1 + k * gradStride]);
          }
          mmC[n][k] = _mm256_slli_epi32(mmC[n][k], 3);
        }

        // Calculation of coefficient matrix
        for (int col = 0; col < n; col++)
        {
          for (int row = col; row <= n; row++)
          {
            calcEqualCoeff16Pxls(mmC[col], mmC[row], (__m128i*) &cov[col + 1][row]);
          }
        }
      }
    }
  }
  else
#endif
  {
    for (int j = 0; j < height; j += 2)
    {
      const __m128i mmIndxJ = _mm_set1_epi32(j | 2);

      for (int k = 0; k < width; k += 4)
      {
        const ptrdiff_t idx1    = j * gradStride + k;
        const __m128i   mmIndxK = _mm_set1_epi32(k + 2);

        __m128i mmC[6 + 1][2];

        for (int k = 0; k < 2; k++)
        {
          mmC[0][k] = _mm_loadu_si128((const __m128i*) &gradHor[idx1 + k * gradStride]);
          mmC[2][k] = _mm_loadu_si128((const __m128i*) &gradVer[idx1 + k * gradStride]);
          mmC[1][k] = _mm_mullo_epi32(mmIndxK, mmC[0][k]);
          mmC[3][k] = _mm_mullo_epi32(mmIndxK, mmC[2][k]);
          mmC[4][k] = _mm_mullo_epi32(mmIndxJ, mmC[0][k]);
          mmC[5][k] = _mm_mullo_epi32(mmIndxJ, mmC[2][k]);

          if (!b6Param)
          {
            mmC[1][k] = _mm_add_epi32(mmC[1][k], mmC[5][k]);
            mmC[3][k] = _mm_sub_epi32(mmC[4][k], mmC[3][k]);
          }

          // Residue
          if constexpr (sizeof(Pel) == 2)
          {
            mmC[n][k] = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i*) &target[idx1 + k * gradStride]));
          }
          else if constexpr (sizeof(Pel) == 4)
          {
            mmC[n][k] = _mm_loadu_si128((const __m128i*) &target[idx1 + k * gradStride]);
          }
          mmC[n][k] = _mm_slli_epi32(mmC[n][k], 3);
        }

        // Calculation of coefficient matrix
        for (int col = 0; col < n; col++)
        {
          for (int row = col; row <= n; row++)
          {
            calcEqualCoeff8Pxls(mmC[col], mmC[row], (__m128i*) &cov[col + 1][row]);
          }
        }
      }
    }
  }

  for (int col = 0; col < n; col++)
  {
    for (int row = col + 1; row < n; row++)
    {
      cov[row + 1][col] = cov[col + 1][row];
    }
  }
}

#if RExt__HIGH_BIT_DEPTH_SUPPORT
#if USE_AVX2
static inline void store6x32(int *ptr, __m256i val)
{
  _mm_storeu_si128((__m128i *)ptr, _mm256_castsi256_si128(val));
  _mm_storel_epi64((__m128i *)(ptr + 4), _mm256_extracti128_si256(val, 1));
}
#endif

template<X86_VEXT vext> static void simdHorizontalSobelFilter_HBD_SIMD(Pel* const src, const ptrdiff_t srcStride,
                                                                       int* const dst, const ptrdiff_t dstStride,
                                                                       const int width, const int height)
{
  assert(!(height % 2));
  assert(!(width % 4));

  /* Derivates of the rows and columns at the boundary are done at the end of this function */
  /* The value of col and row indicate the columns and rows for which the derivates have already been computed */

  int col = 1;
#if USE_AVX2
  if (vext >= AVX2)
  {
    for (; (col + 6) < width; col += 6)
    {
      __m256i x[4];

      x[0] = _mm256_lddqu_si256((__m256i*) &src[col - 1]);
      x[1] = _mm256_lddqu_si256((__m256i*) &src[srcStride + col - 1]);

      for (int row = 1; row < (height - 1); row += 2)
      {
        x[2] = _mm256_lddqu_si256((__m256i*) &src[(row + 1) * srcStride + col - 1]);
        x[3] = _mm256_lddqu_si256((__m256i*) &src[(row + 2) * srcStride + col - 1]);

        __m256i sum[3];
        for (int i = 0; i < 3; i++)
        {
          sum[i] = _mm256_add_epi32(x[i], x[i + 1]);
        }

        __m256i r0 = _mm256_add_epi32(sum[0], sum[1]);
        __m256i r1 = _mm256_add_epi32(sum[1], sum[2]);

        r0 = _mm256_sub_epi32(_mm256_permute4x64_epi64(r0, 0x39), r0); // 00111001
        r1 = _mm256_sub_epi32(_mm256_permute4x64_epi64(r1, 0x39), r1);

        store6x32(&dst[col + row * dstStride], r0);
        store6x32(&dst[col + (row + 1) * dstStride], r1);

        x[0] = x[2];
        x[1] = x[3];
      }
    }
  }
#endif

  for (; (col + 2) < width; col += 2)
  {
    __m128i x[4];

    x[0] = _mm_lddqu_si128((__m128i*) &src[col - 1]);
    x[1] = _mm_lddqu_si128((__m128i*) &src[srcStride + col - 1]);

    for (int row = 1; row < (height - 1); row += 2)
    {
      x[2] = _mm_lddqu_si128((__m128i*) &src[(row + 1) * srcStride + col - 1]);
      x[3] = _mm_lddqu_si128((__m128i*) &src[(row + 2) * srcStride + col - 1]);

      __m128i sum[3];
      for (int i = 0; i < 3; i++)
      {
        sum[i] = _mm_add_epi32(x[i], x[i + 1]);
      }

      __m128i r0 = _mm_add_epi32(sum[0], sum[1]);
      __m128i r1 = _mm_add_epi32(sum[1], sum[2]);

      r0 = _mm_sub_epi32(_mm_srli_si128(r0, 8), r0);
      r1 = _mm_sub_epi32(_mm_srli_si128(r1, 8), r1);

      _mm_storel_epi64((__m128i*) &dst[col + row * dstStride], r0);
      _mm_storel_epi64((__m128i*) &dst[col + (row + 1) * dstStride], r1);

      x[0] = x[2];
      x[1] = x[3];
    }
  }

  for (int j = 1; j < height - 1; j++)
  {
    dst[j * dstStride]               = dst[j * dstStride + 1];
    dst[j * dstStride + (width - 1)] = dst[j * dstStride + (width - 2)];
  }

  sobelPadTopBottom(dst, dstStride, width, height);
}

template<X86_VEXT vext> static void simdVerticalSobelFilter_HBD_SIMD(Pel* const src, const ptrdiff_t srcStride,
                                                                     int* const dst, const ptrdiff_t dstStride,
                                                                     const int width, const int height)
{
  assert(!(height % 2));
  assert(!(width % 4));

  int col = 1;
#if USE_AVX2
  if (vext >= AVX2)
  {
    const __m256i shuffle256 = _mm256_set_epi32(0, 7, 6, 5, 4, 3, 2, 1);

    for (; (col + 6) < width; col += 6)
    {
      __m256i x[4];

      x[0] = _mm256_loadu_si256((__m256i*) &src[col - 1]);
      x[1] = _mm256_loadu_si256((__m256i*) &src[srcStride + col - 1]);

      for (int row = 1; row < (height - 1); row += 2)
      {
        x[2] = _mm256_loadu_si256((__m256i*) &src[(row + 1) * srcStride + col - 1]);
        x[3] = _mm256_loadu_si256((__m256i*) &src[(row + 2) * srcStride + col - 1]);

        __m256i r0 = _mm256_sub_epi32(x[2], x[0]);
        __m256i r1 = _mm256_sub_epi32(x[3], x[1]);

        r0 = _mm256_add_epi32(r0, _mm256_permutevar8x32_epi32(r0, shuffle256));
        r1 = _mm256_add_epi32(r1, _mm256_permutevar8x32_epi32(r1, shuffle256));

        r0 = _mm256_add_epi32(r0, _mm256_permutevar8x32_epi32(r0, shuffle256));
        r1 = _mm256_add_epi32(r1, _mm256_permutevar8x32_epi32(r1, shuffle256));

        store6x32(&dst[col + row * dstStride], r0);
        store6x32(&dst[col + (row + 1) * dstStride], r1);

        x[0] = x[2];
        x[1] = x[3];
      }
    }
  }
#endif

  /* Derivates of the rows and columns at the boundary are done at the end of this function */
  /* The value of col and row indicate the columns and rows for which the derivates have already been computed */
  for (; (col + 2) < width; col += 2)
  {
    __m128i x[4];

    x[0] = _mm_loadu_si128((__m128i*) &src[col - 1]);
    x[1] = _mm_loadu_si128((__m128i*) &src[srcStride + col - 1]);

    for (int row = 1; row < height - 1; row += 2)
    {
      x[2] = _mm_loadu_si128((__m128i*) &src[(row + 1) * srcStride + col - 1]);
      x[3] = _mm_loadu_si128((__m128i*) &src[(row + 2) * srcStride + col - 1]);

      __m128i r0 = _mm_sub_epi32(x[2], x[0]);
      __m128i r1 = _mm_sub_epi32(x[3], x[1]);

      r0 = _mm_add_epi32(r0, _mm_srli_si128(r0, 4));
      r1 = _mm_add_epi32(r1, _mm_srli_si128(r1, 4));

      r0 = _mm_add_epi32(r0, _mm_srli_si128(r0, 4));
      r1 = _mm_add_epi32(r1, _mm_srli_si128(r1, 4));

      _mm_storel_epi64((__m128i*) &dst[col + row * dstStride], r0);
      _mm_storel_epi64((__m128i*) &dst[col + (row + 1) * dstStride], r1);

      x[0] = x[2];
      x[1] = x[3];
    }
  }

  for (int j = 1; j < height - 1; j++)
  {
    dst[j * dstStride]               = dst[j * dstStride + 1];
    dst[j * dstStride + (width - 1)] = dst[j * dstStride + (width - 2)];
  }

  sobelPadTopBottom(dst, dstStride, width, height);
}
#endif

template <X86_VEXT vext>
void AffineGradientSearch::_initAffineGradientSearchX86()
{
#if RExt__HIGH_BIT_DEPTH_SUPPORT
  m_HorizontalSobelFilter = simdHorizontalSobelFilter_HBD_SIMD<vext>;
  m_VerticalSobelFilter   = simdVerticalSobelFilter_HBD_SIMD<vext>;
#else
  m_HorizontalSobelFilter = simdHorizontalSobelFilter<vext>;
  m_VerticalSobelFilter   = simdVerticalSobelFilter<vext>;
#endif
  m_EqualCoeffComputer = simdEqualCoeffComputer<vext>;
}

template void AffineGradientSearch::_initAffineGradientSearchX86<SIMDX86>();

#endif //#ifdef TARGET_SIMD_X86
