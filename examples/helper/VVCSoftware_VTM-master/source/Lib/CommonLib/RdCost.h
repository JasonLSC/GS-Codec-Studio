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

/** \file     RdCost.h
    \brief    RD cost computation classes (header)
*/

#ifndef __RDCOST__
#define __RDCOST__

#include "CommonDef.h"
#include "Mv.h"
#include "Unit.h"
#include "Buffer.h"
#include "Slice.h"
#include "RdCostWeightPrediction.h"
#include <math.h>
#include <functional>

//! \ingroup CommonLib
//! \{

class DistParam;
class EncCfg;

// ====================================================================================================================
// Type definition
// ====================================================================================================================

using DistFunc = std::function<Distortion(const DistParam &)>;

#if WCG_EXT
class RdCost;
using DistFuncWtd = std::function<Distortion(const RdCost *, const DistParam &)>;
#endif
// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// distortion parameter class
class DistParam
{
public:
  CPelBuf               org;
  CPelBuf               cur;
#if WCG_EXT
  CPelBuf               orgLuma;
  DistFuncWtd           distFuncWtd;
#endif
  const Pel*            mask;
  ptrdiff_t             maskStride;
  int                   stepX;
  ptrdiff_t             maskStride2;
  int                   step;
  DistFunc              distFunc;
  int                   bitDepth;

  bool                  useMR;
  bool                  applyWeight;     // whether weighted prediction is used or not
  bool                  isBiPred;

  const WPScalingParam *wpCur;           // weighted prediction scaling parameters for current ref
  ComponentID           compID;
  Distortion            maximumDistortionForEarlyExit; /// During cost calculations, if distortion exceeds this value, cost calculations may early-terminate.

  // (vertical) subsampling shift (for reducing complexity)
  // - 0 = no subsampling, 1 = even rows, 2 = every 4th, etc.
  int                   subShift;
  int                   cShiftX;
  int                   cShiftY;
  DistParam() :
  org(), cur(),
  mask( nullptr ),
  maskStride( 0 ),
  stepX(0),
  maskStride2(0),
  step( 1 ), bitDepth( 0 ), useMR( false ), applyWeight( false ), isBiPred( false ), wpCur( nullptr ), compID( MAX_NUM_COMPONENT ), maximumDistortionForEarlyExit( std::numeric_limits<Distortion>::max() ), subShift( 0 )
  , cShiftX(-1), cShiftY(-1)
  { }
};

/// RD cost computation class
class RdCost
{
private:
  // for distortion

  static EnumArray<DistFunc, DFunc> m_distortionFunc;
  CostMode                m_costMode;
  double                  m_distortionWeight[MAX_NUM_COMPONENT]; // only chroma values are used.
  double                  m_dLambda;
  bool                    m_isLosslessRDCost;

#if WCG_EXT
  static EnumArray<DistFuncWtd, DFuncWtd> m_distortionFuncWtd;
  double                  m_dLambda_unadjusted; // TODO: check is necessary
  double                  m_distScaleUnadjusted;

  std::vector<int32_t> m_reshapeLumaLevelToWeightPLUT;   // scaled by MSE_WEIGHT_ONE
  std::vector<double>  m_lumaLevelToWeightPLUT;

  int32_t  m_chromaWeight = RESHAPE_SIGNAL_NULL;   // scaled by MSE_WEIGHT_ONE
  uint32_t m_signalType = MSE_WEIGHT_ONE;
  int      m_lumaBD = 10;

  ChromaFormat            m_cf;
#endif
  double                  m_distScale;
  double                  m_dLambdaMotionSAD;
  double                  m_lambdaStore[2][3];   // 0-org; 1-act
  double                  m_distScaleStore[2][3];   // 0-org; 1-act
  bool                    m_resetStore;
  int                     m_pairCheck;

  // for motion cost
  Mv                      m_mvPredictor;
  Mv                      m_bvPredictors[2];
  double                  m_motionLambda;
  int                     m_iCostScale;

  double                  m_dCost; // for ibc

