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

/** \file     CodingStructure.h
 *  \brief    A class managing the coding information for a specific image part
 */

#include "CodingStructure.h"

#include "Unit.h"
#include "Slice.h"
#include "Picture.h"
#include "UnitTools.h"
#include "UnitPartitioner.h"

XuPool g_xuPool = XuPool();

// ---------------------------------------------------------------------------
// coding structure method definitions
// ---------------------------------------------------------------------------

CodingStructure::CodingStructure(XuPool &xuPool)
  : area()
  , picture(nullptr)
  , parent(nullptr)
  , bestCS(nullptr)
  , m_isTuEnc(false)
  , m_cuPool(xuPool.cuPool)
  , m_puPool(xuPool.puPool)
  , m_tuPool(xuPool.tuPool)
  , bestParent(nullptr)
  , tmpColorSpaceCost(MAX_DOUBLE)
  , firstColorSpaceSelected(true)
  , resetIBCBuffer(false)
{
  for( uint32_t i = 0; i < MAX_NUM_COMPONENT; i++ )
  {
    m_offsets[ i ] = 0;
  }

  m_cuIdx.fill(nullptr);
  m_puIdx.fill(nullptr);
  m_tuIdx.fill(nullptr);
  m_isDecomp.fill(nullptr);

  m_motionBuf     = nullptr;
  picHeader = nullptr;

  features.resize( NUM_ENC_FEATURES );
  treeType = TREE_D;
  modeType = MODE_TYPE_ALL;
  tmpColorSpaceIntraCost[0] = MAX_DOUBLE;
  tmpColorSpaceIntraCost[1] = MAX_DOUBLE;
  firstColorSpaceTestOnly = false;
}

void CodingStructure::destroy()
{
  picture   = nullptr;
  parent    = nullptr;

  m_pred.destroy();
  m_resi.destroy();
  m_reco.destroy();
  m_orgr.destroy();

  destroyTemporaryCsData();

  delete[] m_motionBuf;
  m_motionBuf = nullptr;
}

void CodingStructure::destroyTemporaryCsData()
{
  destroyCoeffs();

  for (auto &ptr : m_isDecomp)
  {
    delete[] ptr;
    ptr = nullptr;
  }

  for (auto &ptr : m_tuIdx)
  {
    delete[] ptr;
    ptr = nullptr;
  }

  for (auto &ptr : m_puIdx)
  {
    delete[] ptr;
    ptr = nullptr;
  }

  for (auto &ptr : m_cuIdx)
  {
    delete[] ptr;
    ptr = nullptr;
  }

  m_tuPool.giveBack(tus);
  m_puPool.giveBack(pus);
  m_cuPool.giveBack(cus);
  m_numTUs = 0;
  m_numPUs = 0;
  m_numCUs = 0;
}

void CodingStructure::createTemporaryCsData(bool isPLTused)
{
  createCoeffs(isPLTused);

  for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(area.chromaFormat); chType++)
  {
    unsigned _area = unitScale[getFirstComponentOfChannel(chType)].scale(area.block(chType).size()).area();

    m_cuIdx[chType] = _area > 0 ? new unsigned[_area] : nullptr;
    m_puIdx[chType] = _area > 0 ? new unsigned[_area] : nullptr;
    m_tuIdx[chType] = _area > 0 ? new unsigned[_area] : nullptr;
    m_isDecomp[chType] = _area > 0 ? new bool[_area] : nullptr;
  }
}

void CodingStructure::releaseIntermediateData()
{
  clearTUs();
  clearPUs();
  clearCUs();
}

#if GDR_ENABLED
bool CodingStructure::containRefresh(const int begX, const int endX) const
{
  if (begX == endX)
  {
    return false;
  }

  const Area csArea      = area.Y();
  const Area refreshArea = Area(begX, area.ly(), endX - begX, std::min(slice->getPPS()->getPicHeightInLumaSamples(), area.lheight()));

  return csArea.contains(refreshArea);
}

bool CodingStructure::overlapRefresh(const int begX, const int endX) const
{
  if (begX == endX)
  {
    return false;
  }

  const Area csArea = area.Y();
  const Area refreshArea = Area(begX, area.ly(), endX - begX, area.lheight());

  return csArea.overlaps(refreshArea);
}

bool CodingStructure::overlapRefresh() const
{
  const int  csX     = area.lx();
  const int  csWidth = area.lwidth();

  return overlapRefresh(csX, csX + csWidth);
}

bool CodingStructure::withinRefresh(const int begX, const int endX) const
{
  if (begX == endX)
  {
    return false;
  }

  const Area csArea = area.Y();
  const Area refreshArea = Area(begX, area.ly(), endX - begX, area.lheight());

  return refreshArea.contains(csArea);
}

bool CodingStructure::refreshCrossTTV(const int begX, const int endX) const
{
  const int  csX = area.lx();
  const int  csY = area.ly();
  const int  csWidth  = area.lwidth();
  const int  csHeight = area.lheight();

  const Area refreshArea = Area(begX, csY, endX - begX, csHeight);

  const Area csArea0 = Area(csX,                                   csY, csWidth >> 2, csHeight);
  const Area csArea1 = Area(csX + (csWidth >> 2),                  csY, csWidth >> 1, csHeight);
  const Area csArea2 = Area(csX + (csWidth >> 2) + (csWidth >> 1), csY, csWidth >> 2, csHeight);

  const bool overlap0 = csArea0.overlaps(refreshArea);
  const bool overlap1 = csArea1.overlaps(refreshArea);
  const bool overlap2 = csArea2.overlaps(refreshArea);

  return overlap0 || overlap1 || overlap2;
}

bool CodingStructure::refreshCrossBTV(const int begX, const int endX) const
{
  const int  csX = area.lx();
  const int  csY = area.ly();
  const int  csWidth = area.lwidth();
  const int  csHeight = area.lheight();

  const Area refreshArea = Area(begX, csY, endX - begX, csHeight);

  const Area csArea0 = Area(csX,                  csY, (csWidth >> 1), csHeight);
  const Area csArea1 = Area(csX + (csWidth >> 1), csY, (csWidth >> 1), csHeight);

  const bool overlap0 = csArea0.overlaps(refreshArea);
  const bool overlap1 = csArea1.overlaps(refreshArea);

  return overlap0 || overlap1;
}

bool CodingStructure::overlapDirty() const
{
  const Position topLeft  = area.Y().topLeft();
  const Position topRight = area.Y().topRight();

  const bool insideLeft  = isClean(topLeft, ChannelType::LUMA);
  const bool insideRight = isClean(topRight, ChannelType::LUMA);

  return insideLeft != insideRight;
}

bool CodingStructure::dirtyCrossTTV() const
{
  const int  csX = area.lx();
  const int  csY = area.ly();
  const int  csWidth = area.lwidth();
  const int  csHeight = area.lheight();

  const Area csArea0 = Area(csX, csY, csWidth >> 2, csHeight);
  const Area csArea1 = Area(csX + (csWidth >> 2), csY, csWidth >> 1, csHeight);
  const Area csArea2 = Area(csX + (csWidth >> 2) + (csWidth >> 1), csY, csWidth >> 2, csHeight);

  const bool clean0 = isClean(csArea0, ChannelType::LUMA);
  const bool clean1 = isClean(csArea1, ChannelType::LUMA);
  const bool clean2 = isClean(csArea2, ChannelType::LUMA);

  const bool allclean = clean0 && clean1 && clean2;

  return !allclean;
}

bool CodingStructure::dirtyCrossBTV() const
{
  const int  csX = area.lx();
  const int  csY = area.ly();
  const int  csWidth = area.lwidth();
  const int  csHeight = area.lheight();

  const Area csArea0 = Area(csX,                  csY, (csWidth >> 1), csHeight);
  const Area csArea1 = Area(csX + (csWidth >> 1), csY, (csWidth >> 1), csHeight);

  const bool clean0 = isClean(csArea0, ChannelType::LUMA);
  const bool clean1 = isClean(csArea1, ChannelType::LUMA);

  const bool allclean = clean0 && clean1;

  return !allclean;
}

bool CodingStructure::isClean(const Position &intPos, const Mv &fracMv) const
{
  /*
    1. non gdr picture --> false;
    2. gdr picture
         pos in clean area -> true
         pos in dirty area -> false
  */
  const Picture* const curPic = slice->getPic();

  if (!curPic)
  {
    return false;
  }

  const bool isCurGdrPicture = curPic->gdrParam.inGdrInterval;

  if (isCurGdrPicture)
  {
    const int lumaPixelAway = 4;
    const int chromaPixelAway = 5;

    const int mvShift = MV_FRACTIONAL_BITS_INTERNAL;
    const int mvLumaFrac = (1 << mvShift);
    const int mvChromaFrac = (mvLumaFrac << 1);

    const bool isIntLumaMv = (fracMv.getHor() % mvLumaFrac) == 0;
    const bool isIntChromaMv = (fracMv.getHor() % mvChromaFrac) == 0;

    const int scaledEndX = curPic->gdrParam.verBoundary << mvShift;

    const Position origFracPos = Position(intPos.x << mvShift, intPos.y << mvShift);
    const int lastLumaPos = ((origFracPos.x / mvLumaFrac)   * mvLumaFrac) + fracMv.getHor() + (isIntLumaMv ? 0 : (lumaPixelAway << mvShift));
    const int lastChromaPos = ((origFracPos.x / mvChromaFrac) * mvChromaFrac) + fracMv.getHor() + (isIntChromaMv ? 0 : (chromaPixelAway << mvShift));

    const int lastPelPos = std::max(lastLumaPos, lastChromaPos);

    return (lastPelPos < scaledEndX);
  }

  return true;
}

