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

/** \file     WeightPrediction.h
    \brief    weighting prediction class (header)
*/

// Include files
#include "CommonDef.h"
#include "Unit.h"
#include "InterpolationFilter.h"
#include "WeightPrediction.h"
#include "CodingStructure.h"

static inline Pel weightBidir(const int w0, const Pel p0, const int w1, const Pel p1, const int round, const int shift,
                              const int offset, const ClpRng &clpRng)
{
  return ClipPel(
    ((w0 * (p0 + IF_INTERNAL_OFFS) + w1 * (p1 + IF_INTERNAL_OFFS) + round + (offset * (1 << (shift - 1)))) >> shift),
    clpRng);
}

static inline Pel weightUnidir(const int w0, const Pel p0, const int round, const int shift, const int offset,
                               const ClpRng &clpRng)
{
  return ClipPel(((w0 * (p0 + IF_INTERNAL_OFFS) + round) >> shift) + offset, clpRng);
}

static inline Pel noWeightUnidir(const Pel p0, const int round, const int shift, const int offset, const ClpRng &clpRng)
{
  return ClipPel((((p0 + IF_INTERNAL_OFFS) + round) >> shift) + offset, clpRng);
}

static inline Pel noWeightOffsetUnidir(const Pel p0, const int round, const int shift, const ClpRng &clpRng)
{
  return ClipPel((((p0 + IF_INTERNAL_OFFS) + round) >> shift), clpRng);
}


// ====================================================================================================================
// Class definition
// ====================================================================================================================

WeightPrediction::WeightPrediction()
{
}

void WeightPrediction::getWpScaling(Slice *pcSlice, const int refIdx0, const int refIdx1, WPScalingParam *&wp0,
                                    WPScalingParam *&wp1, const ComponentID maxNumComp)
{
  CHECK(refIdx0 < 0 && refIdx1 < 0, "Both picture reference list indices smaller than 0");

  const bool wpBiPred = pcSlice->getPPS()->getWPBiPred();
  const bool biPred   = refIdx0 >= 0 && refIdx1 >= 0;

  CHECK(biPred && !wpBiPred, "Unsupported WP configuration");

  wp0 = pcSlice->getWpScaling(REF_PIC_LIST_0, refIdx0);
  wp1 = pcSlice->getWpScaling(REF_PIC_LIST_1, refIdx1);

  const int numValidComponent = getNumberValidComponents(pcSlice->getSPS()->getChromaFormatIdc());

  const bool useHighPrecisionPredictionWeighting =
    pcSlice->getSPS()->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag();

  if (biPred)
  {
    // Bi-predictive case
    for (int yuv = 0; yuv < numValidComponent && yuv <= maxNumComp; yuv++)
    {
      const int bitDepth = pcSlice->getSPS()->getBitDepth(toChannelType(ComponentID(yuv)));

      const int offsetScalingFactor = useHighPrecisionPredictionWeighting ? 1 : (1 << (bitDepth - 8));

      wp0[yuv].w      = wp0[yuv].codedWeight;
      wp0[yuv].o      = wp0[yuv].codedOffset * offsetScalingFactor;

      wp1[yuv].w      = wp1[yuv].codedWeight;
      wp1[yuv].o      = wp1[yuv].codedOffset * offsetScalingFactor;

      wp0[yuv].offset = wp0[yuv].o + wp1[yuv].o;
      wp0[yuv].shift  = wp0[yuv].log2WeightDenom + 1;
      wp0[yuv].round  = 1 << wp0[yuv].log2WeightDenom;

      wp1[yuv].offset = wp0[yuv].offset;
      wp1[yuv].shift  = wp0[yuv].shift;
      wp1[yuv].round  = wp0[yuv].round;
    }
  }
  else
  {
    // UniPred
    WPScalingParam *const pwp = (refIdx0 >= 0) ? wp0 : wp1;

    for (int yuv = 0; yuv < numValidComponent && yuv <= maxNumComp; yuv++)
    {
      const int bitDepth = pcSlice->getSPS()->getBitDepth(toChannelType(ComponentID(yuv)));

      const int offsetScalingFactor = useHighPrecisionPredictionWeighting ? 1 : (1 << (bitDepth - 8));

      pwp[yuv].w      = pwp[yuv].codedWeight;
      pwp[yuv].offset = pwp[yuv].codedOffset * offsetScalingFactor;
      pwp[yuv].shift  = pwp[yuv].log2WeightDenom;
      pwp[yuv].round  = 1 << pwp[yuv].log2WeightDenom >> 1;
    }
  }
}