  template<bool allowOddSizes> static DFuncDiff sizeOffset(const uint32_t width)
  {
    if (width <= 64 && isPowerOf2(width))
    {
      return static_cast<DFuncDiff>(floorLog2(width));
    }
    else if (width % 16 == 0)
    {
      return DFunc::SAD16N - DFunc::SAD;
    }
    else if (allowOddSizes && width == 12)
    {
      return DFunc::SAD12 - DFunc::SAD;
    }
    else if (allowOddSizes && width == 24)
    {
      return DFunc::SAD24 - DFunc::SAD;
    }
    else if (allowOddSizes && width == 48)
    {
      return DFunc::SAD48 - DFunc::SAD;
    }
    else
    {
      return DFunc::SAD - DFunc::SAD;
    }
  }

public:
  RdCost();
  virtual ~RdCost();

#if WCG_EXT
  void          setChromaFormat       ( const ChromaFormat & _cf) { m_cf = _cf; }
  double        calcRdCost            ( uint64_t fracBits, Distortion distortion, bool useUnadjustedLambda = true );
#else
  double        calcRdCost            ( uint64_t fracBits, Distortion distortion );
#endif

  void          setDistortionWeight   ( const ComponentID compID, const double distortionWeight ) { m_distortionWeight[compID] = distortionWeight; }
  void          setLambda             ( double dLambda, const BitDepths &bitDepths );

#if WCG_EXT
  double        getLambda( bool unadj = false )
                                      { return unadj ? m_dLambda_unadjusted : m_dLambda; }
#else
  double        getLambda()           { return m_dLambda; }
#endif
  double        getChromaWeight()     { return ((m_distortionWeight[COMPONENT_Cb] + m_distortionWeight[COMPONENT_Cr]) / 2.0); }
#if RDOQ_CHROMA_LAMBDA
  double        getDistortionWeight   ( const ComponentID compID ) const { return m_distortionWeight[compID % MAX_NUM_COMPONENT]; }
#endif

  void          setCostMode(CostMode m) { m_costMode = m; }
  void          setLosslessRDCost(bool m) { m_isLosslessRDCost = m; }

  // Distortion Functions
  void          init();
#ifdef TARGET_SIMD_X86
  void          initRdCostX86();
  template <X86_VEXT vext>
  void          _initRdCostX86();
#endif

  void setDistParam(DistParam &rcDP, const CPelBuf &org, const Pel *piRefY, ptrdiff_t iRefStride, int bitDepth,
                    ComponentID compID, int subShiftMode = 0, int step = 1, bool useHadamard = false);
  void           setDistParam( DistParam &rcDP, const CPelBuf &org, const CPelBuf &cur, int bitDepth, ComponentID compID, bool useHadamard = false );
  void setDistParam(DistParam &rcDP, const Pel *pOrg, const Pel *piRefY, ptrdiff_t iOrgStride, ptrdiff_t iRefStride,
                    int bitDepth, ComponentID compID, int width, int height, int subShiftMode = 0, int step = 1,
                    bool useHadamard = false, bool bioApplied = false);
  void setDistParam(DistParam &rcDP, const CPelBuf &org, const Pel *piRefY, ptrdiff_t iRefStride, const Pel *mask,
                    ptrdiff_t iMaskStride, int stepX, ptrdiff_t iMaskStride2, int bitDepth, ComponentID compID);

  double         getMotionLambda          ( )  { return m_dLambdaMotionSAD; }
  void           selectMotionLambda       ( )  { m_motionLambda = getMotionLambda( ); }
  void           setPredictor             ( const Mv& rcMv )
  {
    m_mvPredictor = rcMv;
  }
  void           setCostScale             ( int iCostScale )           { m_iCostScale = iCostScale; }
  Distortion     getCost                  ( uint32_t b )                   { return Distortion( m_motionLambda * b ); }
  // for ibc
  void           getMotionCost(int add) { m_dCost = m_dLambdaMotionSAD + add; }

  void    setPredictors(Mv* pcMv)
  {
    for (int i = 0; i<2; i++)
    {
      m_bvPredictors[i] = pcMv[i];
    }
  }

  inline Distortion getBvCostMultiplePreds(int x, int y, bool useIMV)
  {
    return Distortion(m_dCost * getBitsMultiplePreds(x, y, useIMV));
  }

