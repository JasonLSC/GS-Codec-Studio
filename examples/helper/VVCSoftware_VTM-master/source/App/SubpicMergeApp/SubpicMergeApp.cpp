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

 /** \file     SubpicMergeApp.cpp
     \brief    Subpicture merge application
 */

#include <cstdio>
#include <cctype>
#include <vector>
#include <utility>
#include <fstream>
#include <sstream>
#include <ios>
#include <algorithm>
#include "CommonDef.h"
#include "VLCReader.h"
#include "AnnexBread.h"
#include "NALread.h"
#include "Slice.h"
#include "VLCWriter.h"
#include "NALwrite.h"
#include "AnnexBwrite.h"
#include "SubpicMergeApp.h"
#include "SEIread.h"
#include "SEIEncoder.h"
#include "SEIwrite.h"


 //! \ingroup SubpicMergeApp
 //! \{


static constexpr int MIXED_NALU_PPS_OFFSET = 8;


struct Subpicture {
  int                                  width;
  int                                  height;
  int                                  topLeftCornerX;
  int                                  topLeftCornerY;
  std::ifstream                        *fp;
  InputByteStream                      *bs;
  bool                                 firstSliceInPicture;
  std::vector<InputNALUnit>            nalus;
  std::vector<AnnexBStats>             stats;
  int                                  prevTid0Poc;
  bool                                 dciPresent;
  DCI                                  dci;
  ParameterSetManager                  psManager;
  std::vector<int>                     vpsIds;
  std::vector<int>                     spsIds;
  std::vector<int>                     ppsIds;
  std::vector<std::pair<int, ApsType>> apsIds;
  PicHeader                            picHeader;
  std::vector<Slice>                   slices;
  std::vector<OutputBitstream>         sliceData;
  SEI                                  *decodedPictureHashSei;
};


SubpicMergeApp::SubpicMergeApp(std::vector<SubpicParams> &subpicParams, std::ofstream &outputStream) :
  m_outputStream(outputStream),
  m_prevPicPOC(std::numeric_limits<int>::max())
{
  m_subpics = new std::vector<Subpicture>;
  m_subpics->resize(subpicParams.size());
  for (int i = 0; i < (int)subpicParams.size(); i++)
  {
    Subpicture &subpic = m_subpics->at(i);
    subpic.width          = subpicParams[i].width;
    subpic.height         = subpicParams[i].height;
    subpic.topLeftCornerX = subpicParams[i].topLeftCornerX;
    subpic.topLeftCornerY = subpicParams[i].topLeftCornerY;
    subpic.fp             = &subpicParams[i].fp;
  }

  getOutputPicSize();
}

SubpicMergeApp::~SubpicMergeApp()
{
  delete m_subpics;
}


/**
  - Compute output picture size from subpicture sizes
 */
void SubpicMergeApp::getOutputPicSize()
{
  m_picWidth  = 0;
  m_picHeight = 0;

  for (auto &subpic : *m_subpics)
  {
    m_picWidth  = std::max(m_picWidth,  subpic.topLeftCornerX + subpic.width);
    m_picHeight = std::max(m_picHeight, subpic.topLeftCornerY + subpic.height);
  }
}


/**
 - lookahead through next NAL units to determine if current NAL unit is the first NAL unit in a new picture
 */
bool SubpicMergeApp::isNewPicture(std::ifstream *bitstreamFile, InputByteStream *bytestream, bool firstSliceInPicture)
{
  bool ret = false;
  bool finished = false;

  // cannot be a new picture if there haven't been any slices yet
  if(firstSliceInPicture)
  {
    return false;
  }

  // save stream position for backup
#if RExt__DECODER_DEBUG_STATISTICS
  CodingStatistics::CodingStatisticsData* backupStats = new CodingStatistics::CodingStatisticsData(CodingStatistics::GetStatistics());
  std::streampos location = bitstreamFile->tellg() - std::streampos(bytestream->getNumBufferedBytes());
#else
  std::streampos location = bitstreamFile->tellg();
#endif

  // look ahead until picture start location is determined
  while (!finished && !!(*bitstreamFile))
  {
    AnnexBStats stats = AnnexBStats();
    InputNALUnit nalu;
    byteStreamNALUnit(*bytestream, nalu.getBitstream().getFifo(), stats);
    if (nalu.getBitstream().getFifo().empty())
    {
      msg( ERROR, "Warning: Attempt to decode an empty NAL unit\n");
    }
    else
    {
      // get next NAL unit type
      read(nalu);
      switch( nalu.m_nalUnitType ) {

        // NUT that indicate the start of a new picture
        case NAL_UNIT_ACCESS_UNIT_DELIMITER:
        case NAL_UNIT_OPI:
        case NAL_UNIT_DCI:
        case NAL_UNIT_VPS:
        case NAL_UNIT_SPS:
        case NAL_UNIT_PPS:
        case NAL_UNIT_PH:
          ret = true;
          finished = true;
          break;

        // NUT that are not the start of a new picture
        case NAL_UNIT_CODED_SLICE_TRAIL:
        case NAL_UNIT_CODED_SLICE_STSA:
        case NAL_UNIT_CODED_SLICE_RASL:
        case NAL_UNIT_CODED_SLICE_RADL:
        case NAL_UNIT_RESERVED_VCL_4:
        case NAL_UNIT_RESERVED_VCL_5:
        case NAL_UNIT_RESERVED_VCL_6:
        case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
        case NAL_UNIT_CODED_SLICE_IDR_N_LP:
        case NAL_UNIT_CODED_SLICE_CRA:
        case NAL_UNIT_CODED_SLICE_GDR:
        case NAL_UNIT_RESERVED_IRAP_VCL_11:
          ret = checkPictureHeaderInSliceHeaderFlag(nalu);
          finished = true;
          break;

          // NUT that are not the start of a new picture
        case NAL_UNIT_EOS:
        case NAL_UNIT_EOB:
        case NAL_UNIT_SUFFIX_APS:
        case NAL_UNIT_SUFFIX_SEI:
        case NAL_UNIT_FD:
          ret = false;
          finished = true;
          break;

        // NUT that might indicate the start of a new picture - keep looking
        case NAL_UNIT_PREFIX_APS:
        case NAL_UNIT_PREFIX_SEI:
        case NAL_UNIT_RESERVED_NVCL_26:
        case NAL_UNIT_RESERVED_NVCL_27:
        case NAL_UNIT_UNSPECIFIED_28:
        case NAL_UNIT_UNSPECIFIED_29:
        case NAL_UNIT_UNSPECIFIED_30:
        case NAL_UNIT_UNSPECIFIED_31:
        default:
          break;
      }
    }
  }

  // restore previous stream location - minus 3 due to the need for the annexB parser to read three extra bytes
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  bitstreamFile->clear();
  bitstreamFile->seekg(location);
  bytestream->reset();
  CodingStatistics::SetStatistics(*backupStats);
  delete backupStats;
#else
  bitstreamFile->clear();
  bitstreamFile->seekg(location-std::streamoff(3));
  bytestream->reset();
#endif

  // return TRUE if next NAL unit is the start of a new picture
  return ret;
}

