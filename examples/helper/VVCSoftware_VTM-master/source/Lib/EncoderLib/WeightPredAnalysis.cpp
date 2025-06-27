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

/** \file     WeightPredAnalysis.cpp
    \brief    weighted prediction encoder class
*/

#include "../CommonLib/CommonDef.h"
#include "../CommonLib/Slice.h"
#include "../CommonLib/Picture.h"
#include "WeightPredAnalysis.h"
#include <limits>

static constexpr double WEIGHT_PRED_SAD_RELATIVE_TO_NON_WEIGHT_PRED_SAD=0.99; // NOTE: U0040 used 0.95

//! calculate SAD values for both WP version and non-WP version.
static int64_t xCalcSADvalueWP(const int bitDepth, const Pel *pOrgPel, const Pel *pRefPel, const int width,
                               const int height, const ptrdiff_t orgStride, const ptrdiff_t refStride,
                               const int log2Denom, const int weight, const int offset, const bool useHighPrecision);

//! calculate SAD values for both WP version and non-WP version.
static int64_t xCalcSADvalueWPOptionalClip(const int bitDepth, const Pel *pOrgPel, const Pel *pRefPel, const int width,
                                           const int height, const ptrdiff_t orgStride, const ptrdiff_t refStride,
                                           const int log2Denom, const int weight, const int offset,
                                           const bool useHighPrecision, const bool clipped);

// -----------------------------------------------------------------------------
// Helper functions


//! calculate Histogram for array of pixels
static void xCalcHistogram(const Pel *pPel, std::vector<int> &histogram, const int width, const int height,
                           const ptrdiff_t stride, const int maxPel)
{
  histogram.clear();
  histogram.resize(maxPel);
  for( int y = 0; y < height; y++ )
  {
    for( int x = 0; x < width; x++ )
    {
      const Pel v=pPel[x];
      histogram[v<0?0:(v>=maxPel)?maxPel-1:v]++;
    }
    pPel += stride;
  }
}

static void xScaleHistogram(const std::vector<int> &histogramInput,
                            std::vector<int> &      histogramOutput,   // cannot be the same as the input
                            const int bitDepth, const int log2Denom, const int weight, const int offset,
                            const bool highPrecision)
{
  CHECK(&histogramInput == &histogramOutput, "Input and output histogram are the same");
  const int numElements=int(histogramInput.size());
  histogramOutput.clear();
  histogramOutput.resize(numElements);

  const int64_t realLog2Denom = highPrecision ? 0 : (bitDepth - 8);
  const int64_t realOffset    = ((int64_t) offset) << realLog2Denom;

  const int divOffset = log2Denom == 0 ? 0 : 1 << (log2Denom - 1);
  // Scan histogram and apply illumination parameters appropriately
  // Then compute updated histogram.
  // Note that this technique only works with single list weights/offsets.

  for (int i = 0; i < numElements; i++)
  {
    const int j = Clip3(0, numElements - 1, (int) (((weight * i + divOffset) >> log2Denom) + realOffset));
    histogramOutput[j] += histogramInput[i];
  }
}

static
Distortion xCalcHistCumulDistortion(const std::vector<int>& histogram0,
  const std::vector<int>& histogram1)
{
  Distortion distortion = 0;
  CHECK(histogram0.size() != histogram1.size(), "Different histogram sizes");
  const int numElements = int(histogram0.size());

  int64_t  cumul = 0;

  // Scan histograms to compute histogram distortion
  for (int i = 0; i < numElements; i++)
  {
    cumul += (int64_t)histogram0[i] - (int64_t)histogram1[i];
    distortion += (Distortion)(abs(cumul));
  }

  return distortion;
}

