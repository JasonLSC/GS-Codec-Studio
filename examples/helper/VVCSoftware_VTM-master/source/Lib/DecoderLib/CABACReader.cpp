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

/** \file     CABACReader.cpp
 *  \brief    Reader for low level syntax
 */

#include "CABACReader.h"

#include "CommonLib/CodingStructure.h"
#include "CommonLib/TrQuant.h"
#include "CommonLib/UnitTools.h"
#include "CommonLib/SampleAdaptiveOffset.h"
#include "CommonLib/dtrace_next.h"
#include "CommonLib/Picture.h"
#include "CommonLib/MatrixIntraPrediction.h"

#if RExt__DECODER_DEBUG_BIT_STATISTICS
#include "CommonLib/CodingStatistics.h"
#endif

#if RExt__DECODER_DEBUG_BIT_STATISTICS
#define RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET(x)                                                               \
  const CodingStatisticsClassType CSCT(x);                                                                             \
  m_binDecoder.set(CSCT)
#define RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET2(x, y)                                                           \
  const CodingStatisticsClassType CSCT(x, y);                                                                          \
  m_binDecoder.set(CSCT)
#define RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE(x, s)                                                       \
  const CodingStatisticsClassType CSCT(x, s.width, s.height);                                                          \
  m_binDecoder.set(CSCT)
#define RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2(x, s, z)                                                   \
  const CodingStatisticsClassType CSCT(x, s.width, s.height, z);                                                       \
  m_binDecoder.set(CSCT)
#define RExt__DECODER_DEBUG_BIT_STATISTICS_SET(x) m_binDecoder.set(x);
#else
#define RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET(x)
#define RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET2(x,y)
#define RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE(x,s)
#define RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2(x,s,z)
#define RExt__DECODER_DEBUG_BIT_STATISTICS_SET(x)
#endif


void CABACReader::initCtxModels( Slice& slice )
{
  SliceType sliceType  = slice.getSliceType();
  int       qp         = slice.getSliceQp();
  if( slice.getPPS()->getCabacInitPresentFlag() && slice.getCabacInitFlag() )
  {
    switch( sliceType )
    {
    case P_SLICE:           // change initialization table to B_SLICE initialization
      sliceType = B_SLICE;
      break;
    case B_SLICE:           // change initialization table to P_SLICE initialization
      sliceType = P_SLICE;
      break;
    default     :           // should not occur
      THROW( "Invalid slice type" );
      break;
    }
  }
  m_binDecoder.reset(qp, (int) sliceType);
  m_binDecoder.setBaseLevel(slice.getRiceBaseLevel());
  m_binDecoder.riceStatReset(slice.getSPS()->getBitDepth(ChannelType::LUMA),
                             slice.getSPS()->getSpsRangeExtension().getPersistentRiceAdaptationEnabledFlag());
}


//================================================================================
//  clause 7.3.8.1
//--------------------------------------------------------------------------------
//    bool  terminating_bit()
//    void  remaining_bytes( noTrailingBytesExpected )
//================================================================================

bool CABACReader::terminating_bit()
{
  if (m_binDecoder.decodeBinTrm())
  {
    m_binDecoder.finish();
#if RExt__DECODER_DEBUG_BIT_STATISTICS
    CodingStatistics::IncrementStatisticEP(STATS__TRAILING_BITS, m_bitstream->readOutTrailingBits(), 0);
#else
    m_bitstream->readOutTrailingBits();
#endif
    return true;
  }
  return false;
}

void CABACReader::remaining_bytes( bool noTrailingBytesExpected )
{
  if( noTrailingBytesExpected )
  {
    CHECK(0 != m_bitstream->getNumBitsLeft(), "Bits left when not supposed");
  }
  else
  {
    while (m_bitstream->getNumBitsLeft())
    {
      unsigned trailingNullByte = m_bitstream->readByte();
      if( trailingNullByte != 0 )
      {
        THROW( "Trailing byte should be '0', but has a value of " << std::hex << trailingNullByte << std::dec << "\n" );
      }
    }
  }
}

//================================================================================
//  clause 7.3.8.2
//--------------------------------------------------------------------------------
//    void  coding_tree_unit( cs, area, qpL, qpC, ctuRsAddr )
//================================================================================

void CABACReader::coding_tree_unit(CodingStructure &cs, const UnitArea &area, EnumArray<int, ChannelType> &qps,
                                   unsigned ctuRsAddr)
{
  CUCtx           cuCtx(qps[ChannelType::LUMA]);
  QTBTPartitioner partitioner;

  partitioner.initCtu(area, ChannelType::LUMA, *cs.slice);
  cs.treeType = partitioner.treeType = TREE_D;
  cs.modeType = partitioner.modeType = MODE_TYPE_ALL;


  sao( cs, ctuRsAddr );
  if (cs.sps->getALFEnabledFlag() && (cs.slice->getAlfEnabledFlag(COMPONENT_Y)))
  {
    const PreCalcValues& pcv = *cs.pcv;

    const int frameWidthInCtus = pcv.widthInCtus;

    const int ry = ctuRsAddr / frameWidthInCtus;
    const int rx = ctuRsAddr - ry * frameWidthInCtus;

    const Position pos(rx * cs.pcv->maxCUWidth, ry * cs.pcv->maxCUHeight);

    const uint32_t curSliceIdx = cs.slice->getIndependentSliceIdx();
    const TileIdx  curTileIdx  = cs.pps->getTileIdx(pos);

    const bool leftAvail =
      cs.getCURestricted(pos.offset(-(int) pcv.maxCUWidth, 0), pos, curSliceIdx, curTileIdx, ChannelType::LUMA)
      != nullptr;
    const bool aboveAvail =
      cs.getCURestricted(pos.offset(0, -(int) pcv.maxCUHeight), pos, curSliceIdx, curTileIdx, ChannelType::LUMA)
      != nullptr;

    const int leftCTUAddr  = leftAvail ? ctuRsAddr - 1 : -1;
    const int aboveCTUAddr = aboveAvail ? ctuRsAddr - frameWidthInCtus : -1;

    for( int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++ )
    {
      if (cs.slice->getAlfEnabledFlag((ComponentID)compIdx))
      {
        AlfMode *alfModes = cs.slice->getPic()->getAlfModes(compIdx);
        int ctx = 0;
        ctx += leftCTUAddr > -1 ? (alfModes[leftCTUAddr] != AlfMode::OFF ? 1 : 0) : 0;
        ctx += aboveCTUAddr > -1 ? (alfModes[aboveCTUAddr] != AlfMode::OFF ? 1 : 0) : 0;

        RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET(STATS__CABAC_BITS__ALF);
        const bool enabled = m_binDecoder.decodeBin(Ctx::alfCtbFlag(compIdx * 3 + ctx)) != 0;

        if (!enabled)
        {
          alfModes[ctuRsAddr] = AlfMode::OFF;
        }
        else
        {
          if (isLuma((ComponentID) compIdx))
          {
            readAlfCtuFilterIndex(cs, ctuRsAddr);
          }
          else
          {
            const int apsIdx = cs.slice->getAlfApsIdChroma();
            CHECK(cs.slice->getAlfAPSs()[apsIdx] == nullptr, "APS not initialized");
            const AlfParam &alfParam = cs.slice->getAlfAPSs()[apsIdx]->getAlfAPSParam();
            const int       numAlts  = alfParam.numAlternativesChroma;

            uint8_t decoded = 0;
            while (decoded < numAlts - 1 && m_binDecoder.decodeBin(Ctx::ctbAlfAlternative(compIdx - 1)))
            {
              ++ decoded;
            }

            alfModes[ctuRsAddr] = AlfMode::CHROMA0 + decoded;
          }
        }
      }
    }
  }
  if (cs.sps->getCCALFEnabledFlag())
  {
    for ( int compIdx = 1; compIdx < getNumberValidComponents( cs.pcv->chrFormat ); compIdx++ )
    {
      if (cs.slice->m_ccAlfFilterParam.ccAlfFilterEnabled[compIdx - 1])
      {
        const int filterCount = cs.slice->m_ccAlfFilterParam.ccAlfFilterCount[compIdx - 1];

        const int ry = ctuRsAddr / cs.pcv->widthInCtus;
        const int rx = ctuRsAddr % cs.pcv->widthInCtus;

        const Position lumaPos(rx * cs.pcv->maxCUWidth, ry * cs.pcv->maxCUHeight);

        ccAlfFilterControlIdc(cs, ComponentID(compIdx), ctuRsAddr, cs.slice->m_ccAlfFilterControl[compIdx - 1], lumaPos,
                              filterCount);
      }
    }
  }

  if (CS::isDualITree(cs) && isChromaEnabled(cs.pcv->chrFormat) && cs.pcv->maxCUWidth > 64)
  {
    QTBTPartitioner chromaPartitioner;
    chromaPartitioner.initCtu(area, ChannelType::CHROMA, *cs.slice);
    CUCtx cuCtxChroma(qps[ChannelType::CHROMA]);
    coding_tree(cs, partitioner, cuCtx, &chromaPartitioner, &cuCtxChroma);
    qps[ChannelType::LUMA]   = cuCtx.qp;
    qps[ChannelType::CHROMA] = cuCtxChroma.qp;
  }
  else
  {
    coding_tree(cs, partitioner, cuCtx);
    qps[ChannelType::LUMA] = cuCtx.qp;
    if (CS::isDualITree(cs) && isChromaEnabled(cs.pcv->chrFormat))
    {
      CUCtx cuCtxChroma(qps[ChannelType::CHROMA]);
      partitioner.initCtu(area, ChannelType::CHROMA, *cs.slice);
      coding_tree(cs, partitioner, cuCtxChroma);
      qps[ChannelType::CHROMA] = cuCtxChroma.qp;
    }
  }

  DTRACE_COND( ctuRsAddr == 0, g_trace_ctx, D_QP_PER_CTU, "\n%4d %2d", cs.picture->poc, cs.slice->getSliceQpBase() );
  DTRACE(g_trace_ctx, D_QP_PER_CTU, " %3d", qps[ChannelType::LUMA] - cs.slice->getSliceQpBase());
}

void CABACReader::readAlfCtuFilterIndex(CodingStructure& cs, unsigned ctuRsAddr)
{
  const int  numAps        = cs.slice->getNumAlfApsIdsLuma();
  const bool alfUseApsFlag = numAps > 0 && m_binDecoder.decodeBin(Ctx::alfUseApsFlag()) != 0;

  AlfMode m;
  if (alfUseApsFlag)
  {
    uint32_t alfLumaPrevFilterIdx = 0;
    if (numAps > 1)
    {
      xReadTruncBinCode(alfLumaPrevFilterIdx, numAps);
    }
    m = AlfMode::LUMA0 + alfLumaPrevFilterIdx;
  }
  else
  {
    uint32_t alfLumaFixedFilterIdx = 0;
    xReadTruncBinCode(alfLumaFixedFilterIdx, ALF_NUM_FIXED_FILTER_SETS);
    m = AlfMode::LUMA_FIXED0 + alfLumaFixedFilterIdx;
  }

  AlfMode *alfModes   = cs.slice->getPic()->getAlfModes(COMPONENT_Y);
  alfModes[ctuRsAddr] = m;
}

void CABACReader::ccAlfFilterControlIdc(CodingStructure &cs, const ComponentID compID, const int curIdx,
                                        uint8_t *filterControlIdc, Position lumaPos, int filterCount)
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__CROSS_COMPONENT_ALF_BLOCK_LEVEL_IDC );

  const Position leftLumaPos  = lumaPos.offset(-(int) cs.pcv->maxCUWidth, 0);
  const Position aboveLumaPos = lumaPos.offset(0, -(int) cs.pcv->maxCUWidth);

  const uint32_t curSliceIdx = cs.slice->getIndependentSliceIdx();
  const TileIdx  curTileIdx  = cs.pps->getTileIdx(lumaPos);

  const bool leftAvail =
    cs.getCURestricted(leftLumaPos, lumaPos, curSliceIdx, curTileIdx, ChannelType::LUMA) != nullptr;
  const bool aboveAvail =
    cs.getCURestricted(aboveLumaPos, lumaPos, curSliceIdx, curTileIdx, ChannelType::LUMA) != nullptr;

  int ctxt = 0;
  if (leftAvail)
  {
    ctxt += ( filterControlIdc[curIdx - 1] ) ? 1 : 0;
  }
  if (aboveAvail)
  {
    ctxt += ( filterControlIdc[curIdx - cs.pcv->widthInCtus] ) ? 1 : 0;
  }
  ctxt += ( compID == COMPONENT_Cr ) ? 3 : 0;

  int idcVal = m_binDecoder.decodeBin(Ctx::CcAlfFilterControlFlag(ctxt));
  if ( idcVal )
  {
    while ((idcVal != filterCount) && m_binDecoder.decodeBinEP())
    {
      idcVal++;
    }
  }
  filterControlIdc[curIdx] = idcVal;

  DTRACE(g_trace_ctx, D_SYNTAX, "ccAlfFilterControlIdc() compID=%d pos=(%d,%d) ctxt=%d, filterCount=%d, idcVal=%d\n",
         compID, lumaPos.x, lumaPos.y, ctxt, filterCount, idcVal);
}

//================================================================================
//  clause 7.3.8.3
//--------------------------------------------------------------------------------
//    void  sao( slice, ctuRsAddr )
//================================================================================

void CABACReader::sao( CodingStructure& cs, unsigned ctuRsAddr )
{
  const SPS &sps = *cs.sps;

  if( !sps.getSAOEnabledFlag() )
  {
    return;
  }

  const Slice &slice = *cs.slice;

  SAOBlkParam &saoCtuParams = cs.picture->getSAO()[ctuRsAddr];

  const bool sliceSaoLumaFlag = slice.getSaoEnabledFlag(ChannelType::LUMA);
  const bool sliceSaoChromaFlag =
    slice.getSaoEnabledFlag(ChannelType::CHROMA) && isChromaEnabled(sps.getChromaFormatIdc());

  saoCtuParams[COMPONENT_Y].modeIdc  = SAOMode::OFF;
  saoCtuParams[COMPONENT_Cb].modeIdc = SAOMode::OFF;
  saoCtuParams[COMPONENT_Cr].modeIdc = SAOMode::OFF;

  if (!sliceSaoLumaFlag && !sliceSaoChromaFlag)
  {
    return;
  }

  // merge
  const int frameWidthInCtus = cs.pcv->widthInCtus;

  const int ry = ctuRsAddr / frameWidthInCtus;
  const int rx = ctuRsAddr - ry * frameWidthInCtus;

  const Position  pos( rx * cs.pcv->maxCUWidth, ry * cs.pcv->maxCUHeight );
  const unsigned  curSliceIdx = cs.slice->getIndependentSliceIdx();

  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__SAO );

  auto mergeType = SAOModeMergeTypes::NONE;

  const TileIdx curTileIdx = cs.pps->getTileIdx(pos);

  if (cs.getCURestricted(pos.offset(-(int) cs.pcv->maxCUWidth, 0), pos, curSliceIdx, curTileIdx, ChannelType::LUMA))
  {
    // sao_merge_left_flag
    mergeType = m_binDecoder.decodeBin(Ctx::SaoMergeFlag()) ? SAOModeMergeTypes::LEFT : SAOModeMergeTypes::NONE;
  }

  if (mergeType == SAOModeMergeTypes::NONE
      && cs.getCURestricted(pos.offset(0, -(int) cs.pcv->maxCUHeight), pos, curSliceIdx, curTileIdx, ChannelType::LUMA))
  {
    // sao_merge_above_flag
    mergeType = m_binDecoder.decodeBin(Ctx::SaoMergeFlag()) ? SAOModeMergeTypes::ABOVE : SAOModeMergeTypes::NONE;
  }

  if (mergeType != SAOModeMergeTypes::NONE)
  {
    if (sliceSaoLumaFlag || sliceSaoChromaFlag)
    {
      saoCtuParams[COMPONENT_Y].modeIdc           = SAOMode::MERGE;
      saoCtuParams[COMPONENT_Y].typeIdc.mergeType = mergeType;
    }
    if (sliceSaoChromaFlag)
    {
      saoCtuParams[COMPONENT_Cb].modeIdc           = SAOMode::MERGE;
      saoCtuParams[COMPONENT_Cr].modeIdc           = SAOMode::MERGE;
      saoCtuParams[COMPONENT_Cb].typeIdc.mergeType = mergeType;
      saoCtuParams[COMPONENT_Cr].typeIdc.mergeType = mergeType;
    }
    return;
  }

  // explicit parameters
  ComponentID firstComp = sliceSaoLumaFlag ? COMPONENT_Y : COMPONENT_Cb;
  ComponentID lastComp  = sliceSaoChromaFlag ? COMPONENT_Cr : COMPONENT_Y;
  for( ComponentID compID = firstComp; compID <= lastComp; compID = ComponentID( compID + 1 ) )
  {
    SAOOffset &sao_pars = saoCtuParams[compID];

    // sao_type_idx_luma / sao_type_idx_chroma
    if( compID != COMPONENT_Cr )
    {
      if (m_binDecoder.decodeBin(Ctx::SaoTypeIdx()))
      {
        if (m_binDecoder.decodeBinEP())
        {
          // edge offset
          sao_pars.modeIdc         = SAOMode::NEW;
          sao_pars.typeIdc.newType = SAOModeNewTypes::START_EO;
        }
        else
        {
          // band offset
          sao_pars.modeIdc         = SAOMode::NEW;
          sao_pars.typeIdc.newType = SAOModeNewTypes::START_BO;
        }
      }
    }
    else //Cr, follow Cb SAO type
    {
      sao_pars.modeIdc = saoCtuParams[COMPONENT_Cb].modeIdc;
      sao_pars.typeIdc = saoCtuParams[COMPONENT_Cb].typeIdc;
    }
    if (sao_pars.modeIdc == SAOMode::OFF)
    {
      continue;
    }

    // sao_offset_abs
    int       offset[4];
    const int maxOffsetQVal = SampleAdaptiveOffset::getMaxOffsetQVal( sps.getBitDepth( toChannelType(compID) ) );

    offset[0] = (int) unary_max_eqprob(maxOffsetQVal);
    offset[1] = (int) unary_max_eqprob(maxOffsetQVal);
    offset[2] = (int) unary_max_eqprob(maxOffsetQVal);
    offset[3] = (int) unary_max_eqprob(maxOffsetQVal);

    // band offset mode
    if (sao_pars.typeIdc.newType == SAOModeNewTypes::START_BO)
    {
      // sao_offset_sign
      for( int k = 0; k < 4; k++ )
      {
        if (offset[k] && m_binDecoder.decodeBinEP())
        {
          offset[k] = -offset[k];
        }
      }
      // sao_band_position
      sao_pars.typeAuxInfo = m_binDecoder.decodeBinsEP(NUM_SAO_BO_CLASSES_LOG2);
      for( int k = 0; k < 4; k++ )
      {
        sao_pars.offset[ ( sao_pars.typeAuxInfo + k ) % MAX_NUM_SAO_CLASSES ] = offset[k];
      }
      continue;
    }

    // edge offset mode
    sao_pars.typeAuxInfo = 0;
    if( compID != COMPONENT_Cr )
    {
      // sao_eo_class_luma / sao_eo_class_chroma
      sao_pars.typeIdc.newType =
        SAOModeNewTypes(to_underlying(sao_pars.typeIdc.newType) + m_binDecoder.decodeBinsEP(NUM_SAO_EO_TYPES_LOG2));
    }
    else
    {
      sao_pars.typeIdc = saoCtuParams[COMPONENT_Cb].typeIdc;
    }
    sao_pars.offset[ SAO_CLASS_EO_FULL_VALLEY ] =  offset[0];
    sao_pars.offset[ SAO_CLASS_EO_HALF_VALLEY ] =  offset[1];
    sao_pars.offset[ SAO_CLASS_EO_PLAIN       ] =  0;
    sao_pars.offset[ SAO_CLASS_EO_HALF_PEAK   ] = -offset[2];
    sao_pars.offset[ SAO_CLASS_EO_FULL_PEAK   ] = -offset[3];
  }
}

