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

/** \file     RateCtrl.h
    \brief    Rate control manager class
*/

#ifndef __ENCRATECTRL__
#define __ENCRATECTRL__

#pragma once

#include "../CommonLib/CommonDef.h"

#include <vector>
#include <algorithm>

//! \ingroup EncoderLib
//! \{

#include "../EncoderLib/EncCfg.h"
#include <list>

const int g_RCInvalidQPValue = -999;
const int g_RCSmoothWindowSizeAlpha = 20;
const int g_RCSmoothWindowSizeBeta = 60;
const int g_RCMaxPicListSize = 32;
const double g_RCWeightPicTargetBitInGOP    = 0.9;
const double g_RCWeightPicRargetBitInBuffer = 1.0 - g_RCWeightPicTargetBitInGOP;
const int g_RCIterationNum = 20;
const double g_RCWeightHistoryLambda = 0.5;
const double g_RCWeightCurrentLambda = 1.0 - g_RCWeightHistoryLambda;
const int g_RCLCUSmoothWindowSize = 4;
const double g_RCAlphaMinValue = 0.05;
const double g_RCAlphaMaxValue = 500.0;
const double g_RCBetaMinValue  = -3.0;
const double g_RCBetaMaxValue  = -0.1;

#define ALPHA     6.7542;
#define BETA1     1.2517
#define BETA2     1.7860

struct TRCLCU
{
  int m_actualBits;
  int m_QP;     // QP of skip mode is set to g_RCInvalidQPValue
  int m_targetBits;
  double m_lambda;
  double m_bitWeight;
  int m_numberOfPixel;
  double m_costIntra;
  int m_targetBitsLeft;
  double m_actualSSE;
  double m_actualMSE;
};

struct TRCParameter
{
  double m_alpha;
  double m_beta;
  int    m_validPix;
  double m_skipRatio;
};

class EncRCSeq
{
public:
  EncRCSeq();
  ~EncRCSeq();

public:
  void create(int totalFrames, int targetBitrate, const Fraction& frameRate, int GOPSize, int intraPeriod, int picWidth,
              int picHeight, int LCUWidth, int LCUHeight, int numberOfLevel, bool useLCUSeparateModel, int adaptiveBit);
  void destroy();
  void initBitsRatio( int bitsRatio[] );
  void initGOPID2Level( int GOPID2Level[] );
  void initPicPara(TRCParameter *picPara = nullptr);    // nullptr to initial with default value
  void initLCUPara(TRCParameter **LCUPara = nullptr);   // nullptr to initial with default value
  void updateAfterPic ( int bits );
  void setAllBitRatio( double basicLambda, double* equaCoeffA, double* equaCoeffB );

public:
  int  getTotalFrames()                 { return m_totalFrames; }
  int  getTargetRate()                  { return m_targetRate; }
  const Fraction& getFrameRate() const { return m_frameRate; }
  int  getGOPSize()                     { return m_GOPSize; }
  int  getIntraPeriod()                 { return m_intraPeriod; }
  int  getPicWidth()                    { return m_picWidth; }
  int  getPicHeight()                   { return m_picHeight; }
  int  getLCUWidth()                    { return m_LCUWidth; }
  int  getLCUHeight()                   { return m_LCUHeight; }
  int  getNumberOfLevel()               { return m_numberOfLevel; }
  int  getAverageBits()                 { return m_averageBits; }
  int  getLeftAverageBits()             { CHECK(!( m_framesLeft > 0 ), "No frames left"); return (int)(m_bitsLeft / m_framesLeft); }
  bool getUseLCUSeparateModel()         { return m_useLCUSeparateModel; }

