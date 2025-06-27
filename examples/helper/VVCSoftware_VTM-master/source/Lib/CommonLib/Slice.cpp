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

#include "CommonDef.h"
#include "Unit.h"
#include "Slice.h"
#include "Picture.h"
#include "dtrace_next.h"

#include "UnitTools.h"

Slice::Slice()
  : m_poc(0)
  , m_iLastIDR(0)
  , m_prevGDRInSameLayerPOC(-MAX_INT)
  , m_iAssociatedIRAP(0)
  , m_iAssociatedIRAPType(NAL_UNIT_INVALID)
  , m_prevGDRSubpicPOC(-MAX_INT)
  , m_prevIRAPSubpicPOC(-MAX_INT)
  , m_prevIRAPSubpicType(NAL_UNIT_INVALID)
  , m_rplIdx{ -1, -1 }
  , m_eNalUnitType(NAL_UNIT_CODED_SLICE_IDR_W_RADL)
  , m_pictureHeaderInSliceHeader(false)
  , m_eSliceType(I_SLICE)
  , m_noOutputOfPriorPicsFlag(0)
  , m_iSliceQp(0)
  , m_chromaQpAdjEnabled(false)
  , m_lmcsEnabledFlag(0)
  , m_explicitScalingListUsed(0)
  , m_deblockingFilterDisable(false)
  , m_deblockingFilterOverrideFlag(false)
  , m_deblockingFilterBetaOffsetDiv2(0)
  , m_deblockingFilterTcOffsetDiv2(0)
  , m_deblockingFilterCbBetaOffsetDiv2(0)
  , m_deblockingFilterCbTcOffsetDiv2(0)
  , m_deblockingFilterCrBetaOffsetDiv2(0)
  , m_deblockingFilterCrTcOffsetDiv2(0)
  , m_depQuantEnabledFlag(false)
  , m_reverseLastSigCoeffFlag(false)
  , m_signDataHidingEnabledFlag(false)
  , m_tsResidualCodingDisabledFlag(false)
  , m_pendingRasInit(false)
  , m_checkLdc(false)
  , m_biDirPred(false)
  , m_lmChromaCheckDisable(false)
  , m_iSliceQpDelta(0)
  , m_hierPredLayerIdx(0)
  , m_pcSPS(nullptr)
  , m_pcPPS(nullptr)
  , m_pcPic(nullptr)
  , m_pcPicHeader(nullptr)
  , m_colFromL0Flag(true)
  , m_colRefIdx(0)
  , m_uiTLayer(0)
  , m_bTLayerSwitchingFlag(false)
  , m_independentSliceIdx(0)
  , m_nextSlice(false)
  , m_sliceBits(0)
  , m_finalized(false)
  , m_bTestWeightPred(false)
  , m_bTestWeightBiPred(false)
  , m_substreamSizes()
  , m_numEntryPoints(0)
  , m_cabacInitFlag(false)
  , m_sliceSubPicId(0)
  , m_encCABACTableIdx(I_SLICE)
  , m_iProcessingStartTime(0)
  , m_dProcessingTime(0)
{
  for (uint32_t i = 0; i < MAX_TSRC_RICE; i++)
  {
    m_riceBit[i] = 0;
  }

  m_cntRightBottom = 0;

  for(uint32_t i=0; i<NUM_REF_PIC_LIST_01; i++)
  {
    m_aiNumRefIdx[i] = 0;
  }

  for (uint32_t component = 0; component < MAX_NUM_COMPONENT; component++)
  {
    m_lambdas            [component] = 0.0;
    m_iSliceChromaQpDelta[component] = 0;
  }
  m_iSliceChromaQpDelta[JOINT_CbCr] = 0;

  initEqualRef();

  for ( int idx = 0; idx < MAX_NUM_REF; idx++ )
  {
    m_list1IdxToList0Idx[idx] = -1;
  }

  for(int iNumCount = 0; iNumCount < MAX_NUM_REF; iNumCount++)
  {
    for(uint32_t i=0; i<NUM_REF_PIC_LIST_01; i++)
    {
      m_apcRefPicList[i][iNumCount]  = nullptr;
      m_aiRefPOCList  [i][iNumCount] = 0;
    }
  }

  resetWpScaling();
  initWpAcDcParam();

  m_saoEnabledFlag.fill(false);

  memset(m_alfApss, 0, sizeof(m_alfApss));
  m_ccAlfFilterParam.reset();
  resetAlfEnabledFlag();
  resetCcAlCbfEnabledFlag();
  resetCcAlCrfEnabledFlag();

  m_sliceMap.initSliceMap();
}

Slice::~Slice()
{
  m_sliceMap.initSliceMap();
}


void Slice::initSlice()
{
  for(uint32_t i=0; i<NUM_REF_PIC_LIST_01; i++)
  {
    m_aiNumRefIdx[i]      = 0;
  }
  m_colFromL0Flag = true;
  m_colRefIdx = 0;
  m_lmcsEnabledFlag = 0;
  m_explicitScalingListUsed = 0;
  initEqualRef();

  m_noOutputOfPriorPicsFlag = 0;

  m_checkLdc = false;

  m_biDirPred = false;
  m_lmChromaCheckDisable = false;
  m_symRefIdx[0] = -1;
  m_symRefIdx[1] = -1;

  for (uint32_t component = 0; component < MAX_NUM_COMPONENT; component++)
  {
    m_iSliceChromaQpDelta[component] = 0;
  }
  m_iSliceChromaQpDelta[JOINT_CbCr] = 0;

  m_finalized = false;

  m_substreamSizes.clear();
  m_cabacInitFlag        = false;
  m_enableDRAPSEI        = false;
  m_useLTforDRAP         = false;
  m_isDRAP               = false;
  m_latestDRAPPOC        = MAX_INT;
  m_edrapRapId           = 0;
  m_enableEdrapSEI       = false;
  m_edrapRapId           = 0;
  m_useLTforEdrap        = false;
  m_edrapNumRefRapPics   = 0;
  m_edrapRefRapIds.resize(0);
  m_latestEDRAPPOC       = MAX_INT;
  m_latestEdrapLeadingPicDecodableFlag = false;
  resetAlfEnabledFlag();
  m_ccAlfFilterParam.reset();
  m_ccAlfCbEnabledFlag = 0;
  m_ccAlfCrEnabledFlag = 0;
  m_ccAlfCbApsId = -1;
  m_ccAlfCrApsId = -1;
  m_nuhLayerId = 0;
}

void Slice::inheritFromPicHeader( PicHeader *picHeader, const PPS *pps, const SPS *sps )
{
  if (pps->getRplInfoInPhFlag())
  {
    for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
    {
      const int rplIdx = picHeader->getRplIdx(l);
      setRplIdx(l, rplIdx);
      m_rpl[l] = rplIdx == -1 ? *picHeader->getRpl(l) : *sps->getRplList(l)->getReferencePictureList(rplIdx);
    }
  }

  setDeblockingFilterDisable( picHeader->getDeblockingFilterDisable() );
  setDeblockingFilterBetaOffsetDiv2( picHeader->getDeblockingFilterBetaOffsetDiv2() );
  setDeblockingFilterTcOffsetDiv2( picHeader->getDeblockingFilterTcOffsetDiv2() );
  if (pps->getPPSChromaToolFlag())
  {
    setDeblockingFilterCbBetaOffsetDiv2 ( picHeader->getDeblockingFilterCbBetaOffsetDiv2() );
    setDeblockingFilterCbTcOffsetDiv2   ( picHeader->getDeblockingFilterCbTcOffsetDiv2()   );
    setDeblockingFilterCrBetaOffsetDiv2 ( picHeader->getDeblockingFilterCrBetaOffsetDiv2() );
    setDeblockingFilterCrTcOffsetDiv2   ( picHeader->getDeblockingFilterCrTcOffsetDiv2()   );
  }
  else
  {
    setDeblockingFilterCbBetaOffsetDiv2 ( getDeblockingFilterBetaOffsetDiv2() );
    setDeblockingFilterCbTcOffsetDiv2   ( getDeblockingFilterTcOffsetDiv2()   );
    setDeblockingFilterCrBetaOffsetDiv2 ( getDeblockingFilterBetaOffsetDiv2() );
    setDeblockingFilterCrTcOffsetDiv2   ( getDeblockingFilterTcOffsetDiv2()   );
  }

  setSaoEnabledFlag(ChannelType::LUMA, picHeader->getSaoEnabledFlag(ChannelType::LUMA));
  setSaoEnabledFlag(ChannelType::CHROMA, picHeader->getSaoEnabledFlag(ChannelType::CHROMA));

  setAlfEnabledFlag(COMPONENT_Y,  picHeader->getAlfEnabledFlag(COMPONENT_Y));
  setAlfEnabledFlag(COMPONENT_Cb, picHeader->getAlfEnabledFlag(COMPONENT_Cb));
  setAlfEnabledFlag(COMPONENT_Cr, picHeader->getAlfEnabledFlag(COMPONENT_Cr));
  setNumAlfApsIdsLuma(picHeader->getNumAlfApsIdsLuma());
  setAlfApsIdsLuma(picHeader->getAlfApsIdsLuma());
  setAlfApsIdChroma(picHeader->getAlfApsIdChroma());
  setCcAlfCbEnabledFlag(picHeader->getCcAlfEnabledFlag(COMPONENT_Cb));
  setCcAlfCrEnabledFlag(picHeader->getCcAlfEnabledFlag(COMPONENT_Cr));
  setCcAlfCbApsId(picHeader->getCcAlfCbApsId());
  setCcAlfCrApsId(picHeader->getCcAlfCrApsId());
  m_ccAlfFilterParam.ccAlfFilterEnabled[COMPONENT_Cb - 1] = picHeader->getCcAlfEnabledFlag(COMPONENT_Cb);
  m_ccAlfFilterParam.ccAlfFilterEnabled[COMPONENT_Cr - 1] = picHeader->getCcAlfEnabledFlag(COMPONENT_Cr);
}

void Slice::setNumSubstream(const SPS* sps, const PPS* pps)
{
  uint32_t ctuAddr, ctuX, ctuY;
  m_numSubstream = 0;

  // count the number of CTUs that align with either the start of a tile, or with an entropy coding sync point
  // ignore the first CTU since it doesn't count as an entry point
  for (uint32_t i = 1; i < m_sliceMap.getNumCtuInSlice(); i++)
  {
    ctuAddr = m_sliceMap.getCtuAddrInSlice(i);
    ctuX    = (ctuAddr % pps->getPicWidthInCtu());
    ctuY    = (ctuAddr / pps->getPicWidthInCtu());

    if (pps->ctuIsTileColBd(ctuX) && (pps->ctuIsTileRowBd(ctuY) || sps->getEntropyCodingSyncEnabledFlag()))
    {
      m_numSubstream++;
    }
  }
}

void Slice::setNumEntryPoints(const SPS *sps, const PPS *pps)
{
  uint32_t ctuAddr, ctuX, ctuY;
  uint32_t prevCtuAddr, prevCtuX, prevCtuY;
  m_numEntryPoints = 0;

  if (!sps->getEntryPointsPresentFlag())
  {
    return;
  }

  // count the number of CTUs that align with either the start of a tile, or with an entropy coding sync point
  // ignore the first CTU since it doesn't count as an entry point
  for( uint32_t i = 1; i < m_sliceMap.getNumCtuInSlice(); i++ )
  {
    ctuAddr = m_sliceMap.getCtuAddrInSlice( i );
    ctuX = ( ctuAddr % pps->getPicWidthInCtu() );
    ctuY = ( ctuAddr / pps->getPicWidthInCtu() );
    prevCtuAddr = m_sliceMap.getCtuAddrInSlice(i - 1);
    prevCtuX    = (prevCtuAddr % pps->getPicWidthInCtu());
    prevCtuY    = (prevCtuAddr / pps->getPicWidthInCtu());

    if (pps->ctuToTileRowBd(ctuY) != pps->ctuToTileRowBd(prevCtuY) || pps->ctuToTileColBd(ctuX) != pps->ctuToTileColBd(prevCtuX) || (ctuY != prevCtuY && sps->getEntropyCodingSyncEnabledFlag()))
    {
      m_numEntryPoints++;
    }
  }
}

void Slice::setDefaultClpRng( const SPS& sps )
{
  m_clpRngs.comp[COMPONENT_Y].min = m_clpRngs.comp[COMPONENT_Cb].min  = m_clpRngs.comp[COMPONENT_Cr].min = 0;
  m_clpRngs.comp[COMPONENT_Y].max  = (1 << sps.getBitDepth(ChannelType::LUMA)) - 1;
  m_clpRngs.comp[COMPONENT_Y].bd   = sps.getBitDepth(ChannelType::LUMA);
  m_clpRngs.comp[COMPONENT_Y].n   = 0;
  m_clpRngs.comp[COMPONENT_Cb].max = m_clpRngs.comp[COMPONENT_Cr].max = (1 << sps.getBitDepth(ChannelType::CHROMA)) - 1;
  m_clpRngs.comp[COMPONENT_Cb].bd = m_clpRngs.comp[COMPONENT_Cr].bd = sps.getBitDepth(ChannelType::CHROMA);
  m_clpRngs.comp[COMPONENT_Cb].n   = m_clpRngs.comp[COMPONENT_Cr].n   = 0;
}


bool Slice::getRapPicFlag() const
{
  return getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL
      || getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP
    || getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA;
}

void Slice::sortPicList(PicList &picList)
{
  picList.sort([](Picture *const &a, Picture *const &b) {
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
    // process higher layers first to be able to resolve dependent layers when flushing
    return a->getPOC() < b->getPOC() || (a->getPOC() == b->getPOC() && a->layerId > b->layerId);
#else
    return a->getPOC() < b->getPOC() || (a->getPOC() == b->getPOC() && a->layerId < b->layerId);
#endif
  });
}

Picture* Slice::xGetRefPic( PicList& rcListPic, const int poc, const int layerId )
{
  // return a nullptr, if picture is not found
  Picture* refPic = nullptr;

  for ( auto &currPic : rcListPic )
  {
    if( currPic->getPOC() == poc && currPic->layerId == layerId )
    {
      refPic = currPic;
      break;
    }
  }
  return  refPic;
}

Picture* Slice::xGetLongTermRefPic( PicList& rcListPic, const int poc, const bool pocHasMsb, const int layerId )
{
  // return a nullptr, if picture is not found or the found picture is not long-term
  Picture*  refPic = nullptr;
  const int pocCycle = 1 << getSPS()->getBitsForPOC();

  const int refPoc = pocHasMsb ? poc : (poc & (pocCycle - 1));

  for ( auto &currPic : rcListPic )
  {
    if( currPic->getPOC() != this->getPOC() && currPic->referenced && currPic->layerId == layerId )
    {
      int currPicPoc = pocHasMsb ? currPic->getPOC() : (currPic->getPOC() & (pocCycle - 1));
      if (refPoc == currPicPoc)
      {
        if(currPic->longTerm)
        {
          refPic = currPic;
        }
        break;
      }
    }
  }

  return refPic;
}

Picture* Slice::xGetLongTermRefPicCandidate( PicList& rcListPic, const int poc, const bool pocHasMsb, const int layerId )
{
  // return a nullptr, if picture is not found (might be a short-term or a long-term)
  Picture*  refPic = nullptr;
  const int pocCycle = 1 << getSPS()->getBitsForPOC();

  const int refPoc = pocHasMsb ? poc : (poc & (pocCycle - 1));

  for ( auto &currPic : rcListPic )
  {
    if( currPic->getPOC() != this->getPOC() && currPic->referenced && currPic->layerId == layerId )
    {
      int currPicPoc = pocHasMsb ? currPic->getPOC() : (currPic->getPOC() & (pocCycle - 1));
      if (refPoc == currPicPoc)
      {
        refPic = currPic;
        break;
      }
    }
  }

  return refPic;
}

void Slice::setRefPOCList       ()
{
  for (int dir = 0; dir < NUM_REF_PIC_LIST_01; dir++)
  {
    for (int numRefIdx = 0; numRefIdx < m_aiNumRefIdx[dir]; numRefIdx++)
    {
      m_aiRefPOCList[dir][numRefIdx] = m_apcRefPicList[dir][numRefIdx]->getPOC();
    }
  }

}

void Slice::setList1IdxToList0Idx()
{
  for (int idxL1 = 0; idxL1 < getNumRefIdx(REF_PIC_LIST_1); idxL1++)
  {
    m_list1IdxToList0Idx[idxL1] = -1;
    for (int idxL0 = 0; idxL0 < getNumRefIdx(REF_PIC_LIST_0); idxL0++)
    {
      if ( m_apcRefPicList[REF_PIC_LIST_0][idxL0]->getPOC() == m_apcRefPicList[REF_PIC_LIST_1][idxL1]->getPOC() )
      {
        m_list1IdxToList0Idx[idxL1] = idxL0;
        break;
      }
    }
  }
}

