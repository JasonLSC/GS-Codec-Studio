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

/** \file     TComDataCU.h
    \brief    CU data structure (header)
    \todo     not all entities are documented
*/

#ifndef __TCOMDATACU__
#define __TCOMDATACU__

#include <algorithm>
#include <vector>

// Include files
#include "CommonDef.h"
#include "TComMotionInfo.h"
#include "TComSlice.h"
#include "TComRdCost.h"
#include "TComPattern.h"

//! \ingroup TLibCommon
//! \{

class TComTU; // forward declaration

static const UInt NUM_MOST_PROBABLE_MODES=3;

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// CU data structure class
class TComDataCU
{
private:

  // -------------------------------------------------------------------------------------------------------------------
  // class pointers
  // -------------------------------------------------------------------------------------------------------------------

  TComPic*      m_pcPic;                                ///< picture class pointer
  TComSlice*    m_pcSlice;                              ///< slice header pointer

  // -------------------------------------------------------------------------------------------------------------------
  // CU description
  // -------------------------------------------------------------------------------------------------------------------

  UInt          m_ctuRsAddr;                            ///< CTU (also known as LCU) address in a slice (Raster-scan address, as opposed to tile-scan/encoding order).
  UInt          m_absZIdxInCtu;                         ///< absolute address in a CTU. It's Z scan order
  UInt          m_uiCUPelX;                             ///< CU position in a pixel (X)
  UInt          m_uiCUPelY;                             ///< CU position in a pixel (Y)
  UInt          m_uiNumPartition;                       ///< total number of minimum partitions in a CU
  UChar*        m_puhWidth;                             ///< array of widths
  UChar*        m_puhHeight;                            ///< array of heights
  UChar*        m_puhDepth;                             ///< array of depths
  Int           m_unitSize;                             ///< size of a "minimum partition"

  // -------------------------------------------------------------------------------------------------------------------
  // CU data
  // -------------------------------------------------------------------------------------------------------------------

  Bool*         m_skipFlag;                             ///< array of skip flags
  SChar*        m_pePartSize;                           ///< array of partition sizes
  SChar*        m_pePredMode;                           ///< array of prediction modes
  SChar*        m_crossComponentPredictionAlpha[MAX_NUM_COMPONENT]; ///< array of cross-component prediction alpha values
  Bool*         m_CUTransquantBypass;                   ///< array of cu_transquant_bypass flags
  SChar*        m_phQP;                                 ///< array of QP values
  UChar*        m_ChromaQpAdj;                          ///< array of chroma QP adjustments (indexed). when value = 0, cu_chroma_qp_offset_flag=0; when value>0, indicates cu_chroma_qp_offset_flag=1 and cu_chroma_qp_offset_idx=value-1
  UInt          m_codedChromaQpAdj;
  UChar*        m_puhTrIdx;                             ///< array of transform indices
  UChar*        m_puhTransformSkip[MAX_NUM_COMPONENT];  ///< array of transform skipping flags
  UChar*        m_puhCbf[MAX_NUM_COMPONENT];            ///< array of coded block flags (CBF)
  TComCUMvField m_acCUMvField[NUM_REF_PIC_LIST_01];     ///< array of motion vectors.
  TCoeff*       m_pcTrCoeff[MAX_NUM_COMPONENT];         ///< array of transform coefficient buffers (0->Y, 1->Cb, 2->Cr)
#if ADAPTIVE_QP_SELECTION
  TCoeff*       m_pcArlCoeff[MAX_NUM_COMPONENT];        ///< ARL coefficient buffer (0->Y, 1->Cb, 2->Cr)
  Bool          m_ArlCoeffIsAliasedAllocation;          ///< ARL coefficient buffer is an alias of the global buffer and must not be free()'d
#endif

  Pel*          m_pcIPCMSample[MAX_NUM_COMPONENT];      ///< PCM sample buffer (0->Y, 1->Cb, 2->Cr)

  // -------------------------------------------------------------------------------------------------------------------
  // neighbour access variables
  // -------------------------------------------------------------------------------------------------------------------

  TComDataCU*   m_pCtuAboveLeft;                        ///< pointer of above-left CTU.
  TComDataCU*   m_pCtuAboveRight;                       ///< pointer of above-right CTU.
  TComDataCU*   m_pCtuAbove;                            ///< pointer of above CTU.
  TComDataCU*   m_pCtuLeft;                             ///< pointer of left CTU
  TComMvField   m_cMvFieldA;                            ///< motion vector of position A
  TComMvField   m_cMvFieldB;                            ///< motion vector of position B
  TComMvField   m_cMvFieldC;                            ///< motion vector of position C
  TComMv        m_cMvPred;                              ///< motion vector predictor

