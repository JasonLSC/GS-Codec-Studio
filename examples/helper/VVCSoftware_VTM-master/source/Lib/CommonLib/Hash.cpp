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

 /** \file     TEncHash.cpp
     \brief    hash encoder class
 */
#include "CommonLib/dtrace_codingstruct.h"
#include "CommonLib/Picture.h"
#include "CommonLib/UnitTools.h"
#include "Hash.h"



 // ====================================================================================================================
 // Constructor / destructor / create / destroy
 // ====================================================================================================================

CrcCalculatorLight Hash::m_crcCalculator1(24, 0x5D6DCB);
CrcCalculatorLight Hash::m_crcCalculator2(24, 0x864CFB);

CrcCalculatorLight::CrcCalculatorLight(uint32_t bits, uint32_t truncPoly)
{
  m_remainder = 0;
  m_bits = bits;
  m_truncPoly = truncPoly;
  m_finalResultMask = (1 << bits) - 1;

  xInitTable();
}

CrcCalculatorLight::~CrcCalculatorLight() {}

void CrcCalculatorLight::xInitTable()
{
  const uint32_t highBit = 1 << (m_bits - 1);
  const uint32_t byteHighBit = 1 << (8 - 1);

  for (uint32_t value = 0; value < 256; value++)
  {
    uint32_t remainder = 0;
    for (uint8_t mask = byteHighBit; mask != 0; mask >>= 1)
    {
      if (value & mask)
      {
        remainder ^= highBit;
      }

      if (remainder & highBit)
      {
        remainder <<= 1;
        remainder ^= m_truncPoly;
      }
      else
      {
        remainder <<= 1;
      }
    }

    m_table[value] = remainder;
  }
}

void CrcCalculatorLight::processData(const uint8_t *curData, const size_t dataLength)
{
  for (size_t i = 0; i < dataLength; i++)
  {
    uint8_t index = (m_remainder >> (m_bits - 8)) ^ curData[i];
    m_remainder <<= 8;
    m_remainder ^= m_table[index];
  }
}

Hash::Hash()
{
  m_lookupTable   = nullptr;
  tableHasContent = false;
  hashPic.fill(nullptr);
}

Hash::~Hash()
{
  clearAll();
  if (m_lookupTable != nullptr)
  {
    delete[] m_lookupTable;
    m_lookupTable = nullptr;
  }
}

void Hash::create(int picWidth, int picHeight)
{
  if (m_lookupTable)
  {
    clearAll();
  }

  if (hashPic.front() == nullptr)
  {
    for (auto &p: hashPic)
    {
      p = new uint16_t[picWidth * picHeight];
    }
  }
  if (m_lookupTable)
  {
    return;
  }
  const int maxAddr = 1 << (CRC_BITS + LOG_SIZE_BITS);
  m_lookupTable = new std::vector<BlockHash>*[maxAddr];
  std::fill_n(m_lookupTable, maxAddr, nullptr);
  tableHasContent = false;
}

void Hash::clearAll()
{
  if (hashPic.front() != nullptr)
  {
    for (auto &p: hashPic)
    {
      delete[] p;
      p = nullptr;
    }
  }
  tableHasContent = false;
  if (m_lookupTable == nullptr)
  {
    return;
  }
  const int maxAddr = 1 << (CRC_BITS + LOG_SIZE_BITS);
  for (int i = 0; i < maxAddr; i++)
  {
    if (m_lookupTable[i] != nullptr)
    {
      delete m_lookupTable[i];
      m_lookupTable[i] = nullptr;
    }
  }
}

void Hash::addToTable(uint32_t hashValue, const BlockHash &blockHash)
{
  if (m_lookupTable[hashValue] == nullptr)
  {
    m_lookupTable[hashValue] = new std::vector<BlockHash>;
    m_lookupTable[hashValue]->push_back(blockHash);
  }
  else
  {
    m_lookupTable[hashValue]->push_back(blockHash);
  }
}

int Hash::count(uint32_t hashValue)
{
  if (m_lookupTable[hashValue] == nullptr)
  {
    return 0;
  }
  else
  {
    return static_cast<int>(m_lookupTable[hashValue]->size());
  }
}

int Hash::count(uint32_t hashValue) const
{
  if (m_lookupTable[hashValue] == nullptr)
  {
    return 0;
  }
  else
  {
    return static_cast<int>(m_lookupTable[hashValue]->size());
  }
}

