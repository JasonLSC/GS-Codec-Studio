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

/** \file     Buffer.h
 *  \brief    Low-overhead class describing 2D memory layout
 */

#ifndef __BUFFER__
#define __BUFFER__

#include "Common.h"
#include "CommonDef.h"
#include "ChromaFormat.h"
#include "MotionInfo.h"

#include <string.h>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

// ---------------------------------------------------------------------------
// AreaBuf struct
// ---------------------------------------------------------------------------

struct PelBufferOps
{
  PelBufferOps();

#if ENABLE_SIMD_OPT_BUFFER && defined(TARGET_SIMD_X86)
  void initPelBufOpsX86();
  template<X86_VEXT vext>
  void _initPelBufOpsX86();
#endif

  void (*addAvg4)(const Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride, Pel *dst,
                  ptrdiff_t dstStride, int width, int height, int shift, int offset, const ClpRng &clpRng);
  void (*addAvg8)(const Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride, Pel *dst,
                  ptrdiff_t dstStride, int width, int height, int shift, int offset, const ClpRng &clpRng);
  void (*reco4)(const Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride, Pel *dst,
                ptrdiff_t dstStride, int width, int height, const ClpRng &clpRng);
  void (*reco8)(const Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride, Pel *dst,
                ptrdiff_t dstStride, int width, int height, const ClpRng &clpRng);
  void (*linTf4)(const Pel *src0, ptrdiff_t src0Stride, Pel *dst, ptrdiff_t dstStride, int width, int height, int scale,
                 int shift, int offset, const ClpRng &clpRng, bool bClip);
  void (*linTf8)(const Pel *src0, ptrdiff_t src0Stride, Pel *dst, ptrdiff_t dstStride, int width, int height, int scale,
                 int shift, int offset, const ClpRng &clpRng, bool bClip);
  void (*addBIOAvg4)(const Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride, Pel *dst,
                     ptrdiff_t dstStride, const Pel *gradX0, const Pel *gradX1, const Pel *gradY0, const Pel *gradY1,
                     ptrdiff_t gradStride, int width, int height, int tmpx, int tmpy, int shift, int offset,
                     const ClpRng &clpRng);
  void (*bioGradFilter)(Pel *pSrc, ptrdiff_t srcStride, int width, int height, ptrdiff_t gradStride, Pel *gradX,
                        Pel *gradY, const int bitDepth);
  void (*calcBIOPar)(const Pel *srcY0Temp, const Pel *srcY1Temp, const Pel *gradX0, const Pel *gradX1,
                     const Pel *gradY0, const Pel *gradY1, int *dotProductTemp1, int *dotProductTemp2,
                     int *dotProductTemp3, int *dotProductTemp5, int *dotProductTemp6, const ptrdiff_t src0Stride,
                     const ptrdiff_t src1Stride, const int gradStride, const int widthG, const int heightG,
                     const int bitDepth);
  void (*calcBIOSums)(const Pel *srcY0Tmp, const Pel *srcY1Tmp, Pel *gradX0, Pel *gradX1, Pel *gradY0, Pel *gradY1,
                      int xu, int yu, const ptrdiff_t src0Stride, const ptrdiff_t src1Stride, const int widthG,
                      const int bitDepth, int *sumAbsGX, int *sumAbsGY, int *sumDIX, int *sumDIY, int *sumSignGY_GX);
  void(*calcBlkGradient)(int sx, int sy, int    *arraysGx2, int     *arraysGxGy, int     *arraysGxdI, int     *arraysGy2, int     *arraysGydI, int     &sGx2, int     &sGy2, int     &sGxGy, int     &sGxdI, int     &sGydI, int width, int height, int unitSize);
  void (*copyBuffer)(const Pel *src, ptrdiff_t srcStride, Pel *dst, ptrdiff_t dstStride, int width, int height);
  void (*padding)(Pel *dst, ptrdiff_t stride, int width, int height, int padSize);
#if ENABLE_SIMD_OPT_BCW
  void (*removeWeightHighFreq8)(Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride, int width,
                                int height, int bcwWeight, const Pel minVal, const Pel maxVal);
  void (*removeWeightHighFreq4)(Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride, int width,
                                int height, int bcwWeight, const Pel minVal, const Pel maxVal);
  void (*removeHighFreq8)(Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride, int width,
                          int height);
  void (*removeHighFreq4)(Pel *src0, ptrdiff_t src0Stride, const Pel *src1, ptrdiff_t src1Stride, int width,
                          int height);
#endif
  void (*profGradFilter)(Pel *pSrc, ptrdiff_t srcStride, int width, int height, ptrdiff_t gradStride, Pel *gradX,
                         Pel *gradY, const int bitDepth);
  void (*applyPROF)(Pel *dst, ptrdiff_t dstStride, const Pel *src, ptrdiff_t srcStride, int width, int height,
                    const Pel *gradX, const Pel *gradY, ptrdiff_t gradStride, const int *dMvX, const int *dMvY,
                    ptrdiff_t dMvStride, const bool bi, int shiftNum, Pel offset, const ClpRng &clpRng);
  void (*roundIntVector) (int* v, int size, unsigned int nShift, const int dmvLimit);
};

extern PelBufferOps g_pelBufOP;

