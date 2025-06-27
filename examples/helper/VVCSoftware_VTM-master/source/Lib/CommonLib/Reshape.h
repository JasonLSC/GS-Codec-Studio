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

 /** \file     Reshape.h
     \brief    reshaping header and class (header)
 */

#ifndef __RESHAPE__
#define __RESHAPE__

#pragma once

#include "CommonDef.h"
#include "Rom.h"
#include "CommonLib/Picture.h"
//! \ingroup CommonLib
//! \{
// ====================================================================================================================
// Class definition
// ====================================================================================================================

class Reshape
{
protected:
  SliceReshapeInfo        m_sliceReshapeInfo;
  bool                    m_ctuFlag;
  bool                    m_recReshaped;
  std::vector<Pel>        m_invLUT;
  std::vector<Pel>        m_fwdLUT;
  std::vector<int>        m_chromaAdjHelpLUT;
  std::vector<uint16_t>   m_binCW;
  uint16_t                m_initCW;
  bool                    m_reshape;
  std::vector<Pel>        m_reshapePivot;
  std::vector<Pel>        m_inputPivot;
  std::vector<int32_t>    m_fwdScaleCoef;
  std::vector<int32_t>    m_invScaleCoef;
  int                     m_lumaBD;
  int                     m_reshapeLUTSize;
  int                     m_chromaScale;
  int                     m_vpduX;
  int                     m_vpduY;
public:
  Reshape();
  ~Reshape();

  void createDec(int bitDepth);
  void destroy();

  std::vector<Pel>&  getFwdLUT() { return m_fwdLUT; }
  std::vector<Pel>&  getInvLUT() { return m_invLUT; }
  std::vector<int>&  getChromaAdjHelpLUT() { return m_chromaAdjHelpLUT; }

  bool getCTUFlag() { return m_ctuFlag; }
  void setCTUFlag(bool b) { m_ctuFlag = b; }

  bool getRecReshaped()          { return m_recReshaped; }
  void setRecReshaped(bool b)    { m_recReshaped = b; }
  int  calculateChromaAdj(Pel avgLuma);
  int  getPWLIdxInv(int lumaVal);
  SliceReshapeInfo& getSliceReshaperInfo() { return m_sliceReshapeInfo; }
  void copySliceReshaperInfo(SliceReshapeInfo& tInfo, SliceReshapeInfo& sInfo);

  void constructReshaper();
  bool getReshapeFlag() { return m_reshape; }
  void setReshapeFlag(bool b) { m_reshape = b; }
  int  calculateChromaAdjVpduNei(TransformUnit &tu, const CompArea &areaY);
  void setVPDULoc(int x, int y) { m_vpduX = x, m_vpduY = y; }
  bool isVPDUprocessed(int x, int y) { return ((x == m_vpduX) && (y == m_vpduY)); }
  void setChromaScale (int chromaScale) { m_chromaScale = chromaScale; }
  int  getChromaScale() { return m_chromaScale; }
};// END CLASS DEFINITION Reshape

//! \}
#endif // __RESHAPE__