void WeightPrediction::addWeightBi(const CPelUnitBuf &pcYuvSrc0, const CPelUnitBuf &pcYuvSrc1, const ClpRngs &clpRngs,
                                   const WPScalingParam *const wp0, const WPScalingParam *const wp1,
                                   PelUnitBuf &rpcYuvDst, const ComponentID maxNumComp, bool lumaOnly, bool chromaOnly)
{
  CHECK(lumaOnly && chromaOnly, "Not allowed to have both lumaOnly and chromaOnly selected");

  const int numValidComponent = (int) pcYuvSrc0.bufs.size();
  const int firstComponent    = chromaOnly ? 1 : 0;
  const int lastComponent     = lumaOnly ? 0 : maxNumComp;

  for (int componentIndex = firstComponent; componentIndex < numValidComponent && componentIndex <= lastComponent; componentIndex++)
  {
    const ComponentID compID = ComponentID(componentIndex);

    const Pel *pSrc0 = pcYuvSrc0.bufs[compID].buf;
    const Pel *pSrc1 = pcYuvSrc1.bufs[compID].buf;
    Pel *      pDst  = rpcYuvDst.bufs[compID].buf;

    const ClpRng &clpRng   = clpRngs.comp[compID];

    const int w0       = wp0[compID].w;
    const int offset   = wp0[compID].offset;
    const int clipBD   = clpRng.bd;
    const int shiftNum = IF_INTERNAL_FRAC_BITS(clipBD);
    const int shift    = wp0[compID].shift + shiftNum;
    const int round    = 1 << shift >> 1;
    const int w1       = wp1[compID].w;

    const int height = rpcYuvDst.bufs[compID].height;
    const int width  = rpcYuvDst.bufs[compID].width;

    const ptrdiff_t src0Stride = pcYuvSrc0.bufs[compID].stride;
    const ptrdiff_t src1Stride = pcYuvSrc1.bufs[compID].stride;
    const ptrdiff_t dstStride  = rpcYuvDst.bufs[compID].stride;

    for (int y = 0; y < height; y++)
    {
      for (int x = 0; x < width; x++)
      {
        pDst[x] = weightBidir(w0, pSrc0[x], w1, pSrc1[x], round, shift, offset, clpRng);
      }

      pSrc0 += src0Stride;
      pSrc1 += src1Stride;
      pDst += dstStride;
    }
  }   // compID loop
}