  int  getNumPixel()                    { return m_numberOfPixel; }
  int64_t  getTargetBits()                { return m_targetBits; }
  int  getNumberOfLCU()                 { return m_numberOfLCU; }
  int* getBitRatio()                    { return m_bitsRatio; }
  int  getBitRatio( int idx )           { CHECK(!( idx<m_GOPSize), "Idx exceeds GOP size"); return m_bitsRatio[idx]; }
  int* getGOPID2Level()                 { return m_GOPID2Level; }
  int  getGOPID2Level( int ID )         { CHECK(!( ID < m_GOPSize ), "Idx exceeds GOP size"); return m_GOPID2Level[ID]; }
  TRCParameter*  getPicPara()                                   { return m_picPara; }
  TRCParameter   getPicPara( int level )                        { CHECK(!( level < m_numberOfLevel ), "Level too big"); return m_picPara[level]; }
  void           setPicPara( int level, TRCParameter para )     { CHECK(!( level < m_numberOfLevel ), "Level too big"); m_picPara[level] = para; }
  TRCParameter** getLCUPara()                                   { return m_LCUPara; }
  TRCParameter*  getLCUPara( int level )                        { CHECK(!( level < m_numberOfLevel ), "Level too big"); return m_LCUPara[level]; }
  TRCParameter   getLCUPara( int level, int LCUIdx )            { CHECK(!( LCUIdx  < m_numberOfLCU ), "LCU id exceeds number of LCU"); return getLCUPara(level)[LCUIdx]; }
  void           setLCUPara( int level, int LCUIdx, TRCParameter para ) { CHECK(!( level < m_numberOfLevel ), "Level too big"); CHECK(!( LCUIdx  < m_numberOfLCU ), "LCU id exceeds number of LCU"); m_LCUPara[level][LCUIdx] = para; }

  int  getFramesLeft()                  { return m_framesLeft; }
  int64_t  getBitsLeft()                  { return m_bitsLeft; }

  double getSeqBpp()                    { return m_seqTargetBpp; }
  double getAlphaUpdate()               { return m_alphaUpdate; }
  double getBetaUpdate()                { return m_betaUpdate; }

  int    getAdaptiveBits()              { return m_adaptiveBit;  }
  double getLastLambda()                { return m_lastLambda;   }
  void   setLastLambda( double lamdba ) { m_lastLambda = lamdba; }
  void setBitDepth(int bitDepth) { m_bitDepth = bitDepth; }
  int getbitDepth() { return m_bitDepth; }

private:
  int m_totalFrames;
  int m_targetRate;
  Fraction m_frameRate;
  int m_GOPSize;
  int m_intraPeriod;
  int m_picWidth;
  int m_picHeight;
  int m_LCUWidth;
  int m_LCUHeight;
  int m_numberOfLevel;
  int m_averageBits;

  int m_numberOfPixel;
  int64_t m_targetBits;
  int m_numberOfLCU;
  int* m_bitsRatio;
  int* m_GOPID2Level;
  TRCParameter*  m_picPara;
  TRCParameter** m_LCUPara;

  int m_framesLeft;
  int64_t m_bitsLeft;
  double m_seqTargetBpp;
  double m_alphaUpdate;
  double m_betaUpdate;
  bool m_useLCUSeparateModel;

  int m_adaptiveBit;
  double m_lastLambda;
  int m_bitDepth;
};

class EncRCGOP
{
public:
  EncRCGOP();
  ~EncRCGOP();

public:
  void create(EncRCSeq *encRCSeq, int numPic, bool useAdaptiveBitsRatio);
  void destroy();
  void updateAfterPicture( int bitsCost );

private:
  int  xEstGOPTargetBits( EncRCSeq* encRCSeq, int GOPSize );
  void   xCalEquaCoeff( EncRCSeq* encRCSeq, double* lambdaRatio, double* equaCoeffA, double* equaCoeffB, int GOPSize );
  double xSolveEqua(EncRCSeq* encRCSeq, double targetBpp, double* equaCoeffA, double* equaCoeffB, int GOPSize);

public:
  EncRCSeq* getEncRCSeq()        { return m_encRCSeq; }
  int  getNumPic()                { return m_numPic;}
  int  getTargetBits()            { return m_targetBits; }
  int  getPicLeft()               { return m_picLeft; }
  int  getBitsLeft()              { return m_bitsLeft; }
  int  getTargetBitInGOP( int i ) { return m_picTargetBitInGOP[i]; }
  double getMinEstLambda()        { return m_minEstLambda; }
  double getMaxEstLambda()        { return m_maxEstLambda; }

private:
  EncRCSeq* m_encRCSeq;
  int* m_picTargetBitInGOP;
  int m_numPic;
  int m_targetBits;
  int m_picLeft;
  int m_bitsLeft;
  double m_minEstLambda;
  double m_maxEstLambda;
};

