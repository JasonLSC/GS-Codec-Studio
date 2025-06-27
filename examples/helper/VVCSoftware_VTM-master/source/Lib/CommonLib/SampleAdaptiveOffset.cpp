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

/** \file     SampleAdaptiveOffset.cpp
    \brief    sample adaptive offset class
*/

#include "SampleAdaptiveOffset.h"

#include "UnitTools.h"
#include "UnitPartitioner.h"
#include "CodingStructure.h"
#include "CommonLib/dtrace_codingstruct.h"
#include "CommonLib/dtrace_buffer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

//! \ingroup CommonLib
//! \{

SAOOffset::SAOOffset()
{
  reset();
}

SAOOffset::~SAOOffset()
{

}

void SAOOffset::reset()
{
  modeIdc         = SAOMode::OFF;
  typeIdc.newType = SAOModeNewTypes::NONE;
  typeAuxInfo = -1;
  ::memset(offset, 0, sizeof(int)* MAX_NUM_SAO_CLASSES);
}

const SAOOffset& SAOOffset::operator= (const SAOOffset& src)
{
  modeIdc = src.modeIdc;
  typeIdc = src.typeIdc;
  typeAuxInfo = src.typeAuxInfo;
  ::memcpy(offset, src.offset, sizeof(int)* MAX_NUM_SAO_CLASSES);

  return *this;
}


SAOBlkParam::SAOBlkParam()
{
  reset();
}

SAOBlkParam::~SAOBlkParam()
{

}

void SAOBlkParam::reset()
{
  for(int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++)
  {
    offsetParam[compIdx].reset();
  }
}

const SAOBlkParam& SAOBlkParam::operator= (const SAOBlkParam& src)
{
  for(int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++)
  {
    offsetParam[compIdx] = src.offsetParam[compIdx];
  }
  return *this;
}


SampleAdaptiveOffset::SampleAdaptiveOffset()
{
  m_numberOfComponents = 0;
}

SampleAdaptiveOffset::~SampleAdaptiveOffset()
{
  destroy();

  m_signLineBuf1.clear();
  m_signLineBuf2.clear();
}

void SampleAdaptiveOffset::create(int picWidth, int picHeight, ChromaFormat format, uint32_t maxCUWidth,
                                  uint32_t maxCUHeight, uint32_t maxCUDepth, uint32_t lumaBitShift,
                                  uint32_t chromaBitShift)
{
  //temporary picture buffer
  UnitArea picArea(format, Area(0, 0, picWidth, picHeight));

  m_tempBuf.destroy();
  m_tempBuf.create( picArea );

  //bit-depth related
  for(int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++)
  {
    m_offsetStepLog2  [compIdx] = isLuma(ComponentID(compIdx))? lumaBitShift : chromaBitShift;
  }
  m_numberOfComponents = getNumberValidComponents(format);
}

void SampleAdaptiveOffset::destroy()
{
  m_tempBuf.destroy();
}

void SampleAdaptiveOffset::invertQuantOffsets(ComponentID compIdx, SAOModeNewTypes typeIdc, int typeAuxInfo,
                                              int *dstOffsets, int *srcOffsets)
{
  int codedOffset[MAX_NUM_SAO_CLASSES];

  ::memcpy(codedOffset, srcOffsets, sizeof(int)*MAX_NUM_SAO_CLASSES);
  ::memset(dstOffsets, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);

  if (typeIdc == SAOModeNewTypes::START_BO)
  {
    for(int i=0; i< 4; i++)
    {
      dstOffsets[(typeAuxInfo+ i)%NUM_SAO_BO_CLASSES] = codedOffset[(typeAuxInfo+ i)%NUM_SAO_BO_CLASSES]*(1<<m_offsetStepLog2[compIdx]);
    }
  }
  else //EO
  {
    for(int i=0; i< NUM_SAO_EO_CLASSES; i++)
    {
      dstOffsets[i] = codedOffset[i] *(1<<m_offsetStepLog2[compIdx]);
    }
    CHECK(dstOffsets[SAO_CLASS_EO_PLAIN] != 0, "EO offset is not '0'"); //keep EO plain offset as zero
  }
}

