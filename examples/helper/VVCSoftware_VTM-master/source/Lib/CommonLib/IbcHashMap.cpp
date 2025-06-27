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

/** \file     IbcHashMap.cpp
    \brief    IBC hash map encoder class
*/

#include "CommonLib/dtrace_codingstruct.h"
#include "CommonLib/Picture.h"
#include "CommonLib/UnitTools.h"
#include "IbcHashMap.h"

//! \ingroup IbcHashMap
//! \{

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

IbcHashMap::IbcHashMap()
{
  m_picWidth  = 0;
  m_picHeight = 0;

  m_computeCrc32c = xxComputeCrc32c16bit;

#if ENABLE_SIMD_OPT_IBC
#ifdef TARGET_SIMD_X86
  initIbcHashMapX86();
#endif
#endif
}

IbcHashMap::~IbcHashMap()
{
  destroy();
}

void IbcHashMap::init(const int picWidth, const int picHeight)
{
  if (picWidth != m_picWidth || picHeight != m_picHeight)
  {
    destroy();
  }

  m_picWidth = picWidth;
  m_picHeight = picHeight;
  m_pos2Hash.resize(m_picWidth * m_picHeight);
  m_pos2HashStride = m_picWidth;
}

void IbcHashMap::destroy() { std::vector<unsigned int>().swap(m_pos2Hash); }

