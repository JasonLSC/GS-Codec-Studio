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

/** \file     Buffer.cpp
 *  \brief    Low-overhead class describing 2D memory layout
 */

#define DONT_UNDEF_SIZE_AWARE_PER_EL_OP

// unit needs to come first due to a forward declaration
#include "Unit.h"
#include "Buffer.h"
#include "InterpolationFilter.h"

void applyPROFCore(Pel *dst, ptrdiff_t dstStride, const Pel *src, ptrdiff_t srcStride, int width, int height,
                   const Pel *gradX, const Pel *gradY, ptrdiff_t gradStride, const int *dMvX, const int *dMvY,
                   ptrdiff_t dMvStride, const bool bi, int shiftNum, Pel offset, const ClpRng &clpRng)
{
  int idx = 0;

  const int dILimit = 1 << std::max<int>(clpRng.bd + 1, 13);
  for (int h = 0; h < height; h++)
  {
    for (int w = 0; w < width; w++)
    {
      int32_t dI = dMvX[idx] * gradX[w] + dMvY[idx] * gradY[w];
      dI = Clip3(-dILimit, dILimit - 1, dI);
      dst[w] = src[w] + dI;
      if (!bi)
      {
        dst[w] = (dst[w] + offset) >> shiftNum;
        dst[w] = ClipPel(dst[w], clpRng);
      }

      idx++;
    }
    gradX += gradStride;
    gradY += gradStride;
    dst += dstStride;
    src += srcStride;
  }
}

template<typename T>
void addAvgCore(const T *src1, ptrdiff_t src1Stride, const T *src2, ptrdiff_t src2Stride, T *dest, ptrdiff_t dstStride,
                int width, int height, int rshift, int offset, const ClpRng &clpRng)
{
#define ADD_AVG_CORE_OP( ADDR ) dest[ADDR] = ClipPel( rightShift( ( src1[ADDR] + src2[ADDR] + offset ), rshift ), clpRng )
#define ADD_AVG_CORE_INC    \
  src1 += src1Stride;       \
  src2 += src2Stride;       \
  dest +=  dstStride;       \

  SIZE_AWARE_PER_EL_OP( ADD_AVG_CORE_OP, ADD_AVG_CORE_INC );

#undef ADD_AVG_CORE_OP
#undef ADD_AVG_CORE_INC
}

void addBIOAvgCore(const Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride, Pel *dst,
                   ptrdiff_t dstStride, const Pel *gradX0, const Pel *gradX1, const Pel *gradY0, const Pel *gradY1,
                   ptrdiff_t gradStride, int width, int height, int tmpx, int tmpy, int shift, int offset,
                   const ClpRng &clpRng)
{
  int b = 0;

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x += 4)
    {
      b = tmpx * (gradX0[x] - gradX1[x]) + tmpy * (gradY0[x] - gradY1[x]);
      dst[x] = ClipPel(rightShift((src0[x] + src1[x] + b + offset), shift), clpRng);

      b = tmpx * (gradX0[x + 1] - gradX1[x + 1]) + tmpy * (gradY0[x + 1] - gradY1[x + 1]);
      dst[x + 1] = ClipPel(rightShift((src0[x + 1] + src1[x + 1] + b + offset), shift), clpRng);

      b = tmpx * (gradX0[x + 2] - gradX1[x + 2]) + tmpy * (gradY0[x + 2] - gradY1[x + 2]);
      dst[x + 2] = ClipPel(rightShift((src0[x + 2] + src1[x + 2] + b + offset), shift), clpRng);

      b = tmpx * (gradX0[x + 3] - gradX1[x + 3]) + tmpy * (gradY0[x + 3] - gradY1[x + 3]);
      dst[x + 3] = ClipPel(rightShift((src0[x + 3] + src1[x + 3] + b + offset), shift), clpRng);
    }
    dst += dstStride;       src0 += src0Stride;     src1 += src1Stride;
    gradX0 += gradStride; gradX1 += gradStride; gradY0 += gradStride; gradY1 += gradStride;
  }
}