void paddingCore(Pel *ptr, ptrdiff_t stride, int width, int height, int padSize);
void copyBufferCore(const Pel *src, ptrdiff_t srcStride, Pel *Dst, ptrdiff_t dstStride, int width, int height);

template<typename T>
struct AreaBuf : public Size
{
  T*        buf;
  ptrdiff_t stride;
  // the proper type causes awful lot of errors
  //ptrdiff_t stride;

  AreaBuf() : Size(), buf(nullptr), stride(0) {}
  AreaBuf( T *_buf, const Size &size )                                                    : Size( size ),            buf( _buf ), stride( size.width ) { }
  AreaBuf(T *_buf, const ptrdiff_t &_stride, const Size &size) : Size(size), buf(_buf), stride(_stride) {}
  AreaBuf( T *_buf, const SizeType &_width, const SizeType &_height )                     : Size( _width, _height ), buf( _buf ), stride( _width )     { }
  AreaBuf(T *_buf, const ptrdiff_t &_stride, const SizeType &_width, const SizeType &_height)
    : Size(_width, _height), buf(_buf), stride(_stride)
  {
  }

  operator AreaBuf<const T>() const { return AreaBuf<const T>( buf, stride, width, height ); }

  void fill                 ( const T &val );
  void memset               ( const int val );

  void copyFrom             ( const AreaBuf<const T> &other );
  void roundToOutputBitdepth(const AreaBuf<const T> &src, const ClpRng& clpRng);

  void reconstruct          ( const AreaBuf<const T> &pred, const AreaBuf<const T> &resi, const ClpRng& clpRng);
  void copyClip             ( const AreaBuf<const T> &src, const ClpRng& clpRng);

  void subtract             ( const AreaBuf<const T> &other );
  void extendSingleBorderPel();
  void extendBorderPel      (  unsigned margin );
  void extendBorderPel(unsigned marginX, unsigned marginY);
  void padBorderPel         ( unsigned marginX, unsigned marginY, int dir );
  void addWeightedAvg       ( const AreaBuf<const T> &other1, const AreaBuf<const T> &other2, const ClpRng& clpRng, const int8_t bcwIdx);
  void removeWeightHighFreq ( const AreaBuf<T>& other, const bool bClip, const ClpRng& clpRng, const int8_t iBcwWeight);
  void addAvg               ( const AreaBuf<const T> &other1, const AreaBuf<const T> &other2, const ClpRng& clpRng );
  void removeHighFreq       ( const AreaBuf<T>& other, const bool bClip, const ClpRng& clpRng);
  void updateHistogram      ( std::vector<int32_t>& hist ) const;

  T    meanDiff             ( const AreaBuf<const T> &other ) const;
  void subtract             ( const T val );

  void linearTransform      ( const int scale, const int shift, const int offset, bool bClip, const ClpRng& clpRng );

  void transposedFrom       ( const AreaBuf<const T> &other );

  void rspSignal            ( std::vector<Pel>& pLUT );
  void scaleSignal          ( const int scale, const bool dir , const ClpRng& clpRng);
  void applyLumaCTI(std::vector<Pel>& pLUTY);
  void applyChromaCTI(Pel *bufY, ptrdiff_t strideY, std::vector<Pel> &pLUTUV, int bitDepth, ChromaFormat chrFormat,
                      bool fwdMap);
  T    computeAvg           ( ) const;

        T& at( const int &x, const int &y )          { return buf[y * stride + x]; }
  const T& at( const int &x, const int &y ) const    { return buf[y * stride + x]; }

        T& at( const Position &pos )                 { return buf[pos.y * stride + pos.x]; }
  const T& at( const Position &pos ) const           { return buf[pos.y * stride + pos.x]; }


        T* bufAt( const int &x, const int &y )       { return &at( x, y ); }
  const T* bufAt( const int &x, const int &y ) const { return &at( x, y ); }

        T* bufAt( const Position& pos )              { return &at( pos ); }
  const T* bufAt( const Position& pos ) const        { return &at( pos ); }

  AreaBuf<      T> subBuf( const Position &pos, const Size &size )                                    { return AreaBuf<      T>( bufAt( pos  ), stride, size   ); }
  AreaBuf<const T> subBuf( const Position &pos, const Size &size )                              const { return AreaBuf<const T>( bufAt( pos  ), stride, size   ); }
  AreaBuf<      T> subBuf( const int &x, const int &y, const unsigned &_w, const unsigned &_h )       { return AreaBuf<      T>( bufAt( x, y ), stride, _w, _h ); }
  AreaBuf<const T> subBuf( const int &x, const int &y, const unsigned &_w, const unsigned &_h ) const { return AreaBuf<const T>( bufAt( x, y ), stride, _w, _h ); }
};

typedef AreaBuf<      Pel>  PelBuf;
typedef AreaBuf<const Pel> CPelBuf;

typedef AreaBuf<      TCoeff>  CoeffBuf;
typedef AreaBuf<const TCoeff> CCoeffBuf;

typedef AreaBuf<      MotionInfo>  MotionBuf;
typedef AreaBuf<const MotionInfo> CMotionBuf;

typedef AreaBuf<      TCoeff>  PLTescapeBuf;
typedef AreaBuf<const TCoeff> CPLTescapeBuf;

