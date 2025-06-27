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

/** \file     EncLib.h
    \brief    encoder class (header)
*/

#ifndef __ENCTOP__
#define __ENCTOP__

// Include files
#include "CommonLib/TrQuant.h"
#include "CommonLib/DeblockingFilter.h"
#include "CommonLib/NAL.h"

#include "Utilities/VideoIOYuv.h"

#include "EncCfg.h"
#include "EncGOP.h"
#include "EncSlice.h"
#include "EncHRD.h"
#include "VLCWriter.h"
#include "CABACWriter.h"
#include "InterSearch.h"
#include "IntraSearch.h"
#include "EncSampleAdaptiveOffset.h"
#include "EncReshape.h"
#include "EncAdaptiveLoopFilter.h"
#include "RateCtrl.h"
#include "EncTemporalFilter.h"

#include "CommonLib/SEINeuralNetworkPostFiltering.h"
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
#include "CommonLib/SEIPackedRegionsInfoProcess.h"
#endif

class EncLibCommon;

//! \ingroup EncoderLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// encoder class
class EncLib : public EncCfg
{
private:
  // picture
  int                       m_pocLast;                            ///< time index (POC)
  int                       m_receivedPicCount;                   ///< number of received pictures
  uint32_t                  m_codedPicCount;                      ///< number of coded pictures
  PicList&                  m_cListPic;                           ///< dynamic list of pictures
  int                       m_layerId;
  int                       m_gopRprPpsId;

  // encoder search
  InterSearch               m_cInterSearch;                       ///< encoder search class
  IntraSearch               m_cIntraSearch;                       ///< encoder search class
  // coding tool
  TrQuant                   m_cTrQuant;                           ///< transform & quantization class
  DeblockingFilter          m_deblockingFilter;                   ///< deblocking filter class
  EncSampleAdaptiveOffset   m_cEncSAO;                            ///< sample adaptive offset class
  EncAdaptiveLoopFilter     m_cEncALF;
  HLSWriter                 m_HLSWriter;                          ///< CAVLC encoder
  CABACEncoder              m_CABACEncoder;

  EncReshape                m_cReshaper;                        ///< reshaper class

  // processing unit
  EncGOP                    m_cGOPEncoder;                        ///< GOP encoder
  EncSlice                  m_cSliceEncoder;                      ///< slice encoder
  EncCu                     m_cCuEncoder;                         ///< CU encoder
  // SPS
  ParameterSetMap<SPS>     &m_spsMap;                             ///< SPS. This is the base value
  ParameterSetMap<PPS>     &m_ppsMap;                             ///< PPS. This is the base value
  EnumArray<ParameterSetMap<APS>, ApsType> &m_apsMaps;                            ///< APS. This is the base value
  PicHeader                 m_picHeader;                          ///< picture header
  // RD cost computation
  RdCost                    m_cRdCost;                            ///< RD cost computation class
  CtxPool                   m_ctxPool;                            ///< buffer for temporarily stored context models
  // quality control
  RateCtrl                  m_cRateCtrl;                          ///< Rate control class

  AUWriterIf*               m_AUWriterIf;

#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  CacheModel                m_cacheModel;
#endif

  APS*                      m_apss[ALF_CTB_MAX_NUM_APS];

  APS*                      m_lmcsAPS;
  APS*                      m_scalinglistAPS;

  EncHRD                    m_encHRD;

  bool                      m_doPlt;
#if JVET_O0756_CALCULATE_HDRMETRICS
  std::chrono::duration<long long, std::ratio<1, 1000000000>> m_metricTime;
#endif
  int                       m_picIdInGOP;

  VPS*                      m_vps;

