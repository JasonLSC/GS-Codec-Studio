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

/** \file     RateCtrl.cpp
    \brief    Rate control manager class
*/
#include "RateCtrl.h"
#include "../CommonLib/ChromaFormat.h"

#include <cmath>

#define LAMBDA_PREC                                           1000000

//sequence level
EncRCSeq::EncRCSeq()
{
  m_totalFrames         = 0;
  m_targetRate          = 0;
  m_targetBits          = 0;
  m_GOPSize             = 0;
  m_intraPeriod         = 0;
  m_picWidth            = 0;
  m_picHeight           = 0;
  m_LCUWidth            = 0;
  m_LCUHeight           = 0;
  m_numberOfLevel       = 0;
  m_numberOfLCU         = 0;
  m_averageBits         = 0;
  m_bitsRatio           = nullptr;
  m_GOPID2Level         = nullptr;
  m_picPara             = nullptr;
  m_LCUPara             = nullptr;
  m_numberOfPixel       = 0;
  m_framesLeft          = 0;
  m_bitsLeft            = 0;
  m_useLCUSeparateModel = false;
  m_adaptiveBit         = 0;
  m_lastLambda          = 0.0;
  m_bitDepth          = 0;
}

EncRCSeq::~EncRCSeq()
{
  destroy();
}

void EncRCSeq::create(int totalFrames, int targetBitrate, const Fraction& frameRate, int GOPSize, int intraPeriod,
                      int picWidth, int picHeight, int LCUWidth, int LCUHeight, int numberOfLevel,
                      bool useLCUSeparateModel, int adaptiveBit)
{
  destroy();
  m_totalFrames         = totalFrames;
  m_targetRate          = targetBitrate;
  m_frameRate           = frameRate;
  m_GOPSize             = GOPSize;
  m_intraPeriod         = intraPeriod;
  m_picWidth            = picWidth;
  m_picHeight           = picHeight;
  m_LCUWidth            = LCUWidth;
  m_LCUHeight           = LCUHeight;
  m_numberOfLevel       = numberOfLevel;
  m_useLCUSeparateModel = useLCUSeparateModel;

  m_numberOfPixel   = m_picWidth * m_picHeight;
  m_targetBits =
    (int64_t) m_totalFrames * (int64_t) m_targetRate * (int64_t) m_frameRate.den / (int64_t) m_frameRate.num;
  m_seqTargetBpp = (double) m_targetRate / m_frameRate.getFloatVal() / (double) m_numberOfPixel;
  if ( m_seqTargetBpp < 0.03 )
  {
    m_alphaUpdate = 0.01;
    m_betaUpdate  = 0.005;
  }
  else if ( m_seqTargetBpp < 0.08 )
  {
    m_alphaUpdate = 0.05;
    m_betaUpdate  = 0.025;
  }
  else if ( m_seqTargetBpp < 0.2 )
  {
    m_alphaUpdate = 0.1;
    m_betaUpdate  = 0.05;
  }
  else if ( m_seqTargetBpp < 0.5 )
  {
    m_alphaUpdate = 0.2;
    m_betaUpdate  = 0.1;
  }
  else
  {
    m_alphaUpdate = 0.4;
    m_betaUpdate  = 0.2;
  }

  m_averageBits     = (int)(m_targetBits / totalFrames);
  int picWidthInBU  = ( m_picWidth  % m_LCUWidth  ) == 0 ? m_picWidth  / m_LCUWidth  : m_picWidth  / m_LCUWidth  + 1;
  int picHeightInBU = ( m_picHeight % m_LCUHeight ) == 0 ? m_picHeight / m_LCUHeight : m_picHeight / m_LCUHeight + 1;
  m_numberOfLCU     = picWidthInBU * picHeightInBU;

  m_bitsRatio   = new int[m_GOPSize];
  for ( int i=0; i<m_GOPSize; i++ )
  {
    m_bitsRatio[i] = 1;
  }

  m_GOPID2Level = new int[m_GOPSize];
  for ( int i=0; i<m_GOPSize; i++ )
  {
    m_GOPID2Level[i] = 1;
  }

  m_picPara = new TRCParameter[m_numberOfLevel];
  for ( int i=0; i<m_numberOfLevel; i++ )
  {
    m_picPara[i].m_alpha = 0.0;
    m_picPara[i].m_beta  = 0.0;
    m_picPara[i].m_validPix = -1;
    m_picPara[i].m_skipRatio = 0.0;
  }

  if ( m_useLCUSeparateModel )
  {
    m_LCUPara = new TRCParameter*[m_numberOfLevel];
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
      m_LCUPara[i] = new TRCParameter[m_numberOfLCU];
      for ( int j=0; j<m_numberOfLCU; j++)
      {
        m_LCUPara[i][j].m_alpha = 0.0;
        m_LCUPara[i][j].m_beta  = 0.0;
        m_LCUPara[i][j].m_validPix = -1;
        m_LCUPara[i][j].m_skipRatio = 0.0;
      }
    }
  }

  m_framesLeft = m_totalFrames;
  m_bitsLeft   = m_targetBits;
  m_adaptiveBit = adaptiveBit;
  m_lastLambda = 0.0;
}

void EncRCSeq::destroy()
{
  if (m_bitsRatio != nullptr)
  {
    delete[] m_bitsRatio;
    m_bitsRatio = nullptr;
  }

  if (m_GOPID2Level != nullptr)
  {
    delete[] m_GOPID2Level;
    m_GOPID2Level = nullptr;
  }

  if (m_picPara != nullptr)
  {
    delete[] m_picPara;
    m_picPara = nullptr;
  }

  if (m_LCUPara != nullptr)
  {
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
      delete[] m_LCUPara[i];
    }
    delete[] m_LCUPara;
    m_LCUPara = nullptr;
  }
}

void EncRCSeq::initBitsRatio( int bitsRatio[])
{
  for (int i=0; i<m_GOPSize; i++)
  {
    m_bitsRatio[i] = bitsRatio[i];
  }
}

void EncRCSeq::initGOPID2Level( int GOPID2Level[] )
{
  for ( int i=0; i<m_GOPSize; i++ )
  {
    m_GOPID2Level[i] = GOPID2Level[i];
  }
}

void EncRCSeq::initPicPara( TRCParameter* picPara )
{
  CHECK(m_picPara == nullptr, "Object does not exist");

  if (picPara == nullptr)
  {
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
      if (i>0)
      {
        int bitdepth_luma_scale =
          2
          * (m_bitDepth - 8
            - DISTORTION_PRECISION_ADJUSTMENT(m_bitDepth));
        m_picPara[i].m_alpha = 3.2003 * pow(2.0, bitdepth_luma_scale);
        m_picPara[i].m_beta = -1.367;
      }
      else
      {
        int bitdepth_luma_scale =
          2
          * (m_bitDepth - 8
            - DISTORTION_PRECISION_ADJUSTMENT(m_bitDepth));
        m_picPara[i].m_alpha = pow(2.0, bitdepth_luma_scale) * ALPHA;
        m_picPara[i].m_beta = BETA2;
      }
    }
  }
  else
  {
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
      m_picPara[i] = picPara[i];
    }
  }
}

void EncRCSeq::initLCUPara( TRCParameter** LCUPara )
{
  if (m_LCUPara == nullptr)
  {
    return;
  }
  if (LCUPara == nullptr)
  {
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
      for ( int j=0; j<m_numberOfLCU; j++)
      {
        m_LCUPara[i][j].m_alpha = m_picPara[i].m_alpha;
        m_LCUPara[i][j].m_beta  = m_picPara[i].m_beta;
      }
    }
  }
  else
  {
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
      for ( int j=0; j<m_numberOfLCU; j++)
      {
        m_LCUPara[i][j] = LCUPara[i][j];
      }
    }
  }
}

void EncRCSeq::updateAfterPic ( int bits )
{
  m_bitsLeft -= bits;
  m_framesLeft--;
}

void EncRCSeq::setAllBitRatio( double basicLambda, double* equaCoeffA, double* equaCoeffB )
{
  int* bitsRatio = new int[m_GOPSize];
  for ( int i=0; i<m_GOPSize; i++ )
  {
    bitsRatio[i] = (int)(equaCoeffA[i] * pow(basicLambda, equaCoeffB[i]) * (double)getPicPara(getGOPID2Level(i)).m_validPix);
  }
  initBitsRatio( bitsRatio );
  delete[] bitsRatio;
}

//GOP level
EncRCGOP::EncRCGOP()
{
  m_encRCSeq          = nullptr;
  m_picTargetBitInGOP = nullptr;
  m_numPic     = 0;
  m_targetBits = 0;
  m_picLeft    = 0;
  m_bitsLeft   = 0;
  m_minEstLambda = 0.0;
  m_maxEstLambda = 0.0;
}

EncRCGOP::~EncRCGOP()
{
  destroy();
}