static Distortion xSearchHistogram(const std::vector<int> &histogramSource, const std::vector<int> &histogramRef,
                                   std::vector<int> &outputHistogram, const int bitDepth, const int log2Denom,
                                   int &weightToUpdate, int &offsetToUpdate, const bool highPrecision,
                                   const ComponentID compID)
{
  const int initialWeight   = weightToUpdate;
  const int initialOffset   = offsetToUpdate;
  const int weightRange     = 10;
  const int offsetRange     = 10;
  const int maxOffset       = 1 << ((highPrecision == true) ? (bitDepth - 1) : 7);
  const int range           = highPrecision ? (1 << bitDepth) / 2 : 128;
  const int defaultWeight   = (1<<log2Denom);
  const int minSearchWeight = std::max<int>(initialWeight - weightRange, defaultWeight - range);
  const int maxSearchWeight = std::min<int>(initialWeight + weightRange+1, defaultWeight + range);

  Distortion minDistortion   = std::numeric_limits<Distortion>::max();
  int        bestWeight      = initialWeight;
  int        bestOffset      = initialOffset;

  for (int searchWeight = minSearchWeight; searchWeight < maxSearchWeight; searchWeight++)
  {
    if (compID == COMPONENT_Y)
    {
      for (int searchOffset = std::max<int>(initialOffset - offsetRange, -maxOffset);
               searchOffset <= initialOffset + offsetRange && searchOffset<=(maxOffset-1);
               searchOffset++)
      {
        xScaleHistogram(histogramRef, outputHistogram, bitDepth, log2Denom, searchWeight, searchOffset, highPrecision);
        const Distortion distortion = xCalcHistCumulDistortion(histogramSource, outputHistogram);

        if (distortion < minDistortion)
        {
          minDistortion = distortion;
          bestWeight    = searchWeight;
          bestOffset    = searchOffset;
        }
      }
    }
    else
    {
      const int pred        = ( maxOffset - ( ( maxOffset*searchWeight)>>(log2Denom) ) );

      for (int searchOffset = initialOffset - offsetRange; searchOffset <= initialOffset + offsetRange; searchOffset++)
      {
        const int deltaOffset =
          Clip3(-4 * maxOffset, 4 * maxOffset - 1, (searchOffset - pred));   // signed 10bit (if !highPrecision)
        const int clippedOffset =
          Clip3(-1 * maxOffset, 1 * maxOffset - 1, (deltaOffset + pred));   // signed 8bit  (if !highPrecision)
        xScaleHistogram(histogramRef, outputHistogram, bitDepth, log2Denom, searchWeight, clippedOffset, highPrecision);
        const Distortion distortion = xCalcHistCumulDistortion(histogramSource, outputHistogram);

        if (distortion < minDistortion)
        {
          minDistortion = distortion;
          bestWeight    = searchWeight;
          bestOffset    = clippedOffset;
        }
      }
    }
  }

  weightToUpdate = bestWeight;
  offsetToUpdate = bestOffset;

  // regenerate best histogram
  xScaleHistogram(histogramRef, outputHistogram, bitDepth, log2Denom, bestWeight, bestOffset, highPrecision);

  return minDistortion;
}


// -----------------------------------------------------------------------------
// Member functions

WeightPredAnalysis::WeightPredAnalysis()
{
  for ( uint32_t lst =0 ; lst<NUM_REF_PIC_LIST_01 ; lst++ )
  {
    for ( int refIdx=0 ; refIdx<MAX_NUM_REF ; refIdx++ )
    {
      for ( int comp=0 ; comp<MAX_NUM_COMPONENT ;comp++ )
      {
        WPScalingParam  *pwp   = &(m_wp[lst][refIdx][comp]);
        pwp->presentFlag       = false;
        pwp->log2WeightDenom   = 0;
        pwp->codedWeight       = 1;
        pwp->codedOffset       = 0;
      }
    }
  }
}


