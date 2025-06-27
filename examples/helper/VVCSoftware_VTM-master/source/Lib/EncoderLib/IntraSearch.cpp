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

/** \file     EncSearch.cpp
 *  \brief    encoder intra search class
 */

#include "IntraSearch.h"

#include "EncModeCtrl.h"

#include "CommonLib/CommonDef.h"
#include "CommonLib/Rom.h"
#include "CommonLib/Picture.h"
#include "CommonLib/UnitTools.h"

#include "CommonLib/dtrace_next.h"
#include "CommonLib/dtrace_buffer.h"

#include <math.h>
#include <limits>
 //! \ingroup EncoderLib
 //! \{
#define PLTCtx(c) SubCtx( Ctx::Palette, c )
IntraSearch::IntraSearch()
  : m_pSplitCS(nullptr)
  , m_pFullCS(nullptr)
  , m_pBestCS(nullptr)
  , m_pcEncCfg(nullptr)
  , m_pcTrQuant(nullptr)
  , m_pcRdCost(nullptr)
  , m_pcReshape(nullptr)
  , m_CABACEstimator(nullptr)
  , m_ctxPool(nullptr)
  , m_isInitialized(false)
{
  for( uint32_t ch = 0; ch < MAX_NUM_TBLOCKS; ch++ )
  {
    m_pSharedPredTransformSkip[ch] = nullptr;
  }
  m_minErrorIndexMap = nullptr;
  for (unsigned i = 0; i < (MAXPLTSIZE + 1); i++)
  {
    m_indexError[i] = nullptr;
  }
  for (unsigned i = 0; i < NUM_TRELLIS_STATE; i++)
  {
    m_statePtRDOQ[i] = nullptr;
  }
}

void IntraSearch::destroy()
{
  CHECK( !m_isInitialized, "Not initialized" );

  if( m_pcEncCfg )
  {
    const uint32_t numLayersToAllocateSplit = 1;
    const uint32_t numLayersToAllocateFull  = 1;
    const int      numSaveLayersToAllocate  = 2;

    for (uint32_t layer = 0; layer < numSaveLayersToAllocate; layer++)
    {
      m_pSaveCS[layer]->destroy();
      delete m_pSaveCS[layer];
    }

    const uint32_t numWidths  = gp_sizeIdxInfo->numWidths();
    const uint32_t numHeights = gp_sizeIdxInfo->numHeights();

    for( uint32_t width = 0; width < numWidths; width++ )
    {
      for( uint32_t height = 0; height < numHeights; height++ )
      {
        if( gp_sizeIdxInfo->isCuSize( gp_sizeIdxInfo->sizeFrom( width ) ) && gp_sizeIdxInfo->isCuSize( gp_sizeIdxInfo->sizeFrom( height ) ) 
          && gp_sizeIdxInfo->sizeFrom(width) <= m_pcEncCfg->getMaxCUWidth() && gp_sizeIdxInfo->sizeFrom(height) <= m_pcEncCfg->getMaxCUHeight())
        {
          for (uint32_t layer = 0; layer < numLayersToAllocateSplit; layer++)
          {
            m_pSplitCS[width][height][layer]->destroy();

            delete m_pSplitCS[width][height][layer];
          }

          for (uint32_t layer = 0; layer < numLayersToAllocateFull; layer++)
          {
            m_pFullCS[width][height][layer]->destroy();

            delete m_pFullCS[width][height][layer];
          }

          delete[] m_pSplitCS[width][height];
          delete[] m_pFullCS [width][height];

          m_pBestCS[width][height]->destroy();
          m_pTempCS[width][height]->destroy();

          delete m_pTempCS[width][height];
          delete m_pBestCS[width][height];
        }
      }

      delete[] m_pSplitCS[width];
      delete[] m_pFullCS [width];

      delete[] m_pTempCS[width];
      delete[] m_pBestCS[width];
    }

    delete[] m_pSplitCS;
    delete[] m_pFullCS;

    delete[] m_pBestCS;
    delete[] m_pTempCS;

    delete[] m_pSaveCS;
  }

  m_pSplitCS = m_pFullCS = nullptr;

  m_pBestCS = m_pTempCS = nullptr;

  m_pSaveCS = nullptr;

  for( uint32_t ch = 0; ch < MAX_NUM_TBLOCKS; ch++ )
  {
    delete[] m_pSharedPredTransformSkip[ch];
    m_pSharedPredTransformSkip[ch] = nullptr;
  }

  m_tmpStorageCtu.destroy();
  m_colorTransResiBuf.destroy();
  m_isInitialized = false;
  if (m_indexError[0] != nullptr)
  {
    for (unsigned i = 0; i < (MAXPLTSIZE + 1); i++)
    {
      delete[] m_indexError[i];
      m_indexError[i] = nullptr;
    }
  }
  if (m_minErrorIndexMap != nullptr)
  {
    delete[] m_minErrorIndexMap;
    m_minErrorIndexMap = nullptr;
  }
  if (m_statePtRDOQ[0] != nullptr)
  {
    for (unsigned i = 0; i < NUM_TRELLIS_STATE; i++)
    {
      delete[] m_statePtRDOQ[i];
      m_statePtRDOQ[i] = nullptr;
    }
  }
}

IntraSearch::~IntraSearch()
{
  if( m_isInitialized )
  {
    destroy();
  }
}

void IntraSearch::init(EncCfg *pcEncCfg, TrQuant *pcTrQuant, RdCost *pcRdCost, CABACWriter *CABACEstimator,
                       CtxPool *ctxPool, const uint32_t maxCUWidth, const uint32_t maxCUHeight,
                       const uint32_t maxTotalCUDepth, EncReshape *pcReshape, const unsigned bitDepthY)
{
  CHECK(m_isInitialized, "Already initialized");

  m_pcEncCfg       = pcEncCfg;
  m_pcTrQuant      = pcTrQuant;
  m_pcRdCost       = pcRdCost;
  m_CABACEstimator = CABACEstimator;
  m_ctxPool        = ctxPool;
  m_pcReshape      = pcReshape;

  const ChromaFormat cform = pcEncCfg->getChromaFormatIdc();

  IntraPrediction::init(cform, pcEncCfg->getBitDepth(ChannelType::LUMA));
  m_tmpStorageCtu.create(UnitArea(cform, Area(0, 0, maxCUWidth, maxCUHeight)));
  m_colorTransResiBuf.create(UnitArea(cform, Area(0, 0, maxCUWidth, maxCUHeight)));

  for( uint32_t ch = 0; ch < MAX_NUM_TBLOCKS; ch++ )
  {
    m_pSharedPredTransformSkip[ch] = new Pel[maxCUWidth * maxCUHeight];
  }

  const uint32_t numWidths  = gp_sizeIdxInfo->numWidths();
  const uint32_t numHeights = gp_sizeIdxInfo->numHeights();

  const uint32_t numLayersToAllocateSplit = 1;
  const uint32_t numLayersToAllocateFull  = 1;

  m_pBestCS = new CodingStructure**[numWidths];
  m_pTempCS = new CodingStructure**[numWidths];

  m_pFullCS  = new CodingStructure***[numWidths];
  m_pSplitCS = new CodingStructure***[numWidths];

  for( uint32_t width = 0; width < numWidths; width++ )
  {
    m_pBestCS[width] = new CodingStructure*[numHeights];
    m_pTempCS[width] = new CodingStructure*[numHeights];

    m_pFullCS [width] = new CodingStructure**[numHeights];
    m_pSplitCS[width] = new CodingStructure**[numHeights];

    for( uint32_t height = 0; height < numHeights; height++ )
    {
      if(  gp_sizeIdxInfo->isCuSize( gp_sizeIdxInfo->sizeFrom( width ) ) && gp_sizeIdxInfo->isCuSize( gp_sizeIdxInfo->sizeFrom( height ) ) 
        && gp_sizeIdxInfo->sizeFrom(width) <= maxCUWidth && gp_sizeIdxInfo->sizeFrom(height) <= maxCUHeight)
      {
        m_pBestCS[width][height] = new CodingStructure(m_unitPool);
        m_pTempCS[width][height] = new CodingStructure(m_unitPool);

        m_pBestCS[width][height]->create(m_pcEncCfg->getChromaFormatIdc(), Area(0, 0, gp_sizeIdxInfo->sizeFrom(width), gp_sizeIdxInfo->sizeFrom(height)), false, (bool)pcEncCfg->getPLTMode());
        m_pTempCS[width][height]->create(m_pcEncCfg->getChromaFormatIdc(), Area(0, 0, gp_sizeIdxInfo->sizeFrom(width), gp_sizeIdxInfo->sizeFrom(height)), false, (bool)pcEncCfg->getPLTMode());

        m_pFullCS[width][height]  = new CodingStructure *[numLayersToAllocateFull];
        m_pSplitCS[width][height] = new CodingStructure *[numLayersToAllocateSplit];

        for (uint32_t layer = 0; layer < numLayersToAllocateFull; layer++)
        {
          m_pFullCS[width][height][layer] = new CodingStructure(m_unitPool);

          m_pFullCS[width][height][layer]->create(m_pcEncCfg->getChromaFormatIdc(), Area(0, 0, gp_sizeIdxInfo->sizeFrom(width), gp_sizeIdxInfo->sizeFrom(height)), false, (bool)pcEncCfg->getPLTMode());
        }

        for (uint32_t layer = 0; layer < numLayersToAllocateSplit; layer++)
        {
          m_pSplitCS[width][height][layer] = new CodingStructure(m_unitPool);
          m_pSplitCS[width][height][layer]->create(m_pcEncCfg->getChromaFormatIdc(), Area(0, 0, gp_sizeIdxInfo->sizeFrom(width), gp_sizeIdxInfo->sizeFrom(height)), false, (bool)pcEncCfg->getPLTMode());
        }
      }
      else
      {
        m_pBestCS[width][height] = nullptr;
        m_pTempCS[width][height] = nullptr;

        m_pFullCS [width][height] = nullptr;
        m_pSplitCS[width][height] = nullptr;
      }
    }
  }

  const int numSaveLayersToAllocate = 2;

  m_pSaveCS = new CodingStructure *[numSaveLayersToAllocate];

  for (uint32_t depth = 0; depth < numSaveLayersToAllocate; depth++)
  {
    m_pSaveCS[depth] = new CodingStructure(m_unitPool);
    m_pSaveCS[depth]->create(UnitArea(cform, Area(0, 0, maxCUWidth, maxCUHeight)), false, (bool)pcEncCfg->getPLTMode());
  }

  m_isInitialized = true;
  if (pcEncCfg->getPLTMode())
  {
    if (m_indexError[0] == nullptr)
    {
      for (unsigned i = 0; i < (MAXPLTSIZE + 1); i++)
      {
        m_indexError[i] = new double[MAX_CU_BLKSIZE_PLT*MAX_CU_BLKSIZE_PLT];
      }
    }
    if (m_minErrorIndexMap == nullptr)
    {
      m_minErrorIndexMap = new uint8_t[MAX_CU_BLKSIZE_PLT*MAX_CU_BLKSIZE_PLT];
    }
    if (m_statePtRDOQ[0] == nullptr)
    {
      for (unsigned i = 0; i < NUM_TRELLIS_STATE; i++)
      {
        m_statePtRDOQ[i] = new uint8_t[MAX_CU_BLKSIZE_PLT*MAX_CU_BLKSIZE_PLT];
      }
    }
  }
}


//////////////////////////////////////////////////////////////////////////
// INTRA PREDICTION
//////////////////////////////////////////////////////////////////////////
static constexpr double COST_UNKNOWN = -65536.0;

double IntraSearch::findInterCUCost( CodingUnit &cu )
{
  if( cu.isConsIntra() && !cu.slice->isIntra() )
  {
    //search corresponding inter CU cost
    for( int i = 0; i < m_numCuInSCIPU; i++ )
    {
      if( cu.lumaPos() == m_cuAreaInSCIPU[i].pos() && cu.lumaSize() == m_cuAreaInSCIPU[i].size() )
      {
        return m_cuCostInSCIPU[i];
      }
    }
  }
  return COST_UNKNOWN;
}

#if GDR_ENABLED
int IntraSearch::getNumTopRecons(PredictionUnit &pu, int lumaDirMode, bool isChroma)
{
  const int w = isChroma ? pu.Cb().width : pu.Y().width;
  const int h = isChroma ? pu.Cb().height : pu.Y().height;

  int numOfTopRecons = w;

  const int refIdx             = pu.multiRefIdx;
  const int predModeIntra      = getModifiedWideAngle(w, h, lumaDirMode);
  const int isModeVer          = predModeIntra >= DIA_IDX;
  const int intraPredAngleMode = (isModeVer) ? predModeIntra - VER_IDX : -(predModeIntra - HOR_IDX);

  const int absAngMode         = abs(intraPredAngleMode);
  const int signAng            = intraPredAngleMode < 0 ? -1 : 1;
  const int absAng             = (lumaDirMode > DC_IDX && lumaDirMode < NUM_LUMA_MODE) ? angTable[absAngMode] : 0;

  const int invAngle           = invAngTable[absAngMode];
  const int intraPredAngle     = signAng * absAng;

  const int sideSize = isModeVer ? h : w;
  const int maxScale = 2;

  const int angularScale = std::min(maxScale, floorLog2(sideSize) - (floorLog2(3 * invAngle - 2) - 8));

  bool applyPDPC;

  // 1.0 derive PDPC
  applyPDPC  = (refIdx == 0) ? true : false;
  if (lumaDirMode > DC_IDX && lumaDirMode < NUM_LUMA_MODE)
  {
    if (intraPredAngleMode < 0)
    {
      applyPDPC &= false;
    }
    else if (intraPredAngleMode > 0)
    {
      applyPDPC &= (angularScale >= 0);
    }
  }

  // 2.0 calculate number of recons
  switch (lumaDirMode)
  {
  case PLANAR_IDX:
    numOfTopRecons = applyPDPC ? (w + 1) : (w + 1);
    break;

  case DC_IDX:
    numOfTopRecons = applyPDPC ? (w) : (w);
    break;

  case HOR_IDX:
    numOfTopRecons = applyPDPC ? (w) : (w);
    break;

  case VER_IDX:
    numOfTopRecons = applyPDPC ? (w) : (w);
    break;

  default:
    // 2..66
    // note: There should be a way to reduce the number of top recons, in case of non PDPC
    applyPDPC |= isChroma;

    if (predModeIntra >= DIA_IDX)
    {
      if (intraPredAngle < 0)
      {
        numOfTopRecons = (applyPDPC) ? (w + w) : (w + 1);
      }
      else
      {
        numOfTopRecons = (applyPDPC) ? (w + w) : (w + w);
      }
    }
    else
    {
      if (intraPredAngle < 0)
      {
        numOfTopRecons = (applyPDPC) ? (w + w) : (w);
      }
      else
      {
        numOfTopRecons = (applyPDPC) ? (w + w) : (w);
      }
    }
    break;
  }

  return numOfTopRecons;
}

bool IntraSearch::isValidIntraPredLuma(PredictionUnit &pu, int lumaDirMode)
{
  bool isValid  = true;

  if (pu.cs->picture->gdrParam.inGdrInterval)
  {
    int x = pu.Y().x;

    // count num of recons on the top
    int virX             = pu.cs->picture->gdrParam.verBoundary;
    int numOfTopRecons   = getNumTopRecons(pu, lumaDirMode, false);

    // check if recon is out of boundary
    if (x < virX && virX < (x + numOfTopRecons))
    {
      isValid = false;
    }
  }

  return isValid;
}

bool IntraSearch::isValidIntraPredChroma(PredictionUnit &pu, int lumaDirMode, int chromaDirMode)
{
  bool isValid = true;
  CodingStructure *cs = pu.cs;

  if (pu.cs->picture->gdrParam.inGdrInterval)
  {
    // note: chroma cordinate
    int cbX = pu.Cb().x;
    //int cbY = pu.Cb().y;
    int cbW = pu.Cb().width;
    int cbH = pu.Cb().height;

    int chromaScaleX = getComponentScaleX(COMPONENT_Cb, cs->area.chromaFormat);
    int chromaScaleY = getComponentScaleY(COMPONENT_Cb, cs->area.chromaFormat);

    int lumaX = cbX << chromaScaleX;
    // int lumaY = cbY << chromaScaleY;
    int lumaW = cbW << chromaScaleX;
    int lumaH = cbH << chromaScaleY;

    int numOfTopRecons = lumaW;
    int virX           = pu.cs->picture->gdrParam.verBoundary;

    // count num of recons on the top
    switch (chromaDirMode)
    {

    case LM_CHROMA_IDX :
      numOfTopRecons = lumaW;
      break;

    case MDLM_L_IDX :
      numOfTopRecons = lumaW;
      break;

    // note: could reduce the actual #of
    case MDLM_T_IDX:
      numOfTopRecons = (lumaW + lumaH);
      break;

    case DM_CHROMA_IDX: numOfTopRecons = getNumTopRecons(pu, lumaDirMode, true) << chromaScaleX; break;

    default: numOfTopRecons = getNumTopRecons(pu, chromaDirMode, true) << chromaScaleX; break;
    }

    // check if recon is out of boundary
    if (lumaX < virX && virX < (lumaX + numOfTopRecons))
    {
      isValid = false;
    }
  }

  return isValid;
}
#endif

