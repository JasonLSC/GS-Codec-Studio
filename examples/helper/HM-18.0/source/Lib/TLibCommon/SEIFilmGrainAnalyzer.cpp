/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2021, ITU/ISO/IEC
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

/** \file     SEIFilmGrainAnalyzer.cpp
    \brief    SMPTE RDD5 based film grain analysis functionality from SEI messages
*/

#include "SEIFilmGrainAnalyzer.h"

#if JVET_X0048_X0103_FILM_GRAIN

// ====================================================================================================================
// Edge detection - Canny
// ====================================================================================================================
const int Canny::m_gx[3][3]{ { -1, 0, 1 }, { -2, 0, 2 }, { -1, 0, 1 } };
const int Canny::m_gy[3][3]{ { -1, -2, -1 }, { 0, 0, 0 }, { 1, 2, 1 } };

const int Canny::m_gauss5x5[5][5]{ { 2, 4, 5, 4, 2 },
                                 { 4, 9, 12, 9, 4 },
                                 { 5, 12, 15, 12, 5 },
                                 { 4, 9, 12, 9, 4 },
                                 { 2, 4, 5, 4, 2 } };

Canny::Canny()
{
  // init();
}

Canny::~Canny()
{
  // uninit();
}

void Canny::gradient(TComPicYuv* buff1, TComPicYuv* buff2, unsigned int width, unsigned int height,
                     unsigned int convWidthS, unsigned int convHeightS, unsigned int bitDepth, ComponentID compID)
{
  /*
  buff1 - magnitude; buff2 - orientation (Only luma in buff2)
  */

  // 360 degrees are split into the 8 equal parts; edge direction is quantized 
  const double edge_threshold_22_5  = 22.5;
  const double edge_threshold_67_5  = 67.5;
  const double edge_threshold_112_5 = 112.5;
  const double edge_threshold_157_5 = 157.5;

  const int maxClpRange = (1 << bitDepth) - 1;
  const int padding     = convWidthS / 2;

  // tmp buffs
  TComPicYuv* tmpBuf1 = new TComPicYuv;
  TComPicYuv* tmpBuf2 = new TComPicYuv;
  tmpBuf1->createWithPadding(width, height, CHROMA_400);
  tmpBuf2->createWithPadding(width, height, CHROMA_400);

  buff1->extendPicBorder(compID, padding, padding, false);

  // Gx
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      int acc = 0;
      for (int x = 0; x < convWidthS; x++)
      {
        for (int y = 0; y < convHeightS; y++)
        {
          acc += (buff1->at(x - convWidthS / 2 + i, y - convHeightS / 2 + j, compID, false) * m_gx[x][y]);
        }
      }
      tmpBuf1->at(i, j, ComponentID(0), false) = acc;
    }
  }

  // Gy
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      int acc = 0;
      for (int x = 0; x < convWidthS; x++)
      {
        for (int y = 0; y < convHeightS; y++)
        {
          acc += (buff1->at(x - convWidthS / 2 + i, y - convHeightS / 2 + j, compID, false) * m_gy[x][y]);
        }
      }
      tmpBuf2->at(i, j, ComponentID(0), false) = acc;
    }
  }

  // magnitude
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      Pel tmp                     = (Pel)((abs(tmpBuf1->at(i, j, ComponentID(0), false)) + abs(tmpBuf2->at(i, j, ComponentID(0), false))) / 2);
      buff1->at(i, j, compID, false) = (Pel) Clip3((Pel) 0, (Pel) maxClpRange, tmp);
    }
  }

  // edge direction - quantized edge directions
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      double theta = (atan2(tmpBuf1->at(i, j, ComponentID(0), false), tmpBuf2->at(i, j, ComponentID(0), false)) * 180) / PI;

      /* Convert actual edge direction to approximate value - quantize directions */
      if (((-edge_threshold_22_5 < theta) && (theta <= edge_threshold_22_5)) || ((edge_threshold_157_5 < theta) || (theta <= -edge_threshold_157_5)))
        buff2->at(i, j, ComponentID(0), false) = 0;
      if (((-edge_threshold_157_5 < theta) && (theta <= -edge_threshold_112_5)) || ((edge_threshold_22_5 < theta) && (theta <= edge_threshold_67_5)))
        buff2->at(i, j, ComponentID(0), false) = 45;
      if (((-edge_threshold_112_5 < theta) && (theta <= -edge_threshold_67_5)) || ((edge_threshold_67_5 < theta) && (theta <= edge_threshold_112_5)))
        buff2->at(i, j, ComponentID(0), false) = 90;
      if (((-edge_threshold_67_5 < theta) && (theta <= -edge_threshold_22_5)) || ((edge_threshold_112_5 < theta) && (theta <= edge_threshold_157_5)))
        buff2->at(i, j, ComponentID(0), false) = 135;
    }
  }

  buff1->extendPicBorder(compID, padding, padding, false);   // extend border for the next steps
  tmpBuf1->destroy();
  tmpBuf2->destroy();
  delete tmpBuf1;
  tmpBuf1 = nullptr;
  delete tmpBuf2;
  tmpBuf2 = nullptr;
}

void Canny::suppressNonMax(TComPicYuv* buff1, TComPicYuv* buff2, unsigned int width, unsigned int height,
                           ComponentID compID)
{
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      int rowShift = 0, colShift = 0;

      switch (buff2->at(i, j, ComponentID(0), false))
      {
      case 0:
        rowShift = 1;
        colShift = 0;
        break;
      case 45:
        rowShift = 1;
        colShift = 1;
        break;
      case 90:
        rowShift = 0;
        colShift = 1;
        break;
      case 135:
        rowShift = -1;
        colShift = 1;
        break;
      default: throw("Unsupported gradient direction."); break;
      }

      Pel pelCurrent             = buff1->at(i, j, compID, false);
      Pel pelEdgeDirectionTop    = buff1->at(i + rowShift, j + colShift, compID, false);
      Pel pelEdgeDirectionBottom = buff1->at(i - rowShift, j - colShift, compID, false);
      if ((pelCurrent < pelEdgeDirectionTop) || (pelCurrent < pelEdgeDirectionBottom))
      {
        buff2->at(i, j, ComponentID(0), false) = 0;   // supress
      }
      else
      {
        buff2->at(i, j, ComponentID(0), false) = buff1->at(i, j, compID, false);   // keep
      }
    }
  }
  buff2->copyTo(buff1, ComponentID(0), compID, false, false);
}

void Canny::doubleThreshold(TComPicYuv* buff, unsigned int width, unsigned int height,
                            /*unsigned int windowSizeRatio,*/ unsigned int bitDepth, ComponentID compID)
{
  Pel strongPel = ((Pel) 1 << bitDepth) - 1;
  Pel weekPel   = ((Pel) 1 << (bitDepth - 1)) - 1;

  Pel highThreshold = 0;
  Pel lowThreshold  = strongPel;
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      highThreshold = std::max<Pel>(highThreshold, buff->at(i, j, compID, false));
    }
  }

  // global low and high threshold
  lowThreshold = (Pel)(m_lowThresholdRatio * highThreshold);
  highThreshold =
    Clip3(0, (1 << bitDepth) - 1,
          m_highThresholdRatio * lowThreshold);   // Canny recommended a upper:lower ratio between 2:1 and 3:1.

  // strong, week, supressed
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      if (buff->at(i, j, compID, false) > highThreshold)
        buff->at(i, j, compID, false) = strongPel;
      else if (buff->at(i, j, compID, false) <= highThreshold && buff->at(i, j, compID, false) > lowThreshold)
        buff->at(i, j, compID, false) = weekPel;
      else
        buff->at(i, j, compID, false) = 0;
    }
  }

  buff->extendPicBorder(compID, 1, 1, false); // extend one pixel on each side for the next step
}

void Canny::edgeTracking(TComPicYuv* buff, unsigned int width, unsigned int height, unsigned int windowWidth,
                         unsigned int windowHeight, unsigned int bitDepth, ComponentID compID)
{
  Pel strongPel = ((Pel) 1 << bitDepth) - 1;
  Pel weekPel   = ((Pel) 1 << (bitDepth - 1)) - 1;

  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      if (buff->at(i, j, compID, false) == weekPel)
      {
        bool strong = false;

        for (int x = 0; x < windowWidth; x++)
        {
          for (int y = 0; y < windowHeight; y++)
          {
            if (buff->at(x - windowWidth / 2 + i, y - windowHeight / 2 + j, compID, false) == strongPel)
            {
              strong = true;
              break;
            }
          }
        }

        if (strong)
        {
          buff->at(i, j, compID, false) = strongPel;
        }
        else
        {
          buff->at(i, j, compID, false) = 0;   // supress
        }
      }
    }
  }
}