//! calculate AC and DC values for current original image
void WeightPredAnalysis::xCalcACDCParamSlice(Slice *const slice)
{
  //===== calculate AC/DC value =====
//  PicYuv*   pPic = slice->getPic()->getPicYuvOrg();
  const CPelUnitBuf pPic = slice->getPic()->getOrigBuf();

  WPACDCParam weightACDCParam[MAX_NUM_COMPONENT];

  for(int componentIndex = 0; componentIndex < ::getNumberValidComponents(pPic.chromaFormat); componentIndex++)
  {
    const ComponentID compID = ComponentID(componentIndex);

    const CPelBuf compBuf = pPic.get( compID );

    // calculate DC/AC value for channel

    const ptrdiff_t stride = compBuf.stride;
    const int width  = compBuf.width;
    const int height = compBuf.height;

    const int sample = width*height;

    int64_t orgDC = 0;
    {
      const Pel *pPel = compBuf.buf;

      for(int y = 0; y < height; y++, pPel+=stride )
      {
        for(int x = 0; x < width; x++ )
        {
          orgDC += (int)( pPel[x] );
        }
      }
    }

    const int64_t orgNormDC = ((orgDC+(sample>>1)) / sample);

    int64_t orgAC = 0;
    {
      const Pel *pPel = compBuf.buf;

      for(int y = 0; y < height; y++, pPel += stride )
      {
        for(int x = 0; x < width; x++ )
        {
          orgAC += abs( (int)pPel[x] - (int)orgNormDC );
        }
      }
    }

    const int fixedBitShift = (slice->getSPS()->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag())?RExt__PREDICTION_WEIGHTING_ANALYSIS_DC_PRECISION:0;
    weightACDCParam[compID].dc = (((orgDC << fixedBitShift) + (sample >> 1)) / sample);
    weightACDCParam[compID].ac = orgAC;
  }

  slice->setWpAcDcParam(weightACDCParam);
}


//! check weighted pred or non-weighted pred
void  WeightPredAnalysis::xCheckWPEnable(Slice *const slice)
{
//  const PicYuv *pPic = slice->getPic()->getPicYuvOrg();

  int presentCnt = 0;
  for ( uint32_t lst=0 ; lst<NUM_REF_PIC_LIST_01 ; lst++ )
  {
    for ( int refIdx=0 ; refIdx<MAX_NUM_REF ; refIdx++ )
    {
      for(int componentIndex = 0; componentIndex < ::getNumberValidComponents( slice->getSPS()->getChromaFormatIdc() ); componentIndex++)
      {
        WPScalingParam  *pwp = &(m_wp[lst][refIdx][componentIndex]);
        presentCnt += (int) pwp->presentFlag;
      }
    }
  }

  if(presentCnt==0)
  {
    slice->setTestWeightPred(false);
    slice->setTestWeightBiPred(false);

    for ( uint32_t lst=0 ; lst<NUM_REF_PIC_LIST_01 ; lst++ )
    {
      for ( int refIdx=0 ; refIdx<MAX_NUM_REF ; refIdx++ )
      {
        for(int componentIndex = 0; componentIndex < ::getNumberValidComponents( slice->getSPS()->getChromaFormatIdc() ); componentIndex++)
        {
          WPScalingParam  *pwp = &(m_wp[lst][refIdx][componentIndex]);

          pwp->presentFlag     = false;
          pwp->log2WeightDenom = 0;
          pwp->codedWeight     = 1;
          pwp->codedOffset     = 0;
        }
      }
    }
    slice->setWpScaling( m_wp );
  }
  else
  {
    slice->setTestWeightPred  (slice->getPPS()->getUseWP());
    slice->setTestWeightBiPred(slice->getPPS()->getWPBiPred());
  }
}


//! estimate wp tables for explicit wp
void WeightPredAnalysis::xEstimateWPParamSlice(Slice *const slice, const WeightedPredictionMethod method)
{
  int  denom          = 6;
  bool validRangeFlag = false;

  if(slice->getNumRefIdx(REF_PIC_LIST_0)>3)
  {
    denom = 7;
  }

  do
  {
    validRangeFlag = xUpdatingWPParameters(slice, denom);
    if (!validRangeFlag)
    {
      denom--;   // decrement to satisfy the range limitation
    }
  } while (validRangeFlag == false);

  // selecting whether WP is used, or not (fast search)
  // NOTE: This is not operating on a slice, but the entire picture.
  // clang-format off
  switch (method)
  {
  case WP_PER_PICTURE_WITH_SIMPLE_DC_COMBINED_COMPONENT:
    xSelectWP(slice, denom);
    break;
  case WP_PER_PICTURE_WITH_SIMPLE_DC_PER_COMPONENT:
    xSelectWPHistExtClip(slice, denom, false, false, false);
    break;
  case WP_PER_PICTURE_WITH_HISTOGRAM_AND_PER_COMPONENT:
    xSelectWPHistExtClip(slice, denom, false, false, true);
    break;
  case WP_PER_PICTURE_WITH_HISTOGRAM_AND_PER_COMPONENT_AND_CLIPPING:
    xSelectWPHistExtClip(slice, denom, false, true, true);
    break;
  case WP_PER_PICTURE_WITH_HISTOGRAM_AND_PER_COMPONENT_AND_CLIPPING_AND_EXTENSION:
    xSelectWPHistExtClip(slice, denom, true, true, true);
    break;
  default:
    THROW("Invalid WP method");
    break;
  }
  // clang-format on

  slice->setWpScaling( m_wp );
}


