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

/** \file     TEncAnalyze.h
    \brief    encoder analyzer class (header)
*/

#ifndef __TENCANALYZE__
#define __TENCANALYZE__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include "TLibCommon/CommonDef.h"
#include "TLibCommon/TComChromaFormat.h"
#include "math.h"
#if EXTENSION_360_VIDEO
#include "TAppEncHelper360/TExt360EncAnalyze.h"
#endif

//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// encoder analyzer class
class TEncAnalyze
{
public:
  struct OutputLogControl
  {
    Bool printMSEBasedSNR;
    Bool printSequenceMSE;
    Bool printFrameMSE;
    Bool printMSSSIM;
    Bool printXPSNR;
    Bool printHexPerPOCPSNRs;
  };

  struct ResultData
  {
    ResultData () : bits(0)
      , xpsnr(0)
    {
      for(Int i=0; i<MAX_NUM_COMPONENT; i++)
      {
        psnr[i]=0;
        MSEyuvframe[i]=0;
        MSSSIM[i]=0;
      }
    }
    Double psnr[MAX_NUM_COMPONENT];
    Double bits;
    Double MSEyuvframe[MAX_NUM_COMPONENT];
    Double MSSSIM[MAX_NUM_COMPONENT];
    Double xpsnr;
  };

private:
  ResultData m_runningTotal;
  UInt      m_uiNumPic;
  Double    m_dFrmRate; //--CFG_KDY

#if EXTENSION_360_VIDEO
  TExt360EncAnalyze m_ext360;
#endif

public:
  virtual ~TEncAnalyze()  {}
  TEncAnalyze() { clear(); }

  Void  addResult( const ResultData &result)
  {
    m_runningTotal.bits  += result.bits;
    for(UInt i=0; i<MAX_NUM_COMPONENT; i++)
    {
      m_runningTotal.psnr[i] += result.psnr[i];
      m_runningTotal.MSEyuvframe[i] += result.MSEyuvframe[i];
      m_runningTotal.MSSSIM[i] += result.MSSSIM[i];
    }

    m_runningTotal.xpsnr += result.xpsnr;
    m_uiNumPic++;
  }

  Double  getPsnr(ComponentID compID) const { return  m_runningTotal.psnr[compID];  }
  Double  getMsssim(ComponentID compID) const { return  m_runningTotal.MSSSIM[compID];  }
  Double  getxPSNR()                  const { return m_runningTotal.xpsnr;}
  Double  getBits()                   const { return m_runningTotal.bits;   }
  Void    setBits(Double numBits)           { m_runningTotal.bits=numBits; }
  UInt    getNumPic()                 const { return  m_uiNumPic;   }
#if EXTENSION_360_VIDEO
  TExt360EncAnalyze& getExt360Info() { return m_ext360; }
#endif

  Void    setFrmRate  (Double dFrameRate) { m_dFrmRate = dFrameRate; } //--CFG_KDY
  Void    clear()
  {
    m_runningTotal=ResultData();
    m_uiNumPic = 0;
#if EXTENSION_360_VIDEO
    m_ext360.clear();
#endif
  }


  Void calculateCombinedValues(const ChromaFormat chFmt, Double &PSNRyuv, Double &MSEyuv, const BitDepths &bitDepths)
  {
    MSEyuv    = 0;
    Int scale = 0;

    Int maximumBitDepth = bitDepths.recon[CHANNEL_TYPE_LUMA];
    for (UInt channelTypeIndex = 1; channelTypeIndex < MAX_NUM_CHANNEL_TYPE; channelTypeIndex++)
    {
      if (bitDepths.recon[channelTypeIndex] > maximumBitDepth)
      {
        maximumBitDepth = bitDepths.recon[channelTypeIndex];
      }
    }

    const UInt maxval                = 255 << (maximumBitDepth - 8);
    const UInt numberValidComponents = getNumberValidComponents(chFmt);

    for (UInt comp=0; comp<numberValidComponents; comp++)
    {
      const ComponentID compID        = ComponentID(comp);
      const UInt        csx           = getComponentScaleX(compID, chFmt);
      const UInt        csy           = getComponentScaleY(compID, chFmt);
      const Int         scaleChan     = (4>>(csx+csy));
      const UInt        bitDepthShift = 2 * (maximumBitDepth - bitDepths.recon[toChannelType(compID)]); //*2 because this is a squared number

      const Double      channelMSE    = (m_runningTotal.MSEyuvframe[compID] * Double(1 << bitDepthShift)) / Double(getNumPic());

      scale  += scaleChan;
      MSEyuv += scaleChan * channelMSE;
    }

    MSEyuv /= Double(scale);  // i.e. divide by 6 for 4:2:0, 8 for 4:2:2 etc.
    PSNRyuv = (MSEyuv==0 ? 999.99 : 10*log10((maxval*maxval)/MSEyuv));
  }

