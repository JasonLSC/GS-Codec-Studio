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

/** \file     Unit.cpp
 *  \brief    defines unit as a set of blocks and basic unit types (coding, prediction, transform)
 */

#include <array>

#include "Unit.h"

#include "Buffer.h"
#include "Picture.h"
#include "ChromaFormat.h"

#include "UnitTools.h"
#include "UnitPartitioner.h"

#include "ChromaFormat.h"

 // ---------------------------------------------------------------------------
 // block method definitions
 // ---------------------------------------------------------------------------

void CompArea::xRecalcLumaToChroma()
{
  const uint32_t csx = getComponentScaleX(compID, chromaFormat);
  const uint32_t csy = getComponentScaleY(compID, chromaFormat);

  x      >>= csx;
  y      >>= csy;
  width  >>= csx;
  height >>= csy;
}

Position CompArea::chromaPos() const
{
  if (isLuma(compID))
  {
    uint32_t scaleX = getComponentScaleX(compID, chromaFormat);
    uint32_t scaleY = getComponentScaleY(compID, chromaFormat);

    return Position(x >> scaleX, y >> scaleY);
  }
  else
  {
    return *this;
  }
}

Size CompArea::lumaSize() const
{
  if( isChroma( compID ) )
  {
    uint32_t scaleX = getComponentScaleX( compID, chromaFormat );
    uint32_t scaleY = getComponentScaleY( compID, chromaFormat );

    return Size( width << scaleX, height << scaleY );
  }
  else
  {
    return *this;
  }
}

Size CompArea::chromaSize() const
{
  if( isLuma( compID ) )
  {
    uint32_t scaleX = getComponentScaleX( compID, chromaFormat );
    uint32_t scaleY = getComponentScaleY( compID, chromaFormat );

    return Size( width >> scaleX, height >> scaleY );
  }
  else
  {
    return *this;
  }
}

Position CompArea::lumaPos() const
{
  if( isChroma( compID ) )
  {
    uint32_t scaleX = getComponentScaleX( compID, chromaFormat );
    uint32_t scaleY = getComponentScaleY( compID, chromaFormat );

    return Position( x << scaleX, y << scaleY );
  }
  else
  {
    return *this;
  }
}

Position CompArea::compPos( const ComponentID compID ) const
{
  return isLuma( compID ) ? lumaPos() : chromaPos();
}

Position CompArea::chanPos( const ChannelType chType ) const
{
  return isLuma( chType ) ? lumaPos() : chromaPos();
}

// ---------------------------------------------------------------------------
// unit method definitions
// ---------------------------------------------------------------------------

UnitArea::UnitArea(const ChromaFormat _chromaFormat) : chromaFormat(_chromaFormat) { }

UnitArea::UnitArea(const ChromaFormat _chromaFormat, const Area &_area) : chromaFormat(_chromaFormat), blocks(getNumberValidComponents(_chromaFormat))
{
  const uint32_t numCh = getNumberValidComponents(chromaFormat);

  for (uint32_t i = 0; i < numCh; i++)
  {
    blocks[i] = CompArea(ComponentID(i), chromaFormat, _area, true);
  }
}

UnitArea::UnitArea(const ChromaFormat _chromaFormat, const CompArea &blkY) : chromaFormat(_chromaFormat), blocks { blkY } {}

UnitArea::UnitArea(const ChromaFormat _chromaFormat,       CompArea &&blkY) : chromaFormat(_chromaFormat), blocks { std::forward<CompArea>(blkY) } {}

UnitArea::UnitArea(const ChromaFormat _chromaFormat, const CompArea &blkY, const CompArea &blkCb, const CompArea &blkCr)  : chromaFormat(_chromaFormat), blocks { blkY, blkCb, blkCr } {}

UnitArea::UnitArea(const ChromaFormat _chromaFormat,       CompArea &&blkY,      CompArea &&blkCb,      CompArea &&blkCr) : chromaFormat(_chromaFormat), blocks { std::forward<CompArea>(blkY), std::forward<CompArea>(blkCb), std::forward<CompArea>(blkCr) } {}