//================================================================================
//  clause 7.3.8.4
//--------------------------------------------------------------------------------
//    void  coding_tree       ( cs, partitioner, cuCtx )
//    bool  split_cu_flag     ( cs, partitioner )
//    split split_cu_mode_mt  ( cs, partitioner )
//================================================================================

void CABACReader::coding_tree( CodingStructure& cs, Partitioner& partitioner, CUCtx& cuCtx, Partitioner* pPartitionerChroma, CUCtx* pCuCtxChroma)
{
  const PPS      &pps         = *cs.pps;
  const UnitArea &currArea    = partitioner.currArea();

  // Reset delta QP coding flag and ChromaQPAdjustemt coding flag
  //Note: do not reset qg at chroma CU
  if( pps.getUseDQP() && partitioner.currQgEnable() && !isChroma(partitioner.chType) )
  {
    cuCtx.qgStart    = true;
    cuCtx.isDQPCoded = false;
  }
  if( cs.slice->getUseChromaQpAdj() && partitioner.currQgChromaEnable() )
  {
    cuCtx.isChromaQpAdjCoded  = false;
    cs.chromaQpAdj = 0;
  }

  // Reset delta QP coding flag and ChromaQPAdjustemt coding flag
  if (CS::isDualITree(cs) && pPartitionerChroma != nullptr)
  {
    if (pps.getUseDQP() && pPartitionerChroma->currQgEnable())
    {
      pCuCtxChroma->qgStart    = true;
      pCuCtxChroma->isDQPCoded = false;
    }
    if (cs.slice->getUseChromaQpAdj() && pPartitionerChroma->currQgChromaEnable())
    {
      pCuCtxChroma->isChromaQpAdjCoded = false;
      cs.chromaQpAdj = 0;
    }
  }

  const PartSplit splitMode = split_cu_mode( cs, partitioner );

  CHECK( !partitioner.canSplit( splitMode, cs ), "Got an invalid split!" );

  if( splitMode != CU_DONT_SPLIT )
  {
    if (CS::isDualITree(cs) && pPartitionerChroma != nullptr
        && (partitioner.currArea().lwidth() >= 64 || partitioner.currArea().lheight() >= 64))
    {
      partitioner.splitCurrArea(CU_QUAD_SPLIT, cs);
      pPartitionerChroma->splitCurrArea(CU_QUAD_SPLIT, cs);
      bool beContinue     = true;
      bool lumaContinue   = true;
      bool chromaContinue = true;

      while (beContinue)
      {
        if (partitioner.currArea().lwidth() > 64 || partitioner.currArea().lheight() > 64)
        {
          if (cs.area.block(partitioner.chType).contains(partitioner.currArea().block(partitioner.chType).pos()))
          {
            coding_tree(cs, partitioner, cuCtx, pPartitionerChroma, pCuCtxChroma);
          }
          lumaContinue   = partitioner.nextPart(cs);
          chromaContinue = pPartitionerChroma->nextPart(cs);
          CHECK(lumaContinue != chromaContinue, "luma chroma partition should be matched");
          beContinue = lumaContinue;
        }
        else
        {
          // dual tree coding under 64x64 block
          if (cs.area.block(partitioner.chType).contains(partitioner.currArea().block(partitioner.chType).pos()))
          {
            coding_tree(cs, partitioner, cuCtx);
          }
          lumaContinue = partitioner.nextPart(cs);
          if (cs.area.block(pPartitionerChroma->chType)
                .contains(pPartitionerChroma->currArea().block(pPartitionerChroma->chType).pos()))
          {
            coding_tree(cs, *pPartitionerChroma, *pCuCtxChroma);
          }
          chromaContinue = pPartitionerChroma->nextPart(cs);
          CHECK(lumaContinue != chromaContinue, "luma chroma partition should be matched");
          beContinue = lumaContinue;
        }
      }
      partitioner.exitCurrSplit();
      pPartitionerChroma->exitCurrSplit();

      // cat the chroma CUs together
      CodingUnit *currentCu        = cs.getCU(partitioner.currArea().lumaPos(), ChannelType::LUMA);
      CodingUnit *nextCu           = nullptr;
      CodingUnit *tempLastLumaCu   = nullptr;
      CodingUnit *tempLastChromaCu = nullptr;
      ChannelType currentChType    = currentCu->chType;
      while (currentCu->next != nullptr)
      {
        nextCu = currentCu->next;
        if (currentChType != nextCu->chType && isLuma(currentChType))
        {
          tempLastLumaCu = currentCu;
          if (tempLastChromaCu != nullptr)   // swap
          {
            tempLastChromaCu->next = nextCu;
          }
        }
        else if (currentChType != nextCu->chType && currentChType == ChannelType::CHROMA)
        {
          tempLastChromaCu = currentCu;
          if (tempLastLumaCu != nullptr)   // swap
          {
            tempLastLumaCu->next = nextCu;
          }
        }
        currentCu     = nextCu;
        currentChType = currentCu->chType;
      }

      CodingUnit *chromaFirstCu = cs.getCU(pPartitionerChroma->currArea().chromaPos(), ChannelType::CHROMA);
      tempLastLumaCu->next      = chromaFirstCu;
    }
    else
    {
      const ModeType modeTypeParent = partitioner.modeType;
      cs.modeType = partitioner.modeType = mode_constraint(cs, partitioner, splitMode);   // change for child nodes
      // decide chroma split or not
      bool chromaNotSplit = modeTypeParent == MODE_TYPE_ALL && partitioner.modeType == MODE_TYPE_INTRA;
      CHECK(chromaNotSplit && partitioner.chType != ChannelType::LUMA, "chType must be luma");
      if (partitioner.treeType == TREE_D)
      {
        cs.treeType = partitioner.treeType = chromaNotSplit ? TREE_L : TREE_D;
      }
      partitioner.splitCurrArea( splitMode, cs );
      do
      {
        if (cs.area.block(partitioner.chType).contains(partitioner.currArea().block(partitioner.chType).pos()))
        {
          coding_tree( cs, partitioner, cuCtx );
        }
      } while( partitioner.nextPart( cs ) );

      partitioner.exitCurrSplit();
      if( chromaNotSplit )
      {
        CHECK(partitioner.chType != ChannelType::LUMA, "must be luma status");
        partitioner.chType = ChannelType::CHROMA;
        cs.treeType = partitioner.treeType = TREE_C;

        if (cs.picture->block(partitioner.chType).contains(partitioner.currArea().block(partitioner.chType).pos()))
        {
          coding_tree( cs, partitioner, cuCtx );
        }

        //recover treeType
        partitioner.chType = ChannelType::LUMA;
        cs.treeType = partitioner.treeType = TREE_D;
      }

      //recover ModeType
      cs.modeType = partitioner.modeType = modeTypeParent;
    }
    return;
  }

  CodingUnit& cu = cs.addCU( CS::getArea( cs, currArea, partitioner.chType ), partitioner.chType );

  partitioner.setCUData( cu );
  cu.slice   = cs.slice;
  cu.tileIdx = cs.pps->getTileIdx( currArea.lumaPos() );
  CHECK( cu.cs->treeType != partitioner.treeType, "treeType mismatch" );
  int lumaQPinLocalDualTree = -1;

  // Predict QP on start of quantization group
  if( cuCtx.qgStart )
  {
    cuCtx.qgStart = false;
    cuCtx.qp = CU::predictQP( cu, cuCtx.qp );
  }

  if (pps.getUseDQP() && partitioner.isSepTree(cs) && isChroma(cu.chType))
  {
    const Position chromaCentral(cu.chromaPos().offset(cu.chromaSize().width >> 1, cu.chromaSize().height >> 1));
    const Position lumaRefPos(chromaCentral.x << getComponentScaleX(COMPONENT_Cb, cu.chromaFormat), chromaCentral.y << getComponentScaleY(COMPONENT_Cb, cu.chromaFormat));
    //derive chroma qp, but the chroma qp is saved in cuCtx.qp which is used for luma qp
    //therefore, after decoding the chroma CU, the cuCtx.qp shall be recovered to luma qp in order to decode next luma cu qp
    const CodingUnit* colLumaCu = cs.getLumaCU( lumaRefPos );
    CHECK( colLumaCu == nullptr, "colLumaCU shall exist" );
    lumaQPinLocalDualTree = cuCtx.qp;

    if (colLumaCu)
    {
      cuCtx.qp = colLumaCu->qp;
    }
  }

  cu.qp = cuCtx.qp;                 //NOTE: CU QP can be changed by deltaQP signaling at TU level
  cu.chromaQpAdj = cs.chromaQpAdj;  //NOTE: CU chroma QP adjustment can be changed by adjustment signaling at TU level

  // coding unit

  coding_unit( cu, partitioner, cuCtx );
  //recover cuCtx.qp to luma qp after decoding the chroma CU
  if( pps.getUseDQP() && partitioner.isSepTree( cs ) && isChroma( cu.chType ) )
  {
    cuCtx.qp = lumaQPinLocalDualTree;
  }

  uint32_t compBegin;
  uint32_t numComp;
  bool jointPLT = false;
  if (cu.isSepTree())
  {
    if( cu.isLocalSepTree() )
    {
      compBegin = COMPONENT_Y;
      numComp   = getNumberValidComponents(cu.chromaFormat);
      jointPLT = true;
    }
    else
    {
      if (isLuma(partitioner.chType))
      {
        compBegin = COMPONENT_Y;
        numComp   = 1;
      }
      else
      {
        compBegin = COMPONENT_Cb;
        numComp   = 2;
      }
    }
  }
  else
  {
    compBegin = COMPONENT_Y;
    numComp   = getNumberValidComponents(cu.chromaFormat);
    jointPLT = true;
  }
  if (CU::isPLT(cu))
  {
    cs.reorderPrevPLT(cs.prevPLT, cu.curPLTSize, cu.curPLT, cu.reuseflag, compBegin, numComp, jointPLT);
  }
  if (cu.chType == ChannelType::CHROMA)
  {
    DTRACE( g_trace_ctx, D_QP, "[chroma CU]x=%d, y=%d, w=%d, h=%d, qp=%d\n", cu.Cb().x, cu.Cb().y, cu.Cb().width, cu.Cb().height, cu.qp );
  }
  else
  {
    DTRACE(g_trace_ctx, D_QP, "x=%d, y=%d, w=%d, h=%d, qp=%d\n", cu.Y().x, cu.Y().y, cu.Y().width, cu.Y().height,
           cu.qp);
  }
}

ModeType CABACReader::mode_constraint( CodingStructure& cs, Partitioner &partitioner, PartSplit splitMode )
{
  const int val = cs.signalModeCons(splitMode, partitioner, partitioner.modeType);
  if( val == LDT_MODE_TYPE_SIGNAL )
  {
    int ctxIdx = DeriveCtx::CtxModeConsFlag( cs, partitioner );
    RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2(STATS__CABAC_BITS__MODE_CONSTRAINT_FLAG,
                                                        partitioner.currArea().block(partitioner.chType).size(),
                                                        partitioner.chType);
    bool flag = m_binDecoder.decodeBin(Ctx::ModeConsFlag(ctxIdx));
    DTRACE( g_trace_ctx, D_SYNTAX, "mode_cons_flag() flag=%d\n", flag );
    return flag ? MODE_TYPE_INTRA : MODE_TYPE_INTER;
  }
  else if( val == LDT_MODE_TYPE_INFER )
  {
    return MODE_TYPE_INTRA;
  }
  else
  {
    return partitioner.modeType;
  }
}

PartSplit CABACReader::split_cu_mode( CodingStructure& cs, Partitioner &partitioner )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2(
    STATS__CABAC_BITS__SPLIT_FLAG, partitioner.currArea().block(partitioner.chType).size(), partitioner.chType);

  PartSplit mode = CU_DONT_SPLIT;

  bool canNo, canQt, canBh, canBv, canTh, canTv;
  partitioner.canSplit( cs, canNo, canQt, canBh, canBv, canTh, canTv );

  bool canSpl[6] = { canNo, canQt, canBh, canBv, canTh, canTv };

  unsigned ctxSplit = 0, ctxQtSplit = 0, ctxBttHV = 0, ctxBttH12 = 0, ctxBttV12;
  DeriveCtx::CtxSplit( cs, partitioner, ctxSplit, ctxQtSplit, ctxBttHV, ctxBttH12, ctxBttV12, canSpl );

  bool isSplit = canBh || canBv || canTh || canTv || canQt;

  if( canNo && isSplit )
  {
    isSplit = m_binDecoder.decodeBin(Ctx::SplitFlag(ctxSplit));
  }

  DTRACE( g_trace_ctx, D_SYNTAX, "split_cu_mode() ctx=%d split=%d\n", ctxSplit, isSplit );

  if( !isSplit )
  {
    return CU_DONT_SPLIT;
  }

  const bool canBtt = canBh || canBv || canTh || canTv;
  bool       isQt   = canQt;

  if( isQt && canBtt )
  {
    isQt = m_binDecoder.decodeBin(Ctx::SplitQtFlag(ctxQtSplit));
  }

  DTRACE( g_trace_ctx, D_SYNTAX, "split_cu_mode() ctx=%d qt=%d\n", ctxQtSplit, isQt );

  if( isQt )
  {
    return CU_QUAD_SPLIT;
  }

  const bool canHor = canBh || canTh;
  bool        isVer = canBv || canTv;

  if( isVer && canHor )
  {
    isVer = m_binDecoder.decodeBin(Ctx::SplitHvFlag(ctxBttHV));
  }

  const bool can14 = isVer ? canTv : canTh;
  bool        is12 = isVer ? canBv : canBh;

  if( is12 && can14 )
  {
    is12 = m_binDecoder.decodeBin(Ctx::Split12Flag(isVer ? ctxBttV12 : ctxBttH12));
  }

  if (isVer && is12)
  {
    mode = CU_VERT_SPLIT;
  }
  else if (isVer && !is12)
  {
    mode = CU_TRIV_SPLIT;
  }
  else if (!isVer && is12)
  {
    mode = CU_HORZ_SPLIT;
  }
  else
  {
    mode = CU_TRIH_SPLIT;
  }

  DTRACE( g_trace_ctx, D_SYNTAX, "split_cu_mode() ctxHv=%d ctx12=%d mode=%d\n", ctxBttHV, isVer ? ctxBttV12 : ctxBttH12, mode );

  return mode;
}

//================================================================================
//  clause 7.3.8.5
//--------------------------------------------------------------------------------
//    void  coding_unit               ( cu, partitioner, cuCtx )
//    void  cu_skip_flag              ( cu )
//    void  pred_mode                 ( cu )
//    void  part_mode                 ( cu )
//    void  cu_pred_data              ( pus )
//    void  cu_lic_flag               ( cu )
//    void  intra_luma_pred_modes     ( pus )
//    void  intra_chroma_pred_mode    ( pu )
//    void  cu_residual               ( cu, partitioner, cuCtx )
//    void  rqt_root_cbf              ( cu )
//    void  end_of_ctu                ( cu, cuCtx )
//================================================================================

void CABACReader::coding_unit( CodingUnit &cu, Partitioner &partitioner, CUCtx& cuCtx )
{
  CodingStructure& cs = *cu.cs;
  CHECK( cu.treeType != partitioner.treeType || cu.modeType != partitioner.modeType, "treeType or modeType mismatch" );
  DTRACE( g_trace_ctx, D_SYNTAX, "coding_unit() treeType=%d modeType=%d\n", cu.treeType, cu.modeType );
  PredictionUnit&    pu = cs.addPU(cu, partitioner.chType);
  // skip flag
  if ((!cs.slice->isIntra() || cs.slice->getSPS()->getIBCFlag()) && cu.Y().valid())
  {
    cu_skip_flag( cu );
  }

  // skip data
  if( cu.skip )
  {
    cu.colorTransform = false;
    cs.addEmptyTUs( partitioner );
    prediction_unit  ( pu );
    end_of_ctu( cu, cuCtx );
    return;
  }

  // prediction mode and partitioning data
  pred_mode ( cu );
  if (CU::isIntra(cu))
  {
    adaptive_color_transform(cu);
  }
  if (CU::isPLT(cu))
  {
    cu.colorTransform = false;
    cs.addTU(cu, partitioner.chType);
    if (cu.isSepTree())
    {
      if (isLuma(partitioner.chType))
      {
        cu_palette_info(cu, COMPONENT_Y, 1, cuCtx);
      }
      if (isChromaEnabled(cu.chromaFormat) && partitioner.chType == ChannelType::CHROMA)
      {
        cu_palette_info(cu, COMPONENT_Cb, 2, cuCtx);
      }
    }
    else
    {
      cu_palette_info(cu, COMPONENT_Y, getNumberValidComponents(cu.chromaFormat), cuCtx);
    }
    end_of_ctu(cu, cuCtx);
    return;
  }

  // --> create PUs

  // prediction data ( intra prediction modes / reference indexes + motion vectors )
  cu_pred_data( cu );

  // residual data ( coded block flags + transform coefficient levels )
  cu_residual( cu, partitioner, cuCtx );

  // check end of cu
  end_of_ctu( cu, cuCtx );
}

