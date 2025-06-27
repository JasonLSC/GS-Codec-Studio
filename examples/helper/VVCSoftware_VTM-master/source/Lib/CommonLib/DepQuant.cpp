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

#include "DepQuant.h"
#include "TrQuant.h"
#include "CodingStructure.h"
#include "UnitTools.h"

#include <bitset>

#include "ContextModelling.h"




namespace DQIntern
{
  /*================================================================================*/
  /*=====                                                                      =====*/
  /*=====   R A T E   E S T I M A T O R                                        =====*/
  /*=====                                                                      =====*/
  /*================================================================================*/

  struct NbInfoSbb
  {
    uint8_t   num;
    uint8_t   inPos[5];
  };
  struct NbInfoOut
  {
    uint16_t  maxDist;
    uint16_t  num;
    uint16_t  outPos[5];
  };
  struct CoeffFracBits
  {
    int32_t   bits[6];
  };


  enum ScanPosType { SCAN_ISCSBB = 0, SCAN_SOCSBB = 1, SCAN_EOCSBB = 2 };

  struct ScanInfo
  {
    ScanInfo() {}
    int           sbbSize;
    int           numSbb;
    int           scanIdx;
    int           rasterPos;
    int           sbbPos;
    int           insidePos;
    bool          eosbb;
    ScanPosType   spt;
    unsigned      sigCtxOffsetNext;
    unsigned      gtxCtxOffsetNext;
    int           nextInsidePos;
    NbInfoSbb     nextNbInfoSbb;
    int           nextSbbRight;
    int           nextSbbBelow;
    int           posX;
    int           posY;
    ChannelType   chType;
    int           sbtInfo;
    int           tuWidth;
    int           tuHeight;
  };

  class Rom;
  struct TUParameters
  {
    TUParameters ( const Rom& rom, const unsigned width, const unsigned height, const ChannelType chType );
    ~TUParameters()
    {
      delete [] m_scanInfo;
    }

    ChannelType       m_chType;
    unsigned          m_width;
    unsigned          m_height;
    unsigned          m_numCoeff;
    unsigned          m_numSbb;
    unsigned          m_log2SbbWidth;
    unsigned          m_log2SbbHeight;
    unsigned          m_log2SbbSize;
    unsigned          m_sbbSize;
    unsigned          m_sbbMask;
    unsigned          m_widthInSbb;
    unsigned           m_heightInSbb;
    const ScanElement *m_scanSbbId2SbbPos;
    const ScanElement *m_scanId2BlkPos;
    const NbInfoSbb*  m_scanId2NbInfoSbb;
    const NbInfoOut*  m_scanId2NbInfoOut;
    ScanInfo*         m_scanInfo;
  private:
    void xSetScanInfo( ScanInfo& scanInfo, int scanIdx );
  };

  class Rom
  {
  public:
    Rom() : m_scansInitialized(false) {}
    ~Rom() { xUninitScanArrays(); }
    void                init        ()                       { xInitScanArrays(); }
    const NbInfoSbb*    getNbInfoSbb( int hd, int vd ) const { return m_scanId2NbInfoSbbArray[hd][vd]; }
    const NbInfoOut*    getNbInfoOut( int hd, int vd ) const { return m_scanId2NbInfoOutArray[hd][vd]; }
    const TUParameters* getTUPars   ( const CompArea& area, const ComponentID compID ) const
    {
      return m_tuParameters[floorLog2(area.width)][floorLog2(area.height)][to_underlying(toChannelType(compID))];
    }
  private:
    void  xInitScanArrays   ();
    void  xUninitScanArrays ();
  private:
    bool          m_scansInitialized;
    NbInfoSbb*    m_scanId2NbInfoSbbArray[ MAX_CU_DEPTH+1 ][ MAX_CU_DEPTH+1 ];
    NbInfoOut*    m_scanId2NbInfoOutArray[ MAX_CU_DEPTH+1 ][ MAX_CU_DEPTH+1 ];
    TUParameters* m_tuParameters         [ MAX_CU_DEPTH+1 ][ MAX_CU_DEPTH+1 ][ MAX_NUM_CHANNEL_TYPE ];
  };

  void Rom::xInitScanArrays()
  {
    if( m_scansInitialized )
    {
      return;
    }
    ::memset( m_scanId2NbInfoSbbArray, 0, sizeof(m_scanId2NbInfoSbbArray) );
    ::memset( m_scanId2NbInfoOutArray, 0, sizeof(m_scanId2NbInfoOutArray) );
    ::memset( m_tuParameters,          0, sizeof(m_tuParameters) );

    uint32_t raster2id[ MAX_CU_SIZE * MAX_CU_SIZE ];
    ::memset(raster2id, 0, sizeof(raster2id));

    for( int hd = 0; hd <= MAX_CU_DEPTH; hd++ )
    {
      for( int vd = 0; vd <= MAX_CU_DEPTH; vd++ )
      {
        if( (hd == 0 && vd <= 1) || (hd <= 1 && vd == 0) )
        {
          continue;
        }
        const uint32_t      blockWidth    = (1 << hd);
        const uint32_t      blockHeight   = (1 << vd);
        const uint32_t      log2CGWidth   = g_log2TxSubblockSize[hd][vd].width;
        const uint32_t      log2CGHeight  = g_log2TxSubblockSize[hd][vd].height;
        const uint32_t      groupWidth    = 1 << log2CGWidth;
        const uint32_t      groupHeight   = 1 << log2CGHeight;
        const uint32_t      groupSize     = groupWidth * groupHeight;
        const SizeType      blkWidthIdx   = gp_sizeIdxInfo->idxFrom( blockWidth  );
        const SizeType      blkHeightIdx  = gp_sizeIdxInfo->idxFrom( blockHeight );
        const ScanElement  *scanId2RP = g_scanOrder[SCAN_GROUPED_4x4][CoeffScanType::DIAG][blkWidthIdx][blkHeightIdx];
        NbInfoSbb*&         sId2NbSbb     = m_scanId2NbInfoSbbArray[hd][vd];
        NbInfoOut*&         sId2NbOut     = m_scanId2NbInfoOutArray[hd][vd];
        // consider only non-zero-out region
        const uint32_t      blkWidthNZOut  = getNonzeroTuSize(blockWidth);
        const uint32_t      blkHeightNZOut = getNonzeroTuSize(blockHeight);
        const uint32_t      totalValues   = blkWidthNZOut * blkHeightNZOut;

        sId2NbSbb = new NbInfoSbb[ totalValues ];
        sId2NbOut = new NbInfoOut[ totalValues ];

        for( uint32_t scanId = 0; scanId < totalValues; scanId++ )
        {
          raster2id[scanId2RP[scanId].idx] = scanId;
        }

        for( unsigned scanId = 0; scanId < totalValues; scanId++ )
        {
          const int posX = scanId2RP[scanId].x;
          const int posY = scanId2RP[scanId].y;
          const int rpos = scanId2RP[scanId].idx;
          {
            //===== inside subband neighbours =====
            NbInfoSbb&     nbSbb  = sId2NbSbb[ scanId ];
            const int      begSbb = scanId - ( scanId & (groupSize-1) ); // first pos in current subblock
            int            cpos[5];

            cpos[0] = ( posX + 1 < blkWidthNZOut                              ? ( raster2id[rpos+1           ] < groupSize + begSbb ? raster2id[rpos+1           ] - begSbb : 0 ) : 0 );
            cpos[1] = ( posX + 2 < blkWidthNZOut                              ? ( raster2id[rpos+2           ] < groupSize + begSbb ? raster2id[rpos+2           ] - begSbb : 0 ) : 0 );
            cpos[2] = ( posX + 1 < blkWidthNZOut && posY + 1 < blkHeightNZOut ? ( raster2id[rpos+1+blockWidth] < groupSize + begSbb ? raster2id[rpos+1+blockWidth] - begSbb : 0 ) : 0 );
            cpos[3] = ( posY + 1 < blkHeightNZOut                             ? ( raster2id[rpos+  blockWidth] < groupSize + begSbb ? raster2id[rpos+  blockWidth] - begSbb : 0 ) : 0 );
            cpos[4] = ( posY + 2 < blkHeightNZOut                             ? ( raster2id[rpos+2*blockWidth] < groupSize + begSbb ? raster2id[rpos+2*blockWidth] - begSbb : 0 ) : 0 );

            for( nbSbb.num = 0; true; )
            {
              int nk = -1;
              for( int k = 0; k < 5; k++ )
              {
                if( cpos[k] != 0 && ( nk < 0 || cpos[k] < cpos[nk] ) )
                {
                  nk = k;
                }
              }
              if( nk < 0 )
              {
                break;
              }
              nbSbb.inPos[ nbSbb.num++ ] = uint8_t( cpos[nk] );
              cpos[nk] = 0;
            }
            for( int k = nbSbb.num; k < 5; k++ )
            {
              nbSbb.inPos[k] = 0;
            }
          }
          {
            //===== outside subband neighbours =====
            NbInfoOut&     nbOut  = sId2NbOut[ scanId ];
            const int      begSbb = scanId - ( scanId & (groupSize-1) ); // first pos in current subblock
            int            cpos[5];

            cpos[0] = ( posX + 1 < blkWidthNZOut                              ? ( raster2id[rpos+1           ] >= groupSize + begSbb ? raster2id[rpos+1           ] : 0 ) : 0 );
            cpos[1] = ( posX + 2 < blkWidthNZOut                              ? ( raster2id[rpos+2           ] >= groupSize + begSbb ? raster2id[rpos+2           ] : 0 ) : 0 );
            cpos[2] = ( posX + 1 < blkWidthNZOut && posY + 1 < blkHeightNZOut ? ( raster2id[rpos+1+blockWidth] >= groupSize + begSbb ? raster2id[rpos+1+blockWidth] : 0 ) : 0 );
            cpos[3] = ( posY + 1 < blkHeightNZOut                             ? ( raster2id[rpos+  blockWidth] >= groupSize + begSbb ? raster2id[rpos+  blockWidth] : 0 ) : 0 );
            cpos[4] = ( posY + 2 < blkHeightNZOut                             ? ( raster2id[rpos+2*blockWidth] >= groupSize + begSbb ? raster2id[rpos+2*blockWidth] : 0 ) : 0 );

            for( nbOut.num = 0; true; )
            {
              int nk = -1;
              for( int k = 0; k < 5; k++ )
              {
                if( cpos[k] != 0 && ( nk < 0 || cpos[k] < cpos[nk] ) )
                {
                  nk = k;
                }
              }
              if( nk < 0 )
              {
                break;
              }
              nbOut.outPos[ nbOut.num++ ] = uint16_t( cpos[nk] );
              cpos[nk] = 0;
            }
            for( int k = nbOut.num; k < 5; k++ )
            {
              nbOut.outPos[k] = 0;
            }
            nbOut.maxDist = ( scanId == 0 ? 0 : sId2NbOut[scanId-1].maxDist );
            for( int k = 0; k < nbOut.num; k++ )
            {
              if( nbOut.outPos[k] > nbOut.maxDist )
              {
                nbOut.maxDist = nbOut.outPos[k];
              }
            }
          }
        }

        // make it relative
        for( unsigned scanId = 0; scanId < totalValues; scanId++ )
        {
          NbInfoOut& nbOut  = sId2NbOut[scanId];
          const int  begSbb = scanId - ( scanId & (groupSize-1) ); // first pos in current subblock
          for( int k = 0; k < nbOut.num; k++ )
          {
            CHECK(begSbb > nbOut.outPos[k], "Position must be past sub block begin");
            nbOut.outPos[k] -= begSbb;
          }
          nbOut.maxDist -= scanId;
        }

        for( int chId = 0; chId < MAX_NUM_CHANNEL_TYPE; chId++ )
        {
          m_tuParameters[hd][vd][chId] = new TUParameters( *this, blockWidth, blockHeight, ChannelType(chId) );
        }
      }
    }
    m_scansInitialized = true;
  }

