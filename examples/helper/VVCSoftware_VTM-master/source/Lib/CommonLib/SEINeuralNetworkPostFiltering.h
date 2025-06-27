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

/** \file   SEINeuralNetworkPostFiltering.h
  \brief    SEI NN post filtering (application) class
*/

#ifndef __SEINEURALNETWORKPOSTFILTERING__
#define __SEINEURALNETWORKPOSTFILTERING__

#pragma once

#include "Picture.h"
#include "SEI.h"
#include "PictureParameterSet.h"

#include <map>

typedef std::vector<Picture*> PicVector;

class SEINeuralNetworkPostFiltering
{
private:
  PicVector m_picList;
  PicVector m_clvsPicList;
  SEIMessages m_clvsNnpfcSEIs;

  std::map<uint32_t, bool> m_isNnpfActiveForCLVS;

  void setPicActivatedNnpfc(Picture* picture);

public:
  SEINeuralNetworkPostFiltering();
~SEINeuralNetworkPostFiltering() = default;
  void filterPictures(PicList& picList);
  void checkInputPics(
    Picture* currCodedPic, const SEINeuralNetworkPostFilterCharacteristics* currNnpfc,
    uint32_t sourceWidth, uint32_t sourceHeight, uint32_t croppedWidth, uint32_t croppedHeight);
  bool isPicInCurrentClvs(Picture* pic);
};

#endif
