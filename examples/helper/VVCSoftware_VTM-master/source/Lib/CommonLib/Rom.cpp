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

/** \file     Rom.cpp
    \brief    global variables & functions
*/

#include "Rom.h"
#include "UnitTools.h"

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <iomanip>

constexpr int MmvdIdx::ADD_NUM;
constexpr int MmvdIdx::BASE_MV_NUM;

// ====================================================================================================================
// Initialize / destroy functions
// ====================================================================================================================

#if ENABLE_TRACING
CDTrace *g_trace_ctx = nullptr;
#endif
bool g_mctsDecCheckEnabled = false;

//! \ingroup CommonLib
//! \{

MsgLevel g_verbosity = VERBOSE;

const char* nalUnitTypeToString(NalUnitType type)
{
  switch (type)
  {
  case NAL_UNIT_CODED_SLICE_TRAIL:      return "TRAIL";
  case NAL_UNIT_CODED_SLICE_STSA:       return "STSA";
  case NAL_UNIT_CODED_SLICE_RADL:       return "RADL";
  case NAL_UNIT_CODED_SLICE_RASL:       return "RASL";
  case NAL_UNIT_CODED_SLICE_IDR_W_RADL: return "IDR_W_RADL";
  case NAL_UNIT_CODED_SLICE_IDR_N_LP:   return "IDR_N_LP";
  case NAL_UNIT_CODED_SLICE_CRA:        return "CRA";
  case NAL_UNIT_CODED_SLICE_GDR:        return "GDR";
  case NAL_UNIT_OPI:                    return "OPI";
  case NAL_UNIT_DCI:                    return "DCI";
  case NAL_UNIT_VPS:                    return "VPS";
  case NAL_UNIT_SPS:                    return "SPS";
  case NAL_UNIT_PPS:                    return "PPS";
  case NAL_UNIT_PREFIX_APS:             return "Prefix APS";
  case NAL_UNIT_SUFFIX_APS:             return "Suffix APS";
  case NAL_UNIT_PH:                     return "PH";
  case NAL_UNIT_ACCESS_UNIT_DELIMITER:  return "AUD";
  case NAL_UNIT_EOS:                    return "EOS";
  case NAL_UNIT_EOB:                    return "EOB";
  case NAL_UNIT_PREFIX_SEI:             return "Prefix SEI";
  case NAL_UNIT_SUFFIX_SEI:             return "Suffix SEI";
  case NAL_UNIT_FD:                     return "FD";
  default:                              return "UNK";

  }
}

class ScanGenerator
{
private:
  uint32_t m_line, m_column;
  const uint32_t m_blockWidth, m_blockHeight;
  const uint32_t m_stride;
  const CoeffScanType m_scanType;

public:
  ScanGenerator(uint32_t blockWidth, uint32_t blockHeight, uint32_t stride, CoeffScanType scanType)
    : m_line(0), m_column(0), m_blockWidth(blockWidth), m_blockHeight(blockHeight), m_stride(stride), m_scanType(scanType)
  { }

  uint32_t GetCurrentX() const { return m_column; }
  uint32_t GetCurrentY() const { return m_line; }

