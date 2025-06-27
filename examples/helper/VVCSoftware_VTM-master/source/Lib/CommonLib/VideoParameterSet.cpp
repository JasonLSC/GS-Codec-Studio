
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

#include "CommonDef.h"
#include "VideoParameterSet.h"

// ------------------------------------------------------------------------------------------------
// Video parameter set (VPS)
// ------------------------------------------------------------------------------------------------
VPS::VPS()
  : m_vpsId(0)
  , m_maxLayers(1)
  , m_vpsMaxSubLayers(7)
  , m_vpsDefaultPtlDpbHrdMaxTidFlag(true)
  , m_vpsAllIndependentLayersFlag(true)
  , m_vpsEachLayerIsAnOlsFlag(1)
  , m_vpsOlsModeIdc(0)
  , m_vpsNumOutputLayerSets(1)
  , m_vpsExtensionFlag(false)
  , m_vpsGeneralHrdParamsPresentFlag(false)
  , m_vpsSublayerCpbParamsPresentFlag(false)
  , m_numOlsTimingHrdParamsMinus1(0)
  , m_totalNumOLSs(1)
  , m_numMultiLayeredOlss(0)
  , m_numDpbParams(0)
  , m_sublayerDpbParamsPresentFlag(false)
  , m_targetOlsIdx(0)
{
  for (int i = 0; i < MAX_VPS_SUBLAYERS; i++)
  {
    m_vpsCfgPredDirection[i] = 0;
  }
  for (int i = 0; i < MAX_VPS_LAYERS; i++)
  {
    m_vpsLayerId[i] = 0;
    m_vpsIndependentLayerFlag[i] = true;
    m_generalLayerIdx[i] = 0;
    for (int j = 0; j < MAX_VPS_LAYERS; j++)
    {
      m_vpsDirectRefLayerFlag[i][j] = 0;
      m_directRefLayerIdx[i][j] = MAX_VPS_LAYERS;
      m_interLayerRefIdx[i][i] = NOT_VALID;
    }
  }
  for (int i = 0; i < MAX_NUM_OLSS; i++)
  {
    for (int j = 0; j < MAX_VPS_LAYERS; j++)
    {
      m_vpsOlsOutputLayerFlag[i][j] = 0;
    }
    if (i == 0)
    {
      m_ptPresentFlag[i] = 1;
    }
    else
    {
      m_ptPresentFlag[i] = 0;
    }
    m_ptlMaxTemporalId[i] = m_vpsMaxSubLayers - 1;
    m_olsPtlIdx[i] = 0;
    m_hrdMaxTid[i] = m_vpsMaxSubLayers - 1;
    m_olsTimingHrdIdx[i] = 0;
  }
}

