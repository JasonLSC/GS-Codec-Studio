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

/** \file     EncModeCtrl.cpp
    \brief    Encoder controller for trying out specific modes
*/

#include "EncModeCtrl.h"

#include "EncLib.h"
#include "AQp.h"
#include "RateCtrl.h"

#include "CommonLib/RdCost.h"
#include "CommonLib/CodingStructure.h"
#include "CommonLib/Picture.h"
#include "CommonLib/UnitTools.h"

#include "CommonLib/dtrace_next.h"

#include <cmath>

static constexpr double UNSET_IMV_COST = MAX_DOUBLE * 0.125;   // Some large, unique value

void EncModeCtrl::init( EncCfg *pCfg, RateCtrl *pRateCtrl, RdCost* pRdCost )
{
  m_pcEncCfg      = pCfg;
  m_pcRateCtrl    = pRateCtrl;
  m_pcRdCost      = pRdCost;
  m_fastDeltaQP   = false;
#if SHARP_LUMA_DELTA_QP
  m_lumaQPOffset  = 0;

  initLumaDeltaQpLUT();
#endif
  m_useHashMeInCurrentIntraPeriod = m_pcEncCfg->getUseHashMECfgEnable();
  m_HashMEPOC = 0;
  m_HashMEPOCchecked = false;
  m_HashMEPOC2 = 0;
}

bool EncModeCtrl::tryModeMaster( const EncTestMode& encTestmode, const CodingStructure &cs, Partitioner& partitioner )
{
  return tryMode( encTestmode, cs, partitioner );
}

void EncModeCtrl::setEarlySkipDetected()
{
  m_ComprCUCtxList.back().earlySkip = true;
}

void EncModeCtrl::xExtractFeatures( const EncTestMode encTestmode, CodingStructure& cs )
{
  CHECK( cs.features.size() < NUM_ENC_FEATURES, "Features vector is not initialized" );

  cs.features[ENC_FT_DISTORTION     ] = double( cs.dist              );
  cs.features[ENC_FT_FRAC_BITS      ] = double( cs.fracBits          );
  cs.features[ENC_FT_RD_COST        ] = double( cs.cost              );
  cs.features[ENC_FT_ENC_MODE_TYPE  ] = double( encTestmode.type     );
  cs.features[ENC_FT_ENC_MODE_OPTS  ] = double( encTestmode.opts     );
}

bool EncModeCtrl::nextMode( const CodingStructure &cs, Partitioner &partitioner )
{
  m_ComprCUCtxList.back().lastTestMode = m_ComprCUCtxList.back().testModes.back();

  m_ComprCUCtxList.back().testModes.pop_back();

  while( !m_ComprCUCtxList.back().testModes.empty() && !tryModeMaster( currTestMode(), cs, partitioner ) )
  {
    m_ComprCUCtxList.back().testModes.pop_back();
  }

  return !m_ComprCUCtxList.back().testModes.empty();
}

EncTestMode EncModeCtrl::currTestMode() const
{
  return m_ComprCUCtxList.back().testModes.back();
}

EncTestMode EncModeCtrl::lastTestMode() const
{
  return m_ComprCUCtxList.back().lastTestMode;
}

bool EncModeCtrl::anyMode() const
{
  return !m_ComprCUCtxList.back().testModes.empty();
}

void EncModeCtrl::setBest( CodingStructure& cs )
{
  if( cs.cost != MAX_DOUBLE && !cs.cus.empty() )
  {
    m_ComprCUCtxList.back().bestCS = &cs;
    m_ComprCUCtxList.back().bestCU = cs.cus[0];
    m_ComprCUCtxList.back().bestTU = cs.cus[0]->firstTU;
    m_ComprCUCtxList.back().lastTestMode = getCSEncMode( cs );
  }
}

void EncModeCtrl::xGetMinMaxQP( int& minQP, int& maxQP, const CodingStructure& cs, const Partitioner &partitioner, const int baseQP, const SPS& sps, const PPS& pps, const PartSplit splitMode )
{
  const EncType encType = dynamic_cast<const EncLib*>(m_pcEncCfg)->getEncType();
  if (m_pcEncCfg->getDPF() && encType == ENC_FULL && cs.slice->getSliceType() != I_SLICE)
  {
    minQP = m_qpCtu;
    maxQP = m_qpCtu;
    return;
  }

  if( m_pcEncCfg->getUseRateCtrl() )
  {
    minQP = m_pcRateCtrl->getRCQP();
    maxQP = m_pcRateCtrl->getRCQP();
    return;
  }

  const unsigned subdivIncr = (splitMode == CU_QUAD_SPLIT) ? 2 : (splitMode == CU_BT_SPLIT) ? 1 : 0;
  const bool qgEnable = partitioner.currQgEnable(); // QG possible at current level
  const bool qgEnableChildren = qgEnable && ((partitioner.currSubdiv + subdivIncr) <= cs.slice->getCuQpDeltaSubdiv()) && (subdivIncr > 0); // QG possible at next level
  const bool isLeafQG = (qgEnable && !qgEnableChildren);

  if( isLeafQG ) // QG at deepest level
  {
    int deltaQP = m_pcEncCfg->getMaxDeltaQP();
    minQP               = Clip3(-sps.getQpBDOffset(ChannelType::LUMA), MAX_QP, baseQP - deltaQP);
    maxQP               = Clip3(-sps.getQpBDOffset(ChannelType::LUMA), MAX_QP, baseQP + deltaQP);
    Position pos = partitioner.currQgPos;
    const int ctuSize = sps.getCTUSize();
    const int ctuId = ( pos.y / ctuSize ) * ( ( cs.picture->lwidth() + ctuSize - 1 ) / ctuSize ) + ( pos.x / ctuSize );
    const int bimOffset = getBIMOffset( m_slice->getPOC(), ctuId );
    minQP += bimOffset;
    maxQP += bimOffset;
  }
  else if( qgEnableChildren ) // more splits and not the deepest QG level
  {
    minQP = baseQP;
    maxQP = baseQP;
  }
  else // deeper than QG
  {
    minQP = cs.currQP[partitioner.chType];
    maxQP = minQP;
  }
}


int EncModeCtrl::xComputeDQP( const CodingStructure &cs, const Partitioner &partitioner )
{
  const Picture *picture = cs.picture;

  const unsigned  aqDepth = std::min(partitioner.currSubdiv / 2, (uint32_t) picture->aqlayer.size() - 1);
  const AQpLayer *aqLayer = picture->aqlayer[aqDepth];

  const double maxQpScale   = pow(2.0, m_pcEncCfg->getQPAdaptationRange() / 6.0);
  const double avgActivity  = aqLayer->getAvgActivity();
  const double cuActivity   = aqLayer->getActivity(cs.area.Y().topLeft());
  const double normActivity = (maxQpScale * cuActivity + avgActivity) / (cuActivity + maxQpScale * avgActivity);
  const double qpOffset     = std::log2(normActivity) * 6.0;

  return int(floor(qpOffset + 0.49999));
}


#if SHARP_LUMA_DELTA_QP
void EncModeCtrl::initLumaDeltaQpLUT()
{
  const LumaLevelToDeltaQPMapping &mapping = m_pcEncCfg->getLumaLevelToDeltaQPMapping();

  if( !mapping.isEnabled() )
  {
    return;
  }

  // map the sparse LumaLevelToDeltaQPMapping.mapping to a fully populated linear table.

  int         lastDeltaQPValue = 0;
  std::size_t nextSparseIndex = 0;
  for( int index = 0; index < LUMA_LEVEL_TO_DQP_LUT_MAXSIZE; index++ )
  {
    while( nextSparseIndex < mapping.mapping.size() && index >= mapping.mapping[nextSparseIndex].first )
    {
      lastDeltaQPValue = mapping.mapping[nextSparseIndex].second;
      nextSparseIndex++;
    }
    m_lumaLevelToDeltaQPLUT[index] = lastDeltaQPValue;
  }
}

int EncModeCtrl::calculateLumaDQP( const CPelBuf& rcOrg )
{
  double avg = 0;

  // Get QP offset derived from Luma level
#if !WCG_EXT
  if( m_pcEncCfg->getLumaLevelToDeltaQPMapping().mode == LUMALVL_TO_DQP_AVG_METHOD )
#else
  CHECK( m_pcEncCfg->getLumaLevelToDeltaQPMapping().mode != LUMALVL_TO_DQP_AVG_METHOD, "invalid delta qp mode" );
#endif
  {
    // Use average luma value
    avg = (double) rcOrg.computeAvg();
  }
#if !WCG_EXT
  else
  {
    // Use maximum luma value
    int maxVal = 0;
    for( uint32_t y = 0; y < rcOrg.height; y++ )
    {
      for( uint32_t x = 0; x < rcOrg.width; x++ )
      {
        const Pel& v = rcOrg.at( x, y );
        if( v > maxVal )
        {
          maxVal = v;
        }
      }
    }
    // use a percentage of the maxVal
    avg = ( double ) maxVal * m_pcEncCfg->getLumaLevelToDeltaQPMapping().maxMethodWeight;
  }
#endif
  int lumaBD     = m_pcEncCfg->getBitDepth(ChannelType::LUMA);
  int lumaIdxOrg = Clip3<int>(0, int(1 << lumaBD) - 1, int(avg + 0.5));
  int lumaIdx = lumaBD < 10 ? lumaIdxOrg << (10 - lumaBD) : lumaBD > 10 ? lumaIdxOrg >> (lumaBD - 10) : lumaIdxOrg;
  int QP = m_lumaLevelToDeltaQPLUT[lumaIdx];
  return QP;
}
#endif

int EncModeCtrl::calculateLumaDQPsmooth(const CPelBuf& rcOrg, int baseQP, double threshold, double scale, double offset, int limit)
{
  double avg = 0;
  double diff = 0;
  double thr = (double)threshold*rcOrg.height*rcOrg.width;
  int qp = 0;
  if (rcOrg.height >= 64 && rcOrg.width >= 64)
  {
    const int numBasis = 6;

    double invb[numBasis][numBasis] = { {0.001*0.244140625000000,                         0,                         0,                        0,                        0,                        0},
                                      {                      0,   0.001*0.013204564833946,   0.001*0.002080251479290, -0.001*0.000066039729501, -0.001*0.000165220364313,        0.000000000000000},
                                      {                      0,   0.001*0.002080251479290,   0.001*0.013204564833946, -0.001*0.000066039729501,        0.000000000000000, -0.001*0.000165220364313},
                                      {                      0,  -0.001*0.000066039729501,  -0.001*0.000066039729501,  0.001*0.000002096499349,        0.000000000000000,        0.000000000000000},
                                      {                      0,  -0.001*0.000165220364313,         0.000000000000000,        0.000000000000000,  0.001*0.000002622545465,        0.000000000000000},
                                      {                      0,         0.000000000000000,  -0.001*0.000165220364313,        0.000000000000000,        0.000000000000000,  0.001*0.000002622545465} };
    double boffset[5] = { -31.5, -31.5, -992.25, -1333.5, -1333.5 };

    int listQuadrantsX[4] = { 0, 64, 0, 64 };
    int listQuadrantsY[4] = { 0, 0, 64, 64 };

    double b1sum;
    double b2sum;
    double b3sum;
    double b4sum;
    double b5sum;
    double b6sum;
    int numQuadrantsX = (rcOrg.width == 128) ? 2 : 1;
    int numQuadrantsY = (rcOrg.height == 128) ? 2 : 1;
    //loop over quadrants
    for (int posy = 0; posy < numQuadrantsY; posy++)
    {
      for (int posx = 0; posx < numQuadrantsX; posx++)
      {
        b2sum = 0.0;
        b3sum = 0.0;
        b4sum = 0.0;
        b5sum = 0.0;
        b6sum = 0.0;
        avg = 0.0;
        for (uint32_t y = 0; y < 64; y++)
        {
          for (uint32_t x = 0; x < 64; x++)
          {
            const Pel& v = rcOrg.at(x + listQuadrantsX[posx + 2 * posy], y + listQuadrantsY[posx + 2 * posy]);
            b2sum += ((double)v)*((double)x + boffset[0]);
            b3sum += ((double)v)*((double)y + boffset[1]);
            b4sum += ((double)v)*((double)x*(double)y + boffset[2]);
            b5sum += ((double)v)*((double)x*(double)x + boffset[3]);
            b6sum += ((double)v)*((double)y*(double)y + boffset[4]);
            avg += (double)v;
          }
        }
        b1sum = avg;
        double r[numBasis];
        for (uint32_t b = 0; b < numBasis; b++)
        {
          r[b] = invb[b][0] * b1sum + invb[b][1] * b2sum + invb[b][2] * b3sum + invb[b][3] * b4sum + invb[b][4] * b5sum + invb[b][5] * b6sum;
        }
        // compute SAD for model
        for (uint32_t y = 0; y < 64; y++)
        {
          for (uint32_t x = 0; x < 64; x++)
          {
            const Pel& v = rcOrg.at(x + listQuadrantsX[posx + 2 * posy], y + listQuadrantsY[posx + 2 * posy]);

            diff += abs((int)v - (int)(r[0] + r[1] * ((double)x + boffset[0]) + r[2] * ((double)y + boffset[1]) + r[3] * ((double)x*(double)y + boffset[2]) + r[4] * ((double)x*(double)x + boffset[3]) + r[5] * ((double)y*(double)y + boffset[4])));
          }
        }
      }
    }
    if (diff < thr)
    {
      qp = std::max(limit, std::min(0, (int) (scale * (double) baseQP + offset)));
    }
  }
  return qp;
}

