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

#include "CommonDefX86.h"
#include "../Rom.h"
#include "../InterpolationFilter.h"

//#include "../ChromaFormat.h"

//! \ingroup CommonLib
//! \{

#ifdef TARGET_SIMD_X86

#if defined _MSC_VER
#include <tmmintrin.h>
#else
#include <immintrin.h>
#endif

// ===========================
// Full-pel copy 8-bit/16-bit
// ===========================
template<typename Tsrc, int N, bool isFirst, bool isLast>
static void fullPelCopySSE(const ClpRng &clpRng, const void *_src, const ptrdiff_t srcStride, int16_t *dst,
                           const ptrdiff_t dstStride, int width, int height)
{
  Tsrc* src = (Tsrc*)_src;

  int headroom = IF_INTERNAL_FRAC_BITS(clpRng.bd);
  int headroom_offset = 1 << ( headroom - 1 );
  int offset   = IF_INTERNAL_OFFS;
  __m128i voffset  = _mm_set1_epi16( offset );
  __m128i voffset_headroom  = _mm_set1_epi16( headroom_offset );

  __m128i vibdimin = _mm_set1_epi16( clpRng.min );
  __m128i vibdimax = _mm_set1_epi16( clpRng.max );
  __m128i vsrc, vsum;

  for( int row = 0; row < height; row++ )
  {
    for( int col = 0; col < width; col+=N )
    {
      _mm_prefetch( (const char*)src+2*srcStride, _MM_HINT_T0 );
      _mm_prefetch( (const char*)src+( width>>1 ) + 2*srcStride, _MM_HINT_T0 );
      _mm_prefetch( (const char*)src+width-1 + 2*srcStride, _MM_HINT_T0 );
      for( int i=0; i<N; i+=8 )
      {
        if( sizeof( Tsrc )==1 )
        {
          vsrc = _mm_cvtepu8_epi16( _mm_lddqu_si128( ( __m128i const * )&src[col+i] ) );
        }
        else
        {
          vsrc = _mm_lddqu_si128( ( __m128i const * )&src[col+i] );
        }

        if( isFirst == isLast )
        {
          vsum =  _mm_min_epi16( vibdimax, _mm_max_epi16( vibdimin, vsrc ) );
        }
        else if( isFirst )
        {
          vsrc = _mm_slli_epi16( vsrc, headroom );
          vsum = _mm_sub_epi16( vsrc, voffset );
        }
        else
        {
          vsrc = _mm_add_epi16( vsrc, voffset );
          vsrc = _mm_add_epi16( vsrc, voffset_headroom );
          vsrc = _mm_srai_epi16( vsrc, headroom );
          vsum = _mm_min_epi16( vibdimax, _mm_max_epi16( vibdimin, vsrc ) );
        }
        _mm_storeu_si128( ( __m128i * )&dst[col+i], vsum );
      }
    }
    src += srcStride;
    dst += dstStride;
  }
}

template<typename Tsrc, int N, bool isFirst, bool isLast>
static void fullPelCopyAVX2(const ClpRng &clpRng, const void *_src, const ptrdiff_t srcStride, short *dst,
                            const ptrdiff_t dstStride, int width, int height)
{
#ifdef USE_AVX2
  Tsrc* src = (Tsrc*)_src;

  int headroom = IF_INTERNAL_FRAC_BITS(clpRng.bd);
  int offset   = 1 << ( headroom - 1 );
  int internal_offset = IF_INTERNAL_OFFS;

  __m256i vinternal_offset = _mm256_set1_epi16( internal_offset );
  __m256i vheadroom_offset = _mm256_set1_epi16( offset );

  __m256i vibdimin = _mm256_set1_epi16( clpRng.min );
  __m256i vibdimax = _mm256_set1_epi16( clpRng.max );
  __m256i vsrc, vsum;


  for( int row = 0; row < height; row++ )
  {
    for( int col = 0; col < width; col+=N )
    {
      _mm_prefetch( (const char*)( src+3*srcStride ), _MM_HINT_T0 );
      _mm_prefetch( (const char*)( src+( width>>1 ) + 3*srcStride ), _MM_HINT_T0 );
      _mm_prefetch( (const char*)( src+width-1 + 3*srcStride ), _MM_HINT_T0 );
      for( int i=0; i<N; i+=16 )
      {
        if( sizeof( Tsrc )==1 )
        {
          vsrc = _mm256_cvtepu8_epi16( _mm_loadu_si128( ( const __m128i * )&src[col+i] ) );
        }
        else
        {
          vsrc = _mm256_lddqu_si256( ( const __m256i * )&src[col+i] );
        }

        if( isFirst == isLast )
        {
          vsum = _mm256_min_epi16( vibdimax, _mm256_max_epi16( vibdimin, vsrc ) );
        }
        else if( isFirst )
        {
          vsrc = _mm256_slli_epi16( vsrc, headroom );
          vsum = _mm256_sub_epi16( vsrc, vinternal_offset );
        }
        else
        {
          vsrc = _mm256_add_epi16( vsrc, vinternal_offset );
          vsrc = _mm256_add_epi16( vsrc, vheadroom_offset );
          vsrc = _mm256_srai_epi16( vsrc, headroom );
          vsum = _mm256_min_epi16( vibdimax, _mm256_max_epi16( vibdimin, vsrc ) );
        }
        _mm256_storeu_si256( ( __m256i * )&dst[col+i], vsum );
      }
    }
    src += srcStride;
    dst += dstStride;
  }
#endif
}

template<X86_VEXT vext, bool isFirst, bool isLast>
static void simdFilterCopy(const ClpRng &clpRng, const Pel *src, const ptrdiff_t srcStride, int16_t *dst,
                           const ptrdiff_t dstStride, int width, int height, bool biMCForDMVR)
{
  InterpolationFilter::filterCopy<isFirst, isLast>(clpRng, src, srcStride, dst, dstStride, width, height, biMCForDMVR);
}


// SIMD interpolation horizontal, block width modulo 4
template<X86_VEXT vext, int N, bool CLAMP>
static void simdInterpolateHorM4(const int16_t *src, const ptrdiff_t srcStride, int16_t *dst, const ptrdiff_t dstStride,
                                 int width, int height, int shift, int offset, const ClpRng &clpRng,
                                 int16_t const *coeff)
{
  static_assert(N == 2 || N == 4 || N == 6 || N == 8, "only filter sizes 2, 4, 6, and 8 are supported");

  _mm_prefetch((const char *) (src + srcStride), _MM_HINT_T0);

  const __m128i minVal = _mm_set1_epi16(clpRng.min);
  const __m128i maxVal = _mm_set1_epi16(clpRng.max);

  __m128i c;
  switch (N)
  {
  case 2: c = _mm_cvtsi32_si128(*(int32_t *) coeff); break;
  case 4: c = _mm_loadl_epi64((__m128i const *) coeff); break;
  default: c = _mm_loadu_si128((__m128i const *) coeff); break;
  }

  __m128i coeffs[4];   // should be coeffs[N / 2] but MSVC doesn't like it
  switch (N)
  {
  case 8: coeffs[3] = _mm_shuffle_epi32(c, 0xff);
  case 6: coeffs[2] = _mm_shuffle_epi32(c, 0xaa);
  case 4: coeffs[1] = _mm_shuffle_epi32(c, 0x55);
  default: coeffs[0] = _mm_shuffle_epi32(c, 0x00); break;
  }

  const __m128i shuffle0 = _mm_setr_epi8(0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9);
  const __m128i shuffle1 = _mm_setr_epi8(4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 10, 11, 10, 11, 12, 13);

  for (ptrdiff_t row = 0; row < height; row++)
  {
    _mm_prefetch((const char *) (src + (row + 2) * srcStride), _MM_HINT_T0);

    for (ptrdiff_t col = 0; col < width; col += 4)
    {
      __m128i vsum = _mm_set1_epi32(offset);

      for (ptrdiff_t i = 0; i < N / 2; i += 2)
      {
        const __m128i val = _mm_loadu_si128((const __m128i *) (src + srcStride * row + col + 2 * i));

        vsum = _mm_add_epi32(vsum, _mm_madd_epi16(_mm_shuffle_epi8(val, shuffle0), coeffs[i]));

        if (i + 1 < N / 2)
        {
          vsum = _mm_add_epi32(vsum, _mm_madd_epi16(_mm_shuffle_epi8(val, shuffle1), coeffs[i + 1]));
        }
      }

      vsum = _mm_sra_epi32(vsum, _mm_cvtsi32_si128(shift));
      vsum = _mm_packs_epi32(vsum, vsum);

      if (CLAMP)
      {
        vsum = _mm_min_epi16(vsum, maxVal);
        vsum = _mm_max_epi16(vsum, minVal);
      }

      _mm_storel_epi64((__m128i *) (dst + dstStride * row + col), vsum);
    }
  }
}

// SIMD interpolation horizontal, block width modulo 8
template<X86_VEXT vext, int N, bool CLAMP>
static void simdInterpolateHorM8(const int16_t *src, const ptrdiff_t srcStride, int16_t *dst, const ptrdiff_t dstStride,
                                 int width, int height, int shift, int offset, const ClpRng &clpRng,
                                 int16_t const *coeff)
{
  static_assert(N == 2 || N == 4 || N == 6 || N == 8, "only filter sizes 2, 4, 6, and 8 are supported");

  std::array<ptrdiff_t, 3> memOffsets = { { 2 * srcStride, 2 * srcStride + (width >> 1),
                                            2 * srcStride + width - 8 + (N / 2 + 1) / 2 * 4 + 7 } };

  for (auto &off: memOffsets)
  {
    _mm_prefetch((const char *) (src - srcStride + off), _MM_HINT_T0);
  }

  const __m128i minVal = _mm_set1_epi16(clpRng.min);
  const __m128i maxVal = _mm_set1_epi16(clpRng.max);

  __m128i c;
  switch (N)
  {
  case 2: c = _mm_cvtsi32_si128(*(int32_t *) coeff); break;
  case 4: c = _mm_loadl_epi64((__m128i const *) coeff); break;
  default: c = _mm_loadu_si128((__m128i const *) coeff); break;
  }

  __m128i coeffs[4];   // should be coeffs[N / 2] but MSVC doesn't like it
  switch (N)
  {
  case 8: coeffs[3] = _mm_shuffle_epi32(c, 0xff);
  case 6: coeffs[2] = _mm_shuffle_epi32(c, 0xaa);
  case 4: coeffs[1] = _mm_shuffle_epi32(c, 0x55);
  default: coeffs[0] = _mm_shuffle_epi32(c, 0x00); break;
  }

  const __m128i shuffle0 = _mm_setr_epi8(0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9);
  const __m128i shuffle1 = _mm_setr_epi8(4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 10, 11, 10, 11, 12, 13);

  for (ptrdiff_t row = 0; row < height; row++)
  {
    for (auto &off: memOffsets)
    {
      _mm_prefetch((const char *) (src + row * srcStride + off), _MM_HINT_T0);
    }

    for (ptrdiff_t col = 0; col < width; col += 8)
    {
      __m128i vsum0 = _mm_set1_epi32(offset);
      __m128i vsum1 = _mm_set1_epi32(offset);

      __m128i val0 = _mm_loadu_si128((const __m128i *) (src + srcStride * row + col));

      for (ptrdiff_t i = 0; i < N / 2; i += 2)
      {
        const __m128i val1 = _mm_loadu_si128((const __m128i *) (src + srcStride * row + col + 2 * i + 4));

        vsum0 = _mm_add_epi32(vsum0, _mm_madd_epi16(_mm_shuffle_epi8(val0, shuffle0), coeffs[i]));
        vsum1 = _mm_add_epi32(vsum1, _mm_madd_epi16(_mm_shuffle_epi8(val1, shuffle0), coeffs[i]));

        if (i + 1 < N / 2)
        {
          vsum0 = _mm_add_epi32(vsum0, _mm_madd_epi16(_mm_shuffle_epi8(val0, shuffle1), coeffs[i + 1]));
          vsum1 = _mm_add_epi32(vsum1, _mm_madd_epi16(_mm_shuffle_epi8(val1, shuffle1), coeffs[i + 1]));
        }

        val0 = val1;
      }

      vsum0 = _mm_sra_epi32(vsum0, _mm_cvtsi32_si128(shift));
      vsum1 = _mm_sra_epi32(vsum1, _mm_cvtsi32_si128(shift));

      __m128i vsum = _mm_packs_epi32(vsum0, vsum1);

      if (CLAMP)
      {
        vsum = _mm_min_epi16(vsum, maxVal);
        vsum = _mm_max_epi16(vsum, minVal);
      }

      _mm_storeu_si128((__m128i *) (dst + dstStride * row + col), vsum);
    }
  }
}