bool UnitArea::contains(const UnitArea& other) const
{
  bool ret = true;
  bool any = false;

  for( const auto &blk : other.blocks )
  {
    if( blk.valid() && blocks[blk.compID].valid() )
    {
      ret &= blocks[blk.compID].contains( blk );
      any = true;
    }
  }

  return any && ret;
}

bool UnitArea::contains( const UnitArea& other, const ChannelType chType ) const
{
  bool ret = true;
  bool any = false;

  for( const auto &blk : other.blocks )
  {
    if( toChannelType( blk.compID ) == chType && blk.valid() && blocks[blk.compID].valid() )
    {
      ret &= blocks[blk.compID].contains( blk );
      any = true;
    }
  }

  return any && ret;
}

#if REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
void UnitArea::resizeTo( const UnitArea& unitArea )
{
  for( uint32_t i = 0; i < blocks.size(); i++ )
  {
    blocks[i].resizeTo( unitArea.blocks[i] );
  }
}
#endif

void UnitArea::repositionTo(const UnitArea& unitArea)
{
  for(uint32_t i = 0; i < blocks.size(); i++)
  {
    blocks[i].repositionTo(unitArea.blocks[i]);
  }
}

UnitArea UnitArea::singleChan(const ChannelType chType) const
{
  UnitArea ret(chromaFormat);

  for (const auto &blk : blocks)
  {
    ret.blocks.push_back(toChannelType(blk.compID) == chType ? blk : CompArea());
  }

  return ret;
}

// ---------------------------------------------------------------------------
// coding unit method definitions
// ---------------------------------------------------------------------------

CodingUnit::CodingUnit(const UnitArea &unit)
  : UnitArea(unit)
  , cs(nullptr)
  , slice(nullptr)
  , chType(ChannelType::LUMA)
  , next(nullptr)
  , firstPU(nullptr)
  , lastPU(nullptr)
  , firstTU(nullptr)
  , lastTU(nullptr)
{
  initData();
}
CodingUnit::CodingUnit(const ChromaFormat _chromaFormat, const Area &_area)
  : UnitArea(_chromaFormat, _area)
  , cs(nullptr)
  , slice(nullptr)
  , chType(ChannelType::LUMA)
  , next(nullptr)
  , firstPU(nullptr)
  , lastPU(nullptr)
  , firstTU(nullptr)
  , lastTU(nullptr)
{
  initData();
}

CodingUnit& CodingUnit::operator=( const CodingUnit& other )
{
  slice             = other.slice;
  predMode          = other.predMode;
  qtDepth           = other.qtDepth;
  depth             = other.depth;
  btDepth           = other.btDepth;
  mtDepth           = other.mtDepth;
  splitSeries       = other.splitSeries;
  skip              = other.skip;
  mmvdSkip = other.mmvdSkip;
  affine            = other.affine;
  affineType        = other.affineType;
  colorTransform = other.colorTransform;
  geoFlag           = other.geoFlag;
  bdpcmMode         = other.bdpcmMode;
  bdpcmModeChroma   = other.bdpcmModeChroma;
  qp                = other.qp;
  chromaQpAdj       = other.chromaQpAdj;
  rootCbf           = other.rootCbf;
  sbtInfo           = other.sbtInfo;
  mtsFlag           = other.mtsFlag;
  lfnstIdx          = other.lfnstIdx;
  tileIdx           = other.tileIdx;
  imv               = other.imv;
  bcwIdx            = other.bcwIdx;
  for (int i = 0; i<2; i++)
  {
    refIdxBi[i] = other.refIdxBi[i];
  }

  smvdMode        = other.smvdMode;
  ispMode           = other.ispMode;
  mipFlag           = other.mipFlag;

  if (slice->getSPS()->getPLTMode())
  {
    for (int idx = 0; idx < MAX_NUM_CHANNEL_TYPE; idx++)
    {
      curPLTSize[idx]   = other.curPLTSize[idx];
      useEscape[idx]    = other.useEscape[idx];
      useRotation[idx]  = other.useRotation[idx];
      reusePLTSize[idx] = other.reusePLTSize[idx];
      lastPLTSize[idx]  = other.lastPLTSize[idx];
      std::copy_n(other.reuseflag[idx], MAXPLTPREDSIZE, reuseflag[idx]);
    }

    for (int idx = 0; idx < MAX_NUM_COMPONENT; idx++)
    {
      std::copy_n(other.curPLT[idx], MAXPLTSIZE, curPLT[idx]);
    }
  }

  treeType          = other.treeType;
  modeType          = other.modeType;
  modeTypeSeries    = other.modeTypeSeries;
  return *this;
}