void CABACReader::cu_skip_flag( CodingUnit& cu )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__SKIP_FLAG );

  if ((cu.slice->isIntra() || cu.isConsIntra()) && cu.cs->slice->getSPS()->getIBCFlag())
  {
    cu.skip = false;
    cu.rootCbf = false;
    cu.predMode = MODE_INTRA;
    cu.mmvdSkip = false;
    if (CU::canUseIbc(cu))
    {
      unsigned ctxId = DeriveCtx::CtxSkipFlag(cu);
      unsigned skip  = m_binDecoder.decodeBin(Ctx::SkipFlag(ctxId));
      DTRACE(g_trace_ctx, D_SYNTAX, "cu_skip_flag() ctx=%d skip=%d\n", ctxId, skip ? 1 : 0);
      if (skip)
      {
        cu.skip     = true;
        cu.rootCbf  = false;
        cu.predMode = MODE_IBC;
        cu.mmvdSkip = false;
      }
    }
    return;
  }
  if ( !cu.cs->slice->getSPS()->getIBCFlag() && cu.lwidth() == 4 && cu.lheight() == 4 )
  {
    return;
  }
  if( !cu.cs->slice->getSPS()->getIBCFlag() && cu.isConsIntra() )
  {
    return;
  }
  unsigned ctxId  = DeriveCtx::CtxSkipFlag(cu);
  unsigned skip   = m_binDecoder.decodeBin(Ctx::SkipFlag(ctxId));

  DTRACE( g_trace_ctx, D_SYNTAX, "cu_skip_flag() ctx=%d skip=%d\n", ctxId, skip ? 1 : 0 );

  if (skip && cu.cs->slice->getSPS()->getIBCFlag())
  {
    // disable IBC mode larger than 64x64 and disable IBC when only allowing inter mode
    if (CU::canUseIbc(cu) && !cu.isConsInter())
    {
      if ( cu.lwidth() == 4 && cu.lheight() == 4 )
      {
        cu.skip     = true;
        cu.rootCbf  = false;
        cu.predMode = MODE_IBC;
        cu.mmvdSkip = false;
        return;
      }
      unsigned ctxidx = DeriveCtx::CtxIBCFlag(cu);
      if (m_binDecoder.decodeBin(Ctx::IBCFlag(ctxidx)))
      {
        cu.skip                      = true;
        cu.rootCbf                   = false;
        cu.predMode                  = MODE_IBC;
        cu.mmvdSkip                  = false;
        cu.firstPU->regularMergeFlag = false;
      }
      else
      {
        cu.predMode = MODE_INTER;
      }
      DTRACE(g_trace_ctx, D_SYNTAX, "ibc() ctx=%d cu.predMode=%d\n", ctxidx, cu.predMode);
    }
    else
    {
      cu.predMode = MODE_INTER;
    }
  }
  if ((skip && CU::isInter(cu) && cu.cs->slice->getSPS()->getIBCFlag()) ||
    (skip && !cu.cs->slice->getSPS()->getIBCFlag()))
  {
    cu.skip     = true;
    cu.rootCbf  = false;
    cu.predMode = MODE_INTER;
  }
}

void CABACReader::imv_mode( CodingUnit& cu )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__OTHER );

  if( !cu.cs->sps->getAMVREnabledFlag() )
  {
    return;
  }

  bool nonZeroMvd = CU::hasSubCUNonZeroMVd(cu);
  if (!nonZeroMvd)
  {
    return;
  }

  if ( cu.affine )
  {
    return;
  }

  const SPS *sps = cu.cs->sps;

  unsigned value = 0;
  if (CU::isIBC(cu))
  {
    value = 1;
  }
  else
  {
    value = m_binDecoder.decodeBin(Ctx::ImvFlag(0));
  }
  DTRACE( g_trace_ctx, D_SYNTAX, "imv_mode() value=%d ctx=%d\n", value, 0 );

  cu.imv = value;
  if( sps->getAMVREnabledFlag() && value )
  {
    if (!CU::isIBC(cu))
    {
      value = m_binDecoder.decodeBin(Ctx::ImvFlag(4));
      DTRACE(g_trace_ctx, D_SYNTAX, "imv_mode() value=%d ctx=%d\n", value, 4);
      cu.imv = value ? 1 : IMV_HPEL;
    }
    if (value)
    {
      value = m_binDecoder.decodeBin(Ctx::ImvFlag(1));
      DTRACE(g_trace_ctx, D_SYNTAX, "imv_mode() value=%d ctx=%d\n", value, 1);
      value++;
      cu.imv = value;
    }
  }

  DTRACE( g_trace_ctx, D_SYNTAX, "imv_mode() IMVFlag=%d\n", cu.imv );
}

void CABACReader::affine_amvr_mode( CodingUnit& cu )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__OTHER );

  const SPS* sps = cu.slice->getSPS();

  if( !sps->getAffineAmvrEnabledFlag() || !cu.affine )
  {
    return;
  }

  if ( !CU::hasSubCUNonZeroAffineMVd( cu ) )
  {
    return;
  }

  unsigned value = 0;
  value          = m_binDecoder.decodeBin(Ctx::ImvFlag(2));
  DTRACE( g_trace_ctx, D_SYNTAX, "affine_amvr_mode() value=%d ctx=%d\n", value, 2 );

  if( value )
  {
    value = m_binDecoder.decodeBin(Ctx::ImvFlag(3));
    DTRACE( g_trace_ctx, D_SYNTAX, "affine_amvr_mode() value=%d ctx=%d\n", value, 3 );
    value++;
  }

  cu.imv = value;
  DTRACE( g_trace_ctx, D_SYNTAX, "affine_amvr_mode() IMVFlag=%d\n", cu.imv );
}

void CABACReader::pred_mode( CodingUnit& cu )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__PRED_MODE );
  if (cu.cs->slice->getSPS()->getIBCFlag() && cu.chType != ChannelType::CHROMA)
  {
    if( cu.isConsInter() )
    {
      cu.predMode = MODE_INTER;
      return;
    }

    if ( cu.cs->slice->isIntra() || ( cu.lwidth() == 4 && cu.lheight() == 4 ) || cu.isConsIntra() )
    {
      cu.predMode = MODE_INTRA;
      if (CU::canUseIbc(cu))   // disable IBC mode larger than 64x64
      {
        unsigned ctxidx = DeriveCtx::CtxIBCFlag(cu);
        if (m_binDecoder.decodeBin(Ctx::IBCFlag(ctxidx)))
        {
          cu.predMode = MODE_IBC;
        }
      }
      if (!CU::isIBC(cu) && cu.cs->slice->getSPS()->getPLTMode() && cu.lwidth() <= 64 && cu.lheight() <= 64 && (cu.lumaSize().width * cu.lumaSize().height > 16) )
      {
        if (m_binDecoder.decodeBin(Ctx::PLTFlag(0)))
        {
          cu.predMode = MODE_PLT;
        }
      }
    }
    else
    {
      if (m_binDecoder.decodeBin(Ctx::PredMode(DeriveCtx::CtxPredModeFlag(cu))))
      {
        cu.predMode = MODE_INTRA;
        if (cu.cs->slice->getSPS()->getPLTMode() && cu.lwidth() <= 64 && cu.lheight() <= 64 && (cu.lumaSize().width * cu.lumaSize().height > 16) )
        {
          if (m_binDecoder.decodeBin(Ctx::PLTFlag(0)))
          {
            cu.predMode = MODE_PLT;
          }
        }
      }
      else
      {
        cu.predMode = MODE_INTER;
        if (CU::canUseIbc(cu))   // disable IBC mode larger than 64x64
        {
          unsigned ctxidx = DeriveCtx::CtxIBCFlag(cu);
          if (m_binDecoder.decodeBin(Ctx::IBCFlag(ctxidx)))
          {
            cu.predMode = MODE_IBC;
          }
        }
      }
    }
  }
  else
  {
    if( cu.isConsInter() )
    {
      cu.predMode = MODE_INTER;
      return;
    }

    if ( cu.cs->slice->isIntra() || (cu.lwidth() == 4 && cu.lheight() == 4) || cu.isConsIntra() )
    {
      cu.predMode = MODE_INTRA;
      if (cu.cs->slice->getSPS()->getPLTMode() && cu.lwidth() <= 64 && cu.lheight() <= 64 && ( ( (!isLuma(cu.chType)) && (cu.chromaSize().width * cu.chromaSize().height > 16) ) || ((isLuma(cu.chType)) && ((cu.lumaSize().width * cu.lumaSize().height) > 16 ) )  ) && (!cu.isLocalSepTree() || isLuma(cu.chType)  )  )
      {
        if (m_binDecoder.decodeBin(Ctx::PLTFlag(0)))
        {
          cu.predMode = MODE_PLT;
        }
      }
    }
    else
    {
      cu.predMode = m_binDecoder.decodeBin(Ctx::PredMode(DeriveCtx::CtxPredModeFlag(cu))) ? MODE_INTRA : MODE_INTER;
      if (CU::isIntra(cu) && cu.cs->slice->getSPS()->getPLTMode() && cu.lwidth() <= 64 &&  cu.lheight() <= 64 && ( ( (!isLuma(cu.chType)) && (cu.chromaSize().width * cu.chromaSize().height > 16) ) || ((isLuma(cu.chType)) && ((cu.lumaSize().width * cu.lumaSize().height) > 16 ) )  ) && (!cu.isLocalSepTree() || isLuma(cu.chType)  )  )
      {
        if (m_binDecoder.decodeBin(Ctx::PLTFlag(0)))
        {
          cu.predMode = MODE_PLT;
        }
      }
    }
  }
}

void CABACReader::bdpcm_mode( CodingUnit& cu, const ComponentID compID )
{
  if (!CU::bdpcmAllowed(cu, compID))
  {
    if (isLuma(compID))
    {
      cu.bdpcmMode = BdpcmMode::NONE;
      if (!CS::isDualITree(*cu.cs))
      {
        cu.bdpcmModeChroma = BdpcmMode::NONE;
      }
    }
    else
    {
      cu.bdpcmModeChroma = BdpcmMode::NONE;
    }
    return;
  }

  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2( STATS__CABAC_BITS__BDPCM_MODE, cu.block(compID).lumaSize(), compID );

  BdpcmMode bdpcmMode;
  unsigned ctxId = isLuma( compID ) ? 0 : 2;
  if (m_binDecoder.decodeBin(Ctx::BDPCMMode(ctxId)))
  {
    bdpcmMode = m_binDecoder.decodeBin(Ctx::BDPCMMode(ctxId + 1)) ? BdpcmMode::VER : BdpcmMode::HOR;
  }
  else
  {
    bdpcmMode = BdpcmMode::NONE;
  }
  if (isLuma(compID))
  {
    cu.bdpcmMode = bdpcmMode;
  }
  else
  {
    cu.bdpcmModeChroma = bdpcmMode;
  }
  if (isLuma(compID))
  {
    DTRACE(g_trace_ctx, D_SYNTAX, "bdpcm_mode(%d) x=%d, y=%d, w=%d, h=%d, bdpcm=%d\n", ChannelType::LUMA,
           cu.lumaPos().x, cu.lumaPos().y, cu.lwidth(), cu.lheight(), cu.bdpcmMode);
  }
  else
  {
    DTRACE(g_trace_ctx, D_SYNTAX, "bdpcm_mode(%d) x=%d, y=%d, w=%d, h=%d, bdpcm=%d\n", ChannelType::CHROMA,
           cu.chromaPos().x, cu.chromaPos().y, cu.chromaSize().width, cu.chromaSize().height, cu.bdpcmModeChroma);
  }
}

void CABACReader::cu_pred_data( CodingUnit &cu )
{
  if( CU::isIntra( cu ) )
  {
    if( cu.Y().valid() )
	{
      bdpcm_mode(cu, COMPONENT_Y );
    }
    intra_luma_pred_modes( cu );
    if( ( !cu.Y().valid() || (!cu.isSepTree() && cu.Y().valid() ) ) && isChromaEnabled(cu.chromaFormat) )
    {
      bdpcm_mode(cu, ComponentID(ChannelType::CHROMA));
    }
    intra_chroma_pred_modes( cu );
    return;
  }
  if (!cu.Y().valid()) // dual tree chroma CU
  {
    cu.predMode = MODE_IBC;
    return;
  }

  for( auto &pu : CU::traversePUs( cu ) )
  {
    prediction_unit( pu );
  }

  imv_mode   ( cu );
  affine_amvr_mode( cu );
  cu_bcw_flag( cu );
}

void CABACReader::cu_bcw_flag(CodingUnit& cu)
{
  if(!CU::isBcwIdxCoded(cu))
  {
    return;
  }

  CHECK(!(BCW_NUM > 1 && (BCW_NUM == 2 || (BCW_NUM & 0x01) == 1)), " !( BCW_NUM > 1 && ( BCW_NUM == 2 || ( BCW_NUM & 0x01 ) == 1 ) ) ");

  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET(STATS__CABAC_BITS__BCW_IDX);

  uint32_t idx = 0;

  uint32_t symbol = m_binDecoder.decodeBin(Ctx::bcwIdx(0));

  int32_t numBcw = (cu.slice->getCheckLDC()) ? 5 : 3;
  if(symbol == 1)
  {
    uint32_t prefixNumBits = numBcw - 2;
    uint32_t step = 1;

    idx = 1;

    for(int ui = 0; ui < prefixNumBits; ++ui)
    {
      symbol = m_binDecoder.decodeBinEP();
      if (symbol == 0)
      {
        break;
      }
      idx += step;
    }
  }

  cu.bcwIdx = (uint8_t) g_BcwParsingOrder[idx];

  DTRACE(g_trace_ctx, D_SYNTAX, "cu_bcw_flag() bcw_idx=%d\n", cu.bcwIdx ? 1 : 0);
}

void CABACReader::xReadTruncBinCode(uint32_t &symbol, uint32_t numSymbols)
{
  const int thresh = floorLog2(numSymbols);
  const int val    = 1 << thresh;
  const int b      = numSymbols - val;

  symbol = m_binDecoder.decodeBinsEP(thresh);
  if (symbol >= val - b)
  {
    symbol = 2 * symbol - (val - b) + m_binDecoder.decodeBinEP();
  }
}

void CABACReader::extend_ref_line(CodingUnit& cu)
{
  if (!cu.Y().valid() || !CU::isIntra(cu) || !isLuma(cu.chType) || cu.bdpcmMode != BdpcmMode::NONE)
  {
    cu.firstPU->multiRefIdx = 0;
    return;
  }
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET(STATS__CABAC_BITS__MULTI_REF_LINE);

  const int numBlocks = CU::getNumPUs(cu);
  PredictionUnit* pu = cu.firstPU;

  for (int k = 0; k < numBlocks; k++)
  {
    if( !cu.cs->sps->getUseMRL() )
    {
      pu->multiRefIdx = 0;
      pu = pu->next;
      continue;
    }
    bool isFirstLineOfCtu = (((cu.block(COMPONENT_Y).y)&((cu.cs->sps)->getMaxCUWidth() - 1)) == 0);
    if (isFirstLineOfCtu)
    {
      pu->multiRefIdx = 0;
      continue;
    }
    int multiRefIdx = 0;

    if (MRL_NUM_REF_LINES > 1)
    {
      multiRefIdx =
        m_binDecoder.decodeBin(Ctx::MultiRefLineIdx(0)) == 1 ? MULTI_REF_LINE_IDX[1] : MULTI_REF_LINE_IDX[0];
      if (MRL_NUM_REF_LINES > 2 && multiRefIdx != MULTI_REF_LINE_IDX[0])
      {
        multiRefIdx =
          m_binDecoder.decodeBin(Ctx::MultiRefLineIdx(1)) == 1 ? MULTI_REF_LINE_IDX[2] : MULTI_REF_LINE_IDX[1];
      }
    }
    pu->multiRefIdx = multiRefIdx;
    pu = pu->next;
  }
}

void CABACReader::intra_luma_pred_modes( CodingUnit &cu )
{
  if( !cu.Y().valid() )
  {
    return;
  }

  if (cu.bdpcmMode != BdpcmMode::NONE)
  {
    cu.firstPU->intraDir[ChannelType::LUMA] = cu.bdpcmMode == BdpcmMode::VER ? VER_IDX : HOR_IDX;
    return;
  }

  mip_flag(cu);
  if (cu.mipFlag)
  {
    mip_pred_modes(cu);
    return;
  }
  extend_ref_line( cu );
  isp_mode( cu );

  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2(STATS__CABAC_BITS__INTRA_DIR_ANG, cu.lumaSize(),
                                                      ChannelType::LUMA);

  // prev_intra_luma_pred_flag
  int numBlocks = CU::getNumPUs( cu );
  int mpmFlag[4];
  for( int k = 0; k < numBlocks; k++ )
  {
    CHECK(numBlocks != 1, "not supported yet");
    if ( cu.firstPU->multiRefIdx )
    {
      mpmFlag[0] = true;
    }
    else
    {
      mpmFlag[k] = m_binDecoder.decodeBin(Ctx::IntraLumaMpmFlag());
    }
  }

  PredictionUnit *pu = cu.firstPU;

  unsigned mpm_pred[NUM_MOST_PROBABLE_MODES];  // mpm_idx / rem_intra_luma_pred_mode
  for( int k = 0; k < numBlocks; k++ )
  {
    PU::getIntraMPMs( *pu, mpm_pred );

    if( mpmFlag[k] )
    {
      uint32_t ipred_idx = 0;
      {
        unsigned ctx = (pu->cu->ispMode == ISPType::NONE ? 1 : 0);
        if (pu->multiRefIdx == 0)
        {
          ipred_idx = m_binDecoder.decodeBin(Ctx::IntraLumaPlanarFlag(ctx));
        }
        else
        {
          ipred_idx = 1;
        }
        if( ipred_idx )
        {
          ipred_idx += m_binDecoder.decodeBinEP();
        }
        if (ipred_idx > 1)
        {
          ipred_idx += m_binDecoder.decodeBinEP();
        }
        if (ipred_idx > 2)
        {
          ipred_idx += m_binDecoder.decodeBinEP();
        }
        if (ipred_idx > 3)
        {
          ipred_idx += m_binDecoder.decodeBinEP();
        }
      }
      pu->intraDir[ChannelType::LUMA] = mpm_pred[ipred_idx];
    }
    else
    {
      unsigned ipred_mode = 0;

      xReadTruncBinCode(ipred_mode, NUM_LUMA_MODE - NUM_MOST_PROBABLE_MODES);
      //postponed sorting of MPMs (only in remaining branch)
      std::sort( mpm_pred, mpm_pred + NUM_MOST_PROBABLE_MODES );

      for( uint32_t i = 0; i < NUM_MOST_PROBABLE_MODES; i++ )
      {
        ipred_mode += (ipred_mode >= mpm_pred[i]);
      }

      pu->intraDir[ChannelType::LUMA] = ipred_mode;
    }

    DTRACE(g_trace_ctx, D_SYNTAX, "intra_luma_pred_modes() idx=%d pos=(%d,%d) mode=%d\n", k, pu->lumaPos().x,
           pu->lumaPos().y, pu->intraDir[ChannelType::LUMA]);
    pu = pu->next;
  }
}

void CABACReader::intra_chroma_pred_modes( CodingUnit& cu )
{
  if (!isChromaEnabled(cu.chromaFormat) || (cu.isSepTree() && isLuma(cu.chType)))
  {
    return;
  }

  if (cu.bdpcmModeChroma != BdpcmMode::NONE)
  {
    cu.firstPU->intraDir[ChannelType::CHROMA] = cu.bdpcmModeChroma == BdpcmMode::VER ? VER_IDX : HOR_IDX;
    return;
  }
  PredictionUnit *pu = cu.firstPU;

  CHECK(pu->cu != &cu, "Inconsistent PU-CU mapping");
  intra_chroma_pred_mode(*pu);
}

bool CABACReader::intra_chroma_lmc_mode(PredictionUnit& pu)
{
  int lmModeList[10];
  PU::getLMSymbolList(pu, lmModeList);

  int symbol = m_binDecoder.decodeBin(Ctx::CclmModeIdx(0));

  if (symbol == 0)
  {
    pu.intraDir[ChannelType::CHROMA] = lmModeList[symbol];
    CHECK(pu.intraDir[ChannelType::CHROMA] != LM_CHROMA_IDX, "should be LM_CHROMA");
  }
  else
  {
    symbol += m_binDecoder.decodeBinEP();
    pu.intraDir[ChannelType::CHROMA] = lmModeList[symbol];
  }
  return true; //it will only enter this function for LMC modes, so always return true ;
}

