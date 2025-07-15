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

/** \file     TDecConformance.cpp
    \brief    Decoder conformance functions
*/

#include "TDecConformance.h"
#include "TLibCommon/TComSlice.h"
#include "TLibCommon/TComPic.h"
#include "TLibCommon/TComPicSym.h"
#include "NALread.h"
#include <math.h>

#if !DPB_ENCODER_USAGE_CHECK
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
#endif

UInt
TDecConformanceCheck::getMinLog2CtbSize(const TComPTL &ptl,
                                        UInt layerPlus1)
{
  const ProfileTierLevel *pPTL = (layerPlus1==0) ? ptl.getGeneralPTL() : ptl.getSubLayerPTL(layerPlus1-1);
  return (pPTL->getLevelIdc() < Level::LEVEL5) ? 4 : 5;
}


UInt
TDecConformanceCheck::getMaxLog2CtbSize(const TComPTL &/*ptl*/,
                                        UInt /*layerPlus1*/)
{
  return MAX_CU_DEPTH;
}


TDecConformanceCheck::TDecConformanceCheck()
#if MCTS_ENC_CHECK
  :m_tmctsCheckEnabled(false)
#if DECODER_PARTIAL_CONFORMANCE_CHECK
  , m_numberOfSlicesInPicture(0)
  , m_bytesInPicture(0)
#endif
#else
#if DECODER_PARTIAL_CONFORMANCE_CHECK
  : m_numberOfSlicesInPicture(0)
  , m_bytesInPicture(0)
#endif
#endif
{
}

#if DECODER_PARTIAL_CONFORMANCE_CHECK != 0

#if !DPB_ENCODER_USAGE_CHECK
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
    { Level::LEVEL8_5, MAX_UINT, { MAX_UINT, MAX_UINT }, MAX_UINT, MAX_UINT, MAX_UINT, MAX_CNFUINT64, {MAX_UINT, MAX_UINT }, { 0, 0} },
    { Level::NONE                   }
};