void CodingUnit::initData()
{
  predMode          = NUMBER_OF_PREDICTION_MODES;
  qtDepth           = 0;
  depth             = 0;
  btDepth           = 0;
  mtDepth           = 0;
  splitSeries       = 0;
  skip              = false;
  mmvdSkip = false;
  affine            = false;
  affineType        = AffineModel::_4_PARAMS;
  colorTransform = false;
  geoFlag           = false;
  bdpcmMode         = BdpcmMode::NONE;
  bdpcmModeChroma   = BdpcmMode::NONE;
  qp                = 0;
  chromaQpAdj       = 0;
  rootCbf           = true;
  sbtInfo           = 0;
  mtsFlag           = 0;
  lfnstIdx          = 0;
  tileIdx           = 0;
  imv               = 0;
  bcwIdx            = BCW_DEFAULT;
  for (int i = 0; i < 2; i++)
  {
    refIdxBi[i] = -1;
  }
  smvdMode        = 0;
  ispMode           = ISPType::NONE;
  mipFlag           = false;

  if (cs && cs->sps->getPLTMode())
  {
    for (int idx = 0; idx < MAX_NUM_CHANNEL_TYPE; idx++)
    {
      curPLTSize[idx]   = 0;
      reusePLTSize[idx] = 0;
      lastPLTSize[idx]  = 0;
      useEscape[idx]    = false;
      useRotation[idx]  = false;
      std::fill_n(reuseflag[idx], MAXPLTPREDSIZE, false);
    }

    for (int idx = 0; idx < MAX_NUM_COMPONENT; idx++)
    {
      std::fill_n(curPLT[idx], MAXPLTSIZE, 0);
    }
  }

  treeType          = TREE_D;
  modeType          = MODE_TYPE_ALL;
  modeTypeSeries    = 0;
}

const bool CodingUnit::isSepTree() const
{
  return treeType != TREE_D || CS::isDualITree( *cs );
}

const bool CodingUnit::isLocalSepTree() const
{
  return treeType != TREE_D && !CS::isDualITree(*cs);
}