void CABACReader::intra_chroma_pred_mode(PredictionUnit& pu)
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2(STATS__CABAC_BITS__INTRA_DIR_ANG,
                                                      pu.cu->block(pu.chType).lumaSize(), ChannelType::CHROMA);
  if (pu.cu->colorTransform)
  {
    pu.intraDir[ChannelType::CHROMA] = DM_CHROMA_IDX;
    return;
  }

  // LM chroma mode

  if (pu.cs->sps->getUseLMChroma() && pu.cu->checkCCLMAllowed())
  {
    bool isLMCMode = m_binDecoder.decodeBin(Ctx::CclmModeFlag(0)) ? true : false;
    if (isLMCMode)
    {
      intra_chroma_lmc_mode(pu);
      return;
    }
  }

  if (m_binDecoder.decodeBin(Ctx::IntraChromaPredMode(0)) == 0)
  {
    pu.intraDir[ChannelType::CHROMA] = DM_CHROMA_IDX;
    return;
  }

  unsigned candId = m_binDecoder.decodeBinsEP(2);

  unsigned chromaCandModes[NUM_CHROMA_MODE];
  PU::getIntraChromaCandModes(pu, chromaCandModes);

  CHECK(candId >= NUM_CHROMA_MODE, "Chroma prediction mode index out of bounds");
  CHECK(PU::isLMCMode(chromaCandModes[candId]), "The intra dir cannot be LM_CHROMA for this path");
  CHECK(chromaCandModes[candId] == DM_CHROMA_IDX, "The intra dir cannot be DM_CHROMA for this path");

  pu.intraDir[ChannelType::CHROMA] = chromaCandModes[candId];
}

void CABACReader::cu_residual( CodingUnit& cu, Partitioner &partitioner, CUCtx& cuCtx )
{
  if (!CU::isIntra(cu))
  {
    PredictionUnit& pu = *cu.firstPU;
    if( !pu.mergeFlag )
    {
      rqt_root_cbf( cu );
    }
    else
    {
      cu.rootCbf = true;
    }
    if( cu.rootCbf )
    {
      sbt_mode( cu );
    }
    if( !cu.rootCbf )
    {
      cu.colorTransform = false;
      cu.cs->addEmptyTUs( partitioner );
      return;
    }
  }

  if (CU::isInter(cu) || CU::isIBC(cu))
  {
    adaptive_color_transform(cu);
  }

  cuCtx.violatesLfnstConstrained.fill(false);
  cuCtx.lfnstLastScanPos                              = false;
  cuCtx.violatesMtsCoeffConstraint                    = false;
  cuCtx.mtsLastScanPos                                = false;

  ChromaCbfs chromaCbfs;
  if (cu.ispMode != ISPType::NONE && isLuma(partitioner.chType))
  {
    TUIntraSubPartitioner subTuPartitioner( partitioner );
    transform_tree( *cu.cs, subTuPartitioner, cuCtx, CU::getISPType(cu, getFirstComponentOfChannel(partitioner.chType)), 0 );
  }
  else
  {
    transform_tree( *cu.cs, partitioner, cuCtx             );
  }

  residual_lfnst_mode( cu, cuCtx );
  mts_idx            ( cu, cuCtx );
}

void CABACReader::rqt_root_cbf( CodingUnit& cu )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__QT_ROOT_CBF );

  cu.rootCbf = (m_binDecoder.decodeBin(Ctx::QtRootCbf()));

  DTRACE( g_trace_ctx, D_SYNTAX, "rqt_root_cbf() ctx=0 root_cbf=%d pos=(%d,%d)\n", cu.rootCbf ? 1 : 0, cu.lumaPos().x, cu.lumaPos().y );
}

void CABACReader::adaptive_color_transform(CodingUnit& cu)
{
  if (!cu.slice->getSPS()->getUseColorTrans())
  {
    return;
  }

  if (cu.isSepTree())
  {
    return;
  }

  if (CU::isInter(cu) || CU::isIBC(cu) || CU::isIntra(cu))
  {
    RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__ACT );
    cu.colorTransform = (m_binDecoder.decodeBin(Ctx::ACTFlag()));
  }
}

void CABACReader::sbt_mode( CodingUnit& cu )
{
  const uint8_t sbtAllowed = cu.checkAllowedSbt();
  if( !sbtAllowed )
  {
    return;
  }

  SizeType cuWidth = cu.lwidth();
  SizeType cuHeight = cu.lheight();

  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__SBT_MODE );
  //bin - flag
  uint8_t ctxIdx = ( cuWidth * cuHeight <= 256 ) ? 1 : 0;
  bool    sbtFlag = m_binDecoder.decodeBin(Ctx::SbtFlag(ctxIdx));
  if( !sbtFlag )
  {
    return;
  }

  uint8_t sbtVerHalfAllow = CU::targetSbtAllowed( SBT_VER_HALF, sbtAllowed );
  uint8_t sbtHorHalfAllow = CU::targetSbtAllowed( SBT_HOR_HALF, sbtAllowed );
  uint8_t sbtVerQuadAllow = CU::targetSbtAllowed( SBT_VER_QUAD, sbtAllowed );
  uint8_t sbtHorQuadAllow = CU::targetSbtAllowed( SBT_HOR_QUAD, sbtAllowed );

  //bin - type
  bool sbtQuadFlag = false;
  if( ( sbtHorHalfAllow || sbtVerHalfAllow ) && ( sbtHorQuadAllow || sbtVerQuadAllow ) )
  {
    sbtQuadFlag = m_binDecoder.decodeBin(Ctx::SbtQuadFlag(0));
  }
  else
  {
    sbtQuadFlag = 0;
  }

  //bin - dir
  bool sbtHorFlag = false;
  if( ( sbtQuadFlag && sbtVerQuadAllow && sbtHorQuadAllow ) || ( !sbtQuadFlag && sbtVerHalfAllow && sbtHorHalfAllow ) ) //both direction allowed
  {
    uint8_t ctxIdx = ( cuWidth == cuHeight ) ? 0 : ( cuWidth < cuHeight ? 1 : 2 );
    sbtHorFlag     = m_binDecoder.decodeBin(Ctx::SbtHorFlag(ctxIdx));
  }
  else
  {
    sbtHorFlag = ( sbtQuadFlag && sbtHorQuadAllow ) || ( !sbtQuadFlag && sbtHorHalfAllow );
  }
  cu.setSbtIdx( sbtHorFlag ? ( sbtQuadFlag ? SBT_HOR_QUAD : SBT_HOR_HALF ) : ( sbtQuadFlag ? SBT_VER_QUAD : SBT_VER_HALF ) );

  //bin - pos
  bool sbtPosFlag = m_binDecoder.decodeBin(Ctx::SbtPosFlag(0));
  cu.setSbtPos( sbtPosFlag ? SBT_POS1 : SBT_POS0 );

  DTRACE( g_trace_ctx, D_SYNTAX, "sbt_mode() pos=(%d,%d) sbtInfo=%d\n", cu.lx(), cu.ly(), (int)cu.sbtInfo );
}


void CABACReader::end_of_ctu( CodingUnit& cu, CUCtx& cuCtx )
{
  const Position rbPos =
    recalcPosition(cu.chromaFormat, cu.chType, ChannelType::LUMA, cu.block(cu.chType).bottomRight().offset(1, 1));

  if (((rbPos.x & cu.cs->pcv->maxCUWidthMask) == 0 || rbPos.x == cu.cs->pps->getPicWidthInLumaSamples())
      && ((rbPos.y & cu.cs->pcv->maxCUHeightMask) == 0 || rbPos.y == cu.cs->pps->getPicHeightInLumaSamples())
      && (!cu.isSepTree() || !isChromaEnabled(cu.chromaFormat) || isChroma(cu.chType)))
  {
    cuCtx.isDQPCoded = ( cu.cs->pps->getUseDQP() && !cuCtx.isDQPCoded );
  }
}

void CABACReader::cu_palette_info(CodingUnit& cu, ComponentID compBegin, uint32_t numComp, CUCtx& cuCtx)
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__PLT_MODE );

  const SPS&      sps = *(cu.cs->sps);
  TransformUnit&   tu = *cu.firstTU;
  int curPLTidx = 0;

  if( cu.isLocalSepTree() )
  {
    cu.cs->prevPLT.curPLTSize[compBegin] = cu.cs->prevPLT.curPLTSize[COMPONENT_Y];
  }
  cu.lastPLTSize[compBegin] = cu.cs->prevPLT.curPLTSize[compBegin];

  int maxPltSize = cu.isSepTree() ? MAXPLTSIZE_DUALTREE : MAXPLTSIZE;

  if (cu.lastPLTSize[compBegin])
  {
    xDecodePLTPredIndicator(cu, maxPltSize, compBegin);
  }

  for (int idx = 0; idx < cu.lastPLTSize[compBegin]; idx++)
  {
    if (cu.reuseflag[compBegin][idx])
    {
      if( cu.isLocalSepTree() )
      {
        for( int comp = COMPONENT_Y; comp < MAX_NUM_COMPONENT; comp++ )
        {
          cu.curPLT[comp][curPLTidx] = cu.cs->prevPLT.curPLT[comp][idx];
        }
      }
      else
      {
        for (int comp = compBegin; comp < (compBegin + numComp); comp++)
        {
          cu.curPLT[comp][curPLTidx] = cu.cs->prevPLT.curPLT[comp][idx];
        }
      }
      curPLTidx++;
    }
  }

  uint32_t recievedPLTnum = 0;
  if (curPLTidx < maxPltSize)
  {
    recievedPLTnum = exp_golomb_eqprob(0);
  }

  cu.curPLTSize[compBegin] = curPLTidx + recievedPLTnum;
  if( cu.isLocalSepTree() )
    cu.curPLTSize[COMPONENT_Y] = cu.curPLTSize[compBegin];
  for (int comp = compBegin; comp < (compBegin + numComp); comp++)
  {
    for (int idx = curPLTidx; idx < cu.curPLTSize[compBegin]; idx++)
    {
      ComponentID compID = (ComponentID)comp;
      const int  channelBitDepth = sps.getBitDepth(toChannelType(compID));
      cu.curPLT[compID][idx]      = m_binDecoder.decodeBinsEP(channelBitDepth);
      if( cu.isLocalSepTree() )
      {
        if( isLuma( cu.chType ) )
        {
          cu.curPLT[COMPONENT_Cb][idx] = 1 << (cu.cs->sps->getBitDepth(ChannelType::CHROMA) - 1);
          cu.curPLT[COMPONENT_Cr][idx] = 1 << (cu.cs->sps->getBitDepth(ChannelType::CHROMA) - 1);
        }
        else
        {
          cu.curPLT[COMPONENT_Y][idx] = 1 << (cu.cs->sps->getBitDepth(ChannelType::LUMA) - 1);
        }
      }
    }
  }

  cu.useEscape[compBegin] = true;
  if (cu.curPLTSize[compBegin] > 0)
  {
    uint32_t escCode = 0;
    escCode                 = m_binDecoder.decodeBinEP();
    cu.useEscape[compBegin] = (escCode != 0);
  }
  uint32_t    indexMaxSize = cu.useEscape[compBegin] ? (cu.curPLTSize[compBegin] + 1) : cu.curPLTSize[compBegin];
  //encode index map
  uint32_t    height = cu.block(compBegin).height;
  uint32_t    width = cu.block(compBegin).width;

  uint32_t total = height * width;
  if (indexMaxSize > 1)
  {
    parseScanRotationModeFlag(cu, compBegin);
  }
  else
  {
    cu.useRotation[compBegin] = false;
  }

  if (cu.useEscape[compBegin] && cu.cs->pps->getUseDQP() && !cuCtx.isDQPCoded)
  {
    if (!cu.isSepTree() || isLuma(tu.chType))
    {
      cu_qp_delta(cu, cuCtx.qp, cu.qp);
      cuCtx.qp = cu.qp;
      cuCtx.isDQPCoded = true;
    }
  }
  if (cu.useEscape[compBegin] && cu.cs->slice->getUseChromaQpAdj() && !cuCtx.isChromaQpAdjCoded)
  {
    if (!cu.isSepTree() || isChroma(tu.chType))
    {
      cu_chroma_qp_offset(cu);
      cuCtx.isChromaQpAdjCoded = true;
    }
  }

  m_scanOrder = g_scanOrder[SCAN_UNGROUPED][(cu.useRotation[compBegin]) ? CoeffScanType::TRAV_VER : CoeffScanType::TRAV_HOR][gp_sizeIdxInfo->idxFrom(width)][gp_sizeIdxInfo->idxFrom(height)];
  uint32_t prevRunPos = 0;
  auto     prevRunType = PLTRunMode::INDEX;
  for (int subSetId = 0; subSetId <= (total - 1) >> LOG2_PALETTE_CG_SIZE; subSetId++)
  {
    cuPaletteSubblockInfo(cu, compBegin, numComp, subSetId, prevRunPos, prevRunType);
  }
  CHECK(cu.curPLTSize[compBegin] > maxPltSize, " Current palette size is larger than maximum palette size");
}

void CABACReader::cuPaletteSubblockInfo(CodingUnit& cu, ComponentID compBegin, uint32_t numComp, int subSetId,
                                        uint32_t& prevRunPos, PLTRunMode& prevRunType)
{
  const SPS&      sps = *(cu.cs->sps);
  TransformUnit&  tu = *cu.firstTU;
  PLTtypeBuf      runType      = tu.getrunType(toChannelType(compBegin));
  PelBuf          curPLTIdx = tu.getcurPLTIdx(compBegin);
  uint32_t        indexMaxSize = cu.useEscape[compBegin] ? (cu.curPLTSize[compBegin] + 1) : cu.curPLTSize[compBegin];
  uint32_t        totalPel = cu.block(compBegin).height*cu.block(compBegin).width;

  int minSubPos = subSetId << LOG2_PALETTE_CG_SIZE;
  int maxSubPos = minSubPos + (1 << LOG2_PALETTE_CG_SIZE);
  maxSubPos = (maxSubPos > totalPel) ? totalPel : maxSubPos; // if last position is out of the current CU size

  unsigned runCopyFlag[(1 << LOG2_PALETTE_CG_SIZE)];
  for (int i = 0; i < (1 << LOG2_PALETTE_CG_SIZE); i++)
  {
    runCopyFlag[i] = MAX_INT;
  }
  if (minSubPos == 0)
  {
    runCopyFlag[0] = 0;
  }

  // PLT runCopy flag and runType - context coded
  int curPos = minSubPos;
  for (; curPos < maxSubPos && indexMaxSize > 1; curPos++)
  {
    uint32_t posy = m_scanOrder[curPos].y;
    uint32_t posx = m_scanOrder[curPos].x;
    uint32_t posyprev = (curPos == 0) ? 0 : m_scanOrder[curPos - 1].y;
    uint32_t posxprev = (curPos == 0) ? 0 : m_scanOrder[curPos - 1].x;
    unsigned identityFlag = 1;

    const CtxSet& ctxSet = (prevRunType == PLTRunMode::INDEX) ? Ctx::IdxRunModel : Ctx::CopyRunModel;
    if (curPos > 0)
    {
      int dist = curPos - prevRunPos - 1;
      const unsigned  ctxId = DeriveCtx::CtxPltCopyFlag(prevRunType, dist);
      identityFlag          = m_binDecoder.decodeBin(ctxSet(ctxId));
      DTRACE(g_trace_ctx, D_SYNTAX, "plt_copy_flag() bin=%d ctx=%d\n", identityFlag, ctxId);
      runCopyFlag[curPos - minSubPos] = identityFlag;
    }

    if ( identityFlag == 0 || curPos == 0 )
    {
      if (((posy == 0) && !cu.useRotation[compBegin]) || ((posx == 0) && cu.useRotation[compBegin]))
      {
        runType.at(posx, posy) = PLTRunMode::INDEX;
      }
      else if (curPos != 0 && runType.at(posxprev, posyprev) == PLTRunMode::COPY)
      {
        runType.at(posx, posy) = PLTRunMode::INDEX;
      }
      else
      {
        runType.at(posx, posy) = m_binDecoder.decodeBin(Ctx::RunTypeFlag()) ? PLTRunMode::COPY : PLTRunMode::INDEX;
      }
      DTRACE(g_trace_ctx, D_SYNTAX, "plt_type_flag() bin=%d sp=%d\n", runType.at(posx, posy), curPos);
      prevRunType = runType.at(posx, posy);
      prevRunPos  = curPos;
    }
    else //assign run information
    {
      runType.at(posx, posy) = runType.at(posxprev, posyprev);
    }
  }

  // PLT index values - bypass coded
  uint32_t adjust;
  uint32_t symbol = 0;
  curPos = minSubPos;
  if (indexMaxSize > 1)
  {
    for (; curPos < maxSubPos; curPos++)
    {
      if (curPos > 0)
      {
        adjust = 1;
      }
      else
      {
        adjust = 0;
      }

      uint32_t posy = m_scanOrder[curPos].y;
      uint32_t posx = m_scanOrder[curPos].x;
      uint32_t posyprev = (curPos == 0) ? 0 : m_scanOrder[curPos - 1].y;
      uint32_t posxprev = (curPos == 0) ? 0 : m_scanOrder[curPos - 1].x;
      if (runCopyFlag[curPos - minSubPos] == 0 && runType.at(posx, posy) == PLTRunMode::INDEX)
      {
        xReadTruncBinCode(symbol, indexMaxSize - adjust);
        xAdjustPLTIndex(cu, symbol, curPos, curPLTIdx, runType, indexMaxSize, compBegin);
        DTRACE(g_trace_ctx, D_SYNTAX, "plt_idx_idc() value=%d sp=%d\n", curPLTIdx.at(posx, posy), curPos);
      }
      else if (runType.at(posx, posy) == PLTRunMode::INDEX)
      {
        curPLTIdx.at(posx, posy) = curPLTIdx.at(posxprev, posyprev);
      }
      else
      {
        curPLTIdx.at(posx, posy) = (cu.useRotation[compBegin]) ? curPLTIdx.at(posx - 1, posy) : curPLTIdx.at(posx, posy - 1);
      }
    }
  }
  else
  {
    for (; curPos < maxSubPos; curPos++)
    {
      uint32_t posy = m_scanOrder[curPos].y;
      uint32_t posx = m_scanOrder[curPos].x;
      uint32_t posyprev = (curPos == 0) ? 0 : m_scanOrder[curPos - 1].y;
      uint32_t posxprev = (curPos == 0) ? 0 : m_scanOrder[curPos - 1].x;
      runType.at(posx, posy) = PLTRunMode::INDEX;
      if (runCopyFlag[curPos - minSubPos] == 0 && runType.at(posx, posy) == PLTRunMode::INDEX)
      {
        curPLTIdx.at(posx, posy) = 0;
      }
      else
      {
        curPLTIdx.at(posx, posy) = curPLTIdx.at(posxprev, posyprev);
      }
    }
  }

  // Quantized escape colors - bypass coded
  uint32_t scaleX = getComponentScaleX(COMPONENT_Cb, sps.getChromaFormatIdc());
  uint32_t scaleY = getComponentScaleY(COMPONENT_Cb, sps.getChromaFormatIdc());
  for (int comp = compBegin; comp < (compBegin + numComp); comp++)
  {
    ComponentID compID = (ComponentID)comp;
    for (curPos = minSubPos; curPos < maxSubPos; curPos++)
    {
      uint32_t posy = m_scanOrder[curPos].y;
      uint32_t posx = m_scanOrder[curPos].x;
      if (curPLTIdx.at(posx, posy) == cu.curPLTSize[compBegin])
      {
          PLTescapeBuf    escapeValue = tu.getescapeValue((ComponentID)comp);
          if (compID == COMPONENT_Y || compBegin != COMPONENT_Y)
          {
            escapeValue.at(posx, posy) = exp_golomb_eqprob(5);
            assert(escapeValue.at(posx, posy) < (TCoeff(1) << (cu.cs->sps->getBitDepth(toChannelType((ComponentID)comp)) + 1)));
            DTRACE(g_trace_ctx, D_SYNTAX, "plt_escape_val() value=%d etype=%d sp=%d\n", escapeValue.at(posx, posy), comp, curPos);
          }
          if (compBegin == COMPONENT_Y && compID != COMPONENT_Y && posy % (1 << scaleY) == 0 && posx % (1 << scaleX) == 0)
          {
            uint32_t posxC = posx >> scaleX;
            uint32_t posyC = posy >> scaleY;
            escapeValue.at(posxC, posyC) = exp_golomb_eqprob(5);
            assert(escapeValue.at(posxC, posyC) < (TCoeff(1) << (cu.cs->sps->getBitDepth(toChannelType(compID)) + 1)));
            DTRACE(g_trace_ctx, D_SYNTAX, "plt_escape_val() value=%d etype=%d sp=%d\n", escapeValue.at(posx, posy), comp, curPos);
          }
      }
    }
  }
}

