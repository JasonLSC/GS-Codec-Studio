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

/** \file     EncAdaptiveLoopFilter.cpp
 \brief    estimation part of adaptive loop filter class
 */
#include "EncAdaptiveLoopFilter.h"

#include "CommonLib/Picture.h"
#include "CommonLib/CodingStructure.h"

#define AlfCtx(c) SubCtx( Ctx::Alf, c)

#include <algorithm>

#if MAX_NUM_CC_ALF_FILTERS>1
struct FilterIdxCount
{
  uint64_t count;
  uint8_t filterIdx;
};

bool compareCounts(FilterIdxCount a, FilterIdxCount b) { return a.count > b.count; }
#endif

inline double essentiallyEqual(double a, double b)
{
  constexpr double REL_EPSILON = 0x1p-20;   // 2^-20 or about 1e-6
  constexpr double ABS_EPSILON = 0x1p-30;   // 2^-30 or about 1e-9
  return std::abs(a - b) < std::max(ABS_EPSILON, REL_EPSILON * std::max(std::abs(a), std::abs(b)));
}

void AlfCovariance::getClipMax(const AlfFilterShape& alfShape, AlfClipIdx* clip_max) const
{
  for( int k = 0; k < numCoeff-1; ++k )
  {
    clip_max[k] = 0;

    bool inc = true;
    while (inc && clip_max[k] + 1 < numBins && essentiallyEqual(y(clip_max[k] + 1, k), y(clip_max[k], k)))
    {
      for( int l = 0; inc && l < numCoeff; ++l )
      {
        if (!essentiallyEqual(E(clip_max[k], 0, k, l), E(clip_max[k] + 1, 0, k, l)))
        {
          inc = false;
        }
      }
      if( inc )
      {
        ++clip_max[k];
      }
    }
  }
  clip_max[numCoeff-1] = 0;
}

void AlfCovariance::reduceClipCost(const AlfFilterShape& alfShape, AlfClipIdx* clip) const
{
  for( int k = 0; k < numCoeff-1; ++k )
  {
    bool dec = true;
    while (dec && clip[k] > 0 && essentiallyEqual(y(clip[k] - 1, k), y(clip[k], k)))
    {
      for( int l = 0; dec && l < numCoeff; ++l )
      {
        if (!essentiallyEqual(E(clip[k], clip[l], k, l), E(clip[k] - 1, clip[l], k, l)))
        {
          dec = false;
        }
      }
      if( dec )
      {
        --clip[k];
      }
    }
  }
}

double AlfCovariance::optimizeFilter(const AlfFilterShape& alfShape, AlfClipIdx* clip, double* f,
                                     bool optimizeClip) const
{
  const int size = alfShape.numCoeff;
  AlfClipIdx clip_max[MAX_NUM_ALF_LUMA_COEFF];

  double err_best, err_last;

  TE kE;
  Ty ky;

  if (optimizeClip)
  {
    // Start by looking for min clipping that has no impact => max_clipping
    getClipMax(alfShape, clip_max);
    for (int k=0; k<size; ++k)
    {
      clip[k] = std::max(clip_max[k], clip[k]);
      clip[k] = std::min(clip[k], AlfClipIdx(numBins - 1));
    }
  }

  setEyFromClip( clip, kE, ky, size );

  gnsSolveByChol( kE, ky, f, size );
  err_best = calculateError( clip, f, size );

  int step = optimizeClip ? (numBins + 1) / 2 : 0;

  while( step > 0 )
  {
    double err_min = err_best;
    int idx_min = -1;
    int inc_min = 0;

    for( int k = 0; k < size-1; ++k )
    {
      if( clip[k] - step >= clip_max[k] )
      {
        clip[k] -= step;
        ky[k] = y(clip[k], k);
        for( int l = 0; l < size; l++ )
        {
          const double val = E(clip[k], clip[l], k, l);
          kE[k][l]         = val;
          kE[l][k]         = val;
        }

        gnsSolveByChol( kE, ky, f, size );
        err_last = calculateError( clip, f, size );

        if( err_last < err_min )
        {
          err_min = err_last;
          idx_min = k;
          inc_min = -step;
        }
        clip[k] += step;
      }
      if( clip[k] + step < numBins )
      {
        clip[k] += step;
        ky[k] = y(clip[k], k);
        for( int l = 0; l < size; l++ )
        {
          const double val = E(clip[k], clip[l], k, l);
          kE[k][l]         = val;
          kE[l][k]         = val;
        }

        gnsSolveByChol( kE, ky, f, size );
        err_last = calculateError( clip, f, size );

        if( err_last < err_min )
        {
          err_min = err_last;
          idx_min = k;
          inc_min = step;
        }
        clip[k] -= step;

      }
      ky[k] = y(clip[k], k);
      for( int l = 0; l < size; l++ )
      {
        const double val = E(clip[k], clip[l], k, l);
        kE[k][l]         = val;
        kE[l][k]         = val;
      }
    }

    if( idx_min >= 0 )
    {
      err_best = err_min;
      clip[idx_min] += inc_min;
      ky[idx_min] = y(clip[idx_min], idx_min);
      for( int l = 0; l < size; l++ )
      {
        const double val = E(clip[idx_min], clip[l], idx_min, l);
        kE[idx_min][l]   = val;
        kE[l][idx_min]   = val;
      }
    }
    else
    {
      --step;
    }
  }

  if (optimizeClip)
  {
    // test all max
    for( int k = 0; k < size-1; ++k )
    {
      clip_max[k] = 0;
    }
    TE kE_max;
    Ty ky_max;
    setEyFromClip( clip_max, kE_max, ky_max, size );

    gnsSolveByChol( kE_max, ky_max, f, size );
    err_last = calculateError( clip_max, f, size );
    if( err_last < err_best )
    {
      err_best = err_last;
      for (int k=0; k<size; ++k)
      {
        clip[k] = clip_max[k];
      }
    }
    else
    {
      // update clip to reduce coding cost
      reduceClipCost(alfShape, clip);

      // update f with best solution
      gnsSolveByChol( kE, ky, f, size );
    }
  }

  return err_best;
}

void AlfCovariance::calcInitErrorForCoeffs(double *cAc, double *cA, double *bc,  const AlfClipIdx *clip, const AlfCoeff *coeff, const int numCoeff, const int fractionalBits ) const
{
  const double factor = 1.0 / (1 << fractionalBits);

  *cAc = 0;
  *bc = 0;

  for (ptrdiff_t i = 0; i < numCoeff; i++)   // diagonal
  {
    double sum = 0;
    for (ptrdiff_t j = 0; j < numCoeff; j++)
    {
      sum += E(clip[i],clip[j],i,j) * coeff[j];
    }
    (*cAc) += sum * coeff[i];
    cA[i] = 2*sum;
    *bc += 2*coeff[i] * y(clip[i],i);
  }

  *cAc *= factor * factor;
  for (ptrdiff_t i = 0; i < numCoeff; i++)   // diagonal
  {
    cA[i] *= factor;
  }

  *bc *= factor;
}
void AlfCovariance::updateErrorForCoeffsDelta(double *cAc, double *cA, double *bc,  const AlfClipIdx *clip, const AlfCoeff *coeff, const int numCoeff, double cDelta, int modInd  ) const
{
  *cAc = (*cAc) + cDelta * cA[modInd] + cDelta * cDelta * E( clip[modInd], clip[modInd], modInd, modInd);
  for (int i = 0; i < numCoeff; i++) {
    cA[i] += 2 * cDelta * E( clip[modInd], clip[i], modInd, i);
  }
  (*bc) = (*bc) + 2 * y(clip[modInd], modInd) * cDelta;
}
double AlfCovariance::calcErrorForCoeffsDelta(double cAc, double *cA, double bc,  const AlfClipIdx *clip, const AlfCoeff *coeff, const int numCoeff, double cDelta, int modInd ) const
{
  double error;
  error = cAc - bc;
  error += cDelta * cA[modInd];
  error += cDelta * cDelta * E(clip[modInd],clip[modInd],modInd,modInd);
  error -= 2 * y(clip[modInd],modInd) * cDelta;

  return(error);
}

double AlfCovariance::calcErrorForCoeffs(const AlfClipIdx* clip, const AlfCoeff* coeff, const int numCoeff,
                                         const int fractionalBits) const
{
  const double factor = 1 << fractionalBits;

  double error = 0;

  for( int i = 0; i < numCoeff; i++ )   //diagonal
  {
    double sum = 0;
    for( int j = i + 1; j < numCoeff; j++ )
    {
      // E[j][i] = E[i][j], sum will be multiplied by 2 later
      sum += E(clip[i], clip[j], i, j) * coeff[j];
    }
    error += ((E(clip[i], clip[i], i, i) * coeff[i] + sum * 2) / factor - 2 * y(clip[i], i)) * coeff[i];
  }

  return error / factor;
}

double AlfCovariance::calcErrorForCcAlfCoeffs(const int16_t *coeff, const int numCoeff, const int bitDepth) const
{
  double factor = 1 << (bitDepth - 1);
  double error = 0;

  for (int i = 0; i < numCoeff; i++)   // diagonal
  {
    double sum = 0;
    for (int j = i + 1; j < numCoeff; j++)
    {
      // E[j][i] = E[i][j], sum will be multiplied by 2 later
      sum += E(0, 0, i, j) * coeff[j];
    }
    error += ((E(0, 0, i, i) * coeff[i] + sum * 2) / factor - 2 * y(0, i)) * coeff[i];
  }

  return error / factor;
}

double AlfCovariance::calculateError(const AlfClipIdx* clip, const double* coeff, const int numCoeff) const
{
  double sum = 0;
  for( int i = 0; i < numCoeff; i++ )
  {
    sum += coeff[i] * y(clip[i], i);
  }

  return pixAcc - sum;
}

double AlfCovariance::calculateError(const AlfClipIdx* clip) const
{
  Ty c;

  return optimizeFilter( clip, c, numCoeff );
}
//********************************
// Cholesky decomposition
//********************************

#define ROUND(a)  (((a) < 0)? (int)((a) - 0.5) : (int)((a) + 0.5))
#define REG              0.0001
#define REG_SQR          0.0000001

//Find filter coeff related
int AlfCovariance::gnsCholeskyDec( TE inpMatr, TE outMatr, int numEq ) const
{
  Ty invDiag;  /* Vector of the inverse of diagonal entries of outMatr */

  for( int i = 0; i < numEq; i++ )
  {
    for( int j = i; j < numEq; j++ )
    {
      /* Compute the scaling factor */
      double scale = inpMatr[i][j];
      if( i > 0 )
      {
        for( int k = i - 1; k >= 0; k-- )
        {
          scale -= outMatr[k][j] * outMatr[k][i];
        }
      }

      /* Compute i'th row of outMatr */
      if( i == j )
      {
        if( scale <= REG_SQR ) // if(scale <= 0 )  /* If inpMatr is singular */
        {
          return 0;
        }
        else              /* Normal operation */
        {
          invDiag[i] = 1.0 / ( outMatr[i][i] = sqrt( scale ) );
        }
      }
      else
      {
        outMatr[i][j] = scale * invDiag[i]; /* Upper triangular part          */
        outMatr[j][i] = 0.0;              /* Lower triangular part set to 0 */
      }
    }
  }
  return 1; /* Signal that Cholesky factorization is successfully performed */
}

void AlfCovariance::gnsTransposeBacksubstitution( TE U, double* rhs, double* x, int order ) const
{
  /* Backsubstitution starts */
  x[0] = rhs[0] / U[0][0];               /* First row of U'                   */
  for( int i = 1; i < order; i++ )
  {         /* For the rows 1..order-1           */

    double sum = 0; //Holds backsubstitution from already handled rows

    for( int j = 0; j < i; j++ ) /* Backsubst already solved unknowns */
    {
      sum += x[j] * U[j][i];
    }

    x[i] = ( rhs[i] - sum ) / U[i][i];       /* i'th component of solution vect.  */
  }
}

void AlfCovariance::gnsBacksubstitution( TE R, double* z, int size, double* A ) const
{
  size--;
  A[size] = z[size] / R[size][size];

  for( int i = size - 1; i >= 0; i-- )
  {
    double sum = 0;

    for( int j = i + 1; j <= size; j++ )
    {
      sum += R[i][j] * A[j];
    }

    A[i] = ( z[i] - sum ) / R[i][i];
  }
}

int AlfCovariance::gnsSolveByChol(const AlfClipIdx* clip, double* x, int numEq) const
{
  TE LHS;
  Ty rhs;

  setEyFromClip( clip, LHS, rhs, numEq );
  return gnsSolveByChol( LHS, rhs, x, numEq );
}

int AlfCovariance::gnsSolveByChol( TE LHS, double* rhs, double *x, int numEq ) const
{
  Ty aux;     /* Auxiliary vector */
  TE U;    /* Upper triangular Cholesky factor of LHS */

  int res = 1;  // Signal that Cholesky factorization is successfully performed

                /* The equation to be solved is LHSx = rhs */

                /* Compute upper triangular U such that U'*U = LHS */
  if( gnsCholeskyDec( LHS, U, numEq ) ) /* If Cholesky decomposition has been successful */
  {
    /* Now, the equation is  U'*U*x = rhs, where U is upper triangular
    * Solve U'*aux = rhs for aux
    */
    gnsTransposeBacksubstitution( U, rhs, aux, numEq );

    /* The equation is now U*x = aux, solve it for x (new motion coefficients) */
    gnsBacksubstitution( U, aux, numEq, x );

  }
  else /* LHS was singular */
  {
    res = 0;

    /* Regularize LHS */
    for( int i = 0; i < numEq; i++ )
    {
      LHS[i][i] += REG;
    }

    /* Compute upper triangular U such that U'*U = regularized LHS */
    res = gnsCholeskyDec( LHS, U, numEq );

    if( !res )
    {
      std::memset( x, 0, sizeof( double )*numEq );
      return 0;
    }

    /* Solve  U'*aux = rhs for aux */
    gnsTransposeBacksubstitution( U, rhs, aux, numEq );

    /* Solve U*x = aux for x */
    gnsBacksubstitution( U, aux, numEq, x );
  }
  return res;
}
//////////////////////////////////////////////////////////////////////////////////////////

EncAdaptiveLoopFilter::EncAdaptiveLoopFilter()
  : m_CABACEstimator( nullptr )
{
  for( int i = 0; i < MAX_NUM_COMPONENT; i++ )
  {
    m_alfCovariance[i] = nullptr;
  }
  m_alfCovarianceFrame.fill(nullptr);
  m_filterCoeffSet = nullptr;
  m_filterClippSet = nullptr;

  m_alfCovarianceCcAlf[0] = nullptr;
  m_alfCovarianceCcAlf[1] = nullptr;
  m_alfCovarianceFrameCcAlf[0] = nullptr;
  m_alfCovarianceFrameCcAlf[1] = nullptr;
}

void EncAdaptiveLoopFilter::create(const EncCfg* encCfg, const int picWidth, const int picHeight,
                                   const ChromaFormat chromaFormatIdc, const int maxCUWidth, const int maxCUHeight,
                                   const int maxCUDepth, const BitDepths& inputBitDepth,
                                   const BitDepths& internalBitDepth)
{
  AdaptiveLoopFilter::create(picWidth, picHeight, chromaFormatIdc, maxCUWidth, maxCUHeight, maxCUDepth, inputBitDepth);
  CHECK( encCfg == nullptr, "encCfg must not be null" );
  m_encCfg = encCfg;

  for (const auto chType: { ChannelType::LUMA, ChannelType::CHROMA })
  {
    const int numClasses = isLuma(chType) ? MAX_NUM_ALF_CLASSES : ALF_MAX_NUM_ALTERNATIVES_CHROMA;

    m_alfCovarianceFrame[chType] = new AlfCovariance *[m_filterShapes[chType].size()];
    for (int i = 0; i != m_filterShapes[chType].size(); i++)
    {
      m_alfCovarianceFrame[chType][i] = new AlfCovariance[numClasses];
      for( int k = 0; k < numClasses; k++ )
      {
        m_alfCovarianceFrame[chType][i][k].create(m_filterShapes[chType][i].numCoeff, ALF_NUM_CLIP_VALS[chType]);
      }
    }
  }

  for( int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++ )
  {
    ChannelType chType = toChannelType( ComponentID( compIdx ) );
    int numClasses = compIdx ? 1 : MAX_NUM_ALF_CLASSES;

    m_alfCovariance[compIdx] = new AlfCovariance **[m_filterShapes[chType].size()];

    for (int i = 0; i != m_filterShapes[chType].size(); i++)
    {
      m_alfCovariance[compIdx][i] = new AlfCovariance*[m_numCTUsInPic];
      for( int j = 0; j < m_numCTUsInPic; j++ )
      {
        m_alfCovariance[compIdx][i][j] = new AlfCovariance[numClasses];
        for( int k = 0; k < numClasses; k++ )
        {
          m_alfCovariance[compIdx][i][j][k].create(m_filterShapes[chType][i].numCoeff, ALF_NUM_CLIP_VALS[chType]);
        }
      }
    }
  }

  for (int i = 0; i != m_filterShapes[ChannelType::LUMA].size(); i++)
  {
    for (int j = 0; j <= MAX_NUM_ALF_CLASSES + 1; j++)
    {
      m_alfCovarianceMerged[i][j].create(m_filterShapes[ChannelType::LUMA][i].numCoeff,
                                         ALF_NUM_CLIP_VALS[ChannelType::LUMA]);
    }
  }

  m_filterCoeffSet  = new AlfCoeff*[std::max(MAX_NUM_ALF_CLASSES, ALF_MAX_NUM_ALTERNATIVES_CHROMA)];
  m_filterClippSet  = new AlfClipIdx*[std::max(MAX_NUM_ALF_CLASSES, ALF_MAX_NUM_ALTERNATIVES_CHROMA)];

  for( int i = 0; i < MAX_NUM_ALF_CLASSES; i++ )
  {
    m_filterCoeffSet[i]  = new AlfCoeff[MAX_NUM_ALF_LUMA_COEFF];
    m_filterClippSet[i]  = new AlfClipIdx[MAX_NUM_ALF_LUMA_COEFF];
  }


  m_ctbDistortionFixedFilter = new double[m_numCTUsInPic];
  for (int comp = 0; comp < MAX_NUM_COMPONENT; comp++)
  {
    m_ctbDistortionUnfilter[comp] = new double[m_numCTUsInPic];
    m_indexTmpVec[comp].resize(m_numCTUsInPic);
    m_indexTmp[comp] = m_indexTmpVec[comp].data();
  }
  memset(m_clipDefaultEnc, 0, sizeof(m_clipDefaultEnc));
  m_apsIdCcAlfStart[0] = MAX_NUM_APS(ApsType::ALF);
  m_apsIdCcAlfStart[1] = MAX_NUM_APS(ApsType::ALF);
  for( int compIdx = 1; compIdx < MAX_NUM_COMPONENT; compIdx++ )
  {
    m_alfCovarianceCcAlf[compIdx - 1]      = new AlfCovariance*[m_filterShapesCcAlf[compIdx - 1].size()];
    m_alfCovarianceFrameCcAlf[compIdx - 1] = new AlfCovariance[m_filterShapesCcAlf[compIdx - 1].size()];
    for( int i = 0; i != m_filterShapesCcAlf[compIdx-1].size(); i++ )
    {
      m_alfCovarianceFrameCcAlf[compIdx - 1][i].create(m_filterShapesCcAlf[compIdx - 1][i].numCoeff, 1);

      m_alfCovarianceCcAlf[compIdx - 1][i] = new AlfCovariance[m_numCTUsInPic];
      for (int k = 0; k < m_numCTUsInPic; k++)
      {
        m_alfCovarianceCcAlf[compIdx - 1][i][k].create(m_filterShapesCcAlf[compIdx - 1][i].numCoeff, 1);
      }
    }
  }
  m_trainingCovControl   = new uint8_t[m_numCTUsInPic];
  for ( int i = 0; i < MAX_NUM_CC_ALF_FILTERS; i++ )
  {
    m_trainingDistortion[i] = new uint64_t[m_numCTUsInPic];
  }
  m_filterControl         = new uint8_t[m_numCTUsInPic];
  m_bestFilterControl     = new uint8_t[m_numCTUsInPic];
  m_bestFilterCount = 0;
  uint32_t area           = (picWidth >> getComponentScaleX(COMPONENT_Cb, chromaFormatIdc))
                  * (picHeight >> getComponentScaleY(COMPONENT_Cb, chromaFormatIdc));
  m_bufOrigin             = ( Pel* ) xMalloc( Pel, area );
  m_buf                   = new PelBuf(m_bufOrigin, picWidth >> getComponentScaleX(COMPONENT_Cb, chromaFormatIdc),
                                       picWidth >> getComponentScaleX(COMPONENT_Cb, chromaFormatIdc),
                                       picHeight >> getComponentScaleY(COMPONENT_Cb, chromaFormatIdc));
  m_lumaSwingGreaterThanThresholdCount = new uint64_t[m_numCTUsInPic];
  m_chromaSampleCountNearMidPoint = new uint64_t[m_numCTUsInPic];
}

void EncAdaptiveLoopFilter::destroy()
{
  if (!m_created)
  {
    return;
  }
  for (const auto chType: { ChannelType::LUMA, ChannelType::CHROMA })
  {
    if (m_alfCovarianceFrame[chType])
    {
      int numClasses = chType == ChannelType::CHROMA ? 1 : MAX_NUM_ALF_CLASSES;
      for (int i = 0; i != m_filterShapes[chType].size(); i++)
      {
        for( int k = 0; k < numClasses; k++ )
        {
          m_alfCovarianceFrame[chType][i][k].destroy();
        }
        delete[] m_alfCovarianceFrame[chType][i];
        m_alfCovarianceFrame[chType][i] = nullptr;
      }
      delete[] m_alfCovarianceFrame[chType];
      m_alfCovarianceFrame[chType] = nullptr;
    }
  }

  for( int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++ )
  {
    if( m_alfCovariance[compIdx] )
    {
      ChannelType chType = toChannelType( ComponentID( compIdx ) );
      int numClasses = compIdx ? 1 : MAX_NUM_ALF_CLASSES;

      for (int i = 0; i != m_filterShapes[chType].size(); i++)
      {
        for( int j = 0; j < m_numCTUsInPic; j++ )
        {
          for( int k = 0; k < numClasses; k++ )
          {
            m_alfCovariance[compIdx][i][j][k].destroy();
          }
          delete[] m_alfCovariance[compIdx][i][j];
          m_alfCovariance[compIdx][i][j] = nullptr;

        }
        delete[] m_alfCovariance[compIdx][i];
        m_alfCovariance[compIdx][i] = nullptr;

      }
      delete[] m_alfCovariance[compIdx];
      m_alfCovariance[compIdx] = nullptr;
    }
  }

  for (int i = 0; i != m_filterShapes[ChannelType::LUMA].size(); i++)
  {
    for (int j = 0; j <= MAX_NUM_ALF_CLASSES + 1; j++)
    {
      m_alfCovarianceMerged[i][j].destroy();
    }
  }

  if( m_filterCoeffSet )
  {
    for( int i = 0; i < MAX_NUM_ALF_CLASSES; i++ )
    {
      delete[] m_filterCoeffSet[i];
      m_filterCoeffSet[i] = nullptr;
    }
    delete[] m_filterCoeffSet;
    m_filterCoeffSet = nullptr;
  }

  if( m_filterClippSet )
  {
    for( int i = 0; i < MAX_NUM_ALF_CLASSES; i++ )
    {
      delete[] m_filterClippSet[i];
      m_filterClippSet[i] = nullptr;
    }
    delete[] m_filterClippSet;
    m_filterClippSet = nullptr;
  }

  delete[] m_ctbDistortionFixedFilter;
  m_ctbDistortionFixedFilter = nullptr;
  for (int comp = 0; comp < MAX_NUM_COMPONENT; comp++)
  {
    delete[] m_ctbDistortionUnfilter[comp];
    m_ctbDistortionUnfilter[comp] = nullptr;
  }

  for (int compIdx = 1; compIdx < MAX_NUM_COMPONENT; compIdx++)
  {
    if (m_alfCovarianceFrameCcAlf[compIdx - 1])
    {
      for (int i = 0; i != m_filterShapesCcAlf[compIdx - 1].size(); i++)
      {
        m_alfCovarianceFrameCcAlf[compIdx - 1][i].destroy();
      }
      delete[] m_alfCovarianceFrameCcAlf[compIdx - 1];
      m_alfCovarianceFrameCcAlf[compIdx - 1] = nullptr;
    }

    if (m_alfCovarianceCcAlf[compIdx - 1])
    {
      for (int i = 0; i != m_filterShapesCcAlf[compIdx - 1].size(); i++)
      {
        for (int k = 0; k < m_numCTUsInPic; k++)
        {
          m_alfCovarianceCcAlf[compIdx - 1][i][k].destroy();
        }
        delete[] m_alfCovarianceCcAlf[compIdx - 1][i];
      }
      delete[] m_alfCovarianceCcAlf[compIdx - 1];
      m_alfCovarianceCcAlf[compIdx - 1] = nullptr;
    }
  }

  if (m_trainingCovControl)
  {
    delete[] m_trainingCovControl;
    m_trainingCovControl = nullptr;
  }

  for ( int i = 0; i < MAX_NUM_CC_ALF_FILTERS; i++ )
  {
    if (m_trainingDistortion[i])
    {
      delete[] m_trainingDistortion[i];
      m_trainingDistortion[i] = nullptr;
    }
  }

  if (m_filterControl)
  {
    delete[] m_filterControl;
    m_filterControl = nullptr;
  }

  if (m_bestFilterControl)
  {
    delete[] m_bestFilterControl;
    m_bestFilterControl = nullptr;
  }

  if (m_bufOrigin)
  {
    xFree(m_bufOrigin);
    m_bufOrigin = nullptr;
  }

  if (m_buf)
  {
    delete m_buf;
    m_buf = nullptr;
  }

  if (m_lumaSwingGreaterThanThresholdCount)
  {
    delete[] m_lumaSwingGreaterThanThresholdCount;
    m_lumaSwingGreaterThanThresholdCount = nullptr;
  }
  if (m_chromaSampleCountNearMidPoint)
  {
    delete[] m_chromaSampleCountNearMidPoint;
    m_chromaSampleCountNearMidPoint = nullptr;
  }

  AdaptiveLoopFilter::destroy();
}

