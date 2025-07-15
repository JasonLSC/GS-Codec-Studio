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

/** \file     TComPicYuv.cpp
    \brief    picture YUV buffer class
*/

#include <cstdlib>
#include <assert.h>
#include <memory.h>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include "TComPicYuv.h"
#include "Utilities/TVideoIOYuv.h"

//! \ingroup TLibCommon
//! \{

TComPicYuv::TComPicYuv()
{
  for(UInt i=0; i<MAX_NUM_COMPONENT; i++)
  {
    m_apiPicBuf[i]    = NULL;   // Buffer (including margin)
    m_piPicOrg[i]     = NULL;    // m_apiPicBufY + m_iMarginLuma*getStride() + m_iMarginLuma
  }

  for(UInt i=0; i<MAX_NUM_CHANNEL_TYPE; i++)
  {
    m_ctuOffsetInBuffer[i]=0;
    m_subCuOffsetInBuffer[i]=0;
  }

  m_bIsBorderExtended = false;
}




TComPicYuv::~TComPicYuv()
{
  destroy();
}


#if JVET_X0048_X0103_FILM_GRAIN
Void TComPicYuv::createWithPadding ( const Int picWidth,                 ///< picture width
                                     const Int picHeight,                ///< picture height
                                     const ChromaFormat chromaFormatIDC, ///< chroma format
                                     const Bool bUseMargin,              ///< if true, then a margin of padWidth and padHeight is created around the image.
                                     const Bool bScaleMarginChroma,      ///< if true, margin is scaled based on chroma format. Otherwise, the same padding is used for all components.
                                     const UInt padWidth,                ///< used for margin only
                                     const UInt padHeight)               ///< used for margin only
{
  destroy();

  m_picWidth = picWidth;
  m_picHeight = picHeight;
  m_chromaFormatIDC = chromaFormatIDC;
  m_marginX = bUseMargin ? padWidth : 0;
  m_marginY = bUseMargin ? padHeight : 0;
  m_bIsBorderExtended = false;

  // assign the picture arrays and set up the ptr to the top left of the original picture
  for (UInt comp = 0; comp<getNumberValidComponents(); comp++)
  {
    const ComponentID ch = ComponentID(comp);
    UInt componentScaleX = getComponentScaleX(ch);
    UInt componentScaleY = getComponentScaleY(ch);

    UInt scaledWidth = picWidth >> componentScaleX;
    UInt scaledHeight = picHeight >> componentScaleY;

    unsigned xmargin = m_marginX >> (bScaleMarginChroma ? componentScaleX : 0);
    unsigned ymargin = m_marginY >> (bScaleMarginChroma ? componentScaleY : 0);

    Int  stride          = scaledWidth + 2 * xmargin;
    Int  totalHeight     = scaledHeight + 2 * ymargin;

    m_apiPicBuf[comp] = (Pel*)xMalloc(Pel, stride * totalHeight);
    m_piPicOrg[comp] = m_apiPicBuf[comp] + ymargin * stride + xmargin;
  }
  // initialize pointers for unused components to NULL
  for (UInt comp = getNumberValidComponents(); comp<MAX_NUM_COMPONENT; comp++)
  {
    m_apiPicBuf[comp] = NULL;
    m_piPicOrg[comp] = NULL;
  }

  for (Int chan = 0; chan<MAX_NUM_CHANNEL_TYPE; chan++)
  {
    m_ctuOffsetInBuffer[chan] = NULL;
    m_subCuOffsetInBuffer[chan] = NULL;
  }
}
#endif

Void TComPicYuv::createWithoutCUInfo ( const Int picWidth,                 ///< picture width
                                       const Int picHeight,                ///< picture height
                                       const ChromaFormat chromaFormatIDC, ///< chroma format
                                       const Bool bUseMargin,              ///< if true, then a margin of uiMaxCUWidth+16 and uiMaxCUHeight+16 is created around the image.
                                       const UInt maxCUWidth,              ///< used for margin only
                                       const UInt maxCUHeight)             ///< used for margin only

