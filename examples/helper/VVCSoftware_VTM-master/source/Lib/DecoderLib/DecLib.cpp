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

/** \file     DecLib.cpp
    \brief    decoder class
*/

#include "NALread.h"
#include "DecLib.h"

#include "CommonLib/dtrace_next.h"
#include "CommonLib/dtrace_buffer.h"
#include "CommonLib/Buffer.h"
#include "CommonLib/UnitTools.h"
#include "CommonLib/ProfileTierLevel.h"

#include <fstream>
#include <set>
#include <stdio.h>
#include <fcntl.h>
#include "AnnexBread.h"
#include "NALread.h"
#if K0149_BLOCK_STATISTICS
#include "CommonLib/dtrace_blockstatistics.h"
#endif

#if RExt__DECODER_DEBUG_TOOL_STATISTICS
#include "CommonLib/CodingStatistics.h"
#endif

#include "SEIDigitallySignedContent.h"

bool tryDecodePicture(Picture *pcEncPic, const int expectedPoc, const std::string &bitstreamFileName,
                      const int layerIdx, EnumArray<ParameterSetMap<APS>, ApsType> *apsMap, 
                      bool bDecodeUntilPocFound, int debugCTU, int debugPOC)
{
  int      poc;
  PicList *pcListPic = nullptr;

  static bool layerInitialized[MAX_VPS_LAYERS];             /* TODO: MT */
  static bool loopFiltered[MAX_VPS_LAYERS] = { false };            /* TODO: MT */
  static int  iPOCLastDisplay = -MAX_INT;         /* TODO: MT */

  static std::ifstream* bitstreamFile[MAX_VPS_LAYERS] = { nullptr };  /* TODO: MT */
  static InputByteStream* bytestream[MAX_VPS_LAYERS] = { nullptr };  /* TODO: MT */
  bool bRet = false;

  // create & initialize internal classes
  static DecLib *pcDecLib = nullptr;              /* TODO: MT */

  if( pcEncPic )
  {
    if(pcDecLib == nullptr)
    {
      // create decoder class
      pcDecLib = new DecLib;
      pcDecLib->create();

      // initialize decoder class
      pcDecLib->init(
#if  JVET_J0090_MEMORY_BANDWITH_MEASURE
        ""
#endif
      );

      pcDecLib->setDecodedPictureHashSEIEnabled( true );
      pcDecLib->setAPSMapEnc( apsMap );
    }
    pcDecLib->setDebugCTU( debugCTU );
    pcDecLib->setDebugPOC( debugPOC );

    if(!layerInitialized[layerIdx])
    {
      bitstreamFile[layerIdx] = new std::ifstream( bitstreamFileName.c_str(), std::ifstream::in | std::ifstream::binary );
      bytestream[layerIdx] = new InputByteStream( *bitstreamFile[layerIdx]);

      CHECK( !*bitstreamFile[layerIdx], "failed to open bitstream file " << bitstreamFileName.c_str() << " for reading" ) ;

      layerInitialized[layerIdx] = true;
      msg( INFO, "start to decode %s \n", bitstreamFileName.c_str() );
    }
    if (layerIdx > 0 && pcEncPic->cs->vps->getIndependentLayerFlag(layerIdx) != 1)
    {
      pcDecLib->setPrevPicPOC(pcEncPic->poc);
    }

    bool goOn = true;
    int lastDecodedLayerId = 0;
    bool decodedSliceInAU = false;

    // main decoder loop
    while( !!*bitstreamFile[layerIdx] && goOn )
    {
      InputNALUnit nalu;
      nalu.m_nalUnitType = NAL_UNIT_INVALID;

      // determine if next NAL unit will be the first one from a new picture
      bool bNewPicture = pcDecLib->isNewPicture( bitstreamFile[layerIdx],  bytestream[layerIdx]);
      bool bNewAccessUnit = bNewPicture && decodedSliceInAU && pcDecLib->isNewAccessUnit( bNewPicture, bitstreamFile[layerIdx],  bytestream[layerIdx]);

      if( !bNewPicture )
      {
        AnnexBStats stats = AnnexBStats();
        byteStreamNALUnit(*bytestream[layerIdx], nalu.getBitstream().getFifo(), stats);

        // call actual decoding function
        if (nalu.getBitstream().getFifo().empty())
        {
          /* this can happen if the following occur:
           *  - empty input file
           *  - two back-to-back start_code_prefixes
           *  - start_code_prefix immediately followed by EOF
           */
          msg(ERROR, "Warning: Attempt to decode an empty NAL unit\n");
        }
        else
        {
          read(nalu);
          int iSkipFrame = 0;
          if (nalu.m_nuhLayerId == pcEncPic->layerId
            || (layerInitialized[nalu.m_nuhLayerId] == false && nalu.m_nuhLayerId < pcEncPic->layerId))
          {
            EnumArray<ParameterSetMap<APS>, ApsType>* saved_apsMapEnc = nullptr;
            if (!layerInitialized[nalu.m_nuhLayerId])
            {
              saved_apsMapEnc = pcDecLib->m_apsMapEnc;
              pcDecLib->setAPSMapEnc(nullptr);
            }
            pcDecLib->decode(nalu, iSkipFrame, iPOCLastDisplay, 0);
            if (saved_apsMapEnc != nullptr)
            {
              pcDecLib->setAPSMapEnc(saved_apsMapEnc);
            }
          }
          if (nalu.isSlice())
          {
            decodedSliceInAU = true;
          }
        }
        lastDecodedLayerId = nalu.m_nuhLayerId;
      }


      if ((bNewPicture || !*bitstreamFile[layerIdx] || nalu.m_nalUnitType == NAL_UNIT_EOS) && !pcDecLib->getFirstSliceInSequence(lastDecodedLayerId))
      {
        if (!loopFiltered[lastDecodedLayerId] || *bitstreamFile[layerIdx])
        {
          pcDecLib->finishPictureLight( poc, pcListPic );

          if( pcListPic )
          {
            for( auto & pic : *pcListPic )
            {
              if( pic->poc == poc && pic->layerId == pcEncPic->layerId && (!bDecodeUntilPocFound || expectedPoc == poc ) )
              {
                CHECK( pcEncPic->slices.size() == 0, "at least one slice should be available" );

                CHECK( expectedPoc != poc, "mismatch in POC - check encoder configuration" );

                if( debugCTU < 0 || poc != debugPOC )
                {
                  for (int i = 0; i < pic->slices.size(); i++)
                  {
                    if (pcEncPic->slices.size() <= i)
                    {
                      pcEncPic->slices.push_back(new Slice);
                      pcEncPic->slices.back()->initSlice();
                      pcEncPic->slices.back()->setPPS(pcEncPic->slices[0]->getPPS());
                      pcEncPic->slices.back()->setSPS(pcEncPic->slices[0]->getSPS());
                      pcEncPic->slices.back()->setVPS(pcEncPic->slices[0]->getVPS());
                      pcEncPic->slices.back()->setPic(pcEncPic->slices[0]->getPic());
                    }
                    pcEncPic->slices[i]->copySliceInfo(pic->slices[i], false);
                  }
                }

                pcEncPic->cs->slice = pcEncPic->slices.back();

                if( debugCTU >= 0 && poc == debugPOC )
                {
                  pcEncPic->cs->initStructData();

                  pcEncPic->cs->copyStructure(*pic->cs, ChannelType::LUMA, true, true);

                  if( CS::isDualITree( *pcEncPic->cs ) )
                  {
                    pcEncPic->cs->copyStructure(*pic->cs, ChannelType::CHROMA, true, true);
                  }

                  for( auto &cu : pcEncPic->cs->cus )
                  {
                    cu->slice = pcEncPic->cs->slice;
                  }
                }
                else
                {
                  if (pic->cs->sps->getSAOEnabledFlag())
                  {
                    pcEncPic->copySAO(*pic, 0);
                  }

                  if (pic->cs->sps->getALFEnabledFlag())
                  {
                    pcEncPic->copyAlfData(*pic);

                    for (int i = 0; i < pic->slices.size(); i++)
                    {
                      pcEncPic->slices[i]->setNumAlfApsIdsLuma(pic->slices[i]->getNumAlfApsIdsLuma());
                      pcEncPic->slices[i]->setAlfApsIdsLuma(pic->slices[i]->getAlfApsIdsLuma());
                      pcEncPic->slices[i]->setAlfAPSs(pic->slices[i]->getAlfAPSs());
                      pcEncPic->slices[i]->setAlfApsIdChroma(pic->slices[i]->getAlfApsIdChroma());
                      pcEncPic->slices[i]->setAlfEnabledFlag(COMPONENT_Y,
                                                             pic->slices[i]->getAlfEnabledFlag(COMPONENT_Y));
                      pcEncPic->slices[i]->setAlfEnabledFlag(COMPONENT_Cb,
                                                             pic->slices[i]->getAlfEnabledFlag(COMPONENT_Cb));
                      pcEncPic->slices[i]->setAlfEnabledFlag(COMPONENT_Cr,
                                                             pic->slices[i]->getAlfEnabledFlag(COMPONENT_Cr));
                      pcEncPic->slices[i]->setCcAlfCbApsId(pic->slices[i]->getCcAlfCbApsId());
                      pcEncPic->slices[i]->setCcAlfCbEnabledFlag(pic->slices[i]->getCcAlfCbEnabledFlag());
                      pcEncPic->slices[i]->setCcAlfCrApsId(pic->slices[i]->getCcAlfCrApsId());
                      pcEncPic->slices[i]->setCcAlfCrEnabledFlag(pic->slices[i]->getCcAlfCrEnabledFlag());
                    }
                  }

                  pcDecLib->executeLoopFilters();
                  if (pic->cs->sps->getSAOEnabledFlag())
                  {
                    pcEncPic->copySAO(*pic, 1);
                  }

                  pcEncPic->cs->initStructData();
                  pcEncPic->cs->copyStructure(*pic->cs, ChannelType::LUMA, true, true);

                  if (CS::isDualITree(*pcEncPic->cs))
                  {
                    pcEncPic->cs->copyStructure(*pic->cs, ChannelType::CHROMA, true, true);
                  }
                }
                goOn = false; // exit the loop return
                bRet = true;
                break;
              }
            }
          }
          // postpone loop filters
          if (!bRet)
          {
            pcDecLib->executeLoopFilters();
          }

          pcDecLib->finishPicture( poc, pcListPic, DETAILS );

          // write output
          if( ! pcListPic->empty())
          {
            PicList::iterator iterPic   = pcListPic->begin();
            int numPicsNotYetDisplayed = 0;
            int dpbFullness = 0;
            const SPS* activeSPS = (pcListPic->front()->cs->sps);
            uint32_t maxNrSublayers = activeSPS->getMaxTLayers();
            uint32_t maxNumReorderPicsHighestTid = activeSPS->getMaxNumReorderPics(maxNrSublayers-1);
            uint32_t maxDecPicBufferingHighestTid =  activeSPS->getMaxDecPicBuffering(maxNrSublayers-1);
            const VPS* referredVPS = pcListPic->front()->cs->vps;

            if( referredVPS != nullptr && referredVPS->m_numLayersInOls[referredVPS->m_targetOlsIdx] > 1 )
            {
              maxNumReorderPicsHighestTid = referredVPS->getMaxNumReorderPics( maxNrSublayers - 1 );
              maxDecPicBufferingHighestTid = referredVPS->getMaxDecPicBuffering( maxNrSublayers - 1 );
            }

            while (iterPic != pcListPic->end())
            {
              Picture* pcCurPic = *(iterPic);
              if(pcCurPic->neededForOutput && pcCurPic->getPOC() > iPOCLastDisplay)
              {
                numPicsNotYetDisplayed++;
                dpbFullness++;
              }
              else if(pcCurPic->referenced)
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

            Picture* pcCurPic = *(iterPic);
            if( numPicsNotYetDisplayed>2 && pcCurPic->fieldPic ) //Field Decoding
            {
              THROW( "no field coding support ");
            }
            else if( !pcCurPic->fieldPic ) //Frame Decoding
            {
              iterPic = pcListPic->begin();

              while (iterPic != pcListPic->end())
              {
                pcCurPic = *(iterPic);

                if(pcCurPic->neededForOutput && pcCurPic->getPOC() > iPOCLastDisplay &&
                  (numPicsNotYetDisplayed >  maxNumReorderPicsHighestTid || dpbFullness > maxDecPicBufferingHighestTid))
                {
                  numPicsNotYetDisplayed--;
                  if( ! pcCurPic->referenced )
                  {
                    dpbFullness--;
                  }
                  // update POC of display order
                  iPOCLastDisplay = pcCurPic->getPOC();

                  // erase non-referenced picture in the reference picture list after display
                  if( ! pcCurPic->referenced && pcCurPic->reconstructed )
                  {
                    pcCurPic->reconstructed = false;
                  }
                  pcCurPic->neededForOutput = false;
                }

                iterPic++;
              }
            }
          }

          pcDecLib->updateAssociatedIRAP();
          pcDecLib->updatePrevGDRInSameLayer();
          pcDecLib->updatePrevIRAPAndGDRSubpic();
          // LMCS APS will be assigned later in LMCS initialization step
          pcEncPic->cs->picHeader->setLmcsAPS( nullptr );
          if (bitstreamFile[layerIdx])
          {
            pcDecLib->resetAccessUnitNals();
            pcDecLib->resetAccessUnitApsNals();
          }
        }
        loopFiltered[lastDecodedLayerId] = (nalu.m_nalUnitType == NAL_UNIT_EOS);
        if( nalu.m_nalUnitType == NAL_UNIT_EOS )
        {
          pcDecLib->setFirstSliceInSequence(true, lastDecodedLayerId);
        }

      }
      else if ((bNewPicture || !*bitstreamFile[layerIdx] || nalu.m_nalUnitType == NAL_UNIT_EOS) && pcDecLib->getFirstSliceInSequence(lastDecodedLayerId))
      {
        pcDecLib->setFirstSliceInPicture( true );
      }
      if (bNewAccessUnit)
      {
        decodedSliceInAU = false;
      }
    }
  }

  if( !bRet )
  {
    CHECK( bDecodeUntilPocFound, " decoding failed - check decodeBitstream2 parameter File: " << bitstreamFileName.c_str() );
    bool destroyAll = false;
    if (layerIdx < 0)
    {
      std::fill_n(layerInitialized, MAX_VPS_LAYERS, false);
      std::fill_n(loopFiltered, MAX_VPS_LAYERS, false);
      iPOCLastDisplay = -MAX_INT;
      for (int i = 0; i < MAX_VPS_LAYERS; i++)
      {
        if (bytestream[i])
        {
          delete bytestream[i];
          bytestream[i] = nullptr;
        }
        if (bitstreamFile[i])
        {
          delete bitstreamFile[i];
          bitstreamFile[i] = nullptr;
        }
      }
      destroyAll = true;
    }
    else
    {
      layerInitialized[layerIdx] = false;
      loopFiltered[layerIdx] = false;
      if (bytestream[layerIdx])
      {
        delete bytestream[layerIdx];
        bytestream[layerIdx] = nullptr;
      }
      if (bitstreamFile[layerIdx])
      {
        delete bitstreamFile[layerIdx];
        bitstreamFile[layerIdx] = nullptr;
      }
      if (std::find(layerInitialized, layerInitialized + MAX_VPS_LAYERS, true) == layerInitialized + MAX_VPS_LAYERS)
      {
        destroyAll = true;
      }
    }

    if (destroyAll)
    {
      if (pcDecLib)
      {
        pcDecLib->destroy();
        pcDecLib->deletePicBuffer();
        delete pcDecLib;
        pcDecLib = nullptr;
      }
      iPOCLastDisplay = -MAX_INT;
    }
  }

  return bRet;
}


//! \ingroup DecoderLib
//! \{

DecLib::DecLib()
  : m_maxRefPicNum(0)
  , m_isFirstGeneralHrd(true)
  , m_prevGeneralHrdParams()
  , m_latestDRAPPOC(MAX_INT)
  , m_latestEDRAPPOC(MAX_INT)
  , m_latestEDRAPIndicationLeadingPicturesDecodableFlag(false)
  , m_associatedIRAPDecodingOrderNumber{ 0 }
  , m_decodingOrderCounter(0)
  , m_puCounter(0)
  , m_seiInclusionFlag(false)
  , m_pocRandomAccess(MAX_INT)
  , m_lastRasPoc(MAX_INT)
  , m_cListPic()
  , m_parameterSetManager()
  , m_apcSlicePilot(nullptr)
  , m_SEIs()
  , m_sdiSEIInFirstAU(nullptr)
  , m_maiSEIInFirstAU(nullptr)
  , m_mvpSEIInFirstAU(nullptr)
  , m_cIntraPred()
  , m_cInterPred()
  , m_cTrQuant()
  , m_cSliceDecoder()
  , m_cTrQuantScalingList()
  , m_cCuDecoder()
  , m_HLSReader()
  , m_seiReader()
  , m_deblockingFilter()
  , m_cSAO()
  , m_cReshaper()
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  , m_cacheModel()
#endif
  , m_pcPic(nullptr)
  , m_prevLayerID(MAX_INT)
  , m_prevPOC(MAX_INT)
  , m_prevPicPOC(MAX_INT)
  , m_prevTid0POC(0)
  , m_bFirstSliceInPicture(true)
  , m_firstPictureInSequence(true)
  , m_grainCharacteristic()
  , m_grainBuf()
  , m_colourTranfParams()
  , m_firstSliceInBitstream(true)
  , m_isFirstAuInCvs(true)
  , m_prevSliceSkipped(false)
  , m_skippedPOC(MAX_INT)
  , m_skippedLayerID(MAX_INT)
  , m_lastPOCNoOutputPriorPics(-1)
  , m_isNoOutputPriorPics(false)
  , m_lastNoOutputBeforeRecoveryFlag{ false }
  , m_sliceLmcsApsId(-1)
  , m_pDecodedSEIOutputStream(nullptr)
  , m_audIrapOrGdrAuFlag(false)
#if JVET_S0257_DUMP_360SEI_MESSAGE
  , m_decoded360SeiDumpFileName()
#endif
  , m_decodedPictureHashSEIEnabled(false)
  , m_numberOfChecksumErrorsDetected(0)
  , m_warningMessageSkipPicture(false)
  , m_prefixSEINALUs()
  , m_ShutterFilterEnable(false)
  , m_debugPOC(-1)
  , m_debugCTU(-1)
  , m_opi(nullptr)
  , m_mTidExternalSet(false)
  , m_mTidOpiSet(false)
  , m_tOlsIdxTidExternalSet(false)
  , m_tOlsIdxTidOpiSet(false)
  , m_vps(nullptr)
  , m_maxDecSubPicIdx(0)
  , m_maxDecSliceAddrInSubPic(-1)
  , m_clsVPSid(0)
#if GDR_ENABLED
  , m_lastGdrPoc (-1)
  , m_lastGdrRecoveryPocCnt(0)
#endif
  , m_targetSubPicIdx(0)
  , m_dci(nullptr)
{
#if ENABLE_SIMD_OPT_BUFFER
  g_pelBufOP.initPelBufOpsX86();
#endif
  memset(m_prevEOS, false, sizeof(m_prevEOS));
  memset(m_accessUnitEos, false, sizeof(m_accessUnitEos));
  std::fill_n(m_prevGDRInSameLayerPOC, MAX_VPS_LAYERS, -MAX_INT);
  std::fill_n(m_prevGDRInSameLayerRecoveryPOC, MAX_VPS_LAYERS, -MAX_INT);
  std::fill_n(m_firstSliceInSequence, MAX_VPS_LAYERS, true);
  std::fill_n(m_pocCRA, MAX_VPS_LAYERS, -MAX_INT);
  std::fill_n(m_accessUnitSpsNumSubpic, MAX_VPS_LAYERS, 1);
  for (int i = 0; i < MAX_VPS_LAYERS; i++)
  {
    m_associatedIRAPType[i] = NAL_UNIT_INVALID;
    std::fill_n(m_prevGDRSubpicPOC[i], MAX_NUM_SUB_PICS, -MAX_INT);
    std::fill_n(m_prevIRAPSubpicPOC[i], MAX_NUM_SUB_PICS, -MAX_INT);
    memset(m_prevIRAPSubpicDecOrderNo[i], 0, sizeof(int)*MAX_NUM_SUB_PICS);
    std::fill_n(m_prevIRAPSubpicType[i], MAX_NUM_SUB_PICS, NAL_UNIT_INVALID);
  }
}

DecLib::~DecLib()
{
  resetAccessUnitSeiNalus();
  resetPictureSeiNalus();
  resetPrefixSeiNalus();

  if (m_sdiSEIInFirstAU != nullptr)
  {
    delete m_sdiSEIInFirstAU;
  }
  m_sdiSEIInFirstAU = nullptr;
  if (m_maiSEIInFirstAU != nullptr)
  {
    delete m_maiSEIInFirstAU;
  }
  m_maiSEIInFirstAU = nullptr;
  if (m_mvpSEIInFirstAU != nullptr)
  {
    delete m_mvpSEIInFirstAU;
  }
  m_mvpSEIInFirstAU = nullptr;
}

void DecLib::create()
{
  m_apcSlicePilot = new Slice;
  m_uiSliceSegmentIdx = 0;
}

void DecLib::destroy()
{
  delete m_apcSlicePilot;
  m_apcSlicePilot = nullptr;

  if( m_dci )
  {
    delete m_dci;
    m_dci = nullptr;
  }

  if (m_opi)
  {
    delete m_opi;
    m_opi = nullptr;
  }

  m_cSliceDecoder.destroy();
}

void DecLib::init(
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  const std::string& cacheCfgFileName
#endif
)
{
  m_cSliceDecoder.init( &m_CABACDecoder, &m_cCuDecoder );
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  m_cacheModel.create( cacheCfgFileName );
  m_cacheModel.clear( );
  m_cInterPred.cacheAssign( &m_cacheModel );
#endif
  DTRACE_UPDATE( g_trace_ctx, std::make_pair( "final", 1 ) );
}

void DecLib::deletePicBuffer ( )
{
  PicList::iterator  iterPic   = m_cListPic.begin();
  int                size      = int(m_cListPic.size());

  for (int i = 0; i < size; i++)
  {
    Picture* pcPic = *(iterPic++);
    pcPic->destroy();

    delete pcPic;
    pcPic = nullptr;
  }
  m_cALF.destroy();
  m_cSAO.destroy();
  m_deblockingFilter.destroy();
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  m_cacheModel.reportSequence( );
  m_cacheModel.destroy( );
#endif
  m_cCuDecoder.destoryDecCuReshaprBuf();
  m_cReshaper.destroy();
}

Picture* DecLib::xGetNewPicBuffer( const SPS &sps, const PPS &pps, const uint32_t temporalLayer, const int layerId )
{
  Picture * pcPic = nullptr;
  // getMaxDecPicBuffering() has space for the picture currently being decoded
  m_maxRefPicNum = (m_vps == nullptr || m_vps->m_numLayersInOls[m_vps->m_targetOlsIdx] == 1)
                     ? sps.getMaxDecPicBuffering(temporalLayer)
                     : m_vps->getMaxDecPicBuffering(temporalLayer);

  const bool allocateWrappedPic = (m_vps == nullptr || m_vps->m_numLayersInOls[m_vps->m_targetOlsIdx] == 1)
                                    ? sps.getWrapAroundEnabledFlag()
                                    : true;

  if (m_cListPic.size() < (uint32_t) m_maxRefPicNum)
  {
    pcPic = new Picture();
    pcPic->create(allocateWrappedPic, sps.getChromaFormatIdc(), Size(pps.getPicWidthInLumaSamples(), pps.getPicHeightInLumaSamples()),
      sps.getMaxCUWidth(), sps.getMaxCUWidth() + PIC_MARGIN, true, layerId, getShutterFilterFlag() );

    m_cListPic.push_back( pcPic );

    return pcPic;
  }

  bool bBufferIsAvailable = false;
  for(auto * p: m_cListPic)
  {
    pcPic = p;  // workaround because range-based for-loops don't work with existing variables
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
    if ( pcPic->reconstructed == false && ! pcPic->neededForOutput && pcPic->layerId == layerId )
#else
    if ( pcPic->reconstructed == false && ! pcPic->neededForOutput )
#endif
    {
      pcPic->neededForOutput = false;
      bBufferIsAvailable = true;
      break;
    }

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
    if( ! pcPic->referenced  && ! pcPic->neededForOutput && pcPic->layerId == layerId )
#else
    if( ! pcPic->referenced  && ! pcPic->neededForOutput )
#endif
    {
      pcPic->neededForOutput = false;
      pcPic->reconstructed = false;
      bBufferIsAvailable = true;
      break;
    }
  }

  if( ! bBufferIsAvailable )
  {
    //There is no room for this picture, either because of faulty encoder or dropped NAL. Extend the buffer.
    m_maxRefPicNum++;

    pcPic = new Picture();

    m_cListPic.push_back( pcPic );

    pcPic->create(allocateWrappedPic, sps.getChromaFormatIdc(), Size(pps.getPicWidthInLumaSamples(), pps.getPicHeightInLumaSamples()), sps.getMaxCUWidth(), sps.getMaxCUWidth() + PIC_MARGIN, true, layerId, getShutterFilterFlag());
  }
  else
  {
    if( !pcPic->Y().Size::operator==( Size( pps.getPicWidthInLumaSamples(), pps.getPicHeightInLumaSamples() ) ) || pps.pcv->maxCUWidth != sps.getMaxCUWidth() || pps.pcv->maxCUHeight != sps.getMaxCUHeight() || pcPic->layerId != layerId )
    {
      pcPic->destroy();

      pcPic->create(allocateWrappedPic, sps.getChromaFormatIdc(), Size( pps.getPicWidthInLumaSamples(), pps.getPicHeightInLumaSamples() ), sps.getMaxCUWidth(), sps.getMaxCUWidth() + PIC_MARGIN, true, layerId, getShutterFilterFlag());
    }
  }

  pcPic->setBorderExtension( false );
  pcPic->neededForOutput = false;
  pcPic->reconstructed = false;

  return pcPic;
}

#if JVET_AJ0151_DSC_SEI
void DecLib::xStoreNALUnitForSignature(InputNALUnit &nalu)
{
  std::ostringstream rbspPayload;
  binNalUnit binNalu;
  binNalu.nalUnitType = nalu.m_nalUnitType;
  binNalu.length  = nalu.getBitstream().getOrigFifo().size();
  binNalu.data = new uint8_t [binNalu.length];

  std::memcpy(binNalu.data, nalu.getBitstream().getOrigFifo().data(), binNalu.length);

  m_signedContentNalUnitBuffer.push_back(binNalu);
  nalu.getBitstream().clearOrigFifo();
}

void DecLib::xProcessStoredNALUnitsForSignature(int substreamId)
{
  const bool verificationActive = m_dscSubstreamManager.isVerificationActive();
  for (auto nalu: m_signedContentNalUnitBuffer)
  {
    if (verificationActive)
    {
      m_dscSubstreamManager.addToSubstream(substreamId, (char*)nalu.data, nalu.length);
    }
    delete[] (nalu.data);
  }
  m_signedContentNalUnitBuffer.clear();
}
#endif

void DecLib::executeLoopFilters()
{
  if( !m_pcPic )
  {
    return; // nothing to deblock
  }

  m_pcPic->cs->slice->startProcessingTimer();

  CodingStructure& cs = *m_pcPic->cs;

  if (cs.sps->getUseLmcs() && cs.picHeader->getLmcsEnabledFlag())
  {
    const PreCalcValues &pcv = *cs.pcv;
    for (uint32_t yPos = 0; yPos < pcv.lumaHeight; yPos += pcv.maxCUHeight)
    {
      for (uint32_t xPos = 0; xPos < pcv.lumaWidth; xPos += pcv.maxCUWidth)
      {
        const CodingUnit *cu = cs.getCU(Position(xPos, yPos), ChannelType::LUMA);
        if (cu->slice->getLmcsEnabledFlag())
        {
          const uint32_t width  = (xPos + pcv.maxCUWidth > pcv.lumaWidth) ? (pcv.lumaWidth - xPos) : pcv.maxCUWidth;
          const uint32_t height = (yPos + pcv.maxCUHeight > pcv.lumaHeight) ? (pcv.lumaHeight - yPos) : pcv.maxCUHeight;
          const UnitArea area(cs.area.chromaFormat, Area(xPos, yPos, width, height));
          cs.getRecoBuf(area).get(COMPONENT_Y).rspSignal(m_cReshaper.getInvLUT());
        }
      }
    }
    m_cReshaper.setRecReshaped(false);
    m_cSAO.setReshaper(&m_cReshaper);
  }
#if GREEN_METADATA_SEI_ENABLED
  FeatureCounterStruct initValues;
  cs.m_featureCounter =  initValues;
#endif
  // deblocking filter
  m_deblockingFilter.deblockingFilterPic( cs );
  CS::setRefinedMotionField(cs);
  if( cs.sps->getSAOEnabledFlag() )
  {
    m_cSAO.SAOProcess( cs, cs.picture->getSAO() );
  }

  if( cs.sps->getALFEnabledFlag() )
  {
    m_cALF.getCcAlfFilterParam() = cs.slice->m_ccAlfFilterParam;
    // ALF decodes the differentially coded coefficients and stores them in the parameters structure.
    // Code could be restructured to do directly after parsing. So far we just pass a fresh non-const
    // copy in case the APS gets used more than once.
    m_cALF.ALFProcess(cs);
  }

#if GREEN_METADATA_SEI_ENABLED
  m_featureCounter.addSAO(cs.m_featureCounter);
  m_featureCounter.addALF(cs.m_featureCounter);
  m_featureCounter.addBoundaryStrengths(cs.m_featureCounter);
#endif
  for (int i = 0; i < cs.pps->getNumSubPics() && m_targetSubPicIdx; i++)
  {
    // keep target subpic samples untouched, for other subpics mask their output sample value to 0
    int targetSubPicIdx = m_targetSubPicIdx - 1;
    if (i != targetSubPicIdx)
    {
      SubPic SubPicNoUse = cs.pps->getSubPics()[i];
      uint32_t left  = SubPicNoUse.getSubPicLeft();
      uint32_t right = SubPicNoUse.getSubPicRight();
      uint32_t top   = SubPicNoUse.getSubPicTop();
      uint32_t bottom= SubPicNoUse.getSubPicBottom();
      for (uint32_t row = top; row <= bottom; row++)
      {
        for (uint32_t col = left; col <= right; col++)
        {
          cs.getRecoBuf().Y().at(col, row) = 0;
          // for test only, hard coding using 4:2:0 chroma format
          cs.getRecoBuf().Cb().at(col>>1, row>>1) = 0;
          cs.getRecoBuf().Cr().at(col>>1, row>>1) = 0;
        }
      }
    }
  }

  m_pcPic->cs->slice->stopProcessingTimer();
}

void DecLib::applyNnPostFilter()
{
  if(m_cListPic.empty())
  {
    return;
  }
  m_nnPostFiltering.filterPictures(m_cListPic);
}

void DecLib::finishPictureLight(int& poc, PicList*& rpcListPic )
{
  Slice*  pcSlice = m_pcPic->cs->slice;

  m_pcPic->neededForOutput = (pcSlice->getPicHeader()->getPicOutputFlag() ? true : false);

  const VPS *vps = pcSlice->getVPS();
  if (vps != nullptr)
  {
    if (!vps->getEachLayerIsAnOlsFlag())
    {
      const int layerId        = pcSlice->getNalUnitLayerId();
      const int generalLayerId = vps->getGeneralLayerIdx(layerId);
      bool      layerIsOutput  = true;

      if (vps->getOlsModeIdc() == 0)
      {
        layerIsOutput = generalLayerId == vps->m_targetOlsIdx;
      }
      else if (vps->getOlsModeIdc() == 1)
      {
        layerIsOutput = generalLayerId <= vps->m_targetOlsIdx;
      }
      else if (vps->getOlsModeIdc() == 2)
      {
        layerIsOutput = vps->getOlsOutputLayerFlag(vps->m_targetOlsIdx, generalLayerId);
      }
      if (!layerIsOutput)
      {
        m_pcPic->neededForOutput = false;
      }
    }
  }
  m_pcPic->reconstructed = true;

  Slice::sortPicList( m_cListPic ); // sorting for application output
  poc                 = pcSlice->getPOC();
  rpcListPic          = &m_cListPic;
  m_puCounter++;
}

void DecLib::finishPicture(int &poc, PicList *&rpcListPic, MsgLevel msgl, bool associatedWithNewClvs)
{
#if RExt__DECODER_DEBUG_TOOL_STATISTICS
  CodingStatistics::StatTool& s = CodingStatistics::GetStatisticTool( STATS__TOOL_TOTAL_FRAME );
  s.count++;
  s.pixels = s.count * m_pcPic->Y().width * m_pcPic->Y().height;
#endif

  Slice*  pcSlice = m_pcPic->cs->slice;
  m_prevPicPOC = pcSlice->getPOC();
#if GREEN_METADATA_SEI_ENABLED
  m_featureCounter.height = m_pcPic->Y().height;
  m_featureCounter.width = m_pcPic->Y().width;
#endif
  
  char c = (pcSlice->isIntra() ? 'I' : pcSlice->isInterP() ? 'P' : 'B');
  if (!m_pcPic->referenced)
  {
    c += 32;  // tolower
  }

  if (pcSlice->isDRAP())
  {
    c = 'D';
  }
  if (pcSlice->getEdrapRapId() > 0)
  {
    c = 'E';
  }

  //-- For time output for each slice
  msg( msgl, "POC %4d LId: %2d TId: %1d ( %s, %c-SLICE, QP%3d ) ", pcSlice->getPOC(), pcSlice->getPic()->layerId,
         pcSlice->getTLayer(),
         nalUnitTypeToString(pcSlice->getNalUnitType()),
         c,
         pcSlice->getSliceQp() );
  msg( msgl, "[DT %6.3f] ", pcSlice->getProcessingTime() );

  for (int refList = 0; refList < 2; refList++)
  {
    msg(msgl, "[L%d", refList);
    for (int refIndex = 0; refIndex < pcSlice->getNumRefIdx(RefPicList(refList)); refIndex++)
    {
      const ScalingRatio &scaleRatio = pcSlice->getScalingRatio(RefPicList(refList), refIndex);

      if (pcSlice->getPicHeader()->getEnableTMVPFlag() && pcSlice->getColFromL0Flag() == bool(1 - refList)
          && pcSlice->getColRefIdx() == refIndex)
      {
        if (scaleRatio != SCALE_1X)
        {
          msg(msgl, " %dc(%1.2lfx, %1.2lfx)", pcSlice->getRefPOC(RefPicList(refList), refIndex),
              double(scaleRatio.x) / (1 << ScalingRatio::BITS), double(scaleRatio.y) / (1 << ScalingRatio::BITS));
        }
        else
        {
          msg(msgl, " %dc", pcSlice->getRefPOC(RefPicList(refList), refIndex));
        }
      }
      else
      {
        if (scaleRatio != SCALE_1X)
        {
          msg(msgl, " %d(%1.2lfx, %1.2lfx)", pcSlice->getRefPOC(RefPicList(refList), refIndex),
              double(scaleRatio.x) / (1 << ScalingRatio::BITS), double(scaleRatio.y) / (1 << ScalingRatio::BITS));
        }
        else
        {
          msg(msgl, " %d", pcSlice->getRefPOC(RefPicList(refList), refIndex));
        }
      }

      if (pcSlice->getRefPOC(RefPicList(refList), refIndex) == pcSlice->getPOC())
      {
        msg(msgl, ".%d", pcSlice->getRefPic(RefPicList(refList), refIndex)->layerId);
      }
    }
    msg( msgl, "] ");
  }
  if (m_decodedPictureHashSEIEnabled)
  {
    SEIMessages                  pictureHashes = getSeisByType(m_pcPic->SEIs, SEI::PayloadType::DECODED_PICTURE_HASH);
    const SEIDecodedPictureHash *hash =
      (pictureHashes.size() > 0) ? (SEIDecodedPictureHash *) *(pictureHashes.begin()) : nullptr;
    if (pictureHashes.size() > 1)
    {
      msg( WARNING, "Warning: Got multiple decoded picture hash SEI messages. Using first.");
    }
    m_numberOfChecksumErrorsDetected += calcAndPrintHashStatus(((const Picture*) m_pcPic)->getRecoBuf(), hash, pcSlice->getSPS()->getBitDepths(), msgl);

    SEIMessages snList = getSeisByType(m_pcPic->SEIs, SEI::PayloadType::SCALABLE_NESTING);
    for (auto& sei: snList)
    {
      auto sn = reinterpret_cast<SEIScalableNesting*>(sei);
      if (!sn->subpicId.empty())
      {
        const uint32_t subpicId = sn->subpicId.front();

        SEIMessages nestedPictureHashes = getSeisByType(sn->nestedSeis, SEI::PayloadType::DECODED_PICTURE_HASH);
        for (auto decPicHash : nestedPictureHashes)
        {
          const SubPic& subpic = pcSlice->getPPS()->getSubPic(subpicId);
          const UnitArea area = UnitArea(pcSlice->getSPS()->getChromaFormatIdc(), Area(subpic.getSubPicLeft(), subpic.getSubPicTop(), subpic.getSubPicWidthInLumaSample(), subpic.getSubPicHeightInLumaSample()));
          PelUnitBuf recoBuf = m_pcPic->cs->getRecoBuf(area);
          m_numberOfChecksumErrorsDetected += calcAndPrintHashStatus(
            recoBuf, reinterpret_cast<SEIDecodedPictureHash*>(decPicHash), pcSlice->getSPS()->getBitDepths(), msgl);
        }
      }
    }
  }

  msg( msgl, "\n");

#if JVET_J0090_MEMORY_BANDWITH_MEASURE
  m_cacheModel.reportFrame();
  m_cacheModel.accumulateFrame();
  m_cacheModel.clear();
#endif

  m_pcPic->neededForOutput = (pcSlice->getPicHeader()->getPicOutputFlag() ? true : false);
  if (associatedWithNewClvs && m_pcPic->neededForOutput)
  {
    if (!pcSlice->getPPS()->getMixedNaluTypesInPicFlag() && pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL)
    {
      m_pcPic->neededForOutput = false;
    }
    else if (pcSlice->getPPS()->getMixedNaluTypesInPicFlag())
    {
      bool isRaslPic = true;
      for (int i = 0; isRaslPic && i < m_pcPic->numSlices; i++)
      {
        if (!(pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RADL))
        {
          isRaslPic = false;
        }
      }
      if (isRaslPic)
      {
        m_pcPic->neededForOutput = false;
      }
    }
  }

  const VPS *vps = pcSlice->getVPS();
  if (vps != nullptr)
  {
    if (!vps->getEachLayerIsAnOlsFlag())
    {
      const int layerId        = pcSlice->getNalUnitLayerId();
      const int generalLayerId = vps->getGeneralLayerIdx(layerId);
      bool      layerIsOutput  = true;

      if (vps->getOlsModeIdc() == 0)
      {
        layerIsOutput = generalLayerId == vps->m_targetOlsIdx;
      }
      else if (vps->getOlsModeIdc() == 1)
      {
        layerIsOutput = generalLayerId <= vps->m_targetOlsIdx;
      }
      else if (vps->getOlsModeIdc() == 2)
      {
        layerIsOutput = vps->getOlsOutputLayerFlag(vps->m_targetOlsIdx, generalLayerId);
      }
      if (!layerIsOutput)
      {
        m_pcPic->neededForOutput = false;
      }
    }
  }
  m_pcPic->reconstructed = true;

  // process buffered suffix APS NALUs
  processSuffixApsNalus();

  Slice::sortPicList( m_cListPic ); // sorting for application output
  poc                 = pcSlice->getPOC();
  rpcListPic          = &m_cListPic;
  m_bFirstSliceInPicture  = true; // TODO: immer true? hier ist irgendwas faul
  m_maxDecSubPicIdx = 0;
  m_maxDecSliceAddrInSubPic = -1;

  m_pcPic->destroyTempBuffers();
  m_pcPic->cs->destroyTemporaryCsData();
#if !GDR_ENABLED
  m_pcPic->cs->picHeader->initPicHeader();
#endif
  m_puCounter++;
}

void DecLib::checkNoOutputPriorPics (PicList* pcListPic)
{
  if (!pcListPic || !m_isNoOutputPriorPics)
  {
    return;
  }

  PicList::iterator  iterPic   = pcListPic->begin();

  while (iterPic != pcListPic->end())
  {
    Picture* pcPicTmp = *(iterPic++);
    if (m_lastPOCNoOutputPriorPics != pcPicTmp->getPOC())
    {
      pcPicTmp->neededForOutput = false;
    }
  }
}

void DecLib::xUpdateRasInit(Slice* slice)
{
  slice->setPendingRasInit( false );
  if ( slice->getPOC() > m_lastRasPoc )
  {
    m_lastRasPoc = MAX_INT;
    slice->setPendingRasInit( true );
  }
  if ( slice->isIRAP() )
  {
    m_lastRasPoc = slice->getPOC();
  }
}

void DecLib::xCreateLostPicture( int iLostPoc, const int layerId )
{
  msg( INFO, "\ninserting lost poc : %d\n",iLostPoc);
  Picture *cFillPic = xGetNewPicBuffer( *( m_parameterSetManager.getFirstSPS() ), *( m_parameterSetManager.getFirstPPS() ), 0, layerId );

  CHECK( !cFillPic->slices.size(), "No slices in picture" );

  cFillPic->slices[0]->initSlice();

  PicList::iterator iterPic = m_cListPic.begin();
  int closestPoc = 1000000;
  while ( iterPic != m_cListPic.end())
  {
    Picture * rpcPic = *(iterPic++);
    if(abs(rpcPic->getPOC() -iLostPoc)<closestPoc&&abs(rpcPic->getPOC() -iLostPoc)!=0&&rpcPic->getPOC()!=m_apcSlicePilot->getPOC())
    {
      closestPoc=abs(rpcPic->getPOC() -iLostPoc);
    }
  }
  iterPic = m_cListPic.begin();
  while ( iterPic != m_cListPic.end())
  {
    Picture *rpcPic = *(iterPic++);
    if(abs(rpcPic->getPOC() -iLostPoc)==closestPoc&&rpcPic->getPOC()!=m_apcSlicePilot->getPOC())
    {
      msg( INFO, "copying picture %d to %d (%d)\n",rpcPic->getPOC() ,iLostPoc,m_apcSlicePilot->getPOC());
      cFillPic->getRecoBuf().copyFrom( rpcPic->getRecoBuf() );
      break;
    }
  }

//  for(int ctuRsAddr=0; ctuRsAddr<cFillPic->getNumberOfCtusInFrame(); ctuRsAddr++)  { cFillPic->getCtu(ctuRsAddr)->initCtu(cFillPic, ctuRsAddr); }
  cFillPic->referenced = true;
  cFillPic->slices[0]->setPOC(iLostPoc);
  xUpdatePreviousTid0POC(cFillPic->slices[0]);
  cFillPic->reconstructed = true;
  cFillPic->neededForOutput = true;
  if(m_pocRandomAccess == MAX_INT)
  {
    m_pocRandomAccess = iLostPoc;
  }
}

void  DecLib::xCreateUnavailablePicture( const PPS *pps, const int iUnavailablePoc, const bool longTermFlag, const int temporalId, const int layerId, const bool interLayerRefPicFlag )
{
  msg(INFO, "Note: Inserting unavailable POC : %d\n", iUnavailablePoc);
  auto const sps = m_parameterSetManager.getSPS(pps->getSPSId());
  Picture* cFillPic = xGetNewPicBuffer( *sps, *pps, 0, layerId );

  cFillPic->cs      = new CodingStructure(g_xuPool);
  cFillPic->cs->sps = sps;
  cFillPic->cs->pps = pps;
  cFillPic->cs->vps = m_parameterSetManager.getVPS(sps->getVPSId());
  cFillPic->cs->create(cFillPic->cs->sps->getChromaFormatIdc(), Area(0, 0, cFillPic->cs->pps->getPicWidthInLumaSamples(), cFillPic->cs->pps->getPicHeightInLumaSamples()), true, (bool)(cFillPic->cs->sps->getPLTMode()));
  cFillPic->allocateNewSlice();
  cFillPic->m_chromaFormatIdc = sps->getChromaFormatIdc();
  cFillPic->m_bitDepths = sps->getBitDepths();

  cFillPic->slices[0]->initSlice();

  cFillPic->setDecodingOrderNumber(0);
  cFillPic->subLayerNonReferencePictureDueToSTSA = false;
  cFillPic->unscaledPic = cFillPic;

  uint32_t yFill = 1 << (sps->getBitDepth(ChannelType::LUMA) - 1);
  uint32_t cFill = 1 << (sps->getBitDepth(ChannelType::CHROMA) - 1);
  cFillPic->getRecoBuf().Y().fill(yFill);
  cFillPic->getRecoBuf().Cb().fill(cFill);
  cFillPic->getRecoBuf().Cr().fill(cFill);

  //  for(int ctuRsAddr=0; ctuRsAddr<cFillPic->getNumberOfCtusInFrame(); ctuRsAddr++)  { cFillPic->getCtu(ctuRsAddr)->initCtu(cFillPic, ctuRsAddr); }
  cFillPic->referenced = true;
  cFillPic->interLayerRefPicFlag = interLayerRefPicFlag;
  cFillPic->longTerm = longTermFlag;
  cFillPic->slices[0]->setPOC(iUnavailablePoc);
  cFillPic->poc = iUnavailablePoc;
  if( (cFillPic->slices[0]->getTLayer() == 0) && (cFillPic->slices[0]->getNalUnitType() != NAL_UNIT_CODED_SLICE_RASL) && (cFillPic->slices[0]->getNalUnitType() != NAL_UNIT_CODED_SLICE_RADL) )
  {
    m_prevTid0POC = cFillPic->slices[0]->getPOC();
  }

  cFillPic->reconstructed = true;
  cFillPic->neededForOutput = false;
  // picture header is not derived for generated reference picture
  cFillPic->slices[0]->setPicHeader( nullptr );
  cFillPic->temporalId = temporalId;
  cFillPic->nonReferencePictureFlag = false;
  cFillPic->slices[0]->setPPS( pps );

  if (m_pocRandomAccess == MAX_INT)
  {
    m_pocRandomAccess = iUnavailablePoc;
  }
}

void DecLib::checkPicTypeAfterEos()
{
  int layerId = m_pcPic->slices[0]->getNalUnitLayerId();
  if (m_prevEOS[layerId])
  {
    bool isIrapOrGdrPu = !m_pcPic->cs->pps->getMixedNaluTypesInPicFlag() && ( m_pcPic->slices[0]->isIRAP() || m_pcPic->slices[0]->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR );
    CHECK(!isIrapOrGdrPu, "when present, the next PU of a particular layer after an EOS NAL unit that belongs to the same layer shall be an IRAP or GDR PU");

    m_prevEOS[layerId] = false;
  }
}

void DecLib::checkLayerIdIncludedInCvss()
{
  if (m_accessUnitPicInfo.empty())
  {
    // don't try to access, if there are no entries (e.g. bitstreams ends after skipping leading pictures)
    return;
  }
  if ((getVPS()->getMaxLayers() == 1 || m_audIrapOrGdrAuFlag) && (m_isFirstAuInCvs || m_accessUnitPicInfo.begin()->m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP || m_accessUnitPicInfo.begin()->m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL))
  {
    // store layerIDs in the first AU
    m_firstAccessUnitPicInfo.assign(m_accessUnitPicInfo.begin(), m_accessUnitPicInfo.end());
  }
  else
  {
    // check whether the layerIDs in an AU are included in the layerIDs of the first AU
    for (auto pic = m_accessUnitPicInfo.begin(); pic != m_accessUnitPicInfo.end(); pic++)
    {
      bool layerIdFind;
      if ( m_firstAccessUnitPicInfo.size() == 0 )
      {
        msg( NOTICE, "Note: checkIncludedInFirstAu(), m_firstAccessUnitPicInfo.size() is 0.\n");
        continue;
      }
      for (auto picFirst = m_firstAccessUnitPicInfo.begin(); picFirst != m_firstAccessUnitPicInfo.end(); picFirst++)
      {
        layerIdFind = pic->m_nuhLayerId == picFirst->m_nuhLayerId ? true : false;
        if (layerIdFind)
        {
          break;
        }
      }
      CHECK(!layerIdFind, "each picture in an AU in a CVS shall have nuh_layer_id equal to the nuh_layer_id of one of the pictures present in the first AU of the CVS");
    }


    // check whether the layerID of EOS_NUT is included in the layerIDs of the first AU
    for (int i = 0; i < getVPS()->getMaxLayers(); i++)
    {
      int eosLayerId = getVPS()->getLayerId(i);
      if (m_accessUnitEos[eosLayerId])
      {
        bool eosLayerIdFind;
        for (auto picFirst = m_firstAccessUnitPicInfo.begin(); picFirst != m_firstAccessUnitPicInfo.end(); picFirst++)
        {
          eosLayerIdFind = eosLayerId == picFirst->m_nuhLayerId ? true : false;
          if (eosLayerIdFind)
          {
            break;
          }
        }
        CHECK(!eosLayerIdFind, "When nal_unit_type is equal to EOS_NUT, nuh_layer_id shall be equal to one of the nuh_layer_id values of the layers present in the CVS");
      }
    }
  }
}

void DecLib::resetIsFirstAuInCvs()
{
  // update the value of m_isFirstAuInCvs for the next AU according to NAL_UNIT_EOS in each layer
  for (auto pic = m_accessUnitPicInfo.begin(); pic != m_accessUnitPicInfo.end(); pic++)
  {
    m_isFirstAuInCvs = m_accessUnitEos[pic->m_nuhLayerId] ? true : false;
    if (!m_isFirstAuInCvs)
    {
      break;
    }
  }
}

void DecLib::CheckNoOutputPriorPicFlagsInAccessUnit()
{
  if (m_accessUnitNoOutputPriorPicFlags.size() > 1)
  {
    bool anchor = m_accessUnitNoOutputPriorPicFlags[0];
    bool isDiffFlagsInAu = std::find(m_accessUnitNoOutputPriorPicFlags.begin(), m_accessUnitNoOutputPriorPicFlags.end(), !anchor) != m_accessUnitNoOutputPriorPicFlags.end();
    CHECK(isDiffFlagsInAu, "The value of no_output_of_prior_pics_flag, when present, is required to be the same for all pictures in an AU");
  }
}

void DecLib::checkTidLayerIdInAccessUnit()
{
  int firstPicTid = m_accessUnitPicInfo.begin()->m_temporalId;
  int firstPicLayerId = m_accessUnitPicInfo.begin()->m_nuhLayerId;

  bool isPicTidInAuSame = true;
  bool isSeiTidInAuSameAsAuTid = true;
  bool isFdNaluLayerIdSameAsVclNaluLayerId = true;
  bool isFdTidInAuSameAsAuTid = true;

  for (auto pic = m_accessUnitPicInfo.begin(); pic != m_accessUnitPicInfo.end(); pic++)
  {
    if (pic->m_temporalId != firstPicTid)
    {
      isPicTidInAuSame = false;
      break;
    }
  }
  CHECK(!isPicTidInAuSame, "All pictures in an AU shall have the same value of TemporalId");

  for (auto tid = m_accessUnitSeiTids.begin(); tid != m_accessUnitSeiTids.end(); tid++)
  {
    if ((*tid) != firstPicTid)
    {
      isSeiTidInAuSameAsAuTid = false;
      break;
    }
  }
  CHECK(!isSeiTidInAuSameAsAuTid, "The TemporalId of an SEI NAL unit shall be equal to the TemporalId of the AU containing the NAL unit");

  for (auto tempNalu = m_accessUnitNals.begin(); tempNalu != m_accessUnitNals.end(); tempNalu++)
  {
    if ((tempNalu->m_nalUnitType == NAL_UNIT_FD) && (tempNalu->m_nuhLayerId != firstPicLayerId))
    {
      isFdNaluLayerIdSameAsVclNaluLayerId = false;
      break;
    }
  }
  CHECK(!isFdNaluLayerIdSameAsVclNaluLayerId, "The nuh_layer_id of a filler data NAL unit shall be equal to the nuh_layer_id of associated VCL NAL unit");

  for (auto tempNalu = m_accessUnitNals.begin(); tempNalu != m_accessUnitNals.end(); tempNalu++)
  {
    if ((tempNalu->m_nalUnitType == NAL_UNIT_FD) && (tempNalu->m_temporalId != firstPicTid))
    {
      isFdTidInAuSameAsAuTid = false;
      break;
    }
  }
  CHECK(!isFdTidInAuSameAsAuTid, "The TemporalId of a filler data NAL unit shall be equal to the TemporalId of the AU containing the NAL unit");
}

void DecLib::checkSEIInAccessUnit()
{
  int olsIdxIncludeAllLayes = -1;
  bool isNonNestedSliFound = false;

  bool bSdiPresentInAu = false;
  bool bAuxSEIsBeforeSdiSEIPresent[4] = { false, false, false, false };
  for (auto &sei : m_accessUnitSeiPayLoadTypes)
  {
    enum NalUnitType         naluType = std::get<0>(sei);
    enum SEI::PayloadType payloadType = std::get<2>(sei);
    if (naluType == NAL_UNIT_PREFIX_SEI
        && ((payloadType == SEI::PayloadType::BUFFERING_PERIOD || payloadType == SEI::PayloadType::PICTURE_TIMING
             || payloadType == SEI::PayloadType::DECODING_UNIT_INFO
             || payloadType == SEI::PayloadType::SUBPICTURE_LEVEL_INFO)))
    {
      bool olsIncludeAllLayersFind = false;
      for (int i = 0; i < m_vps->getNumOutputLayerSets(); i++)
      {
        for (auto pic = m_firstAccessUnitPicInfo.begin(); pic != m_firstAccessUnitPicInfo.end(); pic++)
        {
          int targetLayerId = pic->m_nuhLayerId;
          for (int j = 0; j < m_vps->getNumLayersInOls(i); j++)
          {
            olsIncludeAllLayersFind = m_vps->getLayerIdInOls(i, j) == targetLayerId ? true : false;
            if (olsIncludeAllLayersFind)
            {
              break;
            }
          }
          if (!olsIncludeAllLayersFind)
          {
            break;
          }
        }
        if (olsIncludeAllLayersFind)
        {
          olsIdxIncludeAllLayes = i;
          if (payloadType == SEI::PayloadType::SUBPICTURE_LEVEL_INFO)
          {
            isNonNestedSliFound = true;
          }
          break;
        }
      }
      CHECK(!olsIncludeAllLayersFind, "When there is no OLS that includes all layers in the current CVS in the entire bitstream, there shall be no non-scalable-nested SEI message with payloadType equal to 0 (BP), 1 (PT), 130 (DUI), or 203 (SLI)");
    }
    if (payloadType == SEI::PayloadType::SCALABILITY_DIMENSION_INFO)
    {
      bSdiPresentInAu = true;
    }
    else if (payloadType == SEI::PayloadType::MULTIVIEW_ACQUISITION_INFO && !bSdiPresentInAu)
    {
      bAuxSEIsBeforeSdiSEIPresent[0] = true;
    }
    else if (payloadType == SEI::PayloadType::ALPHA_CHANNEL_INFO && !bSdiPresentInAu)
    {
      bAuxSEIsBeforeSdiSEIPresent[1] = true;
    }
    else if (payloadType == SEI::PayloadType::DEPTH_REPRESENTATION_INFO && !bSdiPresentInAu)
    {
      bAuxSEIsBeforeSdiSEIPresent[2] = true;
    }
    else if (payloadType == SEI::PayloadType::MULTIVIEW_VIEW_POSITION && !bSdiPresentInAu)
    {
      bAuxSEIsBeforeSdiSEIPresent[3] = true;
    }
  }

  CHECK(bSdiPresentInAu && bAuxSEIsBeforeSdiSEIPresent[0], "When an AU contains both an SDI SEI message and an MAI SEI message, the SDI SEI message shall precede the MAI SEI message in decoding order.");
  CHECK(bSdiPresentInAu && bAuxSEIsBeforeSdiSEIPresent[1], "When an AU contains both an SDI SEI message with sdi_aux_id[i] equal to 1 for at least one value of i and an ACI SEI message, the SDI SEI message shall precede the ACI SEI message in decoding order.");
  CHECK(bSdiPresentInAu && bAuxSEIsBeforeSdiSEIPresent[2], "When an AU contains both an SDI SEI message with sdi_aux_id[i] equal to 2 for at least one value of i and a DRI SEI message, the SDI SEI message shall precede the DRI SEI message in decoding order.");

  if (m_isFirstAuInCvs)
  {
    // when a non-nested SLI SEI shows up, check sps_num_subpics_minus1 for the OLS contains all layers with multiple subpictures per picture
    if (isNonNestedSliFound)
    {
      checkMultiSubpicNum(olsIdxIncludeAllLayes);
    }

    // when a nested SLI SEI shows up, loop over all applicable OLSs, and for layers in the each applicable OLS, check sps_num_subpics_minus1 for these layers with multiple subpictures per picture
    for (auto sliInfo = m_accessUnitNestedSliSeiInfo.begin(); sliInfo != m_accessUnitNestedSliSeiInfo.end(); sliInfo++)
    {
      if (sliInfo->m_nestedSliPresent)
      {
        for (uint32_t olsIdxNestedSei = 0; olsIdxNestedSei < sliInfo->m_numOlssNestedSli; olsIdxNestedSei++)
        {
          int olsIdx = sliInfo->m_olsIdxNestedSLI[olsIdxNestedSei];
          checkMultiSubpicNum(olsIdx);
        }
      }
    }
  }
}

void DecLib::checkMultiSubpicNum(int olsIdx)
{
  int multiSubpicNum = 0;
  for (int layerIdx = 0; layerIdx < m_vps->getNumLayersInOls(olsIdx); layerIdx++)
  {
    uint32_t layerId = m_vps->getLayerIdInOls(olsIdx, layerIdx);
    if (m_accessUnitSpsNumSubpic[layerId] > 1)
    {
      if (multiSubpicNum == 0)
      {
        multiSubpicNum = m_accessUnitSpsNumSubpic[layerId];
      }
      CHECK(multiSubpicNum != m_accessUnitSpsNumSubpic[layerId], "When an SLI SEI message is present for a CVS, the value of sps_num_subpics_minus1 shall be the same for all the SPSs referenced by the pictures in the layers with multiple subpictures per picture.")
    }
  }
}

// Check the number of identical SEI messages in the current picture
// Section D.2.1 of VVC spec
void DecLib::checkSeiInPictureUnit()
{
  std::vector<std::pair<SEI::PayloadType, std::vector<uint8_t>>> seiList;

  bool prefixPostfilterHintSEI = false;
  bool suffixPostfilterHintSEI = false;

  // payload types subject to constrained SEI repetition
  const std::set<SEI::PayloadType> picUnitRepConSeiList = {
    SEI::PayloadType::BUFFERING_PERIOD,                             // 0,
    SEI::PayloadType::PICTURE_TIMING,                               // 1
    SEI::PayloadType::FILM_GRAIN_CHARACTERISTICS,                   // 19
    SEI::PayloadType::FRAME_PACKING,                                // 45
    SEI::PayloadType::DISPLAY_ORIENTATION,                          // 47
    SEI::PayloadType::GREEN_METADATA,                               // 56
    SEI::PayloadType::PARAMETER_SETS_INCLUSION_INDICATION,          // 129
    SEI::PayloadType::DECODED_PICTURE_HASH,                         // 132
    SEI::PayloadType::SCALABLE_NESTING,                             // 133
    SEI::PayloadType::MASTERING_DISPLAY_COLOUR_VOLUME,              // 137
    SEI::PayloadType::COLOUR_TRANSFORM_INFO,                        // 142
    SEI::PayloadType::CONTENT_LIGHT_LEVEL_INFO,                     // 144
    SEI::PayloadType::DEPENDENT_RAP_INDICATION,                     // 145
    SEI::PayloadType::ALTERNATIVE_TRANSFER_CHARACTERISTICS,         // 147
    SEI::PayloadType::AMBIENT_VIEWING_ENVIRONMENT,                  // 148
    SEI::PayloadType::CONTENT_COLOUR_VOLUME,                        // 149
    SEI::PayloadType::EQUIRECTANGULAR_PROJECTION,                   // 150
    SEI::PayloadType::GENERALIZED_CUBEMAP_PROJECTION,               // 153
    SEI::PayloadType::SPHERE_ROTATION,                              // 154
    SEI::PayloadType::REGION_WISE_PACKING,                          // 155
    SEI::PayloadType::OMNI_VIEWPORT,                                // 156
    SEI::PayloadType::FRAME_FIELD_INFO,                             // 168
    SEI::PayloadType::DEPTH_REPRESENTATION_INFO,                    // 177
    SEI::PayloadType::MULTIVIEW_ACQUISITION_INFO,                   // 179
    SEI::PayloadType::MULTIVIEW_VIEW_POSITION,                      // 180
    SEI::PayloadType::SEI_MANIFEST,                                 // 200
    SEI::PayloadType::SEI_PREFIX_INDICATION,                        // 201
    SEI::PayloadType::ANNOTATED_REGIONS,                            // 202
    SEI::PayloadType::SUBPICTURE_LEVEL_INFO,                        // 203
    SEI::PayloadType::SAMPLE_ASPECT_RATIO_INFO,                     // 204
    SEI::PayloadType::SHUTTER_INTERVAL_INFO,                        // 205
    SEI::PayloadType::EXTENDED_DRAP_INDICATION,                     // 206
    SEI::PayloadType::CONSTRAINED_RASL_ENCODING,                    // 207
    SEI::PayloadType::SCALABILITY_DIMENSION_INFO,                   // 208
    SEI::PayloadType::VDI_SEI_ENVELOPE,                             // 209
    SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_CHARACTERISTICS,   // 210
    SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_ACTIVATION,        // 211
    SEI::PayloadType::PHASE_INDICATION,                             // 212
  };

  // extract SEI messages from NAL units
  for (auto &sei : m_pictureSeiNalus)
  {
    std::pair<SEI::PayloadType, std::vector<uint8_t>> data;

    InputBitstream bs = sei->getBitstream();

    do
    {
      uint32_t val = 0;

      uint32_t payloadType = 0;
      do
      {
        bs.readByte(val);
        payloadType += val;
      } while (val == 0xFF);
      data.first = SEI::PayloadType(payloadType);

      uint32_t payloadSize = 0;
      do
      {
        bs.readByte(val);
        payloadSize += val;
      } while (val == 0xFF);

      data.second.resize(payloadSize);
      for (uint32_t i = 0; i < payloadSize; i++)
      {
        bs.readByte(val);
        data.second[i] = (uint8_t) val;
      }
      seiList.push_back(data);

      if (data.first == SEI::PayloadType::POST_FILTER_HINT)
      {
        if (sei->m_nalUnitType == NalUnitType::NAL_UNIT_PREFIX_SEI)
        {
          prefixPostfilterHintSEI = true;
        }
        else if (sei->m_nalUnitType == NalUnitType::NAL_UNIT_SUFFIX_SEI)
        {
          suffixPostfilterHintSEI = true;
        }
      }
    } while (bs.getNumBitsLeft() > 8);
  }

  // count repeated messages in list
  for (uint32_t i = 0; i < seiList.size(); i++)
  {
    const SEI::PayloadType      payloadType1 = seiList[i].first;
    const std::vector<uint8_t>& data1        = seiList[i].second;

    // only consider SEI payload types in the PicUnitRepConSeiList
    if (picUnitRepConSeiList.count(payloadType1) == 1)
    {
      // compare current SEI message with remaining messages in the list
      int count = 1;

      for (uint32_t j = i + 1; j < seiList.size(); j++)
      {
        const SEI::PayloadType      payloadType2 = seiList[j].first;
        const std::vector<uint8_t>& data2        = seiList[j].second;

        // check for identical SEI type, size, and payload
        if (payloadType1 == payloadType2 && data1 == data2)
        {
          count++;
        }
      }
      CHECK(count > 4,
            "There shall be less than or equal to 4 identical sei_payload( ) syntax structures within a picture unit.");
    }
  }

  CHECK(prefixPostfilterHintSEI && suffixPostfilterHintSEI,
        "Post-filter hint SEI message shall not be present in both a prefix SEI NALU and a suffix SEI NALU in the same "
        "picture unit")
}

/**
 - Reset list of SEI NAL units from the current picture
 */
void DecLib::resetPictureSeiNalus()
{
  while (!m_pictureSeiNalus.empty())
  {
    delete m_pictureSeiNalus.front();
    m_pictureSeiNalus.pop_front();
  }
}

/**
 - Reset list of Prefix SEI NAL units from the current picture
 */
void DecLib::resetPrefixSeiNalus()
{
  while (!m_prefixSEINALUs.empty())
  {
    delete m_prefixSEINALUs.front();
    m_prefixSEINALUs.pop_front();
  }
}

void DecLib::checkSeiContentInAccessUnit()
{
  if (m_accessUnitSeiNalus.empty())
  {
    return;
  }
  std::vector<SeiPayload> seiList;   // payloadType, olsId, isNestedSEI, payloadSize, payload, duiIdx, subPicId

  // get the OLSs that cover all layers
  std::vector<uint32_t> olsIds;
  for (uint32_t i = 0; i < m_vps->getNumOutputLayerSets(); i++)
  {
    bool olsIncludeAllLayersFind = false;
    for (auto pic = m_firstAccessUnitPicInfo.begin(); pic != m_firstAccessUnitPicInfo.end(); pic++)
    {
      int targetLayerId = pic->m_nuhLayerId;
      for (int j = 0; j < m_vps->getNumLayersInOls(i); j++)
      {
        olsIncludeAllLayersFind = m_vps->getLayerIdInOls(i, j) == targetLayerId ? true : false;
        if (olsIncludeAllLayersFind)
        {
          break;
        }
      }
      if (!olsIncludeAllLayersFind)
      {
        break;
      }
    }
    if (olsIncludeAllLayersFind)
    {
      olsIds.push_back(i);
    }
  }

  // extract SEI messages from NAL units
  for (auto &sei : m_accessUnitSeiNalus)
  {
    InputBitstream bs = sei->getBitstream();

    do
    {
      int      payloadTypeVal = 0;
      uint32_t payloadLayerId = sei->m_nuhLayerId;
      uint32_t val = 0;

      do
      {
        bs.readByte(val);
        payloadTypeVal += val;
      } while (val==0xFF);

      auto payloadType = SEI::PayloadType(payloadTypeVal);

      if (payloadType == SEI::PayloadType::USER_DATA_REGISTERED_ITU_T_T35
          || payloadType == SEI::PayloadType::USER_DATA_UNREGISTERED)
      {
        break;
      }

      uint32_t payloadSize = 0;
      do
      {
        bs.readByte(val);
        payloadSize += val;
      } while (val==0xFF);

      if (payloadType != SEI::PayloadType::SCALABLE_NESTING)
      {
        if (payloadType == SEI::PayloadType::BUFFERING_PERIOD || payloadType == SEI::PayloadType::PICTURE_TIMING
            || payloadType == SEI::PayloadType::DECODING_UNIT_INFO
            || payloadType == SEI::PayloadType::SUBPICTURE_LEVEL_INFO)
        {
          uint8_t *payload = new uint8_t[payloadSize];
          int duiIdx = 0;
          if (payloadType == SEI::PayloadType::DECODING_UNIT_INFO)
          {
            m_seiReader.getSEIDecodingUnitInfoDuiIdx(&bs, payloadLayerId, m_HRD, payloadSize, duiIdx);
          }
          for (uint32_t i = 0; i < payloadSize; i++)
          {
            bs.readByte(val);
            payload[i] = (uint8_t)val;
          }
          for (uint32_t i = 0; i < olsIds.size(); i++)
          {
            if (i == 0)
            {
              seiList.push_back(SeiPayload{ payloadType, olsIds.at(i), false, payloadSize, payload, duiIdx, 0 });
            }
            else
            {
              uint8_t *payloadTemp = new uint8_t[payloadSize];
              memcpy(payloadTemp, payload, payloadSize *sizeof(uint8_t));
              seiList.push_back(SeiPayload{ payloadType, olsIds.at(i), false, payloadSize, payloadTemp, duiIdx, 0 });
            }
          }
        }
        else
        {
          uint8_t *payload = new uint8_t[payloadSize];
          for (uint32_t i = 0; i < payloadSize; i++)
          {
            bs.readByte(val);
            payload[i] = (uint8_t)val;
          }
          seiList.push_back(SeiPayload{ payloadType, payloadLayerId, false, payloadSize, payload, 0, 0 });
        }
      }
      else
      {
        const SPS *sps = m_parameterSetManager.getActiveSPS();
        const VPS *vps = m_parameterSetManager.getVPS(sps->getVPSId());
        m_seiReader.parseAndExtractSEIScalableNesting(&bs, sei->m_nalUnitType, payloadLayerId, vps, sps, m_HRD, payloadSize, &seiList);
      }
    }
    while (bs.getNumBitsLeft() > 8);
  }

  // check contents of the repeated messages in list
  for (uint32_t i = 0; i < seiList.size(); i++)
  {
    SEI::PayloadType payloadType1    = seiList[i].payloadType;
    int              payLoadLayerId1 = seiList[i].payloadLayerId;
    bool             payLoadNested1  = seiList[i].payloadNested;
    uint32_t         payloadSize1    = seiList[i].payloadSize;
    uint8_t         *payload1        = seiList[i].payload;
    int              duiIdx1         = seiList[i].duiIdx;
    int              subPicId1       = seiList[i].subpicId;

    // compare current SEI message with remaining messages in the list
    for (uint32_t j = i+1; j < seiList.size(); j++)
    {
      SEI::PayloadType payloadType2    = seiList[j].payloadType;
      int              payLoadLayerId2 = seiList[j].payloadLayerId;
      bool             payLoadNested2  = seiList[j].payloadNested;
      uint32_t         payloadSize2    = seiList[j].payloadSize;
      uint8_t         *payload2        = seiList[j].payload;
      int              duiIdx2         = seiList[j].duiIdx;
      int              subPicId2       = seiList[j].subpicId;

      // check for identical SEI type, olsId or layerId, size, payload, duiIdx, and subPicId
      if (payloadType1 == SEI::PayloadType::BUFFERING_PERIOD || payloadType1 == SEI::PayloadType::PICTURE_TIMING
          || payloadType1 == SEI::PayloadType::DECODING_UNIT_INFO
          || payloadType1 == SEI::PayloadType::SUBPICTURE_LEVEL_INFO)
      {
        CHECK((payloadType1 == payloadType2) && (payLoadLayerId1 == payLoadLayerId2) && (duiIdx1 == duiIdx2) && (subPicId1 == subPicId2) && ((payloadSize1 != payloadSize2) || memcmp(payload1, payload2, payloadSize1*sizeof(uint8_t))), "When there are multiple SEI messages with a particular value of payloadType not equal to 133 that are associated with a particular AU or DU and apply to a particular OLS or layer, regardless of whether some or all of these SEI messages are scalable-nested, the SEI messages shall have the same SEI payload content.");
      }
      else
      if(payloadType1 != SEI::PayloadType::GENERATIVE_FACE_VIDEO
          && payloadType1 != SEI::PayloadType::GENERATIVE_FACE_VIDEO_ENHANCEMENT
          )
      {
        bool sameLayer = false;
        if (!payLoadNested1 && !payLoadNested2)
        {
          sameLayer = (payLoadLayerId1 == payLoadLayerId2);
        }
        else if (payLoadNested1 && payLoadNested2)
        {
          sameLayer = true;
        }
        else
        {
          sameLayer = payLoadNested1 ? payLoadLayerId2 >= payLoadLayerId1 : payLoadLayerId1 >= payLoadLayerId2;
        }
        CHECK(payloadType1 == payloadType2 && sameLayer && (duiIdx1 == duiIdx2) && (subPicId1 == subPicId2)  && ((payloadSize1 != payloadSize2) || memcmp(payload1, payload2, payloadSize1*sizeof(uint8_t))), "When there are multiple SEI messages with a particular value of payloadType not equal to 133 that are associated with a particular AU or DU and apply to a particular OLS or layer, regardless of whether some or all of these SEI messages are scalable-nested, the SEI messages shall have the same SEI payload content.");
      }
      if (payloadType1 == SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_ACTIVATION && payloadType2 == SEI::PayloadType::NEURAL_NETWORK_POST_FILTER_ACTIVATION)
      {
        if ((xGetNnpfaTargetId(payload1, payloadSize1) == xGetNnpfaTargetId(payload2, payloadSize2)) && (payLoadLayerId1 == payLoadLayerId2) && (duiIdx1 == duiIdx2))
        {
          CHECK(!std::equal(payload1, payload1 + payloadSize1, payload2, payload2 + payloadSize2), "When there are multiple SEI messages with payloadType equal to 211 and the same nnpfa_target_id value that are associated with a particular AU or DU and apply to a particular OLS or layer, regardless of whether some or all of these SEI messages are scalable-nested, the SEI messages shall have the same SEI payload content.");
        }
      }
    }
  }

  // free SEI message list memory
  for (uint32_t i = 0; i < seiList.size(); i++)
  {
    uint8_t *payload = seiList[i].payload;
    delete[] payload;
  }
  seiList.clear();
}

/**
 - Reset list of SEI NAL units from the current access unit
 */
void DecLib::resetAccessUnitSeiNalus()
{
  while (!m_accessUnitSeiNalus.empty())
  {
    delete m_accessUnitSeiNalus.front();
    m_accessUnitSeiNalus.pop_front();
  }
}

/**
 - Process buffered list of suffix APS NALUs
 */
void DecLib::processSuffixApsNalus()
{
  while (!m_suffixApsNalus.empty())
  {
    xDecodeAPS(*m_suffixApsNalus.front());
    delete m_suffixApsNalus.front();
    m_suffixApsNalus.pop_front();
  }
}

/**
 - Determine if the first VCL NAL unit of a picture is also the first VCL NAL of an Access Unit
 */
bool DecLib::isSliceNaluFirstInAU( bool newPicture, InputNALUnit &nalu )
{
  // can only be the start of an AU if this is the start of a new picture
  if( newPicture == false )
  {
    return false;
  }

  // should only be called for slice NALU types
  if( nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_TRAIL &&
      nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_STSA &&
      nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_RASL &&
      nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_RADL &&
      nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_IDR_W_RADL &&
      nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_IDR_N_LP &&
      nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_CRA &&
      nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_GDR )
  {
    return false;
  }


  // check for layer ID less than or equal to previous picture's layer ID
  if( nalu.m_nuhLayerId <= m_prevLayerID )
  {
    return true;
  }

  // get slice POC
  m_apcSlicePilot->setPicHeader( &m_picHeader );
  m_apcSlicePilot->initSlice();
  InputBitstream bs(nalu.getBitstream());   // create copy
  m_HLSReader.setBitstream(&bs);
  m_HLSReader.getSlicePoc( m_apcSlicePilot, &m_picHeader, &m_parameterSetManager, m_prevTid0POC );

  // check for different POC
  return (m_apcSlicePilot->getPOC() != m_prevPOC);
}

void DecLib::checkAPSInPictureUnit()
{
  bool firstVCLFound = false;
  bool suffixAPSFound = false;
  for (auto &nalu : m_pictureUnitNals)
  {
    if (NALUnit::isVclNalUnitType(nalu))
    {
      firstVCLFound = true;
      CHECK( suffixAPSFound, "When any suffix APS NAL units are present in a PU, they shall follow the last VCL unit of the PU" );
    }
    else if (nalu == NAL_UNIT_PREFIX_APS)
    {
      CHECK( firstVCLFound, "When any prefix APS NAL units are present in a PU, they shall precede the first VCL unit of the PU");
    }
    else if (nalu == NAL_UNIT_SUFFIX_APS)
    {
      suffixAPSFound = true;
    }
  }
}

void activateAPS(PicHeader* picHeader, Slice* pSlice, ParameterSetManager& parameterSetManager, APS** apss, APS*& lmcsAPS, APS*& scalingListAPS)
{
  const SPS *sps = parameterSetManager.getSPS(picHeader->getSPSId());
  //luma APSs
  if (pSlice->getAlfEnabledFlag(COMPONENT_Y))
  {
    for (int i = 0; i < pSlice->getAlfApsIdsLuma().size(); i++)
    {
      int apsId = pSlice->getAlfApsIdsLuma()[i];
      APS *aps   = parameterSetManager.getAPS(apsId, ApsType::ALF);

      if (aps)
      {
        apss[apsId] = aps;
        if (false == parameterSetManager.activateAPS(apsId, ApsType::ALF))
        {
          THROW("APS activation failed!");
        }

        CHECK( aps->getTemporalId() > pSlice->getTLayer(), "TemporalId shall be less than or equal to the TemporalId of the coded slice NAL unit" );
        //ToDO: APS NAL unit containing the APS RBSP shall have nuh_layer_id either equal to the nuh_layer_id of a coded slice NAL unit that referrs it, or equal to the nuh_layer_id of a direct dependent layer of the layer containing a coded slice NAL unit that referrs it.

        CHECK(!isChromaEnabled(sps->getChromaFormatIdc()) && aps->chromaPresentFlag,
              "When ChromaArrayType is equal to 0, the value of aps_chroma_present_flag of an ApsType::ALF shall be "
              "equal to 0");

        CHECK(((sps->getCCALFEnabledFlag() == false) && (aps->getCcAlfAPSParam().newCcAlfFilter[0] || aps->getCcAlfAPSParam().newCcAlfFilter[1])), "When sps_ccalf_enabled_flag is 0, the values of alf_cc_cb_filter_signal_flag and alf_cc_cr_filter_signal_flag shall be equal to 0");
      }
    }
  }

  if (pSlice->getAlfEnabledFlag(COMPONENT_Cb)||pSlice->getAlfEnabledFlag(COMPONENT_Cr) )
  {
    //chroma APS
    int apsId = pSlice->getAlfApsIdChroma();
    APS *aps   = parameterSetManager.getAPS(apsId, ApsType::ALF);
    if (aps)
    {
      apss[apsId] = aps;
      if (false == parameterSetManager.activateAPS(apsId, ApsType::ALF))
      {
        THROW("APS activation failed!");
      }

      CHECK( aps->getTemporalId() > pSlice->getTLayer(), "TemporalId shall be less than or equal to the TemporalId of the coded slice NAL unit" );
      //ToDO: APS NAL unit containing the APS RBSP shall have nuh_layer_id either equal to the nuh_layer_id of a coded slice NAL unit that referrs it, or equal to the nuh_layer_id of a direct dependent layer of the layer containing a coded slice NAL unit that referrs it.

      CHECK(((sps->getCCALFEnabledFlag() == false) && (aps->getCcAlfAPSParam().newCcAlfFilter[0] || aps->getCcAlfAPSParam().newCcAlfFilter[1])), "When sps_ccalf_enabled_flag is 0, the values of alf_cc_cb_filter_signal_flag and alf_cc_cr_filter_signal_flag shall be equal to 0");
    }
  }

  CcAlfFilterParam &filterParam = pSlice->m_ccAlfFilterParam;
  // cleanup before copying
  for ( int filterIdx = 0; filterIdx < MAX_NUM_CC_ALF_FILTERS; filterIdx++ )
  {
    memset( filterParam.ccAlfCoeff[COMPONENT_Cb - 1][filterIdx], 0, sizeof(filterParam.ccAlfCoeff[COMPONENT_Cb - 1][filterIdx]) );
    memset( filterParam.ccAlfCoeff[COMPONENT_Cr - 1][filterIdx], 0, sizeof(filterParam.ccAlfCoeff[COMPONENT_Cr - 1][filterIdx]) );
  }
  memset( filterParam.ccAlfFilterIdxEnabled[COMPONENT_Cb - 1], false, sizeof(filterParam.ccAlfFilterIdxEnabled[COMPONENT_Cb - 1]) );
  memset( filterParam.ccAlfFilterIdxEnabled[COMPONENT_Cr - 1], false, sizeof(filterParam.ccAlfFilterIdxEnabled[COMPONENT_Cr - 1]) );

  if(pSlice->getCcAlfCbEnabledFlag())
  {
    int apsId = pSlice->getCcAlfCbApsId();
    APS *aps   = parameterSetManager.getAPS(apsId, ApsType::ALF);
    if(aps)
    {
      apss[apsId] = aps;
      if (false == parameterSetManager.activateAPS(apsId, ApsType::ALF))
      {
        THROW("APS activation failed!");
      }

      CHECK( aps->getTemporalId() > pSlice->getTLayer(), "TemporalId shall be less than or equal to the TemporalId of the coded slice NAL unit" );
      //ToDO: APS NAL unit containing the APS RBSP shall have nuh_layer_id either equal to the nuh_layer_id of a coded slice NAL unit that referrs it, or equal to the nuh_layer_id of a direct dependent layer of the layer containing a coded slice NAL unit that referrs it.

      filterParam.ccAlfFilterCount[COMPONENT_Cb - 1] = aps->getCcAlfAPSParam().ccAlfFilterCount[COMPONENT_Cb - 1];
      for (int filterIdx=0; filterIdx < filterParam.ccAlfFilterCount[COMPONENT_Cb - 1]; filterIdx++ )
      {
        filterParam.ccAlfFilterIdxEnabled[COMPONENT_Cb - 1][filterIdx] = aps->getCcAlfAPSParam().ccAlfFilterIdxEnabled[COMPONENT_Cb - 1][filterIdx];
        memcpy(filterParam.ccAlfCoeff[COMPONENT_Cb - 1][filterIdx], aps->getCcAlfAPSParam().ccAlfCoeff[COMPONENT_Cb - 1][filterIdx], sizeof(aps->getCcAlfAPSParam().ccAlfCoeff[COMPONENT_Cb - 1][filterIdx]));
      }
    }
    else
    {
      THROW("CC ALF Cb APS not available!");
    }
  }

  if(pSlice->getCcAlfCrEnabledFlag())
  {
    int apsId = pSlice->getCcAlfCrApsId();
    APS *aps   = parameterSetManager.getAPS(apsId, ApsType::ALF);
    if(aps)
    {
      apss[apsId] = aps;
      if (false == parameterSetManager.activateAPS(apsId, ApsType::ALF))
      {
        THROW("APS activation failed!");
      }

      CHECK( aps->getTemporalId() > pSlice->getTLayer(), "TemporalId shall be less than or equal to the TemporalId of the coded slice NAL unit" );
      //ToDO: APS NAL unit containing the APS RBSP shall have nuh_layer_id either equal to the nuh_layer_id of a coded slice NAL unit that referrs it, or equal to the nuh_layer_id of a direct dependent layer of the layer containing a coded slice NAL unit that referrs it.

      filterParam.ccAlfFilterCount[COMPONENT_Cr - 1] = aps->getCcAlfAPSParam().ccAlfFilterCount[COMPONENT_Cr - 1];
      for (int filterIdx=0; filterIdx < filterParam.ccAlfFilterCount[COMPONENT_Cr - 1]; filterIdx++ )
      {
        filterParam.ccAlfFilterIdxEnabled[COMPONENT_Cr - 1][filterIdx] = aps->getCcAlfAPSParam().ccAlfFilterIdxEnabled[COMPONENT_Cr - 1][filterIdx];
        memcpy(filterParam.ccAlfCoeff[COMPONENT_Cr - 1][filterIdx], aps->getCcAlfAPSParam().ccAlfCoeff[COMPONENT_Cr - 1][filterIdx], sizeof(aps->getCcAlfAPSParam().ccAlfCoeff[COMPONENT_Cr - 1][filterIdx]));
      }
    }
    else
    {
      THROW("CC ALF Cr APS not available!");
    }
  }

  if (picHeader->getLmcsEnabledFlag() && lmcsAPS == nullptr)
  {
    lmcsAPS = parameterSetManager.getAPS(picHeader->getLmcsAPSId(), ApsType::LMCS);
    CHECK(lmcsAPS == nullptr, "No LMCS APS present");
    if (lmcsAPS)
    {
      parameterSetManager.clearAPSChangedFlag(picHeader->getLmcsAPSId(), ApsType::LMCS);
      if (false == parameterSetManager.activateAPS(picHeader->getLmcsAPSId(), ApsType::LMCS))
      {
        THROW("LMCS APS activation failed!");
      }

      CHECK(!isChromaEnabled(sps->getChromaFormatIdc()) && lmcsAPS->chromaPresentFlag,
            "When ChromaArrayType is equal to 0, the value of aps_chroma_present_flag of an ApsType::LMCS shall be "
            "equal to 0");

      CHECK(lmcsAPS->getReshaperAPSInfo().maxNbitsNeededDeltaCW - 1 < 0
              || lmcsAPS->getReshaperAPSInfo().maxNbitsNeededDeltaCW - 1 > sps->getBitDepth(ChannelType::LUMA) - 2,
            "The value of lmcs_delta_cw_prec_minus1 of an ApsType::LMCS shall be in the range of 0 to BitDepth 2, "
            "inclusive");

      CHECK( lmcsAPS->getTemporalId() > pSlice->getTLayer(), "TemporalId shall be less than or equal to the TemporalId of the coded slice NAL unit" );
      //ToDO: APS NAL unit containing the APS RBSP shall have nuh_layer_id either equal to the nuh_layer_id of a coded slice NAL unit that referrs it, or equal to the nuh_layer_id of a direct dependent layer of the layer containing a coded slice NAL unit that referrs it.
    }
  }
  picHeader->setLmcsAPS(lmcsAPS);

  if( picHeader->getExplicitScalingListEnabledFlag() && scalingListAPS == nullptr)
  {
    scalingListAPS = parameterSetManager.getAPS(picHeader->getScalingListAPSId(), ApsType::SCALING_LIST);
    CHECK( scalingListAPS == nullptr, "No SCALING LIST APS present" );
    if( scalingListAPS )
    {
      parameterSetManager.clearAPSChangedFlag(picHeader->getScalingListAPSId(), ApsType::SCALING_LIST);
      if (false == parameterSetManager.activateAPS(picHeader->getScalingListAPSId(), ApsType::SCALING_LIST))
      {
        THROW( "SCALING LIST APS activation failed!" );
      }

      CHECK((!isChromaEnabled(sps->getChromaFormatIdc()) && scalingListAPS->chromaPresentFlag)
              || (isChromaEnabled(sps->getChromaFormatIdc()) && !scalingListAPS->chromaPresentFlag),
            "The value of aps_chroma_present_flag of the APS NAL unit having aps_params_type equal to SCALING_APS and "
            "adaptation_parameter_set_id equal to ph_scaling_list_aps_id shall be equal to ChromaArrayType  = =  0 ? 0 "
            ": 1");

      CHECK( scalingListAPS->getTemporalId() > pSlice->getTLayer(), "TemporalId shall be less than or equal to the TemporalId of the coded slice NAL unit" );
      //ToDO: APS NAL unit containing the APS RBSP shall have nuh_layer_id either equal to the nuh_layer_id of a coded slice NAL unit that referrs it, or equal to the nuh_layer_id of a direct dependent layer of the layer containing a coded slice NAL unit that referrs it.
    }
  }
  picHeader->setScalingListAPS(scalingListAPS);
}

void DecLib::checkParameterSetsInclusionSEIconstraints(const InputNALUnit nalu)
{
  const PPS* pps = m_pcPic->cs->pps;
  const APS* lmcsAPS = m_pcPic->cs->lmcsAps;
  const APS* scalinglistAPS = m_pcPic->cs->scalinglistAps;
  APS** apss = m_parameterSetManager.getAPSs();

  CHECK(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA &&
        pps->getTemporalId() == nalu.m_temporalId &&
        pps->getPuCounter() > m_puCounter, "Violating Parameter Sets Inclusion Indication SEI constraint");

  for (int i = 0; i < ALF_CTB_MAX_NUM_APS; i++)
  {
    if (apss[i] != nullptr)
    {
      CHECK(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA &&
            apss[i]->getTemporalId() == nalu.m_temporalId &&
            apss[i]->getPuCounter() > m_puCounter, "Violating Parameter Sets Inclusion Indication SEI constraint");
    }
  }
  if (lmcsAPS != nullptr)
  {
    CHECK(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA &&
          lmcsAPS->getTemporalId() == nalu.m_temporalId &&
          lmcsAPS->getPuCounter() > m_puCounter, "Violating Parameter Sets Inclusion Indication SEI constraint");
  }
  if (scalinglistAPS != nullptr)
  {
    CHECK(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA &&
          scalinglistAPS->getTemporalId() == nalu.m_temporalId &&
          scalinglistAPS->getPuCounter() > m_puCounter, "Violating Parameter Sets Inclusion Indication SEI constraint");
  }
}

void DecLib::xActivateParameterSets( const InputNALUnit nalu )
{
  const int layerId = nalu.m_nuhLayerId;
  if (m_bFirstSliceInPicture)
  {
    APS** apss = m_parameterSetManager.getAPSs();
    memset(apss, 0, sizeof(*apss) * ALF_CTB_MAX_NUM_APS);
    const PPS *pps = m_parameterSetManager.getPPS(m_picHeader.getPPSId()); // this is a temporary PPS object. Do not store this value
    CHECK(pps == 0, "Referred to PPS not present");

    const SPS *sps = m_parameterSetManager.getSPS(pps->getSPSId());             // this is a temporary SPS object. Do not store this value
    CHECK(sps == 0, "Referred to SPS not present");

    const VPS *vps = m_parameterSetManager.getVPS( sps->getVPSId() );
    CHECK(vps == 0, "Referred to VPS not present");

    if( nullptr != pps->pcv )
    {
      delete m_parameterSetManager.getPPS( m_picHeader.getPPSId() )->pcv;
    }
    m_parameterSetManager.getPPS( m_picHeader.getPPSId() )->pcv = new PreCalcValues( *sps, *pps, false );
    m_parameterSetManager.clearSPSChangedFlag(sps->getSPSId());
    m_parameterSetManager.clearPPSChangedFlag(pps->getPPSId());

    if (false == m_parameterSetManager.activatePPS(m_picHeader.getPPSId(),m_apcSlicePilot->isIRAP()))
    {
      THROW("Parameter set activation failed!");
    }

    // update the stored VPS to the actually referred to VPS
    m_vps = m_parameterSetManager.getVPS(sps->getVPSId());
    if(sps->getVPSId() == 0)
    {
      //No VPS in bitstream: set defaults values of variables in VPS to the ones signalled in SPS
      m_vps->setMaxSubLayers( sps->getMaxTLayers() );
      m_vps->setLayerId( 0, sps->getLayerId() );
      m_vps->deriveOutputLayerSets();
    }
    else
    {
      //VPS in the bitstream: check that SPS and VPS signalling are compatible
      CHECK(sps->getMaxTLayers() > m_vps->getMaxSubLayers(), "The SPS signals more temporal sub-layers than allowed by the VPS");
    }

    m_parameterSetManager.getApsMap(ApsType::ALF)->clearActive();
    for (int i = 0; i < ALF_CTB_MAX_NUM_APS; i++)
    {
      APS *aps = m_parameterSetManager.getAPS(i, ApsType::ALF);
      if (aps)
      {
        m_parameterSetManager.clearAPSChangedFlag(i, ApsType::ALF);
      }
    }
    APS* lmcsAPS = nullptr;
    APS* scalinglistAPS = nullptr;
    activateAPS(&m_picHeader, m_apcSlicePilot, m_parameterSetManager, apss, lmcsAPS, scalinglistAPS);

    if (((vps != nullptr) && (vps->getVPSGeneralHrdParamsPresentFlag())) || (sps->getGeneralHrdParametersPresentFlag()))
    {
      const GeneralHrdParams *generalHrdParams = (sps->getGeneralHrdParametersPresentFlag()
          ? sps->getGeneralHrdParameters()
          : vps->getGeneralHrdParameters());
      m_HRD.setGeneralHrdParameters(*generalHrdParams);
    }

    xParsePrefixSEImessages();

#if RExt__HIGH_BIT_DEPTH_SUPPORT==0
    if (sps->getSpsRangeExtension().getExtendedPrecisionProcessingFlag() || sps->getBitDepth(ChannelType::LUMA) > 12
        || sps->getBitDepth(ChannelType::CHROMA) > 12)
    {
      THROW("High bit depth support must be enabled at compile-time in order to decode this bitstream\n");
    }
#endif

    m_apcSlicePilot->applyReferencePictureListBasedMarking(m_cListPic, m_apcSlicePilot->getRpl(REF_PIC_LIST_0),
                                                           m_apcSlicePilot->getRpl(REF_PIC_LIST_1), layerId, *pps);

    //  Get a new picture buffer. This will also set up m_pcPic, and therefore give us a SPS and PPS pointer that we can use.
    m_pcPic = xGetNewPicBuffer( *sps, *pps, m_apcSlicePilot->getTLayer(), layerId );

    m_pcPic->finalInit( vps, *sps, *pps, &m_picHeader, apss, lmcsAPS, scalinglistAPS );
#if GDR_ENABLED
    m_apcSlicePilot->setPicHeader(m_pcPic->cs->picHeader);
#endif

    m_pcPic->createGrainSynthesizer(m_firstPictureInSequence, &m_grainCharacteristic, &m_grainBuf,
                                    pps->getPicWidthInLumaSamples(), pps->getPicHeightInLumaSamples(),
                                    sps->getChromaFormatIdc(), sps->getBitDepth(ChannelType::LUMA));
    m_pcPic->createColourTransfProcessor(m_firstPictureInSequence, &m_colourTranfParams, &m_invColourTransfBuf,
                                         pps->getPicWidthInLumaSamples(), pps->getPicHeightInLumaSamples(),
                                         sps->getChromaFormatIdc(), sps->getBitDepth(ChannelType::LUMA));
#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
    SEIMessages packedRegionsInfoSEIs = getSeisByType(m_SEIs, SEI::PayloadType::PACKED_REGIONS_INFO);
    if (!packedRegionsInfoSEIs.empty())
    {
      SEIPackedRegionsInfo* sei = (SEIPackedRegionsInfo*)packedRegionsInfoSEIs.front();
      if (sei->m_layerId == layerId)
      {       
        m_priProcess.init(*sei, *sps, pps->getPicWidthInLumaSamples(), pps->getPicHeightInLumaSamples());
      }
    }
#endif
    m_firstPictureInSequence = false;
    m_pcPic->createTempBuffers( m_pcPic->cs->pps->pcv->maxCUWidth, false, false, true, false );
    m_pcPic->cs->createTemporaryCsData((bool)m_pcPic->cs->sps->getPLTMode());
    m_pcPic->cs->initStructData();

    m_pcPic->allocateNewSlice();
    // make the slice-pilot a real slice, and set up the slice-pilot for the next slice
    CHECK(m_pcPic->slices.size() != (m_uiSliceSegmentIdx + 1), "Invalid number of slices");
    m_apcSlicePilot = m_pcPic->swapSliceObject(m_apcSlicePilot, m_uiSliceSegmentIdx);

    // we now have a real slice:
    Slice *pSlice = m_pcPic->slices[m_uiSliceSegmentIdx];
#if GDR_ENABLED
    pSlice->setPicHeader(m_pcPic->cs->picHeader);
#endif

    // Update the PPS and SPS pointers with the ones of the picture.
    pps=pSlice->getPPS();
    sps=pSlice->getSPS();

    // fix Parameter Sets, now that we have the real slice
    m_pcPic->cs->slice = pSlice;
    m_pcPic->cs->sps   = sps;
    m_pcPic->cs->pps   = pps;
    m_pcPic->cs->vps = vps;

    memcpy(m_pcPic->cs->alfApss, apss, sizeof(m_pcPic->cs->alfApss));
    m_pcPic->cs->lmcsAps = lmcsAPS;
    m_pcPic->cs->scalinglistAps = scalinglistAPS;

    m_pcPic->cs->pcv   = pps->pcv;

    // Initialise the various objects for the new set of settings
    const int maxDepth = floorLog2(sps->getMaxCUWidth()) - pps->pcv->minCUWidthLog2;
    const uint32_t log2SaoOffsetScaleLuma =
      (uint32_t) std::max(0, sps->getBitDepth(ChannelType::LUMA) - MAX_SAO_TRUNCATED_BITDEPTH);
    const uint32_t log2SaoOffsetScaleChroma =
      (uint32_t) std::max(0, sps->getBitDepth(ChannelType::CHROMA) - MAX_SAO_TRUNCATED_BITDEPTH);
    m_cSAO.create( pps->getPicWidthInLumaSamples(), pps->getPicHeightInLumaSamples(),
                   sps->getChromaFormatIdc(),
                   sps->getMaxCUWidth(), sps->getMaxCUHeight(),
                   maxDepth,
                   log2SaoOffsetScaleLuma, log2SaoOffsetScaleChroma );
    m_deblockingFilter.create(maxDepth);
    m_cIntraPred.init(sps->getChromaFormatIdc(), sps->getBitDepth(ChannelType::LUMA));
    m_cInterPred.init( &m_cRdCost, sps->getChromaFormatIdc(), sps->getMaxCUHeight() );
    if (sps->getUseLmcs())
    {
      m_cReshaper.createDec(sps->getBitDepth(ChannelType::LUMA));
    }

    bool isField = false;
    bool isTopField = false;

    if(!m_SEIs.empty())
    {
      // Check if any new Frame Field Info SEI has arrived
      SEIMessages frameFieldSEIs = getSeisByType(m_SEIs, SEI::PayloadType::FRAME_FIELD_INFO);
      if (frameFieldSEIs.size()>0)
      {
        SEIFrameFieldInfo* ff = (SEIFrameFieldInfo*) *(frameFieldSEIs.begin());
        isField    = ff->m_fieldPicFlag;
        isTopField = isField && (!ff->m_bottomFieldFlag);
      }
      SEIMessages inclusionSEIs = getSeisByType(m_SEIs, SEI::PayloadType::PARAMETER_SETS_INCLUSION_INDICATION);
      const SEIParameterSetsInclusionIndication *inclusion =
        (inclusionSEIs.size() > 0) ? (SEIParameterSetsInclusionIndication *) *(inclusionSEIs.begin()) : nullptr;
      if (inclusion != nullptr)
      {
        m_seiInclusionFlag = inclusion->m_selfContainedClvsFlag;
      }
    }
    if (m_seiInclusionFlag)
    {
      checkParameterSetsInclusionSEIconstraints(nalu);
    }

    //Set Field/Frame coding mode
    m_pcPic->fieldPic = isField;
    m_pcPic->topField = isTopField;

    // transfer any SEI messages that have been received to the picture
    m_pcPic->SEIs = m_SEIs;
    m_SEIs.clear();

    // Recursive structure
    m_cCuDecoder.init( &m_cTrQuant, &m_cIntraPred, &m_cInterPred );
    if (sps->getUseLmcs())
    {
      m_cCuDecoder.initDecCuReshaper(&m_cReshaper, sps->getChromaFormatIdc());
    }
    m_cTrQuant.init(m_cTrQuantScalingList.getQuant(), sps->getMaxTbSize(), false, false, false, false);

    // RdCost
    m_cRdCost.setCostMode ( COST_STANDARD_LOSSY ); // not used in decoder side RdCost stuff -> set to default

    m_cSliceDecoder.create();

    if( sps->getALFEnabledFlag() )
    {
      const int maxDepth = floorLog2(sps->getMaxCUWidth()) - sps->getLog2MinCodingBlockSize();
      m_cALF.create(pps->getPicWidthInLumaSamples(), pps->getPicHeightInLumaSamples(), sps->getChromaFormatIdc(),
                    sps->getMaxCUWidth(), sps->getMaxCUHeight(), maxDepth, sps->getBitDepths());
    }
    pSlice->m_ccAlfFilterControl[0] = m_cALF.getCcAlfControlIdc(COMPONENT_Cb);
    pSlice->m_ccAlfFilterControl[1] = m_cALF.getCcAlfControlIdc(COMPONENT_Cr);
  }
  else
  {
    // make the slice-pilot a real slice, and set up the slice-pilot for the next slice
    m_pcPic->allocateNewSlice();
    CHECK(m_pcPic->slices.size() != (size_t)(m_uiSliceSegmentIdx + 1), "Invalid number of slices");
    m_apcSlicePilot = m_pcPic->swapSliceObject(m_apcSlicePilot, m_uiSliceSegmentIdx);

    Slice *pSlice = m_pcPic->slices[m_uiSliceSegmentIdx]; // we now have a real slice.

    const SPS *sps = pSlice->getSPS();
    const PPS *pps = pSlice->getPPS();
    APS** apss = pSlice->getAlfAPSs();
    APS *lmcsAPS = m_picHeader.getLmcsAPS();
    APS *scalinglistAPS = m_picHeader.getScalingListAPS();

    // fix Parameter Sets, now that we have the real slice
    m_pcPic->cs->slice = pSlice;
    m_pcPic->cs->sps   = sps;
    m_pcPic->cs->pps   = pps;
    memcpy(m_pcPic->cs->alfApss, apss, sizeof(m_pcPic->cs->alfApss));
    m_pcPic->cs->lmcsAps = lmcsAPS;
    m_pcPic->cs->scalinglistAps = scalinglistAPS;

    m_pcPic->cs->pcv   = pps->pcv;

    // check that the current active PPS has not changed...
    if (m_parameterSetManager.getSPSChangedFlag(sps->getSPSId()) )
    {
      EXIT("Error - a new SPS has been decoded while processing a picture");
    }
    if (m_parameterSetManager.getPPSChangedFlag(pps->getPPSId()) )
    {
      EXIT("Error - a new PPS has been decoded while processing a picture");
    }
    for (int i = 0; i < ALF_CTB_MAX_NUM_APS; i++)
    {
      APS *aps = m_parameterSetManager.getAPS(i, ApsType::ALF);
      if (aps && m_parameterSetManager.getAPSChangedFlag(i, ApsType::ALF))
      {
        EXIT("Error - a new APS has been decoded while processing a picture");
      }
    }

    if (lmcsAPS && m_parameterSetManager.getAPSChangedFlag(lmcsAPS->getAPSId(), ApsType::LMCS))
    {
      EXIT("Error - a new LMCS APS has been decoded while processing a picture");
    }
    if (scalinglistAPS && m_parameterSetManager.getAPSChangedFlag(scalinglistAPS->getAPSId(), ApsType::SCALING_LIST))
    {
      EXIT( "Error - a new SCALING LIST APS has been decoded while processing a picture" );
    }

    activateAPS(&m_picHeader, pSlice, m_parameterSetManager, apss, lmcsAPS, scalinglistAPS);

    m_pcPic->cs->lmcsAps = lmcsAPS;
    m_pcPic->cs->scalinglistAps = scalinglistAPS;

    xParsePrefixSEImessages();

    // Check if any new SEI has arrived
    if (!m_SEIs.empty())
    {
      // Currently only decoding Unit SEI message occurring between VCL NALUs copied
      SEIMessages &picSEI            = m_pcPic->SEIs;
      SEIMessages  decodingUnitInfos = extractSeisByType(picSEI, SEI::PayloadType::DECODING_UNIT_INFO);
      picSEI.insert(picSEI.end(), decodingUnitInfos.begin(), decodingUnitInfos.end());
      deleteSEIs(m_SEIs);
    }
    if (m_seiInclusionFlag)
    {
      checkParameterSetsInclusionSEIconstraints(nalu);
    }
  }
  xCheckParameterSetConstraints(layerId);
}

void DecLib::xCheckParameterSetConstraints(const int layerId)
{
  // Conformance checks
  Slice *slice = m_pcPic->slices[m_uiSliceSegmentIdx];
  const SPS *sps = slice->getSPS();
  const PPS *pps = slice->getPPS();
  const VPS *vps = slice->getVPS();

  if (sps->getVPSId() && (vps != nullptr))
  {
    bool setVpsId = true;
    int checkLayer = 0;
    while (checkLayer <= layerId)
    {
      if (!m_firstSliceInSequence[checkLayer++])
      {
        setVpsId = false;
      }
    }
    if (setVpsId)
    {
      m_clsVPSid = sps->getVPSId();
    }
    CHECK(m_clsVPSid != sps->getVPSId(), "The value of sps_video_parameter_set_id shall be the same in all SPSs that are referred to by CLVSs in a CVS.");
  }

  if (((vps!=nullptr)&&(vps->getVPSGeneralHrdParamsPresentFlag()))||(sps->getGeneralHrdParametersPresentFlag()))
  {
    if (((vps != nullptr) && (vps->getVPSGeneralHrdParamsPresentFlag())) && (sps->getGeneralHrdParametersPresentFlag()))
    {
      CHECK(!(*vps->getGeneralHrdParameters() == *sps->getGeneralHrdParameters()), "It is a requirement of bitstream conformance that the content of the general_hrd_parameters( ) syntax structure present in any VPSs or SPSs in the bitstream shall be identical");
    }
    if (!m_isFirstGeneralHrd)
    {
      CHECK(!(m_prevGeneralHrdParams == (sps->getGeneralHrdParametersPresentFlag() ? *sps->getGeneralHrdParameters() : *vps->getGeneralHrdParameters())), "It is a requirement of bitstream conformance that the content of the general_hrd_parameters( ) syntax structure present in any VPSs or SPSs in the bitstream shall be identical");
    }
    m_prevGeneralHrdParams = (sps->getGeneralHrdParametersPresentFlag() ? *sps->getGeneralHrdParameters() : *vps->getGeneralHrdParameters());
  }
  m_isFirstGeneralHrd = false;
  static std::unordered_map<int, int> m_clvssSPSid;

  if( slice->isClvssPu() && m_bFirstSliceInPicture )
  {
    m_clvssSPSid[layerId] = pps->getSPSId();
  }

  CHECK( m_clvssSPSid[layerId] != pps->getSPSId(), "The value of pps_seq_parameter_set_id shall be the same in all PPSs that are referred to by coded pictures in a CLVS" );

  CHECK(sps->getGDREnabledFlag() == false && m_picHeader.getGdrPicFlag(), "When sps_gdr_enabled_flag is equal to 0, the value of ph_gdr_pic_flag shall be equal to 0 ");
  if( !sps->getUseWP() )
  {
    CHECK( pps->getUseWP(), "When sps_weighted_pred_flag is equal to 0, the value of pps_weighted_pred_flag shall be equal to 0." );
  }

  if( !sps->getUseWPBiPred() )
  {
    CHECK( pps->getWPBiPred(), "When sps_weighted_bipred_flag is equal to 0, the value of pps_weighted_bipred_flag shall be equal to 0." );
  }

  const int minCuSize = 1 << sps->getLog2MinCodingBlockSize();
  CHECK( ( pps->getPicWidthInLumaSamples() % ( std::max( 8, minCuSize) ) ) != 0, "Coded frame width must be a multiple of Max(8, the minimum unit size)" );
  CHECK( ( pps->getPicHeightInLumaSamples() % ( std::max( 8, minCuSize) ) ) != 0, "Coded frame height must be a multiple of Max(8, the minimum unit size)" );
  if (!sps->getResChangeInClvsEnabledFlag())
  {
    CHECK(pps->getPicWidthInLumaSamples() != sps->getMaxPicWidthInLumaSamples(), "When sps_res_change_in_clvs_allowed_flag equal to 0, the value of pps_pic_width_in_luma_samples shall be equal to sps_pic_width_max_in_luma_samples.");
    CHECK(pps->getPicHeightInLumaSamples() != sps->getMaxPicHeightInLumaSamples(), "When sps_res_change_in_clvs_allowed_flag equal to 0, the value of pps_pic_height_in_luma_samples shall be equal to sps_pic_height_max_in_luma_samples.");
  }
  if (sps->getResChangeInClvsEnabledFlag())
  {
    CHECK(sps->getSubPicInfoPresentFlag() != 0, "When sps_res_change_in_clvs_allowed_flag is equal to 1, the value of sps_subpic_info_present_flag shall be equal to 0.");
  }
  CHECK(sps->getResChangeInClvsEnabledFlag() && sps->getVirtualBoundariesPresentFlag(), "when the value of sps_res_change_in_clvs_allowed_flag is equal to 1, the value of sps_virtual_boundaries_present_flag shall be equal to 0");

  if( sps->getCTUSize() + 2 * ( 1 << sps->getLog2MinCodingBlockSize() ) > pps->getPicWidthInLumaSamples() )
  {
    CHECK( pps->getWrapAroundEnabledFlag(), "Wraparound shall be disabled when the value of ( CtbSizeY / MinCbSizeY + 1) is greater than or equal to ( pps_pic_width_in_luma_samples / MinCbSizeY - 1 )" );
  }

  if( vps != nullptr && vps->m_numOutputLayersInOls[vps->m_targetOlsIdx] > 1 )
  {
    CHECK( sps->getMaxPicWidthInLumaSamples() > vps->getOlsDpbPicSize( vps->m_targetOlsIdx ).width, "sps_pic_width_max_in_luma_samples shall be less than or equal to the value of vps_ols_dpb_pic_width[ i ]" );
    CHECK( sps->getMaxPicHeightInLumaSamples() > vps->getOlsDpbPicSize( vps->m_targetOlsIdx ).height, "sps_pic_height_max_in_luma_samples shall be less than or equal to the value of vps_ols_dpb_pic_height[ i ]" );
    CHECK( sps->getChromaFormatIdc() > vps->getOlsDpbChromaFormatIdc( vps->m_targetOlsIdx ), "sps_chroma_format_idc shall be less than or equal to the value of vps_ols_dpb_chroma_format[ i ]");
    CHECK((sps->getBitDepth(ChannelType::LUMA) - 8) > vps->getOlsDpbBitDepthMinus8(vps->m_targetOlsIdx),
          "sps_bitdepth_minus8 shall be less than or equal to the value of vps_ols_dpb_bitdepth_minus8[ i ]");
  }

  static std::unordered_map<int, ChromaFormat> m_layerChromaFormat;
  static std::unordered_map<int, int> m_layerBitDepth;

  if (vps != nullptr && vps->getMaxLayers() > 1)
  {
    int curLayerIdx = vps->getGeneralLayerIdx(layerId);
    ChromaFormat curLayerChromaFormat = sps->getChromaFormatIdc();
    int curLayerBitDepth     = sps->getBitDepth(ChannelType::LUMA);

    if( slice->isClvssPu() && m_bFirstSliceInPicture )
    {
      m_layerChromaFormat[curLayerIdx] = curLayerChromaFormat;
      m_layerBitDepth[curLayerIdx] = curLayerBitDepth;
    }
    else
    {
      CHECK(m_layerChromaFormat[curLayerIdx] != curLayerChromaFormat, "Different chroma format in the same layer.");
      CHECK(m_layerBitDepth[curLayerIdx] != curLayerBitDepth, "Different bit-depth in the same layer.");
    }

    for (int i = 0; i < curLayerIdx; i++)
    {
      if (vps->getDirectRefLayerFlag(curLayerIdx, i))
      {
        ChromaFormat refLayerChromaFormat = m_layerChromaFormat[i];
        CHECK(curLayerChromaFormat != refLayerChromaFormat, "The chroma formats of the current layer and the reference layer are different");
        int refLayerBitDepth = m_layerBitDepth[i];
        CHECK(curLayerBitDepth != refLayerBitDepth, "The bit-depth of the current layer and the reference layer are different");
        if (vps->getMaxTidIlRefPicsPlus1(curLayerIdx, i) == 0 && pps->getMixedNaluTypesInPicFlag())
        {
          for (int j = 0; j < m_uiSliceSegmentIdx; j++)
          {
            Slice* preSlice = m_pcPic->slices[j];
            CHECK( (preSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL || preSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP || preSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA), "mixed IRAP and non-IRAP NAL units in the picture when sps_video_parameter_set_id is greater than 0 and vps_max_tid_il_ref_pics_plus1[i][j] is equal to 0");
          }
        }
      }
    }
  }

  if (sps->getProfileTierLevel()->getConstraintInfo()->getOneTilePerPicConstraintFlag())
  {
    CHECK(pps->getNumTiles() != 1, "When one_tile_per_pic_constraint_flag is equal to 1, each picture shall contain only one tile");
  }

  if (sps->getProfileTierLevel()->getConstraintInfo()->getOneSlicePerPicConstraintFlag())
  {
    CHECK( pps->getRectSliceFlag() && pps->getNumSlicesInPic() != 1, "When one_slice_per_pic_constraint_flag is equal to 1 and if pps_rect_slice_flag is equal to 1, the value of pps_num_slices_in_pic_minus1 shall be equal to 0");
  }

  if (sps->getProfileTierLevel()->getConstraintInfo()->getNoRprConstraintFlag())
  {
    CHECK(sps->getRprEnabledFlag(), "When gci_no_ref_pic_resampling_constraint_flag is equal to 1, the value of sps_ref_pic_resampling_enabled_flag shall be equal to 0");
  }
  if (sps->getProfileTierLevel()->getConstraintInfo()->getNoResChangeInClvsConstraintFlag())
  {
    CHECK(sps->getResChangeInClvsEnabledFlag(), "When gci_no_res_change_in_clvs_constraint_flag is equal to 1, the value of sps_res_change_in_clvs_allowed_flag shall be equal to 0");
  }

  if (sps->getProfileTierLevel()->getConstraintInfo()->getNoIdrRplConstraintFlag())
  {
    CHECK(sps->getIDRRefParamListPresent(), "When gci_no_idr_rpl_constraint_flag equal to 1 , the value of sps_idr_rpl_present_flag shall be equal to 0")
  }

  if (sps->getProfileTierLevel()->getConstraintInfo()->getNoMixedNaluTypesInPicConstraintFlag())
  {
    CHECK(pps->getMixedNaluTypesInPicFlag(), "When gci_no_mixed_nalu_types_in_pic_constraint_flag equal to 1, the value of pps_mixed_nalu_types_in_pic_flag shall be equal to 0")
  }

  if (sps->getProfileTierLevel()->getConstraintInfo()->getNoGdrConstraintFlag())
  {
    CHECK(sps->getGDREnabledFlag(), "gci_no_gdr_constraint_flag equal to 1 specifies that sps_gdr_enabled_flag for all pictures in OlsInScope shall be equal to 0");
  }

  if (sps->getProfileTierLevel()->getConstraintInfo()->getNoRectSliceConstraintFlag())
  {
    CHECK(pps->getRectSliceFlag(), "When gci_no_rectangular_slice_constraint_flag equal to 1, the value of pps_rect_slice_flag shall be equal to 0")
  }

  if (sps->getProfileTierLevel()->getConstraintInfo()->getOneSlicePerSubpicConstraintFlag())
  {
    CHECK(!(pps->getSingleSlicePerSubPicFlag()), "When gci_one_slice_per_subpic_constraint_flag equal to 1, the value of pps_single_slice_per_subpic_flag shall be equal to 1")
  }

  if (sps->getProfileTierLevel()->getConstraintInfo()->getNoSubpicInfoConstraintFlag())
  {
    CHECK(sps->getSubPicInfoPresentFlag(), "When gci_no_subpic_info_constraint_flag is equal to 1, the value of sps_subpic_info_present_flag shall be equal to 0")
  }
  if (sps->getProfileTierLevel()->getConstraintInfo()->getNoMttConstraintFlag())
  {
    CHECK((sps->getMaxMTTHierarchyDepth() || sps->getMaxMTTHierarchyDepthI() || sps->getMaxMTTHierarchyDepthIChroma()), "When gci_no_mtt_constraint_flag is equal to 1, the values of sps_max_mtt_hierarchy_depth_intra_slice_luma, sps_max_mtt_hierarchy_depth_inter_slice and sps_max_mtt_hierarchy_depth_intra_slice_chroma shall be equal to 0");
  }
  if (sps->getProfileTierLevel()->getConstraintInfo()->getNoWeightedPredictionConstraintFlag())
  {
    CHECK((sps->getUseWP() || sps->getUseWPBiPred()), "When gci_no_weighted_prediction_constraint_flag is equal to 1, the values of sps_weighted_pred_flag and sps_weighted_bipred_flag shall be equal to 0");
  }

  if (sps->getProfileTierLevel()->getConstraintInfo()->getNoChromaQpOffsetConstraintFlag())
  {
    CHECK((pps->getCuChromaQpOffsetListEnabledFlag()), "When gci_no_ChromaQpOffset_constraint_flag is equal to 1, the values of pps_cu_chroma_qp_offset_list_enabled_flag shall be equal to 0");
  }

  CHECK(sps->getCTUSize() > (1 << sps->getProfileTierLevel()->getConstraintInfo()->getMaxLog2CtuSizeConstraintIdc()), "The CTU size specified by sps_log2_ctu_size_minus5 shall not exceed the constraint specified by gci_three_minus_max_log2_ctu_size_constraint_idc");

  if (sps->getProfileTierLevel()->getConstraintInfo()->getNoLumaTransformSize64ConstraintFlag())
  {
    CHECK(sps->getLog2MaxTbSize() != 5, "When gci_no_luma_transform_size_64_constraint_flag is equal to 1, the value of sps_max_luma_transform_size_64_flag shall be equal to 0");
  }

  if (sps->getMaxPicWidthInLumaSamples() == pps->getPicWidthInLumaSamples() &&
      sps->getMaxPicHeightInLumaSamples() == pps->getPicHeightInLumaSamples())
  {
    const Window& spsConfWin = sps->getConformanceWindow();
    const Window& ppsConfWin = pps->getConformanceWindow();
    CHECK(spsConfWin.getWindowLeftOffset() != ppsConfWin.getWindowLeftOffset(), "When picture size is equal to maximum picutre size, conformance window left offset in SPS and PPS shall be equal");
    CHECK(spsConfWin.getWindowRightOffset() != ppsConfWin.getWindowRightOffset(), "When picture size is equal to maximum picutre size, conformance window right offset in SPS and PPS shall be equal");
    CHECK(spsConfWin.getWindowTopOffset() != ppsConfWin.getWindowTopOffset(), "When picture size is equal to maximum picutre size, conformance window top offset in SPS and PPS shall be equal");
    CHECK(spsConfWin.getWindowBottomOffset() != ppsConfWin.getWindowBottomOffset(), "When picture size is equal to maximum picutre size, conformance window bottom offset in SPS and PPS shall be equal");
  }
  int levelIdcSps = int(sps->getProfileTierLevel()->getLevelIdc());
  int maxLevelIdxDci = 0;
  if (m_dci)
  {
    for (int i = 0; i < m_dci->getNumPTLs(); i++)
    {
      if (maxLevelIdxDci < int(m_dci->getProfileTierLevel(i).getLevelIdc()))
      {
        maxLevelIdxDci = int(m_dci->getProfileTierLevel(i).getLevelIdc());
      }
    }
    CHECK(levelIdcSps > maxLevelIdxDci, "max level signaled in the DCI shall not be less than the level signaled in the SPS");
  }


  if( slice->getPicHeader()->getGdrOrIrapPicFlag() && !slice->getPicHeader()->getGdrPicFlag() && ( !vps || vps->getIndependentLayerFlag( vps->getGeneralLayerIdx( layerId ) ) ) )
  {
    CHECK( slice->getPicHeader()->getPicInterSliceAllowedFlag(),
      "When ph_gdr_or_irap_pic_flag is equal to 1 and ph_gdr_pic_flag is equal to 0 and vps_independent_layer_flag[ GeneralLayerIdx[ nuh_layer_id ] ] is equal to 1, ph_inter_slice_allowed_flag shall be equal to 0" );
  }

  if( sps->getVPSId() && vps->m_numLayersInOls[vps->m_targetOlsIdx] == 1 )
  {
    CHECK( !sps->getPtlDpbHrdParamsPresentFlag(), "When sps_video_parameter_set_id is greater than 0 and there is an OLS that contains only one layer with nuh_layer_id equal to the nuh_layer_id of the SPS, the value of sps_ptl_dpb_hrd_params_present_flag shall be equal to 1" );
  }

  const ProfileTierLevel &ptl = vps && vps->getNumLayersInOls(vps->m_targetOlsIdx) > 1
      ? vps->getProfileTierLevel(vps->getOlsPtlIdx(vps->m_targetOlsIdx))
      : *sps->getProfileTierLevel();

  ProfileTierLevelFeatures ptlFeatures;
  ptlFeatures.extractPTLInformation(ptl);
  const ProfileFeatures *profileFeatures = ptlFeatures.getProfileFeatures();
  if (profileFeatures != nullptr)
  {
    CHECK(sps->getBitDepth(ChannelType::LUMA) > profileFeatures->maxBitDepth, "Bit depth exceeds profile limit");
    CHECK(sps->getChromaFormatIdc() > profileFeatures->maxChromaFormat, "Chroma format exceeds profile limit");
  }
  else
  {
    CHECK(sps->getProfileTierLevel()->getProfileIdc() != Profile::NONE, "Unknown profile");
    msg(WARNING, "Warning: Profile set to none or unknown value\n");
  }
  const TierLevelFeatures *tierLevelFeatures = ptlFeatures.getTierLevelFeatures();
  if (tierLevelFeatures != nullptr)
  {
    CHECK(pps->getNumTileColumns() > tierLevelFeatures->maxTileCols,
          "Number of tile columns signaled in PPS exceeds level limit");
    CHECK(pps->getNumTiles() > tierLevelFeatures->maxTilesPerAu, "Number of tiles signaled in PPS exceeds level limit");
  }
  else if (profileFeatures != nullptr)
  {
    CHECK(sps->getProfileTierLevel()->getLevelIdc() == Level::LEVEL15_5, "Cannot use level 15.5 with given profile");
    CHECK(sps->getProfileTierLevel()->getLevelIdc() != Level::NONE, "Unknown level");
    msg(WARNING, "Warning: Level set to none, invalid or unknown value\n");
  }
}


void DecLib::xParsePrefixSEIsForUnknownVCLNal()
{
  while (!m_prefixSEINALUs.empty())
  {
    // do nothing?
    msg( NOTICE, "Discarding Prefix SEI associated with unknown VCL NAL unit.\n");
    delete m_prefixSEINALUs.front();
    m_prefixSEINALUs.pop_front();
  }
  // TODO: discard following suffix SEIs as well?
}


void DecLib::xParsePrefixSEImessages()
{
  while (!m_prefixSEINALUs.empty())
  {
    InputNALUnit &nalu=*m_prefixSEINALUs.front();
    m_accessUnitSeiNalus.push_back(new InputNALUnit(nalu));
    m_accessUnitSeiTids.push_back(nalu.m_temporalId);
    const SPS *sps = m_parameterSetManager.getActiveSPS();
    const VPS *vps = m_parameterSetManager.getVPS(sps->getVPSId());
    SEIMessages::iterator newSEI = m_seiReader.parseSEImessage( &(nalu.getBitstream()), m_SEIs, nalu.m_nalUnitType, nalu.m_nuhLayerId, nalu.m_temporalId, vps, sps, m_HRD, m_pDecodedSEIOutputStream );
#if JVET_S0257_DUMP_360SEI_MESSAGE
    m_seiCfgDump.write360SeiDump( m_decoded360SeiDumpFileName, m_SEIs, sps );
#endif
    for (; newSEI != m_SEIs.end(); newSEI++)
    {
      m_accessUnitSeiPayLoadTypes.push_back(std::tuple<NalUnitType, int, SEI::PayloadType>(nalu.m_nalUnitType, nalu.m_nuhLayerId, (*newSEI)->payloadType()));
    }
    delete m_prefixSEINALUs.front();
    m_prefixSEINALUs.pop_front();
  }
  xCheckPrefixSEIMessages(m_SEIs);

  SEIMessages snList = getSeisByType(m_SEIs, SEI::PayloadType::SCALABLE_NESTING);
  if (!snList.empty())
  {
    auto sn = reinterpret_cast<SEIScalableNesting*>(snList.front());

    SEIMessages sliList = getSeisByType(sn->nestedSeis, SEI::PayloadType::SUBPICTURE_LEVEL_INFO);
    if (!sliList.empty())
    {
      AccessUnitNestedSliSeiInfo sliSeiInfo;
      sliSeiInfo.m_nestedSliPresent = true;
      sliSeiInfo.m_numOlssNestedSli = (uint32_t) sn->olsIdx.size();
      for (size_t i = 0; i < sn->olsIdx.size(); i++)
      {
        sliSeiInfo.m_olsIdxNestedSLI[i] = sn->olsIdx[i];
      }
      m_accessUnitNestedSliSeiInfo.push_back(sliSeiInfo);
    }
  }
  xCheckDUISEIMessages(m_SEIs);
}

void DecLib::xCheckPrefixSEIMessages( SEIMessages& prefixSEIs )
{
  SEIMessages picTimingSEIs  = getSeisByType(prefixSEIs, SEI::PayloadType::PICTURE_TIMING);
  SEIMessages frameFieldSEIs = getSeisByType(prefixSEIs, SEI::PayloadType::FRAME_FIELD_INFO);

  if (!picTimingSEIs.empty() && !frameFieldSEIs.empty())
  {
    auto               pt = (SEIPictureTiming*) picTimingSEIs.front();
    SEIFrameFieldInfo *ff = (SEIFrameFieldInfo*) frameFieldSEIs.front();
    if (pt->displayElementalPeriods != ff->m_displayElementalPeriodsMinus1 + 1)
    {
      msg( WARNING, "Warning: ffi_display_elemental_periods_minus1 is different in picture timing and frame field information SEI messages!");
    }
  }
  if ((getVPS()->getMaxLayers() == 1 || m_audIrapOrGdrAuFlag) && (m_isFirstAuInCvs || m_accessUnitPicInfo.begin()->m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP || m_accessUnitPicInfo.begin()->m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL || ((m_accessUnitPicInfo.begin()->m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA || m_accessUnitPicInfo.begin()->m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR) && m_lastNoOutputBeforeRecoveryFlag[m_accessUnitPicInfo.begin()->m_nuhLayerId])) && m_accessUnitPicInfo.size() == 1)
  {
    if (m_sdiSEIInFirstAU != nullptr)
    {
      delete m_sdiSEIInFirstAU;
    }
    m_sdiSEIInFirstAU = nullptr;
    if (m_maiSEIInFirstAU != nullptr)
    {
      delete m_maiSEIInFirstAU;
    }
    m_maiSEIInFirstAU = nullptr;
    if (m_mvpSEIInFirstAU != nullptr)
    {
      delete m_mvpSEIInFirstAU;
    }
    m_mvpSEIInFirstAU    = nullptr;
    SEIMessages sdiSEIs  = getSeisByType(prefixSEIs, SEI::PayloadType::SCALABILITY_DIMENSION_INFO);
    if (!sdiSEIs.empty())
    {
      SEIScalabilityDimensionInfo *sdi = (SEIScalabilityDimensionInfo*)sdiSEIs.front();
      m_sdiSEIInFirstAU = new SEIScalabilityDimensionInfo(*sdi);
      if (sdiSEIs.size() > 1)
      {
        for (SEIMessages::const_iterator it=sdiSEIs.begin(); it!=sdiSEIs.end(); it++)
        {
          CHECK(!m_sdiSEIInFirstAU->isSDISameContent((SEIScalabilityDimensionInfo*)*it), "All SDI SEI messages in a CVS shall have the same content.")
        }
      }
    }
    SEIMessages maiSEIs = getSeisByType(prefixSEIs, SEI::PayloadType::MULTIVIEW_ACQUISITION_INFO);
    if (!maiSEIs.empty())
    {
      SEIMultiviewAcquisitionInfo *mai = (SEIMultiviewAcquisitionInfo*)maiSEIs.front();
      m_maiSEIInFirstAU = new SEIMultiviewAcquisitionInfo(*mai);
      if (maiSEIs.size() > 1)
      {
        for (SEIMessages::const_iterator it=maiSEIs.begin(); it!=maiSEIs.end(); it++)
        {
          CHECK(!m_maiSEIInFirstAU->isMAISameContent((SEIMultiviewAcquisitionInfo*)*it), "All MAI SEI messages in a CVS shall have the same content.")
        }
      }
    }
    SEIMessages mvpSEIs = getSeisByType(prefixSEIs, SEI::PayloadType::MULTIVIEW_VIEW_POSITION);
    if (!mvpSEIs.empty())
    {
      SEIMultiviewViewPosition *mvp = (SEIMultiviewViewPosition*)mvpSEIs.front();
      m_mvpSEIInFirstAU = new SEIMultiviewViewPosition(*mvp);
      if (mvpSEIs.size() > 1)
      {
        for (SEIMessages::const_iterator it = mvpSEIs.begin(); it != mvpSEIs.end(); it++)
        {
          CHECK(!m_mvpSEIInFirstAU->isMVPSameContent((SEIMultiviewViewPosition*)*it), "All MVP SEI messages in a CVS shall have the same content.")
        }
      }
    }
  }
  else
  {
    SEIMessages sdiSEIs = getSeisByType(prefixSEIs, SEI::PayloadType::SCALABILITY_DIMENSION_INFO);
    CHECK(!m_sdiSEIInFirstAU && !sdiSEIs.empty(), "When an SDI SEI message is present in any AU of a CVS, an SDI SEI message shall be present for the first AU of the CVS.");
    if (!sdiSEIs.empty())
    {
      for (SEIMessages::const_iterator it=sdiSEIs.begin(); it!=sdiSEIs.end(); it++)
      {
        CHECK(!m_sdiSEIInFirstAU->isSDISameContent((SEIScalabilityDimensionInfo*)*it), "All SDI SEI messages in a CVS shall have the same content.")
      }
    }
    SEIMessages maiSEIs = getSeisByType(prefixSEIs, SEI::PayloadType::MULTIVIEW_ACQUISITION_INFO);
    CHECK(!m_maiSEIInFirstAU && !maiSEIs.empty(), "When an MAI SEI message is present in any AU of a CVS, an MAI SEI message shall be present for the first AU of the CVS.");
    if (!maiSEIs.empty())
    {
      for (SEIMessages::const_iterator it=maiSEIs.begin(); it!=maiSEIs.end(); it++)
      {
        CHECK(!m_maiSEIInFirstAU->isMAISameContent((SEIMultiviewAcquisitionInfo*)*it), "All MAI SEI messages in a CVS shall have the same content.")
      }
    }
    SEIMessages mvpSEIs = getSeisByType(prefixSEIs, SEI::PayloadType::MULTIVIEW_VIEW_POSITION);
    CHECK(!m_mvpSEIInFirstAU && !mvpSEIs.empty(), "When an MVP SEI message is present in any AU of a CVS, an MVP SEI message shall be present for the first AU of the CVS.");
    if (!mvpSEIs.empty())
    {
      for (SEIMessages::const_iterator it = mvpSEIs.begin(); it != mvpSEIs.end(); it++)
      {
        CHECK(!m_mvpSEIInFirstAU->isMVPSameContent((SEIMultiviewViewPosition*)*it), "All MVP SEI messages in a CVS shall have the same content.")
      }
    }
  }

  for (SEIMessages::const_iterator it=prefixSEIs.begin(); it!=prefixSEIs.end(); it++)
  {
    if ((*it)->payloadType() == SEI::PayloadType::MULTIVIEW_ACQUISITION_INFO)
    {
      CHECK(!m_sdiSEIInFirstAU, "When a CVS does not contain an SDI SEI message, the CVS shall not contain an MAI SEI message.");
      SEIMultiviewAcquisitionInfo *maiSei = (SEIMultiviewAcquisitionInfo*)*it;
      CHECK(m_sdiSEIInFirstAU->m_sdiNumViews - 1 != maiSei->m_maiNumViewsMinus1, "The value of num_views_minus1 shall be equal to NumViews - 1");
    }
    else if ((*it)->payloadType() == SEI::PayloadType::ALPHA_CHANNEL_INFO)
    {
      CHECK(!m_sdiSEIInFirstAU, "When a CVS does not contain an SDI SEI message with sdi_aux_id[i] equal to 1 for at least one value of i, no picture in the CVS shall be associated with an ACI SEI message.");
    }
    else if ((*it)->payloadType() == SEI::PayloadType::DEPTH_REPRESENTATION_INFO)
    {
      CHECK(!m_sdiSEIInFirstAU, "When a CVS does not contain an SDI SEI message with sdi_aux_id[i] equal to 2 for at least one value of i, no picture in the CVS shall be associated with a DRI SEI message.");
    }
    else if ((*it)->payloadType() == SEI::PayloadType::MULTIVIEW_VIEW_POSITION)
    {
      CHECK(!m_sdiSEIInFirstAU, "When a CVS does not contain an SDI SEI message, the CVS shall not contain an MVP SEI message.");
      SEIMultiviewViewPosition *mvpSei = (SEIMultiviewViewPosition*)*it;
      CHECK(m_sdiSEIInFirstAU->m_sdiNumViews - 1 != mvpSei->m_mvpNumViewsMinus1, "The value of num_views_minus1 shall be equal to NumViews - 1");
    }
  }
}

void DecLib::xCheckDUISEIMessages(SEIMessages &prefixSEIs)
{
  SEIMessages BPSEIs  = getSeisByType(prefixSEIs, SEI::PayloadType::BUFFERING_PERIOD);
  SEIMessages DUISEIs = getSeisByType(prefixSEIs, SEI::PayloadType::DECODING_UNIT_INFO);
  if (BPSEIs.empty())
  {
    return;
  }
  else
  {
    bool duDelayFlag = false;

    auto* bp = reinterpret_cast<SEIBufferingPeriod*>(BPSEIs.front());
    if (bp->hasDuHrdParams)
    {
      if (!bp->duDpbParamsInPicTimingSei)
      {
        if (DUISEIs.empty())
        {
          return;
        }
        for (auto it = DUISEIs.cbegin(); it != DUISEIs.cend(); ++it)
        {
          const SEIDecodingUnitInfo *dui = (const SEIDecodingUnitInfo *) *it;
          if (dui->dpbOutputDuDelay != -1)
          {
            duDelayFlag = true;
            break;
          }
        }
        CHECK(duDelayFlag == false, "At least one DUI SEI should have dui->dpbOutputDuDelay not equal to -1")
      }
    }
  }
}


void DecLib::xDecodePicHeader( InputNALUnit& nalu )
{
  m_HLSReader.setBitstream( &nalu.getBitstream() );
  m_HLSReader.parsePictureHeader( &m_picHeader, &m_parameterSetManager, true );
  m_picHeader.setValid();
}

bool DecLib::getMixedNaluTypesInPicFlag()
{
  if (!m_picHeader.isValid())
  {
    return false;
  }

  PPS *pps = m_parameterSetManager.getPPS(m_picHeader.getPPSId());
  CHECK(pps == 0, "No PPS present");

  return pps->getMixedNaluTypesInPicFlag();
}

bool DecLib::xDecodeSlice(InputNALUnit &nalu, int &iSkipFrame, int iPOCLastDisplay )
{
  m_apcSlicePilot->setPicHeader( &m_picHeader );
  m_apcSlicePilot->initSlice(); // the slice pilot is an object to prepare for a new slice
                                // it is not associated with picture, sps or pps structures.

  Picture* scaledRefPic[MAX_NUM_REF] = {};

  if (m_bFirstSliceInPicture)
  {
    m_uiSliceSegmentIdx = 0;
  }
  else
  {
    CHECK(nalu.m_nalUnitType != m_pcPic->slices[m_uiSliceSegmentIdx - 1]->getNalUnitType() && !m_pcPic->cs->pps->getMixedNaluTypesInPicFlag(), "If pps_mixed_nalu_types_in_pic_flag is equal to 0, the value of NAL unit type shall be the same for all coded slice NAL units of a picture");
    m_apcSlicePilot->copySliceInfo( m_pcPic->slices[m_uiSliceSegmentIdx-1] );
  }

  m_apcSlicePilot->setNalUnitType(nalu.m_nalUnitType);
  m_apcSlicePilot->setNalUnitLayerId(nalu.m_nuhLayerId);
  m_apcSlicePilot->setTLayer(nalu.m_temporalId);

  for( auto& naluTemporalId : m_accessUnitNals )
  {
    if (
      naluTemporalId.m_nalUnitType != NAL_UNIT_OPI &&
      naluTemporalId.m_nalUnitType != NAL_UNIT_DCI
      && naluTemporalId.m_nalUnitType != NAL_UNIT_VPS
      && naluTemporalId.m_nalUnitType != NAL_UNIT_SPS
      && naluTemporalId.m_nalUnitType != NAL_UNIT_EOS
      && naluTemporalId.m_nalUnitType != NAL_UNIT_EOB)

    {
      CHECK( naluTemporalId.m_temporalId < nalu.m_temporalId, "TemporalId shall be greater than or equal to the TemporalId of the layer access unit containing the NAL unit" );
    }
  }

  if (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR)
  {
    CHECK(nalu.m_temporalId != 0, "Current GDR picture has TemporalId not equal to 0");
  }

  m_HLSReader.setBitstream( &nalu.getBitstream() );
  m_apcSlicePilot->m_ccAlfFilterParam = m_cALF.getCcAlfFilterParam();
  m_HLSReader.parseSliceHeader( m_apcSlicePilot, &m_picHeader, &m_parameterSetManager, m_prevTid0POC, m_prevPicPOC );

  if (m_picHeader.getGdrOrIrapPicFlag() && m_bFirstSliceInPicture)
  {
    m_accessUnitNoOutputPriorPicFlags.push_back(m_apcSlicePilot->getNoOutputOfPriorPicsFlag());
  }

  if (m_picHeader.getGdrPicFlag() && m_prevGDRInSameLayerPOC[nalu.m_nuhLayerId] == -MAX_INT ) // Only care about recovery POC if it is the first coded GDR picture in the layer
  {
    m_prevGDRInSameLayerRecoveryPOC[nalu.m_nuhLayerId] = m_apcSlicePilot->getPOC() + m_picHeader.getRecoveryPocCnt();
  }

  PPS *pps = m_parameterSetManager.getPPS(m_picHeader.getPPSId());
  CHECK(pps == 0, "No PPS present");
  SPS *sps = m_parameterSetManager.getSPS(pps->getSPSId());
  CHECK(sps == 0, "No SPS present");
  VPS *vps = m_parameterSetManager.getVPS(sps->getVPSId());


  if (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA && vps != nullptr && (vps->getIndependentLayerFlag(vps->getGeneralLayerIdx(nalu.m_nuhLayerId)) == 1))
  {
    CHECK(nalu.m_temporalId == 0, "TemporalID of STSA picture shall not be zero in independent layers");
  }

  int currSubPicIdx = pps->getSubPicIdxFromSubPicId( m_apcSlicePilot->getSliceSubPicId() );
  int currSliceAddr = m_apcSlicePilot->getSliceID();
  for(int sp = 0; sp < currSubPicIdx; sp++)
  {
    currSliceAddr -= pps->getSubPic(sp).getNumSlicesInSubPic();
  }
  CHECK( currSubPicIdx < m_maxDecSubPicIdx, "Error in the order of coded slice NAL units of subpictures" );
  CHECK( currSubPicIdx == m_maxDecSubPicIdx && currSliceAddr <= m_maxDecSliceAddrInSubPic, "Error in the order of coded slice NAL units within a subpicture" );
  if( currSubPicIdx == m_maxDecSubPicIdx )
  {
    m_maxDecSliceAddrInSubPic = currSliceAddr;
  }
  if( currSubPicIdx > m_maxDecSubPicIdx )
  {
    m_maxDecSubPicIdx = currSubPicIdx;
    m_maxDecSliceAddrInSubPic = currSliceAddr;
  }
  if ((sps->getVPSId() == 0) && (m_prevLayerID != MAX_INT))
  {
    CHECK(m_prevLayerID != nalu.m_nuhLayerId, "All VCL NAL unit in the CVS shall have the same value of nuh_layer_id "
                                              "when sps_video_parameter_set_id is equal to 0");
  }
  CHECK((sps->getVPSId() > 0) && (vps == 0), "Invalid VPS");

  const ProfileTierLevel &profileTierLevel = (vps == nullptr || vps->getNumLayersInOls(vps->m_targetOlsIdx) == 1)
    ? *sps->getProfileTierLevel()
    : vps->getProfileTierLevel(vps->getOlsPtlIdx(vps->m_targetOlsIdx));

  if ((profileTierLevel.getMultiLayerEnabledFlag() == 0) && (m_prevLayerID != MAX_INT))
  {
    CHECK(m_prevLayerID != nalu.m_nuhLayerId, "All slices in OlsInScope shall have the same value of nuh_layer_id when ptl_multilayer_enabled_flag is equal to 0" );
  }

  if( vps != nullptr && !vps->getIndependentLayerFlag( vps->getGeneralLayerIdx( nalu.m_nuhLayerId ) ) )
  {
    bool pocIsSet = false;
    for(auto auNALit=m_accessUnitPicInfo.begin(); auNALit != m_accessUnitPicInfo.end();auNALit++)
    {
      for (int refIdx = 0; refIdx < m_apcSlicePilot->getNumRefIdx(REF_PIC_LIST_0) && !pocIsSet; refIdx++)
      {
        if (m_apcSlicePilot->getRefPic(REF_PIC_LIST_0, refIdx)
            && m_apcSlicePilot->getRefPic(REF_PIC_LIST_0, refIdx)->getPOC() == (*auNALit).m_POC)
        {
          m_apcSlicePilot->setPOC(m_apcSlicePilot->getRefPic(REF_PIC_LIST_0, refIdx)->getPOC());
          pocIsSet = true;
        }
      }
      for (int refIdx = 0; refIdx < m_apcSlicePilot->getNumRefIdx(REF_PIC_LIST_1) && !pocIsSet; refIdx++)
      {
        if (m_apcSlicePilot->getRefPic(REF_PIC_LIST_1, refIdx)
            && m_apcSlicePilot->getRefPic(REF_PIC_LIST_1, refIdx)->getPOC() == (*auNALit).m_POC)
        {
          m_apcSlicePilot->setPOC(m_apcSlicePilot->getRefPic(REF_PIC_LIST_1, refIdx)->getPOC());
          pocIsSet = true;
        }
      }
    }
  }

  // update independent slice index
  uint32_t uiIndependentSliceIdx = 0;
  if (!m_bFirstSliceInPicture)
  {
    uiIndependentSliceIdx = m_pcPic->slices[m_uiSliceSegmentIdx-1]->getIndependentSliceIdx();
    uiIndependentSliceIdx++;
  }
  m_apcSlicePilot->setIndependentSliceIdx(uiIndependentSliceIdx);

#if K0149_BLOCK_STATISTICS
  writeBlockStatisticsHeader(sps);
#endif

  DTRACE_UPDATE( g_trace_ctx, std::make_pair( "poc", m_apcSlicePilot->getPOC() ) );


  xUpdatePreviousTid0POC(m_apcSlicePilot);

  m_apcSlicePilot->setPrevGDRInSameLayerPOC(m_prevGDRInSameLayerPOC[nalu.m_nuhLayerId]);
  m_apcSlicePilot->setAssociatedIRAPPOC(m_pocCRA[nalu.m_nuhLayerId]);
  m_apcSlicePilot->setAssociatedIRAPType(m_associatedIRAPType[nalu.m_nuhLayerId]);

  if( m_apcSlicePilot->getRapPicFlag() || m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR )
  {
    // Derive NoOutputBeforeRecoveryFlag
    if( !pps->getMixedNaluTypesInPicFlag() )
    {
      if( m_firstSliceInSequence[nalu.m_nuhLayerId] )
      {
        m_picHeader.setNoOutputBeforeRecoveryFlag( true );
      }
      else if( m_apcSlicePilot->getIdrPicFlag() )
      {
        m_picHeader.setNoOutputBeforeRecoveryFlag( true );
      }
      else if( m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA )
      {
        m_picHeader.setNoOutputBeforeRecoveryFlag( m_picHeader.getHandleCraAsCvsStartFlag() );
      }
      else if( m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR )
      {
        m_picHeader.setNoOutputBeforeRecoveryFlag( m_picHeader.getHandleGdrAsCvsStartFlag() );
      }
    }
    else
    {
      m_picHeader.setNoOutputBeforeRecoveryFlag( false );
    }

    if (m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA || m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR)
    {
      m_lastNoOutputBeforeRecoveryFlag[nalu.m_nuhLayerId] = m_picHeader.getNoOutputBeforeRecoveryFlag();
    }

    if (m_apcSlicePilot->getNoOutputOfPriorPicsFlag())
    {
      m_lastPOCNoOutputPriorPics = m_apcSlicePilot->getPOC();
      m_isNoOutputPriorPics = true;
    }
    else
    {
      m_isNoOutputPriorPics = false;
    }
  }

  if (m_bFirstSliceInPicture && m_apcSlicePilot->getPOC() != m_prevPOC
      && (m_apcSlicePilot->getRapPicFlag() || m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR)
      && m_picHeader.getNoOutputBeforeRecoveryFlag()
      && getNoOutputPriorPicsFlag())
  {
    checkNoOutputPriorPics(&m_cListPic);
    setNoOutputPriorPicsFlag(false);
  }

  //For inference of PicOutputFlag
  if( !pps->getMixedNaluTypesInPicFlag() && ( m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL ) )
  {
    if( m_lastNoOutputBeforeRecoveryFlag[nalu.m_nuhLayerId] )
    {
      m_picHeader.setPicOutputFlag(false);
    }
  }

  {
    PPS *pps = m_parameterSetManager.getPPS(m_picHeader.getPPSId());
    CHECK(pps == 0, "No PPS present");
    SPS *sps = m_parameterSetManager.getSPS(pps->getSPSId());
    CHECK(sps == 0, "No SPS present");
    if (sps->getVPSId() > 0)
    {
      VPS *vps = m_parameterSetManager.getVPS(sps->getVPSId());
      CHECK(vps == 0, "No VPS present");
      bool isCurLayerNotOutput = true;
      for (int i = 0; i < vps->getNumLayersInOls(vps->m_targetOlsIdx); i++)
      {
        if( vps->getLayerIdInOls(vps->m_targetOlsIdx, i) == nalu.m_nuhLayerId )
        {
          isCurLayerNotOutput = false;
          break;
        }
      }

      if(isCurLayerNotOutput)
      {
        m_picHeader.setPicOutputFlag(false);
      }
    }
  }

  //Reset POC MSB when CRA or GDR has NoOutputBeforeRecoveryFlag equal to 1
  if (!pps->getMixedNaluTypesInPicFlag() && (m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA || m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR) && m_lastNoOutputBeforeRecoveryFlag[nalu.m_nuhLayerId])
  {
    int maxPocLsb = 1 << sps->getBitsForPOC();
    m_apcSlicePilot->setPOC(m_apcSlicePilot->getPOC() & (maxPocLsb - 1));
    m_lastPOCNoOutputPriorPics = m_apcSlicePilot->getPOC();
    xUpdatePreviousTid0POC(m_apcSlicePilot);
  }

  AccessUnitPicInfo picInfo;
  picInfo.m_nalUnitType = nalu.m_nalUnitType;
  picInfo.m_nuhLayerId  = nalu.m_nuhLayerId;
  picInfo.m_temporalId  = nalu.m_temporalId;
  picInfo.m_POC         = m_apcSlicePilot->getPOC();
  m_accessUnitPicInfo.push_back(picInfo);

  // Skip pictures due to random access

  if (isRandomAccessSkipPicture(iSkipFrame, iPOCLastDisplay, pps->getMixedNaluTypesInPicFlag(), nalu.m_nuhLayerId))
  {
    m_prevSliceSkipped = true;
    m_skippedPOC = m_apcSlicePilot->getPOC();
    m_skippedLayerID = nalu.m_nuhLayerId;

    // reset variables for bitstream conformance tests
    resetAccessUnitNals();
    resetAccessUnitApsNals();
    resetAccessUnitPicInfo();
    resetPictureUnitNals();
    resetPrefixSeiNalus();
    m_maxDecSubPicIdx = 0;
    m_maxDecSliceAddrInSubPic = -1;
    return false;
  }
  // Skip TFD pictures associated with BLA/BLANT pictures

  // clear previous slice skipped flag
  m_prevSliceSkipped = false;

  //we should only get a different poc for a new picture (with CTU address==0)
  if (m_apcSlicePilot->getPOC() != m_prevPOC && !m_firstSliceInSequence[nalu.m_nuhLayerId] && (m_apcSlicePilot->getFirstCtuRsAddrInSlice() != 0))
  {
    msg( WARNING, "Warning, the first slice of a picture might have been lost!\n");
  }
  m_prevLayerID = nalu.m_nuhLayerId;

  // leave when a new picture is found
  if(m_apcSlicePilot->getFirstCtuRsAddrInSlice() == 0 && !m_bFirstSliceInPicture)
  {
    if (m_prevPOC >= m_pocRandomAccess)
    {
      DTRACE_UPDATE( g_trace_ctx, std::make_pair( "final", 0 ) );
      m_prevPOC = m_apcSlicePilot->getPOC();
      return true;
    }
    m_prevPOC = m_apcSlicePilot->getPOC();
  }
  else
  {
    DTRACE_UPDATE( g_trace_ctx, std::make_pair( "final", 1 ) );
  }

  //detect lost reference picture and insert copy of earlier frame.
  {
    int lostPoc;
    int refPicIndex;
    for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
    {
      const ReferencePictureList *rpl = m_apcSlicePilot->getRpl(l);

      while ((lostPoc = m_apcSlicePilot->checkThatAllRefPicsAreAvailable(m_cListPic, rpl, 0, true, &refPicIndex,
                                                                         m_apcSlicePilot->getNumRefIdx(l)))
             > 0)
      {
        if (!pps->getMixedNaluTypesInPicFlag()
            && ((m_apcSlicePilot->isIDRorBLA() && (sps->getIDRRefParamListPresent() || pps->getRplInfoInPhFlag()))
                || ((m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR
                     || m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA)
                    && m_picHeader.getNoOutputBeforeRecoveryFlag())))
        {
          if (!rpl->isInterLayerRefPic(refPicIndex))
          {
            xCreateUnavailablePicture(pps, lostPoc, rpl->isRefPicLongterm(refPicIndex), m_apcSlicePilot->getTLayer(),
                                      m_apcSlicePilot->getNalUnitLayerId(), rpl->isInterLayerRefPic(refPicIndex));
          }
        }
        else
        {
          xCreateLostPicture(lostPoc - 1, m_apcSlicePilot->getPic()->layerId);
        }
      }
    }
  }

  m_prevPOC = m_apcSlicePilot->getPOC();

  if (m_bFirstSliceInPicture)
  {
    xUpdateRasInit(m_apcSlicePilot);
  }

  // actual decoding starts here
  xActivateParameterSets( nalu );
#if JVET_AJ0151_DSC_SEI
  SEIMessages dscInitSEIs = getSeisByType( m_pcPic->SEIs, SEI::PayloadType::DIGITALLY_SIGNED_CONTENT_INITIALIZATION);
  if (!dscInitSEIs.empty())
  {
    if (dscInitSEIs.size()>1)
    {
      printf ("Warming: received more than one Digitally Signed Content Initialization SEI message at a time. Using first only.\n");
    }
    SEIDigitallySignedContentInitialization* dsci = (SEIDigitallySignedContentInitialization*) dscInitSEIs.front();
    m_dscSubstreamManager.initDscSubstreamManager(dsci->dsciNumVerificationSubstreams, dsci->dsciHashMethodType, dsci->dsciKeySourceUri,
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
                                                  dsci->dsciContentUuidPresentFlag, dsci->dsciContentUuid, dsci->dsciRefSubstreamFlag, dsci->dsciVSSImplicitAssociationModeFlag);
#else
                                                  dsci->dsciContentUuidPresentFlag, dsci->dsciContentUuid, dsci->dsciRefSubstreamFlag);
#endif
#else
                                                  dsci->dsciContentUuidPresentFlag, dsci->dsciContentUuid);
#endif
    if (!m_dscSubstreamManager.initVerificator(m_keyStoreDir, m_trustStoreDir))
    {
      printf("Error: Cannot initialize Digitally Signed Content verification\n");
    }
  }
  SEIMessages dscSelectionSEIs = getSeisByType(m_pcPic->SEIs, SEI::PayloadType::DIGITALLY_SIGNED_CONTENT_SELECTION);
  if (!dscSelectionSEIs.empty())
  {
    if (dscSelectionSEIs.size()>1)
    {
      printf ("Warming: received more than one Digitally Signed Content Selection SEI message at a time. Using first only.\n");
    }
    SEIDigitallySignedContentSelection* dscs = (SEIDigitallySignedContentSelection*) dscSelectionSEIs.front();
    xProcessStoredNALUnitsForSignature(dscs->dscsVerificationSubstreamId);
  }
  else
  {
    // process as substream 0, when no selection SEI is received
    // todo: multiples slices
#if !JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
    xProcessStoredNALUnitsForSignature(0);
#else
    if(m_dscSubstreamManager.getDscAssociationModeFlag())
    {
      int32_t dscsVSSID = vps->getMaxLayers()*nalu.m_nuhLayerId + nalu.m_temporalId;
      xProcessStoredNALUnitsForSignature(dscsVSSID);
    }
    else
    {
      xProcessStoredNALUnitsForSignature(0);
    }
#endif
  }