void CABACReader::parseScanRotationModeFlag(CodingUnit& cu, ComponentID compBegin)
{
  cu.useRotation[compBegin] = m_binDecoder.decodeBin(Ctx::RotationFlag());
}

void CABACReader::xDecodePLTPredIndicator(CodingUnit& cu, uint32_t maxPLTSize, ComponentID compBegin)
{
  uint32_t symbol, numPltPredicted = 0, idx = 0;

  symbol = exp_golomb_eqprob(0);

  if (symbol != 1)
  {
    while (idx < cu.lastPLTSize[compBegin] && numPltPredicted < maxPLTSize)
    {
      if (idx > 0)
      {
        symbol = exp_golomb_eqprob(0);
      }
      if (symbol == 1)
      {
        break;
      }

      if (symbol)
      {
        idx += symbol - 1;
      }
      cu.reuseflag[compBegin][idx] = 1;
      if( cu.isLocalSepTree() )
      {
        cu.reuseflag[COMPONENT_Y][idx] = 1;
      }
      numPltPredicted++;
      idx++;
    }
  }
}
void CABACReader::xAdjustPLTIndex(CodingUnit& cu, Pel curLevel, uint32_t idx, PelBuf& paletteIdx, PLTtypeBuf& paletteRunType, int maxSymbol, ComponentID compBegin)
{
  uint32_t symbol;
  int refLevel = MAX_INT;
  uint32_t posy = m_scanOrder[idx].y;
  uint32_t posx = m_scanOrder[idx].x;
  if (idx)
  {
    uint32_t prevposy = m_scanOrder[idx - 1].y;
    uint32_t prevposx = m_scanOrder[idx - 1].x;
    if (paletteRunType.at(prevposx, prevposy) == PLTRunMode::INDEX)
    {
      refLevel = paletteIdx.at(prevposx, prevposy);
      if (paletteIdx.at(prevposx, prevposy) == cu.curPLTSize[compBegin]) // escape
      {
        refLevel = maxSymbol - 1;
      }
    }
    else
    {
      if (cu.useRotation[compBegin])
      {
        assert(prevposx > 0);
        refLevel = paletteIdx.at(posx - 1, posy);
        if (paletteIdx.at(posx - 1, posy) == cu.curPLTSize[compBegin]) // escape mode
        {
          refLevel = maxSymbol - 1;
        }
      }
      else
      {
        assert(prevposy > 0);
        refLevel = paletteIdx.at(posx, posy - 1);
        if (paletteIdx.at(posx, posy - 1) == cu.curPLTSize[compBegin]) // escape mode
        {
          refLevel = maxSymbol - 1;
        }
      }
    }
    maxSymbol--;
  }
  symbol = curLevel;
  if (curLevel >= refLevel) // include escape mode
  {
    symbol++;
  }
  paletteIdx.at(posx, posy) = symbol;
}

//================================================================================
//  clause 7.3.8.6
//--------------------------------------------------------------------------------
//    void  prediction_unit ( pu, mrgCtx );
//    void  merge_flag      ( pu );
//    void  merge_data      ( pu, mrgCtx );
//    void  merge_idx       ( pu );
//    void  inter_pred_idc  ( pu );
//    void  ref_idx         ( pu, refList );
//    void  mvp_flag        ( pu, refList );
//================================================================================

void CABACReader::prediction_unit( PredictionUnit& pu )
{
  if( pu.cu->skip )
  {
    pu.mergeFlag = true;
  }
  else
  {
    merge_flag( pu );
  }
  if( pu.mergeFlag )
  {
    merge_data(pu);
  }
  else if (CU::isIBC(*pu.cu))
  {
    pu.interDir = 1;
    pu.cu->affine = false;
    pu.refIdx[REF_PIC_LIST_0] = IBC_REF_IDX;
    mvd_coding(pu.mvd[REF_PIC_LIST_0]);
    if (pu.cs->sps->getMaxNumIBCMergeCand() == 1)
    {
      pu.mvpIdx[REF_PIC_LIST_0] = 0;
    }
    else
    {
      mvp_flag(pu, REF_PIC_LIST_0);
    }
  }
  else
  {
    inter_pred_idc( pu );
    affine_flag   ( *pu.cu );
    smvd_mode( pu );

    if( pu.interDir != 2 /* PRED_L1 */ )
    {
      ref_idx     ( pu, REF_PIC_LIST_0 );
      if( pu.cu->affine )
      {
        for (int i = 0; i < pu.cu->getNumAffineMvs(); i++)
        {
          mvd_coding(pu.mvdAffi[REF_PIC_LIST_0][i]);
        }
      }
      else
      {
        mvd_coding( pu.mvd[REF_PIC_LIST_0] );
      }
      mvp_flag    ( pu, REF_PIC_LIST_0 );
    }

    if( pu.interDir != 1 /* PRED_L0 */ )
    {
      if ( pu.cu->smvdMode != 1 )
      {
        ref_idx(pu, REF_PIC_LIST_1);
        if (pu.cu->cs->picHeader->getMvdL1ZeroFlag() && pu.interDir == 3 /* PRED_BI */)
        {
          pu.mvd[REF_PIC_LIST_1]        = Mv();
          pu.mvdAffi[REF_PIC_LIST_1][0] = Mv();
          pu.mvdAffi[REF_PIC_LIST_1][1] = Mv();
          pu.mvdAffi[REF_PIC_LIST_1][2] = Mv();
        }
        else if (pu.cu->affine)
        {
          for (int i = 0; i < pu.cu->getNumAffineMvs(); i++)
          {
            mvd_coding(pu.mvdAffi[REF_PIC_LIST_1][i]);
          }
        }
        else
        {
          mvd_coding(pu.mvd[REF_PIC_LIST_1]);
        }
      }
      mvp_flag    ( pu, REF_PIC_LIST_1 );
    }
  }
  if( pu.interDir == 3 /* PRED_BI */ && PU::isBipredRestriction(pu) )
  {
    pu.mv    [REF_PIC_LIST_1] = Mv(0, 0);
    pu.refIdx[REF_PIC_LIST_1] = -1;
    pu.interDir               =  1;
    pu.cu->bcwIdx             = BCW_DEFAULT;
  }

  if ( pu.cu->smvdMode )
  {
    RefPicList eCurRefList = (RefPicList)(pu.cu->smvdMode - 1);
    pu.mvd[1 - eCurRefList].set( -pu.mvd[eCurRefList].hor, -pu.mvd[eCurRefList].ver );
    CHECK(!((pu.mvd[1 - eCurRefList].getHor() >= MVD_MIN) && (pu.mvd[1 - eCurRefList].getHor() <= MVD_MAX)) || !((pu.mvd[1 - eCurRefList].getVer() >= MVD_MIN) && (pu.mvd[1 - eCurRefList].getVer() <= MVD_MAX)), "Illegal MVD value");
    pu.refIdx[1 - eCurRefList] = pu.cs->slice->getSymRefIdx( 1 - eCurRefList );
  }
}

void CABACReader::smvd_mode( PredictionUnit& pu )
{
  pu.cu->smvdMode = 0;
  if ( pu.interDir != 3 || pu.cu->affine )
  {
    return;
  }

  if ( pu.cs->slice->getBiDirPred() == false )
  {
    return;
  }

  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__SYMMVD_FLAG );

  pu.cu->smvdMode = m_binDecoder.decodeBin(Ctx::SmvdFlag()) ? 1 : 0;

  DTRACE( g_trace_ctx, D_SYNTAX, "symmvd_flag() symmvd=%d pos=(%d,%d) size=%dx%d\n", pu.cu->smvdMode ? 1 : 0, pu.lumaPos().x, pu.lumaPos().y, pu.lumaSize().width, pu.lumaSize().height );
}

void CABACReader::subblock_merge_flag( CodingUnit& cu )
{
  cu.affine = false;

  if ( !cu.cs->slice->isIntra() && (cu.slice->getPicHeader()->getMaxNumAffineMergeCand() > 0) && cu.lumaSize().width >= 8 && cu.lumaSize().height >= 8 )
  {
    RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__AFFINE_FLAG );

    unsigned ctxId = DeriveCtx::CtxAffineFlag( cu );
    cu.affine      = m_binDecoder.decodeBin(Ctx::SubblockMergeFlag(ctxId));
    DTRACE( g_trace_ctx, D_SYNTAX, "subblock_merge_flag() subblock_merge_flag=%d ctx=%d pos=(%d,%d)\n", cu.affine ? 1 : 0, ctxId, cu.Y().x, cu.Y().y );
  }
}

void CABACReader::affine_flag( CodingUnit& cu )
{
  if ( !cu.cs->slice->isIntra() && cu.cs->sps->getUseAffine() && cu.lumaSize().width > 8 && cu.lumaSize().height > 8 )
  {
    RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__AFFINE_FLAG );

    unsigned ctxId = DeriveCtx::CtxAffineFlag( cu );
    cu.affine      = m_binDecoder.decodeBin(Ctx::AffineFlag(ctxId));
    DTRACE( g_trace_ctx, D_SYNTAX, "affine_flag() affine=%d ctx=%d pos=(%d,%d)\n", cu.affine ? 1 : 0, ctxId, cu.Y().x, cu.Y().y );

    if ( cu.affine && cu.cs->sps->getUseAffineType() )
    {
      ctxId = 0;
      cu.affineType = m_binDecoder.decodeBin(Ctx::AffineType(ctxId)) ? AffineModel::_6_PARAMS : AffineModel::_4_PARAMS;
      DTRACE(g_trace_ctx, D_SYNTAX, "affine_type() affine_type=%d ctx=%d pos=(%d,%d)\n",
             cu.affineType == AffineModel::_6_PARAMS ? 1 : 0, ctxId, cu.Y().x, cu.Y().y);
    }
    else
    {
      cu.affineType = AffineModel::_4_PARAMS;
    }
  }
}

void CABACReader::merge_flag( PredictionUnit& pu )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__MERGE_FLAG );

  pu.mergeFlag = (m_binDecoder.decodeBin(Ctx::MergeFlag()));

  DTRACE( g_trace_ctx, D_SYNTAX, "merge_flag() merge=%d pos=(%d,%d) size=%dx%d\n", pu.mergeFlag ? 1 : 0, pu.lumaPos().x, pu.lumaPos().y, pu.lumaSize().width, pu.lumaSize().height );

  if (pu.mergeFlag && CU::isIBC(*pu.cu))
  {
    pu.mmvdMergeFlag = false;
    pu.regularMergeFlag = false;
    return;
  }
}


void CABACReader::merge_data( PredictionUnit& pu )
{
  if (CU::isIBC(*pu.cu))
  {
    merge_idx(pu);
    return;
  }
  else
  {
    CodingUnit cu = *pu.cu;
    subblock_merge_flag(*pu.cu);
    if (pu.cu->affine)
    {
      merge_idx(pu);
      cu.firstPU->regularMergeFlag = false;
      return;
    }

    RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__MERGE_FLAG );

    const bool ciipAvailable = pu.cs->sps->getUseCiip() && !pu.cu->skip && pu.cu->lwidth() < MAX_CU_SIZE && pu.cu->lheight() < MAX_CU_SIZE && pu.cu->lwidth() * pu.cu->lheight() >= 64;
    const bool geoAvailable  = pu.cu->cs->slice->getSPS()->getUseGeo() && pu.cu->cs->slice->isInterB()
                              && pu.cs->sps->getMaxNumGeoCand() > 1 && pu.cu->lwidth() >= GEO_MIN_CU_SIZE
                              && pu.cu->lheight() >= GEO_MIN_CU_SIZE && pu.cu->lwidth() <= GEO_MAX_CU_SIZE
                              && pu.cu->lheight() <= GEO_MAX_CU_SIZE && pu.cu->lwidth() < 8 * pu.cu->lheight()
                              && pu.cu->lheight() < 8 * pu.cu->lwidth();
    if (geoAvailable || ciipAvailable)
    {
      cu.firstPU->regularMergeFlag = m_binDecoder.decodeBin(Ctx::RegularMergeFlag(cu.skip ? 0 : 1));
    }
    else
    {
      cu.firstPU->regularMergeFlag = true;
    }
    if (cu.firstPU->regularMergeFlag)
    {
      if (cu.cs->slice->getSPS()->getUseMMVD())
      {
        cu.firstPU->mmvdMergeFlag = m_binDecoder.decodeBin(Ctx::MmvdFlag(0));
        DTRACE(g_trace_ctx, D_SYNTAX, "mmvd_merge_flag() mmvd_merge=%d pos=(%d,%d) size=%dx%d\n", pu.mmvdMergeFlag ? 1 : 0, pu.lumaPos().x, pu.lumaPos().y, pu.lumaSize().width, pu.lumaSize().height);
      }
      else
      {
        cu.firstPU->mmvdMergeFlag = false;
      }
      if (cu.skip)
      {
        cu.mmvdSkip = cu.firstPU->mmvdMergeFlag;
      }
    }
    else
    {
      pu.mmvdMergeFlag = false;
      pu.cu->mmvdSkip = false;
      if (geoAvailable && ciipAvailable)
      {
        ciip_flag(pu);
      }
      else if (ciipAvailable)
      {
        pu.ciipFlag = true;
      }
      else
      {
        pu.ciipFlag = false;
      }
      if (pu.ciipFlag)
      {
        pu.intraDir[ChannelType::LUMA]   = PLANAR_IDX;
        pu.intraDir[ChannelType::CHROMA] = DM_CHROMA_IDX;
      }
      else
      {
        pu.cu->geoFlag = true;
      }
    }
  }
  if (pu.mmvdMergeFlag || pu.cu->mmvdSkip)
  {
    mmvd_merge_idx(pu);
  }
  else
  {
    merge_idx(pu);
  }
}


void CABACReader::merge_idx( PredictionUnit& pu )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__MERGE_INDEX );

  if ( pu.cu->affine )
  {
    int numCandminus1 = int( pu.cs->picHeader->getMaxNumAffineMergeCand() ) - 1;
    pu.mergeIdx = 0;
    if ( numCandminus1 > 0 )
    {
      if (m_binDecoder.decodeBin(Ctx::AffMergeIdx()))
      {
        pu.mergeIdx++;
        for ( ; pu.mergeIdx < numCandminus1; pu.mergeIdx++ )
        {
          if (!m_binDecoder.decodeBinEP())
          {
            break;
          }
        }
      }
    }
    DTRACE( g_trace_ctx, D_SYNTAX, "aff_merge_idx() aff_merge_idx=%d\n", pu.mergeIdx );
  }
  else
  {
    int numCandminus1 = int(pu.cs->sps->getMaxNumMergeCand()) - 1;
    pu.mergeIdx       = 0;

    if (pu.cu->geoFlag)
    {
      RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET(STATS__CABAC_BITS__GEO_INDEX);
      uint32_t splitDir = 0;
      xReadTruncBinCode(splitDir, GEO_NUM_PARTITION_MODE);
      pu.geoSplitDir          = splitDir;
      const int maxNumGeoCand = pu.cs->sps->getMaxNumGeoCand();
      CHECK(maxNumGeoCand < 2, "Incorrect max number of geo candidates");
      CHECK(pu.cu->lheight() > 64 || pu.cu->lwidth() > 64, "Incorrect block size of geo flag");
      int numCandminus2 = maxNumGeoCand - 2;
      pu.mergeIdx       = 0;
      uint8_t mergeCand0 = 0;
      uint8_t mergeCand1 = 0;
      if (m_binDecoder.decodeBin(Ctx::MergeIdx()))
      {
        mergeCand0 += unary_max_eqprob(numCandminus2) + 1;
      }
      if (numCandminus2 > 0)
      {
        if (m_binDecoder.decodeBin(Ctx::MergeIdx()))
        {
          mergeCand1 += unary_max_eqprob(numCandminus2 - 1) + 1;
        }
      }
      mergeCand1 += mergeCand1 >= mergeCand0 ? 1 : 0;
      pu.geoMergeIdx = { mergeCand0, mergeCand1 };
      DTRACE(g_trace_ctx, D_SYNTAX, "merge_idx() geo_split_dir=%d\n", splitDir);
      DTRACE(g_trace_ctx, D_SYNTAX, "merge_idx() geo_idx0=%d\n", mergeCand0);
      DTRACE(g_trace_ctx, D_SYNTAX, "merge_idx() geo_idx1=%d\n", mergeCand1);
      return;
    }

    if (CU::isIBC(*pu.cu))
    {
      numCandminus1 = int(pu.cs->sps->getMaxNumIBCMergeCand()) - 1;
    }
    if (numCandminus1 > 0)
    {
      if (m_binDecoder.decodeBin(Ctx::MergeIdx()))
      {
        pu.mergeIdx++;
        for (; pu.mergeIdx < numCandminus1; pu.mergeIdx++)
        {
          if (!m_binDecoder.decodeBinEP())
          {
            break;
          }
        }
      }
    }
    DTRACE(g_trace_ctx, D_SYNTAX, "merge_idx() merge_idx=%d\n", pu.mergeIdx);
  }
}