void Canny::detect_edges(const TComPicYuv* orig, TComPicYuv* dest, unsigned int uiBitDepth, ComponentID compID)
{
  /* No noise reduction - Gaussian blur is skipped;
   Gradient calculation;
   Non-maximum suppression;
   Double threshold;
   Edge Tracking by Hysteresis.*/

  const int width = orig->getWidth(compID);
  const int height = orig->getHeight(compID);
  unsigned int convWidthS  = m_convWidthS,
               convHeightS = m_convHeightS;   // Pixel's row and col positions for Sobel filtering
  unsigned int bitDepth    = uiBitDepth;

  // tmp buff
  TComPicYuv* orientationBuf = new TComPicYuv;
  orientationBuf->createWithPadding(width, height, CHROMA_400);
  orig->copyTo(dest, compID, compID, false, false);

  /* Gradient calculation */
  gradient(dest, orientationBuf, width, height, convWidthS, convHeightS, bitDepth, compID);

  /* Non - maximum suppression */
  suppressNonMax(dest, orientationBuf, width, height, compID);

  /* Double threshold */
  doubleThreshold(dest, width, height, /*4,*/ bitDepth, compID);

  /* Edge Tracking by Hysteresis */
  edgeTracking(dest, width, height, convWidthS, convHeightS, bitDepth, compID);

  orientationBuf->destroy();
  delete orientationBuf;
  orientationBuf = nullptr;
}

// ====================================================================================================================
// Morphologigal operations - Dilation and Erosion
// ====================================================================================================================
Morph::Morph()
{
  // init();
}

Morph::~Morph()
{
  // uninit();
}

int Morph::dilation(TComPicYuv* buff, unsigned int bitDepth, ComponentID compID, int numIter, int iter)
{
  if (iter == numIter)
    return iter;

  const int width = buff->getWidth(compID);
  const int height = buff->getHeight(compID);
  unsigned int windowSize = m_kernelSize;
  unsigned int padding    = windowSize / 2;

  Pel strongPel = ((Pel) 1 << bitDepth) - 1;

  TComPicYuv* tmpBuf = new TComPicYuv;
  tmpBuf->createWithPadding(width, height, CHROMA_400);
  buff->copyTo(tmpBuf, compID, ComponentID(0), false, false);

  buff->extendPicBorder(compID, padding, padding, false);

  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      bool strong = false;
      for (int x = 0; x < windowSize; x++)
      {
        for (int y = 0; y < windowSize; y++)
        {
          if (buff->at(x - windowSize / 2 + i, y - windowSize / 2 + j, compID, false) == strongPel)
          {
            strong = true;
            break;
          }
        }
      }
      if (strong)
      {
        tmpBuf->at(i, j, ComponentID(0), false) = strongPel;
      }
    }
  }

  tmpBuf->copyTo(buff, ComponentID(0), compID, false, false);
  tmpBuf->destroy();
  delete tmpBuf;
  tmpBuf = nullptr;

  iter++;

  iter = dilation(buff, bitDepth, compID, numIter, iter);

  return iter;
}

int Morph::erosion(TComPicYuv* buff, unsigned int bitDepth, ComponentID compID, int numIter, int iter)
{
  if (iter == numIter)
    return iter;

  const int width = buff->getWidth(compID);
  const int height = buff->getHeight(compID);
  unsigned int windowSize = m_kernelSize;
  unsigned int padding    = windowSize / 2;

  TComPicYuv* tmpBuf = new TComPicYuv;
  tmpBuf->createWithPadding(width, height, CHROMA_400);
  buff->copyTo(tmpBuf, compID, ComponentID(0), false, false);

  buff->extendPicBorder(compID, padding, padding, false);

  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      bool week = false;
      for (int x = 0; x < windowSize; x++)
      {
        for (int y = 0; y < windowSize; y++)
        {
          if (buff->at(x - windowSize / 2 + i, y - windowSize / 2 + j, compID, false) == 0)
          {
            week = true;
            break;
          }
        }
      }
      if (week)
      {
        tmpBuf->at(i, j, ComponentID(0), false) = 0;
      }
    }
  }

  tmpBuf->copyTo(buff, ComponentID(0), compID, false, false);
  tmpBuf->destroy();
  delete tmpBuf;
  tmpBuf = nullptr;

  iter++;

  iter = erosion(buff, bitDepth, compID, numIter, iter);

  return iter;
}

// ====================================================================================================================
// Film Grain Analysis Functions
// ====================================================================================================================
FGAnalyser::FGAnalyser()
{
  // init();
}

FGAnalyser::~FGAnalyser()
{
  // uninit();
}

// initialize film grain parameters
void FGAnalyser::init(const int width, const int height, const int sourcePaddingWidth, const int sourcePaddingHeight,
    const InputColourSpaceConversion ipCSC, bool bClipInputVideoToRec709Range, const ChromaFormat inputChroma,
    const BitDepths& inputBitDepths, const BitDepths& outputBitDepths, const int frameSkip,
    const bool doAnalysis[], std::string filmGrainExternalMask, std::string filmGrainExternalDenoised)
{
  m_log2ScaleFactor = 2;
  for (int i = 0; i < MAX_NUM_COMPONENT; i++)
  {
    m_compModel[i].bPresentFlag          = true;
    m_compModel[i].numModelValues        = 1;
    m_compModel[i].numIntensityIntervals = 1;
    m_compModel[i].intensityValues.resize(FG_MAX_NUM_INTENSITIES);
    for (int j = 0; j < FG_MAX_NUM_INTENSITIES; j++)
    {
      m_compModel[i].intensityValues[j].intensityIntervalLowerBound = 10;
      m_compModel[i].intensityValues[j].intensityIntervalUpperBound = 250;
      m_compModel[i].intensityValues[j].compModelValue.resize(FG_MAX_ALLOWED_MODEL_VALUES);
      for (int k = 0; k < m_compModel[i].numModelValues; k++)
      {
        m_compModel[i].intensityValues[j].compModelValue[k] = 26 - 13 * (toChannelType((ComponentID) i));   // half intensity for chroma. Provided value is default value, manually tuned.
      }
    }
    m_doAnalysis[i] = doAnalysis[i];
  }

  // initialize picture parameters and create buffers
  m_chromaFormatIDC = inputChroma;
  m_bitDepthsIn = inputBitDepths;
  m_bitDepths = outputBitDepths;
  m_sourcePadding[0] = sourcePaddingWidth;
  m_sourcePadding[1] = sourcePaddingHeight;
  m_ipCSC = ipCSC;
  m_bClipInputVideoToRec709Range = bClipInputVideoToRec709Range;
  m_frameSkip = frameSkip;
  m_filmGrainExternalMask = filmGrainExternalMask;
  m_filmGrainExternalDenoised = filmGrainExternalDenoised;

  int margin = m_edgeDetector.m_convWidthG / 2;   // set margin for padding for filtering

  if (!m_originalBuf)
  {
    m_originalBuf = new TComPicYuv;
    m_originalBuf->createWithPadding(width, height, inputChroma, true, false, margin, margin);
  }
  if (!m_workingBuf)
  {
    m_workingBuf = new TComPicYuv;
    m_workingBuf->createWithPadding(width, height, inputChroma, true, false, margin, margin);
  }
  if (!m_maskBuf)
  {
    m_maskBuf = new TComPicYuv;
    m_maskBuf->createWithPadding(width, height, inputChroma, true, false, margin, margin);
  }
}

// initialize buffers with real data
bool FGAnalyser::initBufs(TComPic *pic)
{
  pic->getPicYuvTrueOrg()->copyToPic(m_originalBuf, true, false);   // original is here
  TComPicYuv     dummyPicBufferTO; // Only used temporary in yuvFrames.read
  dummyPicBufferTO.createWithoutCUInfo(m_workingBuf->getWidth(COMPONENT_Y), m_workingBuf->getHeight(COMPONENT_Y), m_chromaFormatIDC, true, m_sourcePadding[0], m_sourcePadding[1]);
  //dummyPicBufferTO.create(pic->cs->area);
  if (!m_filmGrainExternalDenoised.empty())         // read external denoised frame
  {
      TVideoIOYuv yuvFrames;
      yuvFrames.open(m_filmGrainExternalDenoised, false, m_bitDepthsIn.recon, m_bitDepthsIn.recon, m_bitDepths.recon);
      yuvFrames.skipFrames(pic->getPOC() + m_frameSkip, m_workingBuf->getWidth(COMPONENT_Y) - m_sourcePadding[0],
          m_workingBuf->getHeight(COMPONENT_Y) - m_sourcePadding[1], m_chromaFormatIDC);
      if (!yuvFrames.read(m_workingBuf, &dummyPicBufferTO, m_ipCSC, m_sourcePadding,
          m_chromaFormatIDC, m_bClipInputVideoToRec709Range))
      {
          return false; // eof or read fail
      }
      yuvFrames.close();
  }
  else   // use mctf denoised frame for film grain analysis. note: if mctf is used, it is different from mctf for encoding.
  {
      pic->getPicFilteredFG()->copyToPic(m_workingBuf);   // mctf filtered frame for film grain analysis is in here
  }

  if (!m_filmGrainExternalMask.empty())   // read external mask
  {
      TVideoIOYuv yuvFrames;
      yuvFrames.open(m_filmGrainExternalMask, false, m_bitDepthsIn.recon, m_bitDepthsIn.recon, m_bitDepths.recon);
      yuvFrames.skipFrames(pic->getPOC() + m_frameSkip, m_maskBuf->getWidth(COMPONENT_Y) - m_sourcePadding[0],
          m_maskBuf->getHeight(COMPONENT_Y) - m_sourcePadding[1], m_chromaFormatIDC);
      if (!yuvFrames.read(m_maskBuf, &dummyPicBufferTO, m_ipCSC, m_sourcePadding,
          m_chromaFormatIDC, m_bClipInputVideoToRec709Range))
      {
          return false; // eof or read fail
      }
      yuvFrames.close();
  }
  else // find mask
  {
      findMask();
  }

  return true;
}

