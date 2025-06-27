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

/** \file     MCTS.cpp
    \brief    Motion Constrained Tile Sets class
*/

#include "MCTS.h"

#include "Buffer.h"
#include "UnitTools.h"

#include <memory.h>
#include <algorithm>

//! \ingroup CommonLib
//! \{

// ====================================================================================================================
// Constructor / destructor / initialize
// ====================================================================================================================

void MCTSInfo::init( CodingStructure* cs, int ctuAddr )
{
  // TODO: Tile area calculation can be a part of TileMap or some other high-level-part where tiles are managed

  m_tileArea = MCTSHelper::getTileArea( cs, ctuAddr );
}

Area MCTSInfo::getTileAreaSubPelRestricted( const PredictionUnit &pu )
{
  const int offLT = 3;
  const int offRB = 4;
  return MCTSHelper::getTileAreaRestricted( m_tileArea, offLT, offRB );
}

Area MCTSInfo::getTileAreaIntPelRestricted( const PredictionUnit &pu )
{
  const int offLT = 0;
  const int offRB = 0;
  return MCTSHelper::getTileAreaRestricted( m_tileArea, offLT, offRB );
}

Area MCTSHelper::getTileAreaRestricted( const Area& tileArea, const int offLT, const int offRB )
{
  return Area( tileArea.x + offLT, tileArea.y + offLT, tileArea.width - ( offLT + offRB ), tileArea.height - ( offLT + offRB ) );
}

void MCTSHelper::clipMvToArea( Mv& rcMv, const Area& block, const Area& clipArea, const SPS& sps, const int mvFracBits )
{
  const int horMax = (clipArea.x + clipArea.width - (int) block.x - (int) block.width) << mvFracBits;
  const int horMin = (clipArea.x - (int) block.x) << mvFracBits;

  const int verMax = (clipArea.y + clipArea.height - (int) block.y - (int) block.height) << mvFracBits;
  const int verMin = (clipArea.y - (int) block.y) << mvFracBits;

  rcMv.setHor(Clip3(horMin, horMax, rcMv.getHor()));
  rcMv.setVer(Clip3(verMin, verMax, rcMv.getVer()));
}

Area MCTSHelper::getTileArea( const CodingStructure* cs, const int ctuAddr )
{
  const PPS *pps = cs->pps;
  const int  maxCUWidth  = cs->pcv->maxCUWidth;
  const int  maxCUHeight = cs->pcv->maxCUHeight;

  const TileIdx tileIdx = pps->getTileIdx( (uint32_t)ctuAddr );
  const uint32_t tileX = tileIdx % pps->getNumTileColumns();
  const uint32_t tileY = tileIdx / pps->getNumTileColumns();

  const int tileWidthtInCtus = pps->getTileColumnWidth( tileX );
  const int tileHeightInCtus = pps->getTileRowHeight  ( tileY );
  const int tileXPosInCtus   = pps->getTileColumnBd( tileX );
  const int tileYPosInCtus   = pps->getTileRowBd( tileY );

  const int tileLeftTopPelPosX = maxCUWidth * tileXPosInCtus;
  const int tileLeftTopPelPosY = maxCUHeight * tileYPosInCtus;
  const int tileRightBottomPelPosX = std::min<int>( ( ( tileWidthtInCtus + tileXPosInCtus ) * maxCUWidth ), (int)cs->picture->lwidth() ) - 1;
  const int tileRightBottomPelPosY = std::min<int>( ( ( tileHeightInCtus + tileYPosInCtus ) * maxCUHeight ), (int)cs->picture->lheight() ) - 1;

  return Area( tileLeftTopPelPosX, tileLeftTopPelPosY, tileRightBottomPelPosX - tileLeftTopPelPosX + 1, tileRightBottomPelPosY - tileLeftTopPelPosY + 1 );
}

