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

/** \file     DecSlice.cpp
    \brief    slice decoder class
*/

#include "DecSlice.h"
#include "CommonLib/UnitTools.h"
#include "CommonLib/dtrace_next.h"

#include <vector>

//! \ingroup DecoderLib
//! \{

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DecSlice::DecSlice()
{
}

DecSlice::~DecSlice()
{
}

void DecSlice::create()
{
}

void DecSlice::destroy()
{
}

void DecSlice::init( CABACDecoder* cabacDecoder, DecCu* pcCuDecoder )
{
  m_CABACDecoder    = cabacDecoder;
  m_pcCuDecoder     = pcCuDecoder;
}

void DecSlice::decompressSlice( Slice* slice, InputBitstream* bitstream, int debugCTU )
{
  //-- For time output for each slice
  slice->startProcessingTimer();

  const SPS*     sps          = slice->getSPS();
  Picture*       pic          = slice->getPic();
  CABACReader   &cabacReader  = *m_CABACDecoder->getCABACReader(BpmType::STD);

  // setup coding structure
  CodingStructure& cs = *pic->cs;
  cs.slice            = slice;
  cs.sps              = sps;
  cs.pps              = slice->getPPS();
  memcpy(cs.alfApss, slice->getAlfAPSs(), sizeof(cs.alfApss));

  cs.lmcsAps          = slice->getPicHeader()->getLmcsAPS();
  cs.scalinglistAps   = slice->getPicHeader()->getScalingListAPS();

  cs.pcv              = slice->getPPS()->pcv;
  cs.chromaQpAdj      = 0;

  cs.picture->resizeSAO(cs.pcv->sizeInCtus, 0);

  cs.resetPrevPLT(cs.prevPLT);

  if (slice->getFirstCtuRsAddrInSlice() == 0)
  {
    cs.picture->resizeAlfData(cs.pcv->sizeInCtus);
  }

  const unsigned numSubstreams = slice->getNumberOfSubstreamSizes() + 1;

  // init each couple {EntropyDecoder, Substream}
  // Table of extracted substreams.
  std::vector<InputBitstream*> ppcSubstreams( numSubstreams );
  for( unsigned idx = 0; idx < numSubstreams; idx++ )
  {
    ppcSubstreams[idx] = bitstream->extractSubstream( idx+1 < numSubstreams ? ( slice->getSubstreamSize(idx) << 3 ) : bitstream->getNumBitsLeft() );
  }

  const unsigned  widthInCtus             = cs.pcv->widthInCtus;
  const bool     wavefrontsEnabled           = cs.sps->getEntropyCodingSyncEnabledFlag();
  const bool     entryPointPresent           = cs.sps->getEntryPointsPresentFlag();

  cabacReader.initBitstream( ppcSubstreams[0] );
  cabacReader.initCtxModels( *slice );

  // Quantization parameter
  pic->m_prevQP.fill(slice->getSliceQp());
  CHECK(pic->m_prevQP[ChannelType::LUMA] == std::numeric_limits<int>::max(), "Invalid previous QP");

  DTRACE( g_trace_ctx, D_HEADER, "=========== POC: %d ===========\n", slice->getPOC() );


  if( slice->getSliceType() != I_SLICE && slice->getRefPic( REF_PIC_LIST_0, 0 )->subPictures.size() > 1 )
  {
    clipMv = clipMvInSubpic;
  }
  else
  {
    clipMv = clipMvInPic;
  }
  // for every CTU in the slice segment...
  unsigned subStrmId = 0;
  for( unsigned ctuIdx = 0; ctuIdx < slice->getNumCtuInSlice(); ctuIdx++ )
  {
    const unsigned  ctuRsAddr       = slice->getCtuAddrInSlice(ctuIdx);
    const unsigned  ctuXPosInCtus   = ctuRsAddr % widthInCtus;
    const unsigned  ctuYPosInCtus   = ctuRsAddr / widthInCtus;
    const unsigned  tileColIdx      = slice->getPPS()->ctuToTileCol( ctuXPosInCtus );
    const unsigned  tileRowIdx      = slice->getPPS()->ctuToTileRow( ctuYPosInCtus );
    const unsigned  tileXPosInCtus  = slice->getPPS()->getTileColumnBd( tileColIdx );
    const unsigned  tileYPosInCtus  = slice->getPPS()->getTileRowBd( tileRowIdx );
    const unsigned  tileColWidth    = slice->getPPS()->getTileColumnWidth( tileColIdx );
    const unsigned  tileRowHeight   = slice->getPPS()->getTileRowHeight( tileRowIdx );
    const TileIdx   tileIdx         = slice->getPPS()->getTileIdx( ctuXPosInCtus, ctuYPosInCtus);
    const unsigned  maxCUSize             = sps->getMaxCUWidth();
    Position pos( ctuXPosInCtus*maxCUSize, ctuYPosInCtus*maxCUSize) ;
    UnitArea ctuArea(cs.area.chromaFormat, Area( pos.x, pos.y, maxCUSize, maxCUSize ) );
    const SubPic &curSubPic = slice->getPPS()->getSubPicFromPos(pos);
    // padding/restore at slice level
    if (slice->getPPS()->getNumSubPics()>=2 && curSubPic.getTreatedAsPicFlag() && ctuIdx==0)
    {
      int subPicX      = (int)curSubPic.getSubPicLeft();
      int subPicY      = (int)curSubPic.getSubPicTop();
      int subPicWidth  = (int)curSubPic.getSubPicWidthInLumaSample();
      int subPicHeight = (int)curSubPic.getSubPicHeightInLumaSample();
      for (int rlist = REF_PIC_LIST_0; rlist < NUM_REF_PIC_LIST_01; rlist++)
      {
        int n = slice->getNumRefIdx((RefPicList)rlist);
        for (int idx = 0; idx < n; idx++)
        {
          Picture *refPic = slice->getRefPic((RefPicList)rlist, idx);

          if( !refPic->getSubPicSaved() && refPic->subPictures.size() > 1 )
          {
            refPic->saveSubPicBorder(refPic->getPOC(), subPicX, subPicY, subPicWidth, subPicHeight);
            refPic->extendSubPicBorder(refPic->getPOC(), subPicX, subPicY, subPicWidth, subPicHeight);
            refPic->setSubPicSaved(true);
          }
        }
      }
    }

    DTRACE_UPDATE( g_trace_ctx, std::make_pair( "ctu", ctuRsAddr ) );

    cabacReader.initBitstream( ppcSubstreams[subStrmId] );

    // set up CABAC contexts' state for this CTU
    if( ctuXPosInCtus == tileXPosInCtus && ctuYPosInCtus == tileYPosInCtus )
    {
      if( ctuIdx != 0 ) // if it is the first CTU, then the entropy coder has already been reset
      {
        cabacReader.initCtxModels( *slice );
        cs.resetPrevPLT(cs.prevPLT);
      }
      pic->m_prevQP.fill(slice->getSliceQp());
    }
    else if( ctuXPosInCtus == tileXPosInCtus && wavefrontsEnabled )
    {
      // Synchronize cabac probabilities with top CTU if it's available and at the start of a line.
      if( ctuIdx != 0 ) // if it is the first CTU, then the entropy coder has already been reset
      {
        cabacReader.initCtxModels( *slice );
        cs.resetPrevPLT(cs.prevPLT);
      }
      if (cs.getCURestricted(pos.offset(0, -1), pos, slice->getIndependentSliceIdx(), tileIdx, ChannelType::LUMA))
      {
        // Top is available, so use it.
        cabacReader.getCtx() = m_entropyCodingSyncContextState;
        cabacReader.getCtx().riceStatReset(
          slice->getSPS()->getBitDepth(ChannelType::LUMA),
          slice->getSPS()->getSpsRangeExtension().getPersistentRiceAdaptationEnabledFlag());
        cs.setPrevPLT(m_palettePredictorSyncState);
      }
      pic->m_prevQP.fill(slice->getSliceQp());
    }

    bool updateBcwCodingOrder = cs.slice->getSliceType() == B_SLICE && ctuIdx == 0;
    if(updateBcwCodingOrder)
    {
      resetBcwCodingOrder(true, cs);
    }

    if ((cs.slice->getSliceType() != I_SLICE || cs.sps->getIBCFlag()) && ctuXPosInCtus == tileXPosInCtus)
    {
      cs.motionLut.lut.resize(0);
      cs.motionLut.lutIbc.resize(0);
      cs.resetIBCBuffer = true;
    }

    if( !cs.slice->isIntra() )
    {
      pic->mctsInfo.init( &cs, getCtuAddr( ctuArea.lumaPos(), *( cs.pcv ) ) );
    }

    if( ctuRsAddr == debugCTU )
    {
      break;
    }
    cabacReader.coding_tree_unit( cs, ctuArea, pic->m_prevQP, ctuRsAddr );

    m_pcCuDecoder->decompressCtu( cs, ctuArea );
#if GREEN_METADATA_SEI_ENABLED
    FeatureCounterStruct featureCounter = slice->getFeatureCounter();
    countFeatures( featureCounter, cs,ctuArea);
    slice->setFeatureCounter(featureCounter);
#endif
    
    if( ctuXPosInCtus == tileXPosInCtus && wavefrontsEnabled )
    {
      m_entropyCodingSyncContextState = cabacReader.getCtx();
      cs.storePrevPLT(m_palettePredictorSyncState);
    }


    if( ctuIdx == slice->getNumCtuInSlice()-1 )
    {
      unsigned binVal = cabacReader.terminating_bit();
      CHECK( !binVal, "Expecting a terminating bit" );
#if DECODER_CHECK_SUBSTREAM_AND_SLICE_TRAILING_BYTES
      cabacReader.remaining_bytes( false );
#endif
    }
    else if( ( ctuXPosInCtus + 1 == tileXPosInCtus + tileColWidth ) &&
             ( ctuYPosInCtus + 1 == tileYPosInCtus + tileRowHeight || wavefrontsEnabled ) )
    {
      // The sub-stream/stream should be terminated after this CTU.
      // (end of slice-segment, end of tile, end of wavefront-CTU-row)
      unsigned binVal = cabacReader.terminating_bit();
      CHECK( !binVal, "Expecting a terminating bit" );
      if( entryPointPresent )
      {
#if DECODER_CHECK_SUBSTREAM_AND_SLICE_TRAILING_BYTES
        cabacReader.remaining_bytes( true );
#endif
        subStrmId++;
      }
    }
    if (slice->getPPS()->getNumSubPics() >= 2 && curSubPic.getTreatedAsPicFlag() && ctuIdx == (slice->getNumCtuInSlice() - 1))
    // for last Ctu in the slice
    {
      int subPicX = (int)curSubPic.getSubPicLeft();
      int subPicY = (int)curSubPic.getSubPicTop();
      int subPicWidth = (int)curSubPic.getSubPicWidthInLumaSample();
      int subPicHeight = (int)curSubPic.getSubPicHeightInLumaSample();
      for (int rlist = REF_PIC_LIST_0; rlist < NUM_REF_PIC_LIST_01; rlist++)
      {
        int n = slice->getNumRefIdx((RefPicList)rlist);
        for (int idx = 0; idx < n; idx++)
        {
          Picture *refPic = slice->getRefPic((RefPicList)rlist, idx);
          if (refPic->getSubPicSaved())
          {
            refPic->restoreSubPicBorder(refPic->getPOC(), subPicX, subPicY, subPicWidth, subPicHeight);
            refPic->setSubPicSaved(false);
          }
        }
      }
    }
  }
  
#if GREEN_METADATA_SEI_ENABLED
  FeatureCounterStruct featureCounter = slice->getFeatureCounter();
  featureCounter.baseQP[slice->getSliceQpBase()] ++;
  if (featureCounter.isYUV400 == -1)
  {
    featureCounter.isYUV400 = sps->getChromaFormatIdc() == ChromaFormat::_400 ? 1 : 0;
    featureCounter.isYUV420 = sps->getChromaFormatIdc() == ChromaFormat::_420 ? 1 : 0;
    featureCounter.isYUV422 = sps->getChromaFormatIdc() == ChromaFormat::_422 ? 1 : 0;
    featureCounter.isYUV444 = sps->getChromaFormatIdc() == ChromaFormat::_444 ? 1 : 0;
  }
  
  if (featureCounter.is8bit == -1)
  {
    featureCounter.is8bit  = (sps->getBitDepth(ChannelType::LUMA) == 8) ? 1 : 0;
    featureCounter.is10bit = (sps->getBitDepth(ChannelType::LUMA) == 10) ? 1 : 0;
    featureCounter.is12bit = (sps->getBitDepth(ChannelType::LUMA) == 12) ? 1 : 0;
  }
  
  if (slice->getSliceType() == B_SLICE)
  {
    featureCounter.bSlices++;
  }
  else if (slice->getSliceType() == P_SLICE)
  {
    featureCounter.pSlices++;
  }
  else
  {
    featureCounter.iSlices++;
  }
  slice->setFeatureCounter(featureCounter);
#endif
  
  // deallocate all created substreams, including internal buffers.
  for( auto substr: ppcSubstreams )
  {
    delete substr;
  }
  slice->stopProcessingTimer();
}

//! \}