bool IntraSearch::estIntraPredLumaQT(CodingUnit &cu, Partitioner &partitioner, const double bestCostSoFar, bool mtsCheckRangeFlag, int mtsFirstCheckId, int mtsLastCheckId, bool moreProbMTSIdxFirst, CodingStructure* bestCS)
{
  CodingStructure &cs  = *cu.cs;
  const SPS       &sps = *cs.sps;

  const uint32_t logWidth  = floorLog2(partitioner.currArea().lwidth());
  const uint32_t logHeight = floorLog2(partitioner.currArea().lheight());

  // Lambda calculation at equivalent Qp of 4 is recommended because at that Qp, the quantization divisor is 1.
  const double sqrtLambdaForFirstPass = m_pcRdCost->getMotionLambda( ) * FRAC_BITS_SCALE;

  //===== loop over partitions =====

  const TempCtx ctxStart(m_ctxPool, m_CABACEstimator->getCtx());
  const TempCtx ctxStartMipFlag(m_ctxPool, SubCtx(Ctx::MipFlag, m_CABACEstimator->getCtx()));
  const TempCtx ctxStartIspMode(m_ctxPool, SubCtx(Ctx::ISPMode, m_CABACEstimator->getCtx()));
  const TempCtx ctxStartPlanarFlag(m_ctxPool, SubCtx(Ctx::IntraLumaPlanarFlag, m_CABACEstimator->getCtx()));
  const TempCtx ctxStartIntraMode(m_ctxPool, SubCtx(Ctx::IntraLumaMpmFlag, m_CABACEstimator->getCtx()));
  const TempCtx ctxStartMrlIdx(m_ctxPool, SubCtx(Ctx::MultiRefLineIdx, m_CABACEstimator->getCtx()));

  CHECK( !cu.firstPU, "CU has no PUs" );
  // variables for saving fast intra modes scan results across multiple LFNST passes
  bool lfnstLoadFlag = sps.getUseLFNST() && cu.lfnstIdx != 0;
  bool lfnstSaveFlag = sps.getUseLFNST() && cu.lfnstIdx == 0;

  lfnstSaveFlag &= sps.getExplicitMtsIntraEnabled() ? cu.mtsFlag == 0 : true;

  const uint32_t lfnstIdx = cu.lfnstIdx;
  double costInterCU = findInterCUCost( cu );

  const int width  = partitioner.currArea().lwidth();
  const int height = partitioner.currArea().lheight();

  // Marking MTS usage for faster MTS
  // 0: MTS is either not applicable for current CU (cuWidth > MTS_INTRA_MAX_CU_SIZE or cuHeight > MTS_INTRA_MAX_CU_SIZE), not active in the config file or the fast decision algorithm is not used in this case
  // 1: MTS fast algorithm can be applied for the current CU, and the DCT2 is being checked
  // 2: MTS is being checked for current CU. Stored results of DCT2 can be utilized for speedup
  uint8_t mtsUsageFlag = 0;
  const int maxSizeEMT = MTS_INTRA_MAX_CU_SIZE;
  if (width <= maxSizeEMT && height <= maxSizeEMT && sps.getExplicitMtsIntraEnabled())
  {
    mtsUsageFlag = ( sps.getUseLFNST() && cu.mtsFlag == 1 ) ? 2 : 1;
  }

  if( width * height < 64 && !m_pcEncCfg->getUseFastLFNST() )
  {
    mtsUsageFlag = 0;
  }

  const bool colorTransformIsEnabled = sps.getUseColorTrans() && !CS::isDualITree(cs);
  const bool isFirstColorSpace       = colorTransformIsEnabled && ((m_pcEncCfg->getRGBFormatFlag() && cu.colorTransform) || (!m_pcEncCfg->getRGBFormatFlag() && !cu.colorTransform));
  const bool isSecondColorSpace      = colorTransformIsEnabled && ((m_pcEncCfg->getRGBFormatFlag() && !cu.colorTransform) || (!m_pcEncCfg->getRGBFormatFlag() && cu.colorTransform));

  double bestCurrentCost = bestCostSoFar;
  bool ispCanBeUsed   = sps.getUseISP() && cu.mtsFlag == 0 && cu.lfnstIdx == 0 && CU::canUseISP(width, height, cu.cs->sps->getMaxTbSize());
  bool saveDataForISP = ispCanBeUsed && (!colorTransformIsEnabled || isFirstColorSpace);
  bool testISP        = ispCanBeUsed && (!colorTransformIsEnabled || !cu.colorTransform);

  if ( saveDataForISP )
  {
    //reset the intra modes lists variables
    m_ispCandList[ISPType::HOR].clear();
    m_ispCandList[ISPType::VER].clear();
  }
  if( testISP )
  {
    //reset the variables used for the tests
    m_regIntraRDListWithCosts.clear();
    int numTotalPartsHor = (int)height >> floorLog2(CU::getISPSplitDim(width, height, TU_1D_HORZ_SPLIT));
    int numTotalPartsVer = (int)width  >> floorLog2(CU::getISPSplitDim(width, height, TU_1D_VERT_SPLIT));
    m_ispTestedModes[0].init( numTotalPartsHor, numTotalPartsVer );
    //the total number of subpartitions is modified to take into account the cases where LFNST cannot be combined with ISP due to size restrictions
    numTotalPartsHor = sps.getUseLFNST() && CU::canUseLfnstWithISP(cu.Y(), ISPType::HOR) ? numTotalPartsHor : 0;
    numTotalPartsVer = sps.getUseLFNST() && CU::canUseLfnstWithISP(cu.Y(), ISPType::VER) ? numTotalPartsVer : 0;
    for (int j = 1; j < NUM_LFNST_NUM_PER_SET; j++)
    {
      m_ispTestedModes[j].init(numTotalPartsHor, numTotalPartsVer);
    }
  }

  const bool testBDPCM = sps.getBDPCMEnabledFlag() && CU::bdpcmAllowed(cu, ComponentID(partitioner.chType)) && cu.mtsFlag == 0 && cu.lfnstIdx == 0;
  static_vector<ModeInfo, FAST_UDI_MAX_RDMODE_NUM> hadModeList;
  static_vector<double, FAST_UDI_MAX_RDMODE_NUM>   candCostList;
  static_vector<double, FAST_UDI_MAX_RDMODE_NUM>   candHadList;

  auto &pu = *cu.firstPU;
#if GDR_ENABLED
  const bool isEncodeGdrClean = cs.sps->getGDREnabledFlag() && cs.pcv->isEncoder && cs.picture->gdrParam.inGdrInterval
                                && cs.isClean(pu.Y().topRight(), ChannelType::LUMA);
#endif
  bool validReturn = false;
  {
    candHadList.clear();
    candCostList.clear();
    hadModeList.clear();

    CHECK(pu.cu != &cu, "PU is not contained in the CU");

    //===== determine set of modes to be tested (using prediction signal only) =====
    int numModesAvailable = NUM_LUMA_MODE; // total number of Intra modes
    const bool fastMip    = sps.getUseMIP() && m_pcEncCfg->getUseFastMIP();
    const bool mipAllowed = sps.getUseMIP() && isLuma(partitioner.chType) && ((cu.lfnstIdx == 0) || allowLfnstWithMip(cu.firstPU->lumaSize()));
    const bool testMip = mipAllowed && !(cu.lwidth() > (8 * cu.lheight()) || cu.lheight() > (8 * cu.lwidth()));
    const bool supportedMipBlkSize = pu.lwidth() <= MIP_MAX_WIDTH && pu.lheight() <= MIP_MAX_HEIGHT;

    static_vector<ModeInfo, FAST_UDI_MAX_RDMODE_NUM> rdModeList;

    int numModesForFullRD = g_intraModeNumFastUseMPM2D[logWidth - MIN_CU_LOG2][logHeight - MIN_CU_LOG2];


    if (isSecondColorSpace)
    {
      rdModeList.clear();
      if (m_numSavedRdModeFirstColorSpace[m_savedRdModeIdx] > 0)
      {
        for (int i = 0; i < m_numSavedRdModeFirstColorSpace[m_savedRdModeIdx]; i++)
        {
          rdModeList.push_back(m_savedRdModeFirstColorSpace[m_savedRdModeIdx][i]);
        }
      }
      else
      {
        return false;
      }
    }
    else
    {
      if (mtsUsageFlag != 2)
      {
        // this should always be true
        CHECK(!pu.Y().valid(), "PU is not valid");
        bool isFirstLineOfCtu     = (((pu.block(COMPONENT_Y).y) & ((pu.cs->sps)->getMaxCUWidth() - 1)) == 0);
        int  numOfPassesExtendRef = ((!sps.getUseMRL() || isFirstLineOfCtu) ? 1 : MRL_NUM_REF_LINES);
        pu.multiRefIdx            = 0;

        if (numModesForFullRD != numModesAvailable)
        {
          CHECK(numModesForFullRD >= numModesAvailable, "Too many modes for full RD search");

          const CompArea &area = pu.Y();

          PelBuf piOrg  = cs.getOrgBuf(area);
          PelBuf piPred = cs.getPredBuf(area);

          DistParam distParamSad;
          DistParam distParamHad;
          if (cu.slice->getLmcsEnabledFlag() && m_pcReshape->getCTUFlag())
          {
            CompArea tmpArea(COMPONENT_Y, area.chromaFormat, Position(0, 0), area.size());
            PelBuf   tmpOrg = m_tmpStorageCtu.getBuf(tmpArea);
            tmpOrg.copyFrom(piOrg);
            tmpOrg.rspSignal(m_pcReshape->getFwdLUT());
            m_pcRdCost->setDistParam(distParamSad, tmpOrg, piPred, sps.getBitDepth(ChannelType::LUMA), COMPONENT_Y,
                                     false);   // Use SAD cost
            m_pcRdCost->setDistParam(distParamHad, tmpOrg, piPred, sps.getBitDepth(ChannelType::LUMA), COMPONENT_Y,
                                     true);   // Use HAD (SATD) cost
          }
          else
          {
            m_pcRdCost->setDistParam(distParamSad, piOrg, piPred, sps.getBitDepth(ChannelType::LUMA), COMPONENT_Y,
                                     false);   // Use SAD cost
            m_pcRdCost->setDistParam(distParamHad, piOrg, piPred, sps.getBitDepth(ChannelType::LUMA), COMPONENT_Y,
                                     true);   // Use HAD (SATD) cost
          }

          distParamSad.applyWeight = false;
          distParamHad.applyWeight = false;

          if (testMip && supportedMipBlkSize)
          {
            numModesForFullRD += fastMip
                                   ? std::max(numModesForFullRD, floorLog2(std::min(pu.lwidth(), pu.lheight())) - 1)
                                   : numModesForFullRD;
          }
          const int numHadCand = (testMip ? 2 : 1) * 3;

          //*** Derive (regular) candidates using Hadamard
          cu.mipFlag = false;

          //===== init pattern for luma prediction =====
          initIntraPatternChType(cu, pu.Y(), true);
          bool satdChecked[NUM_INTRA_MODE];
          std::fill_n(satdChecked, NUM_INTRA_MODE, false);

          if (!lfnstLoadFlag)
          {
            for (int modeIdx = 0; modeIdx < numModesAvailable; modeIdx++)
            {
              uint32_t   mode      = modeIdx;
              Distortion minSadHad = 0;

              // Skip checking extended Angular modes in the first round of SATD
              if (mode > DC_IDX && (mode & 1))
              {
                continue;
              }

              satdChecked[mode] = true;

              pu.intraDir[ChannelType::LUMA] = modeIdx;

              initPredIntraParams(pu, pu.Y(), sps);
              predIntraAng(COMPONENT_Y, piPred, pu);
              // Use the min between SAD and HAD as the cost criterion
              // SAD is scaled by 2 to align with the scaling of HAD
              minSadHad += std::min(distParamSad.distFunc(distParamSad) * 2, distParamHad.distFunc(distParamHad));

              // NB xFracModeBitsIntra will not affect the mode for chroma that may have already been pre-estimated.
              m_CABACEstimator->getCtx() = SubCtx( Ctx::MipFlag, ctxStartMipFlag );
              m_CABACEstimator->getCtx() = SubCtx( Ctx::ISPMode, ctxStartIspMode );
              m_CABACEstimator->getCtx() = SubCtx(Ctx::IntraLumaPlanarFlag, ctxStartPlanarFlag);
              m_CABACEstimator->getCtx() = SubCtx(Ctx::IntraLumaMpmFlag, ctxStartIntraMode);
              m_CABACEstimator->getCtx() = SubCtx( Ctx::MultiRefLineIdx, ctxStartMrlIdx );

              uint64_t fracModeBits = xFracModeBitsIntra(pu, mode, ChannelType::LUMA);

              double cost = (double) minSadHad + (double) fracModeBits * sqrtLambdaForFirstPass;

              DTRACE(g_trace_ctx, D_INTRA_COST, "IntraHAD: %u, %llu, %f (%d)\n", minSadHad, fracModeBits, cost, mode);

#if GDR_ENABLED
              if (!isEncodeGdrClean || isValidIntraPredLuma(pu, mode))
#endif
              {
                const ModeInfo mi(false, false, 0, ISPType::NONE, mode);
                updateCandList(mi, cost, rdModeList, candCostList, numModesForFullRD);
                updateCandList(mi, double(minSadHad), hadModeList, candHadList, numHadCand);
              }
            }
            if (!sps.getUseMIP() && lfnstSaveFlag)
            {
              // save found best modes
              m_savedNumRdModesLFNST = numModesForFullRD;
              m_savedRdModeListLFNST = rdModeList;
              m_savedModeCostLFNST   = candCostList;
              // PBINTRA fast
              m_savedHadModeListLFNST   = hadModeList;
              m_savedHadListLFNST       = candHadList;
              lfnstSaveFlag             = false;
            }
          }   // NSSTFlag
          if (!sps.getUseMIP() && lfnstLoadFlag)
          {
            // restore saved modes
            numModesForFullRD = m_savedNumRdModesLFNST;
            rdModeList        = m_savedRdModeListLFNST;
            candCostList      = m_savedModeCostLFNST;
            // PBINTRA fast
            hadModeList = m_savedHadModeListLFNST;
            candHadList = m_savedHadListLFNST;
          }   // !LFNSTFlag

          if (!(sps.getUseMIP() && lfnstLoadFlag))
          {
            static_vector<ModeInfo, FAST_UDI_MAX_RDMODE_NUM> parentCandList = rdModeList;

            // Second round of SATD for extended Angular modes
#if GDR_ENABLED
            int nn = numModesForFullRD;
            if (isEncodeGdrClean)
            {
              nn = std::min((int)numModesForFullRD, (int)parentCandList.size());
            }

            for (int modeIdx = 0; modeIdx < nn; modeIdx++)
#else
            for (int modeIdx = 0; modeIdx < numModesForFullRD; modeIdx++)
#endif
            {
              unsigned parentMode = parentCandList[modeIdx].modeId;
              if (parentMode > (DC_IDX + 1) && parentMode < (NUM_LUMA_MODE - 1))
              {
                for (int subModeIdx = -1; subModeIdx <= 1; subModeIdx += 2)
                {
                  unsigned mode = parentMode + subModeIdx;

                  if (!satdChecked[mode])
                  {
                    pu.intraDir[ChannelType::LUMA] = mode;

                    initPredIntraParams(pu, pu.Y(), sps);
                    predIntraAng(COMPONENT_Y, piPred, pu);

                    // Use the min between SAD and SATD as the cost criterion
                    // SAD is scaled by 2 to align with the scaling of HAD
                    Distortion minSadHad =
                      std::min(distParamSad.distFunc(distParamSad) * 2, distParamHad.distFunc(distParamHad));

                    // NB xFracModeBitsIntra will not affect the mode for chroma that may have already been
                    // pre-estimated.
                    m_CABACEstimator->getCtx() = SubCtx(Ctx::MipFlag, ctxStartMipFlag);
                    m_CABACEstimator->getCtx() = SubCtx(Ctx::ISPMode, ctxStartIspMode);
                    m_CABACEstimator->getCtx() = SubCtx(Ctx::IntraLumaPlanarFlag, ctxStartPlanarFlag);
                    m_CABACEstimator->getCtx() = SubCtx(Ctx::IntraLumaMpmFlag, ctxStartIntraMode);
                    m_CABACEstimator->getCtx() = SubCtx(Ctx::MultiRefLineIdx, ctxStartMrlIdx);

                    uint64_t fracModeBits = xFracModeBitsIntra(pu, mode, ChannelType::LUMA);

                    double cost = (double) minSadHad + (double) fracModeBits * sqrtLambdaForFirstPass;

#if GDR_ENABLED
                    if (!isEncodeGdrClean || isValidIntraPredLuma(pu, mode))
#endif
                    {
                      const ModeInfo mi(false, false, 0, ISPType::NONE, mode);
                      updateCandList(mi, cost, rdModeList, candCostList, numModesForFullRD);
                      updateCandList(mi, double(minSadHad), hadModeList, candHadList, numHadCand);
                    }

                    satdChecked[mode] = true;
                  }
                }
              }
            }
            if (saveDataForISP)
            {
              // we save the regular intra modes list
              m_ispCandList[ISPType::HOR] = rdModeList;
            }
            pu.multiRefIdx    = 1;
            const int numMPMs = NUM_MOST_PROBABLE_MODES;
            unsigned  multiRefMPM[numMPMs];
            PU::getIntraMPMs(pu, multiRefMPM);
            for (int mRefNum = 1; mRefNum < numOfPassesExtendRef; mRefNum++)
            {
              int multiRefIdx = MULTI_REF_LINE_IDX[mRefNum];

              pu.multiRefIdx = multiRefIdx;
              {
                initIntraPatternChType(cu, pu.Y(), true);
              }
              for (int x = 1; x < numMPMs; x++)
              {
                uint32_t mode = multiRefMPM[x];
                {
                  pu.intraDir[ChannelType::LUMA] = mode;
                  initPredIntraParams(pu, pu.Y(), sps);

                  predIntraAng(COMPONENT_Y, piPred, pu);

                  // Use the min between SAD and SATD as the cost criterion
                  // SAD is scaled by 2 to align with the scaling of HAD
                  Distortion minSadHad =
                    std::min(distParamSad.distFunc(distParamSad) * 2, distParamHad.distFunc(distParamHad));

                  // NB xFracModeBitsIntra will not affect the mode for chroma that may have already been pre-estimated.
                  m_CABACEstimator->getCtx() = SubCtx(Ctx::MipFlag, ctxStartMipFlag);
                  m_CABACEstimator->getCtx() = SubCtx(Ctx::ISPMode, ctxStartIspMode);
                  m_CABACEstimator->getCtx() = SubCtx(Ctx::IntraLumaPlanarFlag, ctxStartPlanarFlag);
                  m_CABACEstimator->getCtx() = SubCtx(Ctx::IntraLumaMpmFlag, ctxStartIntraMode);
                  m_CABACEstimator->getCtx() = SubCtx(Ctx::MultiRefLineIdx, ctxStartMrlIdx);

                  uint64_t fracModeBits = xFracModeBitsIntra(pu, mode, ChannelType::LUMA);

                  double cost = (double) minSadHad + (double) fracModeBits * sqrtLambdaForFirstPass;
#if GDR_ENABLED
                  if (!isEncodeGdrClean || isValidIntraPredLuma(pu, mode))
#endif
                  {
                    const ModeInfo mi(false, false, multiRefIdx, ISPType::NONE, mode);
                    updateCandList(mi, cost, rdModeList, candCostList, numModesForFullRD);
                    updateCandList(mi, double(minSadHad), hadModeList, candHadList, numHadCand);
                  }
                }
              }
            }
#if GDR_ENABLED
            if (!isEncodeGdrClean)
#endif
            {
              CHECKD(rdModeList.size() != numModesForFullRD, "Error: RD mode list size");
            }

            if (lfnstSaveFlag && testMip
                && !allowLfnstWithMip(cu.firstPU->lumaSize()))   // save a different set for the next run
            {
              // save found best modes
              m_savedRdModeListLFNST = rdModeList;
              m_savedModeCostLFNST   = candCostList;
              // PBINTRA fast
              m_savedHadModeListLFNST   = hadModeList;
              m_savedHadListLFNST       = candHadList;
              m_savedNumRdModesLFNST    = g_intraModeNumFastUseMPM2D[logWidth - MIN_CU_LOG2][logHeight - MIN_CU_LOG2];
              m_savedRdModeListLFNST.resize(m_savedNumRdModesLFNST);
              m_savedModeCostLFNST.resize(m_savedNumRdModesLFNST);
              // PBINTRA fast
              m_savedHadModeListLFNST.resize(3);
              m_savedHadListLFNST.resize(3);
              lfnstSaveFlag = false;
            }
            //*** Derive MIP candidates using Hadamard
            if (testMip && !supportedMipBlkSize)
            {
              // avoid estimation for unsupported blk sizes
              const int transpOff    = MatrixIntraPrediction::getNumModesMip(pu.Y());
              const int numModesFull = (transpOff << 1);
              for (uint32_t modeFull = 0; modeFull < numModesFull; modeFull++)
              {
                const bool     isTransposed = (modeFull >= transpOff ? true : false);
                const uint32_t mode         = (isTransposed ? modeFull - transpOff : modeFull);

                numModesForFullRD++;
                rdModeList.push_back(ModeInfo(true, isTransposed, 0, ISPType::NONE, mode));
                candCostList.push_back(0);
              }
            }
            else if (testMip)
            {
              cu.mipFlag     = true;
              pu.multiRefIdx = 0;

              double mipHadCost[MAX_NUM_MIP_MODE] = { MAX_DOUBLE };

              initIntraPatternChType(cu, pu.Y());
              initIntraMip(pu, pu.Y());

              const int transpOff    = MatrixIntraPrediction::getNumModesMip(pu.Y());
              const int numModesFull = (transpOff << 1);
              for (uint32_t modeFull = 0; modeFull < numModesFull; modeFull++)
              {
                const bool     isTransposed = (modeFull >= transpOff ? true : false);
                const uint32_t mode         = (isTransposed ? modeFull - transpOff : modeFull);

                pu.mipTransposedFlag           = isTransposed;
                pu.intraDir[ChannelType::LUMA] = mode;
                predIntraMip(COMPONENT_Y, piPred, pu);

                // Use the min between SAD and HAD as the cost criterion
                // SAD is scaled by 2 to align with the scaling of HAD
                Distortion minSadHad =
                  std::min(distParamSad.distFunc(distParamSad) * 2, distParamHad.distFunc(distParamHad));

                m_CABACEstimator->getCtx() = SubCtx(Ctx::MipFlag, ctxStartMipFlag);

                uint64_t fracModeBits = xFracModeBitsIntra(pu, mode, ChannelType::LUMA);

                double cost            = double(minSadHad) + double(fracModeBits) * sqrtLambdaForFirstPass;
                mipHadCost[modeFull]   = cost;
                DTRACE(g_trace_ctx, D_INTRA_COST, "IntraMIP: %u, %llu, %f (%d)\n", minSadHad, fracModeBits, cost,
                       modeFull);

#if GDR_ENABLED
                if (!isEncodeGdrClean || isValidIntraPredLuma(pu, mode))
#endif
                {
                  const ModeInfo mi(true, isTransposed, 0, ISPType::NONE, mode);
                  updateCandList(mi, cost, rdModeList, candCostList, numModesForFullRD + 1);
                  updateCandList(mi, 0.8 * double(minSadHad), hadModeList, candHadList, numHadCand);
                }
              }

              const double thresholdHadCost = 1.0 + 1.4 / sqrt((double) (pu.lwidth() * pu.lheight()));
              reduceHadCandList(rdModeList, candCostList, numModesForFullRD, thresholdHadCost, mipHadCost, pu, fastMip);
            }
            if (sps.getUseMIP() && lfnstSaveFlag)
            {
              // save found best modes
              m_savedNumRdModesLFNST = numModesForFullRD;
              m_savedRdModeListLFNST = rdModeList;
              m_savedModeCostLFNST   = candCostList;
              // PBINTRA fast
              m_savedHadModeListLFNST   = hadModeList;
              m_savedHadListLFNST       = candHadList;
              lfnstSaveFlag             = false;
            }
          }
          else   // if( sps.getUseMIP() && lfnstLoadFlag)
          {
            // restore saved modes
            numModesForFullRD = m_savedNumRdModesLFNST;
            rdModeList        = m_savedRdModeListLFNST;
            candCostList      = m_savedModeCostLFNST;
            // PBINTRA fast
            hadModeList = m_savedHadModeListLFNST;
            candHadList = m_savedHadListLFNST;
          }

          if (m_pcEncCfg->getFastUDIUseMPMEnabled())
          {
            const int numMPMs = NUM_MOST_PROBABLE_MODES;
            unsigned  preds[numMPMs];

            pu.multiRefIdx = 0;

            const int numCand = PU::getIntraMPMs(pu, preds);

            for (int j = 0; j < numCand; j++)
            {
              bool     mostProbableModeIncluded = false;
              ModeInfo mostProbableMode(false, false, 0, ISPType::NONE, preds[j]);

#if GDR_ENABLED
              int nn = numModesForFullRD;
              if (isEncodeGdrClean)
              {
                nn = std::min((int) numModesForFullRD, (int) rdModeList.size());
              }

              for (int i = 0; i < nn; i++)
#else
              for (int i = 0; i < numModesForFullRD; i++)
#endif
              {
                mostProbableModeIncluded |= (mostProbableMode == rdModeList[i]);
              }
#if GDR_ENABLED
              if (!isEncodeGdrClean && !mostProbableModeIncluded)
#else
              if (!mostProbableModeIncluded)
#endif
              {
                numModesForFullRD++;
                rdModeList.push_back(mostProbableMode);
                candCostList.push_back(0);
              }
            }
            if (saveDataForISP)
            {
              // we add the MPMs to the list that contains only regular intra modes
              for (int j = 0; j < numCand; j++)
              {
                bool     mostProbableModeIncluded = false;
                ModeInfo mostProbableMode(false, false, 0, ISPType::NONE, preds[j]);

                for (const auto &x: m_ispCandList[ISPType::HOR])
                {
                  mostProbableModeIncluded |= mostProbableMode == x;
                }
#if GDR_ENABLED
                if (!isEncodeGdrClean && !mostProbableModeIncluded)
#else
                if (!mostProbableModeIncluded)
#endif
                {
                  m_ispCandList[ISPType::HOR].push_back(mostProbableMode);
                }
              }
            }
          }
        }
        else
        {
          THROW("Full search not supported for MIP");
        }
        if (sps.getUseLFNST() && mtsUsageFlag == 1)
        {
          // Store the modes to be checked with RD
          m_savedNumRdModes[lfnstIdx] = numModesForFullRD;
          std::copy_n(rdModeList.begin(), numModesForFullRD, m_savedRdModeList[lfnstIdx]);
        }
      }
      else   // mtsUsage = 2 (here we potentially reduce the number of modes that will be full-RD checked)
      {
        if ((m_pcEncCfg->getUseFastLFNST() || !cu.slice->isIntra()) && m_bestModeCostValid[lfnstIdx])
        {
          numModesForFullRD = 0;

          double thresholdSkipMode = 1.0 + ((cu.lfnstIdx > 0) ? 0.1 : 1.0) * (1.4 / sqrt((double) (width * height)));

          // Skip checking the modes with much larger R-D cost than the best mode
          for (int i = 0; i < m_savedNumRdModes[lfnstIdx]; i++)
          {
            if (m_modeCostStore[lfnstIdx][i] <= thresholdSkipMode * m_bestModeCostStore[lfnstIdx])
            {
              rdModeList.push_back(m_savedRdModeList[lfnstIdx][i]);
              numModesForFullRD++;
            }
          }
        }
        else   // this is necessary because we skip the candidates list calculation, since it was already obtained for
               // the DCT-II. Now we load it
        {
          // Restore the modes to be checked with RD
          numModesForFullRD = m_savedNumRdModes[lfnstIdx];
          rdModeList.resize(numModesForFullRD);
          std::copy_n(m_savedRdModeList[lfnstIdx], m_savedNumRdModes[lfnstIdx], rdModeList.begin());
          candCostList.resize(numModesForFullRD);
        }
      }

#if GDR_ENABLED
      if (!isEncodeGdrClean)
#endif
      {
        CHECK(numModesForFullRD != rdModeList.size(), "Inconsistent state!");
      }

      // after this point, don't use numModesForFullRD

      // PBINTRA fast
      if (m_pcEncCfg->getUsePbIntraFast() && !cs.slice->isIntra() && rdModeList.size() < numModesAvailable
          && !cs.slice->getDisableSATDForRD() && (mtsUsageFlag != 2 || lfnstIdx > 0))
      {
        double   pbintraRatio = (lfnstIdx > 0) ? 1.25 : PBINTRA_RATIO;
        int      maxSize      = -1;
        ModeInfo bestMipMode;
        int      bestMipIdx = -1;
        for (int idx = 0; idx < rdModeList.size(); idx++)
        {
          if (rdModeList[idx].mipFlg)
          {
            bestMipMode = rdModeList[idx];
            bestMipIdx  = idx;
            break;
          }
        }
        const int numHadCand = 3;
        for (int k = numHadCand - 1; k >= 0; k--)
        {
          if (candHadList.size() < (k + 1) || candHadList[k] > cs.interHad * pbintraRatio)
          {
            maxSize = k;
          }
        }
        if (maxSize > 0)
        {
          rdModeList.resize(std::min<size_t>(rdModeList.size(), maxSize));
          if (bestMipIdx >= 0)
          {
            if (rdModeList.size() <= bestMipIdx)
            {
              rdModeList.push_back(bestMipMode);
            }
          }
          if (saveDataForISP)
          {
            m_ispCandList[ISPType::HOR].resize(std::min<size_t>(m_ispCandList[ISPType::HOR].size(), maxSize));
          }
        }
        if (maxSize == 0)
        {
          cs.dist     = std::numeric_limits<Distortion>::max();
          cs.interHad = 0;

          //===== reset context models =====
          m_CABACEstimator->getCtx() = SubCtx(Ctx::MipFlag, ctxStartMipFlag);
          m_CABACEstimator->getCtx() = SubCtx(Ctx::ISPMode, ctxStartIspMode);
          m_CABACEstimator->getCtx() = SubCtx(Ctx::IntraLumaPlanarFlag, ctxStartPlanarFlag);
          m_CABACEstimator->getCtx() = SubCtx(Ctx::IntraLumaMpmFlag, ctxStartIntraMode);
          m_CABACEstimator->getCtx() = SubCtx(Ctx::MultiRefLineIdx, ctxStartMrlIdx);

          return false;
        }
      }
    }

    int numNonISPModes = (int) rdModeList.size();

    if ( testISP )
    {
      // we reserve positions for ISP in the common full RD list
#if GDR_ENABLED
      if (!isEncodeGdrClean)
#endif
      {
        const int maxNumRDModesISP = sps.getUseLFNST() ? 16 * NUM_LFNST_NUM_PER_SET : 16;
        m_curIspLfnstIdx = 0;
        for (int i = 0; i < maxNumRDModesISP; i++)
        {
          rdModeList.push_back(ModeInfo(false, false, 0, ISPType::RESERVED, 0));
        }
      }
    }

    //===== check modes (using r-d costs) =====
    ModeInfo       bestPuMode;
    BdpcmMode      bestBDPCMMode    = BdpcmMode::NONE;
    double         bestCostNonBDPCM = MAX_DOUBLE;

    CodingStructure *csTemp = m_pTempCS[gp_sizeIdxInfo->idxFrom( cu.lwidth() )][gp_sizeIdxInfo->idxFrom( cu.lheight() )];
    CodingStructure *csBest = m_pBestCS[gp_sizeIdxInfo->idxFrom( cu.lwidth() )][gp_sizeIdxInfo->idxFrom( cu.lheight() )];

    csTemp->slice = cs.slice;
    csBest->slice = cs.slice;
    csTemp->initStructData();
    csBest->initStructData();
    csTemp->picture = cs.picture;
    csBest->picture = cs.picture;

    // just to be sure
    numModesForFullRD = (int) rdModeList.size();
    TUIntraSubPartitioner subTuPartitioner( partitioner );
    if ( testISP )
    {
      m_modeCtrl->setIspCost( MAX_DOUBLE );
      m_modeCtrl->setMtsFirstPassNoIspCost( MAX_DOUBLE );
    }
    int bestLfnstIdx = cu.lfnstIdx;

    for (int mode = isSecondColorSpace ? 0 : -2 * int(testBDPCM); mode < (int) rdModeList.size(); mode++)
    {
      // set CU/PU to luma prediction mode
      ModeInfo orgMode;
      if (sps.getUseColorTrans() && !m_pcEncCfg->getRGBFormatFlag() && isSecondColorSpace && mode)
      {
        continue;
      }

      if (mode < 0
          || (isSecondColorSpace && m_savedBDPCMModeFirstColorSpace[m_savedRdModeIdx][mode] != BdpcmMode::NONE))
      {
        cu.bdpcmMode = mode < 0 ? BdpcmMode(-mode) : m_savedBDPCMModeFirstColorSpace[m_savedRdModeIdx][mode];
        orgMode =
          ModeInfo(false, false, 0, ISPType::NONE, cu.bdpcmMode == BdpcmMode::VER ? VER_IDX : HOR_IDX);
      }
      else
      {
        cu.bdpcmMode = BdpcmMode::NONE;
        orgMode      = rdModeList[mode];
      }
      if (cu.bdpcmMode == BdpcmMode::NONE && rdModeList[mode].ispMod == ISPType::RESERVED)
      {
        if (mode == numNonISPModes)   // the list needs to be sorted only once
        {
          if (m_pcEncCfg->getUseFastISP())
          {
            m_modeCtrl->setBestPredModeDCT2(bestPuMode.modeId, bestPuMode.mipFlg);
          }
          if (!xSortISPCandList(bestCurrentCost, csBest->cost, bestPuMode))
          {
            break;
          }
        }
        xGetNextISPMode(rdModeList[mode], (mode > 0 ? &rdModeList[mode - 1] : nullptr), Size(width, height));
        if (rdModeList[mode].ispMod == ISPType::RESERVED)
        {
          continue;
        }
        cu.lfnstIdx = m_curIspLfnstIdx;
        orgMode     = rdModeList[mode];
      }
      cu.mipFlag                     = orgMode.mipFlg;
      pu.mipTransposedFlag           = orgMode.mipTrFlg;
      cu.ispMode                     = orgMode.ispMod;
      pu.multiRefIdx                 = orgMode.mRefId;
      pu.intraDir[ChannelType::LUMA] = orgMode.modeId;

      CHECK(cu.mipFlag && pu.multiRefIdx, "Error: combination of MIP and MRL not supported");
      CHECK(pu.multiRefIdx && (pu.intraDir[ChannelType::LUMA] == PLANAR_IDX),
            "Error: combination of MRL and Planar mode not supported");
      CHECK(cu.ispMode != ISPType::NONE && cu.mipFlag, "Error: combination of ISP and MIP not supported");
      CHECK(cu.ispMode != ISPType::NONE && pu.multiRefIdx, "Error: combination of ISP and MRL not supported");
      CHECK(cu.ispMode != ISPType::NONE && cu.colorTransform, "Error: combination of ISP and ACT not supported");

      pu.intraDir[ChannelType::CHROMA] = cu.colorTransform ? DM_CHROMA_IDX : pu.intraDir[ChannelType::CHROMA];

      // set context models
      m_CABACEstimator->getCtx() = ctxStart;

      // determine residual for partition
      cs.initSubStructure( *csTemp, partitioner.chType, cs.area, true );

      bool tmpValidReturn = false;
      if (cu.ispMode != ISPType::NONE)
      {
        if ( m_pcEncCfg->getUseFastISP() )
        {
          m_modeCtrl->setISPWasTested(true);
        }
        tmpValidReturn = xIntraCodingLumaISP(*csTemp, subTuPartitioner, bestCurrentCost);
        if (csTemp->tus.size() == 0)
        {
          // no TUs were coded
          csTemp->cost = MAX_DOUBLE;
          continue;
        }
        // we save the data for future tests
        m_ispTestedModes[m_curIspLfnstIdx].setModeResults(
          cu.ispMode, (int) orgMode.modeId, (int) csTemp->tus.size(),
          csTemp->cus[0]->firstTU->cbf[COMPONENT_Y] ? csTemp->cost : MAX_DOUBLE, csBest->cost);
        csTemp->cost = !tmpValidReturn ? MAX_DOUBLE : csTemp->cost;
      }
      else
      {
        if (cu.colorTransform)
        {
          tmpValidReturn = xRecurIntraCodingACTQT(*csTemp, partitioner, mtsCheckRangeFlag, mtsFirstCheckId, mtsLastCheckId, moreProbMTSIdxFirst);
        }
        else
        {
          tmpValidReturn = xRecurIntraCodingLumaQT(*csTemp, partitioner, mtsCheckRangeFlag, mtsFirstCheckId, mtsLastCheckId, moreProbMTSIdxFirst);
        }
      }

      if (cu.ispMode == ISPType::NONE && !cu.mtsFlag && !cu.lfnstIdx && cu.bdpcmMode == BdpcmMode::NONE && !pu.multiRefIdx
          && !cu.mipFlag && testISP)
      {
        m_regIntraRDListWithCosts.push_back(
          ModeInfoWithCost(cu.mipFlag, pu.mipTransposedFlag, pu.multiRefIdx, cu.ispMode, orgMode.modeId, csTemp->cost));
      }

      if (cu.ispMode != ISPType::NONE && !csTemp->cus[0]->firstTU->cbf[COMPONENT_Y])
      {
        csTemp->cost = MAX_DOUBLE;
        csTemp->costDbOffset = 0;
        tmpValidReturn = false;
      }
      validReturn |= tmpValidReturn;

      if (sps.getUseLFNST() && mtsUsageFlag == 1 && cu.ispMode == ISPType::NONE && mode >= 0)
      {
        m_modeCostStore[lfnstIdx][mode] = tmpValidReturn ? csTemp->cost : (MAX_DOUBLE / 2.0); //(MAX_DOUBLE / 2.0) ??
      }

      DTRACE(g_trace_ctx, D_INTRA_COST, "IntraCost T [x=%d,y=%d,w=%d,h=%d] %f (%d,%d,%d,%d,%d,%d) \n", cu.blocks[0].x,
             cu.blocks[0].y, (int) width, (int) height, csTemp->cost, orgMode.modeId, orgMode.ispMod, pu.multiRefIdx,
             cu.mipFlag, cu.lfnstIdx, cu.mtsFlag);

      if( tmpValidReturn )
      {
        if (isFirstColorSpace)
        {
          if (m_pcEncCfg->getRGBFormatFlag() || cu.ispMode == ISPType::NONE)
          {
            sortRdModeListFirstColorSpace(
              orgMode, csTemp->cost, cu.bdpcmMode, m_savedRdModeFirstColorSpace[m_savedRdModeIdx],
              m_savedRdCostFirstColorSpace[m_savedRdModeIdx], m_savedBDPCMModeFirstColorSpace[m_savedRdModeIdx],
              m_numSavedRdModeFirstColorSpace[m_savedRdModeIdx]);
          }
        }
        // check r-d cost
        if( csTemp->cost < csBest->cost )
        {
          std::swap( csTemp, csBest );

          bestPuMode    = orgMode;
          bestBDPCMMode = cu.bdpcmMode;
          if (sps.getUseLFNST() && mtsUsageFlag == 1 && cu.ispMode == ISPType::NONE)
          {
            m_bestModeCostStore[ lfnstIdx ] = csBest->cost; //cs.cost;
            m_bestModeCostValid[ lfnstIdx ] = true;
          }
          if( csBest->cost < bestCurrentCost )
          {
            bestCurrentCost = csBest->cost;
          }
          if (cu.ispMode != ISPType::NONE)
          {
            m_modeCtrl->setIspCost(csBest->cost);
            bestLfnstIdx = cu.lfnstIdx;
          }
          else if ( testISP )
          {
            m_modeCtrl->setMtsFirstPassNoIspCost(csBest->cost);
          }
        }
        if (cu.ispMode == ISPType::NONE && cu.bdpcmMode == BdpcmMode::NONE && csBest->cost < bestCostNonBDPCM)
        {
          bestCostNonBDPCM = csBest->cost;
        }
      }

      csTemp->releaseIntermediateData();
      if( m_pcEncCfg->getFastLocalDualTreeMode() )
      {
        if( cu.isConsIntra() && !cu.slice->isIntra() && csBest->cost != MAX_DOUBLE && costInterCU != COST_UNKNOWN && mode >= 0 )
        {
          if( m_pcEncCfg->getFastLocalDualTreeMode() == 2 )
          {
            //Note: only try one intra mode, which is especially useful to reduce EncT for LDB case (around 4%)
            break;
          }
          else
          {
            if( csBest->cost > costInterCU * 1.5 )
            {
              break;
            }
          }
        }
      }
      if (sps.getUseColorTrans() && !CS::isDualITree(cs))
      {
        if ((m_pcEncCfg->getRGBFormatFlag() && !cu.colorTransform) && csBest->cost != MAX_DOUBLE && bestCS->cost != MAX_DOUBLE && mode >= 0)
        {
          if (csBest->cost > bestCS->cost)
          {
            break;
          }
        }
      }
    } // Mode loop
    cu.ispMode  = bestPuMode.ispMod;
    cu.lfnstIdx = bestLfnstIdx;

    if( validReturn )
    {
      if (cu.colorTransform)
      {
        cs.useSubStructure(*csBest, partitioner.chType, pu, true, true, KEEP_PRED_AND_RESI_SIGNALS, KEEP_PRED_AND_RESI_SIGNALS, true);
      }
      else
      {
        cs.useSubStructure(*csBest, partitioner.chType, pu.singleChan(ChannelType::LUMA), true, true,
                           KEEP_PRED_AND_RESI_SIGNALS, KEEP_PRED_AND_RESI_SIGNALS, true);
      }
    }
    csBest->releaseIntermediateData();
    if( validReturn )
    {
      //=== update PU data ====
      cu.mipFlag                     = bestPuMode.mipFlg;
      pu.mipTransposedFlag           = bestPuMode.mipTrFlg;
      pu.multiRefIdx                 = bestPuMode.mRefId;
      pu.intraDir[ChannelType::LUMA] = bestPuMode.modeId;
      cu.bdpcmMode = bestBDPCMMode;
      if (cu.colorTransform)
      {
        CHECK(pu.intraDir[ChannelType::CHROMA] != DM_CHROMA_IDX,
              "chroma should use DM mode for adaptive color transform");
      }
    }
  }

  //===== reset context models =====
  m_CABACEstimator->getCtx() = ctxStart;

  return validReturn;
}