typedef AreaBuf<PLTRunMode>       PLTtypeBuf;
typedef AreaBuf<const PLTRunMode> CPLTtypeBuf;

#define SIZE_AWARE_PER_EL_OP( OP, INC )                     \
if( ( width & 7 ) == 0 )                                    \
{                                                           \
  for( int y = 0; y < height; y++ )                         \
  {                                                         \
    for( int x = 0; x < width; x += 8 )                     \
    {                                                       \
      OP( x + 0 );                                          \
      OP( x + 1 );                                          \
      OP( x + 2 );                                          \
      OP( x + 3 );                                          \
      OP( x + 4 );                                          \
      OP( x + 5 );                                          \
      OP( x + 6 );                                          \
      OP( x + 7 );                                          \
    }                                                       \
                                                            \
    INC;                                                    \
  }                                                         \
}                                                           \
else if( ( width & 3 ) == 0 )                               \
{                                                           \
  for( int y = 0; y < height; y++ )                         \
  {                                                         \
    for( int x = 0; x < width; x += 4 )                     \
    {                                                       \
      OP( x + 0 );                                          \
      OP( x + 1 );                                          \
      OP( x + 2 );                                          \
      OP( x + 3 );                                          \
    }                                                       \
                                                            \
    INC;                                                    \
  }                                                         \
}                                                           \
else if( ( width & 1 ) == 0 )                               \
{                                                           \
  for( int y = 0; y < height; y++ )                         \
  {                                                         \
    for( int x = 0; x < width; x += 2 )                     \
    {                                                       \
      OP( x + 0 );                                          \
      OP( x + 1 );                                          \
    }                                                       \
                                                            \
    INC;                                                    \
  }                                                         \
}                                                           \
else                                                        \
{                                                           \
  for( int y = 0; y < height; y++ )                         \
  {                                                         \
    for( int x = 0; x < width; x++ )                        \
    {                                                       \
      OP( x );                                              \
    }                                                       \
                                                            \
    INC;                                                    \
  }                                                         \
}

template<typename T>
void AreaBuf<T>::fill(const T &val)
{
  if (width == stride)
  {
    std::fill_n(buf, width * height, val);
  }
  else
  {
    T* dest = buf;

    for (unsigned y = 0; y < height; y++)
    {
      std::fill_n(dest, width, val);

      dest += stride;
    }
  }
}

template<typename T>
void AreaBuf<T>::memset( const int val )
{
  if( width == stride )
  {
    std::fill_n(reinterpret_cast<char *>(buf), width * height * sizeof(T), val);
  }
  else
  {
    T *dest = buf;

    for( int y = 0; y < height; y++ )
    {
      std::fill_n(reinterpret_cast<char *>(dest), width * sizeof(T), val);

      dest += stride;
    }
  }
}

template<typename T>
void AreaBuf<T>::copyFrom( const AreaBuf<const T> &other )
{
#if !defined(__GNUC__) || __GNUC__ > 5
  static_assert( std::is_trivially_copyable<T>::value, "Type T is not trivially_copyable" );
#endif

  CHECK( width  != other.width,  "Incompatible size" );
  CHECK( height != other.height, "Incompatible size" );

  if( buf == other.buf )
  {
    return;
  }

  if( width == stride && stride == other.stride )
  {
    memcpy( buf, other.buf, width * height * sizeof( T ) );
  }
  else
  {
          T* dst         = buf;
    const T* src         = other.buf;
    const ptrdiff_t srcStride   = other.stride;

    for( unsigned y = 0; y < height; y++ )
    {
      memcpy( dst, src, width * sizeof( T ) );

      dst += stride;
      src += srcStride;
    }
  }
}


template<typename T>
void AreaBuf<T>::subtract( const AreaBuf<const T> &other )
{
  CHECK( width  != other.width,  "Incompatible size" );
  CHECK( height != other.height, "Incompatible size" );

        T* dest =       buf;
  const T* subs = other.buf;

#define SUBS_INC        \
  dest +=       stride; \
  subs += other.stride; \

#define SUBS_OP( ADDR ) dest[ADDR] -= subs[ADDR]

  SIZE_AWARE_PER_EL_OP( SUBS_OP, SUBS_INC );

#undef SUBS_OP
#undef SUBS_INC
}


template<typename T>
void AreaBuf<T>::copyClip( const AreaBuf<const T> &src, const ClpRng& clpRng )
{
  THROW( "Type not supported" );
}

template<>
void AreaBuf<Pel>::copyClip( const AreaBuf<const Pel> &src, const ClpRng& clpRng );

template<typename T>
void AreaBuf<T>::reconstruct( const AreaBuf<const T> &pred, const AreaBuf<const T> &resi, const ClpRng& clpRng )
{
  THROW( "Type not supported" );
}

template<>
void AreaBuf<Pel>::reconstruct( const AreaBuf<const Pel> &pred, const AreaBuf<const Pel> &resi, const ClpRng& clpRng );


template<typename T>
void AreaBuf<T>::addAvg( const AreaBuf<const T> &other1, const AreaBuf<const T> &other2, const ClpRng& clpRng )
{
  THROW( "Type not supported" );
}

template<>
void AreaBuf<Pel>::addAvg( const AreaBuf<const Pel> &other1, const AreaBuf<const Pel> &other2, const ClpRng& clpRng );

