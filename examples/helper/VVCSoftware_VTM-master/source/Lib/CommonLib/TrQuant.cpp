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

/** \file     TrQuant.cpp
    \brief    transform and quantization class
*/

#include "TrQuant.h"
#include "TrQuant_EMT.h"

#include "UnitTools.h"
#include "ContextModelling.h"
#include "CodingStructure.h"

#include "dtrace_buffer.h"

#include <stdlib.h>
#include <limits>
#include <memory.h>

#include "QuantRDOQ.h"
#include "DepQuant.h"

#if RExt__DECODER_DEBUG_TOOL_STATISTICS
#include "CommonLib/CodingStatistics.h"
#endif

struct coeffGroupRDStats
{
  int    iNNZbeforePos0;
  double d64CodedLevelandDist; // distortion and level cost only
  double d64UncodedDist;    // all zero coded block distortion
  double d64SigCost;
  double d64SigCost_0;
};

//! \ingroup CommonLib
//! \{

static inline int64_t square( const int d ) { return d * (int64_t)d; }

template<int signedMode> std::pair<int64_t,int64_t> fwdTransformCbCr( const PelBuf &resCb, const PelBuf &resCr, PelBuf& resC1, PelBuf& resC2 )
{
  const Pel*  cb  = resCb.buf;
  const Pel*  cr  = resCr.buf;
  Pel*        c1  = resC1.buf;
  Pel*        c2  = resC2.buf;
  int64_t     d1  = 0;
  int64_t     d2  = 0;
  for( SizeType y = 0; y < resCb.height; y++, cb += resCb.stride, cr += resCr.stride, c1 += resC1.stride, c2 += resC2.stride )
  {
    for( SizeType x = 0; x < resCb.width; x++ )
    {
      int cbx = cb[x], crx = cr[x];
      if      ( signedMode ==  1 )
      {
        c1[x] = Pel( ( 4*cbx + 2*crx ) / 5 );
        d1   += square( cbx - c1[x] ) + square( crx - (c1[x]>>1) );
      }
      else if ( signedMode == -1 )
      {
        c1[x] = Pel( ( 4*cbx - 2*crx ) / 5 );
        d1   += square( cbx - c1[x] ) + square( crx - (-c1[x]>>1) );
      }
      else if ( signedMode ==  2 )
      {
        c1[x] = Pel( ( cbx + crx ) / 2 );
        d1   += square( cbx - c1[x] ) + square( crx - c1[x] );
      }
      else if ( signedMode == -2 )
      {
        c1[x] = Pel( ( cbx - crx ) / 2 );
        d1   += square( cbx - c1[x] ) + square( crx + c1[x] );
      }
      else if ( signedMode ==  3 )
      {
        c2[x] = Pel( ( 4*crx + 2*cbx ) / 5 );
        d1   += square( cbx - (c2[x]>>1) ) + square( crx - c2[x] );
      }
      else if ( signedMode == -3 )
      {
        c2[x] = Pel( ( 4*crx - 2*cbx ) / 5 );
        d1   += square( cbx - (-c2[x]>>1) ) + square( crx - c2[x] );
      }
      else
      {
        d1   += square( cbx );
        d2   += square( crx );
      }
    }
  }
  return std::make_pair(d1,d2);
}

template<int signedMode> void invTransformCbCr( PelBuf &resCb, PelBuf &resCr )
{
  Pel*  cb  = resCb.buf;
  Pel*  cr  = resCr.buf;
  for( SizeType y = 0; y < resCb.height; y++, cb += resCb.stride, cr += resCr.stride )
  {
    for( SizeType x = 0; x < resCb.width; x++ )
    {
      if (signedMode == 1)
      {
        cr[x] = cb[x] >> 1;
      }
      else if (signedMode == -1)
      {
        cr[x] = -cb[x] >> 1;
      }
      else if (signedMode == 2)
      {
        cr[x] = cb[x];
      }
      else if (signedMode == -2)
      {
        // non-normative clipping to prevent 16-bit overflow
        cr[x] = (cb[x] == -32768 && sizeof(Pel) == 2) ? 32767 : -cb[x];
      }
      else if (signedMode == 3)
      {
        cb[x] = cr[x] >> 1;
      }
      else if (signedMode == -3)
      {
        cb[x] = -cr[x] >> 1;
      }
    }
  }
}

// ====================================================================================================================
// TrQuant class member functions
// ====================================================================================================================
TrQuant::TrQuant() : m_quant( nullptr )
{
  // allocate temporary buffers
  {
    m_invICT      = m_invICTMem + maxAbsIctMode;
    m_invICT[ 0]  = invTransformCbCr< 0>;
    m_invICT[ 1]  = invTransformCbCr< 1>;
    m_invICT[-1]  = invTransformCbCr<-1>;
    m_invICT[ 2]  = invTransformCbCr< 2>;
    m_invICT[-2]  = invTransformCbCr<-2>;
    m_invICT[ 3]  = invTransformCbCr< 3>;
    m_invICT[-3]  = invTransformCbCr<-3>;
    m_fwdICT      = m_fwdICTMem + maxAbsIctMode;
    m_fwdICT[ 0]  = fwdTransformCbCr< 0>;
    m_fwdICT[ 1]  = fwdTransformCbCr< 1>;
    m_fwdICT[-1]  = fwdTransformCbCr<-1>;
    m_fwdICT[ 2]  = fwdTransformCbCr< 2>;
    m_fwdICT[-2]  = fwdTransformCbCr<-2>;
    m_fwdICT[ 3]  = fwdTransformCbCr< 3>;
    m_fwdICT[-3]  = fwdTransformCbCr<-3>;
  }
}

TrQuant::~TrQuant()
{
  if( m_quant )
  {
    delete m_quant;
    m_quant = nullptr;
  }
}