const bool CodingUnit::checkCCLMAllowed() const
{
  bool allowCCLM = false;

  if( !CS::isDualITree( *cs ) ) //single tree I slice or non-I slice (Note: judging chType is no longer equivalent to checking dual-tree I slice since the local dual-tree is introduced)
  {
    allowCCLM = true;
  }
  else if( slice->getSPS()->getCTUSize() <= 32 ) //dual tree, CTUsize < 64
  {
    allowCCLM = true;
  }
  else //dual tree, CTU size 64 or 128
  {
    int depthFor64x64Node = slice->getSPS()->getCTUSize() == 128 ? 1 : 0;
    const PartSplit cuSplitTypeDepth1 = CU::getSplitAtDepth( *this, depthFor64x64Node );
    const PartSplit cuSplitTypeDepth2 = CU::getSplitAtDepth( *this, depthFor64x64Node + 1 );

    //allow CCLM if 64x64 chroma tree node uses QT split or HBT+VBT split combination
    if( cuSplitTypeDepth1 == CU_QUAD_SPLIT || (cuSplitTypeDepth1 == CU_HORZ_SPLIT && cuSplitTypeDepth2 == CU_VERT_SPLIT) )
    {
      if (chromaFormat == ChromaFormat::_420)
      {
        CHECK( !(blocks[COMPONENT_Cb].width <= 16 && blocks[COMPONENT_Cb].height <= 16), "chroma cu size shall be <= 16x16 for YUV420 format" );
      }
      allowCCLM = true;
    }
    //allow CCLM if 64x64 chroma tree node uses NS (No Split) and becomes a chroma CU containing 32x32 chroma blocks
    else if( cuSplitTypeDepth1 == CU_DONT_SPLIT )
    {
      if (chromaFormat == ChromaFormat::_420)
      {
        CHECK( !(blocks[COMPONENT_Cb].width == 32 && blocks[COMPONENT_Cb].height == 32), "chroma cu size shall be 32x32 for YUV420 format" );
      }
      allowCCLM = true;
    }
    //allow CCLM if 64x32 chroma tree node uses NS and becomes a chroma CU containing 32x16 chroma blocks
    else if( cuSplitTypeDepth1 == CU_HORZ_SPLIT && cuSplitTypeDepth2 == CU_DONT_SPLIT )
    {
      if (chromaFormat == ChromaFormat::_420)
      {
        CHECK( !(blocks[COMPONENT_Cb].width == 32 && blocks[COMPONENT_Cb].height == 16), "chroma cu size shall be 32x16 for YUV420 format" );
      }
      allowCCLM = true;
    }

    //further check luma conditions
    if( allowCCLM )
    {
      //disallow CCLM if luma 64x64 block uses BT or TT or NS with ISP
      const Position lumaRefPos( chromaPos().x << getComponentScaleX( COMPONENT_Cb, chromaFormat ), chromaPos().y << getComponentScaleY( COMPONENT_Cb, chromaFormat ) );
      const CodingUnit *colLumaCu = cs->picture->cs->getCU(lumaRefPos, ChannelType::LUMA);

      if( colLumaCu->lwidth() < 64 || colLumaCu->lheight() < 64 ) //further split at 64x64 luma node
      {
        const PartSplit cuSplitTypeDepth1Luma = CU::getSplitAtDepth( *colLumaCu, depthFor64x64Node );
        CHECK( !(cuSplitTypeDepth1Luma >= CU_QUAD_SPLIT && cuSplitTypeDepth1Luma <= CU_TRIV_SPLIT), "split mode shall be BT, TT or QT" );
        if( cuSplitTypeDepth1Luma != CU_QUAD_SPLIT )
        {
          allowCCLM = false;
        }
      }
      else if (colLumaCu->lwidth() == 64 && colLumaCu->lheight() == 64
               && colLumaCu->ispMode != ISPType::NONE)   // not split at 64x64 luma node and use ISP mode
      {
        allowCCLM = false;
      }
    }
  }

  return allowCCLM;
}

const uint8_t CodingUnit::checkAllowedSbt() const
{
  if( !slice->getSPS()->getUseSBT() )
  {
    return 0;
  }

  //check on prediction mode
  if (predMode == MODE_INTRA || predMode == MODE_IBC || predMode == MODE_PLT ) //intra, palette or IBC
  {
    return 0;
  }
  if( firstPU->ciipFlag )
  {
    return 0;
  }

  const int cuWidth  = lwidth();
  const int cuHeight = lheight();

  //parameter
  const int maxSbtCUSize = cs->sps->getMaxTbSize();
  const int minSbtCUSize = 1 << (MIN_CU_LOG2 + 1);

  //check on size
  if( cuWidth > maxSbtCUSize || cuHeight > maxSbtCUSize )
  {
    return 0;
  }

  std::array<bool, NUMBER_SBT_IDX> allowType;

  allowType.fill(false);
  allowType[SBT_VER_HALF] = cuWidth >= minSbtCUSize;
  allowType[SBT_HOR_HALF] = cuHeight >= minSbtCUSize;
  allowType[SBT_VER_QUAD] = cuWidth >= 2 * minSbtCUSize;
  allowType[SBT_HOR_QUAD] = cuHeight >= 2 * minSbtCUSize;

  uint8_t sbtAllowed = 0;

  for (int i = 0; i < allowType.size(); i++)
  {
    sbtAllowed += (uint8_t) allowType[i] << i;
  }

  return sbtAllowed;
}