////////////////////////////////////////////////////////
// CRC32C calculation in C code, same results as SSE 4.2's implementation
static const uint32_t crc32Table[256] = {
  0x00000000L, 0xF26B8303L, 0xE13B70F7L, 0x1350F3F4L,
  0xC79A971FL, 0x35F1141CL, 0x26A1E7E8L, 0xD4CA64EBL,
  0x8AD958CFL, 0x78B2DBCCL, 0x6BE22838L, 0x9989AB3BL,
  0x4D43CFD0L, 0xBF284CD3L, 0xAC78BF27L, 0x5E133C24L,
  0x105EC76FL, 0xE235446CL, 0xF165B798L, 0x030E349BL,
  0xD7C45070L, 0x25AFD373L, 0x36FF2087L, 0xC494A384L,
  0x9A879FA0L, 0x68EC1CA3L, 0x7BBCEF57L, 0x89D76C54L,
  0x5D1D08BFL, 0xAF768BBCL, 0xBC267848L, 0x4E4DFB4BL,
  0x20BD8EDEL, 0xD2D60DDDL, 0xC186FE29L, 0x33ED7D2AL,
  0xE72719C1L, 0x154C9AC2L, 0x061C6936L, 0xF477EA35L,
  0xAA64D611L, 0x580F5512L, 0x4B5FA6E6L, 0xB93425E5L,
  0x6DFE410EL, 0x9F95C20DL, 0x8CC531F9L, 0x7EAEB2FAL,
  0x30E349B1L, 0xC288CAB2L, 0xD1D83946L, 0x23B3BA45L,
  0xF779DEAEL, 0x05125DADL, 0x1642AE59L, 0xE4292D5AL,
  0xBA3A117EL, 0x4851927DL, 0x5B016189L, 0xA96AE28AL,
  0x7DA08661L, 0x8FCB0562L, 0x9C9BF696L, 0x6EF07595L,
  0x417B1DBCL, 0xB3109EBFL, 0xA0406D4BL, 0x522BEE48L,
  0x86E18AA3L, 0x748A09A0L, 0x67DAFA54L, 0x95B17957L,
  0xCBA24573L, 0x39C9C670L, 0x2A993584L, 0xD8F2B687L,
  0x0C38D26CL, 0xFE53516FL, 0xED03A29BL, 0x1F682198L,
  0x5125DAD3L, 0xA34E59D0L, 0xB01EAA24L, 0x42752927L,
  0x96BF4DCCL, 0x64D4CECFL, 0x77843D3BL, 0x85EFBE38L,
  0xDBFC821CL, 0x2997011FL, 0x3AC7F2EBL, 0xC8AC71E8L,
  0x1C661503L, 0xEE0D9600L, 0xFD5D65F4L, 0x0F36E6F7L,
  0x61C69362L, 0x93AD1061L, 0x80FDE395L, 0x72966096L,
  0xA65C047DL, 0x5437877EL, 0x4767748AL, 0xB50CF789L,
  0xEB1FCBADL, 0x197448AEL, 0x0A24BB5AL, 0xF84F3859L,
  0x2C855CB2L, 0xDEEEDFB1L, 0xCDBE2C45L, 0x3FD5AF46L,
  0x7198540DL, 0x83F3D70EL, 0x90A324FAL, 0x62C8A7F9L,
  0xB602C312L, 0x44694011L, 0x5739B3E5L, 0xA55230E6L,
  0xFB410CC2L, 0x092A8FC1L, 0x1A7A7C35L, 0xE811FF36L,
  0x3CDB9BDDL, 0xCEB018DEL, 0xDDE0EB2AL, 0x2F8B6829L,
  0x82F63B78L, 0x709DB87BL, 0x63CD4B8FL, 0x91A6C88CL,
  0x456CAC67L, 0xB7072F64L, 0xA457DC90L, 0x563C5F93L,
  0x082F63B7L, 0xFA44E0B4L, 0xE9141340L, 0x1B7F9043L,
  0xCFB5F4A8L, 0x3DDE77ABL, 0x2E8E845FL, 0xDCE5075CL,
  0x92A8FC17L, 0x60C37F14L, 0x73938CE0L, 0x81F80FE3L,
  0x55326B08L, 0xA759E80BL, 0xB4091BFFL, 0x466298FCL,
  0x1871A4D8L, 0xEA1A27DBL, 0xF94AD42FL, 0x0B21572CL,
  0xDFEB33C7L, 0x2D80B0C4L, 0x3ED04330L, 0xCCBBC033L,
  0xA24BB5A6L, 0x502036A5L, 0x4370C551L, 0xB11B4652L,
  0x65D122B9L, 0x97BAA1BAL, 0x84EA524EL, 0x7681D14DL,
  0x2892ED69L, 0xDAF96E6AL, 0xC9A99D9EL, 0x3BC21E9DL,
  0xEF087A76L, 0x1D63F975L, 0x0E330A81L, 0xFC588982L,
  0xB21572C9L, 0x407EF1CAL, 0x532E023EL, 0xA145813DL,
  0x758FE5D6L, 0x87E466D5L, 0x94B49521L, 0x66DF1622L,
  0x38CC2A06L, 0xCAA7A905L, 0xD9F75AF1L, 0x2B9CD9F2L,
  0xFF56BD19L, 0x0D3D3E1AL, 0x1E6DCDEEL, 0xEC064EEDL,
  0xC38D26C4L, 0x31E6A5C7L, 0x22B65633L, 0xD0DDD530L,
  0x0417B1DBL, 0xF67C32D8L, 0xE52CC12CL, 0x1747422FL,
  0x49547E0BL, 0xBB3FFD08L, 0xA86F0EFCL, 0x5A048DFFL,
  0x8ECEE914L, 0x7CA56A17L, 0x6FF599E3L, 0x9D9E1AE0L,
  0xD3D3E1ABL, 0x21B862A8L, 0x32E8915CL, 0xC083125FL,
  0x144976B4L, 0xE622F5B7L, 0xF5720643L, 0x07198540L,
  0x590AB964L, 0xAB613A67L, 0xB831C993L, 0x4A5A4A90L,
  0x9E902E7BL, 0x6CFBAD78L, 0x7FAB5E8CL, 0x8DC0DD8FL,
  0xE330A81AL, 0x115B2B19L, 0x020BD8EDL, 0xF0605BEEL,
  0x24AA3F05L, 0xD6C1BC06L, 0xC5914FF2L, 0x37FACCF1L,
  0x69E9F0D5L, 0x9B8273D6L, 0x88D28022L, 0x7AB90321L,
  0xAE7367CAL, 0x5C18E4C9L, 0x4F48173DL, 0xBD23943EL,
  0xF36E6F75L, 0x0105EC76L, 0x12551F82L, 0xE03E9C81L,
  0x34F4F86AL, 0xC69F7B69L, 0xD5CF889DL, 0x27A40B9EL,
  0x79B737BAL, 0x8BDCB4B9L, 0x988C474DL, 0x6AE7C44EL,
  0xBE2DA0A5L, 0x4C4623A6L, 0x5F16D052L, 0xAD7D5351L
};

uint32_t IbcHashMap::xxComputeCrc32c16bit(uint32_t crc, const Pel* pels, size_t n)
{
  for (size_t i = 0; i < n; i++)
  {
    auto   p    = reinterpret_cast<const uint8_t*>(pels + i);
    size_t size = 2;

    while (size--)
    {
      crc = crc32Table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    }
  }

  return crc;
}
// CRC calculation in C code
////////////////////////////////////////////////////////