/**
  - Parse DCI
*/
bool SubpicMergeApp::parseDCI(HLSyntaxReader &hlsReader, DCI &dci)
{
  hlsReader.parseDCI(&dci);
  msg( INFO, "  DCI");
  return true;
}

/**
  - Parse VPS and store it in parameter set manager
*/
int SubpicMergeApp::parseVPS(HLSyntaxReader &hlsReader, ParameterSetManager &psManager)
{
  VPS *vps = new VPS;
  hlsReader.parseVPS(vps);
  int vpsId = vps->getVPSId();
  psManager.storeVPS(vps, hlsReader.getBitstream()->getFifo());
  msg( INFO, "  VPS%i", vpsId);
  return vpsId;
}

/**
  - Parse SPS and store it in parameter set manager
*/
int SubpicMergeApp::parseSPS(HLSyntaxReader &hlsReader, ParameterSetManager &psManager)
{
  SPS *sps = new SPS;
  hlsReader.parseSPS(sps);
  int spsId = sps->getSPSId();
  psManager.storeSPS(sps, hlsReader.getBitstream()->getFifo());
  msg( INFO, "  SPS%i", spsId);
  return spsId;
}

/**
  - Parse PPS and store it in parameter set manager
*/
int SubpicMergeApp::parsePPS(HLSyntaxReader &hlsReader, ParameterSetManager &psManager)
{
  PPS *pps = new PPS;
  hlsReader.parsePPS(pps);
  int ppsId = pps->getPPSId();
  psManager.storePPS(pps, hlsReader.getBitstream()->getFifo());
  msg( INFO, "  PPS%i", ppsId);
  return ppsId;
}

/**
  - Parse APS and store it in parameter set manager
*/
void SubpicMergeApp::parseAPS(HLSyntaxReader &hlsReader, ParameterSetManager &psManager, int &apsId, int &apsType)
{
  APS *aps = new APS;
  hlsReader.parseAPS(aps);
  apsId = aps->getAPSId();
  apsType = (int)aps->getAPSType();
  psManager.storeAPS(aps, hlsReader.getBitstream()->getFifo());
  msg( INFO, "  APS%i", apsId);
}

/**
  - Parse SEI message
*/
void SubpicMergeApp::parseSEI(SEIReader &seiReader, InputNALUnit &nalu, const VPS *vps, const SPS *sps, SEI *&decodePictureHashSei)
{
  SEIMessages seis;
  HRD hrd;

  seiReader.parseSEImessage(seiReader.getBitstream(), seis, nalu.m_nalUnitType, nalu.m_nuhLayerId, nalu.m_temporalId, vps, sps, hrd, 0);

  decodePictureHashSei = nullptr;
  for (auto& s : seis)
  {
    if (s->payloadType() == SEI::PayloadType::DECODED_PICTURE_HASH)
    {
      decodePictureHashSei = s;
      break;
    }
  }
}

/**
  - Parse picture header
*/
void SubpicMergeApp::parsePictureHeader(HLSyntaxReader &hlsReader, PicHeader &picHeader, ParameterSetManager &psManager)
{
  hlsReader.parsePictureHeader(&picHeader, &psManager, true);
  picHeader.setValid();
  msg( INFO, "  PH");
}

/**
  - Parse slice header and store slice data
*/
void SubpicMergeApp::parseSliceHeader(HLSyntaxReader &hlsReader, InputNALUnit &nalu, Slice &slice, PicHeader &picHeader, OutputBitstream &sliceData, ParameterSetManager &psManager, int prevTid0Poc)
{
  slice.initSlice();
  slice.setNalUnitType(nalu.m_nalUnitType);
  slice.setTLayer(nalu.m_temporalId);
  slice.setPicHeader(&picHeader);
  hlsReader.parseSliceHeader(&slice, &picHeader, &psManager, prevTid0Poc, m_prevPicPOC);
  slice.setPPS(psManager.getPPS(picHeader.getPPSId()));
  slice.setSPS(psManager.getSPS(picHeader.getSPSId()));

  InputBitstream &inBs = nalu.getBitstream();
  CHECK(inBs.getNumBitsLeft() & 7, "Slicedata must be byte aligned");
  int numDataBytes = inBs.getNumBitsLeft() / 8;
  for (int i = 0; i < numDataBytes; i++ )
  {
    sliceData.write(inBs.readByte(), 8);
  }

  msg( INFO, "  VCL%i", slice.getPOC());
}

/**
  - Decode NAL unit if it is parameter set or picture header, or decode slice header of VLC NAL unit
 */
