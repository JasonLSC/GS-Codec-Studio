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

/** \file     VLCWriter.cpp
 *  \brief    Writer for high level syntax
 */

#include "VLCWriter.h"
#include "SEIwrite.h"

#include "CommonLib/CommonDef.h"
#include "CommonLib/Unit.h"
#include "CommonLib/Picture.h" // th remove this
#include "CommonLib/dtrace_next.h"
#include "EncAdaptiveLoopFilter.h"
#include "CommonLib/AdaptiveLoopFilter.h"
#include "CommonLib/ProfileTierLevel.h"

//! \ingroup EncoderLib
//! \{

#if ENABLE_TRACING
bool g_HLSTraceEnable = true;
#endif

#if ENABLE_TRACING
void VLCWriter::xWriteSCode( const int value, const uint32_t length, const char *symbolName )
#else
void VLCWriter::xWriteSCode( const int value, const uint32_t length, const char* )
#endif
{
  CHECK ( length < 1 || length > 32, "Syntax element length must be in range 1..32" );
  CHECK (!( length==32 || (value>=-(1<<(length-1)) && value<(1<<(length-1)))), "Invalid syntax element" );
  m_pcBitIf->write( length==32 ? uint32_t(value) : ( uint32_t(value)&((1<<length)-1) ), length );

#if ENABLE_TRACING
  if( g_HLSTraceEnable )
  {
    if( length<10 )
    {
      DTRACE( g_trace_ctx, D_HEADER, "%-50s u(%d)  : %d\n", symbolName, length, value );
    }
    else
    {
      DTRACE( g_trace_ctx, D_HEADER, "%-50s u(%d) : %d\n", symbolName, length, value );
    }
  }
#endif
}

#if ENABLE_TRACING
void VLCWriter::xWriteCode( const uint32_t value, const uint32_t length, const char *symbolName )
#else
void VLCWriter::xWriteCode( const uint32_t value, const uint32_t length, const char* )
#endif
{
  CHECK(length == 0, "Code of length '0' not supported");
  m_pcBitIf->write(value, length);

#if ENABLE_TRACING
  if( g_HLSTraceEnable )
  {
    if( length < 10 )
    {
      DTRACE( g_trace_ctx, D_HEADER, "%-50s u(%d)  : %d\n", symbolName, length, value );
    }
    else
    {
      DTRACE( g_trace_ctx, D_HEADER, "%-50s u(%d) : %d\n", symbolName, length, value );
    }
  }
#endif
}

// write the VLC code without tracing
void VLCWriter::xWriteVlc( uint32_t value )
{
  uint32_t length   = 1;
  uint32_t temp     = ++value;

  CHECK(!temp, "Integer overflow");
  while (1 != temp)
  {
    temp >>= 1;
    length += 2;
  }
  // Take care of cases where length > 32
  m_pcBitIf->write(0, length >> 1);
  m_pcBitIf->write(value, (length + 1) >> 1);
}

#if ENABLE_TRACING
void VLCWriter::xWriteUvlc( const uint32_t value, const char *symbolName )
#else
void VLCWriter::xWriteUvlc( const uint32_t value, const char* )
#endif
{
  xWriteVlc(value);

#if ENABLE_TRACING
  if( g_HLSTraceEnable )
  {
    DTRACE( g_trace_ctx, D_HEADER, "%-50s ue(v) : %d\n", symbolName, value );
  }
#endif
}

void  VLCWriter::xWriteSvlc( const int value, const char *symbolName )
{
  uint32_t unsigendValue = uint32_t( value <= 0 ? (-value)<<1 : (value<<1)-1);
  xWriteVlc( unsigendValue );

#if ENABLE_TRACING
  if( g_HLSTraceEnable )
  {
    DTRACE( g_trace_ctx, D_HEADER, "%-50s se(v) : %d\n", symbolName, value );
  }
#endif
}


#if ENABLE_TRACING
void VLCWriter::xWriteFlag( uint32_t value, const char *symbolName )
#else
void VLCWriter::xWriteFlag( uint32_t value, const char* )
#endif
{
  m_pcBitIf->write( value, 1 );

#if ENABLE_TRACING
  if( g_HLSTraceEnable )
  {
    DTRACE( g_trace_ctx, D_HEADER, "%-50s u(1)  : %d\n", symbolName, value );
  }
#endif
}

#if ENABLE_TRACING
void  VLCWriter::xWriteString( const std::string &value, const char *symbolName )
#else
void  VLCWriter::xWriteString( const std::string &value, const char* )
#endif
{
  for (int i = 0; i < value.length(); ++i)
  {
    m_pcBitIf->write(value[i], 8);
  }
  m_pcBitIf->write('\0', 8);

#if ENABLE_TRACING
  if (g_HLSTraceEnable)
  {
    DTRACE(g_trace_ctx, D_HEADER, "%-50s st(v)  : %s\n", symbolName, value.c_str());
  }
#endif
}

void VLCWriter::xWriteRbspTrailingBits()
{
  xWriteFlag( 1, "rbsp_stop_one_bit");
  int cnt = 0;
  while (m_pcBitIf->getNumBitsUntilByteAligned())
  {
    xWriteFlag( 0, "rbsp_alignment_zero_bit");
    cnt++;
  }
  CHECK(cnt>=8, "More than '8' alignment bytes read");
}

void AUDWriter::codeAUD(OutputBitstream& bs, const bool audIrapOrGdrAuFlag, const int pictureType)
{
#if ENABLE_TRACING
  xTraceAccessUnitDelimiter();
#endif

  CHECK(pictureType >= 3, "Invalid picture type");
  setBitstream(&bs);
  xWriteFlag(audIrapOrGdrAuFlag, "aud_irap_or_gdr_au_flag");
  xWriteCode(pictureType, 3, "pic_type");
  xWriteRbspTrailingBits();
}

void FDWriter::codeFD(OutputBitstream& bs, uint32_t &fdSize)
{
#if ENABLE_TRACING
  xTraceFillerData();
#endif
  setBitstream(&bs);
  uint32_t ffByte = 0xff;
  while( fdSize )
  {
    xWriteCode (ffByte, 8, "ff_byte");
    fdSize--;
  }
  xWriteRbspTrailingBits();
}

void HLSWriter::xCodeRefPicList( const ReferencePictureList* rpl, bool isLongTermPresent, uint32_t ltLsbBitsCount, const bool isForbiddenZeroDeltaPoc, int rplIdx)
{
  uint32_t numRefPic = rpl->getNumRefEntries();
  xWriteUvlc( numRefPic, "num_ref_entries[ listIdx ][ rplsIdx ]" );

  if (isLongTermPresent && numRefPic > 0 && rplIdx != -1)
  {
    xWriteFlag(rpl->getLtrpInSliceHeaderFlag(), "ltrp_in_slice_header_flag[ listIdx ][ rplsIdx ]");
  }
  int prevDelta = MAX_INT;
  int deltaValue = 0;
  bool firstSTRP = true;
  for (int ii = 0; ii < numRefPic; ii++)
  {
    if( rpl->getInterLayerPresentFlag() )
    {
      xWriteFlag( rpl->isInterLayerRefPic( ii ), "inter_layer_ref_pic_flag[ listIdx ][ rplsIdx ][ i ]" );

      if( rpl->isInterLayerRefPic( ii ) )
      {
        CHECK( rpl->getInterLayerRefPicIdx( ii ) < 0, "Wrong inter-layer reference index" );
        xWriteUvlc( rpl->getInterLayerRefPicIdx( ii ), "ilrp_idx[ listIdx ][ rplsIdx ][ i ]" );
      }
    }

    if( !rpl->isInterLayerRefPic( ii ) )
    {
      if (isLongTermPresent)
      {
        xWriteFlag(!rpl->isRefPicLongterm(ii), "st_ref_pic_flag[ listIdx ][ rplsIdx ][ i ]");
      }

      if (!rpl->isRefPicLongterm(ii))
      {
        if (firstSTRP)
        {
          firstSTRP  = false;
          deltaValue = prevDelta = rpl->getRefPicIdentifier(ii);
        }
        else
        {
          deltaValue = rpl->getRefPicIdentifier(ii) - prevDelta;
          prevDelta  = rpl->getRefPicIdentifier(ii);
        }
        unsigned int absDeltaValue = (deltaValue < 0) ? 0 - deltaValue : deltaValue;
        if (isForbiddenZeroDeltaPoc || ii == 0)
        {
          CHECK(!absDeltaValue, "Zero delta POC is not used without WP or is the 0-th entry");
          xWriteUvlc(absDeltaValue - 1, "abs_delta_poc_st[ listIdx ][ rplsIdx ][ i ]");
        }
        else
        {
          xWriteUvlc(absDeltaValue, "abs_delta_poc_st[ listIdx ][ rplsIdx ][ i ]");
        }
        if (absDeltaValue > 0)
        {
          xWriteFlag(deltaValue < 0 ? 1 : 0, "strp_entry_sign_flag[ listIdx ][ rplsIdx ][ i ]");
        }
      }
      else if (!rpl->getLtrpInSliceHeaderFlag())
      {
        xWriteCode(rpl->getRefPicIdentifier(ii), ltLsbBitsCount, "poc_lsb_lt[listIdx][rplsIdx][i]");
      }
    }
  }
}

void HLSWriter::codePPS( const PPS* pcPPS )
{
#if ENABLE_TRACING
  xTracePPSHeader ();
#endif
  xWriteCode( pcPPS->getPPSId(), 6,                          "pps_pic_parameter_set_id" );
  xWriteCode( pcPPS->getSPSId(), 4,                          "pps_seq_parameter_set_id" );

  xWriteFlag( pcPPS->getMixedNaluTypesInPicFlag() ? 1 : 0,   "pps_mixed_nalu_types_in_pic_flag" );

  xWriteUvlc( pcPPS->getPicWidthInLumaSamples(), "pps_pic_width_in_luma_samples" );
  xWriteUvlc( pcPPS->getPicHeightInLumaSamples(), "pps_pic_height_in_luma_samples" );

  const Window& conf = pcPPS->getConformanceWindow();
  xWriteFlag(pcPPS->getConformanceWindowFlag(), "pps_conformance_window_flag");
  if (pcPPS->getConformanceWindowFlag())
  {
    xWriteUvlc(conf.getWindowLeftOffset(), "pps_conf_win_left_offset");
    xWriteUvlc(conf.getWindowRightOffset(), "pps_conf_win_right_offset");
    xWriteUvlc(conf.getWindowTopOffset(), "pps_conf_win_top_offset");
    xWriteUvlc(conf.getWindowBottomOffset(), "pps_conf_win_bottom_offset");
  }
  const Window& scalingWindow = pcPPS->getScalingWindow();
  xWriteFlag( pcPPS->getExplicitScalingWindowFlag(), "pps_scaling_window_explicit_signalling_flag");
  if ( pcPPS->getExplicitScalingWindowFlag() )
  {
    xWriteSvlc( scalingWindow.getWindowLeftOffset(), "pps_scaling_win_left_offset" );
    xWriteSvlc( scalingWindow.getWindowRightOffset(), "pps_scaling_win_right_offset" );
    xWriteSvlc( scalingWindow.getWindowTopOffset(), "pps_scaling_win_top_offset" );
    xWriteSvlc( scalingWindow.getWindowBottomOffset(), "pps_scaling_win_bottom_offset" );
  }

  xWriteFlag( pcPPS->getOutputFlagPresentFlag() ? 1 : 0,     "pps_output_flag_present_flag" );
  xWriteFlag( pcPPS->getNoPicPartitionFlag() ? 1 : 0, "pps_no_pic_partition_flag" );
  xWriteFlag( pcPPS->getSubPicIdMappingInPpsFlag() ? 1 : 0, "pps_subpic_id_mapping_present_flag" );
  if( pcPPS->getSubPicIdMappingInPpsFlag() )
  {
    CHECK(pcPPS->getNumSubPics() < 1, "PPS: NumSubPics cannot be less than 1");
    if( !pcPPS->getNoPicPartitionFlag() )
    {
      xWriteUvlc(pcPPS->getNumSubPics() - 1, "pps_num_subpics_minus1");
    }
    CHECK(pcPPS->getSubPicIdLen() < 1, "PPS: SubPicIdLen cannot be less than 1");
    xWriteUvlc( pcPPS->getSubPicIdLen() - 1, "pps_subpic_id_len_minus1" );

    CHECK((1 << pcPPS->getSubPicIdLen()) < pcPPS->getNumSubPics(), "pps_subpic_id_len exceeds valid range");
    for( int picIdx = 0; picIdx < pcPPS->getNumSubPics( ); picIdx++ )
    {
      xWriteCode( pcPPS->getSubPicId(picIdx), pcPPS->getSubPicIdLen( ), "pps_subpic_id[i]" );
    }
  }
  if( !pcPPS->getNoPicPartitionFlag() )
  {
    int colIdx, rowIdx;

    // CTU size - required to match size in SPS
    xWriteCode( pcPPS->getLog2CtuSize() - 5, 2, "pps_log2_ctu_size_minus5" );

    // number of explicit tile columns/rows
    xWriteUvlc( pcPPS->getNumExpTileColumns() - 1, "pps_num_exp_tile_columns_minus1" );
    xWriteUvlc( pcPPS->getNumExpTileRows() - 1,    "pps_num_exp_tile_rows_minus1" );

    // tile sizes
    for( colIdx = 0; colIdx < pcPPS->getNumExpTileColumns(); colIdx++ )
    {
      xWriteUvlc( pcPPS->getTileColumnWidth( colIdx ) - 1, "pps_tile_column_width_minus1[i]" );
    }
    for( rowIdx = 0; rowIdx < pcPPS->getNumExpTileRows(); rowIdx++ )
    {
      xWriteUvlc( pcPPS->getTileRowHeight( rowIdx ) - 1, "pps_tile_row_height_minus1[i]" );
    }

    // rectangular slice signalling
    if (pcPPS->getNumTiles() > 1)
    {
      xWriteFlag(pcPPS->getLoopFilterAcrossTilesEnabledFlag(), "pps_loop_filter_across_tiles_enabled_flag");
      xWriteFlag(pcPPS->getRectSliceFlag() ? 1 : 0, "pps_rect_slice_flag");
    }
    if (pcPPS->getRectSliceFlag())
    {
      xWriteFlag(pcPPS->getSingleSlicePerSubPicFlag( ) ? 1 : 0, "pps_single_slice_per_subpic_flag");
    }
    if (pcPPS->getRectSliceFlag() && !(pcPPS->getSingleSlicePerSubPicFlag()))
    {
      xWriteUvlc( pcPPS->getNumSlicesInPic( ) - 1, "pps_num_slices_in_pic_minus1" );
      if ((pcPPS->getNumSlicesInPic() - 1) > 1)
      {
        xWriteFlag(pcPPS->getTileIdxDeltaPresentFlag() ? 1 : 0, "pps_tile_idx_delta_present_flag");
      }

      // write rectangular slice parameters
      for( int i = 0; i < pcPPS->getNumSlicesInPic()-1; i++ )
      {
        // complete tiles within a single slice
        if( ( pcPPS->getSliceTileIdx( i ) % pcPPS->getNumTileColumns() ) != pcPPS->getNumTileColumns() - 1 )
        {
          xWriteUvlc( pcPPS->getSliceWidthInTiles( i ) - 1, "pps_slice_width_in_tiles_minus1[i]" );
        }

        if( pcPPS->getSliceTileIdx( i ) / pcPPS->getNumTileColumns() != pcPPS->getNumTileRows() - 1 &&
          ( pcPPS->getTileIdxDeltaPresentFlag() || pcPPS->getSliceTileIdx( i ) % pcPPS->getNumTileColumns() == 0 ) )
        {
          xWriteUvlc(pcPPS->getSliceHeightInTiles(i) - 1, "pps_slice_height_in_tiles_minus1[i]");
        }

        // multiple slices within a single tile special case
        if( pcPPS->getSliceWidthInTiles(i) == 1 && pcPPS->getSliceHeightInTiles(i) == 1 && pcPPS->getTileRowHeight(pcPPS->getSliceTileIdx(i) / pcPPS->getNumTileColumns()) > 1 )
        {
          uint32_t numExpSliceInTile = (pcPPS->getNumSlicesInTile(i) == 1) ? 0 : pcPPS->getNumSlicesInTile(i);
          if (numExpSliceInTile > 1
              && pcPPS->getSliceHeightInCtu(i + numExpSliceInTile - 2)
                   >= pcPPS->getSliceHeightInCtu(i + numExpSliceInTile - 1))
          {
            numExpSliceInTile--;
            while (numExpSliceInTile > 1
                   && pcPPS->getSliceHeightInCtu(i + numExpSliceInTile - 2)
                        == pcPPS->getSliceHeightInCtu(i + numExpSliceInTile - 1))
            {
              numExpSliceInTile--;
            }
          }
          uint32_t expSliceHeightSum = 0;
          xWriteUvlc(numExpSliceInTile, "pps_num_exp_slices_in_tile[i]");
          for( int j = 0; j < numExpSliceInTile; j++ )
          {
            xWriteUvlc(pcPPS->getSliceHeightInCtu(i + j) - 1, "pps_exp_slice_height_in_ctus_minus1[i]");
            expSliceHeightSum += pcPPS->getSliceHeightInCtu(i + j);
          }

          CHECK( expSliceHeightSum > pcPPS->getTileRowHeight(pcPPS->getSliceTileIdx(i) / pcPPS->getNumTileColumns()), "The sum of expressed slice heights is larger than the height of the tile containing the slices.");
          i += (pcPPS->getNumSlicesInTile(i) - 1);
        }

        // tile index offset to start of next slice
        if( i < pcPPS->getNumSlicesInPic()-1 )
        {
          if( pcPPS->getTileIdxDeltaPresentFlag() )
          {
            int32_t  tileIdxDelta = pcPPS->getSliceTileIdx( i + 1 ) - pcPPS->getSliceTileIdx( i );
            xWriteSvlc( tileIdxDelta,  "pps_tile_idx_delta[i]" );
          }
        }
      }
    }

    if (pcPPS->getRectSliceFlag() == 0 || pcPPS->getSingleSlicePerSubPicFlag() || pcPPS->getNumSlicesInPic() > 1)
    {
      xWriteFlag(pcPPS->getLoopFilterAcrossSlicesEnabledFlag(), "pps_loop_filter_across_slices_enabled_flag");
    }
  }

  xWriteFlag( pcPPS->getCabacInitPresentFlag() ? 1 : 0,   "pps_cabac_init_present_flag" );
  xWriteUvlc(pcPPS->getNumRefIdxDefaultActive(REF_PIC_LIST_0) - 1, "pps_num_ref_idx_default_active_minus1[0]");
  xWriteUvlc(pcPPS->getNumRefIdxDefaultActive(REF_PIC_LIST_1) - 1, "pps_num_ref_idx_default_active_minus1[1]");
  xWriteFlag( pcPPS->getRpl1IdxPresentFlag() ? 1 : 0,     "pps_rpl1_idx_present_flag");
  xWriteFlag( pcPPS->getUseWP() ? 1 : 0,  "pps_weighted_pred_flag" );   // Use of Weighting Prediction (P_SLICE)
  xWriteFlag( pcPPS->getWPBiPred() ? 1 : 0, "pps_weighted_bipred_flag" );  // Use of Weighting Bi-Prediction (B_SLICE)
  xWriteFlag( pcPPS->getWrapAroundEnabledFlag() ? 1 : 0, "pps_ref_wraparound_enabled_flag" );
  if( pcPPS->getWrapAroundEnabledFlag() )
  {
    xWriteUvlc(pcPPS->getPicWidthMinusWrapAroundOffset(), "pps_pic_width_minus_wraparound_offset");
  }

  xWriteSvlc( pcPPS->getPicInitQPMinus26(),                  "pps_init_qp_minus26");
  xWriteFlag( pcPPS->getUseDQP() ? 1 : 0, "pps_cu_qp_delta_enabled_flag" );
  xWriteFlag(pcPPS->getPPSChromaToolFlag() ? 1 : 0, "pps_chroma_tool_offsets_present_flag");
  if (pcPPS->getPPSChromaToolFlag())
  {
    xWriteSvlc(pcPPS->getQpOffset(COMPONENT_Cb), "pps_cb_qp_offset");
    xWriteSvlc(pcPPS->getQpOffset(COMPONENT_Cr), "pps_cr_qp_offset");
    xWriteFlag(pcPPS->getJointCbCrQpOffsetPresentFlag() ? 1 : 0, "pps_joint_cbcr_qp_offset_present_flag");
    if (pcPPS->getJointCbCrQpOffsetPresentFlag())
    {
      xWriteSvlc(pcPPS->getQpOffset(JOINT_CbCr), "pps_joint_cbcr_qp_offset_value");
    }

    xWriteFlag(pcPPS->getSliceChromaQpFlag() ? 1 : 0, "pps_slice_chroma_qp_offsets_present_flag");

    xWriteFlag(uint32_t(pcPPS->getCuChromaQpOffsetListEnabledFlag()), "pps_cu_chroma_qp_offset_list_enabled_flag");
    if (pcPPS->getCuChromaQpOffsetListEnabledFlag())
    {
      xWriteUvlc(pcPPS->getChromaQpOffsetListLen() - 1, "pps_chroma_qp_offset_list_len_minus1");
      /* skip zero index */
      for (int cuChromaQpOffsetIdx = 0; cuChromaQpOffsetIdx < pcPPS->getChromaQpOffsetListLen(); cuChromaQpOffsetIdx++)
      {
        xWriteSvlc(pcPPS->getChromaQpOffsetListEntry(cuChromaQpOffsetIdx + 1).u.comp.cbOffset,
                   "pps_cb_qp_offset_list[i]");
        xWriteSvlc(pcPPS->getChromaQpOffsetListEntry(cuChromaQpOffsetIdx + 1).u.comp.crOffset,
                   "pps_cr_qp_offset_list[i]");
        if (pcPPS->getJointCbCrQpOffsetPresentFlag())
        {
          xWriteSvlc(pcPPS->getChromaQpOffsetListEntry(cuChromaQpOffsetIdx + 1).u.comp.jointCbCrOffset,
                     "pps_joint_cbcr_qp_offset_list[i]");
        }
      }
    }
  }
  xWriteFlag( pcPPS->getDeblockingFilterControlPresentFlag()?1 : 0,       "pps_deblocking_filter_control_present_flag");
  if(pcPPS->getDeblockingFilterControlPresentFlag())
  {
    xWriteFlag( pcPPS->getDeblockingFilterOverrideEnabledFlag() ? 1 : 0,  "pps_deblocking_filter_override_enabled_flag" );
    xWriteFlag( pcPPS->getPPSDeblockingFilterDisabledFlag() ? 1 : 0,      "pps_deblocking_filter_disabled_flag" );
    if (!pcPPS->getNoPicPartitionFlag() && pcPPS->getDeblockingFilterOverrideEnabledFlag())
    {
      xWriteFlag(pcPPS->getDbfInfoInPhFlag() ? 1 : 0, "pps_dbf_info_in_ph_flag");
    }
    if(!pcPPS->getPPSDeblockingFilterDisabledFlag())
    {
      xWriteSvlc( pcPPS->getDeblockingFilterBetaOffsetDiv2(),             "pps_beta_offset_div2" );
      xWriteSvlc( pcPPS->getDeblockingFilterTcOffsetDiv2(),               "pps_tc_offset_div2" );
      if( pcPPS->getPPSChromaToolFlag() )
      {
        xWriteSvlc( pcPPS->getDeblockingFilterCbBetaOffsetDiv2(),           "pps_cb_beta_offset_div2" );
        xWriteSvlc( pcPPS->getDeblockingFilterCbTcOffsetDiv2(),             "pps_cb_tc_offset_div2" );
        xWriteSvlc( pcPPS->getDeblockingFilterCrBetaOffsetDiv2(),           "pps_cr_beta_offset_div2" );
        xWriteSvlc( pcPPS->getDeblockingFilterCrTcOffsetDiv2(),             "pps_cr_tc_offset_div2" );
      }
    }
  }
  if (!pcPPS->getNoPicPartitionFlag())
  {
    xWriteFlag(pcPPS->getRplInfoInPhFlag() ? 1 : 0, "pps_rpl_info_in_ph_flag");
    xWriteFlag(pcPPS->getSaoInfoInPhFlag() ? 1 : 0, "pps_sao_info_in_ph_flag");
    xWriteFlag(pcPPS->getAlfInfoInPhFlag() ? 1 : 0, "pps_alf_info_in_ph_flag");
    if ((pcPPS->getUseWP() || pcPPS->getWPBiPred()) && pcPPS->getRplInfoInPhFlag())
    {
      xWriteFlag(pcPPS->getWpInfoInPhFlag() ? 1 : 0, "pps_wp_info_in_ph_flag");
    }
    xWriteFlag(pcPPS->getQpDeltaInfoInPhFlag() ? 1 : 0, "pps_qp_delta_info_in_ph_flag");
  }

  xWriteFlag( pcPPS->getPictureHeaderExtensionPresentFlag() ? 1 : 0, "pps_picture_header_extension_present_flag");
  xWriteFlag( pcPPS->getSliceHeaderExtensionPresentFlag() ? 1 : 0, "pps_slice_header_extension_present_flag");

  xWriteFlag(0, "pps_extension_flag");
  xWriteRbspTrailingBits();
}