  // -------------------------------------------------------------------------------------------------------------------
  // coding tool information
  // -------------------------------------------------------------------------------------------------------------------

  Bool*         m_pbMergeFlag;                          ///< array of merge flags
  UChar*        m_puhMergeIndex;                        ///< array of merge candidate indices
#if AMP_MRG
  Bool          m_bIsMergeAMP;
#endif
  UChar*        m_puhIntraDir[MAX_NUM_CHANNEL_TYPE];
  UChar*        m_puhInterDir;                          ///< array of inter directions
  SChar*        m_apiMVPIdx[NUM_REF_PIC_LIST_01];       ///< array of motion vector predictor candidates
  SChar*        m_apiMVPNum[NUM_REF_PIC_LIST_01];       ///< array of number of possible motion vectors predictors
  Bool*         m_pbIPCMFlag;                           ///< array of intra_pcm flags
#if MCTS_ENC_CHECK
  Bool          m_tMctsMvpIsValid;
#endif

  // -------------------------------------------------------------------------------------------------------------------
  // misc. variables
  // -------------------------------------------------------------------------------------------------------------------

  Bool          m_bDecSubCu;                            ///< indicates decoder-mode
  Double        m_dTotalCost;                           ///< sum of partition RD costs
  Distortion    m_uiTotalDistortion;                    ///< sum of partition distortion
  UInt          m_uiTotalBits;                          ///< sum of partition bits
  UInt          m_uiTotalBins;                          ///< sum of partition bins
  SChar         m_codedQP;
  UChar*        m_explicitRdpcmMode[MAX_NUM_COMPONENT]; ///< Stores the explicit RDPCM mode for all TUs belonging to this CU

protected:

  /// adds a single possible motion vector predictor candidate
  Bool          xAddMVPCandUnscaled           ( AMVPInfo &info, const RefPicList eRefPicList, const Int iRefIdx, const UInt uiPartUnitIdx, const MVP_DIR eDir ) const;
  Bool          xAddMVPCandWithScaling        ( AMVPInfo &info, const RefPicList eRefPicList, const Int iRefIdx, const UInt uiPartUnitIdx, const MVP_DIR eDir ) const;

  Void          deriveRightBottomIdx          ( UInt uiPartIdx, UInt& ruiPartIdxRB ) const;
  Bool          xGetColMVP                    ( const RefPicList eRefPicList, const Int ctuRsAddr, const Int partUnitIdx, TComMv& rcMv, const Int refIdx ) const;

  /// compute scaling factor from POC difference
  static Int    xGetDistScaleFactor           ( Int iCurrPOC, Int iCurrRefPOC, Int iColPOC, Int iColRefPOC );

  Void          xDeriveCenterIdx              ( UInt uiPartIdx, UInt& ruiPartIdxCenter ) const;

public:
                TComDataCU();
  virtual       ~TComDataCU();

  // -------------------------------------------------------------------------------------------------------------------
  // create / destroy / initialize / copy
  // -------------------------------------------------------------------------------------------------------------------

  Void          create                        ( ChromaFormat chromaFormatIDC, UInt uiNumPartition, UInt uiWidth, UInt uiHeight, Bool bDecSubCu, Int unitSize
#if ADAPTIVE_QP_SELECTION
                                                , TCoeff *pParentARLBuffer = 0
#endif
                                              );
  Void          destroy                       ( );

  Void          initCtu                       ( TComPic* pcPic, UInt ctuRsAddr );
  Void          initEstData                   ( const UInt uiDepth, const Int qp, const Bool bTransquantBypass );
  Void          initSubCU                     ( TComDataCU* pcCU, UInt uiPartUnitIdx, UInt uiDepth, Int qp );
  Void          setOutsideCUPart              ( UInt uiAbsPartIdx, UInt uiDepth );

  Void          copySubCU                     ( TComDataCU* pcCU, UInt uiPartUnitIdx );
  Void          copyInterPredInfoFrom         ( TComDataCU* pcCU, UInt uiAbsPartIdx, RefPicList eRefPicList );
  Void          copyPartFrom                  ( TComDataCU* pcCU, UInt uiPartUnitIdx, UInt uiDepth );

  Void          copyToPic                     ( UChar uiDepth );

  // -------------------------------------------------------------------------------------------------------------------
  // member functions for CU description
  // -------------------------------------------------------------------------------------------------------------------