void CacheBlkInfoCtrl::create(int maxCuSize)
{
  const unsigned numPos = maxCuSize >> MIN_CU_LOG2;
  m_maxCuSize = maxCuSize;

  m_numWidths  = gp_sizeIdxInfo->numWidths();
  m_numHeights = gp_sizeIdxInfo->numHeights();

  bool isLog2MttPartitioning = !!dynamic_cast<SizeIndexInfoLog2*>( gp_sizeIdxInfo );

  for( unsigned x = 0; x < numPos; x++ )
  {
    for( unsigned y = 0; y < numPos; y++ )
    {
      m_codedCUInfo[x][y] = new CodedCUInfo**[m_numWidths];

      for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
      {
        if( !( gp_sizeIdxInfo->isCuSize( gp_sizeIdxInfo->sizeFrom( wIdx ) ) && x + ( gp_sizeIdxInfo->sizeFrom( wIdx ) >> MIN_CU_LOG2 ) <= (maxCuSize >> MIN_CU_LOG2 ) ) )
        {
          m_codedCUInfo[x][y][wIdx] = nullptr;
          continue;
        }

        const int wLog2 = floorLog2( gp_sizeIdxInfo->sizeFrom( wIdx ) );

        if( isLog2MttPartitioning && ( ( x << MIN_CU_LOG2 ) & ( ( 1 << ( wLog2 - 1 ) ) - 1 ) ) != 0 )
        {
          m_codedCUInfo[x][y][wIdx] = nullptr;
          continue;
        }

        m_codedCUInfo[x][y][wIdx] = new CodedCUInfo*[gp_sizeIdxInfo->numHeights()];

        for( int hIdx = 0; hIdx < gp_sizeIdxInfo->numHeights(); hIdx++ )
        {
          if( !( gp_sizeIdxInfo->isCuSize( gp_sizeIdxInfo->sizeFrom( hIdx ) ) && y + ( gp_sizeIdxInfo->sizeFrom( hIdx ) >> MIN_CU_LOG2 ) <= (maxCuSize >> MIN_CU_LOG2 ) ) )
          {
            m_codedCUInfo[x][y][wIdx][hIdx] = nullptr;
            continue;
          }

          const int hLog2 = floorLog2( gp_sizeIdxInfo->sizeFrom( hIdx ) );

          if( isLog2MttPartitioning && ( ( ( y << MIN_CU_LOG2 ) & ( ( 1 << ( hLog2 - 1 ) ) - 1 ) ) != 0 ) )
          {
            m_codedCUInfo[x][y][wIdx][hIdx] = nullptr;
            continue;
          }

          m_codedCUInfo[x][y][wIdx][hIdx] = new CodedCUInfo;
        }
      }
    }
  }
}

void CacheBlkInfoCtrl::destroy()
{
  const unsigned numPos = m_maxCuSize >> MIN_CU_LOG2;

  for( unsigned x = 0; x < numPos; x++ )
  {
    for( unsigned y = 0; y < numPos; y++ )
    {
      for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
      {
        if( m_codedCUInfo[x][y][wIdx] )
        {
          for( int hIdx = 0; hIdx < gp_sizeIdxInfo->numHeights(); hIdx++ )
          {
            if( m_codedCUInfo[x][y][wIdx][hIdx] )
            {
              delete m_codedCUInfo[x][y][wIdx][hIdx];
            }
          }

          delete[] m_codedCUInfo[x][y][wIdx];
        }
      }

      delete[] m_codedCUInfo[x][y];
    }
  }
}

void CacheBlkInfoCtrl::init( const Slice &slice )
{
  const unsigned numPos = m_maxCuSize >> MIN_CU_LOG2;

  for( unsigned x = 0; x < numPos; x++ )
  {
    for( unsigned y = 0; y < numPos; y++ )
    {
      for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
      {
        if( m_codedCUInfo[x][y][wIdx] )
        {
          for( int hIdx = 0; hIdx < gp_sizeIdxInfo->numHeights(); hIdx++ )
          {
            if( m_codedCUInfo[x][y][wIdx][hIdx] )
            {
              std::fill_n(reinterpret_cast<char *>(m_codedCUInfo[x][y][wIdx][hIdx]), sizeof(CodedCUInfo), 0);
            }
          }
        }
      }
    }
  }

  m_slice_chblk = &slice;
}

CodedCUInfo& CacheBlkInfoCtrl::getBlkInfo( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return *m_codedCUInfo[idx1][idx2][idx3][idx4];
}

bool CacheBlkInfoCtrl::isSkip( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return m_codedCUInfo[idx1][idx2][idx3][idx4]->isSkip;
}

char CacheBlkInfoCtrl::getSelectColorSpaceOption(const UnitArea& area)
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx(area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4);

  return m_codedCUInfo[idx1][idx2][idx3][idx4]->selectColorSpaceOption;
}

bool CacheBlkInfoCtrl::isMMVDSkip(const UnitArea& area)
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx(area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4);

  return m_codedCUInfo[idx1][idx2][idx3][idx4]->isMMVDSkip;
}

void CacheBlkInfoCtrl::setMv(const UnitArea &area, const RefPicList refPicList, const int refIdx, const Mv &rMv)
{
  if (refIdx >= MAX_STORED_CU_INFO_REFS)
    return;

  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  m_codedCUInfo[idx1][idx2][idx3][idx4]->saveMv[refPicList][refIdx]  = rMv;
  m_codedCUInfo[idx1][idx2][idx3][idx4]->validMv[refPicList][refIdx] = true;
}

bool CacheBlkInfoCtrl::getMv(const UnitArea &area, const RefPicList refPicList, const int refIdx, Mv &rMv) const
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  if (refIdx >= MAX_STORED_CU_INFO_REFS)
  {
    rMv = m_codedCUInfo[idx1][idx2][idx3][idx4]->saveMv[refPicList][0];
    return false;
  }

  rMv = m_codedCUInfo[idx1][idx2][idx3][idx4]->saveMv[refPicList][refIdx];
  return m_codedCUInfo[idx1][idx2][idx3][idx4]->validMv[refPicList][refIdx];
}

void SaveLoadEncInfoSbt::init( const Slice &slice )
{
  m_sliceSbt = &slice;
}

void SaveLoadEncInfoSbt::create(int maxCuSize)
{
  int numSizeIdx = gp_sizeIdxInfo->idxFrom( SBT_MAX_SIZE ) - MIN_CU_LOG2 + 1;
  int numPosIdx = maxCuSize >> MIN_CU_LOG2;
  m_maxCuSize = maxCuSize;

  m_saveLoadSbt = new SaveLoadStructSbt***[numPosIdx];

  for( int xIdx = 0; xIdx < numPosIdx; xIdx++ )
  {
    m_saveLoadSbt[xIdx] = new SaveLoadStructSbt**[numPosIdx];
    for( int yIdx = 0; yIdx < numPosIdx; yIdx++ )
    {
      m_saveLoadSbt[xIdx][yIdx] = new SaveLoadStructSbt*[numSizeIdx];
      for( int wIdx = 0; wIdx < numSizeIdx; wIdx++ )
      {
        m_saveLoadSbt[xIdx][yIdx][wIdx] = new SaveLoadStructSbt[numSizeIdx];
      }
    }
  }
}

void SaveLoadEncInfoSbt::destroy()
{
  int numSizeIdx = gp_sizeIdxInfo->idxFrom( SBT_MAX_SIZE ) - MIN_CU_LOG2 + 1;
  int numPosIdx = m_maxCuSize >> MIN_CU_LOG2;

  for( int xIdx = 0; xIdx < numPosIdx; xIdx++ )
  {
    for( int yIdx = 0; yIdx < numPosIdx; yIdx++ )
    {
      for( int wIdx = 0; wIdx < numSizeIdx; wIdx++ )
      {
        delete[] m_saveLoadSbt[xIdx][yIdx][wIdx];
      }
      delete[] m_saveLoadSbt[xIdx][yIdx];
    }
    delete[] m_saveLoadSbt[xIdx];
  }
  delete[] m_saveLoadSbt;
}

SaveLoadEncInfoSbt::BestSbt SaveLoadEncInfoSbt::findBestSbt(const UnitArea &area, const uint32_t curPuSse)
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_sliceSbt->getPPS()->pcv, idx1, idx2, idx3, idx4 );
  SaveLoadStructSbt* pSbtSave = &m_saveLoadSbt[idx1][idx2][idx3 - MIN_CU_LOG2][idx4 - MIN_CU_LOG2];

  for( int i = 0; i < pSbtSave->numPuInfoStored; i++ )
  {
    if( curPuSse == pSbtSave->puSse[i] )
    {
      return { pSbtSave->puSbt[i], pSbtSave->puTrs[i] };
    }
  }

  return { MAX_UCHAR, MtsType::NONE };
}

bool SaveLoadEncInfoSbt::saveBestSbt(const UnitArea &area, const uint32_t curPuSse, const uint8_t curPuSbt,
                                     const MtsType curPuTrs)
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_sliceSbt->getPPS()->pcv, idx1, idx2, idx3, idx4 );
  SaveLoadStructSbt* pSbtSave = &m_saveLoadSbt[idx1][idx2][idx3 - MIN_CU_LOG2][idx4 - MIN_CU_LOG2];

  if( pSbtSave->numPuInfoStored == SBT_NUM_SL )
  {
    return false;
  }

  pSbtSave->puSse[pSbtSave->numPuInfoStored] = curPuSse;
  pSbtSave->puSbt[pSbtSave->numPuInfoStored] = curPuSbt;
  pSbtSave->puTrs[pSbtSave->numPuInfoStored] = curPuTrs;
  pSbtSave->numPuInfoStored++;
  return true;
}