uint8_t CodingUnit::getSbtTuSplit() const
{
  uint8_t sbtTuSplitType = 0;

  switch( getSbtIdx() )
  {
  case SBT_VER_HALF: sbtTuSplitType = ( getSbtPos() == SBT_POS0 ? 0 : 1 ) + SBT_VER_HALF_POS0_SPLIT; break;
  case SBT_HOR_HALF: sbtTuSplitType = ( getSbtPos() == SBT_POS0 ? 0 : 1 ) + SBT_HOR_HALF_POS0_SPLIT; break;
  case SBT_VER_QUAD: sbtTuSplitType = ( getSbtPos() == SBT_POS0 ? 0 : 1 ) + SBT_VER_QUAD_POS0_SPLIT; break;
  case SBT_HOR_QUAD: sbtTuSplitType = ( getSbtPos() == SBT_POS0 ? 0 : 1 ) + SBT_HOR_QUAD_POS0_SPLIT; break;
  default: assert( 0 );  break;
  }

  assert( sbtTuSplitType <= SBT_HOR_QUAD_POS1_SPLIT && sbtTuSplitType >= SBT_VER_HALF_POS0_SPLIT );
  return sbtTuSplitType;
}

// ---------------------------------------------------------------------------
// prediction unit method definitions
// ---------------------------------------------------------------------------

PredictionUnit::PredictionUnit(const UnitArea &unit)
  : UnitArea(unit), cu(nullptr), cs(nullptr), chType(ChannelType::LUMA), next(nullptr)
{
  initData();
}
PredictionUnit::PredictionUnit(const ChromaFormat _chromaFormat, const Area &_area)
  : UnitArea(_chromaFormat, _area), cu(nullptr), cs(nullptr), chType(ChannelType::LUMA), next(nullptr)
{
  initData();
}

void PredictionUnit::initData()
{
  intraDir[ChannelType::LUMA]   = DC_IDX;
  intraDir[ChannelType::CHROMA] = PLANAR_IDX;
  mipTransposedFlag = false;
  multiRefIdx = 0;

  // inter data
  mergeFlag   = false;
  regularMergeFlag = false;
  mergeIdx    = MAX_UCHAR;
  geoSplitDir  = MAX_UCHAR;
  geoMergeIdx.fill(MAX_UCHAR);
  mmvdMergeFlag = false;
  mmvdMergeIdx.val = MmvdIdx::INVALID;
  interDir    = MAX_UCHAR;
  mergeType        = MergeType::DEFAULT_N;
  bv.setZero();
  bvd.setZero();
  for (uint32_t i = 0; i < MAX_NUM_SUBCU_DMVR; i++)
  {
    mvdL0SubPu[i].setZero();
  }
  dmvrImpreciseMv = false;
  for (uint32_t i = 0; i < NUM_REF_PIC_LIST_01; i++)
  {
    mvpIdx[i] = MAX_UCHAR;
    mvpNum[i] = MAX_UCHAR;
    refIdx[i] = -1;
    mv[i]     .setZero();
    mvd[i]    .setZero();
    for( uint32_t j = 0; j < 3; j++ )
    {
      mvdAffi[i][j].setZero();
    }
    for ( uint32_t j = 0; j < 3; j++ )
    {
      mvAffi[i][j].setZero();
#if GDR_ENABLED
      mvAffiSolid[i][j] = true;
      mvAffiValid[i][j] = true;
#endif
    }
  }
  ciipFlag = false;
  mmvdEncOptMode = 0;
}

PredictionUnit& PredictionUnit::operator=(const IntraPredictionData& predData)
{
  intraDir          = predData.intraDir;
  mipTransposedFlag = predData.mipTransposedFlag;
  multiRefIdx       = predData.multiRefIdx;

  return *this;
}