#ifdef USE_AVX2
template<X86_VEXT vext, int N, bool CLAMP>
static void simdInterpolateHorM8_AVX2(const int16_t *src, const ptrdiff_t srcStride, int16_t *dst,
                                      const ptrdiff_t dstStride, int width, int height, int shift, int offset,
                                      const ClpRng &clpRng, int16_t const *coeff)
{
  static_assert(N == 2 || N == 4 || N == 6 || N == 8, "only filter sizes 2, 4, 6, and 8 are supported");

  std::array<ptrdiff_t, 3> memOffsets = { { 2 * srcStride, 2 * srcStride + (width >> 1),
                                            2 * srcStride + width - 8 + (N / 2 + 1) / 2 * 4 + 7 } };

  for (auto &off: memOffsets)
  {
    _mm_prefetch((const char *) (src - srcStride + off), _MM_HINT_T0);
  }

  const __m128i minVal = _mm_set1_epi16(clpRng.min);
  const __m128i maxVal = _mm_set1_epi16(clpRng.max);

  __m128i c0;
  switch (N)
  {
  case 2: c0 = _mm_cvtsi32_si128(*(int32_t *) coeff); break;
  case 4: c0 = _mm_loadl_epi64((__m128i const *) coeff); break;
  default: c0 = _mm_loadu_si128((__m128i const *) coeff); break;
  }
  __m256i c = _mm256_broadcastsi128_si256(c0);

  __m256i coeffs[4];   // should be coeffs[N / 2] but MSVC doesn't like it
  switch (N)
  {
  case 8: coeffs[3] = _mm256_shuffle_epi32(c, 0xff);
  case 6: coeffs[2] = _mm256_shuffle_epi32(c, 0xaa);
  case 4: coeffs[1] = _mm256_shuffle_epi32(c, 0x55);
  default: coeffs[0] = _mm256_shuffle_epi32(c, 0x00); break;
  }

  const __m256i shuffle0 = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9));
  const __m256i shuffle1 =
    _mm256_broadcastsi128_si256(_mm_setr_epi8(4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 10, 11, 10, 11, 12, 13));

  for (ptrdiff_t row = 0; row < height; row++)
  {
    for (auto &off: memOffsets)
    {
      _mm_prefetch((const char *) (src + row * srcStride + off), _MM_HINT_T0);
    }

    for (ptrdiff_t col = 0; col < width; col += 8)
    {
      __m256i vsum = _mm256_set1_epi32(offset);

      __m128i val0 = _mm_loadu_si128((const __m128i *) (src + srcStride * row + col));

      for (ptrdiff_t i = 0; i < N / 2; i += 2)
      {
        const __m128i val1 = _mm_loadu_si128((const __m128i *) (src + srcStride * row + col + 2 * i + 4));
        const __m256i val  = _mm256_inserti128_si256(_mm256_castsi128_si256(val0), val1, 1);

        vsum = _mm256_add_epi32(vsum, _mm256_madd_epi16(_mm256_shuffle_epi8(val, shuffle0), coeffs[i]));

        if (i + 1 < N / 2)
        {
          vsum = _mm256_add_epi32(vsum, _mm256_madd_epi16(_mm256_shuffle_epi8(val, shuffle1), coeffs[i + 1]));
        }

        val0 = val1;
      }

      vsum = _mm256_sra_epi32(vsum, _mm_cvtsi32_si128(shift));

      __m128i sum = _mm_packs_epi32(_mm256_castsi256_si128(vsum), _mm256_extracti128_si256(vsum, 1));

      if (CLAMP)
      {
        sum = _mm_min_epi16(sum, maxVal);
        sum = _mm_max_epi16(sum, minVal);
      }

      _mm_storeu_si128((__m128i *) (dst + dstStride * row + col), sum);
    }
  }
}
#endif

template<X86_VEXT vext, int N, bool CLAMP>
static void simdInterpolateVerM4(const int16_t *src, const ptrdiff_t srcStride, int16_t *dst, const ptrdiff_t dstStride,
                                 int width, int height, int shift, int offset, const ClpRng &clpRng,
                                 int16_t const *coeff)
{
  static_assert(N == 2 || N == 4 || N == 6 || N == 8, "only filter sizes 2, 4, 6, and 8 are supported");

  const __m128i minVal = _mm_set1_epi16(clpRng.min);
  const __m128i maxVal = _mm_set1_epi16(clpRng.max);

  const __m128i vcoeff =
    N == 2 ? _mm_cvtsi32_si128(*(uint32_t *) coeff)
           : (N == 4 ? _mm_loadl_epi64((__m128i const *) coeff) : _mm_loadu_si128((__m128i const *) coeff));

  for (ptrdiff_t col = 0; col < width; col += 4)
  {
    for (ptrdiff_t row = 0; row < height; row++)
    {
      __m128i vsum = _mm_set1_epi32(offset);

      __m128i val[N / 2];

      for (ptrdiff_t i = 0; i < N / 2; i++)
      {
        const __m128i valA = _mm_loadl_epi64((__m128i const *) (src + col + (row + 2 * i) * srcStride));
        const __m128i valB = _mm_loadl_epi64((__m128i const *) (src + col + (row + 2 * i + 1) * srcStride));

        val[i] = _mm_unpacklo_epi16(valA, valB);
      }

      vsum = _mm_add_epi32(vsum, _mm_madd_epi16(val[0], _mm_shuffle_epi32(vcoeff, 0x00)));
      if (N >= 4)
      {
        vsum = _mm_add_epi32(vsum, _mm_madd_epi16(val[1], _mm_shuffle_epi32(vcoeff, 0x55)));
      }
      if (N >= 6)
      {
        vsum = _mm_add_epi32(vsum, _mm_madd_epi16(val[2], _mm_shuffle_epi32(vcoeff, 0xaa)));
      }
      if (N == 8)
      {
        vsum = _mm_add_epi32(vsum, _mm_madd_epi16(val[3], _mm_shuffle_epi32(vcoeff, 0xff)));
      }

      vsum = _mm_srai_epi32(vsum, shift);
      vsum = _mm_packs_epi32(vsum, vsum);

      if (CLAMP)
      {
        vsum = _mm_min_epi16(vsum, maxVal);
        vsum = _mm_max_epi16(vsum, minVal);
      }

      _mm_storel_epi64((__m128i *) (dst + row * dstStride + col), vsum);
    }
  }
}

template<X86_VEXT vext, int N, bool shiftBack>
static void simdInterpolateVerM8(const int16_t *src, const ptrdiff_t srcStride, int16_t *dst, const ptrdiff_t dstStride,
                                 int width, int height, int shift, int offset, const ClpRng &clpRng,
                                 int16_t const *coeff)
{
  const int16_t *srcOrig = src;
  int16_t *dstOrig = dst;

  __m128i vcoeff[N / 2], vsrc[N];
  __m128i vzero = _mm_setzero_si128();
  __m128i voffset = _mm_set1_epi32( offset );
  __m128i vibdimin = _mm_set1_epi16( clpRng.min );
  __m128i vibdimax = _mm_set1_epi16( clpRng.max );

  __m128i vsum, vsuma, vsumb;

  for( int i = 0; i < N; i += 2 )
  {
    vcoeff[i / 2] = _mm_unpacklo_epi16( _mm_set1_epi16( coeff[i] ), _mm_set1_epi16( coeff[i + 1] ) );
  }

  for( int col = 0; col < width; col += 8 )
  {
    for( int i = 0; i < N - 1; i++ )
    {
      vsrc[i] = _mm_lddqu_si128( ( __m128i const * )&src[col + i * srcStride] );
    }

    for( int row = 0; row < height; row++ )
    {
      vsrc[N - 1] = _mm_lddqu_si128( ( __m128i const * )&src[col + ( N - 1 ) * srcStride] );
      vsuma = vsumb = vzero;
      for( int i = 0; i < N; i += 2 )
      {
        __m128i vsrca = _mm_unpacklo_epi16( vsrc[i], vsrc[i + 1] );
        __m128i vsrcb = _mm_unpackhi_epi16( vsrc[i], vsrc[i + 1] );
        vsuma = _mm_add_epi32( vsuma, _mm_madd_epi16( vsrca, vcoeff[i / 2] ) );
        vsumb = _mm_add_epi32( vsumb, _mm_madd_epi16( vsrcb, vcoeff[i / 2] ) );
      }
      for( int i = 0; i < N - 1; i++ )
      {
        vsrc[i] = vsrc[i + 1];
      }

      vsuma = _mm_add_epi32(vsuma, voffset);
      vsumb = _mm_add_epi32(vsumb, voffset);

      vsuma = _mm_srai_epi32(vsuma, shift);
      vsumb = _mm_srai_epi32(vsumb, shift);

      vsum = _mm_packs_epi32(vsuma, vsumb);

      if( shiftBack ) //clip
      {
        vsum = _mm_min_epi16( vibdimax, _mm_max_epi16( vibdimin, vsum ) );
      }

      _mm_storeu_si128( ( __m128i * )&dst[col], vsum );

      src += srcStride;
      dst += dstStride;
    }
    src = srcOrig;
    dst = dstOrig;
  }
}

#ifdef USE_AVX2
template<X86_VEXT vext, int N, bool CLAMP>
static void simdInterpolateVerM8_AVX2(const int16_t *src, const ptrdiff_t srcStride, int16_t *dst,
                                      const ptrdiff_t dstStride, int width, int height, int shift, int offset,
                                      const ClpRng &clpRng, int16_t const *coeff)
{
  const __m128i minVal = _mm_set1_epi16(clpRng.min);
  const __m128i maxVal = _mm_set1_epi16(clpRng.max);

  __m256i coeffs[N / 2];

  for (int i = 0; i < N / 2; i++)
  {
    coeffs[i] = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(*(int32_t *) &coeff[2 * i]));
  }

  for (ptrdiff_t col = 0; col < width; col += 8)
  {
    __m256i vsrc[N];

    for (int i = 0; i < N - 1; i++)
    {
      vsrc[i] = _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *) (src + col + i * srcStride)));
      vsrc[i] = _mm256_permute4x64_epi64(vsrc[i], _MM_SHUFFLE(1, 1, 0, 0));
    }

    for (ptrdiff_t row = 0; row < height; row++)
    {
      vsrc[N - 1] = _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *) (src + col + (row + N - 1) * srcStride)));
      vsrc[N - 1] = _mm256_permute4x64_epi64(vsrc[N - 1], _MM_SHUFFLE(1, 1, 0, 0));

      __m256i vsum = _mm256_set1_epi32(offset);

      for (int i = 0; i < N / 2; i++)
      {
        __m256i vsrc0 = _mm256_unpacklo_epi16(vsrc[2 * i], vsrc[2 * i + 1]);
        vsum          = _mm256_add_epi32(vsum, _mm256_madd_epi16(vsrc0, coeffs[i]));
      }

      vsum = _mm256_sra_epi32(vsum, _mm_cvtsi32_si128(shift));
      vsum = _mm256_packs_epi32(vsum, vsum);

      __m128i sum = _mm256_castsi256_si128(_mm256_permute4x64_epi64(vsum, _MM_SHUFFLE(3, 1, 2, 0)));

      if (CLAMP)
      {
        sum = _mm_min_epi16(sum, maxVal);
        sum = _mm_max_epi16(sum, minVal);
      }

      _mm_storeu_si128((__m128i *) (dst + row * dstStride + col), sum);

      for (int i = 0; i < N - 1; i++)
      {
        vsrc[i] = vsrc[i + 1];
      }
    }
  }
}
#endif

template<int N, bool isLast>
inline void interpolate(const int16_t *src, const ptrdiff_t cStride, int16_t *dst, int width, int shift, int offset,
                        int bitdepth, int maxVal, int16_t const *c)
{
  for( int col = 0; col < width; col++ )
  {
    int sum;

    sum = src[col + 0 * cStride] * c[0];
    sum += src[col + 1 * cStride] * c[1];
    if( N >= 4 )
    {
      sum += src[col + 2 * cStride] * c[2];
      sum += src[col + 3 * cStride] * c[3];
    }
    if( N >= 6 )
    {
      sum += src[col + 4 * cStride] * c[4];
      sum += src[col + 5 * cStride] * c[5];
    }
    if( N == 8 )
    {
      sum += src[col + 6 * cStride] * c[6];
      sum += src[col + 7 * cStride] * c[7];
    }

    Pel val = ( sum + offset ) >> shift;
    if( isLast )
    {
      val = ( val < 0 ) ? 0 : val;
      val = ( val > maxVal ) ? maxVal : val;
    }
    dst[col] = val;
  }
}

static inline __m128i simdInterpolateLuma2P8(int16_t const *src, const ptrdiff_t srcStride, __m128i *mmCoeff,
                                             const __m128i &mmOffset, int shift)
{
  __m128i sumHi = _mm_setzero_si128();
  __m128i sumLo = _mm_setzero_si128();
  for( int n = 0; n < 2; n++ )
  {
    __m128i mmPix = _mm_loadu_si128( ( __m128i* )src );
    __m128i hi = _mm_mulhi_epi16( mmPix, mmCoeff[n] );
    __m128i lo = _mm_mullo_epi16( mmPix, mmCoeff[n] );
    sumHi = _mm_add_epi32( sumHi, _mm_unpackhi_epi16( lo, hi ) );
    sumLo = _mm_add_epi32( sumLo, _mm_unpacklo_epi16( lo, hi ) );
    src += srcStride;
  }
  sumHi = _mm_srai_epi32( _mm_add_epi32( sumHi, mmOffset ), shift );
  sumLo = _mm_srai_epi32( _mm_add_epi32( sumLo, mmOffset ), shift );
  return( _mm_packs_epi32( sumLo, sumHi ) );
}