bool CodingStructure::isClean(const Position &intPos, const Mv &fracMv, const Picture *const refPic) const
{
  /*
    1. non gdr picture --> false;
    2. gdr picture
         pos in clean area -> true
         pos in dirty area -> false
  */
  if (!refPic)
  {
    return false;
  }

  const bool isRefGdrPicture = refPic->gdrParam.inGdrInterval;

  if (isRefGdrPicture)
  {
    const int lumaPixelAway = 4;
    const int chromaPixelAway = 5;

    const int mvShift = MV_FRACTIONAL_BITS_INTERNAL;
    const int mvLumaFrac = (1 << mvShift);
    const int mvChromaFrac = (mvLumaFrac << 1);

    const bool isIntLumaMv = (fracMv.getHor() % mvLumaFrac) == 0;
    const bool isIntChromaMv = (fracMv.getHor() % mvChromaFrac) == 0;

    const int  scaledEndX = refPic->gdrParam.verBoundary << mvShift;

    const Position origFracPos = Position((intPos.x) << mvShift, intPos.y << mvShift);
    const int lastLumaPos = ((origFracPos.x / mvLumaFrac)   * mvLumaFrac) + fracMv.getHor() + (isIntLumaMv ? 0 : (lumaPixelAway << mvShift));
    const int lastChromaPos = ((origFracPos.x / mvChromaFrac) * mvChromaFrac) + fracMv.getHor() + (isIntChromaMv ? 0 : (chromaPixelAway << mvShift));

    const int lastPelPos = std::max(lastLumaPos, lastChromaPos);

    return (lastPelPos < scaledEndX);
  }
  else
  {
    // refPic is normal picture
    const bool isCurGdrPicture = slice->getPic()->gdrParam.inGdrInterval;

    return !isCurGdrPicture;
  }
}

bool CodingStructure::isClean(const Position &intPos, const Mv &fracMv, RefPicList e, int refIdx, bool isSubPu) const
{
  /*
    1. non gdr picture --> false;
    2. gdr picture
         pos in clean area -> true
         pos in dirty area -> false
  */
  if (refIdx < 0)
  {
    return false;
  }

  const Picture* refPic = (refIdx == IBC_REF_IDX) ? slice->getPic() : slice->getRefPic(e, refIdx);
  const bool isExceedNumRef = isSubPu && (refIdx >= slice->getNumRefIdx(e));

  if (!refPic || isExceedNumRef)
  {
    return false;
  }

  const bool isRefGdrPicture = refPic->gdrParam.inGdrInterval;

  if (isRefGdrPicture)
  {
    const int lumaPixelAway   = 4 + (isSubPu? 1 : 0);
    const int chromaPixelAway = 4 + (isSubPu? 2 : 0);

    const int mvShift      = MV_FRACTIONAL_BITS_INTERNAL;
    const int mvLumaFrac   = (1 << mvShift);
    const int mvChromaFrac = (mvLumaFrac << 1);

    const bool isIntLumaMv      = (fracMv.getHor() % mvLumaFrac  ) == 0;
    const bool isIntChromaMv    = isSubPu ? false : (fracMv.getHor() % mvChromaFrac) == 0;

    const int  scaledEndX       = refPic->gdrParam.verBoundary << mvShift;


    const Position origFracPos = Position((intPos.x) << mvShift, intPos.y << mvShift);
    const int lastLumaPos      = ((origFracPos.x / mvLumaFrac)   * mvLumaFrac)   + fracMv.getHor() + (isIntLumaMv   ? 0 : (lumaPixelAway   << mvShift));
    const int lastChromaPos    = ((origFracPos.x / mvChromaFrac) * mvChromaFrac) + fracMv.getHor() + (isIntChromaMv ? 0 : (chromaPixelAway << mvShift)) ;

    const int lastPelPos    = std::max(lastLumaPos, lastChromaPos);

    return (lastPelPos < scaledEndX);
  }
  else
  {
    // refPic is normal picture
    const bool isCurGdrPicture = slice->getPic()->gdrParam.inGdrInterval;

    return !isCurGdrPicture;
  }
}

bool CodingStructure::isClean(const Position &intPos, RefPicList e, int refIdx) const
{
  /*
    1. non gdr picture --> false;
    2. gdr picture
         pos in clean area -> true
         pos in dirty area -> false
  */
  const Picture* const refPic = slice->getRefPic(e, refIdx);

  if (!refPic || refIdx < 0)
  {
    return false;
  }

  const bool isRefGdrPicture = refPic->gdrParam.inGdrInterval;

  if (isRefGdrPicture)
  {
    return (intPos.x < refPic->gdrParam.verBoundary);
  }
  else
  {
    // refPic is normal picture
    const bool isCurGdrPicture = slice->getPic()->gdrParam.inGdrInterval;

    return !isCurGdrPicture;
  }
}

bool CodingStructure::isClean(const Position &intPos, const Picture* const refPic) const
{
  if (!refPic)
  {
    return false;
  }

  const bool isRefGdrPicture = refPic->gdrParam.inGdrInterval;

  if (isRefGdrPicture)
  {
    return (intPos.x < refPic->gdrParam.verBoundary);
  }
  else
  {
    // refPic is normal picture
    const bool isCurGdrPicture = slice->getPic()->gdrParam.inGdrInterval;

    return !isCurGdrPicture;
  }
}

bool CodingStructure::isClean(const int x, const int y, const ChannelType effChType) const
{
  /*
    1. non gdr picture --> false;
    2. gdr picture
         pos in clean area -> true
         pos in dirty area -> false
  */

  const bool isCurGdrPicture = picture->gdrParam.inGdrInterval;
  if (isCurGdrPicture)
  {
    const int verBoundaryForChannelType = picture->gdrParam.verBoundary >> to_underlying(effChType);
    return (x < verBoundaryForChannelType);
  }

  return true;
}

bool CodingStructure::isClean(const Position &intPos, const ChannelType effChType) const
{
  return isClean(intPos.x, intPos.y, effChType);
}

bool CodingStructure::isClean(const Area &area, const ChannelType effChType) const
{
  Position pTopLeft  = area.topLeft();
  Position pTopRight = area.topRight();
  Position pBotLeft  = area.bottomLeft();
  Position pBotRight = area.bottomRight();

  bool bTopLeft  = isClean(pTopLeft,  effChType);
  bool bTopRight = isClean(pTopRight, effChType);
  bool bBotLeft  = isClean(pBotLeft,  effChType);
  bool bBotRight = isClean(pBotRight, effChType);

  return bTopLeft && bTopRight && bBotLeft && bBotRight;
}

bool CodingStructure::isClean(const ChannelType effChType) const
{
  return isClean(area.Y(), effChType);
}

bool CodingStructure::isSubPuClean(const PredictionUnit &pu, const Mv *mv) const
{
  const CMotionBuf mb = pu.getMotionBuf();

  if (pu.cu->affine)
  {
    const Position puPos = pu.Y().pos();
    const Size subPuSize = Size(4, 4);

    const bool isSubPu = true;

    for (int y = 0; y < mb.height; y++)
    {
      for (int x = 0; x < mb.width; x++)
      {

        const MotionInfo mi = mb.at(x, y);
        const Position subPuPos  = Position{puPos.x + (x << 2), puPos.y + (y << 2)};
        const Area     subPuArea = Area(subPuPos, subPuSize);
        const Position subPuTR   = subPuArea.topRight();

        // check if SubPu with L0 is Out of boundary
        if (mi.refIdx[0] >= 0)
        {
          if (!isClean(subPuTR, mi.mv[0], REF_PIC_LIST_0, mi.refIdx[0], isSubPu))
          {
            return false;
          }
        }

        // check if SubPu wiht L1 is Out of boundary
        if (mi.refIdx[1] >= 0)
        {
          if (!isClean(subPuTR, mi.mv[1], REF_PIC_LIST_1, mi.refIdx[1], isSubPu))
          {
            return false;
          }
        }
      }
    }
  }

  return true;
}
#endif


bool CodingStructure::isDecomp( const Position &pos, const ChannelType effChType )
{
  const CompArea &_blk = area.block(effChType);

  if (_blk.contains(pos))
  {
    return m_isDecomp[effChType][rsAddr(pos, _blk, _blk.width, unitScale[getFirstComponentOfChannel(effChType)])];
  }
  else if( parent )
  {
    return parent->isDecomp( pos, effChType );
  }
  else
  {
    return false;
  }
}

bool CodingStructure::isDecomp( const Position &pos, const ChannelType effChType ) const
{
  const CompArea &_blk = area.block(effChType);

  if (_blk.contains(pos))
  {
    return m_isDecomp[effChType][rsAddr(pos, _blk, _blk.width, unitScale[getFirstComponentOfChannel(effChType)])];
  }
  else if( parent )
  {
    return parent->isDecomp( pos, effChType );
  }
  else
  {
    return false;
  }
}

void CodingStructure::setDecomp(const CompArea &_area, const bool _isCoded /*= true*/)
{
  const UnitScale& scale = unitScale[_area.compID];

  AreaBuf<bool> isCodedBlk(m_isDecomp[toChannelType(_area.compID)]
                             + rsAddr(_area, area.blocks[_area.compID].pos(), area.blocks[_area.compID].width, scale),
                           area.blocks[_area.compID].width >> scale.posx, _area.width >> scale.posx,
                           _area.height >> scale.posy);
  isCodedBlk.fill( _isCoded );
}

