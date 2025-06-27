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

/** \file     VLCWReader.cpp
 *  \brief    Reader for high level syntax
 */

//! \ingroup DecoderLib
//! \{

#include "VLCReader.h"

#include "CommonLib/CommonDef.h"
#include "CommonLib/dtrace_next.h"
#if RExt__DECODER_DEBUG_BIT_STATISTICS
#include "CommonLib/CodingStatistics.h"
#endif
#include "CommonLib/AdaptiveLoopFilter.h"
#include "CommonLib/ProfileTierLevel.h"

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

#if ENABLE_TRACING || RExt__DECODER_DEBUG_BIT_STATISTICS
void  VLCReader::xReadCode( const uint32_t length, uint32_t& value, const char *symbolName )
#else
void  VLCReader::xReadCode( const uint32_t length, uint32_t& value, const char* )
#endif
{
  CHECK(length == 0, "Reading a code of length '0'");
  m_pcBitstream->read(length, value);

#if RExt__DECODER_DEBUG_BIT_STATISTICS
  CodingStatistics::IncrementStatisticEP(symbolName, length, value);
#endif

#if ENABLE_TRACING
  if (length < 10)
  {
    DTRACE( g_trace_ctx, D_HEADER, "%-50s u(%d)  : %u\n", symbolName, length, value );
  }
  else
  {
    DTRACE( g_trace_ctx, D_HEADER, "%-50s u(%d) : %u\n", symbolName, length, value );
  }
#endif
}

#if ENABLE_TRACING || RExt__DECODER_DEBUG_BIT_STATISTICS
void  VLCReader::xReadUvlc(uint32_t& value, const char *symbolName )
#else
void  VLCReader::xReadUvlc(uint32_t& value, const char* )
#endif
{
  uint32_t suffix    = 0;
  uint32_t prefixBit = 0;
  m_pcBitstream->read( 1, prefixBit );

#if RExt__DECODER_DEBUG_BIT_STATISTICS
  uint32_t totalLen=1;
#endif

  if( 0 == prefixBit )
  {
    uint32_t length = 0;

    while( prefixBit == 0 )
    {
      m_pcBitstream->read( 1, prefixBit );
      length++;
    }

    m_pcBitstream->read(length, suffix);
    suffix += (1 << length) - 1;

#if RExt__DECODER_DEBUG_BIT_STATISTICS
    totalLen += length + length;
#endif
  }

  value = suffix;

#if RExt__DECODER_DEBUG_BIT_STATISTICS
  CodingStatistics::IncrementStatisticEP(symbolName, int(totalLen), value);
#endif

#if ENABLE_TRACING
  DTRACE( g_trace_ctx, D_HEADER, "%-50s ue(v) : %u\n", symbolName, value );
#endif
}

#if ENABLE_TRACING || RExt__DECODER_DEBUG_BIT_STATISTICS
void  VLCReader::xReadSvlc( int& value, const char *symbolName )
#else
void  VLCReader::xReadSvlc( int& value, const char* )
#endif
{
  uint32_t prefixBit = 0;
  uint32_t suffix = 0;

#if RExt__DECODER_DEBUG_BIT_STATISTICS
  uint32_t totalLen=1;
#endif

  m_pcBitstream->read(1, prefixBit);

  if (0 == prefixBit)
  {
    uint32_t length = 0;

    while ( prefixBit == 0 )
    {
      m_pcBitstream->read(1, prefixBit);
      length++;
    }

    m_pcBitstream->read(length, suffix);

    suffix += (1 << length);
    value = (suffix & 1) ? -(int) (suffix >> 1) : (int) (suffix >> 1);

#if RExt__DECODER_DEBUG_BIT_STATISTICS
    totalLen += length + length;
#endif
  }
  else
  {
    value = 0;
  }
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  CodingStatistics::IncrementStatisticEP(symbolName, int(totalLen), suffix);
#endif

#if ENABLE_TRACING
  DTRACE( g_trace_ctx, D_HEADER, "%-50s se(v) : %d\n", symbolName, value );
#endif
}

#if ENABLE_TRACING || RExt__DECODER_DEBUG_BIT_STATISTICS
void VLCReader::xReadFlag( uint32_t& value, const char *symbolName )
#else
void VLCReader::xReadFlag( uint32_t& value, const char* )
#endif
{
  m_pcBitstream->read( 1, value );

#if RExt__DECODER_DEBUG_BIT_STATISTICS
  CodingStatistics::IncrementStatisticEP(symbolName, 1, int(/*ruiCode*/0));
#endif

#if ENABLE_TRACING
  DTRACE( g_trace_ctx, D_HEADER, "%-50s u(1)  : %d\n", symbolName, value );
#endif
}

#if ENABLE_TRACING || RExt__DECODER_DEBUG_BIT_STATISTICS
void VLCReader::xReadString( std::string& valueOut, const char *symbolName )
#else
void VLCReader::xReadString( std::string& valueOut, const char*  )
#endif
{
  uint32_t code;
  std::string value( "" );;
  do
  {
    m_pcBitstream->read(8, code);
    if (code != 0)
    {
      value += (char) code;
    }
  } while (code != 0);

#if ENABLE_TRACING
  DTRACE(g_trace_ctx, D_HEADER, "%-50s u(1)  : %s\n", symbolName, value.c_str());
#endif
  valueOut = value;
}

#if RExt__DECODER_DEBUG_BIT_STATISTICS || ENABLE_TRACING
void VLCReader::xReadSCode (const uint32_t length, int& value, const char *symbolName )
#else
void VLCReader::xReadSCode (const uint32_t length, int& value, const char* )
#endif
{
  uint32_t val;
  CHECK ( length < 1 || length > 32, "Syntax element length must be in range 1..32");
  m_pcBitstream->read (length, val);
  value = length>=32 ? int(val) : ( (-int( val & (uint32_t(1)<<(length-1)))) | int(val) );

#if RExt__DECODER_DEBUG_BIT_STATISTICS
  CodingStatistics::IncrementStatisticEP(symbolName, length, value);
#endif
#if ENABLE_TRACING
  if (length < 10)
  {
    DTRACE( g_trace_ctx, D_HEADER, "%-50s i(%d)  : %d\n", symbolName, length, value );
  }
  else
  {
    DTRACE( g_trace_ctx, D_HEADER, "%-50s i(%d) : %d\n", symbolName, length, value );
  }
#endif
}

void VLCReader::xReadRbspTrailingBits()
{
  uint32_t bit;
  xReadFlag( bit, "rbsp_stop_one_bit");
  CHECK(bit!=1, "Trailing bit not '1'");
  int cnt = 0;
  while (m_pcBitstream->getNumBitsUntilByteAligned())
  {
    xReadFlag( bit, "rbsp_alignment_zero_bit");
    CHECK(bit!=0, "Alignment bit is not '0'");
    cnt++;
  }
  CHECK(cnt >= 8, "Read more than '8' trailing bits");
}

void AUDReader::parseAccessUnitDelimiter(InputBitstream* bs, uint32_t &audIrapOrGdrAuFlag, uint32_t &picType)
{
  setBitstream(bs);

#if ENABLE_TRACING
  xTraceAccessUnitDelimiter();
#endif

  xReadFlag (audIrapOrGdrAuFlag, "aud_irap_or_gdr_au_flag");
  xReadCode (3, picType, "pic_type");
  xReadRbspTrailingBits();
}

void FDReader::parseFillerData(InputBitstream* bs, uint32_t &fdSize)
{
  setBitstream(bs);
#if ENABLE_TRACING
  xTraceFillerData();
#endif
  uint32_t ffByte;
  fdSize = 0;
  while( m_pcBitstream->getNumBitsLeft() >8 )
  {
    xReadCode (8, ffByte, "ff_byte");
    CHECK(ffByte!=0xff, "Invalid filler data : not '0xff'");
    fdSize++;
  }
  xReadRbspTrailingBits();
}

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

HLSyntaxReader::HLSyntaxReader()
{
}

HLSyntaxReader::~HLSyntaxReader()
{

}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

void HLSyntaxReader::copyRefPicList(SPS* sps, ReferencePictureList* sourceRpl, ReferencePictureList* destRpl)
{
  destRpl->setNumberOfShorttermPictures(sourceRpl->getNumberOfShorttermPictures());

  destRpl->setNumberOfInterLayerPictures(sps->getInterLayerPresentFlag() ? sourceRpl->getNumberOfInterLayerPictures()
                                                                         : 0);

  if( sps->getLongTermRefsPresent() )
  {
    destRpl->setLtrpInSliceHeaderFlag(sourceRpl->getLtrpInSliceHeaderFlag());
    destRpl->setNumberOfLongtermPictures(sourceRpl->getNumberOfLongtermPictures());
  }
  else
  {
    destRpl->setNumberOfLongtermPictures(0);
  }

  uint32_t numRefPic = destRpl->getNumRefEntries();

  for( int ii = 0; ii < numRefPic; ii++ )
  {
    destRpl->setRefPicIdentifier(ii, sourceRpl->getRefPicIdentifier(ii), sourceRpl->isRefPicLongterm(ii),
                                 sourceRpl->isInterLayerRefPic(ii), sourceRpl->getInterLayerRefPicIdx(ii));
  }
}

void HLSyntaxReader::parseRefPicList(SPS* sps, ReferencePictureList* rpl, int rplIdx)
{
  uint32_t code;
  xReadUvlc(code, "num_ref_entries[ listIdx ][ rplsIdx ]");
  uint32_t numRefPic = code;
  uint32_t numStrp = 0;
  uint32_t numLtrp = 0;
  uint32_t numIlrp = 0;

  if (sps->getLongTermRefsPresent() && numRefPic > 0 && rplIdx != -1)
  {
    xReadFlag(code, "ltrp_in_slice_header_flag[ listIdx ][ rplsIdx ]");
    rpl->setLtrpInSliceHeaderFlag(code);
  }
  else if(sps->getLongTermRefsPresent())
  {
    rpl->setLtrpInSliceHeaderFlag( 1 );
  }

  bool isLongTerm;
  int prevDelta = MAX_INT;
  int deltaValue = 0;
  bool firstSTRP = true;

  rpl->setInterLayerPresentFlag( sps->getInterLayerPresentFlag() );

  for (int ii = 0; ii < numRefPic; ii++)
  {
    uint32_t isInterLayerRefPic = 0;

    if( rpl->getInterLayerPresentFlag() )
    {
      xReadFlag( isInterLayerRefPic, "inter_layer_ref_pic_flag[ listIdx ][ rplsIdx ][ i ]" );

      if( isInterLayerRefPic )
      {
        xReadUvlc( code, "ilrp_idx[ listIdx ][ rplsIdx ][ i ]" );
        rpl->setRefPicIdentifier( ii, 0, true, true, code );
        numIlrp++;
      }
    }

    if( !isInterLayerRefPic )
    {
      isLongTerm = false;
      if (sps->getLongTermRefsPresent())
      {
        xReadFlag(code, "st_ref_pic_flag[ listIdx ][ rplsIdx ][ i ]");
        isLongTerm = (code == 1) ? false : true;
      }

      if (!isLongTerm)
      {
        xReadUvlc(code, "abs_delta_poc_st[ listIdx ][ rplsIdx ][ i ]");
        if ((!sps->getUseWP() && !sps->getUseWPBiPred()) || (ii == 0))
        {
          code++;
        }
        int readValue = code;
        if (readValue > 0)
        {
          xReadFlag(code, "strp_entry_sign_flag[ listIdx ][ rplsIdx ][ i ]");
          if (code)
          {
            readValue = -readValue;
          }
        }
        if (firstSTRP)
        {
          firstSTRP = false;
          prevDelta = deltaValue = readValue;
        }
        else
        {
          deltaValue = prevDelta + readValue;
          prevDelta  = deltaValue;
        }

        rpl->setRefPicIdentifier(ii, deltaValue, isLongTerm, false, 0);
        numStrp++;
      }
      else
      {
        if (!rpl->getLtrpInSliceHeaderFlag())
        {
          xReadCode(sps->getBitsForPOC(), code, "poc_lsb_lt[listIdx][rplsIdx][j]");
        }
        rpl->setRefPicIdentifier(ii, code, isLongTerm, false, 0);
        numLtrp++;
      }
    }
  }
  rpl->setNumberOfShorttermPictures(numStrp);
  rpl->setNumberOfLongtermPictures(numLtrp);
  rpl->setNumberOfInterLayerPictures( numIlrp );
}

void HLSyntaxReader::parsePPS( PPS* pcPPS )
{
#if ENABLE_TRACING
  xTracePPSHeader ();
#endif
  uint32_t  uiCode;

  int   iCode;
  xReadCode(6, uiCode, "pps_pic_parameter_set_id");
  CHECK(uiCode > 63, "PPS id exceeds boundary (63)");
  pcPPS->setPPSId (uiCode);

  xReadCode(4, uiCode, "pps_seq_parameter_set_id");
  pcPPS->setSPSId (uiCode);

  xReadFlag( uiCode, "pps_mixed_nalu_types_in_pic_flag" );       pcPPS->setMixedNaluTypesInPicFlag( uiCode == 1 );

  xReadUvlc( uiCode, "pps_pic_width_in_luma_samples" );          pcPPS->setPicWidthInLumaSamples( uiCode );
  xReadUvlc( uiCode, "pps_pic_height_in_luma_samples" );         pcPPS->setPicHeightInLumaSamples( uiCode );
  xReadFlag( uiCode, "pps_conformance_window_flag");
  pcPPS->setConformanceWindowFlag( uiCode );
  if (uiCode != 0)
  {
    Window& conf = pcPPS->getConformanceWindow();
    xReadUvlc(uiCode, "pps_conf_win_left_offset");               conf.setWindowLeftOffset(uiCode);
    xReadUvlc(uiCode, "pps_conf_win_right_offset");              conf.setWindowRightOffset(uiCode);
    xReadUvlc(uiCode, "pps_conf_win_top_offset");                conf.setWindowTopOffset(uiCode);
    xReadUvlc(uiCode, "pps_conf_win_bottom_offset");             conf.setWindowBottomOffset(uiCode);
  }
  xReadFlag( uiCode, "pps_scaling_window_explicit_signalling_flag" );
  pcPPS->setExplicitScalingWindowFlag( uiCode );
  if( uiCode != 0 )
  {
    Window &scalingWindow = pcPPS->getScalingWindow();
    xReadSvlc( iCode, "pps_scaling_win_left_offset" );               scalingWindow.setWindowLeftOffset( iCode );
    xReadSvlc( iCode, "pps_scaling_win_right_offset" );              scalingWindow.setWindowRightOffset( iCode );
    xReadSvlc( iCode, "pps_scaling_win_top_offset" );                scalingWindow.setWindowTopOffset( iCode );
    xReadSvlc( iCode, "pps_scaling_win_bottom_offset" );             scalingWindow.setWindowBottomOffset( iCode );
  }
  else
  {
    pcPPS->getScalingWindow() = pcPPS->getConformanceWindow();
  }

  xReadFlag( uiCode, "pps_output_flag_present_flag" );                    pcPPS->setOutputFlagPresentFlag( uiCode==1 );

  xReadFlag( uiCode, "pps_no_pic_partition_flag");                     pcPPS->setNoPicPartitionFlag(uiCode == 1);
  xReadFlag( uiCode, "pps_subpic_id_mapping_present_flag" );           pcPPS->setSubPicIdMappingInPpsFlag( uiCode != 0 );
  if( pcPPS->getSubPicIdMappingInPpsFlag() )
  {
    if( !pcPPS->getNoPicPartitionFlag() )
    {
      xReadUvlc(uiCode, "pps_num_subpics_minus1");                         pcPPS->setNumSubPics(uiCode + 1);
    }
    else
    {
      pcPPS->setNumSubPics(1);
    }
    CHECK( uiCode > MAX_NUM_SUB_PICS-1,  "Number of sub-pictures exceeds limit");

    xReadUvlc( uiCode, "pps_subpic_id_len_minus1" );                       pcPPS->setSubPicIdLen( uiCode + 1 );
    CHECK( uiCode > 15, "Invalid pps_subpic_id_len_minus1 signalled");

    CHECK((1 << pcPPS->getSubPicIdLen()) < pcPPS->getNumSubPics(), "pps_subpic_id_len exceeds valid range");
    for( int picIdx = 0; picIdx < pcPPS->getNumSubPics( ); picIdx++ )
    {
      xReadCode( pcPPS->getSubPicIdLen( ), uiCode, "pps_subpic_id[i]" );   pcPPS->setSubPicId( picIdx, uiCode );
    }
  }
  if(!pcPPS->getNoPicPartitionFlag())
  {
    int colIdx, rowIdx;
    pcPPS->resetTileSliceInfo();

    // CTU size - required to match size in SPS
    xReadCode(2, uiCode, "pps_log2_ctu_size_minus5");                 pcPPS->setLog2CtuSize(uiCode + 5);
    CHECK(uiCode > 2, "pps_log2_ctu_size_minus5 must be less than or equal to 2");

    // number of explicit tile columns/rows
    xReadUvlc( uiCode, "pps_num_exp_tile_columns_minus1" );               pcPPS->setNumExpTileColumns( uiCode + 1 );
    xReadUvlc( uiCode, "pps_num_exp_tile_rows_minus1" );                  pcPPS->setNumExpTileRows( uiCode + 1 );
    CHECK(pcPPS->getNumExpTileColumns() > MAX_TILE_COLS,              "Number of explicit tile columns exceeds valid range");

    // tile sizes
    for( colIdx = 0; colIdx < pcPPS->getNumExpTileColumns(); colIdx++ )
    {
      xReadUvlc( uiCode, "pps_tile_column_width_minus1[i]" );             pcPPS->addTileColumnWidth( uiCode + 1 );
      CHECK(uiCode  > (pcPPS->getPicWidthInCtu()-1),                 "The value of pps_tile_column_width_minus1[i] shall be in the range of 0 to PicWidthInCtbY-1, inclusive");
    }
    for( rowIdx = 0; rowIdx < pcPPS->getNumExpTileRows(); rowIdx++ )
    {
      xReadUvlc( uiCode, "pps_tile_row_height_minus1[i]" );               pcPPS->addTileRowHeight( uiCode + 1 );
      CHECK(uiCode > (pcPPS->getPicHeightInCtu() - 1),                "The value of pps_tile_row_height_minus shall be in the range of 0 to PicHeightInCtbY-1, inclusive");
    }
    pcPPS->initTiles();
    // rectangular slice signalling
    if (pcPPS->getNumTiles() > 1)
    {
      xReadCode(1, uiCode, "pps_loop_filter_across_tiles_enabled_flag");    pcPPS->setLoopFilterAcrossTilesEnabledFlag(uiCode == 1);
      xReadCode(1, uiCode, "pps_rect_slice_flag");
    }
    else
    {
      pcPPS->setLoopFilterAcrossTilesEnabledFlag(false);
      uiCode = 1;
    }
    pcPPS->setRectSliceFlag(uiCode == 1);
    if (pcPPS->getRectSliceFlag())
    {
      xReadFlag(uiCode, "pps_single_slice_per_subpic_flag");            pcPPS->setSingleSlicePerSubPicFlag(uiCode == 1);
    }
    else
    {
      pcPPS->setSingleSlicePerSubPicFlag(0);
    }
    if (pcPPS->getRectSliceFlag() && !(pcPPS->getSingleSlicePerSubPicFlag()))
    {
      int32_t tileIdx = 0;

      xReadUvlc( uiCode, "pps_num_slices_in_pic_minus1" );                pcPPS->setNumSlicesInPic( uiCode + 1 );
      CHECK(pcPPS->getNumSlicesInPic() > MAX_SLICES,                  "Number of slices in picture exceeds valid range");
      if ((pcPPS->getNumSlicesInPic() - 1) > 1)
      {
        xReadCode(1, uiCode, "pps_tile_idx_delta_present_flag");
        pcPPS->setTileIdxDeltaPresentFlag(uiCode == 1);
      }
      else
      {
        pcPPS->setTileIdxDeltaPresentFlag(0);
      }
      pcPPS->initRectSlices();

      // read rectangular slice parameters
      for( int i = 0; i < pcPPS->getNumSlicesInPic()-1; i++ )
      {
        pcPPS->setSliceTileIdx( i, tileIdx );

        // complete tiles within a single slice
        if( ( tileIdx % pcPPS->getNumTileColumns() ) != pcPPS->getNumTileColumns() - 1 )
        {
          xReadUvlc( uiCode, "pps_slice_width_in_tiles_minus1[i]" );
          pcPPS->setSliceWidthInTiles ( i, uiCode + 1 );
        }
        else
        {
          pcPPS->setSliceWidthInTiles( i, 1 );
        }

        if( tileIdx / pcPPS->getNumTileColumns() != pcPPS->getNumTileRows() - 1  &&
         ( pcPPS->getTileIdxDeltaPresentFlag() || tileIdx % pcPPS->getNumTileColumns() == 0 ) )
        {
          xReadUvlc( uiCode, "pps_slice_height_in_tiles_minus1[i]" );
          pcPPS->setSliceHeightInTiles( i, uiCode + 1 );
        }
        else
        {
          if( ( tileIdx / pcPPS->getNumTileColumns() ) == pcPPS->getNumTileRows() - 1 )
          {
            pcPPS->setSliceHeightInTiles( i, 1 );
          }
          else
          {
            pcPPS->setSliceHeightInTiles( i, pcPPS->getSliceHeightInTiles( i - 1 ) );
          }
        }

        // multiple slices within a single tile special case
        if( pcPPS->getSliceWidthInTiles(i) == 1 && pcPPS->getSliceHeightInTiles(i) == 1 )
        {
          if( pcPPS->getTileRowHeight(tileIdx / pcPPS->getNumTileColumns()) > 1 )
          {
            xReadUvlc(uiCode, "pps_num_exp_slices_in_tile[i]");
            if (uiCode == 0)
            {
              pcPPS->setNumSlicesInTile(i, 1);
              pcPPS->setSliceHeightInCtu(i, pcPPS->getTileRowHeight(tileIdx / pcPPS->getNumTileColumns()));
            }
            else
            {
              uint32_t numExpSliceInTile = uiCode;
              uint32_t remTileRowHeight  = pcPPS->getTileRowHeight(tileIdx / pcPPS->getNumTileColumns());
              int j = 0;

              for( ; j < numExpSliceInTile; j++ )
              {
                xReadUvlc(uiCode, "pps_exp_slice_height_in_ctus_minus1[i]");
                pcPPS->setSliceHeightInCtu(i + j, uiCode + 1);
                remTileRowHeight -= (uiCode + 1);
              }
              uint32_t uniformSliceHeight = uiCode + 1;

              while( remTileRowHeight >= uniformSliceHeight )
              {
                pcPPS->setSliceHeightInCtu(i + j, uniformSliceHeight);
                remTileRowHeight -= uniformSliceHeight;
                j++;
              }
              if( remTileRowHeight > 0 )
              {
                pcPPS->setSliceHeightInCtu(i + j, remTileRowHeight);
                j++;
              }
              for( int k = 0; k < j; k++ )
              {
                pcPPS->setNumSlicesInTile(i + k, j);
                pcPPS->setSliceWidthInTiles(i + k, 1);
                pcPPS->setSliceHeightInTiles(i + k, 1);
                pcPPS->setSliceTileIdx(i + k, tileIdx);
              }
              i += (j - 1);
            }
          }
          else
          {
            pcPPS->setNumSlicesInTile(i, 1);
            pcPPS->setSliceHeightInCtu(i, pcPPS->getTileRowHeight(tileIdx / pcPPS->getNumTileColumns()));
          }
        }

        // tile index offset to start of next slice
        if( i < pcPPS->getNumSlicesInPic()-1 )
        {
          if( pcPPS->getTileIdxDeltaPresentFlag() )
          {
            int32_t  tileIdxDelta;
            xReadSvlc( tileIdxDelta, "pps_tile_idx_delta[i]" );
            tileIdx += tileIdxDelta;
            CHECK( tileIdx < 0 || tileIdx >= pcPPS->getNumTiles(), "Invalid pps_tile_idx_delta.");
          }
          else
          {
            tileIdx += pcPPS->getSliceWidthInTiles( i );
            if( tileIdx % pcPPS->getNumTileColumns() == 0)
            {
              tileIdx += (pcPPS->getSliceHeightInTiles( i ) - 1) * pcPPS->getNumTileColumns();
            }
          }
        }
      }
      pcPPS->setSliceTileIdx(pcPPS->getNumSlicesInPic()-1, tileIdx );
    }

    if (pcPPS->getRectSliceFlag() == 0 || pcPPS->getSingleSlicePerSubPicFlag() || pcPPS->getNumSlicesInPic() > 1)
    {
      xReadCode(1, uiCode, "pps_loop_filter_across_slices_enabled_flag");   pcPPS->setLoopFilterAcrossSlicesEnabledFlag( uiCode == 1 );
    }
    else
    {
      pcPPS->setLoopFilterAcrossSlicesEnabledFlag( false );
    }
  }
  else
  {
    pcPPS->setSingleSlicePerSubPicFlag(1);
  }

  xReadFlag( uiCode,   "pps_cabac_init_present_flag" );            pcPPS->setCabacInitPresentFlag( uiCode ? true : false );

  xReadUvlc(uiCode, "pps_num_ref_idx_default_active_minus1[0]");
  CHECK(uiCode >= MAX_NUM_ACTIVE_REF,
        "The value of pps_num_ref_idx_default_active_minus1[0] shall be in the range of 0 to 14, inclusive");
  pcPPS->setNumRefIdxDefaultActive(REF_PIC_LIST_0, uiCode + 1);

  xReadUvlc(uiCode, "pps_num_ref_idx_default_active_minus1[1]");
  CHECK(uiCode >= MAX_NUM_ACTIVE_REF,
        "The value of pps_num_ref_idx_default_active_minus1[1] shall be in the range of 0 to 14, inclusive");
  pcPPS->setNumRefIdxDefaultActive(REF_PIC_LIST_1, uiCode + 1);

  xReadFlag(uiCode, "pps_rpl1_idx_present_flag");
  pcPPS->setRpl1IdxPresentFlag(uiCode);
  xReadFlag( uiCode, "pps_weighted_pred_flag" );          // Use of Weighting Prediction (P_SLICE)
  pcPPS->setUseWP( uiCode==1 );
  xReadFlag( uiCode, "pps_weighted_bipred_flag" );         // Use of Bi-Directional Weighting Prediction (B_SLICE)
  pcPPS->setWPBiPred( uiCode==1 );
  xReadFlag(uiCode, "pps_ref_wraparound_enabled_flag");           pcPPS->setWrapAroundEnabledFlag( uiCode ? true : false );
  if (pcPPS->getWrapAroundEnabledFlag())
  {
    xReadUvlc(uiCode, "pps_ref_wraparound_offset");
    pcPPS->setPicWidthMinusWrapAroundOffset(uiCode);
  }
  else
  {
    pcPPS->setPicWidthMinusWrapAroundOffset(0);
  }

  xReadSvlc(iCode, "pps_init_qp_minus26" );                            pcPPS->setPicInitQPMinus26(iCode);
  xReadFlag( uiCode, "pps_cu_qp_delta_enabled_flag" );            pcPPS->setUseDQP( uiCode ? true : false );
  xReadFlag(uiCode, "pps_chroma_tool_offsets_present_flag");
  pcPPS->setPPSChromaToolFlag(uiCode ? true : false);
  if (pcPPS->getPPSChromaToolFlag())
  {
    xReadSvlc(iCode, "pps_cb_qp_offset");
    pcPPS->setQpOffset(COMPONENT_Cb, iCode);
    CHECK(pcPPS->getQpOffset(COMPONENT_Cb) < -12, "Invalid Cb QP offset");
    CHECK(pcPPS->getQpOffset(COMPONENT_Cb) > 12, "Invalid Cb QP offset");

    xReadSvlc(iCode, "pps_cr_qp_offset");
    pcPPS->setQpOffset(COMPONENT_Cr, iCode);
    CHECK(pcPPS->getQpOffset(COMPONENT_Cr) < -12, "Invalid Cr QP offset");
    CHECK(pcPPS->getQpOffset(COMPONENT_Cr) > 12, "Invalid Cr QP offset");

    xReadFlag(uiCode, "pps_joint_cbcr_qp_offset_present_flag");
    pcPPS->setJointCbCrQpOffsetPresentFlag(uiCode ? true : false);

    if (pcPPS->getJointCbCrQpOffsetPresentFlag())
    {
      xReadSvlc(iCode, "pps_joint_cbcr_qp_offset_value");
    }
    else
    {
      iCode = 0;
    }
    pcPPS->setQpOffset(JOINT_CbCr, iCode);

    CHECK(pcPPS->getQpOffset(JOINT_CbCr) < -12, "Invalid CbCr QP offset");
    CHECK(pcPPS->getQpOffset(JOINT_CbCr) > 12, "Invalid CbCr QP offset");

    CHECK(MAX_NUM_COMPONENT > 3, "Invalid maximal number of components");

    xReadFlag(uiCode, "pps_slice_chroma_qp_offsets_present_flag");
    pcPPS->setSliceChromaQpFlag(uiCode ? true : false);

    xReadFlag(uiCode, "pps_cu_chroma_qp_offset_list_enabled_flag");
    if (uiCode == 0)
    {
      pcPPS->clearChromaQpOffsetList();
    }
    else
    {
      uint32_t tableSizeMinus1 = 0;
      xReadUvlc(tableSizeMinus1, "pps_chroma_qp_offset_list_len_minus1");
      CHECK(tableSizeMinus1 >= MAX_QP_OFFSET_LIST_SIZE, "Table size exceeds maximum");

      for (int cuChromaQpOffsetIdx = 0; cuChromaQpOffsetIdx <= (tableSizeMinus1); cuChromaQpOffsetIdx++)
      {
        int cbOffset;
        int crOffset;
        int jointCbCrOffset;
        xReadSvlc(cbOffset, "pps_cb_qp_offset_list[i]");
        CHECK(cbOffset < -12 || cbOffset > 12, "Invalid chroma QP offset");
        xReadSvlc(crOffset, "pps_cr_qp_offset_list[i]");
        CHECK(crOffset < -12 || crOffset > 12, "Invalid chroma QP offset");
        if (pcPPS->getJointCbCrQpOffsetPresentFlag())
        {
          xReadSvlc(jointCbCrOffset, "pps_joint_cbcr_qp_offset_list[i]");
        }
        else
        {
          jointCbCrOffset = 0;
        }
        CHECK(jointCbCrOffset < -12 || jointCbCrOffset > 12, "Invalid chroma QP offset");
        // table uses +1 for index (see comment inside the function)
        pcPPS->setChromaQpOffsetListEntry(cuChromaQpOffsetIdx + 1, cbOffset, crOffset, jointCbCrOffset);
      }
      CHECK(pcPPS->getChromaQpOffsetListLen() != tableSizeMinus1 + 1, "Invalid chroma QP offset list length");
    }
  }
  else
  {
    pcPPS->setQpOffset(COMPONENT_Cb, 0);
    pcPPS->setQpOffset(COMPONENT_Cr, 0);
    pcPPS->setJointCbCrQpOffsetPresentFlag(0);
    pcPPS->setSliceChromaQpFlag(0);
    pcPPS->clearChromaQpOffsetList();
  }
  xReadFlag( uiCode, "pps_deblocking_filter_control_present_flag" );       pcPPS->setDeblockingFilterControlPresentFlag( uiCode ? true : false );
  if(pcPPS->getDeblockingFilterControlPresentFlag())
  {
    xReadFlag( uiCode, "pps_deblocking_filter_override_enabled_flag" );    pcPPS->setDeblockingFilterOverrideEnabledFlag( uiCode ? true : false );
    xReadFlag( uiCode, "pps_deblocking_filter_disabled_flag" );        pcPPS->setPPSDeblockingFilterDisabledFlag(uiCode ? true : false );
    if (!pcPPS->getNoPicPartitionFlag() && pcPPS->getDeblockingFilterOverrideEnabledFlag())
    {
      xReadFlag(uiCode, "pps_dbf_info_in_ph_flag");
      pcPPS->setDbfInfoInPhFlag(uiCode ? true : false);
    }
    else
    {
      pcPPS->setDbfInfoInPhFlag(false);
    }
    if(!pcPPS->getPPSDeblockingFilterDisabledFlag())
    {
      xReadSvlc( iCode, "pps_beta_offset_div2" );                    pcPPS->setDeblockingFilterBetaOffsetDiv2( iCode );
      CHECK(  pcPPS->getDeblockingFilterBetaOffsetDiv2() < -12 ||
              pcPPS->getDeblockingFilterBetaOffsetDiv2() > 12, "Invalid deblocking filter configuration" );

      xReadSvlc( iCode, "pps_tc_offset_div2");                       pcPPS->setDeblockingFilterTcOffsetDiv2( iCode );
      CHECK(  pcPPS->getDeblockingFilterTcOffsetDiv2() < -12 ||
              pcPPS->getDeblockingFilterTcOffsetDiv2() > 12, "Invalid deblocking filter configuration" );

      if( pcPPS->getPPSChromaToolFlag() )
      {
        xReadSvlc( iCode, "pps_cb_beta_offset_div2" );                   pcPPS->setDeblockingFilterCbBetaOffsetDiv2( iCode );
        CHECK( pcPPS->getDeblockingFilterCbBetaOffsetDiv2() < -12 ||
          pcPPS->getDeblockingFilterCbBetaOffsetDiv2() > 12, "Invalid deblocking filter configuration" );

        xReadSvlc( iCode, "pps_cb_tc_offset_div2" );                     pcPPS->setDeblockingFilterCbTcOffsetDiv2( iCode );
        CHECK( pcPPS->getDeblockingFilterCbTcOffsetDiv2() < -12 ||
          pcPPS->getDeblockingFilterCbTcOffsetDiv2() > 12, "Invalid deblocking filter configuration" );

        xReadSvlc( iCode, "pps_cr_beta_offset_div2") ;                   pcPPS->setDeblockingFilterCrBetaOffsetDiv2( iCode );
        CHECK( pcPPS->getDeblockingFilterCrBetaOffsetDiv2() < -12 ||
          pcPPS->getDeblockingFilterCrBetaOffsetDiv2() > 12, "Invalid deblocking filter configuration" );

        xReadSvlc( iCode, "pps_cr_tc_offset_div2" );                     pcPPS->setDeblockingFilterCrTcOffsetDiv2( iCode );
        CHECK(pcPPS->getDeblockingFilterCrTcOffsetDiv2() < -12 ||
          pcPPS->getDeblockingFilterCrTcOffsetDiv2() > 12, "Invalid deblocking filter configuration");
      }
      else
      {
        pcPPS->setDeblockingFilterCbBetaOffsetDiv2 ( pcPPS->getDeblockingFilterBetaOffsetDiv2() );
        pcPPS->setDeblockingFilterCbTcOffsetDiv2   ( pcPPS->getDeblockingFilterTcOffsetDiv2()   );
        pcPPS->setDeblockingFilterCrBetaOffsetDiv2 ( pcPPS->getDeblockingFilterBetaOffsetDiv2() );
        pcPPS->setDeblockingFilterCrTcOffsetDiv2   ( pcPPS->getDeblockingFilterTcOffsetDiv2()   );
      }
    }
  }
  else
  {
    pcPPS->setDeblockingFilterOverrideEnabledFlag(false);
    pcPPS->setDbfInfoInPhFlag(false);
  }

  if (!pcPPS->getNoPicPartitionFlag())
  {
    xReadFlag(uiCode, "pps_rpl_info_in_ph_flag");                    pcPPS->setRplInfoInPhFlag(uiCode ? true : false);
    xReadFlag(uiCode, "pps_sao_info_in_ph_flag");                    pcPPS->setSaoInfoInPhFlag(uiCode ? true : false);
    xReadFlag(uiCode, "pps_alf_info_in_ph_flag");                    pcPPS->setAlfInfoInPhFlag(uiCode ? true : false);
    if ((pcPPS->getUseWP() || pcPPS->getWPBiPred()) && pcPPS->getRplInfoInPhFlag())
    {
      xReadFlag(uiCode, "pps_wp_info_in_ph_flag");                   pcPPS->setWpInfoInPhFlag(uiCode ? true : false);
    }
    else
    {
      pcPPS->setWpInfoInPhFlag(false);
    }
    xReadFlag(uiCode, "pps_qp_delta_info_in_ph_flag");               pcPPS->setQpDeltaInfoInPhFlag(uiCode ? true : false);
  }
  else
  {
    pcPPS->setRplInfoInPhFlag(false);
    pcPPS->setSaoInfoInPhFlag(false);
    pcPPS->setAlfInfoInPhFlag(false);
    pcPPS->setWpInfoInPhFlag(false);
    pcPPS->setQpDeltaInfoInPhFlag(false);
  }

  xReadFlag( uiCode, "pps_picture_header_extension_present_flag");
  pcPPS->setPictureHeaderExtensionPresentFlag(uiCode);
  xReadFlag( uiCode, "pps_slice_header_extension_present_flag");
  pcPPS->setSliceHeaderExtensionPresentFlag(uiCode);

  xReadFlag( uiCode, "pps_extension_flag");

  if (uiCode)
  {
    while (xMoreRbspData())
    {
      xReadFlag(uiCode, "pps_extension_data_flag");
    }
  }
  xReadRbspTrailingBits();
}

