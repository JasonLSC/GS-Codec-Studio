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

#include "../CommonDef.h"

#ifdef TARGET_SIMD_X86

#include <immintrin.h>
#include "CommonDefX86.h"
#include "../Rom.h"
#include "../TrQuant.h"

namespace SIMD::X86::TX
{
#if !RExt__HIGH_BIT_DEPTH_SUPPORT
// Number of elements in a SIMD register
static constexpr size_t NUM_ELEMENTS = sizeof(__m128i) / sizeof(TCoeff);
// Transform size in units of SIMD register
static constexpr size_t N(size_t TX_SIZE) { return TX_SIZE / NUM_ELEMENTS; }

// Reverse order of coefficients in SIMD register
static inline __m128i reverse32(__m128i x) { return _mm_shuffle_epi32(x, _MM_SHUFFLE(0, 1, 2, 3)); }

// Load coefficients into SIMD register
static inline __m128i loadCoeff(const TCoeff* p) { return _mm_loadu_si128((const __m128i*) p); }

// Load matrix coefficients into SIMD register and widen
static inline __m128i loadMatrixCoeff(const TMatrixCoeff* p)
{
  return _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i*) p));
}

// Store coefficients into SIMD register
static inline void storeCoeff(const TCoeff* p, __m128i c) { _mm_storeu_si128((__m128i*) p, c); }

#if USE_AVX2
// Load matrix coefficients into SIMD register and widen
static inline __m256i loadMatrixCoeff2(const TMatrixCoeff* p)
{
  return _mm256_cvtepi16_epi32(_mm_loadu_si128((const __m128i*) p));
}

// Store coefficients into SIMD register
static inline void storeCoeff(const TCoeff* p, __m256i c) { _mm256_storeu_si256((__m256i*) p, c); }
#endif

//---------------------------------------------------------------------------------------------------------------------