template<bool PAD = true>
void gradFilterCore(Pel *pSrc, ptrdiff_t srcStride, int width, int height, ptrdiff_t gradStride, Pel *gradX, Pel *gradY,
                    const int bitDepth)
{
  Pel* srcTmp = pSrc + srcStride + 1;
  Pel* gradXTmp = gradX + gradStride + 1;
  Pel* gradYTmp = gradY + gradStride + 1;
  int  shift1 = 6;

  for (int y = 0; y < (height - 2 * BIO_EXTEND_SIZE); y++)
  {
    for (int x = 0; x < (width - 2 * BIO_EXTEND_SIZE); x++)
    {
      gradYTmp[x] = ( srcTmp[x + srcStride] >> shift1 ) - ( srcTmp[x - srcStride] >> shift1 );
      gradXTmp[x] = ( srcTmp[x + 1] >> shift1 ) - ( srcTmp[x - 1] >> shift1 );
    }
    gradXTmp += gradStride;
    gradYTmp += gradStride;
    srcTmp += srcStride;
  }

  if (PAD)
  {
    gradXTmp = gradX + gradStride + 1;
    gradYTmp = gradY + gradStride + 1;
    for (int y = 0; y < (height - 2 * BIO_EXTEND_SIZE); y++)
    {
      gradXTmp[-1]                          = gradXTmp[0];
      gradXTmp[width - 2 * BIO_EXTEND_SIZE] = gradXTmp[width - 2 * BIO_EXTEND_SIZE - 1];
      gradXTmp += gradStride;

      gradYTmp[-1]                          = gradYTmp[0];
      gradYTmp[width - 2 * BIO_EXTEND_SIZE] = gradYTmp[width - 2 * BIO_EXTEND_SIZE - 1];
      gradYTmp += gradStride;
    }

    gradXTmp = gradX + gradStride;
    gradYTmp = gradY + gradStride;
    ::memcpy(gradXTmp - gradStride, gradXTmp, sizeof(Pel) * (width));
    ::memcpy(gradXTmp + (height - 2 * BIO_EXTEND_SIZE) * gradStride,
             gradXTmp + (height - 2 * BIO_EXTEND_SIZE - 1) * gradStride, sizeof(Pel) * (width));
    ::memcpy(gradYTmp - gradStride, gradYTmp, sizeof(Pel) * (width));
    ::memcpy(gradYTmp + (height - 2 * BIO_EXTEND_SIZE) * gradStride,
             gradYTmp + (height - 2 * BIO_EXTEND_SIZE - 1) * gradStride, sizeof(Pel) * (width));
  }
}

void calcBIOSumsCore(const Pel *srcY0Tmp, const Pel *srcY1Tmp, Pel *gradX0, Pel *gradX1, Pel *gradY0, Pel *gradY1,
                     int xu, int yu, const ptrdiff_t src0Stride, const ptrdiff_t src1Stride, const int widthG,
                     const int bitDepth, int *sumAbsGX, int *sumAbsGY, int *sumDIX, int *sumDIY, int *sumSignGY_GX)
{
  int shift4 = 4;
  int shift5 = 1;

  for (int y = 0; y < 6; y++)
  {
    for (int x = 0; x < 6; x++)
    {
      int tmpGX = (gradX0[x] + gradX1[x]) >> shift5;
      int tmpGY = (gradY0[x] + gradY1[x]) >> shift5;
      int tmpDI = (int)((srcY1Tmp[x] >> shift4) - (srcY0Tmp[x] >> shift4));
      *sumAbsGX += (tmpGX < 0 ? -tmpGX : tmpGX);
      *sumAbsGY += (tmpGY < 0 ? -tmpGY : tmpGY);
      *sumDIX += (tmpGX < 0 ? -tmpDI : (tmpGX == 0 ? 0 : tmpDI));
      *sumDIY += (tmpGY < 0 ? -tmpDI : (tmpGY == 0 ? 0 : tmpDI));
      *sumSignGY_GX += (tmpGY < 0 ? -tmpGX : (tmpGY == 0 ? 0 : tmpGX));

    }
    srcY1Tmp += src1Stride;
    srcY0Tmp += src0Stride;
    gradX0 += widthG;
    gradX1 += widthG;
    gradY0 += widthG;
    gradY1 += widthG;
  }
}