MapIterator Hash::getFirstIterator(uint32_t hashValue) { return m_lookupTable[hashValue]->begin(); }

const MapIterator Hash::getFirstIterator(uint32_t hashValue) const { return m_lookupTable[hashValue]->begin(); }

bool Hash::hasExactMatch(uint32_t hashValue1, uint32_t hashValue2)
{
  if (m_lookupTable[hashValue1] == nullptr)
  {
    return false;
  }
  std::vector<BlockHash>::iterator it;
  for (it = m_lookupTable[hashValue1]->begin(); it != m_lookupTable[hashValue1]->end(); it++)
  {
    if ((*it).hashValue2 == hashValue2)
    {
      return true;
    }
  }
  return false;
}

void Hash::generateBlock2x2HashValue(const PelUnitBuf &curPicBuf, int picWidth, int picHeight,
                                     const BitDepths bitDepths, uint32_t *picBlockHash[2], bool *picBlockSameInfo[3])
{
  const int width = 2;
  const int height = 2;
  int xEnd = picWidth - width + 1;
  int yEnd = picHeight - height + 1;

  int length = width * 2;
  bool includeChroma = false;
  if ((curPicBuf).chromaFormat == ChromaFormat::_444)
  {
    length *= 3;
    includeChroma = true;
  }
  unsigned char* p = new unsigned char[length];

  int pos = 0;
  for (int yPos = 0; yPos < yEnd; yPos++)
  {
    for (int xPos = 0; xPos < xEnd; xPos++)
    {
      Hash::getPixelsIn1DCharArrayByBlock2x2(curPicBuf, p, xPos, yPos, bitDepths, includeChroma);
      picBlockSameInfo[0][pos] = isBlock2x2RowSameValue(p, includeChroma);
      picBlockSameInfo[1][pos] = isBlock2x2ColSameValue(p, includeChroma);

      picBlockHash[0][pos] = Hash::getCRCValue1(p, length * sizeof(unsigned char));
      picBlockHash[1][pos] = Hash::getCRCValue2(p, length * sizeof(unsigned char));

      pos++;
    }
    pos += width - 1;
  }

  delete[] p;
}

void Hash::generateBlockHashValue(int picWidth, int picHeight, int width, int height, uint32_t *srcPicBlockHash[2],
                                  uint32_t *dstPicBlockHash[2], bool *srcPicBlockSameInfo[3],
                                  bool *dstPicBlockSameInfo[3])
{
  int xEnd = picWidth - width + 1;
  int yEnd = picHeight - height + 1;

  int srcWidth = width >> 1;
  int quadWidth = width >> 2;
  int srcHeight = height >> 1;
  int quadHeight = height >> 2;

  int length = 4 * sizeof(uint32_t);

  uint32_t p[4];
  int pos = 0;
  for (int yPos = 0; yPos < yEnd; yPos++)
  {
    for (int xPos = 0; xPos < xEnd; xPos++)
    {
      p[0] = srcPicBlockHash[0][pos];
      p[1] = srcPicBlockHash[0][pos + srcWidth];
      p[2] = srcPicBlockHash[0][pos + srcHeight * picWidth];
      p[3] = srcPicBlockHash[0][pos + srcHeight * picWidth + srcWidth];
      dstPicBlockHash[0][pos] = Hash::getCRCValue1((unsigned char *) p, length);

      p[0] = srcPicBlockHash[1][pos];
      p[1] = srcPicBlockHash[1][pos + srcWidth];
      p[2] = srcPicBlockHash[1][pos + srcHeight * picWidth];
      p[3] = srcPicBlockHash[1][pos + srcHeight * picWidth + srcWidth];
      dstPicBlockHash[1][pos] = Hash::getCRCValue2((unsigned char *) p, length);

      dstPicBlockSameInfo[0][pos] = srcPicBlockSameInfo[0][pos] && srcPicBlockSameInfo[0][pos + quadWidth] && srcPicBlockSameInfo[0][pos + srcWidth]
        && srcPicBlockSameInfo[0][pos + srcHeight * picWidth] && srcPicBlockSameInfo[0][pos + srcHeight * picWidth + quadWidth] && srcPicBlockSameInfo[0][pos + srcHeight * picWidth + srcWidth];

      dstPicBlockSameInfo[1][pos] = srcPicBlockSameInfo[1][pos] && srcPicBlockSameInfo[1][pos + srcWidth] && srcPicBlockSameInfo[1][pos + quadHeight * picWidth]
        && srcPicBlockSameInfo[1][pos + quadHeight * picWidth + srcWidth] && srcPicBlockSameInfo[1][pos + srcHeight * picWidth] && srcPicBlockSameInfo[1][pos + srcHeight * picWidth + srcWidth];

      pos++;
    }
    pos += width - 1;
  }

  if (width >= 4)
  {
    pos = 0;

    for (int yPos = 0; yPos < yEnd; yPos++)
    {
      for (int xPos = 0; xPos < xEnd; xPos++)
      {
        dstPicBlockSameInfo[2][pos] = (!dstPicBlockSameInfo[0][pos] && !dstPicBlockSameInfo[1][pos]);
        pos++;
      }
      pos += width - 1;
    }
  }
}

