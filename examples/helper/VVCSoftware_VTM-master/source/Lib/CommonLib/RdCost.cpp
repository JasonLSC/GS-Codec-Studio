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

/** \file     RdCost.cpp
    \brief    RD cost computation class
*/

#define DONT_UNDEF_SIZE_AWARE_PER_EL_OP

#include "RdCost.h"

#include "Rom.h"
#include "UnitPartitioner.h"

#include <limits>

//! \ingroup CommonLib
//! \{

EnumArray<DistFunc, DFunc> RdCost::m_distortionFunc;

RdCost::RdCost()
{
  init();
}

RdCost::~RdCost()
{
}

#if WCG_EXT
EnumArray<DistFuncWtd, DFuncWtd> RdCost::m_distortionFuncWtd;
double RdCost::calcRdCost( uint64_t fracBits, Distortion distortion, bool useUnadjustedLambda )
#else
double RdCost::calcRdCost( uint64_t fracBits, Distortion distortion )
#endif
{
  if (m_costMode == COST_LOSSLESS_CODING && 0 != distortion && m_isLosslessRDCost)
  {
    return MAX_DOUBLE;
  }
#if WCG_EXT
  return (useUnadjustedLambda ? m_distScaleUnadjusted : m_distScale) * double(distortion) + double(fracBits);
#else
  return m_distScale * double(distortion) + double(fracBits);
#endif
}

void RdCost::setLambda( double dLambda, const BitDepths &bitDepths )
{
  m_dLambda             = dLambda;
  m_distScale           = double(1 << SCALE_BITS) / m_dLambda;
  m_dLambdaMotionSAD    = sqrt(m_dLambda);
}

void RdCost::lambdaAdjustColorTrans(bool forward, ComponentID componentID, bool applyChromaScale, int* resScaleInv)
{
  if (m_resetStore)
  {
    for (uint8_t component = 0; component < MAX_NUM_COMPONENT; component++)
    {
      ComponentID compID = (ComponentID)component;
      int       delta_QP = DELTA_QP_ACT[compID];
      double lamdbaAdjustRate = pow(2.0, delta_QP / 3.0);

      m_lambdaStore[0][component] = m_dLambda;
      m_distScaleStore[0][component] = m_distScale;

      m_lambdaStore[1][component] = m_dLambda * lamdbaAdjustRate;
      m_distScaleStore[1][component] = double(1 << SCALE_BITS) / m_lambdaStore[1][component];
    }
    m_resetStore = false;
  }

  if (forward)
  {
    CHECK(m_pairCheck == 1, "lambda has been already adjusted");
    m_pairCheck = 1;
  }
  else
  {
    CHECK(m_pairCheck == 0, "lambda has not been adjusted");
    m_pairCheck = 0;
  }

  m_dLambda = m_lambdaStore[m_pairCheck][componentID];
  m_distScale = m_distScaleStore[m_pairCheck][componentID];
  if (applyChromaScale)
  {
    CHECK(m_pairCheck == 0 || componentID == COMPONENT_Y, "wrong lambda adjustment for CS");
    double cResScale = (double)(1 << CSCALE_FP_PREC) / (double)(*resScaleInv);
    m_dLambda = m_dLambda / (cResScale*cResScale);
    m_distScale      = double(1 << SCALE_BITS) / m_dLambda;
  }
  if (m_pairCheck == 0)
  {
    CHECK(m_distScale != m_distScaleUnadjusted, "lambda should be adjusted to the original value");
  }
}

// Initialize Function Pointer by [distFunc]
void RdCost::init()
{
  m_distortionFunc[DFunc::SSE]    = RdCost::xGetSSE;
  m_distortionFunc[DFunc::SSE2]   = RdCost::xGetSSE;
  m_distortionFunc[DFunc::SSE4]   = RdCost::xGetSSE4;
  m_distortionFunc[DFunc::SSE8]   = RdCost::xGetSSE8;
  m_distortionFunc[DFunc::SSE16]  = RdCost::xGetSSE16;
  m_distortionFunc[DFunc::SSE32]  = RdCost::xGetSSE32;
  m_distortionFunc[DFunc::SSE64]  = RdCost::xGetSSE64;
  m_distortionFunc[DFunc::SSE16N] = RdCost::xGetSSE16N;

  m_distortionFunc[DFunc::SAD]    = RdCost::xGetSAD;
  m_distortionFunc[DFunc::SAD2]   = RdCost::xGetSAD;
  m_distortionFunc[DFunc::SAD4]   = RdCost::xGetSAD4;
  m_distortionFunc[DFunc::SAD8]   = RdCost::xGetSAD8;
  m_distortionFunc[DFunc::SAD16]  = RdCost::xGetSAD16;
  m_distortionFunc[DFunc::SAD32]  = RdCost::xGetSAD32;
  m_distortionFunc[DFunc::SAD64]  = RdCost::xGetSAD64;
  m_distortionFunc[DFunc::SAD16N] = RdCost::xGetSAD16N;

  m_distortionFunc[DFunc::SAD12] = RdCost::xGetSAD12;
  m_distortionFunc[DFunc::SAD24] = RdCost::xGetSAD24;
  m_distortionFunc[DFunc::SAD48] = RdCost::xGetSAD48;

  m_distortionFunc[DFunc::HAD]    = RdCost::xGetHADs;
  m_distortionFunc[DFunc::HAD2]   = RdCost::xGetHADs;
  m_distortionFunc[DFunc::HAD4]   = RdCost::xGetHADs;
  m_distortionFunc[DFunc::HAD8]   = RdCost::xGetHADs;
  m_distortionFunc[DFunc::HAD16]  = RdCost::xGetHADs;
  m_distortionFunc[DFunc::HAD32]  = RdCost::xGetHADs;
  m_distortionFunc[DFunc::HAD64]  = RdCost::xGetHADs;
  m_distortionFunc[DFunc::HAD16N] = RdCost::xGetHADs;

  m_distortionFunc[DFunc::MRSAD]    = RdCost::xGetMRSAD;
  m_distortionFunc[DFunc::MRSAD2]   = RdCost::xGetMRSAD;
  m_distortionFunc[DFunc::MRSAD4]   = RdCost::xGetMRSAD4;
  m_distortionFunc[DFunc::MRSAD8]   = RdCost::xGetMRSAD8;
  m_distortionFunc[DFunc::MRSAD16]  = RdCost::xGetMRSAD16;
  m_distortionFunc[DFunc::MRSAD32]  = RdCost::xGetMRSAD32;
  m_distortionFunc[DFunc::MRSAD64]  = RdCost::xGetMRSAD64;
  m_distortionFunc[DFunc::MRSAD16N] = RdCost::xGetMRSAD16N;

  m_distortionFunc[DFunc::MRSAD12] = RdCost::xGetMRSAD12;
  m_distortionFunc[DFunc::MRSAD24] = RdCost::xGetMRSAD24;
  m_distortionFunc[DFunc::MRSAD48] = RdCost::xGetMRSAD48;

  m_distortionFunc[DFunc::MRHAD]    = RdCost::xGetMRHADs;
  m_distortionFunc[DFunc::MRHAD2]   = RdCost::xGetMRHADs;
  m_distortionFunc[DFunc::MRHAD4]   = RdCost::xGetMRHADs;
  m_distortionFunc[DFunc::MRHAD8]   = RdCost::xGetMRHADs;
  m_distortionFunc[DFunc::MRHAD16]  = RdCost::xGetMRHADs;
  m_distortionFunc[DFunc::MRHAD32]  = RdCost::xGetMRHADs;
  m_distortionFunc[DFunc::MRHAD64]  = RdCost::xGetMRHADs;
  m_distortionFunc[DFunc::MRHAD16N] = RdCost::xGetMRHADs;

  m_distortionFunc[DFunc::SAD_FULL_NBIT]    = RdCost::xGetSAD_full;
  m_distortionFunc[DFunc::SAD_FULL_NBIT2]   = RdCost::xGetSAD_full;
  m_distortionFunc[DFunc::SAD_FULL_NBIT4]   = RdCost::xGetSAD_full;
  m_distortionFunc[DFunc::SAD_FULL_NBIT8]   = RdCost::xGetSAD_full;
  m_distortionFunc[DFunc::SAD_FULL_NBIT16]  = RdCost::xGetSAD_full;
  m_distortionFunc[DFunc::SAD_FULL_NBIT32]  = RdCost::xGetSAD_full;
  m_distortionFunc[DFunc::SAD_FULL_NBIT64]  = RdCost::xGetSAD_full;
  m_distortionFunc[DFunc::SAD_FULL_NBIT16N] = RdCost::xGetSAD_full;

#if WCG_EXT
  m_distortionFuncWtd[DFuncWtd::SSE_WTD]    = &RdCost::xGetSSE_WTD;
  m_distortionFuncWtd[DFuncWtd::SSE2_WTD]   = &RdCost::xGetSSE2_WTD;
  m_distortionFuncWtd[DFuncWtd::SSE4_WTD]   = &RdCost::xGetSSE4_WTD;
  m_distortionFuncWtd[DFuncWtd::SSE8_WTD]   = &RdCost::xGetSSE8_WTD;
  m_distortionFuncWtd[DFuncWtd::SSE16_WTD]  = &RdCost::xGetSSE16_WTD;
  m_distortionFuncWtd[DFuncWtd::SSE32_WTD]  = &RdCost::xGetSSE32_WTD;
  m_distortionFuncWtd[DFuncWtd::SSE64_WTD]  = &RdCost::xGetSSE64_WTD;
  m_distortionFuncWtd[DFuncWtd::SSE16N_WTD] = &RdCost::xGetSSE16N_WTD;
#endif

  m_distortionFunc[DFunc::SAD_INTERMEDIATE_BITDEPTH] = RdCost::xGetSAD;

  m_distortionFunc[DFunc::SAD_WITH_MASK] = RdCost::xGetSADwMask;

#if ENABLE_SIMD_OPT_DIST
#ifdef TARGET_SIMD_X86
  initRdCostX86();
#endif
#endif

  m_costMode                   = COST_STANDARD_LOSSY;

  m_motionLambda               = 0;
  m_iCostScale                 = 0;
  m_resetStore = true;
  m_pairCheck    = 0;
}

void RdCost::setDistParam(DistParam &rcDP, const CPelBuf &org, const Pel *piRefY, ptrdiff_t iRefStride, int bitDepth,
                          ComponentID compID, int subShiftMode, int step, bool useHadamard)
{
  rcDP.bitDepth   = bitDepth;
  rcDP.compID     = compID;

  // set Original & Curr Pointer / Stride
  rcDP.org        = org;

  rcDP.cur.buf    = piRefY;
  rcDP.cur.stride = iRefStride;

  // set Block Width / Height
  rcDP.cur.width    = org.width;
  rcDP.cur.height   = org.height;
  rcDP.step         = step;
  rcDP.maximumDistortionForEarlyExit = std::numeric_limits<Distortion>::max();

  if( !useHadamard )
  {
    const DFunc baseIdx = rcDP.useMR ? DFunc::MRSAD : DFunc::SAD;

    rcDP.distFunc = m_distortionFunc[baseIdx + sizeOffset<true>(org.width)];
  }
  else
  {
    const DFunc baseIdx = rcDP.useMR ? DFunc::MRHAD : DFunc::HAD;

    rcDP.distFunc = m_distortionFunc[baseIdx + sizeOffset<false>(org.width)];
  }

  // initialize
  rcDP.subShift  = 0;

  if( subShiftMode == 1 )
  {
    if( rcDP.org.height > 32 && ( rcDP.org.height & 15 ) == 0 )
    {
      rcDP.subShift = 4;
    }
    else if( rcDP.org.height > 16 && ( rcDP.org.height & 7 ) == 0 )
    {
      rcDP.subShift = 3;
    }
    else if( rcDP.org.height > 8 && ( rcDP.org.height & 3 ) == 0 )
    {
      rcDP.subShift = 2;
    }
    else if( ( rcDP.org.height & 1 ) == 0 )
    {
      rcDP.subShift = 1;
    }
  }
  else if( subShiftMode == 2 )
  {
    if( rcDP.org.height > 8 && rcDP.org.width <= 64 )
    {
      rcDP.subShift = 1;
    }
  }
  else if( subShiftMode == 3 )
  {
    if (rcDP.org.height > 8 )
    {
      rcDP.subShift = 1;
    }
  }
}

void RdCost::setDistParam( DistParam &rcDP, const CPelBuf &org, const CPelBuf &cur, int bitDepth, ComponentID compID, bool useHadamard )
{
  rcDP.org          = org;
  rcDP.cur          = cur;
  rcDP.step         = 1;
  rcDP.subShift     = 0;
  rcDP.bitDepth     = bitDepth;
  rcDP.compID       = compID;

  if( !useHadamard )
  {
    const DFunc baseIdx = rcDP.useMR ? DFunc::MRSAD : DFunc::SAD;

    rcDP.distFunc = m_distortionFunc[baseIdx + sizeOffset<true>(org.width)];
  }
  else
  {
    const DFunc baseIdx = rcDP.useMR ? DFunc::MRHAD : DFunc::HAD;

    rcDP.distFunc = m_distortionFunc[baseIdx + sizeOffset<false>(org.width)];
  }

  rcDP.maximumDistortionForEarlyExit = std::numeric_limits<Distortion>::max();
}

void RdCost::setDistParam(DistParam &rcDP, const Pel *pOrg, const Pel *piRefY, ptrdiff_t iOrgStride,
                          ptrdiff_t iRefStride, int bitDepth, ComponentID compID, int width, int height,
                          int subShiftMode, int step, bool useHadamard, bool bioApplied)
{
  rcDP.bitDepth   = bitDepth;
  rcDP.compID     = compID;

  rcDP.org.buf    = pOrg;
  rcDP.org.stride = iOrgStride;
  rcDP.org.width  = width;
  rcDP.org.height = height;

  rcDP.cur.buf    = piRefY;
  rcDP.cur.stride = iRefStride;
  rcDP.cur.width  = width;
  rcDP.cur.height = height;
  rcDP.subShift = subShiftMode;
  rcDP.step       = step;
  rcDP.maximumDistortionForEarlyExit = std::numeric_limits<Distortion>::max();
  CHECK(useHadamard || rcDP.useMR, "only used in xDmvrCost with these default parameters (so far...)");
  if ( bioApplied )
  {
    rcDP.distFunc = m_distortionFunc[DFunc::SAD_INTERMEDIATE_BITDEPTH];
    return;
  }

  rcDP.distFunc = m_distortionFunc[DFunc::SAD + sizeOffset<true>(width)];
}

#if WCG_EXT
Distortion RdCost::getDistPart(const CPelBuf &org, const CPelBuf &cur, int bitDepth, const ComponentID compID,
                               DFuncWtd distFuncWtd, const CPelBuf &orgLuma) const
{
  DistParam cDtParam;

  cDtParam.org        = org;
  cDtParam.cur        = cur;
  cDtParam.step       = 1;
  cDtParam.bitDepth   = bitDepth;
  cDtParam.compID     = compID;

  if (isChroma(compID))
  {
    cDtParam.orgLuma  = orgLuma;
  }
  else
  {
    cDtParam.orgLuma  = org;
  }

  Distortion dist;
  if (isChroma(compID) && (m_signalType == RESHAPE_SIGNAL_SDR || m_signalType == RESHAPE_SIGNAL_HLG))
  {
       cDtParam.distFunc = m_distortionFunc[DFunc::SSE + sizeOffset<false>(org.width)];
       int64_t weight = m_chromaWeight;
       dist = (weight * cDtParam.distFunc(cDtParam ) + (1 << MSE_WEIGHT_FRAC_BITS >> 1)) >> (MSE_WEIGHT_FRAC_BITS);
  }
  else
  {
    cDtParam.cShiftX = getComponentScaleX(compID,  m_cf);
    cDtParam.cShiftY = getComponentScaleY(compID,  m_cf);
    cDtParam.distFuncWtd = m_distortionFuncWtd[distFuncWtd + sizeOffset<false>(org.width)];
    dist = cDtParam.distFuncWtd(this, cDtParam);
  }
  if (isChroma(compID))
  {
    dist = (Distortion)(m_distortionWeight[MAP_CHROMA(compID)] * dist);
  }
  return dist;
}
#endif
Distortion RdCost::getDistPart(const CPelBuf &org, const CPelBuf &cur, int bitDepth, const ComponentID compID,
                                   DFunc distFunc) const
{
  DistParam cDtParam;

  cDtParam.org        = org;
  cDtParam.cur        = cur;
  cDtParam.step       = 1;
  cDtParam.bitDepth   = bitDepth;
  cDtParam.compID     = compID;

  cDtParam.distFunc = m_distortionFunc[distFunc + sizeOffset<false>(org.width)];

  if (isChroma(compID))
  {
    return ((Distortion) (m_distortionWeight[ MAP_CHROMA(compID) ] * cDtParam.distFunc( cDtParam )));
  }
  else
  {
    return cDtParam.distFunc( cDtParam );
  }
}

// ====================================================================================================================
// Distortion functions
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// SAD
// --------------------------------------------------------------------------------------------------------------------

Distortion RdCost::xGetSAD_full( const DistParam& rcDtParam )
{
  CHECK( rcDtParam.applyWeight, "Cannot apply weight when using full-bit SAD!" );
  const Pel* piOrg = rcDtParam.org.buf;
  const Pel* piCur = rcDtParam.cur.buf;
  const int  height    = rcDtParam.org.height;
  const int  width     = rcDtParam.org.width;
  const int  subShift  = rcDtParam.subShift;
  const int  subStep   = (1 << subShift);
  const ptrdiff_t strideCur = rcDtParam.cur.stride * subStep;
  const ptrdiff_t strideOrg = rcDtParam.org.stride * subStep;

  Distortion sum = 0;

#define SAD_OP(ADDR) sum += abs(piOrg[ADDR] - piCur[ADDR]);
#define SAD_INC                                                                                                        \
  piOrg += strideOrg;                                                                                                  \
  piCur += strideCur;

  SIZE_AWARE_PER_EL_OP( SAD_OP, SAD_INC )

#undef SAD_OP
#undef SAD_INC

  sum <<= subShift;
  return sum;
}

Distortion RdCost::xGetSAD( const DistParam& rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSADw( rcDtParam );
  }

  const Pel* piOrg           = rcDtParam.org.buf;
  const Pel* piCur           = rcDtParam.cur.buf;
  const int      cols            = rcDtParam.org.width;
  int            rows            = rcDtParam.org.height;
  const int      subShift        = rcDtParam.subShift;
  const int      subStep         = (1 << subShift);
  const ptrdiff_t strideCur       = rcDtParam.cur.stride * subStep;
  const ptrdiff_t strideOrg       = rcDtParam.org.stride * subStep;
  const uint32_t distortionShift = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth);

  Distortion sum = 0;

  for (; rows != 0; rows -= subStep)
  {
    for (int n = 0; n < cols; n++)
    {
      sum += abs(piOrg[n] - piCur[n]);
    }
    if (rcDtParam.maximumDistortionForEarlyExit < (sum >> distortionShift))
    {
      return (sum >> distortionShift);
    }
    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> distortionShift);
}