void IntraSearch::estIntraPredChromaQT( CodingUnit &cu, Partitioner &partitioner, const double maxCostAllowed )
{
  const ChromaFormat format   = cu.chromaFormat;
  const uint32_t    numberValidComponents = getNumberValidComponents(format);
  CodingStructure &cs = *cu.cs;
  const TempCtx      ctxStart(m_ctxPool, m_CABACEstimator->getCtx());

  cs.setDecomp( cs.area.Cb(), false );

  double    bestCostSoFar = maxCostAllowed;
  bool      lumaUsesISP   = !cu.isSepTree() && cu.ispMode != ISPType::NONE;
  PartSplit ispType       = lumaUsesISP ? CU::getISPType( cu, COMPONENT_Y ) : TU_NO_ISP;
  CHECK(cu.ispMode != ISPType::NONE && bestCostSoFar < 0, "bestCostSoFar must be positive!");

  auto &pu = *cu.firstPU;

  {
    uint32_t   bestMode      = 0;
    Distortion bestDist      = 0;
    double     bestCost      = MAX_DOUBLE;
    BdpcmMode  bestBDPCMMode = BdpcmMode::NONE;

    //----- init mode list ----
    {
      int32_t minMode = 0;
      int32_t maxMode = NUM_CHROMA_MODE;
      //----- check chroma modes -----
      uint32_t chromaCandModes[ NUM_CHROMA_MODE ];
      PU::getIntraChromaCandModes( pu, chromaCandModes );

      // create a temporary CS
      CodingStructure &saveCS = *m_pSaveCS[0];
      saveCS.pcv      = cs.pcv;
      saveCS.picture  = cs.picture;
      saveCS.sps      = cs.sps;
      saveCS.area.repositionTo( cs.area );
      saveCS.clearTUs();

      if (!cu.isSepTree() && cu.ispMode != ISPType::NONE)
      {
        saveCS.clearCUs();
        saveCS.clearPUs();
      }

      if( cu.isSepTree() )
      {
        if( partitioner.canSplit( TU_MAX_TR_SPLIT, cs ) )
        {
          partitioner.splitCurrArea( TU_MAX_TR_SPLIT, cs );

          do
          {
            cs.addTU( CS::getArea( cs, partitioner.currArea(), partitioner.chType ), partitioner.chType ).depth = partitioner.currTrDepth;
          } while( partitioner.nextPart( cs ) );

          partitioner.exitCurrSplit();
        }
        else
        {
          cs.addTU(CS::getArea(cs, partitioner.currArea(), partitioner.chType), partitioner.chType);
        }
      }

      auto &orgTUs = m_orgTUs;
      orgTUs.clear();

      if( lumaUsesISP )
      {
        CodingUnit& auxCU = saveCS.addCU( cu, partitioner.chType );
        auxCU.ispMode = cu.ispMode;
        saveCS.addPU( *cu.firstPU, partitioner.chType );
      }

      // create a store for the TUs
      for( const auto &ptu : cs.tus )
      {
        // for split TUs in HEVC, add the TUs without Chroma parts for correct setting of Cbfs
        if (lumaUsesISP || pu.contains(*ptu, ChannelType::CHROMA))
        {
          saveCS.addTU( *ptu, partitioner.chType );
          orgTUs.push_back( ptu );
        }
      }
      if( lumaUsesISP )
      {
        saveCS.clearCUs();
      }
      // SATD pre-selecting.
      int satdModeList[NUM_CHROMA_MODE];
      int64_t satdSortedCost[NUM_CHROMA_MODE];
      for (int i = 0; i < NUM_CHROMA_MODE; i++)
      {
        satdSortedCost[i] = 0; // for the mode not pre-select by SATD, do RDO by default, so set the initial value 0.
        satdModeList[i] = 0;
      }
      bool modeIsEnable[NUM_INTRA_MODE + 1]; // use intra mode idx to check whether enable
      for (int i = 0; i < NUM_INTRA_MODE + 1; i++)
      {
        modeIsEnable[i] = 1;
      }
      DistParam distParamSad;
      DistParam distParamSatd;
      pu.intraDir[ChannelType::CHROMA] =
        MDLM_L_IDX;   // temporary assigned, just to indicate this is a MDLM mode. for luma down-sampling operation.

      initIntraPatternChType(cu, pu.Cb());
      initIntraPatternChType(cu, pu.Cr());
      xGetLumaRecPixels(pu, pu.Cb());

      for (int idx = minMode; idx <= maxMode - 1; idx++)
      {
        int mode = chromaCandModes[idx];
        satdModeList[idx] = mode;
        if (PU::isLMCMode(mode) && (!PU::isLMCModeEnabled(pu, mode) || cu.slice->getDisableLmChromaCheck()))
        {
          continue;
        }
        if ((mode == LM_CHROMA_IDX) || (mode == PLANAR_IDX) || (mode == DM_CHROMA_IDX)) // only pre-check regular modes and MDLM modes, not including DM ,Planar, and LM
        {
          continue;
        }
        pu.intraDir[ChannelType::CHROMA] = mode;   // temporary assigned, for SATD checking.

        int64_t sad = 0;
        int64_t sadCb = 0;
        int64_t satdCb = 0;
        int64_t sadCr = 0;
        int64_t satdCr = 0;
        CodingStructure& cs = *(pu.cs);

        CompArea areaCb = pu.Cb();
        PelBuf orgCb = cs.getOrgBuf(areaCb);
        PelBuf predCb = cs.getPredBuf(areaCb);
        m_pcRdCost->setDistParam(distParamSad, orgCb, predCb, pu.cs->sps->getBitDepth(ChannelType::CHROMA),
                                 COMPONENT_Cb, false);
        m_pcRdCost->setDistParam(distParamSatd, orgCb, predCb, pu.cs->sps->getBitDepth(ChannelType::CHROMA),
                                 COMPONENT_Cb, true);
        distParamSad.applyWeight = false;
        distParamSatd.applyWeight = false;
        if (PU::isLMCMode(mode))
        {
          predIntraChromaLM(COMPONENT_Cb, predCb, pu, areaCb, mode);
        }
        else
        {
          initPredIntraParams(pu, pu.Cb(), *pu.cs->sps);
          predIntraAng(COMPONENT_Cb, predCb, pu);
        }
        sadCb = distParamSad.distFunc(distParamSad) * 2;
        satdCb = distParamSatd.distFunc(distParamSatd);
        sad += std::min(sadCb, satdCb);
        CompArea areaCr = pu.Cr();
        PelBuf orgCr = cs.getOrgBuf(areaCr);
        PelBuf predCr = cs.getPredBuf(areaCr);
        m_pcRdCost->setDistParam(distParamSad, orgCr, predCr, pu.cs->sps->getBitDepth(ChannelType::CHROMA),
                                 COMPONENT_Cr, false);
        m_pcRdCost->setDistParam(distParamSatd, orgCr, predCr, pu.cs->sps->getBitDepth(ChannelType::CHROMA),
                                 COMPONENT_Cr, true);
        distParamSad.applyWeight = false;
        distParamSatd.applyWeight = false;
        if (PU::isLMCMode(mode))
        {
          predIntraChromaLM(COMPONENT_Cr, predCr, pu, areaCr, mode);
        }
        else
        {
          initPredIntraParams(pu, pu.Cr(), *pu.cs->sps);
          predIntraAng(COMPONENT_Cr, predCr, pu);
        }
        sadCr = distParamSad.distFunc(distParamSad) * 2;
        satdCr = distParamSatd.distFunc(distParamSatd);
        sad += std::min(sadCr, satdCr);
        satdSortedCost[idx] = sad;
      }
      // sort the mode based on the cost from small to large.
      int tempIdx = 0;
      int64_t tempCost = 0;
      for (int i = minMode; i <= maxMode - 1; i++)
      {
        for (int j = i + 1; j <= maxMode - 1; j++)
        {
          if (satdSortedCost[j] < satdSortedCost[i])
          {
            tempIdx = satdModeList[i];
            satdModeList[i] = satdModeList[j];
            satdModeList[j] = tempIdx;

            tempCost = satdSortedCost[i];
            satdSortedCost[i] = satdSortedCost[j];
            satdSortedCost[j] = tempCost;
          }
        }
      }
      int reducedModeNumber = 2; // reduce the number of chroma modes
      for (int i = 0; i < reducedModeNumber; i++)
      {
        modeIsEnable[satdModeList[maxMode - 1 - i]] = 0;   // disable the last reducedModeNumber modes
      }

      // save the dist
      Distortion baseDist = cs.dist;

      const bool testBDPCM =
        CU::bdpcmAllowed(cu, COMPONENT_Cb) && cu.ispMode == ISPType::NONE && cu.mtsFlag == 0 && cu.lfnstIdx == 0;
      for (int32_t mode = minMode - (2 * int(testBDPCM)); mode < maxMode; mode++)
      {
        int chromaIntraMode;

        if (mode < 0)
        {
          cu.bdpcmModeChroma = BdpcmMode(-mode);
          chromaIntraMode    = cu.bdpcmModeChroma == BdpcmMode::VER ? chromaCandModes[1] : chromaCandModes[2];
        }
        else
        {
          chromaIntraMode = chromaCandModes[mode];

          cu.bdpcmModeChroma = BdpcmMode::NONE;
          if( PU::isLMCMode( chromaIntraMode ) && ( !PU::isLMCModeEnabled( pu, chromaIntraMode ) || cu.slice->getDisableLmChromaCheck() ) )
          {
            continue;
          }
          if (!modeIsEnable[chromaIntraMode] && PU::isLMCModeEnabled(pu, chromaIntraMode)) // when CCLM is disable, then MDLM is disable. not use satd checking
          {
            continue;
          }
        }
        cs.setDecomp( pu.Cb(), false );
        cs.dist = baseDist;
        //----- restore context models -----
        m_CABACEstimator->getCtx() = ctxStart;

        //----- chroma coding -----
        pu.intraDir[ChannelType::CHROMA] = chromaIntraMode;

        xRecurIntraChromaCodingQT( cs, partitioner, bestCostSoFar, ispType );
        if( lumaUsesISP && cs.dist == MAX_UINT )
        {
          continue;
        }

        if (cs.sps->getTransformSkipEnabledFlag())
        {
          m_CABACEstimator->getCtx() = ctxStart;
        }

        uint64_t fracBits   = xGetIntraFracBitsQT( cs, partitioner, false, true, -1, ispType );
        Distortion dist       = cs.dist;
        double     cost       = m_pcRdCost->calcRdCost(fracBits, dist - baseDist);

        //----- compare -----
#if GDR_ENABLED
        bool allOk = (cost < bestCost);
        if (m_pcEncCfg->getGdrEnabled())
        {
          allOk =
            allOk && bestCost && isValidIntraPredChroma(pu, (int) PU::getCoLocatedIntraLumaMode(pu), chromaIntraMode);
        }

        if (allOk)
#else
        if (cost < bestCost)
#endif
        {
          if (lumaUsesISP && cost < bestCostSoFar)
          {
            bestCostSoFar = cost;
          }
          for (uint32_t i = getFirstComponentOfChannel(ChannelType::CHROMA); i < numberValidComponents; i++)
          {
            const CompArea &area = pu.blocks[i];

            saveCS.getRecoBuf     ( area ).copyFrom( cs.getRecoBuf   ( area ) );
#if KEEP_PRED_AND_RESI_SIGNALS
            saveCS.getPredBuf     ( area ).copyFrom( cs.getPredBuf   ( area ) );
            saveCS.getResiBuf     ( area ).copyFrom( cs.getResiBuf   ( area ) );
#endif
            saveCS.getPredBuf     ( area ).copyFrom( cs.getPredBuf   (area ) );
            cs.picture->getPredBuf( area ).copyFrom( cs.getPredBuf   (area ) );
            cs.picture->getRecoBuf( area ).copyFrom( cs.getRecoBuf( area ) );

            for( uint32_t j = 0; j < saveCS.tus.size(); j++ )
            {
              saveCS.tus[j]->copyComponentFrom( *orgTUs[j], area.compID );
            }
          }

          bestCost      = cost;
          bestDist      = dist;
          bestMode      = chromaIntraMode;
          bestBDPCMMode = cu.bdpcmModeChroma;
        }
      }

      for (uint32_t i = getFirstComponentOfChannel(ChannelType::CHROMA); i < numberValidComponents; i++)
      {
        const CompArea &area = pu.blocks[i];

        cs.getRecoBuf         ( area ).copyFrom( saveCS.getRecoBuf( area ) );
#if KEEP_PRED_AND_RESI_SIGNALS
        cs.getPredBuf         ( area ).copyFrom( saveCS.getPredBuf( area ) );
        cs.getResiBuf         ( area ).copyFrom( saveCS.getResiBuf( area ) );
#endif
        cs.getPredBuf         ( area ).copyFrom( saveCS.getPredBuf( area ) );
        cs.picture->getPredBuf( area ).copyFrom( cs.getPredBuf    ( area ) );

        cs.picture->getRecoBuf( area ).copyFrom( cs.    getRecoBuf( area ) );

        for( uint32_t j = 0; j < saveCS.tus.size(); j++ )
        {
          orgTUs[ j ]->copyComponentFrom( *saveCS.tus[ j ], area.compID );
        }
      }
    }

    pu.intraDir[ChannelType::CHROMA] = bestMode;
    cs.dist            = bestDist;
    cu.bdpcmModeChroma = bestBDPCMMode;
  }

  //----- restore context models -----
  m_CABACEstimator->getCtx() = ctxStart;
  if( lumaUsesISP && bestCostSoFar >= maxCostAllowed )
  {
    cu.ispMode = ISPType::NONE;
  }
}


void IntraSearch::saveCuAreaCostInSCIPU( Area area, double cost )
{
  if( m_numCuInSCIPU < NUM_INTER_CU_INFO_SAVE )
  {
    m_cuAreaInSCIPU[m_numCuInSCIPU] = area;
    m_cuCostInSCIPU[m_numCuInSCIPU] = cost;
    m_numCuInSCIPU++;
  }
}

void IntraSearch::initCuAreaCostInSCIPU()
{
  for( int i = 0; i < NUM_INTER_CU_INFO_SAVE; i++ )
  {
    m_cuAreaInSCIPU[i] = Area();
    m_cuCostInSCIPU[i] = 0;
  }
  m_numCuInSCIPU = 0;
}

void IntraSearch::PLTSearch(CodingStructure &cs, Partitioner& partitioner, ComponentID compBegin, uint32_t numComp)
{
  CodingUnit    &cu = *cs.getCU(partitioner.chType);
  TransformUnit &tu = *cs.getTU(partitioner.chType);
  uint32_t height = cu.block(compBegin).height;
  uint32_t width = cu.block(compBegin).width;
  if (m_pcEncCfg->getLmcs() && (cs.slice->getLmcsEnabledFlag() && m_pcReshape->getCTUFlag()))
  {
    cs.getPredBuf().copyFrom(cs.getOrgBuf());
    cs.getPredBuf().Y().rspSignal(m_pcReshape->getFwdLUT());
  }
  if( cu.isLocalSepTree() )
  {
    cs.prevPLT.curPLTSize[compBegin] = cs.prevPLT.curPLTSize[COMPONENT_Y];
  }
  cu.lastPLTSize[compBegin] = cs.prevPLT.curPLTSize[compBegin];
  //derive palette
  derivePLTLossy(cs, partitioner, compBegin, numComp);
  reorderPLT(cs, partitioner, compBegin, numComp);

  bool idxExist[MAXPLTSIZE + 1] = { false };
  preCalcPLTIndexRD(cs, partitioner, compBegin, numComp); // Pre-calculate distortions for each pixel
  double rdCost = MAX_DOUBLE;
  deriveIndexMap(cs, partitioner, compBegin, numComp, PLT_SCAN_HORTRAV, rdCost, idxExist); // Optimize palette index map (horizontal scan)
  if ((cu.curPLTSize[compBegin] + cu.useEscape[compBegin]) > 1)
  {
    deriveIndexMap(cs, partitioner, compBegin, numComp, PLT_SCAN_VERTRAV, rdCost, idxExist); // Optimize palette index map (vertical scan)
  }
  // Remove unused palette entries
  uint8_t newPLTSize = 0;
  int idxMapping[MAXPLTSIZE + 1];
  memset(idxMapping, -1, sizeof(int) * (MAXPLTSIZE + 1));
  for (int i = 0; i < cu.curPLTSize[compBegin]; i++)
  {
    if (idxExist[i])
    {
      idxMapping[i] = newPLTSize;
      newPLTSize++;
    }
  }
  idxMapping[cu.curPLTSize[compBegin]] = cu.useEscape[compBegin]? newPLTSize: -1;
  if (newPLTSize != cu.curPLTSize[compBegin]) // there exist unused palette entries
  { // update palette table and reuseflag
    Pel curPLTtmp[MAX_NUM_COMPONENT][MAXPLTSIZE];
    int reuseFlagIdx = 0, curPLTtmpIdx = 0, reuseEntrySize = 0;
    memset(cu.reuseflag[compBegin], false, sizeof(bool) * MAXPLTPREDSIZE);
    int compBeginTmp = compBegin;
    int numCompTmp   = numComp;
    if( cu.isLocalSepTree() )
    {
      memset(cu.reuseflag[COMPONENT_Y], false, sizeof(bool) * MAXPLTPREDSIZE);
      compBeginTmp = COMPONENT_Y;
      numCompTmp   = getNumberValidComponents(cu.chromaFormat);
    }
    for (int curIdx = 0; curIdx < cu.curPLTSize[compBegin]; curIdx++)
    {
      if (idxExist[curIdx])
      {
        for (int comp = compBeginTmp; comp < (compBeginTmp + numCompTmp); comp++)
        {
          curPLTtmp[comp][curPLTtmpIdx] = cu.curPLT[comp][curIdx];
        }

        // Update reuse flags
        if (curIdx < cu.reusePLTSize[compBegin])
        {
          bool match = false;
          for (; reuseFlagIdx < cs.prevPLT.curPLTSize[compBegin]; reuseFlagIdx++)
          {
            bool matchTmp = true;
            for (int comp = compBegin; comp < (compBegin + numComp); comp++)
            {
              matchTmp = matchTmp && (curPLTtmp[comp][curPLTtmpIdx] == cs.prevPLT.curPLT[comp][reuseFlagIdx]);
            }
            if (matchTmp)
            {
              match = true;
              break;
            }
          }
          if (match)
          {
            cu.reuseflag[compBegin][reuseFlagIdx] = true;
            if( cu.isLocalSepTree() )
            {
              cu.reuseflag[COMPONENT_Y][reuseFlagIdx] = true;
            }
            reuseEntrySize++;
          }
        }
        curPLTtmpIdx++;
      }
    }
    cu.reusePLTSize[compBegin] = reuseEntrySize;
    // update palette table
    cu.curPLTSize[compBegin] = newPLTSize;
    if( cu.isLocalSepTree() )
    {
      cu.curPLTSize[COMPONENT_Y] = newPLTSize;
    }
    for (int comp = compBeginTmp; comp < (compBeginTmp + numCompTmp); comp++)
    {
      memcpy( cu.curPLT[comp], curPLTtmp[comp], sizeof(Pel)*cu.curPLTSize[compBegin]);
    }
  }
  cu.useRotation[compBegin] = m_bestScanRotationMode;
  int indexMaxSize = cu.useEscape[compBegin] ? (cu.curPLTSize[compBegin] + 1) : cu.curPLTSize[compBegin];
  if (indexMaxSize <= 1)
  {
    cu.useRotation[compBegin] = false;
  }
  //reconstruct pixel
  PelBuf    curPLTIdx = tu.getcurPLTIdx(compBegin);
  for (uint32_t y = 0; y < height; y++)
  {
    for (uint32_t x = 0; x < width; x++)
    {
      curPLTIdx.at(x, y) = idxMapping[curPLTIdx.at(x, y)];
      if (curPLTIdx.at(x, y) == cu.curPLTSize[compBegin])
      {
        calcPixelPred(cs, partitioner, y, x, compBegin, numComp);
      }
      else
      {
        for (uint32_t compID = compBegin; compID < (compBegin + numComp); compID++)
        {
          CompArea area = cu.blocks[compID];
          PelBuf   recBuf = cs.getRecoBuf(area);
          uint32_t scaleX = getComponentScaleX((ComponentID)COMPONENT_Cb, cs.sps->getChromaFormatIdc());
          uint32_t scaleY = getComponentScaleY((ComponentID)COMPONENT_Cb, cs.sps->getChromaFormatIdc());
          if (compBegin != COMPONENT_Y || compID == COMPONENT_Y)
          {
            recBuf.at(x, y) = cu.curPLT[compID][curPLTIdx.at(x, y)];
          }
          else if (compBegin == COMPONENT_Y && compID != COMPONENT_Y && y % (1 << scaleY) == 0 && x % (1 << scaleX) == 0)
          {
            recBuf.at(x >> scaleX, y >> scaleY) = cu.curPLT[compID][curPLTIdx.at(x, y)];
          }
        }
      }
    }
  }

  cs.getPredBuf().fill(0);
  cs.getResiBuf().fill(0);
  cs.getOrgResiBuf().fill(0);

  cs.fracBits = MAX_UINT;
  cs.cost = MAX_DOUBLE;
  Distortion distortion = 0;
  for (uint32_t comp = compBegin; comp < (compBegin + numComp); comp++)
  {
    const ComponentID compID = ComponentID(comp);
    CPelBuf reco = cs.getRecoBuf(compID);
    CPelBuf org = cs.getOrgBuf(compID);
#if WCG_EXT
    if (m_pcEncCfg->getLumaLevelToDeltaQPMapping().isEnabled() || (
      m_pcEncCfg->getLmcs() && (cs.slice->getLmcsEnabledFlag() && m_pcReshape->getCTUFlag())))
    {
      const CPelBuf orgLuma = cs.getOrgBuf(cs.area.blocks[COMPONENT_Y]);

      if (compID == COMPONENT_Y && !(m_pcEncCfg->getLumaLevelToDeltaQPMapping().isEnabled()))
      {
        const CompArea &areaY = cu.Y();

        CompArea tmpArea1(COMPONENT_Y, areaY.chromaFormat, Position(0, 0), areaY.size());
        PelBuf   tmpRecLuma = m_tmpStorageCtu.getBuf(tmpArea1);
        tmpRecLuma.copyFrom(reco);
        tmpRecLuma.rspSignal(m_pcReshape->getInvLUT());
        distortion += m_pcRdCost->getDistPart(org, tmpRecLuma, cs.sps->getBitDepth(toChannelType(compID)), compID,
                                              DFuncWtd::SSE_WTD, orgLuma);
      }
      else
      {
        distortion += m_pcRdCost->getDistPart(org, reco, cs.sps->getBitDepth(toChannelType(compID)), compID,
                                              DFuncWtd::SSE_WTD, orgLuma);
      }
    }
    else
#endif
    {
      distortion += m_pcRdCost->getDistPart(org, reco, cs.sps->getBitDepth(toChannelType(compID)), compID, DFunc::SSE);
    }
  }

  cs.dist += distortion;
  const CompArea &area = cu.blocks[compBegin];
  cs.setDecomp(area);
  cs.picture->getRecoBuf(area).copyFrom(cs.getRecoBuf(area));
}

void IntraSearch::calcPixelPredRD(CodingStructure& cs, Partitioner& partitioner, Pel* orgBuf, Pel* paPixelValue, Pel* paRecoValue, ComponentID compBegin, uint32_t numComp)
{
  CodingUnit &cu = *cs.getCU(partitioner.chType);
  TransformUnit &tu = *cs.getTU(partitioner.chType);

  int qp[3];
  int qpRem[3];
  int qpPer[3];
  int quantiserScale[3];
  int quantiserRightShift[3];
  int rightShiftOffset[3];
  int invquantiserRightShift[3];
  int add[3];

  for (uint32_t ch = compBegin; ch < (compBegin + numComp); ch++)
  {
    QpParam cQP(tu, ComponentID(ch));
    qp[ch] = cQP.Qp(true);
    qpRem[ch] = qp[ch] % 6;
    qpPer[ch] = qp[ch] / 6;
    quantiserScale[ch] = g_quantScales[0][qpRem[ch]];
    quantiserRightShift[ch] = QUANT_SHIFT + qpPer[ch];
    rightShiftOffset[ch] = 1 << (quantiserRightShift[ch] - 1);
    invquantiserRightShift[ch] = IQUANT_SHIFT;
    add[ch] = 1 << (invquantiserRightShift[ch] - 1);
  }

  for (uint32_t ch = compBegin; ch < (compBegin + numComp); ch++)
  {
    const int  channelBitDepth = cu.cs->sps->getBitDepth(toChannelType((ComponentID)ch));
    paPixelValue[ch] = Pel(std::max<int>(0, ((orgBuf[ch] * quantiserScale[ch] + rightShiftOffset[ch]) >> quantiserRightShift[ch])));
    assert(paPixelValue[ch] < (1 << (channelBitDepth + 1)));
    paRecoValue[ch] = (((paPixelValue[ch] * g_invQuantScales[0][qpRem[ch]]) << qpPer[ch]) + add[ch]) >> invquantiserRightShift[ch];
    paRecoValue[ch] = Pel(ClipBD<int>(paRecoValue[ch], channelBitDepth));//to be checked
  }
}

void IntraSearch::preCalcPLTIndexRD(CodingStructure& cs, Partitioner& partitioner, ComponentID compBegin, uint32_t numComp)
{
  CodingUnit &cu = *cs.getCU(partitioner.chType);
  uint32_t height = cu.block(compBegin).height;
  uint32_t width = cu.block(compBegin).width;
  bool lossless = (m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && cs.slice->isLossless());

  CPelBuf   orgBuf[3];
  for (int comp = compBegin; comp < (compBegin + numComp); comp++)
  {
    CompArea  area = cu.blocks[comp];
    if (m_pcEncCfg->getLmcs() && (cs.slice->getLmcsEnabledFlag() && m_pcReshape->getCTUFlag()))
    {
      orgBuf[comp] = cs.getPredBuf(area);
    }
    else
    {
      orgBuf[comp] = cs.getOrgBuf(area);
    }
  }

  int rasPos;
  uint32_t scaleX = getComponentScaleX(COMPONENT_Cb, cs.sps->getChromaFormatIdc());
  uint32_t scaleY = getComponentScaleY(COMPONENT_Cb, cs.sps->getChromaFormatIdc());
  for (uint32_t y = 0; y < height; y++)
  {
    for (uint32_t x = 0; x < width; x++)
    {
      rasPos = y * width + x;;
      // chroma discard
      bool discardChroma = (compBegin == COMPONENT_Y) && (y&scaleY || x&scaleX);
      Pel curPel[3];
      for (int comp = compBegin; comp < (compBegin + numComp); comp++)
      {
        uint32_t pX1 = (comp > 0 && compBegin == COMPONENT_Y) ? (x >> scaleX) : x;
        uint32_t pY1 = (comp > 0 && compBegin == COMPONENT_Y) ? (y >> scaleY) : y;
        curPel[comp] = orgBuf[comp].at(pX1, pY1);
      }

      uint8_t  pltIdx = 0;
      double minError = MAX_DOUBLE;
      uint8_t  bestIdx = 0;
      for (uint8_t z = 0; z < cu.curPLTSize[compBegin]; z++)
      {
        m_indexError[z][rasPos] = minError;
      }
      while (pltIdx < cu.curPLTSize[compBegin])
      {
        uint64_t sqrtError = 0;
        if (lossless)
        {
          for (int comp = compBegin; comp < (discardChroma ? 1 : (compBegin + numComp)); comp++)
          {
            sqrtError += int64_t(abs(curPel[comp] - cu.curPLT[comp][pltIdx]));
          }
          if (sqrtError == 0)
          {
            m_indexError[pltIdx][rasPos] = (double) sqrtError;
            minError                     = (double) sqrtError;
            bestIdx                      = pltIdx;
            break;
          }
        }
        else
        {
          for (int comp = compBegin; comp < (discardChroma ? 1 : (compBegin + numComp)); comp++)
          {
            int64_t tmpErr = int64_t(curPel[comp] - cu.curPLT[comp][pltIdx]);
            if (isChroma((ComponentID) comp))
            {
              sqrtError += uint64_t(tmpErr * tmpErr * ENC_CHROMA_WEIGHTING);
            }
            else
            {
              sqrtError += tmpErr * tmpErr;
            }
          }
          m_indexError[pltIdx][rasPos] = (double) sqrtError;
          if (sqrtError < minError)
          {
            minError = (double) sqrtError;
            bestIdx  = pltIdx;
          }
        }
        pltIdx++;
      }

      Pel paPixelValue[3], paRecoValue[3];
      if (!lossless)
      {
        calcPixelPredRD(cs, partitioner, curPel, paPixelValue, paRecoValue, compBegin, numComp);
      }
      uint64_t error = 0, rate = 0;
      for (int comp = compBegin; comp < (discardChroma ? 1 : (compBegin + numComp)); comp++)
      {
        if (lossless)
        {
          rate += getEpExGolombNumBins(curPel[comp], 5);
        }
        else
        {
          int64_t tmpErr = int64_t(curPel[comp] - paRecoValue[comp]);
          if (isChroma((ComponentID) comp))
          {
            error += uint64_t(tmpErr * tmpErr * ENC_CHROMA_WEIGHTING);
          }
          else
          {
            error += tmpErr * tmpErr;
          }
          rate += getEpExGolombNumBins(paPixelValue[comp], 5);   // encode quantized escape color
        }
      }
      double rdCost = (double)error + m_pcRdCost->getLambda()*(double)rate;
      m_indexError[cu.curPLTSize[compBegin]][rasPos] = rdCost;
      if (rdCost < minError)
      {
        minError = rdCost;
        bestIdx = (uint8_t)cu.curPLTSize[compBegin];
      }
      m_minErrorIndexMap[rasPos] = bestIdx; // save the optimal index of the current pixel
    }
  }
}

void IntraSearch::deriveIndexMap(CodingStructure &cs, Partitioner &partitioner, ComponentID compBegin, uint32_t numComp,
                                 PLTScanMode pltScanMode, double &minCost, bool *idxExist)
{
  CodingUnit    &cu = *cs.getCU(partitioner.chType);
  TransformUnit &tu = *cs.getTU(partitioner.chType);
  uint32_t      height = cu.block(compBegin).height;
  uint32_t      width = cu.block(compBegin).width;

  int   total     = height*width;
  Pel  *runIndex = tu.getPLTIndex(compBegin);
  PLTRunMode* runType   = tu.getRunTypes(toChannelType(compBegin));
  m_scanOrder = g_scanOrder[SCAN_UNGROUPED][pltScanMode ? CoeffScanType::TRAV_VER : CoeffScanType::TRAV_HOR][gp_sizeIdxInfo->idxFrom(width)][gp_sizeIdxInfo->idxFrom(height)];
// Trellis initialization
  for (int i = 0; i < 2; i++)
  {
    std::fill_n(m_prevRunTypeRDOQ[i], NUM_TRELLIS_STATE, PLTRunMode::INDEX);
    memset(m_prevRunPosRDOQ[i],  0, sizeof(int)*NUM_TRELLIS_STATE);
    memset(m_stateCostRDOQ[i],  0, sizeof (double)*NUM_TRELLIS_STATE);
  }
  for (int state = 0; state < NUM_TRELLIS_STATE; state++)
  {
    m_statePtRDOQ[state][0] = 0;
  }
// Context modeling
  const FracBitsAccess& fracBits = m_CABACEstimator->getCtx().getFracBitsAcess();
  BinFracBits fracBitsPltCopyFlagIndex[RUN_IDX_THRE + 1];
  for (int dist = 0; dist <= RUN_IDX_THRE; dist++)
  {
    const unsigned ctxId           = DeriveCtx::CtxPltCopyFlag(PLTRunMode::INDEX, dist);
    fracBitsPltCopyFlagIndex[dist] = fracBits.getFracBitsArray(Ctx::IdxRunModel( ctxId ) );
  }
  BinFracBits fracBitsPltCopyFlagAbove[RUN_IDX_THRE + 1];
  for (int dist = 0; dist <= RUN_IDX_THRE; dist++)
  {
    const unsigned ctxId           = DeriveCtx::CtxPltCopyFlag(PLTRunMode::COPY, dist);
    fracBitsPltCopyFlagAbove[dist] = fracBits.getFracBitsArray(Ctx::CopyRunModel( ctxId ) );
  }
  const BinFracBits fracBitsPltRunType = fracBits.getFracBitsArray( Ctx::RunTypeFlag() );

// Trellis RDO per CG
  bool contTrellisRD = true;
  for (int subSetId = 0; ( subSetId <= (total - 1) >> LOG2_PALETTE_CG_SIZE ) && contTrellisRD; subSetId++)
  {
    int minSubPos = subSetId << LOG2_PALETTE_CG_SIZE;
    int maxSubPos = minSubPos + (1 << LOG2_PALETTE_CG_SIZE);
    maxSubPos = (maxSubPos > total) ? total : maxSubPos; // if last position is out of the current CU size
    contTrellisRD =
      deriveSubblockIndexMap(cs, partitioner, compBegin, pltScanMode, minSubPos, maxSubPos, fracBitsPltRunType,
                             fracBitsPltCopyFlagIndex, fracBitsPltCopyFlagAbove, minCost, (bool) pltScanMode);
  }
  if (!contTrellisRD)
  {
    return;
  }


// best state at the last scan position
  double  sumRdCost = MAX_DOUBLE;
  uint8_t bestState = 0;
  for (uint8_t state = 0; state < NUM_TRELLIS_STATE; state++)
  {
    if (m_stateCostRDOQ[0][state] < sumRdCost)
    {
      sumRdCost = m_stateCostRDOQ[0][state];
      bestState = state;
    }
  }

  PLTRunMode checkRunTable[MAX_CU_BLKSIZE_PLT * MAX_CU_BLKSIZE_PLT];
  uint8_t checkIndexTable[MAX_CU_BLKSIZE_PLT*MAX_CU_BLKSIZE_PLT];
  uint8_t bestStateTable [MAX_CU_BLKSIZE_PLT*MAX_CU_BLKSIZE_PLT];
  uint8_t nextState = bestState;
// best trellis path
  for (int i = (width*height - 1); i >= 0; i--)
  {
    bestStateTable[i] = nextState;
    int rasterPos = m_scanOrder[i].idx;
    nextState = m_statePtRDOQ[nextState][rasterPos];
  }
// reconstruct index and runs based on the state pointers
  for (int i = 0; i < (width*height); i++)
  {
    int rasterPos = m_scanOrder[i].idx;
    int  abovePos = (pltScanMode == PLT_SCAN_HORTRAV) ? m_scanOrder[i].idx - width : m_scanOrder[i].idx - 1;
        nextState = bestStateTable[i];
    if ( nextState == 0 ) // same as the previous
    {
      checkRunTable[rasterPos] = checkRunTable[ m_scanOrder[i - 1].idx ];
      if (checkRunTable[rasterPos] == PLTRunMode::INDEX)
      {
        checkIndexTable[rasterPos] = checkIndexTable[m_scanOrder[i - 1].idx];
      }
      else
      {
        checkIndexTable[rasterPos] = checkIndexTable[ abovePos ];
      }
    }
    else if (nextState == 1) // CopyAbove mode
    {
      checkRunTable[rasterPos]   = PLTRunMode::COPY;
      checkIndexTable[rasterPos] = checkIndexTable[abovePos];
    }
    else if (nextState == 2) // Index mode
    {
      checkRunTable[rasterPos]   = PLTRunMode::INDEX;
      checkIndexTable[rasterPos] = m_minErrorIndexMap[rasterPos];
    }
  }

// Escape flag
  m_bestEscape = false;
  for (int pos = 0; pos < (width*height); pos++)
  {
    uint8_t index = checkIndexTable[pos];
    if (index == cu.curPLTSize[compBegin])
    {
      m_bestEscape = true;
      break;
    }
  }

// Horizontal scan v.s vertical scan
  if (sumRdCost < minCost)
  {
    cu.useEscape[compBegin] = m_bestEscape;
    m_bestScanRotationMode = pltScanMode;
    memset(idxExist, false, sizeof(bool) * (MAXPLTSIZE + 1));
    for (int pos = 0; pos < (width*height); pos++)
    {
      runIndex[pos] = checkIndexTable[pos];
      runType[pos] = checkRunTable[pos];
      idxExist[checkIndexTable[pos]] = true;
    }
    minCost = sumRdCost;
  }
}