#endif

  m_firstSliceInSequence[nalu.m_nuhLayerId] = false;
  m_firstSliceInBitstream  = false;

  Slice* pcSlice = m_pcPic->slices[m_uiSliceSegmentIdx];
  m_pcPic->numSlices = m_uiSliceSegmentIdx + 1;
  pcSlice->setPic( m_pcPic );
  m_pcPic->poc         = pcSlice->getPOC();
  m_pcPic->referenced  = true;
  m_pcPic->temporalId  = nalu.m_temporalId;
  m_pcPic->layerId     = nalu.m_nuhLayerId;
  m_pcPic->subLayerNonReferencePictureDueToSTSA = false;

  if (pcSlice->getSPS()->getSpsRangeExtension().getRrcRiceExtensionEnableFlag())
  {
    int bitDepth  = pcSlice->getSPS()->getBitDepth(ChannelType::LUMA);
    int baseLevel = (bitDepth > 12) ? (pcSlice->isIntra() ? 5 : 2 * 5 ) : (pcSlice->isIntra() ? 2 * 5 : 3 * 5);
    pcSlice->setRiceBaseLevel(baseLevel);
  }
  else
  {
    pcSlice->setRiceBaseLevel(4);
  }

  if (pcSlice->getSPS()->getProfileTierLevel()->getConstraintInfo()->getNoApsConstraintFlag())
  {
    bool flag = pcSlice->getSPS()->getCCALFEnabledFlag() || pcSlice->getPicHeader()->getNumAlfApsIdsLuma() || pcSlice->getPicHeader()->getAlfEnabledFlag(COMPONENT_Cb) || pcSlice->getPicHeader()->getAlfEnabledFlag(COMPONENT_Cr);
    CHECK(flag, "When no_aps_constraint_flag is equal to 1, the values of ph_num_alf_aps_ids_luma, sh_num_alf_aps_ids_luma, ph_alf_cb_flag, ph_alf_cr_flag, sh_alf_cb_flag, sh_alf_cr_flag, and sps_ccalf_enabled_flag shall all be equal to 0")
  }
  if( pcSlice->getNalUnitLayerId() != pcSlice->getSPS()->getLayerId() )
  {
    CHECK( pcSlice->getSPS()->getLayerId() > pcSlice->getNalUnitLayerId(), "Layer Id of SPS cannot be greater than layer Id of VCL NAL unit the refer to it" );
    CHECK( pcSlice->getSPS()->getVPSId() == 0, "VPSId of the referred SPS cannot be 0 when layer Id of SPS and layer Id of current slice are different" );
    for (int i = 0; i < pcSlice->getVPS()->getNumOutputLayerSets(); i++ )
    {
      bool isCurrLayerInOls = false;
      bool isRefLayerInOls = false;
      int j = pcSlice->getVPS()->getNumLayersInOls(i) - 1;
      for (; j >= 0; j--)
      {
        if( pcSlice->getVPS()->getLayerIdInOls(i, j) == pcSlice->getNalUnitLayerId() )
        {
          isCurrLayerInOls = true;
        }
        if( pcSlice->getVPS()->getLayerIdInOls(i, j) == pcSlice->getSPS()->getLayerId() )
        {
          isRefLayerInOls = true;
        }
      }
      CHECK( isCurrLayerInOls && !isRefLayerInOls, "When VCL NAl unit in layer A refers to SPS in layer B, all OLS that contains layer A shall also contains layer B" );
    }
  }
  if( pcSlice->getNalUnitLayerId() != pcSlice->getPPS()->getLayerId() )
  {
    CHECK( pcSlice->getPPS()->getLayerId() > pcSlice->getNalUnitLayerId(), "Layer Id of PPS cannot be greater than layer Id of VCL NAL unit the refer to it" );
    CHECK( pcSlice->getSPS()->getVPSId() == 0, "VPSId of the referred SPS cannot be 0 when layer Id of PPS and layer Id of current slice are different" );
    for (int i = 0; i < pcSlice->getVPS()->getNumOutputLayerSets(); i++ )
    {
      bool isCurrLayerInOls = false;
      bool isRefLayerInOls = false;
      int j = pcSlice->getVPS()->getNumLayersInOls(i) - 1;
      for (; j >= 0; j--)
      {
        if( pcSlice->getVPS()->getLayerIdInOls(i, j) == pcSlice->getNalUnitLayerId() )
        {
          isCurrLayerInOls = true;
        }
        if( pcSlice->getVPS()->getLayerIdInOls(i, j) == pcSlice->getPPS()->getLayerId() )
        {
          isRefLayerInOls = true;
        }
      }
      CHECK( isCurrLayerInOls && !isRefLayerInOls, "When VCL NAl unit in layer A refers to PPS in layer B, all OLS that contains layer A shall also contains layer B" );
    }
  }

  if (m_bFirstSliceInPicture)
  {
    m_pcPic->setDecodingOrderNumber(m_decodingOrderCounter);
    m_decodingOrderCounter++;
    m_pcPic->setPictureType(nalu.m_nalUnitType);
    checkPicTypeAfterEos();
    // store sub-picture numbers, sizes, and locations with a picture
    pcSlice->getPic()->subPictures.clear();

    for( int subPicIdx = 0; subPicIdx < sps->getNumSubPics(); subPicIdx++ )
    {
      pcSlice->getPic()->subPictures.push_back( pps->getSubPic( subPicIdx ) );
    }
    pcSlice->getPic()->numSlices = pps->getNumSlicesInPic();
    pcSlice->getPic()->sliceSubpicIdx.clear();
#if GDR_ENABLED
    const int curPoc = pcSlice->getPOC();
    const PicHeader *picHeader = pcSlice->getPicHeader();

    if (picHeader->getGdrPicFlag())
    {
      setLastGdrPoc(curPoc);
      setLastGdrRecoveryPocCnt(pcSlice->getPicHeader()->getRecoveryPocCnt());
    }

    const int recoveryPocCnt = getLastGdrRecoveryPocCnt();

    pcSlice->getPic()->gdrParam.inGdrInterval = (getLastGdrPoc() > 0 && (getLastGdrPoc() <= curPoc) && (curPoc < (getLastGdrPoc() + recoveryPocCnt)));
  #endif

  #if GDR_DEC_TRACE
    printf("-gdr_pic_flag:%d\n", picHeader->getGdrPicFlag() ? 1 : 0);
    printf("-recovery_poc_cnt:%d\n", picHeader->getRecoveryPocCnt());
  #if GDR_ENABLED
    printf("-inGdrInterval:%d\n", pcSlice->getPic()->gdrParam.inGdrInterval);
  #endif

    printf("-lmcs_enable : %d\n", picHeader->getLmcsEnabledFlag() ? 1 : 0);
    printf("-lmcs_chroma : %d\n", picHeader->getLmcsChromaResidualScaleFlag() ? 1 : 0);
#endif

#if JVET_AL0056_EOI_SEI_QUANT_CONSTRAINT
    SEIMessages eoiSEIs = getSeisByType(m_pcPic->SEIs, SEI::PayloadType::ENCODER_OPTIMIZATION_INFO);
    if (!eoiSEIs.empty())
    {
      SEIEncoderOptimizationInfo *sei = dynamic_cast<SEIEncoderOptimizationInfo*>(eoiSEIs.front());
      if (!sei->m_cancelFlag && (sei->m_type & EOI_OptimizationType::OBJECT_BASED_OPTIMIZATION) && (sei->m_objectBasedIdc & EOI_OBJECT_BASED::COARSER_QUANTIZATION))
      {
        if (sei->m_picQuantObjectFlag)
        {
          CHECK(sei->m_quantThresholdDelta < 1 || sei->m_quantThresholdDelta > (63 - pcSlice->getSliceQp()), "When eoi_pic_quant_object_flag is equal to 1, the value of eoi_quant_threshold_delta shall be in the range of 1 to 63 - PicQuant, inclusive");
        }
        else
        {
          CHECK(sei->m_quantThresholdDelta < 1 || sei->m_quantThresholdDelta > (pcSlice->getSPS()->getQpBDOffset(ChannelType::LUMA) + pcSlice->getSliceQp()), "When eoi_pic_quant_object_flag is equal to 0, the value of eoi_quant_threshold_delta shall be in the range of 1 to QpBdOffset + PicQuant, inclusive");
        }
      }
    }
#endif
  }
  pcSlice->getPic()->sliceSubpicIdx.push_back(pps->getSubPicIdxFromSubPicId(pcSlice->getSliceSubPicId()));
  pcSlice->checkCRA(pcSlice->getRpl(REF_PIC_LIST_0), pcSlice->getRpl(REF_PIC_LIST_1), m_pocCRA[nalu.m_nuhLayerId],
                    m_checkCRAFlags[nalu.m_nuhLayerId], m_cListPic);
  pcSlice->constructRefPicList(m_cListPic);
  pcSlice->setPrevGDRSubpicPOC(m_prevGDRSubpicPOC[nalu.m_nuhLayerId][currSubPicIdx]);
  pcSlice->setPrevIRAPSubpicPOC(m_prevIRAPSubpicPOC[nalu.m_nuhLayerId][currSubPicIdx]);
  pcSlice->setPrevIRAPSubpicType(m_prevIRAPSubpicType[nalu.m_nuhLayerId][currSubPicIdx]);
  pcSlice->checkSubpicTypeConstraints(m_cListPic, pcSlice->getRpl(REF_PIC_LIST_0), pcSlice->getRpl(REF_PIC_LIST_1),
                                      m_prevIRAPSubpicDecOrderNo[nalu.m_nuhLayerId][currSubPicIdx]);
  pcSlice->checkRPL(pcSlice->getRpl(REF_PIC_LIST_0), pcSlice->getRpl(REF_PIC_LIST_1),
                    m_associatedIRAPDecodingOrderNumber[nalu.m_nuhLayerId], m_cListPic);
  pcSlice->checkSTSA(m_cListPic);
  if (m_pcPic->cs->vps && !m_pcPic->cs->vps->getIndependentLayerFlag(m_pcPic->cs->vps->getGeneralLayerIdx(nalu.m_nuhLayerId)) && m_pcPic->cs->pps->getNumSubPics() > 1)
  {
    CU::checkConformanceILRP(pcSlice);
  }

  pcSlice->scaleRefPicList( scaledRefPic, m_pcPic->cs->picHeader, m_parameterSetManager.getAPSs(), m_picHeader.getLmcsAPS(), m_picHeader.getScalingListAPS(), true );

  if (!pcSlice->isIntra())
  {
    bool lowDelay = true;
    int  currPoc  = pcSlice->getPOC();
    int  refIdx   = 0;

    for (refIdx = 0; refIdx < pcSlice->getNumRefIdx(REF_PIC_LIST_0) && lowDelay; refIdx++)
    {
      if (pcSlice->getRefPic(REF_PIC_LIST_0, refIdx)->getPOC() > currPoc)
      {
        lowDelay = false;
      }
    }
    if (pcSlice->isInterB())
    {
      for (refIdx = 0; refIdx < pcSlice->getNumRefIdx(REF_PIC_LIST_1) && lowDelay; refIdx++)
      {
        if (pcSlice->getRefPic(REF_PIC_LIST_1, refIdx)->getPOC() > currPoc)
        {
          lowDelay = false;
        }
      }
    }

    pcSlice->setCheckLDC(lowDelay);
  }

  if (pcSlice->getSPS()->getUseSMVD() && pcSlice->getCheckLDC() == false
      && pcSlice->getPicHeader()->getMvdL1ZeroFlag() == false)
  {
    int currPOC = pcSlice->getPOC();

    int forwardPOC  = currPOC;
    int backwardPOC = currPOC;
    int ref         = 0;
    int refIdx0     = -1;
    int refIdx1     = -1;

    // search nearest forward POC in List 0
    for (ref = 0; ref < pcSlice->getNumRefIdx(REF_PIC_LIST_0); ref++)
    {
      int        poc           = pcSlice->getRefPic(REF_PIC_LIST_0, ref)->getPOC();
      const bool isRefLongTerm = pcSlice->getRefPic(REF_PIC_LIST_0, ref)->longTerm;
      if (poc < currPOC && (poc > forwardPOC || refIdx0 == -1) && !isRefLongTerm)
      {
        forwardPOC = poc;
        refIdx0    = ref;
      }
    }

    // search nearest backward POC in List 1
    for (ref = 0; ref < pcSlice->getNumRefIdx(REF_PIC_LIST_1); ref++)
    {
      int        poc           = pcSlice->getRefPic(REF_PIC_LIST_1, ref)->getPOC();
      const bool isRefLongTerm = pcSlice->getRefPic(REF_PIC_LIST_1, ref)->longTerm;
      if (poc > currPOC && (poc < backwardPOC || refIdx1 == -1) && !isRefLongTerm)
      {
        backwardPOC = poc;
        refIdx1     = ref;
      }
    }

    if (!(forwardPOC < currPOC && backwardPOC > currPOC))
    {
      forwardPOC  = currPOC;
      backwardPOC = currPOC;
      refIdx0     = -1;
      refIdx1     = -1;

      // search nearest backward POC in List 0
      for ( ref = 0; ref < pcSlice->getNumRefIdx( REF_PIC_LIST_0 ); ref++ )
      {
        int poc = pcSlice->getRefPic( REF_PIC_LIST_0, ref )->getPOC();
        const bool isRefLongTerm = pcSlice->getRefPic(REF_PIC_LIST_0, ref)->longTerm;
        if (poc > currPOC && (poc < backwardPOC || refIdx0 == -1) && !isRefLongTerm)
        {
          backwardPOC = poc;
          refIdx0 = ref;
        }
      }

      // search nearest forward POC in List 1
      for ( ref = 0; ref < pcSlice->getNumRefIdx( REF_PIC_LIST_1 ); ref++ )
      {
        int poc = pcSlice->getRefPic( REF_PIC_LIST_1, ref )->getPOC();
        const bool isRefLongTerm = pcSlice->getRefPic(REF_PIC_LIST_1, ref)->longTerm;
        if (poc < currPOC && (poc > forwardPOC || refIdx1 == -1) && !isRefLongTerm)
        {
          forwardPOC = poc;
          refIdx1 = ref;
        }
      }
    }

    if (forwardPOC < currPOC && backwardPOC > currPOC)
    {
      pcSlice->setBiDirPred(true, refIdx0, refIdx1);
    }
    else
    {
      pcSlice->setBiDirPred( false, -1, -1 );
    }
  }
  else
  {
    pcSlice->setBiDirPred(false, -1, -1);
  }

  //---------------
  pcSlice->setRefPOCList();

  NalUnitInfo naluInfo;
  naluInfo.m_nalUnitType     = nalu.m_nalUnitType;
  naluInfo.m_nuhLayerId      = nalu.m_nuhLayerId;
  naluInfo.m_firstCTUinSlice = pcSlice->getFirstCtuRsAddrInSlice();
  naluInfo.m_POC             = pcSlice->getPOC();
  xCheckMixedNalUnit(pcSlice, sps, nalu);
  m_nalUnitInfo[naluInfo.m_nuhLayerId].push_back(naluInfo);
  SEIMessages drapSEIs = getSeisByType(m_pcPic->SEIs, SEI::PayloadType::DEPENDENT_RAP_INDICATION);
  if (!drapSEIs.empty())
  {
    msg(NOTICE, "Dependent RAP indication SEI decoded\n");
    m_latestDRAPPOC = pcSlice->getPOC();
    pcSlice->setDRAP(true);
  }
  pcSlice->setLatestDRAPPOC(m_latestDRAPPOC);
  pcSlice->checkConformanceForDRAP(nalu.m_temporalId);
  if (pcSlice->isIntra())
  {
    pcSlice->getPic()->setEdrapRapId(0);
  }
  SEIMessages edrapSEIs = getSeisByType(m_pcPic->SEIs, SEI::PayloadType::EXTENDED_DRAP_INDICATION);
  if (!edrapSEIs.empty())
  {
    msg(NOTICE, "Extended DRAP indication SEI decoded\n");
    SEIExtendedDrapIndication *seiEdrap = (SEIExtendedDrapIndication *) edrapSEIs.front();
    pcSlice->setEdrapRapId(seiEdrap->m_edrapIndicationRapIdMinus1 + 1);
    pcSlice->getPic()->setEdrapRapId(seiEdrap->m_edrapIndicationRapIdMinus1 + 1);
    pcSlice->setEdrapNumRefRapPics(seiEdrap->m_edrapIndicationNumRefRapPicsMinus1 + 1);
    for (int i = 0; i < pcSlice->getEdrapNumRefRapPics(); i++)
    {
      pcSlice->addEdrapRefRapIds(seiEdrap->m_edrapIndicationRefRapId[i]);
    }
    m_latestEDRAPIndicationLeadingPicturesDecodableFlag = seiEdrap->m_edrapIndicationLeadingPicturesDecodableFlag;
    m_latestEDRAPPOC = pcSlice->getPOC();
  }
  pcSlice->setLatestEDRAPPOC(m_latestEDRAPPOC);
  pcSlice->setLatestEdrapLeadingPicDecodableFlag(m_latestEDRAPIndicationLeadingPicturesDecodableFlag);
  pcSlice->checkConformanceForEDRAP(nalu.m_temporalId);

  Quant *quant = m_cTrQuant.getQuant();

  if (pcSlice->getExplicitScalingListUsed())
  {
    APS* scalingListAPS = pcSlice->getPicHeader()->getScalingListAPS();
    if( pcSlice->getNalUnitLayerId() != scalingListAPS->getLayerId() )
    {
      CHECK( scalingListAPS->getLayerId() > pcSlice->getNalUnitLayerId(), "Layer Id of APS cannot be greater than layer Id of VCL NAL unit the refer to it" );
      CHECK( pcSlice->getSPS()->getVPSId() == 0, "VPSId of the referred SPS cannot be 0 when layer Id of APS and layer Id of current slice are different" );
      for (int i = 0; i < pcSlice->getVPS()->getNumOutputLayerSets(); i++ )
      {
        bool isCurrLayerInOls = false;
        bool isRefLayerInOls = false;
        for( int j = pcSlice->getVPS()->getNumLayersInOls(i) - 1; j >= 0; j-- )
        {
          if( pcSlice->getVPS()->getLayerIdInOls(i, j) == pcSlice->getNalUnitLayerId() )
          {
            isCurrLayerInOls = true;
          }
          if( pcSlice->getVPS()->getLayerIdInOls(i, j) == scalingListAPS->getLayerId() )
          {
            isRefLayerInOls = true;
          }
        }
        CHECK( isCurrLayerInOls && !isRefLayerInOls, "When VCL NAl unit in layer A refers to APS in layer B, all OLS that contains layer A shall also contains layer B" );
      }
    }
    ScalingList scalingList = scalingListAPS->getScalingList();
    quant->setScalingListDec(scalingList);
    quant->setUseScalingList(true);
  }
  else
  {
    quant->setUseScalingList( false );
  }

  if (pcSlice->getSPS()->getUseLmcs())
  {
    if (m_bFirstSliceInPicture)
    {
      m_sliceLmcsApsId = -1;
    }
    if (pcSlice->getLmcsEnabledFlag())
    {
      APS* lmcsAPS = pcSlice->getPicHeader()->getLmcsAPS();
      if (m_sliceLmcsApsId == -1)
      {
        m_sliceLmcsApsId = lmcsAPS->getAPSId();
      }
      else
      {
        CHECK(lmcsAPS->getAPSId() != m_sliceLmcsApsId, "same APS ID shall be used for all slices in one picture");
      }
      if( pcSlice->getNalUnitLayerId() != lmcsAPS->getLayerId() )
      {
        CHECK( lmcsAPS->getLayerId() > pcSlice->getNalUnitLayerId(), "Layer Id of APS cannot be greater than layer Id of VCL NAL unit the refer to it" );
        CHECK( pcSlice->getSPS()->getVPSId() == 0, "VPSId of the referred SPS cannot be 0 when layer Id of APS and layer Id of current slice are different" );
        for (int i = 0; i < pcSlice->getVPS()->getNumOutputLayerSets(); i++ )
        {
          bool isCurrLayerInOls = false;
          bool isRefLayerInOls = false;
          for( int j = pcSlice->getVPS()->getNumLayersInOls(i) - 1; j >= 0; j-- )
          {
            if( pcSlice->getVPS()->getLayerIdInOls(i, j) == pcSlice->getNalUnitLayerId() )
            {
              isCurrLayerInOls = true;
            }
            if( pcSlice->getVPS()->getLayerIdInOls(i, j) == lmcsAPS->getLayerId() )
            {
              isRefLayerInOls = true;
            }
          }
          CHECK( isCurrLayerInOls && !isRefLayerInOls, "When VCL NAl unit in layer A refers to APS in layer B, all OLS that contains layer A shall also contains layer B" );
        }
      }
      SliceReshapeInfo& sInfo = lmcsAPS->getReshaperAPSInfo();
      SliceReshapeInfo& tInfo = m_cReshaper.getSliceReshaperInfo();
      tInfo.reshaperModelMaxBinIdx = sInfo.reshaperModelMaxBinIdx;
      tInfo.reshaperModelMinBinIdx = sInfo.reshaperModelMinBinIdx;
      memcpy(tInfo.reshaperModelBinCWDelta, sInfo.reshaperModelBinCWDelta, sizeof(int)*(PIC_CODE_CW_BINS));
      tInfo.maxNbitsNeededDeltaCW = sInfo.maxNbitsNeededDeltaCW;
      tInfo.chrResScalingOffset = sInfo.chrResScalingOffset;
      tInfo.setUseSliceReshaper(pcSlice->getLmcsEnabledFlag());
      tInfo.setSliceReshapeChromaAdj(pcSlice->getPicHeader()->getLmcsChromaResidualScaleFlag());
      tInfo.setSliceReshapeModelPresentFlag(true);
    }
    else
    {
      SliceReshapeInfo& tInfo = m_cReshaper.getSliceReshaperInfo();
      tInfo.setUseSliceReshaper(false);
      tInfo.setSliceReshapeChromaAdj(false);
      tInfo.setSliceReshapeModelPresentFlag(false);
    }
    if (pcSlice->getLmcsEnabledFlag())
    {
      m_cReshaper.constructReshaper();
    }
    else
    {
      m_cReshaper.setReshapeFlag(false);
    }
    if ((pcSlice->getSliceType() == I_SLICE) && m_cReshaper.getSliceReshaperInfo().getUseSliceReshaper())
    {
      m_cReshaper.setCTUFlag(false);
      m_cReshaper.setRecReshaped(true);
    }
    else
    {
      if (m_cReshaper.getSliceReshaperInfo().getUseSliceReshaper())
      {
        m_cReshaper.setCTUFlag(true);
        m_cReshaper.setRecReshaped(true);
      }
      else
      {
        m_cReshaper.setCTUFlag(false);
        m_cReshaper.setRecReshaped(false);
      }
    }
    m_cReshaper.setVPDULoc(-1, -1);
  }
  else
  {
    m_cReshaper.setCTUFlag(false);
    m_cReshaper.setRecReshaped(false);
  }