void SubpicMergeApp::decodeNalu(Subpicture &subpic, InputNALUnit &nalu, SEI *&decodePictureHashSei)
{
  HLSyntaxReader hlsReader;
  SEIReader seiReader;
  hlsReader.setBitstream(&nalu.getBitstream());
  seiReader.setBitstream(&nalu.getBitstream());
  int apsId;
  int apsType;

  switch (nalu.m_nalUnitType)
  {
  case NAL_UNIT_DCI:
    subpic.dciPresent = parseDCI(hlsReader, subpic.dci);
    break;
  case NAL_UNIT_VPS:
    subpic.vpsIds.push_back(parseVPS(hlsReader, subpic.psManager));
    break;
  case NAL_UNIT_SPS:
    subpic.spsIds.push_back(parseSPS(hlsReader, subpic.psManager));
    break;
  case NAL_UNIT_PPS:
    subpic.ppsIds.push_back(parsePPS(hlsReader, subpic.psManager));
    break;
  case NAL_UNIT_PREFIX_APS:
    parseAPS(hlsReader, subpic.psManager, apsId, apsType);
    subpic.apsIds.push_back(std::pair<int, ApsType>(apsId, (ApsType)apsType));
    break;
  case NAL_UNIT_PH:
    parsePictureHeader(hlsReader, subpic.picHeader, subpic.psManager);
    break;
  case NAL_UNIT_SUFFIX_SEI:
    parseSEI(seiReader, nalu, subpic.slices.front().getVPS(), subpic.slices.front().getSPS(), decodePictureHashSei);
    if (decodePictureHashSei != nullptr)
    {
      msg( INFO, "  hash SEI");
    }
    else
    {
      msg( INFO, "  suffix SEI");
    }
    break;
  default:
    if (nalu.isVcl())
    {
      subpic.slices.emplace_back();
      subpic.sliceData.emplace_back();
      parseSliceHeader(hlsReader, nalu, subpic.slices.back(), subpic.picHeader, subpic.sliceData.back(), subpic.psManager, subpic.prevTid0Poc);
    }
    else if (nalu.isSei())
    {
      msg( INFO, "  prefix SEI");
    }
    else
    {
      msg( INFO, "  ignored NALU");  // Any other NAL unit that is not handled above
    }
    break;
  }
}


/**
  - Parse NAL units of one subpicture
 */
void SubpicMergeApp::parseSubpic(Subpicture &subpic, bool &morePictures)
{
  subpic.nalus.clear();
  subpic.stats.clear();
  subpic.dciPresent = false;
  subpic.vpsIds.clear();
  subpic.spsIds.clear();
  subpic.ppsIds.clear();
  subpic.apsIds.clear();
  subpic.picHeader.initPicHeader();
  subpic.slices.clear();
  subpic.sliceData.clear();
  subpic.firstSliceInPicture = true;
  subpic.decodedPictureHashSei = nullptr;

  bool eof = false;

  while (!eof && !isNewPicture(subpic.fp, subpic.bs, subpic.firstSliceInPicture))
  {
    subpic.nalus.emplace_back();  // Add new nalu
    subpic.stats.emplace_back();  // Add new stats
    InputNALUnit &nalu = subpic.nalus.back();
    AnnexBStats &stats = subpic.stats.back();
    nalu.m_nalUnitType = NAL_UNIT_INVALID;

    // find next NAL unit in stream
    eof = byteStreamNALUnit(*subpic.bs, nalu.getBitstream().getFifo(), stats);

    if (eof)
    {
      morePictures = false;
    }

    if (nalu.getBitstream().getFifo().empty())
    {
      subpic.nalus.pop_back();  // Remove empty nalu
      subpic.stats.pop_back();
      msg( ERROR, "Warning: Attempt to decode an empty NAL unit\n");
      continue;
    }

    read(nalu);  // Convert nalu payload to RBSP and parse nalu header
    decodeNalu(subpic, nalu, subpic.decodedPictureHashSei);

    if (nalu.isVcl())
    {
      subpic.firstSliceInPicture = false;
    }
  }
}


/**
  - Create merged stream VPSes
*/
void SubpicMergeApp::generateMergedStreamVPSes(std::vector<VPS*> &vpsList)
{
  for (auto vpsId : m_subpics->at(0).vpsIds)
  {
    // Create new SPS based on the SPS from the first subpicture
    vpsList.push_back(new VPS(*m_subpics->at(0).psManager.getVPS(vpsId)));
    VPS &vps = *vpsList.back();

    for (int i = 0; i < vps.getNumOutputLayerSets(); i++)
    {
      vps.setOlsDpbPicWidth(i, m_picWidth);
      vps.setOlsDpbPicHeight(i, m_picHeight);
    }
  }
}

/**
  - Create merged stream SPSes with subpicture information
*/
void SubpicMergeApp::generateMergedStreamSPSes(std::vector<SPS*> &spsList)
{
  uint32_t numSubPics = (uint32_t)m_subpics->size();

  for (auto &subpic : *m_subpics)
  {
    for (auto spsId : subpic.spsIds)
    {
      CHECK(subpic.psManager.getSPS(spsId)->getSubPicInfoPresentFlag() && subpic.psManager.getSPS(spsId)->getNumSubPics() > 1, "Input streams containing subpictures not supported");
    }
  }

  for (auto spsId : m_subpics->at(0).spsIds)
  {
    // Create new SPS based on the SPS from the first subpicture
    spsList.push_back(new SPS(*m_subpics->at(0).psManager.getSPS(spsId)));
    SPS &sps = *spsList.back();

    sps.setMaxPicWidthInLumaSamples(m_picWidth);
    sps.setMaxPicHeightInLumaSamples(m_picHeight);

    sps.setSubPicInfoPresentFlag(true);
    sps.setNumSubPics(numSubPics);
    sps.setSubPicIdLen(ceilLog2(numSubPics));
    sps.setSubPicIdMappingExplicitlySignalledFlag(true);
    sps.setSubPicIdMappingPresentFlag(true);

    int subPicId = 0;
    for (auto &subpic : *m_subpics)
    {
      CHECK(subpic.topLeftCornerX % sps.getCTUSize(), "Subpicture top-left X is not multiple of CTU size");
      CHECK(subpic.topLeftCornerY % sps.getCTUSize(), "Subpicture top-left Y is not multiple of CTU size");
      CHECK(subpic.width % sps.getCTUSize(), "Subpicture width is not multiple of CTU size");
      CHECK(subpic.height % sps.getCTUSize(), "Subpicture height is not multiple of CTU size");
      sps.setSubPicCtuTopLeftX(subPicId, (uint32_t)(subpic.topLeftCornerX / sps.getCTUSize()));
      sps.setSubPicCtuTopLeftY(subPicId, (uint32_t)(subpic.topLeftCornerY / sps.getCTUSize()));
      sps.setSubPicWidth(subPicId, (uint32_t)(subpic.width / sps.getCTUSize()));
      sps.setSubPicHeight(subPicId, (uint32_t)(subpic.height / sps.getCTUSize()));
      sps.setSubPicTreatedAsPicFlag(subPicId, true);
      sps.setLoopFilterAcrossSubpicEnabledFlag(subPicId, false);
      sps.setSubPicId(subPicId, (uint16_t)subPicId);
      subPicId++;
    }
  }

}


