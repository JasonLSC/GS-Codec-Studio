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

#pragma once

#include <array>
#include <cstring>
#include <list>
#include <map>
#include <vector>
#include <unordered_map>
#include "CommonDef.h"
#include "Rom.h"
#include "ChromaFormat.h"
#include "Common.h"
#include "HRD.h"
#include "AlfParameters.h"
#include "ConstraintInfo.h"
#include "ProfileTierLevel.h"
#include "VideoParameterSet.h"
#include "SequenceParameterSet.h"
#include "PictureParameterSet.h"

struct Picture;
class TrQuant;

typedef std::list<Picture*> PicList;


/// SCALING_LIST class
class ScalingList
{
public:
             ScalingList();
  virtual    ~ScalingList()                                                 { }

  int*       getScalingListAddress(uint32_t scalingListId)                    { return &(m_scalingListCoef[scalingListId][0]);            } //!< get matrix coefficient
  const int* getScalingListAddress(uint32_t scalingListId) const              { return &(m_scalingListCoef[scalingListId][0]);            } //!< get matrix coefficient
  void       checkPredMode(uint32_t scalingListId);

  void       setRefMatrixId(uint32_t scalingListId, uint32_t u)               { m_refMatrixId[scalingListId] = u;                         } //!< set reference matrix ID
  uint32_t       getRefMatrixId(uint32_t scalingListId) const                 { return m_refMatrixId[scalingListId];                      } //!< get reference matrix ID

  static const int* getScalingListDefaultAddress(uint32_t scalinListId);                                                                           //!< get default matrix coefficient
  void       processDefaultMatrix(uint32_t scalinListId);

  void       setScalingListDC(uint32_t scalinListId, uint32_t u)              { m_scalingListDC[scalinListId] = u;                        } //!< set DC value
  int        getScalingListDC(uint32_t scalinListId) const                    { return m_scalingListDC[scalinListId];                     } //!< get DC value

  void       setScalingListCopyModeFlag(uint32_t scalinListId, bool bIsCopy)  { m_scalingListPredModeFlagIsCopy[scalinListId] = bIsCopy;  }
  bool       getScalingListCopyModeFlag(uint32_t scalinListId) const          { return m_scalingListPredModeFlagIsCopy[scalinListId];     } //getScalingListPredModeFlag
  void       processRefMatrix(uint32_t scalingListId, uint32_t refListId);

  int        lengthUvlc(int uiCode);
  int        lengthSvlc(int uiCode);
  void       CheckBestPredScalingList(int scalingListId, int predListIdx, int& BitsCount);
  void       codePredScalingList(int* scalingList, const int* scalingListPred, int scalingListDC, int scalingListPredDC, int scalinListId, int& bitsCost);
  void       codeScalingList(int* scalingList, int scalingListDC, int scalinListId, int& bitsCost);
  void       setScalingListPreditorModeFlag(uint32_t scalingListId, bool bIsPred) { m_scalingListPreditorModeFlag[scalingListId] = bIsPred; }
  bool       getScalingListPreditorModeFlag(uint32_t scalingListId) const { return m_scalingListPreditorModeFlag[scalingListId]; }
  bool       getChromaScalingListPresentFlag() const {return m_chromaScalingListPresentFlag;}
  void       setChromaScalingListPresentFlag( bool flag) { m_chromaScalingListPresentFlag = flag;}
  bool       isLumaScalingList( int scalingListId) const;
  void       checkDcOfMatrix();
  bool       xParseScalingList(const std::string &fileName);
  void       setDefaultScalingList();
  bool       isNotDefaultScalingList();

  bool operator==( const ScalingList& other )
  {
    if (memcmp(m_scalingListPredModeFlagIsCopy, other.m_scalingListPredModeFlagIsCopy, sizeof(m_scalingListPredModeFlagIsCopy)))
    {
      return false;
    }
    if( memcmp( m_scalingListDC, other.m_scalingListDC, sizeof( m_scalingListDC ) ) )
    {
      return false;
    }
    if( memcmp( m_refMatrixId, other.m_refMatrixId, sizeof( m_refMatrixId ) ) )
    {
      return false;
    }
    if( memcmp( m_scalingListCoef, other.m_scalingListCoef, sizeof( m_scalingListCoef ) ) )
    {
      return false;
    }

    return true;
  }

  bool operator!=( const ScalingList& other )
  {
    return !( *this == other );
  }

private:
  void             outputScalingLists(std::ostream &os) const;
  bool             m_scalingListPredModeFlagIsCopy [30]; //!< reference list index
  int              m_scalingListDC                 [30]; //!< the DC value of the matrix coefficient for 16x16
  uint32_t         m_refMatrixId                   [30]; //!< RefMatrixID
  bool             m_scalingListPreditorModeFlag   [30]; //!< reference list index
  std::vector<int> m_scalingListCoef               [30]; //!< quantization matrix
  bool             m_chromaScalingListPresentFlag;
};


struct CheckCRAFlags
{
  CheckCRAFlags()
  {
    clear();
  }

  void clear()
  {
    seenTrailingFieldPic = false;
    seenLeadingFieldPic = false;
    trailingFieldHadRefIssue = false;
  }

  bool seenTrailingFieldPic;     ///< whether or not have seen trailing field picture after CRA
  bool seenLeadingFieldPic;      ///< whether or not have seen leading field picture after CRA
  bool trailingFieldHadRefIssue; ///< whether or not first trailing field picture had forbidden references pictures
};


class SliceReshapeInfo
{
public:
  bool      sliceReshaperEnableFlag;
  bool      sliceReshaperModelPresentFlag;
  unsigned  enableChromaAdj;
  uint32_t  reshaperModelMinBinIdx;
  uint32_t  reshaperModelMaxBinIdx;
  int       reshaperModelBinCWDelta[PIC_CODE_CW_BINS];
  int       maxNbitsNeededDeltaCW;
  int       chrResScalingOffset;
  void      setUseSliceReshaper(bool b)                                { sliceReshaperEnableFlag = b;            }
  bool      getUseSliceReshaper() const                                { return sliceReshaperEnableFlag;         }
  void      setSliceReshapeModelPresentFlag(bool b)                    { sliceReshaperModelPresentFlag = b;      }
  bool      getSliceReshapeModelPresentFlag() const                    { return   sliceReshaperModelPresentFlag; }
  void      setSliceReshapeChromaAdj(unsigned adj)                     { enableChromaAdj = adj;                  }
  unsigned  getSliceReshapeChromaAdj() const                           { return enableChromaAdj;                 }

  bool operator==( const SliceReshapeInfo& other )
  {
    if( sliceReshaperEnableFlag != other.sliceReshaperEnableFlag )
    {
      return false;
    }
    if( sliceReshaperModelPresentFlag != other.sliceReshaperModelPresentFlag )
    {
      return false;
    }
    if( enableChromaAdj != other.enableChromaAdj )
    {
      return false;
    }
    if( reshaperModelMinBinIdx != other.reshaperModelMinBinIdx )
    {
      return false;
    }
    if( reshaperModelMaxBinIdx != other.reshaperModelMaxBinIdx )
    {
      return false;
    }
    if( maxNbitsNeededDeltaCW != other.maxNbitsNeededDeltaCW )
    {
      return false;
    }
    if (chrResScalingOffset != other.chrResScalingOffset)
    {
      return false;
    }
    if( memcmp( reshaperModelBinCWDelta, other.reshaperModelBinCWDelta, sizeof( reshaperModelBinCWDelta ) ) )
    {
      return false;
    }

    return true;
  }

  bool operator!=( const SliceReshapeInfo& other )
  {
    return !( *this == other );
  }
};

struct ReshapeCW
{
  std::vector<uint32_t> binCW;
  int       updateCtrl;
  int       adpOption;
  uint32_t  initialCW;
  int rspPicSize;
  int rspFps;
  int rspBaseQP;
  int rspTid;
  int rspSliceQP;
  int rspFpsToIp;
};


class DCI
{
private:
  int m_maxSubLayersMinus1;
  std::vector<ProfileTierLevel> m_profileTierLevel;

public:
  DCI()
    : m_maxSubLayersMinus1(0)
  {};

  virtual ~DCI() {};

  int  getMaxSubLayersMinus1() const { return m_maxSubLayersMinus1; }
  void setMaxSubLayersMinus1(int val) { m_maxSubLayersMinus1 = val; }

  size_t getNumPTLs() const { return m_profileTierLevel.size(); }
  void  setProfileTierLevel(const std::vector<ProfileTierLevel>& val) { m_profileTierLevel = val; }
  const ProfileTierLevel& getProfileTierLevel(int idx) const { return m_profileTierLevel[idx]; }
  bool  IsIndenticalDCI(const DCI& comparedDCI) const
  {
    if(m_maxSubLayersMinus1 != comparedDCI.m_maxSubLayersMinus1) return false;
    if(m_profileTierLevel != comparedDCI.m_profileTierLevel) return false;
    return true;
  }
};

class OPI
{
private:
  bool m_olsinfopresentflag;
  bool m_htidinfopresentflag;
  uint32_t  m_opiolsidx;
  uint32_t  m_opihtidplus1;

public:
  OPI()
    : m_olsinfopresentflag (false)
    ,  m_htidinfopresentflag (false)
    ,  m_opiolsidx (-1)
    ,  m_opihtidplus1 (-1)
  {};

  virtual ~OPI() {};

  bool getOlsInfoPresentFlag() const { return m_olsinfopresentflag; }
  void setOlsInfoPresentFlag(bool val) { m_olsinfopresentflag = val; }
  bool getHtidInfoPresentFlag() const { return m_htidinfopresentflag; }
  void setHtidInfoPresentFlag(bool val) { m_htidinfopresentflag = val; }
  uint32_t getOpiOlsIdx() const { return m_opiolsidx; }
  void setOpiOlsIdx(uint32_t val) { m_opiolsidx = val; }
  uint32_t getOpiHtidPlus1() const { return m_opihtidplus1; }
  void setOpiHtidPlus1(uint32_t val) { m_opihtidplus1 = val; }

};