void Slice::constructRefPicList(PicList& rcListPic)
{
  ::memset(m_bIsUsedAsLongTerm, 0, sizeof(m_bIsUsedAsLongTerm));
  if (m_eSliceType == I_SLICE)
  {
    ::memset(m_apcRefPicList, 0, sizeof(m_apcRefPicList));
    ::memset(m_aiNumRefIdx, 0, sizeof(m_aiNumRefIdx));
    return;
  }

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const uint32_t              numActiveRefs = getNumRefIdx(l);
    const ReferencePictureList* rpl           = getRpl(l);

    for (int refIdx = 0; refIdx < rpl->getNumRefEntries(); refIdx++)
    {
      Picture*   refPic      = nullptr;
      const bool isActiveRef = refIdx < numActiveRefs;
      bool       isLongTerm  = false;

      if (rpl->isInterLayerRefPic(refIdx))
      {
        const VPS *vps = m_pcPic->cs->vps;

        const int interLayerIdx = rpl->getInterLayerRefPicIdx(refIdx);
        CHECK(interLayerIdx == NOT_VALID, "Wrong ILRP index");

        const int layerIdx   = vps->getGeneralLayerIdx(m_pcPic->layerId);
        const int refLayerId = vps->getLayerId(vps->getDirectRefLayerIdx(layerIdx, interLayerIdx));

        refPic = xGetRefPic(rcListPic, getPOC(), refLayerId);

        isLongTerm = true;
      }
      else if (!rpl->isRefPicLongterm(refIdx))
      {
        refPic = xGetRefPic(rcListPic, getPOC() + rpl->getRefPicIdentifier(refIdx), m_pcPic->layerId);
      }
      else
      {
        const int  pocBits   = getSPS()->getBitsForPOC();
        const int  pocMask   = (1 << pocBits) - 1;
        int        ltrpPoc   = rpl->getRefPicIdentifier(refIdx) & pocMask;
        const bool pocHasMsb = rpl->getDeltaPocMSBPresentFlag(refIdx);
        if (pocHasMsb)
        {
          ltrpPoc += (getPOC() & ~pocMask) - rpl->getDeltaPocMSBCycleLT(refIdx) * (pocMask + 1);
        }
        refPic = xGetLongTermRefPicCandidate(rcListPic, ltrpPoc, pocHasMsb, m_pcPic->layerId);

        isLongTerm = true;
      }

      // NOTE: refPic may be null if inactive reference picture is not be in DPB
      if (refPic != nullptr)
      {
        refPic->longTerm = isLongTerm;
      }
      if (isActiveRef)
      {
        CHECK(refPic == nullptr, "Active reference picture not found");
        refPic->extendPicBorder(getSPS(), getPPS());
        m_apcRefPicList[l][refIdx]     = refPic;
        m_bIsUsedAsLongTerm[l][refIdx] = refPic->longTerm;
      }
    }
  }
}

void Slice::initEqualRef()
{
  for (int dir = 0; dir < NUM_REF_PIC_LIST_01; dir++)
  {
    for (int refIdx1 = 0; refIdx1 < MAX_NUM_REF; refIdx1++)
    {
      for (int refIdx2 = refIdx1; refIdx2 < MAX_NUM_REF; refIdx2++)
      {
        m_abEqualRef[dir][refIdx1][refIdx2] = m_abEqualRef[dir][refIdx2][refIdx1] = (refIdx1 == refIdx2 ? true : false);
      }
    }
  }
}

void Slice::checkColRefIdx(uint32_t curSliceSegmentIdx, const Picture* pic)
{
  int i;
  Slice* curSlice = pic->slices[curSliceSegmentIdx];
  int currColRefPOC =  curSlice->getRefPOC( RefPicList(1 - curSlice->getColFromL0Flag()), curSlice->getColRefIdx());

  for(i=curSliceSegmentIdx-1; i>=0; i--)
  {
    const Slice* preSlice = pic->slices[i];
    if(preSlice->getSliceType() != I_SLICE)
    {
      const int preColRefPOC  = preSlice->getRefPOC( RefPicList(1 - preSlice->getColFromL0Flag()), preSlice->getColRefIdx());
      if(currColRefPOC != preColRefPOC)
      {
        THROW("sh_collocated_ref_idx shall always be the same for all slices of a coded picture!");
      }
      else
      {
        break;
      }
    }
  }
}

void Slice::checkCRA(const ReferencePictureList* pRPL0, const ReferencePictureList* pRPL1, const int pocCRA, CheckCRAFlags &flags, PicList& rcListPic)
{
  if (pocCRA < MAX_UINT && getPOC() > pocCRA)
  {
    if (flags.seenLeadingFieldPic && flags.trailingFieldHadRefIssue)
    {
      THROW("Invalid state");
    }

    uint32_t numRefPic = pRPL0->getNumRefEntries();
    for (int i = 0; i < numRefPic; i++)
    {
      if (!pRPL0->isRefPicLongterm(i))
      {
        if (getPOC() + pRPL0->getRefPicIdentifier(i) < pocCRA)
        {
          // report error immediately if we are
          //   processing frames
          //   or this is second trailing field picture
          //   or this is trailing field picture after leading field picture
          //   or this is active reference picture of trailing field picture
          CHECK(!getSPS()->getFieldSeqFlag() || flags.seenTrailingFieldPic || flags.seenLeadingFieldPic || i < getNumRefIdx(REF_PIC_LIST_0), "Invalid state");

          // otherwise, we are checking non-active reference picture of first trailing field picture
          flags.trailingFieldHadRefIssue = true;
          return;
        }
      }
      else if (!pRPL0->isInterLayerRefPic(i))
      {
        int pocBits = getSPS()->getBitsForPOC();
        int pocMask = (1 << pocBits) - 1;
        int ltrpPoc = pRPL0->getRefPicIdentifier(i) & pocMask;
        if(pRPL0->getDeltaPocMSBPresentFlag(i))
        {
          ltrpPoc += getPOC() - pRPL0->getDeltaPocMSBCycleLT(i) * (pocMask + 1) - (getPOC() & pocMask);
        }
        const Picture *ltrp =
          xGetLongTermRefPic(rcListPic, ltrpPoc, pRPL0->getDeltaPocMSBPresentFlag(i), m_pcPic->layerId);
        if (ltrp == nullptr || ltrp->getPOC() < pocCRA)
        {
          CHECK(!getSPS()->getFieldSeqFlag() || flags.seenTrailingFieldPic || flags.seenLeadingFieldPic || i < getNumRefIdx(REF_PIC_LIST_0), "Invalid state");
          flags.trailingFieldHadRefIssue = true;
          return;
        }
      }
    }
    numRefPic = pRPL1->getNumRefEntries();
    for (int i = 0; i < numRefPic; i++)
    {
      if (!pRPL1->isRefPicLongterm(i))
      {
        if (getPOC() + pRPL1->getRefPicIdentifier(i) < pocCRA)
        {
          CHECK(!getSPS()->getFieldSeqFlag() || flags.seenTrailingFieldPic || flags.seenLeadingFieldPic || i < getNumRefIdx(REF_PIC_LIST_1), "Invalid state");
          flags.trailingFieldHadRefIssue = true;
          return;
        }
      }
      else if( !pRPL1->isInterLayerRefPic( i ) )
      {
        int pocBits = getSPS()->getBitsForPOC();
        int pocMask = (1 << pocBits) - 1;
        int ltrpPoc = pRPL1->getRefPicIdentifier(i) & pocMask;
        if(pRPL1->getDeltaPocMSBPresentFlag(i))
        {
          ltrpPoc += getPOC() - pRPL1->getDeltaPocMSBCycleLT(i) * (pocMask + 1) - (getPOC() & pocMask);
        }
        const Picture *ltrp =
          xGetLongTermRefPic(rcListPic, ltrpPoc, pRPL1->getDeltaPocMSBPresentFlag(i), m_pcPic->layerId);
        if (ltrp == nullptr || ltrp->getPOC() < pocCRA)
        {
          CHECK(!getSPS()->getFieldSeqFlag() || flags.seenTrailingFieldPic || flags.seenLeadingFieldPic || i < getNumRefIdx(REF_PIC_LIST_1), "Invalid state");
          flags.trailingFieldHadRefIssue = true;
          return;
        }
      }
    }

    if (getSPS()->getFieldSeqFlag())
    {
      flags.seenTrailingFieldPic = true;
    }
  }
  else if (getSPS()->getFieldSeqFlag() && getPOC() < pocCRA)
  {
    flags.seenLeadingFieldPic = true;
    flags.trailingFieldHadRefIssue = false;
  }
}

void Slice::checkRPL(const ReferencePictureList* pRPL0, const ReferencePictureList* pRPL1, const int associatedIRAPDecodingOrderNumber, PicList& rcListPic)
{
  Picture* pcRefPic;
  int refPicPOC;
  int refPicDecodingOrderNumber;

  int irapPOC = getAssociatedIRAPPOC();

  const int numEntries[NUM_REF_PIC_LIST_01]       = { pRPL0->getNumRefEntries(), pRPL1->getNumRefEntries() };
  const int numActiveEntries[NUM_REF_PIC_LIST_01] = { getNumRefIdx(REF_PIC_LIST_0), getNumRefIdx(REF_PIC_LIST_1) };

  const ReferencePictureList *rpl[NUM_REF_PIC_LIST_01] = { pRPL0, pRPL1 };

  const bool fieldSeqFlag = getSPS()->getFieldSeqFlag();
  const int layerIdx = m_pcPic->cs->vps == nullptr ? 0 : m_pcPic->cs->vps->getGeneralLayerIdx( m_pcPic->layerId );

  for( int refPicList = 0; refPicList < 2; refPicList++ )
  {
    for( int i = 0; i < numEntries[refPicList]; i++ )
    {
      if( rpl[refPicList]->isInterLayerRefPic( i ) )
      {
        int refLayerId = m_pcPic->cs->vps->getLayerId( m_pcPic->cs->vps->getDirectRefLayerIdx( layerIdx, rpl[refPicList]->getInterLayerRefPicIdx( i ) ) );
        pcRefPic = xGetRefPic( rcListPic, getPOC(), refLayerId );
        refPicPOC = pcRefPic->getPOC();
      }
      else if( !rpl[refPicList]->isRefPicLongterm( i ) )
      {
        refPicPOC = getPOC() + rpl[refPicList]->getRefPicIdentifier(i);
        pcRefPic = xGetRefPic( rcListPic, refPicPOC, m_pcPic->layerId );
      }
      else
      {
        int pocBits = getSPS()->getBitsForPOC();
        int pocMask = ( 1 << pocBits ) - 1;
        int ltrpPoc = rpl[refPicList]->getRefPicIdentifier( i ) & pocMask;
        if( rpl[refPicList]->getDeltaPocMSBPresentFlag( i ) )
        {
          ltrpPoc += getPOC() - rpl[refPicList]->getDeltaPocMSBCycleLT( i ) * ( pocMask + 1 ) - ( getPOC() & pocMask );
        }
        pcRefPic = xGetLongTermRefPic( rcListPic, ltrpPoc, rpl[refPicList]->getDeltaPocMSBPresentFlag( i ), m_pcPic->layerId );
        refPicPOC = pcRefPic->getPOC();
      }
      if (pcRefPic) // the checks are for all reference picture, but we may not have an inactive reference picture, if starting with a CRA
      {
        refPicDecodingOrderNumber = pcRefPic->getDecodingOrderNumber();

        if( m_eNalUnitType == NAL_UNIT_CODED_SLICE_CRA || m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL || m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP )
        {
          CHECK( refPicPOC < irapPOC || refPicDecodingOrderNumber < associatedIRAPDecodingOrderNumber, "When the current picture, with nuh_layer_id equal to a particular value layerId, "
            "is an IRAP picture, there shall be no picture referred to by an entry in RefPicList[ 0 ] that precedes, in output order or decoding order, any preceding IRAP picture "
            "with nuh_layer_id equal to layerId in decoding order (when present)." );
        }

        if( irapPOC < getPOC() && !fieldSeqFlag )
        {
          CHECK( refPicPOC < irapPOC || refPicDecodingOrderNumber < associatedIRAPDecodingOrderNumber, "When the current picture follows an IRAP picture having the same value "
            "of nuh_layer_id and the leading pictures, if any, associated with that IRAP picture, in both decoding order and output order, there shall be no picture referred "
            "to by an entry in RefPicList[ 0 ] or RefPicList[ 1 ] that precedes that IRAP picture in output order or decoding order." );
        }

        // Generated reference picture does not have picture header
        const bool nonReferencePictureFlag = pcRefPic->nonReferencePictureFlag;
        CHECK( pcRefPic == m_pcPic || nonReferencePictureFlag, "The picture referred to by each entry in RefPicList[ 0 ] or RefPicList[ 1 ] shall not be the current picture and shall have ph_non_ref_pic_flag equal to 0" );

        if( i < numActiveEntries[refPicList] )
        {
          if( irapPOC < getPOC() )
          {
            CHECK( refPicPOC < irapPOC || refPicDecodingOrderNumber < associatedIRAPDecodingOrderNumber, "When the current picture follows an IRAP picture having the same value "
              "of nuh_layer_id in both decoding order and output order, there shall be no picture referred to by an active entry in RefPicList[ 0 ] or RefPicList[ 1 ] that "
              "precedes that IRAP picture in output order or decoding order." );
          }

          // Checking this: "When the current picture is a RADL picture, there shall be no active entry in RefPicList[ 0 ] or
          // RefPicList[ 1 ] that is any of the following: A picture that precedes the associated IRAP picture in decoding order"
          if( m_eNalUnitType == NAL_UNIT_CODED_SLICE_RADL )
          {
            CHECK( refPicDecodingOrderNumber < associatedIRAPDecodingOrderNumber, "RADL picture detected that violate the rule that no active entry in RefPicList[] shall precede the associated IRAP picture in decoding order" );
            // Checking this: "When the current picture is a RADL picture, there shall be no active entry in RefPicList[ 0 ] or
            // RefPicList[ 1 ] that is any of the following: A RASL picture with pps_mixed_nalu_types_in_pic_flag is equal to 0
            for (int i = 0; i < pcRefPic->numSlices; i++)
            {
              if (!pcRefPic->mixedNaluTypesInPicFlag)
              {
                CHECK(pcRefPic->slices[i]->getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL, "When the current picture is a RADL picture, there shall be no active entry in RefPicList[ 0 ] or RefPicList[ 1 ] that is a RASL picture with pps_mixed_nalu_types_in_pic_flag is equal to 0");
              }
            }

          }

          CHECK( pcRefPic->temporalId > m_pcPic->temporalId, "The picture referred to by each active entry in RefPicList[ 0 ] or RefPicList[ 1 ] shall be present in the DPB and shall have TemporalId less than or equal to that of the current picture." );
        }
        // Add a constraint on an ILRP being either an IRAP picture or having TemporalId less than or equal to
        // Max (0, vps_max_tid_il_ref_pics_plus1[ refPicVpsLayerId ] - 1 ), with refPicVpsLayerId equal to the value of
        // the nuh_layer_id of the referenced picture.
        if (rpl[refPicList]->isInterLayerRefPic(i))
        {
          bool cond1      = (pcRefPic->getPictureType() == NAL_UNIT_CODED_SLICE_GDR);
          bool cond2      = (pcRefPic->slices[0]->getPicHeader()->getRecoveryPocCnt() == 0);
          bool cond3      = (pcRefPic->cs->slice->isIRAP());

          const VPS *vps                  = pcRefPic->cs->vps;
          const int  maxTidILRefPicsPlus1 =
            vps->getMaxTidIlRefPicsPlus1(layerIdx, vps->getGeneralLayerIdx(pcRefPic->layerId));
          bool cond4 = (pcRefPic->temporalId < maxTidILRefPicsPlus1);

          CHECK(!((cond1 && cond2) || cond3 || cond4),
                "Either of the following conditions shall apply for the picture referred to by each ILRP entry, when "
                "present, in RefPicList[ 0 ] or RefPicList[ 1 ] of a slice of the current picture:-The picture is a "
                "GDR picture with "
                "ph_recovery_poc_cnt equal to 0 or an IRAP picture."
                "-The picture has TemporalId less than vps_max_tid_il_ref_pics_plus1[ currLayerIdx ][ refLayerIdx ], "
                "where currLayerIdx and refLayerIdx are equal to "
                "GeneralLayerIdx[ nuh_layer_id ] and GeneralLayerIdx[ refpicLayerId ], respectively. ");
        }
      }
    }
  }
}