void Hash::addToHashMapByRowWithPrecalData(uint32_t *picHash[2], bool *picIsSame, int picWidth, int picHeight,
                                           int width, int height)
{
  int xEnd = picWidth - width + 1;
  int yEnd = picHeight - height + 1;

  bool* srcIsAdded = picIsSame;
  uint32_t* srcHash[2] = { picHash[0], picHash[1] };

  int addValue = getIndexFromBlockSize(width, height);
  CHECK(addValue < 0, "Wrong")
  addValue <<= CRC_BITS;
  const int crcMask  = (1 << CRC_BITS) - 1;
  const int blockIdx = floorLog2(width) - MIN_LOG_BLK_SIZE;

  for (int xPos = 0; xPos < xEnd; xPos++)
  {
    for (int yPos = 0; yPos < yEnd; yPos++)
    {
      int pos = yPos * picWidth + xPos;
      hashPic[blockIdx][pos] = (uint16_t)(srcHash[1][pos] & crcMask);
      //valid data
      if (srcIsAdded[pos])
      {
        BlockHash blockHash;
        blockHash.x = xPos;
        blockHash.y = yPos;

        uint32_t hashValue1 = (srcHash[0][pos] & crcMask) + addValue;
        blockHash.hashValue2 = srcHash[1][pos];

        addToTable(hashValue1, blockHash);
      }
    }
  }
}

void Hash::getPixelsIn1DCharArrayByBlock2x2(const PelUnitBuf &curPicBuf, unsigned char *pixelsIn1D, int xStart,
                                            int yStart, const BitDepths &bitDepths, bool includeAllComponent)
{
  ChromaFormat fmt = (curPicBuf).chromaFormat;
  if (fmt != ChromaFormat::_444)
  {
    includeAllComponent = false;
  }

  if (bitDepths[ChannelType::LUMA] == 8 && bitDepths[ChannelType::CHROMA] == 8)
  {
    Pel* curPel[MAX_NUM_COMPONENT]={nullptr};
    ptrdiff_t stride[MAX_NUM_COMPONENT] = { 0 };
    const int maxComponent=includeAllComponent?MAX_NUM_COMPONENT:1;

    for (int id = 0; id < maxComponent; id++)
    {
      ComponentID compID = ComponentID(id);
      stride[id] = (curPicBuf).get(compID).stride;
      curPel[id] = (curPicBuf).get(compID).buf;
      curPel[id] += (yStart >> getComponentScaleY(compID, fmt)) * stride[id] + (xStart >> getComponentScaleX(compID, fmt));
    }

    int index = 0;
    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < 2; j++)
      {
        pixelsIn1D[index++] = static_cast<unsigned char>(curPel[0][j]);
        if (includeAllComponent)
        {
          pixelsIn1D[index++] = static_cast<unsigned char>(curPel[1][j]);
          pixelsIn1D[index++] = static_cast<unsigned char>(curPel[2][j]);
        }
      }
      curPel[0] += stride[0];
      if (includeAllComponent)
      {
        curPel[1] += stride[1];
        curPel[2] += stride[2];
      }
    }
  }
  else
  {
    int       shift                     = bitDepths[ChannelType::LUMA] - 8;
    int       shiftc                    = includeAllComponent ? (bitDepths[ChannelType::CHROMA] - 8) : 0;
    Pel* curPel[MAX_NUM_COMPONENT]={nullptr};
    ptrdiff_t stride[MAX_NUM_COMPONENT] = { 0 };
    const int maxComponent=includeAllComponent?MAX_NUM_COMPONENT:1;

    for (int id = 0; id < maxComponent; id++)
    {
      ComponentID compID = ComponentID(id);
      stride[id] = (curPicBuf).get(compID).stride;
      curPel[id] = (curPicBuf).get(compID).buf;
      curPel[id] += (yStart >> getComponentScaleY(compID, fmt)) * stride[id] + (xStart >> getComponentScaleX(compID, fmt));
    }

    int index = 0;
    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < 2; j++)
      {
        pixelsIn1D[index++] = static_cast<unsigned char>(curPel[0][j] >> shift);
        if (includeAllComponent)
        {
          pixelsIn1D[index++] = static_cast<unsigned char>(curPel[1][j] >> shiftc);
          pixelsIn1D[index++] = static_cast<unsigned char>(curPel[2][j] >> shiftc);
        }
      }
      curPel[0] += stride[0];
      if (includeAllComponent)
      {
        curPel[1] += stride[1];
        curPel[2] += stride[2];
      }
    }
  }
}