class APS
{
private:
  int                    m_APSId;                    // adaptation_parameter_set_id
  int                    m_temporalId;
  int                    m_puCounter;
  int                    m_layerId;
  ApsType                m_APSType;                  // aps_params_type
  AlfParam               m_alfAPSParam;
  SliceReshapeInfo       m_reshapeAPSInfo;
  ScalingList            m_scalingListApsInfo;
  CcAlfFilterParam       m_ccAlfAPSParam;
  bool                   m_hasPrefixNalUnitType;

public:
  APS();
  virtual                ~APS();

  uint8_t                getAPSId() const { return m_APSId; }
  void                   setAPSId(int i)                                                  { m_APSId = i;                                  }

  ApsType                getAPSType() const                                               { return m_APSType;                             }
  void                   setAPSType( ApsType type )                                       { m_APSType = type;                             }

  void                   setAlfAPSParam(AlfParam& alfAPSParam)                            { m_alfAPSParam = alfAPSParam;                  }
  void                   setTemporalId( int i )                                           { m_temporalId = i;                             }
  int                    getTemporalId()                                            const { return m_temporalId;                          }
  void                   setPuCounter(int i)                                              { m_puCounter = i;                              }
  int                    getPuCounter()                                             const { return m_puCounter;                           }
  void                   setLayerId( int i )                                              { m_layerId = i;                                }
  int                    getLayerId()                                               const { return m_layerId;                             }
  AlfParam&              getAlfAPSParam()  { return m_alfAPSParam; }

  void                   setReshaperAPSInfo(SliceReshapeInfo& reshapeAPSInfo)             { m_reshapeAPSInfo = reshapeAPSInfo;            }
  SliceReshapeInfo&      getReshaperAPSInfo()                                             { return m_reshapeAPSInfo;                      }
  void                   setScalingList( ScalingList& scalingListAPSInfo )                { m_scalingListApsInfo = scalingListAPSInfo;    }
  ScalingList&           getScalingList()                                                 { return m_scalingListApsInfo;                  }
  void                   setCcAlfAPSParam(CcAlfFilterParam& ccAlfAPSParam)                { m_ccAlfAPSParam = ccAlfAPSParam;              }
  CcAlfFilterParam&      getCcAlfAPSParam()  { return m_ccAlfAPSParam; }
  void                   setHasPrefixNalUnitType( bool b )                                { m_hasPrefixNalUnitType = b;                   }
  bool                   getHasPrefixNalUnitType() const                                  { return m_hasPrefixNalUnitType;                }
  bool chromaPresentFlag;
};

struct WPScalingParam
{
  // Explicit weighted prediction parameters parsed in slice header,
  // or Implicit weighted prediction parameters (8 bits depth values).
  bool     presentFlag;
  uint32_t log2WeightDenom;
  int      codedWeight;
  int      codedOffset;

  // Weighted prediction scaling values built from above parameters (bitdepth scaled):
  int  w;
  int  o;
  int  offset;
  int  shift;
  int  round;

  static bool isWeighted(const WPScalingParam *wp);
};

inline bool WPScalingParam::isWeighted(const WPScalingParam *wp)
{
  return wp != nullptr && (wp[COMPONENT_Y].presentFlag || wp[COMPONENT_Cb].presentFlag || wp[COMPONENT_Cr].presentFlag);
}

struct WPACDCParam
{
  int64_t ac;
  int64_t dc;
};

// picture header class
class PicHeader
{
private:
  bool                        m_valid;                                                  //!< picture header is valid yet or not
  Picture*                    m_pcPic;                                                  //!< pointer to picture structure
  int                         m_pocLsb;                                                 //!< least significant bits of picture order count
  bool                        m_nonReferencePictureFlag;                                //!< non-reference picture flag
  bool                        m_gdrOrIrapPicFlag;                                       //!< gdr or irap picture flag
  bool                        m_gdrPicFlag;                                             //!< gradual decoding refresh picture flag
#if GDR_ENABLED
  bool                        m_inGdrInterval;
  int                         m_lastGdrIntervalPoc;
#endif
  uint32_t                    m_recoveryPocCnt;                                         //!< recovery POC count
  bool                        m_noOutputBeforeRecoveryFlag;                             //!< NoOutputBeforeRecoveryFlag
  bool                        m_handleCraAsCvsStartFlag;                                //!< HandleCraAsCvsStartFlag
  bool                        m_handleGdrAsCvsStartFlag;                                //!< HandleGdrAsCvsStartFlag
  int                         m_spsId;                                                  //!< sequence parameter set ID
  int                         m_ppsId;                                                  //!< picture parameter set ID
  bool                        m_pocMsbPresentFlag;                                      //!< ph_poc_msb_present_flag
  int                         m_pocMsbVal;                                              //!< poc_msb_val
  bool                        m_virtualBoundariesEnabledFlag;                           //!< loop filtering across virtual boundaries disabled
  bool                        m_virtualBoundariesPresentFlag;                           //!< loop filtering across virtual boundaries disabled
  unsigned                    m_numVerVirtualBoundaries;                                //!< number of vertical virtual boundaries
  unsigned                    m_numHorVirtualBoundaries;                                //!< number of horizontal virtual boundaries
  unsigned                    m_virtualBoundariesPosX[3];                               //!< horizontal virtual boundary positions
  unsigned                    m_virtualBoundariesPosY[3];                               //!< vertical virtual boundary positions
  bool                        m_picOutputFlag;                                          //!< picture output flag
  ReferencePictureList        m_rpl[NUM_REF_PIC_LIST_01];
  int m_rplIdx[NUM_REF_PIC_LIST_01];   // index of used RPL in the SPS or -1 for local RPL in the picture header
  bool                        m_picInterSliceAllowedFlag;                               //!< inter slice allowed flag in PH
  bool                        m_picIntraSliceAllowedFlag;                               //!< intra slice allowed flag in PH
  bool                        m_splitConsOverrideFlag;                                  //!< partitioning constraint override flag
  uint32_t                    m_cuQpDeltaSubdivIntra;                                   //!< CU QP delta maximum subdivision for intra slices
  uint32_t                    m_cuQpDeltaSubdivInter;                                   //!< CU QP delta maximum subdivision for inter slices
  uint32_t                    m_cuChromaQpOffsetSubdivIntra;                            //!< CU chroma QP offset maximum subdivision for intra slices
  uint32_t                    m_cuChromaQpOffsetSubdivInter;                            //!< CU chroma QP offset maximum subdivision for inter slices
  bool                        m_enableTMVPFlag;                                         //!< enable temporal motion vector prediction
  bool                        m_picColFromL0Flag;                                       //!< syntax element collocated_from_l0_flag
  uint32_t                    m_colRefIdx;
  bool                        m_mvdL1ZeroFlag;                                          //!< L1 MVD set to zero flag
  uint32_t                    m_maxNumAffineMergeCand;                                  //!< max number of sub-block merge candidates
  bool                        m_disFracMMVD;                                            //!< fractional MMVD offsets disabled flag
  bool                        m_bdofDisabledFlag;                                       //!< picture level BDOF disable flag
  bool                        m_dmvrDisabledFlag;                                       //!< picture level DMVR disable flag
  bool                        m_profDisabledFlag;                                       //!< picture level PROF disable flag
  bool                        m_jointCbCrSignFlag;                                      //!< joint Cb/Cr residual sign flag
  int                         m_qpDelta;                                                //!< value of Qp delta
  EnumArray<bool, ChannelType> m_saoEnabledFlag;   // sao enabled flags for each channel
  bool                        m_alfEnabledFlag[MAX_NUM_COMPONENT];                      //!< alf enabled flags for each component
  int                         m_numAlfApsIdsLuma;                                       //!< number of alf aps active for the picture

  AlfApsList m_alfApsIdsLuma;   // list of ALF APSs for the picture

  int                         m_alfApsIdChroma;                                         //!< chroma alf aps ID
  bool m_ccalfEnabledFlag[MAX_NUM_COMPONENT];
  int  m_ccalfCbApsId;
  int  m_ccalfCrApsId;
  bool                        m_deblockingFilterOverrideFlag;                           //!< deblocking filter override controls enabled
  bool                        m_deblockingFilterDisable;                                //!< deblocking filter disabled flag
  int                         m_deblockingFilterBetaOffsetDiv2;                         //!< beta offset for deblocking filter
  int                         m_deblockingFilterTcOffsetDiv2;                           //!< tc offset for deblocking filter
  int                         m_deblockingFilterCbBetaOffsetDiv2;                       //!< beta offset for deblocking filter
  int                         m_deblockingFilterCbTcOffsetDiv2;                         //!< tc offset for deblocking filter
  int                         m_deblockingFilterCrBetaOffsetDiv2;                       //!< beta offset for deblocking filter
  int                         m_deblockingFilterCrTcOffsetDiv2;                         //!< tc offset for deblocking filter
  bool                        m_lmcsEnabledFlag;                                        //!< lmcs enabled flag
  int                         m_lmcsApsId;                                              //!< lmcs APS ID
  APS*                        m_lmcsAps;                                                //!< lmcs APS
  bool                        m_lmcsChromaResidualScaleFlag;                            //!< lmcs chroma residual scale flag
  bool                        m_explicitScalingListEnabledFlag;                         //!< explicit quantization scaling list enabled
  int                         m_scalingListApsId;                                       //!< quantization scaling list APS ID
  APS*                        m_scalingListAps;                                         //!< quantization scaling list APS
  unsigned                    m_minQT[3];                                               //!< minimum quad-tree size  0: I slice luma; 1: P/B slice; 2: I slice chroma
  unsigned                    m_maxMTTHierarchyDepth[3];                                //!< maximum MTT depth
  unsigned                    m_maxBTSize[3];                                           //!< maximum BT size
  unsigned                    m_maxTTSize[3];                                           //!< maximum TT size

  RefSetArray<WPScalingParam[MAX_NUM_COMPONENT]> m_weightPredTable;

