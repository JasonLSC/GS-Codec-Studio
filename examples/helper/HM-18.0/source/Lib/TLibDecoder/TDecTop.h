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

/** \file     TDecTop.h
    \brief    decoder class (header)
*/

#ifndef __TDECTOP__
#define __TDECTOP__

#include "TLibCommon/CommonDef.h"
#include "TLibCommon/TComList.h"
#include "TLibCommon/TComPicYuv.h"
#include "TLibCommon/TComPic.h"
#include "TLibCommon/TComTrQuant.h"
#include "TLibCommon/TComPrediction.h"
#include "TLibCommon/SEI.h"

#include "TDecGop.h"
#include "TDecEntropy.h"
#include "TDecSbac.h"
#include "TDecCAVLC.h"
#include "SEIread.h"
#include "TDecConformance.h"

class InputNALUnit;

//! \ingroup TLibDecoder
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// decoder class
class TDecTop
{
private:
  Int                     m_iMaxRefPicNum;

  NalUnitType             m_associatedIRAPType; ///< NAL unit type of the associated IRAP picture
  Int                     m_pocCRA;            ///< POC number of the latest CRA picture
  Int                     m_pocRandomAccess;   ///< POC number of the random access point (the first IDR or CRA picture)

  TComList<TComPic*>      m_cListPic;         //  Dynamic buffer
  ParameterSetManager     m_parameterSetManager;  // storage for parameter sets
  TComSlice*              m_apcSlicePilot;

  SEIMessages             m_SEIs; ///< List of SEI messages that have been received before the first slice and between slices, excluding prefix SEIs...

  // functional classes
  TComPrediction          m_cPrediction;
  TComTrQuant             m_cTrQuant;
  TDecGop                 m_cGopDecoder;
  TDecSlice               m_cSliceDecoder;
  TDecCu                  m_cCuDecoder;
  TDecEntropy             m_cEntropyDecoder;
  TDecCavlc               m_cCavlcDecoder;
  TDecSbac                m_cSbacDecoder;
  TDecBinCABAC            m_cBinCABAC;
  SEIReader               m_seiReader;
  TComLoopFilter          m_cLoopFilter;
  TComSampleAdaptiveOffset m_cSAO;
  TDecConformanceCheck    m_conformanceCheck;

  Bool isSkipPictureForBLA(Int& iPOCLastDisplay);
  Bool isRandomAccessSkipPicture(Int& iSkipFrame,  Int& iPOCLastDisplay);
  TComPic*                m_pcPic;
  UInt                    m_uiSliceIdx;
  Int                     m_prevPOC;
  Int                     m_prevTid0POC;
  Bool                    m_bFirstSliceInPicture;
#if JVET_X0048_X0103_FILM_GRAIN
  Bool                    m_bFirstPictureInSequence;
  SEIFilmGrainSynthesizer m_grainCharacteristic;
  TComPicYuv              m_grainBuf;
#endif
  Bool                    m_bFirstSliceInSequence;
  Bool                    m_prevSliceSkipped;
  Int                     m_skippedPOC;
  Bool                    m_bFirstSliceInBitstream;
  Int                     m_lastPOCNoOutputPriorPics;
  Bool                    m_isNoOutputPriorPics;
  Bool                    m_craNoRaslOutputFlag;    //value of variable NoRaslOutputFlag of the last CRA pic
#if SHUTTER_INTERVAL_SEI_PROCESSING
  Bool                    m_ShutterFilterEnable;    // Shutter Interval SEI Processing
#endif
#if O0043_BEST_EFFORT_DECODING
  UInt                    m_forceDecodeBitDepth;
#endif
  std::ostream           *m_pDecodedSEIOutputStream;

  Bool                    m_warningMessageSkipPicture;

#if MCTS_ENC_CHECK
  Bool                    m_tmctsCheckEnabled;

#endif
  std::list<InputNALUnit*> m_prefixSEINALUs; /// Buffered up prefix SEI NAL Units.
public:
  TDecTop();
  virtual ~TDecTop();

  Void  create  ();
  Void  destroy ();

