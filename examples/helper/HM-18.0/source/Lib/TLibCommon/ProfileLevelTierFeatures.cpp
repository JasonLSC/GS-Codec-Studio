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

/** \file     ProfileLevelTierFeatures.cpp
    \brief    Common profile tier level functions
*/

#include "TLibCommon/TComSlice.h"
//#include "TLibCommon/TComPic.h"
//#include "TLibCommon/TComPicSym.h"
#include <math.h>
#include "ProfileLevelTierFeatures.h"

UInt
LevelTierFeatures::getMaxPicWidthInLumaSamples()  const
{
  return UInt(sqrt(maxLumaPs*8.0));
}

UInt
LevelTierFeatures::getMaxPicHeightInLumaSamples() const
{
  return UInt(sqrt(maxLumaPs*8.0));
}

static const UInt64 MAX_CNFUINT64 = std::numeric_limits<UInt64>::max();

static const LevelTierFeatures mainLevelTierInfo[] =
{
    { Level::LEVEL1  ,    36864, {      350,        0 },       16,        1,        1,     552960ULL, {     128,        0 }, { 2, 2} },
    { Level::LEVEL2  ,   122880, {     1500,        0 },       16,        1,        1,    3686400ULL, {    1500,        0 }, { 2, 2} },
    { Level::LEVEL2_1,   245760, {     3000,        0 },       20,        1,        1,    7372800ULL, {    3000,        0 }, { 2, 2} },
    { Level::LEVEL3  ,   552960, {     6000,        0 },       30,        2,        2,   16588800ULL, {    6000,        0 }, { 2, 2} },
    { Level::LEVEL3_1,   983040, {    10000,        0 },       40,        3,        3,   33177600ULL, {   10000,        0 }, { 2, 2} },
    { Level::LEVEL4  ,  2228224, {    12000,    30000 },       75,        5,        5,   66846720ULL, {   12000,    30000 }, { 4, 4} },
    { Level::LEVEL4_1,  2228224, {    20000,    50000 },       75,        5,        5,  133693440ULL, {   20000,    50000 }, { 4, 4} },
    { Level::LEVEL5  ,  8912896, {    25000,   100000 },      200,       11,       10,  267386880ULL, {   25000,   100000 }, { 6, 4} },
    { Level::LEVEL5_1,  8912896, {    40000,   160000 },      200,       11,       10,  534773760ULL, {   40000,   160000 }, { 8, 4} },
    { Level::LEVEL5_2,  8912896, {    60000,   240000 },      200,       11,       10, 1069547520ULL, {   60000,   240000 }, { 8, 4} },
    { Level::LEVEL6  , 35651584, {    60000,   240000 },      600,       22,       20, 1069547520ULL, {   60000,   240000 }, { 8, 4} },
    { Level::LEVEL6_1, 35651584, {   120000,   480000 },      600,       22,       20, 2139095040ULL, {  120000,   480000 }, { 8, 4} },
    { Level::LEVEL6_2, 35651584, {   240000,   800000 },      600,       22,       20, 4278190080ULL, {  240000,   800000 }, { 6, 4} },
#if JVET_X0079_MODIFIED_BITRATES
    { Level::LEVEL6_3, 80216064, {   240000,  1600000 },      600,       22,       20, 4812963840ULL, {  320000,  1600000 }, { 6, 4} },
#endif
    { Level::LEVEL8_5, MAX_UINT, { MAX_UINT, MAX_UINT }, MAX_UINT, MAX_UINT, MAX_UINT, MAX_CNFUINT64, {MAX_UINT, MAX_UINT }, { 0, 0} },
    { Level::NONE                   }
};