void Slice::checkSTSA(PicList& rcListPic)
{
  int ii;
  Picture *pcRefPic = nullptr;

  int numOfActiveRef = getNumRefIdx(REF_PIC_LIST_0);

  for (ii = 0; ii < numOfActiveRef; ii++)
  {
    pcRefPic = m_apcRefPicList[REF_PIC_LIST_0][ii];

    if( m_eNalUnitType == NAL_UNIT_CODED_SLICE_STSA && pcRefPic->layerId == m_pcPic->layerId )
    {
      CHECK( pcRefPic->temporalId == m_uiTLayer, "When the current picture is an STSA picture and nuh_layer_id equal to that of the current picture, there shall be no active entry in the RPL that has TemporalId equal to that of the current picture" );
    }

    // Checking this: "When the current picture is a picture that follows, in decoding order, an STSA picture that has TemporalId equal to that of the current picture, there shall be no
    // picture that has TemporalId equal to that of the current picture included as an active entry in RefPicList[ 0 ] or RefPicList[ 1 ] that precedes the STSA picture in decoding order."
    CHECK(pcRefPic->subLayerNonReferencePictureDueToSTSA, "The RPL of the current picture contains a picture that is not allowed in this temporal layer due to an earlier STSA picture");
  }

  numOfActiveRef = getNumRefIdx(REF_PIC_LIST_1);
  for (ii = 0; ii < numOfActiveRef; ii++)
  {
    pcRefPic = m_apcRefPicList[REF_PIC_LIST_1][ii];

    if( m_eNalUnitType == NAL_UNIT_CODED_SLICE_STSA && pcRefPic->layerId == m_pcPic->layerId )
    {
      CHECK( pcRefPic->temporalId == m_uiTLayer, "When the current picture is an STSA picture and nuh_layer_id equal to that of the current picture, there shall be no active entry in the RPL that has TemporalId equal to that of the current picture" );
    }

    // Checking this: "When the current picture is a picture that follows, in decoding order, an STSA picture that has TemporalId equal to that of the current picture, there shall be no
    // picture that has TemporalId equal to that of the current picture included as an active entry in RefPicList[ 0 ] or RefPicList[ 1 ] that precedes the STSA picture in decoding order."
    CHECK(pcRefPic->subLayerNonReferencePictureDueToSTSA, "The active RPL part of the current picture contains a picture that is not allowed in this temporal layer due to an earlier STSA picture");
  }

  // If the current picture is an STSA picture, make all reference pictures in the DPB with temporal
  // id equal to the temproal id of the current picture sub-layer non-reference pictures. The flag
  // subLayerNonReferencePictureDueToSTSA equal to true means that the picture may not be used for
  // reference by a picture that follows the current STSA picture in decoding order
  if (getNalUnitType() == NAL_UNIT_CODED_SLICE_STSA)
  {
    PicList::iterator iterPic = rcListPic.begin();
    while (iterPic != rcListPic.end())
    {
      pcRefPic = *(iterPic++);
      if (!pcRefPic->referenced || pcRefPic->getPOC() == m_poc)
      {
        continue;
      }

      if (pcRefPic->temporalId == m_uiTLayer)
      {
        pcRefPic->subLayerNonReferencePictureDueToSTSA = true;
      }
    }
  }
}


/** Function for marking the reference pictures when an IDR/CRA/CRANT/BLA/BLANT is encountered.
 * \param pocCRA POC of the CRA/CRANT/BLA/BLANT picture
 * \param bRefreshPending flag indicating if a deferred decoding refresh is pending
 * \param rcListPic reference to the reference picture list
 * This function marks the reference pictures as "unused for reference" in the following conditions.
 * If the nal_unit_type is IDR/BLA/BLANT, all pictures in the reference picture list
 * are marked as "unused for reference"
 *    If the nal_unit_type is BLA/BLANT, set the pocCRA to the temporal reference of the current picture.
 * Otherwise
 *    If the bRefreshPending flag is true (a deferred decoding refresh is pending) and the current
 *    temporal reference is greater than the temporal reference of the latest CRA/CRANT/BLA/BLANT picture (pocCRA),
 *    mark all reference pictures except the latest CRA/CRANT/BLA/BLANT picture as "unused for reference" and set
 *    the bRefreshPending flag to false.
 *    If the nal_unit_type is CRA/CRANT, set the bRefreshPending flag to true and pocCRA to the temporal
 *    reference of the current picture.
 * Note that the current picture is already placed in the reference list and its marking is not changed.
 * If the current picture has a nal_ref_idc that is not 0, it will remain marked as "used for reference".
 */
void Slice::decodingRefreshMarking(int& pocCRA, bool& bRefreshPending, PicList& rcListPic, const bool bEfficientFieldIRAPEnabled)
{
  Picture* rpcPic;
  int      pocCurr = getPOC();

  if ( getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL
    || getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP)  // IDR picture
  {
    // mark all pictures as not used for reference
    PicList::iterator        iterPic       = rcListPic.begin();
    while (iterPic != rcListPic.end())
    {
      rpcPic = *(iterPic);
      if (rpcPic->getPOC() != pocCurr)
      {
        rpcPic->referenced = false;
        rpcPic->getHashMap()->clearAll();
      }
      iterPic++;
    }
    if (bEfficientFieldIRAPEnabled)
    {
      bRefreshPending = true;
    }
  }
  else // CRA or No DR
  {
    if(bEfficientFieldIRAPEnabled && (getAssociatedIRAPType() == NAL_UNIT_CODED_SLICE_IDR_N_LP || getAssociatedIRAPType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL))
    {
      if (bRefreshPending==true && pocCurr > m_iLastIDR) // IDR reference marking pending
      {
        PicList::iterator        iterPic       = rcListPic.begin();
        while (iterPic != rcListPic.end())
        {
          rpcPic = *(iterPic);
          if (rpcPic->getPOC() != pocCurr && rpcPic->getPOC() != m_iLastIDR)
          {
            rpcPic->referenced = false;
            rpcPic->getHashMap()->clearAll();
          }
          iterPic++;
        }
        bRefreshPending = false;
      }
    }
    else
    {
      if (bRefreshPending==true && pocCurr > pocCRA) // CRA reference marking pending
      {
        PicList::iterator iterPic = rcListPic.begin();
        while (iterPic != rcListPic.end())
        {
          rpcPic = *(iterPic);
          if (rpcPic->getPOC() != pocCurr && rpcPic->getPOC() != pocCRA)
          {
            rpcPic->referenced = false;
            rpcPic->getHashMap()->clearAll();
          }
          iterPic++;
        }
        bRefreshPending = false;
      }
    }
    if ( getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA ) // CRA picture found
    {
      bRefreshPending = true;
      pocCRA = pocCurr;
    }
  }
}

void Slice::copySliceInfo(Slice *pSrc, bool cpyAlmostAll)
{
  CHECK(!pSrc, "Source is nullptr");

  int i, j, k;

  m_poc                               = pSrc->m_poc;
  m_eNalUnitType         = pSrc->m_eNalUnitType;
  m_eSliceType           = pSrc->m_eSliceType;
  m_iSliceQp             = pSrc->m_iSliceQp;
  m_iSliceQpBase         = pSrc->m_iSliceQpBase;
  m_chromaQpAdjEnabled                = pSrc->m_chromaQpAdjEnabled;
  m_deblockingFilterDisable         = pSrc->m_deblockingFilterDisable;
  m_deblockingFilterOverrideFlag    = pSrc->m_deblockingFilterOverrideFlag;
  m_deblockingFilterBetaOffsetDiv2  = pSrc->m_deblockingFilterBetaOffsetDiv2;
  m_deblockingFilterTcOffsetDiv2    = pSrc->m_deblockingFilterTcOffsetDiv2;
  m_deblockingFilterCbBetaOffsetDiv2  = pSrc->m_deblockingFilterCbBetaOffsetDiv2;
  m_deblockingFilterCbTcOffsetDiv2    = pSrc->m_deblockingFilterCbTcOffsetDiv2;
  m_deblockingFilterCrBetaOffsetDiv2  = pSrc->m_deblockingFilterCrBetaOffsetDiv2;
  m_deblockingFilterCrTcOffsetDiv2    = pSrc->m_deblockingFilterCrTcOffsetDiv2;
  m_depQuantEnabledFlag               = pSrc->m_depQuantEnabledFlag;
  m_signDataHidingEnabledFlag         = pSrc->m_signDataHidingEnabledFlag;
  m_tsResidualCodingDisabledFlag      = pSrc->m_tsResidualCodingDisabledFlag;
  m_tsrcIndex                         = pSrc->m_tsrcIndex;

  for (i = 0; i < MAX_TSRC_RICE; i++)
  {
    m_riceBit[i] = pSrc->m_riceBit[i];
  }
  m_reverseLastSigCoeffFlag = pSrc->m_reverseLastSigCoeffFlag;
  m_cntRightBottom          = pSrc->m_cntRightBottom;

  for (i = 0; i < NUM_REF_PIC_LIST_01; i++)
  {
    m_aiNumRefIdx[i]     = pSrc->m_aiNumRefIdx[i];
  }

  for (i = 0; i < MAX_NUM_REF; i++)
  {
    m_list1IdxToList0Idx[i] = pSrc->m_list1IdxToList0Idx[i];
  }

  m_checkLdc             = pSrc->m_checkLdc;
  m_iSliceQpDelta        = pSrc->m_iSliceQpDelta;

  m_biDirPred = pSrc->m_biDirPred;
  m_lmChromaCheckDisable = pSrc->m_lmChromaCheckDisable;;
  m_symRefIdx[0] = pSrc->m_symRefIdx[0];
  m_symRefIdx[1] = pSrc->m_symRefIdx[1];

  for (uint32_t component = 0; component < MAX_NUM_COMPONENT; component++)
  {
    m_iSliceChromaQpDelta[component] = pSrc->m_iSliceChromaQpDelta[component];
  }
  m_iSliceChromaQpDelta[JOINT_CbCr] = pSrc->m_iSliceChromaQpDelta[JOINT_CbCr];

  for (i = 0; i < NUM_REF_PIC_LIST_01; i++)
  {
    for (j = 0; j < MAX_NUM_REF; j++)
    {
      m_apcRefPicList[i][j]  = pSrc->m_apcRefPicList[i][j];
      m_aiRefPOCList[i][j]   = pSrc->m_aiRefPOCList[i][j];
      m_bIsUsedAsLongTerm[i][j] = pSrc->m_bIsUsedAsLongTerm[i][j];
    }
    m_bIsUsedAsLongTerm[i][MAX_NUM_REF] = pSrc->m_bIsUsedAsLongTerm[i][MAX_NUM_REF];
  }
  if (cpyAlmostAll)
  {
    m_hierPredLayerIdx = pSrc->m_hierPredLayerIdx;
  }

  // access channel
  if (cpyAlmostAll)
  {
    m_rpl[REF_PIC_LIST_0] = pSrc->m_rpl[REF_PIC_LIST_0];
    m_rpl[REF_PIC_LIST_1] = pSrc->m_rpl[REF_PIC_LIST_1];
  }
  m_iLastIDR             = pSrc->m_iLastIDR;

  if (cpyAlmostAll)
  {
    m_pcPic = pSrc->m_pcPic;
  }

  m_pcPicHeader          = pSrc->m_pcPicHeader;
  m_colFromL0Flag        = pSrc->m_colFromL0Flag;
  m_colRefIdx            = pSrc->m_colRefIdx;

  if (cpyAlmostAll)
  {
    setLambdas(pSrc->getLambdas());
  }

  for (i = 0; i < NUM_REF_PIC_LIST_01; i++)
  {
    for (j = 0; j < MAX_NUM_REF; j++)
    {
      for (k =0; k < MAX_NUM_REF; k++)
      {
        m_abEqualRef[i][j][k] = pSrc->m_abEqualRef[i][j][k];
      }
    }
  }

  m_uiTLayer                      = pSrc->m_uiTLayer;
  m_bTLayerSwitchingFlag          = pSrc->m_bTLayerSwitchingFlag;

  m_sliceMap                      = pSrc->m_sliceMap;
  m_independentSliceIdx           = pSrc->m_independentSliceIdx;
  m_nextSlice                     = pSrc->m_nextSlice;
  m_clpRngs                       = pSrc->m_clpRngs;
  m_lmcsEnabledFlag               = pSrc->m_lmcsEnabledFlag;
  m_explicitScalingListUsed       = pSrc->m_explicitScalingListUsed;

  m_pendingRasInit                = pSrc->m_pendingRasInit;

  for ( uint32_t e=0 ; e<NUM_REF_PIC_LIST_01 ; e++ )
  {
    for ( uint32_t n=0 ; n<MAX_NUM_REF ; n++ )
    {
      memcpy(m_weightPredTable[e][n], pSrc->m_weightPredTable[e][n], sizeof(WPScalingParam)*MAX_NUM_COMPONENT );
    }
  }

  m_saoEnabledFlag = pSrc->m_saoEnabledFlag;

  m_cabacInitFlag                 = pSrc->m_cabacInitFlag;
  memcpy(m_alfApss, pSrc->m_alfApss, sizeof(m_alfApss)); // this might be quite unsafe
  memcpy( m_alfEnabledFlag, pSrc->m_alfEnabledFlag, sizeof(m_alfEnabledFlag));
  m_numAlfApsIdsLuma              = pSrc->m_numAlfApsIdsLuma;
  m_alfApsIdsLuma                 = pSrc->m_alfApsIdsLuma;
  m_alfApsIdChroma                = pSrc->m_alfApsIdChroma;
  m_disableSATDForRd              = pSrc->m_disableSATDForRd;
  m_isLossless = pSrc->m_isLossless;

  if (cpyAlmostAll)
  {
    m_encCABACTableIdx = pSrc->m_encCABACTableIdx;
  }
  for( int i = 0; i < NUM_REF_PIC_LIST_01; i ++ )
  {
    for (int j = 0; j < MAX_NUM_REF_PICS; j ++ )
    {
      m_scalingRatio[i][j]          = pSrc->m_scalingRatio[i][j];
    }
  }
  m_ccAlfFilterParam                        = pSrc->m_ccAlfFilterParam;
  m_ccAlfFilterControl[0]                   = pSrc->m_ccAlfFilterControl[0];
  m_ccAlfFilterControl[1]                   = pSrc->m_ccAlfFilterControl[1];
  m_ccAlfCbEnabledFlag             = pSrc->m_ccAlfCbEnabledFlag;
  m_ccAlfCrEnabledFlag             = pSrc->m_ccAlfCrEnabledFlag;
  m_ccAlfCbApsId                   = pSrc->m_ccAlfCbApsId;
  m_ccAlfCrApsId                   = pSrc->m_ccAlfCrApsId;
}


/** Function for checking if this is a switching-point
*/
bool Slice::isTemporalLayerSwitchingPoint(PicList& rcListPic) const
{
  // loop through all pictures in the reference picture buffer
  PicList::iterator iterPic = rcListPic.begin();
  while ( iterPic != rcListPic.end())
  {
    const Picture* pcPic = *(iterPic++);
    if( pcPic->referenced && pcPic->poc != getPOC())
    {
      if( pcPic->temporalId >= getTLayer())
      {
        return false;
      }
    }
  }
  return true;
}

/** Function for checking if this is a STSA candidate
 */
bool Slice::isStepwiseTemporalLayerSwitchingPointCandidate(PicList& rcListPic) const
{
  PicList::iterator iterPic = rcListPic.begin();
  while ( iterPic != rcListPic.end())
  {
    const Picture* pcPic = *(iterPic++);
    if( pcPic->referenced && pcPic->poc != getPOC())
    {
      if( pcPic->temporalId >= getTLayer())
      {
        return false;
      }
    }
  }
  return true;
}


