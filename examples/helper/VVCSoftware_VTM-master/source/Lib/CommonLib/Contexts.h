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

/** \file     Contexts.h
 *  \brief    Classes providing probability descriptions and contexts (header)
 */

#ifndef __CONTEXTS__
#define __CONTEXTS__

#include "CommonDef.h"
#include "Slice.h"

#include <vector>

static constexpr int     PROB_BITS   = 15;   // Nominal number of bits to represent probabilities
static constexpr int     PROB_BITS_0 = 10;   // Number of bits to represent 1st estimate
static constexpr int     PROB_BITS_1 = 14;   // Number of bits to represent 2nd estimate
static constexpr int     MASK_0      = ~(~0u << PROB_BITS_0) << (PROB_BITS - PROB_BITS_0);
static constexpr int     MASK_1      = ~(~0u << PROB_BITS_1) << (PROB_BITS - PROB_BITS_1);
static constexpr uint8_t DWS         = 8;   // 0x47 Default window sizes

struct BinFracBits
{
  uint32_t intBits[2];
};

enum class BpmType : int
{
  NONE = -1,
  // List of Binary Probability Models for entropy coding
  // The VVC standard currently defines a single model (STD)
  STD = 0,
  NUM
};

class ProbModelTables
{
protected:
  static const BinFracBits m_binFracBits[256];
  static const uint8_t      m_RenormTable_32  [ 32];          // Std         MP   MPI
};



class BinProbModelBase : public ProbModelTables
{
public:
  BinProbModelBase () {}
  ~BinProbModelBase() {}
  static uint32_t estFracBitsEP ()                    { return  (       1 << SCALE_BITS ); }
  static uint32_t estFracBitsEP ( unsigned numBins )  { return  ( numBins << SCALE_BITS ); }
};




class BinProbModel_Std : public BinProbModelBase
{
public:
  BinProbModel_Std()
  {
    uint16_t half = 1 << (PROB_BITS - 1);
    m_state[0]    = half;
    m_state[1]    = half;
    m_rate        = DWS;
  }
  ~BinProbModel_Std ()                {}
public:
  void            init              ( int qp, int initId );
  void update(unsigned bin)
  {
    int rate0 = m_rate >> 4;
    int rate1 = m_rate & 15;

    m_state[0] -= (m_state[0] >> rate0) & MASK_0;
    m_state[1] -= (m_state[1] >> rate1) & MASK_1;
    if (bin)
    {
      m_state[0] += (0x7fffu >> rate0) & MASK_0;
      m_state[1] += (0x7fffu >> rate1) & MASK_1;
    }
  }
  void setLog2WindowSize(uint8_t log2WindowSize)
  {
    int rate0 = 2 + ((log2WindowSize >> 2) & 3);
    int rate1 = 3 + rate0 + (log2WindowSize & 3);
    m_rate    = 16 * rate0 + rate1;
    CHECK(rate1 > 9, "Second window size is too large!");
  }
  void estFracBitsUpdate(unsigned bin, uint64_t &b)
  {
    b += estFracBits(bin);
    update(bin);
  }
  uint32_t        estFracBits(unsigned bin) const { return getFracBitsArray().intBits[bin]; }
  static uint32_t estFracBitsTrm(unsigned bin) { return (bin ? 0x3bfbb : 0x0010c); }
  BinFracBits     getFracBitsArray() const { return m_binFracBits[state()]; }
public:
  uint8_t state() const { return (m_state[0] + m_state[1]) >> 8; }
  uint8_t mps() const { return state() >> 7; }
  uint8_t getLPS(unsigned range) const
  {
    uint16_t q = state();
    if (q & 0x80)
      q = q ^ 0xff;
    return ((q >> 2) * (range >> 5) >> 1) + 4;
  }
  static uint8_t  getRenormBitsLPS(unsigned lpsRange) { return m_RenormTable_32[lpsRange >> 3]; }
  static uint8_t  getRenormBitsRange( unsigned range )                  { return    1; }
  uint16_t getState() const { return m_state[0] + m_state[1]; }
  void     setState(uint16_t pState)
  {
    m_state[0] = (pState >> 1) & MASK_0;
    m_state[1] = (pState >> 1) & MASK_1;
  }
public:
  uint64_t estFracExcessBits(const BinProbModel_Std &r) const
  {
    int n = 2 * state() + 1;
    return ((512 - n) * r.estFracBits(0) + n * r.estFracBits(1) + 256) >> 9;
  }
private:
  uint16_t m_state[2];
  uint8_t  m_rate;
};






