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

/** \file     dtrace_buffer.h
 *  \brief    Easy to use dtrace calls concerning buffers
 */

#ifndef _DTRACE_BUFFER_H_
#define _DTRACE_BUFFER_H_

#include "dtrace.h"
#include "dtrace_next.h"

#include "CommonLib/CommonDef.h"
#include "CommonLib/Buffer.h"
#include "CommonLib/Unit.h"

inline unsigned calcCheckSum(const int width, const int height, const Pel *p, const ptrdiff_t stride,
                             const int bitdepth)
{
  unsigned checksum = 0;
  for (unsigned y = 0; y < height; y++)
  {
    for (unsigned x = 0; x < width; x++)
    {
      uint8_t xor_mask = (x & 0xff) ^ (y & 0xff) ^ (x >> 8) ^ (y >> 8);
      checksum = (checksum + ((p[y*stride+x] & 0xff) ^ xor_mask)) & 0xffffffff;

      if(bitdepth > 8)
      {
        checksum = (checksum + ((p[y*stride+x]>>8) ^ xor_mask)) & 0xffffffff;
      }
    }
  }
  return checksum;
}

inline unsigned calcCheckSum( const CPelBuf& buf, int bitdepth )
{
  return calcCheckSum( buf.width, buf.height, buf.buf, buf.stride, bitdepth );
}

#if ENABLE_TRACING

//////////////////////////////////////////////////////////////////////////
//
// Specialized helper functions
//
//////////////////////////////////////////////////////////////////////////
inline void dtraceCoeffBuf( DTRACE_CHANNEL channnel, const CCoeffBuf& coefBuf, const UnitArea& ua, PredMode predMode, const ComponentID compId, uint32_t zIdx = 0 )
{
  int x0 = ua.blocks[compId].x;
  int y0 = ua.blocks[compId].y;
  const ptrdiff_t   uiStride = coefBuf.stride;
  const TCoeff*     piReco   = coefBuf.buf;
  const uint32_t    uiWidth  = ua.blocks[compId].width;
  const uint32_t    uiHeight = ua.blocks[compId].height;
  DTRACE(g_trace_ctx, channnel, "@(%4d,%4d) [%2dx%2d] comp=%d predmode=%d \n", x0, y0, uiWidth, uiHeight, compId, predMode);
  DTRACE_BLOCK(g_trace_ctx, channnel, piReco, uiStride, uiWidth, uiHeight);
}

inline void dtracePelBuf( DTRACE_CHANNEL channnel, const CPelBuf& pelBuf, const UnitArea& ua, PredMode predMode, const ComponentID compId )
{
  int x0 = ua.block(compId).x;
  int y0 = ua.block(compId).y;
  const ptrdiff_t   uiStride     = pelBuf.stride;
  const Pel*        piReco       = pelBuf.buf;
  const uint32_t    uiWidth      = ua.block(compId).width;
  const uint32_t    uiHeight     = ua.block(compId).height;
  DTRACE      ( g_trace_ctx, channnel,   "@(%4d,%4d) [%2dx%2d] comp=%d predmode=%d \n", x0, y0, uiWidth, uiHeight, compId, predMode );
  DTRACE_BLOCK( g_trace_ctx, channnel,   piReco, uiStride, uiWidth, uiHeight );
}

inline void dtraceBlockRec( const CPelUnitBuf& pelUnitBuf, const UnitArea& ua, PredMode predMode, uint32_t zIdx = 0 )
{
  if( ua.blocks[COMPONENT_Y].valid() )
  {
    const int     x0           = ua.lumaPos().x;
    const int     y0           = ua.lumaPos().y;
    const ptrdiff_t   uiStride     = pelUnitBuf.Y().stride;
    const Pel*        piReco       = pelUnitBuf.Y().buf;
    const uint32_t    uiWidth      = ua.lumaSize().width;
    const uint32_t    uiHeight     = ua.lumaSize().height;
    DTRACE      ( g_trace_ctx, D_REC_CB_LUMA,   "%d, x=%d, y=%d, size=%dx%d, predmode=%d \n", zIdx, x0, y0, uiWidth, uiHeight, predMode );
    DTRACE_BLOCK( g_trace_ctx, D_REC_CB_LUMA,   piReco, uiStride, uiWidth, uiHeight );
  }
  if( ua.blocks[COMPONENT_Cb].valid() )
  {
    const int     x0           = ua.blocks[1].x;
    const int     y0           = ua.blocks[1].y;
    const uint32_t    uiWidth      = ua.blocks[1].width;
    const uint32_t    uiHeight     = ua.blocks[1].height;
    const ptrdiff_t   uiCStride    = pelUnitBuf.Cb().stride;
    const Pel*    piRecoU      = pelUnitBuf.Cb().buf;
    const Pel*    piRecoV      = pelUnitBuf.Cr().buf;
    DTRACE      ( g_trace_ctx, D_REC_CB_CHROMA, "%d, x=%d, y=%d, size=%dx%d, predmode=%d \n", zIdx, x0, y0, uiWidth, uiHeight, predMode );
    DTRACE_BLOCK( g_trace_ctx, D_REC_CB_CHROMA, piRecoU, uiCStride, uiWidth, uiHeight );
    DTRACE_BLOCK( g_trace_ctx, D_REC_CB_CHROMA, piRecoV, uiCStride, uiWidth, uiHeight );
  }
}