void CodingStructure::setDecomp(const UnitArea &_area, const bool _isCoded /*= true*/)
{
  for( uint32_t i = 0; i < _area.blocks.size(); i++ )
  {
    if (_area.blocks[i].valid())
    {
      setDecomp(_area.blocks[i], _isCoded);
    }
  }
}

const int CodingStructure::signalModeCons( const PartSplit split, Partitioner &partitioner, const ModeType modeTypeParent ) const
{
  if (CS::isDualITree(*this) || modeTypeParent != MODE_TYPE_ALL
      || partitioner.currArea().chromaFormat == ChromaFormat::_444
      || !isChromaEnabled(partitioner.currArea().chromaFormat))
  {
    return LDT_MODE_TYPE_INHERIT;
  }
  int minLumaArea = partitioner.currArea().lumaSize().area();
  if (split == CU_QUAD_SPLIT || split == CU_TRIH_SPLIT || split == CU_TRIV_SPLIT) // the area is split into 3 or 4 parts
  {
    minLumaArea = minLumaArea >> 2;
  }
  else if (split == CU_VERT_SPLIT || split == CU_HORZ_SPLIT) // the area is split into 2 parts
  {
    minLumaArea = minLumaArea >> 1;
  }
  int minChromaBlock =
    minLumaArea >> (getChannelTypeScaleX(ChannelType::CHROMA, partitioner.currArea().chromaFormat)
                    + getChannelTypeScaleY(ChannelType::CHROMA, partitioner.currArea().chromaFormat));
  bool is2xNChroma = (partitioner.currArea().chromaSize().width == 4 && split == CU_VERT_SPLIT) || (partitioner.currArea().chromaSize().width == 8 && split == CU_TRIV_SPLIT);
  return minChromaBlock >= 16 && !is2xNChroma ? LDT_MODE_TYPE_INHERIT : ((minLumaArea < 32) || slice->isIntra()) ? LDT_MODE_TYPE_INFER : LDT_MODE_TYPE_SIGNAL;
}

void CodingStructure::clearCuPuTuIdxMap( const UnitArea &_area, uint32_t numCu, uint32_t numPu, uint32_t numTu, uint32_t* pOffset )
{
  UnitArea clippedArea = clipArea( _area, *picture );

  for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(_area.chromaFormat); chType++)
  {
    const CompArea &_selfBlk = area.block(chType);
    const CompArea &_blk     = clippedArea.block(chType);

    const UnitScale& scale = unitScale[_blk.compID];
    const Area scaledSelf = scale.scale( _selfBlk );
    const Area scaledBlk = scale.scale( _blk );
    const size_t offset = rsAddr( scaledBlk.pos(), scaledSelf.pos(), scaledSelf.width );

    unsigned *idxPtrCU = m_cuIdx[chType] + offset;
    AreaBuf<uint32_t>( idxPtrCU, scaledSelf.width, scaledBlk.size() ).fill( 0 );

    unsigned *idxPtrPU = m_puIdx[chType] + offset;
    AreaBuf<uint32_t>( idxPtrPU, scaledSelf.width, scaledBlk.size() ).fill( 0 );

    unsigned *idxPtrTU = m_tuIdx[chType] + offset;
    AreaBuf<uint32_t>( idxPtrTU, scaledSelf.width, scaledBlk.size() ).fill( 0 );
  }

  //pop cu/pu/tus
  for( int i = m_numTUs; i > numTu; i-- )
  {
    m_tuPool.giveBack(tus.back());
    tus.pop_back();
    m_numTUs--;
  }
  for( int i = m_numPUs; i > numPu; i-- )
  {
    m_puPool.giveBack(pus.back());
    pus.pop_back();
    m_numPUs--;
  }
  for( int i = m_numCUs; i > numCu; i-- )
  {
    m_cuPool.giveBack(cus.back());
    cus.pop_back();
    m_numCUs--;
  }
  for (int i = 0; i < MAX_NUM_COMPONENT; i++)
  {
    m_offsets[i] = pOffset[i];
  }
}

CodingUnit* CodingStructure::getLumaCU( const Position &pos )
{
  const CompArea &_blk = area.block(ChannelType::LUMA);
  CHECK( !_blk.contains( pos ), "must contain the pos" );

  const unsigned idx = m_cuIdx[ChannelType::LUMA][rsAddr(pos, _blk.pos(), _blk.width, unitScale[COMPONENT_Y])];

  if (idx != 0)
  {
    return cus[idx - 1];
  }
  else
  {
    return nullptr;
  }
}

CodingUnit* CodingStructure::getCU( const Position &pos, const ChannelType effChType )
{
  const CompArea &_blk = area.block(effChType);

  if (!_blk.contains(pos) || (treeType == TREE_C && isLuma(effChType)))
  {
    //keep this check, which is helpful to identify bugs
    if (treeType == TREE_C && isLuma(effChType))
    {
      CHECK( parent == nullptr, "parent shall be valid; consider using function getLumaCU()" );
      CHECK( parent->treeType != TREE_D, "wrong parent treeType " );
    }
    if (parent)
    {
      return parent->getCU(pos, effChType);
    }
    else
    {
      return nullptr;
    }
  }
  else
  {
    const unsigned idx =
      m_cuIdx[effChType][rsAddr(pos, _blk.pos(), _blk.width, unitScale[getFirstComponentOfChannel(effChType)])];

    if (idx != 0)
    {
      return cus[idx - 1];
    }
    else
    {
      return nullptr;
    }
  }
}

const CodingUnit* CodingStructure::getCU( const Position &pos, const ChannelType effChType ) const
{
  const CompArea &_blk = area.block(effChType);

  if (!_blk.contains(pos) || (treeType == TREE_C && isLuma(effChType)))
  {
    if (treeType == TREE_C && isLuma(effChType))
    {
      CHECK( parent == nullptr, "parent shall be valid; consider using function getLumaCU()" );
      CHECK( parent->treeType != TREE_D, "wrong parent treeType" );
    }
    if (parent)
    {
      return parent->getCU(pos, effChType);
    }
    else
    {
      return nullptr;
    }
  }
  else
  {
    const unsigned idx =
      m_cuIdx[effChType][rsAddr(pos, _blk.pos(), _blk.width, unitScale[getFirstComponentOfChannel(effChType)])];

    if (idx != 0)
    {
      return cus[idx - 1];
    }
    else
    {
      return nullptr;
    }
  }
}

PredictionUnit* CodingStructure::getPU( const Position &pos, const ChannelType effChType )
{
  const CompArea &_blk = area.block(effChType);

  if( !_blk.contains( pos ) )
  {
    if (parent)
    {
      return parent->getPU(pos, effChType);
    }
    else
    {
      return nullptr;
    }
  }
  else
  {
    const unsigned idx =
      m_puIdx[effChType][rsAddr(pos, _blk.pos(), _blk.width, unitScale[getFirstComponentOfChannel(effChType)])];

    if (idx != 0)
    {
      return pus[idx - 1];
    }
    else
    {
      return nullptr;
    }
  }
}

const PredictionUnit * CodingStructure::getPU( const Position &pos, const ChannelType effChType ) const
{
  const CompArea &_blk = area.block(effChType);

  if( !_blk.contains( pos ) )
  {
    if (parent)
    {
      return parent->getPU(pos, effChType);
    }
    else
    {
      return nullptr;
    }
  }
  else
  {
    const unsigned idx =
      m_puIdx[effChType][rsAddr(pos, _blk.pos(), _blk.width, unitScale[getFirstComponentOfChannel(effChType)])];

    if (idx != 0)
    {
      return pus[idx - 1];
    }
    else
    {
      return nullptr;
    }
  }
}

TransformUnit* CodingStructure::getTU( const Position &pos, const ChannelType effChType, const int subTuIdx )
{
  const CompArea &_blk = area.block(effChType);

  if( !_blk.contains( pos ) )
  {
    if (parent)
    {
      return parent->getTU(pos, effChType);
    }
    else
    {
      return nullptr;
    }
  }
  else
  {
    const unsigned idx =
      m_tuIdx[effChType][rsAddr(pos, _blk.pos(), _blk.width, unitScale[getFirstComponentOfChannel(effChType)])];

    if( idx != 0 )
    {
      unsigned extraIdx = 0;
      if( isLuma( effChType ) )
      {
        const TransformUnit& tu = *tus[idx - 1];

        if (tu.cu->ispMode != ISPType::NONE)   // Intra SubPartitions mode
        {
          //we obtain the offset to index the corresponding sub-partition
          if( subTuIdx != -1 )
          {
            extraIdx = subTuIdx;
          }
          else
          {
            while( !tus[idx - 1 + extraIdx]->blocks[getFirstComponentOfChannel( effChType )].contains( pos ) )
            {
              extraIdx++;
              CHECK( tus[idx - 1 + extraIdx]->cu->treeType == TREE_C, "tu searched by position points to a chroma tree CU" );
              CHECK( extraIdx > 3, "extraIdx > 3" );
            }
          }
        }
      }
      return tus[idx - 1 + extraIdx];
    }
    else if (m_isTuEnc)
    {
      return parent->getTU(pos, effChType);
    }
    else
    {
      return nullptr;
    }
  }
}