void HLSyntaxReader::parseAPS( APS* aps )
{
#if ENABLE_TRACING
  xTraceAPSHeader();
#endif

  uint32_t  code;
  xReadCode(3, code, "aps_params_type");
  aps->setAPSType(ApsType(code));

  xReadCode(5, code, "adaptation_parameter_set_id");
  aps->setAPSId(code);
  uint32_t codeApsChromaPresentFlag;
  xReadFlag(codeApsChromaPresentFlag, "aps_chroma_present_flag");
  aps->chromaPresentFlag = codeApsChromaPresentFlag;

  const ApsType apsType = aps->getAPSType();

  if (apsType == ApsType::ALF)
  {
    parseAlfAps( aps );
  }
  else if (apsType == ApsType::LMCS)
  {
    parseLmcsAps( aps );
  }
  else if (apsType == ApsType::SCALING_LIST)
  {
    parseScalingListAps( aps );
  }
  xReadFlag(code, "aps_extension_flag");
  if (code)
  {
    while (xMoreRbspData())
    {
      xReadFlag(code, "aps_extension_data_flag");
    }
  }
  xReadRbspTrailingBits();
}

void HLSyntaxReader::parseAlfAps( APS* aps )
{
  uint32_t  code;

  AlfParam param = aps->getAlfAPSParam();
  param.reset();
  param.enabledFlag[COMPONENT_Y] = param.enabledFlag[COMPONENT_Cb] = param.enabledFlag[COMPONENT_Cr] = true;
  xReadFlag(code, "alf_luma_new_filter");
  param.newFilterFlag[ChannelType::LUMA] = code;

  if (aps->chromaPresentFlag)
  {
    xReadFlag(code, "alf_chroma_new_filter");
    param.newFilterFlag[ChannelType::CHROMA] = code;
  }
  else
  {
    param.newFilterFlag[ChannelType::CHROMA] = 0;
  }

  CcAlfFilterParam ccAlfParam = aps->getCcAlfAPSParam();
  if (aps->chromaPresentFlag)
  {
    xReadFlag(code, "alf_cc_cb_filter_signal_flag");
    ccAlfParam.newCcAlfFilter[COMPONENT_Cb - 1] = code;
  }
  else
  {
    ccAlfParam.newCcAlfFilter[COMPONENT_Cb - 1] = 0;
  }
  if (aps->chromaPresentFlag)
  {
    xReadFlag(code, "alf_cc_cr_filter_signal_flag");
    ccAlfParam.newCcAlfFilter[COMPONENT_Cr - 1] = code;
  }
  else
  {
    ccAlfParam.newCcAlfFilter[COMPONENT_Cr - 1] = 0;
  }
  CHECK(param.newFilterFlag[ChannelType::LUMA] == 0 && param.newFilterFlag[ChannelType::CHROMA] == 0
          && ccAlfParam.newCcAlfFilter[COMPONENT_Cb - 1] == 0 && ccAlfParam.newCcAlfFilter[COMPONENT_Cr - 1] == 0,
        "bitstream conformance error: one of alf_luma_filter_signal_flag, alf_chroma_filter_signal_flag, "
        "alf_cross_component_cb_filter_signal_flag, and alf_cross_component_cr_filter_signal_flag shall be nonzero");

  if (param.newFilterFlag[ChannelType::LUMA])
  {
    xReadFlag(code, "alf_luma_clip");
    param.nonLinearFlag[ChannelType::LUMA] = code ? true : false;
    xReadUvlc(code, "alf_luma_num_filters_signalled_minus1");
    param.numLumaFilters = code + 1;
    if (param.numLumaFilters > 1)
    {
      const int length =  ceilLog2(param.numLumaFilters);
      for (int i = 0; i < MAX_NUM_ALF_CLASSES; i++)
      {
        xReadCode(length, code, "alf_luma_coeff_delta_idx");
        param.filterCoeffDeltaIdx[i] = code;
      }
    }
    else
    {
      memset(param.filterCoeffDeltaIdx, 0, sizeof(param.filterCoeffDeltaIdx));
    }
    alfFilter( param, false, 0 );
  }
  if (param.newFilterFlag[ChannelType::CHROMA])
  {
    xReadFlag(code, "alf_nonlinear_enable_flag_chroma");
    param.nonLinearFlag[ChannelType::CHROMA] = code ? true : false;

    if constexpr (ALF_MAX_NUM_ALTERNATIVES_CHROMA > 1)
    {
      xReadUvlc( code, "alf_chroma_num_alts_minus1" );
    }
    else
    {
      code = 0;
    }

    param.numAlternativesChroma = code + 1;

    for( int altIdx=0; altIdx < param.numAlternativesChroma; ++altIdx )
    {
      alfFilter( param, true, altIdx );
    }
  }

  for (int ccIdx = 0; ccIdx < 2; ccIdx++)
  {
    if (ccAlfParam.newCcAlfFilter[ccIdx])
    {
      if (MAX_NUM_CC_ALF_FILTERS > 1)
      {
        xReadUvlc(code, ccIdx == 0 ? "alf_cc_cb_filters_signalled_minus1" : "alf_cc_cr_filters_signalled_minus1");
      }
      else
      {
        code = 0;
      }
      ccAlfParam.ccAlfFilterCount[ccIdx] = code + 1;

      for (int filterIdx = 0; filterIdx < ccAlfParam.ccAlfFilterCount[ccIdx]; filterIdx++)
      {
        ccAlfParam.ccAlfFilterIdxEnabled[ccIdx][filterIdx] = true;
        AlfFilterShape alfShape(size_CC_ALF);

        AlfCoeff* coeff = ccAlfParam.ccAlfCoeff[ccIdx][filterIdx];
        // Filter coefficients
        for (int i = 0; i < alfShape.numCoeff - 1; i++)
        {
          xReadCode(CCALF_BITS_PER_COEFF_LEVEL, code,
                    ccIdx == 0 ? "alf_cc_cb_mapped_coeff_abs" : "alf_cc_cr_mapped_coeff_abs");
          if (code == 0)
          {
            coeff[i] = 0;
          }
          else
          {
            coeff[i] = 1 << (code - 1);
            xReadFlag(code, ccIdx == 0 ? "alf_cc_cb_coeff_sign" : "alf_cc_cr_coeff_sign");
            coeff[i] *= 1 - 2 * code;
          }
        }

        DTRACE(g_trace_ctx, D_SYNTAX, "%s coeff filterIdx %d: ", ccIdx == 0 ? "Cb" : "Cr", filterIdx);
        for (int i = 0; i < alfShape.numCoeff; i++)
        {
          DTRACE(g_trace_ctx, D_SYNTAX, "%d ", coeff[i]);
        }
        DTRACE(g_trace_ctx, D_SYNTAX, "\n");
      }

      for (int filterIdx = ccAlfParam.ccAlfFilterCount[ccIdx]; filterIdx < MAX_NUM_CC_ALF_FILTERS; filterIdx++)
      {
        ccAlfParam.ccAlfFilterIdxEnabled[ccIdx][filterIdx] = false;
      }
    }
  }
  aps->setCcAlfAPSParam(ccAlfParam);

  aps->setAlfAPSParam(param);
}

void HLSyntaxReader::parseLmcsAps( APS* aps )
{
  uint32_t  code;

  SliceReshapeInfo& info = aps->getReshaperAPSInfo();
  memset(info.reshaperModelBinCWDelta, 0, PIC_CODE_CW_BINS * sizeof(int));
  xReadUvlc(code, "lmcs_min_bin_idx");                             info.reshaperModelMinBinIdx = code;
  xReadUvlc(code, "lmcs_delta_max_bin_idx");                       info.reshaperModelMaxBinIdx = PIC_CODE_CW_BINS - 1 - code;
  xReadUvlc(code, "lmcs_delta_cw_prec_minus1");                    info.maxNbitsNeededDeltaCW = code + 1;

  for (uint32_t i = info.reshaperModelMinBinIdx; i <= info.reshaperModelMaxBinIdx; i++)
  {
    xReadCode(info.maxNbitsNeededDeltaCW, code, "lmcs_delta_abs_cw[ i ]");
    int absCW = code;
    if (absCW > 0)
    {
      xReadCode(1, code, "lmcs_delta_sign_cw_flag[ i ]");
    }
    int signCW = code;
    info.reshaperModelBinCWDelta[i] = (1 - 2 * signCW) * absCW;
  }
  if (aps->chromaPresentFlag)
  {
    xReadCode(3, code, "lmcs_delta_abs_crs");
  }
  int absCW = aps->chromaPresentFlag ? code : 0;
  if (absCW > 0)
  {
    xReadCode(1, code, "lmcs_delta_sign_crs_flag");
  }
  int signCW = code;
  info.chrResScalingOffset = (1 - 2 * signCW) * absCW;

  aps->setReshaperAPSInfo(info);
}

void HLSyntaxReader::parseScalingListAps( APS* aps )
{
  ScalingList& info = aps->getScalingList();
  parseScalingList(&info, aps->chromaPresentFlag);
}

void  HLSyntaxReader::parseVUI(VUI* pcVUI, SPS *pcSPS)
{
#if ENABLE_TRACING
  DTRACE( g_trace_ctx, D_HEADER, "----------- vui_parameters -----------\n");
#endif
  const uint32_t  vuiPayloadSize = pcSPS->getVuiPayloadSize();
  InputBitstream* spsBitstream   = getBitstream();
  InputBitstream* vuiBitstream   = spsBitstream->extractSubstream(vuiPayloadSize * 8);
  setBitstream(vuiBitstream);

  uint32_t  symbol;

  xReadFlag(symbol,  "vui_progressive_source_flag"          ); pcVUI->setProgressiveSourceFlag(symbol ? true : false);
  xReadFlag(symbol,  "vui_interlaced_source_flag"           ); pcVUI->setInterlacedSourceFlag(symbol ? true : false);
  xReadFlag(symbol, "vui_non_packed_constraint_flag");         pcVUI->setNonPackedFlag(symbol ? true : false);
  xReadFlag(symbol, "vui_non_projected_constraint_flag");      pcVUI->setNonProjectedFlag(symbol ? true : false);
  xReadFlag( symbol, "vui_aspect_ratio_info_present_flag");           pcVUI->setAspectRatioInfoPresentFlag(symbol);
  if (pcVUI->getAspectRatioInfoPresentFlag())
  {
    xReadFlag( symbol, "vui_aspect_ratio_constant_flag");           pcVUI->setAspectRatioConstantFlag(symbol);
    xReadCode(8, symbol, "vui_aspect_ratio_idc");                         pcVUI->setAspectRatioIdc(symbol);
    if (pcVUI->getAspectRatioIdc() == 255)
    {
      xReadCode(16, symbol, "vui_sar_width");                             pcVUI->setSarWidth(symbol);
      xReadCode(16, symbol, "vui_sar_height");                            pcVUI->setSarHeight(symbol);
    }
  }

  xReadFlag(     symbol, "vui_overscan_info_present_flag");               pcVUI->setOverscanInfoPresentFlag(symbol);
  if (pcVUI->getOverscanInfoPresentFlag())
  {
    xReadFlag(   symbol, "vui_overscan_appropriate_flag");                pcVUI->setOverscanAppropriateFlag(symbol);
  }

  xReadFlag(   symbol, "vui_colour_description_present_flag");          pcVUI->setColourDescriptionPresentFlag(symbol);
  if (pcVUI->getColourDescriptionPresentFlag())
  {
    xReadCode(8, symbol, "vui_colour_primaries");                       pcVUI->setColourPrimaries(symbol);
    xReadCode(8, symbol, "vui_transfer_characteristics");               pcVUI->setTransferCharacteristics(symbol);
    xReadCode(8, symbol, "vui_matrix_coeffs");                          pcVUI->setMatrixCoefficients(symbol);
#if JVET_AL0301_MATRIXCOEFFS_CONSTRAINTS
    CHECK((symbol == 0 || symbol == 16 || symbol == 17) && pcSPS->getChromaFormatIdc() == ChromaFormat::_444,
      "vui_matrix_coeffs shall not be equal to 0, 16, or 17 when sps_chroma_format_idc is equal to 3");
#endif
    xReadFlag(   symbol, "vui_full_range_flag");                    pcVUI->setVideoFullRangeFlag(symbol);
  }

  xReadFlag(     symbol, "vui_chroma_loc_info_present_flag");             pcVUI->setChromaLocInfoPresentFlag(symbol);
  if (pcVUI->getChromaLocInfoPresentFlag())
  {
    if(pcVUI->getProgressiveSourceFlag() && !pcVUI->getInterlacedSourceFlag())
    {
      xReadUvlc(symbol, "vui_chroma_sample_loc_type");
      CHECK(symbol >= to_underlying(Chroma420LocType::NUM), "vui_chroma_sample_loc_type out of range");
      pcVUI->setChromaSampleLocType(static_cast<Chroma420LocType>(symbol));
    }
    else
    {
      xReadUvlc(symbol, "vui_chroma_sample_loc_type_top_field");
      CHECK(symbol >= to_underlying(Chroma420LocType::NUM), "vui_chroma_sample_loc_type_top_field out of range");
      pcVUI->setChromaSampleLocTypeTopField(static_cast<Chroma420LocType>(symbol));

      xReadUvlc(symbol, "vui_chroma_sample_loc_type_bottom_field");
      CHECK(symbol >= to_underlying(Chroma420LocType::NUM), "vui_chroma_sample_loc_type_bottom_field out of range");
      pcVUI->setChromaSampleLocTypeBottomField(static_cast<Chroma420LocType>(symbol));
    }
  }

  int payloadBitsRem = getBitstream()->getNumBitsLeft();
  if(payloadBitsRem)      //Corresponds to more_data_in_payload()
  {
    while(payloadBitsRem > 9)    //payload_extension_present()
    {
      xReadCode(1, symbol, "vui_reserved_payload_extension_data");
      payloadBitsRem--;
    }
    int finalBits = getBitstream()->peekBits(payloadBitsRem);
    int numFinalZeroBits = 0;
    int mask = 0xff;
    while(finalBits & (mask >> numFinalZeroBits))
    {
      numFinalZeroBits++;
    }
    while(payloadBitsRem > 9-numFinalZeroBits)     //payload_extension_present()
    {
      xReadCode(1, symbol, "vui_reserved_payload_extension_data");
      payloadBitsRem--;
    }
    xReadFlag(symbol, "vui_payload_bit_equal_to_one");
    CHECK(symbol != 1, "vui_payload_bit_equal_to_one not equal to 1");
    payloadBitsRem--;
    while(payloadBitsRem)
    {
      xReadFlag(symbol, "vui_payload_bit_equal_to_zero");
      CHECK(symbol != 0, "vui_payload_bit_equal_to_zero not equal to 0");
      payloadBitsRem--;
    }
  }
  setBitstream(spsBitstream);
  delete vuiBitstream;
}

void HLSyntaxReader::parseGeneralHrdParameters(GeneralHrdParams *hrd)
{
  uint32_t  symbol;
  xReadCode(32, symbol, "num_units_in_tick");                hrd->setNumUnitsInTick(symbol);
  xReadCode(32, symbol, "time_scale");                       hrd->setTimeScale(symbol);
  xReadFlag(symbol, "general_nal_hrd_parameters_present_flag");           hrd->setGeneralNalHrdParametersPresentFlag(symbol == 1 ? true : false);
  xReadFlag(symbol, "general_vcl_hrd_parameters_present_flag");           hrd->setGeneralVclHrdParametersPresentFlag(symbol == 1 ? true : false);
  if(  hrd->getGeneralNalHrdParametersPresentFlag() || hrd->getGeneralVclHrdParametersPresentFlag() )
  {
    xReadFlag(symbol, "general_same_pic_timing_in_all_ols_flag");           hrd->setGeneralSamePicTimingInAllOlsFlag(symbol == 1 ? true : false);
    xReadFlag(symbol, "general_decoding_unit_hrd_params_present_flag");     hrd->setGeneralDecodingUnitHrdParamsPresentFlag(symbol == 1 ? true : false);
    if (hrd->getGeneralDecodingUnitHrdParamsPresentFlag())
    {
      xReadCode(8, symbol, "tick_divisor_minus2");                        hrd->setTickDivisorMinus2(symbol);
    }
    xReadCode(4, symbol, "bit_rate_scale");                       hrd->setBitRateScale(symbol);
    xReadCode(4, symbol, "cpb_size_scale");                       hrd->setCpbSizeScale(symbol);
    if (hrd->getGeneralDecodingUnitHrdParamsPresentFlag())
    {
      xReadCode(4, symbol, "cpb_size_du_scale");                  hrd->setCpbSizeDuScale(symbol);
    }
    xReadUvlc(symbol, "hrd_cpb_cnt_minus1");                      hrd->setHrdCpbCntMinus1(symbol);
    CHECK(symbol > 31,"The value of hrd_cpb_cnt_minus1 shall be in the range of 0 to 31, inclusive");
  }
}

void HLSyntaxReader::parseOlsHrdParameters(GeneralHrdParams * generalHrd, OlsHrdParams *olsHrd, uint32_t firstSubLayer, uint32_t maxNumSubLayersMinus1)
{
  uint32_t  symbol;

  for( int i = firstSubLayer; i <= maxNumSubLayersMinus1; i ++ )
  {
    OlsHrdParams *hrd = &(olsHrd[i]);
    xReadFlag(symbol, "fixed_pic_rate_general_flag");                     hrd->setFixedPicRateGeneralFlag(symbol == 1 ? true : false);
    if (!hrd->getFixedPicRateGeneralFlag())
    {
      xReadFlag(symbol, "fixed_pic_rate_within_cvs_flag");                hrd->setFixedPicRateWithinCvsFlag(symbol == 1 ? true : false);
    }
    else
    {
      hrd->setFixedPicRateWithinCvsFlag(true);
    }

    hrd->setLowDelayHrdFlag(false); // Inferred to be 0 when not present

    if (hrd->getFixedPicRateWithinCvsFlag())
    {
      xReadUvlc(symbol, "elemental_duration_in_tc_minus1");
      CHECK(symbol > 2047, "elemental_duration_in_tc_minus1 is too large (> 2047)");

      hrd->setElementDurationInTc(symbol + 1);
    }
    else if((generalHrd->getGeneralNalHrdParametersPresentFlag() || generalHrd->getGeneralVclHrdParametersPresentFlag()) && generalHrd->getHrdCpbCntMinus1() == 0)
    {
      xReadFlag(symbol, "low_delay_hrd_flag");                      hrd->setLowDelayHrdFlag(symbol == 1 ? true : false);
    }

    for( int nalOrVcl = 0; nalOrVcl < 2; nalOrVcl ++ )
    {
      if (((nalOrVcl == 0) && (generalHrd->getGeneralNalHrdParametersPresentFlag())) || ((nalOrVcl == 1) && (generalHrd->getGeneralVclHrdParametersPresentFlag())))
      {
        for (int j = 0; j <= (generalHrd->getHrdCpbCntMinus1()); j++)
        {
          xReadUvlc(symbol, "bit_rate_value_minus1");             hrd->setBitRateValueMinus1(j, nalOrVcl, symbol);
          xReadUvlc(symbol, "cpb_size_value_minus1");             hrd->setCpbSizeValueMinus1(j, nalOrVcl, symbol);
          if (generalHrd->getGeneralDecodingUnitHrdParamsPresentFlag())
          {
            xReadUvlc(symbol, "cpb_size_du_value_minus1");             hrd->setDuCpbSizeValueMinus1(j, nalOrVcl, symbol);
            xReadUvlc(symbol, "bit_rate_du_value_minus1");             hrd->setDuBitRateValueMinus1(j, nalOrVcl, symbol);
          }
          xReadFlag(symbol, "cbr_flag");                          hrd->setCbrFlag(j, nalOrVcl, symbol == 1 ? true : false);
        }
      }
    }
  }
  for (int i = 0; i < firstSubLayer; i++)
  {
    OlsHrdParams* hrdHighestTLayer = &(olsHrd[maxNumSubLayersMinus1]);
    OlsHrdParams* hrdTemp = &(olsHrd[i]);
    bool tempFlag = hrdHighestTLayer->getFixedPicRateGeneralFlag();
    hrdTemp->setFixedPicRateGeneralFlag(tempFlag);
    tempFlag = hrdHighestTLayer->getFixedPicRateWithinCvsFlag();
    hrdTemp->setFixedPicRateWithinCvsFlag(tempFlag);
    uint32_t tempElementDurationInTc = hrdHighestTLayer->getElementDurationInTc();
    hrdTemp->setElementDurationInTc(tempElementDurationInTc);
    for (int nalOrVcl = 0; nalOrVcl < 2; nalOrVcl++)
    {
      if (((nalOrVcl == 0) && (generalHrd->getGeneralNalHrdParametersPresentFlag())) || ((nalOrVcl == 1) && (generalHrd->getGeneralVclHrdParametersPresentFlag())))
      {
        for (int j = 0; j <= (generalHrd->getHrdCpbCntMinus1()); j++)
        {
          uint32_t bitRate = hrdHighestTLayer->getBitRateValueMinus1(j, nalOrVcl);
          hrdTemp->setBitRateValueMinus1(j, nalOrVcl, bitRate);
          uint32_t cpbSize = hrdHighestTLayer->getCpbSizeValueMinus1(j, nalOrVcl);
          hrdTemp->setCpbSizeValueMinus1(j, nalOrVcl, cpbSize);
          if (generalHrd->getGeneralDecodingUnitHrdParamsPresentFlag())
          {
            uint32_t bitRateDu = hrdHighestTLayer->getDuBitRateValueMinus1(j, nalOrVcl);
            hrdTemp->setDuBitRateValueMinus1(j, nalOrVcl, bitRateDu);
            uint32_t cpbSizeDu = hrdHighestTLayer->getDuCpbSizeValueMinus1(j, nalOrVcl);
            hrdTemp->setDuCpbSizeValueMinus1(j, nalOrVcl, cpbSizeDu);
          }
          bool flag = hrdHighestTLayer->getCbrFlag(j, nalOrVcl);
          hrdTemp->setCbrFlag(j, nalOrVcl, flag);
        }
      }
    }
  }
}

void HLSyntaxReader::dpb_parameters(int maxSubLayersMinus1, bool subLayerInfoFlag, SPS *pcSPS)
{
  uint32_t code;
  for (int i = (subLayerInfoFlag ? 0 : maxSubLayersMinus1); i <= maxSubLayersMinus1; i++)
  {
    xReadUvlc(code, "dpb_max_dec_pic_buffering_minus1[i]");
    pcSPS->setMaxDecPicBuffering(code + 1, i);
    xReadUvlc(code, "dpb_max_num_reorder_pics[i]");
    pcSPS->setMaxNumReorderPics(code, i);
    CHECK( pcSPS->getMaxNumReorderPics(i) >= pcSPS->getMaxDecPicBuffering(i), "The value of dpb_max_num_reorder_pics[ i ] shall be in the range of 0 to dpb_max_dec_pic_buffering_minus1[ i ], inclusive" );
    xReadUvlc(code, "dpb_max_latency_increase_plus1[i]");
    pcSPS->setMaxLatencyIncreasePlus1(code, i);
  }

  if (!subLayerInfoFlag)
  {
    for(int i = 0; i < maxSubLayersMinus1; ++i)
    {
      pcSPS->setMaxDecPicBuffering(pcSPS->getMaxDecPicBuffering(maxSubLayersMinus1), i);
      pcSPS->setMaxNumReorderPics(pcSPS->getMaxNumReorderPics(maxSubLayersMinus1), i);
      pcSPS->setMaxLatencyIncreasePlus1(pcSPS->getMaxLatencyIncreasePlus1(maxSubLayersMinus1), i);
    }
  }
}