void TrQuant::xDeQuant(const TransformUnit &tu,
                             CoeffBuf      &dstCoeff,
                       const ComponentID   &compID,
                       const QpParam       &cQP)
{
  m_quant->dequant( tu, dstCoeff, compID, cQP );
}

void TrQuant::init( const Quant* otherQuant,
                    const uint32_t uiMaxTrSize,
                    const bool bUseRDOQ,
                    const bool bUseRDOQTS,
                    const bool useSelectiveRDOQ,
                    const bool bEnc
)
{
  delete m_quant;
  m_quant = nullptr;

  m_quant = new DepQuant(otherQuant, bEnc);

  if( m_quant )
  {
    m_quant->init( uiMaxTrSize, bUseRDOQ, bUseRDOQTS, useSelectiveRDOQ );
  }

  m_fwdTx[TransType::DCT2][0] = fastForwardDCT2_B2;
  m_fwdTx[TransType::DCT2][1] = fastForwardDCT2_B4;
  m_fwdTx[TransType::DCT2][2] = fastForwardDCT2_B8;
  m_fwdTx[TransType::DCT2][3] = fastForwardDCT2_B16;
  m_fwdTx[TransType::DCT2][4] = fastForwardDCT2_B32;
  m_fwdTx[TransType::DCT2][5] = fastForwardDCT2_B64;

  m_fwdTx[TransType::DCT8][0] = nullptr;
  m_fwdTx[TransType::DCT8][1] = fastForwardDCT8_B4;
  m_fwdTx[TransType::DCT8][2] = fastForwardDCT8_B8;
  m_fwdTx[TransType::DCT8][3] = fastForwardDCT8_B16;
  m_fwdTx[TransType::DCT8][4] = fastForwardDCT8_B32;
  m_fwdTx[TransType::DCT8][5] = nullptr;

  m_fwdTx[TransType::DST7][0] = nullptr;
  m_fwdTx[TransType::DST7][1] = fastForwardDST7_B4;
  m_fwdTx[TransType::DST7][2] = fastForwardDST7_B8;
  m_fwdTx[TransType::DST7][3] = fastForwardDST7_B16;
  m_fwdTx[TransType::DST7][4] = fastForwardDST7_B32;
  m_fwdTx[TransType::DST7][5] = nullptr;

  m_invTx[TransType::DCT2][0] = fastInverseDCT2_B2;
  m_invTx[TransType::DCT2][1] = fastInverseDCT2_B4;
  m_invTx[TransType::DCT2][2] = fastInverseDCT2_B8;
  m_invTx[TransType::DCT2][3] = fastInverseDCT2_B16;
  m_invTx[TransType::DCT2][4] = fastInverseDCT2_B32;
  m_invTx[TransType::DCT2][5] = fastInverseDCT2_B64;

  m_invTx[TransType::DCT8][0] = nullptr;
  m_invTx[TransType::DCT8][1] = fastInverseDCT8_B4;
  m_invTx[TransType::DCT8][2] = fastInverseDCT8_B8;
  m_invTx[TransType::DCT8][3] = fastInverseDCT8_B16;
  m_invTx[TransType::DCT8][4] = fastInverseDCT8_B32;
  m_invTx[TransType::DCT8][5] = nullptr;

  m_invTx[TransType::DST7][0] = nullptr;
  m_invTx[TransType::DST7][1] = fastInverseDST7_B4;
  m_invTx[TransType::DST7][2] = fastInverseDST7_B8;
  m_invTx[TransType::DST7][3] = fastInverseDST7_B16;
  m_invTx[TransType::DST7][4] = fastInverseDST7_B32;
  m_invTx[TransType::DST7][5] = nullptr;

#ifdef TARGET_SIMD_X86
  initX86();
#endif
}

void TrQuant::fwdLfnstNxN( TCoeff* src, TCoeff* dst, const uint32_t mode, const uint32_t index, const uint32_t size, int zeroOutSize )
{
  const int8_t* trMat  = ( size > 4 ) ? g_lfnst8x8[ mode ][ index ][ 0 ] : g_lfnst4x4[ mode ][ index ][ 0 ];
  const int     trSize = ( size > 4 ) ? 48 : 16;
  TCoeff           coef;
  TCoeff*          out    = dst;
  assert( index < 3 );

  for( int j = 0; j < zeroOutSize; j++ )
  {
    TCoeff*          srcPtr   = src;
    const int8_t* trMatTmp = trMat;
    coef = 0;
    for( int i = 0; i < trSize; i++ )
    {
      coef += *srcPtr++ * *trMatTmp++;
    }
    *out++ = ( coef + 64 ) >> 7;
    trMat += trSize;
  }

  std::fill_n( out, trSize - zeroOutSize, 0 );
}

void TrQuant::invLfnstNxN( TCoeff* src, TCoeff* dst, const uint32_t mode, const uint32_t index, const uint32_t size, int zeroOutSize, const int maxLog2TrDynamicRange )
{
  const TCoeff    outputMinimum         = -( 1 << maxLog2TrDynamicRange );
  const TCoeff    outputMaximum         =  ( 1 << maxLog2TrDynamicRange ) - 1;
  const int8_t*   trMat                 =  ( size > 4 ) ? g_lfnst8x8[ mode ][ index ][ 0 ] : g_lfnst4x4[ mode ][ index ][ 0 ];
  const int       trSize                =  ( size > 4 ) ? 48 : 16;
  TCoeff          resi;
  TCoeff*         out                   =  dst;
  assert( index < 3 );

  for( int j = 0; j < trSize; j++ )
  {
    resi = 0;
    const int8_t* trMatTmp = trMat;
    TCoeff*       srcPtr   = src;
    for( int i = 0; i < zeroOutSize; i++ )
    {
      resi += *srcPtr++ * *trMatTmp;
      trMatTmp += trSize;
    }
    *out++ = Clip3<TCoeff>( outputMinimum, outputMaximum, ( resi + 64 ) >> 7 );
    trMat++;
  }
}

