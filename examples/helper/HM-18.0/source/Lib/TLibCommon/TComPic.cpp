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

/** \file     TComPic.cpp
    \brief    picture class
*/

#include "TComPic.h"
#include "SEI.h"

//! \ingroup TLibCommon
//! \{

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

TComPic::TComPic()
: m_uiTLayer                              (0)
, m_bUsedByCurr                           (false)
, m_bIsLongTerm                           (false)
, m_pcPicYuvPred                          (NULL)
, m_pcPicYuvResi                          (NULL)
, m_bReconstructed                        (false)
, m_bNeededForOutput                      (false)
, m_uiCurrSliceIdx                        (0)
, m_bCheckLTMSB                           (false)
{
  for(UInt i=0; i<NUM_PIC_YUV; i++)
  {
    m_apcPicYuv[i]      = NULL;
  }
#if JVET_X0048_X0103_FILM_GRAIN
  m_isMctfFiltered      = false;
  m_grainCharacteristic = NULL;
  m_grainBuf            = NULL;
#endif
}

TComPic::~TComPic()
{
  destroy();
}

#if REDUCED_ENCODER_MEMORY
Void TComPic::create( const TComSPS &sps, const TComPPS &pps, const Bool bCreateEncoderSourcePicYuv, const Bool bCreateForImmediateReconstruction
#if SHUTTER_INTERVAL_SEI_PROCESSING
  , const Bool bCreateForProcessedReconstruction
#endif
#if JVET_X0048_X0103_FILM_GRAIN
  , const Bool bCreateFilteredSourcePicYuv
#endif
)
#else
Void TComPic::create(const TComSPS &sps, const TComPPS &pps, const Bool bIsVirtual
#if SHUTTER_INTERVAL_SEI_PROCESSING
  , const Bool bCreateForProcessedReconstruction
#endif
#if JVET_X0048_X0103_FILM_GRAIN
  , const Bool bCreateFilteredSourcePicYuv
#endif
)
#endif
{
  destroy();

  const ChromaFormat chromaFormatIDC = sps.getChromaFormatIdc();
  const Int          iWidth          = sps.getPicWidthInLumaSamples();
  const Int          iHeight         = sps.getPicHeightInLumaSamples();
  const UInt         uiMaxCuWidth    = sps.getMaxCUWidth();
  const UInt         uiMaxCuHeight   = sps.getMaxCUHeight();
  const UInt         uiMaxDepth      = sps.getMaxTotalCUDepth();

#if REDUCED_ENCODER_MEMORY
  m_picSym.create( sps, pps, uiMaxDepth, bCreateForImmediateReconstruction );
  if (bCreateEncoderSourcePicYuv)
#else
  m_picSym.create( sps, pps, uiMaxDepth );
  if (!bIsVirtual)
#endif
  {
    m_apcPicYuv[PIC_YUV_ORG    ]   = new TComPicYuv;  m_apcPicYuv[PIC_YUV_ORG     ]->create( iWidth, iHeight, chromaFormatIDC, uiMaxCuWidth, uiMaxCuHeight, uiMaxDepth, true );
    m_apcPicYuv[PIC_YUV_TRUE_ORG]  = new TComPicYuv;  m_apcPicYuv[PIC_YUV_TRUE_ORG]->create( iWidth, iHeight, chromaFormatIDC, uiMaxCuWidth, uiMaxCuHeight, uiMaxDepth, true );
  }
#if REDUCED_ENCODER_MEMORY
  if (bCreateForImmediateReconstruction)
  {
#endif
    m_apcPicYuv[PIC_YUV_REC]  = new TComPicYuv;  m_apcPicYuv[PIC_YUV_REC]->create( iWidth, iHeight, chromaFormatIDC, uiMaxCuWidth, uiMaxCuHeight, uiMaxDepth, true );
#if SHUTTER_INTERVAL_SEI_PROCESSING
    if (bCreateForProcessedReconstruction)
    {
      m_apcPicYuv[PIC_YUV_POST_REC] = new TComPicYuv;  m_apcPicYuv[PIC_YUV_POST_REC]->create(iWidth, iHeight, chromaFormatIDC, uiMaxCuWidth, uiMaxCuHeight, uiMaxDepth, true);
    }
#endif
#if REDUCED_ENCODER_MEMORY
  }
#endif

#if JVET_X0048_X0103_FILM_GRAIN
  if (bCreateFilteredSourcePicYuv)
  {
    m_apcPicYuv[PIC_FILTERED_ORIGINAL_FG] = new TComPicYuv;  m_apcPicYuv[PIC_FILTERED_ORIGINAL_FG]->create(iWidth, iHeight, chromaFormatIDC, uiMaxCuWidth, uiMaxCuHeight, uiMaxDepth, true);
  }
#endif

  // there are no SEI messages associated with this picture initially
  if (m_SEIs.size() > 0)
  {
    deleteSEIs (m_SEIs);
  }
  m_bUsedByCurr = false;
}