void EncAdaptiveLoopFilter::initCABACEstimator(CABACEncoder *cabacEncoder, CtxPool *ctxPool, Slice *pcSlice,
                                               ParameterSetMap<APS> *apsMap)
{
  m_apsMap = apsMap;
  m_CABACEstimator = cabacEncoder->getCABACEstimator( pcSlice->getSPS() );
  m_ctxPool        = ctxPool;
  m_CABACEstimator->initCtxModels( *pcSlice );
  m_CABACEstimator->resetBits();
}

void EncAdaptiveLoopFilter::xSetupCcAlfAPS( CodingStructure &cs )
{
  if (m_ccAlfFilterParam.ccAlfFilterEnabled[COMPONENT_Cb - 1])
  {
    int  ccAlfCbApsId = cs.slice->getCcAlfCbApsId();
    APS *aps          = m_apsMap->getPS(cs.slice->getCcAlfCbApsId());
    if (aps == nullptr)
    {
      aps = m_apsMap->allocatePS(ccAlfCbApsId);
      aps->setTemporalId(cs.slice->getTLayer());
    }
    aps->getCcAlfAPSParam().ccAlfFilterEnabled[COMPONENT_Cb - 1] = 1;
    aps->getCcAlfAPSParam().ccAlfFilterCount[COMPONENT_Cb - 1] = m_ccAlfFilterParam.ccAlfFilterCount[COMPONENT_Cb - 1];
    for ( int filterIdx = 0; filterIdx < MAX_NUM_CC_ALF_FILTERS; filterIdx++ )
    {
      aps->getCcAlfAPSParam().ccAlfFilterIdxEnabled[COMPONENT_Cb - 1][filterIdx] =
        m_ccAlfFilterParam.ccAlfFilterIdxEnabled[COMPONENT_Cb - 1][filterIdx];
      std::copy_n(m_ccAlfFilterParam.ccAlfCoeff[COMPONENT_Cb - 1][filterIdx], MAX_NUM_CC_ALF_CHROMA_COEFF,
                  aps->getCcAlfAPSParam().ccAlfCoeff[COMPONENT_Cb - 1][filterIdx]);
    }
    aps->setAPSId(ccAlfCbApsId);
    aps->setAPSType(ApsType::ALF);
    if (m_reuseApsId[COMPONENT_Cb - 1] < 0)
    {
      aps->getCcAlfAPSParam().newCcAlfFilter[COMPONENT_Cb - 1] = 1;
      m_apsMap->setChangedFlag(ccAlfCbApsId, true);
      aps->setTemporalId(cs.slice->getTLayer());
    }
    cs.slice->setCcAlfCbEnabledFlag(true);
  }
  else
  {
    cs.slice->setCcAlfCbEnabledFlag(false);
  }
  if (m_ccAlfFilterParam.ccAlfFilterEnabled[COMPONENT_Cr - 1])
  {
    int  ccAlfCrApsId = cs.slice->getCcAlfCrApsId();
    APS *aps          = m_apsMap->getPS(cs.slice->getCcAlfCrApsId());
    if (aps == nullptr)
    {
      aps = m_apsMap->allocatePS(ccAlfCrApsId);
      aps->setTemporalId(cs.slice->getTLayer());
    }
    aps->getCcAlfAPSParam().ccAlfFilterEnabled[COMPONENT_Cr - 1] = 1;
    aps->getCcAlfAPSParam().ccAlfFilterCount[COMPONENT_Cr - 1] = m_ccAlfFilterParam.ccAlfFilterCount[COMPONENT_Cr - 1];
    for ( int filterIdx = 0; filterIdx < MAX_NUM_CC_ALF_FILTERS; filterIdx++ )
    {
      aps->getCcAlfAPSParam().ccAlfFilterIdxEnabled[COMPONENT_Cr - 1][filterIdx] =
        m_ccAlfFilterParam.ccAlfFilterIdxEnabled[COMPONENT_Cr - 1][filterIdx];
      std::copy_n(m_ccAlfFilterParam.ccAlfCoeff[COMPONENT_Cr - 1][filterIdx], MAX_NUM_CC_ALF_CHROMA_COEFF,
                  aps->getCcAlfAPSParam().ccAlfCoeff[COMPONENT_Cr - 1][filterIdx]);
    }
    aps->setAPSId(ccAlfCrApsId);
    if (m_reuseApsId[COMPONENT_Cr - 1] < 0)
    {
      aps->getCcAlfAPSParam().newCcAlfFilter[COMPONENT_Cr - 1] = 1;
      m_apsMap->setChangedFlag(ccAlfCrApsId, true);
      aps->setTemporalId(cs.slice->getTLayer());
    }
    aps->setAPSType(ApsType::ALF);
    cs.slice->setCcAlfCrEnabledFlag(true);
  }
  else
  {
    cs.slice->setCcAlfCrEnabledFlag(false);
  }
}

void EncAdaptiveLoopFilter::ALFProcess(CodingStructure& cs, const double *lambdas
#if ENABLE_QPA
                                       , const double lambdaChromaWeight
#endif
                                       , Picture* pcPic, uint32_t numSliceSegments
                                      )
{
  // IRAP AU is assumed
  if( ( cs.slice->getPendingRasInit() || cs.slice->isIDRorBLA() || ( cs.slice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA && m_encCfg->getCraAPSreset() ) ) )
  {
    memset(cs.slice->getAlfAPSs(), 0, sizeof(*cs.slice->getAlfAPSs())*ALF_CTB_MAX_NUM_APS);
    m_apsIdStart = m_encCfg->getALFAPSIDShift() + m_encCfg->getMaxNumALFAPS();

    m_apsMap->clearActive();
    for (int i = m_encCfg->getALFAPSIDShift(); i < m_encCfg->getALFAPSIDShift() + m_encCfg->getMaxNumALFAPS(); i++)
    {
      APS *alfAPS = m_apsMap->getPS(i);
      m_apsMap->clearChangedFlag(i);
      if (alfAPS)
      {
        alfAPS->getAlfAPSParam().reset();
        alfAPS->getCcAlfAPSParam().reset();
        alfAPS = nullptr;
      }
    }
  }
  AlfParam alfParam;
  alfParam.reset();
  const TempCtx ctxStart(m_ctxPool, AlfCtx(m_CABACEstimator->getCtx()));

  const TempCtx ctxStartCcAlf(m_ctxPool, SubCtx(Ctx::CcAlfFilterControlFlag, m_CABACEstimator->getCtx()));

  // set available filter shapes
  alfParam.filterShapes = &m_filterShapes;

  // set clipping range
  m_clpRngs = cs.slice->getClpRngs();

  // set CTU ALF enable flags, it was already reset before ALF process
  for( int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++ )
  {
    m_modes[compIdx] = cs.picture->getAlfModes(compIdx);
  }

  // reset ALF parameters
  alfParam.reset();
  int shiftLuma          = 2 * DISTORTION_PRECISION_ADJUSTMENT(m_inputBitDepth[ChannelType::LUMA]);
  int shiftChroma        = 2 * DISTORTION_PRECISION_ADJUSTMENT(m_inputBitDepth[ChannelType::CHROMA]);
  m_lambda[COMPONENT_Y] = lambdas[COMPONENT_Y] * double(1 << shiftLuma);
  m_lambda[COMPONENT_Cb] = lambdas[COMPONENT_Cb] * double(1 << shiftChroma);
  m_lambda[COMPONENT_Cr] = lambdas[COMPONENT_Cr] * double(1 << shiftChroma);
  PelUnitBuf orgYuv = m_encCfg->getAlfTrueOrg() ? cs.getTrueOrgBuf() : cs.getOrgBuf();

  m_tempBuf.copyFrom( cs.getRecoBuf() );
  PelUnitBuf recYuv = m_tempBuf.getBuf( cs.area );
  recYuv.extendBorderPel( MAX_ALF_FILTER_LENGTH >> 1 );

  // derive classification
  const CPelBuf& recLuma = recYuv.get( COMPONENT_Y );
  const PreCalcValues& pcv = *cs.pcv;
  bool clipTop = false, clipBottom = false, clipLeft = false, clipRight = false;
  int numHorVirBndry = 0, numVerVirBndry = 0;
  int horVirBndryPos[] = { 0, 0, 0 };
  int verVirBndryPos[] = { 0, 0, 0 };

  for( int yPos = 0; yPos < pcv.lumaHeight; yPos += pcv.maxCUHeight )
  {
    for( int xPos = 0; xPos < pcv.lumaWidth; xPos += pcv.maxCUWidth )
    {
      const int width = ( xPos + pcv.maxCUWidth > pcv.lumaWidth ) ? ( pcv.lumaWidth - xPos ) : pcv.maxCUWidth;
      const int height = ( yPos + pcv.maxCUHeight > pcv.lumaHeight ) ? ( pcv.lumaHeight - yPos ) : pcv.maxCUHeight;
      int rasterSliceAlfPad = 0;
      if (isCrossedByVirtualBoundaries( cs, xPos, yPos, width, height, clipTop, clipBottom, clipLeft, clipRight, numHorVirBndry, numVerVirBndry, horVirBndryPos, verVirBndryPos, rasterSliceAlfPad ) )
      {
        int yStart = yPos;
        for( int i = 0; i <= numHorVirBndry; i++ )
        {
          const int yEnd = i == numHorVirBndry ? yPos + height : horVirBndryPos[i];
          const int h = yEnd - yStart;
          const bool clipT = ( i == 0 && clipTop ) || ( i > 0 ) || ( yStart == 0 );
          const bool clipB = ( i == numHorVirBndry && clipBottom ) || ( i < numHorVirBndry ) || ( yEnd == pcv.lumaHeight );
          int xStart = xPos;
          for( int j = 0; j <= numVerVirBndry; j++ )
          {
            const int xEnd = j == numVerVirBndry ? xPos + width : verVirBndryPos[j];
            const int w = xEnd - xStart;
            const bool clipL = ( j == 0 && clipLeft ) || ( j > 0 ) || ( xStart == 0 );
            const bool clipR = ( j == numVerVirBndry && clipRight ) || ( j < numVerVirBndry ) || ( xEnd == pcv.lumaWidth );
            const int wBuf = w + (clipL ? 0 : MAX_ALF_PADDING_SIZE) + (clipR ? 0 : MAX_ALF_PADDING_SIZE);
            const int hBuf = h + (clipT ? 0 : MAX_ALF_PADDING_SIZE) + (clipB ? 0 : MAX_ALF_PADDING_SIZE);
            PelUnitBuf buf = m_tempBuf2.subBuf( UnitArea( cs.area.chromaFormat, Area( 0, 0, wBuf, hBuf ) ) );
            buf.copyFrom( recYuv.subBuf( UnitArea( cs.area.chromaFormat, Area( xStart - (clipL ? 0 : MAX_ALF_PADDING_SIZE), yStart - (clipT ? 0 : MAX_ALF_PADDING_SIZE), wBuf, hBuf ) ) ) );
            // pad top-left unavailable samples for raster slice
            if ( xStart == xPos && yStart == yPos && ( rasterSliceAlfPad & 1 ) )
            {
              buf.padBorderPel( MAX_ALF_PADDING_SIZE, 1 );
            }

            // pad bottom-right unavailable samples for raster slice
            if ( xEnd == xPos + width && yEnd == yPos + height && ( rasterSliceAlfPad & 2 ) )
            {
              buf.padBorderPel( MAX_ALF_PADDING_SIZE, 2 );
            }
            buf.extendBorderPel( MAX_ALF_PADDING_SIZE );
            buf = buf.subBuf( UnitArea ( cs.area.chromaFormat, Area( clipL ? 0 : MAX_ALF_PADDING_SIZE, clipT ? 0 : MAX_ALF_PADDING_SIZE, w, h ) ) );

            const Area blkSrc( 0, 0, w, h );
            const Area blkDst( xStart, yStart, w, h );
            deriveClassification( m_classifier, buf.get(COMPONENT_Y), blkDst, blkSrc );

            xStart = xEnd;
          }

          yStart = yEnd;
        }
      }
      else
      {
        Area blk( xPos, yPos, width, height );
        deriveClassification( m_classifier, recLuma, blk, blk );
      }
    }
  }

  // get CTB stats for filtering
  deriveStatsForFiltering( orgYuv, recYuv, cs );

  AlfMode *indexPtr = cs.slice->getPic()->getAlfModes(COMPONENT_Y);
  for (int ctbIdx = 0; ctbIdx < m_numCTUsInPic; ctbIdx++)
  {
    indexPtr[ctbIdx] = AlfMode::LUMA0;
  }
  // consider using new filter (only)
  alfParam.newFilterFlag[ChannelType::LUMA]   = true;
  alfParam.newFilterFlag[ChannelType::CHROMA] = true;
  cs.slice->setNumAlfApsIdsLuma(1); // Only new filter for RD cost optimization
  // derive filter (luma)
  firstPass(cs, alfParam, orgYuv, recYuv, cs.getRecoBuf(), ChannelType::LUMA
#if ENABLE_QPA
            ,
            lambdaChromaWeight
#endif
  );

  // derive filter (chroma)
  if (!(m_encCfg->getMaxNumALFAPS() == 0) && isChromaEnabled(cs.pcv->chrFormat)) // Find ALF parameters for chroma if ALF APS is enabled
  {
    firstPass(cs, alfParam, orgYuv, recYuv, cs.getRecoBuf(), ChannelType::CHROMA
#if ENABLE_QPA
              ,
              lambdaChromaWeight
#endif
    );
  }

  // let alfEncoderCtb decide now
  alfParam.newFilterFlag.fill(false);
  cs.slice->setNumAlfApsIdsLuma(0);
  m_CABACEstimator->getCtx() = AlfCtx(ctxStart);
  alfEncoderCtb(cs, alfParam
#if ENABLE_QPA
    , lambdaChromaWeight
#endif
  );

  for (int s = 0; s < numSliceSegments; s++)
  {
    if (pcPic->slices[s]->isLossless())
    {
      for (uint32_t ctuIdx = 0; ctuIdx < pcPic->slices[s]->getNumCtuInSlice(); ctuIdx++)
      {
        uint32_t ctuRsAddr = pcPic->slices[s]->getCtuAddrInSlice(ctuIdx);
        m_modes[COMPONENT_Y][ctuRsAddr]  = AlfMode::OFF;
        m_modes[COMPONENT_Cb][ctuRsAddr] = AlfMode::OFF;
        m_modes[COMPONENT_Cr][ctuRsAddr] = AlfMode::OFF;
      }
    }
  }

  alfReconstructor(cs, recYuv);

  // Do not transmit CC ALF if it is unchanged
  if (cs.slice->getAlfEnabledFlag(COMPONENT_Y))
  {
    for (int32_t lumaAlfApsId : cs.slice->getAlfApsIdsLuma())
    {
      APS *aps = (lumaAlfApsId >= 0) ? m_apsMap->getPS(lumaAlfApsId) : nullptr;
      if (aps && m_apsMap->getChangedFlag(lumaAlfApsId))
      {
        aps->getCcAlfAPSParam().newCcAlfFilter[0] = false;
          aps->getCcAlfAPSParam().newCcAlfFilter[1] = false;
      }
    }
  }
  int chromaAlfApsId = ( cs.slice->getAlfEnabledFlag(COMPONENT_Cb) || cs.slice->getAlfEnabledFlag(COMPONENT_Cr) ) ? cs.slice->getAlfApsIdChroma() : -1;
  APS *aps            = (chromaAlfApsId >= 0) ? m_apsMap->getPS(chromaAlfApsId) : nullptr;
  if (aps && m_apsMap->getChangedFlag(chromaAlfApsId))
  {
    aps->getCcAlfAPSParam().newCcAlfFilter[0] = false;
    aps->getCcAlfAPSParam().newCcAlfFilter[1] = false;
  }

  if (!cs.slice->getSPS()->getCCALFEnabledFlag())
  {
    return;
  }

  m_tempBuf.get(COMPONENT_Cb).copyFrom(cs.getRecoBuf().get(COMPONENT_Cb));
  m_tempBuf.get(COMPONENT_Cr).copyFrom(cs.getRecoBuf().get(COMPONENT_Cr));
  recYuv = m_tempBuf.getBuf(cs.area);
  recYuv.extendBorderPel(MAX_ALF_FILTER_LENGTH >> 1);

  deriveStatsForCcAlfFiltering(orgYuv, recYuv, COMPONENT_Cb, m_numCTUsInWidth, cs);
  deriveStatsForCcAlfFiltering(orgYuv, recYuv, COMPONENT_Cr, m_numCTUsInWidth, cs);
  initDistortionCcalf();

  m_CABACEstimator->getCtx() = SubCtx(Ctx::CcAlfFilterControlFlag, ctxStartCcAlf);
  deriveCcAlfFilter(cs, COMPONENT_Cb, orgYuv, recYuv, cs.getRecoBuf());
  m_CABACEstimator->getCtx() = SubCtx(Ctx::CcAlfFilterControlFlag, ctxStartCcAlf);
  deriveCcAlfFilter(cs, COMPONENT_Cr, orgYuv, recYuv, cs.getRecoBuf());

  xSetupCcAlfAPS(cs);

  for (int compIdx = 1; compIdx < getNumberValidComponents(cs.pcv->chrFormat); compIdx++)
  {
    ComponentID compID     = ComponentID(compIdx);
    if (m_ccAlfFilterParam.ccAlfFilterEnabled[compIdx - 1])
    {
      applyCcAlfFilter(cs, compID, cs.getRecoBuf().get(compID), recYuv, m_ccAlfFilterControl[compIdx - 1],
                       m_ccAlfFilterParam.ccAlfCoeff[compIdx - 1], -1);
    }
  }
}

double EncAdaptiveLoopFilter::deriveCtbAlfEnableFlags(CodingStructure &cs, const int shapeIdx, ChannelType channel,
#if ENABLE_QPA
                                                      const double chromaWeight,
#endif
                                                      const int numClasses, const int numCoeff, double &distUnfilter)
{
  TempCtx           ctxTempStart(m_ctxPool);
  TempCtx           ctxTempBest(m_ctxPool);
  TempCtx           ctxTempAltStart(m_ctxPool);
  TempCtx           ctxTempAltBest(m_ctxPool);
  const ComponentID compIDFirst = isLuma( channel ) ? COMPONENT_Y : COMPONENT_Cb;
  const ComponentID compIDLast = isLuma( channel ) ? COMPONENT_Y : COMPONENT_Cr;

  const int numAlts = isLuma( channel ) ? 1 : m_alfParamTemp.numAlternativesChroma;

  double cost = 0;
  distUnfilter = 0;

  setSliceEnabledFlag(m_alfParamTemp, channel, true);
#if ENABLE_QPA
  CHECK ((chromaWeight > 0.0) && (cs.slice->getFirstCtuRsAddrInSlice() != 0), "incompatible start CTU address, must be 0");
#endif

  reconstructCoeff(m_alfParamTemp, channel, true, isLuma(channel));
  for (int altIdx = 0; altIdx < numAlts; altIdx++)
  {
    for (int classIdx = 0; classIdx < (isLuma(channel) ? MAX_NUM_ALF_CLASSES : 1); classIdx++)
    {
      for (int i = 0; i < (isLuma(channel) ? MAX_NUM_ALF_LUMA_COEFF : MAX_NUM_ALF_CHROMA_COEFF); i++)
      {
        m_filterCoeffSet[isLuma(channel) ? classIdx : altIdx][i] = isLuma(channel) ? m_coeffFinal[classIdx * MAX_NUM_ALF_LUMA_COEFF + i] : m_chromaCoeffFinal[altIdx][i];
        m_filterClippSet[isLuma(channel) ? classIdx : altIdx][i] = isLuma(channel) ? m_clippFinal[classIdx * MAX_NUM_ALF_LUMA_COEFF + i] : m_chromaClippFinal[altIdx][i];
      }
    }
  }

  for( int ctuIdx = 0; ctuIdx < m_numCTUsInPic; ctuIdx++ )
  {
    for( int compID = compIDFirst; compID <= compIDLast; compID++ )
    {
#if ENABLE_QPA
      const double ctuLambda = chromaWeight > 0.0 ? (isLuma (channel) ? cs.picture->m_uEnerHpCtu[ctuIdx] : cs.picture->m_uEnerHpCtu[ctuIdx] / chromaWeight) : m_lambda[compID];
#else
      const double ctuLambda = m_lambda[compID];
#endif

      double distUnfilterCtu = getUnfilteredDistortion(m_alfCovariance[compID][shapeIdx][ctuIdx], numClasses);

      ctxTempStart = AlfCtx( m_CABACEstimator->getCtx() );
      m_CABACEstimator->resetBits();
      m_modes[compID][ctuIdx] = isLuma(channel) ? AlfMode::LUMA0 : AlfMode::CHROMA0;
      m_CABACEstimator->codeAlfCtuEnableFlag( cs, ctuIdx, compID, &m_alfParamTemp );
      if( isLuma( channel ) )
      {
        // Evaluate cost of signaling filter set index for convergence of filters enabled flag / filter derivation
        assert( cs.slice->getNumAlfApsIdsLuma() == 1 );
        m_CABACEstimator->codeAlfCtuFilterIndex(cs, ctuIdx, m_alfParamTemp.enabledFlag[COMPONENT_Y]);
      }
      double costOn = distUnfilterCtu + ctuLambda * FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();

      ctxTempBest = AlfCtx( m_CABACEstimator->getCtx() );
      if( isLuma( channel ) )
      {
        costOn += getFilteredDistortion(m_alfCovariance[compID][shapeIdx][ctuIdx], numClasses,
                                        m_alfParamTemp.numLumaFilters - 1, numCoeff);
      }
      else
      {
        double bestAltCost = MAX_DOUBLE;
        int bestAltIdx = -1;
        ctxTempAltStart = AlfCtx( ctxTempBest );
        for( int altIdx = 0; altIdx < numAlts; ++altIdx )
        {
          if( altIdx )
          {
            m_CABACEstimator->getCtx() = AlfCtx( ctxTempAltStart );
          }
          m_CABACEstimator->resetBits();
          m_modes[compID][ctuIdx] = AlfMode::CHROMA0 + altIdx;
          m_CABACEstimator->codeAlfCtuAlternative( cs, ctuIdx, compID, &m_alfParamTemp );
          double r_altCost = ctuLambda * FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();

          double altDist = 0.;
          altDist += m_alfCovariance[compID][shapeIdx][ctuIdx][0].calcErrorForCoeffs(
            m_filterClippSet[altIdx], m_filterCoeffSet[altIdx], numCoeff, COEFF_SCALE_BITS);

          double altCost = altDist + r_altCost;
          if( altCost < bestAltCost )
          {
            bestAltCost = altCost;
            bestAltIdx = altIdx;
            ctxTempBest = AlfCtx( m_CABACEstimator->getCtx() );
          }
        }
        m_modes[compID][ctuIdx] = AlfMode::CHROMA0 + bestAltIdx;
        costOn += bestAltCost;
      }

      AlfMode bestIdxOn          = m_modes[compID][ctuIdx];
      m_CABACEstimator->getCtx() = AlfCtx( ctxTempStart );
      m_CABACEstimator->resetBits();
      m_modes[compID][ctuIdx] = AlfMode::OFF;
      m_CABACEstimator->codeAlfCtuEnableFlag( cs, ctuIdx, compID, &m_alfParamTemp);
      double costOff = distUnfilterCtu + ctuLambda * FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();

      if( costOn < costOff )
      {
        cost += costOn;
        m_CABACEstimator->getCtx() = AlfCtx( ctxTempBest );
        m_modes[compID][ctuIdx]    = bestIdxOn;
      }
      else
      {
        cost += costOff;
        distUnfilter += distUnfilterCtu;
      }
    }
  }

  if( isChroma( channel ) )
  {
    setSliceEnabledFlag(m_alfParamTemp, channel, m_modes);
  }

  return cost;
}

