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

#include <list>
#include <vector>
#include <stdio.h>
#include <fcntl.h>

#include "CommonLib/CommonDef.h"
#include "BitstreamExtractorApp.h"
#include "DecoderLib/AnnexBread.h"
#include "DecoderLib/NALread.h"
#include "EncoderLib/NALwrite.h"
#include "DecoderLib/VLCReader.h"
#include "EncoderLib/VLCWriter.h"
#include "EncoderLib/AnnexBwrite.h"

BitstreamExtractorApp::BitstreamExtractorApp()
:m_vpsId(-1)
, m_removeTimingSEI (false)

{
}

void BitstreamExtractorApp::xPrintVPSInfo (VPS *vps)
{
  msg (VERBOSE, "VPS Info: \n");
  msg (VERBOSE, "  VPS ID         : %d\n", vps->getVPSId());
  msg (VERBOSE, "  Max layers     : %d\n", vps->getMaxLayers());
  msg (VERBOSE, "  Max sub-layers : %d\n", vps->getMaxSubLayers());
  msg (VERBOSE, "  Number of OLS  : %d\n", vps->getTotalNumOLSs());
  for (int olsIdx=0; olsIdx < vps->getTotalNumOLSs(); olsIdx++)
  {
    vps->deriveTargetOutputLayerSet(olsIdx);
    msg (VERBOSE, "    OLS # %d\n", olsIdx);
    msg (VERBOSE, "      Output layers: ");
    for( int i = 0; i < vps->m_targetOutputLayerIdSet.size(); i++ )
    {
      msg (VERBOSE, "%d  ", vps->m_targetOutputLayerIdSet[i]);
    }
    msg (VERBOSE, "\n");

    msg (VERBOSE, "      Target layers: ");
    for( int i = 0; i < vps->m_targetLayerIdSet.size(); i++ )
    {
      msg (VERBOSE, "%d  ", vps->m_targetLayerIdSet[i]);
    }
    msg (VERBOSE, "\n");
  }
}

void BitstreamExtractorApp::xPrintSubPicInfo (PPS *pps)
{
  msg (VERBOSE, "Subpic Info: \n");
  msg (VERBOSE, "  SPS ID         : %d\n", pps->getSPSId());
  msg (VERBOSE, "  PPS ID         : %d\n", pps->getPPSId());
  msg (VERBOSE, "  Subpic enabled : %s\n", pps->getNumSubPics() > 1 ? "yes": "no" );
  if ( pps->getNumSubPics() > 1)
  {
    msg (VERBOSE, "    Number of subpics : %d\n", pps->getNumSubPics() );
    for (int i=0; i<pps->getNumSubPics(); i++)
    {
      SubPic subP = pps->getSubPic(i);
      msg ( VERBOSE, "      SubpicIdx #%d : TL=(%d, %d) Size CTU=(%d, %d) Size Pel=(%d, %d) SubpicID=%d\n", i, subP.getSubPicCtuTopLeftX(), subP.getSubPicCtuTopLeftY(),
            subP.getSubPicWidthInCTUs(), subP.getSubPicHeightInCTUs(), subP.getSubPicWidthInLumaSample(), subP.getSubPicHeightInLumaSample(), subP.getSubPicID());
    }
  }
}

void BitstreamExtractorApp::xReadPicHeader(InputNALUnit &nalu)
{
  m_hlSynaxReader.setBitstream(&nalu.getBitstream());
  m_hlSynaxReader.parsePictureHeader(&m_picHeader, &m_parameterSetManager, true);
  m_picHeader.setValid();
}


Slice BitstreamExtractorApp::xParseSliceHeader(InputNALUnit &nalu)
{
  m_hlSynaxReader.setBitstream(&nalu.getBitstream());
  Slice slice;
  slice.initSlice();
  slice.setNalUnitType(nalu.m_nalUnitType);
  slice.setNalUnitLayerId(nalu.m_nuhLayerId);
  slice.setTLayer(nalu.m_temporalId);

  m_hlSynaxReader.parseSliceHeader(&slice, &m_picHeader, &m_parameterSetManager, m_prevTid0Poc, m_prevPicPOC);

  return slice;
}

bool BitstreamExtractorApp::xCheckSliceSubpicture(Slice &slice, int targetSubPicId)
{
  PPS *pps = m_parameterSetManager.getPPS(m_picHeader.getPPSId());
  CHECK(nullptr == pps, "referenced PPS not found");
  SPS *sps = m_parameterSetManager.getSPS(pps->getSPSId());
  CHECK(nullptr == sps, "referenced SPS not found");

  if (sps->getSubPicInfoPresentFlag())
  {
    // subpic ID is explicitly indicated
    msg(VERBOSE, "found slice subpic id %d\n", slice.getSliceSubPicId());
    return (targetSubPicId == slice.getSliceSubPicId());
  }
  else
  {
    THROW("Subpicture signalling disbled, cannot extract.");
  }

  return true;
}

bool BitstreamExtractorApp::xCheckSEIFiller(SEIMessages SEIs, int targetSubPicId, bool &rmAllFillerInSubpicExt, bool lastSliceWritten)
{
  for (auto sei : SEIs)
  {
    if (sei->payloadType() == SEI::PayloadType::SUBPICTURE_LEVEL_INFO)
    {
      auto sli = (SEISubpictureLevelInfo*) sei;
      if (!sli->cbrConstraint)
      {
        rmAllFillerInSubpicExt = true;
      }
    }
  }
  for (auto sei : SEIs)
  {
    if (sei->payloadType() == SEI::PayloadType::FILLER_PAYLOAD)
    {
      return (rmAllFillerInSubpicExt ? false : lastSliceWritten);
    }
  }
  return true;
}