  void Rom::xUninitScanArrays()
  {
    if( !m_scansInitialized )
    {
      return;
    }
    for( int hd = 0; hd <= MAX_CU_DEPTH; hd++ )
    {
      for( int vd = 0; vd <= MAX_CU_DEPTH; vd++ )
      {
        NbInfoSbb*& sId2NbSbb = m_scanId2NbInfoSbbArray[hd][vd];
        NbInfoOut*& sId2NbOut = m_scanId2NbInfoOutArray[hd][vd];
        if( sId2NbSbb )
        {
          delete [] sId2NbSbb;
        }
        if( sId2NbOut )
        {
          delete [] sId2NbOut;
        }
        for( int chId = 0; chId < MAX_NUM_CHANNEL_TYPE; chId++ )
        {
          TUParameters*& tuPars = m_tuParameters[hd][vd][chId];
          if( tuPars )
          {
            delete tuPars;
          }
        }
      }
    }
    m_scansInitialized = false;
  }


  static Rom g_Rom;


  TUParameters::TUParameters( const Rom& rom, const unsigned width, const unsigned height, const ChannelType chType )
  {
    m_chType              = chType;
    m_width               = width;
    m_height              = height;
    const uint32_t nonzeroWidth  = getNonzeroTuSize(m_width);
    const uint32_t nonzeroHeight = getNonzeroTuSize(m_height);
    m_numCoeff                   = nonzeroWidth * nonzeroHeight;

    const int log2W = floorLog2(m_width);
    const int log2H = floorLog2(m_height);

    m_log2SbbWidth  = g_log2TxSubblockSize[log2W][log2H].width;
    m_log2SbbHeight = g_log2TxSubblockSize[log2W][log2H].height;
    m_log2SbbSize   = m_log2SbbWidth + m_log2SbbHeight;
    m_sbbSize       = 1 << m_log2SbbSize;
    m_sbbMask       = m_sbbSize - 1;
    m_widthInSbb    = nonzeroWidth >> m_log2SbbWidth;
    m_heightInSbb   = nonzeroHeight >> m_log2SbbHeight;
    m_numSbb        = m_widthInSbb * m_heightInSbb;

    SizeType        hsbb  = gp_sizeIdxInfo->idxFrom( m_widthInSbb  );
    SizeType        vsbb  = gp_sizeIdxInfo->idxFrom( m_heightInSbb );
    SizeType        hsId  = gp_sizeIdxInfo->idxFrom( m_width  );
    SizeType        vsId  = gp_sizeIdxInfo->idxFrom( m_height );
    m_scanSbbId2SbbPos           = g_scanOrder[SCAN_UNGROUPED][CoeffScanType::DIAG][hsbb][vsbb];
    m_scanId2BlkPos              = g_scanOrder[SCAN_GROUPED_4x4][CoeffScanType::DIAG][hsId][vsId];
    m_scanId2NbInfoSbb    = rom.getNbInfoSbb( log2W, log2H );
    m_scanId2NbInfoOut    = rom.getNbInfoOut( log2W, log2H );
    m_scanInfo            = new ScanInfo[ m_numCoeff ];
    for( int scanIdx = 0; scanIdx < m_numCoeff; scanIdx++ )
    {
      xSetScanInfo( m_scanInfo[scanIdx], scanIdx );
    }
  }


  void TUParameters::xSetScanInfo( ScanInfo& scanInfo, int scanIdx )
  {
    scanInfo.chType     = m_chType;
    scanInfo.tuWidth    = m_width;
    scanInfo.tuHeight   = m_height;
    scanInfo.sbbSize    = m_sbbSize;
    scanInfo.numSbb     = m_numSbb;
    scanInfo.scanIdx    = scanIdx;
    scanInfo.rasterPos  = m_scanId2BlkPos[scanIdx].idx;
    scanInfo.sbbPos     = m_scanSbbId2SbbPos[scanIdx >> m_log2SbbSize].idx;
    scanInfo.insidePos  = scanIdx & m_sbbMask;
    scanInfo.eosbb      = ( scanInfo.insidePos == 0 );
    scanInfo.spt        = SCAN_ISCSBB;
    if(  scanInfo.insidePos == m_sbbMask && scanIdx > scanInfo.sbbSize && scanIdx < m_numCoeff - 1 )
    {
      scanInfo.spt      = SCAN_SOCSBB;
    }
    else if( scanInfo.eosbb && scanIdx > 0 && scanIdx < m_numCoeff - m_sbbSize )
    {
      scanInfo.spt      = SCAN_EOCSBB;
    }
    scanInfo.posX = m_scanId2BlkPos[scanIdx].x;
    scanInfo.posY = m_scanId2BlkPos[scanIdx].y;
    if( scanIdx )
    {
      const int nextScanIdx = scanIdx - 1;
      const int diag        = m_scanId2BlkPos[nextScanIdx].x + m_scanId2BlkPos[nextScanIdx].y;
      if (isLuma(m_chType))
      {
        scanInfo.sigCtxOffsetNext = ( diag < 2 ? 8 : diag < 5 ?  4 : 0 );
        scanInfo.gtxCtxOffsetNext = ( diag < 1 ? 16 : diag < 3 ? 11 : diag < 10 ? 6 : 1 );
      }
      else
      {
        scanInfo.sigCtxOffsetNext = ( diag < 2 ? 4 : 0 );
        scanInfo.gtxCtxOffsetNext = ( diag < 1 ? 6 : 1 );
      }
      scanInfo.nextInsidePos      = nextScanIdx & m_sbbMask;
      scanInfo.nextNbInfoSbb      = m_scanId2NbInfoSbb[ nextScanIdx ];
      if( scanInfo.eosbb )
      {
        const int nextSbbPos  = m_scanSbbId2SbbPos[nextScanIdx >> m_log2SbbSize].idx;
        const int nextSbbPosY = nextSbbPos               / m_widthInSbb;
        const int nextSbbPosX = nextSbbPos - nextSbbPosY * m_widthInSbb;
        scanInfo.nextSbbRight = ( nextSbbPosX < m_widthInSbb  - 1 ? nextSbbPos + 1            : 0 );
        scanInfo.nextSbbBelow = ( nextSbbPosY < m_heightInSbb - 1 ? nextSbbPos + m_widthInSbb : 0 );
      }
    }
  }



  class RateEstimator
  {
  public:
    RateEstimator () {}
    ~RateEstimator() {}
    void initCtx  ( const TUParameters& tuPars, const TransformUnit& tu, const ComponentID compID, const FracBitsAccess& fracBitsAccess );

    inline const BinFracBits *sigSbbFracBits() const { return m_sigSbbFracBits; }
    inline const BinFracBits *sigFlagBits(unsigned stateId) const
    {
      return m_sigFracBits[std::max(((int) stateId) - 1, 0)];
    }
    inline const CoeffFracBits *gtxFracBits(unsigned stateId) const { return m_gtxFracBits; }
    inline int32_t lastOffset(unsigned scanIdx, int effWidth, int effHeight, bool reverseLast) const
    {
      if (reverseLast)
      {
        return m_lastBitsX[effWidth - 1 - m_scanId2Pos[scanIdx].x] + m_lastBitsY[effHeight - 1 - m_scanId2Pos[scanIdx].y];
      }
      else
      {
        return m_lastBitsX[m_scanId2Pos[scanIdx].x] + m_lastBitsY[m_scanId2Pos[scanIdx].y];
      }
    }

  private:
    void  xSetLastCoeffOffset ( const FracBitsAccess& fracBitsAccess, const TUParameters& tuPars, const TransformUnit& tu, const ComponentID compID );
    void  xSetSigSbbFracBits  ( const FracBitsAccess& fracBitsAccess, ChannelType chType );
    void  xSetSigFlagBits     ( const FracBitsAccess& fracBitsAccess, ChannelType chType );
    void  xSetGtxFlagBits     ( const FracBitsAccess& fracBitsAccess, ChannelType chType );

  private:
    static const unsigned sm_numCtxSetsSig    = 3;
    static const unsigned sm_numCtxSetsGtx    = 2;
    static const unsigned sm_maxNumSigSbbCtx  = 2;
    static const unsigned sm_maxNumSigCtx     = 12;
    static const unsigned sm_maxNumGtxCtx     = 21;

  private:
    const ScanElement * m_scanId2Pos;
    int32_t             m_lastBitsX      [ MAX_TB_SIZEY ];
    int32_t             m_lastBitsY      [ MAX_TB_SIZEY ];
    BinFracBits         m_sigSbbFracBits [ sm_maxNumSigSbbCtx ];
    BinFracBits         m_sigFracBits    [ sm_numCtxSetsSig   ][ sm_maxNumSigCtx ];
    CoeffFracBits       m_gtxFracBits                          [ sm_maxNumGtxCtx ];
  };

  void RateEstimator::initCtx( const TUParameters& tuPars, const TransformUnit& tu, const ComponentID compID, const FracBitsAccess& fracBitsAccess )
  {
    m_scanId2Pos = tuPars.m_scanId2BlkPos;
    xSetSigSbbFracBits  ( fracBitsAccess, tuPars.m_chType );
    xSetSigFlagBits     ( fracBitsAccess, tuPars.m_chType );
    xSetGtxFlagBits     ( fracBitsAccess, tuPars.m_chType );
    xSetLastCoeffOffset ( fracBitsAccess, tuPars, tu, compID );
  }