uint32_t TrQuant::getLFNSTIntraMode( int wideAngPredMode )
{
  uint32_t intraMode;

  if( wideAngPredMode < 0 )
  {
    intraMode = ( uint32_t ) ( wideAngPredMode + ( NUM_EXT_LUMA_MODE >> 1 ) + NUM_LUMA_MODE );
  }
  else if( wideAngPredMode >= NUM_LUMA_MODE )
  {
    intraMode = ( uint32_t ) ( wideAngPredMode + ( NUM_EXT_LUMA_MODE >> 1 ) );
  }
  else
  {
    intraMode = ( uint32_t ) wideAngPredMode;
  }

  return intraMode;
}

bool TrQuant::getTransposeFlag( uint32_t intraMode )
{
  return ( ( intraMode >= NUM_LUMA_MODE ) && ( intraMode >= ( NUM_LUMA_MODE + ( NUM_EXT_LUMA_MODE >> 1 ) ) ) ) ||
         ( ( intraMode <  NUM_LUMA_MODE ) && ( intraMode >  DIA_IDX ) );
}

void TrQuant::xInvLfnst( const TransformUnit &tu, const ComponentID compID )
{
  const int maxLog2TrDynamicRange = tu.cs->sps->getMaxLog2TrDynamicRange(toChannelType(compID));
  const CompArea& area     = tu.blocks[ compID ];
  const uint32_t  width    = area.width;
  const uint32_t  height   = area.height;
  const uint32_t  lfnstIdx = tu.cu->lfnstIdx;
  if (lfnstIdx && tu.mtsIdx[compID] != MtsType::SKIP && (tu.cu->isSepTree() ? true : isLuma(compID)))
  {
    const bool whge3 = width >= 8 && height >= 8;
    const ScanElement * scan = whge3 ? g_coefTopLeftDiagScan8x8[ gp_sizeIdxInfo->idxFrom( width ) ] : g_scanOrder[ SCAN_GROUPED_4x4 ][ CoeffScanType::DIAG ][ gp_sizeIdxInfo->idxFrom( width ) ][ gp_sizeIdxInfo->idxFrom( height ) ];
    uint32_t intraMode = PU::getFinalIntraMode( *tu.cs->getPU( area.pos(), toChannelType( compID ) ), toChannelType( compID ) );

    if (PU::isLMCMode(tu.cs->getPU(area.pos(), toChannelType(compID))->intraDir[toChannelType(compID)]))
    {
      intraMode = PU::getCoLocatedIntraLumaMode( *tu.cs->getPU( area.pos(), toChannelType( compID ) ) );
    }
    if (PU::isMIP(*tu.cs->getPU(area.pos(), toChannelType(compID)), toChannelType(compID)))
    {
      intraMode = PLANAR_IDX;
    }
    CHECK( intraMode >= NUM_INTRA_MODE - 1, "Invalid intra mode" );

    if( lfnstIdx < 3 )
    {
      intraMode = getLFNSTIntraMode( PU::getWideAngle( tu, intraMode, compID ) );
#if RExt__DECODER_DEBUG_TOOL_STATISTICS
      CodingStatistics::IncrementStatisticTool( CodingStatisticsClassType { STATS__TOOL_LFNST, width, height, compID } );
#endif
      bool          transposeFlag   = getTransposeFlag( intraMode );
      const int     sbSize          = whge3 ? 8 : 4;
      bool          tu4x4Flag       = ( width == 4 && height == 4 );
      bool          tu8x8Flag       = ( width == 8 && height == 8 );
      TCoeff*       lfnstTemp;
      TCoeff*       coeffTemp;
      int           y;
      lfnstTemp   = m_tempInMatrix;   // inverse spectral rearrangement
      coeffTemp   = m_tempCoeff;
      TCoeff *dst = lfnstTemp;

      const ScanElement *scanPtr = scan;
      for (y = 0; y < 16; y++)
      {
        *dst++ = coeffTemp[scanPtr->idx];
        scanPtr++;
      }

      invLfnstNxN(m_tempInMatrix, m_tempOutMatrix, g_lfnstLut[intraMode], lfnstIdx - 1, sbSize,
                  (tu4x4Flag || tu8x8Flag) ? 8 : 16, maxLog2TrDynamicRange);
      lfnstTemp = m_tempOutMatrix;   // inverse spectral rearrangement

      if (transposeFlag)
      {
        if (sbSize == 4)
        {
          for (y = 0; y < 4; y++)
          {
            coeffTemp[0] = lfnstTemp[0];
            coeffTemp[1] = lfnstTemp[4];
            coeffTemp[2] = lfnstTemp[8];
            coeffTemp[3] = lfnstTemp[12];
            lfnstTemp++;
            coeffTemp += width;
          }
        }
        else   // ( sbSize == 8 )
        {
          for (y = 0; y < 8; y++)
          {
            coeffTemp[0] = lfnstTemp[0];
            coeffTemp[1] = lfnstTemp[8];
            coeffTemp[2] = lfnstTemp[16];
            coeffTemp[3] = lfnstTemp[24];
            if (y < 4)
            {
              coeffTemp[4] = lfnstTemp[32];
              coeffTemp[5] = lfnstTemp[36];
              coeffTemp[6] = lfnstTemp[40];
              coeffTemp[7] = lfnstTemp[44];
            }
            lfnstTemp++;
            coeffTemp += width;
          }
        }
      }
      else
      {
        for (y = 0; y < sbSize; y++)
        {
          uint32_t uiStride = (y < 4) ? sbSize : 4;
          ::memcpy(coeffTemp, lfnstTemp, uiStride * sizeof(TCoeff));
          lfnstTemp += uiStride;
          coeffTemp += width;
        }
      }
    }
  }
}