template<typename T>
void AreaBuf<T>::linearTransform( const int scale, const int shift, const int offset, bool bClip, const ClpRng& clpRng )
{
  THROW( "Type not supported" );
}

template<>
void AreaBuf<Pel>::linearTransform( const int scale, const int shift, const int offset, bool bClip, const ClpRng& clpRng );

template<typename T>
void AreaBuf<T>::removeWeightHighFreq(const AreaBuf<T> &other, const bool clampToNominalRange, const ClpRng &clpRng,
                                      const int8_t bcwWeight)
{
  const Pel *src        = other.buf;
  const ptrdiff_t  srcStride = other.stride;

  Pel *     dst        = buf;
  const ptrdiff_t dstStride = stride;

  const Pel minVal = clampToNominalRange ? clpRng.min : 5 * clpRng.min - 4 * clpRng.max;
  const Pel maxVal = clampToNominalRange ? clpRng.max : 5 * clpRng.max - 4 * clpRng.min;

#if ENABLE_SIMD_OPT_BCW
  if ((width & 7) == 0 && g_pelBufOP.removeWeightHighFreq8)
  {
    g_pelBufOP.removeWeightHighFreq8(dst, dstStride, src, srcStride, width, height, bcwWeight, minVal, maxVal);
  }
  else if ((width & 3) == 0 && g_pelBufOP.removeWeightHighFreq4)
  {
    g_pelBufOP.removeWeightHighFreq4(dst, dstStride, src, srcStride, width, height, bcwWeight, minVal, maxVal);
  }
  else
#endif
  {
    const int32_t w =
      ((BCW_WEIGHT_BASE << BCW_INV_BITS) + (bcwWeight > 0 ? (bcwWeight >> 1) : -(bcwWeight >> 1))) / bcwWeight;

#define REM_HF_INC                                                                                                     \
  src += srcStride;                                                                                                    \
  dst += dstStride;

#define REM_HF_OP_CLIP(ADDR)                                                                                           \
  dst[ADDR] =                                                                                                          \
    Clip3<T>(minVal, maxVal, (((dst[ADDR] - src[ADDR]) * w + (1 << BCW_INV_BITS >> 1)) >> BCW_INV_BITS) + src[ADDR])
    SIZE_AWARE_PER_EL_OP(REM_HF_OP_CLIP, REM_HF_INC);
#undef REM_HF_OP_CLIP
#undef REM_HF_INC
  }
}

template<typename T>
void AreaBuf<T>::removeHighFreq(const AreaBuf<T> &other, const bool clampToNominalRange, const ClpRng &clpRng)
{
  const T * src        = other.buf;
  const ptrdiff_t srcStride = other.stride;

  T *       dst        = buf;
  const ptrdiff_t dstStride = stride;

#define REM_HF_INC                                                                                                     \
  src += srcStride;                                                                                                    \
  dst += dstStride;

  if (!clampToNominalRange)
  {
#if ENABLE_SIMD_OPT_BCW
    if (!(width & 7) && g_pelBufOP.removeHighFreq8)
    {
      g_pelBufOP.removeHighFreq8(dst, dstStride, src, srcStride, width, height);
      return;
    }
    else if (!(width & 3) && g_pelBufOP.removeHighFreq4)
    {
      g_pelBufOP.removeHighFreq4(dst, dstStride, src, srcStride, width, height);
      return;
    }
#endif
#define REM_HF_OP(ADDR) dst[ADDR] = 2 * dst[ADDR] - src[ADDR]
    SIZE_AWARE_PER_EL_OP(REM_HF_OP, REM_HF_INC);
  }
  else
  {
#define REM_HF_OP_CLIP(ADDR) dst[ADDR] = ClipPel<T>(2 * dst[ADDR] - src[ADDR], clpRng)
    SIZE_AWARE_PER_EL_OP(REM_HF_OP_CLIP, REM_HF_INC);
  }

#undef REM_HF_INC
#undef REM_HF_OP
#undef REM_HF_OP_CLIP
}


template<typename T>
void AreaBuf<T>::updateHistogram( std::vector<int32_t>& hist ) const
{
  const T* data = buf;
  for( std::size_t y = 0; y < height; y++, data += stride )
  {
    for( std::size_t x = 0; x < width; x++ )
    {
      hist[ data[x] ]++;
    }
  }
}

template<typename T>
void AreaBuf<T>::extendBorderPel(unsigned marginX, unsigned marginY)
{
  T* p = buf;
  int h = height;
  int w = width;
  ptrdiff_t s = stride;

  CHECK((w + 2 * marginX) > s, "Size of buffer too small to extend");
  // do left and right margins
  for (int y = 0; y < h; y++)
  {
    for (int x = 0; x < marginX; x++)
    {
      *(p - marginX + x) = p[0];
      p[w + x] = p[w - 1];
    }
    p += s;
  }

  // p is now the (0,height) (bottom left of image within bigger picture
  p -= (s + marginX);
  // p is now the (-margin, height-1)
  for (int y = 0; y < marginY; y++)
  {
    ::memcpy(p + (y + 1) * s, p, sizeof(T) * (w + (marginX << 1)));
  }

  // p is still (-marginX, height-1)
  p -= ((h - 1) * s);
  // p is now (-marginX, 0)
  for (int y = 0; y < marginY; y++)
  {
    ::memcpy(p - (y + 1) * s, p, sizeof(T) * (w + (marginX << 1)));
  }
}

