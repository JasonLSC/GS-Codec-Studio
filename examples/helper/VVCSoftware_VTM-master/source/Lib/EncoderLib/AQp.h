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

/** \file     AQp.h
    \brief    class of picture which includes side information for encoder (header)
*/

#pragma once

#include "CommonLib/CommonDef.h"
#include "CommonLib/Picture.h"

// ====================================================================================================================
// Class definition
// ====================================================================================================================

// Local image characteristics for CUs on a specific depth
class AQpLayer
{
private:
  uint32_t m_partWidth;
  uint32_t m_partHeight;
  uint32_t m_widthInParts;
  uint32_t m_heightInParts;
  double   m_avgActivity;

  std::vector<double> m_activities;

public:
  AQpLayer(int width, int height, uint32_t partWidth, uint32_t partHeight);
  virtual ~AQpLayer();

  uint32_t getAQPartWidth() const { return m_partWidth; }
  uint32_t getAQPartHeight() const { return m_partHeight; }
  uint32_t getNumAQPartInWidth() const { return m_widthInParts; }
  uint32_t getNumAQPartInHeight() const { return m_heightInParts; }
  uint32_t getAQPartStride() const { return m_widthInParts; }

  std::vector<double> &getQPAdaptationUnit() { return m_activities; }

  double getActivity(const Position &pos) const
  {
    const uint32_t x = pos.x / m_partWidth;
    const uint32_t y = pos.y / m_partHeight;
    return m_activities[y * m_widthInParts + x];
  }

  double getAvgActivity() const { return m_avgActivity; }

  void setAvgActivity(double d) { m_avgActivity = d; }
};

// Source picture analyzer class
class AQpPreanalyzer
{
protected:
  AQpPreanalyzer() {}
  virtual ~AQpPreanalyzer() {}
public:
  static void preanalyze(Picture *pic);
};