void EncRCGOP::create(EncRCSeq *encRCSeq, int numPic, bool useAdaptiveBitsRatio)
{
  destroy();
  int targetBits = xEstGOPTargetBits( encRCSeq, numPic );
  int bitdepth_luma_scale =
    2 * (encRCSeq->getbitDepth() - 8
      - DISTORTION_PRECISION_ADJUSTMENT(encRCSeq->getbitDepth()));
  m_minEstLambda = 0.1;
  m_maxEstLambda = 10000.0 * pow(2.0, bitdepth_luma_scale);

  if (useAdaptiveBitsRatio)
  {
    double targetBpp = (double)targetBits / encRCSeq->getNumPixel();
    double basicLambda = 0.0;
    double* lambdaRatio = new double[encRCSeq->getGOPSize()];
    double* equaCoeffA = new double[encRCSeq->getGOPSize()];
    double* equaCoeffB = new double[encRCSeq->getGOPSize()];

    if ( encRCSeq->getAdaptiveBits() == 1 && encRCSeq->getGOPSize() == 4)   // for GOP size =4, low delay case
    {
      if ( encRCSeq->getLastLambda() < 120.0 )
      {
        lambdaRatio[1] = 0.725 * log( encRCSeq->getLastLambda() ) + 0.5793;
        lambdaRatio[0] = 1.3 * lambdaRatio[1];
        lambdaRatio[2] = 1.3 * lambdaRatio[1];
        lambdaRatio[3] = 1.0;
      }
      else
      {
        lambdaRatio[0] = 5.0;
        lambdaRatio[1] = 4.0;
        lambdaRatio[2] = 5.0;
        lambdaRatio[3] = 1.0;
      }
    }
    else if (encRCSeq->getAdaptiveBits() == 1 && encRCSeq->getGOPSize() == 8) // for GOP size =8, low delay case
    {
      if (encRCSeq->getLastLambda() < 120.0)
      {
        lambdaRatio[1] = 0.725 * log(encRCSeq->getLastLambda()) + 0.5793;
        lambdaRatio[3] = 0.725 * log(encRCSeq->getLastLambda()) + 0.5793;
        lambdaRatio[5] = 0.725 * log(encRCSeq->getLastLambda()) + 0.5793;
        lambdaRatio[0] = 1.3 * lambdaRatio[1];
        lambdaRatio[2] = 1.3 * lambdaRatio[1];
        lambdaRatio[4] = 1.3 * lambdaRatio[1];
        lambdaRatio[6] = 1.3 * lambdaRatio[1];
        lambdaRatio[7] = 1.0;
      }
      else
      {
        lambdaRatio[0] = 5.0;
        lambdaRatio[1] = 4.0;
        lambdaRatio[2] = 5.0;
        lambdaRatio[3] = 4.0;
        lambdaRatio[4] = 5.0;
        lambdaRatio[5] = 4.0;
        lambdaRatio[6] = 5.0;
        lambdaRatio[7] = 1.0;
      }
    }
    else if ( encRCSeq->getAdaptiveBits() == 2 && encRCSeq->getGOPSize() == 8 )  // for GOP size = 8, random access case
    {
      if ( encRCSeq->getLastLambda() < 90.0 )
      {
        lambdaRatio[0] = 1.0;
        lambdaRatio[1] = 0.725 * log( encRCSeq->getLastLambda() ) + 0.7963;
        lambdaRatio[2] = 1.3 * lambdaRatio[1];
        lambdaRatio[3] = 3.25 * lambdaRatio[1];
        lambdaRatio[4] = 3.25 * lambdaRatio[1];
        lambdaRatio[5] = 1.3  * lambdaRatio[1];
        lambdaRatio[6] = 3.25 * lambdaRatio[1];
        lambdaRatio[7] = 3.25 * lambdaRatio[1];
      }
      else
      {
        lambdaRatio[0] = 1.0;
        lambdaRatio[1] = 4.0;
        lambdaRatio[2] = 5.0;
        lambdaRatio[3] = 12.3;
        lambdaRatio[4] = 12.3;
        lambdaRatio[5] = 5.0;
        lambdaRatio[6] = 12.3;
        lambdaRatio[7] = 12.3;
      }
    }
    else if (encRCSeq->getAdaptiveBits() == 2 && encRCSeq->getGOPSize() == 16 )  // for GOP size = 16, random access case
    {
      const double qdfParaLev2A = 0.5847;
      const double qdfParaLev2B = -0.0782;
      const double qdfParaLev3A = 0.5468;
      const double qdfParaLev3B = -0.1364;
      const double qdfParaLev4A = 0.6539;
      const double qdfParaLev4B = -0.203;
      const double qdfParaLev5A = 0.8623;
      const double qdfParaLev5B = -0.4676;
      double       qdfLev1Lev2  = Clip3(0.12, 0.9, qdfParaLev2A * encRCSeq->getPicPara(2).m_skipRatio + qdfParaLev2B);
      double       qdfLev1Lev3  = Clip3(0.13, 0.9, qdfParaLev3A * encRCSeq->getPicPara(3).m_skipRatio + qdfParaLev3B);
      double       qdfLev1Lev4  = Clip3(0.15, 0.9, qdfParaLev4A * encRCSeq->getPicPara(4).m_skipRatio + qdfParaLev4B);
      double       qdfLev1Lev5  = Clip3(0.20, 0.9, qdfParaLev5A * encRCSeq->getPicPara(5).m_skipRatio + qdfParaLev5B);
      double       qdfLev2Lev3  = Clip3(0.09, 0.9, qdfLev1Lev3 * (1 - qdfLev1Lev2));
      double       qdfLev2Lev4  = Clip3(0.12, 0.9, qdfLev1Lev4 * (1 - qdfLev1Lev2));
      double       qdfLev2Lev5  = Clip3(0.14, 0.9, qdfLev1Lev5 * (1 - qdfLev1Lev2));
      double       qdfLev3Lev4  = Clip3(0.06, 0.9, qdfLev1Lev4 * (1 - qdfLev1Lev3));
      double       qdfLev3Lev5  = Clip3(0.09, 0.9, qdfLev1Lev5 * (1 - qdfLev1Lev3));
      double       qdfLev4Lev5  = Clip3(0.10, 0.9, qdfLev1Lev5 * (1 - qdfLev1Lev4));

      double lambdaLev1 = 1 / (1 + 2 * (qdfLev1Lev2 + 2 * qdfLev1Lev3 + 4 * qdfLev1Lev4 + 8 * qdfLev1Lev5));
      double lambdaLev2 = 1 / (1 + (3 * qdfLev2Lev3 + 5 * qdfLev2Lev4 + 8 * qdfLev2Lev5));
      double lambdaLev3 = 1 / (1 + 2 * qdfLev3Lev4 + 4 * qdfLev3Lev5);
      double lambdaLev4 = 1 / (1 + 2 * qdfLev4Lev5);
      double lambdaLev5 = 1 / (1.0);

      lambdaRatio[0]  = 1.0;
      lambdaRatio[1]  = lambdaLev2 / lambdaLev1;
      lambdaRatio[2]  = lambdaLev3 / lambdaLev1;
      lambdaRatio[3]  = lambdaLev4 / lambdaLev1;
      lambdaRatio[4]  = lambdaLev5 / lambdaLev1;
      lambdaRatio[5]  = lambdaLev5 / lambdaLev1;
      lambdaRatio[6]  = lambdaLev4 / lambdaLev1;
      lambdaRatio[7]  = lambdaLev5 / lambdaLev1;
      lambdaRatio[8]  = lambdaLev5 / lambdaLev1;
      lambdaRatio[9]  = lambdaLev3 / lambdaLev1;
      lambdaRatio[10] = lambdaLev4 / lambdaLev1;
      lambdaRatio[11] = lambdaLev5 / lambdaLev1;
      lambdaRatio[12] = lambdaLev5 / lambdaLev1;
      lambdaRatio[13] = lambdaLev4 / lambdaLev1;
      lambdaRatio[14] = lambdaLev5 / lambdaLev1;
      lambdaRatio[15] = lambdaLev5 / lambdaLev1;
    }
    else if (encRCSeq->getAdaptiveBits() == 2 && encRCSeq->getGOPSize() == 32 )  // for GOP size = 32, random access case
    {
      const double qdfParaLev2A = 0.7534;
      const double qdfParaLev2B = -0.0303;
      const double qdfParaLev3A = 0.7044;
      const double qdfParaLev3B = -0.0445;
      const double qdfParaLev4A = 0.7084;
      const double qdfParaLev4B = -0.1401;
      const double qdfParaLev5A = 0.8844;
      const double qdfParaLev5B = -0.3676;
      const double qdfParaLev6A = 1.2336;
      const double qdfParaLev6B = -0.7511;

      double qdfLev1Lev2 = Clip3(0.12, 0.9, qdfParaLev2A * encRCSeq->getPicPara(2).m_skipRatio + qdfParaLev2B);
      double qdfLev1Lev3 = Clip3(0.13, 0.9, qdfParaLev3A * encRCSeq->getPicPara(3).m_skipRatio + qdfParaLev3B);
      double qdfLev1Lev4 = Clip3(0.15, 0.9, qdfParaLev4A * encRCSeq->getPicPara(4).m_skipRatio + qdfParaLev4B);
      double qdfLev1Lev5 = Clip3(0.20, 0.9, qdfParaLev5A * encRCSeq->getPicPara(5).m_skipRatio + qdfParaLev5B);
      double qdfLev1Lev6 = Clip3(0.25, 0.9, qdfParaLev6A * encRCSeq->getPicPara(6).m_skipRatio + qdfParaLev6B);
      double qdfLev2Lev3 = Clip3(0.09, 0.9, qdfLev1Lev3 * (1 - qdfLev1Lev2));
      double qdfLev2Lev4 = Clip3(0.12, 0.9, qdfLev1Lev4 * (1 - qdfLev1Lev2));
      double qdfLev2Lev5 = Clip3(0.14, 0.9, qdfLev1Lev5 * (1 - qdfLev1Lev2));
      double qdfLev2Lev6 = Clip3(0.16, 0.9, qdfLev1Lev6 * (1 - qdfLev1Lev2));
      double qdfLev3Lev4 = Clip3(0.06, 0.9, qdfLev1Lev4 * (1 - qdfLev1Lev3));
      double qdfLev3Lev5 = Clip3(0.09, 0.9, qdfLev1Lev5 * (1 - qdfLev1Lev3));
      double qdfLev3Lev6 = Clip3(0.10, 0.9, qdfLev1Lev6 * (1 - qdfLev1Lev3));
      double qdfLev4Lev5 = Clip3(0.10, 0.9, qdfLev1Lev5 * (1 - qdfLev1Lev4));
      double qdfLev4Lev6 = Clip3(0.10, 0.9, qdfLev1Lev6 * (1 - qdfLev1Lev4));
      double qdfLev5Lev6 = Clip3(0.12, 0.9, qdfLev1Lev6 * (1 - qdfLev1Lev5));

      double lambdaLev1 =
        1 / (1 + 2 * qdfLev1Lev2 + 4 * qdfLev1Lev3 + 6 * qdfLev1Lev4 + 8 * qdfLev1Lev5 + 10 * qdfLev1Lev6);
      double lambdaLev2 = 1 / (1 + 3 * qdfLev2Lev3 + 5 * qdfLev2Lev4 + 8 * qdfLev2Lev5 + 9 * qdfLev2Lev6);
      double lambdaLev3 = 1 / (1 + 2 * qdfLev3Lev4 + 4 * qdfLev3Lev5 + 6 * qdfLev3Lev6);
      double lambdaLev4 = 1 / (1 + 2 * qdfLev4Lev5 + 4 * qdfLev4Lev6);
      double lambdaLev5 = 1 / (1 + 2 * qdfLev5Lev6);
      double lambdaLev6 = 1 / (1.0);

      lambdaRatio[0]  = 1.0;
      lambdaRatio[1]  = lambdaLev2 / lambdaLev1;
      lambdaRatio[2]  = lambdaLev3 / lambdaLev1;
      lambdaRatio[3]  = lambdaLev4 / lambdaLev1;
      lambdaRatio[4]  = lambdaLev5 / lambdaLev1;
      lambdaRatio[5]  = lambdaLev6 / lambdaLev1;
      lambdaRatio[6]  = lambdaLev6 / lambdaLev1;
      lambdaRatio[7]  = lambdaLev5 / lambdaLev1;
      lambdaRatio[8]  = lambdaLev6 / lambdaLev1;
      lambdaRatio[9]  = lambdaLev6 / lambdaLev1;
      lambdaRatio[10] = lambdaLev4 / lambdaLev1;
      lambdaRatio[11] = lambdaLev5 / lambdaLev1;
      lambdaRatio[12] = lambdaLev6 / lambdaLev1;
      lambdaRatio[13] = lambdaLev6 / lambdaLev1;
      lambdaRatio[14] = lambdaLev5 / lambdaLev1;
      lambdaRatio[15] = lambdaLev6 / lambdaLev1;
      lambdaRatio[16] = lambdaLev6 / lambdaLev1;
      lambdaRatio[17] = lambdaLev3 / lambdaLev1;
      lambdaRatio[18] = lambdaLev4 / lambdaLev1;
      lambdaRatio[19] = lambdaLev5 / lambdaLev1;
      lambdaRatio[20] = lambdaLev6 / lambdaLev1;
      lambdaRatio[21] = lambdaLev6 / lambdaLev1;
      lambdaRatio[22] = lambdaLev5 / lambdaLev1;
      lambdaRatio[23] = lambdaLev6 / lambdaLev1;
      lambdaRatio[24] = lambdaLev6 / lambdaLev1;
      lambdaRatio[25] = lambdaLev4 / lambdaLev1;
      lambdaRatio[26] = lambdaLev5 / lambdaLev1;
      lambdaRatio[27] = lambdaLev6 / lambdaLev1;
      lambdaRatio[28] = lambdaLev6 / lambdaLev1;
      lambdaRatio[29] = lambdaLev5 / lambdaLev1;
      lambdaRatio[30] = lambdaLev6 / lambdaLev1;
      lambdaRatio[31] = lambdaLev6 / lambdaLev1;
    }
    else
    {
      msg( WARNING, "Warning: Current rate control does not support this coding configuration." );
    }

    xCalEquaCoeff( encRCSeq, lambdaRatio, equaCoeffA, equaCoeffB, encRCSeq->getGOPSize() );
    basicLambda = xSolveEqua(encRCSeq, targetBpp, equaCoeffA, equaCoeffB, encRCSeq->getGOPSize());
    encRCSeq->setAllBitRatio( basicLambda, equaCoeffA, equaCoeffB );

    delete []lambdaRatio;
    delete []equaCoeffA;
    delete []equaCoeffB;
  }

  m_picTargetBitInGOP = new int[numPic];
  int i;
  int totalPicRatio = 0;
  int currPicRatio = 0;
  for ( i=0; i<numPic; i++ )
  {
    totalPicRatio += encRCSeq->getBitRatio( i );
  }
  for ( i=0; i<numPic; i++ )
  {
    currPicRatio = encRCSeq->getBitRatio( i );
    m_picTargetBitInGOP[i] = (int)( ((double)targetBits) * currPicRatio / totalPicRatio );
  }

  m_encRCSeq    = encRCSeq;
  m_numPic       = numPic;
  m_targetBits   = targetBits;
  m_picLeft      = m_numPic;
  m_bitsLeft     = m_targetBits;

}