  unsigned int    getBitsMultiplePreds(int x, int y, bool useIMV)
  {
    int rmvH[2];
    int rmvV[2];
    rmvH[0] = x - m_bvPredictors[0].getHor();
    rmvH[1] = x - m_bvPredictors[1].getHor();

    rmvV[0] = y - m_bvPredictors[0].getVer();
    rmvV[1] = y - m_bvPredictors[1].getVer();
    int absCand[2];
    absCand[0] = abs(rmvH[0]) + abs(rmvV[0]);
    absCand[1] = abs(rmvH[1]) + abs(rmvV[1]);

    int rmvHQP[2];
    int rmvVQP[2];
    if (x % 4 == 0 && y % 4 == 0 && useIMV)
    {
      int imvShift = 2;
      int offset = 1 << (imvShift - 1);

      rmvHQP[0] = (x >> 2) - ((m_bvPredictors[0].getHor() + offset) >> 2);
      rmvHQP[1] = (x >> 2) - ((m_bvPredictors[1].getHor() + offset) >> 2);
      rmvVQP[0] = (y >> 2) - ((m_bvPredictors[0].getVer() + offset) >> 2);
      rmvVQP[1] = (y >> 2) - ((m_bvPredictors[1].getVer() + offset) >> 2);

      int absCandQP[2];
      absCandQP[0] = abs(rmvHQP[0]) + abs(rmvVQP[0]);
      absCandQP[1] = abs(rmvHQP[1]) + abs(rmvVQP[1]);
      unsigned int candBits0QP, candBits1QP;
      if (absCand[0] < absCand[1])
      {
        unsigned int candBits0 = getIComponentBits(rmvH[0]) + getIComponentBits(rmvV[0]);
        if (absCandQP[0] < absCandQP[1])
        {
          candBits0QP = getIComponentBits(rmvHQP[0]) + getIComponentBits(rmvVQP[0]);
          return candBits0QP <candBits0 ? candBits0QP : candBits0;
        }
        else
        {
          candBits1QP = getIComponentBits(rmvHQP[1]) + getIComponentBits(rmvVQP[1]);
          return candBits1QP < candBits0 ? candBits1QP : candBits0;
        }
      }
      else
      {
        unsigned int candBits1 = getIComponentBits(rmvH[1]) + getIComponentBits(rmvV[1]);
        if (absCandQP[0] < absCandQP[1])
        {
          candBits0QP = getIComponentBits(rmvHQP[0]) + getIComponentBits(rmvVQP[0]);
          return candBits0QP < candBits1 ? candBits0QP : candBits1;
        }
        else
        {
          candBits1QP = getIComponentBits(rmvHQP[1]) + getIComponentBits(rmvVQP[1]);
          return candBits1QP < candBits1 ? candBits1QP : candBits1;
        }
      }
    }
    else

    {
      if (absCand[0] < absCand[1])
      {
        return getIComponentBits(rmvH[0]) + getIComponentBits(rmvV[0]);
      }
      else
      {
        return getIComponentBits(rmvH[1]) + getIComponentBits(rmvV[1]);
      }
    }
  }

  unsigned int getIComponentBits(int val)
  {
    if (!val) return 1;

    unsigned int length = 1;
    unsigned int temp = (val <= 0) ? (-val << 1) + 1 : (val << 1);

    while (1 != temp)
    {
      temp >>= 1;
      length += 2;
    }

    return length;
  }

