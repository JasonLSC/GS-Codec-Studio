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

/** \file     DecApp.cpp
    \brief    Decoder application class
*/

#include <list>
#include <numeric>
#include <vector>
#include <stdio.h>
#include <fcntl.h>

#include "DecApp.h"
#include "DecoderLib/AnnexBread.h"
#include "DecoderLib/NALread.h"
#if RExt__DECODER_DEBUG_STATISTICS
#include "CommonLib/CodingStatistics.h"
#endif
#include "CommonLib/dtrace_codingstruct.h"

//! \ingroup DecoderApp
//! \{

// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================

DecApp::DecApp()
: m_iPOCLastDisplay(-MAX_INT)
{
  for (int i = 0; i < MAX_NUM_LAYER_IDS; i++)
  {
    m_newCLVS[i] = true;
  }
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/**
 - create internal class
 - initialize internal class
 - until the end of the bitstream, call decoding function in DecApp class
 - delete allocated buffers
 - destroy internal class
 - returns the number of mismatching pictures
 */
uint32_t DecApp::decode()
{
  int      poc;
  PicList *pcListPic = nullptr;
  
#if GREEN_METADATA_SEI_ENABLED
  FeatureCounterStruct featureCounter;
  FeatureCounterStruct featureCounterOld;
  std::ifstream        bitstreamSize(m_bitstreamFileName.c_str(), std::ifstream::in | std::ifstream::binary);
  std::streampos fsize = 0;
  fsize = bitstreamSize.tellg();
  bitstreamSize.seekg( 0, std::ios::end );
  featureCounter.bytes = (int) bitstreamSize.tellg() - (int) fsize;
  bitstreamSize.close();
#endif

  std::ifstream bitstreamFile(m_bitstreamFileName.c_str(), std::ifstream::in | std::ifstream::binary);
  if (!bitstreamFile)
  {
    EXIT( "Failed to open bitstream file " << m_bitstreamFileName.c_str() << " for reading" ) ;
  }

  InputByteStream bytestream(bitstreamFile);

  if (!m_outputDecodedSEIMessagesFilename.empty() && m_outputDecodedSEIMessagesFilename!="-")
  {
    m_seiMessageFileStream.open(m_outputDecodedSEIMessagesFilename.c_str(), std::ios::out);
    if (!m_seiMessageFileStream.is_open() || !m_seiMessageFileStream.good())
    {
      EXIT( "Unable to open file "<< m_outputDecodedSEIMessagesFilename.c_str() << " for writing decoded SEI messages");
    }
  }

  if (!m_oplFilename.empty() && m_oplFilename!="-")
  {
    m_oplFileStream.open(m_oplFilename.c_str(), std::ios::out);
    if (!m_oplFileStream.is_open() || !m_oplFileStream.good())
    {
      EXIT( "Unable to open file "<< m_oplFilename.c_str() << " to write an opl-file for conformance testing (see JVET-P2008 for details)");
    }
  }

  // create & initialize internal classes
  xCreateDecLib();

  m_iPOCLastDisplay += m_iSkipFrame;      // set the last displayed POC correctly for skip forward.

  // clear contents of colour-remap-information-SEI output file
  if (!m_colourRemapSEIFileName.empty())
  {
    std::ofstream ofile(m_colourRemapSEIFileName.c_str());
    if (!ofile.good() || !ofile.is_open())
    {
      EXIT( "Unable to open file " << m_colourRemapSEIFileName.c_str() << " for writing colour-remap-information-SEI video");
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

  if (!m_objectMaskInfoSEIFileName.empty())
  {
    std::ofstream ofile(m_objectMaskInfoSEIFileName.c_str());
    if (!ofile.good() || !ofile.is_open())
    {
      fprintf(stderr, "\nUnable to open file '%s' for writing Object-Mask-Information-SEI\n", m_objectMaskInfoSEIFileName.c_str());
      exit(EXIT_FAILURE);
    }
  }

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  if (!m_packedRegionsInfoSEIFileName.empty())
  {
    std::ofstream ofile(m_packedRegionsInfoSEIFileName.c_str());
    if (!ofile.good() || !ofile.is_open())
    {
      fprintf(stderr, "\nUnable to open file '%s' for writing packed regions info SEI\n", m_packedRegionsInfoSEIFileName.c_str());
      exit(EXIT_FAILURE);
    }
  }
#endif

  // main decoder loop
  bool loopFiltered[MAX_VPS_LAYERS] = { false };

  bool bPicSkipped = false;

  bool openedPostFile = false;
  setShutterFilterFlag(!m_shutterIntervalPostFileName.empty());   // not apply shutter interval SEI processing if filename is not specified.
  m_cDecLib.setShutterFilterFlag(getShutterFilterFlag());

  bool isEosPresentInPu = false;
  bool isEosPresentInLastPu = false;

  bool outputPicturePresentInBitstream = false;
  auto setOutputPicturePresentInStream = [&]()
  {
    if( !outputPicturePresentInBitstream )
    {
      PicList::iterator iterPic = pcListPic->begin();
      while (!outputPicturePresentInBitstream && iterPic != pcListPic->end())
      {
        Picture *pcPic = *(iterPic++);
        if (pcPic->neededForOutput)
        {
          outputPicturePresentInBitstream = true;
        }
      }
    }
  };

    m_cDecLib.setHTidExternalSetFlag(m_mTidExternalSet);
    m_cDecLib.setTOlsIdxExternalFlag(m_tOlsIdxTidExternalSet);

#if GREEN_METADATA_SEI_ENABLED
    m_cDecLib.setFeatureAnalysisFramewise( m_GMFAFramewise);
    m_cDecLib.setGMFAFile(m_GMFAFile);
#endif
  
  bool gdrRecoveryPeriod[MAX_NUM_LAYER_IDS] = { false };
  bool prevPicSkipped = true;
  int lastNaluLayerId = -1;
  bool decodedSliceInAU = false;

  while (!!bitstreamFile)
  {
    InputNALUnit nalu;
    nalu.m_nalUnitType = NAL_UNIT_INVALID;

    // determine if next NAL unit will be the first one from a new picture
    bool bNewPicture = m_cDecLib.isNewPicture(&bitstreamFile, &bytestream);
    bool bNewAccessUnit = bNewPicture && decodedSliceInAU && m_cDecLib.isNewAccessUnit( bNewPicture, &bitstreamFile, &bytestream );
    if(!bNewPicture)
    {
      AnnexBStats stats = AnnexBStats();

      // find next NAL unit in stream
      byteStreamNALUnit(bytestream, nalu.getBitstream().getFifo(), stats);
      if (nalu.getBitstream().getFifo().empty())
      {
        /* this can happen if the following occur:
         *  - empty input file
         *  - two back-to-back start_code_prefixes
         *  - start_code_prefix immediately followed by EOF
         */
        msg( ERROR, "Warning: Attempt to decode an empty NAL unit\n");
      }
      else
      {
        // read NAL unit header
        read(nalu);

        // flush output for first slice of an IDR picture
        if(m_cDecLib.getFirstSliceInPicture() &&
            (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
             nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP))
        {
          if (!m_cDecLib.getMixedNaluTypesInPicFlag())
          {
            m_newCLVS[nalu.m_nuhLayerId] = true;   // An IDR picture starts a new CLVS
            xFlushOutput(pcListPic, nalu.m_nuhLayerId);
          }
          else
          {
            m_newCLVS[nalu.m_nuhLayerId] = false;
          }
        }
        else if (m_cDecLib.getFirstSliceInPicture() && nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA && isEosPresentInLastPu)
        {
          // A CRA that is immediately preceded by an EOS is a CLVSS
          m_newCLVS[nalu.m_nuhLayerId] = true;
          xFlushOutput(pcListPic, nalu.m_nuhLayerId);
        }
        else if (m_cDecLib.getFirstSliceInPicture() && nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA && !isEosPresentInLastPu)
        {
          // A CRA that is not immediately precede by an EOS is not a CLVSS
          m_newCLVS[nalu.m_nuhLayerId] = false;
        }
        else if(m_cDecLib.getFirstSliceInPicture() && !isEosPresentInLastPu)
        {
          m_newCLVS[nalu.m_nuhLayerId] = false;
        }

        // parse NAL unit syntax if within target decoding layer
        if ((m_maxTemporalLayer == TL_INFINITY || nalu.m_temporalId <= m_maxTemporalLayer)
            && xIsNaluWithinTargetDecLayerIdSet(&nalu))
        {
          if (m_targetDecLayerIdSet.size())
          {
            CHECK(std::find(m_targetDecLayerIdSet.begin(), m_targetDecLayerIdSet.end(), nalu.m_nuhLayerId) == m_targetDecLayerIdSet.end(), "bitstream shall not contain any other layers than included in the OLS with OlsIdx");
          }
          if (bPicSkipped)
          {
            if ((nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_TRAIL) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_RASL) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_RADL) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR))
            {
              if (decodedSliceInAU && m_cDecLib.isSliceNaluFirstInAU(true, nalu))
              {
                m_cDecLib.resetAccessUnitNals();
                m_cDecLib.resetAccessUnitApsNals();
                m_cDecLib.resetAccessUnitPicInfo();
              }
              bPicSkipped = false;
            }
          }

          int skipFrameCounter = m_iSkipFrame;
          m_cDecLib.decode(nalu, m_iSkipFrame, m_iPOCLastDisplay, m_targetOlsIdx);

          if ( prevPicSkipped && nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR )
          {
            gdrRecoveryPeriod[nalu.m_nuhLayerId] = true;
          }

          if ( skipFrameCounter == 1 && ( nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR  || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA ))
          {
            skipFrameCounter--;
          }

          if ( m_iSkipFrame < skipFrameCounter  &&
              ((nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_TRAIL) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_RASL) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_RADL) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA) || (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR)))
          {
            if (decodedSliceInAU && m_cDecLib.isSliceNaluFirstInAU(true, nalu))
            {
              m_cDecLib.checkSeiInPictureUnit();
              m_cDecLib.resetPictureSeiNalus();
              m_cDecLib.checkAPSInPictureUnit();
              m_cDecLib.resetPictureUnitNals();
              m_cDecLib.resetAccessUnitSeiTids();
              m_cDecLib.checkSEIInAccessUnit();
              m_cDecLib.resetAccessUnitSeiPayLoadTypes();
              m_cDecLib.resetAccessUnitNals();
              m_cDecLib.resetAccessUnitApsNals();
              m_cDecLib.resetAccessUnitPicInfo();
            }
            bPicSkipped = true;
            m_iSkipFrame++;   // skipFrame count restore, the real decrement occur at the begin of next frame
          }

          if (nalu.m_nalUnitType == NAL_UNIT_OPI)
          {
            if (!m_cDecLib.getHTidExternalSetFlag() && m_cDecLib.getOPI()->getHtidInfoPresentFlag())
            {
              m_maxTemporalLayer = m_cDecLib.getOPI()->getOpiHtidPlus1() - 1;
            }
            m_cDecLib.setHTidOpiSetFlag(m_cDecLib.getOPI()->getHtidInfoPresentFlag());
          }
          if (nalu.m_nalUnitType == NAL_UNIT_VPS)
          {
            m_cDecLib.deriveTargetOutputLayerSet( m_cDecLib.getVPS()->m_targetOlsIdx );
            m_targetDecLayerIdSet = m_cDecLib.getVPS()->m_targetLayerIdSet;
            m_targetOutputLayerIdSet = m_cDecLib.getVPS()->m_targetOutputLayerIdSet;
          }
          if (nalu.isSlice())
          {
            decodedSliceInAU = true;
          }
        }
        else
        {
          bPicSkipped = true;
          if (nalu.isSlice())
          {
            m_cDecLib.setFirstSliceInPicture(false);
          }
        }
      }

      if( nalu.isSlice() && nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_RASL)
      {
        prevPicSkipped = bPicSkipped;
      }

      // once an EOS NAL unit appears in the current PU, mark the variable isEosPresentInPu as true
      if (nalu.m_nalUnitType == NAL_UNIT_EOS)
      {
        isEosPresentInPu = true;
        m_newCLVS[nalu.m_nuhLayerId] = true;  //The presence of EOS means that the next picture is the beginning of new CLVS
        m_cDecLib.setEosPresentInPu(true);
      }
      // within the current PU, only EOS and EOB are allowed to be sent after an EOS nal unit
      if(isEosPresentInPu)
      {
        CHECK(nalu.m_nalUnitType != NAL_UNIT_EOS && nalu.m_nalUnitType != NAL_UNIT_EOB, "When an EOS NAL unit is present in a PU, it shall be the last NAL unit among all NAL units within the PU other than other EOS NAL units or an EOB NAL unit");
      }
      lastNaluLayerId = nalu.m_nuhLayerId;
    }
    else
    {
      nalu.m_nuhLayerId = lastNaluLayerId;
    }

    if (bNewPicture || !bitstreamFile || nalu.m_nalUnitType == NAL_UNIT_EOS)
    {
      if (!m_cDecLib.getFirstSliceInSequence(nalu.m_nuhLayerId) && !bPicSkipped)
      {
        if (!loopFiltered[nalu.m_nuhLayerId] || bitstreamFile)
        {
          m_cDecLib.executeLoopFilters();
          m_cDecLib.finishPicture(poc, pcListPic, INFO, m_newCLVS[nalu.m_nuhLayerId]);
        }
        loopFiltered[nalu.m_nuhLayerId] = (nalu.m_nalUnitType == NAL_UNIT_EOS);
        if (nalu.m_nalUnitType == NAL_UNIT_EOS)
        {
          m_cDecLib.setFirstSliceInSequence(true, nalu.m_nuhLayerId);
        }

        m_cDecLib.updateAssociatedIRAP();
        m_cDecLib.updatePrevGDRInSameLayer();
        m_cDecLib.updatePrevIRAPAndGDRSubpic();

        if (gdrRecoveryPeriod[nalu.m_nuhLayerId])
        {
          if (m_cDecLib.getGDRRecoveryPocReached())
          {
            gdrRecoveryPeriod[nalu.m_nuhLayerId] = false;
          }
        }
      }
      else
      {
        m_cDecLib.setFirstSliceInPicture(true);
      }
    }

    if( pcListPic )
    {
      if ( gdrRecoveryPeriod[nalu.m_nuhLayerId] ) // Suppress YUV and OPL output during GDR recovery
      {
        PicList::iterator iterPic = pcListPic->begin();
        while (iterPic != pcListPic->end())
        {
          Picture *pcPic = *(iterPic++);
          if (pcPic->layerId == nalu.m_nuhLayerId)
          {
            pcPic->neededForOutput = false;
          }
        }
      }

      BitDepths layerOutputBitDepth;

      PicList::iterator iterPicLayer = pcListPic->begin();
      for (; iterPicLayer != pcListPic->end(); ++iterPicLayer)
      {
        if ((*iterPicLayer)->layerId == nalu.m_nuhLayerId)
        {
          break;
        }
      }
      if (iterPicLayer != pcListPic->end())
      {
        BitDepths &bitDepths = (*iterPicLayer)->m_bitDepths;

        for (auto channelType: { ChannelType::LUMA, ChannelType::CHROMA })
        {
          if (m_outputBitDepth[channelType] == 0)
          {
            layerOutputBitDepth[channelType] = bitDepths[channelType];
          }
          else
          {
            layerOutputBitDepth[channelType] = m_outputBitDepth[channelType];
          }
        }
        if (m_packedYUVMode
            && (layerOutputBitDepth[ChannelType::LUMA] != 10 && layerOutputBitDepth[ChannelType::LUMA] != 12))
        {
          EXIT("Invalid output bit-depth for packed YUV output, aborting\n");
        }

        if (!m_reconFileName.empty() && !m_cVideoIOYuvReconFile[nalu.m_nuhLayerId].isOpen())
        {
          const auto  vps           = m_cDecLib.getVPS();
          std::string reconFileName = m_reconFileName;

          if (m_reconFileName.compare("/dev/null") && vps != nullptr && vps->getMaxLayers() > 1
              && xIsNaluWithinTargetOutputLayerIdSet(&nalu))
          {
            const size_t      pos         = reconFileName.find_last_of('.');
            const std::string layerString = std::string(".layer") + std::to_string(nalu.m_nuhLayerId);

            reconFileName.insert(pos, layerString);
          }

          if (vps == nullptr || vps->getMaxLayers() == 1 || xIsNaluWithinTargetOutputLayerIdSet(&nalu))
          {
            if (isY4mFileExt(reconFileName))
            {
              const auto sps        = pcListPic->front()->cs->sps;
              Fraction   frameRate  = DEFAULT_FRAME_RATE;

              const bool useSpsData = sps->getGeneralHrdParametersPresentFlag();
              if (useSpsData || (vps != nullptr && vps->getVPSGeneralHrdParamsPresentFlag()))
              {
                const GeneralHrdParams* hrd =
                  useSpsData ? sps->getGeneralHrdParameters() : vps->getGeneralHrdParameters();

                const int tLayer = m_maxTemporalLayer == TL_INFINITY
                                     ? (useSpsData ? sps->getMaxTLayers() - 1 : vps->getMaxSubLayers() - 1)
                                     : m_maxTemporalLayer;

                const OlsHrdParams& olsHrdParam =
                  (useSpsData ? sps->getOlsHrdParameters() : vps->getOlsHrdParameters(vps->m_targetOlsIdx))[tLayer];

                int elementDurationInTc = 1;
                if (olsHrdParam.getFixedPicRateWithinCvsFlag())
                {
                  elementDurationInTc = olsHrdParam.getElementDurationInTc();
                }
                else
                {
                  msg(WARNING,
                      "\nWarning: No fixed picture rate info is found in the bitstream, best guess is used.\n");
                }
                frameRate.num = hrd->getTimeScale();
                frameRate.den = hrd->getNumUnitsInTick() * elementDurationInTc;
                const int gcd = std::gcd(frameRate.num, frameRate.den);
                frameRate.num /= gcd;
                frameRate.den /= gcd;
              }
              else
              {
                msg(WARNING, "\nWarning: No frame rate info found in the bitstream, default 50 fps is used.\n");
              }
              const auto pps = pcListPic->front()->cs->pps;
              const auto sx = SPS::getWinUnitX(sps->getChromaFormatIdc());
              const auto sy = SPS::getWinUnitY(sps->getChromaFormatIdc());
              int picWidth = 0, picHeight = 0;
              if (m_upscaledOutput == 2)
              {
                auto confWindow = sps->getConformanceWindow();
                picWidth = sps->getMaxPicWidthInLumaSamples() -(confWindow.getWindowLeftOffset() + confWindow.getWindowRightOffset()) * sx;
                picHeight = sps->getMaxPicHeightInLumaSamples() - (confWindow.getWindowTopOffset() + confWindow.getWindowBottomOffset()) * sy;
              }
              else
              {
                auto confWindow = pps->getConformanceWindow();
                picWidth = pps->getPicWidthInLumaSamples() - (confWindow.getWindowLeftOffset() + confWindow.getWindowRightOffset()) * sx;
                picHeight = pps->getPicHeightInLumaSamples() - (confWindow.getWindowTopOffset() + confWindow.getWindowBottomOffset()) * sy;
              }              
              m_cVideoIOYuvReconFile[nalu.m_nuhLayerId].setOutputY4mInfo(
                picWidth, picHeight, frameRate, layerOutputBitDepth[ChannelType::LUMA], sps->getChromaFormatIdc(),
                sps->getVuiParameters()->getChromaSampleLocType());
            }
            m_cVideoIOYuvReconFile[nalu.m_nuhLayerId].open(reconFileName, true, layerOutputBitDepth,
                                                           layerOutputBitDepth, bitDepths);   // write mode
          }
        }
        // update file bitdepth shift if recon bitdepth changed between sequences
        for (auto channelType: { ChannelType::LUMA, ChannelType::CHROMA })
        {
          int reconBitdepth = (*iterPicLayer)->m_bitDepths[( ChannelType) channelType];
          int fileBitdepth  = m_cVideoIOYuvReconFile[nalu.m_nuhLayerId].getFileBitdepth(channelType);
          int bitdepthShift = m_cVideoIOYuvReconFile[nalu.m_nuhLayerId].getBitdepthShift(channelType);
          if (fileBitdepth + bitdepthShift != reconBitdepth)
          {
            m_cVideoIOYuvReconFile[nalu.m_nuhLayerId].setBitdepthShift(channelType, reconBitdepth - fileBitdepth);
          }
        }

        if (!m_SEIFGSFileName.empty() && !m_videoIOYuvSEIFGSFile[nalu.m_nuhLayerId].isOpen())
        {
          std::string SEIFGSFileName = m_SEIFGSFileName;
          if (m_SEIFGSFileName.compare("/dev/null") && m_cDecLib.getVPS() != nullptr && m_cDecLib.getVPS()->getMaxLayers() > 1 && xIsNaluWithinTargetOutputLayerIdSet(&nalu))
          {
            size_t      pos         = SEIFGSFileName.find_last_of('.');
            std::string layerString = std::string(".layer") + std::to_string(nalu.m_nuhLayerId);
            if (pos != std::string::npos)
            {
              SEIFGSFileName.insert(pos, layerString);
            }
            else
            {
              SEIFGSFileName.append(layerString);
            }
          }
          if ((m_cDecLib.getVPS() != nullptr && (m_cDecLib.getVPS()->getMaxLayers() == 1 || xIsNaluWithinTargetOutputLayerIdSet(&nalu))) || m_cDecLib.getVPS() == nullptr)
          {
            m_videoIOYuvSEIFGSFile[nalu.m_nuhLayerId].open(SEIFGSFileName, true, layerOutputBitDepth,
                                                           layerOutputBitDepth, bitDepths);   // write mode
          }
        }
        // update file bitdepth shift if recon bitdepth changed between sequences
        if (!m_SEIFGSFileName.empty())
        {
          for (const auto channelType: { ChannelType::LUMA, ChannelType::CHROMA })
          {
            int reconBitdepth = (*iterPicLayer)->m_bitDepths[( ChannelType) channelType];
            int fileBitdepth  = m_videoIOYuvSEIFGSFile[nalu.m_nuhLayerId].getFileBitdepth(channelType);
            int bitdepthShift = m_videoIOYuvSEIFGSFile[nalu.m_nuhLayerId].getBitdepthShift(channelType);
            if (fileBitdepth + bitdepthShift != reconBitdepth)
            {
              m_videoIOYuvSEIFGSFile[nalu.m_nuhLayerId].setBitdepthShift(channelType, reconBitdepth - fileBitdepth);
            }
          }
        }

        if (!m_SEICTIFileName.empty() && !m_cVideoIOYuvSEICTIFile[nalu.m_nuhLayerId].isOpen())
        {
          std::string SEICTIFileName = m_SEICTIFileName;
          if (m_SEICTIFileName.compare("/dev/null") && m_cDecLib.getVPS() != nullptr && m_cDecLib.getVPS()->getMaxLayers() > 1 && xIsNaluWithinTargetOutputLayerIdSet(&nalu))
          {
            size_t pos = SEICTIFileName.find_last_of('.');
            if (pos != std::string::npos)
            {
              SEICTIFileName.insert(pos, std::to_string(nalu.m_nuhLayerId));
            }
            else
            {
              SEICTIFileName.append(std::to_string(nalu.m_nuhLayerId));
            }
          }
          if ((m_cDecLib.getVPS() != nullptr && (m_cDecLib.getVPS()->getMaxLayers() == 1 || xIsNaluWithinTargetOutputLayerIdSet(&nalu))) || m_cDecLib.getVPS() == nullptr)
          {
            m_cVideoIOYuvSEICTIFile[nalu.m_nuhLayerId].open(SEICTIFileName, true, layerOutputBitDepth,
                                                            layerOutputBitDepth, bitDepths);   // write mode
          }
        }
      }
      if (!m_annotatedRegionsSEIFileName.empty())
      {
        xOutputAnnotatedRegions(pcListPic);
      }

      PicList::iterator iterPic = pcListPic->begin();
      Picture* pcPic = *(iterPic);
      SEIMessages       shutterIntervalInfo = getSeisByType(pcPic->SEIs, SEI::PayloadType::SHUTTER_INTERVAL_INFO);

      if (!m_shutterIntervalPostFileName.empty())
      {
        bool                    hasValidSII = true;
        SEIShutterIntervalInfo *curSIIInfo  = nullptr;
        if ((pcPic->getPictureType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
          pcPic->getPictureType() == NAL_UNIT_CODED_SLICE_IDR_N_LP) && m_newCLVS[nalu.m_nuhLayerId])
        {
          IdrSiiInfo curSII;
          curSII.m_picPoc = pcPic->getPOC();

          curSII.m_isValidSii                             = false;
          curSII.m_siiInfo.m_siiEnabled                   = false;
          curSII.m_siiInfo.m_siiNumUnitsInShutterInterval = 0;
          curSII.m_siiInfo.m_siiTimeScale = 0;
          curSII.m_siiInfo.m_siiMaxSubLayersMinus1 = 0;
          curSII.m_siiInfo.m_siiFixedSIwithinCLVS = 0;

          if (shutterIntervalInfo.size() > 0)
          {
            SEIShutterIntervalInfo *seiShutterIntervalInfo = (SEIShutterIntervalInfo*) *(shutterIntervalInfo.begin());
            curSII.m_isValidSii                            = true;

            curSII.m_siiInfo.m_siiEnabled = seiShutterIntervalInfo->m_siiEnabled;
            curSII.m_siiInfo.m_siiNumUnitsInShutterInterval = seiShutterIntervalInfo->m_siiNumUnitsInShutterInterval;
            curSII.m_siiInfo.m_siiTimeScale = seiShutterIntervalInfo->m_siiTimeScale;
            curSII.m_siiInfo.m_siiMaxSubLayersMinus1 = seiShutterIntervalInfo->m_siiMaxSubLayersMinus1;
            curSII.m_siiInfo.m_siiFixedSIwithinCLVS = seiShutterIntervalInfo->m_siiFixedSIwithinCLVS;
            curSII.m_siiInfo.m_siiSubLayerNumUnitsInSI.clear();
            for (int i = 0; i < seiShutterIntervalInfo->m_siiSubLayerNumUnitsInSI.size(); i++)
            {
              curSII.m_siiInfo.m_siiSubLayerNumUnitsInSI.push_back(seiShutterIntervalInfo->m_siiSubLayerNumUnitsInSI[i]);
            }

            uint32_t tmpInfo = (uint32_t)(m_activeSiiInfo.size() + 1);
            m_activeSiiInfo.insert(std::pair<uint32_t, IdrSiiInfo>(tmpInfo, curSII));
            curSIIInfo = seiShutterIntervalInfo;
          }
          else
          {
            curSII.m_isValidSii = false;
            hasValidSII         = false;
            uint32_t tmpInfo = (uint32_t)(m_activeSiiInfo.size() + 1);
            m_activeSiiInfo.insert(std::pair<uint32_t, IdrSiiInfo>(tmpInfo, curSII));
          }
        }
        else
        {
          if (m_activeSiiInfo.size() == 1)
          {
            curSIIInfo = &(m_activeSiiInfo.begin()->second.m_siiInfo);
          }
          else
          {
            bool isLast = true;
            for (int i = 1; i < m_activeSiiInfo.size() + 1; i++)
            {
              if (pcPic->getPOC() <= m_activeSiiInfo.at(i).m_picPoc)
              {
                if (m_activeSiiInfo[i - 1].m_isValidSii)
                {
                  curSIIInfo = &(m_activeSiiInfo.at(i - 1).m_siiInfo);
                }
                else
                {
                  hasValidSII = false;
                }
                isLast = false;
                break;
              }
            }
            if (isLast)
            {
              uint32_t tmpInfo = (uint32_t)(m_activeSiiInfo.size());
              curSIIInfo = &(m_activeSiiInfo.at(tmpInfo).m_siiInfo);
            }
          }
        }

        if (hasValidSII)
        {
          if (!curSIIInfo->m_siiFixedSIwithinCLVS)
          {
            uint32_t siiMaxSubLayersMinus1 = curSIIInfo->m_siiMaxSubLayersMinus1;
            uint32_t numUnitsLFR = curSIIInfo->m_siiSubLayerNumUnitsInSI[0];
            uint32_t numUnitsHFR = curSIIInfo->m_siiSubLayerNumUnitsInSI[siiMaxSubLayersMinus1];

            int blending_ratio = (numUnitsLFR / numUnitsHFR);
            bool checkEqualValuesOfSFR = true;
            bool checkSubLayerSI       = false;
            int i;

            //supports only the case of SFR = HFR / 2
            if (curSIIInfo->m_siiSubLayerNumUnitsInSI[siiMaxSubLayersMinus1] <
                        curSIIInfo->m_siiSubLayerNumUnitsInSI[siiMaxSubLayersMinus1 - 1])
            {
              checkSubLayerSI = true;
            }
            else
            {
              fprintf(stderr, "Warning: Shutter Interval SEI message processing is disabled due to SFR != (HFR / 2) \n");
            }
            //check shutter interval for all sublayer remains same for SFR pictures
            for (i = 1; i < siiMaxSubLayersMinus1; i++)
            {
              if (curSIIInfo->m_siiSubLayerNumUnitsInSI[0] != curSIIInfo->m_siiSubLayerNumUnitsInSI[i])
              {
                checkEqualValuesOfSFR = false;
              }
            }
            if (!checkEqualValuesOfSFR)
            {
              fprintf(stderr, "Warning: Shutter Interval SEI message processing is disabled when shutter interval is not same for SFR sublayers \n");
            }
            if (checkSubLayerSI && checkEqualValuesOfSFR)
            {
              setShutterFilterFlag(numUnitsLFR == blending_ratio * numUnitsHFR);
              setBlendingRatio(blending_ratio);
            }
            else
            {
              setShutterFilterFlag(false);
            }

            const SPS* activeSPS = pcListPic->front()->cs->sps;

            if (numUnitsLFR == blending_ratio * numUnitsHFR && activeSPS->getMaxTLayers() == 1 && activeSPS->getMaxDecPicBuffering(0) == 1)
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


      if (iterPicLayer != pcListPic->end())
      {
        if ((!m_shutterIntervalPostFileName.empty()) && (!openedPostFile) && getShutterFilterFlag())
        {
          BitDepths &bitDepths = (*iterPicLayer)->m_bitDepths;
          std::ofstream ofile(m_shutterIntervalPostFileName.c_str());
          if (!ofile.good() || !ofile.is_open())
          {
            fprintf(stderr, "\nUnable to open file '%s' for writing shutter-interval-SEI video\n", m_shutterIntervalPostFileName.c_str());
            exit(EXIT_FAILURE);
          }
          m_cTVideoIOYuvSIIPostFile.open(m_shutterIntervalPostFileName, true, layerOutputBitDepth, layerOutputBitDepth,
                                         bitDepths);   // write mode
          openedPostFile = true;
        }
      }

      // write reconstruction to file
      if( bNewPicture )
      {
        setOutputPicturePresentInStream();
        xWriteOutput( pcListPic, nalu.m_temporalId );
      }
      if (nalu.m_nalUnitType == NAL_UNIT_EOS)
      {
        if (!m_annotatedRegionsSEIFileName.empty() && bNewPicture)
        {
          xOutputAnnotatedRegions(pcListPic);
        }
        setOutputPicturePresentInStream();
        xWriteOutput( pcListPic, nalu.m_temporalId );
        m_cDecLib.setFirstSliceInPicture (false);
      }
      // write reconstruction to file -- for additional bumping as defined in C.5.2.3
      if (!bNewPicture && ((nalu.m_nalUnitType >= NAL_UNIT_CODED_SLICE_TRAIL && nalu.m_nalUnitType <= NAL_UNIT_RESERVED_IRAP_VCL_11)
        || (nalu.m_nalUnitType >= NAL_UNIT_CODED_SLICE_IDR_W_RADL && nalu.m_nalUnitType <= NAL_UNIT_CODED_SLICE_GDR)))
      {
        setOutputPicturePresentInStream();
        xWriteOutput( pcListPic, nalu.m_temporalId );
      }
    }
    if( bNewPicture )
    {
      m_cDecLib.checkSeiInPictureUnit();
      m_cDecLib.resetPictureSeiNalus();
      // reset the EOS present status for the next PU check
      isEosPresentInLastPu = isEosPresentInPu;
      isEosPresentInPu = false;
    }
    if (bNewPicture || !bitstreamFile || nalu.m_nalUnitType == NAL_UNIT_EOS)
    {
      m_cDecLib.checkAPSInPictureUnit();
      m_cDecLib.resetPictureUnitNals();
    }
    if (bNewAccessUnit || !bitstreamFile)
    {
      m_cDecLib.CheckNoOutputPriorPicFlagsInAccessUnit();
      m_cDecLib.resetAccessUnitNoOutputPriorPicFlags();
      m_cDecLib.checkLayerIdIncludedInCvss();
      m_cDecLib.checkSEIInAccessUnit();
      m_cDecLib.resetAccessUnitNestedSliSeiInfo();
      m_cDecLib.resetIsFirstAuInCvs();
      m_cDecLib.resetAccessUnitEos();
      m_cDecLib.resetAudIrapOrGdrAuFlag();
    }
    if(bNewAccessUnit)
    {
      decodedSliceInAU = false;
      m_cDecLib.checkTidLayerIdInAccessUnit();
      m_cDecLib.resetAccessUnitSeiTids();
      m_cDecLib.resetAccessUnitSeiPayLoadTypes();
      m_cDecLib.checkSeiContentInAccessUnit();
      m_cDecLib.resetAccessUnitSeiNalus();
      m_cDecLib.resetAccessUnitNals();
      m_cDecLib.resetAccessUnitApsNals();
      m_cDecLib.resetAccessUnitPicInfo();
    }
#if GREEN_METADATA_SEI_ENABLED
    if (m_GMFA && m_GMFAFramewise && bNewPicture)
    {
      FeatureCounterStruct featureCounterUpdated = m_cDecLib.getFeatureCounter();
      writeGMFAOutput(featureCounterUpdated, featureCounterOld, m_GMFAFile,false);
      featureCounterOld = m_cDecLib.getFeatureCounter();
    }
#endif
  }
  if (!m_annotatedRegionsSEIFileName.empty())
  {
    xOutputAnnotatedRegions(pcListPic);
  }
  // May need to check again one more time as in case one the bitstream has only one picture, the first check may miss it
  setOutputPicturePresentInStream();
  CHECK(!outputPicturePresentInBitstream, "It is required that there shall be at least one picture with PictureOutputFlag equal to 1 in the bitstream")
  
#if GREEN_METADATA_SEI_ENABLED
  if (m_GMFA && m_GMFAFramewise) //Last frame
  {
    FeatureCounterStruct featureCounterUpdated = m_cDecLib.getFeatureCounter();
    writeGMFAOutput(featureCounterUpdated, featureCounterOld, m_GMFAFile, false);
    featureCounterOld = m_cDecLib.getFeatureCounter();
  }
  
  if (m_GMFA)
  {
    // Summary
    FeatureCounterStruct featureCounterFinal = m_cDecLib.getFeatureCounter();
    FeatureCounterStruct dummy;
    writeGMFAOutput(featureCounterFinal, dummy, m_GMFAFile, true);
  }
#endif

  m_cDecLib.applyNnPostFilter();
  
  xFlushOutput( pcListPic );

  if (!m_shutterIntervalPostFileName.empty() && getShutterFilterFlag())
  {
    m_cTVideoIOYuvSIIPostFile.close();
  }

  // get the number of checksum errors
  uint32_t nRet = m_cDecLib.getNumberOfChecksumErrorsDetected();

  // delete buffers
  m_cDecLib.deletePicBuffer();
  // destroy internal classes
  xDestroyDecLib();

#if RExt__DECODER_DEBUG_STATISTICS
  CodingStatistics::DestroyInstance();
#endif

  destroyROM();

  return nRet;
}



void DecApp::writeLineToOutputLog(Picture * pcPic)
{
  if (m_oplFileStream.is_open() && m_oplFileStream.good())
  {
    const SPS *   sps             = pcPic->cs->sps;
    ChromaFormat  chromaFormatIdc = sps->getChromaFormatIdc();
    const Window &conf            = pcPic->getConformanceWindow();
    const int     leftOffset      = conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc);
    const int     rightOffset     = conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc);
    const int     topOffset       = conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc);
    const int     bottomOffset    = conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc);
    PictureHash   recon_digest;
    auto numChar = calcMD5WithCropping(((const Picture *) pcPic)->getRecoBuf(), recon_digest, sps->getBitDepths(),
                                       leftOffset, rightOffset, topOffset, bottomOffset);

    const int croppedWidth  = pcPic->Y().width - leftOffset - rightOffset;
    const int croppedHeight = pcPic->Y().height - topOffset - bottomOffset;

    m_oplFileStream << std::setw(3) << pcPic->layerId << ",";
    m_oplFileStream << std::setw(8) << pcPic->getPOC() << "," << std::setw(5) << croppedWidth << "," << std::setw(5)
                    << croppedHeight << "," << hashToString(recon_digest, numChar) << "\n";
  }
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