void HLSyntaxReader::parseSPS(SPS* pcSPS)
{
  uint32_t  uiCode;

#if ENABLE_TRACING
  xTraceSPSHeader ();
#endif

  xReadCode(4, uiCode, "sps_seq_parameter_set_id");              pcSPS->setSPSId(uiCode);
  xReadCode( 4,  uiCode, "sps_video_parameter_set_id" );      pcSPS->setVPSId( uiCode );
  xReadCode(3, uiCode, "sps_max_sub_layers_minus1");          pcSPS->setMaxTLayers   (uiCode + 1);
  CHECK(uiCode > 6, "Invalid maximum number of T-layer signalled");
  xReadCode(2, uiCode, "sps_chroma_format_idc");
  pcSPS->setChromaFormatIdc(ChromaFormat(uiCode));

  xReadCode(2, uiCode, "sps_log2_ctu_size_minus5");
  pcSPS->setCTUSize(1 << (uiCode + 5));
  CHECK(uiCode > 2, "sps_log2_ctu_size_minus5 must be less than or equal to 2");
  unsigned ctbLog2SizeY = uiCode + 5;
  pcSPS->setMaxCUWidth(pcSPS->getCTUSize());
  pcSPS->setMaxCUHeight(pcSPS->getCTUSize());
  xReadFlag(uiCode, "sps_ptl_dpb_hrd_params_present_flag"); pcSPS->setPtlDpbHrdParamsPresentFlag(uiCode);

  if( !pcSPS->getVPSId() )
  {
    CHECK( !pcSPS->getPtlDpbHrdParamsPresentFlag(), "When sps_video_parameter_set_id is equal to 0, the value of sps_ptl_dpb_hrd_params_present_flag shall be equal to 1" );
  }

  if (pcSPS->getPtlDpbHrdParamsPresentFlag())
  {
    parseProfileTierLevel(pcSPS->getProfileTierLevel(), true, pcSPS->getMaxTLayers() - 1);
  }

  xReadFlag(uiCode, "sps_gdr_enabled_flag");
  pcSPS->setGDREnabledFlag(uiCode);

  if (pcSPS->getProfileTierLevel()->getConstraintInfo()->getNoGdrConstraintFlag())
  {
    CHECK(uiCode != 0, "When gci_no_gdr_constraint_flag equal to 1 , the value of sps_gdr_enabled_flag shall be equal to 0");
  }

  xReadFlag(uiCode, "sps_ref_pic_resampling_enabled_flag");          pcSPS->setRprEnabledFlag(uiCode);
  if (pcSPS->getProfileTierLevel()->getConstraintInfo()->getNoRprConstraintFlag())
  {
    CHECK(uiCode != 0, "When gci_no_ref_pic_resampling_constraint_flag is equal to 1, sps_ref_pic_resampling_enabled_flag shall be equal to 0");
  }
  if (uiCode)
  {
    xReadFlag(uiCode, "sps_res_change_in_clvs_allowed_flag");        pcSPS->setResChangeInClvsEnabledFlag(uiCode);
  }
  else
  {
    pcSPS->setResChangeInClvsEnabledFlag(false);
  }

  if (pcSPS->getProfileTierLevel()->getConstraintInfo()->getNoResChangeInClvsConstraintFlag())
  {
    CHECK(uiCode != 0, "When no_res_change_in_clvs_constraint_flag is equal to 1, sps_res_change_in_clvs_allowed_flag shall be equal to 0");
  }

  xReadUvlc( uiCode, "sps_pic_width_max_in_luma_samples" );          pcSPS->setMaxPicWidthInLumaSamples( uiCode );
  xReadUvlc( uiCode, "sps_pic_height_max_in_luma_samples" );         pcSPS->setMaxPicHeightInLumaSamples( uiCode );
  xReadFlag( uiCode, "sps_conformance_window_flag");
  if (uiCode != 0)
  {
    Window& conf = pcSPS->getConformanceWindow();
    xReadUvlc(uiCode, "sps_conf_win_left_offset");               conf.setWindowLeftOffset(uiCode);
    xReadUvlc(uiCode, "sps_conf_win_right_offset");              conf.setWindowRightOffset(uiCode);
    xReadUvlc(uiCode, "sps_conf_win_top_offset");                conf.setWindowTopOffset(uiCode);
    xReadUvlc(uiCode, "sps_conf_win_bottom_offset");             conf.setWindowBottomOffset(uiCode);
  }

  xReadFlag( uiCode, "sps_subpic_info_present_flag" );               pcSPS->setSubPicInfoPresentFlag(uiCode);
  if (pcSPS->getProfileTierLevel()->getConstraintInfo()->getNoSubpicInfoConstraintFlag())
  {
    CHECK(uiCode != 0, "When gci_no_subpic_info_constraint_flag is equal to 1, the value of sps_subpic_info_present_flag shall be equal to 0");
  }

  if (pcSPS->getSubPicInfoPresentFlag())
  {
    const int maxPicWidthInCtus  = ((pcSPS->getMaxPicWidthInLumaSamples() - 1) / pcSPS->getCTUSize()) + 1;
    const int maxPicHeightInCtus = ((pcSPS->getMaxPicHeightInLumaSamples() - 1) / pcSPS->getCTUSize()) + 1;

    xReadUvlc(uiCode, "sps_num_subpics_minus1"); pcSPS->setNumSubPics(uiCode + 1);
    CHECK(uiCode > maxPicWidthInCtus * maxPicHeightInCtus - 1, "Invalid sps_num_subpics_minus1 value");
    if( pcSPS->getNumSubPics() == 1 )
    {
      pcSPS->setSubPicCtuTopLeftX( 0, 0 );
      pcSPS->setSubPicCtuTopLeftY( 0, 0 );
      pcSPS->setSubPicWidth(0, maxPicWidthInCtus);
      pcSPS->setSubPicHeight(0, maxPicHeightInCtus);

      pcSPS->setIndependentSubPicsFlag(1);
      pcSPS->setSubPicSameSizeFlag(0);

      pcSPS->setSubPicTreatedAsPicFlag(0, 1);
      pcSPS->setLoopFilterAcrossSubpicEnabledFlag(0, 0);
    }
    else
    {
      xReadFlag(uiCode, "sps_independent_subpics_flag"); pcSPS->setIndependentSubPicsFlag(uiCode != 0);
      xReadFlag(uiCode, "sps_subpic_same_size_flag"); pcSPS->setSubPicSameSizeFlag(uiCode);
      uint32_t tmpWidthVal   = maxPicWidthInCtus;
      uint32_t tmpHeightVal  = maxPicHeightInCtus;
      uint32_t numSubpicCols = 1;
      for (int picIdx = 0; picIdx < pcSPS->getNumSubPics(); picIdx++)
      {
        if (!pcSPS->getSubPicSameSizeFlag() || picIdx == 0)
        {
          if ((picIdx > 0) && (pcSPS->getMaxPicWidthInLumaSamples() > pcSPS->getCTUSize()))
          {
            xReadCode(ceilLog2(tmpWidthVal), uiCode, "sps_subpic_ctu_top_left_x[ i ]");
            pcSPS->setSubPicCtuTopLeftX(picIdx, uiCode);
          }
          else
          {
            pcSPS->setSubPicCtuTopLeftX(picIdx, 0);
          }
          if ((picIdx > 0) && (pcSPS->getMaxPicHeightInLumaSamples() > pcSPS->getCTUSize()))
          {
            xReadCode(ceilLog2(tmpHeightVal), uiCode, "sps_subpic_ctu_top_left_y[ i ]");
            pcSPS->setSubPicCtuTopLeftY(picIdx, uiCode);
          }
          else
          {
            pcSPS->setSubPicCtuTopLeftY(picIdx, 0);
          }
          if (picIdx <pcSPS->getNumSubPics() - 1 && pcSPS->getMaxPicWidthInLumaSamples() > pcSPS->getCTUSize())
          {
            xReadCode(ceilLog2(tmpWidthVal), uiCode, "sps_subpic_width_minus1[ i ]");
            pcSPS->setSubPicWidth(picIdx, uiCode + 1);
          }
          else
          {
            pcSPS->setSubPicWidth(picIdx, tmpWidthVal - pcSPS->getSubPicCtuTopLeftX(picIdx));
          }
          if (picIdx <pcSPS->getNumSubPics() - 1 && pcSPS->getMaxPicHeightInLumaSamples() > pcSPS->getCTUSize())
          {
            xReadCode(ceilLog2(tmpHeightVal), uiCode, "sps_subpic_height_minus1[ i ]");
            pcSPS->setSubPicHeight(picIdx, uiCode + 1);
          }
          else
          {
            pcSPS->setSubPicHeight(picIdx, tmpHeightVal - pcSPS->getSubPicCtuTopLeftY(picIdx));
          }
          if (pcSPS->getSubPicSameSizeFlag())
          {
            numSubpicCols = tmpWidthVal / pcSPS->getSubPicWidth(0);
            CHECK(!(tmpWidthVal % pcSPS->getSubPicWidth(0) == 0), "sps_subpic_width_minus1[0] is invalid.");
            CHECK(!(tmpHeightVal % pcSPS->getSubPicHeight(0) == 0), "sps_subpic_height_minus1[0] is invalid.");
            CHECK(!(numSubpicCols * (tmpHeightVal / pcSPS->getSubPicHeight(0)) == pcSPS->getNumSubPics()), "when sps_subpic_same_size_flag is equal to, sps_num_subpics_minus1 is invalid");
          }
        }
        else
        {
          pcSPS->setSubPicCtuTopLeftX(picIdx, (picIdx % numSubpicCols) * pcSPS->getSubPicWidth(0));
          pcSPS->setSubPicCtuTopLeftY(picIdx, (picIdx / numSubpicCols) * pcSPS->getSubPicHeight(0));
          pcSPS->setSubPicWidth(picIdx, pcSPS->getSubPicWidth(0));
          pcSPS->setSubPicHeight(picIdx, pcSPS->getSubPicHeight(0));
        }
        if (!pcSPS->getIndependentSubPicsFlag())
        {
          xReadFlag(uiCode, "sps_subpic_treated_as_pic_flag[ i ]");
          pcSPS->setSubPicTreatedAsPicFlag(picIdx, uiCode);
          xReadFlag(uiCode, "sps_loop_filter_across_subpic_enabled_flag[ i ]");
          pcSPS->setLoopFilterAcrossSubpicEnabledFlag(picIdx, uiCode);
        }
        else
        {
          pcSPS->setSubPicTreatedAsPicFlag(picIdx, 1);
          pcSPS->setLoopFilterAcrossSubpicEnabledFlag(picIdx, 0);
        }
      }
    }

    xReadUvlc( uiCode, "sps_subpic_id_len_minus1" );                       pcSPS->setSubPicIdLen( uiCode + 1 );
    CHECK( uiCode > 15, "Invalid sps_subpic_id_len_minus1 value" );
    CHECK( (1 << (uiCode + 1)) < pcSPS->getNumSubPics(), "Invalid sps_subpic_id_len_minus1 value" );
    xReadFlag( uiCode, "sps_subpic_id_mapping_explicitly_signalled_flag" );    pcSPS->setSubPicIdMappingExplicitlySignalledFlag( uiCode != 0 );
    if (pcSPS->getSubPicIdMappingExplicitlySignalledFlag())
    {
      xReadFlag( uiCode, "sps_subpic_id_mapping_present_flag" );                pcSPS->setSubPicIdMappingPresentFlag( uiCode != 0 );
      if (pcSPS->getSubPicIdMappingPresentFlag())
      {
        for (int picIdx = 0; picIdx < pcSPS->getNumSubPics(); picIdx++)
        {
          xReadCode(pcSPS->getSubPicIdLen(), uiCode, "sps_subpic_id[i]");   pcSPS->setSubPicId(picIdx, uiCode);
        }
      }
    }
  }
  else
  {
    pcSPS->setSubPicIdMappingExplicitlySignalledFlag(0);
    pcSPS->setNumSubPics(1);
    pcSPS->setSubPicCtuTopLeftX(0, 0);
    pcSPS->setSubPicCtuTopLeftY(0, 0);
    pcSPS->setSubPicWidth(0, (pcSPS->getMaxPicWidthInLumaSamples() + pcSPS->getCTUSize() - 1) >> floorLog2(pcSPS->getCTUSize()));
    pcSPS->setSubPicHeight(0, (pcSPS->getMaxPicHeightInLumaSamples() + pcSPS->getCTUSize() - 1) >> floorLog2(pcSPS->getCTUSize()));
  }

  if( !pcSPS->getSubPicIdMappingExplicitlySignalledFlag() || !pcSPS->getSubPicIdMappingPresentFlag() )
  {
    for( int picIdx = 0; picIdx < pcSPS->getNumSubPics( ); picIdx++ )
    {
      pcSPS->setSubPicId( picIdx, picIdx );
    }
  }

  xReadUvlc(uiCode, "sps_bitdepth_minus8");
  CHECK(uiCode > 8, "Invalid bit depth signalled");
  const Profile::Name profile = pcSPS->getProfileTierLevel()->getProfileIdc();
  if (profile != Profile::NONE)
  {
    CHECK(uiCode + 8 > ProfileFeatures::getProfileFeatures(profile)->maxBitDepth, "sps_bitdepth_minus8 exceeds range supported by signalled profile");
  }
  pcSPS->setBitDepth(ChannelType::LUMA, 8 + uiCode);
  pcSPS->setBitDepth(ChannelType::CHROMA, 8 + uiCode);
  pcSPS->setQpBDOffset(ChannelType::LUMA, (int) (6 * uiCode));
  pcSPS->setQpBDOffset(ChannelType::CHROMA, (int) (6 * uiCode));

  xReadFlag( uiCode, "sps_entropy_coding_sync_enabled_flag" );       pcSPS->setEntropyCodingSyncEnabledFlag(uiCode == 1);
  xReadFlag( uiCode, "sps_entry_point_offsets_present_flag");   pcSPS->setEntryPointsPresentFlag(uiCode == 1);
  xReadCode(4, uiCode, "sps_log2_max_pic_order_cnt_lsb_minus4");     pcSPS->setBitsForPOC( 4 + uiCode );
  CHECK(uiCode > 12, "sps_log2_max_pic_order_cnt_lsb_minus4 shall be in the range of 0 to 12");

  xReadFlag(uiCode, "sps_poc_msb_cycle_flag");                    pcSPS->setPocMsbCycleFlag(uiCode ? true : false);
  if (pcSPS->getPocMsbCycleFlag())
  {
    xReadUvlc(uiCode, "sps_poc_msb_cycle_len_minus1");                  pcSPS->setPocMsbCycleLen(1 + uiCode);
    CHECK(uiCode > (32 - ( pcSPS->getBitsForPOC() - 4 )- 5), "The value of sps_poc_msb_cycle_len_minus1 shall be in the range of 0 to 32 - sps_log2_max_pic_order_cnt_lsb_minus4 - 5, inclusive");
  }

  // extra bits are for future extensions, we will read, but ignore them,
  // unless a meaning is specified in the spec
  xReadCode(2, uiCode, "sps_num_extra_ph_bytes");  pcSPS->setNumExtraPHBytes(uiCode);
  int numExtraPhBytes = uiCode;
  std::vector<bool> extraPhBitPresentFlags;
  extraPhBitPresentFlags.resize ( 8 * numExtraPhBytes );
  for (int i=0; i < 8*numExtraPhBytes; i++)
  {
    xReadFlag(uiCode, "sps_extra_ph_bit_present_flag[ i ]");
    extraPhBitPresentFlags[i] = uiCode;
  }
  pcSPS->setExtraPHBitPresentFlags(extraPhBitPresentFlags);
  xReadCode(2, uiCode, "sps_num_extra_sh_bytes");  pcSPS->setNumExtraSHBytes(uiCode);
  int numExtraShBytes = uiCode;
  std::vector<bool> extraShBitPresentFlags;
  extraShBitPresentFlags.resize ( 8 * numExtraShBytes );
  for (int i=0; i < 8*numExtraShBytes; i++)
  {
    xReadFlag(uiCode, "sps_extra_sh_bit_present_flag[ i ]");
    extraShBitPresentFlags[i] = uiCode;
  }
  pcSPS->setExtraSHBitPresentFlags(extraShBitPresentFlags);

  if (pcSPS->getPtlDpbHrdParamsPresentFlag())
  {
    if (pcSPS->getMaxTLayers() - 1 > 0)
    {
      xReadFlag(uiCode, "sps_sublayer_dpb_params_flag");     pcSPS->setSubLayerDpbParamsFlag(uiCode ? true : false);
    }
    dpb_parameters(pcSPS->getMaxTLayers() - 1, pcSPS->getSubLayerDpbParamsFlag(), pcSPS);
  }
  unsigned  minQT[3] = { 0, 0, 0 };
  unsigned  maxBTD[3] = { 0, 0, 0 };

  unsigned  maxBTSize[3] = { 0, 0, 0 };
  unsigned  maxTTSize[3] = { 0, 0, 0 };
  xReadUvlc(uiCode, "sps_log2_min_luma_coding_block_size_minus2");
  int log2MinCUSize = uiCode + 2;
  pcSPS->setLog2MinCodingBlockSize(log2MinCUSize);
  CHECK(uiCode > ctbLog2SizeY - 2, "Invalid sps_log2_min_luma_coding_block_size_minus2 signalled");

  CHECK(log2MinCUSize > std::min(6, (int)(ctbLog2SizeY)), "sps_log2_min_luma_coding_block_size_minus2 shall be in the range of 0 to min (4, log2_ctu_size - 2)");
  const int minCuSize = 1 << pcSPS->getLog2MinCodingBlockSize();
  CHECK( ( pcSPS->getMaxPicWidthInLumaSamples() % ( std::max( 8, minCuSize ) ) ) != 0, "Coded frame width must be a multiple of Max(8, the minimum unit size)" );
  CHECK( ( pcSPS->getMaxPicHeightInLumaSamples() % ( std::max( 8, minCuSize ) ) ) != 0, "Coded frame height must be a multiple of Max(8, the minimum unit size)" );

  xReadFlag(uiCode, "sps_partition_constraints_override_enabled_flag"); pcSPS->setSplitConsOverrideEnabledFlag(uiCode);
  xReadUvlc(uiCode, "sps_log2_diff_min_qt_min_cb_intra_slice_luma");
  unsigned minQtLog2SizeIntraY = uiCode + pcSPS->getLog2MinCodingBlockSize();
  minQT[0] = 1 << minQtLog2SizeIntraY;
  CHECK(minQT[0] > 64, "The value of sps_log2_diff_min_qt_min_cb_intra_slice_luma shall be in the range of 0 to min(6,CtbLog2SizeY) - MinCbLog2Size");
  CHECK(minQT[0] > (1<<ctbLog2SizeY), "The value of sps_log2_diff_min_qt_min_cb_intra_slice_luma shall be in the range of 0 to min(6,CtbLog2SizeY) - MinCbLog2Size");
  xReadUvlc(uiCode, "sps_max_mtt_hierarchy_depth_intra_slice_luma");     maxBTD[0] = uiCode;
  CHECK(uiCode > 2 * (ctbLog2SizeY - log2MinCUSize), "sps_max_mtt_hierarchy_depth_intra_slice_luma shall be in the range 0 to 2*(ctbLog2SizeY - log2MinCUSize)");

  maxTTSize[0] = maxBTSize[0] = minQT[0];
  if (maxBTD[0] != 0)
  {
    xReadUvlc(uiCode, "sps_log2_diff_max_bt_min_qt_intra_slice_luma");     maxBTSize[0] <<= uiCode;
    CHECK(uiCode > ctbLog2SizeY - minQtLog2SizeIntraY, "The value of sps_log2_diff_max_bt_min_qt_intra_slice_luma shall be in the range of 0 to CtbLog2SizeY - MinQtLog2SizeIntraY");
    xReadUvlc(uiCode, "sps_log2_diff_max_tt_min_qt_intra_slice_luma");     maxTTSize[0] <<= uiCode;
    CHECK(uiCode > ctbLog2SizeY - minQtLog2SizeIntraY, "The value of sps_log2_diff_max_tt_min_qt_intra_slice_luma shall be in the range of 0 to CtbLog2SizeY - MinQtLog2SizeIntraY");
    CHECK(maxTTSize[0] > 64, "The value of sps_log2_diff_max_tt_min_qt_intra_slice_luma shall be in the range of 0 to min(6,CtbLog2SizeY) - MinQtLog2SizeIntraY");
  }
  if (isChromaEnabled(pcSPS->getChromaFormatIdc()))
  {
    xReadFlag(uiCode, "sps_qtbtt_dual_tree_intra_flag");           pcSPS->setUseDualITree(uiCode);
  }
  else
  {
    pcSPS->setUseDualITree(0);
  }
  if (pcSPS->getUseDualITree())
  {
    xReadUvlc(uiCode, "sps_log2_diff_min_qt_min_cb_intra_slice_chroma"); minQT[2] = 1 << (uiCode + pcSPS->getLog2MinCodingBlockSize());
    xReadUvlc(uiCode, "sps_max_mtt_hierarchy_depth_intra_slice_chroma"); maxBTD[2] = uiCode;
    CHECK(uiCode > 2 * (ctbLog2SizeY - log2MinCUSize), "sps_max_mtt_hierarchy_depth_intra_slice_chroma shall be in the range 0 to 2*(ctbLog2SizeY - log2MinCUSize)");
    maxTTSize[2] = maxBTSize[2] = minQT[2];
    if (maxBTD[2] != 0)
    {
      xReadUvlc(uiCode, "sps_log2_diff_max_bt_min_qt_intra_slice_chroma");       maxBTSize[2] <<= uiCode;
      xReadUvlc(uiCode, "sps_log2_diff_max_tt_min_qt_intra_slice_chroma");       maxTTSize[2] <<= uiCode;
      CHECK(maxTTSize[2] > 64, "The value of sps_log2_diff_max_tt_min_qt_intra_slice_chroma shall be in the range of 0 to min(6,CtbLog2SizeY) - MinQtLog2SizeIntraChroma");
      CHECK(maxBTSize[2] > 64, "The value of sps_log2_diff_max_bt_min_qt_intra_slice_chroma shall be in the range of 0 to min(6,CtbLog2SizeY) - MinQtLog2SizeIntraChroma");
    }
  }
  xReadUvlc(uiCode, "sps_log2_diff_min_qt_min_cb_inter_slice");
  unsigned minQtLog2SizeInterY = uiCode + pcSPS->getLog2MinCodingBlockSize();
  minQT[1] = 1 << minQtLog2SizeInterY;
  xReadUvlc(uiCode, "sps_max_mtt_hierarchy_depth_inter_slice");     maxBTD[1] = uiCode;
  CHECK(uiCode > 2*(ctbLog2SizeY - log2MinCUSize), "sps_max_mtt_hierarchy_depth_inter_slice shall be in the range 0 to 2*(ctbLog2SizeY - log2MinCUSize)");
  maxTTSize[1] = maxBTSize[1] = minQT[1];
  if (maxBTD[1] != 0)
  {
    xReadUvlc(uiCode, "sps_log2_diff_max_bt_min_qt_inter_slice");     maxBTSize[1] <<= uiCode;
    CHECK(uiCode > ctbLog2SizeY - minQtLog2SizeInterY, "The value of sps_log2_diff_max_bt_min_qt_inter_slice shall be in the range of 0 to CtbLog2SizeY - MinQtLog2SizeInterY");
    xReadUvlc(uiCode, "sps_log2_diff_max_tt_min_qt_inter_slice");     maxTTSize[1] <<= uiCode;
    CHECK(uiCode > ctbLog2SizeY - minQtLog2SizeInterY, "The value of sps_log2_diff_max_tt_min_qt_inter_slice shall be in the range of 0 to CtbLog2SizeY - MinQtLog2SizeInterY");
    CHECK(maxTTSize[1] > 64, "The value of sps_log2_diff_max_tt_min_qt_inter_slice shall be in the range of 0 to min(6,CtbLog2SizeY) - MinQtLog2SizeInterY");
  }

  pcSPS->setMinQTSizes(minQT);
  pcSPS->setMaxMTTHierarchyDepth(maxBTD[1], maxBTD[0], maxBTD[2]);
  pcSPS->setMaxBTSize(maxBTSize[1], maxBTSize[0], maxBTSize[2]);
  pcSPS->setMaxTTSize(maxTTSize[1], maxTTSize[0], maxTTSize[2]);

  if (pcSPS->getCTUSize() > 32)
  {
    xReadFlag(uiCode, "sps_max_luma_transform_size_64_flag");        pcSPS->setLog2MaxTbSize((uiCode ? 1 : 0) + 5);
  }
  else
  {
    pcSPS->setLog2MaxTbSize(5);
  }

  xReadFlag(uiCode, "sps_transform_skip_enabled_flag"); pcSPS->setTransformSkipEnabledFlag(uiCode ? true : false);
  if (pcSPS->getTransformSkipEnabledFlag())
  {
    xReadUvlc(uiCode, "sps_log2_transform_skip_max_size_minus2");
    pcSPS->setLog2MaxTransformSkipBlockSize(uiCode + 2);
    xReadFlag(uiCode, "sps_bdpcm_enabled_flag"); pcSPS->setBDPCMEnabledFlag(uiCode ? true : false);
  }
  xReadFlag(uiCode, "sps_mts_enabled_flag");
  pcSPS->setMtsEnabled(uiCode != 0);
  if (pcSPS->getMtsEnabled())
  {
    xReadFlag(uiCode, "sps_explicit_mts_intra_enabled_flag");
    pcSPS->setExplicitMtsIntraEnabled(uiCode != 0);
    xReadFlag(uiCode, "sps_explicit_mts_inter_enabled_flag");
    pcSPS->setExplicitMtsInterEnabled(uiCode != 0);
  }
  xReadFlag(uiCode, "sps_lfnst_enabled_flag");                    pcSPS->setUseLFNST(uiCode != 0);

  if (isChromaEnabled(pcSPS->getChromaFormatIdc()))
  {
    xReadFlag(uiCode, "sps_joint_cbcr_enabled_flag");                pcSPS->setJointCbCrEnabledFlag(uiCode ? true : false);
    ChromaQpMappingTableParams chromaQpMappingTableParams;
    xReadFlag(uiCode, "sps_same_qp_table_for_chroma_flag");        chromaQpMappingTableParams.setSameCQPTableForAllChromaFlag(uiCode);
    int numQpTables = chromaQpMappingTableParams.getSameCQPTableForAllChromaFlag() ? 1 : (pcSPS->getJointCbCrEnabledFlag() ? 3 : 2);
    chromaQpMappingTableParams.setNumQpTables(numQpTables);
    for (int i = 0; i < numQpTables; i++)
    {
      int32_t qpTableStart = 0;
      xReadSvlc(qpTableStart, "sps_qp_table_starts_minus26");
      chromaQpMappingTableParams.setQpTableStartMinus26(i, qpTableStart);
      CHECK(qpTableStart < -QP26 - pcSPS->getQpBDOffset(ChannelType::LUMA) || qpTableStart > 36,
            "The value of sps_qp_table_start_minus26[ i ] shall be in the range of -26 - QpBdOffset to 36 inclusive");
      xReadUvlc(uiCode, "sps_num_points_in_qp_table_minus1");
      chromaQpMappingTableParams.setNumPtsInCQPTableMinus1(i, uiCode);
      CHECK(uiCode > 36 - qpTableStart, "The value of sps_num_points_in_qp_table_minus1[ i ] shall be in the range of "
                                        "0 to 36 - sps_qp_table_start_minus26[ i ], inclusive");
      std::vector<int> deltaQpInValMinus1(chromaQpMappingTableParams.getNumPtsInCQPTableMinus1(i) + 1);
      std::vector<int> deltaQpOutVal(chromaQpMappingTableParams.getNumPtsInCQPTableMinus1(i) + 1);
      for (int j = 0; j <= chromaQpMappingTableParams.getNumPtsInCQPTableMinus1(i); j++)
      {
        xReadUvlc(uiCode, "sps_delta_qp_in_val_minus1");
        deltaQpInValMinus1[j] = uiCode;
        xReadUvlc(uiCode, "sps_delta_qp_diff_val");
        deltaQpOutVal[j] = uiCode ^ deltaQpInValMinus1[j];
      }
      chromaQpMappingTableParams.setDeltaQpInValMinus1(i, deltaQpInValMinus1);
      chromaQpMappingTableParams.setDeltaQpOutVal(i, deltaQpOutVal);
    }
    pcSPS->setChromaQpMappingTableFromParams(chromaQpMappingTableParams, pcSPS->getQpBDOffset(ChannelType::CHROMA));
    pcSPS->deriveChromaQPMappingTables();
  }


  xReadFlag( uiCode, "sps_sao_enabled_flag" );                      pcSPS->setSAOEnabledFlag ( uiCode ? true : false );
  xReadFlag( uiCode, "sps_alf_enabled_flag" );                      pcSPS->setALFEnabledFlag ( uiCode ? true : false );
  if (pcSPS->getALFEnabledFlag() && isChromaEnabled(pcSPS->getChromaFormatIdc()))
  {
    xReadFlag( uiCode, "sps_ccalf_enabled_flag" );                      pcSPS->setCCALFEnabledFlag ( uiCode ? true : false );
  }
  else
  {
    pcSPS->setCCALFEnabledFlag(false);
  }

  xReadFlag(uiCode, "sps_lmcs_enable_flag");                   pcSPS->setUseLmcs(uiCode == 1);

  xReadFlag( uiCode, "sps_weighted_pred_flag" );                    pcSPS->setUseWP( uiCode ? true : false );
  xReadFlag( uiCode, "sps_weighted_bipred_flag" );                  pcSPS->setUseWPBiPred( uiCode ? true : false );

  xReadFlag(uiCode, "sps_long_term_ref_pics_flag");          pcSPS->setLongTermRefsPresent(uiCode);
  if (pcSPS->getVPSId() > 0)
  {
    xReadFlag( uiCode, "sps_inter_layer_prediction_enabled_flag" );  pcSPS->setInterLayerPresentFlag( uiCode );
  }
  else
  {
    pcSPS->setInterLayerPresentFlag(0);
  }
  xReadFlag( uiCode, "sps_idr_rpl_present_flag" );       pcSPS->setIDRRefParamListPresent( (bool) uiCode );
  if (pcSPS->getProfileTierLevel()->getConstraintInfo()->getNoIdrRplConstraintFlag())
  {
    CHECK(uiCode != 0, "When gci_no_idr_rpl_constraint_flag equal to 1 , the value of sps_idr_rpl_present_flag shall be equal to 0");
  }

  xReadFlag(uiCode, "sps_rpl1_same_as_rpl0_flag");
  pcSPS->setRPL1CopyFromRPL0Flag(uiCode);

  //Read candidate for List0
  xReadUvlc(uiCode, "sps_num_ref_pic_lists[0]");
  uint32_t numberOfRPL = uiCode;
  pcSPS->createRplList(REF_PIC_LIST_0, numberOfRPL);
  RPLList              *rplList = pcSPS->getRplList(REF_PIC_LIST_0);
  ReferencePictureList* rpl;
  for (uint32_t ii = 0; ii < numberOfRPL; ii++)
  {
    rpl = rplList->getReferencePictureList(ii);
    parseRefPicList(pcSPS, rpl, ii);
  }

  //Read candidate for List1
  if (!pcSPS->getRPL1CopyFromRPL0Flag())
  {
    xReadUvlc(uiCode, "sps_num_ref_pic_lists[1]");
    numberOfRPL = uiCode;
    pcSPS->createRplList(REF_PIC_LIST_1, numberOfRPL);
    rplList = pcSPS->getRplList(REF_PIC_LIST_1);
    for (uint32_t ii = 0; ii < numberOfRPL; ii++)
    {
      rpl = rplList->getReferencePictureList(ii);
      parseRefPicList(pcSPS, rpl, ii);
    }
  }
  else
  {
    numberOfRPL = pcSPS->getNumRpl(REF_PIC_LIST_0);
    pcSPS->createRplList(REF_PIC_LIST_1, numberOfRPL);
    RPLList *rplListSource = pcSPS->getRplList(REF_PIC_LIST_0);
    RPLList *rplListDest   = pcSPS->getRplList(REF_PIC_LIST_1);
    for (uint32_t ii = 0; ii < numberOfRPL; ii++)
    {
      copyRefPicList(pcSPS, rplListSource->getReferencePictureList(ii), rplListDest->getReferencePictureList(ii));
    }
  }


  xReadFlag(uiCode, "sps_ref_wraparound_enabled_flag");                  pcSPS->setWrapAroundEnabledFlag( uiCode ? true : false );

  if (pcSPS->getWrapAroundEnabledFlag())
  {
    for (int i = 0; i < pcSPS->getNumSubPics(); i++)
    {
      CHECK(pcSPS->getSubPicTreatedAsPicFlag(i) && (pcSPS->getSubPicWidth(i) != (pcSPS->getMaxPicWidthInLumaSamples() + pcSPS->getCTUSize() - 1) / pcSPS->getCTUSize()), "sps_ref_wraparound_enabled_flag cannot be equal to 1 when there is at least one subpicture with SubPicTreatedAsPicFlag equal to 1 and the subpicture's width is not equal to picture's width");
    }
  }

  xReadFlag( uiCode, "sps_temporal_mvp_enabled_flag" );                  pcSPS->setSPSTemporalMVPEnabledFlag(uiCode);

  if ( pcSPS->getSPSTemporalMVPEnabledFlag() )
  {
    xReadFlag(uiCode, "sps_sbtmvp_enabled_flag");
    pcSPS->setSbTMVPEnabledFlag(uiCode != 0);
  }
  else
  {
    pcSPS->setSbTMVPEnabledFlag(false);
  }

  xReadFlag( uiCode,  "sps_amvr_enabled_flag" );                     pcSPS->setAMVREnabledFlag ( uiCode != 0 );

  xReadFlag( uiCode, "sps_bdof_enabled_flag" );                      pcSPS->setBDOFEnabledFlag ( uiCode != 0 );
  if (pcSPS->getBDOFEnabledFlag())
  {
    xReadFlag(uiCode, "sps_bdof_control_present_in_ph_flag");        pcSPS->setBdofControlPresentInPhFlag( uiCode != 0 );
  }
  else
  {
    pcSPS->setBdofControlPresentInPhFlag( false );
  }
  xReadFlag(uiCode, "sps_smvd_enabled_flag");                       pcSPS->setUseSMVD( uiCode != 0 );
  xReadFlag(uiCode, "sps_dmvr_enabled_flag");                        pcSPS->setUseDMVR(uiCode != 0);
  if (pcSPS->getUseDMVR())
  {
    xReadFlag(uiCode, "sps_dmvr_control_present_in_ph_flag");                 pcSPS->setDmvrControlPresentInPhFlag( uiCode != 0 );
  }
  else
  {
    pcSPS->setDmvrControlPresentInPhFlag( false );
  }
  xReadFlag(uiCode, "sps_mmvd_enabled_flag");                        pcSPS->setUseMMVD(uiCode != 0);
  if (pcSPS->getUseMMVD())
  {
    xReadFlag(uiCode, "sps_mmvd_fullpel_only_flag");                pcSPS->setFpelMmvdEnabledFlag(uiCode != 0);
  }
  else
  {
    pcSPS->setFpelMmvdEnabledFlag( false );
  }

  xReadUvlc(uiCode, "sps_six_minus_max_num_merge_cand");
  CHECK(MRG_MAX_NUM_CANDS <= uiCode, "Incorrrect max number of merge candidates!");
  pcSPS->setMaxNumMergeCand(MRG_MAX_NUM_CANDS - uiCode);
  xReadFlag(uiCode, "sps_sbt_enabled_flag");                        pcSPS->setUseSBT                 ( uiCode != 0 );
  xReadFlag( uiCode,    "sps_affine_enabled_flag" );                            pcSPS->setUseAffine              ( uiCode != 0 );
  if ( pcSPS->getUseAffine() )
  {
    xReadUvlc(uiCode, "sps_five_minus_max_num_subblock_merge_cand");
    CHECK(
      uiCode > 5 - (pcSPS->getSbTMVPEnabledFlag() ? 1 : 0),
      "The value of sps_five_minus_max_num_subblock_merge_cand shall be in the range of 0 to 5 - sps_sbtmvp_enabled_flag");
    CHECK(AFFINE_MRG_MAX_NUM_CANDS < uiCode, "The value of sps_five_minus_max_num_subblock_merge_cand shall be in the range of 0 to 5 - sps_sbtmvp_enabled_flag");
    pcSPS->setMaxNumAffineMergeCand(AFFINE_MRG_MAX_NUM_CANDS - uiCode);
    xReadFlag( uiCode,  "sps_affine_type_flag" );                       pcSPS->setUseAffineType          ( uiCode != 0 );
    if( pcSPS->getAMVREnabledFlag())
    {
      xReadFlag( uiCode, "sps_affine_amvr_enabled_flag" );            pcSPS->setAffineAmvrEnabledFlag  ( uiCode != 0 );
    }
    xReadFlag( uiCode, "sps_affine_prof_enabled_flag" );            pcSPS->setUsePROF                ( uiCode != 0 );
    if (pcSPS->getUsePROF())
    {
      xReadFlag(uiCode, "sps_prof_control_present_in_ph_flag");               pcSPS->setProfControlPresentInPhFlag ( uiCode != 0 );
    }
    else
    {
      pcSPS->setProfControlPresentInPhFlag( false );
    }
  }

  xReadFlag( uiCode,    "sps_bcw_enabled_flag" );                   pcSPS->setUseBcw( uiCode != 0 );

  xReadFlag( uiCode,     "sps_ciip_enabled_flag" );                           pcSPS->setUseCiip             ( uiCode != 0 );
  if (pcSPS->getMaxNumMergeCand() >= 2)
  {
    xReadFlag(uiCode, "sps_gpm_enabled_flag");
    pcSPS->setUseGeo(uiCode != 0);
    if (pcSPS->getUseGeo())
    {
      if (pcSPS->getMaxNumMergeCand() >= 3)
      {
        xReadUvlc(uiCode, "sps_max_num_merge_cand_minus_max_num_gpm_cand");
        CHECK(pcSPS->getMaxNumMergeCand() - 2 < uiCode,
              "sps_max_num_merge_cand_minus_max_num_gpm_cand must not be greater than the number of merge candidates minus 2");
        pcSPS->setMaxNumGeoCand((uint32_t)(pcSPS->getMaxNumMergeCand() - uiCode));
      }
      else
      {
        pcSPS->setMaxNumGeoCand(2);
      }
    }
  }
  else
  {
    pcSPS->setUseGeo(0);
    pcSPS->setMaxNumGeoCand(0);
  }

  xReadUvlc(uiCode, "sps_log2_parallel_merge_level_minus2");
  CHECK(uiCode + 2 > ctbLog2SizeY, "The value of sps_log2_parallel_merge_level_minus2 shall be in the range of 0 to ctbLog2SizeY - 2");
  pcSPS->setLog2ParallelMergeLevelMinus2(uiCode);


  xReadFlag(uiCode, "sps_isp_enabled_flag");                        pcSPS->setUseISP( uiCode != 0 );
  xReadFlag(uiCode, "sps_mrl_enabled_flag");                        pcSPS->setUseMRL( uiCode != 0 );
  xReadFlag(uiCode, "sps_mip_enabled_flag");                        pcSPS->setUseMIP( uiCode != 0 );
  if (isChromaEnabled(pcSPS->getChromaFormatIdc()))
  {
    xReadFlag( uiCode, "sps_cclm_enabled_flag" );                   pcSPS->setUseLMChroma( uiCode != 0 );
  }
  else
  {
    pcSPS->setUseLMChroma(0);
  }
  if (pcSPS->getChromaFormatIdc() == ChromaFormat::_420)
  {
    xReadFlag( uiCode, "sps_chroma_horizontal_collocated_flag" );   pcSPS->setHorCollocatedChromaFlag( uiCode != 0 );
    xReadFlag( uiCode, "sps_chroma_vertical_collocated_flag" );     pcSPS->setVerCollocatedChromaFlag( uiCode != 0 );
  }
  else
  {
    pcSPS->setHorCollocatedChromaFlag(true);
    pcSPS->setVerCollocatedChromaFlag(true);
  }
  xReadFlag( uiCode,  "sps_palette_enabled_flag");                                pcSPS->setPLTMode                ( uiCode != 0 );
  CHECK((profile == Profile::MAIN_12 || profile == Profile::MAIN_12_INTRA || profile == Profile::MAIN_12_STILL_PICTURE)
    && uiCode != 0, "sps_palette_enabled_flag shall be equal to 0 for Main 12 (420) profiles");
  if (pcSPS->getChromaFormatIdc() == ChromaFormat::_444 && pcSPS->getLog2MaxTbSize() != 6)
  {
    xReadFlag(uiCode, "sps_act_enabled_flag");                                pcSPS->setUseColorTrans(uiCode != 0);
  }
  else
  {
    pcSPS->setUseColorTrans(false);
  }
  if (pcSPS->getTransformSkipEnabledFlag() || pcSPS->getPLTMode())
  {
    xReadUvlc(uiCode, "sps_internal_bit_depth_minus_input_bit_depth");
    pcSPS->setInternalMinusInputBitDepth(ChannelType::LUMA, uiCode);
    CHECK(uiCode > 8, "Invalid sps_internal_bit_depth_minus_input_bit_depth signalled");
    pcSPS->setInternalMinusInputBitDepth(ChannelType::CHROMA, uiCode);
  }
  xReadFlag(uiCode, "sps_ibc_enabled_flag");                                    pcSPS->setIBCFlag(uiCode != 0);
  if (pcSPS->getIBCFlag())
  {
    xReadUvlc(uiCode, "sps_six_minus_max_num_ibc_merge_cand");
    CHECK(IBC_MRG_MAX_NUM_CANDS <= uiCode, "Incorrect max number of IBC merge candidates!");
    pcSPS->setMaxNumIBCMergeCand(IBC_MRG_MAX_NUM_CANDS - uiCode);
  }
  else
  {
    pcSPS->setMaxNumIBCMergeCand(0);
  }

  xReadFlag( uiCode, "sps_ladf_enabled_flag" );                     pcSPS->setLadfEnabled( uiCode != 0 );
  if ( pcSPS->getLadfEnabled() )
  {
    int signedSymbol = 0;
    xReadCode( 2, uiCode, "sps_num_ladf_intervals_minus2");         pcSPS->setLadfNumIntervals( uiCode + 2 );
    xReadSvlc(signedSymbol, "sps_ladf_lowest_interval_qp_offset" );      pcSPS->setLadfQpOffset( signedSymbol, 0 );
    for ( int k = 1; k < pcSPS->getLadfNumIntervals(); k++ )
    {
      xReadSvlc(signedSymbol, "sps_ladf_qp_offset" );                    pcSPS->setLadfQpOffset( signedSymbol, k );
      xReadUvlc( uiCode, "sps_ladf_delta_threshold_minus1");
      pcSPS->setLadfIntervalLowerBound(uiCode + pcSPS->getLadfIntervalLowerBound(k - 1) + 1, k);
    }
  }
  xReadFlag(uiCode, "sps_explicit_scaling_list_enabled_flag");                 pcSPS->setScalingListFlag(uiCode);
  if (pcSPS->getProfileTierLevel()->getConstraintInfo()->getNoExplicitScaleListConstraintFlag())
  {
    CHECK(uiCode != 0, "When gci_no_explicit_scaling_list_constraint_flag is equal to 1, sps_explicit_scaling_list_enabled_flag shall be equal to 0");
  }

  if (pcSPS->getUseLFNST() && pcSPS->getScalingListFlag())
  {
    xReadFlag(uiCode, "sps_scaling_matrix_for_lfnst_disabled_flag"); pcSPS->setDisableScalingMatrixForLfnstBlks(uiCode ? true : false);
  }

  if (pcSPS->getUseColorTrans() && pcSPS->getScalingListFlag())
  {
    xReadFlag(uiCode, "sps_scaling_matrix_for_alternative_colour_space_disabled_flag"); pcSPS->setScalingMatrixForAlternativeColourSpaceDisabledFlag(uiCode);
  }
  if (pcSPS->getScalingMatrixForAlternativeColourSpaceDisabledFlag())
  {
    xReadFlag(uiCode, "sps_scaling_matrix_designated_colour_space_flag"); pcSPS->setScalingMatrixDesignatedColourSpaceFlag(uiCode);
  }
  xReadFlag(uiCode, "sps_dep_quant_enabled_flag"); pcSPS->setDepQuantEnabledFlag(uiCode);
  xReadFlag(uiCode, "sps_sign_data_hiding_enabled_flag"); pcSPS->setSignDataHidingEnabledFlag(uiCode);

  xReadFlag( uiCode, "sps_virtual_boundaries_enabled_flag" ); pcSPS->setVirtualBoundariesEnabledFlag( uiCode != 0 );
  if (pcSPS->getProfileTierLevel()->getConstraintInfo()->getNoVirtualBoundaryConstraintFlag())
  {
    CHECK(uiCode != 0, "When gci_no_virtual_boundaries_constraint_flag is equal to 1, sps_virtual_boundaries_enabled_flag shall be equal to 0");
  }

  if( pcSPS->getVirtualBoundariesEnabledFlag() )
  {
    xReadFlag( uiCode, "sps_loop_filter_across_virtual_boundaries_present_flag" ); pcSPS->setVirtualBoundariesPresentFlag( uiCode != 0 );
    if( pcSPS->getVirtualBoundariesPresentFlag() )
    {
      xReadUvlc(uiCode, "sps_num_ver_virtual_boundaries");
      pcSPS->setNumVerVirtualBoundaries(uiCode);
      if (pcSPS->getMaxPicWidthInLumaSamples() <= 8)
      {
        CHECK(pcSPS->getNumVerVirtualBoundaries() != 0, "SPS: When picture width is less than or equal to 8, the "
                                                        "number of vertical virtual boundaries shall be equal to 0");
      }
      else
      {
        CHECK(pcSPS->getNumVerVirtualBoundaries() > 3,
              "SPS: The number of vertical virtual boundaries shall be in the range of 0 to 3");
      }
      for (unsigned i = 0; i < pcSPS->getNumVerVirtualBoundaries(); i++)
      {
        xReadUvlc(uiCode, "sps_virtual_boundary_pos_x_minus1[i]");
        pcSPS->setVirtualBoundariesPosX((uiCode + 1) << 3, i);
        CHECK(uiCode > (((pcSPS->getMaxPicWidthInLumaSamples() + 7) >> 3) - 2),
              "The value of sps_virtual_boundary_pos_x_minus1[ i ] shall be in the range of 0 to Ceil( "
              "sps_pic_width_max_in_luma_samples / 8 ) - 2, inclusive.");
      }
      xReadUvlc(uiCode, "sps_num_hor_virtual_boundaries");
      pcSPS->setNumHorVirtualBoundaries(uiCode);
      if (pcSPS->getMaxPicHeightInLumaSamples() <= 8)
      {
        CHECK(pcSPS->getNumHorVirtualBoundaries() != 0, "SPS: When picture height is less than or equal to 8, the "
                                                        "number of horizontal virtual boundaries shall be equal to 0");
      }
      else
      {
        CHECK(pcSPS->getNumHorVirtualBoundaries() > 3,
              "SPS: The number of horizontal virtual boundaries shall be in the range of 0 to 3");
      }
      for (unsigned i = 0; i < pcSPS->getNumHorVirtualBoundaries(); i++)
      {
        xReadUvlc(uiCode, "sps_virtual_boundary_pos_y_minus1[i]");
        pcSPS->setVirtualBoundariesPosY((uiCode + 1) << 3, i);
        CHECK(uiCode > (((pcSPS->getMaxPicHeightInLumaSamples() + 7) >> 3) - 2),
              "The value of sps_virtual_boundary_pos_y_minus1[ i ] shall be in the range of 0 to Ceil( "
              "sps_pic_height_max_in_luma_samples / 8 ) - 2, inclusive.");
      }
    }
    else
    {
      pcSPS->setNumVerVirtualBoundaries(0);
      pcSPS->setNumHorVirtualBoundaries(0);
    }
  }
  else
  {
    pcSPS->setVirtualBoundariesPresentFlag(false);
  }

  if (pcSPS->getPtlDpbHrdParamsPresentFlag())
  {
    xReadFlag(uiCode, "sps_timing_hrd_params_present_flag");        pcSPS->setGeneralHrdParametersPresentFlag(uiCode);
    if (pcSPS->getGeneralHrdParametersPresentFlag())
    {
      parseGeneralHrdParameters(pcSPS->getGeneralHrdParameters());
      if ((pcSPS->getMaxTLayers()-1) > 0)
      {
        xReadFlag(uiCode, "sps_sublayer_cpb_params_present_flag");  pcSPS->setSubLayerParametersPresentFlag(uiCode);
      }
      else if((pcSPS->getMaxTLayers()-1) == 0)
      {
        pcSPS->setSubLayerParametersPresentFlag(false);
      }

      uint32_t firstSubLayer = pcSPS->getSubLayerParametersPresentFlag() ? 0 : (pcSPS->getMaxTLayers() - 1);
      parseOlsHrdParameters(pcSPS->getGeneralHrdParameters(),pcSPS->getOlsHrdParameters(), firstSubLayer, pcSPS->getMaxTLayers() - 1);
    }
  }

  xReadFlag(     uiCode, "sps_field_seq_flag");                       pcSPS->setFieldSeqFlag(uiCode);
  CHECK( pcSPS->getProfileTierLevel()->getFrameOnlyConstraintFlag() && uiCode, "When ptl_frame_only_constraint_flag equal to 1 , the value of sps_field_seq_flag shall be equal to 0");

  xReadFlag( uiCode, "sps_vui_parameters_present_flag" );             pcSPS->setVuiParametersPresentFlag(uiCode);

  if (pcSPS->getVuiParametersPresentFlag())
  {
    xReadUvlc(uiCode, "sps_vui_payload_size_minus1");
    pcSPS->setVuiPayloadSize(uiCode+1);
    while (!isByteAligned())
    {
      xReadFlag(uiCode, "sps_vui_alignment_zero_bit");
      CHECK(uiCode != 0, "sps_vui_alignment_zero_bit not equal to 0");
    }
    parseVUI(pcSPS->getVuiParameters(), pcSPS);
  }

  xReadFlag( uiCode, "sps_extension_present_flag");

  if (uiCode)
  {
    static const char *syntaxStrings[] = {
      "sps_range_extension_flag", "sps_extension_7bits[0]", "sps_extension_7bits[1]", "sps_extension_7bits[2]",
      "sps_extension_7bits[3]",   "sps_extension_7bits[4]", "sps_extension_7bits[5]", "sps_extension_7bits[6]",
    };

    bool sps_extension_flags[NUM_SPS_EXTENSION_FLAGS];

    for(int i=0; i<NUM_SPS_EXTENSION_FLAGS; i++)
    {
      xReadFlag( uiCode, syntaxStrings[i] );
      sps_extension_flags[i] = uiCode!=0;
    }

    if (pcSPS->getBitDepth(ChannelType::LUMA) <= 10)
    {
      CHECK(sps_extension_flags[SPS_EXT__REXT] == 1,
            "The value of sps_range_extension_flag shall be 0 when BitDepth is less than or equal to 10.");
    }

    bool bSkipTrailingExtensionBits=false;
    for(int i=0; i<NUM_SPS_EXTENSION_FLAGS; i++) // loop used so that the order is determined by the enum.
    {
      if (sps_extension_flags[i])
      {
        switch (SPSExtensionFlagIndex(i))
        {
        case SPS_EXT__REXT:
          CHECK(bSkipTrailingExtensionBits, "Skipping trailing extension bits not supported");
          {
            SPSRExt &spsRangeExtension = pcSPS->getSpsRangeExtension();
            xReadFlag( uiCode, "extended_precision_processing_flag");       spsRangeExtension.setExtendedPrecisionProcessingFlag (uiCode != 0);
            if (pcSPS->getTransformSkipEnabledFlag())
            {
              xReadFlag( uiCode, "sps_ts_residual_coding_rice_present_in_sh_flag"); spsRangeExtension.setTSRCRicePresentFlag(uiCode != 0);
            }
            xReadFlag(uiCode,  "rrc_rice_extension_flag");                  spsRangeExtension.setRrcRiceExtensionEnableFlag      (uiCode != 0);
            xReadFlag( uiCode, "persistent_rice_adaptation_enabled_flag");  spsRangeExtension.setPersistentRiceAdaptationEnabledFlag (uiCode != 0);
            xReadFlag( uiCode, "reverse_last_position_enabled_flag");       spsRangeExtension.setReverseLastSigCoeffEnabledFlag(uiCode != 0);
          }
          break;
        default:
          bSkipTrailingExtensionBits=true;
          break;
        }
      }
    }
    if (bSkipTrailingExtensionBits)
    {
      while ( xMoreRbspData() )
      {
        xReadFlag( uiCode, "sps_extension_data_flag");
      }
    }
  }
  xReadRbspTrailingBits();
}