  TComPic*         getPic                     ( )                                                          { return m_pcPic;                            }
  const TComPic*   getPic                     ( ) const                                                    { return m_pcPic;                            }
  TComSlice*       getSlice                   ( )                                                          { return m_pcSlice;                          }
  const TComSlice* getSlice                   ( ) const                                                    { return m_pcSlice;                          }
  UInt&         getCtuRsAddr                  ( )                                                          { return m_ctuRsAddr;                        }
  UInt          getCtuRsAddr                  ( ) const                                                    { return m_ctuRsAddr;                        }
  UInt          getZorderIdxInCtu             ( ) const                                                    { return m_absZIdxInCtu;                     }
  UInt          getCUPelX                     ( ) const                                                    { return m_uiCUPelX;                         }
  UInt          getCUPelY                     ( ) const                                                    { return m_uiCUPelY;                         }

  UChar*        getDepth                      ( )                                                          { return m_puhDepth;                         }
  UChar         getDepth                      ( UInt uiIdx ) const                                         { return m_puhDepth[uiIdx];                  }
  Void          setDepth                      ( UInt uiIdx, UChar uh )                                     { m_puhDepth[uiIdx] = uh;                    }

  Void          setDepthSubParts              ( UInt uiDepth, UInt uiAbsPartIdx );

#if MCTS_ENC_CHECK
  Void          setTMctsMvpIsValid(Bool b)    { m_tMctsMvpIsValid = b; }
  Bool          getTMctsMvpIsValid()          { return m_tMctsMvpIsValid; }
  Bool          isLastColumnCTUInTile() const;
#endif

  // -------------------------------------------------------------------------------------------------------------------
  // member functions for CU data
  // -------------------------------------------------------------------------------------------------------------------

  SChar*        getPartitionSize              ( )                                                          { return m_pePartSize;                       }
  PartSize      getPartitionSize              ( UInt uiIdx ) const                                         { return static_cast<PartSize>( m_pePartSize[uiIdx] ); }
  Void          setPartitionSize              ( UInt uiIdx, PartSize uh )                                  { m_pePartSize[uiIdx] = uh;                  }
  Void          setPartSizeSubParts           ( PartSize eMode, UInt uiAbsPartIdx, UInt uiDepth );
  Void          setCUTransquantBypassSubParts ( Bool flag, UInt uiAbsPartIdx, UInt uiDepth );

  Bool*         getSkipFlag                   ( )                                                          { return m_skipFlag;                         }
  Bool          getSkipFlag                   ( UInt idx ) const                                           { return m_skipFlag[idx];                    }
  Void          setSkipFlag                   ( UInt idx, Bool skip )                                      { m_skipFlag[idx] = skip;                    }
  Void          setSkipFlagSubParts           ( Bool skip, UInt absPartIdx, UInt depth );

  SChar*        getPredictionMode             ( )                                                          { return m_pePredMode;                       }
  PredMode      getPredictionMode             ( UInt uiIdx ) const                                         { return static_cast<PredMode>( m_pePredMode[uiIdx] ); }
  Void          setPredictionMode             ( UInt uiIdx, PredMode uh )                                  { m_pePredMode[uiIdx] = uh;                  }
  Void          setPredModeSubParts           ( PredMode eMode, UInt uiAbsPartIdx, UInt uiDepth );

  SChar*        getCrossComponentPredictionAlpha( ComponentID compID )                                     { return m_crossComponentPredictionAlpha[compID];        }
  SChar         getCrossComponentPredictionAlpha( UInt uiIdx, ComponentID compID )                         { return m_crossComponentPredictionAlpha[compID][uiIdx]; }

  Bool*         getCUTransquantBypass         ( )                                                          { return m_CUTransquantBypass;               }
  Bool          getCUTransquantBypass         ( UInt uiIdx ) const                                         { return m_CUTransquantBypass[uiIdx];        }

  UChar*        getWidth                      ( )                                                          { return m_puhWidth;                         }
  UChar         getWidth                      ( UInt uiIdx ) const                                         { return m_puhWidth[uiIdx];                  }
  Void          setWidth                      ( UInt uiIdx, UChar  uh )                                    { m_puhWidth[uiIdx] = uh;                    }

  UChar*        getHeight                     ( )                                                          { return m_puhHeight;                        }
  UChar         getHeight                     ( UInt uiIdx ) const                                         { return m_puhHeight[uiIdx];                 }
  Void          setHeight                     ( UInt uiIdx, UChar  uh )                                    { m_puhHeight[uiIdx] = uh;                   }

  Void          setSizeSubParts               ( UInt uiWidth, UInt uiHeight, UInt uiAbsPartIdx, UInt uiDepth );

