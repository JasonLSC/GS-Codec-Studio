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

/** \file     CABACWriter.h
 *  \brief    Writer for low level syntax
 */

#ifndef __CABACWRITER__
#define __CABACWRITER__

#include "CommonLib/BitStream.h"
#include "CommonLib/ContextModelling.h"
#include "BinEncoder.h"


//! \ingroup EncoderLib
//! \{


class EncCu;
class CABACWriter
{
public:
  CABACWriter(BinEncIf &binEncoder) : m_binEncoder(binEncoder), m_bitstream(nullptr)
  {
    m_testCtx = m_binEncoder.getCtx();
    m_encCu   = nullptr;
  }
  virtual ~CABACWriter() {}

public:
  void        initCtxModels             ( const Slice&                  slice );
  void        setEncCu(EncCu *pcEncCu) { m_encCu = pcEncCu; }
  SliceType   getCtxInitId              ( const Slice&                  slice );
  void        initBitstream(OutputBitstream *bitstream)
  {
    m_bitstream = bitstream;
    m_binEncoder.init(m_bitstream);
  }

  const Ctx &getCtx() const { return m_binEncoder.getCtx(); }
  Ctx       &getCtx() { return m_binEncoder.getCtx(); }

  void     start() { m_binEncoder.start(); }
  void     resetBits() { m_binEncoder.resetBits(); }
  uint64_t getEstFracBits() const { return m_binEncoder.getEstFracBits(); }
  uint32_t getNumBins() { return m_binEncoder.getNumBins(); }
  bool     isEncoding() { return m_binEncoder.isEncoding(); }

public:
  // slice segment data (clause 7.3.8.1)
  void        end_of_slice              ();

  // coding tree unit (clause 7.3.8.2)
  void coding_tree_unit(CodingStructure &cs, const UnitArea &area, EnumArray<int, ChannelType> &qps, unsigned ctuRsAddr,
                        bool skipSao = false, bool skipAlf = false);

  // sao (clause 7.3.8.3)
  void        sao                       ( const Slice&                  slice,    unsigned          ctuRsAddr );
  void        sao_block_params(const SAOBlkParam &saoPars, const BitDepths &bitDepths, const bool *sliceEnabled,
                               bool leftMergeAvail, bool aboveMergeAvail, bool onlyEstMergeInfo);
  void        sao_offset_params(const SAOOffset &ctbPars, ComponentID compID, bool sliceEnabled, int bitDepth);
  // coding (quad)tree (clause 7.3.8.4)
  void        coding_tree               ( const CodingStructure&        cs,       Partitioner&      pm,         CUCtx& cuCtx, Partitioner* pPartitionerChroma = nullptr, CUCtx* pCuCtxChroma = nullptr);
  void        split_cu_mode             ( const PartSplit               split,    const CodingStructure& cs,    Partitioner& pm );
  void        mode_constraint           ( const PartSplit               split,    const CodingStructure& cs,    Partitioner& pm,    const ModeType modeType );

  // coding unit (clause 7.3.8.5)
  void        coding_unit               ( const CodingUnit&             cu,       Partitioner&      pm,         CUCtx& cuCtx );
  void        cu_skip_flag              ( const CodingUnit&             cu );
  void        pred_mode                 ( const CodingUnit&             cu );
  void        bdpcm_mode                ( const CodingUnit&             cu,       const ComponentID compID );

  void        cu_pred_data              ( const CodingUnit&             cu );
  void        cu_bcw_flag               ( const CodingUnit&             cu );
  void        extend_ref_line           (const PredictionUnit&          pu );
  void        extend_ref_line           (const CodingUnit&              cu );
  void        intra_luma_pred_modes     ( const CodingUnit&             cu );
  void        intra_luma_pred_mode      ( const PredictionUnit&         pu );
  void        intra_chroma_pred_modes   ( const CodingUnit&             cu );
  void        intra_chroma_lmc_mode     ( const PredictionUnit&         pu );
  void        intra_chroma_pred_mode    ( const PredictionUnit&         pu );
  void        cu_residual               ( const CodingUnit&             cu,       Partitioner&      pm,         CUCtx& cuCtx );
  void        rqt_root_cbf              ( const CodingUnit&             cu );
  void        adaptive_color_transform(const CodingUnit&             cu);
  void        sbt_mode                  ( const CodingUnit&             cu );
  void        end_of_ctu                ( const CodingUnit&             cu,       CUCtx&            cuCtx );
  void        mip_flag                  ( const CodingUnit&             cu );
  void        mip_pred_modes            ( const CodingUnit&             cu );
  void        mip_pred_mode             ( const PredictionUnit&         pu );
  void        cu_palette_info           ( const CodingUnit&             cu,       ComponentID       compBegin,     uint32_t numComp,          CUCtx&       cuCtx);
  void        cuPaletteSubblockInfo(const CodingUnit& cu, ComponentID compBegin, uint32_t numComp, int subSetId,
                                    uint32_t& prevRunPos, PLTRunMode& prevRunType);
  Pel         writePLTIndex             ( const CodingUnit&             cu,       uint32_t          idx,           PelBuf&  paletteIdx,       PLTtypeBuf&  paletteRunType, int         maxSymbol,   ComponentID compBegin );
  // prediction unit (clause 7.3.8.6)
  void        prediction_unit           ( const PredictionUnit&         pu );
  void        merge_flag                ( const PredictionUnit&         pu );
  void        merge_data                ( const PredictionUnit&         pu );
  void        affine_flag               ( const CodingUnit&             cu );
  void        subblock_merge_flag       ( const CodingUnit&             cu );
  void        merge_idx                 ( const PredictionUnit&         pu );
  void        mmvd_merge_idx(const PredictionUnit&         pu);
  void        imv_mode                  ( const CodingUnit&             cu );
  void        affine_amvr_mode          ( const CodingUnit&             cu );
  void        inter_pred_idc            ( const PredictionUnit&         pu );
  void        ref_idx                   ( const PredictionUnit&         pu,       RefPicList        eRefList );
  void        mvp_flag                  ( const PredictionUnit&         pu,       RefPicList        eRefList );