//! update wp tables for explicit wp w.r.t range limitation
bool WeightPredAnalysis::xUpdatingWPParameters(Slice *const slice, const int log2Denom)
{
  const int  numComp                   = ::getNumberValidComponents(slice->getSPS()->getChromaFormatIdc());
  const bool useHighPrecisionWeighting = slice->getSPS()->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag();
  const int  numPredDir                = slice->isInterP() ? 1 : 2;

  CHECK(numPredDir > int(NUM_REF_PIC_LIST_01), "Invalid reference picture list");

  for ( int refList = 0; refList < numPredDir; refList++ )
  {
    const RefPicList eRefPicList = ( refList ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );

    for ( int refIdxTemp = 0; refIdxTemp < slice->getNumRefIdx(eRefPicList); refIdxTemp++ )
    {
      const WPACDCParam *currWeightACDCParam, *refWeightACDCParam;
      slice->getWpAcDcParam(currWeightACDCParam);
      slice->getRefPic(eRefPicList, refIdxTemp)->slices[0]->getWpAcDcParam(refWeightACDCParam);

      for ( int comp = 0; comp < numComp; comp++ )
      {
        const ComponentID compID        = ComponentID(comp);
        const int         bitDepth      = slice->getSPS()->getBitDepth(toChannelType(compID));
        const int         range         = useHighPrecisionWeighting ? (1 << bitDepth) / 2 : 128;
        const int         realLog2Denom =
          log2Denom + (useHighPrecisionWeighting ? RExt__PREDICTION_WEIGHTING_ANALYSIS_DC_PRECISION : (bitDepth - 8));
        const int         realOffset    = ((int)1<<(realLog2Denom-1));

        // current frame
        const int64_t currDC = currWeightACDCParam[comp].dc;
        const int64_t currAC = currWeightACDCParam[comp].ac;
        // reference frame
        const int64_t refDC = refWeightACDCParam[comp].dc;
        const int64_t refAC = refWeightACDCParam[comp].ac;

        // calculating codedWeight and codedOffset params
        const double weightUnquantized =
          (refAC == 0) ? (double) 1.0 : Clip3(-16.0, 15.0, ((double) currAC / (double) refAC));
        const int weight     = (int) (0.5 + weightUnquantized * (double) (1 << log2Denom));
        const int offset     = (int)( ((currDC<<log2Denom) - ((int64_t)weight * refDC) + (int64_t)realOffset) >> realLog2Denom );

        int clippedOffset;
        if(isChroma(compID)) // Chroma offset range limination
        {
          const int pred        = ( range - ( ( range*weight)>>(log2Denom) ) );
          const int deltaOffset = Clip3( -4*range, 4*range-1, (offset - pred) ); // signed 10bit

          clippedOffset = Clip3( -range, range-1, (deltaOffset + pred) );  // signed 8bit
        }
        else // Luma offset range limitation
        {
          clippedOffset = Clip3( -range, range-1, offset);
        }

        // Weighting factor limitation
        const int defaultWeight = (1<<log2Denom);
        const int deltaWeight   = (weight - defaultWeight);

        if(deltaWeight >= range || deltaWeight < -range)
        {
          return false;
        }

        m_wp[refList][refIdxTemp][comp].presentFlag     = true;
        m_wp[refList][refIdxTemp][comp].codedWeight     = weight;
        m_wp[refList][refIdxTemp][comp].codedOffset     = clippedOffset;
        m_wp[refList][refIdxTemp][comp].log2WeightDenom = log2Denom;
      }
    }
  }
  return true;
}