void BitstreamExtractorApp::xRewriteSPS (SPS &targetSPS, const SPS &sourceSPS, SubPic &subPic)
{
  targetSPS = sourceSPS;
  // set the number of subpicture to 1, location should not be transmited
  targetSPS.setNumSubPics(1);
  // set the target subpicture ID as first ID
  targetSPS.setSubPicIdMappingExplicitlySignalledFlag(true);
  targetSPS.setSubPicIdMappingPresentFlag(true);
  targetSPS.setSubPicId(0, subPic.getSubPicID());
  targetSPS.setMaxPicWidthInLumaSamples(subPic.getSubPicWidthInLumaSample());
  targetSPS.setMaxPicHeightInLumaSamples(subPic.getSubPicHeightInLumaSample());

  // Set the new conformance window
  Window& conf = targetSPS.getConformanceWindow();
  int subpicConfWinLeftOffset = (subPic.getSubPicCtuTopLeftX() == 0) ? conf.getWindowLeftOffset() : 0;
  int subpicConfWinRightOffset = ((subPic.getSubPicCtuTopLeftX() + subPic.getSubPicWidthInCTUs()) * sourceSPS.getCTUSize() >= sourceSPS.getMaxPicWidthInLumaSamples()) ?
                                 conf.getWindowRightOffset() : 0;
  int subpicConfWinTopOffset = (subPic.getSubPicCtuTopLeftY() == 0) ? conf.getWindowTopOffset() : 0;
  int subpicConfWinBottomOffset = ((subPic.getSubPicCtuTopLeftY() + subPic.getSubPicHeightInCTUs()) * sourceSPS.getCTUSize() >= sourceSPS.getMaxPicHeightInLumaSamples()) ?
                                  conf.getWindowBottomOffset() : 0;
  conf.setWindowLeftOffset(subpicConfWinLeftOffset);
  conf.setWindowRightOffset(subpicConfWinRightOffset);
  conf.setWindowTopOffset(subpicConfWinTopOffset);
  conf.setWindowBottomOffset(subpicConfWinBottomOffset);

  if (sourceSPS.getVirtualBoundariesEnabledFlag() && sourceSPS.getVirtualBoundariesPresentFlag())
  {
    targetSPS.setNumVerVirtualBoundaries(0);
    for (int i = 0; i < sourceSPS.getNumVerVirtualBoundaries() ; i ++)
    {
      int subPicLeftX = subPic.getSubPicCtuTopLeftX() * sourceSPS.getCTUSize();
      int subPicRightX = (subPic.getSubPicCtuTopLeftX() + subPic.getSubPicWidthInCTUs()) * sourceSPS.getCTUSize();
      if (subPicRightX > sourceSPS.getMaxPicWidthInLumaSamples())
      {
        subPicRightX = sourceSPS.getMaxPicWidthInLumaSamples();
      }
      if ( sourceSPS.getVirtualBoundariesPosX(i) > subPicLeftX && sourceSPS.getVirtualBoundariesPosX(i) < subPicRightX)
      {
        targetSPS.setVirtualBoundariesPosX(targetSPS.getNumVerVirtualBoundaries(), sourceSPS.getVirtualBoundariesPosX(i) - subPicLeftX);
        targetSPS.setNumVerVirtualBoundaries(targetSPS.getNumVerVirtualBoundaries() + 1);
      }
    }

    targetSPS.setNumHorVirtualBoundaries(0);
    for (int i = 0; i < sourceSPS.getNumHorVirtualBoundaries(); i++)
    {
      int subPicTopY = subPic.getSubPicCtuTopLeftY() * sourceSPS.getCTUSize();
      int subPicBottomY = (subPic.getSubPicCtuTopLeftY() + subPic.getSubPicHeightInCTUs()) * sourceSPS.getCTUSize();
      if (subPicBottomY > sourceSPS.getMaxPicHeightInLumaSamples())
      {
        subPicBottomY = sourceSPS.getMaxPicHeightInLumaSamples();
      }
      if (sourceSPS.getVirtualBoundariesPosY(i) > subPicTopY && sourceSPS.getVirtualBoundariesPosY(i) < subPicBottomY)
      {
        targetSPS.setVirtualBoundariesPosY(targetSPS.getNumHorVirtualBoundaries(), sourceSPS.getVirtualBoundariesPosY(i) - subPicTopY);
        targetSPS.setNumHorVirtualBoundaries(targetSPS.getNumHorVirtualBoundaries() + 1);
      }
    }
    if (targetSPS.getNumVerVirtualBoundaries() == 0 && targetSPS.getNumHorVirtualBoundaries() == 0)
    {
      targetSPS.setVirtualBoundariesEnabledFlag(0);
    }
  }
}