/**
  - Derive tiles dimensions based on subpicture tile dimensions
*/
void SubpicMergeApp::getTileDimensions(std::vector<int> &tileWidths, std::vector<int> &tileHeights)
{
  std::vector<int> tileX;
  std::vector<int> tileY;

  // Add subpicture boundaries as tile boundaries
  for (auto &subpic : *m_subpics)
  {
    bool addTileXForCurrentSubpic = true;
    bool addTileYForCurrentSubpic = true;

    // Check if current subpic boundary need to be added as tile boundary
    for (auto &subpicScan : *m_subpics)
    {
      if (subpic.topLeftCornerX >= subpicScan.topLeftCornerX && (subpic.topLeftCornerX + subpic.width) <= (subpicScan.topLeftCornerX + subpicScan.width) && subpic.width < subpicScan.width)
      {
        addTileXForCurrentSubpic = false;
      }
      if (subpic.topLeftCornerY >= subpicScan.topLeftCornerY && (subpic.topLeftCornerY + subpic.height) <= (subpicScan.topLeftCornerY + subpicScan.height) && subpic.height < subpicScan.height)
      {
        addTileYForCurrentSubpic = false;
      }
    }

    if (addTileXForCurrentSubpic)
    {
      tileX.push_back(subpic.topLeftCornerX);
    }
    if (addTileYForCurrentSubpic)
    {
      tileY.push_back(subpic.topLeftCornerY);
    }
  }

  // Add tile boundaries from tiles within subpictures
  for (auto &subpic : *m_subpics)
  {
    const PPS &pps = *subpic.slices[0].getPPS();
    if (!pps.getNoPicPartitionFlag())
    {
      if (pps.getNumTileColumns() > 1)
      {
        int x = subpic.topLeftCornerX;
        for (int i = 0; i < pps.getNumTileColumns(); i++)
        {
          x += pps.getTileColumnWidth(i) * pps.getCtuSize();
          tileX.push_back(x);
        }
      }

      if (pps.getNumTileRows() > 1)
      {
        int y = subpic.topLeftCornerY;
        for (int i = 0; i < pps.getNumTileRows(); i++)
        {
          y += pps.getTileRowHeight(i) * pps.getCtuSize();
          tileY.push_back(y);
        }
      }
    }
  }

  tileX.push_back(m_picWidth);
  tileY.push_back(m_picHeight);

  std::sort(tileX.begin(), tileX.end());
  std::sort(tileY.begin(), tileY.end());

  tileX.erase(std::unique(tileX.begin(), tileX.end()), tileX.end());  // Remove duplicates
  tileY.erase(std::unique(tileY.begin(), tileY.end()), tileY.end());  // Remove duplicates

  tileWidths.clear();
  tileHeights.clear();

  for (auto x = tileX.begin(); x != tileX.end() - 1; x++)
  {
    tileWidths.push_back(*(x + 1) - *x);
  }
  for (auto y = tileY.begin(); y != tileY.end() - 1; y++)
  {
    tileHeights.push_back(*(y + 1) - *y);
  }
}


