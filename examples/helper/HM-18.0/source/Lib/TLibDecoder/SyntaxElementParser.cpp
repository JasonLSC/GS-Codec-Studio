/* The copyright in this software is being made available under the BSD
* License, included below. This software may be subject to other third party
* and contributor rights, including patent rights, and no such rights are
* granted under this license.
*
* Copyright (c) 2010-2022, ITU/ISO/IEC
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

/** \file     SyntaxElementParser.cpp
    \brief    Parsing functionality high level syntax
*/

//! \ingroup TLibDecoder
//! \{

#include "TLibCommon/CommonDef.h"
#include "TLibCommon/TComRom.h"
#include "TLibCommon/TComBitStream.h"
#include "SyntaxElementParser.h"
#if RExt__DECODER_DEBUG_BIT_STATISTICS
#include "TLibCommon/TComCodingStatistics.h"
#endif

#if ENC_DEC_TRACE

Void  xTraceAccessUnitDelimiter ()
{
  fprintf( g_hTrace, "=========== Access Unit Delimiter ===========\n");
}

Void xTraceFillerData ()
{
  fprintf( g_hTrace, "=========== Filler Data ===========\n");
}

#endif



#if DECODER_PARTIAL_CONFORMANCE_CHECK!=0

Void SyntaxElementParser::xReadSCodeChk ( UInt   length, Int& val, const TChar *pSymbolName, const Int minValIncl, const Int maxValIncl )
{
  READ_SCODE(length, val, pSymbolName);
  TDecConformanceCheck::checkRange(val, pSymbolName, minValIncl, maxValIncl);
}

Void SyntaxElementParser::xReadCodeChk ( UInt   length, UInt& val, const TChar *pSymbolName, const UInt minValIncl, const UInt maxValIncl )
{
  READ_CODE(length, val, pSymbolName);
  TDecConformanceCheck::checkRange(val, pSymbolName, minValIncl, maxValIncl);
}

Void SyntaxElementParser::xReadUvlcChk ( UInt&  val, const TChar *pSymbolName, const UInt minValIncl, const UInt maxValIncl )
{
  READ_UVLC(val, pSymbolName);
  TDecConformanceCheck::checkRange(val, pSymbolName, minValIncl, maxValIncl);
}

Void SyntaxElementParser::xReadSvlcChk ( Int&   val, const TChar *pSymbolName, const Int  minValIncl, const Int  maxValIncl )
{
  READ_SVLC(val, pSymbolName);
  TDecConformanceCheck::checkRange(val, pSymbolName, minValIncl, maxValIncl);
}

Void SyntaxElementParser::xReadFlagChk ( UInt&  val, const TChar *pSymbolName, const UInt minValIncl, const UInt maxValIncl )
{
  READ_FLAG(val, pSymbolName);
  TDecConformanceCheck::checkRange(val, pSymbolName, minValIncl, maxValIncl);
}
#endif

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================
#if RExt__DECODER_DEBUG_BIT_STATISTICS || ENC_DEC_TRACE
Void SyntaxElementParser::xReadSCode (UInt uiLength, Int& rValue, const TChar *pSymbolName)
#else
Void SyntaxElementParser::xReadSCode (UInt uiLength, Int& rValue)
#endif
{
  UInt val;
  assert ( uiLength > 0 && uiLength<=32);
  m_pcBitstream->read (uiLength, val);
  rValue= uiLength>=32 ? Int(val) : ( (-Int( val & (UInt(1)<<(uiLength-1)))) | Int(val) );

#if RExt__DECODER_DEBUG_BIT_STATISTICS
  TComCodingStatistics::IncrementStatisticEP(pSymbolName, uiLength, rValue);
#endif
#if ENC_DEC_TRACE
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  if (uiLength < 10)
  {
    fprintf( g_hTrace, "%-50s i(%d)  : %d\n", pSymbolName, uiLength, rValue );
  }
  else
  {
    fprintf( g_hTrace, "%-50s i(%d) : %d\n", pSymbolName, uiLength, rValue );
  }
  fflush ( g_hTrace );
#endif
}

#if RExt__DECODER_DEBUG_BIT_STATISTICS || ENC_DEC_TRACE
Void SyntaxElementParser::xReadCode (UInt uiLength, UInt& rValue, const TChar *pSymbolName)
#else
Void SyntaxElementParser::xReadCode (UInt uiLength, UInt& rValue)
#endif
{
  assert ( uiLength > 0 );
  m_pcBitstream->read (uiLength, rValue);
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  TComCodingStatistics::IncrementStatisticEP(pSymbolName, uiLength, rValue);
#endif
#if ENC_DEC_TRACE
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  if (uiLength < 10)
  {
    fprintf( g_hTrace, "%-50s u(%d)  : %u\n", pSymbolName, uiLength, rValue );
  }
  else
  {
    fprintf( g_hTrace, "%-50s u(%d) : %u\n", pSymbolName, uiLength, rValue );
  }
  fflush ( g_hTrace );
#endif
}