void calcBlkGradientCore(int sx, int sy, int     *arraysGx2, int     *arraysGxGy, int     *arraysGxdI, int     *arraysGy2, int     *arraysGydI, int     &sGx2, int     &sGy2, int     &sGxGy, int     &sGxdI, int     &sGydI, int width, int height, int unitSize)
{
  int     *Gx2 = arraysGx2;
  int     *Gy2 = arraysGy2;
  int     *GxGy = arraysGxGy;
  int     *GxdI = arraysGxdI;
  int     *GydI = arraysGydI;

  // set to the above row due to JVET_K0485_BIO_EXTEND_SIZE
  Gx2 -= (BIO_EXTEND_SIZE*width);
  Gy2 -= (BIO_EXTEND_SIZE*width);
  GxGy -= (BIO_EXTEND_SIZE*width);
  GxdI -= (BIO_EXTEND_SIZE*width);
  GydI -= (BIO_EXTEND_SIZE*width);

  for (int y = -BIO_EXTEND_SIZE; y < unitSize + BIO_EXTEND_SIZE; y++)
  {
    for (int x = -BIO_EXTEND_SIZE; x < unitSize + BIO_EXTEND_SIZE; x++)
    {
      sGx2 += Gx2[x];
      sGy2 += Gy2[x];
      sGxGy += GxGy[x];
      sGxdI += GxdI[x];
      sGydI += GydI[x];
    }
    Gx2 += width;
    Gy2 += width;
    GxGy += width;
    GxdI += width;
    GydI += width;
  }
}

template<typename T>
void reconstructCore(const T *src1, ptrdiff_t src1Stride, const T *src2, ptrdiff_t src2Stride, T *dest,
                     ptrdiff_t dstStride, int width, int height, const ClpRng &clpRng)
{
#define RECO_CORE_OP( ADDR ) dest[ADDR] = ClipPel( src1[ADDR] + src2[ADDR], clpRng )
#define RECO_CORE_INC     \
  src1 += src1Stride;     \
  src2 += src2Stride;     \
  dest +=  dstStride;     \

  SIZE_AWARE_PER_EL_OP( RECO_CORE_OP, RECO_CORE_INC );

#undef RECO_CORE_OP
#undef RECO_CORE_INC
}

template<typename T>
void linTfCore(const T *src, ptrdiff_t srcStride, Pel *dst, ptrdiff_t dstStride, int width, int height, int scale,
               int shift, int offset, const ClpRng &clpRng, bool bClip)
{
#define LINTF_CORE_OP( ADDR ) dst[ADDR] = ( Pel ) bClip ? ClipPel( rightShift( scale * src[ADDR], shift ) + offset, clpRng ) : ( rightShift( scale * src[ADDR], shift ) + offset )
#define LINTF_CORE_INC  \
  src += srcStride;     \
  dst += dstStride;     \

  SIZE_AWARE_PER_EL_OP( LINTF_CORE_OP, LINTF_CORE_INC );

#undef LINTF_CORE_OP
#undef LINTF_CORE_INC
}

PelBufferOps::PelBufferOps()
{
  addAvg4 = addAvgCore<Pel>;
  addAvg8 = addAvgCore<Pel>;

  reco4 = reconstructCore<Pel>;
  reco8 = reconstructCore<Pel>;

  linTf4 = linTfCore<Pel>;
  linTf8 = linTfCore<Pel>;

  addBIOAvg4      = addBIOAvgCore;
  bioGradFilter   = gradFilterCore;
  calcBIOSums = calcBIOSumsCore;

  copyBuffer = copyBufferCore;
  padding = paddingCore;
#if ENABLE_SIMD_OPT_BCW
  removeWeightHighFreq8 = nullptr;
  removeWeightHighFreq4 = nullptr;
  removeHighFreq8       = nullptr;
  removeHighFreq4       = nullptr;
#endif

  profGradFilter = gradFilterCore <false>;
  applyPROF      = applyPROFCore;
  roundIntVector = nullptr;
}

PelBufferOps g_pelBufOP = PelBufferOps();

void copyBufferCore(const Pel *src, ptrdiff_t srcStride, Pel *dst, ptrdiff_t dstStride, int width, int height)
{
  int numBytes = width * sizeof(Pel);
  for (int i = 0; i < height; i++)
  {
    memcpy(dst + i * dstStride, src + i * srcStride, numBytes);
  }
}

