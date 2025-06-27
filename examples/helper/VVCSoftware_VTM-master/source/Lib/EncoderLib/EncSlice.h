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

/** \file     EncSlice.h
    \brief    slice encoder class (header)
*/

#ifndef __ENCSLICE__
#define __ENCSLICE__

// Include files
#include "EncCu.h"
#include "WeightPredAnalysis.h"
#include "RateCtrl.h"

#include "CommonLib/CommonDef.h"
#include "CommonLib/Picture.h"

//! \ingroup EncoderLib
//! \{

class EncLib;
class EncGOP;

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// slice encoder class
class EncSlice
  : public WeightPredAnalysis
{
private:
  // encoder configuration
  EncCfg*                 m_pcCfg;                              ///< encoder configuration class

  EncLib*                 m_pcLib;

  // pictures
  PicList*                m_pcListPic;                          ///< list of pictures

  // processing units
  EncGOP*                 m_pcGOPEncoder;                       ///< GOP encoder
  EncCu*                  m_pcCuEncoder;                        ///< CU encoder

  // encoder search
  InterSearch*            m_pcInterSearch;                      ///< encoder search class

  // coding tools
  CABACWriter*            m_CABACWriter;
  TrQuant*                m_pcTrQuant;                          ///< transform & quantization

  // RD optimization
  RdCost*                 m_pcRdCost;                           ///< RD cost computation
  CABACWriter*            m_CABACEstimator;
  uint64_t                  m_uiPicTotalBits;                     ///< total bits for the picture
  uint64_t                  m_uiPicDist;                          ///< total distortion for the picture
  std::vector<double>     m_vdRdPicLambda;                      ///< array of lambda candidates
  std::vector<double>     m_vdRdPicQp;                          ///< array of picture QP candidates (double-type for lambda)
  std::vector<int>        m_viRdPicQp;                          ///< array of picture QP candidates (int-type)
  RateCtrl*               m_pcRateCtrl;                         ///< Rate control manager
  uint32_t                    m_uiSliceSegmentIdx;
  Ctx                     m_entropyCodingSyncContextState;      ///< context storage for state of contexts at the wavefront/WPP/entropy-coding-sync second CTU of tile-row
  SliceType               m_encCABACTableIdx;
  PLTBuf                  m_palettePredictorSyncState;
#if SHARP_LUMA_DELTA_QP || ENABLE_QPA_SUB_CTU
  int                     m_gopID;
#endif

public:
  double initializeLambda(const Slice *slice, const int gopId, const int refQP,
                          const double dQP);   // called by calculateLambda() and updateLambda()
#if SHARP_LUMA_DELTA_QP || ENABLE_QPA_SUB_CTU
  int     getGopId() const { return m_gopID; }
  double  calculateLambda(const Slice *slice, const int gopId, const double refQP, const double dQP, int &qp);
#endif
  void setUpLambda(Slice *slice, const double dLambda, int qp);

#if ENABLE_QPA
  int                     m_adaptedLumaQP;

#endif
  EncSlice();
  virtual ~EncSlice();

  void    create(int width, int height, ChromaFormat chromaFormat, uint32_t iMaxCUWidth, uint32_t iMaxCUHeight,
                 uint8_t uhTotalDepth);
  void    destroy             ();
  void    init                ( EncLib* pcEncLib, const SPS& sps );

  /// preparation of slice encoding (reference marking, QP and lambda)
  void initEncSlice(Picture *pcPic, const int pocLast, const int pocCurr, const int gopId, Slice *&rpcSlice,
                    const bool isField, bool isEncodeLtRef, int layerId, NalUnitType nalType);

  void    resetQP             ( Picture* pic, int sliceQP, double lambda );

  // compress and encode slice
  void    precompressSlice    ( Picture* pcPic                                     );      ///< precompress slice for multi-loop slice-level QP opt.
  void    compressSlice       ( Picture* pcPic, const bool bCompressEntireSlice, const bool bFastDeltaQP );      ///< analysis stage of slice
  void    calCostSliceI       ( Picture* pcPic );
  void    calCostPictureI     ( Picture* picture );
  void    setLosslessSlice(Picture* pcPic, bool b);      ///< Set if the slice is lossless or not
  void    encodeSlice         ( Picture* pcPic, OutputBitstream* pcSubstreams, uint32_t &numBinsCoded );
  void    encodeCtus          ( Picture* pcPic, const bool bCompressEntireSlice, const bool bFastDeltaQP, EncLib* pcEncLib );
  void    checkDisFracMmvd    ( Picture* pcPic, uint32_t startCtuTsAddr, uint32_t boundingCtuTsAddr );
  void    setJointCbCrModes( CodingStructure& cs, const Position topLeftLuma, const Size sizeLuma );

  // misc. functions
  void    setSearchRange      ( Slice* pcSlice  );                                  ///< set ME range adaptively

  EncCu*  getCUEncoder        ()                    { return m_pcCuEncoder; }                        ///< CU encoder
  uint32_t    getSliceSegmentIdx  ()                    { return m_uiSliceSegmentIdx;       }
  void    setSliceSegmentIdx  (uint32_t i)              { m_uiSliceSegmentIdx = i;          }

  SliceType getEncCABACTableIdx() const             { return m_encCABACTableIdx;        }
  void    setEncCABACTableIdx (SliceType b)         { m_encCABACTableIdx = b; }
private:
  double  xGetQPValueAccordingToLambda ( double lambda );

private:
  std::vector<double>     m_lambdaWeight;
  double                  m_lambda;
  int                     m_qpCtu;
  PelStorage              m_pre;
  std::vector<double>     m_factorBlk;
  int**                   m_pixelPredErr;
  int**                   m_pixelRecDis;
  int                     m_maxPicWidth;
  int                     m_maxPicHeight;

  void    setCTULambdaQpByWeight(uint32_t ctuIdx, TrQuant* pTrQuant, RdCost* pRdCost, Slice* pcSlice);
  void    setLambdaWeightByDPF(Picture* pcPic);

public:
  int     getQpCtu() const { return m_qpCtu; }
};

//! \}

#endif // __ENCSLICE__