void Slice::checkLeadingPictureRestrictions(PicList& rcListPic, const PPS& pps) const
{
  int nalUnitType = this->getNalUnitType();

  // When a picture is a leading picture, it shall be a RADL or RASL picture.
  if(this->getAssociatedIRAPPOC() > this->getPOC())
  {
    //check this only when pps_mixed_nalu_types_in_pic_flag is equal to 0
    if (!pps.getMixedNaluTypesInPicFlag())
    {
      // Do not check IRAP pictures since they may get a POC lower than their associated IRAP
      if (nalUnitType < NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
          nalUnitType > NAL_UNIT_CODED_SLICE_CRA)
      {
        CHECK(nalUnitType != NAL_UNIT_CODED_SLICE_RASL &&
              nalUnitType != NAL_UNIT_CODED_SLICE_RADL, "Invalid NAL unit type");
      }
    }
  }

  if (this->getAssociatedIRAPPOC() <= this->getPOC())
  {
    if (!pps.getMixedNaluTypesInPicFlag())
    {
      CHECK(nalUnitType == NAL_UNIT_CODED_SLICE_RASL || nalUnitType == NAL_UNIT_CODED_SLICE_RADL, "When a picture is not a leading picture, it shall not be a RADL or RASL picture.");
    }
  }

  // No RASL pictures shall be present in the bitstream that are associated with
  // an IDR picture.
  if (nalUnitType == NAL_UNIT_CODED_SLICE_RASL && !pps.getMixedNaluTypesInPicFlag())
  {
    CHECK( this->getAssociatedIRAPType() == NAL_UNIT_CODED_SLICE_IDR_N_LP   ||
           this->getAssociatedIRAPType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL, "Invalid NAL unit type");
  }

  // No RADL pictures shall be present in the bitstream that are associated with
  // a BLA picture having nal_unit_type equal to BLA_N_LP or that are associated
  // with an IDR picture having nal_unit_type equal to IDR_N_LP.
  if (nalUnitType == NAL_UNIT_CODED_SLICE_RADL && !pps.getMixedNaluTypesInPicFlag())
  {
    CHECK (this->getAssociatedIRAPType() == NAL_UNIT_CODED_SLICE_IDR_N_LP, "Invalid NAL unit type");
  }

  // loop through all pictures in the reference picture buffer
  PicList::iterator iterPic = rcListPic.begin();
  int numNonLPFound = 0;
  while ( iterPic != rcListPic.end())
  {
    Picture* pcPic = *(iterPic++);
    if( ! pcPic->reconstructed)
    {
      continue;
    }
    if( pcPic->poc == this->getPOC())
    {
      continue;
    }
    const Slice* pcSlice = pcPic->slices[0];

    if(pcSlice->getPicHeader()) // Generated reference picture does not have picture header
    {
      if (pcSlice->getPicHeader()->getPicOutputFlag() == 1 && !this->getNoOutputOfPriorPicsFlag() && pcPic->layerId == this->m_nuhLayerId)
      {
        if ((nalUnitType == NAL_UNIT_CODED_SLICE_CRA || nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP || nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL) && !pps.getMixedNaluTypesInPicFlag())
        {
          CHECK(pcPic->poc >= this->getPOC(), "Any picture, with nuh_layer_id equal to a particular value layerId, that precedes an IRAP picture with nuh_layer_id "
                "equal to layerId in decoding order shall precede the IRAP picture in output order.");
        }
      }

      if (pcSlice->getPicHeader()->getPicOutputFlag() == 1 && pcPic->layerId == this->m_nuhLayerId)
      {
        if (nalUnitType == NAL_UNIT_CODED_SLICE_RADL)
        {
          if (this->getAssociatedIRAPPOC() > pcSlice->getAssociatedIRAPPOC() && !pps.getMixedNaluTypesInPicFlag())
          {
            if (this->getAssociatedIRAPPOC() != pcPic->poc)
            {
              CHECK(pcPic->poc >= this->getPOC(), "Any picture, with nuh_layer_id equal to a particular value layerId, that precedes an IRAP picture with nuh_layer_id "
                    "equal to layerId in decoding order shall precede any RADL picture associated with the IRAP picture in output order.");
            }
          }
        }
      }

      if (pcSlice->getPicHeader()->getPicOutputFlag() == 1 && !this->getPicHeader()->getNoOutputBeforeRecoveryFlag() && pcPic->layerId == this->m_nuhLayerId
          && nalUnitType != NAL_UNIT_CODED_SLICE_GDR && this->getPicHeader()->getRecoveryPocCnt() != -1)
      {
        if (this->getPOC() == this->getPicHeader()->getRecoveryPocCnt() + this->getPrevGDRInSameLayerPOC())
        {
          CHECK(pcPic->poc >= this->getPOC(), "Any picture, with nuh_layer_id equal to a particular value layerId, that precedes a recovery point picture with "
                "nuh_layer_id equal to layerId in decoding order shall precede the recovery point picture in output order.");
        }
      }
    }

    if ((nalUnitType == NAL_UNIT_CODED_SLICE_RASL || nalUnitType == NAL_UNIT_CODED_SLICE_RADL) &&
      (pcSlice->getNalUnitType() != NAL_UNIT_CODED_SLICE_RASL && pcSlice->getNalUnitType() != NAL_UNIT_CODED_SLICE_RADL) && !pps.getMixedNaluTypesInPicFlag())
    {
      if (pcSlice->getAssociatedIRAPPOC() == this->getAssociatedIRAPPOC() && pcPic->layerId == this->m_nuhLayerId)
      {
        numNonLPFound++;
        int limitNonLP = 0;
        if (pcSlice->getSPS()->getFieldSeqFlag())
        {
          limitNonLP = 1;
        }
        CHECK(pcPic->poc > this->getAssociatedIRAPPOC() && numNonLPFound > limitNonLP, "If sps_field_seq_flag is equal to 0 and the current picture, with nuh_layer_id "
              "equal to a particular value layerId, is a leading picture associated with an IRAP picture, it shall precede, in decoding order, all non-leading "
              "pictures that are associated with the same IRAP picture.Otherwise, let picA and picB be the first and the last leading pictures, in decoding order, "
              "associated with an IRAP picture, respectively, there shall be at most one non-leading picture with nuh_layer_id equal to layerId preceding picA in "
              "decoding order, and there shall be no non-leading picture with nuh_layer_id equal to layerId between picA and picB in decoding order.");
      }
    }

    if (nalUnitType == NAL_UNIT_CODED_SLICE_RASL && !pps.getMixedNaluTypesInPicFlag())
    {
      if ((this->getAssociatedIRAPType() == NAL_UNIT_CODED_SLICE_CRA) &&
          this->getAssociatedIRAPPOC() == pcSlice->getAssociatedIRAPPOC())
      {
        if (pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RADL)
        {
          CHECK(pcPic->poc <= this->getPOC(), "Any RASL picture associated with a CRA picture shall precede any RADL picture associated with the CRA picture in output order.");
        }
      }
    }

    if (nalUnitType == NAL_UNIT_CODED_SLICE_RASL && !pps.getMixedNaluTypesInPicFlag())
    {
      if(this->getAssociatedIRAPType() == NAL_UNIT_CODED_SLICE_CRA)
      {
        if(pcSlice->getPOC() < this->getAssociatedIRAPPOC() &&
          (
            pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP   ||
            pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
            pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA ||
            pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR) &&
            pcPic->layerId == this->m_nuhLayerId)
        {
          CHECK(this->getPOC() <= pcSlice->getPOC(), "Any RASL picture, with nuh_layer_id equal to a particular value layerId, associated with a CRA picture shall follow, "
               "in output order, any IRAP or GDR picture with nuh_layer_id equal to layerId that precedes the CRA picture in decoding order.");
        }
      }
    }
  }
}

void Slice::checkSubpicTypeConstraints(PicList& rcListPic, const ReferencePictureList* pRPL0, const ReferencePictureList* pRPL1, const int prevIRAPSubpicDecOrderNo)
{
  int curSubpicIdx = getPPS()->getSubPicIdxFromSubPicId(getSliceSubPicId());

  if (getPPS()->getMixedNaluTypesInPicFlag() && getSliceType() != I_SLICE)
  {
    CHECK(!getSPS()->getSubPicTreatedAsPicFlag(curSubpicIdx), "When pps_mixed_nalu_types_in_pic_flag is equal 1, the value of sps_subpic_treated_as_pic_flag shall be equal to 1 "
          "for all the subpictures that are in the picture and contain at least one P or B slice");
  }

  int nalUnitType = getNalUnitType();
  int prevIRAPSubpicPOC = getPrevIRAPSubpicPOC();

  if (getCtuAddrInSlice(0) == getPPS()->getSubPic(curSubpicIdx).getFirstCTUInSubPic())
  {
    // subpicture type related constraints invoked only if the current slice is the first slice of a subpicture
    int prevGDRSubpicPOC = getPrevGDRSubpicPOC();
    int prevIRAPSubpicType = getPrevIRAPSubpicType();

    if (prevIRAPSubpicPOC > getPOC() && (nalUnitType < NAL_UNIT_CODED_SLICE_IDR_W_RADL || nalUnitType > NAL_UNIT_CODED_SLICE_CRA))
    {
      CHECK(nalUnitType != NAL_UNIT_CODED_SLICE_RASL && nalUnitType != NAL_UNIT_CODED_SLICE_RADL,
        "When a subpicture is a leading subpicture of an IRAP subpicture, it shall be a RADL or RASL subpicture");
    }

    if (prevIRAPSubpicPOC <= getPOC())
    {
      CHECK(nalUnitType == NAL_UNIT_CODED_SLICE_RASL || nalUnitType == NAL_UNIT_CODED_SLICE_RADL,
        "When a subpicture is not a leading subpicture of an IRAP subpicture, it shall not be a RADL or RASL subpicture");
    }

    CHECK(nalUnitType == NAL_UNIT_CODED_SLICE_RASL && (prevIRAPSubpicType == NAL_UNIT_CODED_SLICE_IDR_N_LP || prevIRAPSubpicType == NAL_UNIT_CODED_SLICE_IDR_W_RADL),
      "No RASL subpictures shall be present in the bitstream that are associated with an IDR subpicture");

    CHECK(nalUnitType == NAL_UNIT_CODED_SLICE_RADL && prevIRAPSubpicType == NAL_UNIT_CODED_SLICE_IDR_N_LP,
      "No RADL subpictures shall be present in the bitstream that are associated with an IDR subpicture having nal_unit_type equal to IDR_N_LP");

    //constraints related to current subpicture type and its preceding subpicture types
    PicList::iterator iterPic = rcListPic.begin();
    int numNonLeadingPic = 0;
    while (iterPic != rcListPic.end())
    {
      Picture* bufPic = *(iterPic++);
      if (!bufPic->reconstructed)
      {
        continue;
      }
      if (bufPic->poc == getPOC())
      {
        continue;
      }

      //identify the subpicture in the reference picture buffer that with nuh_layer_id equal to current subpicture layerId and subpicture index equal to current subpicIdx
      bool isBufPicOutput = false;
      int bufSubpicType = NAL_UNIT_INVALID;
      int bufSubpicPrevIRAPSubpicPOC = 0;

      if (bufPic->slices[0]->getPicHeader() != nullptr)   // Generated reference picture does not have picture header
      {
        for (int i = 0; i < bufPic->numSlices; i++)
        {
          if (bufPic->sliceSubpicIdx[i] == curSubpicIdx)
          {
            isBufPicOutput = bufPic->slices[i]->getPicHeader()->getPicOutputFlag();
            bufSubpicType = bufPic->slices[i]->getNalUnitType();
            bufSubpicPrevIRAPSubpicPOC = bufPic->slices[i]->getPrevIRAPSubpicPOC();
            break;
          }
        }
      }

      if ((nalUnitType == NAL_UNIT_CODED_SLICE_CRA || nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP || nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL) &&
        !this->getNoOutputOfPriorPicsFlag() && isBufPicOutput == 1 && bufPic->layerId == m_nuhLayerId)
      {
        CHECK(bufPic->poc >= getPOC(), "Any subpicture, with nuh_layer_id equal to a particular value layerId and subpicture index equal to a particular value subpicIdx, that "
          "precedes, in decoding order, an IRAP subpicture with nuh_layer_id equal to layerId and subpicture index equal to subpicIdx shall precede, in output order, the "
          "IRAP subpicture");
      }

      if (nalUnitType == NAL_UNIT_CODED_SLICE_RADL && isBufPicOutput == 1 && bufPic->layerId == m_nuhLayerId &&
        prevIRAPSubpicPOC > bufSubpicPrevIRAPSubpicPOC && prevIRAPSubpicPOC != bufPic->poc)
      {
        CHECK(bufPic->poc >= getPOC(), "Any subpicture, with nuh_layer_id equal to a particular value layerId and subpicture index equal to a particular value subpicIdx, that "
          "precedes, in decoding order, an IRAP subpicture with nuh_layer_id equal to layerId and subpicture index equal to subpicIdx shall precede, in output order, all "
          "its associated RADL subpictures");
      }

      if ((getPOC() == getPicHeader()->getRecoveryPocCnt() + prevGDRSubpicPOC) && !this->getNoOutputOfPriorPicsFlag() && isBufPicOutput == 1 &&
        bufPic->layerId == m_nuhLayerId && nalUnitType != NAL_UNIT_CODED_SLICE_GDR && getPicHeader()->getRecoveryPocCnt() != -1)
      {
        CHECK(bufPic->poc >= getPOC(), "Any subpicture, with nuh_layer_id equal to a particular value layerId and subpicture index equal to a particular value subpicIdx, that "
          "precedes, in decoding order, a subpicture with nuh_layer_id equal to layerId and subpicture index equal to subpicIdx in a recovery point picture shall precede "
          "that subpicture in the recovery point picture in output order");
      }

      if (nalUnitType == NAL_UNIT_CODED_SLICE_RASL && prevIRAPSubpicType == NAL_UNIT_CODED_SLICE_CRA && bufSubpicType == NAL_UNIT_CODED_SLICE_RADL &&
        prevIRAPSubpicPOC == bufSubpicPrevIRAPSubpicPOC)
      {
        CHECK(bufPic->poc <= getPOC(), "Any RASL subpicture associated with a CRA subpicture shall precede any RADL subpicture associated with the CRA subpicture in output order");
      }

      if (nalUnitType == NAL_UNIT_CODED_SLICE_RASL && prevIRAPSubpicType == NAL_UNIT_CODED_SLICE_CRA && bufPic->layerId == m_nuhLayerId && bufPic->poc < prevIRAPSubpicPOC)
      {
        if (bufSubpicType == NAL_UNIT_CODED_SLICE_IDR_N_LP || bufSubpicType == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
          bufSubpicType == NAL_UNIT_CODED_SLICE_CRA || bufSubpicType == NAL_UNIT_CODED_SLICE_GDR)
        {
          CHECK(bufPic->poc >= getPOC(), "Any RASL subpicture, with nuh_layer_id equal to a particular value layerId and subpicture index equal to a particular value subpicIdx, "
            "associated with a CRA subpicture shall follow, in output order, any IRAP or GDR subpicture , with nuh_layer_id equal to layerId and subpicture index equal to "
            "subpicIdx, that precedes the CRA subpicture in decoding order");
        }
      }

      if ((nalUnitType == NAL_UNIT_CODED_SLICE_RASL || nalUnitType == NAL_UNIT_CODED_SLICE_RADL) &&
        bufSubpicType != NAL_UNIT_CODED_SLICE_RASL && bufSubpicType != NAL_UNIT_CODED_SLICE_RADL &&
        bufSubpicPrevIRAPSubpicPOC == prevIRAPSubpicPOC && bufPic->layerId == m_nuhLayerId)
      {
        numNonLeadingPic++;
        int th = bufPic->cs->sps->getFieldSeqFlag() ? 1 : 0;
        CHECK(bufPic->poc > prevIRAPSubpicPOC && numNonLeadingPic > th, "If sps_field_seq_flag is equal to 0 and the current subpicture, with nuh_layer_id equal to a particular value "
          "layerId and subpicture index equal to a particular value subpicIdx, is a leading subpicture associated with an IRAP subpicture, it shall precede, in decoding order, "
          "all non-leading subpictures that are associated with the same IRAP subpicture. Otherwise, let subpicA and subpicB be the first and the last leading subpictures, in "
          "decoding order, associated with an IRAP subpicture, respectively, there shall be at most one non-leading subpicture with nuh_layer_id equal to layerId and subpicture "
          "index equal to subpicIdx preceding subpicA in decoding order, and there shall be no non-leading picture with nuh_layer_id equal to layerId and subpicture index equal "
          "to subpicIdx between picA and picB in decoding order");
      }
    }
  }

  // subpic RPL related constraints
  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const ReferencePictureList *rpl = l == REF_PIC_LIST_0 ? pRPL0 : pRPL1;

    const int numEntries       = rpl->getNumRefEntries();
    const int numActiveEntries = getNumRefIdx(l);

    for (int i = 0; i < numEntries; i++)
    {
      Picture *refPic;
      int      refPicPOC;

      if (rpl->isInterLayerRefPic(i))
      {
        const VPS *vps        = m_pcPic->cs->vps;
        const int  layerIdx   = vps->getGeneralLayerIdx(m_pcPic->layerId);
        const int  refLayerId = vps->getLayerId(vps->getDirectRefLayerIdx(layerIdx, rpl->getInterLayerRefPicIdx(i)));

        refPic    = xGetRefPic(rcListPic, getPOC(), refLayerId);
        refPicPOC = refPic->getPOC();
      }
      else if (!rpl->isRefPicLongterm(i))
      {
        refPicPOC = getPOC() + rpl->getRefPicIdentifier(i);
        refPic    = xGetRefPic(rcListPic, refPicPOC, m_pcPic->layerId);
      }
      else
      {
        int pocBits = getSPS()->getBitsForPOC();
        int pocMask = (1 << pocBits) - 1;
        int ltrpPoc = rpl->getRefPicIdentifier(i) & pocMask;
        if (rpl->getDeltaPocMSBPresentFlag(i))
        {
          ltrpPoc += getPOC() - rpl->getDeltaPocMSBCycleLT(i) * (pocMask + 1) - (getPOC() & pocMask);
        }
        refPic    = xGetLongTermRefPic(rcListPic, ltrpPoc, rpl->getDeltaPocMSBPresentFlag(i), m_pcPic->layerId);
        refPicPOC = refPic->getPOC();
      }

      // checks are for all reference pictures, but inactive reference pictures may be missing if starting with a CRA
      if (refPic != nullptr)
      {
        const int refPicDecodingOrderNumber = refPic->getDecodingOrderNumber();

        if (nalUnitType == NAL_UNIT_CODED_SLICE_CRA || nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL
            || nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP)
        {
          CHECK(refPicPOC < prevIRAPSubpicPOC || refPicDecodingOrderNumber < prevIRAPSubpicDecOrderNo,
                "When the current subpicture, with nuh_layer_id equal to a particular value layerId and subpicture "
                "index equal to a particular value subpicIdx, is an IRAP subpicture, there shall be no picture "
                "referred to by an entry in RefPicList[i] that precedes, in output order or decoding order,any "
                "preceding picture, in decoding order (when present), containing an IRAP subpicture with nuh_layer_id "
                "equal to layerId and subpicture index equal to subpicIdx");
        }

        if (prevIRAPSubpicPOC < getPOC() && !getSPS()->getFieldSeqFlag())
        {
          CHECK(refPicPOC < prevIRAPSubpicPOC || refPicDecodingOrderNumber < prevIRAPSubpicDecOrderNo,
                "When the current subpicture follows an IRAP subpicture having the same value of nuh_layer_id and the "
                "same value of subpicture index in both decoding and output order, there shall be no picture referred "
                "to by an active entry in RefPicList[ i ] that precedes the picture containing that IRAP subpicture in "
                "output order or decoding order");
        }

        if (i < numActiveEntries)
        {
          if (prevIRAPSubpicPOC < getPOC())
          {
            CHECK(refPicPOC < prevIRAPSubpicPOC || refPicDecodingOrderNumber < prevIRAPSubpicDecOrderNo,
                  "When the current subpicture follows an IRAP subpicture having the same value "
                  "of nuh_layer_id and the same value of subpicture index and the leading subpictures, if any, "
                  "associated with that IRAP subpicture in both decoding and output order, "
                  "there shall be no picture referred to by an entry in RefPicList[ i ] that precedes the picture "
                  "containing that IRAP subpicture in output order or decoding order");
          }

          if (nalUnitType == NAL_UNIT_CODED_SLICE_RADL)
          {
            CHECK(refPicDecodingOrderNumber < prevIRAPSubpicDecOrderNo,
                  "When the current subpicture, with nuh_layer_id equal to a particular value layerId and subpicture "
                  "index equal to a particular value subpicIdx, is a RADL subpicture, there shall be no active entry "
                  "in RefPicList[ i ] that is a picture that precedes the picture containing the associated IRAP "
                  "subpicture in decoding order");

            if (refPic->layerId == m_nuhLayerId)
            {
              for (int i = 0; i < refPic->numSlices; i++)
              {
                if (refPic->sliceSubpicIdx[i] == curSubpicIdx)
                {
                  CHECK(refPic->slices[i]->getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL,
                        "When the current subpicture, with nuh_layer_id equal to a particular value layerId and "
                        "subpicture index equal to a particular value subpicIdx, is a RADL subpicture, there shall be "
                        "no active entry in RefPicList[ i ] that is a picture with nuh_layer_id equal to layerId "
                        "containing a RASL subpicture with subpicture index equal to subpicIdx");
                }
              }
            }
          }
        }
      }
    }
  }
}