void HLSWriter::codeAPS( APS* pcAPS )
{
#if ENABLE_TRACING
  xTraceAPSHeader();
#endif

  xWriteCode((int)pcAPS->getAPSType(), 3, "aps_params_type");
  xWriteCode(pcAPS->getAPSId(), 5, "adaptation_parameter_set_id");
  xWriteFlag(pcAPS->chromaPresentFlag, "aps_chroma_present_flag");

  if (pcAPS->getAPSType() == ApsType::ALF)
  {
    codeAlfAps(pcAPS);
  }
  else if (pcAPS->getAPSType() == ApsType::LMCS)
  {
    codeLmcsAps (pcAPS);
  }
  else if (pcAPS->getAPSType() == ApsType::SCALING_LIST)
  {
    codeScalingListAps( pcAPS );
  }
  xWriteFlag(0, "aps_extension_flag");   //Implementation when this flag is equal to 1 should be added when it is needed. Currently in the spec we don't have case when this flag is equal to 1
  xWriteRbspTrailingBits();
}

void HLSWriter::codeAlfAps( APS* pcAPS )
{
  AlfParam param = pcAPS->getAlfAPSParam();

  xWriteFlag(param.newFilterFlag[ChannelType::LUMA], "alf_luma_new_filter");
  if (pcAPS->chromaPresentFlag)
  {
    xWriteFlag(param.newFilterFlag[ChannelType::CHROMA], "alf_chroma_new_filter");
  }

  CcAlfFilterParam paramCcAlf = pcAPS->getCcAlfAPSParam();
  if (pcAPS->chromaPresentFlag)
  {
    xWriteFlag(paramCcAlf.newCcAlfFilter[COMPONENT_Cb - 1], "alf_cc_cb_filter_signal_flag");
    xWriteFlag(paramCcAlf.newCcAlfFilter[COMPONENT_Cr - 1], "alf_cc_cr_filter_signal_flag");
  }

  if (param.newFilterFlag[ChannelType::LUMA])
  {
    xWriteFlag(param.nonLinearFlag[ChannelType::LUMA], "alf_luma_clip");

    xWriteUvlc(param.numLumaFilters - 1, "alf_luma_num_filters_signalled_minus1");
    if (param.numLumaFilters > 1)
    {
      const int length =  ceilLog2( param.numLumaFilters);
      for (int i = 0; i < MAX_NUM_ALF_CLASSES; i++)
      {
        xWriteCode(param.filterCoeffDeltaIdx[i], length, "alf_luma_coeff_delta_idx" );
      }
    }
    alfFilter(param, false, 0);
  }

  if (param.newFilterFlag[ChannelType::CHROMA])
  {
    xWriteFlag(param.nonLinearFlag[ChannelType::CHROMA], "alf_nonlinear_enable_flag_chroma");
    if constexpr (ALF_MAX_NUM_ALTERNATIVES_CHROMA > 1)
    {
      xWriteUvlc( param.numAlternativesChroma - 1, "alf_chroma_num_alts_minus1" );
    }
    for( int altIdx=0; altIdx < param.numAlternativesChroma; ++altIdx )
    {
      alfFilter(param, true, altIdx);
    }
  }
  for (int ccIdx = 0; ccIdx < 2; ccIdx++)
  {
    if (paramCcAlf.newCcAlfFilter[ccIdx])
    {
      const int filterCount = paramCcAlf.ccAlfFilterCount[ccIdx];
      CHECK(filterCount > MAX_NUM_CC_ALF_FILTERS, "CC ALF Filter count is too large");
      CHECK(filterCount == 0, "CC ALF Filter count is too small");

      if (MAX_NUM_CC_ALF_FILTERS > 1)
      {
        xWriteUvlc(filterCount - 1,
                   ccIdx == 0 ? "alf_cc_cb_filters_signalled_minus1" : "alf_cc_cr_filters_signalled_minus1");
      }

      for (int filterIdx = 0; filterIdx < filterCount; filterIdx++)
      {
        AlfFilterShape alfShape(size_CC_ALF);

        const AlfCoeff* coeff = paramCcAlf.ccAlfCoeff[ccIdx][filterIdx];
        // Filter coefficients
        for (int i = 0; i < alfShape.numCoeff - 1; i++)
        {
          if (coeff[i] == 0)
          {
            xWriteCode(0, CCALF_BITS_PER_COEFF_LEVEL,
                       ccIdx == 0 ? "alf_cc_cb_mapped_coeff_abs" : "alf_cc_cr_mapped_coeff_abs");
          }
          else
          {
            xWriteCode(1 + floorLog2(abs(coeff[i])), CCALF_BITS_PER_COEFF_LEVEL,
                       ccIdx == 0 ? "alf_cc_cb_mapped_coeff_abs" : "alf_cc_cr_mapped_coeff_abs");
            xWriteFlag(coeff[i] < 0 ? 1 : 0, ccIdx == 0 ? "alf_cc_cb_coeff_sign" : "alf_cc_cr_coeff_sign");
          }
        }

        DTRACE(g_trace_ctx, D_SYNTAX, "%s coeff filterIdx %d: ", ccIdx == 0 ? "Cb" : "Cr", filterIdx);
        for (int i = 0; i < alfShape.numCoeff; i++)
        {
          DTRACE(g_trace_ctx, D_SYNTAX, "%d ", coeff[i]);
        }
        DTRACE(g_trace_ctx, D_SYNTAX, "\n");
      }
    }
  }
}

void HLSWriter::codeLmcsAps( APS* pcAPS )
{
  SliceReshapeInfo param = pcAPS->getReshaperAPSInfo();
  xWriteUvlc(param.reshaperModelMinBinIdx, "lmcs_min_bin_idx");
  xWriteUvlc(PIC_CODE_CW_BINS - 1 - param.reshaperModelMaxBinIdx, "lmcs_delta_max_bin_idx");
  CHECKD(param.maxNbitsNeededDeltaCW < 1, "maxNbitsNeededDeltaCW must be equal to or greater than 1");
  xWriteUvlc(param.maxNbitsNeededDeltaCW - 1, "lmcs_delta_cw_prec_minus1");

  for (int i = param.reshaperModelMinBinIdx; i <= param.reshaperModelMaxBinIdx; i++)
  {
    int deltaCW = param.reshaperModelBinCWDelta[i];
    int signCW = (deltaCW < 0) ? 1 : 0;
    int absCW = (deltaCW < 0) ? (-deltaCW) : deltaCW;
    xWriteCode(absCW, param.maxNbitsNeededDeltaCW, "lmcs_delta_abs_cw[ i ]");
    if (absCW > 0)
    {
      xWriteFlag(signCW, "lmcs_delta_sign_cw_flag[ i ]");
    }
  }
  int deltaCRS = pcAPS->chromaPresentFlag ? param.chrResScalingOffset : 0;
  int signCRS = (deltaCRS < 0) ? 1 : 0;
  int absCRS = (deltaCRS < 0) ? (-deltaCRS) : deltaCRS;
  if (pcAPS->chromaPresentFlag)
  {
    xWriteCode(absCRS, 3, "lmcs_delta_abs_crs");
  }
  if (absCRS > 0)
  {
    xWriteFlag(signCRS, "lmcs_delta_sign_crs_flag");
  }
}

void HLSWriter::codeScalingListAps( APS* pcAPS )
{
  ScalingList param = pcAPS->getScalingList();
  codeScalingList(param, pcAPS->chromaPresentFlag);
}

void HLSWriter::codeVUI( const VUI *pcVUI, const SPS* pcSPS )
{
#if ENABLE_TRACING
  if( g_HLSTraceEnable )
  {
    DTRACE( g_trace_ctx, D_HEADER, "----------- vui_parameters -----------\n");
  }
#endif


  xWriteFlag(pcVUI->getProgressiveSourceFlag(),   "vui_progressive_source_flag"         );
  xWriteFlag(pcVUI->getInterlacedSourceFlag(),    "vui_interlaced_source_flag"          );
  xWriteFlag(pcVUI->getNonPackedFlag(),           "vui_non_packed_constraint_flag");
  xWriteFlag(pcVUI->getNonProjectedFlag(),        "vui_non_projected_constraint_flag");
  xWriteFlag(pcVUI->getAspectRatioInfoPresentFlag(),            "vui_aspect_ratio_info_present_flag");
  if (pcVUI->getAspectRatioInfoPresentFlag())
  {
    xWriteFlag(pcVUI->getAspectRatioConstantFlag(),             "vui_aspect_ratio_constant_flag");
    xWriteCode(pcVUI->getAspectRatioIdc(), 8,                   "vui_aspect_ratio_idc" );
    if (pcVUI->getAspectRatioIdc() == 255)
    {
      xWriteCode(pcVUI->getSarWidth(), 16,                      "vui_sar_width");
      xWriteCode(pcVUI->getSarHeight(), 16,                     "vui_sar_height");
    }
  }
  xWriteFlag(pcVUI->getOverscanInfoPresentFlag(),               "vui_overscan_info_present_flag");
  if (pcVUI->getOverscanInfoPresentFlag())
  {
    xWriteFlag(pcVUI->getOverscanAppropriateFlag(),             "vui_overscan_appropriate_flag");
  }
  xWriteFlag(pcVUI->getColourDescriptionPresentFlag(),        "vui_colour_description_present_flag");
  if (pcVUI->getColourDescriptionPresentFlag())
  {
    xWriteCode(pcVUI->getColourPrimaries(), 8,                "vui_colour_primaries");
    xWriteCode(pcVUI->getTransferCharacteristics(), 8,        "vui_transfer_characteristics");
#if JVET_AL0301_MATRIXCOEFFS_CONSTRAINTS
    CHECK((pcVUI->getMatrixCoefficients() == 0 || pcVUI->getMatrixCoefficients() == 16 || pcVUI->getMatrixCoefficients() == 17) && pcSPS->getChromaFormatIdc() == ChromaFormat::_444,
    "vui_matrix_coeffs shall not be equal to 0, 16, or 17 when sps_chroma_format_idc is equal to 3"); 
#endif
    xWriteCode(pcVUI->getMatrixCoefficients(), 8,             "vui_matrix_coeffs");
    xWriteFlag(pcVUI->getVideoFullRangeFlag(),                "vui_full_range_flag");
  }
  xWriteFlag(pcVUI->getChromaLocInfoPresentFlag(),              "vui_chroma_loc_info_present_flag");
  if (pcVUI->getChromaLocInfoPresentFlag())
  {
    if(pcVUI->getProgressiveSourceFlag() && !pcVUI->getInterlacedSourceFlag())
    {
      xWriteUvlc(to_underlying(pcVUI->getChromaSampleLocType()), "vui_chroma_sample_loc_type");
    }
    else
    {
      xWriteUvlc(to_underlying(pcVUI->getChromaSampleLocTypeTopField()), "vui_chroma_sample_loc_type_top_field");
      xWriteUvlc(to_underlying(pcVUI->getChromaSampleLocTypeBottomField()), "vui_chroma_sample_loc_type_bottom_field");
    }
  }
  if(!isByteAligned())
  {
    xWriteFlag(1, "vui_payload_bit_equal_to_one");
    while(!isByteAligned())
    {
      xWriteFlag(0, "vui_payload_bit_equal_to_zero");
    }
  }
}

void HLSWriter::codeGeneralHrdparameters(const GeneralHrdParams * hrd)
{
  xWriteCode(hrd->getNumUnitsInTick(), 32, "num_units_in_tick");
  xWriteCode(hrd->getTimeScale(), 32, "time_scale");
  xWriteFlag(hrd->getGeneralNalHrdParametersPresentFlag() ? 1 : 0, "general_nal_hrd_parameters_present_flag");
  xWriteFlag(hrd->getGeneralVclHrdParametersPresentFlag() ? 1 : 0, "general_vcl_hrd_parameters_present_flag");
  if( hrd->getGeneralNalHrdParametersPresentFlag() || hrd->getGeneralVclHrdParametersPresentFlag() )
  {
    xWriteFlag(hrd->getGeneralSamePicTimingInAllOlsFlag() ? 1 : 0, "general_same_pic_timing_in_all_ols_flag");
    xWriteFlag(hrd->getGeneralDecodingUnitHrdParamsPresentFlag() ? 1 : 0, "general_decoding_unit_hrd_params_present_flag");
    if (hrd->getGeneralDecodingUnitHrdParamsPresentFlag())
    {
      xWriteCode(hrd->getTickDivisorMinus2(), 8, "tick_divisor_minus2");
    }
    xWriteCode(hrd->getBitRateScale(), 4, "bit_rate_scale");
    xWriteCode(hrd->getCpbSizeScale(), 4, "cpb_size_scale");
    if (hrd->getGeneralDecodingUnitHrdParamsPresentFlag())
    {
      xWriteCode(hrd->getCpbSizeDuScale(), 4, "cpb_size_du_scale");
    }
    xWriteUvlc(hrd->getHrdCpbCntMinus1(), "hrd_cpb_cnt_minus1");
  }
}
void HLSWriter::codeOlsHrdParameters(const GeneralHrdParams * generalHrd, const OlsHrdParams *olsHrd, const uint32_t firstSubLayer, const uint32_t maxNumSubLayersMinus1)
{

  for( int i = firstSubLayer; i <= maxNumSubLayersMinus1; i ++ )
  {
    const OlsHrdParams *hrd = &(olsHrd[i]);
    xWriteFlag(hrd->getFixedPicRateGeneralFlag() ? 1 : 0, "fixed_pic_rate_general_flag");

    if (!hrd->getFixedPicRateGeneralFlag())
    {
      xWriteFlag(hrd->getFixedPicRateWithinCvsFlag() ? 1 : 0, "fixed_pic_rate_within_cvs_flag");
    }
    if (hrd->getFixedPicRateWithinCvsFlag())
    {
      const uint32_t elementDurationInTc = hrd->getElementDurationInTc();
      CHECK(elementDurationInTc < 1 || elementDurationInTc > 2048, "elementDurationInTc is out of range");
      xWriteUvlc(elementDurationInTc - 1, "elemental_duration_in_tc_minus1");
    }
    else if ( (generalHrd->getGeneralNalHrdParametersPresentFlag() || generalHrd->getGeneralVclHrdParametersPresentFlag()) && generalHrd->getHrdCpbCntMinus1() == 0)
    {
      xWriteFlag(hrd->getLowDelayHrdFlag() ? 1 : 0, "low_delay_hrd_flag");
    }

    for( int nalOrVcl = 0; nalOrVcl < 2; nalOrVcl ++ )
    {
      if (((nalOrVcl == 0) && (generalHrd->getGeneralNalHrdParametersPresentFlag())) || ((nalOrVcl == 1) && (generalHrd->getGeneralVclHrdParametersPresentFlag())))
      {
        for (int j = 0; j <= (generalHrd->getHrdCpbCntMinus1()); j++)
        {
          xWriteUvlc(hrd->getBitRateValueMinus1(j, nalOrVcl), "bit_rate_value_minus1");
          xWriteUvlc(hrd->getCpbSizeValueMinus1(j, nalOrVcl), "cpb_size_value_minus1");
          if (generalHrd->getGeneralDecodingUnitHrdParamsPresentFlag())
          {
            xWriteUvlc(hrd->getDuCpbSizeValueMinus1(j, nalOrVcl), "cpb_size_du_value_minus1");
            xWriteUvlc(hrd->getDuBitRateValueMinus1(j, nalOrVcl), "bit_rate_du_value_minus1");
          }
          xWriteFlag(hrd->getCbrFlag(j, nalOrVcl) ? 1 : 0, "cbr_flag");
        }
      }
    }
  }
}

void HLSWriter::dpb_parameters(int maxSubLayersMinus1, bool subLayerInfoFlag, const SPS *pcSPS)
{
  for (uint32_t i = (subLayerInfoFlag ? 0 : maxSubLayersMinus1); i <= maxSubLayersMinus1; i++)
  {
    CHECK(pcSPS->getMaxDecPicBuffering(i) < 1, "MaxDecPicBuffering must be greater than 0");
    xWriteUvlc(pcSPS->getMaxDecPicBuffering(i) - 1, "dpb_max_dec_pic_buffering_minus1[i]");
    xWriteUvlc(pcSPS->getMaxNumReorderPics(i), "dpb_max_num_reorder_pics[i]");
    xWriteUvlc(pcSPS->getMaxLatencyIncreasePlus1(i), "dpb_max_latency_increase_plus1[i]");
  }
}