  int m_numWeights[NUM_REF_PIC_LIST_01];   // number of weights for each list

public:
                              PicHeader();
  virtual                     ~PicHeader();
  void                        initPicHeader();
  bool                        isValid()                                                 { return m_valid;                                                                              }
  void                        setValid()                                                { m_valid = true;                                                                              }
  void                        setPic( Picture* p )                                      { m_pcPic = p;                                                                                 }
  Picture*                    getPic()                                                  { return m_pcPic;                                                                              }
  const Picture*              getPic() const                                            { return m_pcPic;                                                                              }
  void                        setPocLsb(int i)                                          { m_pocLsb = i;                                                                                }
  int                         getPocLsb()                                               { return m_pocLsb;                                                                             }
  void                        setNonReferencePictureFlag( bool b )                      { m_nonReferencePictureFlag = b;                                                               }
  bool                        getNonReferencePictureFlag() const                        { return m_nonReferencePictureFlag;                                                            }
  void                        setGdrOrIrapPicFlag( bool b )                             { m_gdrOrIrapPicFlag = b;                                                                      }
  bool                        getGdrOrIrapPicFlag() const                               { return m_gdrOrIrapPicFlag;                                                                   }
  void                        setGdrPicFlag( bool b )                                   { m_gdrPicFlag = b;                                                                            }
  bool                        getGdrPicFlag() const                                     { return m_gdrPicFlag;                                                                         }
  void                        setRecoveryPocCnt( uint32_t u )                           { m_recoveryPocCnt = u;                                                                        }
  uint32_t                    getRecoveryPocCnt() const                                 { return m_recoveryPocCnt;                                                                     }
  void                        setSPSId( uint32_t u )                                    { m_spsId = u;                                                                                 }
  uint32_t                    getSPSId() const                                          { return m_spsId;                                                                              }
  void                        setPPSId( uint32_t u )                                    { m_ppsId = u;                                                                                 }
  uint32_t                    getPPSId() const                                          { return m_ppsId;                                                                              }
  void                        setPocMsbPresentFlag(bool b)                              { m_pocMsbPresentFlag = b;                                                                     }
  bool                        getPocMsbPresentFlag() const                              { return m_pocMsbPresentFlag;                                                                  }
  void                        setPocMsbVal(int i)                                       { m_pocMsbVal = i;                                                                             }
  int                         getPocMsbVal()                                            { return m_pocMsbVal;                                                                          }
  void                        setVirtualBoundariesPresentFlag( bool b )                 { m_virtualBoundariesPresentFlag = b;                                                          }
  bool                        getVirtualBoundariesPresentFlag() const                   { return m_virtualBoundariesPresentFlag;                                                       }
  void                        setNumVerVirtualBoundaries(unsigned u)                    { m_numVerVirtualBoundaries = u;                                                               }
  unsigned                    getNumVerVirtualBoundaries() const                        { return m_numVerVirtualBoundaries;                                                            }
  void                        setNumHorVirtualBoundaries(unsigned u)                    { m_numHorVirtualBoundaries = u;                                                               }
  unsigned                    getNumHorVirtualBoundaries() const                        { return m_numHorVirtualBoundaries;                                                            }
  void                        setVirtualBoundariesPosX(unsigned u, unsigned idx)        { CHECK( idx >= 3, "boundary index exceeds valid range" ); m_virtualBoundariesPosX[idx] = u;   }
  unsigned                    getVirtualBoundariesPosX(unsigned idx) const              { CHECK( idx >= 3, "boundary index exceeds valid range" ); return m_virtualBoundariesPosX[idx];}
  void                        setVirtualBoundariesPosY(unsigned u, unsigned idx)        { CHECK( idx >= 3, "boundary index exceeds valid range" ); m_virtualBoundariesPosY[idx] = u;   }
  unsigned                    getVirtualBoundariesPosY(unsigned idx) const              { CHECK( idx >= 3, "boundary index exceeds valid range" ); return m_virtualBoundariesPosY[idx];}
  void                        setPicOutputFlag( bool b )                                { m_picOutputFlag = b;                                                                         }
  bool                        getPicOutputFlag() const                                  { return m_picOutputFlag;                                                                      }
  int                         getRplIdx(RefPicList l) const { return m_rplIdx[l]; }
  ReferencePictureList       *getRpl(RefPicList l) { return &m_rpl[l]; }
  void                        setRplIdx(RefPicList l, int rplIdx) { m_rplIdx[l] = rplIdx; }
  void                        setPicInterSliceAllowedFlag(bool b)                       { m_picInterSliceAllowedFlag = b; }
  bool                        getPicInterSliceAllowedFlag() const                       { return m_picInterSliceAllowedFlag; }
  void                        setPicIntraSliceAllowedFlag(bool b)                       { m_picIntraSliceAllowedFlag = b; }
  bool                        getPicIntraSliceAllowedFlag() const                       { return m_picIntraSliceAllowedFlag; }
  void                        setSplitConsOverrideFlag( bool b )                        { m_splitConsOverrideFlag = b;                                                                 }
  bool                        getSplitConsOverrideFlag() const                          { return m_splitConsOverrideFlag;                                                              }
  void                        setCuQpDeltaSubdivIntra( uint32_t u )                     { m_cuQpDeltaSubdivIntra = u;                                                                  }
  uint32_t                    getCuQpDeltaSubdivIntra() const                           { return m_cuQpDeltaSubdivIntra;                                                               }
  void                        setCuQpDeltaSubdivInter( uint32_t u )                     { m_cuQpDeltaSubdivInter = u;                                                                  }
  uint32_t                    getCuQpDeltaSubdivInter() const                           { return m_cuQpDeltaSubdivInter;                                                               }
  void                        setCuChromaQpOffsetSubdivIntra( uint32_t u )              { m_cuChromaQpOffsetSubdivIntra = u;                                                           }
  uint32_t                    getCuChromaQpOffsetSubdivIntra() const                    { return m_cuChromaQpOffsetSubdivIntra;                                                        }
  void                        setCuChromaQpOffsetSubdivInter( uint32_t u )              { m_cuChromaQpOffsetSubdivInter = u;                                                           }
  uint32_t                    getCuChromaQpOffsetSubdivInter() const                    { return m_cuChromaQpOffsetSubdivInter;                                                        }
  void                        setEnableTMVPFlag( bool b )                               { m_enableTMVPFlag = b;                                                                        }
  bool                        getEnableTMVPFlag() const                                 { return m_enableTMVPFlag;                                                                     }
  void                        setPicColFromL0Flag(bool val)                             { m_picColFromL0Flag = val;                                                                     }
  bool                        getPicColFromL0Flag() const                               { return m_picColFromL0Flag;                                                                    }
  void                        setColRefIdx( uint32_t refIdx)                             { m_colRefIdx = refIdx;                                                                       }
  uint32_t                    getColRefIdx()                                             { return m_colRefIdx;                                                                         }
  void                        setMvdL1ZeroFlag( bool b )                                { m_mvdL1ZeroFlag = b;                                                                         }
  bool                        getMvdL1ZeroFlag() const                                  { return m_mvdL1ZeroFlag;                                                                      }
  void                        setMaxNumAffineMergeCand( uint32_t val )                  { m_maxNumAffineMergeCand = val;                                                               }
  uint32_t                    getMaxNumAffineMergeCand() const                          { return m_maxNumAffineMergeCand;                                                              }
  void                        setDisFracMMVD( bool val )                                { m_disFracMMVD = val;                                                                         }
  bool                        getDisFracMMVD() const                                    { return m_disFracMMVD;                                                                        }
  void                        setBdofDisabledFlag( bool val )                           { m_bdofDisabledFlag = val;                                                                    }
  bool                        getBdofDisabledFlag() const                               { return m_bdofDisabledFlag;                                                                   }
  void                        setDmvrDisabledFlag( bool val )                           { m_dmvrDisabledFlag = val;                                                                    }
  bool                        getDmvrDisabledFlag() const                               { return m_dmvrDisabledFlag;                                                                   }
  void                        setProfDisabledFlag( bool val )                           { m_profDisabledFlag = val;                                                                    }
  bool                        getProfDisabledFlag() const                               { return m_profDisabledFlag;                                                                   }
  void                        setJointCbCrSignFlag( bool b )                            { m_jointCbCrSignFlag = b;                                                                     }
  bool                        getJointCbCrSignFlag() const                              { return m_jointCbCrSignFlag;                                                                  }
  void                        setQpDelta(int b)                                         { m_qpDelta = b;                                                                               }
  int                         getQpDelta() const                                        { return m_qpDelta;                                                                            }
  void                        setSaoEnabledFlag(const ChannelType chType, bool b) { m_saoEnabledFlag[chType] = b; }
  bool                        getSaoEnabledFlag(const ChannelType chType) const { return m_saoEnabledFlag[chType]; }
  void                        setAlfEnabledFlag(ComponentID compId, bool b)             { m_alfEnabledFlag[compId] = b;                                                                }
  bool                        getAlfEnabledFlag(ComponentID compId) const               { return m_alfEnabledFlag[compId];                                                             }
  void                        setNumAlfApsIdsLuma(int i)                                { m_numAlfApsIdsLuma = i;                                                                      }
  int                         getNumAlfApsIdsLuma() const                               { return m_numAlfApsIdsLuma;                                                                   }
  void                        setAlfApsIdChroma(int i)                                  { m_alfApsIdChroma = i;                                                                        }
  int                         getAlfApsIdChroma() const                                 { return m_alfApsIdChroma;                                                                     }
  void                        setCcAlfEnabledFlag(ComponentID compId, bool b)           { m_ccalfEnabledFlag[compId] = b; }
  bool                        getCcAlfEnabledFlag(ComponentID compId) const             { return m_ccalfEnabledFlag[compId]; }
  void                        setCcAlfCbApsId(int i)                                    { m_ccalfCbApsId = i; }
  int                         getCcAlfCbApsId() const                                   { return m_ccalfCbApsId; }
  void                        setCcAlfCrApsId(int i)                                    { m_ccalfCrApsId = i; }
  int                         getCcAlfCrApsId() const                                   { return m_ccalfCrApsId; }
  void                        setDeblockingFilterOverrideFlag( bool b )                 { m_deblockingFilterOverrideFlag = b;                                                          }
  bool                        getDeblockingFilterOverrideFlag() const                   { return m_deblockingFilterOverrideFlag;                                                       }
  void                        setDeblockingFilterDisable( bool b )                      { m_deblockingFilterDisable= b;                                                                }
  bool                        getDeblockingFilterDisable() const                        { return m_deblockingFilterDisable;                                                            }
  void                        setDeblockingFilterBetaOffsetDiv2( int i )                { m_deblockingFilterBetaOffsetDiv2 = i;                                                        }
  int                         getDeblockingFilterBetaOffsetDiv2()const                  { return m_deblockingFilterBetaOffsetDiv2;                                                     }
  void                        setDeblockingFilterTcOffsetDiv2( int i )                  { m_deblockingFilterTcOffsetDiv2 = i;                                                          }
  int                         getDeblockingFilterTcOffsetDiv2() const                   { return m_deblockingFilterTcOffsetDiv2;                                                       }
  void                        setDeblockingFilterCbBetaOffsetDiv2( int i )              { m_deblockingFilterCbBetaOffsetDiv2 = i;                                                      }
  int                         getDeblockingFilterCbBetaOffsetDiv2()const                { return m_deblockingFilterCbBetaOffsetDiv2;                                                   }
  void                        setDeblockingFilterCbTcOffsetDiv2( int i )                { m_deblockingFilterCbTcOffsetDiv2 = i;                                                        }
  int                         getDeblockingFilterCbTcOffsetDiv2() const                 { return m_deblockingFilterCbTcOffsetDiv2;                                                     }
  void                        setDeblockingFilterCrBetaOffsetDiv2( int i )              { m_deblockingFilterCrBetaOffsetDiv2 = i;                                                      }
  int                         getDeblockingFilterCrBetaOffsetDiv2()const                { return m_deblockingFilterCrBetaOffsetDiv2;                                                   }
  void                        setDeblockingFilterCrTcOffsetDiv2( int i )                { m_deblockingFilterCrTcOffsetDiv2 = i;                                                        }
  int                         getDeblockingFilterCrTcOffsetDiv2() const                 { return m_deblockingFilterCrTcOffsetDiv2;                                                     }
  void                        setLmcsEnabledFlag(bool b)                                { m_lmcsEnabledFlag = b;                                                                       }
  bool                        getLmcsEnabledFlag()                                      { return m_lmcsEnabledFlag;                                                                    }
  const bool                  getLmcsEnabledFlag() const                                { return m_lmcsEnabledFlag;                                                                    }
  void                        setLmcsAPS(APS* aps)                                      { m_lmcsAps = aps; m_lmcsApsId = (aps) ? aps->getAPSId() : -1;                                 }
  APS*                        getLmcsAPS() const                                        { return m_lmcsAps;                                                                            }
  void                        setLmcsAPSId(int id)                                      { m_lmcsApsId = id;                                                                            }
  int                         getLmcsAPSId() const                                      { return m_lmcsApsId;                                                                          }
  void                        setLmcsChromaResidualScaleFlag(bool b)                    { m_lmcsChromaResidualScaleFlag = b;                                                           }
  bool                        getLmcsChromaResidualScaleFlag()                          { return m_lmcsChromaResidualScaleFlag;                                                        }
  const bool                  getLmcsChromaResidualScaleFlag() const                    { return m_lmcsChromaResidualScaleFlag;                                                        }
  void                        setScalingListAPS( APS* aps )                             { m_scalingListAps = aps; m_scalingListApsId = ( aps ) ? aps->getAPSId() : -1;                 }
  APS*                        getScalingListAPS() const                                 { return m_scalingListAps;                                                                     }
  void                        setScalingListAPSId( int id )                             { m_scalingListApsId = id;                                                                     }
  int                         getScalingListAPSId() const                               { return m_scalingListApsId;                                                                   }
  void                        setExplicitScalingListEnabledFlag( bool b )               { m_explicitScalingListEnabledFlag = b;                                                        }
  bool                        getExplicitScalingListEnabledFlag()                       { return m_explicitScalingListEnabledFlag;                                                     }
  const bool                  getExplicitScalingListEnabledFlag() const                 { return m_explicitScalingListEnabledFlag;                                                     }

