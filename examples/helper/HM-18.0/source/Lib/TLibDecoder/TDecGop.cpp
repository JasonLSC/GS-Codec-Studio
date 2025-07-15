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

/** \file     TDecGop.cpp
    \brief    GOP decoder class
*/

#include "TDecGop.h"
#include "TDecCAVLC.h"
#include "TDecSbac.h"
#include "TDecBinCoder.h"
#include "TDecBinCoderCABAC.h"
#include "libmd5/MD5.h"
#include "TLibCommon/SEI.h"

#include <time.h>

//! \ingroup TLibDecoder
//! \{
static Void calcAndPrintHashStatus(TComPicYuv& pic, const SEIDecodedPictureHash* pictureHashSEI, const BitDepths &bitDepths, UInt &numChecksumErrors);
// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================

TDecGop::TDecGop()
 : m_numberOfChecksumErrorsDetected(0)
{
  m_dDecTime = 0;
}

TDecGop::~TDecGop()
{

}

Void TDecGop::create()
{

}


Void TDecGop::destroy()
{
}

Void TDecGop::init( TDecEntropy*            pcEntropyDecoder,
                   TDecSbac*               pcSbacDecoder,
                   TDecBinCABAC*           pcBinCABAC,
                   TDecCavlc*              pcCavlcDecoder,
                   TDecSlice*              pcSliceDecoder,
                   TComLoopFilter*         pcLoopFilter,
                   TComSampleAdaptiveOffset* pcSAO
                   )
{
  m_pcEntropyDecoder      = pcEntropyDecoder;
  m_pcSbacDecoder         = pcSbacDecoder;
  m_pcBinCABAC            = pcBinCABAC;
  m_pcCavlcDecoder        = pcCavlcDecoder;
  m_pcSliceDecoder        = pcSliceDecoder;
  m_pcLoopFilter          = pcLoopFilter;
  m_pcSAO                 = pcSAO;
  m_numberOfChecksumErrorsDetected = 0;
}


// ====================================================================================================================
// Private member functions
// ====================================================================================================================
// ====================================================================================================================
// Public member functions
// ====================================================================================================================

Void TDecGop::decompressSlice(TComInputBitstream* pcBitstream, TComPic* pcPic)
{
  TComSlice*  pcSlice = pcPic->getSlice(pcPic->getCurrSliceIdx());
  // Table of extracted substreams.
  // These must be deallocated AND their internal fifos, too.
  TComInputBitstream **ppcSubstreams = NULL;

  //-- For time output for each slice
  clock_t iBeforeTime = clock();
  m_pcSbacDecoder->init( (TDecBinIf*)m_pcBinCABAC );
  m_pcEntropyDecoder->setEntropyDecoder (m_pcSbacDecoder);

  const UInt uiNumSubstreams = pcSlice->getNumberOfSubstreamSizes()+1;

  // init each couple {EntropyDecoder, Substream}
  ppcSubstreams    = new TComInputBitstream*[uiNumSubstreams];
  for ( UInt ui = 0 ; ui < uiNumSubstreams ; ui++ )
  {
    ppcSubstreams[ui] = pcBitstream->extractSubstream(ui+1 < uiNumSubstreams ? (pcSlice->getSubstreamSize(ui)<<3) : pcBitstream->getNumBitsLeft());
  }

  m_pcSliceDecoder->decompressSlice( ppcSubstreams, pcPic, m_pcSbacDecoder);
  // deallocate all created substreams, including internal buffers.
  for (UInt ui = 0; ui < uiNumSubstreams; ui++)
  {
    delete ppcSubstreams[ui];
  }
  delete[] ppcSubstreams;

  m_dDecTime += (Double)(clock()-iBeforeTime) / CLOCKS_PER_SEC;
}