void HLSWriter::codeSPS( const SPS* pcSPS )
{
#if ENABLE_TRACING
  xTraceSPSHeader ();
#endif
  xWriteCode(pcSPS->getSPSId(), 4, "sps_seq_parameter_set_id");
  xWriteCode( pcSPS->getVPSId(), 4, "sps_video_parameter_set_id" );
  CHECK(pcSPS->getMaxTLayers() == 0, "Maximum number of temporal sub-layers is '0'");

  xWriteCode(pcSPS->getMaxTLayers() - 1, 3, "sps_max_sub_layers_minus1");
  xWriteCode(int(pcSPS->getChromaFormatIdc()), 2, "sps_chroma_format_idc");
  xWriteCode(floorLog2(pcSPS->getCTUSize()) - 5, 2, "sps_log2_ctu_size_minus5");
  xWriteFlag(pcSPS->getPtlDpbHrdParamsPresentFlag(), "sps_ptl_dpb_hrd_params_present_flag");

  if( !pcSPS->getVPSId() )
  {
    CHECK( !pcSPS->getPtlDpbHrdParamsPresentFlag(), "When sps_video_parameter_set_id is equal to 0, the value of sps_ptl_dpb_hrd_params_present_flag shall be equal to 1" );
  }

  if (pcSPS->getPtlDpbHrdParamsPresentFlag())
  {
    codeProfileTierLevel(pcSPS->getProfileTierLevel(), true, pcSPS->getMaxTLayers() - 1);
  }

  xWriteFlag(pcSPS->getGDREnabledFlag(), "sps_gdr_enabled_flag");

  xWriteFlag(pcSPS->getRprEnabledFlag(), "sps_ref_pic_resampling_enabled_flag");
  if (pcSPS->getRprEnabledFlag())
  {
    xWriteFlag(pcSPS->getResChangeInClvsEnabledFlag(), "sps_res_change_in_clvs_allowed_flag");
  }
  CHECK(!pcSPS->getRprEnabledFlag() && pcSPS->getResChangeInClvsEnabledFlag(), "When sps_ref_pic_resampling_enabled_flag is equal to 0, sps_res_change_in_clvs_allowed_flag shall be equal to 0");

  xWriteUvlc( pcSPS->getMaxPicWidthInLumaSamples(), "sps_pic_width_max_in_luma_samples" );
  xWriteUvlc( pcSPS->getMaxPicHeightInLumaSamples(), "sps_pic_height_max_in_luma_samples" );
  const Window& conf = pcSPS->getConformanceWindow();
  xWriteFlag(!conf.isZero(), "sps_conformance_window_flag");
  if (!conf.isZero())
  {
    xWriteUvlc(conf.getWindowLeftOffset(), "sps_conf_win_left_offset");
    xWriteUvlc(conf.getWindowRightOffset(), "sps_conf_win_right_offset");
    xWriteUvlc(conf.getWindowTopOffset(), "sps_conf_win_top_offset");
    xWriteUvlc(conf.getWindowBottomOffset(), "sps_conf_win_bottom_offset");
  }


  xWriteFlag(pcSPS->getSubPicInfoPresentFlag(), "sps_subpic_info_present_flag");

  if (pcSPS->getSubPicInfoPresentFlag())
  {
    CHECK(pcSPS->getNumSubPics() < 1, "SPS: NumSubPics cannot be less than 1");
    xWriteUvlc(pcSPS->getNumSubPics() - 1, "sps_num_subpics_minus1");
    if( pcSPS->getNumSubPics() > 1 )
    {
      xWriteFlag(pcSPS->getIndependentSubPicsFlag(), "sps_independent_subpics_flag");
      xWriteFlag(pcSPS->getSubPicSameSizeFlag(), "sps_subpic_same_size_flag");
      uint32_t tmpWidthVal = (pcSPS->getMaxPicWidthInLumaSamples() + pcSPS->getCTUSize() - 1) / pcSPS->getCTUSize();
      uint32_t tmpHeightVal = (pcSPS->getMaxPicHeightInLumaSamples() + pcSPS->getCTUSize() - 1) / pcSPS->getCTUSize();
      for (int picIdx = 0; picIdx < pcSPS->getNumSubPics(); picIdx++)
      {
        if (!pcSPS->getSubPicSameSizeFlag() || picIdx == 0)
        {
          if ((picIdx > 0) && (pcSPS->getMaxPicWidthInLumaSamples() > pcSPS->getCTUSize()))
          {
            xWriteCode(pcSPS->getSubPicCtuTopLeftX(picIdx), ceilLog2(tmpWidthVal), "sps_subpic_ctu_top_left_x[ i ]");
          }
          if ((picIdx > 0) && (pcSPS->getMaxPicHeightInLumaSamples() > pcSPS->getCTUSize()))
          {
            xWriteCode(pcSPS->getSubPicCtuTopLeftY(picIdx), ceilLog2(tmpHeightVal), "sps_subpic_ctu_top_left_y[ i ]");
          }
          if (picIdx<pcSPS->getNumSubPics() - 1 && pcSPS->getMaxPicWidthInLumaSamples() > pcSPS->getCTUSize())
          {
            xWriteCode(pcSPS->getSubPicWidth(picIdx) - 1, ceilLog2(tmpWidthVal), "sps_subpic_width_minus1[ i ]");
          }
          if (picIdx<pcSPS->getNumSubPics() - 1 && pcSPS->getMaxPicHeightInLumaSamples() > pcSPS->getCTUSize())
          {
            xWriteCode(pcSPS->getSubPicHeight(picIdx) - 1, ceilLog2(tmpHeightVal), "sps_subpic_height_minus1[ i ]");
          }
        }
        if (!pcSPS->getIndependentSubPicsFlag())
        {
          xWriteFlag(pcSPS->getSubPicTreatedAsPicFlag(picIdx), "sps_subpic_treated_as_pic_flag[ i ]");
          xWriteFlag(pcSPS->getLoopFilterAcrossSubpicEnabledFlag(picIdx), "sps_loop_filter_across_subpic_enabled_flag[ i ]");
        }
      }
    }

    CHECK(pcSPS->getSubPicIdLen() < 1, "SPS: SubPicIdLen cannot be less than 1");
    xWriteUvlc(pcSPS->getSubPicIdLen() - 1, "sps_subpic_id_len_minus1");
    xWriteFlag(pcSPS->getSubPicIdMappingExplicitlySignalledFlag(), "sps_subpic_id_mapping_explicitly_signalled_flag");
    if (pcSPS->getSubPicIdMappingExplicitlySignalledFlag())
    {
      xWriteFlag(pcSPS->getSubPicIdMappingPresentFlag(), "sps_subpic_id_mapping_present_flag");
      if (pcSPS->getSubPicIdMappingPresentFlag())
      {
        for (int picIdx = 0; picIdx < pcSPS->getNumSubPics(); picIdx++)
        {
          xWriteCode(pcSPS->getSubPicId(picIdx), pcSPS->getSubPicIdLen(), "sps_subpic_id[i]");
        }
      }
    }
  }

  const Profile::Name profile = pcSPS->getProfileTierLevel()->getProfileIdc();
  if (profile != Profile::NONE)
  {
    CHECK(pcSPS->getBitDepth(ChannelType::LUMA) > ProfileFeatures::getProfileFeatures(profile)->maxBitDepth,
          "sps_bitdepth_minus8 exceeds range supported by signalled profile");
  }
  xWriteUvlc(pcSPS->getBitDepth(ChannelType::LUMA) - 8, "sps_bitdepth_minus8");
  xWriteFlag( pcSPS->getEntropyCodingSyncEnabledFlag() ? 1 : 0, "sps_entropy_coding_sync_enabled_flag" );
  xWriteFlag( pcSPS->getEntryPointsPresentFlag() ? 1 : 0, "sps_entry_point_offsets_present_flag" );
  xWriteCode(pcSPS->getBitsForPOC()-4, 4, "sps_log2_max_pic_order_cnt_lsb_minus4");

  xWriteFlag(pcSPS->getPocMsbCycleFlag() ? 1 : 0, "sps_poc_msb_cycle_flag");
  if (pcSPS->getPocMsbCycleFlag())
  {
    xWriteUvlc(pcSPS->getPocMsbCycleLen() - 1, "sps_poc_msb_cycle_len_minus1");
  }
  // extra bits are for future extensions, so these are currently hard coded to not being sent
  xWriteCode(0, 2, "sps_num_extra_ph_bytes");
  // for( i = 0; i < (sps_num_extra_ph_bytes * 8 ); i++ )
  //   sps_extra_ph_bit_present_flag[ i ]
  xWriteCode(0, 2, "sps_num_extra_sh_bytes");
  // for( i = 0; i < (sps_num_extra_sh_bytes * 8 ); i++ )
  //   sps_extra_sh_bit_present_flag[ i ]

  if (pcSPS->getPtlDpbHrdParamsPresentFlag())
  {
    if (pcSPS->getMaxTLayers() - 1 > 0)
    {
      xWriteFlag(pcSPS->getSubLayerDpbParamsFlag(), "sps_sublayer_dpb_params_flag");
    }
    dpb_parameters(pcSPS->getMaxTLayers() - 1, pcSPS->getSubLayerDpbParamsFlag(), pcSPS);
  }
  CHECK( pcSPS->getMaxCUWidth() != pcSPS->getMaxCUHeight(),                          "Rectangular CTUs not supported" );
  xWriteUvlc(pcSPS->getLog2MinCodingBlockSize() - 2, "sps_log2_min_luma_coding_block_size_minus2");
  xWriteFlag(pcSPS->getSplitConsOverrideEnabledFlag(), "sps_partition_constraints_override_enabled_flag");
  xWriteUvlc(floorLog2(pcSPS->getMinQTSize(I_SLICE)) - pcSPS->getLog2MinCodingBlockSize(), "sps_log2_diff_min_qt_min_cb_intra_slice_luma");
  xWriteUvlc(pcSPS->getMaxMTTHierarchyDepthI(), "sps_max_mtt_hierarchy_depth_intra_slice_luma");
  if (pcSPS->getMaxMTTHierarchyDepthI() != 0)
  {
    xWriteUvlc(floorLog2(pcSPS->getMaxBTSizeI()) - floorLog2(pcSPS->getMinQTSize(I_SLICE)), "sps_log2_diff_max_bt_min_qt_intra_slice_luma");
    xWriteUvlc(floorLog2(pcSPS->getMaxTTSizeI()) - floorLog2(pcSPS->getMinQTSize(I_SLICE)), "sps_log2_diff_max_tt_min_qt_intra_slice_luma");
  }
  if (isChromaEnabled(pcSPS->getChromaFormatIdc()))
  {
    xWriteFlag(pcSPS->getUseDualITree(), "sps_qtbtt_dual_tree_intra_flag");
  }
  if (pcSPS->getUseDualITree())
  {
    xWriteUvlc(floorLog2(pcSPS->getMinQTSize(I_SLICE, ChannelType::CHROMA)) - pcSPS->getLog2MinCodingBlockSize(),
               "sps_log2_diff_min_qt_min_cb_intra_slice_chroma");
    xWriteUvlc(pcSPS->getMaxMTTHierarchyDepthIChroma(), "sps_max_mtt_hierarchy_depth_intra_slice_chroma");
    if (pcSPS->getMaxMTTHierarchyDepthIChroma() != 0)
    {
      xWriteUvlc(floorLog2(pcSPS->getMaxBTSizeIChroma()) - floorLog2(pcSPS->getMinQTSize(I_SLICE, ChannelType::CHROMA)),
                 "sps_log2_diff_max_bt_min_qt_intra_slice_chroma");
      xWriteUvlc(floorLog2(pcSPS->getMaxTTSizeIChroma()) - floorLog2(pcSPS->getMinQTSize(I_SLICE, ChannelType::CHROMA)),
                 "sps_log2_diff_max_tt_min_qt_intra_slice_chroma");
    }
  }
  xWriteUvlc(floorLog2(pcSPS->getMinQTSize(B_SLICE)) - pcSPS->getLog2MinCodingBlockSize(), "sps_log2_diff_min_qt_min_cb_inter_slice");
  xWriteUvlc(pcSPS->getMaxMTTHierarchyDepth(), "sps_max_mtt_hierarchy_depth_inter_slice");
  if (pcSPS->getMaxMTTHierarchyDepth() != 0)
  {
    xWriteUvlc(floorLog2(pcSPS->getMaxBTSize()) - floorLog2(pcSPS->getMinQTSize(B_SLICE)), "sps_log2_diff_max_bt_min_qt_inter_slice");
    xWriteUvlc(floorLog2(pcSPS->getMaxTTSize()) - floorLog2(pcSPS->getMinQTSize(B_SLICE)), "sps_log2_diff_max_tt_min_qt_inter_slice");
  }
  if (pcSPS->getCTUSize() > 32)
  {
    xWriteFlag( (pcSPS->getLog2MaxTbSize() - 5) ? 1 : 0,                       "sps_max_luma_transform_size_64_flag" );
  }

  xWriteFlag(pcSPS->getTransformSkipEnabledFlag() ? 1 : 0, "sps_transform_skip_enabled_flag");
  if (pcSPS->getTransformSkipEnabledFlag())
  {
    xWriteUvlc(pcSPS->getLog2MaxTransformSkipBlockSize() - 2, "sps_log2_transform_skip_max_size_minus2");
    xWriteFlag(pcSPS->getBDPCMEnabledFlag() ? 1 : 0, "sps_bdpcm_enabled_flag");
  }
  else
  {
    CHECK(pcSPS->getBDPCMEnabledFlag(), "BDPCM cannot be used when transform skip is disabled");
  }
  xWriteFlag(pcSPS->getMtsEnabled() ? 1 : 0, "sps_mts_enabled_flag");
  if (pcSPS->getMtsEnabled())
  {
    xWriteFlag(pcSPS->getExplicitMtsIntraEnabled() ? 1 : 0, "sps_explicit_mts_intra_enabled_flag");
    xWriteFlag(pcSPS->getExplicitMtsInterEnabled() ? 1 : 0, "sps_explicit_mts_inter_enabled_flag");
  }
  xWriteFlag(pcSPS->getUseLFNST() ? 1 : 0, "sps_lfnst_enabled_flag");

  if (isChromaEnabled(pcSPS->getChromaFormatIdc()))
  {
    xWriteFlag(pcSPS->getJointCbCrEnabledFlag(), "sps_joint_cbcr_enabled_flag");
    const ChromaQpMappingTable& chromaQpMappingTable = pcSPS->getChromaQpMappingTable();
    xWriteFlag(chromaQpMappingTable.getSameCQPTableForAllChromaFlag(), "sps_same_qp_table_for_chroma_flag");
    int numQpTables = chromaQpMappingTable.getSameCQPTableForAllChromaFlag() ? 1 : (pcSPS->getJointCbCrEnabledFlag() ? 3 : 2);
    CHECK(numQpTables != chromaQpMappingTable.getNumQpTables(), " numQpTables does not match at encoder side ");
    for (int i = 0; i < numQpTables; i++)
    {
      xWriteSvlc(chromaQpMappingTable.getQpTableStartMinus26(i), "sps_qp_table_starts_minus26");
      xWriteUvlc(chromaQpMappingTable.getNumPtsInCQPTableMinus1(i), "sps_num_points_in_qp_table_minus1");

      for (int j = 0; j <= chromaQpMappingTable.getNumPtsInCQPTableMinus1(i); j++)
      {
        xWriteUvlc(chromaQpMappingTable.getDeltaQpInValMinus1(i, j), "sps_delta_qp_in_val_minus1");
        xWriteUvlc(chromaQpMappingTable.getDeltaQpOutVal(i, j) ^ chromaQpMappingTable.getDeltaQpInValMinus1(i, j),
                   "sps_delta_qp_diff_val");
      }
    }
  }

  xWriteFlag( pcSPS->getSAOEnabledFlag(),                                            "sps_sao_enabled_flag");
  xWriteFlag( pcSPS->getALFEnabledFlag(),                                            "sps_alf_enabled_flag" );
  if (pcSPS->getALFEnabledFlag() && isChromaEnabled(pcSPS->getChromaFormatIdc()))
  {
    xWriteFlag( pcSPS->getCCALFEnabledFlag(),                                            "sps_ccalf_enabled_flag" );
  }
  xWriteFlag(pcSPS->getUseLmcs() ? 1 : 0, "sps_lmcs_enable_flag");
  xWriteFlag(pcSPS->getUseWP() ? 1 : 0, "sps_weighted_pred_flag");           // Use of Weighting Prediction (P_SLICE)
  xWriteFlag(pcSPS->getUseWPBiPred() ? 1 : 0, "sps_weighted_bipred_flag");   // Use of Weighting Bi-Prediction (B_SLICE)

  xWriteFlag(pcSPS->getLongTermRefsPresent() ? 1 : 0, "sps_long_term_ref_pics_flag");
  if( pcSPS->getVPSId() > 0 )
  {
    xWriteFlag( pcSPS->getInterLayerPresentFlag() ? 1 : 0, "sps_inter_layer_prediction_enabled_flag" );
  }
  xWriteFlag(pcSPS->getIDRRefParamListPresent() ? 1 : 0, "sps_idr_rpl_present_flag" );
  xWriteFlag(pcSPS->getRPL1CopyFromRPL0Flag() ? 1 : 0, "sps_rpl1_same_as_rpl0_flag");

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    if (l == REF_PIC_LIST_1 && pcSPS->getRPL1CopyFromRPL0Flag())
    {
      continue;
    }
    const RPLList *rplList = pcSPS->getRplList(l);
    const int      numRpl  = pcSPS->getNumRpl(l);
    xWriteUvlc(numRpl, l == REF_PIC_LIST_0 ? "sps_num_ref_pic_lists[0]" : "sps_num_ref_pic_lists[1]");

    for (int rplIdx = 0; rplIdx < numRpl; rplIdx++)
    {
      const ReferencePictureList *rpl = rplList->getReferencePictureList(rplIdx);
      xCodeRefPicList(rpl, pcSPS->getLongTermRefsPresent(), pcSPS->getBitsForPOC(),
                      !pcSPS->getUseWP() && !pcSPS->getUseWPBiPred(), rplIdx);
    }
  }

  xWriteFlag( pcSPS->getWrapAroundEnabledFlag() ? 1 : 0,                              "sps_ref_wraparound_enabled_flag" );

  xWriteFlag( pcSPS->getSPSTemporalMVPEnabledFlag()  ? 1 : 0,                        "sps_temporal_mvp_enabled_flag" );

  if ( pcSPS->getSPSTemporalMVPEnabledFlag() )
  {
    xWriteFlag(pcSPS->getSbTMVPEnabledFlag() ? 1 : 0, "sps_sbtmvp_enabled_flag");
  }

  xWriteFlag( pcSPS->getAMVREnabledFlag() ? 1 : 0,                                   "sps_amvr_enabled_flag" );

  xWriteFlag( pcSPS->getBDOFEnabledFlag() ? 1 : 0,                                   "sps_bdof_enabled_flag" );
  if (pcSPS->getBDOFEnabledFlag())
  {
    xWriteFlag(pcSPS->getBdofControlPresentInPhFlag() ? 1 : 0,                        "sps_bdof_control_present_in_ph_flag");
  }
  xWriteFlag( pcSPS->getUseSMVD() ? 1 : 0,                                            "sps_smvd_enabled_flag" );
  xWriteFlag( pcSPS->getUseDMVR() ? 1 : 0,                                            "sps_dmvr_enabled_flag" );
  if (pcSPS->getUseDMVR())
  {
    xWriteFlag(pcSPS->getDmvrControlPresentInPhFlag() ? 1 : 0,                        "sps_dmvr_control_present_in_ph_flag");
  }
  xWriteFlag(pcSPS->getUseMMVD() ? 1 : 0,                                             "sps_mmvd_enabled_flag");
  if (pcSPS->getUseMMVD())
  {
    xWriteFlag(pcSPS->getFpelMmvdEnabledFlag() ? 1 : 0,                               "sps_mmvd_fullpel_only_flag");
  }
  xWriteUvlc(MRG_MAX_NUM_CANDS - pcSPS->getMaxNumMergeCand(), "sps_six_minus_max_num_merge_cand");
  xWriteFlag( pcSPS->getUseSBT() ? 1 : 0,                                                      "sps_sbt_enabled_flag");
  xWriteFlag( pcSPS->getUseAffine() ? 1 : 0,                                                   "sps_affine_enabled_flag" );
  if ( pcSPS->getUseAffine() )
  {
    xWriteUvlc(AFFINE_MRG_MAX_NUM_CANDS - pcSPS->getMaxNumAffineMergeCand(), "sps_five_minus_max_num_subblock_merge_cand");
    xWriteFlag( pcSPS->getUseAffineType() ? 1 : 0,                                             "sps_affine_type_flag" );
    if (pcSPS->getAMVREnabledFlag())
    {
      xWriteFlag( pcSPS->getAffineAmvrEnabledFlag() ? 1 : 0,                                     "sps_affine_amvr_enabled_flag" );
    }
    xWriteFlag( pcSPS->getUsePROF() ? 1 : 0,                                                   "sps_affine_prof_enabled_flag" );
    if (pcSPS->getUsePROF())
    {
      xWriteFlag(pcSPS->getProfControlPresentInPhFlag() ? 1 : 0,                                   "sps_prof_control_present_in_ph_flag" );
    }
  }

  xWriteFlag(pcSPS->getUseBcw() ? 1 : 0, "sps_bcw_enabled_flag");

  xWriteFlag( pcSPS->getUseCiip() ? 1 : 0,                                                  "sps_ciip_enabled_flag" );
  if (pcSPS->getMaxNumMergeCand() >= 2)
  {
    xWriteFlag(pcSPS->getUseGeo() ? 1 : 0, "sps_gpm_enabled_flag");
    if (pcSPS->getUseGeo())
    {
      CHECK(pcSPS->getMaxNumMergeCand() < pcSPS->getMaxNumGeoCand(),
            "The number of GPM candidates must not be greater than the number of merge candidates");
      CHECK(2 > pcSPS->getMaxNumGeoCand(),
            "The number of GPM candidates must not be smaller than 2");
      if (pcSPS->getMaxNumMergeCand() >= 3)
      {
        xWriteUvlc(pcSPS->getMaxNumMergeCand() - pcSPS->getMaxNumGeoCand(),
                   "sps_max_num_merge_cand_minus_max_num_gpm_cand");
      }
    }
  }

  xWriteUvlc(pcSPS->getLog2ParallelMergeLevelMinus2(), "sps_log2_parallel_merge_level_minus2");

  xWriteFlag( pcSPS->getUseISP() ? 1 : 0,                                             "sps_isp_enabled_flag");
  xWriteFlag( pcSPS->getUseMRL() ? 1 : 0,                                             "sps_mrl_enabled_flag");
  xWriteFlag( pcSPS->getUseMIP() ? 1 : 0,                                             "sps_mip_enabled_flag");
  if (isChromaEnabled(pcSPS->getChromaFormatIdc()))
  {
    xWriteFlag( pcSPS->getUseLMChroma() ? 1 : 0,                                      "sps_cclm_enabled_flag");
  }
  if (pcSPS->getChromaFormatIdc() == ChromaFormat::_420)
  {
    xWriteFlag( pcSPS->getHorCollocatedChromaFlag() ? 1 : 0, "sps_chroma_horizontal_collocated_flag" );
    xWriteFlag( pcSPS->getVerCollocatedChromaFlag() ? 1 : 0, "sps_chroma_vertical_collocated_flag" );
  }
  else
  {
    CHECK(pcSPS->getHorCollocatedChromaFlag() != 1, "Invalid value for horizontal collocated chroma flag");
    CHECK(pcSPS->getVerCollocatedChromaFlag() != 1, "Invalid value for vertical collocated chroma flag");
  }
  CHECK(pcSPS->getMaxNumMergeCand() > MRG_MAX_NUM_CANDS, "More merge candidates signalled than supported");
  xWriteFlag(pcSPS->getPLTMode() ? 1 : 0,                                                    "sps_palette_enabled_flag" );
  if (pcSPS->getChromaFormatIdc() == ChromaFormat::_444 && pcSPS->getLog2MaxTbSize() != 6)
  {
    xWriteFlag(pcSPS->getUseColorTrans() ? 1 : 0, "sps_act_enabled_flag");
  }
  if (pcSPS->getTransformSkipEnabledFlag() || pcSPS->getPLTMode())
  {
    xWriteUvlc(pcSPS->getInternalMinusInputBitDepth(ChannelType::LUMA), "sps_internal_bit_depth_minus_input_bit_depth");
  }
  xWriteFlag(pcSPS->getIBCFlag() ? 1 : 0,                                                      "sps_ibc_enabled_flag");
  if (pcSPS->getIBCFlag())
  {
    CHECK(pcSPS->getMaxNumIBCMergeCand() > IBC_MRG_MAX_NUM_CANDS, "More IBC merge candidates signalled than supported");
    xWriteUvlc(IBC_MRG_MAX_NUM_CANDS - pcSPS->getMaxNumIBCMergeCand(), "sps_six_minus_max_num_ibc_merge_cand");
  }
  xWriteFlag( pcSPS->getLadfEnabled() ? 1 : 0,                                                 "sps_ladf_enabled_flag" );
  if ( pcSPS->getLadfEnabled() )
  {
    xWriteCode( pcSPS->getLadfNumIntervals() - 2, 2,                                           "sps_num_ladf_intervals_minus2" );
    xWriteSvlc( pcSPS->getLadfQpOffset( 0 ),                                                   "sps_ladf_lowest_interval_qp_offset");
    for ( int k = 1; k< pcSPS->getLadfNumIntervals(); k++ )
    {
      xWriteSvlc( pcSPS->getLadfQpOffset( k ),                                                 "sps_ladf_qp_offset" );
      xWriteUvlc( pcSPS->getLadfIntervalLowerBound( k ) - pcSPS->getLadfIntervalLowerBound( k - 1 ) - 1, "sps_ladf_delta_threshold_minus1" );
    }
  }
  // KJS: reference picture sets to be replaced


  // KJS: remove scaling lists?
  xWriteFlag( pcSPS->getScalingListFlag() ? 1 : 0,                                   "sps_explicit_scaling_list_enabled_flag" );

  if (pcSPS->getUseLFNST() && pcSPS->getScalingListFlag())
  {
    xWriteFlag(pcSPS->getDisableScalingMatrixForLfnstBlks(), "sps_scaling_matrix_for_lfnst_disabled_flag");
  }

  if (pcSPS->getUseColorTrans() && pcSPS->getScalingListFlag())
  {
    xWriteFlag(pcSPS->getScalingMatrixForAlternativeColourSpaceDisabledFlag(), "sps_scaling_matrix_for_alternative_colour_space_disabled_flag");
  }
  if (pcSPS->getScalingMatrixForAlternativeColourSpaceDisabledFlag())
  {
    xWriteFlag(pcSPS->getScalingMatrixDesignatedColourSpaceFlag(), "sps_scaling_matrix_designated_colour_space_flag");
  }
  xWriteFlag(pcSPS->getDepQuantEnabledFlag(), "sps_dep_quant_enabled_flag");
  xWriteFlag(pcSPS->getSignDataHidingEnabledFlag(), "sps_sign_data_hiding_enabled_flag");

  xWriteFlag( pcSPS->getVirtualBoundariesEnabledFlag(), "sps_virtual_boundaries_enabled_flag" );
  if( pcSPS->getVirtualBoundariesEnabledFlag() )
  {
    xWriteFlag( pcSPS->getVirtualBoundariesPresentFlag(), "sps_loop_filter_across_virtual_boundaries_present_flag" );
    if( pcSPS->getVirtualBoundariesPresentFlag() )
    {
      xWriteUvlc( pcSPS->getNumVerVirtualBoundaries(), "sps_num_ver_virtual_boundaries");
      if (pcSPS->getMaxPicWidthInLumaSamples() <= 8)
      {
        CHECK(pcSPS->getNumVerVirtualBoundaries() != 0, "SPS: When picture width is less than or equal to 8, the number of vertical virtual boundaries shall be equal to 0");
      }
      else
      {
        CHECK(pcSPS->getNumVerVirtualBoundaries() > 3, "SPS: The number of vertical virtual boundaries shall be in the range of 0 to 3");
      }
      for( unsigned i = 0; i < pcSPS->getNumVerVirtualBoundaries(); i++ )
      {
        xWriteUvlc((pcSPS->getVirtualBoundariesPosX(i)>>3) - 1, "sps_virtual_boundary_pos_x_minus1[i]");
        CHECK(((pcSPS->getVirtualBoundariesPosX(i)>>3) - 1) > (((pcSPS->getMaxPicWidthInLumaSamples() + 7) >> 3) - 2), "The value of sps_virtual_boundary_pos_x_minus1[ i ] shall be in the range of 0 to Ceil( sps_pic_width_max_in_luma_samples / 8 ) - 2, inclusive.");
      }
      xWriteUvlc(pcSPS->getNumHorVirtualBoundaries(), "sps_num_hor_virtual_boundaries");
      if (pcSPS->getMaxPicHeightInLumaSamples() <= 8)
      {
        CHECK(pcSPS->getNumHorVirtualBoundaries() != 0, "SPS: When picture height is less than or equal to 8, the number of horizontal virtual boundaries shall be equal to 0");
      }
      else
      {
        CHECK(pcSPS->getNumHorVirtualBoundaries() > 3, "SPS: The number of horizontal virtual boundaries shall be in the range of 0 to 3");
      }
      for( unsigned i = 0; i < pcSPS->getNumHorVirtualBoundaries(); i++ )
      {
        xWriteUvlc((pcSPS->getVirtualBoundariesPosY(i)>>3) - 1, "sps_virtual_boundary_pos_y_minus1[i]");
        CHECK(((pcSPS->getVirtualBoundariesPosY(i)>>3) - 1) > (((pcSPS->getMaxPicHeightInLumaSamples() + 7) >> 3) - 2), "The value of sps_virtual_boundary_pos_y_minus1[ i ] shall be in the range of 0 to Ceil( sps_pic_height_max_in_luma_samples / 8 ) - 2, inclusive.");
      }
    }
  }
  if (pcSPS->getPtlDpbHrdParamsPresentFlag())
  {
    xWriteFlag(pcSPS->getGeneralHrdParametersPresentFlag(), "sps_timing_hrd_params_present_flag");
    if (pcSPS->getGeneralHrdParametersPresentFlag())
    {
      codeGeneralHrdparameters(pcSPS->getGeneralHrdParameters());
      if ((pcSPS->getMaxTLayers() - 1) > 0)
      {
        xWriteFlag(pcSPS->getSubLayerParametersPresentFlag(), "sps_sublayer_cpb_params_present_flag");
      }
      uint32_t firstSubLayer = pcSPS->getSubLayerParametersPresentFlag() ? 0 : (pcSPS->getMaxTLayers() - 1);
      codeOlsHrdParameters(pcSPS->getGeneralHrdParameters(), pcSPS->getOlsHrdParameters(), firstSubLayer,
                           pcSPS->getMaxTLayers() - 1);
    }
  }

  xWriteFlag(pcSPS->getFieldSeqFlag(),                          "sps_field_seq_flag");
  xWriteFlag( pcSPS->getVuiParametersPresentFlag(),            "sps_vui_parameters_present_flag" );
  if (pcSPS->getVuiParametersPresentFlag())
  {
    OutputBitstream *bs = getBitstream();
    OutputBitstream bs_count;
    setBitstream(&bs_count);
#if ENABLE_TRACING
    bool traceEnable = g_HLSTraceEnable;
    g_HLSTraceEnable = false;
#endif
    codeVUI(pcSPS->getVuiParameters(), pcSPS);
#if ENABLE_TRACING
    g_HLSTraceEnable = traceEnable;
#endif
    unsigned vui_payload_data_num_bits = bs_count.getNumberOfWrittenBits();
    CHECK( vui_payload_data_num_bits % 8 != 0, "Invalid number of VUI payload data bits" );
    setBitstream(bs);
    xWriteUvlc((vui_payload_data_num_bits >> 3) - 1, "sps_vui_payload_size_minus1");
    while (!isByteAligned())
    {
      xWriteFlag(0, "sps_vui_alignment_zero_bit");
    }
    codeVUI(pcSPS->getVuiParameters(), pcSPS);
  }

  bool sps_extension_present_flag=false;
  bool sps_extension_flags[NUM_SPS_EXTENSION_FLAGS]={false};

  sps_extension_flags[SPS_EXT__REXT] = pcSPS->getSpsRangeExtension().settingsDifferFromDefaults();

  // Other SPS extension flags checked here.

  for(int i=0; i<NUM_SPS_EXTENSION_FLAGS; i++)
  {
    sps_extension_present_flag|=sps_extension_flags[i];
  }

  xWriteFlag( (sps_extension_present_flag?1:0), "sps_extension_present_flag" );

  if (sps_extension_present_flag)
  {
    static const char *syntaxStrings[]={ "sps_range_extension_flag",
      "sps_extension_7bits[0]",
      "sps_extension_7bits[1]",
      "sps_extension_7bits[2]",
      "sps_extension_7bits[3]",
      "sps_extension_7bits[4]",
      "sps_extension_7bits[5]",
      "sps_extension_7bits[6]" };

    if (pcSPS->getBitDepth(ChannelType::LUMA) <= 10)
    {
      CHECK((sps_extension_flags[SPS_EXT__REXT] == 1),
            "The value of sps_range_extension_flag shall be 0 when BitDepth is less than or equal to 10.");
    }

    for(int i=0; i<NUM_SPS_EXTENSION_FLAGS; i++)
    {
      xWriteFlag( sps_extension_flags[i]?1:0, syntaxStrings[i] );
    }

    for(int i=0; i<NUM_SPS_EXTENSION_FLAGS; i++) // loop used so that the order is determined by the enum.
    {
      if (sps_extension_flags[i])
      {
        switch (SPSExtensionFlagIndex(i))
        {
        case SPS_EXT__REXT:
        {
          const SPSRExt &spsRangeExtension=pcSPS->getSpsRangeExtension();

          xWriteFlag( (spsRangeExtension.getExtendedPrecisionProcessingFlag() ? 1 : 0),       "extended_precision_processing_flag" );
          if (pcSPS->getTransformSkipEnabledFlag())
          {
            xWriteFlag( (spsRangeExtension.getTSRCRicePresentFlag() ? 1 : 0),                 "sps_ts_residual_coding_rice_present_in_sh_flag");
          }
          xWriteFlag( (spsRangeExtension.getRrcRiceExtensionEnableFlag() ? 1 : 0),                   "rrc_rice_extension_flag");
          xWriteFlag( (spsRangeExtension.getPersistentRiceAdaptationEnabledFlag() ? 1 : 0),   "persistent_rice_adaptation_enabled_flag" );
          xWriteFlag( (spsRangeExtension.getReverseLastSigCoeffEnabledFlag() ? 1 : 0),        "reverse_last_sig_coeff_enabled_flag" );
          break;
        }
        default:
          CHECK(sps_extension_flags[i]!=false, "Unknown PPS extension signalled"); // Should never get here with an active SPS extension flag.
          break;
        }
      }
    }
  }
  xWriteRbspTrailingBits();
}

