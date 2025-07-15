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

/** \file     TComPicSym.h
    \brief    picture symbol class (header)
*/

#ifndef __TCOMPICSYM__
#define __TCOMPICSYM__


// Include files
#include <deque>
#include "CommonDef.h"
#include "TComSlice.h"
#include "TComDataCU.h"
class TComSampleAdaptiveOffset;
class TComPPS;

//! \ingroup TLibCommon
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

class TComTile
{
private:
  UInt      m_tileWidthInCtus;
  UInt      m_tileHeightInCtus;
  UInt      m_rightEdgePosInCtus;
  UInt      m_bottomEdgePosInCtus;
  UInt      m_firstCtuRsAddr;

public:
  TComTile();
  virtual ~TComTile();

  Void      setTileWidthInCtus     ( UInt i )            { m_tileWidthInCtus = i; }
  UInt      getTileWidthInCtus     () const              { return m_tileWidthInCtus; }
  Void      setTileHeightInCtus    ( UInt i )            { m_tileHeightInCtus = i; }
  UInt      getTileHeightInCtus    () const              { return m_tileHeightInCtus; }
  Void      setRightEdgePosInCtus  ( UInt i )            { m_rightEdgePosInCtus = i; }
  UInt      getRightEdgePosInCtus  () const              { return m_rightEdgePosInCtus; }
  Void      setBottomEdgePosInCtus ( UInt i )            { m_bottomEdgePosInCtus = i; }
  UInt      getBottomEdgePosInCtus () const              { return m_bottomEdgePosInCtus; }
  Void      setFirstCtuRsAddr      ( UInt i )            { m_firstCtuRsAddr = i; }
  UInt      getFirstCtuRsAddr      () const              { return m_firstCtuRsAddr; }
};

/// picture symbol class
class TComPicSym
{
private:
  UInt          m_frameWidthInCtus;
  UInt          m_frameHeightInCtus;

  UInt          m_uiMinCUWidth;
  UInt          m_uiMinCUHeight;

  UChar         m_uhTotalDepth;       ///< max. depth
  UInt          m_numPartitionsInCtu;
  UInt          m_numPartInCtuWidth;
  UInt          m_numPartInCtuHeight;
  UInt          m_numCtusInFrame;

  std::deque<TComSlice*> m_apSlices;
  TComDataCU**  m_pictureCtuArray;        ///< array of CU data.

  Int           m_numTileColumnsMinus1;
  Int           m_numTileRowsMinus1;
  std::vector<TComTile> m_tileParameters;
  UInt*         m_ctuTsToRsAddrMap;    ///< for a given TS (Tile-Scan; coding order) address, returns the RS (Raster-Scan) address. cf CtbAddrTsToRs in specification.
  UInt*         m_puiTileIdxMap;       ///< the map of the tile index relative to CTU raster scan address
  UInt*         m_ctuRsToTsAddrMap;    ///< for a given RS (Raster-Scan) address, returns the TS (Tile-Scan; coding order) address. cf CtbAddrRsToTs in specification.

#if REDUCED_ENCODER_MEMORY
public:
#if MCTS_EXTRACTION
  friend class MctsExtractorTComPicSym;
#endif
  struct DPBPerCtuData
  {
    Bool isInter(const UInt absPartAddr)                const { return m_pePredMode[absPartAddr] == MODE_INTER; }
    PartSize getPartitionSize( const UInt absPartAddr ) const { return static_cast<PartSize>( m_pePartSize[absPartAddr] ); }
    const TComCUMvField* getCUMvField ( RefPicList e )  const { return &m_CUMvField[e];                  }
    const TComSlice* getSlice()                         const { return m_pSlice; }

    SChar        * m_pePredMode;
    SChar        * m_pePartSize;
    TComCUMvField  m_CUMvField[NUM_REF_PIC_LIST_01];
    TComSlice    * m_pSlice;
  };

private:
  DPBPerCtuData *m_dpbPerCtuData;
#endif
  SAOBlkParam  *m_saoBlkParams;
#if ADAPTIVE_QP_SELECTION
  TCoeff*       m_pParentARLBuffer;
#endif
  TComSPS       m_sps;
  TComPPS       m_pps;

  Void               xInitTiles( );
  Void               xInitCtuTsRsAddrMaps();
  Void               setNumTileColumnsMinus1( Int i )                      { m_numTileColumnsMinus1 = i;    }
  Void               setNumTileRowsMinus1( Int i )                         { m_numTileRowsMinus1 = i;       }
  Void               setCtuTsToRsAddrMap( Int ctuTsAddr, Int ctuRsAddr )   { *(m_ctuTsToRsAddrMap + ctuTsAddr) = ctuRsAddr; }
  Void               setCtuRsToTsAddrMap( Int ctuRsAddr, Int ctuTsOrder )  { *(m_ctuRsToTsAddrMap + ctuRsAddr) = ctuTsOrder; }

public:
#if REDUCED_ENCODER_MEMORY
  Void               create  ( const TComSPS &sps, const TComPPS &pps, UInt uiMaxDepth, const Bool bAllocateCtuArray );
  Void               prepareForReconstruction();
  Void               releaseReconstructionIntermediateData();
  Void               releaseAllReconstructionData();
#else
  Void               create  ( const TComSPS &sps, const TComPPS &pps, UInt uiMaxDepth );
#endif
  Void               destroy ();