class EncRCPic
{
public:
  EncRCPic();
  ~EncRCPic();

public:
  void create(EncRCSeq *encRCSeq, EncRCGOP *encRCGOP, int frameLevel, std::list<EncRCPic *> &listPreviousPictures);
  void destroy();

  int    estimatePicQP(double lambda, std::list<EncRCPic *> &listPreviousPictures);
  int    getRefineBitsForIntra(int orgBits);
  double calculateLambdaIntra(double alpha, double beta, double MADPerPixel, double bitsPerPixel);
  double estimatePicLambda(std::list<EncRCPic *> &listPreviousPictures, bool isIRAP);

  void   updateAlphaBetaIntra(double *alpha, double *beta);

  double getLCUTargetBpp(bool isIRAP);
  double getLCUEstLambdaAndQP(double bpp, int clipPicQP, int *estQP);
  double getLCUEstLambda( double bpp );
  int    getLCUEstQP( double lambda, int clipPicQP );
  void updateAfterCTU(int LCUIdx, int bits, int QP, double lambda, double skipRatio, bool updateLCUParameter = true);
  void updateAfterPicture( int actualHeaderBits, int actualTotalBits, double averageQP, double averageLambda, bool isIRAP);

  double clipRcAlpha(const int bitdepth, const double alpha);
  double clipRcBeta(const double beta);

  void   addToPictureLsit(std::list<EncRCPic *> &listPreviousPictures);
  double calAverageQP();
  double calAverageLambda();

private:
  int xEstPicTargetBits( EncRCSeq* encRCSeq, EncRCGOP* encRCGOP );
  int xEstPicHeaderBits(std::list<EncRCPic *> &listPreviousPictures, int frameLevel);
  int xEstPicLowerBound( EncRCSeq* encRCSeq, EncRCGOP* encRCGOP );

public:
  EncRCSeq*      getRCSequence()                         { return m_encRCSeq; }
  EncRCGOP*      getRCGOP()                              { return m_encRCGOP; }

  int  getFrameLevel()                                    { return m_frameLevel; }
  int  getNumberOfPixel()                                 { return m_numberOfPixel; }
  int  getNumberOfLCU()                                   { return m_numberOfLCU; }
  int  getTargetBits()                                    { return m_targetBits; }
  int  getEstHeaderBits()                                 { return m_estHeaderBits; }
  int  getLCULeft()                                       { return m_LCULeft; }
  int  getBitsLeft()                                      { return m_bitsLeft; }
  int  getPixelsLeft()                                    { return m_pixelsLeft; }
  int  getBitsCoded()                                     { return m_targetBits - m_estHeaderBits - m_bitsLeft; }
  int  getLCUCoded()                                      { return m_numberOfLCU - m_LCULeft; }
  int  getLowerBound()                                    { return m_lowerBound; }
  TRCLCU* getLCU()                                        { return m_LCUs; }
  TRCLCU& getLCU( int LCUIdx )                            { return m_LCUs[LCUIdx]; }
  int  getPicActualHeaderBits()                           { return m_picActualHeaderBits; }
  void setBitLeft(int bits)                               { m_bitsLeft = bits; }
  void setTargetBits( int bits )                          { m_targetBits = bits; m_bitsLeft = bits;}
  void setTotalIntraCost(double cost)                     { m_totalCostIntra = cost; }
  void getLCUInitTargetBits();