void HLSWriter::codeDCI(const DCI* dci)
{
#if ENABLE_TRACING
  xTraceDCIHeader();
#endif
  xWriteCode(0, 4, "dci_reserved_zero_4bits");
  uint32_t numPTLs = (uint32_t)dci->getNumPTLs();
  CHECK( (numPTLs < 1) || ( numPTLs > 15), "dci_num_plts_minus1 shall be in the range of 0 - 14");

  xWriteCode(numPTLs - 1, 4, "dci_num_ptls_minus1");

  for (int i = 0; i < numPTLs; i++)
  {
    ProfileTierLevel ptl = dci->getProfileTierLevel(i);
    codeProfileTierLevel(&ptl, true, 0);
  }

  xWriteFlag(0, "dci_extension_flag");
  xWriteRbspTrailingBits();
}

void HLSWriter::codeOPI(const OPI *opi)
{
#if ENABLE_TRACING
  xTraceOPIHeader();
#endif
  xWriteFlag(opi->getOlsInfoPresentFlag(), "opi_ols_info_present_flag");
  xWriteFlag(opi->getHtidInfoPresentFlag(), "opi_htid_info_present_flag");

  if (opi->getOlsInfoPresentFlag())
  {
    xWriteUvlc(opi->getOpiOlsIdx(), "opi_ols_idx");
  }

  if (opi->getHtidInfoPresentFlag())
  {
    xWriteCode(opi->getOpiHtidPlus1(), 3, "opi_htid_plus1");
  }
  xWriteFlag(0, "opi_extension_flag");
  xWriteRbspTrailingBits();
}

void HLSWriter::codeVPS(const VPS* pcVPS)
{
#if ENABLE_TRACING
  xTraceVPSHeader();
#endif
  xWriteCode(pcVPS->getVPSId(), 4, "vps_video_parameter_set_id");
  xWriteCode(pcVPS->getMaxLayers() - 1, 6, "vps_max_layers_minus1");
  xWriteCode(pcVPS->getMaxSubLayers() - 1, 3, "vps_max_sublayers_minus1");
  if (pcVPS->getMaxLayers() > 1 && pcVPS->getMaxSubLayers() > 1)
  {
    xWriteFlag(pcVPS->getDefaultPtlDpbHrdMaxTidFlag(), "vps_default_ptl_dpb_hrd_max_tid_flag");
  }
  if (pcVPS->getMaxLayers() > 1)
  {
    xWriteFlag(pcVPS->getAllIndependentLayersFlag(), "vps_all_independent_layers_flag");
  }
  for (uint32_t i = 0; i < pcVPS->getMaxLayers(); i++)
  {
    xWriteCode(pcVPS->getLayerId(i), 6, "vps_layer_id");
    if (i > 0 && !pcVPS->getAllIndependentLayersFlag())
    {
      xWriteFlag(pcVPS->getIndependentLayerFlag(i), "vps_independent_layer_flag");
      if (!pcVPS->getIndependentLayerFlag(i))
      {
        bool presentFlag = false;
        for (int j = 0; j < i; j++)
        {
          presentFlag |= ((pcVPS->getMaxTidIlRefPicsPlus1(i, j) != MAX_TLAYER) && pcVPS->getDirectRefLayerFlag(i, j));
        }
        xWriteFlag(presentFlag, "max_tid_ref_present_flag[ i ]");
        for (int j = 0; j < i; j++)
        {
          xWriteFlag(pcVPS->getDirectRefLayerFlag(i, j), "vps_direct_ref_layer_flag");
          if (presentFlag && pcVPS->getDirectRefLayerFlag(i, j))
          {
            xWriteCode(pcVPS->getMaxTidIlRefPicsPlus1(i, j), 3, "max_tid_il_ref_pics_plus1[ i ][ j ]");
          }
        }
      }
    }
  }
  if( pcVPS->getMaxLayers() > 1 )
  {
    if (pcVPS->getAllIndependentLayersFlag())
    {
      xWriteFlag(pcVPS->getEachLayerIsAnOlsFlag(), "vps_each_layer_is_an_ols_flag");
    }
    if (!pcVPS->getEachLayerIsAnOlsFlag())
    {
      if (!pcVPS->getAllIndependentLayersFlag())
      {
        xWriteCode(pcVPS->getOlsModeIdc(), 2, "vps_ols_mode_idc");
      }
      if (pcVPS->getOlsModeIdc() == 2)
      {
        xWriteCode(pcVPS->getNumOutputLayerSets() - 2, 8, "vps_num_output_layer_sets_minus2");
        for (uint32_t i = 1; i < pcVPS->getNumOutputLayerSets(); i++)
        {
          for (uint32_t j = 0; j < pcVPS->getMaxLayers(); j++)
          {
            xWriteFlag(pcVPS->getOlsOutputLayerFlag(i, j), "vps_ols_output_layer_flag");
          }
        }
      }
    }
    CHECK(pcVPS->getNumPtls() - 1 >= pcVPS->getTotalNumOLSs(), "vps_num_ptls_minus1 shall be less than TotalNumOlss");
    xWriteCode(pcVPS->getNumPtls() - 1, 8, "vps_num_ptls_minus1");
  }

  int totalNumOlss = pcVPS->getTotalNumOLSs();
  for (int i = 0; i < pcVPS->getNumPtls(); i++)
  {
    if(i > 0)
    {
      xWriteFlag(pcVPS->getPtPresentFlag(i), "vps_pt_present_flag");
    }
    if (!pcVPS->getDefaultPtlDpbHrdMaxTidFlag())
    {
      xWriteCode(pcVPS->getPtlMaxTemporalId(i), 3, "vps_ptl_max_tid");
    }
    else
    {
      CHECK(pcVPS->getPtlMaxTemporalId(i) != pcVPS->getMaxSubLayers() - 1, "When vps_default_ptl_dpb_hrd_max_tid_flag is equal to 1, the value of vps_ptl_max_tid[ i ] is inferred to be equal to vps_max_sublayers_minus1");
    }
  }
  int cnt = 0;
  while (m_pcBitIf->getNumBitsUntilByteAligned())
  {
    xWriteFlag( 0, "vps_ptl_reserved_zero_bit");
    cnt++;
  }
  CHECK(cnt>=8, "More than '8' alignment bytes written");
  for (int i = 0; i < pcVPS->getNumPtls(); i++)
  {
    codeProfileTierLevel(&pcVPS->getProfileTierLevel(i), pcVPS->getPtPresentFlag(i), pcVPS->getPtlMaxTemporalId(i));
  }
  for (int i = 0; i < totalNumOlss; i++)
  {
    if (pcVPS->getNumPtls() > 1 && pcVPS->getNumPtls() != pcVPS->getTotalNumOLSs())
    {
      xWriteCode(pcVPS->getOlsPtlIdx(i), 8, "vps_ols_ptl_idx");
    }
  }

  if( !pcVPS->getEachLayerIsAnOlsFlag() )
  {
    xWriteUvlc( pcVPS->m_numDpbParams - 1, "vps_num_dpb_params_minus1" );

    if( pcVPS->getMaxSubLayers() > 1 )
    {
      xWriteFlag( pcVPS->m_sublayerDpbParamsPresentFlag, "vps_sublayer_dpb_params_present_flag" );
    }

    for( int i = 0; i < pcVPS->m_numDpbParams; i++ )
    {
      if (!pcVPS->getDefaultPtlDpbHrdMaxTidFlag())
      {
        xWriteCode(pcVPS->m_dpbMaxTemporalId[i], 3, "vps_dpb_max_tid[i]");
      }
      else
      {
        CHECK(pcVPS->m_dpbMaxTemporalId[i] != pcVPS->getMaxSubLayers() - 1, "When vps_default_ptl_dpb_hrd_max_tid_flag is equal to 1, the value of vps_dpb_max_tid[ i ] is inferred to be equal to vps_max_sublayers_minus1");
      }

      for( int j = ( pcVPS->m_sublayerDpbParamsPresentFlag ? 0 : pcVPS->m_dpbMaxTemporalId[i] ); j <= pcVPS->m_dpbMaxTemporalId[i]; j++ )
      {
        CHECK(pcVPS->m_dpbParameters[i].maxDecPicBuffering[j] < 1, "MaxDecPicBuffering must be greater than 0");
        xWriteUvlc(pcVPS->m_dpbParameters[i].maxDecPicBuffering[j] - 1, "dpb_max_dec_pic_buffering_minus1[i]");
        xWriteUvlc( pcVPS->m_dpbParameters[i].maxNumReorderPics[j], "dpb_max_num_reorder_pics[i]" );
        xWriteUvlc( pcVPS->m_dpbParameters[i].maxLatencyIncreasePlus1[j], "dpb_max_latency_increase_plus1[i]" );
      }
    }

    for( int i = 0; i < pcVPS->getTotalNumOLSs(); i++ )
    {
      if( pcVPS->m_numLayersInOls[i] > 1 )
      {
        xWriteUvlc( pcVPS->getOlsDpbPicSize( i ).width, "vps_ols_dpb_pic_width[i]" );
        xWriteUvlc( pcVPS->getOlsDpbPicSize( i ).height, "vps_ols_dpb_pic_height[i]" );
        xWriteCode(to_underlying(pcVPS->m_olsDpbChromaFormatIdc[i]), 2, "vps_ols_dpb_chroma_format[i]");
        const Profile::Name profile = pcVPS->getProfileTierLevel(pcVPS->getOlsPtlIdx(i)).getProfileIdc();
        if (profile != Profile::NONE)
        {
          CHECK(pcVPS->m_olsDpbBitDepthMinus8[i] + 8 > ProfileFeatures::getProfileFeatures(profile)->maxBitDepth, "vps_ols_dpb_bitdepth_minus8[ i ] exceeds range supported by signalled profile");
        }
        xWriteUvlc( pcVPS->m_olsDpbBitDepthMinus8[i], "vps_ols_dpb_bitdepth_minus8[i]");
        if( (pcVPS->m_numDpbParams > 1) && (pcVPS->m_numDpbParams != pcVPS->m_numMultiLayeredOlss) )
        {
          xWriteUvlc( pcVPS->getOlsDpbParamsIdx( i ), "vps_ols_dpb_params_idx[i]" );
        }
      }
    }
  }
  if (!pcVPS->getEachLayerIsAnOlsFlag())
  {
    xWriteFlag(pcVPS->getVPSGeneralHrdParamsPresentFlag(), "vps_general_hrd_params_present_flag");
  }
  if (pcVPS->getVPSGeneralHrdParamsPresentFlag())
  {
    codeGeneralHrdparameters(pcVPS->getGeneralHrdParameters());
    if ((pcVPS->getMaxSubLayers()-1) > 0)
    {
      xWriteFlag(pcVPS->getVPSSublayerCpbParamsPresentFlag(), "vps_sublayer_cpb_params_present_flag");
    }
    xWriteUvlc(pcVPS->getNumOlsTimingHrdParamsMinus1(), "vps_num_ols_timing_hrd_params_minus1");
    for (int i = 0; i <= pcVPS->getNumOlsTimingHrdParamsMinus1(); i++)
    {
      if (!pcVPS->getDefaultPtlDpbHrdMaxTidFlag())
      {
        xWriteCode(pcVPS->getHrdMaxTid(i), 3, "vps_hrd_max_tid[i]");
      }
      else
      {
        CHECK(pcVPS->getHrdMaxTid(i) != pcVPS->getMaxSubLayers() - 1, "When vps_default_ptl_dpb_hrd_max_tid_flag is equal to 1, the value of vps_hrd_max_tid[ i ] is inferred to be equal to vps_max_sublayers_minus1");
      }
      uint32_t firstSublayer = pcVPS->getVPSSublayerCpbParamsPresentFlag() ? 0 : pcVPS->getHrdMaxTid(i);
      codeOlsHrdParameters(pcVPS->getGeneralHrdParameters(), pcVPS->getOlsHrdParameters(i),firstSublayer, pcVPS->getHrdMaxTid(i));
    }
    if ((pcVPS->getNumOlsTimingHrdParamsMinus1() > 0) && ((pcVPS->getNumOlsTimingHrdParamsMinus1() + 1) != pcVPS->m_numMultiLayeredOlss))
    {
      for (int i = 0; i < pcVPS->m_numMultiLayeredOlss; i++)
      {
        xWriteUvlc(pcVPS->getOlsTimingHrdIdx(i), "vps_ols_timing_hrd_idx[i]");
      }
    }
  }

  xWriteFlag(0, "vps_extension_flag");

  //future extensions here..
  xWriteRbspTrailingBits();
}