  Void    printOut ( TChar cDelim, const ChromaFormat chFmt, const OutputLogControl &logctrl, const BitDepths &bitDepths )
  {
    Double dFps     =   m_dFrmRate; //--CFG_KDY
    Double dScale   = dFps / 1000 / (Double)m_uiNumPic;

    Double MSEBasedSNR[MAX_NUM_COMPONENT];
    if (logctrl.printMSEBasedSNR)
    {
      for (UInt componentIndex = 0; componentIndex < MAX_NUM_COMPONENT; componentIndex++)
      {
        const ComponentID compID = ComponentID(componentIndex);

        if (getNumPic() == 0)
        {
          MSEBasedSNR[compID] = 0 * dScale; // this is the same calculation that will be evaluated for any other statistic when there are no frames (it should result in NaN). We use it here so all the output is consistent.
        }
        else
        {
          //NOTE: this is not the true maximum value for any bitDepth other than 8. It comes from the original HM PSNR calculation
          const UInt maxval = 255 << (bitDepths.recon[toChannelType(compID)] - 8);
          const Double MSE = m_runningTotal.MSEyuvframe[compID];

          MSEBasedSNR[compID] = (MSE == 0) ? 999.99 : (10 * log10((maxval * maxval) / (MSE / (Double)getNumPic())));
        }
      }
    }

    switch (chFmt)
    {
      case CHROMA_400:
        if (logctrl.printMSEBasedSNR)
        {
          printf( "         " );
        }

        printf( "\tTotal Frames |   "   "Bitrate     "  "Y-PSNR    " );

        if (logctrl.printMSSSIM)
        {
          printf( "  Y-MS-SSIM  ");
        }

        if (logctrl.printXPSNR)
        {
          printf( "    xPSNR  ");
        }

        if (logctrl.printSequenceMSE)
        {
          printf( "   Y-MSE    \n" );
        }
        else
        {
          printf("\n");
        }


        if (logctrl.printMSEBasedSNR)
        {
          printf( "Average: ");
        }

        printf( "\t %8d    %c "          "%12.4lf  "    "%8.4lf  ",
                 getNumPic(), cDelim,
                 getBits() * dScale,
                 getPsnr(COMPONENT_Y) / (Double)getNumPic() );

        if (logctrl.printMSSSIM)
        {
          printf("   %8.6lf  ", getMsssim(COMPONENT_Y) / (Double)getNumPic());
        }
        if(logctrl.printXPSNR)
        {
          printf(" %8.4lf  ",
                 getxPSNR() / (Double)getNumPic());
        }

        if (logctrl.printSequenceMSE)
        {
          printf( "  %8.4lf  \n", m_runningTotal.MSEyuvframe[COMPONENT_Y ] / (Double)getNumPic() );
        }
        else
        {
          printf("\n");
        }

        if (logctrl.printMSEBasedSNR)
        {
          printf( "From MSE:\t %8d    %c "          "%12.4lf  "    "%8.4lf\n",
                 getNumPic(), cDelim,
                 getBits() * dScale,
                 MSEBasedSNR[COMPONENT_Y] );
        }
        break;

      case CHROMA_420:
      case CHROMA_422:
      case CHROMA_444:
        {
          Double PSNRyuv = MAX_DOUBLE;
          Double MSEyuv  = MAX_DOUBLE;
          
          calculateCombinedValues(chFmt, PSNRyuv, MSEyuv, bitDepths);

          if (logctrl.printMSEBasedSNR)
          {
            printf( "         " );
          }

          printf( "\tTotal Frames |   "   "Bitrate     "  "Y-PSNR    "  "U-PSNR    "  "V-PSNR    "  "YUV-PSNR  " );

          if (logctrl.printMSSSIM)
          {
            printf("  Y-MS-SSIM    " "U-MS-SSIM    " "V-MS-SSIM  ");
          }
          if (logctrl.printXPSNR)
          {
            printf( "    xPSNR  ");
          }

#if EXTENSION_360_VIDEO
            m_ext360.printHeader();
#endif
          if (logctrl.printSequenceMSE)
          {
            printf( "  Y-MSE     "  "U-MSE     "  "V-MSE     "  "YUV-MSE  \n" );
          }
          else
          {
            printf("\n");
          }

          if (logctrl.printMSEBasedSNR)
          {
            printf( "Average: ");
          }
            
          printf( "\t %8d    %c "          "%12.4lf  "    "%8.4lf  "   "%8.4lf  "    "%8.4lf  "   "%8.4lf  ",
                 getNumPic(), cDelim,
                 getBits() * dScale,
                 getPsnr(COMPONENT_Y) / (Double)getNumPic(),
                 getPsnr(COMPONENT_Cb) / (Double)getNumPic(),
                 getPsnr(COMPONENT_Cr) / (Double)getNumPic(),
                 PSNRyuv );

          if (logctrl.printMSSSIM)
          {
            printf("   %8.6lf     " "%8.6lf     " "%8.6lf  ",
                   getMsssim(COMPONENT_Y) / (Double)getNumPic(),
                   getMsssim(COMPONENT_Cb) / (Double)getNumPic(),
                   getMsssim(COMPONENT_Cr) / (Double)getNumPic());
          }

          if(logctrl.printXPSNR)
          {
            printf(" %8.4lf  ",
                   getxPSNR() / (Double)getNumPic());
          }

#if EXTENSION_360_VIDEO
          m_ext360.printPSNRs(getNumPic());
#endif

          if (logctrl.printSequenceMSE)
          {
            printf( " %8.4lf  "   "%8.4lf  "    "%8.4lf  "   "%8.4lf  \n",
                   m_runningTotal.MSEyuvframe[COMPONENT_Y ] / (Double)getNumPic(),
                   m_runningTotal.MSEyuvframe[COMPONENT_Cb] / (Double)getNumPic(),
                   m_runningTotal.MSEyuvframe[COMPONENT_Cr] / (Double)getNumPic(),
                   MSEyuv );
          }
          else
          {
            printf("\n");
          }

          if (logctrl.printMSEBasedSNR)
          {
            printf( "From MSE:\t %8d    %c "          "%12.4lf  "    "%8.4lf  "   "%8.4lf  "    "%8.4lf  "   "%8.4lf\n",
                   getNumPic(), cDelim,
                   getBits() * dScale,
                   MSEBasedSNR[COMPONENT_Y],
                   MSEBasedSNR[COMPONENT_Cb],
                   MSEBasedSNR[COMPONENT_Cr],
                   PSNRyuv );
          }
        }
        break;
      default:
        fprintf(stderr, "Unknown format during print out\n");
        exit(1);
        break;
    }
  }