template<typename T>
void AreaBuf<T>::padBorderPel( unsigned marginX, unsigned marginY, int dir )
{
  T*  p = buf;
  ptrdiff_t s = stride;
  int h = height;
  int w = width;

  CHECK( w  > s, "Size of buffer too small to extend" );

  // top-left margin
  if ( dir == 1 )
  {
    for( int y = 0; y < marginY; y++ )
    {
      for( int x = 0; x < marginX; x++ )
      {
        p[x] = p[marginX];
      }
      p += s;
    }
  }

  // bottom-right margin
  if ( dir == 2 )
  {
    p = buf + s * ( h - marginY ) + w - marginX;

    for( int y = 0; y < marginY; y++ )
    {
      for( int x = 0; x < marginX; x++ )
      {
        p[x] = p[-1];
      }
      p += s;
    }
  }
}

template<typename T>
void AreaBuf<T>::extendBorderPel( unsigned margin )
{
  T*  p = buf;
  int h = height;
  int w = width;
  ptrdiff_t s = stride;

  CHECK( ( w + 2 * margin ) > s, "Size of buffer too small to extend" );
  // do left and right margins
  for( int y = 0; y < h; y++ )
  {
    for( int x = 0; x < margin; x++ )
    {
      *( p - margin + x ) = p[0];
      p[w + x] = p[w - 1];
    }
    p += s;
  }

  // p is now the (0,height) (bottom left of image within bigger picture
  p -= ( s + margin );
  // p is now the (-margin, height-1)
  for( int y = 0; y < margin; y++ )
  {
    ::memcpy( p + ( y + 1 ) * s, p, sizeof( T ) * ( w + ( margin << 1 ) ) );
  }

  // pi is still (-marginX, height-1)
  p -= ( ( h - 1 ) * s );
  // pi is now (-marginX, 0)
  for( int y = 0; y < margin; y++ )
  {
    ::memcpy( p - ( y + 1 ) * s, p, sizeof( T ) * ( w + ( margin << 1 ) ) );
  }
}

template<typename T>
T AreaBuf<T>::meanDiff( const AreaBuf<const T> &other ) const
{
  int64_t acc = 0;

  CHECK( width  != other.width,  "Incompatible size" );
  CHECK( height != other.height, "Incompatible size" );

  const T* src1 =       buf;
  const T* src2 = other.buf;

#define MEAN_DIFF_INC   \
  src1 +=       stride; \
  src2 += other.stride; \

#define MEAN_DIFF_OP(ADDR) acc += src1[ADDR] - src2[ADDR]

  SIZE_AWARE_PER_EL_OP( MEAN_DIFF_OP, MEAN_DIFF_INC );

#undef MEAN_DIFF_INC
#undef MEAN_DIFF_OP

  return T( acc / area() );
}

#if ENABLE_SIMD_OPT_BUFFER && defined(TARGET_SIMD_X86)
template<> void AreaBuf<Pel>::subtract( const Pel val );
#endif

template<typename T>
void AreaBuf<T>::subtract( const T val )
{
  T* dst = buf;

#define OFFSET_INC       dst       += stride
#define OFFSET_OP(ADDR)  dst[ADDR] -= val

  SIZE_AWARE_PER_EL_OP( OFFSET_OP, OFFSET_INC );

#undef OFFSET_INC
#undef OFFSET_OP
}

template<typename T>
void AreaBuf<T>::transposedFrom( const AreaBuf<const T> &other )
{
  CHECK( width * height != other.width * other.height, "Incompatible size" );

        T* dst  =       buf;
  const T* src  = other.buf;
  width         = other.height;
  height        = other.width;
  stride        = stride < width ? width : stride;

  for( unsigned y = 0; y < other.height; y++ )
  {
    for( unsigned x = 0; x < other.width; x++ )
    {
      dst[y + x*stride] = src[x + y*other.stride];
    }
  }
}

template<typename T>
T AreaBuf <T> ::computeAvg() const
{
    const T* src = buf;
#if ENABLE_QPA
    int64_t  acc = 0; // for picture-wise use in getGlaringColorQPOffset() and applyQPAdaptationChroma()
#else
    int32_t  acc = 0;
#endif
#define AVG_INC      src += stride
#define AVG_OP(ADDR) acc += src[ADDR]
    SIZE_AWARE_PER_EL_OP(AVG_OP, AVG_INC);
#undef AVG_INC
#undef AVG_OP
    return T ((acc + (area() >> 1)) / area());
}

#ifndef DONT_UNDEF_SIZE_AWARE_PER_EL_OP
#undef SIZE_AWARE_PER_EL_OP
#endif // !DONT_UNDEF_SIZE_AWARE_PER_EL_OP

// ---------------------------------------------------------------------------
// UnitBuf struct
// ---------------------------------------------------------------------------

struct UnitArea;

template<typename T>
struct UnitBuf
{
  typedef static_vector<AreaBuf<T>,       MAX_NUM_COMPONENT> UnitBufBuffers;
  typedef static_vector<AreaBuf<const T>, MAX_NUM_COMPONENT> ConstUnitBufBuffers;