// delete picture buffers
void FGAnalyser::destroy()
{
  if (m_originalBuf != nullptr) {
    m_originalBuf->destroy();
    delete m_originalBuf;
    m_originalBuf = nullptr;
  }
  if (m_workingBuf != nullptr)
  {
    m_workingBuf->destroy();
    delete m_workingBuf;
    m_workingBuf = nullptr;
  }
  if (m_maskBuf != nullptr)
  {
    m_maskBuf->destroy();
    delete m_maskBuf;
    m_maskBuf = nullptr;
  }
}

// main functions for film grain analysis
void FGAnalyser::estimate_grain(TComPic*pic)
{
  // estimate parameters
  estimate_grain_parameters();
}

// find flat and low complexity regions of the frame
void FGAnalyser::findMask()
{
  const int      width      = m_workingBuf->getWidth(COMPONENT_Y);
  const int      height     = m_workingBuf->getHeight(COMPONENT_Y);
  const int      newWidth2  = m_workingBuf->getWidth(COMPONENT_Y) / 2;
  const int      newHeight2 = m_workingBuf->getHeight(COMPONENT_Y) / 2;
  const int      newWidth4  = m_workingBuf->getWidth(COMPONENT_Y) / 4;
  const int      newHeight4 = m_workingBuf->getHeight(COMPONENT_Y) / 4;
  const unsigned padding    = m_edgeDetector.m_convWidthG / 2;   // for filtering

  // create tmp buffs
  TComPicYuv* workingBufSubsampled2 = new TComPicYuv;
  TComPicYuv* maskSubsampled2       = new TComPicYuv;
  TComPicYuv* workingBufSubsampled4 = new TComPicYuv;
  TComPicYuv* maskSubsampled4       = new TComPicYuv;
  TComPicYuv* maskUpsampled         = new TComPicYuv;

  workingBufSubsampled2->createWithPadding(newWidth2, newHeight2, m_workingBuf->getChromaFormat(), true, false, padding, padding);
  maskSubsampled2->createWithPadding(newWidth2, newHeight2, m_workingBuf->getChromaFormat(), true, false, padding, padding);
  workingBufSubsampled4->createWithPadding(newWidth4, newHeight4, m_workingBuf->getChromaFormat(), true, false, padding, padding);
  maskSubsampled4->createWithPadding(newWidth4, newHeight4, m_workingBuf->getChromaFormat(), true, false, padding, padding);
  maskUpsampled->createWithPadding(width, height, m_workingBuf->getChromaFormat(), true, false, padding, padding);

  for (int compIdx = 0; compIdx < getNumberValidComponents(m_chromaFormatIDC); compIdx++)
  {
    ComponentID compID    = ComponentID(compIdx);
    ChannelType channelId = toChannelType(compID);
    int         bitDepth  = m_bitDepths[channelId];

    if (!m_doAnalysis[compID])
      continue;

    // subsample original picture
    subsample(*m_workingBuf, *workingBufSubsampled2, compID, 2, padding);
    subsample(*m_workingBuf, *workingBufSubsampled4, compID, 4, padding);

    // full resolution
    m_edgeDetector.detect_edges(m_workingBuf, m_maskBuf, bitDepth, compID);
    suppressLowIntensity(*m_workingBuf, *m_maskBuf, bitDepth, compID);
    m_morphOperation.dilation(m_maskBuf, bitDepth, compID, 4);

    // subsampled 2
    m_edgeDetector.detect_edges(workingBufSubsampled2, maskSubsampled2, bitDepth, compID);
    suppressLowIntensity(*workingBufSubsampled2, *maskSubsampled2, bitDepth, compID);
    m_morphOperation.dilation(maskSubsampled2, bitDepth, compID, 3);
    
    // upsample, combine maskBuf and maskUpsampled
    upsample(*maskSubsampled2, *maskUpsampled, compID, 2);
    combineMasks(*m_maskBuf, *maskUpsampled, compID);

    // subsampled 4
    m_edgeDetector.detect_edges(workingBufSubsampled4, maskSubsampled4, bitDepth, compID);
    suppressLowIntensity(*workingBufSubsampled4, *maskSubsampled4, bitDepth, compID);
    m_morphOperation.dilation(maskSubsampled4, bitDepth, compID, 2);

    // upsample, combine maskBuf and maskUpsampled
    upsample(*maskSubsampled4, *maskUpsampled, compID, 4);
    combineMasks(*m_maskBuf, *maskUpsampled, compID);

    // final dilation to fill the holes + same number of erosion
    m_morphOperation.dilation(m_maskBuf, bitDepth, compID, 2);
    m_morphOperation.erosion(m_maskBuf, bitDepth, compID, 1);
  }

  workingBufSubsampled2->destroy();
  maskSubsampled2->destroy();
  workingBufSubsampled4->destroy();
  maskSubsampled4->destroy();
  maskUpsampled->destroy();

  delete workingBufSubsampled2;
  workingBufSubsampled2 = nullptr;
  delete maskSubsampled2;
  maskSubsampled2 = nullptr;
  delete workingBufSubsampled4;
  workingBufSubsampled4 = nullptr;
  delete maskSubsampled4;
  maskSubsampled4 = nullptr;
  delete maskUpsampled;
  maskUpsampled = nullptr;
}

void FGAnalyser::suppressLowIntensity(const TComPicYuv& buff1, TComPicYuv& buff2, unsigned int bitDepth, ComponentID compID)
{
  // buff1 - intensity values ( luma or chroma samples); buff2 - mask

  const int width = buff2.getWidth(compID);
  const int height = buff2.getHeight(compID);
  Pel maxIntensity          = ((Pel) 1 << bitDepth) - 1;
  Pel lowIntensityThreshold = (Pel)(m_lowIntensityRatio * maxIntensity);

  // strong, week, supressed
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      if (buff1.at(i, j, compID, false) < lowIntensityThreshold)
        buff2.at(i, j, compID, false) = maxIntensity;
    }
  }
}

void FGAnalyser::subsample(const TComPicYuv& input, TComPicYuv& output, ComponentID compID, const int factor, const int padding) const
{
  const int newWidth  = input.getWidth(compID) / factor;
  const int newHeight = input.getHeight(compID) / factor;

  const Pel *srcRow    = input.getAddr(compID);
  const int  srcStride = input.getStride(compID,false);
  Pel *      dstRow    = output.getAddr(compID);
  const int  dstStride = output.getStride(compID,false);

  for (int y = 0; y < newHeight; y++, srcRow += factor * srcStride, dstRow += dstStride)
  {
    const Pel *inRow      = srcRow;
    const Pel *inRowBelow = srcRow + srcStride;
    Pel *      target     = dstRow;

    for (int x = 0; x < newWidth; x++)
    {
      target[x] = (inRow[0] + inRowBelow[0] + inRow[1] + inRowBelow[1] + 2) >> 2;
      inRow += factor;
      inRowBelow += factor;
    }
  }

  if (padding)
  {
    output.extendPicBorder(compID, padding, padding, false);
  }
}

void FGAnalyser::upsample(const TComPicYuv& input, TComPicYuv& output, ComponentID compID, const int factor, const int padding) const
{
  // binary mask upsampling
  // use simple replication of pixels

  const int width  = input.getWidth(compID);
  const int height = input.getHeight(compID);

  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      Pel currentPel = input.at(i, j, compID, false);

      for (int x = 0; x < factor; x++)
      {
        for (int y = 0; y < factor; y++)
        {
          output.at(i * factor + x, j * factor + y, compID, false) = currentPel;
        }
      }
    }
  }

  if (padding)
  {
    output.extendPicBorder(compID, padding, padding, false);
  }
}