static inline __m128i simdInterpolateLuma2P4(int16_t const *src, const ptrdiff_t srcStride, __m128i *mmCoeff,
                                             const __m128i &mmOffset, int shift)
{
  __m128i sumHi = _mm_setzero_si128();
  __m128i sumLo = _mm_setzero_si128();
  for( int n = 0; n < 2; n++ )
  {
    __m128i mmPix = _mm_loadl_epi64( ( __m128i* )src );
    __m128i hi = _mm_mulhi_epi16( mmPix, mmCoeff[n] );
    __m128i lo = _mm_mullo_epi16( mmPix, mmCoeff[n] );
    sumHi = _mm_add_epi32( sumHi, _mm_unpackhi_epi16( lo, hi ) );
    sumLo = _mm_add_epi32( sumLo, _mm_unpacklo_epi16( lo, hi ) );
    src += srcStride;
  }
  sumHi = _mm_srai_epi32( _mm_add_epi32( sumHi, mmOffset ), shift );
  sumLo = _mm_srai_epi32( _mm_add_epi32( sumLo, mmOffset ), shift );
  return( _mm_packs_epi32( sumLo, sumHi ) );
}

static inline __m128i simdClip3( __m128i mmMin, __m128i mmMax, __m128i mmPix )
{
  __m128i mmMask = _mm_cmpgt_epi16( mmPix, mmMin );
  mmPix = _mm_or_si128( _mm_and_si128( mmMask, mmPix ), _mm_andnot_si128( mmMask, mmMin ) );
  mmMask = _mm_cmplt_epi16( mmPix, mmMax );
  mmPix = _mm_or_si128( _mm_and_si128( mmMask, mmPix ), _mm_andnot_si128( mmMask, mmMax ) );
  return( mmPix );
}

template<X86_VEXT vext, bool isLast>
static void simdInterpolateN2_M8(const int16_t *src, const ptrdiff_t srcStride, int16_t *dst, const ptrdiff_t dstStride,
                                 const ptrdiff_t cStride, int width, int height, int shift, int offset,
                                 const ClpRng &clpRng, int16_t const *c)
{
  int row, col;
  __m128i mmOffset = _mm_set1_epi32( offset );
  __m128i mmCoeff[2];
  __m128i mmMin = _mm_set1_epi16( clpRng.min );
  __m128i mmMax = _mm_set1_epi16( clpRng.max );
  for( int n = 0; n < 2; n++ )
  {
    mmCoeff[n] = _mm_set1_epi16( c[n] );
  }
  for( row = 0; row < height; row++ )
  {
    for( col = 0; col < width; col += 8 )
    {
      __m128i mmFiltered = simdInterpolateLuma2P8( src + col, cStride, mmCoeff, mmOffset, shift );
      if( isLast )
      {
        mmFiltered = simdClip3( mmMin, mmMax, mmFiltered );
      }
      _mm_storeu_si128( ( __m128i * )( dst + col ), mmFiltered );
    }
    src += srcStride;
    dst += dstStride;
  }
}

template<X86_VEXT vext, bool isLast>
static void simdInterpolateN2_M4(const int16_t *src, const ptrdiff_t srcStride, int16_t *dst, const ptrdiff_t dstStride,
                                 const ptrdiff_t cStride, int width, int height, int shift, int offset,
                                 const ClpRng &clpRng, int16_t const *c)
{
  int row, col;
  __m128i mmOffset = _mm_set1_epi32( offset );
  __m128i mmCoeff[8];
  __m128i mmMin = _mm_set1_epi16( clpRng.min );
  __m128i mmMax = _mm_set1_epi16( clpRng.max );
  for( int n = 0; n < 2; n++ )
  {
    mmCoeff[n] = _mm_set1_epi16( c[n] );
  }
  for( row = 0; row < height; row++ )
  {
    for( col = 0; col < width; col += 4 )
    {
      __m128i mmFiltered = simdInterpolateLuma2P4( src + col, cStride, mmCoeff, mmOffset, shift );
      if( isLast )
      {
        mmFiltered = simdClip3( mmMin, mmMax, mmFiltered );
      }
      _mm_storel_epi64( ( __m128i * )( dst + col ), mmFiltered );
    }
    src += srcStride;
    dst += dstStride;
  }
}
#ifdef USE_AVX2
static inline __m256i simdInterpolateLuma10Bit2P16(int16_t const *src1, const ptrdiff_t srcStride, __m256i *mmCoeff,
                                                   const __m256i &mmOffset, __m128i &mmShift)
{
  __m256i sumLo;
  __m256i mmPix  = _mm256_loadu_si256((__m256i *) src1);
  __m256i mmPix1 = _mm256_loadu_si256((__m256i *) (src1 + srcStride));
  __m256i lo0    = _mm256_mullo_epi16(mmPix, mmCoeff[0]);
  __m256i lo1    = _mm256_mullo_epi16(mmPix1, mmCoeff[1]);
  sumLo          = _mm256_add_epi16(lo0, lo1);
  sumLo = _mm256_sra_epi16(_mm256_add_epi16(sumLo, mmOffset), mmShift);
  return(sumLo);
}
#endif

static inline __m128i simdInterpolateLuma10Bit2P8(int16_t const *src1, const ptrdiff_t srcStride, __m128i *mmCoeff,
                                                  const __m128i &mmOffset, __m128i &mmShift)
{
  __m128i sumLo;
  __m128i mmPix  = _mm_loadu_si128((__m128i *) src1);
  __m128i mmPix1 = _mm_loadu_si128((__m128i *) (src1 + srcStride));
  __m128i lo0    = _mm_mullo_epi16(mmPix, mmCoeff[0]);
  __m128i lo1    = _mm_mullo_epi16(mmPix1, mmCoeff[1]);
  sumLo          = _mm_add_epi16(lo0, lo1);
  sumLo = _mm_sra_epi16(_mm_add_epi16(sumLo, mmOffset), mmShift);
  return(sumLo);
}

static inline __m128i simdInterpolateLuma10Bit2P4(int16_t const *src, const ptrdiff_t srcStride, __m128i *mmCoeff,
                                                  const __m128i &mmOffset, __m128i &mmShift)
{
  __m128i sumLo;
  __m128i mmPix  = _mm_loadl_epi64((__m128i *) src);
  __m128i mmPix1 = _mm_loadl_epi64((__m128i *) (src + srcStride));
  __m128i lo0    = _mm_mullo_epi16(mmPix, mmCoeff[0]);
  __m128i lo1    = _mm_mullo_epi16(mmPix1, mmCoeff[1]);
  sumLo          = _mm_add_epi16(lo0, lo1);
  sumLo = _mm_sra_epi16(_mm_add_epi16(sumLo, mmOffset), mmShift);
  return sumLo;
}

#ifdef USE_AVX2
static inline __m256i simdInterpolateLumaHighBit2P16(int16_t const *src1, const ptrdiff_t srcStride, __m256i *mmCoeff,
                                                     const __m256i &mmOffset, __m128i &mmShift)
{
  __m256i mm_mul_lo = _mm256_setzero_si256();
  __m256i mm_mul_hi = _mm256_setzero_si256();

  for (int coefIdx = 0; coefIdx < 2; coefIdx++)
  {
    __m256i mmPix = _mm256_lddqu_si256((__m256i*)(src1 + coefIdx * srcStride));
    __m256i mm_hi = _mm256_mulhi_epi16(mmPix, mmCoeff[coefIdx]);
    __m256i mm_lo = _mm256_mullo_epi16(mmPix, mmCoeff[coefIdx]);
    mm_mul_lo = _mm256_add_epi32(mm_mul_lo, _mm256_unpacklo_epi16(mm_lo, mm_hi));
    mm_mul_hi = _mm256_add_epi32(mm_mul_hi, _mm256_unpackhi_epi16(mm_lo, mm_hi));
  }
  mm_mul_lo = _mm256_sra_epi32(_mm256_add_epi32(mm_mul_lo, mmOffset), mmShift);
  mm_mul_hi = _mm256_sra_epi32(_mm256_add_epi32(mm_mul_hi, mmOffset), mmShift);
  __m256i mm_sum = _mm256_packs_epi32(mm_mul_lo, mm_mul_hi);
  return (mm_sum);
}
#endif

static inline __m128i simdInterpolateLumaHighBit2P8(int16_t const *src1, const ptrdiff_t srcStride, __m128i *mmCoeff,
                                                    const __m128i &mmOffset, __m128i &mmShift)
{
  __m128i mm_mul_lo = _mm_setzero_si128();
  __m128i mm_mul_hi = _mm_setzero_si128();

  for (int coefIdx = 0; coefIdx < 2; coefIdx++)
  {
    __m128i mmPix = _mm_loadu_si128((__m128i*)(src1 + coefIdx * srcStride));
    __m128i mm_hi = _mm_mulhi_epi16(mmPix, mmCoeff[coefIdx]);
    __m128i mm_lo = _mm_mullo_epi16(mmPix, mmCoeff[coefIdx]);
    mm_mul_lo = _mm_add_epi32(mm_mul_lo, _mm_unpacklo_epi16(mm_lo, mm_hi));
    mm_mul_hi = _mm_add_epi32(mm_mul_hi, _mm_unpackhi_epi16(mm_lo, mm_hi));
  }
  mm_mul_lo = _mm_sra_epi32(_mm_add_epi32(mm_mul_lo, mmOffset), mmShift);
  mm_mul_hi = _mm_sra_epi32(_mm_add_epi32(mm_mul_hi, mmOffset), mmShift);
  __m128i mm_sum = _mm_packs_epi32(mm_mul_lo, mm_mul_hi);
  return(mm_sum);
}

static inline __m128i simdInterpolateLumaHighBit2P4(int16_t const *src1, const ptrdiff_t srcStride, __m128i *mmCoeff,
                                                    const __m128i &mmOffset, __m128i &mmShift)
{
  __m128i mm_sum = _mm_setzero_si128();
  __m128i mm_zero = _mm_setzero_si128();
  for (int coefIdx = 0; coefIdx < 2; coefIdx++)
  {
    __m128i mmPix = _mm_loadl_epi64((__m128i*)(src1 + coefIdx * srcStride));
    __m128i mm_hi = _mm_mulhi_epi16(mmPix, mmCoeff[coefIdx]);
    __m128i mm_lo = _mm_mullo_epi16(mmPix, mmCoeff[coefIdx]);
    __m128i mm_mul = _mm_unpacklo_epi16(mm_lo, mm_hi);
    mm_sum = _mm_add_epi32(mm_sum, mm_mul);
  }
  mm_sum = _mm_sra_epi32(_mm_add_epi32(mm_sum, mmOffset), mmShift);
  mm_sum = _mm_packs_epi32(mm_sum, mm_zero);
  return(mm_sum);
}

template<X86_VEXT vext, bool isLast>
static void simdInterpolateN2_HIGHBIT_M4(const int16_t *src, const ptrdiff_t srcStride, int16_t *dst,
                                         const ptrdiff_t dstStride, const ptrdiff_t cStride, int width, int height,
                                         int shift, int offset, const ClpRng &clpRng, int16_t const *c)
{
#if USE_AVX2
  __m256i mm256Offset = _mm256_set1_epi32(offset);
  __m256i mm256Coeff[2];
  for (int n = 0; n < 2; n++)
  {
    mm256Coeff[n] = _mm256_set1_epi16(c[n]);
  }
#endif
  __m128i mmOffset = _mm_set1_epi32(offset);
  __m128i mmCoeff[2];
  for (int n = 0; n < 2; n++)
  {
    mmCoeff[n] = _mm_set1_epi16(c[n]);
  }

  __m128i mmShift = _mm_cvtsi64_si128(shift);

  CHECK(isLast, "Not Supported");
  CHECK(width % 4 != 0, "Not Supported");

  for (int row = 0; row < height; row++)
  {
    int col = 0;
#if USE_AVX2
    for (; col < ((width >> 4) << 4); col += 16)
    {
      __m256i mmFiltered = simdInterpolateLumaHighBit2P16(src + col, cStride, mm256Coeff, mm256Offset, mmShift);
      _mm256_storeu_si256((__m256i *)(dst + col), mmFiltered);
    }
#endif
    for (; col < ((width >> 3) << 3); col += 8)
    {
      __m128i mmFiltered = simdInterpolateLumaHighBit2P8(src + col, cStride, mmCoeff, mmOffset, mmShift);
      _mm_storeu_si128((__m128i *)(dst + col), mmFiltered);
    }

    for (; col < ((width >> 2) << 2); col += 4)
    {
      __m128i mmFiltered = simdInterpolateLumaHighBit2P4(src + col, cStride, mmCoeff, mmOffset, mmShift);
      _mm_storel_epi64((__m128i *)(dst + col), mmFiltered);
    }
    src += srcStride;
    dst += dstStride;
  }
}