//Function for applying picture marking based on the Reference Picture List
void Slice::applyReferencePictureListBasedMarking( PicList& rcListPic, const ReferencePictureList *pRPL0, const ReferencePictureList *pRPL1, const int layerId, const PPS& pps ) const
{
  checkLeadingPictureRestrictions(rcListPic, pps);

  // mark long-term reference pictures in List0
  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const ReferencePictureList *rpl = l == REF_PIC_LIST_0 ? pRPL0 : pRPL1;

    for (int i = 0; i < rpl->getNumRefEntries(); i++)
    {
      if (!rpl->isRefPicLongterm(i) || rpl->isInterLayerRefPic(i))
      {
        continue;
      }

      bool isAvailable = false;
      for (const Picture *pic: rcListPic)
      {
        if (!pic->referenced)
        {
          continue;
        }
        int pocCycle = 1 << (pic->cs->sps->getBitsForPOC());
        int curPoc   = pic->getPOC();
        int refPoc   = rpl->getRefPicIdentifier(i) & (pocCycle - 1);
        if (rpl->getDeltaPocMSBPresentFlag(i))
        {
          refPoc += getPOC() - rpl->getDeltaPocMSBCycleLT(i) * pocCycle - (getPOC() & (pocCycle - 1));
        }
        else
        {
          curPoc = curPoc & (pocCycle - 1);
        }
        if (pic->longTerm && curPoc == refPoc && pic->referenced)
        {
          isAvailable = true;
          break;
        }
      }
      // if there was no such long-term check the short terms
      if (!isAvailable)
      {
        for (Picture *pic: rcListPic)
        {
          if (!pic->referenced)
          {
            continue;
          }
          int pocCycle = 1 << (pic->cs->sps->getBitsForPOC());
          int curPoc   = pic->getPOC();
          int refPoc   = rpl->getRefPicIdentifier(i) & (pocCycle - 1);
          if (rpl->getDeltaPocMSBPresentFlag(i))
          {
            refPoc += getPOC() - rpl->getDeltaPocMSBCycleLT(i) * pocCycle - (getPOC() & (pocCycle - 1));
          }
          else
          {
            curPoc = curPoc & (pocCycle - 1);
          }
          if (!pic->longTerm && curPoc == refPoc && pic->referenced)
          {
            isAvailable   = true;
            pic->longTerm = true;
            break;
          }
        }
      }
    }
  }

  if (isIDRorBLA() && !pps.getMixedNaluTypesInPicFlag())
  {
    return;
  }

  // loop through all pictures in the reference picture buffer
  for (Picture *pic: rcListPic)
  {
    if (!pic->referenced)
    {
      continue;
    }

    bool isReference = false;

    for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
    {
      const ReferencePictureList *rpl = l == REF_PIC_LIST_0 ? pRPL0 : pRPL1;

      // loop through all pictures in the Reference Picture Set
      // to see if the picture should be kept as reference picture
      for (int i = 0; !isReference && i < rpl->getNumRefEntries(); i++)
      {
        if (rpl->isInterLayerRefPic(i))
        {
          // Diagonal inter-layer prediction is not allowed
          CHECK(rpl->getRefPicIdentifier(i), "ILRP identifier should be 0");

          if (pic->poc == m_poc)
          {
            isReference   = true;
            pic->longTerm = true;
          }
        }
        else if (pic->layerId == layerId)
        {
          if (!rpl->isRefPicLongterm(i))
          {
            if (pic->poc == getPOC() + rpl->getRefPicIdentifier(i))
            {
              isReference   = true;
              pic->longTerm = false;
            }
          }
          else
          {
            const int pocCycle = 1 << pic->cs->sps->getBitsForPOC();
            const int pocMask  = pocCycle - 1;
            int       curPoc   = pic->poc;
            int       refPoc   = rpl->getRefPicIdentifier(i) & pocMask;
            if (rpl->getDeltaPocMSBPresentFlag(i))
            {
              refPoc += (getPOC() & ~pocMask) - rpl->getDeltaPocMSBCycleLT(i) * pocCycle;
            }
            else
            {
              curPoc &= pocMask;
            }
            if (pic->longTerm && curPoc == refPoc)
            {
              isReference   = true;
              pic->longTerm = true;
            }
          }
        }
      }
    }

    // mark the picture as "unused for reference" if it is not in
    // the Reference Picture List
    if (pic->layerId == layerId && pic->poc != m_poc && !isReference)
    {
      pic->referenced = false;
      pic->longTerm   = false;
    }

    // sanity checks
    if (pic->referenced)
    {
      //check that pictures of higher temporal layers are not used
      CHECK(pic->usedByCurr && pic->temporalId > this->getTLayer(), "Invalid state");
    }
  }
}

int Slice::checkThatAllRefPicsAreAvailable(PicList& rcListPic, const ReferencePictureList *pRPL, int rplIdx, bool printErrors) const
{
  Picture* rpcPic;
  int isAvailable = 0;
  int notPresentPoc = 0;

  if (this->isIDRorBLA())
  {
    return 0;   // Assume that all pic in the DPB will be flushed anyway so no need to check.
  }

  int numberOfPictures = pRPL->getNumRefEntries();
  //Check long term ref pics
  for (int ii = 0; pRPL->getNumberOfLongtermPictures() > 0 && ii < numberOfPictures; ii++)
  {
    if( !pRPL->isRefPicLongterm( ii ) || pRPL->isInterLayerRefPic( ii ) )
    {
      continue;
    }

    notPresentPoc = pRPL->getRefPicIdentifier(ii);
    isAvailable = 0;
    PicList::iterator iterPic = rcListPic.begin();
    while (iterPic != rcListPic.end())
    {
      rpcPic = *(iterPic++);
      int pocCycle = 1 << (rpcPic->cs->sps->getBitsForPOC());
      int curPoc = rpcPic->getPOC();
      int refPoc = pRPL->getRefPicIdentifier(ii) & (pocCycle - 1);
      if(pRPL->getDeltaPocMSBPresentFlag(ii))
      {
        refPoc += getPOC() - pRPL->getDeltaPocMSBCycleLT(ii) * pocCycle - (getPOC() & (pocCycle - 1));
      }
      else
      {
        curPoc = curPoc & (pocCycle - 1);
      }
      if (rpcPic->longTerm && curPoc == refPoc && rpcPic->referenced && rpcPic->reconstructed && rpcPic->layerId == m_nuhLayerId)
      {
        isAvailable = 1;
        break;
      }
    }
    // if there was no such long-term check the short terms
    if (!isAvailable)
    {
      iterPic = rcListPic.begin();
      while (iterPic != rcListPic.end())
      {
        rpcPic = *(iterPic++);
        int pocCycle = 1 << (rpcPic->cs->sps->getBitsForPOC());
        int curPoc = rpcPic->getPOC();
        int refPoc = pRPL->getRefPicIdentifier(ii) & (pocCycle - 1);
        if(pRPL->getDeltaPocMSBPresentFlag(ii))
        {
          refPoc += getPOC() - pRPL->getDeltaPocMSBCycleLT(ii) * pocCycle - (getPOC() & (pocCycle - 1));
        }
        else
        {
          curPoc = curPoc & (pocCycle - 1);
        }
        if (!rpcPic->longTerm && curPoc == refPoc && rpcPic->referenced && rpcPic->reconstructed && rpcPic->layerId == m_nuhLayerId)
        {
          isAvailable = 1;
          rpcPic->longTerm = true;
          break;
        }
      }
    }
    if (!isAvailable)
    {
      if (printErrors)
      {
        msg(ERROR, "Error: Current picture: %d Long-term reference picture with POC = %3d seems to have been removed or not correctly decoded.\n", this->getPOC(), notPresentPoc);
      }
      return notPresentPoc;
    }
  }
  //report that a picture is lost if it is in the Reference Picture List but not in the DPB

  isAvailable = 0;
  const VPS *vps = getVPS();
  //Check short term ref pics
  for (int ii = 0; ii < numberOfPictures; ii++)
  {
    if (pRPL->isRefPicLongterm(ii) && !pRPL->isInterLayerRefPic(ii) )
    {
      continue;
    }

    notPresentPoc = this->getPOC() + pRPL->getRefPicIdentifier(ii);
    isAvailable = 0;
    PicList::iterator iterPic = rcListPic.begin();
    while (iterPic != rcListPic.end())
    {
      rpcPic = *(iterPic++);
      if (!pRPL->isInterLayerRefPic(ii))
      {
        if (rpcPic->getPOC() == this->getPOC() + pRPL->getRefPicIdentifier(ii) && rpcPic->referenced && rpcPic->layerId == m_nuhLayerId)
        {
          isAvailable = 1;
          break;
        }
      }
      else
      {
        const int  refLayerId = (vps == nullptr) ? 0 : vps->getLayerId(vps->getDirectRefLayerIdx(vps->getGeneralLayerIdx(m_nuhLayerId), pRPL->getInterLayerRefPicIdx(ii)));

        if (rpcPic->getPOC() == this->getPOC() && rpcPic->referenced && rpcPic->layerId == refLayerId)
        {
          isAvailable = 1;
          break;
        }
      }
    }
    //report that a picture is lost if it is in the Reference Picture List but not in the DPB
    if (isAvailable == 0 && (pRPL->getNumberOfShorttermPictures() > 0 || pRPL->getNumberOfInterLayerPictures() > 0))
    {
      if (printErrors)
      {
        msg(ERROR, "Error: Current picture: %d Short-term reference picture with POC = %3d seems to have been removed or not correctly decoded.\n", this->getPOC(), notPresentPoc);
      }
      return notPresentPoc;
    }
  }
  return 0;
}

int Slice::checkThatAllRefPicsAreAvailable(PicList& rcListPic, const ReferencePictureList *pRPL, int rplIdx, bool printErrors, int *refPicIndex, int numActiveRefPics) const
{
  Picture* rpcPic;
  int isAvailable = 0;
  int notPresentPoc = 0;
  *refPicIndex = 0;

  if (this->isIDRorBLA()) return 0; //Assume that all pic in the DPB will be flushed anyway so no need to check.

  int numberOfPictures = numActiveRefPics;
  //Check long term ref pics
  for (int ii = 0; pRPL->getNumberOfLongtermPictures() > 0 && ii < numberOfPictures; ii++)
  {
    if( !pRPL->isRefPicLongterm( ii ) || pRPL->isInterLayerRefPic( ii ) )
    {
      continue;
    }

    notPresentPoc = pRPL->getRefPicIdentifier(ii);
    isAvailable = 0;
    PicList::iterator iterPic = rcListPic.begin();
    while (iterPic != rcListPic.end())
    {
      rpcPic = *(iterPic++);
      int pocCycle = 1 << (rpcPic->cs->sps->getBitsForPOC());
      int curPoc = rpcPic->getPOC();
      int refPoc = pRPL->getRefPicIdentifier(ii) & (pocCycle - 1);
      if(pRPL->getDeltaPocMSBPresentFlag(ii))
      {
        refPoc += getPOC() - pRPL->getDeltaPocMSBCycleLT(ii) * pocCycle - (getPOC() & (pocCycle - 1));
      }
      else
      {
        curPoc = curPoc & (pocCycle - 1);
      }
      if (rpcPic->longTerm && curPoc == refPoc && rpcPic->referenced && rpcPic->reconstructed && rpcPic->layerId == getNalUnitLayerId())
      {
        isAvailable = 1;
        break;
      }
    }
    // if there was no such long-term check the short terms
    if (!isAvailable)
    {
      iterPic = rcListPic.begin();
      while (iterPic != rcListPic.end())
      {
        rpcPic = *(iterPic++);
        int pocCycle = 1 << (rpcPic->cs->sps->getBitsForPOC());
        int curPoc = rpcPic->getPOC();
        int refPoc = pRPL->getRefPicIdentifier(ii) & (pocCycle - 1);
        if(pRPL->getDeltaPocMSBPresentFlag(ii))
        {
          refPoc += getPOC() - pRPL->getDeltaPocMSBCycleLT(ii) * pocCycle - (getPOC() & (pocCycle - 1));
        }
        else
        {
          curPoc = curPoc & (pocCycle - 1);
        }
        if (!rpcPic->longTerm && curPoc == refPoc && rpcPic->referenced && rpcPic->reconstructed && rpcPic->layerId == getNalUnitLayerId())
        {
          isAvailable = 1;
          rpcPic->longTerm = true;
          break;
        }
      }
    }
    if (!isAvailable)
    {
      if (printErrors)
      {
        msg(ERROR, "Error: Current picture: %d Long-term reference picture with POC = %3d seems to have been removed or not correctly decoded.\n", this->getPOC(), notPresentPoc);
      }
      *refPicIndex = ii;
      return notPresentPoc;
    }
  }
  //report that a picture is lost if it is in the Reference Picture List but not in the DPB

  isAvailable = 0;
  const VPS *vps = getVPS();
  //Check short term ref pics
  for (int ii = 0; ii < numberOfPictures; ii++)
  {
    if (pRPL->isRefPicLongterm(ii) && !pRPL->isInterLayerRefPic(ii) )
    {
      continue;
    }

    notPresentPoc = this->getPOC() + pRPL->getRefPicIdentifier(ii);
    isAvailable = 0;
    PicList::iterator iterPic = rcListPic.begin();
    while (iterPic != rcListPic.end())
    {
      rpcPic = *(iterPic++);
      if (!pRPL->isInterLayerRefPic(ii))
      {
        if (rpcPic->getPOC() == this->getPOC() + pRPL->getRefPicIdentifier(ii) && rpcPic->referenced && rpcPic->layerId == m_nuhLayerId)
        {
          isAvailable = 1;
          break;
        }
      }
      else
      {
        const int  refLayerId = (vps == nullptr) ? 0 : vps->getLayerId(vps->getDirectRefLayerIdx(vps->getGeneralLayerIdx(m_nuhLayerId), pRPL->getInterLayerRefPicIdx(ii)));

        if (rpcPic->getPOC() == this->getPOC() && rpcPic->referenced && rpcPic->layerId == refLayerId)
        {
          isAvailable = 1;
          break;
        }
      }
    }
    //report that a picture is lost if it is in the Reference Picture List but not in the DPB
    if (isAvailable == 0 && (pRPL->getNumberOfShorttermPictures() > 0 || pRPL->getNumberOfInterLayerPictures() > 0))
    {
      if (printErrors)
      {
        msg(ERROR, "Error: Current picture: %d Short-term reference picture with POC = %3d seems to have been removed or not correctly decoded.\n", this->getPOC(), notPresentPoc);
      }
      *refPicIndex = ii;
      return notPresentPoc;
    }
  }
  return 0;
}

bool Slice::isPOCInRefPicList(const ReferencePictureList *rpl, int poc )
{
  for (int i = 0; i < rpl->getNumRefEntries(); i++)
  {
    if( rpl->isInterLayerRefPic( i ) )
    {
      // Diagonal inter-layer prediction is not allowed
      CHECK( rpl->getRefPicIdentifier( i ), "ILRP identifier should be 0" );

      if (poc == m_poc)
      {
        return true;
      }
    }
    else if (rpl->isRefPicLongterm(i))
    {
      if (poc == rpl->getRefPicIdentifier(i))
      {
        return true;
      }
    }
    else
    {
      if (poc == getPOC() + rpl->getRefPicIdentifier(i))
      {
        return true;
      }
    }
  }
  return false;
}

bool Slice::isPocRestrictedByDRAP( int poc, bool precedingDRAPInDecodingOrder )
{
  if (!getEnableDRAPSEI())
  {
    return false;
  }
  return ( isDRAP() && poc != getAssociatedIRAPPOC() ) ||
         ( cvsHasPreviousDRAP() && getPOC() > getLatestDRAPPOC() && (precedingDRAPInDecodingOrder || poc < getLatestDRAPPOC()) );
}

