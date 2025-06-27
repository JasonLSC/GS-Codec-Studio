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


#include "BinEncoder.h"

#include "CommonLib/Rom.h"
#include "CommonLib/dtrace_next.h"

BinCounter::BinCounter()
  : m_ctxBinsCodedBuffer(Ctx::NumberOfContexts)
  , m_numBinsCtx(m_ctxBinsCodedBuffer.data())
  , m_numBinsEP(0)
  , m_numBinsTrm(0)
{}


void BinCounter::reset()
{
  for (std::size_t k = 0; k < m_ctxBinsCodedBuffer.size(); k++)
  {
    m_numBinsCtx[k] = 0;
  }
  m_numBinsEP  = 0;
  m_numBinsTrm = 0;
}


uint32_t BinCounter::getAll() const
{
  uint32_t count = m_numBinsEP + m_numBinsTrm;
  for (std::size_t k = 0; k < m_ctxBinsCodedBuffer.size(); k++)
  {
    count += m_numBinsCtx[k];
  }
  return count;
}

template<class BinProbModel>
BinEncoderBase::BinEncoderBase(const BinProbModel *dummy)
  : BinEncIf(dummy), m_bitstream(nullptr), m_low(0), m_range(0), m_bufferedByte(0), m_numBufferedBytes(0), m_bitsLeft(0)
{}

void BinEncoderBase::init( OutputBitstream* bitstream )
{
  m_bitstream = bitstream;
}

void BinEncoderBase::uninit()
{
  m_bitstream = nullptr;
}

void BinEncoderBase::start()
{
  m_low               = 0;
  m_range             = 510;
  m_bufferedByte      = 0xff;
  m_numBufferedBytes  = 0;
  m_bitsLeft          = 23;
  BinCounter::reset();
  m_binStore.reset();
}

void BinEncoderBase::finish()
{
  if (m_low >> (32 - m_bitsLeft))
  {
    m_bitstream->write(m_bufferedByte + 1, 8);
    while( m_numBufferedBytes > 1 )
    {
      m_bitstream->write(0x00, 8);
      m_numBufferedBytes--;
    }
    m_low -= 1 << (32 - m_bitsLeft);
  }
  else
  {
    if( m_numBufferedBytes > 0 )
    {
      m_bitstream->write(m_bufferedByte, 8);
    }
    while( m_numBufferedBytes > 1 )
    {
      m_bitstream->write(0xff, 8);
      m_numBufferedBytes--;
    }
  }
  m_bitstream->write(m_low >> 8, 24 - m_bitsLeft);
}

void BinEncoderBase::restart()
{
  m_low               = 0;
  m_range             = 510;
  m_bufferedByte      = 0xff;
  m_numBufferedBytes  = 0;
  m_bitsLeft          = 23;
}

void BinEncoderBase::reset( int qp, int initId )
{
  Ctx::init( qp, initId );
  start();
}

void BinEncoderBase::resetBits()
{
  m_low               = 0;
  m_bufferedByte      = 0xff;
  m_numBufferedBytes  = 0;
  m_bitsLeft          = 23;
  BinCounter::reset();
}

void BinEncoderBase::encodeBinEP( unsigned bin )
{
  DTRACE(g_trace_ctx, D_CABAC, "%d  %d  EP=%d \n", DTRACE_GET_COUNTER(g_trace_ctx, D_CABAC), m_range, bin);

  BinCounter::addEP();
  m_low <<= 1;
  if( bin )
  {
    m_low += m_range;
  }
  m_bitsLeft--;
  if( m_bitsLeft < 12 )
  {
    writeOut();
  }
}

void BinEncoderBase::encodeBinsEP( unsigned bins, unsigned numBins )
{
  for(int i = 0; i < numBins; i++)
  {
    DTRACE(g_trace_ctx, D_CABAC, "%d  %d  EP=%d \n", DTRACE_GET_COUNTER(g_trace_ctx, D_CABAC), m_range,
           (bins >> (numBins - 1 - i)) & 1);
  }

  BinCounter::addEP( numBins );
  if (m_range == 256)
  {
    encodeAlignedBinsEP( bins, numBins );
    return;
  }
  while( numBins > 8 )
  {
    numBins          -= 8;
    unsigned pattern  = bins >> numBins;
    m_low <<= 8;
    m_low += m_range * pattern;
    bins             -= pattern << numBins;
    m_bitsLeft       -= 8;
    if( m_bitsLeft < 12 )
    {
      writeOut();
    }
  }
  m_low <<= numBins;
  m_low += m_range * bins;
  m_bitsLeft -= numBins;
  if( m_bitsLeft < 12 )
  {
    writeOut();
  }
}