void CABACReader::mmvd_merge_idx(PredictionUnit& pu)
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET(STATS__CABAC_BITS__MERGE_INDEX);

  int mvdBaseIdx = 0;
  if (pu.cs->sps->getMaxNumMergeCand() > 1)
  {
    static_assert(MmvdIdx::BASE_MV_NUM == 2, "");
    mvdBaseIdx = m_binDecoder.decodeBin(Ctx::MmvdMergeIdx());
  }
  DTRACE(g_trace_ctx, D_SYNTAX, "base_mvp_idx() base_mvp_idx=%d\n", mvdBaseIdx);
  int numStepCandMinus1 = MmvdIdx::REFINE_STEP - 1;
  int mvdStep           = 0;
  if (m_binDecoder.decodeBin(Ctx::MmvdStepMvpIdx()))
  {
    mvdStep++;
    for (; mvdStep < numStepCandMinus1; mvdStep++)
    {
      if (!m_binDecoder.decodeBinEP())
      {
        break;
      }
    }
  }
  DTRACE(g_trace_ctx, D_SYNTAX, "MmvdStepMvpIdx() MmvdStepMvpIdx=%d\n", mvdStep);
  const int mvdPosition = m_binDecoder.decodeBinsEP(2);
  DTRACE(g_trace_ctx, D_SYNTAX, "pos() pos=%d\n", mvdPosition);
  pu.mmvdMergeIdx.pos.position = mvdPosition;
  pu.mmvdMergeIdx.pos.step     = mvdStep;
  pu.mmvdMergeIdx.pos.baseIdx  = mvdBaseIdx;
  DTRACE(g_trace_ctx, D_SYNTAX, "mmvd_merge_idx() mmvd_merge_idx=%d\n", pu.mmvdMergeIdx);
}

void CABACReader::inter_pred_idc( PredictionUnit& pu )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__INTER_DIR );

  if( pu.cs->slice->isInterP() )
  {
    pu.interDir = 1;
    return;
  }
  if( !(PU::isBipredRestriction(pu)) )
  {
    unsigned ctxId = DeriveCtx::CtxInterDir(pu);
    if (m_binDecoder.decodeBin(Ctx::InterDir(ctxId)))
    {
      DTRACE( g_trace_ctx, D_SYNTAX, "inter_pred_idc() ctx=%d value=%d pos=(%d,%d)\n", ctxId, 3, pu.lumaPos().x, pu.lumaPos().y );
      pu.interDir = 3;
      return;
    }
  }
  if (m_binDecoder.decodeBin(Ctx::InterDir(5)))
  {
    DTRACE( g_trace_ctx, D_SYNTAX, "inter_pred_idc() ctx=5 value=%d pos=(%d,%d)\n", 2, pu.lumaPos().x, pu.lumaPos().y );
    pu.interDir = 2;
    return;
  }
  DTRACE( g_trace_ctx, D_SYNTAX, "inter_pred_idc() ctx=5 value=%d pos=(%d,%d)\n", 1, pu.lumaPos().x, pu.lumaPos().y );
  pu.interDir = 1;
  return;
}

void CABACReader::ref_idx( PredictionUnit &pu, RefPicList eRefList )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__REF_FRM_IDX );

  if ( pu.cu->smvdMode )
  {
    pu.refIdx[eRefList] = pu.cs->slice->getSymRefIdx( eRefList );
    return;
  }

  int numRef  = pu.cs->slice->getNumRefIdx(eRefList);

  if (numRef <= 1 || !m_binDecoder.decodeBin(Ctx::RefPic()))
  {
    if( numRef > 1 )
    {
      DTRACE( g_trace_ctx, D_SYNTAX, "ref_idx() value=%d pos=(%d,%d)\n", 0, pu.lumaPos().x, pu.lumaPos().y );
    }
    pu.refIdx[eRefList] = 0;
    return;
  }
  if (numRef <= 2 || !m_binDecoder.decodeBin(Ctx::RefPic(1)))
  {
    DTRACE( g_trace_ctx, D_SYNTAX, "ref_idx() value=%d pos=(%d,%d)\n", 1, pu.lumaPos().x, pu.lumaPos().y );
    pu.refIdx[eRefList] = 1;
    return;
  }
  for( int idx = 3; ; idx++ )
  {
    if (numRef <= idx || !m_binDecoder.decodeBinEP())
    {
      pu.refIdx[eRefList] = (signed char)( idx - 1 );
      DTRACE( g_trace_ctx, D_SYNTAX, "ref_idx() value=%d pos=(%d,%d)\n", idx-1, pu.lumaPos().x, pu.lumaPos().y );
      return;
    }
  }
}

void CABACReader::mvp_flag( PredictionUnit& pu, RefPicList eRefList )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__MVP_IDX );

  unsigned mvpIdx = m_binDecoder.decodeBin(Ctx::MVPIdx());
  DTRACE(g_trace_ctx, D_SYNTAX, "mvp_flag() value=%d pos=(%d,%d)\n", mvpIdx, pu.lumaPos().x, pu.lumaPos().y);
  pu.mvpIdx[eRefList] = mvpIdx;
  DTRACE(g_trace_ctx, D_SYNTAX, "mvpIdx(refList:%d)=%d\n", eRefList, mvpIdx);
}

void CABACReader::ciip_flag(PredictionUnit &pu)
{
  if (!pu.cs->sps->getUseCiip())
  {
    pu.ciipFlag = false;
    return;
  }
  if (pu.cu->skip)
  {
    pu.ciipFlag = false;
    return;
  }

  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET(STATS__CABAC_BITS__MH_INTRA_FLAG);

  pu.ciipFlag = (m_binDecoder.decodeBin(Ctx::CiipFlag()));
  DTRACE(g_trace_ctx, D_SYNTAX, "ciip_flag() Ciip=%d pos=(%d,%d) size=%dx%d\n", pu.ciipFlag ? 1 : 0, pu.lumaPos().x,
         pu.lumaPos().y, pu.lumaSize().width, pu.lumaSize().height);
}

//================================================================================
//  clause 7.3.8.8
//--------------------------------------------------------------------------------
//    void  transform_tree      ( cs, area, cuCtx, chromaCbfs )
//    bool  split_transform_flag( depth )
//    bool  cbf_comp            ( area, depth )
//================================================================================

void CABACReader::transform_tree( CodingStructure &cs, Partitioner &partitioner, CUCtx& cuCtx,                         const PartSplit ispType, const int subTuIdx )
{
  const UnitArea&   area = partitioner.currArea();
  CodingUnit       &cu           = *cs.getCU(area.block(partitioner.chType), partitioner.chType);
  int       subTuCounter = subTuIdx;

  // split_transform_flag
  bool split = partitioner.canSplit(TU_MAX_TR_SPLIT, cs);
  const unsigned  trDepth = partitioner.currTrDepth;

  if( cu.sbtInfo && partitioner.canSplit( PartSplit( cu.getSbtTuSplit() ), cs ) )
  {
    split = true;
  }

  if (!split && cu.ispMode != ISPType::NONE)
  {
    split = partitioner.canSplit( ispType, cs );
  }

  if( split )
  {
    if (partitioner.canSplit(TU_MAX_TR_SPLIT, cs))
    {
#if ENABLE_TRACING
      const CompArea &tuArea = partitioner.currArea().block(partitioner.chType);
      DTRACE(g_trace_ctx, D_SYNTAX, "transform_tree() maxTrSplit chType=%d pos=(%d,%d) size=%dx%d\n",
             partitioner.chType, tuArea.x, tuArea.y, tuArea.width, tuArea.height);

#endif
      partitioner.splitCurrArea(TU_MAX_TR_SPLIT, cs);
    }
    else if (cu.ispMode != ISPType::NONE)
    {
      partitioner.splitCurrArea(ispType, cs);
    }
    else if (cu.sbtInfo && partitioner.canSplit(PartSplit(cu.getSbtTuSplit()), cs))
    {
      partitioner.splitCurrArea(PartSplit(cu.getSbtTuSplit()), cs);
    }
    else
    {
      THROW("Implicit TU split not available!");
    }

    do
    {
      transform_tree( cs, partitioner, cuCtx,          ispType, subTuCounter );
      subTuCounter += subTuCounter != -1 ? 1 : 0;
    } while( partitioner.nextPart( cs ) );

    partitioner.exitCurrSplit();
  }
  else
  {
    TransformUnit &tu = cs.addTU( CS::getArea( cs, area, partitioner.chType ), partitioner.chType );
    unsigned numBlocks = ::getNumberValidTBlocks( *cs.pcv );
    tu.checkTuNoResidual( partitioner.currPartIdx() );
    const bool usePlt = cs.sps->getPLTMode();

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
    DTRACE(g_trace_ctx, D_SYNTAX, "transform_unit() pos=(%d,%d) size=%dx%d depth=%d trDepth=%d\n",
           tu.block(tu.chType).x, tu.block(tu.chType).y, tu.block(tu.chType).width, tu.block(tu.chType).height,
           cu.depth, partitioner.currTrDepth);

    transform_unit(tu, cuCtx, partitioner, subTuCounter);
  }
}

bool CABACReader::cbf_comp(const CompArea &area, unsigned depth, const bool prevCbf, const bool useISP,
                           const BdpcmMode bdpcmMode)
{
  unsigned  ctxId = DeriveCtx::CtxQtCbf(area.compID, prevCbf, useISP && isLuma(area.compID));
  const CtxSet&   ctxSet  = Ctx::QtCbf[ area.compID ];

  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2(STATS__CABAC_BITS__QT_CBF, area.size(), area.compID);

  if (bdpcmMode != BdpcmMode::NONE)
  {
    ctxId = area.compID == COMPONENT_Cr ? 2 : 1;
  }

  const bool cbf = m_binDecoder.decodeBin(ctxSet(ctxId)) != 0;

  DTRACE(g_trace_ctx, D_SYNTAX, "cbf_comp() etype=%d pos=(%d,%d) ctx=%d cbf=%d\n", area.compID, area.x, area.y, ctxId,
         cbf ? 1 : 0);

  return cbf;
}

//================================================================================
//  clause 7.3.8.9
//--------------------------------------------------------------------------------
//    void  mvd_coding( pu, refList )
//================================================================================

void CABACReader::mvd_coding( Mv &rMvd )
{
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  CodingStatisticsClassType ctype_mvd    ( STATS__CABAC_BITS__MVD );
  CodingStatisticsClassType ctype_mvd_ep ( STATS__CABAC_BITS__MVD_EP );
#endif

  RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_mvd );

  // abs_mvd_greater0_flag[ 0 | 1 ]
  int horAbs = (int) m_binDecoder.decodeBin(Ctx::Mvd());
  int verAbs = (int) m_binDecoder.decodeBin(Ctx::Mvd());

  // abs_mvd_greater1_flag[ 0 | 1 ]
  if (horAbs)
  {
    horAbs += (int) m_binDecoder.decodeBin(Ctx::Mvd(1));
  }
  if (verAbs)
  {
    verAbs += (int) m_binDecoder.decodeBin(Ctx::Mvd(1));
  }

  RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_mvd_ep );

  // abs_mvd_minus2[ 0 | 1 ] and mvd_sign_flag[ 0 | 1 ]
  if (horAbs)
  {
    if (horAbs > 1)
    {
      horAbs += m_binDecoder.decodeRemAbsEP(1, 0, MV_BITS - 1);
    }
    if (m_binDecoder.decodeBinEP())
    {
      horAbs = -horAbs;
    }
  }
  if (verAbs)
  {
    if (verAbs > 1)
    {
      verAbs += m_binDecoder.decodeRemAbsEP(1, 0, MV_BITS - 1);
    }
    if (m_binDecoder.decodeBinEP())
    {
      verAbs = -verAbs;
    }
  }
  rMvd = Mv(horAbs, verAbs);
  CHECK(!((horAbs >= MVD_MIN) && (horAbs <= MVD_MAX)) || !((verAbs >= MVD_MIN) && (verAbs <= MVD_MAX)), "Illegal MVD value");
}

//================================================================================
//  clause 7.3.8.10
//--------------------------------------------------------------------------------
//    void  transform_unit      ( tu, cuCtx, chromaCbfs )
//    void  cu_qp_delta         ( cu )
//    void  cu_chroma_qp_offset ( cu )
//================================================================================
void CABACReader::transform_unit( TransformUnit& tu, CUCtx& cuCtx, Partitioner& partitioner, const int subTuCounter)
{
  const UnitArea&         area = partitioner.currArea();
  const unsigned          trDepth = partitioner.currTrDepth;

  CodingUnit&       cu = *tu.cu;
  ChromaCbfs        chromaCbfs;
  chromaCbfs.Cb = chromaCbfs.Cr = false;

  const bool chromaCbfISP =
    isChromaEnabled(area.chromaFormat) && area.blocks[COMPONENT_Cb].valid() && cu.ispMode != ISPType::NONE;

  // cbf_cb & cbf_cr
  if (isChromaEnabled(area.chromaFormat) && area.blocks[COMPONENT_Cb].valid()
      && (!cu.isSepTree() || partitioner.chType == ChannelType::CHROMA)
      && (cu.ispMode == ISPType::NONE || chromaCbfISP))
  {
    const int cbfDepth = chromaCbfISP ? trDepth - 1 : trDepth;
    if (!(cu.sbtInfo && tu.noResidual))
    {
      chromaCbfs.Cb = cbf_comp(area.blocks[COMPONENT_Cb], cbfDepth, false, false, cu.bdpcmModeChroma);
    }

    if (!(cu.sbtInfo && tu.noResidual))
    {
      chromaCbfs.Cr = cbf_comp(area.blocks[COMPONENT_Cr], cbfDepth, chromaCbfs.Cb, false, cu.bdpcmModeChroma);
    }
  }
  else if (cu.isSepTree())
  {
    chromaCbfs = ChromaCbfs(false);
  }

  if (!isChroma(partitioner.chType))
  {
    if (!CU::isIntra(cu) && trDepth == 0 && !chromaCbfs.sigChroma(area.chromaFormat))
    {
      TU::setCbfAtDepth(tu, COMPONENT_Y, trDepth, 1);
    }
    else if (cu.sbtInfo && tu.noResidual)
    {
      TU::setCbfAtDepth(tu, COMPONENT_Y, trDepth, 0);
    }
    else if (cu.sbtInfo && !chromaCbfs.sigChroma(area.chromaFormat))
    {
      assert(!tu.noResidual);
      TU::setCbfAtDepth(tu, COMPONENT_Y, trDepth, 1);
    }
    else
    {
      bool lumaCbfIsInferredACT =
        (cu.colorTransform && CU::isIntra(cu) && trDepth == 0 && !chromaCbfs.sigChroma(area.chromaFormat));
      bool lastCbfIsInferred    = lumaCbfIsInferredACT; // ISP and ACT are mutually exclusive
      bool previousCbf          = false;
      bool rootCbfSoFar         = false;
      if (cu.ispMode != ISPType::NONE)
      {
        uint32_t nTus =
          cu.ispMode == ISPType::HOR ? cu.lheight() >> floorLog2(tu.lheight()) : cu.lwidth() >> floorLog2(tu.lwidth());
        if (subTuCounter == nTus - 1)
        {
          TransformUnit* tuPointer = cu.firstTU;
          for (int tuIdx = 0; tuIdx < nTus - 1; tuIdx++)
          {
            rootCbfSoFar |= TU::getCbfAtDepth(*tuPointer, COMPONENT_Y, trDepth);
            tuPointer = tuPointer->next;
          }
          if (!rootCbfSoFar)
          {
            lastCbfIsInferred = true;
          }
        }
        if (!lastCbfIsInferred)
        {
          previousCbf = TU::getPrevTuCbfAtDepth(tu, COMPONENT_Y, trDepth);
        }
      }
      bool cbfY = lastCbfIsInferred ? true : cbf_comp(tu.Y(), trDepth, previousCbf, cu.ispMode != ISPType::NONE, cu.bdpcmMode);
      TU::setCbfAtDepth(tu, COMPONENT_Y, trDepth, (cbfY ? 1 : 0));
    }
  }
  if (isChromaEnabled(area.chromaFormat) && (cu.ispMode == ISPType::NONE || chromaCbfISP))
  {
    TU::setCbfAtDepth(tu, COMPONENT_Cb, trDepth, (chromaCbfs.Cb ? 1 : 0));
    TU::setCbfAtDepth(tu, COMPONENT_Cr, trDepth, (chromaCbfs.Cr ? 1 : 0));
  }
  bool        lumaOnly   = !isChromaEnabled(cu.chromaFormat) || !tu.blocks[COMPONENT_Cb].valid();
  bool        cbfLuma    = ( tu.cbf[ COMPONENT_Y ] != 0 );
  bool        cbfChroma  = ( lumaOnly ? false : ( chromaCbfs.Cb || chromaCbfs.Cr ) );

  if( ( cu.lwidth() > 64 || cu.lheight() > 64 || cbfLuma || cbfChroma ) &&
    (!tu.cu->isSepTree() || isLuma(tu.chType)) )
  {
    if( cu.cs->pps->getUseDQP() && !cuCtx.isDQPCoded )
    {
      cu_qp_delta(cu, cuCtx.qp, cu.qp);
      cuCtx.qp = cu.qp;
      cuCtx.isDQPCoded = true;
    }
  }
  if (!cu.isSepTree() || isChroma(tu.chType))   // !DUAL_TREE_LUMA
  {
    SizeType channelWidth = !cu.isSepTree() ? cu.lwidth() : cu.chromaSize().width;
    SizeType channelHeight = !cu.isSepTree() ? cu.lheight() : cu.chromaSize().height;

    if (cu.cs->slice->getUseChromaQpAdj() && (channelWidth > 64 || channelHeight > 64 || cbfChroma) && !cuCtx.isChromaQpAdjCoded)
    {
      cu_chroma_qp_offset(cu);
      cuCtx.isChromaQpAdjCoded = true;
    }
  }

  if( !lumaOnly )
  {
    joint_cb_cr( tu, ( tu.cbf[COMPONENT_Cb] ? 2 : 0 ) + ( tu.cbf[COMPONENT_Cr] ? 1 : 0 ) );
  }

  if (cbfLuma)
  {
    residual_coding(tu, COMPONENT_Y, cuCtx);
  }
  if (!lumaOnly)
  {
    for (ComponentID compID = COMPONENT_Cb; compID <= COMPONENT_Cr; compID = ComponentID(compID + 1))
    {
      if (tu.cbf[compID])
      {
        residual_coding(tu, compID, cuCtx);
      }
    }
  }
}

void CABACReader::cu_qp_delta( CodingUnit& cu, int predQP, int8_t& qp )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__DELTA_QP_EP );

  CHECK( predQP == std::numeric_limits<int>::max(), "Invalid predicted QP" );
  int qpY = predQP;
  int DQp = unary_max_symbol( Ctx::DeltaQP(), Ctx::DeltaQP(1), CU_DQP_TU_CMAX );
  if( DQp >= CU_DQP_TU_CMAX )
  {
    DQp += exp_golomb_eqprob( CU_DQP_EG_k  );
  }
  if( DQp > 0 )
  {
    if (m_binDecoder.decodeBinEP())
    {
      DQp = -DQp;
    }
    int qpBdOffsetY = cu.cs->sps->getQpBDOffset(ChannelType::LUMA);
    qpY = ( (predQP + DQp + (MAX_QP + 1) + 2 * qpBdOffsetY) % ((MAX_QP + 1) + qpBdOffsetY)) - qpBdOffsetY;
  }
  qp = (int8_t)qpY;

  DTRACE(g_trace_ctx, D_DQP, "x=%d, y=%d, d=%d, pred_qp=%d, DQp=%d, qp=%d\n", cu.block(cu.chType).lumaPos().x,
         cu.block(cu.chType).lumaPos().y, cu.qtDepth, predQP, DQp, qp);
}


