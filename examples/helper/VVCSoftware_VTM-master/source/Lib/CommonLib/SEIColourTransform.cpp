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

 /** \file     SEIColourTransform.cpp
     \brief    Colour transform SEI
 */

#include "SEIColourTransform.h"

#include "SEI.h"
#include "Unit.h"
#include "Buffer.h"

SEIColourTransformApply::SEIColourTransformApply()
  : m_width(0), m_height(0), m_chromaFormat(ChromaFormat::UNDEFINED), m_bitDepth(0), m_pColourTransfParams(nullptr)
{
}

void SEIColourTransformApply::create(uint32_t width, uint32_t height, ChromaFormat fmt, uint8_t bitDepth)
{
  m_width               = width;
  m_height              = height;
  m_chromaFormat        = fmt;
  m_bitDepth            = bitDepth;
  m_pColourTransfParams = new SEIColourTransformInfo;
  m_lutSize             = 1 << m_bitDepth;
  for (int i = 0; i < MAX_NUM_COMPONENT; i++)
  {
    m_mapLut[i].resize(m_lutSize, 0);
  }
}

SEIColourTransformApply::~SEIColourTransformApply()
{
  delete m_pColourTransfParams;
}

void SEIColourTransformApply::inverseColourTransform(PelStorage* transformBuf)
{
  const int numComp = getNumberValidComponents(m_chromaFormat);
  PelBuf*   buffY   = &transformBuf->Y();
  PelBuf*   buffCb  = &transformBuf->Cb();
  PelBuf*   buffCr  = &transformBuf->Cr();

  if (numComp == 3)
  {
    if (m_pColourTransfParams->m_crossComponentFlag)
    {
      buffCb->applyChromaCTI(buffY->buf, buffY->stride, m_mapLut[COMPONENT_Cb], m_bitDepth, m_chromaFormat, false);
      buffCr->applyChromaCTI(buffY->buf, buffY->stride, m_mapLut[COMPONENT_Cr], m_bitDepth, m_chromaFormat, false);
    }
    else
    {
      buffCb->applyLumaCTI(m_mapLut[COMPONENT_Cb]); // apply direct mapping like in luma (no cross component mapping); same function, but different lut.
      buffCr->applyLumaCTI(m_mapLut[COMPONENT_Cr]);
    }
  }
  buffY->applyLumaCTI(m_mapLut[COMPONENT_Y]);
}

