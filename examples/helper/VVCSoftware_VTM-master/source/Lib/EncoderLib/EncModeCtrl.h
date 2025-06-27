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

/** \file     EncModeCtrl.h
    \brief    Encoder controller for trying out specific modes
*/

#ifndef __ENCMODECTRL__
#define __ENCMODECTRL__

// Include files
#include "EncCfg.h"

#include "CommonLib/CommonDef.h"
#include "CommonLib/CodingStructure.h"
#include "InterSearch.h"

#include <typeinfo>
#include <vector>

//////////////////////////////////////////////////////////////////////////
// Encoder modes to try out
//////////////////////////////////////////////////////////////////////////


enum EncTestModeType
{
  ETM_HASH_INTER,
  ETM_MERGE_SKIP,
  ETM_INTER_ME,
  ETM_INTRA,
  ETM_PALETTE,
  ETM_SPLIT_QT,
  ETM_SPLIT_BT_H,
  ETM_SPLIT_BT_V,
  ETM_SPLIT_TT_H,
  ETM_SPLIT_TT_V,
  ETM_POST_DONT_SPLIT, // dummy mode to collect the data from the unsplit coding
#if REUSE_CU_RESULTS
  ETM_RECO_CACHED,
#endif
  ETM_TRIGGER_IMV_LIST,
  ETM_IBC,    // ibc mode
  ETM_IBC_MERGE, // ibc merge mode
  ETM_INVALID
};

enum EncTestModeOpts
{
  ETO_STANDARD    =  0,                   // empty      (standard option)
  ETO_FORCE_MERGE =  1<<0,                // bit   0    (indicates forced merge)
  ETO_IMV_SHIFT   =     1,                // bits  1-3  (imv parameter starts at bit 1)
  ETO_IMV         =  7<<ETO_IMV_SHIFT,    // bits  1-3  (imv parameter uses 3 bits)
  ETO_DUMMY       =  1<<5,                // bit   5    (dummy)
  ETO_INVALID     = 0xffffffff            // bits 0-31  (invalid option)
};

enum ExtraFeatures
{
  DID_HORZ_SPLIT = 0,
  DID_VERT_SPLIT,
  DID_QUAD_SPLIT,
  BEST_HORZ_SPLIT_COST,
  BEST_VERT_SPLIT_COST,
  BEST_TRIH_SPLIT_COST,
  BEST_TRIV_SPLIT_COST,
  DO_TRIH_SPLIT,
  DO_TRIV_SPLIT,
  BEST_NON_SPLIT_COST,
  BEST_NO_IMV_COST,
  BEST_IMV_COST,
  QT_BEFORE_BT,
  IS_BEST_NOSPLIT_SKIP,
  MAX_QT_SUB_DEPTH,
#if REUSE_CU_RESULTS
  IS_REUSING_CU,
#endif
  NUM_EXTRA_FEATURES
};

static void getAreaIdx(const Area& area, const PreCalcValues &pcv, unsigned &idx1, unsigned &idx2, unsigned &idx3, unsigned &idx4)
{
  idx1 = (area.x & pcv.maxCUWidthMask)  >> MIN_CU_LOG2;
  idx2 = (area.y & pcv.maxCUHeightMask) >> MIN_CU_LOG2;
  idx3 = gp_sizeIdxInfo->idxFrom( area.width  );
  idx4 = gp_sizeIdxInfo->idxFrom( area.height );
}

struct EncTestMode
{
  enum struct AmvrSearchMode
  {
    NONE,
    FULL_PEL,
    FOUR_PEL,
    FOUR_PEL_FAST,
    HALF_PEL
  };

  EncTestMode()
    : type( ETM_INVALID ), opts( ETO_INVALID  ), qp( -1  ) {}
  EncTestMode( EncTestModeType _type )
    : type( _type       ), opts( ETO_STANDARD ), qp( -1  ) {}
  EncTestMode( EncTestModeType _type, int _qp )
    : type( _type       ), opts( ETO_STANDARD ), qp( _qp ) {}
  EncTestMode( EncTestModeType _type, EncTestModeOpts _opts, int _qp )
    : type( _type       ), opts( _opts        ), qp( _qp ) {}

