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

#include <cmath>
#include "ProfileTierLevel.h"
#include "CommonLib/Slice.h"

ProfileTierLevel::ProfileTierLevel()
  : m_tierFlag(Level::MAIN)
  , m_profileIdc(Profile::NONE)
  , m_levelIdc(Level::NONE)
  , m_frameOnlyConstraintFlag(true)
  , m_multiLayerEnabledFlag(false)
{
  m_subLayerLevelPresentFlag.fill(false);
  m_subLayerLevelIdc.fill(Level::NONE);
}

bool operator == (const ProfileTierLevel& op1, const ProfileTierLevel& op2)
{
  if (op1.m_tierFlag != op2.m_tierFlag)
  {
    return false;
  }
  if (op1.m_profileIdc != op2.m_profileIdc)
  {
    return false;
  }
  if (op1.m_levelIdc != op2.m_levelIdc)
  {
    return false;
  }
  if (op1.m_frameOnlyConstraintFlag != op2.m_frameOnlyConstraintFlag)
  {
    return false;
  }
  if (op1.m_multiLayerEnabledFlag != op2.m_multiLayerEnabledFlag)
  {
    return false;
  }
  if (op1.m_constraintInfo != op2.m_constraintInfo)
  {
    return false;
  }
  if (op1.m_subProfileIdc != op2.m_subProfileIdc)
  {
    return false;
  }

  for (int i = 0; i < MAX_TLAYER - 1; i++)
  {
    if (op1.m_subLayerLevelPresentFlag[i] != op2.m_subLayerLevelPresentFlag[i])
    {
      return false;
    }
  }
  for (int i = 0; i < MAX_TLAYER; i++)
  {
    if (op1.m_subLayerLevelIdc[i] != op2.m_subLayerLevelIdc[i])
    {
      return false;
    }
  }
  return true;
}

bool operator != (const ProfileTierLevel& op1, const ProfileTierLevel& op2)
{
  return !(op1 == op2);
}

uint32_t TierLevelFeatures::getMaxPicWidthInLumaSamples()  const
{
  return uint32_t(sqrt(maxLumaPs*8.0));
}

uint32_t TierLevelFeatures::getMaxPicHeightInLumaSamples() const
{
  return uint32_t(sqrt(maxLumaPs*8.0));
}

static const uint64_t MAX_CNFUINT64 = std::numeric_limits<uint64_t>::max();

static const TierLevelFeatures mainTierLevelInfo[] =
{
      //  level,       maxlumaps,      maxcpb[tier],,  maxSlicesPerAu,maxTilesPerAu,cols, maxLumaSr,       maxBr[tier],,    minCr[tier],,
    { Level::LEVEL1  ,    36864, {      350,        0 },       16,        1,        1,     552960ULL, {     128,        0 }, { 2, 0} },
    { Level::LEVEL2  ,   122880, {     1500,        0 },       16,        1,        1,    3686400ULL, {    1500,        0 }, { 2, 0} },
    { Level::LEVEL2_1,   245760, {     3000,        0 },       20,        1,        1,    7372800ULL, {    3000,        0 }, { 2, 0} },
    { Level::LEVEL3  ,   552960, {     6000,        0 },       30,        4,        2,   16588800ULL, {    6000,        0 }, { 2, 0} },
    { Level::LEVEL3_1,   983040, {    10000,        0 },       40,        9,        3,   33177600ULL, {   10000,        0 }, { 2, 0} },
    { Level::LEVEL4  ,  2228224, {    12000,    30000 },       75,       25,        5,   66846720ULL, {   12000,    30000 }, { 4, 4} },
    { Level::LEVEL4_1,  2228224, {    20000,    50000 },       75,       25,        5,  133693440ULL, {   20000,    50000 }, { 4, 4} },
    { Level::LEVEL5  ,  8912896, {    25000,   100000 },      200,      110,       10,  267386880ULL, {   25000,   100000 }, { 6, 4} },
    { Level::LEVEL5_1,  8912896, {    40000,   160000 },      200,      110,       10,  534773760ULL, {   40000,   160000 }, { 8, 4} },
    { Level::LEVEL5_2,  8912896, {    60000,   240000 },      200,      110,       10, 1069547520ULL, {   60000,   240000 }, { 8, 4} },
    { Level::LEVEL6  , 35651584, {    80000,   240000 },      600,      440,       20, 1069547520ULL, {   60000,   240000 }, { 8, 4} },
    { Level::LEVEL6_1, 35651584, {   120000,   480000 },      600,      440,       20, 2139095040ULL, {  120000,   480000 }, { 8, 4} },
    { Level::LEVEL6_2, 35651584, {   180000,   800000 },      600,      440,       20, 4278190080ULL, {  240000,   800000 }, { 8, 4} },
    { Level::LEVEL6_3, 80216064, {   240000,  1600000 },     1000,      990,       30, 4812963840ULL, {  320000,  1600000 }, { 8, 4} },
    { Level::LEVEL15_5, MAX_UINT,{ MAX_UINT, MAX_UINT }, MAX_UINT, MAX_UINT, MAX_UINT, MAX_CNFUINT64, {MAX_UINT, MAX_UINT }, { 0, 0} },
    { Level::NONE    }
};