  uint32_t GetNextIndex(uint32_t blockOffsetX, uint32_t blockOffsetY)
  {
    const uint32_t rtn = ((m_line + blockOffsetY) * m_stride) + m_column + blockOffsetX;

    //advance line and column to the next position
    switch (m_scanType)
    {
      //------------------------------------------------

      case CoeffScanType::DIAG:

        if ((m_column == m_blockWidth - 1) || (m_line == 0)) //if we reach the end of a rank, go diagonally down to the next one
        {
          m_line += m_column + 1;
          m_column = 0;

          if (m_line >= m_blockHeight) //if that takes us outside the block, adjust so that we are back on the bottom row
          {
            m_column += m_line - (m_blockHeight - 1);
            m_line = m_blockHeight - 1;
          }
        }
        else
        {
          m_column++;
          m_line--;
        }
        break;

      case CoeffScanType::TRAV_HOR:
        if (m_line % 2 == 0)
        {
          if (m_column == (m_blockWidth - 1))
          {
            m_line++;
            m_column = m_blockWidth - 1;
          }
          else
          {
            m_column++;
          }
        }
        else
        {
          if (m_column == 0)
          {
            m_line++;
            m_column = 0;
          }
          else
          {
            m_column--;
          }
        }
        break;

      case CoeffScanType::TRAV_VER:
        if (m_column % 2 == 0)
        {
          if (m_line == (m_blockHeight - 1))
          {
            m_column++;
            m_line = m_blockHeight - 1;
          }
          else
          {
            m_line++;
          }
        }
        else
        {
          if (m_line == 0)
          {
            m_column++;
            m_line = 0;
          }
          else
          {
            m_line--;
          }
        }
        break;
      //------------------------------------------------

      default:

        THROW("ERROR: Unknown scan type \"" << to_underlying(m_scanType) << "\"in ScanGenerator::GetNextIndex");
        break;
    }

    return rtn;
  }
};

const int8_t g_BcwWeights[BCW_NUM] = { -2, 3, 4, 5, 10 };
const int8_t g_BcwSearchOrder[BCW_NUM] = { BCW_DEFAULT, BCW_DEFAULT - 2, BCW_DEFAULT + 2, BCW_DEFAULT - 1, BCW_DEFAULT + 1 };
int8_t g_BcwCodingOrder[BCW_NUM];
int8_t g_BcwParsingOrder[BCW_NUM];

int8_t getBcwWeight(uint8_t bcwIdx, uint8_t refFrameList)
{
  // Weights for the model: p0 + w * (p1 - p0) = (1-w) * p0 + w * p1
  // Retuning  1-w for p0 or w for p1
  return (refFrameList == REF_PIC_LIST_0 ? BCW_WEIGHT_BASE - g_BcwWeights[bcwIdx] : g_BcwWeights[bcwIdx]);
}

void resetBcwCodingOrder(bool runDecoding, const CodingStructure &cs)
{
  // Form parsing order: { BCW_DEFAULT, BCW_DEFAULT+1, BCW_DEFAULT-1, BCW_DEFAULT+2, BCW_DEFAULT-2, ... }
  g_BcwParsingOrder[0] = BCW_DEFAULT;
  for (int i = 1; i <= (BCW_NUM >> 1); ++i)
  {
    g_BcwParsingOrder[2 * i - 1] = BCW_DEFAULT + (int8_t)i;
    g_BcwParsingOrder[2 * i] = BCW_DEFAULT - (int8_t)i;
  }

  // Form encoding order
  if (!runDecoding)
  {
    for (int i = 0; i < BCW_NUM; ++i)
    {
      g_BcwCodingOrder[(uint32_t)g_BcwParsingOrder[i]] = i;
    }
  }
}

uint32_t deriveWeightIdxBits(uint8_t bcwIdx) // Note: align this with TEncSbac::codeBcwIdx and TDecSbac::parseBcwIdx
{
  uint32_t numBits = 1;
  uint8_t  bcwCodingIdx = (uint8_t)g_BcwCodingOrder[bcwIdx];

  if (BCW_NUM > 2 && bcwCodingIdx != 0)
  {
    uint32_t prefixNumBits = BCW_NUM - 2;
    uint32_t step = 1;
    uint8_t  prefixSymbol = bcwCodingIdx;

    // Truncated unary code
    uint8_t idx = 1;
    for (int ui = 0; ui < prefixNumBits; ++ui)
    {
      if (prefixSymbol == idx)
      {
        ++numBits;
        break;
      }
      else
      {
        ++numBits;
        idx += step;
      }
    }
  }
  return numBits;
}