void SaveLoadEncInfoSbt::resetSaveloadSbt( int maxSbtSize )
{
  int numSizeIdx = gp_sizeIdxInfo->idxFrom( maxSbtSize ) - MIN_CU_LOG2 + 1;
  int numPosIdx = m_maxCuSize >> MIN_CU_LOG2;

  for( int xIdx = 0; xIdx < numPosIdx; xIdx++ )
  {
    for( int yIdx = 0; yIdx < numPosIdx; yIdx++ )
    {
      for( int wIdx = 0; wIdx < numSizeIdx; wIdx++ )
      {
        memset( m_saveLoadSbt[xIdx][yIdx][wIdx], 0, numSizeIdx * sizeof( SaveLoadStructSbt ) );
      }
    }
  }
}

bool CacheBlkInfoCtrl::getInter(const UnitArea& area)
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx(area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4);

  return m_codedCUInfo[idx1][idx2][idx3][idx4]->isInter;
}

void CacheBlkInfoCtrl::setBcwIdx(const UnitArea& area, uint8_t gBiIdx)
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx(area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4);

  m_codedCUInfo[idx1][idx2][idx3][idx4]->bcwIdx = gBiIdx;
}

uint8_t CacheBlkInfoCtrl::getBcwIdx(const UnitArea& area)
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx(area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4);

  return m_codedCUInfo[idx1][idx2][idx3][idx4]->bcwIdx;
}

#if REUSE_CU_RESULTS
static bool isTheSameNbHood( const CodingUnit &cu, const CodingStructure& cs, const Partitioner &partitioner
                            , const PredictionUnit &pu, int picW, int picH
                           )
{
  if( cu.chType != partitioner.chType )
  {
    return false;
  }

  const PartitioningStack &ps = partitioner.getPartStack();

  int i = 1;

  for( ; i < ps.size(); i++ )
  {
    if( ps[i].split != CU::getSplitAtDepth( cu, i - 1 ) )
    {
      break;
    }
  }

  const UnitArea &cmnAnc = ps[i - 1].parts[ps[i - 1].idx];
  const UnitArea cuArea  = CS::getArea( cs, cu, partitioner.chType );
//#endif

  for( int i = 0; i < cmnAnc.blocks.size(); i++ )
  {
    if( i < cuArea.blocks.size() && cuArea.blocks[i].valid() && cuArea.blocks[i].pos() != cmnAnc.blocks[i].pos() )
    {
      return false;
    }
  }

  return true;
}

void BestEncInfoCache::create( const ChromaFormat chFmt, int maxCuSize)
{
  const unsigned numPos = maxCuSize >> MIN_CU_LOG2;
  m_maxCuSize = maxCuSize;

  m_numWidths  = gp_sizeIdxInfo->numWidths();
  m_numHeights = gp_sizeIdxInfo->numHeights();

  bool isLog2MttPartitioning = !!dynamic_cast<SizeIndexInfoLog2*>( gp_sizeIdxInfo );

  for( unsigned x = 0; x < numPos; x++ )
  {
    for( unsigned y = 0; y < numPos; y++ )
    {
      m_bestEncInfo[x][y] = new BestEncodingInfo**[m_numWidths];

      for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
      {
        if( !( gp_sizeIdxInfo->isCuSize( gp_sizeIdxInfo->sizeFrom( wIdx ) ) && x + ( gp_sizeIdxInfo->sizeFrom( wIdx ) >> MIN_CU_LOG2 ) <= (m_maxCuSize >> MIN_CU_LOG2 ) ) )
        {
          m_bestEncInfo[x][y][wIdx] = nullptr;
          continue;
        }

        const int wLog2 = floorLog2( gp_sizeIdxInfo->sizeFrom( wIdx ) );

        if( isLog2MttPartitioning && ( ( x << MIN_CU_LOG2 ) & ( ( 1 << ( wLog2 - 1 ) ) - 1 ) ) != 0 )
        {
          m_bestEncInfo[x][y][wIdx] = nullptr;
          continue;
        }

        m_bestEncInfo[x][y][wIdx] = new BestEncodingInfo*[gp_sizeIdxInfo->numHeights()];

        for( int hIdx = 0; hIdx < gp_sizeIdxInfo->numHeights(); hIdx++ )
        {
          if( !( gp_sizeIdxInfo->isCuSize( gp_sizeIdxInfo->sizeFrom( hIdx ) ) && y + ( gp_sizeIdxInfo->sizeFrom( hIdx ) >> MIN_CU_LOG2 ) <= (m_maxCuSize >> MIN_CU_LOG2 ) ) )
          {
            m_bestEncInfo[x][y][wIdx][hIdx] = nullptr;
            continue;
          }

          const int hLog2 = floorLog2( gp_sizeIdxInfo->sizeFrom( hIdx ) );

          if( isLog2MttPartitioning && ( ( ( y << MIN_CU_LOG2 ) & ( ( 1 << ( hLog2 - 1 ) ) - 1 ) ) != 0 ) )
          {
            m_bestEncInfo[x][y][wIdx][hIdx] = nullptr;
            continue;
          }

          m_bestEncInfo[x][y][wIdx][hIdx] = new BestEncodingInfo;

          int w = gp_sizeIdxInfo->sizeFrom( wIdx );
          int h = gp_sizeIdxInfo->sizeFrom( hIdx );

          const UnitArea area( chFmt, Area( 0, 0, w, h ) );

          new ( &m_bestEncInfo[x][y][wIdx][hIdx]->cu ) CodingUnit    ( area );
          new ( &m_bestEncInfo[x][y][wIdx][hIdx]->pu ) PredictionUnit( area );
#if REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
          m_bestEncInfo[x][y][wIdx][hIdx]->numTus = 0;
          for( int i = 0; i < MAX_NUM_TUS; i++ )
          {
            new ( &m_bestEncInfo[x][y][wIdx][hIdx]->tus[i] ) TransformUnit( area );
          }
#else
          new ( &m_bestEncInfo[x][y][wIdx][hIdx]->tu ) TransformUnit( area );
#endif

          m_bestEncInfo[x][y][wIdx][hIdx]->poc      = -1;
          m_bestEncInfo[x][y][wIdx][hIdx]->testMode = EncTestMode();
        }
      }
    }
  }
}

void BestEncInfoCache::destroy()
{
  const unsigned numPos = m_maxCuSize >> MIN_CU_LOG2;

  for( unsigned x = 0; x < numPos; x++ )
  {
    for( unsigned y = 0; y < numPos; y++ )
    {
      for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
      {
        if( m_bestEncInfo[x][y][wIdx] )
        {
          for( int hIdx = 0; hIdx < gp_sizeIdxInfo->numHeights(); hIdx++ )
          {
            if( m_bestEncInfo[x][y][wIdx][hIdx] )
            {
              delete m_bestEncInfo[x][y][wIdx][hIdx];
            }
          }

          delete[] m_bestEncInfo[x][y][wIdx];
        }
      }

      delete[] m_bestEncInfo[x][y];
    }
  }

  delete[] m_pCoeff;
  delete[] m_pPcmBuf;

  if (m_runType != nullptr)
  {
    delete[] m_runType;
    m_runType = nullptr;
  }
}

void BestEncInfoCache::init( const Slice &slice )
{
  bool isInitialized = m_slice_bencinf;

  m_slice_bencinf = &slice;

  if (isInitialized)
  {
    if (slice.getSliceQp() != m_sliceQp)
    {
      const unsigned numPos = m_maxCuSize >> MIN_CU_LOG2;
      for (unsigned x = 0; x < numPos; x++)
      {
        for (unsigned y = 0; y < numPos; y++)
        {
          for (int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++)
          {
            if (m_bestEncInfo[x][y][wIdx] != nullptr)
            {
              for (int hIdx = 0; hIdx < gp_sizeIdxInfo->numHeights(); hIdx++)
              {
                if (m_bestEncInfo[x][y][wIdx][hIdx] != nullptr)
                {
                  m_bestEncInfo[x][y][wIdx][hIdx]->cu.qp = -128;
                }
              }
            }
          }
        }
      }
      m_sliceQp = slice.getSliceQp();
    }
    return;
  }

  const unsigned numPos = slice.getSPS()->getMaxCUWidth() >> MIN_CU_LOG2;

  m_numWidths  = gp_sizeIdxInfo->numWidths();
  m_numHeights = gp_sizeIdxInfo->numHeights();

  size_t numCoeff = 0;

  for( unsigned x = 0; x < numPos; x++ )
  {
    for( unsigned y = 0; y < numPos; y++ )
    {
      for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
      {
        if( m_bestEncInfo[x][y][wIdx] ) for( int hIdx = 0; hIdx < gp_sizeIdxInfo->numHeights(); hIdx++ )
        {
          if( m_bestEncInfo[x][y][wIdx][hIdx] )
          {
            for( const CompArea& blk : m_bestEncInfo[x][y][wIdx][hIdx]->cu.blocks )
            {
              numCoeff += blk.area();
            }
          }
        }
      }
    }
  }

#if REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
  m_pCoeff  = new TCoeff[numCoeff*MAX_NUM_TUS];
  m_pPcmBuf = new Pel   [numCoeff*MAX_NUM_TUS];
  if (slice.getSPS()->getPLTMode())
  {
    m_runType = new PLTRunMode[numCoeff * MAX_NUM_TUS];
  }
#else
  m_pCoeff  = new TCoeff[numCoeff];
  m_pPcmBuf = new Pel   [numCoeff];
  if (slice.getSPS()->getPLTMode())
  {
    m_runType = new PLTRunMode[numCoeff];
  }
#endif

  TCoeff *coeffPtr = m_pCoeff;
  Pel    *pcmPtr   = m_pPcmBuf;
  PLTRunMode* runTypePtr = m_runType;
  m_dummyCS.pcv = m_slice_bencinf->getPPS()->pcv;

  for( unsigned x = 0; x < numPos; x++ )
  {
    for( unsigned y = 0; y < numPos; y++ )
    {
      for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
      {
        if( m_bestEncInfo[x][y][wIdx] ) for( int hIdx = 0; hIdx < gp_sizeIdxInfo->numHeights(); hIdx++ )
        {
          if( m_bestEncInfo[x][y][wIdx][hIdx] )
          {
            TCoeff *coeff[MAX_NUM_TBLOCKS] = { 0, };
            Pel    *pcmbf[MAX_NUM_TBLOCKS] = { 0, };
            EnumArray<PLTRunMode*, ChannelType> runType;
            runType.fill(nullptr);

#if REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
            for( int i = 0; i < MAX_NUM_TUS; i++ )
            {
              TransformUnit &tu = m_bestEncInfo[x][y][wIdx][hIdx]->tus[i];
              const UnitArea &area = tu;

              for( int i = 0; i < area.blocks.size(); i++ )
              {
                coeff[i] = coeffPtr; coeffPtr += area.blocks[i].area();
                pcmbf[i] = pcmPtr;   pcmPtr += area.blocks[i].area();
                const auto        compId = ComponentID(i);
                const ChannelType chType = toChannelType(compId);
                if (compId == getFirstComponentOfChannel(chType) && runTypePtr != nullptr)
                {
                  runType[chType] = runTypePtr;
                  runTypePtr += area.blocks[i].area();
                }
              }

              tu.cs = &m_dummyCS;
              tu.init(coeff, pcmbf, runType);
            }
#else
            const UnitArea &area = m_bestEncInfo[x][y][wIdx][hIdx]->tu;

            for( int i = 0; i < area.blocks.size(); i++ )
            {
              coeff[i] = coeffPtr; coeffPtr += area.blocks[i].area();
              pcmbf[i] =   pcmPtr;   pcmPtr += area.blocks[i].area();
              runType[i] = runTypePtr;     runTypePtr += area.blocks[i].area();
              runLength[i] = runLengthPtr; runLengthPtr += area.blocks[i].area();
            }

            m_bestEncInfo[x][y][wIdx][hIdx]->tu.cs = &m_dummyCS;
            m_bestEncInfo[x][y][wIdx][hIdx]->tu.init(coeff, pcmbf, runLength, runType);
#endif
          }
        }
      }
    }
  }
}