  EncTestModeType type;
  EncTestModeOpts opts;
  int             qp;
  double          maxCostAllowed;

  AmvrSearchMode getAmvrSearchMode() const { return AmvrSearchMode((opts & ETO_IMV) >> ETO_IMV_SHIFT); }
};


inline bool isModeSplit( const EncTestMode& encTestmode )
{
  switch( encTestmode.type )
  {
  case ETM_SPLIT_QT     :
  case ETM_SPLIT_BT_H   :
  case ETM_SPLIT_BT_V   :
  case ETM_SPLIT_TT_H   :
  case ETM_SPLIT_TT_V   :
    return true;
  default:
    return false;
  }
}

inline bool isModeNoSplit( const EncTestMode& encTestmode )
{
  return !isModeSplit( encTestmode ) && encTestmode.type != ETM_POST_DONT_SPLIT;
}

inline bool isModeInter( const EncTestMode& encTestmode ) // perhaps remove
{
  return (   encTestmode.type == ETM_INTER_ME
          || encTestmode.type == ETM_MERGE_SKIP
          || encTestmode.type == ETM_HASH_INTER
         );
}

inline PartSplit getPartSplit( const EncTestMode& encTestmode )
{
  switch( encTestmode.type )
  {
  case ETM_SPLIT_QT     : return CU_QUAD_SPLIT;
  case ETM_SPLIT_BT_H   : return CU_HORZ_SPLIT;
  case ETM_SPLIT_BT_V   : return CU_VERT_SPLIT;
  case ETM_SPLIT_TT_H   : return CU_TRIH_SPLIT;
  case ETM_SPLIT_TT_V   : return CU_TRIV_SPLIT;
  default:                return CU_DONT_SPLIT;
  }
}

inline EncTestMode getCSEncMode( const CodingStructure& cs )
{
  return EncTestMode( EncTestModeType( (unsigned)cs.features[ENC_FT_ENC_MODE_TYPE] ),
                      EncTestModeOpts( (unsigned)cs.features[ENC_FT_ENC_MODE_OPTS] ),
                      false);
}



//////////////////////////////////////////////////////////////////////////
// EncModeCtrl controls if specific modes should be tested
//////////////////////////////////////////////////////////////////////////

struct IspPredModeVal
{
  uint16_t valid : 1;
  uint16_t notIsp : 1;
  uint16_t verIsp : 1;
  uint16_t ispLfnstIdx : 2;
  uint16_t mipFlag : 1;
  uint16_t lowIspCost : 1;
  uint16_t bestPredModeDCT2 : 9;
};

struct ComprCUCtx
{
  ComprCUCtx() : testModes(), extraFeatures()
  {
  }

  ComprCUCtx(const CodingStructure &cs, const uint32_t _minDepth, const uint32_t _maxDepth)
    : minDepth(_minDepth), maxDepth(_maxDepth), testModes(), lastTestMode()
  {
    getAreaIdx( cs.area.Y(), *cs.pcv, cuX, cuY, cuW, cuH );
    partIdx = ( ( cuX << 8 ) | cuY );

    extraFeatures.fill(0);
    extraFeaturesd.fill(0.0);
  }

  unsigned                 minDepth;
  unsigned                 maxDepth;
  unsigned                 cuX, cuY, cuW, cuH, partIdx;
  std::vector<EncTestMode> testModes;
  EncTestMode              lastTestMode;
  CodingStructure         *bestCS{ nullptr };
  CodingUnit              *bestCU{ nullptr };
  TransformUnit           *bestTU{ nullptr };

  std::array<int64_t, NUM_EXTRA_FEATURES> extraFeatures;
  std::array<double, NUM_EXTRA_FEATURES>  extraFeaturesd;

  double bestInterCost{ MAX_DOUBLE };
  double bestMtsSize2Nx2N1stPass{ MAX_DOUBLE };
  double bestCostWithoutSplitFlags{ MAX_DOUBLE };
  double bestCostMtsFirstPassNoIsp{ MAX_DOUBLE };
  double bestCostIsp{ MAX_DOUBLE };
  double bestDCT2NonISPCost{ MAX_DOUBLE };
  double bestNonDCT2Cost{ MAX_DOUBLE };