{
  destroy();

  m_picWidth          = picWidth;
  m_picHeight         = picHeight;
  m_chromaFormatIDC   = chromaFormatIDC;
  m_marginX          = (bUseMargin?maxCUWidth:0) + 16;   // for 16-byte alignment
  m_marginY          = (bUseMargin?maxCUHeight:0) + 16;  // margin for 8-tap filter and infinite padding
  m_bIsBorderExtended = false;

  // assign the picture arrays and set up the ptr to the top left of the original picture
  for(UInt comp=0; comp<getNumberValidComponents(); comp++)
  {
    const ComponentID ch=ComponentID(comp);
    m_apiPicBuf[comp] = (Pel*)xMalloc( Pel, getStride(ch) * getTotalHeight(ch));
    m_piPicOrg[comp]  = m_apiPicBuf[comp] + (m_marginY >> getComponentScaleY(ch)) * getStride(ch) + (m_marginX >> getComponentScaleX(ch));
  }
  // initialize pointers for unused components to NULL
  for(UInt comp=getNumberValidComponents();comp<MAX_NUM_COMPONENT; comp++)
  {
    m_apiPicBuf[comp] = NULL;
    m_piPicOrg[comp]  = NULL;
  }

  for(Int chan=0; chan<MAX_NUM_CHANNEL_TYPE; chan++)
  {
    m_ctuOffsetInBuffer[chan]   = NULL;
    m_subCuOffsetInBuffer[chan] = NULL;
  }
}



Void TComPicYuv::create ( const Int picWidth,                 ///< picture width
                          const Int picHeight,                ///< picture height
                          const ChromaFormat chromaFormatIDC, ///< chroma format
                          const UInt maxCUWidth,              ///< used for generating offsets to CUs.
                          const UInt maxCUHeight,             ///< used for generating offsets to CUs.
                          const UInt maxCUDepth,              ///< used for generating offsets to CUs.
                          const Bool bUseMargin)              ///< if true, then a margin of uiMaxCUWidth+16 and uiMaxCUHeight+16 is created around the image.

{
  createWithoutCUInfo(picWidth, picHeight, chromaFormatIDC, bUseMargin, maxCUWidth, maxCUHeight);


  const Int numCuInWidth  = m_picWidth  / maxCUWidth  + (m_picWidth  % maxCUWidth  != 0);
  const Int numCuInHeight = m_picHeight / maxCUHeight + (m_picHeight % maxCUHeight != 0);
  for(Int chan=0; chan<MAX_NUM_CHANNEL_TYPE; chan++)
  {
    const ChannelType ch= ChannelType(chan);
    const Int ctuHeight = maxCUHeight>>getChannelTypeScaleY(ch);
    const Int ctuWidth  = maxCUWidth>>getChannelTypeScaleX(ch);
    const Int stride    = getStride(ch);

    m_ctuOffsetInBuffer[chan] = new Int[numCuInWidth * numCuInHeight];

    for (Int cuRow = 0; cuRow < numCuInHeight; cuRow++)
    {
      for (Int cuCol = 0; cuCol < numCuInWidth; cuCol++)
      {
        m_ctuOffsetInBuffer[chan][cuRow * numCuInWidth + cuCol] = stride * cuRow * ctuHeight + cuCol * ctuWidth;
      }
    }

    m_subCuOffsetInBuffer[chan] = new Int[(size_t)1 << (2 * maxCUDepth)];

    const Int numSubBlockPartitions=(1<<maxCUDepth);
    const Int minSubBlockHeight    =(ctuHeight >> maxCUDepth);
    const Int minSubBlockWidth     =(ctuWidth  >> maxCUDepth);

    for (Int buRow = 0; buRow < numSubBlockPartitions; buRow++)
    {
      for (Int buCol = 0; buCol < numSubBlockPartitions; buCol++)
      {
        m_subCuOffsetInBuffer[chan][(buRow << maxCUDepth) + buCol] = stride  * buRow * minSubBlockHeight + buCol * minSubBlockWidth;
      }
    }
  }
}

Void TComPicYuv::destroy()
{
  for(Int comp=0; comp<MAX_NUM_COMPONENT; comp++)
  {
    m_piPicOrg[comp] = NULL;

    if( m_apiPicBuf[comp] )
    {
      xFree( m_apiPicBuf[comp] );
      m_apiPicBuf[comp] = NULL;
    }
  }

  for(UInt chan=0; chan<MAX_NUM_CHANNEL_TYPE; chan++)
  {
    if (m_ctuOffsetInBuffer[chan])
    {
      delete[] m_ctuOffsetInBuffer[chan];
      m_ctuOffsetInBuffer[chan] = NULL;
    }
    if (m_subCuOffsetInBuffer[chan])
    {
      delete[] m_subCuOffsetInBuffer[chan];
      m_subCuOffsetInBuffer[chan] = NULL;
    }
  }
}