Distortion RdCost::xGetSAD4( const DistParam& rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSADw( rcDtParam );
  }

  const Pel* piOrg   = rcDtParam.org.buf;
  const Pel* piCur   = rcDtParam.cur.buf;
  int        rows      = rcDtParam.org.height;
  const int  subShift  = rcDtParam.subShift;
  const int  subStep   = (1 << subShift);
  const ptrdiff_t strideCur = rcDtParam.cur.stride * subStep;
  const ptrdiff_t strideOrg = rcDtParam.org.stride * subStep;

  Distortion sum = 0;

  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0]);
    sum += abs(piOrg[1] - piCur[1]);
    sum += abs(piOrg[2] - piCur[2]);
    sum += abs(piOrg[3] - piCur[3]);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetSAD8( const DistParam& rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSADw( rcDtParam );
  }

  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  const int  subShift   = rcDtParam.subShift;
  const int  subStep    = (1 << subShift);
  const ptrdiff_t strideCur  = rcDtParam.cur.stride * subStep;
  const ptrdiff_t strideOrg  = rcDtParam.org.stride * subStep;

  Distortion sum = 0;

  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0]);
    sum += abs(piOrg[1] - piCur[1]);
    sum += abs(piOrg[2] - piCur[2]);
    sum += abs(piOrg[3] - piCur[3]);
    sum += abs(piOrg[4] - piCur[4]);
    sum += abs(piOrg[5] - piCur[5]);
    sum += abs(piOrg[6] - piCur[6]);
    sum += abs(piOrg[7] - piCur[7]);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetSAD16( const DistParam& rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSADw( rcDtParam );
  }

  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  const int  subShift   = rcDtParam.subShift;
  const int  subStep    = (1 << subShift);
  const ptrdiff_t strideCur  = rcDtParam.cur.stride * subStep;
  const ptrdiff_t strideOrg  = rcDtParam.org.stride * subStep;

  Distortion sum = 0;

  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0]);
    sum += abs(piOrg[1] - piCur[1]);
    sum += abs(piOrg[2] - piCur[2]);
    sum += abs(piOrg[3] - piCur[3]);
    sum += abs(piOrg[4] - piCur[4]);
    sum += abs(piOrg[5] - piCur[5]);
    sum += abs(piOrg[6] - piCur[6]);
    sum += abs(piOrg[7] - piCur[7]);
    sum += abs(piOrg[8] - piCur[8]);
    sum += abs(piOrg[9] - piCur[9]);
    sum += abs(piOrg[10] - piCur[10]);
    sum += abs(piOrg[11] - piCur[11]);
    sum += abs(piOrg[12] - piCur[12]);
    sum += abs(piOrg[13] - piCur[13]);
    sum += abs(piOrg[14] - piCur[14]);
    sum += abs(piOrg[15] - piCur[15]);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetSAD12( const DistParam& rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSADw( rcDtParam );
  }

  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  const int  subShift   = rcDtParam.subShift;
  const int  subStep    = (1 << subShift);
  const ptrdiff_t strideCur  = rcDtParam.cur.stride * subStep;
  const ptrdiff_t strideOrg  = rcDtParam.org.stride * subStep;

  Distortion sum = 0;

  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0]);
    sum += abs(piOrg[1] - piCur[1]);
    sum += abs(piOrg[2] - piCur[2]);
    sum += abs(piOrg[3] - piCur[3]);
    sum += abs(piOrg[4] - piCur[4]);
    sum += abs(piOrg[5] - piCur[5]);
    sum += abs(piOrg[6] - piCur[6]);
    sum += abs(piOrg[7] - piCur[7]);
    sum += abs(piOrg[8] - piCur[8]);
    sum += abs(piOrg[9] - piCur[9]);
    sum += abs(piOrg[10] - piCur[10]);
    sum += abs(piOrg[11] - piCur[11]);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetSAD16N( const DistParam &rcDtParam )
{
  const Pel* piOrg  = rcDtParam.org.buf;
  const Pel* piCur  = rcDtParam.cur.buf;
  int        rows      = rcDtParam.org.height;
  const int  cols      = rcDtParam.org.width;
  const int  subShift  = rcDtParam.subShift;
  const int  subStep   = (1 << subShift);
  const ptrdiff_t strideCur = rcDtParam.cur.stride * subStep;
  const ptrdiff_t strideOrg = rcDtParam.org.stride * subStep;

  Distortion sum = 0;

  for (; rows != 0; rows -= subStep)
  {
    for (int n = 0; n < cols; n += 16)
    {
      sum += abs(piOrg[n + 0] - piCur[n + 0]);
      sum += abs(piOrg[n + 1] - piCur[n + 1]);
      sum += abs(piOrg[n + 2] - piCur[n + 2]);
      sum += abs(piOrg[n + 3] - piCur[n + 3]);
      sum += abs(piOrg[n + 4] - piCur[n + 4]);
      sum += abs(piOrg[n + 5] - piCur[n + 5]);
      sum += abs(piOrg[n + 6] - piCur[n + 6]);
      sum += abs(piOrg[n + 7] - piCur[n + 7]);
      sum += abs(piOrg[n + 8] - piCur[n + 8]);
      sum += abs(piOrg[n + 9] - piCur[n + 9]);
      sum += abs(piOrg[n + 10] - piCur[n + 10]);
      sum += abs(piOrg[n + 11] - piCur[n + 11]);
      sum += abs(piOrg[n + 12] - piCur[n + 12]);
      sum += abs(piOrg[n + 13] - piCur[n + 13]);
      sum += abs(piOrg[n + 14] - piCur[n + 14]);
      sum += abs(piOrg[n + 15] - piCur[n + 15]);
    }
    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetSAD32( const DistParam &rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSADw( rcDtParam );
  }

  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        subShift   = rcDtParam.subShift;
  int        subStep    = (1 << subShift);
  ptrdiff_t  strideCur  = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride * subStep;

  Distortion sum = 0;

  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0]);
    sum += abs(piOrg[1] - piCur[1]);
    sum += abs(piOrg[2] - piCur[2]);
    sum += abs(piOrg[3] - piCur[3]);
    sum += abs(piOrg[4] - piCur[4]);
    sum += abs(piOrg[5] - piCur[5]);
    sum += abs(piOrg[6] - piCur[6]);
    sum += abs(piOrg[7] - piCur[7]);
    sum += abs(piOrg[8] - piCur[8]);
    sum += abs(piOrg[9] - piCur[9]);
    sum += abs(piOrg[10] - piCur[10]);
    sum += abs(piOrg[11] - piCur[11]);
    sum += abs(piOrg[12] - piCur[12]);
    sum += abs(piOrg[13] - piCur[13]);
    sum += abs(piOrg[14] - piCur[14]);
    sum += abs(piOrg[15] - piCur[15]);
    sum += abs(piOrg[16] - piCur[16]);
    sum += abs(piOrg[17] - piCur[17]);
    sum += abs(piOrg[18] - piCur[18]);
    sum += abs(piOrg[19] - piCur[19]);
    sum += abs(piOrg[20] - piCur[20]);
    sum += abs(piOrg[21] - piCur[21]);
    sum += abs(piOrg[22] - piCur[22]);
    sum += abs(piOrg[23] - piCur[23]);
    sum += abs(piOrg[24] - piCur[24]);
    sum += abs(piOrg[25] - piCur[25]);
    sum += abs(piOrg[26] - piCur[26]);
    sum += abs(piOrg[27] - piCur[27]);
    sum += abs(piOrg[28] - piCur[28]);
    sum += abs(piOrg[29] - piCur[29]);
    sum += abs(piOrg[30] - piCur[30]);
    sum += abs(piOrg[31] - piCur[31]);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetSAD24( const DistParam &rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSADw( rcDtParam );
  }

  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        subShift   = rcDtParam.subShift;
  int        subStep    = (1 << subShift);
  ptrdiff_t  strideCur  = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride * subStep;

  Distortion sum = 0;

  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0]);
    sum += abs(piOrg[1] - piCur[1]);
    sum += abs(piOrg[2] - piCur[2]);
    sum += abs(piOrg[3] - piCur[3]);
    sum += abs(piOrg[4] - piCur[4]);
    sum += abs(piOrg[5] - piCur[5]);
    sum += abs(piOrg[6] - piCur[6]);
    sum += abs(piOrg[7] - piCur[7]);
    sum += abs(piOrg[8] - piCur[8]);
    sum += abs(piOrg[9] - piCur[9]);
    sum += abs(piOrg[10] - piCur[10]);
    sum += abs(piOrg[11] - piCur[11]);
    sum += abs(piOrg[12] - piCur[12]);
    sum += abs(piOrg[13] - piCur[13]);
    sum += abs(piOrg[14] - piCur[14]);
    sum += abs(piOrg[15] - piCur[15]);
    sum += abs(piOrg[16] - piCur[16]);
    sum += abs(piOrg[17] - piCur[17]);
    sum += abs(piOrg[18] - piCur[18]);
    sum += abs(piOrg[19] - piCur[19]);
    sum += abs(piOrg[20] - piCur[20]);
    sum += abs(piOrg[21] - piCur[21]);
    sum += abs(piOrg[22] - piCur[22]);
    sum += abs(piOrg[23] - piCur[23]);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetSAD64( const DistParam &rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSADw( rcDtParam );
  }

  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        subShift   = rcDtParam.subShift;
  int        subStep    = (1 << subShift);
  ptrdiff_t  strideCur  = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride * subStep;

  Distortion sum = 0;

  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0]);
    sum += abs(piOrg[1] - piCur[1]);
    sum += abs(piOrg[2] - piCur[2]);
    sum += abs(piOrg[3] - piCur[3]);
    sum += abs(piOrg[4] - piCur[4]);
    sum += abs(piOrg[5] - piCur[5]);
    sum += abs(piOrg[6] - piCur[6]);
    sum += abs(piOrg[7] - piCur[7]);
    sum += abs(piOrg[8] - piCur[8]);
    sum += abs(piOrg[9] - piCur[9]);
    sum += abs(piOrg[10] - piCur[10]);
    sum += abs(piOrg[11] - piCur[11]);
    sum += abs(piOrg[12] - piCur[12]);
    sum += abs(piOrg[13] - piCur[13]);
    sum += abs(piOrg[14] - piCur[14]);
    sum += abs(piOrg[15] - piCur[15]);
    sum += abs(piOrg[16] - piCur[16]);
    sum += abs(piOrg[17] - piCur[17]);
    sum += abs(piOrg[18] - piCur[18]);
    sum += abs(piOrg[19] - piCur[19]);
    sum += abs(piOrg[20] - piCur[20]);
    sum += abs(piOrg[21] - piCur[21]);
    sum += abs(piOrg[22] - piCur[22]);
    sum += abs(piOrg[23] - piCur[23]);
    sum += abs(piOrg[24] - piCur[24]);
    sum += abs(piOrg[25] - piCur[25]);
    sum += abs(piOrg[26] - piCur[26]);
    sum += abs(piOrg[27] - piCur[27]);
    sum += abs(piOrg[28] - piCur[28]);
    sum += abs(piOrg[29] - piCur[29]);
    sum += abs(piOrg[30] - piCur[30]);
    sum += abs(piOrg[31] - piCur[31]);
    sum += abs(piOrg[32] - piCur[32]);
    sum += abs(piOrg[33] - piCur[33]);
    sum += abs(piOrg[34] - piCur[34]);
    sum += abs(piOrg[35] - piCur[35]);
    sum += abs(piOrg[36] - piCur[36]);
    sum += abs(piOrg[37] - piCur[37]);
    sum += abs(piOrg[38] - piCur[38]);
    sum += abs(piOrg[39] - piCur[39]);
    sum += abs(piOrg[40] - piCur[40]);
    sum += abs(piOrg[41] - piCur[41]);
    sum += abs(piOrg[42] - piCur[42]);
    sum += abs(piOrg[43] - piCur[43]);
    sum += abs(piOrg[44] - piCur[44]);
    sum += abs(piOrg[45] - piCur[45]);
    sum += abs(piOrg[46] - piCur[46]);
    sum += abs(piOrg[47] - piCur[47]);
    sum += abs(piOrg[48] - piCur[48]);
    sum += abs(piOrg[49] - piCur[49]);
    sum += abs(piOrg[50] - piCur[50]);
    sum += abs(piOrg[51] - piCur[51]);
    sum += abs(piOrg[52] - piCur[52]);
    sum += abs(piOrg[53] - piCur[53]);
    sum += abs(piOrg[54] - piCur[54]);
    sum += abs(piOrg[55] - piCur[55]);
    sum += abs(piOrg[56] - piCur[56]);
    sum += abs(piOrg[57] - piCur[57]);
    sum += abs(piOrg[58] - piCur[58]);
    sum += abs(piOrg[59] - piCur[59]);
    sum += abs(piOrg[60] - piCur[60]);
    sum += abs(piOrg[61] - piCur[61]);
    sum += abs(piOrg[62] - piCur[62]);
    sum += abs(piOrg[63] - piCur[63]);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetSAD48( const DistParam &rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSADw( rcDtParam );
  }

  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        subShift   = rcDtParam.subShift;
  int        subStep    = (1 << subShift);
  ptrdiff_t  strideCur  = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride * subStep;

  Distortion sum = 0;

  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0]);
    sum += abs(piOrg[1] - piCur[1]);
    sum += abs(piOrg[2] - piCur[2]);
    sum += abs(piOrg[3] - piCur[3]);
    sum += abs(piOrg[4] - piCur[4]);
    sum += abs(piOrg[5] - piCur[5]);
    sum += abs(piOrg[6] - piCur[6]);
    sum += abs(piOrg[7] - piCur[7]);
    sum += abs(piOrg[8] - piCur[8]);
    sum += abs(piOrg[9] - piCur[9]);
    sum += abs(piOrg[10] - piCur[10]);
    sum += abs(piOrg[11] - piCur[11]);
    sum += abs(piOrg[12] - piCur[12]);
    sum += abs(piOrg[13] - piCur[13]);
    sum += abs(piOrg[14] - piCur[14]);
    sum += abs(piOrg[15] - piCur[15]);
    sum += abs(piOrg[16] - piCur[16]);
    sum += abs(piOrg[17] - piCur[17]);
    sum += abs(piOrg[18] - piCur[18]);
    sum += abs(piOrg[19] - piCur[19]);
    sum += abs(piOrg[20] - piCur[20]);
    sum += abs(piOrg[21] - piCur[21]);
    sum += abs(piOrg[22] - piCur[22]);
    sum += abs(piOrg[23] - piCur[23]);
    sum += abs(piOrg[24] - piCur[24]);
    sum += abs(piOrg[25] - piCur[25]);
    sum += abs(piOrg[26] - piCur[26]);
    sum += abs(piOrg[27] - piCur[27]);
    sum += abs(piOrg[28] - piCur[28]);
    sum += abs(piOrg[29] - piCur[29]);
    sum += abs(piOrg[30] - piCur[30]);
    sum += abs(piOrg[31] - piCur[31]);
    sum += abs(piOrg[32] - piCur[32]);
    sum += abs(piOrg[33] - piCur[33]);
    sum += abs(piOrg[34] - piCur[34]);
    sum += abs(piOrg[35] - piCur[35]);
    sum += abs(piOrg[36] - piCur[36]);
    sum += abs(piOrg[37] - piCur[37]);
    sum += abs(piOrg[38] - piCur[38]);
    sum += abs(piOrg[39] - piCur[39]);
    sum += abs(piOrg[40] - piCur[40]);
    sum += abs(piOrg[41] - piCur[41]);
    sum += abs(piOrg[42] - piCur[42]);
    sum += abs(piOrg[43] - piCur[43]);
    sum += abs(piOrg[44] - piCur[44]);
    sum += abs(piOrg[45] - piCur[45]);
    sum += abs(piOrg[46] - piCur[46]);
    sum += abs(piOrg[47] - piCur[47]);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}




// --------------------------------------------------------------------------------------------------------------------
// MRSAD
// --------------------------------------------------------------------------------------------------------------------

Distortion RdCost::xGetMRSAD( const DistParam& rcDtParam )
{
  const Pel* piOrg           = rcDtParam.org.buf;
  const Pel* piCur           = rcDtParam.cur.buf;
  const int      cols            = rcDtParam.org.width;
  int            rows            = rcDtParam.org.height;
  const int      subShift        = rcDtParam.subShift;
  const int      subStep         = (1 << subShift);
  const ptrdiff_t strideCur       = rcDtParam.cur.stride * subStep;
  const ptrdiff_t strideOrg       = rcDtParam.org.stride * subStep;
  const uint32_t distortionShift = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth);

  int32_t deltaSum = 0;
  for (int r = rows; r != 0; r -= subStep, piOrg += strideOrg, piCur += strideCur)
  {
    for (int n = 0; n < cols; n++)
    {
      deltaSum += ( piOrg[n] - piCur[n] );
    }
  }

  const Pel offset  = Pel(deltaSum / (cols * (rows >> subShift)));
  piOrg             = rcDtParam.org.buf;
  piCur             = rcDtParam.cur.buf;
  Distortion sum    = 0;
  for (; rows != 0; rows -= subStep)
  {
    for (int n = 0; n < cols; n++)
    {
      sum += abs(piOrg[n] - piCur[n] - offset);
    }
    if (rcDtParam.maximumDistortionForEarlyExit < (sum >> distortionShift))
    {
      return (sum >> distortionShift);
    }
    piOrg += strideOrg;
    piCur += strideCur;
  }
  sum <<= subShift;
  return (sum >> distortionShift);
}


Distortion RdCost::xGetMRSAD4( const DistParam& rcDtParam )
{
  const Pel* piOrg   = rcDtParam.org.buf;
  const Pel* piCur   = rcDtParam.cur.buf;
  int        rows      = rcDtParam.org.height;
  int        subShift  = rcDtParam.subShift;
  int        subStep   = (1 << subShift);
  ptrdiff_t  strideCur = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg = rcDtParam.org.stride * subStep;

  int32_t deltaSum = 0;
  for (int r = rows; r != 0; r -= subStep, piOrg += strideOrg, piCur += strideCur)
  {
    deltaSum += ( piOrg[0] - piCur[0] );
    deltaSum += ( piOrg[1] - piCur[1] );
    deltaSum += ( piOrg[2] - piCur[2] );
    deltaSum += ( piOrg[3] - piCur[3] );
  }

  const Pel offset  = Pel(deltaSum / (4 * (rows >> subShift)));
  piOrg             = rcDtParam.org.buf;
  piCur             = rcDtParam.cur.buf;
  Distortion sum    = 0;
  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0] - offset);
    sum += abs(piOrg[1] - piCur[1] - offset);
    sum += abs(piOrg[2] - piCur[2] - offset);
    sum += abs(piOrg[3] - piCur[3] - offset);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}