template<X86_VEXT vext, bool isLast>
static void simdInterpolateN2_10BIT_M4(const int16_t *src, const ptrdiff_t srcStride, int16_t *dst,
                                       const ptrdiff_t dstStride, const ptrdiff_t cStride, int width, int height,
                                       int shift, int offset, const ClpRng &clpRng, int16_t const *c)
{
  int row, col;
  __m128i mmOffset = _mm_set1_epi16(offset);
  __m128i mmShift = _mm_set_epi64x(0, shift);
  __m128i mmCoeff[2];
  for (int n = 0; n < 2; n++)
  {
    mmCoeff[n] = _mm_set1_epi16(c[n]);
  }

  CHECK(isLast, "Not Supported");

#if USE_AVX2
  __m256i mm256Offset = _mm256_set1_epi16(offset);
  __m256i mm256Coeff[2];
  for (int n = 0; n < 2; n++)
  {
    mm256Coeff[n] = _mm256_set1_epi16(c[n]);
  }
#endif
  for (row = 0; row < height; row++)
  {
    col = 0;
#if USE_AVX2
    // multiple of 16
    for (; col < ((width >> 4) << 4); col += 16)
    {
      __m256i mmFiltered = simdInterpolateLuma10Bit2P16(src + col, cStride, mm256Coeff, mm256Offset, mmShift);
      _mm256_storeu_si256((__m256i *)(dst + col), mmFiltered);
    }
#endif
    // multiple of 8
    for (; col < ((width >> 3) << 3); col += 8)
    {
      __m128i mmFiltered = simdInterpolateLuma10Bit2P8(src + col, cStride, mmCoeff, mmOffset, mmShift);
      _mm_storeu_si128((__m128i *)(dst + col), mmFiltered);
    }

    // last 4 samples
    __m128i mmFiltered = simdInterpolateLuma10Bit2P4(src + col, cStride, mmCoeff, mmOffset, mmShift);
    _mm_storel_epi64((__m128i *)(dst + col), mmFiltered);
    src += srcStride;
    dst += dstStride;
  }
}
#if RExt__HIGH_BIT_DEPTH_SUPPORT
template<X86_VEXT vext, int N, bool CLAMP>
static void simdInterpolateHorM8_HBD(const Pel *src, const ptrdiff_t srcStride, Pel *dst, const ptrdiff_t dstStride,
                                     int width, int height, int shift, int offset, const ClpRng &clpRng,
                                     Pel const *coeff)
{
  static_assert(N == 2 || N == 4 || N == 6 || N == 8, "only filter sizes 2, 4, 6, and 8 are supported");
  static_assert(sizeof(Pel) == 4, "samples must be 32 bits wide");

  std::array<ptrdiff_t, 3> memOffsets = { { 2 * srcStride, 2 * srcStride + (width >> 1),
                                            2 * srcStride + width - 8 + (N / 2 + 1) / 2 * 4 + 7 } };

  for (auto &off: memOffsets)
  {
    _mm_prefetch((const char *) (src - srcStride + off), _MM_HINT_T0);
  }

  const __m128i minVal = _mm_set1_epi32(clpRng.min);
  const __m128i maxVal = _mm_set1_epi32(clpRng.max);

  __m128i coeffs[N];

  for (int k = 0; k < N; k++)
  {
    coeffs[k] = _mm_set1_epi32(coeff[k]);
  }

  for (ptrdiff_t row = 0; row < height; row++)
  {
    for (auto &off: memOffsets)
    {
      _mm_prefetch((const char *) (src + row * srcStride + off), _MM_HINT_T0);
    }

    for (ptrdiff_t col = 0; col < width; col += 8)
    {
      __m128i sum0 = _mm_set1_epi32(offset);
      __m128i sum1 = _mm_set1_epi32(offset);

      for (ptrdiff_t k = 0; k < N; k++)
      {
        const __m128i a = _mm_loadu_si128((const __m128i *) (src + row * srcStride + col + k));
        const __m128i b = _mm_loadu_si128((const __m128i *) (src + row * srcStride + col + k + 4));

        sum0 = _mm_add_epi32(sum0, _mm_mullo_epi32(a, coeffs[k]));
        sum1 = _mm_add_epi32(sum1, _mm_mullo_epi32(b, coeffs[k]));
      }

      sum0 = _mm_sra_epi32(sum0, _mm_cvtsi32_si128(shift));
      sum1 = _mm_sra_epi32(sum1, _mm_cvtsi32_si128(shift));

      if (CLAMP)
      {
        sum0 = _mm_min_epi32(sum0, maxVal);
        sum0 = _mm_max_epi32(sum0, minVal);
        sum1 = _mm_min_epi32(sum1, maxVal);
        sum1 = _mm_max_epi32(sum1, minVal);
      }

      _mm_storeu_si128((__m128i *) (dst + row * dstStride + col), sum0);
      _mm_storeu_si128((__m128i *) (dst + row * dstStride + col + 4), sum1);
    }
  }
}

template<X86_VEXT vext, int N, bool CLAMP>
static void simdInterpolateHorM8_HBD_AVX2(const Pel *src, const ptrdiff_t srcStride, Pel *dst,
                                          const ptrdiff_t dstStride, int width, int height, int shift, int offset,
                                          const ClpRng &clpRng, Pel const *coeff)
{
#if USE_AVX2
  static_assert(N == 2 || N == 4 || N == 6 || N == 8, "only filter sizes 2, 4, 6, and 8 are supported");
  static_assert(sizeof(Pel) == 4, "samples must be 32 bits wide");

  std::array<ptrdiff_t, 3> memOffsets = { { 2 * srcStride, 2 * srcStride + (width >> 1),
                                            2 * srcStride + width - 8 + (N / 2 + 1) / 2 * 4 + 7 } };

  for (auto &off: memOffsets)
  {
    _mm_prefetch((const char *) (src - srcStride + off), _MM_HINT_T0);
  }

  const __m256i minVal = _mm256_set1_epi32(clpRng.min);
  const __m256i maxVal = _mm256_set1_epi32(clpRng.max);

  __m256i coeffs[N];

  for (int k = 0; k < N; k++)
  {
    coeffs[k] = _mm256_set1_epi32(coeff[k]);
  }

  for (ptrdiff_t row = 0; row < height; row++)
  {
    for (auto &off: memOffsets)
    {
      _mm_prefetch((const char *) (src + row * srcStride + off), _MM_HINT_T0);
    }

    for (ptrdiff_t col = 0; col < width; col += 8)
    {
      __m256i sum = _mm256_set1_epi32(offset);

      for (ptrdiff_t k = 0; k < N; k++)
      {
        const __m256i a = _mm256_loadu_si256((const __m256i *) (src + row * srcStride + col + k));

        sum = _mm256_add_epi32(sum, _mm256_mullo_epi32(a, coeffs[k]));
      }

      sum = _mm256_sra_epi32(sum, _mm_cvtsi32_si128(shift));

      if (CLAMP)
      {
        sum = _mm256_min_epi32(sum, maxVal);
        sum = _mm256_max_epi32(sum, minVal);
      }

      _mm256_storeu_si256((__m256i *) (dst + row * dstStride + col), sum);
    }
  }
#endif
}

template<X86_VEXT vext, int N, bool shiftBack>
static void simdInterpolateVerM8_HBD(const Pel *src, const ptrdiff_t srcStride, Pel *dst, const ptrdiff_t dstStride,
                                     int width, int height, int shift, int offset, const ClpRng &clpRng,
                                     Pel const *coeff)
{
  const Pel *srcOrig = src;
  Pel *dstOrig = dst;

  __m128i vcoeff[N], vsrc0[N], vsrc1[N];
  __m128i vzero = _mm_setzero_si128();
  __m128i voffset = _mm_set1_epi32(offset);
  __m128i vibdimin = _mm_set1_epi32(clpRng.min);
  __m128i vibdimax = _mm_set1_epi32(clpRng.max);

  __m128i vsuma, vsumb;
  for (int i = 0; i < N; i++)
  {
    vcoeff[i] = _mm_set1_epi32(coeff[i]);
  }

  for (int col = 0; col < width; col += 8)
  {
    for (int i = 0; i < N - 1; i++)
    {
      vsrc0[i] = _mm_lddqu_si128((__m128i const *)&src[col + i * srcStride]);
      vsrc1[i] = _mm_lddqu_si128((__m128i const *)&src[col + 4 + i * srcStride]);
    }

    for (int row = 0; row < height; row++)
    {
      vsrc0[N - 1] = _mm_lddqu_si128((__m128i const *)&src[col + (N - 1) * srcStride]);
      vsrc1[N - 1] = _mm_lddqu_si128((__m128i const *)&src[col + 4 + (N - 1) * srcStride]);

      vsuma = vsumb = vzero;
      for (int i = 0; i < N; i++)
      {
        vsuma = _mm_add_epi32(vsuma, _mm_mullo_epi32(vsrc0[i], vcoeff[i]));
        vsumb = _mm_add_epi32(vsumb, _mm_mullo_epi32(vsrc1[i], vcoeff[i]));
      }

      for (int i = 0; i < N - 1; i++)
      {
        vsrc0[i] = vsrc0[i + 1];
        vsrc1[i] = vsrc1[i + 1];
      }

      vsuma = _mm_add_epi32(vsuma, voffset);
      vsumb = _mm_add_epi32(vsumb, voffset);

      vsuma = _mm_srai_epi32(vsuma, shift);
      vsumb = _mm_srai_epi32(vsumb, shift);

      if (shiftBack) //clip
      {
        vsuma = _mm_min_epi32(vibdimax, _mm_max_epi32(vibdimin, vsuma));
        vsumb = _mm_min_epi32(vibdimax, _mm_max_epi32(vibdimin, vsumb));
      }

      _mm_storeu_si128((__m128i *)&dst[col], vsuma);
      _mm_storeu_si128((__m128i *)&dst[col + 4], vsumb);

      src += srcStride;
      dst += dstStride;
    }
    src = srcOrig;
    dst = dstOrig;
  }
}

template<X86_VEXT vext, int N, bool shiftBack>
static void simdInterpolateVerM8_HBD_AVX2(const Pel *src, const ptrdiff_t srcStride, Pel *dst,
                                          const ptrdiff_t dstStride, int width, int height, int shift, int offset,
                                          const ClpRng &clpRng, Pel const *coeff)
{
#ifdef USE_AVX2
  __m256i voffset = _mm256_set1_epi32(offset);
  __m256i vibdimin = _mm256_set1_epi32(clpRng.min);
  __m256i vibdimax = _mm256_set1_epi32(clpRng.max);

  __m256i vsrc[N], vcoeff[N];
  for (int i = 0; i < N; i++)
  {
    vcoeff[i] = _mm256_set1_epi32(coeff[i]);
  }

  const Pel *srcOrig = src;
  Pel *dstOrig = dst;

  for (int col = 0; col < width; col += 8)
  {
    for (int i = 0; i < N - 1; i++)
    {
      vsrc[i] = _mm256_loadu_si256((const __m256i *)&src[col + i * srcStride]);
    }

    for (int row = 0; row < height; row++)
    {
      vsrc[N - 1] = _mm256_loadu_si256((const __m256i *)&src[col + (N - 1) * srcStride]);

      __m256i vsum = _mm256_setzero_si256();
      for (int i = 0; i < N; i++)
      {
        vsum = _mm256_add_epi32(vsum, _mm256_mullo_epi32(vsrc[i], vcoeff[i]));
      }

      for (int i = 0; i < N - 1; i++)
      {
        vsrc[i] = vsrc[i + 1];
      }

      vsum = _mm256_add_epi32(vsum, voffset);
      vsum = _mm256_srai_epi32(vsum, shift);

      if (shiftBack)
      {
        vsum = _mm256_min_epi32(vibdimax, _mm256_max_epi32(vibdimin, vsum));
      }
      _mm256_storeu_si256((__m256i *)&dst[col], vsum);

      src += srcStride;
      dst += dstStride;
    }
    src = srcOrig;
    dst = dstOrig;
  }
#endif
}

template<X86_VEXT vext, int N, bool CLAMP>
static void simdInterpolateHorM4_HBD(const Pel *src, const ptrdiff_t srcStride, Pel *dst, const ptrdiff_t dstStride,
                                     int width, int height, int shift, int offset, const ClpRng &clpRng,
                                     Pel const *coeff)
{
  static_assert(N == 2 || N == 4 || N == 6 || N == 8, "only filter sizes 2, 4, 6, and 8 are supported");
  static_assert(sizeof(Pel) == 4, "samples must be 32 bits wide");

  const __m128i minVal = _mm_set1_epi32(clpRng.min);
  const __m128i maxVal = _mm_set1_epi32(clpRng.max);

  __m128i coeffs[N];

  for (int k = 0; k < N; k++)
  {
    coeffs[k] = _mm_set1_epi32(coeff[k]);
  }

  for (ptrdiff_t row = 0; row < height; row++)
  {
    for (ptrdiff_t col = 0; col < width; col += 4)
    {
      __m128i sum = _mm_set1_epi32(offset);

      for (ptrdiff_t k = 0; k < N; k++)
      {
        const __m128i a = _mm_loadu_si128((const __m128i *) (src + row * srcStride + col + k));

        sum = _mm_add_epi32(sum, _mm_mullo_epi32(a, coeffs[k]));
      }

      sum = _mm_sra_epi32(sum, _mm_cvtsi32_si128(shift));

      if (CLAMP)
      {
        sum = _mm_min_epi32(sum, maxVal);
        sum = _mm_max_epi32(sum, minVal);
      }

      _mm_storeu_si128((__m128i *) (dst + row * dstStride + col), sum);
    }
  }
}