#if GDR_LEAK_TEST
  if (m_gdrPocRandomAccess == pcSlice->getPOC())
  {
    for (int e = 0; e < 2; e++)
    {
      for (int ridx = 0; ridx < pcSlice->getNumRefIdx((RefPicList)e); ridx++)
      {
        Picture *pic = pcSlice->getRefPic((RefPicList)e, ridx);
        if (pic)
        {
          CodingStructure& cs = *pic->cs;
          cs.getRecoBuf().Y().fill(0 * 4); // for 8-bit sequence
          cs.getRecoBuf().Cb().fill(0 * 4);
          cs.getRecoBuf().Cr().fill(0 * 4);
          cs.getMotionBuf().memset(0);    // clear MV storage
        }
      }
    }
  }
#endif // GDR_LEAK_TEST
#if GREEN_METADATA_SEI_ENABLED
  pcSlice->setFeatureCounter(this->m_featureCounter);
#endif
  //  Decode a picture
  m_cSliceDecoder.decompressSlice( pcSlice, &( nalu.getBitstream() ), ( m_pcPic->poc == getDebugPOC() ? getDebugCTU() : -1 ) );
#if GREEN_METADATA_SEI_ENABLED
  this->m_featureCounter = pcSlice->getFeatureCounter();
#endif
  
  m_bFirstSliceInPicture = false;
  m_uiSliceSegmentIdx++;

  pcSlice->freeScaledRefPicList( scaledRefPic );

  return false;
}