void HLSWriter::codePictureHeader( PicHeader* picHeader, bool writeRbspTrailingBits, Slice *slice )
{
  const PPS *pps = nullptr;
  const SPS *sps = nullptr;

#if ENABLE_TRACING
  xTracePictureHeader ();
#endif

  if (!slice)
  {
    slice = picHeader->getPic()->cs->slice;
  }
  xWriteFlag(picHeader->getGdrOrIrapPicFlag(), "ph_gdr_or_irap_pic_flag");
  xWriteFlag(picHeader->getNonReferencePictureFlag(), "ph_non_ref_pic_flag");
  if (picHeader->getGdrOrIrapPicFlag())
  {
    xWriteFlag(picHeader->getGdrPicFlag(), "ph_gdr_pic_flag");
  }
  // Q0781, two-flags
  xWriteFlag(picHeader->getPicInterSliceAllowedFlag(), "ph_inter_slice_allowed_flag");
  if (picHeader->getPicInterSliceAllowedFlag())
  {
    xWriteFlag(picHeader->getPicIntraSliceAllowedFlag(), "ph_intra_slice_allowed_flag");
  }
  // parameter sets
  xWriteUvlc(picHeader->getPPSId(), "ph_pic_parameter_set_id");
  pps = slice->getPPS();
  CHECK(pps == 0, "Invalid PPS");
  sps = slice->getSPS();
  CHECK(sps == 0, "Invalid SPS");
  int pocBits = slice->getSPS()->getBitsForPOC();
  int pocMask = (1 << pocBits) - 1;
  xWriteCode(slice->getPOC() & pocMask, pocBits, "ph_pic_order_cnt_lsb");
  if( picHeader->getGdrPicFlag() )
  {
    xWriteUvlc(picHeader->getRecoveryPocCnt(), "ph_recovery_poc_cnt");
  }
  else
  {
    picHeader->setRecoveryPocCnt( -1 );
  }
  // PH extra bits are not written in the reference encoder
  // as these bits are reserved for future extensions
  // for( i = 0; i < NumExtraPhBits; i++ )
  //    ph_extra_bit[ i ]

  if (sps->getPocMsbCycleFlag())
  {
    xWriteFlag(picHeader->getPocMsbPresentFlag(), "ph_poc_msb_present_flag");
    if (picHeader->getPocMsbPresentFlag())
    {
      xWriteCode(picHeader->getPocMsbVal(), sps->getPocMsbCycleLen(), "ph_poc_msb_cycle_val");
    }
  }


   // alf enable flags and aps IDs
  if( sps->getALFEnabledFlag() )
  {
    if (pps->getAlfInfoInPhFlag())
    {
      xWriteFlag(picHeader->getAlfEnabledFlag(COMPONENT_Y), "ph_alf_enabled_flag");
      if (picHeader->getAlfEnabledFlag(COMPONENT_Y))
      {
        xWriteCode(picHeader->getNumAlfApsIdsLuma(), 3, "ph_num_alf_aps_ids_luma");
        const AlfApsList &apsId = picHeader->getAlfApsIdsLuma();
        for (int i = 0; i < picHeader->getNumAlfApsIdsLuma(); i++)
        {
          xWriteCode(apsId[i], 3, "ph_alf_aps_id_luma");
        }

        const int alfChromaIdc = picHeader->getAlfEnabledFlag(COMPONENT_Cb) + picHeader->getAlfEnabledFlag(COMPONENT_Cr) * 2 ;
        if (isChromaEnabled(sps->getChromaFormatIdc()))
        {
          xWriteCode(picHeader->getAlfEnabledFlag(COMPONENT_Cb), 1, "ph_alf_cb_enabled_flag");
          xWriteCode(picHeader->getAlfEnabledFlag(COMPONENT_Cr), 1, "ph_alf_cr_enabled_flag");
        }
        if (alfChromaIdc)
        {
          xWriteCode(picHeader->getAlfApsIdChroma(), 3, "ph_alf_aps_id_chroma");
        }
        if (sps->getCCALFEnabledFlag())
        {
          xWriteFlag(picHeader->getCcAlfEnabledFlag(COMPONENT_Cb), "ph_cc_alf_cb_enabled_flag");
          if (picHeader->getCcAlfEnabledFlag(COMPONENT_Cb))
          {
            xWriteCode(picHeader->getCcAlfCbApsId(), 3, "ph_cc_alf_cb_aps_id");
          }
          xWriteFlag(picHeader->getCcAlfEnabledFlag(COMPONENT_Cr), "ph_cc_alf_cr_enabled_flag");
          if (picHeader->getCcAlfEnabledFlag(COMPONENT_Cr))
          {
            xWriteCode(picHeader->getCcAlfCrApsId(), 3, "ph_cc_alf_cr_aps_id");
          }
        }
      }
    }
    else
    {
      picHeader->setAlfEnabledFlag(COMPONENT_Y,  true);
      picHeader->setAlfEnabledFlag(COMPONENT_Cb, true);
      picHeader->setAlfEnabledFlag(COMPONENT_Cr, true);
      picHeader->setCcAlfEnabledFlag(COMPONENT_Cb, sps->getCCALFEnabledFlag());
      picHeader->setCcAlfEnabledFlag(COMPONENT_Cr, sps->getCCALFEnabledFlag());
    }
  }
  else
  {
    picHeader->setAlfEnabledFlag(COMPONENT_Y,  false);
    picHeader->setAlfEnabledFlag(COMPONENT_Cb, false);
    picHeader->setAlfEnabledFlag(COMPONENT_Cr, false);
    picHeader->setCcAlfEnabledFlag(COMPONENT_Cb, false);
    picHeader->setCcAlfEnabledFlag(COMPONENT_Cr, false);
  }

  // luma mapping / chroma scaling controls
  if (sps->getUseLmcs())
  {
    xWriteFlag(picHeader->getLmcsEnabledFlag(), "ph_lmcs_enabled_flag");
    if (picHeader->getLmcsEnabledFlag())
    {
      xWriteCode(picHeader->getLmcsAPSId(), 2, "ph_lmcs_aps_id");
      if (isChromaEnabled(sps->getChromaFormatIdc()))
      {
        xWriteFlag(picHeader->getLmcsChromaResidualScaleFlag(), "ph_chroma_residual_scale_flag");
      }
      else
      {
        picHeader->setLmcsChromaResidualScaleFlag(false);
      }
    }
  }
  else
  {
    picHeader->setLmcsEnabledFlag(false);
    picHeader->setLmcsChromaResidualScaleFlag(false);
  }

  // quantization scaling lists
  if( sps->getScalingListFlag() )
  {
    xWriteFlag( picHeader->getExplicitScalingListEnabledFlag(), "ph_scaling_list_present_flag" );
    if( picHeader->getExplicitScalingListEnabledFlag() )
    {
      xWriteCode( picHeader->getScalingListAPSId(), 3, "ph_scaling_list_aps_id" );
    }
  }
  else
  {
    picHeader->setExplicitScalingListEnabledFlag( false );
  }

  // virtual boundaries
  if( sps->getVirtualBoundariesEnabledFlag() && !sps->getVirtualBoundariesPresentFlag() )
  {
    xWriteFlag( picHeader->getVirtualBoundariesPresentFlag(), "ph_virtual_boundaries_present_flag" );
    if( picHeader->getVirtualBoundariesPresentFlag() )
    {
#if GDR_ENABLED
      if (sps->getGDREnabledFlag())
      {
        int n = picHeader->getNumVerVirtualBoundaries();
        for (unsigned i = 0; i < n; i++)
        {
          if (picHeader->getVirtualBoundariesPosX(i) == pps->getPicWidthInLumaSamples())
          {
            n = n - 1;
          }
        }

        xWriteUvlc(n, "ph_num_ver_virtual_boundaries");

        if (pps->getPicWidthInLumaSamples() <= 8)
        {
          CHECK(picHeader->getNumVerVirtualBoundaries() != 0, "PH: When picture width is less than or equal to 8, the number of vertical virtual boundaries shall be equal to 0");
        }
        else
        {
          CHECK(picHeader->getNumVerVirtualBoundaries() > 3, "PH: The number of vertical virtual boundaries shall be in the range of 0 to 3");
        }

        for (unsigned i = 0; i < picHeader->getNumVerVirtualBoundaries(); i++)
        {
          if (picHeader->getVirtualBoundariesPosX(i) != pps->getPicWidthInLumaSamples())
          {
            xWriteUvlc((picHeader->getVirtualBoundariesPosX(i) >> 3) - 1, "ph_virtual_boundary_pos_x_minus1[i]");
            CHECK(((picHeader->getVirtualBoundariesPosX(i) >> 3) - 1) > (((pps->getPicWidthInLumaSamples() + 7) >> 3) - 2), "The value of ph_virtual_boundary_pos_x_minus1[ i ] shall be in the range of 0 to Ceil( pps_pic_width_in_luma_samples / 8 ) - 2, inclusive.");
          }
        }
      }
      else
      {
        xWriteUvlc(picHeader->getNumVerVirtualBoundaries(), "ph_num_ver_virtual_boundaries");
        if (pps->getPicWidthInLumaSamples() <= 8)
        {
          CHECK(picHeader->getNumVerVirtualBoundaries() != 0, "PH: When picture width is less than or equal to 8, the number of vertical virtual boundaries shall be equal to 0");
        }
        else
        {
          CHECK(picHeader->getNumVerVirtualBoundaries() > 3, "PH: The number of vertical virtual boundaries shall be in the range of 0 to 3");
        }
        for (unsigned i = 0; i < picHeader->getNumVerVirtualBoundaries(); i++)
        {
          xWriteUvlc((picHeader->getVirtualBoundariesPosX(i) >> 3) - 1, "ph_virtual_boundary_pos_x_minus1[i]");
          CHECK(((picHeader->getVirtualBoundariesPosX(i) >> 3) - 1) > (((pps->getPicWidthInLumaSamples() + 7) >> 3) - 2), "The value of ph_virtual_boundary_pos_x_minus1[ i ] shall be in the range of 0 to Ceil( pps_pic_width_in_luma_samples / 8 ) - 2, inclusive.");
        }
      }
#else
      xWriteUvlc(picHeader->getNumVerVirtualBoundaries(), "ph_num_ver_virtual_boundaries");
      if (pps->getPicWidthInLumaSamples() <= 8)
      {
        CHECK(picHeader->getNumVerVirtualBoundaries() != 0, "PH: When picture width is less than or equal to 8, the number of vertical virtual boundaries shall be equal to 0");
      }
      else
      {
        CHECK(picHeader->getNumVerVirtualBoundaries() > 3, "PH: The number of vertical virtual boundaries shall be in the range of 0 to 3");
      }
      for( unsigned i = 0; i < picHeader->getNumVerVirtualBoundaries(); i++ )
      {
        xWriteUvlc((picHeader->getVirtualBoundariesPosX(i) >> 3) - 1, "ph_virtual_boundary_pos_x_minus1[i]");
        CHECK(((picHeader->getVirtualBoundariesPosX(i)>>3) - 1) > (((pps->getPicWidthInLumaSamples() + 7) >> 3) - 2), "The value of ph_virtual_boundary_pos_x_minus1[ i ] shall be in the range of 0 to Ceil( pps_pic_width_in_luma_samples / 8 ) - 2, inclusive.");
      }
#endif
      xWriteUvlc(picHeader->getNumHorVirtualBoundaries(), "ph_num_hor_virtual_boundaries");
      if (pps->getPicHeightInLumaSamples() <= 8)
      {
        CHECK(picHeader->getNumHorVirtualBoundaries() != 0, "PH: When picture width is less than or equal to 8, the number of horizontal virtual boundaries shall be equal to 0");
      }
      else
      {
        CHECK(picHeader->getNumHorVirtualBoundaries() > 3, "PH: The number of horizontal virtual boundaries shall be in the range of 0 to 3");
      }
      for( unsigned i = 0; i < picHeader->getNumHorVirtualBoundaries(); i++ )
      {
        xWriteUvlc((picHeader->getVirtualBoundariesPosY(i)>>3) - 1, "ph_virtual_boundary_pos_y_minus1[i]");
        CHECK(((picHeader->getVirtualBoundariesPosY(i)>>3) - 1) > (((pps->getPicHeightInLumaSamples() + 7) >> 3) - 2), "The value of ph_virtual_boundary_pos_y_minus1[ i ] shall be in the range of 0 to Ceil( pps_pic_height_in_luma_samples / 8 ) - 2, inclusive.");
      }
    }
    else
    {
      picHeader->setVirtualBoundariesPresentFlag( false );
      picHeader->setNumVerVirtualBoundaries( 0 );
      picHeader->setNumHorVirtualBoundaries( 0 );
    }
  }
  else
  {
    picHeader->setVirtualBoundariesPresentFlag(sps->getVirtualBoundariesPresentFlag());
    if (picHeader->getVirtualBoundariesPresentFlag())
    {
      picHeader->setNumVerVirtualBoundaries(sps->getNumVerVirtualBoundaries());
      picHeader->setNumHorVirtualBoundaries(sps->getNumHorVirtualBoundaries());
      for( unsigned i = 0; i < 3; i++ )
      {
        picHeader->setVirtualBoundariesPosX( sps->getVirtualBoundariesPosX(i), i );
        picHeader->setVirtualBoundariesPosY( sps->getVirtualBoundariesPosY(i), i );
      }
    }
  }


  // picture output flag
  if (pps->getOutputFlagPresentFlag() && !picHeader->getNonReferencePictureFlag())
  {
    xWriteFlag( picHeader->getPicOutputFlag(), "ph_pic_output_flag" );
  }
  else
  {
    picHeader->setPicOutputFlag(true);
  }

  // reference picture lists
  if (pps->getRplInfoInPhFlag())
  {
    // List0 and List1
    for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
    {
      const int  numRplsInSps = sps->getNumRpl(l);
      const int  rplIdx       = picHeader->getRplIdx(l);
      const bool rplSpsFlag   = rplIdx != -1;

      if (numRplsInSps == 0)
      {
        CHECK(rplSpsFlag, "rpl_sps_flag[1] will be infer to 0 and this is not what was expected");
      }
      else if (l == REF_PIC_LIST_0 || pps->getRpl1IdxPresentFlag())
      {
        xWriteFlag(rplSpsFlag ? 1 : 0, "rpl_sps_flag[i]");
      }
      else
      {
        bool rplSpsFlag0 = picHeader->getRplIdx(REF_PIC_LIST_0) != -1;
        CHECK(rplSpsFlag != rplSpsFlag0, "rpl_sps_flag[1] will be infer to 0 and this is not what was expected");
      }

      if (rplSpsFlag)
      {
        CHECK(rplIdx >= numRplsInSps, "rpl_idx is too large");

        if (l == REF_PIC_LIST_0 || pps->getRpl1IdxPresentFlag())
        {
          if (numRplsInSps > 1)
          {
            const int numBits = ceilLog2(numRplsInSps);
            xWriteCode(rplIdx, numBits, "rpl_idx[i]");
          }
        }
        else
        {
          CHECK(rplIdx != picHeader->getRplIdx(REF_PIC_LIST_0),
                "RPL1Idx is not signalled but it is not the same as RPL0Idx");
        }
      }
      // explicit RPL in picture header
      else
      {
        xCodeRefPicList(picHeader->getRpl(l), sps->getLongTermRefsPresent(), sps->getBitsForPOC(),
                        !sps->getUseWP() && !sps->getUseWPBiPred(), -1);
      }

      // POC MSB cycle signalling for LTRP
      const ReferencePictureList *rpl = picHeader->getRpl(l);

      if (rpl != nullptr && rpl->getNumberOfLongtermPictures() > 0)
      {
        for (int i = 0; i < rpl->getNumRefEntries(); i++)
        {
          if (rpl->isRefPicLongterm(i))
          {
            if (rpl->getLtrpInSliceHeaderFlag())
            {
              xWriteCode(rpl->getRefPicIdentifier(i), sps->getBitsForPOC(), "poc_lsb_lt[listIdx][rplsIdx][j]");
            }
            xWriteFlag(rpl->getDeltaPocMSBPresentFlag(i) ? 1 : 0, "delta_poc_msb_present_flag[i][j]");
            if (rpl->getDeltaPocMSBPresentFlag(i))
            {
              xWriteUvlc(rpl->getDeltaPocMSBCycleLT(i), "delta_poc_msb_cycle_lt[i][j]");
            }
          }
        }
      }
    }
  }

  // partitioning constraint overrides
  if (sps->getSplitConsOverrideEnabledFlag())
  {
    xWriteFlag(picHeader->getSplitConsOverrideFlag(), "ph_partition_constraints_override_flag");
  }
  else
  {
    picHeader->setSplitConsOverrideFlag(false);
  }
  // Q0781, two-flags
  if (picHeader->getPicIntraSliceAllowedFlag())
  {
    if (picHeader->getSplitConsOverrideFlag())
    {
      xWriteUvlc(floorLog2(picHeader->getMinQTSize(I_SLICE)) - sps->getLog2MinCodingBlockSize(), "ph_log2_diff_min_qt_min_cb_intra_slice_luma");
      xWriteUvlc(picHeader->getMaxMTTHierarchyDepth(I_SLICE), "ph_max_mtt_hierarchy_depth_intra_slice_luma");
      if (picHeader->getMaxMTTHierarchyDepth(I_SLICE) != 0)
      {
        xWriteUvlc(floorLog2(picHeader->getMaxBTSize(I_SLICE)) - floorLog2(picHeader->getMinQTSize(I_SLICE)), "ph_log2_diff_max_bt_min_qt_intra_slice_luma");
        xWriteUvlc(floorLog2(picHeader->getMaxTTSize(I_SLICE)) - floorLog2(picHeader->getMinQTSize(I_SLICE)), "ph_log2_diff_max_tt_min_qt_intra_slice_luma");
      }

      if (sps->getUseDualITree())
      {
        xWriteUvlc(floorLog2(picHeader->getMinQTSize(I_SLICE, ChannelType::CHROMA)) - sps->getLog2MinCodingBlockSize(),
                   "ph_log2_diff_min_qt_min_cb_intra_slice_chroma");
        xWriteUvlc(picHeader->getMaxMTTHierarchyDepth(I_SLICE, ChannelType::CHROMA),
                   "ph_max_mtt_hierarchy_depth_intra_slice_chroma");
        if (picHeader->getMaxMTTHierarchyDepth(I_SLICE, ChannelType::CHROMA) != 0)
        {
          xWriteUvlc(floorLog2(picHeader->getMaxBTSize(I_SLICE, ChannelType::CHROMA))
                       - floorLog2(picHeader->getMinQTSize(I_SLICE, ChannelType::CHROMA)),
                     "ph_log2_diff_max_bt_min_qt_intra_slice_chroma");
          xWriteUvlc(floorLog2(picHeader->getMaxTTSize(I_SLICE, ChannelType::CHROMA))
                       - floorLog2(picHeader->getMinQTSize(I_SLICE, ChannelType::CHROMA)),
                     "ph_log2_diff_max_tt_min_qt_intra_slice_chroma");
        }
      }
    }
  }
  if (picHeader->getPicIntraSliceAllowedFlag())
  {
  // delta quantization and chrom and chroma offset
    if (pps->getUseDQP())
    {
      xWriteUvlc( picHeader->getCuQpDeltaSubdivIntra(), "ph_cu_qp_delta_subdiv_intra_slice" );
    }
    else
    {
      picHeader->setCuQpDeltaSubdivIntra( 0 );
    }
    if (pps->getCuChromaQpOffsetListEnabledFlag())
    {
      xWriteUvlc( picHeader->getCuChromaQpOffsetSubdivIntra(), "ph_cu_chroma_qp_offset_subdiv_intra_slice" );
    }
  }


  if (picHeader->getPicInterSliceAllowedFlag())
  {
    if (picHeader->getSplitConsOverrideFlag())
    {
      xWriteUvlc(floorLog2(picHeader->getMinQTSize(P_SLICE)) - sps->getLog2MinCodingBlockSize(), "ph_log2_diff_min_qt_min_cb_inter_slice");
      xWriteUvlc(picHeader->getMaxMTTHierarchyDepth(P_SLICE), "ph_max_mtt_hierarchy_depth_inter_slice");
      if (picHeader->getMaxMTTHierarchyDepth(P_SLICE) != 0)
      {
        xWriteUvlc(floorLog2(picHeader->getMaxBTSize(P_SLICE)) - floorLog2(picHeader->getMinQTSize(P_SLICE)), "ph_log2_diff_max_bt_min_qt_inter_slice");
        xWriteUvlc(floorLog2(picHeader->getMaxTTSize(P_SLICE)) - floorLog2(picHeader->getMinQTSize(P_SLICE)), "ph_log2_diff_max_tt_min_qt_inter_slice");
      }
    }

    // delta quantization and chrom and chroma offset
    if (pps->getUseDQP())
    {
      xWriteUvlc(picHeader->getCuQpDeltaSubdivInter(), "ph_cu_qp_delta_subdiv_inter_slice");
    }
    else
    {
      picHeader->setCuQpDeltaSubdivInter(0);
    }
    if (pps->getCuChromaQpOffsetListEnabledFlag())
    {
      xWriteUvlc(picHeader->getCuChromaQpOffsetSubdivInter(), "ph_cu_chroma_qp_offset_subdiv_inter_slice");
    }

    // temporal motion vector prediction
    if (sps->getSPSTemporalMVPEnabledFlag())
    {
      xWriteFlag( picHeader->getEnableTMVPFlag(), "ph_temporal_mvp_enabled_flag" );
      if (picHeader->getEnableTMVPFlag() && pps->getRplInfoInPhFlag())
      {
        if (picHeader->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 0)
        {
          xWriteCode(picHeader->getPicColFromL0Flag(), 1, "ph_collocated_from_l0_flag");
        }
        if ((picHeader->getPicColFromL0Flag() && picHeader->getRpl(REF_PIC_LIST_0)->getNumRefEntries() > 1)
            || (!picHeader->getPicColFromL0Flag() && picHeader->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 1))
        {
          xWriteUvlc(picHeader->getColRefIdx(), "ph_collocated_ref_idx");
        }
      }
    }
    else
    {
      picHeader->setEnableTMVPFlag(false);
    }

    // merge candidate list size
    // subblock merge candidate list size
    if ( sps->getUseAffine() )
    {
      picHeader->setMaxNumAffineMergeCand(sps->getMaxNumAffineMergeCand());
    }
    else
    {
      picHeader->setMaxNumAffineMergeCand(sps->getSbTMVPEnabledFlag() && picHeader->getEnableTMVPFlag());
    }

  // full-pel MMVD flag
    if (sps->getFpelMmvdEnabledFlag())
    {
      xWriteFlag( picHeader->getDisFracMMVD(), "ph_fpel_mmvd_enabled_flag" );
    }
    else
    {
      picHeader->setDisFracMMVD(false);
    }

    // mvd L1 zero flag
    if (!pps->getRplInfoInPhFlag() || picHeader->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 0)
    {
      xWriteFlag(picHeader->getMvdL1ZeroFlag(), "ph_mvd_l1_zero_flag");
    }

    // picture level BDOF disable flags
    if (sps->getBdofControlPresentInPhFlag()
        && (!pps->getRplInfoInPhFlag() || picHeader->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 0))
    {
      xWriteFlag(picHeader->getBdofDisabledFlag(), "ph_bdof_disabled_flag");
    }
    else
    {
      picHeader->setBdofDisabledFlag(false);
    }

  // picture level DMVR disable flags
    if (sps->getDmvrControlPresentInPhFlag()
        && (!pps->getRplInfoInPhFlag() || picHeader->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 0))
    {
      xWriteFlag(picHeader->getDmvrDisabledFlag(), "ph_dmvr_disabled_flag");
    }
    else
    {
      picHeader->setDmvrDisabledFlag(false);
    }

  // picture level PROF disable flags
    if (sps->getProfControlPresentInPhFlag())
    {
      xWriteFlag(picHeader->getProfDisabledFlag(), "ph_prof_disabled_flag");
    }

    if ((pps->getUseWP() || pps->getWPBiPred()) && pps->getWpInfoInPhFlag())
    {
      xCodePredWeightTable(picHeader, pps, sps);
    }
  }
  // inherit constraint values from SPS
  if (!sps->getSplitConsOverrideEnabledFlag() || !picHeader->getSplitConsOverrideFlag())
  {
    picHeader->setMinQTSizes(sps->getMinQTSizes());
    picHeader->setMaxMTTHierarchyDepths(sps->getMaxMTTHierarchyDepths());
    picHeader->setMaxBTSizes(sps->getMaxBTSizes());
    picHeader->setMaxTTSizes(sps->getMaxTTSizes());
  }
  // ibc merge candidate list size
  if (pps->getQpDeltaInfoInPhFlag())
  {
    xWriteSvlc(picHeader->getQpDelta(), "ph_qp_delta");
  }

  // joint Cb/Cr sign flag
  if (sps->getJointCbCrEnabledFlag())
  {
    xWriteFlag( picHeader->getJointCbCrSignFlag(), "ph_joint_cbcr_sign_flag" );
  }
  else
  {
    picHeader->setJointCbCrSignFlag(false);
  }

  // sao enable flags
  if(sps->getSAOEnabledFlag())
  {
    if (pps->getSaoInfoInPhFlag())
    {
      xWriteFlag(picHeader->getSaoEnabledFlag(ChannelType::LUMA), "ph_sao_luma_enabled_flag");
      if (isChromaEnabled(sps->getChromaFormatIdc()))
      {
        xWriteFlag(picHeader->getSaoEnabledFlag(ChannelType::CHROMA), "ph_sao_chroma_enabled_flag");
      }
    }
    else
    {
      picHeader->setSaoEnabledFlag(ChannelType::LUMA, true);
      picHeader->setSaoEnabledFlag(ChannelType::CHROMA, true);
    }
  }
  else
  {
    picHeader->setSaoEnabledFlag(ChannelType::LUMA, false);
    picHeader->setSaoEnabledFlag(ChannelType::CHROMA, false);
  }

  // deblocking filter controls
  if (pps->getDeblockingFilterControlPresentFlag())
  {
    if ( pps->getDbfInfoInPhFlag() )
    {
      xWriteFlag( picHeader->getDeblockingFilterOverrideFlag(), "ph_deblocking_params_present_flag" );
    }
    else
    {
      picHeader->setDeblockingFilterOverrideFlag(false);
    }

    if(picHeader->getDeblockingFilterOverrideFlag())
    {
      if (!pps->getPPSDeblockingFilterDisabledFlag())
      {
        xWriteFlag(picHeader->getDeblockingFilterDisable(), "ph_deblocking_filter_disabled_flag");
      }
      if( !picHeader->getDeblockingFilterDisable() )
      {
        xWriteSvlc( picHeader->getDeblockingFilterBetaOffsetDiv2(), "ph_beta_offset_div2" );
        xWriteSvlc( picHeader->getDeblockingFilterTcOffsetDiv2(), "ph_tc_offset_div2" );
        if( pps->getPPSChromaToolFlag() )
        {
          xWriteSvlc( picHeader->getDeblockingFilterCbBetaOffsetDiv2(), "ph_cb_beta_offset_div2" );
          xWriteSvlc( picHeader->getDeblockingFilterCbTcOffsetDiv2(), "ph_cb_tc_offset_div2" );
          xWriteSvlc( picHeader->getDeblockingFilterCrBetaOffsetDiv2(), "ph_cr_beta_offset_div2" );
          xWriteSvlc( picHeader->getDeblockingFilterCrTcOffsetDiv2(), "ph_cr_tc_offset_div2" );
        }
      }
    }
    else
    {
      picHeader->setDeblockingFilterDisable       ( pps->getPPSDeblockingFilterDisabledFlag() );
      picHeader->setDeblockingFilterBetaOffsetDiv2( pps->getDeblockingFilterBetaOffsetDiv2() );
      picHeader->setDeblockingFilterTcOffsetDiv2  ( pps->getDeblockingFilterTcOffsetDiv2() );
      picHeader->setDeblockingFilterCbBetaOffsetDiv2( pps->getDeblockingFilterCbBetaOffsetDiv2() );
      picHeader->setDeblockingFilterCbTcOffsetDiv2  ( pps->getDeblockingFilterCbTcOffsetDiv2() );
      picHeader->setDeblockingFilterCrBetaOffsetDiv2( pps->getDeblockingFilterCrBetaOffsetDiv2() );
      picHeader->setDeblockingFilterCrTcOffsetDiv2  ( pps->getDeblockingFilterCrTcOffsetDiv2() );
    }
  }
  else
  {
    picHeader->setDeblockingFilterDisable       ( false );
    picHeader->setDeblockingFilterBetaOffsetDiv2( 0 );
    picHeader->setDeblockingFilterTcOffsetDiv2  ( 0 );
    picHeader->setDeblockingFilterCbBetaOffsetDiv2( 0 );
    picHeader->setDeblockingFilterCbTcOffsetDiv2  ( 0 );
    picHeader->setDeblockingFilterCrBetaOffsetDiv2( 0 );
    picHeader->setDeblockingFilterCrTcOffsetDiv2  ( 0 );
  }


  // picture header extension
  if(pps->getPictureHeaderExtensionPresentFlag())
  {
    xWriteUvlc(0,"ph_extension_length");
  }

  if ( writeRbspTrailingBits )
  {
    xWriteRbspTrailingBits();
  }
}