template<X86_VEXT vext, int N, bool shiftBack>
static void simdInterpolateVerM4_HBD(const Pel *src, const ptrdiff_t srcStride, Pel *dst, const ptrdiff_t dstStride,
                                     int width, int height, int shift, int offset, const ClpRng &clpRng,
                                     Pel const *coeff)
{
  const Pel *srcOrig = src;
  Pel *dstOrig = dst;

  __m128i vsum, vcoeff[N], vsrc[N];
  __m128i vzero = _mm_setzero_si128();
  __m128i voffset = _mm_set1_epi32(offset);
  __m128i vibdimin = _mm_set1_epi32(clpRng.min);
  __m128i vibdimax = _mm_set1_epi32(clpRng.max);
  for (int i = 0; i < N; i++)
  {
    vcoeff[i] = _mm_set1_epi32(coeff[i]);
  }

  for (int col = 0; col < width; col += 4)
  {
    for (int i = 0; i < N - 1; i++)
    {
      vsrc[i] = _mm_lddqu_si128((__m128i const *)&src[col + i * srcStride]);
    }

    for (int row = 0; row < height; row++)
    {
      vsrc[N - 1] = _mm_lddqu_si128((__m128i const *)&src[col + (N - 1) * srcStride]);

      vsum = vzero;
      for (int i = 0; i < N; i++)
      {
        vsum = _mm_add_epi32(vsum, _mm_mullo_epi32(vsrc[i], vcoeff[i]));
      }

      vsum = _mm_add_epi32(vsum, voffset);
      vsum = _mm_srai_epi32(vsum, shift);

      if (shiftBack)
      {
        vsum = _mm_min_epi32(vibdimax, _mm_max_epi32(vibdimin, vsum));
      }

      _mm_storeu_si128((__m128i *)&dst[col], vsum);

      for (int i = 0; i < N - 1; i++)
      {
        vsrc[i] = vsrc[i + 1];
      }

      src += srcStride;
      dst += dstStride;
    }
    src = srcOrig;
    dst = dstOrig;
  }
}

template<X86_VEXT vext, bool isLast>
static void simdInterpolateN2_M8_HBD(const Pel *src, const ptrdiff_t srcStride, Pel *dst, const ptrdiff_t dstStride,
                                     const ptrdiff_t cStride, int width, int height, int shift, int offset,
                                     const ClpRng &clpRng, Pel const *c)
{
  int row, col;
  __m128i mmOffset = _mm_set1_epi32(offset);
  __m128i mmMin = _mm_set1_epi32(clpRng.min);
  __m128i mmMax = _mm_set1_epi32(clpRng.max);
  __m128i mmCoeff[2];
  for (int n = 0; n < 2; n++)
  {
    mmCoeff[n] = _mm_set1_epi32(c[n]);
  }

  for (row = 0; row < height; row++)
  {
    for (col = 0; col < width; col += 8)
    {
      const Pel* src_tmp = src;
      __m128i vsuma = _mm_setzero_si128();
      __m128i vsumb = _mm_setzero_si128();

      for (int i = 0; i < 2; i++)
      {
        __m128i vsrc0 = _mm_lddqu_si128((__m128i *)&src_tmp[col]);
        __m128i vsrc1 = _mm_lddqu_si128((__m128i *)&src_tmp[col + 4]);
        vsuma = _mm_add_epi32(vsuma, _mm_mullo_epi32(vsrc0, mmCoeff[i]));
        vsumb = _mm_add_epi32(vsumb, _mm_mullo_epi32(vsrc1, mmCoeff[i]));
        src_tmp += cStride;
      }

      vsuma = _mm_srai_epi32(_mm_add_epi32(vsuma, mmOffset), shift);
      vsumb = _mm_srai_epi32(_mm_add_epi32(vsumb, mmOffset), shift);
      if (isLast)
      {
        vsuma = _mm_min_epi32(mmMax, _mm_max_epi32(mmMin, vsuma));
        vsumb = _mm_min_epi32(mmMax, _mm_max_epi32(mmMin, vsumb));
      }

      _mm_storeu_si128((__m128i *)&dst[col], vsuma);
      _mm_storeu_si128((__m128i *)&dst[col + 4], vsumb);
    }
    src += srcStride;
    dst += dstStride;
  }
}

template<X86_VEXT vext, bool isLast>
static void simdInterpolateN2_M4_HBD(const Pel *src, const ptrdiff_t srcStride, Pel *dst, const ptrdiff_t dstStride,
                                     const ptrdiff_t cStride, int width, int height, int shift, int offset,
                                     const ClpRng &clpRng, Pel const *c)
{
  int row, col;
  __m128i mmOffset = _mm_set1_epi32(offset);
  __m128i mmMin = _mm_set1_epi32(clpRng.min);
  __m128i mmMax = _mm_set1_epi32(clpRng.max);
  __m128i mmCoeff[2];
  for (int n = 0; n < 2; n++)
  {
    mmCoeff[n] = _mm_set1_epi32(c[n]);
  }

  for (row = 0; row < height; row++)
  {
    for (col = 0; col < width; col += 4)
    {
      const Pel* src_tmp = src;
      __m128i vsum = _mm_setzero_si128();

      for (int i = 0; i < 2; i++)
      {
        __m128i vsrc = _mm_lddqu_si128((__m128i *)&src_tmp[col]);
        vsum = _mm_add_epi32(vsum, _mm_mullo_epi32(vsrc, mmCoeff[i]));
        src_tmp += cStride;
      }

      vsum = _mm_srai_epi32(_mm_add_epi32(vsum, mmOffset), shift);
      if (isLast)
      {
        vsum = _mm_min_epi32(mmMax, _mm_max_epi32(mmMin, vsum));
      }

      _mm_storeu_si128((__m128i *)&dst[col], vsum);
    }
    src += srcStride;
    dst += dstStride;
  }
}

template<X86_VEXT vext, bool isLast>
static void simdInterpolateN2_HBD_M4(const Pel *src, const ptrdiff_t srcStride, Pel *dst, const ptrdiff_t dstStride,
                                     const ptrdiff_t cStride, int width, int height, int shift, int offset,
                                     const ClpRng &clpRng, Pel const *c)
{
  CHECK(isLast, "Not Supported");
  CHECK(width % 4 != 0, "Not Supported");

  __m128i mmOffset = _mm_set1_epi32(offset);
  __m128i mmCoeff[2];
  for (int n = 0; n < 2; n++)
  {
    mmCoeff[n] = _mm_set1_epi32(c[n]);
  }

  for (int row = 0; row < height; row++)
  {
    for (int col = 0; col < width; col += 4)
    {
      const Pel* src_tmp = src;
      __m128i vsum = _mm_setzero_si128();
      for (int i = 0; i < 2; i++)
      {
        __m128i vsrc = _mm_lddqu_si128((__m128i *)&src_tmp[col]);
        vsum = _mm_add_epi32(vsum, _mm_mullo_epi32(vsrc, mmCoeff[i]));
        src_tmp += cStride;
      }
      vsum = _mm_srai_epi32(_mm_add_epi32(vsum, mmOffset), shift);
      _mm_storeu_si128((__m128i *)&dst[col], vsum);
    }
    src += srcStride;
    dst += dstStride;
  }
}

template<X86_VEXT vext, bool isLast>
static void simdInterpolateN2_HBD_M4_AVX2(const Pel *src, const ptrdiff_t srcStride, Pel *dst,
                                          const ptrdiff_t dstStride, const ptrdiff_t cStride, int width, int height,
                                          int shift, int offset, const ClpRng &clpRng, Pel const *c)
{
#ifdef USE_AVX2
  CHECK(isLast, "Not Supported");
  CHECK(width % 4 != 0, "Not Supported");

  __m256i mmOffset = _mm256_set1_epi32(offset);
  __m256i mmCoeff[2];
  for (int n = 0; n < 2; n++)
  {
    mmCoeff[n] = _mm256_set1_epi32(c[n]);
  }

  ptrdiff_t srcStride2 = (srcStride << 1);
  ptrdiff_t dstStride2 = (dstStride << 1);

  for (int row = 0; row < height; row += 2)
  {
    for (int col = 0; col < width; col += 4)
    {
      const Pel* src_tmp = src;
      __m256i vsum = _mm256_setzero_si256();
      for (int i = 0; i < 2; i++)
      {
        __m256i vsrc = _mm256_castsi128_si256(_mm_lddqu_si128((__m128i *)&src_tmp[col]));
        vsrc = _mm256_inserti128_si256(vsrc, _mm_lddqu_si128((__m128i *)&src_tmp[col + srcStride]), 1);
        vsum = _mm256_add_epi32(vsum, _mm256_mullo_epi32(vsrc, mmCoeff[i]));
        src_tmp += cStride;
      }
      vsum = _mm256_srai_epi32(_mm256_add_epi32(vsum, mmOffset), shift);

      _mm_storeu_si128((__m128i *)&dst[col], _mm256_castsi256_si128(vsum));
      _mm_storeu_si128((__m128i *)&dst[col + dstStride], _mm256_castsi256_si128(_mm256_permute4x64_epi64(vsum, 0xee)));
    }
    src += srcStride2;
    dst += dstStride2;
  }
#endif
}

template<X86_VEXT vext, bool isFirst, bool isLast>
static void simdFilterCopy_HBD(const ClpRng &clpRng, const Pel *src, const ptrdiff_t srcStride, Pel *dst,
                               const ptrdiff_t dstStride, int width, int height, bool biMCForDMVR)
{
  int row;

  if (isFirst == isLast)
  {
    for (row = 0; row < height; row++)
    {
      memcpy(&dst[0], &src[0], width * sizeof(Pel));
      src += srcStride;
      dst += dstStride;
    }
  }
  else if (isFirst)
  {
    if (width & 1)
    {
      InterpolationFilter::filterCopy<isFirst, isLast>(clpRng, src, srcStride, dst, dstStride, width, height,
                                                       biMCForDMVR);
      return;
    }

    if (biMCForDMVR)
    {
      int shift10BitOut = (clpRng.bd - IF_INTERNAL_PREC_BILINEAR);
      if (shift10BitOut <= 0)
      {
        const __m128i shift = _mm_cvtsi32_si128(-shift10BitOut);
        for (row = 0; row < height; row++)
        {
          int col = 0;
#ifdef USE_AVX2
          if (vext >= AVX2)
          {
            for (; col < ((width >> 3) << 3); col += 8)
            {
              __m256i val = _mm256_lddqu_si256((__m256i *) &src[col]);
              val         = _mm256_sll_epi32(val, shift);
              _mm256_storeu_si256((__m256i *) &dst[col], val);
            }
          }
#endif

          for (; col < ((width >> 2) << 2); col += 4)
          {
            __m128i val = _mm_lddqu_si128((__m128i *) &src[col]);
            val         = _mm_sll_epi32(val, shift);
            _mm_storeu_si128((__m128i *) &dst[col], val);
          }

          for (; col < width; col += 2)
          {
            __m128i val = _mm_loadl_epi64((__m128i *) &src[col]);
            val         = _mm_sll_epi32(val, shift);
            _mm_storel_epi64((__m128i *) &dst[col], val);
          }
          src += srcStride;
          dst += dstStride;
        }
      }
      else
      {
        int offset = (1 << (shift10BitOut - 1));
        for (row = 0; row < height; row++)
        {
          int col = 0;
#ifdef USE_AVX2
          if (vext >= AVX2)
          {
            __m256i mm256_offset = _mm256_set1_epi32(offset);
            for (; col < ((width >> 3) << 3); col += 8)
            {
              __m256i vsrc = _mm256_lddqu_si256((__m256i *) &src[col]);
              vsrc         = _mm256_srai_epi32(_mm256_add_epi32(vsrc, mm256_offset), shift10BitOut);
              _mm256_storeu_si256((__m256i *) &dst[col], vsrc);
            }
          }
#endif

          __m128i mm128_offset = _mm_set1_epi32(offset);
          for (; col < ((width >> 2) << 2); col += 4)
          {
            __m128i vsrc = _mm_lddqu_si128((__m128i *) &src[col]);
            vsrc         = _mm_srai_epi32(_mm_add_epi32(vsrc, mm128_offset), shift10BitOut);
            _mm_storeu_si128((__m128i *) &dst[col], vsrc);
          }

          for (; col < width; col += 2)
          {
            __m128i vsrc = _mm_loadl_epi64((__m128i *) &src[col]);
            vsrc         = _mm_srai_epi32(_mm_add_epi32(vsrc, mm128_offset), shift10BitOut);
            _mm_storel_epi64((__m128i *) &dst[col], vsrc);
          }
          src += srcStride;
          dst += dstStride;
        }
      }
    }
    else
    {
      const int shift = IF_INTERNAL_FRAC_BITS(clpRng.bd);
      for (row = 0; row < height; row++)
      {
        int col = 0;
#ifdef USE_AVX2
        if (vext >= AVX2)
        {
          __m256i mm256_offset = _mm256_set1_epi32(IF_INTERNAL_OFFS);
          for (; col < ((width >> 3) << 3); col += 8)
          {
            __m256i vsrc = _mm256_lddqu_si256((__m256i *)&src[col]);
            vsrc = _mm256_sub_epi32(_mm256_slli_epi32(vsrc, shift), mm256_offset);
            _mm256_storeu_si256((__m256i *)&dst[col], vsrc);
          }
        }
#endif

        __m128i mm128_offset = _mm_set1_epi32(IF_INTERNAL_OFFS);
        for (; col < ((width >> 2) << 2); col += 4)
        {
          __m128i vsrc = _mm_lddqu_si128((__m128i *)&src[col]);
          vsrc = _mm_sub_epi32(_mm_slli_epi32(vsrc, shift), mm128_offset);
          _mm_storeu_si128((__m128i *)&dst[col], vsrc);
        }

        for (; col < width; col += 2)
        {
          __m128i vsrc = _mm_loadl_epi64((__m128i *)&src[col]);
          vsrc = _mm_sub_epi32(_mm_slli_epi32(vsrc, shift), mm128_offset);
          _mm_storel_epi64((__m128i *)&dst[col], vsrc);
        }
        src += srcStride;
        dst += dstStride;
      }
    }
  }
  else
  {
    if (width & 1)
    {
      InterpolationFilter::filterCopy<isFirst, isLast>(clpRng, src, srcStride, dst, dstStride, width, height,
                                                       biMCForDMVR);
      return;
    }

    CHECK(biMCForDMVR, "isLast must be false when biMCForDMVR is true");
    const int shift = IF_INTERNAL_FRAC_BITS(clpRng.bd);
    for (row = 0; row < height; row++)
    {
      int col = 0;
#ifdef USE_AVX2
      if (vext >= AVX2)
      {
        __m256i mm256_offset = _mm256_set1_epi32(IF_INTERNAL_OFFS);
        __m256i mm256_min    = _mm256_set1_epi32(clpRng.min);
        __m256i mm256_max    = _mm256_set1_epi32(clpRng.max);
        for (; col < ((width >> 3) << 3); col += 8)
        {
          __m256i vsrc = _mm256_lddqu_si256((__m256i *) &src[col]);
          vsrc         = _mm256_add_epi32(vsrc, mm256_offset);
          if (shift <= 0)
          {
            vsrc = _mm256_slli_epi32(vsrc, (-shift));
          }
          else
          {
            vsrc = _mm256_srai_epi32(_mm256_add_epi32(vsrc, _mm256_set1_epi32(1 << (shift - 1))), shift);
          }
          vsrc = _mm256_min_epi32(mm256_max, _mm256_max_epi32(mm256_min, vsrc));

          _mm256_storeu_si256((__m256i *) &dst[col], vsrc);
        }
      }
#endif

      __m128i mm128_offset = _mm_set1_epi32(IF_INTERNAL_OFFS);
      __m128i mm128_min    = _mm_set1_epi32(clpRng.min);
      __m128i mm128_max    = _mm_set1_epi32(clpRng.max);
      for (; col < ((width >> 2) << 2); col += 4)
      {
        __m128i vsrc = _mm_lddqu_si128((__m128i *) &src[col]);
        vsrc         = _mm_add_epi32(vsrc, mm128_offset);
        if (shift <= 0)
        {
          vsrc = _mm_slli_epi32(vsrc, (-shift));
        }
        else
        {
          vsrc = _mm_srai_epi32(_mm_add_epi32(vsrc, _mm_set1_epi32(1 << (shift - 1))), shift);
        }
        vsrc = _mm_min_epi32(mm128_max, _mm_max_epi32(mm128_min, vsrc));

        _mm_storeu_si128((__m128i *) &dst[col], vsrc);
      }

      for (; col < width; col += 2)
      {
        __m128i vsrc = _mm_loadl_epi64((__m128i *) &src[col]);
        vsrc         = _mm_add_epi32(vsrc, mm128_offset);
        if (shift <= 0)
        {
          vsrc = _mm_slli_epi32(vsrc, (-shift));
        }
        else
        {
          vsrc = _mm_srai_epi32(_mm_add_epi32(vsrc, _mm_set1_epi32(1 << (shift - 1))), shift);
        }
        vsrc = _mm_min_epi32(mm128_max, _mm_max_epi32(mm128_min, vsrc));

        _mm_storel_epi64((__m128i *) &dst[col], vsrc);
      }

      src += srcStride;
      dst += dstStride;
    }
  }
}