void EncAdaptiveLoopFilter::firstPass(CodingStructure &cs, AlfParam &alfParam, const PelUnitBuf &orgUnitBuf,
                                      const PelUnitBuf &recExtBuf, const PelUnitBuf &recBuf, const ChannelType channel
#if ENABLE_QPA
                                      ,
                                      const double lambdaChromaWeight   // = 0.0
#endif
)
{
  const TempCtx ctxStart(m_ctxPool, AlfCtx(m_CABACEstimator->getCtx()));
  TempCtx       ctxBest(m_ctxPool);

  double costMin = MAX_DOUBLE;

  std::vector<AlfFilterShape> &alfFilterShape = (*alfParam.filterShapes)[channel];
  m_bitsNewFilter[channel]                    = 0;
  const int numClasses = isLuma( channel ) ? MAX_NUM_ALF_CLASSES : 1;
  int coeffBits = 0;

  for (int shapeIdx = 0; shapeIdx < alfFilterShape.size(); shapeIdx++)
  {
    m_alfParamTemp = alfParam;
    //1. get unfiltered distortion
    if( isChroma(channel) )
    {
      m_alfParamTemp.numAlternativesChroma = 1;
    }
    double cost = getUnfilteredDistortion(m_alfCovarianceFrame[channel][shapeIdx], channel);
    cost /= 1.001; // slight preference for unfiltered choice

    if( cost < costMin )
    {
      costMin = cost;
      setSliceEnabledFlag(alfParam, channel, false);
      // no CABAC signalling
      ctxBest = AlfCtx( ctxStart );
      setCtuEnableFlag(m_indexTmp, channel, AlfMode::OFF);
    }

    // For chroma, nonlinear flag is checked for each alternative filter
    const bool useNonlinearAlf =
      isLuma(channel) ? m_encCfg->getUseNonLinearAlfLuma() : m_encCfg->getUseNonLinearAlfChroma();

    for (bool nonLinearFlag: { false, true })
    {
      if (nonLinearFlag && !useNonlinearAlf)
      {
        continue;
      }

      for (int numAlternatives = isLuma(channel) ? 1 : getMaxNumAlternativesChroma(); numAlternatives > 0;
           numAlternatives--)
      {
        if (isChroma(channel))
        {
          m_alfParamTemp.numAlternativesChroma = numAlternatives;
        }
        // 2. all CTUs are on
        setSliceEnabledFlag(m_alfParamTemp, channel, true);
        m_alfParamTemp.nonLinearFlag[channel] = nonLinearFlag;
        m_CABACEstimator->getCtx()            = AlfCtx(ctxStart);

        // all alternatives are on
        if (isChroma(channel))
        {
          initCtuAlternativeChroma(m_modes);
        }
        else
        {
          setCtuEnableFlag(m_modes, channel, AlfMode::LUMA0);
        }

        cost = getFilterCoeffAndCost(cs, 0, channel, true, shapeIdx, coeffBits);

        if (cost < costMin)
        {
          m_bitsNewFilter[channel] = coeffBits;
          costMin                  = cost;
          copyAlfParam(alfParam, m_alfParamTemp, channel);
          ctxBest = AlfCtx(m_CABACEstimator->getCtx());
          copyIndices(m_indexTmp, m_modes, channel);
        }

        // 3. CTU decision
        double    distUnfilter = 0;
        double    prevItCost   = MAX_DOUBLE;
        const int iterNum = isLuma(channel) ? (2 * 4 + 1) : (2 * (2 + m_alfParamTemp.numAlternativesChroma - 1) + 1);

        for (int iter = 0; iter < iterNum; iter++)
        {
          if (iter % 2 == 0)
          {
            m_CABACEstimator->getCtx() = AlfCtx(ctxStart);

            cost                       = m_lambda[isLuma(channel) ? COMPONENT_Y : COMPONENT_Cb] * coeffBits;

            cost += deriveCtbAlfEnableFlags(cs, shapeIdx, channel,
#if ENABLE_QPA
                                            lambdaChromaWeight,
#endif
                                            numClasses, alfFilterShape[shapeIdx].numCoeff, distUnfilter);

            if (cost < costMin)
            {
              m_bitsNewFilter[channel] = coeffBits;
              costMin                  = cost;
              ctxBest                  = AlfCtx(m_CABACEstimator->getCtx());
              copyIndices(m_indexTmp, m_modes, channel);
              copyAlfParam(alfParam, m_alfParamTemp, channel);
            }
            else if (cost >= prevItCost)
            {
              // High probability that we have converged or we are diverging
              break;
            }
            prevItCost = cost;
          }
          else
          {
            // unfiltered distortion is added due to some CTBs may not use filter
            // no need to reset CABAC here, since coeffBits is not affected
            /*cost = */ getFilterCoeffAndCost(cs, distUnfilter, channel, true, shapeIdx, coeffBits);
          }
        }   // for iter
        // Decrease number of alternatives and reset ctu params and filters
      }
    }// for nonLineaFlag
  }//for shapeIdx
  m_CABACEstimator->getCtx() = AlfCtx( ctxBest );

  copyIndices(m_modes, m_indexTmp, channel);
}

void EncAdaptiveLoopFilter::copyAlfParam( AlfParam& alfParamDst, AlfParam& alfParamSrc, ChannelType channel )
{
  if( isLuma( channel ) )
  {
    alfParamDst = alfParamSrc;
  }
  else
  {
    alfParamDst.enabledFlag[COMPONENT_Cb] = alfParamSrc.enabledFlag[COMPONENT_Cb];
    alfParamDst.enabledFlag[COMPONENT_Cr] = alfParamSrc.enabledFlag[COMPONENT_Cr];
    alfParamDst.numAlternativesChroma = alfParamSrc.numAlternativesChroma;
    alfParamDst.nonLinearFlag[ChannelType::CHROMA] = alfParamSrc.nonLinearFlag[ChannelType::CHROMA];
    memcpy( alfParamDst.chromaCoeff, alfParamSrc.chromaCoeff, sizeof( alfParamDst.chromaCoeff ) );
    memcpy( alfParamDst.chromaClipp, alfParamSrc.chromaClipp, sizeof( alfParamDst.chromaClipp ) );
  }
}

double EncAdaptiveLoopFilter::getFilterCoeffAndCost(CodingStructure &cs, double distUnfilter, ChannelType channel,
                                                    bool bReCollectStat, int shapeIdx, int &coeffBits,
                                                    bool onlyFilterCost)
{
  //collect stat based on CTU decision
  if( bReCollectStat )
  {
    getFrameStats(channel, shapeIdx);
  }

  double dist = distUnfilter;
  coeffBits = 0;
  AlfFilterShape &alfFilterShape = (*m_alfParamTemp.filterShapes)[channel][shapeIdx];
  //get filter coeff
  if( isLuma( channel ) )
  {
    std::fill_n(m_alfClipMerged[shapeIdx][0][0], MAX_NUM_ALF_LUMA_COEFF * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_CLASSES,
                m_alfParamTemp.nonLinearFlag[channel] ? ALF_NUM_CLIP_VALS[ChannelType::LUMA] / 2 : 0);
    // Reset Merge Tmp Cov
    m_alfCovarianceMerged[shapeIdx][MAX_NUM_ALF_CLASSES].reset(ALF_NUM_CLIP_VALS[channel]);
    m_alfCovarianceMerged[shapeIdx][MAX_NUM_ALF_CLASSES + 1].reset(ALF_NUM_CLIP_VALS[channel]);
    //distortion
    dist += mergeFiltersAndCost(m_alfParamTemp, alfFilterShape, m_alfCovarianceFrame[channel][shapeIdx],
                                m_alfCovarianceMerged[shapeIdx], m_alfClipMerged[shapeIdx], coeffBits);
  }
  else
  {
    //distortion
    for( int altIdx = 0; altIdx < m_alfParamTemp.numAlternativesChroma; ++altIdx )
    {
      assert(alfFilterShape.numCoeff == m_alfCovarianceFrame[channel][shapeIdx][altIdx].numCoeff);
      AlfParam bestSliceParam;
      double bestCost = MAX_DOUBLE;
      double bestDist = MAX_DOUBLE;
      int bestCoeffBits = 0;
      const int nonLinearFlagMax = m_encCfg->getUseNonLinearAlfChroma() ? 1 : 0;

      for (int nonLinearFlag = 0; nonLinearFlag <= nonLinearFlagMax; nonLinearFlag++)
      {
        const int currentNonLinearFlag = m_alfParamTemp.nonLinearFlag[channel] ? 1 : 0;
        if (nonLinearFlag != currentNonLinearFlag)
        {
          continue;
        }

        std::fill_n(m_filterClippSet[altIdx], MAX_NUM_ALF_CHROMA_COEFF,
                    nonLinearFlag ? ALF_NUM_CLIP_VALS[ChannelType::CHROMA] / 2 : 0);
        double dist = m_alfCovarianceFrame[channel][shapeIdx][altIdx].pixAcc
                      + deriveCoeffQuant(m_filterClippSet[altIdx], m_filterCoeffSet[altIdx],
                                         m_alfCovarianceFrame[channel][shapeIdx][altIdx], alfFilterShape,
                                         COEFF_SCALE_BITS, nonLinearFlag);
        for( int i = 0; i < MAX_NUM_ALF_CHROMA_COEFF; i++ )
        {
          m_alfParamTemp.chromaCoeff[altIdx][i] = m_filterCoeffSet[altIdx][i];
          m_alfParamTemp.chromaClipp[altIdx][i] = m_filterClippSet[altIdx][i];
        }
        int coeffBits = getChromaCoeffRate( m_alfParamTemp, altIdx );
        double cost      = dist + m_lambda[isLuma(channel) ? COMPONENT_Y : COMPONENT_Cb] * coeffBits;
        if( cost < bestCost )
        {
          bestCost = cost;
          bestDist = dist;
          bestCoeffBits = coeffBits;
          bestSliceParam = m_alfParamTemp;
        }
      }
      coeffBits += bestCoeffBits;
      dist += bestDist;
      m_alfParamTemp = bestSliceParam;
    }
    coeffBits += lengthUvlc( m_alfParamTemp.numAlternativesChroma-1 );
    coeffBits++;
  }
  if (onlyFilterCost)
  {
    return dist + m_lambda[isLuma(channel) ? COMPONENT_Y : COMPONENT_Cb] * coeffBits;
  }
  double rate = coeffBits;
  m_CABACEstimator->resetBits();
  m_CABACEstimator->codeAlfCtuEnableFlags( cs, channel, &m_alfParamTemp);
  for( int ctuIdx = 0; ctuIdx < m_numCTUsInPic; ctuIdx++ )
  {
    if( isLuma( channel ) )
    {
      // Evaluate cost of signaling filter set index for convergence of filters enabled flag / filter derivation
      assert(cs.slice->getPic()->getAlfModes(COMPONENT_Y)[ctuIdx] == AlfMode::LUMA0
             || cs.slice->getPic()->getAlfModes(COMPONENT_Y)[ctuIdx] == AlfMode::OFF);
      assert( cs.slice->getNumAlfApsIdsLuma() == 1 );
      m_CABACEstimator->codeAlfCtuFilterIndex(cs, ctuIdx, m_alfParamTemp.enabledFlag[COMPONENT_Y]);
    }
  }
  m_CABACEstimator->codeAlfCtuAlternatives( cs, channel, &m_alfParamTemp );
  rate += FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();
  return dist + m_lambda[isLuma(channel) ? COMPONENT_Y : COMPONENT_Cb] * rate;
}

int EncAdaptiveLoopFilter::getChromaCoeffRate( AlfParam& alfParam, int altIdx )
{
  int iBits = 0;

  AlfFilterShape alfShape(5);
  // Filter coefficients
  for( int i = 0; i < alfShape.numCoeff - 1; i++ )
  {
    iBits += lengthUvlc( abs( alfParam.chromaCoeff[ altIdx ][ i ] ) );  // alf_coeff_chroma[altIdx][i]
    if( ( alfParam.chromaCoeff[ altIdx ][ i ] ) != 0 )
    {
      iBits += 1;
    }
  }
  if (m_alfParamTemp.nonLinearFlag[ChannelType::CHROMA])
  {
    for (int i = 0; i < alfShape.numCoeff - 1; i++)
    {
      if( !abs( alfParam.chromaCoeff[altIdx][i] ) )
      {
        alfParam.chromaClipp[altIdx][i] = 0;
      }
    }
    iBits += ((alfShape.numCoeff - 1) << 1);
  }
  return iBits;
}

double EncAdaptiveLoopFilter::getUnfilteredDistortion( AlfCovariance* cov, ChannelType channel )
{
  double dist = 0;
  if( isLuma( channel ) )
  {
    dist = getUnfilteredDistortion( cov, MAX_NUM_ALF_CLASSES );
  }
  else
  {
    dist = getUnfilteredDistortion( cov, 1 );
  }
  return dist;
}

double EncAdaptiveLoopFilter::getUnfilteredDistortion( AlfCovariance* cov, const int numClasses )
{
  double dist = 0;
  for( int classIdx = 0; classIdx < numClasses; classIdx++ )
  {
    dist += cov[classIdx].pixAcc;
  }
  return dist;
}

double EncAdaptiveLoopFilter::getFilteredDistortion( AlfCovariance* cov, const int numClasses, const int numFiltersMinus1, const int numCoeff )
{
  double dist = 0;

  for( int classIdx = 0; classIdx < numClasses; classIdx++ )
  {
    dist += cov[classIdx].calcErrorForCoeffs(m_filterClippSet[classIdx], m_filterCoeffSet[classIdx], numCoeff,
                                             COEFF_SCALE_BITS);
  }

  return dist;
}

double EncAdaptiveLoopFilter::mergeFiltersAndCost(
  AlfParam& alfParam, AlfFilterShape& alfShape, AlfCovariance* covFrame, AlfCovariance* covMerged,
  AlfClipIdx clipMerged[MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_LUMA_COEFF], int& coeffBitsFinal)
{
  int numFiltersBest = 0;
  int numFilters = MAX_NUM_ALF_CLASSES;
  bool   codedVarBins[MAX_NUM_ALF_CLASSES];
  double errorForce0CoeffTab[MAX_NUM_ALF_CLASSES][2];

  double cost, cost0, dist, distForce0, costMin = MAX_DOUBLE;
  int coeffBits, coeffBitsForce0;

  mergeClasses( alfShape, covFrame, covMerged, clipMerged, MAX_NUM_ALF_CLASSES, m_filterIndices );

  while( numFilters >= 1 )
  {
    dist = deriveFilterCoeffs(covFrame, covMerged, clipMerged, alfShape, m_filterIndices[numFilters - 1], numFilters, errorForce0CoeffTab, alfParam);
    // filter coeffs are stored in m_filterCoeffSet
    distForce0 = getDistForce0( alfShape, numFilters, errorForce0CoeffTab, codedVarBins );
    coeffBits       = deriveFilterCoefficientsPredictionMode(alfShape, m_filterCoeffSet, numFilters);
    coeffBitsForce0 = getCostFilterCoeffForce0( alfShape, m_filterCoeffSet, numFilters, codedVarBins );

    cost = dist + m_lambda[COMPONENT_Y] * coeffBits;
    cost0 = distForce0 + m_lambda[COMPONENT_Y] * coeffBitsForce0;

    if( cost0 < cost )
    {
      cost = cost0;
    }

    if( cost <= costMin )
    {
      costMin = cost;
      numFiltersBest = numFilters;
    }
    numFilters--;
  }

  dist = deriveFilterCoeffs( covFrame, covMerged, clipMerged, alfShape, m_filterIndices[numFiltersBest - 1], numFiltersBest, errorForce0CoeffTab, alfParam );
  coeffBits       = deriveFilterCoefficientsPredictionMode(alfShape, m_filterCoeffSet, numFiltersBest);
  distForce0 = getDistForce0( alfShape, numFiltersBest, errorForce0CoeffTab, codedVarBins );
  coeffBitsForce0 = getCostFilterCoeffForce0( alfShape, m_filterCoeffSet, numFiltersBest, codedVarBins );

  cost = dist + m_lambda[COMPONENT_Y] * coeffBits;
  cost0 = distForce0 + m_lambda[COMPONENT_Y] * coeffBitsForce0;

  alfParam.numLumaFilters = numFiltersBest;
  double distReturn;
  if (cost <= cost0)
  {
    distReturn     = dist;
    coeffBitsFinal = coeffBits;

    alfParam.alfLumaCoeffDeltaFlag = 0;
  }
  else
  {
    distReturn     = distForce0;
    coeffBitsFinal = coeffBitsForce0;

    alfParam.alfLumaCoeffDeltaFlag = 1;
    memcpy( alfParam.alfLumaCoeffFlag, codedVarBins, sizeof( codedVarBins ) );

    for( int varInd = 0; varInd < numFiltersBest; varInd++ )
    {
      if( codedVarBins[varInd] == 0 )
      {
        std::fill_n(m_filterCoeffSet[varInd], MAX_NUM_ALF_LUMA_COEFF, 0);
        std::fill_n(m_filterClippSet[varInd], MAX_NUM_ALF_LUMA_COEFF, 0);
      }
    }
  }

  for( int ind = 0; ind < alfParam.numLumaFilters; ++ind )
  {
    for( int i = 0; i < alfShape.numCoeff; i++ )
    {
      alfParam.lumaCoeff[ind * MAX_NUM_ALF_LUMA_COEFF + i] = m_filterCoeffSet[ind][i];
      alfParam.lumaClipp[ind * MAX_NUM_ALF_LUMA_COEFF + i] = m_filterClippSet[ind][i];
    }
  }

  memcpy(alfParam.filterCoeffDeltaIdx, m_filterIndices[numFiltersBest - 1], sizeof(AlfBankIdx) * MAX_NUM_ALF_CLASSES);
  coeffBitsFinal += getNonFilterCoeffRate(alfParam);
  return distReturn;
}

int EncAdaptiveLoopFilter::getNonFilterCoeffRate( AlfParam& alfParam )
{
  int len = 0   // alf_coefficients_delta_flag
          + 2                                          // slice_alf_chroma_idc                     u(2)
          + lengthUvlc (alfParam.numLumaFilters - 1);  // alf_luma_num_filters_signalled_minus1   ue(v)

  if( alfParam.numLumaFilters > 1 )
  {
    const int coeffLength = ceilLog2(alfParam.numLumaFilters);
    for( int i = 0; i < MAX_NUM_ALF_CLASSES; i++ )
    {
      len += coeffLength;                              // alf_luma_coeff_delta_idx   u(v)
    }
  }
  return len;
}

int EncAdaptiveLoopFilter::getCostFilterCoeffForce0(AlfFilterShape& alfShape, AlfCoeff** pDiffQFilterCoeffIntPP,
                                                    const int numFilters, bool* codedVarBins)
{
  int len = 0;
  // Filter coefficients
  for( int ind = 0; ind < numFilters; ++ind )
  {
    if( codedVarBins[ind] )
    {
      for( int i = 0; i < alfShape.numCoeff - 1; i++ )
      {
        len += lengthUvlc( abs( pDiffQFilterCoeffIntPP[ ind ][ i ] ) ); // alf_coeff_luma_delta[i][j]
        if( ( abs( pDiffQFilterCoeffIntPP[ ind ][ i ] ) != 0 ) )
          len += 1;
      }
    }
    else
    {
      for (int i = 0; i < alfShape.numCoeff - 1; i++)
      {
        len += lengthUvlc( 0 ); // alf_coeff_luma_delta[i][j]
      }
    }
  }
  if (m_alfParamTemp.nonLinearFlag[ChannelType::LUMA])
  {
    for (int ind = 0; ind < numFilters; ++ind)
    {
      for (int i = 0; i < alfShape.numCoeff - 1; i++)
      {
        if (!abs(pDiffQFilterCoeffIntPP[ind][i]))
        {
          m_filterClippSet[ind][i] = 0;
        }
        len += 2;
      }
    }
  }

  return len;
}

int EncAdaptiveLoopFilter::deriveFilterCoefficientsPredictionMode(AlfFilterShape& alfShape, AlfCoeff** filterSet,
                                                                  const int numFilters)
{
  return (m_alfParamTemp.nonLinearFlag[ChannelType::LUMA] ? getCostFilterClipp(alfShape, filterSet, numFilters) : 0)
         + getCostFilterCoeff(alfShape, filterSet, numFilters);
}

int EncAdaptiveLoopFilter::getCostFilterCoeff(AlfFilterShape& alfShape, AlfCoeff** pDiffQFilterCoeffIntPP,
                                              const int numFilters)
{
  return lengthFilterCoeffs( alfShape, numFilters, pDiffQFilterCoeffIntPP );  // alf_coeff_luma_delta[i][j];
}

int EncAdaptiveLoopFilter::getCostFilterClipp(AlfFilterShape& alfShape, AlfCoeff** pDiffQFilterCoeffIntPP,
                                              const int numFilters)
{
  for (int filterIdx = 0; filterIdx < numFilters; ++filterIdx)
  {
    for (int i = 0; i < alfShape.numCoeff - 1; i++)
    {
      if (!abs(pDiffQFilterCoeffIntPP[filterIdx][i]))
      {
        m_filterClippSet[filterIdx][i] = 0;
      }
    }
  }
  return (numFilters * (alfShape.numCoeff - 1)) << 1;
}

int EncAdaptiveLoopFilter::lengthFilterCoeffs(AlfFilterShape& alfShape, const int numFilters, AlfCoeff** FilterCoeff)
{
  int bitCnt = 0;

  for( int ind = 0; ind < numFilters; ++ind )
  {
    for( int i = 0; i < alfShape.numCoeff - 1; i++ )
    {
      bitCnt += lengthUvlc( abs( FilterCoeff[ ind ][ i ] ) );
      if( abs( FilterCoeff[ ind ][ i ] ) != 0 )
      {
        bitCnt += 1;
      }
    }
  }
  return bitCnt;
}