void DecApp::xCreateDecLib()
{
  initROM();

  // create decoder class
  m_cDecLib.create();

  // initialize decoder class
  m_cDecLib.init(
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
    m_cacheCfgFile
#endif
  );
  m_cDecLib.setDecodedPictureHashSEIEnabled(m_decodedPictureHashSEIEnabled);

#if JVET_AJ0151_DSC_SEI
  m_cDecLib.setKeyStoreParameters(m_keyStoreDir, m_trustStoreDir);
#endif

  if (!m_outputDecodedSEIMessagesFilename.empty())
  {
    std::ostream &os=m_seiMessageFileStream.is_open() ? m_seiMessageFileStream : std::cout;
    m_cDecLib.setDecodedSEIMessageOutputStream(&os);
  }
#if JVET_S0257_DUMP_360SEI_MESSAGE
  if (!m_outputDecoded360SEIMessagesFilename.empty())
  {
    m_cDecLib.setDecoded360SEIMessageFileName(m_outputDecoded360SEIMessagesFilename);
  }
#endif
  m_cDecLib.m_targetSubPicIdx = this->m_targetSubPicIdx;
  m_cDecLib.initScalingList();
#if GDR_LEAK_TEST
  m_cDecLib.m_gdrPocRandomAccess = this->m_gdrPocRandomAccess;
#endif // GDR_LEAK_TEST
}

