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

/** \file     TEncGOP.h
    \brief    GOP encoder class (header)
*/

#ifndef __TENCGOP__
#define __TENCGOP__

#include <list>

#include <stdlib.h>

#include "TLibCommon/TComList.h"
#include "TLibCommon/TComPic.h"
#include "TLibCommon/TComBitCounter.h"
#include "TLibCommon/TComLoopFilter.h"
#include "TLibCommon/AccessUnit.h"
#include "TEncSampleAdaptiveOffset.h"
#include "TEncSlice.h"
#include "TEncEntropy.h"
#include "TEncCavlc.h"
#include "TEncSbac.h"
#include "SEIwrite.h"
#include "SEIEncoder.h"
#if EXTENSION_360_VIDEO
#include "TAppEncHelper360/TExt360EncGop.h"
#endif
#if JVET_X0048_X0103_FILM_GRAIN
#include "TLibCommon/SEIFilmGrainAnalyzer.h"
#endif

#include "TEncAnalyze.h"
#include "TEncRateCtrl.h"
#include <vector>

//! \ingroup TLibEncoder
//! \{

class TEncTop;

// ====================================================================================================================
// Class definition
// ====================================================================================================================

class TEncGOP
{
  class DUData
  {
  public:
    DUData()
    :accumBitsDU(0)
    ,accumNalsDU(0) {};

    Int accumBitsDU;
    Int accumNalsDU;
  };

private:

  TEncAnalyze             m_gcAnalyzeAll;
  TEncAnalyze             m_gcAnalyzeI;
  TEncAnalyze             m_gcAnalyzeP;
  TEncAnalyze             m_gcAnalyzeB;

  TEncAnalyze             m_gcAnalyzeAll_in;

#if EXTENSION_360_VIDEO
  TExt360EncGop           m_ext360;
public:
  TExt360EncGop &getExt360Data() { return m_ext360; }
private:
#endif

  //  Data
  Bool                    m_bLongtermTestPictureHasBeenCoded;
  Bool                    m_bLongtermTestPictureHasBeenCoded2;
  UInt                    m_numLongTermRefPicSPS;
  UInt                    m_ltRefPicPocLsbSps[MAX_NUM_LONG_TERM_REF_PICS];
  Bool                    m_ltRefPicUsedByCurrPicFlag[MAX_NUM_LONG_TERM_REF_PICS];
  Int                     m_iLastIDR;
  Int                     m_RASPOCforResetEncoder; // an IDR POC number, after which the next POC (in output order) will be reset. If MAX_INT, then no reset is pending.
  Int                     m_iGopSize;
  Int                     m_iNumPicCoded;
  Bool                    m_bFirst;
  Int                     m_iLastRecoveryPicPOC;

  //  Access channel
  TEncTop*                m_pcEncTop;
  TEncCfg*                m_pcCfg;
  TEncSlice*              m_pcSliceEncoder;
  TComList<TComPic*>*     m_pcListPic;

  TEncEntropy*            m_pcEntropyCoder;
  TEncCavlc*              m_pcCavlcCoder;
  TEncSbac*               m_pcSbacCoder;
  TEncBinCABAC*           m_pcBinCABAC;
  TComLoopFilter*         m_pcLoopFilter;

  SEIWriter               m_seiWriter;

#if JVET_X0048_X0103_FILM_GRAIN
  FGAnalyser              m_FGAnalyser;
#endif

  //--Adaptive Loop filter
  TEncSampleAdaptiveOffset*  m_pcSAO;
  TEncRateCtrl*           m_pcRateCtrl;
  // indicate sequence first
  Bool                    m_bSeqFirst;

  // clean decoding refresh
  Bool                    m_bRefreshPending;
  Int                     m_pocCRA;
  NalUnitType             m_associatedIRAPType;
  Int                     m_associatedIRAPPOC;