inline void dtraceUnitComp( DTRACE_CHANNEL channel, CPelUnitBuf& pelUnitBuf, const UnitArea& ua, ComponentID compId, PredMode predMode, uint32_t zIdx = 0 )
{
  if (!g_trace_ctx)
  {
    return;
  }
  if (!isChromaEnabled(pelUnitBuf.chromaFormat) && compId != COMPONENT_Y)
  {
    return;
  }
  const Pel* piReco   = pelUnitBuf.bufs[compId].buf;
  ptrdiff_t      uiStride = pelUnitBuf.bufs[compId].stride;
  uint32_t       uiWidth  = ua.blocks[compId].width;
  uint32_t       uiHeight = ua.blocks[compId].height;
  int x0              = ua.lumaPos().x;
  int y0              = ua.lumaPos().y;

  DTRACE      ( g_trace_ctx, channel, "%s: %d, x=%d, y=%d, size=%dx%d, predmode=%d \n", g_trace_ctx->getChannelName(channel), zIdx, x0, y0, uiWidth, uiHeight, predMode );
  DTRACE_BLOCK( g_trace_ctx, channel, piReco, uiStride, uiWidth, uiHeight );
}

inline void dtraceCRC(CDTrace *trace_ctx, DTRACE_CHANNEL channel, const CodingStructure &cs,
                      const CPelUnitBuf &pelUnitBuf, const Area *parea = nullptr)
{
  const Area& area = parea ? *parea : cs.area.Y();
  DTRACE(trace_ctx, channel, " CRC: %6lld %3d @(%4d,%4d) [%2dx%2d] ,Checksum(%x %x %x)\n",
         DTRACE_GET_COUNTER(g_trace_ctx, channel), cs.slice->getPOC(), area.x, area.y, area.width, area.height,
         calcCheckSum(pelUnitBuf.bufs[COMPONENT_Y], cs.sps->getBitDepth(ChannelType::LUMA)),
         calcCheckSum(pelUnitBuf.bufs[COMPONENT_Cb], cs.sps->getBitDepth(ChannelType::CHROMA)),
         calcCheckSum(pelUnitBuf.bufs[COMPONENT_Cr], cs.sps->getBitDepth(ChannelType::CHROMA)));
}

inline void dtraceCCRC(CDTrace *trace_ctx, DTRACE_CHANNEL channel, const CodingStructure &cs, const CPelBuf &pelBuf,
                       ComponentID compId, const Area *parea = nullptr)
{
  const Area& area = parea ? *parea : cs.area.Y();
  DTRACE( trace_ctx, channel, "CRC: %6lld %3d @(%4d,%4d) [%2dx%2d] ,comp %d Checksum(%x)\n",
      DTRACE_GET_COUNTER( g_trace_ctx, channel ),
      cs.slice->getPOC(),
      area.x, area.y, area.width, area.height, compId,
      calcCheckSum( pelBuf, cs.sps->getBitDepth ( toChannelType(compId) )));
}

inline void dtraceMotField( CDTrace *trace_ctx, const PredictionUnit& pu )
{
  DTRACE( trace_ctx, D_MOT_FIELD, "PU %d,%d @ %d,%d\n", pu.lwidth(), pu.lheight(), pu.lx(), pu.ly() );
  const CMotionBuf mb = pu.getMotionBuf();
  for( uint32_t listIdx = 0; listIdx < 2; listIdx++ )
  {
    RefPicList eListIdx = RefPicList( listIdx );
    for( int y = 0, i = 0; y < pu.lheight(); y += 4 )
    {
      for( int x = 0; x < pu.lwidth(); x += 4, i++ )
      {
        const MotionInfo &mi = mb.at( x >> 2, y >> 2 );
        DTRACE( trace_ctx, D_MOT_FIELD, "%d,%d:%d  ", mi.mv[eListIdx].getHor(), mi.mv[eListIdx].getVer(), mi.refIdx[eListIdx] );
      }
      DTRACE( trace_ctx, D_MOT_FIELD, "\n" );
    }
    DTRACE( trace_ctx, D_MOT_FIELD, "\n" );
  }
}

#define DTRACE_PEL_BUF(...)              dtracePelBuf( __VA_ARGS__ )
#define DTRACE_COEFF_BUF(...)            dtraceCoeffBuf( __VA_ARGS__ )
#define DTRACE_BLOCK_REC(...)            dtraceBlockRec( __VA_ARGS__ )
#define DTRACE_PEL_BUF_COND(_cond,...)   { if((_cond)) dtracePelBuf( __VA_ARGS__ ); }
#define DTRACE_COEFF_BUF_COND(_cond,...) { if((_cond)) dtraceCoeffBuf( __VA_ARGS__ ); }
#define DTRACE_BLOCK_REC_COND(_cond,...) { if((_cond)) dtraceBlockRec( __VA_ARGS__ ); }
#define DTRACE_UNIT_COMP(...)            dtraceUnitComp( __VA_ARGS__ )
#define DTRACE_CRC(...)                  dtraceCRC( __VA_ARGS__ )
#define DTRACE_CCRC(...)                 dtraceCCRC( __VA_ARGS__ )
#define DTRACE_MOT_FIELD(...)            dtraceMotField( __VA_ARGS__ )

#else

#define DTRACE_PEL_BUF(...)
#define DTRACE_COEFF_BUF(...)
#define DTRACE_BLOCK_REC(...)
#define DTRACE_PEL_BUF_COND(...)
#define DTRACE_COEFF_BUF_COND(...)
#define DTRACE_BLOCK_REC_COND(...)
#define DTRACE_UNIT_COMP(...)
#define DTRACE_CRC(...)
#define DTRACE_CCRC(...)
#define DTRACE_MOT_FIELD(...)

#endif

#endif // _DTRACE_HEVC_H_