void EncRCGOP::xCalEquaCoeff( EncRCSeq* encRCSeq, double* lambdaRatio, double* equaCoeffA, double* equaCoeffB, int GOPSize )
{
  for ( int i=0; i<GOPSize; i++ )
  {
    int frameLevel = encRCSeq->getGOPID2Level(i);
    double alpha   = encRCSeq->getPicPara(frameLevel).m_alpha;
    double beta    = encRCSeq->getPicPara(frameLevel).m_beta;
    equaCoeffA[i] = pow( 1.0/alpha, 1.0/beta ) * pow( lambdaRatio[i], 1.0/beta );
    equaCoeffB[i] = 1.0/beta;
  }
}

double EncRCGOP::xSolveEqua(EncRCSeq* encRCSeq, double targetBpp, double* equaCoeffA, double* equaCoeffB, int GOPSize)
{
  double solution = 100.0;
  double minNumber = m_minEstLambda;
  double maxNumber = m_maxEstLambda;
  for ( int i=0; i<g_RCIterationNum; i++ )
  {
    double fx = 0.0;
    for ( int j=0; j<GOPSize; j++ )
    {
      double tmpBpp = equaCoeffA[j] * pow(solution, equaCoeffB[j]);
      double actualBpp = tmpBpp * (double)encRCSeq->getPicPara(encRCSeq->getGOPID2Level(j)).m_validPix / (double)encRCSeq->getNumPixel();
      fx += actualBpp;
    }

    if ( fabs( fx - targetBpp ) < 0.000001 )
    {
      break;
    }

    if ( fx > targetBpp )
    {
      minNumber = solution;
      solution = ( solution + maxNumber ) / 2.0;
    }
    else
    {
      maxNumber = solution;
      solution = ( solution + minNumber ) / 2.0;
    }
  }

  solution = Clip3(m_minEstLambda, m_maxEstLambda, solution);
  return solution;
}

void EncRCGOP::destroy()
{
  m_encRCSeq = nullptr;
  if (m_picTargetBitInGOP != nullptr)
  {
    delete[] m_picTargetBitInGOP;
    m_picTargetBitInGOP = nullptr;
  }
}

void EncRCGOP::updateAfterPicture( int bitsCost )
{
  m_bitsLeft -= bitsCost;
  m_picLeft--;
}

int EncRCGOP::xEstGOPTargetBits( EncRCSeq* encRCSeq, int GOPSize )
{
  int realInfluencePicture =
    std::min(g_RCSmoothWindowSizeAlpha * GOPSize / std::max(encRCSeq->getIntraPeriod(), 32) + g_RCSmoothWindowSizeBeta,
             encRCSeq->getFramesLeft());
  int averageTargetBitsPerPic = (int)( encRCSeq->getTargetBits() / encRCSeq->getTotalFrames() );
  int currentTargetBitsPerPic = (int)( ( encRCSeq->getBitsLeft() - averageTargetBitsPerPic * (encRCSeq->getFramesLeft() - realInfluencePicture) ) / realInfluencePicture );
  int targetBits = currentTargetBitsPerPic * GOPSize;

  if ( targetBits < 200 )
  {
    targetBits = 200;   // at least allocate 200 bits for one GOP
  }

  return targetBits;
}