namespace Fwd   // Forward transform functions
{
static constexpr size_t STEP = 4;

template<size_t TX_SIZE>
static inline void butterfly1(__m128i even[N(TX_SIZE)], __m128i odd[N(TX_SIZE)], const TCoeff* src)
{
  constexpr size_t M = N(TX_SIZE) / 2;

  for (int k = 0; k < M; k++)
  {
    const __m128i a = loadCoeff(src + NUM_ELEMENTS * k);
    const __m128i b = reverse32(loadCoeff(src + NUM_ELEMENTS * (2 * M - 1 - k)));
    even[k]         = _mm_add_epi32(a, b);
    odd[M + k]      = _mm_sub_epi32(a, b);
  }
}

template<size_t TX_SIZE, size_t D> static inline void butterfly(__m128i even[N(TX_SIZE)], __m128i odd[N(TX_SIZE)])
{
  constexpr size_t M = N(TX_SIZE) / D;

  if constexpr (M > 0)
  {
    for (size_t k = 0; k < M; k++)
    {
      __m128i a   = even[k];
      __m128i b   = reverse32(even[2 * M - 1 - k]);
      even[k]     = _mm_add_epi32(a, b);
      odd[M + k]  = _mm_sub_epi32(a, b);
    }
  }
}

template<size_t TX_SIZE, size_t D, bool FIRST>
static inline void mul(const TMatrixCoeff m[TX_SIZE][TX_SIZE], const __m128i* src, __m128i dst[TX_SIZE * STEP],
                       size_t numActiveRowsOut, size_t i, size_t numBatchRowsIn)
{
  constexpr size_t M = N(TX_SIZE) / D;

  if constexpr (M > 0)
  {
#if USE_AVX2
    if constexpr (M % 2 == 0)
    {
      for (size_t k = FIRST ? 0 : D / 2; k < numActiveRowsOut; k += D)
      {
        __m256i sum = _mm256_setzero_si256();

        for (size_t l = 0; l < M / 2; l++)
        {
          __m256i c = loadMatrixCoeff2(&m[k][2 * NUM_ELEMENTS * l]);
          __m256i x = _mm256_loadu_si256((const __m256i*) &src[(FIRST ? 0 : M) + 2 * l]);
          sum       = _mm256_add_epi32(sum, _mm256_mullo_epi32(c, x));
        }

        dst[k * numBatchRowsIn + i] = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
      }

      return;
    }
#endif
    for (size_t k = FIRST ? 0 : D / 2; k < numActiveRowsOut; k += D)
    {
      __m128i sum = _mm_setzero_si128();

      for (size_t l = 0; l < M; l++)
      {
        __m128i c = loadMatrixCoeff(&m[k][NUM_ELEMENTS * l]);
        sum       = _mm_add_epi32(sum, _mm_mullo_epi32(c, src[(FIRST ? 0 : M) + l]));
      }

      dst[k * numBatchRowsIn + i] = sum;
    }
  }
}

template<size_t TX_SIZE> static inline void store(const __m128i tmp[TX_SIZE * STEP], TCoeff* dst, ptrdiff_t dstStride,
                                                  size_t numBatchRowsIn, TCoeff add, int shift, size_t numActiveRowsOut)
{
  const int log2numBatchRowsIn = std::min((int) numBatchRowsIn - 1, 2);

  const size_t increment = 4 >> log2numBatchRowsIn;

  for (size_t k = 0; k < numActiveRowsOut << log2numBatchRowsIn >> 2; k++)
  {
    const __m128i* x     = tmp + STEP * k;
    const __m128i  x01   = _mm_add_epi32(_mm_unpacklo_epi32(x[0], x[1]), _mm_unpackhi_epi32(x[0], x[1]));
    const __m128i  x23   = _mm_add_epi32(_mm_unpacklo_epi32(x[2], x[3]), _mm_unpackhi_epi32(x[2], x[3]));
    const __m128i  x0123 = _mm_add_epi32(_mm_unpacklo_epi64(x01, x23), _mm_unpackhi_epi64(x01, x23));

    const __m128i y = _mm_sra_epi32(_mm_add_epi32(x0123, _mm_set1_epi32(add)), _mm_cvtsi32_si128(shift));

    storeCoeff(dst + k * increment * dstStride, y);
  }
}

template<size_t TX_SIZE> static inline void clear(size_t numRowsIn, size_t numActiveRowsIn, size_t numActiveRowsOut,
                                                  TCoeff* dst, ptrdiff_t dstStride)
{
  if (numRowsIn > numActiveRowsIn)
  {
    for (size_t j = 0; j < numActiveRowsOut; j++)
    {
      for (size_t k = numActiveRowsIn; k < numRowsIn; k += NUM_ELEMENTS)
      {
        storeCoeff(dst + j * dstStride + k, _mm_setzero_si128());
      }
    }
  }

  if (numActiveRowsOut < TX_SIZE)
  {
    for (size_t j = numActiveRowsOut * dstStride; j < TX_SIZE * dstStride; j += NUM_ELEMENTS)
    {
      storeCoeff(dst + j, _mm_setzero_si128());
    }
  }
}

template<X86_VEXT vext, size_t TX_SIZE, const TMatrixCoeff M[TRANSFORM_NUMBER_OF_DIRECTIONS][TX_SIZE][TX_SIZE]>
static void dct2(const TCoeff* src, TCoeff* dst, int shift, int numRowsIn, int numZeroTrailRowsIn,
                 int numZeroTrailRowsOut)
{
  static_assert(sizeof(TCoeff) == 4);
  static_assert(sizeof(TMatrixCoeff) == 2);
  CHECK(numZeroTrailRowsIn & 3, "numZeroTrailRowsIn should be a multiple of 4");
  CHECK((numRowsIn & 3) == 3, "numRowsIn mod 4 should not be 3");

  const TCoeff add = 1 << shift >> 1;

  const size_t    numActiveRowsIn  = numRowsIn - numZeroTrailRowsIn;
  const ptrdiff_t dstStride        = numRowsIn;
  const size_t    numActiveRowsOut = TX_SIZE - numZeroTrailRowsOut;

  for (size_t j = 0; j < numActiveRowsIn; j += STEP)
  {
    __m128i tmp[TX_SIZE * STEP];

    const size_t numBatchRowsIn = std::min(numActiveRowsIn - j, STEP);

    for (size_t i = 0; i < numBatchRowsIn; i++)
    {
      static_assert(N(TX_SIZE) > 1);   // minimum size of butterfly to apply

      __m128i even[N(TX_SIZE) / 2], odd[N(TX_SIZE)];

      butterfly1<TX_SIZE>(even, odd, src);
      butterfly<TX_SIZE, 4>(even, odd);
      butterfly<TX_SIZE, 8>(even, odd);
      butterfly<TX_SIZE, 16>(even, odd);

      mul<TX_SIZE, N(TX_SIZE), true>(M[TRANSFORM_FORWARD], even, tmp, numActiveRowsOut, i, numBatchRowsIn);
      mul<TX_SIZE, 16, false>(M[TRANSFORM_FORWARD], odd, tmp, numActiveRowsOut, i, numBatchRowsIn);
      mul<TX_SIZE, 8, false>(M[TRANSFORM_FORWARD], odd, tmp, numActiveRowsOut, i, numBatchRowsIn);
      mul<TX_SIZE, 4, false>(M[TRANSFORM_FORWARD], odd, tmp, numActiveRowsOut, i, numBatchRowsIn);
      mul<TX_SIZE, 2, false>(M[TRANSFORM_FORWARD], odd, tmp, numActiveRowsOut, i, numBatchRowsIn);

      src += TX_SIZE;
    }

    store<TX_SIZE>(tmp, dst + j, dstStride, numBatchRowsIn, add, shift, numActiveRowsOut);
  }

  clear<TX_SIZE>(numRowsIn, numActiveRowsIn, numActiveRowsOut, dst, dstStride);
}

template<X86_VEXT vext, size_t TX_SIZE>
static void matrixMultCore(const TCoeff* src, TCoeff* dst, int shift, int numRowsIn, int numZeroTrailRowsIn,
                           int numZeroTrailRowsOut, const TMatrixCoeff m[TX_SIZE][TX_SIZE])
{
  static_assert(sizeof(TCoeff) == 4);
  static_assert(sizeof(TMatrixCoeff) == 2);
  CHECK(numZeroTrailRowsIn & 3, "numZeroTrailRowsIn should be a multiple of 4");
  CHECK((numRowsIn & 3) == 3, "numRowsIn mod 4 should not be 3");

  const TCoeff add = 1 << shift >> 1;

  const size_t    numActiveRowsIn  = numRowsIn - numZeroTrailRowsIn;
  const size_t    numActiveRowsOut = TX_SIZE - numZeroTrailRowsOut;
  const ptrdiff_t dstStride        = numRowsIn;

  for (size_t j = 0; j < numActiveRowsIn; j += STEP)
  {
    const size_t numBatchRowsIn = std::min(numActiveRowsIn - j, STEP);

    __m128i tmp[TX_SIZE * STEP];

    for (size_t i = 0; i < numBatchRowsIn; i++)
    {
      mul<TX_SIZE, 1, true>(m, (__m128i*) src, tmp, numActiveRowsOut, i, numBatchRowsIn);

      src += TX_SIZE;
    }

    store<TX_SIZE>(tmp, dst + j, dstStride, numBatchRowsIn, add, shift, numActiveRowsOut);
  }

  clear<TX_SIZE>(numRowsIn, numActiveRowsIn, numActiveRowsOut, dst, dstStride);
}

template<X86_VEXT vext, size_t TX_SIZE, const TMatrixCoeff M[TRANSFORM_NUMBER_OF_DIRECTIONS][TX_SIZE][TX_SIZE]>
static void matrixMult(const TCoeff* src, TCoeff* dst, int shift, int numRowsIn, int numZeroTrailRowsIn,
                       int numZeroTrailRowsOut)
{
  matrixMultCore<vext, TX_SIZE>(src, dst, shift, numRowsIn, numZeroTrailRowsIn, numZeroTrailRowsOut,
                                M[TRANSFORM_FORWARD]);
}

}   // namespace Fwd