void FGAnalyser::combineMasks(TComPicYuv& buff1, TComPicYuv& buff2, ComponentID compID)
{
  const int width = buff1.getWidth(compID);
  const int height = buff1.getHeight(compID);

  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      buff1.at(i, j, compID, false) = (buff1.at(i, j, compID, false) | buff2.at(i, j, compID, false));
    }
  }
}


// estimate cut-off frequencies and scaling factors for different intensity intervals
void FGAnalyser::estimate_grain_parameters()
{
  TComPicYuv* tmpBuff = new TComPicYuv;   // tmpBuff is diference between original and filtered => film grain estimate

  tmpBuff->createWithPadding(m_workingBuf->getWidth(ComponentID(0)), m_workingBuf->getHeight(ComponentID(0)), m_workingBuf->getChromaFormat());
  m_workingBuf->copyToPic(tmpBuff, false, false);   // workingBuf is filtered image

  subtract(*m_originalBuf, *tmpBuff);   // find difference between original and filtered/reconstructed frame => film grain estimate

  int blockSize = FG_BLK_8;
  uint32_t picSizeInLumaSamples = m_workingBuf->getHeight(ComponentID(0)) * m_workingBuf->getWidth(ComponentID(0));
  if (picSizeInLumaSamples <= (1920 * 1080))
  {
    blockSize = FG_BLK_8;
  }
  else if (picSizeInLumaSamples <= (3840 * 2160))
  {
    blockSize = FG_BLK_16;
  }
  else
  {
    blockSize = FG_BLK_32;
  }

  for (int compIdx = 0; compIdx < getNumberValidComponents(m_chromaFormatIDC); compIdx++)
  {   // loop over components
    ComponentID compID    = ComponentID(compIdx);
    ChannelType channelId = toChannelType(compID);

    if (!m_doAnalysis[compID])
      continue;

    unsigned int width = m_workingBuf->getWidth(compID);    // Width of current frame
    unsigned int height = m_workingBuf->getHeight(compID);  // Height of current frame
    unsigned int windowSize  = FG_DATA_BASE_SIZE;           // Size for Film Grain block
    int          bitDepth     = m_bitDepths[channelId];
    int          detect_edges = 0;
    int          mean         = 0;
    int          var          = 0;

    std::vector<int>       vec_mean;
    std::vector<int>       vec_var;
    std::vector<PelMatrix> squared_dct_grain_block_list;

    for (int i = 0; i <= width - windowSize; i += windowSize)
    {   // loop over windowSize x windowSize blocks
      for (int j = 0; j <= height - windowSize; j += windowSize)
      {
        detect_edges = count_edges(*m_maskBuf, windowSize, compID, i, j);   // for flat region without edges

        if (detect_edges)   // selection of uniform, flat and low-complexity area; extend to other features, e.g., variance.
        {
          // find transformed blocks; cut-off frequency estimation is done on 64 x 64 blocks as low-pass filtering on synthesis side is done on 64 x 64 blocks.
          block_transform(*tmpBuff, squared_dct_grain_block_list, i, j, bitDepth, compID);
        }

        int step = windowSize / blockSize;
        for (int k = 0; k < step; k++)
        {
          for (int m = 0; m < step; m++)
          {
            detect_edges = count_edges(*m_maskBuf, blockSize, compID, i + k * blockSize, j + m * blockSize);   // for flat region without edges

            if (detect_edges)   // selection of uniform, flat and low-complexity area; extend to other features, e.g., variance.
            {
              // collect all data for parameter estimation; mean and variance are caluclated on blockSize x blockSize blocks
              mean = meanVar(*m_workingBuf, blockSize, compID, i + k * blockSize, j + m * blockSize, false);
              var  = meanVar(*tmpBuff, blockSize, compID, i + k * blockSize, j + m * blockSize, true);
              // regularize high variations; controls excessively fluctuating points
              double tmp = 3.0 * pow((double)(var), .5) + .5;
              var = (int)tmp;

              if (var < (MAX_REAL_SCALE << (bitDepth - FG_BIT_DEPTH_8))) // limit data points to meaningful values. higher variance can be result of not perfect mask estimation (non-flat regions fall in estimation process)
              {
                vec_mean.push_back(mean);   // mean of the filtered frame
                vec_var.push_back(var);     // variance of the film grain estimate
              }
            }
          }
        }
      }
    }

    // calculate film grain parameters
    estimate_cutoff_freq(squared_dct_grain_block_list, compID);
    estimate_scaling_factors(vec_mean, vec_var, bitDepth, compID);
  }

  tmpBuff->destroy();
  delete tmpBuff;
  tmpBuff = nullptr;
}

// find compModelValue[0] - different scaling based on the pixel intensity
void FGAnalyser::estimate_scaling_factors(std::vector<int> &data_x, std::vector<int> &data_y, unsigned int bitDepth, ComponentID compID)
{
  if (!m_compModel[compID].bPresentFlag || data_x.size() < MIN_POINTS_FOR_INTENSITY_ESTIMATION)   // if cutoff frequencies are not estimated previously, do not proceed since presentFlag is set to false in a previous step
    return;                                                                                      // also if there is no enough points to estimate film grain intensities, default or previously estimated parameters are used

  // estimate intensity regions
  std::vector<double> coeffs;
  std::vector<double> scalingVec;
  std::vector<int>    quantVec;
  double              distortion = 0.0;

  // Fit the points with the curve. Quantization of the curve using Lloyd Max quantization.
  bool valid;
  for (int i = 0; i < NUM_PASSES; i++)   // if num_passes = 2, filtering of the dataset points is performed
  {
    valid = fit_function(data_x, data_y, coeffs, scalingVec, ORDER, bitDepth, i);   // n-th order polynomial regression for scaling function estimation
    if (!valid)
      break;
  }
  if (valid)
  {
    avg_scaling_vec(scalingVec, compID, bitDepth);   // scale with previously fitted function to smooth the intensity
    valid = lloyd_max(scalingVec, quantVec, distortion, QUANT_LEVELS, bitDepth);   // train quantizer and quantize curve using Lloyd Max
  }

  // Based on quantized intervals, set intensity region and scaling parameter
  if (valid)   // if not valid, reuse previous parameters (for example, if var is all zero)
    setEstimatedParameters(quantVec, bitDepth, compID);
}

// Horizontal and Vertical cutoff frequencies estimation. Assumption is that for complete sequence there is only one set of the cut-off frequencies (implementation decision)
void FGAnalyser::estimate_cutoff_freq(const std::vector<PelMatrix> &blocks, ComponentID compID)
{
  PelMatrixDouble mean_squared_dct_grain(FG_DATA_BASE_SIZE, std::vector<double>(FG_DATA_BASE_SIZE));
  vector<double>  vec_mean_dct_grain_row(FG_DATA_BASE_SIZE, 0.0);
  vector<double>  vec_mean_dct_grain_col(FG_DATA_BASE_SIZE, 0.0);
  static bool     isFirstCutoffEst[MAX_NUM_COMPONENT] = {true, true, true };

  int num_blocks = (int) blocks.size();
  if (num_blocks < MIN_BLOCKS_FOR_CUTOFF_ESTIMATION)   // if there is no enough 64 x 64 blocks to estimate cut-off freq, skip cut-off freq estimation and use previous parameters
    return;

  // iterate over the block and find avarage block
  for (int x = 0; x < FG_DATA_BASE_SIZE; x++)
  {
    for (int y = 0; y < FG_DATA_BASE_SIZE; y++)
    {
      for (const auto &dct_grain_block: blocks)
      {
        mean_squared_dct_grain[x][y] += dct_grain_block[x][y];
      }
      mean_squared_dct_grain[x][y] /= num_blocks;

      // Computation of horizontal and vertical mean vector (DC component is skipped)
      vec_mean_dct_grain_row[x] += ((x != 0) && (y != 0)) ? mean_squared_dct_grain[x][y] : 0.0;
      vec_mean_dct_grain_col[y] += ((x != 0) && (y != 0)) ? mean_squared_dct_grain[x][y] : 0.0;
    }
  }

  for (int x = 0; x < FG_DATA_BASE_SIZE; x++)
  {
    vec_mean_dct_grain_row[x] /= (x == 0) ? FG_DATA_BASE_SIZE - 1 : FG_DATA_BASE_SIZE;
    vec_mean_dct_grain_col[x] /= (x == 0) ? FG_DATA_BASE_SIZE - 1 : FG_DATA_BASE_SIZE;
  }

  int cutoff_vertical   = cutoff_frequency(vec_mean_dct_grain_row);
  int cutoff_horizontal = cutoff_frequency(vec_mean_dct_grain_col);

  if (cutoff_vertical && cutoff_horizontal)
  {
    m_compModel[compID].bPresentFlag    = true;
    m_compModel[compID].numModelValues = 1;
  }
  else
  {
    m_compModel[compID].bPresentFlag = false;
  }

  if (m_compModel[compID].bPresentFlag)
  {
    if (isFirstCutoffEst[compID])   // to avoid averaging with default
    {
      m_compModel[compID].intensityValues[0].compModelValue[1] = cutoff_horizontal;
      m_compModel[compID].intensityValues[0].compModelValue[2] = cutoff_vertical;
      isFirstCutoffEst[compID]                                 = false;
    }
    else
    {
      m_compModel[compID].intensityValues[0].compModelValue[1] = (m_compModel[compID].intensityValues[0].compModelValue[1] + cutoff_horizontal + 1) / 2;
      m_compModel[compID].intensityValues[0].compModelValue[2] = (m_compModel[compID].intensityValues[0].compModelValue[2] + cutoff_vertical + 1) / 2;
    }

    if (m_compModel[compID].intensityValues[0].compModelValue[1] != 8 || m_compModel[compID].intensityValues[0].compModelValue[2] != 8)   // default is 8
      m_compModel[compID].numModelValues++;

    if (m_compModel[compID].intensityValues[0].compModelValue[1] != m_compModel[compID].intensityValues[0].compModelValue[2])
      m_compModel[compID].numModelValues++;
  }
}