void VPS::deriveOutputLayerSets()
{
  if( m_vpsEachLayerIsAnOlsFlag || m_vpsOlsModeIdc < 2 )
  {
    m_totalNumOLSs = m_maxLayers;
  }
  else if( m_vpsOlsModeIdc == 2 )
  {
    m_totalNumOLSs = m_vpsNumOutputLayerSets;
  }

  m_olsDpbParamsIdx.resize( m_totalNumOLSs );
  m_olsDpbPicSize.resize( m_totalNumOLSs, Size(0, 0) );
  m_numOutputLayersInOls.resize( m_totalNumOLSs );
  m_numLayersInOls.resize( m_totalNumOLSs );
  m_outputLayerIdInOls.resize( m_totalNumOLSs, std::vector<int>( m_maxLayers, NOT_VALID ) );
  m_numSubLayersInLayerInOLS.resize( m_totalNumOLSs, std::vector<int>( m_maxLayers, NOT_VALID ) );
  m_layerIdInOls.resize( m_totalNumOLSs, std::vector<int>( m_maxLayers, NOT_VALID ) );
  m_olsDpbChromaFormatIdc.resize(m_totalNumOLSs);
  m_olsDpbBitDepthMinus8.resize(m_totalNumOLSs);

  std::vector<int> numRefLayers( m_maxLayers );
  std::vector<std::vector<int>> outputLayerIdx( m_totalNumOLSs, std::vector<int>( m_maxLayers, NOT_VALID ) );
  std::vector<std::vector<int>> layerIncludedInOlsFlag( m_totalNumOLSs, std::vector<int>( m_maxLayers, 0 ) );
  std::vector<std::vector<int>> dependencyFlag( m_maxLayers, std::vector<int>( m_maxLayers, NOT_VALID ) );
  std::vector<std::vector<int>> refLayerIdx( m_maxLayers, std::vector<int>( m_maxLayers, NOT_VALID ) );
  std::vector<int> layerUsedAsRefLayerFlag( m_maxLayers, 0 );
  std::vector<int> layerUsedAsOutputLayerFlag( m_maxLayers, NOT_VALID );

  for( int i = 0; i < m_maxLayers; i++ )
  {
    int r = 0;

    for( int j = 0; j < m_maxLayers; j++ )
    {
      dependencyFlag[i][j] = m_vpsDirectRefLayerFlag[i][j];

      for( int k = 0; k < i; k++ )
      {
        if( m_vpsDirectRefLayerFlag[i][k] && dependencyFlag[k][j] )
        {
          dependencyFlag[i][j] = 1;
        }
      }
      if (m_vpsDirectRefLayerFlag[i][j])
      {
        layerUsedAsRefLayerFlag[j] = 1;
      }

      if( dependencyFlag[i][j] )
      {
        refLayerIdx[i][r++] = j;
      }
    }

    numRefLayers[i] = r;
  }

  m_numOutputLayersInOls[0] = 1;
  m_outputLayerIdInOls[0][0] = m_vpsLayerId[0];
  m_numSubLayersInLayerInOLS[0][0] = m_ptlMaxTemporalId[m_olsPtlIdx[0]] + 1;
  layerUsedAsOutputLayerFlag[0] = 1;
  for (int i = 1; i < m_maxLayers; i++)
  {
    if (m_vpsEachLayerIsAnOlsFlag || m_vpsOlsModeIdc < 2)
    {
      layerUsedAsOutputLayerFlag[i] = 1;
    }
    else
    {
      layerUsedAsOutputLayerFlag[i] = 0;
    }
  }
  for( int i = 1; i < m_totalNumOLSs; i++ )
  {
    if( m_vpsEachLayerIsAnOlsFlag || m_vpsOlsModeIdc == 0 )
    {
      m_numOutputLayersInOls[i] = 1;
      m_outputLayerIdInOls[i][0] = m_vpsLayerId[i];
      if (m_vpsEachLayerIsAnOlsFlag)
      {
        m_numSubLayersInLayerInOLS[i][0] = m_ptlMaxTemporalId[m_olsPtlIdx[i]] + 1;
      }
      else
      {
        m_numSubLayersInLayerInOLS[i][i] = m_ptlMaxTemporalId[m_olsPtlIdx[i]] + 1;
        for (int k = i - 1; k >= 0; k--)
        {
          m_numSubLayersInLayerInOLS[i][k] = 0;
          for (int m = k + 1; m <= i; m++)
          {
            uint32_t maxSublayerNeeded = std::min((uint32_t)m_numSubLayersInLayerInOLS[i][m], m_vpsMaxTidIlRefPicsPlus1[m][k]);
            if (m_vpsDirectRefLayerFlag[m][k] && m_numSubLayersInLayerInOLS[i][k] < maxSublayerNeeded)
            {
              m_numSubLayersInLayerInOLS[i][k] = maxSublayerNeeded;
            }
          }
        }
      }
    }
    else if( m_vpsOlsModeIdc == 1 )
    {
      m_numOutputLayersInOls[i] = i + 1;

      for( int j = 0; j < m_numOutputLayersInOls[i]; j++ )
      {
        m_outputLayerIdInOls[i][j] = m_vpsLayerId[j];
        m_numSubLayersInLayerInOLS[i][j] = m_ptlMaxTemporalId[m_olsPtlIdx[i]] + 1;
      }
    }
    else if( m_vpsOlsModeIdc == 2 )
    {
      int j = 0;
      int highestIncludedLayer = 0;
      for( j = 0; j  <  m_maxLayers; j++ )
      {
        m_numSubLayersInLayerInOLS[i][j] = 0;
      }
      j = 0;
      for( int k = 0; k < m_maxLayers; k++ )
      {
        if( m_vpsOlsOutputLayerFlag[i][k] )
        {
          layerIncludedInOlsFlag[i][k] = 1;
          highestIncludedLayer = k;
          layerUsedAsOutputLayerFlag[k] = 1;
          outputLayerIdx[i][j] = k;
          m_outputLayerIdInOls[i][j++] = m_vpsLayerId[k];
          m_numSubLayersInLayerInOLS[i][k] = m_ptlMaxTemporalId[m_olsPtlIdx[i]] + 1;
        }
      }
      m_numOutputLayersInOls[i] = j;

      for( j = 0; j < m_numOutputLayersInOls[i]; j++ )
      {
        int idx = outputLayerIdx[i][j];
        for( int k = 0; k < numRefLayers[idx]; k++ )
        {
          layerIncludedInOlsFlag[i][refLayerIdx[idx][k]] = 1;
        }
      }
      for (int k = highestIncludedLayer - 1; k >= 0; k--)
      {
        if (layerIncludedInOlsFlag[i][k] && !m_vpsOlsOutputLayerFlag[i][k])
        {
          for (int m = k + 1; m <= highestIncludedLayer; m++)
          {
            uint32_t maxSublayerNeeded = std::min((uint32_t)m_numSubLayersInLayerInOLS[i][m], m_vpsMaxTidIlRefPicsPlus1[m][k]);
            if (m_vpsDirectRefLayerFlag[m][k] && layerIncludedInOlsFlag[i][m] && m_numSubLayersInLayerInOLS[i][k] < maxSublayerNeeded)
            {
              m_numSubLayersInLayerInOLS[i][k] = maxSublayerNeeded;
            }
          }
        }
      }
    }
  }
  for (int i = 0; i < m_maxLayers; i++)
  {
    CHECK(layerUsedAsRefLayerFlag[i] == 0 && layerUsedAsOutputLayerFlag[i] == 0, "There shall be no layer that is neither an output layer nor a direct reference layer");
  }

  m_numLayersInOls[0] = 1;
  m_layerIdInOls[0][0] = m_vpsLayerId[0];
  m_numMultiLayeredOlss = 0;
  for( int i = 1; i < m_totalNumOLSs; i++ )
  {
    if( m_vpsEachLayerIsAnOlsFlag )
    {
      m_numLayersInOls[i] = 1;
      m_layerIdInOls[i][0] = m_vpsLayerId[i];
    }
    else if( m_vpsOlsModeIdc == 0 || m_vpsOlsModeIdc == 1 )
    {
      m_numLayersInOls[i] = i + 1;
      for( int j = 0; j < m_numLayersInOls[i]; j++ )
      {
        m_layerIdInOls[i][j] = m_vpsLayerId[j];
      }
    }
    else if( m_vpsOlsModeIdc == 2 )
    {
      int j = 0;
      for( int k = 0; k < m_maxLayers; k++ )
      {
        if( layerIncludedInOlsFlag[i][k] )
        {
          m_layerIdInOls[i][j++] = m_vpsLayerId[k];
        }
      }

      m_numLayersInOls[i] = j;
    }
    if( m_numLayersInOls[i] > 1 )
    {
      m_multiLayerOlsIdx[i] = m_numMultiLayeredOlss;
      m_numMultiLayeredOlss++;
    }
  }
  m_multiLayerOlsIdxToOlsIdx.resize(m_numMultiLayeredOlss);

  for (int i=0, j=0; i<m_totalNumOLSs; i++)
  {
    if (m_numLayersInOls[i] > 1)
    {
      m_multiLayerOlsIdxToOlsIdx[j++] = i;
    }
  }
}