//picture level
EncRCPic::EncRCPic()
{
  m_encRCSeq = nullptr;
  m_encRCGOP = nullptr;

  m_frameLevel    = 0;
  m_numberOfPixel = 0;
  m_numberOfLCU   = 0;
  m_targetBits    = 0;
  m_estHeaderBits = 0;
  m_estPicQP      = 0;
  m_estPicLambda  = 0.0;

  m_LCULeft       = 0;
  m_bitsLeft      = 0;
  m_pixelsLeft    = 0;

  m_LCUs                = nullptr;
  m_picActualHeaderBits = 0;
  m_picActualBits       = 0;
  m_picQP               = 0;
  m_picLambda           = 0.0;
  m_picMSE              = 0.0;
  m_validPixelsInPic    = 0;
}

EncRCPic::~EncRCPic()
{
  destroy();
}

int EncRCPic::xEstPicTargetBits( EncRCSeq* encRCSeq, EncRCGOP* encRCGOP )
{
  int targetBits        = 0;
  int GOPbitsLeft       = encRCGOP->getBitsLeft();

  int i;
  int currPicPosition = encRCGOP->getNumPic()-encRCGOP->getPicLeft();
  int currPicRatio    = encRCSeq->getBitRatio( currPicPosition );
  int totalPicRatio   = 0;
  for ( i=currPicPosition; i<encRCGOP->getNumPic(); i++ )
  {
    totalPicRatio += encRCSeq->getBitRatio( i );
  }

  targetBits  = int( ((double)GOPbitsLeft) * currPicRatio / totalPicRatio );

  if ( targetBits < 100 )
  {
    targetBits = 100;   // at least allocate 100 bits for one picture
  }

  if ( m_encRCSeq->getFramesLeft() > 16 )
  {
    targetBits = int( g_RCWeightPicRargetBitInBuffer * targetBits + g_RCWeightPicTargetBitInGOP * m_encRCGOP->getTargetBitInGOP( currPicPosition ) );
  }

  return targetBits;
}

int EncRCPic::xEstPicHeaderBits(std::list<EncRCPic *> &listPreviousPictures, int frameLevel)
{
  int numPreviousPics   = 0;
  int totalPreviousBits = 0;

  std::list<EncRCPic *>::iterator it;
  for ( it = listPreviousPictures.begin(); it != listPreviousPictures.end(); it++ )
  {
    if ( (*it)->getFrameLevel() == frameLevel )
    {
      totalPreviousBits += (*it)->getPicActualHeaderBits();
      numPreviousPics++;
    }
  }

  int estHeaderBits = 0;
  if ( numPreviousPics > 0 )
  {
    estHeaderBits = totalPreviousBits / numPreviousPics;
  }

  return estHeaderBits;
}

int EncRCPic::xEstPicLowerBound(EncRCSeq* encRCSeq, EncRCGOP* encRCGOP)
{
  int lowerBound = 0;
  int GOPbitsLeft = encRCGOP->getBitsLeft();

  const int nextPicPosition = (encRCGOP->getNumPic() - encRCGOP->getPicLeft() + 1) % encRCGOP->getNumPic();
  const int nextPicRatio = encRCSeq->getBitRatio(nextPicPosition);

  int totalPicRatio = 0;
  for (int i = nextPicPosition; i < encRCGOP->getNumPic(); i++)
  {
    totalPicRatio += encRCSeq->getBitRatio(i);
  }

  if (nextPicPosition == 0)
  {
    GOPbitsLeft = encRCGOP->getTargetBits();
  }
  else
  {
    GOPbitsLeft -= m_targetBits;
  }

  lowerBound = int(((double)GOPbitsLeft) * nextPicRatio / totalPicRatio);

  if (lowerBound < 100)
  {
    lowerBound = 100;   // at least allocate 100 bits for one picture
  }

  if (m_encRCSeq->getFramesLeft() > 16)
  {
    lowerBound = int(g_RCWeightPicRargetBitInBuffer * lowerBound + g_RCWeightPicTargetBitInGOP * m_encRCGOP->getTargetBitInGOP(nextPicPosition));
  }

  return lowerBound;
}

void EncRCPic::addToPictureLsit(std::list<EncRCPic *> &listPreviousPictures)
{
  if ( listPreviousPictures.size() > g_RCMaxPicListSize )
  {
    EncRCPic* p = listPreviousPictures.front();
    listPreviousPictures.pop_front();
    p->destroy();
    delete p;
  }

  listPreviousPictures.push_back( this );
}

void EncRCPic::create(EncRCSeq *encRCSeq, EncRCGOP *encRCGOP, int frameLevel,
                      std::list<EncRCPic *> &listPreviousPictures)
{
  destroy();
  m_encRCSeq = encRCSeq;
  m_encRCGOP = encRCGOP;

  int targetBits    = xEstPicTargetBits( encRCSeq, encRCGOP );
  int estHeaderBits = xEstPicHeaderBits( listPreviousPictures, frameLevel );

  if ( targetBits < estHeaderBits + 100 )
  {
    targetBits = estHeaderBits + 100;   // at least allocate 100 bits for picture data
  }

  m_frameLevel       = frameLevel;
  m_numberOfPixel    = encRCSeq->getNumPixel();
  m_numberOfLCU      = encRCSeq->getNumberOfLCU();
  m_estPicLambda     = 100.0;
  m_targetBits       = targetBits;
  m_estHeaderBits    = estHeaderBits;
  m_bitsLeft         = m_targetBits;
  int picWidth       = encRCSeq->getPicWidth();
  int picHeight      = encRCSeq->getPicHeight();
  int LCUWidth       = encRCSeq->getLCUWidth();
  int LCUHeight      = encRCSeq->getLCUHeight();
  int picWidthInLCU  = ( picWidth  % LCUWidth  ) == 0 ? picWidth  / LCUWidth  : picWidth  / LCUWidth  + 1;
  int picHeightInLCU = ( picHeight % LCUHeight ) == 0 ? picHeight / LCUHeight : picHeight / LCUHeight + 1;
  m_lowerBound       = xEstPicLowerBound( encRCSeq, encRCGOP );

  m_LCULeft         = m_numberOfLCU;
  m_bitsLeft       -= m_estHeaderBits;
  m_pixelsLeft      = m_numberOfPixel;

  m_LCUs           = new TRCLCU[m_numberOfLCU];
  int i, j;
  int LCUIdx;
  for ( i=0; i<picWidthInLCU; i++ )
  {
    for ( j=0; j<picHeightInLCU; j++ )
    {
      LCUIdx = j*picWidthInLCU + i;
      m_LCUs[LCUIdx].m_actualBits = 0;
      m_LCUs[LCUIdx].m_actualSSE  = 0.0;
      m_LCUs[LCUIdx].m_actualMSE  = 0.0;
      m_LCUs[LCUIdx].m_QP         = 0;
      m_LCUs[LCUIdx].m_lambda     = 0.0;
      m_LCUs[LCUIdx].m_targetBits = 0;
      m_LCUs[LCUIdx].m_bitWeight  = 1.0;
      int currWidth  = ( (i == picWidthInLCU -1) ? picWidth  - LCUWidth *(picWidthInLCU -1) : LCUWidth  );
      int currHeight = ( (j == picHeightInLCU-1) ? picHeight - LCUHeight*(picHeightInLCU-1) : LCUHeight );
      m_LCUs[LCUIdx].m_numberOfPixel = currWidth * currHeight;
    }
  }
  m_picActualHeaderBits = 0;
  m_picActualBits       = 0;
  m_picQP               = 0;
  m_picLambda           = 0.0;
  m_validPixelsInPic    = 0;
  m_picMSE              = 0.0;
}

void EncRCPic::destroy()
{
  if (m_LCUs != nullptr)
  {
    delete[] m_LCUs;
    m_LCUs = nullptr;
  }
  m_encRCSeq = nullptr;
  m_encRCGOP = nullptr;
}