int FGAnalyser::cutoff_frequency(std::vector<double> &mean)
{
  std::vector<double> sum(FG_DATA_BASE_SIZE, 0.0);

  // Regularize the curve to suppress peaks
  mean.push_back(mean.back());
  mean.insert(mean.begin(), mean.front());
  for (int j = 1; j < FG_DATA_BASE_SIZE + 1; j++)
  {
    sum[j - 1] = (m_tap_filtar[0] * mean[j - 1] + m_tap_filtar[1] * mean[j] + m_tap_filtar[2] * mean[j + 1]) / m_normTap;
  }

  double target = 0;
  for (int j = 0; j < FG_DATA_BASE_SIZE; j++)
    target += sum[j];
  target /= FG_DATA_BASE_SIZE;

  // find final cut-off frequency
  std::vector<int> intersectionPointList;

  for (int x = 0; x < FG_DATA_BASE_SIZE - 1; x++)
  {
    if ((target < sum[x] && target >= sum[x + 1]) || (target > sum[x] && target <= sum[x + 1]))
    {   // there is intersection
      double first_point  = fabs(target - sum[x]);
      double second_point = fabs(target - sum[x + 1]);
      if (first_point < second_point)
      {
        intersectionPointList.push_back(x);
      }
      else
      {
        intersectionPointList.push_back(x + 1);
      }
    }
  }

  int size = (int) intersectionPointList.size();

  if (size > 0)
  {                                                                         
    return Clip3<int>(2, 14, (intersectionPointList[size - 1] - 1) >> 2);   // clip to RDD5 range, (h-3)/4 + 0.5
  }
  else
  {
    return 0;
  }
}

// DCT-2 64x64 as defined in VVC
void FGAnalyser::block_transform(const TComPicYuv& buff, std::vector<PelMatrix> &squared_dct_grain_block_list,
                                 int offsetX, int offsetY, unsigned int bitDepth, ComponentID compID)
{
  unsigned int     windowSize       = FG_DATA_BASE_SIZE;               // Size for Film Grain block
  Intermediate_Int max_dynamic_range = (1 << (bitDepth + 6)) - 1;   // Dynamic range after DCT transform for 64x64 block
  Intermediate_Int min_dynamic_range = -((1 << (bitDepth + 6)) - 1);
  Intermediate_Int sum;

  const TMatrixCoeff *tmp = g_aiT64[TRANSFORM_FORWARD][0];
  const int transform_scale = 9;                      // upscaling of original transform as specified in VVC (for 64x64 block)
  const int add_1st = 1 << (transform_scale - 1);

  TMatrixCoeff tr[FG_DATA_BASE_SIZE][FG_DATA_BASE_SIZE];    // Original
  TMatrixCoeff trt[FG_DATA_BASE_SIZE][FG_DATA_BASE_SIZE];   // Transpose
  for (int x = 0; x < FG_DATA_BASE_SIZE; x++)
  {
    for (int y = 0; y < FG_DATA_BASE_SIZE; y++)
    {
      tr[x][y]  = tmp[x * 64 + y]; /* Matrix Original */
      trt[y][x] = tmp[x * 64 + y]; /* Matrix Transpose */
    }
  }

  // DCT transform
  PelMatrix blockDCT(windowSize, std::vector<Intermediate_Int>(windowSize));
  PelMatrix blockTmp(windowSize, std::vector<Intermediate_Int>(windowSize));

  for (int x = 0; x < windowSize; x++)
  {
    for (int y = 0; y < windowSize; y++)
    {
      sum = 0;
      for (int k = 0; k < windowSize; k++)
      {
        sum += tr[x][k] * buff.at(offsetX + k, offsetY + y, compID, false);
      }
      blockTmp[x][y] = (sum + add_1st) >> transform_scale;
    }
  }

  for (int x = 0; x < windowSize; x++)
  {
    for (int y = 0; y < windowSize; y++)
    {
      sum = 0;
      for (int k = 0; k < windowSize; k++)
      {
        sum += blockTmp[x][k] * trt[k][y];
      }
      blockDCT[x][y] = Clip3(min_dynamic_range, max_dynamic_range, (sum + add_1st) >> transform_scale);
    }
  }

  for (int x = 0; x < windowSize; x++)
  {
    for (int y = 0; y < windowSize; y++)
    {
      blockDCT[x][y] = blockDCT[x][y] * blockDCT[x][y];
    }
  }

  // store squared transformed block for further analysis
  squared_dct_grain_block_list.push_back(blockDCT);
}

// check edges
int FGAnalyser::count_edges(TComPicYuv& buffer, int windowSize, ComponentID compID, int offsetX, int offsetY)
{
  for (int x = 0; x < windowSize; x++)
  {
    for (int y = 0; y < windowSize; y++)
    {
      if (buffer.at(offsetX + x, offsetY + y, compID, false))
      {
        return 0;
      }
    }
  }

  return 1;
}

// calulate mean and variance for windowSize x windowSize block
int FGAnalyser::meanVar(TComPicYuv& buffer, int windowSize, ComponentID compID, int offsetX, int offsetY, bool getVar)
{
    double m = 0, v = 0;

    for (int x = 0; x < windowSize; x++)
    {
        for (int y = 0; y < windowSize; y++)
        {
            m += buffer.at(offsetX + x, offsetY + y, compID, false);
            v += (buffer.at(offsetX + x, offsetY + y, compID, false) * buffer.at(offsetX + x, offsetY + y, compID, false));
        }
    }

    m = m / (windowSize * windowSize);
    if (getVar)
    {
        return (int)(v / (windowSize * windowSize) - m * m + .5);
    }

    return (int)(m + .5);
}