Distortion RdCost::xGetMRSAD8( const DistParam& rcDtParam )
{
  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        subShift   = rcDtParam.subShift;
  int        subStep    = (1 << subShift);
  ptrdiff_t  strideCur  = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride * subStep;

  int32_t deltaSum = 0;
  for (int r = rows; r != 0; r -= subStep, piOrg += strideOrg, piCur += strideCur)
  {
    deltaSum += ( piOrg[0] - piCur[0] );
    deltaSum += ( piOrg[1] - piCur[1] );
    deltaSum += ( piOrg[2] - piCur[2] );
    deltaSum += ( piOrg[3] - piCur[3] );
    deltaSum += ( piOrg[4] - piCur[4] );
    deltaSum += ( piOrg[5] - piCur[5] );
    deltaSum += ( piOrg[6] - piCur[6] );
    deltaSum += ( piOrg[7] - piCur[7] );
  }

  const Pel offset  = Pel(deltaSum / (8 * (rows >> subShift)));
  piOrg             = rcDtParam.org.buf;
  piCur             = rcDtParam.cur.buf;
  Distortion sum    = 0;
  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0] - offset);
    sum += abs(piOrg[1] - piCur[1] - offset);
    sum += abs(piOrg[2] - piCur[2] - offset);
    sum += abs(piOrg[3] - piCur[3] - offset);
    sum += abs(piOrg[4] - piCur[4] - offset);
    sum += abs(piOrg[5] - piCur[5] - offset);
    sum += abs(piOrg[6] - piCur[6] - offset);
    sum += abs(piOrg[7] - piCur[7] - offset);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetMRSAD16( const DistParam& rcDtParam )
{
  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        subShift   = rcDtParam.subShift;
  int        subStep    = (1 << subShift);
  ptrdiff_t  strideCur  = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride * subStep;

  int32_t deltaSum = 0;
  for (int r = rows; r != 0; r -= subStep, piOrg += strideOrg, piCur += strideCur)
  {
    deltaSum += ( piOrg[ 0] - piCur[ 0] );
    deltaSum += ( piOrg[ 1] - piCur[ 1] );
    deltaSum += ( piOrg[ 2] - piCur[ 2] );
    deltaSum += ( piOrg[ 3] - piCur[ 3] );
    deltaSum += ( piOrg[ 4] - piCur[ 4] );
    deltaSum += ( piOrg[ 5] - piCur[ 5] );
    deltaSum += ( piOrg[ 6] - piCur[ 6] );
    deltaSum += ( piOrg[ 7] - piCur[ 7] );
    deltaSum += ( piOrg[ 8] - piCur[ 8] );
    deltaSum += ( piOrg[ 9] - piCur[ 9] );
    deltaSum += ( piOrg[10] - piCur[10] );
    deltaSum += ( piOrg[11] - piCur[11] );
    deltaSum += ( piOrg[12] - piCur[12] );
    deltaSum += ( piOrg[13] - piCur[13] );
    deltaSum += ( piOrg[14] - piCur[14] );
    deltaSum += ( piOrg[15] - piCur[15] );
  }

  const Pel offset  = Pel(deltaSum / (16 * (rows >> subShift)));
  piOrg             = rcDtParam.org.buf;
  piCur             = rcDtParam.cur.buf;
  Distortion sum    = 0;
  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0] - offset);
    sum += abs(piOrg[1] - piCur[1] - offset);
    sum += abs(piOrg[2] - piCur[2] - offset);
    sum += abs(piOrg[3] - piCur[3] - offset);
    sum += abs(piOrg[4] - piCur[4] - offset);
    sum += abs(piOrg[5] - piCur[5] - offset);
    sum += abs(piOrg[6] - piCur[6] - offset);
    sum += abs(piOrg[7] - piCur[7] - offset);
    sum += abs(piOrg[8] - piCur[8] - offset);
    sum += abs(piOrg[9] - piCur[9] - offset);
    sum += abs(piOrg[10] - piCur[10] - offset);
    sum += abs(piOrg[11] - piCur[11] - offset);
    sum += abs(piOrg[12] - piCur[12] - offset);
    sum += abs(piOrg[13] - piCur[13] - offset);
    sum += abs(piOrg[14] - piCur[14] - offset);
    sum += abs(piOrg[15] - piCur[15] - offset);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetMRSAD12( const DistParam& rcDtParam )
{
  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        subShift   = rcDtParam.subShift;
  int        subStep    = (1 << subShift);
  ptrdiff_t  strideCur  = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride * subStep;

  int32_t deltaSum = 0;
  for (int r = rows; r != 0; r -= subStep, piOrg += strideOrg, piCur += strideCur)
  {
    deltaSum += ( piOrg[ 0] - piCur[ 0] );
    deltaSum += ( piOrg[ 1] - piCur[ 1] );
    deltaSum += ( piOrg[ 2] - piCur[ 2] );
    deltaSum += ( piOrg[ 3] - piCur[ 3] );
    deltaSum += ( piOrg[ 4] - piCur[ 4] );
    deltaSum += ( piOrg[ 5] - piCur[ 5] );
    deltaSum += ( piOrg[ 6] - piCur[ 6] );
    deltaSum += ( piOrg[ 7] - piCur[ 7] );
    deltaSum += ( piOrg[ 8] - piCur[ 8] );
    deltaSum += ( piOrg[ 9] - piCur[ 9] );
    deltaSum += ( piOrg[10] - piCur[10] );
    deltaSum += ( piOrg[11] - piCur[11] );
  }

  const Pel offset  = Pel(deltaSum / (12 * (rows >> subShift)));
  piOrg             = rcDtParam.org.buf;
  piCur             = rcDtParam.cur.buf;
  Distortion sum    = 0;
  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0] - offset);
    sum += abs(piOrg[1] - piCur[1] - offset);
    sum += abs(piOrg[2] - piCur[2] - offset);
    sum += abs(piOrg[3] - piCur[3] - offset);
    sum += abs(piOrg[4] - piCur[4] - offset);
    sum += abs(piOrg[5] - piCur[5] - offset);
    sum += abs(piOrg[6] - piCur[6] - offset);
    sum += abs(piOrg[7] - piCur[7] - offset);
    sum += abs(piOrg[8] - piCur[8] - offset);
    sum += abs(piOrg[9] - piCur[9] - offset);
    sum += abs(piOrg[10] - piCur[10] - offset);
    sum += abs(piOrg[11] - piCur[11] - offset);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetMRSAD16N( const DistParam &rcDtParam )
{
  const Pel* piOrg  = rcDtParam.org.buf;
  const Pel* piCur  = rcDtParam.cur.buf;
  int        rows      = rcDtParam.org.height;
  int        cols      = rcDtParam.org.width;
  int        subShift  = rcDtParam.subShift;
  int        subStep   = (1 << subShift);
  ptrdiff_t  strideCur = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg = rcDtParam.org.stride * subStep;

  int32_t deltaSum = 0;
  for (int r = rows; r != 0; r -= subStep, piOrg += strideOrg, piCur += strideCur)
  {
    for (int n = 0; n < cols; n += 16)
    {
      deltaSum += ( piOrg[n+ 0] - piCur[n+ 0] );
      deltaSum += ( piOrg[n+ 1] - piCur[n+ 1] );
      deltaSum += ( piOrg[n+ 2] - piCur[n+ 2] );
      deltaSum += ( piOrg[n+ 3] - piCur[n+ 3] );
      deltaSum += ( piOrg[n+ 4] - piCur[n+ 4] );
      deltaSum += ( piOrg[n+ 5] - piCur[n+ 5] );
      deltaSum += ( piOrg[n+ 6] - piCur[n+ 6] );
      deltaSum += ( piOrg[n+ 7] - piCur[n+ 7] );
      deltaSum += ( piOrg[n+ 8] - piCur[n+ 8] );
      deltaSum += ( piOrg[n+ 9] - piCur[n+ 9] );
      deltaSum += ( piOrg[n+10] - piCur[n+10] );
      deltaSum += ( piOrg[n+11] - piCur[n+11] );
      deltaSum += ( piOrg[n+12] - piCur[n+12] );
      deltaSum += ( piOrg[n+13] - piCur[n+13] );
      deltaSum += ( piOrg[n+14] - piCur[n+14] );
      deltaSum += ( piOrg[n+15] - piCur[n+15] );
    }
  }

  const Pel offset  = Pel(deltaSum / (cols * (rows >> subShift)));
  piOrg             = rcDtParam.org.buf;
  piCur             = rcDtParam.cur.buf;
  Distortion sum    = 0;
  for (; rows != 0; rows -= subStep)
  {
    for (int n = 0; n < cols; n += 16)
    {
      sum += abs(piOrg[n + 0] - piCur[n + 0] - offset);
      sum += abs(piOrg[n + 1] - piCur[n + 1] - offset);
      sum += abs(piOrg[n + 2] - piCur[n + 2] - offset);
      sum += abs(piOrg[n + 3] - piCur[n + 3] - offset);
      sum += abs(piOrg[n + 4] - piCur[n + 4] - offset);
      sum += abs(piOrg[n + 5] - piCur[n + 5] - offset);
      sum += abs(piOrg[n + 6] - piCur[n + 6] - offset);
      sum += abs(piOrg[n + 7] - piCur[n + 7] - offset);
      sum += abs(piOrg[n + 8] - piCur[n + 8] - offset);
      sum += abs(piOrg[n + 9] - piCur[n + 9] - offset);
      sum += abs(piOrg[n + 10] - piCur[n + 10] - offset);
      sum += abs(piOrg[n + 11] - piCur[n + 11] - offset);
      sum += abs(piOrg[n + 12] - piCur[n + 12] - offset);
      sum += abs(piOrg[n + 13] - piCur[n + 13] - offset);
      sum += abs(piOrg[n + 14] - piCur[n + 14] - offset);
      sum += abs(piOrg[n + 15] - piCur[n + 15] - offset);
    }
    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetMRSAD32( const DistParam &rcDtParam )
{
  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        subShift   = rcDtParam.subShift;
  int        subStep    = (1 << subShift);
  ptrdiff_t  strideCur  = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride * subStep;

  int32_t deltaSum = 0;
  for (int r = rows; r != 0; r -= subStep, piOrg += strideOrg, piCur += strideCur)
  {
    deltaSum += ( piOrg[ 0] - piCur[ 0] );
    deltaSum += ( piOrg[ 1] - piCur[ 1] );
    deltaSum += ( piOrg[ 2] - piCur[ 2] );
    deltaSum += ( piOrg[ 3] - piCur[ 3] );
    deltaSum += ( piOrg[ 4] - piCur[ 4] );
    deltaSum += ( piOrg[ 5] - piCur[ 5] );
    deltaSum += ( piOrg[ 6] - piCur[ 6] );
    deltaSum += ( piOrg[ 7] - piCur[ 7] );
    deltaSum += ( piOrg[ 8] - piCur[ 8] );
    deltaSum += ( piOrg[ 9] - piCur[ 9] );
    deltaSum += ( piOrg[10] - piCur[10] );
    deltaSum += ( piOrg[11] - piCur[11] );
    deltaSum += ( piOrg[12] - piCur[12] );
    deltaSum += ( piOrg[13] - piCur[13] );
    deltaSum += ( piOrg[14] - piCur[14] );
    deltaSum += ( piOrg[15] - piCur[15] );
    deltaSum += ( piOrg[16] - piCur[16] );
    deltaSum += ( piOrg[17] - piCur[17] );
    deltaSum += ( piOrg[18] - piCur[18] );
    deltaSum += ( piOrg[19] - piCur[19] );
    deltaSum += ( piOrg[20] - piCur[20] );
    deltaSum += ( piOrg[21] - piCur[21] );
    deltaSum += ( piOrg[22] - piCur[22] );
    deltaSum += ( piOrg[23] - piCur[23] );
    deltaSum += ( piOrg[24] - piCur[24] );
    deltaSum += ( piOrg[25] - piCur[25] );
    deltaSum += ( piOrg[26] - piCur[26] );
    deltaSum += ( piOrg[27] - piCur[27] );
    deltaSum += ( piOrg[28] - piCur[28] );
    deltaSum += ( piOrg[29] - piCur[29] );
    deltaSum += ( piOrg[30] - piCur[30] );
    deltaSum += ( piOrg[31] - piCur[31] );
  }

  const Pel offset  = Pel(deltaSum / (32 * (rows >> subShift)));
  piOrg             = rcDtParam.org.buf;
  piCur             = rcDtParam.cur.buf;
  Distortion sum    = 0;
  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0] - offset);
    sum += abs(piOrg[1] - piCur[1] - offset);
    sum += abs(piOrg[2] - piCur[2] - offset);
    sum += abs(piOrg[3] - piCur[3] - offset);
    sum += abs(piOrg[4] - piCur[4] - offset);
    sum += abs(piOrg[5] - piCur[5] - offset);
    sum += abs(piOrg[6] - piCur[6] - offset);
    sum += abs(piOrg[7] - piCur[7] - offset);
    sum += abs(piOrg[8] - piCur[8] - offset);
    sum += abs(piOrg[9] - piCur[9] - offset);
    sum += abs(piOrg[10] - piCur[10] - offset);
    sum += abs(piOrg[11] - piCur[11] - offset);
    sum += abs(piOrg[12] - piCur[12] - offset);
    sum += abs(piOrg[13] - piCur[13] - offset);
    sum += abs(piOrg[14] - piCur[14] - offset);
    sum += abs(piOrg[15] - piCur[15] - offset);
    sum += abs(piOrg[16] - piCur[16] - offset);
    sum += abs(piOrg[17] - piCur[17] - offset);
    sum += abs(piOrg[18] - piCur[18] - offset);
    sum += abs(piOrg[19] - piCur[19] - offset);
    sum += abs(piOrg[20] - piCur[20] - offset);
    sum += abs(piOrg[21] - piCur[21] - offset);
    sum += abs(piOrg[22] - piCur[22] - offset);
    sum += abs(piOrg[23] - piCur[23] - offset);
    sum += abs(piOrg[24] - piCur[24] - offset);
    sum += abs(piOrg[25] - piCur[25] - offset);
    sum += abs(piOrg[26] - piCur[26] - offset);
    sum += abs(piOrg[27] - piCur[27] - offset);
    sum += abs(piOrg[28] - piCur[28] - offset);
    sum += abs(piOrg[29] - piCur[29] - offset);
    sum += abs(piOrg[30] - piCur[30] - offset);
    sum += abs(piOrg[31] - piCur[31] - offset);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetMRSAD24( const DistParam &rcDtParam )
{
  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        subShift   = rcDtParam.subShift;
  int        subStep    = (1 << subShift);
  ptrdiff_t  strideCur  = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride * subStep;

  int32_t deltaSum = 0;
  for (int r = rows; r != 0; r -= subStep, piOrg += strideOrg, piCur += strideCur)
  {
    deltaSum += ( piOrg[ 0] - piCur[ 0] );
    deltaSum += ( piOrg[ 1] - piCur[ 1] );
    deltaSum += ( piOrg[ 2] - piCur[ 2] );
    deltaSum += ( piOrg[ 3] - piCur[ 3] );
    deltaSum += ( piOrg[ 4] - piCur[ 4] );
    deltaSum += ( piOrg[ 5] - piCur[ 5] );
    deltaSum += ( piOrg[ 6] - piCur[ 6] );
    deltaSum += ( piOrg[ 7] - piCur[ 7] );
    deltaSum += ( piOrg[ 8] - piCur[ 8] );
    deltaSum += ( piOrg[ 9] - piCur[ 9] );
    deltaSum += ( piOrg[10] - piCur[10] );
    deltaSum += ( piOrg[11] - piCur[11] );
    deltaSum += ( piOrg[12] - piCur[12] );
    deltaSum += ( piOrg[13] - piCur[13] );
    deltaSum += ( piOrg[14] - piCur[14] );
    deltaSum += ( piOrg[15] - piCur[15] );
    deltaSum += ( piOrg[16] - piCur[16] );
    deltaSum += ( piOrg[17] - piCur[17] );
    deltaSum += ( piOrg[18] - piCur[18] );
    deltaSum += ( piOrg[19] - piCur[19] );
    deltaSum += ( piOrg[20] - piCur[20] );
    deltaSum += ( piOrg[21] - piCur[21] );
    deltaSum += ( piOrg[22] - piCur[22] );
    deltaSum += ( piOrg[23] - piCur[23] );
  }

  const Pel offset  = Pel(deltaSum / (24 * (rows >> subShift)));
  piOrg             = rcDtParam.org.buf;
  piCur             = rcDtParam.cur.buf;
  Distortion sum    = 0;
  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0] - offset);
    sum += abs(piOrg[1] - piCur[1] - offset);
    sum += abs(piOrg[2] - piCur[2] - offset);
    sum += abs(piOrg[3] - piCur[3] - offset);
    sum += abs(piOrg[4] - piCur[4] - offset);
    sum += abs(piOrg[5] - piCur[5] - offset);
    sum += abs(piOrg[6] - piCur[6] - offset);
    sum += abs(piOrg[7] - piCur[7] - offset);
    sum += abs(piOrg[8] - piCur[8] - offset);
    sum += abs(piOrg[9] - piCur[9] - offset);
    sum += abs(piOrg[10] - piCur[10] - offset);
    sum += abs(piOrg[11] - piCur[11] - offset);
    sum += abs(piOrg[12] - piCur[12] - offset);
    sum += abs(piOrg[13] - piCur[13] - offset);
    sum += abs(piOrg[14] - piCur[14] - offset);
    sum += abs(piOrg[15] - piCur[15] - offset);
    sum += abs(piOrg[16] - piCur[16] - offset);
    sum += abs(piOrg[17] - piCur[17] - offset);
    sum += abs(piOrg[18] - piCur[18] - offset);
    sum += abs(piOrg[19] - piCur[19] - offset);
    sum += abs(piOrg[20] - piCur[20] - offset);
    sum += abs(piOrg[21] - piCur[21] - offset);
    sum += abs(piOrg[22] - piCur[22] - offset);
    sum += abs(piOrg[23] - piCur[23] - offset);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetMRSAD64( const DistParam &rcDtParam )
{
  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        subShift   = rcDtParam.subShift;
  int        subStep    = (1 << subShift);
  ptrdiff_t  strideCur  = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride * subStep;

  int32_t deltaSum = 0;
  for (int r = rows; r != 0; r -= subStep, piOrg += strideOrg, piCur += strideCur)
  {
    deltaSum += ( piOrg[ 0] - piCur[ 0] );
    deltaSum += ( piOrg[ 1] - piCur[ 1] );
    deltaSum += ( piOrg[ 2] - piCur[ 2] );
    deltaSum += ( piOrg[ 3] - piCur[ 3] );
    deltaSum += ( piOrg[ 4] - piCur[ 4] );
    deltaSum += ( piOrg[ 5] - piCur[ 5] );
    deltaSum += ( piOrg[ 6] - piCur[ 6] );
    deltaSum += ( piOrg[ 7] - piCur[ 7] );
    deltaSum += ( piOrg[ 8] - piCur[ 8] );
    deltaSum += ( piOrg[ 9] - piCur[ 9] );
    deltaSum += ( piOrg[10] - piCur[10] );
    deltaSum += ( piOrg[11] - piCur[11] );
    deltaSum += ( piOrg[12] - piCur[12] );
    deltaSum += ( piOrg[13] - piCur[13] );
    deltaSum += ( piOrg[14] - piCur[14] );
    deltaSum += ( piOrg[15] - piCur[15] );
    deltaSum += ( piOrg[16] - piCur[16] );
    deltaSum += ( piOrg[17] - piCur[17] );
    deltaSum += ( piOrg[18] - piCur[18] );
    deltaSum += ( piOrg[19] - piCur[19] );
    deltaSum += ( piOrg[20] - piCur[20] );
    deltaSum += ( piOrg[21] - piCur[21] );
    deltaSum += ( piOrg[22] - piCur[22] );
    deltaSum += ( piOrg[23] - piCur[23] );
    deltaSum += ( piOrg[24] - piCur[24] );
    deltaSum += ( piOrg[25] - piCur[25] );
    deltaSum += ( piOrg[26] - piCur[26] );
    deltaSum += ( piOrg[27] - piCur[27] );
    deltaSum += ( piOrg[28] - piCur[28] );
    deltaSum += ( piOrg[29] - piCur[29] );
    deltaSum += ( piOrg[30] - piCur[30] );
    deltaSum += ( piOrg[31] - piCur[31] );
    deltaSum += ( piOrg[32] - piCur[32] );
    deltaSum += ( piOrg[33] - piCur[33] );
    deltaSum += ( piOrg[34] - piCur[34] );
    deltaSum += ( piOrg[35] - piCur[35] );
    deltaSum += ( piOrg[36] - piCur[36] );
    deltaSum += ( piOrg[37] - piCur[37] );
    deltaSum += ( piOrg[38] - piCur[38] );
    deltaSum += ( piOrg[39] - piCur[39] );
    deltaSum += ( piOrg[40] - piCur[40] );
    deltaSum += ( piOrg[41] - piCur[41] );
    deltaSum += ( piOrg[42] - piCur[42] );
    deltaSum += ( piOrg[43] - piCur[43] );
    deltaSum += ( piOrg[44] - piCur[44] );
    deltaSum += ( piOrg[45] - piCur[45] );
    deltaSum += ( piOrg[46] - piCur[46] );
    deltaSum += ( piOrg[47] - piCur[47] );
    deltaSum += ( piOrg[48] - piCur[48] );
    deltaSum += ( piOrg[49] - piCur[49] );
    deltaSum += ( piOrg[50] - piCur[50] );
    deltaSum += ( piOrg[51] - piCur[51] );
    deltaSum += ( piOrg[52] - piCur[52] );
    deltaSum += ( piOrg[53] - piCur[53] );
    deltaSum += ( piOrg[54] - piCur[54] );
    deltaSum += ( piOrg[55] - piCur[55] );
    deltaSum += ( piOrg[56] - piCur[56] );
    deltaSum += ( piOrg[57] - piCur[57] );
    deltaSum += ( piOrg[58] - piCur[58] );
    deltaSum += ( piOrg[59] - piCur[59] );
    deltaSum += ( piOrg[60] - piCur[60] );
    deltaSum += ( piOrg[61] - piCur[61] );
    deltaSum += ( piOrg[62] - piCur[62] );
    deltaSum += ( piOrg[63] - piCur[63] );
  }

  const Pel offset  = Pel(deltaSum / (64 * (rows >> subShift)));
  piOrg             = rcDtParam.org.buf;
  piCur             = rcDtParam.cur.buf;
  Distortion sum    = 0;
  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0] - offset);
    sum += abs(piOrg[1] - piCur[1] - offset);
    sum += abs(piOrg[2] - piCur[2] - offset);
    sum += abs(piOrg[3] - piCur[3] - offset);
    sum += abs(piOrg[4] - piCur[4] - offset);
    sum += abs(piOrg[5] - piCur[5] - offset);
    sum += abs(piOrg[6] - piCur[6] - offset);
    sum += abs(piOrg[7] - piCur[7] - offset);
    sum += abs(piOrg[8] - piCur[8] - offset);
    sum += abs(piOrg[9] - piCur[9] - offset);
    sum += abs(piOrg[10] - piCur[10] - offset);
    sum += abs(piOrg[11] - piCur[11] - offset);
    sum += abs(piOrg[12] - piCur[12] - offset);
    sum += abs(piOrg[13] - piCur[13] - offset);
    sum += abs(piOrg[14] - piCur[14] - offset);
    sum += abs(piOrg[15] - piCur[15] - offset);
    sum += abs(piOrg[16] - piCur[16] - offset);
    sum += abs(piOrg[17] - piCur[17] - offset);
    sum += abs(piOrg[18] - piCur[18] - offset);
    sum += abs(piOrg[19] - piCur[19] - offset);
    sum += abs(piOrg[20] - piCur[20] - offset);
    sum += abs(piOrg[21] - piCur[21] - offset);
    sum += abs(piOrg[22] - piCur[22] - offset);
    sum += abs(piOrg[23] - piCur[23] - offset);
    sum += abs(piOrg[24] - piCur[24] - offset);
    sum += abs(piOrg[25] - piCur[25] - offset);
    sum += abs(piOrg[26] - piCur[26] - offset);
    sum += abs(piOrg[27] - piCur[27] - offset);
    sum += abs(piOrg[28] - piCur[28] - offset);
    sum += abs(piOrg[29] - piCur[29] - offset);
    sum += abs(piOrg[30] - piCur[30] - offset);
    sum += abs(piOrg[31] - piCur[31] - offset);
    sum += abs(piOrg[32] - piCur[32] - offset);
    sum += abs(piOrg[33] - piCur[33] - offset);
    sum += abs(piOrg[34] - piCur[34] - offset);
    sum += abs(piOrg[35] - piCur[35] - offset);
    sum += abs(piOrg[36] - piCur[36] - offset);
    sum += abs(piOrg[37] - piCur[37] - offset);
    sum += abs(piOrg[38] - piCur[38] - offset);
    sum += abs(piOrg[39] - piCur[39] - offset);
    sum += abs(piOrg[40] - piCur[40] - offset);
    sum += abs(piOrg[41] - piCur[41] - offset);
    sum += abs(piOrg[42] - piCur[42] - offset);
    sum += abs(piOrg[43] - piCur[43] - offset);
    sum += abs(piOrg[44] - piCur[44] - offset);
    sum += abs(piOrg[45] - piCur[45] - offset);
    sum += abs(piOrg[46] - piCur[46] - offset);
    sum += abs(piOrg[47] - piCur[47] - offset);
    sum += abs(piOrg[48] - piCur[48] - offset);
    sum += abs(piOrg[49] - piCur[49] - offset);
    sum += abs(piOrg[50] - piCur[50] - offset);
    sum += abs(piOrg[51] - piCur[51] - offset);
    sum += abs(piOrg[52] - piCur[52] - offset);
    sum += abs(piOrg[53] - piCur[53] - offset);
    sum += abs(piOrg[54] - piCur[54] - offset);
    sum += abs(piOrg[55] - piCur[55] - offset);
    sum += abs(piOrg[56] - piCur[56] - offset);
    sum += abs(piOrg[57] - piCur[57] - offset);
    sum += abs(piOrg[58] - piCur[58] - offset);
    sum += abs(piOrg[59] - piCur[59] - offset);
    sum += abs(piOrg[60] - piCur[60] - offset);
    sum += abs(piOrg[61] - piCur[61] - offset);
    sum += abs(piOrg[62] - piCur[62] - offset);
    sum += abs(piOrg[63] - piCur[63] - offset);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