  SChar*        getQP                         ( )                                                          { return m_phQP;                             }
  SChar         getQP                         ( UInt uiIdx ) const                                         { return m_phQP[uiIdx];                      }
  Void          setQP                         ( UInt uiIdx, SChar value )                                  { m_phQP[uiIdx] =  value;                    }
  Void          setQPSubParts                 ( Int qp,   UInt uiAbsPartIdx, UInt uiDepth );
  Int           getLastValidPartIdx           ( Int iAbsPartIdx ) const;
  SChar         getLastCodedQP                ( UInt uiAbsPartIdx ) const;
  Void          setQPSubCUs                   ( Int qp, UInt absPartIdx, UInt depth, Bool &foundNonZeroCbf );
  Void          setCodedQP                    ( SChar qp )                                                 { m_codedQP = qp;                            }
  SChar         getCodedQP                    ( ) const                                                    { return m_codedQP;                          }

  UChar*        getChromaQpAdj                ( )                                                          { return m_ChromaQpAdj;                      } ///< array of chroma QP adjustments (indexed). when value = 0, cu_chroma_qp_offset_flag=0; when value>0, indicates cu_chroma_qp_offset_flag=1 and cu_chroma_qp_offset_idx=value-1
  UChar         getChromaQpAdj                ( Int idx ) const                                            { return m_ChromaQpAdj[idx];                 } ///< When value = 0, cu_chroma_qp_offset_flag=0; when value>0, indicates cu_chroma_qp_offset_flag=1 and cu_chroma_qp_offset_idx=value-1
  Void          setChromaQpAdj                ( Int idx, UChar val )                                       { m_ChromaQpAdj[idx] = val;                  } ///< When val = 0,   cu_chroma_qp_offset_flag=0; when val>0,   indicates cu_chroma_qp_offset_flag=1 and cu_chroma_qp_offset_idx=val-1
  Void          setChromaQpAdjSubParts        ( UChar val, Int absPartIdx, Int depth );
  Void          setCodedChromaQpAdj           ( SChar qp )                                                 { m_codedChromaQpAdj = qp;                   }
  SChar         getCodedChromaQpAdj           ( ) const                                                    { return m_codedChromaQpAdj;                 }

  Bool          isLosslessCoded               ( UInt absPartIdx ) const;

  UChar*        getTransformIdx               ( )                                                          { return m_puhTrIdx;                         }
  UChar         getTransformIdx               ( UInt uiIdx ) const                                         { return m_puhTrIdx[uiIdx];                  }
  Void          setTrIdxSubParts              ( UInt uiTrIdx, UInt uiAbsPartIdx, UInt uiDepth );

  UChar*        getTransformSkip              ( ComponentID compID )                                       { return m_puhTransformSkip[compID];         }
  UChar         getTransformSkip              ( UInt uiIdx, ComponentID compID ) const                     { return m_puhTransformSkip[compID][uiIdx];  }
  Void          setTransformSkipSubParts      ( UInt useTransformSkip, ComponentID compID, UInt uiAbsPartIdx, UInt uiDepth );
  Void          setTransformSkipSubParts      ( const UInt useTransformSkip[MAX_NUM_COMPONENT], UInt uiAbsPartIdx, UInt uiDepth );

  UChar*        getExplicitRdpcmMode          ( ComponentID component )                                    { return m_explicitRdpcmMode[component];     }
  UChar         getExplicitRdpcmMode          ( ComponentID component, UInt partIdx ) const                { return m_explicitRdpcmMode[component][partIdx]; }
  Void          setExplicitRdpcmModePartRange ( UInt rdpcmMode, ComponentID compID, UInt uiAbsPartIdx, UInt uiCoveredPartIdxes );

  Bool          isRDPCMEnabled                ( UInt uiAbsPartIdx ) const                                  { return getSlice()->getSPS()->getSpsRangeExtension().getRdpcmEnabledFlag(isIntra(uiAbsPartIdx) ? RDPCM_SIGNAL_IMPLICIT : RDPCM_SIGNAL_EXPLICIT); }

  Void          setCrossComponentPredictionAlphaPartRange ( SChar alphaValue, ComponentID compID, UInt uiAbsPartIdx, UInt uiCoveredPartIdxes );
  Void          setTransformSkipPartRange     ( UInt useTransformSkip, ComponentID compID, UInt uiAbsPartIdx, UInt uiCoveredPartIdxes );

  UInt          getQuadtreeTULog2MinSizeInCU  ( UInt uiIdx ) const;

        TComCUMvField* getCUMvField           ( RefPicList e )                                             { return &m_acCUMvField[e];                  }
  const TComCUMvField* getCUMvField           ( RefPicList e ) const                                       { return &m_acCUMvField[e];                  }