  Void setDecodedPictureHashSEIEnabled(Int enabled) { m_cGopDecoder.setDecodedPictureHashSEIEnabled(enabled); }
#if MCTS_ENC_CHECK
  Void setTMctsCheckEnabled(Bool enabled) { m_tmctsCheckEnabled = enabled; }

#endif  
  Void  init();
#if MCTS_EXTRACTION
  SEIMessages& getSEIs() { return m_SEIs; }
  TComSlice* getApcSlicePilot() { return m_apcSlicePilot; }
  TComPic* getPcPic() const { return m_pcPic; }
  Bool  decode(InputNALUnit& nalu, Int& iSkipFrame, Int& iPOCLastDisplay, Bool bSkipCabacAndReconstruction=false);
#else
  Bool  decode(InputNALUnit& nalu, Int& iSkipFrame, Int& iPOCLastDisplay);
#endif
  Void  deletePicBuffer();

  
  Void  executeLoopFilters(Int& poc, TComList<TComPic*>*& rpcListPic);
  Void  checkNoOutputPriorPics (TComList<TComPic*>* rpcListPic);

  Bool  getNoOutputPriorPicsFlag () { return m_isNoOutputPriorPics; }
  Void  setNoOutputPriorPicsFlag (Bool val) { m_isNoOutputPriorPics = val; }
  Void  setFirstSliceInPicture (bool val)  { m_bFirstSliceInPicture = val; }
  Bool  getFirstSliceInSequence ()         { return m_bFirstSliceInSequence; }
  Void  setFirstSliceInSequence (bool val) { m_bFirstSliceInSequence = val; }
#if SHUTTER_INTERVAL_SEI_PROCESSING
  Bool  getShutterFilterFlag () const     { return m_ShutterFilterEnable; }
  Void  setShutterFilterFlag (Bool value) { m_ShutterFilterEnable = value; }
#endif
#if O0043_BEST_EFFORT_DECODING
  Void  setForceDecodeBitDepth(UInt bitDepth) { m_forceDecodeBitDepth = bitDepth; }
#endif
  Void  setDecodedSEIMessageOutputStream(std::ostream *pOpStream) { m_pDecodedSEIOutputStream = pOpStream; }
  UInt  getNumberOfChecksumErrorsDetected() const { return m_cGopDecoder.getNumberOfChecksumErrorsDetected(); }

protected:
  Void  xGetNewPicBuffer  (const TComSPS &sps, const TComPPS &pps, TComPic*& rpcPic, const UInt temporalLayer);
  Void  xCreateLostPicture (Int iLostPOC);

#if MCTS_EXTRACTION
  Bool      xDecodeSlice(InputNALUnit &nalu, Int &iSkipFrame, Int iPOCLastDisplay, Bool bSkipCabacAndReconstruction);
  Void      xActivateParameterSets(Bool bSkipCabacAndReconstruction);
#else
  Bool      xDecodeSlice(InputNALUnit &nalu, Int &iSkipFrame, Int iPOCLastDisplay);
  Void      xActivateParameterSets();
#endif
  Void      xDecodeVPS(const std::vector<UChar> &naluData);
  Void      xDecodeSPS(const std::vector<UChar> &naluData);
  Void      xDecodePPS(const std::vector<UChar> &naluData);
  Void      xUpdatePreviousTid0POC( TComSlice *pSlice ) { if ((pSlice->getTLayer()==0) && (pSlice->isReferenceNalu() && (pSlice->getNalUnitType()!=NAL_UNIT_CODED_SLICE_RASL_R)&& (pSlice->getNalUnitType()!=NAL_UNIT_CODED_SLICE_RADL_R))) { m_prevTid0POC=pSlice->getPOC(); } }
#if MCTS_EXTRACTION
public:
#endif
  TComList<TComPic*>*  getRpcListPic()  { return &m_cListPic; };
  Void      xParsePrefixSEImessages();
#if MCTS_EXTRACTION
private:
#endif
#if MCTS_ENC_CHECK
  Void      xAnalysePrefixSEImessages();
#endif
  Void      xParsePrefixSEIsForUnknownVCLNal();

};// END CLASS DEFINITION TDecTop


//! \}

#endif // __TDECTOP__