  void RateEstimator::xSetLastCoeffOffset( const FracBitsAccess& fracBitsAccess, const TUParameters& tuPars, const TransformUnit& tu, const ComponentID compID )
  {
    const ChannelType chType = toChannelType(compID);

    BinFracBits bits = { 0, 0 };

    if (isLuma(chType) && !CU::isIntra(*tu.cu) && !tu.depth)
    {
      bits = fracBitsAccess.getFracBitsArray(Ctx::QtRootCbf());
    }
    else if (tu.cu->ispMode != ISPType::NONE && isLuma(chType))
    {
      bool lastCbfIsInferred = false;
      if (CU::isISPLast(*tu.cu, tu.Y(), compID))
      {
        TransformUnit *tuPointer = tu.cu->firstTU;

        const uint32_t nTus = tu.cu->ispMode == ISPType::HOR ? tu.cu->lheight() >> floorLog2(tu.lheight())
                                                             : tu.cu->lwidth() >> floorLog2(tu.lwidth());

        lastCbfIsInferred = true;
        for (int tuIdx = 0; tuIdx < nTus - 1; tuIdx++)
        {
          if (TU::getCbfAtDepth(*tuPointer, COMPONENT_Y, tu.depth))
          {
            lastCbfIsInferred = false;
            break;
          }
          tuPointer = tuPointer->next;
        }
      }

      if (!lastCbfIsInferred)
      {
        const bool prevLumaCbf = TU::getPrevTuCbfAtDepth(tu, compID, tu.depth);
        bits = fracBitsAccess.getFracBitsArray(Ctx::QtCbf[compID](DeriveCtx::CtxQtCbf(compID, prevLumaCbf, true)));
      }
    }
    else
    {
      bits = fracBitsAccess.getFracBitsArray(Ctx::QtCbf[compID](DeriveCtx::CtxQtCbf(compID, tu.cbf[COMPONENT_Cb])));
    }

    const int32_t cbfDeltaBits = int32_t(bits.intBits[1]) - int32_t(bits.intBits[0]);

    for (int xy = 0; xy < 2; xy++)
    {
      const bool isY = xy != 0;

      const unsigned size       = isY ? tuPars.m_height : tuPars.m_width;
      const int      log2Size   = ceilLog2(size);
      const CtxSet  &ctxSetLast = (isY ? Ctx::LastY : Ctx::LastX)[to_underlying(chType)];
      const unsigned lastShift  = isLuma(chType) ? (log2Size + 1) >> 2 : Clip3<unsigned>(0, 2, size >> 3);
      const unsigned lastOffset = isLuma(chType) ? CoeffCodingContext::prefixCtx[log2Size] : 0;
      const int      nzSize     = getNonzeroTuSize(size);
      const int      maxCtxId   = g_groupIdx[nzSize - 1];

      int sumBits = isY ? cbfDeltaBits : 0;

      std::array<int32_t, LAST_SIGNIFICANT_GROUPS> ctxBits;
      ctxBits.fill(0);

      for (int ctxId = 0; ctxId <= maxCtxId; ctxId++)
      {
        ctxBits[ctxId] = sumBits + (ctxId > 3 ? ((ctxId - 2) >> 1) << SCALE_BITS : 0);

        if (ctxId < maxCtxId)
        {
          const BinFracBits bits = fracBitsAccess.getFracBitsArray(ctxSetLast(lastOffset + (ctxId >> lastShift)));

          ctxBits[ctxId] += bits.intBits[0];
          sumBits += bits.intBits[1];
        }
      }

      int32_t *lastBits = isY ? m_lastBitsY : m_lastBitsX;

      for (int pos = 0; pos < nzSize; pos++)
      {
        lastBits[pos] = ctxBits[g_groupIdx[pos]];
      }
    }
  }

  void RateEstimator::xSetSigSbbFracBits( const FracBitsAccess& fracBitsAccess, ChannelType chType )
  {
    const CtxSet &ctxSet = Ctx::SigCoeffGroup[to_underlying(chType)];
    for( unsigned ctxId = 0; ctxId < sm_maxNumSigSbbCtx; ctxId++ )
    {
      m_sigSbbFracBits[ ctxId ] = fracBitsAccess.getFracBitsArray( ctxSet( ctxId ) );
    }
  }

  void RateEstimator::xSetSigFlagBits( const FracBitsAccess& fracBitsAccess, ChannelType chType )
  {
    for( unsigned ctxSetId = 0; ctxSetId < sm_numCtxSetsSig; ctxSetId++ )
    {
      BinFracBits*    bits    = m_sigFracBits [ ctxSetId ];
      const CtxSet   &ctxSet  = Ctx::SigFlag[to_underlying(chType) + 2 * ctxSetId];
      const unsigned  numCtx  = isLuma(chType) ? 12 : 8;
      for( unsigned ctxId = 0; ctxId < numCtx; ctxId++ )
      {
        bits[ ctxId ] = fracBitsAccess.getFracBitsArray( ctxSet( ctxId ) );
      }
    }
  }

  void RateEstimator::xSetGtxFlagBits(const FracBitsAccess &fracBitsAccess, const ChannelType chType)
  {
    const auto     chIdx     = to_underlying(chType);
    const CtxSet  &ctxSetPar = Ctx::ParFlag[chIdx];
    const CtxSet  &ctxSetGt1 = Ctx::GtxFlag[2 + chIdx];
    const CtxSet  &ctxSetGt2 = Ctx::GtxFlag[chIdx];
    const unsigned numCtx    = isLuma(chType) ? 21 : 11;
    for( unsigned ctxId = 0; ctxId < numCtx; ctxId++ )
    {
      BinFracBits     fbPar = fracBitsAccess.getFracBitsArray( ctxSetPar( ctxId ) );
      BinFracBits     fbGt1 = fracBitsAccess.getFracBitsArray( ctxSetGt1( ctxId ) );
      BinFracBits     fbGt2 = fracBitsAccess.getFracBitsArray( ctxSetGt2( ctxId ) );
      CoeffFracBits&  cb    = m_gtxFracBits[ ctxId ];
      int32_t         par0  = (1<<SCALE_BITS) + int32_t(fbPar.intBits[0]);
      int32_t         par1  = (1<<SCALE_BITS) + int32_t(fbPar.intBits[1]);
      cb.bits[0] = 0;
      cb.bits[1] = fbGt1.intBits[0] + (1 << SCALE_BITS);
      cb.bits[2] = fbGt1.intBits[1] + par0 + fbGt2.intBits[0];
      cb.bits[3] = fbGt1.intBits[1] + par1 + fbGt2.intBits[0];
      cb.bits[4] = fbGt1.intBits[1] + par0 + fbGt2.intBits[1];
      cb.bits[5] = fbGt1.intBits[1] + par1 + fbGt2.intBits[1];
    }
  }





  /*================================================================================*/
  /*=====                                                                      =====*/
  /*=====   D A T A   S T R U C T U R E S                                      =====*/
  /*=====                                                                      =====*/
  /*================================================================================*/


  struct PQData
  {
    TCoeff  absLevel;
    int64_t deltaDist;
  };


  struct Decision
  {
    int64_t rdCost;
    TCoeff  absLevel;
    int     prevId;
  };




  /*================================================================================*/
  /*=====                                                                      =====*/
  /*=====   P R E - Q U A N T I Z E R                                          =====*/
  /*=====                                                                      =====*/
  /*================================================================================*/

  class Quantizer
  {
  public:
    Quantizer() {}
    void  dequantBlock         ( const TransformUnit& tu, const ComponentID compID, const QpParam& cQP, CoeffBuf& recCoeff, bool enableScalingLists, int* piDequantCoef ) const;
    void  initQuantBlock       ( const TransformUnit& tu, const ComponentID compID, const QpParam& cQP, const double lambda, int gValue );
    inline void   preQuantCoeff( const TCoeff absCoeff, PQData *pqData, TCoeff quanCoeff ) const;
    inline TCoeff getLastThreshold() const { return m_thresLast; }
    inline TCoeff getSSbbThreshold() const { return m_thresSSbb; }

    inline int64_t getQScale()       const { return m_QScale; }
  private:
    // quantization
    int               m_QShift;
    int64_t           m_QAdd;
    int64_t           m_QScale;
    TCoeff            m_maxQIdx;
    TCoeff            m_thresLast;
    TCoeff            m_thresSSbb;
    // distortion normalization
    int               m_DistShift;
    int64_t           m_DistAdd;
    int64_t           m_DistStepAdd;
    int64_t           m_DistOrgFact;
  };

  inline int ceil_log2(uint64_t x)
  {
    static const uint64_t t[6] = { 0xFFFFFFFF00000000ull, 0x00000000FFFF0000ull, 0x000000000000FF00ull, 0x00000000000000F0ull, 0x000000000000000Cull, 0x0000000000000002ull };
    int y = (((x & (x - 1)) == 0) ? 0 : 1);
    int j = 32;
    for( int i = 0; i < 6; i++)
    {
      int k = (((x & t[i]) == 0) ? 0 : j);
      y += k;
      x >>= k;
      j >>= 1;
    }
    return y;
  }
  void Quantizer::initQuantBlock(const TransformUnit& tu, const ComponentID compID, const QpParam& cQP, const double lambda, int gValue = -1)
  {
    CHECKD( lambda <= 0.0, "Lambda must be greater than 0" );

    const int         qpDQ                  = cQP.Qp(tu.mtsIdx[compID] == MtsType::SKIP) + 1;
    const int         qpPer                 = qpDQ / 6;
    const int         qpRem                 = qpDQ - 6 * qpPer;
    const SPS&        sps                   = *tu.cs->sps;
    const CompArea&   area                  = tu.blocks[ compID ];
    const ChannelType chType                = toChannelType( compID );
    const int         channelBitDepth       = sps.getBitDepth( chType );
    const int         maxLog2TrDynamicRange = sps.getMaxLog2TrDynamicRange( chType );
    const int         nomTransformShift     = getTransformShift( channelBitDepth, area.size(), maxLog2TrDynamicRange );

    const bool clipTransformShift =
      tu.mtsIdx[compID] == MtsType::SKIP && sps.getSpsRangeExtension().getExtendedPrecisionProcessingFlag();

    const bool    needsSqrt2ScaleAdjustment = TU::needsSqrt2Scale(tu, compID);
    const int         transformShift        = ( clipTransformShift ? std::max<int>( 0, nomTransformShift ) : nomTransformShift ) + (needsSqrt2ScaleAdjustment?-1:0);
    // quant parameters
    m_QShift                    = QUANT_SHIFT  - 1 + qpPer + transformShift;
    m_QAdd                      = -( ( 3 << m_QShift ) >> 1 );
    int               invShift  = IQUANT_SHIFT + 1 - qpPer - transformShift;
    m_QScale                    = g_quantScales[needsSqrt2ScaleAdjustment?1:0][ qpRem ];
    const unsigned    qIdxBD    = std::min<unsigned>( maxLog2TrDynamicRange + 1, 8*sizeof(Intermediate_Int) + invShift - IQUANT_SHIFT - 1 );
    m_maxQIdx                   = ( 1 << (qIdxBD-1) ) - 4;
    m_thresLast                 = TCoeff((int64_t(4) << m_QShift));
    m_thresSSbb                 = TCoeff((int64_t(3) << m_QShift));
    // distortion calculation parameters
    const int64_t qScale        = (gValue==-1) ? m_QScale : gValue;
    const int nomDShift =
      SCALE_BITS - 2 * (nomTransformShift + DISTORTION_PRECISION_ADJUSTMENT(channelBitDepth)) + m_QShift + (needsSqrt2ScaleAdjustment ? 1 : 0);
    const double  qScale2       = double( qScale * qScale );
    const double  nomDistFactor = ( nomDShift < 0 ? 1.0/(double(int64_t(1)<<(-nomDShift))*qScale2*lambda) : double(int64_t(1)<<nomDShift)/(qScale2*lambda) );
    const int64_t pow2dfShift   = (int64_t)( nomDistFactor * qScale2 ) + 1;
    const int     dfShift       = ceil_log2( pow2dfShift );
    m_DistShift                 = 62 + m_QShift - 2*maxLog2TrDynamicRange - dfShift;
    m_DistAdd                   = (int64_t(1) << m_DistShift) >> 1;
    m_DistStepAdd               = (int64_t)( nomDistFactor * double(int64_t(1)<<(m_DistShift+m_QShift)) + .5 );
    m_DistOrgFact               = (int64_t)( nomDistFactor * double(int64_t(1)<<(m_DistShift+1       )) + .5 );
  }