void DecApp::xDestroyDecLib()
{
  if( !m_reconFileName.empty() )
  {
    for( auto & recFile : m_cVideoIOYuvReconFile )
    {
      recFile.second.close();
    }
  }
  if (!m_SEIFGSFileName.empty())
  {
    for (auto &recFile: m_videoIOYuvSEIFGSFile)
    {
      recFile.second.close();
    }
  }
  if (!m_SEICTIFileName.empty())
  {
    for (auto& recFile : m_cVideoIOYuvSEICTIFile)
    {
      recFile.second.close();
    }
  }

  // destroy decoder class
  m_cDecLib.destroy();
}


/** \param pcListPic list of pictures to be written to file
    \param tId       temporal sub-layer ID
 */
void DecApp::xWriteOutput( PicList* pcListPic, uint32_t tId )
{
  if (pcListPic->empty())
  {
    return;
  }

  PicList::iterator iterPic   = pcListPic->begin();
  int numPicsNotYetDisplayed = 0;
  int dpbFullness = 0;
  uint32_t maxNumReorderPicsHighestTid;
  uint32_t maxDecPicBufferingHighestTid;
  const VPS* referredVPS = pcListPic->front()->cs->vps;

  if( referredVPS == nullptr || referredVPS->m_numLayersInOls[referredVPS->m_targetOlsIdx] == 1 )
  {
    const SPS* activeSPS = (pcListPic->front()->cs->sps);
    const int  temporalId = (m_maxTemporalLayer == TL_INFINITY || m_maxTemporalLayer >= activeSPS->getMaxTLayers())
                              ? activeSPS->getMaxTLayers() - 1
                              : m_maxTemporalLayer;
    maxNumReorderPicsHighestTid = activeSPS->getMaxNumReorderPics( temporalId );
    maxDecPicBufferingHighestTid = activeSPS->getMaxDecPicBuffering( temporalId );
  }
  else
  {
    const int temporalId = (m_maxTemporalLayer == TL_INFINITY || m_maxTemporalLayer >= referredVPS->getMaxSubLayers())
                             ? referredVPS->getMaxSubLayers() - 1
                             : m_maxTemporalLayer;
    maxNumReorderPicsHighestTid = referredVPS->getMaxNumReorderPics( temporalId );
    maxDecPicBufferingHighestTid = referredVPS->getMaxDecPicBuffering( temporalId );
  }

  while (iterPic != pcListPic->end())
  {
    Picture* pcPic = *(iterPic);
    if(pcPic->neededForOutput && pcPic->getPOC() >= m_iPOCLastDisplay)
    {
      numPicsNotYetDisplayed++;
      dpbFullness++;
    }
    else if(pcPic->referenced)
    {
      dpbFullness++;
    }
    iterPic++;
  }

  iterPic = pcListPic->begin();

  if (numPicsNotYetDisplayed>=2)
  {
    iterPic++;
  }

  Picture* pcPic = *(iterPic);
  if( numPicsNotYetDisplayed>=2 && pcPic->fieldPic ) //Field Decoding
  {
    PicList::iterator endPic   = pcListPic->end();
    endPic--;
    iterPic   = pcListPic->begin();
    while (iterPic != endPic)
    {
      Picture* pcPicTop = *(iterPic);
      iterPic++;
      PicList::iterator iterPic2 = iterPic;
      while (iterPic2 != pcListPic->end())
      {
        if ((*iterPic2)->layerId == pcPicTop->layerId && (*iterPic2)->fieldPic && (*iterPic2)->topField != pcPicTop->topField)
        {
          break;
        }
        iterPic2++;
      }
      if (iterPic2 == pcListPic->end())
      {
        continue;
      }
      
      Picture* pcPicBottom = *(iterPic2);

      if ( pcPicTop->neededForOutput && pcPicBottom->neededForOutput &&
          (numPicsNotYetDisplayed >  maxNumReorderPicsHighestTid || dpbFullness > maxDecPicBufferingHighestTid) &&
          pcPicBottom->getPOC() >= m_iPOCLastDisplay )
      {
        // write to file
        numPicsNotYetDisplayed = numPicsNotYetDisplayed-2;
        if ( !m_reconFileName.empty() )
        {
          const Window &conf = pcPicTop->getConformanceWindow();
          const bool isTff = pcPicTop->topField;

          bool display = true;

          if (display)
          {
            m_cVideoIOYuvReconFile[pcPicTop->layerId].write(
              pcPicTop->getRecoBuf(), pcPicBottom->getRecoBuf(), m_outputColourSpaceConvert,
              false,   // TODO: m_packedYUVMode,
              conf.getWindowLeftOffset() * SPS::getWinUnitX(pcPicTop->cs->sps->getChromaFormatIdc()),
              conf.getWindowRightOffset() * SPS::getWinUnitX(pcPicTop->cs->sps->getChromaFormatIdc()),
              conf.getWindowTopOffset() * SPS::getWinUnitY(pcPicTop->cs->sps->getChromaFormatIdc()),
              conf.getWindowBottomOffset() * SPS::getWinUnitY(pcPicTop->cs->sps->getChromaFormatIdc()),
              ChromaFormat::UNDEFINED, isTff);
          }
        }
        writeLineToOutputLog(pcPicTop);
        writeLineToOutputLog(pcPicBottom);

        // update POC of display order
        m_iPOCLastDisplay = pcPicBottom->getPOC();

        // erase non-referenced picture in the reference picture list after display
        if ( ! pcPicTop->referenced && pcPicTop->reconstructed )
        {
          pcPicTop->reconstructed = false;
        }
        if ( ! pcPicBottom->referenced && pcPicBottom->reconstructed )
        {
          pcPicBottom->reconstructed = false;
        }
        pcPicTop->neededForOutput = false;
        pcPicBottom->neededForOutput = false;
      }
    }
  }
  else if( !pcPic->fieldPic ) //Frame Decoding
  {
    iterPic = pcListPic->begin();

    while (iterPic != pcListPic->end())
    {
      pcPic = *(iterPic);

      if(pcPic->neededForOutput && pcPic->getPOC() >= m_iPOCLastDisplay &&
        (numPicsNotYetDisplayed >  maxNumReorderPicsHighestTid || dpbFullness > maxDecPicBufferingHighestTid))
      {
        // write to file
        numPicsNotYetDisplayed--;
        if (!pcPic->referenced)
        {
          dpbFullness--;
        }


        if (!m_reconFileName.empty())
        {
          const Window &conf = pcPic->getConformanceWindow();
          ChromaFormat  chromaFormatIdc = pcPic->m_chromaFormatIdc;
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
          if (m_cDecLib.getPriProcess().m_enabled && m_cDecLib.getPriProcess().m_layerId == pcPic->layerId
            && m_cDecLib.getPriProcess().m_targetPicWidth > 0 && m_cDecLib.getPriProcess().m_targetPicHeight > 0)
          {
            PelStorage outPic;
            const Area a = Area( Position(0, 0), Size(m_cDecLib.getPriProcess().m_targetPicWidth, m_cDecLib.getPriProcess().m_targetPicHeight) );
            outPic.create( chromaFormatIdc, a, 0 );
            m_cDecLib.getPriProcess().reconstruct(pcListPic, pcPic, outPic, *pcPic->cs->sps);
            m_cVideoIOYuvReconFile[pcPic->layerId].write(
              outPic.get(COMPONENT_Y).width, outPic.get(COMPONENT_Y).height, outPic, m_outputColourSpaceConvert,
              m_packedYUVMode, 0, 0, 0, 0, ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range);
          }
          else if( m_upscaledOutput )
#else
          if( m_upscaledOutput )
#endif
          {
            const SPS* sps = pcPic->cs->sps;
            m_cVideoIOYuvReconFile[pcPic->layerId].writeUpscaledPicture(
              *sps, *pcPic->cs->pps, pcPic->getRecoBuf(), m_outputColourSpaceConvert, m_packedYUVMode, m_upscaledOutput,
              ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range, m_upscaleFilterForDisplay, m_upscaledOutputWidth, m_upscaledOutputHeight);
          }
          else
          {
            m_cVideoIOYuvReconFile[pcPic->layerId].write(
              pcPic->getRecoBuf().get(COMPONENT_Y).width, pcPic->getRecoBuf().get(COMPONENT_Y).height,
              pcPic->getRecoBuf(), m_outputColourSpaceConvert, m_packedYUVMode,
              conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
              conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
              conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
              conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc), ChromaFormat::UNDEFINED,
              m_clipOutputVideoToRec709Range);
            }
        }
        // Perform FGS on decoded frame and write to output FGS file
        if (!m_SEIFGSFileName.empty())
        {
          const Window& conf            = pcPic->getConformanceWindow();
          const SPS* sps                = pcPic->cs->sps;
          ChromaFormat  chromaFormatIdc    = sps->getChromaFormatIdc();
          if (m_upscaledOutput)
          {
#if JVET_AL0339_FGS_SEI_SPATIAL_RESOLUTION
            if (pcPic->filmGrainAfterUpscale())
            {
              const Window& confSps = sps->getConformanceWindow();
              m_videoIOYuvSEIFGSFile[pcPic->layerId].write(
                sps->getMaxPicWidthInLumaSamples(), sps->getMaxPicHeightInLumaSamples(),
                pcPic->getDisplayBufFGUpscaled(*sps, *pcPic->cs->pps, m_upscaledOutput, sps->getChromaFormatIdc(), m_upscaleFilterForDisplay, m_upscaledOutputWidth, m_upscaledOutputHeight),
                m_outputColourSpaceConvert, m_packedYUVMode,
                confSps.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
                confSps.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
                confSps.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
                confSps.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc), ChromaFormat::UNDEFINED,
                m_clipOutputVideoToRec709Range);
            }
            else
            {
              m_videoIOYuvSEIFGSFile[pcPic->layerId].writeUpscaledPicture(
                *sps, *pcPic->cs->pps, pcPic->getDisplayBufFG(), m_outputColourSpaceConvert, m_packedYUVMode,
                m_upscaledOutput, ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range, m_upscaleFilterForDisplay, m_upscaledOutputWidth, m_upscaledOutputHeight);
            }
#else
            m_videoIOYuvSEIFGSFile[pcPic->layerId].writeUpscaledPicture(
              *sps, *pcPic->cs->pps, pcPic->getDisplayBufFG(), m_outputColourSpaceConvert, m_packedYUVMode,
              m_upscaledOutput, ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range, m_upscaleFilterForDisplay, m_upscaledOutputWidth, m_upscaledOutputHeight);
#endif
          }
          else
          {
            m_videoIOYuvSEIFGSFile[pcPic->layerId].write(
              pcPic->getRecoBuf().get(COMPONENT_Y).width, pcPic->getRecoBuf().get(COMPONENT_Y).height,
              pcPic->getDisplayBufFG(), m_outputColourSpaceConvert, m_packedYUVMode,
              conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
              conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
              conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
              conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc), ChromaFormat::UNDEFINED,
              m_clipOutputVideoToRec709Range);
          }
        }


        if (!m_shutterIntervalPostFileName.empty() && getShutterFilterFlag())
        {
          int blendingRatio = getBlendingRatio();
          pcPic->xOutputPostFilteredPic(pcPic, pcListPic, blendingRatio);

          const Window &conf = pcPic->getConformanceWindow();
          const SPS* sps = pcPic->cs->sps;
          ChromaFormat  chromaFormatIdc = sps->getChromaFormatIdc();

          m_cTVideoIOYuvSIIPostFile.write(pcPic->getPostRecBuf().get(COMPONENT_Y).width,
                                          pcPic->getPostRecBuf().get(COMPONENT_Y).height, pcPic->getPostRecBuf(),
                                          m_outputColourSpaceConvert, m_packedYUVMode,
                                          conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
                                          conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
                                          conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
                                          conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc),
                                          ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range);
        }

        // Perform CTI on decoded frame and write to output CTI file
        if (!m_SEICTIFileName.empty())
        {
          const Window& conf = pcPic->getConformanceWindow();
          const SPS* sps = pcPic->cs->sps;
          ChromaFormat  chromaFormatIdc = sps->getChromaFormatIdc();
          if (m_upscaledOutput)
          {
            m_cVideoIOYuvSEICTIFile[pcPic->layerId].writeUpscaledPicture(
              *sps, *pcPic->cs->pps, pcPic->getDisplayBuf(), m_outputColourSpaceConvert, m_packedYUVMode,
              m_upscaledOutput, ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range, m_upscaleFilterForDisplay, m_upscaledOutputWidth, m_upscaledOutputHeight);
          }
          else
          {
            m_cVideoIOYuvSEICTIFile[pcPic->layerId].write(
              pcPic->getRecoBuf().get(COMPONENT_Y).width, pcPic->getRecoBuf().get(COMPONENT_Y).height,
              pcPic->getDisplayBuf(), m_outputColourSpaceConvert, m_packedYUVMode,
              conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
              conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
              conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
              conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc), ChromaFormat::UNDEFINED,
              m_clipOutputVideoToRec709Range);
          }
        }
        writeLineToOutputLog(pcPic);

        if (!m_objectMaskInfoSEIFileName.empty())
        {
          xOutputObjectMaskInfos(pcPic);
        }
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
        if (!m_packedRegionsInfoSEIFileName.empty())
        {
          xOutputPackedRegionsInfo(pcPic);
        }