void TrQuant::xFwdLfnst( const TransformUnit &tu, const ComponentID compID, const bool loadTr )
{
  const CompArea& area     = tu.blocks[ compID ];
  const uint32_t  width    = area.width;
  const uint32_t  height   = area.height;
  const uint32_t  lfnstIdx = tu.cu->lfnstIdx;
  if (lfnstIdx && tu.mtsIdx[compID] != MtsType::SKIP && (tu.cu->isSepTree() ? true : isLuma(compID)))
  {
    const bool whge3 = width >= 8 && height >= 8;
    const ScanElement * scan = whge3 ? g_coefTopLeftDiagScan8x8[ gp_sizeIdxInfo->idxFrom( width ) ] : g_scanOrder[ SCAN_GROUPED_4x4 ][ CoeffScanType::DIAG ][ gp_sizeIdxInfo->idxFrom( width ) ][ gp_sizeIdxInfo->idxFrom( height ) ];
    uint32_t intraMode = PU::getFinalIntraMode( *tu.cs->getPU( area.pos(), toChannelType( compID ) ), toChannelType( compID ) );

    if (PU::isLMCMode(tu.cs->getPU(area.pos(), toChannelType(compID))->intraDir[toChannelType(compID)]))
    {
      intraMode = PU::getCoLocatedIntraLumaMode( *tu.cs->getPU( area.pos(), toChannelType( compID ) ) );
    }
    if (PU::isMIP(*tu.cs->getPU(area.pos(), toChannelType(compID)), toChannelType(compID)))
    {
      intraMode = PLANAR_IDX;
    }
    CHECK( intraMode >= NUM_INTRA_MODE - 1, "Invalid intra mode" );

    if( lfnstIdx < 3 )
    {
      intraMode = getLFNSTIntraMode( PU::getWideAngle( tu, intraMode, compID ) );

      bool            transposeFlag   = getTransposeFlag( intraMode );
      const int       sbSize          = whge3 ? 8 : 4;
      bool            tu4x4Flag       = ( width == 4 && height == 4 );
      bool            tu8x8Flag       = ( width == 8 && height == 8 );
      TCoeff*         lfnstTemp;
      TCoeff*         coeffTemp;
      TCoeff *        tempCoeff = loadTr ? m_mtsCoeffs[tu.mtsIdx[compID]] : m_tempCoeff;

      int y;
      lfnstTemp = m_tempInMatrix;   // forward low frequency non-separable transform
      coeffTemp = tempCoeff;

      if (transposeFlag)
      {
        if (sbSize == 4)
        {
          for (y = 0; y < 4; y++)
          {
            lfnstTemp[0]  = coeffTemp[0];
            lfnstTemp[4]  = coeffTemp[1];
            lfnstTemp[8]  = coeffTemp[2];
            lfnstTemp[12] = coeffTemp[3];
            lfnstTemp++;
            coeffTemp += width;
          }
        }
        else   // ( sbSize == 8 )
        {
          for (y = 0; y < 8; y++)
          {
            lfnstTemp[0]  = coeffTemp[0];
            lfnstTemp[8]  = coeffTemp[1];
            lfnstTemp[16] = coeffTemp[2];
            lfnstTemp[24] = coeffTemp[3];
            if (y < 4)
            {
              lfnstTemp[32] = coeffTemp[4];
              lfnstTemp[36] = coeffTemp[5];
              lfnstTemp[40] = coeffTemp[6];
              lfnstTemp[44] = coeffTemp[7];
            }
            lfnstTemp++;
            coeffTemp += width;
          }
        }
      }
      else
      {
        for (y = 0; y < sbSize; y++)
        {
          uint32_t uiStride = (y < 4) ? sbSize : 4;
          ::memcpy(lfnstTemp, coeffTemp, uiStride * sizeof(TCoeff));
          lfnstTemp += uiStride;
          coeffTemp += width;
        }
      }

      fwdLfnstNxN(m_tempInMatrix, m_tempOutMatrix, g_lfnstLut[intraMode], lfnstIdx - 1, sbSize,
                  (tu4x4Flag || tu8x8Flag) ? 8 : 16);

      lfnstTemp         = m_tempOutMatrix;   // forward spectral rearrangement
      coeffTemp         = tempCoeff;
      int lfnstCoeffNum = (sbSize == 4) ? sbSize * sbSize : 48;

      const ScanElement *scanPtr = scan;

      for (y = 0; y < lfnstCoeffNum; y++)
      {
        coeffTemp[scanPtr->idx] = *lfnstTemp++;
        scanPtr++;
      }
    }
  }
}


void TrQuant::invTransformNxN( TransformUnit &tu, const ComponentID &compID, PelBuf &pResi, const QpParam &cQP )
{
  const CompArea &area    = tu.blocks[compID];
  const uint32_t uiWidth      = area.width;
  const uint32_t uiHeight     = area.height;

  CHECK( uiWidth > tu.cs->sps->getMaxTbSize() || uiHeight > tu.cs->sps->getMaxTbSize(), "Maximal allowed transformation size exceeded!" );
  CoeffBuf tempCoeff = CoeffBuf(m_tempCoeff, area);
  xDeQuant(tu, tempCoeff, compID, cQP);

  DTRACE_COEFF_BUF(D_TCOEFF, tempCoeff, tu, tu.cu->predMode, compID);

  if (tu.cs->sps->getUseLFNST())
  {
    xInvLfnst(tu, compID);
  }

  if (tu.mtsIdx[compID] == MtsType::SKIP)
  {
    xITransformSkip(tempCoeff, pResi, tu, compID);
  }
  else
  {
    xIT(tu, compID, tempCoeff, pResi);
  }

  //DTRACE_BLOCK_COEFF(tu.getCoeffs(compID), tu, tu.cu->predMode, compID);
  DTRACE_PEL_BUF( D_RESIDUALS, pResi, tu, tu.cu->predMode, compID);
}