  void Quantizer::dequantBlock( const TransformUnit& tu, const ComponentID compID, const QpParam& cQP, CoeffBuf& recCoeff, bool enableScalingLists, int* piDequantCoef) const
  {

    //----- set basic parameters -----
    const CompArea&     area      = tu.blocks[ compID ];
    const int           numCoeff  = area.area();
    const SizeType      hsId      = gp_sizeIdxInfo->idxFrom( area.width  );
    const SizeType      vsId      = gp_sizeIdxInfo->idxFrom( area.height );
    const ScanElement  *scan      = g_scanOrder[SCAN_GROUPED_4x4][CoeffScanType::DIAG][hsId][vsId];
    const TCoeff*       qCoeff    = tu.getCoeffs( compID ).buf;
          TCoeff*       tCoeff    = recCoeff.buf;

    //----- reset coefficients and get last scan index -----
    ::memset( tCoeff, 0, numCoeff * sizeof(TCoeff) );
    int lastScanIdx = -1;
    for( int scanIdx = numCoeff - 1; scanIdx >= 0; scanIdx-- )
    {
      if (qCoeff[scan[scanIdx].idx])
      {
        lastScanIdx = scanIdx;
        break;
      }
    }
    if( lastScanIdx < 0 )
    {
      return;
    }

    //----- set dequant parameters -----
    const int         qpDQ                  = cQP.Qp(tu.mtsIdx[compID] == MtsType::SKIP) + 1;
    const int         qpPer                 = qpDQ / 6;
    const int         qpRem                 = qpDQ - 6 * qpPer;
    const SPS&        sps                   = *tu.cs->sps;
    const ChannelType chType                = toChannelType( compID );
    const int         channelBitDepth       = sps.getBitDepth( chType );
    const int         maxLog2TrDynamicRange = sps.getMaxLog2TrDynamicRange( chType );
    const TCoeff      minTCoeff             = -( 1 << maxLog2TrDynamicRange );
    const TCoeff      maxTCoeff             =  ( 1 << maxLog2TrDynamicRange ) - 1;
    const int         nomTransformShift     = getTransformShift( channelBitDepth, area.size(), maxLog2TrDynamicRange );

    const bool clipTransformShift =
      tu.mtsIdx[compID] == MtsType::SKIP && sps.getSpsRangeExtension().getExtendedPrecisionProcessingFlag();

    const bool    needsSqrt2ScaleAdjustment = TU::needsSqrt2Scale(tu, compID);
    const int         transformShift        = ( clipTransformShift ? std::max<int>( 0, nomTransformShift ) : nomTransformShift ) + (needsSqrt2ScaleAdjustment?-1:0);
    Intermediate_Int  shift                 = IQUANT_SHIFT + 1 - qpPer - transformShift + (enableScalingLists ? LOG2_SCALING_LIST_NEUTRAL_VALUE : 0);
    Intermediate_Int  invQScale             = g_invQuantScales[needsSqrt2ScaleAdjustment?1:0][ qpRem ];
    Intermediate_Int  add = (shift < 0) ? 0 : ((1 << shift) >> 1);
    //----- dequant coefficients -----
    for( int state = 0, scanIdx = lastScanIdx; scanIdx >= 0; scanIdx-- )
    {
      const unsigned  rasterPos = scan[scanIdx].idx;
      const TCoeff&   level     = qCoeff[ rasterPos ];
      if( level )
      {
        if (enableScalingLists)
        {
          invQScale = piDequantCoef[rasterPos];//scalingfactor*levelScale
        }
        if (shift < 0 && (enableScalingLists || scanIdx == lastScanIdx))
        {
          invQScale <<= -shift;
        }
        Intermediate_Int qIdx = 2 * level + (level > 0 ? -(state >> 1) : (state >> 1));
        CHECK(qIdx < minTCoeff || qIdx > maxTCoeff, "TransCoeffLevel outside allowable range");

        int64_t  nomTCoeff          = ((int64_t)qIdx * (int64_t)invQScale + add) >> ((shift < 0) ? 0 : shift);
        tCoeff[rasterPos]           = (TCoeff)Clip3<int64_t>(minTCoeff, maxTCoeff, nomTCoeff);
      }
      state = ( 32040 >> ((state<<2)+((level&1)<<1)) ) & 3;   // the 16-bit value "32040" represent the state transition table
    }
  }

  inline void Quantizer::preQuantCoeff(const TCoeff absCoeff, PQData *pqData, TCoeff quanCoeff) const
  {
    int64_t scaledOrg = int64_t( absCoeff ) * quanCoeff;
    TCoeff  qIdx      = std::max<TCoeff>( 1, std::min<TCoeff>( m_maxQIdx, TCoeff( ( scaledOrg + m_QAdd ) >> m_QShift ) ) );
    int64_t scaledAdd = qIdx * m_DistStepAdd - scaledOrg * m_DistOrgFact;
    PQData& pq_a      = pqData[ qIdx & 3 ];
    pq_a.deltaDist    = ( scaledAdd * qIdx + m_DistAdd ) >> m_DistShift;
    pq_a.absLevel     = ( ++qIdx ) >> 1;
    scaledAdd        += m_DistStepAdd;
    PQData& pq_b      = pqData[ qIdx & 3 ];
    pq_b.deltaDist    = ( scaledAdd * qIdx + m_DistAdd ) >> m_DistShift;
    pq_b.absLevel     = ( ++qIdx ) >> 1;
    scaledAdd        += m_DistStepAdd;
    PQData& pq_c      = pqData[ qIdx & 3 ];
    pq_c.deltaDist    = ( scaledAdd * qIdx + m_DistAdd ) >> m_DistShift;
    pq_c.absLevel     = ( ++qIdx ) >> 1;
    scaledAdd        += m_DistStepAdd;
    PQData& pq_d      = pqData[ qIdx & 3 ];
    pq_d.deltaDist    = ( scaledAdd * qIdx + m_DistAdd ) >> m_DistShift;
    pq_d.absLevel     = ( ++qIdx ) >> 1;
  }







  /*================================================================================*/
  /*=====                                                                      =====*/
  /*=====   T C Q   S T A T E                                                  =====*/
  /*=====                                                                      =====*/
  /*================================================================================*/

  class State;

  struct SbbCtx
  {
    uint8_t*  sbbFlags;
    uint8_t*  levels;
  };

  class CommonCtx
  {
  public:
    CommonCtx() : m_currSbbCtx( m_allSbbCtx ), m_prevSbbCtx( m_currSbbCtx + 4 ) {}

    inline void swap() { std::swap(m_currSbbCtx, m_prevSbbCtx); }

    inline void reset( const TUParameters& tuPars, const RateEstimator &rateEst)
    {
      m_nbInfo = tuPars.m_scanId2NbInfoOut;
      ::memcpy( m_sbbFlagBits, rateEst.sigSbbFracBits(), 2*sizeof(BinFracBits) );
      const int numSbb    = tuPars.m_numSbb;
      const int chunkSize = numSbb + tuPars.m_numCoeff;
      uint8_t*  nextMem   = m_memory;
      for( int k = 0; k < 8; k++, nextMem += chunkSize )
      {
        m_allSbbCtx[k].sbbFlags = nextMem;
        m_allSbbCtx[k].levels   = nextMem + numSbb;
      }
    }

    inline void update(const ScanInfo &scanInfo, const State *prevState, State &currState);

  private:
    const NbInfoOut*            m_nbInfo;
    BinFracBits                 m_sbbFlagBits[2];
    SbbCtx                      m_allSbbCtx  [8];
    SbbCtx*                     m_currSbbCtx;
    SbbCtx*                     m_prevSbbCtx;
    uint8_t                     m_memory[ 8 * ( MAX_TB_SIZEY * MAX_TB_SIZEY + MLS_GRP_NUM ) ];
  };

#if JVET_V0106_DEP_QUANT_ENC_OPT
#define RICEMAX 64
#define RICE_ORDER_MAX 16
  const int32_t g_goRiceBits[RICE_ORDER_MAX][RICEMAX] =
#else
#define RICEMAX 32
  const int32_t g_goRiceBits[4][RICEMAX] =
#endif
  {
#if JVET_V0106_DEP_QUANT_ENC_OPT
    { 32768, 65536, 98304, 131072, 163840, 196608, 262144, 262144, 327680, 327680, 327680, 327680, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288 },
    { 65536, 65536, 98304, 98304, 131072, 131072, 163840, 163840, 196608, 196608, 229376, 229376, 294912, 294912, 294912, 294912, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520 },
    { 98304, 98304, 98304, 98304, 131072, 131072, 131072, 131072, 163840, 163840, 163840, 163840, 196608, 196608, 196608, 196608, 229376, 229376, 229376, 229376, 262144, 262144, 262144, 262144, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752 },
    { 131072, 131072, 131072, 131072, 131072, 131072, 131072, 131072, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448 },
    { 163840, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144 },
    { 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376 },
    { 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376 },
    { 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144, 262144 },
    { 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912, 294912 },
    { 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680 },
    { 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448 },
    { 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216 },
    { 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984 },
    { 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752 },
    { 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520, 491520 },
    { 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288, 524288 },
#else
    { 32768,  65536,  98304, 131072, 163840, 196608, 262144, 262144, 327680, 327680, 327680, 327680, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 393216, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752, 458752},
    { 65536,  65536,  98304,  98304, 131072, 131072, 163840, 163840, 196608, 196608, 229376, 229376, 294912, 294912, 294912, 294912, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 360448, 425984, 425984, 425984, 425984, 425984, 425984, 425984, 425984},
    { 98304,  98304,  98304,  98304, 131072, 131072, 131072, 131072, 163840, 163840, 163840, 163840, 196608, 196608, 196608, 196608, 229376, 229376, 229376, 229376, 262144, 262144, 262144, 262144, 327680, 327680, 327680, 327680, 327680, 327680, 327680, 327680},
    {131072, 131072, 131072, 131072, 131072, 131072, 131072, 131072, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 163840, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 196608, 229376, 229376, 229376, 229376, 229376, 229376, 229376, 229376}
#endif
  };