void VPS::checkVPS()
{
  for (int multiLayerOlsIdx=0; multiLayerOlsIdx < m_numMultiLayeredOlss; multiLayerOlsIdx++)
  {
    const int olsIdx = m_multiLayerOlsIdxToOlsIdx[multiLayerOlsIdx];
    const int olsTimingHrdIdx = getOlsTimingHrdIdx(multiLayerOlsIdx);
    const int olsPtlIdx = getOlsPtlIdx(olsIdx);
    CHECK (getHrdMaxTid(olsTimingHrdIdx) < getPtlMaxTemporalId(olsPtlIdx), "The value of vps_hrd_max_tid[vps_ols_timing_hrd_idx[m]] shall be greater than or equal to "
                                                                     "vps_ptl_max_tid[ vps_ols_ptl_idx[n]] for each m-th multi-layer OLS for m from 0 to "
                                                                     "NumMultiLayerOlss - 1, inclusive, and n being the OLS index of the m-th multi-layer OLS among all OLSs.");
    const int olsDpbParamsIdx = getOlsDpbParamsIdx(olsIdx);
    CHECK (m_dpbMaxTemporalId[olsDpbParamsIdx] < getPtlMaxTemporalId(olsPtlIdx), "The value of vps_dpb_max_tid[vps_ols_dpb_params_idx[m]] shall be greater than or equal to "
                                                                     "vps_ptl_max_tid[ vps_ols_ptl_idx[n]] for each m-th multi-layer OLS for m from 0 to "
                                                                     "NumMultiLayerOlss - 1, inclusive, and n being the OLS index of the m-th multi-layer OLS among all OLSs.");
  }
}


void VPS::deriveTargetOutputLayerSet( int targetOlsIdx )
{
  m_targetOlsIdx = targetOlsIdx < 0 ? m_maxLayers - 1 : targetOlsIdx;
  m_targetOutputLayerIdSet.clear();
  m_targetLayerIdSet.clear();

  for( int i = 0; i < m_numOutputLayersInOls[m_targetOlsIdx]; i++ )
  {
    m_targetOutputLayerIdSet.push_back( m_outputLayerIdInOls[m_targetOlsIdx][i] );
  }

  for( int i = 0; i < m_numLayersInOls[m_targetOlsIdx]; i++ )
  {
    m_targetLayerIdSet.push_back( m_layerIdInOls[m_targetOlsIdx][i] );
  }
}

int VPS::deriveTargetOLSIdx(void)
{
  int lowestIdx = 0;

  for (int idx = 1; idx < m_totalNumOLSs; idx++)
  {
    if ((m_numLayersInOls[lowestIdx] == m_numLayersInOls[idx]
         && m_numOutputLayersInOls[lowestIdx] < m_numOutputLayersInOls[idx])
        || m_numLayersInOls[lowestIdx] < m_numLayersInOls[idx])
    {
      lowestIdx = idx;
    }
  }

  return lowestIdx;
}

uint32_t VPS::getMaxTidinTOls(int m_targetOlsIdx)
{
  return getPtlMaxTemporalId(getOlsPtlIdx(m_targetOlsIdx));
}