#endif
        // update POC of display order
        m_iPOCLastDisplay = pcPic->getPOC();

        // erase non-referenced picture in the reference picture list after display
        if (!pcPic->referenced && pcPic->reconstructed)
        {
          pcPic->reconstructed = false;
        }
        pcPic->neededForOutput = false;
      }

      iterPic++;
    }
  }
}

/** \param pcListPic list of pictures to be written to file
 */
void DecApp::xFlushOutput( PicList* pcListPic, const int layerId )
{
  if(!pcListPic || pcListPic->empty())
  {
    return;
  }
  PicList::iterator iterPic   = pcListPic->begin();

  iterPic   = pcListPic->begin();
  Picture* pcPic = *(iterPic);

  if (pcPic->fieldPic ) //Field Decoding
  {
    PicList::iterator endPic = pcListPic->end();
    while (iterPic != endPic)
    {
      Picture *pcPicTop = *iterPic;
      iterPic++;

      if (pcPicTop == nullptr || (pcPicTop->layerId != layerId && layerId != NOT_VALID))
      {
        continue;
      }

      PicList::iterator iterPic2 = iterPic;
      while (iterPic2 != endPic)
      {
        if ((*iterPic2) != nullptr && (*iterPic2)->layerId == pcPicTop->layerId && (*iterPic2)->fieldPic && (*iterPic2)->topField != pcPicTop->topField)
        {
          break;
        }
        iterPic2++;
      }
      Picture *pcPicBottom = iterPic2 == endPic ? nullptr : *iterPic2;

      if (pcPicBottom != nullptr && pcPicTop->neededForOutput && pcPicBottom->neededForOutput)
      {
          // write to file
          if ( !m_reconFileName.empty() )
          {
            const Window &conf = pcPicTop->getConformanceWindow();
            const bool    isTff   = pcPicTop->topField;

            m_cVideoIOYuvReconFile[pcPicTop->layerId].write(
              pcPicTop->getRecoBuf(), pcPicBottom->getRecoBuf(), m_outputColourSpaceConvert,
              false,   // TODO: m_packedYUVMode,
              conf.getWindowLeftOffset() * SPS::getWinUnitX(pcPicTop->cs->sps->getChromaFormatIdc()),
              conf.getWindowRightOffset() * SPS::getWinUnitX(pcPicTop->cs->sps->getChromaFormatIdc()),
              conf.getWindowTopOffset() * SPS::getWinUnitY(pcPicTop->cs->sps->getChromaFormatIdc()),
              conf.getWindowBottomOffset() * SPS::getWinUnitY(pcPicTop->cs->sps->getChromaFormatIdc()),
              ChromaFormat::UNDEFINED, isTff);
          }
          writeLineToOutputLog(pcPicTop);
          writeLineToOutputLog(pcPicBottom);
        // update POC of display order
        m_iPOCLastDisplay = pcPicBottom->getPOC();

        // erase non-referenced picture in the reference picture list after display
        if( ! pcPicTop->referenced && pcPicTop->reconstructed )
        {
          pcPicTop->reconstructed = false;
        }
        if( ! pcPicBottom->referenced && pcPicBottom->reconstructed )
        {
          pcPicBottom->reconstructed = false;
        }
        pcPicTop->neededForOutput = false;
        pcPicBottom->neededForOutput = false;

        pcPicTop->destroy();
        delete pcPicTop;
        pcPicBottom->destroy();
        delete pcPicBottom;
        iterPic--;
        *iterPic = nullptr;
        iterPic++;
        *iterPic2 = nullptr;
      }
      else
      {
        pcPicTop->destroy();
        delete pcPicTop;
        iterPic--;
        *iterPic = nullptr;
        iterPic++;
      }
    }
  }
  else //Frame decoding
  {
    while (iterPic != pcListPic->end())
    {
      pcPic = *(iterPic);

      if( pcPic->layerId != layerId && layerId != NOT_VALID )
      {
        iterPic++;
        continue;
      }

      if (pcPic->neededForOutput)
      {
          // write to file
          if (!m_reconFileName.empty())
          {
            const Window &conf = pcPic->getConformanceWindow();
            ChromaFormat  chromaFormatIdc = pcPic->m_chromaFormatIdc;
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
            if (m_cDecLib.getPriProcess().m_enabled && m_cDecLib.getPriProcess().m_layerId == pcPic->layerId
              && m_cDecLib.getPriProcess().m_targetPicWidth > 0 && m_cDecLib.getPriProcess().m_targetPicHeight > 0)
            {
              PelStorage outPic;
              const Area a = Area( Position(0, 0), Size(m_cDecLib.getPriProcess().m_targetPicWidth, m_cDecLib.getPriProcess().m_targetPicHeight) );
              outPic.create( chromaFormatIdc, a, 0 );

              m_cDecLib.getPriProcess().reconstruct(pcListPic, pcPic, outPic, *pcPic->cs->sps);
              m_cVideoIOYuvReconFile[pcPic->layerId].write(
                outPic.get(COMPONENT_Y).width, outPic.get(COMPONENT_Y).height, outPic, m_outputColourSpaceConvert,
                m_packedYUVMode, 0, 0, 0, 0, ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range);
            }
            else if( m_upscaledOutput )
#else
            if( m_upscaledOutput )
#endif
            {
              const SPS* sps = pcPic->cs->sps;
              m_cVideoIOYuvReconFile[pcPic->layerId].writeUpscaledPicture(
                *sps, *pcPic->cs->pps, pcPic->getRecoBuf(), m_outputColourSpaceConvert, m_packedYUVMode,
                m_upscaledOutput, ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range, m_upscaleFilterForDisplay, m_upscaledOutputWidth, m_upscaledOutputHeight);
            }
            else
            {
              m_cVideoIOYuvReconFile[pcPic->layerId].write(
                pcPic->getRecoBuf().get(COMPONENT_Y).width, pcPic->getRecoBuf().get(COMPONENT_Y).height,
                pcPic->getRecoBuf(), m_outputColourSpaceConvert, m_packedYUVMode,
                conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
                conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
                conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
                conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc), ChromaFormat::UNDEFINED,
                m_clipOutputVideoToRec709Range);
              }
          }
          // Perform FGS on decoded frame and write to output FGS file
          if (!m_SEIFGSFileName.empty())
          {
            const Window& conf            = pcPic->getConformanceWindow();
            const SPS*    sps             = pcPic->cs->sps;
            ChromaFormat  chromaFormatIdc = sps->getChromaFormatIdc();
            if (m_upscaledOutput)
            {
#if JVET_AL0339_FGS_SEI_SPATIAL_RESOLUTION
              if (pcPic->filmGrainAfterUpscale())
              {
                const Window& confSps = sps->getConformanceWindow();
                m_videoIOYuvSEIFGSFile[pcPic->layerId].write(
                  sps->getMaxPicWidthInLumaSamples(), sps->getMaxPicHeightInLumaSamples(),
                  pcPic->getDisplayBufFGUpscaled(*sps, *pcPic->cs->pps, m_upscaledOutput, sps->getChromaFormatIdc(), m_upscaleFilterForDisplay, m_upscaledOutputWidth, m_upscaledOutputHeight),
                  m_outputColourSpaceConvert, m_packedYUVMode,
                  confSps.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
                  confSps.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
                  confSps.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
                  confSps.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc), ChromaFormat::UNDEFINED,
                  m_clipOutputVideoToRec709Range);
              }
              else
              {
                m_videoIOYuvSEIFGSFile[pcPic->layerId].writeUpscaledPicture(
                  *sps, *pcPic->cs->pps, pcPic->getDisplayBufFG(), m_outputColourSpaceConvert, m_packedYUVMode,
                  m_upscaledOutput, ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range, m_upscaleFilterForDisplay, m_upscaledOutputWidth, m_upscaledOutputHeight);
              }
#else
              m_videoIOYuvSEIFGSFile[pcPic->layerId].writeUpscaledPicture(
                *sps, *pcPic->cs->pps, pcPic->getDisplayBufFG(), m_outputColourSpaceConvert, m_packedYUVMode,
                m_upscaledOutput, ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range, m_upscaleFilterForDisplay, m_upscaledOutputWidth, m_upscaledOutputHeight);
#endif
            }
            else
            {
              m_videoIOYuvSEIFGSFile[pcPic->layerId].write(
                pcPic->getRecoBuf().get(COMPONENT_Y).width, pcPic->getRecoBuf().get(COMPONENT_Y).height,
                pcPic->getDisplayBufFG(), m_outputColourSpaceConvert, m_packedYUVMode,
                conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
                conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
                conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
                conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc), ChromaFormat::UNDEFINED,
                m_clipOutputVideoToRec709Range);
            }
          }

          if (!m_shutterIntervalPostFileName.empty() && getShutterFilterFlag())
          {
            int blendingRatio = getBlendingRatio();
            pcPic->xOutputPostFilteredPic(pcPic, pcListPic, blendingRatio);

            const Window &conf = pcPic->getConformanceWindow();
            const SPS* sps = pcPic->cs->sps;
            ChromaFormat  chromaFormatIdc = sps->getChromaFormatIdc();

            m_cTVideoIOYuvSIIPostFile.write(pcPic->getPostRecBuf().get(COMPONENT_Y).width,
                                            pcPic->getPostRecBuf().get(COMPONENT_Y).height, pcPic->getPostRecBuf(),
                                            m_outputColourSpaceConvert, m_packedYUVMode,
                                            conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
                                            conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
                                            conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
                                            conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc),
                                            ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range);
          }

          // Perform CTI on decoded frame and write to output CTI file
          if (!m_SEICTIFileName.empty())
          {
            const Window& conf = pcPic->getConformanceWindow();
            const SPS* sps = pcPic->cs->sps;
            ChromaFormat  chromaFormatIdc = sps->getChromaFormatIdc();
            if (m_upscaledOutput)
            {
              m_cVideoIOYuvSEICTIFile[pcPic->layerId].writeUpscaledPicture(
                *sps, *pcPic->cs->pps, pcPic->getDisplayBuf(), m_outputColourSpaceConvert, m_packedYUVMode,
                m_upscaledOutput, ChromaFormat::UNDEFINED, m_clipOutputVideoToRec709Range, m_upscaleFilterForDisplay, m_upscaledOutputWidth, m_upscaledOutputHeight);
            }
            else
            {
              m_cVideoIOYuvSEICTIFile[pcPic->layerId].write(
                pcPic->getRecoBuf().get(COMPONENT_Y).width, pcPic->getRecoBuf().get(COMPONENT_Y).height,
                pcPic->getDisplayBuf(), m_outputColourSpaceConvert, m_packedYUVMode,
                conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
                conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
                conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
                conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc), ChromaFormat::UNDEFINED,
                m_clipOutputVideoToRec709Range);
            }
          }
          writeLineToOutputLog(pcPic);
          if (!m_objectMaskInfoSEIFileName.empty())
          {
            xOutputObjectMaskInfos(pcPic);
          }
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
          if (!m_packedRegionsInfoSEIFileName.empty())
          {
            xOutputPackedRegionsInfo(pcPic);
          }