#if REDUCED_ENCODER_MEMORY
#if JVET_X0048_X0103_FILM_GRAIN
Void TComPic::prepareForEncoderSourcePicYuv( const Bool bCreateFilteredSourcePicYuv )
#else
Void TComPic::prepareForEncoderSourcePicYuv()
#endif
{
  const TComSPS &sps=m_picSym.getSPS();

  const ChromaFormat chromaFormatIDC = sps.getChromaFormatIdc();
  const Int          iWidth          = sps.getPicWidthInLumaSamples();
  const Int          iHeight         = sps.getPicHeightInLumaSamples();
  const UInt         uiMaxCuWidth    = sps.getMaxCUWidth();
  const UInt         uiMaxCuHeight   = sps.getMaxCUHeight();
  const UInt         uiMaxDepth      = sps.getMaxTotalCUDepth();

  if (m_apcPicYuv[PIC_YUV_ORG    ]==NULL)
  {
    m_apcPicYuv[PIC_YUV_ORG    ]   = new TComPicYuv;  m_apcPicYuv[PIC_YUV_ORG     ]->create( iWidth, iHeight, chromaFormatIDC, uiMaxCuWidth, uiMaxCuHeight, uiMaxDepth, true );
  }
  if (m_apcPicYuv[PIC_YUV_TRUE_ORG    ]==NULL)
  {
    m_apcPicYuv[PIC_YUV_TRUE_ORG]  = new TComPicYuv;  m_apcPicYuv[PIC_YUV_TRUE_ORG]->create( iWidth, iHeight, chromaFormatIDC, uiMaxCuWidth, uiMaxCuHeight, uiMaxDepth, true );
  }
#if JVET_X0048_X0103_FILM_GRAIN
  if (m_apcPicYuv[PIC_FILTERED_ORIGINAL_FG] == NULL && bCreateFilteredSourcePicYuv)
  {
    m_apcPicYuv[PIC_FILTERED_ORIGINAL_FG] = new TComPicYuv; m_apcPicYuv[PIC_FILTERED_ORIGINAL_FG]->create(iWidth, iHeight, chromaFormatIDC, uiMaxCuWidth, uiMaxCuHeight, uiMaxDepth, true);
  }
#endif
}

#if SHUTTER_INTERVAL_SEI_PROCESSING
Void TComPic::prepareForReconstruction( const Bool bCreateForProcessedReconstruction )
#else
Void TComPic::prepareForReconstruction()
#endif
{
  if (m_apcPicYuv[PIC_YUV_REC] == NULL)
  {
    const TComSPS &sps=m_picSym.getSPS();
    const ChromaFormat chromaFormatIDC = sps.getChromaFormatIdc();
    const Int          iWidth          = sps.getPicWidthInLumaSamples();
    const Int          iHeight         = sps.getPicHeightInLumaSamples();
    const UInt         uiMaxCuWidth    = sps.getMaxCUWidth();
    const UInt         uiMaxCuHeight   = sps.getMaxCUHeight();
    const UInt         uiMaxDepth      = sps.getMaxTotalCUDepth();

    m_apcPicYuv[PIC_YUV_REC]  = new TComPicYuv;  m_apcPicYuv[PIC_YUV_REC]->create( iWidth, iHeight, chromaFormatIDC, uiMaxCuWidth, uiMaxCuHeight, uiMaxDepth, true );
  }

  // mark it should be extended
  m_apcPicYuv[PIC_YUV_REC]->setBorderExtension(false);

#if SHUTTER_INTERVAL_SEI_PROCESSING
  if (m_apcPicYuv[PIC_YUV_POST_REC] == NULL && bCreateForProcessedReconstruction)
  {
    const TComSPS &sps = m_picSym.getSPS();
    const ChromaFormat chromaFormatIDC = sps.getChromaFormatIdc();
    const Int          iWidth = sps.getPicWidthInLumaSamples();
    const Int          iHeight = sps.getPicHeightInLumaSamples();
    const UInt         uiMaxCuWidth = sps.getMaxCUWidth();
    const UInt         uiMaxCuHeight = sps.getMaxCUHeight();
    const UInt         uiMaxDepth = sps.getMaxTotalCUDepth();

    m_apcPicYuv[PIC_YUV_POST_REC] = new TComPicYuv;  m_apcPicYuv[PIC_YUV_POST_REC]->create(iWidth, iHeight, chromaFormatIDC, uiMaxCuWidth, uiMaxCuHeight, uiMaxDepth, true);
  }

  // mark it should be extended
  if (bCreateForProcessedReconstruction)
  {
    m_apcPicYuv[PIC_YUV_POST_REC]->setBorderExtension(false);
  }
#endif

  m_picSym.prepareForReconstruction();
}