Void  TComPicYuv::copyToPic (TComPicYuv*  pcPicYuvDst) const
{
  assert( m_chromaFormatIDC == pcPicYuvDst->getChromaFormat() );

  for(Int comp=0; comp<getNumberValidComponents(); comp++)
  {
    const ComponentID compId=ComponentID(comp);
    const Int width     = getWidth(compId);
    const Int height    = getHeight(compId);
    const Int strideSrc = getStride(compId);
    assert(pcPicYuvDst->getWidth(compId) == width);
    assert(pcPicYuvDst->getHeight(compId) == height);
    if (strideSrc==pcPicYuvDst->getStride(compId))
    {
      ::memcpy ( pcPicYuvDst->getBuf(compId), getBuf(compId), sizeof(Pel)*strideSrc*getTotalHeight(compId));
    }
    else
    {
      const Pel *pSrc       = getAddr(compId);
            Pel *pDest      = pcPicYuvDst->getAddr(compId);
      const UInt strideDest = pcPicYuvDst->getStride(compId);

      for(Int y=0; y<height; y++, pSrc+=strideSrc, pDest+=strideDest)
      {
        ::memcpy(pDest, pSrc, width*sizeof(Pel));
      }
    }
  }
}

#if JVET_X0048_X0103_FILM_GRAIN
Void  TComPicYuv::copyToPic(TComPicYuv* pcPicYuvDst, const Bool bScaleMarginChromaSrc, const Bool bScaleMarginChromaDst) const
{
  assert(m_chromaFormatIDC == pcPicYuvDst->getChromaFormat());

  for (Int comp = 0; comp < getNumberValidComponents(); comp++)
  {
    const ComponentID compId = ComponentID(comp);
    const Int width = getWidth(compId);
    const Int height = getHeight(compId);
    const Int strideSrc = getStride(compId, bScaleMarginChromaSrc);
    assert(pcPicYuvDst->getWidth(compId) == width);
    assert(pcPicYuvDst->getHeight(compId) == height);
    if (strideSrc == pcPicYuvDst->getStride(compId, bScaleMarginChromaDst))
    {
      ::memcpy(pcPicYuvDst->getBuf(compId), getBuf(compId), sizeof(Pel) * strideSrc * getTotalHeight(compId, bScaleMarginChromaSrc));
    }
    else
    {
      const Pel* pSrc = getAddr(compId);
      Pel* pDest = pcPicYuvDst->getAddr(compId);
      const UInt strideDest = pcPicYuvDst->getStride(compId, bScaleMarginChromaDst);
      for (Int y = 0; y < height; y++, pSrc += strideSrc, pDest += strideDest)
      {
        ::memcpy(pDest, pSrc, width * sizeof(Pel));
      }
    }
  }
}

Void  TComPicYuv::copyTo(TComPicYuv* pcPicYuvDst, ComponentID compIdSrc, ComponentID compIdDst, const Bool bScaleMarginChromaSrc, const Bool bScaleMarginChromaDst) const
{
  const Int width = getWidth(compIdSrc); // source
  const Int height = getHeight(compIdSrc);
  const Int strideSrc = getStride(compIdSrc, bScaleMarginChromaSrc);
  assert(pcPicYuvDst->getWidth(compIdDst) == width);
  assert(pcPicYuvDst->getHeight(compIdDst) == height);
  if (strideSrc == pcPicYuvDst->getStride(compIdDst, bScaleMarginChromaDst))
  {
    ::memcpy(pcPicYuvDst->getBuf(compIdDst), getBuf(compIdSrc), sizeof(Pel) * strideSrc * getTotalHeight(compIdSrc,false));
  }
  else
  {
    const Pel* pSrc = getAddr(compIdSrc);
    Pel* pDest = pcPicYuvDst->getAddr(compIdDst);
    const UInt strideDest = pcPicYuvDst->getStride(compIdDst, bScaleMarginChromaDst);
    for (Int y = 0; y < height; y++, pSrc += strideSrc, pDest += strideDest)
    {
      ::memcpy(pDest, pSrc, width * sizeof(Pel));
    }
  }
}
#endif


