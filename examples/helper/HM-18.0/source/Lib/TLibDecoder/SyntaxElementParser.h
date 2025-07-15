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

/** \file     SyntaxElementParser.h
    \brief    Parsing functionality high level syntax
*/

#ifndef __SYNTAXELEMENTPARSER__
#define __SYNTAXELEMENTPARSER__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TLibCommon/TComRom.h"
#include "TDecConformance.h"

#if DECODER_PARTIAL_CONFORMANCE_CHECK!=0
#define READ_SCODE_CHK(length, code, name, minValIncl, maxValIncl) xReadSCodeChk ( length, code, name, minValIncl, maxValIncl )
#define READ_CODE_CHK(length, code, name, minValIncl, maxValIncl)  xReadCodeChk  ( length, code, name, minValIncl, maxValIncl )
#define READ_UVLC_CHK(        code, name, minValIncl, maxValIncl)  xReadUvlcChk  (         code, name, minValIncl, maxValIncl )
#define READ_SVLC_CHK(        code, name, minValIncl, maxValIncl)  xReadSvlcChk  (         code, name, minValIncl, maxValIncl )
#define READ_FLAG_CHK(        code, name, minValIncl, maxValIncl)  xReadFlagChk  (         code, name, minValIncl, maxValIncl )
#elif RExt__DECODER_DEBUG_BIT_STATISTICS || ENC_DEC_TRACE
#define READ_SCODE_CHK(length, code, name, minValIncl, maxValIncl) xReadSCode ( length, code, name )
#define READ_CODE_CHK(length, code, name, minValIncl, maxValIncl)  xReadCode  ( length, code, name )
#define READ_UVLC_CHK(        code, name, minValIncl, maxValIncl)  xReadUvlc  (         code, name )
#define READ_SVLC_CHK(        code, name, minValIncl, maxValIncl)  xReadSvlc  (         code, name )
#define READ_FLAG_CHK(        code, name, minValIncl, maxValIncl)  xReadFlag  (         code, name )
#else
#define READ_SCODE_CHK(length, code, name, minValIncl, maxValIncl) xReadSCode ( length, code )
#define READ_CODE_CHK(length, code, name, minValIncl, maxValIncl)  xReadCode  ( length, code )
#define READ_UVLC_CHK(        code, name, minValIncl, maxValIncl)  xReadUvlc  (         code )
#define READ_SVLC_CHK(        code, name, minValIncl, maxValIncl)  xReadSvlc  (         code )
#define READ_FLAG_CHK(        code, name, minValIncl, maxValIncl)  xReadFlag  (         code )
#endif


#if RExt__DECODER_DEBUG_BIT_STATISTICS || ENC_DEC_TRACE
#define READ_SCODE(length, code, name)    xReadSCode ( length, code, name )
#define READ_CODE(length, code, name)     xReadCode  ( length, code, name )
#define READ_UVLC(        code, name)     xReadUvlc  (         code, name )
#define READ_SVLC(        code, name)     xReadSvlc  (         code, name )
#define READ_FLAG(        code, name)     xReadFlag  (         code, name )

#else
#define READ_SCODE(length, code, name)    xReadSCode ( length, code )
#define READ_CODE(length, code, name)     xReadCode  ( length, code )
#define READ_UVLC(        code, name)     xReadUvlc  (         code )
#define READ_SVLC(        code, name)     xReadSvlc  (         code )
#define READ_FLAG(        code, name)     xReadFlag  (         code )

#endif

//! \ingroup TLibDecoder
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

class SyntaxElementParser
{
protected:
  TComInputBitstream*   m_pcBitstream;

  SyntaxElementParser()
  : m_pcBitstream (NULL)
  {};
  virtual ~SyntaxElementParser() {};

#if DECODER_PARTIAL_CONFORMANCE_CHECK!=0
  Void  xReadSCodeChk ( UInt   length, Int& val, const TChar *pSymbolName, const Int minValIncl, const Int maxValIncl );
  Void  xReadCodeChk  ( UInt   length, UInt& val, const TChar *pSymbolName, const UInt minValIncl, const UInt maxValIncl );
  Void  xReadUvlcChk  ( UInt&  val, const TChar *pSymbolName, const UInt minValIncl, const UInt maxValIncl );
  Void  xReadSvlcChk  ( Int&   val, const TChar *pSymbolName, const Int  minValIncl, const Int  maxValIncl );
  Void  xReadFlagChk  ( UInt&  val, const TChar *pSymbolName, const UInt minValIncl, const UInt maxValIncl );
#endif

#if RExt__DECODER_DEBUG_BIT_STATISTICS || ENC_DEC_TRACE
  Void  xReadSCode   ( UInt   length, Int& val, const TChar *pSymbolName );
  Void  xReadCode    ( UInt   length, UInt& val, const TChar *pSymbolName );
  Void  xReadUvlc    ( UInt&  val, const TChar *pSymbolName );
  Void  xReadSvlc    ( Int&   val, const TChar *pSymbolName );
  Void  xReadFlag    ( UInt&  val, const TChar *pSymbolName );
#else
  Void  xReadSCode   ( UInt   length, Int& val );
  Void  xReadCode    ( UInt   length, UInt& val );
  Void  xReadUvlc    ( UInt&  val );
  Void  xReadSvlc    ( Int&   val );
  Void  xReadFlag    ( UInt&  val );
#endif
public:
  Void  setBitstream ( TComInputBitstream* p )   { m_pcBitstream = p; }
  TComInputBitstream* getBitstream() { return m_pcBitstream; }

protected:
  Void xReadRbspTrailingBits();
};

class AUDReader: public SyntaxElementParser
{
public:
  AUDReader() {};
  virtual ~AUDReader() {};
  Void parseAccessUnitDelimiter(TComInputBitstream* bs, UInt &picType);
};

class FDReader: public SyntaxElementParser
{
public:
  FDReader() {};
  virtual ~FDReader() {};
  Void parseFillerData(TComInputBitstream* bs, UInt &fdSize);
};


//! \}

#endif // !defined(__SYNTAXELEMENTPARSER__)

