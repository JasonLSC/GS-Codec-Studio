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

/** \file     Analyze.h
    \brief    encoder analyzer class (header)
*/

#ifndef __ANALYZE__
#define __ANALYZE__

#pragma once

#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <cinttypes>
#include "CommonLib/CommonDef.h"
#include "CommonLib/ChromaFormat.h"
#include "math.h"
#if EXTENSION_360_VIDEO
#include "AppEncHelper360/TExt360EncAnalyze.h"
#endif

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// encoder analyzer class
class Analyze
{
private:
  uint32_t m_picCount;

  double m_dPSNRSum[MAX_NUM_COMPONENT];
  double m_dAddBits;
  Fraction m_frameRate;
  double m_mseYuvFrame[MAX_NUM_COMPONENT];   // sum of MSEs
  double m_upscaledPSNR[MAX_NUM_COMPONENT];
  double m_msssim[MAX_NUM_COMPONENT];
  double m_upscaledMsssim[MAX_NUM_COMPONENT];

#if EXTENSION_360_VIDEO
  TExt360EncAnalyze m_ext360;
#endif
#if JVET_O0756_CALCULATE_HDRMETRICS
  double m_logDeltaESum[hdrtoolslib::NB_REF_WHITE];
  double m_psnrLSum[hdrtoolslib::NB_REF_WHITE];
#endif

public:
  virtual ~Analyze()  {}
  Analyze() { clear(); }

  void addResult(const double psnr[MAX_NUM_COMPONENT], double bits, const double mseYuvFrame[MAX_NUM_COMPONENT],
                 const double upscaledPSNR[MAX_NUM_COMPONENT], const double msssim[MAX_NUM_COMPONENT],
                 const double upscaledMsssim[MAX_NUM_COMPONENT], bool isEncodeLtRef)
  {
    m_dAddBits  += bits;
    if (isEncodeLtRef)
    {
      return;
    }
    for(uint32_t i=0; i<MAX_NUM_COMPONENT; i++)
    {
      m_dPSNRSum[i] += psnr[i];
      m_mseYuvFrame[i] += mseYuvFrame[i];
      m_upscaledPSNR[i] += upscaledPSNR[i];
      m_msssim[i] += msssim[i];
      m_upscaledMsssim[i] += upscaledMsssim[i];
    }

    m_picCount++;
  }
  double getWPSNR(ComponentID compID) const { return m_dPSNRSum[compID] / (double) m_picCount; }
  double getPsnr(ComponentID compID) const { return m_dPSNRSum[compID]; }
  double getMsssim(ComponentID compID) const { return m_msssim[compID]; }
#if JVET_O0756_CALCULATE_HDRMETRICS
  double getDeltaE() const { return m_logDeltaESum[0]; }
  double getPsnrL() const { return m_psnrLSum[0]; }
#endif
  double   getBits() const { return m_dAddBits; }
  void     setBits(double numBits) { m_dAddBits = numBits; }
  uint32_t getNumPic() const { return m_picCount; }

#if EXTENSION_360_VIDEO
  TExt360EncAnalyze& getExt360Info() { return m_ext360; }
#endif
#if JVET_O0756_CALCULATE_HDRMETRICS
  void addHDRMetricsResult(double deltaE[hdrtoolslib::NB_REF_WHITE], double psnrL[hdrtoolslib::NB_REF_WHITE])
  {
    for (int i=0; i<hdrtoolslib::NB_REF_WHITE; i++)
    {
      m_logDeltaESum[i] += deltaE[i];
      m_psnrLSum[i] += psnrL[i];
    }
  }
#endif

  void setFrameRate(const Fraction& frameRate) { m_frameRate = frameRate; }
  void clear()
  {
    m_dAddBits = 0;
    for(uint32_t i=0; i<MAX_NUM_COMPONENT; i++)
    {
      m_dPSNRSum[i] = 0;
      m_mseYuvFrame[i]  = 0;
      m_upscaledPSNR[i] = 0;
      m_msssim[i] = 0;
      m_upscaledMsssim[i] = 0;
    }
    m_picCount = 0;
#if EXTENSION_360_VIDEO
    m_ext360.clear();
#endif
#if JVET_O0756_CALCULATE_HDRMETRICS
    for (int i=0; i<hdrtoolslib::NB_REF_WHITE; i++)
    {
      m_logDeltaESum[i] = 0.0;
      m_psnrLSum[i] = 0.0;
    }
#endif
  }