bool MCTSHelper::isRefBlockAtRestrictedTileBoundary( const PredictionUnit &pu )
{
  const Area& tileArea = pu.cs->picture->mctsInfo.getTileArea();
  const int mvPrecBits = MV_FRACTIONAL_BITS_INTERNAL;

  for( int refList = 0; refList < NUM_REF_PIC_LIST_01; refList++ )
  {
    if( pu.refIdx[refList] >= 0 )
    {
      const Mv& mv = pu.mv[refList];
      Area targetBlock( pu.Y().offset( mv.getHor() >> mvPrecBits, mv.getVer() >> mvPrecBits ), pu.Y().size() );
      // NOTE: at boundary take sub-pel filter samples into account
      if(
        targetBlock.x < tileArea.x + 3 ||
        targetBlock.y < tileArea.y + 3 ||
        targetBlock.bottomRight().x > tileArea.bottomRight().x - 4 ||
        targetBlock.bottomRight().y > tileArea.bottomRight().y - 4 )
      {
        return true;
      }
    }
  }
  return false;
}

static void getMotInfoBlockPartPos( const PredictionUnit& pu, int xOff, int yOff, const Mv& mv, int& ruiPredXLeft, int& ruiPredYTop, int& ruiPredXRight, int& ruiPredYBottom )
{
  const int mvFracBits = MV_FRACTIONAL_BITS_INTERNAL;

  ruiPredXLeft   = ( mv.getHor() >> mvFracBits ) + pu.lx() + xOff;
  ruiPredYTop    = ( mv.getVer() >> mvFracBits ) + pu.ly() + yOff;
  ruiPredXRight  = ruiPredXLeft + ( 1 << g_miScaling.posx ) - 1;
  ruiPredYBottom = ruiPredYTop + ( 1 << g_miScaling.posy ) - 1;
}

static bool checkMVRange( const Mv& mv, const Area& tileArea, int predXLeft, int predXRight, int predYTop, int predYBottom, ChromaFormat chromaFormat, bool isLuma, bool msgFlag )
{
  // Filter length of sub-sample generation filter to be considered
  int sampleOffsetLT, sampleOffsetRB, fracDiv;

  if( isLuma )
  {
    sampleOffsetLT = 3;
    sampleOffsetRB = 4;
    fracDiv = 1 << MV_FRACTIONAL_BITS_INTERNAL;
  }
  else
  {
    sampleOffsetLT = 2; //1; // 2 in luma coordinates!
    sampleOffsetRB = 3; //2; // 3 in luma coordinates!
    fracDiv = 1 << ( MV_FRACTIONAL_BITS_INTERNAL + 1 );
  }

  const int leftTopPelPosX = tileArea.topLeft().x;
  const int leftTopPelPosY = tileArea.topLeft().y;
  const int rightBottomPelPosX = tileArea.bottomRight().x;
  const int rightBottomPelPosY = tileArea.bottomRight().y;

  const bool isFullPelHor = ( mv.getHor() % fracDiv == 0 );
  const bool isFullPelVer = ( mv.getVer() % fracDiv == 0 );

  const int rangeXLeft   = leftTopPelPosX + ( isFullPelHor ? 0 : sampleOffsetLT );
  const int rangeYTop    = leftTopPelPosY + ( isFullPelVer ? 0 : sampleOffsetLT );
  const int rangeXRight  = rightBottomPelPosX - ( isFullPelHor ? 0 : sampleOffsetRB );
  const int rangeYBottom = rightBottomPelPosY - ( isFullPelVer ? 0 : sampleOffsetRB );

  if( !( predXLeft >= rangeXLeft && predXLeft <= rangeXRight ) || !( predXRight >= rangeXLeft && predXRight <= rangeXRight ) )
  {
    if( msgFlag )
    {
      msg( WARNING, "%s: pu motion vector across tile boundaries MV(%d,%d) RangeLR(%d,%d) PredLR(%d,%d)\n", (isLuma ? "LUMA": "CHROMA"), mv.getHor(), mv.getVer(), rangeXLeft, rangeXRight, predXLeft, predXRight );
    }
    return false;
  }
  else if( !( predYTop >= rangeYTop && predYTop <= rangeYBottom ) || !( predYBottom >= rangeYTop && predYBottom <= rangeYBottom ) )
  {
    if( msgFlag )
    {
      msg( WARNING, "%s: pu motion vector across tile boundaries MV(%d,%d) RangeTB(%d,%d) PredTB(%d,%d)\n", (isLuma ? "LUMA" : "CHROMA"), mv.getHor(), mv.getVer(), rangeYTop, rangeYBottom, predYTop, predYBottom );
    }
    return false;
  }

  return true;
}