void CABACReader::cu_chroma_qp_offset( CodingUnit& cu )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2(STATS__CABAC_BITS__CHROMA_QP_ADJUSTMENT,
                                                      cu.block(cu.chType).lumaSize(), ChannelType::CHROMA);

  // cu_chroma_qp_offset_flag
  int       length  = cu.cs->pps->getChromaQpOffsetListLen();
  unsigned  qpAdj   = m_binDecoder.decodeBin(Ctx::ChromaQpAdjFlag());
  if( qpAdj && length > 1 )
  {
    // cu_chroma_qp_offset_idx
    qpAdj += unary_max_symbol( Ctx::ChromaQpAdjIdc(), Ctx::ChromaQpAdjIdc(), length-1 );
  }
  /* NB, symbol = 0 if outer flag is not set,
   *              1 if outer flag is set and there is no inner flag
   *              1+ otherwise */
  cu.chromaQpAdj = cu.cs->chromaQpAdj = qpAdj;
}

//================================================================================
//  clause 7.3.8.11
//--------------------------------------------------------------------------------
//    void        residual_coding         ( tu, compID )
//    bool        transform_skip_flag     ( tu, compID )
//    int         last_sig_coeff          ( coeffCtx )
//    void        residual_coding_subblock( coeffCtx )
//================================================================================

void CABACReader::joint_cb_cr( TransformUnit& tu, const int cbfMask )
{
  if ( !tu.cu->slice->getSPS()->getJointCbCrEnabledFlag() )
  {
    return;
  }

  if ((CU::isIntra(*tu.cu) && cbfMask != 0) || cbfMask == CBF_MASK_CBCR)
  {
    RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2(STATS__CABAC_BITS__JOINT_CB_CR,
                                                        tu.blocks[COMPONENT_Cr].lumaSize(), ChannelType::CHROMA);
    tu.jointCbCr = (m_binDecoder.decodeBin(Ctx::JointCbCrFlag(cbfMask - 1)) ? cbfMask : 0);
  }
}

void CABACReader::residual_coding( TransformUnit& tu, ComponentID compID, CUCtx& cuCtx )
{
  const CodingUnit& cu = *tu.cu;
  DTRACE( g_trace_ctx, D_SYNTAX, "residual_coding() etype=%d pos=(%d,%d) size=%dx%d predMode=%d\n", tu.blocks[compID].compID, tu.blocks[compID].x, tu.blocks[compID].y, tu.blocks[compID].width, tu.blocks[compID].height, cu.predMode );

  if( compID == COMPONENT_Cr && tu.jointCbCr == 3 )
  {
    return;
  }

  ts_flag            ( tu, compID );

  if (tu.mtsIdx[compID] == MtsType::SKIP && !tu.cs->slice->getTSResidualCodingDisabledFlag())
  {
    residual_codingTS( tu, compID );
    return;
  }

  // determine sign hiding
  bool signHiding = cu.cs->slice->getSignDataHidingEnabledFlag();

  // init coeff coding context
  CoeffCodingContext  cctx(tu, compID, signHiding, cu.getBdpcmMode(compID));
  TCoeff*             coeff   = tu.getCoeffs( compID ).buf;

  // parse last coeff position
  cctx.setScanPosLast( last_sig_coeff( cctx, tu, compID ) );
  if (tu.mtsIdx[compID] != MtsType::SKIP && tu.blocks[compID].height >= 4 && tu.blocks[compID].width >= 4)
  {
    const int maxLfnstPos = ((tu.blocks[compID].height == 4 && tu.blocks[compID].width == 4) || (tu.blocks[compID].height == 8 && tu.blocks[compID].width == 8)) ? 7 : 15;
    cuCtx.violatesLfnstConstrained[toChannelType(compID)] |= cctx.scanPosLast() > maxLfnstPos;
  }
  if (tu.mtsIdx[compID] != MtsType::SKIP && tu.blocks[compID].height >= 4 && tu.blocks[compID].width >= 4)
  {
    const int lfnstLastScanPosTh = isLuma( compID ) ? LFNST_LAST_SIG_LUMA : LFNST_LAST_SIG_CHROMA;
    cuCtx.lfnstLastScanPos |= cctx.scanPosLast() >= lfnstLastScanPosTh;
  }
  if (isLuma(compID) && tu.mtsIdx[compID] != MtsType::SKIP)
  {
    cuCtx.mtsLastScanPos |= cctx.scanPosLast() >= 1;
  }

  // parse subblocks
  const int stateTransTab = ( tu.cs->slice->getDepQuantEnabledFlag() ? 32040 : 0 );
  int       state         = 0;

  int ctxBinSampleRatio = (compID == COMPONENT_Y) ? MAX_TU_LEVEL_CTX_CODED_BIN_CONSTRAINT_LUMA : MAX_TU_LEVEL_CTX_CODED_BIN_CONSTRAINT_CHROMA;
  cctx.regBinLimit = (tu.getTbAreaAfterCoefZeroOut(compID) * ctxBinSampleRatio) >> 4;

  int baseLevel = m_binDecoder.getCtx().getBaseLevel();
  cctx.setBaseLevel(baseLevel);
  if (tu.cs->slice->getSPS()->getSpsRangeExtension().getPersistentRiceAdaptationEnabledFlag())
  {
    cctx.setUpdateHist(1);
    unsigned riceStats    = m_binDecoder.getCtx().getGRAdaptStats((unsigned) compID);
    TCoeff historyValue = (TCoeff)1 << riceStats;
    cctx.setHistValue(historyValue);
  }
  for (int subSetId = (cctx.scanPosLast() >> cctx.log2CGSize()); subSetId >= 0; subSetId--)
  {
    cctx.initSubblock(subSetId);

    if (tu.cs->sps->getMtsEnabled() && tu.cu->sbtInfo != 0 && tu.blocks[compID].height <= 32
        && tu.blocks[compID].width <= 32 && compID == COMPONENT_Y)
    {
      if ((tu.blocks[compID].height == 32 && cctx.cgPosY() >= (16 >> cctx.log2CGHeight()))
          || (tu.blocks[compID].width == 32 && cctx.cgPosX() >= (16 >> cctx.log2CGWidth())))
      {
        continue;
      }
    }
    residual_coding_subblock(cctx, coeff, stateTransTab, state);

    if (isLuma(compID) && cctx.isSigGroup() && (cctx.cgPosY() > 3 || cctx.cgPosX() > 3))
    {
      cuCtx.violatesMtsCoeffConstraint = true;
    }
  }
}

void CABACReader::ts_flag( TransformUnit& tu, ComponentID compID )
{
  int tsFlag = tu.cu->getBdpcmMode(compID) != BdpcmMode::NONE || tu.mtsIdx[compID] == MtsType::SKIP ? 1 : 0;
  int ctxIdx = isLuma(compID) ? 0 : 1;

  if( TU::isTSAllowed ( tu, compID ) )
  {
    RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2( STATS__CABAC_BITS__MTS_FLAGS, tu.blocks[compID], compID );
    tsFlag = m_binDecoder.decodeBin(Ctx::TransformSkipFlag(ctxIdx)) != 0;
  }

  tu.mtsIdx[compID] = tsFlag ? MtsType::SKIP : MtsType::DCT2_DCT2;

  DTRACE(g_trace_ctx, D_SYNTAX, "ts_flag() etype=%d pos=(%d,%d) mtsIdx=%d\n", COMPONENT_Y, tu.cu->lx(), tu.cu->ly(),
         (int) tsFlag);
}

void CABACReader::mts_idx( CodingUnit& cu, CUCtx& cuCtx )
{
  TransformUnit &tu = *cu.firstTU;
  MtsType        mtsIdx = tu.mtsIdx[COMPONENT_Y];   // Transform skip flag has already been decoded

  if (CU::isMTSAllowed(cu, COMPONENT_Y) && !cuCtx.violatesMtsCoeffConstraint && cuCtx.mtsLastScanPos && cu.lfnstIdx == 0
      && mtsIdx != MtsType::SKIP)
  {
    RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2( STATS__CABAC_BITS__MTS_FLAGS, tu.blocks[COMPONENT_Y], COMPONENT_Y );
    int ctxIdx = 0;
    int symbol = m_binDecoder.decodeBin(Ctx::MTSIdx(ctxIdx));

    if( symbol )
    {
      ctxIdx = 1;
      mtsIdx = MtsType::DST7_DST7;   // mtsIdx = 2 -- 4
      for( int i = 0; i < 3; i++, ctxIdx++ )
      {
        symbol = m_binDecoder.decodeBin(Ctx::MTSIdx(ctxIdx));

        if( !symbol )
        {
          break;
        }

        mtsIdx++;
      }
    }
  }

  tu.mtsIdx[COMPONENT_Y] = mtsIdx;

  DTRACE(g_trace_ctx, D_SYNTAX, "mts_idx() etype=%d pos=(%d,%d) mtsIdx=%d\n", COMPONENT_Y, tu.cu->lx(), tu.cu->ly(), mtsIdx);
}

void CABACReader::isp_mode( CodingUnit& cu )
{
  if (!CU::isIntra(cu) || !isLuma(cu.chType) || cu.firstPU->multiRefIdx || !cu.cs->sps->getUseISP()
      || cu.bdpcmMode != BdpcmMode::NONE || !CU::canUseISP(cu, getFirstComponentOfChannel(cu.chType))
      || cu.colorTransform)
  {
    cu.ispMode = ISPType::NONE;
    return;
  }

  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET(STATS__CABAC_BITS__ISP_MODE_FLAG);

  int symbol = m_binDecoder.decodeBin(Ctx::ISPMode(0));

  if( symbol )
  {
    RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__ISP_SPLIT_FLAG );
    cu.ispMode = m_binDecoder.decodeBin(Ctx::ISPMode(1)) ? ISPType::VER : ISPType::HOR;
  }
  DTRACE(g_trace_ctx, D_SYNTAX, "intra_subPartitions() etype=%d pos=(%d,%d) ispIdx=%d\n", cu.chType,
         cu.block(cu.chType).x, cu.block(cu.chType).y, (int) cu.ispMode);
}

void CABACReader::residual_lfnst_mode( CodingUnit& cu,  CUCtx& cuCtx  )
{
  int chIdx = cu.isSepTree() && isChroma(cu.chType) ? 1 : 0;
  if ((cu.ispMode != ISPType::NONE && !CU::canUseLfnstWithISP(cu, cu.chType))
      || (cu.cs->sps->getUseLFNST() && CU::isIntra(cu) && cu.mipFlag && !allowLfnstWithMip(cu.firstPU->lumaSize()))
      || (cu.isSepTree() && isChroma(cu.chType) && std::min(cu.chromaSize().width, cu.chromaSize().height) < 4)
      || (cu.blocks[chIdx].lumaSize().width > cu.cs->sps->getMaxTbSize()
          || cu.blocks[chIdx].lumaSize().height > cu.cs->sps->getMaxTbSize()))
  {
    return;
  }

  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__LFNST );

  if( cu.cs->sps->getUseLFNST() && CU::isIntra( cu ) )
  {
    const bool lumaFlag              = cu.isSepTree() ? (   isLuma( cu.chType ) ? true : false ) : true;
    const bool chromaFlag            = cu.isSepTree() ? ( isChroma( cu.chType ) ? true : false ) : true;
    bool       nonZeroCoeffNonTsCorner8x8 = (lumaFlag && cuCtx.violatesLfnstConstrained[ChannelType::LUMA])
                                      || (chromaFlag && cuCtx.violatesLfnstConstrained[ChannelType::CHROMA]);
    bool isTrSkip = false;
    for (auto &currTU : CU::traverseTUs(cu))
    {
      const uint32_t numValidComp = getNumberValidComponents(cu.chromaFormat);
      for (uint32_t compID = COMPONENT_Y; compID < numValidComp; compID++)
      {
        if (currTU.blocks[compID].valid() && TU::getCbf(currTU, (ComponentID) compID)
            && currTU.mtsIdx[compID] == MtsType::SKIP)
        {
          isTrSkip = true;
          break;
        }
      }
    }
    if ((!cuCtx.lfnstLastScanPos && cu.ispMode == ISPType::NONE) || nonZeroCoeffNonTsCorner8x8 || isTrSkip)
    {
      cu.lfnstIdx = 0;
      return;
    }
  }
  else
  {
    cu.lfnstIdx = 0;
    return;
  }

  unsigned cctx = 0;
  if ( cu.isSepTree() ) cctx++;

  uint32_t idxLFNST = m_binDecoder.decodeBin(Ctx::LFNSTIdx(cctx));
  if( idxLFNST )
  {
    idxLFNST += m_binDecoder.decodeBin(Ctx::LFNSTIdx(2));
  }
  cu.lfnstIdx = idxLFNST;

  DTRACE( g_trace_ctx, D_SYNTAX, "residual_lfnst_mode() etype=%d pos=(%d,%d) mode=%d\n", COMPONENT_Y, cu.lx(), cu.ly(), ( int ) cu.lfnstIdx );
}

int CABACReader::last_sig_coeff( CoeffCodingContext& cctx, TransformUnit& tu, ComponentID compID )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2( STATS__CABAC_BITS__LAST_SIG_X_Y, Size( cctx.width(), cctx.height() ), cctx.compID() );

  unsigned PosLastX = 0, PosLastY = 0;
  unsigned maxLastPosX = cctx.maxLastPosX();
  unsigned maxLastPosY = cctx.maxLastPosY();
  unsigned zoTbWdith   = getNonzeroTuSize(cctx.width());
  unsigned zoTbHeight  = getNonzeroTuSize(cctx.height());

  if (tu.cs->sps->getMtsEnabled() && tu.cu->sbtInfo != 0 && tu.blocks[compID].width <= 32
      && tu.blocks[compID].height <= 32 && compID == COMPONENT_Y)
  {
    maxLastPosX = (tu.blocks[compID].width == 32) ? g_groupIdx[15] : maxLastPosX;
    maxLastPosY = (tu.blocks[compID].height == 32) ? g_groupIdx[15] : maxLastPosY;
    zoTbWdith  = (tu.blocks[compID].width == 32) ? 16 : zoTbWdith;
    zoTbHeight = (tu.blocks[compID].height == 32) ? 16 : zoTbHeight;
  }

  for( ; PosLastX < maxLastPosX; PosLastX++ )
  {
    if (!m_binDecoder.decodeBin(cctx.lastXCtxId(PosLastX)))
    {
      break;
    }
  }
  for( ; PosLastY < maxLastPosY; PosLastY++ )
  {
    if (!m_binDecoder.decodeBin(cctx.lastYCtxId(PosLastY)))
    {
      break;
    }
  }
  if( PosLastX > 3 )
  {
    uint32_t temp    = 0;
    uint32_t uiCount = ( PosLastX - 2 ) >> 1;
    for ( int i = uiCount - 1; i >= 0; i-- )
    {
      temp += m_binDecoder.decodeBinEP() << i;
    }
    PosLastX = g_minInGroup[PosLastX] + temp;
  }
  if( PosLastY > 3 )
  {
    uint32_t temp    = 0;
    uint32_t uiCount = ( PosLastY - 2 ) >> 1;
    for ( int i = uiCount - 1; i >= 0; i-- )
    {
      temp += m_binDecoder.decodeBinEP() << i;
    }
    PosLastY = g_minInGroup[PosLastY] + temp;
  }

  if (tu.cu->slice->getReverseLastSigCoeffFlag())
  {
    PosLastX = zoTbWdith - 1 - PosLastX;
    PosLastY = zoTbHeight - 1 - PosLastY;
  }
  int blkPos;
  blkPos = PosLastX + (PosLastY * cctx.width());

  int scanPos = 0;
  for( ; scanPos < cctx.maxNumCoeff() - 1; scanPos++ )
  {
    if( blkPos == cctx.blockPos( scanPos ) )
    {
      break;
    }
  }
  return scanPos;
}

static void checkCoeffInRange(const CoeffCodingContext &cctx, const TCoeff coeff)
{
  CHECK( coeff < cctx.minCoeff() || coeff > cctx.maxCoeff(),
         "TransCoeffLevel outside allowable range" );
}

