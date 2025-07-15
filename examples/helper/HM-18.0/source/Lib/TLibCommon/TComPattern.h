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

/** \file     TComPattern.h
    \brief    neighbouring pixel access classes (header)
*/

#ifndef __TCOMPATTERN__
#define __TCOMPATTERN__

// Include files
#include "CommonDef.h"

//! \ingroup TLibCommon
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

class TComDataCU;
class TComTU;

/// neighbouring pixel access class for all components
class TComPattern
{
private:
  Pel*  m_piROIOrigin;
  Int   m_roiWidth;
  Int   m_roiHeight;
  Int   m_patternStride;
  Int   m_bitDepth;

#if MCTS_ENC_CHECK  
  Int   m_roiPosX;
  Int   m_roiPosY;
  Int   m_tileLeftTopPelPosX;
  Int   m_tileLeftTopPelPosY;
  Int   m_tileRightBottomPelPosX;
  Int   m_tileRightBottomPelPosY;
#endif

public:
  // ROI & pattern information, (ROI = &pattern[AboveOffset][LeftOffset])
  Int   getROIYWidth() const      { return m_roiWidth;       }
  Int   getROIYHeight() const     { return m_roiHeight;      }
  Int   getPatternLStride() const { return m_patternStride;  }
  Int   getBitDepthY() const      { return m_bitDepth;       }
#if MCTS_ENC_CHECK
  Int   getROIYPosX() const       { return m_roiPosX; }
  Int   getROIYPosY() const       { return m_roiPosY; }

  Int   getTileLeftTopPelPosX() const { return m_tileLeftTopPelPosX; }
  Int   getTileLeftTopPelPosY() const { return m_tileLeftTopPelPosY; }
  Int   getTileRightBottomPelPosX() const { return m_tileRightBottomPelPosX; }
  Int   getTileRightBottomPelPosY() const { return m_tileRightBottomPelPosY; }
#endif

  __inline Pel*  getROIY()
  {
    return  m_piROIOrigin;
  }
  __inline const Pel*  getROIY() const
  {
    return  m_piROIOrigin;
  }

  TComPattern()
  : m_piROIOrigin(NULL)
  , m_roiWidth(0)
  , m_roiHeight(0)
  , m_patternStride(0)
  , m_bitDepth(0)
#if MCTS_ENC_CHECK
  , m_roiPosX(0)
  , m_roiPosY(0)
  , m_tileLeftTopPelPosX(0)
  , m_tileLeftTopPelPosY(0)
  , m_tileRightBottomPelPosX(0)
  , m_tileRightBottomPelPosY(0)
#endif
  {};


  // -------------------------------------------------------------------------------------------------------------------
  // initialization functions
  // -------------------------------------------------------------------------------------------------------------------

  /// set parameters from Pel buffers for accessing neighbouring pixels
#if MCTS_ENC_CHECK
  Void initPattern(Pel* piY, Int roiWidth, Int roiHeight, Int stride, Int bitDepthLuma, Int roiPosX, Int roiPosY);
  Void setTileBorders(Int tileLeftTopPelPosX, Int tileLeftTopPelPosY, Int tileRightBottomPelPosX, Int tileRightBottomPelPosY);
#else
  Void initPattern(Pel* piY, Int roiWidth, Int roiHeight, Int stride, Int bitDepthLuma);
#endif
};

//! \}

#endif // __TCOMPATTERN__