void DecLib::updatePrevGDRInSameLayer()
{
  const NalUnitType pictureType = m_pcPic->getPictureType();

  if (pictureType == NAL_UNIT_CODED_SLICE_GDR && !m_pcPic->cs->pps->getMixedNaluTypesInPicFlag())
  {
    m_prevGDRInSameLayerPOC[m_pcPic->layerId] = m_pcPic->getPOC();
  }
}

void DecLib::updateAssociatedIRAP()
{
  const NalUnitType pictureType = m_pcPic->getPictureType();

  if ((pictureType == NAL_UNIT_CODED_SLICE_IDR_W_RADL || pictureType == NAL_UNIT_CODED_SLICE_IDR_N_LP || pictureType == NAL_UNIT_CODED_SLICE_CRA) && !m_pcPic->cs->pps->getMixedNaluTypesInPicFlag())
  {
    m_associatedIRAPDecodingOrderNumber[m_pcPic->layerId] = m_pcPic->getDecodingOrderNumber();
    m_pocCRA[m_pcPic->layerId] = m_pcPic->getPOC();
    m_checkCRAFlags[m_pcPic->layerId].clear();
    m_associatedIRAPType[m_pcPic->layerId] = pictureType;
  }
}

void DecLib::updatePrevIRAPAndGDRSubpic()
{
  for (int j = 0; j < m_uiSliceSegmentIdx; j++)
  {
    Slice* pcSlice = m_pcPic->slices[j];
    const int subpicIdx = pcSlice->getPPS()->getSubPicIdxFromSubPicId(pcSlice->getSliceSubPicId());
    if (pcSlice->getCtuAddrInSlice(0) == m_pcPic->cs->pps->getSubPic(subpicIdx).getFirstCTUInSubPic())
    {
      const NalUnitType subpicType = pcSlice->getNalUnitType();
      if (subpicType == NAL_UNIT_CODED_SLICE_IDR_W_RADL || subpicType == NAL_UNIT_CODED_SLICE_IDR_N_LP || subpicType == NAL_UNIT_CODED_SLICE_CRA)
      {
        m_prevIRAPSubpicPOC[m_pcPic->layerId][subpicIdx] = m_pcPic->getPOC();
        m_prevIRAPSubpicType[m_pcPic->layerId][subpicIdx] = subpicType;
        m_prevIRAPSubpicDecOrderNo[m_pcPic->layerId][subpicIdx] = m_pcPic->getDecodingOrderNumber();
      }
      else if (subpicType == NAL_UNIT_CODED_SLICE_GDR)
      {
        m_prevGDRSubpicPOC[m_pcPic->layerId][subpicIdx] = m_pcPic->getPOC();
      }
    }
  }
}