const TransformUnit * CodingStructure::getTU( const Position &pos, const ChannelType effChType, const int subTuIdx ) const
{
  const CompArea &_blk = area.block(effChType);

  if( !_blk.contains( pos ) )
  {
    if (parent)
    {
      return parent->getTU(pos, effChType);
    }
    else
    {
      return nullptr;
    }
  }
  else
  {
    const unsigned idx =
      m_tuIdx[effChType][rsAddr(pos, _blk.pos(), _blk.width, unitScale[getFirstComponentOfChannel(effChType)])];
    if( idx != 0 )
    {
      unsigned extraIdx = 0;
      if( isLuma( effChType ) )
      {
        const TransformUnit& tu = *tus[idx - 1];
        if (tu.cu->ispMode != ISPType::NONE)   // Intra SubPartitions mode
        {
          //we obtain the offset to index the corresponding sub-partition
          if( subTuIdx != -1 )
          {
            extraIdx = subTuIdx;
          }
          else
          {
            while ( !tus[idx - 1 + extraIdx]->blocks[getFirstComponentOfChannel( effChType )].contains(pos) )
            {
              extraIdx++;
              CHECK( tus[idx - 1 + extraIdx]->cu->treeType == TREE_C, "tu searched by position points to a chroma tree CU" );
              CHECK( extraIdx > 3, "extraIdx > 3" );
            }
          }
        }
      }
      return tus[idx - 1 + extraIdx];
    }
    else if (m_isTuEnc)
    {
      return parent->getTU(pos, effChType);
    }
    else
    {
      return nullptr;
    }
  }
}

CodingUnit& CodingStructure::addCU( const UnitArea &unit, const ChannelType chType )
{
  CodingUnit *cu = m_cuPool.get();

  cu->UnitArea::operator=( unit );
  cu->cs        = this;
  cu->slice     = nullptr;
  cu->next      = nullptr;
  cu->firstPU   = nullptr;
  cu->lastPU    = nullptr;
  cu->firstTU   = nullptr;
  cu->lastTU    = nullptr;
  cu->initData();
  cu->chType    = chType;
  cu->treeType = treeType;
  cu->modeType = modeType;

  CodingUnit *prevCU = m_numCUs > 0 ? cus.back() : nullptr;

  if( prevCU )
  {
    prevCU->next = cu;
  }

  cus.push_back( cu );

  uint32_t idx = ++m_numCUs;
  cu->idx  = idx;

  for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(area.chromaFormat); chType++)
  {
    if (!cu->block(chType).valid())
    {
      continue;
    }

    const CompArea &_selfBlk = area.block(chType);
    const CompArea &_blk     = cu->block(chType);

    const UnitScale& scale = unitScale[_blk.compID];
    const Area scaledSelf  = scale.scale( _selfBlk );
    const Area scaledBlk   = scale.scale(     _blk );
    unsigned        *idxPtr      = m_cuIdx[chType] + rsAddr(scaledBlk.pos(), scaledSelf.pos(), scaledSelf.width);
    CHECK( *idxPtr, "Overwriting a pre-existing value, should be '0'!" );
    AreaBuf<uint32_t>( idxPtr, scaledSelf.width, scaledBlk.size() ).fill( idx );
  }

  return *cu;
}

PredictionUnit& CodingStructure::addPU( const UnitArea &unit, const ChannelType chType )
{
  PredictionUnit *pu = m_puPool.get();

  pu->UnitArea::operator=( unit );
  pu->initData();
  pu->next   = nullptr;
  pu->cs     = this;
  pu->cu     = m_isTuEnc ? cus[0] : getCU(unit.block(chType).pos(), chType);
  pu->chType = chType;

  PredictionUnit *prevPU = m_numPUs > 0 ? pus.back() : nullptr;

  if( prevPU && prevPU->cu == pu->cu )
  {
    prevPU->next = pu;
  }

  pus.push_back( pu );

  if( pu->cu->firstPU == nullptr )
  {
    pu->cu->firstPU = pu;
  }
  pu->cu->lastPU = pu;

  uint32_t idx = ++m_numPUs;
  pu->idx  = idx;

  for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(area.chromaFormat); chType++)
  {
    if (!pu->block(chType).valid())
    {
      continue;
    }

    const CompArea &_selfBlk = area.block(chType);
    const CompArea &_blk     = pu->block(chType);

    const UnitScale& scale = unitScale[_blk.compID];
    const Area scaledSelf  = scale.scale( _selfBlk );
    const Area scaledBlk   = scale.scale(     _blk );
    unsigned        *idxPtr      = m_puIdx[chType] + rsAddr(scaledBlk.pos(), scaledSelf.pos(), scaledSelf.width);
    CHECK( *idxPtr, "Overwriting a pre-existing value, should be '0'!" );
    AreaBuf<uint32_t>( idxPtr, scaledSelf.width, scaledBlk.size() ).fill( idx );
  }

  return *pu;
}