class CtxSet
{
public:
  CtxSet( uint16_t offset, uint16_t size ) : Offset( offset ), Size( size ) {}
  CtxSet( const CtxSet& ctxSet ) : Offset( ctxSet.Offset ), Size( ctxSet.Size ) {}
  CtxSet( std::initializer_list<CtxSet> ctxSets );
public:
  uint16_t  operator()  ()  const
  {
    return Offset;
  }
  uint16_t  operator()  ( uint16_t inc )  const
  {
    CHECKD( inc >= Size, "Specified context increment (" << inc << ") exceed range of context set [0;" << Size - 1 << "]." );
    return Offset + inc;
  }
  bool operator== ( const CtxSet& ctxSet ) const
  {
    return ( Offset == ctxSet.Offset && Size == ctxSet.Size );
  }
  bool operator!= ( const CtxSet& ctxSet ) const
  {
    return ( Offset != ctxSet.Offset || Size != ctxSet.Size );
  }
public:
  uint16_t  Offset;
  uint16_t  Size;
};



class ContextSetCfg
{
public:
  // context sets: specify offset and size
  static const CtxSet   SplitFlag;
  static const CtxSet   SplitQtFlag;
  static const CtxSet   SplitHvFlag;
  static const CtxSet   Split12Flag;
  static const CtxSet   ModeConsFlag;
  static const CtxSet   SkipFlag;
  static const CtxSet   MergeFlag;
  static const CtxSet   RegularMergeFlag;
  static const CtxSet   MergeIdx;
  static const CtxSet   PredMode;
  static const CtxSet   MultiRefLineIdx;
  static const CtxSet   IntraLumaMpmFlag;
  static const CtxSet   IntraLumaPlanarFlag;
  static const CtxSet   CclmModeFlag;
  static const CtxSet   CclmModeIdx;
  static const CtxSet   IntraChromaPredMode;
  static const CtxSet   MipFlag;
  static const CtxSet   DeltaQP;
  static const CtxSet   InterDir;
  static const CtxSet   RefPic;
  static const CtxSet   MmvdFlag;
  static const CtxSet   MmvdMergeIdx;
  static const CtxSet   MmvdStepMvpIdx;
  static const CtxSet   SubblockMergeFlag;
  static const CtxSet   AffineFlag;
  static const CtxSet   AffineType;
  static const CtxSet   AffMergeIdx;
  static const CtxSet   Mvd;
  static const CtxSet   BDPCMMode;
  static const CtxSet   QtRootCbf;
  static const CtxSet   ACTFlag;
  static const CtxSet   QtCbf           [3];    // [ channel ]
  static const CtxSet   SigCoeffGroup   [2];    // [ ChannelType ]
  static const CtxSet   LastX           [2];    // [ ChannelType ]
  static const CtxSet   LastY           [2];    // [ ChannelType ]
  static const CtxSet   SigFlag         [6];    // [ ChannelType + State ]
  static const CtxSet   ParFlag         [2];    // [ ChannelType ]
  static const CtxSet   GtxFlag         [4];    // [ ChannelType + x ]
  static const CtxSet   TsSigCoeffGroup;
  static const CtxSet   TsSigFlag;
  static const CtxSet   TsParFlag;
  static const CtxSet   TsGtxFlag;
  static const CtxSet   TsLrg1Flag;
  static const CtxSet   TsResidualSign;
  static const CtxSet   MVPIdx;
  static const CtxSet   SaoMergeFlag;
  static const CtxSet   SaoTypeIdx;
  static const CtxSet   TransformSkipFlag;
  static const CtxSet   MTSIdx;
  static const CtxSet   LFNSTIdx;
  static const CtxSet   PLTFlag;
  static const CtxSet   RotationFlag;
  static const CtxSet   RunTypeFlag;
  static const CtxSet   IdxRunModel;
  static const CtxSet   CopyRunModel;
  static const CtxSet   SbtFlag;
  static const CtxSet   SbtQuadFlag;
  static const CtxSet   SbtHorFlag;
  static const CtxSet   SbtPosFlag;
  static const CtxSet   ChromaQpAdjFlag;
  static const CtxSet   ChromaQpAdjIdc;
  static const CtxSet   ImvFlag;
  static const CtxSet   bcwIdx;
  static const CtxSet   alfCtbFlag;
  static const CtxSet   ctbAlfAlternative;
  static const CtxSet   alfUseApsFlag;
  static const CtxSet   CcAlfFilterControlFlag;
  static const CtxSet   CiipFlag;
  static const CtxSet   SmvdFlag;
  static const CtxSet   IBCFlag;
  static const CtxSet   ISPMode;
  static const CtxSet   JointCbCrFlag;
  static const unsigned NumberOfContexts;