  class State
  {
    friend class CommonCtx;
  public:
    State( const RateEstimator& rateEst, CommonCtx& commonCtx, const int stateId );

    template<uint8_t numIPos>
    inline void updateState(const ScanInfo &scanInfo, const State *prevStates, const Decision &decision, const int baseLevel, const bool extRiceFlag);
    inline void updateStateEOS(const ScanInfo &scanInfo, const State *prevStates, const State *skipStates,
                               const Decision &decision, const int baseLevel, const bool extRiceFlag);

    inline void init()
    {
      m_rdCost        = std::numeric_limits<int64_t>::max()>>1;
      m_numSigSbb     = 0;
      m_remRegBins    = 4;  // just large enough for last scan pos
      m_refSbbCtxId   = -1;
      m_sigFracBits   = m_sigFracBitsArray[ 0 ];
      m_coeffFracBits = m_gtxFracBitsArray[ 0 ];
      m_goRicePar     = 0;
      m_goRiceZero    = 0;
    }
    void checkRdCosts( const ScanPosType spt, const PQData& pqDataA, const PQData& pqDataB, Decision& decisionA, Decision& decisionB ) const
    {
      const int32_t*  goRiceTab = g_goRiceBits[m_goRicePar];
      int64_t         rdCostA   = m_rdCost + pqDataA.deltaDist;
      int64_t         rdCostB   = m_rdCost + pqDataB.deltaDist;
      int64_t         rdCostZ   = m_rdCost;
      if (m_remRegBins >= 4)
      {
        if (pqDataA.absLevel < 4)
        {
          rdCostA += m_coeffFracBits.bits[pqDataA.absLevel];
        }
        else
        {
          const TCoeff value = (pqDataA.absLevel - 4) >> 1;
          rdCostA +=
            m_coeffFracBits.bits[pqDataA.absLevel - (value << 1)] + goRiceTab[value < RICEMAX ? value : RICEMAX - 1];
        }
        if (pqDataB.absLevel < 4)
        {
          rdCostB += m_coeffFracBits.bits[pqDataB.absLevel];
        }
        else
        {
          const TCoeff value = (pqDataB.absLevel - 4) >> 1;
          rdCostB +=
            m_coeffFracBits.bits[pqDataB.absLevel - (value << 1)] + goRiceTab[value < RICEMAX ? value : RICEMAX - 1];
        }
        if (spt == SCAN_ISCSBB)
        {
          rdCostA += m_sigFracBits.intBits[1];
          rdCostB += m_sigFracBits.intBits[1];
          rdCostZ += m_sigFracBits.intBits[0];
        }
        else if (spt == SCAN_SOCSBB)
        {
          rdCostA += m_sbbFracBits.intBits[1] + m_sigFracBits.intBits[1];
          rdCostB += m_sbbFracBits.intBits[1] + m_sigFracBits.intBits[1];
          rdCostZ += m_sbbFracBits.intBits[1] + m_sigFracBits.intBits[0];
        }
        else if (m_numSigSbb)
        {
          rdCostA += m_sigFracBits.intBits[1];
          rdCostB += m_sigFracBits.intBits[1];
          rdCostZ += m_sigFracBits.intBits[0];
        }
        else
        {
          rdCostZ = decisionA.rdCost;
        }
      }
      else
      {
        rdCostA +=
          (1 << SCALE_BITS)
          + goRiceTab[pqDataA.absLevel <= m_goRiceZero ? pqDataA.absLevel - 1
                                                       : (pqDataA.absLevel < RICEMAX ? pqDataA.absLevel : RICEMAX - 1)];
        rdCostB +=
          (1 << SCALE_BITS)
          + goRiceTab[pqDataB.absLevel <= m_goRiceZero ? pqDataB.absLevel - 1
                                                       : (pqDataB.absLevel < RICEMAX ? pqDataB.absLevel : RICEMAX - 1)];
        rdCostZ += goRiceTab[m_goRiceZero];
      }
      if (rdCostA < decisionA.rdCost)
      {
        decisionA.rdCost   = rdCostA;
        decisionA.absLevel = pqDataA.absLevel;
        decisionA.prevId   = m_stateId;
      }
      if (rdCostZ < decisionA.rdCost)
      {
        decisionA.rdCost   = rdCostZ;
        decisionA.absLevel = 0;
        decisionA.prevId   = m_stateId;
      }
      if (rdCostB < decisionB.rdCost)
      {
        decisionB.rdCost   = rdCostB;
        decisionB.absLevel = pqDataB.absLevel;
        decisionB.prevId   = m_stateId;
      }
    }

    inline void checkRdCostStart(int32_t lastOffset, const PQData &pqData, Decision &decision) const
    {
      int64_t rdCost = pqData.deltaDist + lastOffset;
      if (pqData.absLevel < 4)
      {
        rdCost += m_coeffFracBits.bits[pqData.absLevel];
      }
      else
      {
        const TCoeff value = (pqData.absLevel - 4) >> 1;
        rdCost += m_coeffFracBits.bits[pqData.absLevel - (value << 1)] + g_goRiceBits[m_goRicePar][value < RICEMAX ? value : RICEMAX-1];
      }
      if( rdCost < decision.rdCost )
      {
        decision.rdCost   = rdCost;
        decision.absLevel = pqData.absLevel;
        decision.prevId   = -1;
      }
    }

    inline void checkRdCostSkipSbb(Decision &decision) const
    {
      int64_t rdCost = m_rdCost + m_sbbFracBits.intBits[0];
      if( rdCost < decision.rdCost )
      {
        decision.rdCost   = rdCost;
        decision.absLevel = 0;
        decision.prevId   = 4+m_stateId;
      }
    }

    inline void checkRdCostSkipSbbZeroOut(Decision &decision) const
    {
      int64_t rdCost = m_rdCost + m_sbbFracBits.intBits[0];
      decision.rdCost = rdCost;
      decision.absLevel = 0;
      decision.prevId = 4 + m_stateId;
    }

  private:
    int64_t                   m_rdCost;
    uint16_t                  m_absLevelsAndCtxInit[24];  // 16x8bit for abs levels + 16x16bit for ctx init id
    int8_t                    m_numSigSbb;
    int                       m_remRegBins;
    int8_t                    m_refSbbCtxId;
    BinFracBits               m_sbbFracBits;
    BinFracBits               m_sigFracBits;
    CoeffFracBits             m_coeffFracBits;
    int8_t                    m_goRicePar;
    int8_t                    m_goRiceZero;
    const int8_t              m_stateId;
    const BinFracBits*const   m_sigFracBitsArray;
    const CoeffFracBits*const m_gtxFracBitsArray;
    CommonCtx&                m_commonCtx;
  public:
    unsigned                  effWidth;
    unsigned                  effHeight;
  };

  State::State( const RateEstimator& rateEst, CommonCtx& commonCtx, const int stateId )
    : m_sbbFracBits     { { 0, 0 } }
    , m_stateId         ( stateId )
    , m_sigFracBitsArray( rateEst.sigFlagBits(stateId) )
    , m_gtxFracBitsArray( rateEst.gtxFracBits(stateId) )
    , m_commonCtx       ( commonCtx )
  {
  }

