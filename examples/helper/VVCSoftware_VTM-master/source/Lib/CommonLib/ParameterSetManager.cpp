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
#include "ParameterSetManager.h"

ParameterSetManager::ParameterSetManager()
  : m_spsMap(MAX_NUM_SPS), m_ppsMap(MAX_NUM_PPS), m_vpsMap(MAX_NUM_VPS), m_activeSPSId(-1), m_activeVPSId(-1)
{
  for (const auto t: { ApsType::ALF, ApsType::LMCS, ApsType::SCALING_LIST })
  {
    m_apsMap[t] = ParameterSetMap<APS>(MAX_NUM_APS(t));
  }
}


ParameterSetManager::~ParameterSetManager()
{
}

VPS* ParameterSetManager::getVPS( int vpsId )
{
  if ( vpsId != 0)
  {
    return m_vpsMap.getPS( vpsId );
  }
  else
  {
    // VPS Id zero contains inferred values only. Allocate (with defaults) on first access
    VPS *vps = m_vpsMap.getPS (0);
    if (vps == nullptr)
    {
      m_vpsMap.allocatePS(0);
      vps = m_vpsMap.getPS( 0 );
      vps->deriveOutputLayerSets();
    }
    return m_vpsMap.getPS( 0 );
  }
};

// activate a PPS and depending on isIRAP parameter also SPS
// returns true, if activation is successful
bool ParameterSetManager::activatePPS(int ppsId, bool isIRAP)
{
  PPS *pps = m_ppsMap.getPS(ppsId);
  if (pps)
  {
    int spsId = pps->getSPSId();
    {
      SPS *sps = m_spsMap.getPS(spsId);

      if (sps)
      {
        int vpsId = sps->getVPSId();

        VPS *vps = m_vpsMap.getPS(vpsId);
        if(vps)
        {
          m_activeVPSId = vpsId;
          m_vpsMap.clearActive();
          m_vpsMap.setActive(vpsId);
        }
        else
        {
          msg( WARNING, "Warning: tried to activate a PPS that refers to a non-existing VPS." );
        }

        m_spsMap.clearActive();
        m_spsMap.setActive(spsId);
        m_activeSPSId = spsId;
        m_ppsMap.clearActive();
        m_ppsMap.setActive(ppsId);
        return true;
      }
      else
      {
        msg( WARNING, "Warning: tried to activate a PPS that refers to a non-existing SPS.");
      }
    }
  }
  else
  {
    msg( WARNING, "Warning: tried to activate a non-existing PPS.");
  }

  // Failed to activate if reach here.
  m_activeSPSId=-1;
  return false;
}

bool ParameterSetManager::activateAPS(uint8_t apsId, ApsType apsType)
{
  APS *aps = m_apsMap[apsType].getPS(apsId);
  if (aps)
  {
    m_apsMap[apsType].setActive(apsId);
    return true;
  }
  else
  {
    msg(WARNING, "Warning: tried to activate a non-existing APS.");
  }
  return false;
}

template <>
void ParameterSetMap<APS>::setID(APS* parameterSet, const int psId)
{
  parameterSet->setAPSId(psId);
}
template <>
void ParameterSetMap<PPS>::setID(PPS* parameterSet, const int psId)
{
  parameterSet->setPPSId(psId);
}

template <>
void ParameterSetMap<SPS>::setID(SPS* parameterSet, const int psId)
{
  parameterSet->setSPSId(psId);
}

template <>
void ParameterSetMap<VPS>::setID(VPS* parameterSet, const int psId)
{
  parameterSet->setVPSId(psId);
}

void calculateParameterSetChangedFlag(bool &changed, const std::vector<uint8_t> *oldData, const std::vector<uint8_t> *newData)
{
  if (!changed)
  {
    if ((oldData==0 && newData!=0) || (oldData!=0 && newData==0))
    {
      changed=true;
    }
    else if (oldData!=0 && newData!=0)
    {
      // compare the two
      if (oldData->size() != newData->size())
      {
        changed=true;
      }
      else
      {
        const uint8_t *pNewDataArray=&(*newData)[0];
        const uint8_t *pOldDataArray=&(*oldData)[0];
        if (memcmp(pOldDataArray, pNewDataArray, oldData->size()))
        {
          changed=true;
        }
      }
    }
  }
}