void DecLib::xDecodeOPI( InputNALUnit& nalu )
{
  m_opi = new OPI();
  m_HLSReader.setBitstream( &nalu.getBitstream() );

  CHECK( nalu.m_temporalId, "The value of TemporalId of OPI NAL units shall be equal to 0" );

  m_HLSReader.parseOPI( m_opi );
}

void DecLib::xDecodeVPS( InputNALUnit& nalu )
{
  VPS* vps = new VPS();
  m_HLSReader.setBitstream( &nalu.getBitstream() );

  CHECK( nalu.m_temporalId, "The value of TemporalId of VPS NAL units shall be equal to 0" );

  m_HLSReader.parseVPS( vps );

  // storeVPS may directly delete the new VPS in case it is a repetition. Need to retrieve proper initialized memory back
  int vpsID = vps->getVPSId();
  m_parameterSetManager.storeVPS( vps, nalu.getBitstream().getFifo());

  if (m_vps==nullptr)
  {
    // m_vps is used for conformance checks. Unless a VPS is referred to, just set the first one we received
    // repeated parameter sets may be deleted, set a valid VPS pointer back from parameter set manager
    m_vps = m_parameterSetManager.getVPS(vpsID);
  }
}

void DecLib::xDecodeDCI(InputNALUnit& nalu)
{
  m_HLSReader.setBitstream(&nalu.getBitstream());

  CHECK(nalu.m_temporalId, "The value of TemporalId of DCI NAL units shall be equal to 0");
  if (!m_dci)
  {
    m_dci = new DCI;
    m_HLSReader.parseDCI(m_dci);
  }
  else
  {
    DCI dupDCI;
    m_HLSReader.parseDCI(&dupDCI);
    CHECK( !m_dci->IsIndenticalDCI(dupDCI), "Two signaled DCIs are different");
  }
}