void paddingCore(Pel *ptr, ptrdiff_t stride, int width, int height, int padSize)
{
  /*left and right padding*/
  Pel *ptrTemp1 = ptr;
  Pel *ptrTemp2 = ptr + (width - 1);

  for (int i = 0; i < height; i++)
  {
    for (int j = 1; j <= padSize; j++)
    {
      ptrTemp1[stride * i - j] = ptrTemp1[stride * i];
      ptrTemp2[stride * i + j] = ptrTemp2[stride * i];
    }
  }
  /*Top and Bottom padding*/
  int numBytes = (width + padSize + padSize) * sizeof(Pel);
  ptrTemp1     = ptr - padSize;
  ptrTemp2     = ptr + stride * (height - 1) - padSize;
  for (int i = 1; i <= padSize; i++)
  {
    memcpy(ptrTemp1 - i * stride, (ptrTemp1), numBytes);
    memcpy(ptrTemp2 + i * stride, (ptrTemp2), numBytes);
  }
}
template<>
void AreaBuf<Pel>::addWeightedAvg(const AreaBuf<const Pel> &other1, const AreaBuf<const Pel> &other2, const ClpRng& clpRng, const int8_t bcwIdx)
{
  const int8_t w0 = getBcwWeight(bcwIdx, REF_PIC_LIST_0);
  const int8_t w1 = getBcwWeight(bcwIdx, REF_PIC_LIST_1);
  const int8_t log2WeightBase = BCW_LOG2_WEIGHT_BASE;

  const Pel* src0 = other1.buf;
  const Pel* src2 = other2.buf;
  Pel* dest = buf;

  const ptrdiff_t src1Stride = other1.stride;
  const ptrdiff_t src2Stride = other2.stride;
  const ptrdiff_t destStride = stride;
  const int clipbd = clpRng.bd;
  const int shiftNum = IF_INTERNAL_FRAC_BITS(clipbd) + log2WeightBase;
  const int offset = (1 << (shiftNum - 1)) + (IF_INTERNAL_OFFS << log2WeightBase);

#define ADD_AVG_OP( ADDR ) dest[ADDR] = ClipPel( rightShift( ( src0[ADDR]*w0 + src2[ADDR]*w1 + offset ), shiftNum ), clpRng )
#define ADD_AVG_INC     \
    src0 += src1Stride; \
    src2 += src2Stride; \
    dest += destStride; \

  SIZE_AWARE_PER_EL_OP(ADD_AVG_OP, ADD_AVG_INC);

#undef ADD_AVG_OP
#undef ADD_AVG_INC
}

template<>
void AreaBuf<Pel>::rspSignal(std::vector<Pel>& pLUT)
{
  Pel* dst = buf;
  Pel* src = buf;
  for (unsigned y = 0; y < height; y++)
  {
    for (unsigned x = 0; x < width; x++)
    {
      dst[x] = pLUT[src[x]];
    }
    dst += stride;
    src += stride;
  }
}

template<>
void AreaBuf<Pel>::scaleSignal(const int scale, const bool dir, const ClpRng& clpRng)
{
  Pel* dst = buf;
  Pel* src = buf;
  int  absval;
  int maxAbsclipBD = (1<<clpRng.bd) - 1;

  if (dir) // forward
  {
    if (width == 1)
    {
      THROW("Blocks of width = 1 not supported");
    }
    else
    {
      for (unsigned y = 0; y < height; y++)
      {
        for (unsigned x = 0; x < width; x++)
        {
          const int sign = sgn2(src[x]);
          absval = sign * src[x];
          dst[x] = (Pel)Clip3(-maxAbsclipBD, maxAbsclipBD, sign * (((absval << CSCALE_FP_PREC) + (scale >> 1)) / scale));
        }
        dst += stride;
        src += stride;
      }
    }
  }
  else // inverse
  {
    for (unsigned y = 0; y < height; y++)
    {
      for (unsigned x = 0; x < width; x++)
      {
        src[x] = (Pel)Clip3((Pel)(-maxAbsclipBD - 1), (Pel)maxAbsclipBD, src[x]);
        const int sign = sgn2(src[x]);
        absval = sign * src[x];
        int val = sign * ((absval * scale + (1 << (CSCALE_FP_PREC - 1))) >> CSCALE_FP_PREC);
        if (sizeof(Pel) == 2) // avoid overflow when storing data
        {
           val = Clip3<int>(-32768, 32767, val);
        }
        dst[x] = (Pel)val;
      }
      dst += stride;
      src += stride;
    }
  }
}

template<>
void AreaBuf<Pel>::applyLumaCTI(std::vector<Pel>& pLUTY)
{
  Pel* dst = buf;
  Pel* src = buf;
  for (unsigned y = 0; y < height; y++)
  {
    for (unsigned x = 0; x < width; x++)
    {
      dst[x] = pLUTY[src[x]];
    }
    dst += stride;
    src += stride;
  }
}