double EncAdaptiveLoopFilter::getDistForce0( AlfFilterShape& alfShape, const int numFilters, double errorTabForce0Coeff[MAX_NUM_ALF_CLASSES][2], bool* codedVarBins )
{
  int bitsVarBin[MAX_NUM_ALF_CLASSES];

  for( int ind = 0; ind < numFilters; ++ind )
  {
    bitsVarBin[ind] = 0;
    for( int i = 0; i < alfShape.numCoeff - 1; i++ )
    {
      bitsVarBin[ ind ] += lengthUvlc( abs( m_filterCoeffSet[ ind ][ i ] ) );
      if( abs( m_filterCoeffSet[ ind ][ i ] ) != 0 )
      {
        bitsVarBin[ ind ] += 1;
      }
    }
  }

  int zeroBitsVarBin = 0;
  for (int i = 0; i < alfShape.numCoeff - 1; i++)
  {
    zeroBitsVarBin += lengthUvlc( 0 );
  }
  if (m_alfParamTemp.nonLinearFlag[ChannelType::LUMA])
  {
    for (int ind = 0; ind < numFilters; ++ind)
    {
      for (int i = 0; i < alfShape.numCoeff - 1; i++)
      {
        if (!abs(m_filterCoeffSet[ind][i]))
        {
          m_filterClippSet[ind][i] = 0;
        }
      }
    }
  }

  double distForce0 = getDistCoeffForce0( codedVarBins, errorTabForce0Coeff, bitsVarBin, zeroBitsVarBin, numFilters);

  return distForce0;
}

double EncAdaptiveLoopFilter::getDistCoeffForce0( bool* codedVarBins, double errorForce0CoeffTab[MAX_NUM_ALF_CLASSES][2], int* bitsVarBin, int zeroBitsVarBin, const int numFilters)
{
  double distForce0 = 0;
  std::memset( codedVarBins, 0, sizeof( *codedVarBins ) * MAX_NUM_ALF_CLASSES );

  for( int filtIdx = 0; filtIdx < numFilters; filtIdx++ )
  {
    double costDiff = (errorForce0CoeffTab[filtIdx][0] + m_lambda[COMPONENT_Y] * zeroBitsVarBin) - (errorForce0CoeffTab[filtIdx][1] + m_lambda[COMPONENT_Y] * bitsVarBin[filtIdx]);
    codedVarBins[filtIdx] = costDiff > 0 ? true : false;
    distForce0 += errorForce0CoeffTab[filtIdx][codedVarBins[filtIdx] ? 1 : 0];
  }

  return distForce0;
}

int EncAdaptiveLoopFilter::lengthUvlc(int code)
{
  CHECK(code < 0,        "Unsigned VLC cannot be negative");
  CHECK(code == MAX_INT, "Maximum supported UVLC code is MAX_INT-1");

  int length = 1;
  int temp = ++code;

  while (1 != temp)
  {
    temp >>= 1;
    length += 2;
  }
  // Take care of cases where length > 32
  return (length >> 1) + ((length + 1) >> 1);
}

double EncAdaptiveLoopFilter::deriveFilterCoeffs(
  AlfCovariance* cov, AlfCovariance* covMerged,
  AlfClipIdx clipMerged[MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_LUMA_COEFF], AlfFilterShape& alfShape,
  AlfBankIdx* filterIndices, int numFilters, double errorTabForce0Coeff[MAX_NUM_ALF_CLASSES][2], AlfParam& alfParam)
{
  double error = 0.0;
  AlfCovariance& tmpCov = covMerged[MAX_NUM_ALF_CLASSES];

  for( int filtIdx = 0; filtIdx < numFilters; filtIdx++ )
  {
    tmpCov.reset();
    bool found_clip = false;
    for( int classIdx = 0; classIdx < MAX_NUM_ALF_CLASSES; classIdx++ )
    {
      if( filterIndices[classIdx] == filtIdx )
      {
        tmpCov += cov[classIdx];
        if( !found_clip )
        {
          found_clip = true; // clip should be at the adress of shortest one
          std::copy_n(clipMerged[numFilters - 1][classIdx], MAX_NUM_ALF_LUMA_COEFF, m_filterClippSet[filtIdx]);
        }
      }
    }

    // Find coeffcients
    assert(alfShape.numCoeff == tmpCov.numCoeff);
    errorTabForce0Coeff[filtIdx][1] = tmpCov.pixAcc
                                      + deriveCoeffQuant(m_filterClippSet[filtIdx], m_filterCoeffSet[filtIdx], tmpCov,
                                                         alfShape, COEFF_SCALE_BITS, false);
    errorTabForce0Coeff[filtIdx][0] = tmpCov.pixAcc;
    error += errorTabForce0Coeff[filtIdx][1];
  }
  return error;
}

double EncAdaptiveLoopFilter::deriveCoeffQuant(AlfClipIdx* filterClipp, AlfCoeff* filterCoeffQuant,
                                               const AlfCovariance& cov, const AlfFilterShape& shape,
                                               const int fractionalBits, const bool optimizeClip)
{
  const AlfCoeff factor   = 1 << fractionalBits;
  const AlfCoeff maxValue = factor - 1;
  const AlfCoeff minValue = -factor + 1;

  const int numCoeff = shape.numCoeff;
  double    filterCoeff[MAX_NUM_ALF_LUMA_COEFF];
  double cAc, bc, coeffDelta, cA[MAX_NUM_ALF_LUMA_COEFF];

  cov.optimizeFilter( shape, filterClipp, filterCoeff, optimizeClip );
  roundFiltCoeff( filterCoeffQuant, filterCoeff, numCoeff, factor );

  for ( int i = 0; i < numCoeff - 1; i++ )
  {
    filterCoeffQuant[i] = std::min(maxValue, std::max(minValue, filterCoeffQuant[i]));
  }
  filterCoeffQuant[numCoeff - 1] = 0;

  int modified=1;
  bool isLumaFilter = numCoeff > 7 ? 1 : 0;
  if ((isLumaFilter && m_encCfg->getALFStrengthLuma() != 1.0) || (!isLumaFilter && m_encCfg->getALFStrengthChroma() != 1.0))
  {
    modified = 0;
  }
  double errRef = cov.calcErrorForCoeffs(filterClipp, filterCoeffQuant, numCoeff, fractionalBits);
  cov.calcInitErrorForCoeffs(&cAc, cA, &bc, filterClipp, filterCoeffQuant, numCoeff, fractionalBits);
  while( modified )
  {
    modified=0;
    for( int sign: {1, -1} )
    {
      double errMin = MAX_DOUBLE;
      int minInd = -1;
      coeffDelta = ( double )-sign / ( double )factor;

      for( int k = 0; k < numCoeff-1; k++ )
      {
        if (filterCoeffQuant[k] - sign > maxValue || filterCoeffQuant[k] - sign < minValue)
        {
          continue;
        }

        filterCoeffQuant[k] -= sign;

        double error = cov.calcErrorForCoeffsDelta(cAc, cA, bc, filterClipp, filterCoeffQuant, numCoeff, coeffDelta, k);
        if( error < errMin )
        {
          errMin = error;
          minInd = k;
        }
        filterCoeffQuant[k] += sign;
      }
      if( errMin < errRef )
      {
        filterCoeffQuant[minInd] -= sign;
        modified++;
        errRef = errMin;
        cov.updateErrorForCoeffsDelta(&cAc, cA, &bc, filterClipp, filterCoeffQuant, numCoeff, coeffDelta, minInd);
      }
    }
  }

  return errRef;
}

void EncAdaptiveLoopFilter::roundFiltCoeff(AlfCoeff* filterCoeffQuant, double* filterCoeff, const int numCoeff,
                                           const AlfCoeff factor)
{
  bool isLumaFilter = numCoeff > 7 ? 1 : 0;
  double alfStrength = isLumaFilter ? m_encCfg->getALFStrengthLuma() : m_encCfg->getALFStrengthChroma();
  for( int i = 0; i < numCoeff; i++ )
  {
    const int sign      = sgn2(filterCoeff[i]);
    const double absVal    = filterCoeff[i] * alfStrength * sign;
    filterCoeffQuant[i]    = int(std::min(absVal, 2.0) * factor + 0.5) * sign;
  }
}

void EncAdaptiveLoopFilter::roundFiltCoeffCCALF(AlfCoeff* filterCoeffQuant, double* filterCoeff, const int numCoeff,
                                                const AlfCoeff factor)
{
  for (int i = 0; i < numCoeff; i++)
  {
    const int sign = sgn2(filterCoeff[i]);

    double bestErr   = factor * factor;
    int    bestIndex = 0;

    const double val = filterCoeff[i] * m_encCfg->getCCALFStrength() * sign * factor;

    for (int k = 0; k < CCALF_CANDS_COEFF_NR; k++)
    {
      const double diff = val - CCALF_SMALL_TAB[k];
      const double err  = diff * diff;

      if (err < bestErr)
      {
        bestErr   = err;
        bestIndex = k;
      }
    }

    filterCoeffQuant[i] = CCALF_SMALL_TAB[bestIndex] * sign;
  }
}

void EncAdaptiveLoopFilter::mergeClasses(
  const AlfFilterShape& alfShape, AlfCovariance* cov, AlfCovariance* covMerged,
  AlfClipIdx clipMerged[MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_LUMA_COEFF], const int numClasses,
  AlfBankIdx filterIndices[MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_CLASSES])
{
  AlfClipIdx tmpClip[MAX_NUM_ALF_LUMA_COEFF];
  AlfClipIdx bestMergeClip[MAX_NUM_ALF_LUMA_COEFF];
  double  err[MAX_NUM_ALF_CLASSES];
  double  bestMergeErr = std::numeric_limits<double>::max();
  bool    availableClass[MAX_NUM_ALF_CLASSES];
  uint8_t indexList[MAX_NUM_ALF_CLASSES];
  uint8_t indexListTemp[MAX_NUM_ALF_CLASSES];
  int numRemaining = numClasses;

  std::fill_n(filterIndices[0], MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_CLASSES, 0);

  for( int i = 0; i < numClasses; i++ )
  {
    filterIndices[numRemaining - 1][i] = i;
    indexList[i] = i;
    availableClass[i] = true;
    covMerged[i] = cov[i];
    covMerged[i].numBins = m_alfParamTemp.nonLinearFlag[ChannelType::LUMA] ? ALF_NUM_CLIP_VALS[ChannelType::LUMA] : 1;
  }

  // Try merging different covariance matrices

  // temporal AlfCovariance structure is allocated as the last element in covMerged array, the size of covMerged is MAX_NUM_ALF_CLASSES + 1
  AlfCovariance& tmpCov = covMerged[MAX_NUM_ALF_CLASSES];
  tmpCov.numBins        = m_alfParamTemp.nonLinearFlag[ChannelType::LUMA] ? ALF_NUM_CLIP_VALS[ChannelType::LUMA] : 1;

  // init Clip
  for( int i = 0; i < numClasses; i++ )
  {
    AlfClipIdx* clipIdxs = clipMerged[numRemaining - 1][i];
    std::fill_n(clipIdxs, MAX_NUM_ALF_LUMA_COEFF,
                m_alfParamTemp.nonLinearFlag[ChannelType::LUMA] ? ALF_NUM_CLIP_VALS[ChannelType::LUMA] / 2 : 0);
    if (m_alfParamTemp.nonLinearFlag[ChannelType::LUMA])
    {
      err[i] = covMerged[i].optimizeFilterClip(alfShape, clipIdxs);
    }
    else
    {
      err[i] = covMerged[i].calculateError(clipIdxs);
    }
  }

  while( numRemaining >= 2 )
  {
    double errorMin = std::numeric_limits<double>::max();
    int bestToMergeIdx1 = 0, bestToMergeIdx2 = 1;

    for( int i = 0; i < numClasses - 1; i++ )
    {
      if( availableClass[i] )
      {
        for( int j = i + 1; j < numClasses; j++ )
        {
          if( availableClass[j] )
          {
            double error1 = err[i];
            double error2 = err[j];

            tmpCov.add( covMerged[i], covMerged[j] );
            for( int l = 0; l < MAX_NUM_ALF_LUMA_COEFF; ++l )
            {
              tmpClip[l] = (clipMerged[numRemaining-1][i][l] + clipMerged[numRemaining-1][j][l] + 1 ) >> 1;
            }
            double errorMerged = m_alfParamTemp.nonLinearFlag[ChannelType::LUMA]
                                   ? tmpCov.optimizeFilterClip(alfShape, tmpClip)
                                   : tmpCov.calculateError(tmpClip);
            double error = errorMerged - error1 - error2;

            if( error < errorMin )
            {
              bestMergeErr = errorMerged;
              memcpy(bestMergeClip, tmpClip, sizeof(bestMergeClip));
              errorMin = error;
              bestToMergeIdx1 = i;
              bestToMergeIdx2 = j;
            }
          }
        }
      }
    }

    covMerged[bestToMergeIdx1] += covMerged[bestToMergeIdx2];
    std::copy_n(&clipMerged[numRemaining - 1][0][0], MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF, &clipMerged[numRemaining - 2][0][0]);
    std::copy_n(bestMergeClip, MAX_NUM_ALF_LUMA_COEFF, clipMerged[numRemaining - 2][bestToMergeIdx1]);
    err[bestToMergeIdx1] = bestMergeErr;
    availableClass[bestToMergeIdx2] = false;

    for( int i = 0; i < numClasses; i++ )
    {
      if( indexList[i] == bestToMergeIdx2 )
      {
        indexList[i] = bestToMergeIdx1;
      }
    }

    numRemaining--;
    if( numRemaining <= numClasses )
    {
      std::memcpy( indexListTemp, indexList, sizeof( uint8_t ) * numClasses );

      bool exist = false;
      int ind = 0;

      for( int j = 0; j < numClasses; j++ )
      {
        exist = false;
        for( int i = 0; i < numClasses; i++ )
        {
          if( indexListTemp[i] == j )
          {
            exist = true;
            break;
          }
        }

        if( exist )
        {
          for( int i = 0; i < numClasses; i++ )
          {
            if( indexListTemp[i] == j )
            {
              filterIndices[numRemaining - 1][i] = ind;
              indexListTemp[i] = -1;
            }
          }
          ind++;
        }
      }
    }
  }
}

void EncAdaptiveLoopFilter::getFrameStats(ChannelType channel, int shapeIdx)
{
  int numClasses = isLuma( channel ) ? MAX_NUM_ALF_CLASSES : 1;
  int numAlternatives = isLuma( channel ) ? 1 : m_alfParamTemp.numAlternativesChroma;
  // When calling this function m_ctuEnableFlag shall be set to 0 for CTUs using alternative APS
  // Here we compute frame stats for building new alternative filters
  for( int altIdx = 0; altIdx < numAlternatives; ++altIdx )
  {
    for( int i = 0; i < numClasses; i++ )
    {
      m_alfCovarianceFrame[channel][shapeIdx][isLuma(channel) ? i : altIdx].reset(ALF_NUM_CLIP_VALS[channel]);
    }
    if( isLuma( channel ) )
    {
      getFrameStat(m_alfCovarianceFrame[ChannelType::LUMA][shapeIdx], m_alfCovariance[COMPONENT_Y][shapeIdx],
                   m_modes[COMPONENT_Y], numClasses, altIdx);
    }
    else
    {
      getFrameStat(m_alfCovarianceFrame[ChannelType::CHROMA][shapeIdx], m_alfCovariance[COMPONENT_Cb][shapeIdx],
                   m_modes[COMPONENT_Cb], numClasses, altIdx);
      getFrameStat(m_alfCovarianceFrame[ChannelType::CHROMA][shapeIdx], m_alfCovariance[COMPONENT_Cr][shapeIdx],
                   m_modes[COMPONENT_Cr], numClasses, altIdx);
    }
  }
}

void EncAdaptiveLoopFilter::getFrameStat(AlfCovariance *frameCov, AlfCovariance **ctbCov, AlfMode *ctbEnableFlags,
                                         const int numClasses, int altIdx)
{
  const ChannelType channel = numClasses > 1 ? ChannelType::LUMA : ChannelType::CHROMA;
  for( int ctuIdx = 0; ctuIdx < m_numCTUsInPic; ctuIdx++ )
  {
    if (ctbEnableFlags[ctuIdx] != AlfMode::OFF)
    {
      for( int classIdx = 0; classIdx < numClasses; classIdx++ )
      {
        if (isLuma(channel) || altIdx == ctbEnableFlags[ctuIdx] - AlfMode::CHROMA0)
        {
          frameCov[isLuma( channel ) ? classIdx : altIdx] += ctbCov[ctuIdx][classIdx];
        }
      }
    }
  }
}

void EncAdaptiveLoopFilter::deriveStatsForFiltering( PelUnitBuf& orgYuv, PelUnitBuf& recYuv, CodingStructure& cs )
{
  int ctuRsAddr = 0;
  const int numberOfComponents = getNumberValidComponents( m_chromaFormat );

  // init CTU stats buffers
  for( int compIdx = 0; compIdx < numberOfComponents; compIdx++ )
  {
    const ComponentID compID = ComponentID( compIdx );
    const int numClasses = isLuma( compID ) ? MAX_NUM_ALF_CLASSES : 1;

    for (int shape = 0; shape != m_filterShapes[toChannelType(compID)].size(); shape++)
    {
      for( int classIdx = 0; classIdx < numClasses; classIdx++ )
      {
        for( int ctuIdx = 0; ctuIdx < m_numCTUsInPic; ctuIdx++ )
        {
          m_alfCovariance[compIdx][shape][ctuIdx][classIdx].reset(ALF_NUM_CLIP_VALS[toChannelType(compID)]);
        }
      }
    }
  }

  // init Frame stats buffers
  for (auto chType = ChannelType::LUMA; chType <= ::getLastChannel(m_chromaFormat); chType++)
  {
    const int numAlts    = isLuma(chType) ? 1 : ALF_MAX_NUM_ALTERNATIVES_CHROMA;
    const int numClasses = isLuma(chType) ? MAX_NUM_ALF_CLASSES : 1;

    for( int altIdx = 0; altIdx < numAlts; ++altIdx )
    {
      for (int shape = 0; shape != m_filterShapes[chType].size(); shape++)
      {
        for (int classIdx = 0; classIdx < numClasses; classIdx++)
        {
          m_alfCovarianceFrame[chType][shape][isLuma(chType) ? classIdx : altIdx].reset(ALF_NUM_CLIP_VALS[chType]);
        }
      }
    }
  }

  const PreCalcValues& pcv = *cs.pcv;
  bool clipTop = false, clipBottom = false, clipLeft = false, clipRight = false;
  int numHorVirBndry = 0, numVerVirBndry = 0;
  int horVirBndryPos[] = { 0, 0, 0 };
  int verVirBndryPos[] = { 0, 0, 0 };

  for( int yPos = 0; yPos < m_picHeight; yPos += m_maxCUHeight )
  {
    for( int xPos = 0; xPos < m_picWidth; xPos += m_maxCUWidth )
    {
      const int width = ( xPos + m_maxCUWidth > m_picWidth ) ? ( m_picWidth - xPos ) : m_maxCUWidth;
      const int height = ( yPos + m_maxCUHeight > m_picHeight ) ? ( m_picHeight - yPos ) : m_maxCUHeight;
      int rasterSliceAlfPad = 0;
      if( isCrossedByVirtualBoundaries( cs, xPos, yPos, width, height, clipTop, clipBottom, clipLeft, clipRight, numHorVirBndry, numVerVirBndry, horVirBndryPos, verVirBndryPos, rasterSliceAlfPad ) )
      {
        int yStart = yPos;
        for( int i = 0; i <= numHorVirBndry; i++ )
        {
          const int yEnd = i == numHorVirBndry ? yPos + height : horVirBndryPos[i];
          const int h = yEnd - yStart;
          const bool clipT = ( i == 0 && clipTop ) || ( i > 0 ) || ( yStart == 0 );
          const bool clipB = ( i == numHorVirBndry && clipBottom ) || ( i < numHorVirBndry ) || ( yEnd == pcv.lumaHeight );
          int xStart = xPos;
          for( int j = 0; j <= numVerVirBndry; j++ )
          {
            const int xEnd = j == numVerVirBndry ? xPos + width : verVirBndryPos[j];
            const int w = xEnd - xStart;
            const bool clipL = ( j == 0 && clipLeft ) || ( j > 0 ) || ( xStart == 0 );
            const bool clipR = ( j == numVerVirBndry && clipRight ) || ( j < numVerVirBndry ) || ( xEnd == pcv.lumaWidth );
            const int wBuf = w + (clipL ? 0 : MAX_ALF_PADDING_SIZE) + (clipR ? 0 : MAX_ALF_PADDING_SIZE);
            const int hBuf = h + (clipT ? 0 : MAX_ALF_PADDING_SIZE) + (clipB ? 0 : MAX_ALF_PADDING_SIZE);
            PelUnitBuf recBuf = m_tempBuf2.subBuf( UnitArea( cs.area.chromaFormat, Area( 0, 0, wBuf, hBuf ) ) );
            recBuf.copyFrom( recYuv.subBuf( UnitArea( cs.area.chromaFormat, Area( xStart - (clipL ? 0 : MAX_ALF_PADDING_SIZE), yStart - (clipT ? 0 : MAX_ALF_PADDING_SIZE), wBuf, hBuf ) ) ) );
            // pad top-left unavailable samples for raster slice
            if ( xStart == xPos && yStart == yPos && ( rasterSliceAlfPad & 1 ) )
            {
              recBuf.padBorderPel( MAX_ALF_PADDING_SIZE, 1 );
            }

            // pad bottom-right unavailable samples for raster slice
            if ( xEnd == xPos + width && yEnd == yPos + height && ( rasterSliceAlfPad & 2 ) )
            {
              recBuf.padBorderPel( MAX_ALF_PADDING_SIZE, 2 );
            }
            recBuf.extendBorderPel( MAX_ALF_PADDING_SIZE );
            recBuf = recBuf.subBuf( UnitArea ( cs.area.chromaFormat, Area( clipL ? 0 : MAX_ALF_PADDING_SIZE, clipT ? 0 : MAX_ALF_PADDING_SIZE, w, h ) ) );

            const UnitArea area( m_chromaFormat, Area( 0, 0, w, h ) );
            const UnitArea areaDst( m_chromaFormat, Area( xStart, yStart, w, h ) );
            for( int compIdx = 0; compIdx < numberOfComponents; compIdx++ )
            {
              const ComponentID compID = ComponentID( compIdx );
              const CompArea& compArea = area.block( compID );

              ptrdiff_t recStride = recBuf.get(compID).stride;
              Pel* rec = recBuf.get( compID ).bufAt( compArea );

              ptrdiff_t orgStride = orgYuv.get(compID).stride;
              Pel* org = orgYuv.get(compID).bufAt(xStart >> ::getComponentScaleX(compID, m_chromaFormat), yStart >> ::getComponentScaleY(compID, m_chromaFormat));

              ptrdiff_t orgLumaStride = orgYuv.get(COMPONENT_Y).stride;
              Pel      *orgLuma       = orgYuv.get(COMPONENT_Y).bufAt(xStart, yStart);

              ChannelType chType = toChannelType( compID );

              for (int shape = 0; shape != m_filterShapes[chType].size(); shape++)
              {
                const CompArea &compAreaDst = areaDst.block(compID);
                getBlkStats(m_alfCovariance[compIdx][shape][ctuRsAddr], m_filterShapes[chType][shape],
                            compIdx ? nullptr : m_classifier, org, orgStride, orgLuma, orgLumaStride, rec, recStride,
                            compAreaDst, compArea, chType,
                            ((compIdx == 0) ? m_alfVBLumaCTUHeight : m_alfVBChmaCTUHeight),
                            (compIdx == 0) ? m_alfVBLumaPos : m_alfVBChmaPos);
              }
            }

            xStart = xEnd;
          }

          yStart = yEnd;
        }

        for( int compIdx = 0; compIdx < numberOfComponents; compIdx++ )
        {
          const ComponentID compID = ComponentID( compIdx );

          ChannelType chType = toChannelType( compID );

          for (int shape = 0; shape != m_filterShapes[chType].size(); shape++)
          {
            const int numClasses = isLuma( compID ) ? MAX_NUM_ALF_CLASSES : 1;

            for( int classIdx = 0; classIdx < numClasses; classIdx++ )
            {
              m_alfCovarianceFrame[chType][shape][isLuma(compID) ? classIdx : 0] +=
                m_alfCovariance[compIdx][shape][ctuRsAddr][classIdx];
            }
          }
        }
      }
      else
      {
        const UnitArea area(m_chromaFormat, Area(xPos, yPos, width, height));

        for (int compIdx = 0; compIdx < numberOfComponents; compIdx++)
        {
          const ComponentID compID   = ComponentID(compIdx);
          const CompArea &  compArea = area.block(compID);

          ptrdiff_t recStride = recYuv.get(compID).stride;
          Pel *rec       = recYuv.get(compID).bufAt(compArea);

          ptrdiff_t orgStride = orgYuv.get(compID).stride;
          Pel *org       = orgYuv.get(compID).bufAt(compArea);

          ptrdiff_t orgLumaStride = orgYuv.get(COMPONENT_Y).stride;
          Pel      *orgLuma       = orgYuv.get(COMPONENT_Y).bufAt(area.block(COMPONENT_Y));

          ChannelType chType = toChannelType(compID);

          for (int shape = 0; shape != m_filterShapes[chType].size(); shape++)
          {
            getBlkStats(m_alfCovariance[compIdx][shape][ctuRsAddr], m_filterShapes[chType][shape],
                        compIdx ? nullptr : m_classifier, org, orgStride, orgLuma, orgLumaStride, rec, recStride,
                        compArea, compArea, chType, ((compIdx == 0) ? m_alfVBLumaCTUHeight : m_alfVBChmaCTUHeight),
                        (compIdx == 0) ? m_alfVBLumaPos : m_alfVBChmaPos);

            const int numClasses = isLuma(compID) ? MAX_NUM_ALF_CLASSES : 1;

            for (int classIdx = 0; classIdx < numClasses; classIdx++)
            {
              m_alfCovarianceFrame[chType][shape][isLuma(compID) ? classIdx : 0] +=
                m_alfCovariance[compIdx][shape][ctuRsAddr][classIdx];
            }
          }
        }
      }
      ctuRsAddr++;
    }
  }
}