  // combined sets for less complex copying
  // NOTE: The contained CtxSet's should directly follow each other in the initalization list;
  //       otherwise, you will copy more elements than you want !!!
  static const CtxSet   Sao;
  static const CtxSet   Alf;
  static const CtxSet   Palette;
  static const CtxSet   ctxPartition;

public:
  static const std::vector<uint8_t>&  getInitTable( unsigned initId );
private:
  static std::vector<std::vector<uint8_t> > sm_InitTables;
  static CtxSet addCtxSet( std::initializer_list<std::initializer_list<uint8_t> > initSet2d );
};



class FracBitsAccess
{
public:
  virtual BinFracBits getFracBitsArray( unsigned ctxId ) const = 0;
};



template <class BinProbModel>
class CtxStore : public FracBitsAccess
{
public:
  CtxStore();
  CtxStore( bool dummy );
  CtxStore( const CtxStore<BinProbModel>& ctxStore );
public:
  void copyFrom(const CtxStore<BinProbModel> &src)
  {
    checkInit();
    std::copy_n(reinterpret_cast<const char *>(src.m_ctx), sizeof(BinProbModel) * ContextSetCfg::NumberOfContexts,
                reinterpret_cast<char *>(m_ctx));
  }
  void copyFrom(const CtxStore<BinProbModel> &src, const CtxSet &ctxSet)
  {
    checkInit();
    std::copy_n(reinterpret_cast<const char *>(src.m_ctx + ctxSet.Offset), sizeof(BinProbModel) * ctxSet.Size,
                reinterpret_cast<char *>(m_ctx + ctxSet.Offset));
  }
  void init       ( int qp, int initId );
  void setWinSizes( const std::vector<uint8_t>&   log2WindowSizes );
  void loadPStates( const std::vector<uint16_t>&  probStates );
  void savePStates( std::vector<uint16_t>&        probStates )  const;

  const BinProbModel &operator[](unsigned ctxId) const { return m_ctx[ctxId]; }
  BinProbModel       &operator[](unsigned ctxId) { return m_ctx[ctxId]; }
  uint32_t            estFracBits(unsigned bin, unsigned ctxId) const { return m_ctx[ctxId].estFracBits(bin); }

  BinFracBits getFracBitsArray(unsigned ctxId) const { return m_ctx[ctxId].getFracBitsArray(); }

private:
  inline void checkInit()
  {
    if (m_ctx)
    {
      return;
    }
    m_ctxBuffer.resize(ContextSetCfg::NumberOfContexts);
    m_ctx = m_ctxBuffer.data();
  }

private:
  std::vector<BinProbModel> m_ctxBuffer;
  BinProbModel             *m_ctx;
};



class Ctx;
class SubCtx
{
  friend class Ctx;
public:
  SubCtx(const CtxSet &ctxSet, const Ctx &ctx) : m_CtxSet(ctxSet), m_ctx(ctx) {}
  SubCtx(const SubCtx &subCtx) : m_CtxSet(subCtx.m_CtxSet), m_ctx(subCtx.m_ctx) {}
  const SubCtx& operator= ( const SubCtx& ) = delete;
private:
  const CtxSet  m_CtxSet;
  const Ctx    &m_ctx;
};



class Ctx : public ContextSetCfg
{
public:
  Ctx();
  Ctx( const BinProbModel_Std*    dummy );
  Ctx( const Ctx&                 ctx   );

public:
  const Ctx& operator= ( const Ctx& ctx )
  {
    m_bpmType = ctx.m_bpmType;
    switch (m_bpmType)
    {
    case BpmType::STD: m_CtxStore_Std.copyFrom(ctx.m_CtxStore_Std); break;
    default:        break;
    }
    ::memcpy( m_GRAdaptStats, ctx.m_GRAdaptStats, sizeof( unsigned ) * RExt__GOLOMB_RICE_ADAPTATION_STATISTICS_SETS );
    return *this;
  }

  SubCtx operator= ( SubCtx&& subCtx )
  {
    m_bpmType = subCtx.m_ctx.m_bpmType;
    switch (m_bpmType)
    {
    case BpmType::STD: m_CtxStore_Std.copyFrom(subCtx.m_ctx.m_CtxStore_Std, subCtx.m_CtxSet); break;
    default:        break;
    }
    return std::move(subCtx);
  }