  template<uint8_t numIPos>
  inline void State::updateState(const ScanInfo &scanInfo, const State *prevStates, const Decision &decision, const int baseLevel, const bool extRiceFlag)
  {
    m_rdCost = decision.rdCost;
    if( decision.prevId > -2 )
    {
      if( decision.prevId >= 0 )
      {
        const State*  prvState  = prevStates            +   decision.prevId;
        m_numSigSbb             = prvState->m_numSigSbb + !!decision.absLevel;
        m_refSbbCtxId           = prvState->m_refSbbCtxId;
        m_sbbFracBits           = prvState->m_sbbFracBits;
        m_remRegBins            = prvState->m_remRegBins - 1;
        m_goRicePar             = prvState->m_goRicePar;
        if( m_remRegBins >= 4 )
        {
          m_remRegBins -= (decision.absLevel < 2 ? (unsigned)decision.absLevel : 3);
        }
        ::memcpy( m_absLevelsAndCtxInit, prvState->m_absLevelsAndCtxInit, 48*sizeof(uint8_t) );
      }
      else
      {
        m_numSigSbb     =  1;
        m_refSbbCtxId   = -1;
        int ctxBinSampleRatio = isLuma(scanInfo.chType) ? MAX_TU_LEVEL_CTX_CODED_BIN_CONSTRAINT_LUMA
                                                        : MAX_TU_LEVEL_CTX_CODED_BIN_CONSTRAINT_CHROMA;
        m_remRegBins = (effWidth * effHeight * ctxBinSampleRatio) / 16 - (decision.absLevel < 2 ? (unsigned)decision.absLevel : 3);
        ::memset( m_absLevelsAndCtxInit, 0, 48*sizeof(uint8_t) );
      }

      uint8_t* levels               = reinterpret_cast<uint8_t*>(m_absLevelsAndCtxInit);
      levels[ scanInfo.insidePos ]  = (uint8_t)std::min<TCoeff>( 255, decision.absLevel );

      if (m_remRegBins >= 4)
      {
        TCoeff  tinit = m_absLevelsAndCtxInit[8 + scanInfo.nextInsidePos];
        TCoeff  sumAbs1 = (tinit >> 3) & 31;
        TCoeff  sumNum = tinit & 7;
#define UPDATE(k) {TCoeff t=levels[scanInfo.nextNbInfoSbb.inPos[k]]; sumAbs1+=std::min<TCoeff>(4+(t&1),t); sumNum+=!!t; }
        if (numIPos == 1)
        {
          UPDATE(0);
        }
        else if (numIPos == 2)
        {
          UPDATE(0);
          UPDATE(1);
        }
        else if (numIPos == 3)
        {
          UPDATE(0);
          UPDATE(1);
          UPDATE(2);
        }
        else if (numIPos == 4)
        {
          UPDATE(0);
          UPDATE(1);
          UPDATE(2);
          UPDATE(3);
        }
        else if (numIPos == 5)
        {
          UPDATE(0);
          UPDATE(1);
          UPDATE(2);
          UPDATE(3);
          UPDATE(4);
        }
#undef UPDATE
        TCoeff sumGt1 = sumAbs1 - sumNum;
        m_sigFracBits = m_sigFracBitsArray[scanInfo.sigCtxOffsetNext + std::min<TCoeff>( (sumAbs1+1)>>1, 3 )];
        m_coeffFracBits = m_gtxFracBitsArray[scanInfo.gtxCtxOffsetNext + (sumGt1 < 4 ? sumGt1 : 4)];

        TCoeff  sumAbs = m_absLevelsAndCtxInit[8 + scanInfo.nextInsidePos] >> 8;
#define UPDATE(k) {TCoeff t=levels[scanInfo.nextNbInfoSbb.inPos[k]]; sumAbs+=t; }
        if (numIPos == 1)
        {
          UPDATE(0);
        }
        else if (numIPos == 2)
        {
          UPDATE(0);
          UPDATE(1);
        }
        else if (numIPos == 3)
        {
          UPDATE(0);
          UPDATE(1);
          UPDATE(2);
        }
        else if (numIPos == 4)
        {
          UPDATE(0);
          UPDATE(1);
          UPDATE(2);
          UPDATE(3);
        }
        else if (numIPos == 5)
        {
          UPDATE(0);
          UPDATE(1);
          UPDATE(2);
          UPDATE(3);
          UPDATE(4);
        }
#undef UPDATE
        if (extRiceFlag)
        {
          unsigned currentShift = CoeffCodingContext::templateAbsCompare(sumAbs);
          sumAbs = sumAbs >> currentShift;
          int sumAll = std::max(std::min(31, (int)sumAbs - (int)baseLevel), 0);
          m_goRicePar = g_goRiceParsCoeff[sumAll];
          m_goRicePar += currentShift;
        }
        else
        {
          int sumAll = std::max(std::min(31, (int)sumAbs - 4 * 5), 0);
          m_goRicePar = g_goRiceParsCoeff[sumAll];
        }
      }
      else
      {
        TCoeff  sumAbs = m_absLevelsAndCtxInit[8 + scanInfo.nextInsidePos] >> 8;
#define UPDATE(k) {TCoeff t=levels[scanInfo.nextNbInfoSbb.inPos[k]]; sumAbs+=t; }
        if (numIPos == 1)
        {
          UPDATE(0);
        }
        else if (numIPos == 2)
        {
          UPDATE(0);
          UPDATE(1);
        }
        else if (numIPos == 3)
        {
          UPDATE(0);
          UPDATE(1);
          UPDATE(2);
        }
        else if (numIPos == 4)
        {
          UPDATE(0);
          UPDATE(1);
          UPDATE(2);
          UPDATE(3);
        }
        else if (numIPos == 5)
        {
          UPDATE(0);
          UPDATE(1);
          UPDATE(2);
          UPDATE(3);
          UPDATE(4);
        }
#undef UPDATE
        if (extRiceFlag)
        {
          unsigned currentShift = CoeffCodingContext::templateAbsCompare(sumAbs);
          sumAbs = sumAbs >> currentShift;
          sumAbs = std::min<TCoeff>(31, sumAbs);
          m_goRicePar = g_goRiceParsCoeff[sumAbs];
          m_goRicePar += currentShift;
        }
        else
        {
          sumAbs = std::min<TCoeff>(31, sumAbs);
          m_goRicePar = g_goRiceParsCoeff[sumAbs];
        }
        m_goRiceZero = g_goRicePosCoeff0(m_stateId, m_goRicePar);
      }
    }
  }

  inline void State::updateStateEOS(const ScanInfo &scanInfo, const State *prevStates, const State *skipStates,
                                    const Decision &decision, const int baseLevel, const bool extRiceFlag)
  {
    m_rdCost = decision.rdCost;
    if( decision.prevId > -2 )
    {
      const State* prvState = 0;
      if( decision.prevId  >= 4 )
      {
        CHECK( decision.absLevel != 0, "cannot happen" );
        prvState     = skipStates + ( decision.prevId - 4 );
        m_numSigSbb  = 0;
        m_remRegBins = prvState->m_remRegBins;
        ::memset( m_absLevelsAndCtxInit, 0, 16*sizeof(uint8_t) );
      }
      else if( decision.prevId  >= 0 )
      {
        prvState      = prevStates            +   decision.prevId;
        m_numSigSbb   = prvState->m_numSigSbb + !!decision.absLevel;
        m_remRegBins  = prvState->m_remRegBins - 1;
        m_remRegBins -= decision.absLevel < 2 ? (int) decision.absLevel : 3;
        ::memcpy( m_absLevelsAndCtxInit, prvState->m_absLevelsAndCtxInit, 16*sizeof(uint8_t) );
      }
      else
      {
        m_numSigSbb           = 1;
        int ctxBinSampleRatio = isLuma(scanInfo.chType) ? MAX_TU_LEVEL_CTX_CODED_BIN_CONSTRAINT_LUMA
                                                        : MAX_TU_LEVEL_CTX_CODED_BIN_CONSTRAINT_CHROMA;
        m_remRegBins          = (effWidth * effHeight * ctxBinSampleRatio) / 16;
        m_remRegBins         -= decision.absLevel < 2 ? (unsigned) decision.absLevel : 3;
        ::memset( m_absLevelsAndCtxInit, 0, 16*sizeof(uint8_t) );
      }
      reinterpret_cast<uint8_t*>(m_absLevelsAndCtxInit)[ scanInfo.insidePos ] = (uint8_t)std::min<TCoeff>( 255, decision.absLevel );

      m_commonCtx.update( scanInfo, prvState, *this );

      if (m_remRegBins >= 4)
      {
        TCoeff  tinit   = m_absLevelsAndCtxInit[8 + scanInfo.nextInsidePos];
        TCoeff  sumNum  =   tinit        & 7;
        TCoeff  sumAbs1 = ( tinit >> 3 ) & 31;
        TCoeff  sumGt1  = sumAbs1 - sumNum;
        TCoeff  sumAbs  = tinit >> 8;
        m_sigFracBits   = m_sigFracBitsArray[ scanInfo.sigCtxOffsetNext + std::min<TCoeff>( (sumAbs1+1)>>1, 3 ) ];
        m_coeffFracBits = m_gtxFracBitsArray[ scanInfo.gtxCtxOffsetNext + ( sumGt1  < 4 ? sumGt1  : 4 ) ];

        if (extRiceFlag)
        {
          unsigned currentShift = CoeffCodingContext::templateAbsCompare(sumAbs);
          sumAbs                = sumAbs >> currentShift;
          int sumAll            = std::max(std::min(31, (int) sumAbs - (int) baseLevel), 0);
          m_goRicePar           = g_goRiceParsCoeff[sumAll];
          m_goRicePar          += currentShift;
        }
        else
        {
          int sumAll  = std::max(std::min(31, (int) sumAbs - 4 * 5), 0);
          m_goRicePar = g_goRiceParsCoeff[sumAll];
        }
      }
      else
      {
        TCoeff tinit   = m_absLevelsAndCtxInit[8 + scanInfo.nextInsidePos];
        TCoeff sumAbs  = tinit >> 8;

        if (extRiceFlag)
        {
          unsigned currentShift = CoeffCodingContext::templateAbsCompare(sumAbs);
          sumAbs                = sumAbs >> currentShift;
          sumAbs                = std::min<TCoeff>(31, sumAbs);
          m_goRicePar           = g_goRiceParsCoeff[sumAbs];
          m_goRicePar          += currentShift;
        }
        else
        {
          sumAbs      = std::min<TCoeff>(31, sumAbs);
          m_goRicePar = g_goRiceParsCoeff[sumAbs];
        }
        m_goRiceZero = g_goRicePosCoeff0(m_stateId, m_goRicePar);
      }
    }
  }

  inline void CommonCtx::update(const ScanInfo &scanInfo, const State *prevState, State &currState)
  {
    uint8_t*    sbbFlags  = m_currSbbCtx[ currState.m_stateId ].sbbFlags;
    uint8_t*    levels    = m_currSbbCtx[ currState.m_stateId ].levels;
    std::size_t setCpSize = m_nbInfo[ scanInfo.scanIdx - 1 ].maxDist * sizeof(uint8_t);
    if( prevState && prevState->m_refSbbCtxId >= 0 )
    {
      ::memcpy( sbbFlags,                  m_prevSbbCtx[prevState->m_refSbbCtxId].sbbFlags,                  scanInfo.numSbb*sizeof(uint8_t) );
      ::memcpy( levels + scanInfo.scanIdx, m_prevSbbCtx[prevState->m_refSbbCtxId].levels + scanInfo.scanIdx, setCpSize );
    }
    else
    {
      ::memset( sbbFlags,                  0, scanInfo.numSbb*sizeof(uint8_t) );
      ::memset( levels + scanInfo.scanIdx, 0, setCpSize );
    }
    sbbFlags[ scanInfo.sbbPos ] = !!currState.m_numSigSbb;
    ::memcpy( levels + scanInfo.scanIdx, currState.m_absLevelsAndCtxInit, scanInfo.sbbSize*sizeof(uint8_t) );

    const int       sigNSbb   = ( ( scanInfo.nextSbbRight ? sbbFlags[ scanInfo.nextSbbRight ] : false ) || ( scanInfo.nextSbbBelow ? sbbFlags[ scanInfo.nextSbbBelow ] : false ) ? 1 : 0 );
    currState.m_numSigSbb     = 0;
    currState.m_goRicePar     = 0;
    currState.m_refSbbCtxId   = currState.m_stateId;
    currState.m_sbbFracBits   = m_sbbFlagBits[ sigNSbb ];

    uint16_t          templateCtxInit[16];
    const int         scanBeg   = scanInfo.scanIdx - scanInfo.sbbSize;
    const NbInfoOut*  nbOut     = m_nbInfo + scanBeg;
    const uint8_t*    absLevels = levels   + scanBeg;
    for( int id = 0; id < scanInfo.sbbSize; id++, nbOut++ )
    {
      if( nbOut->num )
      {
        TCoeff sumAbs = 0, sumAbs1 = 0, sumNum = 0;
#define UPDATE(k) {TCoeff t=absLevels[nbOut->outPos[k]]; sumAbs+=t; sumAbs1+=std::min<TCoeff>(4+(t&1),t); sumNum+=!!t; }
        UPDATE(0);
        if( nbOut->num > 1 )
        {
          UPDATE(1);
          if( nbOut->num > 2 )
          {
            UPDATE(2);
            if( nbOut->num > 3 )
            {
              UPDATE(3);
              if( nbOut->num > 4 )
              {
                UPDATE(4);
              }
            }
          }
        }
#undef UPDATE
        templateCtxInit[id] = uint16_t(sumNum) + ( uint16_t(sumAbs1) << 3 ) + ( (uint16_t)std::min<TCoeff>( 127, sumAbs ) << 8 );
      }
      else
      {
        templateCtxInit[id] = 0;
      }
    }
    ::memset( currState.m_absLevelsAndCtxInit,     0,               16*sizeof(uint8_t) );
    ::memcpy( currState.m_absLevelsAndCtxInit + 8, templateCtxInit, 16*sizeof(uint16_t) );
  }



