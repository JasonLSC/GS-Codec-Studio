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

/** \file     BinDecoder.h
 *  \brief    Low level binary symbol writer
 */

#pragma once


#include "CommonLib/Contexts.h"
#include "CommonLib/BitStream.h"


#if RExt__DECODER_DEBUG_BIT_STATISTICS
class CodingStatisticsClassType;
#endif



class BinDecoderBase : public Ctx
{
protected:
  template <class BinProbModel>
  BinDecoderBase ( const BinProbModel* dummy );
public:
  ~BinDecoderBase() {}
public:
  void      init    ( InputBitstream* bitstream );
  void      uninit  ();
  void      start   ();
  void      finish  ();
  void      reset   ( int qp, int initId );
  void      riceStatReset(int bitDepth, bool persistentRiceAdaptationEnabledFlag);
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  void      set     ( const CodingStatisticsClassType& type) { ptype = &type; }
#endif

public:
  virtual unsigned  decodeBin           ( unsigned ctxId    ) = 0;

public:
  unsigned          decodeBinEP         ();
  unsigned          decodeBinsEP        ( unsigned numBins  );
  unsigned          decodeRemAbsEP      ( unsigned goRicePar, unsigned cutoff, int maxLog2TrDynamicRange );
  unsigned          decodeBinTrm        ();
  void              align               ();
  unsigned          getNumBitsRead()
  {
    return m_bitstream->getNumBitsRead() + m_bitsNeeded;
  }

private:
  unsigned          decodeAlignedBinsEP ( unsigned numBins  );
protected:
  InputBitstream   *m_bitstream;
  uint32_t          m_range;
  uint32_t          m_value;
  int32_t           m_bitsNeeded;
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  const CodingStatisticsClassType* ptype;
#endif
};



template <class BinProbModel>
class TBinDecoder : public BinDecoderBase
{
public:
  TBinDecoder ();
  ~TBinDecoder() {}
  unsigned decodeBin ( unsigned ctxId );
private:
  CtxStore<BinProbModel> &m_ctx;
};



typedef TBinDecoder<BinProbModel_Std>   BinDecoder_Std;