int SampleAdaptiveOffset::getMergeList(CodingStructure &cs, int ctuRsAddr, SAOBlkParam *blkParams,
                                       MergeBlkParams &mergeList)
{
  const PreCalcValues& pcv = *cs.pcv;

  int ctuX = ctuRsAddr % pcv.widthInCtus;
  int ctuY = ctuRsAddr / pcv.widthInCtus;
  const CodingUnit &cu   = *cs.getCU(Position(ctuX * pcv.maxCUWidth, ctuY * pcv.maxCUHeight), ChannelType::LUMA);
  int mergedCTUPos;
  int numValidMergeCandidates = 0;

  for (const auto mergeType: { SAOModeMergeTypes::LEFT, SAOModeMergeTypes::ABOVE })
  {
    SAOBlkParam *mergeCandidate = nullptr;

    switch(mergeType)
    {
    case SAOModeMergeTypes::ABOVE:
      if (ctuY > 0)
      {
        mergedCTUPos = ctuRsAddr - pcv.widthInCtus;
        if (cs.getCURestricted(Position(ctuX * pcv.maxCUWidth, (ctuY - 1) * pcv.maxCUHeight), cu, cu.chType))
        {
          mergeCandidate = &(blkParams[mergedCTUPos]);
        }
      }
      break;
    case SAOModeMergeTypes::LEFT:
      if (ctuX > 0)
      {
        mergedCTUPos = ctuRsAddr - 1;
        if (cs.getCURestricted(Position((ctuX - 1) * pcv.maxCUWidth, ctuY * pcv.maxCUHeight), cu, cu.chType))
        {
          mergeCandidate = &(blkParams[mergedCTUPos]);
        }
      }
      break;
    default:
      THROW("not a supported merge type");
      break;
    }

    mergeList[mergeType]=mergeCandidate;
    if (mergeCandidate != nullptr)
    {
      numValidMergeCandidates++;
    }
  }

  return numValidMergeCandidates;
}

void SampleAdaptiveOffset::reconstructBlkSAOParam(SAOBlkParam &recParam, MergeBlkParams &mergeList)
{
  const int numberOfComponents = m_numberOfComponents;
  for(int compIdx = 0; compIdx < numberOfComponents; compIdx++)
  {
    const ComponentID component = ComponentID(compIdx);
    SAOOffset& offsetParam = recParam[component];

    if (offsetParam.modeIdc == SAOMode::OFF)
    {
      continue;
    }

    switch(offsetParam.modeIdc)
    {
    case SAOMode::NEW:
      invertQuantOffsets(component, offsetParam.typeIdc.newType, offsetParam.typeAuxInfo, offsetParam.offset,
                         offsetParam.offset);
      break;
    case SAOMode::MERGE:
    {
      SAOBlkParam *mergeTarget = mergeList[offsetParam.typeIdc.mergeType];
      CHECK(mergeTarget == nullptr, "Merge target does not exist");

      offsetParam = (*mergeTarget)[component];
      break;
    }
    default:
      THROW("Not a supported mode");
      break;
    }
  }
}

void SampleAdaptiveOffset::xReconstructBlkSAOParams(CodingStructure& cs, SAOBlkParam* saoBlkParams)
{
  for(uint32_t compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++)
  {
    m_picSAOEnabled[compIdx] = false;
  }

  const uint32_t numberOfComponents = getNumberValidComponents(cs.pcv->chrFormat);

  for(int ctuRsAddr=0; ctuRsAddr< cs.pcv->sizeInCtus; ctuRsAddr++)
  {
    MergeBlkParams mergeList;
    mergeList.fill(nullptr);
    getMergeList(cs, ctuRsAddr, saoBlkParams, mergeList);

    reconstructBlkSAOParam(saoBlkParams[ctuRsAddr], mergeList);

    for(uint32_t compIdx = 0; compIdx < numberOfComponents; compIdx++)
    {
      if (saoBlkParams[ctuRsAddr][compIdx].modeIdc != SAOMode::OFF)
      {
        m_picSAOEnabled[compIdx] = true;
      }
    }
  }
}