/**
  - Create merged stream PPSes based on the first subpicture PPSes
*/
void SubpicMergeApp::generateMergedStreamPPSes(ParameterSetManager &psManager, std::vector<PPS*> &ppsList)
{
  std::vector<int> tileWidths;
  std::vector<int> tileHeights;

  for (auto &subpic : *m_subpics)
  {
    for (auto ppsId : subpic.ppsIds)
    {
      CHECK(!subpic.psManager.getPPS(ppsId)->getScalingWindow().isZero(),
            "Scaling window in input streams not supported")
    }
  }

  getTileDimensions(tileWidths, tileHeights);

  for (auto ppsId : m_subpics->at(0).ppsIds)
  {
    // Create new PPS based on the PPS from the first subpicture
    ppsList.push_back(new PPS(*m_subpics->at(0).psManager.getPPS(ppsId)));
    PPS &pps = *ppsList.back();
    SPS &sps = *psManager.getSPS(pps.getSPSId());

    pps.setPicWidthInLumaSamples(sps.getMaxPicWidthInLumaSamples());
    pps.setPicHeightInLumaSamples(sps.getMaxPicHeightInLumaSamples());

    pps.resetTileSliceInfo();
    pps.setNoPicPartitionFlag(false);
    pps.setLog2CtuSize( (uint8_t)ceilLog2(sps.getCTUSize()) );
    pps.setSingleSlicePerSubPicFlag(false);

    pps.setNumExpTileColumns((uint32_t)std::max(1, (int)tileWidths.size() - 1));
    auto tileWidthIt = tileWidths.begin();
    for (uint32_t i = 0; i < pps.getNumExpTileColumns(); i++, tileWidthIt++)
    {
      pps.addTileColumnWidth( *tileWidthIt / sps.getCTUSize());
    }

    pps.setNumExpTileRows((uint32_t)std::max(1, (int)tileHeights.size() - 1));
    auto tileHeightIt = tileHeights.begin();
    for (uint32_t i = 0; i < pps.getNumExpTileRows(); i++, tileHeightIt++)
    {
      pps.addTileRowHeight( *tileHeightIt / sps.getCTUSize());
    }

    pps.initTiles();
    pps.setRectSliceFlag( 1 );

    unsigned int numSlicesInPIc = 0;
    for (auto &subpic : *m_subpics)
    {
      const PPS &subpicPPS = *subpic.slices[0].getPPS();
      numSlicesInPIc += subpicPPS.getNumSlicesInPic();
    }

    pps.setNumSlicesInPic( numSlicesInPIc );
    pps.setTileIdxDeltaPresentFlag(true);
    pps.initRectSlices( );

    pps.setSingleSlicePerSubPicFlag(true);
    for (auto &subpic : *m_subpics)
    {
      const PPS &subpicPPS = *subpic.slices[0].getPPS();
      if (subpicPPS.getNumSlicesInPic() > 1)
      {
        pps.setSingleSlicePerSubPicFlag(false);
        break;
      }
    }

    if (!pps.getSingleSlicePerSubPicFlag())
    {
      unsigned int numTileColsInPic = pps.getNumTileColumns();

      unsigned int sliceIdx = 0;
      for (auto& subpic : *m_subpics)
      {
        unsigned int tileIdxY = 0;
        for (unsigned int tileY = 0; tileIdxY < tileHeights.size(); tileIdxY++)
        {
          if (tileY == subpic.topLeftCornerY || (tileY + tileHeights[tileIdxY]) == (subpic.topLeftCornerY + subpic.height) ||
              (tileY < subpic.topLeftCornerY && (tileY + tileHeights[tileIdxY]) >(subpic.topLeftCornerY + subpic.height)))
          {
            break;
          }
          tileY += tileHeights[tileIdxY];
        }
        CHECK(tileIdxY == tileHeights.size(), "Could not find subpicture to tile mapping");

        unsigned int tileIdxX = 0;
        for (unsigned int tileX = 0; tileIdxX < tileWidths.size(); tileIdxX++)
        {
          if (tileX == subpic.topLeftCornerX || (tileX + tileWidths[tileIdxX]) == (subpic.topLeftCornerX + subpic.width) ||
              (tileX < subpic.topLeftCornerX && (tileX + tileWidths[tileIdxX]) >(subpic.topLeftCornerX + subpic.width)))
          {
            break;
          }
          tileX += tileWidths[tileIdxX];
        }
        CHECK(tileIdxX == tileWidths.size(), "Could not find subpicture to tile mapping")

        const PPS& subpicPPS = *subpic.slices[0].getPPS();

        if (subpicPPS.getNumSlicesInPic() == 1)
        {
          pps.setSliceWidthInTiles(sliceIdx, subpicPPS.getNumTileColumns());
          pps.setSliceHeightInTiles(sliceIdx, subpicPPS.getNumTileRows());
          pps.setNumSlicesInTile(sliceIdx, 1);
          unsigned int sliceTileIdx = tileIdxY * numTileColsInPic + tileIdxX;
          pps.setSliceTileIdx(sliceIdx, sliceTileIdx);
          pps.setSliceHeightInCtu(sliceIdx, subpicPPS.getPicHeightInCtu());

          sliceIdx++;
        }
        else
        {
          for (int subpicSliceIdx = 0; subpicSliceIdx < subpicPPS.getNumSlicesInPic(); subpicSliceIdx++, sliceIdx++)
          {
            pps.setSliceWidthInTiles(sliceIdx, subpicPPS.getSliceWidthInTiles(subpicSliceIdx));
            pps.setSliceHeightInTiles(sliceIdx, subpicPPS.getSliceHeightInTiles(subpicSliceIdx));
            pps.setNumSlicesInTile(sliceIdx, subpicPPS.getNumSlicesInTile(subpicSliceIdx));
            unsigned int sliceTileIdxSubpic = subpicPPS.getSliceTileIdx(subpicSliceIdx);
            unsigned int sliceTileIdx = (sliceTileIdxSubpic / subpicPPS.getNumTileColumns() + tileIdxY) * numTileColsInPic + tileIdxX + (sliceTileIdxSubpic % subpicPPS.getNumTileColumns());
            pps.setSliceTileIdx(sliceIdx, sliceTileIdx);
            pps.setSliceHeightInCtu(sliceIdx, subpicPPS.getSliceHeightInCtu(subpicSliceIdx));
          }
        }
      }
    }
    else
    {
      pps.setTileIdxDeltaPresentFlag(false);
    }

    pps.initRectSliceMap(&sps);

    pps.setLoopFilterAcrossTilesEnabledFlag(false);
    pps.setLoopFilterAcrossSlicesEnabledFlag(false);
  }

}

/**
  - Configure slice headers of all subpicture for merged stream
*/
void SubpicMergeApp::updateSliceHeadersForMergedStream(ParameterSetManager &psManager)
{
  int subPicId = 0;
  for (auto &subpic : *m_subpics)
  {
    for (auto &slice : subpic.slices)
    {
      // Update slice headers to use new SPSes and PPSes
      int ppsId = slice.getPPS()->getPPSId();
      int spsId = slice.getSPS()->getSPSId();
      CHECK(!psManager.getSPS(spsId), "Invalid SPS");
      CHECK(!psManager.getSPS(ppsId), "Invalid PPS");
      slice.setSPS(psManager.getSPS(spsId));
      slice.setPPS(psManager.getPPS(ppsId));

      slice.setSliceSubPicId(subPicId);
      slice.setPictureHeaderInSliceHeader(false);
    }
    subPicId++;
  }
}