void DecLib::xDecodeSPS( InputNALUnit& nalu )
{
  SPS* sps = new SPS();
  m_HLSReader.setBitstream( &nalu.getBitstream() );

  CHECK( nalu.m_temporalId, "The value of TemporalId of SPS NAL units shall be equal to 0" );

  m_HLSReader.parseSPS( sps );
  sps->setLayerId( nalu.m_nuhLayerId );
  DTRACE( g_trace_ctx, D_QP_PER_CTU, "CTU Size: %dx%d", sps->getMaxCUWidth(), sps->getMaxCUHeight() );
  m_accessUnitSpsNumSubpic[nalu.m_nuhLayerId] = sps->getNumSubPics();
  m_parameterSetManager.storeSPS( sps, nalu.getBitstream().getFifo() );
}

void DecLib::xDecodePPS( InputNALUnit& nalu )
{
  PPS* pps = new PPS();
  m_HLSReader.setBitstream( &nalu.getBitstream() );
  m_HLSReader.parsePPS( pps );
  pps->setLayerId( nalu.m_nuhLayerId );
  pps->setTemporalId( nalu.m_temporalId );
  pps->setPuCounter( m_puCounter );
  m_parameterSetManager.storePPS( pps, nalu.getBitstream().getFifo() );
}

void DecLib::xDecodeAPS(InputNALUnit& nalu)
{
  APS* aps = new APS();
  m_HLSReader.setBitstream(&nalu.getBitstream());
  m_HLSReader.parseAPS(aps);
  aps->setTemporalId(nalu.m_temporalId);
  aps->setLayerId( nalu.m_nuhLayerId );
  aps->setHasPrefixNalUnitType( nalu.m_nalUnitType == NAL_UNIT_PREFIX_APS );
  aps->setPuCounter( m_puCounter );
  m_parameterSetManager.checkAuApsContent(aps, m_accessUnitApsNals[aps->getAPSType()]);
  if( m_apsMapEnc )
  {
    APS* apsEnc = new APS();
    *apsEnc = *aps;
    (*m_apsMapEnc)[aps->getAPSType()].storePS(apsEnc->getAPSId(), apsEnc);
  }

  if( nalu.m_nalUnitType == NAL_UNIT_SUFFIX_APS && m_prevSliceSkipped )
  {
    m_accessUnitApsNals[aps->getAPSType()].pop_back();
  }

  // aps will be deleted if it was already stored (and did not changed),
  // thus, storing it must be last action.
  m_parameterSetManager.storeAPS(aps, nalu.getBitstream().getFifo());
}

