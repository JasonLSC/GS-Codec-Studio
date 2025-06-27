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

/** \file     EncLibCommon.h
    \brief    Common encoder library class (header)
*/

#pragma once
#include <list>
#include <fstream>
#include "CommonLib/Slice.h"
#include "CommonLib/ParameterSetManager.h"
#if JVET_AJ0151_DSC_SEI && JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
#include "CommonLib/SEIDigitallySignedContent.h"
#endif

class EncLibCommon
{
private:
  ParameterSetMap<SPS>      m_spsMap;             ///< SPS, it is shared across all layers
  ParameterSetMap<PPS>      m_ppsMap;             ///< PPS, it is shared across all layers
  EnumArray<ParameterSetMap<APS>, ApsType> m_apsMaps;            ///< APS, it is shared across all layers
  PicList                   m_cListPic;           ///< DPB, it is shared across all layers
  VPS                       m_vps;
  int                       m_layerDecPicBuffering[MAX_VPS_LAYERS*MAX_TLAYER];  // to store number of required DPB pictures per layer
#if JVET_AJ0151_DSC_SEI && JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  DscSubstreamManager       m_dscSubstreamManager;
#endif

public:
  EncLibCommon();
  virtual ~EncLibCommon();

  PicList&                 getPictureBuffer()      { return m_cListPic;   }
  ParameterSetMap<SPS>&    getSpsMap()             { return m_spsMap;     }
  ParameterSetMap<PPS>&    getPpsMap()             { return m_ppsMap;     }
  EnumArray<ParameterSetMap<APS>, ApsType> &getApsMaps() { return m_apsMaps; }
  VPS*                     getVPS()                { return &m_vps;       }
  int*                     getDecPicBuffering()    { return m_layerDecPicBuffering; }
#if JVET_AJ0151_DSC_SEI && JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  DscSubstreamManager*     getDscSubstreamManager() { return &m_dscSubstreamManager; }
#endif
};