template<>
void AreaBuf<Pel>::applyChromaCTI(Pel *bufY, ptrdiff_t strideY, std::vector<Pel> &pLUTC, int bitDepth,
                                  ChromaFormat chrFormat, bool fwdMap)
{
  int range = 1 << bitDepth;
  int offset = range / 2;
  int sx = 1 << getComponentScaleX(COMPONENT_Cb, chrFormat);
  int sy = 1 << getComponentScaleY(COMPONENT_Cb, chrFormat);

  Pel* dst = buf;
  Pel* src = buf;
  if (fwdMap)
  {
    for (unsigned y = 0; y < height; y++)
    {
      for (unsigned x = 0; x < width; x++)
      {
        int pelY = bufY[sy * y * strideY + sx * x];
        double scale = (double)pLUTC[pelY] / (double)(1 << CSCALE_FP_PREC);
        dst[x] = Clip3((Pel)0, (Pel)(range - 1), (Pel)(offset + (double)(src[x] - offset) / scale + .5));
      }
      dst += stride;
      src += stride;
    }
  }
  else
  {
    for (unsigned y = 0; y < height; y++)
    {
      for (unsigned x = 0; x < width; x++)
      {
        int pelY = bufY[sy * y * strideY + sx * x];
        int scal = pLUTC[pelY];
        dst[x] = Clip3(0, range - 1, ((offset << CSCALE_FP_PREC) + (src[x] - offset) * scal + (1 << (CSCALE_FP_PREC - 1))) >> CSCALE_FP_PREC);
      }
      dst += stride;
      src += stride;
    }
  }
}

template<>
void AreaBuf<Pel>::addAvg( const AreaBuf<const Pel> &other1, const AreaBuf<const Pel> &other2, const ClpRng& clpRng)
{
  const Pel* src0 = other1.buf;
  const Pel* src2 = other2.buf;
  Pel       *dest = buf;

  const ptrdiff_t src1Stride = other1.stride;
  const ptrdiff_t src2Stride = other2.stride;
  const ptrdiff_t destStride = stride;
  const int       clipbd     = clpRng.bd;
  const int       shiftNum   = IF_INTERNAL_FRAC_BITS(clipbd) + 1;
  const int       offset     = (1 << (shiftNum - 1)) + 2 * IF_INTERNAL_OFFS;

#if ENABLE_SIMD_OPT_BUFFER && defined(TARGET_SIMD_X86)
  if( ( width & 7 ) == 0 )
  {
    g_pelBufOP.addAvg8( src0, src1Stride, src2, src2Stride, dest, destStride, width, height, shiftNum, offset, clpRng );
  }
  else if( ( width & 3 ) == 0 )
  {
    g_pelBufOP.addAvg4( src0, src1Stride, src2, src2Stride, dest, destStride, width, height, shiftNum, offset, clpRng );
  }
  else
#endif
  {
#define ADD_AVG_OP( ADDR ) dest[ADDR] = ClipPel( rightShift( ( src0[ADDR] + src2[ADDR] + offset ), shiftNum ), clpRng )
#define ADD_AVG_INC     \
    src0 += src1Stride; \
    src2 += src2Stride; \
    dest += destStride; \

    SIZE_AWARE_PER_EL_OP( ADD_AVG_OP, ADD_AVG_INC );

#undef ADD_AVG_OP
#undef ADD_AVG_INC
  }
}

template<>
void AreaBuf<Pel>::copyClip( const AreaBuf<const Pel> &src, const ClpRng& clpRng )
{
  const Pel* srcp = src.buf;
  Pel       *dest = buf;

  const ptrdiff_t srcStride  = src.stride;
  const ptrdiff_t destStride = stride;

  if (width == 1)
  {
    THROW("Blocks of width = 1 not supported");
  }
  else
  {
#define RECO_OP( ADDR ) dest[ADDR] = ClipPel( srcp[ADDR], clpRng )
#define RECO_INC        \
    srcp += srcStride;  \
    dest += destStride; \

    SIZE_AWARE_PER_EL_OP( RECO_OP, RECO_INC );

#undef RECO_OP
#undef RECO_INC
  }
}