  TCoeff*       getCoeff                      ( ComponentID component )                                    { return m_pcTrCoeff[component];             }

#if ADAPTIVE_QP_SELECTION
  TCoeff*       getArlCoeff                   ( ComponentID component )                                    { return m_pcArlCoeff[component];            }
#endif
  Pel*          getPCMSample                  ( ComponentID component )                                    { return m_pcIPCMSample[component];          }

  UChar         getCbf                        ( UInt uiIdx, ComponentID eType ) const                      { return m_puhCbf[eType][uiIdx];             }
  UChar*        getCbf                        ( ComponentID eType )                                        { return m_puhCbf[eType];                    }
  UChar         getCbf                        ( UInt uiIdx, ComponentID eType, UInt uiTrDepth ) const      { return ( ( getCbf( uiIdx, eType ) >> uiTrDepth ) & 0x1 ); }
  Void          setCbf                        ( UInt uiIdx, ComponentID eType, UChar uh )                  { m_puhCbf[eType][uiIdx] = uh;               }
  Void          clearCbf                      ( UInt uiIdx, ComponentID eType, UInt uiNumParts );
  UChar         getQtRootCbf                  ( UInt uiIdx ) const;

  Void          setCbfSubParts                ( const UInt uiCbf[MAX_NUM_COMPONENT],  UInt uiAbsPartIdx, UInt uiDepth );
  Void          setCbfSubParts                ( UInt uiCbf, ComponentID compID, UInt uiAbsPartIdx, UInt uiDepth );
  Void          setCbfSubParts                ( UInt uiCbf, ComponentID compID, UInt uiAbsPartIdx, UInt uiPartIdx, UInt uiDepth );

  Void          setCbfPartRange               ( UInt uiCbf, ComponentID compID, UInt uiAbsPartIdx, UInt uiCoveredPartIdxes );
  Void          bitwiseOrCbfPartRange         ( UInt uiCbf, ComponentID compID, UInt uiAbsPartIdx, UInt uiCoveredPartIdxes );

  // -------------------------------------------------------------------------------------------------------------------
  // member functions for coding tool information
  // -------------------------------------------------------------------------------------------------------------------

  Bool*         getMergeFlag                  ( )                                                          { return m_pbMergeFlag;                      }
  Bool          getMergeFlag                  ( UInt uiIdx ) const                                         { return m_pbMergeFlag[uiIdx];               }
  Void          setMergeFlag                  ( UInt uiIdx, Bool b )                                       { m_pbMergeFlag[uiIdx] = b;                  }
  Void          setMergeFlagSubParts          ( Bool bMergeFlag, UInt uiAbsPartIdx, UInt uiPartIdx, UInt uiDepth );

  UChar*        getMergeIndex                 ( )                                                          { return m_puhMergeIndex;                    }
  UChar         getMergeIndex                 ( UInt uiIdx ) const                                         { return m_puhMergeIndex[uiIdx];             }
  Void          setMergeIndex                 ( UInt uiIdx, UInt uiMergeIndex )                            { m_puhMergeIndex[uiIdx] = uiMergeIndex;     }
  Void          setMergeIndexSubParts         ( UInt uiMergeIndex, UInt uiAbsPartIdx, UInt uiPartIdx, UInt uiDepth );
  template <typename T>
  Void          setSubPart                    ( T bParameter, T* pbBaseCtu, UInt uiCUAddr, UInt uiCUDepth, UInt uiPUIdx );

#if AMP_MRG
  Void          setMergeAMP                   ( Bool b )                                                   { m_bIsMergeAMP = b;                         }
  Bool          getMergeAMP                   ( ) const                                                    { return m_bIsMergeAMP;                      }
#endif

  UChar*        getIntraDir                   ( const ChannelType channelType )                   const    { return m_puhIntraDir[channelType];         }
  UChar         getIntraDir                   ( const ChannelType channelType, const UInt uiIdx ) const    { return m_puhIntraDir[channelType][uiIdx];  }

  Void          setIntraDirSubParts           ( const ChannelType channelType,
                                                const UInt uiDir,
                                                const UInt uiAbsPartIdx,
                                                const UInt uiDepth );

