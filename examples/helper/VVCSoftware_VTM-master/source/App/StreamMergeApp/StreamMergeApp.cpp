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

 /** \file     StreamMergeApp.cpp
     \brief    Decoder application class
 */

#include <list>
#include <vector>
#include <stdio.h>
#include <fcntl.h>

#include "StreamMergeApp.h"
#include "AnnexBwrite.h"
#include "NALwrite.h"
#if RExt__DECODER_DEBUG_BIT_STATISTICS
#include "CommonLib/CodingStatistics.h"
#endif

//! \ingroup DecoderApp
//! \{

// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================

StreamMergeApp::StreamMergeApp()
{

}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

static void _byteStreamNALUnit(SingleLayerStream &bs, std::istream &istream, std::vector<uint8_t> &nalUnit,
                               AnnexBStats &stats)
{
  /* At the beginning of the decoding process, the decoder initialises its
   * current position in the byte stream to the beginning of the byte stream.
   * It then extracts and discards each leading_zero_8bits syntax element (if
   * present), moving the current position in the byte stream forward one
   * byte at a time, until the current position in the byte stream is such
   * that the next four bytes in the bitstream form the four-byte sequence
   * 0x00000001.
   */
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  CodingStatistics::SStat &statBits = CodingStatistics::GetStatisticEP(STATS__NAL_UNIT_PACKING);
#endif
  while ((bs.eofBeforeNBytes(24 / 8, istream) || bs.peekBytes(24 / 8, istream) != 0x000001)
    && (bs.eofBeforeNBytes(32 / 8, istream) || bs.peekBytes(32 / 8, istream) != 0x00000001))
  {
    uint8_t leading_zero_8bits = bs.readByte(istream);
#if RExt__DECODER_DEBUG_BIT_STATISTICS
    statBits.bits += 8; statBits.count++;
#endif
    if (leading_zero_8bits != 0) { THROW("Leading zero bits not zero"); }
    stats.m_numLeadingZero8BitsBytes++;
  }

  /* 1. When the next four bytes in the bitstream form the four-byte sequence
   * 0x00000001, the next byte in the byte stream (which is a zero_byte
   * syntax element) is extracted and discarded and the current position in
   * the byte stream is set equal to the position of the byte following this
   * discarded byte.
   */
   /* NB, the previous step guarantees this will succeed -- if EOF was
    * encountered, an exception will stop execution getting this far */
  if (bs.peekBytes(24 / 8, istream) != 0x000001)
  {
    uint8_t zero_byte = bs.readByte(istream);
#if RExt__DECODER_DEBUG_BIT_STATISTICS
    statBits.bits += 8; statBits.count++;
#endif
    CHECK(zero_byte != 0, "Zero byte not '0'");
    stats.m_numZeroByteBytes++;
  }

  /* 2. The next three-byte sequence in the byte stream (which is a
   * start_code_prefix_one_3bytes) is extracted and discarded and the current
   * position in the byte stream is set equal to the position of the byte
   * following this three-byte sequence.
   */
   /* NB, (1) guarantees that the next three bytes are 0x00 00 01 */
  uint32_t start_code_prefix_one_3bytes = bs.readBytes(24 / 8, istream);
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  statBits.bits += 24; statBits.count += 3;
#endif
  if (start_code_prefix_one_3bytes != 0x000001) { THROW("Invalid code prefix"); }
  stats.m_numStartCodePrefixBytes += 3;

  /* 3. NumBytesInNALunit is set equal to the number of bytes starting with
   * the byte at the current position in the byte stream up to and including
   * the last byte that precedes the location of any of the following
   * conditions:
   *   a. A subsequent byte-aligned three-byte sequence equal to 0x000000, or
   *   b. A subsequent byte-aligned three-byte sequence equal to 0x000001, or
   *   c. The end of the byte stream, as determined by unspecified means.
   */
   /* 4. NumBytesInNALunit bytes are removed from the bitstream and the
    * current position in the byte stream is advanced by NumBytesInNALunit
    * bytes. This sequence of bytes is nal_unit( NumBytesInNALunit ) and is
    * decoded using the NAL unit decoding process
    */
    /* NB, (unsigned)x > 2 implies n!=0 && n!=1 */
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  CodingStatistics::SStat &bodyStats = CodingStatistics::GetStatisticEP(STATS__NAL_UNIT_TOTAL_BODY);
#endif
  while (bs.eofBeforeNBytes(24 / 8, istream) || bs.peekBytes(24 / 8, istream) > 2)
  {
#if RExt__DECODER_DEBUG_BIT_STATISTICS
    uint8_t thebyte = bs.readByte(istream); bodyStats.bits += 8; bodyStats.count++;
    nalUnit.push_back(thebyte);
#else
    nalUnit.push_back(bs.readByte(istream));
#endif
  }

  /* 5. When the current position in the byte stream is:
   *  - not at the end of the byte stream (as determined by unspecified means)
   *  - and the next bytes in the byte stream do not start with a three-byte
   *    sequence equal to 0x000001
   *  - and the next bytes in the byte stream do not start with a four byte
   *    sequence equal to 0x00000001,
   * the decoder extracts and discards each trailing_zero_8bits syntax
   * element, moving the current position in the byte stream forward one byte
   * at a time, until the current position in the byte stream is such that:
   *  - the next bytes in the byte stream form the four-byte sequence
   *    0x00000001 or
   *  - the end of the byte stream has been encountered (as determined by
   *    unspecified means).
   */
   /* NB, (3) guarantees there are at least three bytes available or none */
  while ((bs.eofBeforeNBytes(24 / 8, istream) || bs.peekBytes(24 / 8, istream) != 0x000001)
    && (bs.eofBeforeNBytes(32 / 8, istream) || bs.peekBytes(32 / 8, istream) != 0x00000001))
  {
    uint8_t trailing_zero_8bits = bs.readByte(istream);
#if RExt__DECODER_DEBUG_BIT_STATISTICS
    statBits.bits += 8; statBits.count++;
#endif
    CHECK(trailing_zero_8bits != 0, "Trailing zero bits not '0'");
    stats.m_numTrailingZero8BitsBytes++;
  }
}