void BinEncoderBase::encodeRemAbsEP(unsigned bins, unsigned goRicePar, unsigned cutoff, int maxLog2TrDynamicRange)
{
  const unsigned threshold = cutoff << goRicePar;
  if (bins < threshold)
  {
    const unsigned bitMask = (1 << goRicePar) - 1;
    const unsigned length = (bins >> goRicePar) + 1;
    encodeBinsEP((1 << length) - 2, length);
    encodeBinsEP(bins & bitMask, goRicePar);
  }
  else
  {
    const unsigned  maxPrefixLength = 32 - cutoff - maxLog2TrDynamicRange;
    unsigned        prefixLength = 0;
    unsigned        codeValue = (bins >> goRicePar) - cutoff;
    unsigned        suffixLength;
    if (codeValue >= ((1 << maxPrefixLength) - 1))
    {
      prefixLength = maxPrefixLength;
      suffixLength = maxLog2TrDynamicRange;
    }
    else
    {
      while (codeValue > ((2 << prefixLength) - 2))
      {
        prefixLength++;
      }
      suffixLength = prefixLength + goRicePar + 1; //+1 for the separator bit
    }
    const unsigned totalPrefixLength = prefixLength + cutoff;
    const unsigned bitMask = (1 << goRicePar) - 1;
    const unsigned prefix = (1 << totalPrefixLength) - 1;
    const unsigned suffix = ((codeValue - ((1 << prefixLength) - 1)) << goRicePar) | (bins & bitMask);
    encodeBinsEP(prefix, totalPrefixLength); //prefix
    encodeBinsEP(suffix, suffixLength); //separator, suffix, and rParam bits
  }
}

void BinEncoderBase::encodeBinTrm( unsigned bin )
{
  BinCounter::addTrm();
  m_range -= 2;
  if( bin )
  {
    m_low += m_range;
    m_low <<= 7;
    m_range = 2 << 7;
    m_bitsLeft -= 7;
  }
  else if (m_range >= 256)
  {
    return;
  }
  else
  {
    m_low <<= 1;
    m_range <<= 1;
    m_bitsLeft--;
  }
  if( m_bitsLeft < 12 )
  {
    writeOut();
  }
}


void BinEncoderBase::align()
{
  m_range = 256;
}


void BinEncoderBase::encodeAlignedBinsEP( unsigned bins, unsigned numBins )
{
  unsigned remBins = numBins;
  while( remBins > 0 )
  {
    //The process of encoding an EP bin is the same as that of coding a normal
    //bin where the symbol ranges for 1 and 0 are both half the range:
    //
    //  low = (low + range/2) << 1       (to encode a 1)
    //  low =  low            << 1       (to encode a 0)
    //
    //  i.e.
    //  low = (low + (bin * range/2)) << 1
    //
    //  which is equivalent to:
    //
    //  low = (low << 1) + (bin * range)
    //
    //  this can be generalised for multiple bins, producing the following expression:
    //
    unsigned binsToCode = std::min<unsigned>( remBins, 8); //code bytes if able to take advantage of the system's byte-write function
    unsigned binMask    = ( 1 << binsToCode ) - 1;
    unsigned newBins    = ( bins >> ( remBins - binsToCode ) ) & binMask;
    m_low               = (m_low << binsToCode) + (newBins << 8);   // range is known to be 256
    remBins            -= binsToCode;
    m_bitsLeft         -= binsToCode;
    if( m_bitsLeft < 12 )
    {
      writeOut();
    }
  }
}

void BinEncoderBase::writeOut()
{
  unsigned leadByte = m_low >> (24 - m_bitsLeft);
  m_bitsLeft       += 8;
  m_low &= 0xffffffffu >> m_bitsLeft;
  if( leadByte == 0xff )
  {
    m_numBufferedBytes++;
  }
  else
  {
    if( m_numBufferedBytes > 0 )
    {
      unsigned carry  = leadByte >> 8;
      unsigned byte   = m_bufferedByte + carry;
      m_bufferedByte  = leadByte & 0xff;
      m_bitstream->write(byte, 8);
      byte            = ( 0xff + carry ) & 0xff;
      while( m_numBufferedBytes > 1 )
      {
        m_bitstream->write(byte, 8);
        m_numBufferedBytes--;
      }
    }
    else
    {
      m_numBufferedBytes  = 1;
      m_bufferedByte      = leadByte;
    }
  }
}