  unsigned*                   getMinQTSizes() const                                     { return (unsigned *)m_minQT;                                                                  }
  unsigned*                   getMaxMTTHierarchyDepths() const                          { return (unsigned *)m_maxMTTHierarchyDepth;                                                   }
  unsigned*                   getMaxBTSizes() const                                     { return (unsigned *)m_maxBTSize;                                                              }
  unsigned*                   getMaxTTSizes() const                                     { return (unsigned *)m_maxTTSize;                                                              }

  void                        setMinQTSize(unsigned idx, unsigned minQT)                { m_minQT[idx] = minQT;                                                                        }
  void                        setMaxMTTHierarchyDepth(unsigned idx, unsigned maxMTT)    { m_maxMTTHierarchyDepth[idx] = maxMTT;                                                        }
  void                        setMaxBTSize(unsigned idx, unsigned maxBT)                { m_maxBTSize[idx] = maxBT;                                                                    }
  void                        setMaxTTSize(unsigned idx, unsigned maxTT)                { m_maxTTSize[idx] = maxTT;                                                                    }

  void                        setMinQTSizes(unsigned*   minQT)                          { m_minQT[0] = minQT[0]; m_minQT[1] = minQT[1]; m_minQT[2] = minQT[2];                                                 }
  void                        setMaxMTTHierarchyDepths(unsigned*   maxMTT)              { m_maxMTTHierarchyDepth[0] = maxMTT[0]; m_maxMTTHierarchyDepth[1] = maxMTT[1]; m_maxMTTHierarchyDepth[2] = maxMTT[2]; }
  void                        setMaxBTSizes(unsigned*   maxBT)                          { m_maxBTSize[0] = maxBT[0]; m_maxBTSize[1] = maxBT[1]; m_maxBTSize[2] = maxBT[2];                                     }
  void                        setMaxTTSizes(unsigned*   maxTT)                          { m_maxTTSize[0] = maxTT[0]; m_maxTTSize[1] = maxTT[1]; m_maxTTSize[2] = maxTT[2];                                     }

  unsigned getMinQTSize(SliceType slicetype, ChannelType chType = ChannelType::LUMA) const
  {
    return slicetype == I_SLICE ? (isLuma(chType) ? m_minQT[0] : m_minQT[2]) : m_minQT[1];
  }
  unsigned getMaxMTTHierarchyDepth(SliceType slicetype, ChannelType chType = ChannelType::LUMA) const
  {
    return slicetype == I_SLICE ? (isLuma(chType) ? m_maxMTTHierarchyDepth[0] : m_maxMTTHierarchyDepth[2])
                                : m_maxMTTHierarchyDepth[1];
  }
  unsigned getMaxBTSize(SliceType slicetype, ChannelType chType = ChannelType::LUMA) const
  {
    return slicetype == I_SLICE ? (isLuma(chType) ? m_maxBTSize[0] : m_maxBTSize[2]) : m_maxBTSize[1];
  }
  unsigned getMaxTTSize(SliceType slicetype, ChannelType chType = ChannelType::LUMA) const
  {
    return slicetype == I_SLICE ? (isLuma(chType) ? m_maxTTSize[0] : m_maxTTSize[2]) : m_maxTTSize[1];
  }

  void              setAlfApsIdsLuma(const AlfApsList &apsIDs) { m_alfApsIdsLuma = apsIDs; }
  const AlfApsList &getAlfApsIdsLuma() const { return m_alfApsIdsLuma; }

  void                        setWpScaling(WPScalingParam *wp)
  {
    memcpy(m_weightPredTable, wp, sizeof(WPScalingParam) * NUM_REF_PIC_LIST_01 * MAX_NUM_REF * MAX_NUM_COMPONENT);
  }
  const WPScalingParam *      getWpScaling(const RefPicList refPicList, const int refIdx) const;
  WPScalingParam *            getWpScaling(const RefPicList refPicList, const int refIdx);
  WPScalingParam*             getWpScalingAll()                                        { return (WPScalingParam *) m_weightPredTable; }
  void                        resetWpScaling();
  void                        setNumWeights(RefPicList l, int n) { m_numWeights[l] = n; }
  int                         getNumWeights(RefPicList l) { return m_numWeights[l]; }

  void                        setNoOutputBeforeRecoveryFlag( bool val )                { m_noOutputBeforeRecoveryFlag = val;  }
  bool                        getNoOutputBeforeRecoveryFlag() const                    { return m_noOutputBeforeRecoveryFlag; }
  void                        setHandleCraAsCvsStartFlag( bool val )                   { m_handleCraAsCvsStartFlag = val;     }
  bool                        getHandleCraAsCvsStartFlag() const                       { return m_handleCraAsCvsStartFlag;    }
  void                        setHandleGdrAsCvsStartFlag( bool val )                   { m_handleGdrAsCvsStartFlag = val;     }
  bool                        getHandleGdrAsCvsStartFlag() const                       { return m_handleGdrAsCvsStartFlag;    }
};

/// slice header class
class Slice
{

private:
  //  Bitstream writing
  EnumArray<bool, ChannelType> m_saoEnabledFlag;
  int                        m_poc;
  int                        m_iLastIDR;
  int                        m_prevGDRInSameLayerPOC;  //< the previous GDR in the same layer
  int                        m_iAssociatedIRAP;
  NalUnitType                m_iAssociatedIRAPType;
  int                        m_prevGDRSubpicPOC;
  int                        m_prevIRAPSubpicPOC;
  NalUnitType                m_prevIRAPSubpicType;
  bool                       m_enableDRAPSEI;
  bool                       m_useLTforDRAP;
  bool                       m_isDRAP;
  int                        m_latestDRAPPOC;
  bool                       m_enableEdrapSEI;
  int                        m_edrapRapId;
  bool                       m_useLTforEdrap;
  int                        m_edrapNumRefRapPics;
  std::vector<int>           m_edrapRefRapIds;
  int                        m_latestEDRAPPOC;
  bool                       m_latestEdrapLeadingPicDecodableFlag;
  ReferencePictureList         m_rpl[NUM_REF_PIC_LIST_01];
  int m_rplIdx[NUM_REF_PIC_LIST_01];   //< index of used RPL in the SPS or -1 for local RPL in the slice header
  NalUnitType                m_eNalUnitType;         ///< Nal unit type for the slice
  bool                       m_pictureHeaderInSliceHeader;
  uint32_t                   m_nuhLayerId;           ///< Nal unit layer id
  SliceType                  m_eSliceType;
  bool                       m_noOutputOfPriorPicsFlag;           //!< no output of prior pictures flag
  int                        m_iSliceQp;
  int                        m_iSliceQpBase;
  bool                       m_chromaQpAdjEnabled;
  bool                       m_lmcsEnabledFlag;
  bool                       m_explicitScalingListUsed;
  bool                       m_deblockingFilterDisable;
  bool                       m_deblockingFilterOverrideFlag;      //< offsets for deblocking filter inherit from PPS
  int                        m_deblockingFilterBetaOffsetDiv2;    //< beta offset for deblocking filter
  int                        m_deblockingFilterTcOffsetDiv2;      //< tc offset for deblocking filter
  int                        m_deblockingFilterCbBetaOffsetDiv2;  //< beta offset for deblocking filter
  int                        m_deblockingFilterCbTcOffsetDiv2;    //< tc offset for deblocking filter
  int                        m_deblockingFilterCrBetaOffsetDiv2;  //< beta offset for deblocking filter
  int                        m_deblockingFilterCrTcOffsetDiv2;    //< tc offset for deblocking filter
  bool                       m_depQuantEnabledFlag;               //!< dependent quantization enabled flag
  int                        m_riceBaseLevelValue;                //< baseLevel value for abs_remainder
  bool                       m_reverseLastSigCoeffFlag;
  bool                       m_signDataHidingEnabledFlag;         //!< sign data hiding enabled flag
  bool                       m_tsResidualCodingDisabledFlag;
  int                        m_list1IdxToList0Idx[MAX_NUM_REF];
  int                        m_aiNumRefIdx   [NUM_REF_PIC_LIST_01];    //  for multiple reference of current slice
  bool                       m_pendingRasInit;