  ChromaFormat chromaFormat;
  UnitBufBuffers bufs;

  UnitBuf() : chromaFormat(ChromaFormat::UNDEFINED) {}
  UnitBuf( const ChromaFormat &_chromaFormat, const UnitBufBuffers&  _bufs ) : chromaFormat( _chromaFormat ), bufs( _bufs ) { }
  UnitBuf( const ChromaFormat &_chromaFormat,       UnitBufBuffers&& _bufs ) : chromaFormat( _chromaFormat ), bufs( std::forward<UnitBufBuffers>( _bufs ) ) { }
  UnitBuf( const ChromaFormat &_chromaFormat, const AreaBuf<T>  &blkY ) : chromaFormat( _chromaFormat ), bufs{ blkY } { }
  UnitBuf( const ChromaFormat &_chromaFormat,       AreaBuf<T> &&blkY ) : chromaFormat( _chromaFormat ), bufs{ std::forward<AreaBuf<T> >(blkY) } { }
  UnitBuf( const ChromaFormat &_chromaFormat, const AreaBuf<T>  &blkY, const AreaBuf<T>  &blkCb, const AreaBuf<T>  &blkCr ) : chromaFormat( _chromaFormat ), bufs{ blkY, blkCb, blkCr } { }
  UnitBuf( const ChromaFormat &_chromaFormat,       AreaBuf<T> &&blkY,       AreaBuf<T> &&blkCb,       AreaBuf<T> &&blkCr ) : chromaFormat( _chromaFormat ), bufs{ std::forward<AreaBuf<T> >(blkY), std::forward<AreaBuf<T> >(blkCb), std::forward<AreaBuf<T> >(blkCr) } { }

  operator UnitBuf<const T>() const
  {
    return UnitBuf<const T>( chromaFormat, ConstUnitBufBuffers( bufs.begin(), bufs.end() ) );
  }

        AreaBuf<T>& get( const ComponentID comp )        { return bufs[comp]; }
  const AreaBuf<T>& get( const ComponentID comp )  const { return bufs[comp]; }

        AreaBuf<T>& Y()        { return bufs[0]; }
  const AreaBuf<T>& Y()  const { return bufs[0]; }
        AreaBuf<T>& Cb()       { return bufs[1]; }
  const AreaBuf<T>& Cb() const { return bufs[1]; }
        AreaBuf<T>& Cr()       { return bufs[2]; }
  const AreaBuf<T>& Cr() const { return bufs[2]; }

  void fill                 ( const T &val );
  void copyFrom             ( const UnitBuf<const T> &other, const bool lumaOnly = false, const bool chromaOnly = false );
  void roundToOutputBitdepth(const UnitBuf<const T> &src, const ClpRngs& clpRngs);
  void reconstruct          ( const UnitBuf<const T> &pred, const UnitBuf<const T> &resi, const ClpRngs& clpRngs );
  void copyClip             ( const UnitBuf<const T> &src, const ClpRngs& clpRngs, const bool lumaOnly = false, const bool chromaOnly = false );
  void subtract             ( const UnitBuf<const T> &other );
  void addWeightedAvg       ( const UnitBuf<const T> &other1, const UnitBuf<const T> &other2, const ClpRngs& clpRngs, const uint8_t bcwIdx = BCW_DEFAULT, const bool chromaOnly = false, const bool lumaOnly = false);
  void addAvg               ( const UnitBuf<const T> &other1, const UnitBuf<const T> &other2, const ClpRngs& clpRngs, const bool chromaOnly = false, const bool lumaOnly = false);
  void extendSingleBorderPel();
  void extendBorderPel(unsigned marginX, unsigned marginY);
  void padBorderPel         ( unsigned margin, int dir );
  void extendBorderPel      ( unsigned margin );
  void removeHighFreq       ( const UnitBuf<T>& other, const bool bClip, const ClpRngs& clpRngs
                            , const int8_t bcwWeight = g_BcwWeights[BCW_DEFAULT]
                            );

        UnitBuf<      T> subBuf (const UnitArea& subArea);
  const UnitBuf<const T> subBuf (const UnitArea& subArea) const;
  void colorSpaceConvert(const UnitBuf<T> &other, const bool forward, const ClpRng& clpRng);
};

typedef UnitBuf<      Pel>  PelUnitBuf;
typedef UnitBuf<const Pel> CPelUnitBuf;

typedef UnitBuf<      TCoeff>  CoeffUnitBuf;
typedef UnitBuf<const TCoeff> CCoeffUnitBuf;

template<typename T>
void UnitBuf<T>::fill( const T &val )
{
  for( unsigned i = 0; i < bufs.size(); i++ )
  {
    bufs[i].fill( val );
  }
}

template<typename T>
void UnitBuf<T>::copyFrom(const UnitBuf<const T> &other, const bool lumaOnly, const bool chromaOnly )
{
  CHECK( chromaFormat != other.chromaFormat, "Incompatible formats" );

  CHECK( lumaOnly && chromaOnly, "Not allowed to have both lumaOnly and chromaOnly selected" );
  const size_t compStart = chromaOnly ? 1 : 0;
  const size_t compEnd   = lumaOnly ? 1 : (unsigned) bufs.size();
  for( size_t i = compStart; i < compEnd; i++ )
  {
    bufs[i].copyFrom( other.bufs[i] );
  }
}