bool DecLib::decode(InputNALUnit& nalu, int& iSkipFrame, int& iPOCLastDisplay, int iTargetOlsIdx)
{
  bool ret;
  // ignore all NAL units of layers > 0
  if( (nalu.m_nalUnitType != NAL_UNIT_SUFFIX_APS       &&
       nalu.m_nalUnitType != NAL_UNIT_EOS              &&
       nalu.m_nalUnitType != NAL_UNIT_EOB              &&
       nalu.m_nalUnitType != NAL_UNIT_SUFFIX_SEI       &&
       nalu.m_nalUnitType != NAL_UNIT_FD               &&
       nalu.m_nalUnitType != NAL_UNIT_RESERVED_NVCL_27 &&
       nalu.m_nalUnitType != NAL_UNIT_UNSPECIFIED_30   &&
       nalu.m_nalUnitType != NAL_UNIT_UNSPECIFIED_31)  ||
       !m_prevSliceSkipped )
  {
    AccessUnitInfo auInfo;
    auInfo.m_nalUnitType = nalu.m_nalUnitType;
    auInfo.m_nuhLayerId = nalu.m_nuhLayerId;
    auInfo.m_temporalId = nalu.m_temporalId;
    m_accessUnitNals.push_back(auInfo);
    m_pictureUnitNals.push_back( nalu.m_nalUnitType );
  }
  switch (nalu.m_nalUnitType)
  {
  case NAL_UNIT_VPS:
#if JVET_AJ0151_DSC_SEI
    xStoreNALUnitForSignature(nalu);
#endif
    xDecodeVPS(nalu);
    if (getTOlsIdxExternalFlag())
    {
      m_vps->m_targetOlsIdx = iTargetOlsIdx;
    }
    else if (getTOlsIdxOpiFlag())
    {
      m_vps->m_targetOlsIdx = m_opi->getOpiOlsIdx();
    }
    else
    {
      m_vps->m_targetOlsIdx = m_vps->deriveTargetOLSIdx();
    }
    return false;
  case NAL_UNIT_OPI: xDecodeOPI(nalu); return false;
  case NAL_UNIT_DCI: xDecodeDCI(nalu); return false;
  case NAL_UNIT_SPS:
#if JVET_AJ0151_DSC_SEI
    xStoreNALUnitForSignature(nalu);
#endif
    xDecodeSPS(nalu);
    return false;

  case NAL_UNIT_PPS:
#if JVET_AJ0151_DSC_SEI
    xStoreNALUnitForSignature(nalu);
#endif
    xDecodePPS(nalu);
    return false;

  case NAL_UNIT_PH:
#if JVET_AJ0151_DSC_SEI
    xStoreNALUnitForSignature(nalu);
#endif
    xDecodePicHeader(nalu);
    return !m_bFirstSliceInPicture;

  case NAL_UNIT_PREFIX_APS:
#if JVET_AJ0151_DSC_SEI
    xStoreNALUnitForSignature(nalu);
#endif
    xDecodeAPS(nalu);
    return false;

  case NAL_UNIT_SUFFIX_APS:
#if JVET_AJ0151_DSC_SEI
    xStoreNALUnitForSignature(nalu);
#endif
    if (m_prevSliceSkipped)
    {
      xDecodeAPS(nalu);
    }
    else
    {
      m_suffixApsNalus.push_back(new InputNALUnit(nalu));
    }
    return false;

  case NAL_UNIT_PREFIX_SEI:
    // Buffer up prefix SEI messages until SPS of associated VCL is known.
    m_prefixSEINALUs.push_back(new InputNALUnit(nalu));
    m_pictureSeiNalus.push_back(new InputNALUnit(nalu));
    return false;

  case NAL_UNIT_SUFFIX_SEI:
    if (m_pcPic)
    {
      if (m_prevSliceSkipped)
      {
        msg(NOTICE, "Note: received suffix SEI but current picture is skipped.\n");
        return false;
      }
      m_pictureSeiNalus.push_back(new InputNALUnit(nalu));
      m_accessUnitSeiNalus.push_back(new InputNALUnit(nalu));
      m_accessUnitSeiTids.push_back(nalu.m_temporalId);
      const SPS *sps = m_parameterSetManager.getActiveSPS();
      const VPS *vps = m_parameterSetManager.getVPS(sps->getVPSId());
      SEIMessages::iterator newSEI = m_seiReader.parseSEImessage(&(nalu.getBitstream()), m_pcPic->SEIs, nalu.m_nalUnitType, nalu.m_nuhLayerId,
                                                              nalu.m_temporalId, vps, sps, m_HRD, m_pDecodedSEIOutputStream);
#if JVET_S0257_DUMP_360SEI_MESSAGE
      m_seiCfgDump.write360SeiDump(m_decoded360SeiDumpFileName, m_pcPic->SEIs, sps);
#endif
      for (;newSEI != m_pcPic->SEIs.end(); newSEI++)
      {
        m_accessUnitSeiPayLoadTypes.push_back(std::tuple<NalUnitType, int, SEI::PayloadType>(
          nalu.m_nalUnitType, nalu.m_nuhLayerId, (*newSEI)->payloadType()));
      }
#if JVET_AJ0151_DSC_SEI
      SEIMessages dscVerifySEIs = getSeisByType( m_pcPic->SEIs, SEI::PayloadType::DIGITALLY_SIGNED_CONTENT_VERIFICATION);
      if (!dscVerifySEIs.empty())
      {
        for (auto dscvsei:dscVerifySEIs)
        {
          SEIDigitallySignedContentVerification *sei = (SEIDigitallySignedContentVerification*) dscvsei;
          m_dscSubstreamManager.verifySubstream(sei->dscvVerificationSubstreamId, sei->dscvSignature );
        }
      }

#endif
    }
    else
    {
      msg(NOTICE, "Note: received suffix SEI but no picture currently active.\n");
    }
    return false;

  case NAL_UNIT_CODED_SLICE_TRAIL:
  case NAL_UNIT_CODED_SLICE_STSA:
  case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
  case NAL_UNIT_CODED_SLICE_IDR_N_LP:
  case NAL_UNIT_CODED_SLICE_CRA:
  case NAL_UNIT_CODED_SLICE_GDR:
  case NAL_UNIT_CODED_SLICE_RADL:
  case NAL_UNIT_CODED_SLICE_RASL:
#if JVET_AJ0151_DSC_SEI
    xStoreNALUnitForSignature(nalu);
#endif
    ret = xDecodeSlice(nalu, iSkipFrame, iPOCLastDisplay);
    return ret;

  case NAL_UNIT_EOS:
    m_associatedIRAPType[nalu.m_nuhLayerId]            = NAL_UNIT_INVALID;
    m_pocCRA[nalu.m_nuhLayerId]                        = -MAX_INT;
    m_checkCRAFlags[nalu.m_nuhLayerId].clear();
    m_prevGDRInSameLayerPOC[nalu.m_nuhLayerId]         = -MAX_INT;
    m_prevGDRInSameLayerRecoveryPOC[nalu.m_nuhLayerId] = -MAX_INT;
    std::fill_n(m_prevGDRSubpicPOC[nalu.m_nuhLayerId], MAX_NUM_SUB_PICS, -MAX_INT);
    std::fill_n(m_prevIRAPSubpicPOC[nalu.m_nuhLayerId], MAX_NUM_SUB_PICS, -MAX_INT);
    memset(m_prevIRAPSubpicDecOrderNo[nalu.m_nuhLayerId], 0, sizeof(int) * MAX_NUM_SUB_PICS);
    std::fill_n(m_prevIRAPSubpicType[nalu.m_nuhLayerId], MAX_NUM_SUB_PICS, NAL_UNIT_INVALID);
    m_pocRandomAccess                  = MAX_INT;
    m_prevLayerID                      = MAX_INT;
    m_prevPOC                          = -MAX_INT;
    m_prevSliceSkipped                 = false;
    m_skippedPOC                       = 0;
    m_accessUnitEos[nalu.m_nuhLayerId] = true;
    m_prevEOS[nalu.m_nuhLayerId]       = true;
    return false;

  case NAL_UNIT_ACCESS_UNIT_DELIMITER:
  {
    AUDReader audReader;
    uint32_t  picType;
    audReader.parseAccessUnitDelimiter(&(nalu.getBitstream()), m_audIrapOrGdrAuFlag, picType);
    return !m_bFirstSliceInPicture;
  }

  case NAL_UNIT_EOB: return false;

  case NAL_UNIT_FD:
  {
    FDReader fdReader;
    uint32_t fdSize;
    fdReader.parseFillerData(&(nalu.getBitstream()), fdSize);
    msg(NOTICE, "Note: found NAL_UNIT_FD with %u bytes payload.\n", fdSize);
    return false;
  }

  case NAL_UNIT_RESERVED_IRAP_VCL_11:
    msg(NOTICE, "Note: found reserved VCL NAL unit.\n");
    xParsePrefixSEIsForUnknownVCLNal();
    return false;
  case NAL_UNIT_RESERVED_VCL_4:
  case NAL_UNIT_RESERVED_VCL_5:
  case NAL_UNIT_RESERVED_VCL_6:
  case NAL_UNIT_RESERVED_NVCL_26:
  case NAL_UNIT_RESERVED_NVCL_27: msg(NOTICE, "Note: found reserved NAL unit.\n"); return false;
  case NAL_UNIT_UNSPECIFIED_28:
  case NAL_UNIT_UNSPECIFIED_29:
  case NAL_UNIT_UNSPECIFIED_30:
  case NAL_UNIT_UNSPECIFIED_31: msg(NOTICE, "Note: found unspecified NAL unit.\n"); return false;
  default: THROW("Invalid NAL unit type"); break;
  }

  return false;
}