bool BestEncInfoCache::setFromCs( const CodingStructure& cs, const Partitioner& partitioner )
{
#if REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
  if( cs.cus.size() != 1 || cs.pus.size() != 1 )
#else
  if( cs.cus.size() != 1 || cs.tus.size() != 1 || cs.pus.size() != 1 )
#endif
  {
    return false;
  }

  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( cs.area.Y(), *m_slice_bencinf->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  BestEncodingInfo& encInfo = *m_bestEncInfo[idx1][idx2][idx3][idx4];

  encInfo.poc            =  cs.picture->poc;
  encInfo.cu.repositionTo( *cs.cus.front() );
  encInfo.pu.repositionTo( *cs.pus.front() );
#if !REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
  encInfo.tu.repositionTo( *cs.tus.front() );
#endif
  encInfo.cu             = *cs.cus.front();
  encInfo.pu             = *cs.pus.front();
#if REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
  int tuIdx = 0;
  for( auto tu : cs.tus )
  {
    encInfo.tus[tuIdx].repositionTo( *tu );
    encInfo.tus[tuIdx].resizeTo( *tu );
    for( auto &blk : tu->blocks )
    {
      if( blk.valid() )
      {
        encInfo.tus[tuIdx].copyComponentFrom( *tu, blk.compID );
      }
    }
    tuIdx++;
  }
  CHECKD( cs.tus.size() > MAX_NUM_TUS, "Exceeding tus array boundaries" );
  encInfo.numTus = cs.tus.size();
#else
  for( auto &blk : cs.tus.front()->blocks )
  {
    if (blk.valid())
    {
      encInfo.tu.copyComponentFrom(*cs.tus.front(), blk.compID);
    }
  }
#endif
  encInfo.testMode       = getCSEncMode( cs );

  return true;
}

bool BestEncInfoCache::isValid( const CodingStructure& cs, const Partitioner& partitioner, int qp )
{
  if( partitioner.treeType == TREE_C )
  {
    return false; //if save & load is allowed for chroma CUs, we should check whether luma info (pred, recon, etc) is the same, which is quite complex
  }
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( cs.area.Y(), *m_slice_bencinf->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  BestEncodingInfo& encInfo = *m_bestEncInfo[idx1][idx2][idx3][idx4];

  if( encInfo.cu.treeType != partitioner.treeType || encInfo.cu.modeType != partitioner.modeType )
  {
    return false;
  }
  if( encInfo.cu.qp != qp || cs.slice->getUseChromaQpAdj())
  {
    return false;
  }
  if (cs.picture->poc != encInfo.poc
      || CS::getArea(cs, cs.area, partitioner.chType) != CS::getArea(cs, encInfo.cu, partitioner.chType)
      || !isTheSameNbHood(encInfo.cu, cs, partitioner, encInfo.pu, (cs.picture->Y().width), (cs.picture->Y().height))
      || CU::isIBC(encInfo.cu) || partitioner.currQgEnable() || cs.currQP[partitioner.chType] != encInfo.cu.qp)
  {
    return false;
  }
  else
  {
    return true;
  }
}

bool BestEncInfoCache::setCsFrom( CodingStructure& cs, EncTestMode& testMode, const Partitioner& partitioner ) const
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( cs.area.Y(), *m_slice_bencinf->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  BestEncodingInfo& encInfo = *m_bestEncInfo[idx1][idx2][idx3][idx4];

  if (cs.picture->poc != encInfo.poc
      || CS::getArea(cs, cs.area, partitioner.chType) != CS::getArea(cs, encInfo.cu, partitioner.chType)
      || !isTheSameNbHood(encInfo.cu, cs, partitioner, encInfo.pu, (cs.picture->Y().width), (cs.picture->Y().height))
      || partitioner.currQgEnable() || cs.currQP[partitioner.chType] != encInfo.cu.qp)
  {
    return false;
  }

  CodingUnit     &cu = cs.addCU( CS::getArea( cs, cs.area, partitioner.chType ), partitioner.chType );
  PredictionUnit &pu = cs.addPU( CS::getArea( cs, cs.area, partitioner.chType ), partitioner.chType );
#if !REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
  TransformUnit  &tu = cs.addTU( CS::getArea( cs, cs.area, partitioner.chType ), partitioner.chType );
#endif

  cu          .repositionTo( encInfo.cu );
  pu          .repositionTo( encInfo.pu );
#if !REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
  tu          .repositionTo( encInfo.tu );
#endif

  cu          = encInfo.cu;
  pu          = encInfo.pu;
#if REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
  CHECKD( !( encInfo.numTus > 0 ), "Empty tus array" );
  for( int i = 0; i < encInfo.numTus; i++ )
  {
    TransformUnit  &tu = cs.addTU( encInfo.tus[i], partitioner.chType );

    for( auto &blk : tu.blocks )
    {
      if( blk.valid() ) tu.copyComponentFrom( encInfo.tus[i], blk.compID );
    }
  }
#else
  for( auto &blk : tu.blocks )
  {
    if( blk.valid() ) tu.copyComponentFrom( encInfo.tu, blk.compID );
  }
#endif

  testMode    = encInfo.testMode;

  return true;
}

#endif

static bool interHadActive( const ComprCUCtx& ctx )
{
  return ctx.interHad != 0;
}

//////////////////////////////////////////////////////////////////////////
// EncModeCtrlQTBT
//////////////////////////////////////////////////////////////////////////

void EncModeCtrlMTnoRQT::create( const EncCfg& cfg )
{
#if GDR_ENABLED
  m_encCfg = cfg;
#endif
  CacheBlkInfoCtrl::create(cfg.getCTUSize());
#if REUSE_CU_RESULTS
  BestEncInfoCache::create( cfg.getChromaFormatIdc(), cfg.getCTUSize());
#endif
  SaveLoadEncInfoSbt::create(cfg.getCTUSize());
}

void EncModeCtrlMTnoRQT::destroy()
{
  CacheBlkInfoCtrl::destroy();
#if REUSE_CU_RESULTS
  BestEncInfoCache::destroy();
#endif
  SaveLoadEncInfoSbt::destroy();
}

void EncModeCtrlMTnoRQT::initCTUEncoding( const Slice &slice )
{
  CacheBlkInfoCtrl::init( slice );
#if REUSE_CU_RESULTS
  BestEncInfoCache::init( slice );
#endif
  SaveLoadEncInfoSbt::init( slice );

  CHECK( !m_ComprCUCtxList.empty(), "Mode list is not empty at the beginning of a CTU" );

  m_slice             = &slice;

  if( m_pcEncCfg->getUseE0023FastEnc() )
  {
    if (m_pcEncCfg->getUseCompositeRef())
    {
      m_skipThreshold = ( ( slice.getMinPictureDistance() <= PICTURE_DISTANCE_TH * 2 ) ? FAST_SKIP_DEPTH : SKIP_DEPTH );
    }
    else
    {
      m_skipThreshold = ((slice.getMinPictureDistance() <= PICTURE_DISTANCE_TH) ? FAST_SKIP_DEPTH : SKIP_DEPTH);
    }
  }
  else
  {
    m_skipThreshold = SKIP_DEPTH;
  }
}

void EncModeCtrlMTnoRQT::initCULevel( Partitioner &partitioner, const CodingStructure& cs )
{
  // Min/max depth
  unsigned minDepth = 0;
  unsigned maxDepth = floorLog2(cs.sps->getCTUSize()) - floorLog2(cs.sps->getMinQTSize( m_slice->getSliceType(), partitioner.chType ));
  if( m_pcEncCfg->getUseFastLCTU() )
  {
    if( auto adPartitioner = dynamic_cast<AdaptiveDepthPartitioner*>( &partitioner ) )
    {
      // LARGE CTU
      adPartitioner->setMaxMinDepth( minDepth, maxDepth, cs );
    }
  }

  m_ComprCUCtxList.push_back(ComprCUCtx(cs, minDepth, maxDepth));

  const CodingUnit *cuLeft  = cs.getCU(cs.area.block(partitioner.chType).pos().offset(-1, 0), partitioner.chType);
  const CodingUnit *cuAbove = cs.getCU(cs.area.block(partitioner.chType).pos().offset(0, -1), partitioner.chType);

  const bool qtBeforeBt =
    ((cuLeft && cuAbove && cuLeft->qtDepth > partitioner.currQtDepth && cuAbove->qtDepth > partitioner.currQtDepth)
     || (cuLeft && !cuAbove && cuLeft->qtDepth > partitioner.currQtDepth)
     || (!cuLeft && cuAbove && cuAbove->qtDepth > partitioner.currQtDepth)
     || (!cuAbove && !cuLeft && cs.area.lwidth() >= (32 << cs.slice->getHierPredLayerIdx())))
    && (cs.area.lwidth() > (cs.pcv->getMinQtSize(*cs.slice, partitioner.chType) << 1));

  // set features
  ComprCUCtx &cuECtx  = m_ComprCUCtxList.back();
  cuECtx.set( BEST_NON_SPLIT_COST,  MAX_DOUBLE );
  cuECtx.set( BEST_VERT_SPLIT_COST, MAX_DOUBLE );
  cuECtx.set( BEST_HORZ_SPLIT_COST, MAX_DOUBLE );
  cuECtx.set( BEST_TRIH_SPLIT_COST, MAX_DOUBLE );
  cuECtx.set( BEST_TRIV_SPLIT_COST, MAX_DOUBLE );
  cuECtx.set( DO_TRIH_SPLIT,        1 );
  cuECtx.set( DO_TRIV_SPLIT,        1 );
  cuECtx.set(BEST_IMV_COST, UNSET_IMV_COST);
  cuECtx.set(BEST_NO_IMV_COST, UNSET_IMV_COST);
  cuECtx.set( QT_BEFORE_BT,         qtBeforeBt );
  cuECtx.set( DID_QUAD_SPLIT,       false );
  cuECtx.set( IS_BEST_NOSPLIT_SKIP, false );
  cuECtx.set( MAX_QT_SUB_DEPTH,     0 );

  // QP
  int baseQP = cs.baseQP;
  if (!partitioner.isSepTree(cs) || isLuma(partitioner.chType))
  {
    if (m_pcEncCfg->getUseAdaptiveQP())
    {
      baseQP = Clip3(-cs.sps->getQpBDOffset(ChannelType::LUMA), MAX_QP, baseQP + xComputeDQP(cs, partitioner));
    }
#if ENABLE_QPA_SUB_CTU
    else if (m_pcEncCfg->getUsePerceptQPA() && !m_pcEncCfg->getUseRateCtrl() && cs.pps->getUseDQP() && cs.slice->getCuQpDeltaSubdiv() > 0)
    {
      const PreCalcValues &pcv = *cs.pcv;

      if ((partitioner.currArea().lwidth() < pcv.maxCUWidth) && (partitioner.currArea().lheight() < pcv.maxCUHeight) && cs.picture)
      {
        const Position    &pos = partitioner.currQgPos;
        const unsigned mtsLog2 = (unsigned)floorLog2(std::min (cs.sps->getMaxTbSize(), pcv.maxCUWidth));
        const unsigned  stride = pcv.maxCUWidth >> mtsLog2;

        baseQP = cs.picture->m_subCtuQP[((pos.x & pcv.maxCUWidthMask) >> mtsLog2) + stride * ((pos.y & pcv.maxCUHeightMask) >> mtsLog2)];
      }
    }
#endif
#if SHARP_LUMA_DELTA_QP
    if (m_pcEncCfg->getLumaLevelToDeltaQPMapping().isEnabled())
    {
      if (partitioner.currQgEnable())
      {
        m_lumaQPOffset = calculateLumaDQP (cs.getOrgBuf (clipArea (cs.area.Y(), cs.picture->Y())));
      }
      baseQP = Clip3(-cs.sps->getQpBDOffset(ChannelType::LUMA), MAX_QP, baseQP - m_lumaQPOffset);
    }
#endif
    if (m_pcEncCfg->getSmoothQPReductionEnable())
    {
      int smoothQPoffset = 0;
      if (partitioner.currQgEnable())
      {
        // enable smooth QP reduction on selected frames
        bool checkSmoothQP = false;
        if (m_pcEncCfg->getSmoothQPReductionPeriodicity() != 0)
        {
          checkSmoothQP = ((m_pcEncCfg->getSmoothQPReductionPeriodicity() == 0) && cs.slice->isIntra()) || (m_pcEncCfg->getSmoothQPReductionPeriodicity() == 1) || ((cs.slice->getPOC() % m_pcEncCfg->getSmoothQPReductionPeriodicity()) == 0);
        }
        else
        {
          checkSmoothQP = ((m_pcEncCfg->getSmoothQPReductionPeriodicity() == 0) && cs.slice->isIntra());
        }
        if (checkSmoothQP)
        {
          bool isIntraSlice = cs.slice->isIntra();
          if (isIntraSlice)
          {
            smoothQPoffset = calculateLumaDQPsmooth(cs.getOrgBuf(clipArea(cs.area.Y(), cs.picture->Y())), baseQP, m_pcEncCfg->getSmoothQPReductionThresholdIntra(), m_pcEncCfg->getSmoothQPReductionModelScaleIntra(), m_pcEncCfg->getSmoothQPReductionModelOffsetIntra(), m_pcEncCfg->getSmoothQPReductionLimitIntra());
          }
          else
          {
            smoothQPoffset = calculateLumaDQPsmooth(cs.getOrgBuf(clipArea(cs.area.Y(), cs.picture->Y())), baseQP, m_pcEncCfg->getSmoothQPReductionThresholdInter(), m_pcEncCfg->getSmoothQPReductionModelScaleInter(), m_pcEncCfg->getSmoothQPReductionModelOffsetInter(), m_pcEncCfg->getSmoothQPReductionLimitInter());
          }
        }
      }
      baseQP = Clip3(-cs.sps->getQpBDOffset(ChannelType::LUMA), MAX_QP, baseQP + smoothQPoffset);
    }
  }
  int minQP = baseQP;
  int maxQP = baseQP;

  xGetMinMaxQP( minQP, maxQP, cs, partitioner, baseQP, *cs.sps, *cs.pps, CU_QUAD_SPLIT );
  bool checkIbc = true;
  if (partitioner.chType == ChannelType::CHROMA)
  {
    checkIbc = false;
  }
  // Add coding modes here
  // NOTE: Working back to front, as a stack, which is more efficient with the container
  // NOTE: First added modes will be processed at the end.

  //////////////////////////////////////////////////////////////////////////
  // Add unit split modes
  const EncType encType = dynamic_cast<const EncLib*>(m_pcEncCfg)->getEncType();
  if (m_pcEncCfg->getDPF() && encType == ENC_PRE)
  {
    // fix test modes for pre-encoding
    const int width = m_currCsArea->lwidth();
    const int heiht = m_currCsArea->lheight();
    const int sizeCu = m_pcEncCfg->getCTUSize();
    int sizeBlk = BLK_32;
    int maxDepth = floorLog2(sizeCu / sizeBlk);
    while (width % sizeBlk != 0 || heiht % sizeBlk != 0)
    {
      maxDepth++;
      sizeBlk >>= 1;
    }
    if (partitioner.currDepth < maxDepth)
    {
      for (int qp = maxQP; qp >= minQP; qp--)
      {
        m_ComprCUCtxList.back().testModes.push_back({ ETM_SPLIT_QT, ETO_STANDARD, qp });
      }
    }
    m_ComprCUCtxList.back().testModes.push_back({ ETM_POST_DONT_SPLIT });
    xGetMinMaxQP(minQP, maxQP, cs, partitioner, baseQP, *cs.sps, *cs.pps, CU_DONT_SPLIT);
    int  lowestQP = minQP;
    for (int qpLoop = maxQP; qpLoop >= minQP; qpLoop--)
    {
      const int qp = std::max(qpLoop, lowestQP);
      m_ComprCUCtxList.back().testModes.push_back({ ETM_INTER_ME, ETO_STANDARD, qp });
    }
    if (!tryModeMaster(m_ComprCUCtxList.back().testModes.back(), cs, partitioner))
    {
      nextMode(cs, partitioner);
    }
    m_ComprCUCtxList.back().lastTestMode = EncTestMode();
    return;
  }

  if( !cuECtx.get<bool>( QT_BEFORE_BT ) )
  {
    for( int qp = maxQP; qp >= minQP; qp-- )
    {
      m_ComprCUCtxList.back().testModes.push_back( { ETM_SPLIT_QT, ETO_STANDARD, qp } );
    }
  }

  if( partitioner.canSplit( CU_TRIV_SPLIT, cs ) )
  {
    // add split modes
    for( int qp = maxQP; qp >= minQP; qp-- )
    {
      m_ComprCUCtxList.back().testModes.push_back( { ETM_SPLIT_TT_V, ETO_STANDARD, qp } );
    }
  }

  if( partitioner.canSplit( CU_TRIH_SPLIT, cs ) )
  {
    // add split modes
    for( int qp = maxQP; qp >= minQP; qp-- )
    {
      m_ComprCUCtxList.back().testModes.push_back( { ETM_SPLIT_TT_H, ETO_STANDARD, qp } );
    }
  }

  int minQPq = minQP;
  int maxQPq = maxQP;
  xGetMinMaxQP( minQP, maxQP, cs, partitioner, baseQP, *cs.sps, *cs.pps, CU_BT_SPLIT );
  if( partitioner.canSplit( CU_VERT_SPLIT, cs ) )
  {
    // add split modes
    for( int qp = maxQP; qp >= minQP; qp-- )
    {
      m_ComprCUCtxList.back().testModes.push_back( { ETM_SPLIT_BT_V, ETO_STANDARD, qp } );
    }
    m_ComprCUCtxList.back().set( DID_VERT_SPLIT, true );
  }
  else
  {
    m_ComprCUCtxList.back().set( DID_VERT_SPLIT, false );
  }

  if( partitioner.canSplit( CU_HORZ_SPLIT, cs ) )
  {
    // add split modes
    for( int qp = maxQP; qp >= minQP; qp-- )
    {
      m_ComprCUCtxList.back().testModes.push_back( { ETM_SPLIT_BT_H, ETO_STANDARD, qp } );
    }
    m_ComprCUCtxList.back().set( DID_HORZ_SPLIT, true );
  }
  else
  {
    m_ComprCUCtxList.back().set( DID_HORZ_SPLIT, false );
  }

  if( cuECtx.get<bool>( QT_BEFORE_BT ) )
  {
    for( int qp = maxQPq; qp >= minQPq; qp-- )
    {
      m_ComprCUCtxList.back().testModes.push_back( { ETM_SPLIT_QT, ETO_STANDARD, qp } );
    }
  }

  m_ComprCUCtxList.back().testModes.push_back( { ETM_POST_DONT_SPLIT } );

  xGetMinMaxQP( minQP, maxQP, cs, partitioner, baseQP, *cs.sps, *cs.pps, CU_DONT_SPLIT );

  int  lowestQP = minQP;

  //////////////////////////////////////////////////////////////////////////
  // Add unit coding modes: Intra, InterME, InterMerge ...
  bool tryIntraRdo = true;
  bool tryInterRdo = true;
  bool tryIBCRdo   = true;
  if( partitioner.isConsIntra() )
  {
    tryInterRdo = false;
  }
  else if( partitioner.isConsInter() )
  {
    tryIntraRdo = tryIBCRdo = false;
  }
  checkIbc &= tryIBCRdo;

  for( int qpLoop = maxQP; qpLoop >= minQP; qpLoop-- )
  {
    const int  qp       = std::max( qpLoop, lowestQP );
#if REUSE_CU_RESULTS
    const bool isReusingCu = isValid( cs, partitioner, qp );
    cuECtx.set( IS_REUSING_CU, isReusingCu );
    if( isReusingCu )
    {
      m_ComprCUCtxList.back().testModes.push_back( {ETM_RECO_CACHED, ETO_STANDARD, qp} );
    }
#endif
    // add intra modes
    if( tryIntraRdo )
    {
      if (cs.slice->getSPS()->getPLTMode()
          && (partitioner.treeType != TREE_D || cs.slice->isIntra()
              || (cs.area.lwidth() == 4 && cs.area.lheight() == 4))
          && getPltEnc())
      {
        m_ComprCUCtxList.back().testModes.push_back({ ETM_PALETTE, ETO_STANDARD, qp });
      }
      m_ComprCUCtxList.back().testModes.push_back({ ETM_INTRA, ETO_STANDARD, qp });
      if (cs.slice->getSPS()->getPLTMode() && partitioner.treeType == TREE_D && !cs.slice->isIntra()
          && !(cs.area.lwidth() == 4 && cs.area.lheight() == 4) && getPltEnc())
      {
        m_ComprCUCtxList.back().testModes.push_back({ ETM_PALETTE, ETO_STANDARD, qp });
      }
    }
    // add ibc mode to intra path
    if (cs.sps->getIBCFlag() && checkIbc)
    {
      m_ComprCUCtxList.back().testModes.push_back({ ETM_IBC,         ETO_STANDARD,  qp });
      if (isLuma(partitioner.chType))
      {
        m_ComprCUCtxList.back().testModes.push_back({ ETM_IBC_MERGE,   ETO_STANDARD,  qp });
      }
    }
  }

  // add first pass modes
  if ( !m_slice->isIntra() && !( cs.area.lwidth() == 4 && cs.area.lheight() == 4 ) && tryInterRdo )
  {
    for( int qpLoop = maxQP; qpLoop >= minQP; qpLoop-- )
    {
      const int  qp       = std::max( qpLoop, lowestQP );
      if (m_pcEncCfg->getIMV())
      {
        m_ComprCUCtxList.back().testModes.push_back(
          { ETM_INTER_ME, EncTestModeOpts(int(EncTestMode::AmvrSearchMode::HALF_PEL) << ETO_IMV_SHIFT), qp });
      }
      if( m_pcEncCfg->getIMV() || m_pcEncCfg->getUseAffineAmvr() )
      {
        const auto imv = m_pcEncCfg->getIMV4PelFast() ? EncTestMode::AmvrSearchMode::FOUR_PEL_FAST
                                                      : EncTestMode::AmvrSearchMode::FOUR_PEL;
        m_ComprCUCtxList.back().testModes.push_back({ ETM_INTER_ME, EncTestModeOpts(int(imv) << ETO_IMV_SHIFT), qp });
        m_ComprCUCtxList.back().testModes.push_back(
          { ETM_INTER_ME, EncTestModeOpts(int(EncTestMode::AmvrSearchMode::FULL_PEL) << ETO_IMV_SHIFT), qp });
      }
      // add inter modes
      if( m_pcEncCfg->getUseEarlySkipDetection() )
      {
        m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_SKIP,  ETO_STANDARD, qp } );
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME,    ETO_STANDARD, qp } );
      }
      else
      {
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME,    ETO_STANDARD, qp } );
        m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_SKIP,  ETO_STANDARD, qp } );
      }
      if (getUseHashME())
      {
        int minSize = std::min(cs.area.lwidth(), cs.area.lheight());
        if (minSize < 128 && minSize >= 4)
        {
          m_ComprCUCtxList.back().testModes.push_back({ ETM_HASH_INTER, ETO_STANDARD, qp });
        }
      }
    }
  }

  // ensure to skip unprobable modes
  if( !tryModeMaster( m_ComprCUCtxList.back().testModes.back(), cs, partitioner ) )
  {
    nextMode( cs, partitioner );
  }

  m_ComprCUCtxList.back().lastTestMode = EncTestMode();
}