bool IntraSearch::deriveSubblockIndexMap(CodingStructure &cs, Partitioner &partitioner, ComponentID compBegin,
                                         PLTScanMode pltScanMode, int minSubPos, int maxSubPos,
                                         const BinFracBits &fracBitsPltRunType,
                                         const BinFracBits *fracBitsPltIndexINDEX,
                                         const BinFracBits *fracBitsPltIndexCOPY, const double minCost, bool useRotate)
{
  CodingUnit &cu    = *cs.getCU(partitioner.chType);
  uint32_t   height = cu.block(compBegin).height;
  uint32_t   width  = cu.block(compBegin).width;
  int indexMaxValue = cu.curPLTSize[compBegin];

  int refId = 0;
  int currRasterPos, currScanPos, prevScanPos, aboveScanPos, roffset;
  int log2Width = (pltScanMode == PLT_SCAN_HORTRAV) ? floorLog2(width): floorLog2(height);
  int buffersize = (pltScanMode == PLT_SCAN_HORTRAV) ? 2*width: 2*height;
  for (int curPos = minSubPos; curPos < maxSubPos; curPos++)
  {
    currRasterPos = m_scanOrder[curPos].idx;
    prevScanPos = (curPos == 0) ? 0 : (curPos - 1) % buffersize;
    roffset = (curPos >> log2Width) << log2Width;
    aboveScanPos = roffset - (curPos - roffset + 1);
    aboveScanPos %= buffersize;
    currScanPos = curPos % buffersize;
    if ((pltScanMode == PLT_SCAN_HORTRAV && curPos < width) || (pltScanMode == PLT_SCAN_VERTRAV && curPos < height))
    {
      aboveScanPos = -1; // first column/row: above row is not valid
    }

// Trellis stats:
// 1st state: same as previous scanned sample
// 2nd state: Copy_Above mode
// 3rd state: Index mode
// Loop of current state
    for ( int curState = 0; curState < NUM_TRELLIS_STATE; curState++ )
    {
      double    minRdCost          = MAX_DOUBLE;
      int       minState           = 0; // best prevState
      uint8_t   bestRunIndex       = 0;
      auto      bestRunType        = PLTRunMode::INDEX;
      auto      bestPrevCodedType  = PLTRunMode::INDEX;
      int       bestPrevCodedPos   = 0;
      if ( ( curState == 0 && curPos == 0 ) || ( curState == 1 && aboveScanPos < 0 ) ) // state not available
      {
        m_stateCostRDOQ[1 - refId][curState] = MAX_DOUBLE;
        continue;
      }

      PLTRunMode runType  = PLTRunMode::INDEX;
      uint8_t runIndex = 0;
      if ( curState == 1 ) // 2nd state: Copy_Above mode
      {
        runType = PLTRunMode::COPY;
      }
      else if ( curState == 2 ) // 3rd state: Index mode
      {
        runType  = PLTRunMode::INDEX;
        runIndex = m_minErrorIndexMap[currRasterPos];
      }

// Loop of previous state
      for ( int stateID = 0; stateID < NUM_TRELLIS_STATE; stateID++ )
      {
        if ( m_stateCostRDOQ[refId][stateID] == MAX_DOUBLE )
        {
          continue;
        }
        if ( curState == 0 ) // 1st state: same as previous scanned sample
        {
          runType = m_runMapRDOQ[refId][stateID][prevScanPos];
          runIndex = (runType == PLTRunMode::INDEX) ? m_indexMapRDOQ[refId][stateID][prevScanPos]
                                                    : m_indexMapRDOQ[refId][stateID][aboveScanPos];
        }
        else if ( curState == 1 ) // 2nd state: Copy_Above mode
        {
          runIndex = m_indexMapRDOQ[refId][stateID][aboveScanPos];
        }
        PLTRunMode prevRunType   = m_runMapRDOQ[refId][stateID][prevScanPos];
        uint8_t prevRunIndex  = m_indexMapRDOQ[refId][stateID][prevScanPos];
        uint8_t aboveRunIndex = (aboveScanPos >= 0) ? m_indexMapRDOQ[refId][stateID][aboveScanPos] : 0;
        int      dist = curPos - m_prevRunPosRDOQ[refId][stateID] - 1;
        double rdCost = m_stateCostRDOQ[refId][stateID];
        if (rdCost >= minRdCost)
        {
          continue;
        }

// Calculate Rd cost
        PLTRunMode         prevCodedRunType = m_prevRunTypeRDOQ[refId][stateID];
        int  prevCodedPos     = m_prevRunPosRDOQ [refId][stateID];
        const BinFracBits* fracBitsPt =
          (m_prevRunTypeRDOQ[refId][stateID] == PLTRunMode::INDEX) ? fracBitsPltIndexINDEX : fracBitsPltIndexCOPY;
        rdCost += rateDistOptPLT(runType, runIndex, prevRunType, prevRunIndex, aboveRunIndex, prevCodedRunType, prevCodedPos, curPos, (pltScanMode == PLT_SCAN_HORTRAV) ? width : height, dist, indexMaxValue, fracBitsPt, fracBitsPltRunType);
        if (rdCost < minRdCost) // update minState ( minRdCost )
        {
          minRdCost    = rdCost;
          minState     = stateID;
          bestRunType  = runType;
          bestRunIndex = runIndex;
          bestPrevCodedType = prevCodedRunType;
          bestPrevCodedPos  = prevCodedPos;
        }
      }
// Update trellis info of current state
      m_stateCostRDOQ  [1 - refId][curState]  = minRdCost;
      m_prevRunTypeRDOQ[1 - refId][curState]  = bestPrevCodedType;
      m_prevRunPosRDOQ [1 - refId][curState]  = bestPrevCodedPos;
      m_statePtRDOQ[curState][currRasterPos] = minState;
      int buffer2update = std::min(buffersize, curPos);
      memcpy(m_indexMapRDOQ[1 - refId][curState], m_indexMapRDOQ[refId][minState], sizeof(uint8_t)*buffer2update);
      std::copy_n(m_runMapRDOQ[refId][minState], buffer2update, m_runMapRDOQ[1 - refId][curState]);
      m_indexMapRDOQ[1 - refId][curState][currScanPos] = bestRunIndex;
      m_runMapRDOQ  [1 - refId][curState][currScanPos] = bestRunType;
    }

    if (useRotate) // early terminate: Rd cost >= min cost in horizontal scan
    {
      if ((m_stateCostRDOQ[1 - refId][0] >= minCost) &&
         (m_stateCostRDOQ[1 - refId][1] >= minCost) &&
         (m_stateCostRDOQ[1 - refId][2] >= minCost) )
      {
        return 0;
      }
    }
    refId = 1 - refId;
  }
  return 1;
}

double IntraSearch::rateDistOptPLT(PLTRunMode runType, uint8_t runIndex, PLTRunMode prevRunType, uint8_t prevRunIndex,
                                   uint8_t aboveRunIndex, PLTRunMode& prevCodedRunType, int& prevCodedPos, int scanPos,
                                   uint32_t width, int dist, int indexMaxValue, const BinFracBits* IndexfracBits,
                                   const BinFracBits& TypefracBits)
{
  double rdCost = 0.0;
  bool   identityFlag = !((runType != prevRunType) || ((runType == PLTRunMode::INDEX) && (runIndex != prevRunIndex)));

  if ((!identityFlag && runType == PLTRunMode::INDEX) || scanPos == 0)   // encode index value
  {
    uint8_t refIndex = (prevRunType == PLTRunMode::INDEX) ? prevRunIndex : aboveRunIndex;
    refIndex = (scanPos == 0) ? ( indexMaxValue + 1) : refIndex;
    if ( runIndex == refIndex )
    {
      rdCost = MAX_DOUBLE;
      return rdCost;
    }
    rdCost += m_pcRdCost->getLambda()*(getTruncBinBits((runIndex > refIndex) ? runIndex - 1 : runIndex, (scanPos == 0) ? (indexMaxValue + 1) : indexMaxValue)  << SCALE_BITS);
  }
  rdCost += m_indexError[runIndex][m_scanOrder[scanPos].idx] * (1 << SCALE_BITS);
  if (scanPos > 0)
  {
    rdCost += m_pcRdCost->getLambda()*( identityFlag ? (IndexfracBits[(dist < RUN_IDX_THRE) ? dist : RUN_IDX_THRE].intBits[1]) : (IndexfracBits[(dist < RUN_IDX_THRE) ? dist : RUN_IDX_THRE].intBits[0] ) );
  }
  if (!identityFlag && scanPos >= width && prevRunType != PLTRunMode::COPY)
  {
    rdCost += m_pcRdCost->getLambda() * TypefracBits.intBits[runType == PLTRunMode::INDEX ? 0 : 1];
  }
  if (!identityFlag || scanPos == 0)
  {
    prevCodedRunType = runType;
    prevCodedPos = scanPos;
  }
  return rdCost;
}

uint32_t IntraSearch::getEpExGolombNumBins(uint32_t symbol, uint32_t count)
{
  uint32_t numBins = 0;
  while (symbol >= (uint32_t)(1 << count))
  {
    numBins++;
    symbol -= 1 << count;
    count++;
  }
  numBins++;
  numBins += count;
  assert(numBins <= 32);
  return numBins;
}

uint32_t IntraSearch::getTruncBinBits(const uint32_t symbol, const uint32_t numSymbols)
{
  CHECKD(symbol >= numSymbols, "symbol must be less than numSymbols");

  const uint32_t thresh = floorLog2(numSymbols);

  const uint32_t val = 1 << thresh;

  const uint32_t b = numSymbols - val;

  return symbol < val - b ? thresh : thresh + 1;
}

void IntraSearch::calcPixelPred(CodingStructure& cs, Partitioner& partitioner, uint32_t yPos, uint32_t xPos, ComponentID compBegin, uint32_t numComp)
{
  CodingUnit    &cu = *cs.getCU(partitioner.chType);
  TransformUnit &tu = *cs.getTU(partitioner.chType);
  bool lossless = (m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && cs.slice->isLossless());

  CPelBuf   orgBuf[3];
  for (int comp = compBegin; comp < (compBegin + numComp); comp++)
  {
    CompArea  area = cu.blocks[comp];
    if (m_pcEncCfg->getLmcs() && (cs.slice->getLmcsEnabledFlag() && m_pcReshape->getCTUFlag()))
    {
      orgBuf[comp] = cs.getPredBuf(area);
    }
    else
    {
      orgBuf[comp] = cs.getOrgBuf(area);
    }
  }

  int qp[3];
  int qpRem[3];
  int qpPer[3];
  int quantiserScale[3];
  int quantiserRightShift[3];
  int rightShiftOffset[3];
  int invquantiserRightShift[3];
  int add[3];
  if (!lossless)
  {
    for (uint32_t ch = compBegin; ch < (compBegin + numComp); ch++)
    {
      QpParam cQP(tu, ComponentID(ch));
      qp[ch]                     = cQP.Qp(true);
      qpRem[ch]                  = qp[ch] % 6;
      qpPer[ch]                  = qp[ch] / 6;
      quantiserScale[ch]         = g_quantScales[0][qpRem[ch]];
      quantiserRightShift[ch]    = QUANT_SHIFT + qpPer[ch];
      rightShiftOffset[ch]       = 1 << (quantiserRightShift[ch] - 1);
      invquantiserRightShift[ch] = IQUANT_SHIFT;
      add[ch]                    = 1 << (invquantiserRightShift[ch] - 1);
    }
  }

  uint32_t scaleX = getComponentScaleX(COMPONENT_Cb, cs.sps->getChromaFormatIdc());
  uint32_t scaleY = getComponentScaleY(COMPONENT_Cb, cs.sps->getChromaFormatIdc());
  for (uint32_t ch = compBegin; ch < (compBegin + numComp); ch++)
  {
    const int channelBitDepth = cu.cs->sps->getBitDepth(toChannelType((ComponentID)ch));
    CompArea  area = cu.blocks[ch];
    PelBuf    recBuf = cs.getRecoBuf(area);
    PLTescapeBuf escapeValue = tu.getescapeValue((ComponentID)ch);
    if (compBegin != COMPONENT_Y || ch == 0)
    {
      if (lossless)
      {
        escapeValue.at(xPos, yPos) = orgBuf[ch].at(xPos, yPos);
        recBuf.at(xPos, yPos)      = orgBuf[ch].at(xPos, yPos);
      }
      else
      {
        escapeValue.at(xPos, yPos) = std::max<TCoeff>(
          0, ((orgBuf[ch].at(xPos, yPos) * quantiserScale[ch] + rightShiftOffset[ch]) >> quantiserRightShift[ch]));
        assert(escapeValue.at(xPos, yPos) < (TCoeff(1) << (channelBitDepth + 1)));
        TCoeff value = (((escapeValue.at(xPos, yPos) * g_invQuantScales[0][qpRem[ch]]) << qpPer[ch]) + add[ch])
                       >> invquantiserRightShift[ch];
        recBuf.at(xPos, yPos) = Pel(ClipBD<TCoeff>(value, channelBitDepth));   // to be checked
      }
    }
    else if (compBegin == COMPONENT_Y && ch > 0 && yPos % (1 << scaleY) == 0 && xPos % (1 << scaleX) == 0)
    {
      uint32_t yPosC = yPos >> scaleY;
      uint32_t xPosC = xPos >> scaleX;
      if (lossless)
      {
        escapeValue.at(xPosC, yPosC) = orgBuf[ch].at(xPosC, yPosC);
        recBuf.at(xPosC, yPosC)      = orgBuf[ch].at(xPosC, yPosC);
      }
      else
      {
        escapeValue.at(xPosC, yPosC) = std::max<TCoeff>(
          0, ((orgBuf[ch].at(xPosC, yPosC) * quantiserScale[ch] + rightShiftOffset[ch]) >> quantiserRightShift[ch]));
        assert(escapeValue.at(xPosC, yPosC) < (TCoeff(1) << (channelBitDepth + 1)));
        TCoeff value = (((escapeValue.at(xPosC, yPosC) * g_invQuantScales[0][qpRem[ch]]) << qpPer[ch]) + add[ch])
                       >> invquantiserRightShift[ch];
        recBuf.at(xPosC, yPosC) = Pel(ClipBD<TCoeff>(value, channelBitDepth));   // to be checked
      }
    }
  }
}

void IntraSearch::derivePLTLossy(CodingStructure& cs, Partitioner& partitioner, ComponentID compBegin, uint32_t numComp)
{
  CodingUnit &cu = *cs.getCU(partitioner.chType);
  const int   channelBitDepth_L = cs.sps->getBitDepth(ChannelType::LUMA);
  const int   channelBitDepth_C = cs.sps->getBitDepth(ChannelType::CHROMA);
  bool lossless        = (m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && cs.slice->isLossless());
  int  pcmShiftRight_L = (channelBitDepth_L - PLT_ENCBITDEPTH);
  int  pcmShiftRight_C = (channelBitDepth_C - PLT_ENCBITDEPTH);
  if (lossless)
  {
    pcmShiftRight_L = 0;
    pcmShiftRight_C = 0;
  }

  int maxPltSize = cu.isSepTree() ? MAXPLTSIZE_DUALTREE : MAXPLTSIZE;

  uint32_t height = cu.block(compBegin).height;
  uint32_t width = cu.block(compBegin).width;

  CPelBuf   orgBuf[3];
  for (int comp = compBegin; comp < (compBegin + numComp); comp++)
  {
    CompArea  area = cu.blocks[comp];
    if (m_pcEncCfg->getLmcs() && (cs.slice->getLmcsEnabledFlag() && m_pcReshape->getCTUFlag()))
    {
      orgBuf[comp] = cs.getPredBuf(area);
    }
    else
    {
      orgBuf[comp] = cs.getOrgBuf(area);
    }
  }

  TransformUnit &tu = *cs.getTU(partitioner.chType);
  QpParam cQP(tu, compBegin);
  int qp = cQP.Qp(true) - 6*(channelBitDepth_L - 8);
  qp = (qp < 0) ? 0 : ((qp > 56) ? 56 : qp);
  int errorLimit = g_paletteQuant[qp];

  if (lossless)
  {
    errorLimit = 0;
  }
  uint32_t totalSize = height*width;
  SortingElement *pelList = new SortingElement[totalSize];
  SortingElement  element;
  SortingElement *pelListSort = new SortingElement[MAXPLTSIZE + 1];
  uint32_t dictMaxSize = maxPltSize;
  uint32_t idx = 0;
  int last = -1;

  uint32_t scaleX = getComponentScaleX(COMPONENT_Cb, cs.sps->getChromaFormatIdc());
  uint32_t scaleY = getComponentScaleY(COMPONENT_Cb, cs.sps->getChromaFormatIdc());
  for (uint32_t y = 0; y < height; y++)
  {
    for (uint32_t x = 0; x < width; x++)
    {
      uint32_t org[3], pX, pY;
      for (int comp = compBegin; comp < (compBegin + numComp); comp++)
      {
        pX = (comp > 0 && compBegin == COMPONENT_Y) ? (x >> scaleX) : x;
        pY = (comp > 0 && compBegin == COMPONENT_Y) ? (y >> scaleY) : y;
        org[comp] = orgBuf[comp].at(pX, pY);
      }
      element.setAll(org, compBegin, numComp);

      ComponentID tmpCompBegin = compBegin;
      int tmpNumComp = numComp;
      if (cs.sps->getChromaFormatIdc() != ChromaFormat::_444 && numComp == 3
          && (x != ((x >> scaleX) << scaleX) || (y != ((y >> scaleY) << scaleY))))
      {
        tmpCompBegin = COMPONENT_Y;
        tmpNumComp   = 1;
      }
      int besti = last, bestSAD = (last == -1) ? MAX_UINT : pelList[last].getSAD(element, cs.sps->getBitDepths(), tmpCompBegin, tmpNumComp, lossless);
      if (lossless)
      {
        if (bestSAD)
        {
          for (int i = idx - 1; i >= 0; i--)
          {
            uint32_t sad = pelList[i].getSAD(element, cs.sps->getBitDepths(), tmpCompBegin, tmpNumComp, lossless);
            if (sad == 0)
            {
              bestSAD = sad;
              besti   = i;
              break;
            }
          }
        }
      }
      else
      {
        if (bestSAD)
        {
          for (int i = idx - 1; i >= 0; i--)
          {
            uint32_t sad = pelList[i].getSAD(element, cs.sps->getBitDepths(), tmpCompBegin, tmpNumComp, lossless);
            if (sad < bestSAD)
            {
              bestSAD = sad;
              besti   = i;
              if (!sad)
              {
                break;
              }
            }
          }
        }
      }
      if (besti >= 0 && pelList[besti].almostEqualData(element, errorLimit, cs.sps->getBitDepths(), tmpCompBegin, tmpNumComp, lossless))
      {
        pelList[besti].addElement(element, tmpCompBegin, tmpNumComp);
        last = besti;
      }
      else
      {
        pelList[idx].copyDataFrom(element, tmpCompBegin, tmpNumComp);
        for (int comp = tmpCompBegin; comp < (tmpCompBegin + tmpNumComp); comp++)
        {
          pelList[idx].setCnt(1, comp);
        }
        last = idx;
        idx++;
      }
    }
  }

  if (cs.sps->getChromaFormatIdc() != ChromaFormat::_444 && numComp == 3)
  {
    for( int i = 0; i < idx; i++ )
    {
      pelList[i].setCnt( pelList[i].getCnt(COMPONENT_Y) + (pelList[i].getCnt(COMPONENT_Cb) >> 2), MAX_NUM_COMPONENT);
    }
  }
  else
  {
    if( compBegin == 0 )
    {
      for( int i = 0; i < idx; i++ )
      {
        pelList[i].setCnt(pelList[i].getCnt(COMPONENT_Y), COMPONENT_Cb);
        pelList[i].setCnt(pelList[i].getCnt(COMPONENT_Y), COMPONENT_Cr);
        pelList[i].setCnt(pelList[i].getCnt(COMPONENT_Y), MAX_NUM_COMPONENT);
      }
    }
    else
    {
      for( int i = 0; i < idx; i++ )
      {
        pelList[i].setCnt(pelList[i].getCnt(COMPONENT_Cb), COMPONENT_Y);
        pelList[i].setCnt(pelList[i].getCnt(COMPONENT_Cb), MAX_NUM_COMPONENT);
      }
    }
  }

  for (int i = 0; i < dictMaxSize; i++)
  {
    pelListSort[i].setCnt(0, COMPONENT_Y);
    pelListSort[i].setCnt(0, COMPONENT_Cb);
    pelListSort[i].setCnt(0, COMPONENT_Cr);
    pelListSort[i].setCnt(0, MAX_NUM_COMPONENT);
    pelListSort[i].resetAll(compBegin, numComp);
  }

  //bubble sorting
  dictMaxSize = 1;
  for (int i = 0; i < idx; i++)
  {
    if( pelList[i].getCnt(MAX_NUM_COMPONENT) > pelListSort[dictMaxSize - 1].getCnt(MAX_NUM_COMPONENT) )
    {
      int j;
      for (j = dictMaxSize; j > 0; j--)
      {
        if (pelList[i].getCnt(MAX_NUM_COMPONENT) > pelListSort[j - 1].getCnt(MAX_NUM_COMPONENT))
        {
          pelListSort[j].copyAllFrom(pelListSort[j - 1], compBegin, numComp);
          dictMaxSize = std::min(dictMaxSize + 1, (uint32_t)maxPltSize);
        }
        else
        {
          break;
        }
      }
      pelListSort[j].copyAllFrom(pelList[i], compBegin, numComp);
    }
  }

  uint32_t paletteSize = 0;
  uint64_t numColorBits = 0;
  for (int comp = compBegin; comp < (compBegin + numComp); comp++)
  {
    numColorBits += (comp > 0) ? channelBitDepth_C : channelBitDepth_L;
  }
  const int plt_lambda_shift = (compBegin > 0) ? pcmShiftRight_C : pcmShiftRight_L;
  double    bitCost          = m_pcRdCost->getLambda() / (double) (1 << (2 * plt_lambda_shift)) * numColorBits;
  bool   reuseflag[MAXPLTPREDSIZE] = { false };
  int    run;
  double reuseflagCost;
  for (int i = 0; i < maxPltSize; i++)
  {
    if( pelListSort[i].getCnt(MAX_NUM_COMPONENT) )
    {
      ComponentID tmpCompBegin = compBegin;
      int tmpNumComp = numComp;
      if (cs.sps->getChromaFormatIdc() != ChromaFormat::_444 && numComp == 3
          && pelListSort[i].getCnt(COMPONENT_Cb) == 0)
      {
        tmpCompBegin = COMPONENT_Y;
        tmpNumComp   = 1;
      }

      for( int comp = tmpCompBegin; comp < (tmpCompBegin + tmpNumComp); comp++ )
      {
        int half = pelListSort[i].getCnt(comp) >> 1;
        cu.curPLT[comp][paletteSize] = (pelListSort[i].getSumData(comp) + half) / pelListSort[i].getCnt(comp);
      }

      int best = -1;
      if( errorLimit )
      {
        double pal[MAX_NUM_COMPONENT], err = 0.0, bestCost = 0.0;
        for( int comp = tmpCompBegin; comp < (tmpCompBegin + tmpNumComp); comp++ )
        {
          pal[comp] = pelListSort[i].getSumData(comp) / (double)pelListSort[i].getCnt(comp);
          err = pal[comp] - cu.curPLT[comp][paletteSize];
          if( isChroma((ComponentID) comp) )
          {
            bestCost += (err * err * PLT_CHROMA_WEIGHTING) / (1 << (2 * pcmShiftRight_C)) * pelListSort[i].getCnt(comp);
          }
          else
          {
            bestCost += (err * err) / (1 << (2 * pcmShiftRight_L)) * pelListSort[i].getCnt(comp);
          }
        }
        bestCost += bitCost;

        for( int t = 0; t < cs.prevPLT.curPLTSize[compBegin]; t++ )
        {
          double cost = 0.0;
          for( int comp = tmpCompBegin; comp < (tmpCompBegin + tmpNumComp); comp++ )
          {
            err = pal[comp] - cs.prevPLT.curPLT[comp][t];
            if( isChroma((ComponentID) comp) )
            {
              cost += (err * err * PLT_CHROMA_WEIGHTING) / (1 << (2 * pcmShiftRight_C)) * pelListSort[i].getCnt(comp);
            }
            else
            {
              cost += (err * err) / (1 << (2 * pcmShiftRight_L)) * pelListSort[i].getCnt(comp);
            }
          }
          run = 0;
          for (int t2 = t; t2 >= 0; t2--)
          {
            if (!reuseflag[t2])
            {
              run++;
            }
            else
            {
              break;
            }
          }
          reuseflagCost = m_pcRdCost->getLambda() / (double)(1 << (2 * plt_lambda_shift)) * getEpExGolombNumBins(run ? run + 1 : run, 0);
          cost += reuseflagCost;

          if( cost < bestCost )
          {
            best = t;
            bestCost = cost;
          }
        }
        if( best != -1 )
        {
          for( int comp = tmpCompBegin; comp < (tmpCompBegin + tmpNumComp); comp++ )
          {
            cu.curPLT[comp][paletteSize] = cs.prevPLT.curPLT[comp][best];
          }
          reuseflag[best] = true;
        }
      }

      bool duplicate = false;
      if( pelListSort[i].getCnt(MAX_NUM_COMPONENT) == 1 && best == -1 )
      {
        duplicate = true;
      }
      else
      {
        for( int t = 0; t < paletteSize; t++ )
        {
          bool duplicateTmp = true;
          for( int comp = tmpCompBegin; comp < (tmpCompBegin + tmpNumComp); comp++ )
          {
            duplicateTmp = duplicateTmp && (cu.curPLT[comp][paletteSize] == cu.curPLT[comp][t]);
          }
          if( duplicateTmp )
          {
            duplicate = true;
            break;
          }
        }
      }
      if( !duplicate )
      {
        if (cs.sps->getChromaFormatIdc() != ChromaFormat::_444 && numComp == 3
            && pelListSort[i].getCnt(COMPONENT_Cb) == 0)
        {
          if( best != -1 )
          {
            cu.curPLT[COMPONENT_Cb][paletteSize] = cs.prevPLT.curPLT[COMPONENT_Cb][best];
            cu.curPLT[COMPONENT_Cr][paletteSize] = cs.prevPLT.curPLT[COMPONENT_Cr][best];
          }
          else
          {
            cu.curPLT[COMPONENT_Cb][paletteSize] = 1 << (channelBitDepth_C - 1);
            cu.curPLT[COMPONENT_Cr][paletteSize] = 1 << (channelBitDepth_C - 1);
          }
        }
        paletteSize++;
      }
    }
    else
    {
      break;
    }
  }
  cu.curPLTSize[compBegin] = paletteSize;
  if( cu.isLocalSepTree() )
  {
    cu.curPLTSize[COMPONENT_Y] = paletteSize;
  }

  delete[] pelList;
  delete[] pelListSort;
}
// -------------------------------------------------------------------------------------------------------------------
// Intra search
// -------------------------------------------------------------------------------------------------------------------

void IntraSearch::xEncIntraHeader(CodingStructure &cs, Partitioner &partitioner, const bool &hasLuma,
                                  const bool &hasChroma, const int subTuIdx)
{
  CodingUnit &cu = *cs.getCU( partitioner.chType );

  if (hasLuma)
  {
    bool isFirst = cu.ispMode != ISPType::NONE ? subTuIdx == 0 : partitioner.currArea().lumaPos() == cs.area.lumaPos();

    // CU header
    if( isFirst )
    {
      if ((!cs.slice->isIntra() || cs.slice->getSPS()->getIBCFlag() || cs.slice->getSPS()->getPLTMode())
          && cu.Y().valid())
      {
        m_CABACEstimator->cu_skip_flag( cu );
        m_CABACEstimator->pred_mode   ( cu );
      }
      if (CU::isPLT(cu))
      {
        return;
      }
    }

    PredictionUnit &pu = *cs.getPU(partitioner.currArea().lumaPos(), partitioner.chType);

    // luma prediction mode
    if (isFirst)
    {
      if ( !cu.Y().valid())
      {
        m_CABACEstimator->pred_mode( cu );
      }
      m_CABACEstimator->bdpcm_mode( cu, COMPONENT_Y );
      m_CABACEstimator->intra_luma_pred_mode( pu );
    }
  }

  if (hasChroma)
  {
    bool isFirst = partitioner.currArea().Cb().valid() && partitioner.currArea().chromaPos() == cs.area.chromaPos();

    PredictionUnit &pu = *cs.getPU(partitioner.currArea().chromaPos(), ChannelType::CHROMA);

    if( isFirst )
    {
      m_CABACEstimator->bdpcm_mode(cu, ComponentID(ChannelType::CHROMA));
      m_CABACEstimator->intra_chroma_pred_mode( pu );
    }
  }
}

void IntraSearch::xEncSubdivCbfQT(CodingStructure &cs, Partitioner &partitioner, const bool &hasLuma,
                                  const bool &hasChroma, const int subTuIdx, const PartSplit ispType)
{
  const UnitArea &currArea = partitioner.currArea();
          int subTuCounter = subTuIdx;
          TransformUnit &currTU       = *cs.getTU(currArea.block(partitioner.chType), partitioner.chType, subTuCounter);
          CodingUnit    &currCU       = *currTU.cu;
          uint32_t       currDepth    = partitioner.currTrDepth;

          const bool  subdiv = currTU.depth > currDepth;
          ComponentID compID = isLuma(partitioner.chType) ? COMPONENT_Y : COMPONENT_Cb;

          if (partitioner.canSplit(TU_MAX_TR_SPLIT, cs))
          {
            CHECK(!subdiv, "TU split implied");
          }
  else
  {
    CHECK(subdiv && currCU.ispMode == ISPType::NONE && isLuma(compID), "No TU subdivision is allowed with QTBT");
  }

  if (hasChroma)
  {
    const bool chromaCbfISP = currArea.blocks[COMPONENT_Cb].valid() && currCU.ispMode != ISPType::NONE && !subdiv;
    if (currCU.ispMode == ISPType::NONE || chromaCbfISP)
    {
      const uint32_t numberValidComponents = getNumberValidComponents(currArea.chromaFormat);
      const uint32_t cbfDepth              = (chromaCbfISP ? currDepth - 1 : currDepth);

      for (uint32_t ch = COMPONENT_Cb; ch < numberValidComponents; ch++)
      {
        const ComponentID compID = ComponentID(ch);

        if (currDepth == 0 || TU::getCbfAtDepth(currTU, compID, currDepth - 1) || chromaCbfISP)
        {
          const bool prevCbf = (compID == COMPONENT_Cr ? TU::getCbfAtDepth(currTU, COMPONENT_Cb, currDepth) : false);
          m_CABACEstimator->cbf_comp(TU::getCbfAtDepth(currTU, compID, currDepth), currArea.blocks[compID], cbfDepth,
                                     prevCbf, false, currCU.getBdpcmMode(compID));
        }
      }
    }
  }

  if (subdiv)
  {
    if( partitioner.canSplit( TU_MAX_TR_SPLIT, cs ) )
    {
      partitioner.splitCurrArea( TU_MAX_TR_SPLIT, cs );
    }
    else if (currCU.ispMode != ISPType::NONE && isLuma(compID))
    {
      partitioner.splitCurrArea( ispType, cs );
    }
    else
    {
      THROW("Cannot perform an implicit split!");
    }

    do
    {
      xEncSubdivCbfQT(cs, partitioner, hasLuma, hasChroma, subTuCounter, ispType);
      subTuCounter += subTuCounter != -1 ? 1 : 0;
    } while( partitioner.nextPart( cs ) );

    partitioner.exitCurrSplit();
  }
  else
  {
    //===== Cbfs =====
    if (hasLuma)
    {
      bool previousCbf       = false;
      bool lastCbfIsInferred = false;
      if( ispType != TU_NO_ISP )
      {
        bool rootCbfSoFar = false;

        const uint32_t nTus = currCU.ispMode == ISPType::HOR ? currCU.lheight() >> floorLog2(currTU.lheight())
                                                             : currCU.lwidth() >> floorLog2(currTU.lwidth());
        if( subTuCounter == nTus - 1 )
        {
          TransformUnit* tuPointer = currCU.firstTU;
          for( int tuIdx = 0; tuIdx < nTus - 1; tuIdx++ )
          {
            rootCbfSoFar |= TU::getCbfAtDepth( *tuPointer, COMPONENT_Y, currDepth );
            tuPointer = tuPointer->next;
          }
          if( !rootCbfSoFar )
          {
            lastCbfIsInferred = true;
          }
        }
        if( !lastCbfIsInferred )
        {
          previousCbf = TU::getPrevTuCbfAtDepth( currTU, COMPONENT_Y, partitioner.currTrDepth );
        }
      }
      if( !lastCbfIsInferred )
      {
        m_CABACEstimator->cbf_comp(TU::getCbfAtDepth(currTU, COMPONENT_Y, currDepth), currTU.Y(), currTU.depth,
                                   previousCbf, currCU.ispMode != ISPType::NONE, currCU.getBdpcmMode(COMPONENT_Y));
      }
    }
  }
}

void IntraSearch::xEncCoeffQT( CodingStructure &cs, Partitioner &partitioner, const ComponentID compID, const int subTuIdx, const PartSplit ispType, CUCtx* cuCtx )
{
  const UnitArea &currArea  = partitioner.currArea();

  int            subTuCounter = subTuIdx;
  TransformUnit &currTU       = *cs.getTU(currArea.block(partitioner.chType), partitioner.chType, subTuIdx);
  uint32_t       currDepth    = partitioner.currTrDepth;
  const bool     subdiv       = currTU.depth > currDepth;

  if (subdiv)
  {
    if (partitioner.canSplit(TU_MAX_TR_SPLIT, cs))
    {
      partitioner.splitCurrArea(TU_MAX_TR_SPLIT, cs);
    }
    else if (currTU.cu->ispMode != ISPType::NONE)
    {
      partitioner.splitCurrArea( ispType, cs );
    }
    else
    {
      THROW("Implicit TU split not available!");
    }

    do
    {
      xEncCoeffQT( cs, partitioner, compID, subTuCounter, ispType, cuCtx );
      subTuCounter += subTuCounter != -1 ? 1 : 0;
    } while( partitioner.nextPart( cs ) );

    partitioner.exitCurrSplit();
  }
  else
  {
    if (currArea.blocks[compID].valid())
    {
      if (compID == COMPONENT_Cr)
      {
        const int cbfMask =
          (TU::getCbf(currTU, COMPONENT_Cb) ? CBF_MASK_CB : 0) + (TU::getCbf(currTU, COMPONENT_Cr) ? CBF_MASK_CR : 0);
        m_CABACEstimator->joint_cb_cr(currTU, cbfMask);
      }
      if (TU::getCbf(currTU, compID))
      {
        if (isLuma(compID))
        {
          m_CABACEstimator->residual_coding(currTU, compID, cuCtx);
          m_CABACEstimator->mts_idx(*currTU.cu, cuCtx);
        }
        else
        {
          m_CABACEstimator->residual_coding(currTU, compID);
        }
      }
    }
  }
}