  Distortion interHad{ std::numeric_limits<Distortion>::max() };

  uint16_t       bestPredModeDCT2{ UINT8_MAX };
  IspPredModeVal ispPredModeVal{ 0, 0, 0, 0, 0, 0, 0 };

  bool earlySkip{ false };
  bool isHashPerfectMatch{ false };
  bool skipSecondMTSPass{ false };
  bool ispWasTested{ false };
  bool relatedCuIsValid{ false };
  bool mipFlag{ false };
  bool stopNonDCT2Transforms{ false };

  uint8_t bestISPIntraMode{ NOMODE_IDX };
  ISPType ispMode{ ISPType::NONE };
  uint8_t ispLfnstIdx{ 0 };

  template<typename T> T    get( int ft )       const { return typeid(T) == typeid(double) ? (T&)extraFeaturesd[ft] : T(extraFeatures[ft]); }
  template<typename T> void set( int ft, T val )      { extraFeatures [ft] = int64_t( val ); }
  void                      set( int ft, double val ) { extraFeaturesd[ft] = val; }
};

//////////////////////////////////////////////////////////////////////////
// EncModeCtrl - abstract class specifying the general flow of mode control
//////////////////////////////////////////////////////////////////////////

class EncModeCtrl
{
protected:

  const EncCfg         *m_pcEncCfg;
  const class RateCtrl *m_pcRateCtrl;
        class RdCost   *m_pcRdCost;
  const Slice          *m_slice;
#if SHARP_LUMA_DELTA_QP
  int                   m_lumaLevelToDeltaQPLUT[LUMA_LEVEL_TO_DQP_LUT_MAXSIZE];
  int                   m_lumaQPOffset;
#endif
  std::map<int, int*>  *m_bimQPMap;
  bool                  m_fastDeltaQP;
  static_vector<ComprCUCtx, ( MAX_CU_DEPTH << 2 )> m_ComprCUCtxList;
  InterSearch*          m_pcInterSearch;

  bool                  m_doPlt;

  bool                  m_useHashMeInCurrentIntraPeriod;
  int                   m_HashMEPOC;
  bool                  m_HashMEPOCchecked;
  int                   m_HashMEPOC2;

  double                m_noSplitIntraRdCost;
  int                   m_qpCtu;
  const UnitArea*       m_currCsArea;

public:

  virtual ~EncModeCtrl              () {}

  virtual void create               ( const EncCfg& cfg )                                                                   = 0;
  virtual void destroy              ()                                                                                      = 0;
  virtual void initCTUEncoding      ( const Slice &slice )                                                                  = 0;
  virtual void initCULevel          ( Partitioner &partitioner, const CodingStructure& cs )                                 = 0;
  virtual void finishCULevel        ( Partitioner &partitioner )                                                            = 0;

protected:

  virtual bool tryMode              ( const EncTestMode& encTestmode, const CodingStructure &cs, Partitioner& partitioner ) = 0;

public:

  virtual bool useModeResult        ( const EncTestMode& encTestmode, CodingStructure*& tempCS,  Partitioner& partitioner ) = 0;
  virtual bool checkSkipOtherLfnst  ( const EncTestMode& encTestmode, CodingStructure*& tempCS,  Partitioner& partitioner ) = 0;