template<>
void AreaBuf<Pel>::roundToOutputBitdepth( const AreaBuf<const Pel> &src, const ClpRng& clpRng )
{
  const Pel* srcp = src.buf;
  Pel            *dest       = buf;
  const ptrdiff_t srcStride  = src.stride;
  const ptrdiff_t destStride = stride;

  const int32_t clipbd        = clpRng.bd;
  const int32_t shiftDefault  = IF_INTERNAL_FRAC_BITS(clipbd);
  const int32_t offsetDefault = (1 << (shiftDefault - 1)) + IF_INTERNAL_OFFS;

  if (width == 1)
  {
    THROW("Blocks of width = 1 not supported");
  }
  else
  {
#define RND_OP( ADDR ) dest[ADDR] = ClipPel( rightShift( srcp[ADDR] + offsetDefault, shiftDefault), clpRng )
#define RND_INC        \
    srcp += srcStride;  \
    dest += destStride; \

    SIZE_AWARE_PER_EL_OP( RND_OP, RND_INC );

#undef RND_OP
#undef RND_INC
  }
}


template<>
void AreaBuf<Pel>::reconstruct( const AreaBuf<const Pel> &pred, const AreaBuf<const Pel> &resi, const ClpRng& clpRng )
{
  const Pel* src1 = pred.buf;
  const Pel* src2 = resi.buf;
  Pel       *dest = buf;

  const ptrdiff_t src1Stride = pred.stride;
  const ptrdiff_t src2Stride = resi.stride;
  const ptrdiff_t destStride = stride;

#if ENABLE_SIMD_OPT_BUFFER && defined(TARGET_SIMD_X86)
  if( ( width & 7 ) == 0 )
  {
    g_pelBufOP.reco8( src1, src1Stride, src2, src2Stride, dest, destStride, width, height, clpRng );
  }
  else if( ( width & 3 ) == 0 )
  {
    g_pelBufOP.reco4( src1, src1Stride, src2, src2Stride, dest, destStride, width, height, clpRng );
  }
  else
#endif
  {
#define RECO_OP( ADDR ) dest[ADDR] = ClipPel( src1[ADDR] + src2[ADDR], clpRng )
#define RECO_INC        \
    src1 += src1Stride; \
    src2 += src2Stride; \
    dest += destStride; \

    SIZE_AWARE_PER_EL_OP( RECO_OP, RECO_INC );

#undef RECO_OP
#undef RECO_INC
  }
}

template<>
void AreaBuf<Pel>::linearTransform( const int scale, const int shift, const int offset, bool bClip, const ClpRng& clpRng )
{
  const Pel* src = buf;
        Pel* dst = buf;

  if( width == 1 )
  {
    THROW( "Blocks of width = 1 not supported" );
  }
#if ENABLE_SIMD_OPT_BUFFER && defined(TARGET_SIMD_X86)
  else if( ( width & 7 ) == 0 )
  {
    g_pelBufOP.linTf8( src, stride, dst, stride, width, height, scale, shift, offset, clpRng, bClip );
  }
  else if( ( width & 3 ) == 0 )
  {
    g_pelBufOP.linTf4( src, stride, dst, stride, width, height, scale, shift, offset, clpRng, bClip );
  }
#endif
  else
  {
#define LINTF_OP( ADDR ) dst[ADDR] = ( Pel ) bClip ? ClipPel( rightShift( scale * src[ADDR], shift ) + offset, clpRng ) : ( rightShift( scale * src[ADDR], shift ) + offset )
#define LINTF_INC        \
    src += stride;       \
    dst += stride;       \

    SIZE_AWARE_PER_EL_OP( LINTF_OP, LINTF_INC );

#undef LINTF_OP
#undef LINTF_INC
  }
}

#if ENABLE_SIMD_OPT_BUFFER && defined(TARGET_SIMD_X86)
template<>
void AreaBuf<Pel>::subtract( const Pel val )
{
  ClpRng clpRngDummy;
  linearTransform( 1, 0, -val, false, clpRngDummy );
}
#endif


PelStorage::PelStorage()
{
  for( uint32_t i = 0; i < MAX_NUM_COMPONENT; i++ )
  {
    m_origin[i] = nullptr;
  }
}

PelStorage::~PelStorage()
{
  destroy();
}

void PelStorage::create( const UnitArea &_UnitArea )
{
  create( _UnitArea.chromaFormat, _UnitArea.blocks[0] );
}