void EncAdaptiveLoopFilter::getBlkStats(AlfCovariance *alfCovariance, const AlfFilterShape &shape,
                                        AlfClassifier **classifier, Pel *org, const ptrdiff_t orgStride,
                                        const Pel *orgLuma, const ptrdiff_t orgLumaStride, Pel *rec,
                                        const ptrdiff_t recStride, const CompArea &areaDst, const CompArea &area,
                                        const ChannelType channel, int vbCTUHeight, int vbPos)
{
  Pel ELocal[MAX_NUM_ALF_LUMA_COEFF][MAX_ALF_NUM_CLIP_VALS];

  const int numBins = ALF_NUM_CLIP_VALS[channel];

  const double strength =
    isLuma(channel) ? m_encCfg->getALFStrengthTargetLuma() : m_encCfg->getALFStrengthTargetChroma();
  const double invStrength = strength != 0.0 ? 1.0 / strength : 0.0;

  for( int i = 0; i < area.height; i++ )
  {
    const int vbDistance = ((areaDst.y + i) % vbCTUHeight) - vbPos;
    for( int j = 0; j < area.width; j++ )
    {
      std::fill_n(ELocal[0], MAX_NUM_ALF_LUMA_COEFF * MAX_ALF_NUM_CLIP_VALS, 0);

      int transposeIdx = 0;
      int classIdx     = 0;
      if( classifier )
      {
        AlfClassifier& cl = classifier[areaDst.y + i][areaDst.x + j];
        transposeIdx = cl.transposeIdx;
        classIdx = cl.classIdx;
      }

      calcCovariance(ELocal, rec + j, recStride, shape, transposeIdx, channel, vbDistance);

      const ComponentID compID  = isLuma(channel) ? COMPONENT_Y : COMPONENT_Cb;
      const Pel        *lumaPtr = orgLuma + (i << ::getComponentScaleY(compID, m_chromaFormat)) * orgLumaStride
                           + (j << ::getComponentScaleX(compID, m_chromaFormat));
      const double weight = m_alfWSSD ? m_lumaLevelToWeightPLUT[*lumaPtr] : 1.0;
      const double yLocal = org[j] - rec[j];

      double e[MAX_ALF_NUM_CLIP_VALS][MAX_NUM_ALF_LUMA_COEFF];

      for (int b = 0; b < numBins; b++)
      {
        for (int k = 0; k < shape.numCoeff; k++)
        {
          e[b][k] = invStrength * ELocal[k][b];
        }
      }

      for (int b0 = 0; b0 < numBins; b0++)
      {
        for (int k = 0; k < shape.numCoeff; k++)
        {
          const double we = weight * e[b0][k];

          for (int b1 = 0; b1 <= b0; b1++)
          {
            const int maxl = b0 == b1 ? k + 1 : shape.numCoeff;

            for (int l = 0; l < maxl; l++)
            {
              const ptrdiff_t oe = alfCovariance[classIdx].getOffsetEfast(b0, b1, k, l);
              alfCovariance[classIdx].data[oe] += we * e[b1][l];
            }
          }
          alfCovariance[classIdx].y(b0, k) += we * yLocal;
        }
      }
      alfCovariance[classIdx].pixAcc += weight * yLocal * yLocal;
    }
    org += orgStride;
    rec += recStride;
  }
}

void EncAdaptiveLoopFilter::calcCovariance(Pel ELocal[MAX_NUM_ALF_LUMA_COEFF][MAX_ALF_NUM_CLIP_VALS], const Pel *rec,
                                           const ptrdiff_t stride, const AlfFilterShape &shape, const int transposeIdx,
                                           const ChannelType channel, int vbDistance)
{
  int clipTopRow = -4;
  int clipBotRow = 4;
  if (vbDistance >= -3 && vbDistance < 0)
  {
    clipBotRow = -vbDistance - 1;
    clipTopRow = -clipBotRow; // symmetric
  }
  else if (vbDistance >= 0 && vbDistance < 3)
  {
    clipTopRow = -vbDistance;
    clipBotRow = -clipTopRow; // symmetric
  }
  const int *filterPattern = shape.pattern.data();
  const int halfFilterLength = shape.filterLength >> 1;
  const Pel *clip             = m_alfClippingValues[channel].data();

  const int numBins = ALF_NUM_CLIP_VALS[channel];

  int k = 0;

  const Pel curr = rec[0];

  if( transposeIdx == 0 )
  {
    for( int i = -halfFilterLength; i < 0; i++ )
    {
      const Pel* rec0 = rec + std::max(i, clipTopRow) * stride;
      const Pel* rec1 = rec - std::max(i, -clipBotRow) * stride;
      for( int j = -halfFilterLength - i; j <= halfFilterLength + i; j++, k++ )
      {
        for( int b = 0; b < numBins; b++ )
        {
          ELocal[filterPattern[k]][b] += clipALF(clip[b], curr, rec0[j], rec1[-j]);
        }
      }
    }
    for( int j = -halfFilterLength; j < 0; j++, k++ )
    {
      for( int b = 0; b < numBins; b++ )
      {
        ELocal[filterPattern[k]][b] += clipALF(clip[b], curr, rec[j], rec[-j]);
      }
    }
  }
  else if( transposeIdx == 1 )
  {
    for( int j = -halfFilterLength; j < 0; j++ )
    {
      const Pel* rec0 = rec + j;
      const Pel* rec1 = rec - j;
      for (int i = -halfFilterLength - j; i <= halfFilterLength + j; i++, k++)
      {
        for (int b = 0; b < numBins; b++)
        {
          ELocal[filterPattern[k]][b] += clipALF(clip[b], curr, rec0[std::max(i, clipTopRow) * stride], rec1[-std::max(i, -clipBotRow) * stride]);
        }
      }
    }
    for (int i = -halfFilterLength; i < 0; i++, k++)
    {
      for (int b = 0; b < numBins; b++)
      {
        ELocal[filterPattern[k]][b] += clipALF(clip[b], curr, rec[std::max(i, clipTopRow) * stride], rec[-std::max(i, -clipBotRow) * stride]);
      }
    }
  }
  else if( transposeIdx == 2 )
  {
    for( int i = -halfFilterLength; i < 0; i++ )
    {
      const Pel* rec0 = rec + std::max(i, clipTopRow) * stride;
      const Pel* rec1 = rec - std::max(i, -clipBotRow) * stride;

      for( int j = halfFilterLength + i; j >= -halfFilterLength - i; j--, k++ )
      {
        for( int b = 0; b < numBins; b++ )
        {
          ELocal[filterPattern[k]][b] += clipALF(clip[b], curr, rec0[j], rec1[-j]);
        }
      }
    }
    for( int j = -halfFilterLength; j < 0; j++, k++ )
    {
      for( int b = 0; b < numBins; b++ )
      {
        ELocal[filterPattern[k]][b] += clipALF(clip[b], curr, rec[j], rec[-j]);
      }
    }
  }
  else
  {
    for( int j = -halfFilterLength; j < 0; j++ )
    {
      const Pel* rec0 = rec + j;
      const Pel* rec1 = rec - j;
      for (int i = halfFilterLength + j; i >= -halfFilterLength - j; i--, k++)
      {
        for (int b = 0; b < numBins; b++)
        {
          ELocal[filterPattern[k]][b] += clipALF(clip[b], curr, rec0[std::max(i, clipTopRow) * stride], rec1[-std::max(i, -clipBotRow) * stride]);
        }
      }
    }
    for (int i = -halfFilterLength; i < 0; i++, k++)
    {
      for (int b = 0; b < numBins; b++)
      {
        ELocal[filterPattern[k]][b] += clipALF(clip[b], curr, rec[std::max(i, clipTopRow) * stride], rec[-std::max(i, -clipBotRow) * stride]);
      }
    }

  }
  for( int b = 0; b < numBins; b++ )
  {
    ELocal[filterPattern[k]][b] += curr;
  }
}

void EncAdaptiveLoopFilter::setSliceEnabledFlag(AlfParam &alfSlicePara, ChannelType channel, bool val)
{
  if (isLuma(channel))
  {
    alfSlicePara.enabledFlag[COMPONENT_Y] = val;
  }
  else
  {
    alfSlicePara.enabledFlag[COMPONENT_Cb] = val;
    alfSlicePara.enabledFlag[COMPONENT_Cr] = val;
  }
}

void EncAdaptiveLoopFilter::setSliceEnabledFlag(AlfParam &alfSlicePara, ChannelType channel,
                                                AlfMode *ctuFlags[MAX_NUM_COMPONENT])
{
  const ComponentID compIDFirst = isLuma( channel ) ? COMPONENT_Y : COMPONENT_Cb;
  const ComponentID compIDLast = isLuma( channel ) ? COMPONENT_Y : COMPONENT_Cr;
  for( int compId = compIDFirst; compId <= compIDLast; compId++ )
  {
    alfSlicePara.enabledFlag[compId] =
      std::any_of(ctuFlags[compId], ctuFlags[compId] + m_numCTUsInPic, [](AlfMode x) { return x != AlfMode::OFF; });
  }
}

void EncAdaptiveLoopFilter::copyIndices(AlfMode *ctuFlagsDst[MAX_NUM_COMPONENT],
                                        AlfMode *ctuFlagsSrc[MAX_NUM_COMPONENT], ChannelType channel)
{
  if( isLuma( channel ) )
  {
    std::copy_n(ctuFlagsSrc[COMPONENT_Y], m_numCTUsInPic, ctuFlagsDst[COMPONENT_Y]);
  }
  else
  {
    std::copy_n(ctuFlagsSrc[COMPONENT_Cb], m_numCTUsInPic, ctuFlagsDst[COMPONENT_Cb]);
    std::copy_n(ctuFlagsSrc[COMPONENT_Cr], m_numCTUsInPic, ctuFlagsDst[COMPONENT_Cr]);
  }
}

void EncAdaptiveLoopFilter::setCtuEnableFlag(AlfMode *ctuFlags[MAX_NUM_COMPONENT], ChannelType channel, AlfMode val)
{
  if( isLuma( channel ) )
  {
    std::fill_n(ctuFlags[COMPONENT_Y], m_numCTUsInPic, val);
  }
  else
  {
    std::fill_n(ctuFlags[COMPONENT_Cb], m_numCTUsInPic, val);
    std::fill_n(ctuFlags[COMPONENT_Cr], m_numCTUsInPic, val);
  }
}

int EncAdaptiveLoopFilter::getAvailableApsIdsLuma(CodingStructure &cs)
{
  APS** apss = cs.slice->getAlfAPSs();
  const int firstApsId = m_encCfg->getALFAPSIDShift();
  const int lastApsId  = firstApsId + m_encCfg->getMaxNumALFAPS();

  for (int i = firstApsId; i < lastApsId; i++)
  {
    apss[i] = m_apsMap->getPS(i);
  }

  AlfApsList result;

  int curApsId = m_apsIdStart;

  if (curApsId < lastApsId && !cs.slice->isIRAP() && !cs.slice->getPendingRasInit())
  {
    for (int i = 0; i < m_encCfg->getMaxNumALFAPS(); i++)
    {
      APS* curAPS = apss[curApsId];

      if (curAPS != nullptr && curAPS->getLayerId() == cs.slice->getPic()->layerId
          && curAPS->getTemporalId() <= cs.slice->getTLayer()
          && curAPS->getAlfAPSParam().newFilterFlag[ChannelType::LUMA])
      {
        result.push_back(curApsId);
      }
      if (++curApsId >= lastApsId)
      {
        curApsId = firstApsId;
      }
    }
  }

  cs.slice->setNumAlfApsIdsLuma((int)result.size());
  cs.slice->setAlfApsIdsLuma(result);

  int newApsId = m_apsIdStart - 1;
  if (newApsId < firstApsId)
  {
    newApsId = lastApsId - 1;
  }
  CHECK(newApsId >= lastApsId, "Wrong APS index assignment");

  return newApsId;
}

void  EncAdaptiveLoopFilter::initDistortion()
{
  for (int comp = 0; comp < MAX_NUM_COMPONENT; comp++)
  {
    for (int ctbIdx = 0; ctbIdx < m_numCTUsInPic; ctbIdx++)
    {
      m_ctbDistortionUnfilter[comp][ctbIdx] = getUnfilteredDistortion(m_alfCovariance[comp][0][ctbIdx], comp == 0 ? MAX_NUM_ALF_CLASSES : 1);
    }
  }
}

void  EncAdaptiveLoopFilter::initDistortionCcalf()
{
  for (int comp = 1; comp < MAX_NUM_COMPONENT; comp++)
  {
    for (int ctbIdx = 0; ctbIdx < m_numCTUsInPic; ctbIdx++)
    {
      m_ctbDistortionUnfilter[comp][ctbIdx] = m_alfCovarianceCcAlf[comp - 1][0][ctbIdx].pixAcc;
    }
  }
}

