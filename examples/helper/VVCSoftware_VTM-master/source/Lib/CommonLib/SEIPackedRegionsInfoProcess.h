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

/** \file     SEIPackedRegionsInfo.h
    \brief    Packed regions info SEI processing
*/

#ifndef __SEIPACKEDREGIONSINFOPROCESS__
#define __SEIPACKEDREGIONSINFOPROCESS__

#include "SEI.h"
#include "Unit.h"
#include "Buffer.h"
#include "Unit.h"
struct Picture;

//! \ingroup CommonLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

class SEIPackedRegionsInfoProcess
{
public:
  SEIPackedRegionsInfoProcess()
    : m_layerId(0)
    , m_enabled(false)
    , m_persistence(false)
    , m_picWidth(0)
    , m_picHeight(0)
    , m_maxPicWidth(0)
    , m_maxPicHeight(0)
    , m_targetPicWidth(0)
    , m_targetPicHeight(0)
    , m_bitDepthY(10)
    , m_bitDepthC(10)
    , m_priUnitSize(1)
    , m_chromaFormat(ChromaFormat::_420)
    , m_subWidthC(2)
    , m_subHeightC(2)
    , m_priNumRegions(0)
    , m_multilayerFlag(false)
#if JVET_AL0324_AL0070_PRI_SEI
    , m_priUseMaxDimensionsFlag(false)
#endif
  {}
  ~SEIPackedRegionsInfoProcess() {}
  void init(SEIPackedRegionsInfo& sei, const SPS& sps, uint32_t picWidth, uint32_t picHeight);
  void packRegions(PelUnitBuf& src, int layerId, PelUnitBuf& dst, const SPS& sps);
  void reconstruct(PicList* pcListPic, Picture* currentPic, PelUnitBuf& dst, const SPS& sps);

  int                   m_layerId;
  bool                  m_enabled;
  bool                  m_persistence;
  uint32_t              m_picWidth;
  uint32_t              m_picHeight;
  uint32_t              m_maxPicWidth;
  uint32_t              m_maxPicHeight;
  uint32_t              m_targetPicWidth;
  uint32_t              m_targetPicHeight;
  uint32_t              m_bitDepthY;
  uint32_t              m_bitDepthC;
  uint32_t              m_priUnitSize;
  ChromaFormat          m_chromaFormat;
  uint32_t              m_subWidthC;
  uint32_t              m_subHeightC;
  uint32_t              m_priNumRegions;
  bool                  m_multilayerFlag;
#if JVET_AL0324_AL0070_PRI_SEI
  bool                  m_priUseMaxDimensionsFlag;
#endif
  std::vector<uint32_t> m_priRegionTopLeftX;
  std::vector<uint32_t> m_priRegionTopLeftY;
  std::vector<uint32_t> m_priRegionWidth;
  std::vector<uint32_t> m_priRegionHeight;
  std::vector<uint32_t> m_priResampleWidthNum;
  std::vector<uint32_t> m_priResampleWidthDenom;
  std::vector<uint32_t> m_priResampleHeightNum;
  std::vector<uint32_t> m_priResampleHeightDenom;
  std::vector<uint32_t> m_priTargetRegionTopLeftX;
  std::vector<uint32_t> m_priTargetRegionTopLeftY;
  std::vector<uint32_t> m_priTargetRegionWidth;
  std::vector<uint32_t> m_priTargetRegionHeight;
  std::vector<uint32_t> m_priRegionId;
  std::vector<uint32_t> m_regionLayerId;
  std::vector<uint8_t>  m_regionIsALayerFlag;
};

//! \}

#endif // __SEIPACKEDREGIONSINFO__