void HLSWriter::codeSliceHeader         ( Slice* pcSlice, PicHeader *picHeader )
{
#if ENABLE_TRACING
  xTraceSliceHeader ();
#endif

  if (!picHeader)
  {
    CodingStructure& cs = *pcSlice->getPic()->cs;
    picHeader = cs.picHeader;
  }
  const ChromaFormat format                = pcSlice->getSPS()->getChromaFormatIdc();
  const uint32_t         numberValidComponents = getNumberValidComponents(format);
  const bool         chromaEnabled         = isChromaEnabled(format);
  xWriteFlag(pcSlice->getPictureHeaderInSliceHeader() ? 1 : 0, "sh_picture_header_in_slice_header_flag");
  if (pcSlice->getPictureHeaderInSliceHeader())
  {
    codePictureHeader(picHeader, false);
#if GDR_ENC_TRACE
    printf("-gdr_pic_flag:%d\n", picHeader->getGdrPicFlag());
    printf("-recovery_poc_cnt:%d\n", picHeader->getRecoveryPocCnt());
    printf("-InGdrInterval:%d\n", pcSlice->getPic()->gdrParam.inGdrInterval);
    printf("-pic_lmcs_enabled_flag:%d\n", picHeader->getLmcsEnabledFlag() ? 1 : 0);
    printf("-pic_chroma_residual_scale_flag:%d\n", picHeader->getLmcsChromaResidualScaleFlag() ? 1 : 0);
#endif
  }

  if (pcSlice->getSPS()->getSubPicInfoPresentFlag())
  {
    uint32_t bitsSubPicId;
    bitsSubPicId = pcSlice->getSPS()->getSubPicIdLen();
    xWriteCode(pcSlice->getSliceSubPicId(), bitsSubPicId, "sh_subpic_id");
  }

  // raster scan slices
  if( pcSlice->getPPS()->getRectSliceFlag() == 0 )
  {
    // slice address is the raster scan tile index of first tile in slice
    if( pcSlice->getPPS()->getNumTiles() > 1 )
    {
      int bitsSliceAddress = ceilLog2(pcSlice->getPPS()->getNumTiles());
      xWriteCode( pcSlice->getSliceID(), bitsSliceAddress, "sh_slice_address");
      if ((int)pcSlice->getPPS()->getNumTiles() - (int)pcSlice->getSliceID() > 1)
      {
        xWriteUvlc(pcSlice->getNumTilesInSlice() - 1, "sh_num_tiles_in_slice_minus1");
      }
    }
  }
  // rectangular slices
  else
  {
    // slice address is the index of the slice within the current sub-picture
    uint32_t currSubPicIdx = pcSlice->getPPS()->getSubPicIdxFromSubPicId( pcSlice->getSliceSubPicId() );
    SubPic currSubPic = pcSlice->getPPS()->getSubPic(currSubPicIdx);
    if( currSubPic.getNumSlicesInSubPic() > 1 )
    {
      int numSlicesInPreviousSubPics = 0;
      for(int sp = 0; sp < currSubPicIdx; sp++)
      {
        numSlicesInPreviousSubPics += pcSlice->getPPS()->getSubPic(sp).getNumSlicesInSubPic();
      }
      int bitsSliceAddress = ceilLog2(currSubPic.getNumSlicesInSubPic());
      xWriteCode( pcSlice->getSliceID() - numSlicesInPreviousSubPics, bitsSliceAddress, "sh_slice_address");
    }
  }

  if (picHeader->getPicInterSliceAllowedFlag())
  {
    xWriteUvlc(pcSlice->getSliceType(), "sh_slice_type");
  }
  if (pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR)
  {
    xWriteFlag(pcSlice->getNoOutputOfPriorPicsFlag(), "sh_no_output_of_prior_pics_flag");
  }
  if (!picHeader->getPicIntraSliceAllowedFlag())
  {
    CHECK(pcSlice->getSliceType() == I_SLICE, "when ph_intra_slice_allowed_flag = 0, no I_Slice is allowed");
  }

  if (pcSlice->getSPS()->getALFEnabledFlag() && !pcSlice->getPPS()->getAlfInfoInPhFlag())
  {
    const int alfEnabled = pcSlice->getAlfEnabledFlag(COMPONENT_Y);
    xWriteFlag(alfEnabled, "sh_alf_enabled_flag");

    if (alfEnabled)
    {
      xWriteCode(pcSlice->getNumAlfApsIdsLuma(), 3, "sh_num_alf_aps_ids_luma");
      const AlfApsList &apsId = pcSlice->getAlfApsIdsLuma();
      for (int i = 0; i < pcSlice->getNumAlfApsIdsLuma(); i++)
      {
        xWriteCode(apsId[i], 3, "sh_alf_aps_id_luma[i]");
      }

      const int alfChromaIdc = pcSlice->getAlfEnabledFlag(COMPONENT_Cb) + pcSlice->getAlfEnabledFlag(COMPONENT_Cr) * 2;
      if (chromaEnabled)
      {
        xWriteCode(pcSlice->getAlfEnabledFlag(COMPONENT_Cb), 1, "sh_alf_cb_enabled_flag");
        xWriteCode(pcSlice->getAlfEnabledFlag(COMPONENT_Cr), 1, "sh_alf_cr_enabled_flag");
      }
      if (alfChromaIdc)
      {
        xWriteCode(pcSlice->getAlfApsIdChroma(), 3, "sh_alf_aps_id_chroma");
      }

      if (pcSlice->getSPS()->getCCALFEnabledFlag())
      {
        CcAlfFilterParam &filterParam = pcSlice->m_ccAlfFilterParam;
        xWriteFlag(filterParam.ccAlfFilterEnabled[COMPONENT_Cb - 1] ? 1 : 0, "sh_alf_cc_cb_enabled_flag");
        if (filterParam.ccAlfFilterEnabled[COMPONENT_Cb - 1])
        {
          // write CC ALF Cb APS ID
          xWriteCode(pcSlice->getCcAlfCbApsId(), 3, "sh_alf_cc_cb_aps_id");
        }
        // Cr
        xWriteFlag(filterParam.ccAlfFilterEnabled[COMPONENT_Cr - 1] ? 1 : 0, "sh_alf_cc_cr_enabled_flag");
        if (filterParam.ccAlfFilterEnabled[COMPONENT_Cr - 1])
        {
          // write CC ALF Cr APS ID
          xWriteCode(pcSlice->getCcAlfCrApsId(), 3, "sh_alf_cc_cr_aps_id");
        }
      }
    }
  }

  if (picHeader->getLmcsEnabledFlag() && !pcSlice->getPictureHeaderInSliceHeader())
  {
    xWriteFlag(pcSlice->getLmcsEnabledFlag(), "sh_lmcs_used_flag");
  }
  if (picHeader->getExplicitScalingListEnabledFlag() && !pcSlice->getPictureHeaderInSliceHeader())
  {
    xWriteFlag(pcSlice->getExplicitScalingListUsed(), "sh_explicit_scaling_list_used_flag");
  }

  if( !pcSlice->getPPS()->getRplInfoInPhFlag() && (!pcSlice->getIdrPicFlag() || pcSlice->getSPS()->getIDRRefParamListPresent()))
  {
    // Write L0 related syntax elements
    const int numRplsInSps = pcSlice->getSPS()->getNumRpl(REF_PIC_LIST_0);
    const int rplIdx       = pcSlice->getRplIdx(REF_PIC_LIST_0);

    if (numRplsInSps > 0)
    {
      xWriteFlag(rplIdx != -1 ? 1 : 0, "ref_pic_list_sps_flag[0]");
    }
    if (rplIdx != -1)
    {
      if (numRplsInSps > 1)
      {
        int numBits = ceilLog2(numRplsInSps);
        xWriteCode(rplIdx, numBits, "ref_pic_list_idx[0]");
      }
    }
    else
    {   // write local RPL0
      xCodeRefPicList(pcSlice->getRpl(REF_PIC_LIST_0), pcSlice->getSPS()->getLongTermRefsPresent(),
                      pcSlice->getSPS()->getBitsForPOC(),
                      !pcSlice->getSPS()->getUseWP() && !pcSlice->getSPS()->getUseWPBiPred(), -1);
    }
    // Deal POC Msb cycle signalling for LTRP
    if (pcSlice->getRpl(REF_PIC_LIST_0)->getNumberOfLongtermPictures())
    {
      for (int i = 0; i < pcSlice->getRpl(REF_PIC_LIST_0)->getNumRefEntries(); i++)
      {
        if (pcSlice->getRpl(REF_PIC_LIST_0)->isRefPicLongterm(i))
        {
          if (pcSlice->getRpl(REF_PIC_LIST_0)->getLtrpInSliceHeaderFlag())
          {
            xWriteCode(pcSlice->getRpl(REF_PIC_LIST_0)->getRefPicIdentifier(i), pcSlice->getSPS()->getBitsForPOC(),
                       "slice_poc_lsb_lt[listIdx][rplsIdx][j]");
          }
          xWriteFlag(pcSlice->getRpl(REF_PIC_LIST_0)->getDeltaPocMSBPresentFlag(i) ? 1 : 0,
                     "delta_poc_msb_present_flag[i][j]");
          if (pcSlice->getRpl(REF_PIC_LIST_0)->getDeltaPocMSBPresentFlag(i))
          {
            xWriteUvlc(pcSlice->getRpl(REF_PIC_LIST_0)->getDeltaPocMSBCycleLT(i), "delta_poc_msb_cycle_lt[i][j]");
          }
        }
      }
    }

    // Write L1 related syntax elements
    if (pcSlice->getSPS()->getNumRpl(REF_PIC_LIST_1) > 0 && pcSlice->getPPS()->getRpl1IdxPresentFlag())
    {
      xWriteFlag(pcSlice->getRplIdx(REF_PIC_LIST_1) != -1 ? 1 : 0, "ref_pic_list_sps_flag[1]");
    }
    else if (pcSlice->getSPS()->getNumRpl(REF_PIC_LIST_1) == 0)
    {
      CHECK(pcSlice->getRplIdx(REF_PIC_LIST_1) != -1,
            "rpl_sps_flag[1] will be infer to 0 and this is not what was expected");
    }
    else
    {
      auto rplsSpsFlag0 = pcSlice->getRplIdx(REF_PIC_LIST_0) != -1 ? 1 : 0;
      auto rplsSpsFlag1 = pcSlice->getRplIdx(REF_PIC_LIST_1) != -1 ? 1 : 0;
      CHECK(rplsSpsFlag1 != rplsSpsFlag0, "rpl_sps_flag[1] will be infer to 0 and this is not what was expected");
    }

    if (pcSlice->getRplIdx(REF_PIC_LIST_1) != -1)
    {
      if (pcSlice->getSPS()->getNumRpl(REF_PIC_LIST_1) > 1 && pcSlice->getPPS()->getRpl1IdxPresentFlag())
      {
        int numBits = ceilLog2(pcSlice->getSPS()->getNumRpl(REF_PIC_LIST_1));
        xWriteCode(pcSlice->getRplIdx(REF_PIC_LIST_1), numBits, "ref_pic_list_idx[1]");
      }
      else if (pcSlice->getSPS()->getNumRpl(REF_PIC_LIST_1) == 1)
      {
        CHECK(pcSlice->getRplIdx(REF_PIC_LIST_1) != 0, "RPL1Idx is not signalled but it is not equal to 0");
      }
      else
      {
        CHECK(pcSlice->getRplIdx(REF_PIC_LIST_1) != pcSlice->getRplIdx(REF_PIC_LIST_0),
              "RPL1Idx is not signalled but it is not the same as RPL0Idx");
      }
    }
    else
    {   // write local RPL1
      xCodeRefPicList(pcSlice->getRpl(REF_PIC_LIST_1), pcSlice->getSPS()->getLongTermRefsPresent(),
                      pcSlice->getSPS()->getBitsForPOC(),
                      !pcSlice->getSPS()->getUseWP() && !pcSlice->getSPS()->getUseWPBiPred(), -1);
    }
    // Deal POC Msb cycle signalling for LTRP
    if (pcSlice->getRpl(REF_PIC_LIST_1)->getNumberOfLongtermPictures())
    {
      for (int i = 0; i < pcSlice->getRpl(REF_PIC_LIST_1)->getNumRefEntries(); i++)
      {
        if (pcSlice->getRpl(REF_PIC_LIST_1)->isRefPicLongterm(i))
        {
          if (pcSlice->getRpl(REF_PIC_LIST_1)->getLtrpInSliceHeaderFlag())
          {
            xWriteCode(pcSlice->getRpl(REF_PIC_LIST_1)->getRefPicIdentifier(i), pcSlice->getSPS()->getBitsForPOC(),
                       "slice_poc_lsb_lt[listIdx][rplsIdx][j]");
          }
          xWriteFlag(pcSlice->getRpl(REF_PIC_LIST_1)->getDeltaPocMSBPresentFlag(i) ? 1 : 0,
                     "delta_poc_msb_present_flag[i][j]");
          if (pcSlice->getRpl(REF_PIC_LIST_1)->getDeltaPocMSBPresentFlag(i))
          {
            xWriteUvlc(pcSlice->getRpl(REF_PIC_LIST_1)->getDeltaPocMSBCycleLT(i), "delta_poc_msb_cycle_lt[i][j]");
          }
        }
      }
    }
  }

  // check if numbers of active references match the defaults. If not, override

  CHECK(pcSlice->isIntra() && pcSlice->getNumRefIdx(REF_PIC_LIST_0) > 0, "Bad number of refs");
  CHECK(!pcSlice->isInterB() && pcSlice->getNumRefIdx(REF_PIC_LIST_1) > 0, "Bad number of refs");

  if ((!pcSlice->isIntra() && pcSlice->getRpl(REF_PIC_LIST_0)->getNumRefEntries() > 1)
      || (pcSlice->isInterB() && pcSlice->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 1))
  {
    const int defaultL0 = std::min(pcSlice->getRpl(REF_PIC_LIST_0)->getNumRefEntries(),
                                   pcSlice->getPPS()->getNumRefIdxDefaultActive(REF_PIC_LIST_0));

    bool overrideFlag = pcSlice->getNumRefIdx(REF_PIC_LIST_0) != defaultL0;

    if (!overrideFlag && pcSlice->isInterB())
    {
      const int defaultL1 = std::min(pcSlice->getRpl(REF_PIC_LIST_1)->getNumRefEntries(),
                                     pcSlice->getPPS()->getNumRefIdxDefaultActive(REF_PIC_LIST_1));

      overrideFlag = pcSlice->getNumRefIdx(REF_PIC_LIST_1) != defaultL1;
    }

    xWriteFlag(overrideFlag ? 1 : 0, "sh_num_ref_idx_active_override_flag");
    if (overrideFlag)
    {
      if (pcSlice->getRpl(REF_PIC_LIST_0)->getNumRefEntries() > 1)
      {
        xWriteUvlc(pcSlice->getNumRefIdx(REF_PIC_LIST_0) - 1, "sh_num_ref_idx_active_minus1[0]");
      }

      if (pcSlice->isInterB() && pcSlice->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 1)
      {
        xWriteUvlc(pcSlice->getNumRefIdx(REF_PIC_LIST_1) - 1, "sh_num_ref_idx_active_minus1[1]");
      }
    }
  }

  if (!pcSlice->isIntra())
  {
    if (!pcSlice->isIntra() && pcSlice->getPPS()->getCabacInitPresentFlag())
    {
      SliceType sliceType        = pcSlice->getSliceType();
      SliceType encCABACTableIdx = pcSlice->getEncCABACTableIdx();
      bool      encCabacInitFlag = (sliceType != encCABACTableIdx && encCABACTableIdx != I_SLICE) ? true : false;
      pcSlice->setCabacInitFlag(encCabacInitFlag);
      xWriteFlag(encCabacInitFlag ? 1 : 0, "sh_cabac_init_flag");
    }
  }
  if (pcSlice->getPicHeader()->getEnableTMVPFlag() && !pcSlice->getPPS()->getRplInfoInPhFlag())
  {
    if (!pcSlice->getPPS()->getRplInfoInPhFlag())
    {
      if (pcSlice->getSliceType() == B_SLICE)
      {
        xWriteFlag(pcSlice->getColFromL0Flag(), "sh_collocated_from_l0_flag");
      }
    }

    if (pcSlice->getSliceType() != I_SLICE
        && ((pcSlice->getColFromL0Flag() == 1 && pcSlice->getNumRefIdx(REF_PIC_LIST_0) > 1)
            || (pcSlice->getColFromL0Flag() == 0 && pcSlice->getNumRefIdx(REF_PIC_LIST_1) > 1)))
    {
      xWriteUvlc(pcSlice->getColRefIdx(), "sh_collocated_ref_idx");
    }
  }

  if ((pcSlice->getPPS()->getUseWP() && pcSlice->getSliceType() == P_SLICE)
      || (pcSlice->getPPS()->getWPBiPred() && pcSlice->getSliceType() == B_SLICE))
  {
    if (!pcSlice->getPPS()->getWpInfoInPhFlag())
    {
      xCodePredWeightTable(pcSlice);
    }
  }

  if (!pcSlice->getPPS()->getQpDeltaInfoInPhFlag())
  {
    xWriteSvlc(pcSlice->getSliceQp() - (pcSlice->getPPS()->getPicInitQPMinus26() + QP26), "sh_qp_delta");
  }
  if (pcSlice->getPPS()->getSliceChromaQpFlag())
  {
    if (numberValidComponents > COMPONENT_Cb)
    {
      xWriteSvlc(pcSlice->getSliceChromaQpDelta(COMPONENT_Cb), "sh_cb_qp_offset");
    }
    if (numberValidComponents > COMPONENT_Cr)
    {
      xWriteSvlc(pcSlice->getSliceChromaQpDelta(COMPONENT_Cr), "sh_cr_qp_offset");
      if (pcSlice->getSPS()->getJointCbCrEnabledFlag())
      {
        xWriteSvlc(pcSlice->getSliceChromaQpDelta(JOINT_CbCr), "sh_joint_cbcr_qp_offset");
      }
    }
    CHECK(numberValidComponents < COMPONENT_Cr + 1, "Too many valid components");
  }

  if (pcSlice->getPPS()->getCuChromaQpOffsetListEnabledFlag())
  {
    xWriteFlag(pcSlice->getUseChromaQpAdj(), "sh_cu_chroma_qp_offset_enabled_flag");
  }

  if (pcSlice->getSPS()->getSAOEnabledFlag() && !pcSlice->getPPS()->getSaoInfoInPhFlag())
  {
    xWriteFlag(pcSlice->getSaoEnabledFlag(ChannelType::LUMA), "sh_sao_luma_used_flag");
    if (chromaEnabled)
    {
      xWriteFlag(pcSlice->getSaoEnabledFlag(ChannelType::CHROMA), "sh_sao_chroma_used_flag");
    }
  }

  if (pcSlice->getPPS()->getDeblockingFilterControlPresentFlag())
  {
    if (pcSlice->getPPS()->getDeblockingFilterOverrideEnabledFlag() && !pcSlice->getPPS()->getDbfInfoInPhFlag())
    {
      xWriteFlag(pcSlice->getDeblockingFilterOverrideFlag(), "sh_deblocking_params_present_flag");
    }
    else
    {
      pcSlice->setDeblockingFilterOverrideFlag(false);
    }
    if (pcSlice->getDeblockingFilterOverrideFlag())
    {
      if (!pcSlice->getPPS()->getPPSDeblockingFilterDisabledFlag())
      {
        xWriteFlag(pcSlice->getDeblockingFilterDisable(), "sh_deblocking_filter_disabled_flag");
      }
      if (!pcSlice->getDeblockingFilterDisable())
      {
        xWriteSvlc(pcSlice->getDeblockingFilterBetaOffsetDiv2(), "sh_luma_beta_offset_div2");
        xWriteSvlc(pcSlice->getDeblockingFilterTcOffsetDiv2(), "sh_luma_tc_offset_div2");
        if (pcSlice->getPPS()->getPPSChromaToolFlag())
        {
          xWriteSvlc(pcSlice->getDeblockingFilterCbBetaOffsetDiv2(), "sh_cb_beta_offset_div2");
          xWriteSvlc(pcSlice->getDeblockingFilterCbTcOffsetDiv2(), "sh_cb_tc_offset_div2");
          xWriteSvlc(pcSlice->getDeblockingFilterCrBetaOffsetDiv2(), "sh_cr_beta_offset_div2");
          xWriteSvlc(pcSlice->getDeblockingFilterCrTcOffsetDiv2(), "sh_cr_tc_offset_div2");
        }
      }
    }
    else
    {
      pcSlice->setDeblockingFilterDisable(picHeader->getDeblockingFilterDisable());
      pcSlice->setDeblockingFilterBetaOffsetDiv2(picHeader->getDeblockingFilterBetaOffsetDiv2());
      pcSlice->setDeblockingFilterTcOffsetDiv2(picHeader->getDeblockingFilterTcOffsetDiv2());
      pcSlice->setDeblockingFilterCbBetaOffsetDiv2(picHeader->getDeblockingFilterCbBetaOffsetDiv2());
      pcSlice->setDeblockingFilterCbTcOffsetDiv2(picHeader->getDeblockingFilterCbTcOffsetDiv2());
      pcSlice->setDeblockingFilterCrBetaOffsetDiv2(picHeader->getDeblockingFilterCrBetaOffsetDiv2());
      pcSlice->setDeblockingFilterCrTcOffsetDiv2(picHeader->getDeblockingFilterCrTcOffsetDiv2());
    }
  }
  else
  {
    pcSlice->setDeblockingFilterDisable(false);
    pcSlice->setDeblockingFilterBetaOffsetDiv2(0);
    pcSlice->setDeblockingFilterTcOffsetDiv2(0);
    pcSlice->setDeblockingFilterCbBetaOffsetDiv2(0);
    pcSlice->setDeblockingFilterCbTcOffsetDiv2(0);
    pcSlice->setDeblockingFilterCrBetaOffsetDiv2(0);
    pcSlice->setDeblockingFilterCrTcOffsetDiv2(0);
  }

  // dependent quantization
  if( pcSlice->getSPS()->getDepQuantEnabledFlag() )
  {
    xWriteFlag(pcSlice->getDepQuantEnabledFlag(), "sh_dep_quant_used_flag");
  }
  else
  {
    pcSlice->setDepQuantEnabledFlag(false);
  }

  // sign data hiding
  if( pcSlice->getSPS()->getSignDataHidingEnabledFlag() && !pcSlice->getDepQuantEnabledFlag() )
  {
    xWriteFlag(pcSlice->getSignDataHidingEnabledFlag(), "sh_sign_data_hiding_used_flag" );
  }
  else
  {
    pcSlice->setSignDataHidingEnabledFlag(false);
  }

  // signal TS residual coding disabled flag
  if (pcSlice->getSPS()->getTransformSkipEnabledFlag() && !pcSlice->getDepQuantEnabledFlag() && !pcSlice->getSignDataHidingEnabledFlag())
  {
    xWriteFlag(pcSlice->getTSResidualCodingDisabledFlag() ? 1 : 0, "sh_ts_residual_coding_disabled_flag");
  }

  if ((!pcSlice->getTSResidualCodingDisabledFlag()) && (pcSlice->getSPS()->getSpsRangeExtension().getTSRCRicePresentFlag()))
  {
    xWriteCode(pcSlice->getTsrcIndex(), 3, "sh_ts_residual_coding_rice_idx_minus1");
  }
  if (pcSlice->getSPS()->getSpsRangeExtension().getReverseLastSigCoeffEnabledFlag())
  {
    xWriteFlag(pcSlice->getReverseLastSigCoeffFlag(), "sh_reverse_last_sig_coeff_flag");
  }
  if(pcSlice->getPPS()->getSliceHeaderExtensionPresentFlag())
  {
    xWriteUvlc(0,"sh_slice_header_extension_length");
  }
}