Distortion RdCost::xGetMRSAD48( const DistParam &rcDtParam )
{
  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        subShift   = rcDtParam.subShift;
  int        subStep    = (1 << subShift);
  ptrdiff_t  strideCur  = rcDtParam.cur.stride * subStep;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride * subStep;

  int32_t deltaSum = 0;
  for (int r = rows; r != 0; r -= subStep, piOrg += strideOrg, piCur += strideCur)
  {
    deltaSum += ( piOrg[ 0] - piCur[ 0] );
    deltaSum += ( piOrg[ 1] - piCur[ 1] );
    deltaSum += ( piOrg[ 2] - piCur[ 2] );
    deltaSum += ( piOrg[ 3] - piCur[ 3] );
    deltaSum += ( piOrg[ 4] - piCur[ 4] );
    deltaSum += ( piOrg[ 5] - piCur[ 5] );
    deltaSum += ( piOrg[ 6] - piCur[ 6] );
    deltaSum += ( piOrg[ 7] - piCur[ 7] );
    deltaSum += ( piOrg[ 8] - piCur[ 8] );
    deltaSum += ( piOrg[ 9] - piCur[ 9] );
    deltaSum += ( piOrg[10] - piCur[10] );
    deltaSum += ( piOrg[11] - piCur[11] );
    deltaSum += ( piOrg[12] - piCur[12] );
    deltaSum += ( piOrg[13] - piCur[13] );
    deltaSum += ( piOrg[14] - piCur[14] );
    deltaSum += ( piOrg[15] - piCur[15] );
    deltaSum += ( piOrg[16] - piCur[16] );
    deltaSum += ( piOrg[17] - piCur[17] );
    deltaSum += ( piOrg[18] - piCur[18] );
    deltaSum += ( piOrg[19] - piCur[19] );
    deltaSum += ( piOrg[20] - piCur[20] );
    deltaSum += ( piOrg[21] - piCur[21] );
    deltaSum += ( piOrg[22] - piCur[22] );
    deltaSum += ( piOrg[23] - piCur[23] );
    deltaSum += ( piOrg[24] - piCur[24] );
    deltaSum += ( piOrg[25] - piCur[25] );
    deltaSum += ( piOrg[26] - piCur[26] );
    deltaSum += ( piOrg[27] - piCur[27] );
    deltaSum += ( piOrg[28] - piCur[28] );
    deltaSum += ( piOrg[29] - piCur[29] );
    deltaSum += ( piOrg[30] - piCur[30] );
    deltaSum += ( piOrg[31] - piCur[31] );
    deltaSum += ( piOrg[32] - piCur[32] );
    deltaSum += ( piOrg[33] - piCur[33] );
    deltaSum += ( piOrg[34] - piCur[34] );
    deltaSum += ( piOrg[35] - piCur[35] );
    deltaSum += ( piOrg[36] - piCur[36] );
    deltaSum += ( piOrg[37] - piCur[37] );
    deltaSum += ( piOrg[38] - piCur[38] );
    deltaSum += ( piOrg[39] - piCur[39] );
    deltaSum += ( piOrg[40] - piCur[40] );
    deltaSum += ( piOrg[41] - piCur[41] );
    deltaSum += ( piOrg[42] - piCur[42] );
    deltaSum += ( piOrg[43] - piCur[43] );
    deltaSum += ( piOrg[44] - piCur[44] );
    deltaSum += ( piOrg[45] - piCur[45] );
    deltaSum += ( piOrg[46] - piCur[46] );
    deltaSum += ( piOrg[47] - piCur[47] );
  }

  const Pel offset  = Pel(deltaSum / (48 * (rows >> subShift)));
  piOrg             = rcDtParam.org.buf;
  piCur             = rcDtParam.cur.buf;
  Distortion sum    = 0;
  for (; rows != 0; rows -= subStep)
  {
    sum += abs(piOrg[0] - piCur[0] - offset);
    sum += abs(piOrg[1] - piCur[1] - offset);
    sum += abs(piOrg[2] - piCur[2] - offset);
    sum += abs(piOrg[3] - piCur[3] - offset);
    sum += abs(piOrg[4] - piCur[4] - offset);
    sum += abs(piOrg[5] - piCur[5] - offset);
    sum += abs(piOrg[6] - piCur[6] - offset);
    sum += abs(piOrg[7] - piCur[7] - offset);
    sum += abs(piOrg[8] - piCur[8] - offset);
    sum += abs(piOrg[9] - piCur[9] - offset);
    sum += abs(piOrg[10] - piCur[10] - offset);
    sum += abs(piOrg[11] - piCur[11] - offset);
    sum += abs(piOrg[12] - piCur[12] - offset);
    sum += abs(piOrg[13] - piCur[13] - offset);
    sum += abs(piOrg[14] - piCur[14] - offset);
    sum += abs(piOrg[15] - piCur[15] - offset);
    sum += abs(piOrg[16] - piCur[16] - offset);
    sum += abs(piOrg[17] - piCur[17] - offset);
    sum += abs(piOrg[18] - piCur[18] - offset);
    sum += abs(piOrg[19] - piCur[19] - offset);
    sum += abs(piOrg[20] - piCur[20] - offset);
    sum += abs(piOrg[21] - piCur[21] - offset);
    sum += abs(piOrg[22] - piCur[22] - offset);
    sum += abs(piOrg[23] - piCur[23] - offset);
    sum += abs(piOrg[24] - piCur[24] - offset);
    sum += abs(piOrg[25] - piCur[25] - offset);
    sum += abs(piOrg[26] - piCur[26] - offset);
    sum += abs(piOrg[27] - piCur[27] - offset);
    sum += abs(piOrg[28] - piCur[28] - offset);
    sum += abs(piOrg[29] - piCur[29] - offset);
    sum += abs(piOrg[30] - piCur[30] - offset);
    sum += abs(piOrg[31] - piCur[31] - offset);
    sum += abs(piOrg[32] - piCur[32] - offset);
    sum += abs(piOrg[33] - piCur[33] - offset);
    sum += abs(piOrg[34] - piCur[34] - offset);
    sum += abs(piOrg[35] - piCur[35] - offset);
    sum += abs(piOrg[36] - piCur[36] - offset);
    sum += abs(piOrg[37] - piCur[37] - offset);
    sum += abs(piOrg[38] - piCur[38] - offset);
    sum += abs(piOrg[39] - piCur[39] - offset);
    sum += abs(piOrg[40] - piCur[40] - offset);
    sum += abs(piOrg[41] - piCur[41] - offset);
    sum += abs(piOrg[42] - piCur[42] - offset);
    sum += abs(piOrg[43] - piCur[43] - offset);
    sum += abs(piOrg[44] - piCur[44] - offset);
    sum += abs(piOrg[45] - piCur[45] - offset);
    sum += abs(piOrg[46] - piCur[46] - offset);
    sum += abs(piOrg[47] - piCur[47] - offset);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  sum <<= subShift;
  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}

// --------------------------------------------------------------------------------------------------------------------
// SSE
// --------------------------------------------------------------------------------------------------------------------

Distortion RdCost::xGetSSE( const DistParam &rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }

  const Pel* piOrg      = rcDtParam.org.buf;
  const Pel* piCur      = rcDtParam.cur.buf;
  int        rows       = rcDtParam.org.height;
  int        cols       = rcDtParam.org.width;
  ptrdiff_t  strideCur  = rcDtParam.cur.stride;
  ptrdiff_t  strideOrg  = rcDtParam.org.stride;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;

  Intermediate_Int temp;

  for (; rows != 0; rows--)
  {
    for (int n = 0; n < cols; n++)
    {
      temp = piOrg[n] - piCur[n];
      sum += Distortion((temp * temp) >> shift);
    }
    piOrg += strideOrg;
    piCur += strideCur;
  }

  return (sum);
}

Distortion RdCost::xGetSSE4( const DistParam &rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    CHECK( rcDtParam.org.width != 4, "Invalid size" );
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }

  const Pel* piOrg   = rcDtParam.org.buf;
  const Pel* piCur   = rcDtParam.cur.buf;
  int        rows      = rcDtParam.org.height;
  ptrdiff_t  strideOrg = rcDtParam.org.stride;
  ptrdiff_t  strideCur = rcDtParam.cur.stride;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;

  Intermediate_Int temp;

  for (; rows != 0; rows--)
  {
    temp = piOrg[0] - piCur[0];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[1] - piCur[1];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[2] - piCur[2];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[3] - piCur[3];
    sum += Distortion((temp * temp) >> shift);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  return (sum);
}

Distortion RdCost::xGetSSE8( const DistParam &rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    CHECK( rcDtParam.org.width != 8, "Invalid size" );
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }

  const Pel* piOrg   = rcDtParam.org.buf;
  const Pel* piCur   = rcDtParam.cur.buf;
  int        rows      = rcDtParam.org.height;
  ptrdiff_t  strideOrg = rcDtParam.org.stride;
  ptrdiff_t  strideCur = rcDtParam.cur.stride;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;

  Intermediate_Int temp;

  for (; rows != 0; rows--)
  {
    temp = piOrg[0] - piCur[0];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[1] - piCur[1];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[2] - piCur[2];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[3] - piCur[3];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[4] - piCur[4];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[5] - piCur[5];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[6] - piCur[6];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[7] - piCur[7];
    sum += Distortion((temp * temp) >> shift);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  return (sum);
}

Distortion RdCost::xGetSSE16( const DistParam &rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    CHECK( rcDtParam.org.width != 16, "Invalid size" );
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }

  const Pel* piOrg   = rcDtParam.org.buf;
  const Pel* piCur   = rcDtParam.cur.buf;
  int        rows      = rcDtParam.org.height;
  ptrdiff_t  strideOrg = rcDtParam.org.stride;
  ptrdiff_t  strideCur = rcDtParam.cur.stride;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;

  Intermediate_Int temp;

  for (; rows != 0; rows--)
  {
    temp = piOrg[0] - piCur[0];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[1] - piCur[1];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[2] - piCur[2];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[3] - piCur[3];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[4] - piCur[4];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[5] - piCur[5];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[6] - piCur[6];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[7] - piCur[7];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[8] - piCur[8];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[9] - piCur[9];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[10] - piCur[10];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[11] - piCur[11];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[12] - piCur[12];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[13] - piCur[13];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[14] - piCur[14];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[15] - piCur[15];
    sum += Distortion((temp * temp) >> shift);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  return (sum);
}

Distortion RdCost::xGetSSE16N( const DistParam &rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }
  const Pel* piOrg   = rcDtParam.org.buf;
  const Pel* piCur   = rcDtParam.cur.buf;
  int        rows      = rcDtParam.org.height;
  int        cols      = rcDtParam.org.width;
  ptrdiff_t  strideOrg = rcDtParam.org.stride;
  ptrdiff_t  strideCur = rcDtParam.cur.stride;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;

  Intermediate_Int temp;

  for (; rows != 0; rows--)
  {
    for (int n = 0; n < cols; n += 16)
    {
      temp = piOrg[n + 0] - piCur[n + 0];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 1] - piCur[n + 1];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 2] - piCur[n + 2];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 3] - piCur[n + 3];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 4] - piCur[n + 4];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 5] - piCur[n + 5];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 6] - piCur[n + 6];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 7] - piCur[n + 7];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 8] - piCur[n + 8];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 9] - piCur[n + 9];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 10] - piCur[n + 10];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 11] - piCur[n + 11];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 12] - piCur[n + 12];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 13] - piCur[n + 13];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 14] - piCur[n + 14];
      sum += Distortion((temp * temp) >> shift);
      temp = piOrg[n + 15] - piCur[n + 15];
      sum += Distortion((temp * temp) >> shift);
    }
    piOrg += strideOrg;
    piCur += strideCur;
  }

  return (sum);
}