// Fit data to a function using n-th order polynomial interpolation
bool FGAnalyser::fit_function(std::vector<int> &data_x, std::vector<int> &data_y, std::vector<double> &coeffs,
                              std::vector<double> &scalingVec, int order, int bitDepth, bool second_pass)
{
  PelMatrixLongDouble a(MAXPAIRS + 1, std::vector<long double>(MAXPAIRS + 1));
  PelVectorLongDouble B(MAXPAIRS + 1), C(MAXPAIRS + 1), S(MAXPAIRS + 1);
  long double         A1, A2, Y1, m, S1, x1;
  long double         xscale, yscale;
  long double         xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
  long double         polycoefs[MAXORDER + 1];

  int i, j, k, L, R;

  // several data filtering and data manipulations before fitting the function
  // create interval points for function fitting
  int              INTENSITY_INTERVAL_NUMBER = (1 << bitDepth) / INTERVAL_SIZE;
  std::vector<int> vec_mean_intensity(INTENSITY_INTERVAL_NUMBER, 0);
  std::vector<int> vec_variance_intensity(INTENSITY_INTERVAL_NUMBER, 0);
  std::vector<int> element_number_per_interval(INTENSITY_INTERVAL_NUMBER, 0);
  std::vector<int> tmp_data_x;
  std::vector<int> tmp_data_y;
  double           mn = 0.0, sd = 0.0;

  if (second_pass)   // in second pass, filter based on the variance of the data_y. remove all high and low points
  {
    xmin = scalingVec.back();
    scalingVec.pop_back();
    xmax = scalingVec.back();
    scalingVec.pop_back();
    int n = (int) data_y.size();
    if (n != 0)
    {
      mn = accumulate(data_y.begin(), data_y.end(), 0.0) / n;
      for (int cnt = 0; cnt < n; cnt++)
      {
        sd += (data_y[cnt] - mn) * (data_y[cnt] - mn);
      }
      sd /= n;
      sd = sqrt(sd);
    }
  }

  for (int cnt = 0; cnt < data_x.size(); cnt++)
  {
    if (second_pass)
    {
      if (data_x[cnt] >= xmin && data_x[cnt] <= xmax)
      {
        if ((data_y[cnt] < scalingVec[data_x[cnt] - (int) xmin] + sd * VAR_SCALE_UP) && (data_y[cnt] > scalingVec[data_x[cnt] - (int) xmin] - sd * VAR_SCALE_DOWN))
        {
          int block_index = data_x[cnt] / INTERVAL_SIZE;
          vec_mean_intensity[block_index] += data_x[cnt];
          vec_variance_intensity[block_index] += data_y[cnt];
          element_number_per_interval[block_index]++;
        }
      }
    }
    else
    {
      int block_index = data_x[cnt] / INTERVAL_SIZE;
      vec_mean_intensity[block_index] += data_x[cnt];
      vec_variance_intensity[block_index] += data_y[cnt];
      element_number_per_interval[block_index]++;
    }
  }

  // create a points per intensity interval
  for (int block_idx = 0; block_idx < INTENSITY_INTERVAL_NUMBER; block_idx++)
  {
    if (element_number_per_interval[block_idx] >= MIN_ELEMENT_NUMBER_PER_INTENSITY_INTERVAL)
    {
      tmp_data_x.push_back(vec_mean_intensity[block_idx] / element_number_per_interval[block_idx]);
      tmp_data_y.push_back(vec_variance_intensity[block_idx] / element_number_per_interval[block_idx]);
    }
  }

  // There needs to be at least ORDER+1 points to fit the function
  if (tmp_data_x.size() < (order + 1))
	  return false;   // if there is no enough blocks to estimate film grain parameters, default or previously estimated parameters are used

  for (i = 0; i < tmp_data_x.size(); i++) // remove single points before extending and fitting
  {
	  int check = 0;
	  for (j = -WINDOW; j <= WINDOW; j++)
	  {
		  int idx = i + j;
		  if (idx >= 0 && idx < tmp_data_x.size() && j != 0)
		  {
			  check += abs(tmp_data_x[i] / INTERVAL_SIZE - tmp_data_x[idx] / INTERVAL_SIZE) <= WINDOW ? 1 : 0;
		  }
	  }

	  if (check < NBRS)
	  {
		  tmp_data_x.erase(tmp_data_x.begin() + i);
		  tmp_data_y.erase(tmp_data_y.begin() + i);
		  i--;
	  }
  }

  extend_points(tmp_data_x, tmp_data_y, bitDepth);   // find the most left and the most right point, and extend edges

  // fitting the function starts here
  xmin = tmp_data_x[0];
  xmax = tmp_data_x[0];
  ymin = tmp_data_y[0];
  ymax = tmp_data_y[0];
  for (i = 0; i < tmp_data_x.size(); i++)
  {
    if (tmp_data_x[i] < xmin)
      xmin = tmp_data_x[i];
    if (tmp_data_x[i] > xmax)
      xmax = tmp_data_x[i];
    if (tmp_data_y[i] < ymin)
      ymin = tmp_data_y[i];
    if (tmp_data_y[i] > ymax)
      ymax = tmp_data_y[i];
  }

  long double xlow = xmax;
  long double ylow = ymax;

  int data_pairs = (int) tmp_data_x.size();

  PelMatrixDouble data_array(2, std::vector<double>(MAXPAIRS + 1));

  for (i = 0; i < data_pairs; i++)
  {
    data_array[0][i + 1] = (double) tmp_data_x[i];
    data_array[1][i + 1] = (double) tmp_data_y[i];
  }

  // release memory for data_x and data_y, and clear previous vectors
  vector<int>().swap(tmp_data_x);
  vector<int>().swap(tmp_data_y);
  if (second_pass)
  {
    vector<int>().swap(data_x);
    vector<int>().swap(data_y);
    vector<double>().swap(coeffs);
    vector<double>().swap(scalingVec);
  }

  for (i = 1; i <= data_pairs; i++)
  {
    if (data_array[0][i] < xlow && data_array[0][i] != 0)
      xlow = data_array[0][i];
    if (data_array[1][i] < ylow && data_array[1][i] != 0)
      ylow = data_array[1][i];
  }

  if (xlow < .001 && xmax < 1000)
    xscale = 1 / xlow;
  else if (xmax > 1000 && xlow > .001)
    xscale = 1 / xmax;
  else
    xscale = 1;

  if (ylow < .001 && ymax < 1000)
    yscale = 1 / ylow;
  else if (ymax > 1000 && ylow > .001)
    yscale = 1 / ymax;
  else
    yscale = 1;

  // initialise array variables
  for (i = 0; i <= MAXPAIRS; i++)
  {
    B[i] = 0;
    C[i] = 0;
    S[i] = 0;
    for (j = 0; j < MAXPAIRS; j++)
      a[i][j] = 0;
  }

  for (i = 0; i <= MAXORDER; i++)
    polycoefs[i] = 0;

  Y1 = 0;
  for (j = 1; j <= data_pairs; j++)
  {
    for (i = 1; i <= order; i++)
    {
      B[i] = B[i] + data_array[1][j] * yscale * ldpow(data_array[0][j] * xscale, i);
      if (B[i] == std::numeric_limits<long double>::max())
        return false;
      for (k = 1; k <= order; k++)
      {
        a[i][k] = a[i][k] + ldpow(data_array[0][j] * xscale, (i + k));
        if (a[i][k] == std::numeric_limits<long double>::max())
          return false;
      }
      S[i] = S[i] + ldpow(data_array[0][j] * xscale, i);
      if (S[i] == std::numeric_limits<long double>::max())
        return false;
    }
    Y1 = Y1 + data_array[1][j] * yscale;
    if (Y1 == std::numeric_limits<long double>::max())
      return false;
  }

  for (i = 1; i <= order; i++)
  {
    for (j = 1; j <= order; j++)
    {
      a[i][j] = a[i][j] - S[i] * S[j] / (long double) data_pairs;
      if (a[i][j] == std::numeric_limits<long double>::max())
        return false;
    }
    B[i] = B[i] - Y1 * S[i] / (long double) data_pairs;
    if (B[i] == std::numeric_limits<long double>::max())
      return false;
  }

  for (k = 1; k <= order; k++)
  {
    R  = k;
    A1 = 0;
    for (L = k; L <= order; L++)
    {
      A2 = fabsl(a[L][k]);
      if (A2 > A1)
      {
        A1 = A2;
        R  = L;
      }
    }
    if (A1 == 0)
      return false;
    if (R == k)
      goto polfit1;
    for (j = k; j <= order; j++)
    {
      x1      = a[R][j];
      a[R][j] = a[k][j];
      a[k][j] = x1;
    }
    x1   = B[R];
    B[R] = B[k];
    B[k] = x1;
  polfit1:
    for (i = k; i <= order; i++)
    {
      m = a[i][k];
      for (j = k; j <= order; j++)
      {
        if (i == k)
          a[i][j] = a[i][j] / m;
        else
          a[i][j] = a[i][j] - m * a[k][j];
      }
      if (i == k)
        B[i] = B[i] / m;
      else
        B[i] = B[i] - m * B[k];
    }
  }

  polycoefs[order] = B[order];
  for (k = 1; k <= order - 1; k++)
  {
    i  = order - k;
    S1 = 0;
    for (j = 1; j <= order; j++)
    {
      S1 = S1 + a[i][j] * polycoefs[j];
      if (S1 == std::numeric_limits<long double>::max())
        return false;
    }
    polycoefs[i] = B[i] - S1;
  }

  S1 = 0;
  for (i = 1; i <= order; i++)
  {
    S1 = S1 + polycoefs[i] * S[i] / (long double) data_pairs;
    if (S1 == std::numeric_limits<long double>::max())
      return false;
  }
  polycoefs[0] = (Y1 / (long double) data_pairs - S1);

  // zero all coeficient values smaller than +/- .00000000001 (avoids -0)
  for (i = 0; i <= order; i++)
    if (fabsl(polycoefs[i] * 100000000000) < 1)
      polycoefs[i] = 0;

  // rescale parameters
  for (i = 0; i <= order; i++)
  {
    polycoefs[i] = (1 / yscale) * polycoefs[i] * ldpow(xscale, i);
    coeffs.push_back(polycoefs[i]);
  }

  // create fg scaling function. interpolation based on coeffs which returns lookup table from 0 - 2^B-1. n-th order polinomial regression
  for (i = (int) xmin; i <= (int) xmax; i++)
  {
    double val = coeffs[0];
    for (j = 1; j < coeffs.size(); j++)
    {
      val += (coeffs[j] * ldpow(i, j));
    }

    val = Clip3(0.0, (double) (1 << bitDepth) - 1, val);
    scalingVec.push_back(val);
  }

  // save in scalingVec min and max value for further use
  scalingVec.push_back(xmax);
  scalingVec.push_back(xmin);

  return true;
}