void BitstreamExtractorApp::xRewritePPS(PPS &targetPPS, const PPS &sourcePPS, const SPS &sourceSPS, SubPic &subPic)
{
  targetPPS = sourcePPS;

  // set number of subpictures to 1
  targetPPS.setNumSubPics(1);
  // set taget subpicture ID as first ID
  targetPPS.setSubPicId(0, subPic.getSubPicID());
  // we send the ID in the SPS, so don't sent it in the PPS (hard coded decision)
  targetPPS.setSubPicIdMappingInPpsFlag(false);
  // picture size
  targetPPS.setPicWidthInLumaSamples(subPic.getSubPicWidthInLumaSample());
  targetPPS.setPicHeightInLumaSamples(subPic.getSubPicHeightInLumaSample());
  // todo: Conformance window (conf window rewriting is not needed per JVET-S0117)
  if (!sourcePPS.getScalingWindow().isZero())
  {
  int subWidthC = SPS::getWinUnitX(sourceSPS.getChromaFormatIdc());
  int subHeightC = SPS::getWinUnitY(sourceSPS.getChromaFormatIdc());
  int subpicScalWinLeftOffset = sourcePPS.getScalingWindow().getWindowLeftOffset() - (int)subPic.getSubPicCtuTopLeftX() * sourceSPS.getCTUSize() / subWidthC;
  int rightSubpicBd = (subPic.getSubPicCtuTopLeftX() + subPic.getSubPicWidthInCTUs()) * sourceSPS.getCTUSize();
  int subpicScalWinRightOffset = rightSubpicBd >= sourceSPS.getMaxPicWidthInLumaSamples() ? sourcePPS.getScalingWindow().getWindowRightOffset() : sourcePPS.getScalingWindow().getWindowRightOffset() - (int)(sourceSPS.getMaxPicWidthInLumaSamples() - rightSubpicBd) / subWidthC;
  int subpicScalWinTopOffset = sourcePPS.getScalingWindow().getWindowTopOffset() - (int)subPic.getSubPicCtuTopLeftY() * sourceSPS.getCTUSize() / subHeightC;
  int botSubpicBd = (subPic.getSubPicCtuTopLeftY() + subPic.getSubPicHeightInCTUs()) * sourceSPS.getCTUSize();
  int subpicScalWinBotOffset = botSubpicBd >= sourceSPS.getMaxPicHeightInLumaSamples() ? sourcePPS.getScalingWindow().getWindowBottomOffset() : sourcePPS.getScalingWindow().getWindowBottomOffset() - (int)(sourceSPS.getMaxPicHeightInLumaSamples() - botSubpicBd) / subHeightC;
  Window scalingWindow(subpicScalWinLeftOffset, subpicScalWinRightOffset, subpicScalWinTopOffset,
                       subpicScalWinBotOffset);
  targetPPS.setScalingWindow(scalingWindow);
  }
  // Tiles
  int                   numTileCols = 1;
  int                   numTileRows = 1;
  std::vector<uint32_t> tileColWidth;
  std::vector<uint32_t> tileRowHeight;
  std::vector<uint32_t> tileColBd;
  std::vector<uint32_t> tileRowBd;
  int                   subpicTopLeftTileX = -1;
  int                   subpicTopLeftTileY = -1;

  for (int i=0; i<= sourcePPS.getNumTileColumns(); i++)
  {
    const int currentColBd = sourcePPS.getTileColumnBd(i);
    if ((currentColBd >= subPic.getSubPicCtuTopLeftX()) && (currentColBd <= (subPic.getSubPicCtuTopLeftX() + subPic.getSubPicWidthInCTUs())))
    {
      tileColBd.push_back(currentColBd - subPic.getSubPicCtuTopLeftX());
      if (subpicTopLeftTileX == -1)
      {
        subpicTopLeftTileX = i;
      }
    }
  }
  numTileCols=(int)tileColBd.size() - 1;
  CHECK (numTileCols < 1, "After extraction there should be at least one tile horizonally.");
  tileColWidth.resize(numTileCols);
  for (int i=0; i<numTileCols; i++)
  {
    tileColWidth[i] = tileColBd[i+1] - tileColBd[i];
  }
  targetPPS.setNumExpTileColumns(numTileCols);
  targetPPS.setNumTileColumns(numTileCols);
  targetPPS.setTileColumnWidths(tileColWidth);

  for (int i=0; i<= sourcePPS.getNumTileRows(); i++)
  {
    const int currentRowBd = sourcePPS.getTileRowBd(i);
    if ((currentRowBd >= subPic.getSubPicCtuTopLeftY()) && (currentRowBd <= (subPic.getSubPicCtuTopLeftY() + subPic.getSubPicHeightInCTUs())))
    {
      tileRowBd.push_back(currentRowBd - subPic.getSubPicCtuTopLeftY());
      if(subpicTopLeftTileY == -1)
      {
        subpicTopLeftTileY = i;
      }
    }
  }
  numTileRows=(int)tileRowBd.size() - 1;
  // if subpicture was part of a tile, top and/or bottom borders need to be added
  // note: this can only happen with vertical slice splits of a tile
  if (numTileRows < 1 )
  {
    if (tileRowBd.size()==0)
    {
      tileRowBd.push_back(0);
      tileRowBd.push_back(subPic.getSubPicHeightInCTUs());
      numTileRows+=2;
    }
    else
    {
      if (tileRowBd[0] == 0)
      {
        // top border exists, add bottom
        tileRowBd.push_back(subPic.getSubPicHeightInCTUs());
        numTileRows++;
      }
      else
      {
        // bottom border exists, add top
        const int row1 = tileRowBd[0];
        tileRowBd[0] = 0;
        tileRowBd.push_back(row1);
        numTileRows++;
      }
    }
  }
  tileRowHeight.resize(numTileRows);
  for (int i=0; i<numTileRows; i++)
  {
    tileRowHeight[i] = tileRowBd[i+1] - tileRowBd[i];
  }
  targetPPS.setNumExpTileRows(numTileRows);
  targetPPS.setNumTileRows(numTileRows);
  targetPPS.setTileRowHeights(tileRowHeight);

  // slices
  // no change reqired when each slice is one subpicture
  if (!sourcePPS.getSingleSlicePerSubPicFlag())
  {
    int targetNumSlices = subPic.getNumSlicesInSubPic();
    targetPPS.setNumSlicesInPic(targetNumSlices);
    // To avoid the bitstream writer writing pps_tile_idx_delta in the bitstream
    if ( (targetPPS.getNumSlicesInPic() - 1) <= 1)
    {
      targetPPS.setTileIdxDeltaPresentFlag(0);
    }

    for (int i=0, cnt=0; i<sourcePPS.getNumSlicesInPic(); i++)
    {
      SliceMap slMap= sourcePPS.getSliceMap(i);

      if (subPic.containsCtu(slMap.getCtuAddrInSlice(0)))
      {
        targetPPS.setSliceWidthInTiles(cnt, sourcePPS.getSliceWidthInTiles(i));
        targetPPS.setSliceHeightInTiles(cnt, sourcePPS.getSliceHeightInTiles(i));
        targetPPS.setNumSlicesInTile(cnt, sourcePPS.getNumSlicesInTile(i));
        targetPPS.setSliceHeightInCtu(cnt, sourcePPS.getSliceHeightInCtu(i));
        targetPPS.setSliceTileIdx(cnt, sourcePPS.getSliceTileIdx(i));
        cnt++;
      }
    }
    // Find out new slices tile index after removal of some tiles
    for (int i=0; i<targetPPS.getNumSlicesInPic(); i++)
    {
      int tileInPicX = targetPPS.getSliceTileIdx(i) % sourcePPS.getNumTileColumns();
      int tileInPicY = targetPPS.getSliceTileIdx(i) / sourcePPS.getNumTileColumns();
      int tileInSubpicX = tileInPicX - subpicTopLeftTileX;
      int tileInSubpicY = tileInPicY - subpicTopLeftTileY;
      targetPPS.setSliceTileIdx(i, tileInSubpicY * numTileCols + tileInSubpicX);
    }

  }

}