template<typename T>
void UnitBuf<T>::subtract( const UnitBuf<const T> &other )
{
  CHECK( chromaFormat != other.chromaFormat, "Incompatible formats" );

  for( unsigned i = 0; i < bufs.size(); i++ )
  {
    bufs[i].subtract( other.bufs[i] );
  }
}

template<typename T>
void UnitBuf<T>::copyClip(const UnitBuf<const T> &src, const ClpRngs &clpRngs, const bool lumaOnly, const bool chromaOnly )
{
  CHECK( chromaFormat != src.chromaFormat, "Incompatible formats" );

  CHECK( lumaOnly && chromaOnly, "Not allowed to have both lumaOnly and chromaOnly selected" );
  const size_t compStart = chromaOnly ? 1 : 0;
  const size_t compEnd   = lumaOnly ? 1 : bufs.size();
  for( size_t i = compStart; i < compEnd; i++ )
  {
    bufs[i].copyClip( src.bufs[i], clpRngs.comp[i] );
  }
}


template<typename T>
void UnitBuf<T>::roundToOutputBitdepth(const UnitBuf<const T> &src, const ClpRngs& clpRngs)
{
  CHECK(chromaFormat != src.chromaFormat, "Incompatible formats");

  for (unsigned i = 0; i < bufs.size(); i++)
  {
    bufs[i].roundToOutputBitdepth(src.bufs[i], clpRngs.comp[i]);
  }
}

template<typename T>
void UnitBuf<T>::reconstruct(const UnitBuf<const T> &pred, const UnitBuf<const T> &resi, const ClpRngs& clpRngs)
{
  CHECK( chromaFormat != pred.chromaFormat, "Incompatible formats" );
  CHECK( chromaFormat != resi.chromaFormat, "Incompatible formats" );

  for( unsigned i = 0; i < bufs.size(); i++ )
  {
    bufs[i].reconstruct( pred.bufs[i], resi.bufs[i], clpRngs.comp[i] );
  }
}

template<typename T>
void UnitBuf<T>::addWeightedAvg(const UnitBuf<const T> &other1, const UnitBuf<const T> &other2, const ClpRngs& clpRngs, const uint8_t bcwIdx /* = BCW_DEFAULT */, const bool chromaOnly /* = false */, const bool lumaOnly /* = false */)
{
  const size_t istart = chromaOnly ? 1 : 0;
  const size_t iend = lumaOnly ? 1 : bufs.size();

  CHECK(lumaOnly && chromaOnly, "should not happen");

  for(size_t i = istart; i < iend; i++)
  {
    bufs[i].addWeightedAvg(other1.bufs[i], other2.bufs[i], clpRngs.comp[i], bcwIdx);
  }
}

template<typename T>
void UnitBuf<T>::addAvg(const UnitBuf<const T> &other1, const UnitBuf<const T> &other2, const ClpRngs& clpRngs, const bool chromaOnly /* = false */, const bool lumaOnly /* = false */)
{
  const size_t istart = chromaOnly ? 1 : 0;
  const size_t iend   = lumaOnly   ? 1 : bufs.size();

  CHECK( lumaOnly && chromaOnly, "should not happen" );

  for( size_t i = istart; i < iend; i++)
  {
    bufs[i].addAvg( other1.bufs[i], other2.bufs[i], clpRngs.comp[i]);
  }
}

template<typename T>
void UnitBuf<T>::colorSpaceConvert(const UnitBuf<T> &other, const bool forward, const ClpRng& clpRng)
{
  THROW("Type not supported");
}

template<>
void UnitBuf<Pel>::colorSpaceConvert(const UnitBuf<Pel> &other, const bool forward, const ClpRng& clpRng);

template<typename T>
void UnitBuf<T>::extendSingleBorderPel()
{
  for( unsigned i = 0; i < bufs.size(); i++ )
  {
    bufs[i].extendSingleBorderPel();
  }
}

template<typename T>
void UnitBuf<T>::extendBorderPel(unsigned marginX, unsigned marginY)
{
  for (unsigned i = 0; i < bufs.size(); i++)
  {
    bufs[i].extendBorderPel(marginX >> getComponentScaleX(ComponentID(i), chromaFormat), marginY >> getComponentScaleY(ComponentID(i), chromaFormat));
  }
}

template<typename T>
void UnitBuf<T>::padBorderPel( unsigned margin, int dir )
{
  for( unsigned i = 0; i < bufs.size(); i++ )
  {
    bufs[i].padBorderPel( margin >> getComponentScaleX( ComponentID( i ), chromaFormat ), margin >> getComponentScaleY( ComponentID( i ), chromaFormat ), dir );
  }
}

template<typename T>
void UnitBuf<T>::extendBorderPel( unsigned margin )
{
  for( unsigned i = 0; i < bufs.size(); i++ )
  {
    bufs[i].extendBorderPel( margin );
  }
}

template<typename T>
void UnitBuf<T>::removeHighFreq( const UnitBuf<T>& other, const bool bClip, const ClpRngs& clpRngs
                               , const int8_t bcwWeight
                               )
{
  if(bcwWeight != g_BcwWeights[BCW_DEFAULT])
  {
    bufs[0].removeWeightHighFreq(other.bufs[0], bClip, clpRngs.comp[0], bcwWeight);
    return;
  }
  bufs[0].removeHighFreq(other.bufs[0], bClip, clpRngs.comp[0]);

}