PredictionUnit& PredictionUnit::operator=(const InterPredictionData& predData)
{
  mergeFlag        = predData.mergeFlag;
  regularMergeFlag = predData.regularMergeFlag;
  mergeIdx         = predData.mergeIdx;
  geoSplitDir      = predData.geoSplitDir;
  geoMergeIdx      = predData.geoMergeIdx;
  mmvdMergeFlag    = predData.mmvdMergeFlag;
  mmvdMergeIdx     = predData.mmvdMergeIdx;
  interDir         = predData.interDir;
  mergeType        = predData.mergeType;
  bv               = predData.bv;
  bvd              = predData.bvd;
  ciipFlag         = predData.ciipFlag;

  for (uint32_t i = 0; i < MAX_NUM_SUBCU_DMVR; i++)
  {
    mvdL0SubPu[i] = predData.mvdL0SubPu[i];
  }
  dmvrImpreciseMv = predData.dmvrImpreciseMv;
  for (uint32_t i = 0; i < NUM_REF_PIC_LIST_01; i++)
  {
    mvpIdx[i]   = predData.mvpIdx[i];
    mvpNum[i]   = predData.mvpNum[i];
    mv[i]       = predData.mv[i];
    mvd[i]      = predData.mvd[i];
    refIdx[i]   = predData.refIdx[i];
    for( uint32_t j = 0; j < 3; j++ )
    {
      mvdAffi[i][j] = predData.mvdAffi[i][j];
    }
    for ( uint32_t j = 0; j < 3; j++ )
    {
      mvAffi[i][j] = predData.mvAffi[i][j];
    }
  }

  return *this;
}

PredictionUnit& PredictionUnit::operator=( const PredictionUnit& other )
{
  intraDir          = other.intraDir;
  mipTransposedFlag = other.mipTransposedFlag;
  multiRefIdx       = other.multiRefIdx;

  mergeFlag        = other.mergeFlag;
  regularMergeFlag = other.regularMergeFlag;
  mergeIdx         = other.mergeIdx;
  geoSplitDir      = other.geoSplitDir;
  geoMergeIdx      = other.geoMergeIdx;
  mmvdMergeFlag    = other.mmvdMergeFlag;
  mmvdMergeIdx     = other.mmvdMergeIdx;
  interDir         = other.interDir;
  mergeType        = other.mergeType;
  bv               = other.bv;
  bvd              = other.bvd;
  ciipFlag         = other.ciipFlag;

  for (uint32_t i = 0; i < MAX_NUM_SUBCU_DMVR; i++)
  {
    mvdL0SubPu[i] = other.mvdL0SubPu[i];
  }
  dmvrImpreciseMv = other.dmvrImpreciseMv;
  for (uint32_t i = 0; i < NUM_REF_PIC_LIST_01; i++)
  {
    mvpIdx[i]   = other.mvpIdx[i];
    mvpNum[i]   = other.mvpNum[i];
    mv[i]       = other.mv[i];
    mvd[i]      = other.mvd[i];
    refIdx[i]   = other.refIdx[i];
    for( uint32_t j = 0; j < 3; j++ )
    {
      mvdAffi[i][j] = other.mvdAffi[i][j];
    }
    for ( uint32_t j = 0; j < 3; j++ )
    {
      mvAffi[i][j] = other.mvAffi[i][j];
    }
  }

  return *this;
}

PredictionUnit& PredictionUnit::operator=( const MotionInfo& mi )
{
  interDir = mi.interDir;

  for( uint32_t i = 0; i < NUM_REF_PIC_LIST_01; i++ )
  {
    refIdx[i] = mi.refIdx[i];
    mv    [i] = mi.mv[i];
  }

  return *this;
}

const MotionInfo& PredictionUnit::getMotionInfo() const
{
  return cs->getMotionInfo( lumaPos() );
}

const MotionInfo& PredictionUnit::getMotionInfo( const Position& pos ) const
{
  CHECKD( !Y().contains( pos ), "Trying to access motion info outsied of PU" );
  return cs->getMotionInfo( pos );
}

MotionBuf PredictionUnit::getMotionBuf()
{
  return cs->getMotionBuf( *this );
}

CMotionBuf PredictionUnit::getMotionBuf() const
{
  return cs->getMotionBuf( *this );
}

bool PredictionUnit::checkUseInterLayerRef() const
{
  bool useInterLayerRef=false;
  for( uint32_t i = 0; i < NUM_REF_PIC_LIST_01; i++ )
  {
    if (refIdx[i] >= 0)
    {
      useInterLayerRef |= cu->slice->getRpl(RefPicList(i))->isInterLayerRefPic(refIdx[i]);
    }
  }
  return useInterLayerRef;
}