// avg scaling vector with previous result to smooth transition betweeen frames
void FGAnalyser::avg_scaling_vec(std::vector<double> &scalingVec, ComponentID compID, int bitDepth)
{
  int xmin = (int) scalingVec.back();
  scalingVec.pop_back();
  int xmax = (int) scalingVec.back();
  scalingVec.pop_back();

  static std::vector<std::vector<double>> scalingVecAvg(MAX_NUM_COMPONENT, std::vector<double>((int)(1<<bitDepth)));
  static bool                isFirstScalingEst[MAX_NUM_COMPONENT] = { true, true, true };

  if (isFirstScalingEst[compID])
  {
    for (int i = xmin; i <= xmax; i++)
      scalingVecAvg[compID][i] = scalingVec[i-xmin];

    isFirstScalingEst[compID] = false;
  }
  else
  {
    for (int i = 0; i < scalingVec.size(); i++)
      scalingVecAvg[compID][i + xmin] += scalingVec[i];
    for (int i = 0; i < scalingVecAvg[compID].size(); i++)
      scalingVecAvg[compID][i] /= 2;
  }

  // re-init scaling vec and add new min and max to be used in other functions
  int index = 0;
  for (; index < scalingVecAvg[compID].size(); index++)
    if (scalingVecAvg[compID][index])
      break;
  xmin = index;

  index = (int) scalingVecAvg[compID].size() - 1;
  for (; index >=0 ; index--)
    if (scalingVecAvg[compID][index])
      break;
  xmax = index;

  scalingVec.resize(xmax - xmin + 1);
  for (int i = xmin; i <= xmax; i++)
    scalingVec[i-xmin] = scalingVecAvg[compID][i];

  scalingVec.push_back(xmax);
  scalingVec.push_back(xmin);
}

// Lloyd Max quantizer
bool FGAnalyser::lloyd_max(std::vector<double> &scalingVec, std::vector<int> &quantizedVec, double &distortion, int numQuantizedLevels, int bitDepth)
{
  if (scalingVec.size() <= 0) { throw("Empty training dataset."); }

  int xmin = (int) scalingVec.back();
  scalingVec.pop_back();
  scalingVec.pop_back();   // dummy pop_pack ==> int xmax = (int)scalingVec.back();

  double ymin          = 0.0;
  double ymax          = 0.0;
  double init_training = 0.0;
  double tolerance     = 0.0000001;
  double last_distor   = 0.0;
  double rel_distor    = 0.0;

  std::vector<double> codebook(numQuantizedLevels);
  std::vector<double> partition(numQuantizedLevels - 1);

  std::vector<double> tmpVec(scalingVec.size(), 0.0);
  distortion = 0.0;

  ymin = scalingVec[0];
  ymax = scalingVec[0];
  for (int i = 0; i < scalingVec.size(); i++)
  {
    if (scalingVec[i] < ymin)
      ymin = scalingVec[i];
    if (scalingVec[i] > ymax)
      ymax = scalingVec[i];
  }

  init_training = (ymax - ymin) / numQuantizedLevels;

  if (init_training <= 0)
  {
    // msg(WARNING, "Invalid training dataset. Film grain parameter estimation is not performed. Default or previously estimated parameters are reused.\n");
    return false;
  }

  // initial codebook
  double step = init_training / 2;
  for (int i = 0; i < numQuantizedLevels; i++)
  {
    codebook[i] = ymin + i * init_training + step;
  }

  // initial partition
  for (int i = 0; i < numQuantizedLevels - 1; i++)
  {
    partition[i] = (codebook[i] + codebook[i + 1]) / 2;
  }

  // quantizer initialization
  quantize(scalingVec, tmpVec, distortion, partition, codebook);

  double tolerance2 = std::numeric_limits<double>::epsilon() * ymax;
  if (distortion > tolerance2)
    rel_distor = abs(distortion - last_distor) / distortion;
  else
    rel_distor = distortion;

  // optimization: find optimal codebook and partition
  while ((rel_distor > tolerance) && (rel_distor > tolerance2))
  {
    for (int i = 0; i < numQuantizedLevels; i++)
    {
      int    count = 0;
      double sum   = 0.0;

      for (int j = 0; j < tmpVec.size(); j++)
      {
        if (codebook[i] == tmpVec[j])
        {
          count++;
          sum += scalingVec[j];
        }
      }

      if (count)
      {
        codebook[i] = sum / (double) count;
      }
      else
      {
        sum   = 0.0;
        count = 0;
        if (i == 0)
        {
          for (int j = 0; j < tmpVec.size(); j++)
          {
            if (scalingVec[j] <= partition[i])
            {
              count++;
              sum += scalingVec[j];
            }
          }
          if (count)
          {
            codebook[i] = sum / (double) count;
          }
          else
          {
            codebook[i] = (partition[i] + ymin) / 2;
          }
        }
        else if (i == numQuantizedLevels - 1)
        {
          for (int j = 0; j < tmpVec.size(); j++)
          {
            if (scalingVec[j] >= partition[i - 1])
            {
              count++;
              sum += scalingVec[j];
            }
          }
          if (count)
          {
            codebook[i] = sum / (double) count;
          }
          else
          {
            codebook[i] = (partition[i - 1] + ymax) / 2;
          }
        }
        else
        {
          for (int j = 0; j < tmpVec.size(); j++)
          {
            if (scalingVec[j] >= partition[i - 1] && scalingVec[j] <= partition[i])
            {
              count++;
              sum += scalingVec[j];
            }
          }
          if (count)
          {
            codebook[i] = sum / (double) count;
          }
          else
          {
            codebook[i] = (partition[i - 1] + partition[i]) / 2;
          }
        }
      }
    }

    // compute and sort partition
    for (int i = 0; i < numQuantizedLevels - 1; i++)
    {
      partition[i] = (codebook[i] + codebook[i + 1]) / 2;
    }
    std::sort(partition.begin(), partition.end());

    // final quantization - testing condition
    last_distor = distortion;
    quantize(scalingVec, tmpVec, distortion, partition, codebook);

    if (distortion > tolerance2)
      rel_distor = abs(distortion - last_distor) / distortion;
    else
      rel_distor = distortion;
  }

  // fill the final quantized vector
  quantizedVec.resize((int) (1 << bitDepth), 0);
  for (int i = 0; i < tmpVec.size(); i++)
    quantizedVec[i + xmin] = Clip3(0, FG_MAX_STANDARD_DEVIATION << (bitDepth - FG_BIT_DEPTH_8), (int) (tmpVec[i] + .5));

  return true;
}

void FGAnalyser::quantize(std::vector<double> &scalingVec, std::vector<double> &quantizedVec, double &distortion,
                          std::vector<double> partition, std::vector<double> codebook)
{
  if (partition.size() <= 0 || codebook.size() <= 0) { throw("Check partitions and codebook."); }

  // reset previous quantizedVec to 0 and distortion to 0
  std::fill(quantizedVec.begin(), quantizedVec.end(), 0.0);
  distortion = 0.0;

  // quantize input vector
  for (int i = 0; i < scalingVec.size(); i++)
  {
    for (int j = 0; j < partition.size(); j++)
    {
      quantizedVec[i] =
        quantizedVec[i] + (scalingVec[i] > partition[j]);   // partition need to be sorted in acceding order
    }
    quantizedVec[i] = codebook[(int) quantizedVec[i]];
  }

  // compute distortion - mse
  for (int i = 0; i < scalingVec.size(); i++)
  {
    distortion += ((scalingVec[i] - quantizedVec[i]) * (scalingVec[i] - quantizedVec[i]));
  }
  distortion /= scalingVec.size();
}