std::pair<int64_t,int64_t> TrQuant::fwdTransformICT( const TransformUnit &tu, const PelBuf &resCb, const PelBuf &resCr, PelBuf &resC1, PelBuf &resC2, int jointCbCr )
{
  CHECK( Size(resCb) != Size(resCr), "resCb and resCr have different sizes" );
  CHECK( Size(resCb) != Size(resC1), "resCb and resC1 have different sizes" );
  CHECK( Size(resCb) != Size(resC2), "resCb and resC2 have different sizes" );
  return (*m_fwdICT[ TU::getICTMode(tu, jointCbCr) ])( resCb, resCr, resC1, resC2 );
}

void TrQuant::invTransformICT( const TransformUnit &tu, PelBuf &resCb, PelBuf &resCr )
{
  CHECK( Size(resCb) != Size(resCr), "resCb and resCr have different sizes" );
  (*m_invICT[ TU::getICTMode(tu) ])( resCb, resCr );
}

void TrQuant::selectICTCandidates(const TransformUnit &tu, CompStorage *resCb, CompStorage *resCr,
                                  CbfMaskList &cbfMasksToTest)
{
  CHECK( !resCb[0].valid() || !resCr[0].valid(), "standard components are not valid" );

  cbfMasksToTest.clear();

  if( !CU::isIntra( *tu.cu ) )
  {
    int cbfMask = CBF_MASK_CBCR;
    resCb[cbfMask].create( tu.blocks[COMPONENT_Cb] );
    resCr[cbfMask].create( tu.blocks[COMPONENT_Cr] );
    fwdTransformICT(tu, resCb[0], resCr[0], resCb[cbfMask], resCr[cbfMask], cbfMask);
    cbfMasksToTest.push_back( cbfMask );
    return;
  }

  std::pair<int64_t,int64_t> pairDist[4];
  for( int cbfMask = 0; cbfMask < 4; cbfMask++ )
  {
    if( cbfMask )
    {
      CHECK( resCb[cbfMask].valid() || resCr[cbfMask].valid(), "target components for cbfMask=" << cbfMask << " are already present" );
      resCb[cbfMask].create( tu.blocks[COMPONENT_Cb] );
      resCr[cbfMask].create( tu.blocks[COMPONENT_Cr] );
    }
    pairDist[cbfMask] = fwdTransformICT( tu, resCb[0], resCr[0], resCb[cbfMask], resCr[cbfMask], cbfMask );
  }

  int64_t minDist1  = std::min<int64_t>( pairDist[0].first, pairDist[0].second );
  int64_t minDist2  = std::numeric_limits<int64_t>::max();
  int     cbfMask1  = 0;
  int     cbfMask2  = 0;
  for (int cbfMask: { CBF_MASK_CB, CBF_MASK_CR, CBF_MASK_CBCR })
  {
    if( pairDist[cbfMask].first < minDist1 )
    {
      cbfMask2  = cbfMask1; minDist2  = minDist1;
      cbfMask1  = cbfMask;  minDist1  = pairDist[cbfMask1].first;
    }
    else if( pairDist[cbfMask].first < minDist2 )
    {
      cbfMask2  = cbfMask;  minDist2  = pairDist[cbfMask2].first;
    }
  }
  if( cbfMask1 )
  {
    cbfMasksToTest.push_back( cbfMask1 );
  }
  if( cbfMask2 && ( ( minDist2 < (9*minDist1)/8 ) || ( !cbfMask1 && minDist2 < (3*minDist1)/2 ) ) )
  {
    cbfMasksToTest.push_back( cbfMask2 );
  }
}



// ------------------------------------------------------------------------------------------------
// Logical transform
// ------------------------------------------------------------------------------------------------

void TrQuant::getTrTypes(const TransformUnit tu, const ComponentID compID, TransType &trTypeHor, TransType &trTypeVer)
{
  const bool isExplicitMTS = (CU::isIntra(*tu.cu) ? tu.cs->sps->getExplicitMtsIntraEnabled()
                                                  : tu.cs->sps->getExplicitMtsInterEnabled() && CU::isInter(*tu.cu))
                             && isLuma(compID);
  const bool isImplicitMTS = CU::isIntra(*tu.cu) && tu.cs->sps->getImplicitMTSIntraEnabled() && isLuma(compID)
                             && tu.cu->lfnstIdx == 0 && tu.cu->mipFlag == 0;
  const bool isISP = CU::isIntra(*tu.cu) && tu.cu->ispMode != ISPType::NONE && isLuma(compID);
  const bool isSBT = CU::isInter(*tu.cu) && tu.cu->sbtInfo && isLuma(compID);

  trTypeHor = TransType::DCT2;
  trTypeVer = TransType::DCT2;

  if (isISP && tu.cu->lfnstIdx)
  {
    return;
  }

  if (!tu.cs->sps->getMtsEnabled())
  {
    return;
  }

  if (isImplicitMTS || isISP)
  {
    int  width = tu.blocks[compID].width;
    int  height = tu.blocks[compID].height;
    bool widthDstOk = width >= 4 && width <= 16;
    bool heightDstOk = height >= 4 && height <= 16;

    if (widthDstOk)
    {
      trTypeHor = TransType::DST7;
    }
    if (heightDstOk)
    {
      trTypeVer = TransType::DST7;
    }
    return;
  }


  if (isSBT)
  {
    uint8_t sbtIdx = tu.cu->getSbtIdx();
    uint8_t sbtPos = tu.cu->getSbtPos();

    if( sbtIdx == SBT_VER_HALF || sbtIdx == SBT_VER_QUAD )
    {
      assert( tu.lwidth() <= MTS_INTER_MAX_CU_SIZE );
      if( tu.lheight() > MTS_INTER_MAX_CU_SIZE )
      {
        trTypeHor = trTypeVer = TransType::DCT2;
      }
      else
      {
        if (sbtPos == SBT_POS0)
        {
          trTypeHor = TransType::DCT8;
          trTypeVer = TransType::DST7;
        }
        else
        {
          trTypeHor = TransType::DST7;
          trTypeVer = TransType::DST7;
        }
      }
    }
    else
    {
      assert( tu.lheight() <= MTS_INTER_MAX_CU_SIZE );
      if( tu.lwidth() > MTS_INTER_MAX_CU_SIZE )
      {
        trTypeHor = trTypeVer = TransType::DCT2;
      }
      else
      {
        if (sbtPos == SBT_POS0)
        {
          trTypeHor = TransType::DST7;
          trTypeVer = TransType::DCT8;
        }
        else
        {
          trTypeHor = TransType::DST7;
          trTypeVer = TransType::DST7;
        }
      }
    }
    return;
  }

  if (isExplicitMTS)
  {
    if (tu.mtsIdx[compID] > MtsType::SKIP)
    {
      int indHor = (tu.mtsIdx[compID] - MtsType::DST7_DST7) & 1;
      int indVer = (tu.mtsIdx[compID] - MtsType::DST7_DST7) >> 1;
      trTypeHor  = indHor ? TransType::DCT8 : TransType::DST7;
      trTypeVer  = indVer ? TransType::DCT8 : TransType::DST7;
    }
  }
}