// ---------------------------------------------------------------------------
// transform unit method definitions
// ---------------------------------------------------------------------------

TransformUnit::TransformUnit(const UnitArea &unit)
  : UnitArea(unit), cu(nullptr), cs(nullptr), chType(ChannelType::LUMA), next(nullptr)
{
  for( unsigned i = 0; i < MAX_NUM_TBLOCKS; i++ )
  {
    m_coeffs[i] = nullptr;
    m_pltIdxBuf[i] = nullptr;
  }

  m_runType.fill(nullptr);

  initData();
}

TransformUnit::TransformUnit(const ChromaFormat _chromaFormat, const Area &_area)
  : UnitArea(_chromaFormat, _area), cu(nullptr), cs(nullptr), chType(ChannelType::LUMA), next(nullptr)
{
  for( unsigned i = 0; i < MAX_NUM_TBLOCKS; i++ )
  {
    m_coeffs[i] = nullptr;
    m_pltIdxBuf[i] = nullptr;
  }

  m_runType.fill(nullptr);

  initData();
}

void TransformUnit::initData()
{
  for( unsigned i = 0; i < MAX_NUM_TBLOCKS; i++ )
  {
    cbf[i]           = 0;
    mtsIdx[i]        = MtsType::DCT2_DCT2;
  }
  depth              = 0;
  noResidual         = false;
  jointCbCr          = 0;
  m_chromaResScaleInv = 0;
}

void TransformUnit::init(TCoeff** coeffs, Pel** pltIdxBuf, EnumArray<PLTRunMode*, ChannelType>& runType)
{
  uint32_t numBlocks = getNumberValidTBlocks(*cs->pcv);

  for (uint32_t i = 0; i < numBlocks; i++)
  {
    m_coeffs[i] = coeffs[i];
    m_pltIdxBuf[i] = pltIdxBuf[i];
  }

  for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(cs->pcv->chrFormat); chType++)
  {
    m_runType[chType] = runType[chType];
  }
}

TransformUnit& TransformUnit::operator=(const TransformUnit& other)
{
  CHECK( chromaFormat != other.chromaFormat, "Incompatible formats" );

  const int numBlocks = ::getNumberValidTBlocks(*cs->pcv);

  for (int i = 0; i < numBlocks; i++)
  {
    CHECKD( blocks[i].area() != other.blocks[i].area(), "Transformation units cover different areas" );

    const uint32_t area = blocks[i].area();

    if (m_coeffs[i] && other.m_coeffs[i] && m_coeffs[i] != other.m_coeffs[i])
    {
      std::copy_n(other.m_coeffs[i], area, m_coeffs[i]);
    }

    if (cs->sps->getPLTMode() && m_pltIdxBuf[i] && other.m_pltIdxBuf[i] && m_pltIdxBuf[i] != other.m_pltIdxBuf[i])
    {
      std::copy_n(other.m_pltIdxBuf[i], area, m_pltIdxBuf[i]);
    }

    cbf[i]    = other.cbf[i];
    mtsIdx[i] = other.mtsIdx[i];
  }

  if (cu->slice->getSPS()->getPLTMode())
  {
    for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(cs->pcv->chrFormat); chType++)
    {
      if (m_runType[chType] != nullptr && other.m_runType[chType] != nullptr
          && m_runType[chType] != other.m_runType[chType])
      {
        const uint32_t area = block(chType).area();
        std::copy_n(other.m_runType[chType], area, m_runType[chType]);
      }
    }
  }

  depth      = other.depth;
  noResidual = other.noResidual;
  jointCbCr  = other.jointCbCr;

  return *this;
}