void EncModeCtrlMTnoRQT::finishCULevel( Partitioner &partitioner )
{
  m_ComprCUCtxList.pop_back();
}


bool EncModeCtrlMTnoRQT::tryMode( const EncTestMode& encTestmode, const CodingStructure &cs, Partitioner& partitioner )
{
  ComprCUCtx& cuECtx = m_ComprCUCtxList.back();

  if (m_pcEncCfg->getUseMttSkip() && partitioner.currQtDepth == 1 && partitioner.currBtDepth == 0
      && partitioner.currArea().lwidth() == 64
      && partitioner.currArea().lheight() == 64)
  {
    if (((partitioner.currArea().Y().x + 63 < cs.picture->lwidth())
         && (partitioner.currArea().Y().y + 63 < cs.picture->lheight()))

        && (encTestmode.type == ETM_SPLIT_BT_H || encTestmode.type == ETM_SPLIT_BT_V
            || encTestmode.type == ETM_SPLIT_TT_H || encTestmode.type == ETM_SPLIT_TT_V)
        && partitioner.chType == ChannelType::LUMA)
    {
      int thresholdMTT = Clip3(0, MAX_INT, (120 - ((m_pcEncCfg->getBaseQP() - 22) * 3)) * 1000000);
      if (m_noSplitIntraRdCost > thresholdMTT)
      {
        const PartSplit split = getPartSplit(encTestmode);

        if (split == CU_HORZ_SPLIT)
        {
          cuECtx.set(DID_HORZ_SPLIT, false);
        }
        if (split == CU_VERT_SPLIT)
        {
          cuECtx.set(DID_VERT_SPLIT, false);
        }
        return false;
      }
    }
  }

  // Fast checks, partitioning depended
  if (cuECtx.isHashPerfectMatch && encTestmode.type != ETM_MERGE_SKIP && encTestmode.type != ETM_INTER_ME)
  {
    return false;
  }

  // if early skip detected, skip all modes checking but the splits
  if( cuECtx.earlySkip && m_pcEncCfg->getUseEarlySkipDetection() && !isModeSplit( encTestmode ) && !( isModeInter( encTestmode ) ) )
  {
    return false;
  }

  const PartSplit implicitSplit = partitioner.getImplicitSplit( cs );
  const bool isBoundary         = implicitSplit != CU_DONT_SPLIT;

  if( isBoundary && encTestmode.type != ETM_SPLIT_QT )
  {
    return getPartSplit( encTestmode ) == implicitSplit;
  }
  else if( isBoundary && encTestmode.type == ETM_SPLIT_QT )
  {
    return partitioner.canSplit( CU_QUAD_SPLIT, cs );
  }

#if REUSE_CU_RESULTS
  if( cuECtx.get<bool>( IS_REUSING_CU ) )
  {
    if( encTestmode.type == ETM_RECO_CACHED )
    {
      return true;
    }

    if( isModeNoSplit( encTestmode ) )
    {
      return false;
    }
  }

#endif
  const Slice&           slice       = *m_slice;
  const SPS&             sps         = *slice.getSPS();
  const uint32_t             numComp     = getNumberValidComponents( slice.getSPS()->getChromaFormatIdc() );
  const uint32_t             width       = partitioner.currArea().lumaSize().width;
  const CodingStructure *bestCS      = cuECtx.bestCS;
  const CodingUnit      *bestCU      = cuECtx.bestCU;
  const EncTestMode      bestMode    = bestCS ? getCSEncMode( *bestCS ) : EncTestMode();

  CodedCUInfo    &relatedCU          = getBlkInfo( partitioner.currArea() );

  if( cuECtx.minDepth > partitioner.currQtDepth && partitioner.canSplit( CU_QUAD_SPLIT, cs ) )
  {
    // enforce QT
    return encTestmode.type == ETM_SPLIT_QT;
  }
  else if( encTestmode.type == ETM_SPLIT_QT && cuECtx.maxDepth <= partitioner.currQtDepth )
  {
    // don't check this QT depth
    return false;
  }

  if( bestCS && bestCS->cus.size() == 1 )
  {
    // update the best non-split cost
    cuECtx.set( BEST_NON_SPLIT_COST, bestCS->cost );
  }

  if( encTestmode.type == ETM_INTRA )
  {
    if( getFastDeltaQp() )
    {
      if( cs.area.lumaSize().width > cs.pcv->fastDeltaQPCuMaxSize )
      {
        return false; // only check necessary 2Nx2N Intra in fast delta-QP mode
      }
    }

    if( m_pcEncCfg->getUseFastLCTU() && partitioner.currArea().lumaSize().area() > 4096 )
    {
      return (m_pcEncCfg->getDualITree() == 0 && m_pcEncCfg->getMaxMTTHierarchyDepthI() == 0 && cs.sps->getMinQTSize(cs.slice->getSliceType(), partitioner.chType) > 64);
    }

    if (CS::isDualITree(cs) && (partitioner.currArea().lumaSize().width > 64 || partitioner.currArea().lumaSize().height > 64))
    {
      return false;
    }

    if (m_pcEncCfg->getUsePbIntraFast() && (!cs.slice->isIntra() || cs.slice->getSPS()->getIBCFlag()) && !interHadActive(cuECtx) && cuECtx.bestCU && !CU::isIntra(*cuECtx.bestCU))
    {
      return false;
    }

    // INTRA MODES
    if (cs.sps->getIBCFlag() && !cuECtx.bestTU)
    {
      return true;
    }
    if( partitioner.isConsIntra() && !cuECtx.bestTU )
    {
      return true;
    }
    if ( partitioner.currArea().lumaSize().width == 4 && partitioner.currArea().lumaSize().height == 4 && !slice.isIntra() && !cuECtx.bestTU )
    {
      return true;
    }
    if( !( slice.isIntra() || bestMode.type == ETM_INTRA || !cuECtx.bestTU ||
      ((!m_pcEncCfg->getDisableIntraPUsInInterSlices()) && (!relatedCU.isInter || !relatedCU.isIBC) && (
                                         ( cuECtx.bestTU->cbf[0] != 0 ) ||
           ( ( numComp > COMPONENT_Cb ) && cuECtx.bestTU->cbf[1] != 0 ) ||
           ( ( numComp > COMPONENT_Cr ) && cuECtx.bestTU->cbf[2] != 0 )  // avoid very complex intra if it is unlikely
         ) ) ) )
    {
      return false;
    }
    if ((m_pcEncCfg->getIBCFastMethod() & IBC_FAST_METHOD_NOINTRA_IBCCBF0)
      && (bestMode.type == ETM_IBC || bestMode.type == ETM_IBC_MERGE)
      && (!cuECtx.bestCU->Y().valid() || cuECtx.bestTU->cbf[0] == 0)
      && (!cuECtx.bestCU->Cb().valid() || cuECtx.bestTU->cbf[1] == 0)
      && (!cuECtx.bestCU->Cr().valid() || cuECtx.bestTU->cbf[2] == 0))
    {
      return false;
    }
    if( lastTestMode().type != ETM_INTRA && cuECtx.bestCS && cuECtx.bestCU && interHadActive( cuECtx ) )
    {
      // Get SATD threshold from best Inter-CU
      if (!cs.slice->isIntra() && m_pcEncCfg->getUsePbIntraFast() && !cs.slice->getDisableSATDForRD())
      {
        CodingUnit* bestCU = cuECtx.bestCU;
        if (bestCU && !CU::isIntra(*bestCU))
        {
          DistParam distParam;
          const bool useHad = true;
          m_pcRdCost->setDistParam(distParam, cs.getOrgBuf(COMPONENT_Y), cuECtx.bestCS->getPredBuf(COMPONENT_Y),
                                   cs.sps->getBitDepth(ChannelType::LUMA), COMPONENT_Y, useHad);
          cuECtx.interHad = distParam.distFunc( distParam );
        }
      }
    }
    if (bestMode.type == ETM_PALETTE && !slice.isIntra() && partitioner.treeType == TREE_D && !(partitioner.currArea().lumaSize().width == 4 && partitioner.currArea().lumaSize().height == 4)) // inter slice
    {
      return false;
    }
    if ( m_pcEncCfg->getUseFastISP() && relatedCU.relatedCuIsValid )
    {
      cuECtx.ispPredModeVal     = relatedCU.ispPredModeVal;
      cuECtx.bestDCT2NonISPCost = relatedCU.bestDCT2NonISPCost;
      cuECtx.relatedCuIsValid   = relatedCU.relatedCuIsValid;
      cuECtx.bestNonDCT2Cost    = relatedCU.bestNonDCT2Cost;
      cuECtx.bestISPIntraMode   = relatedCU.bestISPIntraMode;
    }
    return true;
  }
  else if (encTestmode.type == ETM_PALETTE)
  {
    if (partitioner.currArea().lumaSize().width > 64 || partitioner.currArea().lumaSize().height > 64
        || ((partitioner.currArea().lumaSize().width * partitioner.currArea().lumaSize().height <= 16) && (isLuma(partitioner.chType)) )
        || ((partitioner.currArea().chromaSize().width * partitioner.currArea().chromaSize().height <= 16) && (!isLuma(partitioner.chType)) && partitioner.isSepTree(cs) )
      || (partitioner.isLocalSepTree(cs)  && (!isLuma(partitioner.chType)) ) )
    {
      return false;
    }
    const Area curr_cu = CS::getArea(cs, cs.area, partitioner.chType).blocks[getFirstComponentOfChannel(partitioner.chType)];
    try
    {
      double stored_cost = slice.m_mapPltCost[isChroma(partitioner.chType)].at(curr_cu.pos()).at(curr_cu.size());
      if (bestMode.type != ETM_INVALID && stored_cost > cuECtx.bestCS->cost)
      {
        return false;
      }
    }
    catch (const std::out_of_range &)
    {
      // do nothing if no stored cost value was found.
    }
    return true;
  }
  else if (encTestmode.type == ETM_IBC || encTestmode.type == ETM_IBC_MERGE)
  {
    // IBC MODES
    return sps.getIBCFlag() && CU::canUseIbc(partitioner.currArea());
  }
  else if( isModeInter( encTestmode ) )
  {
    // INTER MODES (ME + MERGE/SKIP)
    CHECK( slice.isIntra(), "Inter-mode should not be in the I-Slice mode list!" );

    if( getFastDeltaQp() )
    {
      if( encTestmode.type == ETM_MERGE_SKIP )
      {
        return false;
      }
      if( cs.area.lumaSize().width > cs.pcv->fastDeltaQPCuMaxSize )
      {
        return false; // only check necessary 2Nx2N Inter in fast deltaqp mode
      }
    }

    // --- Check if we can quit current mode using SAVE/LOAD coding history

    if( encTestmode.type == ETM_INTER_ME )
    {
      if( encTestmode.opts == ETO_STANDARD )
      {
        // NOTE: ETO_STANDARD is always done when early SKIP mode detection is enabled
        if( !m_pcEncCfg->getUseEarlySkipDetection() )
        {
          if( relatedCU.isSkip || relatedCU.isIntra )
          {
            return false;
          }
        }
      }
      else if (encTestmode.getAmvrSearchMode() != EncTestMode::AmvrSearchMode::NONE)
      {
        if (encTestmode.getAmvrSearchMode() == EncTestMode::AmvrSearchMode::FOUR_PEL_FAST
            && cuECtx.get<double>(BEST_NO_IMV_COST) * AMVR_FAST_4PEL_TH < cuECtx.get<double>(BEST_IMV_COST))
        {
          if ( !m_pcEncCfg->getUseAffineAmvr() )
          {
            return false;
          }
        }
      }
    }

    return true;
  }
  else if( isModeSplit( encTestmode ) )
  {
    //////////////////////////////////////////////////////////////////////////
    // skip-history rule - don't split further if at least for three past levels
    //                     in the split tree it was found that skip is the best mode
    //////////////////////////////////////////////////////////////////////////
    int skipScore = 0;

    if ((!slice.isIntra() || slice.getSPS()->getIBCFlag()) && cuECtx.get<bool>(IS_BEST_NOSPLIT_SKIP))
    {
      for( int i = 2; i < m_ComprCUCtxList.size(); i++ )
      {
        if( ( m_ComprCUCtxList.end() - i )->get<bool>( IS_BEST_NOSPLIT_SKIP ) )
        {
          skipScore += 1;
        }
        else
        {
          break;
        }
      }
    }

    const PartSplit split = getPartSplit( encTestmode );
    if( !partitioner.canSplit( split, cs ) || skipScore >= 2 )
    {
      if (split == CU_HORZ_SPLIT)
      {
        cuECtx.set(DID_HORZ_SPLIT, false);
      }
      if (split == CU_VERT_SPLIT)
      {
        cuECtx.set(DID_VERT_SPLIT, false);
      }
      if (split == CU_QUAD_SPLIT)
      {
        cuECtx.set(DID_QUAD_SPLIT, false);
      }
      return false;
    }

    if( m_pcEncCfg->getUseContentBasedFastQtbt() )
    {
      const CompArea& currArea = partitioner.currArea().Y();
      int cuHeight  = currArea.height;
      int cuWidth   = currArea.width;

      const bool condIntraInter = m_pcEncCfg->getIntraPeriod() == 1 ? ( partitioner.currBtDepth == 0 ) : ( cuHeight > 32 && cuWidth > 32 );

      if( cuWidth == cuHeight && condIntraInter && getPartSplit( encTestmode ) != CU_QUAD_SPLIT )
      {
        const CPelBuf bufCurrArea = cs.getOrgBuf( partitioner.currArea().block( COMPONENT_Y ) );

        double horVal = 0;
        double verVal = 0;
        double dupVal = 0;
        double dowVal = 0;

        const double th = m_pcEncCfg->getIntraPeriod() == 1 ? 1.2 : 1.0;

        unsigned j, k;

        for( j = 0; j < cuWidth - 1; j++ )
        {
          for( k = 0; k < cuHeight - 1; k++ )
          {
            horVal += abs( bufCurrArea.at( j + 1, k     ) - bufCurrArea.at( j, k ) );
            verVal += abs( bufCurrArea.at( j    , k + 1 ) - bufCurrArea.at( j, k ) );
            dowVal += abs( bufCurrArea.at( j + 1, k )     - bufCurrArea.at( j, k + 1 ) );
            dupVal += abs( bufCurrArea.at( j + 1, k + 1 ) - bufCurrArea.at( j, k ) );
          }
        }
        if( horVal > th * verVal && sqrt( 2 ) * horVal > th * dowVal && sqrt( 2 ) * horVal > th * dupVal && ( getPartSplit( encTestmode ) == CU_HORZ_SPLIT || getPartSplit( encTestmode ) == CU_TRIH_SPLIT ) )
        {
          return false;
        }
        if( th * dupVal < sqrt( 2 ) * verVal && th * dowVal < sqrt( 2 ) * verVal && th * horVal < verVal && ( getPartSplit( encTestmode ) == CU_VERT_SPLIT || getPartSplit( encTestmode ) == CU_TRIV_SPLIT ) )
        {
          return false;
        }
      }

      if( m_pcEncCfg->getIntraPeriod() == 1 && cuWidth <= 32 && cuHeight <= 32 && bestCS && bestCS->tus.size() == 1 && bestCU && bestCU->depth == partitioner.currDepth && partitioner.currBtDepth > 1 && isLuma( partitioner.chType ) )
      {
        if( !bestCU->rootCbf )
        {
          return false;
        }
      }
    }

    if( bestCU && bestCU->skip && bestCU->mtDepth >= m_skipThreshold && !isModeSplit( cuECtx.lastTestMode ) )
    {
      return false;
    }

    int featureToSet = -1;

    switch( getPartSplit( encTestmode ) )
    {
      case CU_QUAD_SPLIT:
        {
          if( !cuECtx.get<bool>( QT_BEFORE_BT ) && bestCU )
          {
            unsigned maxBTD        = cs.pcv->getMaxBtDepth( slice, partitioner.chType );
            const CodingUnit *cuBR = bestCS->cus.back();
            unsigned height        = partitioner.currArea().lumaSize().height;

            if (bestCU && ((bestCU->btDepth == 0 && maxBTD >= ((slice.isIntra() && !slice.getSPS()->getIBCFlag()) ? 3 : 2))
              || (bestCU->btDepth == 1 && cuBR && cuBR->btDepth == 1 && maxBTD >= ((slice.isIntra() && !slice.getSPS()->getIBCFlag()) ? 4 : 3)))
              && (width <= MAX_TB_SIZEY && height <= MAX_TB_SIZEY)
              && cuECtx.get<bool>(DID_HORZ_SPLIT) && cuECtx.get<bool>(DID_VERT_SPLIT))
            {
              return false;
            }
          }
          if( m_pcEncCfg->getUseEarlyCU() && bestCS->cost != MAX_DOUBLE && bestCU && bestCU->skip )
          {
            return false;
          }
          if( getFastDeltaQp() && width <= slice.getPPS()->pcv->fastDeltaQPCuMaxSize )
          {
            return false;
          }
        }
        break;
      case CU_HORZ_SPLIT:
        featureToSet = DID_HORZ_SPLIT;
        break;
      case CU_VERT_SPLIT:
        featureToSet = DID_VERT_SPLIT;
        break;
      case CU_TRIH_SPLIT:
        if( cuECtx.get<bool>( DID_HORZ_SPLIT ) && bestCU && bestCU->btDepth == partitioner.currBtDepth && !bestCU->rootCbf )
        {
          return false;
        }

        if( !cuECtx.get<bool>( DO_TRIH_SPLIT ) )
        {
          return false;
        }
        break;
      case CU_TRIV_SPLIT:
        if( cuECtx.get<bool>( DID_VERT_SPLIT ) && bestCU && bestCU->btDepth == partitioner.currBtDepth && !bestCU->rootCbf )
        {
          return false;
        }

        if( !cuECtx.get<bool>( DO_TRIV_SPLIT ) )
        {
          return false;
        }
        break;
      default:
        THROW( "Only CU split modes are governed by the EncModeCtrl" );
        return false;
        break;
    }

    switch( split )
    {
      case CU_HORZ_SPLIT:
      case CU_TRIH_SPLIT:
        if( cuECtx.get<bool>( QT_BEFORE_BT ) && cuECtx.get<bool>( DID_QUAD_SPLIT ) )
        {
          if( cuECtx.get<int>( MAX_QT_SUB_DEPTH ) > partitioner.currQtDepth + 1 )
          {
            if (featureToSet >= 0)
            {
              cuECtx.set(featureToSet, false);
            }
            return false;
          }
        }
        if (m_pcEncCfg->getFastTTskip() && split == CU_TRIH_SPLIT)
        {
          bool skipTtSplitMode = xSkipTreeCandidate(getPartSplit(encTestmode), cs.splitRdCostBest, m_slice->getSliceType());
          if (skipTtSplitMode)
          {
            return false;
          }
        }
        break;
      case CU_VERT_SPLIT:
      case CU_TRIV_SPLIT:
        if( cuECtx.get<bool>( QT_BEFORE_BT ) && cuECtx.get<bool>( DID_QUAD_SPLIT ) )
        {
          if( cuECtx.get<int>( MAX_QT_SUB_DEPTH ) > partitioner.currQtDepth + 1 )
          {
            if (featureToSet >= 0)
            {
              cuECtx.set(featureToSet, false);
            }
            return false;
          }
        }
        if (m_pcEncCfg->getFastTTskip() && split == CU_TRIV_SPLIT) {
          bool skipTtSplitMode = xSkipTreeCandidate(getPartSplit(encTestmode), cs.splitRdCostBest, m_slice->getSliceType());
          if (skipTtSplitMode)
          {
            return false;
          }
        }
        break;
      default:
        break;
    }

    if (split == CU_QUAD_SPLIT)
    {
      cuECtx.set(DID_QUAD_SPLIT, true);
    }
    if (cs.sps->getLog2ParallelMergeLevelMinus2())
    {
      const CompArea& area = partitioner.currArea().Y();
      const SizeType size = 1 << (cs.sps->getLog2ParallelMergeLevelMinus2() + 2);
      if (!cs.slice->isIntra() && (area.width > size || area.height > size))
      {
        if (area.height <= size && split == CU_HORZ_SPLIT) return false;
        if (area.width <= size && split == CU_VERT_SPLIT) return false;
        if (area.height <= 2 * size && split == CU_TRIH_SPLIT) return false;
        if (area.width <= 2 * size && split == CU_TRIV_SPLIT) return false;
      }
    }
    return true;
  }
  else
  {
    CHECK( encTestmode.type != ETM_POST_DONT_SPLIT, "Unknown mode" );
#if REUSE_CU_RESULTS
    if ((cuECtx.get<double>(BEST_NO_IMV_COST) == UNSET_IMV_COST || cuECtx.get<bool>(IS_REUSING_CU)) && !slice.isIntra())
#else
    if (cuECtx.get<double>(BEST_NO_IMV_COST) == UNSET_IMV_COST && !slice.isIntra())
#endif
    {
      unsigned idx1, idx2, idx3, idx4;
      getAreaIdx(partitioner.currArea().Y(), *slice.getPPS()->pcv, idx1, idx2, idx3, idx4);
      CHECKD(idx3 >= MAX_NUM_SIZES || idx4 >= MAX_NUM_SIZES, "MAX_NUM_SIZES is too small");
      if (g_isReusedUniMVsFilled[idx1][idx2][idx3][idx4])
      {
        m_pcInterSearch->insertUniMvCands(partitioner.currArea().Y(), g_reusedUniMVs[idx1][idx2][idx3][idx4]);
      }
    }
    if( !bestCS || ( bestCS && isModeSplit( bestMode ) ) )
    {
      return false;
    }
    else
    {
#if REUSE_CU_RESULTS
      setFromCs( *bestCS, partitioner );

#endif
      if (partitioner.modeType == MODE_TYPE_INTRA && isLuma(partitioner.chType))
      {
        return false; //not set best coding mode for intra coding pass
      }
      // assume the non-split modes are done and set the marks for the best found mode
      if( bestCS && bestCU )
      {
        if( CU::isInter( *bestCU ) )
        {
          relatedCU.isInter   = true;
          relatedCU.isSkip   |= bestCU->skip;
          relatedCU.isMMVDSkip |= bestCU->mmvdSkip;
          relatedCU.bcwIdx = bestCU->bcwIdx;
          if (bestCU->slice->getSPS()->getUseColorTrans())
          {
            if (m_pcEncCfg->getRGBFormatFlag())
            {
              if (bestCU->colorTransform && bestCU->rootCbf)
              {
                relatedCU.selectColorSpaceOption = 1;
              }
              else
              {
                relatedCU.selectColorSpaceOption = 2;
              }
            }
            else
            {
              if (!bestCU->colorTransform || !bestCU->rootCbf)
              {
                relatedCU.selectColorSpaceOption = 1;
              }
              else
              {
                relatedCU.selectColorSpaceOption = 2;
              }
            }
          }
        }
        else if (CU::isIBC(*bestCU))
        {
          relatedCU.isIBC = true;
          relatedCU.isSkip |= bestCU->skip;
          if (bestCU->slice->getSPS()->getUseColorTrans())
          {
            if (m_pcEncCfg->getRGBFormatFlag())
            {
              if (bestCU->colorTransform && bestCU->rootCbf)
              {
                relatedCU.selectColorSpaceOption = 1;
              }
              else
              {
                relatedCU.selectColorSpaceOption = 2;
              }
            }
            else
            {
              if (!bestCU->colorTransform || !bestCU->rootCbf)
              {
                relatedCU.selectColorSpaceOption = 1;
              }
              else
              {
                relatedCU.selectColorSpaceOption = 2;
              }
            }
          }
        }
        else if( CU::isIntra( *bestCU ) )
        {
          relatedCU.isIntra   = true;
          if ( m_pcEncCfg->getUseFastISP() && cuECtx.ispWasTested && ( !relatedCU.relatedCuIsValid || bestCS->cost < relatedCU.bestCost ) )
          {
            // Compact data
            relatedCU.ispPredModeVal.valid            = 1;
            relatedCU.ispPredModeVal.notIsp           = cuECtx.ispMode == ISPType::NONE ? 1 : 0;
            relatedCU.ispPredModeVal.verIsp           = cuECtx.ispMode == ISPType::VER;
            relatedCU.ispPredModeVal.ispLfnstIdx      = cuECtx.ispLfnstIdx;
            relatedCU.ispPredModeVal.mipFlag          = cuECtx.mipFlag;
            relatedCU.ispPredModeVal.lowIspCost       = cuECtx.bestCostIsp < cuECtx.bestNonDCT2Cost * 0.95;
            relatedCU.ispPredModeVal.bestPredModeDCT2 = cuECtx.bestPredModeDCT2;
            relatedCU.bestDCT2NonISPCost = cuECtx.bestDCT2NonISPCost;
            relatedCU.bestCost           = bestCS->cost;
            relatedCU.bestNonDCT2Cost    = cuECtx.bestNonDCT2Cost;
            relatedCU.bestISPIntraMode   = cuECtx.bestISPIntraMode;
            relatedCU.relatedCuIsValid   = true;
          }
        }

        cuECtx.set( IS_BEST_NOSPLIT_SKIP, bestCU->skip );
      }
    }

    return false;
  }
}