TransformUnit& CodingStructure::addTU( const UnitArea &unit, const ChannelType chType )
{
  TransformUnit *tu = m_tuPool.get();

  tu->UnitArea::operator=( unit );
  tu->initData();
  tu->next   = nullptr;
  tu->prev   = nullptr;
  tu->cs     = this;
  tu->cu     = m_isTuEnc ? cus[0] : getCU(unit.block(chType).pos(), chType);
  tu->chType = chType;

  TransformUnit *prevTU = m_numTUs > 0 ? tus.back() : nullptr;

  if( prevTU && prevTU->cu == tu->cu )
  {
    prevTU->next = tu;
    tu->prev     = prevTU;
  }

  tus.push_back( tu );

  if( tu->cu )
  {
    if( tu->cu->firstTU == nullptr )
    {
      tu->cu->firstTU = tu;
    }
    tu->cu->lastTU = tu;
  }

  uint32_t idx = ++m_numTUs;
  tu->idx  = idx;

  TCoeff *coeffs[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
  Pel    *pltIdxBuf[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
  EnumArray<PLTRunMode*, ChannelType> runType;

  for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(area.chromaFormat); chType++)
  {
    if (!tu->block(chType).valid())
    {
      continue;
    }

    const CompArea &_selfBlk = area.block(chType);
    const CompArea &_blk     = tu->block(chType);

    bool isIspTu = tu->cu != nullptr && tu->cu->ispMode != ISPType::NONE && isLuma(_blk.compID);

    bool isFirstIspTu = false;
    if (isIspTu)
    {
      isFirstIspTu = CU::isISPFirst(*tu->cu, _blk, getFirstComponentOfChannel(chType));
    }
    if (!isIspTu || isFirstIspTu)
    {
      const UnitScale &scale = unitScale[_blk.compID];

      const Area scaledSelf = scale.scale(_selfBlk);
      const Area scaledBlk  = isIspTu ? scale.scale(tu->cu->block(chType)) : scale.scale(_blk);
      unsigned  *idxPtr     = m_tuIdx[chType] + rsAddr(scaledBlk.pos(), scaledSelf.pos(), scaledSelf.width);
      CHECK(*idxPtr, "Overwriting a pre-existing value, should be '0'!");
      AreaBuf<uint32_t>(idxPtr, scaledSelf.width, scaledBlk.size()).fill(idx);
    }

    if (!m_runType[chType].empty())
    {
      runType[chType] = m_runType[chType].data() + m_offsets[getFirstComponentOfChannel(chType)];
    }
  }

  const bool usePlt = sps->getPLTMode();
  uint32_t numComp = ::getNumberValidComponents(area.chromaFormat);

  for (uint32_t i = 0; i < numComp; i++)
  {
    if (!tu->blocks[i].valid())
    {
      continue;
    }

    coeffs[i] = m_coeffs[i].data() + m_offsets[i];
    if (usePlt)
    {
      pltIdxBuf[i] = m_pltIdxBuf[i].data() + m_offsets[i];
    }

    unsigned areaSize = tu->blocks[i].area();
    m_offsets[i] += areaSize;
  }
  tu->init(coeffs, pltIdxBuf, runType);

  return *tu;
}

void CodingStructure::addEmptyTUs( Partitioner &partitioner )
{
  const UnitArea& area    = partitioner.currArea();
  bool            split   = partitioner.canSplit(TU_MAX_TR_SPLIT, *this);
  const unsigned  trDepth = partitioner.currTrDepth;

  if( split )
  {
    partitioner.splitCurrArea( TU_MAX_TR_SPLIT, *this );
    do
    {
      addEmptyTUs( partitioner );
    } while( partitioner.nextPart( *this ) );

    partitioner.exitCurrSplit();
  }
  else
  {
    TransformUnit &tu = this->addTU( CS::getArea( *this, area, partitioner.chType ), partitioner.chType );
    unsigned numBlocks = ::getNumberValidTBlocks( *this->pcv );
    const bool usePlt = sps->getPLTMode();
    for( unsigned compID = COMPONENT_Y; compID < numBlocks; compID++ )
    {
      if( tu.blocks[compID].valid() )
      {
        tu.getCoeffs( ComponentID( compID ) ).fill( 0 );
        if (usePlt)
        {
          tu.getcurPLTIdx( ComponentID( compID ) ).fill( 0 );
        }
      }
    }
    tu.depth = trDepth;
  }
}

CUTraverser CodingStructure::traverseCUs( const UnitArea& unit, const ChannelType effChType )
{
  CodingUnit* firstCU = getCU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  CodingUnit* lastCU = firstCU;
  if( !CS::isDualITree( *this ) ) //for a more generalized separate tree
  {
    bool bContinue = true;
    CodingUnit* currCU = firstCU;
    while( bContinue )
    {
      if( currCU == nullptr )
      {
        bContinue = false;
        lastCU = currCU;
      }
      else if( currCU->chType != effChType )
      {
        lastCU = currCU;
        currCU = currCU->next;
      }
      else
      {
        if( unit.contains( *currCU ) )
        {
          lastCU = currCU;
          currCU = currCU->next;
        }
        else
        {
          bContinue = false;
          lastCU = currCU;
        }
      }
    }
  }
  else
  {
    do
    {
    } while (lastCU && (lastCU = lastCU->next) && unit.contains(*lastCU));
  }

  return CUTraverser( firstCU, lastCU );
}

PUTraverser CodingStructure::traversePUs( const UnitArea& unit, const ChannelType effChType )
{
  PredictionUnit* firstPU = getPU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  PredictionUnit* lastPU  = firstPU;

  do { } while( lastPU && ( lastPU = lastPU->next ) && unit.contains( *lastPU ) );

  return PUTraverser( firstPU, lastPU );
}

TUTraverser CodingStructure::traverseTUs( const UnitArea& unit, const ChannelType effChType )
{
  TransformUnit* firstTU = getTU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  TransformUnit* lastTU  = firstTU;

  do { } while( lastTU && ( lastTU = lastTU->next ) && unit.contains( *lastTU ) );

  return TUTraverser( firstTU, lastTU );
}

cCUTraverser CodingStructure::traverseCUs( const UnitArea& unit, const ChannelType effChType ) const
{
  const CodingUnit* firstCU = getCU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  const CodingUnit* lastCU  = firstCU;

  do { } while( lastCU && ( lastCU = lastCU->next ) && unit.contains( *lastCU ) );

  return cCUTraverser( firstCU, lastCU );
}

cPUTraverser CodingStructure::traversePUs( const UnitArea& unit, const ChannelType effChType ) const
{
  const PredictionUnit* firstPU = getPU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  const PredictionUnit* lastPU  = firstPU;

  do { } while( lastPU && ( lastPU = lastPU->next ) && unit.contains( *lastPU ) );

  return cPUTraverser( firstPU, lastPU );
}

cTUTraverser CodingStructure::traverseTUs( const UnitArea& unit, const ChannelType effChType ) const
{
  const TransformUnit* firstTU = getTU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  const TransformUnit* lastTU  = firstTU;

  do { } while( lastTU && ( lastTU = lastTU->next ) && unit.contains( *lastTU ) );

  return cTUTraverser( firstTU, lastTU );
}

// coding utilities

void CodingStructure::allocateVectorsAtPicLevel()
{
  const int  twice     = !pcv->ISingleTree && slice->isIRAP() && isChromaEnabled(pcv->chrFormat) ? 2 : 1;
  size_t     allocSize = twice * unitScale[COMPONENT_Y].scale(area.blocks[COMPONENT_Y].size()).area();

  cus.reserve( allocSize );
  pus.reserve( allocSize );
  tus.reserve( allocSize );
}

void CodingStructure::create(const ChromaFormat &_chromaFormat, const Area& _area, const bool isTopLayer, const bool isPLTused)
{
  createInternals(UnitArea(_chromaFormat, _area), isTopLayer, isPLTused);

  if (isTopLayer)
  {
    return;
  }

  m_reco.create( area );
  m_pred.create( area );
  m_resi.create( area );
  m_orgr.create( area );
}

void CodingStructure::create(const UnitArea& _unit, const bool isTopLayer, const bool isPLTused)
{
  createInternals(_unit, isTopLayer, isPLTused);

  if (isTopLayer)
  {
    return;
  }

  m_reco.create( area );
  m_pred.create( area );
  m_resi.create( area );
  m_orgr.create( area );
}

void CodingStructure::createInternals(const UnitArea& _unit, const bool isTopLayer, const bool isPLTused)
{
  area = _unit;

  for (int i = 0; i < unitScale.size(); i++)
  {
    const auto c       = ComponentID(i);
    const bool present = isLuma(c) || isChromaEnabled(area.chromaFormat);
    const int  scaleX  = present ? 2 >> getComponentScaleX(c, area.chromaFormat) : 0;
    const int  scaleY  = present ? 2 >> getComponentScaleY(c, area.chromaFormat) : 0;
    unitScale[i]       = UnitScale(scaleX, scaleY);
  }

  picture = nullptr;
  parent  = nullptr;

  const int numComp = getNumberValidComponents(area.chromaFormat);

  for (int i = 0; i < numComp; i++)
  {
    m_offsets[i] = 0;
  }

  if (!isTopLayer)
  {
    createTemporaryCsData(isPLTused);
  }

  unsigned _lumaAreaScaled = g_miScaling.scale( area.lumaSize() ).area();
  m_motionBuf       = new MotionInfo[_lumaAreaScaled];
  if (!isTopLayer)
  {
    initStructData();
  }
}

void CodingStructure::addMiToLut(static_vector<MotionInfo, MAX_NUM_HMVP_CANDS> &lut, const MotionInfo &mi)
{
  size_t currCnt = lut.size();

  bool pruned      = false;
  int  sameCandIdx = 0;

  for (int idx = 0; idx < currCnt; idx++)
  {
    if (lut[idx] == mi)
    {
      sameCandIdx = idx;
      pruned      = true;
      break;
    }
  }

  if (pruned || currCnt == lut.capacity())
  {
    lut.erase(lut.begin() + sameCandIdx);
  }

  lut.push_back(mi);
}

void CodingStructure::resetPrevPLT(PLTBuf& prevPLT)
{
  for (int ch = 0; ch < MAX_NUM_CHANNEL_TYPE; ch++)
  {
    prevPLT.curPLTSize[ch] = 0;
  }

  for (int comp = 0; comp < MAX_NUM_COMPONENT; comp++)
  {
    memset(prevPLT.curPLT[comp], 0, MAXPLTPREDSIZE * sizeof(Pel));
  }
}

void CodingStructure::reorderPrevPLT(PLTBuf& prevPLT, uint8_t curPLTSize[MAX_NUM_CHANNEL_TYPE], Pel curPLT[MAX_NUM_COMPONENT][MAXPLTSIZE], bool reuseflag[MAX_NUM_CHANNEL_TYPE][MAXPLTPREDSIZE], uint32_t compBegin, uint32_t numComp, bool jointPLT)
{
  Pel stuffedPLT[MAX_NUM_COMPONENT][MAXPLTPREDSIZE];
  uint8_t tempCurPLTsize[MAX_NUM_CHANNEL_TYPE];
  uint8_t stuffPLTsize[MAX_NUM_COMPONENT];

  uint32_t maxPredPltSize = jointPLT ? MAXPLTPREDSIZE : MAXPLTPREDSIZE_DUALTREE;

  for (int i = compBegin; i < (compBegin + numComp); i++)
  {
    ComponentID comID = jointPLT ? (ComponentID)compBegin : ((i > 0) ? COMPONENT_Cb : COMPONENT_Y);
    tempCurPLTsize[comID] = curPLTSize[comID];
    stuffPLTsize[i] = 0;
    memcpy(stuffedPLT[i], curPLT[i], curPLTSize[comID] * sizeof(Pel));
  }

  for (int ch = compBegin; ch < (compBegin + numComp); ch++)
  {
    ComponentID comID = jointPLT ? (ComponentID)compBegin : ((ch > 0) ? COMPONENT_Cb : COMPONENT_Y);
    if (ch > 1) break;
    for (int i = 0; i < prevPLT.curPLTSize[comID]; i++)
    {
      if (tempCurPLTsize[comID] + stuffPLTsize[ch] >= maxPredPltSize)
      {
        break;
      }

      if (!reuseflag[comID][i])
      {
        if (ch == COMPONENT_Y)
        {
          stuffedPLT[0][tempCurPLTsize[comID] + stuffPLTsize[ch]] = prevPLT.curPLT[0][i];
        }
        else
        {
          stuffedPLT[1][tempCurPLTsize[comID] + stuffPLTsize[ch]] = prevPLT.curPLT[1][i];
          stuffedPLT[2][tempCurPLTsize[comID] + stuffPLTsize[ch]] = prevPLT.curPLT[2][i];
        }
        stuffPLTsize[ch]++;
      }
    }
  }

  for (int i = compBegin; i < (compBegin + numComp); i++)
  {
    ComponentID comID = jointPLT ? (ComponentID)compBegin : ((i > 0) ? COMPONENT_Cb : COMPONENT_Y);
    prevPLT.curPLTSize[comID] = curPLTSize[comID] + stuffPLTsize[comID];
    memcpy(prevPLT.curPLT[i], stuffedPLT[i], prevPLT.curPLTSize[comID] * sizeof(Pel));
    CHECK(prevPLT.curPLTSize[comID] > maxPredPltSize, " Maximum palette predictor size exceed limit");
  }
}

void CodingStructure::setPrevPLT(PLTBuf predictor)
{
  for (int comp = 0; comp < MAX_NUM_CHANNEL_TYPE; comp++)
  {
    prevPLT.curPLTSize[comp] = predictor.curPLTSize[comp];
  }
  for (int comp = 0; comp < MAX_NUM_COMPONENT; comp++)
  {
    memcpy(prevPLT.curPLT[comp], predictor.curPLT[comp], MAXPLTPREDSIZE * sizeof(Pel));
  }
}

void CodingStructure::storePrevPLT(PLTBuf& predictor)
{
  for (int comp = 0; comp < MAX_NUM_CHANNEL_TYPE; comp++)
  {
    predictor.curPLTSize[comp] = prevPLT.curPLTSize[comp];
  }
  for (int comp = 0; comp < MAX_NUM_COMPONENT; comp++)
  {
    memcpy(predictor.curPLT[comp], prevPLT.curPLT[comp], MAXPLTPREDSIZE * sizeof(Pel));
  }
}

void CodingStructure::rebindPicBufs()
{
  CHECK( parent, "rebindPicBufs can only be used for the top level CodingStructure" );

  if (!picture->M_BUFS(0, PIC_RECONSTRUCTION).bufs.empty())
  {
    m_reco.createFromBuf(picture->M_BUFS(0, PIC_RECONSTRUCTION));
  }
  else
  {
    m_reco.destroy();
  }
  if (!picture->M_BUFS(0, PIC_PREDICTION).bufs.empty())
  {
    m_pred.createFromBuf(picture->M_BUFS(0, PIC_PREDICTION));
  }
  else
  {
    m_pred.destroy();
  }
  if (!picture->M_BUFS(0, PIC_RESIDUAL).bufs.empty())
  {
    m_resi.createFromBuf(picture->M_BUFS(0, PIC_RESIDUAL));
  }
  else
  {
    m_resi.destroy();
  }
  if( pcv->isEncoder )
  {
    if (!picture->M_BUFS(0, PIC_RESIDUAL).bufs.empty())
    {
      m_orgr.create(area.chromaFormat, area.blocks[0], pcv->maxCUWidth);
    }
    else
    {
      m_orgr.destroy();
    }
  }
}

void CodingStructure::createCoeffs(const bool isPLTused)
{
  const unsigned numCh = getNumberValidComponents( area.chromaFormat );

  for( unsigned i = 0; i < numCh; i++ )
  {
    unsigned _area = area.blocks[i].area();

    m_coeffs[i].resize(_area);
    if (isPLTused)
    {
      m_pltIdxBuf[i].resize(_area);
    }
  }

  if (isPLTused)
  {
    for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(area.chromaFormat); chType++)
    {
      unsigned _area = area.block(chType).area();

      m_runType[chType].resize(_area);
    }
  }
}