  UChar*        getInterDir                   ( )                                                          { return m_puhInterDir;                      }
  UChar         getInterDir                   ( UInt uiIdx ) const                                         { return m_puhInterDir[uiIdx];               }
  Void          setInterDir                   ( UInt uiIdx, UChar  uh )                                    { m_puhInterDir[uiIdx] = uh;                 }
  Void          setInterDirSubParts           ( UInt uiDir,  UInt uiAbsPartIdx, UInt uiPartIdx, UInt uiDepth );
  Bool*         getIPCMFlag                   ( )                                                          { return m_pbIPCMFlag;                       }
  Bool          getIPCMFlag                   ( UInt uiIdx ) const                                         { return m_pbIPCMFlag[uiIdx];                }
  Void          setIPCMFlag                   ( UInt uiIdx, Bool b )                                       { m_pbIPCMFlag[uiIdx] = b;                   }
  Void          setIPCMFlagSubParts           ( Bool bIpcmFlag, UInt uiAbsPartIdx, UInt uiDepth );

  // -------------------------------------------------------------------------------------------------------------------
  // member functions for accessing partition information
  // -------------------------------------------------------------------------------------------------------------------

  Void          getPartIndexAndSize           ( UInt uiPartIdx, UInt& ruiPartAddr, Int& riWidth, Int& riHeight ) const; // This is for use by a leaf/sub CU object only, with no additional AbsPartIdx
  UChar         getNumPartitions              ( const UInt uiAbsPartIdx = 0 ) const;
  Bool          isFirstAbsZorderIdxInDepth    ( UInt uiAbsPartIdx, UInt uiDepth ) const;

  // -------------------------------------------------------------------------------------------------------------------
  // member functions for motion vector
  // -------------------------------------------------------------------------------------------------------------------

  static Void   getMvField                    ( const TComDataCU* pcCU, UInt uiAbsPartIdx, RefPicList eRefPicList, TComMvField& rcMvField );

  Void          fillMvpCand                   ( const UInt uiPartIdx, const UInt uiPartAddr, const RefPicList eRefPicList, const Int iRefIdx, AMVPInfo* pInfo ) const;
  Bool          isDiffMER                     ( Int xN, Int yN, Int xP, Int yP ) const;
  Void          getPartPosition               ( UInt partIdx, Int& xP, Int& yP, Int& nPSW, Int& nPSH ) const;

  Void          setMVPIdx                     ( RefPicList eRefPicList, UInt uiIdx, Int iMVPIdx)           { m_apiMVPIdx[eRefPicList][uiIdx] = iMVPIdx; }
  Int           getMVPIdx                     ( RefPicList eRefPicList, UInt uiIdx) const                  { return m_apiMVPIdx[eRefPicList][uiIdx];    }
  SChar*        getMVPIdx                     ( RefPicList eRefPicList )                                   { return m_apiMVPIdx[eRefPicList];           }

  Void          setMVPNum                     ( RefPicList eRefPicList, UInt uiIdx, Int iMVPNum )          { m_apiMVPNum[eRefPicList][uiIdx] = iMVPNum; }
  Int           getMVPNum                     ( RefPicList eRefPicList, UInt uiIdx ) const                 { return m_apiMVPNum[eRefPicList][uiIdx];    }
  SChar*        getMVPNum                     ( RefPicList eRefPicList )                                   { return m_apiMVPNum[eRefPicList];           }

  Void          setMVPIdxSubParts             ( Int iMVPIdx, RefPicList eRefPicList, UInt uiAbsPartIdx, UInt uiPartIdx, UInt uiDepth );
  Void          setMVPNumSubParts             ( Int iMVPNum, RefPicList eRefPicList, UInt uiAbsPartIdx, UInt uiPartIdx, UInt uiDepth );

  Void          clipMv                        ( TComMv&     rcMv     ) const;
  Void          getMvPredLeft                 ( TComMv&     rcMvPred ) const                               { rcMvPred = m_cMvFieldA.getMv();            }
  Void          getMvPredAbove                ( TComMv&     rcMvPred ) const                               { rcMvPred = m_cMvFieldB.getMv();            }
  Void          getMvPredAboveRight           ( TComMv&     rcMvPred ) const                               { rcMvPred = m_cMvFieldC.getMv();            }

  Void          compressMV                    ();

  // -------------------------------------------------------------------------------------------------------------------
  // utility functions for neighbouring information
  // -------------------------------------------------------------------------------------------------------------------