// Set correctlly SEI parameters based on the quantized curve
void FGAnalyser::setEstimatedParameters(std::vector<int> &quantizedVec, unsigned int bitDepth, ComponentID compID)
{
  std::vector<std::vector<int>> finalIntervalsandScalingFactors(3);   // lower_bound, upper_bound, scaling_factor

  int cutoff_horizontal = m_compModel[compID].intensityValues[0].compModelValue[1];
  int cutoff_vertical   = m_compModel[compID].intensityValues[0].compModelValue[2];

  // calculate intervals and scaling factors
  define_intervals_and_scalings(finalIntervalsandScalingFactors, quantizedVec, bitDepth);

  // merge small intervals with left or right interval
  for (int i = 0; i < finalIntervalsandScalingFactors[2].size(); i++)
  {
    int tmp1 = finalIntervalsandScalingFactors[1][i] - finalIntervalsandScalingFactors[0][i];

    if (tmp1 < (2 << (bitDepth - FG_BIT_DEPTH_8)))
    {
      int diffRight =
        (i == (finalIntervalsandScalingFactors[2].size() - 1)) || (finalIntervalsandScalingFactors[2][i + 1] == 0)
          ? std::numeric_limits<int>::max()
          : abs(finalIntervalsandScalingFactors[2][i] - finalIntervalsandScalingFactors[2][i + 1]);
      int diffLeft = (i == 0) || (finalIntervalsandScalingFactors[2][i - 1] == 0)
                       ? std::numeric_limits<int>::max()
                       : abs(finalIntervalsandScalingFactors[2][i] - finalIntervalsandScalingFactors[2][i - 1]);

      if (diffLeft < diffRight)   // merge with left
      {
        int tmp2     = finalIntervalsandScalingFactors[1][i - 1] - finalIntervalsandScalingFactors[0][i - 1];
        int newScale = (tmp2 * finalIntervalsandScalingFactors[2][i - 1] + tmp1 * finalIntervalsandScalingFactors[2][i]) / (tmp2 + tmp1);

        finalIntervalsandScalingFactors[1][i - 1] = finalIntervalsandScalingFactors[1][i];
        finalIntervalsandScalingFactors[2][i - 1] = newScale;
        for (int j = 0; j < 3; j++)
          finalIntervalsandScalingFactors[j].erase(finalIntervalsandScalingFactors[j].begin() + i);
        i--;
      }
      else   // merge with right
      {
        int tmp2     = finalIntervalsandScalingFactors[1][i + 1] - finalIntervalsandScalingFactors[0][i + 1];
        int newScale = (tmp2 * finalIntervalsandScalingFactors[2][i + 1] + tmp1 * finalIntervalsandScalingFactors[2][i]) / (tmp2 + tmp1);

        finalIntervalsandScalingFactors[1][i] = finalIntervalsandScalingFactors[1][i + 1];
        finalIntervalsandScalingFactors[2][i] = newScale;
        for (int j = 0; j < 3; j++)
          finalIntervalsandScalingFactors[j].erase(finalIntervalsandScalingFactors[j].begin() + i + 1);
        i--;
      }
    }
  }

  // scale to 8-bit range as supported by current sei and rdd5
  scale_down(finalIntervalsandScalingFactors, bitDepth);

  // because of scaling in previous step, some intervals may overlap. Check intervals for errors.
  confirm_intervals(finalIntervalsandScalingFactors);

  // set number of intervals; exculde intervals with scaling factor 0.
  m_compModel[compID].numIntensityIntervals =
    (int) finalIntervalsandScalingFactors[2].size()
    - (int) count(finalIntervalsandScalingFactors[2].begin(), finalIntervalsandScalingFactors[2].end(), 0);

  if (m_compModel[compID].numIntensityIntervals == 0)
  {   // check if all intervals are 0, and if yes set presentFlag to false
    m_compModel[compID].bPresentFlag = false;
    return;
  }

  // set final interval boundaries and scaling factors. check if some interval has scaling factor 0, and do not encode
  // them within SEI.
  for (int i = 0, j = 0; i < finalIntervalsandScalingFactors[2].size(); i++)
  {
    if (finalIntervalsandScalingFactors[2][i] != 0)
    {
      m_compModel[compID].intensityValues[j].intensityIntervalLowerBound = finalIntervalsandScalingFactors[0][i];
      m_compModel[compID].intensityValues[j].intensityIntervalUpperBound = finalIntervalsandScalingFactors[1][i];
      m_compModel[compID].intensityValues[j].compModelValue[0]           = finalIntervalsandScalingFactors[2][i];
      m_compModel[compID].intensityValues[j].compModelValue[1]           = cutoff_horizontal;
      m_compModel[compID].intensityValues[j].compModelValue[2]           = cutoff_vertical;
      j++;
    }
  }
}

long double FGAnalyser::ldpow(long double n, unsigned p)
{
  long double x = 1;
  unsigned    i;

  for (i = 0; i < p; i++)
    x = x * n;

  return x;
}

// find bounds of intensity intervals and scaling factors for each interval
void FGAnalyser::define_intervals_and_scalings(std::vector<std::vector<int>> &parameters,
                                               std::vector<int> &quantizedVec, int bitDepth)
{
  parameters[0].push_back(0);
  parameters[2].push_back(quantizedVec[0]);
  for (int i = 0; i < quantizedVec.size() - 1; i++)
  {
    if (quantizedVec[i] != quantizedVec[i + 1])
    {
      parameters[0].push_back(i + 1);
      parameters[1].push_back(i);
      parameters[2].push_back(quantizedVec[i + 1]);
    }
  }
  parameters[1].push_back((1 << bitDepth) - 1);
}

// scale everything to 8-bit ranges as supported by SEI message
void FGAnalyser::scale_down(std::vector<std::vector<int>> &parameters, int bitDepth)
{
  for (int i = 0; i < parameters[2].size(); i++)
  {
    parameters[0][i] >>= (bitDepth - FG_BIT_DEPTH_8);
    parameters[1][i] >>= (bitDepth - FG_BIT_DEPTH_8);
    parameters[2][i] <<= m_log2ScaleFactor;
    parameters[2][i] >>= (bitDepth - FG_BIT_DEPTH_8);
  }
}

// check if intervals are properly set after scaling to 8-bit representation
void FGAnalyser::confirm_intervals(std::vector<std::vector<int>> &parameters)
{
  std::vector<int> tmp;
  for (int i = 0; i < parameters[2].size(); i++)
  {
    tmp.push_back(parameters[0][i]);
    tmp.push_back(parameters[1][i]);
  }
  for (int i = 0; i < tmp.size() - 1; i++)
  {
    if (tmp[i] == tmp[i + 1])
      tmp[i + 1]++;
  }
  for (int i = 0; i < parameters[2].size(); i++)
  {
    parameters[0][i] = tmp[2 * i];
    parameters[1][i] = tmp[2 * i + 1];
  }
}

void FGAnalyser::extend_points(std::vector<int> &data_x, std::vector<int> &data_y, int bitDepth)
{
  int xmin = data_x[0];
  int xmax = data_x[0];
  int ymin = data_y[0];
  int ymax = data_y[0];
  for (int i = 0; i < data_x.size(); i++)
  {
    if (data_x[i] < xmin)
    {
      xmin = data_x[i];
      ymin = data_y[i];   // not real ymin
    }
    if (data_x[i] > xmax)
    {
      xmax = data_x[i];
      ymax = data_y[i];   // not real ymax
    }
  }

  // extend points to the left
  int    step  = POINT_STEP;
  double scale = POINT_SCALE;
  int num_extra_point_left = MAX_NUM_POINT_TO_EXTEND;
  int num_extra_point_right = MAX_NUM_POINT_TO_EXTEND;
  while (xmin >= step && ymin > 1 && num_extra_point_left > 0)
  {
    xmin -= step;
    ymin = static_cast<int>(ymin / scale);
    data_x.push_back(xmin);
    data_y.push_back(ymin);
    num_extra_point_left--;
  }

  // extend points to the right
  while (xmax + step <= ((1 << bitDepth) - 1) && ymax > 1 && num_extra_point_right > 0)
  {
    xmax += step;
    ymax = static_cast<int>(ymax / scale);
    data_x.push_back(xmax);
    data_y.push_back(ymax);
    num_extra_point_right--;
  }
  for (int i = 0; i < data_x.size(); i++)
  {
      if (data_x[i] < MIN_INTENSITY || data_x[i] > MAX_INTENSITY)
      {
          data_x.erase(data_x.begin() + i);
          data_y.erase(data_y.begin() + i);
          i--;
      }
  }
}

void FGAnalyser::subtract(TComPicYuv& buffer1, TComPicYuv& buffer2)
{
  for (int compIdx = 0; compIdx < getNumberValidComponents(buffer1.getChromaFormat()); compIdx++)
  {
    ComponentID compID = ComponentID(compIdx);
    const Int widthSrc = buffer1.getWidth(compID);
    const Int heightSrc = buffer1.getHeight(compID);

    for (int x=0; x<widthSrc; x++)
    {
      for (int y = 0; y < heightSrc; y++)
      {
        buffer2.at(x,y, compID, false) -= buffer1.at(x, y, compID, false);
      }
    }
  }
}

#endif