Distortion RdCost::xGetSSE32( const DistParam &rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    CHECK( rcDtParam.org.width != 32, "Invalid size" );
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }

  const Pel* piOrg   = rcDtParam.org.buf;
  const Pel* piCur   = rcDtParam.cur.buf;
  int        rows      = rcDtParam.org.height;
  ptrdiff_t  strideOrg = rcDtParam.org.stride;
  ptrdiff_t  strideCur = rcDtParam.cur.stride;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;

  Intermediate_Int temp;

  for (; rows != 0; rows--)
  {
    temp = piOrg[0] - piCur[0];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[1] - piCur[1];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[2] - piCur[2];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[3] - piCur[3];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[4] - piCur[4];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[5] - piCur[5];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[6] - piCur[6];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[7] - piCur[7];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[8] - piCur[8];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[9] - piCur[9];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[10] - piCur[10];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[11] - piCur[11];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[12] - piCur[12];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[13] - piCur[13];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[14] - piCur[14];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[15] - piCur[15];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[16] - piCur[16];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[17] - piCur[17];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[18] - piCur[18];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[19] - piCur[19];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[20] - piCur[20];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[21] - piCur[21];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[22] - piCur[22];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[23] - piCur[23];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[24] - piCur[24];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[25] - piCur[25];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[26] - piCur[26];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[27] - piCur[27];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[28] - piCur[28];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[29] - piCur[29];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[30] - piCur[30];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[31] - piCur[31];
    sum += Distortion((temp * temp) >> shift);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  return (sum);
}

Distortion RdCost::xGetSSE64( const DistParam &rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    CHECK( rcDtParam.org.width != 64, "Invalid size" );
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }

  const Pel* piOrg   = rcDtParam.org.buf;
  const Pel* piCur   = rcDtParam.cur.buf;
  int        rows      = rcDtParam.org.height;
  ptrdiff_t  strideOrg = rcDtParam.org.stride;
  ptrdiff_t  strideCur = rcDtParam.cur.stride;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;

  Intermediate_Int temp;

  for (; rows != 0; rows--)
  {
    temp = piOrg[0] - piCur[0];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[1] - piCur[1];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[2] - piCur[2];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[3] - piCur[3];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[4] - piCur[4];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[5] - piCur[5];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[6] - piCur[6];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[7] - piCur[7];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[8] - piCur[8];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[9] - piCur[9];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[10] - piCur[10];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[11] - piCur[11];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[12] - piCur[12];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[13] - piCur[13];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[14] - piCur[14];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[15] - piCur[15];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[16] - piCur[16];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[17] - piCur[17];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[18] - piCur[18];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[19] - piCur[19];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[20] - piCur[20];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[21] - piCur[21];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[22] - piCur[22];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[23] - piCur[23];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[24] - piCur[24];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[25] - piCur[25];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[26] - piCur[26];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[27] - piCur[27];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[28] - piCur[28];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[29] - piCur[29];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[30] - piCur[30];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[31] - piCur[31];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[32] - piCur[32];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[33] - piCur[33];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[34] - piCur[34];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[35] - piCur[35];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[36] - piCur[36];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[37] - piCur[37];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[38] - piCur[38];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[39] - piCur[39];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[40] - piCur[40];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[41] - piCur[41];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[42] - piCur[42];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[43] - piCur[43];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[44] - piCur[44];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[45] - piCur[45];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[46] - piCur[46];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[47] - piCur[47];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[48] - piCur[48];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[49] - piCur[49];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[50] - piCur[50];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[51] - piCur[51];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[52] - piCur[52];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[53] - piCur[53];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[54] - piCur[54];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[55] - piCur[55];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[56] - piCur[56];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[57] - piCur[57];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[58] - piCur[58];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[59] - piCur[59];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[60] - piCur[60];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[61] - piCur[61];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[62] - piCur[62];
    sum += Distortion((temp * temp) >> shift);
    temp = piOrg[63] - piCur[63];
    sum += Distortion((temp * temp) >> shift);

    piOrg += strideOrg;
    piCur += strideCur;
  }

  return (sum);
}

// --------------------------------------------------------------------------------------------------------------------
// HADAMARD with step (used in fractional search)
// --------------------------------------------------------------------------------------------------------------------

Distortion RdCost::xCalcHADs2x2(const Pel *piOrg, const Pel *piCur, ptrdiff_t strideOrg, ptrdiff_t strideCur, int step)
{
  Distortion satd = 0;
  TCoeff diff[4], m[4];
  CHECK(step != 1, "Invalid step");
  diff[0] = piOrg[0             ] - piCur[0];
  diff[1] = piOrg[1             ] - piCur[1];
  diff[2] = piOrg[strideOrg] - piCur[0 + strideCur];
  diff[3] = piOrg[strideOrg + 1] - piCur[1 + strideCur];
  m[0] = diff[0] + diff[2];
  m[1] = diff[1] + diff[3];
  m[2] = diff[0] - diff[2];
  m[3] = diff[1] - diff[3];

#if JVET_R0164_MEAN_SCALED_SATD
  satd += abs(m[0] + m[1]) >> 2;
#else
  satd += abs(m[0] + m[1]);
#endif
  satd += abs(m[0] - m[1]);
  satd += abs(m[2] + m[3]);
  satd += abs(m[2] - m[3]);

  return satd;
}

Distortion RdCost::xCalcHADs4x4(const Pel *piOrg, const Pel *piCur, ptrdiff_t strideOrg, ptrdiff_t strideCur, int step)
{
  int k;
  Distortion satd = 0;
  TCoeff diff[16], m[16], d[16];

  CHECK(step != 1, "Invalid step");
  for( k = 0; k < 16; k+=4 )
  {
    diff[k+0] = piOrg[0] - piCur[0];
    diff[k+1] = piOrg[1] - piCur[1];
    diff[k+2] = piOrg[2] - piCur[2];
    diff[k+3] = piOrg[3] - piCur[3];

    piCur += strideCur;
    piOrg += strideOrg;
  }

  /*===== hadamard transform =====*/
  m[ 0] = diff[ 0] + diff[12];
  m[ 1] = diff[ 1] + diff[13];
  m[ 2] = diff[ 2] + diff[14];
  m[ 3] = diff[ 3] + diff[15];
  m[ 4] = diff[ 4] + diff[ 8];
  m[ 5] = diff[ 5] + diff[ 9];
  m[ 6] = diff[ 6] + diff[10];
  m[ 7] = diff[ 7] + diff[11];
  m[ 8] = diff[ 4] - diff[ 8];
  m[ 9] = diff[ 5] - diff[ 9];
  m[10] = diff[ 6] - diff[10];
  m[11] = diff[ 7] - diff[11];
  m[12] = diff[ 0] - diff[12];
  m[13] = diff[ 1] - diff[13];
  m[14] = diff[ 2] - diff[14];
  m[15] = diff[ 3] - diff[15];

  d[ 0] = m[ 0] + m[ 4];
  d[ 1] = m[ 1] + m[ 5];
  d[ 2] = m[ 2] + m[ 6];
  d[ 3] = m[ 3] + m[ 7];
  d[ 4] = m[ 8] + m[12];
  d[ 5] = m[ 9] + m[13];
  d[ 6] = m[10] + m[14];
  d[ 7] = m[11] + m[15];
  d[ 8] = m[ 0] - m[ 4];
  d[ 9] = m[ 1] - m[ 5];
  d[10] = m[ 2] - m[ 6];
  d[11] = m[ 3] - m[ 7];
  d[12] = m[12] - m[ 8];
  d[13] = m[13] - m[ 9];
  d[14] = m[14] - m[10];
  d[15] = m[15] - m[11];

  m[ 0] = d[ 0] + d[ 3];
  m[ 1] = d[ 1] + d[ 2];
  m[ 2] = d[ 1] - d[ 2];
  m[ 3] = d[ 0] - d[ 3];
  m[ 4] = d[ 4] + d[ 7];
  m[ 5] = d[ 5] + d[ 6];
  m[ 6] = d[ 5] - d[ 6];
  m[ 7] = d[ 4] - d[ 7];
  m[ 8] = d[ 8] + d[11];
  m[ 9] = d[ 9] + d[10];
  m[10] = d[ 9] - d[10];
  m[11] = d[ 8] - d[11];
  m[12] = d[12] + d[15];
  m[13] = d[13] + d[14];
  m[14] = d[13] - d[14];
  m[15] = d[12] - d[15];

  d[ 0] = m[ 0] + m[ 1];
  d[ 1] = m[ 0] - m[ 1];
  d[ 2] = m[ 2] + m[ 3];
  d[ 3] = m[ 3] - m[ 2];
  d[ 4] = m[ 4] + m[ 5];
  d[ 5] = m[ 4] - m[ 5];
  d[ 6] = m[ 6] + m[ 7];
  d[ 7] = m[ 7] - m[ 6];
  d[ 8] = m[ 8] + m[ 9];
  d[ 9] = m[ 8] - m[ 9];
  d[10] = m[10] + m[11];
  d[11] = m[11] - m[10];
  d[12] = m[12] + m[13];
  d[13] = m[12] - m[13];
  d[14] = m[14] + m[15];
  d[15] = m[15] - m[14];

  for (k=0; k<16; ++k)
  {
    satd += abs(d[k]);
  }

#if JVET_R0164_MEAN_SCALED_SATD
  satd -= abs(d[0]);
  satd += abs(d[0]) >> 2;
#endif
  satd  = ((satd+1)>>1);

  return satd;
}

Distortion RdCost::xCalcHADs8x8(const Pel *piOrg, const Pel *piCur, ptrdiff_t strideOrg, ptrdiff_t strideCur, int step)
{
  int k, i, j, jj;
  Distortion sad = 0;
  TCoeff diff[64], m1[8][8], m2[8][8], m3[8][8];
  CHECK(step != 1, "Invalid step");
  for( k = 0; k < 64; k += 8 )
  {
    diff[k+0] = piOrg[0] - piCur[0];
    diff[k+1] = piOrg[1] - piCur[1];
    diff[k+2] = piOrg[2] - piCur[2];
    diff[k+3] = piOrg[3] - piCur[3];
    diff[k+4] = piOrg[4] - piCur[4];
    diff[k+5] = piOrg[5] - piCur[5];
    diff[k+6] = piOrg[6] - piCur[6];
    diff[k+7] = piOrg[7] - piCur[7];

    piCur += strideCur;
    piOrg += strideOrg;
  }

  //horizontal
  for (j=0; j < 8; j++)
  {
    jj = j << 3;
    m2[j][0] = diff[jj  ] + diff[jj+4];
    m2[j][1] = diff[jj+1] + diff[jj+5];
    m2[j][2] = diff[jj+2] + diff[jj+6];
    m2[j][3] = diff[jj+3] + diff[jj+7];
    m2[j][4] = diff[jj  ] - diff[jj+4];
    m2[j][5] = diff[jj+1] - diff[jj+5];
    m2[j][6] = diff[jj+2] - diff[jj+6];
    m2[j][7] = diff[jj+3] - diff[jj+7];

    m1[j][0] = m2[j][0] + m2[j][2];
    m1[j][1] = m2[j][1] + m2[j][3];
    m1[j][2] = m2[j][0] - m2[j][2];
    m1[j][3] = m2[j][1] - m2[j][3];
    m1[j][4] = m2[j][4] + m2[j][6];
    m1[j][5] = m2[j][5] + m2[j][7];
    m1[j][6] = m2[j][4] - m2[j][6];
    m1[j][7] = m2[j][5] - m2[j][7];

    m2[j][0] = m1[j][0] + m1[j][1];
    m2[j][1] = m1[j][0] - m1[j][1];
    m2[j][2] = m1[j][2] + m1[j][3];
    m2[j][3] = m1[j][2] - m1[j][3];
    m2[j][4] = m1[j][4] + m1[j][5];
    m2[j][5] = m1[j][4] - m1[j][5];
    m2[j][6] = m1[j][6] + m1[j][7];
    m2[j][7] = m1[j][6] - m1[j][7];
  }

  //vertical
  for (i=0; i < 8; i++)
  {
    m3[0][i] = m2[0][i] + m2[4][i];
    m3[1][i] = m2[1][i] + m2[5][i];
    m3[2][i] = m2[2][i] + m2[6][i];
    m3[3][i] = m2[3][i] + m2[7][i];
    m3[4][i] = m2[0][i] - m2[4][i];
    m3[5][i] = m2[1][i] - m2[5][i];
    m3[6][i] = m2[2][i] - m2[6][i];
    m3[7][i] = m2[3][i] - m2[7][i];

    m1[0][i] = m3[0][i] + m3[2][i];
    m1[1][i] = m3[1][i] + m3[3][i];
    m1[2][i] = m3[0][i] - m3[2][i];
    m1[3][i] = m3[1][i] - m3[3][i];
    m1[4][i] = m3[4][i] + m3[6][i];
    m1[5][i] = m3[5][i] + m3[7][i];
    m1[6][i] = m3[4][i] - m3[6][i];
    m1[7][i] = m3[5][i] - m3[7][i];

    m2[0][i] = m1[0][i] + m1[1][i];
    m2[1][i] = m1[0][i] - m1[1][i];
    m2[2][i] = m1[2][i] + m1[3][i];
    m2[3][i] = m1[2][i] - m1[3][i];
    m2[4][i] = m1[4][i] + m1[5][i];
    m2[5][i] = m1[4][i] - m1[5][i];
    m2[6][i] = m1[6][i] + m1[7][i];
    m2[7][i] = m1[6][i] - m1[7][i];
  }

  for (i = 0; i < 8; i++)
  {
    for (j = 0; j < 8; j++)
    {
      sad += abs(m2[i][j]);
    }
  }

#if JVET_R0164_MEAN_SCALED_SATD
  sad -= abs(m2[0][0]);
  sad += abs(m2[0][0]) >> 2;
#endif
  sad  = ((sad+2)>>2);

  return sad;
}

Distortion RdCost::xCalcHADs16x8(const Pel *piOrg, const Pel *piCur, ptrdiff_t strideOrg, ptrdiff_t strideCur)
{   //need to add SIMD implementation ,JCA
  int k, i, j, jj, sad = 0;
  int diff[128], m1[8][16], m2[8][16];
  for( k = 0; k < 128; k += 16 )
  {
    diff[k + 0] = piOrg[0] - piCur[0];
    diff[k + 1] = piOrg[1] - piCur[1];
    diff[k + 2] = piOrg[2] - piCur[2];
    diff[k + 3] = piOrg[3] - piCur[3];
    diff[k + 4] = piOrg[4] - piCur[4];
    diff[k + 5] = piOrg[5] - piCur[5];
    diff[k + 6] = piOrg[6] - piCur[6];
    diff[k + 7] = piOrg[7] - piCur[7];

    diff[k + 8] = piOrg[8] - piCur[8];
    diff[k + 9] = piOrg[9] - piCur[9];
    diff[k + 10] = piOrg[10] - piCur[10];
    diff[k + 11] = piOrg[11] - piCur[11];
    diff[k + 12] = piOrg[12] - piCur[12];
    diff[k + 13] = piOrg[13] - piCur[13];
    diff[k + 14] = piOrg[14] - piCur[14];
    diff[k + 15] = piOrg[15] - piCur[15];

    piCur += strideCur;
    piOrg += strideOrg;
  }

  //horizontal
  for( j = 0; j < 8; j++ )
  {
    jj = j << 4;

    m2[j][0] = diff[jj    ] + diff[jj + 8];
    m2[j][1] = diff[jj + 1] + diff[jj + 9];
    m2[j][2] = diff[jj + 2] + diff[jj + 10];
    m2[j][3] = diff[jj + 3] + diff[jj + 11];
    m2[j][4] = diff[jj + 4] + diff[jj + 12];
    m2[j][5] = diff[jj + 5] + diff[jj + 13];
    m2[j][6] = diff[jj + 6] + diff[jj + 14];
    m2[j][7] = diff[jj + 7] + diff[jj + 15];
    m2[j][8] = diff[jj    ] - diff[jj + 8];
    m2[j][9] = diff[jj + 1] - diff[jj + 9];
    m2[j][10] = diff[jj + 2] - diff[jj + 10];
    m2[j][11] = diff[jj + 3] - diff[jj + 11];
    m2[j][12] = diff[jj + 4] - diff[jj + 12];
    m2[j][13] = diff[jj + 5] - diff[jj + 13];
    m2[j][14] = diff[jj + 6] - diff[jj + 14];
    m2[j][15] = diff[jj + 7] - diff[jj + 15];

    m1[j][0] = m2[j][0] + m2[j][4];
    m1[j][1] = m2[j][1] + m2[j][5];
    m1[j][2] = m2[j][2] + m2[j][6];
    m1[j][3] = m2[j][3] + m2[j][7];
    m1[j][4] = m2[j][0] - m2[j][4];
    m1[j][5] = m2[j][1] - m2[j][5];
    m1[j][6] = m2[j][2] - m2[j][6];
    m1[j][7] = m2[j][3] - m2[j][7];
    m1[j][8] = m2[j][8] + m2[j][12];
    m1[j][9] = m2[j][9] + m2[j][13];
    m1[j][10] = m2[j][10] + m2[j][14];
    m1[j][11] = m2[j][11] + m2[j][15];
    m1[j][12] = m2[j][8] - m2[j][12];
    m1[j][13] = m2[j][9] - m2[j][13];
    m1[j][14] = m2[j][10] - m2[j][14];
    m1[j][15] = m2[j][11] - m2[j][15];

    m2[j][0] = m1[j][0] + m1[j][2];
    m2[j][1] = m1[j][1] + m1[j][3];
    m2[j][2] = m1[j][0] - m1[j][2];
    m2[j][3] = m1[j][1] - m1[j][3];
    m2[j][4] = m1[j][4] + m1[j][6];
    m2[j][5] = m1[j][5] + m1[j][7];
    m2[j][6] = m1[j][4] - m1[j][6];
    m2[j][7] = m1[j][5] - m1[j][7];
    m2[j][8] = m1[j][8] + m1[j][10];
    m2[j][9] = m1[j][9] + m1[j][11];
    m2[j][10] = m1[j][8] - m1[j][10];
    m2[j][11] = m1[j][9] - m1[j][11];
    m2[j][12] = m1[j][12] + m1[j][14];
    m2[j][13] = m1[j][13] + m1[j][15];
    m2[j][14] = m1[j][12] - m1[j][14];
    m2[j][15] = m1[j][13] - m1[j][15];

    m1[j][0] = m2[j][0] + m2[j][1];
    m1[j][1] = m2[j][0] - m2[j][1];
    m1[j][2] = m2[j][2] + m2[j][3];
    m1[j][3] = m2[j][2] - m2[j][3];
    m1[j][4] = m2[j][4] + m2[j][5];
    m1[j][5] = m2[j][4] - m2[j][5];
    m1[j][6] = m2[j][6] + m2[j][7];
    m1[j][7] = m2[j][6] - m2[j][7];
    m1[j][8] = m2[j][8] + m2[j][9];
    m1[j][9] = m2[j][8] - m2[j][9];
    m1[j][10] = m2[j][10] + m2[j][11];
    m1[j][11] = m2[j][10] - m2[j][11];
    m1[j][12] = m2[j][12] + m2[j][13];
    m1[j][13] = m2[j][12] - m2[j][13];
    m1[j][14] = m2[j][14] + m2[j][15];
    m1[j][15] = m2[j][14] - m2[j][15];
  }

  //vertical
  for( i = 0; i < 16; i++ )
  {
    m2[0][i] = m1[0][i] + m1[4][i];
    m2[1][i] = m1[1][i] + m1[5][i];
    m2[2][i] = m1[2][i] + m1[6][i];
    m2[3][i] = m1[3][i] + m1[7][i];
    m2[4][i] = m1[0][i] - m1[4][i];
    m2[5][i] = m1[1][i] - m1[5][i];
    m2[6][i] = m1[2][i] - m1[6][i];
    m2[7][i] = m1[3][i] - m1[7][i];

    m1[0][i] = m2[0][i] + m2[2][i];
    m1[1][i] = m2[1][i] + m2[3][i];
    m1[2][i] = m2[0][i] - m2[2][i];
    m1[3][i] = m2[1][i] - m2[3][i];
    m1[4][i] = m2[4][i] + m2[6][i];
    m1[5][i] = m2[5][i] + m2[7][i];
    m1[6][i] = m2[4][i] - m2[6][i];
    m1[7][i] = m2[5][i] - m2[7][i];

    m2[0][i] = m1[0][i] + m1[1][i];
    m2[1][i] = m1[0][i] - m1[1][i];
    m2[2][i] = m1[2][i] + m1[3][i];
    m2[3][i] = m1[2][i] - m1[3][i];
    m2[4][i] = m1[4][i] + m1[5][i];
    m2[5][i] = m1[4][i] - m1[5][i];
    m2[6][i] = m1[6][i] + m1[7][i];
    m2[7][i] = m1[6][i] - m1[7][i];
  }

  for( i = 0; i < 8; i++ )
  {
    for( j = 0; j < 16; j++ )
    {
      sad += abs( m2[i][j] );
    }
  }

#if JVET_R0164_MEAN_SCALED_SATD
  sad -= abs(m2[0][0]);
  sad += abs(m2[0][0]) >> 2;
#endif
  sad  = ( int ) ( sad / sqrt( 16.0 * 8 ) * 2 );

  return sad;
}