void  EncAdaptiveLoopFilter::alfEncoderCtb(CodingStructure& cs, AlfParam& alfParamNewFilters
#if ENABLE_QPA
  , const double lambdaChromaWeight
#endif
)
{
  TempCtx        ctxStart(m_ctxPool, AlfCtx(m_CABACEstimator->getCtx()));
  TempCtx        ctxBest(m_ctxPool);
  TempCtx        ctxTempStart(m_ctxPool);
  TempCtx        ctxTempBest(m_ctxPool);
  TempCtx        ctxTempAltStart(m_ctxPool);
  TempCtx        ctxTempAltBest(m_ctxPool);
  AlfParam  alfParamNewFiltersBest = alfParamNewFilters;
  
  APS **apss = cs.slice->getAlfAPSs();

  initDistortion();

  //luma
  m_alfParamTemp = alfParamNewFilters;
  setCtuEnableFlag(m_modes, ChannelType::LUMA, AlfMode::LUMA0);
  getFrameStats(ChannelType::LUMA, 0);
  setCtuEnableFlag(m_modes, ChannelType::LUMA, AlfMode::OFF);
  double costOff = getUnfilteredDistortion(m_alfCovarianceFrame[ChannelType::LUMA][0], ChannelType::LUMA);

  const int         newApsId = getAvailableApsIdsLuma(cs);
  const AlfApsList &apsIds   = cs.slice->getAlfApsIdsLuma();
  AlfApsList        bestApsIds;
  double costMin = MAX_DOUBLE;
  reconstructCoeffAPSs(cs, true, false, true);

  m_lumaNewAps = 0;

  for (bool useNewFilter: { false, true })
  {
    if (useNewFilter && !alfParamNewFilters.enabledFlag[COMPONENT_Y])
    {
      continue;
    }

    int bitsNewFilter = 0;
    if (useNewFilter)
    {
      bitsNewFilter = m_bitsNewFilter[ChannelType::LUMA];
      reconstructCoeff(alfParamNewFilters, ChannelType::LUMA, true, true);
    }

    for (int numTemporalAps = 0; numTemporalAps <= apsIds.size(); numTemporalAps++)
    {
      const int numApss = numTemporalAps + (useNewFilter ? 1 : 0);

      if (m_encCfg->getMaxNumALFAPS() == 0 && numApss > 0)
      {
        continue;
      }

      if (numApss > std::min(ALF_CTB_MAX_NUM_APS - 1, m_encCfg->getMaxNumALFAPS()))
      {
        continue;
      }

      cs.slice->setNumAlfApsIdsLuma(numApss);

      const int numFilterSet = ALF_NUM_FIXED_FILTER_SETS + numApss;

      if (numTemporalAps == apsIds.size() && numTemporalAps > 0 && useNewFilter && newApsId == apsIds.back())
      {
        // last temporalAPS is occupied by new filter set and this temporal APS becomes unavailable
        continue;
      }

      const int numIter = useNewFilter ? 2 : 1;

      for (int iter = 0; iter < numIter; iter++)
      {
        m_alfParamTemp = alfParamNewFilters;
        m_alfParamTemp.enabledFlag[COMPONENT_Y] = true;

        double curCost = 3 * m_lambda[COMPONENT_Y];

        if (iter > 0)
        {
          // re-derive new filter-set
          double dDistOrgNewFilter = 0;
          int blocksUsingNewFilter = 0;
          for (int ctbIdx = 0; ctbIdx < m_numCTUsInPic; ctbIdx++)
          {
            if (m_modes[COMPONENT_Y][ctbIdx] != AlfMode::LUMA0)
            {
              m_modes[COMPONENT_Y][ctbIdx] = AlfMode::OFF;
            }
            else
            {
              blocksUsingNewFilter++;
              dDistOrgNewFilter += m_ctbDistortionUnfilter[COMPONENT_Y][ctbIdx];
              for (int classIdx = 0; classIdx < MAX_NUM_ALF_CLASSES; classIdx++)
              {
                AlfCoeff* pCoeff   = m_coeffFinal;
                AlfClipIdx* pClipp   = m_clippFinal;
                for (int i = 0; i < MAX_NUM_ALF_LUMA_COEFF; i++)
                {
                  m_filterTmp[i] = pCoeff[classIdx * MAX_NUM_ALF_LUMA_COEFF + i];
                  m_clipTmp[i] = pClipp[classIdx * MAX_NUM_ALF_LUMA_COEFF + i];
                }
                dDistOrgNewFilter += m_alfCovariance[COMPONENT_Y][0][ctbIdx][classIdx].calcErrorForCoeffs(
                  m_clipTmp, m_filterTmp, MAX_NUM_ALF_LUMA_COEFF, COEFF_SCALE_BITS);
              }
            }
          }

          if (blocksUsingNewFilter > 0 && blocksUsingNewFilter < m_numCTUsInPic)
          {
            int bitNL[2] = { 0, 0 };
            double errNL[2] = { MAX_DOUBLE, MAX_DOUBLE };

            for (bool nonlinearFlag: { true, false })
            {
              if (nonlinearFlag && !m_encCfg->getUseNonLinearAlfLuma())
              {
                continue;
              }

              m_alfParamTemp.nonLinearFlag[ChannelType::LUMA] = nonlinearFlag;

              const int idx = nonlinearFlag ? 1 : 0;
              errNL[idx]    = getFilterCoeffAndCost(cs, 0, ChannelType::LUMA, true, 0, bitNL[idx], true);

              if (nonlinearFlag)
              {
                m_alfParamTempNL = m_alfParamTemp;
              }
            }

            const bool   useNonlinear          = errNL[1] < errNL[0];
            const int    bitsNewFilterTempLuma = bitNL[useNonlinear ? 1 : 0];
            const double err                   = errNL[useNonlinear ? 1 : 0];

            if (useNonlinear)
            {
              m_alfParamTemp = m_alfParamTempNL;
            }

            if (dDistOrgNewFilter + m_lambda[COMPONENT_Y] * m_bitsNewFilter[ChannelType::LUMA] < err)
            {
              // re-derived filter is not good, skip
              continue;
            }

            reconstructCoeff(m_alfParamTemp, ChannelType::LUMA, true, true);
            bitsNewFilter = bitsNewFilterTempLuma;
          }
          else
          {
            // no block or all blocks using new filter, skip
            continue;
          }
        }

        m_CABACEstimator->getCtx() = ctxStart;
        for (int ctbIdx = 0; ctbIdx < m_numCTUsInPic; ctbIdx++)
        {
          double distUnfilterCtb = m_ctbDistortionUnfilter[COMPONENT_Y][ctbIdx];
          // ctb on
          double         costOn = MAX_DOUBLE;
          ctxTempStart = AlfCtx(m_CABACEstimator->getCtx());
          int bestFilterSetIdx  = 0;

          const int firstFilterSetIdx = m_encCfg->getALFAllowPredefinedFilters() ? 0 : ALF_NUM_FIXED_FILTER_SETS;

          for (int filterSetIdx = firstFilterSetIdx; filterSetIdx < numFilterSet; filterSetIdx++)
          {
            m_modes[COMPONENT_Y][ctbIdx] = AlfMode::LUMA_FIXED0 + filterSetIdx;

            //rate
            m_CABACEstimator->getCtx() = AlfCtx(ctxTempStart);
            m_CABACEstimator->resetBits();
            m_CABACEstimator->codeAlfCtuEnableFlag(cs, ctbIdx, COMPONENT_Y, &m_alfParamTemp);
            m_CABACEstimator->codeAlfCtuFilterIndex(cs, ctbIdx, m_alfParamTemp.enabledFlag[COMPONENT_Y]);
            double rateOn = FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();

            //distortion
            double dist = distUnfilterCtb;
            for (int classIdx = 0; classIdx < MAX_NUM_ALF_CLASSES; classIdx++)
            {
              if (filterSetIdx < ALF_NUM_FIXED_FILTER_SETS)
              {
                int filterIdx = m_classToFilterMapping[filterSetIdx][classIdx];
                dist += m_alfCovariance[COMPONENT_Y][0][ctbIdx][classIdx].calcErrorForCoeffs(
                  m_clipDefaultEnc, m_fixedFilterSetCoeff[filterIdx], MAX_NUM_ALF_LUMA_COEFF, COEFF_SCALE_BITS);
              }
              else
              {
                AlfCoeff* pCoeff;
                AlfClipIdx* pClipp;
                if (useNewFilter && filterSetIdx == ALF_NUM_FIXED_FILTER_SETS)
                {
                  pCoeff = m_coeffFinal;
                  pClipp = m_clippFinal;
                }
                else
                {
                  pCoeff = m_coeffApsLuma[filterSetIdx - (useNewFilter ? 1 : 0) - ALF_NUM_FIXED_FILTER_SETS];
                  pClipp = m_clippApsLuma[filterSetIdx - (useNewFilter ? 1 : 0) - ALF_NUM_FIXED_FILTER_SETS];
                }

                for (int i = 0; i < MAX_NUM_ALF_LUMA_COEFF; i++)
                {
                  m_filterTmp[i] = pCoeff[classIdx * MAX_NUM_ALF_LUMA_COEFF + i];
                  m_clipTmp[i] = pClipp[classIdx * MAX_NUM_ALF_LUMA_COEFF + i];
                }
                dist += m_alfCovariance[COMPONENT_Y][0][ctbIdx][classIdx].calcErrorForCoeffs(
                  m_clipTmp, m_filterTmp, MAX_NUM_ALF_LUMA_COEFF, COEFF_SCALE_BITS);
              }
            }
            //cost
            double costOnTmp = dist + m_lambda[COMPONENT_Y] * rateOn;
            if (costOnTmp < costOn)
            {
              ctxTempBest = AlfCtx(m_CABACEstimator->getCtx());
              costOn = costOnTmp;
              bestFilterSetIdx = filterSetIdx;
            }
          }
          //ctb off
          m_modes[COMPONENT_Y][ctbIdx] = AlfMode::OFF;
          //rate
          m_CABACEstimator->getCtx() = AlfCtx(ctxTempStart);
          m_CABACEstimator->resetBits();
          m_CABACEstimator->codeAlfCtuEnableFlag(cs, ctbIdx, COMPONENT_Y, &m_alfParamTemp);
          //cost
          double costOff =
            distUnfilterCtb + m_lambda[COMPONENT_Y] * FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();
          if (costOn < costOff)
          {
            m_CABACEstimator->getCtx() = AlfCtx(ctxTempBest);
            m_modes[COMPONENT_Y][ctbIdx] = AlfMode::LUMA_FIXED0 + bestFilterSetIdx;
            curCost += costOn;
          }
          else
          {
            curCost += costOff;
          }
        } //for(ctbIdx)

        int tmpBits = bitsNewFilter + 3 * (numFilterSet - ALF_NUM_FIXED_FILTER_SETS);
        curCost += tmpBits * m_lambda[COMPONENT_Y];
        if (curCost < costMin)
        {
          costMin = curCost;
          bestApsIds.resize(numFilterSet - ALF_NUM_FIXED_FILTER_SETS);
          for (int i = 0; i < bestApsIds.size(); i++)
          {
            if (i == 0 && useNewFilter)
            {
              bestApsIds[i] = newApsId;
            }
            else
            {
              bestApsIds[i] = apsIds[i - (useNewFilter ? 1 : 0)];
            }
          }
          alfParamNewFiltersBest = m_alfParamTemp;
          ctxBest = AlfCtx(m_CABACEstimator->getCtx());
          copyIndices(m_indexTmp, m_modes, ChannelType::LUMA);
          alfParamNewFiltersBest.newFilterFlag[ChannelType::LUMA] = useNewFilter;
        }
      }//for (int iter = 0; iter < numIter; iter++)
    }// for (int numTemporalAps = 0; numTemporalAps < apsIds.size(); numTemporalAps++)
  }//for (int useNewFilter = 0; useNewFilter <= 1; useNewFilter++)

  cs.slice->setCcAlfCbApsId(newApsId);
  cs.slice->setCcAlfCrApsId(newApsId);

  if (costOff <= costMin)
  {
    cs.slice->resetAlfEnabledFlag();
    cs.slice->setNumAlfApsIdsLuma(0);
    setCtuEnableFlag(m_modes, ChannelType::LUMA, AlfMode::OFF);
    setCtuEnableFlag(m_modes, ChannelType::CHROMA, AlfMode::OFF);
    return;
  }
  else
  {
    cs.slice->setAlfEnabledFlag(COMPONENT_Y, true);
    cs.slice->setNumAlfApsIdsLuma((int)bestApsIds.size());
    cs.slice->setAlfApsIdsLuma(bestApsIds);

    copyIndices(m_modes, m_indexTmp, ChannelType::LUMA);

    if (alfParamNewFiltersBest.newFilterFlag[ChannelType::LUMA])
    {
      if (cs.slice->getSliceType() != I_SLICE)
      {
        m_lumaNewAps = 1;
      }
      APS *newAPS = m_apsMap->getPS(newApsId);
      if (newAPS == nullptr)
      {
        newAPS = m_apsMap->allocatePS(newApsId);
        newAPS->setAPSId(newApsId);
        newAPS->setAPSType(ApsType::ALF);
      }
      newAPS->setAlfAPSParam(alfParamNewFiltersBest);
      newAPS->setTemporalId( cs.slice->getTLayer() );
      newAPS->getAlfAPSParam().newFilterFlag[ChannelType::CHROMA] = false;
      m_apsMap->setChangedFlag(newApsId);
      m_apsIdStart = newApsId;
    }

    const AlfApsList &apsIds = cs.slice->getAlfApsIdsLuma();
    for (int i = 0; i < (int)cs.slice->getNumAlfApsIdsLuma(); i++)
    {
      apss[apsIds[i]] = m_apsMap->getPS(apsIds[i]);
    }
  }

  //chroma
  if (m_encCfg->getMaxNumALFAPS() != 0
      && isChromaEnabled(cs.pcv->chrFormat))   //  Find ALF parameters for chroma if ALF APS is enabled.
  {
    m_alfParamTemp = alfParamNewFiltersBest;
    if (m_alfParamTemp.numAlternativesChroma < 1)
    {
      m_alfParamTemp.numAlternativesChroma = 1;
    }
    setCtuEnableFlag(m_modes, ChannelType::CHROMA, AlfMode::CHROMA0);
    getFrameStats(ChannelType::CHROMA, 0);
    costOff = getUnfilteredDistortion(m_alfCovarianceFrame[ChannelType::CHROMA][0], ChannelType::CHROMA);
    costMin = MAX_DOUBLE;
    m_CABACEstimator->getCtx() = AlfCtx(ctxBest);
    ctxStart                   = AlfCtx(m_CABACEstimator->getCtx());
    int newApsIdChroma         = -1;
    if (alfParamNewFiltersBest.newFilterFlag[ChannelType::LUMA]
        && (alfParamNewFiltersBest.enabledFlag[COMPONENT_Cb] || alfParamNewFiltersBest.enabledFlag[COMPONENT_Cr]))
    {
      newApsIdChroma = newApsId;
    }
    else if (alfParamNewFiltersBest.enabledFlag[COMPONENT_Cb] || alfParamNewFiltersBest.enabledFlag[COMPONENT_Cr])
    {
      int curId = m_apsIdStart;
      // Do not assign ALF APS for chroma if any new APS ID is not avaiable

      int counter = m_encCfg->getMaxNumALFAPS();
      while ((newApsIdChroma < 0) && ((counter--)))
      {
        curId--;
        if (curId < m_encCfg->getALFAPSIDShift())
        {
          curId = m_encCfg->getALFAPSIDShift() + m_encCfg->getMaxNumALFAPS() - 1;
        }
        if (std::find(bestApsIds.begin(), bestApsIds.end(), curId) == bestApsIds.end())
        {
          newApsIdChroma = curId;
        }
      }
    }

    int chromaHisApsNums = 0;
    for (int curApsId = 0; curApsId < ALF_CTB_MAX_NUM_APS; curApsId++)
    {
      if ((cs.slice->getPendingRasInit() || cs.slice->isIRAP()) && curApsId != newApsIdChroma)
      {
        continue;
      }
      APS *curAPS = m_apsMap->getPS(curApsId);

      if (curApsId != newApsIdChroma && curAPS && curAPS->getTemporalId() <= cs.slice->getTLayer()
          && curAPS->getLayerId() == cs.slice->getPic()->layerId
          && curAPS->getAlfAPSParam().newFilterFlag[ChannelType::CHROMA])
      {
        chromaHisApsNums++;
      }
      else
      {
        continue;
      }
    }

    setLambdaFactor(cs, chromaHisApsNums, m_lumaNewAps);
    
    for (int curApsId = m_encCfg->getALFAPSIDShift(); curApsId < m_encCfg->getALFAPSIDShift() + m_encCfg->getMaxNumALFAPS(); curApsId++)
    {
      const bool reuseExistingAPS = curApsId != newApsIdChroma;

      if ((cs.slice->getPendingRasInit() || cs.slice->isIRAP()) && reuseExistingAPS)
      {
        continue;
      }
      APS *curAPS = m_apsMap->getPS(curApsId);
      double curCost = m_lambda[COMPONENT_Cb] * m_lambdaFactor * 3;
      
      if (!reuseExistingAPS)
      {
        m_alfParamTemp = alfParamNewFilters;
        curCost += m_lambda[COMPONENT_Cb] * m_lambdaFactor * m_bitsNewFilter[ChannelType::CHROMA];
      }
      else if (curAPS && curAPS->getTemporalId() <= cs.slice->getTLayer()
               && curAPS->getLayerId() == cs.slice->getPic()->layerId
               && curAPS->getAlfAPSParam().newFilterFlag[ChannelType::CHROMA])
      {
        m_alfParamTemp = curAPS->getAlfAPSParam();
      }
      else
      {
        continue;
      }
      reconstructCoeff(m_alfParamTemp, ChannelType::CHROMA, true, true);
      m_CABACEstimator->getCtx() = AlfCtx(ctxStart);
      for (int compId = 1; compId < MAX_NUM_COMPONENT; compId++)
      {
        m_alfParamTemp.enabledFlag[compId] = true;
        for (int ctbIdx = 0; ctbIdx < m_numCTUsInPic; ctbIdx++)
        {
          // set to first mode for rate calculation of CTU enable flag
          m_modes[compId][ctbIdx] = AlfMode::CHROMA0;

          double distUnfilterCtu = m_ctbDistortionUnfilter[compId][ctbIdx];
          // cost on
          ctxTempStart = AlfCtx(m_CABACEstimator->getCtx());
          // rate
          m_CABACEstimator->getCtx() = AlfCtx(ctxTempStart);
          m_CABACEstimator->resetBits();
          // ctb flag
          m_CABACEstimator->codeAlfCtuEnableFlag(cs, ctbIdx, compId, &m_alfParamTemp);
          double rateOn = FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();
#if ENABLE_QPA
          const double ctuLambda = lambdaChromaWeight > 0.0 ? cs.picture->m_uEnerHpCtu[ctbIdx] / lambdaChromaWeight
                                                            : m_lambda[compId] * m_lambdaFactor;
#else
          const double ctuLambda = m_lambda[compId] * m_lambdaFactor;
#endif
          double dist        = MAX_DOUBLE;
          int    numAlts     = m_alfParamTemp.numAlternativesChroma;
          ctxTempBest        = AlfCtx(m_CABACEstimator->getCtx());
          double bestAltRate = 0;
          double bestAltCost = MAX_DOUBLE;
          int    bestAltIdx  = -1;
          ctxTempAltStart    = AlfCtx(ctxTempBest);
          for (int altIdx = 0; altIdx < numAlts; ++altIdx)
          {
            m_modes[compId][ctbIdx] = AlfMode::CHROMA0 + altIdx;

            if (altIdx)
            {
              m_CABACEstimator->getCtx() = AlfCtx(ctxTempAltStart);
            }
            m_CABACEstimator->resetBits();
            m_CABACEstimator->codeAlfCtuAlternative(cs, ctbIdx, compId, &m_alfParamTemp);
            double altRate   = FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();
            double r_altCost = ctuLambda * altRate;

            // distortion
            for (int i = 0; i < MAX_NUM_ALF_CHROMA_COEFF; i++)
            {
              m_filterTmp[i] = m_chromaCoeffFinal[altIdx][i];
              m_clipTmp[i]   = m_chromaClippFinal[altIdx][i];
            }
            double altDist = m_alfCovariance[compId][0][ctbIdx][0].calcErrorForCoeffs(
              m_clipTmp, m_filterTmp, MAX_NUM_ALF_CHROMA_COEFF, COEFF_SCALE_BITS);
            double altCost = altDist + r_altCost;
            if (altCost < bestAltCost)
            {
              bestAltCost = altCost;
              bestAltIdx  = altIdx;
              bestAltRate = altRate;
              ctxTempBest = AlfCtx(m_CABACEstimator->getCtx());
              dist        = altDist;
            }
          }
          rateOn += bestAltRate;
          dist += distUnfilterCtu;
          // cost
          double costOn = dist + ctuLambda * rateOn;
          // cost off
          m_modes[compId][ctbIdx] = AlfMode::OFF;
          // rate
          m_CABACEstimator->getCtx() = AlfCtx(ctxTempStart);
          m_CABACEstimator->resetBits();
          m_CABACEstimator->codeAlfCtuEnableFlag(cs, ctbIdx, compId, &m_alfParamTemp);
          // cost
          double costOff = distUnfilterCtu + m_lambda[compId] * m_lambdaFactor * FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();
          if (costOn < costOff)
          {
            m_CABACEstimator->getCtx()      = AlfCtx(ctxTempBest);
            m_modes[compId][ctbIdx]         = AlfMode::CHROMA0 + bestAltIdx;
            curCost += costOn;
          }
          else
          {
            curCost += costOff;
          }
        }
      }
      // chroma idc
      setSliceEnabledFlag(m_alfParamTemp, ChannelType::CHROMA, m_modes);

      if (curCost < costMin)
      {
        costMin = curCost;
        cs.slice->setAlfApsIdChroma(curApsId);
        cs.slice->setAlfEnabledFlag(COMPONENT_Cb, m_alfParamTemp.enabledFlag[COMPONENT_Cb]);
        cs.slice->setAlfEnabledFlag(COMPONENT_Cr, m_alfParamTemp.enabledFlag[COMPONENT_Cr]);
        copyIndices(m_indexTmp, m_modes, ChannelType::CHROMA);
      }
    }

    if (newApsIdChroma >= 0)
    {
      cs.slice->setCcAlfCbApsId(newApsIdChroma);
      cs.slice->setCcAlfCrApsId(newApsIdChroma);
    }
    if (costOff < costMin)
    {
      cs.slice->setAlfEnabledFlag(COMPONENT_Cb, false);
      cs.slice->setAlfEnabledFlag(COMPONENT_Cr, false);
      setCtuEnableFlag(m_modes, ChannelType::CHROMA, AlfMode::OFF);
    }
    else
    {
      copyIndices(m_modes, m_indexTmp, ChannelType::CHROMA);
      if (cs.slice->getAlfApsIdChroma() == newApsIdChroma)   // new filter
      {
        APS *newAPS = m_apsMap->getPS(newApsIdChroma);
        if (newAPS == nullptr)
        {
          newAPS = m_apsMap->allocatePS(newApsIdChroma);
          newAPS->setAPSType(ApsType::ALF);
          newAPS->setAPSId(newApsIdChroma);
          newAPS->getAlfAPSParam().reset();
        }
        newAPS->getAlfAPSParam().newFilterFlag[ChannelType::CHROMA] = true;
        if (!alfParamNewFiltersBest.newFilterFlag[ChannelType::LUMA])
        {
          newAPS->getAlfAPSParam().newFilterFlag[ChannelType::LUMA] = false;
        }
        newAPS->getAlfAPSParam().numAlternativesChroma = alfParamNewFilters.numAlternativesChroma;
        newAPS->getAlfAPSParam().nonLinearFlag[ChannelType::CHROMA] =
          alfParamNewFilters.nonLinearFlag[ChannelType::CHROMA];
        newAPS->setTemporalId(cs.slice->getTLayer());
        for (int altIdx = 0; altIdx < ALF_MAX_NUM_ALTERNATIVES_CHROMA; ++altIdx)
        {
          for (int i = 0; i < MAX_NUM_ALF_CHROMA_COEFF; i++)
          {
            newAPS->getAlfAPSParam().chromaCoeff[altIdx][i] = alfParamNewFilters.chromaCoeff[altIdx][i];
            newAPS->getAlfAPSParam().chromaClipp[altIdx][i] = alfParamNewFilters.chromaClipp[altIdx][i];
          }
        }
        m_apsMap->setChangedFlag(newApsIdChroma);
        m_apsIdStart = newApsIdChroma;
      }
      apss[cs.slice->getAlfApsIdChroma()] = m_apsMap->getPS(cs.slice->getAlfApsIdChroma());
    }
  }
}

void EncAdaptiveLoopFilter::alfReconstructor(CodingStructure& cs, const PelUnitBuf& recExtBuf)
{
  if (!cs.slice->getAlfEnabledFlag(COMPONENT_Y))
  {
    return;
  }
  reconstructCoeffAPSs(cs, true, cs.slice->getAlfEnabledFlag(COMPONENT_Cb) || cs.slice->getAlfEnabledFlag(COMPONENT_Cr),
                       false);
  PelUnitBuf& recBuf = cs.getRecoBufRef();
  const PreCalcValues& pcv = *cs.pcv;

  int ctuIdx = 0;
  bool clipTop = false, clipBottom = false, clipLeft = false, clipRight = false;
  int numHorVirBndry = 0, numVerVirBndry = 0;
  int horVirBndryPos[] = { 0, 0, 0 };
  int verVirBndryPos[] = { 0, 0, 0 };
  for (int yPos = 0; yPos < pcv.lumaHeight; yPos += pcv.maxCUHeight)
  {
    for (int xPos = 0; xPos < pcv.lumaWidth; xPos += pcv.maxCUWidth)
    {
      const int width = (xPos + pcv.maxCUWidth > pcv.lumaWidth) ? (pcv.lumaWidth - xPos) : pcv.maxCUWidth;
      const int height = (yPos + pcv.maxCUHeight > pcv.lumaHeight) ? (pcv.lumaHeight - yPos) : pcv.maxCUHeight;

      bool ctuEnableFlag = m_modes[COMPONENT_Y][ctuIdx] != AlfMode::OFF;
      for (int compIdx = 1; compIdx < MAX_NUM_COMPONENT; compIdx++)
      {
        ctuEnableFlag |= m_modes[compIdx][ctuIdx] != AlfMode::OFF;
      }
      int rasterSliceAlfPad = 0;
      if ( ctuEnableFlag && isCrossedByVirtualBoundaries( cs, xPos, yPos, width, height, clipTop, clipBottom, clipLeft, clipRight, numHorVirBndry, numVerVirBndry, horVirBndryPos, verVirBndryPos, rasterSliceAlfPad ) )
      {
        int yStart = yPos;
        for (int i = 0; i <= numHorVirBndry; i++)
        {
          const int yEnd = i == numHorVirBndry ? yPos + height : horVirBndryPos[i];
          const int h = yEnd - yStart;
          const bool clipT = (i == 0 && clipTop) || (i > 0) || (yStart == 0);
          const bool clipB = (i == numHorVirBndry && clipBottom) || (i < numHorVirBndry ) || (yEnd == pcv.lumaHeight);
          int xStart = xPos;
          for (int j = 0; j <= numVerVirBndry; j++)
          {
            const int xEnd = j == numVerVirBndry ? xPos + width : verVirBndryPos[j];
            const int w = xEnd - xStart;
            const bool clipL = (j == 0 && clipLeft) || (j > 0) || (xStart == 0);
            const bool clipR = (j == numVerVirBndry && clipRight) || (j < numVerVirBndry ) || (xEnd == pcv.lumaWidth);
            const int wBuf = w + (clipL ? 0 : MAX_ALF_PADDING_SIZE) + (clipR ? 0 : MAX_ALF_PADDING_SIZE);
            const int hBuf = h + (clipT ? 0 : MAX_ALF_PADDING_SIZE) + (clipB ? 0 : MAX_ALF_PADDING_SIZE);
            PelUnitBuf buf = m_tempBuf2.subBuf(UnitArea(cs.area.chromaFormat, Area(0, 0, wBuf, hBuf)));
            buf.copyFrom(recExtBuf.subBuf(UnitArea(cs.area.chromaFormat, Area(xStart - (clipL ? 0 : MAX_ALF_PADDING_SIZE), yStart - (clipT ? 0 : MAX_ALF_PADDING_SIZE), wBuf, hBuf))));
            // pad top-left unavailable samples for raster slice
            if ( xStart == xPos && yStart == yPos && ( rasterSliceAlfPad & 1 ) )
            {
              buf.padBorderPel( MAX_ALF_PADDING_SIZE, 1 );
            }

            // pad bottom-right unavailable samples for raster slice
            if ( xEnd == xPos + width && yEnd == yPos + height && ( rasterSliceAlfPad & 2 ) )
            {
              buf.padBorderPel( MAX_ALF_PADDING_SIZE, 2 );
            }
            buf.extendBorderPel(MAX_ALF_PADDING_SIZE);
            buf = buf.subBuf(UnitArea(cs.area.chromaFormat, Area(clipL ? 0 : MAX_ALF_PADDING_SIZE, clipT ? 0 : MAX_ALF_PADDING_SIZE, w, h)));

            if (m_modes[COMPONENT_Y][ctuIdx] != AlfMode::OFF)
            {
              const Area blkSrc(0, 0, w, h);
              const Area blkDst(xStart, yStart, w, h);
              const AlfMode m     = m_modes[COMPONENT_Y][ctuIdx];
              const AlfCoeff* coeff = getCoeffVals(m);
              const Pel*    clip  = getClipVals(m);
#if GREEN_METADATA_SEI_ENABLED
              cs.m_featureCounter.alfLumaType7+= (width * height / 16) ;
              cs.m_featureCounter.alfLumaPels += (width * height);
#endif
              m_filter7x7Blk(m_classifier, recBuf, buf, blkDst, blkSrc, COMPONENT_Y, coeff, clip,
                             m_clpRngs.comp[COMPONENT_Y], cs, m_alfVBLumaCTUHeight, m_alfVBLumaPos);
            }

            for (int compIdx = 1; compIdx < MAX_NUM_COMPONENT; compIdx++)
            {
              ComponentID compID = ComponentID(compIdx);
              const int chromaScaleX = getComponentScaleX(compID, recBuf.chromaFormat);
              const int chromaScaleY = getComponentScaleY(compID, recBuf.chromaFormat);
              if (m_modes[compIdx][ctuIdx] != AlfMode::OFF)
              {
                const Area blkSrc(0, 0, w >> chromaScaleX, h >> chromaScaleY);
                const Area blkDst(xStart >> chromaScaleX, yStart >> chromaScaleY, w >> chromaScaleX, h >> chromaScaleY);
                const int  altNum = m_modes[compID][ctuIdx] - AlfMode::CHROMA0;
                m_filter5x5Blk(m_classifier, recBuf, buf, blkDst, blkSrc, compID, m_chromaCoeffFinal[altNum],
                               m_chromaClipValsFinal[altNum], m_clpRngs.comp[compIdx], cs, m_alfVBChmaCTUHeight,
                               m_alfVBChmaPos);
#if GREEN_METADATA_SEI_ENABLED
                cs.m_featureCounter.alfChromaType5+= ((width >> chromaScaleX) * (height >> chromaScaleY) / 16);
                cs.m_featureCounter.alfChromaPels += ((width >> chromaScaleX) * (height >> chromaScaleY)) ;
#endif
              }
            }

            xStart = xEnd;
          }

          yStart = yEnd;
        }
      }
      else
      {
        const UnitArea area(cs.area.chromaFormat, Area(xPos, yPos, width, height));
        if (m_modes[COMPONENT_Y][ctuIdx] != AlfMode::OFF)
        {
          Area   blk(xPos, yPos, width, height);
          const AlfMode m     = m_modes[COMPONENT_Y][ctuIdx];
          const AlfCoeff* coeff = getCoeffVals(m);
          const Pel*    clip  = getClipVals(m);
          m_filter7x7Blk(m_classifier, recBuf, recExtBuf, blk, blk, COMPONENT_Y, coeff, clip,
                         m_clpRngs.comp[COMPONENT_Y], cs, m_alfVBLumaCTUHeight, m_alfVBLumaPos);
#if GREEN_METADATA_SEI_ENABLED
          cs.m_featureCounter.alfLumaType7+= (width * height / 16) ;
          cs.m_featureCounter.alfLumaPels += (width * height);
#endif
        }

        for (int compIdx = 1; compIdx < MAX_NUM_COMPONENT; compIdx++)
        {
          ComponentID compID       = ComponentID(compIdx);
          const int   chromaScaleX = getComponentScaleX(compID, recBuf.chromaFormat);
          const int   chromaScaleY = getComponentScaleY(compID, recBuf.chromaFormat);
          if (m_modes[compIdx][ctuIdx] != AlfMode::OFF)
          {
            Area      blk(xPos >> chromaScaleX, yPos >> chromaScaleY, width >> chromaScaleX, height >> chromaScaleY);
            const int altNum = m_modes[compID][ctuIdx] - AlfMode::CHROMA0;
            m_filter5x5Blk(m_classifier, recBuf, recExtBuf, blk, blk, compID, m_chromaCoeffFinal[altNum],
                           m_chromaClipValsFinal[altNum], m_clpRngs.comp[compIdx], cs, m_alfVBChmaCTUHeight,
                           m_alfVBChmaPos);
#if GREEN_METADATA_SEI_ENABLED
            cs.m_featureCounter.alfChromaType5+= ((width >> chromaScaleX) * (height >> chromaScaleY) / 16) ;
            cs.m_featureCounter.alfChromaPels += ((width >> chromaScaleX) * (height >> chromaScaleY)) ;
#endif
          }
        }
      }
      ctuIdx++;
    }
  }
}