bool WeightPredAnalysis::xSelectWPHistExtClip(Slice *const slice, const int log2Denom, const bool doEnhancement,
                                              const bool clipInitialSADWP, const bool useHistogram)
{

  const CPelUnitBuf       pPic             = slice->getPic()->getOrigBuf();
  const int               defaultWeight    = 1<<log2Denom;
  const int               numPredDir       = slice->isInterP() ? 1 : 2;
  const bool              useHighPrecision = slice->getSPS()->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag();

  CHECK(numPredDir > int(NUM_REF_PIC_LIST_01), "Invalid reference picture list");

  for ( int refList = 0; refList < numPredDir; refList++ )
  {
    const RefPicList eRefPicList = ( refList ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );

    for ( int refIdxTemp = 0; refIdxTemp < slice->getNumRefIdx(eRefPicList); refIdxTemp++ )
    {
      bool  useChromaWeight = false;

      for (int comp = 0; comp < ::getNumberValidComponents(pPic.chromaFormat); comp++)
      {
        const ComponentID  compID     = ComponentID(comp);
        const Pel         *pRef       = slice->getRefPic(eRefPicList, refIdxTemp)->getRecoBuf().get(compID).buf;
        const ptrdiff_t    refStride  = slice->getRefPic(eRefPicList, refIdxTemp)->getRecoBuf().get(compID).stride;
        ;
        const CPelBuf      compBuf    = pPic.get( compID );
        const Pel         *pOrg       = compBuf.buf;
        const ptrdiff_t    orgStride  = compBuf.stride;
        const int          width      = compBuf.width;
        const int          height     = compBuf.height;
        const int          bitDepth   = slice->getSPS()->getBitDepth(toChannelType(compID));

        WPScalingParam &wp = m_wp[refList][refIdxTemp][compID];

        int weight    = wp.codedWeight;
        int offset    = wp.codedOffset;
        int weightDef = defaultWeight;
        int offsetDef = 0;

        // calculate SAD costs with/without wp for luma
        std::vector<int> histogramOrg;
        std::vector<int> histogramRef;
        Distortion       sadNoWP = std::numeric_limits<Distortion>::max();
        if (useHistogram && compID == COMPONENT_Y)
        {
          xCalcHistogram(pOrg, histogramOrg, width, height, orgStride, 1 << bitDepth);
          xCalcHistogram(pRef, histogramRef, width, height, refStride, 1 << bitDepth);

          std::vector<int> histogramRef_noWP;
          xScaleHistogram(histogramRef, histogramRef_noWP, bitDepth, log2Denom, defaultWeight, 0, useHighPrecision);
          sadNoWP = xCalcHistCumulDistortion(histogramOrg, histogramRef_noWP);
        }
        else
        {
          sadNoWP = xCalcSADvalueWPOptionalClip(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom,
                                                defaultWeight, 0, useHighPrecision, clipInitialSADWP);
        }
        if (sadNoWP > 0)
        {
          Distortion sadWP = std::numeric_limits<Distortion>::max();
          if (useHistogram && compID == COMPONENT_Y)
          {
            std::vector<int> histogramRef_WP;
            xScaleHistogram(histogramRef, histogramRef_WP, bitDepth, log2Denom, weight, offset, useHighPrecision);
            sadWP = xCalcHistCumulDistortion(histogramOrg, histogramRef_WP);
          }
          else
          {
            sadWP = xCalcSADvalueWPOptionalClip(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom,
                                                weight, offset, useHighPrecision, clipInitialSADWP);
          }
          const double dRatioSAD    = (double) sadWP / (double) sadNoWP;
          double dRatioSr0SAD = std::numeric_limits<double>::max();
          double dRatioSrSAD  = std::numeric_limits<double>::max();

          if (useHistogram)
          {
            std::vector<int> searchedHistogram;


            // Do a histogram search around DC WP parameters; resulting distortion and 'searchedHistogram' is discarded
            xSearchHistogram(histogramOrg, histogramRef, searchedHistogram, bitDepth, log2Denom, weight, offset, useHighPrecision, compID);
            // calculate updated WP SAD
            uint64_t SADSrWP = std::numeric_limits<uint64_t>::max();
            if (useHistogram && compID == COMPONENT_Y)
            {
              std::vector<int> histogramRef_SrWP;
              xScaleHistogram(histogramRef, histogramRef_SrWP, bitDepth, log2Denom, weight, offset, useHighPrecision);
              SADSrWP = (uint64_t)xCalcHistCumulDistortion(histogramOrg, histogramRef_SrWP);
            }
            else
            {
              SADSrWP = (uint64_t)xCalcSADvalueWP(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom, weight, offset, useHighPrecision);
            }
            dRatioSrSAD = (double) SADSrWP / (double) sadNoWP;

            if (doEnhancement)
            {
              // Do the same around the default ones; resulting distortion and 'searchedHistogram' is discarded
              xSearchHistogram(histogramOrg, histogramRef, searchedHistogram, bitDepth, log2Denom, weightDef, offsetDef, useHighPrecision, compID);
              // calculate updated WP SAD
              uint64_t SADSr0WP = std::numeric_limits<uint64_t>::max();
              if (useHistogram && compID == COMPONENT_Y)
              {
                std::vector<int> histogramRef_SrWP;
                xScaleHistogram(histogramRef, histogramRef_SrWP, bitDepth, log2Denom, weightDef, offsetDef, useHighPrecision);
                SADSr0WP = (uint64_t)xCalcHistCumulDistortion(histogramOrg, histogramRef_SrWP);
              }
              else
              {
                SADSr0WP = (uint64_t)xCalcSADvalueWP(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom, weightDef, offsetDef, useHighPrecision);
              }
              dRatioSr0SAD = (double) SADSr0WP / (double) sadNoWP;
            }
          }

          if(std::min(dRatioSr0SAD, std::min(dRatioSAD, dRatioSrSAD)) >= WEIGHT_PRED_SAD_RELATIVE_TO_NON_WEIGHT_PRED_SAD)
          {
            wp.presentFlag     = false;
            wp.codedOffset     = 0;
            wp.codedWeight     = defaultWeight;
            wp.log2WeightDenom = log2Denom;
          }
          else
          {
            if (compID != COMPONENT_Y)
            {
              useChromaWeight = true;
            }

            if (dRatioSr0SAD < dRatioSrSAD && dRatioSr0SAD < dRatioSAD)
            {
              wp.presentFlag     = true;
              wp.codedOffset     = offsetDef;
              wp.codedWeight     = weightDef;
              wp.log2WeightDenom = log2Denom;
            }
            else if (dRatioSrSAD < dRatioSAD)
            {
              wp.presentFlag     = true;
              wp.codedOffset     = offset;
              wp.codedWeight     = weight;
              wp.log2WeightDenom = log2Denom;
            }
          }
        }
        else   // (sadNoWP <= 0)
        {
          wp.presentFlag     = false;
          wp.codedOffset     = 0;
          wp.codedWeight     = defaultWeight;
          wp.log2WeightDenom = log2Denom;
        }
      }

      for (int comp = 1; comp < ::getNumberValidComponents(pPic.chromaFormat); comp++)
      {
        m_wp[refList][refIdxTemp][comp].presentFlag = useChromaWeight;
      }
    }
  }

  return true;
}