uint64_t IntraSearch::xGetIntraFracBitsQT(CodingStructure &cs, Partitioner &partitioner, const bool &hasLuma,
                                          const bool &hasChroma, const int subTuIdx, const PartSplit ispType,
                                          CUCtx *cuCtx)
{
  m_CABACEstimator->resetBits();

  xEncIntraHeader(cs, partitioner, hasLuma, hasChroma, subTuIdx);
  xEncSubdivCbfQT(cs, partitioner, hasLuma, hasChroma, subTuIdx, ispType);

  if (hasLuma)
  {
    xEncCoeffQT( cs, partitioner, COMPONENT_Y, subTuIdx, ispType, cuCtx );
  }
  if (hasChroma)
  {
    xEncCoeffQT( cs, partitioner, COMPONENT_Cb, subTuIdx, ispType );
    xEncCoeffQT( cs, partitioner, COMPONENT_Cr, subTuIdx, ispType );
  }

  CodingUnit& cu = *cs.getCU(partitioner.chType);
  if (cuCtx && hasLuma && cu.isSepTree()
      && (cu.ispMode == ISPType::NONE || (cu.lfnstIdx && subTuIdx == 0)
          || (!cu.lfnstIdx && subTuIdx == m_ispTestedModes[cu.lfnstIdx].numTotalParts[cu.ispMode] - 1)))
  {
    m_CABACEstimator->residual_lfnst_mode(cu, *cuCtx);
  }

  uint64_t fracBits = m_CABACEstimator->getEstFracBits();
  return fracBits;
}

uint64_t IntraSearch::xGetIntraFracBitsQTSingleChromaComponent( CodingStructure &cs, Partitioner &partitioner, const ComponentID compID )
{
  m_CABACEstimator->resetBits();

  if( compID == COMPONENT_Cb )
  {
    //intra mode coding
    PredictionUnit &pu = *cs.getPU( partitioner.currArea().lumaPos(), partitioner.chType );
    m_CABACEstimator->intra_chroma_pred_mode( pu );
    //xEncIntraHeader(cs, partitioner, false, true);
  }
  CHECK( partitioner.currTrDepth != 1, "error in the depth!" );
  const UnitArea &currArea = partitioner.currArea();

  TransformUnit &currTU = *cs.getTU(currArea.block(partitioner.chType), partitioner.chType);

  //cbf coding
  const bool prevCbf = ( compID == COMPONENT_Cr ? TU::getCbfAtDepth( currTU, COMPONENT_Cb, partitioner.currTrDepth ) : false );
  m_CABACEstimator->cbf_comp(TU::getCbfAtDepth(currTU, compID, partitioner.currTrDepth), currArea.blocks[compID],
                             partitioner.currTrDepth - 1, prevCbf, false, currTU.cu->getBdpcmMode(compID));
  //coeffs coding and cross comp coding
  if( TU::getCbf( currTU, compID ) )
  {
    m_CABACEstimator->residual_coding( currTU, compID );
  }

  uint64_t fracBits = m_CABACEstimator->getEstFracBits();
  return fracBits;
}

uint64_t IntraSearch::xGetIntraFracBitsQTChroma(TransformUnit& currTU, const ComponentID &compID)
{
  m_CABACEstimator->resetBits();
  // Include Cbf and jointCbCr flags here as we make decisions across components

  if ( currTU.jointCbCr )
  {
    const bool cbfMaskCb = TU::getCbf(currTU, COMPONENT_Cb);
    const bool cbfMaskCr = TU::getCbf(currTU, COMPONENT_Cr);
    const int  cbfMask   = (cbfMaskCb ? CBF_MASK_CB : 0) + (cbfMaskCr ? CBF_MASK_CR : 0);

    m_CABACEstimator->cbf_comp(cbfMaskCb, currTU.blocks[COMPONENT_Cb], currTU.depth, false, false,
                               currTU.cu->getBdpcmMode(COMPONENT_Cb));
    m_CABACEstimator->cbf_comp(cbfMaskCr, currTU.blocks[COMPONENT_Cr], currTU.depth, cbfMaskCb, false,
                               currTU.cu->getBdpcmMode(COMPONENT_Cr));

    if (cbfMask != 0)
    {
      m_CABACEstimator->joint_cb_cr( currTU, cbfMask );
    }
    if (cbfMaskCb)
    {
      m_CABACEstimator->residual_coding( currTU, COMPONENT_Cb );
    }
    if (cbfMaskCr)
    {
      m_CABACEstimator->residual_coding( currTU, COMPONENT_Cr );
    }
  }
  else
  {
    if ( compID == COMPONENT_Cb )
    {
      m_CABACEstimator->cbf_comp(TU::getCbf(currTU, compID), currTU.blocks[compID], currTU.depth, false, false,
                                 currTU.cu->getBdpcmMode(compID));
    }
    else
    {
      const bool cbCbf    = TU::getCbf( currTU, COMPONENT_Cb );
      const bool crCbf    = TU::getCbf( currTU, compID );
      const int  cbfMask  = (cbCbf ? CBF_MASK_CB : 0) + (crCbf ? CBF_MASK_CR : 0);
      m_CABACEstimator->cbf_comp(crCbf, currTU.blocks[compID], currTU.depth, cbCbf, false,
                                 currTU.cu->getBdpcmMode(compID));
      m_CABACEstimator->joint_cb_cr( currTU, cbfMask );
    }
  }

  if( !currTU.jointCbCr && TU::getCbf( currTU, compID ) )
  {
    m_CABACEstimator->residual_coding( currTU, compID );
  }

  uint64_t fracBits = m_CABACEstimator->getEstFracBits();
  return fracBits;
}

void IntraSearch::xIntraCodingTUBlock(TransformUnit &tu, const ComponentID &compID, Distortion &dist,
                                      const int &default0Save1Load2, uint32_t *numSig, TrModeList *trModes,
                                      const bool loadTr)
{
  if (!tu.blocks[compID].valid())
  {
    return;
  }

  CodingStructure &cs                       = *tu.cs;
  m_pcRdCost->setChromaFormat(cs.sps->getChromaFormatIdc());

  const CompArea      &area                 = tu.blocks[compID];
  const SPS           &sps                  = *cs.sps;

  const ChannelType    chType               = toChannelType(compID);
  const int            bitDepth             = sps.getBitDepth(chType);

  PelBuf         piOrg                      = cs.getOrgBuf    (area);
  PelBuf         piPred                     = cs.getPredBuf   (area);
  PelBuf         piResi                     = cs.getResiBuf   (area);
  PelBuf         piReco                     = cs.getRecoBuf   (area);

  const PredictionUnit &pu          = *cs.getPU(area.pos(), chType);
  const uint32_t        chFinalMode = PU::getFinalIntraMode(pu, chType);

  //===== init availability pattern =====
  CHECK( tu.jointCbCr && compID == COMPONENT_Cr, "wrong combination of compID and jointCbCr" );
  bool jointCbCr = tu.jointCbCr && compID == COMPONENT_Cb;

  if (compID == COMPONENT_Y)
  {
    PelBuf sharedPredTS( m_pSharedPredTransformSkip[compID], area );
    if( default0Save1Load2 != 2 )
    {
      bool predRegDiffFromTB = CU::isPredRegDiffFromTB(*tu.cu, compID);
      bool firstTBInPredReg = CU::isFirstTBInPredReg(*tu.cu, compID, area);
      CompArea areaPredReg(COMPONENT_Y, tu.chromaFormat, area);
      if (tu.cu->ispMode != ISPType::NONE && isLuma(compID))
      {
        if (predRegDiffFromTB)
        {
          if (firstTBInPredReg)
          {
            CU::adjustPredArea(areaPredReg);
            initIntraPatternChTypeISP(*tu.cu, areaPredReg, piReco);
          }
        }
        else
        {
          initIntraPatternChTypeISP(*tu.cu, area, piReco);
        }
      }
      else
      {
        initIntraPatternChType(*tu.cu, area);
      }

      //===== get prediction signal =====
      if (compID != COMPONENT_Y && tu.cu->bdpcmModeChroma == BdpcmMode::NONE && PU::isLMCMode(chFinalMode))
      {
        xGetLumaRecPixels( pu, area );
        predIntraChromaLM(compID, piPred, pu, area, chFinalMode);
      }
      else
      {
        if( PU::isMIP( pu, chType ) )
        {
          initIntraMip( pu, area );
          predIntraMip( compID, piPred, pu );
        }
        else
        {
          if (predRegDiffFromTB)
          {
            if (firstTBInPredReg)
            {
              PelBuf piPredReg = cs.getPredBuf(areaPredReg);
              predIntraAng(compID, piPredReg, pu);
            }
          }
          else
          {
            predIntraAng(compID, piPred, pu);
          }
        }
      }

      // save prediction
      if( default0Save1Load2 == 1 )
      {
        sharedPredTS.copyFrom( piPred );
      }
    }
    else
    {
      // load prediction
      piPred.copyFrom( sharedPredTS );
    }
  }

  DTRACE(g_trace_ctx, D_PRED, "@(%4d,%4d) [%2dx%2d] IMode=%d\n", tu.lx(), tu.ly(), tu.lwidth(), tu.lheight(),
         chFinalMode);
  //DTRACE_PEL_BUF( D_PRED, piPred, tu, tu.cu->predMode, COMPONENT_Y );

  const Slice           &slice = *cs.slice;
  bool flag = slice.getLmcsEnabledFlag() && (slice.isIntra() || (!slice.isIntra() && m_pcReshape->getCTUFlag()));
  if (isLuma(compID))
  {
    //===== get residual signal =====
    piResi.copyFrom( piOrg  );
    if (slice.getLmcsEnabledFlag() && m_pcReshape->getCTUFlag() && compID == COMPONENT_Y)
    {
      CompArea      tmpArea(COMPONENT_Y, area.chromaFormat, Position(0, 0), area.size());
      PelBuf        tmpPred = m_tmpStorageCtu.getBuf(tmpArea);
      tmpPred.copyFrom(piPred);
      piResi.rspSignal(m_pcReshape->getFwdLUT());
      piResi.subtract(tmpPred);
    }
    else
    {
      piResi.subtract( piPred );
    }
  }

  //===== transform and quantization =====
  //--- init rate estimation arrays for RDOQ ---
  //--- transform and quantization           ---
  TCoeff absSum = 0;

  const QpParam cQP(tu, compID);

#if RDOQ_CHROMA_LAMBDA
  m_pcTrQuant->selectLambda(compID);
#endif

  flag =flag && (tu.blocks[compID].width*tu.blocks[compID].height > 4);
  if (flag && isChroma(compID) && slice.getPicHeader()->getLmcsChromaResidualScaleFlag() )
  {
    int cResScaleInv = tu.getChromaAdj();
    double cResScale = (double)(1 << CSCALE_FP_PREC) / (double)cResScaleInv;
    m_pcTrQuant->setLambda(m_pcTrQuant->getLambda() / (cResScale*cResScale));
  }

  PelBuf          crOrg;
  PelBuf          crPred;
  PelBuf          crResi;
  PelBuf          crReco;

  if (isChroma(compID))
  {
    const CompArea &crArea = tu.blocks[ COMPONENT_Cr ];
    crOrg  = cs.getOrgBuf  ( crArea );
    crPred = cs.getPredBuf ( crArea );
    crResi = cs.getResiBuf ( crArea );
    crReco = cs.getRecoBuf ( crArea );
  }

  if ( jointCbCr )
  {
    // Lambda is loosened for the joint mode with respect to single modes as the same residual is used for both chroma blocks
    const int    absIct = abs( TU::getICTMode(tu) );
    const double lfact  = ( absIct == 1 || absIct == 3 ? 0.8 : 0.5 );
    m_pcTrQuant->setLambda( lfact * m_pcTrQuant->getLambda() );
  }
  if ( sps.getJointCbCrEnabledFlag() && isChroma(compID) && (tu.cu->cs->slice->getSliceQp() > 18) )
  {
    m_pcTrQuant->setLambda( 1.3 * m_pcTrQuant->getLambda() );
  }

  if( isLuma(compID) )
  {
    if (trModes)
    {
      m_pcTrQuant->transformNxN(tu, compID, cQP, *trModes, m_pcEncCfg->getMTSIntraMaxCand());
      tu.mtsIdx[compID] = trModes->at(0).first;
    }
    if (!(m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless()
          && tu.mtsIdx[compID] == MtsType::DCT2_DCT2)
        || tu.cu->bdpcmMode != BdpcmMode::NONE)
    {
      m_pcTrQuant->transformNxN(tu, compID, cQP, absSum, m_CABACEstimator->getCtx(), loadTr);
    }

    DTRACE(g_trace_ctx, D_TU_ABS_SUM, "%d: comp=%d, abssum=%d\n", DTRACE_GET_COUNTER(g_trace_ctx, D_TU_ABS_SUM), compID,
           absSum);

    if (tu.cu->ispMode != ISPType::NONE && isLuma(compID) && CU::isISPLast(*tu.cu, area, area.compID)
        && CU::allLumaCBFsAreZero(*tu.cu))
    {
      // ISP has to have at least one non-zero CBF
      dist = MAX_INT;
      return;
    }
    if (m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless()
        && tu.mtsIdx[compID] == MtsType::DCT2_DCT2 && BdpcmMode::NONE == tu.cu->bdpcmMode)
    {
      absSum = 0;
      tu.getCoeffs(compID).fill(0);
      TU::setCbfAtDepth(tu, compID, tu.depth, 0);
    }

    //--- inverse transform ---
    if (absSum > 0)
    {
      m_pcTrQuant->invTransformNxN(tu, compID, piResi, cQP);
    }
    else
    {
      piResi.fill(0);
    }
  }
  else // chroma
  {
    ComponentID codeCompId =
      tu.jointCbCr != 0 ? ((tu.jointCbCr & CBF_MASK_CB) != 0 ? COMPONENT_Cb : COMPONENT_Cr) : compID;

    const QpParam qpCbCr(tu, codeCompId);

    if( tu.jointCbCr )
    {
      ComponentID otherCompId = ( codeCompId==COMPONENT_Cr ? COMPONENT_Cb : COMPONENT_Cr );
      tu.getCoeffs( otherCompId ).fill(0); // do we need that?
      TU::setCbfAtDepth (tu, otherCompId, tu.depth, false );
    }
    PelBuf& codeResi = ( codeCompId == COMPONENT_Cr ? crResi : piResi );
    absSum           = 0;

    if (trModes)
    {
      m_pcTrQuant->transformNxN(tu, codeCompId, qpCbCr, *trModes, m_pcEncCfg->getMTSIntraMaxCand());
      tu.mtsIdx[codeCompId] = trModes->at(0).first;
      if (tu.jointCbCr)
      {
        tu.mtsIdx[(codeCompId == COMPONENT_Cr) ? COMPONENT_Cb : COMPONENT_Cr] = MtsType::DCT2_DCT2;
      }
    }
    // encoder bugfix: Set loadTr to aovid redundant transform process
    if (!(m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless()
          && tu.mtsIdx[compID] == MtsType::DCT2_DCT2)
        || tu.cu->bdpcmModeChroma != BdpcmMode::NONE)
    {
      m_pcTrQuant->transformNxN(tu, codeCompId, qpCbCr, absSum, m_CABACEstimator->getCtx(), loadTr);
    }
    if ((m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless()
         && tu.mtsIdx[compID] == MtsType::DCT2_DCT2)
        && BdpcmMode::NONE == tu.cu->bdpcmModeChroma)
    {
      absSum = 0;
      tu.getCoeffs(compID).fill(0);
      TU::setCbfAtDepth(tu, compID, tu.depth, 0);
    }

    DTRACE(g_trace_ctx, D_TU_ABS_SUM, "%d: comp=%d, abssum=%d\n", DTRACE_GET_COUNTER(g_trace_ctx, D_TU_ABS_SUM),
           codeCompId, absSum);

    int codedCbfMask = 0;

    if (absSum > 0)
    {
      m_pcTrQuant->invTransformNxN(tu, codeCompId, codeResi, qpCbCr);
      codedCbfMask += codeCompId == COMPONENT_Cb ? CBF_MASK_CB : CBF_MASK_CR;
    }
    else
    {
      codeResi.fill(0);
    }

    if( tu.jointCbCr )
    {
      if (tu.jointCbCr == 3 && codedCbfMask == CBF_MASK_CB)
      {
        codedCbfMask = CBF_MASK_CBCR;
        TU::setCbfAtDepth (tu, COMPONENT_Cr, tu.depth, true );
      }
      if( tu.jointCbCr != codedCbfMask )
      {
        dist = std::numeric_limits<Distortion>::max();
        return;
      }
      m_pcTrQuant->invTransformICT( tu, piResi, crResi );
      absSum = codedCbfMask;
    }
  }

  //===== reconstruction =====
  if (flag && absSum > 0 && isChroma(compID) && slice.getPicHeader()->getLmcsChromaResidualScaleFlag())
  {
    piResi.scaleSignal(tu.getChromaAdj(), 0, tu.cu->cs->slice->clpRng(compID));
    if( jointCbCr )
    {
      crResi.scaleSignal(tu.getChromaAdj(), 0, tu.cu->cs->slice->clpRng(COMPONENT_Cr));
    }
  }

  if (slice.getLmcsEnabledFlag() && m_pcReshape->getCTUFlag() && compID == COMPONENT_Y)
  {
    CompArea      tmpArea(COMPONENT_Y, area.chromaFormat, Position(0,0), area.size());
    PelBuf        tmpPred = m_tmpStorageCtu.getBuf(tmpArea);
    tmpPred.copyFrom(piPred);
    piReco.reconstruct(tmpPred, piResi, cs.slice->clpRng(compID));
  }
  else
  {
    piReco.reconstruct(piPred, piResi, cs.slice->clpRng( compID ));
    if( jointCbCr )
    {
      crReco.reconstruct(crPred, crResi, cs.slice->clpRng( COMPONENT_Cr ));
    }
  }


  //===== update distortion =====
#if WCG_EXT
  if (m_pcEncCfg->getLumaLevelToDeltaQPMapping().isEnabled() || (m_pcEncCfg->getLmcs()
    && slice.getLmcsEnabledFlag() && (m_pcReshape->getCTUFlag() || (isChroma(compID) && m_pcEncCfg->getReshapeIntraCMD()))))
  {
    const CPelBuf orgLuma = cs.getOrgBuf( cs.area.blocks[COMPONENT_Y] );
    if (compID == COMPONENT_Y  && !(m_pcEncCfg->getLumaLevelToDeltaQPMapping().isEnabled()))
    {
      CompArea tmpArea1(COMPONENT_Y, area.chromaFormat, Position(0, 0), area.size());
      PelBuf   tmpRecLuma = m_tmpStorageCtu.getBuf(tmpArea1);
      tmpRecLuma.copyFrom(piReco);
      tmpRecLuma.rspSignal(m_pcReshape->getInvLUT());
      dist += m_pcRdCost->getDistPart(piOrg, tmpRecLuma, sps.getBitDepth(toChannelType(compID)), compID, DFuncWtd::SSE_WTD,
                                      orgLuma);
    }
    else
    {
      dist += m_pcRdCost->getDistPart(piOrg, piReco, bitDepth, compID, DFuncWtd::SSE_WTD, orgLuma);
      if( jointCbCr )
      {
        dist += m_pcRdCost->getDistPart(crOrg, crReco, bitDepth, COMPONENT_Cr, DFuncWtd::SSE_WTD, orgLuma);
      }
    }
  }
  else
#endif
  {
    dist += m_pcRdCost->getDistPart(piOrg, piReco, bitDepth, compID, DFunc::SSE);
    if( jointCbCr )
    {
      dist += m_pcRdCost->getDistPart(crOrg, crReco, bitDepth, COMPONENT_Cr, DFunc::SSE);
    }
  }
}

void IntraSearch::xIntraCodingACTTUBlock(TransformUnit &tu, const ComponentID &compID, Distortion &dist,
                                         TrModeList *trModes, const bool loadTr)
{
  if (!tu.blocks[compID].valid())
  {
    THROW("tu does not exist");
  }

  CodingStructure     &cs = *tu.cs;
  const SPS           &sps = *cs.sps;
  const Slice         &slice = *cs.slice;
  const CompArea      &area = tu.blocks[compID];
  const CompArea &crArea = tu.blocks[COMPONENT_Cr];

  PelBuf piOrgResi = cs.getOrgResiBuf(area);
  PelBuf piResi    = cs.getResiBuf(area);
  PelBuf crOrgResi = cs.getOrgResiBuf(crArea);
  PelBuf crResi    = cs.getResiBuf(crArea);
  TCoeff absSum    = 0;

  CHECK(tu.jointCbCr && compID == COMPONENT_Cr, "wrong combination of compID and jointCbCr");
  bool jointCbCr = tu.jointCbCr && compID == COMPONENT_Cb;

  m_pcRdCost->setChromaFormat(cs.sps->getChromaFormatIdc());
  if (m_pcEncCfg->getCostMode() != COST_LOSSLESS_CODING || !slice.isLossless())
  m_pcTrQuant->lambdaAdjustColorTrans(true);

  if (jointCbCr)
  {
    ComponentID compIdCode = (tu.jointCbCr >> 1 ? COMPONENT_Cb : COMPONENT_Cr);
    m_pcTrQuant->selectLambda(compIdCode);
  }
  else
  {
    m_pcTrQuant->selectLambda(compID);
  }

  bool flag = slice.getLmcsEnabledFlag() && (slice.isIntra() || (!slice.isIntra() && m_pcReshape->getCTUFlag())) && (tu.blocks[compID].width*tu.blocks[compID].height > 4);
  if (flag && isChroma(compID) && slice.getPicHeader()->getLmcsChromaResidualScaleFlag())
  {
    int    cResScaleInv = tu.getChromaAdj();
    double cResScale = (double)(1 << CSCALE_FP_PREC) / (double)cResScaleInv;
    m_pcTrQuant->setLambda(m_pcTrQuant->getLambda() / (cResScale*cResScale));
  }

  if (jointCbCr)
  {
    // Lambda is loosened for the joint mode with respect to single modes as the same residual is used for both chroma blocks
    const int    absIct = abs(TU::getICTMode(tu));
    const double lfact = (absIct == 1 || absIct == 3 ? 0.8 : 0.5);
    m_pcTrQuant->setLambda(lfact * m_pcTrQuant->getLambda());
  }
  if (sps.getJointCbCrEnabledFlag() && isChroma(compID) && (slice.getSliceQp() > 18))
  {
    m_pcTrQuant->setLambda(1.3 * m_pcTrQuant->getLambda());
  }

  if (isLuma(compID))
  {
    QpParam cQP(tu, compID);

    if (trModes)
    {
      m_pcTrQuant->transformNxN(tu, compID, cQP, *trModes, m_pcEncCfg->getMTSIntraMaxCand());
      tu.mtsIdx[compID] = trModes->at(0).first;
    }
    if (!(m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless()
          && tu.mtsIdx[compID] == MtsType::DCT2_DCT2)
        || tu.cu->bdpcmMode != BdpcmMode::NONE)
    {
      m_pcTrQuant->transformNxN(tu, compID, cQP, absSum, m_CABACEstimator->getCtx(), loadTr);
    }
    if ((m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless()
         && tu.mtsIdx[compID] == MtsType::DCT2_DCT2)
        && tu.cu->bdpcmMode == BdpcmMode::NONE)
    {
      absSum = 0;
      tu.getCoeffs(compID).fill(0);
      TU::setCbfAtDepth(tu, compID, tu.depth, 0);
    }

    if (absSum > 0)
    {
      m_pcTrQuant->invTransformNxN(tu, compID, piResi, cQP);
    }
    else
    {
      piResi.fill(0);
    }
  }
  else
  {
    int         codedCbfMask = 0;
    ComponentID codeCompId = (tu.jointCbCr ? (tu.jointCbCr >> 1 ? COMPONENT_Cb : COMPONENT_Cr) : compID);
    QpParam qpCbCr(tu, codeCompId);

    if (tu.jointCbCr)
    {
      ComponentID otherCompId = (codeCompId == COMPONENT_Cr ? COMPONENT_Cb : COMPONENT_Cr);
      tu.getCoeffs(otherCompId).fill(0);
      TU::setCbfAtDepth(tu, otherCompId, tu.depth, false);
    }

    PelBuf& codeResi = (codeCompId == COMPONENT_Cr ? crResi : piResi);
    absSum           = 0;
    if (trModes)
    {
      m_pcTrQuant->transformNxN(tu, codeCompId, qpCbCr, *trModes, m_pcEncCfg->getMTSIntraMaxCand());
      tu.mtsIdx[codeCompId] = trModes->at(0).first;
      if (tu.jointCbCr)
      {
        tu.mtsIdx[(codeCompId == COMPONENT_Cr) ? COMPONENT_Cb : COMPONENT_Cr] = MtsType::DCT2_DCT2;
      }
    }
    if (!(m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless()
          && tu.mtsIdx[codeCompId] == MtsType::DCT2_DCT2)
        || tu.cu->bdpcmModeChroma != BdpcmMode::NONE)
    {
      m_pcTrQuant->transformNxN(tu, codeCompId, qpCbCr, absSum, m_CABACEstimator->getCtx(), loadTr);
    }
    if (absSum > 0)
    {
      m_pcTrQuant->invTransformNxN(tu, codeCompId, codeResi, qpCbCr);
      codedCbfMask += codeCompId == COMPONENT_Cb ? CBF_MASK_CB : CBF_MASK_CR;
    }
    else
    {
      codeResi.fill(0);
    }

    if (tu.jointCbCr)
    {
      if (tu.jointCbCr == 3 && codedCbfMask == CBF_MASK_CB)
      {
        codedCbfMask = CBF_MASK_CBCR;
        TU::setCbfAtDepth(tu, COMPONENT_Cr, tu.depth, true);
      }
      if (tu.jointCbCr != codedCbfMask)
      {
        dist = std::numeric_limits<Distortion>::max();
        if (m_pcEncCfg->getCostMode() != COST_LOSSLESS_CODING || !slice.isLossless())
        m_pcTrQuant->lambdaAdjustColorTrans(false);
        return;
      }
      m_pcTrQuant->invTransformICT(tu, piResi, crResi);
      absSum = codedCbfMask;
    }
  }

  if (m_pcEncCfg->getCostMode() != COST_LOSSLESS_CODING || !slice.isLossless())
  {
    m_pcTrQuant->lambdaAdjustColorTrans(false);
  }

  dist += m_pcRdCost->getDistPart(piOrgResi, piResi, sps.getBitDepth(toChannelType(compID)), compID, DFunc::SSE);
  if (jointCbCr)
  {
    dist += m_pcRdCost->getDistPart(crOrgResi, crResi, sps.getBitDepth(toChannelType(COMPONENT_Cr)), COMPONENT_Cr,
                                    DFunc::SSE);
  }
}

bool IntraSearch::xIntraCodingLumaISP(CodingStructure& cs, Partitioner& partitioner, const double bestCostSoFar)
{
  int               subTuCounter = 0;
  const CodingUnit& cu = *cs.getCU(partitioner.currArea().lumaPos(), partitioner.chType);
  bool              earlySkipISP = false;
  bool              splitCbfLuma = false;
  const PartSplit   ispType = CU::getISPType(cu, COMPONENT_Y);

  cs.cost = 0;

  partitioner.splitCurrArea(ispType, cs);

  CUCtx cuCtx;
  cuCtx.isDQPCoded = true;
  cuCtx.isChromaQpAdjCoded = true;

  do   // subpartitions loop
  {
    uint32_t   numSig = 0;
    Distortion singleDistTmpLuma = 0;
    uint64_t   singleTmpFracBits = 0;
    double     singleCostTmp = 0;

    TransformUnit& tu = cs.addTU(CS::getArea(cs, partitioner.currArea(), partitioner.chType), partitioner.chType);
    tu.depth = partitioner.currTrDepth;

    // Encode TU
    xIntraCodingTUBlock(tu, COMPONENT_Y, singleDistTmpLuma, 0, &numSig);
    if (singleDistTmpLuma == MAX_INT)   // all zero CBF skip
    {
      earlySkipISP = true;
      partitioner.exitCurrSplit();
      cs.cost = MAX_DOUBLE;
      return false;
    }

    if (m_pcRdCost->calcRdCost(cs.fracBits, cs.dist + singleDistTmpLuma) > bestCostSoFar)
    {
      // The accumulated cost + distortion is already larger than the best cost so far, so it is not necessary to
      // calculate the rate
      earlySkipISP = true;
    }
    else
    {
      singleTmpFracBits = xGetIntraFracBitsQT(cs, partitioner, true, false, subTuCounter, ispType, &cuCtx);
    }
    singleCostTmp = m_pcRdCost->calcRdCost(singleTmpFracBits, singleDistTmpLuma);

    cs.cost += singleCostTmp;
    cs.dist += singleDistTmpLuma;
    cs.fracBits += singleTmpFracBits;

    subTuCounter++;

    splitCbfLuma |= TU::getCbfAtDepth(*cs.getTU(partitioner.currArea().lumaPos(), partitioner.chType, subTuCounter - 1), COMPONENT_Y, partitioner.currTrDepth);
    int nSubPartitions = m_ispTestedModes[cu.lfnstIdx].numTotalParts[cu.ispMode];
    if (subTuCounter < nSubPartitions)
    {
      // exit condition if the accumulated cost is already larger than the best cost so far (no impact in RD performance)
      if (cs.cost > bestCostSoFar)
      {
        earlySkipISP = true;
        break;
      }
      else if (subTuCounter < nSubPartitions)
      {
        // more restrictive exit condition
        double threshold = nSubPartitions == 2 ? 0.95 : subTuCounter == 1 ? 0.83 : 0.91;
        if (subTuCounter < nSubPartitions && cs.cost > bestCostSoFar * threshold)
        {
          earlySkipISP = true;
          break;
        }
      }
    }
  } while (partitioner.nextPart(cs));   // subpartitions loop

  partitioner.exitCurrSplit();
  const UnitArea& currArea = partitioner.currArea();
  const uint32_t  currDepth = partitioner.currTrDepth;

  if (earlySkipISP)
  {
    cs.cost = MAX_DOUBLE;
  }
  else
  {
    cs.cost = m_pcRdCost->calcRdCost(cs.fracBits, cs.dist);
    // The cost check is necessary here again to avoid superfluous operations if the maximum number of coded subpartitions was reached and yet ISP did not win
    if (cs.cost < bestCostSoFar)
    {
      cs.setDecomp(cu.Y());
      cs.picture->getRecoBuf(currArea.Y()).copyFrom(cs.getRecoBuf(currArea.Y()));

      for (auto& ptu : cs.tus)
      {
        if (currArea.Y().contains(ptu->Y()))
        {
          TU::setCbfAtDepth(*ptu, COMPONENT_Y, currDepth, splitCbfLuma ? 1 : 0);
        }
      }
    }
    else
    {
      earlySkipISP = true;
    }
  }
  return !earlySkipISP;
}