  void calculateCombinedValues(const ChromaFormat chFmt, double &PSNRyuv, double &mseYuv, const BitDepths &bitDepths)
  {
    mseYuv    = 0;
    int scale = 0;

    const int maximumBitDepth = std::max(bitDepths[ChannelType::LUMA], bitDepths[ChannelType::CHROMA]);

    const uint32_t maxval                = 255 << (maximumBitDepth - 8);
    const uint32_t numberValidComponents = getNumberValidComponents(chFmt);

    for (uint32_t comp=0; comp<numberValidComponents; comp++)
    {
      const ComponentID compID = ComponentID(comp);

      const uint32_t csx       = getComponentScaleX(compID, chFmt);
      const uint32_t csy       = getComponentScaleY(compID, chFmt);
      const int      scaleChan = 4 >> (csx + csy);
      const uint32_t bitDepthShift =
        2 * (maximumBitDepth - bitDepths[toChannelType(compID)]);   //*2 because this is a squared number

      const double channelMSE = (m_mseYuvFrame[compID] * double(1 << bitDepthShift)) / double(getNumPic());

      scale  += scaleChan;
      mseYuv += scaleChan * channelMSE;
    }

    mseYuv /= double(scale);   // i.e. divide by 6 for 4:2:0, 8 for 4:2:2 etc.
    PSNRyuv = (mseYuv == 0) ? 999.99 : 10.0 * log10((maxval * maxval) / mseYuv);
  }

