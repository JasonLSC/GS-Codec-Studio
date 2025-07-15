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

/** \file     TAppDecTop.cpp
    \brief    Decoder application class
*/

#include <list>
#include <vector>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>

#include "TAppDecTop.h"
#include "TLibDecoder/AnnexBread.h"
#include "TLibDecoder/NALread.h"
#if RExt__DECODER_DEBUG_BIT_STATISTICS
#include "TLibCommon/TComCodingStatistics.h"
#endif

//! \ingroup TAppDecoder
//! \{

// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================

TAppDecTop::TAppDecTop()
: m_iPOCLastDisplay(-MAX_INT)
 ,m_pcSeiColourRemappingInfoPrevious(NULL)
{
}

Void TAppDecTop::create()
{
}

Void TAppDecTop::destroy()
{
  m_bitstreamFileName.clear();
  m_reconFileName.clear();
#if JVET_X0048_X0103_FILM_GRAIN
  m_SEIFGSFileName.clear();
#endif
#if SHUTTER_INTERVAL_SEI_PROCESSING
  m_shutterIntervalPostFileName.clear();
#endif
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/**
 - create internal class
 - initialize internal class
 - until the end of the bitstream, call decoding function in TDecTop class
 - delete allocated buffers
 - destroy internal class
 .
 */
Void TAppDecTop::decode()
{
  Int                 poc;
  TComList<TComPic*>* pcListPic = NULL;

  ifstream bitstreamFile(m_bitstreamFileName.c_str(), ifstream::in | ifstream::binary);
  if (!bitstreamFile)
  {
    fprintf(stderr, "\nfailed to open bitstream file `%s' for reading\n", m_bitstreamFileName.c_str());
    exit(EXIT_FAILURE);
  }

  InputByteStream bytestream(bitstreamFile);

  if (!m_outputDecodedSEIMessagesFilename.empty() && m_outputDecodedSEIMessagesFilename!="-")
  {
    m_seiMessageFileStream.open(m_outputDecodedSEIMessagesFilename.c_str(), std::ios::out);
    if (!m_seiMessageFileStream.is_open() || !m_seiMessageFileStream.good())
    {
      fprintf(stderr, "\nUnable to open file `%s' for writing decoded SEI messages\n", m_outputDecodedSEIMessagesFilename.c_str());
      exit(EXIT_FAILURE);
    }
  }

  // create & initialize internal classes
  xCreateDecLib();
  xInitDecLib  ();
  m_iPOCLastDisplay += m_iSkipFrame;      // set the last displayed POC correctly for skip forward.

  // clear contents of colour-remap-information-SEI output file
  if (!m_colourRemapSEIFileName.empty())
  {
    std::ofstream ofile(m_colourRemapSEIFileName.c_str());
    if (!ofile.good() || !ofile.is_open())
    {
      fprintf(stderr, "\nUnable to open file '%s' for writing colour-remap-information-SEI video\n", m_colourRemapSEIFileName.c_str());
      exit(EXIT_FAILURE);
    }
  }

  // clear contents of annotated-Regions-SEI output file
  if (!m_annotatedRegionsSEIFileName.empty())
  {
    std::ofstream ofile(m_annotatedRegionsSEIFileName.c_str());
    if (!ofile.good() || !ofile.is_open())
    {
      fprintf(stderr, "\nUnable to open file '%s' for writing annotated-Regions-SEI\n", m_annotatedRegionsSEIFileName.c_str());
      exit(EXIT_FAILURE);
    }
  }

  // main decoder loop
  Bool openedReconFile = false; // reconstruction file not yet opened. (must be performed after SPS is seen)
#if JVET_X0048_X0103_FILM_GRAIN
  Bool openedSEIFGSFile = false; // reconstruction file (with FGS) not yet opened. (must be performed after SPS is seen)
#endif
  Bool loopFiltered = false;
#if SHUTTER_INTERVAL_SEI_PROCESSING
  Bool openedPostFile = false;
  setShutterFilterFlag(!m_shutterIntervalPostFileName.empty());   // not apply shutter interval SEI processing if filename is not specified.
  m_cTDecTop.setShutterFilterFlag(getShutterFilterFlag());
#endif

  while (!!bitstreamFile)
  {
    /* location serves to work around a design fault in the decoder, whereby
     * the process of reading a new slice that is the first slice of a new frame
     * requires the TDecTop::decode() method to be called again with the same
     * nal unit. */
#if RExt__DECODER_DEBUG_BIT_STATISTICS
    TComCodingStatistics::TComCodingStatisticsData backupStats(TComCodingStatistics::GetStatistics());
    streampos location = bitstreamFile.tellg() - streampos(bytestream.GetNumBufferedBytes());
#else
    streampos location = bitstreamFile.tellg();
#endif
    AnnexBStats stats = AnnexBStats();

    InputNALUnit nalu;
    byteStreamNALUnit(bytestream, nalu.getBitstream().getFifo(), stats);

    // call actual decoding function
    Bool bNewPicture = false;
    if (nalu.getBitstream().getFifo().empty())
    {
      /* this can happen if the following occur:
       *  - empty input file
       *  - two back-to-back start_code_prefixes
       *  - start_code_prefix immediately followed by EOF
       */
      fprintf(stderr, "Warning: Attempt to decode an empty NAL unit\n");
    }
    else
    {
      read(nalu);
      if( (m_iMaxTemporalLayer >= 0 && nalu.m_temporalId > m_iMaxTemporalLayer) || !isNaluWithinTargetDecLayerIdSet(&nalu)  )
      {
        bNewPicture = false;
      }
      else
      {
        bNewPicture = m_cTDecTop.decode(nalu, m_iSkipFrame, m_iPOCLastDisplay);
        if (bNewPicture)
        {
          bitstreamFile.clear();
          /* location points to the current nalunit payload[1] due to the
           * need for the annexB parser to read three extra bytes.
           * [1] except for the first NAL unit in the file
           *     (but bNewPicture doesn't happen then) */
#if RExt__DECODER_DEBUG_BIT_STATISTICS
          bitstreamFile.seekg(location);
          bytestream.reset();
          TComCodingStatistics::SetStatistics(backupStats);
#else
          bitstreamFile.seekg(location-streamoff(3));
          bytestream.reset();
#endif
        }
      }
    }

    if ( (bNewPicture || !bitstreamFile || nalu.m_nalUnitType == NAL_UNIT_EOS) &&
        !m_cTDecTop.getFirstSliceInSequence () )
    {
      if (!loopFiltered || bitstreamFile)
      {
        m_cTDecTop.executeLoopFilters(poc, pcListPic);
      }
      loopFiltered = (nalu.m_nalUnitType == NAL_UNIT_EOS);
      if (nalu.m_nalUnitType == NAL_UNIT_EOS)
      {
        m_cTDecTop.setFirstSliceInSequence(true);
      }
    }
    else if ( (bNewPicture || !bitstreamFile || nalu.m_nalUnitType == NAL_UNIT_EOS ) &&
              m_cTDecTop.getFirstSliceInSequence () ) 
    {
      m_cTDecTop.setFirstSliceInPicture (true);
    }

    if( pcListPic )
    {
      if ( (!m_reconFileName.empty()) && (!openedReconFile) )
      {
        const BitDepths &bitDepths=pcListPic->front()->getPicSym()->getSPS().getBitDepths(); // use bit depths of first reconstructed picture.
        for (UInt channelType = 0; channelType < MAX_NUM_CHANNEL_TYPE; channelType++)
        {
          if (m_outputBitDepth[channelType] == 0)
          {
            m_outputBitDepth[channelType] = bitDepths.recon[channelType];
          }
        }

        m_cTVideoIOYuvReconFile.open( m_reconFileName, true, m_outputBitDepth, m_outputBitDepth, bitDepths.recon ); // write mode
        openedReconFile = true;
      }
#if JVET_X0048_X0103_FILM_GRAIN
      // Initialize file handle to write output with film grain
      if ((!m_SEIFGSFileName.empty()) && (!openedSEIFGSFile))
      {
        const BitDepths &bitDepths = pcListPic->front()->getPicSym()->getSPS().getBitDepths(); // use bit depths of first reconstructed picture.
        for (UInt channelType = 0; channelType < MAX_NUM_CHANNEL_TYPE; channelType++)
        {
          if (m_outputBitDepth[channelType] == 0)
          {
            m_outputBitDepth[channelType] = bitDepths.recon[channelType];
          }
        }

        m_cTVideoIOYuvSEIFGSFile.open(m_SEIFGSFileName, true, m_outputBitDepth, m_outputBitDepth, bitDepths.recon); // write mode
        openedSEIFGSFile = true;
      }
#endif

#if SHUTTER_INTERVAL_SEI_PROCESSING
      TComList<TComPic*>::iterator iterPic = pcListPic->begin();
      TComPic* pcPic = *(iterPic);
      SEIMessages shutterIntervalInfo = getSeisByType(pcPic->getSEIs(), SEI::SHUTTER_INTERVAL_INFO);
      if (!m_shutterIntervalPostFileName.empty())
      {
        if (shutterIntervalInfo.size() > 0)
        {
          SEIShutterIntervalInfo *seiShutterIntervalInfo = (SEIShutterIntervalInfo*) *(shutterIntervalInfo.begin());
          if (!seiShutterIntervalInfo->m_siiFixedSIwithinCLVS)
          {
            UInt arraySize = seiShutterIntervalInfo->m_siiMaxSubLayersMinus1 + 1;
            UInt numUnitsLFR = seiShutterIntervalInfo->m_siiSubLayerNumUnitsInSI[0];
            UInt numUnitsHFR = seiShutterIntervalInfo->m_siiSubLayerNumUnitsInSI[arraySize - 1];
            setShutterFilterFlag(numUnitsLFR == 2 * numUnitsHFR);

            const TComSPS* activeSPS = &(pcListPic->front()->getPicSym()->getSPS());
            if (numUnitsLFR == 2 * numUnitsHFR && activeSPS->getMaxTLayers() == 1 && activeSPS->getMaxDecPicBuffering(0) == 1)
            {
              fprintf(stderr, "Warning: Shutter Interval SEI message processing is disabled for single TempLayer and single frame in DPB\n");
              setShutterFilterFlag(false);
            }
          }
          else
          {
            fprintf(stderr, "Warning: Shutter Interval SEI message processing is disabled for fixed shutter interval case\n");
            setShutterFilterFlag(false);
          }
        }
        else
        {
          fprintf(stderr, "Warning: Shutter Interval information should be specified in SII-SEI message\n");
          setShutterFilterFlag(false);
        }
      }

      if ((!m_shutterIntervalPostFileName.empty()) && (!openedPostFile) && getShutterFilterFlag())
      {
        const BitDepths &bitDepths = pcListPic->front()->getPicSym()->getSPS().getBitDepths();
        for (UInt channelType = 0; channelType < MAX_NUM_CHANNEL_TYPE; channelType++)
        {
          if (m_outputBitDepth[channelType] == 0)
          {
            m_outputBitDepth[channelType] = bitDepths.recon[channelType];
          }
        }

        std::ofstream ofile(m_shutterIntervalPostFileName.c_str());
        if (!ofile.good() || !ofile.is_open())
        {
          fprintf(stderr, "\nUnable to open file '%s' for writing shutter-interval-SEI video\n", m_shutterIntervalPostFileName.c_str());
          exit(EXIT_FAILURE);
        }

        m_cTVideoIOYuvSIIPostFile.open(m_shutterIntervalPostFileName, true, m_outputBitDepth, m_outputBitDepth, bitDepths.recon); // write mode
        openedPostFile = true;
      }
#endif
      // write reconstruction to file
      if( bNewPicture )
      {
        xWriteOutput( pcListPic, nalu.m_temporalId );
      }
      if ( (bNewPicture || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA) && m_cTDecTop.getNoOutputPriorPicsFlag() )
      {
        m_cTDecTop.checkNoOutputPriorPics( pcListPic );
        m_cTDecTop.setNoOutputPriorPicsFlag (false);
      }
      if ( bNewPicture &&
           (   nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL
            || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP
            || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA_N_LP
            || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA_W_RADL
            || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA_W_LP ) )
      {
        xFlushOutput( pcListPic );
      }
      if (nalu.m_nalUnitType == NAL_UNIT_EOS)
      {
        xWriteOutput( pcListPic, nalu.m_temporalId );
        m_cTDecTop.setFirstSliceInPicture (false);
      }
      // write reconstruction to file -- for additional bumping as defined in C.5.2.3
      if(!bNewPicture && nalu.m_nalUnitType >= NAL_UNIT_CODED_SLICE_TRAIL_N && nalu.m_nalUnitType <= NAL_UNIT_RESERVED_VCL31)
      {
        xWriteOutput( pcListPic, nalu.m_temporalId );
      }
    }
  }

  xFlushOutput( pcListPic );
  // delete buffers
  m_cTDecTop.deletePicBuffer();

  // destroy internal classes
  xDestroyDecLib();
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

Void TAppDecTop::xCreateDecLib()
{
  // create decoder class
  m_cTDecTop.create();
}

Void TAppDecTop::xDestroyDecLib()
{
  if ( !m_reconFileName.empty() )
  {
    m_cTVideoIOYuvReconFile.close();
  }
#if JVET_X0048_X0103_FILM_GRAIN
  if (!m_SEIFGSFileName.empty())
  {
    m_cTVideoIOYuvSEIFGSFile.close();
  }
#endif
#if SHUTTER_INTERVAL_SEI_PROCESSING
  if (!m_shutterIntervalPostFileName.empty() && getShutterFilterFlag())
  {
    m_cTVideoIOYuvSIIPostFile.close();
  }
#endif

  // destroy decoder class
  m_cTDecTop.destroy();

  if (m_pcSeiColourRemappingInfoPrevious != NULL)
  {
    delete m_pcSeiColourRemappingInfoPrevious;
    m_pcSeiColourRemappingInfoPrevious = NULL;
  }
  m_arObjects.clear();
  m_arLabels.clear();
}

Void TAppDecTop::xInitDecLib()
{
  // initialize decoder class
  m_cTDecTop.init();
  m_cTDecTop.setDecodedPictureHashSEIEnabled(m_decodedPictureHashSEIEnabled);
#if MCTS_ENC_CHECK
  m_cTDecTop.setTMctsCheckEnabled(m_tmctsCheck);
#endif
#if O0043_BEST_EFFORT_DECODING
  m_cTDecTop.setForceDecodeBitDepth(m_forceDecodeBitDepth);
#endif
  if (!m_outputDecodedSEIMessagesFilename.empty())
  {
    std::ostream &os=m_seiMessageFileStream.is_open() ? m_seiMessageFileStream : std::cout;
    m_cTDecTop.setDecodedSEIMessageOutputStream(&os);
  }
  if (m_pcSeiColourRemappingInfoPrevious != NULL)
  {
    delete m_pcSeiColourRemappingInfoPrevious;
    m_pcSeiColourRemappingInfoPrevious = NULL;
  }
  m_arObjects.clear();
  m_arLabels.clear();
}

/** \param pcListPic list of pictures to be written to file
    \param tId       temporal sub-layer ID
 */
Void TAppDecTop::xWriteOutput( TComList<TComPic*>* pcListPic, UInt tId )
{
  if (pcListPic->empty())
  {
    return;
  }

  TComList<TComPic*>::iterator iterPic   = pcListPic->begin();
  Int numPicsNotYetDisplayed = 0;
  Int dpbFullness = 0;
  const TComSPS* activeSPS = &(pcListPic->front()->getPicSym()->getSPS());
  UInt numReorderPicsHighestTid;
  UInt maxDecPicBufferingHighestTid;
  UInt maxNrSublayers = activeSPS->getMaxTLayers();

  if(m_iMaxTemporalLayer == -1 || m_iMaxTemporalLayer >= maxNrSublayers)
  {
    numReorderPicsHighestTid = activeSPS->getNumReorderPics(maxNrSublayers-1);
    maxDecPicBufferingHighestTid =  activeSPS->getMaxDecPicBuffering(maxNrSublayers-1); 
  }
  else
  {
    numReorderPicsHighestTid = activeSPS->getNumReorderPics(m_iMaxTemporalLayer);
    maxDecPicBufferingHighestTid = activeSPS->getMaxDecPicBuffering(m_iMaxTemporalLayer); 
  }

  while (iterPic != pcListPic->end())
  {
    TComPic* pcPic = *(iterPic);
    if(pcPic->getOutputMark() && pcPic->getPOC() > m_iPOCLastDisplay)
    {
       numPicsNotYetDisplayed++;
      dpbFullness++;
    }
    else if(pcPic->getSlice( 0 )->isReferenced())
    {
      dpbFullness++;
    }
    iterPic++;
  }

  iterPic = pcListPic->begin();

  if (numPicsNotYetDisplayed>2)
  {
    iterPic++;
  }

  TComPic* pcPic = *(iterPic);
  if (numPicsNotYetDisplayed>2 && pcPic->isField()) //Field Decoding
  {
    TComList<TComPic*>::iterator endPic   = pcListPic->end();
    endPic--;
    iterPic   = pcListPic->begin();
    while (iterPic != endPic)
    {
      TComPic* pcPicTop = *(iterPic);
      iterPic++;
      TComPic* pcPicBottom = *(iterPic);

      if ( pcPicTop->getOutputMark() && pcPicBottom->getOutputMark() &&
          (numPicsNotYetDisplayed >  numReorderPicsHighestTid || dpbFullness > maxDecPicBufferingHighestTid) &&
          (!(pcPicTop->getPOC()%2) && pcPicBottom->getPOC() == pcPicTop->getPOC()+1) &&
          (pcPicTop->getPOC() == m_iPOCLastDisplay+1 || m_iPOCLastDisplay < 0))
      {
        // write to file
        numPicsNotYetDisplayed = numPicsNotYetDisplayed-2;
        if ( !m_reconFileName.empty() )
        {
          const Window &conf = pcPicTop->getConformanceWindow();
          const Window  defDisp = m_respectDefDispWindow ? pcPicTop->getDefDisplayWindow() : Window();
          const Bool isTff = pcPicTop->isTopField();

          Bool display = true;
          if( m_decodedNoDisplaySEIEnabled )
          {
            SEIMessages noDisplay = getSeisByType(pcPic->getSEIs(), SEI::NO_DISPLAY );
            const SEINoDisplay *nd = ( noDisplay.size() > 0 ) ? (SEINoDisplay*) *(noDisplay.begin()) : NULL;
            if( (nd != NULL) && nd->m_noDisplay )
            {
              display = false;
            }
          }

          if (display)
          {
            m_cTVideoIOYuvReconFile.write( pcPicTop->getPicYuvRec(), pcPicBottom->getPicYuvRec(),
                                           m_outputColourSpaceConvert,
                                           conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
                                           conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
                                           conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
                                           conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(), NUM_CHROMA_FORMAT, isTff );
          }
        }

        // update POC of display order
        m_iPOCLastDisplay = pcPicBottom->getPOC();

        // erase non-referenced picture in the reference picture list after display
        if ( !pcPicTop->getSlice(0)->isReferenced() && pcPicTop->getReconMark() == true )
        {
          pcPicTop->setReconMark(false);

          // mark it should be extended later
          pcPicTop->getPicYuvRec()->setBorderExtension( false );
        }
        if ( !pcPicBottom->getSlice(0)->isReferenced() && pcPicBottom->getReconMark() == true )
        {
          pcPicBottom->setReconMark(false);

          // mark it should be extended later
          pcPicBottom->getPicYuvRec()->setBorderExtension( false );
        }
        pcPicTop->setOutputMark(false);
        pcPicBottom->setOutputMark(false);
      }
    }
  }
  else if (!pcPic->isField()) //Frame Decoding
  {
    iterPic = pcListPic->begin();

    while (iterPic != pcListPic->end())
    {
      pcPic = *(iterPic);

      if(pcPic->getOutputMark() && pcPic->getPOC() > m_iPOCLastDisplay &&
        (numPicsNotYetDisplayed >  numReorderPicsHighestTid || dpbFullness > maxDecPicBufferingHighestTid))
      {
        // write to file
         numPicsNotYetDisplayed--;
        if(pcPic->getSlice(0)->isReferenced() == false)
        {
          dpbFullness--;
        }

        if ( !m_reconFileName.empty() )
        {
          const Window &conf    = pcPic->getConformanceWindow();
          const Window  defDisp = m_respectDefDispWindow ? pcPic->getDefDisplayWindow() : Window();

          m_cTVideoIOYuvReconFile.write( pcPic->getPicYuvRec(),
                                         m_outputColourSpaceConvert,
                                         conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
                                         conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
                                         conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
                                         conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(),
                                         NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range  );
        }

#if JVET_X0048_X0103_FILM_GRAIN
        // Perform FGS on decoded frame and write to output FGS file
        if (!m_SEIFGSFileName.empty())
        {
          const Window &conf = pcPic->getConformanceWindow();
          const Window  defDisp = m_respectDefDispWindow ? pcPic->getDefDisplayWindow() : Window();
          m_cTVideoIOYuvSEIFGSFile.write(pcPic->getPicYuvDisp(),
                                          m_outputColourSpaceConvert,
                                          conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
                                          conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
                                          conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
                                          conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(),
                                          NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range);
        }
#endif
        if (!m_annotatedRegionsSEIFileName.empty())
        {
          xOutputAnnotatedRegions(pcPic);
        }

        if (!m_colourRemapSEIFileName.empty())
        {
          xOutputColourRemapPic(pcPic);
        }

#if SHUTTER_INTERVAL_SEI_PROCESSING
        if (!m_shutterIntervalPostFileName.empty() && getShutterFilterFlag())
        {
          pcPic->xOutputPostFilteredPic(pcPic, pcListPic);

          const Window &conf = pcPic->getConformanceWindow();
          const Window  defDisp = m_respectDefDispWindow ? pcPic->getDefDisplayWindow() : Window();

          m_cTVideoIOYuvSIIPostFile.write( pcPic->getPicYuvPostRec(),
                                        m_outputColourSpaceConvert,
                                        conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
                                        conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
                                        conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
                                        conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(),
                                        NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range  );
        }
#endif

        // update POC of display order
        m_iPOCLastDisplay = pcPic->getPOC();

        // erase non-referenced picture in the reference picture list after display
        if ( !pcPic->getSlice(0)->isReferenced() && pcPic->getReconMark() == true )
        {
          pcPic->setReconMark(false);

          // mark it should be extended later
          pcPic->getPicYuvRec()->setBorderExtension( false );
        }
        pcPic->setOutputMark(false);
      }

      iterPic++;
    }
  }
}

/** \param pcListPic list of pictures to be written to file
 */
Void TAppDecTop::xFlushOutput( TComList<TComPic*>* pcListPic )
{
  if(!pcListPic || pcListPic->empty())
  {
    return;
  }
  TComList<TComPic*>::iterator iterPic   = pcListPic->begin();

  iterPic   = pcListPic->begin();
  TComPic* pcPic = *(iterPic);

  if (pcPic->isField()) //Field Decoding
  {
    TComList<TComPic*>::iterator endPic   = pcListPic->end();
    endPic--;
    TComPic *pcPicTop, *pcPicBottom = NULL;
    while (iterPic != endPic)
    {
      pcPicTop = *(iterPic);
      iterPic++;
      pcPicBottom = *(iterPic);

      if ( pcPicTop->getOutputMark() && pcPicBottom->getOutputMark() && !(pcPicTop->getPOC()%2) && (pcPicBottom->getPOC() == pcPicTop->getPOC()+1) )
      {
        // write to file
        if ( !m_reconFileName.empty() )
        {
          const Window &conf = pcPicTop->getConformanceWindow();
          const Window  defDisp = m_respectDefDispWindow ? pcPicTop->getDefDisplayWindow() : Window();
          const Bool isTff = pcPicTop->isTopField();
          m_cTVideoIOYuvReconFile.write( pcPicTop->getPicYuvRec(), pcPicBottom->getPicYuvRec(),
                                         m_outputColourSpaceConvert,
                                         conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
                                         conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
                                         conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
                                         conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(), NUM_CHROMA_FORMAT, isTff );
        }

        // update POC of display order
        m_iPOCLastDisplay = pcPicBottom->getPOC();

        // erase non-referenced picture in the reference picture list after display
        if ( !pcPicTop->getSlice(0)->isReferenced() && pcPicTop->getReconMark() == true )
        {
          pcPicTop->setReconMark(false);

          // mark it should be extended later
          pcPicTop->getPicYuvRec()->setBorderExtension( false );
        }
        if ( !pcPicBottom->getSlice(0)->isReferenced() && pcPicBottom->getReconMark() == true )
        {
          pcPicBottom->setReconMark(false);

          // mark it should be extended later
          pcPicBottom->getPicYuvRec()->setBorderExtension( false );
        }
        pcPicTop->setOutputMark(false);
        pcPicBottom->setOutputMark(false);

        if(pcPicTop)
        {
          pcPicTop->destroy();
          delete pcPicTop;
          pcPicTop = NULL;
        }
      }
    }
    if(pcPicBottom)
    {
      pcPicBottom->destroy();
      delete pcPicBottom;
      pcPicBottom = NULL;
    }
  }
  else //Frame decoding
  {
    while (iterPic != pcListPic->end())
    {
      pcPic = *(iterPic);

      if ( pcPic->getOutputMark() )
      {
        // write to file
        if ( !m_reconFileName.empty() )
        {
          const Window &conf    = pcPic->getConformanceWindow();
          const Window  defDisp = m_respectDefDispWindow ? pcPic->getDefDisplayWindow() : Window();

          m_cTVideoIOYuvReconFile.write( pcPic->getPicYuvRec(),
                                         m_outputColourSpaceConvert,
                                         conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
                                         conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
                                         conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
                                         conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(),
                                         NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range );
        }

#if JVET_X0048_X0103_FILM_GRAIN
        // Perform FGS on decoded frame and write to output FGS file
        if (!m_SEIFGSFileName.empty())
        {
          const Window &conf = pcPic->getConformanceWindow();
          const Window  defDisp = m_respectDefDispWindow ? pcPic->getDefDisplayWindow() : Window();
          m_cTVideoIOYuvSEIFGSFile.write(pcPic->getPicYuvDisp(),
                                          m_outputColourSpaceConvert,
                                          conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
                                          conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
                                          conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
                                          conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(),
                                          NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range);
        }
#endif
#if SHUTTER_INTERVAL_SEI_PROCESSING
        if (!m_shutterIntervalPostFileName.empty() && getShutterFilterFlag())
        {
          pcPic->xOutputPostFilteredPic(pcPic, pcListPic);

          const Window &conf = pcPic->getConformanceWindow();
          const Window  defDisp = m_respectDefDispWindow ? pcPic->getDefDisplayWindow() : Window();

          m_cTVideoIOYuvSIIPostFile.write( pcPic->getPicYuvPostRec(),
                                        m_outputColourSpaceConvert,
                                        conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
                                        conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
                                        conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
                                        conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(),
                                        NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range  );
        }
#endif

        if (!m_colourRemapSEIFileName.empty())
        {
          xOutputColourRemapPic(pcPic);
        }

        if (!m_annotatedRegionsSEIFileName.empty())
        {
          xOutputAnnotatedRegions(pcPic);
        }

        // update POC of display order
        m_iPOCLastDisplay = pcPic->getPOC();

        // erase non-referenced picture in the reference picture list after display
        if ( !pcPic->getSlice(0)->isReferenced() && pcPic->getReconMark() == true )
        {
          pcPic->setReconMark(false);

          // mark it should be extended later
          pcPic->getPicYuvRec()->setBorderExtension( false );
        }
        pcPic->setOutputMark(false);
      }
#if SHUTTER_INTERVAL_SEI_PROCESSING
      if (pcPic != NULL && (m_shutterIntervalPostFileName.empty() || !getShutterFilterFlag()))
#else
      if(pcPic != NULL)
#endif
      {
        pcPic->destroy();
        delete pcPic;
        pcPic = NULL;
      }
      iterPic++;
    }
#if SHUTTER_INTERVAL_SEI_PROCESSING
    if (!m_shutterIntervalPostFileName.empty() && getShutterFilterFlag())
    {
      iterPic = pcListPic->begin();
      while (iterPic != pcListPic->end())
      {
        pcPic = *(iterPic);
        if (pcPic != NULL)
        {
          pcPic->destroy();
          delete pcPic;
          pcPic = NULL;
        }
        iterPic++;
      }
    }
#endif
  }
  pcListPic->clear();
  m_iPOCLastDisplay = -MAX_INT;
}

/** \param nalu Input nalu to check whether its LayerId is within targetDecLayerIdSet
 */
Bool TAppDecTop::isNaluWithinTargetDecLayerIdSet( InputNALUnit* nalu )
{
  if ( m_targetDecLayerIdSet.size() == 0 ) // By default, the set is empty, meaning all LayerIds are allowed
  {
    return true;
  }
  for (std::vector<Int>::iterator it = m_targetDecLayerIdSet.begin(); it != m_targetDecLayerIdSet.end(); it++)
  {
    if ( nalu->m_nuhLayerId == (*it) )
    {
      return true;
    }
  }
  return false;
}

Void TAppDecTop::xOutputColourRemapPic(TComPic* pcPic)
{
  const TComSPS &sps=pcPic->getPicSym()->getSPS();
  SEIMessages colourRemappingInfo = getSeisByType(pcPic->getSEIs(), SEI::COLOUR_REMAPPING_INFO );
  SEIColourRemappingInfo *seiColourRemappingInfo = ( colourRemappingInfo.size() > 0 ) ? (SEIColourRemappingInfo*) *(colourRemappingInfo.begin()) : NULL;

  if (colourRemappingInfo.size() > 1)
  {
    printf ("Warning: Got multiple Colour Remapping Information SEI messages. Using first.");
  }
  if (seiColourRemappingInfo)
  {
    applyColourRemapping(*pcPic->getPicYuvRec(), *seiColourRemappingInfo, sps);

    // save the last CRI SEI received
    if (m_pcSeiColourRemappingInfoPrevious == NULL)
    {
      m_pcSeiColourRemappingInfoPrevious = new SEIColourRemappingInfo();
    }
    m_pcSeiColourRemappingInfoPrevious->copyFrom(*seiColourRemappingInfo);
  }
  else  // using the last CRI SEI received
  {
    // TODO: prevent persistence of CRI SEI across C(L)VS.
    if (m_pcSeiColourRemappingInfoPrevious != NULL)
    {
      if (m_pcSeiColourRemappingInfoPrevious->m_colourRemapPersistenceFlag == false)
      {
        printf("Warning No SEI-CRI message is present for the current picture, persistence of the CRI is not managed\n");
      }
      applyColourRemapping(*pcPic->getPicYuvRec(), *m_pcSeiColourRemappingInfoPrevious, sps);
    }
  }
}

Void TAppDecTop::xOutputAnnotatedRegions(TComPic* pcPic)
{

  // Check if any annotated region SEI has arrived
  SEIMessages annotatedRegionSEIs = getSeisByType(pcPic->getSEIs(), SEI::ANNOTATED_REGIONS);
  for(auto it=annotatedRegionSEIs.begin(); it!=annotatedRegionSEIs.end(); it++)
  {
    const SEIAnnotatedRegions &seiAnnotatedRegions = *(SEIAnnotatedRegions*)(*it);
    
    if (seiAnnotatedRegions.m_hdr.m_cancelFlag)
    {
      m_arObjects.clear();
      m_arLabels.clear();
    }
    else
    {
      if (m_arHeader.m_receivedSettingsOnce)
      {
        // validate those settings that must stay constant are constant.
        assert(m_arHeader.m_occludedObjectFlag              == seiAnnotatedRegions.m_hdr.m_occludedObjectFlag);
        assert(m_arHeader.m_partialObjectFlagPresentFlag    == seiAnnotatedRegions.m_hdr.m_partialObjectFlagPresentFlag);
        assert(m_arHeader.m_objectConfidenceInfoPresentFlag == seiAnnotatedRegions.m_hdr.m_objectConfidenceInfoPresentFlag);
        assert((!m_arHeader.m_objectConfidenceInfoPresentFlag) || m_arHeader.m_objectConfidenceLength == seiAnnotatedRegions.m_hdr.m_objectConfidenceLength);
      }
      else
      {
        m_arHeader.m_receivedSettingsOnce=true;
        m_arHeader=seiAnnotatedRegions.m_hdr; // copy the settings.
      }
      // Process label updates
      if (seiAnnotatedRegions.m_hdr.m_objectLabelPresentFlag)
      {
        for(auto srcIt=seiAnnotatedRegions.m_annotatedLabels.begin(); srcIt!=seiAnnotatedRegions.m_annotatedLabels.end(); srcIt++)
        {
          const UInt labIdx = srcIt->first;
          if (srcIt->second.labelValid)
          {
            m_arLabels[labIdx] = srcIt->second.label;
          }
          else
          {
            m_arLabels.erase(labIdx);
          }
        }
      }

      // Process object updates
      for(auto srcIt=seiAnnotatedRegions.m_annotatedRegions.begin(); srcIt!=seiAnnotatedRegions.m_annotatedRegions.end(); srcIt++)
      {
        UInt objIdx = srcIt->first;
        const SEIAnnotatedRegions::AnnotatedRegionObject &src =srcIt->second;

        if (src.objectCancelFlag)
        {
          m_arObjects.erase(objIdx);
        }
        else
        {
          auto destIt = m_arObjects.find(objIdx);

          if (destIt == m_arObjects.end())
          {
            //New object arrived, needs to be appended to the map of tracked objects
            m_arObjects[objIdx] = src;
          }
          else //Existing object, modifications to be done
          {
            SEIAnnotatedRegions::AnnotatedRegionObject &dst=destIt->second;

            if (seiAnnotatedRegions.m_hdr.m_objectLabelPresentFlag && src.objectLabelValid)
            {
              dst.objectLabelValid=true;
              dst.objLabelIdx = src.objLabelIdx;
            }
            if (src.boundingBoxValid)
            {
              dst.boundingBoxTop    = src.boundingBoxTop   ;
              dst.boundingBoxLeft   = src.boundingBoxLeft  ;
              dst.boundingBoxWidth  = src.boundingBoxWidth ;
              dst.boundingBoxHeight = src.boundingBoxHeight;
              if (seiAnnotatedRegions.m_hdr.m_partialObjectFlagPresentFlag)
              {
                dst.partialObjectFlag = src.partialObjectFlag;
              }
              if (seiAnnotatedRegions.m_hdr.m_objectConfidenceInfoPresentFlag)
              {
                dst.objectConfidence = src.objectConfidence;
              }
            }
          }
        }
      }
    }
  }

  if (!m_arObjects.empty())
  {
    FILE *fp_persist = fopen(m_annotatedRegionsSEIFileName.c_str(), "ab");
    if (fp_persist == NULL)
    {
      std::cout << "Not able to open file for writing persist SEI messages" << std::endl;
    }
    else
    {
      fprintf(fp_persist, "\n");
      fprintf(fp_persist, "Number of objects = %d\n", (Int)m_arObjects.size());
      for (auto it = m_arObjects.begin(); it != m_arObjects.end(); ++it)
      {
        fprintf(fp_persist, "Object Idx = %d\n",    it->first);
        fprintf(fp_persist, "Object Top = %d\n",    it->second.boundingBoxTop);
        fprintf(fp_persist, "Object Left = %d\n",   it->second.boundingBoxLeft);
        fprintf(fp_persist, "Object Width = %d\n",  it->second.boundingBoxWidth);
        fprintf(fp_persist, "Object Height = %d\n", it->second.boundingBoxHeight);
        if (it->second.objectLabelValid)
        {
          auto labelIt=m_arLabels.find(it->second.objLabelIdx);
          fprintf(fp_persist, "Object Label = %s\n", labelIt!=m_arLabels.end() ? (labelIt->second.c_str()) : "<UNKNOWN>");
        }
        if (m_arHeader.m_partialObjectFlagPresentFlag)
        {
          fprintf(fp_persist, "Object Partial = %d\n", it->second.partialObjectFlag?1:0);
        }
        if (m_arHeader.m_objectConfidenceInfoPresentFlag)
        {
          fprintf(fp_persist, "Object Conf = %d\n", it->second.objectConfidence);
        }
      }
      fclose(fp_persist);
    }
  }
}


// compute lut from SEI
// use at lutPoints points aligned on a power of 2 value
// SEI Lut must be in ascending values of coded Values
static std::vector<Int>
initColourRemappingInfoLut(const Int                                          bitDepth_in,     // bit-depth of the input values of the LUT
                           const Int                                          nbDecimalValues, // Position of the fixed point
                           const std::vector<SEIColourRemappingInfo::CRIlut> &lut,
                           const Int                                          maxValue, // maximum output value
                           const Int                                          lutOffset)
{
  const Int lutPoints = (1 << bitDepth_in) + 1 ;
  std::vector<Int> retLut(lutPoints);

  // missing values: need to define default values before first definition (check codedValue[0] == 0)
  Int iTargetPrev = (lut.size() && lut[0].codedValue == 0) ? lut[0].targetValue: 0;
  Int startPivot = (lut.size())? ((lut[0].codedValue == 0)? 1: 0): 1;
  Int iCodedPrev  = 0;
  // set max value with the coded bit-depth
  // + ((1 << nbDecimalValues) - 1) is for the added bits
  const Int maxValueFixedPoint = (maxValue << nbDecimalValues) + ((1 << nbDecimalValues) - 1);

  Int iValue = 0;

  for ( Int iPivot=startPivot ; iPivot < (Int)lut.size(); iPivot++ )
  {
    Int iCodedNext  = lut[iPivot].codedValue;
    Int iTargetNext = lut[iPivot].targetValue;

    // ensure correct bit depth and avoid overflow in lut address
    Int iCodedNext_bitDepth = std::min(iCodedNext, (1 << bitDepth_in));

    const Int divValue =  (iCodedNext - iCodedPrev > 0)? (iCodedNext - iCodedPrev): 1;
    const Int lutValInit = (lutOffset + iTargetPrev) << nbDecimalValues;
    const Int roundValue = divValue / 2;
    for ( ; iValue<iCodedNext_bitDepth; iValue++ )
    {
      Int value = iValue;
      Int interpol = ((((value-iCodedPrev) * (iTargetNext - iTargetPrev)) << nbDecimalValues) + roundValue) / divValue;               
      retLut[iValue]  = std::min(lutValInit + interpol , maxValueFixedPoint);
    }
    iCodedPrev  = iCodedNext;
    iTargetPrev = iTargetNext;
  }
  // fill missing values if necessary
  if(iCodedPrev < (1 << bitDepth_in)+1)
  {
    Int iCodedNext  = (1 << bitDepth_in);
    Int iTargetNext = (1 << bitDepth_in) - 1;

    const Int divValue =  (iCodedNext - iCodedPrev > 0)? (iCodedNext - iCodedPrev): 1;
    const Int lutValInit = (lutOffset + iTargetPrev) << nbDecimalValues;
    const Int roundValue = divValue / 2;

    for ( ; iValue<=iCodedNext; iValue++ )
    {
      Int value = iValue;
      Int interpol = ((((value-iCodedPrev) * (iTargetNext - iTargetPrev)) << nbDecimalValues) + roundValue) / divValue; 
      retLut[iValue]  = std::min(lutValInit + interpol , maxValueFixedPoint);
    }
  }
  return retLut;
}

static Void
initColourRemappingInfoLuts(std::vector<Int>      (&preLut)[3],
                            std::vector<Int>      (&postLut)[3],
                            SEIColourRemappingInfo &pCriSEI,
                            const Int               maxBitDepth)
{
  Int internalBitDepth = pCriSEI.m_colourRemapBitDepth;
  for ( Int c=0 ; c<3 ; c++ )
  {
    std::sort(pCriSEI.m_preLut[c].begin(), pCriSEI.m_preLut[c].end()); // ensure preLut is ordered in ascending values of codedValues   
    preLut[c] = initColourRemappingInfoLut(pCriSEI.m_colourRemapInputBitDepth, maxBitDepth - pCriSEI.m_colourRemapInputBitDepth, pCriSEI.m_preLut[c], ((1 << internalBitDepth) - 1), 0); //Fill preLut

    std::sort(pCriSEI.m_postLut[c].begin(), pCriSEI.m_postLut[c].end()); // ensure postLut is ordered in ascending values of codedValues       
    postLut[c] = initColourRemappingInfoLut(pCriSEI.m_colourRemapBitDepth, maxBitDepth - pCriSEI.m_colourRemapBitDepth, pCriSEI.m_postLut[c], (1 << internalBitDepth) - 1, 0); //Fill postLut
  }
}

// apply lut.
// Input lut values are aligned on power of 2 boundaries
static Int
applyColourRemappingInfoLut1D(Int inVal, const std::vector<Int> &lut, const Int inValPrecisionBits)
{
  const Int roundValue = (inValPrecisionBits)? 1 << (inValPrecisionBits - 1): 0;
  inVal = std::min(std::max(0, inVal), (Int)(((lut.size()-1) << inValPrecisionBits)));
  Int index  = (Int) std::min((inVal >> inValPrecisionBits), (Int)(lut.size()-2));
  Int outVal = (( inVal - (index<<inValPrecisionBits) ) * (lut[index+1] - lut[index]) + roundValue) >> inValPrecisionBits;
  outVal +=  lut[index] ;

  return outVal;
}  

static Int
applyColourRemappingInfoMatrix(const Int (&colourRemapCoeffs)[3], const Int postOffsetShift, const Int p0, const Int p1, const Int p2, const Int offset)
{
  Int YUVMat = (colourRemapCoeffs[0]* p0 + colourRemapCoeffs[1]* p1 + colourRemapCoeffs[2]* p2  + offset) >> postOffsetShift;
  return YUVMat;
}

static Void
setColourRemappingInfoMatrixOffset(Int (&matrixOffset)[3], Int offset0, Int offset1, Int offset2)
{
  matrixOffset[0] = offset0;
  matrixOffset[1] = offset1;
  matrixOffset[2] = offset2;
}

static Void
setColourRemappingInfoMatrixOffsets(      Int  (&matrixInputOffset)[3],
                                          Int  (&matrixOutputOffset)[3],
                                    const Int  bitDepth,
                                    const Bool crInputFullRangeFlag,
                                    const Int  crInputMatrixCoefficients,
                                    const Bool crFullRangeFlag,
                                    const Int  crMatrixCoefficients)
{
  // set static matrix offsets
  Int crInputOffsetLuma = (crInputFullRangeFlag)? 0:-(16 << (bitDepth-8));
  Int crOffsetLuma = (crFullRangeFlag)? 0:(16 << (bitDepth-8));
  Int crInputOffsetChroma = 0;
  Int crOffsetChroma = 0;

  switch(crInputMatrixCoefficients)
  {
    case MATRIX_COEFFICIENTS_RGB:
      crInputOffsetChroma = 0;
      if(!crInputFullRangeFlag)
      {
        fprintf(stderr, "WARNING: crInputMatrixCoefficients set to MATRIX_COEFFICIENTS_RGB and crInputFullRangeFlag not set\n");
        crInputOffsetLuma = 0;
      }
      break;
    case MATRIX_COEFFICIENTS_UNSPECIFIED:
    case MATRIX_COEFFICIENTS_BT709:
    case MATRIX_COEFFICIENTS_BT2020_NON_CONSTANT_LUMINANCE:
      crInputOffsetChroma = -(1 << (bitDepth-1));
      break;
    default:
      fprintf(stderr, "WARNING: crInputMatrixCoefficients set to undefined value: %d\n", crInputMatrixCoefficients);
  }

  switch(crMatrixCoefficients)
  {
    case MATRIX_COEFFICIENTS_RGB:
      crOffsetChroma = 0;
      if(!crFullRangeFlag)
      {
        fprintf(stderr, "WARNING: crMatrixCoefficients set to MATRIX_COEFFICIENTS_RGB and crInputFullRangeFlag not set\n");
        crOffsetLuma = 0;
      }
      break;
    case MATRIX_COEFFICIENTS_UNSPECIFIED:
    case MATRIX_COEFFICIENTS_BT709:
    case MATRIX_COEFFICIENTS_BT2020_NON_CONSTANT_LUMINANCE:
      crOffsetChroma = (1 << (bitDepth-1));
      break;
    default:
      fprintf(stderr, "WARNING: crMatrixCoefficients set to undefined value: %d\n", crMatrixCoefficients);
  }

  setColourRemappingInfoMatrixOffset(matrixInputOffset, crInputOffsetLuma, crInputOffsetChroma, crInputOffsetChroma);
  setColourRemappingInfoMatrixOffset(matrixOutputOffset, crOffsetLuma, crOffsetChroma, crOffsetChroma);
}

Void TAppDecTop::applyColourRemapping(const TComPicYuv& pic, SEIColourRemappingInfo& criSEI, const TComSPS &activeSPS)
{  
  const Int maxBitDepth = 16;

  // create colour remapped picture
  if( !criSEI.m_colourRemapCancelFlag && pic.getChromaFormat()!=CHROMA_400) // 4:0:0 not supported.
  {
    const Int          iHeight         = pic.getHeight(COMPONENT_Y);
    const Int          iWidth          = pic.getWidth(COMPONENT_Y);
    const ChromaFormat chromaFormatIDC = pic.getChromaFormat();

    TComPicYuv picYuvColourRemapped;
    picYuvColourRemapped.createWithoutCUInfo( iWidth, iHeight, chromaFormatIDC );

    const Int  iStrideIn   = pic.getStride(COMPONENT_Y);
    const Int  iCStrideIn  = pic.getStride(COMPONENT_Cb);
    const Int  iStrideOut  = picYuvColourRemapped.getStride(COMPONENT_Y);
    const Int  iCStrideOut = picYuvColourRemapped.getStride(COMPONENT_Cb);
    const Bool b444        = ( pic.getChromaFormat() == CHROMA_444 );
    const Bool b422        = ( pic.getChromaFormat() == CHROMA_422 );
    const Bool b420        = ( pic.getChromaFormat() == CHROMA_420 );

    std::vector<Int> preLut[3];
    std::vector<Int> postLut[3];
    Int matrixInputOffset[3];
    Int matrixOutputOffset[3];
    const Pel *YUVIn[MAX_NUM_COMPONENT];
    Pel *YUVOut[MAX_NUM_COMPONENT];
    YUVIn[COMPONENT_Y]  = pic.getAddr(COMPONENT_Y);
    YUVIn[COMPONENT_Cb] = pic.getAddr(COMPONENT_Cb);
    YUVIn[COMPONENT_Cr] = pic.getAddr(COMPONENT_Cr);
    YUVOut[COMPONENT_Y]  = picYuvColourRemapped.getAddr(COMPONENT_Y);
    YUVOut[COMPONENT_Cb] = picYuvColourRemapped.getAddr(COMPONENT_Cb);
    YUVOut[COMPONENT_Cr] = picYuvColourRemapped.getAddr(COMPONENT_Cr);

    const Int bitDepth = criSEI.m_colourRemapBitDepth;
    BitDepths        bitDepthsCriFile;
    bitDepthsCriFile.recon[CHANNEL_TYPE_LUMA]   = bitDepth;
    bitDepthsCriFile.recon[CHANNEL_TYPE_CHROMA] = bitDepth; // Different bitdepth is not implemented

    const Int postOffsetShift = criSEI.m_log2MatrixDenom;
    const Int matrixRound = 1 << (postOffsetShift - 1);
    const Int postLutInputPrecision = (maxBitDepth - criSEI.m_colourRemapBitDepth);

    if ( ! criSEI.m_colourRemapVideoSignalInfoPresentFlag ) // setting default
    {
      setColourRemappingInfoMatrixOffsets(matrixInputOffset, matrixOutputOffset, maxBitDepth,
          activeSPS.getVuiParameters()->getVideoFullRangeFlag(), activeSPS.getVuiParameters()->getMatrixCoefficients(),
          activeSPS.getVuiParameters()->getVideoFullRangeFlag(), activeSPS.getVuiParameters()->getMatrixCoefficients());
    }
    else
    {
      setColourRemappingInfoMatrixOffsets(matrixInputOffset, matrixOutputOffset, maxBitDepth,
          activeSPS.getVuiParameters()->getVideoFullRangeFlag(), activeSPS.getVuiParameters()->getMatrixCoefficients(),
          criSEI.m_colourRemapFullRangeFlag, criSEI.m_colourRemapMatrixCoefficients);
    }

    // add matrix rounding to output matrix offsets
    matrixOutputOffset[0] = (matrixOutputOffset[0] << postOffsetShift) + matrixRound;
    matrixOutputOffset[1] = (matrixOutputOffset[1] << postOffsetShift) + matrixRound;
    matrixOutputOffset[2] = (matrixOutputOffset[2] << postOffsetShift) + matrixRound;

    // Merge   matrixInputOffset and matrixOutputOffset to matrixOutputOffset
    matrixOutputOffset[0] += applyColourRemappingInfoMatrix(criSEI.m_colourRemapCoeffs[0], 0, matrixInputOffset[0], matrixInputOffset[1], matrixInputOffset[2], 0);
    matrixOutputOffset[1] += applyColourRemappingInfoMatrix(criSEI.m_colourRemapCoeffs[1], 0, matrixInputOffset[0], matrixInputOffset[1], matrixInputOffset[2], 0);
    matrixOutputOffset[2] += applyColourRemappingInfoMatrix(criSEI.m_colourRemapCoeffs[2], 0, matrixInputOffset[0], matrixInputOffset[1], matrixInputOffset[2], 0);

    // rescaling output: include CRI/output frame difference
    const Int scaleShiftOut_neg = abs(bitDepth - maxBitDepth);
    const Int scaleOut_round = 1 << (scaleShiftOut_neg-1);

    initColourRemappingInfoLuts(preLut, postLut, criSEI, maxBitDepth);

    assert(pic.getChromaFormat() != CHROMA_400);
    const Int hs = pic.getComponentScaleX(ComponentID(COMPONENT_Cb));
    const Int maxOutputValue = (1 << bitDepth) - 1;

    for( Int y = 0; y < iHeight; y++ )
    {
      for( Int x = 0; x < iWidth; x++ )
      {
        const Int xc = (x>>hs);
        Bool computeChroma = b444 || ((b422 || !(y&1)) && !(x&1));

        Int YUVPre_0 = applyColourRemappingInfoLut1D(YUVIn[COMPONENT_Y][x], preLut[0], 0);
        Int YUVPre_1 = applyColourRemappingInfoLut1D(YUVIn[COMPONENT_Cb][xc], preLut[1], 0);
        Int YUVPre_2 = applyColourRemappingInfoLut1D(YUVIn[COMPONENT_Cr][xc], preLut[2], 0);

        Int YUVMat_0 = applyColourRemappingInfoMatrix(criSEI.m_colourRemapCoeffs[0], postOffsetShift, YUVPre_0, YUVPre_1, YUVPre_2, matrixOutputOffset[0]);
        Int YUVLutB_0 = applyColourRemappingInfoLut1D(YUVMat_0, postLut[0], postLutInputPrecision);
        YUVOut[COMPONENT_Y][x] = std::min(maxOutputValue, (YUVLutB_0 + scaleOut_round) >> scaleShiftOut_neg);

        if( computeChroma )
        {
          Int YUVMat_1 = applyColourRemappingInfoMatrix(criSEI.m_colourRemapCoeffs[1], postOffsetShift, YUVPre_0, YUVPre_1, YUVPre_2, matrixOutputOffset[1]);
          Int YUVLutB_1 = applyColourRemappingInfoLut1D(YUVMat_1, postLut[1], postLutInputPrecision);
          YUVOut[COMPONENT_Cb][xc] = std::min(maxOutputValue, (YUVLutB_1 + scaleOut_round) >> scaleShiftOut_neg);

          Int YUVMat_2 = applyColourRemappingInfoMatrix(criSEI.m_colourRemapCoeffs[2], postOffsetShift, YUVPre_0, YUVPre_1, YUVPre_2, matrixOutputOffset[2]);
          Int YUVLutB_2 = applyColourRemappingInfoLut1D(YUVMat_2, postLut[2], postLutInputPrecision);
          YUVOut[COMPONENT_Cr][xc] = std::min(maxOutputValue, (YUVLutB_2 + scaleOut_round) >> scaleShiftOut_neg);
        }
      }

      YUVIn[COMPONENT_Y]  += iStrideIn;
      YUVOut[COMPONENT_Y] += iStrideOut;
      if( !(b420 && !(y&1)) )
      {
         YUVIn[COMPONENT_Cb]  += iCStrideIn;
         YUVIn[COMPONENT_Cr]  += iCStrideIn;
         YUVOut[COMPONENT_Cb] += iCStrideOut;
         YUVOut[COMPONENT_Cr] += iCStrideOut;
      }
    }
    //Write remapped picture in display order
    picYuvColourRemapped.dump( m_colourRemapSEIFileName, bitDepthsCriFile, true );
    picYuvColourRemapped.destroy();
  }
}

//! \}