  void         init                 ( EncCfg *pCfg, RateCtrl *pRateCtrl, RdCost *pRdCost );
  bool         tryModeMaster        ( const EncTestMode& encTestmode, const CodingStructure &cs, Partitioner& partitioner );
  bool         nextMode             ( const CodingStructure &cs, Partitioner &partitioner );
  EncTestMode  currTestMode         () const;
  EncTestMode  lastTestMode         () const;
  void         setEarlySkipDetected ();
  void         setIsHashPerfectMatch( bool b ) { m_ComprCUCtxList.back().isHashPerfectMatch = b; }
  bool         getIsHashPerfectMatch() { return m_ComprCUCtxList.back().isHashPerfectMatch; }
  void         setUseHashME(bool b) { m_useHashMeInCurrentIntraPeriod = b; }
  bool         getUseHashME()                  const { return m_useHashMeInCurrentIntraPeriod; }
  void         setUseHashMEPOCToCheck(int thePOCtoCheck) { m_HashMEPOC = thePOCtoCheck; }
  int          getUseHashMEPOCToCheck() { return m_HashMEPOC; }
  void         setUseHashMEPOCChecked(bool thePOCChecked) { m_HashMEPOCchecked = thePOCChecked; }
  bool         getUseHashMEPOCChecked() { return m_HashMEPOCchecked; }
  void         setUseHashMENextPOCToCheck(int thePOCtoCheck) { m_HashMEPOC2 = thePOCtoCheck; }
  int          getUseHashMENextPOCToCheck() { return m_HashMEPOC2; }
  virtual void setBest              ( CodingStructure& cs );
  bool         anyMode              () const;

  void         setNoSplitIntraCost  (double cost) { m_noSplitIntraRdCost = cost; }
  const ComprCUCtx& getComprCUCtx   () { CHECK( m_ComprCUCtxList.empty(), "Accessing empty list!"); return m_ComprCUCtxList.back(); }

#if SHARP_LUMA_DELTA_QP
  void                  initLumaDeltaQpLUT();
  int                   calculateLumaDQP  ( const CPelBuf& rcOrg );
#endif
  int                                 calculateLumaDQPsmooth(const CPelBuf& rcOrg, int baseQP, double threshold, double scale, double offset, int limit);
  void setFastDeltaQp                 ( bool b )                {        m_fastDeltaQP = b;                               }
  bool getFastDeltaQp                 ()                  const { return m_fastDeltaQP;                                   }

  double getBestInterCost             ()                  const { return m_ComprCUCtxList.back().bestInterCost;           }
  Distortion getInterHad              ()                  const { return m_ComprCUCtxList.back().interHad;                }
  void enforceInterHad                ( Distortion had )        {        m_ComprCUCtxList.back().interHad = had;          }
  double getMtsSize2Nx2NFirstPassCost ()                  const { return m_ComprCUCtxList.back().bestMtsSize2Nx2N1stPass; }
  bool   getSkipSecondMTSPass         ()                  const { return m_ComprCUCtxList.back().skipSecondMTSPass;       }
  void   setSkipSecondMTSPass         ( bool b )                { m_ComprCUCtxList.back().skipSecondMTSPass = b;          }
  double getBestCostWithoutSplitFlags ()                  const { return m_ComprCUCtxList.back().bestCostWithoutSplitFlags;         }
  void   setBestCostWithoutSplitFlags ( double cost )           { m_ComprCUCtxList.back().bestCostWithoutSplitFlags = cost;         }
  double getMtsFirstPassNoIspCost     ()                  const { return m_ComprCUCtxList.back().bestCostMtsFirstPassNoIsp;         }
  void   setMtsFirstPassNoIspCost     ( double cost )           { m_ComprCUCtxList.back().bestCostMtsFirstPassNoIsp = cost;         }
  double getIspCost                   ()                  const { return m_ComprCUCtxList.back().bestCostIsp; }
  void   setIspCost                   ( double val )            { m_ComprCUCtxList.back().bestCostIsp = val; }
  bool   getISPWasTested              ()                  const { return m_ComprCUCtxList.back().ispWasTested; }
  void   setISPWasTested              ( bool val )              { m_ComprCUCtxList.back().ispWasTested = val; }
  void   setBestPredModeDCT2          ( uint16_t intraMode, bool mipFlag )          
  { 
    m_ComprCUCtxList.back().bestPredModeDCT2 = intraMode; 
    m_ComprCUCtxList.back().mipFlag = mipFlag; 
  }
  bool   getRelatedCuIsValid          ()                  const { return m_ComprCUCtxList.back().relatedCuIsValid; }
  void   setRelatedCuIsValid          ( bool val )              { m_ComprCUCtxList.back().relatedCuIsValid = val; }
  IspPredModeVal getIspPredModeValRelCU() const
  {
    return m_ComprCUCtxList.back().ispPredModeVal;
  }
  double getBestDCT2NonISPCostRelCU   ()                  const { return m_ComprCUCtxList.back().bestDCT2NonISPCost; }
  void   setBestDCT2NonISPCostRelCU   ( double val )            { m_ComprCUCtxList.back().bestDCT2NonISPCost = val; }
  double getBestNonDCT2Cost           ()                  const { return m_ComprCUCtxList.back().bestNonDCT2Cost; }
  void   setBestNonDCT2Cost           ( double val )            { m_ComprCUCtxList.back().bestNonDCT2Cost = val; }
  uint8_t getBestISPIntraModeRelCU    ()                  const { return m_ComprCUCtxList.back().bestISPIntraMode; }
  void   setBestISPIntraModeRelCU     ( uint8_t val )           { m_ComprCUCtxList.back().bestISPIntraMode = val; }