bool MCTSHelper::checkMvBufferForMCTSConstraint( const PredictionUnit &pu, bool msgFlag )
{
  const Area& tileArea = pu.cs->picture->mctsInfo.getTileArea();
  const ChromaFormat chromaFormat = pu.cs->sps->getChromaFormatIdc();

  CMotionBuf mb = pu.getMotionBuf();

  const int blkW = 1 << ( g_miScaling.posx );
  const int blkH = 1 << ( g_miScaling.posy );

  for( int y = 0; y < mb.height; y++ )
  {
    for( int x = 0; x < mb.width; x++ )
    {
      const MotionInfo &mi = mb.at( x, y );
      int xOff = x << g_miScaling.posx;
      int yOff = y << g_miScaling.posy;

      for( int refList = 0; refList < NUM_REF_PIC_LIST_01; refList++ )
      {
        if( pu.refIdx[refList] >= 0 || mi.refIdx[refList] >= 0 )
        {
          const Mv& mv = mi.mv[refList];
          int predXLeft, predYTop, predXRight, predYBottom;
          getMotInfoBlockPartPos( pu, xOff, yOff, mv, predXLeft, predYTop, predXRight, predYBottom );
          if( !checkMVRange( mv, tileArea, predXLeft, predXRight, predYTop, predYBottom, chromaFormat, true, msgFlag ) )
          {
            return false;
          }

          // Chroma
          if( !pu.cu->affine )
          {
            if( !checkMVRange( mv, tileArea, predXLeft, predXRight, predYTop, predYBottom, chromaFormat, false, msgFlag ) )
            {
              return false;
            }
          }
          else
          {
            if( x % 2 == 0 && y % 2 == 0 )
            {
              const MotionInfo &miA = mi;
              const MotionInfo &miB = mb.at( x + 1, y + 1 );
              Mv mvAff = miA.mv[refList] + miB.mv[refList];
              mvAff >>= 1;
              getMotInfoBlockPartPos( pu, xOff, yOff, mvAff, predXLeft, predYTop, predXRight, predYBottom );
              if( !checkMVRange( mvAff, tileArea, predXLeft, predXRight + blkW, predYTop, predYBottom + blkH, chromaFormat, false, msgFlag ) )
              {
                return false;
              }
            }
          }
        }
      }
    }
  }

  return true;
}

bool MCTSHelper::checkMvIsNotInRestrictedArea( const PredictionUnit &pu, const Mv& mv, const Area& restrArea, const MvPrecision mvPrec )
{
  CHECKD(mvPrec < MvPrecision::ONE, "Wrong MV precision!");
  Mv testMv = mv;
  testMv >>= mvPrec - MvPrecision::ONE;
  Area targetArea = pu.Y();
  targetArea.repositionTo( targetArea.offset( testMv.getHor(), testMv.getVer() ) );
  if( !restrArea.contains( targetArea ) )
  {
    // Skip this pos
    return false;
  }
  return true;
}

bool MCTSHelper::checkMvForMCTSConstraint( const PredictionUnit &pu, const Mv& mv, const MvPrecision mvPrec )
{
  return checkMvIsNotInRestrictedArea( pu, mv, pu.cs->picture->mctsInfo.getTileAreaSubPelRestricted( pu ), mvPrec );
}


//! \}