void TrQuant::xT( const TransformUnit &tu, const ComponentID &compID, const CPelBuf &resi, CoeffBuf &dstCoeff, const int width, const int height )
{
  const unsigned maxLog2TrDynamicRange  = tu.cs->sps->getMaxLog2TrDynamicRange( toChannelType( compID ) );
  const unsigned bitDepth               = tu.cs->sps->getBitDepth(              toChannelType( compID ) );
  const int      TRANSFORM_MATRIX_SHIFT = g_transformMatrixShift[TRANSFORM_FORWARD];
  const uint32_t transformWidthIndex    = floorLog2(width ) - 1;  // nLog2WidthMinus1, since transform start from 2-point
  const uint32_t transformHeightIndex   = floorLog2(height) - 1;  // nLog2HeightMinus1, since transform start from 2-point

  auto trTypeHor = TransType::DCT2;
  auto trTypeVer = TransType::DCT2;

  getTrTypes ( tu, compID, trTypeHor, trTypeVer );

  int skipWidth  = (trTypeHor != TransType::DCT2 && width == 32) ? 16 : std::max(width - MAX_NONZERO_TU_SIZE, 0);
  int skipHeight = (trTypeVer != TransType::DCT2 && height == 32) ? 16 : std::max(height - MAX_NONZERO_TU_SIZE, 0);

  if( tu.cs->sps->getUseLFNST() && tu.cu->lfnstIdx )
  {
    if( (width == 4 && height > 4) || (width > 4 && height == 4) )
    {
      skipWidth  = width  - 4;
      skipHeight = height - 4;
    }
    else if( (width >= 8 && height >= 8) )
    {
      skipWidth  = width  - 8;
      skipHeight = height - 8;
    }
  }

#if RExt__DECODER_DEBUG_TOOL_STATISTICS
  if (trTypeHor != TransType::DCT2)
  {
    CodingStatistics::IncrementStatisticTool( CodingStatisticsClassType{ STATS__TOOL_EMT, uint32_t( width ), uint32_t( height ), compID } );
  }
#endif

  alignas(MEMORY_ALIGN_DEF_SIZE) TCoeff block[MAX_TB_SIZEY * MAX_TB_SIZEY];

  const Pel *resiBuf    = resi.buf;
  const ptrdiff_t resiStride = resi.stride;

  for( int y = 0; y < height; y++ )
  {
    for( int x = 0; x < width; x++ )
    {
      block[( y * width ) + x] = resiBuf[( y * resiStride ) + x];
    }
  }

  if( width > 1 && height > 1 ) // 2-D transform
  {
    const int      shift_1st              = ((floorLog2(width )) + bitDepth + TRANSFORM_MATRIX_SHIFT) - maxLog2TrDynamicRange + COM16_C806_TRANS_PREC;
    const int      shift_2nd              =  (floorLog2(height))            + TRANSFORM_MATRIX_SHIFT                          + COM16_C806_TRANS_PREC;
    CHECK( shift_1st < 0, "Negative shift" );
    CHECK( shift_2nd < 0, "Negative shift" );
    TCoeff *tmp = (TCoeff *) alloca(width * height * sizeof(TCoeff));

    m_fwdTx[trTypeHor][transformWidthIndex](block, tmp, shift_1st, height, 0, skipWidth);
    m_fwdTx[trTypeVer][transformHeightIndex](tmp, dstCoeff.buf, shift_2nd, width, skipWidth, skipHeight);
  }
  else if( height == 1 ) //1-D horizontal transform
  {
    const int      shift              = ((floorLog2(width )) + bitDepth + TRANSFORM_MATRIX_SHIFT) - maxLog2TrDynamicRange + COM16_C806_TRANS_PREC;
    CHECK( shift < 0, "Negative shift" );
    CHECKD( ( transformWidthIndex < 0 ), "There is a problem with the width." );
    m_fwdTx[trTypeHor][transformWidthIndex](block, dstCoeff.buf, shift, 1, 0, skipWidth);
  }
  else   // if (width == 1) //1-D vertical transform
  {
    int shift = ( ( floorLog2(height) ) + bitDepth + TRANSFORM_MATRIX_SHIFT ) - maxLog2TrDynamicRange + COM16_C806_TRANS_PREC;
    CHECK( shift < 0, "Negative shift" );
    CHECKD( ( transformHeightIndex < 0 ), "There is a problem with the height." );
    m_fwdTx[trTypeVer][transformHeightIndex](block, dstCoeff.buf, shift, 1, 0, skipHeight);
  }
}