bool EncModeCtrlMTnoRQT::xSkipTreeCandidate(const PartSplit split, const double* splitRdCostBest, const SliceType& sliceType) const
{
  if (!splitRdCostBest)
  {
    return false;
  }
  double ttEncSpeedRate = m_pcEncCfg->getFastTTskipThr();
  double horXorVerRate = m_pcEncCfg->getFastTTskipThr();

  if (!(m_pcEncCfg->getFastTTskip() & FAST_METHOD_TT_ENC_SPEEDUP_ISLICE))
  {
    if (sliceType == I_SLICE)
    {
      return false;
    }
  }

  if (!(m_pcEncCfg->getFastTTskip() & FAST_METHOD_TT_ENC_SPEEDUP_BSLICE))
  {
    if (sliceType == B_SLICE)
    {
      return false;
    }
  }
  bool res = false;

  if (split == CU_TRIH_SPLIT)
  {
    if (m_pcEncCfg->getFastTTskip() & FAST_METHOD_ENC_SPEEDUP_BT_BASED)
    {
      if (splitRdCostBest[CTU_LEVEL] < MAX_DOUBLE && splitRdCostBest[CU_HORZ_SPLIT] < MAX_DOUBLE)
      {
        if (splitRdCostBest[CU_HORZ_SPLIT] > ttEncSpeedRate * splitRdCostBest[CTU_LEVEL])
        {
          res = true;
        }
      }
    }

    if (m_pcEncCfg->getFastTTskip() & FAST_METHOD_HOR_XOR_VER)
    {
      if (splitRdCostBest[CU_HORZ_SPLIT] < MAX_DOUBLE && splitRdCostBest[CU_VERT_SPLIT] < MAX_DOUBLE)
      {
        if (splitRdCostBest[CU_HORZ_SPLIT] > horXorVerRate * splitRdCostBest[CU_VERT_SPLIT])
        {
          res = true;
        }
      }
    }
  }
  if (split == CU_TRIV_SPLIT)
  {
    if (m_pcEncCfg->getFastTTskip() & FAST_METHOD_ENC_SPEEDUP_BT_BASED)
    {
      if (splitRdCostBest[CTU_LEVEL] < MAX_DOUBLE && splitRdCostBest[CU_VERT_SPLIT] < MAX_DOUBLE)
      {
        if (splitRdCostBest[CU_VERT_SPLIT] > ttEncSpeedRate * splitRdCostBest[CTU_LEVEL])
        {
          res = true;
        }
      }
    }

    if (m_pcEncCfg->getFastTTskip() & FAST_METHOD_HOR_XOR_VER)
    {
      if (splitRdCostBest[CU_HORZ_SPLIT] < MAX_DOUBLE && splitRdCostBest[CU_VERT_SPLIT] < MAX_DOUBLE)
      {
        if (splitRdCostBest[CU_VERT_SPLIT] > horXorVerRate * splitRdCostBest[CU_HORZ_SPLIT])
        {
          res = true;
        }
      }
    }
  }
  return res;
}