bool Hash::isBlock2x2RowSameValue(unsigned char *p, bool includeAllComponent)
{
  if (includeAllComponent)
  {
    if (p[0] != p[3] || p[6] != p[9])
    {
      return false;
    }
    if (p[1] != p[4] || p[7] != p[10])
    {
      return false;
    }
    if (p[2] != p[5] || p[8] != p[11])
    {
      return false;
    }
  }
  else
  {
    if (p[0] != p[1] || p[2] != p[3])
    {
      return false;
    }
  }

  return true;
}

bool Hash::isBlock2x2ColSameValue(unsigned char *p, bool includeAllComponent)
{
  if (includeAllComponent)
  {
    if (p[0] != p[6] || p[3] != p[9])
    {
      return false;
    }
    if (p[1] != p[7] || p[4] != p[10])
    {
      return false;
    }
    if (p[2] != p[8] || p[5] != p[11])
    {
      return false;
    }
  }
  else
  {
    if ((p[0] != p[2]) || (p[1] != p[3]))
    {
      return false;
    }
  }

  return true;
}

bool Hash::isHorizontalPerfectLuma(const Pel *srcPel, ptrdiff_t stride, int width, int height)
{
  for (int i = 0; i < height; i++)
  {
    for (int j = 1; j < width; j++)
    {
      if (srcPel[j] != srcPel[0])
      {
        return false;
      }
    }
    srcPel += stride;
  }
  return true;
}

bool Hash::isVerticalPerfectLuma(const Pel *srcPel, ptrdiff_t stride, int width, int height)
{
  for (int i = 0; i < width; i++)
  {
    for (int j = 1; j < height; j++)
    {
      if (srcPel[j*stride + i] != srcPel[i])
      {
        return false;
      }
    }
  }
  return true;
}