void HLSyntaxReader::parseOPI(OPI* opi)
{
#if ENABLE_TRACING
  xTraceOPIHeader();
#endif
  uint32_t  symbol;

  xReadFlag(symbol, "opi_ols_info_present_flag");
  opi->setOlsInfoPresentFlag(symbol);
  xReadFlag(symbol, "opi_htid_info_present_flag");
  opi->setHtidInfoPresentFlag(symbol);

  if (opi->getOlsInfoPresentFlag())
  {
    xReadUvlc(symbol, "opi_ols_idx");
    opi->setOpiOlsIdx(symbol);
  }

  if (opi->getHtidInfoPresentFlag())
  {
    xReadCode(3, symbol, "opi_htid_plus1");
    opi->setOpiHtidPlus1(symbol);
  }

  xReadFlag(symbol, "opi_extension_flag");
  if (symbol)
  {
    while (xMoreRbspData())
    {
      xReadFlag(symbol, "opi_extension_data_flag");
    }
  }
  xReadRbspTrailingBits();
}


void HLSyntaxReader::parseDCI(DCI* dci)
{
#if ENABLE_TRACING
  xTraceDCIHeader();
#endif
  uint32_t  symbol;

  xReadCode(4, symbol, "dci_reserved_zero_4bits");

  uint32_t numPTLs;
  xReadCode(4, numPTLs, "dci_num_ptls_minus1");
  numPTLs += 1;

  std::vector<ProfileTierLevel> ptls;
  ptls.resize(numPTLs);
  for (int i = 0; i < numPTLs; i++)
  {
    parseProfileTierLevel(&ptls[i], true, 0);
  }
  dci->setProfileTierLevel(ptls);

  xReadFlag(symbol, "dci_extension_flag");
  if (symbol)
  {
    while (xMoreRbspData())
    {
      xReadFlag(symbol, "dci_extension_data_flag");
    }
  }
  xReadRbspTrailingBits();
}

void HLSyntaxReader::parseVPS(VPS* pcVPS)
{
#if ENABLE_TRACING
  xTraceVPSHeader();
#endif
  uint32_t  uiCode;

  xReadCode(4, uiCode, "vps_video_parameter_set_id");
  CHECK( uiCode == 0, "vps_video_parameter_set_id equal to zero is reserved and shall not be used in a bitstream" );
  pcVPS->setVPSId(uiCode);

  xReadCode(6, uiCode, "vps_max_layers_minus1");              pcVPS->setMaxLayers(uiCode + 1);    CHECK(uiCode + 1 > MAX_VPS_LAYERS, "Signalled number of layers larger than MAX_VPS_LAYERS.");
  if (pcVPS->getMaxLayers() - 1 == 0)
  {
    pcVPS->setEachLayerIsAnOlsFlag(1);
  }
  xReadCode(3, uiCode, "vps_max_sublayers_minus1");           pcVPS->setMaxSubLayers(uiCode + 1); CHECK(uiCode + 1 > MAX_VPS_SUBLAYERS, "Signalled number of sublayers larger than MAX_VPS_SUBLAYERS.");
  if( pcVPS->getMaxLayers() > 1 && pcVPS->getMaxSubLayers() > 1)
  {
    xReadFlag(uiCode, "vps_default_ptl_dpb_hrd_max_tid_flag"); pcVPS->setDefaultPtlDpbHrdMaxTidFlag(uiCode);
  }
  else
  {
    pcVPS->setDefaultPtlDpbHrdMaxTidFlag(1);
  }
  if( pcVPS->getMaxLayers() > 1 )
  {
    xReadFlag(uiCode, "vps_all_independent_layers_flag");  pcVPS->setAllIndependentLayersFlag(uiCode);
    if (pcVPS->getAllIndependentLayersFlag() == 0)
    {
      pcVPS->setEachLayerIsAnOlsFlag(0);
    }
  }
  std::vector<std::vector<uint32_t>> maxTidilRefPicsPlus1;
  maxTidilRefPicsPlus1.resize(pcVPS->getMaxLayers(), std::vector<uint32_t>(pcVPS->getMaxLayers(), NOT_VALID));
  pcVPS->setMaxTidIlRefPicsPlus1(maxTidilRefPicsPlus1);
  for (uint32_t i = 0; i < pcVPS->getMaxLayers(); i++)
  {
    xReadCode(6, uiCode, "vps_layer_id");                     pcVPS->setLayerId(i, uiCode);
    pcVPS->setGeneralLayerIdx(uiCode, i);

    if (i > 0 && !pcVPS->getAllIndependentLayersFlag())
    {
      xReadFlag(uiCode, "vps_independent_layer_flag");     pcVPS->setIndependentLayerFlag(i, uiCode);
      if (!pcVPS->getIndependentLayerFlag(i))
      {
        xReadFlag(uiCode, "max_tid_ref_present_flag[ i ]");
        bool presentFlag = uiCode;
        uint16_t sumUiCode = 0;
        for (int j = 0, k = 0; j < i; j++)
        {
          xReadFlag(uiCode, "vps_direct_ref_layer_flag"); pcVPS->setDirectRefLayerFlag(i, j, uiCode);
          if (uiCode)
          {
            pcVPS->setInterLayerRefIdc(i, j, k);
            pcVPS->setDirectRefLayerIdx(i, k++, j);
            sumUiCode++;
          }
          if (presentFlag && pcVPS->getDirectRefLayerFlag(i, j))
          {
            xReadCode(3, uiCode, "max_tid_il_ref_pics_plus1[ i ][ j ]");
            pcVPS->setMaxTidIlRefPicsPlus1(i, j, uiCode);
          }
          else
          {
            pcVPS->setMaxTidIlRefPicsPlus1(i, j, 7);
          }
        }
        CHECK(sumUiCode == 0, "There has to be at least one value of j such that the value of vps_direct_dependency_flag[ i ][ j ] is equal to 1,when vps_independent_layer_flag[ i ] is equal to 0 ");
      }
    }
  }

  if (pcVPS->getMaxLayers() > 1)
  {
    if (pcVPS->getAllIndependentLayersFlag())
    {
      xReadFlag(uiCode, "vps_each_layer_is_an_ols_flag");  pcVPS->setEachLayerIsAnOlsFlag(uiCode);
      if (pcVPS->getEachLayerIsAnOlsFlag() == 0)
      {
        pcVPS->setOlsModeIdc(2);
      }
    }
    if (!pcVPS->getEachLayerIsAnOlsFlag())
    {
      if (!pcVPS->getAllIndependentLayersFlag())
      {
        xReadCode(2, uiCode, "vps_ols_mode_idc");         pcVPS->setOlsModeIdc(uiCode); CHECK(uiCode > MAX_VPS_OLS_MODE_IDC, "vps_ols_mode_idc shall be in the range of 0 to 2");
      }
      if (pcVPS->getOlsModeIdc() == 2)
      {
        xReadCode(8, uiCode, "vps_num_output_layer_sets_minus2");   pcVPS->setNumOutputLayerSets(uiCode + 2);
        pcVPS->setOlsOutputLayerFlag(0, 0, true);
        for (uint32_t i = 1; i <= pcVPS->getNumOutputLayerSets() - 1; i++)
        {
          for (uint32_t j = 0; j < pcVPS->getMaxLayers(); j++)
          {
            xReadFlag(uiCode, "vps_ols_output_layer_flag");        pcVPS->setOlsOutputLayerFlag(i, j, uiCode);
          }
        }
      }
    }
    xReadCode(8, uiCode, "vps_num_ptls_minus1");      pcVPS->setNumPtls(uiCode + 1);
  }
  else
  {
    pcVPS->setNumPtls(1);
  }
  pcVPS->deriveOutputLayerSets();
  CHECK( pcVPS->getNumPtls() > pcVPS->getTotalNumOLSs(),"The value of vps_num_ptls_minus1 shall be less than TotalNumOlss");
  std::vector<bool> isPTLReferred( pcVPS->getNumPtls(), false);

  for (int i = 0; i < pcVPS->getNumPtls(); i++)
  {
    if(i > 0)
    {
      xReadFlag(uiCode, "vps_pt_present_flag");
      pcVPS->setPtPresentFlag(i, uiCode != 0);
    }
    else
    {
      pcVPS->setPtPresentFlag(0, true);
    }
    if (!pcVPS->getDefaultPtlDpbHrdMaxTidFlag())
    {
      xReadCode(3, uiCode, "vps_ptl_max_tid");
      pcVPS->setPtlMaxTemporalId(i, uiCode);
    }
    else
    {
      pcVPS->setPtlMaxTemporalId(i, pcVPS->getMaxSubLayers() - 1);
    }
  }
  int cnt = 0;
  while (m_pcBitstream->getNumBitsUntilByteAligned())
  {
    xReadFlag( uiCode, "vps_ptl_reserved_zero_bit");
    CHECK(uiCode!=0, "Alignment bit is not '0'");
    cnt++;
  }
  CHECK(cnt >= 8, "Read more than '8' alignment bits");

  for (int i = 0; i < pcVPS->getNumPtls(); i++)
  {
    ProfileTierLevel ptl;
    parseProfileTierLevel(&ptl, pcVPS->getPtPresentFlag(i), pcVPS->getPtlMaxTemporalId(i));

    if (!pcVPS->getPtPresentFlag(i))
    {
      CHECK(i == 0, "Profile/Tier should always be present for first entry");

      ptl.copyProfileTierConstraintsFrom(pcVPS->getProfileTierLevel(i - 1));
    }
    pcVPS->setProfileTierLevel(i, ptl);
  }

  for (int i = 0; i < pcVPS->getTotalNumOLSs(); i++)
  {
    if (pcVPS->getNumPtls() > 1 && pcVPS->getNumPtls() != pcVPS->getTotalNumOLSs())
    {
      xReadCode(8, uiCode, "vps_ols_ptl_idx");
      pcVPS->setOlsPtlIdx(i, uiCode);
    }
    else if (pcVPS->getNumPtls() == pcVPS->getTotalNumOLSs())
    {
      pcVPS->setOlsPtlIdx(i, i);
    }
    else
    {
      pcVPS->setOlsPtlIdx(i, 0);
    }
    isPTLReferred[pcVPS->getOlsPtlIdx(i)] = true;
  }
  for( int i = 0; i < pcVPS->getNumPtls(); i++ )
  {
    CHECK( !isPTLReferred[i],"Each profile_tier_level( ) syntax structure in the VPS shall be referred to by at least one value of vps_ols_ptl_idx[ i ] for i in the range of 0 to TotalNumOlss ? 1, inclusive");
  }

  if( !pcVPS->getEachLayerIsAnOlsFlag() )
  {
    xReadUvlc( uiCode, "vps_num_dpb_params_minus1" ); pcVPS->m_numDpbParams = uiCode + 1;

    CHECK( pcVPS->m_numDpbParams > pcVPS->getNumMultiLayeredOlss(),"The value of vps_num_dpb_params_minus1 shall be in the range of 0 to NumMultiLayerOlss - 1, inclusive");
    std::vector<bool> isDPBParamReferred(pcVPS->m_numDpbParams, false);

    if( pcVPS->m_numDpbParams > 0 && pcVPS->getMaxSubLayers() > 1 )
    {
      xReadFlag( uiCode, "vps_sublayer_dpb_params_present_flag" ); pcVPS->m_sublayerDpbParamsPresentFlag = uiCode;
    }

    pcVPS->m_dpbParameters.resize( pcVPS->m_numDpbParams );

    for( int i = 0; i < pcVPS->m_numDpbParams; i++ )
    {
      if (!pcVPS->getDefaultPtlDpbHrdMaxTidFlag())
      {
        xReadCode(3, uiCode, "vps_dpb_max_tid[i]");
        pcVPS->m_dpbMaxTemporalId.push_back(uiCode);
        CHECK (uiCode > (pcVPS->getMaxSubLayers() - 1), "The value of vps_dpb_max_tid[i] shall be in the range of 0 to vps_max_sublayers_minus1, inclusive." )
      }
      else
      {
        pcVPS->m_dpbMaxTemporalId.push_back(pcVPS->getMaxSubLayers() - 1);
      }

      for( int j = ( pcVPS->m_sublayerDpbParamsPresentFlag ? 0 : pcVPS->m_dpbMaxTemporalId[i] ); j <= pcVPS->m_dpbMaxTemporalId[i]; j++ )
      {
        xReadUvlc(uiCode, "dpb_max_dec_pic_buffering_minus1[i]");
        pcVPS->m_dpbParameters[i].maxDecPicBuffering[j] = uiCode + 1;
        xReadUvlc( uiCode, "dpb_max_num_reorder_pics[i]" );          pcVPS->m_dpbParameters[i].maxNumReorderPics[j] = uiCode;
        xReadUvlc( uiCode, "dpb_max_latency_increase_plus1[i]" );    pcVPS->m_dpbParameters[i].maxLatencyIncreasePlus1[j] = uiCode;
      }

      for( int j = ( pcVPS->m_sublayerDpbParamsPresentFlag ? pcVPS->m_dpbMaxTemporalId[i] : 0 ); j < pcVPS->m_dpbMaxTemporalId[i]; j++ )
      {
        // When dpb_max_dec_pic_buffering_minus1[ i ] is not present for i in the range of 0 to maxSubLayersMinus1 - 1, inclusive, due to subLayerInfoFlag being equal to 0, it is inferred to be equal to dpb_max_dec_pic_buffering_minus1[ maxSubLayersMinus1 ].
        pcVPS->m_dpbParameters[i].maxDecPicBuffering[j] = pcVPS->m_dpbParameters[i].maxDecPicBuffering[pcVPS->m_dpbMaxTemporalId[i]];

        // When dpb_max_num_reorder_pics[ i ] is not present for i in the range of 0 to maxSubLayersMinus1 - 1, inclusive, due to subLayerInfoFlag being equal to 0, it is inferred to be equal to dpb_max_num_reorder_pics[ maxSubLayersMinus1 ].
        pcVPS->m_dpbParameters[i].maxNumReorderPics[j] = pcVPS->m_dpbParameters[i].maxNumReorderPics[pcVPS->m_dpbMaxTemporalId[i]];

        // When dpb_max_latency_increase_plus1[ i ] is not present for i in the range of 0 to maxSubLayersMinus1 - 1, inclusive, due to subLayerInfoFlag being equal to 0, it is inferred to be equal to dpb_max_latency_increase_plus1[ maxSubLayersMinus1 ].
        pcVPS->m_dpbParameters[i].maxLatencyIncreasePlus1[j] = pcVPS->m_dpbParameters[i].maxLatencyIncreasePlus1[pcVPS->m_dpbMaxTemporalId[i]];
      }
    }


    for( int i = 0, j=0; i < pcVPS->getTotalNumOLSs(); i++ )
    {
      if( pcVPS->m_numLayersInOls[i] > 1 )
      {
        xReadUvlc( uiCode, "vps_ols_dpb_pic_width[i]" ); pcVPS->setOlsDpbPicWidth( i, uiCode );
        xReadUvlc( uiCode, "vps_ols_dpb_pic_height[i]" ); pcVPS->setOlsDpbPicHeight( i, uiCode );
        xReadCode(2, uiCode, "vps_ols_dpb_chroma_format[i]");
        pcVPS->setOlsDpbChromaFormatIdc(i, static_cast<ChromaFormat>(uiCode));
        xReadUvlc( uiCode, "vps_ols_dpb_bitdepth_minus8[i]"); pcVPS->setOlsDpbBitDepthMinus8(i, uiCode);
        const Profile::Name profile = pcVPS->getProfileTierLevel(pcVPS->getOlsPtlIdx(i)).getProfileIdc();
        if (profile != Profile::NONE)
        {
          CHECK(uiCode + 8 > ProfileFeatures::getProfileFeatures(profile)->maxBitDepth, "vps_ols_dpb_bitdepth_minus8[ i ] exceeds range supported by signalled profile");
        }
        if ((pcVPS->m_numDpbParams > 1) && (pcVPS->m_numDpbParams != pcVPS->m_numMultiLayeredOlss))
        {
          xReadUvlc( uiCode, "vps_ols_dpb_params_idx[i]" ); pcVPS->setOlsDpbParamsIdx( i, uiCode );
        }
        else if (pcVPS->m_numDpbParams == 1)
        {
          pcVPS->setOlsDpbParamsIdx(i, 0);
        }
        else
        {
          pcVPS->setOlsDpbParamsIdx(i, j);
        }
        j += 1;
        isDPBParamReferred[pcVPS->getOlsDpbParamsIdx(i)] = true;
      }
    }
    for( int i = 0; i < pcVPS->m_numDpbParams; i++ )
    {
      CHECK( !isDPBParamReferred[i],"Each dpb_parameters( ) syntax structure in the VPS shall be referred to by at least one value of vps_ols_dpb_params_idx[i] for i in the range of 0 to NumMultiLayerOlss - 1, inclusive");
    }
  }

  if (!pcVPS->getEachLayerIsAnOlsFlag())
  {
    xReadFlag(uiCode, "vps_general_hrd_params_present_flag");  pcVPS->setVPSGeneralHrdParamsPresentFlag(uiCode);
  }
  if (pcVPS->getVPSGeneralHrdParamsPresentFlag())
  {
    parseGeneralHrdParameters(pcVPS->getGeneralHrdParameters());
    if ((pcVPS->getMaxSubLayers() - 1) > 0)
    {
      xReadFlag(uiCode, "vps_sublayer_cpb_params_present_flag");  pcVPS->setVPSSublayerCpbParamsPresentFlag(uiCode);
    }
    else
    {
      pcVPS->setVPSSublayerCpbParamsPresentFlag(0);
    }
    xReadUvlc(uiCode, "vps_num_ols_timing_hrd_params_minus1"); pcVPS->setNumOlsTimingHrdParamsMinus1(uiCode);
    CHECK( uiCode >= pcVPS->getNumMultiLayeredOlss(),"The value of vps_num_ols_timing_hrd_params_minus1 shall be in the range of 0 to NumMultiLayerOlss - 1, inclusive");
    std::vector<bool> isHRDParamReferred( uiCode + 1, false);
    pcVPS->m_olsHrdParams.clear();
    pcVPS->m_olsHrdParams.resize(pcVPS->getNumOlsTimingHrdParamsMinus1() + 1, std::vector<OlsHrdParams>(pcVPS->getMaxSubLayers()));
    for (int i = 0; i <= pcVPS->getNumOlsTimingHrdParamsMinus1(); i++)
    {
      if (!pcVPS->getDefaultPtlDpbHrdMaxTidFlag())
      {
        xReadCode(3, uiCode, "vps_hrd_max_tid[i]");  pcVPS->setHrdMaxTid(i, uiCode);
        CHECK (uiCode > (pcVPS->getMaxSubLayers() - 1), "The value of vps_hrd_max_tid[i] shall be in the range of 0 to vps_max_sublayers_minus1, inclusive." )
      }
      else
      {
        pcVPS->setHrdMaxTid(i, pcVPS->getMaxSubLayers() - 1);
      }
      uint32_t firstSublayer = pcVPS->getVPSSublayerCpbParamsPresentFlag() ? 0 : pcVPS->getHrdMaxTid(i);
      parseOlsHrdParameters(pcVPS->getGeneralHrdParameters(),pcVPS->getOlsHrdParameters(i), firstSublayer, pcVPS->getHrdMaxTid(i));
    }
    for (int i = pcVPS->getNumOlsTimingHrdParamsMinus1() + 1; i < pcVPS->getTotalNumOLSs(); i++)
    {
      pcVPS->setHrdMaxTid(i, pcVPS->getMaxSubLayers() - 1);
    }
    for (int i = 0; i < pcVPS->m_numMultiLayeredOlss; i++)
    {
      if (((pcVPS->getNumOlsTimingHrdParamsMinus1() + 1) != pcVPS->m_numMultiLayeredOlss) && (pcVPS->getNumOlsTimingHrdParamsMinus1() > 0))
      {
        xReadUvlc(uiCode, "vps_ols_timing_hrd_idx[i]"); pcVPS->setOlsTimingHrdIdx(i, uiCode);
        CHECK(uiCode > pcVPS->getNumOlsTimingHrdParamsMinus1(), "The value of vps_ols_timing_hrd_idx[[ i ] shall be in the range of 0 to vps_num_ols_timing_hrd_params_minus1, inclusive.");
      }
      else if (pcVPS->getNumOlsTimingHrdParamsMinus1() == 0)
      {
        pcVPS->setOlsTimingHrdIdx(i, 0);
      }
      else
      {
        pcVPS->setOlsTimingHrdIdx(i, i);
      }
      isHRDParamReferred[pcVPS->getOlsTimingHrdIdx(i)] = true;
    }
    for( int i = 0; i <= pcVPS->getNumOlsTimingHrdParamsMinus1(); i++ )
    {
      CHECK( !isHRDParamReferred[i], "Each vps_ols_timing_hrd_parameters( ) syntax structure in the VPS shall be referred to by at least one value of vps_ols_timing_hrd_idx[ i ] for i in the range of 1 to NumMultiLayerOlss - 1, inclusive");
    }
  }
  else
  {
    for (int i = 0; i < pcVPS->getTotalNumOLSs(); i++)
    {
      pcVPS->setHrdMaxTid(i, pcVPS->getMaxSubLayers() - 1);
    }
  }


  xReadFlag(uiCode, "vps_extension_flag");
  if (uiCode)
  {
    while (xMoreRbspData())
    {
      xReadFlag(uiCode, "vps_extension_data_flag");
    }
  }
  pcVPS->checkVPS();
  xReadRbspTrailingBits();
}