  std::vector<Int> m_vRVM_RP;
  UInt                    m_lastBPSEI;
  UInt                    m_totalCoded;
  Bool                    m_bufferingPeriodSEIPresentInAU;
  SEIEncoder              m_seiEncoder;
  TComPicYuv*             m_pcDeblockingTempPicYuv;
  Int                     m_DBParam[MAX_ENCODER_DEBLOCKING_QUALITY_LAYERS][4];   //[layer_id][0: available; 1: bDBDisabled; 2: Beta Offset Div2; 3: Tc Offset Div2;]

public:
  TEncGOP();
  virtual ~TEncGOP();

  Void  create      ();
  Void  destroy     ();

  Void  init        ( TEncTop* pcTEncTop );
  Void  compressGOP ( Int iPOCLast, Int iNumPicRcvd, TComList<TComPic*>& rcListPic, TComList<TComPicYuv*>& rcListPicYuvRec,
                     std::list<AccessUnit>& accessUnitsInGOP, Bool isField, Bool isTff, const InputColourSpaceConversion ip_conversion, const InputColourSpaceConversion snr_conversion, const TEncAnalyze::OutputLogControl &outputLogCtrl );
  Void  xAttachSliceDataToNalUnit (OutputNALUnit& rNalu, TComOutputBitstream* pcBitstreamRedirect);


  Int   getGOPSize()          { return  m_iGopSize;  }

  TComList<TComPic*>*   getListPic()      { return m_pcListPic; }

  Void  printOutSummary      ( UInt uiNumAllPicCoded, Bool isField, const TEncAnalyze::OutputLogControl &outputLogCtrl, const BitDepths &bitDepths );

  Void  preLoopFilterPicAll  ( TComPic* pcPic, UInt64& ruiDist );

  TEncSlice*  getSliceEncoder()   { return m_pcSliceEncoder; }
  NalUnitType getNalUnitType( Int pocCurr, Int lastIdr, Bool isField );
  Void arrangeLongtermPicturesInRPS(TComSlice *, TComList<TComPic*>& );

  TEncAnalyze& getAnalyzeAllData() { return m_gcAnalyzeAll; }
  TEncAnalyze& getAnalyzeIData()   { return m_gcAnalyzeI; }
  TEncAnalyze& getAnalyzePData()   { return m_gcAnalyzeP; }
  TEncAnalyze& getAnalyzeBData()   { return m_gcAnalyzeB; }

#if MCTS_EXTRACTION
    Void generateVPS_RBSP(TComBitIf* rbsp, const TComVPS *vps);
    Void generateSPS_RBSP(TComBitIf* rbsp, const TComSPS *sps);
    Void generatePPS_RBSP(TComBitIf* rbsp, const TComPPS *pps);
#endif

protected:
  TEncRateCtrl* getRateCtrl()       { return m_pcRateCtrl;  }

protected:

  Void  xInitGOP          ( Int iPOCLast, Int iNumPicRcvd, Bool isField );
  Void  xGetBuffer        ( TComList<TComPic*>& rcListPic, TComList<TComPicYuv*>& rcListPicYuvRecOut, Int iNumPicRcvd, Int iTimeOffset, TComPic*& rpcPic, TComPicYuv*& rpcPicYuvRecOut, Int pocCurr, Bool isField );

  Void  xCalculateAddPSNRs         ( const Bool isField, const Bool isFieldTopFieldFirst, const Int iGOPid, TComPic* pcPic, const AccessUnit&accessUnit, TComList<TComPic*> &rcListPic, Double dEncTime, const InputColourSpaceConversion ip_conversion, const InputColourSpaceConversion snr_conversion, const TEncAnalyze::OutputLogControl &outputLogCtrl, Double* PSNR_Y );
  Void  xCalculateAddPSNR          ( TComPic* pcPic, TComPicYuv* pcPicD, const AccessUnit&, Double dEncTime, const InputColourSpaceConversion ip_conversion, const InputColourSpaceConversion snr_conversion, const TEncAnalyze::OutputLogControl &outputLogCtrl, Double* PSNR_Y );
  Void  xCalculateInterlacedAddPSNR( TComPic* pcPicOrgFirstField, TComPic* pcPicOrgSecondField,
                                    TComPicYuv* pcPicRecFirstField, TComPicYuv* pcPicRecSecondField,
                                    const InputColourSpaceConversion snr_conversion, const TEncAnalyze::OutputLogControl &outputLogCtrl, Double* PSNR_Y );
  Double xCalculateMSSSIM (const Pel *pOrg, const Int orgStride, const Pel* pRec, const Int recStride, const Int width, const Int height, const UInt bitDepth);

