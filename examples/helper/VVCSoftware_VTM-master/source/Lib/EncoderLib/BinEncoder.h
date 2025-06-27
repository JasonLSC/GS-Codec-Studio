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


#pragma once

#include "CommonLib/Contexts.h"
#include "CommonLib/BitStream.h"
#include "CommonLib/dtrace_next.h"



class BinStore
{
public:
  BinStore () : m_inUse(false), m_allocated(false)  {}
  ~BinStore()                                       {}

  void  reset   ()
  {
    if( m_inUse )
    {
      for( unsigned n = 0; n < Ctx::NumberOfContexts; n++ )
      {
        m_binBuffer[n].clear();
      }
    }
  }
  void  addBin  ( unsigned bin, unsigned ctxId )
  {
    if( m_inUse )
    {
      std::vector<bool>& binBuffer = m_binBuffer[ctxId];
      if( binBuffer.size() < m_maxNumBins )
      {
        binBuffer.push_back( bin == 1 );
      }
    }
  }

  void                      setUse      ( bool useStore )         { m_inUse = useStore; if(m_inUse){xCheckAlloc();} }
  bool                      inUse       ()                  const { return m_inUse; }
  const std::vector<bool>&  getBinVector( unsigned ctxId )  const { return m_binBuffer[ctxId]; }

private:
  void  xCheckAlloc()
  {
    if( !m_allocated )
    {
      m_binBuffer.resize( Ctx::NumberOfContexts );
      for( unsigned n = 0; n < Ctx::NumberOfContexts; n++ )
      {
        m_binBuffer[n].reserve( m_maxNumBins );
      }
      m_allocated = true;
    }
  }

private:
  static const std::size_t          m_maxNumBins = 100000;
  bool                              m_inUse;
  bool                              m_allocated;
  std::vector< std::vector<bool> >  m_binBuffer;
};


class BinEncIf : public Ctx
{
protected:
  template <class BinProbModel>
  BinEncIf( const BinProbModel* dummy ) : Ctx( dummy ) {}
public:
  virtual ~BinEncIf() {}
public:
  virtual void      init              ( OutputBitstream* bitstream )        = 0;
  virtual void      uninit            ()                                    = 0;
  virtual void      start             ()                                    = 0;
  virtual void      finish            ()                                    = 0;
  virtual void      restart           ()                                    = 0;
  virtual void      reset             ( int qp, int initId )                = 0;
public:
  virtual void      resetBits         ()                                    = 0;
  virtual uint64_t  getEstFracBits    ()                              const = 0;
  virtual unsigned  getNumBins        ( unsigned    ctxId )           const = 0;
public:
  virtual void      encodeBin         ( unsigned bin,   unsigned ctxId    ) = 0;
  virtual void      encodeBinEP       ( unsigned bin                      ) = 0;
  virtual void      encodeBinsEP      ( unsigned bins,  unsigned numBins  ) = 0;
  virtual void      encodeRemAbsEP    ( unsigned bins,
                                        unsigned goRicePar,
                                        unsigned cutoff,
                                        int      maxLog2TrDynamicRange    ) = 0;
  virtual void      encodeBinTrm      ( unsigned bin                      ) = 0;
  virtual void      align             ()                                    = 0;
public:
  virtual uint32_t  getNumBins        ()                                    = 0;
  virtual bool      isEncoding        ()                                    = 0;
  virtual unsigned  getNumWrittenBits ()                                    = 0;
public:
  virtual void            setBinStorage     ( bool b )                      = 0;
  virtual const BinStore* getBinStore       ()                        const = 0;
  virtual BinEncIf*       getTestBinEncoder ()                        const = 0;
};



class BinCounter
{
public:
  BinCounter();
  ~BinCounter() {}
public:
  void      reset   ();
  void      addCtx(unsigned ctxId) { m_numBinsCtx[ctxId]++; }
  void      addEP(unsigned num) { m_numBinsEP += num; }
  void      addEP() { m_numBinsEP++; }
  void      addTrm() { m_numBinsTrm++; }
  uint32_t  getAll  ()                  const;
  uint32_t  getCtx(unsigned ctxId) const { return m_numBinsCtx[ctxId]; }
  uint32_t  getEP() const { return m_numBinsEP; }
  uint32_t  getTrm() const { return m_numBinsTrm; }

private:
  std::vector<uint32_t> m_ctxBinsCodedBuffer;
  uint32_t             *m_numBinsCtx;
  uint32_t              m_numBinsEP;
  uint32_t              m_numBinsTrm;
};