void HLSyntaxReader::parsePictureHeader( PicHeader* picHeader, ParameterSetManager *parameterSetManager, bool readRbspTrailingBits )
{
  uint32_t  uiCode;
  int       iCode;

  PPS *pps = nullptr;
  SPS *sps = nullptr;

#if ENABLE_TRACING
  xTracePictureHeader();
#endif

  xReadFlag(uiCode, "ph_gdr_or_irap_pic_flag");               picHeader->setGdrOrIrapPicFlag(uiCode != 0);
  xReadFlag(uiCode, "ph_non_ref_pic_flag");                picHeader->setNonReferencePictureFlag(uiCode != 0);
  if (picHeader->getGdrOrIrapPicFlag())
  {
    xReadFlag(uiCode, "ph_gdr_pic_flag");                     picHeader->setGdrPicFlag(uiCode != 0);
  }
  else
  {
    picHeader->setGdrPicFlag(false);
  }
  xReadFlag(uiCode, "ph_inter_slice_allowed_flag");       picHeader->setPicInterSliceAllowedFlag(uiCode != 0);
  if (picHeader->getPicInterSliceAllowedFlag())
  {
    xReadFlag(uiCode, "ph_intra_slice_allowed_flag");       picHeader->setPicIntraSliceAllowedFlag(uiCode != 0);
  }
  else
  {
    picHeader->setPicIntraSliceAllowedFlag(true);
  }
  CHECK(picHeader->getPicInterSliceAllowedFlag() == 0 && picHeader->getPicIntraSliceAllowedFlag() == 0, "Invalid picture without intra or inter slice");
  // parameter sets
  xReadUvlc(uiCode, "ph_pic_parameter_set_id");
  picHeader->setPPSId(uiCode);
  pps = parameterSetManager->getPPS(picHeader->getPPSId());
  CHECK(pps == nullptr, "Invalid PPS");
  picHeader->setSPSId(pps->getSPSId());
  sps = parameterSetManager->getSPS(picHeader->getSPSId());
  CHECK(sps == nullptr, "Invalid SPS");
  xReadCode(sps->getBitsForPOC(), uiCode, "ph_pic_order_cnt_lsb");
  picHeader->setPocLsb(uiCode);
  if( picHeader->getGdrPicFlag() )
  {
    xReadUvlc(uiCode, "ph_recovery_poc_cnt");               picHeader->setRecoveryPocCnt( uiCode );
  }
  else
  {
    picHeader->setRecoveryPocCnt( -1 );
  }

  bool isIrapOrGdrWRecoveryPocCnt0 = (picHeader->getGdrOrIrapPicFlag() && !picHeader->getGdrPicFlag()) ||
                                     (picHeader->getGdrPicFlag() && picHeader->getRecoveryPocCnt() == 0);

  if (!isIrapOrGdrWRecoveryPocCnt0)
  {
    const Profile::Name profile = sps->getProfileTierLevel()->getProfileIdc();
    bool isIntraProfile = profile == Profile::MAIN_12_INTRA || profile == Profile::MAIN_12_444_INTRA ||
                          profile == Profile::MAIN_16_444_INTRA;

    CHECK(isIntraProfile && !isIrapOrGdrWRecoveryPocCnt0,
          "Invalid non-irap pictures or gdr pictures with ph_recovery_poc_cnt!=0 for Intra profile");
    CHECK(sps->getProfileTierLevel()->getConstraintInfo()->getAllRapPicturesFlag() == 1 && !isIrapOrGdrWRecoveryPocCnt0,
          "gci_all_rap_pictures_flag equal to 1 specifies that all pictures in OlsInScope are IRAP pictures or GDR pictures with ph_recovery_poc_cnt equal to 0");
  }

  std::vector<bool> phExtraBitsPresent = sps->getExtraPHBitPresentFlags();
  for (int i=0; i< sps->getNumExtraPHBytes() * 8; i++)
  {
    // extra bits are ignored (when present)
    if (phExtraBitsPresent[i])
    {
      xReadFlag(uiCode, "ph_extra_bit[ i ]");
    }
  }

  if (sps->getPocMsbCycleFlag())
  {
    xReadFlag(uiCode, "ph_poc_msb_present_flag"); picHeader->setPocMsbPresentFlag(uiCode != 0);
    if (picHeader->getPocMsbPresentFlag())
    {
      xReadCode(sps->getPocMsbCycleLen(), uiCode, "ph_poc_msb_cycle_val");
      picHeader->setPocMsbVal(uiCode);
    }
  }


  // alf enable flags and aps IDs
  picHeader->setCcAlfEnabledFlag(COMPONENT_Cb, false);
  picHeader->setCcAlfEnabledFlag(COMPONENT_Cr, false);
  if (sps->getALFEnabledFlag())
  {
    if (pps->getAlfInfoInPhFlag())
    {
      xReadFlag(uiCode, "ph_alf_enabled_flag");
      const bool alfEnabledFlag = uiCode != 0;
      picHeader->setAlfEnabledFlag(COMPONENT_Y, alfEnabledFlag);

      bool alfCbEnabledFlag = false;
      bool alfCrEnabledFlag = false;

      AlfApsList apsIds;
      if (alfEnabledFlag)
      {
        xReadCode(3, uiCode, "ph_num_alf_aps_ids_luma");
        const int numAps = uiCode;

        for (int i = 0; i < numAps; i++)
        {
          xReadCode(3, uiCode, "ph_alf_aps_id_luma");
          const int apsId = uiCode;

          apsIds.push_back(apsId);

          APS *apsToCheckLuma = parameterSetManager->getAPS(apsId, ApsType::ALF);
          CHECK(apsToCheckLuma == nullptr, "referenced APS not found");
          CHECK(apsToCheckLuma->getAlfAPSParam().newFilterFlag[ChannelType::LUMA] != 1,
                "bitstream conformance error, alf_luma_filter_signal_flag shall be equal to 1");
        }

        if (isChromaEnabled(sps->getChromaFormatIdc()))
        {
          xReadCode(1, uiCode, "ph_alf_cb_enabled_flag");
          alfCbEnabledFlag = uiCode != 0;
          xReadCode(1, uiCode, "ph_alf_cr_enabled_flag");
          alfCrEnabledFlag = uiCode != 0;
        }

        if (alfCbEnabledFlag || alfCrEnabledFlag)
        {
          xReadCode(3, uiCode, "ph_alf_aps_id_chroma");
          picHeader->setAlfApsIdChroma(uiCode);
          APS *apsToCheckChroma = parameterSetManager->getAPS(uiCode, ApsType::ALF);
          CHECK(apsToCheckChroma == nullptr, "referenced APS not found");
          CHECK(apsToCheckChroma->getAlfAPSParam().newFilterFlag[ChannelType::CHROMA] != 1,
                "bitstream conformance error, alf_chroma_filter_signal_flag shall be equal to 1");
        }
        if (sps->getCCALFEnabledFlag())
        {
          xReadFlag(uiCode, "ph_cc_alf_cb_enabled_flag");
          picHeader->setCcAlfEnabledFlag(COMPONENT_Cb, uiCode != 0);
          picHeader->setCcAlfCbApsId(-1);
          if (picHeader->getCcAlfEnabledFlag(COMPONENT_Cb))
          {
            // parse APS ID
            xReadCode(3, uiCode, "ph_cc_alf_cb_aps_id");
            picHeader->setCcAlfCbApsId(uiCode);
            APS *apsToCheckCcCb = parameterSetManager->getAPS(uiCode, ApsType::ALF);
            CHECK(apsToCheckCcCb == nullptr, "referenced APS not found");
            CHECK(apsToCheckCcCb->getCcAlfAPSParam().newCcAlfFilter[COMPONENT_Cb - 1] != 1, "bitstream conformance error, alf_cc_cb_filter_signal_flag shall be equal to 1");
          }
          // Cr
          xReadFlag(uiCode, "ph_cc_alf_cr_enabled_flag");
          picHeader->setCcAlfEnabledFlag(COMPONENT_Cr, uiCode != 0);
          picHeader->setCcAlfCrApsId(-1);
          if (picHeader->getCcAlfEnabledFlag(COMPONENT_Cr))
          {
            // parse APS ID
            xReadCode(3, uiCode, "ph_cc_alf_cr_aps_id");
            picHeader->setCcAlfCrApsId(uiCode);
            APS *apsToCheckCcCr = parameterSetManager->getAPS(uiCode, ApsType::ALF);
            CHECK(apsToCheckCcCr == nullptr, "referenced APS not found");
            CHECK(apsToCheckCcCr->getCcAlfAPSParam().newCcAlfFilter[COMPONENT_Cr - 1] != 1, "bitstream conformance error, alf_cc_cr_filter_signal_flag shall be equal to 1");
          }
        }
      }

      picHeader->setNumAlfApsIdsLuma((int) apsIds.size());
      picHeader->setAlfApsIdsLuma(apsIds);
      picHeader->setAlfEnabledFlag(COMPONENT_Cb, alfCbEnabledFlag);
      picHeader->setAlfEnabledFlag(COMPONENT_Cr, alfCrEnabledFlag);
    }
    else
    {
      picHeader->setAlfEnabledFlag(COMPONENT_Y, true);
      picHeader->setAlfEnabledFlag(COMPONENT_Cb, true);
      picHeader->setAlfEnabledFlag(COMPONENT_Cr, true);
    }
  }
  else
  {
    picHeader->setAlfEnabledFlag(COMPONENT_Y, false);
    picHeader->setAlfEnabledFlag(COMPONENT_Cb, false);
    picHeader->setAlfEnabledFlag(COMPONENT_Cr, false);
  }
  // luma mapping / chroma scaling controls
  if (sps->getUseLmcs())
  {
    xReadFlag(uiCode, "ph_lmcs_enabled_flag");
    picHeader->setLmcsEnabledFlag(uiCode != 0);

    if (picHeader->getLmcsEnabledFlag())
    {
      xReadCode(2, uiCode, "ph_lmcs_aps_id");
      picHeader->setLmcsAPSId(uiCode);

      if (isChromaEnabled(sps->getChromaFormatIdc()))
      {
        xReadFlag(uiCode, "ph_chroma_residual_scale_flag");
        picHeader->setLmcsChromaResidualScaleFlag(uiCode != 0);
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
  if (sps->getScalingListFlag())
  {
    xReadFlag(uiCode, "ph_explicit_scaling_list_enabled_flag");
    picHeader->setExplicitScalingListEnabledFlag(uiCode);
    if (picHeader->getExplicitScalingListEnabledFlag())
    {
      xReadCode(3, uiCode, "ph_scaling_list_aps_id");
      picHeader->setScalingListAPSId(uiCode);
    }
  }
  else
  {
    picHeader->setExplicitScalingListEnabledFlag(false);
  }
  if (pps->getPicWidthInLumaSamples() == sps->getMaxPicWidthInLumaSamples() && pps->getPicHeightInLumaSamples() == sps->getMaxPicHeightInLumaSamples())
  {
    CHECK(pps->getConformanceWindowFlag(), "When pps_pic_width_in_luma_samples is equal to sps_pic_width_max_in_luma_samples and pps_pic_height_in_luma_samples is equal to sps_pic_height_max_in_luma_samples, the value of pps_conformance_window_flag shall be equal to 0");
    pps->getConformanceWindow().setWindowLeftOffset(sps->getConformanceWindow().getWindowLeftOffset());
    pps->getConformanceWindow().setWindowRightOffset(sps->getConformanceWindow().getWindowRightOffset());
    pps->getConformanceWindow().setWindowTopOffset(sps->getConformanceWindow().getWindowTopOffset());
    pps->getConformanceWindow().setWindowBottomOffset(sps->getConformanceWindow().getWindowBottomOffset());
    if (!pps->getExplicitScalingWindowFlag())
    {
      pps->setScalingWindow(pps->getConformanceWindow());
    }
  }
  CHECK(!sps->getRprEnabledFlag() && pps->getExplicitScalingWindowFlag(), "When sps_ref_pic_resampling_enabled_flag is equal to 0, the value of pps_scaling_window_explicit_signalling_flag shall be equal to 0");

  // initialize tile/slice info for no partitioning case

  if( pps->getNoPicPartitionFlag() )
  {
    pps->resetTileSliceInfo();
    pps->setLog2CtuSize( ceilLog2(sps->getCTUSize()) );
    pps->setNumExpTileColumns(1);
    pps->setNumExpTileRows(1);
    pps->addTileColumnWidth( pps->getPicWidthInCtu( ) );
    pps->addTileRowHeight( pps->getPicHeightInCtu( ) );
    pps->initTiles();
    pps->setRectSliceFlag( 1 );
    pps->setNumSlicesInPic( 1 );
    pps->initRectSlices( );
    pps->setTileIdxDeltaPresentFlag( 0 );
    pps->setSliceTileIdx( 0, 0 );
    pps->initRectSliceMap(sps);
    // when no Pic partition, number of sub picture shall be less than 2
    CHECK(pps->getNumSubPics()>=2, "error, no picture partitions, but have equal to or more than 2 sub pictures");
  }
  else
  {
    CHECK(pps->getCtuSize() != sps->getCTUSize(), "PPS CTU size does not match CTU size in SPS");
    if (pps->getRectSliceFlag())
    {
      pps->initRectSliceMap(sps);
    }
  }

  pps->initSubPic(*sps);

  // set wraparound offset from PPS and SPS info
  int minCbSizeY = (1 << sps->getLog2MinCodingBlockSize());
  CHECK( !sps->getWrapAroundEnabledFlag() && pps->getWrapAroundEnabledFlag(), "When sps_ref_wraparound_enabled_flag is equal to 0, the value of pps_ref_wraparound_enabled_flag shall be equal to 0.");
  CHECK( (((sps->getCTUSize() / minCbSizeY) + 1) > ((pps->getPicWidthInLumaSamples() / minCbSizeY) - 1)) && pps->getWrapAroundEnabledFlag(), "When the value of CtbSizeY / MinCbSizeY + 1 is greater than pps_pic_width_in_luma_samples / MinCbSizeY - 1, the value of pps_ref_wraparound_enabled_flag shall be equal to 0.");
  if( pps->getWrapAroundEnabledFlag() )
  {
    CHECK((pps->getPicWidthMinusWrapAroundOffset() > (pps->getPicWidthInLumaSamples() / minCbSizeY - sps->getCTUSize() / minCbSizeY - 2)), "pps_pic_width_minus_wraparound_ofsfet shall be less than or equal to pps_pic_width_in_luma_samples/MinCbSizeY - CtbSizeY/MinCbSizeY-2");
    pps->setWrapAroundOffset(minCbSizeY * (pps->getPicWidthInLumaSamples()/minCbSizeY- pps->getPicWidthMinusWrapAroundOffset()));
  }
  else
  {
    pps->setWrapAroundOffset( 0 );
  }

  // virtual boundaries
  if( sps->getVirtualBoundariesEnabledFlag() && !sps->getVirtualBoundariesPresentFlag() )
  {
    xReadFlag( uiCode, "ph_virtual_boundaries_present_flag" );
    picHeader->setVirtualBoundariesPresentFlag( uiCode != 0 );
    if( picHeader->getVirtualBoundariesPresentFlag() )
    {
      xReadUvlc(uiCode, "ph_num_ver_virtual_boundaries");        picHeader->setNumVerVirtualBoundaries( uiCode );
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
        xReadUvlc(uiCode, "ph_virtual_boundary_pos_x_minus1[i]");        picHeader->setVirtualBoundariesPosX((uiCode + 1) << 3, i);
        CHECK(uiCode > (((pps->getPicWidthInLumaSamples() + 7) >> 3) - 2), "The value of ph_virtual_boundary_pos_x_minus1[ i ] shall be in the range of 0 to Ceil( pps_pic_width_in_luma_samples / 8 ) - 2, inclusive.");
      }
#if GDR_DEC_TRACE
      printf("\n");
      printf("-num_ver_boundary :%d\n", picHeader->getNumVerVirtualBoundaries());
      printf("-vir_boundary_pos :%d\n", picHeader->getVirtualBoundariesPosX(0));
#endif
      xReadUvlc(uiCode, "ph_num_hor_virtual_boundaries");        picHeader->setNumHorVirtualBoundaries( uiCode );
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
        xReadUvlc(uiCode, "ph_virtual_boundary_pos_y_minus1[i]");        picHeader->setVirtualBoundariesPosY((uiCode + 1) << 3, i);
        CHECK(uiCode > (((pps->getPicHeightInLumaSamples() + 7) >> 3) - 2), "The value of ph_virtual_boundary_pos_y_minus1[ i ] shall be in the range of 0 to Ceil( pps_pic_height_in_luma_samples / 8 ) - 2, inclusive.");
      }
    }
    else
    {
      picHeader->setNumVerVirtualBoundaries( 0 );
      picHeader->setNumHorVirtualBoundaries( 0 );
    }
  }
  else
  {
    picHeader->setVirtualBoundariesPresentFlag( sps->getVirtualBoundariesPresentFlag() );
    if( picHeader->getVirtualBoundariesPresentFlag() )
    {
      picHeader->setNumVerVirtualBoundaries(sps->getNumVerVirtualBoundaries());
      picHeader->setNumHorVirtualBoundaries(sps->getNumHorVirtualBoundaries());
      for (unsigned i = 0; i < 3; i++)
      {
        picHeader->setVirtualBoundariesPosX(sps->getVirtualBoundariesPosX(i), i);
        picHeader->setVirtualBoundariesPosY(sps->getVirtualBoundariesPosY(i), i);
      }
    }
  }


  // picture output flag
  if (pps->getOutputFlagPresentFlag() && !picHeader->getNonReferencePictureFlag())
  {
    xReadFlag( uiCode, "ph_pic_output_flag" ); picHeader->setPicOutputFlag( uiCode != 0 );
  }
  else
  {
    picHeader->setPicOutputFlag( true );
  }

  // reference picture lists
  if (pps->getRplInfoInPhFlag())
  {
    bool rplSpsFlag = false;

    for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
    {
      int numRplsInSps = sps->getNumRpl(l);
      if (numRplsInSps == 0)
      {
        rplSpsFlag = false;
      }
      else if (l == REF_PIC_LIST_0 || pps->getRpl1IdxPresentFlag())
      {
        xReadFlag(uiCode, "rpl_sps_flag[i]");
        rplSpsFlag = uiCode != 0;
      }

      ReferencePictureList *rpl = picHeader->getRpl(l);
      if (!rplSpsFlag)
      {
        // explicit RPL in picture header
        *rpl = ReferencePictureList();
        parseRefPicList(sps, rpl, -1);
        picHeader->setRplIdx(l, -1);
      }
      else
      {
        // use list from SPS
        int rplIdx = 0;

        if (numRplsInSps > 1 && (l == REF_PIC_LIST_0 || pps->getRpl1IdxPresentFlag()))
        {
          int numBits = ceilLog2(numRplsInSps);
          xReadCode(numBits, uiCode, "rpl_idx[i]");
          rplIdx = uiCode;
        }
        else if(numRplsInSps != 1)
        {
          rplIdx = picHeader->getRplIdx(REF_PIC_LIST_0);
          CHECK(rplIdx == -1, "There should be a list 0 RPL");
        }

        picHeader->setRplIdx(l, rplIdx);
        *rpl = *sps->getRplList(l)->getReferencePictureList(rplIdx);
      }
      if (picHeader->getPicInterSliceAllowedFlag() && l == REF_PIC_LIST_0)
      {
        CHECK(picHeader->getRpl(REF_PIC_LIST_0)->getNumRefEntries() <= 0,
              "When pps_rpl_info_in_ph_flag is equal to 1 and ph_inter_slice_allowed_flag is equal to 1, the value of "
              "num_ref_entries[ 0 ][ RplsIdx[ 0 ] ] shall be greater than 0");
      }
      // POC MSB cycle signalling for LTRP
      for (int i = 0; i < rpl->getNumRefEntries(); i++)
      {
        rpl->setDeltaPocMSBPresentFlag(i, false);
        rpl->setDeltaPocMSBCycleLT(i, 0);
      }
      if (rpl->getNumberOfLongtermPictures())
      {
        for (int i = 0; i < rpl->getNumRefEntries(); i++)
        {
          if (rpl->isRefPicLongterm(i) && !rpl->isInterLayerRefPic(i))
          {
            if (rpl->getLtrpInSliceHeaderFlag())
            {
              xReadCode(sps->getBitsForPOC(), uiCode, "poc_lsb_lt[i][j]");
              rpl->setRefPicIdentifier( i, uiCode, true, false, 0 );
            }
            xReadFlag(uiCode, "delta_poc_msb_present_flag[i][j]");
            rpl->setDeltaPocMSBPresentFlag(i, uiCode ? true : false);
            if (uiCode)
            {
              xReadUvlc(uiCode, "delta_poc_msb_cycle_lt[i][j]");
              if(i != 0)
              {
                uiCode += rpl->getDeltaPocMSBCycleLT(i-1);
              }
              rpl->setDeltaPocMSBCycleLT(i, uiCode);
            }
            else if(i != 0)
            {
              rpl->setDeltaPocMSBCycleLT(i, rpl->getDeltaPocMSBCycleLT(i-1));
            }
            else
            {
              rpl->setDeltaPocMSBCycleLT(i,0);
            }
          }
          else if(i != 0)
          {
            rpl->setDeltaPocMSBCycleLT(i, rpl->getDeltaPocMSBCycleLT(i-1));
          }
          else
          {
            rpl->setDeltaPocMSBCycleLT(i,0);
          }
        }
      }
    }
  }

  // partitioning constraint overrides
  if (sps->getSplitConsOverrideEnabledFlag())
  {
    xReadFlag(uiCode, "ph_partition_constraints_override_flag");  picHeader->setSplitConsOverrideFlag( uiCode != 0 );
  }
  else
  {
    picHeader->setSplitConsOverrideFlag(0);
  }
  // Q0781, two-flags
  unsigned  minQT[3] = { 0, 0, 0 };
  unsigned  maxBTD[3] = { 0, 0, 0 };
  unsigned  maxBTSize[3] = { 0, 0, 0 };
  unsigned  maxTTSize[3] = { 0, 0, 0 };
  unsigned  ctbLog2SizeY = floorLog2(sps->getCTUSize());

  if (picHeader->getPicIntraSliceAllowedFlag())
  {
    if (picHeader->getSplitConsOverrideFlag())
    {
      xReadUvlc(uiCode, "ph_log2_diff_min_qt_min_cb_intra_slice_luma");
      unsigned minQtLog2SizeIntraY = uiCode + sps->getLog2MinCodingBlockSize();
      minQT[0] = 1 << minQtLog2SizeIntraY;
      CHECK(minQT[0] > 64, "The value of ph_log2_diff_min_qt_min_cb_intra_slice_luma shall be in the range of 0 to min(6,CtbLog2SizeY) - MinCbLog2Size");
      xReadUvlc(uiCode, "ph_max_mtt_hierarchy_depth_intra_slice_luma");         maxBTD[0] = uiCode;

      maxTTSize[0] = maxBTSize[0] = minQT[0];
      if (maxBTD[0] != 0)
      {
        xReadUvlc(uiCode, "ph_log2_diff_max_bt_min_qt_intra_slice_luma");       maxBTSize[0] <<= uiCode;
        CHECK(uiCode > ctbLog2SizeY - minQtLog2SizeIntraY, "The value of ph_log2_diff_max_bt_min_qt_intra_slice_luma shall be in the range of 0 to CtbLog2SizeY - MinQtLog2SizeIntraY");
        xReadUvlc(uiCode, "ph_log2_diff_max_tt_min_qt_intra_slice_luma");       maxTTSize[0] <<= uiCode;
        CHECK(uiCode > ctbLog2SizeY - minQtLog2SizeIntraY, "The value of ph_log2_diff_max_tt_min_qt_intra_slice_luma shall be in the range of 0 to CtbLog2SizeY - MinQtLog2SizeIntraY");
        CHECK(maxTTSize[0] > 64, "The value of ph_log2_diff_max_tt_min_qt_intra_slice_luma shall be in the range of 0 to min(6,CtbLog2SizeY) - MinQtLog2SizeIntraY");
      }

      if (sps->getUseDualITree())
      {
        xReadUvlc(uiCode, "ph_log2_diff_min_qt_min_cb_intra_slice_chroma");     minQT[2] = 1 << (uiCode + sps->getLog2MinCodingBlockSize());
        CHECK(minQT[2] > 64, "The value of ph_log2_diff_min_qt_min_cb_intra_slice_chroma shall be in the range of 0 to min(6,CtbLog2SizeY) - MinCbLog2Size");
        xReadUvlc(uiCode, "ph_max_mtt_hierarchy_depth_intra_slice_chroma");     maxBTD[2] = uiCode;
        maxTTSize[2] = maxBTSize[2] = minQT[2];
        if (maxBTD[2] != 0)
        {
          xReadUvlc(uiCode, "ph_log2_diff_max_bt_min_qt_intra_slice_chroma");   maxBTSize[2] <<= uiCode;
          xReadUvlc(uiCode, "ph_log2_diff_max_tt_min_qt_intra_slice_chroma");   maxTTSize[2] <<= uiCode;
          CHECK(maxBTSize[2] > 64, "The value of ph_log2_diff_max_bt_min_qt_intra_slice_chroma shall be in the range of 0 to min(6,CtbLog2SizeY) - MinQtLog2SizeIntraChroma");
          CHECK(maxTTSize[2] > 64, "The value of ph_log2_diff_max_tt_min_qt_intra_slice_chroma shall be in the range of 0 to min(6,CtbLog2SizeY) - MinQtLog2SizeIntraChroma");
        }
      }
    }
  }


  if (picHeader->getPicIntraSliceAllowedFlag())
  {
  // delta quantization and chrom and chroma offset
    if (pps->getUseDQP())
    {
      xReadUvlc( uiCode, "ph_cu_qp_delta_subdiv_intra_slice" );   picHeader->setCuQpDeltaSubdivIntra( uiCode );
    }
    else
    {
      picHeader->setCuQpDeltaSubdivIntra( 0 );
    }
    if (pps->getCuChromaQpOffsetListEnabledFlag())
    {
      xReadUvlc( uiCode, "ph_cu_chroma_qp_offset_subdiv_intra_slice" );   picHeader->setCuChromaQpOffsetSubdivIntra( uiCode );
    }
    else
    {
      picHeader->setCuChromaQpOffsetSubdivIntra( 0 );
    }
  }


  if (picHeader->getPicInterSliceAllowedFlag())
  {
    if (picHeader->getSplitConsOverrideFlag())
    {
      xReadUvlc(uiCode, "ph_log2_diff_min_qt_min_cb_inter_slice");
      unsigned minQtLog2SizeInterY = uiCode + sps->getLog2MinCodingBlockSize();
      minQT[1] = 1 << minQtLog2SizeInterY;
      CHECK(minQT[1] > 64, "The value of ph_log2_diff_min_qt_min_cb_inter_slice shall be in the range of 0 to min(6, CtbLog2SizeY) - MinCbLog2SizeY.");
      CHECK(minQT[1] > (1<<ctbLog2SizeY), "The value of ph_log2_diff_min_qt_min_cb_inter_slice shall be in the range of 0 to min(6, CtbLog2SizeY) - MinCbLog2SizeY");
      xReadUvlc(uiCode, "ph_max_mtt_hierarchy_depth_inter_slice");              maxBTD[1] = uiCode;

      maxTTSize[1] = maxBTSize[1] = minQT[1];
      if (maxBTD[1] != 0)
      {
        xReadUvlc(uiCode, "ph_log2_diff_max_bt_min_qt_inter_slice");            maxBTSize[1] <<= uiCode;
        CHECK(uiCode > ctbLog2SizeY - minQtLog2SizeInterY, "The value of ph_log2_diff_max_bt_min_qt_inter_slice shall be in the range of 0 to CtbLog2SizeY - MinQtLog2SizeInterY");
        xReadUvlc(uiCode, "ph_log2_diff_max_tt_min_qt_inter_slice");            maxTTSize[1] <<= uiCode;
        CHECK(uiCode > ctbLog2SizeY - minQtLog2SizeInterY, "The value of ph_log2_diff_max_tt_min_qt_inter_slice shall be in the range of 0 to CtbLog2SizeY - MinQtLog2SizeInterY");
        CHECK(maxTTSize[1] > 64, "The value of ph_log2_diff_max_tt_min_qt_inter_slice shall be in the range of 0 to min(6,CtbLog2SizeY) - MinQtLog2SizeInterY.");
      }
    }
    // delta quantization and chrom and chroma offset
    if (pps->getUseDQP())
    {
      xReadUvlc(uiCode, "ph_cu_qp_delta_subdiv_inter_slice");   picHeader->setCuQpDeltaSubdivInter(uiCode);
    }
    else
    {
      picHeader->setCuQpDeltaSubdivInter(0);
    }
    if (pps->getCuChromaQpOffsetListEnabledFlag())
    {
      xReadUvlc(uiCode, "ph_cu_chroma_qp_offset_subdiv_inter_slice");   picHeader->setCuChromaQpOffsetSubdivInter(uiCode);
    }
    else
    {
      picHeader->setCuChromaQpOffsetSubdivInter(0);
    }

  // temporal motion vector prediction
    if (sps->getSPSTemporalMVPEnabledFlag())
    {
      xReadFlag( uiCode, "ph_temporal_mvp_enabled_flag" );
      picHeader->setEnableTMVPFlag( uiCode != 0 );
    }
    else
    {
      picHeader->setEnableTMVPFlag(false);
    }

    if (picHeader->getEnableTMVPFlag() && pps->getRplInfoInPhFlag())
    {
      if (picHeader->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 0)
      {
        xReadCode(1, uiCode, "ph_collocated_from_l0_flag");
        picHeader->setPicColFromL0Flag(uiCode);
      }
      else
      {
        picHeader->setPicColFromL0Flag(true);
      }
      if (picHeader->getRpl(picHeader->getPicColFromL0Flag() ? REF_PIC_LIST_0 : REF_PIC_LIST_1)->getNumRefEntries() > 1)
      {
        xReadUvlc(uiCode, "ph_collocated_ref_idx");
        picHeader->setColRefIdx(uiCode);
      }
      else
      {
        picHeader->setColRefIdx(0);
      }
    }
    else
    {
      picHeader->setPicColFromL0Flag(false);
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
      xReadFlag( uiCode, "ph_fpel_mmvd_enabled_flag" );
      picHeader->setDisFracMMVD( uiCode != 0 );
    }
    else
    {
      picHeader->setDisFracMMVD(false);
    }

    // mvd L1 zero flag
    if (!pps->getRplInfoInPhFlag() || picHeader->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 0)
    {
      xReadFlag(uiCode, "ph_mvd_l1_zero_flag");
    }
    else
    {
      uiCode = 1;
    }
    picHeader->setMvdL1ZeroFlag(uiCode != 0);

    // picture level BDOF disable flags
    if (sps->getBdofControlPresentInPhFlag()
        && (!pps->getRplInfoInPhFlag() || picHeader->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 0))
    {
      xReadFlag(uiCode, "ph_bdof_disabled_flag");  picHeader->setBdofDisabledFlag(uiCode != 0);
    }
    else
    {
      if (!sps->getBdofControlPresentInPhFlag())
      {
        picHeader->setBdofDisabledFlag(!sps->getBDOFEnabledFlag());
      }
      else
      {
        picHeader->setBdofDisabledFlag(true);
      }
    }

  // picture level DMVR disable flags
    if (sps->getDmvrControlPresentInPhFlag()
        && (!pps->getRplInfoInPhFlag() || picHeader->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 0))
    {
      xReadFlag(uiCode, "ph_dmvr_disabled_flag");  picHeader->setDmvrDisabledFlag(uiCode != 0);
    }
    else
    {
      if (!sps->getDmvrControlPresentInPhFlag())
      {
        picHeader->setDmvrDisabledFlag(!sps->getUseDMVR());
      }
      else
      {
        picHeader->setDmvrDisabledFlag(true);
      }
    }

  // picture level PROF disable flags
    if (sps->getProfControlPresentInPhFlag())
    {
      xReadFlag(uiCode, "ph_prof_disabled_flag");  picHeader->setProfDisabledFlag(uiCode != 0);
    }
    else
    {
      picHeader->setProfDisabledFlag(!sps->getUsePROF());
    }

    if( (pps->getUseWP() || pps->getWPBiPred()) && pps->getWpInfoInPhFlag() )
    {
      parsePredWeightTable(picHeader, pps, sps);
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
  else
  {
    picHeader->setMinQTSizes(minQT);
    picHeader->setMaxMTTHierarchyDepths(maxBTD);
    picHeader->setMaxBTSizes(maxBTSize);
    picHeader->setMaxTTSizes(maxTTSize);
  }
  // ibc merge candidate list size
  if (pps->getQpDeltaInfoInPhFlag())
  {
    int iCode = 0;
    xReadSvlc(iCode, "ph_qp_delta");
    picHeader->setQpDelta(iCode);
  }

  // joint Cb/Cr sign flag
  if (sps->getJointCbCrEnabledFlag())
  {
    xReadFlag( uiCode, "ph_joint_cbcr_sign_flag" );
    picHeader->setJointCbCrSignFlag(uiCode != 0);
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
      xReadFlag(uiCode, "ph_sao_luma_enabled_flag");
      picHeader->setSaoEnabledFlag(ChannelType::LUMA, uiCode != 0);

      if (isChromaEnabled(sps->getChromaFormatIdc()))
      {
        xReadFlag(uiCode, "ph_sao_chroma_enabled_flag");
        picHeader->setSaoEnabledFlag(ChannelType::CHROMA, uiCode != 0);
      }
    }
    else
    {
      picHeader->setSaoEnabledFlag(ChannelType::LUMA, true);
      picHeader->setSaoEnabledFlag(ChannelType::CHROMA, isChromaEnabled(sps->getChromaFormatIdc()));
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
      xReadFlag( uiCode, "ph_deblocking_params_present_flag" );
      picHeader->setDeblockingFilterOverrideFlag( uiCode != 0 );
    }
    else
    {
      picHeader->setDeblockingFilterOverrideFlag(false);
    }

    if(picHeader->getDeblockingFilterOverrideFlag())
    {
      if (!pps->getPPSDeblockingFilterDisabledFlag())
      {
        xReadFlag(uiCode, "ph_deblocking_filter_disabled_flag");
        picHeader->setDeblockingFilterDisable(uiCode != 0);
      }
      else
      {
        picHeader->setDeblockingFilterDisable(false);
      }
      if (!picHeader->getDeblockingFilterDisable())
      {
        xReadSvlc( iCode, "ph_beta_offset_div2" );
        picHeader->setDeblockingFilterBetaOffsetDiv2(iCode);
        CHECK(  picHeader->getDeblockingFilterBetaOffsetDiv2() < -12 ||
                picHeader->getDeblockingFilterBetaOffsetDiv2() > 12, "Invalid deblocking filter configuration");

        xReadSvlc( iCode, "ph_tc_offset_div2" );
        picHeader->setDeblockingFilterTcOffsetDiv2(iCode);
        CHECK(  picHeader->getDeblockingFilterTcOffsetDiv2() < -12 ||
                picHeader->getDeblockingFilterTcOffsetDiv2() > 12, "Invalid deblocking filter configuration");

        if( pps->getPPSChromaToolFlag() )
        {
          xReadSvlc( iCode, "ph_cb_beta_offset_div2" );
          picHeader->setDeblockingFilterCbBetaOffsetDiv2(iCode);
          CHECK(  picHeader->getDeblockingFilterCbBetaOffsetDiv2() < -12 ||
                  picHeader->getDeblockingFilterCbBetaOffsetDiv2() > 12, "Invalid deblocking filter configuration");

          xReadSvlc( iCode, "ph_cb_tc_offset_div2" );
          picHeader->setDeblockingFilterCbTcOffsetDiv2(iCode);
          CHECK(  picHeader->getDeblockingFilterCbTcOffsetDiv2() < -12 ||
                  picHeader->getDeblockingFilterCbTcOffsetDiv2() > 12, "Invalid deblocking filter configuration");

          xReadSvlc( iCode, "ph_cr_beta_offset_div2" );
          picHeader->setDeblockingFilterCrBetaOffsetDiv2(iCode);
          CHECK(  picHeader->getDeblockingFilterCrBetaOffsetDiv2() < -12 ||
                  picHeader->getDeblockingFilterCrBetaOffsetDiv2() > 12, "Invalid deblocking filter configuration");

          xReadSvlc( iCode, "ph_cr_tc_offset_div2" );
          picHeader->setDeblockingFilterCrTcOffsetDiv2(iCode);
          CHECK(  picHeader->getDeblockingFilterCrTcOffsetDiv2() < -12 ||
                  picHeader->getDeblockingFilterCrTcOffsetDiv2() > 12, "Invalid deblocking filter configuration");
        }
        else
        {
          picHeader->setDeblockingFilterCbBetaOffsetDiv2 ( picHeader->getDeblockingFilterBetaOffsetDiv2() );
          picHeader->setDeblockingFilterCbTcOffsetDiv2   ( picHeader->getDeblockingFilterTcOffsetDiv2()   );
          picHeader->setDeblockingFilterCrBetaOffsetDiv2 ( picHeader->getDeblockingFilterBetaOffsetDiv2() );
          picHeader->setDeblockingFilterCrTcOffsetDiv2   ( picHeader->getDeblockingFilterTcOffsetDiv2()   );
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
    picHeader->setDeblockingFilterCrBetaOffsetDiv2(0);
    picHeader->setDeblockingFilterCrTcOffsetDiv2(0);
  }


  // picture header extension
  if(pps->getPictureHeaderExtensionPresentFlag())
  {
    xReadUvlc(uiCode,"ph_extension_length");
    for(int i=0; i<uiCode; i++)
    {
      uint32_t ignore_;
      xReadCode(8,ignore_,"ph_extension_data_byte");
    }
  }

  if( readRbspTrailingBits )
  {
    xReadRbspTrailingBits();
  }
}

void  HLSyntaxReader::checkAlfNaluTidAndPicTid(Slice* pcSlice, PicHeader* picHeader, ParameterSetManager *parameterSetManager)
{
  SPS* sps = parameterSetManager->getSPS(picHeader->getSPSId());
  PPS* pps = parameterSetManager->getPPS(picHeader->getPPSId());
  VPS* vps = parameterSetManager->getVPS(sps->getVPSId());
  int curPicTid = pcSlice->getTLayer();
  APS* aps;
  const AlfApsList &apsId = picHeader->getAlfApsIdsLuma();

  if (sps->getALFEnabledFlag() && pps->getAlfInfoInPhFlag() && picHeader->getAlfEnabledFlag(COMPONENT_Y))
  {
    //luma
    for (int i = 0; i < picHeader->getNumAlfApsIdsLuma(); i++)
    {
      aps = parameterSetManager->getAPS(apsId[i], ApsType::ALF);
      CHECK(aps->getTemporalId() > curPicTid,
            "The TemporalId of the APS NAL unit having aps_params_type equal to ApsType::ALF and "
            "adaptation_parameter_set_id equal to ph_alf_aps_id_luma[ i ] shall be less than or equal to the "
            "TemporalId of the picture associated with the PH.");
      if( pcSlice->getNalUnitLayerId() != aps->getLayerId() )
      {
        CHECK( aps->getLayerId() > pcSlice->getNalUnitLayerId(), "Layer Id of APS cannot be greater than layer Id of VCL NAL unit the refer to it" );
        CHECK( sps->getVPSId() == 0, "VPSId of the referred SPS cannot be 0 when layer Id of APS and layer Id of current slice are different" );
        for (int i = 0; i < vps->getNumOutputLayerSets(); i++ )
        {
          bool isCurrLayerInOls = false;
          bool isRefLayerInOls = false;
          for( int j = vps->getNumLayersInOls(i) - 1; j >= 0; j-- )
          {
            if( vps->getLayerIdInOls(i, j) == pcSlice->getNalUnitLayerId() )
            {
              isCurrLayerInOls = true;
            }
            if( vps->getLayerIdInOls(i, j) == aps->getLayerId() )
            {
              isRefLayerInOls = true;
            }
          }
          CHECK( isCurrLayerInOls && !isRefLayerInOls, "When VCL NAl unit in layer A refers to APS in layer B, all OLS that contains layer A shall also contains layer B" );
        }
      }
    }
    //chroma
    if (picHeader->getAlfEnabledFlag(COMPONENT_Cb) || picHeader->getAlfEnabledFlag(COMPONENT_Cr))
    {
      int chromaAlfApsId = picHeader->getAlfApsIdChroma();
      aps                = parameterSetManager->getAPS(chromaAlfApsId, ApsType::ALF);
      CHECK(aps->getTemporalId() > curPicTid,
            "The TemporalId of the APS NAL unit having aps_params_type equal to ApsType::ALF and "
            "adaptation_parameter_set_id equal to ph_alf_aps_id_chroma shall be less than or equal to the TemporalId "
            "of the picture associated with the PH.");
      if( pcSlice->getNalUnitLayerId() != aps->getLayerId() )
      {
        CHECK( aps->getLayerId() > pcSlice->getNalUnitLayerId(), "Layer Id of APS cannot be greater than layer Id of VCL NAL unit the refer to it" );
        CHECK( sps->getVPSId() == 0, "VPSId of the referred SPS cannot be 0 when layer Id of APS and layer Id of current slice are different" );
        for (int i = 0; i < vps->getNumOutputLayerSets(); i++ )
        {
          bool isCurrLayerInOls = false;
          bool isRefLayerInOls = false;
          for( int j = vps->getNumLayersInOls(i) - 1; j >= 0; j-- )
          {
            if( vps->getLayerIdInOls(i, j) == pcSlice->getNalUnitLayerId() )
            {
              isCurrLayerInOls = true;
            }
            if( vps->getLayerIdInOls(i, j) == aps->getLayerId() )
            {
              isRefLayerInOls = true;
            }
          }
          CHECK( isCurrLayerInOls && !isRefLayerInOls, "When VCL NAl unit in layer A refers to APS in layer B, all OLS that contains layer A shall also contains layer B" );
        }
      }
    }
  }
}

void HLSyntaxReader::parseSliceHeader (Slice* pcSlice, PicHeader* picHeader, ParameterSetManager *parameterSetManager, const int prevTid0POC, const int prevPicPOC)
{
  uint32_t  uiCode;
  int   iCode;

#if ENABLE_TRACING
  xTraceSliceHeader();
#endif
  PPS *pps = nullptr;
  SPS *sps = nullptr;
  xReadFlag(uiCode, "sh_picture_header_in_slice_header_flag");
  pcSlice->setPictureHeaderInSliceHeader(uiCode);
  if (uiCode)
  {
    parsePictureHeader(picHeader, parameterSetManager, false);
    picHeader->setValid();
  }
  CHECK(picHeader == nullptr, "Invalid Picture Header");
  CHECK(picHeader->isValid()==false, "Invalid Picture Header");
  checkAlfNaluTidAndPicTid(pcSlice, picHeader, parameterSetManager);
  pps = parameterSetManager->getPPS( picHeader->getPPSId() );
  //!KS: need to add error handling code here, if PPS is not available
  CHECK(pps == nullptr, "Invalid PPS");
  sps = parameterSetManager->getSPS(pps->getSPSId());
  //!KS: need to add error handling code here, if SPS is not available
  CHECK(sps == nullptr, "Invalid SPS");
  if (sps->getProfileTierLevel()->getConstraintInfo()->getPicHeaderInSliceHeaderConstraintFlag())
  {
    CHECK(pcSlice->getPictureHeaderInSliceHeader() == false, "PH shall be present in SH, when pic_header_in_slice_header_constraint_flag is equal to 1");
  }
  CHECK(pcSlice->getPictureHeaderInSliceHeader() && pps->getRplInfoInPhFlag(),
        "When sh_picture_header_in_slice_header_flag is equal to 1, rpl_info_in_ph_flag shall be equal to 0");
  CHECK(pcSlice->getPictureHeaderInSliceHeader() && pps->getDbfInfoInPhFlag(),
        "When sh_picture_header_in_slice_header_flag is equal to 1, dbf_info_in_ph_flag shall be equal to 0");
  CHECK(pcSlice->getPictureHeaderInSliceHeader() && pps->getSaoInfoInPhFlag(),
        "When sh_picture_header_in_slice_header_flag is equal to 1, sao_info_in_ph_flag shall be equal to 0");
  CHECK(pcSlice->getPictureHeaderInSliceHeader() && pps->getAlfInfoInPhFlag(),
        "When sh_picture_header_in_slice_header_flag is equal to 1, alf_info_in_ph_flag shall be equal to 0");
  CHECK(pcSlice->getPictureHeaderInSliceHeader() && pps->getWpInfoInPhFlag(),
        "When sh_picture_header_in_slice_header_flag is equal to 1, wp_info_in_ph_flag shall be equal to 0");
  CHECK(pcSlice->getPictureHeaderInSliceHeader() && pps->getQpDeltaInfoInPhFlag(),
        "When sh_picture_header_in_slice_header_flag is equal to 1, qp_delta_info_in_ph_flag shall be equal to 0");
  CHECK(pcSlice->getPictureHeaderInSliceHeader() && sps->getSubPicInfoPresentFlag(),
        "When sps_subpic_info_present_flag is equal to 1, the value of sh_picture_header_in_slice_header_flag shall be "
        "equal to 0");
  CHECK(sps->getSubPicInfoPresentFlag() && sps->getVirtualBoundariesEnabledFlag()
          && sps->getVirtualBoundariesPresentFlag() == 0,
        "when sps_subpic_info_present_flag is equal to 1 and sps_virtual_boundaries_enabled_flag is equal to 1, "
        "sps_virtual_boundaries_present_flag shall be equal 1");

  const ChromaFormat chFmt        = sps->getChromaFormatIdc();
  const uint32_t     numValidComp = getNumberValidComponents(chFmt);
  const bool         hasChroma    = isChromaEnabled(chFmt);

  // picture order count
  uiCode = picHeader->getPocLsb();
  int pocLsb    = uiCode;
  int maxPocLsb = 1 << sps->getBitsForPOC();
  int pocMsb;
  if (pcSlice->getIdrPicFlag())
  {
    if (picHeader->getPocMsbPresentFlag())
    {
      pocMsb = picHeader->getPocMsbVal() * maxPocLsb;
    }
    else
    {
      pocMsb = 0;
    }
    pcSlice->setPOC(pocMsb + pocLsb);
  }
  else
  {
    int prevPoc    = prevTid0POC;
    int prevPocLsb = prevPoc & (maxPocLsb - 1);
    int prevPocMsb = prevPoc - prevPocLsb;
    if (picHeader->getPocMsbPresentFlag())
    {
      pocMsb = picHeader->getPocMsbVal() * maxPocLsb;
    }
    else
    {
      if ((pocLsb < prevPocLsb) && ((prevPocLsb - pocLsb) >= (maxPocLsb / 2)))
      {
        pocMsb = prevPocMsb + maxPocLsb;
      }
      else if ((pocLsb > prevPocLsb) && ((pocLsb - prevPocLsb) > (maxPocLsb / 2)))
      {
        pocMsb = prevPocMsb - maxPocLsb;
      }
      else
      {
        pocMsb = prevPocMsb;
      }
    }
    pcSlice->setPOC(pocMsb + pocLsb);
  }

  if (sps->getSubPicInfoPresentFlag())
  {
    uint32_t bitsSubPicId;
    bitsSubPicId = sps->getSubPicIdLen();
    xReadCode(bitsSubPicId, uiCode, "sh_subpic_id");    pcSlice->setSliceSubPicId(uiCode);
  }
  else
  {
    pcSlice->setSliceSubPicId(0);
  }

  // raster scan slices
  uint32_t sliceAddr = 0;
  if(pps->getRectSliceFlag() == 0)
  {
    // slice address is the raster scan tile index of first tile in slice
    if( pps->getNumTiles() > 1 )
    {
      int bitsSliceAddress = ceilLog2(pps->getNumTiles());
      xReadCode(bitsSliceAddress, uiCode, "sh_slice_address");  sliceAddr = uiCode;
    }
  }
  // rectangular slices
  else
  {
    // slice address is the index of the slice within the current sub-picture
    uint32_t currSubPicIdx = pps->getSubPicIdxFromSubPicId( pcSlice->getSliceSubPicId() );
    SubPic currSubPic = pps->getSubPic(currSubPicIdx);
    if( currSubPic.getNumSlicesInSubPic() > 1 )
    {
      int bitsSliceAddress = ceilLog2(currSubPic.getNumSlicesInSubPic());
      xReadCode(bitsSliceAddress, uiCode, "sh_slice_address");  sliceAddr = uiCode;
      CHECK(sliceAddr >= currSubPic.getNumSlicesInSubPic(), "Invalid slice address");
    }
    uint32_t picLevelSliceIdx = sliceAddr;
    for(int subpic = 0; subpic < currSubPicIdx; subpic++)
    {
      picLevelSliceIdx += pps->getSubPic(subpic).getNumSlicesInSubPic();
    }
    pcSlice->setSliceMap( pps->getSliceMap(picLevelSliceIdx) );
    pcSlice->setSliceID(picLevelSliceIdx);
  }

  std::vector<bool> shExtraBitsPresent = sps->getExtraSHBitPresentFlags();
  for (int i=0; i< sps->getNumExtraSHBytes() * 8; i++)
  {
    // extra bits are ignored (when present)
    if (shExtraBitsPresent[i])
    {
      xReadFlag(uiCode, "sh_extra_bit[ i ]");
    }
  }

  if(pps->getRectSliceFlag() == 0)
  {
    uint32_t numTilesInSlice = 1;
    if( pps->getNumTiles() > 1 )
    {
      if (((int)pps->getNumTiles() - (int)sliceAddr) > 1)
      {
        xReadUvlc(uiCode, "sh_num_tiles_in_slice_minus1");        numTilesInSlice = uiCode + 1;
      }
      if (!pps->getRectSliceFlag() && sps->getProfileTierLevel()->getConstraintInfo()->getOneSlicePerPicConstraintFlag())
      {
        CHECK(pps->getNumTiles() != uiCode + 1, "When pps_rect_slice_flag is equal to 0 and one_slice_per_pic_constraint_flag equal to 1, the value of sh_num_tiles_in_slice_minus1 present in each slice header shall be equal to NumTilesInPic - 1");
      }
    }
    CHECK(sliceAddr >= pps->getNumTiles(), "Invalid slice address");
    pcSlice->initSliceMap();
    pcSlice->setSliceID(sliceAddr);

    for( uint32_t tileIdx = sliceAddr; tileIdx < sliceAddr + numTilesInSlice; tileIdx++ )
    {
      uint32_t tileX = tileIdx % pps->getNumTileColumns();
      uint32_t tileY = tileIdx / pps->getNumTileColumns();
      CHECK(tileY >= pps->getNumTileRows(), "Number of tiles in slice exceeds the remaining number of tiles in picture");

      pcSlice->addCtusToSlice(pps->getTileColumnBd(tileX), pps->getTileColumnBd(tileX + 1),
                              pps->getTileRowBd(tileY), pps->getTileRowBd(tileY + 1), pps->getPicWidthInCtu());
    }
  }

  if (picHeader->getPicInterSliceAllowedFlag())
  {
    xReadUvlc (    uiCode, "sh_slice_type" );            pcSlice->setSliceType((SliceType)uiCode);
    VPS *vps = parameterSetManager->getVPS(sps->getVPSId());
    if (pcSlice->isIRAP() && (sps->getVPSId() == 0 || pcSlice->getPOC() != prevPicPOC || vps->getIndependentLayerFlag(vps->getGeneralLayerIdx(pcSlice->getNalUnitLayerId())) == 1))
    {
      CHECK(uiCode != 2, "When nal_unit_type is in the range of IDR_W_RADL to CRA_NUT, inclusive, and vps_independent_layer_flag[ GeneralLayerIdx[ nuh_layer_id ] ] is equal to 1 or the current picture is the first picture in the current AU, sh_slice_type shall be equal to 2");
    }
  }
  else
  {
    pcSlice->setSliceType(I_SLICE);
  }
  if (!picHeader->getPicIntraSliceAllowedFlag())
  {
    CHECK(pcSlice->getSliceType() == I_SLICE, "when ph_intra_slice_allowed_flag = 0, no I_Slice is allowed");
  }
  if (pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_W_RADL || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR)
  {
    xReadFlag(uiCode, "sh_no_output_of_prior_pics_flag");   pcSlice->setNoOutputOfPriorPicsFlag(uiCode != 0);
  }
  // inherit values from picture header
  //   set default values in case slice overrides are disabled
  pcSlice->inheritFromPicHeader(picHeader, pps, sps);

  if (sps->getALFEnabledFlag() && !pps->getAlfInfoInPhFlag())
  {
    xReadFlag(uiCode, "sh_alf_enabled_flag");
    const bool alfEnabledFlag = uiCode != 0;
    pcSlice->setAlfEnabledFlag(COMPONENT_Y, alfEnabledFlag);

    bool alfCbEnabledFlag = false;
    bool alfCrEnabledFlag = false;

    AlfApsList apsIds;
    if (alfEnabledFlag)
    {
      xReadCode(3, uiCode, "sh_num_alf_aps_ids_luma");
      const int numAps = uiCode;

      for (int i = 0; i < numAps; i++)
      {
        xReadCode(3, uiCode, "sh_alf_aps_id_luma[i]");
        const int apsId = uiCode;

        apsIds.push_back(apsId);

        APS *apsToCheckLuma = parameterSetManager->getAPS(apsId, ApsType::ALF);
        CHECK(apsToCheckLuma == nullptr, "referenced APS not found");
        CHECK(apsToCheckLuma->getAlfAPSParam().newFilterFlag[ChannelType::LUMA] != 1,
              "bitstream conformance error, alf_luma_filter_signal_flag shall be equal to 1");
      }

      if (hasChroma)
      {
        xReadCode(1, uiCode, "sh_alf_cb_enabled_flag");
        alfCbEnabledFlag = uiCode != 0;
        xReadCode(1, uiCode, "sh_alf_cr_enabled_flag");
        alfCrEnabledFlag = uiCode != 0;
      }

      if (alfCbEnabledFlag || alfCrEnabledFlag)
      {
        xReadCode(3, uiCode, "sh_alf_aps_id_chroma");
        pcSlice->setAlfApsIdChroma(uiCode);
        APS *apsToCheckChroma = parameterSetManager->getAPS(uiCode, ApsType::ALF);
        CHECK(apsToCheckChroma == nullptr, "referenced APS not found");
        CHECK(apsToCheckChroma->getAlfAPSParam().newFilterFlag[ChannelType::CHROMA] != 1,
              "bitstream conformance error, alf_chroma_filter_signal_flag shall be equal to 1");
      }
    }

    pcSlice->setNumAlfApsIdsLuma((int) apsIds.size());
    pcSlice->setAlfApsIdsLuma(apsIds);
    pcSlice->setAlfEnabledFlag(COMPONENT_Cb, alfCbEnabledFlag);
    pcSlice->setAlfEnabledFlag(COMPONENT_Cr, alfCrEnabledFlag);

    CcAlfFilterParam &filterParam = pcSlice->m_ccAlfFilterParam;
    if (sps->getCCALFEnabledFlag() && pcSlice->getAlfEnabledFlag(COMPONENT_Y))
    {
      xReadFlag(uiCode, "sh_alf_cc_cb_enabled_flag");
      pcSlice->setCcAlfCbEnabledFlag(uiCode);
      filterParam.ccAlfFilterEnabled[COMPONENT_Cb - 1] = (uiCode == 1) ? true : false;
      pcSlice->setCcAlfCbApsId(-1);
      if (filterParam.ccAlfFilterEnabled[COMPONENT_Cb - 1])
      {
        // parse APS ID
        xReadCode(3, uiCode, "sh_alf_cc_cb_aps_id");
        pcSlice->setCcAlfCbApsId(uiCode);
        APS *apsToCheckCcCb = parameterSetManager->getAPS(uiCode, ApsType::ALF);
        CHECK(apsToCheckCcCb == nullptr, "referenced APS not found");
        CHECK(apsToCheckCcCb->getCcAlfAPSParam().newCcAlfFilter[COMPONENT_Cb - 1] != 1, "bitstream conformance error, alf_cc_cb_filter_signal_flag shall be equal to 1");
      }
      // Cr
      xReadFlag(uiCode, "sh_alf_cc_cr_enabled_flag");
      pcSlice->setCcAlfCrEnabledFlag(uiCode);
      filterParam.ccAlfFilterEnabled[COMPONENT_Cr - 1] = (uiCode == 1) ? true : false;
      pcSlice->setCcAlfCrApsId(-1);
      if (filterParam.ccAlfFilterEnabled[COMPONENT_Cr - 1])
      {
        // parse APS ID
        xReadCode(3, uiCode, "sh_alf_cc_cr_aps_id");
        pcSlice->setCcAlfCrApsId(uiCode);
        APS *apsToCheckCcCr = parameterSetManager->getAPS(uiCode, ApsType::ALF);
        CHECK(apsToCheckCcCr == nullptr, "referenced APS not found");
        CHECK(apsToCheckCcCr->getCcAlfAPSParam().newCcAlfFilter[COMPONENT_Cr - 1] != 1, "bitstream conformance error, alf_cc_cr_filter_signal_flag shall be equal to 1");
      }
    }
    else
    {
      filterParam.ccAlfFilterEnabled[COMPONENT_Cb - 1] = false;
      filterParam.ccAlfFilterEnabled[COMPONENT_Cr - 1] = false;
      pcSlice->setCcAlfCbApsId(-1);
      pcSlice->setCcAlfCrApsId(-1);
    }
  }
  if (picHeader->getLmcsEnabledFlag() && !pcSlice->getPictureHeaderInSliceHeader())
  {
    xReadFlag(uiCode, "sh_lmcs_used_flag");
    pcSlice->setLmcsEnabledFlag(uiCode);
  }
  else
  {
    pcSlice->setLmcsEnabledFlag(pcSlice->getPictureHeaderInSliceHeader() ? picHeader->getLmcsEnabledFlag() : false);
  }
  if (picHeader->getExplicitScalingListEnabledFlag() && !pcSlice->getPictureHeaderInSliceHeader())
  {
    xReadFlag(uiCode, "sh_explicit_scaling_list_used_flag");
    pcSlice->setExplicitScalingListUsed(uiCode);
  }
  else
  {
    pcSlice->setExplicitScalingListUsed(pcSlice->getPictureHeaderInSliceHeader() ? picHeader->getExplicitScalingListEnabledFlag() : false);
  }

  if (pps->getRplInfoInPhFlag())
  {
    *pcSlice->getRpl(REF_PIC_LIST_0) = *picHeader->getRpl(REF_PIC_LIST_0);
    *pcSlice->getRpl(REF_PIC_LIST_1) = *picHeader->getRpl(REF_PIC_LIST_1);
  }
  else if (pcSlice->getIdrPicFlag() && !(sps->getIDRRefParamListPresent()))
  {
    ReferencePictureList *rpl0 = pcSlice->getRpl(REF_PIC_LIST_0);
    *rpl0                      = ReferencePictureList();
    ReferencePictureList *rpl1 = pcSlice->getRpl(REF_PIC_LIST_1);
    *rpl1                      = ReferencePictureList();
  }
  else
  {
    // Read L0 related syntax elements
    bool rplSpsFlag0 = false;

    if (sps->getNumRpl(REF_PIC_LIST_0) > 0)
    {
      xReadFlag(uiCode, "ref_pic_list_sps_flag[0]");
      rplSpsFlag0 = uiCode != 0;
    }

    auto const rpl0 = pcSlice->getRpl(REF_PIC_LIST_0);
    if (!rplSpsFlag0)   // explicitly carried in this SH
    {
      *rpl0 = ReferencePictureList();
      parseRefPicList(sps, rpl0, -1);
      pcSlice->setRplIdx(REF_PIC_LIST_0, -1);
    }
    else   // Refer to list in SPS
    {
      int rpsIdx = 0;
      if (sps->getNumRpl(REF_PIC_LIST_0) > 1)
      {
        int numBits = ceilLog2(sps->getNumRpl(REF_PIC_LIST_0));
        xReadCode(numBits, uiCode, "ref_pic_list_idx[0]");
        rpsIdx = uiCode;
      }

      pcSlice->setRplIdx(REF_PIC_LIST_0, rpsIdx);
      *rpl0 = *sps->getRplList(REF_PIC_LIST_0)->getReferencePictureList(rpsIdx);
    }
    // Deal POC Msb cycle signalling for LTRP
    for (int i = 0; i < rpl0->getNumRefEntries(); i++)
    {
      rpl0->setDeltaPocMSBPresentFlag(i, false);
      rpl0->setDeltaPocMSBCycleLT(i, 0);
    }
    if (rpl0->getNumberOfLongtermPictures())
    {
      for (int i = 0; i < rpl0->getNumRefEntries(); i++)
      {
        if (rpl0->isRefPicLongterm(i) && !rpl0->isInterLayerRefPic(i))
        {
          if (rpl0->getLtrpInSliceHeaderFlag())
          {
            xReadCode(sps->getBitsForPOC(), uiCode, "slice_poc_lsb_lt[i][j]");
            rpl0->setRefPicIdentifier(i, uiCode, true, false, 0);
          }
          xReadFlag(uiCode, "delta_poc_msb_present_flag[i][j]");
          rpl0->setDeltaPocMSBPresentFlag(i, uiCode ? true : false);
          if (uiCode)
          {
            xReadUvlc(uiCode, "slice_delta_poc_msb_cycle_lt[i][j]");
            if (i != 0)
            {
              uiCode += rpl0->getDeltaPocMSBCycleLT(i - 1);
            }
            rpl0->setDeltaPocMSBCycleLT(i, uiCode);
          }
          else if(i != 0)
          {
            rpl0->setDeltaPocMSBCycleLT(i, rpl0->getDeltaPocMSBCycleLT(i-1));
          }
          else
          {
            rpl0->setDeltaPocMSBCycleLT(i,0);
          }
        }
        else if (i != 0)
        {
          rpl0->setDeltaPocMSBCycleLT(i, rpl0->getDeltaPocMSBCycleLT(i - 1));
        }
        else
        {
          rpl0->setDeltaPocMSBCycleLT(i, 0);
        }
      }
    }

    // Read L1 related syntax elements
    bool rplSpsFlag1 = sps->getNumRpl(REF_PIC_LIST_1) == 0 ? false : rplSpsFlag0;
    if (sps->getNumRpl(REF_PIC_LIST_1) > 0 && pps->getRpl1IdxPresentFlag())
    {
      xReadFlag(uiCode, "ref_pic_list_sps_flag[1]");
      rplSpsFlag1 = uiCode != 0;
    }

    auto const rpl1 = pcSlice->getRpl(REF_PIC_LIST_1);
    if (rplSpsFlag1)
    {
      if (sps->getNumRpl(REF_PIC_LIST_1) > 1 && pps->getRpl1IdxPresentFlag())
      {
        int numBits = ceilLog2(sps->getNumRpl(REF_PIC_LIST_1));
        xReadCode(numBits, uiCode, "ref_pic_list_idx[1]");
        pcSlice->setRplIdx(REF_PIC_LIST_1, uiCode);
        *rpl1 = *sps->getRplList(REF_PIC_LIST_1)->getReferencePictureList(uiCode);
      }
      else if (sps->getNumRpl(REF_PIC_LIST_1) == 1)
      {
        pcSlice->setRplIdx(REF_PIC_LIST_1, 0);
        *rpl1 = *sps->getRplList(REF_PIC_LIST_1)->getReferencePictureList(0);
      }
      else
      {
        assert(pcSlice->getRplIdx(REF_PIC_LIST_0) != -1);
        pcSlice->setRplIdx(REF_PIC_LIST_1, pcSlice->getRplIdx(REF_PIC_LIST_0));
        *rpl1 = *sps->getRplList(REF_PIC_LIST_1)->getReferencePictureList(pcSlice->getRplIdx(REF_PIC_LIST_0));
      }
    }
    else
    {
      (*rpl1) = ReferencePictureList();
      parseRefPicList(sps, rpl1, -1);
      pcSlice->setRplIdx(REF_PIC_LIST_1, -1);
    }

    // Deal POC Msb cycle signalling for LTRP
    for (int i = 0; i < rpl1->getNumRefEntries(); i++)
    {
      rpl1->setDeltaPocMSBPresentFlag(i, false);
      rpl1->setDeltaPocMSBCycleLT(i, 0);
    }
    if (rpl1->getNumberOfLongtermPictures())
    {
      for (int i = 0; i < rpl1->getNumRefEntries(); i++)
      {
        if (rpl1->isRefPicLongterm(i) && !rpl1->isInterLayerRefPic(i))
        {
          if (rpl1->getLtrpInSliceHeaderFlag())
          {
            xReadCode(sps->getBitsForPOC(), uiCode, "slice_poc_lsb_lt[i][j]");
            rpl1->setRefPicIdentifier(i, uiCode, true, false, 0);
          }
          xReadFlag(uiCode, "delta_poc_msb_present_flag[i][j]");
          rpl1->setDeltaPocMSBPresentFlag(i, uiCode ? true : false);
          if (uiCode)
          {
            xReadUvlc(uiCode, "slice_delta_poc_msb_cycle_lt[i][j]");
            if (i != 0)
            {
              uiCode += rpl1->getDeltaPocMSBCycleLT(i - 1);
            }
            rpl1->setDeltaPocMSBCycleLT(i, uiCode);
          }
          else if(i != 0)
          {
            rpl1->setDeltaPocMSBCycleLT(i, rpl1->getDeltaPocMSBCycleLT(i-1));
          }
          else
          {
            rpl1->setDeltaPocMSBCycleLT(i,0);
          }
        }
        else if (i != 0)
        {
          rpl1->setDeltaPocMSBCycleLT(i, rpl1->getDeltaPocMSBCycleLT(i - 1));
        }
        else
        {
          rpl1->setDeltaPocMSBCycleLT(i, 0);
        }
      }
    }
  }

  uint32_t numActiveRefs[NUM_REF_PIC_LIST_01] = { pcSlice->isIntra() ? 0u : 1u, pcSlice->isInterB() ? 1u : 0u };

  if ((!pcSlice->isIntra() && pcSlice->getRpl(REF_PIC_LIST_0)->getNumRefEntries() > 1)
      || (pcSlice->isInterB() && pcSlice->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 1))
  {
    xReadFlag(uiCode, "sh_num_ref_idx_active_override_flag");
    if (uiCode)
    {
      if (pcSlice->getRpl(REF_PIC_LIST_0)->getNumRefEntries() > 1)
      {
        xReadUvlc(uiCode, "sh_num_ref_idx_active_minus1[0]");
        CHECK(uiCode >= MAX_NUM_ACTIVE_REF,
              "The value of sh_num_ref_idx_active_minus1[0] shall be in the range of 0 to 14, inclusive");
        numActiveRefs[REF_PIC_LIST_0] = uiCode + 1;
      }
      if (pcSlice->isInterB() && pcSlice->getRpl(REF_PIC_LIST_1)->getNumRefEntries() > 1)
      {
        xReadUvlc(uiCode, "sh_num_ref_idx_active_minus1[1]");
        CHECK(uiCode >= MAX_NUM_ACTIVE_REF,
              "The value of sh_num_ref_idx_active_minus1[1] shall be in the range of 0 to 14, inclusive");
        numActiveRefs[REF_PIC_LIST_1] = uiCode + 1;
      }
    }
    else
    {
      numActiveRefs[REF_PIC_LIST_0] = std::min<int>(pcSlice->getRpl(REF_PIC_LIST_0)->getNumRefEntries(),
                                                    pps->getNumRefIdxDefaultActive(REF_PIC_LIST_0));

      if (pcSlice->isInterB())
      {
        numActiveRefs[REF_PIC_LIST_1] = std::min<int>(pcSlice->getRpl(REF_PIC_LIST_1)->getNumRefEntries(),
                                                      pps->getNumRefIdxDefaultActive(REF_PIC_LIST_1));
      }
    }
  }

  pcSlice->setNumRefIdx(REF_PIC_LIST_0, numActiveRefs[REF_PIC_LIST_0]);
  pcSlice->setNumRefIdx(REF_PIC_LIST_1, numActiveRefs[REF_PIC_LIST_1]);

  if (pcSlice->isInterP() || pcSlice->isInterB())
  {
    CHECK(pcSlice->getNumRefIdx(REF_PIC_LIST_0) == 0,
          "Number of active entries in RPL0 of P or B picture shall be greater than 0");

    if (pcSlice->isInterB())
    {
      CHECK(pcSlice->getNumRefIdx(REF_PIC_LIST_1) == 0,
            "Number of active entries in RPL1 of B picture shall be greater than 0");
    }
  }

  pcSlice->setCabacInitFlag(false);   // default
  if (pps->getCabacInitPresentFlag() && !pcSlice->isIntra())
  {
    xReadFlag(uiCode, "sh_cabac_init_flag");
    pcSlice->setCabacInitFlag(uiCode ? true : false);
    pcSlice->setEncCABACTableIdx(pcSlice->getSliceType() == B_SLICE ? (uiCode ? P_SLICE : B_SLICE)
                                                                    : (uiCode ? B_SLICE : P_SLICE));
  }

  if (picHeader->getEnableTMVPFlag())
  {
    if (pcSlice->getSliceType() == P_SLICE)
    {
      pcSlice->setColFromL0Flag(true);
    }
    else if (!pps->getRplInfoInPhFlag() && pcSlice->getSliceType() == B_SLICE)
    {
      xReadFlag(uiCode, "sh_collocated_from_l0_flag");
      pcSlice->setColFromL0Flag(uiCode);
    }
    else
    {
      pcSlice->setColFromL0Flag(picHeader->getPicColFromL0Flag());
    }

    if (!pps->getRplInfoInPhFlag())
    {
      if (pcSlice->getSliceType() != I_SLICE
          && ((pcSlice->getColFromL0Flag() == 1 && pcSlice->getNumRefIdx(REF_PIC_LIST_0) > 1)
              || (pcSlice->getColFromL0Flag() == 0 && pcSlice->getNumRefIdx(REF_PIC_LIST_1) > 1)))
      {
        xReadUvlc(uiCode, "sh_collocated_ref_idx");
        pcSlice->setColRefIdx(uiCode);
      }
      else
      {
        pcSlice->setColRefIdx(0);
      }
    }
    else
    {
      pcSlice->setColRefIdx(picHeader->getColRefIdx());
    }
  }
  if ((pps->getUseWP() && pcSlice->getSliceType() == P_SLICE)
      || (pps->getWPBiPred() && pcSlice->getSliceType() == B_SLICE))
  {
    if (pps->getWpInfoInPhFlag())
    {
      CHECK(pcSlice->getNumRefIdx(REF_PIC_LIST_0) > picHeader->getNumWeights(REF_PIC_LIST_0),
            "ERROR: Number of active reference picture L0 is greater than the number of weighted prediction signalled "
            "in Picture Header");
      CHECK(pcSlice->getNumRefIdx(REF_PIC_LIST_1) > picHeader->getNumWeights(REF_PIC_LIST_1),
            "ERROR: Number of active reference picture L1 is greater than the number of weighted prediction signalled "
            "in Picture Header");
      pcSlice->setWpScaling(picHeader->getWpScalingAll());
    }
    else
    {
      parsePredWeightTable(pcSlice, sps);
    }
    pcSlice->initWpScaling(sps);
  }
  else
  {
    for (int iNumRef = 0; iNumRef < ((pcSlice->getSliceType() == B_SLICE) ? 2 : 1); iNumRef++)
    {
      RefPicList eRefPicList = (iNumRef ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
      for (int refIdx = 0; refIdx < pcSlice->getNumRefIdx(eRefPicList); refIdx++)
      {
        WPScalingParam *wp = pcSlice->getWpScaling(eRefPicList, refIdx);

        wp[0].presentFlag = false;
        wp[1].presentFlag = false;
        wp[2].presentFlag = false;
      }
    }
  }

  int qpDelta = 0;
  if (pps->getQpDeltaInfoInPhFlag())
  {
    qpDelta = picHeader->getQpDelta();
  }
  else
  {
    xReadSvlc(iCode, "sh_qp_delta");
    qpDelta = iCode;
  }
  pcSlice->setSliceQp(QP26 + pps->getPicInitQPMinus26() + qpDelta);
  pcSlice->setSliceQpBase(pcSlice->getSliceQp());

  CHECK(pcSlice->getSliceQp() < -sps->getQpBDOffset(ChannelType::LUMA), "Invalid slice QP delta");
  CHECK(pcSlice->getSliceQp() > MAX_QP, "Invalid slice QP");

  if (pps->getSliceChromaQpFlag())
  {
    if (numValidComp > COMPONENT_Cb)
    {
      xReadSvlc(iCode, "sh_cb_qp_offset");
      pcSlice->setSliceChromaQpDelta(COMPONENT_Cb, iCode);
      CHECK(pcSlice->getSliceChromaQpDelta(COMPONENT_Cb) < -12, "Invalid chroma QP offset");
      CHECK(pcSlice->getSliceChromaQpDelta(COMPONENT_Cb) > 12, "Invalid chroma QP offset");
      CHECK((pps->getQpOffset(COMPONENT_Cb) + pcSlice->getSliceChromaQpDelta(COMPONENT_Cb)) < -12,
            "Invalid chroma QP offset");
      CHECK((pps->getQpOffset(COMPONENT_Cb) + pcSlice->getSliceChromaQpDelta(COMPONENT_Cb)) > 12,
            "Invalid chroma QP offset");
    }

    if (numValidComp > COMPONENT_Cr)
    {
      xReadSvlc(iCode, "sh_cr_qp_offset");
      pcSlice->setSliceChromaQpDelta(COMPONENT_Cr, iCode);
      CHECK(pcSlice->getSliceChromaQpDelta(COMPONENT_Cr) < -12, "Invalid chroma QP offset");
      CHECK(pcSlice->getSliceChromaQpDelta(COMPONENT_Cr) > 12, "Invalid chroma QP offset");
      CHECK((pps->getQpOffset(COMPONENT_Cr) + pcSlice->getSliceChromaQpDelta(COMPONENT_Cr)) < -12,
            "Invalid chroma QP offset");
      CHECK((pps->getQpOffset(COMPONENT_Cr) + pcSlice->getSliceChromaQpDelta(COMPONENT_Cr)) > 12,
            "Invalid chroma QP offset");
      if (sps->getJointCbCrEnabledFlag())
      {
        xReadSvlc(iCode, "sh_joint_cbcr_qp_offset");
        pcSlice->setSliceChromaQpDelta(JOINT_CbCr, iCode);
        CHECK(pcSlice->getSliceChromaQpDelta(JOINT_CbCr) < -12, "Invalid chroma QP offset");
        CHECK(pcSlice->getSliceChromaQpDelta(JOINT_CbCr) > 12, "Invalid chroma QP offset");
        CHECK((pps->getQpOffset(JOINT_CbCr) + pcSlice->getSliceChromaQpDelta(JOINT_CbCr)) < -12,
              "Invalid chroma QP offset");
        CHECK((pps->getQpOffset(JOINT_CbCr) + pcSlice->getSliceChromaQpDelta(JOINT_CbCr)) > 12,
              "Invalid chroma QP offset");
      }
    }
  }

  if (pps->getCuChromaQpOffsetListEnabledFlag())
  {
    xReadFlag(uiCode, "sh_cu_chroma_qp_offset_enabled_flag");
    pcSlice->setUseChromaQpAdj(uiCode != 0);
  }
  else
  {
    pcSlice->setUseChromaQpAdj(false);
  }

  if (sps->getSAOEnabledFlag() && !pps->getSaoInfoInPhFlag())
  {
    xReadFlag(uiCode, "sh_sao_luma_used_flag");
    pcSlice->setSaoEnabledFlag(ChannelType::LUMA, uiCode != 0);

    if (hasChroma)
    {
      xReadFlag(uiCode, "sh_sao_chroma_used_flag");
      pcSlice->setSaoEnabledFlag(ChannelType::CHROMA, uiCode != 0);
    }
  }

  if (pps->getDeblockingFilterControlPresentFlag())
  {
    if (pps->getDeblockingFilterOverrideEnabledFlag() && !pps->getDbfInfoInPhFlag())
    {
      xReadFlag(uiCode, "sh_deblocking_params_present_flag");
      pcSlice->setDeblockingFilterOverrideFlag(uiCode ? true : false);
    }
    else
    {
      pcSlice->setDeblockingFilterOverrideFlag(false);
    }
    if (pcSlice->getDeblockingFilterOverrideFlag())
    {
      if (!pps->getPPSDeblockingFilterDisabledFlag())
      {
        xReadFlag(uiCode, "sh_deblocking_filter_disabled_flag");
        pcSlice->setDeblockingFilterDisable(uiCode != 0);
      }
      else
      {
        pcSlice->setDeblockingFilterDisable(false);
      }
      if (!pcSlice->getDeblockingFilterDisable())
      {
        xReadSvlc(iCode, "sh_luma_beta_offset_div2");
        pcSlice->setDeblockingFilterBetaOffsetDiv2(iCode);
        CHECK(pcSlice->getDeblockingFilterBetaOffsetDiv2() < -12 || pcSlice->getDeblockingFilterBetaOffsetDiv2() > 12,
              "Invalid deblocking filter configuration");
        xReadSvlc(iCode, "sh_luma_tc_offset_div2");
        pcSlice->setDeblockingFilterTcOffsetDiv2(iCode);
        CHECK(pcSlice->getDeblockingFilterTcOffsetDiv2() < -12 || pcSlice->getDeblockingFilterTcOffsetDiv2() > 12,
              "Invalid deblocking filter configuration");

        if (pps->getPPSChromaToolFlag())
        {
          xReadSvlc(iCode, "sh_cb_beta_offset_div2");
          pcSlice->setDeblockingFilterCbBetaOffsetDiv2(iCode);
          CHECK(pcSlice->getDeblockingFilterCbBetaOffsetDiv2() < -12
                  || pcSlice->getDeblockingFilterCbBetaOffsetDiv2() > 12,
                "Invalid deblocking filter configuration");
          xReadSvlc(iCode, "sh_cb_tc_offset_div2");
          pcSlice->setDeblockingFilterCbTcOffsetDiv2(iCode);
          CHECK(pcSlice->getDeblockingFilterCbTcOffsetDiv2() < -12 || pcSlice->getDeblockingFilterCbTcOffsetDiv2() > 12,
                "Invalid deblocking filter configuration");

          xReadSvlc(iCode, "sh_cr_beta_offset_div2");
          pcSlice->setDeblockingFilterCrBetaOffsetDiv2(iCode);
          CHECK(pcSlice->getDeblockingFilterCrBetaOffsetDiv2() < -12
                  || pcSlice->getDeblockingFilterCrBetaOffsetDiv2() > 12,
                "Invalid deblocking filter configuration");
          xReadSvlc(iCode, "sh_cr_tc_offset_div2");
          pcSlice->setDeblockingFilterCrTcOffsetDiv2(iCode);
          CHECK(pcSlice->getDeblockingFilterCrTcOffsetDiv2() < -12 || pcSlice->getDeblockingFilterCrTcOffsetDiv2() > 12,
                "Invalid deblocking filter configuration");
        }
        else
        {
          pcSlice->setDeblockingFilterCbBetaOffsetDiv2(pcSlice->getDeblockingFilterBetaOffsetDiv2());
          pcSlice->setDeblockingFilterCbTcOffsetDiv2(pcSlice->getDeblockingFilterTcOffsetDiv2());
          pcSlice->setDeblockingFilterCrBetaOffsetDiv2(pcSlice->getDeblockingFilterBetaOffsetDiv2());
          pcSlice->setDeblockingFilterCrTcOffsetDiv2(pcSlice->getDeblockingFilterTcOffsetDiv2());
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
  if( sps->getDepQuantEnabledFlag() )
  {
    xReadFlag(uiCode, "sh_dep_quant_used_flag");
    pcSlice->setDepQuantEnabledFlag(uiCode != 0);
  }
  else
  {
    pcSlice->setDepQuantEnabledFlag(false);
  }

  // sign data hiding
  if( sps->getSignDataHidingEnabledFlag() && !pcSlice->getDepQuantEnabledFlag() )
  {
    xReadFlag( uiCode, "sh_sign_data_hiding_used_flag" );
    pcSlice->setSignDataHidingEnabledFlag( uiCode != 0 );
  }
  else
  {
    pcSlice->setSignDataHidingEnabledFlag(false);
  }

  // signal TS residual coding disabled flag
  if (sps->getTransformSkipEnabledFlag() && !pcSlice->getDepQuantEnabledFlag() && !pcSlice->getSignDataHidingEnabledFlag())
  {
    xReadFlag(uiCode, "sh_ts_residual_coding_disabled_flag");
    pcSlice->setTSResidualCodingDisabledFlag( uiCode != 0 );
  }
  else
  {
    pcSlice->setTSResidualCodingDisabledFlag( false );
  }

  if (!pcSlice->getTSResidualCodingDisabledFlag() && sps->getSpsRangeExtension().getTSRCRicePresentFlag())
  {
    xReadCode(3, uiCode, "sh_ts_residual_coding_rice_idx_minus1");
    pcSlice->setTsrcIndex(uiCode);
  }
  if (sps->getSpsRangeExtension().getReverseLastSigCoeffEnabledFlag())
  {
    xReadFlag(uiCode, "sh_reverse_last_sig_coeff_flag");
    pcSlice->setReverseLastSigCoeffFlag(uiCode != 0);
  }
  else
  {
    pcSlice->setReverseLastSigCoeffFlag(false);
  }

  if( pcSlice->getFirstCtuRsAddrInSlice() == 0 )
  {
    pcSlice->setDefaultClpRng( *sps );
  }

  if(pps->getSliceHeaderExtensionPresentFlag())
  {
    xReadUvlc(uiCode,"sh_slice_header_extension_length");
    for(int i=0; i<uiCode; i++)
    {
      uint32_t ignore_;
      xReadCode(8,ignore_,"sh_slice_header_extension_data_byte");
    }
  }

  std::vector<uint32_t> entryPointOffset;

  pcSlice->resetNumberOfSubstream();
  pcSlice->setNumSubstream(sps, pps);

  pcSlice->setNumEntryPoints( sps, pps );
  if( pcSlice->getNumEntryPoints() > 0 )
  {
    uint32_t offsetLenMinus1;
    xReadUvlc( offsetLenMinus1, "sh_entry_offset_len_minus1" );
    entryPointOffset.resize( pcSlice->getNumEntryPoints() );
    for( uint32_t idx = 0; idx < pcSlice->getNumEntryPoints(); idx++ )
    {
      xReadCode( offsetLenMinus1 + 1, uiCode, "sh_entry_point_offset_minus1" );
      entryPointOffset[idx] = uiCode + 1;
    }
  }

#if RExt__DECODER_DEBUG_BIT_STATISTICS
  CodingStatistics::IncrementStatisticEP(STATS__BYTE_ALIGNMENT_BITS,m_pcBitstream->readByteAlignment(),0);
#else
  m_pcBitstream->readByteAlignment();
#endif

  pcSlice->clearSubstreamSizes();

  if( pcSlice->getNumEntryPoints() > 0 )
  {
    int endOfSliceHeaderLocation = m_pcBitstream->getByteLocation();

    // Adjust endOfSliceHeaderLocation to account for emulation prevention bytes in the slice segment header
    for ( uint32_t curByteIdx  = 0; curByteIdx<m_pcBitstream->numEmulationPreventionBytesRead(); curByteIdx++ )
    {
      if ( m_pcBitstream->getEmulationPreventionByteLocation( curByteIdx ) < endOfSliceHeaderLocation )
      {
        endOfSliceHeaderLocation++;
      }
    }

    int  curEntryPointOffset     = 0;
    int  prevEntryPointOffset    = 0;
    for (uint32_t idx=0; idx<entryPointOffset.size(); idx++)
    {
      curEntryPointOffset += entryPointOffset[ idx ];

      int emulationPreventionByteCount = 0;
      for ( uint32_t curByteIdx  = 0; curByteIdx<m_pcBitstream->numEmulationPreventionBytesRead(); curByteIdx++ )
      {
        if (m_pcBitstream->getEmulationPreventionByteLocation(curByteIdx)
              >= (prevEntryPointOffset + endOfSliceHeaderLocation)
            && m_pcBitstream->getEmulationPreventionByteLocation(curByteIdx)
                 < (curEntryPointOffset + endOfSliceHeaderLocation))
        {
          emulationPreventionByteCount++;
        }
      }

      entryPointOffset[ idx ] -= emulationPreventionByteCount;
      prevEntryPointOffset = curEntryPointOffset;
      pcSlice->addSubstreamSize(entryPointOffset [ idx ] );
    }
  }
}

void HLSyntaxReader::getSlicePoc(Slice* pcSlice, PicHeader* picHeader, ParameterSetManager *parameterSetManager, const int prevTid0POC)
{
  uint32_t  uiCode;
  uint32_t  pocLsb;

  PPS *pps = nullptr;
  SPS *sps = nullptr;

  CHECK(picHeader == nullptr, "Invalid Picture Header");
  CHECK(picHeader->isValid()==false, "Invalid Picture Header");
  pps = parameterSetManager->getPPS( picHeader->getPPSId() );
  //!KS: need to add error handling code here, if PPS is not available
  CHECK(pps == nullptr, "Invalid PPS");
  sps = parameterSetManager->getSPS(pps->getSPSId());
  //!KS: need to add error handling code here, if SPS is not available
  CHECK(sps == nullptr, "Invalid SPS");

  DTRACE_UPDATE( g_trace_ctx, std::make_pair( "final", 0 ) );

  xReadFlag(uiCode, "sh_picture_header_in_slice_header_flag");
  if (uiCode == 0)
  {
    pocLsb = picHeader->getPocLsb();
  }
  else
  {
    uint32_t phGdrOrIrapPicFlag;
    xReadFlag(phGdrOrIrapPicFlag, "ph_gdr_or_irap_pic_flag");
    xReadFlag(uiCode, "ph_non_ref_pic_flag");
    if (phGdrOrIrapPicFlag)
    {
      xReadFlag(uiCode, "ph_gdr_pic_flag");
    }
    xReadFlag(uiCode, "ph_inter_slice_allowed_flag");
    if (uiCode)
    {
      xReadFlag(uiCode, "ph_intra_slice_allowed_flag");
    }
    // parameter sets
    xReadUvlc(uiCode, "ph_pic_parameter_set_id");
    // picture order count
    xReadCode(sps->getBitsForPOC(), pocLsb, "ph_pic_order_cnt_lsb");
  }
  int maxPocLsb = 1 << sps->getBitsForPOC();
  int pocMsb;
  if (pcSlice->getIdrPicFlag())
  {
    if (picHeader->getPocMsbPresentFlag())
    {
      pocMsb = picHeader->getPocMsbVal()*maxPocLsb;
    }
    else
    {
      pocMsb = 0;
    }
    pcSlice->setPOC(pocMsb + pocLsb);
  }
  else
  {
    int prevPoc = prevTid0POC;
    int prevPocLsb = prevPoc & (maxPocLsb - 1);
    int prevPocMsb = prevPoc - prevPocLsb;
    if (picHeader->getPocMsbPresentFlag())
    {
      pocMsb = picHeader->getPocMsbVal()*maxPocLsb;
    }
    else
    {
      if ((pocLsb < prevPocLsb) && ((prevPocLsb - pocLsb) >= (maxPocLsb / 2)))
      {
        pocMsb = prevPocMsb + maxPocLsb;
      }
      else if ((pocLsb > prevPocLsb) && ((pocLsb - prevPocLsb) > (maxPocLsb / 2)))
      {
        pocMsb = prevPocMsb - maxPocLsb;
      }
      else
      {
        pocMsb = prevPocMsb;
      }
    }
    pcSlice->setPOC(pocMsb + pocLsb);
  }
  DTRACE_UPDATE( g_trace_ctx, std::make_pair( "final", 1 ) );
}

void HLSyntaxReader::parseConstraintInfo(ConstraintInfo *cinfo, const ProfileTierLevel* ptl )
{
  uint32_t symbol;
  xReadFlag(symbol, "gci_present_flag"); cinfo->setGciPresentFlag(symbol ? true : false);
  if (cinfo->getGciPresentFlag())
  {
    /* general */
    xReadFlag(symbol, "gci_intra_only_constraint_flag");                 cinfo->setIntraOnlyConstraintFlag(symbol ? true : false);
    xReadFlag(symbol, "gci_all_layers_independent_constraint_flag");     cinfo->setAllLayersIndependentConstraintFlag(symbol ? true : false);
    xReadFlag(symbol, "gci_one_au_only_constraint_flag");                cinfo->setOnePictureOnlyConstraintFlag(symbol ? true : false);

    /* picture format */
    xReadCode(4, symbol, "gci_sixteen_minus_max_bitdepth_constraint_idc"); cinfo->setMaxBitDepthConstraintIdc(symbol>8 ? 16 : (16 - symbol));
    CHECK(symbol>8, "gci_sixteen_minus_max_bitdepth_constraint_idc shall be in the range 0 to 8, inclusive");
    xReadCode(2, symbol, "gci_three_minus_max_chroma_format_constraint_idc"); cinfo->setMaxChromaFormatConstraintIdc((ChromaFormat)(3 - symbol));

    /* NAL unit type related */
    xReadFlag(symbol, "gci_no_mixed_nalu_types_in_pic_constraint_flag"); cinfo->setNoMixedNaluTypesInPicConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_trail_constraint_flag");                   cinfo->setNoTrailConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_stsa_constraint_flag");                    cinfo->setNoStsaConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_rasl_constraint_flag");                    cinfo->setNoRaslConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_radl_constraint_flag");                    cinfo->setNoRadlConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_idr_constraint_flag");                     cinfo->setNoIdrConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_cra_constraint_flag");                     cinfo->setNoCraConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_gdr_constraint_flag");                     cinfo->setNoGdrConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_aps_constraint_flag");                     cinfo->setNoApsConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_idr_rpl_constraint_flag");                 cinfo->setNoIdrRplConstraintFlag(symbol > 0 ? true : false);

    /* tile, slice, subpicture partitioning */
    xReadFlag(symbol, "gci_one_tile_per_pic_constraint_flag");           cinfo->setOneTilePerPicConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_pic_header_in_slice_header_constraint_flag"); cinfo->setPicHeaderInSliceHeaderConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_one_slice_per_pic_constraint_flag");          cinfo->setOneSlicePerPicConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_rectangular_slice_constraint_flag");       cinfo->setNoRectSliceConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_one_slice_per_subpic_constraint_flag");       cinfo->setOneSlicePerSubpicConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_subpic_info_constraint_flag");             cinfo->setNoSubpicInfoConstraintFlag(symbol > 0 ? true : false);

    /* CTU and block partitioning */
    xReadCode(2, symbol, "gci_three_minus_max_log2_ctu_size_constraint_idc");   cinfo->setMaxLog2CtuSizeConstraintIdc(((3 - symbol) + 5));
    xReadFlag(symbol, "gci_no_partition_constraints_override_constraint_flag"); cinfo->setNoPartitionConstraintsOverrideConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_mtt_constraint_flag");                            cinfo->setNoMttConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_qtbtt_dual_tree_intra_constraint_flag");          cinfo->setNoQtbttDualTreeIntraConstraintFlag(symbol > 0 ? true : false);

    /* intra */
    xReadFlag(symbol, "gci_no_palette_constraint_flag");                 cinfo->setNoPaletteConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_ibc_constraint_flag");                     cinfo->setNoIbcConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_isp_constraint_flag");                     cinfo->setNoIspConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_mrl_constraint_flag");                     cinfo->setNoMrlConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_mip_constraint_flag");                     cinfo->setNoMipConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_cclm_constraint_flag");                    cinfo->setNoCclmConstraintFlag(symbol > 0 ? true : false);

    /* inter */
    xReadFlag(symbol, "gci_no_ref_pic_resampling_constraint_flag");      cinfo->setNoRprConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_res_change_in_clvs_constraint_flag");      cinfo->setNoResChangeInClvsConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_weighted_prediction_constraint_flag");     cinfo->setNoWeightedPredictionConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_ref_wraparound_constraint_flag");          cinfo->setNoRefWraparoundConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_temporal_mvp_constraint_flag");            cinfo->setNoTemporalMvpConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_sbtmvp_constraint_flag");                  cinfo->setNoSbtmvpConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_amvr_constraint_flag");                    cinfo->setNoAmvrConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_bdof_constraint_flag");                    cinfo->setNoBdofConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_smvd_constraint_flag");                    cinfo->setNoSmvdConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_dmvr_constraint_flag");                    cinfo->setNoDmvrConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_mmvd_constraint_flag");                    cinfo->setNoMmvdConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_affine_motion_constraint_flag");           cinfo->setNoAffineMotionConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_prof_constraint_flag");                    cinfo->setNoProfConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_bcw_constraint_flag");                     cinfo->setNoBcwConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_ciip_constraint_flag");                    cinfo->setNoCiipConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_gpm_constraint_flag");                     cinfo->setNoGeoConstraintFlag(symbol > 0 ? true : false);

    /* transform, quantization, residual */
    xReadFlag(symbol, "gci_no_luma_transform_size_64_constraint_flag");  cinfo->setNoLumaTransformSize64ConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_transform_skip_constraint_flag");          cinfo->setNoTransformSkipConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_bdpcm_constraint_flag");                   cinfo->setNoBDPCMConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_mts_constraint_flag");                     cinfo->setNoMtsConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_lfnst_constraint_flag");                   cinfo->setNoLfnstConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_joint_cbcr_constraint_flag");              cinfo->setNoJointCbCrConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_sbt_constraint_flag");                     cinfo->setNoSbtConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_act_constraint_flag");                     cinfo->setNoActConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_explicit_scaling_list_constraint_flag");   cinfo->setNoExplicitScaleListConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_dep_quant_constraint_flag");               cinfo->setNoDepQuantConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_sign_data_hiding_constraint_flag");        cinfo->setNoSignDataHidingConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_cu_qp_delta_constraint_flag");             cinfo->setNoCuQpDeltaConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_chroma_qp_offset_constraint_flag");        cinfo->setNoChromaQpOffsetConstraintFlag(symbol > 0 ? true : false);

    /* loop filter */
    xReadFlag(symbol, "gci_no_sao_constraint_flag");                     cinfo->setNoSaoConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_alf_constraint_flag");                     cinfo->setNoAlfConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_ccalf_constraint_flag");                   cinfo->setNoCCAlfConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_lmcs_constraint_flag");                    cinfo->setNoLmcsConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_ladf_constraint_flag");                    cinfo->setNoLadfConstraintFlag(symbol > 0 ? true : false);
    xReadFlag(symbol, "gci_no_virtual_boundaries_constraint_flag");      cinfo->setNoVirtualBoundaryConstraintFlag(symbol > 0 ? true : false);
    xReadCode(8, symbol, "gci_num_additional_bits");
    uint32_t const numAdditionalBits = symbol;
    int numAdditionalBitsUsed;
    if (numAdditionalBits > 5)
    {
      xReadFlag(symbol, "gci_all_rap_pictures_flag");                    cinfo->setAllRapPicturesFlag(symbol > 0 ? true : false);
      xReadFlag(symbol, "gci_no_extended_precision_processing_constraint_flag");  cinfo->setNoExtendedPrecisionProcessingConstraintFlag(symbol > 0 ? true : false);
      xReadFlag(symbol, "gci_no_ts_residual_coding_rice_constraint_flag");        cinfo->setNoTsResidualCodingRiceConstraintFlag(symbol > 0 ? true : false);
      xReadFlag(symbol, "gci_no_rrc_rice_extension_constraint_flag");             cinfo->setNoRrcRiceExtensionConstraintFlag(symbol > 0 ? true : false);
      xReadFlag(symbol, "gci_no_persistent_rice_adaptation_constraint_flag");     cinfo->setNoPersistentRiceAdaptationConstraintFlag(symbol > 0 ? true : false);
      xReadFlag(symbol, "gci_no_reverse_last_sig_coeff_constraint_flag");         cinfo->setNoReverseLastSigCoeffConstraintFlag(symbol > 0 ? true : false);
      numAdditionalBitsUsed = 6;
    }
    else if (numAdditionalBits > 0)
    {
      msg(ERROR, "Invalid bitstream: gci_num_additional_bits set to value %d (must be 0 or >= 6)\n", numAdditionalBits);
      numAdditionalBitsUsed = 0;
    }
    else
    {
      numAdditionalBitsUsed = 0;
    }
    for (int i = 0; i < numAdditionalBits - numAdditionalBitsUsed; i++)
    {
      xReadFlag(symbol, "gci_reserved_bit");
    }
  }
  while (!isByteAligned())
  {
    xReadFlag(symbol, "gci_alignment_zero_bit");                     CHECK(symbol != 0, "gci_alignment_zero_bit not equal to zero");
  }
}