static const ProfileFeatures validProfiles[] = {
  // profile, pNameString, maxBitDepth, maxChrFmt, lvl15.5, cpbvcl, cpbnal, fcf*1000, mincr*100, levelInfo
  // most constrained profiles must appear first.
  { Profile::MAIN_10_STILL_PICTURE, "Main_10_Still_Picture", 10, ChromaFormat::_420, true, 1000, 1100, 1875, 100,
    mainTierLevelInfo, true },
  { Profile::MULTILAYER_MAIN_10_STILL_PICTURE, "Multilayer_Main_10_Still_Picture", 10, ChromaFormat::_420, true, 1000,
    1100, 1875, 100, mainTierLevelInfo, true },
  { Profile::MAIN_10_444_STILL_PICTURE, "Main_444_10_Still_Picture", 10, ChromaFormat::_444, true, 2500, 2750, 3750, 75,
    mainTierLevelInfo, true },
  { Profile::MULTILAYER_MAIN_10_444_STILL_PICTURE, "Multilayer_Main_444_10_Still_Picture", 10, ChromaFormat::_444, true,
    2500, 2750, 3750, 75, mainTierLevelInfo, true },
  { Profile::MAIN_10, "Main_10", 10, ChromaFormat::_420, true, 1000, 1100, 1875, 100, mainTierLevelInfo, false },
  { Profile::MULTILAYER_MAIN_10, "Multilayer_Main_10", 10, ChromaFormat::_420, true, 1000, 1100, 1875, 100,
    mainTierLevelInfo, false },
  { Profile::MAIN_10_444, "Main_444_10", 10, ChromaFormat::_444, true, 2500, 2750, 3750, 75, mainTierLevelInfo, false },
  { Profile::MULTILAYER_MAIN_10_444, "Multilayer_Main_444_10", 10, ChromaFormat::_444, true, 2500, 2750, 3750, 75,
    mainTierLevelInfo, false },
  { Profile::MAIN_12, "Main_12", 12, ChromaFormat::_420, true, 1200, 1320, 1875, 100, mainTierLevelInfo, false },
  { Profile::MAIN_12_INTRA, "Main_12_Intra", 12, ChromaFormat::_420, true, 2400, 2640, 1875, 100, mainTierLevelInfo,
    false },
  { Profile::MAIN_12_STILL_PICTURE, "Main_12_Still_Picture", 12, ChromaFormat::_420, true, 2400, 2640, 1875, 100,
    mainTierLevelInfo, false },
  { Profile::MAIN_12_444, "Main_12_444", 12, ChromaFormat::_444, true, 3000, 3300, 3750, 75, mainTierLevelInfo, false },
  { Profile::MAIN_12_444_INTRA, "Main_12_444_Intra", 12, ChromaFormat::_444, true, 6000, 6600, 3750, 75,
    mainTierLevelInfo, false },
  { Profile::MAIN_12_444_STILL_PICTURE, "Main_12_444_Still_Picture", 12, ChromaFormat::_444, true, 6000, 6600, 3750, 75,
    mainTierLevelInfo, false },
  { Profile::MAIN_16_444, "Main_16_444", 16, ChromaFormat::_444, true, 4000, 4400, 6000, 75, mainTierLevelInfo, false },
  { Profile::MAIN_16_444_INTRA, "Main_16_444_Intra", 16, ChromaFormat::_444, true, 8000, 8800, 6000, 75,
    mainTierLevelInfo, false },
  { Profile::MAIN_16_444_STILL_PICTURE, "Main_16_444_Still_Picture", 16, ChromaFormat::_444, true, 8000, 8800, 6000, 75,
    mainTierLevelInfo, false },
  { Profile::NONE, 0 },
};