template< X86_VEXT vext >
void xWeightedGeoBlk_HBD_SIMD(const PredictionUnit &pu, const uint32_t width, const uint32_t height, const ComponentID compIdx, const uint8_t splitDir, PelUnitBuf& predDst, PelUnitBuf& predSrc0, PelUnitBuf& predSrc1)
{
  Pel* dst = predDst.get(compIdx).buf;
  Pel* src0 = predSrc0.get(compIdx).buf;
  Pel* src1 = predSrc1.get(compIdx).buf;
  ptrdiff_t strideDst  = predDst.get(compIdx).stride;
  ptrdiff_t strideSrc0 = predSrc0.get(compIdx).stride;
  ptrdiff_t strideSrc1 = predSrc1.get(compIdx).stride;

  const char    log2WeightBase = 3;
  const ClpRng  clpRng = pu.cu->slice->clpRngs().comp[compIdx];
  const int32_t shiftWeighted = IF_INTERNAL_FRAC_BITS(clpRng.bd) + log2WeightBase;
  const int32_t offsetWeighted = (1 << (shiftWeighted - 1)) + (IF_INTERNAL_OFFS << log2WeightBase);

  int16_t wIdx = floorLog2(pu.lwidth()) - GEO_MIN_CU_LOG2;
  int16_t hIdx = floorLog2(pu.lheight()) - GEO_MIN_CU_LOG2;

  const int angle = g_geoParams[splitDir].angleIdx;

  int16_t stepY = 0;
  int16_t* weight = nullptr;

  const int16_t *wOffset = g_weightOffset[splitDir][hIdx][wIdx];

  if (g_angle2mirror[angle] == 2)
  {
    stepY = -GEO_WEIGHT_MASK_SIZE;
    weight = &g_globalGeoWeights[g_angle2mask[angle]]
                                [(GEO_WEIGHT_MASK_SIZE - 1 - wOffset[1]) * GEO_WEIGHT_MASK_SIZE + wOffset[0]];
  }
  else if (g_angle2mirror[angle] == 1)
  {
    stepY = GEO_WEIGHT_MASK_SIZE;
    weight = &g_globalGeoWeights[g_angle2mask[angle]]
                                [wOffset[1] * GEO_WEIGHT_MASK_SIZE + (GEO_WEIGHT_MASK_SIZE - 1 - wOffset[0])];
  }
  else
  {
    stepY = GEO_WEIGHT_MASK_SIZE;
    weight = &g_globalGeoWeights[g_angle2mask[angle]][wOffset[1] * GEO_WEIGHT_MASK_SIZE + wOffset[0]];
  }

  const __m128i mmEight = _mm_set1_epi16(8);
  const __m128i mmOffset = _mm_set1_epi32(offsetWeighted);
  const __m128i mmShift = _mm_cvtsi32_si128(shiftWeighted);
  const __m128i mmMin = _mm_set1_epi32(clpRng.min);
  const __m128i mmMax = _mm_set1_epi32(clpRng.max);

  if (compIdx != COMPONENT_Y && pu.chromaFormat == ChromaFormat::_420)
  {
    stepY <<= 1;
  }

  if (width == 4)
  {
    // it will occur to chroma only
    for (int y = 0; y < height; y++)
    {
      __m128i s0 = _mm_lddqu_si128((__m128i *) (src0));
      __m128i s1 = _mm_lddqu_si128((__m128i *) (src1));
      __m128i w0;
      if (g_angle2mirror[angle] == 1)
      {
        w0 = _mm_loadu_si128((__m128i *) (weight - (8 - 1)));
        const __m128i shuffle_mask = _mm_set_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
        w0 = _mm_shuffle_epi8(w0, shuffle_mask);
      }
      else
      {
        w0 = _mm_loadu_si128((__m128i *) (weight));
      }
      w0 = _mm_shuffle_epi8(w0, _mm_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13, 0, 0, 0, 0, 0, 0, 0, 0));
      __m128i w1 = _mm_sub_epi16(mmEight, w0);

      w0 = _mm_cvtepi16_epi32(w0);
      w1 = _mm_cvtepi16_epi32(w1);

      s0 = _mm_add_epi32(_mm_mullo_epi32(s0, w0), _mm_mullo_epi32(s1, w1));
      s0 = _mm_sra_epi32(_mm_add_epi32(s0, mmOffset), mmShift);
      s0 = _mm_min_epi32(mmMax, _mm_max_epi32(s0, mmMin));

      _mm_storeu_si128((__m128i *)dst, s0);

      dst += strideDst;
      src0 += strideSrc0;
      src1 += strideSrc1;
      weight += stepY;
    }
  }
#ifdef USE_AVX2
  else if ((vext >= AVX2) && (width >= 16))
  {
    const __m256i mmEightAVX2 = _mm256_set1_epi16(8);
    const __m256i mmOffsetAVX2 = _mm256_set1_epi32(offsetWeighted);
    const __m256i mmMinAVX2 = _mm256_set1_epi32(clpRng.min);
    const __m256i mmMaxAVX2 = _mm256_set1_epi32(clpRng.max);
    for (int y = 0; y < height; y++)
    {
      for (int x = 0; x < width; x += 16)
      {
        __m256i s00 = _mm256_lddqu_si256((__m256i *) (src0 + x));
        __m256i s01 = _mm256_lddqu_si256((__m256i *) (src0 + x + 8));
        __m256i s10 = _mm256_lddqu_si256((__m256i *) (src1 + x));
        __m256i s11 = _mm256_lddqu_si256((__m256i *) (src1 + x + 8));

        __m256i w0 = _mm256_lddqu_si256((__m256i *) (weight + x));
        if (compIdx != COMPONENT_Y && pu.chromaFormat != ChromaFormat::_444)
        {
          const __m256i mask = _mm256_set_epi16(0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1);
          __m256i w0p0, w0p1;
          if (g_angle2mirror[angle] == 1)
          {
            w0p0 = _mm256_lddqu_si256((__m256i *) (weight - (x << 1) - (16 - 1))); // first sub-sample the required weights.
            w0p1 = _mm256_lddqu_si256((__m256i *) (weight - (x << 1) - 16 - (16 - 1)));
            const __m256i shuffle_mask = _mm256_set_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
            w0p0 = _mm256_shuffle_epi8(w0p0, shuffle_mask);
            w0p0 = _mm256_permute4x64_epi64(w0p0, _MM_SHUFFLE(1, 0, 3, 2));
            w0p1 = _mm256_shuffle_epi8(w0p1, shuffle_mask);
            w0p1 = _mm256_permute4x64_epi64(w0p1, _MM_SHUFFLE(1, 0, 3, 2));
          }
          else
          {
            w0p0 = _mm256_lddqu_si256((__m256i *) (weight + (x << 1))); // first sub-sample the required weights.
            w0p1 = _mm256_lddqu_si256((__m256i *) (weight + (x << 1) + 16));
          }
          w0p0 = _mm256_mullo_epi16(w0p0, mask);
          w0p1 = _mm256_mullo_epi16(w0p1, mask);
          w0 = _mm256_packs_epi16(w0p0, w0p1);
          w0 = _mm256_permute4x64_epi64(w0, _MM_SHUFFLE(3, 1, 2, 0));
        }
        else
        {
          if (g_angle2mirror[angle] == 1)
          {
            w0 = _mm256_lddqu_si256((__m256i *) (weight - x - (16 - 1)));
            const __m256i shuffle_mask = _mm256_set_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
            w0 = _mm256_shuffle_epi8(w0, shuffle_mask);
            w0 = _mm256_permute4x64_epi64(w0, _MM_SHUFFLE(1, 0, 3, 2));
          }
          else
          {
            w0 = _mm256_lddqu_si256((__m256i *) (weight + x));
          }
        }
        __m256i w1 = _mm256_sub_epi16(mmEightAVX2, w0);

        __m256i w00 = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(w0));
        __m256i w01 = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(_mm256_permute4x64_epi64(w0, 0xee)));
        __m256i w10 = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(w1));
        __m256i w11 = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(_mm256_permute4x64_epi64(w1, 0xee)));

        __m256i s0 = _mm256_add_epi32(_mm256_mullo_epi32(s00, w00), _mm256_mullo_epi32(s10, w10));
        __m256i s1 = _mm256_add_epi32(_mm256_mullo_epi32(s01, w01), _mm256_mullo_epi32(s11, w11));

        s0 = _mm256_sra_epi32(_mm256_add_epi32(s0, mmOffsetAVX2), mmShift);
        s1 = _mm256_sra_epi32(_mm256_add_epi32(s1, mmOffsetAVX2), mmShift);

        s0 = _mm256_min_epi32(mmMaxAVX2, _mm256_max_epi32(s0, mmMinAVX2));
        s1 = _mm256_min_epi32(mmMaxAVX2, _mm256_max_epi32(s1, mmMinAVX2));

        _mm256_storeu_si256((__m256i *) (dst + x), s0);
        _mm256_storeu_si256((__m256i *) (dst + x + 8), s1);
      }

      dst += strideDst;
      src0 += strideSrc0;
      src1 += strideSrc1;
      weight += stepY;
    }
  }