void HLSyntaxReader::parseProfileTierLevel(ProfileTierLevel *ptl, bool profileTierPresentFlag, int maxNumSubLayersMinus1)
{
  uint32_t symbol;
  if(profileTierPresentFlag)
  {
    xReadCode(7 , symbol,   "general_profile_idc"              ); ptl->setProfileIdc  (Profile::Name(symbol));
    xReadFlag(    symbol,   "general_tier_flag"                ); ptl->setTierFlag    (symbol ? Level::HIGH : Level::MAIN);
  }

  xReadCode( 8, symbol, "general_level_idc" ); ptl->setLevelIdc( Level::Name( symbol ) );
  CHECK(ptl->getProfileIdc() != Profile::NONE && ptl->getLevelIdc() < Level::LEVEL4 && ptl->getTierFlag() == Level::HIGH,
        "High tier not defined for levels below 4");

  xReadFlag(      symbol,   "ptl_frame_only_constraint_flag"   ); ptl->setFrameOnlyConstraintFlag(symbol);
  xReadFlag(      symbol,   "ptl_multilayer_enabled_flag"      ); ptl->setMultiLayerEnabledFlag(symbol);
  CHECK((ptl->getProfileIdc() == Profile::MAIN_10 || ptl->getProfileIdc() == Profile::MAIN_10_444
         || ptl->getProfileIdc() == Profile::MAIN_10_STILL_PICTURE
         || ptl->getProfileIdc() == Profile::MAIN_10_444_STILL_PICTURE)
          && symbol,
        "ptl_multilayer_enabled_flag shall be equal to 0 for non-multilayer profiles");

  if(profileTierPresentFlag)
  {
    parseConstraintInfo(ptl->getConstraintInfo(), ptl);
  }

  for (int i = maxNumSubLayersMinus1 - 1; i >= 0; i--)
  {
    xReadFlag( symbol, "sub_layer_level_present_flag[i]"   ); ptl->setSubLayerLevelPresentFlag  (i, symbol);
  }

  while (!isByteAligned())
  {
    xReadFlag(    symbol,   "ptl_reserved_zero_bit"         ); CHECK (symbol != 0, "ptl_reserved_zero_bit not equal to zero");
  }

  for (int i = maxNumSubLayersMinus1 - 1; i >= 0; i--)
  {
    if (ptl->getSubLayerLevelPresentFlag(i))
    {
      xReadCode(8 , symbol,   "sub_layer_level_idc"                ); ptl->setSubLayerLevelIdc    (i, Level::Name(symbol));
    }
  }
  ptl->setSubLayerLevelIdc(maxNumSubLayersMinus1, ptl->getLevelIdc());
  for( int i = maxNumSubLayersMinus1 - 1; i >= 0; i-- )
  {
    if( !ptl->getSubLayerLevelPresentFlag( i ) )
    {
      ptl->setSubLayerLevelIdc( i, ptl->getSubLayerLevelIdc( i + 1 ) );
    }
  }

  if (profileTierPresentFlag)
  {
    xReadCode(8, symbol, "ptl_num_sub_profiles");
    uint8_t numSubProfiles = symbol;
    ptl->setNumSubProfile(numSubProfiles);
    for (int i = 0; i < numSubProfiles; i++)
    {
      xReadCode(32, symbol, "general_sub_profile_idc[i]");
      ptl->setSubProfileIdc(i, symbol);
    }
  }
}