#endif
        // update POC of display order
        m_iPOCLastDisplay = pcPic->getPOC();

        // erase non-referenced picture in the reference picture list after display
        if (!pcPic->referenced && pcPic->reconstructed)
        {
          pcPic->reconstructed = false;
        }
        pcPic->neededForOutput = false;
      }
      if (pcPic != nullptr && (m_shutterIntervalPostFileName.empty() || !getShutterFilterFlag()))
      {
        pcPic->destroy();
        delete pcPic;
        pcPic    = nullptr;
        *iterPic = nullptr;
      }
      iterPic++;
    }
  }

  if( layerId != NOT_VALID )
  {
    pcListPic->remove_if([](Picture* p) { return p == nullptr; });
  }
  else
  {
    pcListPic->clear();
  }
  m_iPOCLastDisplay = -MAX_INT;
}

/** \param pcListPic list of pictures to be written to file
 */
void DecApp::xOutputAnnotatedRegions(PicList* pcListPic)
{
  if(!pcListPic || pcListPic->empty())
  {
    return;
  }
  PicList::iterator iterPic   = pcListPic->begin();

  while (iterPic != pcListPic->end())
  {
    Picture* pcPic = *(iterPic);
    if (pcPic->neededForOutput)
    {
      // Check if any annotated region SEI has arrived
      SEIMessages annotatedRegionSEIs = getSeisByType(pcPic->SEIs, SEI::PayloadType::ANNOTATED_REGIONS);
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
              const uint32_t labIdx = srcIt->first;
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
            uint32_t objIdx = srcIt->first;
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
        FILE *fpPersist = fopen(m_annotatedRegionsSEIFileName.c_str(), "ab");
        if (fpPersist == nullptr)
        {
          std::cout << "Not able to open file for writing persist SEI messages" << std::endl;
        }
        else
        {
          fprintf(fpPersist, "\n");
          fprintf(fpPersist, "Number of objects = %d\n", (int)m_arObjects.size());
          for (auto it = m_arObjects.begin(); it != m_arObjects.end(); ++it)
          {
            fprintf(fpPersist, "Object Idx = %d\n",    it->first);
            fprintf(fpPersist, "Object Top = %d\n",    it->second.boundingBoxTop);
            fprintf(fpPersist, "Object Left = %d\n",   it->second.boundingBoxLeft);
            fprintf(fpPersist, "Object Width = %d\n",  it->second.boundingBoxWidth);
            fprintf(fpPersist, "Object Height = %d\n", it->second.boundingBoxHeight);
            if (it->second.objectLabelValid)
            {
              auto labelIt=m_arLabels.find(it->second.objLabelIdx);
              fprintf(fpPersist, "Object Label = %s\n", labelIt!=m_arLabels.end() ? (labelIt->second.c_str()) : "<UNKNOWN>");
            }
            if (m_arHeader.m_partialObjectFlagPresentFlag)
            {
              fprintf(fpPersist, "Object Partial = %d\n", it->second.partialObjectFlag?1:0);
            }
            if (m_arHeader.m_objectConfidenceInfoPresentFlag)
            {
              fprintf(fpPersist, "Object Conf = %d\n", it->second.objectConfidence);
            }
          }
          fclose(fpPersist);
        }
      }
    }
   iterPic++;
  }
}

