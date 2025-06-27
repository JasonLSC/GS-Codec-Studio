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

/** \file     VLCWriter.h
 *  \brief    Writer for high level syntax
 */

#ifndef __VLCWRITER__
#define __VLCWRITER__

#include "CommonLib/CommonDef.h"
#include "CommonLib/BitStream.h"
#include "CommonLib/Rom.h"
#include "CommonLib/Slice.h"
#include "CABACWriter.h"

//! \ingroup EncoderLib
//! \{

#if ENABLE_TRACING
extern bool g_HLSTraceEnable;
#endif

class VLCWriter
{
protected:

  OutputBitstream*    m_pcBitIf;

  VLCWriter() : m_pcBitIf(nullptr) {}
  virtual ~VLCWriter() {}

  void  setBitstream          ( OutputBitstream* p )  { m_pcBitIf = p;  }
  OutputBitstream* getBitstream( )                    { return m_pcBitIf; }

  void  xWriteSCode         ( int         value, uint32_t  length, const char *symbolName );
  void  xWriteCode          ( uint32_t    value, uint32_t  length, const char *symbolName );
  void  xWriteUvlc          ( uint32_t    value,                   const char *symbolName );
  void  xWriteSvlc          ( int         value,                   const char *symbolName );
  void  xWriteFlag          ( uint32_t    value,                   const char *symbolName );
  void  xWriteString        ( const std::string &value,            const char *symbolName );

  void  xWriteRbspTrailingBits();
  bool isByteAligned()      { return (m_pcBitIf->getNumBitsUntilByteAligned() == 0); } ;

private:
  void  xWriteVlc           ( uint32_t    value );

};


class AUDWriter : public VLCWriter
{
public:
  AUDWriter() {};
  virtual ~AUDWriter() {};

  void  codeAUD(OutputBitstream& bs, const bool audIrapOrGdrAuFlag, const int pictureType);
};

class FDWriter : public VLCWriter
{
public:
  FDWriter() {};
  virtual ~FDWriter() {};

  void  codeFD(OutputBitstream& bs, uint32_t &fdSize);
};


class HLSWriter : public VLCWriter
{
public:
  HLSWriter() {}
  virtual ~HLSWriter() {}

private:
  void xCodeRefPicList( const ReferencePictureList* rpl, bool isLongTermPresent, uint32_t ltLsbBitsCount, const bool isForbiddenZeroDeltaPoc, int rplIdx);
  bool xFindMatchingLTRP        ( Slice* pcSlice, uint32_t *ltrpsIndex, int ltrpPOC, bool usedFlag );
  void xCodePredWeightTable     ( Slice* pcSlice );
  void xCodePredWeightTable     ( PicHeader *picHeader, const PPS *pps, const SPS *sps );
  void xCodeScalingList         ( const ScalingList* scalingList, uint32_t scalinListId, bool isPredictor);
public:
  void  setBitstream            ( OutputBitstream* p )  { m_pcBitIf = p;  }
  uint32_t  getNumberOfWrittenBits  ()                      { return m_pcBitIf->getNumberOfWrittenBits();  }
  void  codeVUI                 ( const VUI *pcVUI, const SPS* pcSPS );
  void  codeSPS                 ( const SPS* pcSPS );
  void  codePPS                 ( const PPS* pcPPS );
  void  codeAPS                 ( APS* pcAPS );
  void  codeAlfAps              ( APS* pcAPS );
  void  codeLmcsAps             ( APS* pcAPS );
  void  codeScalingListAps      ( APS* pcAPS );
  void  codeVPS                 ( const VPS* pcVPS );
  void  codeDCI                 ( const DCI* dci );
  void  codePictureHeader       ( PicHeader* picHeader, bool writeRbspTrailingBits, Slice *slice = 0 );
  void  codeSliceHeader         ( Slice* pcSlice, PicHeader *picheader = 0 );
  void  codeOPI                 ( const OPI* opi );
  void  codeConstraintInfo      ( const ConstraintInfo* cinfo, const ProfileTierLevel* ptl );
  void  codeProfileTierLevel    ( const ProfileTierLevel* ptl, bool profileTierPresentFlag, int maxNumSubLayersMinus1 );
  void  codeOlsHrdParameters(const GeneralHrdParams * generalHrd, const OlsHrdParams *olsHrd , const uint32_t firstSubLayer, const uint32_t maxNumSubLayersMinus1);

  void codeGeneralHrdparameters(const GeneralHrdParams *hrd);
  void  codeTilesWPPEntryPoint  ( Slice* pSlice );
  void codeScalingList(const ScalingList &scalingList, bool aps_chromaPresentFlag);
  void alfFilter( const AlfParam& alfParam, const bool isChroma, const int altIdx );
  void dpb_parameters(int maxSubLayersMinus1, bool subLayerInfoFlag, const SPS *pcSPS);
private:
};

//! \}

#endif