void HLSyntaxReader::parseTerminatingBit(uint32_t& bit)
{
  bit          = false;
  int bitsLeft = m_pcBitstream->getNumBitsLeft();
  if (bitsLeft <= 8)
  {
    uint32_t peekValue = m_pcBitstream->peekBits(bitsLeft);
    if (peekValue == (1 << (bitsLeft - 1)))
    {
      bit = true;
    }
  }
}

void HLSyntaxReader::parseRemainingBytes( bool noTrailingBytesExpected )
{
  if (noTrailingBytesExpected)
  {
    CHECK( 0 != m_pcBitstream->getNumBitsLeft(), "Bits left although no bits expected" );
  }
  else
  {
    while (m_pcBitstream->getNumBitsLeft())
    {
      uint32_t trailingNullByte=m_pcBitstream->readByte();
      if (trailingNullByte!=0)
      {
        msg( ERROR, "Trailing byte should be 0, but has value %02x\n", trailingNullByte);
        THROW("Invalid trailing '0' byte");
      }
    }
  }
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

//! parse explicit wp tables
void HLSyntaxReader::parsePredWeightTable( Slice* pcSlice, const SPS *sps )
{
  const ChromaFormat chFmt        = sps->getChromaFormatIdc();
  const int          numValidComp = int(getNumberValidComponents(chFmt));
  const bool         hasChroma    = isChromaEnabled(chFmt);

  uint32_t log2WeightDenomLuma       = 0;
  uint32_t log2WeightDenomChroma     = 0;
  uint32_t totalSignalledWeightFlags = 0;

  int deltaDenom;
  // decode delta_luma_log2_weight_denom :
  xReadUvlc(log2WeightDenomLuma, "luma_log2_weight_denom");
  CHECK(log2WeightDenomLuma > 7, "The value of luma_log2_weight_denom shall be in the range of 0 to 7");
  if (hasChroma)
  {
    xReadSvlc(deltaDenom, "delta_chroma_log2_weight_denom");
    CHECK((deltaDenom + (int) log2WeightDenomLuma) < 0,
          "luma_log2_weight_denom + delta_chroma_log2_weight_denom shall be in the range of 0 to 7");
    CHECK((deltaDenom + (int) log2WeightDenomLuma) > 7,
          "luma_log2_weight_denom + delta_chroma_log2_weight_denom shall be in the range of 0 to 7");
    log2WeightDenomChroma = (uint32_t) (deltaDenom + log2WeightDenomLuma);
  }

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const bool l0 = l == REF_PIC_LIST_0;

    if (!l0 && !pcSlice->isInterB())
    {
      continue;
    }

    for (int refIdx = 0; refIdx < pcSlice->getNumRefIdx(l); refIdx++)
    {
      WPScalingParam *wp = pcSlice->getWpScaling(l, refIdx);

      wp[COMPONENT_Y].log2WeightDenom = log2WeightDenomLuma;
      for(int j=1; j<numValidComp; j++)
      {
        wp[j].log2WeightDenom = log2WeightDenomChroma;
      }

      uint32_t  uiCode;
      xReadFlag(uiCode, (l0 ? "luma_weight_l0_flag[i]" : "luma_weight_l1_flag[i]"));
      wp[COMPONENT_Y].presentFlag = uiCode != 0;
      totalSignalledWeightFlags += wp[COMPONENT_Y].presentFlag ? 1 : 0;
    }
    if (hasChroma)
    {
      for (int refIdx = 0; refIdx < pcSlice->getNumRefIdx(l); refIdx++)
      {
        WPScalingParam *wp = pcSlice->getWpScaling(l, refIdx);
        uint32_t        uiCode;
        xReadFlag(uiCode, (l0 ? "chroma_weight_l0_flag[i]" : "chroma_weight_l1_flag[i]"));
        for(int j=1; j<numValidComp; j++)
        {
          wp[j].presentFlag = uiCode != 0;
          totalSignalledWeightFlags += wp[COMPONENT_Cb].presentFlag ? 1 : 0;
        }
      }
    }
    else
    {
      for (int refIdx = 0; refIdx < MAX_NUM_REF; refIdx++)
      {
        WPScalingParam *wp = pcSlice->getWpScaling(l, refIdx);

        wp[COMPONENT_Cb].presentFlag = false;
        wp[COMPONENT_Cr].presentFlag = false;
      }
    }
    for (int refIdx = 0; refIdx < pcSlice->getNumRefIdx(l); refIdx++)
    {
      WPScalingParam *wp = pcSlice->getWpScaling(l, refIdx);
      if (wp[COMPONENT_Y].presentFlag)
      {
        int deltaWeight;
        xReadSvlc(deltaWeight, (l0 ? "delta_luma_weight_l0[i]" : "delta_luma_weight_l1[i]"));
        CHECK(deltaWeight < -128, "delta_luma_weight_lx shall be in the rage of -128 to 127");
        CHECK(deltaWeight > 127, "delta_luma_weight_lx shall be in the rage of -128 to 127");
        wp[COMPONENT_Y].codedWeight = (deltaWeight + (1 << wp[COMPONENT_Y].log2WeightDenom));
        xReadSvlc(wp[COMPONENT_Y].codedOffset, (l0 ? "luma_offset_l0[i]" : "luma_offset_l1[i]"));
        const int range = sps->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag()
                            ? (1 << sps->getBitDepth(ChannelType::LUMA)) / 2
                            : 128;
        CHECK(wp[COMPONENT_Y].codedOffset < -range, "luma_offset_lx shall be in the rage of -128 to 127");
        CHECK(wp[COMPONENT_Y].codedOffset >= range, "luma_offset_lx shall be in the rage of -128 to 127");
      }
      else
      {
        wp[COMPONENT_Y].codedWeight = 1 << wp[COMPONENT_Y].log2WeightDenom;
        wp[COMPONENT_Y].codedOffset = 0;
      }
      if (hasChroma)
      {
        if (wp[COMPONENT_Cb].presentFlag)
        {
          int range = sps->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag()
                        ? (1 << sps->getBitDepth(ChannelType::CHROMA)) / 2
                        : 128;
          for ( int j=1 ; j<numValidComp ; j++ )
          {
            int deltaWeight;
            xReadSvlc(deltaWeight, (l0 ? "delta_chroma_weight_l0[i]" : "delta_chroma_weight_l1[i]"));
            CHECK(deltaWeight < -128, "delta_chroma_weight_lx shall be in the rage of -128 to 127");
            CHECK(deltaWeight > 127, "delta_chroma_weight_lx shall be in the rage of -128 to 127");
            wp[j].codedWeight = (deltaWeight + (1 << wp[j].log2WeightDenom));

            int deltaChroma;
            xReadSvlc(deltaChroma, (l0 ? "delta_chroma_offset_l0[i]" : "delta_chroma_offset_l1[i]"));
            CHECK(deltaChroma < -4 * range, "delta_chroma_offset_lx shall be in the range of -4 * 128 to 4 * 127");
            CHECK(deltaChroma > 4 * (range - 1), "delta_chroma_offset_lx shall be in the range of -4 * 128 to 4 * 127");
            int pred          = (range - ((range * wp[j].codedWeight) >> (wp[j].log2WeightDenom)));
            wp[j].codedOffset = Clip3(-range, range - 1, (deltaChroma + pred));
          }
        }
        else
        {
          for ( int j=1 ; j<numValidComp ; j++ )
          {
            wp[j].codedWeight = 1 << wp[j].log2WeightDenom;
            wp[j].codedOffset = 0;
          }
        }
      }
    }

    for (int refIdx = pcSlice->getNumRefIdx(l); refIdx < MAX_NUM_REF; refIdx++)
    {
      WPScalingParam *wp = pcSlice->getWpScaling(l, refIdx);

      wp[COMPONENT_Y].presentFlag  = false;
      wp[COMPONENT_Cb].presentFlag = false;
      wp[COMPONENT_Cr].presentFlag = false;
    }
  }
  CHECK(totalSignalledWeightFlags > 24, "Too many weight flag signalled");
}