  int  getPicActualBits()                                 { return m_picActualBits; }
  int  getPicActualQP()                                   { return m_picQP; }
  double getPicActualLambda()                             { return m_picLambda; }
  int  getPicEstQP()                                      { return m_estPicQP; }
  void setPicEstQP( int QP )                              { m_estPicQP = QP; }
  double getPicEstLambda()                                { return m_estPicLambda; }
  void setPicEstLambda( double lambda )                   { m_picLambda = lambda; }
  double getPicMSE()                                      { return m_picMSE; }
  void  setPicMSE(double avgMSE)                           { m_picMSE = avgMSE; }

private:
  EncRCSeq* m_encRCSeq;
  EncRCGOP* m_encRCGOP;

  int m_frameLevel;
  int m_numberOfPixel;
  int m_numberOfLCU;
  int m_targetBits;
  int m_estHeaderBits;
  int m_estPicQP;
  int m_lowerBound;
  double m_estPicLambda;

  int m_LCULeft;
  int m_bitsLeft;
  int m_pixelsLeft;

  TRCLCU* m_LCUs;
  int m_picActualHeaderBits;    // only SH and potential APS
  double m_totalCostIntra;
  double m_remainingCostIntra;
  int m_picActualBits;          // the whole picture, including header
  int m_picQP;                  // in integer form
  double m_picLambda;
  double m_picMSE;
  int m_validPixelsInPic;
};

class RateCtrl
{
public:
  RateCtrl();
  ~RateCtrl();

public:
  void init(int totalFrames, int targetBitrate, const Fraction& frameRate, int GOPSize, int intraPeriod, int picWidth,
            int picHeight, int LCUWidth, int LCUHeight, int bitDepth, int keepHierBits, bool useLCUSeparateModel,
            GOPEntry GOPList[MAX_GOP]);
  void destroy();
  void initRCPic( int frameLevel );
  void initRCGOP( int numberOfPictures );
  void destroyRCGOP();

public:
  void       setRCQP ( int QP ) { m_RCQP = QP;   }
  int        getRCQP () const   { return m_RCQP; }
  EncRCSeq  *getRCSeq()
  {
    CHECK(m_encRCSeq == nullptr, "Object does not exist");
    return m_encRCSeq;
  }
  EncRCGOP *getRCGOP()
  {
    CHECK(m_encRCGOP == nullptr, "Object does not exist");
    return m_encRCGOP;
  }
  EncRCPic *getRCPic()
  {
    CHECK(m_encRCPic == nullptr, "Object does not exist");
    return m_encRCPic;
  }
  std::list<EncRCPic *> &getPicList() { return m_listRCPictures; }
  bool       getCpbSaturationEnabled()  { return m_CpbSaturationEnabled;  }
  uint32_t       getCpbState()              { return m_cpbState;       }
  uint32_t       getCpbSize()               { return m_cpbSize;        }
  uint32_t       getBufferingRate()         { return m_bufferingRate;  }
  int        updateCpbState(int actualBits);
  void initHrdParam(const GeneralHrdParams* generalHrd, const OlsHrdParams* olsHrd, const Fraction& frameRate,
                    double fInitialCpbFullness);

private:
  EncRCSeq* m_encRCSeq;
  EncRCGOP* m_encRCGOP;
  EncRCPic* m_encRCPic;
  std::list<EncRCPic *> m_listRCPictures;
  int        m_RCQP;
  bool       m_CpbSaturationEnabled;    // Enable target bits saturation to avoid CPB overflow and underflow
  int        m_cpbState;                // CPB State
  uint32_t       m_cpbSize;                 // CPB size
  uint32_t       m_bufferingRate;           // Buffering rate
};

#endif