  void setISPMode(const ISPType val)
  {
    m_ComprCUCtxList.back().ispMode = val;
  }

  void   setISPLfnstIdx               ( uint8_t val )           { m_ComprCUCtxList.back().ispLfnstIdx = val; }
  bool   getStopNonDCT2Transforms     ()                  const { return m_ComprCUCtxList.back().stopNonDCT2Transforms; }
  void   setStopNonDCT2Transforms     ( bool val )              { m_ComprCUCtxList.back().stopNonDCT2Transforms = val; }
  void setInterSearch                 (InterSearch* pcInterSearch)   { m_pcInterSearch = pcInterSearch; }
  void   setPltEnc                    ( bool b )                { m_doPlt = b; }
  bool   getPltEnc()                                      const { return m_doPlt; }
  void   setBIMQPMap                  ( std::map<int, int*> *qpMap ) { m_bimQPMap = qpMap; }
  int    getBIMOffset                 ( int poc, int ctuId )
  {
    auto it = m_bimQPMap->find(poc);
    return (it == m_bimQPMap->end()) ? 0 : (*m_bimQPMap)[poc][ctuId];
  }

  void setQpCtu                       ( int qp )                     { m_qpCtu = qp; }
  void setCurrCsArea                  ( const UnitArea &currCsArea ) { m_currCsArea = &currCsArea; }

#if GDR_ENABLED
void forceIntraMode()
{
  // remove all inter or split to force make intra
  int n = (int) m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (isModeInter(etm.type))
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void forceIntraNoSplit()
{
  // remove all inter or split to force make intra
  int n = (int)m_ComprCUCtxList.back().testModes.size();

  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (isModeInter(etm.type) || isModeSplit(etm.type))
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

// Note: ForceInterMode
void forceInterMode()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];
    if (etm.type == ETM_INTRA)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void removeHashInter()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];
    if (etm.type == ETM_HASH_INTER)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void removeMergeSkip()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];
    if (etm.type == ETM_MERGE_SKIP)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void removeInterME()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];
    if (etm.type == ETM_INTER_ME)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}


void removeIntra()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];
    if (etm.type == ETM_INTRA)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

bool anyPredModeLeft()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (etm.type == ETM_HASH_INTER || etm.type == ETM_MERGE_SKIP || etm.type == ETM_INTER_ME 
        || etm.type == ETM_INTRA || etm.type == ETM_PALETTE || etm.type == ETM_IBC
        || etm.type == ETM_IBC_MERGE)
    {
      return true;
    }
  }

  return false;
}

bool anyIntraIBCMode()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (etm.type == ETM_INTRA || etm.type == ETM_IBC)
    {
      return true;
    }
  }

  return false;
}