void CodingStructure::destroyCoeffs()
{
  for( uint32_t i = 0; i < MAX_NUM_COMPONENT; i++ )
  {
    free(m_coeffs[i]);
    free(m_pltIdxBuf[i]);
  }

  for (auto &ptr: m_runType)
  {
    free(ptr);
  }
}

void CodingStructure::initSubStructure( CodingStructure& subStruct, const ChannelType _chType, const UnitArea &subArea, const bool &isTuEnc )
{
  CHECK( this == &subStruct, "Trying to init self as sub-structure" );

  subStruct.useDbCost = false;
  subStruct.costDbOffset = 0;
  subStruct.useInterlayerRef = false;

  for( uint32_t i = 0; i < subStruct.area.blocks.size(); i++ )
  {
    CHECKD( subStruct.area.blocks[i].size() != subArea.blocks[i].size(), "Trying to init sub-structure of incompatible size" );

    subStruct.area.blocks[i].pos() = subArea.blocks[i].pos();
  }

  if( parent )
  {
    // allow this to be false at the top level (need for edge CTU's)
    CHECKD( !area.contains( subStruct.area ), "Trying to init sub-structure not contained in the parent" );
  }

  subStruct.parent    = this;
  subStruct.picture   = picture;

  subStruct.sps       = sps;
  subStruct.vps       = vps;
  subStruct.pps       = pps;
  subStruct.picHeader = picHeader;

  memcpy(subStruct.alfApss, alfApss, sizeof(alfApss));

  subStruct.lmcsAps = lmcsAps;
  subStruct.scalinglistAps = scalinglistAps;

  subStruct.slice     = slice;
  subStruct.baseQP    = baseQP;
  subStruct.prevQP[_chType] = prevQP[_chType];
  subStruct.pcv       = pcv;

  subStruct.m_isTuEnc = isTuEnc;

  subStruct.motionLut = motionLut;

  subStruct.prevPLT = prevPLT;

  subStruct.treeType  = treeType;
  subStruct.modeType  = modeType;

  subStruct.initStructData(currQP[_chType]);

  if( isTuEnc )
  {
    CHECKD( area != subStruct.area, "Trying to init sub-structure for TU-encoding of incompatible size" );

    for( const auto &pcu : cus )
    {
      CodingUnit &cu = subStruct.addCU( *pcu, _chType );

      cu = *pcu;
    }

    for( const auto &ppu : pus )
    {
      PredictionUnit &pu = subStruct.addPU( *ppu, _chType );

      pu = *ppu;
    }

    for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(area.chromaFormat); chType++)
    {
      std::copy_n(m_isDecomp[chType],
                  unitScale[getFirstComponentOfChannel(chType)].scale(area.block(chType).size()).area(),
                  subStruct.m_isDecomp[chType]);
    }
  }
}

void CodingStructure::useSubStructure( const CodingStructure& subStruct, const ChannelType chType, const UnitArea &subArea, const bool cpyPred /*= true*/, const bool cpyReco /*= true*/, const bool cpyOrgResi /*= true*/, const bool cpyResi /*= true*/, const bool updateCost /*= true*/ )
{
  UnitArea clippedArea = clipArea( subArea, *picture );

  setDecomp( clippedArea );

  CPelUnitBuf subPredBuf = cpyPred ? subStruct.getPredBuf( clippedArea ) : CPelUnitBuf();
  CPelUnitBuf subResiBuf = cpyResi ? subStruct.getResiBuf( clippedArea ) : CPelUnitBuf();
  CPelUnitBuf subRecoBuf = cpyReco ? subStruct.getRecoBuf( clippedArea ) : CPelUnitBuf();

  if( parent )
  {
    // copy data to picture
    if (cpyPred)
    {
      getPredBuf(clippedArea).copyFrom(subPredBuf);
    }
    if (cpyResi)
    {
      getResiBuf(clippedArea).copyFrom(subResiBuf);
    }
    if (cpyReco)
    {
      getRecoBuf(clippedArea).copyFrom(subRecoBuf);
    }
    if (cpyOrgResi)
    {
      getOrgResiBuf(clippedArea).copyFrom(subStruct.getOrgResiBuf(clippedArea));
    }
  }

  if (cpyPred)
  {
    picture->getPredBuf(clippedArea).copyFrom(subPredBuf);
  }
  if (cpyResi)
  {
    picture->getResiBuf(clippedArea).copyFrom(subResiBuf);
  }
  if (cpyReco)
  {
    picture->getRecoBuf(clippedArea).copyFrom(subRecoBuf);
  }

  if (!subStruct.m_isTuEnc && ((!slice->isIntra() || slice->getSPS()->getIBCFlag()) && chType != ChannelType::CHROMA))
  {
    // copy motion buffer
    MotionBuf ownMB  = getMotionBuf          ( clippedArea );
    CMotionBuf subMB = subStruct.getMotionBuf( clippedArea );

    ownMB.copyFrom( subMB );

    motionLut = subStruct.motionLut;
  }
  prevPLT = subStruct.prevPLT;


  if ( updateCost )
  {
    fracBits += subStruct.fracBits;
    dist     += subStruct.dist;
    cost     += subStruct.cost;
    costDbOffset += subStruct.costDbOffset;
    useInterlayerRef |= subStruct.useInterlayerRef;
  }
  if( parent )
  {
    // allow this to be false at the top level
    CHECKD( !area.contains( subArea ), "Trying to use a sub-structure not contained in self" );
  }

  // copy the CUs over
  if( subStruct.m_isTuEnc )
  {
    // don't copy if the substruct was created for encoding of the TUs
  }
  else
  {
    for( const auto &pcu : subStruct.cus )
    {
      // add an analogue CU into own CU store
      const UnitArea &cuPatch = *pcu;
      CodingUnit &cu = addCU( cuPatch, pcu->chType );

      // copy the CU info from subPatch
      cu = *pcu;
    }
  }

  // copy the PUs over
  if( subStruct.m_isTuEnc )
  {
    // don't copy if the substruct was created for encoding of the TUs
  }
  else
  {
    for( const auto &ppu : subStruct.pus )
    {
      // add an analogue PU into own PU store
      const UnitArea &puPatch = *ppu;
      PredictionUnit &pu = addPU( puPatch, ppu->chType );

      // copy the PU info from subPatch
      pu = *ppu;
    }
  }
  // copy the TUs over
  for( const auto &ptu : subStruct.tus )
  {
    // add an analogue TU into own TU store
    const UnitArea &tuPatch = *ptu;
    TransformUnit &tu = addTU( tuPatch, ptu->chType );

    // copy the TU info from subPatch
    tu = *ptu;
  }
}