template<typename T>
UnitBuf<T> UnitBuf<T>::subBuf( const UnitArea& subArea )
{
  UnitBuf<T> subBuf;
  subBuf.chromaFormat = chromaFormat;
  unsigned blockIdx = 0;

  for( auto &subAreaBuf : bufs )
  {
    subBuf.bufs.push_back( subAreaBuf.subBuf( subArea.blocks[blockIdx].pos(), subArea.blocks[blockIdx].size() ) );
    blockIdx++;
  }

  return subBuf;
}


template<typename T>
const UnitBuf<const T> UnitBuf<T>::subBuf( const UnitArea& subArea ) const
{
  UnitBuf<const T> subBuf;
  subBuf.chromaFormat = chromaFormat;
  unsigned blockIdx = 0;

  for( const auto &subAreaBuf : bufs )
  {
    subBuf.bufs.push_back( subAreaBuf.subBuf( subArea.blocks[blockIdx].pos(), subArea.blocks[blockIdx].size() ) );
    blockIdx++;
  }

  return subBuf;
}

// ---------------------------------------------------------------------------
// PelStorage struct (PelUnitBuf which allocates its own memory)
// ---------------------------------------------------------------------------

struct UnitArea;
struct CompArea;

struct PelStorage : public PelUnitBuf
{
  PelStorage();
  ~PelStorage();

  void swap( PelStorage& other );
  void createFromBuf( PelUnitBuf buf );
  void create( const UnitArea &_unit );
  void create( const ChromaFormat &_chromaFormat, const Area& _area, const unsigned _maxCUSize = 0, const unsigned _margin = 0, const unsigned _alignment = 0, const bool _scaleChromaMargin = true );
  void destroy();

  PelBuf getBuf(const ComponentID CompID) { return bufs[CompID]; }

  const CPelBuf getBuf(const ComponentID CompID) const { return bufs[CompID]; }

  PelBuf getBuf(const CompArea &blk)
  {
    const PelBuf &r = bufs[blk.compID];

    CHECKD(rsAddr(blk.bottomRight(), r.stride) >= ((r.height - 1) * r.stride + r.width),
           "Trying to access a buf outside of bound!");

    return PelBuf(r.buf + rsAddr(blk, r.stride), r.stride, blk);
  }

  const CPelBuf getBuf(const CompArea &blk) const
  {
    const PelBuf &r = bufs[blk.compID];
    return CPelBuf(r.buf + rsAddr(blk, r.stride), r.stride, blk);
  }

  PelUnitBuf getBuf(const UnitArea &unit)
  {
    return !isChromaEnabled(chromaFormat)
             ? PelUnitBuf(chromaFormat, getBuf(unit.Y()))
             : PelUnitBuf(chromaFormat, getBuf(unit.Y()), getBuf(unit.Cb()), getBuf(unit.Cr()));
  }

  const CPelUnitBuf getBuf(const UnitArea &unit) const
  {
    return !isChromaEnabled(chromaFormat)
             ? CPelUnitBuf(chromaFormat, getBuf(unit.Y()))
             : CPelUnitBuf(chromaFormat, getBuf(unit.Y()), getBuf(unit.Cb()), getBuf(unit.Cr()));
  }

  Pel *getOrigin(const int id) const { return m_origin[id]; }

private:
  Pel *m_origin[MAX_NUM_COMPONENT];
};

struct CompStorage : public PelBuf
{
  CompStorage () { m_memory = nullptr; }
  ~CompStorage() { if (valid()) delete [] m_memory; }

  void create( const Size& size )
  {
    CHECK( m_memory, "Trying to re-create an already initialized buffer" );
    m_memory = new Pel [ size.area() ];
    *static_cast<PelBuf*>(this) = PelBuf( m_memory, size );
  }
  void destroy()
  {
    if (valid()) delete [] m_memory;
    m_memory = nullptr;
  }
  bool valid() { return m_memory != nullptr; }
private:
  Pel* m_memory;
};

class PelUnitBufPool
{
private:
  Pool<PelStorage> m_pelStoragePool;
  Pool<PelUnitBuf> m_pelUnitBufPool;
  std::unordered_map<PelUnitBuf*, PelStorage*> m_map;
  ChromaFormat m_chromaFormat;
  Area m_ctuArea;

public:
  PelUnitBufPool();
  ~PelUnitBufPool();

  void initPelUnitBufPool(ChromaFormat chromaFormat, int ctuWidth, int ctuHeight);
  PelUnitBuf* getPelUnitBuf(const UnitArea& unitArea);
  void giveBack(PelUnitBuf* p);

  template<size_t N>
  void giveBack(static_vector<PelUnitBuf*, N>& v)
  {
    for (auto p : v)
    {
      giveBack(p);
    }
    v.clear();
  }
};

template<size_t N>
class PelUnitBufVector : public static_vector<PelUnitBuf*, N>
{
private:
  PelUnitBufPool* m_pool;

public:
  PelUnitBufVector(PelUnitBufPool& pool) : m_pool(&pool) {}
  ~PelUnitBufVector()
  {
    m_pool->giveBack(*this);
  }
};
#endif