/** Function for checking if picture should be skipped because of random access. This function checks the skipping of pictures in the case of -s option random access.
 *  All pictures prior to the random access point indicated by the counter iSkipFrame are skipped.
 *  It also checks the type of Nal unit type at the random access point.
 *  If the random access point is CRA/CRANT/BLA/BLANT, TFD pictures with POC less than the POC of the random access point are skipped.
 *  If the random access point is IDR all pictures after the random access point are decoded.
 *  If the random access point is none of the above, a warning is issues, and decoding of pictures with POC
 *  equal to or greater than the random access point POC is attempted. For non IDR/CRA/BLA random
 *  access point there is no guarantee that the decoder will not crash.
 */
bool DecLib::isRandomAccessSkipPicture( int& iSkipFrame, int& iPOCLastDisplay, bool mixedNaluInPicFlag, uint32_t layerId )
{
  if( (iSkipFrame > 0) &&
      (m_apcSlicePilot->getFirstCtuRsAddrInSlice() == 0 && layerId == 0) &&
      (m_skippedPOC != MAX_INT) && (m_skippedLayerID != MAX_INT))
  {
    // When skipFrame count greater than 0, and current frame is not the first frame of sequence, decrement skipFrame count.
    // If skipFrame count is still greater than 0, the current frame will be skipped.
    iSkipFrame--;
  }

  if (iSkipFrame)
  {
    iSkipFrame--;   // decrement the counter
    m_maxDecSubPicIdx = 0;
    m_maxDecSliceAddrInSubPic = -1;
    return true;
  }
  else if ( m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL || m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP )
  {
    m_pocRandomAccess = -MAX_INT; // no need to skip the reordered pictures in IDR, they are decodable.
  }
  else if (m_pocRandomAccess == MAX_INT) // start of random access point, m_pocRandomAccess has not been set yet.
  {
    if (m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA || m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR )
    {
      // set the POC random access since we need to skip the reordered pictures in the case of CRA/CRANT/BLA/BLANT.
      m_pocRandomAccess = m_apcSlicePilot->getPOC();
    }
    else
    {
      if(!m_warningMessageSkipPicture)
      {
        msg( WARNING, "Warning: This is not a valid random access point and the data is discarded until the first CRA or GDR picture\n");
        m_warningMessageSkipPicture = true;
      }
      iSkipFrame--;
      m_maxDecSubPicIdx = 0;
      m_maxDecSliceAddrInSubPic = -1;
      return true;
    }
  }
  // skip the reordered pictures, if necessary
  else if (m_apcSlicePilot->getPOC() < m_pocRandomAccess &&
      (m_apcSlicePilot->getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL ||
       mixedNaluInPicFlag))
  {
    iPOCLastDisplay++;
    iSkipFrame--;
    m_maxDecSubPicIdx = 0;
    m_maxDecSliceAddrInSubPic = -1;
    return true;
  }
  // if we reach here, then the picture is not skipped.
  return false;
}

void DecLib::checkNalUnitConstraints( uint32_t naluType )
{
  if (m_parameterSetManager.getActiveSPS() != nullptr
      && m_parameterSetManager.getActiveSPS()->getProfileTierLevel() != nullptr)
  {
    const ConstraintInfo *cInfo = m_parameterSetManager.getActiveSPS()->getProfileTierLevel()->getConstraintInfo();
    xCheckNalUnitConstraintFlags( cInfo, naluType );
  }
}

void DecLib::xCheckNalUnitConstraintFlags( const ConstraintInfo *cInfo, uint32_t naluType )
{
  if (cInfo != nullptr)
  {
    CHECK(cInfo->getNoTrailConstraintFlag() && naluType == NAL_UNIT_CODED_SLICE_TRAIL,
      "Non-conforming bitstream. no_trail_constraint_flag is equal to 1 but bitstream contains NAL unit of type TRAIL_NUT.");
    CHECK(cInfo->getNoStsaConstraintFlag() && naluType == NAL_UNIT_CODED_SLICE_STSA,
      "Non-conforming bitstream. no_stsa_constraint_flag is equal to 1 but bitstream contains NAL unit of type STSA_NUT.");
    CHECK(cInfo->getNoRaslConstraintFlag() && naluType == NAL_UNIT_CODED_SLICE_RASL,
      "Non-conforming bitstream. no_rasl_constraint_flag is equal to 1 but bitstream contains NAL unit of type RASL_NUT.");
    CHECK(cInfo->getNoRadlConstraintFlag() && naluType == NAL_UNIT_CODED_SLICE_RADL,
      "Non-conforming bitstream. no_radl_constraint_flag is equal to 1 but bitstream contains NAL unit of type RADL_NUT.");
    CHECK(cInfo->getNoIdrConstraintFlag() && (naluType == NAL_UNIT_CODED_SLICE_IDR_W_RADL),
      "Non-conforming bitstream. no_idr_constraint_flag is equal to 1 but bitstream contains NAL unit of type IDR_W_RADL.");
    CHECK(cInfo->getNoIdrConstraintFlag() && (naluType == NAL_UNIT_CODED_SLICE_IDR_N_LP),
      "Non-conforming bitstream. no_idr_constraint_flag is equal to 1 but bitstream contains NAL unit of type IDR_N_LP.");
    CHECK(cInfo->getNoCraConstraintFlag() && naluType == NAL_UNIT_CODED_SLICE_CRA,
      "Non-conforming bitstream. no_cra_constraint_flag is equal to 1 but bitstream contains NAL unit of type CRA_NUT.");
    CHECK(cInfo->getNoGdrConstraintFlag() && naluType == NAL_UNIT_CODED_SLICE_GDR,
      "Non-conforming bitstream. no_gdr_constraint_flag is equal to 1 but bitstream contains NAL unit of type GDR_NUT.");
    CHECK(cInfo->getNoApsConstraintFlag() && naluType == NAL_UNIT_PREFIX_APS,
      "Non-conforming bitstream. no_aps_constraint_flag is equal to 1 but bitstream contains NAL unit of type APS_PREFIX_NUT.");
    CHECK(cInfo->getNoApsConstraintFlag() && naluType == NAL_UNIT_SUFFIX_APS,
      "Non-conforming bitstream. no_aps_constraint_flag is equal to 1 but bitstream contains NAL unit of type APS_SUFFIX_NUT.");
  }
}

void DecLib::xCheckMixedNalUnit(Slice* pcSlice, SPS *sps, InputNALUnit &nalu)
{
  if (pcSlice->getPPS()->getMixedNaluTypesInPicFlag())
  {
    CHECK(pcSlice->getPPS()->getNumSlicesInPic() < 2, "mixed nal unit type picture, but with less than 2 slices");

    CHECK( pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR, "picture with mixed NAL unit type cannot have GDR slice");

    //Check that if current slice is IRAP type, the other type of NAL can only be TRAIL_NUT
    if( pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA )
    {
      for( int i = 0; i < m_uiSliceSegmentIdx; i++ )
      {
        Slice* PreSlice = m_pcPic->slices[i];
        CHECK( (pcSlice->getNalUnitType() != PreSlice->getNalUnitType()) && (PreSlice->getNalUnitType() != NAL_UNIT_CODED_SLICE_TRAIL), "In a mixed NAL unt type picture, an IRAP slice can be mixed with Trail slice(s) only");
      }
    }

    // if this is the last slice of the picture, check whether that there are at least two different NAL unit types in the picture
    if (pcSlice->getPPS()->getNumSlicesInPic() == (m_uiSliceSegmentIdx + 1))
    {
      bool hasDiffTypes = false;
      for( int i = 1; !hasDiffTypes && i <= m_uiSliceSegmentIdx; i++ )
      {
        Slice* slice1 = m_pcPic->slices[i-1];
        Slice* slice2 = m_pcPic->slices[i];
        if( slice1->getNalUnitType() != slice2->getNalUnitType())
        {
          hasDiffTypes = true;
        }
      }
      CHECK( !hasDiffTypes, "VCL NAL units of the picture shall have two or more different nal_unit_type values");
    }
  }
  else // all slices shall have the same nal unit type
  {
    bool sameNalUnitType = true;
    for (int i = 0; i < m_uiSliceSegmentIdx; i++)
    {
      Slice *PreSlice = m_pcPic->slices[i];
      if (PreSlice->getNalUnitType() != pcSlice->getNalUnitType())
      {
        sameNalUnitType = false;
      }
    }
    CHECK(!sameNalUnitType, "pps_mixed_nalu_types_in_pic_flag is zero, but have different nal unit types");
  }
}

uint32_t DecLib::xGetNnpfaTargetId(uint8_t* payload, uint32_t payloadSize)
{
  uint64_t codeword = 0;
  uint32_t numBytes;
  for (numBytes = 0; numBytes < 8 && numBytes < payloadSize; numBytes++)
  {
    codeword = (codeword << 8) | payload[numBytes];
  }
  uint32_t firstBitPos = numBytes * 8 - 1;
  uint32_t bitPos;
  for (bitPos = firstBitPos; bitPos > 0 && !(codeword & ((uint64_t)1 << bitPos)); bitPos--);
  uint32_t cwLen = 2 * (firstBitPos - bitPos) + 1;
  return (uint32_t)((codeword >> (numBytes * 8 - cwLen)) - 1); // return decoded ID
}

/**
- lookahead through next NAL units to determine if current NAL unit is the first NAL unit in a new picture
*/
bool DecLib::isNewPicture(std::ifstream *bitstreamFile, class InputByteStream *bytestream)
{
  bool ret = false;
  bool finished = false;

  // cannot be a new picture if there haven't been any slices yet
  if(getFirstSliceInPicture())
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
        ret = true;
        finished = true;
        break;

      // NUT that may be the start of a new picture - check first bit in slice header
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
- lookahead through next NAL units to determine if current NAL unit is the first NAL unit in a new access unit
*/
bool DecLib::isNewAccessUnit( bool newPicture, std::ifstream *bitstreamFile, class InputByteStream *bytestream )
{
  bool ret = false;
  bool finished = false;

  // can only be the start of an AU if this is the start of a new picture
  if( newPicture == false )
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

  // look ahead until access unit start location is determined
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
      switch (nalu.m_nalUnitType)
      {
      // AUD always indicates the start of a new access unit
      case NAL_UNIT_ACCESS_UNIT_DELIMITER:
        ret = true;
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
        ret = isSliceNaluFirstInAU( newPicture, nalu );
        finished = true;
        break;

      // NUT that are not the start of a new access unit
      case NAL_UNIT_EOS:
      case NAL_UNIT_EOB:
      case NAL_UNIT_SUFFIX_APS:
      case NAL_UNIT_SUFFIX_SEI:
      case NAL_UNIT_FD:
        ret = false;
        finished = true;
        break;

      // all other NUT - keep looking to find first VCL
      default:
        break;
      }
    }
  }

  // restore previous stream location
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  bitstreamFile->clear();
  bitstreamFile->seekg(location);
  bytestream->reset();
  CodingStatistics::SetStatistics(*backupStats);
  delete backupStats;
#else
  bitstreamFile->clear();
  bitstreamFile->seekg(location);
  bytestream->reset();
#endif

  // return TRUE if next NAL unit is the start of a new picture
  return ret;
}
//! \}