static const ProfileFeatures validProfiles[] =
{   //  profile,                   pNameString,             maxBitDepth, maxChrFmt, intra, 1pic,   lowerBR,                   RExtTools,                 ExtPrec ,                  ChrmQPOf,                  align,                     HBRFactor,   , wve+t,  tiles,, lvl8.5, cpbvcl, cpbnal, fcf*1000, mincr*10
    { Profile::MAIN,               "Main",                            8, CHROMA_420, false, false, ProfileFeatures::ENABLED , ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, HBR_1        , false, 256, 64, false,   1000,   1100,     1500,   10    , mainLevelTierInfo },
    { Profile::MAIN10,             "Main10",                         10, CHROMA_420, false, false, ProfileFeatures::ENABLED , ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, HBR_1        , false, 256, 64, false,   1000,   1100,     1875,   10    , mainLevelTierInfo },
    { Profile::MAIN10,             "Main10 Still Picture",           10, CHROMA_420, false,  true, ProfileFeatures::ENABLED , ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, HBR_1        , false, 256, 64, true ,   1000,   1100,     1875,   10    , mainLevelTierInfo },
    { Profile::MAINSTILLPICTURE,   "Main Still Picture",              8, CHROMA_420, false, false, ProfileFeatures::ENABLED , ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, HBR_1        , false, 256, 64, true ,   1000,   1100,     1500,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Monochrome",                      8, CHROMA_400, false, false, ProfileFeatures::ENABLED , ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,    667,    733,     1000,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Monochrome 12",                  12, CHROMA_400, false, false, ProfileFeatures::ENABLED , ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1000,   1100,     1500,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Monochrome 16",                  16, CHROMA_400, false, false, ProfileFeatures::ENABLED , ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1333,   1467,     2000,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 12",                        12, CHROMA_420, false, false, ProfileFeatures::ENABLED , ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1500,   1650,     2250,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:2:2 10",                  10, CHROMA_422, false, false, ProfileFeatures::ENABLED , ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1667,   1833,     2500,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:2:2 12",                  12, CHROMA_422, false, false, ProfileFeatures::ENABLED , ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2000,   2200,     3000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4",                      8, CHROMA_444, false, false, ProfileFeatures::ENABLED , ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2000,   2200,     3000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 10",                  10, CHROMA_444, false, false, ProfileFeatures::ENABLED , ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2500,   2750,     3750,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 12",                  12, CHROMA_444, false, false, ProfileFeatures::ENABLED , ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   3000,   3300,     4500,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main Intra",                      8, CHROMA_420, true , false, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1000,   1100,     1500,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 10 Intra",                  10, CHROMA_420, true , false, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1000,   1100,     1875,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 12 Intra",                  12, CHROMA_420, true , false, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1500,   1650,     2250,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:2:2 10 Intra",            10, CHROMA_422, true , false, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1667,   1833,     2500,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:2:2 12 Intra",            12, CHROMA_422, true , false, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::DISABLED, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2000,   2200,     3000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 Intra",                8, CHROMA_444, true , false, ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2000,   2200,     3000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 10 Intra",            10, CHROMA_444, true , false, ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2500,   2750,     3750,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 12 Intra",            12, CHROMA_444, true , false, ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   3000,   3300,     4500,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 16 Intra",            16, CHROMA_444, true , false, ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, false,   4000,   4400,     6000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 Still Picture",        8, CHROMA_444, true , true , ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, true ,   2000,   2200,     3000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 16 Still Picture",    16, CHROMA_444, true , true , ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::DISABLED, HBR_1_OR_2   , false, 256, 64, true ,   4000,   4400,     6000,    5    , mainLevelTierInfo },
    { Profile::HIGHTHROUGHPUTREXT, "High Throughput 4:4:4 16 Intra", 16, CHROMA_444, true , false, ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::OPTIONAL, ProfileFeatures::ENABLED , HBR_12_OR_24 , true , 256, 64, false,   4000,   4400,     6000,    5    , mainLevelTierInfo },
    { Profile::NONE, 0 }
};