  int*                      m_layerDecPicBuffering;
  RPLList                   m_rplLists[2];
#if GREEN_METADATA_SEI_ENABLED
  FeatureCounterStruct             m_featureCounter;
  bool                      m_GMFAFramewise;
  std::string   m_GMFAFile;
#endif
  EncTemporalFilter         m_temporalFilter;
  EncTemporalFilter         m_temporalFilterForFG;
  SEINeuralNetworkPostFiltering m_nnPostFiltering;
  EncType                   m_encType;
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  SEIPackedRegionsInfoProcess m_priProcess;
#endif
public:
  SPS*                      getSPS( int spsId ) { return m_spsMap.getPS( spsId ); };
  APS**                     getApss() { return m_apss; }
  Ctx                       m_entropyCodingSyncContextState;      ///< leave in addition to vector for compatibility
  PLTBuf                    m_palettePredictorSyncState;
  const RPLList            *getRplList(RefPicList l) const { return &m_rplLists[l]; }
  RPLList                  *getRplList(RefPicList l) { return &m_rplLists[l]; }
  uint32_t                  getNumRpl(RefPicList l) const { return m_rplLists[l].getNumberOfReferencePictureLists(); }
  bool                      m_rprPPSCodedAfterIntraList[NUM_RPR_PPS]; // 4 resolutions, full, 5/6, 2/3 and 1/2
  bool                      m_refLayerRescaledAvailable;
protected:
  void  xGetNewPicBuffer  ( std::list<PelUnitBuf*>& rcListPicYuvRecOut, Picture*& rpcPic, int ppsId ); ///< get picture buffer which will be processed. If ppsId<0, then the ppsMap will be queried for the first match.
  void  xInitOPI(OPI& opi); ///< initialize Operating point Information (OPI) from encoder options
  void  xInitDCI(DCI& dci, const SPS& sps); ///< initialize Decoding Capability Information (DCI) from encoder options
  void  xInitVPS( const SPS& sps ); ///< initialize VPS from encoder options
  void  xInitSPS( SPS& sps );       ///< initialize SPS from encoder options
  void  xInitPPS          (PPS &pps, const SPS &sps); ///< initialize PPS from encoder options
  void  xInitPicHeader    (PicHeader &picHeader, const SPS &sps, const PPS &pps); ///< initialize Picture Header from encoder options
  void  xInitAPS          (APS &aps);                 ///< initialize APS from encoder options
  void  xInitScalingLists ( SPS &sps, APS *aps );     ///< initialize scaling lists
  void  xInitPPSforLT(PPS& pps);
  void  xInitHrdParameters(SPS &sps);                 ///< initialize HRDParameters parameters

  void xInitRPL(SPS &sps);   ///< initialize SPS from encoder options

public:
  EncLib( EncLibCommon* encLibCommon );
  virtual ~EncLib();

  void      create          ( const int layerId );
  void      destroy         ();
  void      init(AUWriterIf *auWriterIf);
  void      deletePicBuffer ();

  // -------------------------------------------------------------------------------------------------------------------
  // member access functions
  // -------------------------------------------------------------------------------------------------------------------

  AUWriterIf*             getAUWriterIf         ()              { return   m_AUWriterIf;           }
  PicList*                getListPic            ()              { return  &m_cListPic;             }
  InterSearch*            getInterSearch        ()              { return  &m_cInterSearch;         }
  IntraSearch*            getIntraSearch        ()              { return  &m_cIntraSearch;         }

  TrQuant*                getTrQuant            ()              { return  &m_cTrQuant;             }
  DeblockingFilter*       getDeblockingFilter   ()              { return  &m_deblockingFilter;     }
  EncSampleAdaptiveOffset* getSAO               ()              { return  &m_cEncSAO;              }
  EncAdaptiveLoopFilter*  getALF                ()              { return  &m_cEncALF;              }
  EncGOP*                 getGOPEncoder         ()              { return  &m_cGOPEncoder;          }
  EncSlice*               getSliceEncoder       ()              { return  &m_cSliceEncoder;        }
  EncHRD*                 getHRD                ()              { return  &m_encHRD;               }
  EncCu*                  getCuEncoder          ()              { return  &m_cCuEncoder;           }
  HLSWriter*              getHLSWriter          ()              { return  &m_HLSWriter;            }
  CABACEncoder*           getCABACEncoder       ()              { return  &m_CABACEncoder;         }