  // for motion cost
  static uint32_t    xGetExpGolombNumberOfBits( int iVal )
  {
    CHECKD( iVal == std::numeric_limits<int>::min(), "Wrong value" );
    unsigned uiLength2 = 1, uiTemp2 = ( iVal <= 0 ) ? ( unsigned( -iVal ) << 1 ) + 1 : unsigned( iVal << 1 );

    while( uiTemp2 > MAX_CU_SIZE )
    {
      uiLength2 += ( MAX_CU_DEPTH << 1 );
      uiTemp2  >>=   MAX_CU_DEPTH;
    }

    return uiLength2 + ( floorLog2(uiTemp2) << 1 );
  }
  Distortion     getCostOfVectorWithPredictor( const int x, const int y, const unsigned imvShift )  { return Distortion( m_motionLambda * getBitsOfVectorWithPredictor(x, y, imvShift )); }
  uint32_t       getBitsOfVectorWithPredictor(const int x, const int y, const unsigned imvShift)
  {
    return xGetExpGolombNumberOfBits(((x * (1 << m_iCostScale)) - m_mvPredictor.getHor()) >> imvShift)
           + xGetExpGolombNumberOfBits(((y * (1 << m_iCostScale)) - m_mvPredictor.getVer()) >> imvShift);
  }
#if WCG_EXT
  void saveUnadjustedLambda();
  void initLumaLevelToWeightTable(int bitDepth);
  void initLumaLevelToWeightTableReshape();
  void updateReshapeLumaLevelToWeightTableChromaMD(std::vector<Pel> &ILUT);
  void restoreReshapeLumaLevelToWeightTable();
  void updateReshapeLumaLevelToWeightTable(SliceReshapeInfo &sliceReshape, Pel *wtTable, double cwt);

  void setReshapeInfo(uint32_t type, int lumaBD)
  {
    m_signalType = type;
    m_lumaBD     = lumaBD;
  }

  double               getWPSNRLumaLevelWeight(int val) { return m_lumaLevelToWeightPLUT[val]; }
  std::vector<double> &getLumaLevelWeightTable() { return m_lumaLevelToWeightPLUT; }
#endif

  void           lambdaAdjustColorTrans(bool forward, ComponentID compID, bool applyChromaScale = false,
                                        int *resScaleInv = nullptr);
  void           resetStore() { m_resetStore = true; }

private:

  static Distortion xGetSSE           ( const DistParam& pcDtParam );
  static Distortion xGetSSE4          ( const DistParam& pcDtParam );
  static Distortion xGetSSE8          ( const DistParam& pcDtParam );
  static Distortion xGetSSE16         ( const DistParam& pcDtParam );
  static Distortion xGetSSE32         ( const DistParam& pcDtParam );
  static Distortion xGetSSE64         ( const DistParam& pcDtParam );
  static Distortion xGetSSE16N        ( const DistParam& pcDtParam );

#if WCG_EXT
  Distortion getWeightedMSE(const int compIdx, const Pel org, const Pel cur, const uint32_t shift, const Pel orgLuma) const;
  Distortion xGetSSE_WTD       ( const DistParam& pcDtParam ) const;
  Distortion xGetSSE2_WTD      ( const DistParam& pcDtParam ) const;
  Distortion xGetSSE4_WTD      ( const DistParam& pcDtParam ) const;
  Distortion xGetSSE8_WTD      ( const DistParam& pcDtParam ) const;
  Distortion xGetSSE16_WTD     ( const DistParam& pcDtParam ) const;
  Distortion xGetSSE32_WTD     ( const DistParam& pcDtParam ) const;
  Distortion xGetSSE64_WTD     ( const DistParam& pcDtParam ) const;
  Distortion xGetSSE16N_WTD    ( const DistParam& pcDtParam ) const;
#endif

  static Distortion xGetSAD           ( const DistParam& pcDtParam );
  static Distortion xGetSAD4          ( const DistParam& pcDtParam );
  static Distortion xGetSAD8          ( const DistParam& pcDtParam );
  static Distortion xGetSAD16         ( const DistParam& pcDtParam );
  static Distortion xGetSAD32         ( const DistParam& pcDtParam );
  static Distortion xGetSAD64         ( const DistParam& pcDtParam );
  static Distortion xGetSAD16N        ( const DistParam& pcDtParam );

  static Distortion xGetSAD12         ( const DistParam& pcDtParam );
  static Distortion xGetSAD24         ( const DistParam& pcDtParam );
  static Distortion xGetSAD48         ( const DistParam& pcDtParam );

  static Distortion xGetSAD_full      ( const DistParam& pcDtParam );
  static Distortion xGetSADwMask      ( const DistParam& pcDtParam );