void EncAdaptiveLoopFilter::initCtuAlternativeChroma(AlfMode *ctuAlts[MAX_NUM_COMPONENT])
{
  int altIdx = 0;
  for( int ctuIdx = 0; ctuIdx < m_numCTUsInPic; ++ctuIdx )
  {
    ctuAlts[COMPONENT_Cb][ctuIdx] = AlfMode::CHROMA0 + altIdx;
    ctuAlts[COMPONENT_Cr][ctuIdx] = AlfMode::CHROMA0 + altIdx;
    if( (ctuIdx+1) * m_alfParamTemp.numAlternativesChroma >= (altIdx+1)*m_numCTUsInPic )
    {
      ++altIdx;
    }
  }
}

int EncAdaptiveLoopFilter::getMaxNumAlternativesChroma( )
{
  return std::min<int>( m_numCTUsInPic * 2, m_encCfg->getMaxNumAlfAlternativesChroma() );
}

int EncAdaptiveLoopFilter::getCoeffRateCcAlf(AlfCoeff chromaCoeff[MAX_NUM_CC_ALF_FILTERS][MAX_NUM_CC_ALF_CHROMA_COEFF],
                                             bool filterEnabled[MAX_NUM_CC_ALF_FILTERS], uint8_t filterCount,
                                             ComponentID compID)
{
  int bits = 0;

  if ( filterCount > 0 )
  {
    bits += lengthUvlc(filterCount - 1);
    int signaledFilterCount = 0;
    for ( int filterIdx=0; filterIdx<MAX_NUM_CC_ALF_FILTERS; filterIdx++ )
    {
      if (filterEnabled[filterIdx])
      {
        AlfFilterShape alfShape(size_CC_ALF);
        // Filter coefficients
        for (int i = 0; i < alfShape.numCoeff - 1; i++)
        {
          bits += CCALF_BITS_PER_COEFF_LEVEL + (chromaCoeff[filterIdx][i] == 0 ? 0 : 1);
        }

        signaledFilterCount++;
      }
    }
    CHECK(signaledFilterCount != filterCount, "Number of filter signaled not same as indicated");
  }

  return bits;
}

void EncAdaptiveLoopFilter::deriveCcAlfFilterCoeff(
  ComponentID compID, const PelUnitBuf& recYuv, const PelUnitBuf& recYuvExt,
  AlfCoeff filterCoeff[MAX_NUM_CC_ALF_FILTERS][MAX_NUM_CC_ALF_CHROMA_COEFF], const uint8_t filterIdx)
{
  int forward_tab[CCALF_CANDS_COEFF_NR * 2 - 1] = {0};
  for (int i = 0; i < CCALF_CANDS_COEFF_NR; i++)
  {
    forward_tab[CCALF_CANDS_COEFF_NR - 1 + i] = CCALF_SMALL_TAB[i];
    forward_tab[CCALF_CANDS_COEFF_NR - 1 - i] = (-1) * CCALF_SMALL_TAB[i];
  }
  using TE = double[MAX_NUM_ALF_LUMA_COEFF][MAX_NUM_ALF_LUMA_COEFF];
  using Ty = double[MAX_NUM_ALF_LUMA_COEFF];

  double filterCoeffDbl[MAX_NUM_CC_ALF_CHROMA_COEFF];
  int16_t filterCoeffInt[MAX_NUM_CC_ALF_CHROMA_COEFF];

  std::fill_n(filterCoeffInt, MAX_NUM_CC_ALF_CHROMA_COEFF, 0);

  TE        kE;
  Ty        ky;
  const int size = m_filterShapesCcAlf[compID - 1][0].numCoeff - 1;

  for (int k = 0; k < size; k++)
  {
    ky[k] = m_alfCovarianceFrameCcAlf[compID - 1][0].y(0, k);
    for (int l = 0; l < size; l++)
    {
      kE[k][l] = m_alfCovarianceFrameCcAlf[compID - 1][0].E(0, 0, k, l);
    }
  }

  m_alfCovarianceFrameCcAlf[compID - 1][0].gnsSolveByChol(kE, ky, filterCoeffDbl, size);
  roundFiltCoeffCCALF(filterCoeffInt, filterCoeffDbl, size, 1 << COEFF_SCALE_BITS);

  for (int k = 0; k < size; k++)
  {
    CHECK( filterCoeffInt[k] < -(1 << CCALF_DYNAMIC_RANGE), "this is not possible: filterCoeffInt[k] <  -(1 << CCALF_DYNAMIC_RANGE)");
    CHECK( filterCoeffInt[k] > (1 << CCALF_DYNAMIC_RANGE), "this is not possible: filterCoeffInt[k] >  (1 << CCALF_DYNAMIC_RANGE)");
  }

  // Refine quanitzation
  int modified       = 1;
  if (m_encCfg->getCCALFStrength() != 1.0)
  {
    modified = 0;
  }
  double errRef =
    m_alfCovarianceFrameCcAlf[compID - 1][0].calcErrorForCcAlfCoeffs(filterCoeffInt, size, COEFF_SCALE_BITS + 1);
  while (modified)
  {
    modified = 0;
    for (int delta : { 1, -1 })
    {
      double errMin = MAX_DOUBLE;
      int    idxMin = -1;
      int minIndex = -1;

      for (int k = 0; k < size; k++)
      {
        int org_idx = -1;
        for (int i = 0; i < CCALF_CANDS_COEFF_NR * 2 - 1; i++)
        {
          if (forward_tab[i] == filterCoeffInt[k])
          {
            org_idx = i;
            break;
          }
        }
        CHECK( org_idx < 0, "this is wrong, does not find coeff from forward_tab");
        if ( (org_idx - delta < 0) || (org_idx - delta >= CCALF_CANDS_COEFF_NR * 2 - 1) )
        {
          continue;
        }

        filterCoeffInt[k] = forward_tab[org_idx - delta];

        double error =
          m_alfCovarianceFrameCcAlf[compID - 1][0].calcErrorForCcAlfCoeffs(filterCoeffInt, size, COEFF_SCALE_BITS + 1);
        if( error < errMin )
        {
          errMin = error;
          idxMin = k;
          minIndex = org_idx;
        }
        filterCoeffInt[k] = forward_tab[org_idx];
      }
      if (errMin < errRef)
      {
        minIndex -= delta;
        CHECK( minIndex < 0, "this is wrong, index - delta < 0");
        CHECK( minIndex >= CCALF_CANDS_COEFF_NR * 2 - 1, "this is wrong, index - delta >= CCALF_CANDS_COEFF_NR * 2 - 1");
        filterCoeffInt[idxMin] = forward_tab[minIndex];
        modified++;
        errRef = errMin;
      }
    }
  }

  for (int k = 0; k < (size + 1); k++)
  {
    CHECK((filterCoeffInt[k] < -(1 << CCALF_DYNAMIC_RANGE)) || (filterCoeffInt[k] > (1 << CCALF_DYNAMIC_RANGE)), "Exceeded valid range for CC ALF coefficient");
    filterCoeff[filterIdx][k] = filterCoeffInt[k];
  }
}

void EncAdaptiveLoopFilter::determineControlIdcValues(CodingStructure &cs, const ComponentID compID, const PelBuf *buf,
                                                      const int ctuWidthC, const int ctuHeightC, const int picWidthC,
                                                      const int picHeightC, double **unfilteredDistortion,
                                                      uint64_t *trainingDistortion[MAX_NUM_CC_ALF_FILTERS],
                                                      uint64_t *lumaSwingGreaterThanThresholdCount,
                                                      uint64_t *chromaSampleCountNearMidPoint,
                                                      bool reuseTemporalFilterCoeff, uint8_t *trainingCovControl,
                                                      uint8_t *filterControl, uint64_t &curTotalDistortion,
                                                      double &curTotalRate, bool filterEnabled[MAX_NUM_CC_ALF_FILTERS],
                                                      uint8_t  mapFilterIdxToFilterIdc[MAX_NUM_CC_ALF_FILTERS + 1],
                                                      uint8_t &ccAlfFilterCount)
{
  bool curFilterEnabled[MAX_NUM_CC_ALF_FILTERS];
  std::fill_n(curFilterEnabled, MAX_NUM_CC_ALF_FILTERS, false);

#if MAX_NUM_CC_ALF_FILTERS>1
  FilterIdxCount filterIdxCount[MAX_NUM_CC_ALF_FILTERS];
  for (int i = 0; i < MAX_NUM_CC_ALF_FILTERS; i++)
  {
    filterIdxCount[i].count     = 0;
    filterIdxCount[i].filterIdx = i;
  }

  double prevRate = curTotalRate;
#endif

  TempCtx ctxInitial(m_ctxPool);
  TempCtx ctxBest(m_ctxPool);
  TempCtx ctxStart(m_ctxPool);
  ctxInitial = SubCtx(Ctx::CcAlfFilterControlFlag, m_CABACEstimator->getCtx());
  ctxBest    = SubCtx(Ctx::CcAlfFilterControlFlag, m_CABACEstimator->getCtx());

  int ctuIdx = 0;
  for (int yCtu = 0; yCtu < buf->height; yCtu += ctuHeightC)
  {
    for (int xCtu = 0; xCtu < buf->width; xCtu += ctuWidthC)
    {
      uint64_t ssd;
      double   rate;
      double   cost;

      uint64_t bestSSD       = MAX_UINT64;
      double   bestRate      = MAX_DOUBLE;
      double   bestCost      = MAX_DOUBLE;
      uint8_t  bestFilterIdc = 0;
      uint8_t  bestFilterIdx = 0;
      const uint32_t thresholdS = std::min<int>(buf->height - yCtu, ctuHeightC) << getComponentScaleY(COMPONENT_Cb, m_chromaFormat);
      const uint32_t numberOfChromaSamples = std::min<int>(buf->height - yCtu, ctuHeightC) * std::min<int>(buf->width - xCtu, ctuWidthC);
      const uint32_t thresholdC = (numberOfChromaSamples >> 2);

      m_CABACEstimator->getCtx() = ctxBest;
      ctxStart                   = SubCtx(Ctx::CcAlfFilterControlFlag, m_CABACEstimator->getCtx());

      for (int filterIdx = 0; filterIdx <= MAX_NUM_CC_ALF_FILTERS; filterIdx++)
      {
        uint8_t filterIdc = mapFilterIdxToFilterIdc[filterIdx];
        if (filterIdx < MAX_NUM_CC_ALF_FILTERS && !filterEnabled[filterIdx])
        {
          continue;
        }

        if (filterIdx == MAX_NUM_CC_ALF_FILTERS)
        {
          ssd = (uint64_t)unfilteredDistortion[compID][ctuIdx];   // restore saved distortion computation
        }
        else
        {
          ssd = trainingDistortion[filterIdx][ctuIdx];
        }
        m_CABACEstimator->getCtx() = ctxStart;
        m_CABACEstimator->resetBits();
        const Position lumaPos = Position({ xCtu << getComponentScaleX(compID, cs.pcv->chrFormat),
          yCtu << getComponentScaleY(compID, cs.pcv->chrFormat) });
        m_CABACEstimator->codeCcAlfFilterControlIdc(filterIdc, cs, compID, ctuIdx, filterControl, lumaPos,
                                                    ccAlfFilterCount);
        rate = FRAC_BITS_SCALE * m_CABACEstimator->getEstFracBits();
        cost = rate * m_lambda[compID] + ssd;

        bool limitationExceeded = false;
        if (m_limitCcAlf && filterIdx < MAX_NUM_CC_ALF_FILTERS)
        {
          limitationExceeded = limitationExceeded || (lumaSwingGreaterThanThresholdCount[ctuIdx] >= thresholdS);
          limitationExceeded = limitationExceeded || (chromaSampleCountNearMidPoint[ctuIdx] >= thresholdC);
        }
        if (cost < bestCost && !limitationExceeded)
        {
          bestCost      = cost;
          bestRate      = rate;
          bestSSD       = ssd;
          bestFilterIdc = filterIdc;
          bestFilterIdx = filterIdx;

          ctxBest = SubCtx(Ctx::CcAlfFilterControlFlag, m_CABACEstimator->getCtx());

          trainingCovControl[ctuIdx] = (filterIdx == MAX_NUM_CC_ALF_FILTERS) ? 0 : (filterIdx + 1);
          filterControl[ctuIdx]      = (filterIdx == MAX_NUM_CC_ALF_FILTERS) ? 0 : (filterIdx + 1);
        }
      }
      if (bestFilterIdc != 0)
      {
        curFilterEnabled[bestFilterIdx] = true;
#if MAX_NUM_CC_ALF_FILTERS>1
        filterIdxCount[bestFilterIdx].count++;
#endif
      }
      curTotalRate += bestRate;
      curTotalDistortion += bestSSD;
      ctuIdx++;
    }
  }

#if MAX_NUM_CC_ALF_FILTERS>1
  if (!reuseTemporalFilterCoeff)
  {
    std::copy_n(curFilterEnabled, MAX_NUM_CC_ALF_FILTERS, filterEnabled);

    std::stable_sort(filterIdxCount, filterIdxCount + MAX_NUM_CC_ALF_FILTERS, compareCounts);

    int filterIdc = 1;
    ccAlfFilterCount = 0;
    for ( FilterIdxCount &s : filterIdxCount )
    {
      const int filterIdx = s.filterIdx;
      if (filterEnabled[filterIdx])
      {
        mapFilterIdxToFilterIdc[filterIdx] = filterIdc;
        filterIdc++;
        ccAlfFilterCount++;
      }
    }

    curTotalRate = prevRate;
    m_CABACEstimator->getCtx() = ctxInitial;
    m_CABACEstimator->resetBits();
    int ctuIdx = 0;
    for (int y = 0; y < buf->height; y += ctuHeightC)
    {
      for (int x = 0; x < buf->width; x += ctuWidthC)
      {
        const int filterIdxPlus1 = filterControl[ctuIdx];

        const Position lumaPos = Position(
                                          { x << getComponentScaleX(compID, cs.pcv->chrFormat), y << getComponentScaleY(compID, cs.pcv->chrFormat) });

        m_CABACEstimator->codeCcAlfFilterControlIdc(filterIdxPlus1 == 0 ? 0
                                                    : mapFilterIdxToFilterIdc[filterIdxPlus1 - 1],
                                                    cs, compID, ctuIdx, filterControl, lumaPos, ccAlfFilterCount);

        ctuIdx++;
      }
    }
    curTotalRate += FRAC_BITS_SCALE*m_CABACEstimator->getEstFracBits();
  }
#endif

  // restore for next iteration
  m_CABACEstimator->getCtx() = ctxInitial;
}

std::vector<int> EncAdaptiveLoopFilter::getAvailableCcAlfApsIds(CodingStructure& cs, ComponentID compID)
{
  APS** apss = cs.slice->getAlfAPSs();
  for (int i = m_encCfg->getALFAPSIDShift(); i < m_encCfg->getALFAPSIDShift() + m_encCfg->getMaxNumALFAPS(); i++)
  {
    apss[i] = m_apsMap->getPS(i);
  }

  std::vector<int> result;
  int              numApsIdsChecked = 0, curApsId = m_apsIdStart;
  if (curApsId < m_encCfg->getALFAPSIDShift() + m_encCfg->getMaxNumALFAPS())
  {
    while ((numApsIdsChecked < m_encCfg->getMaxNumALFAPS()) && !cs.slice->isIRAP()
           && (result.size() < m_encCfg->getMaxNumALFAPS()) && !cs.slice->getPendingRasInit())
    {
      APS* curAPS = apss[curApsId];
      if (curAPS && curAPS->getLayerId() == cs.slice->getPic()->layerId
          && curAPS->getTemporalId() <= cs.slice->getTLayer() && curAPS->getCcAlfAPSParam().newCcAlfFilter[compID - 1])
      {
        result.push_back(curApsId);
      }
      numApsIdsChecked++;
      curApsId++;
      if (curApsId >= m_encCfg->getALFAPSIDShift() + m_encCfg->getMaxNumALFAPS())
      {
        curApsId = m_encCfg->getALFAPSIDShift();
      }
    }
  }
  return result;
}

void EncAdaptiveLoopFilter::getFrameStatsCcalf(ComponentID compIdx, const int filterIdc)
{
  int ctuRsAddr = 0;

  // init Frame stats buffers
  for (int shape = 0; shape != m_filterShapesCcAlf[compIdx - 1].size(); shape++)
  {
    m_alfCovarianceFrameCcAlf[compIdx - 1][shape].reset();
  }

  for (int yPos = 0; yPos < m_picHeight; yPos += m_maxCUHeight)
  {
    for (int xPos = 0; xPos < m_picWidth; xPos += m_maxCUWidth)
    {
      if (m_trainingCovControl[ctuRsAddr] == filterIdc)
      {
        for (int shape = 0; shape != m_filterShapesCcAlf[compIdx - 1].size(); shape++)
        {
          m_alfCovarianceFrameCcAlf[compIdx - 1][shape] += m_alfCovarianceCcAlf[compIdx - 1][shape][ctuRsAddr];
        }
      }
      ctuRsAddr++;
    }
  }
}