void forceRemoveDontSplit()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (etm.type == ETM_POST_DONT_SPLIT)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void forceVerSplitOnly()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (etm.type != ETM_SPLIT_QT && etm.type != ETM_SPLIT_BT_V && etm.type != ETM_SPLIT_TT_V)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void forceRemoveTTV()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (etm.type == ETM_SPLIT_TT_V)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void forceRemoveBTV()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (etm.type == ETM_SPLIT_BT_V)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void forceRemoveQT()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (etm.type == ETM_SPLIT_QT)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void forceRemoveHT()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (etm.type == ETM_SPLIT_BT_H || etm.type == ETM_SPLIT_TT_H)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void forceRemoveQTHT()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (etm.type == ETM_SPLIT_QT || etm.type == ETM_SPLIT_BT_H || etm.type == ETM_SPLIT_TT_H)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void forceRemoveAllSplit()
{
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (etm.type == ETM_SPLIT_QT || etm.type == ETM_SPLIT_BT_H || etm.type == ETM_SPLIT_BT_V
        || etm.type == ETM_SPLIT_TT_H || etm.type == ETM_SPLIT_TT_V)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

void forceQTonlyMode()
{
  // remove all split except QT
  int n = (int)m_ComprCUCtxList.back().testModes.size();
  for (int j = 0; j < n; j++)
  {
    const EncTestMode etm = m_ComprCUCtxList.back().testModes[j];

    if (etm.type != ETM_SPLIT_QT)
    {
      m_ComprCUCtxList.back().testModes.erase(m_ComprCUCtxList.back().testModes.begin() + j);
      j--;
      n--;
    }
  }
}

#endif

protected:
  void xExtractFeatures ( const EncTestMode encTestmode, CodingStructure& cs );
  void xGetMinMaxQP     ( int& iMinQP, int& iMaxQP, const CodingStructure& cs, const Partitioner &pm, const int baseQP, const SPS& sps, const PPS& pps, const PartSplit splitMode );
  int  xComputeDQP      ( const CodingStructure &cs, const Partitioner &pm );
};


//////////////////////////////////////////////////////////////////////////
// some utility interfaces that expose some functionality that can be used without concerning about which particular controller is used
//////////////////////////////////////////////////////////////////////////
struct SaveLoadStructSbt
{
  uint8_t  numPuInfoStored;
  uint32_t puSse[SBT_NUM_SL];
  uint8_t  puSbt[SBT_NUM_SL];
  MtsType  puTrs[SBT_NUM_SL];
};

class SaveLoadEncInfoSbt
{
protected:
  void init( const Slice &slice );
  void create(int maxCuSize);
  void destroy();

private:
  SaveLoadStructSbt ****m_saveLoadSbt;
  Slice const       *m_sliceSbt;
  int m_maxCuSize;

public:
  virtual  ~SaveLoadEncInfoSbt() { }
  void     resetSaveloadSbt( int maxSbtSize );

  struct BestSbt
  {
    uint8_t sbt;
    MtsType trs;
  };

  BestSbt findBestSbt(const UnitArea &area, const uint32_t curPuSse);
  bool    saveBestSbt(const UnitArea &area, const uint32_t curPuSse, const uint8_t curPuSbt, const MtsType curPuTrs);
};

static constexpr int MAX_STORED_CU_INFO_REFS = 4;

struct CodedCUInfo
{
  bool isInter;
  bool isIntra;
  bool isSkip;
  bool isMMVDSkip;
  bool isIBC;
  bool validMv[NUM_REF_PIC_LIST_01][MAX_STORED_CU_INFO_REFS];
  Mv   saveMv [NUM_REF_PIC_LIST_01][MAX_STORED_CU_INFO_REFS];

  uint8_t bcwIdx;
  char selectColorSpaceOption;   // 0 - test both two color spaces; 1 - only test the first color spaces; 2 - only test
                                 // the second color spaces
  IspPredModeVal ispPredModeVal;
  double   bestDCT2NonISPCost;
  double   bestCost;
  double   bestNonDCT2Cost;
  bool     relatedCuIsValid;
  uint8_t  bestISPIntraMode;
};

class CacheBlkInfoCtrl
{
private:

  unsigned         m_numWidths, m_numHeights;
  Slice const     *m_slice_chblk;
  // x in CTU, y in CTU, width, height
  CodedCUInfo   ***m_codedCUInfo[MAX_CU_SIZE >> MIN_CU_LOG2][MAX_CU_SIZE >> MIN_CU_LOG2];
  int m_maxCuSize;

protected:

  void create   (int maxCuSize);
  void destroy  ();
  void init     ( const Slice &slice );

  CodedCUInfo& getBlkInfo( const UnitArea& area );

public:

  virtual ~CacheBlkInfoCtrl() {}

  bool isSkip ( const UnitArea& area );
  bool isMMVDSkip(const UnitArea& area);
  bool getMv(const UnitArea &area, const RefPicList refPicList, const int refIdx, Mv &rMv) const;
  void setMv(const UnitArea &area, const RefPicList refPicList, const int refIdx, const Mv &rMv);

  bool  getInter( const UnitArea& area );
  void  setBcwIdx( const UnitArea& area, uint8_t gBiIdx );
  uint8_t getBcwIdx( const UnitArea& area );

  char  getSelectColorSpaceOption(const UnitArea& area);
};

#if REUSE_CU_RESULTS
struct BestEncodingInfo
{
  CodingUnit     cu;
  PredictionUnit pu;
#if REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
  TransformUnit  tus[MAX_NUM_TUS];
  size_t         numTus;
#else
  TransformUnit  tu;
#endif
  EncTestMode    testMode;

  int            poc;
};

class BestEncInfoCache
{
private:

  unsigned            m_numWidths, m_numHeights;
  int                 m_sliceQp = -128;
  const Slice        *m_slice_bencinf;
  BestEncodingInfo ***m_bestEncInfo[MAX_CU_SIZE >> MIN_CU_LOG2][MAX_CU_SIZE >> MIN_CU_LOG2];
  TCoeff             *m_pCoeff;
  Pel                *m_pPcmBuf;
  PLTRunMode*         m_runType;
  XuPool              m_dummyPool;
  CodingStructure     m_dummyCS;
  int                 m_maxCuSize;

protected:

  void create   ( const ChromaFormat chFmt, int maxCuSize);
  void destroy  ();

  bool setFromCs( const CodingStructure& cs, const Partitioner& partitioner );
  bool isValid  ( const CodingStructure &cs, const Partitioner &partitioner, int qp );
public:
  BestEncInfoCache() : m_slice_bencinf(nullptr), m_dummyCS(m_dummyPool) {}
  virtual ~BestEncInfoCache() {}
  void     init     ( const Slice &slice );
  bool     setCsFrom( CodingStructure& cs, EncTestMode& testMode, const Partitioner& partitioner ) const;
};

#endif
//////////////////////////////////////////////////////////////////////////
// EncModeCtrlMTnoRQT - allows and controls modes introduced by QTBT (inkl. multi-type-tree)
//                    - only 2Nx2N, no RQT, additional binary/triary CU splits
//////////////////////////////////////////////////////////////////////////

class EncModeCtrlMTnoRQT : public EncModeCtrl, public CacheBlkInfoCtrl
#if REUSE_CU_RESULTS
  , public BestEncInfoCache
#endif
  , public SaveLoadEncInfoSbt
{
  unsigned m_skipThreshold;
#if GDR_ENABLED
  EncCfg m_encCfg;
#endif

public:

  virtual void create             ( const EncCfg& cfg );
  virtual void destroy            ();
  virtual void initCTUEncoding    ( const Slice &slice );
  virtual void initCULevel        ( Partitioner &partitioner, const CodingStructure& cs );
  virtual void finishCULevel      ( Partitioner &partitioner );

  virtual bool tryMode            ( const EncTestMode& encTestmode, const CodingStructure &cs, Partitioner& partitioner );
  virtual bool useModeResult      ( const EncTestMode& encTestmode, CodingStructure*& tempCS,  Partitioner& partitioner );

  virtual bool checkSkipOtherLfnst( const EncTestMode& encTestmode, CodingStructure*& tempCS, Partitioner& partitioner );

  bool xSkipTreeCandidate(const PartSplit split, const double* splitRdCostBest, const SliceType& sliceType) const;
};

//! \}

#endif // __ENCMODECTRL__