Void TDecGop::filterPicture(TComPic* pcPic)
{
  TComSlice*  pcSlice = pcPic->getSlice(pcPic->getCurrSliceIdx());

  //-- For time output for each slice
  clock_t iBeforeTime = clock();

  // deblocking filter
  Bool bLFCrossTileBoundary = pcSlice->getPPS()->getLoopFilterAcrossTilesEnabledFlag();
  m_pcLoopFilter->setCfg(bLFCrossTileBoundary);
  m_pcLoopFilter->loopFilterPic( pcPic );

  if( pcSlice->getSPS()->getUseSAO() )
  {
    m_pcSAO->reconstructBlkSAOParams(pcPic, pcPic->getPicSym()->getSAOBlkParam());
    m_pcSAO->SAOProcess(pcPic);
    m_pcSAO->PCMLFDisableProcess(pcPic);
  }

  pcPic->compressMotion();
  TChar c = (pcSlice->isIntra() ? 'I' : pcSlice->isInterP() ? 'P' : 'B');
  if (!pcSlice->isReferenced())
  {
    c += 32;
  }

  //-- For time output for each slice
  printf("POC %4d TId: %1d ( %c-SLICE, QP%3d ) ", pcSlice->getPOC(),
                                                  pcSlice->getTLayer(),
                                                  c,
                                                  pcSlice->getSliceQp() );

  m_dDecTime += (Double)(clock()-iBeforeTime) / CLOCKS_PER_SEC;
  printf ("[DT %6.3f] ", m_dDecTime );
  m_dDecTime  = 0;

  for (Int iRefList = 0; iRefList < 2; iRefList++)
  {
    printf ("[L%d ", iRefList);
    for (Int iRefIndex = 0; iRefIndex < pcSlice->getNumRefIdx(RefPicList(iRefList)); iRefIndex++)
    {
      printf ("%d ", pcSlice->getRefPOC(RefPicList(iRefList), iRefIndex));
    }
    printf ("] ");
  }
  if (m_decodedPictureHashSEIEnabled)
  {
    SEIMessages pictureHashes = getSeisByType(pcPic->getSEIs(), SEI::DECODED_PICTURE_HASH );
    const SEIDecodedPictureHash *hash = ( pictureHashes.size() > 0 ) ? (SEIDecodedPictureHash*) *(pictureHashes.begin()) : NULL;
    if (pictureHashes.size() > 1)
    {
      printf ("Warning: Got multiple decoded picture hash SEI messages. Using first.");
    }
    calcAndPrintHashStatus(*(pcPic->getPicYuvRec()), hash, pcSlice->getSPS()->getBitDepths(), m_numberOfChecksumErrorsDetected);
  }

  printf("\n");

  pcPic->setOutputMark(pcPic->getSlice(0)->getPicOutputFlag() ? true : false);
  pcPic->setReconMark(true);
}

/**
 * Calculate and print hash for pic, compare to picture_digest SEI if
 * present in seis.  seis may be NULL.  Hash is printed to stdout, in
 * a manner suitable for the status line. Theformat is:
 *  [Hash_type:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx,(yyy)]
 * Where, x..x is the hash
 *        yyy has the following meanings:
 *            OK          - calculated hash matches the SEI message
 *            ***ERROR*** - calculated hash does not match the SEI message
 *            unk         - no SEI message was available for comparison
 */
static Void calcAndPrintHashStatus(TComPicYuv& pic, const SEIDecodedPictureHash* pictureHashSEI, const BitDepths &bitDepths, UInt &numChecksumErrors)
{
  /* calculate MD5sum for entire reconstructed picture */
  TComPictureHash recon_digest;
  Int numChar=0;
  const TChar* hashType = "\0";

  if (pictureHashSEI)
  {
    switch (pictureHashSEI->method)
    {
      case HASHTYPE_MD5:
        {
          hashType = "MD5";
          numChar = calcMD5(pic, recon_digest, bitDepths);
          break;
        }
      case HASHTYPE_CRC:
        {
          hashType = "CRC";
          numChar = calcCRC(pic, recon_digest, bitDepths);
          break;
        }
      case HASHTYPE_CHECKSUM:
        {
          hashType = "Checksum";
          numChar = calcChecksum(pic, recon_digest, bitDepths);
          break;
        }
      default:
        {
          assert (!"unknown hash type");
          break;
        }
    }
  }

  /* compare digest against received version */
  const TChar* ok = "(unk)";
  Bool mismatch = false;

  if (pictureHashSEI)
  {
    ok = "(OK)";
    if (recon_digest != pictureHashSEI->m_pictureHash)
    {
      ok = "(***ERROR***)";
      mismatch = true;
    }
  }

  printf("[%s:%s,%s] ", hashType, hashToString(recon_digest, numChar).c_str(), ok);

  if (mismatch)
  {
    numChecksumErrors++;
    printf("[rx%s:%s] ", hashType, hashToString(pictureHashSEI->m_pictureHash, numChar).c_str());
  }
}
//! \}