void  HLSWriter::codeConstraintInfo  ( const ConstraintInfo* cinfo, const ProfileTierLevel* ptl )
{
  xWriteFlag(cinfo->getGciPresentFlag(), "gci_present_flag");
  if (cinfo->getGciPresentFlag())
  {
    /* general */
    xWriteFlag(cinfo->getIntraOnlyConstraintFlag() ? 1 : 0, "gci_intra_only_constraint_flag");
    xWriteFlag(cinfo->getAllLayersIndependentConstraintFlag() ? 1 : 0, "gci_all_layers_independent_constraint_flag");
    xWriteFlag(cinfo->getOnePictureOnlyConstraintFlag() ? 1 : 0, "gci_one_au_only_constraint_flag");

    /* picture format */
    xWriteCode(16 - cinfo->getMaxBitDepthConstraintIdc(), 4, "gci_sixteen_minus_max_bitdepth_constraint_idc");
    xWriteCode(3 - to_underlying(cinfo->getMaxChromaFormatConstraintIdc()), 2,
               "gci_three_minus_max_chroma_format_constraint_idc");

    /* NAL unit type related */
    xWriteFlag(cinfo->getNoMixedNaluTypesInPicConstraintFlag() ? 1 : 0, "gci_no_mixed_nalu_types_in_pic_constraint_flag");
    xWriteFlag(cinfo->getNoTrailConstraintFlag() ? 1 : 0, "gci_no_trail_constraint_flag");
    xWriteFlag(cinfo->getNoStsaConstraintFlag() ? 1 : 0, "gci_no_stsa_constraint_flag");
    xWriteFlag(cinfo->getNoRaslConstraintFlag() ? 1 : 0, "gci_no_rasl_constraint_flag");
    xWriteFlag(cinfo->getNoRadlConstraintFlag() ? 1 : 0, "gci_no_radl_constraint_flag");
    xWriteFlag(cinfo->getNoIdrConstraintFlag() ? 1 : 0, "gci_no_idr_constraint_flag");
    xWriteFlag(cinfo->getNoCraConstraintFlag() ? 1 : 0, "gci_no_cra_constraint_flag");
    xWriteFlag(cinfo->getNoGdrConstraintFlag() ? 1 : 0, "gci_no_gdr_constraint_flag");
    xWriteFlag(cinfo->getNoApsConstraintFlag() ? 1 : 0, "gci_no_aps_constraint_flag");
    xWriteFlag(cinfo->getNoIdrRplConstraintFlag() ? 1: 0, "gci_no_idr_rpl_constraint_flag");

    /* tile, slice, subpicture partitioning */
    xWriteFlag(cinfo->getOneTilePerPicConstraintFlag() ? 1 : 0, "gci_one_tile_per_pic_constraint_flag");
    xWriteFlag(cinfo->getPicHeaderInSliceHeaderConstraintFlag() ? 1 : 0, "gci_pic_header_in_slice_header_constraint_flag");
    xWriteFlag(cinfo->getOneSlicePerPicConstraintFlag() ? 1 : 0, "gci_one_slice_per_pic_constraint_flag");
    xWriteFlag(cinfo->getNoRectSliceConstraintFlag() ? 1 : 0, "gci_no_rectangular_slice_constraint_flag");
    xWriteFlag(cinfo->getOneSlicePerSubpicConstraintFlag() ? 1 : 0, "gci_one_slice_per_subpic_constraint_flag");
    xWriteFlag(cinfo->getNoSubpicInfoConstraintFlag() ? 1 : 0, "gci_no_subpic_info_constraint_flag");


    /* CTU and block partitioning */
    xWriteCode(3 - (cinfo->getMaxLog2CtuSizeConstraintIdc() - 5), 2, "gci_three_minus_max_log2_ctu_size_constraint_idc");
    xWriteFlag(cinfo->getNoPartitionConstraintsOverrideConstraintFlag() ? 1 : 0, "gci_no_partition_constraints_override_constraint_flag");
    xWriteFlag(cinfo->getNoMttConstraintFlag() ? 1 : 0, "gci_no_mtt_constraint_flag");
    xWriteFlag(cinfo->getNoQtbttDualTreeIntraConstraintFlag() ? 1 : 0, "gci_no_qtbtt_dual_tree_intra_constraint_flag");

    /* intra */
    xWriteFlag(cinfo->getNoPaletteConstraintFlag() ? 1 : 0, "gci_no_palette_constraint_flag");
    xWriteFlag(cinfo->getNoIbcConstraintFlag() ? 1 : 0, "gci_no_ibc_constraint_flag");
    xWriteFlag(cinfo->getNoIspConstraintFlag() ? 1 : 0, "gci_no_isp_constraint_flag");
    xWriteFlag(cinfo->getNoMrlConstraintFlag() ? 1 : 0, "gci_no_mrl_constraint_flag");
    xWriteFlag(cinfo->getNoMipConstraintFlag() ? 1 : 0, "gci_no_mip_constraint_flag");
    xWriteFlag(cinfo->getNoCclmConstraintFlag() ? 1 : 0, "gci_no_cclm_constraint_flag");

    /* inter */
    xWriteFlag(cinfo->getNoRprConstraintFlag() ? 1 : 0, "gci_no_ref_pic_resampling_constraint_flag");
    xWriteFlag(cinfo->getNoResChangeInClvsConstraintFlag() ? 1 : 0, "gci_no_res_change_in_clvs_constraint_flag");
    xWriteFlag(cinfo->getNoWeightedPredictionConstraintFlag() ? 1 : 0, "gci_no_weighted_prediction_constraint_flag");
    xWriteFlag(cinfo->getNoRefWraparoundConstraintFlag() ? 1 : 0, "gci_no_ref_wraparound_constraint_flag");
    xWriteFlag(cinfo->getNoTemporalMvpConstraintFlag() ? 1 : 0, "gci_no_temporal_mvp_constraint_flag");
    xWriteFlag(cinfo->getNoSbtmvpConstraintFlag() ? 1 : 0, "gci_no_sbtmvp_constraint_flag");
    xWriteFlag(cinfo->getNoAmvrConstraintFlag() ? 1 : 0, "gci_no_amvr_constraint_flag");
    xWriteFlag(cinfo->getNoBdofConstraintFlag() ? 1 : 0, "gci_no_bdof_constraint_flag");
    xWriteFlag(cinfo->getNoSmvdConstraintFlag() ? 1 : 0, "gci_no_smvd_constraint_flag");
    xWriteFlag(cinfo->getNoDmvrConstraintFlag() ? 1 : 0, "gci_no_dmvr_constraint_flag");
    xWriteFlag(cinfo->getNoMmvdConstraintFlag() ? 1 : 0, "gci_no_mmvd_constraint_flag");
    xWriteFlag(cinfo->getNoAffineMotionConstraintFlag() ? 1 : 0, "gci_no_affine_motion_constraint_flag");
    xWriteFlag(cinfo->getNoProfConstraintFlag() ? 1 : 0, "gci_no_prof_constraint_flag");
    xWriteFlag(cinfo->getNoBcwConstraintFlag() ? 1 : 0, "gci_no_bcw_constraint_flag");
    xWriteFlag(cinfo->getNoCiipConstraintFlag() ? 1 : 0, "gci_no_ciip_constraint_flag");
    xWriteFlag(cinfo->getNoGeoConstraintFlag() ? 1 : 0, "gci_no_gpm_constraint_flag");

    /* transform, quantization, residual */
    xWriteFlag(cinfo->getNoLumaTransformSize64ConstraintFlag() ? 1 : 0, "gci_no_luma_transform_size_64_constraint_flag");
    xWriteFlag(cinfo->getNoTransformSkipConstraintFlag() ? 1 : 0, "gci_no_transform_skip_constraint_flag");
    xWriteFlag(cinfo->getNoBDPCMConstraintFlag() ? 1 : 0, "gci_no_bdpcm_constraint_flag");
    xWriteFlag(cinfo->getNoMtsConstraintFlag() ? 1 : 0, "gci_no_mts_constraint_flag");
    xWriteFlag(cinfo->getNoLfnstConstraintFlag() ? 1 : 0, "gci_no_lfnst_constraint_flag");
    xWriteFlag(cinfo->getNoJointCbCrConstraintFlag() ? 1 : 0, "gci_no_joint_cbcr_constraint_flag");
    xWriteFlag(cinfo->getNoSbtConstraintFlag() ? 1 : 0, "gci_no_sbt_constraint_flag");
    xWriteFlag(cinfo->getNoActConstraintFlag() ? 1 : 0, "gci_no_act_constraint_flag");
    xWriteFlag(cinfo->getNoExplicitScaleListConstraintFlag() ? 1 : 0, "gci_no_explicit_scaling_list_constraint_flag");
    xWriteFlag(cinfo->getNoDepQuantConstraintFlag() ? 1 : 0, "gci_no_dep_quant_constraint_flag");
    xWriteFlag(cinfo->getNoSignDataHidingConstraintFlag() ? 1 : 0, "gci_no_sign_data_hiding_constraint_flag");
    xWriteFlag(cinfo->getNoCuQpDeltaConstraintFlag() ? 1 : 0, "gci_no_cu_qp_delta_constraint_flag");
    xWriteFlag(cinfo->getNoChromaQpOffsetConstraintFlag() ? 1 : 0, "gci_no_chroma_qp_offset_constraint_flag");

    /* loop filter */
    xWriteFlag(cinfo->getNoSaoConstraintFlag() ? 1 : 0, "gci_no_sao_constraint_flag");
    xWriteFlag(cinfo->getNoAlfConstraintFlag() ? 1 : 0, "gci_no_alf_constraint_flag");
    xWriteFlag(cinfo->getNoCCAlfConstraintFlag() ? 1 : 0, "gci_no_ccalf_constraint_flag");
    xWriteFlag(cinfo->getNoLmcsConstraintFlag() ? 1 : 0, "gci_no_lmcs_constraint_flag");
    xWriteFlag(cinfo->getNoLadfConstraintFlag() ? 1 : 0, "gci_no_ladf_constraint_flag");
    xWriteFlag(cinfo->getNoVirtualBoundaryConstraintFlag() ? 1 : 0, "gci_no_virtual_boundaries_constraint_flag");
    Profile::Name profile = ptl->getProfileIdc();
    if (profile == Profile::MAIN_12 || profile == Profile::MAIN_12_INTRA || profile == Profile::MAIN_12_STILL_PICTURE ||
        profile == Profile::MAIN_12_444 || profile == Profile::MAIN_12_444_INTRA || profile == Profile::MAIN_12_444_STILL_PICTURE ||
        profile == Profile::MAIN_16_444 || profile == Profile::MAIN_16_444_INTRA || profile == Profile::MAIN_16_444_STILL_PICTURE)
    {
      int numAdditionalBits = 6;
      xWriteCode(numAdditionalBits, 8, "gci_num_additional_bits");
      xWriteFlag(cinfo->getAllRapPicturesFlag() ? 1 : 0, "gci_all_rap_pictures_flag");
      xWriteFlag(cinfo->getNoExtendedPrecisionProcessingConstraintFlag() ? 1 : 0, "gci_no_extended_precision_processing_constraint_flag");
      xWriteFlag(cinfo->getNoTsResidualCodingRiceConstraintFlag() ? 1 : 0, "gci_no_ts_residual_coding_rice_constraint_flag");
      xWriteFlag(cinfo->getNoRrcRiceExtensionConstraintFlag() ? 1 : 0, "gci_no_rrc_rice_extension_constraint_flag");
      xWriteFlag(cinfo->getNoPersistentRiceAdaptationConstraintFlag() ? 1 : 0, "gci_no_persistent_rice_adaptation_constraint_flag");
      xWriteFlag(cinfo->getNoReverseLastSigCoeffConstraintFlag() ? 1 : 0, "gci_no_reverse_last_sig_coeff_constraint_flag");
    }
    else
    {
      xWriteCode(0, 8, "gci_num_additional_bits");
    }
  }

  while (!isByteAligned())
  {
    xWriteFlag(0, "gci_alignment_zero_bit");
  }
}