Size g_log2TxSubblockSize[MAX_CU_DEPTH + 1][MAX_CU_DEPTH + 1] = {
  { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 4 }, { 0, 4 }, { 0, 4 } },
  { { 1, 0 }, { 1, 1 }, { 1, 1 }, { 1, 3 }, { 1, 3 }, { 1, 3 }, { 1, 3 }, { 1, 3 } },
  { { 2, 0 }, { 1, 1 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 } },
  { { 3, 0 }, { 3, 1 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 } },
  { { 4, 0 }, { 3, 1 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 } },
  { { 4, 0 }, { 3, 1 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 } },
  { { 4, 0 }, { 3, 1 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 } },
  { { 4, 0 }, { 3, 1 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 } }
};

// initialize ROM variables
void initROM()
{
  gp_sizeIdxInfo = new SizeIndexInfoLog2();
  gp_sizeIdxInfo->init(MAX_CU_SIZE);


  SizeIndexInfoLog2 sizeInfo;
  sizeInfo.init(MAX_CU_SIZE);

  // initialize scan orders
  for (uint32_t blockHeightIdx = 0; blockHeightIdx < sizeInfo.numAllHeights(); blockHeightIdx++)
  {
    for (uint32_t blockWidthIdx = 0; blockWidthIdx < sizeInfo.numAllWidths(); blockWidthIdx++)
    {
      const uint32_t blockWidth  = sizeInfo.sizeFrom(blockWidthIdx);
      const uint32_t blockHeight = sizeInfo.sizeFrom(blockHeightIdx);
      const uint32_t totalValues = blockWidth * blockHeight;

      //--------------------------------------------------------------------------------------------------

      //non-grouped scan orders

      for (auto scanType = CoeffScanType::DIAG; scanType < CoeffScanType::NUM; scanType++)
      {
        ScanElement *scan = nullptr;

        if (blockWidthIdx < sizeInfo.numWidths() && blockHeightIdx < sizeInfo.numHeights())
        {
          scan = new ScanElement[totalValues];
        }

        g_scanOrder[SCAN_UNGROUPED][scanType][blockWidthIdx][blockHeightIdx] = scan;

        if (scan == nullptr)
        {
          continue;
        }

        ScanGenerator fullBlockScan(blockWidth, blockHeight, blockWidth, scanType);

        for (uint32_t scanPosition = 0; scanPosition < totalValues; scanPosition++)
        {
          const int rasterPos = fullBlockScan.GetNextIndex( 0, 0 );
          const int posY      = rasterPos / blockWidth;
          const int posX      = rasterPos - ( posY * blockWidth );

          scan[scanPosition].idx = rasterPos;
          scan[scanPosition].x   = posX;
          scan[scanPosition].y   = posY;
        }
      }

      //--------------------------------------------------------------------------------------------------

      //grouped scan orders
      const Size    &log2TxSubblockSize = g_log2TxSubblockSize[floorLog2(blockWidth)][floorLog2(blockHeight)];
      const uint32_t log2CGWidth        = log2TxSubblockSize.width;
      const uint32_t log2CGHeight       = log2TxSubblockSize.height;

      const uint32_t  groupWidth     = 1 << log2CGWidth;
      const uint32_t  groupHeight    = 1 << log2CGHeight;
      const uint32_t  widthInGroups  = getNonzeroTuSize(blockWidth) >> log2CGWidth;
      const uint32_t  heightInGroups = getNonzeroTuSize(blockHeight) >> log2CGHeight;

      const uint32_t  groupSize      = groupWidth    * groupHeight;
      const uint32_t  totalGroups    = widthInGroups * heightInGroups;

      for (auto scanType = CoeffScanType::DIAG; scanType < CoeffScanType::NUM; scanType++)
      {
        ScanElement *scan = new ScanElement[totalValues];

        g_scanOrder[SCAN_GROUPED_4x4][scanType][blockWidthIdx][blockHeightIdx] = scan;

        if (blockWidth > MAX_NONZERO_TU_SIZE || blockHeight > MAX_NONZERO_TU_SIZE)
        {
          for (uint32_t i = 0; i < totalValues; i++)
          {
            scan[i].idx = totalValues - 1;
            scan[i].x   = blockWidth - 1;
            scan[i].y   = blockHeight - 1;
          }
        }

        ScanGenerator fullBlockScan(widthInGroups, heightInGroups, groupWidth, scanType);

        for (uint32_t groupIndex = 0; groupIndex < totalGroups; groupIndex++)
        {
          const uint32_t groupPositionY  = fullBlockScan.GetCurrentY();
          const uint32_t groupPositionX  = fullBlockScan.GetCurrentX();
          const uint32_t groupOffsetX    = groupPositionX * groupWidth;
          const uint32_t groupOffsetY    = groupPositionY * groupHeight;
          const uint32_t groupOffsetScan = groupIndex     * groupSize;

          ScanGenerator groupScan(groupWidth, groupHeight, blockWidth, scanType);

          for (uint32_t scanPosition = 0; scanPosition < groupSize; scanPosition++)
          {
            const int rasterPos = groupScan.GetNextIndex( groupOffsetX, groupOffsetY );
            const int posY      = rasterPos / blockWidth;
            const int posX      = rasterPos - ( posY * blockWidth );

            scan[groupOffsetScan + scanPosition].idx = rasterPos;
            scan[groupOffsetScan + scanPosition].x   = posX;
            scan[groupOffsetScan + scanPosition].y   = posY;
          }

          fullBlockScan.GetNextIndex(0, 0);
        }
      }

      //--------------------------------------------------------------------------------------------------
    }
  }

  // initialize CoefTopLeftDiagScan8x8 for LFNST
  for( uint32_t blockWidthIdx = 0; blockWidthIdx < sizeInfo.numAllWidths(); blockWidthIdx++ )
  {
    const uint32_t blockWidth = sizeInfo.sizeFrom( blockWidthIdx );

    const static uint8_t g_auiXYDiagScan8x8[ 64 ][ 2 ] =
    {
      { 0, 0 }, { 0, 1 }, { 1, 0 }, { 0, 2 }, { 1, 1 }, { 2, 0 }, { 0, 3 }, { 1, 2 },
      { 2, 1 }, { 3, 0 }, { 1, 3 }, { 2, 2 }, { 3, 1 }, { 2, 3 }, { 3, 2 }, { 3, 3 },
      { 0, 4 }, { 0, 5 }, { 1, 4 }, { 0, 6 }, { 1, 5 }, { 2, 4 }, { 0, 7 }, { 1, 6 },
      { 2, 5 }, { 3, 4 }, { 1, 7 }, { 2, 6 }, { 3, 5 }, { 2, 7 }, { 3, 6 }, { 3, 7 },
      { 4, 0 }, { 4, 1 }, { 5, 0 }, { 4, 2 }, { 5, 1 }, { 6, 0 }, { 4, 3 }, { 5, 2 },
      { 6, 1 }, { 7, 0 }, { 5, 3 }, { 6, 2 }, { 7, 1 }, { 6, 3 }, { 7, 2 }, { 7, 3 },
      { 4, 4 }, { 4, 5 }, { 5, 4 }, { 4, 6 }, { 5, 5 }, { 6, 4 }, { 4, 7 }, { 5, 6 },
      { 6, 5 }, { 7, 4 }, { 5, 7 }, { 6, 6 }, { 7, 5 }, { 6, 7 }, { 7, 6 }, { 7, 7 }
    };
    for( int i = 0; i < 64; i++ )
    {
      g_coefTopLeftDiagScan8x8[ blockWidthIdx ][ i ].idx = g_auiXYDiagScan8x8[ i ][ 0 ] + g_auiXYDiagScan8x8[ i ][ 1 ] * blockWidth;
      g_coefTopLeftDiagScan8x8[ blockWidthIdx ][ i ].x   = g_auiXYDiagScan8x8[ i ][ 0 ];
      g_coefTopLeftDiagScan8x8[ blockWidthIdx ][ i ].y   = g_auiXYDiagScan8x8[ i ][ 1 ];
    }
  }

  initGeoTemplate();

  ::memset(g_isReusedUniMVsFilled, 0, sizeof(g_isReusedUniMVsFilled));

  for (int qp = 0; qp < 57; qp++)
  {
    int qpRem = (qp + 12) % 6;
    int qpPer = (qp + 12) / 6;
    int quantiserScale = g_quantScales[0][qpRem];
    int quantiserRightShift = QUANT_SHIFT + qpPer;
    double threshQP = ((double)(1 << quantiserRightShift)) / quantiserScale;
    g_paletteQuant[qp] = (int)(threshQP*0.16 + 0.5);
  }
}