Distortion RdCost::xCalcHADs8x16(const Pel *piOrg, const Pel *piCur, ptrdiff_t strideOrg, ptrdiff_t strideCur)
{
  int k, i, j, jj, sad = 0;
  int diff[128], m1[16][8], m2[16][8];
  for( k = 0; k < 128; k += 8 )
  {
    diff[k + 0] = piOrg[0] - piCur[0];
    diff[k + 1] = piOrg[1] - piCur[1];
    diff[k + 2] = piOrg[2] - piCur[2];
    diff[k + 3] = piOrg[3] - piCur[3];
    diff[k + 4] = piOrg[4] - piCur[4];
    diff[k + 5] = piOrg[5] - piCur[5];
    diff[k + 6] = piOrg[6] - piCur[6];
    diff[k + 7] = piOrg[7] - piCur[7];

    piCur += strideCur;
    piOrg += strideOrg;
  }

  //horizontal
  for( j = 0; j < 16; j++ )
  {
    jj = j << 3;

    m2[j][0] = diff[jj] + diff[jj + 4];
    m2[j][1] = diff[jj + 1] + diff[jj + 5];
    m2[j][2] = diff[jj + 2] + diff[jj + 6];
    m2[j][3] = diff[jj + 3] + diff[jj + 7];
    m2[j][4] = diff[jj] - diff[jj + 4];
    m2[j][5] = diff[jj + 1] - diff[jj + 5];
    m2[j][6] = diff[jj + 2] - diff[jj + 6];
    m2[j][7] = diff[jj + 3] - diff[jj + 7];

    m1[j][0] = m2[j][0] + m2[j][2];
    m1[j][1] = m2[j][1] + m2[j][3];
    m1[j][2] = m2[j][0] - m2[j][2];
    m1[j][3] = m2[j][1] - m2[j][3];
    m1[j][4] = m2[j][4] + m2[j][6];
    m1[j][5] = m2[j][5] + m2[j][7];
    m1[j][6] = m2[j][4] - m2[j][6];
    m1[j][7] = m2[j][5] - m2[j][7];

    m2[j][0] = m1[j][0] + m1[j][1];
    m2[j][1] = m1[j][0] - m1[j][1];
    m2[j][2] = m1[j][2] + m1[j][3];
    m2[j][3] = m1[j][2] - m1[j][3];
    m2[j][4] = m1[j][4] + m1[j][5];
    m2[j][5] = m1[j][4] - m1[j][5];
    m2[j][6] = m1[j][6] + m1[j][7];
    m2[j][7] = m1[j][6] - m1[j][7];
  }

  //vertical
  for( i = 0; i < 8; i++ )
  {
    m1[0][i] = m2[0][i] + m2[8][i];
    m1[1][i] = m2[1][i] + m2[9][i];
    m1[2][i] = m2[2][i] + m2[10][i];
    m1[3][i] = m2[3][i] + m2[11][i];
    m1[4][i] = m2[4][i] + m2[12][i];
    m1[5][i] = m2[5][i] + m2[13][i];
    m1[6][i] = m2[6][i] + m2[14][i];
    m1[7][i] = m2[7][i] + m2[15][i];
    m1[8][i] = m2[0][i] - m2[8][i];
    m1[9][i] = m2[1][i] - m2[9][i];
    m1[10][i] = m2[2][i] - m2[10][i];
    m1[11][i] = m2[3][i] - m2[11][i];
    m1[12][i] = m2[4][i] - m2[12][i];
    m1[13][i] = m2[5][i] - m2[13][i];
    m1[14][i] = m2[6][i] - m2[14][i];
    m1[15][i] = m2[7][i] - m2[15][i];

    m2[0][i] = m1[0][i] + m1[4][i];
    m2[1][i] = m1[1][i] + m1[5][i];
    m2[2][i] = m1[2][i] + m1[6][i];
    m2[3][i] = m1[3][i] + m1[7][i];
    m2[4][i] = m1[0][i] - m1[4][i];
    m2[5][i] = m1[1][i] - m1[5][i];
    m2[6][i] = m1[2][i] - m1[6][i];
    m2[7][i] = m1[3][i] - m1[7][i];
    m2[8][i] = m1[8][i] + m1[12][i];
    m2[9][i] = m1[9][i] + m1[13][i];
    m2[10][i] = m1[10][i] + m1[14][i];
    m2[11][i] = m1[11][i] + m1[15][i];
    m2[12][i] = m1[8][i] - m1[12][i];
    m2[13][i] = m1[9][i] - m1[13][i];
    m2[14][i] = m1[10][i] - m1[14][i];
    m2[15][i] = m1[11][i] - m1[15][i];

    m1[0][i] = m2[0][i] + m2[2][i];
    m1[1][i] = m2[1][i] + m2[3][i];
    m1[2][i] = m2[0][i] - m2[2][i];
    m1[3][i] = m2[1][i] - m2[3][i];
    m1[4][i] = m2[4][i] + m2[6][i];
    m1[5][i] = m2[5][i] + m2[7][i];
    m1[6][i] = m2[4][i] - m2[6][i];
    m1[7][i] = m2[5][i] - m2[7][i];
    m1[8][i] = m2[8][i] + m2[10][i];
    m1[9][i] = m2[9][i] + m2[11][i];
    m1[10][i] = m2[8][i] - m2[10][i];
    m1[11][i] = m2[9][i] - m2[11][i];
    m1[12][i] = m2[12][i] + m2[14][i];
    m1[13][i] = m2[13][i] + m2[15][i];
    m1[14][i] = m2[12][i] - m2[14][i];
    m1[15][i] = m2[13][i] - m2[15][i];

    m2[0][i] = m1[0][i] + m1[1][i];
    m2[1][i] = m1[0][i] - m1[1][i];
    m2[2][i] = m1[2][i] + m1[3][i];
    m2[3][i] = m1[2][i] - m1[3][i];
    m2[4][i] = m1[4][i] + m1[5][i];
    m2[5][i] = m1[4][i] - m1[5][i];
    m2[6][i] = m1[6][i] + m1[7][i];
    m2[7][i] = m1[6][i] - m1[7][i];
    m2[8][i] = m1[8][i] + m1[9][i];
    m2[9][i] = m1[8][i] - m1[9][i];
    m2[10][i] = m1[10][i] + m1[11][i];
    m2[11][i] = m1[10][i] - m1[11][i];
    m2[12][i] = m1[12][i] + m1[13][i];
    m2[13][i] = m1[12][i] - m1[13][i];
    m2[14][i] = m1[14][i] + m1[15][i];
    m2[15][i] = m1[14][i] - m1[15][i];
  }

  for( i = 0; i < 16; i++ )
  {
    for( j = 0; j < 8; j++ )
    {
      sad += abs( m2[i][j] );
    }
  }

#if JVET_R0164_MEAN_SCALED_SATD
  sad -= abs(m2[0][0]);
  sad += abs(m2[0][0]) >> 2;
#endif
  sad  = ( int ) ( sad / sqrt( 16.0 * 8 ) * 2 );

  return sad;
}
Distortion RdCost::xCalcHADs4x8(const Pel *piOrg, const Pel *piCur, ptrdiff_t strideOrg, ptrdiff_t strideCur)
{
  int k, i, j, jj, sad = 0;
  int diff[32], m1[8][4], m2[8][4];
  for( k = 0; k < 32; k += 4 )
  {
    diff[k + 0] = piOrg[0] - piCur[0];
    diff[k + 1] = piOrg[1] - piCur[1];
    diff[k + 2] = piOrg[2] - piCur[2];
    diff[k + 3] = piOrg[3] - piCur[3];

    piCur += strideCur;
    piOrg += strideOrg;
  }

  //horizontal
  for( j = 0; j < 8; j++ )
  {
    jj = j << 2;
    m2[j][0] = diff[jj] + diff[jj + 2];
    m2[j][1] = diff[jj + 1] + diff[jj + 3];
    m2[j][2] = diff[jj] - diff[jj + 2];
    m2[j][3] = diff[jj + 1] - diff[jj + 3];

    m1[j][0] = m2[j][0] + m2[j][1];
    m1[j][1] = m2[j][0] - m2[j][1];
    m1[j][2] = m2[j][2] + m2[j][3];
    m1[j][3] = m2[j][2] - m2[j][3];
  }

  //vertical
  for( i = 0; i < 4; i++ )
  {
    m2[0][i] = m1[0][i] + m1[4][i];
    m2[1][i] = m1[1][i] + m1[5][i];
    m2[2][i] = m1[2][i] + m1[6][i];
    m2[3][i] = m1[3][i] + m1[7][i];
    m2[4][i] = m1[0][i] - m1[4][i];
    m2[5][i] = m1[1][i] - m1[5][i];
    m2[6][i] = m1[2][i] - m1[6][i];
    m2[7][i] = m1[3][i] - m1[7][i];

    m1[0][i] = m2[0][i] + m2[2][i];
    m1[1][i] = m2[1][i] + m2[3][i];
    m1[2][i] = m2[0][i] - m2[2][i];
    m1[3][i] = m2[1][i] - m2[3][i];
    m1[4][i] = m2[4][i] + m2[6][i];
    m1[5][i] = m2[5][i] + m2[7][i];
    m1[6][i] = m2[4][i] - m2[6][i];
    m1[7][i] = m2[5][i] - m2[7][i];

    m2[0][i] = m1[0][i] + m1[1][i];
    m2[1][i] = m1[0][i] - m1[1][i];
    m2[2][i] = m1[2][i] + m1[3][i];
    m2[3][i] = m1[2][i] - m1[3][i];
    m2[4][i] = m1[4][i] + m1[5][i];
    m2[5][i] = m1[4][i] - m1[5][i];
    m2[6][i] = m1[6][i] + m1[7][i];
    m2[7][i] = m1[6][i] - m1[7][i];
  }

  for( i = 0; i < 8; i++ )
  {
    for( j = 0; j < 4; j++ )
    {
      sad += abs( m2[i][j] );
    }
  }

#if JVET_R0164_MEAN_SCALED_SATD
  sad -= abs(m2[0][0]);
  sad += abs(m2[0][0]) >> 2;
#endif
  sad  = ( int ) ( sad / sqrt( 4.0 * 8 ) * 2 );

  return sad;
}

Distortion RdCost::xCalcHADs8x4(const Pel *piOrg, const Pel *piCur, ptrdiff_t strideOrg, ptrdiff_t strideCur)
{
  int k, i, j, jj, sad = 0;
  int diff[32], m1[4][8], m2[4][8];
  for( k = 0; k < 32; k += 8 )
  {
    diff[k + 0] = piOrg[0] - piCur[0];
    diff[k + 1] = piOrg[1] - piCur[1];
    diff[k + 2] = piOrg[2] - piCur[2];
    diff[k + 3] = piOrg[3] - piCur[3];
    diff[k + 4] = piOrg[4] - piCur[4];
    diff[k + 5] = piOrg[5] - piCur[5];
    diff[k + 6] = piOrg[6] - piCur[6];
    diff[k + 7] = piOrg[7] - piCur[7];

    piCur += strideCur;
    piOrg += strideOrg;
  }

  //horizontal
  for( j = 0; j < 4; j++ )
  {
    jj = j << 3;

    m2[j][0] = diff[jj] + diff[jj + 4];
    m2[j][1] = diff[jj + 1] + diff[jj + 5];
    m2[j][2] = diff[jj + 2] + diff[jj + 6];
    m2[j][3] = diff[jj + 3] + diff[jj + 7];
    m2[j][4] = diff[jj] - diff[jj + 4];
    m2[j][5] = diff[jj + 1] - diff[jj + 5];
    m2[j][6] = diff[jj + 2] - diff[jj + 6];
    m2[j][7] = diff[jj + 3] - diff[jj + 7];

    m1[j][0] = m2[j][0] + m2[j][2];
    m1[j][1] = m2[j][1] + m2[j][3];
    m1[j][2] = m2[j][0] - m2[j][2];
    m1[j][3] = m2[j][1] - m2[j][3];
    m1[j][4] = m2[j][4] + m2[j][6];
    m1[j][5] = m2[j][5] + m2[j][7];
    m1[j][6] = m2[j][4] - m2[j][6];
    m1[j][7] = m2[j][5] - m2[j][7];

    m2[j][0] = m1[j][0] + m1[j][1];
    m2[j][1] = m1[j][0] - m1[j][1];
    m2[j][2] = m1[j][2] + m1[j][3];
    m2[j][3] = m1[j][2] - m1[j][3];
    m2[j][4] = m1[j][4] + m1[j][5];
    m2[j][5] = m1[j][4] - m1[j][5];
    m2[j][6] = m1[j][6] + m1[j][7];
    m2[j][7] = m1[j][6] - m1[j][7];
  }

  //vertical
  for( i = 0; i < 8; i++ )
  {
    m1[0][i] = m2[0][i] + m2[2][i];
    m1[1][i] = m2[1][i] + m2[3][i];
    m1[2][i] = m2[0][i] - m2[2][i];
    m1[3][i] = m2[1][i] - m2[3][i];

    m2[0][i] = m1[0][i] + m1[1][i];
    m2[1][i] = m1[0][i] - m1[1][i];
    m2[2][i] = m1[2][i] + m1[3][i];
    m2[3][i] = m1[2][i] - m1[3][i];
  }

  for( i = 0; i < 4; i++ )
  {
    for( j = 0; j < 8; j++ )
    {
      sad += abs( m2[i][j] );
    }
  }

#if JVET_R0164_MEAN_SCALED_SATD
  sad -= abs(m2[0][0]);
  sad += abs(m2[0][0]) >> 2;
#endif
  sad  = ( int ) ( sad / sqrt( 4.0 * 8 ) * 2 );

  return sad;
}

Distortion RdCost::xGetHADs( const DistParam &rcDtParam )
{
  if( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetHADsw( rcDtParam );
  }
  const Pel* piOrg = rcDtParam.org.buf;
  const Pel* piCur = rcDtParam.cur.buf;
  const int  rows      = rcDtParam.org.height;
  const int  cols      = rcDtParam.org.width;
  const ptrdiff_t strideCur = rcDtParam.cur.stride;
  const ptrdiff_t strideOrg = rcDtParam.org.stride;
  const int  step      = rcDtParam.step;

  int  x = 0, y = 0;

  Distortion sum = 0;

  if (cols > rows && (rows & 7) == 0 && (cols & 15) == 0)
  {
    for (y = 0; y < rows; y += 8)
    {
      for (x = 0; x < cols; x += 16)
      {
        sum += xCalcHADs16x8(&piOrg[x], &piCur[x], strideOrg, strideCur);
      }
      piOrg += strideOrg * 8;
      piCur += strideCur * 8;
    }
  }
  else if (cols < rows && (cols & 7) == 0 && (rows & 15) == 0)
  {
    for (y = 0; y < rows; y += 16)
    {
      for (x = 0; x < cols; x += 8)
      {
        sum += xCalcHADs8x16(&piOrg[x], &piCur[x], strideOrg, strideCur);
      }
      piOrg += strideOrg * 16;
      piCur += strideCur * 16;
    }
  }
  else if (cols > rows && (rows & 3) == 0 && (cols & 7) == 0)
  {
    for (y = 0; y < rows; y += 4)
    {
      for (x = 0; x < cols; x += 8)
      {
        sum += xCalcHADs8x4(&piOrg[x], &piCur[x], strideOrg, strideCur);
      }
      piOrg += strideOrg * 4;
      piCur += strideCur * 4;
    }
  }
  else if (cols < rows && (cols & 3) == 0 && (rows & 7) == 0)
  {
    for (y = 0; y < rows; y += 8)
    {
      for (x = 0; x < cols; x += 4)
      {
        sum += xCalcHADs4x8(&piOrg[x], &piCur[x], strideOrg, strideCur);
      }
      piOrg += strideOrg * 8;
      piCur += strideCur * 8;
    }
  }
  else if ((rows % 8 == 0) && (cols % 8 == 0))
  {
    ptrdiff_t offsetOrg = strideOrg << 3;
    ptrdiff_t offsetCur = strideCur << 3;
    for (y = 0; y < rows; y += 8)
    {
      for (x = 0; x < cols; x += 8)
      {
        sum += xCalcHADs8x8(&piOrg[x], &piCur[x * step], strideOrg, strideCur, step);
      }
      piOrg += offsetOrg;
      piCur += offsetCur;
    }
  }
  else if ((rows % 4 == 0) && (cols % 4 == 0))
  {
    ptrdiff_t offsetOrg = strideOrg << 2;
    ptrdiff_t offsetCur = strideCur << 2;

    for (y = 0; y < rows; y += 4)
    {
      for (x = 0; x < cols; x += 4)
      {
        sum += xCalcHADs4x4(&piOrg[x], &piCur[x * step], strideOrg, strideCur, step);
      }
      piOrg += offsetOrg;
      piCur += offsetCur;
    }
  }
  else if ((rows % 2 == 0) && (cols % 2 == 0))
  {
    ptrdiff_t offsetOrg = strideOrg << 1;
    ptrdiff_t offsetCur = strideCur << 1;
    for (y = 0; y < rows; y += 2)
    {
      for (x = 0; x < cols; x += 2)
      {
        sum += xCalcHADs2x2(&piOrg[x], &piCur[x * step], strideOrg, strideCur, step);
      }
      piOrg += offsetOrg;
      piCur += offsetCur;
    }
  }
  else
  {
    THROW( "Invalid size" );
  }

  return (sum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth));
}