unsigned int IbcHashMap::xxCalcBlockHash(const Pel *pel, const ptrdiff_t stride, const int width, const int height,
                                         unsigned int crc)
{
  for (int y = 0; y < height; y++)
  {
    crc = m_computeCrc32c(crc, pel + y * stride, width);
  }
  return crc;
}

template<ChromaFormat chromaFormat>
void IbcHashMap::xxBuildPicHashMap(const PelUnitBuf& pic)
{
  const int chromaScalingX     = getChannelTypeScaleX(ChannelType::CHROMA, chromaFormat);
  const int chromaScalingY     = getChannelTypeScaleY(ChannelType::CHROMA, chromaFormat);
  const int chromaMinBlkWidth = MIN_PU_SIZE >> chromaScalingX;
  const int chromaMinBlkHeight = MIN_PU_SIZE >> chromaScalingY;

  const Pel *pelY  = nullptr;
  const Pel *pelCb = nullptr;
  const Pel *pelCr = nullptr;

  Position pos;
  for (pos.y = 0; pos.y + MIN_PU_SIZE <= pic.Y().height; pos.y++)
  {
    // row pointer
    pelY = pic.Y().bufAt(0, pos.y);
    if (isChromaEnabled(chromaFormat))
    {
      int chromaY = pos.y >> chromaScalingY;
      pelCb = pic.Cb().bufAt(0, chromaY);
      pelCr = pic.Cr().bufAt(0, chromaY);
    }

    for (pos.x = 0; pos.x + MIN_PU_SIZE <= pic.Y().width; pos.x++)
    {
      // 0x1FF is just an initial value
      HashValue hashValue = 0x1FF;

      // luma part
      hashValue = xxCalcBlockHash(&pelY[pos.x], pic.Y().stride, MIN_PU_SIZE, MIN_PU_SIZE, hashValue);

      // chroma part
      if (isChromaEnabled(chromaFormat))
      {
        int chromaX = pos.x >> chromaScalingX;
        hashValue = xxCalcBlockHash(&pelCb[chromaX], pic.Cb().stride, chromaMinBlkWidth, chromaMinBlkHeight, hashValue);
        hashValue = xxCalcBlockHash(&pelCr[chromaX], pic.Cr().stride, chromaMinBlkWidth, chromaMinBlkHeight, hashValue);
      }

      // hash table
      m_hashPos.push_back(std::make_pair(hashValue, pos));
      m_pos2Hash[pos.y * m_pos2HashStride + pos.x] = hashValue;
    }
  }

  std::stable_sort(m_hashPos.begin(), m_hashPos.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
}

void IbcHashMap::rebuildPicHashMap(const PelUnitBuf& pic)
{
  m_hashPos.clear();

  switch (pic.chromaFormat)
  {
  case ChromaFormat::_400:
    xxBuildPicHashMap<ChromaFormat::_400>(pic);
    break;
  case ChromaFormat::_420:
    xxBuildPicHashMap<ChromaFormat::_420>(pic);
    break;
  case ChromaFormat::_422:
    xxBuildPicHashMap<ChromaFormat::_422>(pic);
    break;
  case ChromaFormat::_444:
    xxBuildPicHashMap<ChromaFormat::_444>(pic);
    break;
  default:
    THROW("invalid chroma fomat");
    break;
  }
}

bool IbcHashMap::ibcHashMatch(const Area& lumaArea, std::vector<Position>& cand, const CodingStructure& cs, const int maxCand, const int searchRange4SmallBlk)
{
  cand.clear();

  // find the block with least candidates
  size_t minSize = MAX_UINT;
  HashValue targetHashOneBlock = 0;
  Position targetBlockOffsetInCu(0, 0);
  // Find 4x4 block with fewest matches
  for (SizeType y = 0; y < lumaArea.height && minSize > 1; y += MIN_PU_SIZE)
  {
    for (SizeType x = 0; x < lumaArea.width && minSize > 1; x += MIN_PU_SIZE)
    {
      HashValue hash = m_pos2Hash[(lumaArea.pos().y + y) * m_pos2HashStride + lumaArea.pos().x + x];
      if (getNumPositions(hash) < minSize)
      {
        minSize            = getNumPositions(hash);
        targetHashOneBlock = hash;
        targetBlockOffsetInCu.repositionTo(Position(x, y));
      }
    }
  }

  if (getNumPositions(targetHashOneBlock) > 1)
  {
    auto [first, last] = getPositions(targetHashOneBlock);

    // check whether whole block match
    for (auto it = first; it != last; it++)
    {
      auto     refBlockPos     = it->second;
      Position topLeft         = refBlockPos.offset(-targetBlockOffsetInCu.x, -targetBlockOffsetInCu.y);
      Position bottomRight = topLeft.offset(lumaArea.width - 1, lumaArea.height - 1);
      bool wholeBlockMatch = true;
      if (lumaArea.width > MIN_PU_SIZE || lumaArea.height > MIN_PU_SIZE)
      {
        if (!cs.isDecomp(bottomRight, ChannelType::LUMA) || bottomRight.x >= m_picWidth || bottomRight.y >= m_picHeight
            || topLeft.x < 0 || topLeft.y < 0)
        {
          continue;
        }
        for (SizeType y = 0; y < lumaArea.height && wholeBlockMatch; y += MIN_PU_SIZE)
        {
          for (SizeType x = 0; x < lumaArea.width && wholeBlockMatch; x += MIN_PU_SIZE)
          {
            // whether the reference block and current block has the same hash
            wholeBlockMatch &= (m_pos2Hash[(lumaArea.pos().y + y) * m_pos2HashStride + lumaArea.pos().x + x]
                                == m_pos2Hash[(topLeft.y + y) * m_pos2HashStride + topLeft.x + x]);
          }
        }
      }
      else
      {
        CHECK(topLeft != refBlockPos, "4x4 target block should not have offset!");
        if (abs(topLeft.x - lumaArea.x) > searchRange4SmallBlk || abs(topLeft.y - lumaArea.y) > searchRange4SmallBlk
            || !cs.isDecomp(bottomRight, ChannelType::LUMA))
        {
          continue;
        }
      }
      if (wholeBlockMatch)
      {
        cand.push_back(topLeft);
        if (cand.size() > maxCand)
        {
          break;
        }
      }
    }
  }

  return cand.size() > 0;
}

int IbcHashMap::getHashHitRatio(const Area& lumaArea)
{
  int maxX = std::min((int)(lumaArea.x + lumaArea.width), m_picWidth);
  int maxY = std::min((int)(lumaArea.y + lumaArea.height), m_picHeight);
  int hit   = 0;
  int total = 0;
  for (int y = lumaArea.y; y < maxY; y += MIN_PU_SIZE)
  {
    for (int x = lumaArea.x; x < maxX; x += MIN_PU_SIZE)
    {
      const HashValue hash = m_pos2Hash[y * m_pos2HashStride + x];
      hit += (getNumPositions(hash) > 1);
      total++;
    }
  }
  return 100 * hit / total;
}

int IbcHashMap::calHashBlkMatchPerc(const Area& lumaArea)
{
  int maxX = std::min((int)(lumaArea.x + lumaArea.width), m_picWidth);
  int maxY = std::min((int) (lumaArea.y + lumaArea.height), m_picHeight);

  static constexpr int numExcludedHashValue = 36;

  std::array<int, numExcludedHashValue>          maxUsage;
  std::array<unsigned int, numExcludedHashValue> mostSelHash;

  maxUsage.fill(0);
  mostSelHash.fill(0);

  int k = 0;
  while (k < m_hashPos.size())
  {
    HashValue hash  = m_hashPos[k].first;
    int       usage = 1;
    while (k + usage < m_hashPos.size() && m_hashPos[k + usage].first == hash)
    {
      usage++;
    }

    int insertPos = 0;
    for (; insertPos < numExcludedHashValue; insertPos++)
    {
      if (usage > maxUsage[insertPos])
      {
        break;
      }
    }
    assert(insertPos <= numExcludedHashValue);

    if (insertPos < numExcludedHashValue)
    {
      for (int i = numExcludedHashValue - 1; i > insertPos; i--)
      {
        maxUsage[i] = maxUsage[i - 1];
        mostSelHash[i] = mostSelHash[i - 1];
      }
      maxUsage[insertPos] = usage;
      mostSelHash[insertPos] = hash;
    }

    k += usage;
  }

  int hit   = 0;
  int total = 0;
  for (int y = lumaArea.y; y < maxY; y += MIN_PU_SIZE)
  {
    for (int x = lumaArea.x; x < maxX; x += MIN_PU_SIZE)
    {
      HashValue hash = m_pos2Hash[y * m_pos2HashStride + x];

      bool excludedHash = false;
      for (int i = 0; i < numExcludedHashValue && !excludedHash; i++)
      {
        if (hash == mostSelHash[i])
        {
          excludedHash = true;
        }
      }

      if (excludedHash)
      {
        continue;
      }

      hit += (getNumPositions(hash) > 1);
      total++;
    }
  }

  if (total == 0)
  {
    return 0;
  }
  else
  {
    return 100 * hit / total;
  }
}
//! \}