void destroyROM()
{
  unsigned numWidths = gp_sizeIdxInfo->numAllWidths();
  unsigned numHeights = gp_sizeIdxInfo->numAllHeights();

  for (uint32_t groupTypeIndex = 0; groupTypeIndex < SCAN_NUMBER_OF_GROUP_TYPES; groupTypeIndex++)
  {
    for (auto scanOrder = CoeffScanType::DIAG; scanOrder < CoeffScanType::NUM; scanOrder++)
    {
      for (uint32_t blockWidthIdx = 0; blockWidthIdx <= numWidths; blockWidthIdx++)
      {
        for (uint32_t blockHeightIdx = 0; blockHeightIdx <= numHeights; blockHeightIdx++)
        {
          delete[] g_scanOrder[groupTypeIndex][scanOrder][blockWidthIdx][blockHeightIdx];
          g_scanOrder[groupTypeIndex][scanOrder][blockWidthIdx][blockHeightIdx] = nullptr;
        }
      }
    }
  }

  delete gp_sizeIdxInfo;
  gp_sizeIdxInfo = nullptr;

  for( int i = 0; i < GEO_NUM_PRESTORED_MASK; i++ )
  {
    delete[] g_globalGeoWeights   [i];
    delete[] g_globalGeoEncSADmask[i];
    g_globalGeoWeights   [i] = nullptr;
    g_globalGeoEncSADmask[i] = nullptr;
  }
}