#if WCG_EXT
void RdCost::saveUnadjustedLambda()
{
  m_dLambda_unadjusted = m_dLambda;
  m_distScaleUnadjusted = m_distScale;
}

void RdCost::initLumaLevelToWeightTable(int bitDepth)
{
  int lutSize = 1 << bitDepth;
  if (m_lumaLevelToWeightPLUT.empty())
  {
    m_lumaLevelToWeightPLUT.resize(lutSize, 1.0);
  }
  for (int i = 0; i < lutSize; i++)
  {
    double x = bitDepth < 10 ? i << (10 - bitDepth) : bitDepth > 10 ? i >> (bitDepth - 10) : i;
    double y;

    y = 0.015 * x - 1.5
        - 6;   // this is the Equation used to derive the luma qp LUT for HDR in MPEG HDR anchor3.2 (JCTCX-X1020)
    y = y < -3 ? -3 : (y > 6 ? 6 : y);

    m_lumaLevelToWeightPLUT[i] = pow(2.0, y / 3.0);      // or power(10, dQp/10)      they are almost equal
  }
}

void RdCost::initLumaLevelToWeightTableReshape()
{
  int lutSize = 1 << m_lumaBD;
  if (m_reshapeLumaLevelToWeightPLUT.empty())
  {
    m_reshapeLumaLevelToWeightPLUT.resize(lutSize, MSE_WEIGHT_ONE);
  }
  if (m_lumaLevelToWeightPLUT.empty())
  {
    m_lumaLevelToWeightPLUT.resize(lutSize, 1.0);
  }
  if (m_signalType == RESHAPE_SIGNAL_PQ)
  {
    for (int i = 0; i < (1 << m_lumaBD); i++)
    {
      double x = m_lumaBD < 10 ? i << (10 - m_lumaBD) : m_lumaBD > 10 ? i >> (m_lumaBD - 10) : i;
      double y;
      y = 0.015*x - 1.5 - 6;
      y = y < -3 ? -3 : (y > 6 ? 6 : y);
      const double weight               = pow(2.0, y / 3.0);
      m_reshapeLumaLevelToWeightPLUT[i] = (int) (weight * MSE_WEIGHT_ONE);
      m_lumaLevelToWeightPLUT[i]        = weight;
    }
  }
}

void RdCost::updateReshapeLumaLevelToWeightTableChromaMD(std::vector<Pel>& ILUT)
{
  for (int i = 0; i < (1 << m_lumaBD); i++)
  {
    m_reshapeLumaLevelToWeightPLUT[i] = (int) (m_lumaLevelToWeightPLUT[ILUT[i]] * MSE_WEIGHT_ONE);
  }
}

void RdCost::restoreReshapeLumaLevelToWeightTable()
{
  for (int i = 0; i < (1 << m_lumaBD); i++)
  {
    m_reshapeLumaLevelToWeightPLUT.at(i) = (int) (m_lumaLevelToWeightPLUT.at(i) * MSE_WEIGHT_ONE);
  }
}

void RdCost::updateReshapeLumaLevelToWeightTable(SliceReshapeInfo &sliceReshape, Pel *wtTable, double cwt)
{
  if (m_signalType == RESHAPE_SIGNAL_SDR || m_signalType == RESHAPE_SIGNAL_HLG)
  {
    if (sliceReshape.getSliceReshapeModelPresentFlag())
    {
      double wBin = 1.0;
      double weight = 1.0;
      int histLens = (1 << m_lumaBD) / PIC_CODE_CW_BINS;

      for (int i = 0; i < PIC_CODE_CW_BINS; i++)
      {
        if ((i < sliceReshape.reshaperModelMinBinIdx) || (i > sliceReshape.reshaperModelMaxBinIdx))
        {
          weight = 1.0;
        }
        else
        {
          if (sliceReshape.reshaperModelBinCWDelta[i] == 1 || (sliceReshape.reshaperModelBinCWDelta[i] == -1 * histLens))
          {
            weight = wBin;
          }
          else
          {
            weight = (double)wtTable[i] / (double)histLens;
            weight = weight*weight;
          }
        }
        for (int j = 0; j < histLens; j++)
        {
          int ii = i*histLens + j;
          m_reshapeLumaLevelToWeightPLUT[ii] = (int) (weight * MSE_WEIGHT_ONE);
        }
      }
      m_chromaWeight = (int) (cwt * MSE_WEIGHT_ONE);
    }
    else
    {
      THROW("updateReshapeLumaLevelToWeightTable ERROR!!");
    }
  }
  else
  {
    THROW("updateReshapeLumaLevelToWeightTable not support other signal types!!");
  }
}

Distortion RdCost::getWeightedMSE(const int compIdx, const Pel org, const Pel cur, const uint32_t shift, const Pel orgLuma) const
{
  CHECKD(org < 0, "Sample value must be positive");

  if (compIdx == COMPONENT_Y)
  {
    CHECKD(org != orgLuma, "Luma sample values must be equal to each other");
  }

  Pel diff = org - cur;

  // use luma to get weight
  int64_t weight = m_reshapeLumaLevelToWeightPLUT[orgLuma];

  return (weight * (diff * diff) + (1 << MSE_WEIGHT_FRAC_BITS >> 1)) >> (MSE_WEIGHT_FRAC_BITS + shift);
}

Distortion RdCost::xGetSSE_WTD( const DistParam &rcDtParam ) const
{
  if( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );  // ignore it for now
  }
  int           rows             = rcDtParam.org.height;
  const Pel* piOrg = rcDtParam.org.buf;
  const Pel* piCur = rcDtParam.cur.buf;
  const int     cols             = rcDtParam.org.width;
  const ptrdiff_t strideCur        = rcDtParam.cur.stride;
  const ptrdiff_t strideOrg        = rcDtParam.org.stride;
  const Pel* piOrgLuma        = rcDtParam.orgLuma.buf;
  const ptrdiff_t strideOrgLuma    = rcDtParam.orgLuma.stride;
  const size_t  cShift  = rcDtParam.cShiftX;
  const size_t  cShiftY = rcDtParam.cShiftY;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;
  for (; rows != 0; rows--)
  {
    for (int n = 0; n < cols; n++)
    {
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n], piCur[n], shift, piOrgLuma[n << cShift]);
    }
    piOrg += strideOrg;
    piCur += strideCur;

    piOrgLuma += strideOrgLuma << cShiftY;
  }
  return (sum);
}

Distortion RdCost::xGetSSE2_WTD( const DistParam &rcDtParam ) const
{
  if( rcDtParam.applyWeight )
  {
    CHECK( rcDtParam.org.width != 2, "" );
    return RdCostWeightPrediction::xGetSSEw( rcDtParam ); // ignore it for now
  }

  int           rows                = rcDtParam.org.height;
  const Pel* piOrg = rcDtParam.org.buf;
  const Pel* piCur = rcDtParam.cur.buf;
  const ptrdiff_t strideCur           = rcDtParam.cur.stride;
  const ptrdiff_t strideOrg           = rcDtParam.org.stride;
  const Pel* piOrgLuma           = rcDtParam.orgLuma.buf;
  const size_t  strideOrgLuma       = rcDtParam.orgLuma.stride;
  const size_t  cShift  = rcDtParam.cShiftX;
  const size_t  cShiftY = rcDtParam.cShiftY;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;
  for (; rows != 0; rows--)
  {
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[0], piCur[0], shift,
      piOrgLuma[size_t(0) << cShift]);   // piOrg[0] - piCur[0]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[1], piCur[1], shift,
      piOrgLuma[size_t(1) << cShift]);   // piOrg[1] - piCur[1]; sum += Distortion(( temp * temp ) >> shift);
    piOrg += strideOrg;
    piCur += strideCur;
    piOrgLuma += strideOrgLuma << cShiftY;
  }
  return (sum);
}

Distortion RdCost::xGetSSE4_WTD( const DistParam &rcDtParam ) const
{
  if( rcDtParam.applyWeight )
  {
    CHECK( rcDtParam.org.width != 4, "" );
    return RdCostWeightPrediction::xGetSSEw( rcDtParam ); // ignore it for now
  }

  int           rows             = rcDtParam.org.height;
  const Pel* piOrg = rcDtParam.org.buf;
  const Pel* piCur = rcDtParam.cur.buf;
  const ptrdiff_t strideCur        = rcDtParam.cur.stride;
  const ptrdiff_t strideOrg        = rcDtParam.org.stride;
  const Pel* piOrgLuma        = rcDtParam.orgLuma.buf;
  const size_t  strideOrgLuma    = rcDtParam.orgLuma.stride;
  const size_t  cShift  = rcDtParam.cShiftX;
  const size_t  cShiftY = rcDtParam.cShiftY;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;
  for (; rows != 0; rows--)
  {
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[0], piCur[0], shift,
      piOrgLuma[size_t(0) << cShift]);   // piOrg[0] - piCur[0]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[1], piCur[1], shift,
      piOrgLuma[size_t(1) << cShift]);   // piOrg[1] - piCur[1]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[2], piCur[2], shift,
      piOrgLuma[size_t(2) << cShift]);   // piOrg[2] - piCur[2]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[3], piCur[3], shift,
      piOrgLuma[size_t(3) << cShift]);   // piOrg[3] - piCur[3]; sum += Distortion(( temp * temp ) >> shift);
    piOrg += strideOrg;
    piCur += strideCur;
    piOrgLuma += strideOrgLuma << cShiftY;
  }
  return (sum);
}

Distortion RdCost::xGetSSE8_WTD( const DistParam &rcDtParam ) const
{
  if( rcDtParam.applyWeight )
  {
    CHECK( rcDtParam.org.width != 8, "" );
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }

  int           rows             = rcDtParam.org.height;
  const Pel* piOrg = rcDtParam.org.buf;
  const Pel* piCur = rcDtParam.cur.buf;
  const ptrdiff_t strideCur        = rcDtParam.cur.stride;
  const ptrdiff_t strideOrg        = rcDtParam.org.stride;
  const Pel* piOrgLuma        = rcDtParam.orgLuma.buf;
  const size_t  strideOrgLuma    = rcDtParam.orgLuma.stride;
  const size_t  cShift  = rcDtParam.cShiftX;
  const size_t  cShiftY = rcDtParam.cShiftY;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;
  for (; rows != 0; rows--)
  {
    sum += getWeightedMSE(rcDtParam.compID, piOrg[0], piCur[0], shift,
                          piOrgLuma[0]);   // piOrg[0] - piCur[0]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[1], piCur[1], shift,
      piOrgLuma[size_t(1) << cShift]);   // piOrg[1] - piCur[1]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[2], piCur[2], shift,
      piOrgLuma[size_t(2) << cShift]);   // piOrg[2] - piCur[2]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[3], piCur[3], shift,
      piOrgLuma[size_t(3) << cShift]);   // piOrg[3] - piCur[3]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[4], piCur[4], shift,
      piOrgLuma[size_t(4) << cShift]);   // piOrg[4] - piCur[4]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[5], piCur[5], shift,
      piOrgLuma[size_t(5) << cShift]);   // piOrg[5] - piCur[5]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[6], piCur[6], shift,
      piOrgLuma[size_t(6) << cShift]);   // piOrg[6] - piCur[6]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[7], piCur[7], shift,
      piOrgLuma[size_t(7) << cShift]);   // piOrg[7] - piCur[7]; sum += Distortion(( temp * temp ) >> shift);
    piOrg += strideOrg;
    piCur += strideCur;
    piOrgLuma += strideOrgLuma << cShiftY;
  }
  return (sum);
}

Distortion RdCost::xGetSSE16_WTD( const DistParam &rcDtParam ) const
{
  if( rcDtParam.applyWeight )
  {
    CHECK( rcDtParam.org.width != 16, "" );
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }
  int           rows             = rcDtParam.org.height;
  const Pel* piOrg = rcDtParam.org.buf;
  const Pel* piCur = rcDtParam.cur.buf;
  const ptrdiff_t strideCur        = rcDtParam.cur.stride;
  const ptrdiff_t strideOrg        = rcDtParam.org.stride;
  const Pel* piOrgLuma        = rcDtParam.orgLuma.buf;
  const size_t  strideOrgLuma    = rcDtParam.orgLuma.stride;
  const size_t  cShift  = rcDtParam.cShiftX;
  const size_t  cShiftY = rcDtParam.cShiftY;
  Distortion    sum              = 0;
  uint32_t      shift            = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;
  for (; rows != 0; rows--)
  {
    sum += getWeightedMSE(rcDtParam.compID, piOrg[0], piCur[0], shift,
                          piOrgLuma[0]);   // piOrg[ 0] - piCur[ 0]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[1], piCur[1], shift,
      piOrgLuma[size_t(1) << cShift]);   // piOrg[ 1] - piCur[ 1]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[2], piCur[2], shift,
      piOrgLuma[size_t(2) << cShift]);   // piOrg[ 2] - piCur[ 2]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[3], piCur[3], shift,
      piOrgLuma[size_t(3) << cShift]);   // piOrg[ 3] - piCur[ 3]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[4], piCur[4], shift,
      piOrgLuma[size_t(4) << cShift]);   // piOrg[ 4] - piCur[ 4]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[5], piCur[5], shift,
      piOrgLuma[size_t(5) << cShift]);   // piOrg[ 5] - piCur[ 5]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[6], piCur[6], shift,
      piOrgLuma[size_t(6) << cShift]);   // piOrg[ 6] - piCur[ 6]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[7], piCur[7], shift,
      piOrgLuma[size_t(7) << cShift]);   // piOrg[ 7] - piCur[ 7]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[8], piCur[8], shift,
      piOrgLuma[size_t(8) << cShift]);   // piOrg[ 8] - piCur[ 8]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[9], piCur[9], shift,
      piOrgLuma[size_t(9) << cShift]);   // piOrg[ 9] - piCur[ 9]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[10], piCur[10], shift,
      piOrgLuma[size_t(10) << cShift]);   // piOrg[10] - piCur[10]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[11], piCur[11], shift,
      piOrgLuma[size_t(11) << cShift]);   // piOrg[11] - piCur[11]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[12], piCur[12], shift,
      piOrgLuma[size_t(12) << cShift]);   // piOrg[12] - piCur[12]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[13], piCur[13], shift,
      piOrgLuma[size_t(13) << cShift]);   // piOrg[13] - piCur[13]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[14], piCur[14], shift,
      piOrgLuma[size_t(14) << cShift]);   // piOrg[14] - piCur[14]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[15], piCur[15], shift,
      piOrgLuma[size_t(15) << cShift]);   // piOrg[15] - piCur[15]; sum += Distortion(( temp * temp ) >> shift);
    piOrg += strideOrg;
    piCur += strideCur;

    piOrgLuma += strideOrgLuma << cShiftY;
  }
  return (sum);
}

Distortion RdCost::xGetSSE16N_WTD( const DistParam &rcDtParam ) const
{
  if( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }
  int           rows             = rcDtParam.org.height;
  const Pel* piOrg = rcDtParam.org.buf;
  const Pel* piCur = rcDtParam.cur.buf;
  const int     cols             = rcDtParam.org.width;
  const ptrdiff_t strideCur        = rcDtParam.cur.stride;
  const ptrdiff_t strideOrg        = rcDtParam.org.stride;
  const Pel* piOrgLuma        = rcDtParam.orgLuma.buf;
  const size_t  strideOrgLuma    = rcDtParam.orgLuma.stride;
  const size_t  cShift  = rcDtParam.cShiftX;
  const size_t  cShiftY = rcDtParam.cShiftY;
  Distortion    sum              = 0;
  uint32_t      shift            = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;
  for (; rows != 0; rows--)
  {
    for (int n = 0; n < cols; n += 16)
    {
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 0], piCur[n + 0], shift,
                            piOrgLuma[size_t(n + 0) << cShift]);   // temp = piOrg[n+ 0] - piCur[n+ 0]; sum +=
                                                                   // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 1], piCur[n + 1], shift,
                            piOrgLuma[size_t(n + 1) << cShift]);   // temp = piOrg[n+ 1] - piCur[n+ 1]; sum +=
                                                                   // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 2], piCur[n + 2], shift,
                            piOrgLuma[size_t(n + 2) << cShift]);   // temp = piOrg[n+ 2] - piCur[n+ 2]; sum +=
                                                                   // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 3], piCur[n + 3], shift,
                            piOrgLuma[size_t(n + 3) << cShift]);   // temp = piOrg[n+ 3] - piCur[n+ 3]; sum +=
                                                                   // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 4], piCur[n + 4], shift,
                            piOrgLuma[size_t(n + 4) << cShift]);   // temp = piOrg[n+ 4] - piCur[n+ 4]; sum +=
                                                                   // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 5], piCur[n + 5], shift,
                            piOrgLuma[size_t(n + 5) << cShift]);   // temp = piOrg[n+ 5] - piCur[n+ 5]; sum +=
                                                                   // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 6], piCur[n + 6], shift,
                            piOrgLuma[size_t(n + 6) << cShift]);   // temp = piOrg[n+ 6] - piCur[n+ 6]; sum +=
                                                                   // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 7], piCur[n + 7], shift,
                            piOrgLuma[size_t(n + 7) << cShift]);   // temp = piOrg[n+ 7] - piCur[n+ 7]; sum +=
                                                                   // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 8], piCur[n + 8], shift,
                            piOrgLuma[size_t(n + 8) << cShift]);   // temp = piOrg[n+ 8] - piCur[n+ 8]; sum +=
                                                                   // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 9], piCur[n + 9], shift,
                            piOrgLuma[size_t(n + 9) << cShift]);   // temp = piOrg[n+ 9] - piCur[n+ 9]; sum +=
                                                                   // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 10], piCur[n + 10], shift,
                            piOrgLuma[size_t(n + 10) << cShift]);   // temp = piOrg[n+10] - piCur[n+10]; sum +=
                                                                    // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 11], piCur[n + 11], shift,
                            piOrgLuma[size_t(n + 11) << cShift]);   // temp = piOrg[n+11] - piCur[n+11]; sum +=
                                                                    // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 12], piCur[n + 12], shift,
                            piOrgLuma[size_t(n + 12) << cShift]);   // temp = piOrg[n+12] - piCur[n+12]; sum +=
                                                                    // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 13], piCur[n + 13], shift,
                            piOrgLuma[size_t(n + 13) << cShift]);   // temp = piOrg[n+13] - piCur[n+13]; sum +=
                                                                    // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 14], piCur[n + 14], shift,
                            piOrgLuma[size_t(n + 14) << cShift]);   // temp = piOrg[n+14] - piCur[n+14]; sum +=
                                                                    // Distortion(( temp * temp ) >> shift);
      sum += getWeightedMSE(rcDtParam.compID, piOrg[n + 15], piCur[n + 15], shift,
                            piOrgLuma[size_t(n + 15) << cShift]);   // temp = piOrg[n+15] - piCur[n+15]; sum +=
                                                                    // Distortion(( temp * temp ) >> shift);
    }
    piOrg += strideOrg;
    piCur += strideCur;
    piOrgLuma += strideOrgLuma << cShiftY;
  }
  return (sum);
}