  /*================================================================================*/
  /*=====                                                                      =====*/
  /*=====   T C Q                                                              =====*/
  /*=====                                                                      =====*/
  /*================================================================================*/
  class DepQuant : private RateEstimator
  {
  public:
    DepQuant();

    void    quant   ( TransformUnit& tu, const CCoeffBuf& srcCoeff, const ComponentID compID, const QpParam& cQP, const double lambda, const Ctx& ctx, TCoeff& absSum, bool enableScalingLists, int* quantCoeff );
    void    dequant ( const TransformUnit& tu, CoeffBuf& recCoeff, const ComponentID compID, const QpParam& cQP, bool enableScalingLists, int* quantCoeff );

    int m_baseLevel;
    bool m_extRiceRRCFlag;

  private:
    void    xDecideAndUpdate  ( const TCoeff absCoeff, const ScanInfo &scanInfo, bool zeroOut, TCoeff quantCoeff, int effWidth, int effHeight, bool reverseLast );
    void    xDecide           ( const ScanPosType spt, const TCoeff absCoeff, const int lastOffset, Decision* decisions, bool zeroOut, TCoeff quantCoeff );

  private:
    CommonCtx   m_commonCtx;
    State       m_allStates[ 12 ];
    State*      m_currStates;
    State*      m_prevStates;
    State*      m_skipStates;
    State       m_startState;
    Quantizer   m_quant;
    Decision    m_trellis[ MAX_TB_SIZEY * MAX_TB_SIZEY ][ 8 ];
  };


#define TINIT(x) {*this,m_commonCtx,x}
  DepQuant::DepQuant()
    : RateEstimator ()
    , m_commonCtx   ()
    , m_allStates   {TINIT(0),TINIT(1),TINIT(2),TINIT(3),TINIT(0),TINIT(1),TINIT(2),TINIT(3),TINIT(0),TINIT(1),TINIT(2),TINIT(3)}
    , m_currStates  (  m_allStates      )
    , m_prevStates  (  m_currStates + 4 )
    , m_skipStates  (  m_prevStates + 4 )
    , m_startState  TINIT(0)
  {}
#undef TINIT


  void DepQuant::dequant( const TransformUnit& tu,  CoeffBuf& recCoeff, const ComponentID compID, const QpParam& cQP, bool enableScalingLists, int* piDequantCoef )
  {
    m_quant.dequantBlock( tu, compID, cQP, recCoeff, enableScalingLists, piDequantCoef );
  }


#define DINIT(l,p) {std::numeric_limits<int64_t>::max()>>2,l,p}
  static const Decision startDec[8] = {DINIT(-1,-2),DINIT(-1,-2),DINIT(-1,-2),DINIT(-1,-2),DINIT(0,4),DINIT(0,5),DINIT(0,6),DINIT(0,7)};
#undef  DINIT


  void DepQuant::xDecide( const ScanPosType spt, const TCoeff absCoeff, const int lastOffset, Decision* decisions, bool zeroOut, TCoeff quanCoeff)
  {
    ::memcpy( decisions, startDec, 8*sizeof(Decision) );

    if( zeroOut )
    {
      if( spt==SCAN_EOCSBB )
      {
        m_skipStates[0].checkRdCostSkipSbbZeroOut( decisions[0] );
        m_skipStates[1].checkRdCostSkipSbbZeroOut( decisions[1] );
        m_skipStates[2].checkRdCostSkipSbbZeroOut( decisions[2] );
        m_skipStates[3].checkRdCostSkipSbbZeroOut( decisions[3] );
      }
      return;
    }

    PQData  pqData[4];
    m_quant.preQuantCoeff( absCoeff, pqData, quanCoeff );
    m_prevStates[0].checkRdCosts( spt, pqData[0], pqData[2], decisions[0], decisions[2]);
    m_prevStates[1].checkRdCosts( spt, pqData[0], pqData[2], decisions[2], decisions[0]);
    m_prevStates[2].checkRdCosts( spt, pqData[3], pqData[1], decisions[1], decisions[3]);
    m_prevStates[3].checkRdCosts( spt, pqData[3], pqData[1], decisions[3], decisions[1]);
    if( spt==SCAN_EOCSBB )
    {
      m_skipStates[0].checkRdCostSkipSbb(decisions[0]);
      m_skipStates[1].checkRdCostSkipSbb(decisions[1]);
      m_skipStates[2].checkRdCostSkipSbb(decisions[2]);
      m_skipStates[3].checkRdCostSkipSbb(decisions[3]);
    }

    m_startState.checkRdCostStart( lastOffset, pqData[0], decisions[0] );
    m_startState.checkRdCostStart( lastOffset, pqData[2], decisions[2] );
  }