  bool m_checkLdc;

  bool                       m_biDirPred;
  bool                       m_lmChromaCheckDisable;
  int                        m_symRefIdx[2];
  bool                       m_meetBiPredT;

  //  Data
  int                        m_iSliceQpDelta;
  int                        m_iSliceChromaQpDelta[MAX_NUM_COMPONENT+1];
  Picture*                   m_apcRefPicList [NUM_REF_PIC_LIST_01][MAX_NUM_REF+1];
  int                        m_aiRefPOCList  [NUM_REF_PIC_LIST_01][MAX_NUM_REF+1];
  bool                       m_bIsUsedAsLongTerm[NUM_REF_PIC_LIST_01][MAX_NUM_REF+1];
  int                        m_hierPredLayerIdx;   // hierarchical prediction layer index
  Picture*                   m_scaledRefPicList[NUM_REF_PIC_LIST_01][MAX_NUM_REF + 1];
  Picture*                   m_savedRefPicList[NUM_REF_PIC_LIST_01][MAX_NUM_REF + 1];
  ScalingRatio               m_scalingRatio[NUM_REF_PIC_LIST_01][MAX_NUM_REF_PICS];

  // access channel
  const VPS*                 m_pcVPS;
  const SPS*                 m_pcSPS;
  const PPS*                 m_pcPPS;
  Picture*                   m_pcPic;
  const PicHeader*           m_pcPicHeader;    //!< pointer to picture header structure
  bool                       m_colFromL0Flag;  // collocated picture from List0 flag


  uint32_t                   m_colRefIdx;
  double                     m_lambdas[MAX_NUM_COMPONENT];

  RefSetArray<bool[MAX_NUM_REF]> m_abEqualRef;
  uint32_t                   m_uiTLayer;
  bool                       m_bTLayerSwitchingFlag;

  SliceMap                   m_sliceMap;                     //!< list of CTUs in current slice - raster scan CTU addresses
  uint32_t                   m_independentSliceIdx;
  bool                       m_nextSlice;
  uint32_t                   m_sliceBits;
  bool                       m_finalized;

  bool                       m_bTestWeightPred;
  bool                       m_bTestWeightBiPred;
  RefSetArray<WPScalingParam[MAX_NUM_COMPONENT]> m_weightPredTable;
  WPACDCParam                m_weightACDCParam[MAX_NUM_COMPONENT];
  ClpRngs                    m_clpRngs;
  std::vector<uint32_t>      m_substreamSizes;
  uint32_t                   m_numEntryPoints;
  uint32_t                   m_numSubstream;

  bool                       m_cabacInitFlag;

  uint32_t                   m_sliceSubPicId;


  SliceType                  m_encCABACTableIdx;           // Used to transmit table selection across slices.

  clock_t                    m_iProcessingStartTime;
  double                     m_dProcessingTime;

  int                        m_rpPicOrderCntVal;
  APS*                       m_alfApss[ALF_CTB_MAX_NUM_APS];
  bool                       m_alfEnabledFlag[MAX_NUM_COMPONENT];
  int                        m_numAlfApsIdsLuma;

  AlfApsList m_alfApsIdsLuma;

  int                        m_alfApsIdChroma;
  bool                       m_ccAlfCbEnabledFlag;
  bool                       m_ccAlfCrEnabledFlag;
  int                        m_ccAlfCbApsId;
  int                        m_ccAlfCrApsId;
  bool                       m_disableSATDForRd{ false };
  bool                       m_isLossless{ false };
  int                        m_tsrcIndex{ 0 };
  unsigned                   m_riceBit[8];
  int                        m_cntRightBottom;
#if GREEN_METADATA_SEI_ENABLED
  FeatureCounterStruct m_featureCounter;
#endif

public:
                              Slice();
  virtual                     ~Slice();
  void                        initSlice();
  void                        inheritFromPicHeader( PicHeader *picHeader, const PPS *pps, const SPS *sps );
  void                        setPicHeader( const PicHeader* pcPicHeader )           { m_pcPicHeader = pcPicHeader;                                  }
  const PicHeader*            getPicHeader() const                                   { return m_pcPicHeader;                                         }
  int                         getRefIdx4MVPair( RefPicList eCurRefPicList, int nCurRefIdx );


  void                        setSPS( const SPS* pcSPS )                             { m_pcSPS = pcSPS;                                              }
  const SPS*                  getSPS() const                                         { return m_pcSPS;                                               }
  void                        setVPS( const VPS* pcVPS )                             { m_pcVPS = pcVPS;                                              }
  const VPS*                  getVPS() const                                         { return m_pcVPS;                                               }

  void                        setPPS( const PPS* pcPPS )                             { m_pcPPS = pcPPS;                                              }
  const PPS*                  getPPS() const                                         { return m_pcPPS;                                               }

  void                        setAlfAPSs(APS** apss)                                 { memcpy(m_alfApss, apss, sizeof(m_alfApss));                   }
  APS**                       getAlfAPSs()                                           { return m_alfApss;                                             }
  void                        setSaoEnabledFlag(ChannelType chType, bool s) { m_saoEnabledFlag[chType] = s; }
  bool                        getSaoEnabledFlag(ChannelType chType) const { return m_saoEnabledFlag[chType]; }
  ReferencePictureList       *getRpl(RefPicList l) { return &m_rpl[l]; }
  void                        setRplIdx(RefPicList l, int rplIdx) { m_rplIdx[l] = rplIdx; }
  int                         getRplIdx(RefPicList l) const { return m_rplIdx[l]; }
  void                        setLastIDR(int iIDRPOC)                                { m_iLastIDR = iIDRPOC;                                         }
  int                         getLastIDR() const                                     { return m_iLastIDR;                                            }
  void                        setPrevGDRInSameLayerPOC(int prevGDRInSameLayerPOC)    { m_prevGDRInSameLayerPOC = prevGDRInSameLayerPOC;              }
  int                         getPrevGDRInSameLayerPOC() const                       { return m_prevGDRInSameLayerPOC;                               }
  void                        setAssociatedIRAPPOC(int iAssociatedIRAPPOC)           { m_iAssociatedIRAP = iAssociatedIRAPPOC;                       }
  int                         getAssociatedIRAPPOC() const                           { return m_iAssociatedIRAP;                                     }
  void                        setAssociatedIRAPType(NalUnitType associatedIRAPType)  { m_iAssociatedIRAPType = associatedIRAPType;                   }
  NalUnitType                 getAssociatedIRAPType() const                          { return m_iAssociatedIRAPType;                                 }
  void                        setPrevGDRSubpicPOC(int poc)                           { m_prevGDRSubpicPOC = poc;                                     }
  int                         getPrevGDRSubpicPOC() const                            { return m_prevGDRSubpicPOC;                                    }
  void                        setPrevIRAPSubpicPOC(int poc)                          { m_prevIRAPSubpicPOC = poc;                                    }
  int                         getPrevIRAPSubpicPOC() const                           { return m_prevIRAPSubpicPOC;                                   }
  void                        setPrevIRAPSubpicType(NalUnitType type)                { m_prevIRAPSubpicType = type;                                  }
  NalUnitType                 getPrevIRAPSubpicType() const                          { return m_prevIRAPSubpicType;                                  }
  void                        checkSubpicTypeConstraints(PicList& rcListPic, const ReferencePictureList* pRPL0, const ReferencePictureList* pRPL1, const int prevIRAPSubpicDecOrderNo);
  SliceType                   getSliceType() const                                   { return m_eSliceType;                                          }
  void                        setNoOutputOfPriorPicsFlag(bool b)                     { m_noOutputOfPriorPicsFlag = b;                                }
  bool                        getNoOutputOfPriorPicsFlag() const                     { return m_noOutputOfPriorPicsFlag;                             }
  int                         getPOC() const { return m_poc; }
  int                         getSliceQp() const                                     { return m_iSliceQp;                                            }
  bool                        getUseWeightedPrediction() const                       { return( (m_eSliceType==P_SLICE && testWeightPred()) || (m_eSliceType==B_SLICE && testWeightBiPred()) ); }
  int                         getSliceQpDelta() const                                { return m_iSliceQpDelta;                                       }
  int                         getSliceChromaQpDelta(ComponentID compID) const        { return isLuma(compID) ? 0 : m_iSliceChromaQpDelta[compID];    }
  bool                        getUseChromaQpAdj() const { return m_chromaQpAdjEnabled; }
  bool                        getDeblockingFilterDisable() const                     { return m_deblockingFilterDisable;                             }
  bool                        getDeblockingFilterOverrideFlag() const                { return m_deblockingFilterOverrideFlag;                        }
  int                         getDeblockingFilterBetaOffsetDiv2()const               { return m_deblockingFilterBetaOffsetDiv2;                      }
  int                         getDeblockingFilterTcOffsetDiv2() const                { return m_deblockingFilterTcOffsetDiv2;                        }
  int                         getDeblockingFilterCbBetaOffsetDiv2()const             { return m_deblockingFilterCbBetaOffsetDiv2;                    }
  int                         getDeblockingFilterCbTcOffsetDiv2() const              { return m_deblockingFilterCbTcOffsetDiv2;                      }
  int                         getDeblockingFilterCrBetaOffsetDiv2()const             { return m_deblockingFilterCrBetaOffsetDiv2;                    }
  int                         getDeblockingFilterCrTcOffsetDiv2() const              { return m_deblockingFilterCrTcOffsetDiv2;                      }
  bool                        getPendingRasInit() const                              { return m_pendingRasInit;                                      }
  void                        setPendingRasInit( bool val )                          { m_pendingRasInit = val;                                       }