static const ProfileFeatures validProfiles[] =
{   //  profile,                   pNameString,             maxBitDepth, maxChrFmt, intra, 1pic,   lowerBR, RExtTools, ExtPrec , ChrmQPOf, align,    HBRFactor,   , wve+t,  tiles,, lvl8.5, cpbvcl, cpbnal, fcf*1000, mincr*10
    { Profile::MAIN,               "Main",                            8, CHROMA_420, false, false, ENABLED , DISABLED, DISABLED, DISABLED, DISABLED, HBR_1        , false, 256, 64, false,   1000,   1100,     1500,   10    , mainLevelTierInfo },
    { Profile::MAIN10,             "Main10",                         10, CHROMA_420, false, false, ENABLED , DISABLED, DISABLED, DISABLED, DISABLED, HBR_1        , false, 256, 64, false,   1000,   1100,     1875,   10    , mainLevelTierInfo },
    { Profile::MAIN10,             "Main10 Still Picture",           10, CHROMA_420, false,  true, ENABLED , DISABLED, DISABLED, DISABLED, DISABLED, HBR_1        , false, 256, 64, true ,   1000,   1100,     1875,   10    , mainLevelTierInfo },
    { Profile::MAINSTILLPICTURE,   "Main Still Picture",              8, CHROMA_420, false, false, ENABLED , DISABLED, DISABLED, DISABLED, DISABLED, HBR_1        , false, 256, 64, true ,   1000,   1100,     1500,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Monochrome",                      8, CHROMA_400, false, false, ENABLED , DISABLED, DISABLED, DISABLED, DISABLED, HBR_1_OR_2   , false, 256, 64, false,    667,    733,     1000,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Monochrome 12",                  12, CHROMA_400, false, false, ENABLED , DISABLED, DISABLED, DISABLED, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1000,   1100,     1500,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Monochrome 16",                  16, CHROMA_400, false, false, ENABLED , OPTIONAL, OPTIONAL, DISABLED, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1333,   1467,     2000,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 12",                        12, CHROMA_420, false, false, ENABLED , DISABLED, DISABLED, DISABLED, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1500,   1650,     2250,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:2:2 10",                  10, CHROMA_422, false, false, ENABLED , DISABLED, DISABLED, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1667,   1833,     2500,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:2:2 12",                  12, CHROMA_422, false, false, ENABLED , DISABLED, DISABLED, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2000,   2200,     3000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4",                      8, CHROMA_444, false, false, ENABLED , OPTIONAL, DISABLED, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2000,   2200,     3000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 10",                  10, CHROMA_444, false, false, ENABLED , OPTIONAL, DISABLED, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2500,   2750,     3750,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 12",                  12, CHROMA_444, false, false, ENABLED , OPTIONAL, DISABLED, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   3000,   3300,     4500,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main Intra",                      8, CHROMA_420, true , false, OPTIONAL, DISABLED, DISABLED, DISABLED, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1000,   1100,     1500,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 10 Intra",                  10, CHROMA_420, true , false, OPTIONAL, DISABLED, DISABLED, DISABLED, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1000,   1100,     1875,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 12 Intra",                  12, CHROMA_420, true , false, OPTIONAL, DISABLED, DISABLED, DISABLED, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1500,   1650,     2250,   10    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:2:2 10 Intra",            10, CHROMA_422, true , false, OPTIONAL, DISABLED, DISABLED, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   1667,   1833,     2500,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:2:2 12 Intra",            12, CHROMA_422, true , false, OPTIONAL, DISABLED, DISABLED, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2000,   2200,     3000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 Intra",                8, CHROMA_444, true , false, OPTIONAL, OPTIONAL, DISABLED, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2000,   2200,     3000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 10 Intra",            10, CHROMA_444, true , false, OPTIONAL, OPTIONAL, DISABLED, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   2500,   2750,     3750,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 12 Intra",            12, CHROMA_444, true , false, OPTIONAL, OPTIONAL, DISABLED, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   3000,   3300,     4500,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 16 Intra",            16, CHROMA_444, true , false, OPTIONAL, OPTIONAL, OPTIONAL, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, false,   4000,   4400,     6000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 Still Picture",        8, CHROMA_444, true , true , OPTIONAL, OPTIONAL, DISABLED, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, true ,   2000,   2200,     3000,    5    , mainLevelTierInfo },
    { Profile::MAINREXT,           "Main 4:4:4 16 Still Picture",    16, CHROMA_444, true , true , OPTIONAL, OPTIONAL, OPTIONAL, OPTIONAL, DISABLED, HBR_1_OR_2   , false, 256, 64, true ,   4000,   4400,     6000,    5    , mainLevelTierInfo },
    { Profile::HIGHTHROUGHPUTREXT, "High Throughput 4:4:4 16 Intra", 16, CHROMA_444, true , false, OPTIONAL, OPTIONAL, OPTIONAL, OPTIONAL, ENABLED , HBR_12_OR_24 , true , 256, 64, false,   4000,   4400,     6000,    5    , mainLevelTierInfo },
    { Profile::NONE, 0 }
};
#endif




static Void
checkSPS(const TComSPS &sps,
         const ProfileLevelTierFeatures &features)
{
  // sps_max_sub_layers_minus1 shall be less than or equal to vsp_max_sub_layers_minus1

  // TODO - check conformance of sps_max_dec_pic_buffering_minus1, sps_max_num_reorder_pics, sps_max_latency_increase_plus1
  //        needs VPS, which can only be done on activation.

  const UInt minCbSizeY = 1<<(sps.getLog2MinCodingBlockSize());

  if (sps.getPicWidthInLumaSamples() % minCbSizeY != 0 )
  {
    TDecConformanceCheck::getStream() << "picture width (" << sps.getPicWidthInLumaSamples() << ") must be a multiple of minCbSizeY (=" << minCbSizeY << ")\n";
    TDecConformanceCheck::finishWarningReport();
  }

  if (sps.getPicHeightInLumaSamples() % minCbSizeY != 0 )
  {
    TDecConformanceCheck::getStream() << "picture height (" << sps.getPicHeightInLumaSamples() << ") must be a multiple of minCbSizeY (=" << minCbSizeY << ")\n";
    TDecConformanceCheck::finishWarningReport();
  }


  if (sps.getPicWidthInLumaSamples() > features.getLevelTierFeatures()->getMaxPicWidthInLumaSamples())
  {
    TDecConformanceCheck::getStream() << "picture width (" << sps.getPicWidthInLumaSamples() << ") exceeds the maximum allowed by the level (" << features.getLevelTierFeatures()->getMaxPicWidthInLumaSamples() << ")\n";
    TDecConformanceCheck::finishWarningReport();
  }

  if (sps.getPicHeightInLumaSamples() > features.getLevelTierFeatures()->getMaxPicHeightInLumaSamples())
  {
    TDecConformanceCheck::getStream() << "picture height (" << sps.getPicHeightInLumaSamples() << ") exceeds the maximum allowed by the level (" << features.getLevelTierFeatures()->getMaxPicHeightInLumaSamples() << ")\n";
    TDecConformanceCheck::finishWarningReport();
  }

  if (sps.getPicWidthInLumaSamples() * sps.getPicHeightInLumaSamples() > features.getLevelTierFeatures()->maxLumaPs)
  {
    TDecConformanceCheck::getStream() << "picture samples (" << sps.getPicWidthInLumaSamples() << " * " << sps.getPicHeightInLumaSamples() << ") exceeds the maximum allowed by the level (" << features.getLevelTierFeatures()->maxLumaPs << ")\n";
    TDecConformanceCheck::finishWarningReport();
  }
}


static Void
checkTiles(const TComSPS &sps,
           const TComPPS &pps,
           const TComPic &pic,
           const ProfileLevelTierFeatures &features)
{
  if (pps.getTilesEnabledFlag())
  {
    // Tile size check
    const Int minWidthInCtus  = features.getProfileFeatures()->minTileColumnWidthInLumaSamples/ sps.getMaxCUWidth();
    const Int minHeightInCtus = features.getProfileFeatures()->minTileRowHeightInLumaSamples  / sps.getMaxCUHeight();

    const Int numCols = pps.getNumTileColumnsMinus1() + 1;
    const Int numRows = pps.getNumTileRowsMinus1() + 1;
    if (numCols > features.getLevelTierFeatures()->maxTileCols)
    {
      TDecConformanceCheck::getStream() << "number of tile columns (" << numCols << ") exceeds the maximum allowed by the level (" << features.getLevelTierFeatures()->maxTileCols << ")\n";
      TDecConformanceCheck::finishWarningReport();
    }
    if (numRows > features.getLevelTierFeatures()->maxTileRows)
    {
      TDecConformanceCheck::getStream() << "number of tile rows (" << numRows << ") exceeds the maximum allowed by the level (" << features.getLevelTierFeatures()->maxTileRows << ")\n";
      TDecConformanceCheck::finishWarningReport();
    }

    for(Int row=0; row < numRows; row++)
    {
      for(Int col=0; col < numCols; col++)
      {
        const Int tileIdx = row * numCols + col;
        const TComTile &tile = *(pic.getPicSym()->getTComTile(tileIdx));
        if (tile.getTileWidthInCtus() < minWidthInCtus)
        {
          TDecConformanceCheck::getStream() << "width of tile (" << col << ", " << row << ") is too narrow for the profile - read " << tile.getTileWidthInCtus()*sps.getMaxCUWidth() << "must be " << minWidthInCtus*sps.getMaxCUWidth() << "\n";
          TDecConformanceCheck::finishWarningReport();
        }
        if (tile.getTileHeightInCtus() < minHeightInCtus)
        {
          TDecConformanceCheck::getStream() << "height of tile (" << col << ", " << row << ") is too thin for the profile - read " << tile.getTileHeightInCtus()*sps.getMaxCUHeight() << " must be " << minHeightInCtus*sps.getMaxCUHeight() << "\n";
          TDecConformanceCheck::finishWarningReport();
        }
      }
    }

    if (pps.getEntropyCodingSyncEnabledFlag() && !features.getProfileFeatures()->bWavefrontsAndTilesCanBeUsedSimultaneously)
    {
      TDecConformanceCheck::getStream() << "profile does not permit entropy coding sync and tiles to be simultaneously enabled\n";
      TDecConformanceCheck::finishWarningReport();
    }
  }
}


static Void
checkPPS(const TComSPS &sps,
         const TComPPS &pps,
         const TComPic &pic,
         const ProfileLevelTierFeatures &features)
{
  TDecConformanceCheck::checkRange<Int>(pps.getPicInitQPMinus26(), "init_qp_minus26",     -(sps.getBitDepth(CHANNEL_TYPE_LUMA)-8)*6-26, 25);
  TDecConformanceCheck::checkRange<UInt>(pps.getMaxCuDQPDepth(), "diff_cu_qp_delta_depth", 0, sps.getLog2DiffMaxMinCodingBlockSize());
  TDecConformanceCheck::checkRange<UInt>(pps.getLog2ParallelMergeLevelMinus2(), "log2_parallel_merge_level_minus2", 0, sps.getLog2MinCodingBlockSize() + sps.getLog2DiffMaxMinCodingBlockSize() - 2);

  TDecConformanceCheck::checkRange<UInt>(pps.getPpsRangeExtension().getLog2SaoOffsetScale(CHANNEL_TYPE_LUMA), "log2_sao_offset_scale_luma", 0, std::max(sps.getBitDepth(CHANNEL_TYPE_LUMA), 10)-10);
  TDecConformanceCheck::checkRange<UInt>(pps.getPpsRangeExtension().getLog2SaoOffsetScale(CHANNEL_TYPE_CHROMA), "log2_sao_offset_scale_chroma", 0, std::max(sps.getBitDepth(CHANNEL_TYPE_CHROMA), 10)-10);

  checkTiles(sps, pps, pic, features);
}

#if !DPB_ENCODER_USAGE_CHECK
Void
ProfileLevelTierFeatures::activate(const TComSPS &sps)
{
  const ProfileTierLevel &ptl = *(sps.getPTL()->getGeneralPTL());

  m_tier = ptl.getTierFlag();

  for(Int i=0; validProfiles[i].profile != Profile::NONE; i++)
  {
    if (ptl.getProfileIdc() == validProfiles[i].profile &&
        ptl.getBitDepthConstraint() == validProfiles[i].maxBitDepth &&
        ptl.getChromaFormatConstraint() == validProfiles[i].maxChromaFormat &&
        ptl.getIntraConstraintFlag() == validProfiles[i].generalIntraConstraintFlag &&
        ptl.getOnePictureOnlyConstraintFlag() == validProfiles[i].generalOnePictureOnlyConstraintFlag )
    {
      m_pProfile = &(validProfiles[i]);
      break;
    }
  }

  if (m_pProfile != 0)
  {
    // Now identify the level:
    const LevelTierFeatures *pLTF = m_pProfile->pLevelTiersListInfo;
    const Level::Name spsLevelName = sps.getPTL()->getGeneralPTL()->getLevelIdc();
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
    const UInt ctbSizeY   = sps.getMaxCUWidth();
    const UInt bitDepthY  = sps.getBitDepth(CHANNEL_TYPE_LUMA);
    const UInt ctbWidthC  = ctbSizeY >> getChannelTypeScaleX(CHANNEL_TYPE_CHROMA, sps.getChromaFormatIdc());
    const UInt ctbHeightC = ctbSizeY >> getChannelTypeScaleY(CHANNEL_TYPE_CHROMA, sps.getChromaFormatIdc());
    const UInt bitDepthC  = sps.getBitDepth(CHANNEL_TYPE_LUMA);

    const UInt rawCtuBits = ctbSizeY*ctbSizeY*bitDepthY+2*(ctbWidthC*ctbHeightC)*bitDepthC;
    m_maxRawCtuBits=(rawCtuBits*5)/3;
  }

}
#endif


static Void
checkToolAvailability(const TComSPS &sps,
                      const TComPPS &pps,
                      const ProfileLevelTierFeatures &features)
{
#if DPB_ENCODER_USAGE_CHECK
  const ProfileFeatures::TRISTATE rextToolsEnabled = features.getProfileFeatures()->generalRExtToolsEnabled;
  if ( rextToolsEnabled != ProfileFeatures::OPTIONAL)
  {
    const Bool bWantedFlagState = rextToolsEnabled == ProfileFeatures::ENABLED;
#else
  const TRISTATE rextToolsEnabled = features.getProfileFeatures()->generalRExtToolsEnabled;
  if ( rextToolsEnabled != OPTIONAL)
  {
    const Bool bWantedFlagState = rextToolsEnabled == ENABLED;
#endif
    std::string flags;
    if (sps.getSpsRangeExtension().getTransformSkipRotationEnabledFlag()      != bWantedFlagState) flags+=", transform_skip_rotation_enabled_flag";
    if (sps.getSpsRangeExtension().getTransformSkipContextEnabledFlag()       != bWantedFlagState) flags+=", transform_skip_context_enabled_flag";
    if (sps.getSpsRangeExtension().getRdpcmEnabledFlag(RDPCM_SIGNAL_IMPLICIT) != bWantedFlagState) flags+=", implicit_rdpcm_enabled_flag";
    if (sps.getSpsRangeExtension().getRdpcmEnabledFlag(RDPCM_SIGNAL_EXPLICIT) != bWantedFlagState) flags+=", explicit_rdpcm_enabled_flag";
    if (sps.getSpsRangeExtension().getIntraSmoothingDisabledFlag()            != bWantedFlagState) flags+=", intra_smoothing_disabled_flag";
    if (sps.getSpsRangeExtension().getPersistentRiceAdaptationEnabledFlag()   != bWantedFlagState) flags+=", persistent_rice_adaptation_enabled_flag";
#if DPB_ENCODER_USAGE_CHECK
    if (pps.getPpsRangeExtension().getLog2MaxTransformSkipBlockSize()         != 2 && rextToolsEnabled==ProfileFeatures::DISABLED ) flags+=", log2_max_transform_skip_block_size_minus2";
#else
    if (pps.getPpsRangeExtension().getLog2MaxTransformSkipBlockSize()         != 2 && rextToolsEnabled==DISABLED ) flags+=", log2_max_transform_skip_block_size_minus2";
#endif

    if (!flags.empty())
    {
      TDecConformanceCheck::getStream() << "the following flags must all be " << (bWantedFlagState ? "1" : "0") << " in the profile '" << features.getProfileFeatures()->pNameString << "' conformance point: " + flags.substr(2) + "\n";
      TDecConformanceCheck::finishWarningReport();
    }
  }
  else
  {
    TDecConformanceCheck::checkRange<UInt>(pps.getPpsRangeExtension().getLog2MaxTransformSkipBlockSize()-2, "log2_max_transform_skip_block_size_minus2", 0, sps.getQuadtreeTULog2MaxSize()-2);
  }

#if DPB_ENCODER_USAGE_CHECK
  if (features.getProfileFeatures()->extendedPrecisionProcessingFlag != ProfileFeatures::OPTIONAL)
  {
    const Bool bWantedFlagState = features.getProfileFeatures()->extendedPrecisionProcessingFlag == ProfileFeatures::ENABLED;
#else
  if (features.getProfileFeatures()->extendedPrecisionProcessingFlag != OPTIONAL)
  {
    const Bool bWantedFlagState = features.getProfileFeatures()->extendedPrecisionProcessingFlag == ENABLED;
#endif
    if (sps.getSpsRangeExtension().getExtendedPrecisionProcessingFlag() != bWantedFlagState)
    {
      TDecConformanceCheck::getStream() << "extended_precision_processing_flag must be " << (bWantedFlagState ? "1" : "0") << " in the profile '" << features.getProfileFeatures()->pNameString << "' conformance point\n";
      TDecConformanceCheck::finishWarningReport();
    }
  }

#if DPB_ENCODER_USAGE_CHECK
  if (features.getProfileFeatures()->chromaQpOffsetListEnabledFlag != ProfileFeatures::OPTIONAL)
  {
    const Bool bWantedFlagState = features.getProfileFeatures()->chromaQpOffsetListEnabledFlag == ProfileFeatures::ENABLED;
#else
  if (features.getProfileFeatures()->chromaQpOffsetListEnabledFlag != OPTIONAL)
  {
    const Bool bWantedFlagState = features.getProfileFeatures()->chromaQpOffsetListEnabledFlag == ENABLED;
#endif
    if (pps.getPpsRangeExtension().getChromaQpOffsetListEnabledFlag() != bWantedFlagState)
    {
      TDecConformanceCheck::getStream() << "chroma_qp_offset_list_enabled_flag must be " << (bWantedFlagState ? "1" : "0") << " in the profile '" << features.getProfileFeatures()->pNameString << "' conformance point\n";
      TDecConformanceCheck::finishWarningReport();
    }
  }
  else if (pps.getPpsRangeExtension().getChromaQpOffsetListEnabledFlag())
  {
    TDecConformanceCheck::checkRange<UInt>(pps.getPpsRangeExtension().getDiffCuChromaQpOffsetDepth(), "diff_cu_chroma_qp_offset_depth", 0, sps.getLog2DiffMaxMinCodingBlockSize());
  }

#if DPB_ENCODER_USAGE_CHECK
  if (features.getProfileFeatures()->cabacBypassAlignmentEnabledFlag != ProfileFeatures::OPTIONAL)
  {
    const Bool bWantedFlagState = features.getProfileFeatures()->cabacBypassAlignmentEnabledFlag == ProfileFeatures::ENABLED;
#else
  if (features.getProfileFeatures()->cabacBypassAlignmentEnabledFlag != OPTIONAL)
  {
    const Bool bWantedFlagState = features.getProfileFeatures()->cabacBypassAlignmentEnabledFlag == ENABLED;
#endif
    if (sps.getSpsRangeExtension().getCabacBypassAlignmentEnabledFlag() != bWantedFlagState)
    {
      TDecConformanceCheck::getStream() << "cabac_bypass_alignment_enabled_flag must be " << (bWantedFlagState ? "1" : "0") << " in the profile '" << features.getProfileFeatures()->pNameString << "' conformance point\n";
      TDecConformanceCheck::finishWarningReport();
    }
  }
}


Void
TDecConformanceCheck::checkSliceActivation(const TComSlice &slice,
                                           const InputNALUnit &nalu,
                                           const TComPic &pic,
                                           const Bool bFirstSliceInStream,
                                           const Bool bFirstSliceInSequence,
                                           const Bool bFirstSliceInPicture)
{
  const TComSPS &sps=*(slice.getSPS());
  const TComPPS &pps=*(slice.getPPS());

  if (!doChecking()) return;

  m_activatedFeatures.activate(sps);

  if (m_activatedFeatures.getProfileFeatures() == 0 || m_activatedFeatures.getLevelTierFeatures() == 0)
  {
    if (m_activatedFeatures.getProfileFeatures() == 0)
    {
      getStream() << "Unknown profile/constraint flag combination discovered\n";
    }
    else
    {
      getStream() << "Unknown level IDC discovered\n";
    }
    finishWarningReport();
  }
  else
  {
    // Most of the parameters have been individually checked prior to this as they were decoded.
    checkSPS(sps, m_activatedFeatures);           // checks SPS limits
    checkPPS(sps, pps, pic, m_activatedFeatures); // checks PPS limits
    checkToolAvailability(sps, pps, m_activatedFeatures);

    if (m_activatedFeatures.getProfileFeatures()->onlyIRAPPictures() && (nalu.m_nalUnitType < NAL_UNIT_CODED_SLICE_BLA_W_LP || nalu.m_nalUnitType> NAL_UNIT_RESERVED_IRAP_VCL23))
    {
      getStream() << "Bad NALU for an intra constrained profile\n";
      finishWarningReport();
    }

    if (bFirstSliceInStream || bFirstSliceInSequence || bFirstSliceInPicture)
    {
      m_numberOfSlicesInPicture=1;
      m_bytesInPicture=0;
    }
    else
    {
      m_numberOfSlicesInPicture++;
      if (m_numberOfSlicesInPicture > m_activatedFeatures.getLevelTierFeatures()->maxSliceSegmentsPerPicture)
      {
        getStream() << "Too many slice segments in the picture\n";
        finishWarningReport();
      }
    }

    // Currently no HRD analysis is made. For now, just ensure access unit fits within the CPB.
    m_bytesInPicture+=nalu.getBitstream().getFifo().size();
    if (m_bytesInPicture*8 > m_activatedFeatures.getCpbSizeInBits())
    {
      getStream() << "Entire access unit must fit within the CPB even if split into multiple decoding units (section C.2.2 Timing of decoding unit arrival)\n";
      finishWarningReport();
    }
  }
}


Void
TDecConformanceCheck::checkCtuDecoding(const UInt numUsedBits)
{
  if (numUsedBits > m_activatedFeatures.getMaxRawCtuBits())
  {
    getStream() << "CTU exceeds maximum RAW CTU bits limits.\n";
    finishWarningReport();
  }
}

#endif