void EncAdaptiveLoopFilter::deriveCcAlfFilter( CodingStructure& cs, ComponentID compID, const PelUnitBuf& orgYuv, const PelUnitBuf& tempDecYuvBuf, const PelUnitBuf& dstYuv )
{
  if (!cs.slice->getAlfEnabledFlag(COMPONENT_Y))
  {
    m_ccAlfFilterParam.ccAlfFilterEnabled[compID - 1] = false;
    return;
  }

  m_limitCcAlf = m_encCfg->getBaseQP() >= m_encCfg->getCCALFQpThreshold();
  if (m_limitCcAlf && cs.slice->getSliceQp() <= m_encCfg->getBaseQP() + 1)
  {
    m_ccAlfFilterParam.ccAlfFilterEnabled[compID - 1] = false;
    return;
  }

  uint8_t bestMapFilterIdxToFilterIdc[MAX_NUM_CC_ALF_FILTERS+1];
  const int scaleX               = getComponentScaleX(compID, cs.pcv->chrFormat);
  const int scaleY               = getComponentScaleY(compID, cs.pcv->chrFormat);
  const int ctuWidthC            = cs.pcv->maxCUWidth >> scaleX;
  const int ctuHeightC           = cs.pcv->maxCUHeight >> scaleY;
  const int picWidthC            = cs.pcv->lumaWidth >> scaleX;
  const int picHeightC           = cs.pcv->lumaHeight >> scaleY;
  const int maxTrainingIterCount = 15;

  if (m_limitCcAlf)
  {
    countLumaSwingGreaterThanThreshold(dstYuv.get(COMPONENT_Y).bufAt(0, 0), dstYuv.get(COMPONENT_Y).stride, dstYuv.get(COMPONENT_Y).height, dstYuv.get(COMPONENT_Y).width, cs.pcv->maxCUWidthLog2, cs.pcv->maxCUHeightLog2, m_lumaSwingGreaterThanThresholdCount, m_numCTUsInWidth);
  }
  if (m_limitCcAlf)
  {
    countChromaSampleValueNearMidPoint(dstYuv.get(compID).bufAt(0, 0), dstYuv.get(compID).stride, dstYuv.get(compID).height, dstYuv.get(compID).width, cs.pcv->maxCUWidthLog2 - scaleX, cs.pcv->maxCUHeightLog2 - scaleY, m_chromaSampleCountNearMidPoint, m_numCTUsInWidth);
  }

  for ( int filterIdx = 0; filterIdx <= MAX_NUM_CC_ALF_FILTERS; filterIdx++ )
  {
    if ( filterIdx < MAX_NUM_CC_ALF_FILTERS)
    {
      memset( m_bestFilterCoeffSet[filterIdx], 0, sizeof(m_bestFilterCoeffSet[filterIdx]) );
      bestMapFilterIdxToFilterIdc[filterIdx] = filterIdx + 1;
    }
    else
    {
      bestMapFilterIdxToFilterIdc[filterIdx] = 0;
    }
  }
  memset(m_bestFilterControl, 0, sizeof(uint8_t) * m_numCTUsInPic);
  int ccalfReuseApsId      = -1;
  m_reuseApsId[compID - 1] = -1;

  const TempCtx ctxStartCcAlfFilterControlFlag(m_ctxPool,
                                               SubCtx(Ctx::CcAlfFilterControlFlag, m_CABACEstimator->getCtx()));

  // compute cost of not filtering
  uint64_t unfilteredDistortion = 0;
  for (int ctbIdx = 0; ctbIdx < m_numCTUsInPic; ctbIdx++)
  {
    unfilteredDistortion += (uint64_t) m_alfCovarianceCcAlf[compID - 1][0][ctbIdx].pixAcc;
  }

  setLambdaFactor(cs, (int) (getAvailableCcAlfApsIds(cs, compID).size()), m_lumaNewAps);
  double bestUnfilteredTotalCost = 1 * m_lambda[compID] * m_lambdaFactor + unfilteredDistortion;   // 1 bit is for gating flag

  bool             ccAlfFilterIdxEnabled[MAX_NUM_CC_ALF_FILTERS];
  AlfCoeff         ccAlfFilterCoeff[MAX_NUM_CC_ALF_FILTERS][MAX_NUM_CC_ALF_CHROMA_COEFF];
  uint8_t          ccAlfFilterCount             = MAX_NUM_CC_ALF_FILTERS;
  double bestFilteredTotalCost        = MAX_DOUBLE;
  bool   bestreuseTemporalFilterCoeff = false;
  std::vector<int> apsIds             = getAvailableCcAlfApsIds(cs, compID);

  for (int testFilterIdx = 0; testFilterIdx < ( apsIds.size() + 1 ); testFilterIdx++ )
  {
    bool referencingExistingAps   = (testFilterIdx < apsIds.size()) ? true : false;
    int maxNumberOfFiltersBeingTested = MAX_NUM_CC_ALF_FILTERS - (testFilterIdx - static_cast<int>(apsIds.size()));

    if (maxNumberOfFiltersBeingTested < 0)
    {
      maxNumberOfFiltersBeingTested = 1;
    }

    {
      // Instead of rewriting the control buffer for every training iteration just keep a mapping from filterIdx to filterIdc
      uint8_t mapFilterIdxToFilterIdc[MAX_NUM_CC_ALF_FILTERS + 1];
      for (int filterIdx = 0; filterIdx <= MAX_NUM_CC_ALF_FILTERS; filterIdx++)
      {
        if (filterIdx == MAX_NUM_CC_ALF_FILTERS)
        {
          mapFilterIdxToFilterIdc[filterIdx] = 0;
        }
        else
        {
          mapFilterIdxToFilterIdc[filterIdx] = filterIdx + 1;
        }
      }

      // initialize filters
      for ( int filterIdx = 0; filterIdx < MAX_NUM_CC_ALF_FILTERS; filterIdx++ )
      {
        ccAlfFilterIdxEnabled[filterIdx] = false;
        memset(ccAlfFilterCoeff[filterIdx], 0, sizeof(ccAlfFilterCoeff[filterIdx]));
      }
      if ( referencingExistingAps )
      {
        maxNumberOfFiltersBeingTested =
          m_apsMap->getPS(apsIds[testFilterIdx])->getCcAlfAPSParam().ccAlfFilterCount[compID - 1];
        ccAlfFilterCount = maxNumberOfFiltersBeingTested;
        for (int filterIdx = 0; filterIdx < maxNumberOfFiltersBeingTested; filterIdx++)
        {
          ccAlfFilterIdxEnabled[filterIdx] = true;
          memcpy(ccAlfFilterCoeff[filterIdx], m_ccAlfFilterParam.ccAlfCoeff[compID - 1][filterIdx],
                 sizeof(ccAlfFilterCoeff[filterIdx]));
        }
        memcpy(ccAlfFilterCoeff, m_apsMap->getPS(apsIds[testFilterIdx])->getCcAlfAPSParam().ccAlfCoeff[compID - 1],
               sizeof(ccAlfFilterCoeff));
      }
      else
      {
        for (int i = 0; i < maxNumberOfFiltersBeingTested; i++)
        {
          ccAlfFilterIdxEnabled[i] = true;
        }
        ccAlfFilterCount = maxNumberOfFiltersBeingTested;
      }

      // initialize
      int controlIdx = 0;
      const int columnSize = ( m_buf->width / maxNumberOfFiltersBeingTested);
      for (int y = 0; y < m_buf->height; y += ctuHeightC)
      {
        for (int x = 0; x < m_buf->width; x += ctuWidthC)
        {
          m_trainingCovControl[controlIdx] = ( x / columnSize ) + 1;
          controlIdx++;
        }
      }

      // compute cost of filtering
      int    trainingIterCount = 0;
      bool   keepTraining      = true;
      bool   improvement       = false;
      double prevTotalCost     = MAX_DOUBLE;
      while (keepTraining)
      {
        improvement = false;
        for (int filterIdx = 0; filterIdx < maxNumberOfFiltersBeingTested; filterIdx++)
        {
          if (ccAlfFilterIdxEnabled[filterIdx])
          {
            if (!referencingExistingAps)
            {
              getFrameStatsCcalf(compID, filterIdx + 1);
              deriveCcAlfFilterCoeff(compID, dstYuv, tempDecYuvBuf, ccAlfFilterCoeff, filterIdx);
            }
            const int numCoeff  = m_filterShapesCcAlf[compID - 1][0].numCoeff - 1;
            int log2BlockWidth  = cs.pcv->maxCUWidthLog2 - scaleX;
            int log2BlockHeight = cs.pcv->maxCUHeightLog2 - scaleY;
            for (int y = 0; y < m_buf->height; y += (1 << log2BlockHeight))
            {
              for (int x = 0; x < m_buf->width; x += (1 << log2BlockWidth))
              {
                int ctuIdx = (y >> log2BlockHeight) * m_numCTUsInWidth + (x >> log2BlockWidth);
                m_trainingDistortion[filterIdx][ctuIdx] =
                  int(m_ctbDistortionUnfilter[compID][ctuIdx]
                      + m_alfCovarianceCcAlf[compID - 1][0][ctuIdx].calcErrorForCcAlfCoeffs(
                        ccAlfFilterCoeff[filterIdx], numCoeff, COEFF_SCALE_BITS + 1));
              }
            }
          }
        }

        m_CABACEstimator->getCtx() = ctxStartCcAlfFilterControlFlag;

        uint64_t curTotalDistortion = 0;
        double curTotalRate = 0;
        determineControlIdcValues(cs, compID, m_buf, ctuWidthC, ctuHeightC, picWidthC, picHeightC,
                                  m_ctbDistortionUnfilter, m_trainingDistortion,
                                  m_lumaSwingGreaterThanThresholdCount,
                                  m_chromaSampleCountNearMidPoint,
                                  (referencingExistingAps == true),
                                  m_trainingCovControl, m_filterControl, curTotalDistortion, curTotalRate,
                                  ccAlfFilterIdxEnabled, mapFilterIdxToFilterIdc, ccAlfFilterCount);

        // compute coefficient coding bit cost
        if (ccAlfFilterCount > 0)
        {
          if (referencingExistingAps)
          {
            curTotalRate += 1 + 3; // +1 for enable flag, +3 APS ID in slice header
          }
          else
          {
            curTotalRate += getCoeffRateCcAlf(ccAlfFilterCoeff, ccAlfFilterIdxEnabled, ccAlfFilterCount, compID) + 1
            + 9;   // +1 for the enable flag, +9 3-bit for APS ID in slice header, 5-bit for APS ID in APS, a 1-bit
            // new filter flags (ignore shared cost such as other new-filter flags/NALU header/RBSP
            // terminating bit/byte alignment bits)
          }

          double curTotalCost = curTotalRate * m_lambda[compID] * m_lambdaFactor + curTotalDistortion;

          if (curTotalCost < prevTotalCost)
          {
            prevTotalCost = curTotalCost;
            improvement = true;
          }

          if (curTotalCost < bestFilteredTotalCost)
          {
            bestFilteredTotalCost = curTotalCost;
            memcpy(m_bestFilterIdxEnabled, ccAlfFilterIdxEnabled, sizeof(ccAlfFilterIdxEnabled));
            memcpy(m_bestFilterCoeffSet, ccAlfFilterCoeff, sizeof(ccAlfFilterCoeff));
            memcpy(m_bestFilterControl, m_filterControl, sizeof(uint8_t) * m_numCTUsInPic);
            m_bestFilterCount = ccAlfFilterCount;
            ccalfReuseApsId = referencingExistingAps ? apsIds[testFilterIdx] : -1;
            memcpy(bestMapFilterIdxToFilterIdc, mapFilterIdxToFilterIdc, sizeof(mapFilterIdxToFilterIdc));
          }
        }

        trainingIterCount++;
        if (!improvement || trainingIterCount > maxTrainingIterCount || referencingExistingAps)
        {
          keepTraining = false;
        }
      }
    }
  }

  if (bestUnfilteredTotalCost < bestFilteredTotalCost)
  {
    memset(m_bestFilterControl, 0, sizeof(uint8_t) * m_numCTUsInPic);
  }

  // save best coeff and control
  bool atleastOneBlockUndergoesFitlering = false;
  for (int controlIdx = 0; m_bestFilterCount > 0 && controlIdx < m_numCTUsInPic; controlIdx++)
  {
    if (m_bestFilterControl[controlIdx])
    {
      atleastOneBlockUndergoesFitlering = true;
      break;
    }
  }
  m_ccAlfFilterParam.numberValidComponents          = getNumberValidComponents(m_chromaFormat);
  m_ccAlfFilterParam.ccAlfFilterEnabled[compID - 1] = atleastOneBlockUndergoesFitlering;
  if (atleastOneBlockUndergoesFitlering)
  {
    // update the filter control indicators
    if (bestreuseTemporalFilterCoeff!=1)
    {
      AlfCoeff storedBestFilterCoeffSet[MAX_NUM_CC_ALF_FILTERS][MAX_NUM_CC_ALF_CHROMA_COEFF];
      for (int filterIdx=0; filterIdx<MAX_NUM_CC_ALF_FILTERS; filterIdx++)
      {
        memcpy(storedBestFilterCoeffSet[filterIdx], m_bestFilterCoeffSet[filterIdx], sizeof(m_bestFilterCoeffSet[filterIdx]));
      }
      memcpy(m_filterControl, m_bestFilterControl, sizeof(uint8_t) * m_numCTUsInPic);

      int filterCount = 0;
      for ( int filterIdx = 0; filterIdx < MAX_NUM_CC_ALF_FILTERS; filterIdx++ )
      {
        uint8_t curFilterIdc = bestMapFilterIdxToFilterIdc[filterIdx];
        if (m_bestFilterIdxEnabled[filterIdx])
        {
          for (int controlIdx = 0; controlIdx < m_numCTUsInPic; controlIdx++)
          {
            if (m_filterControl[controlIdx] == (filterIdx+1) )
            {
              m_bestFilterControl[controlIdx] = curFilterIdc;
            }
          }
          memcpy( m_bestFilterCoeffSet[curFilterIdc-1], storedBestFilterCoeffSet[filterIdx], sizeof(storedBestFilterCoeffSet[filterIdx]) );
          filterCount++;
        }
        m_bestFilterIdxEnabled[filterIdx] = ( filterIdx < m_bestFilterCount ) ? true : false;
      }
      CHECK( filterCount != m_bestFilterCount, "Number of filters enabled did not match the filter count");
    }

    m_ccAlfFilterParam.ccAlfFilterCount[compID - 1] = m_bestFilterCount;
    // cleanup before copying
    memset(m_ccAlfFilterControl[compID - 1], 0, sizeof(uint8_t) * m_numCTUsInPic);
    for ( int filterIdx = 0; filterIdx < MAX_NUM_CC_ALF_FILTERS; filterIdx++ )
    {
      memset(m_ccAlfFilterParam.ccAlfCoeff[compID - 1][filterIdx], 0,
             sizeof(m_ccAlfFilterParam.ccAlfCoeff[compID - 1][filterIdx]));
    }
    memset(m_ccAlfFilterParam.ccAlfFilterIdxEnabled[compID - 1], false,
           sizeof(m_ccAlfFilterParam.ccAlfFilterIdxEnabled[compID - 1]));
    for ( int filterIdx = 0; filterIdx < m_bestFilterCount; filterIdx++ )
    {
      m_ccAlfFilterParam.ccAlfFilterIdxEnabled[compID - 1][filterIdx] = m_bestFilterIdxEnabled[filterIdx];
      memcpy(m_ccAlfFilterParam.ccAlfCoeff[compID - 1][filterIdx], m_bestFilterCoeffSet[filterIdx],
             sizeof(m_bestFilterCoeffSet[filterIdx]));
    }
    memcpy(m_ccAlfFilterControl[compID - 1], m_bestFilterControl, sizeof(uint8_t) * m_numCTUsInPic);
    if ( ccalfReuseApsId >= 0 )
    {
      m_reuseApsId[compID - 1] = ccalfReuseApsId;
      if (compID == COMPONENT_Cb)
      {
        cs.slice->setCcAlfCbApsId(ccalfReuseApsId);
      }
      else
      {
        cs.slice->setCcAlfCrApsId(ccalfReuseApsId);
      }
    }
  }
}

void EncAdaptiveLoopFilter::deriveStatsForCcAlfFiltering(const PelUnitBuf& orgYuv, const PelUnitBuf& recYuv,
                                                         const int compIdx, const int maskStride, CodingStructure& cs)
{
  // init CTU stats buffers
  for( int shape = 0; shape != m_filterShapesCcAlf[compIdx-1].size(); shape++ )
  {
    for (int ctuIdx = 0; ctuIdx < m_numCTUsInPic; ctuIdx++)
    {
      m_alfCovarianceCcAlf[compIdx - 1][shape][ctuIdx].reset();
    }
  }

  // init Frame stats buffers
  for (int shape = 0; shape != m_filterShapesCcAlf[compIdx - 1].size(); shape++)
  {
    m_alfCovarianceFrameCcAlf[compIdx - 1][shape].reset();
  }

  int                  ctuRsAddr = 0;
  const PreCalcValues &pcv       = *cs.pcv;
  bool                 clipTop = false, clipBottom = false, clipLeft = false, clipRight = false;
  int                  numHorVirBndry = 0, numVerVirBndry = 0;
  int                  horVirBndryPos[] = { 0, 0, 0 };
  int                  verVirBndryPos[] = { 0, 0, 0 };

  for (int yPos = 0; yPos < m_picHeight; yPos += m_maxCUHeight)
  {
    for (int xPos = 0; xPos < m_picWidth; xPos += m_maxCUWidth)
    {
      const int width             = (xPos + m_maxCUWidth > m_picWidth) ? (m_picWidth - xPos) : m_maxCUWidth;
      const int height            = (yPos + m_maxCUHeight > m_picHeight) ? (m_picHeight - yPos) : m_maxCUHeight;
      int       rasterSliceAlfPad = 0;
      if (isCrossedByVirtualBoundaries(cs, xPos, yPos, width, height, clipTop, clipBottom, clipLeft, clipRight,
                                       numHorVirBndry, numVerVirBndry, horVirBndryPos, verVirBndryPos,
                                       rasterSliceAlfPad))
      {
        int yStart = yPos;
        for (int i = 0; i <= numHorVirBndry; i++)
        {
          const int  yEnd   = i == numHorVirBndry ? yPos + height : horVirBndryPos[i];
          const int  h      = yEnd - yStart;
          const bool clipT  = (i == 0 && clipTop) || (i > 0) || (yStart == 0);
          const bool clipB  = (i == numHorVirBndry && clipBottom) || (i < numHorVirBndry) || (yEnd == pcv.lumaHeight);
          int        xStart = xPos;
          for (int j = 0; j <= numVerVirBndry; j++)
          {
            const int  xEnd   = j == numVerVirBndry ? xPos + width : verVirBndryPos[j];
            const int  w      = xEnd - xStart;
            const bool clipL  = (j == 0 && clipLeft) || (j > 0) || (xStart == 0);
            const bool clipR  = (j == numVerVirBndry && clipRight) || (j < numVerVirBndry) || (xEnd == pcv.lumaWidth);
            const int  wBuf   = w + (clipL ? 0 : MAX_ALF_PADDING_SIZE) + (clipR ? 0 : MAX_ALF_PADDING_SIZE);
            const int  hBuf   = h + (clipT ? 0 : MAX_ALF_PADDING_SIZE) + (clipB ? 0 : MAX_ALF_PADDING_SIZE);
            PelUnitBuf recBuf = m_tempBuf2.subBuf(UnitArea(cs.area.chromaFormat, Area(0, 0, wBuf, hBuf)));
            recBuf.copyFrom(recYuv.subBuf(
              UnitArea(cs.area.chromaFormat, Area(xStart - (clipL ? 0 : MAX_ALF_PADDING_SIZE),
                                                  yStart - (clipT ? 0 : MAX_ALF_PADDING_SIZE), wBuf, hBuf))));
            // pad top-left unavailable samples for raster slice
            if (xStart == xPos && yStart == yPos && (rasterSliceAlfPad & 1))
            {
              recBuf.padBorderPel(MAX_ALF_PADDING_SIZE, 1);
            }

            // pad bottom-right unavailable samples for raster slice
            if (xEnd == xPos + width && yEnd == yPos + height && (rasterSliceAlfPad & 2))
            {
              recBuf.padBorderPel(MAX_ALF_PADDING_SIZE, 2);
            }
            recBuf.extendBorderPel(MAX_ALF_PADDING_SIZE);
            recBuf = recBuf.subBuf(UnitArea(
              cs.area.chromaFormat, Area(clipL ? 0 : MAX_ALF_PADDING_SIZE, clipT ? 0 : MAX_ALF_PADDING_SIZE, w, h)));

            const UnitArea area(m_chromaFormat, Area(0, 0, w, h));
            const UnitArea areaDst(m_chromaFormat, Area(xStart, yStart, w, h));

            const ComponentID compID = ComponentID(compIdx);

            for (int shape = 0; shape != m_filterShapesCcAlf[compIdx - 1].size(); shape++)
            {
              getBlkStatsCcAlf(m_alfCovarianceCcAlf[compIdx - 1][0][ctuRsAddr], m_filterShapesCcAlf[compIdx - 1][shape],
                               orgYuv, recBuf, areaDst, area, compID, yPos);
              m_alfCovarianceFrameCcAlf[compIdx - 1][shape] += m_alfCovarianceCcAlf[compIdx - 1][shape][ctuRsAddr];
            }

            xStart = xEnd;
          }

          yStart = yEnd;
        }
      }
      else
      {
        const UnitArea area(m_chromaFormat, Area(xPos, yPos, width, height));

        const ComponentID compID = ComponentID(compIdx);

        for (int shape = 0; shape != m_filterShapesCcAlf[compIdx - 1].size(); shape++)
        {
          getBlkStatsCcAlf(m_alfCovarianceCcAlf[compIdx - 1][0][ctuRsAddr], m_filterShapesCcAlf[compIdx - 1][shape],
                           orgYuv, recYuv, area, area, compID, yPos);
          m_alfCovarianceFrameCcAlf[compIdx - 1][shape] += m_alfCovarianceCcAlf[compIdx - 1][shape][ctuRsAddr];
        }
      }
      ctuRsAddr++;
    }
  }
}

void EncAdaptiveLoopFilter::getBlkStatsCcAlf(AlfCovariance &alfCovariance, const AlfFilterShape &shape,
                                             const PelUnitBuf &orgYuv, const PelUnitBuf &recYuv,
                                             const UnitArea &areaDst, const UnitArea &area, const ComponentID compID,
                                             const int yPos)
{
  const int numberOfComponents = getNumberValidComponents( m_chromaFormat );
  const CompArea &compArea           = areaDst.block(compID);
  ptrdiff_t       recStride[MAX_NUM_COMPONENT];
  const Pel* rec[MAX_NUM_COMPONENT];
  for ( int cIdx = 0; cIdx < numberOfComponents; cIdx++ )
  {
    recStride[cIdx] = recYuv.get(ComponentID(cIdx)).stride;
    rec[cIdx] = recYuv.get(ComponentID(cIdx)).bufAt(isLuma(ComponentID(cIdx)) ? area.lumaPos() : area.chromaPos());
  }

  ptrdiff_t  orgStride = orgYuv.get(compID).stride;
  const Pel *org       = orgYuv.get(compID).bufAt(compArea);

  const ptrdiff_t orgLumaStride = orgYuv.get(COMPONENT_Y).stride;
  const Pel      *orgLuma       = orgYuv.get(COMPONENT_Y).bufAt(areaDst.block(COMPONENT_Y));

  int vbCTUHeight = m_alfVBLumaCTUHeight;
  int vbPos       = m_alfVBLumaPos;
  if ((yPos + m_maxCUHeight) >= m_picHeight)
  {
    vbPos = m_picHeight;
  }

  Pel ELocal[MAX_NUM_CC_ALF_CHROMA_COEFF][1];

  const double strength    = m_encCfg->getCCALFStrengthTarget();
  const double invStrength = strength != 0.0 ? 1.0 / strength : 0.0;

  for (int i = 0; i < compArea.height; i++)
  {
    const int iY = i << getComponentScaleY(compID, m_chromaFormat);

    const int  vbDistance  = (iY % vbCTUHeight) - vbPos;
    const bool skipThisRow = getComponentScaleY(compID, m_chromaFormat) == 0 && (vbDistance == 0 || vbDistance == 1);
    for (int j = 0; j < compArea.width && (!skipThisRow); j++)
    {
      const int jY = j << getComponentScaleX(compID, m_chromaFormat);

      std::memset(ELocal, 0, sizeof(ELocal));

      calcCovarianceCcAlf(ELocal, rec[COMPONENT_Y] + jY, recStride[COMPONENT_Y], shape, vbDistance);

      const Pel *lumaPtr = orgLuma + iY * orgLumaStride + jY;

      const double weight = m_alfWSSD ? m_lumaLevelToWeightPLUT[*lumaPtr] : 1.0;
      const double yLocal = org[j] - rec[compID][j];

      double e[MAX_NUM_CC_ALF_CHROMA_COEFF];

      for (int k = 0; k < shape.numCoeff - 1; k++)
      {
        e[k] = invStrength * ELocal[k][0];
      }

      for( int k = 0; k < (shape.numCoeff - 1); k++ )
      {
        const double we = weight * e[k];

        for (int l = 0; l <= k; l++)
        {
          const ptrdiff_t oe = alfCovariance.getOffsetEfast(0, 0, k, l);
          alfCovariance.data[oe] += we * e[l];
        }
        alfCovariance.y(0, k) += we * yLocal;
      }
      alfCovariance.pixAcc += weight * yLocal * yLocal;
    }
    org += orgStride;
    for (int srcCIdx = 0; srcCIdx < numberOfComponents; srcCIdx++)
    {
      ComponentID srcCompID = ComponentID(srcCIdx);
      if (toChannelType(srcCompID) == toChannelType(compID))
      {
        rec[srcCIdx] += recStride[srcCIdx];
      }
      else
      {
        if (isLuma(compID))
        {
          rec[srcCIdx] += (recStride[srcCIdx] >> getComponentScaleY(srcCompID, m_chromaFormat));
        }
        else
        {
          rec[srcCIdx] += (recStride[srcCIdx] << getComponentScaleY(compID, m_chromaFormat));
        }
      }
    }
  }
}

void EncAdaptiveLoopFilter::calcCovarianceCcAlf(Pel ELocal[MAX_NUM_CC_ALF_CHROMA_COEFF][1], const Pel *rec,
                                                const ptrdiff_t stride, const AlfFilterShape &shape, int vbDistance)
{
  CHECK(shape.filterType != CC_ALF, "Bad CC ALF shape");

  const Pel *recYM1 = rec - 1 * stride;
  const Pel *recY0  = rec;
  const Pel *recYP1 = rec + 1 * stride;
  const Pel *recYP2 = rec + 2 * stride;

  if (vbDistance == -2 || vbDistance == +1)
  {
    recYP2 = recYP1;
  }
  else if (vbDistance == -1 || vbDistance == 0)
  {
    recYM1 = recY0;
    recYP2 = recYP1 = recY0;
  }

  for (int b = 0; b < 1; b++)
  {
    const Pel centerValue = recY0[+0];
    ELocal[0][b] += recYM1[+0] - centerValue;
    ELocal[1][b] += recY0[-1] - centerValue;
    ELocal[2][b] += recY0[+1] - centerValue;
    ELocal[3][b] += recYP1[-1] - centerValue;
    ELocal[4][b] += recYP1[+0] - centerValue;
    ELocal[5][b] += recYP1[+1] - centerValue;
    ELocal[6][b] += recYP2[+0] - centerValue;
  }
}

void EncAdaptiveLoopFilter::countLumaSwingGreaterThanThreshold(const Pel *luma, ptrdiff_t lumaStride, int height,
                                                               int width, int log2BlockWidth, int log2BlockHeight,
                                                               uint64_t *lumaSwingGreaterThanThresholdCount,
                                                               int       lumaCountStride)
{
  const int lumaBitDepth = m_inputBitDepth[ChannelType::LUMA];
  const int threshold    = (1 << (m_inputBitDepth[ChannelType::LUMA] - 2)) - 1;

  // 3x4 Diamond
  int xSupport[] = {  0, -1, 0, 1, -1, 0, 1, 0 };
  int ySupport[] = { -1,  0, 0, 0,  1, 1, 1, 2 };

  for (int y = 0; y < height; y += (1 << log2BlockHeight))
  {
    for (int x = 0; x < width; x += (1 << log2BlockWidth))
    {
      lumaSwingGreaterThanThresholdCount[(y >> log2BlockHeight) * lumaCountStride + (x >> log2BlockWidth)] = 0;

      for (int yOff = 0; yOff < (1 << log2BlockHeight); yOff++)
      {
        for (int xOff = 0; xOff < (1 << log2BlockWidth); xOff++)
        {
          if ((y + yOff) >= (height - 2) || (x + xOff) >= (width - 1) || (y + yOff) < 1 || (x + xOff) < 1) // only consider samples that are fully supported by picture
          {
            continue;
          }

          int minVal = ((1 << lumaBitDepth) - 1);
          int maxVal = 0;
          for (int i = 0; i < 8; i++)
          {
            Pel p = luma[(yOff + ySupport[i]) * lumaStride + x + xOff + xSupport[i]];

            if ( p < minVal )
            {
              minVal = p;
            }
            if ( p > maxVal )
            {
              maxVal = p;
            }
          }

          if ((maxVal - minVal) > threshold)
          {
            lumaSwingGreaterThanThresholdCount[(y >> log2BlockHeight) * lumaCountStride + (x >> log2BlockWidth)]++;
          }
        }
      }
    }
    luma += (lumaStride << log2BlockHeight);
  }
}

void EncAdaptiveLoopFilter::countChromaSampleValueNearMidPoint(const Pel *chroma, ptrdiff_t chromaStride, int height,
                                                               int width, int log2BlockWidth, int log2BlockHeight,
                                                               uint64_t *chromaSampleCountNearMidPoint,
                                                               int       chromaSampleCountNearMidPointStride)
{
  const int midPoint  = (1 << m_inputBitDepth[ChannelType::CHROMA]) >> 1;
  const int threshold = 16;

  for (int y = 0; y < height; y += (1 << log2BlockHeight))
  {
    for (int x = 0; x < width; x += (1 << log2BlockWidth))
    {
      chromaSampleCountNearMidPoint[(y >> log2BlockHeight)* chromaSampleCountNearMidPointStride + (x >> log2BlockWidth)] = 0;

      for (int yOff = 0; yOff < (1 << log2BlockHeight); yOff++)
      {
        for (int xOff = 0; xOff < (1 << log2BlockWidth); xOff++)
        {
          if ((y + yOff) >= height || (x + xOff) >= width)
          {
            continue;
          }

          int distanceToMidPoint = abs(chroma[yOff * chromaStride + x + xOff] - midPoint);
          if (distanceToMidPoint < threshold)
          {
            chromaSampleCountNearMidPoint[(y >> log2BlockHeight)* chromaSampleCountNearMidPointStride + (x >> log2BlockWidth)]++;
          }
        }
      }
    }
    chroma += (chromaStride << log2BlockHeight);
  }
}
