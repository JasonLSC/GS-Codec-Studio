/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2022, ITU/ISO/IEC
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

#include "../TLibCommon/CommonDef.h"
#include "../TLibCommon/TComSlice.h"
#include "../TLibCommon/TComPic.h"
#include "../TLibCommon/TComPicYuv.h"
#include "WeightPredAnalysis.h"
#include <limits>

static const Double WEIGHT_PRED_SAD_RELATIVE_TO_NON_WEIGHT_PRED_SAD=0.99; // NOTE: U0040 used 0.95

//! calculate SAD values for both WP version and non-WP version.
static
Int64 xCalcSADvalueWP(const Int   bitDepth,
                      const Pel  *pOrgPel,
                      const Pel  *pRefPel,
                      const Int   width,
                      const Int   height,
                      const Int   orgStride,
                      const Int   refStride,
                      const Int   log2Denom,
                      const Int   weight,
                      const Int   offset,
                      const Bool  useHighPrecision);

//! calculate SAD values for both WP version and non-WP version.
static
Int64 xCalcSADvalueWPOptionalClip(const Int   bitDepth,
                                  const Pel  *pOrgPel,
                                  const Pel  *pRefPel,
                                  const Int   width,
                                  const Int   height,
                                  const Int   orgStride,
                                  const Int   refStride,
                                  const Int   log2Denom,
                                  const Int   weight,
                                  const Int   offset,
                                  const Bool  useHighPrecision,
                                  const Bool  clipped);

// -----------------------------------------------------------------------------
// Helper functions


//! calculate Histogram for array of pixels
static
Void xCalcHistogram(const Pel  *pPel,
                    std::vector<Int> &histogram,
                    const Int   width,
                    const Int   height,
                    const Int   stride,
                    const Int   maxPel)
{
  histogram.clear();
  histogram.resize(maxPel);
  for( Int y = 0; y < height; y++ )
  {
    for( Int x = 0; x < width; x++ )
    {
      const Pel v=pPel[x];
      histogram[v<0?0:(v>=maxPel)?maxPel-1:v]++;
    }
    pPel += stride;
  }
}

static
Distortion xCalcHistDistortion (const std::vector<Int> &histogram0,
                                const std::vector<Int> &histogram1)
{
  Distortion distortion = 0;
  assert(histogram0.size()==histogram1.size());
  const Int numElements=Int(histogram0.size());

  // Scan histograms to compute histogram distortion
  for (Int i = 0; i <= numElements; i++)
  {
    distortion += (Distortion)(abs(histogram0[i] - histogram1[i]));
  }

  return distortion;
}

static
void xScaleHistogram(const std::vector<Int> &histogramInput,
                           std::vector<Int> &histogramOutput, // cannot be the same as the input
                     const Int               bitDepth,
                     const Int               log2Denom,
                     const Int               weight,
                     const Int               offset,
                     const Bool              bHighPrecision)
{
  assert(&histogramInput != &histogramOutput);
  const Int numElements=Int(histogramInput.size());
  histogramOutput.clear();
  histogramOutput.resize(numElements);

  const Int64 iRealLog2Denom = bHighPrecision ? 0 : (bitDepth - 8);
  const Int64 iRealOffset    = ((Int64)offset)<<iRealLog2Denom;

  const Int divOffset = log2Denom == 0 ? 0 : 1 << (log2Denom - 1);
  // Scan histogram and apply illumination parameters appropriately
  // Then compute updated histogram.
  // Note that this technique only works with single list weights/offsets.

  for (Int i = 0; i < numElements; i++)
  {
    const Int j = Clip3(0, numElements - 1, (Int)(((weight * i + divOffset) >> log2Denom) + iRealOffset));
    histogramOutput[j] += histogramInput[i];
  }
}