/**
  - Copy input NAL unit to ouput NAL unit
*/
void SubpicMergeApp::copyInputNaluToOutputNalu(OutputNALUnit &outNalu, InputNALUnit &inNalu)
{
  // Copy nal header info
  outNalu = inNalu;

  // Copy payload
  std::vector<uint8_t> &inFifo = inNalu.getBitstream().getFifo();
  std::vector<uint8_t> &outFifo = outNalu.m_bitstream.getFifo();
  outFifo = std::vector<uint8_t>(inFifo.begin() + 2, inFifo.end());
}

/**
  - Copy NAL unit with NAL unit type naluType to access unit
*/
void SubpicMergeApp::copyNalUnitsToAccessUnit(AccessUnit &accessUnit, std::vector<InputNALUnit> &nalus, int naluType)
{
  for (auto &inNalu : nalus)
  {
    if (inNalu.m_nalUnitType == (NalUnitType)naluType)
    {
      if (!(naluType == NAL_UNIT_SUFFIX_SEI
            && SEI::PayloadType(inNalu.getBitstream().getFifo().at(2))
                 == SEI::PayloadType::DECODED_PICTURE_HASH))   // Don't copy decoded_picture_hash SEI
      {
        OutputNALUnit outNalu((NalUnitType)naluType);
        copyInputNaluToOutputNalu(outNalu, inNalu);
        accessUnit.push_back(new NALUnitEBSP(outNalu));
      }
    }
  }
}


/**
  - Find out whether output picture has mixed NAL unit types
 */
bool SubpicMergeApp::getMixedNalPicFlag()
{
  bool IRAPFound = false;
  bool nonIRAPNonTrailingFound = false;
  bool mixedNaluTypesFlag = false;
  NalUnitType prevNaluType = m_subpics->at(0).slices[0].getNalUnitType();

  for (auto &subpic : *m_subpics)
  {
    if (subpic.slices[0].isIRAP())
    {
      IRAPFound = true;
    }
    else if (subpic.slices[0].getNalUnitType() != NAL_UNIT_CODED_SLICE_TRAIL)
    {
      nonIRAPNonTrailingFound = true;
    }
    if (subpic.slices[0].getNalUnitType() != prevNaluType)
    {
      mixedNaluTypesFlag = true;
    }
    prevNaluType = subpic.slices[0].getNalUnitType();
  }

  CHECK(IRAPFound && nonIRAPNonTrailingFound, "IRAP subpictures can only be mixed with trailing subpictures");

  return mixedNaluTypesFlag;
}


/**
  - Select subpicture where picture header will be taken from
*/
Subpicture &SubpicMergeApp::selectSubpicForPicHeader(bool isMixedNaluPic)
{
  // Can take any pic header if there are no mixed NALUs in picture
  if (!isMixedNaluPic)
  {
    return m_subpics->at(0);
  }

  Subpicture *subpicToReturn = nullptr;
  bool IRAPFound = false;

  // Find first non-IRAP subpicture
  for (auto &subpic : *m_subpics)
  {
    if (subpic.slices[0].isIRAP())
    {
      IRAPFound = true;
    }
    else if (!subpicToReturn)
    {
      subpicToReturn = &subpic;
    }
  }

  CHECK(subpicToReturn == nullptr, "Could not find non-IRAP subpicture when mixed NALU types in enabled");

  if (IRAPFound)
  {
    subpicToReturn->picHeader.setPicIntraSliceAllowedFlag(true);
  }

  return *subpicToReturn;
}


/**
  - Merge subpictures for one picture
 */
