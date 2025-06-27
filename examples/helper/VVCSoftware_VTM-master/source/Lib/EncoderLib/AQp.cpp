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

/** \file     AQp.cpp
    \brief    class of picture which includes side information for encoder
*/

#include "AQp.h"

// Constructor
AQpLayer::AQpLayer(int width, int height, uint32_t partWidth, uint32_t partHeight)
  : m_partWidth(partWidth)
  , m_partHeight(partHeight)
  , m_widthInParts((width + partWidth - 1) / partWidth)
  , m_heightInParts((height + partHeight - 1) / partHeight)
  , m_avgActivity(0.0)
{
  m_activities.reserve(m_widthInParts * m_heightInParts);   // allocate memory
}

// Destructor
AQpLayer::~AQpLayer()
{
}

// Analyze source picture and compute local image characteristics used for QP adaptation
void AQpPreanalyzer::preanalyze(Picture *pic)
{
  const CPelBuf lumaPlane = pic->getOrigBuf().Y();

  const int       width  = lumaPlane.width;
  const int       height = lumaPlane.height;
  const ptrdiff_t stride = lumaPlane.stride;

  for (auto aqLayer: pic->aqlayer)
  {
    const uint32_t partWidth  = aqLayer->getAQPartWidth();
    const uint32_t partHeight = aqLayer->getAQPartHeight();

    std::vector<double> &activities = aqLayer->getQPAdaptationUnit();

    double activitySum = 0.0;
    for (uint32_t y = 0; y < height; y += partHeight)
    {
      const uint32_t curPartHeight = std::min(partHeight, height - y);
      CHECK((curPartHeight & 1) != 0, "Odd part height unsupported");

      for (uint32_t x = 0; x < width; x += partWidth)
      {
        const uint32_t curPartWidth = std::min(partWidth, width - x);
        CHECK((curPartWidth & 1) != 0, "Odd part width unsupported");

        std::array<uint64_t, 4> sum;
        std::array<uint64_t, 4> sumSq;

        sum.fill(0);
        sumSq.fill(0);

        const uint32_t quadrantWidth  = curPartWidth >> 1;
        const uint32_t quadrantHeight = curPartHeight >> 1;

        const Pel *pBlkY0 = lumaPlane.bufAt(x, y);
        const Pel *pBlkY1 = pBlkY0 + quadrantWidth;
        const Pel *pBlkY2 = pBlkY0 + quadrantHeight * stride;
        const Pel *pBlkY3 = pBlkY2 + quadrantWidth;

        for (ptrdiff_t by = 0; by < quadrantHeight; by++)
        {
          for (ptrdiff_t bx = 0; bx < quadrantWidth; bx++)
          {
            const ptrdiff_t k = bx + by * stride;

            sum[0] += pBlkY0[k];
            sumSq[0] += pBlkY0[k] * pBlkY0[k];

            sum[1] += pBlkY1[k];
            sumSq[1] += pBlkY1[k] * pBlkY1[k];

            sum[2] += pBlkY2[k];
            sumSq[2] += pBlkY2[k] * pBlkY2[k];

            sum[3] += pBlkY3[k];
            sumSq[3] += pBlkY3[k] * pBlkY3[k];
          }
        }

        const uint32_t quadrantSize = quadrantWidth * quadrantHeight;

        double minVariance = MAX_DOUBLE;
        if (quadrantSize != 0)
        {
          for (int i = 0; i < sum.size(); i++)
          {
            const double average  = double(sum[i]) / quadrantSize;
            const double variance = double(sumSq[i]) / quadrantSize - average * average;

            minVariance = std::min(minVariance, variance);
          }
        }
        else
        {
          minVariance = 0.0;
        }

        // activity is 1 + the lowest variance amongst the 4 quadrants in the part
        const double activity = 1.0 + minVariance;

        activities.push_back(activity);
        activitySum += activity;
      }
    }

    const double activityAvg = activitySum / (aqLayer->getNumAQPartInWidth() * aqLayer->getNumAQPartInHeight());

    aqLayer->setAvgActivity(activityAvg);
  }
}