void PelStorage::create( const ChromaFormat &_chromaFormat, const Area& _area, const unsigned _maxCUSize, const unsigned _margin, const unsigned _alignment, const bool _scaleChromaMargin )
{
  CHECK( !bufs.empty(), "Trying to re-create an already initialized buffer" );

  chromaFormat = _chromaFormat;

  const uint32_t numCh = getNumberValidComponents( _chromaFormat );

  unsigned extHeight = _area.height;
  unsigned extWidth  = _area.width;

  if( _maxCUSize )
  {
    extHeight = ( ( _area.height + _maxCUSize - 1 ) / _maxCUSize ) * _maxCUSize;
    extWidth  = ( ( _area.width  + _maxCUSize - 1 ) / _maxCUSize ) * _maxCUSize;
  }

  for( uint32_t i = 0; i < numCh; i++ )
  {
    const ComponentID compID = ComponentID( i );
    const unsigned scaleX = ::getComponentScaleX( compID, _chromaFormat );
    const unsigned scaleY = ::getComponentScaleY( compID, _chromaFormat );

    unsigned scaledHeight = extHeight >> scaleY;
    unsigned scaledWidth  = extWidth  >> scaleX;
    unsigned ymargin      = _margin >> (_scaleChromaMargin?scaleY:0);
    unsigned xmargin      = _margin >> (_scaleChromaMargin?scaleX:0);
    unsigned totalWidth   = scaledWidth + 2*xmargin;
    unsigned totalHeight  = scaledHeight +2*ymargin;

    if( _alignment )
    {
      // make sure buffer lines are align
      CHECK( _alignment != MEMORY_ALIGN_DEF_SIZE, "Unsupported alignment" );
      totalWidth = ( ( totalWidth + _alignment - 1 ) / _alignment ) * _alignment;
    }
    uint32_t area = totalWidth * totalHeight;
    CHECK( !area, "Trying to create a buffer with zero area" );

    m_origin[i] = ( Pel* ) xMalloc( Pel, area );
    Pel* topLeft = m_origin[i] + totalWidth * ymargin + xmargin;
    bufs.push_back( PelBuf( topLeft, totalWidth, _area.width >> scaleX, _area.height >> scaleY ) );
  }
}

void PelStorage::createFromBuf( PelUnitBuf buf )
{
  chromaFormat = buf.chromaFormat;

  const uint32_t numCh = ::getNumberValidComponents( chromaFormat );

  bufs.resize(numCh);

  for( uint32_t i = 0; i < numCh; i++ )
  {
    PelBuf cPelBuf = buf.get( ComponentID( i ) );
    bufs[i] = PelBuf( cPelBuf.bufAt( 0, 0 ), cPelBuf.stride, cPelBuf.width, cPelBuf.height );
  }
}

void PelStorage::swap( PelStorage& other )
{
  const uint32_t numCh = ::getNumberValidComponents( chromaFormat );

  for( uint32_t i = 0; i < numCh; i++ )
  {
    // check this otherwise it would turn out to get very weird
    CHECK( chromaFormat                   != other.chromaFormat                  , "Incompatible formats" );
    CHECK( get( ComponentID( i ) )        != other.get( ComponentID( i ) )       , "Incompatible formats" );
    CHECK( get( ComponentID( i ) ).stride != other.get( ComponentID( i ) ).stride, "Incompatible formats" );

    std::swap( bufs[i].buf,    other.bufs[i].buf );
    std::swap( bufs[i].stride, other.bufs[i].stride );
    std::swap( m_origin[i],    other.m_origin[i] );
  }
}

void PelStorage::destroy()
{
  chromaFormat = ChromaFormat::UNDEFINED;
  for( uint32_t i = 0; i < MAX_NUM_COMPONENT; i++ )
  {
    if( m_origin[i] )
    {
      xFree( m_origin[i] );
      m_origin[i] = nullptr;
    }
  }
  bufs.clear();
}