  void printOut(std::string &header, std::string &metrics, const std::string &delim, ChromaFormat chFmt,
                bool printMSEBasedSNR, bool printSequenceMSE, bool printMSSSIM, bool printHexPsnr, bool printRprPsnr,
                const BitDepths &bitDepths, bool useWPSNR, [[maybe_unused]] bool printHdrMetrics)
  {
    std::ostringstream headeross, metricoss;

    auto addField = [&](const std::string &header, const char *fmt, auto x, bool withchroma = true)
    {
      if (!withchroma)
      {
        return;
      }
      char buffer[512];
      headeross<<header;
      snprintf(buffer,512,fmt,x);
      metricoss<<buffer;
    };

    auto hexValue = [](double x) -> uint64_t
    {
      uint64_t y;
      std::copy(reinterpret_cast<uint8_t *>(&x), reinterpret_cast<uint8_t *>(&x) + sizeof(x),
                reinterpret_cast<uint8_t *>(&y));
      return y;
    };

    double fps   = m_frameRate.getFloatVal();
    double scale = fps / 1000 / (double) m_picCount;

    double mseBasedSNR[MAX_NUM_COMPONENT];
    if (printMSEBasedSNR || printRprPsnr)
    {
      for (uint32_t componentIndex = 0; componentIndex < MAX_NUM_COMPONENT; componentIndex++)
      {
        const ComponentID compID = ComponentID(componentIndex);
        if (getNumPic() == 0)
        {
          // this is the same calculation that will be evaluated for any other statistic when there are no frames (it
          // should result in NaN). We use it here so all the output is consistent
          mseBasedSNR[compID] = 0 * scale;
        }
        else
        {
          const uint32_t maxval = 255 << (bitDepths[toChannelType(compID)] - 8);
          const double   MSE    = m_mseYuvFrame[compID];
          mseBasedSNR[compID] = (MSE == 0) ? 999.99 : 10.0 * log10((maxval * maxval) / (MSE / (double)getNumPic()));
        }
      }
    }

    addField("\tTotal Frames", "\t%-8d    ", getNumPic());
    addField(" |  ", " %s ", delim.c_str());
    addField("Bitrate      ", "%-12.4lf ", getBits() * scale);

    const bool withchroma = isChromaEnabled(chFmt);
    double psnrYUV = MAX_DOUBLE;
    double mseYUV  = MAX_DOUBLE;
    if (withchroma)
    {
      calculateCombinedValues(chFmt, psnrYUV, mseYUV, bitDepths);
    }

    if (useWPSNR)
    {
      addField("Y-WPSNR   ", "%-8.4lf  ", getWPSNR(COMPONENT_Y));
      addField("U-WPSNR   ", "%-8.4lf  ", getWPSNR(COMPONENT_Cb), withchroma);
      addField("V-WPSNR   ", "%-8.4lf  ", getWPSNR(COMPONENT_Cr), withchroma);
      addField("YUV-WPSNR ", "%-8.4lf  ", psnrYUV, withchroma);
    }
    else
    {
      addField("Y-PSNR   ", "%-8.4lf ", getPsnr(COMPONENT_Y) / (double) getNumPic());
      addField("U-PSNR   ", "%-8.4lf ", getPsnr(COMPONENT_Cb) / (double) getNumPic(), withchroma);
      addField("V-PSNR   ", "%-8.4lf ", getPsnr(COMPONENT_Cr) / (double) getNumPic(), withchroma);
      addField("YUV-PSNR ", "%-8.4lf ", psnrYUV, withchroma);
    }
#if JVET_O0756_CALCULATE_HDRMETRICS
    if (printHdrMetrics && withchroma)
    {
      addField("DeltaE   ", "%-8.4lf ", getDeltaE() / (double) getNumPic());
      addField("PSNRL    ", "%-8.4lf ", getPsnrL() / (double) getNumPic());
    }
#endif
#if EXTENSION_360_VIDEO
    m_ext360.printInfos(headeross,metricoss,getNumPic());
#endif
    if (printHexPsnr)
    {
      if (useWPSNR)
      {
        addField("xY-WPSNR         ", "%-16" PRIx64 " ", hexValue(getWPSNR(COMPONENT_Y)));
        addField("xU-WPSNR         ", "%-16" PRIx64 " ", hexValue(getWPSNR(COMPONENT_Cb)), withchroma);
        addField("xV-WPSNR         ", "%-16" PRIx64 " ", hexValue(getWPSNR(COMPONENT_Cr)), withchroma);
      }
      else
      {
        addField("xY-PSNR          ", "%-16" PRIx64 " ", hexValue(getPsnr(COMPONENT_Y) / (double) getNumPic()));
        addField("xU-PSNR          ", "%-16" PRIx64 " ", hexValue(getPsnr(COMPONENT_Cb) / (double) getNumPic()),
                 withchroma);
        addField("xV-PSNR          ", "%-16" PRIx64 " ", hexValue(getPsnr(COMPONENT_Cr) / (double) getNumPic()),
                 withchroma);
      }
    }
#if JVET_O0756_CALCULATE_HDRMETRICS
    if (printHdrMetrics && printHexPsnr && withchroma)
    {
      addField("xDeltaE          ", "%-16" PRIx64 " ", hexValue(getDeltaE() / (double) getNumPic()));
      addField("xPSNRL           ", "%-16" PRIx64 " ", hexValue(getPsnrL() / (double) getNumPic()));
    }
#endif
    if (printMSSSIM)
    {
      addField("Y-MS-SSIM  ", "%-9.7lf  ", getMsssim(COMPONENT_Y) / (double) getNumPic());
      addField("U-MS-SSIM  ", "%-9.7lf  ", getMsssim(COMPONENT_Cb) / (double) getNumPic(), withchroma);
      addField("V-MS-SSIM  ", "%-9.7lf  ", getMsssim(COMPONENT_Cr) / (double) getNumPic(), withchroma);
    }
    if (printSequenceMSE)
    {
      addField("Y-MSE      ", "%-10.4lf ", m_mseYuvFrame[COMPONENT_Y] / (double) getNumPic());
      addField("U-MSE      ", "%-10.4lf ", m_mseYuvFrame[COMPONENT_Cb] / (double) getNumPic(), withchroma);
      addField("V-MSE      ", "%-10.4lf ", m_mseYuvFrame[COMPONENT_Cr] / (double) getNumPic(), withchroma);
      addField("YUV-MSE    ", "%-10.4lf ", mseYUV, withchroma);
    }

    if (printMSEBasedSNR && !printRprPsnr)
    {
      addField("MSE-Y-PSNR   ", "%-8.4lf     ", mseBasedSNR[COMPONENT_Y]);
      addField("MSE-U-PSNR   ", "%-8.4lf     ", mseBasedSNR[COMPONENT_Cb], withchroma);
      addField("MSE-V-PSNR   ", "%-8.4lf     ", mseBasedSNR[COMPONENT_Cr], withchroma);
      addField("MSE-YUV-PSNR ", "%-8.4lf     ", psnrYUV, withchroma);
    }
    if (printRprPsnr)
    {
      addField("Y-PSNR1  ", "%-8.4lf ", mseBasedSNR[COMPONENT_Y]);
      addField("U-PSNR1  ", "%-8.4lf ", mseBasedSNR[COMPONENT_Cb], withchroma);
      addField("V-PSNR1  ", "%-8.4lf ", mseBasedSNR[COMPONENT_Cr], withchroma);
      addField("Y-PSNR2  ", "%-8.4lf ", m_upscaledPSNR[COMPONENT_Y] / (double) getNumPic());
      addField("U-PSNR2  ", "%-8.4lf ", m_upscaledPSNR[COMPONENT_Cb] / (double) getNumPic(), withchroma);
      addField("V-PSNR2  ", "%-8.4lf ", m_upscaledPSNR[COMPONENT_Cr] / (double) getNumPic(), withchroma);
      addField("Y-MS-SSIM2  ", "%-11.7lf ", m_upscaledMsssim[COMPONENT_Y] / (double) getNumPic());
      addField("U-MS-SSIM2  ", "%-11.7lf ", m_upscaledMsssim[COMPONENT_Cb] / (double) getNumPic(), withchroma);
      addField("V-MS-SSIM2  ", "%-11.7lf ", m_upscaledMsssim[COMPONENT_Cr] / (double) getNumPic(), withchroma);
    }
    header=headeross.str();
    metrics=metricoss.str();
  }