template<class BinProbModel>
TBinEncoder<BinProbModel>::TBinEncoder()
  : BinEncoderBase(static_cast<const BinProbModel *>(nullptr)), m_ctx(static_cast<CtxStore<BinProbModel> &>(*this))
{}

template <class BinProbModel>
void TBinEncoder<BinProbModel>::encodeBin( unsigned bin, unsigned ctxId )
{
  BinCounter::addCtx( ctxId );
  BinProbModel &probModel = m_ctx[ctxId];
  uint32_t      lpsRange  = probModel.getLPS(m_range);

  DTRACE(g_trace_ctx, D_CABAC, "%d %d %d  [%d:%d]  %2d(MPS=%d)    -  %d\n", DTRACE_GET_COUNTER(g_trace_ctx, D_CABAC),
         ctxId, m_range, m_range - lpsRange, lpsRange, (unsigned int) (probModel.state()), bin == probModel.mps(), bin);

  m_range -= lpsRange;
  if (bin != probModel.mps())
  {
    int numBits = probModel.getRenormBitsLPS(lpsRange);
    m_bitsLeft   -= numBits;
    m_low += m_range;
    m_low   = m_low << numBits;
    m_range = lpsRange << numBits;
    if( m_bitsLeft < 12 )
    {
      writeOut();
    }
  }
  else
  {
    if (m_range < 256)
    {
      int numBits = probModel.getRenormBitsRange(m_range);
      m_bitsLeft   -= numBits;
      m_low <<= numBits;
      m_range <<= numBits;
      if( m_bitsLeft < 12 )
      {
        writeOut();
      }
    }
  }
  probModel.update(bin);
  BinEncoderBase::m_binStore.addBin(bin, ctxId);
}

template <class BinProbModel>
BinEncIf* TBinEncoder<BinProbModel>::getTestBinEncoder() const
{
  BinEncIf* testBinEncoder = 0;
  if (m_binStore.inUse())
  {
    testBinEncoder = new TBinEncoder<BinProbModel>();
  }
  return testBinEncoder;
}





template <class BinProbModel>
BitEstimatorBase::BitEstimatorBase( const BinProbModel* dummy )
  : BinEncIf      ( dummy )
{
  m_estFracBits = 0;
}

void BitEstimatorBase::encodeRemAbsEP(unsigned bins, unsigned goRicePar, unsigned cutoff, int maxLog2TrDynamicRange)
{
  const unsigned threshold = cutoff << goRicePar;
  if (bins < threshold)
  {
    m_estFracBits += BinProbModelBase::estFracBitsEP((bins >> goRicePar) + 1 + goRicePar);
  }
  else
  {
    const unsigned  maxPrefixLength = 32 - cutoff - maxLog2TrDynamicRange;
    unsigned        prefixLength = 0;
    unsigned        codeValue = (bins >> goRicePar) - cutoff;
    unsigned        suffixLength;
    if (codeValue >= ((1 << maxPrefixLength) - 1))
    {
      prefixLength = maxPrefixLength;
      suffixLength = maxLog2TrDynamicRange;
    }
    else
    {
      while (codeValue > ((2 << prefixLength) - 2))
      {
        prefixLength++;
      }
      suffixLength = prefixLength + goRicePar + 1; //+1 for the separator bit
    }
    m_estFracBits += BinProbModelBase::estFracBitsEP(cutoff + prefixLength + suffixLength);
  }
}

void BitEstimatorBase::align()
{
  static const uint64_t add   = BinProbModelBase::estFracBitsEP() - 1;
  static const uint64_t mask  = ~add;
  m_estFracBits += add;
  m_estFracBits &= mask;
}

template<class BinProbModel>
TBitEstimator<BinProbModel>::TBitEstimator()
  : BitEstimatorBase(static_cast<const BinProbModel *>(nullptr)), m_ctx(static_cast<CtxStore<BinProbModel> &>(*this))
{}



template class TBinEncoder<BinProbModel_Std>;

template class TBitEstimator<BinProbModel_Std>;