template<>
void UnitBuf<Pel>::colorSpaceConvert(const UnitBuf<Pel> &other, const bool forward, const ClpRng& clpRng)
{
  const Pel* pOrg0 = bufs[COMPONENT_Y].buf;
  const Pel* pOrg1 = bufs[COMPONENT_Cb].buf;
  const Pel* pOrg2 = bufs[COMPONENT_Cr].buf;
  const ptrdiff_t strideOrg = bufs[COMPONENT_Y].stride;

  Pel* pDst0 = other.bufs[COMPONENT_Y].buf;
  Pel* pDst1 = other.bufs[COMPONENT_Cb].buf;
  Pel* pDst2 = other.bufs[COMPONENT_Cr].buf;
  const ptrdiff_t strideDst = other.bufs[COMPONENT_Y].stride;

  int width = bufs[COMPONENT_Y].width;
  int height = bufs[COMPONENT_Y].height;
  int maxAbsclipBD = (1 << (clpRng.bd + 1)) - 1;
  int r, g, b;
  int y0, cg, co;

  CHECK(bufs[COMPONENT_Y].stride != bufs[COMPONENT_Cb].stride || bufs[COMPONENT_Y].stride != bufs[COMPONENT_Cr].stride, "unequal stride for 444 content");
  CHECK(other.bufs[COMPONENT_Y].stride != other.bufs[COMPONENT_Cb].stride || other.bufs[COMPONENT_Y].stride != other.bufs[COMPONENT_Cr].stride, "unequal stride for 444 content");
  CHECK(bufs[COMPONENT_Y].width != other.bufs[COMPONENT_Y].width || bufs[COMPONENT_Y].height != other.bufs[COMPONENT_Y].height, "unequal block size")

  if (forward)
  {
    for (int y = 0; y < height; y++)
    {
      for (int x = 0; x < width; x++)
      {
        r = pOrg2[x];
        g = pOrg0[x];
        b = pOrg1[x];

        co       = r - b;
        int t    = b + (co >> 1);
        cg       = g - t;
        pDst0[x] = t + (cg >> 1);
        pDst1[x] = cg;
        pDst2[x] = co;
      }
      pOrg0 += strideOrg;
      pOrg1 += strideOrg;
      pOrg2 += strideOrg;
      pDst0 += strideDst;
      pDst1 += strideDst;
      pDst2 += strideDst;
    }
  }
  else
  {
    for (int y = 0; y < height; y++)
    {
      for (int x = 0; x < width; x++)
      {
        y0 = pOrg0[x];
        cg = pOrg1[x];
        co = pOrg2[x];

        y0 = Clip3((-maxAbsclipBD - 1), maxAbsclipBD, y0);
        cg = Clip3((-maxAbsclipBD - 1), maxAbsclipBD, cg);
        co = Clip3((-maxAbsclipBD - 1), maxAbsclipBD, co);

        int t    = y0 - (cg >> 1);
        pDst0[x] = cg + t;
        pDst1[x] = t - (co >> 1);
        pDst2[x] = co + pDst1[x];
      }

      pOrg0 += strideOrg;
      pOrg1 += strideOrg;
      pOrg2 += strideOrg;
      pDst0 += strideDst;
      pDst1 += strideDst;
      pDst2 += strideDst;
    }
  }
}

PelUnitBufPool::PelUnitBufPool()
{

}

PelUnitBufPool::~PelUnitBufPool()
{
}

void PelUnitBufPool::initPelUnitBufPool(ChromaFormat chromaFormat, int ctuWidth, int ctuHeight)
{
  m_chromaFormat = chromaFormat;
  m_ctuArea.x = 0;
  m_ctuArea.y = 0;
  m_ctuArea.width = ctuWidth;
  m_ctuArea.height = ctuHeight;
}

PelUnitBuf* PelUnitBufPool::getPelUnitBuf(const UnitArea& unitArea)
{
  PelStorage* pelStorage = m_pelStoragePool.get();
  if (pelStorage->bufs.empty())
  {
    pelStorage->create(m_chromaFormat, m_ctuArea);
  }
  
  PelUnitBuf* pelUnitBuf = m_pelUnitBufPool.get();
  *pelUnitBuf = pelStorage->getBuf(unitArea);

  if (m_map.find(pelUnitBuf) == m_map.end())
  {
    m_map[pelUnitBuf] = pelStorage;
  }
  else
  {
    CHECK(m_map[pelUnitBuf] != pelStorage, "Wrong mapping in PelUnitBufPool");
  }
  
  return pelUnitBuf;
}

void PelUnitBufPool::giveBack(PelUnitBuf* p)
{
  CHECK(m_map.find(p) == m_map.end(), "Unknown PelUnitBuf in PelUnitBufPool");
  m_pelStoragePool.giveBack(m_map[p]);
  m_pelUnitBufPool.giveBack(p);
}
