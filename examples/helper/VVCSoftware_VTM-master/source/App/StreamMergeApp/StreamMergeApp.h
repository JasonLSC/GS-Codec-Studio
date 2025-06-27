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

/** \file     SEIRemovalApp.h
    \brief    Decoder application class (header)
*/

#ifndef __STREAMMERGEAPP__
#define __STREAMMERGEAPP__

#pragma once

#include <stdio.h>
#include <fstream>
#include <iostream>
#include "CommonDef.h"
#include "NALread.h"
#include "CABACWriter.h"
#include "AnnexBread.h"
#include "VLCReader.h"
#include "VLCWriter.h"
#include "StreamMergeAppCfg.h"

struct MergeLayer;
class SingleLayerStream;
using OldToNewIdMapping = std::map<uint32_t, uint32_t>;

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// stream merger application class
class StreamMergeApp : public StreamMergeAppCfg
{

public:
  StreamMergeApp();
  virtual ~StreamMergeApp() {}

  VPS vps;

  uint32_t mergeStreams();   ///< main stream merging function

private:
  bool isNewPicture(std::ifstream *bitstreamFile, InputByteStream *bytestream, bool firstSliceInPicture);
  bool isNewAccessUnit(bool newPicture, std::ifstream *bitstreamFile, InputByteStream *bytestream);
  void inputNaluHeaderToOutputNalu(InputNALUnit &inNalu, OutputNALUnit &outNalu);
  bool preInjectNalu(MergeLayer &layer, InputNALUnit &inNalu, OutputNALUnit &outNalu);
  void decodeAndRewriteNalu(MergeLayer &layer, InputNALUnit &inNalu, OutputNALUnit &outNalu);

  int vpsId = -1;
  int idIncrement = 0;
};




struct MergeLayer
{
  int id;

  std::ifstream             *fp;
  InputByteStream *          bs;
  bool                       firstSliceInPicture = true;
  bool                       doneReading = false;
  std::vector<AnnexBStats>   stats;
  ParameterSetManager        oldIDsPsManager;
  ParameterSetManager        psManager;
  std::vector<int>           vpsIds;
  std::vector<int>           spsIds;
  std::vector<int>           ppsIds;

  OldToNewIdMapping vpsIdMapping;
  OldToNewIdMapping spsIdMapping;
  OldToNewIdMapping ppsIdMapping;
  OldToNewIdMapping apsIdMapping;
};


class SingleLayerStream
{
public:
  /**
  * Create a bytestream reader that will extract bytes from
  * istream.
  *
  * NB, it isn't safe to access istream while in use by a
  * InputByteStream.
  *
  * Side-effects: the exception mask of istream is set to eofbit
  */
  SingleLayerStream()
    : m_numFutureBytes(0)
    , m_futureBytes(0)
  {
  }

  /**
  * Reset the internal state.  Must be called if input stream is
  * modified externally to this class
  */
  void reset()
  {
    m_numFutureBytes = 0;
    m_futureBytes = 0;
  }

  void init(std::istream& istream)
  {
    istream.exceptions(std::istream::eofbit | std::istream::badbit);
  }

  /**
  * returns true if an EOF will be encountered within the next
  * n bytes.
  */
  bool eofBeforeNBytes(uint32_t n, std::istream &m_input)
  {
    CHECK(n > 4, "Unsupported look-ahead value");
    if (m_numFutureBytes >= n)
    {
      return false;
    }

    n -= m_numFutureBytes;
    try
    {
      for (uint32_t i = 0; i < n; i++)
      {
        m_futureBytes = (m_futureBytes << 8) | m_input.get();
        m_numFutureBytes++;
      }
    }
    catch (...)
    {
      return true;
    }
    return false;
  }

  /**
  * return the next n bytes in the stream without advancing
  * the stream pointer.
  *
  * Returns: an unsigned integer representing an n byte bigendian
  * word.
  *
  * If an attempt is made to read past EOF, an n-byte word is
  * returned, but the portion that required input bytes beyond EOF
  * is undefined.
  *
  */
  uint32_t peekBytes(uint32_t n, std::istream &m_input)
  {
    eofBeforeNBytes(n, m_input);
    return m_futureBytes >> 8 * (m_numFutureBytes - n);
  }

  /**
  * consume and return one byte from the input.
  *
  * If bytestream is already at EOF prior to a call to readByte(),
  * an exception std::ios_base::failure is thrown.
  */
  uint8_t readByte(std::istream &m_input)
  {
    if (!m_numFutureBytes)
    {
      uint8_t byte = m_input.get();
      return byte;
    }
    m_numFutureBytes--;
    uint8_t wantedByte = m_futureBytes >> 8 * m_numFutureBytes;
    m_futureBytes &= ~(0xff << 8 * m_numFutureBytes);
    return wantedByte;
  }

  /**
  * consume and return n bytes from the input.  n bytes from
  * bytestream are interpreted as bigendian when assembling
  * the return value.
  */
  uint32_t readBytes(uint32_t n, std::istream &m_input)
  {
    uint32_t val = 0;
    for (uint32_t i = 0; i < n; i++)
    {
      val = (val << 8) | readByte(m_input);
    }
    return val;
  }

private:
  uint32_t m_numFutureBytes; /* number of valid bytes in m_futureBytes */
  uint32_t m_futureBytes; /* bytes that have been peeked */
};

bool byteStreamNALUnit(SingleLayerStream &bs, std::istream &istream, std::vector<uint8_t> &nalUnit, AnnexBStats &stats);

#endif // __STREAMMERGEAPP__