double EncRCPic::estimatePicLambda(std::list<EncRCPic *> &listPreviousPictures, bool isIRAP)
{
  double alpha         = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
  double beta          = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;
  double bpp       = (double)m_targetBits/(double)m_numberOfPixel;

  int bitdepth_luma_scale =
    2 * (m_encRCSeq->getbitDepth() - 8
      - DISTORTION_PRECISION_ADJUSTMENT(m_encRCSeq->getbitDepth()));

  int lastPicValPix = 0;
  if (listPreviousPictures.size() > 0)
  {
    lastPicValPix = m_encRCSeq->getPicPara(m_frameLevel).m_validPix;
  }
  if (lastPicValPix > 0)
  {
    bpp = (double)m_targetBits / (double)lastPicValPix;
  }

  double estLambda;
  if (isIRAP)
  {
    estLambda = calculateLambdaIntra(alpha, beta, pow(m_totalCostIntra/(double)m_numberOfPixel, BETA1), bpp);
  }
  else
  {
    estLambda = alpha * pow( bpp, beta );
  }

  double lastLevelLambda = -1.0;
  double lastPicLambda   = -1.0;
  double lastValidLambda = -1.0;
  std::list<EncRCPic *>::iterator it;
  for ( it = listPreviousPictures.begin(); it != listPreviousPictures.end(); it++ )
  {
    if ( (*it)->getFrameLevel() == m_frameLevel )
    {
      lastLevelLambda = (*it)->getPicActualLambda();
    }
    lastPicLambda     = (*it)->getPicActualLambda();

    if ( lastPicLambda > 0.0 )
    {
      lastValidLambda = lastPicLambda;
    }
  }

  if ( lastLevelLambda > 0.0 )
  {
    lastLevelLambda = Clip3(m_encRCGOP->getMinEstLambda(), m_encRCGOP->getMaxEstLambda(), lastLevelLambda);
    estLambda = Clip3( lastLevelLambda * pow( 2.0, -3.0/3.0 ), lastLevelLambda * pow( 2.0, 3.0/3.0 ), estLambda );
  }

  if ( lastPicLambda > 0.0 )
  {
    lastPicLambda = Clip3(m_encRCGOP->getMinEstLambda(), 2000.0 * pow(2.0, bitdepth_luma_scale), lastPicLambda);
    estLambda = Clip3( lastPicLambda * pow( 2.0, -10.0/3.0 ), lastPicLambda * pow( 2.0, 10.0/3.0 ), estLambda );
  }
  else if ( lastValidLambda > 0.0 )
  {
    lastValidLambda = Clip3(m_encRCGOP->getMinEstLambda(), 2000.0 * pow(2.0, bitdepth_luma_scale), lastValidLambda);
    estLambda = Clip3( lastValidLambda * pow(2.0, -10.0/3.0), lastValidLambda * pow(2.0, 10.0/3.0), estLambda );
  }
  else
  {
    estLambda = Clip3(m_encRCGOP->getMinEstLambda(), m_encRCGOP->getMaxEstLambda(), estLambda);
  }

  if ( estLambda < m_encRCGOP->getMinEstLambda())
  {
    estLambda = m_encRCGOP->getMinEstLambda();
  }

  //Avoid different results in different platforms. The problem is caused by the different results of pow() in different platforms.
  estLambda = double(int64_t(estLambda * (double)LAMBDA_PREC + 0.5)) / (double)LAMBDA_PREC;
  m_estPicLambda = estLambda;

  double totalWeight = 0.0;
  // initial BU bit allocation weight
  for ( int i=0; i<m_numberOfLCU; i++ )
  {
    double alphaLCU, betaLCU;
    if ( m_encRCSeq->getUseLCUSeparateModel() )
    {
      alphaLCU = m_encRCSeq->getLCUPara( m_frameLevel, i ).m_alpha;
      betaLCU  = m_encRCSeq->getLCUPara( m_frameLevel, i ).m_beta;
    }
    else
    {
      alphaLCU = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
      betaLCU  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;
    }

    m_LCUs[i].m_bitWeight =  m_LCUs[i].m_numberOfPixel * pow( estLambda/alphaLCU, 1.0/betaLCU );

    if ( m_LCUs[i].m_bitWeight < 0.01 )
    {
      m_LCUs[i].m_bitWeight = 0.01;
    }
    totalWeight += m_LCUs[i].m_bitWeight;
  }
  for ( int i=0; i<m_numberOfLCU; i++ )
  {
    double BUTargetBits = m_targetBits * m_LCUs[i].m_bitWeight / totalWeight;
    m_LCUs[i].m_bitWeight = BUTargetBits;
  }

  return estLambda;
}

int EncRCPic::estimatePicQP(double lambda, std::list<EncRCPic *> &listPreviousPictures)
{
  int bitdepth_luma_scale =
    2
    * (m_encRCSeq->getbitDepth() - 8
      - DISTORTION_PRECISION_ADJUSTMENT(m_encRCSeq->getbitDepth()));

  int QP = int(4.2005 * log(lambda / pow(2.0, bitdepth_luma_scale)) + 13.7122 + 0.5);

  int lastLevelQP = g_RCInvalidQPValue;
  int lastPicQP   = g_RCInvalidQPValue;
  int lastValidQP = g_RCInvalidQPValue;
  std::list<EncRCPic *>::iterator it;
  for ( it = listPreviousPictures.begin(); it != listPreviousPictures.end(); it++ )
  {
    if ( (*it)->getFrameLevel() == m_frameLevel )
    {
      lastLevelQP = (*it)->getPicActualQP();
    }
    lastPicQP = (*it)->getPicActualQP();
    if ( lastPicQP > g_RCInvalidQPValue )
    {
      lastValidQP = lastPicQP;
    }
  }

  if ( lastLevelQP > g_RCInvalidQPValue )
  {
    QP = Clip3( lastLevelQP - 3, lastLevelQP + 3, QP );
  }

  if( lastPicQP > g_RCInvalidQPValue )
  {
    QP = Clip3( lastPicQP - 10, lastPicQP + 10, QP );
  }
  else if( lastValidQP > g_RCInvalidQPValue )
  {
    QP = Clip3( lastValidQP - 10, lastValidQP + 10, QP );
  }

  return QP;
}

double EncRCPic::getLCUTargetBpp(bool isIRAP)
{
  int   LCUIdx    = getLCUCoded();
  double bpp      = -1.0;
  int avgBits     = 0;

  if (isIRAP)
  {
    int    bitrateWindow = std::min(4, m_LCULeft);
    double MAD      = getLCU(LCUIdx).m_costIntra;

    if (m_remainingCostIntra > 0.1 )
    {
      double weightedBitsLeft = (m_bitsLeft*bitrateWindow+(m_bitsLeft-getLCU(LCUIdx).m_targetBitsLeft)*m_LCULeft)/(double)bitrateWindow;
      avgBits = int( MAD*weightedBitsLeft/m_remainingCostIntra );
    }
    else
    {
      avgBits = int( m_bitsLeft / m_LCULeft );
    }
    m_remainingCostIntra -= MAD;
  }
  else
  {
    double totalWeight = 0;
    for ( int i=LCUIdx; i<m_numberOfLCU; i++ )
    {
      totalWeight += m_LCUs[i].m_bitWeight;
    }
    int realInfluenceLCU = std::min(g_RCLCUSmoothWindowSize, getLCULeft());
    avgBits = (int)( m_LCUs[LCUIdx].m_bitWeight - ( totalWeight - m_bitsLeft ) / realInfluenceLCU + 0.5 );
  }

  if ( avgBits < 1 )
  {
    avgBits = 1;
  }

  bpp = ( double )avgBits/( double )m_LCUs[ LCUIdx ].m_numberOfPixel;
  m_LCUs[ LCUIdx ].m_targetBits = avgBits;

  return bpp;
}

double EncRCPic::getLCUEstLambda( double bpp )
{
  int   LCUIdx = getLCUCoded();
  double alpha;
  double beta;
  if ( m_encRCSeq->getUseLCUSeparateModel() )
  {
    alpha = m_encRCSeq->getLCUPara( m_frameLevel, LCUIdx ).m_alpha;
    beta  = m_encRCSeq->getLCUPara( m_frameLevel, LCUIdx ).m_beta;
  }
  else
  {
    alpha = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
    beta  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;
  }

  double estLambda = alpha * pow( bpp, beta );
  //for Lambda clip, picture level clip
  double clipPicLambda = m_estPicLambda;

  //for Lambda clip, LCU level clip
  double clipNeighbourLambda = -1.0;
  for ( int i=LCUIdx - 1; i>=0; i-- )
  {
    if ( m_LCUs[i].m_lambda > 0 )
    {
      clipNeighbourLambda = m_LCUs[i].m_lambda;
      break;
    }
  }

  if ( clipNeighbourLambda > 0.0 )
  {
    estLambda = Clip3( clipNeighbourLambda * pow( 2.0, -1.0/3.0 ), clipNeighbourLambda * pow( 2.0, 1.0/3.0 ), estLambda );
  }

  if ( clipPicLambda > 0.0 )
  {
    estLambda = Clip3( clipPicLambda * pow( 2.0, -2.0/3.0 ), clipPicLambda * pow( 2.0, 2.0/3.0 ), estLambda );
  }
  else
  {
    int bitdepth_luma_scale =
      2
      * (m_encRCSeq->getbitDepth() - 8
        - DISTORTION_PRECISION_ADJUSTMENT(m_encRCSeq->getbitDepth()));
    estLambda = Clip3(10.0 * pow(2.0, bitdepth_luma_scale), 1000.0 * pow(2.0, bitdepth_luma_scale), estLambda);
  }

  if ( estLambda < 0.1 )
  {
    estLambda = 0.1;
  }

  //Avoid different results in different platforms. The problem is caused by the different results of pow() in different platforms.
  estLambda = double(int64_t(estLambda * (double)LAMBDA_PREC + 0.5)) / (double)LAMBDA_PREC;
  return estLambda;
}