Void TComPic::releaseReconstructionIntermediateData()
{
  m_picSym.releaseReconstructionIntermediateData();
}

Void TComPic::releaseEncoderSourceImageData()
{
  if (m_apcPicYuv[PIC_YUV_ORG    ])
  {
    m_apcPicYuv[PIC_YUV_ORG]->destroy();
    delete m_apcPicYuv[PIC_YUV_ORG];
    m_apcPicYuv[PIC_YUV_ORG] = NULL;
  }
  if (m_apcPicYuv[PIC_YUV_TRUE_ORG    ])
  {
    m_apcPicYuv[PIC_YUV_TRUE_ORG]->destroy();
    delete m_apcPicYuv[PIC_YUV_TRUE_ORG];
    m_apcPicYuv[PIC_YUV_TRUE_ORG] = NULL;
  }
#if JVET_X0048_X0103_FILM_GRAIN
  if (m_apcPicYuv[PIC_FILTERED_ORIGINAL_FG])
  {
    m_apcPicYuv[PIC_FILTERED_ORIGINAL_FG]->destroy();
    delete m_apcPicYuv[PIC_FILTERED_ORIGINAL_FG];
    m_apcPicYuv[PIC_FILTERED_ORIGINAL_FG] = NULL;
  }
#endif
}

Void TComPic::releaseAllReconstructionData()
{
#if SHUTTER_INTERVAL_SEI_PROCESSING
  if (m_apcPicYuv[PIC_YUV_POST_REC])
  {
    m_apcPicYuv[PIC_YUV_POST_REC]->destroy();
    delete m_apcPicYuv[PIC_YUV_POST_REC];
    m_apcPicYuv[PIC_YUV_POST_REC] = NULL;
  }
#endif
  if (m_apcPicYuv[PIC_YUV_REC    ])
  {
    m_apcPicYuv[PIC_YUV_REC]->destroy();
    delete m_apcPicYuv[PIC_YUV_REC];
    m_apcPicYuv[PIC_YUV_REC] = NULL;
  }
  m_picSym.releaseAllReconstructionData();
}
#endif

Void TComPic::destroy()
{
  m_picSym.destroy();

  for(UInt i=0; i<NUM_PIC_YUV; i++)
  {
    if (m_apcPicYuv[i])
    {
      m_apcPicYuv[i]->destroy();
      delete m_apcPicYuv[i];
      m_apcPicYuv[i]  = NULL;
    }
  }

  deleteSEIs(m_SEIs);
#if JVET_X0048_X0103_FILM_GRAIN
  m_grainBuf = NULL;
#endif
}

Void TComPic::compressMotion()
{
  TComPicSym* pPicSym = getPicSym();
  for ( UInt uiCUAddr = 0; uiCUAddr < pPicSym->getNumberOfCtusInFrame(); uiCUAddr++ )
  {
    TComDataCU* pCtu = pPicSym->getCtu(uiCUAddr);
    pCtu->compressMV();
  }
}

Bool  TComPic::getSAOMergeAvailability(Int currAddr, Int mergeAddr)
{
  Bool mergeCtbInSliceSeg = (mergeAddr >= getPicSym()->getCtuTsToRsAddrMap(getCtu(currAddr)->getSlice()->getSliceCurStartCtuTsAddr()));
  Bool mergeCtbInTile     = (getPicSym()->getTileIdxMap(mergeAddr) == getPicSym()->getTileIdxMap(currAddr));
  return (mergeCtbInSliceSeg && mergeCtbInTile);
}