// ====================================================================================================================
// Data structure related table & variable
// ====================================================================================================================

const int g_quantScales[2][SCALING_LIST_REM_NUM] = // can be represented as a 9 element table
{
    { 26214,23302,20560,18396,16384,14564 },
    { 18396,16384,14564,13107,11651,10280 } // Note: last 3 values of second row == half of the first 3 values of the first row
};

const int g_invQuantScales[2][SCALING_LIST_REM_NUM] = // can be represented as a 9 element table
{
  { 40,45,51,57,64,72 },
  { 57,64,72,80,90,102 } // Note: last 3 values of second row == double of the first 3 values of the first row
};

//--------------------------------------------------------------------------------------------------
//structures
//--------------------------------------------------------------------------------------------------
//coefficients
//--------------------------------------------------------------------------------------------------
// ====================================================================================================================
// Intra prediction
// ====================================================================================================================

const uint8_t g_intraModeNumFastUseMPM2D[7 - MIN_CU_LOG2 + 1][7 - MIN_CU_LOG2 + 1] = {
  { 3, 3, 3, 3, 2, 2 },   //   4x4,   4x8,   4x16,   4x32,   4x64,   4x128,
  { 3, 3, 3, 3, 3, 2 },   //   8x4,   8x8,   8x16,   8x32,   8x64,   8x128,
  { 3, 3, 3, 3, 3, 2 },   //  16x4,  16x8,  16x16,  16x32,  16x64,  16x128,
  { 3, 3, 3, 3, 3, 2 },   //  32x4,  32x8,  32x16,  32x32,  32x64,  32x128,
  { 2, 3, 3, 3, 3, 2 },   //  64x4,  64x8,  64x16,  64x32,  64x64,  64x128,
  { 2, 2, 2, 2, 2, 3 },   // 128x4, 128x8, 128x16, 128x32, 128x64, 128x128,
};