Distortion RdCost::xGetSSE32_WTD( const DistParam &rcDtParam ) const
{
  if( rcDtParam.applyWeight )
  {
    CHECK( rcDtParam.org.width != 32, "" );
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }
  int           rows             = rcDtParam.org.height;
  const Pel* piOrg = rcDtParam.org.buf;
  const Pel* piCur = rcDtParam.cur.buf;
  const ptrdiff_t strideCur        = rcDtParam.cur.stride;
  const ptrdiff_t strideOrg        = rcDtParam.org.stride;
  const Pel* piOrgLuma        = rcDtParam.orgLuma.buf;
  const size_t  strideOrgLuma    = rcDtParam.orgLuma.stride;
  const size_t  cShift  = rcDtParam.cShiftX;
  const size_t  cShiftY = rcDtParam.cShiftY;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;
  for (; rows != 0; rows--)
  {
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[0], piCur[0], shift,
      piOrgLuma[size_t(0)]);   // temp = piOrg[ 0] - piCur[ 0]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[1], piCur[1], shift,
                          piOrgLuma[size_t(1) << cShift]);   // temp = piOrg[ 1] - piCur[ 1]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[2], piCur[2], shift,
                          piOrgLuma[size_t(2) << cShift]);   // temp = piOrg[ 2] - piCur[ 2]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[3], piCur[3], shift,
                          piOrgLuma[size_t(3) << cShift]);   // temp = piOrg[ 3] - piCur[ 3]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[4], piCur[4], shift,
                          piOrgLuma[size_t(4) << cShift]);   // temp = piOrg[ 4] - piCur[ 4]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[5], piCur[5], shift,
                          piOrgLuma[size_t(5) << cShift]);   // temp = piOrg[ 5] - piCur[ 5]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[6], piCur[6], shift,
                          piOrgLuma[size_t(6) << cShift]);   // temp = piOrg[ 6] - piCur[ 6]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[7], piCur[7], shift,
                          piOrgLuma[size_t(7) << cShift]);   // temp = piOrg[ 7] - piCur[ 7]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[8], piCur[8], shift,
                          piOrgLuma[size_t(8) << cShift]);   // temp = piOrg[ 8] - piCur[ 8]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[9], piCur[9], shift,
                          piOrgLuma[size_t(9) << cShift]);   // temp = piOrg[ 9] - piCur[ 9]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[10], piCur[10], shift,
                          piOrgLuma[size_t(10) << cShift]);   // temp = piOrg[10] - piCur[10]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[11], piCur[11], shift,
                          piOrgLuma[size_t(11) << cShift]);   // temp = piOrg[11] - piCur[11]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[12], piCur[12], shift,
                          piOrgLuma[size_t(12) << cShift]);   // temp = piOrg[12] - piCur[12]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[13], piCur[13], shift,
                          piOrgLuma[size_t(13) << cShift]);   // temp = piOrg[13] - piCur[13]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[14], piCur[14], shift,
                          piOrgLuma[size_t(14) << cShift]);   // temp = piOrg[14] - piCur[14]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[15], piCur[15], shift,
                          piOrgLuma[size_t(15) << cShift]);   // temp = piOrg[15] - piCur[15]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[16], piCur[16], shift,
                          piOrgLuma[size_t(16) << cShift]);   //  temp = piOrg[16] - piCur[16]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[17], piCur[17], shift,
                          piOrgLuma[size_t(17) << cShift]);   //  temp = piOrg[17] - piCur[17]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[18], piCur[18], shift,
                          piOrgLuma[size_t(18) << cShift]);   //  temp = piOrg[18] - piCur[18]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[19], piCur[19], shift,
                          piOrgLuma[size_t(19) << cShift]);   //  temp = piOrg[19] - piCur[19]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[20], piCur[20], shift,
                          piOrgLuma[size_t(20) << cShift]);   //  temp = piOrg[20] - piCur[20]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[21], piCur[21], shift,
                          piOrgLuma[size_t(21) << cShift]);   //  temp = piOrg[21] - piCur[21]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[22], piCur[22], shift,
                          piOrgLuma[size_t(22) << cShift]);   //  temp = piOrg[22] - piCur[22]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[23], piCur[23], shift,
                          piOrgLuma[size_t(23) << cShift]);   //  temp = piOrg[23] - piCur[23]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[24], piCur[24], shift,
                          piOrgLuma[size_t(24) << cShift]);   //  temp = piOrg[24] - piCur[24]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[25], piCur[25], shift,
                          piOrgLuma[size_t(25) << cShift]);   //  temp = piOrg[25] - piCur[25]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[26], piCur[26], shift,
                          piOrgLuma[size_t(26) << cShift]);   //  temp = piOrg[26] - piCur[26]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[27], piCur[27], shift,
                          piOrgLuma[size_t(27) << cShift]);   //  temp = piOrg[27] - piCur[27]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[28], piCur[28], shift,
                          piOrgLuma[size_t(28) << cShift]);   //  temp = piOrg[28] - piCur[28]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[29], piCur[29], shift,
                          piOrgLuma[size_t(29) << cShift]);   //  temp = piOrg[29] - piCur[29]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[30], piCur[30], shift,
                          piOrgLuma[size_t(30) << cShift]);   //  temp = piOrg[30] - piCur[30]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[31], piCur[31], shift,
                          piOrgLuma[size_t(31) << cShift]);   //  temp = piOrg[31] - piCur[31]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    piOrg += strideOrg;
    piCur += strideCur;
    piOrgLuma += strideOrgLuma << cShiftY;
  }
  return (sum);
}

Distortion RdCost::xGetSSE64_WTD( const DistParam &rcDtParam ) const
{
  if( rcDtParam.applyWeight )
  {
    CHECK( rcDtParam.org.width != 64, "" );
    return RdCostWeightPrediction::xGetSSEw( rcDtParam );
  }
  int           rows             = rcDtParam.org.height;
  const Pel* piOrg = rcDtParam.org.buf;
  const Pel* piCur = rcDtParam.cur.buf;
  const ptrdiff_t strideCur        = rcDtParam.cur.stride;
  const ptrdiff_t strideOrg        = rcDtParam.org.stride;
  const Pel* piOrgLuma        = rcDtParam.orgLuma.buf;
  const size_t  strideOrgLuma    = rcDtParam.orgLuma.stride;
  const size_t  cShift  = rcDtParam.cShiftX;
  const size_t  cShiftY = rcDtParam.cShiftY;

  Distortion sum     = 0;
  uint32_t   shift   = DISTORTION_PRECISION_ADJUSTMENT((rcDtParam.bitDepth)) << 1;
  for (; rows != 0; rows--)
  {
    sum += getWeightedMSE(
      rcDtParam.compID, piOrg[0], piCur[0], shift,
      piOrgLuma[size_t(0)]);   // temp = piOrg[ 0] - piCur[ 0]; sum += Distortion(( temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[1], piCur[1], shift,
                          piOrgLuma[size_t(1) << cShift]);   // temp = piOrg[ 1] - piCur[ 1]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[2], piCur[2], shift,
                          piOrgLuma[size_t(2) << cShift]);   // temp = piOrg[ 2] - piCur[ 2]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[3], piCur[3], shift,
                          piOrgLuma[size_t(3) << cShift]);   // temp = piOrg[ 3] - piCur[ 3]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[4], piCur[4], shift,
                          piOrgLuma[size_t(4) << cShift]);   // temp = piOrg[ 4] - piCur[ 4]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[5], piCur[5], shift,
                          piOrgLuma[size_t(5) << cShift]);   // temp = piOrg[ 5] - piCur[ 5]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[6], piCur[6], shift,
                          piOrgLuma[size_t(6) << cShift]);   // temp = piOrg[ 6] - piCur[ 6]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[7], piCur[7], shift,
                          piOrgLuma[size_t(7) << cShift]);   // temp = piOrg[ 7] - piCur[ 7]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[8], piCur[8], shift,
                          piOrgLuma[size_t(8) << cShift]);   // temp = piOrg[ 8] - piCur[ 8]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[9], piCur[9], shift,
                          piOrgLuma[size_t(9) << cShift]);   // temp = piOrg[ 9] - piCur[ 9]; sum += Distortion((
                                                             // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[10], piCur[10], shift,
                          piOrgLuma[size_t(10) << cShift]);   // temp = piOrg[10] - piCur[10]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[11], piCur[11], shift,
                          piOrgLuma[size_t(11) << cShift]);   // temp = piOrg[11] - piCur[11]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[12], piCur[12], shift,
                          piOrgLuma[size_t(12) << cShift]);   // temp = piOrg[12] - piCur[12]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[13], piCur[13], shift,
                          piOrgLuma[size_t(13) << cShift]);   // temp = piOrg[13] - piCur[13]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[14], piCur[14], shift,
                          piOrgLuma[size_t(14) << cShift]);   // temp = piOrg[14] - piCur[14]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[15], piCur[15], shift,
                          piOrgLuma[size_t(15) << cShift]);   // temp = piOrg[15] - piCur[15]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[16], piCur[16], shift,
                          piOrgLuma[size_t(16) << cShift]);   //  temp = piOrg[16] - piCur[16]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[17], piCur[17], shift,
                          piOrgLuma[size_t(17) << cShift]);   //  temp = piOrg[17] - piCur[17]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[18], piCur[18], shift,
                          piOrgLuma[size_t(18) << cShift]);   //  temp = piOrg[18] - piCur[18]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[19], piCur[19], shift,
                          piOrgLuma[size_t(19) << cShift]);   //  temp = piOrg[19] - piCur[19]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[20], piCur[20], shift,
                          piOrgLuma[size_t(20) << cShift]);   //  temp = piOrg[20] - piCur[20]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[21], piCur[21], shift,
                          piOrgLuma[size_t(21) << cShift]);   //  temp = piOrg[21] - piCur[21]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[22], piCur[22], shift,
                          piOrgLuma[size_t(22) << cShift]);   //  temp = piOrg[22] - piCur[22]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[23], piCur[23], shift,
                          piOrgLuma[size_t(23) << cShift]);   //  temp = piOrg[23] - piCur[23]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[24], piCur[24], shift,
                          piOrgLuma[size_t(24) << cShift]);   //  temp = piOrg[24] - piCur[24]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[25], piCur[25], shift,
                          piOrgLuma[size_t(25) << cShift]);   //  temp = piOrg[25] - piCur[25]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[26], piCur[26], shift,
                          piOrgLuma[size_t(26) << cShift]);   //  temp = piOrg[26] - piCur[26]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[27], piCur[27], shift,
                          piOrgLuma[size_t(27) << cShift]);   //  temp = piOrg[27] - piCur[27]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[28], piCur[28], shift,
                          piOrgLuma[size_t(28) << cShift]);   //  temp = piOrg[28] - piCur[28]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[29], piCur[29], shift,
                          piOrgLuma[size_t(29) << cShift]);   //  temp = piOrg[29] - piCur[29]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[30], piCur[30], shift,
                          piOrgLuma[size_t(30) << cShift]);   //  temp = piOrg[30] - piCur[30]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[31], piCur[31], shift,
                          piOrgLuma[size_t(31) << cShift]);   //  temp = piOrg[31] - piCur[31]; sum += Distortion((
                                                              //  temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[32], piCur[32], shift,
                          piOrgLuma[size_t(32) << cShift]);   // temp = piOrg[32] - piCur[32]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[33], piCur[33], shift,
                          piOrgLuma[size_t(33) << cShift]);   // temp = piOrg[33] - piCur[33]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[34], piCur[34], shift,
                          piOrgLuma[size_t(34) << cShift]);   // temp = piOrg[34] - piCur[34]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[35], piCur[35], shift,
                          piOrgLuma[size_t(35) << cShift]);   // temp = piOrg[35] - piCur[35]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[36], piCur[36], shift,
                          piOrgLuma[size_t(36) << cShift]);   // temp = piOrg[36] - piCur[36]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[37], piCur[37], shift,
                          piOrgLuma[size_t(37) << cShift]);   // temp = piOrg[37] - piCur[37]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[38], piCur[38], shift,
                          piOrgLuma[size_t(38) << cShift]);   // temp = piOrg[38] - piCur[38]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[39], piCur[39], shift,
                          piOrgLuma[size_t(39) << cShift]);   // temp = piOrg[39] - piCur[39]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[40], piCur[40], shift,
                          piOrgLuma[size_t(40) << cShift]);   // temp = piOrg[40] - piCur[40]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[41], piCur[41], shift,
                          piOrgLuma[size_t(41) << cShift]);   // temp = piOrg[41] - piCur[41]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[42], piCur[42], shift,
                          piOrgLuma[size_t(42) << cShift]);   // temp = piOrg[42] - piCur[42]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[43], piCur[43], shift,
                          piOrgLuma[size_t(43) << cShift]);   // temp = piOrg[43] - piCur[43]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[44], piCur[44], shift,
                          piOrgLuma[size_t(44) << cShift]);   // temp = piOrg[44] - piCur[44]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[45], piCur[45], shift,
                          piOrgLuma[size_t(45) << cShift]);   // temp = piOrg[45] - piCur[45]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[46], piCur[46], shift,
                          piOrgLuma[size_t(46) << cShift]);   // temp = piOrg[46] - piCur[46]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[47], piCur[47], shift,
                          piOrgLuma[size_t(47) << cShift]);   // temp = piOrg[47] - piCur[47]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[48], piCur[48], shift,
                          piOrgLuma[size_t(48) << cShift]);   // temp = piOrg[48] - piCur[48]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[49], piCur[49], shift,
                          piOrgLuma[size_t(49) << cShift]);   // temp = piOrg[49] - piCur[49]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[50], piCur[50], shift,
                          piOrgLuma[size_t(50) << cShift]);   // temp = piOrg[50] - piCur[50]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[51], piCur[51], shift,
                          piOrgLuma[size_t(51) << cShift]);   // temp = piOrg[51] - piCur[51]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[52], piCur[52], shift,
                          piOrgLuma[size_t(52) << cShift]);   // temp = piOrg[52] - piCur[52]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[53], piCur[53], shift,
                          piOrgLuma[size_t(53) << cShift]);   // temp = piOrg[53] - piCur[53]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[54], piCur[54], shift,
                          piOrgLuma[size_t(54) << cShift]);   // temp = piOrg[54] - piCur[54]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[55], piCur[55], shift,
                          piOrgLuma[size_t(55) << cShift]);   // temp = piOrg[55] - piCur[55]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[56], piCur[56], shift,
                          piOrgLuma[size_t(56) << cShift]);   // temp = piOrg[56] - piCur[56]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[57], piCur[57], shift,
                          piOrgLuma[size_t(57) << cShift]);   // temp = piOrg[57] - piCur[57]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[58], piCur[58], shift,
                          piOrgLuma[size_t(58) << cShift]);   // temp = piOrg[58] - piCur[58]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[59], piCur[59], shift,
                          piOrgLuma[size_t(59) << cShift]);   // temp = piOrg[59] - piCur[59]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[60], piCur[60], shift,
                          piOrgLuma[size_t(60) << cShift]);   // temp = piOrg[60] - piCur[60]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[61], piCur[61], shift,
                          piOrgLuma[size_t(61) << cShift]);   // temp = piOrg[61] - piCur[61]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[62], piCur[62], shift,
                          piOrgLuma[size_t(62) << cShift]);   // temp = piOrg[62] - piCur[62]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    sum += getWeightedMSE(rcDtParam.compID, piOrg[63], piCur[63], shift,
                          piOrgLuma[size_t(63) << cShift]);   // temp = piOrg[63] - piCur[63]; sum += Distortion((
                                                              // temp * temp ) >> shift);
    piOrg += strideOrg;
    piCur += strideCur;

    piOrgLuma += strideOrgLuma << cShiftY;
  }
  return (sum);
}
#endif


Pel orgCopy[MAX_CU_SIZE * MAX_CU_SIZE];

#if _OPENMP
#pragma omp threadprivate(orgCopy)
#endif

Distortion RdCost::xGetMRHADs( const DistParam &rcDtParam )
{
  const Pel offset = rcDtParam.org.meanDiff( rcDtParam.cur );

  PelBuf modOrg( orgCopy, rcDtParam.org );

  modOrg.copyFrom( rcDtParam.org );
  modOrg.subtract( offset );

  DistParam modDistParam = rcDtParam;
  modDistParam.org = modOrg;

  return m_distortionFunc[DFunc::HAD](modDistParam);
}

void RdCost::setDistParam(DistParam &rcDP, const CPelBuf &org, const Pel *piRefY, ptrdiff_t iRefStride, const Pel *mask,
                          ptrdiff_t iMaskStride, int stepX, ptrdiff_t iMaskStride2, int bitDepth, ComponentID compID)
{
  rcDP.bitDepth     = bitDepth;
  rcDP.compID       = compID;

  // set Original & Curr Pointer / Stride
  rcDP.org          = org;
  rcDP.cur.buf      = piRefY;
  rcDP.cur.stride   = iRefStride;

  // set Mask
  rcDP.mask         = mask;
  rcDP.maskStride   = iMaskStride;
  rcDP.stepX = stepX;
  rcDP.maskStride2 = iMaskStride2;

  // set Block Width / Height
  rcDP.cur.width    = org.width;
  rcDP.cur.height   = org.height;
  rcDP.maximumDistortionForEarlyExit = std::numeric_limits<Distortion>::max();

  // set Cost function for motion estimation with Mask
  rcDP.distFunc = m_distortionFunc[DFunc::SAD_WITH_MASK];
}

Distortion RdCost::xGetSADwMask( const DistParam& rcDtParam )
{
  if ( rcDtParam.applyWeight )
  {
    return RdCostWeightPrediction::xGetSADw( rcDtParam );
  }

  const Pel* org           = rcDtParam.org.buf;
  const Pel* cur           = rcDtParam.cur.buf;
  const Pel* mask          = rcDtParam.mask;
  const int  cols           = rcDtParam.org.width;
  int        rows           = rcDtParam.org.height;
  const int  subShift       = rcDtParam.subShift;
  const int  subStep        = ( 1 << subShift);
  const ptrdiff_t strideCur       = rcDtParam.cur.stride * subStep;
  const ptrdiff_t strideOrg       = rcDtParam.org.stride * subStep;
  const ptrdiff_t strideMask      = rcDtParam.maskStride * subStep;
  const int  stepX = rcDtParam.stepX;
  const ptrdiff_t strideMask2     = rcDtParam.maskStride2;
  const uint32_t distortionShift = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth);

  Distortion sum = 0;
  for (; rows != 0; rows -= subStep)
  {
    for (int n = 0; n < cols; n++)
    {
      sum += abs(org[n] - cur[n]) * *mask;
      mask += stepX;
    }
    org += strideOrg;
    cur += strideCur;
    mask += strideMask;
    mask += strideMask2;
  }
  sum <<= subShift;
  return (sum >> distortionShift );
}
//! \}