  TComDataCU*   getCtuLeft                    ( )                                                          { return m_pCtuLeft;                         }
  TComDataCU*   getCtuAbove                   ( )                                                          { return m_pCtuAbove;                        }
  TComDataCU*   getCtuAboveLeft               ( )                                                          { return m_pCtuAboveLeft;                    }
  TComDataCU*   getCtuAboveRight              ( )                                                          { return m_pCtuAboveRight;                   }
  Bool          CUIsFromSameSlice             ( const TComDataCU *pCU /* Can be NULL */ ) const            { return ( pCU!=NULL && pCU->getSlice()->getSliceCurStartCtuTsAddr() == getSlice()->getSliceCurStartCtuTsAddr() ); }
  Bool          CUIsFromSameTile              ( const TComDataCU *pCU /* Can be NULL */ ) const;
  Bool          CUIsFromSameSliceAndTile      ( const TComDataCU *pCU /* Can be NULL */ ) const;
  Bool          CUIsFromSameSliceTileAndWavefrontRow( const TComDataCU *pCU /* Can be NULL */ ) const;
  Bool          isLastSubCUOfCtu              ( const UInt absPartIdx ) const;


  const TComDataCU*   getPULeft               ( UInt& uiLPartUnitIdx,
                                                UInt  uiCurrPartUnitIdx,
                                                Bool  bEnforceSliceRestriction=true,
                                                Bool  bEnforceTileRestriction=true ) const;

  const TComDataCU*   getPUAbove              ( UInt& uiAPartUnitIdx,
                                                UInt  uiCurrPartUnitIdx,
                                                Bool  bEnforceSliceRestriction=true,
                                                Bool  planarAtCTUBoundary = false,
                                                Bool  bEnforceTileRestriction=true ) const;

  const TComDataCU*   getPUAboveLeft          ( UInt&  uiALPartUnitIdx, UInt uiCurrPartUnitIdx, Bool bEnforceSliceRestriction=true ) const;

  const TComDataCU*   getQpMinCuLeft          ( UInt&  uiLPartUnitIdx,  UInt uiCurrAbsIdxInCtu ) const;
  const TComDataCU*   getQpMinCuAbove         ( UInt&  uiAPartUnitIdx,  UInt uiCurrAbsIdxInCtu ) const;

  /// returns CU and part index of the PU above the top row of the current uiCurrPartUnitIdx of the CU, at a horizontal offset (to the right) of uiPartUnitOffset (in parts)
  const TComDataCU*   getPUAboveRight         ( UInt&  uiARPartUnitIdx, UInt uiCurrPartUnitIdx, UInt uiPartUnitOffset = 1, Bool bEnforceSliceRestriction=true ) const;
  /// returns CU and part index of the PU left of the lefthand column of the current uiCurrPartUnitIdx of the CU, at a vertical offset (below) of uiPartUnitOffset (in parts)
  const TComDataCU*   getPUBelowLeft          ( UInt&  uiBLPartUnitIdx, UInt uiCurrPartUnitIdx, UInt uiPartUnitOffset = 1, Bool bEnforceSliceRestriction=true ) const;

  SChar         getRefQP                      ( UInt uiCurrAbsIdxInCtu ) const;

  Void          deriveLeftRightTopIdx         ( UInt uiPartIdx, UInt& ruiPartIdxLT, UInt& ruiPartIdxRT ) const;
  Void          deriveLeftBottomIdx           ( UInt uiPartIdx, UInt& ruiPartIdxLB ) const;

  Bool          hasEqualMotion                ( UInt uiAbsPartIdx, const TComDataCU* pcCandCU, UInt uiCandAbsPartIdx ) const;
#if MCTS_ENC_CHECK
  Void          getInterMergeCandidates       ( UInt uiAbsPartIdx, UInt uiPUIdx, TComMvField* pcMFieldNeighbours, UChar* puhInterDirNeighbours, Int& numValidMergeCand, UInt& numSpatialMergeCandidates , Int mrgCandIdx = -1) const;
#else
  Void          getInterMergeCandidates       ( UInt uiAbsPartIdx, UInt uiPUIdx, TComMvField* pcMFieldNeighbours, UChar* puhInterDirNeighbours, Int& numValidMergeCand, Int mrgCandIdx = -1 ) const;
#endif

  Void          deriveLeftRightTopIdxGeneral  ( UInt uiAbsPartIdx, UInt uiPartIdx, UInt& ruiPartIdxLT, UInt& ruiPartIdxRT ) const;
  Void          deriveLeftBottomIdxGeneral    ( UInt uiAbsPartIdx, UInt uiPartIdx, UInt& ruiPartIdxLB ) const;

  // -------------------------------------------------------------------------------------------------------------------
  // member functions for modes
  // -------------------------------------------------------------------------------------------------------------------

  Bool          isIntra                       ( UInt uiPartIdx ) const                                     { return m_pePredMode[ uiPartIdx ] == MODE_INTRA; }
  Bool          isInter                       ( UInt uiPartIdx ) const                                     { return m_pePredMode[ uiPartIdx ] == MODE_INTER; }
  Bool          isSkipped                     ( UInt uiPartIdx ) const; ///< returns true, if the partiton is skipped
  Bool          isBipredRestriction           ( UInt puIdx     ) const;