  void DepQuant::xDecideAndUpdate( const TCoeff absCoeff, const ScanInfo &scanInfo, bool zeroOut, TCoeff quantCoeff, int effWidth, int effHeight, bool reverseLast )
  {
    Decision* decisions = m_trellis[ scanInfo.scanIdx ];

    std::swap( m_prevStates, m_currStates );

    xDecide( scanInfo.spt, absCoeff, lastOffset(scanInfo.scanIdx, effWidth, effHeight, reverseLast), decisions, zeroOut, quantCoeff );

    if( scanInfo.scanIdx )
    {
      if( scanInfo.eosbb )
      {
        m_commonCtx.swap();
        m_currStates[0].updateStateEOS( scanInfo, m_prevStates, m_skipStates, decisions[0], m_baseLevel, m_extRiceRRCFlag );
        m_currStates[1].updateStateEOS( scanInfo, m_prevStates, m_skipStates, decisions[1], m_baseLevel, m_extRiceRRCFlag );
        m_currStates[2].updateStateEOS( scanInfo, m_prevStates, m_skipStates, decisions[2], m_baseLevel, m_extRiceRRCFlag );
        m_currStates[3].updateStateEOS( scanInfo, m_prevStates, m_skipStates, decisions[3], m_baseLevel, m_extRiceRRCFlag );
        ::memcpy( decisions+4, decisions, 4*sizeof(Decision) );
      }
      else if( !zeroOut )
      {
        switch( scanInfo.nextNbInfoSbb.num )
        {
        case 0:
          m_currStates[0].updateState<0>(scanInfo, m_prevStates, decisions[0], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[1].updateState<0>(scanInfo, m_prevStates, decisions[1], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[2].updateState<0>(scanInfo, m_prevStates, decisions[2], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[3].updateState<0>(scanInfo, m_prevStates, decisions[3], m_baseLevel, m_extRiceRRCFlag);
          break;
        case 1:
          m_currStates[0].updateState<1>(scanInfo, m_prevStates, decisions[0], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[1].updateState<1>(scanInfo, m_prevStates, decisions[1], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[2].updateState<1>(scanInfo, m_prevStates, decisions[2], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[3].updateState<1>(scanInfo, m_prevStates, decisions[3], m_baseLevel, m_extRiceRRCFlag);
          break;
        case 2:
          m_currStates[0].updateState<2>(scanInfo, m_prevStates, decisions[0], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[1].updateState<2>(scanInfo, m_prevStates, decisions[1], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[2].updateState<2>(scanInfo, m_prevStates, decisions[2], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[3].updateState<2>(scanInfo, m_prevStates, decisions[3], m_baseLevel, m_extRiceRRCFlag);
          break;
        case 3:
          m_currStates[0].updateState<3>(scanInfo, m_prevStates, decisions[0], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[1].updateState<3>(scanInfo, m_prevStates, decisions[1], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[2].updateState<3>(scanInfo, m_prevStates, decisions[2], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[3].updateState<3>(scanInfo, m_prevStates, decisions[3], m_baseLevel, m_extRiceRRCFlag);
          break;
        case 4:
          m_currStates[0].updateState<4>(scanInfo, m_prevStates, decisions[0], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[1].updateState<4>(scanInfo, m_prevStates, decisions[1], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[2].updateState<4>(scanInfo, m_prevStates, decisions[2], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[3].updateState<4>(scanInfo, m_prevStates, decisions[3], m_baseLevel, m_extRiceRRCFlag);
          break;
        default:
          m_currStates[0].updateState<5>(scanInfo, m_prevStates, decisions[0], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[1].updateState<5>(scanInfo, m_prevStates, decisions[1], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[2].updateState<5>(scanInfo, m_prevStates, decisions[2], m_baseLevel, m_extRiceRRCFlag);
          m_currStates[3].updateState<5>(scanInfo, m_prevStates, decisions[3], m_baseLevel, m_extRiceRRCFlag);
        }
      }

      if( scanInfo.spt == SCAN_SOCSBB )
      {
        std::swap( m_prevStates, m_skipStates );
      }
    }
  }


  void DepQuant::quant( TransformUnit& tu, const CCoeffBuf& srcCoeff, const ComponentID compID, const QpParam& cQP, const double lambda, const Ctx& ctx, TCoeff& absSum, bool enableScalingLists, int* quantCoeff )
  {
    CHECKD( tu.cs->sps->getSpsRangeExtension().getExtendedPrecisionProcessingFlag(), "ext precision is not supported" );

    //===== reset / pre-init =====
    const TUParameters& tuPars  = *g_Rom.getTUPars( tu.blocks[compID], compID );
    m_quant.initQuantBlock    ( tu, compID, cQP, lambda );
    m_baseLevel = ctx.getBaseLevel();
    m_extRiceRRCFlag = tu.cs->sps->getSpsRangeExtension().getRrcRiceExtensionEnableFlag();
    TCoeff*       qCoeff      = tu.getCoeffs( compID ).buf;
    const TCoeff* tCoeff      = srcCoeff.buf;
    const int     numCoeff    = tu.blocks[compID].area();
    ::memset( tu.getCoeffs( compID ).buf, 0x00, numCoeff*sizeof(TCoeff) );
    absSum          = 0;

    const CompArea& area     = tu.blocks[ compID ];
    const uint32_t  width    = area.width;
    const uint32_t  height   = area.height;
    const uint32_t  lfnstIdx = tu.cu->lfnstIdx;
    //===== scaling matrix ====
    //const int         qpDQ = cQP.Qp + 1;
    //const int         qpPer = qpDQ / 6;
    //const int         qpRem = qpDQ - 6 * qpPer;

    //TCoeff thresTmp = thres;
    bool zeroOut = false;
    bool zeroOutforThres = false;
    int effWidth = tuPars.m_width, effHeight = tuPars.m_height;
    if ((tu.mtsIdx[compID] > MtsType::SKIP
         || (tu.cs->sps->getMtsEnabled() && tu.cu->sbtInfo != 0 && tuPars.m_height <= 32 && tuPars.m_width <= 32))
        && compID == COMPONENT_Y)
    {
      effHeight = (tuPars.m_height == 32) ? 16 : tuPars.m_height;
      effWidth = (tuPars.m_width == 32) ? 16 : tuPars.m_width;
      zeroOut = (effHeight < tuPars.m_height || effWidth < tuPars.m_width);
    }
    zeroOutforThres = zeroOut || (32 < tuPars.m_height || 32 < tuPars.m_width);
    //===== find first test position =====
    int firstTestPos = numCoeff - 1;
    if (lfnstIdx > 0 && tu.mtsIdx[compID] != MtsType::SKIP && width >= 4 && height >= 4)
    {
      firstTestPos = ( ( width == 4 && height == 4 ) || ( width == 8 && height == 8 ) )  ? 7 : 15 ;
    }
    const TCoeff defaultQuantisationCoefficient = (TCoeff)m_quant.getQScale();
    const TCoeff thres = m_quant.getLastThreshold();
    for( ; firstTestPos >= 0; firstTestPos-- )
    {
      if (zeroOutforThres && (tuPars.m_scanId2BlkPos[firstTestPos].x >= ((tuPars.m_width == 32 && zeroOut) ? 16 : 32)
                           || tuPars.m_scanId2BlkPos[firstTestPos].y >= ((tuPars.m_height == 32 && zeroOut) ? 16 : 32)))
      {
        continue;
      }
      TCoeff thresTmp = (enableScalingLists) ? TCoeff(thres / (4 * quantCoeff[tuPars.m_scanId2BlkPos[firstTestPos].idx]))
                                             : TCoeff(thres / (4 * defaultQuantisationCoefficient));

      if (abs(tCoeff[tuPars.m_scanId2BlkPos[firstTestPos].idx]) > thresTmp)
      {
        break;
      }
    }
    if( firstTestPos < 0 )
    {
      return;
    }

    //===== real init =====
    RateEstimator::initCtx( tuPars, tu, compID, ctx.getFracBitsAcess() );
    m_commonCtx.reset( tuPars, *this );
    for( int k = 0; k < 12; k++ )
    {
      m_allStates[k].init();
    }
    m_startState.init();


    int effectWidth = std::min(32, effWidth);
    int effectHeight = std::min(32, effHeight);
    for (int k = 0; k < 12; k++)
    {
      m_allStates[k].effWidth = effectWidth;
      m_allStates[k].effHeight = effectHeight;
    }
    m_startState.effWidth = effectWidth;
    m_startState.effHeight = effectHeight;

    //===== populate trellis =====
    for( int scanIdx = firstTestPos; scanIdx >= 0; scanIdx-- )
    {
      const ScanInfo& scanInfo = tuPars.m_scanInfo[ scanIdx ];
      if (enableScalingLists)
      {
        m_quant.initQuantBlock(tu, compID, cQP, lambda, quantCoeff[scanInfo.rasterPos]);
        xDecideAndUpdate( abs( tCoeff[scanInfo.rasterPos]), scanInfo, (zeroOut && (scanInfo.posX >= effWidth || scanInfo.posY >= effHeight)), quantCoeff[scanInfo.rasterPos], effectWidth, effectHeight, tu.cu->slice->getReverseLastSigCoeffFlag() );
      }
      else
      {
        xDecideAndUpdate( abs( tCoeff[scanInfo.rasterPos]), scanInfo, (zeroOut && (scanInfo.posX >= effWidth || scanInfo.posY >= effHeight)), defaultQuantisationCoefficient, effectWidth, effectHeight, tu.cu->slice->getReverseLastSigCoeffFlag() );
      }
    }

    //===== find best path =====
    Decision  decision    = { std::numeric_limits<int64_t>::max(), -1, -2 };
    int64_t   minPathCost =  0;
    for( int8_t stateId = 0; stateId < 4; stateId++ )
    {
      int64_t pathCost = m_trellis[0][stateId].rdCost;
      if( pathCost < minPathCost )
      {
        decision.prevId = stateId;
        minPathCost     = pathCost;
      }
    }

    //===== backward scanning =====
    int scanIdx = 0;
    for( ; decision.prevId >= 0; scanIdx++ )
    {
      decision          = m_trellis[ scanIdx ][ decision.prevId ];
      int32_t blkpos    = tuPars.m_scanId2BlkPos[scanIdx].idx;
      qCoeff[ blkpos ]  = ( tCoeff[ blkpos ] < 0 ? -decision.absLevel : decision.absLevel );
      absSum           += decision.absLevel;
    }
  }

}; // namespace DQIntern




//===== interface class =====
DepQuant::DepQuant( const Quant* other, bool enc ) : QuantRDOQ( other )
{
  const DepQuant* dq = dynamic_cast<const DepQuant*>( other );
  CHECK( other && !dq, "The DepQuant cast must be successfull!" );
  p = new DQIntern::DepQuant();
  if( enc )
  {
    DQIntern::g_Rom.init();
  }
}

DepQuant::~DepQuant()
{
  delete static_cast<DQIntern::DepQuant*>(p);
}

void DepQuant::quant(TransformUnit &tu, const ComponentID &compID, const CCoeffBuf &pSrc, TCoeff &absSum,
                     const QpParam &cQP, const Ctx &ctx)
{
  const bool useRegularResidualCoding =
    tu.cu->slice->getTSResidualCodingDisabledFlag() || tu.mtsIdx[compID] != MtsType::SKIP;
  if( tu.cs->slice->getDepQuantEnabledFlag() && useRegularResidualCoding )
  {
    //===== scaling matrix ====
    const int         qpDQ            = cQP.Qp(tu.mtsIdx[compID] == MtsType::SKIP) + 1;
    const int         qpPer           = qpDQ / 6;
    const int         qpRem           = qpDQ - 6 * qpPer;
    const CompArea   &rect            = tu.blocks[compID];
    const int         width           = rect.width;
    const int         height          = rect.height;
    uint32_t          scalingListType = getScalingListType(tu.cu->predMode, compID);
    CHECK(scalingListType >= SCALING_LIST_NUM, "Invalid scaling list");
    const uint32_t    log2TrWidth     = floorLog2(width);
    const uint32_t    log2TrHeight    = floorLog2(height);

    const bool        disableSMForLFNST = tu.cs->slice->getExplicitScalingListUsed() ? tu.cs->slice->getSPS()->getDisableScalingMatrixForLfnstBlks() : false;
    const bool        isLfnstApplied = tu.cu->lfnstIdx > 0 && (tu.cu->isSepTree() ? true : isLuma(compID));
    const bool        disableSMForACT = tu.cs->slice->getSPS()->getScalingMatrixForAlternativeColourSpaceDisabledFlag() && (tu.cs->slice->getSPS()->getScalingMatrixDesignatedColourSpaceFlag() == tu.cu->colorTransform);

    const bool enableScalingLists = getUseScalingList(width, height, tu.mtsIdx[compID] == MtsType::SKIP, isLfnstApplied,
                                                      disableSMForLFNST, disableSMForACT);

    static_cast<DQIntern::DepQuant *>(p)->quant(
      tu, pSrc, compID, cQP, Quant::m_dLambda, ctx, absSum, enableScalingLists,
      Quant::getQuantCoeff(scalingListType, qpRem, log2TrWidth, log2TrHeight));
  }
  else
  {
    QuantRDOQ::quant(tu, compID, pSrc, absSum, cQP, ctx);
  }
}

void DepQuant::dequant( const TransformUnit &tu, CoeffBuf &dstCoeff, const ComponentID &compID, const QpParam &cQP )
{
  const bool useRegularResidualCoding =
    tu.cu->slice->getTSResidualCodingDisabledFlag() || tu.mtsIdx[compID] != MtsType::SKIP;
  if( tu.cs->slice->getDepQuantEnabledFlag() && useRegularResidualCoding )
  {
    const int         qpDQ            = cQP.Qp(tu.mtsIdx[compID] == MtsType::SKIP) + 1;
    const int         qpPer           = qpDQ / 6;
    const int         qpRem           = qpDQ - 6 * qpPer;
    const CompArea   &rect            = tu.blocks[compID];
    const int         width           = rect.width;
    const int         height          = rect.height;
    uint32_t          scalingListType = getScalingListType(tu.cu->predMode, compID);
    CHECK(scalingListType >= SCALING_LIST_NUM, "Invalid scaling list");
    const uint32_t    log2TrWidth  = floorLog2(width);
    const uint32_t    log2TrHeight = floorLog2(height);

    const bool disableSMForLFNST = tu.cs->slice->getExplicitScalingListUsed() ? tu.cs->slice->getSPS()->getDisableScalingMatrixForLfnstBlks() : false;
    const bool isLfnstApplied = tu.cu->lfnstIdx > 0 && (tu.cu->isSepTree() ? true : isLuma(compID));
    const bool disableSMForACT = tu.cs->slice->getSPS()->getScalingMatrixForAlternativeColourSpaceDisabledFlag() && (tu.cs->slice->getSPS()->getScalingMatrixDesignatedColourSpaceFlag() == tu.cu->colorTransform);
    const bool enableScalingLists = getUseScalingList(width, height, tu.mtsIdx[compID] == MtsType::SKIP, isLfnstApplied,
                                                      disableSMForLFNST, disableSMForACT);
    static_cast<DQIntern::DepQuant*>(p)->dequant( tu, dstCoeff, compID, cQP, enableScalingLists, Quant::getDequantCoeff(scalingListType, qpRem, log2TrWidth, log2TrHeight) );
  }
  else
  {
    QuantRDOQ::dequant( tu, dstCoeff, compID, cQP );
  }
}