void TransformUnit::copyComponentFrom(const TransformUnit& other, const ComponentID i)
{
  CHECK( chromaFormat != other.chromaFormat, "Incompatible formats" );
  CHECKD( blocks[i].area() != other.blocks[i].area(), "Transformation units cover different areas" );

  const uint32_t area = blocks[i].area();

  if (m_coeffs[i] && other.m_coeffs[i] && m_coeffs[i] != other.m_coeffs[i])
  {
    std::copy_n(other.m_coeffs[i], area, m_coeffs[i]);
  }

  if (m_pltIdxBuf[i] && other.m_pltIdxBuf[i] && m_pltIdxBuf[i] != other.m_pltIdxBuf[i])
  {
    std::copy_n(other.m_pltIdxBuf[i], area, m_pltIdxBuf[i]);
  }

  const ChannelType chType = toChannelType(i);
  if (i == getFirstComponentOfChannel(chType))
  {
    if (m_runType[chType] != nullptr && other.m_runType[chType] != nullptr
        && m_runType[chType] != other.m_runType[chType])
    {
      std::copy_n(other.m_runType[chType], area, m_runType[chType]);
    }
  }

  cbf[i]     = other.cbf[i];
  mtsIdx[i]  = other.mtsIdx[i];
  depth      = other.depth;
  noResidual = other.noResidual;
  jointCbCr  = isChroma(i) ? other.jointCbCr : jointCbCr;
}

       CoeffBuf TransformUnit::getCoeffs(const ComponentID id)       { return  CoeffBuf(m_coeffs[id], blocks[id]); }
const CCoeffBuf TransformUnit::getCoeffs(const ComponentID id) const { return CCoeffBuf(m_coeffs[id], blocks[id]); }

       PelBuf       TransformUnit::getcurPLTIdx(const ComponentID id)         { return        PelBuf(m_pltIdxBuf[id], blocks[id]); }
const CPelBuf       TransformUnit::getcurPLTIdx(const ComponentID id)   const { return       CPelBuf(m_pltIdxBuf[id], blocks[id]); }

PLTtypeBuf        TransformUnit::getrunType(const ChannelType id) { return PLTtypeBuf(m_runType[id], block(id)); }
const CPLTtypeBuf TransformUnit::getrunType(const ChannelType id) const
{
  return CPLTtypeBuf(m_runType[id], block(id));
}

       PLTescapeBuf TransformUnit::getescapeValue(const ComponentID id)       { return  PLTescapeBuf(m_coeffs[id], blocks[id]); }
const CPLTescapeBuf TransformUnit::getescapeValue(const ComponentID id) const { return CPLTescapeBuf(m_coeffs[id], blocks[id]); }

      Pel*          TransformUnit::getPLTIndex   (const ComponentID id)       { return  m_pltIdxBuf[id];    }
      PLTRunMode*   TransformUnit::getRunTypes(const ChannelType id) { return m_runType[id]; }

      void TransformUnit::checkTuNoResidual(unsigned idx)
      {
        if (CU::getSbtIdx(cu->sbtInfo) == SBT_OFF_DCT)
        {
          return;
        }

        if ((CU::getSbtPos(cu->sbtInfo) == SBT_POS0 && idx == 1)
            || (CU::getSbtPos(cu->sbtInfo) == SBT_POS1 && idx == 0))
        {
          noResidual = true;
        }
      }

int TransformUnit::getTbAreaAfterCoefZeroOut(ComponentID compID) const
{
  int tbArea = blocks[compID].width * blocks[compID].height;
  int tbZeroOutWidth = blocks[compID].width;
  int tbZeroOutHeight = blocks[compID].height;

  if (cs->sps->getMtsEnabled() && cu->sbtInfo != 0 && blocks[compID].width <= 32 && blocks[compID].height <= 32
      && compID == COMPONENT_Y)
  {
    tbZeroOutWidth = (blocks[compID].width == 32) ? 16 : tbZeroOutWidth;
    tbZeroOutHeight = (blocks[compID].height == 32) ? 16 : tbZeroOutHeight;
  }
  tbZeroOutWidth  = getNonzeroTuSize(tbZeroOutWidth);
  tbZeroOutHeight = getNonzeroTuSize(tbZeroOutHeight);
  tbArea = tbZeroOutWidth * tbZeroOutHeight;
  return tbArea;
}

int          TransformUnit::getChromaAdj()                     const { return m_chromaResScaleInv; }
void         TransformUnit::setChromaAdj(int i)                      { m_chromaResScaleInv = i;    }
