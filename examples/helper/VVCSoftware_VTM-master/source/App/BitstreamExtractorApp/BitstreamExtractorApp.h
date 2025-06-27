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

#ifndef __BITSTREAMEXTRACTORAPP__
#define __BITSTREAMEXTRACTORAPP__

#pragma once

#include <stdio.h>
#include <fstream>
#include <iostream>

#include "CommonLib/CommonDef.h"
#include "BitstreamExtractorAppCfg.h"
#include "CommonLib/ParameterSetManager.h"
#include "DecoderLib/NALread.h"
#include "VLCReader.h"
#include "VLCWriter.h"

#include "SEIread.h"
#include "SEIwrite.h"
class BitstreamExtractorApp : public BitstreamExtractorAppCfg
{

public:
  BitstreamExtractorApp();
  virtual ~BitstreamExtractorApp         ()  {}

  uint32_t  decode            (); ///< main decoding function

protected:
  void xPrintVPSInfo (VPS *vps);
  void xPrintSubPicInfo (PPS *pps);
  void xRewriteSPS (SPS &targetSPS, const SPS &sourceSPS, SubPic &subPic);
  void xRewritePPS (PPS &targetPPS, const PPS &sourcePPS, const SPS &sourceSPS, SubPic &subPic);
  bool xCheckSEIFiller(SEIMessages SEIs, int targetSubPicId, bool &rmAllFillerInSubpicExt, bool lastSliceWritten);

  Slice xParseSliceHeader(InputNALUnit &nalu);
  bool  xCheckSliceSubpicture(Slice &slice, int subPicId);
  void xReadPicHeader(InputNALUnit &nalu);
  bool xIsTargetOlsIncludeAllVclLayers();
  bool xCheckSEIsSubPicture(SEIMessages& SEIs, InputNALUnit& nalu, std::ostream& out, int subpicId, VPS *vps);

  void xSetSPSUpdated(int spsId)   { return m_updatedSPSList.push_back(spsId); }
  bool xIsSPSUpdate(int spsId)     { return (std::find(m_updatedSPSList.begin(),m_updatedSPSList.end(), spsId) != m_updatedSPSList.end()); }
  void xClearSPSUpdated(int spsId) { m_updatedSPSList.erase(std::remove(m_updatedSPSList.begin(), m_updatedSPSList.end(), spsId)); };

  bool xCheckNumSubLayers(InputNALUnit &nalu, VPS *vps);

  void xWriteVPS(VPS *vps, std::ostream& out, int layerId, int temporalId);
  void xWriteSPS(SPS *sps, std::ostream& out, int layerId, int temporalId);
  void xWritePPS(PPS *pps, std::ostream& out, int layerId, int temporalId);

  ParameterSetManager   m_parameterSetManager;
  HLSyntaxReader        m_hlSynaxReader;
  HLSWriter             m_hlSyntaxWriter;
  SEIReader             m_seiReader;
  SEIWriter             m_seiWriter;
  HRD                   m_hrd;

  int                   m_vpsId;
  bool                  m_removeTimingSEI;

  PicHeader             m_picHeader;
  int                   m_prevTid0Poc;
  int                   m_prevPicPOC;
  std::vector<int>      m_updatedVPSList;
  std::vector<int>      m_updatedSPSList;
  std::vector<int>      m_updatedPPSList;
};

#endif // __BITSTREAMEXTRACTORAPP__