//! select whether weighted pred enables or not.
bool WeightPredAnalysis::xSelectWP(Slice *const slice, const int log2Denom)
{
  const CPelUnitBuf       pPic                                = slice->getPic()->getOrigBuf();
  const int               defaultWeight                       = 1<<log2Denom;
  const int               numPredDir                          = slice->isInterP() ? 1 : 2;
  const bool              useHighPrecisionPredictionWeighting = slice->getSPS()->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag();

  CHECK(numPredDir > int(NUM_REF_PIC_LIST_01), "Invalid reference picture list");

  for ( int refList = 0; refList < numPredDir; refList++ )
  {
    const RefPicList eRefPicList = ( refList ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );

    for ( int refIdxTemp = 0; refIdxTemp < slice->getNumRefIdx(eRefPicList); refIdxTemp++ )
    {
      int64_t sadWP = 0, sadNoWP = 0;

      for (int comp = 0; comp < ::getNumberValidComponents(pPic.chromaFormat); comp++)
      {
        const ComponentID  compID     = ComponentID(comp);
        const CPelBuf      compBuf    = pPic.get( compID );
        const Pel         *pRef       = slice->getRefPic(eRefPicList, refIdxTemp)->getRecoBuf().get( compID ).buf;
        const ptrdiff_t    refStride  = slice->getRefPic(eRefPicList, refIdxTemp)->getRecoBuf().get(compID).stride;
        const Pel         *pOrg       = compBuf.buf;
        const ptrdiff_t    orgStride  = compBuf.stride;
        const int          width      = compBuf.width;
        const int          height     = compBuf.height;
        const int          bitDepth   = slice->getSPS()->getBitDepth(toChannelType(compID));

        // calculate SAD costs with/without wp for luma
        sadWP += xCalcSADvalueWP(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom,
                                 m_wp[refList][refIdxTemp][compID].codedWeight,
                                 m_wp[refList][refIdxTemp][compID].codedOffset, useHighPrecisionPredictionWeighting);
        sadNoWP += xCalcSADvalueWP(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom, defaultWeight,
                                   0, useHighPrecisionPredictionWeighting);
      }

      const double dRatio = sadNoWP > 0 ? (((double) sadWP / (double) sadNoWP)) : std::numeric_limits<double>::max();
      const double dMaxRatio  = double( 0.99 );
      if(dRatio >= dMaxRatio)
      {
        for(int comp=0; comp < ::getNumberValidComponents(pPic.chromaFormat); comp++)
        {
          WPScalingParam &wp=m_wp[refList][refIdxTemp][comp];
          wp.presentFlag     = false;
          wp.codedOffset     = 0;
          wp.codedWeight     = defaultWeight;
          wp.log2WeightDenom = log2Denom;
        }
      }
    }
  }

  return true;
}