  void                        setLmcsEnabledFlag(bool b)                              { m_lmcsEnabledFlag = b;                                       }
  bool                        getLmcsEnabledFlag()                                    { return m_lmcsEnabledFlag;                                    }
  const bool                  getLmcsEnabledFlag() const                              { return m_lmcsEnabledFlag;                                    }

  void                        setExplicitScalingListUsed(bool b)                      { m_explicitScalingListUsed = b;                               }
  bool                        getExplicitScalingListUsed()                            { return m_explicitScalingListUsed;                            }

  int                         getNumRefIdx( RefPicList e ) const                     { return m_aiNumRefIdx[e];                                      }
  Picture*                    getPic()                                               { return m_pcPic;                                               }
  const Picture*              getPic() const                                         { return m_pcPic;                                               }
  Picture *                   getRefPic(RefPicList e, int refIdx) const { return m_apcRefPicList[e][refIdx]; }
  int                         getRefPOC(RefPicList e, int refIdx) const { return m_aiRefPOCList[e][refIdx]; }
  int                         getHierPredLayerIdx() const { return m_hierPredLayerIdx; }
  bool                        getColFromL0Flag() const                               { return m_colFromL0Flag;                                       }
  uint32_t                    getColRefIdx() const                                   { return m_colRefIdx;                                           }
  void                        checkColRefIdx(uint32_t curSliceSegmentIdx, const Picture* pic);
  bool                        getIsUsedAsLongTerm(int i, int j) const                { return m_bIsUsedAsLongTerm[i][j];                             }
  void                        setIsUsedAsLongTerm(int i, int j, bool value)          { m_bIsUsedAsLongTerm[i][j] = value;                            }
  bool                        getCheckLDC() const { return m_checkLdc; }
  int                         getList1IdxToList0Idx( int list1Idx ) const            { return m_list1IdxToList0Idx[list1Idx];                        }
  void                        setPOC(int i) { m_poc = i; }
  bool                        getPictureHeaderInSliceHeader() const                  { return m_pictureHeaderInSliceHeader;                         }
  void                        setPictureHeaderInSliceHeader( bool e )                { m_pictureHeaderInSliceHeader = e;                            }
  void                        setNalUnitType( NalUnitType e )                        { m_eNalUnitType      = e;                                      }
  NalUnitType                 getNalUnitType() const                                 { return m_eNalUnitType;                                        }
  void                        setNalUnitLayerId( uint32_t i )                        { m_nuhLayerId = i;                                             }
  uint32_t                    getNalUnitLayerId() const                              { return m_nuhLayerId;                                          }
  bool                        getRapPicFlag() const;
  bool                        getIdrPicFlag() const                                  { return getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL || getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP; }
  bool                        isIRAP() const { return (getNalUnitType() >= NAL_UNIT_CODED_SLICE_IDR_W_RADL) && (getNalUnitType() <= NAL_UNIT_CODED_SLICE_CRA); }
  // CLVSS PU is either an IRAP PU with NoOutputBeforeRecoveryFlag equal to 1 or a GDR PU with NoOutputBeforeRecoveryFlag equal to 1.
  bool                        isClvssPu() const                                      { return m_eNalUnitType >= NAL_UNIT_CODED_SLICE_IDR_W_RADL && m_eNalUnitType <= NAL_UNIT_CODED_SLICE_GDR && !m_pcPPS->getMixedNaluTypesInPicFlag() && m_pcPicHeader->getNoOutputBeforeRecoveryFlag(); }
  bool                        isIDRorBLA() const { return (getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL) || (getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP); }
  bool                        isLeadingPic() const
  {
    return getNalUnitType() == NAL_UNIT_CODED_SLICE_RADL || getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL;
  }
  void                        checkCRA(const ReferencePictureList* pRPL0, const ReferencePictureList* pRPL1, const int pocCRA, CheckCRAFlags &flags, PicList& rcListPic);
  void                        checkSTSA(PicList& rcListPic);
  void                        checkRPL(const ReferencePictureList* pRPL0, const ReferencePictureList* pRPL1, const int associatedIRAPDecodingOrderNumber, PicList& rcListPic);
  void                        decodingRefreshMarking(int& pocCRA, bool& bRefreshPending, PicList& rcListPic, const bool bEfficientFieldIRAPEnabled);
  void                        setSliceType( SliceType e )                            { m_eSliceType        = e;                                      }
  void                        setSliceQp( int i )                                    { m_iSliceQp          = i;                                      }
  void                        setSliceQpDelta( int i )                               { m_iSliceQpDelta     = i;                                      }
  void                        setSliceChromaQpDelta( ComponentID compID, int i )     { m_iSliceChromaQpDelta[compID] = isLuma(compID) ? 0 : i;       }
  void                        setUseChromaQpAdj(bool b) { m_chromaQpAdjEnabled = b; }
  void                        setDeblockingFilterDisable( bool b )                   { m_deblockingFilterDisable= b;                                 }
  void                        setDeblockingFilterOverrideFlag( bool b )              { m_deblockingFilterOverrideFlag = b;                           }
  void                        setDeblockingFilterBetaOffsetDiv2( int i )             { m_deblockingFilterBetaOffsetDiv2 = i;                         }
  void                        setDeblockingFilterTcOffsetDiv2( int i )               { m_deblockingFilterTcOffsetDiv2 = i;                           }
  void                        setDeblockingFilterCbBetaOffsetDiv2( int i )           { m_deblockingFilterCbBetaOffsetDiv2 = i;                         }
  void                        setDeblockingFilterCbTcOffsetDiv2( int i )             { m_deblockingFilterCbTcOffsetDiv2 = i;                           }
  void                        setDeblockingFilterCrBetaOffsetDiv2( int i )           { m_deblockingFilterCrBetaOffsetDiv2 = i;                         }
  void                        setDeblockingFilterCrTcOffsetDiv2( int i )             { m_deblockingFilterCrTcOffsetDiv2 = i;                           }
  void                        setDepQuantEnabledFlag( bool b )                       { m_depQuantEnabledFlag = b;                                                                   }
  bool                        getDepQuantEnabledFlag() const { return m_depQuantEnabledFlag; }
  void                        setRiceBaseLevel(int b) { m_riceBaseLevelValue = b; }
  int                         getRiceBaseLevel() const { return m_riceBaseLevelValue; }
  void                        setReverseLastSigCoeffFlag( bool b )                   { m_reverseLastSigCoeffFlag = b;                                }
  bool                        getReverseLastSigCoeffFlag() const                     { return m_reverseLastSigCoeffFlag;                             }
  void                        setSignDataHidingEnabledFlag( bool b )                 { m_signDataHidingEnabledFlag = b;                                                             }
  bool                        getSignDataHidingEnabledFlag() const { return m_signDataHidingEnabledFlag; }
  void                        setTSResidualCodingDisabledFlag(bool b) { m_tsResidualCodingDisabledFlag = b; }
  bool                        getTSResidualCodingDisabledFlag() const { return m_tsResidualCodingDisabledFlag; }

  void                        setNumRefIdx( RefPicList e, int i )                    { m_aiNumRefIdx[e]    = i;                                      }
  void                        setPic( Picture* p )                                   { m_pcPic             = p;                                      }
  void                        setHierPredLayerIdx(int idx) { m_hierPredLayerIdx = idx; }

  void                        constructRefPicList(PicList& rcListPic);
  void                        setRefPOCList();

  void                        setColFromL0Flag( bool colFromL0 )                     { m_colFromL0Flag = colFromL0;                                  }
  void                        setColRefIdx( uint32_t refIdx)                             { m_colRefIdx = refIdx;                                         }
  void                        setCheckLDC(bool b) { m_checkLdc = b; }

  void                        setBiDirPred( bool b, int refIdx0, int refIdx1 ) { m_biDirPred = b; m_symRefIdx[0] = refIdx0; m_symRefIdx[1] = refIdx1; }
  bool                        getBiDirPred() const { return m_biDirPred; }
  void                        setMeetBiPredT(bool b)                                 { m_meetBiPredT = b; }
  bool                        getMeetBiPredT() const                                 { return m_meetBiPredT;  }
  void                        setDisableLmChromaCheck( bool b )  { m_lmChromaCheckDisable = b; }
  bool                        getDisableLmChromaCheck() const { return m_lmChromaCheckDisable; }
  int                         getSymRefIdx( int refList ) const { return m_symRefIdx[refList]; }

