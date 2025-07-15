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

/** \file     TComPicYuv.h
    \brief    picture YUV buffer class (header)
*/

#ifndef __TCOMPICYUV__
#define __TCOMPICYUV__

#include <stdio.h>
#include "CommonDef.h"
#include "TComRom.h"
#include "TComChromaFormat.h"
#include "SEI.h"

//! \ingroup TLibCommon
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// picture YUV buffer class
class TComPicYuv
{
private:

  // ------------------------------------------------------------------------------------------------
  //  YUV buffer
  // ------------------------------------------------------------------------------------------------

  Pel*  m_apiPicBuf[MAX_NUM_COMPONENT];             ///< Buffer (including margin)

  Pel*  m_piPicOrg[MAX_NUM_COMPONENT];              ///< m_apiPicBufY + m_iMarginLuma*getStride() + m_iMarginLuma

  // ------------------------------------------------------------------------------------------------
  //  Parameter for general YUV buffer usage
  // ------------------------------------------------------------------------------------------------

  Int   m_picWidth;                                 ///< Width of picture in pixels
  Int   m_picHeight;                                ///< Height of picture in pixels
  ChromaFormat m_chromaFormatIDC;                   ///< Chroma Format

  Int*  m_ctuOffsetInBuffer[MAX_NUM_CHANNEL_TYPE];  ///< Gives an offset in the buffer for a given CTU (and channel)
  Int*  m_subCuOffsetInBuffer[MAX_NUM_CHANNEL_TYPE];///< Gives an offset in the buffer for a given sub-CU (and channel), relative to start of CTU

  Int   m_marginX;                                  ///< margin of Luma channel (chroma's may be smaller, depending on ratio)
  Int   m_marginY;                                  ///< margin of Luma channel (chroma's may be smaller, depending on ratio)

  Bool  m_bIsBorderExtended;

public:
               TComPicYuv         ();
  virtual     ~TComPicYuv         ();

  // ------------------------------------------------------------------------------------------------
  //  Memory management
  // ------------------------------------------------------------------------------------------------

  Void          create            (const Int picWidth,
                                   const Int picHeight,
                                   const ChromaFormat chromaFormatIDC,
                                   const UInt maxCUWidth,  ///< used for generating offsets to CUs.
                                   const UInt maxCUHeight, ///< used for generating offsets to CUs.
                                   const UInt maxCUDepth,  ///< used for generating offsets to CUs.
                                   const Bool bUseMargin);   ///< if true, then a margin of uiMaxCUWidth+16 and uiMaxCUHeight+16 is created around the image.

  Void          createWithoutCUInfo(const Int picWidth,
                                    const Int picHeight,
                                    const ChromaFormat chromaFormatIDC,
                                    const Bool bUseMargin=false, ///< if true, then a margin of uiMaxCUWidth+16 and uiMaxCUHeight+16 is created around the image.
                                    const UInt maxCUWidth=0,   ///< used for margin only
                                    const UInt maxCUHeight=0); ///< used for margin only

#if JVET_X0048_X0103_FILM_GRAIN
  Void          createWithPadding  (const Int picWidth,
                                    const Int picHeight,
                                    const ChromaFormat chromaFormatIDC,
                                    const Bool bUseMargin=false, ///< if true, then a margin of padWidth and padHeight is created around the image.
                                    const Bool bScaleMarginChroma=true, ///< if true, margin is scaled based on chroma format. Otherwise, the same padding is used for all components.
                                    const UInt padWidth=0,   ///< used for margin only
                                    const UInt padHeight=0); ///< used for margin only
#endif

  Void          destroy           ();

  // The following have been removed - Use CHROMA_400 in the above function call.
  //Void  createLuma  ( Int iPicWidth, Int iPicHeight, UInt uiMaxCUWidth, UInt uiMaxCUHeight, UInt uhMaxCUDepth );
  //Void  destroyLuma ();

  // ------------------------------------------------------------------------------------------------
  //  Get information of picture
  // ------------------------------------------------------------------------------------------------

  Int           getWidth          (const ComponentID id) const { return  m_picWidth >> getComponentScaleX(id);   }
  Int           getHeight         (const ComponentID id) const { return  m_picHeight >> getComponentScaleY(id);  }
  ChromaFormat  getChromaFormat   ()                     const { return m_chromaFormatIDC; }
  UInt          getNumberValidComponents() const { return ::getNumberValidComponents(m_chromaFormatIDC); }

  Int           getStride         (const ComponentID id) const { return ((m_picWidth     ) + (m_marginX  <<1)) >> getComponentScaleX(id); }
private:
  Int           getStride         (const ChannelType id) const { return ((m_picWidth     ) + (m_marginX  <<1)) >> getChannelTypeScaleX(id); }
public:
  Int           getTotalHeight    (const ComponentID id) const { return ((m_picHeight    ) + (m_marginY  <<1)) >> getComponentScaleY(id); }

#if JVET_X0048_X0103_FILM_GRAIN
  Int           getStride(const ComponentID id, const Bool bScaleMarginChroma) const {
    unsigned xmargin = m_marginX >> (bScaleMarginChroma ? getComponentScaleX(id) : 0);
    return ((m_picWidth >> getComponentScaleX(id)) + (2 * xmargin));
  }