void SubpicMergeApp::generateMergedPic(ParameterSetManager &psManager, bool mixedNaluFlag)
{
  AccessUnit accessUnit;
  HLSWriter hlsWriter;
  Subpicture &subpic0 = m_subpics->at(0);  // Non-VCL NAL units are copied from the first subpicture only
  std::vector<VPS*> vpsList;        // List of SPS that will be coded in this AU
  std::vector<SPS*> spsList;        // List of SPS that will be coded in this AU
  std::vector<PPS*> ppsList;        // List of PPS that will be coded in this AU

  // Copy AUD NAL unit if it exists
  copyNalUnitsToAccessUnit(accessUnit, subpic0.nalus, (int)NAL_UNIT_ACCESS_UNIT_DELIMITER);

  // Code unmodified DCI NAL unit if it exists
  if (subpic0.dciPresent)
  {
    OutputNALUnit nalu(NAL_UNIT_DCI);
    hlsWriter.setBitstream(&nalu.m_bitstream);
    hlsWriter.codeDCI(&subpic0.dci);
    accessUnit.push_back(new NALUnitEBSP(nalu));
  }

  generateMergedStreamVPSes(vpsList);

  // Code unmodified VPS NAL units if any exists
  for (auto vps : vpsList)
  {
    OutputNALUnit nalu(NAL_UNIT_VPS);
    hlsWriter.setBitstream(&nalu.m_bitstream);
    hlsWriter.codeVPS(vps);
    psManager.storeVPS(vps, nalu.m_bitstream.getFifo());
    accessUnit.push_back(new NALUnitEBSP(nalu));
  }

  generateMergedStreamSPSes(spsList);

  // Code merged stream SPS NAL units if any was generated
  for (auto sps : spsList)
  {
    OutputNALUnit nalu(NAL_UNIT_SPS);
    hlsWriter.setBitstream(&nalu.m_bitstream);
    hlsWriter.codeSPS(sps);
    psManager.storeSPS(sps, nalu.m_bitstream.getFifo());
    accessUnit.push_back(new NALUnitEBSP(nalu));
  }

  generateMergedStreamPPSes(psManager, ppsList);

  // Code merged stream PPS NAL units if any was generated
  for (auto pps : ppsList)
  {
    PPS *ppsMixed = nullptr;
    if (mixedNaluFlag)
    {
      ppsMixed = new PPS(*pps);
      if (pps->pcv)
      {
        ppsMixed->pcv = new PreCalcValues(*pps->pcv);
      }
    }

    OutputNALUnit nalu(NAL_UNIT_PPS);
    hlsWriter.setBitstream(&nalu.m_bitstream);
    hlsWriter.codePPS(pps);
    psManager.storePPS(pps, nalu.m_bitstream.getFifo());
    accessUnit.push_back(new NALUnitEBSP(nalu));

    if (mixedNaluFlag)
    {
      OutputNALUnit naluMixed(NAL_UNIT_PPS);
      hlsWriter.setBitstream(&naluMixed.m_bitstream);
      ppsMixed->setPPSId(ppsMixed->getPPSId() + MIXED_NALU_PPS_OFFSET);
      ppsMixed->setMixedNaluTypesInPicFlag(true);
      hlsWriter.codePPS(ppsMixed);
      psManager.storePPS(ppsMixed, naluMixed.m_bitstream.getFifo());
      accessUnit.push_back(new NALUnitEBSP(naluMixed));
    }
  }

  updateSliceHeadersForMergedStream(psManager);

  // Code merged stream prefix APS NAL units
  std::vector<int> usedApsIds;
  for (auto &subpic : *m_subpics)
  {
    auto apsIdIt = subpic.apsIds.begin();
    for (auto &nalu : subpic.nalus)
    {
      if (nalu.m_nalUnitType == NAL_UNIT_PREFIX_APS)
      {
        if (std::find(usedApsIds.begin(), usedApsIds.end(), apsIdIt->first) == usedApsIds.end())
        {
        OutputNALUnit naluOut(NAL_UNIT_PREFIX_APS, nalu.m_nuhLayerId, nalu.m_temporalId);
        hlsWriter.setBitstream(&naluOut.m_bitstream);
        hlsWriter.codeAPS(subpic.psManager.getAPS(apsIdIt->first, apsIdIt->second));
        accessUnit.push_back(new NALUnitEBSP(naluOut));
        apsIdIt++;
          usedApsIds.push_back(apsIdIt->first);
        }
      }
    }
  }

  bool isMixedNaluPic = getMixedNalPicFlag();
  CHECK(!mixedNaluFlag && isMixedNaluPic, "Mixed NALU types is disabled but picture contains mixed NALU types");

  Subpicture &subpicForPicHeader = selectSubpicForPicHeader(isMixedNaluPic);
  bool writePicHeader = true;

  for (auto &subpic : *m_subpics)
  {
    auto slice = subpic.slices.begin();
    auto sliceData = subpic.sliceData.begin();

    for (auto &nalu : subpic.nalus)
    {
      if (nalu.isVcl())
      {
        if (writePicHeader)
        {
          OutputNALUnit naluOut(NAL_UNIT_PH, nalu.m_nuhLayerId, nalu.m_temporalId);  // Copy IDs from slice NALU
          hlsWriter.setBitstream(&naluOut.m_bitstream);
          if (isMixedNaluPic)
          {
            subpicForPicHeader.picHeader.setPPSId(subpicForPicHeader.picHeader.getPPSId() + MIXED_NALU_PPS_OFFSET);
          }
          hlsWriter.codePictureHeader(&subpicForPicHeader.picHeader, true, &subpicForPicHeader.slices[0]);
          accessUnit.push_back(new NALUnitEBSP(naluOut));
          writePicHeader = false;
        }
        OutputNALUnit naluOut(nalu.m_nalUnitType, nalu.m_nuhLayerId, nalu.m_temporalId);
        hlsWriter.setBitstream(&naluOut.m_bitstream);
        hlsWriter.codeSliceHeader(&(*slice), &subpicForPicHeader.picHeader);
        naluOut.m_bitstream.writeByteAlignment();
        naluOut.m_bitstream.addSubstream(&(*sliceData));
        accessUnit.push_back(new NALUnitEBSP(naluOut));
        slice++;
        sliceData++;
      }
    }
  }

  // Code Decoded picture hash SEI messages within Scalable nesting SEI messages
  uint32_t layerId = m_subpics->at(0).slices[0].getNalUnitLayerId();
  uint32_t temporalId = m_subpics->at(0).slices[0].getTLayer();
  int subpicId = 0;
  for (auto& subpic : *m_subpics)
  {
    if (subpic.decodedPictureHashSei != nullptr)
    {
      SEIEncoder seiEncoder;
      SEIWriter seiWriter;
      SEIMessages seiMessages;
      SEIMessages nestedSEI;
      HRD hrd;
      nestedSEI.push_back(subpic.decodedPictureHashSei);
      const std::vector<uint16_t> subPicIds = { (uint16_t)subpicId };
      std::vector<int> targetOLS;
      std::vector<int> targetLayers = { (int)subpic.nalus[0].m_nuhLayerId };

      auto sn = new SEIScalableNesting();
      seiEncoder.init(0, 0, 0);
      const uint16_t maxSubpicIdInPic =
        subPicIds.size() == 0 ? 0 : *std::max_element(subPicIds.begin(), subPicIds.end());
      seiEncoder.initSEIScalableNesting(sn, nestedSEI, targetOLS, targetLayers, subPicIds, maxSubpicIdInPic);
      OutputNALUnit nalu( NAL_UNIT_SUFFIX_SEI, layerId, temporalId );
      seiMessages.push_back(sn);
      seiWriter.writeSEImessages(nalu.m_bitstream, seiMessages, hrd, false, temporalId);
      accessUnit.push_back(new NALUnitEBSP(nalu));
    }
    subpicId++;
  }

  copyNalUnitsToAccessUnit(accessUnit, subpic0.nalus, (int)NAL_UNIT_EOS);
  copyNalUnitsToAccessUnit(accessUnit, subpic0.nalus, (int)NAL_UNIT_EOB);

  writeAnnexBAccessUnit(m_outputStream, accessUnit);
}