  void        ciip_flag(const PredictionUnit &pu);
  void        smvd_mode              ( const PredictionUnit&         pu );


  // transform tree (clause 7.3.8.8)
  void        transform_tree            ( const CodingStructure&        cs,       Partitioner&      pm,     CUCtx& cuCtx,                         const PartSplit ispType = TU_NO_ISP, const int subTuIdx = -1 );
  void cbf_comp(bool cbf, const CompArea &area, unsigned depth, bool prevCbf, bool useISP, BdpcmMode bdpcmMode);

  // mvd coding (clause 7.3.8.9)
  void        mvd_coding                ( const PredictionUnit& pu, Mv mvd, int amvr );
  // transform unit (clause 7.3.8.10)
  void        transform_unit            ( const TransformUnit&          tu,       CUCtx&            cuCtx,  Partitioner& pm,       const int subTuCounter = -1 );
  void        cu_qp_delta               ( const CodingUnit&             cu,       int               predQP, const int8_t qp );
  void        cu_chroma_qp_offset       ( const CodingUnit&             cu );

  // residual coding (clause 7.3.8.11)
  void        residual_coding           ( const TransformUnit&          tu,       ComponentID       compID, CUCtx* cuCtx = nullptr );
  void        ts_flag                   ( const TransformUnit&          tu,       ComponentID       compID );
  void        mts_idx                   ( const CodingUnit&             cu,       CUCtx*            cuCtx  );
  void        residual_lfnst_mode       ( const CodingUnit&             cu,       CUCtx&            cuCtx );
  void        isp_mode                  ( const CodingUnit&             cu );
  void        last_sig_coeff            ( CoeffCodingContext&           cctx,     const TransformUnit& tu, ComponentID       compID );
  void        residual_coding_subblock  ( CoeffCodingContext&           cctx,     const TCoeff*     coeff, const int stateTransTable, int& state );
  void        residual_codingTS         ( const TransformUnit&          tu,       ComponentID       compID );
  void        residual_coding_subblockTS( CoeffCodingContext&           cctx,     const TCoeff*     coeff, unsigned (&RiceBit)[8], int riceParam, bool ricePresentFlag);
  void        joint_cb_cr               ( const TransformUnit&          tu,       const int cbfMask );


  void        codeAlfCtuEnableFlags     ( CodingStructure& cs, ChannelType channel, AlfParam* alfParam);
  void        codeAlfCtuEnableFlags     ( CodingStructure& cs, ComponentID compID, AlfParam* alfParam);
  void        codeAlfCtuEnableFlag      ( CodingStructure& cs, uint32_t ctuRsAddr, const int compIdx, AlfParam* alfParam );
  void        codeAlfCtuFilterIndex(CodingStructure& cs, uint32_t ctuRsAddr, bool alfEnableLuma);

  void        codeAlfCtuAlternatives     ( CodingStructure& cs, ChannelType channel, AlfParam* alfParam);
  void        codeAlfCtuAlternatives     ( CodingStructure& cs, ComponentID compID, AlfParam* alfParam);
  void        codeAlfCtuAlternative(CodingStructure &cs, uint32_t ctuRsAddr, const int compIdx,
                                    const AlfParam *alfParam = nullptr);
  void codeCcAlfFilterControlIdc(uint8_t idcVal, CodingStructure &cs, const ComponentID compID, const int curIdx,
                                 const uint8_t *filterControlIdc, Position lumaPos, const int filterCount);

private:
  void        unary_max_symbol          ( unsigned symbol, unsigned ctxId0, unsigned ctxIdN, unsigned maxSymbol );
  void        unary_max_eqprob          ( unsigned symbol,                                   unsigned maxSymbol );
  void        exp_golomb_eqprob         ( unsigned symbol, unsigned count );

  void        xWriteTruncBinCode(uint32_t symbol, uint32_t numSymbols);
  void        codeScanRotationModeFlag   ( const CodingUnit& cu,     ComponentID compBegin);
  void        xEncodePLTPredIndicator    ( const CodingUnit& cu,     uint32_t    maxPltSize, ComponentID compBegin);
private:
  BinEncIf         &m_binEncoder;
  OutputBitstream  *m_bitstream;
  Ctx               m_testCtx;
  EncCu            *m_encCu;
  ScanElement*      m_scanOrder;
};



class CABACEncoder
{
public:
  CABACEncoder()
    : m_CABACWriterStd(m_BinEncoderStd)
    , m_CABACEstimatorStd(m_BitEstimatorStd)
    , m_CABACWriter{ &m_CABACWriterStd }
    , m_CABACEstimator{ &m_CABACEstimatorStd }
  {}

  CABACWriter *getCABACWriter(const SPS *sps) { return m_CABACWriter[BpmType::STD]; }
  CABACWriter *getCABACEstimator(const SPS *sps) { return m_CABACEstimator[BpmType::STD]; }

private:
  BinEncoder_Std      m_BinEncoderStd;
  BitEstimator_Std    m_BitEstimatorStd;
  CABACWriter         m_CABACWriterStd;
  CABACWriter         m_CABACEstimatorStd;

  EnumArray<CABACWriter *, BpmType> m_CABACWriter;
  EnumArray<CABACWriter *, BpmType> m_CABACEstimator;
};

//! \}

#endif //__CABACWRITER__