int EncRCPic::getLCUEstQP( double lambda, int clipPicQP )
{
  int LCUIdx = getLCUCoded();
  int bitdepth_luma_scale =
    2
    * (m_encRCSeq->getbitDepth() - 8
      - DISTORTION_PRECISION_ADJUSTMENT(m_encRCSeq->getbitDepth()));

  int estQP = int(4.2005 * log(lambda / pow(2.0, bitdepth_luma_scale)) + 13.7122 + 0.5);

  //for Lambda clip, LCU level clip
  int clipNeighbourQP = g_RCInvalidQPValue;
  for ( int i=LCUIdx - 1; i>=0; i-- )
  {
    if ( (getLCU(i)).m_QP > g_RCInvalidQPValue )
    {
      clipNeighbourQP = getLCU(i).m_QP;
      break;
    }
  }

  if ( clipNeighbourQP > g_RCInvalidQPValue )
  {
    estQP = Clip3( clipNeighbourQP - 1, clipNeighbourQP + 1, estQP );
  }

  estQP = Clip3( clipPicQP - 2, clipPicQP + 2, estQP );

  return estQP;
}

void EncRCPic::updateAfterCTU(int LCUIdx, int bits, int QP, double lambda, double skipRatio, bool updateLCUParameter)
{
  m_LCUs[LCUIdx].m_actualBits = bits;
  m_LCUs[LCUIdx].m_QP         = QP;
  m_LCUs[LCUIdx].m_lambda     = lambda;
  m_LCUs[LCUIdx].m_actualSSE  = m_LCUs[LCUIdx].m_actualMSE * m_LCUs[LCUIdx].m_numberOfPixel;

  m_LCULeft--;
  m_bitsLeft   -= bits;
  m_pixelsLeft -= m_LCUs[LCUIdx].m_numberOfPixel;

  if ( !updateLCUParameter )
  {
    return;
  }

  if ( !m_encRCSeq->getUseLCUSeparateModel() )
  {
    return;
  }

  int LCUActualBits   = m_LCUs[LCUIdx].m_actualBits;
  int LCUTotalPixels  = m_LCUs[LCUIdx].m_numberOfPixel;
  double bpp         = ( double )LCUActualBits/( double )LCUTotalPixels;
  double inputLambda = m_LCUs[LCUIdx].m_lambda;

  TRCParameter rcPara;
  rcPara.m_skipRatio = skipRatio;
  if (QP == g_RCInvalidQPValue && m_encRCSeq->getAdaptiveBits() == 1)
  {
    rcPara.m_validPix = 0;
  }
  else
  {
    rcPara.m_validPix = LCUTotalPixels;
  }

  double MSE = m_LCUs[LCUIdx].m_actualMSE;
  double updatedK = MSE > 0 ? bpp * inputLambda / MSE : 0.0;
  double updatedC = MSE / pow(bpp, -updatedK);
  rcPara.m_alpha = updatedC * updatedK;
  rcPara.m_beta = -updatedK - 1.0;

  if (MSE > 0)
  {
    rcPara.m_alpha = clipRcAlpha( m_encRCSeq->getbitDepth(), rcPara.m_alpha );
    rcPara.m_beta = clipRcBeta( rcPara.m_beta );
    m_encRCSeq->setLCUPara(m_frameLevel, LCUIdx, rcPara);
  }
}

double EncRCPic::calAverageQP()
{
  int totalQPs = 0;
  int numTotalLCUs = 0;

  int i;
  for ( i=0; i<m_numberOfLCU; i++ )
  {
    if ( m_LCUs[i].m_QP > 0 )
    {
      totalQPs += m_LCUs[i].m_QP;
      numTotalLCUs++;
    }
  }

  double avgQP = 0.0;
  if ( numTotalLCUs == 0 )
  {
    avgQP = g_RCInvalidQPValue;
  }
  else
  {
    avgQP = ((double)totalQPs) / ((double)numTotalLCUs);
  }
  return avgQP;
}

double EncRCPic::calAverageLambda()
{
  double totalLambdas = 0.0;
  int numTotalLCUs = 0;

  double totalSSE = 0.0;
  int totalPixels = 0;
  int i;
  for ( i=0; i<m_numberOfLCU; i++ )
  {
    if ( m_LCUs[i].m_lambda > 0.01 )
    {
      if (m_LCUs[i].m_QP > 0 || m_encRCSeq->getAdaptiveBits() != 1)
      {
        m_validPixelsInPic += m_LCUs[i].m_numberOfPixel;

        totalLambdas += log(m_LCUs[i].m_lambda);
        numTotalLCUs++;
      }

      if (m_LCUs[i].m_QP > 0 || m_encRCSeq->getAdaptiveBits() != 1)
      {
        totalSSE += m_LCUs[i].m_actualSSE;
        totalPixels += m_LCUs[i].m_numberOfPixel;
      }
    }
  }

  setPicMSE(totalPixels > 0 ? totalSSE / (double)totalPixels : 1.0); //1.0 is useless in the following process, just to make sure the divisor not be 0
  double avgLambda;
  if( numTotalLCUs == 0 )
  {
    avgLambda = -1.0;
  }
  else
  {
    avgLambda = pow( 2.7183, totalLambdas / numTotalLCUs );
  }
  return avgLambda;
}


void EncRCPic::updateAfterPicture( int actualHeaderBits, int actualTotalBits, double averageQP, double averageLambda, bool isIRAP)
{
  m_picActualHeaderBits = actualHeaderBits;
  m_picActualBits       = actualTotalBits;
  if ( averageQP > 0.0 )
  {
    m_picQP             = int( averageQP + 0.5 );
  }
  else
  {
    m_picQP             = g_RCInvalidQPValue;
  }
  m_picLambda           = averageLambda;
  double alpha = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
  double beta  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;
  double skipRatio = 0;
  int numOfSkipPixel = 0;
  for (int LCUIdx = 0; LCUIdx < m_numberOfLCU; LCUIdx++)
  {
    numOfSkipPixel += int(m_encRCSeq->getLCUPara(m_frameLevel, LCUIdx).m_skipRatio*m_LCUs[LCUIdx].m_numberOfPixel);
  }
  skipRatio = (double)numOfSkipPixel / (double)m_numberOfPixel;

  if (isIRAP)
  {
    updateAlphaBetaIntra(&alpha, &beta);
    TRCParameter rcPara;
    rcPara.m_skipRatio = skipRatio;
    rcPara.m_validPix = m_validPixelsInPic;
    rcPara.m_alpha = alpha;
    rcPara.m_beta = beta;
    m_encRCSeq->setPicPara(m_frameLevel, rcPara);
  }
  else
  {
    // update parameters
    double picActualBits = ( double )m_picActualBits;
    double picActualBpp = m_validPixelsInPic > 0 ? picActualBits / (double)m_validPixelsInPic : 0.001;
    TRCParameter rcPara;
    rcPara.m_skipRatio = skipRatio;
    rcPara.m_validPix = m_validPixelsInPic;
    double avgMSE = getPicMSE();
    double updatedK = picActualBpp * averageLambda / avgMSE;
    double updatedC = avgMSE / pow(picActualBpp, -updatedK);
    rcPara.m_alpha = updatedC * updatedK;
    rcPara.m_beta = -updatedK - 1.0;
    if (m_validPixelsInPic > 0)
    {
      rcPara.m_alpha = clipRcAlpha(m_encRCSeq->getbitDepth(), rcPara.m_alpha);
      rcPara.m_beta = clipRcBeta( rcPara.m_beta );
      m_encRCSeq->setPicPara(m_frameLevel, rcPara);
    }
    if ( m_frameLevel == 1 )
    {
      double currLambda = Clip3(m_encRCGOP->getMinEstLambda(), m_encRCGOP->getMaxEstLambda(), m_picLambda);
      double updateLastLambda = g_RCWeightHistoryLambda * m_encRCSeq->getLastLambda() + g_RCWeightCurrentLambda * currLambda;
      m_encRCSeq->setLastLambda( updateLastLambda );
    }
  }
}

double EncRCPic::clipRcAlpha(const int bitdepth, const double alpha)
{
  int bitdepth_luma_scale =
    2
    * (bitdepth - 8
      - DISTORTION_PRECISION_ADJUSTMENT(bitdepth));
  return Clip3(g_RCAlphaMinValue, g_RCAlphaMaxValue * pow(2.0, bitdepth_luma_scale), alpha);
}

double EncRCPic::clipRcBeta(const double beta)
{
  return Clip3(g_RCBetaMinValue, g_RCBetaMaxValue, beta);
}

int EncRCPic::getRefineBitsForIntra( int orgBits )
{
  double alpha=0.25, beta=0.5582;
  int iIntraBits;

  if (orgBits*40 < m_numberOfPixel)
  {
    alpha=0.25;
  }
  else
  {
    alpha=0.30;
  }

  iIntraBits = (int)(alpha* pow(m_totalCostIntra*4.0/(double)orgBits, beta)*(double)orgBits+0.5);

  return iIntraBits;
}

double EncRCPic::calculateLambdaIntra(double alpha, double beta, double MADPerPixel, double bitsPerPixel)
{
  return ( (alpha/256.0) * pow( MADPerPixel/bitsPerPixel, beta ) );
}