bool Slice::isPocRestrictedByEdrap( int poc )
{
  if (!getEnableEdrapSEI())
  {
    return false;
  }
  return getEdrapRapId() > 0 && poc != getAssociatedIRAPPOC();
}

void Slice::checkConformanceForDRAP( uint32_t temporalId )
{
  if (!(isDRAP() || cvsHasPreviousDRAP()))
  {
    return;
  }

  if (isDRAP())
  {
    if (!(getNalUnitType() == NalUnitType::NAL_UNIT_CODED_SLICE_TRAIL ||
          getNalUnitType() == NalUnitType::NAL_UNIT_CODED_SLICE_STSA))
    {
      msg( WARNING, "Warning, non-conforming bitstream. The DRAP picture should be a trailing picture.\n");
    }
    if ( temporalId != 0)
    {
      msg( WARNING, "Warning, non-conforming bitstream. The DRAP picture shall have a temporal sublayer identifier equal to 0.\n");
    }
    for (int i = 0; i < getNumRefIdx(REF_PIC_LIST_0); i++)
    {
      if (getRefPic(REF_PIC_LIST_0,i)->getPOC() != getAssociatedIRAPPOC())
      {
        msg( WARNING, "Warning, non-conforming bitstream. The DRAP picture shall not include any pictures in the active "
                      "entries of its reference picture lists except the preceding IRAP picture in decoding order.\n");
      }
    }
    for (int i = 0; i < getNumRefIdx(REF_PIC_LIST_1); i++)
    {
      if (getRefPic(REF_PIC_LIST_1,i)->getPOC() != getAssociatedIRAPPOC())
      {
        msg( WARNING, "Warning, non-conforming bitstream. The DRAP picture shall not include any pictures in the active "
                      "entries of its reference picture lists except the preceding IRAP picture in decoding order.\n");
      }
    }
  }

  if (cvsHasPreviousDRAP() && getPOC() > getLatestDRAPPOC())
  {
    for (int i = 0; i < getNumRefIdx(REF_PIC_LIST_0); i++)
    {
      if (getRefPic(REF_PIC_LIST_0,i)->getPOC() < getLatestDRAPPOC() && getRefPic(REF_PIC_LIST_0,i)->getPOC() != getAssociatedIRAPPOC())
      {
        msg( WARNING, "Warning, non-conforming bitstream. Any picture that follows the DRAP picture in both decoding order "
                    "and output order shall not include, in the active entries of its reference picture lists, any picture "
                    "that precedes the DRAP picture in decoding order or output order, with the exception of the preceding "
                    "IRAP picture in decoding order. Problem is POC %d in RPL0.\n", getRefPic(REF_PIC_LIST_0,i)->getPOC());
      }
    }
    for (int i = 0; i < getNumRefIdx(REF_PIC_LIST_1); i++)
    {
      if (getRefPic(REF_PIC_LIST_1,i)->getPOC() < getLatestDRAPPOC() && getRefPic(REF_PIC_LIST_1,i)->getPOC() != getAssociatedIRAPPOC())
      {
        msg( WARNING, "Warning, non-conforming bitstream. Any picture that follows the DRAP picture in both decoding order "
                    "and output order shall not include, in the active entries of its reference picture lists, any picture "
                    "that precedes the DRAP picture in decoding order or output order, with the exception of the preceding "
                    "IRAP picture in decoding order. Problem is POC %d in RPL1", getRefPic(REF_PIC_LIST_1,i)->getPOC());
      }
    }
  }
}

void Slice::checkConformanceForEDRAP( uint32_t temporalId )
{
  if (!(getEdrapRapId() > 0 || cvsHasPreviousEDRAP()))
  {
    return;
  }

  if (getEdrapRapId() > 0)
  {
    if (!(getNalUnitType() == NalUnitType::NAL_UNIT_CODED_SLICE_TRAIL ||
          getNalUnitType() == NalUnitType::NAL_UNIT_CODED_SLICE_STSA))
    {
      msg( WARNING, "Warning, non-conforming bitstream. The EDRAP picture should be a trailing picture.\n");
    }
    if ( temporalId != 0)
    {
      msg( WARNING, "Warning, non-conforming bitstream. The EDRAP picture shall have a temporal sublayer identifier equal to 0.\n");
    }
    for (int i = 0; i < getNumRefIdx(REF_PIC_LIST_0); i++)
    {
      if (getRefPic(REF_PIC_LIST_0,i)->getEdrapRapId() < 0)
      {
        msg( WARNING, "Warning, non-conforming bitstream. Any picture that is in the same layer and follows the EDRAP picture in both decoding order and output order does not include, in the active entries of its reference picture lists, any picture that is in the same layer and precedes the EDRAP picture in decoding order or output order, with the exception of the referenceablePictures.\n");
      }
    }
    for (int i = 0; i < getNumRefIdx(REF_PIC_LIST_1); i++)
    {
      if (getRefPic(REF_PIC_LIST_1,i)->getEdrapRapId() < 0)
      {
        msg( WARNING, "Warning, non-conforming bitstream. Any picture that is in the same layer and follows the EDRAP picture in both decoding order and output order does not include, in the active entries of its reference picture lists, any picture that is in the same layer and precedes the EDRAP picture in decoding order or output order, with the exception of the referenceablePictures.\n");
      }
    }
  }

  if (cvsHasPreviousEDRAP() && getPOC() > getLatestEDRAPPOC() && getLatestEdrapLeadingPicDecodableFlag())
  {
    for (int i = 0; i < getNumRefIdx(REF_PIC_LIST_0); i++)
    {
      if (getRefPic(REF_PIC_LIST_0,i)->getPOC() < getLatestEDRAPPOC() && getRefPic(REF_PIC_LIST_0,i)->getEdrapRapId() < 0)
      {
        msg( WARNING, "Warning, non-conforming bitstream. Any picture that is in the same layer and follows the EDRAP picture in decoding order and precedes the EDRAP picture in output order does not include, in the active entries of its reference picture lists, any picture that is in the same layer and precedes the EDRAP picture in decoding order, with the exception of the referenceablePictures. Problem is POC %d in RPL0.\n", getRefPic(REF_PIC_LIST_0,i)->getPOC());
      }
    }
    for (int i = 0; i < getNumRefIdx(REF_PIC_LIST_1); i++)
    {
      if (getRefPic(REF_PIC_LIST_1,i)->getPOC() < getLatestEDRAPPOC() && getRefPic(REF_PIC_LIST_1,i)->getEdrapRapId() < 0)
      {
        msg( WARNING, "Warning, non-conforming bitstream. Any picture that is in the same layer and follows the EDRAP picture in decoding order and precedes the EDRAP picture in output order does not include, in the active entries of its reference picture lists, any picture that is in the same layer and precedes the EDRAP picture in decoding order, with the exception of the referenceablePictures. Problem is POC %d in RPL1\n", getRefPic(REF_PIC_LIST_1,i)->getPOC());
      }
    }
  }
}


//! get AC and DC values for weighted pred
void  Slice::getWpAcDcParam(const WPACDCParam *&wp) const
{
  wp = m_weightACDCParam;
}

//! init AC and DC values for weighted pred
void  Slice::initWpAcDcParam()
{
  for(int iComp = 0; iComp < MAX_NUM_COMPONENT; iComp++ )
  {
    m_weightACDCParam[iComp].ac = 0;
    m_weightACDCParam[iComp].dc = 0;
  }
}

//! get tables for weighted prediction
const WPScalingParam *Slice::getWpScaling(const RefPicList refPicList, const int refIdx) const
{
  CHECK(refPicList >= NUM_REF_PIC_LIST_01, "Invalid picture reference list");
  if (refIdx < 0)
  {
    return nullptr;
  }
  else
  {
    return m_weightPredTable[refPicList][refIdx];
  }
}

WPScalingParam *Slice::getWpScaling(const RefPicList refPicList, const int refIdx)
{
  CHECK(refPicList >= NUM_REF_PIC_LIST_01, "Invalid picture reference list");
  if (refIdx < 0)
  {
    return nullptr;
  }
  else
  {
    return m_weightPredTable[refPicList][refIdx];
  }
}

//! reset Default WP tables settings : no weight.
void  Slice::resetWpScaling()
{
  for ( int e=0 ; e<NUM_REF_PIC_LIST_01 ; e++ )
  {
    for ( int i=0 ; i<MAX_NUM_REF ; i++ )
    {
      for ( int yuv=0 ; yuv<MAX_NUM_COMPONENT ; yuv++ )
      {
        WPScalingParam  *pwp = &(m_weightPredTable[e][i][yuv]);
        pwp->presentFlag     = false;
        pwp->log2WeightDenom = 0;
        pwp->log2WeightDenom = 0;
        pwp->codedWeight     = 1;
        pwp->codedOffset     = 0;
      }
    }
  }
}

//! init WP table
void  Slice::initWpScaling(const SPS *sps)
{
  const bool useHighPrecisionPredictionWeighting = sps->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag();
  for ( int e=0 ; e<NUM_REF_PIC_LIST_01 ; e++ )
  {
    for ( int i=0 ; i<MAX_NUM_REF ; i++ )
    {
      for ( int yuv=0 ; yuv<MAX_NUM_COMPONENT ; yuv++ )
      {
        WPScalingParam  *pwp = &(m_weightPredTable[e][i][yuv]);
        if (!pwp->presentFlag)
        {
          // Inferring values not present :
          pwp->codedWeight = (1 << pwp->log2WeightDenom);
          pwp->codedOffset = 0;
        }

        const int offsetScalingFactor =
          useHighPrecisionPredictionWeighting ? 1 : (1 << (sps->getBitDepth(toChannelType(ComponentID(yuv))) - 8));

        pwp->w = pwp->codedWeight;
        pwp->o = pwp->codedOffset * offsetScalingFactor;   // NOTE: This value of the ".o" variable is never used - .o
                                                           // is set immediately before it gets used
        pwp->shift = pwp->log2WeightDenom;
        pwp->round = (pwp->log2WeightDenom >= 1) ? (1 << (pwp->log2WeightDenom - 1)) : (0);
      }
    }
  }
}


void Slice::startProcessingTimer()
{
  m_iProcessingStartTime = clock();
}

void Slice::stopProcessingTimer()
{
  m_dProcessingTime += (double)(clock()-m_iProcessingStartTime) / CLOCKS_PER_SEC;
  m_iProcessingStartTime = 0;
}

unsigned Slice::getMinPictureDistance() const
{
  int minPicDist = MAX_INT;
  if (getSPS()->getIBCFlag())
  {
    minPicDist = 0;
  }
  else if (!isIntra())
  {
    const int currPOC  = getPOC();
    for (int refIdx = 0; refIdx < getNumRefIdx(REF_PIC_LIST_0); refIdx++)
    {
      if (getRefPic(REF_PIC_LIST_0, refIdx)->layerId == getNalUnitLayerId())
      {
        minPicDist = std::min( minPicDist, std::abs(currPOC - getRefPic(REF_PIC_LIST_0, refIdx)->getPOC()));
      }
    }
    if( getSliceType() == B_SLICE )
    {
      for (int refIdx = 0; refIdx < getNumRefIdx(REF_PIC_LIST_1); refIdx++)
      {
        if (getRefPic(REF_PIC_LIST_1, refIdx)->layerId == getNalUnitLayerId())
        {
          minPicDist = std::min(minPicDist, std::abs(currPOC - getRefPic(REF_PIC_LIST_1, refIdx)->getPOC()));
        }
      }
    }
  }
  return (unsigned) minPicDist;
}


// ------------------------------------------------------------------------------------------------
// Picture Header
// ------------------------------------------------------------------------------------------------

PicHeader::PicHeader()
  : m_valid(0)
  , m_nonReferencePictureFlag(0)
  , m_gdrPicFlag(0)
  , m_recoveryPocCnt(-1)
  , m_noOutputBeforeRecoveryFlag(false)
  , m_handleCraAsCvsStartFlag(false)
  , m_handleGdrAsCvsStartFlag(false)
  , m_spsId(-1)
  , m_ppsId(-1)
  , m_pocMsbPresentFlag(0)
  , m_pocMsbVal(0)
  , m_virtualBoundariesEnabledFlag(0)
  , m_virtualBoundariesPresentFlag(0)
  , m_numVerVirtualBoundaries(0)
  , m_numHorVirtualBoundaries(0)
  , m_picOutputFlag(true)
  , m_rplIdx{ 0, 0 }
  , m_splitConsOverrideFlag(0)
  , m_cuQpDeltaSubdivIntra(0)
  , m_cuQpDeltaSubdivInter(0)
  , m_cuChromaQpOffsetSubdivIntra(0)
  , m_cuChromaQpOffsetSubdivInter(0)
  , m_enableTMVPFlag(true)
  , m_picColFromL0Flag(true)
  , m_colRefIdx(0)
  , m_mvdL1ZeroFlag(0)
  , m_maxNumAffineMergeCand(AFFINE_MRG_MAX_NUM_CANDS)
  , m_disFracMMVD(0)
  , m_bdofDisabledFlag(0)
  , m_dmvrDisabledFlag(0)
  , m_profDisabledFlag(0)
  , m_jointCbCrSignFlag(0)
  , m_qpDelta(0)
  , m_numAlfApsIdsLuma(0)
  , m_alfApsIdsLuma(0)
  , m_alfApsIdChroma(0)
  , m_ccalfEnabledFlag{ false }
  , m_ccalfCbApsId(-1)
  , m_ccalfCrApsId(-1)
  , m_deblockingFilterOverrideFlag(0)
  , m_deblockingFilterDisable(0)
  , m_deblockingFilterBetaOffsetDiv2(0)
  , m_deblockingFilterTcOffsetDiv2(0)
  , m_deblockingFilterCbBetaOffsetDiv2(0)
  , m_deblockingFilterCbTcOffsetDiv2(0)
  , m_deblockingFilterCrBetaOffsetDiv2(0)
  , m_deblockingFilterCrTcOffsetDiv2(0)
  , m_lmcsEnabledFlag(0)
  , m_lmcsApsId(-1)
  , m_lmcsAps(nullptr)
  , m_lmcsChromaResidualScaleFlag(0)
  , m_explicitScalingListEnabledFlag(0)
  , m_scalingListApsId(-1)
  , m_scalingListAps(nullptr)
  , m_numWeights{ 0, 0 }
{
  memset(m_virtualBoundariesPosX,                   0,    sizeof(m_virtualBoundariesPosX));
  memset(m_virtualBoundariesPosY,                   0,    sizeof(m_virtualBoundariesPosY));
  m_saoEnabledFlag.fill(false);
  memset(m_alfEnabledFlag,                          0,    sizeof(m_alfEnabledFlag));
  memset(m_minQT,                                   0,    sizeof(m_minQT));
  memset(m_maxMTTHierarchyDepth,                    0,    sizeof(m_maxMTTHierarchyDepth));
  memset(m_maxBTSize,                               0,    sizeof(m_maxBTSize));
  memset(m_maxTTSize,                               0,    sizeof(m_maxTTSize));

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    m_rpl[l].setNumberOfActivePictures(0);
    m_rpl[l].setNumberOfShorttermPictures(0);
    m_rpl[l].setNumberOfLongtermPictures(0);
    m_rpl[l].setLtrpInSliceHeaderFlag(0);
    m_rpl[l].setNumberOfInterLayerPictures(0);
  }

  m_alfApsIdsLuma.resize(0);

  resetWpScaling();
}

PicHeader::~PicHeader()
{
  m_alfApsIdsLuma.resize(0);
}

/**
 - initialize picture header to defaut state
 */