static
Distortion xSearchHistogram(const std::vector<Int> &histogramSource,
                            const std::vector<Int> &histogramRef,
                                  std::vector<Int> &outputHistogram,
                            const Int               bitDepth,
                            const Int               log2Denom,
                                  Int              &weightToUpdate,
                                  Int              &offsetToUpdate,
                            const Bool              bHighPrecision,
                            const ComponentID       compID)
{
  const Int initialWeight   = weightToUpdate;
  const Int initialOffset   = offsetToUpdate;
  const Int weightRange     = 10;
  const Int offsetRange     = 10;
  const Int maxOffset       = 1 << ((bHighPrecision == true) ? (bitDepth - 1) : 7);
  const Int range           = bHighPrecision ? (1<<bitDepth) / 2 : 128;
  const Int defaultWeight   = (1<<log2Denom);
  const Int minSearchWeight = std::max<Int>(initialWeight - weightRange, defaultWeight - range);
  const Int maxSearchWeight = std::min<Int>(initialWeight + weightRange+1, defaultWeight + range);

  Distortion minDistortion   = std::numeric_limits<Distortion>::max();
  Int        bestWeight      = initialWeight;
  Int        bestOffset      = initialOffset;

  for (Int searchWeight = minSearchWeight; searchWeight < maxSearchWeight; searchWeight++)
  {
    if (compID == COMPONENT_Y)
    {
      for (Int searchOffset = std::max<Int>(initialOffset - offsetRange, -maxOffset);
               searchOffset <= initialOffset + offsetRange && searchOffset<=(maxOffset-1);
               searchOffset++)
      {
        xScaleHistogram(histogramRef, outputHistogram, bitDepth, log2Denom, searchWeight, searchOffset, bHighPrecision);
        const Distortion distortion = xCalcHistDistortion(histogramSource, outputHistogram);

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
      const Int pred        = ( maxOffset - ( ( maxOffset*searchWeight)>>(log2Denom) ) );

      for (Int searchOffset = initialOffset - offsetRange; searchOffset <= initialOffset + offsetRange; searchOffset++)
      {
        const Int deltaOffset   = Clip3( -4*maxOffset, 4*maxOffset-1, (searchOffset - pred) ); // signed 10bit (if !bHighPrecision)
        const Int clippedOffset = Clip3( -1*maxOffset, 1*maxOffset-1, (deltaOffset  + pred) ); // signed 8bit  (if !bHighPrecision)
        xScaleHistogram(histogramRef, outputHistogram, bitDepth, log2Denom, searchWeight, clippedOffset, bHighPrecision);
        const Distortion distortion = xCalcHistDistortion(histogramSource, outputHistogram);

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
  xScaleHistogram(histogramRef, outputHistogram, bitDepth, log2Denom, bestWeight, bestOffset, bHighPrecision);

  return minDistortion;
}


// -----------------------------------------------------------------------------
// Member functions

WeightPredAnalysis::WeightPredAnalysis()
{
  for ( UInt lst =0 ; lst<NUM_REF_PIC_LIST_01 ; lst++ )
  {
    for ( Int refIdx=0 ; refIdx<MAX_NUM_REF ; refIdx++ )
    {
      for ( Int comp=0 ; comp<MAX_NUM_COMPONENT ;comp++ )
      {
        WPScalingParam  *pwp   = &(m_wp[lst][refIdx][comp]);
        pwp->bPresentFlag      = false;
        pwp->uiLog2WeightDenom = 0;
        pwp->iWeight           = 1;
        pwp->iOffset           = 0;
      }
    }
  }
}


//! calculate AC and DC values for current original image
Void WeightPredAnalysis::xCalcACDCParamSlice(TComSlice *const slice)
{
  //===== calculate AC/DC value =====
  TComPicYuv*   pPic = slice->getPic()->getPicYuvOrg();

  WPACDCParam weightACDCParam[MAX_NUM_COMPONENT];

  for(Int componentIndex = 0; componentIndex < pPic->getNumberValidComponents(); componentIndex++)
  {
    const ComponentID compID = ComponentID(componentIndex);

    // calculate DC/AC value for channel

    const Int stride = pPic->getStride(compID);
    const Int width  = pPic->getWidth(compID);
    const Int height = pPic->getHeight(compID);

    const Int sample = width*height;

    Int64 orgDC = 0;
    {
      const Pel *pPel = pPic->getAddr(compID);

      for(Int y = 0; y < height; y++, pPel+=stride )
      {
        for(Int x = 0; x < width; x++ )
        {
          orgDC += (Int)( pPel[x] );
        }
      }
    }

    const Int64 orgNormDC = ((orgDC+(sample>>1)) / sample);

    Int64 orgAC = 0;
    {
      const Pel *pPel = pPic->getAddr(compID);

      for(Int y = 0; y < height; y++, pPel += stride )
      {
        for(Int x = 0; x < width; x++ )
        {
          orgAC += abs( (Int)pPel[x] - (Int)orgNormDC );
        }
      }
    }

    const Int fixedBitShift = (slice->getSPS()->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag())?RExt__PREDICTION_WEIGHTING_ANALYSIS_DC_PRECISION:0;
    weightACDCParam[compID].iDC = (((orgDC<<fixedBitShift)+(sample>>1)) / sample);
    weightACDCParam[compID].iAC = orgAC;
  }

  slice->setWpAcDcParam(weightACDCParam);
}


//! check weighted pred or non-weighted pred
Void  WeightPredAnalysis::xCheckWPEnable(TComSlice *const slice)
{
  const TComPicYuv *pPic = slice->getPic()->getPicYuvOrg();

  Int presentCnt = 0;
  for ( UInt lst=0 ; lst<NUM_REF_PIC_LIST_01 ; lst++ )
  {
    for ( Int refIdx=0 ; refIdx<MAX_NUM_REF ; refIdx++ )
    {
      for(Int componentIndex = 0; componentIndex < pPic->getNumberValidComponents(); componentIndex++)
      {
        WPScalingParam  *pwp = &(m_wp[lst][refIdx][componentIndex]);
        presentCnt += (Int)pwp->bPresentFlag;
      }
    }
  }

  if(presentCnt==0)
  {
    slice->setTestWeightPred(false);
    slice->setTestWeightBiPred(false);

    for ( UInt lst=0 ; lst<NUM_REF_PIC_LIST_01 ; lst++ )
    {
      for ( Int refIdx=0 ; refIdx<MAX_NUM_REF ; refIdx++ )
      {
        for(Int componentIndex = 0; componentIndex < pPic->getNumberValidComponents(); componentIndex++)
        {
          WPScalingParam  *pwp = &(m_wp[lst][refIdx][componentIndex]);

          pwp->bPresentFlag      = false;
          pwp->uiLog2WeightDenom = 0;
          pwp->iWeight           = 1;
          pwp->iOffset           = 0;
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
Void WeightPredAnalysis::xEstimateWPParamSlice(TComSlice *const slice, const WeightedPredictionMethod method)
{
  Int  iDenom         = 6;
  Bool validRangeFlag = false;

  if(slice->getNumRefIdx(REF_PIC_LIST_0)>3)
  {
    iDenom = 7;
  }

  do
  {
    validRangeFlag = xUpdatingWPParameters(slice, iDenom);
    if (!validRangeFlag)
    {
      iDenom--; // decrement to satisfy the range limitation
    }
  } while (validRangeFlag == false);

  // selecting whether WP is used, or not (fast search)
  // NOTE: This is not operating on a slice, but the entire picture.
  switch (method)
  {
    case WP_PER_PICTURE_WITH_SIMPLE_DC_COMBINED_COMPONENT:
      xSelectWP(slice, iDenom);
      break;
    case WP_PER_PICTURE_WITH_SIMPLE_DC_PER_COMPONENT:
      xSelectWPHistExtClip(slice, iDenom, false, false, false);
      break;
    case WP_PER_PICTURE_WITH_HISTOGRAM_AND_PER_COMPONENT:
      xSelectWPHistExtClip(slice, iDenom, false, false, true);
      break;
    case WP_PER_PICTURE_WITH_HISTOGRAM_AND_PER_COMPONENT_AND_CLIPPING:
      xSelectWPHistExtClip(slice, iDenom, false, true, true);
      break;
    case WP_PER_PICTURE_WITH_HISTOGRAM_AND_PER_COMPONENT_AND_CLIPPING_AND_EXTENSION:
      xSelectWPHistExtClip(slice, iDenom, true, true, true);
      break;
    default:
      assert(0);
      exit(1);
  }

  slice->setWpScaling( m_wp );
}


//! update wp tables for explicit wp w.r.t range limitation
Bool WeightPredAnalysis::xUpdatingWPParameters(TComSlice *const slice, const Int log2Denom)
{
  const Int  numComp                    = slice->getPic()->getPicYuvOrg()->getNumberValidComponents();
  const Bool bUseHighPrecisionWeighting = slice->getSPS()->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag();
  const Int numPredDir                  = slice->isInterP() ? 1 : 2;

  assert (numPredDir <= Int(NUM_REF_PIC_LIST_01));

  for ( Int refList = 0; refList < numPredDir; refList++ )
  {
    const RefPicList eRefPicList = ( refList ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );

    for ( Int refIdxTemp = 0; refIdxTemp < slice->getNumRefIdx(eRefPicList); refIdxTemp++ )
    {
      WPACDCParam *currWeightACDCParam, *refWeightACDCParam;
      slice->getWpAcDcParam(currWeightACDCParam);
      slice->getRefPic(eRefPicList, refIdxTemp)->getSlice(0)->getWpAcDcParam(refWeightACDCParam);

      for ( Int comp = 0; comp < numComp; comp++ )
      {
        const ComponentID compID        = ComponentID(comp);
        const Int         bitDepth      = slice->getSPS()->getBitDepth(toChannelType(compID));
        const Int         range         = bUseHighPrecisionWeighting ? (1<<bitDepth)/2 : 128;
        const Int         realLog2Denom = log2Denom + (bUseHighPrecisionWeighting ? RExt__PREDICTION_WEIGHTING_ANALYSIS_DC_PRECISION : (bitDepth - 8));
        const Int         realOffset    = ((Int)1<<(realLog2Denom-1));

        // current frame
        const Int64 currDC = currWeightACDCParam[comp].iDC;
        const Int64 currAC = currWeightACDCParam[comp].iAC;
        // reference frame
        const Int64 refDC  = refWeightACDCParam[comp].iDC;
        const Int64 refAC  = refWeightACDCParam[comp].iAC;

        // calculating iWeight and iOffset params
        const Double dWeight = (refAC==0) ? (Double)1.0 : Clip3( -16.0, 15.0, ((Double)currAC / (Double)refAC) );
        const Int weight     = (Int)( 0.5 + dWeight * (Double)(1<<log2Denom) );
        const Int offset     = (Int)( ((currDC<<log2Denom) - ((Int64)weight * refDC) + (Int64)realOffset) >> realLog2Denom );

        Int clippedOffset;
        if(isChroma(compID)) // Chroma offset range limination
        {
          const Int pred        = ( range - ( ( range*weight)>>(log2Denom) ) );
          const Int deltaOffset = Clip3( -4*range, 4*range-1, (offset - pred) ); // signed 10bit

          clippedOffset = Clip3( -range, range-1, (deltaOffset + pred) );  // signed 8bit
        }
        else // Luma offset range limitation
        {
          clippedOffset = Clip3( -range, range-1, offset);
        }

        // Weighting factor limitation
        const Int defaultWeight = (1<<log2Denom);
        const Int deltaWeight   = (weight - defaultWeight);

        if(deltaWeight >= range || deltaWeight < -range)
        {
          return false;
        }

        m_wp[refList][refIdxTemp][comp].bPresentFlag      = true;
        m_wp[refList][refIdxTemp][comp].iWeight           = weight;
        m_wp[refList][refIdxTemp][comp].iOffset           = clippedOffset;
        m_wp[refList][refIdxTemp][comp].uiLog2WeightDenom = log2Denom;
      }
    }
  }
  return true;
}


/** select whether weighted pred enables or not.
 * \param TComSlice *slice
 * \param log2Denom
 * \returns Bool
 */
Bool WeightPredAnalysis::xSelectWPHistExtClip(TComSlice *const slice, const Int log2Denom, const Bool bDoEnhancement, const Bool bClipInitialSADWP, const Bool bUseHistogram)
{
  const TComPicYuv *const pPic             = slice->getPic()->getPicYuvOrg();
  const Int               defaultWeight    = 1<<log2Denom;
  const Int               numPredDir       = slice->isInterP() ? 1 : 2;
  const Bool              useHighPrecision = slice->getSPS()->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag();

  assert (numPredDir <= Int(NUM_REF_PIC_LIST_01));

  for ( Int refList = 0; refList < numPredDir; refList++ )
  {
    const RefPicList eRefPicList = ( refList ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );

    for ( Int refIdxTemp = 0; refIdxTemp < slice->getNumRefIdx(eRefPicList); refIdxTemp++ )
    {
      Bool  useChromaWeight = false;

      for(Int comp=0; comp<pPic->getNumberValidComponents(); comp++)
      {
        const ComponentID  compID     = ComponentID(comp);
        const Pel         *pOrg       = pPic->getAddr(compID);
        const Pel         *pRef       = slice->getRefPic(eRefPicList, refIdxTemp)->getPicYuvRec()->getAddr(compID);
        const Int          orgStride  = pPic->getStride(compID);
        const Int          refStride  = slice->getRefPic(eRefPicList, refIdxTemp)->getPicYuvRec()->getStride(compID);
        const Int          width      = pPic->getWidth(compID);
        const Int          height     = pPic->getHeight(compID);
        const Int          bitDepth   = slice->getSPS()->getBitDepth(toChannelType(compID));
              WPScalingParam &wp      = m_wp[refList][refIdxTemp][compID];
              Int          weight     = wp.iWeight;
              Int          offset     = wp.iOffset;
              Int          weightDef  = defaultWeight;
              Int          offsetDef  = 0;

        // calculate SAD costs with/without wp for luma
        const Int64 SADnoWP = xCalcSADvalueWPOptionalClip(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom, defaultWeight, 0, useHighPrecision, bClipInitialSADWP);
        if (SADnoWP > 0)
        {
          const Int64 SADWP   = xCalcSADvalueWPOptionalClip(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom, weight,   offset, useHighPrecision, bClipInitialSADWP);
          const Double dRatioSAD = (Double)SADWP / (Double)SADnoWP;
          Double dRatioSr0SAD = std::numeric_limits<Double>::max();
          Double dRatioSrSAD  = std::numeric_limits<Double>::max();

          if (bUseHistogram)
          {
            std::vector<Int> histogramOrg;// = pPic->getHistogram(compID);
            std::vector<Int> histogramRef;// = slice->getRefPic(eRefPicList, refIdxTemp)->getPicYuvRec()->getHistogram(compID);
            std::vector<Int> searchedHistogram;

            // Compute histograms
            xCalcHistogram(pOrg, histogramOrg, width, height, orgStride, 1 << bitDepth);
            xCalcHistogram(pRef, histogramRef, width, height, refStride, 1 << bitDepth);

            // Do a histogram search around DC WP parameters; resulting distortion and 'searchedHistogram' is discarded
            xSearchHistogram(histogramOrg, histogramRef, searchedHistogram, bitDepth, log2Denom, weight, offset, useHighPrecision, compID);
            // calculate updated WP SAD
            const Int64 SADSrWP = xCalcSADvalueWP(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom, weight, offset, useHighPrecision);
            dRatioSrSAD  = (Double)SADSrWP  / (Double)SADnoWP;

            if (bDoEnhancement)
            {
              // Do the same around the default ones; resulting distortion and 'searchedHistogram' is discarded
              xSearchHistogram(histogramOrg, histogramRef, searchedHistogram, bitDepth, log2Denom, weightDef, offsetDef, useHighPrecision, compID);
              // calculate updated WP SAD
              const Int64 SADSr0WP = xCalcSADvalueWP(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom, weightDef, offsetDef, useHighPrecision);
              dRatioSr0SAD = (Double)SADSr0WP / (Double)SADnoWP;
            }
          }

          if(min(dRatioSr0SAD, min(dRatioSAD, dRatioSrSAD)) >= WEIGHT_PRED_SAD_RELATIVE_TO_NON_WEIGHT_PRED_SAD)
          {
            wp.bPresentFlag      = false;
            wp.iOffset           = 0;
            wp.iWeight           = defaultWeight;
            wp.uiLog2WeightDenom = log2Denom;
          }
          else
          {
            if (compID != COMPONENT_Y)
            {
              useChromaWeight = true;
            }

            if (dRatioSr0SAD < dRatioSrSAD && dRatioSr0SAD < dRatioSAD)
            {
              wp.bPresentFlag      = true;
              wp.iOffset           = offsetDef;
              wp.iWeight           = weightDef;
              wp.uiLog2WeightDenom = log2Denom;
            }
            else if (dRatioSrSAD < dRatioSAD)
            {
              wp.bPresentFlag      = true;
              wp.iOffset           = offset;
              wp.iWeight           = weight;
              wp.uiLog2WeightDenom = log2Denom;
            }
          }
        }
        else // (SADnoWP <= 0)
        {
          wp.bPresentFlag      = false;
          wp.iOffset           = 0;
          wp.iWeight           = defaultWeight;
          wp.uiLog2WeightDenom = log2Denom;
        }
      }

      for(Int comp=1; comp<pPic->getNumberValidComponents(); comp++)
      {
        m_wp[refList][refIdxTemp][comp].bPresentFlag = useChromaWeight;
      }
    }
  }

  return true;
}

//! select whether weighted pred enables or not.
Bool WeightPredAnalysis::xSelectWP(TComSlice *const slice, const Int log2Denom)
{
        TComPicYuv *const pPic                                = slice->getPic()->getPicYuvOrg();
  const Int               defaultWeight                       = 1<<log2Denom;
  const Int               numPredDir                          = slice->isInterP() ? 1 : 2;
  const Bool              useHighPrecisionPredictionWeighting = slice->getSPS()->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag();

  assert (numPredDir <= Int(NUM_REF_PIC_LIST_01));

  for ( Int refList = 0; refList < numPredDir; refList++ )
  {
    const RefPicList eRefPicList = ( refList ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );

    for ( Int refIdxTemp = 0; refIdxTemp < slice->getNumRefIdx(eRefPicList); refIdxTemp++ )
    {
      Int64 SADWP = 0, SADnoWP = 0;

      for(Int comp=0; comp<pPic->getNumberValidComponents(); comp++)
      {
        const ComponentID  compID     = ComponentID(comp);
              Pel         *pOrg       = pPic->getAddr(compID);
              Pel         *pRef       = slice->getRefPic(eRefPicList, refIdxTemp)->getPicYuvRec()->getAddr(compID);
        const Int          orgStride = pPic->getStride(compID);
        const Int          refStride = slice->getRefPic(eRefPicList, refIdxTemp)->getPicYuvRec()->getStride(compID);
        const Int          width     = pPic->getWidth(compID);
        const Int          height    = pPic->getHeight(compID);
        const Int          bitDepth   = slice->getSPS()->getBitDepth(toChannelType(compID));

        // calculate SAD costs with/without wp for luma
        SADWP   += xCalcSADvalueWP(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom, m_wp[refList][refIdxTemp][compID].iWeight, m_wp[refList][refIdxTemp][compID].iOffset, useHighPrecisionPredictionWeighting);
        SADnoWP += xCalcSADvalueWP(bitDepth, pOrg, pRef, width, height, orgStride, refStride, log2Denom, defaultWeight, 0, useHighPrecisionPredictionWeighting);
      }

      const Double dRatio = SADnoWP > 0 ? (((Double)SADWP / (Double)SADnoWP)) : std::numeric_limits<Double>::max();
      if(dRatio >= WEIGHT_PRED_SAD_RELATIVE_TO_NON_WEIGHT_PRED_SAD)
      {
        for(Int comp=0; comp<pPic->getNumberValidComponents(); comp++)
        {
          WPScalingParam &wp=m_wp[refList][refIdxTemp][comp];
          wp.bPresentFlag      = false;
          wp.iOffset           = 0;
          wp.iWeight           = defaultWeight;
          wp.uiLog2WeightDenom = log2Denom;
        }
      }
    }
  }

  return true;
}

// Alternatively, a SSE-based measure could be used instead.
// The respective function has been removed as it currently redundant.
static
Int64 xCalcSADvalueWP(const Int   bitDepth,
                      const Pel  *pOrgPel,
                      const Pel  *pRefPel,
                      const Int   width,
                      const Int   height,
                      const Int   orgStride,
                      const Int   refStride,
                      const Int   log2Denom,
                      const Int   weight,
                      const Int   offset,
                      const Bool  useHighPrecision)
{
  //const Int64 iSize          = iWidth*iHeight;
  const Int64 realLog2Denom = useHighPrecision ? log2Denom : (log2Denom + (bitDepth - 8));
  const Int64 realOffset    = ((Int64)offset)<<realLog2Denom;

  Int64 SAD = 0;
  for( Int y = 0; y < height; y++ )
  {
    for( Int x = 0; x < width; x++ )
    {
      SAD += abs(( ((Int64)pOrgPel[x] << (Int64) log2Denom) - ( (Int64) pRefPel[x] * (Int64) weight + (realOffset) ) ) );
    }
    pOrgPel += orgStride;
    pRefPel += refStride;
  }

  //return (iSAD/iSize);
  return SAD;
}

static
Int64 xCalcSADvalueWPOptionalClip(const Int   bitDepth,
                                  const Pel  *pOrgPel,
                                  const Pel  *pRefPel,
                                  const Int   width,
                                  const Int   height,
                                  const Int   orgStride,
                                  const Int   refStride,
                                  const Int   log2Denom,
                                  const Int   weight,
                                  const Int   offset,
                                  const Bool  useHighPrecision,
                                  const Bool  clipped)
{
  Int64 SAD = 0;
  if (clipped)
  {
    const Int64 realLog2Denom = useHighPrecision ? 0 : (bitDepth - 8);
    const Int64 realOffset    = (Int64)offset<<realLog2Denom;
    const Int64 roundOffset = (log2Denom == 0) ? 0 : 1 << (log2Denom - 1);
    const Int64 minValue = 0;
    const Int64 maxValue = (1 << bitDepth) - 1;

    for( Int y = 0; y < height; y++ )
    {
      for( Int x = 0; x < width; x++ )
      {
        Int64 scaledValue = Clip3(minValue, maxValue,  ((((Int64) pRefPel[x] * (Int64) weight + roundOffset) ) >>  (Int64) log2Denom) + realOffset);
        SAD += abs((Int64)pOrgPel[x] -  scaledValue);
      }
      pOrgPel += orgStride;
      pRefPel += refStride;
    }
  }
  else
  {
    //const Int64 iSize          = iWidth*iHeight;
    const Int64 realLog2Denom = useHighPrecision ? log2Denom : (log2Denom + (bitDepth - 8));
    const Int64 realOffset    = ((Int64)offset)<<realLog2Denom;

    for( Int y = 0; y < height; y++ )
    {
      for( Int x = 0; x < width; x++ )
      {
        SAD += abs(( ((Int64)pOrgPel[x] << (Int64) log2Denom) - ( (Int64) pRefPel[x] * (Int64) weight + (realOffset) ) ) );
      }
      pOrgPel += orgStride;
      pRefPel += refStride;
    }
  }
  return SAD;
}