bool IntraSearch::xRecurIntraCodingLumaQT( CodingStructure &cs, Partitioner &partitioner, bool mtsCheckRangeFlag, int mtsFirstCheckId, int mtsLastCheckId, bool moreProbMTSIdxFirst )
{
  const UnitArea &currArea = partitioner.currArea();
  const CodingUnit     &cu = *cs.getCU( currArea.lumaPos(), partitioner.chType );
  uint32_t currDepth       = partitioner.currTrDepth;
  const SPS &sps           = *cs.sps;

  bool checkFull  = !partitioner.canSplit(TU_MAX_TR_SPLIT, cs);
  bool checkSplit = partitioner.canSplit(TU_MAX_TR_SPLIT, cs);

  const Slice &slice = *cs.slice;

  CHECK(cu.ispMode != ISPType::NONE, "Use the function xIntraCodingLumaISP for ISP cases.");

  uint32_t numSig = 0;

  double     singleCost                         = MAX_DOUBLE;
  Distortion singleDistLuma                     = 0;
  uint64_t   singleFracBits                     = 0;
  bool       checkTransformSkip                 = sps.getTransformSkipEnabledFlag();
  uint8_t    nNumTransformCands                 = cu.mtsFlag ? 4 : 1;
  uint8_t    numTransformIndexCands             = nNumTransformCands;

  std::array<int, MAX_NUM_COMPONENT> bestModeIds;
  bestModeIds.fill(0);

  const TempCtx ctxStart(m_ctxPool, m_CABACEstimator->getCtx());
  TempCtx       ctxBest(m_ctxPool);

  CodingStructure *csSplit = nullptr;
  CodingStructure *csFull  = nullptr;

  CUCtx cuCtx;
  cuCtx.isDQPCoded = true;
  cuCtx.isChromaQpAdjCoded = true;

  if (checkSplit)
  {
    csSplit = &cs;
  }
  else if (checkFull)
  {
    csFull = &cs;
  }

  bool validReturnFull = false;

  if (checkFull)
  {
    csFull->cost = 0.0;

    TransformUnit &tu = csFull->addTU( CS::getArea( *csFull, currArea, partitioner.chType ), partitioner.chType );
    tu.depth = currDepth;

    const bool tsAllowed  = TU::isTSAllowed( tu, COMPONENT_Y );
    const bool mtsAllowed = CU::isMTSAllowed( cu, COMPONENT_Y );
    TrModeList trModes;

    if( sps.getUseLFNST() )
    {
      checkTransformSkip &= tsAllowed;
      checkTransformSkip &= !cu.mtsFlag;
      checkTransformSkip &= !cu.lfnstIdx;

      if( !cu.mtsFlag && checkTransformSkip )
      {
        trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));   // DCT2
        trModes.push_back(TrMode(MtsType::SKIP, true));        // TS
      }
    }
    else
    {
      nNumTransformCands = 1 + ( tsAllowed ? 1 : 0 ) + ( mtsAllowed ? 4 : 0 ); // DCT + TS + 4 MTS = 6 tests
      if (m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless())
      {
        nNumTransformCands = 1;
        CHECK(!tsAllowed && cu.bdpcmMode == BdpcmMode::NONE, "transform skip should be enabled for LS");
        if (cu.bdpcmMode != BdpcmMode::NONE)
        {
          trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));
        }
        else
        {
          trModes.push_back(TrMode(MtsType::SKIP, true));
        }
      }
      else
      {
        trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));   // DCT2
        if (tsAllowed)
        {
          trModes.push_back(TrMode(MtsType::SKIP, true));
        }
        if (mtsAllowed)
        {
          for (MtsType mtsIdx = MtsType::DST7_DST7; mtsIdx < MtsType::NUM; mtsIdx++)
          {
            trModes.push_back(TrMode(mtsIdx, true));
          }
        }
      }
    }

    CHECK( !tu.Y().valid(), "Invalid TU" );

    CodingStructure &saveCS = *m_pSaveCS[0];

    TransformUnit *tmpTU = nullptr;

    Distortion singleDistTmpLuma = 0;
    uint64_t   singleTmpFracBits = 0;
    double     singleCostTmp     = 0;
    int        firstCheckId      = (sps.getUseLFNST() && mtsCheckRangeFlag && cu.mtsFlag) ? mtsFirstCheckId : 0;

    //we add the MTS candidates to the loop. TransformSkip will still be the last one to be checked (when modeId == lastCheckId) as long as checkTransformSkip is true
    int  lastCheckId             = sps.getUseLFNST() ? ((mtsCheckRangeFlag && cu.mtsFlag)
                                                          ? (mtsLastCheckId + (int) checkTransformSkip)
                                                          : (numTransformIndexCands - (firstCheckId + 1) + (int) checkTransformSkip))
                                                     : trModes[nNumTransformCands - 1].first - MtsType::DCT2_DCT2;
    bool isNotOnlyOneMode        = sps.getUseLFNST() ? lastCheckId != firstCheckId : nNumTransformCands != 1;

    if( isNotOnlyOneMode )
    {
      saveCS.pcv     = cs.pcv;
      saveCS.picture = cs.picture;
      saveCS.sps     = cs.sps;
      saveCS.area.repositionTo(cs.area);
      saveCS.clearTUs();
      tmpTU = &saveCS.addTU(currArea, partitioner.chType);
    }

    bool cbfBestMode      = false;
    bool cbfBestModeValid = false;
    bool cbfDCT2          = true;

    for( int modeId = firstCheckId; modeId <= ( sps.getUseLFNST() ? lastCheckId : ( nNumTransformCands - 1 ) ); modeId++ )
    {
      int transformIndex = modeId;

      if( sps.getUseLFNST() )
      {
        if( ( transformIndex < lastCheckId ) || ( ( transformIndex == lastCheckId ) && !checkTransformSkip ) ) //we avoid this if the mode is transformSkip
        {
          // Skip checking other transform candidates if zero CBF is encountered and it is the best transform so far
          if( m_pcEncCfg->getUseFastLFNST() && transformIndex && !cbfBestMode && cbfBestModeValid )
          {
            continue;
          }
        }
      }
      else
      {
        if (!(m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless()))
        {
          if (!cbfDCT2
              || (m_pcEncCfg->getUseTransformSkipFast()
                  && MtsType::DCT2_DCT2 + bestModeIds[COMPONENT_Y] == MtsType::SKIP))
          {
            break;
          }
          if (!trModes[modeId].second)
          {
            continue;
          }
        }
        tu.mtsIdx[COMPONENT_Y] = trModes[modeId].first;
      }


      if ((modeId != firstCheckId) && isNotOnlyOneMode)
      {
        m_CABACEstimator->getCtx() = ctxStart;
      }

      int default0Save1Load2 = 0;
      singleDistTmpLuma = 0;

      if( modeId == firstCheckId && ( sps.getUseLFNST() ? ( modeId != lastCheckId ) : ( nNumTransformCands > 1 ) ) )
      {
        default0Save1Load2 = 1;
      }
      else if (modeId != firstCheckId)
      {
        if( sps.getUseLFNST() && !cbfBestModeValid )
        {
          default0Save1Load2 = 1;
        }
        else
        {
          default0Save1Load2 = 2;
        }
      }
      if( sps.getUseLFNST() )
      {
        if( cu.mtsFlag )
        {
          if( moreProbMTSIdxFirst )
          {
            const ChannelType     chType      = toChannelType( COMPONENT_Y );
            const CompArea&       area        = tu.blocks[ COMPONENT_Y ];
            const PredictionUnit& pu          = *cs.getPU( area.pos(), chType );
            uint32_t              intraMode   = pu.intraDir[chType];

            if( transformIndex == 1 )
            {
              tu.mtsIdx[COMPONENT_Y] = (intraMode < DIA_IDX) ? MtsType::DST7_DCT8 : MtsType::DCT8_DST7;
            }
            else if( transformIndex == 2 )
            {
              tu.mtsIdx[COMPONENT_Y] = (intraMode < DIA_IDX) ? MtsType::DCT8_DST7 : MtsType::DST7_DCT8;
            }
            else
            {
              tu.mtsIdx[COMPONENT_Y] = MtsType::DST7_DST7 + transformIndex;
            }
          }
          else
          {
            tu.mtsIdx[COMPONENT_Y] = MtsType::DST7_DST7 + transformIndex;
          }
        }
        else
        {
          tu.mtsIdx[COMPONENT_Y] = MtsType::DCT2_DCT2 + transformIndex;
        }

        if( !cu.mtsFlag && checkTransformSkip )
        {
          xIntraCodingTUBlock( tu, COMPONENT_Y, singleDistTmpLuma, default0Save1Load2, &numSig, modeId == 0 ? &trModes : nullptr, true );
          if( modeId == 0 )
          {
            for( int i = 0; i < 2; i++ )
            {
              if( trModes[ i ].second )
              {
                lastCheckId = trModes[i].first - MtsType::DCT2_DCT2;
              }
            }
          }
        }
        else
        {
          xIntraCodingTUBlock( tu, COMPONENT_Y, singleDistTmpLuma, default0Save1Load2, &numSig );
        }
      }
      else
      {
        if( nNumTransformCands > 1 )
        {
          xIntraCodingTUBlock( tu, COMPONENT_Y, singleDistTmpLuma, default0Save1Load2, &numSig, modeId == 0 ? &trModes : nullptr, true );
          if( modeId == 0 )
          {
            for( int i = 0; i < nNumTransformCands; i++ )
            {
              if( trModes[ i ].second )
              {
                lastCheckId = trModes[i].first - MtsType::DCT2_DCT2;
              }
            }
          }
        }
        else
        {
          xIntraCodingTUBlock( tu, COMPONENT_Y, singleDistTmpLuma, default0Save1Load2, &numSig );
        }
      }

      cuCtx.mtsLastScanPos = false;
      cuCtx.violatesMtsCoeffConstraint = false;
      //----- determine rate and r-d cost -----
      if ((sps.getUseLFNST() ? (modeId == lastCheckId && modeId != 0 && checkTransformSkip)
                             : (trModes[modeId].first != MtsType::DCT2_DCT2))
          && !TU::getCbfAtDepth(tu, COMPONENT_Y, currDepth))
      {
        //In order not to code TS flag when cbf is zero, the case for TS with cbf being zero is forbidden.
        if (m_pcEncCfg->getCostMode() != COST_LOSSLESS_CODING || !slice.isLossless())
        {
          singleCostTmp = MAX_DOUBLE;
        }
        else
        {
          singleTmpFracBits = xGetIntraFracBitsQT(*csFull, partitioner, true, false, -1, TU_NO_ISP, &cuCtx);
          singleCostTmp = m_pcRdCost->calcRdCost(singleTmpFracBits, singleDistTmpLuma);
        }
      }
      else
      {
        singleTmpFracBits = xGetIntraFracBitsQT(*csFull, partitioner, true, false, -1, TU_NO_ISP, &cuCtx);
        if (tu.mtsIdx[COMPONENT_Y] > MtsType::SKIP)
        {
          if (!cuCtx.mtsLastScanPos)
          {
            singleCostTmp = MAX_DOUBLE;
          }
          else
          {
            singleCostTmp = m_pcRdCost->calcRdCost(singleTmpFracBits, singleDistTmpLuma);
          }
        }
        else
        {
          singleCostTmp = m_pcRdCost->calcRdCost(singleTmpFracBits, singleDistTmpLuma);
        }
      }

      if (singleCostTmp < singleCost)
      {
        singleCost     = singleCostTmp;
        singleDistLuma = singleDistTmpLuma;
        singleFracBits = singleTmpFracBits;

        if( sps.getUseLFNST() )
        {
          bestModeIds[COMPONENT_Y] = modeId;
          cbfBestMode = TU::getCbfAtDepth( tu, COMPONENT_Y, currDepth );
          cbfBestModeValid = true;
          validReturnFull = true;
        }
        else
        {
          bestModeIds[COMPONENT_Y] = trModes[modeId].first - MtsType::DCT2_DCT2;
          if (trModes[modeId].first == MtsType::DCT2_DCT2)
          {
            cbfDCT2 = TU::getCbfAtDepth( tu, COMPONENT_Y, currDepth );
          }
        }

        if (bestModeIds[COMPONENT_Y] != lastCheckId)
        {
          saveCS.getPredBuf( tu.Y() ).copyFrom( csFull->getPredBuf( tu.Y() ) );
          saveCS.getRecoBuf( tu.Y() ).copyFrom( csFull->getRecoBuf( tu.Y() ) );

          if( KEEP_PRED_AND_RESI_SIGNALS )
          {
            saveCS.getResiBuf   ( tu.Y() ).copyFrom( csFull->getResiBuf   ( tu.Y() ) );
            saveCS.getOrgResiBuf( tu.Y() ).copyFrom( csFull->getOrgResiBuf( tu.Y() ) );
          }

          tmpTU->copyComponentFrom( tu, COMPONENT_Y );

          ctxBest = m_CABACEstimator->getCtx();
        }
      }
    }

    if( sps.getUseLFNST() && !validReturnFull )
    {
      csFull->cost = MAX_DOUBLE;

      if (checkSplit)
      {
        ctxBest = m_CABACEstimator->getCtx();
      }
    }
    else
    {
      if (bestModeIds[COMPONENT_Y] != lastCheckId)
      {
        csFull->getPredBuf( tu.Y() ).copyFrom( saveCS.getPredBuf( tu.Y() ) );
        csFull->getRecoBuf( tu.Y() ).copyFrom( saveCS.getRecoBuf( tu.Y() ) );

        if( KEEP_PRED_AND_RESI_SIGNALS )
        {
          csFull->getResiBuf   ( tu.Y() ).copyFrom( saveCS.getResiBuf   ( tu.Y() ) );
          csFull->getOrgResiBuf( tu.Y() ).copyFrom( saveCS.getOrgResiBuf( tu.Y() ) );
        }

        tu.copyComponentFrom( *tmpTU, COMPONENT_Y );

        if (!checkSplit)
        {
          m_CABACEstimator->getCtx() = ctxBest;
        }
      }
      else if (checkSplit)
      {
        ctxBest = m_CABACEstimator->getCtx();
      }

      csFull->cost += singleCost;
      csFull->dist += singleDistLuma;
      csFull->fracBits += singleFracBits;
    }
  }

  bool validReturnSplit = false;
  if (checkSplit)
  {
    //----- store full entropy coding status, load original entropy coding status -----
    if (checkFull)
    {
      m_CABACEstimator->getCtx() = ctxStart;
    }
    //----- code splitted block -----
    csSplit->cost = 0;

    bool splitCbfLuma    = false;
    bool splitIsSelected = true;
    if( partitioner.canSplit( TU_MAX_TR_SPLIT, cs ) )
    {
      partitioner.splitCurrArea( TU_MAX_TR_SPLIT, cs );
    }

    do
    {
      bool tmpValidReturnSplit = xRecurIntraCodingLumaQT( *csSplit, partitioner, false, mtsCheckRangeFlag, mtsFirstCheckId, mtsLastCheckId );
      if( sps.getUseLFNST() && !tmpValidReturnSplit )
      {
        splitIsSelected = false;
        break;
      }

      csSplit->setDecomp(partitioner.currArea().Y());

      splitCbfLuma |= TU::getCbfAtDepth(*csSplit->getTU(partitioner.currArea().lumaPos(), partitioner.chType, -1),
                                        COMPONENT_Y, partitioner.currTrDepth);

    } while( partitioner.nextPart( *csSplit ) );

    partitioner.exitCurrSplit();

    if( splitIsSelected )
    {
      for( auto &ptu : csSplit->tus )
      {
        if( currArea.Y().contains( ptu->Y() ) )
        {
          TU::setCbfAtDepth(*ptu, COMPONENT_Y, currDepth, splitCbfLuma ? 1 : 0);
        }
      }

      //----- restore context states -----
      m_CABACEstimator->getCtx() = ctxStart;

      cuCtx.violatesLfnstConstrained.fill(false);
      cuCtx.lfnstLastScanPos = false;
      cuCtx.violatesMtsCoeffConstraint = false;
      cuCtx.mtsLastScanPos = false;

      //----- determine rate and r-d cost -----
      csSplit->fracBits = xGetIntraFracBitsQT( *csSplit, partitioner, true, false, -1, TU_NO_ISP, &cuCtx );

      //--- update cost ---
      csSplit->cost     = m_pcRdCost->calcRdCost(csSplit->fracBits, csSplit->dist);

      validReturnSplit = true;
    }
  }

  bool retVal = false;
  if( csFull || csSplit )
  {
    if( !sps.getUseLFNST() || validReturnFull || validReturnSplit )
    {
      // otherwise this would've happened in useSubStructure
      cs.picture->getRecoBuf(currArea.Y()).copyFrom(cs.getRecoBuf(currArea.Y()));
      cs.picture->getPredBuf(currArea.Y()).copyFrom(cs.getPredBuf(currArea.Y()));
      cs.cost = m_pcRdCost->calcRdCost(cs.fracBits, cs.dist);
      retVal = true;
    }
  }
  return retVal;
}

bool IntraSearch::xRecurIntraCodingACTQT(CodingStructure &cs, Partitioner &partitioner, bool mtsCheckRangeFlag, int mtsFirstCheckId, int mtsLastCheckId, bool moreProbMTSIdxFirst)
{
  const UnitArea &currArea = partitioner.currArea();
  uint32_t       currDepth = partitioner.currTrDepth;
  const Slice    &slice = *cs.slice;
  const SPS      &sps = *cs.sps;

  bool checkFull  = !partitioner.canSplit(TU_MAX_TR_SPLIT, cs);
  bool checkSplit = !checkFull;

  TempCtx ctxStart(m_ctxPool, m_CABACEstimator->getCtx());
  TempCtx ctxBest(m_ctxPool);

  CodingStructure *csSplit = nullptr;
  CodingStructure *csFull = nullptr;
  if (checkSplit)
  {
    csSplit = &cs;
  }
  else if (checkFull)
  {
    csFull = &cs;
  }

  bool validReturnFull = false;

  if (checkFull)
  {
    TransformUnit        &tu = csFull->addTU(CS::getArea(*csFull, currArea, partitioner.chType), partitioner.chType);
    tu.depth = currDepth;
    const CodingUnit     &cu = *csFull->getCU(tu.Y().pos(), ChannelType::LUMA);
    const PredictionUnit &pu = *csFull->getPU(tu.Y().pos(), ChannelType::LUMA);
    CHECK(!tu.Y().valid() || !tu.Cb().valid() || !tu.Cr().valid(), "Invalid TU");
    CHECK(tu.cu != &cu, "wrong CU fetch");
    CHECK(cu.ispMode != ISPType::NONE, "adaptive color transform cannot be applied to ISP");
    CHECK(pu.intraDir[ChannelType::CHROMA] != DM_CHROMA_IDX, "chroma should use DM mode for adaptive color transform");

    // 1. intra prediction and forward color transform

    PelUnitBuf orgBuf = csFull->getOrgBuf(tu);
    PelUnitBuf predBuf = csFull->getPredBuf(tu);
    PelUnitBuf resiBuf = csFull->getResiBuf(tu);
    PelUnitBuf orgResiBuf = csFull->getOrgResiBuf(tu);
    bool doReshaping = (slice.getLmcsEnabledFlag() && slice.getPicHeader()->getLmcsChromaResidualScaleFlag() && (slice.isIntra() || m_pcReshape->getCTUFlag()) && (tu.blocks[COMPONENT_Cb].width * tu.blocks[COMPONENT_Cb].height > 4));
    if (doReshaping)
    {
      const Area area =
        tu.Y().valid() ? tu.Y()
                       : Area(recalcPosition(tu.chromaFormat, tu.chType, ChannelType::LUMA, tu.block(tu.chType).pos()),
                              recalcSize(tu.chromaFormat, tu.chType, ChannelType::LUMA, tu.block(tu.chType).size()));
      const CompArea &areaY = CompArea(COMPONENT_Y, tu.chromaFormat, area);
      int             adj = m_pcReshape->calculateChromaAdjVpduNei(tu, areaY);
      tu.setChromaAdj(adj);
    }

    for (int i = 0; i < getNumberValidComponents(tu.chromaFormat); i++)
    {
      ComponentID          compID = (ComponentID)i;
      const CompArea       &area = tu.blocks[compID];
      const ChannelType    chType = toChannelType(compID);

      PelBuf         piOrg = orgBuf.bufs[compID];
      PelBuf         piPred = predBuf.bufs[compID];
      PelBuf         piResi = resiBuf.bufs[compID];

      initIntraPatternChType(*tu.cu, area);
      if (PU::isMIP(pu, chType))
      {
        initIntraMip(pu, area);
        predIntraMip(compID, piPred, pu);
      }
      else
      {
        predIntraAng(compID, piPred, pu);
      }

      piResi.copyFrom(piOrg);
      if (slice.getLmcsEnabledFlag() && m_pcReshape->getCTUFlag() && compID == COMPONENT_Y)
      {
        CompArea tmpArea(COMPONENT_Y, area.chromaFormat, Position(0, 0), area.size());
        PelBuf   tmpPred = m_tmpStorageCtu.getBuf(tmpArea);
        tmpPred.copyFrom(piPred);
        piResi.rspSignal(m_pcReshape->getFwdLUT());
        piResi.subtract(tmpPred);
      }
      else if (doReshaping && (compID != COMPONENT_Y))
      {
        piResi.subtract(piPred);
        int cResScaleInv = tu.getChromaAdj();
        piResi.scaleSignal(cResScaleInv, 1, slice.clpRng(compID));
      }
      else
      {
        piResi.subtract(piPred);
      }
    }

    resiBuf.colorSpaceConvert(orgResiBuf, true, cs.slice->clpRng(COMPONENT_Y));

    // 2. luma residual optimization
    double  singleCostLuma         = MAX_DOUBLE;
    bool    checkTransformSkip     = sps.getTransformSkipEnabledFlag();
    int     bestLumaModeId         = 0;
    uint8_t nNumTransformCands     = cu.mtsFlag ? 4 : 1;
    uint8_t numTransformIndexCands = nNumTransformCands;

    const bool tsAllowed = TU::isTSAllowed(tu, COMPONENT_Y);
    const bool mtsAllowed = CU::isMTSAllowed(cu, COMPONENT_Y);
    const bool lossless = m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless();
    TrModeList trModes;

    if (sps.getUseLFNST())
    {
      checkTransformSkip &= tsAllowed;
      checkTransformSkip &= !cu.mtsFlag;
      checkTransformSkip &= !cu.lfnstIdx;

      if (!cu.mtsFlag && checkTransformSkip)
      {
        trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));   // DCT2
        trModes.push_back(TrMode(MtsType::SKIP, true));        // TS
      }
    }
    else
    {
      if (lossless)
      {
        nNumTransformCands = 1;
        CHECK(!tsAllowed && cu.bdpcmMode == BdpcmMode::NONE, "transform skip should be enabled for LS");
        if (cu.bdpcmMode != BdpcmMode::NONE)
        {
          trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));
        }
        else
        {
          trModes.push_back(TrMode(MtsType::SKIP, true));
        }
      }
      else
      {
        nNumTransformCands = 1 + (tsAllowed ? 1 : 0) + (mtsAllowed ? 4 : 0);   // DCT + TS + 4 MTS = 6 tests

        trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));   // DCT2
        if (tsAllowed)
        {
          trModes.push_back(TrMode(MtsType::SKIP, true));
        }
        if (mtsAllowed)
        {
          for (int i = 2; i < 6; i++)
          {
            trModes.push_back(TrMode(MtsType(i), true));
          }
        }
      }
    }

    CodingStructure &saveLumaCS = *m_pSaveCS[0];
    TransformUnit   *tmpTU = nullptr;
    Distortion      singleDistTmpLuma = 0;
    uint64_t        singleTmpFracBits = 0;
    double          singleCostTmp = 0;
    int             firstCheckId = (sps.getUseLFNST() && mtsCheckRangeFlag && cu.mtsFlag) ? mtsFirstCheckId : 0;
    int              lastCheckId       = sps.getUseLFNST() ? ((mtsCheckRangeFlag && cu.mtsFlag)
                                                                ? (mtsLastCheckId + (int) checkTransformSkip)
                                                                : (numTransformIndexCands - (firstCheckId + 1) + (int) checkTransformSkip))
                                                           : trModes[nNumTransformCands - 1].first - MtsType::DCT2_DCT2;
    bool            isNotOnlyOneMode = sps.getUseLFNST() ? lastCheckId != firstCheckId : nNumTransformCands != 1;

    if (isNotOnlyOneMode)
    {
      saveLumaCS.pcv = csFull->pcv;
      saveLumaCS.picture = csFull->picture;
      saveLumaCS.sps = csFull->sps;
      saveLumaCS.area.repositionTo(csFull->area);
      saveLumaCS.clearTUs();
      tmpTU = &saveLumaCS.addTU(currArea, partitioner.chType);
    }

    bool cbfBestMode      = false;
    bool cbfBestModeValid = false;
    bool cbfDCT2          = true;

    if (m_pcEncCfg->getCostMode() != COST_LOSSLESS_CODING || !slice.isLossless())
    {
      m_pcRdCost->lambdaAdjustColorTrans(true, COMPONENT_Y);
    }

    for (int modeIndex = firstCheckId; sps.getUseLFNST() || modeIndex < trModes.size(); modeIndex++)
    {
      const int modeId = sps.getUseLFNST() ? modeIndex : trModes[modeIndex].first - MtsType::DCT2_DCT2;
      if (modeId > lastCheckId)
      {
        break;
      }
      uint8_t transformIndex = modeId;
      csFull->getResiBuf(tu.Y()).copyFrom(csFull->getOrgResiBuf(tu.Y()));

      m_CABACEstimator->getCtx() = ctxStart;
      m_CABACEstimator->resetBits();

      if (sps.getUseLFNST())
      {
        if ((transformIndex < lastCheckId) || ((transformIndex == lastCheckId) && !checkTransformSkip)) //we avoid this if the mode is transformSkip
        {
          // Skip checking other transform candidates if zero CBF is encountered and it is the best transform so far
          if (m_pcEncCfg->getUseFastLFNST() && transformIndex && !cbfBestMode && cbfBestModeValid)
          {
            continue;
          }
        }
      }
      else
      {
        if (!lossless)
        {
          if (!cbfDCT2 || (m_pcEncCfg->getUseTransformSkipFast() && bestLumaModeId == 1))
          {
            break;
          }
          if (!trModes[modeIndex].second)
          {
            continue;
          }
        }
        tu.mtsIdx[COMPONENT_Y] = MtsType::DCT2_DCT2 + modeId;
      }

      singleDistTmpLuma = 0;
      if (sps.getUseLFNST())
      {
        if (cu.mtsFlag)
        {
          if (moreProbMTSIdxFirst)
          {
            const uint32_t intraMode = pu.intraDir[ChannelType::LUMA];

            if (transformIndex == 1)
            {
              tu.mtsIdx[COMPONENT_Y] = (intraMode < DIA_IDX) ? MtsType::DST7_DCT8 : MtsType::DCT8_DST7;
            }
            else if (transformIndex == 2)
            {
              tu.mtsIdx[COMPONENT_Y] = (intraMode < DIA_IDX) ? MtsType::DCT8_DST7 : MtsType::DST7_DCT8;
            }
            else
            {
              tu.mtsIdx[COMPONENT_Y] = MtsType::DST7_DST7 + transformIndex;
            }
          }
          else
          {
            tu.mtsIdx[COMPONENT_Y] = MtsType::DST7_DST7 + transformIndex;
          }
        }
        else
        {
          tu.mtsIdx[COMPONENT_Y] = MtsType::DCT2_DCT2 + transformIndex;
        }

        if (!cu.mtsFlag && checkTransformSkip)
        {
          xIntraCodingACTTUBlock(tu, COMPONENT_Y, singleDistTmpLuma, modeId == 0 ? &trModes : nullptr, true);
          if (modeId == 0)
          {
            for (int i = 0; i < 2; i++)
            {
              if (trModes[i].second)
              {
                lastCheckId = trModes[i].first - MtsType::DCT2_DCT2;
              }
            }
          }
        }
        else
        {
          xIntraCodingACTTUBlock(tu, COMPONENT_Y, singleDistTmpLuma);
        }
      }
      else
      {
        if (nNumTransformCands > 1)
        {
          xIntraCodingACTTUBlock(tu, COMPONENT_Y, singleDistTmpLuma, modeId == 0 ? &trModes : nullptr, true);
          if (modeId == 0)
          {
            for (int i = 0; i < nNumTransformCands; i++)
            {
              if (trModes[i].second)
              {
                lastCheckId = trModes[i].first - MtsType::DCT2_DCT2;
              }
            }
          }
        }
        else
        {
          xIntraCodingACTTUBlock(tu, COMPONENT_Y, singleDistTmpLuma);
        }
      }

      CUCtx cuCtx;
      cuCtx.isDQPCoded = true;
      cuCtx.isChromaQpAdjCoded = true;
      //----- determine rate and r-d cost -----
      if ((sps.getUseLFNST() ? (modeId == lastCheckId && modeId != 0 && checkTransformSkip) : (modeId != 0)) && !TU::getCbfAtDepth(tu, COMPONENT_Y, currDepth))
      {
        //In order not to code TS flag when cbf is zero, the case for TS with cbf being zero is forbidden.
        if (m_pcEncCfg->getCostMode() != COST_LOSSLESS_CODING || !slice.isLossless())
        singleCostTmp = MAX_DOUBLE;
        else
        {
          singleTmpFracBits = xGetIntraFracBitsQT(*csFull, partitioner, true, false, -1, TU_NO_ISP);
          singleCostTmp = m_pcRdCost->calcRdCost(singleTmpFracBits, singleDistTmpLuma, false);
        }
      }
      else
      {
        singleTmpFracBits = xGetIntraFracBitsQT(*csFull, partitioner, true, false, -1, TU_NO_ISP, &cuCtx);

        if (tu.mtsIdx[COMPONENT_Y] > MtsType::SKIP)
        {
          if (!cuCtx.mtsLastScanPos)
          {
            singleCostTmp = MAX_DOUBLE;
          }
          else
          {
            singleCostTmp = m_pcRdCost->calcRdCost(singleTmpFracBits, singleDistTmpLuma, false);
          }
        }
        else
        {
          singleCostTmp = m_pcRdCost->calcRdCost(singleTmpFracBits, singleDistTmpLuma, false);
        }
      }

      if (singleCostTmp < singleCostLuma)
      {
        singleCostLuma  = singleCostTmp;
        validReturnFull = true;

        if (sps.getUseLFNST())
        {
          bestLumaModeId = modeId;
          cbfBestMode = TU::getCbfAtDepth(tu, COMPONENT_Y, currDepth);
          cbfBestModeValid = true;
        }
        else
        {
          bestLumaModeId = modeId;
          if (modeId == 0)
          {
            cbfDCT2 = TU::getCbfAtDepth(tu, COMPONENT_Y, currDepth);
          }
        }

        if (bestLumaModeId != lastCheckId)
        {
          saveLumaCS.getResiBuf(tu.Y()).copyFrom(csFull->getResiBuf(tu.Y()));
          tmpTU->copyComponentFrom(tu, COMPONENT_Y);
          ctxBest = m_CABACEstimator->getCtx();
        }
      }
    }
    if (m_pcEncCfg->getCostMode() != COST_LOSSLESS_CODING || !slice.isLossless())
    {
      m_pcRdCost->lambdaAdjustColorTrans(false, COMPONENT_Y);
    }

    if (sps.getUseLFNST())
    {
      if (!validReturnFull)
      {
        csFull->cost = MAX_DOUBLE;
        return false;
      }
    }
    else
    {
      CHECK(!validReturnFull, "no transform mode was tested for luma");
    }

    csFull->setDecomp(currArea.Y(), true);
    csFull->setDecomp(currArea.Cb(), true);

    if (bestLumaModeId != lastCheckId)
    {
      csFull->getResiBuf(tu.Y()).copyFrom(saveLumaCS.getResiBuf(tu.Y()));
      tu.copyComponentFrom(*tmpTU, COMPONENT_Y);
      m_CABACEstimator->getCtx() = ctxBest;
    }

    // 3 chroma residual optimization
    CodingStructure &saveChromaCS = *m_pSaveCS[1];
    saveChromaCS.pcv = csFull->pcv;
    saveChromaCS.picture = csFull->picture;
    saveChromaCS.sps = csFull->sps;
    saveChromaCS.area.repositionTo(csFull->area);
    saveChromaCS.initStructData(MAX_INT, true);
    tmpTU = &saveChromaCS.addTU(currArea, partitioner.chType);

    CompArea&  cbArea = tu.blocks[COMPONENT_Cb];
    CompArea&  crArea = tu.blocks[COMPONENT_Cr];

    tu.jointCbCr = 0;

    CompStorage  orgResiCb[5], orgResiCr[5]; // 0:std, 1-3:jointCbCr (placeholder at this stage), 4:crossComp
    orgResiCb[0].create(cbArea);
    orgResiCr[0].create(crArea);
    orgResiCb[0].copyFrom(csFull->getOrgResiBuf(cbArea));
    orgResiCr[0].copyFrom(csFull->getOrgResiBuf(crArea));

    // 3.1 regular chroma residual coding
    csFull->getResiBuf(cbArea).copyFrom(orgResiCb[0]);
    csFull->getResiBuf(crArea).copyFrom(orgResiCr[0]);

    for (uint32_t c = COMPONENT_Cb; c < ::getNumberValidTBlocks(*csFull->pcv); c++)
    {
      const ComponentID compID = ComponentID(c);

      double  singleBestCostChroma = MAX_DOUBLE;
      int     bestModeId           = -1;
      bool    tsAllowed            = TU::isTSAllowed(tu, compID) && (m_pcEncCfg->getUseChromaTS()) && !cu.lfnstIdx;
      uint8_t numTransformCands    = 1 + (tsAllowed ? 1 : 0);   // DCT + TS = 2 tests
      bool    cbfDCT2              = true;

      trModes.clear();
      if (lossless)
      {
        numTransformCands = 1;
        CHECK(!tsAllowed && cu.bdpcmModeChroma == BdpcmMode::NONE, "transform skip should be enabled for LS");
        if (cu.bdpcmModeChroma != BdpcmMode::NONE)
        {
          trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));
        }
        else
        {
          trModes.push_back(TrMode(MtsType::SKIP, true));
        }
      }
      else
      {
        trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));   // DCT
        if (tsAllowed)
        {
          trModes.push_back(TrMode(MtsType::SKIP, true));   // TS
        }
      }
      if (m_pcEncCfg->getCostMode() != COST_LOSSLESS_CODING || !slice.isLossless())
      {
        if (doReshaping)
        {
          int cResScaleInv = tu.getChromaAdj();
          m_pcRdCost->lambdaAdjustColorTrans(true, compID, true, &cResScaleInv);
        }
        else
        {
          m_pcRdCost->lambdaAdjustColorTrans(true, compID);
        }
      }

      TempCtx ctxBegin(m_ctxPool);
      ctxBegin = m_CABACEstimator->getCtx();

      for (int modeId = 0; modeId < numTransformCands; modeId++)
      {
        if (m_pcEncCfg->getCostMode() != COST_LOSSLESS_CODING || !slice.isLossless())
        {
          if (modeId && !cbfDCT2)
          {
            continue;
          }
          if (!trModes[modeId].second)
          {
            continue;
          }
        }

        if (modeId > 0)
        {
          m_CABACEstimator->getCtx() = ctxBegin;
        }

        tu.mtsIdx[compID] = trModes[modeId].first;
        Distortion singleDistChroma = 0;
        if (numTransformCands > 1)
        {
          xIntraCodingACTTUBlock(tu, compID, singleDistChroma, modeId == 0 ? &trModes : nullptr, true);
        }
        else
        {
          xIntraCodingACTTUBlock(tu, compID, singleDistChroma);
        }
        if (tu.mtsIdx[compID] == MtsType::DCT2_DCT2)
        {
          cbfDCT2 = TU::getCbfAtDepth(tu, compID, currDepth);
        }
        uint64_t fracBitChroma    = xGetIntraFracBitsQTChroma(tu, compID);
        double   singleCostChroma = m_pcRdCost->calcRdCost(fracBitChroma, singleDistChroma, false);
        if (singleCostChroma < singleBestCostChroma)
        {
          singleBestCostChroma  = singleCostChroma;
          bestModeId            = modeId;
          if (bestModeId != (numTransformCands - 1))
          {
            saveChromaCS.getResiBuf(tu.blocks[compID]).copyFrom(csFull->getResiBuf(tu.blocks[compID]));
            tmpTU->copyComponentFrom(tu, compID);
            ctxBest = m_CABACEstimator->getCtx();
          }
        }
      }

      if (bestModeId != (numTransformCands - 1))
      {
        csFull->getResiBuf(tu.blocks[compID]).copyFrom(saveChromaCS.getResiBuf(tu.blocks[compID]));
        tu.copyComponentFrom(*tmpTU, compID);
        m_CABACEstimator->getCtx() = ctxBest;
      }
      if (m_pcEncCfg->getCostMode() != COST_LOSSLESS_CODING || !slice.isLossless())
      {
        m_pcRdCost->lambdaAdjustColorTrans(false, compID);
      }
    }

    Position tuPos = tu.Y();
    tuPos.relativeTo(cu.Y());
    const UnitArea relativeUnitArea(tu.chromaFormat, Area(tuPos, tu.Y().size()));
    PelUnitBuf     invColorTransResidual = m_colorTransResiBuf.getBuf(relativeUnitArea);
    csFull->getResiBuf(tu).colorSpaceConvert(invColorTransResidual, false, cs.slice->clpRng(COMPONENT_Y));

    Distortion totalDist = 0;
    for (uint32_t c = COMPONENT_Y; c < ::getNumberValidTBlocks(*csFull->pcv); c++)
    {
      const ComponentID compID = ComponentID(c);
      const CompArea&   area = tu.blocks[compID];
      PelBuf            piOrg = csFull->getOrgBuf(area);
      PelBuf            piReco = csFull->getRecoBuf(area);
      PelBuf            piPred = csFull->getPredBuf(area);
      PelBuf            piResi = invColorTransResidual.bufs[compID];

      if (doReshaping && (compID != COMPONENT_Y))
      {
        piResi.scaleSignal(tu.getChromaAdj(), 0, slice.clpRng(compID));
      }
      piReco.reconstruct(piPred, piResi, cs.slice->clpRng(compID));

      if (m_pcEncCfg->getLumaLevelToDeltaQPMapping().isEnabled() || (m_pcEncCfg->getLmcs()
        && slice.getLmcsEnabledFlag() && (m_pcReshape->getCTUFlag() || (isChroma(compID) && m_pcEncCfg->getReshapeIntraCMD()))))
      {
        const CPelBuf orgLuma = csFull->getOrgBuf(csFull->area.blocks[COMPONENT_Y]);
        if (compID == COMPONENT_Y && !(m_pcEncCfg->getLumaLevelToDeltaQPMapping().isEnabled()))
        {
          CompArea tmpArea1(COMPONENT_Y, area.chromaFormat, Position(0, 0), area.size());
          PelBuf   tmpRecLuma = m_tmpStorageCtu.getBuf(tmpArea1);
          tmpRecLuma.copyFrom(piReco);
          tmpRecLuma.rspSignal(m_pcReshape->getInvLUT());
          totalDist += m_pcRdCost->getDistPart(piOrg, tmpRecLuma, sps.getBitDepth(toChannelType(compID)), compID,
                                               DFuncWtd::SSE_WTD, orgLuma);
        }
        else
        {
          totalDist += m_pcRdCost->getDistPart(piOrg, piReco, sps.getBitDepth(toChannelType(compID)), compID,
                                               DFuncWtd::SSE_WTD, orgLuma);
        }
      }
      else
      {
        totalDist += m_pcRdCost->getDistPart(piOrg, piReco, sps.getBitDepth(toChannelType(compID)), compID, DFunc::SSE);
      }
    }

    m_CABACEstimator->getCtx() = ctxStart;
    uint64_t totalBits = xGetIntraFracBitsQT(*csFull, partitioner, true, true, -1, TU_NO_ISP);
    double   totalCost = m_pcRdCost->calcRdCost(totalBits, totalDist);

    saveChromaCS.getResiBuf(cbArea).copyFrom(csFull->getResiBuf(cbArea));
    saveChromaCS.getResiBuf(crArea).copyFrom(csFull->getResiBuf(crArea));
    saveChromaCS.getRecoBuf(tu).copyFrom(csFull->getRecoBuf(tu));
    tmpTU->copyComponentFrom(tu, COMPONENT_Cb);
    tmpTU->copyComponentFrom(tu, COMPONENT_Cr);
    ctxBest = m_CABACEstimator->getCtx();

    // 3.2 jointCbCr
    double     bestCostJointCbCr = totalCost;
    Distortion bestDistJointCbCr = totalDist;
    uint64_t   bestBitsJointCbCr = totalBits;
    int        bestJointCbCr = tu.jointCbCr; assert(!bestJointCbCr);

    bool       lastIsBest = false;
    CbfMaskList jointCbfMasksToTest;
    if (sps.getJointCbCrEnabledFlag() && (TU::getCbf(tu, COMPONENT_Cb) || TU::getCbf(tu, COMPONENT_Cr)))
    {
      m_pcTrQuant->selectICTCandidates(tu, orgResiCb, orgResiCr, jointCbfMasksToTest);
    }

    for (int cbfMask : jointCbfMasksToTest)
    {
      tu.jointCbCr = (uint8_t)cbfMask;

      ComponentID codeCompId  = (cbfMask & CBF_MASK_CB) != 0 ? COMPONENT_Cb : COMPONENT_Cr;
      ComponentID otherCompId = codeCompId == COMPONENT_Cb ? COMPONENT_Cr : COMPONENT_Cb;

      bool        tsAllowed = TU::isTSAllowed(tu, codeCompId) && (m_pcEncCfg->getUseChromaTS()) && !cu.lfnstIdx;
      uint8_t     numTransformCands = 1 + (tsAllowed ? 1 : 0); // DCT + TS = 2 tests
      bool        cbfDCT2 = true;

      trModes.clear();
      trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));   // DCT2
      if (tsAllowed)
      {
        trModes.push_back(TrMode(MtsType::SKIP, true));   // TS
      }

      for (int modeId = 0; modeId < numTransformCands; modeId++)
      {
        if (modeId && !cbfDCT2)
        {
          continue;
        }
        if (!trModes[modeId].second)
        {
          continue;
        }
        Distortion distTmp = 0;
        tu.mtsIdx[codeCompId] = trModes[modeId].first;
        tu.mtsIdx[otherCompId]     = MtsType::DCT2_DCT2;
        m_CABACEstimator->getCtx() = ctxStart;
        csFull->getResiBuf(cbArea).copyFrom(orgResiCb[cbfMask]);
        csFull->getResiBuf(crArea).copyFrom(orgResiCr[cbfMask]);
        if (nNumTransformCands > 1)
        {
          xIntraCodingACTTUBlock(tu, COMPONENT_Cb, distTmp, modeId == 0 ? &trModes : nullptr, true);
        }
        else
        {
          xIntraCodingACTTUBlock(tu, COMPONENT_Cb, distTmp);
        }

        double   costTmp = std::numeric_limits<double>::max();
        uint64_t bitsTmp = 0;
        if (distTmp < std::numeric_limits<Distortion>::max())
        {
          if (tu.mtsIdx[codeCompId] == MtsType::DCT2_DCT2)
          {
            cbfDCT2 = true;
          }
          csFull->getResiBuf(tu).colorSpaceConvert(invColorTransResidual, false, csFull->slice->clpRng(COMPONENT_Y));
          distTmp = 0;
          for (uint32_t c = COMPONENT_Y; c < ::getNumberValidTBlocks(*csFull->pcv); c++)
          {
            const ComponentID compID = ComponentID(c);
            const CompArea &  area   = tu.blocks[compID];
            PelBuf            piOrg  = csFull->getOrgBuf(area);
            PelBuf            piReco = csFull->getRecoBuf(area);
            PelBuf            piPred = csFull->getPredBuf(area);
            PelBuf            piResi = invColorTransResidual.bufs[compID];

            if (doReshaping && (compID != COMPONENT_Y))
            {
              piResi.scaleSignal(tu.getChromaAdj(), 0, slice.clpRng(compID));
            }
            piReco.reconstruct(piPred, piResi, cs.slice->clpRng(compID));
            if (m_pcEncCfg->getLumaLevelToDeltaQPMapping().isEnabled()
                || (m_pcEncCfg->getLmcs() && slice.getLmcsEnabledFlag()
                    && (m_pcReshape->getCTUFlag() || (isChroma(compID) && m_pcEncCfg->getReshapeIntraCMD()))))
            {
              const CPelBuf orgLuma = csFull->getOrgBuf(csFull->area.blocks[COMPONENT_Y]);
              if (compID == COMPONENT_Y && !(m_pcEncCfg->getLumaLevelToDeltaQPMapping().isEnabled()))
              {
                CompArea tmpArea1(COMPONENT_Y, area.chromaFormat, Position(0, 0), area.size());
                PelBuf   tmpRecLuma = m_tmpStorageCtu.getBuf(tmpArea1);
                tmpRecLuma.copyFrom(piReco);
                tmpRecLuma.rspSignal(m_pcReshape->getInvLUT());
                distTmp += m_pcRdCost->getDistPart(piOrg, tmpRecLuma, sps.getBitDepth(toChannelType(compID)), compID,
                                                   DFuncWtd::SSE_WTD, orgLuma);
              }
              else
              {
                distTmp += m_pcRdCost->getDistPart(piOrg, piReco, sps.getBitDepth(toChannelType(compID)), compID,
                                                   DFuncWtd::SSE_WTD, orgLuma);
              }
            }
            else
            {
              distTmp +=
                m_pcRdCost->getDistPart(piOrg, piReco, sps.getBitDepth(toChannelType(compID)), compID, DFunc::SSE);
            }
          }

          bitsTmp = xGetIntraFracBitsQT(*csFull, partitioner, true, true, -1, TU_NO_ISP);
          costTmp = m_pcRdCost->calcRdCost(bitsTmp, distTmp);
        }
        else if (tu.mtsIdx[codeCompId] == MtsType::DCT2_DCT2)
        {
          cbfDCT2 = false;
        }

        if (costTmp < bestCostJointCbCr)
        {
          bestCostJointCbCr = costTmp;
          bestDistJointCbCr = distTmp;
          bestBitsJointCbCr = bitsTmp;
          bestJointCbCr     = tu.jointCbCr;
          lastIsBest        = (cbfMask == jointCbfMasksToTest.back() && modeId == (numTransformCands - 1));

          // store data
          if (!lastIsBest)
          {
            saveChromaCS.getResiBuf(cbArea).copyFrom(csFull->getResiBuf(cbArea));
            saveChromaCS.getResiBuf(crArea).copyFrom(csFull->getResiBuf(crArea));
            saveChromaCS.getRecoBuf(tu).copyFrom(csFull->getRecoBuf(tu));
            tmpTU->copyComponentFrom(tu, COMPONENT_Cb);
            tmpTU->copyComponentFrom(tu, COMPONENT_Cr);

            ctxBest = m_CABACEstimator->getCtx();
          }
        }
      }
    }

    if (!lastIsBest)
    {
      csFull->getResiBuf(cbArea).copyFrom(saveChromaCS.getResiBuf(cbArea));
      csFull->getResiBuf(crArea).copyFrom(saveChromaCS.getResiBuf(crArea));
      csFull->getRecoBuf(tu).copyFrom(saveChromaCS.getRecoBuf(tu));
      tu.copyComponentFrom(*tmpTU, COMPONENT_Cb);
      tu.copyComponentFrom(*tmpTU, COMPONENT_Cr);

      m_CABACEstimator->getCtx() = ctxBest;
    }
    tu.jointCbCr = bestJointCbCr;
    csFull->picture->getRecoBuf(tu).copyFrom(csFull->getRecoBuf(tu));

    csFull->dist += bestDistJointCbCr;
    csFull->fracBits += bestBitsJointCbCr;
    csFull->cost = m_pcRdCost->calcRdCost(csFull->fracBits, csFull->dist);
  }

  bool validReturnSplit = false;
  if (checkSplit)
  {
    if (partitioner.canSplit(TU_MAX_TR_SPLIT, *csSplit))
    {
      partitioner.splitCurrArea(TU_MAX_TR_SPLIT, *csSplit);
    }

    bool splitIsSelected = true;
    do
    {
      bool tmpValidReturnSplit = xRecurIntraCodingACTQT(*csSplit, partitioner, mtsCheckRangeFlag, mtsFirstCheckId, mtsLastCheckId, moreProbMTSIdxFirst);
      if (sps.getUseLFNST())
      {
        if (!tmpValidReturnSplit)
        {
          splitIsSelected = false;
          break;
        }
      }
      else
      {
        CHECK(!tmpValidReturnSplit, "invalid RD of sub-TU partitions for ACT");
      }
    } while (partitioner.nextPart(*csSplit));

    partitioner.exitCurrSplit();

    if (splitIsSelected)
    {
      unsigned compCbf[3] = { 0, 0, 0 };
      for (auto &currTU : csSplit->traverseTUs(currArea, partitioner.chType))
      {
        for (unsigned ch = 0; ch < getNumberValidTBlocks(*csSplit->pcv); ch++)
        {
          compCbf[ch] |= (TU::getCbfAtDepth(currTU, ComponentID(ch), currDepth + 1) ? 1 : 0);
        }
      }

      for (auto &currTU : csSplit->traverseTUs(currArea, partitioner.chType))
      {
        TU::setCbfAtDepth(currTU, COMPONENT_Y, currDepth, compCbf[COMPONENT_Y]);
        TU::setCbfAtDepth(currTU, COMPONENT_Cb, currDepth, compCbf[COMPONENT_Cb]);
        TU::setCbfAtDepth(currTU, COMPONENT_Cr, currDepth, compCbf[COMPONENT_Cr]);
      }

      m_CABACEstimator->getCtx() = ctxStart;
      csSplit->fracBits = xGetIntraFracBitsQT(*csSplit, partitioner, true, true, -1, TU_NO_ISP);
      csSplit->cost = m_pcRdCost->calcRdCost(csSplit->fracBits, csSplit->dist);

      validReturnSplit = true;
    }
  }

  bool retVal = false;
  if (csFull || csSplit)
  {
    if (sps.getUseLFNST())
    {
      if (validReturnFull || validReturnSplit)
      {
        retVal = true;
      }
    }
    else
    {
      CHECK(!validReturnFull && !validReturnSplit, "illegal TU optimization");
      retVal = true;
    }
  }
  return retVal;
}

