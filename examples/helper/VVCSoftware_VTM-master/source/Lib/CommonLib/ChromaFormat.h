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

#ifndef __CHROMAFORMAT__
#define __CHROMAFORMAT__

#include "Common.h"
#include "CommonDef.h"
#include "Rom.h"

#include <iostream>
#include <vector>

//======================================================================================================================
//Chroma format utility functions  =====================================================================================
//======================================================================================================================

constexpr ChannelType toChannelType(ComponentID c)
{
  return c == COMPONENT_Y ? ChannelType::LUMA : ChannelType::CHROMA;
}

constexpr bool isLuma(ComponentID c) { return c == COMPONENT_Y; }
constexpr bool isLuma(ChannelType ch) { return ch == ChannelType::LUMA; }
constexpr bool isChroma(ComponentID c) { return c != COMPONENT_Y; }
constexpr bool isChroma(ChannelType ch) { return ch != ChannelType::LUMA; }

constexpr uint32_t getChannelTypeScaleX(ChannelType ch, ChromaFormat cf)
{
  return isLuma(ch) || cf == ChromaFormat::_444 ? 0 : 1;
}

constexpr uint32_t getChannelTypeScaleY(ChannelType ch, ChromaFormat cf)
{
  return isLuma(ch) || cf != ChromaFormat::_420 ? 0 : 1;
}

constexpr uint32_t getComponentScaleX(ComponentID c, ChromaFormat cf)
{
  return getChannelTypeScaleX(toChannelType(c), cf);
}

constexpr uint32_t getComponentScaleY(ComponentID c, ChromaFormat cf)
{
  return getChannelTypeScaleY(toChannelType(c), cf);
}

constexpr bool isChromaEnabled(ChromaFormat cf) { return cf != ChromaFormat::_400; }

constexpr uint32_t getNumberValidComponents(ChromaFormat cf) { return !isChromaEnabled(cf) ? 1 : MAX_NUM_COMPONENT; }
constexpr uint32_t getNumberValidChannels(ChromaFormat cf) { return !isChromaEnabled(cf) ? 1 : MAX_NUM_CHANNEL_TYPE; }

constexpr ChannelType getLastChannel(ChromaFormat cf)
{
  return !isChromaEnabled(cf) ? ChannelType::LUMA : ChannelType::CHROMA;
}

constexpr ComponentID getFirstComponentOfChannel(ChannelType ch) { return isLuma(ch) ? COMPONENT_Y : COMPONENT_Cb; }

InputColourSpaceConversion stringToInputColourSpaceConvert(const std::string &value, const bool bIsForward);
std::string getListOfColourSpaceConverts(const bool bIsForward);

//------------------------------------------------

static inline uint32_t getTotalSamples(const uint32_t width, const uint32_t height, const ChromaFormat format)
{
  const uint32_t samplesPerChannel = width * height;

  switch (format)
  {
  case ChromaFormat::_400:
    return samplesPerChannel;
    break;
  case ChromaFormat::_420:
    return (samplesPerChannel * 3) >> 1;
    break;
  case ChromaFormat::_422:
    return samplesPerChannel * 2;
    break;
  case ChromaFormat::_444:
    return samplesPerChannel * 3;
    break;
  default:
    EXIT("ERROR: Unrecognised chroma format in getTotalSamples() ");
    break;
  }

  return MAX_UINT;
}

//======================================================================================================================
//Intra prediction  ====================================================================================================
//======================================================================================================================

//------------------------------------------------

static inline int getTransformShift(const int channelBitDepth, const Size size, const int maxLog2TrDynamicRange)
{
  return maxLog2TrDynamicRange - channelBitDepth - ( ( floorLog2(size.width) + floorLog2(size.height) ) >> 1 );
}


//------------------------------------------------

//======================================================================================================================
//Scaling lists  =======================================================================================================
//======================================================================================================================

static inline int getScalingListType(const PredMode predMode, const ComponentID compID)
{
  return ((predMode == MODE_INTRA) ? 0 : MAX_NUM_COMPONENT) + MAP_CHROMA(compID);
}

#endif