// Alternatively, a SSE-based measure could be used instead.
// The respective function has been removed as it currently redundant.
static int64_t xCalcSADvalueWP(const int bitDepth, const Pel *pOrgPel, const Pel *pRefPel, const int width,
                               const int height, const ptrdiff_t orgStride, const ptrdiff_t refStride,
                               const int log2Denom, const int weight, const int offset, const bool useHighPrecision)
{
  const int64_t realLog2Denom = useHighPrecision ? log2Denom : (log2Denom + (bitDepth - 8));
  const int64_t realOffset    = ((int64_t)offset)<<realLog2Denom;

  int64_t SAD = 0;
  for( int y = 0; y < height; y++ )
  {
    for( int x = 0; x < width; x++ )
    {
      SAD += abs(( ((int64_t)pOrgPel[x] << (int64_t) log2Denom) - ( (int64_t) pRefPel[x] * (int64_t) weight + (realOffset) ) ) );
    }
    pOrgPel += orgStride;
    pRefPel += refStride;
  }

  return SAD;
}

static int64_t xCalcSADvalueWPOptionalClip(const int bitDepth, const Pel *pOrgPel, const Pel *pRefPel, const int width,
                                           const int height, const ptrdiff_t orgStride, const ptrdiff_t refStride,
                                           const int log2Denom, const int weight, const int offset,
                                           const bool useHighPrecision, const bool clipped)
{
  int64_t sad = 0;
  if (clipped)
  {
    const int64_t realLog2Denom = useHighPrecision ? 0 : (bitDepth - 8);
    const int64_t realOffset    = (int64_t)offset<<realLog2Denom;
    const int64_t roundOffset = (log2Denom == 0) ? 0 : 1 << (log2Denom - 1);
    const int64_t minValue = 0;
    const int64_t maxValue = (1 << bitDepth) - 1;

    for( int y = 0; y < height; y++ )
    {
      for( int x = 0; x < width; x++ )
      {
        int64_t scaledValue = Clip3(minValue, maxValue,  ((((int64_t) pRefPel[x] * (int64_t) weight + roundOffset) ) >>  (int64_t) log2Denom) + realOffset); //ClipPel
        sad += abs((int64_t) pOrgPel[x] - scaledValue);
      }
      pOrgPel += orgStride;
      pRefPel += refStride;
    }
  }
  else
  {
    const int64_t realLog2Denom = useHighPrecision ? log2Denom : (log2Denom + (bitDepth - 8));
    const int64_t realOffset    = ((int64_t)offset)<<realLog2Denom;

    for( int y = 0; y < height; y++ )
    {
      for( int x = 0; x < width; x++ )
      {
        sad += abs(
          (((int64_t) pOrgPel[x] << (int64_t) log2Denom) - ((int64_t) pRefPel[x] * (int64_t) weight + (realOffset))));
      }
      pOrgPel += orgStride;
      pRefPel += refStride;
    }
  }
  return sad;
}