ChromaCbfs IntraSearch::xRecurIntraChromaCodingQT( CodingStructure &cs, Partitioner& partitioner, const double bestCostSoFar, const PartSplit ispType )
{
  UnitArea   currArea = partitioner.currArea();
  const bool keepResi = cs.sps->getUseLMChroma() || KEEP_PRED_AND_RESI_SIGNALS;

  if (!currArea.Cb().valid())
  {
    return ChromaCbfs(false);
  }
  const Slice           &slice = *cs.slice;

  TransformUnit        &currTU = *cs.getTU(currArea.chromaPos(), ChannelType::CHROMA);
  const PredictionUnit &pu     = *cs.getPU(currArea.chromaPos(), ChannelType::CHROMA);

  bool       lumaUsesISP = false;
  uint32_t   currDepth   = partitioner.currTrDepth;
  ChromaCbfs cbfs(false);

  if (currDepth == currTU.depth)
  {
    if (!currArea.Cb().valid() || !currArea.Cr().valid())
    {
      return cbfs;
    }

    CodingStructure &saveCS = *m_pSaveCS[1];
    saveCS.pcv      = cs.pcv;
    saveCS.picture  = cs.picture;
    saveCS.sps      = cs.sps;
    saveCS.area.repositionTo( cs.area );
    saveCS.initStructData( MAX_INT, true );

    if (!currTU.cu->isSepTree() && currTU.cu->ispMode != ISPType::NONE)
    {
      saveCS.clearCUs();
      CodingUnit& auxCU = saveCS.addCU( *currTU.cu, partitioner.chType );
      auxCU.ispMode = currTU.cu->ispMode;
      saveCS.clearPUs();
      saveCS.addPU( *currTU.cu->firstPU, partitioner.chType );
    }

    TransformUnit &tmpTU = saveCS.addTU(currArea, partitioner.chType);

    cs.setDecomp(currArea.Cb(), true); // set in advance (required for Cb2/Cr2 in 4:2:2 video)

    const unsigned      numTBlocks  = ::getNumberValidTBlocks( *cs.pcv );

    CompArea&  cbArea         = currTU.blocks[COMPONENT_Cb];
    CompArea&  crArea         = currTU.blocks[COMPONENT_Cr];
    double     bestCostCb     = MAX_DOUBLE;
    double     bestCostCr     = MAX_DOUBLE;
    Distortion bestDistCb     = 0;
    Distortion bestDistCr     = 0;
    int        maxModesTested = 0;
    bool       earlyExitISP   = false;

    TempCtx ctxStartTU(m_ctxPool);
    TempCtx ctxStart(m_ctxPool);
    TempCtx ctxBest(m_ctxPool);

    ctxStartTU       = m_CABACEstimator->getCtx();
    currTU.jointCbCr = 0;

    // Do predictions here to avoid repeating the "default0Save1Load2" stuff
    int predMode =
      pu.cu->bdpcmModeChroma != BdpcmMode::NONE ? BDPCM_IDX : PU::getFinalIntraMode(pu, ChannelType::CHROMA);

    PelBuf piPredCb = cs.getPredBuf(cbArea);
    PelBuf piPredCr = cs.getPredBuf(crArea);

    initIntraPatternChType( *currTU.cu, cbArea);
    initIntraPatternChType( *currTU.cu, crArea);

    if( PU::isLMCMode( predMode ) )
    {
      xGetLumaRecPixels( pu, cbArea );
      predIntraChromaLM( COMPONENT_Cb, piPredCb, pu, cbArea, predMode );
      predIntraChromaLM( COMPONENT_Cr, piPredCr, pu, crArea, predMode );
    }
    else if (PU::isMIP(pu, ChannelType::CHROMA))
    {
      initIntraMip(pu, cbArea);
      predIntraMip(COMPONENT_Cb, piPredCb, pu);

      initIntraMip(pu, crArea);
      predIntraMip(COMPONENT_Cr, piPredCr, pu);
    }
    else
    {
      predIntraAng( COMPONENT_Cb, piPredCb, pu);
      predIntraAng( COMPONENT_Cr, piPredCr, pu);
    }

    // determination of chroma residuals including reshaping and cross-component prediction
    //----- get chroma residuals -----
    PelBuf resiCb  = cs.getResiBuf(cbArea);
    PelBuf resiCr  = cs.getResiBuf(crArea);
    resiCb.copyFrom( cs.getOrgBuf (cbArea) );
    resiCr.copyFrom( cs.getOrgBuf (crArea) );
    resiCb.subtract( piPredCb );
    resiCr.subtract( piPredCr );

    //----- get reshape parameter ----
    bool doReshaping = ( cs.slice->getLmcsEnabledFlag() && cs.picHeader->getLmcsChromaResidualScaleFlag()
                         && (cs.slice->isIntra() || m_pcReshape->getCTUFlag()) && (cbArea.width * cbArea.height > 4) );
    if( doReshaping )
    {
      const Area area =
        currTU.Y().valid()
          ? currTU.Y()
          : Area(
            recalcPosition(currTU.chromaFormat, currTU.chType, ChannelType::LUMA, currTU.block(currTU.chType).pos()),
            recalcSize(currTU.chromaFormat, currTU.chType, ChannelType::LUMA, currTU.block(currTU.chType).size()));
      const CompArea &areaY = CompArea(COMPONENT_Y, currTU.chromaFormat, area);
      int adj = m_pcReshape->calculateChromaAdjVpduNei(currTU, areaY);
      currTU.setChromaAdj(adj);
    }

    //----- get cross component prediction parameters -----
    //===== store original residual signals =====
    CompStorage  orgResiCb[4], orgResiCr[4]; // 0:std, 1-3:jointCbCr (placeholder at this stage)
    orgResiCb[0].create( cbArea );
    orgResiCr[0].create( crArea );
    orgResiCb[0].copyFrom( resiCb );
    orgResiCr[0].copyFrom( resiCr );
    if( doReshaping )
    {
      int cResScaleInv = currTU.getChromaAdj();
      orgResiCb[0].scaleSignal( cResScaleInv, 1, currTU.cu->cs->slice->clpRng(COMPONENT_Cb) );
      orgResiCr[0].scaleSignal( cResScaleInv, 1, currTU.cu->cs->slice->clpRng(COMPONENT_Cr) );
    }

    for( uint32_t c = COMPONENT_Cb; c < numTBlocks; c++)
    {
      const ComponentID compID  = ComponentID(c);
      const CompArea&   area    = currTU.blocks[compID];

      double     singleCost     = MAX_DOUBLE;
      int        bestModeId     = 0;
      Distortion singleDistCTmp = 0;
      double     singleCostTmp  = 0;

      const bool tsAllowed = TU::isTSAllowed(currTU, compID) && m_pcEncCfg->getUseChromaTS() && !currTU.cu->lfnstIdx;
      uint8_t    nNumTransformCands = 1 + (tsAllowed ? 1 : 0);   // DCT + TS = 2 tests
      TrModeList trModes;
      if (m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless())
      {
        nNumTransformCands = 1;
        CHECK(!tsAllowed && currTU.cu->bdpcmModeChroma == BdpcmMode::NONE, "transform skip should be enabled for LS");
        if (currTU.cu->bdpcmModeChroma != BdpcmMode::NONE)
        {
          trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));
        }
        else
        {
          trModes.push_back(TrMode(MtsType::SKIP, true));
        }
      }
      else
      {
        trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));   // DCT2

        if (tsAllowed)
        {
          trModes.push_back(TrMode(MtsType::SKIP, true));   // TS
        }
      }
      CHECK(!currTU.Cb().valid(), "Invalid TU");

      const int  totalModesToTest = nNumTransformCands;
      bool       cbfDCT2          = true;
      const bool isOneMode        = false;
      maxModesTested              = totalModesToTest > maxModesTested ? totalModesToTest : maxModesTested;

      int currModeId = 0;
      int default0Save1Load2 = 0;

      if (!isOneMode)
      {
        ctxStart = m_CABACEstimator->getCtx();
      }

      for (int modeId = 0; modeId < nNumTransformCands; modeId++)
      {
        resiCb.copyFrom(orgResiCb[0]);
        resiCr.copyFrom(orgResiCr[0]);
        currTU.mtsIdx[compID] = currTU.cu->bdpcmModeChroma != BdpcmMode::NONE ? MtsType::SKIP : trModes[modeId].first;

        currModeId++;

        const bool isFirstMode = (currModeId == 1);
        const bool isLastMode  = false;   // Always store output to saveCS and tmpTU
        if (!(m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless()))
        {
          // if DCT2's cbf==0, skip ts search
          if (!cbfDCT2 && trModes[modeId].first == MtsType::SKIP)
          {
            break;
          }
          if (!trModes[modeId].second)
          {
            continue;
          }
        }

        if (!isFirstMode)   // if not first mode to be tested
        {
          m_CABACEstimator->getCtx() = ctxStart;
        }

        singleDistCTmp = 0;

        if (nNumTransformCands > 1)
        {
          xIntraCodingTUBlock(currTU, compID, singleDistCTmp, default0Save1Load2, nullptr,
                              modeId == 0 ? &trModes : nullptr, true);
        }
        else
        {
          xIntraCodingTUBlock(currTU, compID, singleDistCTmp, default0Save1Load2);
        }

        if (((currTU.mtsIdx[compID] == MtsType::SKIP && currTU.cu->bdpcmModeChroma == BdpcmMode::NONE)
             && !TU::getCbf(currTU, compID)))   // In order not to code TS flag when cbf is zero, the case for TS with
                                                // cbf being zero is forbidden.
        {
          if (m_pcEncCfg->getCostMode() != COST_LOSSLESS_CODING || !slice.isLossless())
          {
            singleCostTmp = MAX_DOUBLE;
          }
          else
          {
            uint64_t fracBitsTmp = xGetIntraFracBitsQTChroma(currTU, compID);
            singleCostTmp        = m_pcRdCost->calcRdCost(fracBitsTmp, singleDistCTmp);
          }
        }
        else if (lumaUsesISP && bestCostSoFar != MAX_DOUBLE && c == COMPONENT_Cb)
        {
          uint64_t fracBitsTmp = xGetIntraFracBitsQTSingleChromaComponent(cs, partitioner, ComponentID(c));
          singleCostTmp        = m_pcRdCost->calcRdCost(fracBitsTmp, singleDistCTmp);
          if (isOneMode || (!isOneMode && !isLastMode))
          {
            m_CABACEstimator->getCtx() = ctxStart;
          }
        }
        else if (!isOneMode)
        {
          uint64_t fracBitsTmp = xGetIntraFracBitsQTChroma(currTU, compID);
          singleCostTmp        = m_pcRdCost->calcRdCost(fracBitsTmp, singleDistCTmp);
        }

        if (singleCostTmp < singleCost)
        {
          singleCost  = singleCostTmp;
          bestModeId  = currModeId;

          if (c == COMPONENT_Cb)
          {
            bestCostCb = singleCostTmp;
            bestDistCb = singleDistCTmp;
          }
          else
          {
            bestCostCr = singleCostTmp;
            bestDistCr = singleDistCTmp;
          }

          if (currTU.mtsIdx[compID] == MtsType::DCT2_DCT2)
          {
            cbfDCT2 = TU::getCbfAtDepth(currTU, compID, currDepth);
          }

          if (!isLastMode)
          {
#if KEEP_PRED_AND_RESI_SIGNALS
            saveCS.getPredBuf(area).copyFrom(cs.getPredBuf(area));
            saveCS.getOrgResiBuf(area).copyFrom(cs.getOrgResiBuf(area));
#endif
            saveCS.getPredBuf(area).copyFrom(cs.getPredBuf(area));
            if (keepResi)
            {
              saveCS.getResiBuf(area).copyFrom(cs.getResiBuf(area));
            }
            saveCS.getRecoBuf(area).copyFrom(cs.getRecoBuf(area));

            tmpTU.copyComponentFrom(currTU, compID);

            ctxBest = m_CABACEstimator->getCtx();
          }
        }
      }

      if (lumaUsesISP && singleCost > bestCostSoFar && c == COMPONENT_Cb)
      {
        //Luma + Cb cost is already larger than the best cost, so we don't need to test Cr
        cs.dist = MAX_UINT;
        m_CABACEstimator->getCtx() = ctxStart;
        earlyExitISP               = true;
        break;
        //return cbfs;
      }

      // Done with one component of separate coding of Cr and Cb, just switch to the best Cb contexts if Cr coding is still to be done
      if ((c == COMPONENT_Cb && bestModeId < totalModesToTest) || (c == COMPONENT_Cb && m_pcEncCfg->getCostMode() == COST_LOSSLESS_CODING && slice.isLossless()))
      {
        m_CABACEstimator->getCtx() = ctxBest;

        currTU.copyComponentFrom(tmpTU, COMPONENT_Cb); // Cbf of Cb is needed to estimate cost for Cr Cbf
      }
    }

    if ( !earlyExitISP )
    {
      // Test using joint chroma residual coding
      double     bestCostCbCr   = bestCostCb + bestCostCr;
      Distortion bestDistCbCr   = bestDistCb + bestDistCr;
      int        bestJointCbCr  = 0;
      CbfMaskList jointCbfMasksToTest;

      const bool cbfCb = TU::getCbf(tmpTU, COMPONENT_Cb);
      const bool cbfCr = TU::getCbf(tmpTU, COMPONENT_Cr);

      if (cs.sps->getJointCbCrEnabledFlag() && (cbfCb || cbfCr))
      {
        m_pcTrQuant->selectICTCandidates(currTU, orgResiCb, orgResiCr, jointCbfMasksToTest);
      }

      const bool dctCb = cbfCb && tmpTU.mtsIdx[COMPONENT_Cb] == MtsType::DCT2_DCT2;
      const bool dctCr = cbfCr && tmpTU.mtsIdx[COMPONENT_Cr] == MtsType::DCT2_DCT2;

      const bool tsCb = cbfCb && tmpTU.mtsIdx[COMPONENT_Cb] == MtsType::SKIP;
      const bool tsCr = cbfCr && tmpTU.mtsIdx[COMPONENT_Cr] == MtsType::SKIP;

      const bool checkDctOnly = (dctCb && !cbfCr) || (dctCr && !cbfCb) || (dctCb && dctCr);
      const bool checkTsOnly  = (tsCb && !cbfCr) || (tsCr && !cbfCb) || (tsCb && tsCr);

      if (jointCbfMasksToTest.size() && currTU.cu->bdpcmModeChroma != BdpcmMode::NONE)
      {
        CHECK(!checkTsOnly || checkDctOnly, "bdpcm only allows transform skip");
      }
      for( int cbfMask : jointCbfMasksToTest )
      {
        currTU.jointCbCr = (uint8_t) cbfMask;

        ComponentID codeCompId  = (cbfMask & CBF_MASK_CB) != 0 ? COMPONENT_Cb : COMPONENT_Cr;
        ComponentID otherCompId = codeCompId == COMPONENT_Cb ? COMPONENT_Cr : COMPONENT_Cb;

        bool        tsAllowed = TU::isTSAllowed(currTU, codeCompId) && (m_pcEncCfg->getUseChromaTS()) && !currTU.cu->lfnstIdx;
        uint8_t     numTransformCands = 1 + (tsAllowed ? 1 : 0); // DCT + TS = 2 tests
        bool        cbfDCT2 = true;

        TrModeList trModes;
        if (checkDctOnly || checkTsOnly)
        {
          numTransformCands = 1;
        }

        if (!checkTsOnly || currTU.cu->bdpcmModeChroma != BdpcmMode::NONE)
        {
          trModes.push_back(TrMode(MtsType::DCT2_DCT2, true));   // DCT2
        }
        if (tsAllowed && !checkDctOnly)
        {
          trModes.push_back(TrMode(MtsType::SKIP, true));   // TS
        }
        for (int modeId = 0; modeId < numTransformCands; modeId++)
        {
          if (modeId && !cbfDCT2)
          {
            continue;
          }
          if (!trModes[modeId].second)
          {
            continue;
          }
          Distortion distTmp = 0;
          currTU.mtsIdx[codeCompId] =
            currTU.cu->bdpcmModeChroma != BdpcmMode::NONE ? MtsType::SKIP : trModes[modeId].first;
          currTU.mtsIdx[otherCompId] = MtsType::DCT2_DCT2;
          m_CABACEstimator->getCtx() = ctxStartTU;

          resiCb.copyFrom(orgResiCb[cbfMask]);
          resiCr.copyFrom(orgResiCr[cbfMask]);
          if (numTransformCands > 1)
          {
            xIntraCodingTUBlock(currTU, COMPONENT_Cb, distTmp, 0, nullptr, modeId == 0 ? &trModes : nullptr, true);
          }
          else
          {
            xIntraCodingTUBlock(currTU, COMPONENT_Cb, distTmp, 0);
          }
          double costTmp = std::numeric_limits<double>::max();
          if (distTmp < std::numeric_limits<Distortion>::max())
          {
            uint64_t bits = xGetIntraFracBitsQTChroma(currTU, COMPONENT_Cb);
            costTmp       = m_pcRdCost->calcRdCost(bits, distTmp);
            if (currTU.mtsIdx[codeCompId] == MtsType::DCT2_DCT2)
            {
              cbfDCT2 = true;
            }
          }
          else if (currTU.mtsIdx[codeCompId] == MtsType::DCT2_DCT2)
          {
            cbfDCT2 = false;
          }

          if (costTmp < bestCostCbCr)
          {
            bestCostCbCr  = costTmp;
            bestDistCbCr  = distTmp;
            bestJointCbCr = currTU.jointCbCr;

            // store data
            {
#if KEEP_PRED_AND_RESI_SIGNALS
              saveCS.getOrgResiBuf(cbArea).copyFrom(cs.getOrgResiBuf(cbArea));
              saveCS.getOrgResiBuf(crArea).copyFrom(cs.getOrgResiBuf(crArea));
#endif
              saveCS.getPredBuf(cbArea).copyFrom(cs.getPredBuf(cbArea));
              saveCS.getPredBuf(crArea).copyFrom(cs.getPredBuf(crArea));
              if (keepResi)
              {
                saveCS.getResiBuf(cbArea).copyFrom(cs.getResiBuf(cbArea));
                saveCS.getResiBuf(crArea).copyFrom(cs.getResiBuf(crArea));
              }
              saveCS.getRecoBuf(cbArea).copyFrom(cs.getRecoBuf(cbArea));
              saveCS.getRecoBuf(crArea).copyFrom(cs.getRecoBuf(crArea));

              tmpTU.copyComponentFrom(currTU, COMPONENT_Cb);
              tmpTU.copyComponentFrom(currTU, COMPONENT_Cr);

              ctxBest = m_CABACEstimator->getCtx();
            }
          }
        }
      }

      // Retrieve the best CU data (unless it was the very last one tested)
      {
#if KEEP_PRED_AND_RESI_SIGNALS
        cs.getPredBuf   (cbArea).copyFrom(saveCS.getPredBuf   (cbArea));
        cs.getOrgResiBuf(cbArea).copyFrom(saveCS.getOrgResiBuf(cbArea));
        cs.getPredBuf   (crArea).copyFrom(saveCS.getPredBuf   (crArea));
        cs.getOrgResiBuf(crArea).copyFrom(saveCS.getOrgResiBuf(crArea));
#endif
        cs.getPredBuf   (cbArea).copyFrom(saveCS.getPredBuf   (cbArea));
        cs.getPredBuf   (crArea).copyFrom(saveCS.getPredBuf   (crArea));

        if( keepResi )
        {
          cs.getResiBuf (cbArea).copyFrom(saveCS.getResiBuf   (cbArea));
          cs.getResiBuf (crArea).copyFrom(saveCS.getResiBuf   (crArea));
        }
        cs.getRecoBuf   (cbArea).copyFrom(saveCS.getRecoBuf   (cbArea));
        cs.getRecoBuf   (crArea).copyFrom(saveCS.getRecoBuf   (crArea));

        currTU.copyComponentFrom(tmpTU, COMPONENT_Cb);
        currTU.copyComponentFrom(tmpTU, COMPONENT_Cr);

        m_CABACEstimator->getCtx() = ctxBest;
      }

      // Copy results to the picture structures
      cs.picture->getRecoBuf(cbArea).copyFrom(cs.getRecoBuf(cbArea));
      cs.picture->getRecoBuf(crArea).copyFrom(cs.getRecoBuf(crArea));
      cs.picture->getPredBuf(cbArea).copyFrom(cs.getPredBuf(cbArea));
      cs.picture->getPredBuf(crArea).copyFrom(cs.getPredBuf(crArea));

      cbfs.cbf(COMPONENT_Cb) = TU::getCbf(currTU, COMPONENT_Cb);
      cbfs.cbf(COMPONENT_Cr) = TU::getCbf(currTU, COMPONENT_Cr);

      currTU.jointCbCr = ( (cbfs.cbf(COMPONENT_Cb) + cbfs.cbf(COMPONENT_Cr)) ? bestJointCbCr : 0 );
      cs.dist         += bestDistCbCr;
    }
  }
  else
  {
    unsigned    numValidTBlocks   = ::getNumberValidTBlocks( *cs.pcv );
    ChromaCbfs  SplitCbfs         ( false );

    if( partitioner.canSplit( TU_MAX_TR_SPLIT, cs ) )
    {
      partitioner.splitCurrArea( TU_MAX_TR_SPLIT, cs );
    }
    else if (currTU.cu->ispMode != ISPType::NONE)
    {
      partitioner.splitCurrArea( ispType, cs );
    }
    else
    {
      THROW( "Implicit TU split not available" );
    }

    do
    {
      ChromaCbfs subCbfs = xRecurIntraChromaCodingQT( cs, partitioner, bestCostSoFar, ispType );

      for( uint32_t ch = COMPONENT_Cb; ch < numValidTBlocks; ch++ )
      {
        const ComponentID compID = ComponentID( ch );
        SplitCbfs.cbf( compID ) |= subCbfs.cbf( compID );
      }
    } while( partitioner.nextPart( cs ) );

    partitioner.exitCurrSplit();

    if( lumaUsesISP && cs.dist == MAX_UINT )
    {
      return cbfs;
    }
    cbfs.Cb |= SplitCbfs.Cb;
    cbfs.Cr |= SplitCbfs.Cr;

    if (!lumaUsesISP)
    {
      for (auto &ptu: cs.tus)
      {
        if (currArea.Cb().contains(ptu->Cb()) || (!ptu->Cb().valid() && currArea.Y().contains(ptu->Y())))
        {
          TU::setCbfAtDepth(*ptu, COMPONENT_Cb, currDepth, SplitCbfs.Cb);
          TU::setCbfAtDepth(*ptu, COMPONENT_Cr, currDepth, SplitCbfs.Cr);
        }
      }
    }
  }

  return cbfs;
}