class BinEncoderBase : public BinEncIf, public BinCounter
{
protected:
  template <class BinProbModel>
  BinEncoderBase ( const BinProbModel* dummy );
public:
  ~BinEncoderBase() {}
public:
  void      init    ( OutputBitstream* bitstream );
  void      uninit  ();
  void      start   ();
  void      finish  ();
  void      restart ();
  void      reset   ( int qp, int initId );
  void      riceStatReset(int bitDepth, bool persistentRiceAdaptationEnabledFlag);
public:
  void      resetBits           ();
  uint64_t  getEstFracBits      ()                    const { THROW( "not supported" ); return 0; }
  unsigned  getNumBins          ( unsigned ctxId )    const { return BinCounter::getCtx(ctxId); }
public:
  void      encodeBinEP         ( unsigned bin                      );
  void      encodeBinsEP        ( unsigned bins,  unsigned numBins  );
  void      encodeRemAbsEP      ( unsigned bins,
                                  unsigned goRicePar,
                                  unsigned cutoff,
                                  int      maxLog2TrDynamicRange    );
  void      encodeBinTrm        ( unsigned bin                      );
  void      align               ();
  unsigned  getNumWrittenBits()
  {
    return (m_bitstream->getNumberOfWrittenBits() + 8 * m_numBufferedBytes + 23 - m_bitsLeft);
  }

public:
  uint32_t  getNumBins          ()                          { return BinCounter::getAll(); }
  bool      isEncoding          ()                          { return true; }
protected:
  void      encodeAlignedBinsEP ( unsigned bins,  unsigned numBins  );
  void      writeOut            ();
protected:
  OutputBitstream        *m_bitstream;
  uint32_t                m_low;
  uint32_t                m_range;
  uint32_t                m_bufferedByte;
  int32_t                 m_numBufferedBytes;
  int32_t                 m_bitsLeft;
  BinStore                m_binStore;
};



template <class BinProbModel>
class TBinEncoder : public BinEncoderBase
{
public:
  TBinEncoder ();
  ~TBinEncoder() {}
  void  encodeBin   ( unsigned bin, unsigned ctxId );
public:
  void            setBinStorage(bool b) { m_binStore.setUse(b); }
  const BinStore *getBinStore() const { return &m_binStore; }
  BinEncIf*       getTestBinEncoder ()          const;
private:
  CtxStore<BinProbModel> &m_ctx;
};





class BitEstimatorBase : public BinEncIf
{
protected:
  template <class BinProbModel>
  BitEstimatorBase ( const BinProbModel* dummy );
public:
  ~BitEstimatorBase() {}
public:
  void      init                ( OutputBitstream* bitstream )        {}
  void      uninit              ()                                    {}
  void      start() { m_estFracBits = 0; }
  void      finish              ()                                    {}
  void      restart() { m_estFracBits = (m_estFracBits >> SCALE_BITS) << SCALE_BITS; }
  void      reset(int qp, int initId)
  {
    Ctx::init(qp, initId);
    m_estFracBits = 0;
  }

public:
  void resetBits() { m_estFracBits = 0; }

  uint64_t  getEstFracBits() const { return m_estFracBits; }
  unsigned  getNumBins          ( unsigned ctxId )              const { THROW( "not supported for BitEstimator" ); return 0; }
public:
  void      encodeBinEP(unsigned bin) { m_estFracBits += BinProbModelBase::estFracBitsEP(); }
  void      encodeBinsEP(unsigned bins, unsigned numBins) { m_estFracBits += BinProbModelBase::estFracBitsEP(numBins); }
  void      encodeRemAbsEP      ( unsigned bins,
                                  unsigned goRicePar,
                                  unsigned cutoff,
                                  int      maxLog2TrDynamicRange    );
  void      align               ();
public:
  uint32_t  getNumBins          ()                                      { THROW("Not supported"); return 0; }
  bool      isEncoding          ()                                      { return false; }
  unsigned  getNumWrittenBits()
  {
    // THROW( "Not supported" );
    return (uint32_t) 0 /*(m_estFracBits >> SCALE_BITS)*/;
  }

protected:
  uint64_t m_estFracBits;
};



template <class BinProbModel>
class TBitEstimator : public BitEstimatorBase
{
public:
  TBitEstimator ();
  ~TBitEstimator() {}
  void            encodeBin(unsigned bin, unsigned ctxId) { m_ctx[ctxId].estFracBitsUpdate(bin, m_estFracBits); }
  void            encodeBinTrm(unsigned bin) { m_estFracBits += BinProbModel::estFracBitsTrm(bin); }
  void            setBinStorage     ( bool b )        {}
  const BinStore* getBinStore       ()          const { return 0; }
  BinEncIf*       getTestBinEncoder ()          const { return 0; }
private:
  CtxStore<BinProbModel> &m_ctx;
};



typedef TBinEncoder  <BinProbModel_Std>   BinEncoder_Std;

typedef TBitEstimator<BinProbModel_Std>   BitEstimator_Std;