void EncRCPic::updateAlphaBetaIntra(double *alpha, double *beta)
{
  double lnbpp = log(pow(m_totalCostIntra / (double)m_numberOfPixel, BETA1)) - log((double)m_picActualBits / (double)m_numberOfPixel);
  double diffLambda = (*beta)*(log((double)m_picActualBits)-log((double)m_targetBits));

  diffLambda = Clip3(-0.125, 0.125, 0.25*diffLambda);
  *alpha    =  (*alpha) * exp(diffLambda);
  *beta     =  (*beta) + diffLambda / lnbpp;
}


void EncRCPic::getLCUInitTargetBits()
{
  int iAvgBits     = 0;

  m_remainingCostIntra = m_totalCostIntra;
  for (int i=m_numberOfLCU-1; i>=0; i--)
  {
    iAvgBits += int(m_targetBits * getLCU(i).m_costIntra/m_totalCostIntra);
    getLCU(i).m_targetBitsLeft = iAvgBits;
  }
}


double EncRCPic::getLCUEstLambdaAndQP(double bpp, int clipPicQP, int *estQP)
{
  int   LCUIdx = getLCUCoded();

  double   alpha = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
  double   beta  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;

  double costPerPixel = getLCU(LCUIdx).m_costIntra/(double)getLCU(LCUIdx).m_numberOfPixel;
  costPerPixel = pow(costPerPixel, BETA1);
  double estLambda = calculateLambdaIntra(alpha, beta, costPerPixel, bpp);

  int clipNeighbourQP = g_RCInvalidQPValue;
  for (int i=LCUIdx-1; i>=0; i--)
  {
    if ((getLCU(i)).m_QP > g_RCInvalidQPValue)
    {
      clipNeighbourQP = getLCU(i).m_QP;
      break;
    }
  }

  int minQP = clipPicQP - 2;
  int maxQP = clipPicQP + 2;

  if ( clipNeighbourQP > g_RCInvalidQPValue )
  {
    maxQP = std::min(clipNeighbourQP + 1, maxQP);
    minQP = std::max(clipNeighbourQP - 1, minQP);
  }

  int bitdepth_luma_scale =
    2
    * (m_encRCSeq->getbitDepth() - 8
      - DISTORTION_PRECISION_ADJUSTMENT(m_encRCSeq->getbitDepth()));

  double maxLambda = exp(((double)(maxQP + 0.49) - 13.7122) / 4.2005) * pow(2.0, bitdepth_luma_scale);
  double minLambda = exp(((double)(minQP - 0.49) - 13.7122) / 4.2005) * pow(2.0, bitdepth_luma_scale);

  estLambda = Clip3(minLambda, maxLambda, estLambda);

  //Avoid different results in different platforms. The problem is caused by the different results of pow() in different platforms.
  estLambda = double(int64_t(estLambda * (double)LAMBDA_PREC + 0.5)) / (double)LAMBDA_PREC;
  *estQP = int(4.2005 * log(estLambda / pow(2.0, bitdepth_luma_scale)) + 13.7122 + 0.5);
  *estQP = Clip3(minQP, maxQP, *estQP);

  return estLambda;
}

RateCtrl::RateCtrl()
{
  m_encRCSeq = nullptr;
  m_encRCGOP = nullptr;
  m_encRCPic = nullptr;
}

RateCtrl::~RateCtrl()
{
  destroy();
}

void RateCtrl::destroy()
{
  if (m_encRCSeq != nullptr)
  {
    delete m_encRCSeq;
    m_encRCSeq = nullptr;
  }
  if (m_encRCGOP != nullptr)
  {
    delete m_encRCGOP;
    m_encRCGOP = nullptr;
  }
  while ( m_listRCPictures.size() > 0 )
  {
    EncRCPic* p = m_listRCPictures.front();
    m_listRCPictures.pop_front();
    delete p;
  }
}