//---------------------------------------------------------------------------------------------------------------------

namespace Inv   // Inverse transform functions
{
template<size_t TX_SIZE, size_t FIRST, size_t D>
static inline void mul(__m128i dst[N(TX_SIZE)], const TCoeff* src, const ptrdiff_t srcStride,
                       const size_t numActiveRowsIn, const TMatrixCoeff m[TX_SIZE][TX_SIZE])
{
  constexpr size_t M = N(TX_SIZE) / D;

  if constexpr (M > 0)
  {
#if USE_AVX2
    if constexpr (M % 2 == 0)
    {
      for (size_t j = 0; j < M / 2; j++)
      {
        __m256i sum = _mm256_setzero_si256();
        for (size_t k = FIRST; k < numActiveRowsIn; k += D)
        {
          __m256i x = _mm256_set1_epi32(src[k * srcStride]);
          sum       = _mm256_add_epi32(sum, _mm256_mullo_epi32(x, loadMatrixCoeff2(&m[k][j * 2 * NUM_ELEMENTS])));
        }
        storeCoeff((TCoeff*) &dst[(FIRST == 0 ? 0 : M) + 2 * j], sum);
      }

      return;
    }
#endif
    for (size_t j = 0; j < M; j++)
    {
      __m128i sum = _mm_setzero_si128();
      for (size_t k = FIRST; k < numActiveRowsIn; k += D)
      {
        __m128i x = _mm_set1_epi32(src[k * srcStride]);
        sum       = _mm_add_epi32(sum, _mm_mullo_epi32(x, loadMatrixCoeff(&m[k][j * NUM_ELEMENTS])));
      }
      dst[(FIRST == 0 ? 0 : M) + j] = sum;
    }
  }
}

template<size_t TX_SIZE, size_t D> static inline void butterfly(__m128i even[N(TX_SIZE)], const __m128i odd[N(TX_SIZE)])
{
  constexpr size_t M = N(TX_SIZE) / D;

  if constexpr (M > 0)
  {
    // values even[0..M] and odd[M..2M] are combined into even[0..2M]
    for (size_t j = 0; j < M; j++)
    {
      const __m128i a     = even[j];
      const __m128i b     = odd[M + j];
      even[j]             = _mm_add_epi32(a, b);
      even[2 * M - j - 1] = reverse32(_mm_sub_epi32(a, b));
    }
  }
}

template<size_t TX_SIZE> static inline void store(const __m128i src[N(TX_SIZE)], TCoeff* dst, const TCoeff add,
                                                  const int shift, const TCoeff minOutVal, const TCoeff maxOutVal)
{
#if USE_AVX2
  if constexpr (N(TX_SIZE) > 1)
  {
    for (size_t j = 0; j < N(TX_SIZE) / 2; j++)
    {
      __m256i sum = _mm256_add_epi32(_mm256_loadu_si256((const __m256i*) &src[2 * j]), _mm256_set1_epi32(add));
      sum         = _mm256_sra_epi32(sum, _mm_cvtsi32_si128(shift));
      sum         = _mm256_min_epi32(sum, _mm256_set1_epi32(maxOutVal));
      sum         = _mm256_max_epi32(sum, _mm256_set1_epi32(minOutVal));

      storeCoeff(dst + 2 * NUM_ELEMENTS * j, sum);
    }

    return;
  }
#endif
  for (size_t j = 0; j < N(TX_SIZE); j++)
  {
    __m128i sum = _mm_add_epi32(src[j], _mm_set1_epi32(add));
    sum         = _mm_sra_epi32(sum, _mm_cvtsi32_si128(shift));
    sum         = _mm_min_epi32(sum, _mm_set1_epi32(maxOutVal));
    sum         = _mm_max_epi32(sum, _mm_set1_epi32(minOutVal));

    storeCoeff(dst + NUM_ELEMENTS * j, sum);
  }
}

template<size_t TX_SIZE> static inline void clear(TCoeff* dst, const size_t numActiveRowsOut, const size_t numRowsOut)
{
  for (size_t j = numActiveRowsOut * TX_SIZE; j < numRowsOut * TX_SIZE; j += NUM_ELEMENTS)
  {
    storeCoeff(dst + j, _mm_setzero_si128());
  }
}

template<X86_VEXT vext, size_t TX_SIZE, const TMatrixCoeff m[TRANSFORM_NUMBER_OF_DIRECTIONS][TX_SIZE][TX_SIZE]>
static void dct2(const TCoeff* src, TCoeff* dst, int shift, int numRowsOut, int numZeroTrailRowsOut,
                 int numZeroTrailRowsIn, const TCoeff minOutVal, const TCoeff maxOutVal)
{
  const TCoeff add = 1 << shift >> 1;

  const size_t    numActiveRowsOut = numRowsOut - numZeroTrailRowsOut;
  const size_t    numActiveRowsIn  = TX_SIZE - numZeroTrailRowsIn;
  const ptrdiff_t srcStride        = numRowsOut;
  const ptrdiff_t dstStride        = TX_SIZE;

  for (size_t i = 0; i < numActiveRowsOut; i++)
  {
    __m128i odd[N(TX_SIZE)], even[N(TX_SIZE)];

    mul<TX_SIZE, 1, 2>(odd, src, srcStride, numActiveRowsIn, m[TRANSFORM_INVERSE]);
    mul<TX_SIZE, 2, 4>(odd, src, srcStride, numActiveRowsIn, m[TRANSFORM_INVERSE]);
    mul<TX_SIZE, 4, 8>(odd, src, srcStride, numActiveRowsIn, m[TRANSFORM_INVERSE]);
    mul<TX_SIZE, 8, 16>(odd, src, srcStride, numActiveRowsIn, m[TRANSFORM_INVERSE]);
    mul<TX_SIZE, 0, N(TX_SIZE)>(even, src, srcStride, numActiveRowsIn, m[TRANSFORM_INVERSE]);

    butterfly<TX_SIZE, 16>(even, odd);
    butterfly<TX_SIZE, 8>(even, odd);
    butterfly<TX_SIZE, 4>(even, odd);
    butterfly<TX_SIZE, 2>(even, odd);

    store<TX_SIZE>(even, dst + i * dstStride, add, shift, minOutVal, maxOutVal);

    src++;
  }

  clear<TX_SIZE>(dst, numActiveRowsOut, numRowsOut);
}

template<X86_VEXT vext, size_t TX_SIZE>
static void matrixMultCore(const TCoeff* src, TCoeff* dst, const int shift, const int numRowsOut,
                           const int numZeroTrailRowsOut, const int numZeroTrailRowsIn, const TCoeff minOutVal,
                           const TCoeff maxOutVal, const TMatrixCoeff m[TX_SIZE][TX_SIZE])
{
  const TCoeff add = 1 << shift >> 1;

  const size_t    numActiveRowsOut = numRowsOut - numZeroTrailRowsOut;
  const size_t    numActiveRowsIn  = TX_SIZE - numZeroTrailRowsIn;
  const ptrdiff_t srcStride        = numRowsOut;
  const ptrdiff_t dstStride        = TX_SIZE;

  for (size_t i = 0; i < numActiveRowsOut; i++)
  {
    __m128i tmp[N(TX_SIZE)];

    mul<TX_SIZE, 0, 1>(tmp, src, srcStride, numActiveRowsIn, m);

    store<TX_SIZE>(tmp, dst + i * dstStride, add, shift, minOutVal, maxOutVal);

    src++;
  }

  clear<TX_SIZE>(dst, numActiveRowsOut, numRowsOut);
}

template<X86_VEXT vext, size_t TX_SIZE, const TMatrixCoeff M[TRANSFORM_NUMBER_OF_DIRECTIONS][TX_SIZE][TX_SIZE]>
static void matrixMult(const TCoeff* src, TCoeff* dst, int shift, int numRowsIn, int numZeroTrailRowsIn,
                       int numZeroTrailRowsOut, const TCoeff minOutVal, const TCoeff maxOutVal)
{
  matrixMultCore<vext, TX_SIZE>(src, dst, shift, numRowsIn, numZeroTrailRowsIn, numZeroTrailRowsOut, minOutVal,
                                maxOutVal, M[TRANSFORM_INVERSE]);
}
}   // namespace Inv
#endif
}   // namespace SIMD::X86::TX