UInt TComPic::getSubstreamForCtuAddr(const UInt ctuAddr, const Bool bAddressInRaster, TComSlice *pcSlice)
{
  UInt subStrm;
  const bool bWPPEnabled=pcSlice->getPPS()->getEntropyCodingSyncEnabledFlag();
  const TComPicSym &picSym            = *(getPicSym());

  if ((bWPPEnabled && picSym.getFrameHeightInCtus()>1) || (picSym.getNumTiles()>1)) // wavefronts, and possibly tiles being used.
  {
    if (bWPPEnabled)
    {
      const UInt ctuRsAddr                = bAddressInRaster?ctuAddr : picSym.getCtuTsToRsAddrMap(ctuAddr);
      const UInt frameWidthInCtus         = picSym.getFrameWidthInCtus();
      const UInt tileIndex                = picSym.getTileIdxMap(ctuRsAddr);
      const UInt numTileColumns           = (picSym.getNumTileColumnsMinus1()+1);
      const TComTile *pTile               = picSym.getTComTile(tileIndex);
      const UInt firstCtuRsAddrOfTile     = pTile->getFirstCtuRsAddr();
      const UInt tileYInCtus              = firstCtuRsAddrOfTile / frameWidthInCtus;
      // independent tiles => substreams are "per tile"
      const UInt ctuLine                  = ctuRsAddr / frameWidthInCtus;
      const UInt startingSubstreamForTile =(tileYInCtus*numTileColumns) + (pTile->getTileHeightInCtus()*(tileIndex%numTileColumns));
      subStrm = startingSubstreamForTile + (ctuLine - tileYInCtus);
    }
    else
    {
      const UInt ctuRsAddr                = bAddressInRaster?ctuAddr : picSym.getCtuTsToRsAddrMap(ctuAddr);
      const UInt tileIndex                = picSym.getTileIdxMap(ctuRsAddr);
      subStrm=tileIndex;
    }
  }
  else
  {
    // dependent tiles => substreams are "per frame".
    subStrm = 0;
  }
  return subStrm;
}

#if JVET_X0048_X0103_FILM_GRAIN
// initialization of frequency based film grain syntheis buffers and parameters
void TComPic::createGrainSynthesizer(Bool bFirstPictureInSequence, SEIFilmGrainSynthesizer* pGrainCharacteristics, TComPicYuv* pGrainBuf, const TComSPS* sps)
{
    m_grainCharacteristic = pGrainCharacteristics;
    m_grainBuf = pGrainBuf;

    Int width         = sps->getPicWidthInLumaSamples();
    Int height        = sps->getPicHeightInLumaSamples();
    ChromaFormat fmt  = sps->getChromaFormatIdc();
    Int bitDepth      = sps->getBitDepth(CHANNEL_TYPE_LUMA);

    // Padding to make wd and ht multiple of max fgs window size(64)
    int paddedWdFGS = ((width - 1) | 0x3F) + 1 - width;
    int paddedHtFGS = ((height - 1) | 0x3F) + 1 - height;
    m_padValue = (paddedWdFGS > paddedHtFGS) ? paddedWdFGS : paddedHtFGS;

    if (bFirstPictureInSequence)
    {
      // Create and initialize the Film Grain Synthesizer
      m_grainCharacteristic->create(width, height, fmt, bitDepth, 1);

      //Frame level TComPicYuv buffer created to blend Film Grain Noise into it
      m_grainBuf->createWithPadding(width, height, fmt, true, true, m_padValue, m_padValue);

      m_grainCharacteristic->fgsInit();
    }
}

TComPicYuv* TComPic::getPicYuvDisp()
{
    Int payloadType = 0;
    std::list<SEI*>::iterator message;

    for (message = m_SEIs.begin(); message != m_SEIs.end(); ++message)
    {
        payloadType = (*message)->payloadType();
        if (payloadType == SEI::FILM_GRAIN_CHARACTERISTICS)
        {
          m_grainCharacteristic->m_errorCode = -1;
          *m_grainCharacteristic->m_fgcParameters = *static_cast<SEIFilmGrainCharacteristics*>(*message);
          /* Validation of Film grain characteristic parameters for the constrains of SMPTE-RDD5*/
          m_grainCharacteristic->m_errorCode = m_grainCharacteristic->grainValidateParams();
          break;
        }
    }

    if (FGS_SUCCESS == m_grainCharacteristic->m_errorCode)
    {
      m_apcPicYuv[PIC_YUV_REC]->copyToPic(m_grainBuf);
      m_grainBuf->extendPicBorder();  // Padding to make wd and ht multiple of max fgs window size(64)

      m_grainCharacteristic->m_poc = getPOC();
      m_grainCharacteristic->grainSynthesizeAndBlend(m_grainBuf, getSlice(0)->getIdrPicFlag());
      return m_grainBuf;
    }
    else
    {
      if (payloadType == SEI::FILM_GRAIN_CHARACTERISTICS)
      {
        fprintf(stdout, "Film Grain synthesis is not performed. Error code: 0x%x \n", m_grainCharacteristic->m_errorCode);
      }
      return  m_apcPicYuv[PIC_YUV_REC];
    }
}
#endif