const uint8_t g_chroma422IntraAngleMappingTable[NUM_INTRA_MODE] =
//                                    *                                H                              *                                D      *   *   *   *       *   *   *                   *        V       *                   *   *   *      *   *   *   *
//0, 1,  2,  3,  4,  5,  6,  7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, DM
{ 0, 1, 61, 62, 63, 64, 65, 66, 2, 3,  5,  6,  8, 10, 12, 13, 14, 16, 18, 20, 22, 23, 24, 26, 28, 30, 31, 33, 34, 35, 36, 37, 38, 39, 40, 41, 41, 42, 43, 43, 44, 44, 45, 45, 46, 47, 48, 48, 49, 49, 50, 51, 51, 52, 52, 53, 54, 55, 55, 56, 56, 57, 57, 58, 59, 59, 60, DM_CHROMA_IDX };



// ====================================================================================================================
// Misc.
// ====================================================================================================================
SizeIndexInfo *gp_sizeIdxInfo = nullptr;

const int                 g_ictModes[2][4] = { { 0, 3, 1, 2 }, { 0, -3, -1, -2 } };

UnitScale g_miScaling( MIN_CU_LOG2, MIN_CU_LOG2 );


// ====================================================================================================================
// Scanning order & context model mapping
// ====================================================================================================================
const std::array<TCoeff, 4> g_riceThreshold = { 32, 128, 512, 2048 };

const std::array<uint8_t, g_riceThreshold.size() + 1> g_riceShift = { 0, 2, 4, 6, 8 };

// scanning order table
EnumArray<ScanElement *[MAX_CU_SIZE / 2 + 1][MAX_CU_SIZE / 2 + 1], CoeffScanType> g_scanOrder[SCAN_NUMBER_OF_GROUP_TYPES];
ScanElement  g_coefTopLeftDiagScan8x8[ MAX_CU_SIZE / 2 + 1 ][ 64 ];

const uint32_t g_minInGroup[LAST_SIGNIFICANT_GROUPS] = { 0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96 };

const uint32_t g_groupIdx[MAX_TB_SIZEY] = { 0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,
                                            8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9,
                                            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                                            11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11 };

const uint32_t g_goRiceParsCoeff[32] = { 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2,
                                         2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3 };

const char *matrixType[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM] = {
  { "INTRA1X1_LUMA", "INTRA1X1_CHROMAU", "INTRA1X1_CHROMAV", "INTER1X1_LUMA", "INTER1X1_CHROMAU", "INTER1X1_CHROMAV" },
  { "INTRA2X2_LUMA", "INTRA2X2_CHROMAU", "INTRA2X2_CHROMAV", "INTER2X2_LUMA", "INTER2X2_CHROMAU", "INTER2X2_CHROMAV" },
  { "INTRA4X4_LUMA", "INTRA4X4_CHROMAU", "INTRA4X4_CHROMAV", "INTER4X4_LUMA", "INTER4X4_CHROMAU", "INTER4X4_CHROMAV" },
  { "INTRA8X8_LUMA", "INTRA8X8_CHROMAU", "INTRA8X8_CHROMAV", "INTER8X8_LUMA", "INTER8X8_CHROMAU", "INTER8X8_CHROMAV" },
  { "INTRA16X16_LUMA", "INTRA16X16_CHROMAU", "INTRA16X16_CHROMAV", "INTER16X16_LUMA", "INTER16X16_CHROMAU",
    "INTER16X16_CHROMAV" },
  { "INTRA32X32_LUMA", "INTRA32X32_CHROMAU", "INTRA32X32_CHROMAV", "INTER32X32_LUMA", "INTER32X32_CHROMAU",
    "INTER32X32_CHROMAV" },
  { "INTRA64X64_LUMA", "INTRA64X64_CHROMAU", "INTRA64X64_CHROMAV", "INTER64X64_LUMA", "INTER64X64_CHROMAU",
    "INTER64X64_CHROMAV" },
  {},
};