  Void printSummary(const ChromaFormat chFmt, const OutputLogControl &logctrl, const BitDepths &bitDepths, const std::string &sFilename)
  {
    FILE* pFile = fopen (sFilename.c_str(), "at");

    Double dFps     =   m_dFrmRate; //--CFG_KDY
    Double dScale   = dFps / 1000 / (Double)m_uiNumPic;
    switch (chFmt)
    {
      case CHROMA_400:
        fprintf(pFile, "%f\t %f\n",
            getBits() * dScale,
            getPsnr(COMPONENT_Y) / (Double)getNumPic() );
        break;
      case CHROMA_420:
      case CHROMA_422:
      case CHROMA_444:
        {
          Double PSNRyuv = MAX_DOUBLE;
          Double MSEyuv  = MAX_DOUBLE;
          
          calculateCombinedValues(chFmt, PSNRyuv, MSEyuv, bitDepths);

          fprintf(pFile, "%f\t %f\t %f\t %f\t %f",
              getBits() * dScale,
              getPsnr(COMPONENT_Y) / (Double)getNumPic(),
              getPsnr(COMPONENT_Cb) / (Double)getNumPic(),
              getPsnr(COMPONENT_Cr) / (Double)getNumPic(),
              PSNRyuv );

          if (logctrl.printSequenceMSE)
          {
            fprintf(pFile, "\t %f\t %f\t %f\t %f\n",
                m_runningTotal.MSEyuvframe[COMPONENT_Y ] / (Double)getNumPic(),
                m_runningTotal.MSEyuvframe[COMPONENT_Cb] / (Double)getNumPic(),
                m_runningTotal.MSEyuvframe[COMPONENT_Cr] / (Double)getNumPic(),
                MSEyuv );
          }
          else
          {
            fprintf(pFile, "\n");
          }

          break;
        }

      default:
          fprintf(stderr, "Unknown format during print out\n");
          exit(1);
          break;
    }

    fclose(pFile);
  }
};

extern TEncAnalyze             m_gcAnalyzeAll;
extern TEncAnalyze             m_gcAnalyzeI;
extern TEncAnalyze             m_gcAnalyzeP;
extern TEncAnalyze             m_gcAnalyzeB;

extern TEncAnalyze             m_gcAnalyzeAll_in;

//! \}

#endif // !defined(AFX_TENCANALYZE_H__C79BCAA2_6AC8_4175_A0FE_CF02F5829233__INCLUDED_)