void BitstreamExtractorApp::xWriteVPS(VPS *vps, std::ostream& out, int layerId, int temporalId)
{
  // create a new NAL unit for output
  OutputNALUnit naluOut (NAL_UNIT_VPS, layerId, temporalId);
  CHECK( naluOut.m_temporalId, "The value of TemporalId of VPS NAL units shall be equal to 0" );

  // write the VPS to the newly created NAL unit buffer
  m_hlSyntaxWriter.setBitstream(&naluOut.m_bitstream);
  m_hlSyntaxWriter.codeVPS( vps );

  NALUnitEBSP naluWithHeader(naluOut);
  writeAnnexBNalUnit(out, naluWithHeader, true);
}

void BitstreamExtractorApp::xWriteSPS(SPS *sps, std::ostream& out, int layerId, int temporalId)
{
  // create a new NAL unit for output
  OutputNALUnit naluOut (NAL_UNIT_SPS, layerId, temporalId);
  CHECK( naluOut.m_temporalId, "The value of TemporalId of SPS NAL units shall be equal to 0" );

  // write the SPS to the newly created NAL unit buffer
  m_hlSyntaxWriter.setBitstream(&naluOut.m_bitstream);
  m_hlSyntaxWriter.codeSPS( sps );

  NALUnitEBSP naluWithHeader(naluOut);
  writeAnnexBNalUnit(out, naluWithHeader, true);
}

void BitstreamExtractorApp::xWritePPS(PPS *pps, std::ostream& out, int layerId, int temporalId)
{
  // create a new NAL unit for output
  OutputNALUnit naluOut (NAL_UNIT_PPS, layerId, temporalId);

  // write the PPS to the newly created NAL unit buffer
  m_hlSyntaxWriter.setBitstream(&naluOut.m_bitstream);
  m_hlSyntaxWriter.codePPS( pps );

  NALUnitEBSP naluWithHeader(naluOut);
  writeAnnexBNalUnit(out, naluWithHeader, true);
}


// returns true, if the NAL unit is to be discarded
bool BitstreamExtractorApp::xCheckNumSubLayers(InputNALUnit &nalu, VPS *vps)
{
  bool retval = (nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_IDR_N_LP)
                && (nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_IDR_W_RADL)
                && (nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_CRA)
                && !( (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR) && (m_picHeader.getRecoveryPocCnt() == 0) );

  retval &= nalu.m_temporalId >= vps->getNumSubLayersInLayerInOLS(m_targetOlsIdx, vps->getGeneralLayerIdx(nalu.m_nuhLayerId));

  return retval;
}