  void printSummary(const ChromaFormat chFmt, const bool printSequenceMSE, const bool printHexPsnr,
                    const BitDepths &bitDepths, const std::string &sFilename)
  {
    FILE* pFile = fopen (sFilename.c_str(), "at");

    double dFps     = m_frameRate.getFloatVal();
    double dScale   = dFps / 1000 / (double) m_picCount;
    switch (chFmt)
    {
    case ChromaFormat::_400:
      fprintf(pFile, "%f\t %f\n", getBits() * dScale, getPsnr(COMPONENT_Y) / (double) getNumPic());
      break;
    case ChromaFormat::_420:
    case ChromaFormat::_422:
    case ChromaFormat::_444:
      {
        double PSNRyuv = MAX_DOUBLE;
        double mseYuv  = MAX_DOUBLE;

        calculateCombinedValues(chFmt, PSNRyuv, mseYuv, bitDepths);

        fprintf(pFile, "%f\t %f\t %f\t %f\t %f", getBits() * dScale, getPsnr(COMPONENT_Y) / (double) getNumPic(),
                getPsnr(COMPONENT_Cb) / (double) getNumPic(), getPsnr(COMPONENT_Cr) / (double) getNumPic(), PSNRyuv);

        if (printSequenceMSE)
        {
          fprintf(pFile, "\t %f\t %f\t %f\t %f\n", m_mseYuvFrame[COMPONENT_Y] / (double) getNumPic(),
                  m_mseYuvFrame[COMPONENT_Cb] / (double) getNumPic(),
                  m_mseYuvFrame[COMPONENT_Cr] / (double) getNumPic(), mseYuv);
        }
        else
        {
          fprintf(pFile, "\n");
        }

        break;
      }

    default:
      THROW("Unknown format during print out");
      break;
    }

    fclose(pFile);
  }
};

extern Analyze m_gcAnalyzeAll;
extern Analyze m_gcAnalyzeI;
extern Analyze m_gcAnalyzeP;
extern Analyze m_gcAnalyzeB;
#if WCG_WPSNR
extern Analyze m_gcAnalyzeWPSNR;
#endif
extern Analyze m_gcAnalyzeAllField;

#endif   // __ANALYZE__