/**
 * Parse an AVC AnnexB Bytestream bs to extract a single nalUnit
 * while accumulating bytestream statistics into stats.
 *
 * Returns false if EOF was reached (NB, nalunit data may be valid),
 *         otherwise true.
 */
bool byteStreamNALUnit(SingleLayerStream &bs, std::istream &istream, std::vector<uint8_t> &nalUnit, AnnexBStats &stats)
{
  bool eof = false;
  try
  {
    _byteStreamNALUnit(bs, istream, nalUnit, stats);
  }
  catch (...)
  {
    eof = true;
  }
  stats.m_numBytesInNALUnit = uint32_t(nalUnit.size());
  return eof;
}

/**
 - lookahead through next NAL units to determine if current NAL unit is the first NAL unit in a new picture
 */
bool StreamMergeApp::isNewPicture(std::ifstream *bitstreamFile, InputByteStream *bytestream, bool firstSliceInPicture)
{
  bool ret      = false;
  bool finished = false;

  // cannot be a new picture if there haven't been any slices yet
  if (firstSliceInPicture)
  {
    return false;
  }

  // save stream position for backup
  std::streampos location = bitstreamFile->tellg();

  // look ahead until picture start location is determined
  while (!finished && !!(*bitstreamFile))
  {
    AnnexBStats  stats = AnnexBStats();
    InputNALUnit nalu;
    byteStreamNALUnit(*bytestream, nalu.getBitstream().getFifo(), stats);
    if (nalu.getBitstream().getFifo().empty())
    {
      msg(ERROR, "Warning: Attempt to decode an empty NAL unit\n");
    }
    else
    {
      // get next NAL unit type
      read(nalu);
      switch (nalu.m_nalUnitType)
      {
      // NUT that indicate the start of a new picture
      case NAL_UNIT_ACCESS_UNIT_DELIMITER:
      case NAL_UNIT_OPI:
      case NAL_UNIT_DCI:
      case NAL_UNIT_VPS:
      case NAL_UNIT_SPS:
      case NAL_UNIT_PPS:
      case NAL_UNIT_PH:
        ret      = true;
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
        ret      = checkPictureHeaderInSliceHeaderFlag(nalu);
        finished = true;
        break;

        // NUT that are not the start of a new picture
      case NAL_UNIT_EOS:
      case NAL_UNIT_EOB:
      case NAL_UNIT_SUFFIX_APS:
      case NAL_UNIT_SUFFIX_SEI:
      case NAL_UNIT_FD:
        ret      = false;
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
  bitstreamFile->clear();
  bitstreamFile->seekg(location - std::streamoff(3));
  bytestream->reset();

  // return TRUE if next NAL unit is the start of a new picture
  return ret;
}

/**
- lookahead through next NAL units to determine if current NAL unit is the first NAL unit in a new access unit
*/
bool StreamMergeApp::isNewAccessUnit(bool newPicture, std::ifstream *bitstreamFile, class InputByteStream *bytestream)
{
  bool ret      = false;
  bool finished = false;

  // can only be the start of an AU if this is the start of a new picture
  if (newPicture == false)
  {
    return false;
  }

  // save stream position for backup
  std::streampos location = bitstreamFile->tellg();

  // look ahead until access unit start location is determined
  while (!finished && !!(*bitstreamFile))
  {
    AnnexBStats  stats = AnnexBStats();
    InputNALUnit nalu;
    byteStreamNALUnit(*bytestream, nalu.getBitstream().getFifo(), stats);
    if (nalu.getBitstream().getFifo().empty())
    {
      msg(ERROR, "Warning: Attempt to decode an empty NAL unit\n");
    }
    else
    {
      // get next NAL unit type
      read(nalu);
      switch (nalu.m_nalUnitType)
      {
      // AUD always indicates the start of a new access unit
      case NAL_UNIT_ACCESS_UNIT_DELIMITER:
        ret      = true;
        finished = true;
        break;

      // slice types - check layer ID and POC
      case NAL_UNIT_CODED_SLICE_TRAIL:
      case NAL_UNIT_CODED_SLICE_STSA:
      case NAL_UNIT_CODED_SLICE_RASL:
      case NAL_UNIT_CODED_SLICE_RADL:
      case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
      case NAL_UNIT_CODED_SLICE_IDR_N_LP:
      case NAL_UNIT_CODED_SLICE_CRA:
      case NAL_UNIT_CODED_SLICE_GDR:
        ret      = true; // isSliceNaluFirstInAU(newPicture, nalu); // TODO: according to DecLib::isSliceNaluFirstInAU(), true if layerID==prevLayerID, otherwise true if POC!=prevPOC.
        finished = true;
        break;

      // NUT that are not the start of a new access unit
      case NAL_UNIT_EOS:
      case NAL_UNIT_EOB:
      case NAL_UNIT_SUFFIX_APS:
      case NAL_UNIT_SUFFIX_SEI:
      case NAL_UNIT_FD:
        ret      = false;
        finished = true;
        break;

      // all other NUT - keep looking to find first VCL
      default: break;
      }
    }
  }

  // restore previous stream location
  bitstreamFile->clear();
  bitstreamFile->seekg(location);
  bytestream->reset();

  // return TRUE if next NAL unit is the start of a new picture
  return ret;
}

void StreamMergeApp::inputNaluHeaderToOutputNalu(InputNALUnit& inNalu, OutputNALUnit& outNalu) {
  outNalu.m_forbiddenZeroBit   = inNalu.m_forbiddenZeroBit;
  outNalu.m_nalUnitType        = inNalu.m_nalUnitType;
  outNalu.m_nuhLayerId         = inNalu.m_nuhLayerId;
  outNalu.m_nuhReservedZeroBit = inNalu.m_nuhReservedZeroBit;
  outNalu.m_temporalId         = inNalu.m_temporalId;
}

bool StreamMergeApp::preInjectNalu(MergeLayer &layer, InputNALUnit &inNalu, OutputNALUnit &outNalu)
{
  HLSyntaxReader hlsReader;
  HLSWriter      hlsWriter;
  hlsReader.setBitstream(&inNalu.getBitstream());
  hlsWriter.setBitstream(&outNalu.m_bitstream);

  switch (inNalu.m_nalUnitType)
  {
  case NAL_UNIT_SPS:
  {
    VPS *vps = new VPS();
    if (vpsId == -1)
    {
      vpsId = ++idIncrement;
    }
    vps->setVPSId(vpsId);
    for (int i = 0; i < m_numInputStreams; i++)
    {
      vps->setLayerId(i, i);   // Because we use layer IDs that are layer indices.
    }
    vps->setMaxLayers(m_numInputStreams);
    vps->setNumPtls(1);
    layer.vpsIdMapping[0] = vps->getVPSId();
    layer.psManager.storeVPS(vps, hlsReader.getBitstream()->getFifo());
    hlsWriter.codeVPS(vps);
    outNalu.m_nalUnitType = NAL_UNIT_VPS;
    msg(INFO, " layer %i, nalu type VPS%i injected\n", layer.id, vps->getVPSId());
    return true;
  }
  default:
    break;
  }
  return false;
}

/**
  - Decode NAL unit if it is parameter set or picture header, or decode slice header of VLC NAL unit
 */
void StreamMergeApp::decodeAndRewriteNalu(MergeLayer &layer, InputNALUnit &inNalu, OutputNALUnit &outNalu)
{
  HLSyntaxReader hlsReader;
  HLSWriter      hlsWriter;
  hlsReader.setBitstream(&inNalu.getBitstream());
  hlsWriter.setBitstream(&outNalu.m_bitstream);

  msg(INFO, " layer %i, nalu type ", layer.id);
  switch (inNalu.m_nalUnitType)
  {
  case NAL_UNIT_SPS:
  {
    SPS *oldSps = new SPS();
    SPS *newSps = new SPS();
    hlsReader.parseSPS(oldSps);
    inNalu.getBitstream().resetToStart();
    uint32_t uiCode;
    inNalu.getBitstream().read(16, uiCode);
    hlsReader.parseSPS(newSps);
    // Set new values.
    newSps->setSPSId(++idIncrement);
    newSps->setVPSId(layer.vpsIdMapping.at(oldSps->getVPSId()));
    newSps->setLayerId(layer.id);
    // Store values for later reference.
    layer.spsIdMapping.insert({ oldSps->getSPSId(), newSps->getSPSId() });
    layer.oldIDsPsManager.storeSPS(oldSps, hlsReader.getBitstream()->getFifo());
    layer.psManager.storeSPS(newSps, hlsReader.getBitstream()->getFifo());
    hlsWriter.codeSPS(newSps);
    msg(INFO, "SPS%i", newSps->getSPSId());
    break;
  }
  case NAL_UNIT_PPS:
  {
    PPS *oldPps = new PPS();
    PPS *newPps = new PPS();
    hlsReader.parsePPS(oldPps);
    inNalu.getBitstream().resetToStart();
    uint32_t uiCode;
    inNalu.getBitstream().read(16, uiCode);
    hlsReader.parsePPS(newPps);
    // Set new values.
    newPps->setPPSId(++idIncrement);
    newPps->setSPSId(layer.spsIdMapping.at(oldPps->getSPSId()));
    newPps->setLayerId(layer.id);
    // Store values for later reference.
    layer.ppsIdMapping.insert({ oldPps->getPPSId(), newPps->getPPSId() });
    layer.oldIDsPsManager.storePPS(oldPps, hlsReader.getBitstream()->getFifo());
    layer.psManager.storePPS(newPps, hlsReader.getBitstream()->getFifo());
    hlsWriter.codePPS(newPps);
    msg(INFO, "PPS%i", newPps->getPPSId());
    break;
  }
  case NAL_UNIT_PREFIX_APS:
  case NAL_UNIT_SUFFIX_APS:
  {
    APS *aps = new APS();
    hlsReader.parseAPS(aps);
    layer.apsIdMapping.insert({ aps->getAPSId(), ++idIncrement });
    aps->setLayerId(layer.id);
    aps->setAPSId(idIncrement);
    layer.psManager.storeAPS(aps, hlsReader.getBitstream()->getFifo());
    hlsWriter.codeAPS(aps);
    msg(INFO, "APS%s%i", inNalu.m_nalUnitType == NAL_UNIT_PREFIX_APS ? "p" : "s", aps->getAPSId());
    break;
  }
  case NAL_UNIT_PH:
  {
    PicHeader ph = PicHeader();
    hlsReader.parsePictureHeader(&ph, &layer.oldIDsPsManager, true);
    Slice slice = Slice();
    slice.setPPS(layer.psManager.getPPS(layer.ppsIdMapping.at(ph.getPPSId())));
    slice.setSPS(layer.psManager.getSPS(layer.spsIdMapping.at(ph.getSPSId())));
    slice.setPOC(ph.getPocLsb());
    ph.setPPSId(layer.ppsIdMapping.at(ph.getPPSId()));
    hlsWriter.codePictureHeader(&ph, true, &slice);
    msg(INFO, "PH");
    break;
  }
  default:
  {
    if (inNalu.isVcl())
    {
      msg(INFO, "VCL");
    }
    else if (inNalu.isSei())
    {
      msg(INFO, "SEI");
    }
    else
    {
      msg(INFO, "NNN");   // Any other NAL unit that is not handled above
    }
    msg(INFO, " with index %i", inNalu.m_nalUnitType);
    // Copy payload from input nalu to output nalu. Code copied from SubpicMergeApp::copyInputNaluToOutputNalu().
    std::vector<uint8_t> &inFifo  = inNalu.getBitstream().getFifo();
    std::vector<uint8_t> &outFifo = outNalu.m_bitstream.getFifo();
    outFifo                       = std::vector<uint8_t>(inFifo.begin() + 2, inFifo.end());
    break;
  }
  }
  msg(INFO, "\n");
}

uint32_t StreamMergeApp::mergeStreams()
{
  std::ofstream outputStream(m_bitstreamFileNameOut, std::ifstream::out | std::ifstream::binary);

  std::vector<MergeLayer> *layers = new std::vector<MergeLayer>;
  layers->resize(m_numInputStreams);

  // Prepare merge layers.
  for (int i = 0; i < layers->size(); i++)
  {
    MergeLayer &layer = layers->at(i);
    layer.id          = i;

    // Open input file.
    layer.fp = new std::ifstream();
    layer.fp->open(m_bitstreamFileNameIn[i], std::ifstream::in | std::ifstream::binary);
    if (!layer.fp->is_open())
    {
      EXIT("failed to open bitstream file " << m_bitstreamFileNameIn[i] << " for reading");
    }
    layer.fp->clear();
    layer.fp->seekg(0, std::ios::beg);

    // Prep other values.
    layer.bs = new InputByteStream(*(layer.fp));

    VPS* vps = new VPS;
    vps->setMaxLayers((uint32_t) layers->size());
    vps->setLayerId(layer.id, layer.id);   // Layer ID is rewritten here.
    layer.vpsIdMapping.insert({ vps->getVPSId(), 0 });
    vps->setVPSId(0);
    layer.psManager.storeVPS(vps, std::vector<uint8_t>()); // Create VPS with default values (VTM slice header parser needs this)
  }

  // Loop over layers until every one is entirely read.
  uint32_t layersStillToRead = (uint32_t) layers->size();
  while (layersStillToRead > 0)
  {
    // Loop over every layer.
    for (auto &layer: *layers)
    {
      if (layer.doneReading) continue;

      //vector<OutputNALUnit> outNalus; // collection of nalus of this interleave part.
      AccessUnit outAccessUnit;
      // Read until eof or after first vcl nalu.
      bool eoi = false; // end of interleave part.
      while (!eoi) {
        AnnexBStats  stats;
        InputNALUnit inNalu;
        inNalu.m_nalUnitType = NAL_UNIT_INVALID;

        // Find next nalu in stream.
        bool eof = byteStreamNALUnit(*layer.bs, inNalu.getBitstream().getFifo(), stats);

        // End of file reached.
        if (eof) {
          eoi = true;
          layersStillToRead--;
          layer.doneReading = true;
        }

        if (inNalu.getBitstream().getFifo().empty())
        {
          msg(ERROR, "Warning: Attempt to decode an empty NAL unit\n");
          continue;
        }


        read(inNalu);   // Convert nalu payload to RBSP and parse nalu header

        // NALU to optionally inject before the main output NALU.
        OutputNALUnit injectedOutNalu((NalUnitType) inNalu.m_nalUnitType);
        inputNaluHeaderToOutputNalu(inNalu, injectedOutNalu);
        injectedOutNalu.m_nuhLayerId = layer.id;
        if (preInjectNalu(layer, inNalu, injectedOutNalu))
        {
          outAccessUnit.push_back(new NALUnitEBSP(injectedOutNalu));
        }

        // Change input NALU to output NALU.
        OutputNALUnit outNalu((NalUnitType) inNalu.m_nalUnitType);
        inputNaluHeaderToOutputNalu(inNalu, outNalu);
        outNalu.m_nuhLayerId = layer.id;
        decodeAndRewriteNalu(layer, inNalu, outNalu);
        outAccessUnit.push_back(new NALUnitEBSP(outNalu));

        if (inNalu.isVcl())
        {
          layer.firstSliceInPicture = false;
        }

        try
        {
          bool bIsNewPicture = isNewPicture(layer.fp, layer.bs, layer.firstSliceInPicture);
          if (isNewAccessUnit(bIsNewPicture, layer.fp, layer.bs))
          {
            layer.firstSliceInPicture = bIsNewPicture;
            eoi                       = true;
          }
        }
        catch (std::ios_base::failure&)
        {
          eoi = true;
        }
      }
      writeAnnexBAccessUnit(outputStream, outAccessUnit);
    }
  }
  return 0;
}

//! \}