  bool                        isIntra() const                                        { return m_eSliceType == I_SLICE;                               }
  bool                        isInterB() const                                       { return m_eSliceType == B_SLICE;                               }
  bool                        isInterP() const                                       { return m_eSliceType == P_SLICE;                               }
#if GDR_ENABLED
  bool isInterGDR() const
  {
    return (m_eSliceType == B_SLICE && m_eNalUnitType == NAL_UNIT_CODED_SLICE_GDR);
  }
#endif
#if GREEN_METADATA_SEI_ENABLED
  void setFeatureCounter (FeatureCounterStruct b ) {m_featureCounter = b;}
  FeatureCounterStruct getFeatureCounter (){return m_featureCounter;}
#endif
  bool                        getEnableDRAPSEI () const                              { return m_enableDRAPSEI;                                       }
  void                        setEnableDRAPSEI ( bool b )                            { m_enableDRAPSEI = b;                                          }
  bool                        getUseLTforDRAP () const                               { return m_useLTforDRAP;                                        }
  void                        setUseLTforDRAP ( bool b )                             { m_useLTforDRAP = b;                                           }
  bool                        isDRAP () const                                        { return m_isDRAP;                                              }
  void                        setDRAP ( bool b )                                     { m_isDRAP = b;                                                 }
  void                        setLatestDRAPPOC ( int i )                             { m_latestDRAPPOC = i;                                          }
  int                         getLatestDRAPPOC () const                              { return m_latestDRAPPOC;                                       }
  bool                        cvsHasPreviousDRAP() const                             { return m_latestDRAPPOC != MAX_INT;                            }
  bool                        isPocRestrictedByDRAP( int poc, bool precedingDRAPinDecodingOrder );
  bool                        isPOCInRefPicList( const ReferencePictureList *rpl, int poc );
  void                        checkConformanceForDRAP( uint32_t temporalId );
  bool                        getEnableEdrapSEI () const                             { return m_enableEdrapSEI; }
  void                        setEnableEdrapSEI ( bool b )                           { m_enableEdrapSEI = b; }
  int                         getEdrapRapId () const                                 { return m_edrapRapId; }
  void                        setEdrapRapId (int i)                                  { m_edrapRapId = i; }
  bool                        getUseLTforEdrap () const                              { return m_useLTforEdrap; }
  void                        setUseLTforEdrap ( bool b )                            { m_useLTforEdrap = b; }
  int                         getEdrapNumRefRapPics () const                         { return m_edrapNumRefRapPics; }
  void                        setEdrapNumRefRapPics (int i)                          { m_edrapNumRefRapPics = i; }
  int                         getEdrapRefRapId (int idx) const                       { return m_edrapRefRapIds[idx]; }
  void                        addEdrapRefRapIds (int i)                              { m_edrapRefRapIds.push_back(i); }
  void                        deleteEdrapRefRapIds (int i)                           { m_edrapRefRapIds.erase(m_edrapRefRapIds.begin() + i); m_edrapNumRefRapPics--; }
  bool                        isPocRestrictedByEdrap( int poc );
  void                        setLatestEDRAPPOC ( int i )                            { m_latestEDRAPPOC = i; }
  int                         getLatestEDRAPPOC () const                             { return m_latestEDRAPPOC; }
  bool                        cvsHasPreviousEDRAP() const                            { return m_latestEDRAPPOC != MAX_INT; }
  void                        setLatestEdrapLeadingPicDecodableFlag ( bool b )       { m_latestEdrapLeadingPicDecodableFlag = b; }
  bool                        getLatestEdrapLeadingPicDecodableFlag () const         { return m_latestEdrapLeadingPicDecodableFlag; }
  void                        checkConformanceForEDRAP( uint32_t temporalId );

  void                        setLambdas( const double lambdas[MAX_NUM_COMPONENT] )  { for (int component = 0; component < MAX_NUM_COMPONENT; component++) m_lambdas[component] = lambdas[component]; }
  const double*               getLambdas() const                                     { return m_lambdas;                                             }

  void                        setSliceSubPicId(int i)                               { m_sliceSubPicId = i;   }
  uint32_t                    getSliceSubPicId() const                              { return m_sliceSubPicId; }
  uint32_t                    getCuQpDeltaSubdiv() const                             { return this->isIntra() ? m_pcPicHeader->getCuQpDeltaSubdivIntra() : m_pcPicHeader->getCuQpDeltaSubdivInter(); }
  uint32_t                    getCuChromaQpOffsetSubdiv() const                      { return this->isIntra() ? m_pcPicHeader->getCuChromaQpOffsetSubdivIntra() : m_pcPicHeader->getCuChromaQpOffsetSubdivInter(); }
  void                        initEqualRef();
  bool                        isEqualRef(RefPicList e, int refIdx1, int refIdx2)
  {
    CHECK(e>=NUM_REF_PIC_LIST_01, "Invalid reference picture list");
    if (refIdx1 < 0 || refIdx2 < 0)
    {
      return false;
    }
    else
    {
      return m_abEqualRef[e][refIdx1][refIdx2];
    }
  }

  void setEqualRef(RefPicList e, int refIdx1, int refIdx2, bool b)
  {
    CHECK( e >= NUM_REF_PIC_LIST_01, "Invalid reference picture list" );
    m_abEqualRef[e][refIdx1][refIdx2] = m_abEqualRef[e][refIdx2][refIdx1] = b;
  }

  static void                 sortPicList( PicList& rcListPic );
  void                        setList1IdxToList0Idx();

  uint32_t                    getTLayer() const                                      { return m_uiTLayer;                                            }
  void                        setTLayer( uint32_t uiTLayer )                             { m_uiTLayer = uiTLayer;                                        }

  void                        checkLeadingPictureRestrictions( PicList& rcListPic, const PPS& pps)                                         const;
  int                         checkThatAllRefPicsAreAvailable(PicList& rcListPic, const ReferencePictureList* pRPL, int rplIdx, bool printErrors, int* refPicIndex, int numActiveRefPics) const;

  void                        applyReferencePictureListBasedMarking( PicList& rcListPic, const ReferencePictureList *pRPL0, const ReferencePictureList *pRPL1, const int layerId, const PPS& pps )  const;
  bool                        isTemporalLayerSwitchingPoint( PicList& rcListPic )                                           const;
  bool                        isStepwiseTemporalLayerSwitchingPointCandidate( PicList& rcListPic )                          const;
  int                         checkThatAllRefPicsAreAvailable(PicList& rcListPic, const ReferencePictureList *pRPL, int rplIdx, bool printErrors)                const;

  void                        setNumTilesInSlice( uint32_t u )                       { m_sliceMap.setNumTilesInSlice( u );                                       }
  uint32_t                    getNumTilesInSlice() const                             { return m_sliceMap.getNumTilesInSlice();                                   }
  void                        setSliceMap( SliceMap map )                            { m_sliceMap = map;                                                         }
  uint32_t                    getFirstCtuRsAddrInSlice() const                       { return m_sliceMap.getCtuAddrInSlice(0);                                   }
  void                        setSliceID( uint32_t u )                               { m_sliceMap.setSliceID( u );                                               }
  uint32_t                    getSliceID() const                                     { return m_sliceMap.getSliceID();                                           }
  uint32_t                    getNumCtuInSlice() const                               { return m_sliceMap.getNumCtuInSlice();                                     }
  uint32_t                    getCtuAddrInSlice( int idx ) const                     { return m_sliceMap.getCtuAddrInSlice( idx );                               }
  void                        initSliceMap()                                         { m_sliceMap.initSliceMap();                                                }
  void                        addCtusToSlice( uint32_t startX, uint32_t stopX,
                                              uint32_t startY, uint32_t stopY,
                                              uint32_t picWidthInCtbsY )             { m_sliceMap.addCtusToSlice(startX, stopX, startY, stopY, picWidthInCtbsY); }
  void                        setIndependentSliceIdx( uint32_t i)                        { m_independentSliceIdx = i;                                    }
  uint32_t                        getIndependentSliceIdx() const                         { return  m_independentSliceIdx;                                }
  void                        copySliceInfo(Slice *pcSliceSrc, bool cpyAlmostAll = true);
  void                        setSliceBits( uint32_t uiVal )                             { m_sliceBits = uiVal;                                          }
  uint32_t                        getSliceBits() const                                   { return m_sliceBits;                                           }

  // clang-format off
  void setFinalized(bool val) { m_finalized = val; }
  bool getFinalized() const { return m_finalized; }
  // clang-format on

  bool                        testWeightPred( ) const                                { return m_bTestWeightPred;                                     }
  void                            setTestWeightPred(bool value) { m_bTestWeightPred = value; }
  bool                        testWeightBiPred( ) const                              { return m_bTestWeightBiPred;                                   }
  void                            setTestWeightBiPred(bool value) { m_bTestWeightBiPred = value; }
  void                            setWpScaling(RefSetArray<WPScalingParam[MAX_NUM_COMPONENT]> &wp)
  {
    memcpy(m_weightPredTable, wp, sizeof(m_weightPredTable));
  }
  void setWpScaling(WPScalingParam *wp) { memcpy(m_weightPredTable, wp, sizeof(m_weightPredTable)); }
  WPScalingParam *            getWpScalingAll()                                      { return (WPScalingParam *) m_weightPredTable;                  }
  WPScalingParam *            getWpScaling(const RefPicList refPicList, const int refIdx);
  const WPScalingParam *      getWpScaling(const RefPicList refPicList, const int refIdx) const;

  void                        resetWpScaling();
  void                        initWpScaling(const SPS *sps);

  void                        setWpAcDcParam( WPACDCParam wp[MAX_NUM_COMPONENT] )    { memcpy(m_weightACDCParam, wp, sizeof(WPACDCParam)*MAX_NUM_COMPONENT); }

  void                        getWpAcDcParam( const WPACDCParam *&wp ) const;
  void                        initWpAcDcParam();

  void                        clearSubstreamSizes( )                                 { return m_substreamSizes.clear();                              }
  uint32_t                        getNumberOfSubstreamSizes( )                           { return (uint32_t) m_substreamSizes.size();                        }
  void                        addSubstreamSize( uint32_t size )                          { m_substreamSizes.push_back(size);                             }
  uint32_t                        getSubstreamSize( uint32_t idx )                           { CHECK(idx>=getNumberOfSubstreamSizes(),"Invalid index"); return m_substreamSizes[idx]; }
  void                        resetNumberOfSubstream()                               { m_numSubstream = 0;                                           }
  uint32_t                    getNumberOfSubstream()                                 { return (uint32_t) m_numSubstream;                             }
  void                        increaseNumberOfSubstream()                            { m_numSubstream++;                                             }

  void                        setCabacInitFlag( bool val )                           { m_cabacInitFlag = val;                                        } //!< set CABAC initial flag
  bool                        getCabacInitFlag()                               const { return m_cabacInitFlag;                                       } //!< get CABAC initial flag

  void                        setEncCABACTableIdx( SliceType idx )                   { m_encCABACTableIdx = idx;                                     }
  SliceType                   getEncCABACTableIdx() const                            { return m_encCABACTableIdx;                                    }