void  HLSWriter::codeProfileTierLevel    ( const ProfileTierLevel* ptl, bool profileTierPresentFlag, int maxNumSubLayersMinus1 )
{
  if(profileTierPresentFlag)
  {
    xWriteCode( int(ptl->getProfileIdc()), 7 ,   "general_profile_idc"                     );
    xWriteFlag( ptl->getTierFlag()==Level::HIGH, "general_tier_flag"                       );
  }

  xWriteCode( int( ptl->getLevelIdc() ), 8, "general_level_idc" );

  xWriteFlag( ptl->getFrameOnlyConstraintFlag(), "ptl_frame_only_constraint_flag" );
  xWriteFlag( ptl->getMultiLayerEnabledFlag(),   "ptl_multilayer_enabled_flag"    );

  if(profileTierPresentFlag)
  {
    codeConstraintInfo(ptl->getConstraintInfo(), ptl);
  }

  for (int i = maxNumSubLayersMinus1 - 1; i >= 0; i--)
  {
    xWriteFlag( ptl->getSubLayerLevelPresentFlag(i),   "sub_layer_level_present_flag[i]" );
  }

  while (!isByteAligned())
  {
    xWriteFlag(0, "ptl_reserved_zero_bit");
  }

  for (int i = maxNumSubLayersMinus1 - 1; i >= 0; i--)
  {
    if( ptl->getSubLayerLevelPresentFlag(i) )
    {
      xWriteCode( int(ptl->getSubLayerLevelIdc(i)), 8, "sub_layer_level_idc[i]" );
    }
  }

  if (profileTierPresentFlag)
  {
    xWriteCode(ptl->getNumSubProfile(), 8, "ptl_num_sub_profiles");
    for (int i = 0; i < ptl->getNumSubProfile(); i++)
    {
      xWriteCode(ptl->getSubProfileIdc(i), 32, "general_sub_profile_idc[i]");
    }
  }
}


/**
* Write tiles and wavefront substreams sizes for the slice header (entry points).
*
* \param pSlice Slice structure that contains the substream size information.
*/
void  HLSWriter::codeTilesWPPEntryPoint( Slice* pSlice )
{
  pSlice->setNumEntryPoints( pSlice->getSPS(), pSlice->getPPS() );
  if( pSlice->getNumEntryPoints() == 0 )
  {
    return;
  }
  uint32_t maxOffset = 0;
  for(int idx=0; idx<pSlice->getNumberOfSubstreamSizes(); idx++)
  {
    uint32_t offset=pSlice->getSubstreamSize(idx);
    if ( offset > maxOffset )
    {
      maxOffset = offset;
    }
  }

  // Determine number of bits "offsetLenMinus1+1" required for entry point information
  uint32_t offsetLenMinus1 = 0;
  while (maxOffset >= (1u << (offsetLenMinus1 + 1)))
  {
    offsetLenMinus1++;
    CHECK(offsetLenMinus1 + 1 >= 32, "Invalid offset length minus 1");
  }

  if (pSlice->getNumberOfSubstreamSizes()>0)
  {
    xWriteUvlc(offsetLenMinus1, "sh_entry_offset_len_minus1");
    for (uint32_t idx=0; idx<pSlice->getNumberOfSubstreamSizes(); idx++)
    {
      xWriteCode(pSlice->getSubstreamSize(idx)-1, offsetLenMinus1+1, "sh_entry_point_offset_minus1");
    }
  }
}


// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

//! Code weighted prediction tables
void HLSWriter::xCodePredWeightTable( Slice* pcSlice )
{
  WPScalingParam  *wp;
  const ChromaFormat format                    = pcSlice->getSPS()->getChromaFormatIdc();
  const uint32_t     numberValidComponents     = getNumberValidComponents(format);
  const bool         hasChroma                 = isChromaEnabled(format);
  uint32_t           totalSignalledWeightFlags = 0;

  wp = pcSlice->getWpScaling(REF_PIC_LIST_0, 0);

  xWriteUvlc(wp[COMPONENT_Y].log2WeightDenom, "luma_log2_weight_denom");

  if (hasChroma)
  {
    CHECK(wp[COMPONENT_Cb].log2WeightDenom != wp[COMPONENT_Cr].log2WeightDenom,
          "Chroma blocks of different size not supported");
    const int deltaDenom = (wp[COMPONENT_Cb].log2WeightDenom - wp[COMPONENT_Y].log2WeightDenom);
    xWriteSvlc(deltaDenom, "delta_chroma_log2_weight_denom");
  }

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const bool l0 = l == REF_PIC_LIST_0;

    if (!l0 && !pcSlice->isInterB())
    {
      continue;
    }

    // NOTE: wp[].log2WeightDenom and wp[].presentFlag are actually per-channel-type settings.

    for (int refIdx = 0; refIdx < pcSlice->getNumRefIdx(l); refIdx++)
    {
      wp = pcSlice->getWpScaling(l, refIdx);
      xWriteFlag(wp[COMPONENT_Y].presentFlag, (l0 ? "luma_weight_l0_flag[i]" : "luma_weight_l1_flag[i]"));
      totalSignalledWeightFlags += wp[COMPONENT_Y].presentFlag;
    }
    if (hasChroma)
    {
      for (int refIdx = 0; refIdx < pcSlice->getNumRefIdx(l); refIdx++)
      {
        wp = pcSlice->getWpScaling(l, refIdx);
        CHECK(wp[COMPONENT_Cb].presentFlag != wp[COMPONENT_Cr].presentFlag,
              "Inconsistent settings for chroma channels");
        xWriteFlag(wp[COMPONENT_Cb].presentFlag, (l0 ? "chroma_weight_l0_flag[i]" : "chroma_weight_l1_flag[i]"));
        totalSignalledWeightFlags += 2 * wp[COMPONENT_Cb].presentFlag;
      }
    }

    for (int refIdx = 0; refIdx < pcSlice->getNumRefIdx(l); refIdx++)
    {
      wp = pcSlice->getWpScaling(l, refIdx);
      if (wp[COMPONENT_Y].presentFlag)
      {
        int deltaWeight = (wp[COMPONENT_Y].codedWeight - (1 << wp[COMPONENT_Y].log2WeightDenom));
        xWriteSvlc(deltaWeight, (l0 ? "delta_luma_weight_l0[i]" : "delta_luma_weight_l1[i]"));
        xWriteSvlc(wp[COMPONENT_Y].codedOffset, (l0 ? "luma_offset_l0[i]" : "luma_offset_l1[i]"));
      }

      if (hasChroma)
      {
        if (wp[COMPONENT_Cb].presentFlag)
        {
          for (int j = COMPONENT_Cb; j < numberValidComponents; j++)
          {
            CHECK(wp[COMPONENT_Cb].log2WeightDenom != wp[COMPONENT_Cr].log2WeightDenom,
                  "Chroma blocks of different size not supported");
            int deltaWeight = (wp[j].codedWeight - (1 << wp[COMPONENT_Cb].log2WeightDenom));
            xWriteSvlc(deltaWeight, (l0 ? "delta_chroma_weight_l0[i]" : "delta_chroma_weight_l1[i]"));

            int range       = pcSlice->getSPS()->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag()
                                ? (1 << pcSlice->getSPS()->getBitDepth(ChannelType::CHROMA)) / 2
                                : 128;
            int pred        = (range - ((range * wp[j].codedWeight) >> (wp[j].log2WeightDenom)));
            int deltaChroma = (wp[j].codedOffset - pred);
            xWriteSvlc(deltaChroma, (l0 ? "delta_chroma_offset_l0[i]" : "delta_chroma_offset_l1[i]"));
          }
        }
      }
    }
  }
  CHECK(totalSignalledWeightFlags > 24, "Too many signalled weight flags");
}

void HLSWriter::xCodePredWeightTable(PicHeader *picHeader, const PPS *pps, const SPS *sps)
{
  WPScalingParam *   wp;
  const ChromaFormat format                      = sps->getChromaFormatIdc();
  const uint32_t     numberValidComponents       = getNumberValidComponents(format);
  const bool         chroma                      = isChromaEnabled(format);
  uint32_t           totalSignalledWeightFlags   = 0;

  wp = picHeader->getWpScaling(REF_PIC_LIST_0, 0);
  xWriteUvlc(wp[COMPONENT_Y].log2WeightDenom, "luma_log2_weight_denom");

  if (chroma)
  {
    CHECK(wp[COMPONENT_Cb].log2WeightDenom != wp[COMPONENT_Cr].log2WeightDenom,
          "Chroma blocks of different size not supported");
    const int deltaDenom = (wp[COMPONENT_Cb].log2WeightDenom - wp[COMPONENT_Y].log2WeightDenom);
    xWriteSvlc(deltaDenom, "delta_chroma_log2_weight_denom");
  }

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const bool l0 = l == REF_PIC_LIST_0;

    int numLxWeights = 0;
    if (l0 || pps->getWPBiPred())
    {
      numLxWeights = picHeader->getNumWeights(l);
      xWriteUvlc(numLxWeights, (l0 ? "num_l0_weights" : "num_l1_weights"));
    }

    for (int refIdx = 0; refIdx < numLxWeights; refIdx++)
    {
      wp = picHeader->getWpScaling(l, refIdx);
      xWriteFlag(wp[COMPONENT_Y].presentFlag, (l0 ? "luma_weight_l0_flag[i]" : "luma_weight_l1_flag[i]"));
      totalSignalledWeightFlags += wp[COMPONENT_Y].presentFlag;
    }

    if (chroma)
    {
      for (int refIdx = 0; refIdx < numLxWeights; refIdx++)
      {
        wp = picHeader->getWpScaling(l, refIdx);
        CHECK(wp[COMPONENT_Cb].presentFlag != wp[COMPONENT_Cr].presentFlag,
              "Inconsistent settings for chroma channels");
        xWriteFlag(wp[COMPONENT_Cb].presentFlag, (l0 ? "chroma_weight_l0_flag[i]" : "chroma_weight_l1_flag[i]"));
        totalSignalledWeightFlags += 2 * wp[COMPONENT_Cb].presentFlag;
      }
    }

    for (int refIdx = 0; refIdx < numLxWeights; refIdx++)
    {
      wp = picHeader->getWpScaling(l, refIdx);
      if (wp[COMPONENT_Y].presentFlag)
      {
        int deltaWeight = (wp[COMPONENT_Y].codedWeight - (1 << wp[COMPONENT_Y].log2WeightDenom));
        xWriteSvlc(deltaWeight, (l0 ? "delta_luma_weight_l0[i]" : "delta_luma_weight_l1[i]"));
        xWriteSvlc(wp[COMPONENT_Y].codedOffset, (l0 ? "luma_offset_l0[i]" : "luma_offset_l1[i]"));
      }

      if (chroma)
      {
        if (wp[COMPONENT_Cb].presentFlag)
        {
          for (int j = COMPONENT_Cb; j < numberValidComponents; j++)
          {
            CHECK(wp[COMPONENT_Cb].log2WeightDenom != wp[COMPONENT_Cr].log2WeightDenom,
                  "Chroma blocks of different size not supported");
            int deltaWeight = (wp[j].codedWeight - (1 << wp[COMPONENT_Cb].log2WeightDenom));
            xWriteSvlc(deltaWeight, (l0 ? "delta_chroma_weight_l0[i]" : "delta_chroma_weight_l1[i]"));

            int range       = sps->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag()
                                ? (1 << sps->getBitDepth(ChannelType::CHROMA)) / 2
                                : 128;
            int pred        = (range - ((range * wp[j].codedWeight) >> (wp[j].log2WeightDenom)));
            int deltaChroma = (wp[j].codedOffset - pred);
            xWriteSvlc(deltaChroma, (l0 ? "delta_chroma_offset_l0[i]" : "delta_chroma_offset_l1[i]"));
          }
        }
      }
    }
  }

  CHECK(totalSignalledWeightFlags > 24, "Too many signalled weight flags");
}

/** code quantization matrix
*  \param scalingList quantization matrix information
*/
void HLSWriter::codeScalingList( const ScalingList &scalingList, bool aps_chromaPresentFlag )
{
  //for each size
  for (uint32_t scalingListId = 0; scalingListId < 28; scalingListId++)
  {
    if (aps_chromaPresentFlag || scalingList.isLumaScalingList(scalingListId))
    {
      bool scalingListCopyModeFlag = scalingList.getScalingListCopyModeFlag(scalingListId);
      xWriteFlag(scalingListCopyModeFlag, "scaling_list_copy_mode_flag");   // copy mode
      if (!scalingListCopyModeFlag)                                         // Copy Mode
      {
        xWriteFlag(scalingList.getScalingListPreditorModeFlag(scalingListId), "scaling_list_predictor_mode_flag");
      }
      if ((scalingListCopyModeFlag || scalingList.getScalingListPreditorModeFlag(scalingListId))
          && scalingListId != SCALING_LIST_1D_START_2x2 && scalingListId != SCALING_LIST_1D_START_4x4
          && scalingListId != SCALING_LIST_1D_START_8x8)
      {
        xWriteUvlc((int) scalingListId - (int) scalingList.getRefMatrixId(scalingListId),
                   "scaling_list_pred_matrix_id_delta");
      }
      if (!scalingListCopyModeFlag)
      {
        // DPCM
        xCodeScalingList(&scalingList, scalingListId, scalingList.getScalingListPreditorModeFlag(scalingListId));
      }
    }
  }
  return;
}
/** code DPCM
* \param scalingList quantization matrix information
* \param sizeId      size index
* \param listId      list index
*/
void HLSWriter::xCodeScalingList(const ScalingList* scalingList, uint32_t scalingListId, bool isPredictor)
{
  int matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : ((scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8);
  int coefNum = matrixSize * matrixSize;
  ScanElement *scan = g_scanOrder[SCAN_UNGROUPED][CoeffScanType::DIAG][gp_sizeIdxInfo->idxFrom(matrixSize)][gp_sizeIdxInfo->idxFrom(matrixSize)];
  int nextCoef = (isPredictor) ? 0 : SCALING_LIST_START_VALUE;

  int data;
  const int *src = scalingList->getScalingListAddress(scalingListId);
  int PredListId = scalingList->getRefMatrixId(scalingListId);
  const int *srcPred      = (isPredictor)
                              ? ((scalingListId == PredListId) ? scalingList->getScalingListDefaultAddress(scalingListId)
                                                               : scalingList->getScalingListAddress(PredListId))
                              : nullptr;
  int deltasrc[65] = { 0 };

  if (isPredictor)
  {
    if (scalingListId >= SCALING_LIST_1D_START_16x16)
    {
      deltasrc[64] = scalingList->getScalingListDC(scalingListId) - ((PredListId >= SCALING_LIST_1D_START_16x16) ? ((scalingListId == PredListId) ? 16 : scalingList->getScalingListDC(PredListId)) : srcPred[scan[0].idx]);
    }
    for (int i = 0; i < coefNum; i++)
    {
      deltasrc[i] = (src[scan[i].idx] - srcPred[scan[i].idx]);
    }
  }
  if (scalingListId >= SCALING_LIST_1D_START_16x16)
  {
    if (isPredictor)
    {
      data = deltasrc[64];
      nextCoef = deltasrc[64];
    }
    else
    {
      data = scalingList->getScalingListDC(scalingListId) - nextCoef;
      nextCoef = scalingList->getScalingListDC(scalingListId);
    }
    data = ((data + 128) & 255) - 128;
    xWriteSvlc((int8_t)data, "scaling_list_dc_coef");
  }
  for(int i=0;i<coefNum;i++)
  {
    if (scalingListId >= SCALING_LIST_1D_START_64x64 && scan[i].x >= 4 && scan[i].y >= 4)
    {
      continue;
    }
    data = (isPredictor) ? (deltasrc[i] - nextCoef) : (src[scan[i].idx] - nextCoef);
    nextCoef = (isPredictor) ? deltasrc[i] : src[scan[i].idx];
    data = ((data + 128) & 255) - 128;
    xWriteSvlc((int8_t)data, "scaling_list_delta_coef");
  }
}

bool HLSWriter::xFindMatchingLTRP(Slice* pcSlice, uint32_t *ltrpsIndex, int ltrpPOC, bool usedFlag)
{
  // bool state = true, state2 = false;
  int lsb = ltrpPOC & ((1<<pcSlice->getSPS()->getBitsForPOC())-1);
  for (int k = 0; k < pcSlice->getSPS()->getNumLongTermRefPicSPS(); k++)
  {
    if ( (lsb == pcSlice->getSPS()->getLtRefPicPocLsbSps(k)) && (usedFlag == pcSlice->getSPS()->getUsedByCurrPicLtSPSFlag(k)) )
    {
      *ltrpsIndex = k;
      return true;
    }
  }
  return false;
}


void HLSWriter::alfFilter( const AlfParam& alfParam, const bool isChroma, const int altIdx )
{
  AlfFilterShape alfShape(isChroma ? 5 : 7);
  const AlfCoeff* coeff      = isChroma ? alfParam.chromaCoeff[altIdx] : alfParam.lumaCoeff;
  const AlfClipIdx* clipp      = isChroma ? alfParam.chromaClipp[altIdx] : alfParam.lumaClipp;
  const int numFilters = isChroma ? 1 : alfParam.numLumaFilters;

  // vlc for all

  // Filter coefficients
  for( int ind = 0; ind < numFilters; ++ind )
  {
    for( int i = 0; i < alfShape.numCoeff - 1; i++ )
    {
      xWriteUvlc( abs(coeff[ ind* MAX_NUM_ALF_LUMA_COEFF + i ]), isChroma ? "alf_chroma_coeff_abs" : "alf_luma_coeff_abs" ); //alf_coeff_chroma[i], alf_coeff_luma_delta[i][j]
      if( abs( coeff[ ind* MAX_NUM_ALF_LUMA_COEFF + i ] ) != 0 )
      {
        xWriteFlag( ( coeff[ ind* MAX_NUM_ALF_LUMA_COEFF + i ] < 0 ) ? 1 : 0, isChroma ? "alf_chroma_coeff_sign" : "alf_luma_coeff_sign" );
      }
    }
  }

  // Clipping values coding
  if (alfParam.nonLinearFlag[isChroma ? ChannelType::CHROMA : ChannelType::LUMA])
  {
    for (int ind = 0; ind < numFilters; ++ind)
    {
      for (int i = 0; i < alfShape.numCoeff - 1; i++)
      {
        xWriteCode(clipp[ind* MAX_NUM_ALF_LUMA_COEFF + i], 2, isChroma ? "alf_chroma_clip_idx" : "alf_luma_clip_idx");
      }
    }
  }
}


//! \}