void SampleAdaptiveOffset::offsetBlock(const int channelBitDepth, const ClpRng &clpRng, SAOModeNewTypes typeIdx,
                                       int *offset, const Pel *srcBlk, Pel *resBlk, ptrdiff_t srcStride,
                                       ptrdiff_t resStride, int width, int height, bool isLeftAvail, bool isRightAvail,
                                       bool isAboveAvail, bool isBelowAvail, bool isAboveLeftAvail,
                                       bool isAboveRightAvail, bool isBelowLeftAvail, bool isBelowRightAvail,
                                       bool isCtuCrossedByVirtualBoundaries, int horVirBndryPos[], int verVirBndryPos[],
                                       int numHorVirBndry, int numVerVirBndry)
{
  int x,y, startX, startY, endX, endY, edgeType;
  int firstLineStartX, firstLineEndX, lastLineStartX, lastLineEndX;
  int8_t signLeft, signRight, signDown;

  const Pel* srcLine = srcBlk;
        Pel* resLine = resBlk;

  switch(typeIdx)
  {
  case SAOModeNewTypes::EO_0:
  {
    offset += 2;
    startX = isLeftAvail ? 0 : 1;
    endX   = isRightAvail ? width : (width - 1);
    for (y = 0; y < height; y++)
    {
      signLeft = (int8_t) sgn(srcLine[startX] - srcLine[startX - 1]);
      for (x = startX; x < endX; x++)
      {
        signRight = (int8_t) sgn(srcLine[x] - srcLine[x + 1]);
        if (isCtuCrossedByVirtualBoundaries
            && isProcessDisabled(x, y, numVerVirBndry, 0, verVirBndryPos, horVirBndryPos))
        {
          signLeft = -signRight;
          continue;
        }
        edgeType = signRight + signLeft;
        signLeft = -signRight;

        resLine[x] = ClipPel<int>(srcLine[x] + offset[edgeType], clpRng);
      }
      srcLine += srcStride;
      resLine += resStride;
    }
  }
    break;
    case SAOModeNewTypes::EO_90:
    {
      offset += 2;
      int8_t *signUpLine = m_signLineBuf1.data();

      startY = isAboveAvail ? 0 : 1;
      endY   = isBelowAvail ? height : height-1;
      if (!isAboveAvail)
      {
        srcLine += srcStride;
        resLine += resStride;
      }

      const Pel* srcLineAbove= srcLine- srcStride;
      for (x=0; x< width; x++)
      {
        signUpLine[x] = (int8_t)sgn(srcLine[x] - srcLineAbove[x]);
      }

      const Pel* srcLineBelow;
      for (y=startY; y<endY; y++)
      {
        srcLineBelow= srcLine+ srcStride;

        for (x=0; x< width; x++)
        {
          signDown  = (int8_t)sgn(srcLine[x] - srcLineBelow[x]);
          if (isCtuCrossedByVirtualBoundaries && isProcessDisabled(x, y, 0, numHorVirBndry, verVirBndryPos, horVirBndryPos))
          {
            signUpLine[x] = -signDown;
            continue;
          }
          edgeType = signDown + signUpLine[x];
          signUpLine[x]= -signDown;

          resLine[x] = ClipPel<int>(srcLine[x] + offset[edgeType], clpRng);
        }
        srcLine += srcStride;
        resLine += resStride;
      }

    }
    break;
    case SAOModeNewTypes::EO_135:
    {
      offset += 2;
      int8_t *signTmpLine;

      int8_t *signUpLine   = m_signLineBuf1.data();
      int8_t *signDownLine = m_signLineBuf2.data();

      startX = isLeftAvail ? 0 : 1 ;
      endX   = isRightAvail ? width : (width-1);

      //prepare 2nd line's upper sign
      const Pel* srcLineBelow= srcLine+ srcStride;
      for (x=startX; x< endX+1; x++)
      {
        signUpLine[x] = (int8_t)sgn(srcLineBelow[x] - srcLine[x- 1]);
      }

      //1st line
      const Pel* srcLineAbove= srcLine- srcStride;
      firstLineStartX = isAboveLeftAvail ? 0 : 1;
      firstLineEndX   = isAboveAvail? endX: 1;
      for(x= firstLineStartX; x< firstLineEndX; x++)
      {
        if (isCtuCrossedByVirtualBoundaries && isProcessDisabled(x, 0, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
        {
          continue;
        }
        edgeType  =  sgn(srcLine[x] - srcLineAbove[x- 1]) - signUpLine[x+1];

        resLine[x] = ClipPel<int>( srcLine[x] + offset[edgeType], clpRng);
      }
      srcLine  += srcStride;
      resLine  += resStride;

      //middle lines
      for (y= 1; y< height-1; y++)
      {
        srcLineBelow= srcLine+ srcStride;

        for (x=startX; x<endX; x++)
        {
          signDown =  (int8_t)sgn(srcLine[x] - srcLineBelow[x+ 1]);
          if (isCtuCrossedByVirtualBoundaries && isProcessDisabled(x, y, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
          {
            signDownLine[x + 1] = -signDown;
            continue;
          }
          edgeType =  signDown + signUpLine[x];
          resLine[x] = ClipPel<int>( srcLine[x] + offset[edgeType], clpRng);

          signDownLine[x+1] = -signDown;
        }
        signDownLine[startX] = (int8_t)sgn(srcLineBelow[startX] - srcLine[startX-1]);

        signTmpLine  = signUpLine;
        signUpLine   = signDownLine;
        signDownLine = signTmpLine;

        srcLine += srcStride;
        resLine += resStride;
      }

      //last line
      srcLineBelow= srcLine+ srcStride;
      lastLineStartX = isBelowAvail ? startX : (width -1);
      lastLineEndX   = isBelowRightAvail ? width : (width -1);
      for(x= lastLineStartX; x< lastLineEndX; x++)
      {
        if (isCtuCrossedByVirtualBoundaries && isProcessDisabled(x, height - 1, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
        {
          continue;
        }
        edgeType =  sgn(srcLine[x] - srcLineBelow[x+ 1]) + signUpLine[x];
        resLine[x] = ClipPel<int>( srcLine[x] + offset[edgeType], clpRng);

      }
    }
    break;
    case SAOModeNewTypes::EO_45:
    {
      offset += 2;
      int8_t *signUpLine = m_signLineBuf1.data() + 1;

      startX = isLeftAvail ? 0 : 1;
      endX   = isRightAvail ? width : (width -1);

      //prepare 2nd line upper sign
      const Pel* srcLineBelow= srcLine+ srcStride;
      for (x=startX-1; x< endX; x++)
      {
        signUpLine[x] = (int8_t)sgn(srcLineBelow[x] - srcLine[x+1]);
      }


      //first line
      const Pel* srcLineAbove= srcLine- srcStride;
      firstLineStartX = isAboveAvail ? startX : (width -1 );
      firstLineEndX   = isAboveRightAvail ? width : (width-1);
      for(x= firstLineStartX; x< firstLineEndX; x++)
      {
        if (isCtuCrossedByVirtualBoundaries && isProcessDisabled(x, 0, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
        {
          continue;
        }
        edgeType = sgn(srcLine[x] - srcLineAbove[x+1]) -signUpLine[x-1];
        resLine[x] = ClipPel<int>(srcLine[x] + offset[edgeType], clpRng);
      }
      srcLine += srcStride;
      resLine += resStride;

      //middle lines
      for (y= 1; y< height-1; y++)
      {
        srcLineBelow= srcLine+ srcStride;

        for(x= startX; x< endX; x++)
        {
          signDown =  (int8_t)sgn(srcLine[x] - srcLineBelow[x-1]);
          if (isCtuCrossedByVirtualBoundaries && isProcessDisabled(x, y, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
          {
            signUpLine[x - 1] = -signDown;
            continue;
          }
          edgeType =  signDown + signUpLine[x];
          resLine[x] = ClipPel<int>(srcLine[x] + offset[edgeType], clpRng);
          signUpLine[x-1] = -signDown;
        }
        signUpLine[endX-1] = (int8_t)sgn(srcLineBelow[endX-1] - srcLine[endX]);
        srcLine  += srcStride;
        resLine += resStride;
      }

      //last line
      srcLineBelow= srcLine+ srcStride;
      lastLineStartX = isBelowLeftAvail ? 0 : 1;
      lastLineEndX   = isBelowAvail ? endX : 1;
      for(x= lastLineStartX; x< lastLineEndX; x++)
      {
        if (isCtuCrossedByVirtualBoundaries && isProcessDisabled(x, height - 1, numVerVirBndry, numHorVirBndry, verVirBndryPos, horVirBndryPos))
        {
          continue;
        }
        edgeType = sgn(srcLine[x] - srcLineBelow[x-1]) + signUpLine[x];
        resLine[x] = ClipPel<int>(srcLine[x] + offset[edgeType], clpRng);
      }
    }
    break;
    case SAOModeNewTypes::BO:
    {
      const int shiftBits = channelBitDepth - NUM_SAO_BO_CLASSES_LOG2;
      for (y=0; y< height; y++)
      {
        for (x=0; x< width; x++)
        {
          resLine[x] = ClipPel<int>(srcLine[x] + offset[srcLine[x] >> shiftBits], clpRng );
        }
        srcLine += srcStride;
        resLine += resStride;
      }
    }
    break;
  default:
    {
      THROW("Not a supported SAO types\n");
    }
  }
}

void SampleAdaptiveOffset::offsetCTU(const UnitArea &area, const CPelUnitBuf &src, PelUnitBuf &res,
                                     SAOBlkParam &saoblkParam, CodingStructure &cs)
{
  const uint32_t numberOfComponents = getNumberValidComponents( area.chromaFormat );

  bool allOff = true;
  for (int compIdx = 0; compIdx < numberOfComponents; compIdx++)
  {
    if (saoblkParam[compIdx].modeIdc != SAOMode::OFF)
    {
      allOff = false;
    }
  }
  if (allOff)
  {
    return;
  }

  bool isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail, isAboveLeftAvail, isAboveRightAvail, isBelowLeftAvail, isBelowRightAvail;

  //block boundary availability
  deriveLoopFilterBoundaryAvailability(cs, area.Y(), isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail,
                                       isAboveLeftAvail, isAboveRightAvail, isBelowLeftAvail, isBelowRightAvail);

  const size_t lineBufferSize = area.Y().width + 1;
  if (m_signLineBuf1.size() < lineBufferSize)
  {
    m_signLineBuf1.resize(lineBufferSize);
    m_signLineBuf2.resize(lineBufferSize);
  }

  int numHorVirBndry = 0, numVerVirBndry = 0;
  int horVirBndryPos[] = { -1,-1,-1 };
  int verVirBndryPos[] = { -1,-1,-1 };
  int horVirBndryPosComp[] = { -1,-1,-1 };
  int verVirBndryPosComp[] = { -1,-1,-1 };
  const bool isCtuCrossedByVirtualBoundaries =
    isCrossedByVirtualBoundaries(area.Y().x, area.Y().y, area.Y().width, area.Y().height, numHorVirBndry,
                                 numVerVirBndry, horVirBndryPos, verVirBndryPos, cs.picHeader);
  for(int compIdx = 0; compIdx < numberOfComponents; compIdx++)
  {
    const ComponentID compID = ComponentID(compIdx);
    const CompArea& compArea = area.block(compID);
    SAOOffset& ctbOffset     = saoblkParam[compIdx];

    if (ctbOffset.modeIdc != SAOMode::OFF)
    {
      const ptrdiff_t srcStride = src.get(compID).stride;
      const ptrdiff_t resStride = res.get(compID).stride;

      const Pel *srcBlk       = src.get(compID).bufAt(compArea);
      Pel* resBlk       = res.get(compID).bufAt(compArea);
      for (int i = 0; i < numHorVirBndry; i++)
      {
        horVirBndryPosComp[i] = (horVirBndryPos[i] >> ::getComponentScaleY(compID, area.chromaFormat)) - compArea.y;
      }
      for (int i = 0; i < numVerVirBndry; i++)
      {
        verVirBndryPosComp[i] = (verVirBndryPos[i] >> ::getComponentScaleX(compID, area.chromaFormat)) - compArea.x;
      }
#if GREEN_METADATA_SEI_ENABLED
      if (ctbOffset.typeIdc.newType == SAOModeNewTypes::START_BO)
      {
        if (compID == COMPONENT_Y)
        {
          cs.m_featureCounter.saoLumaBO++;
          cs.m_featureCounter.saoLumaPels += area.lumaSize().width * area.lumaSize().height;
        }
        else
        {
          cs.m_featureCounter.saoChromaBO++;
          cs.m_featureCounter.saoChromaPels += area.chromaSize().width * area.chromaSize().height;
        }
      }
      else if (ctbOffset.typeIdc.newType == SAOModeNewTypes::EO_0 || ctbOffset.typeIdc.newType == SAOModeNewTypes::EO_135 || ctbOffset.typeIdc.newType == SAOModeNewTypes::EO_45 ||ctbOffset.typeIdc.newType == SAOModeNewTypes::EO_90 )
      {
        if (compID == COMPONENT_Y)
        {
          cs.m_featureCounter.saoLumaEO++;
          cs.m_featureCounter.saoLumaPels += area.lumaSize().width * area.lumaSize().height;
        }
        else
        {
          cs.m_featureCounter.saoChromaEO++;
          cs.m_featureCounter.saoChromaPels += area.chromaSize().width * area.chromaSize().height;
        }
      }
#endif
      offsetBlock(cs.sps->getBitDepth(toChannelType(compID)), cs.slice->clpRng(compID), ctbOffset.typeIdc.newType,
                  ctbOffset.offset, srcBlk, resBlk, srcStride, resStride, compArea.width, compArea.height, isLeftAvail,
                  isRightAvail, isAboveAvail, isBelowAvail, isAboveLeftAvail, isAboveRightAvail, isBelowLeftAvail,
                  isBelowRightAvail, isCtuCrossedByVirtualBoundaries, horVirBndryPosComp, verVirBndryPosComp,
                  numHorVirBndry, numVerVirBndry);
    }
  } //compIdx
}

void SampleAdaptiveOffset::SAOProcess( CodingStructure& cs, SAOBlkParam* saoBlkParams
                                      )
{
  CHECK(!saoBlkParams, "No parameters present");

  xReconstructBlkSAOParams(cs, saoBlkParams);

  const uint32_t numberOfComponents = getNumberValidComponents(cs.area.chromaFormat);

  bool allDisabled = true;
  for (uint32_t compIdx = 0; compIdx < numberOfComponents; compIdx++)
  {
    if (m_picSAOEnabled[compIdx])
    {
      allDisabled = false;
    }
  }
  if (allDisabled)
  {
    return;
  }

  const PreCalcValues& pcv = *cs.pcv;
  PelUnitBuf rec = cs.getRecoBuf();
  m_tempBuf.copyFrom( rec );

  int ctuRsAddr = 0;
  for( uint32_t yPos = 0; yPos < pcv.lumaHeight; yPos += pcv.maxCUHeight )
  {
    for (uint32_t xPos = 0; xPos < pcv.lumaWidth; xPos += pcv.maxCUWidth, ctuRsAddr++)
    {
      const uint32_t width  = (xPos + pcv.maxCUWidth  > pcv.lumaWidth)  ? (pcv.lumaWidth - xPos)  : pcv.maxCUWidth;
      const uint32_t height = (yPos + pcv.maxCUHeight > pcv.lumaHeight) ? (pcv.lumaHeight - yPos) : pcv.maxCUHeight;
      const UnitArea area( cs.area.chromaFormat, Area(xPos , yPos, width, height) );

      offsetCTU(area, m_tempBuf, rec, cs.picture->getSAO()[ctuRsAddr], cs);
    }
  }

  DTRACE_UPDATE(g_trace_ctx, (std::make_pair("poc", cs.slice->getPOC())));
  DTRACE_PIC_COMP(D_REC_CB_LUMA_SAO, cs, cs.getRecoBuf(), COMPONENT_Y);
  DTRACE_PIC_COMP(D_REC_CB_CHROMA_SAO, cs, cs.getRecoBuf(), COMPONENT_Cb);
  DTRACE_PIC_COMP(D_REC_CB_CHROMA_SAO, cs, cs.getRecoBuf(), COMPONENT_Cr);

  DTRACE    ( g_trace_ctx, D_CRC, "SAO" );
  DTRACE_CRC( g_trace_ctx, D_CRC, cs, cs.getRecoBuf() );
}

void SampleAdaptiveOffset::deriveLoopFilterBoundaryAvailability(CodingStructure &cs, const Position &pos,
                                                                bool &isLeftAvail, bool &isRightAvail,
                                                                bool &isAboveAvail, bool &isBelowAvail,
                                                                bool &isAboveLeftAvail, bool &isAboveRightAvail,
                                                                bool &isBelowLeftAvail, bool &isBelowRightAvail) const
{
  const int width = cs.pcv->maxCUWidth;
  const int height = cs.pcv->maxCUHeight;
  const CodingUnit *cuCurr       = cs.getCU(pos, ChannelType::LUMA);
  const CodingUnit *cuLeft       = cs.getCU(pos.offset(-width, 0), ChannelType::LUMA);
  const CodingUnit *cuRight      = cs.getCU(pos.offset(width, 0), ChannelType::LUMA);
  const CodingUnit *cuAbove      = cs.getCU(pos.offset(0, -height), ChannelType::LUMA);
  const CodingUnit *cuBelow      = cs.getCU(pos.offset(0, height), ChannelType::LUMA);
  const CodingUnit *cuAboveLeft  = cs.getCU(pos.offset(-width, -height), ChannelType::LUMA);
  const CodingUnit *cuAboveRight = cs.getCU(pos.offset(width, -height), ChannelType::LUMA);
  const CodingUnit *cuBelowLeft  = cs.getCU(pos.offset(-width, height), ChannelType::LUMA);
  const CodingUnit *cuBelowRight = cs.getCU(pos.offset(width, height), ChannelType::LUMA);

  // check cross slice flags
  const bool isLoopFilterAcrossSlicePPS = cs.pps->getLoopFilterAcrossSlicesEnabledFlag();
  if (!isLoopFilterAcrossSlicePPS)
  {
    isLeftAvail       = (cuLeft == nullptr) ? false : CU::isSameSlice(*cuCurr, *cuLeft);
    isAboveAvail      = (cuAbove == nullptr) ? false : CU::isSameSlice(*cuCurr, *cuAbove);
    isRightAvail      = (cuRight == nullptr) ? false : CU::isSameSlice(*cuCurr, *cuRight);
    isBelowAvail      = (cuBelow == nullptr) ? false : CU::isSameSlice(*cuCurr, *cuBelow);
    isAboveLeftAvail  = (cuAboveLeft == nullptr) ? false : CU::isSameSlice(*cuCurr, *cuAboveLeft);
    isAboveRightAvail = (cuAboveRight == nullptr) ? false : CU::isSameSlice(*cuCurr, *cuAboveRight);
    isBelowLeftAvail  = (cuBelowLeft == nullptr) ? false : CU::isSameSlice(*cuCurr, *cuBelowLeft);
    isBelowRightAvail = (cuBelowRight == nullptr) ? false : CU::isSameSlice(*cuCurr, *cuBelowRight);
  }
  else
  {
    isLeftAvail       = (cuLeft != nullptr);
    isAboveAvail      = (cuAbove != nullptr);
    isRightAvail      = (cuRight != nullptr);
    isBelowAvail      = (cuBelow != nullptr);
    isAboveLeftAvail  = (cuAboveLeft != nullptr);
    isAboveRightAvail = (cuAboveRight != nullptr);
    isBelowLeftAvail  = (cuBelowLeft != nullptr);
    isBelowRightAvail = (cuBelowRight != nullptr);
  }

  // check cross tile flags
  const bool isLoopFilterAcrossTilePPS = cs.pps->getLoopFilterAcrossTilesEnabledFlag();
  if (!isLoopFilterAcrossTilePPS)
  {
    isLeftAvail       = (!isLeftAvail)       ? false : CU::isSameTile(*cuCurr, *cuLeft);
    isAboveAvail      = (!isAboveAvail)      ? false : CU::isSameTile(*cuCurr, *cuAbove);
    isRightAvail      = (!isRightAvail)      ? false : CU::isSameTile(*cuCurr, *cuRight);
    isBelowAvail      = (!isBelowAvail)      ? false : CU::isSameTile(*cuCurr, *cuBelow);
    isAboveLeftAvail  = (!isAboveLeftAvail)  ? false : CU::isSameTile(*cuCurr, *cuAboveLeft);
    isAboveRightAvail = (!isAboveRightAvail) ? false : CU::isSameTile(*cuCurr, *cuAboveRight);
    isBelowLeftAvail  = (!isBelowLeftAvail)  ? false : CU::isSameTile(*cuCurr, *cuBelowLeft);
    isBelowRightAvail = (!isBelowRightAvail) ? false : CU::isSameTile(*cuCurr, *cuBelowRight);
  }

  // check cross subpic flags
  const SubPic& curSubPic = cs.pps->getSubPicFromCU(*cuCurr);
  if (!curSubPic.getloopFilterAcrossEnabledFlag())
  {
    isLeftAvail       = (!isLeftAvail)       ? false : CU::isSameSubPic(*cuCurr, *cuLeft);
    isAboveAvail      = (!isAboveAvail)      ? false : CU::isSameSubPic(*cuCurr, *cuAbove);
    isRightAvail      = (!isRightAvail)      ? false : CU::isSameSubPic(*cuCurr, *cuRight);
    isBelowAvail      = (!isBelowAvail)      ? false : CU::isSameSubPic(*cuCurr, *cuBelow);
    isAboveLeftAvail  = (!isAboveLeftAvail)  ? false : CU::isSameSubPic(*cuCurr, *cuAboveLeft);
    isAboveRightAvail = (!isAboveRightAvail) ? false : CU::isSameSubPic(*cuCurr, *cuAboveRight);
    isBelowLeftAvail  = (!isBelowLeftAvail)  ? false : CU::isSameSubPic(*cuCurr, *cuBelowLeft);
    isBelowRightAvail = (!isBelowRightAvail) ? false : CU::isSameSubPic(*cuCurr, *cuBelowRight);
  }
}

bool SampleAdaptiveOffset::isCrossedByVirtualBoundaries(const int xPos, const int yPos, const int width,
                                                        const int height, int &numHorVirBndry, int &numVerVirBndry,
                                                        int horVirBndryPos[], int verVirBndryPos[],
                                                        const PicHeader *picHeader)
{
  numHorVirBndry = 0;
  numVerVirBndry = 0;

  if (picHeader->getVirtualBoundariesPresentFlag())
  {
    for (int i = 0; i < picHeader->getNumHorVirtualBoundaries(); i++)
    {
      const int vbPosY = picHeader->getVirtualBoundariesPosY(i);
      if (yPos <= vbPosY && vbPosY <= yPos + height)
      {
        horVirBndryPos[numHorVirBndry++] = vbPosY;
      }
    }
    for (int i = 0; i < picHeader->getNumVerVirtualBoundaries(); i++)
    {
      const int vbPosX = picHeader->getVirtualBoundariesPosX(i);
      if (xPos <= vbPosX && vbPosX <= xPos + width)
      {
        verVirBndryPos[numVerVirBndry++] = vbPosX;
      }
    }
  }
  return numHorVirBndry > 0 || numVerVirBndry > 0 ;
}
//! \}