//---------------------------------------------------------------------------------------------------------------------

template<X86_VEXT vext> void TrQuant::_initX86()
{
#if RExt__HIGH_BIT_DEPTH_SUPPORT
  // No HBD implementation so far
#else
  m_fwdTx[TransType::DCT2][1] = SIMD::X86::TX::Fwd::matrixMult<vext, 4, g_trCoreDCT2P4>;
  m_fwdTx[TransType::DCT2][2] = SIMD::X86::TX::Fwd::dct2<vext, 8, g_trCoreDCT2P8>;
  m_fwdTx[TransType::DCT2][3] = SIMD::X86::TX::Fwd::dct2<vext, 16, g_trCoreDCT2P16>;
  m_fwdTx[TransType::DCT2][4] = SIMD::X86::TX::Fwd::dct2<vext, 32, g_trCoreDCT2P32>;
  m_fwdTx[TransType::DCT2][5] = SIMD::X86::TX::Fwd::dct2<vext, 64, g_trCoreDCT2P64>;

  m_fwdTx[TransType::DST7][1] = SIMD::X86::TX::Fwd::matrixMult<vext, 4, g_trCoreDST7P4>;
  m_fwdTx[TransType::DST7][2] = SIMD::X86::TX::Fwd::matrixMult<vext, 8, g_trCoreDST7P8>;
  m_fwdTx[TransType::DST7][3] = SIMD::X86::TX::Fwd::matrixMult<vext, 16, g_trCoreDST7P16>;
  m_fwdTx[TransType::DST7][4] = SIMD::X86::TX::Fwd::matrixMult<vext, 32, g_trCoreDST7P32>;

  m_fwdTx[TransType::DCT8][1] = SIMD::X86::TX::Fwd::matrixMult<vext, 4, g_trCoreDCT8P4>;
  m_fwdTx[TransType::DCT8][2] = SIMD::X86::TX::Fwd::matrixMult<vext, 8, g_trCoreDCT8P8>;
  m_fwdTx[TransType::DCT8][3] = SIMD::X86::TX::Fwd::matrixMult<vext, 16, g_trCoreDCT8P16>;
  m_fwdTx[TransType::DCT8][4] = SIMD::X86::TX::Fwd::matrixMult<vext, 32, g_trCoreDCT8P32>;

  m_invTx[TransType::DCT2][1] = SIMD::X86::TX::Inv::matrixMult<vext, 4, g_trCoreDCT2P4>;
  m_invTx[TransType::DCT2][2] = SIMD::X86::TX::Inv::dct2<vext, 8, g_trCoreDCT2P8>;
  m_invTx[TransType::DCT2][3] = SIMD::X86::TX::Inv::dct2<vext, 16, g_trCoreDCT2P16>;
  m_invTx[TransType::DCT2][4] = SIMD::X86::TX::Inv::dct2<vext, 32, g_trCoreDCT2P32>;
  m_invTx[TransType::DCT2][5] = SIMD::X86::TX::Inv::dct2<vext, 64, g_trCoreDCT2P64>;

  m_invTx[TransType::DST7][1] = SIMD::X86::TX::Inv::matrixMult<vext, 4, g_trCoreDST7P4>;
  m_invTx[TransType::DST7][2] = SIMD::X86::TX::Inv::matrixMult<vext, 8, g_trCoreDST7P8>;
  m_invTx[TransType::DST7][3] = SIMD::X86::TX::Inv::matrixMult<vext, 16, g_trCoreDST7P16>;
  m_invTx[TransType::DST7][4] = SIMD::X86::TX::Inv::matrixMult<vext, 32, g_trCoreDST7P32>;

  m_invTx[TransType::DCT8][1] = SIMD::X86::TX::Inv::matrixMult<vext, 4, g_trCoreDCT8P4>;
  m_invTx[TransType::DCT8][2] = SIMD::X86::TX::Inv::matrixMult<vext, 8, g_trCoreDCT8P8>;
  m_invTx[TransType::DCT8][3] = SIMD::X86::TX::Inv::matrixMult<vext, 16, g_trCoreDCT8P16>;
  m_invTx[TransType::DCT8][4] = SIMD::X86::TX::Inv::matrixMult<vext, 32, g_trCoreDCT8P32>;
#endif
}

template void TrQuant::_initX86<SIMDX86>();

#endif