#endif
  else
  {
    for (int y = 0; y < height; y++)
    {
      for (int x = 0; x < width; x += 8)
      {
        __m128i s00 = _mm_lddqu_si128((__m128i *) (src0 + x));
        __m128i s01 = _mm_lddqu_si128((__m128i *) (src0 + x + 4));
        __m128i s10 = _mm_lddqu_si128((__m128i *) (src1 + x));
        __m128i s11 = _mm_lddqu_si128((__m128i *) (src1 + x + 4));
        __m128i w0;
        if (compIdx != COMPONENT_Y && pu.chromaFormat != ChromaFormat::_444)
        {
          const __m128i mask = _mm_set_epi16(0, 1, 0, 1, 0, 1, 0, 1);
          __m128i w0p0, w0p1;
          if (g_angle2mirror[angle] == 1)
          {
            w0p0 = _mm_lddqu_si128((__m128i *) (weight - (x << 1) - (8 - 1))); // first sub-sample the required weights.
            w0p1 = _mm_lddqu_si128((__m128i *) (weight - (x << 1) - 8 - (8 - 1)));
            const __m128i shuffle_mask = _mm_set_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
            w0p0 = _mm_shuffle_epi8(w0p0, shuffle_mask);
            w0p1 = _mm_shuffle_epi8(w0p1, shuffle_mask);
          }
          else
          {
            w0p0 = _mm_lddqu_si128((__m128i *) (weight + (x << 1))); // first sub-sample the required weights.
            w0p1 = _mm_lddqu_si128((__m128i *) (weight + (x << 1) + 8));
          }
          w0p0 = _mm_mullo_epi16(w0p0, mask);
          w0p1 = _mm_mullo_epi16(w0p1, mask);
          w0 = _mm_packs_epi32(w0p0, w0p1);
        }
        else
        {
          if (g_angle2mirror[angle] == 1)
          {
            w0 = _mm_lddqu_si128((__m128i *) (weight - x - (8 - 1)));  // 16b
            const __m128i shuffle_mask = _mm_set_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
            w0 = _mm_shuffle_epi8(w0, shuffle_mask);
          }
          else
          {
            w0 = _mm_lddqu_si128((__m128i *) (weight + x));
          }
        }
        __m128i w1 = _mm_sub_epi16(mmEight, w0);

        __m128i w00 = _mm_cvtepi16_epi32(w0);
        __m128i w01 = _mm_cvtepi16_epi32(_mm_shuffle_epi32(w0, 0xee));
        __m128i w10 = _mm_cvtepi16_epi32(w1);
        __m128i w11 = _mm_cvtepi16_epi32(_mm_shuffle_epi32(w1, 0xee));

        __m128i s0 = _mm_add_epi32(_mm_mullo_epi32(s00, w00), _mm_mullo_epi32(s10, w10));
        __m128i s1 = _mm_add_epi32(_mm_mullo_epi32(s01, w01), _mm_mullo_epi32(s11, w11));

        s0 = _mm_sra_epi32(_mm_add_epi32(s0, mmOffset), mmShift);
        s1 = _mm_sra_epi32(_mm_add_epi32(s1, mmOffset), mmShift);

        s0 = _mm_min_epi32(mmMax, _mm_max_epi32(s0, mmMin));
        s1 = _mm_min_epi32(mmMax, _mm_max_epi32(s1, mmMin));

        _mm_storeu_si128((__m128i *) (dst + x), s0);
        _mm_storeu_si128((__m128i *) (dst + x + 4), s1);
      }
      dst += strideDst;
      src0 += strideSrc0;
      src1 += strideSrc1;
      weight += stepY;
    }
  }
}
#endif
template<X86_VEXT vext, int N, bool VERTICAL, bool FIRST, bool LAST, bool biMCForDMVR>
static void simdFilter(const ClpRng &clpRng, Pel const *src, const ptrdiff_t srcStride, Pel *dst,
                       const ptrdiff_t dstStride, int width, int height, TFilterCoeff const *coeff)
{
  int row, col;

  Pel c[8];
  c[0] = coeff[0];
  c[1] = coeff[1];
  if( N >= 4 )
  {
    c[2] = coeff[2];
    c[3] = coeff[3];
  }
  if( N >= 6 )
  {
    c[4] = coeff[4];
    c[5] = coeff[5];
  }
  if( N == 8 )
  {
    c[6] = coeff[6];
    c[7] = coeff[7];
  }

  const ptrdiff_t cStride = (VERTICAL) ? srcStride : 1;
  src -= ( N/2 - 1 ) * cStride;

  int offset;
  int headRoom = IF_INTERNAL_FRAC_BITS(clpRng.bd);
  int shift    = IF_FILTER_PREC;
  // with the current settings (IF_INTERNAL_PREC = 14 and IF_FILTER_PREC = 6), though headroom can be
  // negative for bit depths greater than 14, shift will remain non-negative for bit depths of 8->20

  if (biMCForDMVR)
  {
    if (FIRST)
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
  else
  {
    if (LAST)
    {
      shift += (FIRST) ? 0 : headRoom;
      offset = 1 << (shift - 1);
      offset += (FIRST) ? 0 : IF_INTERNAL_OFFS << IF_FILTER_PREC;
    }
    else
    {
      shift -= (FIRST) ? headRoom : 0;
      offset = (FIRST) ? -(IF_INTERNAL_OFFS << shift) : 0;
    }
  }

  const bool widthMult8 = (width & 7) == 0;
  const bool widthMult4 = (width & 3) == 0;

  {
    if ((N == 8 || N == 6) && widthMult8)
    {
      if (!VERTICAL)
      {
#if RExt__HIGH_BIT_DEPTH_SUPPORT
        if (vext >= AVX2)
        {
          simdInterpolateHorM8_HBD_AVX2<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset,
                                                       clpRng, c);
        }
        else
        {
          simdInterpolateHorM8_HBD<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng,
                                                  c);
        }
#else
#ifdef USE_AVX2
        if( vext>= AVX2 )
        {
          simdInterpolateHorM8_AVX2<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng,
                                                   c);
        }
        else
#endif
        {
          simdInterpolateHorM8<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng, c);
        }
#endif
      }
      else
      {
#if RExt__HIGH_BIT_DEPTH_SUPPORT
        if (vext >= AVX2)
        {
          simdInterpolateVerM8_HBD_AVX2<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset,
                                                       clpRng, c);
        }
        else
        {
          simdInterpolateVerM8_HBD<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng,
                                                  c);
        }
#else
#ifdef USE_AVX2
        if( vext>= AVX2 )
        {
          simdInterpolateVerM8_AVX2<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng,
                                                   c);
        }
        else
#endif
        {
          simdInterpolateVerM8<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng, c);
        }
#endif
      }
      return;
    }
    else if ((N == 8 || N == 6) && widthMult4)
    {
      if (!VERTICAL)
      {
#if RExt__HIGH_BIT_DEPTH_SUPPORT
        simdInterpolateHorM4_HBD<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng,
                                                c);
#else
        simdInterpolateHorM4<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng, c);
#endif
      }
      else
      {
#if RExt__HIGH_BIT_DEPTH_SUPPORT
        simdInterpolateVerM4_HBD<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng,
                                                c);
#else
        simdInterpolateVerM4<vext, N, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng, c);
#endif
      }
      return;
    }
    else if (N == 4 && widthMult4)
    {
      if (!VERTICAL)
      {
        if (widthMult8)
        {
#if RExt__HIGH_BIT_DEPTH_SUPPORT
          if (vext >= AVX2)
          {
            simdInterpolateHorM8_HBD_AVX2<vext, 4, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset,
                                                         clpRng, c);
          }
          else
          {
            simdInterpolateHorM8_HBD<vext, 4, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset,
                                                    clpRng, c);
          }
#else
#ifdef USE_AVX2
          if( vext>= AVX2 )
          {
            simdInterpolateHorM8_AVX2<vext, 4, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset,
                                                     clpRng, c);
          }
          else
#endif
          {
            simdInterpolateHorM8<vext, 4, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng,
                                                c);
          }
#endif
        }
        else
        {
#if RExt__HIGH_BIT_DEPTH_SUPPORT
          simdInterpolateHorM4_HBD<vext, 4, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng,
                                                  c);
#else
          simdInterpolateHorM4<vext, 4, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng, c);
#endif
        }
      }
      else
      {
#if RExt__HIGH_BIT_DEPTH_SUPPORT
        simdInterpolateVerM4_HBD<vext, 4, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng,
                                                c);
#else
        simdInterpolateVerM4<vext, 4, LAST>(src, srcStride, dst, dstStride, width, height, shift, offset, clpRng, c);
#endif
      }
      return;
    }
    else if (biMCForDMVR)
    {
      if (N == 2 && widthMult4)
      {
#if RExt__HIGH_BIT_DEPTH_SUPPORT
        if (vext >= AVX2)
        {
          simdInterpolateN2_HBD_M4_AVX2<vext, LAST>(src, srcStride, dst, dstStride, cStride, width, height, shift,
                                                    offset, clpRng, c);
        }
        else
        {
          simdInterpolateN2_HBD_M4<vext, LAST>(src, srcStride, dst, dstStride, cStride, width, height, shift, offset,
                                               clpRng, c);
        }
#else
        if (clpRng.bd <= 10)
        {
          simdInterpolateN2_10BIT_M4<vext, LAST>(src, srcStride, dst, dstStride, cStride, width, height, shift, offset,
                                                 clpRng, c);
        }
        else
        {
          simdInterpolateN2_HIGHBIT_M4<vext, LAST>(src, srcStride, dst, dstStride, cStride, width, height, shift,
                                                   offset, clpRng, c);
        }
#endif
        return;
      }
    }
    else if (N == 2 && widthMult8)
    {
#if RExt__HIGH_BIT_DEPTH_SUPPORT
      simdInterpolateN2_M8_HBD<vext, LAST>(src, srcStride, dst, dstStride, cStride, width, height, shift, offset,
                                           clpRng, c);
#else
      simdInterpolateN2_M8<vext, LAST>(src, srcStride, dst, dstStride, cStride, width, height, shift, offset, clpRng,
                                       c);
#endif
      return;
    }
    else if (N == 2 && widthMult4)
    {
#if RExt__HIGH_BIT_DEPTH_SUPPORT
      simdInterpolateN2_M4_HBD<vext, LAST>(src, srcStride, dst, dstStride, cStride, width, height, shift, offset,
                                           clpRng, c);
#else
      simdInterpolateN2_M4<vext, LAST>(src, srcStride, dst, dstStride, cStride, width, height, shift, offset, clpRng,
                                       c);
#endif
      return;
    }
  }

  for( row = 0; row < height; row++ )
  {
    for( col = 0; col < width; col++ )
    {
      int sum;

      sum  = src[col + 0 * cStride] * c[0];
      sum += src[col + 1 * cStride] * c[1];
      if( N >= 4 )
      {
        sum += src[col + 2 * cStride] * c[2];
        sum += src[col + 3 * cStride] * c[3];
      }
      if( N >= 6 )
      {
        sum += src[col + 4 * cStride] * c[4];
        sum += src[col + 5 * cStride] * c[5];
      }
      if( N == 8 )
      {
        sum += src[col + 6 * cStride] * c[6];
        sum += src[col + 7 * cStride] * c[7];
      }

      Pel val = ( sum + offset ) >> shift;
      if (LAST)
      {
        val = ClipPel( val, clpRng );
      }
      dst[col] = val;
    }

    src += srcStride;
    dst += dstStride;
  }
}

template< X86_VEXT vext >
void xWeightedGeoBlk_SSE(const PredictionUnit &pu, const uint32_t width, const uint32_t height, const ComponentID compIdx, const uint8_t splitDir, PelUnitBuf& predDst, PelUnitBuf& predSrc0, PelUnitBuf& predSrc1)
{
  Pel* dst = predDst.get(compIdx).buf;
  Pel* src0 = predSrc0.get(compIdx).buf;
  Pel* src1 = predSrc1.get(compIdx).buf;
  ptrdiff_t strideDst  = predDst.get(compIdx).stride;
  ptrdiff_t strideSrc0 = predSrc0.get(compIdx).stride;
  ptrdiff_t strideSrc1 = predSrc1.get(compIdx).stride;

  const char    log2WeightBase = 3;
  const ClpRng  clpRng = pu.cu->slice->clpRngs().comp[compIdx];
  const int32_t shiftWeighted = IF_INTERNAL_FRAC_BITS(clpRng.bd) + log2WeightBase;
  const int32_t offsetWeighted = (1 << (shiftWeighted - 1)) + (IF_INTERNAL_OFFS << log2WeightBase);

  int16_t wIdx = floorLog2(pu.lwidth()) - GEO_MIN_CU_LOG2;
  int16_t hIdx = floorLog2(pu.lheight()) - GEO_MIN_CU_LOG2;

  const int angle = g_geoParams[splitDir].angleIdx;

  int16_t stepY = 0;
  int16_t* weight = nullptr;
  if (g_angle2mirror[angle] == 2)
  {
    stepY = -GEO_WEIGHT_MASK_SIZE;
    weight = &g_globalGeoWeights[g_angle2mask[angle]][(GEO_WEIGHT_MASK_SIZE - 1 - g_weightOffset[splitDir][hIdx][wIdx][1]) * GEO_WEIGHT_MASK_SIZE + g_weightOffset[splitDir][hIdx][wIdx][0]];
  }
  else if (g_angle2mirror[angle] == 1)
  {
    stepY = GEO_WEIGHT_MASK_SIZE;
    weight = &g_globalGeoWeights[g_angle2mask[angle]][g_weightOffset[splitDir][hIdx][wIdx][1] * GEO_WEIGHT_MASK_SIZE + (GEO_WEIGHT_MASK_SIZE - 1 - g_weightOffset[splitDir][hIdx][wIdx][0])];
  }
  else
  {
    stepY = GEO_WEIGHT_MASK_SIZE;
    weight = &g_globalGeoWeights[g_angle2mask[angle]][g_weightOffset[splitDir][hIdx][wIdx][1] * GEO_WEIGHT_MASK_SIZE + g_weightOffset[splitDir][hIdx][wIdx][0]];
  }

  const __m128i mmEight = _mm_set1_epi16(8);
  const __m128i mmOffset = _mm_set1_epi32(offsetWeighted);
  const __m128i mmShift = _mm_cvtsi32_si128(shiftWeighted);
  const __m128i mmMin = _mm_set1_epi16(clpRng.min);
  const __m128i mmMax = _mm_set1_epi16(clpRng.max);

  if (compIdx != COMPONENT_Y && pu.chromaFormat == ChromaFormat::_420)
  {
    stepY *= 2;
  }
  if (width == 4)
  {
    // it will occur to chroma only
    for (int y = 0; y < height; y++)
    {
      __m128i s0 = _mm_loadl_epi64((__m128i *) (src0));
      __m128i s1 = _mm_loadl_epi64((__m128i *) (src1));
      __m128i w0;
      if (g_angle2mirror[angle] == 1)
      {
        w0 = _mm_loadu_si128((__m128i *) (weight - (8 - 1)));
        const __m128i shuffle_mask = _mm_set_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
        w0 = _mm_shuffle_epi8(w0, shuffle_mask);
      }
      else
      {
        w0 = _mm_loadu_si128((__m128i *) (weight));
      }
      w0 = _mm_shuffle_epi8(w0, _mm_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13, 0, 0, 0, 0, 0, 0, 0, 0));
      __m128i w1 = _mm_sub_epi16(mmEight, w0);
      s0 = _mm_unpacklo_epi16(s0, s1);
      w0 = _mm_unpacklo_epi16(w0, w1);
      s0 = _mm_add_epi32(_mm_madd_epi16(s0, w0), mmOffset);
      s0 = _mm_sra_epi32(s0, mmShift);
      s0 = _mm_packs_epi32(s0, s0);
      s0 = _mm_min_epi16(mmMax, _mm_max_epi16(s0, mmMin));
      _mm_storel_epi64((__m128i *) (dst), s0);
      dst += strideDst;
      src0 += strideSrc0;
      src1 += strideSrc1;
      weight += stepY;
    }
  }