#if RExt__DECODER_DEBUG_BIT_STATISTICS || ENC_DEC_TRACE
Void SyntaxElementParser::xReadUvlc( UInt& rValue, const TChar *pSymbolName)
#else
Void SyntaxElementParser::xReadUvlc( UInt& rValue)
#endif
{
  UInt uiVal = 0;
  UInt uiCode = 0;
  UInt uiLength;
  m_pcBitstream->read( 1, uiCode );
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  UInt totalLen=1;
#endif

  if( 0 == uiCode )
  {
    uiLength = 0;

    while( ! ( uiCode & 1 ))
    {
      m_pcBitstream->read( 1, uiCode );
      uiLength++;
    }

    m_pcBitstream->read( uiLength, uiVal );

    uiVal += (1 << uiLength)-1;
#if RExt__DECODER_DEBUG_BIT_STATISTICS
    totalLen+=uiLength+uiLength;
#endif
  }

  rValue = uiVal;
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  TComCodingStatistics::IncrementStatisticEP(pSymbolName, Int(totalLen), rValue);
#endif

#if ENC_DEC_TRACE
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-50s ue(v) : %u\n", pSymbolName, rValue );
  fflush ( g_hTrace );
#endif
}

#if RExt__DECODER_DEBUG_BIT_STATISTICS || ENC_DEC_TRACE
Void SyntaxElementParser::xReadSvlc( Int& rValue, const TChar *pSymbolName)
#else
Void SyntaxElementParser::xReadSvlc( Int& rValue)
#endif
{
  UInt uiBits = 0;
  m_pcBitstream->read( 1, uiBits );
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  UInt totalLen=1;
#endif
  if( 0 == uiBits )
  {
    UInt uiLength = 0;

    while( ! ( uiBits & 1 ))
    {
      m_pcBitstream->read( 1, uiBits );
      uiLength++;
    }

    m_pcBitstream->read( uiLength, uiBits );

    uiBits += (1 << uiLength);
    rValue = ( uiBits & 1) ? -(Int)(uiBits>>1) : (Int)(uiBits>>1);
#if RExt__DECODER_DEBUG_BIT_STATISTICS
    totalLen+=uiLength+uiLength;
#endif
  }
  else
  {
    rValue = 0;
  }
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  TComCodingStatistics::IncrementStatisticEP(pSymbolName, Int(totalLen), rValue);
#endif

#if ENC_DEC_TRACE
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-50s se(v) : %d\n", pSymbolName, rValue );
  fflush ( g_hTrace );
#endif

}

#if RExt__DECODER_DEBUG_BIT_STATISTICS || ENC_DEC_TRACE
Void SyntaxElementParser::xReadFlag (UInt& rValue, const TChar *pSymbolName)
#else
Void SyntaxElementParser::xReadFlag (UInt& rValue)
#endif
{
  m_pcBitstream->read( 1, rValue );
#if RExt__DECODER_DEBUG_BIT_STATISTICS
  TComCodingStatistics::IncrementStatisticEP(pSymbolName, 1, Int(rValue));
#endif

#if ENC_DEC_TRACE
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-50s u(1)  : %d\n", pSymbolName, rValue );
  fflush ( g_hTrace );
#endif
}

Void SyntaxElementParser::xReadRbspTrailingBits()
{
  UInt bit;
  READ_FLAG( bit, "rbsp_stop_one_bit");
  assert (bit==1);
  Int cnt = 0;
  while (m_pcBitstream->getNumBitsUntilByteAligned())
  {
    READ_FLAG( bit, "rbsp_alignment_zero_bit");
    assert (bit==0);
    cnt++;
  }
  assert(cnt<8);
}

Void AUDReader::parseAccessUnitDelimiter(TComInputBitstream* bs, UInt &picType)
{
  setBitstream(bs);

#if ENC_DEC_TRACE
  xTraceAccessUnitDelimiter();
#endif

  READ_CODE (3, picType, "pic_type");
  xReadRbspTrailingBits();
}

Void FDReader::parseFillerData(TComInputBitstream* bs, UInt &fdSize)
{
  setBitstream(bs);
#if ENC_DEC_TRACE
  xTraceFillerData();
#endif
  UInt ffByte;
  fdSize = 0;
  while( m_pcBitstream->getNumBitsLeft() >8 )
  {
    READ_CODE (8, ffByte, "ff_byte");
    assert (ffByte==0xff);
    fdSize++;
  }
  xReadRbspTrailingBits();
}


//! \}

