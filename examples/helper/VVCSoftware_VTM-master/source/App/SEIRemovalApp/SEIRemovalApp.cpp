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

/** \file     SEIRemovalApp.cpp
    \brief    Decoder application class
*/

#include <list>
#include <vector>
#include <stdio.h>
#include <fcntl.h>

#include "SEIRemovalApp.h"
#include "DecoderLib/AnnexBread.h"
#include "DecoderLib/NALread.h"

//! \ingroup DecoderApp
//! \{

// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================

SEIRemovalApp::SEIRemovalApp()
{
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/**
 - create internal class
 - initialize internal class
 - until the end of the bitstream, call decoding function in SEIRemovalApp class
 - delete allocated buffers
 - destroy internal class
 - returns the number of mismatching pictures
 */

void read2(InputNALUnit& nalu)
{
  InputBitstream& bs = nalu.getBitstream();

  nalu.m_forbiddenZeroBit   = bs.read(1);                 // forbidden zero bit
  nalu.m_nuhReservedZeroBit = bs.read(1);                 // nuh_reserved_zero_bit
  nalu.m_nuhLayerId         = bs.read(6);                 // nuh_layer_id
  nalu.m_nalUnitType        = (NalUnitType) bs.read(5);   // nal_unit_type
  nalu.m_temporalId         = bs.read(3) - 1;             // nuh_temporal_id_plus1
}

uint32_t SEIRemovalApp::decode()
{
  std::ifstream bitstreamFileIn(m_bitstreamFileNameIn.c_str(), std::ifstream::in | std::ifstream::binary);
  if (!bitstreamFileIn)
  {
    EXIT( "failed to open bitstream file " << m_bitstreamFileNameIn.c_str() << " for reading" ) ;
  }

  std::ofstream bitstreamFileOut(m_bitstreamFileNameOut.c_str(), std::ifstream::out | std::ifstream::binary);

  InputByteStream bytestream(bitstreamFileIn);

  bitstreamFileIn.clear();
  bitstreamFileIn.seekg(0, std::ios::beg);

  int unitCnt = 0;

  while (!!bitstreamFileIn)
  {
    /* location serves to work around a design fault in the decoder, whereby
     * the process of reading a new slice that is the first slice of a new frame
     * requires the SEIRemovalApp::decode() method to be called again with the same
     * nal unit. */
    AnnexBStats stats = AnnexBStats();

    InputNALUnit nalu;
    byteStreamNALUnit(bytestream, nalu.getBitstream().getFifo(), stats);

    // call actual decoding function
    if (nalu.getBitstream().getFifo().empty())
    {
      /* this can happen if the following occur:
       *  - empty input file
       *  - two back-to-back start_code_prefixes
       *  - start_code_prefix immediately followed by EOF
       */
      std::cerr << "Warning: Attempt to decode an empty NAL unit" <<  std::endl;
    }
    else
    {
      read2( nalu );
      unitCnt++;

      bool bWrite = true;
      // just kick out all suffix SEIS
      bWrite &= (( !m_discardSuffixSEIs || nalu.m_nalUnitType != NAL_UNIT_SUFFIX_SEI ) && ( !m_discardPrefixSEIs || nalu.m_nalUnitType != NAL_UNIT_PREFIX_SEI ));
      bWrite &= unitCnt >= m_numNALUnitsToSkip;
      bWrite &= m_numNALUnitsToWrite < 0 || unitCnt <= m_numNALUnitsToWrite;

      if( bWrite )
      {
        int iNumZeros = stats.m_numLeadingZero8BitsBytes + stats.m_numZeroByteBytes + stats.m_numStartCodePrefixBytes -1;
        char ch = 0;
        for( int i = 0 ; i < iNumZeros; i++ ) { bitstreamFileOut.write( &ch, 1 ); }
        ch = 1; bitstreamFileOut.write( &ch, 1 );
        bitstreamFileOut.write( (const char*)nalu.getBitstream().getFifo().data(), nalu.getBitstream().getFifo().size() );
      }
    }
  }

  return 0;
}

//! \}