void TrQuant::xIT( const TransformUnit &tu, const ComponentID &compID, const CCoeffBuf &pCoeff, PelBuf &pResidual )
{
  const int      width                  = pCoeff.width;
  const int      height                 = pCoeff.height;
  const unsigned maxLog2TrDynamicRange  = tu.cs->sps->getMaxLog2TrDynamicRange( toChannelType( compID ) );
  const unsigned bitDepth               = tu.cs->sps->getBitDepth(              toChannelType( compID ) );
  const int      TRANSFORM_MATRIX_SHIFT = g_transformMatrixShift[TRANSFORM_INVERSE];
  const TCoeff   clipMinimum            = -( 1 << maxLog2TrDynamicRange );
  const TCoeff   clipMaximum            =  ( 1 << maxLog2TrDynamicRange ) - 1;
  const TCoeff   pelMinimum             = std::numeric_limits<Pel>::min();
  const TCoeff   pelMaximum             = std::numeric_limits<Pel>::max();
  const uint32_t transformWidthIndex    = floorLog2(width ) - 1;                                // nLog2WidthMinus1, since transform start from 2-point
  const uint32_t transformHeightIndex   = floorLog2(height) - 1;                                // nLog2HeightMinus1, since transform start from 2-point

  auto trTypeHor = TransType::DCT2;
  auto trTypeVer = TransType::DCT2;

  getTrTypes ( tu, compID, trTypeHor, trTypeVer );
  int skipWidth  = (trTypeHor != TransType::DCT2 && width == 32) ? 16 : std::max(width - MAX_NONZERO_TU_SIZE, 0);
  int skipHeight = (trTypeVer != TransType::DCT2 && height == 32) ? 16 : std::max(height - MAX_NONZERO_TU_SIZE, 0);

  if( tu.cs->sps->getUseLFNST() && tu.cu->lfnstIdx )
  {
    if( (width == 4 && height > 4) || (width > 4 && height == 4) )
    {
      skipWidth  = width  - 4;
      skipHeight = height - 4;
    }
    else if( (width >= 8 && height >= 8) )
    {
      skipWidth  = width  - 8;
      skipHeight = height - 8;
    }
  }

  TCoeff *block = ( TCoeff * ) alloca( width * height * sizeof( TCoeff ) );

  if( width > 1 && height > 1 ) //2-D transform
  {
    const int      shift_1st              =   TRANSFORM_MATRIX_SHIFT + 1 + COM16_C806_TRANS_PREC; // 1 has been added to shift_1st at the expense of shift_2nd
    const int      shift_2nd              = ( TRANSFORM_MATRIX_SHIFT + maxLog2TrDynamicRange - 1 ) - bitDepth + COM16_C806_TRANS_PREC;
    CHECK( shift_1st < 0, "Negative shift" );
    CHECK( shift_2nd < 0, "Negative shift" );
    TCoeff *tmp = ( TCoeff * ) alloca( width * height * sizeof( TCoeff ) );
    m_invTx[trTypeVer][transformHeightIndex](pCoeff.buf, tmp, shift_1st, width, skipWidth, skipHeight, clipMinimum,
                                             clipMaximum);
    m_invTx[trTypeHor][transformWidthIndex](tmp, block, shift_2nd, height, 0, skipWidth, pelMinimum, pelMaximum);
  }
  else if( width == 1 ) //1-D vertical transform
  {
    int shift = ( TRANSFORM_MATRIX_SHIFT + maxLog2TrDynamicRange - 1 ) - bitDepth + COM16_C806_TRANS_PREC;
    CHECK( shift < 0, "Negative shift" );
    CHECK( ( transformHeightIndex < 0 ), "There is a problem with the height." );
    m_invTx[trTypeVer][transformHeightIndex](pCoeff.buf, block, shift + 1, 1, 0, skipHeight, pelMinimum, pelMaximum);
  }
  else   // if(height == 1) //1-D horizontal transform
  {
    const int      shift              = ( TRANSFORM_MATRIX_SHIFT + maxLog2TrDynamicRange - 1 ) - bitDepth + COM16_C806_TRANS_PREC;
    CHECK( shift < 0, "Negative shift" );
    CHECK( ( transformWidthIndex < 0 ), "There is a problem with the width." );
    m_invTx[trTypeHor][transformWidthIndex](pCoeff.buf, block, shift + 1, 1, 0, skipWidth, pelMinimum, pelMaximum);
  }

  Pel *resiBuf    = pResidual.buf;
  ptrdiff_t resiStride = pResidual.stride;

  for( int y = 0; y < height; y++ )
  {
    for( int x = 0; x < width; x++ )
    {
      resiBuf[( y * resiStride ) + x] = Pel( block[( y * width ) + x] );
    }
  }
}

/** Wrapper function between HM interface and core NxN transform skipping
 */
void TrQuant::xITransformSkip(const CCoeffBuf     &pCoeff,
                                    PelBuf        &pResidual,
                              const TransformUnit &tu,
                              const ComponentID   &compID)
{
  const CompArea &area      = tu.blocks[compID];
  const int width           = area.width;
  const int height          = area.height;

  for (uint32_t y = 0; y < height; y++)
  {
    for (uint32_t x = 0; x < width; x++)
    {
      pResidual.at(x, y) = Pel(pCoeff.at(x, y));
    }
  }
}