  static Distortion xGetMRSAD         ( const DistParam& pcDtParam );
  static Distortion xGetMRSAD4        ( const DistParam& pcDtParam );
  static Distortion xGetMRSAD8        ( const DistParam& pcDtParam );
  static Distortion xGetMRSAD16       ( const DistParam& pcDtParam );
  static Distortion xGetMRSAD32       ( const DistParam& pcDtParam );
  static Distortion xGetMRSAD64       ( const DistParam& pcDtParam );
  static Distortion xGetMRSAD16N      ( const DistParam& pcDtParam );
  static Distortion xGetMRSAD12       ( const DistParam& pcDtParam );
  static Distortion xGetMRSAD24       ( const DistParam& pcDtParam );
  static Distortion xGetMRSAD48       ( const DistParam& pcDtParam );
  static Distortion xGetMRHADs        ( const DistParam& pcDtParam );

  static Distortion xGetHADs          ( const DistParam& pcDtParam );
  static Distortion xCalcHADs2x2(const Pel *piOrg, const Pel *piCurr, ptrdiff_t strideOrg, ptrdiff_t strideCur,
                                 int step);
  static Distortion xCalcHADs4x4(const Pel *piOrg, const Pel *piCurr, ptrdiff_t strideOrg, ptrdiff_t strideCur,
                                 int step);
  static Distortion xCalcHADs8x8(const Pel *piOrg, const Pel *piCurr, ptrdiff_t strideOrg, ptrdiff_t strideCur,
                                 int step);

  static Distortion xCalcHADs16x8(const Pel *piOrg, const Pel *piCur, ptrdiff_t strideOrg, ptrdiff_t strideCur);
  static Distortion xCalcHADs8x16(const Pel *piOrg, const Pel *piCur, ptrdiff_t strideOrg, ptrdiff_t strideCur);
  static Distortion xCalcHADs4x8(const Pel *piOrg, const Pel *piCur, ptrdiff_t strideOrg, ptrdiff_t strideCur);
  static Distortion xCalcHADs8x4(const Pel *piOrg, const Pel *piCur, ptrdiff_t strideOrg, ptrdiff_t strideCur);

#ifdef TARGET_SIMD_X86
  template<X86_VEXT vext>
  static Distortion xGetSSE_SIMD    ( const DistParam& pcDtParam );
  template<int width, X86_VEXT vext> static Distortion xGetSSE_NxN_SIMD(const DistParam &pcDtParam);
#if RExt__HIGH_BIT_DEPTH_SUPPORT
  template<X86_VEXT vext>
  static Distortion xGetSSE_HBD_SIMD(const DistParam& pcDtParam);
#endif

  template<X86_VEXT vext>
  static Distortion xGetSAD_SIMD    ( const DistParam& pcDtParam );
  template<int width, X86_VEXT vext> static Distortion xGetSAD_NxN_SIMD(const DistParam &pcDtParam);
  template<X86_VEXT vext>
  static Distortion xGetSAD_IBD_SIMD( const DistParam& pcDtParam );
#if RExt__HIGH_BIT_DEPTH_SUPPORT
  template<X86_VEXT vext>
  static Distortion xGetHADs_HBD_SIMD(const DistParam& pcDtParam);
#else
  template<X86_VEXT vext>
  static Distortion xGetHADs_SIMD   ( const DistParam& pcDtParam );
#endif

  template< X86_VEXT vext >
  static Distortion xGetSADwMask_SIMD( const DistParam& pcDtParam );
#if RExt__HIGH_BIT_DEPTH_SUPPORT
  template<X86_VEXT vext>
  static Distortion xGetSAD_HBD_SIMD(const DistParam& pcDtParam);

  template< X86_VEXT vext >
  static Distortion xGetSADwMask_HBD_SIMD(const DistParam& pcDtParam);
#endif
#endif

public:

#if WCG_EXT
  Distortion getDistPart(const CPelBuf &org, const CPelBuf &cur, int bitDepth, const ComponentID compID, DFuncWtd distFuncWtd,
                         const CPelBuf &orgLuma) const;
#endif
  Distortion getDistPart(const CPelBuf &org, const CPelBuf &cur, int bitDepth, const ComponentID compID,
                         DFunc distFunc) const;

  Distortion getDistPart(const CPelBuf &org, const CPelBuf &cur, const Pel *mask, int bitDepth,
                         const ComponentID compID, DFunc distFunc);
};// END CLASS DEFINITION RdCost

//! \}

#endif // __RDCOST__