#if SHUTTER_INTERVAL_SEI_PROCESSING
TComPic*  TComPic::findPrevPicPOC(TComPic* pcPic, TComList<TComPic*>* pcListPic)
{
  TComPic*  prevPic = NULL;
  TComPic*  listPic = NULL;
  TComList<TComPic*>::iterator  iterListPic = pcListPic->begin();
  for (Int i = 0; i < (Int)(pcListPic->size()); i++)
  {
    listPic = *(iterListPic);
    listPic->setCurrSliceIdx(0);
    if (listPic->getPOC() == pcPic->getPOC() - 1)
    {
      prevPic = *(iterListPic);
      prevPic->setCurrSliceIdx(0);
    }
    iterListPic++;
  }
  return prevPic;
}
Void TComPic::xOutputPostFilteredPic(TComPic* pcPic, TComList<TComPic*>* pcListPic)
{
  if (pcPic->getPOC() % 2 == 0)
  {
    TComPic* prevPic = findPrevPicPOC(pcPic, pcListPic);
    if (prevPic)
    {
      TComPicYuv* currYuv = pcPic->getPicYuvRec();
      TComPicYuv* prevYuv = prevPic->getPicYuvRec();
      TComPicYuv* postYuv = pcPic->getPicYuvPostRec();
      for (Int chan = 0; chan < currYuv->getNumberValidComponents(); chan++)
      {
        const ComponentID ch = ComponentID(chan);
        const ChannelType cType = (ch == COMPONENT_Y) ? CHANNEL_TYPE_LUMA : CHANNEL_TYPE_CHROMA;
        const Int bitDepth = pcPic->getSlice(0)->getSPS()->getBitDepth(cType);
        const Int maxOutputValue = (1 << bitDepth) - 1;

        Pel*  currPxl = currYuv->getAddr(ch);
        Pel*  prevPxl = prevYuv->getAddr(ch);
        Pel*  postPxl = postYuv->getAddr(ch);
        Int iStride = currYuv->getStride(ch);
        Int iHeight = currYuv->getHeight(ch);
        Int iWidth = currYuv->getWidth(ch);
        for (Int y = 0; y < iHeight; y++)
        {
          for (Int x = 0; x < iWidth; x++)
          {
            postPxl[x] = std::min(maxOutputValue, std::max(0, (currPxl[x] << 1) - prevPxl[x]));
          }
          currPxl += iStride;
          prevPxl += iStride;
          postPxl += iStride;
        }
      }
    }
    else
    {
      pcPic->getPicYuvRec()->copyToPic(pcPic->getPicYuvPostRec());
    }
  }
  else
  {
    pcPic->getPicYuvRec()->copyToPic(pcPic->getPicYuvPostRec());
  }
}
Void TComPic::xOutputPreFilteredPic(TComPic* pcPic, TComList<TComPic*>* pcListPic)
{
  if (pcPic->getPOC() % 2 == 0)
  {
    TComPic* prevPic = findPrevPicPOC(pcPic, pcListPic);
    if (prevPic)
    {
      TComPicYuv* currYuv = pcPic->getPicYuvOrg();
      TComPicYuv* prevYuv = prevPic->getPicYuvOrg();
      for (Int chan = 0; chan < currYuv->getNumberValidComponents(); chan++)
      {
        const ComponentID ch = ComponentID(chan);
        const ChannelType cType = (ch == COMPONENT_Y) ? CHANNEL_TYPE_LUMA : CHANNEL_TYPE_CHROMA;
        const Int bitDepth = pcPic->getSlice(0)->getSPS()->getBitDepth(cType);
        const Int maxOutputValue = (1 << bitDepth) - 1;

        Pel*  currPxl = currYuv->getAddr(ch);
        Pel*  prevPxl = prevYuv->getAddr(ch);
        Int iStride = currYuv->getStride(ch);
        Int iHeight = currYuv->getHeight(ch);
        Int iWidth = currYuv->getWidth(ch);
        for (Int y = 0; y < iHeight; y++)
        {
          for (Int x = 0; x < iWidth; x++)
          {
            currPxl[x] = std::min( maxOutputValue, std::max( 0, (currPxl[x] + prevPxl[x]) >> 1) );
          }
          currPxl += iStride;
          prevPxl += iStride;
        }
      }
    }
  }
}
#endif
//! \}