bool BitstreamExtractorApp::xCheckSEIsSubPicture(SEIMessages& SEIs, InputNALUnit& nalu, std::ostream& out, int subpicId, VPS *vps)
{
  SEIMessages scalableNestingSEIs = getSeisByType(SEIs, SEI::PayloadType::SCALABLE_NESTING);
  if (scalableNestingSEIs.size())
  {
    CHECK ( scalableNestingSEIs.size() > 1, "There shall be only one Scalable Nesting SEI in one NAL unit" );
    CHECK ( scalableNestingSEIs.size() != SEIs.size(), "Scalable Nesting SEI shall not be in the same NAL unit as other SEIs" );
    // check, if the scalable nesting SEI applies to the target subpicture
    auto sn = (SEIScalableNesting*) scalableNestingSEIs.front();

    if (sn->subpicId.empty())
    {
      // does not apply to a subpicture -> remove
      return false;
    }
    if (std::find(sn->subpicId.begin(), sn->subpicId.end(), subpicId) != sn->subpicId.end())
    {
      // C.7 step 7.c
      if (!sn->olsIdx.empty() || vps->getNumLayersInOls(m_targetOlsIdx) == 1)
      {
        // applies to target subpicture -> extract
        OutputNALUnit outNalu( nalu.m_nalUnitType, nalu.m_nuhLayerId, nalu.m_temporalId );
        m_seiWriter.writeSEImessages(outNalu.m_bitstream, sn->nestedSeis, m_hrd, false, nalu.m_temporalId);
        NALUnitEBSP naluWithHeader(outNalu);
        writeAnnexBNalUnit(out, naluWithHeader, true);
        return false;
      }
    }
    else
    {
      // does not apply to target subpicture -> remove
      return false;
    }
  }
  // remove not nested decoded picture hash SEIs
  SEIMessages hashSEI = getSeisByType(SEIs, SEI::PayloadType::DECODED_PICTURE_HASH);
  if (hashSEI.size() > 0)
  {
    return false;
  }
  // keep all other SEIs
  return true;
}


bool BitstreamExtractorApp::xIsTargetOlsIncludeAllVclLayers()
{
  std::ifstream bitstreamFileIn(m_bitstreamFileNameIn.c_str(), std::ifstream::in | std::ifstream::binary);
  if (!bitstreamFileIn)
  {
    EXIT("failed to open bitstream file " << m_bitstreamFileNameIn.c_str() << " for reading");
  }

  InputByteStream bytestream(bitstreamFileIn);

  bitstreamFileIn.clear();
  bitstreamFileIn.seekg(0, std::ios::beg);

  if (m_targetOlsIdx >= 0)
  {
    std::vector<int> layerIdInTargetOls;
    std::vector<int> layerIdInVclNalu;
    while (!!bitstreamFileIn)
    {
      AnnexBStats stats = AnnexBStats();
      InputNALUnit nalu;
      byteStreamNALUnit(bytestream, nalu.getBitstream().getFifo(), stats);

      // call actual decoding function
      if (nalu.getBitstream().getFifo().empty())
      {
        msg(WARNING, "Warning: Attempt to decode an empty NAL unit");
      }
      else
      {
        read(nalu);

        if (nalu.m_nalUnitType == NAL_UNIT_VPS)
        {
          VPS* vps = new VPS();
          m_hlSynaxReader.setBitstream(&nalu.getBitstream());
          m_hlSynaxReader.parseVPS(vps);
          int vpsId = vps->getVPSId();
          // note: storeVPS may invalidate the vps pointer!
          m_parameterSetManager.storeVPS(vps, nalu.getBitstream().getFifo());
          // get VPS back
          vps = m_parameterSetManager.getVPS(vpsId);
          m_vpsId = vps->getVPSId();
        }

        VPS *vps = nullptr;
        if (m_vpsId > 0)
        {
          vps = m_parameterSetManager.getVPS(m_vpsId);
          layerIdInTargetOls = vps->getLayerIdsInOls(m_targetOlsIdx);
          if (NALUnit::isVclNalUnitType(nalu.m_nalUnitType))
          {
            if (layerIdInVclNalu.size() == 0 || nalu.m_nuhLayerId >= layerIdInVclNalu[layerIdInVclNalu.size() - 1])
            {
              layerIdInVclNalu.push_back(nalu.m_nuhLayerId);
            }
            else
            {
              break;
            }
          }
        }
      }
    }

    //When LayerIdInOls[ targetOlsIdx ] does not include all values of nuh_layer_id in all VCL NAL units in the bitstream inBitstream
    for (int i = 0; i < layerIdInVclNalu.size(); i++)
    {
      bool vclLayerIncludedInTargetOls = std::find(layerIdInTargetOls.begin(), layerIdInTargetOls.end(), layerIdInVclNalu[i]) != layerIdInTargetOls.end();
      if (!vclLayerIncludedInTargetOls)
      {
        return false;
      }
    }
  }
  return true;
}