void PicHeader::initPicHeader()
{
  m_valid                                         = 0;
  m_nonReferencePictureFlag                       = 0;
  m_gdrPicFlag                                    = 0;
  m_recoveryPocCnt                                = -1;
  m_spsId                                         = -1;
  m_ppsId                                         = -1;
  m_pocMsbPresentFlag                             = 0;
  m_pocMsbVal                                     = 0;
  m_virtualBoundariesEnabledFlag                  = 0;
  m_virtualBoundariesPresentFlag                  = 0;
  m_numVerVirtualBoundaries                       = 0;
  m_numHorVirtualBoundaries                       = 0;
  m_picOutputFlag                                 = true;
  m_rplIdx[REF_PIC_LIST_0]                        = 0;
  m_rplIdx[REF_PIC_LIST_1]                        = 0;
  m_splitConsOverrideFlag                         = 0;
  m_cuQpDeltaSubdivIntra                          = 0;
  m_cuQpDeltaSubdivInter                          = 0;
  m_cuChromaQpOffsetSubdivIntra                   = 0;
  m_cuChromaQpOffsetSubdivInter                   = 0;
  m_enableTMVPFlag                                = true;
  m_picColFromL0Flag                              = true;
  m_colRefIdx                                     = 0;
  m_mvdL1ZeroFlag                                 = 0;
  m_maxNumAffineMergeCand                         = AFFINE_MRG_MAX_NUM_CANDS;
  m_disFracMMVD                                   = 0;
  m_bdofDisabledFlag                              = 0;
  m_dmvrDisabledFlag                              = 0;
  m_profDisabledFlag                              = 0;
  m_jointCbCrSignFlag                             = 0;
  m_qpDelta                                       = 0;
  m_numAlfApsIdsLuma                              = 0;
  m_alfApsIdChroma                                = 0;
  memset(m_ccalfEnabledFlag,                        false,    sizeof(m_ccalfEnabledFlag));
  m_ccalfCbApsId                                  = -1;
  m_ccalfCrApsId                                  = -1;
  m_deblockingFilterOverrideFlag                  = 0;
  m_deblockingFilterDisable                       = 0;
  m_deblockingFilterBetaOffsetDiv2                = 0;
  m_deblockingFilterTcOffsetDiv2                  = 0;
  m_deblockingFilterCbBetaOffsetDiv2              = 0;
  m_deblockingFilterCbTcOffsetDiv2                = 0;
  m_deblockingFilterCrBetaOffsetDiv2              = 0;
  m_deblockingFilterCrTcOffsetDiv2                = 0;
  m_lmcsEnabledFlag                               = 0;
  m_lmcsApsId                                     = -1;
  m_lmcsAps                                       = nullptr;
  m_lmcsChromaResidualScaleFlag                   = 0;
  m_explicitScalingListEnabledFlag                = 0;
  m_scalingListApsId                              = -1;
  m_scalingListAps                                = nullptr;
  m_numWeights[REF_PIC_LIST_0]                    = 0;
  m_numWeights[REF_PIC_LIST_1]                    = 0;
  memset(m_virtualBoundariesPosX,                   0,    sizeof(m_virtualBoundariesPosX));
  memset(m_virtualBoundariesPosY,                   0,    sizeof(m_virtualBoundariesPosY));
  m_saoEnabledFlag.fill(false);
  memset(m_alfEnabledFlag,                          0,    sizeof(m_alfEnabledFlag));
  memset(m_minQT,                                   0,    sizeof(m_minQT));
  memset(m_maxMTTHierarchyDepth,                    0,    sizeof(m_maxMTTHierarchyDepth));
  memset(m_maxBTSize,                               0,    sizeof(m_maxBTSize));
  memset(m_maxTTSize,                               0,    sizeof(m_maxTTSize));

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    m_rpl[l].setNumberOfActivePictures(0);
    m_rpl[l].setNumberOfShorttermPictures(0);
    m_rpl[l].setNumberOfLongtermPictures(0);
    m_rpl[l].setLtrpInSliceHeaderFlag(0);
  }

  m_alfApsIdsLuma.resize(0);
#if GDR_ENABLED
  m_inGdrInterval      = false;
  m_lastGdrIntervalPoc = -1;
#endif
}

const WPScalingParam *PicHeader::getWpScaling(const RefPicList refPicList, const int refIdx) const
{
  CHECK(refPicList >= NUM_REF_PIC_LIST_01, "Invalid picture reference list");
  if (refIdx < 0)
  {
    return nullptr;
  }
  else
  {
    return m_weightPredTable[refPicList][refIdx];
  }
}

WPScalingParam *PicHeader::getWpScaling(const RefPicList refPicList, const int refIdx)
{
  CHECK(refPicList >= NUM_REF_PIC_LIST_01, "Invalid picture reference list");
  if (refIdx < 0)
  {
    return nullptr;
  }
  else
  {
    return m_weightPredTable[refPicList][refIdx];
  }
}

void PicHeader::resetWpScaling()
{
  for ( int e=0 ; e<NUM_REF_PIC_LIST_01 ; e++ )
  {
    for ( int i=0 ; i<MAX_NUM_REF ; i++ )
    {
      for ( int yuv=0 ; yuv<MAX_NUM_COMPONENT ; yuv++ )
      {
        WPScalingParam  *pwp = &(m_weightPredTable[e][i][yuv]);
        pwp->presentFlag     = false;
        pwp->log2WeightDenom = 0;
        pwp->codedWeight     = 1;
        pwp->codedOffset     = 0;
      }
    }
  }
}

APS::APS()
: m_APSId(0)
, m_temporalId( 0 )
, m_layerId( 0 )
{
}

APS::~APS()
{
}