/**
  - Check that subpictures are valid for merging
 */
void SubpicMergeApp::validateSubpics()
{
  const SPS *sps = m_subpics->at(0).slices.at(0).getSPS();

  if (sps->getJointCbCrEnabledFlag())
  {
    int jointCbCrSignFlag = -1;

    for (auto &subpic : *m_subpics)
    {
      if (jointCbCrSignFlag == -1)
      {
        jointCbCrSignFlag = subpic.picHeader.getJointCbCrSignFlag() ? 1 : 0;
      }
      else
      {
        int jointCbCrSignFlagCurr = subpic.picHeader.getJointCbCrSignFlag() ? 1 : 0;
        CHECK(jointCbCrSignFlag != jointCbCrSignFlagCurr, "ph_joint_cbcr_sign_flag must have indentical value in all input subpictures");
      }
    }
  }

  for (auto &subpic : *m_subpics)
  {
    for (auto nal : subpic.nalus)
    {
      if (nal.m_nalUnitType == NAL_UNIT_PREFIX_APS || nal.m_nalUnitType == NAL_UNIT_SUFFIX_APS)
      {
        uint8_t APSIdType = nal.getBitstream().getFifo().at(2);
        for (auto &subpic2 : *m_subpics)
        {
          for (auto nal2 : subpic2.nalus)
          {
            if (nal2.m_nalUnitType == NAL_UNIT_PREFIX_APS || nal2.m_nalUnitType == NAL_UNIT_SUFFIX_APS)
            {
              uint8_t APSIdType2 = nal2.getBitstream().getFifo().at(2);
              if (APSIdType == APSIdType2)
              {
                bool sameContent = nal.getBitstream().getFifo() == nal2.getBitstream().getFifo();
                CHECK(!sameContent, "Two APS with the same ID and the same type must have identical content");
              }
            }
          }
        }
      }
    }
  }
}


/**
  - Merge subpicture bitstreams into one bitstream
 */
void SubpicMergeApp::mergeStreams(bool mixedNaluFlag)
{
  ParameterSetManager psManager;  // Parameter sets for merged stream
  int picNum = 0;

  msg( INFO, "Output picture size is %ix%i\n", m_picWidth, m_picHeight);

  for (auto &subpic : *m_subpics)
  {
    subpic.bs = new InputByteStream(*(subpic.fp));
    subpic.prevTid0Poc = 0;
    subpic.psManager.storeVPS(new VPS, std::vector<uint8_t>());  // Create VPS with default values (VTM slice header parser needs this)
  }

  bool morePictures = true;
  while (morePictures)
  {
    msg( INFO, "Picture %i\n", picNum);
    int subPicNum = 0;

    for (auto &subpic : *m_subpics)
    {
      msg( INFO, " Subpicture %i\n", subPicNum);
      parseSubpic(subpic, morePictures);
      subPicNum++;
      msg( INFO, "\n");
    }

    validateSubpics();

    generateMergedPic(psManager, mixedNaluFlag);

    // Update prevTid0Poc flags for subpictures
    for (auto &subpic : *m_subpics)
    {
      if (subpic.slices.size() > 0 && subpic.slices[0].getTLayer() == 0 &&
          subpic.slices[0].getNalUnitType() != NAL_UNIT_CODED_SLICE_RADL &&
          subpic.slices[0].getNalUnitType() != NAL_UNIT_CODED_SLICE_RASL )
      {
        subpic.prevTid0Poc = subpic.slices[0].getPOC();
      }
    }

    m_prevPicPOC = m_subpics->at(0).slices.at(0).getPOC();

    picNum++;
  }
}

/**
  - Merge input subpicture yuv files into one yuv file
 */
void SubpicMergeApp::mergeYuvFiles(int bitdepth, int chromaFormat)
{
  uint8_t *destPic[3];

  int numBytesPerSample = (bitdepth + 7) / 8;
  int numBytesPerPicY = m_picWidth * m_picHeight * numBytesPerSample;
  int numBytesPerPicUV = numBytesPerPicY / (chromaFormat == 420 ? 4 : (chromaFormat == 422 ? 2 : 1));

  destPic[0] = new uint8_t[numBytesPerPicY];
  destPic[1] = new uint8_t[numBytesPerPicUV];
  destPic[2] = new uint8_t[numBytesPerPicUV];

  int picNum = 0;
  bool morePics = true;

  while (morePics)
  {
    for (auto &subpic : *m_subpics)
    {
      for (int cIdx = 0; morePics && cIdx < (chromaFormat == 400 ? 1 : 3); cIdx++)
      {
        int cDivX = (cIdx == 0 || chromaFormat == 444) ? 1 : 2;
        int cDivY = (cIdx == 0 || chromaFormat == 444 || chromaFormat == 422) ? 1 : 2;
        int picW = m_picWidth / cDivX;
        int subpicW = subpic.width / cDivX;
        uint8_t *dest = &destPic[cIdx][(subpic.topLeftCornerY / cDivY * picW + subpic.topLeftCornerX / cDivX) * numBytesPerSample];

        for (int y = 0; y < subpic.height / cDivY; y++)
        {
          subpic.fp->read(reinterpret_cast<char*>(dest), subpicW * numBytesPerSample);
          if (subpic.fp->eof())
          {
            morePics = false;
            break;
          }
          dest += picW * numBytesPerSample;
        }
      }

      if (!morePics)
      {
        break;
      }
    }

    if (morePics)
    {
      for (int cIdx = 0; cIdx < (chromaFormat == 400 ? 1 : 3); cIdx++)
      {
        m_outputStream.write(reinterpret_cast<char*>(destPic[cIdx]), cIdx == 0 ? numBytesPerPicY : numBytesPerPicUV);
      }

      msg( INFO, "Merged YUV picture %i\n", picNum);
    }

    picNum++;
  }

  delete[] destPic[0];
  delete[] destPic[1];
  delete[] destPic[2];
}


//! \}