  Int           getTotalHeight(const ComponentID id, const Bool bScaleMarginChroma) const {
    unsigned ymargin = m_marginY >> (bScaleMarginChroma ? getComponentScaleY(id) : 0);
    return ((m_picHeight >> getComponentScaleY(id)) + (2 * ymargin));
  }

  Pel& at(const int& x, const int& y, const ComponentID id, const Bool bScaleMarginChroma = true) { int stride = getStride(id, bScaleMarginChroma); return m_piPicOrg[id][y * stride + x]; }
  const Pel& at(const int& x, const int& y, const ComponentID id, const Bool bScaleMarginChroma = true) const { int stride = getStride(id, bScaleMarginChroma); return m_piPicOrg[id][y * stride + x]; }

#endif

  Int           getMarginX        (const ComponentID id) const { return m_marginX >> getComponentScaleX(id);  }
  Int           getMarginY        (const ComponentID id) const { return m_marginY >> getComponentScaleY(id);  }

  // ------------------------------------------------------------------------------------------------
  //  Access function for picture buffer
  // ------------------------------------------------------------------------------------------------

  //  Access starting position of picture buffer with margin
  Pel*          getBuf            (const ComponentID ch)       { return  m_apiPicBuf[ch];   }
  const Pel*    getBuf            (const ComponentID ch) const { return  m_apiPicBuf[ch];   }

  //  Access starting position of original picture
  Pel*          getAddr           (const ComponentID ch)       { return  m_piPicOrg[ch];   }
  const Pel*    getAddr           (const ComponentID ch) const { return  m_piPicOrg[ch];   }

  //  Access starting position of original picture for specific coding unit (CU) or partition unit (PU)
  Pel*          getAddr           (const ComponentID ch, const Int ctuRSAddr )       { return m_piPicOrg[ch] + m_ctuOffsetInBuffer[ch==0?0:1][ ctuRSAddr ]; }
  const Pel*    getAddr           (const ComponentID ch, const Int ctuRSAddr ) const { return m_piPicOrg[ch] + m_ctuOffsetInBuffer[ch==0?0:1][ ctuRSAddr ]; }
  Pel*          getAddr           (const ComponentID ch, const Int ctuRSAddr, const Int uiAbsZorderIdx )
                                     { return m_piPicOrg[ch] + m_ctuOffsetInBuffer[ch==0?0:1][ctuRSAddr] + m_subCuOffsetInBuffer[ch==0?0:1][g_auiZscanToRaster[uiAbsZorderIdx]]; }
  const Pel*    getAddr           (const ComponentID ch, const Int ctuRSAddr, const Int uiAbsZorderIdx ) const
                                     { return m_piPicOrg[ch] + m_ctuOffsetInBuffer[ch==0?0:1][ctuRSAddr] + m_subCuOffsetInBuffer[ch==0?0:1][g_auiZscanToRaster[uiAbsZorderIdx]]; }

  UInt          getComponentScaleX(const ComponentID id) const { return ::getComponentScaleX(id, m_chromaFormatIDC); }
  UInt          getComponentScaleY(const ComponentID id) const { return ::getComponentScaleY(id, m_chromaFormatIDC); }

  UInt          getChannelTypeScaleX(const ChannelType id) const { return ::getChannelTypeScaleX(id, m_chromaFormatIDC); }
  UInt          getChannelTypeScaleY(const ChannelType id) const { return ::getChannelTypeScaleY(id, m_chromaFormatIDC); }

  // ------------------------------------------------------------------------------------------------
  //  Miscellaneous
  // ------------------------------------------------------------------------------------------------

  //  Copy function to picture
#if JVET_X0048_X0103_FILM_GRAIN
  Void  copyTo(TComPicYuv* pcPicYuvDst, ComponentID compIdSrc, ComponentID compIdDst, const Bool bScaleMarginChromaSrc, const Bool bScaleMarginChromaDst) const;
  Void  extendPicBorder(const ComponentID compId, const Int marginX, const Int marginY, const Bool bScaleMarginChroma = true);
  Void  copyToPic(TComPicYuv* pcPicYuvDst, const Bool bScaleMarginChromaSrc, const Bool bScaleMarginChromaDst) const;
#endif
  Void          copyToPic(TComPicYuv* pcPicYuvDst) const;

  //  Extend function of picture buffer
  Void          extendPicBorder   ();

  //  Dump picture
#if JVET_X0048_X0103_FILM_GRAIN
  Void          dump(const std::string& fileName, const BitDepths& bitDepths, const Bool bAppend = false, const Bool bForceTo8Bit = false, const Bool bScaleMarginChroma = true) const;
#else
  Void          dump(const std::string& fileName, const BitDepths& bitDepths, const Bool bAppend = false, const Bool bForceTo8Bit = false) const;
#endif
  
  // Set border extension flag
  Void          setBorderExtension(Bool b) { m_bIsBorderExtended = b; }
};// END CLASS DEFINITION TComPicYuv


// These functions now return the length of the digest strings.
UInt calcChecksum(const TComPicYuv& pic, TComPictureHash &digest, const BitDepths &bitDepths);
UInt calcCRC     (const TComPicYuv& pic, TComPictureHash &digest, const BitDepths &bitDepths);
UInt calcMD5     (const TComPicYuv& pic, TComPictureHash &digest, const BitDepths &bitDepths);
std::string hashToString(const TComPictureHash &digest, Int numChar);
//! \}

#endif // __TCOMPICYUV__
