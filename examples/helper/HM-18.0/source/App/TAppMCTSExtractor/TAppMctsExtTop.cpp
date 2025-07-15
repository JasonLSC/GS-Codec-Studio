/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2022, ITU/ISO/IEC
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

 /** \file     TAppMctsExtTop.cpp
  \brief    MCTS Extractor application class
  */

#include <list>
#include <vector>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <fstream>

#include "TAppMctsExtTop.h"
#include "TLibDecoder/AnnexBread.h"
#include "TLibDecoder/NALread.h"
#include "TLibEncoder/NALwrite.h"
#include "TLibCommon/SEI.h"

#if MCTS_EXTRACTION
  //! \ingroup TAppMctsExt
  //! \{

  // ====================================================================================================================
  // Constructor / destructor / initialization / destroy
  // ====================================================================================================================

TAppMctsExtTop::TAppMctsExtTop()
{
}


Void TAppMctsExtTop::create()
{
  m_mctsExtractionInfoPresent = false;
}

Void TAppMctsExtTop::destroy()
{
  m_inputBitstreamFileName.clear();
  m_outputBitstreamFileName.clear();
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/**
 - create internal class
 - initialize internal class
 - until the end of the bitstream, call extraction function in TDecMctsExt class
 - delete allocated buffers
 - destroy internal class
 .
 */
Void TAppMctsExtTop::extract()
{
  ifstream bitstreamFile(m_inputBitstreamFileName.c_str(), ifstream::in | ifstream::binary);
  if (!bitstreamFile)
  {
    fprintf(stderr, "\nfailed to open input bitstream file `%s' for reading\n", m_inputBitstreamFileName.c_str());
    exit(EXIT_FAILURE);
  }

  InputByteStream bytestream(bitstreamFile);

  // create & initialize internal classes
  xCreateMctsExtLib();
  xInitMctsExtLib();

  Int iSkipFrame = 0;
  Int iPOCLastDisplay = 0;

  fstream bitstreamFileOut(m_outputBitstreamFileName.c_str(), fstream::binary | fstream::out);
  if (!bitstreamFileOut)
  {
    fprintf(stderr, "\nfailed to open output bitstream file `%s' for writing\n", m_outputBitstreamFileName.c_str());
    exit(EXIT_FAILURE);
  }

  AccessUnit outAccessUnit;
  while (!!bitstreamFile)
  {
    streampos location = bitstreamFile.tellg();
    AnnexBStats stats = AnnexBStats();
    InputNALUnit inNalu;

    byteStreamNALUnit(bytestream, inNalu.getBitstream().getFifo(), stats);

    Bool bNewPicture = false;
    if (inNalu.getBitstream().getFifo().empty())
    {
      fprintf(stderr, "Warning: Attempt to extract an empty NAL unit\n");
    }
    else
    {
      read(inNalu);
      m_pcSlice = m_cTDecTop.getApcSlicePilot();
      // decode HLS, skipping cabac decoding and reconstruction
      bNewPicture = m_cTDecTop.decode(inNalu, iSkipFrame, iPOCLastDisplay, true);

      if (bNewPicture)
      {
        bitstreamFile.clear();
        bitstreamFile.seekg(location - streamoff(3));
        bytestream.reset();
      }
    }

    if ((bNewPicture || !bitstreamFile || inNalu.m_nalUnitType == NAL_UNIT_EOS) &&
      !m_cTDecTop.getFirstSliceInSequence())
    {
      m_cTDecTop.getPcPic()->setReconMark(true);
      if (bitstreamFile || inNalu.m_nalUnitType == NAL_UNIT_EOS)
      {
        m_cTDecTop.setFirstSliceInPicture(true);
      }
    }

    if ((inNalu.m_nalUnitType == NAL_UNIT_VPS || inNalu.m_nalUnitType == NAL_UNIT_SPS ||
      inNalu.m_nalUnitType == NAL_UNIT_PPS))
    {
      continue;
    }
    else if (!m_mctsExtractionInfoPresent && inNalu.isSei())
    {
      // search matching EIS and push parameter sets into AU
      m_cTDecTop.xParsePrefixSEImessages();
      if (!m_cTDecTop.getSEIs().empty())
      {
        xExtractSuitableParameterSets(
          getSeisByType(m_cTDecTop.getSEIs(), SEI::TEMP_MOTION_CONSTRAINED_TILE_SETS),
          getSeisByType(m_cTDecTop.getSEIs(), SEI::MCTS_EXTRACTION_INFO_SET),
          outAccessUnit);
      }
    }
    else if (!bNewPicture && m_mctsExtractionInfoPresent && inNalu.isSlice() && xIsNaluWithinMCTSSet(m_targetMctsIdx))
    {
      //rewrite input slice 

      // setup
      UInt numCtusInOutputPictures = m_cTDecTop.getPcPic()->getPicSym()->getTComTile(m_targetMctsIdx)->getTileHeightInCtus() * m_cTDecTop.getPcPic()->getPicSym()->getTComTile(m_targetMctsIdx)->getTileWidthInCtus();
      UInt numCtusInInputPictures = m_cTDecTop.getPcPic()->getNumberOfCtusInFrame();
 
      m_cSlicePicSym.setNumberOfCtusInFrame(m_cTDecTop.getPcPic()->getPicSym(), numCtusInOutputPictures);
      m_pcSlice->setPic(m_cTDecTop.getPcPic());
      m_pcSlice->setSliceSegmentCurStartCtuTsAddr(0);
      OutputNALUnit outNalu(m_pcSlice->getNalUnitType(), m_pcSlice->getTLayer());
      m_cEntropyCoder.setEntropyCoder(&m_cCavlcCoder);
      m_cEntropyCoder.setBitstream(&outNalu.m_Bitstream);
      m_cEntropyCoder.encodeSliceHeader(m_pcSlice);

      //convert
      xInputToOutputSliceNaluConversion(inNalu, outNalu, m_pcSlice);

      //write to file
      outAccessUnit.push_back(new NALUnitEBSP(outNalu));
      xWriteOutput(bitstreamFileOut, outAccessUnit);
      outAccessUnit.clear();

      //m_cTDecTop.getPcPic()->setNumberOfCtusInFrame(numCtusInInputPictures);
      m_cSlicePicSym.setNumberOfCtusInFrame(m_cTDecTop.getPcPic()->getPicSym(), numCtusInInputPictures);

      // console output
      TChar c = (m_pcSlice->isIntra() ? 'I' : m_pcSlice->isInterP() ? 'P' : 'B');
      if (!m_pcSlice->isReferenced())
      {
        c += 32;
      }
      printf("POC %4d TId: %1d ( %c-SLICE, QP%3d ) ", m_pcSlice->getPOC(),
        m_pcSlice->getTLayer(),
        c,
        m_pcSlice->getSliceQp());
      printf(" %10d bits\n", (int)inNalu.getBitstream().getFifo().size());
    }
  }

  if (!m_mctsExtractionInfoPresent)
  {
    fprintf(stderr, "\nInput bitstream file `%s' does not contain MCTS extraction information for target MCTS index %d\n", m_inputBitstreamFileName.c_str(), m_targetMctsIdx);
  }

  // delete buffers
  m_cTDecTop.deletePicBuffer();

  // destroy internal classes
  xDestroyMctsExtLib();

}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================


Void TAppMctsExtTop::xCreateMctsExtLib()
{
  // create decoder class
  m_cTDecTop.create();
}

Void TAppMctsExtTop::xDestroyMctsExtLib()
{
  // destroy decoder class
  m_cTDecTop.destroy();
}

Void TAppMctsExtTop::xInitMctsExtLib()
{
  // initialize decoder class
  m_cTDecTop.init();
}


Void TAppMctsExtTop::xExtractSuitableParameterSets(SEIMessages SEIMctsSEIs, SEIMessages SEIMctsEisSEIs, AccessUnit &accessUnit)
{
  if (SEIMctsSEIs.size() && SEIMctsEisSEIs.size())
  {
    SEIMCTSExtractionInfoSet* SEIMCTSExtractionInfoSetSEI = (SEIMCTSExtractionInfoSet*) *(SEIMctsEisSEIs.begin());
    for (std::vector<SEIMCTSExtractionInfoSet::MCTSExtractionInfo>::iterator EisIter = SEIMCTSExtractionInfoSetSEI->m_MCTSExtractionInfoSets.begin(); EisIter != SEIMCTSExtractionInfoSetSEI->m_MCTSExtractionInfoSets.end(); EisIter++)
    {
      for (int j = 0; j < EisIter->m_idxOfMctsInSet.size() && !m_mctsExtractionInfoPresent; j++)
      {
        for (int k = 0; k < EisIter->m_idxOfMctsInSet[j].size() && !m_mctsExtractionInfoPresent; k++)
        {
          if (EisIter->m_idxOfMctsInSet[j][k] == m_targetMctsIdx)
          {
            std::vector<TComInputBitstream> vps_rbsps;
            vps_rbsps.resize(EisIter->m_vpsRbspData.size());
            for (int jj = 0; jj < EisIter->m_vpsRbspData.size(); jj++)
            {
              for (int kk = 0; kk < EisIter->m_vpsRbspDataLength[jj]; kk++)
              {
                vps_rbsps[jj].getFifo().push_back(EisIter->m_vpsRbspData[jj][kk]);
              }
              OutputNALUnit vpsout(NAL_UNIT_VPS, 0, 0);
              vpsout.m_Bitstream.getFIFO() = vps_rbsps[jj].getFifo();
              accessUnit.push_back(new NALUnitEBSP(vpsout));
            }

            std::vector<TComInputBitstream> sps_rbsps;
            sps_rbsps.resize(EisIter->m_spsRbspData.size());
            for (int jj = 0; jj < EisIter->m_spsRbspData.size(); jj++)
            {
              for (int kk = 0; kk < EisIter->m_spsRbspDataLength[jj]; kk++)
              {
                sps_rbsps[jj].getFifo().push_back(EisIter->m_spsRbspData[jj][kk]);
              }
              OutputNALUnit spsout(NAL_UNIT_SPS, 0, 0);
              spsout.m_Bitstream.getFIFO() = sps_rbsps[jj].getFifo();
              accessUnit.push_back(new NALUnitEBSP(spsout));
            }

            std::vector<TComInputBitstream> pps_rbsps;
            pps_rbsps.resize(EisIter->m_ppsRbspData.size());
            for (int jj = 0; jj < EisIter->m_ppsRbspData.size(); jj++)
            {
              for (int kk = 0; kk < EisIter->m_ppsRbspDataLength[jj]; kk++)
              {
                pps_rbsps[jj].getFifo().push_back(EisIter->m_ppsRbspData[jj][kk]);
              }
              OutputNALUnit ppsout(NAL_UNIT_PPS, 0, 0);
              ppsout.m_Bitstream.getFIFO() = pps_rbsps[jj].getFifo();
              accessUnit.push_back(new NALUnitEBSP(ppsout));
            }
            TComSPS* nestedSps = new TComSPS();
            m_cEntropyDecoder.setEntropyDecoder(&m_cCavlcDecoder);
            m_cEntropyDecoder.setBitstream(&sps_rbsps[0]);
            m_cEntropyDecoder.decodeSPS(nestedSps);

            printf("MCTS extraction info for target MCTS index %d found\n", m_targetMctsIdx);
            printf("Output bitstream resolution: %dx%d\n\n", nestedSps->getPicWidthInLumaSamples(), nestedSps->getPicHeightInLumaSamples());

            m_mctsExtractionInfoPresent = true;
          }
        }
      }
    }
  }
}


Bool TAppMctsExtTop::xIsNaluWithinMCTSSet(Int mcts_id)
{
  UInt currNaluTSSliceSegAddr = m_pcSlice->getSliceSegmentCurStartCtuTsAddr();
  UInt firstCtuAddrTSInMcts = m_cTDecTop.getPcPic()->getPicSym()->getCtuRsToTsAddrMap(m_cTDecTop.getPcPic()->getPicSym()->getTComTile(m_targetMctsIdx)->getFirstCtuRsAddr());
  UInt lastCtuAddrTSInMcts = -1;

  if (mcts_id < (m_cTDecTop.getPcPic()->getPicSym()->getNumTiles() - 1))
  {// not last tile
    lastCtuAddrTSInMcts = m_cTDecTop.getPcPic()->getPicSym()->getCtuRsToTsAddrMap(m_cTDecTop.getPcPic()->getPicSym()->getTComTile(m_targetMctsIdx + 1)->getFirstCtuRsAddr()) - 1;
  }
  else
  {
    lastCtuAddrTSInMcts = m_cTDecTop.getPcPic()->getNumberOfCtusInFrame() - 1;
  }

  return (currNaluTSSliceSegAddr >= firstCtuAddrTSInMcts && currNaluTSSliceSegAddr <= lastCtuAddrTSInMcts);
}


Void TAppMctsExtTop::xInputToOutputSliceNaluConversion(InputNALUnit &inNalu, OutputNALUnit &outNalu, TComSlice* curSlice)
{
  const UInt uiNumSubstreams = curSlice->getNumberOfSubstreamSizes() + 1;

  TComInputBitstream **ppcSubstreams = NULL;
  ppcSubstreams = new TComInputBitstream*[uiNumSubstreams];
  for (UInt ui = 0; ui < uiNumSubstreams; ui++)
  {
    ppcSubstreams[ui] = inNalu.getBitstream().extractSubstream(ui + 1 < uiNumSubstreams ? (curSlice->getSubstreamSize(ui) << 3) : inNalu.getBitstream().getNumBitsLeft());
  }

  TComOutputBitstream  *pcBitstreamRedirect;
  pcBitstreamRedirect = new TComOutputBitstream;

  // Append substreams...
  TComOutputBitstream *pcOut = pcBitstreamRedirect;
  std::vector<TComOutputBitstream> substreamsOut(uiNumSubstreams);

  for (UInt ui = 0; ui < uiNumSubstreams; ui++)
  {
    std::vector<uint8_t> &bufIn = ppcSubstreams[ui]->getFifo();
    std::vector<uint8_t> &bufOut = substreamsOut[ui].getFIFO();
    bufOut.resize(bufIn.size());
    bufOut = bufIn;
    pcOut->addSubstream(&(substreamsOut[ui]));
  }

  // Byte-align
  outNalu.m_Bitstream.writeByteAlignment();

  // Perform bitstream concatenation
  if (pcOut->getNumberOfWrittenBits() > 0)
  {
    outNalu.m_Bitstream.addSubstream(pcOut);
  }

  // clean-up
  for (UInt ui = 0; ui < uiNumSubstreams; ui++)
  {
    delete ppcSubstreams[ui];
  }
  delete[] ppcSubstreams;
  delete pcBitstreamRedirect;

  return;

}

Void TAppMctsExtTop::xWriteOutput(std::ostream& bitstreamFile, const AccessUnit accessUnit)
{
  for (AccessUnit::const_iterator it = accessUnit.begin(); it != accessUnit.end(); it++)
  {
    const NALUnitEBSP& nalu = **it;

    static const UChar start_code_prefix[] = { 0,0,0,1 };
    if (it == accessUnit.begin() || nalu.m_nalUnitType == NAL_UNIT_VPS || nalu.m_nalUnitType == NAL_UNIT_SPS || nalu.m_nalUnitType == NAL_UNIT_PPS)
    {
      /* From AVC, When any of the following conditions are fulfilled, the
      * zero_byte syntax element shall be present:
      *  - the nal_unit_type within the nal_unit() is equal to 7 (sequence
      *    parameter set) or 8 (picture parameter set),
      *  - the byte stream NAL unit syntax structure contains the first NAL
      *    unit of an access unit in decoding order, as specified by subclause
      *    7.4.1.2.3.
      */
      bitstreamFile.write(reinterpret_cast<const TChar*>(start_code_prefix), 4);
    }
    else
    {
      bitstreamFile.write(reinterpret_cast<const TChar*>(start_code_prefix + 1), 3);
    }
    bitstreamFile << nalu.m_nalUnitData.str();

  }
}
#endif
