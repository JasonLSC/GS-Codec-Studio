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

#pragma once

#ifndef __PARAMETERSETMANAGER__
#define __PARAMETERSETMANAGER__

#include "Slice.h"
#include <map>

void calculateParameterSetChangedFlag(bool &changed, const std::vector<uint8_t> *pOldData,
                                      const std::vector<uint8_t> *pNewData);

template <class T> class ParameterSetMap
{
public:
  template <class Tm>
  struct MapData
  {
    bool                    changed;
    std::vector<uint8_t>   *pNaluData; // Can be null
    Tm*                   parameterSet;
  };

  ParameterSetMap(int maxId = 0) : m_maxId(maxId), m_lastActiveParameterSet(nullptr) { m_activePsId.clear(); }

  ~ParameterSetMap()
  {
    for (typename std::map<int,MapData<T> >::iterator i = m_paramsetMap.begin(); i!= m_paramsetMap.end(); i++)
    {
      delete (*i).second.pNaluData;
      delete (*i).second.parameterSet;
    }
    delete m_lastActiveParameterSet;
    m_lastActiveParameterSet = nullptr;
  }

  T *allocatePS(const int psId)
  {
    CHECK( psId >= m_maxId, "Invalid PS id" );
    if ( m_paramsetMap.find(psId) == m_paramsetMap.end() )
    {
      m_paramsetMap[psId].changed      = true;
      m_paramsetMap[psId].pNaluData=0;
      m_paramsetMap[psId].parameterSet = new T;
      setID(m_paramsetMap[psId].parameterSet, psId);
    }
    return m_paramsetMap[psId].parameterSet;
  }

  void clearMap()
  {
    m_paramsetMap.clear();
  }
  void storePS( int psId, T *ps )
  {
    CHECK( psId >= m_maxId, "Invalid PS id" );
    if( m_paramsetMap.find( psId ) != m_paramsetMap.end() )
    {
      delete m_paramsetMap[psId].parameterSet;
    }
    m_paramsetMap[psId].parameterSet = ps;
    m_paramsetMap[psId].changed      = true;
  }

  void storePS(int psId, T *ps, const std::vector<uint8_t> *pNaluData)
  {
    CHECK( psId >= m_maxId, "Invalid PS id" );
    if ( m_paramsetMap.find(psId) != m_paramsetMap.end() )
    {
      MapData<T> &mapData=m_paramsetMap[psId];

      // work out changed flag
      calculateParameterSetChangedFlag(mapData.changed, mapData.pNaluData, pNaluData);

      if (!mapData.changed)
      {
        // just keep the old one
        delete ps;
        return;
      }

      if (find(m_activePsId.begin(), m_activePsId.end(), psId) != m_activePsId.end())
      {
        std::swap( m_paramsetMap[psId].parameterSet, m_lastActiveParameterSet );
      }
      delete m_paramsetMap[psId].pNaluData;
      delete m_paramsetMap[psId].parameterSet;

      m_paramsetMap[psId].parameterSet = ps;
    }
    else
    {
      m_paramsetMap[psId].parameterSet = ps;
      m_paramsetMap[psId].changed      = false;
    }
    if (pNaluData != 0)
    {
      m_paramsetMap[psId].pNaluData=new std::vector<uint8_t>;
      *(m_paramsetMap[psId].pNaluData) = *pNaluData;
    }
    else
    {
      m_paramsetMap[psId].pNaluData=0;
    }
  }

  void checkAuApsContent( APS *aps, std::vector<int>& accessUnitApsNals )
  {
    int apsId = aps->getAPSId();

    if (std::find(accessUnitApsNals.begin(), accessUnitApsNals.end(), apsId) != accessUnitApsNals.end())
    {
      CHECK( m_paramsetMap.find( apsId ) == m_paramsetMap.end(), "APS does not exist" );
      APS* existedAPS = m_paramsetMap[apsId].parameterSet;
      bool sameNalUnitType = aps->getHasPrefixNalUnitType() == existedAPS->getHasPrefixNalUnitType();
      bool samePU = aps->getLayerId() == existedAPS->getLayerId();
      if (aps->getAPSType() == ApsType::LMCS)
      {
        CHECK( samePU && sameNalUnitType && aps->getReshaperAPSInfo() != existedAPS->getReshaperAPSInfo(), "All APS NAL units with a particular value of nal_unit_type, a particular value of aps_adaptation_parameter_set_id, and a particular value of aps_params_type within a PU shall have the same content" );
      }
      else if (aps->getAPSType() == ApsType::ALF)
      {
        CHECK( samePU && sameNalUnitType && aps->getAlfAPSParam() != existedAPS->getAlfAPSParam(), "All APS NAL units with a particular value of nal_unit_type, a particular value of aps_adaptation_parameter_set_id, and a particular value of aps_params_type within a PU shall have the same content" );
      }
      else if (aps->getAPSType() == ApsType::SCALING_LIST)
      {
        CHECK( samePU && sameNalUnitType && aps->getScalingList() != existedAPS->getScalingList(), "All APS NAL units with a particular value of nal_unit_type, a particular value of aps_adaptation_parameter_set_id, and a particular value of aps_params_type within a PU shall have the same content" );
      }
      else
      {
        THROW("Wrong APS type");
      }
    }
    else
    {
      accessUnitApsNals.push_back( apsId );
    }
  }

  void setChangedFlag(int psId, bool changed = true)
  {
    if ( m_paramsetMap.find(psId) != m_paramsetMap.end() )
    {
      m_paramsetMap[psId].changed = changed;
    }
  }

  void clearChangedFlag(int psId)
  {
    if ( m_paramsetMap.find(psId) != m_paramsetMap.end() )
    {
      m_paramsetMap[psId].changed = false;
    }
  }

  bool getChangedFlag(int psId) const
  {
    const typename std::map<int,MapData<T> >::const_iterator constit=m_paramsetMap.find(psId);
    if ( constit != m_paramsetMap.end() )
    {
      return constit->second.changed;
    }
    return false;
  }

  T* getPS(int psId)
  {
    typename std::map<int,MapData<T> >::iterator it=m_paramsetMap.find(psId);
    return (it == m_paramsetMap.end()) ? nullptr : (it)->second.parameterSet;
  }

  const T* getPS(int psId) const
  {
    typename std::map<int,MapData<T> >::const_iterator it=m_paramsetMap.find(psId);
    return (it == m_paramsetMap.end()) ? nullptr : (it)->second.parameterSet;
  }

  T* getFirstPS()
  {
    return (m_paramsetMap.begin() == m_paramsetMap.end()) ? nullptr : m_paramsetMap.begin()->second.parameterSet;
  }

  void setActive(int psId) { m_activePsId.push_back(psId); }
  void clearActive() { m_activePsId.clear(); }

private:
  std::map<int,MapData<T> > m_paramsetMap;
  int                       m_maxId;
  std::vector<int>          m_activePsId;
  T*                        m_lastActiveParameterSet;
  static void setID(T* parameterSet, const int psId);
};