void CodingStructure::copyStructure( const CodingStructure& other, const ChannelType chType, const bool copyTUs, const bool copyRecoBuf )
{
  fracBits = other.fracBits;
  dist     = other.dist;
  cost     = other.cost;
  costDbOffset = other.costDbOffset;
  useInterlayerRef = other.useInterlayerRef;

  CHECKD( area != other.area, "Incompatible sizes" );

  const UnitArea dualITreeArea = CS::getArea( *this, this->area, chType );

  // copy the CUs over
  for (const auto &pcu : other.cus)
  {
    if( !dualITreeArea.contains( *pcu ) )
    {
      continue;
    }
    // add an analogue CU into own CU store
    const UnitArea &cuPatch = *pcu;

    CodingUnit &cu = addCU(cuPatch, pcu->chType);

    // copy the CU info from subPatch
    cu = *pcu;
  }

  // copy the PUs over
  for (const auto &ppu : other.pus)
  {
    if( !dualITreeArea.contains( *ppu ) )
    {
      continue;
    }
    // add an analogue PU into own PU store
    const UnitArea &puPatch = *ppu;

    PredictionUnit &pu = addPU(puPatch, ppu->chType);
    // copy the PU info from subPatch
    pu = *ppu;
  }

  if (!other.slice->isIntra() || other.slice->getSPS()->getIBCFlag())
  {
    // copy motion buffer
    MotionBuf  ownMB = getMotionBuf();
    CMotionBuf subMB = other.getMotionBuf();

    ownMB.copyFrom( subMB );

    motionLut = other.motionLut;
  }
  prevPLT = other.prevPLT;

  if( copyTUs )
  {
    // copy the TUs over
    for( const auto &ptu : other.tus )
    {
      if( !dualITreeArea.contains( *ptu ) )
      {
        continue;
      }
      // add an analogue TU into own TU store
      const UnitArea &tuPatch = *ptu;
      TransformUnit &tu = addTU( tuPatch, ptu->chType );
      // copy the TU info from subPatch
      tu = *ptu;
    }
  }

  if( copyRecoBuf )
  {
    CPelUnitBuf recoBuf = other.getRecoBuf( area );

    if( parent )
    {
      // copy data to self for neighbors
      getRecoBuf( area ).copyFrom( recoBuf );
    }

    // copy data to picture
    picture->getRecoBuf( area ).copyFrom( recoBuf );
    if (other.pcv->isEncoder)
    {
      CPelUnitBuf predBuf = other.getPredBuf(area);
      if (parent)
      {
        getPredBuf(area).copyFrom(predBuf);
      }
      picture->getPredBuf(area).copyFrom(predBuf);
    }

    // required for DebugCTU
    for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(area.chromaFormat); chType++)
    {
      const size_t _area = unitScale[getFirstComponentOfChannel(chType)].scaleArea(area.block(chType).area());
      std::copy_n(other.m_isDecomp[chType], _area, m_isDecomp[chType]);
    }
  }
}

void CodingStructure::initStructData( const int &QP, const bool &skipMotBuf )
{
  clearPUs();
  clearTUs();
  clearCUs();

  if( QP < MAX_INT )
  {
    currQP.fill(QP);
  }

  if (!skipMotBuf && (!parent || ((!slice->isIntra() || slice->getSPS()->getIBCFlag()) && !m_isTuEnc)))
  {
    getMotionBuf().memset(0);
  }

  fracBits = 0;
  dist     = 0;
  cost     = MAX_DOUBLE;
  lumaCost = MAX_DOUBLE;
  costDbOffset = 0;
  useDbCost = false;
  interHad = std::numeric_limits<Distortion>::max();
  useInterlayerRef = false;
}


void CodingStructure::clearTUs()
{
  for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(area.chromaFormat); chType++)
  {
    size_t _area = (area.block(chType).area() >> unitScale[getFirstComponentOfChannel(chType)].area);
    std::fill_n(m_isDecomp[chType], _area, false);
    std::fill_n(m_tuIdx[chType], _area, 0);
  }

  const int numComp = getNumberValidComponents(area.chromaFormat);
  for (int i = 0; i < numComp; i++)
  {
    m_offsets[i] = 0;
  }

  for( auto &pcu : cus )
  {
    pcu->firstTU = pcu->lastTU = nullptr;
  }

  m_tuPool.giveBack(tus);
  m_numTUs = 0;
}

void CodingStructure::clearPUs()
{
  for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(area.chromaFormat); chType++)
  {
    std::fill_n(m_puIdx[chType], unitScale[getFirstComponentOfChannel(chType)].scaleArea(area.block(chType).area()), 0);
  }

  m_puPool.giveBack(pus);
  m_numPUs = 0;

  for( auto &pcu : cus )
  {
    pcu->firstPU = pcu->lastPU = nullptr;
  }
}

void CodingStructure::clearCUs()
{
  for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(area.chromaFormat); chType++)
  {
    std::fill_n(m_cuIdx[chType], unitScale[getFirstComponentOfChannel(chType)].scaleArea(area.block(chType).area()), 0);
  }

  m_cuPool.giveBack(cus);
  m_numCUs = 0;
}

MotionBuf CodingStructure::getMotionBuf( const Area& _area )
{
  const CompArea& _luma = area.Y();

  CHECKD( !_luma.contains( _area ), "Trying to access motion information outside of this coding structure" );

  const Area miArea   = g_miScaling.scale( _area );
  const Area selfArea = g_miScaling.scale( _luma );

  return MotionBuf( m_motionBuf + rsAddr( miArea.pos(), selfArea.pos(), selfArea.width ), selfArea.width, miArea.size() );
}

const CMotionBuf CodingStructure::getMotionBuf( const Area& _area ) const
{
  const CompArea& _luma = area.Y();

  CHECKD( !_luma.contains( _area ), "Trying to access motion information outside of this coding structure" );

  const Area miArea   = g_miScaling.scale( _area );
  const Area selfArea = g_miScaling.scale( _luma );

  return MotionBuf( m_motionBuf + rsAddr( miArea.pos(), selfArea.pos(), selfArea.width ), selfArea.width, miArea.size() );
}

MotionInfo& CodingStructure::getMotionInfo( const Position& pos )
{
  CHECKD( !area.Y().contains( pos ), "Trying to access motion information outside of this coding structure" );

  //return getMotionBuf().at( g_miScaling.scale( pos - area.lumaPos() ) );
  // bypass the motion buf calling and get the value directly
  const unsigned stride = g_miScaling.scaleHor( area.lumaSize().width );
  const Position miPos  = g_miScaling.scale( pos - area.lumaPos() );

  return *( m_motionBuf + miPos.y * stride + miPos.x );
}

const MotionInfo& CodingStructure::getMotionInfo( const Position& pos ) const
{
  CHECKD( !area.Y().contains( pos ), "Trying to access motion information outside of this coding structure" );

  //return getMotionBuf().at( g_miScaling.scale( pos - area.lumaPos() ) );
  // bypass the motion buf calling and get the value directly
  const unsigned stride = g_miScaling.scaleHor( area.lumaSize().width );
  const Position miPos  = g_miScaling.scale( pos - area.lumaPos() );

  return *( m_motionBuf + miPos.y * stride + miPos.x );
}


// data accessors
       PelBuf     CodingStructure::getPredBuf(const CompArea &blk)           { return getBuf(blk,  PIC_PREDICTION); }
const CPelBuf     CodingStructure::getPredBuf(const CompArea &blk)     const { return getBuf(blk,  PIC_PREDICTION); }
       PelUnitBuf CodingStructure::getPredBuf(const UnitArea &unit)          { return getBuf(unit, PIC_PREDICTION); }
const CPelUnitBuf CodingStructure::getPredBuf(const UnitArea &unit)    const { return getBuf(unit, PIC_PREDICTION); }

       PelBuf     CodingStructure::getResiBuf(const CompArea &blk)           { return getBuf(blk,  PIC_RESIDUAL); }
const CPelBuf     CodingStructure::getResiBuf(const CompArea &blk)     const { return getBuf(blk,  PIC_RESIDUAL); }
       PelUnitBuf CodingStructure::getResiBuf(const UnitArea &unit)          { return getBuf(unit, PIC_RESIDUAL); }
const CPelUnitBuf CodingStructure::getResiBuf(const UnitArea &unit)    const { return getBuf(unit, PIC_RESIDUAL); }

       PelBuf     CodingStructure::getRecoBuf(const CompArea &blk)           { return getBuf(blk,  PIC_RECONSTRUCTION); }
const CPelBuf     CodingStructure::getRecoBuf(const CompArea &blk)     const { return getBuf(blk,  PIC_RECONSTRUCTION); }
       PelUnitBuf CodingStructure::getRecoBuf(const UnitArea &unit)          { return getBuf(unit, PIC_RECONSTRUCTION); }
const CPelUnitBuf CodingStructure::getRecoBuf(const UnitArea &unit)    const { return getBuf(unit, PIC_RECONSTRUCTION); }

       PelBuf     CodingStructure::getOrgResiBuf(const CompArea &blk)        { return getBuf(blk,  PIC_ORG_RESI); }
const CPelBuf     CodingStructure::getOrgResiBuf(const CompArea &blk)  const { return getBuf(blk,  PIC_ORG_RESI); }
       PelUnitBuf CodingStructure::getOrgResiBuf(const UnitArea &unit)       { return getBuf(unit, PIC_ORG_RESI); }
const CPelUnitBuf CodingStructure::getOrgResiBuf(const UnitArea &unit) const { return getBuf(unit, PIC_ORG_RESI); }

       PelBuf     CodingStructure::getOrgBuf(const CompArea &blk)            { return getBuf(blk,  PIC_ORIGINAL); }
const CPelBuf     CodingStructure::getOrgBuf(const CompArea &blk)      const { return getBuf(blk,  PIC_ORIGINAL); }
       PelUnitBuf CodingStructure::getOrgBuf(const UnitArea &unit)           { return getBuf(unit, PIC_ORIGINAL); }