uint64_t IntraSearch::xFracModeBitsIntra(PredictionUnit &pu, const uint32_t &mode, const ChannelType &chType)
{
  uint32_t orgMode = mode;

  if (!pu.ciipFlag)
  {
    std::swap(orgMode, pu.intraDir[chType]);
  }

  m_CABACEstimator->resetBits();

  if( isLuma( chType ) )
  {
    if (!pu.ciipFlag)
    {
      m_CABACEstimator->intra_luma_pred_mode(pu);
    }
  }
  else
  {
    m_CABACEstimator->intra_chroma_pred_mode( pu );
  }

  if ( !pu.ciipFlag )
  {
    std::swap(orgMode, pu.intraDir[chType]);
  }

  return m_CABACEstimator->getEstFracBits();
}

void IntraSearch::sortRdModeListFirstColorSpace(ModeInfo mode, double cost, const BdpcmMode bdpcmMode,
                                                ModeInfo *rdModeList, double *rdCostList, BdpcmMode *bdpcmModeList,
                                                int &candNum)
{
  if (candNum == 0)
  {
    rdModeList[0] = mode;
    rdCostList[0] = cost;
    bdpcmModeList[0] = bdpcmMode;
    candNum++;
    return;
  }

  int insertPos = -1;
  for (int pos = candNum - 1; pos >= 0; pos--)
  {
    if (cost < rdCostList[pos])
    {
      insertPos = pos;
    }
  }

  if (insertPos >= 0)
  {
    for (int i = candNum - 1; i >= insertPos; i--)
    {
      rdModeList[i + 1] = rdModeList[i];
      rdCostList[i + 1] = rdCostList[i];
      bdpcmModeList[i + 1] = bdpcmModeList[i];
    }
    rdModeList[insertPos] = mode;
    rdCostList[insertPos] = cost;
    bdpcmModeList[insertPos] = bdpcmMode;
    candNum++;
  }
  else
  {
    rdModeList[candNum] = mode;
    rdCostList[candNum] = cost;
    bdpcmModeList[candNum] = bdpcmMode;
    candNum++;
  }

  CHECK(candNum > FAST_UDI_MAX_RDMODE_NUM, "exceed intra mode candidate list capacity");

  return;
}

void IntraSearch::invalidateBestRdModeFirstColorSpace()
{
  int numSaveRdClass = 4 * NUM_LFNST_NUM_PER_SET * 2;
  int savedRdModeListSize = FAST_UDI_MAX_RDMODE_NUM;

  for (int i = 0; i < numSaveRdClass; i++)
  {
    m_numSavedRdModeFirstColorSpace[i] = 0;
    for (int j = 0; j < savedRdModeListSize; j++)
    {
      m_savedRdModeFirstColorSpace[i][j] = ModeInfo(false, false, 0, ISPType::NONE, 0);
      m_savedBDPCMModeFirstColorSpace[i][j] = BdpcmMode::NONE;
      m_savedRdCostFirstColorSpace[i][j] = MAX_DOUBLE;
    }
  }
}

template<typename T, size_t N>
void IntraSearch::reduceHadCandList(static_vector<T, N>& candModeList, static_vector<double, N>& candCostList, int& numModesForFullRD, const double thresholdHadCost, const double* mipHadCost, const PredictionUnit &pu, const bool fastMip)
{
  const int maxCandPerType = numModesForFullRD >> 1;
  static_vector<ModeInfo, FAST_UDI_MAX_RDMODE_NUM> tempRdModeList;
  static_vector<double, FAST_UDI_MAX_RDMODE_NUM> tempCandCostList;
  const double minCost = candCostList[0];
  bool keepOneMip = candModeList.size() > numModesForFullRD;

  int numConv = 0;
  int numMip = 0;
  for (int idx = 0; idx < candModeList.size() - (keepOneMip?0:1); idx++)
  {
    bool addMode = false;
    const ModeInfo& orgMode = candModeList[idx];

    if (!orgMode.mipFlg)
    {
      addMode = (numConv < 3);
      numConv += addMode ? 1:0;
    }
    else
    {
      addMode = ( numMip < maxCandPerType || (candCostList[idx] < thresholdHadCost * minCost) || keepOneMip );
      keepOneMip = false;
      numMip += addMode ? 1:0;
    }
    if( addMode )
    {
      tempRdModeList.push_back(orgMode);
      tempCandCostList.push_back(candCostList[idx]);
    }
  }

  if ((pu.lwidth() > 8 && pu.lheight() > 8))
  {
    // Sort MIP candidates by Hadamard cost
    const int transpOff = MatrixIntraPrediction::getNumModesMip(pu.Y());

    static_vector<uint8_t, FAST_UDI_MAX_RDMODE_NUM> sortedMipModes(0);
    static_vector<double, FAST_UDI_MAX_RDMODE_NUM> sortedMipCost(0);
    for( uint8_t mode : { 0, 1, 2 } )
    {
      uint8_t candMode = mode + uint8_t((mipHadCost[mode + transpOff] < mipHadCost[mode]) ? transpOff : 0);
      updateCandList(candMode, mipHadCost[candMode], sortedMipModes, sortedMipCost, 3);
    }

    // Append MIP mode to RD mode list
    const int modeListSize = int(tempRdModeList.size());
    for (int idx = 0; idx < 3; idx++)
    {
      const bool     isTransposed = (sortedMipModes[idx] >= transpOff ? true : false);
      const uint32_t mipIdx       = (isTransposed ? sortedMipModes[idx] - transpOff : sortedMipModes[idx]);
      const ModeInfo mipMode(true, isTransposed, 0, ISPType::NONE, mipIdx);
      bool alreadyIncluded = false;
      for (int modeListIdx = 0; modeListIdx < modeListSize; modeListIdx++)
      {
        if (tempRdModeList[modeListIdx] == mipMode)
        {
          alreadyIncluded = true;
          break;
        }
      }

      if (!alreadyIncluded)
      {
        tempRdModeList.push_back(mipMode);
        tempCandCostList.push_back(0);
        if (fastMip)
        {
          break;
        }
      }
    }
  }

  candModeList = tempRdModeList;
  candCostList = tempCandCostList;
  numModesForFullRD = int(candModeList.size());
}

// It decides which modes from the ISP lists can be full RD tested
void IntraSearch::xGetNextISPMode(ModeInfo& modeInfo, const ModeInfo* lastMode, const Size cuSize)
{
  if (m_curIspLfnstIdx >= NUM_LFNST_NUM_PER_SET)
  {
    // All LFNST indices have been checked
    return;
  }

  ISPType nextISPcandSplitType;
  auto      &ispTestedModes       = m_ispTestedModes[m_curIspLfnstIdx];
  const bool horSplitIsTerminated = ispTestedModes.splitIsFinished[ISPType::HOR];
  const bool verSplitIsTerminated = ispTestedModes.splitIsFinished[ISPType::VER];
  if (!horSplitIsTerminated && !verSplitIsTerminated)
  {
    nextISPcandSplitType = !lastMode ? ISPType::HOR : lastMode->ispMod == ISPType::HOR ? ISPType::VER : ISPType::HOR;
  }
  else if (!horSplitIsTerminated && verSplitIsTerminated)
  {
    nextISPcandSplitType = ISPType::HOR;
  }
  else if (horSplitIsTerminated && !verSplitIsTerminated)
  {
    nextISPcandSplitType = ISPType::VER;
  }
  else
  {
    xFinishISPModes();
    return;   // no more modes will be tested
  }

  int maxNumSubPartitions = ispTestedModes.numTotalParts[nextISPcandSplitType];

  // We try to break the split here for lfnst > 0 according to the first mode
  if (m_curIspLfnstIdx > 0 && ispTestedModes.numTestedModes[nextISPcandSplitType] == 1)
  {
    int firstModeThisSplit = ispTestedModes.getTestedIntraMode(nextISPcandSplitType, 0);
    int numSubPartsFirstModeThisSplit = ispTestedModes.getNumCompletedSubParts(nextISPcandSplitType, firstModeThisSplit);
    CHECK(numSubPartsFirstModeThisSplit < 0, "wrong number of subpartitions!");
    bool stopThisSplit = false;
    bool stopThisSplitAllLfnsts = false;
    if (numSubPartsFirstModeThisSplit < maxNumSubPartitions)
    {
      stopThisSplit = true;
      if (m_pcEncCfg->getUseFastISP() && m_curIspLfnstIdx == 1
          && numSubPartsFirstModeThisSplit < maxNumSubPartitions - 1)
      {
        stopThisSplitAllLfnsts = true;
      }
    }

    if (stopThisSplit)
    {
      ispTestedModes.splitIsFinished[nextISPcandSplitType] = true;
      if (m_curIspLfnstIdx == 1 && stopThisSplitAllLfnsts)
      {
        m_ispTestedModes[2].splitIsFinished[nextISPcandSplitType] = true;
      }
      return;
    }
  }

  // We try to break the split here for lfnst = 0 or all lfnst indices according to the first two modes
  if (m_curIspLfnstIdx == 0 && ispTestedModes.numTestedModes[nextISPcandSplitType] == 2)
  {
    // Split stop criteria after checking the performance of previously tested intra modes
    const int thresholdSplit1 = maxNumSubPartitions;
    bool stopThisSplit = false;
    bool stopThisSplitForAllLFNSTs = false;
    const int thresholdSplit1ForAllLFNSTs = maxNumSubPartitions - 1;

    std::array<int, 2> modes;
    int numSubPartsBestMode[2];

    for (int i = 0; i < 2; i++)
    {
      modes[i] = ispTestedModes.getTestedIntraMode(nextISPcandSplitType, i);
      modes[i] = modes[i] == DC_IDX ? NOMODE_IDX : modes[i];
      numSubPartsBestMode[i] =
        modes[i] != NOMODE_IDX ? ispTestedModes.getNumCompletedSubParts(nextISPcandSplitType, modes[i]) : -1;
    }

    // 1) The 2 most promising modes do not reach a certain number of sub-partitions
    if (numSubPartsBestMode[0] != -1 && numSubPartsBestMode[1] != -1)
    {
      if (numSubPartsBestMode[0] < thresholdSplit1 && numSubPartsBestMode[1] < thresholdSplit1)
      {
        stopThisSplit = true;
        if (m_curIspLfnstIdx == 0 && numSubPartsBestMode[0] < thresholdSplit1ForAllLFNSTs
            && numSubPartsBestMode[1] < thresholdSplit1ForAllLFNSTs)
        {
          stopThisSplitForAllLFNSTs = true;
        }
      }
      else
      {
        // we stop also if the cost is MAX_DOUBLE for all modes
        if (std::find_if(modes.begin(), modes.end(),
                         [&](const int &x) { return ispTestedModes.getRDCost(nextISPcandSplitType, x) < MAX_DOUBLE; })
            == modes.end())
        {
          stopThisSplit = true;
        }
      }
    }

    if (!stopThisSplit)
    {
      int numSubPartsBestModeAltSplit[2];

      // 2) One split type may be discarded by comparing the number of sub-partitions of the best angle modes of both splits
      const ISPType otherSplit            = nextISPcandSplitType == ISPType::HOR ? ISPType::VER : ISPType::HOR;
      numSubPartsBestModeAltSplit[1] =
        modes[1] != NOMODE_IDX ? ispTestedModes.getNumCompletedSubParts(otherSplit, modes[1]) : -1;
      if (numSubPartsBestModeAltSplit[1] != -1 && numSubPartsBestMode[1] != -1
          && ispTestedModes.bestSplitSoFar != nextISPcandSplitType)
      {
        if (numSubPartsBestModeAltSplit[1] > numSubPartsBestMode[1])
        {
          stopThisSplit = true;
        }
        // both have the same number of subpartitions
        else if (numSubPartsBestModeAltSplit[1] == numSubPartsBestMode[1])
        {
          // both have the maximum number of subpartitions, so it compares RD costs to decide
          if (numSubPartsBestModeAltSplit[1] == maxNumSubPartitions)
          {
            double rdCostBestMode2ThisSplit  = ispTestedModes.getRDCost(nextISPcandSplitType, modes[1]);
            double rdCostBestMode2OtherSplit = ispTestedModes.getRDCost(otherSplit, modes[1]);
            double threshold = 1.3;
            if (rdCostBestMode2ThisSplit == MAX_DOUBLE || rdCostBestMode2OtherSplit < rdCostBestMode2ThisSplit * threshold)
            {
              stopThisSplit = true;
            }
          }
          else // none of them reached the maximum number of subpartitions with the best angle modes, so it compares the results with the the planar mode
          {
            numSubPartsBestModeAltSplit[0] =
              modes[0] != NOMODE_IDX ? ispTestedModes.getNumCompletedSubParts(otherSplit, modes[0]) : -1;
            if (numSubPartsBestModeAltSplit[0] != -1 && numSubPartsBestMode[0] != -1
                && numSubPartsBestModeAltSplit[0] > numSubPartsBestMode[0])
            {
              stopThisSplit = true;
            }
          }
        }
      }
    }
    if (stopThisSplit)
    {
      ispTestedModes.splitIsFinished[nextISPcandSplitType] = true;
      if (stopThisSplitForAllLFNSTs)
      {
        for (int lfnstIdx = 1; lfnstIdx < NUM_LFNST_NUM_PER_SET; lfnstIdx++)
        {
          m_ispTestedModes[lfnstIdx].splitIsFinished[nextISPcandSplitType] = true;
        }
      }
      return;
    }
  }

  // Now a new mode is retrieved from the list and it has to be decided whether it should be tested or not
  if (ispTestedModes.candIndexInList[nextISPcandSplitType] < m_ispCandList[nextISPcandSplitType].size())
  {
    ModeInfo candidate = m_ispCandList[nextISPcandSplitType].at(ispTestedModes.candIndexInList[nextISPcandSplitType]);
    ispTestedModes.candIndexInList[nextISPcandSplitType]++;

    // extra modes are only tested if ISP has won so far
    if (ispTestedModes.candIndexInList[nextISPcandSplitType] > ispTestedModes.numOrigModesToTest)
    {
      if (ispTestedModes.bestSplitSoFar != candidate.ispMod || ispTestedModes.bestModeSoFar == PLANAR_IDX)
      {
        ispTestedModes.splitIsFinished[nextISPcandSplitType] = true;
        return;
      }
    }

    bool testCandidate = true;

    // we look for a reference mode that has already been tested within the window and decide to test the new one according to the reference mode costs
    if (maxNumSubPartitions > 2
        && (m_curIspLfnstIdx > 0
            || (candidate.modeId >= DC_IDX && ispTestedModes.numTestedModes[nextISPcandSplitType] >= 2)))
    {
      std::array<int, 2> similarModes;
      similarModes.fill(NOMODE_IDX);

      constexpr int ANG_WINDOW_SIZE = 5;
      const int     windowSize      = candidate.modeId > DC_IDX ? ANG_WINDOW_SIZE : 1;

      int refLfnstIdx = m_curIspLfnstIdx;
      xFindAlreadyTestedNearbyIntraModes((int) candidate.modeId, refLfnstIdx, similarModes, candidate.ispMod,
                                         windowSize);

      int numSubPartsRefMode = 0;
      if (refLfnstIdx != m_curIspLfnstIdx)
      {
        numSubPartsRefMode = m_ispTestedModes[refLfnstIdx].getNumCompletedSubParts(candidate.ispMod, candidate.modeId);
        CHECK(numSubPartsRefMode <= 0, "Wrong value of the number of subpartitions completed!");
      }
      else
      {
        for (auto m: similarModes)
        {
          if (m != NOMODE_IDX)
          {
            numSubPartsRefMode =
              std::max(numSubPartsRefMode, ispTestedModes.getNumCompletedSubParts(candidate.ispMod, m));
          }
        }
      }

      if (numSubPartsRefMode > 0)
      {
        const int numSamples       = cuSize.width << floorLog2(cuSize.height);
        const int numSubPartsLimit = numSamples >= 256 ? maxNumSubPartitions - 1 : 2;

        // The mode was found. Now we check the condition
        testCandidate = numSubPartsRefMode > numSubPartsLimit;
      }
    }

    if (testCandidate)
    {
      modeInfo = candidate;
    }
  }
  else
  {
    //the end of the list was reached, so the split is invalidated
    ispTestedModes.splitIsFinished[nextISPcandSplitType] = true;
  }
}

void IntraSearch::xFindAlreadyTestedNearbyIntraModes(const int currentIntraMode, int &refLfnstIdx,
                                                     std::array<int, 2> &similarModes, const ISPType ispOption,
                                                     const int windowSize)
{
  //first we check if the exact intra mode was already tested for another lfnstIdx value
  for (int idx = refLfnstIdx - 1; idx >= 0; idx--)
  {
    if (m_ispTestedModes[idx].modeHasBeenTested[currentIntraMode][ispOption])
    {
      refLfnstIdx = idx;
      return;
    }
  }

  //The mode has not been checked for another lfnstIdx value, so now we look for a similar mode within a window using the same lfnstIdx
  for (int k = 1; k <= windowSize; k++)
  {
    const int leftMode =
      (currentIntraMode + NUM_INTRA_ANGULAR_MODES - ANGULAR_BASE - k) % NUM_INTRA_ANGULAR_MODES + ANGULAR_BASE;
    const int rightMode = currentIntraMode < ANGULAR_BASE
                            ? PLANAR_IDX
                            : (currentIntraMode - ANGULAR_BASE + k) % NUM_INTRA_ANGULAR_MODES + ANGULAR_BASE;

    auto found = [&](int m)
    { return m != currentIntraMode ? m_ispTestedModes[refLfnstIdx].modeHasBeenTested[m][ispOption] : false; };

    const bool leftModeFound  = found(leftMode);
    const bool rightModeFound = found(rightMode);

    if (leftModeFound || rightModeFound)
    {
      similarModes[0] = leftModeFound ? leftMode : NOMODE_IDX;
      similarModes[1] = rightModeFound ? rightMode : NOMODE_IDX;
      return;
    }
  }
}

//It prepares the list of potential intra modes candidates that will be tested using RD costs
bool IntraSearch::xSortISPCandList(double bestCostSoFar, double bestNonISPCost, const ModeInfo &bestNonISPMode)
{
  int bestISPModeInRelCU = NOMODE_IDX;
  m_modeCtrl->setStopNonDCT2Transforms(false);

  if (m_pcEncCfg->getUseFastISP())
  {
    // check if the ISP tests can be skipped
    const double thSkipISP = 1.4;

    if (bestNonISPCost > bestCostSoFar * thSkipISP)
    {
      for (int j = 0; j < NUM_LFNST_NUM_PER_SET; j++)
      {
        m_ispTestedModes[j].splitIsFinished.fill(true);
      }
      return false;
    }

    if (!updateISPStatusFromRelCU(bestNonISPCost, bestNonISPMode, bestISPModeInRelCU))
    {
      return false;
    }
  }

  for (auto &c: m_ispCandList[ISPType::HOR])
  {
    // set the correct ISP split type value
    c.ispMod = ISPType::HOR;
  }

  auto     origHadList = m_ispCandList[ISPType::HOR];   // save the original hadamard list of regular intra
  ModeInfo refMode     = origHadList.front();

  m_ispCandList[ISPType::HOR].clear();
  m_ispCandList[ISPType::VER].clear();

  // we sort the normal intra modes according to their full RD costs
  std::stable_sort(m_regIntraRDListWithCosts.begin(), m_regIntraRDListWithCosts.end(), ModeInfoWithCost::compare);

  // we get the best angle from the regular intra list
  const auto p = std::find_if(m_regIntraRDListWithCosts.begin(), m_regIntraRDListWithCosts.end(),
                              [](const ModeInfoWithCost &mi) { return mi.modeId >= ANGULAR_BASE; });

  const int bestNormalIntraAngle = p == m_regIntraRDListWithCosts.end() ? NOMODE_IDX : p->modeId;

  auto &destList = m_ispCandList[ISPType::HOR];

  std::array<bool, NUM_LUMA_MODE> modeIsInList;
  modeIsInList.fill(false);

  //List creation

  auto addMode = [&](const int m) -> bool
  {
    if (!modeIsInList[m])
    {
      refMode.modeId = m;
      destList.push_back(refMode);
      modeIsInList[m] = true;
      return true;
    }
    return false;
  };

  if (m_pcEncCfg->getUseFastISP() && bestISPModeInRelCU != NOMODE_IDX)   // RelCU intra mode
  {
    addMode(bestISPModeInRelCU);
  }

  // Planar
  addMode(PLANAR_IDX);

  // Best angle in regular intra
  if (bestNormalIntraAngle != NOMODE_IDX)
  {
    addMode(bestNormalIntraAngle);
  }

  // Remaining regular intra modes that were full RD tested (except DC, which is added after the angles from regular intra)
  bool addDc = false;

  for (const auto &e: m_regIntraRDListWithCosts)
  {
    if (e.modeId == DC_IDX)
    {
      addDc = true;
    }
    else
    {
      addMode(e.modeId);
    }
  }

  // DC is added after the angles from regular intra
  if (addDc)
  {
    addMode(DC_IDX);
  }

  // We add extra candidates to the list that will only be tested if ISP is likely to win
  for (int j = 0; j < NUM_LFNST_NUM_PER_SET; j++)
  {
    m_ispTestedModes[j].numOrigModesToTest = (int) destList.size();
  }

  const int addedModesFromHadList = 3;
  int       newModesAdded = 0;

  for (const auto &e: origHadList)
  {
    if (addMode(e.modeId))
    {
      if (++newModesAdded == addedModesFromHadList)
      {
        break;
      }
    }
  }

  if (m_pcEncCfg->getUseFastISP() && bestISPModeInRelCU != NOMODE_IDX)
  {
    destList.resize(1);
  }

  // Copy modes to other split-type list
  m_ispCandList[ISPType::VER] = m_ispCandList[ISPType::HOR];
  for (auto &x: m_ispCandList[ISPType::VER])
  {
    x.ispMod = ISPType::VER;
  }

  // Reset the tested modes information to 0
  for (int j = 0; j < NUM_LFNST_NUM_PER_SET; j++)
  {
    for (const auto &x: m_ispCandList[ISPType::HOR])
    {
      m_ispTestedModes[j].clearISPModeInfo(x.modeId);
    }
  }
  return true;
}

void IntraSearch::xSortISPCandListLFNST()
{
  //It resorts the list of intra mode candidates for lfnstIdx > 0 by checking the RD costs for lfnstIdx = 0
  ISPTestedModesInfo& ispTestedModesRef = m_ispTestedModes[0];
  for (const auto ispMode: { ISPType::HOR, ISPType::VER })
  {
    if (!m_ispTestedModes[m_curIspLfnstIdx].splitIsFinished[ispMode]
        && ispTestedModesRef.testedModes[ispMode].size() > 1)
    {
      auto  &candList     = m_ispCandList[ispMode];
      int    bestModeId   = candList[1].modeId > DC_IDX ? candList[1].modeId : NOMODE_IDX;
      int bestSubParts = candList[1].modeId > DC_IDX ? ispTestedModesRef.getNumCompletedSubParts(ispMode, bestModeId) : -1;
      double bestCost  = candList[1].modeId > DC_IDX ? ispTestedModesRef.getRDCost(ispMode, bestModeId) : MAX_DOUBLE;
      for (int i = 0; i < candList.size(); i++)
      {
        const int candSubParts = ispTestedModesRef.getNumCompletedSubParts(ispMode, candList[i].modeId);
        const double candCost = ispTestedModesRef.getRDCost(ispMode, candList[i].modeId);
        if (candSubParts > bestSubParts || candCost < bestCost)
        {
          bestModeId = candList[i].modeId;
          bestCost = candCost;
          bestSubParts = candSubParts;
        }
      }

      if (bestModeId != NOMODE_IDX && bestModeId != candList[0].modeId)
      {
        auto prevMode      = candList[0];
        candList[0].modeId = bestModeId;
        for (int i = 1; i < candList.size(); i++)
        {
          auto nextMode = candList[i];
          candList[i]   = prevMode;
          if (nextMode.modeId == bestModeId)
          {
            break;
          }
          prevMode = nextMode;
        }
      }
    }
  }
}

bool IntraSearch::updateISPStatusFromRelCU(double bestNonISPCostCurrCu, const ModeInfo &bestNonISPModeCurrCu,
                                           int &bestISPModeInRelCU)
{
  //It compares the data of a related CU with the current CU to cancel or reduce the ISP tests
  bestISPModeInRelCU = NOMODE_IDX;
  if (m_modeCtrl->getRelatedCuIsValid())
  {
    const IspPredModeVal ispPredModeVal = m_modeCtrl->getIspPredModeValRelCU();

    const bool bestModeRelCuIsMip = ispPredModeVal.mipFlag;
    const int  relatedCuIntraMode = ispPredModeVal.bestPredModeDCT2;

    double bestNonISPCostRelCU = m_modeCtrl->getBestDCT2NonISPCostRelCU();
    double costRatio           = bestNonISPCostCurrCu / bestNonISPCostRelCU;
    bool   bestModeCurrCuIsMip = bestNonISPModeCurrCu.mipFlg;
    bool   isSameTypeOfMode    = bestModeRelCuIsMip == bestModeCurrCuIsMip;
    bool   bothModesAreAngular = isSameTypeOfMode && !bestModeCurrCuIsMip && bestNonISPModeCurrCu.modeId > DC_IDX && relatedCuIntraMode > DC_IDX;
    bool   modesAreComparable =
      isSameTypeOfMode
      && (bestNonISPModeCurrCu.modeId == relatedCuIntraMode
          || (bothModesAreAngular && abs(relatedCuIntraMode - (int) bestNonISPModeCurrCu.modeId) <= 5));

    CHECK(!ispPredModeVal.valid, "Wrong ISP relCU status");

    if (ispPredModeVal.notIsp)   // ISP was not selected in the relCU
    {
      double bestNonDCT2Cost = m_modeCtrl->getBestNonDCT2Cost();
      double ratioWithNonDCT2 = bestNonDCT2Cost / bestNonISPCostRelCU;
      const double margin           = ratioWithNonDCT2 < 0.95 ? 0.2 : 0.1;

      if (costRatio > 1.0 - margin && costRatio < 1.0 + margin && modesAreComparable)
      {
        for (int lfnstVal = 0; lfnstVal < NUM_LFNST_NUM_PER_SET; lfnstVal++)
        {
          m_ispTestedModes[lfnstVal].splitIsFinished[ISPType::HOR] = true;
          m_ispTestedModes[lfnstVal].splitIsFinished[ISPType::VER] = true;
        }
        return false;
      }
    }
    else
    {
      const double margin = 0.05;

      if (costRatio > 1.0 - margin && costRatio < 1.0 + margin && modesAreComparable)
      {
        bestISPModeInRelCU = (int)m_modeCtrl->getBestISPIntraModeRelCU();

        for (const auto splitIdx: { ISPType::HOR, ISPType::VER })
        {
          for (int lfnstVal = 0; lfnstVal < NUM_LFNST_NUM_PER_SET; lfnstVal++)
          {
            if (lfnstVal == ispPredModeVal.ispLfnstIdx
                && splitIdx == (ispPredModeVal.verIsp == 0 ? ISPType::HOR : ISPType::VER))
            {
              continue;
            }
            m_ispTestedModes[lfnstVal].splitIsFinished[splitIdx] = true;
          }
        }

        m_modeCtrl->setStopNonDCT2Transforms(ispPredModeVal.lowIspCost);
      }
    }
  }

  return true;
}

void IntraSearch::xFinishISPModes()
{
  //Continue to the next lfnst index
  m_curIspLfnstIdx++;

  if (m_curIspLfnstIdx < NUM_LFNST_NUM_PER_SET)
  {
    //Check if LFNST is applicable
    if (m_curIspLfnstIdx == 1)
    {
      bool canTestLFNST = false;
      for (int lfnstIdx = 1; lfnstIdx < NUM_LFNST_NUM_PER_SET; lfnstIdx++)
      {
        canTestLFNST |= !m_ispTestedModes[lfnstIdx].splitIsFinished[ISPType::HOR]
                        || !m_ispTestedModes[lfnstIdx].splitIsFinished[ISPType::VER];
      }
      if (canTestLFNST)
      {
        //Construct the intra modes candidates list for the lfnst > 0 cases
        xSortISPCandListLFNST();
      }
    }
  }
}