  // -------------------------------------------------------------------------------------------------------------------
  // member functions for symbol prediction (most probable / mode conversion)
  // -------------------------------------------------------------------------------------------------------------------

  UInt          getIntraSizeIdx               ( UInt uiAbsPartIdx ) const;

  Void          getAllowedChromaDir           ( UInt uiAbsPartIdx, UInt* uiModeList ) const;
  Void          getIntraDirPredictor          ( UInt uiAbsPartIdx, Int uiIntraDirPred[NUM_MOST_PROBABLE_MODES], const ComponentID compID, Int* piMode = NULL ) const;

  // -------------------------------------------------------------------------------------------------------------------
  // member functions for SBAC context
  // -------------------------------------------------------------------------------------------------------------------

  UInt          getCtxSplitFlag               ( UInt   uiAbsPartIdx, UInt uiDepth     ) const;
  UInt          getCtxQtCbf                   ( TComTU &rTu, const ChannelType chType ) const;

  UInt          getCtxSkipFlag                ( UInt   uiAbsPartIdx ) const;
  UInt          getCtxInterDir                ( UInt   uiAbsPartIdx ) const;

  UInt&         getTotalBins                  ( )                                                          { return m_uiTotalBins;       }
  // -------------------------------------------------------------------------------------------------------------------
  // member functions for RD cost storage
  // -------------------------------------------------------------------------------------------------------------------

  Double&       getTotalCost                  ( )                                                          { return m_dTotalCost;        }
  Distortion&   getTotalDistortion            ( )                                                          { return m_uiTotalDistortion; }
  UInt&         getTotalBits                  ( )                                                          { return m_uiTotalBits;       }
  UInt&         getTotalNumPart               ( )                                                          { return m_uiNumPartition;    }

  UInt          getCoefScanIdx                ( const UInt uiAbsPartIdx, const UInt uiWidth, const UInt uiHeight, const ComponentID compID ) const ;

};

namespace RasterAddress
{
  /** Check whether 2 addresses point to the same column
   * \param addrA          First address in raster scan order
   * \param addrB          Second address in raters scan order
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline Bool isEqualCol( Int addrA, Int addrB, Int numUnitsPerRow )
  {
    // addrA % numUnitsPerRow == addrB % numUnitsPerRow
    return (( addrA ^ addrB ) &  ( numUnitsPerRow - 1 ) ) == 0;
  }

  /** Check whether 2 addresses point to the same row
   * \param addrA          First address in raster scan order
   * \param addrB          Second address in raters scan order
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline Bool isEqualRow( Int addrA, Int addrB, Int numUnitsPerRow )
  {
    // addrA / numUnitsPerRow == addrB / numUnitsPerRow
    return (( addrA ^ addrB ) &~ ( numUnitsPerRow - 1 ) ) == 0;
  }

  /** Check whether 2 addresses point to the same row or column
   * \param addrA          First address in raster scan order
   * \param addrB          Second address in raters scan order
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline Bool isEqualRowOrCol( Int addrA, Int addrB, Int numUnitsPerRow )
  {
    return isEqualCol( addrA, addrB, numUnitsPerRow ) | isEqualRow( addrA, addrB, numUnitsPerRow );
  }

  /** Check whether one address points to the first column
   * \param addr           Address in raster scan order
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline Bool isZeroCol( Int addr, Int numUnitsPerRow )
  {
    // addr % numUnitsPerRow == 0
    return ( addr & ( numUnitsPerRow - 1 ) ) == 0;
  }

  /** Check whether one address points to the first row
   * \param addr           Address in raster scan order
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline Bool isZeroRow( Int addr, Int numUnitsPerRow )
  {
    // addr / numUnitsPerRow == 0
    return ( addr &~ ( numUnitsPerRow - 1 ) ) == 0;
  }

  /** Check whether one address points to a column whose index is smaller than a given value
   * \param addr           Address in raster scan order
   * \param val            Given column index value
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline Bool lessThanCol( Int addr, Int val, Int numUnitsPerRow )
  {
    // addr % numUnitsPerRow < val
    return ( addr & ( numUnitsPerRow - 1 ) ) < val;
  }

  /** Check whether one address points to a row whose index is smaller than a given value
   * \param addr           Address in raster scan order
   * \param val            Given row index value
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline Bool lessThanRow( Int addr, Int val, Int numUnitsPerRow )
  {
    // addr / numUnitsPerRow < val
    return addr < val * numUnitsPerRow;
  }
}

//! \}

#endif