void DecApp::xOutputObjectMaskInfos(Picture* pcPic)
{
  if (pcPic->getPictureType() == NAL_UNIT_CODED_SLICE_CRA || pcPic->getPictureType() == NAL_UNIT_CODED_SLICE_IDR_N_LP)
  {
    m_omiMasks.clear();
    m_omiHeader.m_receivedSettingsOnce = false;
  }
  SEIMessages objectMaskInfoSEIs = getSeisByType(pcPic->SEIs, SEI::PayloadType::OBJECT_MASK_INFO);
  for (auto it = objectMaskInfoSEIs.begin(); it != objectMaskInfoSEIs.end(); it++)
  {
    const SEIObjectMaskInfos& seiObjectMaskInfo = *(SEIObjectMaskInfos*) (*it);
    if (m_omiMasks.empty())
    {
      CHECK(seiObjectMaskInfo.m_hdr.m_cancelFlag, "OMI SEI message cannot be cancel from empty.");
    }
    if (seiObjectMaskInfo.m_hdr.m_cancelFlag)
    {
      m_omiMasks.clear();
    }
    else
    {
      if (m_omiHeader.m_receivedSettingsOnce)
      {
        CHECK(m_omiHeader.m_numAuxPicLayerMinus1 != seiObjectMaskInfo.m_hdr.m_numAuxPicLayerMinus1, "The value of omi_num_aux_pic_layer_minus1 should be consistent within the CLVS.")
        CHECK(m_omiHeader.m_maskIdLengthMinus1 != seiObjectMaskInfo.m_hdr.m_maskIdLengthMinus1, "The value of omi_mask_id_length_minus1 should be consistent within the CLVS.")
        CHECK(m_omiHeader.m_maskSampleValueLengthMinus8 != seiObjectMaskInfo.m_hdr.m_maskSampleValueLengthMinus8,"The value of omi_mask_sample_value_length_minus8 should be consistent within the CLVS.")
        CHECK(m_omiHeader.m_maskConfidenceInfoPresentFlag != seiObjectMaskInfo.m_hdr.m_maskConfidenceInfoPresentFlag,"Confidence info present flag should be consistent within the CLVS.");
        if (m_omiHeader.m_maskConfidenceInfoPresentFlag)
        {
          CHECK(m_omiHeader.m_maskConfidenceLengthMinus1 != seiObjectMaskInfo.m_hdr.m_maskConfidenceLengthMinus1, "Confidence length should be consistent within the CLVS.");
        }
        CHECK(m_omiHeader.m_maskDepthInfoPresentFlag != seiObjectMaskInfo.m_hdr.m_maskDepthInfoPresentFlag,"Depth info present flag should be consistent within the CLVS.");
        if (m_omiHeader.m_maskDepthInfoPresentFlag)
        {
          CHECK(m_omiHeader.m_maskDepthLengthMinus1 != seiObjectMaskInfo.m_hdr.m_maskDepthLengthMinus1, "Depth length should be consistent within the CLVS.");
        }
      }
      else
      {
        m_omiHeader                        = seiObjectMaskInfo.m_hdr;  
        m_omiHeader.m_receivedSettingsOnce = true;
        m_omiMasks.resize(m_omiHeader.m_numAuxPicLayerMinus1 + 1);
      }
      m_omiHeader.m_persistenceFlag = seiObjectMaskInfo.m_hdr.m_persistenceFlag;
      uint32_t objMaskInfoCnt = 0;
      for (uint32_t i = 0; i <= m_omiHeader.m_numAuxPicLayerMinus1; i++)
      {
        if (seiObjectMaskInfo.m_maskPicUpdateFlag[i])
        {
          if (m_omiMasks[i].empty())
          {
            CHECK(!seiObjectMaskInfo.m_numMaskInPic[i], "The value of omi_num_mask_in_pic should not be equal to 0 at the first update.");
          }
          m_omiMasks[i].resize(seiObjectMaskInfo.m_numMaskInPic[i]);
          for (uint32_t j = 0; j < seiObjectMaskInfo.m_numMaskInPic[i]; j++)
          {
            m_omiMasks[i][j] = (std::pair<uint32_t, SEIObjectMaskInfos::ObjectMaskInfo>(seiObjectMaskInfo.m_objectMaskInfos[objMaskInfoCnt].maskId + (1 << (seiObjectMaskInfo.m_hdr.m_maskIdLengthMinus1 + 1)) * i, seiObjectMaskInfo.m_objectMaskInfos[objMaskInfoCnt]));
            ++objMaskInfoCnt;
          }
        }
      }
      if (!m_omiMasks.empty())
      {
        std::set<uint32_t> MaskIdSet;
        for (auto masks : m_omiMasks)
        {
          for (auto mask : masks)
          {
            if (MaskIdSet.find(mask.first) == MaskIdSet.end())
            {
              MaskIdSet.insert(mask.first);
            }
            else
            {
              CHECK(true, "MaskId is a globle id, which should be unique.");
            }
          }
        }
      }
    }
  }
  if ((!objectMaskInfoSEIs.empty() && !m_omiMasks.empty()) || (objectMaskInfoSEIs.empty() && m_omiHeader.m_persistenceFlag))
  {
    FILE* fpPersist = fopen(m_objectMaskInfoSEIFileName.c_str(), "ab");
    if (fpPersist == nullptr)
    {
      std::cout << "Not able to open file for writing persist SEI messages" << std::endl;
    }
    else
    {
      fprintf(fpPersist, "======== POC %d ========\n", (int)pcPic->getPOC());
      // header
      fprintf(fpPersist, "OMI Cancel Flag = %d\n", m_omiHeader.m_cancelFlag);
      if (!m_omiHeader.m_cancelFlag)
      {
        fprintf(fpPersist, "OMI Persistence Flag = %d\n", m_omiHeader.m_persistenceFlag);
        fprintf(fpPersist, "OMI AuxPicLayer Num = %d\n", m_omiHeader.m_numAuxPicLayerMinus1 + 1);
        fprintf(fpPersist, "OMI MaskId Length = %d\n", m_omiHeader.m_maskIdLengthMinus1 + 1);
        fprintf(fpPersist, "OMI MaskSampleValue Length = %d\n", m_omiHeader.m_maskSampleValueLengthMinus8 + 8);
        fprintf(fpPersist, "OMI MaskConf Present = %d\n", m_omiHeader.m_maskConfidenceInfoPresentFlag);
        if (m_omiHeader.m_maskConfidenceInfoPresentFlag)
        {
          fprintf(fpPersist, "OMI MaskConf Length = %d\n", m_omiHeader.m_maskConfidenceLengthMinus1 + 1);
        }
        fprintf(fpPersist, "OMI MaskDepth Present = %d\n", m_omiHeader.m_maskDepthInfoPresentFlag);
        if (m_omiHeader.m_maskDepthInfoPresentFlag)
        {
          fprintf(fpPersist, "OMI MaskDepth Length = %d\n", m_omiHeader.m_maskDepthLengthMinus1 + 1);
        }
        fprintf(fpPersist, "OMI MaskLabel Present = %d\n", m_omiHeader.m_maskLabelInfoPresentFlag);
        if (m_omiHeader.m_maskLabelInfoPresentFlag)
        {
          fprintf(fpPersist, "OMI MaskLabelLang Present = %d\n", m_omiHeader.m_maskLabelLanguagePresentFlag);
          if (m_omiHeader.m_maskLabelLanguagePresentFlag)
          {
            fprintf(fpPersist, "OMI MaskLabelLang = %s\n", m_omiHeader.m_maskLabelLanguage.c_str());
          }
        }
        fprintf(fpPersist, "\n");
        // infos
        for (int layerIdx = 0; layerIdx < m_omiMasks.size(); layerIdx++)
        {
          fprintf(fpPersist, "[Auxiliary Layer-%d]\n", layerIdx);
          fprintf(fpPersist, "MaskNumInPic[%d]: %d\n\n", layerIdx, (int)m_omiMasks[layerIdx].size());

          for (int maskIdx = 0; maskIdx < m_omiMasks[layerIdx].size(); maskIdx++)
          {
            fprintf(fpPersist, "MaskId[%d][%d]: %d\n", layerIdx, maskIdx, (m_omiMasks[layerIdx][maskIdx].second.maskId + (1 << (m_omiHeader.m_maskIdLengthMinus1 + 1)) * layerIdx));
            fprintf(fpPersist, "MaskIdNewObjectFlag[%d][%d]: %d\n", layerIdx, maskIdx, m_omiMasks[layerIdx][maskIdx].second.maskNew);
            fprintf(fpPersist, "AuxSampleValue[%d][%d]: %d\n", layerIdx, maskIdx, m_omiMasks[layerIdx][maskIdx].second.auxSampleValue);
            fprintf(fpPersist, "MaskBBoxPresentFlag[%d][%d]: %d\n", layerIdx, maskIdx, m_omiMasks[layerIdx][maskIdx].second.maskBoundingBoxPresentFlag);
            if (m_omiMasks[layerIdx][maskIdx].second.maskBoundingBoxPresentFlag)
            {
              fprintf(fpPersist, "MaskTop[%d][%d]: %d\n", layerIdx, maskIdx, m_omiMasks[layerIdx][maskIdx].second.maskTop);
              fprintf(fpPersist, "MaskLeft[%d][%d]: %d\n", layerIdx, maskIdx, m_omiMasks[layerIdx][maskIdx].second.maskLeft);
              fprintf(fpPersist, "MaskWidth[%d][%d]: %d\n", layerIdx, maskIdx, m_omiMasks[layerIdx][maskIdx].second.maskWidth);
              fprintf(fpPersist, "MaskHeight[%d][%d]: %d\n", layerIdx, maskIdx, m_omiMasks[layerIdx][maskIdx].second.maskHeight);
            }
            if (m_omiHeader.m_maskConfidenceInfoPresentFlag)
            {
              fprintf(fpPersist, "MaskConf[%d][%d]: %d\n", layerIdx, maskIdx, m_omiMasks[layerIdx][maskIdx].second.maskConfidence);
            }
            if (m_omiHeader.m_maskDepthInfoPresentFlag)
            {
              fprintf(fpPersist, "MaskDepth[%d][%d]: %d\n", layerIdx, maskIdx, m_omiMasks[layerIdx][maskIdx].second.maskDepth);
            }
            if (m_omiHeader.m_maskLabelInfoPresentFlag)
            {
              fprintf(fpPersist, "MaskLabel[%d][%d]: %s\n", layerIdx, maskIdx, m_omiMasks[layerIdx][maskIdx].second.maskLabel.c_str());
            }
            fprintf(fpPersist, "\n");
          }
        }
      }
      fclose(fpPersist);
    }
  }
}

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
void DecApp::xOutputPackedRegionsInfo(Picture* pcPic)
{
  SEIMessages seis = getSeisByType(pcPic->SEIs, SEI::PayloadType::PACKED_REGIONS_INFO);
  if (!seis.empty())
  {
    const SEIPackedRegionsInfo& sei = *((SEIPackedRegionsInfo*)seis.front());
    FILE* fp = fopen(m_packedRegionsInfoSEIFileName.c_str(), "a");
    if (fp == nullptr)
    {
      std::cout << "Not able to open file for writing packed regions info SEI messages" << std::endl;
    }
    else
    {
      fprintf(fp, "SEIPRICancelFlag : %d\n", sei.m_cancelFlag);
      fprintf(fp, "SEIPRIPersistenceFlag : %d\n", sei.m_persistenceFlag);
      fprintf(fp, "SEIPRINumRegionsMinus1 : %d\n", sei.m_numRegionsMinus1);
      fprintf(fp, "SEIPRIMultilayerFlag : %d\n", sei.m_multilayerFlag);
      fprintf(fp, "SEIPRIUseMaxDimensionsFlag : %d\n", sei.m_useMaxDimensionsFlag);
      fprintf(fp, "SEIPRILog2UnitSize : %d\n", sei.m_log2UnitSize);
      fprintf(fp, "SEIPRIRegionSizeLenMinus1 : %d\n", sei.m_regionSizeLenMinus1);
      fprintf(fp, "SEIPRIRegionIdPresentFlag : %d\n", sei.m_regionIdPresentFlag);
      fprintf(fp, "SEIPRITargetPicParamsPresentFlag : %d\n", sei.m_targetPicParamsPresentFlag);
      if (sei.m_targetPicParamsPresentFlag)
      {
        fprintf(fp, "SEIPRITargetPicWidthMinus1 : %d\n", sei.m_targetPicWidthMinus1);
        fprintf(fp, "SEIPRITargetPicHeightMinus1 : %d\n", sei.m_targetPicHeightMinus1);
      }
      fprintf(fp, "SEIPRINumResamplingRatiosMinus1 : %d\n", sei.m_numResamplingRatiosMinus1);
      xOutputPackedRegionsInfoVector(fp, "SEIPRIResamplingWidthNumMinus1 :", sei.m_resamplingWidthNumMinus1);
      xOutputPackedRegionsInfoVector(fp, "SEIPRIResamplingWidthDenomMinus1 :", sei.m_resamplingWidthDenomMinus1);
      xOutputPackedRegionsInfoVector(fp, "SEIPRIResamplingHeightNumMinus1 :", sei.m_resamplingHeightNumMinus1);
      xOutputPackedRegionsInfoVector(fp, "SEIPRIResamplingHeightDenomMinus1 :", sei.m_resamplingHeightDenomMinus1);
      xOutputPackedRegionsInfoVector(fp, "SEIPRIRegionId :", sei.m_regionId);
      if (sei.m_multilayerFlag)
      {
        xOutputPackedRegionsInfoVector(fp, "SEIPRIRegionLayerId :", sei.m_regionLayerId);
        xOutputPackedRegionsInfoVector(fp, "SEIPRIRegionIsALayerFlag :", sei.m_regionIsALayerFlag);
      }
      xOutputPackedRegionsInfoVector(fp, "SEIPRIRegionTopLeftInUnitsX :", sei.m_regionTopLeftInUnitsX);
      xOutputPackedRegionsInfoVector(fp, "SEIPRIRegionTopLeftInUnitsY :", sei.m_regionTopLeftInUnitsY);
      xOutputPackedRegionsInfoVector(fp, "SEIPRIRegionWidthInUnitsMinus1 :", sei.m_regionWidthInUnitsMinus1);
      xOutputPackedRegionsInfoVector(fp, "SEIPRIRegionHeightInUnitsMinus1 :", sei.m_regionHeightInUnitsMinus1);
      xOutputPackedRegionsInfoVector(fp, "SEIPRIResamplingRatioIdx :", sei.m_resamplingRatioIdx);
      if (sei.m_targetPicParamsPresentFlag)
      {
#if JVET_AL0324_AL0070_PRI_SEI
        xOutputPackedRegionsInfoVector(fp, "SEIPRITargetRegionTopLeftInUnitsX :", sei.m_targetRegionTopLeftInUnitsX);
        xOutputPackedRegionsInfoVector(fp, "SEIPRITargetRegionTopLeftInUnitsY :", sei.m_targetRegionTopLeftInUnitsY);
#else
        xOutputPackedRegionsInfoVector(fp, "SEIPRITargetRegionTopLeftX :", sei.m_targetRegionTopLeftX);
        xOutputPackedRegionsInfoVector(fp, "SEIPRITargetRegionTopLeftY :", sei.m_targetRegionTopLeftY);
#endif
      }
      fclose(fp);
    }
  }
}
#endif

/** \param nalu Input nalu to check whether its LayerId is within targetDecLayerIdSet
 */
bool DecApp::xIsNaluWithinTargetDecLayerIdSet( const InputNALUnit* nalu ) const
{
  if( !m_targetDecLayerIdSet.size() ) // By default, the set is empty, meaning all LayerIds are allowed
  {
    return true;
  }

  return std::find(m_targetDecLayerIdSet.begin(), m_targetDecLayerIdSet.end(), nalu->m_nuhLayerId)
         != m_targetDecLayerIdSet.end();
}

/** \param nalu Input nalu to check whether its LayerId is within targetOutputLayerIdSet
 */
bool DecApp::xIsNaluWithinTargetOutputLayerIdSet( const InputNALUnit* nalu ) const
{
  if( !m_targetOutputLayerIdSet.size() ) // By default, the set is empty, meaning all LayerIds are allowed
  {
    return true;
  }

  return std::find(m_targetOutputLayerIdSet.begin(), m_targetOutputLayerIdSet.end(), nalu->m_nuhLayerId)
         != m_targetOutputLayerIdSet.end();
}



//! \}