  UInt64 xFindDistortionFrame (TComPicYuv* pcPic0, TComPicYuv* pcPic1, const BitDepths &bitDepths);

  Double xCalculateRVM();

  Void xWriteAccessUnitDelimiter (AccessUnit &accessUnit, TComSlice *slice);
#if MCTS_EXTRACTION
  Void xCreateIRAPLeadingSEIMessages (SEIMessages& seiMessages, const TComVPS *vps, const TComSPS *sps, const TComPPS *pps);
#else
  Void xCreateIRAPLeadingSEIMessages (SEIMessages& seiMessages, const TComSPS *sps, const TComPPS *pps);
#endif
  Void xCreatePerPictureSEIMessages (Int picInGOP, SEIMessages& seiMessages, SEIMessages& nestedSeiMessages, TComSlice *slice);
  Void xCreatePictureTimingSEI  (Int IRAPGOPid, SEIMessages& seiMessages, SEIMessages& nestedSeiMessages, SEIMessages& duInfoSeiMessages, TComSlice *slice, Bool isField, std::deque<DUData> &duData);
  Void xUpdateDuData(AccessUnit &testAU, std::deque<DUData> &duData);
  Void xUpdateTimingSEI(SEIPictureTiming *pictureTimingSEI, std::deque<DUData> &duData, const TComSPS *sps);
  Void xUpdateDuInfoSEI(SEIMessages &duInfoSeiMessages, SEIPictureTiming *pictureTimingSEI);

  Void xCreateScalableNestingSEI (SEIMessages& seiMessages, SEIMessages& nestedSeiMessages);
  Void xWriteSEI (NalUnitType naluType, SEIMessages& seiMessages, AccessUnit &accessUnit, AccessUnit::iterator &auPos, Int temporalId, const TComSPS *sps);
  Void xWriteSEISeparately (NalUnitType naluType, SEIMessages& seiMessages, AccessUnit &accessUnit, AccessUnit::iterator &auPos, Int temporalId, const TComSPS *sps);
  Void xClearSEIs(SEIMessages& seiMessages, Bool deleteMessages);
  Void xWriteLeadingSEIOrdered (SEIMessages& seiMessages, SEIMessages& duInfoSeiMessages, AccessUnit &accessUnit, Int temporalId, const TComSPS *sps, Bool testWrite);
  Void xWriteLeadingSEIMessages  (SEIMessages& seiMessages, SEIMessages& duInfoSeiMessages, AccessUnit &accessUnit, Int temporalId, const TComSPS *sps, std::deque<DUData> &duData);
  Void xWriteTrailingSEIMessages (SEIMessages& seiMessages, AccessUnit &accessUnit, Int temporalId, const TComSPS *sps);
  Void xWriteDuSEIMessages       (SEIMessages& duInfoSeiMessages, AccessUnit &accessUnit, Int temporalId, const TComSPS *sps, std::deque<DUData> &duData);

  Int xWriteVPS (AccessUnit &accessUnit, const TComVPS *vps);
  Int xWriteSPS (AccessUnit &accessUnit, const TComSPS *sps);
  Int xWritePPS (AccessUnit &accessUnit, const TComPPS *pps);
  Int xWriteParameterSets (AccessUnit &accessUnit, TComSlice *slice, const Bool bSeqFirst);

  Void applyDeblockingFilterMetric( TComPic* pcPic, UInt uiNumSlices );
  Void applyDeblockingFilterParameterSelection( TComPic* pcPic, const UInt numSlices, const Int gopID );
};// END CLASS DEFINITION TEncGOP

//! \}

#endif // __TENCGOP__