void RateCtrl::init(int totalFrames, int targetBitrate, const Fraction& frameRate, int GOPSize, int intraPeriod,
                    int picWidth, int picHeight, int LCUWidth, int LCUHeight, int bitDepth, int keepHierBits,
                    bool useLCUSeparateModel, GOPEntry GOPList[MAX_GOP])
{
  destroy();

  bool isLowdelay = true;
  for ( int i=0; i<GOPSize-1; i++ )
  {
    if ( GOPList[i].m_POC > GOPList[i+1].m_POC )
    {
      isLowdelay = false;
      break;
    }
  }

  int numberOfLevel = 1;
  int adaptiveBit = 0;
  if ( keepHierBits > 0 || ( !isLowdelay && (GOPSize == 32 || GOPSize == 16 || GOPSize == 8) ) )
  {
    numberOfLevel = int( log((double)GOPSize)/log(2.0) + 0.5 ) + 1;
  }
  numberOfLevel++;    // intra picture
  numberOfLevel++;    // non-reference picture


  int* bitsRatio;
  bitsRatio = new int[ GOPSize ];
  for ( int i=0; i<GOPSize; i++ )
  {
    bitsRatio[i] = 10;
    if ( !GOPList[i].m_refPic )
    {
      bitsRatio[i] = 2;
    }
  }

  if ( keepHierBits > 0 )
  {
    double bpp = (double) (targetBitrate / (frameRate.getFloatVal() * picWidth * picHeight));
    if ( GOPSize == 4 && isLowdelay )
    {
      if ( bpp > 0.2 )
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 6;
      }
      else if( bpp > 0.1 )
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 10;
      }
      else if ( bpp > 0.05 )
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 12;
      }
      else
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 14;
      }

      if ( keepHierBits == 2 )
      {
        adaptiveBit = 1;
      }
    }
    else if (GOPSize == 8 && isLowdelay)
    {
      if (bpp > 0.2)
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 3;
        bitsRatio[4] = 2;
        bitsRatio[5] = 3;
        bitsRatio[6] = 2;
        bitsRatio[7] = 6;
      }
      else if (bpp > 0.1)
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 3;
        bitsRatio[4] = 2;
        bitsRatio[5] = 3;
        bitsRatio[6] = 2;
        bitsRatio[7] = 10;
      }
      else if (bpp > 0.05)
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 3;
        bitsRatio[4] = 2;
        bitsRatio[5] = 3;
        bitsRatio[6] = 2;
        bitsRatio[7] = 12;
      }
      else
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 3;
        bitsRatio[4] = 2;
        bitsRatio[5] = 3;
        bitsRatio[6] = 2;
        bitsRatio[7] = 14;
      }

      if (keepHierBits == 2)
      {
        adaptiveBit = 1;
      }
    }
    else if ( GOPSize == 8 && !isLowdelay )
    {
      if ( bpp > 0.2 )
      {
        bitsRatio[0] = 15;
        bitsRatio[1] = 5;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }
      else if ( bpp > 0.1 )
      {
        bitsRatio[0] = 20;
        bitsRatio[1] = 6;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }
      else if ( bpp > 0.05 )
      {
        bitsRatio[0] = 25;
        bitsRatio[1] = 7;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }
      else
      {
        bitsRatio[0] = 30;
        bitsRatio[1] = 8;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }

      if ( keepHierBits == 2 )
      {
        adaptiveBit = 2;
      }
    }
    else if (GOPSize == 16 && !isLowdelay)
    {
      if (bpp > 0.2)
      {
        bitsRatio[0] = 10;
        bitsRatio[1] = 8;
        bitsRatio[2] = 4;
        bitsRatio[3] = 2;
        bitsRatio[4] = 1;
        bitsRatio[5] = 1;
        bitsRatio[6] = 2;
        bitsRatio[7] = 1;
        bitsRatio[8] = 1;
        bitsRatio[9] = 4;
        bitsRatio[10] = 2;
        bitsRatio[11] = 1;
        bitsRatio[12] = 1;
        bitsRatio[13] = 2;
        bitsRatio[14] = 1;
        bitsRatio[15] = 1;
      }
      else if (bpp > 0.1)
      {
        bitsRatio[0] = 15;
        bitsRatio[1] = 9;
        bitsRatio[2] = 4;
        bitsRatio[3] = 2;
        bitsRatio[4] = 1;
        bitsRatio[5] = 1;
        bitsRatio[6] = 2;
        bitsRatio[7] = 1;
        bitsRatio[8] = 1;
        bitsRatio[9] = 4;
        bitsRatio[10] = 2;
        bitsRatio[11] = 1;
        bitsRatio[12] = 1;
        bitsRatio[13] = 2;
        bitsRatio[14] = 1;
        bitsRatio[15] = 1;
      }
      else if (bpp > 0.05)
      {
        bitsRatio[0] = 40;
        bitsRatio[1] = 17;
        bitsRatio[2] = 7;
        bitsRatio[3] = 2;
        bitsRatio[4] = 1;
        bitsRatio[5] = 1;
        bitsRatio[6] = 2;
        bitsRatio[7] = 1;
        bitsRatio[8] = 1;
        bitsRatio[9] = 7;
        bitsRatio[10] = 2;
        bitsRatio[11] = 1;
        bitsRatio[12] = 1;
        bitsRatio[13] = 2;
        bitsRatio[14] = 1;
        bitsRatio[15] = 1;
      }
      else
      {
        bitsRatio[0] = 40;
        bitsRatio[1] = 15;
        bitsRatio[2] = 6;
        bitsRatio[3] = 3;
        bitsRatio[4] = 1;
        bitsRatio[5] = 1;
        bitsRatio[6] = 3;
        bitsRatio[7] = 1;
        bitsRatio[8] = 1;
        bitsRatio[9] = 6;
        bitsRatio[10] = 3;
        bitsRatio[11] = 1;
        bitsRatio[12] = 1;
        bitsRatio[13] = 3;
        bitsRatio[14] = 1;
        bitsRatio[15] = 1;
      }

      if (keepHierBits == 2)
      {
        adaptiveBit = 2;
      }
    }
    else if (GOPSize == 32 && !isLowdelay)
    {
      int bitsRatioInit[4][6]={
        {16, 10, 8, 4, 2, 1},
        {16, 10, 8, 4, 2, 1},
        {16, 10, 8, 4, 2, 1},
        {10, 8, 6, 4, 2, 1},
      };
      int cls;
      if (bpp > 0.2)
      {
        cls = 0;
      }
      else if(bpp > 0.1)
      {
        cls = 1;
      }
      else if(bpp > 0.05)
      {
        cls = 2;
      }
      else
      {
        cls = 3;
      }
      int index[32] = {0, 1, 2, 3, 4, 5, 5, 4, 5, 5, 3, 4, 5, 5, 4, 5, 5, 2, 3, 4, 5, 5, 4, 5, 5, 3, 4, 5, 5, 4, 5, 5};

      for (int i = 0; i < 32; i++)
      {
        bitsRatio[i] = bitsRatioInit[cls][index[i]];
      }
      if (keepHierBits == 2)
      {
        adaptiveBit = 2;
      }
    }
    else
    {
      msg( WARNING, "\n hierarchical bit allocation is not support for the specified coding structure currently.\n" );
    }
  }

  int* GOPID2Level = new int[ GOPSize ];
  for ( int i=0; i<GOPSize; i++ )
  {
    GOPID2Level[i] = 1;
    if ( !GOPList[i].m_refPic )
    {
      GOPID2Level[i] = 2;
    }
  }

  if ( keepHierBits > 0 )
  {
    if ( GOPSize == 4 && isLowdelay )
    {
      GOPID2Level[0] = 3;
      GOPID2Level[1] = 2;
      GOPID2Level[2] = 3;
      GOPID2Level[3] = 1;
    }
    if (GOPSize == 8 && isLowdelay)
    {
      GOPID2Level[0] = 3;
      GOPID2Level[1] = 2;
      GOPID2Level[2] = 3;
      GOPID2Level[3] = 2;
      GOPID2Level[4] = 3;
      GOPID2Level[5] = 2;
      GOPID2Level[6] = 3;
      GOPID2Level[7] = 1;
    }
    else if ( GOPSize == 8 && !isLowdelay )
    {
      GOPID2Level[0] = 1;
      GOPID2Level[1] = 2;
      GOPID2Level[2] = 3;
      GOPID2Level[3] = 4;
      GOPID2Level[4] = 4;
      GOPID2Level[5] = 3;
      GOPID2Level[6] = 4;
      GOPID2Level[7] = 4;
    }
    else if (GOPSize == 16 && !isLowdelay)
    {
      GOPID2Level[0] = 1;
      GOPID2Level[1] = 2;
      GOPID2Level[2] = 3;
      GOPID2Level[3] = 4;
      GOPID2Level[4] = 5;
      GOPID2Level[5] = 5;
      GOPID2Level[6] = 4;
      GOPID2Level[7] = 5;
      GOPID2Level[8] = 5;
      GOPID2Level[9] = 3;
      GOPID2Level[10] = 4;
      GOPID2Level[11] = 5;
      GOPID2Level[12] = 5;
      GOPID2Level[13] = 4;
      GOPID2Level[14] = 5;
      GOPID2Level[15] = 5;
    }
  }

  if ( !isLowdelay && GOPSize == 8 )
  {
    GOPID2Level[0] = 1;
    GOPID2Level[1] = 2;
    GOPID2Level[2] = 3;
    GOPID2Level[3] = 4;
    GOPID2Level[4] = 4;
    GOPID2Level[5] = 3;
    GOPID2Level[6] = 4;
    GOPID2Level[7] = 4;
  }
  else if (GOPSize == 16 && !isLowdelay)
  {
    GOPID2Level[0] = 1;
    GOPID2Level[1] = 2;
    GOPID2Level[2] = 3;
    GOPID2Level[3] = 4;
    GOPID2Level[4] = 5;
    GOPID2Level[5] = 5;
    GOPID2Level[6] = 4;
    GOPID2Level[7] = 5;
    GOPID2Level[8] = 5;
    GOPID2Level[9] = 3;
    GOPID2Level[10] = 4;
    GOPID2Level[11] = 5;
    GOPID2Level[12] = 5;
    GOPID2Level[13] = 4;
    GOPID2Level[14] = 5;
    GOPID2Level[15] = 5;
  }
  else if(GOPSize == 32 && !isLowdelay)
  {
    GOPID2Level[0]  = 1;
    GOPID2Level[1]  = 2;
    GOPID2Level[2]  = 3;
    GOPID2Level[3]  = 4;
    GOPID2Level[4]  = 5;
    GOPID2Level[5]  = 6;
    GOPID2Level[6]  = 6;
    GOPID2Level[7]  = 5;
    GOPID2Level[8]  = 6;
    GOPID2Level[9]  = 6;
    GOPID2Level[10] = 4;
    GOPID2Level[11] = 5;
    GOPID2Level[12] = 6;
    GOPID2Level[13] = 6;
    GOPID2Level[14] = 5;
    GOPID2Level[15] = 6;
    GOPID2Level[16] = 6;
    GOPID2Level[17] = 3;
    GOPID2Level[18] = 4;
    GOPID2Level[19] = 5;
    GOPID2Level[20] = 6;
    GOPID2Level[21] = 6;
    GOPID2Level[22] = 5;
    GOPID2Level[23] = 6;
    GOPID2Level[24] = 6;
    GOPID2Level[25] = 4;
    GOPID2Level[26] = 5;
    GOPID2Level[27] = 6;
    GOPID2Level[28] = 6;
    GOPID2Level[29] = 5;
    GOPID2Level[30] = 6;
    GOPID2Level[31] = 6;
  }

  m_encRCSeq = new EncRCSeq;
  m_encRCSeq->create(totalFrames, targetBitrate, frameRate, GOPSize, intraPeriod, picWidth, picHeight, LCUWidth, LCUHeight, numberOfLevel, useLCUSeparateModel, adaptiveBit);
  m_encRCSeq->initBitsRatio( bitsRatio );
  m_encRCSeq->initGOPID2Level( GOPID2Level );
  m_encRCSeq->setBitDepth(bitDepth);
  m_encRCSeq->initPicPara();
  if ( useLCUSeparateModel )
  {
    m_encRCSeq->initLCUPara();
  }
  m_CpbSaturationEnabled = false;
  m_cpbSize              = targetBitrate;
  m_cpbState             = (uint32_t)(m_cpbSize*0.5f);
  m_bufferingRate        = (int) (targetBitrate * frameRate.den / frameRate.num);

  delete[] bitsRatio;
  delete[] GOPID2Level;
}

void RateCtrl::initRCPic( int frameLevel )
{
  m_encRCPic = new EncRCPic;
  m_encRCPic->create( m_encRCSeq, m_encRCGOP, frameLevel, m_listRCPictures );
}

void RateCtrl::initRCGOP( int numberOfPictures )
{
  m_encRCGOP = new EncRCGOP;
  bool useAdaptiveBitsRatio = (m_encRCSeq->getAdaptiveBits() > 0) && (m_listRCPictures.size() >= m_encRCSeq->getGOPSize());
  m_encRCGOP->create(m_encRCSeq, numberOfPictures, useAdaptiveBitsRatio);
}

int  RateCtrl::updateCpbState(int actualBits)
{
  int cpbState = 1;

  m_cpbState -= actualBits;
  if (m_cpbState < 0)
  {
    cpbState = -1;
  }

  m_cpbState += m_bufferingRate;
  if (m_cpbState > m_cpbSize)
  {
    cpbState = 0;
  }

  return cpbState;
}

void RateCtrl::initHrdParam(const GeneralHrdParams* generalHrd, const OlsHrdParams* olsHrd, const Fraction& frameRate,
                            double fInitialCpbFullness)
{
  m_CpbSaturationEnabled = true;
  m_cpbSize = (olsHrd->getCpbSizeValueMinus1(0, 0) + 1) << (4 + generalHrd->getCpbSizeScale());
  m_cpbState = (uint32_t)(m_cpbSize*fInitialCpbFullness);
  m_bufferingRate = (uint32_t) (((olsHrd->getBitRateValueMinus1(0, 0) + 1) << (6 + generalHrd->getBitRateScale()))
                                * frameRate.den / frameRate.num);
  msg(NOTICE, "\nHRD - [Initial CPB state %6d] [CPB Size %6d] [Buffering Rate %6d]\n", m_cpbState, m_cpbSize, m_bufferingRate);
}

void RateCtrl::destroyRCGOP()
{
  delete m_encRCGOP;
  m_encRCGOP = nullptr;
}