#if USE_AVX2
  else if (width >= 16)
  {
    const __m256i mmEightAVX2 = _mm256_set1_epi16(8);
    const __m256i mmOffsetAVX2 = _mm256_set1_epi32(offsetWeighted);
    const __m256i mmMinAVX2 = _mm256_set1_epi16(clpRng.min);
    const __m256i mmMaxAVX2 = _mm256_set1_epi16(clpRng.max);
    for (int y = 0; y < height; y++)
    {
      for (int x = 0; x < width; x += 16)
      {
        __m256i s0 = _mm256_lddqu_si256((__m256i *) (src0 + x)); // why not aligned with 128/256 bit boundaries
        __m256i s1 = _mm256_lddqu_si256((__m256i *) (src1 + x));

        __m256i w0 = _mm256_lddqu_si256((__m256i *) (weight + x));
        if (compIdx != COMPONENT_Y && pu.chromaFormat != ChromaFormat::_444)
        {
          const __m256i mask = _mm256_set_epi16(0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1);
          __m256i w0p0, w0p1;
          if (g_angle2mirror[angle] == 1)
          {
            w0p0 = _mm256_lddqu_si256((__m256i *) (weight - (x << 1) - (16 - 1))); // first sub-sample the required weights.
            w0p1 = _mm256_lddqu_si256((__m256i *) (weight - (x << 1) - 16 - (16 - 1)));
            const __m256i shuffle_mask = _mm256_set_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
            w0p0 = _mm256_shuffle_epi8(w0p0, shuffle_mask);
            w0p0 = _mm256_permute4x64_epi64(w0p0, _MM_SHUFFLE(1, 0, 3, 2));
            w0p1 = _mm256_shuffle_epi8(w0p1, shuffle_mask);
            w0p1 = _mm256_permute4x64_epi64(w0p1, _MM_SHUFFLE(1, 0, 3, 2));
          }
          else
          {
            w0p0 = _mm256_lddqu_si256((__m256i *) (weight + (x << 1))); // first sub-sample the required weights.
            w0p1 = _mm256_lddqu_si256((__m256i *) (weight + (x << 1) + 16));
          }
          w0p0 = _mm256_mullo_epi16(w0p0, mask);
          w0p1 = _mm256_mullo_epi16(w0p1, mask);
          w0 = _mm256_packs_epi16(w0p0, w0p1);
          w0 = _mm256_permute4x64_epi64(w0, _MM_SHUFFLE(3, 1, 2, 0));
        }
        else
        {
          if (g_angle2mirror[angle] == 1)
          {
            w0 = _mm256_lddqu_si256((__m256i *) (weight - x - (16 - 1)));
            const __m256i shuffle_mask = _mm256_set_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
            w0 = _mm256_shuffle_epi8(w0, shuffle_mask);
            w0 = _mm256_permute4x64_epi64(w0, _MM_SHUFFLE(1, 0, 3, 2));
          }
          else
          {
            w0 = _mm256_lddqu_si256((__m256i *) (weight + x));
          }
        }
        __m256i w1 = _mm256_sub_epi16(mmEightAVX2, w0);

        __m256i s0tmp = _mm256_unpacklo_epi16(s0, s1);
        __m256i w0tmp = _mm256_unpacklo_epi16(w0, w1);
        s0tmp = _mm256_add_epi32(_mm256_madd_epi16(s0tmp, w0tmp), mmOffsetAVX2);
        s0tmp = _mm256_sra_epi32(s0tmp, mmShift);

        s0 = _mm256_unpackhi_epi16(s0, s1);
        w0 = _mm256_unpackhi_epi16(w0, w1);
        s0 = _mm256_add_epi32(_mm256_madd_epi16(s0, w0), mmOffsetAVX2);
        s0 = _mm256_sra_epi32(s0, mmShift);

        s0 = _mm256_packs_epi32(s0tmp, s0);
        s0 = _mm256_min_epi16(mmMaxAVX2, _mm256_max_epi16(s0, mmMinAVX2));
        _mm256_storeu_si256((__m256i *) (dst + x), s0);
      }
      dst += strideDst;
      src0 += strideSrc0;
      src1 += strideSrc1;
      weight += stepY;
    }
  }
#endif
  else
  {
    for (int y = 0; y < height; y++)
    {
      for (int x = 0; x < width; x += 8)
      {
        __m128i s0 = _mm_lddqu_si128((__m128i *) (src0 + x));
        __m128i s1 = _mm_lddqu_si128((__m128i *) (src1 + x));
        __m128i w0;
        if (compIdx != COMPONENT_Y && pu.chromaFormat != ChromaFormat::_444)
        {
          const __m128i mask = _mm_set_epi16(0, 1, 0, 1, 0, 1, 0, 1);
          __m128i w0p0, w0p1;
          if (g_angle2mirror[angle] == 1)
          {
            w0p0 = _mm_lddqu_si128((__m128i *) (weight - (x << 1) - (8 - 1))); // first sub-sample the required weights.
            w0p1 = _mm_lddqu_si128((__m128i *) (weight - (x << 1) - 8 - (8 - 1)));
            const __m128i shuffle_mask = _mm_set_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
            w0p0 = _mm_shuffle_epi8(w0p0, shuffle_mask);
            w0p1 = _mm_shuffle_epi8(w0p1, shuffle_mask);
          }
          else
          {
            w0p0 = _mm_lddqu_si128((__m128i *) (weight + (x << 1))); // first sub-sample the required weights.
            w0p1 = _mm_lddqu_si128((__m128i *) (weight + (x << 1) + 8));
          }
          w0p0 = _mm_mullo_epi16(w0p0, mask);
          w0p1 = _mm_mullo_epi16(w0p1, mask);
          w0 = _mm_packs_epi32(w0p0, w0p1);
        }
        else
        {
          if (g_angle2mirror[angle] == 1)
          {
            w0 = _mm_lddqu_si128((__m128i *) (weight - x - (8 - 1)));
            const __m128i shuffle_mask = _mm_set_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
            w0 = _mm_shuffle_epi8(w0, shuffle_mask);
          }
          else
          {
            w0 = _mm_lddqu_si128((__m128i *) (weight + x));
          }
        }
        __m128i w1 = _mm_sub_epi16(mmEight, w0);

        __m128i s0tmp = _mm_unpacklo_epi16(s0, s1);
        __m128i w0tmp = _mm_unpacklo_epi16(w0, w1);
        s0tmp = _mm_add_epi32(_mm_madd_epi16(s0tmp, w0tmp), mmOffset);
        s0tmp = _mm_sra_epi32(s0tmp, mmShift);

        s0 = _mm_unpackhi_epi16(s0, s1);
        w0 = _mm_unpackhi_epi16(w0, w1);
        s0 = _mm_add_epi32(_mm_madd_epi16(s0, w0), mmOffset);
        s0 = _mm_sra_epi32(s0, mmShift);

        s0 = _mm_packs_epi32(s0tmp, s0);
        s0 = _mm_min_epi16(mmMax, _mm_max_epi16(s0, mmMin));
        _mm_storeu_si128((__m128i *) (dst + x), s0);
      }
      dst += strideDst;
      src0 += strideSrc0;
      src1 += strideSrc1;
      weight += stepY;
    }
  }
}

template <X86_VEXT vext>
void InterpolationFilter::_initInterpolationFilterX86()
{
  m_filterHor[_8_TAPS][0][0] = simdFilter<vext, 8, false, false, false, false>;
  m_filterHor[_8_TAPS][0][1] = simdFilter<vext, 8, false, false, true, false>;
  m_filterHor[_8_TAPS][1][0] = simdFilter<vext, 8, false, true, false, false>;
  m_filterHor[_8_TAPS][1][1] = simdFilter<vext, 8, false, true, true, false>;

  m_filterHor[_4_TAPS][0][0] = simdFilter<vext, 4, false, false, false, false>;
  m_filterHor[_4_TAPS][0][1] = simdFilter<vext, 4, false, false, true, false>;
  m_filterHor[_4_TAPS][1][0] = simdFilter<vext, 4, false, true, false, false>;
  m_filterHor[_4_TAPS][1][1] = simdFilter<vext, 4, false, true, true, false>;

  m_filterHor[_2_TAPS_DMVR][0][0] = simdFilter<vext, 2, false, false, false, true>;
  m_filterHor[_2_TAPS_DMVR][0][1] = simdFilter<vext, 2, false, false, true, true>;
  m_filterHor[_2_TAPS_DMVR][1][0] = simdFilter<vext, 2, false, true, false, true>;
  m_filterHor[_2_TAPS_DMVR][1][1] = simdFilter<vext, 2, false, true, true, true>;

  m_filterHor[_6_TAPS][0][0] = simdFilter<vext, 6, false, false, false, false>;
  m_filterHor[_6_TAPS][0][1] = simdFilter<vext, 6, false, false, true, false>;
  m_filterHor[_6_TAPS][1][0] = simdFilter<vext, 6, false, true, false, false>;
  m_filterHor[_6_TAPS][1][1] = simdFilter<vext, 6, false, true, true, false>;

  m_filterVer[_8_TAPS][0][0] = simdFilter<vext, 8, true, false, false, false>;
  m_filterVer[_8_TAPS][0][1] = simdFilter<vext, 8, true, false, true, false>;
  m_filterVer[_8_TAPS][1][0] = simdFilter<vext, 8, true, true, false, false>;
  m_filterVer[_8_TAPS][1][1] = simdFilter<vext, 8, true, true, true, false>;

  m_filterVer[_4_TAPS][0][0] = simdFilter<vext, 4, true, false, false, false>;
  m_filterVer[_4_TAPS][0][1] = simdFilter<vext, 4, true, false, true, false>;
  m_filterVer[_4_TAPS][1][0] = simdFilter<vext, 4, true, true, false, false>;
  m_filterVer[_4_TAPS][1][1] = simdFilter<vext, 4, true, true, true, false>;

  m_filterVer[_2_TAPS_DMVR][0][0] = simdFilter<vext, 2, true, false, false, true>;
  m_filterVer[_2_TAPS_DMVR][0][1] = simdFilter<vext, 2, true, false, true, true>;
  m_filterVer[_2_TAPS_DMVR][1][0] = simdFilter<vext, 2, true, true, false, true>;
  m_filterVer[_2_TAPS_DMVR][1][1] = simdFilter<vext, 2, true, true, true, true>;

  m_filterVer[_6_TAPS][0][0] = simdFilter<vext, 6, true, false, false, false>;
  m_filterVer[_6_TAPS][0][1] = simdFilter<vext, 6, true, false, true, false>;
  m_filterVer[_6_TAPS][1][0] = simdFilter<vext, 6, true, true, false, false>;
  m_filterVer[_6_TAPS][1][1] = simdFilter<vext, 6, true, true, true, false>;

#if RExt__HIGH_BIT_DEPTH_SUPPORT
  m_filterCopy[0][0] = simdFilterCopy_HBD<vext, false, false>;
  m_filterCopy[0][1] = simdFilterCopy_HBD<vext, false, true>;
  m_filterCopy[1][0] = simdFilterCopy_HBD<vext, true, false>;
  m_filterCopy[1][1] = simdFilterCopy_HBD<vext, true, true>;

  m_weightedGeoBlk = xWeightedGeoBlk_HBD_SIMD<vext>;
#else
  m_filterCopy[0][0]   = simdFilterCopy<vext, false, false>;
  m_filterCopy[0][1]   = simdFilterCopy<vext, false, true>;
  m_filterCopy[1][0]   = simdFilterCopy<vext, true, false>;
  m_filterCopy[1][1]   = simdFilterCopy<vext, true, true>;

  m_weightedGeoBlk = xWeightedGeoBlk_SSE<vext>;
#endif
}

template void InterpolationFilter::_initInterpolationFilterX86<SIMDX86>();

#endif //#ifdef TARGET_SIMD_X86
//! \}