void WeightPrediction::addWeightUni(const CPelUnitBuf &pcYuvSrc0, const ClpRngs &clpRngs,
                                    const WPScalingParam *const wp0, PelUnitBuf &rpcYuvDst,
                                    const ComponentID maxNumComp, bool lumaOnly, bool chromaOnly)
{
  CHECK( lumaOnly && chromaOnly, "Not allowed to have both lumaOnly and chromaOnly selected" );

  const int numValidComponent = (int) pcYuvSrc0.bufs.size();
  const int firstComponent    = chromaOnly ? 1 : 0;
  const int lastComponent     = lumaOnly ? 0 : maxNumComp;

  for (int componentIndex = firstComponent; componentIndex < numValidComponent && componentIndex <= lastComponent;
       componentIndex++)
  {
    const ComponentID compID = ComponentID(componentIndex);

    const Pel *pSrc0 = pcYuvSrc0.bufs[compID].buf;
    Pel       *pDst  = rpcYuvDst.bufs[compID].buf;

    const ClpRng &clpRng = clpRngs.comp[compID];

    const int w0       = wp0[compID].w;
    const int offset   = wp0[compID].offset;
    const int clipBD   = clpRng.bd;
    const int shiftNum = IF_INTERNAL_FRAC_BITS(clipBD);
    const int shift    = wp0[compID].shift + shiftNum;
    const int height   = rpcYuvDst.bufs[compID].height;
    const int width    = rpcYuvDst.bufs[compID].width;

    const ptrdiff_t src0Stride = pcYuvSrc0.bufs[compID].stride;
    const ptrdiff_t dstStride  = rpcYuvDst.bufs[compID].stride;

    if (w0 != 1 << wp0[compID].shift)
    {
      const int round = 1 << shift >> 1;
      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < width; x++)
        {
          pDst[x] = weightUnidir(w0, pSrc0[x], round, shift, offset, clpRng);
        }

        pSrc0 += src0Stride;
        pDst += dstStride;
      }
    }
    else
    {
      const int round = 1 << shiftNum >> 1;
      if (offset == 0)
      {
        for (int y = 0; y < height; y++)
        {
          for (int x = 0; x < width; x++)
          {
            pDst[x] = noWeightOffsetUnidir(pSrc0[x], round, shiftNum, clpRng);
          }

          pSrc0 += src0Stride;
          pDst += dstStride;
        }
      }
      else
      {
        for (int y = 0; y < height; y++)
        {
          for (int x = 0; x < width; x++)
          {
            pDst[x] = noWeightUnidir(pSrc0[x], round, shiftNum, offset, clpRng);
          }

          pSrc0 += src0Stride;
          pDst += dstStride;
        }
      }
    }
  }
}

void WeightPrediction::xWeightedPredictionUni(const PredictionUnit &pu, const CPelUnitBuf &pcYuvSrc,
                                              const RefPicList &eRefPicList, PelUnitBuf &pcYuvPred,
                                              const int refIdxInput /* = -1*/, const ComponentID maxNumComp,
                                              const bool lumaOnly, const bool chromaOnly)
{
  const int refIdx = refIdxInput < 0 ? pu.refIdx[eRefPicList] : refIdxInput;

  CHECK(refIdx < 0, "Negative reference picture list index");

  WPScalingParam *pwp, *pwpTmp;

  if (eRefPicList == REF_PIC_LIST_0)
  {
    getWpScaling(pu.cs->slice, refIdx, -1, pwp, pwpTmp, maxNumComp);
  }
  else
  {
    getWpScaling(pu.cs->slice, -1, refIdx, pwpTmp, pwp, maxNumComp);
  }

  addWeightUni(pcYuvSrc, pu.cu->slice->clpRngs(), pwp, pcYuvPred, maxNumComp, lumaOnly, chromaOnly);
}

void WeightPrediction::xWeightedPredictionBi(const PredictionUnit &pu, const CPelUnitBuf &pcYuvSrc0,
                                             const CPelUnitBuf &pcYuvSrc1, PelUnitBuf &rpcYuvDst,
                                             const ComponentID maxNumComp, const bool lumaOnly, const bool chromaOnly)
{
  CHECK(!pu.cs->pps->getWPBiPred(), "Weighted Bi-prediction disabled");

  const int refIdx0 = pu.refIdx[0];
  const int refIdx1 = pu.refIdx[1];

  if (refIdx0 < 0 && refIdx1 < 0)
  {
    return;
  }

  WPScalingParam *pwp0;
  WPScalingParam *pwp1;

  getWpScaling(pu.cu->slice, refIdx0, refIdx1, pwp0, pwp1, maxNumComp);

  if (refIdx0 >= 0 && refIdx1 >= 0)
  {
    addWeightBi(pcYuvSrc0, pcYuvSrc1, pu.cu->slice->clpRngs(), pwp0, pwp1, rpcYuvDst, maxNumComp, lumaOnly, chromaOnly);
  }
  else if (refIdx0 >= 0)
  {
    addWeightUni(pcYuvSrc0, pu.cu->slice->clpRngs(), pwp0, rpcYuvDst, maxNumComp, lumaOnly, chromaOnly);
  }
  else
  {
    addWeightUni(pcYuvSrc1, pu.cu->slice->clpRngs(), pwp1, rpcYuvDst, maxNumComp, lumaOnly, chromaOnly);
  }
}