const char *matrixTypeDc[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM] = {
  {
    // 1x1
  },
  {},
  {},
  {},
  { "INTRA16X16_LUMA_DC", "INTRA16X16_CHROMAU_DC", "INTRA16X16_CHROMAV_DC", "INTER16X16_LUMA_DC",
    "INTER16X16_CHROMAU_DC", "INTER16X16_CHROMAV_DC" },
  { "INTRA32X32_LUMA_DC", "INTRA32X32_CHROMAU_DC", "INTRA32X32_CHROMAV_DC", "INTER32X32_LUMA_DC",
    "INTER32X32_CHROMAU_DC", "INTER32X32_CHROMAV_DC" },
  { "INTRA64X64_LUMA_DC", "INTRA64X64_CHROMAU_DC", "INTRA64X64_CHROMAV_DC", "INTER64X64_LUMA_DC",
    "INTER64X64_CHROMAU_DC", "INTER64X64_CHROMAV_DC" },
  {},
};

const int g_quantTSDefault4x4[4 * 4] =
{
  16,16,16,16,
  16,16,16,16,
  16,16,16,16,
  16,16,16,16
};

const int g_quantIntraDefault8x8[8 * 8] =
{
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16
};

const int g_quantInterDefault8x8[8 * 8] =
{
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16
};

const uint32_t g_scalingListSize [SCALING_LIST_SIZE_NUM] = { 1, 4, 16, 64, 256, 1024, 4096, 16384 };
const uint32_t g_scalingListSizeX[SCALING_LIST_SIZE_NUM] = { 1, 2,  4,  8,  16,   32,   64,   128 };

const uint32_t g_scalingListId[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM] =
{
  {  0,  0,  0,  0,  0,  0},  // SCALING_LIST_1x1
  {  0,  0,  0,  0,  0,  1},  // SCALING_LIST_2x2
  {  2,  3,  4,  5,  6,  7},  // SCALING_LIST_4x4
  {  8,  9, 10, 11, 12, 13},  // SCALING_LIST_8x8
  { 14, 15, 16, 17, 18, 19},  // SCALING_LIST_16x16
  { 20, 21, 22, 23, 24, 25},  // SCALING_LIST_32x32
  { 26, 21, 22, 27, 24, 25},  // SCALING_LIST_64x64
  {  0,  0,  0,  0,  0,  0},  // SCALING_LIST_128x128
};

RefSetArray<Mv> g_reusedUniMVs[MAX_CU_SIZE_IN_PARTS][MAX_CU_SIZE_IN_PARTS][MAX_NUM_SIZES][MAX_NUM_SIZES];
bool g_isReusedUniMVsFilled[MAX_CU_SIZE_IN_PARTS][MAX_CU_SIZE_IN_PARTS][MAX_NUM_SIZES][MAX_NUM_SIZES];

uint16_t g_paletteQuant[57];
uint8_t g_paletteRunTopLut [5] = { 0, 1, 1, 2, 2 };
uint8_t g_paletteRunLeftLut[5] = { 0, 1, 2, 3, 4 };