Void TComPicYuv::extendPicBorder ()
{
  if ( m_bIsBorderExtended )
  {
    return;
  }

  for(Int comp=0; comp<getNumberValidComponents(); comp++)
  {
    const ComponentID compId=ComponentID(comp);
    Pel *piTxt=getAddr(compId); // piTxt = point to (0,0) of image within bigger picture.
    const Int stride=getStride(compId);
    const Int width=getWidth(compId);
    const Int height=getHeight(compId);
    const Int marginX=getMarginX(compId);
    const Int marginY=getMarginY(compId);

    Pel*  pi = piTxt;
    // do left and right margins
    for (Int y = 0; y < height; y++)
    {
      for (Int x = 0; x < marginX; x++ )
      {
        pi[ -marginX + x ] = pi[0];
        pi[    width + x ] = pi[width-1];
      }
      pi += stride;
    }

    // pi is now the (0,height) (bottom left of image within bigger picture
    pi -= (stride + marginX);
    // pi is now the (-marginX, height-1)
    for (Int y = 0; y < marginY; y++ )
    {
      ::memcpy( pi + (y+1)*stride, pi, sizeof(Pel)*(width + (marginX<<1)) );
    }

    // pi is still (-marginX, height-1)
    pi -= ((height-1) * stride);
    // pi is now (-marginX, 0)
    for (Int y = 0; y < marginY; y++ )
    {
      ::memcpy( pi - (y+1)*stride, pi, sizeof(Pel)*(width + (marginX<<1)) );
    }
  }

  m_bIsBorderExtended = true;
}

#if JVET_X0048_X0103_FILM_GRAIN
Void TComPicYuv::extendPicBorder(const ComponentID compId, const Int marginX, const Int marginY, const Bool bScaleMarginChroma)
{
  Pel* piTxt = getAddr(compId); // piTxt = point to (0,0) of image within bigger picture.
  const Int stride = getStride(compId, bScaleMarginChroma);
  const Int width = getWidth(compId);
  const Int height = getHeight(compId);

  Pel* pi = piTxt;
  // do left and right margins
  for (Int y = 0; y < height; y++)
  {
    for (Int x = 0; x < marginX; x++)
    {
      pi[-marginX + x] = pi[0];
      pi[width + x] = pi[width - 1];
    }
    pi += stride;
  }

  // pi is now the (0,height) (bottom left of image within bigger picture
  pi -= (stride + marginX);
  // pi is now the (-marginX, height-1)
  for (Int y = 0; y < marginY; y++)
  {
    ::memcpy(pi + (y + 1) * stride, pi, sizeof(Pel) * (width + (marginX << 1)));
  }

  // pi is still (-marginX, height-1)
  pi -= ((height - 1) * stride);
  // pi is now (-marginX, 0)
  for (Int y = 0; y < marginY; y++)
  {
    ::memcpy(pi - (y + 1) * stride, pi, sizeof(Pel) * (width + (marginX << 1)));
  }
}
#endif


// NOTE: This function is never called, but may be useful for developers.
#if JVET_X0048_X0103_FILM_GRAIN
Void TComPicYuv::dump(const std::string& fileName, const BitDepths& bitDepths, const Bool bAppend, const Bool bForceTo8Bit, const Bool bScaleMarginChroma) const
#else
Void TComPicYuv::dump (const std::string &fileName, const BitDepths &bitDepths, const Bool bAppend, const Bool bForceTo8Bit) const
#endif
{
  FILE *pFile = fopen (fileName.c_str(), bAppend?"ab":"wb");

  Bool is16bit=false;
  for(Int comp = 0; comp < getNumberValidComponents() && !bForceTo8Bit; comp++)
  {
    if (bitDepths.recon[toChannelType(ComponentID(comp))]>8)
    {
      is16bit=true;
    }
  }

  for(Int comp = 0; comp < getNumberValidComponents(); comp++)
  {
    const ComponentID  compId = ComponentID(comp);
    const Pel         *pi     = getAddr(compId);
#if JVET_X0048_X0103_FILM_GRAIN
    const Int          stride = getStride(compId, bScaleMarginChroma);
#else
    const Int          stride = getStride(compId);
#endif
    const Int          height = getHeight(compId);
    const Int          width  = getWidth(compId);

    if (is16bit)
    {
      for (Int y = 0; y < height; y++ )
      {
        for (Int x = 0; x < width; x++ )
        {
          UChar uc = (UChar)((pi[x]>>0) & 0xff);
          fwrite( &uc, sizeof(UChar), 1, pFile );
          uc = (UChar)((pi[x]>>8) & 0xff);
          fwrite( &uc, sizeof(UChar), 1, pFile );
        }
        pi += stride;
      }
    }
    else
    {
      const Int shift  = bitDepths.recon[toChannelType(compId)] - 8;
      const Int offset = (shift>0)?(1<<(shift-1)):0;
      for (Int y = 0; y < height; y++ )
      {
        for (Int x = 0; x < width; x++ )
        {
          UChar uc = (UChar)Clip3<Pel>(0, 255, (pi[x]+offset)>>shift);
          fwrite( &uc, sizeof(UChar), 1, pFile );
        }
        pi += stride;
      }
    }
  }

  fclose(pFile);
}

//! \}