  void                        setSliceQpBase( int i )                                { m_iSliceQpBase = i;                                           }
  int                         getSliceQpBase()                                 const { return m_iSliceQpBase;                                        }

  void                        setDefaultClpRng( const SPS& sps );
  const ClpRngs&              clpRngs()                                         const { return m_clpRngs;}
  const ClpRng&               clpRng( ComponentID id)                           const { return m_clpRngs.comp[id];}
  ClpRngs&                    getClpRngs()                                            { return m_clpRngs;}
  unsigned                    getMinPictureDistance()                           const ;
  void startProcessingTimer();
  void stopProcessingTimer();
  void resetProcessingTime()       { m_dProcessingTime = m_iProcessingStartTime = 0; }
  double getProcessingTime() const { return m_dProcessingTime; }

  void                        resetAlfEnabledFlag() { memset(m_alfEnabledFlag, 0, sizeof(m_alfEnabledFlag)); }
  bool                        getAlfEnabledFlag(ComponentID compId) const { return m_alfEnabledFlag[compId]; }
  void                        setAlfEnabledFlag(ComponentID compId, bool b) { m_alfEnabledFlag[compId] = b; }
  int                         getNumAlfApsIdsLuma() const { return m_numAlfApsIdsLuma; }
  void                        setNumAlfApsIdsLuma(int i) { m_numAlfApsIdsLuma = i; }
  int                         getAlfApsIdChroma() const { return m_alfApsIdChroma; }
  void                        setAlfApsIdChroma(int i) { m_alfApsIdChroma = i; }

  const AlfApsList &getAlfApsIdsLuma() const { return m_alfApsIdsLuma; }
  void              setAlfApsIdsLuma(const AlfApsList &apsIDs) { m_alfApsIdsLuma = apsIDs; }

  void resetCcAlCbfEnabledFlag() { m_ccAlfCbEnabledFlag = 0; }
  void resetCcAlCrfEnabledFlag() { m_ccAlfCrEnabledFlag = 0; }

  void setCcAlfCbEnabledFlag(bool b) { m_ccAlfCbEnabledFlag = b; }
  void setCcAlfCrEnabledFlag(bool b) { m_ccAlfCrEnabledFlag = b; }
  void setCcAlfCbApsId(int i) { m_ccAlfCbApsId = i; }
  void setCcAlfCrApsId(int i) { m_ccAlfCrApsId = i; }

  bool getCcAlfCbEnabledFlag() { return m_ccAlfCbEnabledFlag; }
  bool getCcAlfCrEnabledFlag() { return m_ccAlfCrEnabledFlag; }
  int  getCcAlfCbApsId() { return m_ccAlfCbApsId; }
  int  getCcAlfCrApsId() { return m_ccAlfCrApsId; }
  void                        setDisableSATDForRD(bool b) { m_disableSATDForRd = b; }
  bool                        getDisableSATDForRD() { return m_disableSATDForRd; }
  void                        setLossless(bool b) { m_isLossless = b; }
  bool                        isLossless() const { return m_isLossless; }
  void                        scaleRefPicList( Picture *scaledRefPic[ ], PicHeader *picHeader, APS** apss, APS* lmcsAps, APS* scalingListAps, const bool isDecoder );
  void                        freeScaledRefPicList( Picture *scaledRefPic[] );
  bool                        checkRPR();
  const ScalingRatio         &getScalingRatio(const RefPicList refPicList, const int refIdx) const
  {
    CHECK(refIdx < 0, "Invalid reference index");
    return m_scalingRatio[refPicList][refIdx];
  }
  void                        setNumSubstream( const SPS *sps, const PPS *pps );
  void                        setNumEntryPoints( const SPS *sps, const PPS *pps );
  uint32_t                    getNumEntryPoints( ) const { return m_numEntryPoints;  }
  bool                        isLastSliceInSubpic();

  CcAlfFilterParam            m_ccAlfFilterParam;
  uint8_t*                    m_ccAlfFilterControl[2];

  void setTsrcIndex(int v)
  {
    m_tsrcIndex = v;
  }
  int getTsrcIndex() const
  {
    return m_tsrcIndex;
  }

  void                        setRiceBit(int idx, int i) { m_riceBit[idx] = i; }
  unsigned                    getRiceBit(int idx) const { return m_riceBit[idx]; }

  // clang-format off
  void updateCntRightBottom(int val) { m_cntRightBottom += val; }
  int  getCntRightBottom() const { return m_cntRightBottom; }
  // clang-format on

protected:
  Picture*              xGetRefPic( PicList& rcListPic, const int poc, const int layerId );
  Picture*              xGetLongTermRefPic( PicList& rcListPic, const int poc, const bool pocHasMsb, const int layerId );
  Picture*              xGetLongTermRefPicCandidate( PicList& rcListPic, const int poc, const bool pocHasMsb, const int layerId );
public:
  std::unordered_map< Position, std::unordered_map< Size, double> > m_mapPltCost[2];
private:
};// END CLASS DEFINITION Slice

class PreCalcValues
{
public:
  PreCalcValues(const SPS &sps, const PPS &pps, bool _isEncoder)
    : chrFormat(sps.getChromaFormatIdc())
    , multiBlock422(false)
    , maxCUWidth(sps.getMaxCUWidth())
    , maxCUHeight(sps.getMaxCUHeight())
    , maxCUWidthMask(maxCUWidth - 1)
    , maxCUHeightMask(maxCUHeight - 1)
    , maxCUWidthLog2(floorLog2(maxCUWidth))
    , maxCUHeightLog2(floorLog2(maxCUHeight))
    , minCUWidth(1 << MIN_CU_LOG2)
    , minCUHeight(1 << MIN_CU_LOG2)
    , minCUWidthLog2(floorLog2(minCUWidth))
    , minCUHeightLog2(floorLog2(minCUHeight))
    , partsInCtuWidth(maxCUWidth >> MIN_CU_LOG2)
    , partsInCtuHeight(maxCUHeight >> MIN_CU_LOG2)
    , partsInCtu(partsInCtuWidth * partsInCtuHeight)
    , widthInCtus((pps.getPicWidthInLumaSamples() + sps.getMaxCUWidth() - 1) / sps.getMaxCUWidth())
    , heightInCtus((pps.getPicHeightInLumaSamples() + sps.getMaxCUHeight() - 1) / sps.getMaxCUHeight())
    , sizeInCtus(widthInCtus * heightInCtus)
    , lumaWidth(pps.getPicWidthInLumaSamples())
    , lumaHeight(pps.getPicHeightInLumaSamples())
    , fastDeltaQPCuMaxSize(Clip3(1u << sps.getLog2MinCodingBlockSize(), sps.getMaxCUHeight(), 32u))
    , noChroma2x2(false)
    , isEncoder(_isEncoder)
    , ISingleTree(!sps.getUseDualITree())
    , maxBtDepth{ sps.getMaxMTTHierarchyDepthI(), sps.getMaxMTTHierarchyDepth(), sps.getMaxMTTHierarchyDepthIChroma() }
    , minBtSize{ 1u << sps.getLog2MinCodingBlockSize(), 1u << sps.getLog2MinCodingBlockSize(),
                 1u << sps.getLog2MinCodingBlockSize() }
    , maxBtSize{ sps.getMaxBTSizeI(), sps.getMaxBTSize(), sps.getMaxBTSizeIChroma() }
    , minTtSize{ 1u << sps.getLog2MinCodingBlockSize(), 1u << sps.getLog2MinCodingBlockSize(),
                 1u << sps.getLog2MinCodingBlockSize() }
    , maxTtSize{ sps.getMaxTTSizeI(), sps.getMaxTTSize(), sps.getMaxTTSizeIChroma() }
    , minQtSize{ sps.getMinQTSize(I_SLICE, ChannelType::LUMA), sps.getMinQTSize(B_SLICE, ChannelType::LUMA),
                 sps.getMinQTSize(I_SLICE, ChannelType::CHROMA) }
  {}

  const ChromaFormat chrFormat;
  const bool         multiBlock422;
  const unsigned     maxCUWidth;
  const unsigned     maxCUHeight;
  // to get CTU position, use (x & maxCUWidthMask) rather than (x % maxCUWidth)
  const unsigned     maxCUWidthMask;
  const unsigned     maxCUHeightMask;
  const unsigned     maxCUWidthLog2;
  const unsigned     maxCUHeightLog2;
  const unsigned     minCUWidth;
  const unsigned     minCUHeight;
  const unsigned     minCUWidthLog2;
  const unsigned     minCUHeightLog2;
  const unsigned     partsInCtuWidth;
  const unsigned     partsInCtuHeight;
  const unsigned     partsInCtu;
  const unsigned     widthInCtus;
  const unsigned     heightInCtus;
  const unsigned     sizeInCtus;
  const unsigned     lumaWidth;
  const unsigned     lumaHeight;
  const unsigned     fastDeltaQPCuMaxSize;
  const bool         noChroma2x2;
  const bool         isEncoder;
  const bool         ISingleTree;

private:
  const unsigned     maxBtDepth[3];
  const unsigned     minBtSize [3];
  const unsigned     maxBtSize [3];
  const unsigned     minTtSize [3];
  const unsigned     maxTtSize [3];
  const unsigned     minQtSize [3];

  unsigned getValIdx    ( const Slice &slice, const ChannelType chType ) const;

public:
  unsigned getMaxBtDepth( const Slice &slice, const ChannelType chType ) const;
  unsigned getMinBtSize ( const Slice &slice, const ChannelType chType ) const;
  unsigned getMaxBtSize ( const Slice &slice, const ChannelType chType ) const;
  unsigned getMinTtSize ( const Slice &slice, const ChannelType chType ) const;
  unsigned getMaxTtSize ( const Slice &slice, const ChannelType chType ) const;
  unsigned getMinQtSize ( const Slice &slice, const ChannelType chType ) const;
};

#if ENABLE_TRACING
void xTraceVPSHeader();
void xTraceOPIHeader();
void xTraceDCIHeader();
void xTraceSPSHeader();
void xTracePPSHeader();
void xTraceAPSHeader();
void xTracePictureHeader();
void xTraceSliceHeader();
void xTraceAccessUnitDelimiter();
void xTraceFillerData();
#endif