void HLSyntaxReader::parsePredWeightTable(PicHeader *picHeader, const PPS *pps, const SPS *sps)
{
  const ChromaFormat chFmt        = sps->getChromaFormatIdc();
  const int          numValidComp = getNumberValidComponents(chFmt);
  const bool         chroma       = isChromaEnabled(chFmt);

  uint32_t log2WeightDenomLuma       = 0;
  uint32_t log2WeightDenomChroma     = 0;
  uint32_t totalSignalledWeightFlags = 0;
  xReadUvlc(log2WeightDenomLuma, "luma_log2_weight_denom");
  CHECK(log2WeightDenomLuma > 7, "The value of luma_log2_weight_denom shall be in the range of 0 to 7");
  if (chroma)
  {
    int deltaDenom;
    xReadSvlc(deltaDenom, "delta_chroma_log2_weight_denom");
    log2WeightDenomChroma = deltaDenom + log2WeightDenomLuma;
    CHECK(log2WeightDenomChroma > 7,
          "luma_log2_weight_denom + delta_chroma_log2_weight_denom shall be in the range of 0 to 7");
  }

  for (const auto l: { REF_PIC_LIST_0, REF_PIC_LIST_1 })
  {
    const bool l0 = l == REF_PIC_LIST_0;

    WPScalingParam *wp;

    uint32_t numLxWeights = 0;
    if (l0 || (pps->getWPBiPred() && picHeader->getRpl(l)->getNumRefEntries() > 0))
    {
      xReadUvlc(numLxWeights, (l0 ? "num_l0_weights" : "num_l1_weights"));
    }
    picHeader->setNumWeights(l, numLxWeights);

    for (int refIdx = 0; refIdx < numLxWeights; refIdx++)
    {
      wp = picHeader->getWpScaling(l, refIdx);

      wp[COMPONENT_Y].log2WeightDenom = log2WeightDenomLuma;
      for (int j = 1; j < numValidComp; j++)
      {
        wp[j].log2WeightDenom = log2WeightDenomChroma;
      }

      uint32_t uiCode;
      xReadFlag(uiCode, (l0 ? "luma_weight_l0_flag[i]" : "luma_weight_l1_flag[i]"));
      wp[COMPONENT_Y].presentFlag = uiCode != 0;
      totalSignalledWeightFlags += wp[COMPONENT_Y].presentFlag ? 1 : 0;
    }
    if (chroma)
    {
      uint32_t uiCode;
      for (int refIdx = 0; refIdx < numLxWeights; refIdx++)
      {
        wp = picHeader->getWpScaling(l, refIdx);
        xReadFlag(uiCode, (l0 ? "chroma_weight_l0_flag[i]" : "chroma_weight_l1_flag[i]"));
        for (int j = 1; j < numValidComp; j++)
        {
          wp[j].presentFlag = uiCode != 0;
          totalSignalledWeightFlags += wp[COMPONENT_Cb].presentFlag ? 1 : 0;
        }
      }
    }
    else
    {
      for ( int refIdx=0; refIdx<MAX_NUM_REF; refIdx++ )
      {
        wp = picHeader->getWpScaling(l, refIdx);

        wp[1].presentFlag = false;
        wp[2].presentFlag = false;
      }
    }
    for (int refIdx = 0; refIdx < numLxWeights; refIdx++)
    {
      wp = picHeader->getWpScaling(l, refIdx);
      if (wp[COMPONENT_Y].presentFlag)
      {
        int deltaWeight;
        xReadSvlc(deltaWeight, (l0 ? "delta_luma_weight_l0[i]" : "delta_luma_weight_l1[i]"));
        CHECK(deltaWeight < -128, "delta_luma_weight_lx shall be in the rage of -128 to 127");
        CHECK(deltaWeight > 127, "delta_luma_weight_lx shall be in the rage of -128 to 127");
        wp[COMPONENT_Y].codedWeight = (deltaWeight + (1 << wp[COMPONENT_Y].log2WeightDenom));
        xReadSvlc(wp[COMPONENT_Y].codedOffset, (l0 ? "luma_offset_l0[i]" : "luma_offset_l1[i]"));
        const int range = sps->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag()
                            ? (1 << sps->getBitDepth(ChannelType::LUMA)) / 2
                            : 128;
        CHECK(wp[0].codedOffset < -range, "luma_offset_lx shall be in the rage of -128 to 127");
        CHECK(wp[0].codedOffset >= range, "luma_offset_lx shall be in the rage of -128 to 127");
      }
      else
      {
        wp[COMPONENT_Y].codedWeight = (1 << wp[COMPONENT_Y].log2WeightDenom);
        wp[COMPONENT_Y].codedOffset = 0;
      }
      if (chroma)
      {
        if (wp[COMPONENT_Cb].presentFlag)
        {
          int range = sps->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag()
                        ? (1 << sps->getBitDepth(ChannelType::CHROMA)) / 2
                        : 128;
          for (int j = 1; j < numValidComp; j++)
          {
            int deltaWeight;
            xReadSvlc(deltaWeight, (l0 ? "delta_chroma_weight_l0[i]" : "delta_chroma_weight_l1[i]"));
            CHECK( deltaWeight < -128, "delta_chroma_weight_lx shall be in the rage of -128 to 127" );
            CHECK( deltaWeight >  127, "delta_chroma_weight_lx shall be in the rage of -128 to 127" );
            wp[j].codedWeight = (deltaWeight + (1 << wp[j].log2WeightDenom));

            int deltaChroma;
            xReadSvlc(deltaChroma, (l0 ? "delta_chroma_offset_l0[i]" : "delta_chroma_offset_l1[i]"));
            CHECK( deltaChroma <  -4*range, "delta_chroma_offset_lx shall be in the range of -4 * 128 to 4 * 127" );
            CHECK( deltaChroma >=  4*range, "delta_chroma_offset_lx shall be in the range of -4 * 128 to 4 * 127" );
            int pred          = (range - ((range * wp[j].codedWeight) >> (wp[j].log2WeightDenom)));
            wp[j].codedOffset = Clip3(-range, range - 1, (deltaChroma + pred));
          }
        }
        else
        {
          for (int j = 1; j < numValidComp; j++)
          {
            wp[j].codedWeight = (1 << wp[j].log2WeightDenom);
            wp[j].codedOffset = 0;
          }
        }
      }
    }

    for (int refIdx = numLxWeights; refIdx < MAX_NUM_REF; refIdx++)
    {
      wp = picHeader->getWpScaling(l, refIdx);

      wp[COMPONENT_Y].presentFlag  = false;
      wp[COMPONENT_Cb].presentFlag = false;
      wp[COMPONENT_Cr].presentFlag = false;
    }
  }
  CHECK(totalSignalledWeightFlags > 24, "Too many weight flag signalled");
}

/** decode quantization matrix
* \param scalingList quantization matrix information
*/
void HLSyntaxReader::parseScalingList(ScalingList *scalingList, bool aps_chromaPrsentFlag)
{
  uint32_t  code;
  bool scalingListCopyModeFlag;
  scalingList->setChromaScalingListPresentFlag(aps_chromaPrsentFlag);
  for (int scalingListId = 0; scalingListId < 28; scalingListId++)
  {
    if (aps_chromaPrsentFlag || scalingList->isLumaScalingList(scalingListId))
    {
      xReadFlag(code, "scaling_list_copy_mode_flag");
      scalingListCopyModeFlag = (code) ? true : false;
      scalingList->setScalingListCopyModeFlag(scalingListId, scalingListCopyModeFlag);

      scalingList->setScalingListPreditorModeFlag(scalingListId, false);
      if (!scalingListCopyModeFlag)
      {
        xReadFlag(code, "scaling_list_predictor_mode_flag");
        scalingList->setScalingListPreditorModeFlag(scalingListId, code);
      }

      if ((scalingListCopyModeFlag || scalingList->getScalingListPreditorModeFlag(scalingListId))
          && scalingListId != SCALING_LIST_1D_START_2x2 && scalingListId != SCALING_LIST_1D_START_4x4
          && scalingListId != SCALING_LIST_1D_START_8x8)   // Copy Mode
      {
        xReadUvlc(code, "scaling_list_pred_matrix_id_delta");
        scalingList->setRefMatrixId(scalingListId, (uint32_t) ((int) (scalingListId) - (code)));
      }
      else if (scalingListCopyModeFlag || scalingList->getScalingListPreditorModeFlag(scalingListId))
      {
        scalingList->setRefMatrixId(scalingListId, (uint32_t) ((int) (scalingListId)));
      }
      if (scalingListCopyModeFlag)   // copy
      {
        if (scalingListId >= SCALING_LIST_1D_START_16x16)
        {
          scalingList->setScalingListDC(
            scalingListId, ((scalingListId == scalingList->getRefMatrixId(scalingListId)) ? 16
                            : (scalingList->getRefMatrixId(scalingListId) < SCALING_LIST_1D_START_16x16)
                              ? scalingList->getScalingListAddress(scalingList->getRefMatrixId(scalingListId))[0]
                              : scalingList->getScalingListDC(scalingList->getRefMatrixId(scalingListId))));
        }
        scalingList->processRefMatrix(scalingListId, scalingList->getRefMatrixId(scalingListId));
      }
      else
      {
        decodeScalingList(scalingList, scalingListId, scalingList->getScalingListPreditorModeFlag(scalingListId));
      }
    }
    else
    {
      scalingListCopyModeFlag = true;
      scalingList->setScalingListCopyModeFlag(scalingListId, scalingListCopyModeFlag);
      scalingList->setRefMatrixId(scalingListId, (uint32_t)((int)(scalingListId)));
      if (scalingListId >= SCALING_LIST_1D_START_16x16)
      {
        scalingList->setScalingListDC(scalingListId, 16);
      }
      scalingList->processRefMatrix(scalingListId, scalingList->getRefMatrixId(scalingListId));
    }
  }
}

/** decode DPCM
* \param scalingList  quantization matrix information
* \param sizeId size index
* \param listId list index
*/
void HLSyntaxReader::decodeScalingList(ScalingList *scalingList, uint32_t scalingListId, bool isPredictor)
{
  int matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : (scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;
  int i, coefNum = matrixSize * matrixSize;
  int data;
  int scalingListDcCoefMinus8 = 0;
  int nextCoef = (isPredictor) ? 0 : SCALING_LIST_START_VALUE;
  ScanElement *scan = g_scanOrder[SCAN_UNGROUPED][CoeffScanType::DIAG][gp_sizeIdxInfo->idxFrom(matrixSize)][gp_sizeIdxInfo->idxFrom(matrixSize)];
  int *dst = scalingList->getScalingListAddress(scalingListId);

  int PredListId = scalingList->getRefMatrixId(scalingListId);
  CHECK(isPredictor && PredListId > scalingListId, "Scaling List error predictor!");
  const int *srcPred = (isPredictor)
                         ? ((scalingListId == PredListId) ? scalingList->getScalingListDefaultAddress(scalingListId)
                                                          : scalingList->getScalingListAddress(PredListId))
                         : nullptr;
  if(isPredictor && scalingListId == PredListId)
  {
    scalingList->setScalingListDC(PredListId, SCALING_LIST_DC);
  }
  int predCoef = 0;

  if (scalingListId >= SCALING_LIST_1D_START_16x16)
  {
    xReadSvlc(scalingListDcCoefMinus8, "scaling_list_dc_coef_minus8");
    nextCoef += scalingListDcCoefMinus8;
    if (isPredictor)
    {
      predCoef = (PredListId >= SCALING_LIST_1D_START_16x16) ? scalingList->getScalingListDC(PredListId) : srcPred[0];
    }
    scalingList->setScalingListDC(scalingListId, (nextCoef + predCoef + 256) & 255);
  }

  for(i = 0; i < coefNum; i++)
  {
    if (scalingListId >= SCALING_LIST_1D_START_64x64 && scan[i].x >= 4 && scan[i].y >= 4)
    {
      dst[scan[i].idx] = 0;
      continue;
    }
    xReadSvlc( data, "scaling_list_delta_coef");
    nextCoef += data;
    predCoef = (isPredictor) ? srcPred[scan[i].idx] : 0;
    dst[scan[i].idx] = (nextCoef + predCoef + 256) & 255;
  }
}

bool HLSyntaxReader::xMoreRbspData()
{
  int bitsLeft = m_pcBitstream->getNumBitsLeft();

  // if there are more than 8 bits, it cannot be rbsp_trailing_bits
  if (bitsLeft > 8)
  {
    return true;
  }

  uint8_t lastByte = m_pcBitstream->peekBits(bitsLeft);
  int cnt = bitsLeft;

  // remove trailing bits equal to zero
  while ((cnt>0) && ((lastByte & 1) == 0))
  {
    lastByte >>= 1;
    cnt--;
  }
  // remove bit equal to one
  cnt--;

  // we should not have a negative number of bits
  CHECK (cnt<0, "Negative number of bits");

  // we have more data, if cnt is not zero
  return (cnt>0);
}


void HLSyntaxReader::alfFilter( AlfParam& alfParam, const bool isChroma, const int altIdx )
{
  uint32_t code;

  // derive maxGolombIdx
  AlfFilterShape alfShape( isChroma ? 5 : 7 );
  const int numFilters = isChroma ? 1 : alfParam.numLumaFilters;
  AlfCoeff*      coeff      = isChroma ? alfParam.chromaCoeff[altIdx] : alfParam.lumaCoeff;
  AlfClipIdx*    clipp      = isChroma ? alfParam.chromaClipp[altIdx] : alfParam.lumaClipp;

  // Filter coefficients
  for( int ind = 0; ind < numFilters; ++ind )
  {
    for( int i = 0; i < alfShape.numCoeff - 1; i++ )
    {
      xReadUvlc( code, isChroma ? "alf_chroma_coeff_abs" : "alf_luma_coeff_abs" );
      coeff[ ind * MAX_NUM_ALF_LUMA_COEFF + i ] = code;
      if( coeff[ ind * MAX_NUM_ALF_LUMA_COEFF + i ] != 0 )
      {
        xReadFlag( code, isChroma ? "alf_chroma_coeff_sign" : "alf_luma_coeff_sign" );
        coeff[ ind * MAX_NUM_ALF_LUMA_COEFF + i ] = ( code ) ? -coeff[ ind * MAX_NUM_ALF_LUMA_COEFF + i ] : coeff[ ind * MAX_NUM_ALF_LUMA_COEFF + i ];
      }
      CHECK( isChroma &&
             ( coeff[ind * MAX_NUM_ALF_LUMA_COEFF + i] > 127 || coeff[ind * MAX_NUM_ALF_LUMA_COEFF + i] < -128 )
             , "AlfCoeffC shall be in the range of -128 to 127, inclusive" );
    }
  }

  // Clipping values coding
  if (alfParam.nonLinearFlag[isChroma ? ChannelType::CHROMA : ChannelType::LUMA])
  {
    // Filter coefficients
    for( int ind = 0; ind < numFilters; ++ind )
    {
      for( int i = 0; i < alfShape.numCoeff - 1; i++ )
      {
        xReadCode(2, code, isChroma ? "alf_chroma_clip_idx" : "alf_luma_clip_idx");
        clipp[ind * MAX_NUM_ALF_LUMA_COEFF + i] = code;
      }
    }
  }
  else
  {
    for( int ind = 0; ind < numFilters; ++ind )
    {
      std::fill_n( clipp + ind * MAX_NUM_ALF_LUMA_COEFF, alfShape.numCoeff, 0 );
    }
  }
}


//! \}

