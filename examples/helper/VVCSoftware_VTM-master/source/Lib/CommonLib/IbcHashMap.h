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

/** \file     IbcHashMap.h
    \brief    IBC hash map encoder class (header)
*/

#ifndef __IBCHASHMAP__
#define __IBCHASHMAP__

// Include files
#include "CommonLib/CommonDef.h"
#include "CommonLib/IntraPrediction.h"
#include "CommonLib/InterPrediction.h"
#include "CommonLib/TrQuant.h"
#include "CommonLib/Unit.h"
#include "CommonLib/UnitPartitioner.h"

#include <unordered_map>
#include <vector>
//! \ingroup EncoderLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

class IbcHashMap
{
private:
  using HashValue = uint32_t;

  int     m_picWidth;
  int     m_picHeight;

  std::vector<HashValue> m_pos2Hash;
  ptrdiff_t              m_pos2HashStride;

  // sorted according to hash value
  std::vector<std::pair<HashValue, Position>> m_hashPos;

  auto getPositions(HashValue hash) const
  {
    return std::equal_range(m_hashPos.begin(), m_hashPos.end(), std::make_pair(hash, Position()),
                            [](const auto& a, const auto& b) { return a.first < b.first; });
  }

  size_t getNumPositions(HashValue hash)
  {
    auto [first, last] = getPositions(hash);
    return std::distance(first, last);
  }

  unsigned int xxCalcBlockHash(const Pel *pel, const ptrdiff_t stride, const int width, const int height,
                               unsigned int crc);

  template<ChromaFormat chromaFormat>
  void    xxBuildPicHashMap(const PelUnitBuf& pic);

  static uint32_t xxComputeCrc32c16bit(uint32_t crc, const Pel* pels, size_t n);

public:
  uint32_t (*m_computeCrc32c)(uint32_t crc, const Pel* pels, size_t n);

  IbcHashMap();
  virtual ~IbcHashMap();

  void    init(const int picWidth, const int picHeight);
  void    destroy();
  void    rebuildPicHashMap(const PelUnitBuf& pic);
  bool    ibcHashMatch(const Area& lumaArea, std::vector<Position>& cand, const CodingStructure& cs, const int maxCand, const int searchRange4SmallBlk);
  int     getHashHitRatio(const Area& lumaArea);

  int     calHashBlkMatchPerc(const Area& lumaArea);

#ifdef TARGET_SIMD_X86
  void    initIbcHashMapX86();
  template <X86_VEXT vext>
  void    _initIbcHashMapX86();
#endif

};

//! \}

#endif // __IBCHASHMAP__