class ParameterSetManager
{
public:
                 ParameterSetManager();
  virtual        ~ParameterSetManager();

  // store video parameter set and take ownership of it
  // warning: vps object cannot be used after storing (repeated parameter sets are directly deleted)
  void           storeVPS(VPS *vps, const std::vector<uint8_t> &naluData)    { m_vpsMap.storePS(vps->getVPSId(), vps, &naluData); }
  VPS*           getVPS( int vpsId );

  // store sequence parameter set and take ownership of it
  // warning: sps object cannot be used after storing (repeated parameter sets are directly deleted)
  void           storeSPS(SPS *sps, const std::vector<uint8_t> &naluData)    { m_spsMap.storePS( sps->getSPSId(), sps, &naluData); };
  // get pointer to existing sequence parameter set
  SPS*           getSPS(int spsId)                                           { return m_spsMap.getPS(spsId); };
  bool           getSPSChangedFlag(int spsId) const                          { return m_spsMap.getChangedFlag(spsId); }
  void           clearSPSChangedFlag(int spsId)                              { m_spsMap.clearChangedFlag(spsId); }
  SPS*           getFirstSPS()                                               { return m_spsMap.getFirstPS(); };

  // store picture parameter set and take ownership of it
  // warning: pps object cannot be used after storing (repeated parameter sets are directly deleted)
  void           storePPS(PPS *pps, const std::vector<uint8_t> &naluData)    { m_ppsMap.storePS( pps->getPPSId(), pps, &naluData); };
  // get pointer to existing picture parameter set
  PPS*           getPPS(int ppsId)                                           { return m_ppsMap.getPS(ppsId); };
  bool           getPPSChangedFlag(int ppsId) const                          { return m_ppsMap.getChangedFlag(ppsId); }
  void           clearPPSChangedFlag(int ppsId)                              { m_ppsMap.clearChangedFlag(ppsId); }
  PPS*           getFirstPPS()                                               { return m_ppsMap.getFirstPS(); };

  // activate a PPS and depending on isIRAP parameter also SPS
  // returns true, if activation is successful
  bool           activatePPS(int ppsId, bool isIRAP);

  APS**          getAPSs() { return &m_apss[0]; }
  ParameterSetMap<APS> *getApsMap(ApsType apsType) { return &m_apsMap[apsType]; }
  // store adaptation parameter set and take ownership of it
  // warning: aps object cannot be used after storing (repeated parameter sets are directly deleted)
  void storeAPS(APS *aps, const std::vector<uint8_t> &naluData)
  {
    m_apsMap[aps->getAPSType()].storePS(aps->getAPSId(), aps, &naluData);
  }
  APS           *getAPS(int apsId, ApsType apsType) { return m_apsMap[apsType].getPS(apsId); };
  bool           getAPSChangedFlag(int apsId, ApsType apsType) const { return m_apsMap[apsType].getChangedFlag(apsId); }
  void           clearAPSChangedFlag(int apsId, ApsType apsType) { m_apsMap[apsType].clearChangedFlag(apsId); }
  bool           activateAPS(uint8_t apsId, ApsType apsType);
  const SPS*     getActiveSPS()const                                         { return m_spsMap.getPS(m_activeSPSId); };
  void           checkAuApsContent(APS *aps, std::vector<int> &accessUnitApsNals)
  {
    m_apsMap[aps->getAPSType()].checkAuApsContent(aps, accessUnitApsNals);
  }

protected:
  ParameterSetMap<SPS> m_spsMap;
  ParameterSetMap<PPS> m_ppsMap;
  EnumArray<ParameterSetMap<APS>, ApsType> m_apsMap;
  ParameterSetMap<VPS> m_vpsMap;

  APS* m_apss[ALF_CTB_MAX_NUM_APS];
  int m_activeSPSId; // -1 for nothing active
  int m_activeVPSId; // -1 for nothing active
};


#endif