  void  init ( int qp, int initId )
  {
    switch (m_bpmType)
    {
    case BpmType::STD: m_CtxStore_Std.init(qp, initId); break;
    default:        break;
    }
    for( std::size_t k = 0; k < RExt__GOLOMB_RICE_ADAPTATION_STATISTICS_SETS; k++ )
    {
      m_GRAdaptStats[k] = 0;
    }
  }

  void riceStatReset(int bitDepth, bool persistentRiceAdaptationEnabledFlag)
  {
    for (std::size_t k = 0; k < RExt__GOLOMB_RICE_ADAPTATION_STATISTICS_SETS; k++)
    {
      if (persistentRiceAdaptationEnabledFlag)
      {
          CHECK(bitDepth <= 10,"BitDepth shall be larger than 10.");
          m_GRAdaptStats[k] = 2 * floorLog2(bitDepth - 10);
      }
      else
      {
          m_GRAdaptStats[k] = 0;
      }
    }
  }

  void  loadPStates( const std::vector<uint16_t>& probStates )
  {
    switch (m_bpmType)
    {
    case BpmType::STD: m_CtxStore_Std.loadPStates(probStates); break;
    default:        break;
    }
  }

  void  savePStates( std::vector<uint16_t>& probStates ) const
  {
    switch (m_bpmType)
    {
    case BpmType::STD: m_CtxStore_Std.savePStates(probStates); break;
    default:        break;
    }
  }

  void  initCtxAndWinSize( unsigned ctxId, const Ctx& ctx, const uint8_t winSize )
  {
    switch (m_bpmType)
    {
    case BpmType::STD:
      m_CtxStore_Std  [ctxId] = ctx.m_CtxStore_Std  [ctxId];
      m_CtxStore_Std  [ctxId] . setLog2WindowSize   (winSize);
      break;
    default:
      break;
    }
  }

  const unsigned&     getGRAdaptStats ( unsigned      id )      const { return m_GRAdaptStats[id]; }
  unsigned&           getGRAdaptStats ( unsigned      id )            { return m_GRAdaptStats[id]; }

  const unsigned           getBaseLevel()                     const { return m_baseLevel; }
  void                setBaseLevel(int value)                         { m_baseLevel = value; }

public:
  BpmType             getBpmType() const { return m_bpmType; }
  const Ctx&          getCtx          ()                        const { return *this; }
  Ctx&                getCtx          ()                              { return *this; }

  explicit operator   const CtxStore<BinProbModel_Std>  &()     const { return m_CtxStore_Std; }
  explicit operator         CtxStore<BinProbModel_Std>  &()           { return m_CtxStore_Std; }

  const FracBitsAccess&   getFracBitsAcess()  const
  {
    switch (m_bpmType)
    {
    case BpmType::STD: return m_CtxStore_Std;
    default:        THROW("BPMType out of range");
    }
  }

private:
  BpmType                       m_bpmType;
  CtxStore<BinProbModel_Std>    m_CtxStore_Std;
protected:
  unsigned                      m_GRAdaptStats[RExt__GOLOMB_RICE_ADAPTATION_STATISTICS_SETS];
  int m_baseLevel;

};

typedef Pool<Ctx> CtxPool;

class TempCtx
{
  TempCtx( const TempCtx& ) = delete;
  const TempCtx& operator=( const TempCtx& ) = delete;
public:
  TempCtx(CtxPool *pool) : m_ctx(*pool->get()), m_pool(pool) {}
  TempCtx(CtxPool *pool, const Ctx &ctx) : m_ctx(*pool->get()), m_pool(pool) { m_ctx = ctx; }
  TempCtx(CtxPool *pool, SubCtx &&subCtx) : m_ctx(*pool->get()), m_pool(pool) { m_ctx = std::forward<SubCtx>(subCtx); }
  ~TempCtx() { m_pool->giveBack(&m_ctx); }
  const Ctx& operator=( const Ctx& ctx )          { return ( m_ctx = ctx ); }
  SubCtx     operator=( SubCtx&&   subCtx )       { return m_ctx = std::forward<SubCtx>( subCtx ); }
  operator const Ctx& ()           const          { return m_ctx; }
  operator       Ctx& ()                          { return m_ctx; }
private:
  Ctx&      m_ctx;
  CtxPool  *m_pool;
};



#endif