void SEIColourTransformApply::generateColourTransfLUTs()
{
  const int numComp         = getNumberValidComponents(m_chromaFormat);
  int numPreLutPoints = 1 << m_pColourTransfParams->m_log2NumberOfPointsPerLut;
  int dynamicRange    = 1 << m_bitDepth;
  const int orgCW     = dynamicRange / numPreLutPoints;
  int scalingPreLut   = 1 << ( 11 - (int)floorLog2(orgCW) ); // scale-up values from cfg file (chroma preLut is scaled down in cfg)

  std::vector<Pel> pivotInPoints;
  std::vector<Pel> pivotMappedPointsY(numPreLutPoints+1);
  std::vector<Pel> pivotMappedPointsX(numPreLutPoints+1);

  // Create Inverse Luma LUT - same for all possible combinations of ctiCrossComp and ctiChromaLutInferred

  std::vector<int> invScale(numPreLutPoints);

  pivotInPoints = m_pColourTransfParams->m_lut[0].lutValues;
  pivotMappedPointsX[0] = pivotInPoints[0];
  pivotMappedPointsY[0] = 0;
  for (int j = 1; j < numPreLutPoints; j++)
  {
    pivotMappedPointsX[j] = pivotMappedPointsX[j - 1] + pivotInPoints[j];
    pivotMappedPointsY[j] = j * orgCW;
  }

  for (int i = 0; i < numPreLutPoints; i++)
  {
    invScale[i] = ((int32_t)m_pColourTransfParams->m_lut[0].lutValues[i + 1] * (1 << FP_PREC) + (1 << (floorLog2(orgCW) - 1))) >> floorLog2(orgCW);
  }

  for (int i = 0; i < dynamicRange; i++)
  {
    int idx = i / orgCW;
    int tempVal = pivotMappedPointsX[idx] + ((invScale[idx] * (i - pivotMappedPointsY[idx]) + (1 << (FP_PREC - 1))) >> FP_PREC);
    m_mapLut[0][i] = Clip3((Pel)0, (Pel)(dynamicRange - 1), (Pel)(tempVal));
  }

  //  calculate chroma LUTs
  if (m_pColourTransfParams->m_crossComponentInferred == 0)
  {
    for (int i = 1; i < numComp; i++)
    { // loop for U and V
      if (m_pColourTransfParams->m_crossComponentFlag == 1)
      {
        // cross-component U and V LUT
        for (int j = 0; j < dynamicRange; j++)
        {
          int     idx     = j / orgCW;
          int  slope = scalingPreLut * (m_pColourTransfParams->m_lut[i].lutValues[idx + 1] - m_pColourTransfParams->m_lut[i].lutValues[idx]);
          m_mapLut[i][j] = scalingPreLut * m_pColourTransfParams->m_lut[i].lutValues[idx] + slope * (j - pivotMappedPointsY[idx]) / orgCW;
        }
      }
      else
      {
        // intra-component Chroma (U and V) LUT
        // initialize pivot points
        pivotInPoints = m_pColourTransfParams->m_lut[i].lutValues;
        pivotMappedPointsX[0] = pivotInPoints[0];
        for (int j = 1; j <= numPreLutPoints; j++)
        {
          pivotMappedPointsX[j] = pivotMappedPointsX[j-1] + pivotInPoints[j];
        }

        for (int i = 0; i < numPreLutPoints; i++)
        {
          invScale[i] = ((int32_t)m_pColourTransfParams->m_lut[0].lutValues[i + 1] * (1 << FP_PREC) + (1 << (floorLog2(orgCW) - 1))) >> floorLog2(orgCW);
        }

        for (int j = 0; j < dynamicRange; j++)
        {
          int idx = j / orgCW;
          int tempVal = pivotMappedPointsX[idx] + ((invScale[idx] * (j - pivotMappedPointsY[idx]) + (1 << (FP_PREC - 1))) >> FP_PREC);
          m_mapLut[i][j] = Clip3((Pel)0, (Pel)(dynamicRange - 1), (Pel)(tempVal));
        }
      }
    }
  }
  else
  {
    int chrOffset = m_pColourTransfParams->m_chromaOffset;

    std::vector<int> chromaAdjHelpLUT (numPreLutPoints);
    for (int i = 0; i < numPreLutPoints; i++)
    {
      chromaAdjHelpLUT[i] = (m_pColourTransfParams->m_lut[0].lutValues[i + 1] == 0) ? (1 << CSCALE_FP_PREC) : ((int32_t)((m_pColourTransfParams->m_lut[0].lutValues[i + 1] + chrOffset) * (1 << FP_PREC) / orgCW));
    }

    // generate smoothed chroma LUT as done by JVET-U0078
    std::vector<int> interpLut(numPreLutPoints + 1);
    for (int i = 1; i < numPreLutPoints; i++)
    {
      interpLut[i] = (chromaAdjHelpLUT[i] + chromaAdjHelpLUT[i - 1] + 1) / 2;
    }
    interpLut[0]                = chromaAdjHelpLUT[0];
    interpLut[numPreLutPoints]  = chromaAdjHelpLUT[numPreLutPoints - 1];

    for (int i = 0; i < dynamicRange; i++)
    {
      int idx = i / orgCW;
      int slope = interpLut[idx + 1] - interpLut[idx];
      m_mapLut[1][i] = interpLut[idx] + slope * (i - pivotMappedPointsY[idx]) / orgCW;
      m_mapLut[2][i] = m_mapLut[1][i];
    }
  }
}