Void
ProfileLevelTierFeatures::activate(const TComSPS &sps)
{
  const ProfileTierLevel ptl=*(sps.getPTL()->getGeneralPTL());
  activate(ptl.getProfileIdc(),
           ptl.getBitDepthConstraint(),
           ptl.getChromaFormatConstraint(),
           ptl.getIntraConstraintFlag(),
           ptl.getOnePictureOnlyConstraintFlag(),
           ptl.getLevelIdc(),
           ptl.getTierFlag(),
           sps.getMaxCUWidth(),
           sps.getBitDepth(CHANNEL_TYPE_LUMA),
           sps.getBitDepth(CHANNEL_TYPE_CHROMA),
           sps.getChromaFormatIdc());
}

Void
ProfileLevelTierFeatures::activate(const Profile::Name profileIdc,
                                   const UInt          bitDepthConstraint,
                                   const ChromaFormat  chromaFormatConstraint,
                                   const Bool          intraConstraintFlag,
                                   const Bool          onePictureOnlyConstraintFlag,
                                   const Level::Name   level,
                                   const Level::Tier   tier,
                                   const UInt          ctbSizeY,
                                   const UInt          bitDepthY,
                                   const UInt          bitDepthC,
                                   const ChromaFormat  chFormat)
{
  m_tier = tier;

  for(Int i=0; validProfiles[i].profile != Profile::NONE; i++)
  {
    if (profileIdc == validProfiles[i].profile &&
        bitDepthConstraint == validProfiles[i].maxBitDepth &&
        chromaFormatConstraint == validProfiles[i].maxChromaFormat &&
        intraConstraintFlag == validProfiles[i].generalIntraConstraintFlag &&
        onePictureOnlyConstraintFlag == validProfiles[i].generalOnePictureOnlyConstraintFlag )
    {
      m_pProfile = &(validProfiles[i]);
      break;
    }
  }

  if (m_pProfile != 0)
  {
    // Now identify the level:
    const LevelTierFeatures *pLTF = m_pProfile->pLevelTiersListInfo;
    const Level::Name spsLevelName = level;
    if (spsLevelName!=Level::LEVEL8_5 || m_pProfile->bCanUseLevel8p5)
    {
      for(Int i=0; pLTF[i].level!=Level::NONE; i++)
      {
        if (pLTF[i].level == spsLevelName)
        {
          m_pLevelTier = &(pLTF[i]);
        }
      }
    }
  }

  {
    const UInt ctbWidthC  = ctbSizeY >> getChannelTypeScaleX(CHANNEL_TYPE_CHROMA, chFormat);
    const UInt ctbHeightC = ctbSizeY >> getChannelTypeScaleY(CHANNEL_TYPE_CHROMA, chFormat);

    const UInt rawCtuBits = ctbSizeY*ctbSizeY*bitDepthY+2*(ctbWidthC*ctbHeightC)*bitDepthC;
    m_maxRawCtuBits=(rawCtuBits*5)/3;
  }

}

Int ProfileLevelTierFeatures::getMaxDPBNumFrames(const UInt PicSizeInSamplesY) // returns -1 if no limit, otherwise a limit of DPB pictures is indicated.
{
  Int MaxDpbSize=-1;

  if (m_pLevelTier!=0)
  {
    UInt MaxLumaPs=m_pLevelTier->maxLumaPs;
    Int maxDpbPicBuf=6; // SCC profiles may set this to 7.

    if( PicSizeInSamplesY <= ( MaxLumaPs >> 2 ) )
    {
       MaxDpbSize = min( 4 * maxDpbPicBuf, 16 );
    }
    else if( PicSizeInSamplesY <= ( MaxLumaPs >> 1 ) )
    {
       MaxDpbSize = min( 2 * maxDpbPicBuf, 16 );
    }
    else if( PicSizeInSamplesY <= ( ( 3 * MaxLumaPs ) >> 2 ) )
    {
       MaxDpbSize = min( ( 4 * maxDpbPicBuf ) / 3, 16 );
    }
    else
    {
       MaxDpbSize = maxDpbPicBuf;
    }
  }
  return MaxDpbSize;
}