  RdCost*                 getRdCost             ()              { return  &m_cRdCost;              }
  CtxPool                *getCtxCache() { return &m_ctxPool; }
  RateCtrl*               getRateCtrl           ()              { return  &m_cRateCtrl;            }
  void                    setRefLayerRescaledAvailable(bool b)  { m_refLayerRescaledAvailable = b; }
  bool                    isRefLayerRescaledAvailable() const   { return m_refLayerRescaledAvailable; }

#if GREEN_METADATA_SEI_ENABLED
  FeatureCounterStruct getFeatureCounter(){return m_featureCounter;}
  void setFeatureCounter(FeatureCounterStruct b){m_featureCounter=b;}
  bool getGMFAFramewise() {return m_GMFAFramewise;}
  void setGMFAFile(std::string b){m_GMFAFile = b;}
#endif

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  SEIPackedRegionsInfoProcess& getPriProcess() { return m_priProcess; }
#endif

  void selectReferencePictureList(Slice *slice, int pocCurr, int gopId, int ltPoc);

  void                   setParamSetChanged(int spsId, int ppsId);
  bool                   PPSNeedsWriting(int ppsId);
  bool                   SPSNeedsWriting(int spsId);
  const PPS* getPPS( int Id ) { return m_ppsMap.getPS( Id); }
  const APS             *getAPS(int Id, ApsType apsType) { return m_apsMaps[apsType].getPS(Id); }

  EncReshape*            getReshaper()                          { return  &m_cReshaper; }

  ParameterSetMap<APS>                     *getApsMap(ApsType apsType) { return &m_apsMaps[apsType]; }
  EnumArray<ParameterSetMap<APS>, ApsType> *getApsMaps() { return &m_apsMaps; }

  EncTemporalFilter&     getTemporalFilter() { return m_temporalFilter; }
  EncTemporalFilter&     getTemporalFilterForFG() { return m_temporalFilterForFG; }
  void                   setRprPPSCodedAfterIntra(int num, bool isCoded) { m_rprPPSCodedAfterIntraList[num] = isCoded; }
  bool                   getRprPPSCodedAfterIntra(int num) { return m_rprPPSCodedAfterIntraList[num]; }

  bool                   getPltEnc()                      const { return   m_doPlt; }
  void                   checkPltStats( Picture* pic );
  EncType                getEncType()          const { return m_encType; }
  void                   setEncType(EncType enctype) { m_encType = enctype; }
#if JVET_O0756_CALCULATE_HDRMETRICS
  std::chrono::duration<long long, std::ratio<1, 1000000000>> getMetricTime() const { return m_metricTime; };
#endif
  // -------------------------------------------------------------------------------------------------------------------
  // encoder function
  // -------------------------------------------------------------------------------------------------------------------

  // encode several number of pictures until end-of-sequence
  // snrCSC used for SNR calculations. Picture in original colour space.
  bool encodePrep(bool flush, PelStorage *pcPicYuvOrg, const InputColourSpaceConversion snrCSC,
                  std::list<PelUnitBuf *> &rcListPicYuvRecOut, int &numEncoded, PelStorage** ppcPicYuvRPR);

  bool encode(const InputColourSpaceConversion snrCSC, std::list<PelUnitBuf *> &rcListPicYuvRecOut, int &numEncoded);

  bool encodePrep(bool flush, PelStorage *pcPicYuvOrg, const InputColourSpaceConversion snrCSC, 
    std::list<PelUnitBuf *> &rcListPicYuvRecOut, int &numEncoded,
                  bool isTff);

  bool encode(const InputColourSpaceConversion snrCSC, std::list<PelUnitBuf *> &rcListPicYuvRecOut, int &numEncoded,
              bool isTff);

  void applyNnPostFilter();

  void printSummary(bool isField)
  {
    m_cGOPEncoder.printOutSummary(m_codedPicCount, isField, m_printMSEBasedSequencePSNR, m_printSequenceMSE,
                                  m_printMSSSIM, m_printHexPsnr, (m_resChangeInClvsEnabled || m_refLayerRescaledAvailable),
                                  m_spsMap.getFirstPS()->getBitDepths(), m_layerId);

  }

  int getLayerId() const { return m_layerId; }
  VPS* getVPS()          { return m_vps;     }
};

//! \}

#endif // __ENCTOP__