bool EncModeCtrlMTnoRQT::checkSkipOtherLfnst( const EncTestMode& encTestmode, CodingStructure*& tempCS, Partitioner& partitioner )
{
  xExtractFeatures( encTestmode, *tempCS );

  ComprCUCtx& cuECtx  = m_ComprCUCtxList.back();
  bool skipOtherLfnst = false;

  if( encTestmode.type == ETM_INTRA )
  {
    if( !cuECtx.bestCS || ( tempCS->cost >= cuECtx.bestCS->cost && cuECtx.bestCS->cus.size() == 1 && CU::isIntra( *cuECtx.bestCS->cus[ 0 ] ) )
      || ( tempCS->cost <  cuECtx.bestCS->cost && CU::isIntra( *tempCS->cus[ 0 ] ) ) )
    {
      skipOtherLfnst = !tempCS->cus[ 0 ]->rootCbf;
    }
  }

  return skipOtherLfnst;
}

bool EncModeCtrlMTnoRQT::useModeResult( const EncTestMode& encTestmode, CodingStructure*& tempCS, Partitioner& partitioner )
{
  xExtractFeatures( encTestmode, *tempCS );

  ComprCUCtx& cuECtx = m_ComprCUCtxList.back();

  if(      encTestmode.type == ETM_SPLIT_BT_H )
  {
    cuECtx.set( BEST_HORZ_SPLIT_COST, tempCS->cost );
  }
  else if( encTestmode.type == ETM_SPLIT_BT_V )
  {
    cuECtx.set( BEST_VERT_SPLIT_COST, tempCS->cost );
  }
  else if( encTestmode.type == ETM_SPLIT_TT_H )
  {
    cuECtx.set( BEST_TRIH_SPLIT_COST, tempCS->cost );
  }
  else if( encTestmode.type == ETM_SPLIT_TT_V )
  {
    cuECtx.set( BEST_TRIV_SPLIT_COST, tempCS->cost );
  }
  else if( encTestmode.type == ETM_INTRA )
  {
    const CodingUnit& cu = *tempCS->getCU( partitioner.chType );

    if( !cu.mtsFlag )
    {
      cuECtx.bestMtsSize2Nx2N1stPass   = tempCS->cost;
    }
    if (cu.ispMode == ISPType::NONE)
    {
      cuECtx.bestCostMtsFirstPassNoIsp = tempCS->cost;
    }
  }

  if( m_pcEncCfg->getIMV4PelFast() && m_pcEncCfg->getIMV() && encTestmode.type == ETM_INTER_ME )
  {
    const auto amvrSearchMode = encTestmode.getAmvrSearchMode();

    if (amvrSearchMode == EncTestMode::AmvrSearchMode::FULL_PEL)
    {
      if( tempCS->cost < cuECtx.get<double>( BEST_IMV_COST ) )
      {
        cuECtx.set( BEST_IMV_COST, tempCS->cost );
      }
    }
    else if (amvrSearchMode == EncTestMode::AmvrSearchMode::NONE)
    {
      if( tempCS->cost < cuECtx.get<double>( BEST_NO_IMV_COST ) )
      {
        cuECtx.set( BEST_NO_IMV_COST, tempCS->cost );
      }
    }
  }

  if( encTestmode.type == ETM_SPLIT_QT )
  {
    int maxQtD = 0;
    for( const auto& cu : tempCS->cus )
    {
      maxQtD = std::max<int>( maxQtD, cu->qtDepth );
    }
    cuECtx.set( MAX_QT_SUB_DEPTH, maxQtD );
  }
  if( !m_pcEncCfg->getDisableFastDecisionTT() )
  {
    int maxMtD = tempCS->pcv->getMaxBtDepth( *tempCS->slice, partitioner.chType ) + partitioner.currImplicitBtDepth;

    if( encTestmode.type == ETM_SPLIT_BT_H )
    {
      if( tempCS->cus.size() > 2 )
      {
        int h_2   = tempCS->area.block(partitioner.chType).height / 2;
        int cu1_h = tempCS->cus.front()->block(partitioner.chType).height;
        int cu2_h = tempCS->cus.back()->block(partitioner.chType).height;

        cuECtx.set( DO_TRIH_SPLIT, cu1_h < h_2 || cu2_h < h_2 || partitioner.currMtDepth + 1 == maxMtD );
      }
    }
    else if( encTestmode.type == ETM_SPLIT_BT_V )
    {
      if( tempCS->cus.size() > 2 )
      {
        int w_2   = tempCS->area.block(partitioner.chType).width / 2;
        int cu1_w = tempCS->cus.front()->block(partitioner.chType).width;
        int cu2_w = tempCS->cus.back()->block(partitioner.chType).width;

        cuECtx.set( DO_TRIV_SPLIT, cu1_w < w_2 || cu2_w < w_2 || partitioner.currMtDepth + 1 == maxMtD );
      }
    }
  }

  bool chooseTemp = false;
  if( tempCS->features[ENC_FT_RD_COST] != MAX_DOUBLE )
  {
    if(!cuECtx.bestCS)
    {
      chooseTemp = true;
    }
    else
    {
      // for now just a simple decision based on RD-cost or choose tempCS if bestCS is not yet coded
      chooseTemp = ( tempCS->features[ENC_FT_RD_COST] + ( tempCS->useDbCost ? tempCS->costDbOffset : 0 ) ) < ( cuECtx.bestCS->features[ENC_FT_RD_COST] + ( tempCS->useDbCost ? cuECtx.bestCS->costDbOffset : 0 ) );

      // Adjust criterion to choose mode with inter-layer prediction if it has lower modified RD cost with lambda modifier (regardless of original RD cost).
      if (m_pcEncCfg->getEncILOpt() && tempCS->useInterlayerRef != cuECtx.bestCS->useInterlayerRef)
      {
        const double costTemp = m_pcRdCost->calcRdCost(uint64_t(tempCS->features[ENC_FT_FRAC_BITS] * m_pcEncCfg->getEncILOptLambdaModifier()), tempCS->dist);
        const double costBest = m_pcRdCost->calcRdCost(uint64_t(cuECtx.bestCS->features[ENC_FT_FRAC_BITS] * m_pcEncCfg->getEncILOptLambdaModifier()), cuECtx.bestCS->dist);
        if (tempCS->useInterlayerRef && costTemp < costBest)
        {
          chooseTemp = true;
        }
        else if (!tempCS->useInterlayerRef && costBest < costTemp)
        {
          chooseTemp = false;
        }
      }
    }
  }
  if (chooseTemp)
  {
    cuECtx.bestCS = tempCS;
    cuECtx.bestCU = tempCS->cus[0];
    cuECtx.bestTU = cuECtx.bestCU->firstTU;

    if( isModeInter( encTestmode ) )
    {
      //Here we take the best cost of both inter modes. We are assuming only the inter modes (and all of them) have come before the intra modes!!!
      cuECtx.bestInterCost = cuECtx.bestCS->cost;
    }

    return true;
  }
  else
  {
    return false;
  }
}