void CABACReader::residual_coding_subblock( CoeffCodingContext& cctx, TCoeff* coeff, const int stateTransTable, int& state )
{
  // NOTE: All coefficients of the subblock must be set to zero before calling this function
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  CodingStatisticsClassType ctype_group ( STATS__CABAC_BITS__SIG_COEFF_GROUP_FLAG,  cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_map   ( STATS__CABAC_BITS__SIG_COEFF_MAP_FLAG,    cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_par   ( STATS__CABAC_BITS__PAR_FLAG,              cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_gt1   ( STATS__CABAC_BITS__GT1_FLAG,              cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_gt2   ( STATS__CABAC_BITS__GT2_FLAG,              cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_escs  ( STATS__CABAC_BITS__ESCAPE_BITS,           cctx.width(), cctx.height(), cctx.compID() );
#endif

  //===== init =====
  const int   minSubPos   = cctx.minSubPos();
  const bool  isLast      = cctx.isLast();
  int         firstSigPos = ( isLast ? cctx.scanPosLast() : cctx.maxSubPos() );
  int         nextSigPos  = firstSigPos;
  int baseLevel = cctx.getBaseLevel();
  bool updateHistory = cctx.getUpdateHist();

  //===== decode significant_coeffgroup_flag =====
  RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_group );
  bool sigGroup = ( isLast || !minSubPos );
  if( !sigGroup )
  {
    sigGroup = m_binDecoder.decodeBin(cctx.sigGroupCtxId());
  }
  if( sigGroup )
  {
    cctx.setSigGroup();
  }
  else
  {
    return;
  }

  uint8_t   ctxOffset[16];

  //===== decode absolute values =====
  const int inferSigPos   = nextSigPos != cctx.scanPosLast() ? ( cctx.isNotFirst() ? minSubPos : -1 ) : nextSigPos;
  int       firstNZPos    = nextSigPos;
  int       lastNZPos     = -1;
  int       numNonZero    =  0;
  int       remRegBins    = cctx.regBinLimit;
  int       firstPosMode2 = minSubPos - 1;
  int       sigBlkPos[ 1 << MLS_CG_SIZE ];

  for( ; nextSigPos >= minSubPos && remRegBins >= 4; nextSigPos-- )
  {
    int      blkPos     = cctx.blockPos( nextSigPos );
    unsigned sigFlag    = ( !numNonZero && nextSigPos == inferSigPos );
    if( !sigFlag )
    {
      RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_map );
      const unsigned sigCtxId = cctx.sigCtxIdAbs( nextSigPos, coeff, state );
      sigFlag                 = m_binDecoder.decodeBin(sigCtxId);
      DTRACE( g_trace_ctx, D_SYNTAX_RESI, "sig_bin() bin=%d ctx=%d\n", sigFlag, sigCtxId );
      remRegBins--;
    }
    else if( nextSigPos != cctx.scanPosLast() )
    {
      cctx.sigCtxIdAbs( nextSigPos, coeff, state ); // required for setting variables that are needed for gtx/par context selection
    }

    if( sigFlag )
    {
      uint8_t&  ctxOff = ctxOffset[ nextSigPos - minSubPos ];
      ctxOff           = cctx.ctxOffsetAbs();
      sigBlkPos[ numNonZero++ ] = blkPos;
      firstNZPos = nextSigPos;
      lastNZPos  = std::max<int>( lastNZPos, nextSigPos );

      RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_gt1 );
      unsigned gt1Flag = m_binDecoder.decodeBin(cctx.greater1CtxIdAbs(ctxOff));
      DTRACE( g_trace_ctx, D_SYNTAX_RESI, "gt1_flag() bin=%d ctx=%d\n", gt1Flag, cctx.greater1CtxIdAbs(ctxOff) );
      remRegBins--;

      unsigned parFlag = 0;
      unsigned gt2Flag = 0;
      if( gt1Flag )
      {
        RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_par );
        parFlag = m_binDecoder.decodeBin(cctx.parityCtxIdAbs(ctxOff));
        DTRACE( g_trace_ctx, D_SYNTAX_RESI, "par_flag() bin=%d ctx=%d\n", parFlag, cctx.parityCtxIdAbs( ctxOff ) );

        remRegBins--;
        RExt__DECODER_DEBUG_BIT_STATISTICS_SET(ctype_gt2);
        gt2Flag = m_binDecoder.decodeBin(cctx.greater2CtxIdAbs(ctxOff));
        DTRACE( g_trace_ctx, D_SYNTAX_RESI, "gt2_flag() bin=%d ctx=%d\n", gt2Flag, cctx.greater2CtxIdAbs( ctxOff ) );
        remRegBins--;
      }
      coeff[ blkPos ] += 1 + parFlag + gt1Flag + (gt2Flag << 1);
    }

    state = ( stateTransTable >> ((state<<2)+((coeff[blkPos]&1)<<1)) ) & 3;
  }
  firstPosMode2 = nextSigPos;
  cctx.regBinLimit = remRegBins;

  //===== 2nd PASS: Go-rice codes =====
  for( int scanPos = firstSigPos; scanPos > firstPosMode2; scanPos-- )
  {
    TCoeff& tcoeff = coeff[ cctx.blockPos( scanPos ) ];
    if( tcoeff >= 4 )
    {
      const unsigned ricePar = (cctx.*(cctx.deriveRiceRRC))(scanPos, coeff, baseLevel);

      RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_escs );
      int rem = m_binDecoder.decodeRemAbsEP(ricePar, COEF_REMAIN_BIN_REDUCTION, cctx.maxLog2TrDRange());
      DTRACE( g_trace_ctx, D_SYNTAX_RESI, "rem_val() bin=%d ctx=%d\n", rem, ricePar );
      tcoeff += (rem<<1);
      if ((updateHistory) && (rem > 0))
      {
        unsigned &riceStats = m_binDecoder.getCtx().getGRAdaptStats((unsigned) (cctx.compID()));
        cctx.updateRiceStat(riceStats, rem, 1);
        cctx.setUpdateHist(0);
        updateHistory = 0;
      }
    }
  }

  //===== coeff bypass ====
  for( int scanPos = firstPosMode2; scanPos >= minSubPos; scanPos-- )
  {
    int rice = (cctx.*(cctx.deriveRiceRRC))(scanPos, coeff, 0);
    int       pos0   = g_goRicePosCoeff0(state, rice);
    RExt__DECODER_DEBUG_BIT_STATISTICS_SET(ctype_escs);
    int rem = m_binDecoder.decodeRemAbsEP(rice, COEF_REMAIN_BIN_REDUCTION, cctx.maxLog2TrDRange());
    DTRACE( g_trace_ctx, D_SYNTAX_RESI, "rem_val() bin=%d ctx=%d\n", rem, rice );
    TCoeff    tcoeff  = ( rem == pos0 ? 0 : rem < pos0 ? rem+1 : rem );
    state = ( stateTransTable >> ((state<<2)+((tcoeff&1)<<1)) ) & 3;
    if ((updateHistory) && (rem > 0))
    {
      unsigned &riceStats = m_binDecoder.getCtx().getGRAdaptStats((unsigned) (cctx.compID()));
      cctx.updateRiceStat(riceStats, rem, 0);
      cctx.setUpdateHist(0);
      updateHistory = 0;
    }
    if( tcoeff )
    {
      int        blkPos         = cctx.blockPos( scanPos );
      sigBlkPos[ numNonZero++ ] = blkPos;
      firstNZPos = scanPos;
      lastNZPos  = std::max<int>( lastNZPos, scanPos );
      coeff[blkPos] = tcoeff;
    }
  }

  //===== decode sign's =====
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2( STATS__CABAC_BITS__SIGN_BIT, Size( cctx.width(), cctx.height() ), cctx.compID() );
  const unsigned  numSigns    = ( cctx.hideSign( firstNZPos, lastNZPos ) ? numNonZero - 1 : numNonZero );
  unsigned        signPattern = numSigns > 0 ? m_binDecoder.decodeBinsEP(numSigns) << (32 - numSigns) : 0;

  //===== set final coefficents =====
  TCoeff sumAbs = 0;
  for( unsigned k = 0; k < numSigns; k++ )
  {
    TCoeff AbsCoeff       = coeff[ sigBlkPos[ k ] ];
    sumAbs               += AbsCoeff;
    coeff[ sigBlkPos[k] ] = ( signPattern & ( 1u << 31 ) ? -AbsCoeff : AbsCoeff );
    signPattern         <<= 1;

    checkCoeffInRange(cctx, coeff[sigBlkPos[k]]);
    // NOTE: when Slice::getDepQuantEnabledFlag() is true, additional checks are required to determine
    // whether coeff is in valid range (see DQIntern::Quantizer::dequantBlock)
  }
  if( numNonZero > numSigns )
  {
    int k                 = numSigns;
    TCoeff AbsCoeff       = coeff[ sigBlkPos[ k ] ];
    sumAbs               += AbsCoeff;
    coeff[ sigBlkPos[k] ] = ( sumAbs & 1 ? -AbsCoeff : AbsCoeff );
    checkCoeffInRange(cctx, coeff[sigBlkPos[k]]);
  }
}

void CABACReader::residual_codingTS( TransformUnit& tu, ComponentID compID )
{
  DTRACE( g_trace_ctx, D_SYNTAX, "residual_codingTS() etype=%d pos=(%d,%d) size=%dx%d\n", tu.blocks[compID].compID, tu.blocks[compID].x, tu.blocks[compID].y, tu.blocks[compID].width, tu.blocks[compID].height );

  // init coeff coding context
  CoeffCodingContext  cctx(tu, compID, false, tu.cu->getBdpcmMode(compID));
  TCoeff*             coeff   = tu.getCoeffs( compID ).buf;
  int maxCtxBins = (cctx.maxNumCoeff() * 7) >> 2;
  cctx.setNumCtxBins(maxCtxBins);

  for( int subSetId = 0; subSetId <= ( cctx.maxNumCoeff() - 1 ) >> cctx.log2CGSize(); subSetId++ )
  {
    cctx.initSubblock         ( subSetId );
    int goRiceParam = 1;
    if (tu.cu->slice->getSPS()->getSpsRangeExtension().getTSRCRicePresentFlag() && tu.mtsIdx[compID] == MtsType::SKIP)
    {
      goRiceParam = goRiceParam + tu.cu->slice->getTsrcIndex();
    }
    residual_coding_subblockTS( cctx, coeff, goRiceParam);
  }
}

void CABACReader::residual_coding_subblockTS(CoeffCodingContext &cctx, TCoeff *coeff, const int riceParam)
{
  // NOTE: All coefficients of the subblock must be set to zero before calling this function
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  CodingStatisticsClassType ctype_group ( STATS__CABAC_BITS__SIG_COEFF_GROUP_FLAG,  cctx.width(), cctx.height(), cctx.compID() );
#if TR_ONLY_COEFF_STATS
  CodingStatisticsClassType ctype_map   ( STATS__CABAC_BITS__SIG_COEFF_MAP_FLAG_TS, cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_par   ( STATS__CABAC_BITS__PAR_FLAG_TS,           cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_gt1   ( STATS__CABAC_BITS__GT1_FLAG_TS,           cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_gt2   ( STATS__CABAC_BITS__GT2_FLAG_TS,           cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_escs  ( STATS__CABAC_BITS__ESCAPE_BITS_TS,        cctx.width(), cctx.height(), cctx.compID() );
#else
  CodingStatisticsClassType ctype_map   ( STATS__CABAC_BITS__SIG_COEFF_MAP_FLAG,    cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_par   ( STATS__CABAC_BITS__PAR_FLAG,              cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_gt1   ( STATS__CABAC_BITS__GT1_FLAG,              cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_gt2   ( STATS__CABAC_BITS__GT2_FLAG,              cctx.width(), cctx.height(), cctx.compID() );
  CodingStatisticsClassType ctype_escs  ( STATS__CABAC_BITS__ESCAPE_BITS,           cctx.width(), cctx.height(), cctx.compID() );
#endif

#endif

  //===== init =====
  const int   minSubPos   = cctx.maxSubPos();
  int         firstSigPos = cctx.minSubPos();
  int         nextSigPos  = firstSigPos;
  unsigned    signPattern = 0;

  //===== decode significant_coeffgroup_flag =====
  RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_group );
  bool sigGroup = cctx.isLastSubSet() && cctx.noneSigGroup();
  if( !sigGroup )
  {
    sigGroup = m_binDecoder.decodeBin(cctx.sigGroupCtxId(true));
    DTRACE(g_trace_ctx, D_SYNTAX_RESI, "ts_sigGroup() bin=%d ctx=%d\n", sigGroup, cctx.sigGroupCtxId());
  }
  if( sigGroup )
  {
    cctx.setSigGroup();
  }
  else
  {
    return;
  }

  //===== decode absolute values =====
  const int inferSigPos   = minSubPos;
  int       numNonZero    =  0;
  int       sigBlkPos[ 1 << MLS_CG_SIZE ];

  int lastScanPosPass1 = -1;
  int lastScanPosPass2 = -1;
  for (; nextSigPos <= minSubPos && cctx.numCtxBins() >= 4; nextSigPos++)
  {
    int      blkPos     = cctx.blockPos( nextSigPos );
    unsigned sigFlag    = ( !numNonZero && nextSigPos == inferSigPos );
    if( !sigFlag )
    {
      RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_map );
      const unsigned sigCtxId = cctx.sigCtxIdAbsTS(nextSigPos, coeff);
      sigFlag                 = m_binDecoder.decodeBin(sigCtxId);
      DTRACE(g_trace_ctx, D_SYNTAX_RESI, "ts_sig_bin() bin=%d ctx=%d\n", sigFlag, sigCtxId);
      cctx.decimateNumCtxBins(1);
    }

    if( sigFlag )
    {
      //===== decode sign's =====
#if TR_ONLY_COEFF_STATS
      RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2(STATS__CABAC_BITS__SIGN_BIT_TS, Size(cctx.width(), cctx.height()), cctx.compID());
#else
      RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET_SIZE2( STATS__CABAC_BITS__SIGN_BIT, Size( cctx.width(), cctx.height() ), cctx.compID() );
#endif
      int sign;
      const unsigned signCtxId = cctx.signCtxIdAbsTS(nextSigPos, coeff, cctx.bdpcm());
      sign                     = m_binDecoder.decodeBin(signCtxId);
      cctx.decimateNumCtxBins(1);

      signPattern += ( sign << numNonZero );

      sigBlkPos[numNonZero++] = blkPos;

      RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_gt1 );
      unsigned gt1Flag;
      const unsigned gt1CtxId = cctx.lrg1CtxIdAbsTS(nextSigPos, coeff, cctx.bdpcm());
      gt1Flag                 = m_binDecoder.decodeBin(gt1CtxId);
      DTRACE(g_trace_ctx, D_SYNTAX_RESI, "ts_gt1_flag() bin=%d ctx=%d\n", gt1Flag, gt1CtxId);
      cctx.decimateNumCtxBins(1);

      unsigned parFlag = 0;
      if( gt1Flag )
      {
        RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_par );
        parFlag = m_binDecoder.decodeBin(cctx.parityCtxIdAbsTS());
        DTRACE(g_trace_ctx, D_SYNTAX_RESI, "ts_par_flag() bin=%d ctx=%d\n", parFlag, cctx.parityCtxIdAbsTS());
        cctx.decimateNumCtxBins(1);
      }
      coeff[ blkPos ] = (sign ? -1 : 1 ) * (TCoeff)(1 + parFlag + gt1Flag);
    }
    lastScanPosPass1 = nextSigPos;
  }

  int cutoffVal = 2;
  const int numGtBins = 4;

  //===== 2nd PASS: gt2 =====
  for (int scanPos = firstSigPos; scanPos <= minSubPos && cctx.numCtxBins() >= 4; scanPos++)
  {
    TCoeff& tcoeff = coeff[cctx.blockPos(scanPos)];
    cutoffVal = 2;
    for (int i = 0; i < numGtBins; i++)
    {
      if( tcoeff < 0)
      {
        tcoeff = -tcoeff;
      }
      if (tcoeff >= cutoffVal)
      {
        RExt__DECODER_DEBUG_BIT_STATISTICS_SET(ctype_gt2);
        unsigned gt2Flag;
        gt2Flag = m_binDecoder.decodeBin(cctx.greaterXCtxIdAbsTS(cutoffVal >> 1));
        tcoeff += (gt2Flag << 1);
        DTRACE(g_trace_ctx, D_SYNTAX_RESI, "ts_gt%d_flag() bin=%d ctx=%d sp=%d coeff=%d\n", i, gt2Flag,
               cctx.greaterXCtxIdAbsTS(cutoffVal >> 1), scanPos, tcoeff);
        cctx.decimateNumCtxBins(1);
      }
      cutoffVal += 2;
    }
    lastScanPosPass2 = scanPos;
  }
  //===== 3rd PASS: Go-rice codes =====
  for( int scanPos = firstSigPos; scanPos <= minSubPos; scanPos++ )
  {
    TCoeff& tcoeff = coeff[ cctx.blockPos( scanPos ) ];
    RExt__DECODER_DEBUG_BIT_STATISTICS_SET( ctype_escs );

    cutoffVal = (scanPos <= lastScanPosPass2 ? 10 : (scanPos <= lastScanPosPass1 ? 2 : 0));
    if (tcoeff < 0)
    {
      tcoeff = -tcoeff;
    }
    if( tcoeff >= cutoffVal )
    {
      int       rice = riceParam;
      int       rem  = m_binDecoder.decodeRemAbsEP(rice, COEF_REMAIN_BIN_REDUCTION, cctx.maxLog2TrDRange());
      DTRACE( g_trace_ctx, D_SYNTAX_RESI, "ts_rem_val() bin=%d ctx=%d sp=%d\n", rem, rice, scanPos );
      tcoeff += (scanPos <= lastScanPosPass1) ? (rem << 1) : rem;
      if (tcoeff && scanPos > lastScanPosPass1)
      {
        int      blkPos = cctx.blockPos(scanPos);
        int      sign   = m_binDecoder.decodeBinEP();
        signPattern += (sign << numNonZero);
        sigBlkPos[numNonZero++] = blkPos;
      }
    }
    if (cctx.bdpcm() == BdpcmMode::NONE && cutoffVal)
    {
      if (tcoeff > 0)
      {
        int rightPixel, belowPixel;
        cctx.neighTS(rightPixel, belowPixel, scanPos, coeff);
        tcoeff = cctx.decDeriveModCoeff(rightPixel, belowPixel, tcoeff);
      }
    }
  }

  //===== set final coefficents =====
  for( unsigned k = 0; k < numNonZero; k++ )
  {
    TCoeff AbsCoeff       = coeff[ sigBlkPos[ k ] ];
    coeff[ sigBlkPos[k] ] = ( signPattern & 1 ? -AbsCoeff : AbsCoeff );
    signPattern         >>= 1;
    checkCoeffInRange(cctx, coeff[sigBlkPos[k]]);
  }
}

//================================================================================
//  helper functions
//--------------------------------------------------------------------------------
//    unsigned  unary_max_symbol ( ctxId0, ctxId1, maxSymbol )
//    unsigned  unary_max_eqprob (                 maxSymbol )
//    unsigned  exp_golomb_eqprob( count )
//================================================================================

unsigned CABACReader::unary_max_symbol( unsigned ctxId0, unsigned ctxIdN, unsigned maxSymbol  )
{
  unsigned onesRead = 0;
  while (onesRead < maxSymbol && m_binDecoder.decodeBin(onesRead == 0 ? ctxId0 : ctxIdN) == 1)
  {
    ++onesRead;
  }
  return onesRead;
}

unsigned CABACReader::unary_max_eqprob( unsigned maxSymbol )
{
  for( unsigned k = 0; k < maxSymbol; k++ )
  {
    if (!m_binDecoder.decodeBinEP())
    {
      return k;
    }
  }
  return maxSymbol;
}

unsigned CABACReader::exp_golomb_eqprob( unsigned count )
{
  unsigned symbol = 0;
  unsigned bit    = 1;
  while( bit )
  {
    bit = m_binDecoder.decodeBinEP();
    symbol += bit << count++;
  }
  if( --count )
  {
    symbol += m_binDecoder.decodeBinsEP(count);
  }
  return symbol;
}

void CABACReader::mip_flag( CodingUnit& cu )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__OTHER );

  if( !cu.Y().valid() )
  {
    return;
  }
  if( !cu.cs->sps->getUseMIP() )
  {
    cu.mipFlag = false;
    return;
  }

  unsigned ctxId = DeriveCtx::CtxMipFlag( cu );
  cu.mipFlag     = m_binDecoder.decodeBin(Ctx::MipFlag(ctxId));
  DTRACE( g_trace_ctx, D_SYNTAX, "mip_flag() pos=(%d,%d) mode=%d\n", cu.lumaPos().x, cu.lumaPos().y, cu.mipFlag ? 1 : 0 );
}

void CABACReader::mip_pred_modes( CodingUnit &cu )
{
  RExt__DECODER_DEBUG_BIT_STATISTICS_CREATE_SET( STATS__CABAC_BITS__OTHER );

  if( !cu.Y().valid() )
  {
    return;
  }
  for( auto &pu : CU::traversePUs( cu ) )
  {
    mip_pred_mode( pu );
  }
}

void CABACReader::mip_pred_mode( PredictionUnit &pu )
{
  pu.mipTransposedFlag = bool(m_binDecoder.decodeBinEP());

  uint32_t mipMode;
  const int numModes = MatrixIntraPrediction::getNumModesMip(pu.Y());
  xReadTruncBinCode( mipMode, numModes );
  pu.intraDir[ChannelType::LUMA] = mipMode;
  CHECKD(pu.intraDir[ChannelType::LUMA] < 0 || pu.intraDir[ChannelType::LUMA] >= numModes, "Invalid MIP mode");

  DTRACE(g_trace_ctx, D_SYNTAX, "mip_pred_mode() pos=(%d,%d) mode=%d transposed=%d\n", pu.lumaPos().x, pu.lumaPos().y,
         pu.intraDir[ChannelType::LUMA], pu.mipTransposedFlag ? 1 : 0);
}