uint32_t BitstreamExtractorApp::decode()
{
  std::ifstream bitstreamFileIn(m_bitstreamFileNameIn.c_str(), std::ifstream::in | std::ifstream::binary);
  if (!bitstreamFileIn)
  {
    EXIT( "failed to open bitstream file " << m_bitstreamFileNameIn.c_str() << " for reading" ) ;
  }

  std::ofstream bitstreamFileOut(m_bitstreamFileNameOut.c_str(), std::ifstream::out | std::ifstream::binary);

  InputByteStream bytestream(bitstreamFileIn);

  bitstreamFileIn.clear();
  bitstreamFileIn.seekg( 0, std::ios::beg );

  bool lastSliceWritten= false;   // stores status of previous slice for associated filler data NAL units

  VPS *vpsIdZero = new VPS();
  std::vector<uint8_t> empty;
  m_parameterSetManager.storeVPS(vpsIdZero, empty);

  int subpicIdTarget[MAX_VPS_LAYERS];
  for (int i = 0; i < MAX_VPS_LAYERS; i++)
  {
    subpicIdTarget[i] = -1;
  }
  bool isVclNalUnitRemoved[MAX_VPS_LAYERS] = { false };
  bool isMultiSubpicLayer[MAX_VPS_LAYERS] = { false };
  bool rmAllFillerInSubpicExt[MAX_VPS_LAYERS] = { false };

  bool targetOlsIncludeAllVclLayers = xIsTargetOlsIncludeAllVclLayers();

  while (!!bitstreamFileIn)
  {
    AnnexBStats stats = AnnexBStats();

    InputNALUnit nalu;
    byteStreamNALUnit(bytestream, nalu.getBitstream().getFifo(), stats);

    // call actual decoding function
    if (nalu.getBitstream().getFifo().empty())
    {
      /* this can happen if the following occur:
       *  - empty input file
       *  - two back-to-back start_code_prefixes
       *  - start_code_prefix immediately followed by EOF
       */
      msg(WARNING, "Warning: Attempt to decode an empty NAL unit" );
    }
    else
    {
      read(nalu);

      bool writeInpuNalUnitToStream = true;

      // Remove NAL units with TemporalId greater than tIdTarget.
      writeInpuNalUnitToStream &= ( m_maxTemporalLayer < 0  ) || ( nalu.m_temporalId <= m_maxTemporalLayer );

      if( nalu.m_nalUnitType == NAL_UNIT_VPS )
      {
        VPS* vps = new VPS();
        m_hlSynaxReader.setBitstream( &nalu.getBitstream() );
        m_hlSynaxReader.parseVPS( vps );
        int vpsId = vps->getVPSId();
        // note: storeVPS may invalidate the vps pointer!
        m_parameterSetManager.storeVPS( vps, nalu.getBitstream().getFifo() );
        // get VPS back
        vps = m_parameterSetManager.getVPS(vpsId);
        xPrintVPSInfo(vps);
        m_vpsId = vps->getVPSId();
        // example: just write the parsed VPS back to the stream
        // *** add modifications here ***
        // only write, if not dropped earlier
        if (writeInpuNalUnitToStream)
        {
          xWriteVPS(vps, bitstreamFileOut, nalu.m_nuhLayerId, nalu.m_temporalId);
          writeInpuNalUnitToStream = false;
        }
      }

      VPS *vps = nullptr;
      bool isIncludedInTargetOls = true;
      if (m_targetOlsIdx >= 0 && m_vpsId >=0 )
      {
        // if there is no VPS nal unit, there shall be one OLS and one layer.
        if (m_vpsId == 0)
        {
          CHECK(m_targetOlsIdx != 0, "only one OLS and one layer exist, but target olsIdx is not equal to zero");
        }
        // Remove NAL units with nal_unit_type not equal to any of VPS_NUT, DPS_NUT, and EOB_NUT and with nuh_layer_id not included in the list LayerIdInOls[targetOlsIdx].
        NalUnitType t = nalu.m_nalUnitType;
        // remove from outBitstream all NAL units that have nuh_layer_id not included in the list LayerIdInOls[ targetOlsIdx ] and are not DCI, OPI, VPS, AUD, EOB or SEI NAL units
        bool isSpecialNalTypes = t == NAL_UNIT_OPI || t == NAL_UNIT_VPS || t == NAL_UNIT_DCI || t == NAL_UNIT_EOB || t == NAL_UNIT_PREFIX_SEI || t == NAL_UNIT_SUFFIX_SEI;
        vps = m_parameterSetManager.getVPS(m_vpsId);
        if (m_vpsId == 0)
        {
          // this initialization can be removed once the dummy VPS is properly initialized in the parameter set manager
          vps->deriveOutputLayerSets();
        }
        uint32_t numOlss = vps->getTotalNumOLSs();
        CHECK(m_targetOlsIdx <0  || m_targetOlsIdx >= numOlss, "target OLS shall be in the range of OLSs specified by the VPS");
        CHECK(m_maxTemporalLayer < -1 || m_maxTemporalLayer > (int)vps->getPtlMaxTemporalId(vps->getOlsPtlIdx(m_targetOlsIdx)), "MaxTemporalLayer shall either be equal -1 (for disabled) or in the range of 0 to vps_ptl_max_tid[ vps_ols_ptl_idx[ targetOlsIdx ] ], inclusive");
        std::vector<int> layerIdInOls = vps->getLayerIdsInOls(m_targetOlsIdx);
        isIncludedInTargetOls = std::find(layerIdInOls.begin(), layerIdInOls.end(), nalu.m_nuhLayerId) != layerIdInOls.end();
        writeInpuNalUnitToStream &= (isSpecialNalTypes || isIncludedInTargetOls);
        writeInpuNalUnitToStream &= !xCheckNumSubLayers(nalu, vps);
        m_removeTimingSEI = !vps->getGeneralHrdParameters()->getGeneralSamePicTimingInAllOlsFlag();
      }
      if( nalu.m_nalUnitType == NAL_UNIT_SPS )
      {
        SPS* sps = new SPS();
        m_hlSynaxReader.setBitstream( &nalu.getBitstream() );
        m_hlSynaxReader.parseSPS( sps );
        int spsId = sps->getSPSId();
        // note: storeSPS may invalidate the sps pointer!
        m_parameterSetManager.storeSPS( sps, nalu.getBitstream().getFifo() );
        // get SPS back
        sps = m_parameterSetManager.getSPS(spsId);
        msg (VERBOSE, "SPS Info: SPS ID = %d\n", spsId);

        // example: just write the parsed SPS back to the stream
        // *** add modifications here ***
        // only write, if not dropped earlier
        // rewrite the SPS
        isMultiSubpicLayer[nalu.m_nuhLayerId] = sps->getNumSubPics() > 1 ? true : false;
        if (isMultiSubpicLayer[nalu.m_nuhLayerId])
        {
          subpicIdTarget[nalu.m_nuhLayerId] = 0;
        }
        if (m_subPicIdx >= 0 && isMultiSubpicLayer[nalu.m_nuhLayerId])
        {
          CHECK(m_subPicIdx >= sps->getNumSubPics(), "Target subpicture not found");
          CHECK(!sps->getSubPicTreatedAsPicFlag(m_subPicIdx), "sps_subpic_treated_as_pic_flag[subpicIdxTarget] should be equal to 1 for subpicture extraction");
          xSetSPSUpdated(sps->getSPSId());
          writeInpuNalUnitToStream = false;
        }
        if (writeInpuNalUnitToStream)
        {
          xWriteSPS(sps, bitstreamFileOut, nalu.m_nuhLayerId, nalu.m_temporalId);
          writeInpuNalUnitToStream = false;
        }
      }

      if( nalu.m_nalUnitType == NAL_UNIT_PPS )
      {
        PPS* pps = new PPS();
        m_hlSynaxReader.setBitstream( &nalu.getBitstream() );
        m_hlSynaxReader.parsePPS( pps );
        int ppsId = pps->getPPSId();
        // note: storePPS may invalidate the pps pointer!
        m_parameterSetManager.storePPS( pps, nalu.getBitstream().getFifo() );
        // get PPS back
        pps = m_parameterSetManager.getPPS(ppsId);
        msg (VERBOSE, "PPS Info: PPS ID = %d\n", pps->getPPSId());

        SPS *sps = m_parameterSetManager.getSPS(pps->getSPSId());
        if ( nullptr == sps)
        {
          printf("Cannot find SPS referred to by PPS, ignoring");
        }
        else
        {
          if( pps->getNoPicPartitionFlag() )
          {
            pps->resetTileSliceInfo();
            pps->setLog2CtuSize( ceilLog2(sps->getCTUSize()) );
            pps->setNumExpTileColumns(1);
            pps->setNumExpTileRows(1);
            pps->addTileColumnWidth( pps->getPicWidthInCtu( ) );
            pps->addTileRowHeight( pps->getPicHeightInCtu( ) );
            pps->initTiles();
            pps->setRectSliceFlag( 1 );
            pps->setNumSlicesInPic( 1 );
            pps->initRectSlices( );
            pps->setTileIdxDeltaPresentFlag( 0 );
            pps->setSliceTileIdx( 0, 0 );
          }
          pps->initRectSliceMap(sps);
          pps->initSubPic(*sps);
          xPrintSubPicInfo (pps);
          if (m_subPicIdx >= 0 && isMultiSubpicLayer[nalu.m_nuhLayerId] && writeInpuNalUnitToStream)
          {
            SubPic subPic;
            subPic = pps->getSubPic(m_subPicIdx);
            subpicIdTarget[nalu.m_nuhLayerId] = subPic.getSubPicID();

            // if the referred SPS was updated, modify and write it
            if (xIsSPSUpdate(sps->getSPSId()))
            {
              SPS targetSPS;
              xRewriteSPS(targetSPS, *sps, subPic);
              xWriteSPS(&targetSPS, bitstreamFileOut, nalu.m_nuhLayerId, nalu.m_temporalId);
              xClearSPSUpdated(sps->getSPSId());
            }

            // rewrite the PPS
            PPS targetPPS;
            xRewritePPS(targetPPS, *pps, *sps, subPic);
            xWritePPS(&targetPPS, bitstreamFileOut, nalu.m_nuhLayerId, nalu.m_temporalId);
            writeInpuNalUnitToStream = false;
          }
        }

        // example: just write the parsed PPS back to the stream
        // *** add modifications here ***
        // only write, if not dropped earlier
        if (writeInpuNalUnitToStream)
        {
          xWritePPS(pps, bitstreamFileOut, nalu.m_nuhLayerId, nalu.m_temporalId);
          writeInpuNalUnitToStream = false;
        }
      }
      // when re-using code for slice header parsing, we need to store APSs
      if( ( nalu.m_nalUnitType == NAL_UNIT_PREFIX_APS ) || ( nalu.m_nalUnitType == NAL_UNIT_SUFFIX_APS ))
      {
        APS* aps = new APS();
        m_hlSynaxReader.setBitstream( &nalu.getBitstream() );
        m_hlSynaxReader.parseAPS( aps );
        msg (VERBOSE, "APS Info: APS ID = %d Type = %d Layer = %d\n", aps->getAPSId(), aps->getAPSType(), nalu.m_nuhLayerId);
        int apsId = aps->getAPSId();
        ApsType apsType = aps->getAPSType();
        // note: storeAPS may invalidate the aps pointer!
        m_parameterSetManager.storeAPS(aps, nalu.getBitstream().getFifo());
        // get APS back
        aps = m_parameterSetManager.getAPS(apsId, apsType);
      }

      if (nalu.m_nalUnitType == NAL_UNIT_PH)
      {
        xReadPicHeader(nalu);
      }
      if ( (nalu.m_nalUnitType == NAL_UNIT_PREFIX_SEI) || (nalu.m_nalUnitType == NAL_UNIT_SUFFIX_SEI))
      {
        // decode SEI
        SEIMessages SEIs;
        m_seiReader.parseSEImessage(&(nalu.getBitstream()), SEIs, nalu.m_nalUnitType, nalu.m_nuhLayerId, nalu.m_temporalId, vps, m_parameterSetManager.getActiveSPS(), m_hrd, &std::cout);
        if (m_targetOlsIdx>=0)
        {
          for (auto sei : SEIs)
          {
            // remove from outBitstream all NAL units that have nuh_layer_id not included in the list LayerIdInOls[ targetOlsIdx ] and ( are SEI NAL units containing (scalable-nested SEI messages) or (non-scalable-nested SEI messages with PayloadType not equal to 0, 1, 130, or 203) )
            bool isNonNestedHRDSEI = false;
            if (sei->payloadType() == SEI::PayloadType::BUFFERING_PERIOD
                || sei->payloadType() == SEI::PayloadType::PICTURE_TIMING
                || sei->payloadType() == SEI::PayloadType::DECODING_UNIT_INFO
                || sei->payloadType() == SEI::PayloadType::SUBPICTURE_LEVEL_INFO)
            {
              isNonNestedHRDSEI = true;
            }
            writeInpuNalUnitToStream &=
              isIncludedInTargetOls || (sei->payloadType() != SEI::PayloadType::SCALABLE_NESTING && isNonNestedHRDSEI);
            // remove unqualified scalable nesting SEI
            if (sei->payloadType() == SEI::PayloadType::SCALABLE_NESTING)
            {
              auto sn = (SEIScalableNesting*) sei;
              if (!sn->olsIdx.empty())
              {
                bool targetOlsIdxInNestingAppliedOls =
                  std::find(sn->olsIdx.begin(), sn->olsIdx.end(), m_targetOlsIdx) != sn->olsIdx.end();
                writeInpuNalUnitToStream &= targetOlsIdxInNestingAppliedOls;
              }
              // C.6 step 9.c
              if (writeInpuNalUnitToStream && !targetOlsIncludeAllVclLayers && sn->subpicId.empty())
              {
                if (!sn->olsIdx.empty() || vps->getNumLayersInOls(m_targetOlsIdx) == 1)
                {
                  OutputNALUnit outNalu(nalu.m_nalUnitType, nalu.m_nuhLayerId, nalu.m_temporalId);
                  m_seiWriter.writeSEImessages(outNalu.m_bitstream, sn->nestedSeis, m_hrd, false, nalu.m_temporalId);
                  NALUnitEBSP naluWithHeader(outNalu);
                  writeAnnexBNalUnit(bitstreamFileOut, naluWithHeader, true);
                  writeInpuNalUnitToStream = false;
                }
              }
            }
            // remove unqualified timing related SEI
            if (sei->payloadType() == SEI::PayloadType::BUFFERING_PERIOD
                || (m_removeTimingSEI && sei->payloadType() == SEI::PayloadType::PICTURE_TIMING)
                || sei->payloadType() == SEI::PayloadType::DECODING_UNIT_INFO
                || sei->payloadType() == SEI::PayloadType::SUBPICTURE_LEVEL_INFO)
            {
              writeInpuNalUnitToStream &= targetOlsIncludeAllVclLayers;
            }
          }
          if (m_vpsId == -1)
          {
            delete vps;
          }
        }
        writeInpuNalUnitToStream &= xCheckSEIFiller(SEIs, subpicIdTarget[nalu.m_nuhLayerId], rmAllFillerInSubpicExt[nalu.m_nuhLayerId], lastSliceWritten);
        if (writeInpuNalUnitToStream && isVclNalUnitRemoved[nalu.m_nuhLayerId] && m_subPicIdx >= 0)
        {
          writeInpuNalUnitToStream &= xCheckSEIsSubPicture(SEIs, nalu, bitstreamFileOut, subpicIdTarget[nalu.m_nuhLayerId], vps);
        }
      }

      Slice slice;
      if (nalu.isSlice())
      {
         slice = xParseSliceHeader(nalu);
      }
      if (isMultiSubpicLayer[nalu.m_nuhLayerId] && writeInpuNalUnitToStream)
      {
        if (m_subPicIdx >= 0 && nalu.isSlice())
        {
          writeInpuNalUnitToStream = xCheckSliceSubpicture(slice, subpicIdTarget[nalu.m_nuhLayerId]);
          if (!writeInpuNalUnitToStream)
          {
            isVclNalUnitRemoved[nalu.m_nuhLayerId] = true;
          }
        }
        if (nalu.m_nalUnitType == NAL_UNIT_FD)
        {
          writeInpuNalUnitToStream = rmAllFillerInSubpicExt[nalu.m_nuhLayerId] ? false : lastSliceWritten;
        }
      }
      if (nalu.isSlice() && writeInpuNalUnitToStream)
      {
        m_prevPicPOC = slice.getPOC();
      }

      if( writeInpuNalUnitToStream )
      {
        int numZeros = stats.m_numLeadingZero8BitsBytes + stats.m_numZeroByteBytes + stats.m_numStartCodePrefixBytes -1;
        // write start code
        char ch = 0;
        for( int i = 0 ; i < numZeros; i++ )
        {
          bitstreamFileOut.write( &ch, 1 );
        }
        ch = 1;
        bitstreamFileOut.write( &ch, 1 );

        // create output NAL unit
        OutputNALUnit out (nalu.m_nalUnitType, nalu.m_nuhLayerId, nalu.m_temporalId);
        out.m_bitstream.getFifo() = nalu.getBitstream().getFifo();
        // write with start code emulation prevention
        writeNaluContent (bitstreamFileOut, out);
      }

      // update status of previous slice
      if (nalu.isSlice())
      {
        if (writeInpuNalUnitToStream)
        {
          lastSliceWritten = true;
        }
        else
        {
          lastSliceWritten=false;
        }
      }
    }
  }

  return 0;
}