bool Hash::getBlockHashValue(const PelUnitBuf &curPicBuf, int width, int height, int xStart, int yStart,
                             const BitDepths bitDepths, uint32_t &hashValue1, uint32_t &hashValue2)
{
  int addValue = getIndexFromBlockSize(width, height);

  CHECK(addValue < 0, "Wrong")
  addValue <<= CRC_BITS;
  const int crcMask = (1 << CRC_BITS) - 1;

  const bool includeChroma = (curPicBuf).chromaFormat == ChromaFormat::_444;

  static_vector<uint8_t, 12> p;
  p.resize(4 * (includeChroma ? 3 : 1));

  int block2x2Num = (width*height) >> 2;

  uint32_t* hashValueBuffer[2][2];
  for (int i = 0; i < 2; i++)
  {
    for (int j = 0; j < 2; j++)
    {
      hashValueBuffer[i][j] = new uint32_t[block2x2Num];
    }
  }

  //2x2 subblock hash values in current CU
  int subBlockInWidth  = width >> 1;
  int subBlockInHeight = height >> 1;
  for (int yPos = 0; yPos < subBlockInHeight; yPos++)
  {
    for (int xPos = 0; xPos < subBlockInWidth; xPos++)
    {
      Hash::getPixelsIn1DCharArrayByBlock2x2(curPicBuf, p.data(), xStart + 2 * xPos, yStart + 2 * yPos, bitDepths,
                                             includeChroma);

      const int pos = yPos * subBlockInWidth + xPos;

      hashValueBuffer[0][0][pos] = Hash::getCRCValue1(p.data(), p.byte_size());
      hashValueBuffer[1][0][pos] = Hash::getCRCValue2(p.data(), p.byte_size());
    }
  }

  int srcSubBlockInWidth = subBlockInWidth;
  subBlockInWidth >>= 1;
  subBlockInHeight >>= 1;

  static_vector<uint32_t, 4> toHash;
  toHash.resize(4);

  int srcIdx = 1;
  int dstIdx = 0;

  //4x4 subblock hash values to current block hash values
  int minSize = std::min(height, width);
  for (int subWidth = 4; subWidth <= minSize; subWidth *= 2)
  {
    srcIdx = 1 - srcIdx;
    dstIdx = 1 - dstIdx;

    int dstPos = 0;
    for (int yPos = 0; yPos < subBlockInHeight; yPos++)
    {
      for (int xPos = 0; xPos < subBlockInWidth; xPos++)
      {
        const int srcPos = 2 * yPos * srcSubBlockInWidth + 2 * xPos;

        toHash[0] = hashValueBuffer[0][srcIdx][srcPos];
        toHash[1] = hashValueBuffer[0][srcIdx][srcPos + 1];
        toHash[2] = hashValueBuffer[0][srcIdx][srcPos + srcSubBlockInWidth];
        toHash[3] = hashValueBuffer[0][srcIdx][srcPos + srcSubBlockInWidth + 1];

        hashValueBuffer[0][dstIdx][dstPos] =
          getCRCValue1(reinterpret_cast<uint8_t *>(toHash.data()), toHash.byte_size());

        toHash[0] = hashValueBuffer[1][srcIdx][srcPos];
        toHash[1] = hashValueBuffer[1][srcIdx][srcPos + 1];
        toHash[2] = hashValueBuffer[1][srcIdx][srcPos + srcSubBlockInWidth];
        toHash[3] = hashValueBuffer[1][srcIdx][srcPos + srcSubBlockInWidth + 1];
        hashValueBuffer[1][dstIdx][dstPos] =
          getCRCValue2(reinterpret_cast<uint8_t *>(toHash.data()), toHash.byte_size());

        dstPos++;
      }
    }

    srcSubBlockInWidth = subBlockInWidth;
    subBlockInWidth >>= 1;
    subBlockInHeight >>= 1;
  }

  if (width != height)//currently support 1:2 or 2:1 block size
  {
    toHash.resize(2);
    CHECK(width != (height << 1) && (width << 1) != height, "Wrong")
    bool isHorizontal = width == (height << 1) ? true : false;
    srcIdx = 1 - srcIdx;
    dstIdx = 1 - dstIdx;
    if (isHorizontal)
    {
      toHash[0] = hashValueBuffer[0][srcIdx][0];
      toHash[1] = hashValueBuffer[0][srcIdx][1];
      hashValueBuffer[0][dstIdx][0] = getCRCValue1(reinterpret_cast<uint8_t *>(toHash.data()), toHash.byte_size());

      toHash[0] = hashValueBuffer[1][srcIdx][0];
      toHash[1] = hashValueBuffer[1][srcIdx][1];
      hashValueBuffer[1][dstIdx][0] = getCRCValue2(reinterpret_cast<uint8_t *>(toHash.data()), toHash.byte_size());
    }
    else
    {
      CHECK(srcSubBlockInWidth != 1, "Wrong")
      toHash[0] = hashValueBuffer[0][srcIdx][0];
      toHash[1] = hashValueBuffer[0][srcIdx][srcSubBlockInWidth];
      hashValueBuffer[0][dstIdx][0] = getCRCValue1(reinterpret_cast<uint8_t *>(toHash.data()), toHash.byte_size());

      toHash[0] = hashValueBuffer[1][srcIdx][0];
      toHash[1] = hashValueBuffer[1][srcIdx][srcSubBlockInWidth];
      hashValueBuffer[1][dstIdx][0] = getCRCValue2(reinterpret_cast<uint8_t *>(toHash.data()), toHash.byte_size());
    }
  }

  hashValue1 = (hashValueBuffer[0][dstIdx][0] & crcMask) + addValue;
  hashValue2 = hashValueBuffer[1][dstIdx][0];

  for (int i = 0; i < 2; i++)
  {
    for (int j = 0; j < 2; j++)
    {
      delete[] hashValueBuffer[i][j];
    }
  }

  return true;
}

uint32_t Hash::getCRCValue1(const uint8_t *p, size_t length)
{
  m_crcCalculator1.reset();
  m_crcCalculator1.processData(p, length);
  return m_crcCalculator1.getCRC();
}

uint32_t Hash::getCRCValue2(const uint8_t *p, size_t length)
{
  m_crcCalculator2.reset();
  m_crcCalculator2.processData(p, length);
  return m_crcCalculator2.getCRC();
}
//! \}