const CPelUnitBuf CodingStructure::getOrgBuf(const UnitArea &unit)     const { return getBuf(unit, PIC_ORIGINAL); }

       PelBuf     CodingStructure::getOrgBuf(const ComponentID &compID)      { return picture->getBuf(area.blocks[compID], PIC_ORIGINAL); }
const CPelBuf     CodingStructure::getOrgBuf(const ComponentID &compID)const { return picture->getBuf(area.blocks[compID], PIC_ORIGINAL); }
       PelUnitBuf CodingStructure::getOrgBuf()                               { return picture->getBuf(area, PIC_ORIGINAL); }
const CPelUnitBuf CodingStructure::getOrgBuf()                         const { return picture->getBuf(area, PIC_ORIGINAL); }
       PelUnitBuf CodingStructure::getTrueOrgBuf()                           { return picture->getBuf(area, PIC_TRUE_ORIGINAL); }
const CPelUnitBuf CodingStructure::getTrueOrgBuf()                     const { return picture->getBuf(area, PIC_TRUE_ORIGINAL); }

PelBuf CodingStructure::getBuf( const CompArea &blk, const PictureType &type )
{
  if (!blk.valid())
  {
    return PelBuf();
  }

  if (type == PIC_ORIGINAL)
  {
    return picture->getBuf(blk, type);
  }

  const ComponentID compID = blk.compID;

  PelStorage* buf = type == PIC_PREDICTION ? &m_pred : ( type == PIC_RESIDUAL ? &m_resi : ( type == PIC_RECONSTRUCTION ? &m_reco : ( type == PIC_ORG_RESI ? &m_orgr : nullptr ) ) );

  CHECK( !buf, "Unknown buffer requested" );

  CHECKD( !area.blocks[compID].contains( blk ), "Buffer not contained in self requested" );

  CompArea cFinal = blk;
  cFinal.relativeTo( area.blocks[compID] );

#if !KEEP_PRED_AND_RESI_SIGNALS
  if( !parent && ( type == PIC_RESIDUAL || type == PIC_PREDICTION ) )
  {
    cFinal.x &= ( pcv->maxCUWidthMask  >> getComponentScaleX( blk.compID, blk.chromaFormat ) );
    cFinal.y &= ( pcv->maxCUHeightMask >> getComponentScaleY( blk.compID, blk.chromaFormat ) );
  }
#endif

  return buf->getBuf( cFinal );
}

const CPelBuf CodingStructure::getBuf( const CompArea &blk, const PictureType &type ) const
{
  if (!blk.valid())
  {
    return PelBuf();
  }

  if (type == PIC_ORIGINAL)
  {
    return picture->getBuf(blk, type);
  }

  const ComponentID compID = blk.compID;

  const PelStorage* buf = type == PIC_PREDICTION ? &m_pred : ( type == PIC_RESIDUAL ? &m_resi : ( type == PIC_RECONSTRUCTION ? &m_reco : ( type == PIC_ORG_RESI ? &m_orgr : nullptr ) ) );

  CHECK( !buf, "Unknown buffer requested" );

  CHECKD( !area.blocks[compID].contains( blk ), "Buffer not contained in self requested" );

  CompArea cFinal = blk;
  cFinal.relativeTo( area.blocks[compID] );

#if !KEEP_PRED_AND_RESI_SIGNALS
  if( !parent && ( type == PIC_RESIDUAL || type == PIC_PREDICTION ) )
  {
    cFinal.x &= ( pcv->maxCUWidthMask  >> getComponentScaleX( blk.compID, blk.chromaFormat ) );
    cFinal.y &= ( pcv->maxCUHeightMask >> getComponentScaleY( blk.compID, blk.chromaFormat ) );
  }
#endif

  return buf->getBuf( cFinal );
}

PelUnitBuf CodingStructure::getBuf( const UnitArea &unit, const PictureType &type )
{
  // no parent fetching for buffers
  if (!isChromaEnabled(area.chromaFormat))
  {
    return PelUnitBuf( area.chromaFormat, getBuf( unit.Y(), type ) );
  }
  else
  {
    return PelUnitBuf( area.chromaFormat, getBuf( unit.Y(), type ), getBuf( unit.Cb(), type ), getBuf( unit.Cr(), type ) );
  }
}

const CPelUnitBuf CodingStructure::getBuf( const UnitArea &unit, const PictureType &type ) const
{
  // no parent fetching for buffers
  if (!isChromaEnabled(area.chromaFormat))
  {
    return CPelUnitBuf( area.chromaFormat, getBuf( unit.Y(), type ) );
  }
  else
  {
    return CPelUnitBuf( area.chromaFormat, getBuf( unit.Y(), type ), getBuf( unit.Cb(), type ), getBuf( unit.Cr(), type ) );
  }
}

const CodingUnit* CodingStructure::getCURestricted( const Position &pos, const CodingUnit& curCu, const ChannelType _chType ) const
{
  const CodingUnit* cu = getCU( pos, _chType );
  // exists       same slice and tile                  cu precedes curCu in encoding order
  //                                                  (thus, is either from parent CS in RD-search or its index is lower)
  const bool wavefrontsEnabled = curCu.slice->getSPS()->getEntropyCodingSyncEnabledFlag();
  int ctuSizeBit = floorLog2(curCu.cs->sps->getMaxCUWidth());
  int xNbY  = pos.x << getChannelTypeScaleX( _chType, curCu.chromaFormat );
  int        xCurr             = curCu.block(_chType).x << getChannelTypeScaleX(_chType, curCu.chromaFormat);
  bool addCheck = (wavefrontsEnabled && (xNbY >> ctuSizeBit) >= (xCurr >> ctuSizeBit) + 1 ) ? false : true;
  if( cu && CU::isSameSliceAndTile( *cu, curCu ) && ( cu->cs != curCu.cs || cu->idx <= curCu.idx ) && addCheck)
  {
    return cu;
  }
  else
  {
    return nullptr;
  }
}

const CodingUnit* CodingStructure::getCURestricted( const Position &pos, const Position curPos, const unsigned curSliceIdx, const TileIdx curTileIdx, const ChannelType _chType ) const
{
  const CodingUnit* cu = getCU( pos, _chType );
  const bool wavefrontsEnabled = this->slice->getSPS()->getEntropyCodingSyncEnabledFlag();
  int ctuSizeBit = floorLog2(this->sps->getMaxCUWidth());

  const int xNbY  = pos.x * (1 << getChannelTypeScaleX(_chType, this->area.chromaFormat));
  const int xCurr = curPos.x * (1 << getChannelTypeScaleX(_chType, this->area.chromaFormat));

  bool addCheck = (wavefrontsEnabled && (xNbY >> ctuSizeBit) >= (xCurr >> ctuSizeBit) + 1 ) ? false : true;
  return ( cu && cu->slice->getIndependentSliceIdx() == curSliceIdx && cu->tileIdx == curTileIdx && addCheck ) ? cu : nullptr;
}

const PredictionUnit* CodingStructure::getPURestricted( const Position &pos, const PredictionUnit& curPu, const ChannelType _chType ) const
{
  const PredictionUnit* pu = getPU( pos, _chType );
  // exists       same slice and tile                  pu precedes curPu in encoding order
  //                                                  (thus, is either from parent CS in RD-search or its index is lower)
  const bool wavefrontsEnabled = curPu.cu->slice->getSPS()->getEntropyCodingSyncEnabledFlag();
  int ctuSizeBit = floorLog2(curPu.cs->sps->getMaxCUWidth());
  int xNbY  = pos.x << getChannelTypeScaleX( _chType, curPu.chromaFormat );
  int        xCurr             = curPu.block(_chType).x << getChannelTypeScaleX(_chType, curPu.chromaFormat);
  bool addCheck = (wavefrontsEnabled && (xNbY >> ctuSizeBit) >= (xCurr >> ctuSizeBit) + 1 ) ? false : true;
  if( pu && CU::isSameSliceAndTile( *pu->cu, *curPu.cu ) && ( pu->cs != curPu.cs || pu->idx <= curPu.idx ) && addCheck )
  {
    return pu;
  }
  else
  {
    return nullptr;
  }
}

const TransformUnit* CodingStructure::getTURestricted( const Position &pos, const TransformUnit& curTu, const ChannelType _chType ) const
{
  const TransformUnit* tu = getTU( pos, _chType );
  // exists       same slice and tile                  tu precedes curTu in encoding order
  //                                                  (thus, is either from parent CS in RD-search or its index is lower)
  const bool wavefrontsEnabled = curTu.cu->slice->getSPS()->getEntropyCodingSyncEnabledFlag();
  int ctuSizeBit = floorLog2(curTu.cs->sps->getMaxCUWidth());
  int xNbY  = pos.x << getChannelTypeScaleX( _chType, curTu.chromaFormat );
  int        xCurr             = curTu.block(_chType).x << getChannelTypeScaleX(_chType, curTu.chromaFormat);
  bool addCheck = (wavefrontsEnabled && (xNbY >> ctuSizeBit) >= (xCurr >> ctuSizeBit) + 1 ) ? false : true;
  if( tu && CU::isSameSliceAndTile( *tu->cu, *curTu.cu ) && ( tu->cs != curTu.cs || tu->idx <= curTu.idx ) && addCheck )
  {
    return tu;
  }
  else
  {
    return nullptr;
  }
}