void TrQuant::xQuant(TransformUnit &tu, const ComponentID &compID, const CCoeffBuf &pSrc, TCoeff &absSum,
                     const QpParam &cQP, const Ctx &ctx)
{
  m_quant->quant(tu, compID, pSrc, absSum, cQP, ctx);
}

void TrQuant::transformNxN(TransformUnit &tu, const ComponentID &compID, const QpParam &cQP, TrModeList &trModes,
                           const int maxCand)
{
        CodingStructure &cs = *tu.cs;
  const CompArea &rect      = tu.blocks[compID];
  const uint32_t width      = rect.width;
  const uint32_t height     = rect.height;

  const CPelBuf  resiBuf    = cs.getResiBuf(rect);

  CHECK( cs.sps->getMaxTbSize() < width, "Unsupported transformation size" );

  int pos = 0;
  static_vector<TrCost, TrModeList::max_num_elements> trCosts;
  const double facBB[] = { 1.2, 1.3, 1.3, 1.4, 1.5 };
  for (auto &it: trModes)
  {
    tu.mtsIdx[compID] = it.first;
    CoeffBuf tempCoeff( m_mtsCoeffs[tu.mtsIdx[compID]], rect);
    if( tu.noResidual )
    {
      int sumAbs = 0;
      trCosts.push_back(TrCost(sumAbs, pos++));
      continue;
    }

    if (tu.mtsIdx[compID] == MtsType::SKIP)
    {
      xTransformSkip( tu, compID, resiBuf, tempCoeff.buf );
    }
    else
    {
      xT( tu, compID, resiBuf, tempCoeff, width, height );
    }

    TCoeff sumAbs = 0;
    for (int k = 0; k < width * height; k++)
    {
      sumAbs += abs(tempCoeff.buf[k]);
    }

    double scaleSAD=1.0;
    if (tu.mtsIdx[compID] == MtsType::SKIP && ((floorLog2(width) + floorLog2(height)) & 1) == 1)
    {
      scaleSAD=1.0/1.414213562; // compensate for not scaling transform skip coefficients by 1/sqrt(2)
    }
    if (tu.mtsIdx[compID] == MtsType::SKIP)
    {
      int trShift = getTransformShift(tu.cu->slice->getSPS()->getBitDepth(toChannelType(compID)), rect.size(),
                                      tu.cu->slice->getSPS()->getMaxLog2TrDynamicRange(toChannelType(compID)));
      scaleSAD *= pow(2, trShift);
    }

    trCosts.push_back(TrCost(int(std::min<double>(sumAbs * scaleSAD, std::numeric_limits<int>::max())), pos++));
  }

  const double fac   = facBB[std::max(0, floorLog2(std::max(width, height)) - 2)];
  const double thr   = fac * trCosts.begin()->first;
  const double thrTS = trCosts.begin()->first;

  int numTests = 0;
  for (auto &itC: trCosts)
  {
    const bool isTS   = trModes.at(itC.second).first == MtsType::SKIP;
    const bool testTr = itC.first <= (isTS ? thrTS : thr) && numTests <= maxCand;

    trModes.at(itC.second).second = testTr;
    numTests += testTr;
  }
}

void TrQuant::transformNxN(TransformUnit &tu, const ComponentID &compID, const QpParam &cQP, TCoeff &absSum,
                           const Ctx &ctx, const bool loadTr)
{
        CodingStructure &cs = *tu.cs;
  const SPS &sps            = *cs.sps;
  const CompArea &rect      = tu.blocks[compID];
  const uint32_t uiWidth        = rect.width;
  const uint32_t uiHeight       = rect.height;

  const CPelBuf resiBuf     = cs.getResiBuf(rect);

  if( tu.noResidual )
  {
    absSum = 0;
    TU::setCbfAtDepth(tu, compID, tu.depth, absSum > 0);
    return;
  }

  if (tu.cu->getBdpcmMode(compID) != BdpcmMode::NONE)
  {
    tu.mtsIdx[compID] = MtsType::SKIP;
  }

  absSum = 0;

  // transform and quantize
  CHECK(cs.sps->getMaxTbSize() < uiWidth, "Unsupported transformation size");

  CoeffBuf tempCoeff(loadTr ? m_mtsCoeffs[tu.mtsIdx[compID]] : m_tempCoeff, rect);

  DTRACE_PEL_BUF(D_RESIDUALS, resiBuf, tu, tu.cu->predMode, compID);

  if (!loadTr)
  {
    if (tu.mtsIdx[compID] == MtsType::SKIP)
    {
      xTransformSkip(tu, compID, resiBuf, tempCoeff.buf);
    }
    else
    {
      xT(tu, compID, resiBuf, tempCoeff, uiWidth, uiHeight);
    }
  }

  if (sps.getUseLFNST())
  {
    xFwdLfnst(tu, compID, loadTr);
  }

  DTRACE_COEFF_BUF(D_TCOEFF, tempCoeff, tu, tu.cu->predMode, compID);

  xQuant(tu, compID, tempCoeff, absSum, cQP, ctx);

  DTRACE_COEFF_BUF(D_TCOEFF, tu.getCoeffs(compID), tu, tu.cu->predMode, compID);

  // set coded block flag (CBF)
  TU::setCbfAtDepth(tu, compID, tu.depth, absSum > 0);
}


void TrQuant::xTransformSkip(const TransformUnit &tu, const ComponentID &compID, const CPelBuf &resi, TCoeff* psCoeff)
{
  const CompArea &rect = tu.blocks[compID];
  const uint32_t width = rect.width;
  const uint32_t height = rect.height;

  for (uint32_t y = 0, coefficientIndex = 0; y < height; y++)
  {
    for (uint32_t x = 0; x < width; x++, coefficientIndex++)
    {
      psCoeff[coefficientIndex] = TCoeff(resi.at(x, y));
    }
  }
}

//! \}