void initGeoTemplate()
{
  int modeIdx = 0;
  for( int angleIdx = 0; angleIdx < GEO_NUM_ANGLES; angleIdx++ )
  {
    for( int distanceIdx = 0; distanceIdx < GEO_NUM_DISTANCES; distanceIdx++ )
    {
      if( (distanceIdx == 0 && angleIdx >= 16)
        || ((distanceIdx == 2 || distanceIdx == 0) && (g_angle2mask[angleIdx] == 0 || g_angle2mask[angleIdx] == 5))
        || g_angle2mask[angleIdx] == -1 )
      {
        continue;
      }
      g_geoParams[modeIdx].angleIdx    = angleIdx;
      g_geoParams[modeIdx].distanceIdx = distanceIdx;
      modeIdx++;
    }
  }
  for (int angleIdx = 0; angleIdx < (GEO_NUM_ANGLES >> 2) + 1; angleIdx++)
  {
    if (g_angle2mask[angleIdx] == -1)
    {
      continue;
    }
    g_globalGeoWeights[g_angle2mask[angleIdx]] = new int16_t[GEO_WEIGHT_MASK_SIZE * GEO_WEIGHT_MASK_SIZE];
    g_globalGeoEncSADmask[g_angle2mask[angleIdx]] = new Pel[GEO_WEIGHT_MASK_SIZE * GEO_WEIGHT_MASK_SIZE];

    int distanceX = angleIdx;
    int distanceY = (distanceX + (GEO_NUM_ANGLES >> 2)) % GEO_NUM_ANGLES;

    int16_t rho = (g_dis[distanceX] * 2 * GEO_MAX_CU_SIZE) + (g_dis[distanceY] * 2 * GEO_MAX_CU_SIZE);

    constexpr int16_t maskOffset = (2*GEO_MAX_CU_SIZE - GEO_WEIGHT_MASK_SIZE) >> 1;
    int index = 0;
    for( int y = 0; y < GEO_WEIGHT_MASK_SIZE; y++ )
    {
      int16_t lookUpY = (((y + maskOffset) << 1) + 1) * g_dis[distanceY];
      for( int x = 0; x < GEO_WEIGHT_MASK_SIZE; x++, index++ )
      {
        int16_t sx_i = ((x + maskOffset) << 1) + 1;
        int16_t weightIdx                                    = sx_i * g_dis[distanceX] + lookUpY - rho;
        int weightLinearIdx = 32 + weightIdx;
        g_globalGeoWeights[g_angle2mask[angleIdx]][index] = Clip3(0, 8, (weightLinearIdx + 4) >> 3);
        g_globalGeoEncSADmask[g_angle2mask[angleIdx]][index] = weightIdx > 0 ? 1 : 0;
      }
    }
  }

  for( int hIdx = 0; hIdx < GEO_NUM_CU_SIZE; hIdx++ )
  {
    const int height = 1 << (hIdx + GEO_MIN_CU_LOG2);
    for( int wIdx = 0; wIdx < GEO_NUM_CU_SIZE; wIdx++ )
    {
      const int width = 1 << (wIdx + GEO_MIN_CU_LOG2);
      for( int splitDir = 0; splitDir < GEO_NUM_PARTITION_MODE; splitDir++ )
      {
        const int angle    = g_geoParams[splitDir].angleIdx;
        const int distance = g_geoParams[splitDir].distanceIdx;

        int offsetX = (GEO_WEIGHT_MASK_SIZE - width) >> 1;
        int offsetY = (GEO_WEIGHT_MASK_SIZE - height) >> 1;

        if( distance > 0 )
        {
          const int angleMod = angle % (GEO_NUM_ANGLES / 2);
          if (angleMod == GEO_NUM_ANGLES / 4 || (angleMod != 0 && height >= width))
          {
            const int d = distance * height >> 3;
            offsetY += angle < GEO_NUM_ANGLES / 2 ? d : -d;
          }
          else
          {
            const int d = distance * width >> 3;
            offsetX += angle < GEO_NUM_ANGLES / 2 ? d : -d;
          }
        }
        g_weightOffset[splitDir][hIdx][wIdx][0] = offsetX;
        g_weightOffset[splitDir][hIdx][wIdx][1] = offsetY;
      }
    }
  }
}

GeoParam g_geoParams[GEO_NUM_PARTITION_MODE];

int16_t*  g_globalGeoWeights   [GEO_NUM_PRESTORED_MASK];
Pel*      g_globalGeoEncSADmask[GEO_NUM_PRESTORED_MASK];
int16_t   g_weightOffset       [GEO_NUM_PARTITION_MODE][GEO_NUM_CU_SIZE][GEO_NUM_CU_SIZE][2];
int8_t    g_angle2mask[GEO_NUM_ANGLES] = { 0, -1, 1, 2, 3, 4, -1, -1, 5, -1, -1, 4, 3, 2, 1, -1, 0, -1, 1, 2, 3, 4, -1, -1, 5, -1, -1, 4, 3, 2, 1, -1 };

int8_t g_dis[GEO_NUM_ANGLES] = { 8,  8,  8,  8,  4,  4,  2,  1,  0, -1, -2, -4, -4, -8, -8, -8,
                                 -8, -8, -8, -8, -4, -4, -2, -1, 0, 1,  2,  4,  4,  8,  8,  8 };

int8_t    g_angle2mirror[GEO_NUM_ANGLES] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 2 };
//! \}