  TComPicSym  ();
  ~TComPicSym();

  TComSlice*         getSlice(UInt i)                                      { return m_apSlices[i];             }
  const TComSlice*   getSlice(UInt i) const                                { return m_apSlices[i];             }
  UInt               getFrameWidthInCtus() const                           { return m_frameWidthInCtus;            }
  UInt               getFrameHeightInCtus() const                          { return m_frameHeightInCtus;           }
  UInt               getMinCUWidth() const                                 { return m_uiMinCUWidth;                }
  UInt               getMinCUHeight() const                                { return m_uiMinCUHeight;               }
  UInt               getNumberOfCtusInFrame() const                        { return m_numCtusInFrame;              }
  TComDataCU*        getCtu( UInt ctuRsAddr )                              { return m_pictureCtuArray[ctuRsAddr];  }
  const TComDataCU*  getCtu( UInt ctuRsAddr ) const                        { return m_pictureCtuArray[ctuRsAddr];  }
  const TComSPS&     getSPS()                 const                        { return m_sps; }
  const TComPPS&     getPPS()                 const                        { return m_pps; }
#if REDUCED_ENCODER_MEMORY
  Bool                 hasDPBPerCtuData() const                            { return (m_dpbPerCtuData!=0); };
  DPBPerCtuData&       getDPBPerCtuData(UInt ctuRsAddr)                    { return m_dpbPerCtuData[ctuRsAddr]; }
  const DPBPerCtuData& getDPBPerCtuData(UInt ctuRsAddr) const              { return m_dpbPerCtuData[ctuRsAddr]; }
#endif

  TComSlice *        swapSliceObject(TComSlice* p, UInt i)                 { p->setSPS(&m_sps); p->setPPS(&m_pps); TComSlice *pTmp=m_apSlices[i];m_apSlices[i] = p; pTmp->setSPS(0); pTmp->setPPS(0); return pTmp; }
  UInt               getNumAllocatedSlice() const                          { return UInt(m_apSlices.size());       }
  Void               allocateNewSlice();
  Void               clearSliceBuffer();
  UInt               getNumPartitionsInCtu() const                         { return m_numPartitionsInCtu;   }
  UInt               getNumPartInCtuWidth() const                          { return m_numPartInCtuWidth;    }
  UInt               getNumPartInCtuHeight() const                         { return m_numPartInCtuHeight;   }
  Int                getNumTileColumnsMinus1() const                       { return m_numTileColumnsMinus1; }
  Int                getNumTileRowsMinus1() const                          { return m_numTileRowsMinus1;    }
  Int                getNumTiles() const                                   { return (m_numTileRowsMinus1+1)*(m_numTileColumnsMinus1+1); }
  TComTile*          getTComTile  ( UInt tileIdx )                         { return &(m_tileParameters[tileIdx]); }
  const TComTile*    getTComTile  ( UInt tileIdx ) const                   { return &(m_tileParameters[tileIdx]); }
  UInt               getCtuTsToRsAddrMap( Int ctuTsAddr ) const            { return *(m_ctuTsToRsAddrMap + (ctuTsAddr>=m_numCtusInFrame ? m_numCtusInFrame : ctuTsAddr)); }
  UInt               getTileIdxMap( Int ctuRsAddr ) const                  { return *(m_puiTileIdxMap + ctuRsAddr); }
  UInt               getCtuRsToTsAddrMap( Int ctuRsAddr ) const            { return *(m_ctuRsToTsAddrMap + (ctuRsAddr>=m_numCtusInFrame ? m_numCtusInFrame : ctuRsAddr)); }
  SAOBlkParam*       getSAOBlkParam()                                      { return m_saoBlkParams;}
  const SAOBlkParam* getSAOBlkParam() const                                { return m_saoBlkParams;}
  Void               deriveLoopFilterBoundaryAvailibility(Int ctuRsAddr,
                                                          Bool& isLeftAvail, Bool& isRightAvail, Bool& isAboveAvail, Bool& isBelowAvail,
                                                          Bool& isAboveLeftAvail, Bool& isAboveRightAvail, Bool& isBelowLeftAvail, Bool& isBelowRightAvail);
protected:
  UInt               xCalculateNextCtuRSAddr( UInt uiCurrCtuRSAddr );

};// END CLASS DEFINITION TComPicSym


#if MCTS_EXTRACTION
// Mcts Extractor helper class
class MctsExtractorTComPicSym {
public:
  Void               setNumberOfCtusInFrame(TComPicSym *picsym, UInt numOfCtusInFrame) { picsym->m_numCtusInFrame = numOfCtusInFrame; }  ///< allows setting numOfCtusInFrame which controls memory allocations of several variables in TComPic, TComPicSym, TComLoopFilter and others. Make sure to have equal values of numOfCtusInFrame at time of creation and destruction of an object lifetime to avoid memory leaks.
};
#endif


//! \}

#if MCTS_ENC_CHECK
Void getTilePosition(const TComDataCU* const pcCU, UInt &tileXPosInCtus, UInt &tileYPosInCtus, UInt &tileWidthtInCtus, UInt &tileHeightInCtus);
#endif

#endif // __TCOMPICSYM__