const ProfileFeatures *ProfileFeatures::getProfileFeatures(const Profile::Name p)
{
  int i;
  for (i = 0; validProfiles[i].profile != Profile::NONE; i++)
  {
    if (validProfiles[i].profile == p)
    {
      return &validProfiles[i];
    }
  }

  return &validProfiles[i];
}

void ProfileTierLevelFeatures::extractPTLInformation(const SPS &sps)
{
  extractPTLInformation(*sps.getProfileTierLevel());
}

void ProfileTierLevelFeatures::extractPTLInformation(const ProfileTierLevel &ptl)
{
  const ProfileTierLevel &spsPtl = ptl;

  m_profile = nullptr;
  m_tierLevel = nullptr;
  m_tier = spsPtl.getTierFlag();

  // Identify the profile from the profile Idc, and possibly other constraints.
  for(int32_t i=0; validProfiles[i].profile != Profile::NONE; i++)
  {
    if (spsPtl.getProfileIdc() == validProfiles[i].profile)
    {
      m_profile = &(validProfiles[i]);
      break;
    }
  }

  if (m_profile != nullptr)
  {
    // Now identify the level:
    const TierLevelFeatures *tlf = m_profile->tierLevelListInfo;
    const Level::Name spsLevelName = spsPtl.getLevelIdc();
    if (spsLevelName!=Level::LEVEL15_5 || m_profile->canUseLevel15p5)
    {
      for(int i=0; tlf[i].level!=Level::NONE; i++)
      {
        if (tlf[i].level == spsLevelName)
        {
          m_tierLevel = &(tlf[i]);
        }
      }
    }
  }
  if (m_profile)
  {
    Profile::Name profile = m_profile->profile;
    if (profile == Profile::MAIN_10 || profile == Profile::MAIN_10_444 ||
        profile == Profile::MULTILAYER_MAIN_10 || profile == Profile::MULTILAYER_MAIN_10_444)
    {
      m_hbrFactor = 1;
    }
    else
    {
      m_hbrFactor = 1 + ptl.getTierFlag();
    }
  }
}

double ProfileTierLevelFeatures::getMinCr() const
{
  return (m_tierLevel!=0 && m_profile!=0) ? (m_profile->minCrScaleFactorx100 * m_tierLevel->minCrBase[m_tier?1:0] / m_hbrFactor)/100.0 : 0.0 ;
}

uint64_t ProfileTierLevelFeatures::getCpbSizeInBits() const
{
  return (m_tierLevel!=0 && m_profile!=0) ? uint64_t(m_profile->cpbVclFactor) * m_tierLevel->maxCpb[m_tier?1:0] * m_hbrFactor : uint64_t(0);
}

uint32_t ProfileTierLevelFeatures::getMaxDpbSize( uint32_t picSizeMaxInSamplesY ) const
{
  const uint32_t maxDpbPicBuf = 8;
  uint32_t       maxDpbSize;

  if (m_tierLevel->level == Level::LEVEL15_5)
  {
    // maxDpbSize is unconstrained in this case
    maxDpbSize = std::numeric_limits<uint32_t>::max();
  }
  else if (2 * picSizeMaxInSamplesY <= m_tierLevel->maxLumaPs)
  {
    maxDpbSize = 2 * maxDpbPicBuf;
  }
  else if (3 * picSizeMaxInSamplesY <= 2 * m_tierLevel->maxLumaPs)
  {
    maxDpbSize = 3 * maxDpbPicBuf / 2;
  }
  else
  {
    maxDpbSize = maxDpbPicBuf;
  }

  return maxDpbSize;
}