ScalingList::ScalingList()
  : m_scalingListPredModeFlagIsCopy{ false }
  , m_scalingListDC{ 0 }
  , m_refMatrixId{ 0 } 
  , m_scalingListPreditorModeFlag{ false }
{
  m_chromaScalingListPresentFlag = true;
  for (uint32_t scalingListId = 0; scalingListId < 28; scalingListId++)
  {
    int matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : (scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;
    m_scalingListCoef[scalingListId].resize(matrixSize*matrixSize);
  }
}

/** set default quantization matrix to array
*/
void ScalingList::setDefaultScalingList()
{
  for (uint32_t scalingListId = 0; scalingListId < 28; scalingListId++)
  {
    processDefaultMatrix(scalingListId);
  }
}
/** check if use default quantization matrix
 * \returns true if the scaling list is not equal to the default quantization matrix
*/
bool ScalingList::isNotDefaultScalingList()
{
  bool isAllDefault = true;
  for (uint32_t scalingListId = 0; scalingListId < 28; scalingListId++)
  {
    int matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : (scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;
    if (scalingListId < SCALING_LIST_1D_START_16x16)
    {
      if (::memcmp(getScalingListAddress(scalingListId), getScalingListDefaultAddress(scalingListId), sizeof(int) * matrixSize * matrixSize))
      {
        isAllDefault = false;
        break;
      }
    }
    else
    {
      if ((::memcmp(getScalingListAddress(scalingListId), getScalingListDefaultAddress(scalingListId), sizeof(int) * MAX_MATRIX_COEF_NUM)) || (getScalingListDC(scalingListId) != 16))
      {
        isAllDefault = false;
        break;
      }
    }
    if (!isAllDefault)
    {
      break;
    }
  }

  return !isAllDefault;
}

int ScalingList::lengthUvlc(int code)
{
  CHECK(code < 0,        "Unsigned VLC cannot be negative");
  CHECK(code == MAX_INT, "Maximum supported UVLC code is MAX_INT-1");

  int length = 1;
  int temp = ++code;


  while (1 != temp)
  {
    temp >>= 1;
    length += 2;
  }
  return (length >> 1) + ((length + 1) >> 1);
}

int ScalingList::lengthSvlc(int code)
{
  uint32_t code2 = uint32_t(code <= 0 ? (-code) << 1 : (code << 1) - 1);
  int length = 1;
  int temp = ++code2;

  CHECK(temp < 0, "Integer overflow constructing SVLC code");

  while (1 != temp)
  {
    temp >>= 1;
    length += 2;
  }
  return (length >> 1) + ((length + 1) >> 1);
}

void ScalingList::codePredScalingList(int* scalingList, const int* scalingListPred, int scalingListDC, int scalingListPredDC, int scalingListId, int& bitsCost) //sizeId, listId is current to-be-coded matrix idx
{
  int deltaValue = 0;
  int matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : (scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;
  int coefNum = matrixSize*matrixSize;
  ScanElement *scan = g_scanOrder[SCAN_UNGROUPED][CoeffScanType::DIAG][gp_sizeIdxInfo->idxFrom(matrixSize)][gp_sizeIdxInfo->idxFrom(matrixSize)];
  int nextCoef = 0;

  int8_t data;
  const int *src = scalingList;
  const int *srcPred = scalingListPred;
  if (scalingListDC!=-1 && scalingListPredDC!=-1)
  {
    bitsCost += lengthSvlc((int8_t)(scalingListDC - scalingListPredDC - nextCoef));
    nextCoef =  scalingListDC - scalingListPredDC;
  }
  else if ((scalingListDC != -1 && scalingListPredDC == -1))
  {
    bitsCost += lengthSvlc((int8_t)(scalingListDC - srcPred[scan[0].idx] - nextCoef));
    nextCoef =  scalingListDC - srcPred[scan[0].idx];
  }
  else if ((scalingListDC == -1 && scalingListPredDC == -1))
  {
  }
  else
  {
    printf("Predictor DC mismatch! \n");
  }
  for (int i = 0; i < coefNum; i++)
  {
    if (scalingListId >= SCALING_LIST_1D_START_64x64 && scan[i].x >= 4 && scan[i].y >= 4)
    {
      continue;
    }
    deltaValue = (src[scan[i].idx] - srcPred[scan[i].idx]);
    data = (int8_t)(deltaValue - nextCoef);
    nextCoef = deltaValue;

    bitsCost += lengthSvlc(data);
  }
}

void ScalingList::codeScalingList(int* scalingList, int scalingListDC, int scalingListId, int& bitsCost) //sizeId, listId is current to-be-coded matrix idx
{
  int matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : (scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;
  int coefNum = matrixSize * matrixSize;
  ScanElement *scan = g_scanOrder[SCAN_UNGROUPED][CoeffScanType::DIAG][gp_sizeIdxInfo->idxFrom(matrixSize)][gp_sizeIdxInfo->idxFrom(matrixSize)];
  int nextCoef = SCALING_LIST_START_VALUE;
  int8_t data;
  const int *src = scalingList;

  if (scalingListId >= SCALING_LIST_1D_START_16x16)
  {
    bitsCost += lengthSvlc(int8_t(getScalingListDC(scalingListId) - nextCoef));
    nextCoef = getScalingListDC(scalingListId);
  }

  for (int i = 0; i < coefNum; i++)
  {
    if (scalingListId >= SCALING_LIST_1D_START_64x64 && scan[i].x >= 4 && scan[i].y >= 4)
    {
      continue;
    }
    data = int8_t(src[scan[i].idx] - nextCoef);
    nextCoef = src[scan[i].idx];

    bitsCost += lengthSvlc(data);
  }
}
void ScalingList::CheckBestPredScalingList(int scalingListId, int predListId, int& BitsCount)
{
  //check previously coded matrix as a predictor, code "lengthUvlc" function
  int *scalingList = getScalingListAddress(scalingListId);
  const int *scalingListPred = (scalingListId == predListId) ? ((predListId < SCALING_LIST_1D_START_8x8) ? g_quantTSDefault4x4 : g_quantIntraDefault8x8) : getScalingListAddress(predListId);
  int scalingListDC = (scalingListId >= SCALING_LIST_1D_START_16x16) ? getScalingListDC(scalingListId) : -1;
  int scalingListPredDC = (predListId >= SCALING_LIST_1D_START_16x16) ? ((scalingListId == predListId) ? 16 : getScalingListDC(predListId)) : -1;

  int bitsCost = 0;
  int matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : (scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;
  int predMatrixSize = (predListId < SCALING_LIST_1D_START_4x4) ? 2 : (predListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;

  CHECK(matrixSize != predMatrixSize, "Predictor size mismatch");

  bitsCost = 2 + lengthUvlc(scalingListId - predListId);
  //copy-flag + predictor-mode-flag + deltaListId
  codePredScalingList(scalingList, scalingListPred, scalingListDC, scalingListPredDC, scalingListId, bitsCost);
  BitsCount = bitsCost;
}

void ScalingList::processRefMatrix(uint32_t scalinListId, uint32_t refListId)
{
  int matrixSize = (scalinListId < SCALING_LIST_1D_START_4x4) ? 2 : (scalinListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;
  ::memcpy(getScalingListAddress(scalinListId), ((scalinListId == refListId) ? getScalingListDefaultAddress(refListId) : getScalingListAddress(refListId)), sizeof(int)*matrixSize*matrixSize);
}

void ScalingList::checkPredMode(uint32_t scalingListId)
{
  int bestBitsCount = MAX_INT;
  int BitsCount = 2;
  setScalingListPreditorModeFlag(scalingListId, false);
  codeScalingList(getScalingListAddress(scalingListId), ((scalingListId >= SCALING_LIST_1D_START_16x16) ? getScalingListDC(scalingListId) : -1), scalingListId, BitsCount);
  bestBitsCount = BitsCount;

  for (int predListIdx = (int)scalingListId; predListIdx >= 0; predListIdx--)
  {

    int matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : (scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;
    int predMatrixSize = (predListIdx < SCALING_LIST_1D_START_4x4) ? 2 : (predListIdx < SCALING_LIST_1D_START_8x8) ? 4 : 8;
    if (((scalingListId == SCALING_LIST_1D_START_2x2 || scalingListId == SCALING_LIST_1D_START_4x4 || scalingListId == SCALING_LIST_1D_START_8x8) && predListIdx != (int)scalingListId) || matrixSize != predMatrixSize)
    {
      continue;
    }
    const int* refScalingList = (scalingListId == predListIdx) ? getScalingListDefaultAddress(predListIdx) : getScalingListAddress(predListIdx);
    const int refDC = (predListIdx < SCALING_LIST_1D_START_16x16) ? refScalingList[0] : (scalingListId == predListIdx) ? 16 : getScalingListDC(predListIdx);
    if (!::memcmp(getScalingListAddress(scalingListId), refScalingList, sizeof(int)*matrixSize*matrixSize) // check value of matrix
      // check DC value
      && (scalingListId < SCALING_LIST_1D_START_16x16 || getScalingListDC(scalingListId) == refDC))
    {
      //copy mode
      setRefMatrixId(scalingListId, predListIdx);
      setScalingListCopyModeFlag(scalingListId, true);
      setScalingListPreditorModeFlag(scalingListId, false);
      return;
    }
    else
    {
      //predictor mode
      //use previously coded matrix as a predictor
      CheckBestPredScalingList(scalingListId, predListIdx, BitsCount);
      if (BitsCount < bestBitsCount)
      {
        bestBitsCount = BitsCount;
        setScalingListCopyModeFlag(scalingListId, false);
        setScalingListPreditorModeFlag(scalingListId, true);
        setRefMatrixId(scalingListId, predListIdx);
      }
    }
  }
  setScalingListCopyModeFlag(scalingListId, false);
}

static void outputScalingListHelp(std::ostream &os)
{
  os << "The scaling list file specifies all matrices and their DC values; none can be missing,\n"
         "but their order is arbitrary.\n\n"
         "The matrices are specified by:\n"
         "<matrix name><unchecked data>\n"
         "  <value>,<value>,<value>,....\n\n"
         "  Line-feeds can be added arbitrarily between values, and the number of values needs to be\n"
         "  at least the number of entries for the matrix (superfluous entries are ignored).\n"
         "  The <unchecked data> is text on the same line as the matrix that is not checked\n"
         "  except to ensure that the matrix name token is unique. It is recommended that it is ' ='\n"
         "  The values in the matrices are the absolute values (0-255), not the delta values as\n"
         "  exchanged between the encoder and decoder\n\n"
         "The DC values (for matrix sizes larger than 8x8) are specified by:\n"
         "<matrix name>_DC<unchecked data>\n"
         "  <value>\n";

  os << "The permitted matrix names are:\n";
  for (uint32_t sizeIdc = SCALING_LIST_2x2; sizeIdc <= SCALING_LIST_64x64; sizeIdc++)
  {
    for (uint32_t listIdc = 0; listIdc < SCALING_LIST_NUM; listIdc++)
    {
      if (!(((sizeIdc == SCALING_LIST_64x64) && (listIdc % (SCALING_LIST_NUM / SCALING_LIST_PRED_MODES) != 0)) || ((sizeIdc == SCALING_LIST_2x2) && (listIdc % (SCALING_LIST_NUM / SCALING_LIST_PRED_MODES) == 0))))
      {
        os << "  " << matrixType[sizeIdc][listIdc] << '\n';
      }
    }
  }
}

void ScalingList::outputScalingLists(std::ostream &os) const
{
  int scalingListId = 0;
  for (uint32_t sizeIdc = SCALING_LIST_2x2; sizeIdc <= SCALING_LIST_64x64; sizeIdc++)
  {
    const uint32_t size = (sizeIdc == 1) ? 2 : ((sizeIdc == 2) ? 4 : 8);
    for(uint32_t listIdc = 0; listIdc < SCALING_LIST_NUM; listIdc++)
    {
      if (!((sizeIdc== SCALING_LIST_64x64 && listIdc % (SCALING_LIST_NUM / SCALING_LIST_PRED_MODES) != 0) || (sizeIdc == SCALING_LIST_2x2 && listIdc < 4)))
      {
        const int *src = getScalingListAddress(scalingListId);
        os << (matrixType[sizeIdc][listIdc]) << " =\n  ";
        for(uint32_t y=0; y<size; y++)
        {
          for(uint32_t x=0; x<size; x++, src++)
          {
            os << std::setw(3) << (*src) << ", ";
          }
          os << (y+1<size?"\n  ":"\n");
        }
        if(sizeIdc > SCALING_LIST_8x8)
        {
          os << matrixTypeDc[sizeIdc][listIdc] << " = \n  " << std::setw(3) << getScalingListDC(scalingListId) << "\n";
        }
        os << "\n";
        scalingListId++;
      }
    }
  }
}

bool ScalingList::xParseScalingList(const std::string &fileName)
{
  static const int LINE_SIZE=1024;

  FILE *fp = nullptr;

  char line[LINE_SIZE];

  if (fileName.empty())
  {
    msg( ERROR, "Error: no scaling list file specified. Help on scaling lists being output\n");
    outputScalingListHelp(std::cout);
    std::cout << "\n\nExample scaling list file using default values:\n\n";
    outputScalingLists(std::cout);
    return true;
  }
  else if ((fp = fopen(fileName.c_str(), "r")) == nullptr)
  {
    msg( ERROR, "Error: cannot open scaling list file %s for reading\n", fileName.c_str());
    return true;
  }

  int scalingListId = 0;
  for (uint32_t sizeIdc = SCALING_LIST_2x2; sizeIdc <= SCALING_LIST_64x64; sizeIdc++)//2x2-128x128
  {
    const uint32_t size = std::min(MAX_MATRIX_COEF_NUM,(int)g_scalingListSize[sizeIdc]);

    for(uint32_t listIdc = 0; listIdc < SCALING_LIST_NUM; listIdc++)
    {

      if ((sizeIdc == SCALING_LIST_64x64 && listIdc % (SCALING_LIST_NUM / SCALING_LIST_PRED_MODES) != 0) || (sizeIdc == SCALING_LIST_2x2 && listIdc < 4))
      {
        continue;
      }
      else
      {
        int * const src = getScalingListAddress(scalingListId);
        {
          fseek(fp, 0, SEEK_SET);
          bool found = false;
          while ((!feof(fp)) && (!found))
          {
            char *ret = fgets(line, LINE_SIZE, fp);
            char *findNamePosition = ret == nullptr ? nullptr : strstr(line, matrixType[sizeIdc][listIdc]);
            // This could be a match against the DC string as well, so verify it isn't
            if (findNamePosition != nullptr
                && (matrixTypeDc[sizeIdc][listIdc] == nullptr
                    || strstr(line, matrixTypeDc[sizeIdc][listIdc]) == nullptr))
            {
              found = true;
            }
          }
          if (!found)
          {
            msg(ERROR, "Error: cannot find Matrix %s from scaling list file %s\n", matrixType[sizeIdc][listIdc],
                fileName.c_str());
            return true;

          }
        }
        for (uint32_t i=0; i<size; i++)
        {
          int data;
          if (fscanf(fp, "%d,", &data)!=1)
          {
            msg(ERROR, "Error: cannot read value #%d for Matrix %s from scaling list file %s at file position %ld\n", i,
                matrixType[sizeIdc][listIdc], fileName.c_str(), ftell(fp));
            return true;
          }
          if (data<0 || data>255)
          {
            msg(ERROR,
                "Error: QMatrix entry #%d of value %d for Matrix %s from scaling list file %s at file position %ld is "
                "out of range (0 to 255)\n",
                i, data, matrixType[sizeIdc][listIdc], fileName.c_str(), ftell(fp));
            return true;
          }
          src[i] = data;
        }

        //set DC value for default matrix check
        setScalingListDC(scalingListId, src[0]);

        if(sizeIdc > SCALING_LIST_8x8)
        {
          {
            fseek(fp, 0, SEEK_SET);
            bool found = false;
            while ((!feof(fp)) && (!found))
            {
              char *ret = fgets(line, LINE_SIZE, fp);
              char *findNamePosition = ret == nullptr ? nullptr : strstr(line, matrixTypeDc[sizeIdc][listIdc]);
              if (findNamePosition != nullptr)
              {
                // This won't be a match against the non-DC string.
                found = true;
              }
            }
            if (!found)
            {
              msg(ERROR, "Error: cannot find DC Matrix %s from scaling list file %s\n", matrixTypeDc[sizeIdc][listIdc],
                  fileName.c_str());
              return true;
            }
          }
          int data;
          if (fscanf(fp, "%d,", &data)!=1)
          {
            msg(ERROR, "Error: cannot read DC %s from scaling list file %s at file position %ld\n",
                matrixTypeDc[sizeIdc][listIdc], fileName.c_str(), ftell(fp));
            return true;
          }
          if (data<0 || data>255)
          {
            msg(ERROR,
                "Error: DC value %d for Matrix %s from scaling list file %s at file position %ld is out of range (0 to "
                "255)\n",
                data, matrixType[sizeIdc][listIdc], fileName.c_str(), ftell(fp));
            return true;
          }
          //overwrite DC value when size of matrix is larger than 16x16
          setScalingListDC(scalingListId, data);
        }
      }
      scalingListId++;
    }
  }
//  std::cout << "\n\nRead scaling lists of:\n\n";
//  outputScalingLists(std::cout);

  fclose(fp);
  return false;
}


/** get default address of quantization matrix
 * \param sizeId size index
 * \param listId list index
 * \returns pointer of quantization matrix
 */
const int* ScalingList::getScalingListDefaultAddress(uint32_t scalingListId)
{
  const int *src = 0;
  int sizeId = (scalingListId < SCALING_LIST_1D_START_8x8) ? 2 : 3;
  switch (sizeId)
  {
    case SCALING_LIST_1x1:
    case SCALING_LIST_2x2:
    case SCALING_LIST_4x4:
      src = g_quantTSDefault4x4;
      break;
    case SCALING_LIST_8x8:
    case SCALING_LIST_16x16:
    case SCALING_LIST_32x32:
    case SCALING_LIST_64x64:
    case SCALING_LIST_128x128:
      src = g_quantInterDefault8x8;
      break;
    default:
      THROW( "Invalid scaling list" );
      src = nullptr;
      break;
  }
  return src;
}

/** process of default matrix
 * \param sizeId size index
 * \param listId index of input matrix
 */
void ScalingList::processDefaultMatrix(uint32_t scalingListId)
{
  int matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : (scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;
  ::memcpy(getScalingListAddress(scalingListId), getScalingListDefaultAddress(scalingListId), sizeof(int)*matrixSize*matrixSize);
  setScalingListDC(scalingListId, SCALING_LIST_DC);
}

/** check DC value of matrix for default matrix signaling
 */
void ScalingList::checkDcOfMatrix()
{
  for (uint32_t scalingListId = 0; scalingListId < 28; scalingListId++)
  {
    //check default matrix?
    if (getScalingListDC(scalingListId) == 0)
    {
      processDefaultMatrix(scalingListId);
    }
  }
}

bool ScalingList::isLumaScalingList( int scalingListId) const
{
  return (scalingListId % MAX_NUM_COMPONENT == SCALING_LIST_1D_START_4x4 || scalingListId == SCALING_LIST_1D_START_64x64 + 1);
}

uint32_t PreCalcValues::getValIdx( const Slice &slice, const ChannelType chType ) const
{
  return slice.isIntra() ? (ISingleTree || isLuma(chType) ? 0 : 2) : 1;
}

uint32_t PreCalcValues::getMaxBtDepth( const Slice &slice, const ChannelType chType ) const
{
  if ( slice.getPicHeader()->getSplitConsOverrideFlag() )
  {
    return slice.getPicHeader()->getMaxMTTHierarchyDepth(slice.getSliceType(),
                                                         ISingleTree ? ChannelType::LUMA : chType);
  }
  else
  {
    return maxBtDepth[getValIdx(slice, chType)];
  }
}

uint32_t PreCalcValues::getMinBtSize( const Slice &slice, const ChannelType chType ) const
{
  return minBtSize[getValIdx( slice, chType )];
}

uint32_t PreCalcValues::getMaxBtSize( const Slice &slice, const ChannelType chType ) const
{
  if (slice.getPicHeader()->getSplitConsOverrideFlag())
  {
    return slice.getPicHeader()->getMaxBTSize(slice.getSliceType(), ISingleTree ? ChannelType::LUMA : chType);
  }
  else
  {
    return maxBtSize[getValIdx(slice, chType)];
  }
}

uint32_t PreCalcValues::getMinTtSize( const Slice &slice, const ChannelType chType ) const
{
  return minTtSize[getValIdx( slice, chType )];
}

uint32_t PreCalcValues::getMaxTtSize( const Slice &slice, const ChannelType chType ) const
{
  if (slice.getPicHeader()->getSplitConsOverrideFlag())
  {
    return slice.getPicHeader()->getMaxTTSize(slice.getSliceType(), ISingleTree ? ChannelType::LUMA : chType);
  }
  else
  {
    return maxTtSize[getValIdx(slice, chType)];
  }
}
uint32_t PreCalcValues::getMinQtSize( const Slice &slice, const ChannelType chType ) const
{
  if (slice.getPicHeader()->getSplitConsOverrideFlag())
  {
    return slice.getPicHeader()->getMinQTSize(slice.getSliceType(), ISingleTree ? ChannelType::LUMA : chType);
  }
  else
  {
    return minQtSize[getValIdx(slice, chType)];
  }
}

void Slice::scaleRefPicList( Picture *scaledRefPic[ ], PicHeader *picHeader, APS** apss, APS* lmcsAps, APS* scalingListAps, const bool isDecoder )
{
  int i;
  const SPS* sps = getSPS();
  const PPS* pps = getPPS();

  bool refPicIsSameRes = false;

  // this is needed for IBC
  m_pcPic->unscaledPic = m_pcPic;

  if( m_eSliceType == I_SLICE )
  {
    return;
  }

  freeScaledRefPicList( scaledRefPic );

  for( int refList = 0; refList < NUM_REF_PIC_LIST_01; refList++ )
  {
    if( refList == 1 && m_eSliceType != B_SLICE )
    {
      continue;
    }

    for( int rIdx = 0; rIdx < m_aiNumRefIdx[refList]; rIdx++ )
    {
      // if rescaling is needed, otherwise just reuse the original picture pointer; it is needed for motion field, otherwise motion field requires a copy as well
      // reference resampling for the whole picture is not applied at decoder

      CU::getRprScaling(sps, pps, m_apcRefPicList[refList][rIdx], m_scalingRatio[refList][rIdx]);

      CHECK( m_apcRefPicList[refList][rIdx]->unscaledPic == nullptr, "unscaledPic is not properly set" );

      if( m_apcRefPicList[refList][rIdx]->isRefScaled( sps, pps ) == false )
      {
        refPicIsSameRes = true;
      }

      if( m_scalingRatio[refList][rIdx] == SCALE_1X || isDecoder )
      {
        m_scaledRefPicList[refList][rIdx] = m_apcRefPicList[refList][rIdx];
      }
      else
      {
        int poc = m_apcRefPicList[refList][rIdx]->getPOC();
        int layerId = m_apcRefPicList[refList][rIdx]->layerId;

        // check whether the reference picture has already been scaled
        for( i = 0; i < MAX_NUM_REF; i++ )
        {
          if( scaledRefPic[i] != nullptr && scaledRefPic[i]->poc == poc && scaledRefPic[i]->layerId == layerId )
          {
            break;
          }
        }

        if( i == MAX_NUM_REF )
        {
          int j;
          // search for unused Picture structure in scaledRefPic
          for( j = 0; j < MAX_NUM_REF; j++ )
          {
            if( scaledRefPic[j] == nullptr )
            {
              break;
            }
          }

          CHECK( j >= MAX_NUM_REF, "scaledRefPic can not hold all reference pictures!" );

          if( j >= MAX_NUM_REF )
          {
            j = 0;
          }

          if( scaledRefPic[j] == nullptr )
          {
            scaledRefPic[j] = new Picture;

            scaledRefPic[j]->setBorderExtension( false );
            scaledRefPic[j]->reconstructed = false;
            scaledRefPic[j]->referenced = true;

            scaledRefPic[j]->finalInit( m_pcPic->cs->vps, *sps, *pps, picHeader, apss, lmcsAps, scalingListAps );

            scaledRefPic[j]->poc = NOT_VALID;

            scaledRefPic[j]->create(sps->getWrapAroundEnabledFlag(), sps->getChromaFormatIdc(), Size( pps->getPicWidthInLumaSamples(), pps->getPicHeightInLumaSamples() ), sps->getMaxCUWidth(), sps->getMaxCUWidth() + 16, isDecoder, layerId, false);
          }

          scaledRefPic[j]->poc = poc;
          scaledRefPic[j]->longTerm = m_apcRefPicList[refList][rIdx]->longTerm;

          // rescale the reference picture
          const bool downsampling = m_apcRefPicList[refList][rIdx]->getRecoBuf().Y().width >= scaledRefPic[j]->getRecoBuf().Y().width && m_apcRefPicList[refList][rIdx]->getRecoBuf().Y().height >= scaledRefPic[j]->getRecoBuf().Y().height;
          Picture::rescalePicture(m_scalingRatio[refList][rIdx], m_apcRefPicList[refList][rIdx]->getRecoBuf(),
                                  m_apcRefPicList[refList][rIdx]->slices[0]->getPPS()->getScalingWindow(),
                                  scaledRefPic[j]->getRecoBuf(), pps->getScalingWindow(), sps->getChromaFormatIdc(),
                                  sps->getBitDepths(), true, downsampling, sps->getHorCollocatedChromaFlag(),
                                  sps->getVerCollocatedChromaFlag());
          scaledRefPic[j]->unscaledPic = m_apcRefPicList[refList][rIdx];
          scaledRefPic[j]->extendPicBorder(getSPS(), getPPS() );

          m_scaledRefPicList[refList][rIdx] = scaledRefPic[j];
        }
        else
        {
          m_scaledRefPicList[refList][rIdx] = scaledRefPic[i];
        }
      }
    }
  }

  // make the scaled reference picture list as the default reference picture list
  for( int refList = 0; refList < NUM_REF_PIC_LIST_01; refList++ )
  {
    if( refList == 1 && m_eSliceType != B_SLICE )
    {
      continue;
    }

    for( int rIdx = 0; rIdx < m_aiNumRefIdx[refList]; rIdx++ )
    {
      m_savedRefPicList[refList][rIdx] = m_apcRefPicList[refList][rIdx];
      m_apcRefPicList[refList][rIdx] = m_scaledRefPicList[refList][rIdx];

      // allow the access of the unscaled version in xPredInterBlk()
      m_apcRefPicList[refList][rIdx]->unscaledPic = m_savedRefPicList[refList][rIdx];
    }
  }

  //Make sure that TMVP is disabled when there are no reference pictures with the same resolution
  if(!refPicIsSameRes)
  {
    CHECK(getPicHeader()->getEnableTMVPFlag() != 0, "TMVP cannot be enabled in pictures that have no reference pictures with the same resolution")
  }
}

void Slice::freeScaledRefPicList( Picture *scaledRefPic[] )
{
  if( m_eSliceType == I_SLICE )
  {
    return;
  }
  for( int i = 0; i < MAX_NUM_REF; i++ )
  {
    if( scaledRefPic[i] != nullptr )
    {
      scaledRefPic[i]->destroy();
      scaledRefPic[i] = nullptr;
    }
  }
}

bool Slice::checkRPR()
{
  const PPS* pps = getPPS();

  for( int refList = 0; refList < NUM_REF_PIC_LIST_01; refList++ )
  {

    if( refList == 1 && m_eSliceType != B_SLICE )
    {
      continue;
    }

    for( int rIdx = 0; rIdx < m_aiNumRefIdx[refList]; rIdx++ )
    {
      if( m_scaledRefPicList[refList][rIdx]->cs->pcv->lumaWidth != pps->getPicWidthInLumaSamples() || m_scaledRefPicList[refList][rIdx]->cs->pcv->lumaHeight != pps->getPicHeightInLumaSamples() )
      {
        return true;
      }
    }
  }

  return false;
}

bool Slice::isLastSliceInSubpic()
{
  CHECK(m_pcPPS == nullptr, "PPS pointer not initialized");

  int lastCTUAddrInSlice = m_sliceMap.getCtuAddrList().back();

  if (m_pcPPS->getNumSubPics() > 1)
  {
    const SubPic& subpic = m_pcPPS->getSubPic(m_pcPPS->getSubPicIdxFromSubPicId(getSliceSubPicId()));
    return subpic.isLastCTUinSubPic(lastCTUAddrInSlice);
  }
  else
  {
    const CodingStructure *cs = m_pcPic->cs;
    const PreCalcValues* pcv = cs->pcv;
    const uint32_t picSizeInCtus   = pcv->heightInCtus * pcv->widthInCtus;
    return lastCTUAddrInSlice == (picSizeInCtus-1);
  }
}


#if ENABLE_TRACING
void xTraceVPSHeader()
{
  DTRACE( g_trace_ctx, D_HEADER, "=========== Video Parameter Set     ===========\n" );
}

void xTraceOPIHeader()
{
  DTRACE(g_trace_ctx, D_HEADER, "=========== Operating Point Information     ===========\n");
}

void xTraceDCIHeader()
{
  DTRACE( g_trace_ctx, D_HEADER, "=========== Decoding Capability Information     ===========\n" );
}

void xTraceSPSHeader()
{
  DTRACE( g_trace_ctx, D_HEADER, "=========== Sequence Parameter Set  ===========\n" );
}

void xTracePPSHeader()
{
  DTRACE( g_trace_ctx, D_HEADER, "=========== Picture Parameter Set  ===========\n" );
}

void xTraceAPSHeader()
{
  DTRACE(g_trace_ctx, D_HEADER, "=========== Adaptation Parameter Set  ===========\n");
}

void xTracePictureHeader()
{
  DTRACE( g_trace_ctx, D_HEADER, "=========== Picture Header ===========\n" );
}

void xTraceSliceHeader()
{
  DTRACE( g_trace_ctx, D_HEADER, "=========== Slice ===========\n" );
}

void xTraceAccessUnitDelimiter()
{
  DTRACE( g_trace_ctx, D_HEADER, "=========== Access Unit Delimiter ===========\n" );
}

void xTraceFillerData ()
{
  DTRACE( g_trace_ctx, D_HEADER, "=========== Filler Data ===========\n" );
}
#endif
