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

/** \file     TComTrQuant.cpp
    \brief    transform and quantization class
*/

#include <stdlib.h>
#include <math.h>
#include <limits>
#include <memory.h>
#include "TComTrQuant.h"
#include "TComPic.h"
#include "ContextTables.h"
#include "TComTU.h"
#include "Debug.h"

typedef struct
{
  Int    iNNZbeforePos0;
  Double d64CodedLevelandDist; // distortion and level cost only
  Double d64UncodedDist;    // all zero coded block distortion
  Double d64SigCost;
  Double d64SigCost_0;
} coeffGroupRDStats;

//! \ingroup TLibCommon
//! \{

// ====================================================================================================================
// Constants
// ====================================================================================================================

#define RDOQ_CHROMA                 1           ///< use of RDOQ in chroma


// ====================================================================================================================
// QpParam constructor
// ====================================================================================================================

QpParam::QpParam(const Int           qpy,
                 const ChannelType   chType,
                 const Int           qpBdOffset,
                 const Int           chromaQPOffset,
                 const ChromaFormat  chFmt )
{
  Int baseQp;

  if(isLuma(chType))
  {
    baseQp = qpy + qpBdOffset;
  }
  else
  {
    baseQp = Clip3( -qpBdOffset, (chromaQPMappingTableSize - 1), qpy + chromaQPOffset );

    if(baseQp < 0)
    {
      baseQp = baseQp + qpBdOffset;
    }
    else
    {
      baseQp = getScaledChromaQP(baseQp, chFmt) + qpBdOffset;
    }
  }

  Qp =baseQp;
  per=baseQp/6;
  rem=baseQp%6;
}

QpParam::QpParam(const TComDataCU &cu, const ComponentID compID)
{
  Int chromaQpOffset = 0;

  if (isChroma(compID))
  {
    chromaQpOffset += cu.getSlice()->getPPS()->getQpOffset(compID);
    chromaQpOffset += cu.getSlice()->getSliceChromaQpDelta(compID);

    chromaQpOffset += cu.getSlice()->getPPS()->getPpsRangeExtension().getChromaQpOffsetListEntry(cu.getChromaQpAdj(0)).u.offset[Int(compID)-1];
  }

  *this = QpParam(cu.getQP( 0 ),
                  toChannelType(compID),
                  cu.getSlice()->getSPS()->getQpBDOffset(toChannelType(compID)),
                  chromaQpOffset,
                  cu.getPic()->getChromaFormat());
}


// ====================================================================================================================
// TComTrQuant class member functions
// ====================================================================================================================

TComTrQuant::TComTrQuant()
{
  // allocate temporary buffers
  m_plTempCoeff  = new TCoeff[ MAX_CU_SIZE*MAX_CU_SIZE ];

  // allocate bit estimation class  (for RDOQ)
  m_pcEstBitsSbac = new estBitsSbacStruct;
  initScalingList();
}

TComTrQuant::~TComTrQuant()
{
  // delete temporary buffers
  if ( m_plTempCoeff )
  {
    delete [] m_plTempCoeff;
    m_plTempCoeff = NULL;
  }

  // delete bit estimation class
  if ( m_pcEstBitsSbac )
  {
    delete m_pcEstBitsSbac;
  }
  destroyScalingList();
}

#if ADAPTIVE_QP_SELECTION
Void TComTrQuant::storeSliceQpNext(TComSlice* pcSlice)
{
  // NOTE: does this work with negative QPs or when some blocks are transquant-bypass enabled?

  Int qpBase = pcSlice->getSliceQpBase();
  Int sliceQpused = pcSlice->getSliceQp();
  Int sliceQpnext;
  Double alpha = qpBase < 17 ? 0.5 : 1;

  Int cnt=0;
  for(Int u=1; u<=LEVEL_RANGE; u++)
  {
    cnt += m_sliceNsamples[u] ;
  }

  if( !m_useRDOQ )
  {
    sliceQpused = qpBase;
    alpha = 0.5;
  }

  if( cnt > 120 )
  {
    Double sum = 0;
    Int k = 0;
    for(Int u=1; u<LEVEL_RANGE; u++)
    {
      sum += u*m_sliceSumC[u];
      k += u*u*m_sliceNsamples[u];
    }

    Int v;
    Double q[MAX_QP+1] ;
    for(v=0; v<=MAX_QP; v++)
    {
      q[v] = (Double)(g_invQuantScales[v%6] * (1<<(v/6)))/64 ;
    }

    Double qnext = sum/k * q[sliceQpused] / (1<<ARL_C_PRECISION);

    for(v=0; v<MAX_QP; v++)
    {
      if(qnext < alpha * q[v] + (1 - alpha) * q[v+1] )
      {
        break;
      }
    }
    sliceQpnext = Clip3(sliceQpused - 3, sliceQpused + 3, v);
  }
  else
  {
    sliceQpnext = sliceQpused;
  }

  m_qpDelta[qpBase] = sliceQpnext - qpBase;
}

Void TComTrQuant::initSliceQpDelta()
{
  for(Int qp=0; qp<=MAX_QP; qp++)
  {
    m_qpDelta[qp] = qp < 17 ? 0 : 1;
  }
}

Void TComTrQuant::clearSliceARLCnt()
{
  memset(m_sliceSumC, 0, sizeof(Double)*(LEVEL_RANGE+1));
  memset(m_sliceNsamples, 0, sizeof(Int)*(LEVEL_RANGE+1));
}
#endif



#if MATRIX_MULT
/** NxN forward transform (2D) using brute force matrix multiplication (3 nested loops)
 *  \param block pointer to input data (residual)
 *  \param coeff pointer to output data (transform coefficients)
 *  \param uiStride stride of input data
 *  \param uiTrSize transform size (uiTrSize x uiTrSize)
 *  \param uiMode is Intra Prediction mode used in Mode-Dependent DCT/DST only
 */
Void xTr(Int bitDepth, Pel *block, TCoeff *coeff, UInt uiStride, UInt uiTrSize, Bool useDST, const Int maxLog2TrDynamicRange)
{
  UInt i,j,k;
  TCoeff iSum;
  TCoeff tmp[MAX_TU_SIZE * MAX_TU_SIZE];
  const TMatrixCoeff *iT;
  UInt uiLog2TrSize = g_aucConvertToBit[ uiTrSize ] + 2;

  if (uiTrSize==4)
  {
    iT  = (useDST ? g_as_DST_MAT_4[TRANSFORM_FORWARD][0] : g_aiT4[TRANSFORM_FORWARD][0]);
  }
  else if (uiTrSize==8)
  {
    iT = g_aiT8[TRANSFORM_FORWARD][0];
  }
  else if (uiTrSize==16)
  {
    iT = g_aiT16[TRANSFORM_FORWARD][0];
  }
  else if (uiTrSize==32)
  {
    iT = g_aiT32[TRANSFORM_FORWARD][0];
  }
  else
  {
    assert(0);
  }

  const Int TRANSFORM_MATRIX_SHIFT = g_transformMatrixShift[TRANSFORM_FORWARD];

  const Int shift_1st = (uiLog2TrSize +  bitDepth + TRANSFORM_MATRIX_SHIFT) - maxLog2TrDynamicRange;
  const Int shift_2nd = uiLog2TrSize + TRANSFORM_MATRIX_SHIFT;
  const Int add_1st = (shift_1st>0) ? (1<<(shift_1st-1)) : 0;
  const Int add_2nd = 1<<(shift_2nd-1);

  /* Horizontal transform */

  for (i=0; i<uiTrSize; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += iT[i*uiTrSize+k]*block[j*uiStride+k];
      }
      tmp[i*uiTrSize+j] = (iSum + add_1st)>>shift_1st;
    }
  }

  /* Vertical transform */
  for (i=0; i<uiTrSize; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += iT[i*uiTrSize+k]*tmp[j*uiTrSize+k];
      }
      coeff[i*uiTrSize+j] = (iSum + add_2nd)>>shift_2nd;
    }
  }
}

/** NxN inverse transform (2D) using brute force matrix multiplication (3 nested loops)
 *  \param coeff pointer to input data (transform coefficients)
 *  \param block pointer to output data (residual)
 *  \param uiStride stride of output data
 *  \param uiTrSize transform size (uiTrSize x uiTrSize)
 *  \param uiMode is Intra Prediction mode used in Mode-Dependent DCT/DST only
 */
Void xITr(Int bitDepth, TCoeff *coeff, Pel *block, UInt uiStride, UInt uiTrSize, Bool useDST, const Int maxLog2TrDynamicRange)
{
  UInt i,j,k;
  TCoeff iSum;
  TCoeff tmp[MAX_TU_SIZE * MAX_TU_SIZE];
  const TMatrixCoeff *iT;

  if (uiTrSize==4)
  {
    iT  = (useDST ? g_as_DST_MAT_4[TRANSFORM_INVERSE][0] : g_aiT4[TRANSFORM_INVERSE][0]);
  }
  else if (uiTrSize==8)
  {
    iT = g_aiT8[TRANSFORM_INVERSE][0];
  }
  else if (uiTrSize==16)
  {
    iT = g_aiT16[TRANSFORM_INVERSE][0];
  }
  else if (uiTrSize==32)
  {
    iT = g_aiT32[TRANSFORM_INVERSE][0];
  }
  else
  {
    assert(0);
  }

  const Int TRANSFORM_MATRIX_SHIFT = g_transformMatrixShift[TRANSFORM_INVERSE];

  const Int shift_1st = TRANSFORM_MATRIX_SHIFT + 1; //1 has been added to shift_1st at the expense of shift_2nd
  const Int shift_2nd = (TRANSFORM_MATRIX_SHIFT + maxLog2TrDynamicRange - 1) - bitDepth;
  const TCoeff clipMinimum = -(1 << maxLog2TrDynamicRange);
  const TCoeff clipMaximum =  (1 << maxLog2TrDynamicRange) - 1;
  assert(shift_2nd>=0);
  const Int add_1st = 1<<(shift_1st-1);
  const Int add_2nd = (shift_2nd>0) ? (1<<(shift_2nd-1)) : 0;

  /* Horizontal transform */
  for (i=0; i<uiTrSize; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += iT[k*uiTrSize+i]*coeff[k*uiTrSize+j];
      }

      // Clipping here is not in the standard, but is used to protect the "Pel" data type into which the inverse-transformed samples will be copied
      tmp[i*uiTrSize+j] = Clip3<TCoeff>(clipMinimum, clipMaximum, (iSum + add_1st)>>shift_1st);
    }
  }

  /* Vertical transform */
  for (i=0; i<uiTrSize; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += iT[k*uiTrSize+j]*tmp[i*uiTrSize+k];
      }

      block[i*uiStride+j] = Clip3<TCoeff>(std::numeric_limits<Pel>::min(), std::numeric_limits<Pel>::max(), (iSum + add_2nd)>>shift_2nd);
    }
  }
}

#endif //MATRIX_MULT


/** 4x4 forward transform implemented using partial butterfly structure (1D)
 *  \param src   input data (residual)
 *  \param dst   output data (transform coefficients)
 *  \param shift specifies right shift after 1D transform
 *  \param line
 */
Void partialButterfly4(TCoeff *src, TCoeff *dst, Int shift, Int line)
{
  Int j;
  TCoeff E[2],O[2];
  TCoeff add = (shift > 0) ? (1<<(shift-1)) : 0;

  for (j=0; j<line; j++)
  {
    /* E and O */
    E[0] = src[0] + src[3];
    O[0] = src[0] - src[3];
    E[1] = src[1] + src[2];
    O[1] = src[1] - src[2];

    dst[0]      = (g_aiT4[TRANSFORM_FORWARD][0][0]*E[0] + g_aiT4[TRANSFORM_FORWARD][0][1]*E[1] + add)>>shift;
    dst[2*line] = (g_aiT4[TRANSFORM_FORWARD][2][0]*E[0] + g_aiT4[TRANSFORM_FORWARD][2][1]*E[1] + add)>>shift;
    dst[line]   = (g_aiT4[TRANSFORM_FORWARD][1][0]*O[0] + g_aiT4[TRANSFORM_FORWARD][1][1]*O[1] + add)>>shift;
    dst[3*line] = (g_aiT4[TRANSFORM_FORWARD][3][0]*O[0] + g_aiT4[TRANSFORM_FORWARD][3][1]*O[1] + add)>>shift;

    src += 4;
    dst ++;
  }
}

// Fast DST Algorithm. Full matrix multiplication for DST and Fast DST algorithm
// give identical results
Void fastForwardDst(TCoeff *block, TCoeff *coeff, Int shift)  // input block, output coeff
{
  Int i;
  TCoeff c[4];
  TCoeff rnd_factor = (shift > 0) ? (1<<(shift-1)) : 0;
  for (i=0; i<4; i++)
  {
    // Intermediate Variables
    c[0] = block[4*i+0];
    c[1] = block[4*i+1];
    c[2] = block[4*i+2];
    c[3] = block[4*i+3];

    for (Int row = 0; row < 4; row++)
    {
      TCoeff result = 0;
      for (Int column = 0; column < 4; column++)
      {
        result += c[column] * g_as_DST_MAT_4[TRANSFORM_FORWARD][row][column]; // use the defined matrix, rather than hard-wired numbers
      }

      coeff[(row * 4) + i] = rightShift((result + rnd_factor), shift);
    }
  }
}

Void fastInverseDst(TCoeff *tmp, TCoeff *block, Int shift, const TCoeff outputMinimum, const TCoeff outputMaximum)  // input tmp, output block
{
  Int i;
  TCoeff c[4];
  TCoeff rnd_factor = (shift > 0) ? (1<<(shift-1)) : 0;
  for (i=0; i<4; i++)
  {
    // Intermediate Variables
    c[0] = tmp[   i];
    c[1] = tmp[4 +i];
    c[2] = tmp[8 +i];
    c[3] = tmp[12+i];

    for (Int column = 0; column < 4; column++)
    {
      TCoeff &result = block[(i * 4) + column];

      result = 0;
      for (Int row = 0; row < 4; row++)
      {
        result += c[row] * g_as_DST_MAT_4[TRANSFORM_INVERSE][row][column]; // use the defined matrix, rather than hard-wired numbers
      }

      result = Clip3( outputMinimum, outputMaximum, rightShift((result + rnd_factor), shift));
    }
  }
}

/** 4x4 inverse transform implemented using partial butterfly structure (1D)
 *  \param src   input data (transform coefficients)
 *  \param dst   output data (residual)
 *  \param shift specifies right shift after 1D transform
 *  \param line
 *  \param outputMinimum  minimum for clipping
 *  \param outputMaximum  maximum for clipping
 */
Void partialButterflyInverse4(TCoeff *src, TCoeff *dst, Int shift, Int line, const TCoeff outputMinimum, const TCoeff outputMaximum)
{
  Int j;
  TCoeff E[2],O[2];
  TCoeff add = (shift > 0) ? (1<<(shift-1)) : 0;

  for (j=0; j<line; j++)
  {
    /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
    O[0] = g_aiT4[TRANSFORM_INVERSE][1][0]*src[line] + g_aiT4[TRANSFORM_INVERSE][3][0]*src[3*line];
    O[1] = g_aiT4[TRANSFORM_INVERSE][1][1]*src[line] + g_aiT4[TRANSFORM_INVERSE][3][1]*src[3*line];
    E[0] = g_aiT4[TRANSFORM_INVERSE][0][0]*src[0]    + g_aiT4[TRANSFORM_INVERSE][2][0]*src[2*line];
    E[1] = g_aiT4[TRANSFORM_INVERSE][0][1]*src[0]    + g_aiT4[TRANSFORM_INVERSE][2][1]*src[2*line];

    /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
    dst[0] = Clip3( outputMinimum, outputMaximum, (E[0] + O[0] + add)>>shift );
    dst[1] = Clip3( outputMinimum, outputMaximum, (E[1] + O[1] + add)>>shift );
    dst[2] = Clip3( outputMinimum, outputMaximum, (E[1] - O[1] + add)>>shift );
    dst[3] = Clip3( outputMinimum, outputMaximum, (E[0] - O[0] + add)>>shift );

    src   ++;
    dst += 4;
  }
}

/** 8x8 forward transform implemented using partial butterfly structure (1D)
 *  \param src   input data (residual)
 *  \param dst   output data (transform coefficients)
 *  \param shift specifies right shift after 1D transform
 *  \param line
 */
Void partialButterfly8(TCoeff *src, TCoeff *dst, Int shift, Int line)
{
  Int j,k;
  TCoeff E[4],O[4];
  TCoeff EE[2],EO[2];
  TCoeff add = (shift > 0) ? (1<<(shift-1)) : 0;

  for (j=0; j<line; j++)
  {
    /* E and O*/
    for (k=0;k<4;k++)
    {
      E[k] = src[k] + src[7-k];
      O[k] = src[k] - src[7-k];
    }
    /* EE and EO */
    EE[0] = E[0] + E[3];
    EO[0] = E[0] - E[3];
    EE[1] = E[1] + E[2];
    EO[1] = E[1] - E[2];

    dst[0]      = (g_aiT8[TRANSFORM_FORWARD][0][0]*EE[0] + g_aiT8[TRANSFORM_FORWARD][0][1]*EE[1] + add)>>shift;
    dst[4*line] = (g_aiT8[TRANSFORM_FORWARD][4][0]*EE[0] + g_aiT8[TRANSFORM_FORWARD][4][1]*EE[1] + add)>>shift;
    dst[2*line] = (g_aiT8[TRANSFORM_FORWARD][2][0]*EO[0] + g_aiT8[TRANSFORM_FORWARD][2][1]*EO[1] + add)>>shift;
    dst[6*line] = (g_aiT8[TRANSFORM_FORWARD][6][0]*EO[0] + g_aiT8[TRANSFORM_FORWARD][6][1]*EO[1] + add)>>shift;

    dst[line]   = (g_aiT8[TRANSFORM_FORWARD][1][0]*O[0] + g_aiT8[TRANSFORM_FORWARD][1][1]*O[1] + g_aiT8[TRANSFORM_FORWARD][1][2]*O[2] + g_aiT8[TRANSFORM_FORWARD][1][3]*O[3] + add)>>shift;
    dst[3*line] = (g_aiT8[TRANSFORM_FORWARD][3][0]*O[0] + g_aiT8[TRANSFORM_FORWARD][3][1]*O[1] + g_aiT8[TRANSFORM_FORWARD][3][2]*O[2] + g_aiT8[TRANSFORM_FORWARD][3][3]*O[3] + add)>>shift;
    dst[5*line] = (g_aiT8[TRANSFORM_FORWARD][5][0]*O[0] + g_aiT8[TRANSFORM_FORWARD][5][1]*O[1] + g_aiT8[TRANSFORM_FORWARD][5][2]*O[2] + g_aiT8[TRANSFORM_FORWARD][5][3]*O[3] + add)>>shift;
    dst[7*line] = (g_aiT8[TRANSFORM_FORWARD][7][0]*O[0] + g_aiT8[TRANSFORM_FORWARD][7][1]*O[1] + g_aiT8[TRANSFORM_FORWARD][7][2]*O[2] + g_aiT8[TRANSFORM_FORWARD][7][3]*O[3] + add)>>shift;

    src += 8;
    dst ++;
  }
}

/** 8x8 inverse transform implemented using partial butterfly structure (1D)
 *  \param src   input data (transform coefficients)
 *  \param dst   output data (residual)
 *  \param shift specifies right shift after 1D transform
 *  \param line
 *  \param outputMinimum  minimum for clipping
 *  \param outputMaximum  maximum for clipping
 */
Void partialButterflyInverse8(TCoeff *src, TCoeff *dst, Int shift, Int line, const TCoeff outputMinimum, const TCoeff outputMaximum)
{
  Int j,k;
  TCoeff E[4],O[4];
  TCoeff EE[2],EO[2];
  TCoeff add = (shift > 0) ? (1<<(shift-1)) : 0;

  for (j=0; j<line; j++)
  {
    /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
    for (k=0;k<4;k++)
    {
      O[k] = g_aiT8[TRANSFORM_INVERSE][ 1][k]*src[line]   + g_aiT8[TRANSFORM_INVERSE][ 3][k]*src[3*line] +
             g_aiT8[TRANSFORM_INVERSE][ 5][k]*src[5*line] + g_aiT8[TRANSFORM_INVERSE][ 7][k]*src[7*line];
    }

    EO[0] = g_aiT8[TRANSFORM_INVERSE][2][0]*src[ 2*line ] + g_aiT8[TRANSFORM_INVERSE][6][0]*src[ 6*line ];
    EO[1] = g_aiT8[TRANSFORM_INVERSE][2][1]*src[ 2*line ] + g_aiT8[TRANSFORM_INVERSE][6][1]*src[ 6*line ];
    EE[0] = g_aiT8[TRANSFORM_INVERSE][0][0]*src[ 0      ] + g_aiT8[TRANSFORM_INVERSE][4][0]*src[ 4*line ];
    EE[1] = g_aiT8[TRANSFORM_INVERSE][0][1]*src[ 0      ] + g_aiT8[TRANSFORM_INVERSE][4][1]*src[ 4*line ];

    /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
    E[0] = EE[0] + EO[0];
    E[3] = EE[0] - EO[0];
    E[1] = EE[1] + EO[1];
    E[2] = EE[1] - EO[1];
    for (k=0;k<4;k++)
    {
      dst[ k   ] = Clip3( outputMinimum, outputMaximum, (E[k] + O[k] + add)>>shift );
      dst[ k+4 ] = Clip3( outputMinimum, outputMaximum, (E[3-k] - O[3-k] + add)>>shift );
    }
    src ++;
    dst += 8;
  }
}

/** 16x16 forward transform implemented using partial butterfly structure (1D)
 *  \param src   input data (residual)
 *  \param dst   output data (transform coefficients)
 *  \param shift specifies right shift after 1D transform
 *  \param line
 */
Void partialButterfly16(TCoeff *src, TCoeff *dst, Int shift, Int line)
{
  Int j,k;
  TCoeff E[8],O[8];
  TCoeff EE[4],EO[4];
  TCoeff EEE[2],EEO[2];
  TCoeff add = (shift > 0) ? (1<<(shift-1)) : 0;

  for (j=0; j<line; j++)
  {
    /* E and O*/
    for (k=0;k<8;k++)
    {
      E[k] = src[k] + src[15-k];
      O[k] = src[k] - src[15-k];
    }
    /* EE and EO */
    for (k=0;k<4;k++)
    {
      EE[k] = E[k] + E[7-k];
      EO[k] = E[k] - E[7-k];
    }
    /* EEE and EEO */
    EEE[0] = EE[0] + EE[3];
    EEO[0] = EE[0] - EE[3];
    EEE[1] = EE[1] + EE[2];
    EEO[1] = EE[1] - EE[2];

    dst[ 0      ] = (g_aiT16[TRANSFORM_FORWARD][ 0][0]*EEE[0] + g_aiT16[TRANSFORM_FORWARD][ 0][1]*EEE[1] + add)>>shift;
    dst[ 8*line ] = (g_aiT16[TRANSFORM_FORWARD][ 8][0]*EEE[0] + g_aiT16[TRANSFORM_FORWARD][ 8][1]*EEE[1] + add)>>shift;
    dst[ 4*line ] = (g_aiT16[TRANSFORM_FORWARD][ 4][0]*EEO[0] + g_aiT16[TRANSFORM_FORWARD][ 4][1]*EEO[1] + add)>>shift;
    dst[ 12*line] = (g_aiT16[TRANSFORM_FORWARD][12][0]*EEO[0] + g_aiT16[TRANSFORM_FORWARD][12][1]*EEO[1] + add)>>shift;

    for (k=2;k<16;k+=4)
    {
      dst[ k*line ] = (g_aiT16[TRANSFORM_FORWARD][k][0]*EO[0] + g_aiT16[TRANSFORM_FORWARD][k][1]*EO[1] +
                       g_aiT16[TRANSFORM_FORWARD][k][2]*EO[2] + g_aiT16[TRANSFORM_FORWARD][k][3]*EO[3] + add)>>shift;
    }

    for (k=1;k<16;k+=2)
    {
      dst[ k*line ] = (g_aiT16[TRANSFORM_FORWARD][k][0]*O[0] + g_aiT16[TRANSFORM_FORWARD][k][1]*O[1] +
                       g_aiT16[TRANSFORM_FORWARD][k][2]*O[2] + g_aiT16[TRANSFORM_FORWARD][k][3]*O[3] +
                       g_aiT16[TRANSFORM_FORWARD][k][4]*O[4] + g_aiT16[TRANSFORM_FORWARD][k][5]*O[5] +
                       g_aiT16[TRANSFORM_FORWARD][k][6]*O[6] + g_aiT16[TRANSFORM_FORWARD][k][7]*O[7] + add)>>shift;
    }

    src += 16;
    dst ++;

  }
}

/** 16x16 inverse transform implemented using partial butterfly structure (1D)
 *  \param src            input data (transform coefficients)
 *  \param dst            output data (residual)
 *  \param shift          specifies right shift after 1D transform
 *  \param line
 *  \param outputMinimum  minimum for clipping
 *  \param outputMaximum  maximum for clipping
 */
Void partialButterflyInverse16(TCoeff *src, TCoeff *dst, Int shift, Int line, const TCoeff outputMinimum, const TCoeff outputMaximum)
{
  Int j,k;
  TCoeff E[8],O[8];
  TCoeff EE[4],EO[4];
  TCoeff EEE[2],EEO[2];
  TCoeff add = (shift > 0) ? (1<<(shift-1)) : 0;

  for (j=0; j<line; j++)
  {
    /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
    for (k=0;k<8;k++)
    {
      O[k] = g_aiT16[TRANSFORM_INVERSE][ 1][k]*src[ line]   + g_aiT16[TRANSFORM_INVERSE][ 3][k]*src[ 3*line] +
             g_aiT16[TRANSFORM_INVERSE][ 5][k]*src[ 5*line] + g_aiT16[TRANSFORM_INVERSE][ 7][k]*src[ 7*line] +
             g_aiT16[TRANSFORM_INVERSE][ 9][k]*src[ 9*line] + g_aiT16[TRANSFORM_INVERSE][11][k]*src[11*line] +
             g_aiT16[TRANSFORM_INVERSE][13][k]*src[13*line] + g_aiT16[TRANSFORM_INVERSE][15][k]*src[15*line];
    }
    for (k=0;k<4;k++)
    {
      EO[k] = g_aiT16[TRANSFORM_INVERSE][ 2][k]*src[ 2*line] + g_aiT16[TRANSFORM_INVERSE][ 6][k]*src[ 6*line] +
              g_aiT16[TRANSFORM_INVERSE][10][k]*src[10*line] + g_aiT16[TRANSFORM_INVERSE][14][k]*src[14*line];
    }
    EEO[0] = g_aiT16[TRANSFORM_INVERSE][4][0]*src[ 4*line ] + g_aiT16[TRANSFORM_INVERSE][12][0]*src[ 12*line ];
    EEE[0] = g_aiT16[TRANSFORM_INVERSE][0][0]*src[ 0      ] + g_aiT16[TRANSFORM_INVERSE][ 8][0]*src[ 8*line  ];
    EEO[1] = g_aiT16[TRANSFORM_INVERSE][4][1]*src[ 4*line ] + g_aiT16[TRANSFORM_INVERSE][12][1]*src[ 12*line ];
    EEE[1] = g_aiT16[TRANSFORM_INVERSE][0][1]*src[ 0      ] + g_aiT16[TRANSFORM_INVERSE][ 8][1]*src[ 8*line  ];

    /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
    for (k=0;k<2;k++)
    {
      EE[k] = EEE[k] + EEO[k];
      EE[k+2] = EEE[1-k] - EEO[1-k];
    }
    for (k=0;k<4;k++)
    {
      E[k] = EE[k] + EO[k];
      E[k+4] = EE[3-k] - EO[3-k];
    }
    for (k=0;k<8;k++)
    {
      dst[k]   = Clip3( outputMinimum, outputMaximum, (E[k] + O[k] + add)>>shift );
      dst[k+8] = Clip3( outputMinimum, outputMaximum, (E[7-k] - O[7-k] + add)>>shift );
    }
    src ++;
    dst += 16;
  }
}

/** 32x32 forward transform implemented using partial butterfly structure (1D)
 *  \param src   input data (residual)
 *  \param dst   output data (transform coefficients)
 *  \param shift specifies right shift after 1D transform
 *  \param line
 */
Void partialButterfly32(TCoeff *src, TCoeff *dst, Int shift, Int line)
{
  Int j,k;
  TCoeff E[16],O[16];
  TCoeff EE[8],EO[8];
  TCoeff EEE[4],EEO[4];
  TCoeff EEEE[2],EEEO[2];
  TCoeff add = (shift > 0) ? (1<<(shift-1)) : 0;

  for (j=0; j<line; j++)
  {
    /* E and O*/
    for (k=0;k<16;k++)
    {
      E[k] = src[k] + src[31-k];
      O[k] = src[k] - src[31-k];
    }
    /* EE and EO */
    for (k=0;k<8;k++)
    {
      EE[k] = E[k] + E[15-k];
      EO[k] = E[k] - E[15-k];
    }
    /* EEE and EEO */
    for (k=0;k<4;k++)
    {
      EEE[k] = EE[k] + EE[7-k];
      EEO[k] = EE[k] - EE[7-k];
    }
    /* EEEE and EEEO */
    EEEE[0] = EEE[0] + EEE[3];
    EEEO[0] = EEE[0] - EEE[3];
    EEEE[1] = EEE[1] + EEE[2];
    EEEO[1] = EEE[1] - EEE[2];

    dst[ 0       ] = (g_aiT32[TRANSFORM_FORWARD][ 0][0]*EEEE[0] + g_aiT32[TRANSFORM_FORWARD][ 0][1]*EEEE[1] + add)>>shift;
    dst[ 16*line ] = (g_aiT32[TRANSFORM_FORWARD][16][0]*EEEE[0] + g_aiT32[TRANSFORM_FORWARD][16][1]*EEEE[1] + add)>>shift;
    dst[ 8*line  ] = (g_aiT32[TRANSFORM_FORWARD][ 8][0]*EEEO[0] + g_aiT32[TRANSFORM_FORWARD][ 8][1]*EEEO[1] + add)>>shift;
    dst[ 24*line ] = (g_aiT32[TRANSFORM_FORWARD][24][0]*EEEO[0] + g_aiT32[TRANSFORM_FORWARD][24][1]*EEEO[1] + add)>>shift;
    for (k=4;k<32;k+=8)
    {
      dst[ k*line ] = (g_aiT32[TRANSFORM_FORWARD][k][0]*EEO[0] + g_aiT32[TRANSFORM_FORWARD][k][1]*EEO[1] +
                       g_aiT32[TRANSFORM_FORWARD][k][2]*EEO[2] + g_aiT32[TRANSFORM_FORWARD][k][3]*EEO[3] + add)>>shift;
    }
    for (k=2;k<32;k+=4)
    {
      dst[ k*line ] = (g_aiT32[TRANSFORM_FORWARD][k][0]*EO[0] + g_aiT32[TRANSFORM_FORWARD][k][1]*EO[1] +
                       g_aiT32[TRANSFORM_FORWARD][k][2]*EO[2] + g_aiT32[TRANSFORM_FORWARD][k][3]*EO[3] +
                       g_aiT32[TRANSFORM_FORWARD][k][4]*EO[4] + g_aiT32[TRANSFORM_FORWARD][k][5]*EO[5] +
                       g_aiT32[TRANSFORM_FORWARD][k][6]*EO[6] + g_aiT32[TRANSFORM_FORWARD][k][7]*EO[7] + add)>>shift;
    }
    for (k=1;k<32;k+=2)
    {
      dst[ k*line ] = (g_aiT32[TRANSFORM_FORWARD][k][ 0]*O[ 0] + g_aiT32[TRANSFORM_FORWARD][k][ 1]*O[ 1] +
                       g_aiT32[TRANSFORM_FORWARD][k][ 2]*O[ 2] + g_aiT32[TRANSFORM_FORWARD][k][ 3]*O[ 3] +
                       g_aiT32[TRANSFORM_FORWARD][k][ 4]*O[ 4] + g_aiT32[TRANSFORM_FORWARD][k][ 5]*O[ 5] +
                       g_aiT32[TRANSFORM_FORWARD][k][ 6]*O[ 6] + g_aiT32[TRANSFORM_FORWARD][k][ 7]*O[ 7] +
                       g_aiT32[TRANSFORM_FORWARD][k][ 8]*O[ 8] + g_aiT32[TRANSFORM_FORWARD][k][ 9]*O[ 9] +
                       g_aiT32[TRANSFORM_FORWARD][k][10]*O[10] + g_aiT32[TRANSFORM_FORWARD][k][11]*O[11] +
                       g_aiT32[TRANSFORM_FORWARD][k][12]*O[12] + g_aiT32[TRANSFORM_FORWARD][k][13]*O[13] +
                       g_aiT32[TRANSFORM_FORWARD][k][14]*O[14] + g_aiT32[TRANSFORM_FORWARD][k][15]*O[15] + add)>>shift;
    }

    src += 32;
    dst ++;
  }
}

/** 32x32 inverse transform implemented using partial butterfly structure (1D)
 *  \param src   input data (transform coefficients)
 *  \param dst   output data (residual)
 *  \param shift specifies right shift after 1D transform
 *  \param line
 *  \param outputMinimum  minimum for clipping
 *  \param outputMaximum  maximum for clipping
 */
Void partialButterflyInverse32(TCoeff *src, TCoeff *dst, Int shift, Int line, const TCoeff outputMinimum, const TCoeff outputMaximum)
{
  Int j,k;
  TCoeff E[16],O[16];
  TCoeff EE[8],EO[8];
  TCoeff EEE[4],EEO[4];
  TCoeff EEEE[2],EEEO[2];
  TCoeff add = (shift > 0) ? (1<<(shift-1)) : 0;

  for (j=0; j<line; j++)
  {
    /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
    for (k=0;k<16;k++)
    {
      O[k] = g_aiT32[TRANSFORM_INVERSE][ 1][k]*src[ line    ] + g_aiT32[TRANSFORM_INVERSE][ 3][k]*src[ 3*line  ] +
             g_aiT32[TRANSFORM_INVERSE][ 5][k]*src[ 5*line  ] + g_aiT32[TRANSFORM_INVERSE][ 7][k]*src[ 7*line  ] +
             g_aiT32[TRANSFORM_INVERSE][ 9][k]*src[ 9*line  ] + g_aiT32[TRANSFORM_INVERSE][11][k]*src[ 11*line ] +
             g_aiT32[TRANSFORM_INVERSE][13][k]*src[ 13*line ] + g_aiT32[TRANSFORM_INVERSE][15][k]*src[ 15*line ] +
             g_aiT32[TRANSFORM_INVERSE][17][k]*src[ 17*line ] + g_aiT32[TRANSFORM_INVERSE][19][k]*src[ 19*line ] +
             g_aiT32[TRANSFORM_INVERSE][21][k]*src[ 21*line ] + g_aiT32[TRANSFORM_INVERSE][23][k]*src[ 23*line ] +
             g_aiT32[TRANSFORM_INVERSE][25][k]*src[ 25*line ] + g_aiT32[TRANSFORM_INVERSE][27][k]*src[ 27*line ] +
             g_aiT32[TRANSFORM_INVERSE][29][k]*src[ 29*line ] + g_aiT32[TRANSFORM_INVERSE][31][k]*src[ 31*line ];
    }
    for (k=0;k<8;k++)
    {
      EO[k] = g_aiT32[TRANSFORM_INVERSE][ 2][k]*src[ 2*line  ] + g_aiT32[TRANSFORM_INVERSE][ 6][k]*src[ 6*line  ] +
              g_aiT32[TRANSFORM_INVERSE][10][k]*src[ 10*line ] + g_aiT32[TRANSFORM_INVERSE][14][k]*src[ 14*line ] +
              g_aiT32[TRANSFORM_INVERSE][18][k]*src[ 18*line ] + g_aiT32[TRANSFORM_INVERSE][22][k]*src[ 22*line ] +
              g_aiT32[TRANSFORM_INVERSE][26][k]*src[ 26*line ] + g_aiT32[TRANSFORM_INVERSE][30][k]*src[ 30*line ];
    }
    for (k=0;k<4;k++)
    {
      EEO[k] = g_aiT32[TRANSFORM_INVERSE][ 4][k]*src[  4*line ] + g_aiT32[TRANSFORM_INVERSE][12][k]*src[ 12*line ] +
               g_aiT32[TRANSFORM_INVERSE][20][k]*src[ 20*line ] + g_aiT32[TRANSFORM_INVERSE][28][k]*src[ 28*line ];
    }
    EEEO[0] = g_aiT32[TRANSFORM_INVERSE][8][0]*src[ 8*line ] + g_aiT32[TRANSFORM_INVERSE][24][0]*src[ 24*line ];
    EEEO[1] = g_aiT32[TRANSFORM_INVERSE][8][1]*src[ 8*line ] + g_aiT32[TRANSFORM_INVERSE][24][1]*src[ 24*line ];
    EEEE[0] = g_aiT32[TRANSFORM_INVERSE][0][0]*src[ 0      ] + g_aiT32[TRANSFORM_INVERSE][16][0]*src[ 16*line ];
    EEEE[1] = g_aiT32[TRANSFORM_INVERSE][0][1]*src[ 0      ] + g_aiT32[TRANSFORM_INVERSE][16][1]*src[ 16*line ];

    /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
    EEE[0] = EEEE[0] + EEEO[0];
    EEE[3] = EEEE[0] - EEEO[0];
    EEE[1] = EEEE[1] + EEEO[1];
    EEE[2] = EEEE[1] - EEEO[1];
    for (k=0;k<4;k++)
    {
      EE[k] = EEE[k] + EEO[k];
      EE[k+4] = EEE[3-k] - EEO[3-k];
    }
    for (k=0;k<8;k++)
    {
      E[k] = EE[k] + EO[k];
      E[k+8] = EE[7-k] - EO[7-k];
    }
    for (k=0;k<16;k++)
    {
      dst[k]    = Clip3( outputMinimum, outputMaximum, (E[k] + O[k] + add)>>shift );
      dst[k+16] = Clip3( outputMinimum, outputMaximum, (E[15-k] - O[15-k] + add)>>shift );
    }
    src ++;
    dst += 32;
  }
}

/** MxN forward transform (2D)
*  \param bitDepth              [in]  bit depth
*  \param block                 [in]  residual block
*  \param coeff                 [out] transform coefficients
*  \param iWidth                [in]  width of transform
*  \param iHeight               [in]  height of transform
*  \param useDST                [in]
*  \param maxLog2TrDynamicRange [in]

*/
Void xTrMxN(Int bitDepth, TCoeff *block, TCoeff *coeff, Int iWidth, Int iHeight, Bool useDST, const Int maxLog2TrDynamicRange)
{
  const Int TRANSFORM_MATRIX_SHIFT = g_transformMatrixShift[TRANSFORM_FORWARD];

  const Int shift_1st = ((g_aucConvertToBit[iWidth] + 2) +  bitDepth + TRANSFORM_MATRIX_SHIFT) - maxLog2TrDynamicRange;
  const Int shift_2nd = (g_aucConvertToBit[iHeight] + 2) + TRANSFORM_MATRIX_SHIFT;

  assert(shift_1st >= 0);
  assert(shift_2nd >= 0);

  TCoeff tmp[ MAX_TU_SIZE * MAX_TU_SIZE ];

  switch (iWidth)
  {
    case 4:
      {
        if ((iHeight == 4) && useDST)    // Check for DCT or DST
        {
           fastForwardDst( block, tmp, shift_1st );
        }
        else
        {
          partialButterfly4 ( block, tmp, shift_1st, iHeight );
        }
      }
      break;

    case 8:     partialButterfly8 ( block, tmp, shift_1st, iHeight );  break;
    case 16:    partialButterfly16( block, tmp, shift_1st, iHeight );  break;
    case 32:    partialButterfly32( block, tmp, shift_1st, iHeight );  break;
    default:
      assert(0); exit (1); break;
  }

  switch (iHeight)
  {
    case 4:
      {
        if ((iWidth == 4) && useDST)    // Check for DCT or DST
        {
          fastForwardDst( tmp, coeff, shift_2nd );
        }
        else
        {
          partialButterfly4 ( tmp, coeff, shift_2nd, iWidth );
        }
      }
      break;

    case 8:     partialButterfly8 ( tmp, coeff, shift_2nd, iWidth );    break;
    case 16:    partialButterfly16( tmp, coeff, shift_2nd, iWidth );    break;
    case 32:    partialButterfly32( tmp, coeff, shift_2nd, iWidth );    break;
    default:
      assert(0); exit (1); break;
  }
}


/** MxN inverse transform (2D)
*  \param bitDepth              [in]  bit depth
*  \param coeff                 [in]  transform coefficients
*  \param block                 [out] residual block
*  \param iWidth                [in]  width of transform
*  \param iHeight               [in]  height of transform
*  \param useDST                [in]
*  \param maxLog2TrDynamicRange [in]
*/
Void xITrMxN(Int bitDepth, TCoeff *coeff, TCoeff *block, Int iWidth, Int iHeight, Bool useDST, const Int maxLog2TrDynamicRange)
{
  const Int TRANSFORM_MATRIX_SHIFT = g_transformMatrixShift[TRANSFORM_INVERSE];

  Int shift_1st = TRANSFORM_MATRIX_SHIFT + 1; //1 has been added to shift_1st at the expense of shift_2nd
  Int shift_2nd = (TRANSFORM_MATRIX_SHIFT + maxLog2TrDynamicRange - 1) - bitDepth;
  const TCoeff clipMinimum = -(1 << maxLog2TrDynamicRange);
  const TCoeff clipMaximum =  (1 << maxLog2TrDynamicRange) - 1;

  assert(shift_1st >= 0);
  assert(shift_2nd >= 0);

  TCoeff tmp[MAX_TU_SIZE * MAX_TU_SIZE];

  switch (iHeight)
  {
    case 4:
      {
        if ((iWidth == 4) && useDST)    // Check for DCT or DST
        {
          fastInverseDst( coeff, tmp, shift_1st, clipMinimum, clipMaximum);
        }
        else
        {
          partialButterflyInverse4 ( coeff, tmp, shift_1st, iWidth, clipMinimum, clipMaximum);
        }
      }
      break;

    case  8: partialButterflyInverse8 ( coeff, tmp, shift_1st, iWidth, clipMinimum, clipMaximum); break;
    case 16: partialButterflyInverse16( coeff, tmp, shift_1st, iWidth, clipMinimum, clipMaximum); break;
    case 32: partialButterflyInverse32( coeff, tmp, shift_1st, iWidth, clipMinimum, clipMaximum); break;

    default:
      assert(0); exit (1); break;
  }

  switch (iWidth)
  {
    // Clipping here is not in the standard, but is used to protect the "Pel" data type into which the inverse-transformed samples will be copied
    case 4:
      {
        if ((iHeight == 4) && useDST)    // Check for DCT or DST
        {
          fastInverseDst( tmp, block, shift_2nd, std::numeric_limits<Pel>::min(), std::numeric_limits<Pel>::max() );
        }
        else
        {
          partialButterflyInverse4 ( tmp, block, shift_2nd, iHeight, std::numeric_limits<Pel>::min(), std::numeric_limits<Pel>::max());
        }
      }
      break;

    case  8: partialButterflyInverse8 ( tmp, block, shift_2nd, iHeight, std::numeric_limits<Pel>::min(), std::numeric_limits<Pel>::max()); break;
    case 16: partialButterflyInverse16( tmp, block, shift_2nd, iHeight, std::numeric_limits<Pel>::min(), std::numeric_limits<Pel>::max()); break;
    case 32: partialButterflyInverse32( tmp, block, shift_2nd, iHeight, std::numeric_limits<Pel>::min(), std::numeric_limits<Pel>::max()); break;

    default:
      assert(0); exit (1); break;
  }
}


// To minimize the distortion only. No rate is considered.
Void TComTrQuant::signBitHidingHDQ( TCoeff* pQCoef, TCoeff* pCoef, TCoeff* deltaU, const TUEntropyCodingParameters &codingParameters, const Int maxLog2TrDynamicRange )
{
  const UInt width     = codingParameters.widthInGroups  << MLS_CG_LOG2_WIDTH;
  const UInt height    = codingParameters.heightInGroups << MLS_CG_LOG2_HEIGHT;
  const UInt groupSize = 1 << MLS_CG_SIZE;

  const TCoeff entropyCodingMinimum = -(1 << maxLog2TrDynamicRange);
  const TCoeff entropyCodingMaximum =  (1 << maxLog2TrDynamicRange) - 1;

  Int lastCG = -1;
  Int absSum = 0 ;
  Int n ;

  for( Int subSet = (width*height-1) >> MLS_CG_SIZE; subSet >= 0; subSet-- )
  {
    Int  subPos = subSet << MLS_CG_SIZE;
    Int  firstNZPosInCG=groupSize , lastNZPosInCG=-1 ;
    absSum = 0 ;

    for(n = groupSize-1; n >= 0; --n )
    {
      if( pQCoef[ codingParameters.scan[ n + subPos ]] )
      {
        lastNZPosInCG = n;
        break;
      }
    }

    for(n = 0; n <groupSize; n++ )
    {
      if( pQCoef[ codingParameters.scan[ n + subPos ]] )
      {
        firstNZPosInCG = n;
        break;
      }
    }

    for(n = firstNZPosInCG; n <=lastNZPosInCG; n++ )
    {
      absSum += Int(pQCoef[ codingParameters.scan[ n + subPos ]]);
    }

    if(lastNZPosInCG>=0 && lastCG==-1)
    {
      lastCG = 1 ;
    }

    if( lastNZPosInCG-firstNZPosInCG>=SBH_THRESHOLD )
    {
      UInt signbit = (pQCoef[codingParameters.scan[subPos+firstNZPosInCG]]>0?0:1) ;
      if( signbit!=(absSum&0x1) )  //compare signbit with sum_parity
      {
        TCoeff curCost    = std::numeric_limits<TCoeff>::max();
        TCoeff minCostInc = std::numeric_limits<TCoeff>::max();
        Int minPos =-1, finalChange=0, curChange=0;

        for( n = (lastCG==1?lastNZPosInCG:groupSize-1) ; n >= 0; --n )
        {
          UInt blkPos   = codingParameters.scan[ n+subPos ];
          if(pQCoef[ blkPos ] != 0 )
          {
            if(deltaU[blkPos]>0)
            {
              curCost = - deltaU[blkPos];
              curChange=1 ;
            }
            else
            {
              //curChange =-1;
              if(n==firstNZPosInCG && abs(pQCoef[blkPos])==1)
              {
                curCost = std::numeric_limits<TCoeff>::max();
              }
              else
              {
                curCost = deltaU[blkPos];
                curChange =-1;
              }
            }
          }
          else
          {
            if(n<firstNZPosInCG)
            {
              UInt thisSignBit = (pCoef[blkPos]>=0?0:1);
              if(thisSignBit != signbit )
              {
                curCost = std::numeric_limits<TCoeff>::max();
              }
              else
              {
                curCost = - (deltaU[blkPos])  ;
                curChange = 1 ;
              }
            }
            else
            {
              curCost = - (deltaU[blkPos])  ;
              curChange = 1 ;
            }
          }

          if( curCost<minCostInc)
          {
            minCostInc = curCost ;
            finalChange = curChange ;
            minPos = blkPos ;
          }
        } //CG loop

        if(pQCoef[minPos] == entropyCodingMaximum || pQCoef[minPos] == entropyCodingMinimum)
        {
          finalChange = -1;
        }

        if(pCoef[minPos]>=0)
        {
          pQCoef[minPos] += finalChange ;
        }
        else
        {
          pQCoef[minPos] -= finalChange ;
        }
      } // Hide
    }
    if(lastCG==1)
    {
      lastCG=0 ;
    }
  } // TU loop

  return;
}


Void TComTrQuant::xQuant(       TComTU       &rTu,
                                TCoeff      * pSrc,
                                TCoeff      * pDes,
#if ADAPTIVE_QP_SELECTION
                                TCoeff      *pArlDes,
#endif
                                TCoeff       &uiAbsSum,
                          const ComponentID   compID,
                          const QpParam      &cQP )
{
  const TComRectangle &rect = rTu.getRect(compID);
  const UInt uiWidth        = rect.width;
  const UInt uiHeight       = rect.height;
  TComDataCU* pcCU          = rTu.getCU();
  const UInt uiAbsPartIdx   = rTu.GetAbsPartIdxTU();
  const Int channelBitDepth = pcCU->getSlice()->getSPS()->getBitDepth(toChannelType(compID));

  TCoeff* piCoef    = pSrc;
  TCoeff* piQCoef   = pDes;
#if ADAPTIVE_QP_SELECTION
  TCoeff* piArlCCoef = pArlDes;
#endif

  const Bool useTransformSkip      = pcCU->getTransformSkip(uiAbsPartIdx, compID);
  const Int  maxLog2TrDynamicRange = pcCU->getSlice()->getSPS()->getMaxLog2TrDynamicRange(toChannelType(compID));

  Bool useRDOQ = useTransformSkip ? m_useRDOQTS : m_useRDOQ;
  if ( useRDOQ && (isLuma(compID) || RDOQ_CHROMA) )
  {
    if ( !m_useSelectiveRDOQ || xNeedRDOQ( rTu, piCoef, compID, cQP ) )
    {
#if ADAPTIVE_QP_SELECTION
      xRateDistOptQuant( rTu, piCoef, pDes, pArlDes, uiAbsSum, compID, cQP );
#else
      xRateDistOptQuant( rTu, piCoef, pDes, uiAbsSum, compID, cQP );
#endif
    }
    else
    {
      memset( pDes, 0, sizeof( TCoeff ) * uiWidth *uiHeight );
      uiAbsSum = 0;
    }
  }
  else
  {
    TUEntropyCodingParameters codingParameters;
    getTUEntropyCodingParameters(codingParameters, rTu, compID);

    const TCoeff entropyCodingMinimum = -(1 << maxLog2TrDynamicRange);
    const TCoeff entropyCodingMaximum =  (1 << maxLog2TrDynamicRange) - 1;

    TCoeff deltaU[MAX_TU_SIZE * MAX_TU_SIZE];

    const UInt uiLog2TrSize = rTu.GetEquivalentLog2TrSize(compID);

    Int scalingListType = getScalingListType(pcCU->getPredictionMode(uiAbsPartIdx), compID);
    assert(scalingListType < SCALING_LIST_NUM);
    Int *piQuantCoeff = getQuantCoeff(scalingListType, cQP.rem, uiLog2TrSize-2);

    const Bool enableScalingLists             = getUseScalingList(uiWidth, uiHeight, (pcCU->getTransformSkip(uiAbsPartIdx, compID) != 0));
    const Int  defaultQuantisationCoefficient = g_quantScales[cQP.rem];

    /* for 422 chroma blocks, the effective scaling applied during transformation is not a power of 2, hence it cannot be
     * implemented as a bit-shift (the quantised result will be sqrt(2) * larger than required). Alternatively, adjust the
     * uiLog2TrSize applied in iTransformShift, such that the result is 1/sqrt(2) the required result (i.e. smaller)
     * Then a QP+3 (sqrt(2)) or QP-3 (1/sqrt(2)) method could be used to get the required result
     */

    // Represents scaling through forward transform
    Int iTransformShift = getTransformShift(channelBitDepth, uiLog2TrSize, maxLog2TrDynamicRange);
    if (useTransformSkip && pcCU->getSlice()->getSPS()->getSpsRangeExtension().getExtendedPrecisionProcessingFlag())
    {
      iTransformShift = std::max<Int>(0, iTransformShift);
    }

    const Int iQBits = QUANT_SHIFT + cQP.per + iTransformShift;
    // QBits will be OK for any internal bit depth as the reduction in transform shift is balanced by an increase in Qp_per due to QpBDOffset

#if ADAPTIVE_QP_SELECTION
    Int iQBitsC = MAX_INT;
    Int iAddC   = MAX_INT;

    if (m_bUseAdaptQpSelect)
    {
      iQBitsC = iQBits - ARL_C_PRECISION;
      iAddC   = 1 << (iQBitsC-1);
    }
#endif

    const Int iAdd   = (pcCU->getSlice()->getSliceType()==I_SLICE ? 171 : 85) << (iQBits-9);
    const Int qBits8 = iQBits - 8;

    for( Int uiBlockPos = 0; uiBlockPos < uiWidth*uiHeight; uiBlockPos++ )
    {
      const TCoeff iLevel   = piCoef[uiBlockPos];
      const TCoeff iSign    = (iLevel < 0 ? -1: 1);

      const Int64  tmpLevel = (Int64)abs(iLevel) * (enableScalingLists ? piQuantCoeff[uiBlockPos] : defaultQuantisationCoefficient);

#if ADAPTIVE_QP_SELECTION
      if( m_bUseAdaptQpSelect )
      {
        piArlCCoef[uiBlockPos] = (TCoeff)((tmpLevel + iAddC ) >> iQBitsC);
      }
#endif

      const TCoeff quantisedMagnitude = TCoeff((tmpLevel + iAdd ) >> iQBits);
      deltaU[uiBlockPos] = (TCoeff)((tmpLevel - (quantisedMagnitude<<iQBits) )>> qBits8);

      uiAbsSum += quantisedMagnitude;
      const TCoeff quantisedCoefficient = quantisedMagnitude * iSign;

      piQCoef[uiBlockPos] = Clip3<TCoeff>( entropyCodingMinimum, entropyCodingMaximum, quantisedCoefficient );
    } // for n

    if( pcCU->getSlice()->getPPS()->getSignDataHidingEnabledFlag() )
    {
      if(uiAbsSum >= 2) //this prevents TUs with only one coefficient of value 1 from being tested
      {
        signBitHidingHDQ( piQCoef, piCoef, deltaU, codingParameters, maxLog2TrDynamicRange ) ;
      }
    }
  } //if RDOQ
  //return;
}

Bool TComTrQuant::xNeedRDOQ( TComTU &rTu, TCoeff * pSrc, const ComponentID compID, const QpParam &cQP )
{
  const TComRectangle &rect = rTu.getRect(compID);
  const UInt uiWidth        = rect.width;
  const UInt uiHeight       = rect.height;
  TComDataCU* pcCU          = rTu.getCU();
  const UInt uiAbsPartIdx   = rTu.GetAbsPartIdxTU();
  const Int channelBitDepth = pcCU->getSlice()->getSPS()->getBitDepth(toChannelType(compID));

  TCoeff* piCoef    = pSrc;

  const Bool useTransformSkip      = pcCU->getTransformSkip(uiAbsPartIdx, compID);
  const Int  maxLog2TrDynamicRange = pcCU->getSlice()->getSPS()->getMaxLog2TrDynamicRange(toChannelType(compID));

  const UInt uiLog2TrSize = rTu.GetEquivalentLog2TrSize(compID);

  Int scalingListType = getScalingListType(pcCU->getPredictionMode(uiAbsPartIdx), compID);
  assert(scalingListType < SCALING_LIST_NUM);
  Int *piQuantCoeff = getQuantCoeff(scalingListType, cQP.rem, uiLog2TrSize-2);

  const Bool enableScalingLists             = getUseScalingList(uiWidth, uiHeight, (pcCU->getTransformSkip(uiAbsPartIdx, compID) != 0));
  const Int  defaultQuantisationCoefficient = g_quantScales[cQP.rem];

  /* for 422 chroma blocks, the effective scaling applied during transformation is not a power of 2, hence it cannot be
    * implemented as a bit-shift (the quantised result will be sqrt(2) * larger than required). Alternatively, adjust the
    * uiLog2TrSize applied in iTransformShift, such that the result is 1/sqrt(2) the required result (i.e. smaller)
    * Then a QP+3 (sqrt(2)) or QP-3 (1/sqrt(2)) method could be used to get the required result
    */

  // Represents scaling through forward transform
  Int iTransformShift = getTransformShift(channelBitDepth, uiLog2TrSize, maxLog2TrDynamicRange);
  if (useTransformSkip && pcCU->getSlice()->getSPS()->getSpsRangeExtension().getExtendedPrecisionProcessingFlag())
  {
    iTransformShift = std::max<Int>(0, iTransformShift);
  }

  const Int iQBits = QUANT_SHIFT + cQP.per + iTransformShift;
  // QBits will be OK for any internal bit depth as the reduction in transform shift is balanced by an increase in Qp_per due to QpBDOffset

  // iAdd is different from the iAdd used in normal quantization
  const Int iAdd   = (compID == COMPONENT_Y ? 171 : 256) << (iQBits-9);

  for( Int uiBlockPos = 0; uiBlockPos < uiWidth*uiHeight; uiBlockPos++ )
  {
    const TCoeff iLevel   = piCoef[uiBlockPos];
    const Int64  tmpLevel = (Int64)abs(iLevel) * (enableScalingLists ? piQuantCoeff[uiBlockPos] : defaultQuantisationCoefficient);
    const TCoeff quantisedMagnitude = TCoeff((tmpLevel + iAdd ) >> iQBits);

    if ( quantisedMagnitude != 0 )
    {
      return true;
    }
  } // for n
  return false;
}

Void TComTrQuant::xDeQuant(       TComTU        &rTu,
                            const TCoeff       * pSrc,
                                  TCoeff       * pDes,
                            const ComponentID    compID,
                            const QpParam       &cQP )
{
  assert(compID<MAX_NUM_COMPONENT);

        TComDataCU          *pcCU               = rTu.getCU();
  const UInt                 uiAbsPartIdx       = rTu.GetAbsPartIdxTU();
  const TComRectangle       &rect               = rTu.getRect(compID);
  const UInt                 uiWidth            = rect.width;
  const UInt                 uiHeight           = rect.height;
  const TCoeff        *const piQCoef            = pSrc;
        TCoeff        *const piCoef             = pDes;
  const UInt                 uiLog2TrSize       = rTu.GetEquivalentLog2TrSize(compID);
  const UInt                 numSamplesInBlock  = uiWidth*uiHeight;
  const Int                  maxLog2TrDynamicRange  = pcCU->getSlice()->getSPS()->getMaxLog2TrDynamicRange(toChannelType(compID));
  const TCoeff               transformMinimum   = -(1 << maxLog2TrDynamicRange);
  const TCoeff               transformMaximum   =  (1 << maxLog2TrDynamicRange) - 1;
  const Bool                 enableScalingLists = getUseScalingList(uiWidth, uiHeight, (pcCU->getTransformSkip(uiAbsPartIdx, compID) != 0));
  const Int                  scalingListType    = getScalingListType(pcCU->getPredictionMode(uiAbsPartIdx), compID);
#if O0043_BEST_EFFORT_DECODING
  const Int                  channelBitDepth    = pcCU->getSlice()->getSPS()->getStreamBitDepth(toChannelType(compID));
#else
  const Int                  channelBitDepth    = pcCU->getSlice()->getSPS()->getBitDepth(toChannelType(compID));
#endif

  assert (scalingListType < SCALING_LIST_NUM);
  assert ( uiWidth <= m_uiMaxTrSize );

  // Represents scaling through forward transform
  const Bool bClipTransformShiftTo0 = (pcCU->getTransformSkip(uiAbsPartIdx, compID) != 0) && pcCU->getSlice()->getSPS()->getSpsRangeExtension().getExtendedPrecisionProcessingFlag();
  const Int  originalTransformShift = getTransformShift(channelBitDepth, uiLog2TrSize, maxLog2TrDynamicRange);
  const Int  iTransformShift        = bClipTransformShiftTo0 ? std::max<Int>(0, originalTransformShift) : originalTransformShift;

  const Int QP_per = cQP.per;
  const Int QP_rem = cQP.rem;

  const Int rightShift = (IQUANT_SHIFT - (iTransformShift + QP_per)) + (enableScalingLists ? LOG2_SCALING_LIST_NEUTRAL_VALUE : 0);

  if(enableScalingLists)
  {
    //from the dequantisation equation:
    //iCoeffQ                         = ((Intermediate_Int(clipQCoef) * piDequantCoef[deQuantIdx]) + iAdd ) >> rightShift
    //(sizeof(Intermediate_Int) * 8)  =              inputBitDepth    +    dequantCoefBits                   - rightShift
    const UInt             dequantCoefBits     = 1 + IQUANT_SHIFT + SCALING_LIST_BITS;
    const UInt             targetInputBitDepth = std::min<UInt>((maxLog2TrDynamicRange + 1), (((sizeof(Intermediate_Int) * 8) + rightShift) - dequantCoefBits));

    const Intermediate_Int inputMinimum        = -(1 << (targetInputBitDepth - 1));
    const Intermediate_Int inputMaximum        =  (1 << (targetInputBitDepth - 1)) - 1;

    Int *piDequantCoef = getDequantCoeff(scalingListType,QP_rem,uiLog2TrSize-2);

    if(rightShift > 0)
    {
      const Intermediate_Int iAdd = 1 << (rightShift - 1);

      for( Int n = 0; n < numSamplesInBlock; n++ )
      {
        const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, piQCoef[n]));
        const Intermediate_Int iCoeffQ   = ((Intermediate_Int(clipQCoef) * piDequantCoef[n]) + iAdd ) >> rightShift;

        piCoef[n] = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
      }
    }
    else
    {
      const Int leftShift = -rightShift;

      for( Int n = 0; n < numSamplesInBlock; n++ )
      {
        const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, piQCoef[n]));
        const Intermediate_Int iCoeffQ   = (Intermediate_Int(clipQCoef) * piDequantCoef[n]) << leftShift;

        piCoef[n] = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
      }
    }
  }
  else
  {
    const Int scale     =  g_invQuantScales[QP_rem];
    const Int scaleBits =     (IQUANT_SHIFT + 1)   ;

    //from the dequantisation equation:
    //iCoeffQ                         = Intermediate_Int((Int64(clipQCoef) * scale + iAdd) >> rightShift);
    //(sizeof(Intermediate_Int) * 8)  =                    inputBitDepth   + scaleBits      - rightShift
    const UInt             targetInputBitDepth = std::min<UInt>((maxLog2TrDynamicRange + 1), (((sizeof(Intermediate_Int) * 8) + rightShift) - scaleBits));
    const Intermediate_Int inputMinimum        = -(1 << (targetInputBitDepth - 1));
    const Intermediate_Int inputMaximum        =  (1 << (targetInputBitDepth - 1)) - 1;

    if (rightShift > 0)
    {
      const Intermediate_Int iAdd = 1 << (rightShift - 1);

      for( Int n = 0; n < numSamplesInBlock; n++ )
      {
        const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, piQCoef[n]));
        const Intermediate_Int iCoeffQ   = (Intermediate_Int(clipQCoef) * scale + iAdd) >> rightShift;

        piCoef[n] = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
      }
    }
    else
    {
      const Int leftShift = -rightShift;

      for( Int n = 0; n < numSamplesInBlock; n++ )
      {
        const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, piQCoef[n]));
        const Intermediate_Int iCoeffQ   = (Intermediate_Int(clipQCoef) * scale) << leftShift;

        piCoef[n] = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
      }
    }
  }
}


Void TComTrQuant::init(   UInt  uiMaxTrSize,
                          Bool  bUseRDOQ,
                          Bool  bUseRDOQTS,
                          Bool  useSelectiveRDOQ,
                          Bool  bEnc,
                          Bool  useTransformSkipFast
#if ADAPTIVE_QP_SELECTION
                        , Bool bUseAdaptQpSelect
#endif
                       )
{
  m_uiMaxTrSize  = uiMaxTrSize;
  m_bEnc         = bEnc;
  m_useRDOQ      = bUseRDOQ;
  m_useRDOQTS    = bUseRDOQTS;
  m_useSelectiveRDOQ = useSelectiveRDOQ;
#if ADAPTIVE_QP_SELECTION
  m_bUseAdaptQpSelect = bUseAdaptQpSelect;
#endif
  m_useTransformSkipFast = useTransformSkipFast;
}


Void TComTrQuant::transformNxN(       TComTU        & rTu,
                                const ComponentID     compID,
                                      Pel          *  pcResidual,
                                const UInt            uiStride,
                                      TCoeff       *  rpcCoeff,
#if ADAPTIVE_QP_SELECTION
                                      TCoeff       *  pcArlCoeff,
#endif
                                      TCoeff        & uiAbsSum,
                                const QpParam       & cQP
                              )
{
  const TComRectangle &rect = rTu.getRect(compID);
  const UInt uiWidth        = rect.width;
  const UInt uiHeight       = rect.height;
  TComDataCU* pcCU          = rTu.getCU();
  const UInt uiAbsPartIdx   = rTu.GetAbsPartIdxTU();
  const UInt uiOrgTrDepth   = rTu.GetTransformDepthRel();

  uiAbsSum=0;

  RDPCMMode rdpcmMode = RDPCM_OFF;
  rdpcmNxN( rTu, compID, pcResidual, uiStride, cQP, rpcCoeff, uiAbsSum, rdpcmMode );

  if (rdpcmMode == RDPCM_OFF)
  {
    uiAbsSum = 0;
    //transform and quantise
    if(pcCU->getCUTransquantBypass(uiAbsPartIdx))
    {
      const Bool rotateResidual = rTu.isNonTransformedResidualRotated(compID);
      const UInt uiSizeMinus1   = (uiWidth * uiHeight) - 1;

      for (UInt y = 0, coefficientIndex = 0; y<uiHeight; y++)
      {
        for (UInt x = 0; x<uiWidth; x++, coefficientIndex++)
        {
          const Pel currentSample = pcResidual[(y * uiStride) + x];

          rpcCoeff[rotateResidual ? (uiSizeMinus1 - coefficientIndex) : coefficientIndex] = currentSample;
          uiAbsSum += TCoeff(abs(currentSample));
        }
      }
    }
    else
    {
#if DEBUG_TRANSFORM_AND_QUANTISE
      std::cout << g_debugCounter << ": " << uiWidth << "x" << uiHeight << " channel " << compID << " TU at input to transform\n";
      printBlock(pcResidual, uiWidth, uiHeight, uiStride);
#endif

      assert( (pcCU->getSlice()->getSPS()->getMaxTrSize() >= uiWidth) );

      if(pcCU->getTransformSkip(uiAbsPartIdx, compID) != 0)
      {
        xTransformSkip( pcResidual, uiStride, m_plTempCoeff, rTu, compID );
      }
      else
      {
        const Int channelBitDepth=pcCU->getSlice()->getSPS()->getBitDepth(toChannelType(compID));
        xT( channelBitDepth, rTu.useDST(compID), pcResidual, uiStride, m_plTempCoeff, uiWidth, uiHeight, pcCU->getSlice()->getSPS()->getMaxLog2TrDynamicRange(toChannelType(compID)) );
      }

#if DEBUG_TRANSFORM_AND_QUANTISE
      std::cout << g_debugCounter << ": " << uiWidth << "x" << uiHeight << " channel " << compID << " TU between transform and quantiser\n";
      printBlock(m_plTempCoeff, uiWidth, uiHeight, uiWidth);
#endif

      xQuant( rTu, m_plTempCoeff, rpcCoeff,

#if ADAPTIVE_QP_SELECTION
              pcArlCoeff,
#endif
              uiAbsSum, compID, cQP );

#if DEBUG_TRANSFORM_AND_QUANTISE
      std::cout << g_debugCounter << ": " << uiWidth << "x" << uiHeight << " channel " << compID << " TU at output of quantiser\n";
      printBlock(rpcCoeff, uiWidth, uiHeight, uiWidth);
#endif
    }
  }

    //set the CBF
  pcCU->setCbfPartRange((((uiAbsSum > 0) ? 1 : 0) << uiOrgTrDepth), compID, uiAbsPartIdx, rTu.GetAbsPartIdxNumParts(compID));
}


Void TComTrQuant::invTransformNxN(      TComTU        &rTu,
                                  const ComponentID    compID,
                                        Pel          *pcResidual,
                                  const UInt           uiStride,
                                        TCoeff       * pcCoeff,
                                  const QpParam       &cQP
                                        DEBUG_STRING_FN_DECLAREP(psDebug))
{
  TComDataCU* pcCU=rTu.getCU();
  const UInt uiAbsPartIdx = rTu.GetAbsPartIdxTU();
  const TComRectangle &rect = rTu.getRect(compID);
  const UInt uiWidth = rect.width;
  const UInt uiHeight = rect.height;

  if (uiWidth != uiHeight) //for intra, the TU will have been split above this level, so this condition won't be true, hence this only affects inter
  {
    //------------------------------------------------

    //recurse deeper

    TComTURecurse subTURecurse(rTu, false, TComTU::VERTICAL_SPLIT, true, compID);

    do
    {
      //------------------

      const UInt lineOffset = subTURecurse.GetSectionNumber() * subTURecurse.getRect(compID).height;

      Pel    *subTUResidual     = pcResidual + (lineOffset * uiStride);
      TCoeff *subTUCoefficients = pcCoeff     + (lineOffset * subTURecurse.getRect(compID).width);

      invTransformNxN(subTURecurse, compID, subTUResidual, uiStride, subTUCoefficients, cQP DEBUG_STRING_PASS_INTO(psDebug));

      //------------------

    } while (subTURecurse.nextSection(rTu));

    //------------------------------------------------

    return;
  }

#if DEBUG_STRING
  if (psDebug)
  {
    std::stringstream ss(stringstream::out);
    printBlockToStream(ss, (compID==0)?"###InvTran ip Ch0: " : ((compID==1)?"###InvTran ip Ch1: ":"###InvTran ip Ch2: "), pcCoeff, uiWidth, uiHeight, uiWidth);
    DEBUG_STRING_APPEND((*psDebug), ss.str())
  }
#endif

  if(pcCU->getCUTransquantBypass(uiAbsPartIdx))
  {
    const Bool rotateResidual = rTu.isNonTransformedResidualRotated(compID);
    const UInt uiSizeMinus1   = (uiWidth * uiHeight) - 1;

    for (UInt y = 0, coefficientIndex = 0; y<uiHeight; y++)
    {
      for (UInt x = 0; x<uiWidth; x++, coefficientIndex++)
      {
        pcResidual[(y * uiStride) + x] = Pel(pcCoeff[rotateResidual ? (uiSizeMinus1 - coefficientIndex) : coefficientIndex]);
      }
    }
  }
  else
  {
#if DEBUG_TRANSFORM_AND_QUANTISE
    std::cout << g_debugCounter << ": " << uiWidth << "x" << uiHeight << " channel " << compID << " TU at input to dequantiser\n";
    printBlock(pcCoeff, uiWidth, uiHeight, uiWidth);
#endif

    xDeQuant(rTu, pcCoeff, m_plTempCoeff, compID, cQP);

#if DEBUG_TRANSFORM_AND_QUANTISE
    std::cout << g_debugCounter << ": " << uiWidth << "x" << uiHeight << " channel " << compID << " TU between dequantiser and inverse-transform\n";
    printBlock(m_plTempCoeff, uiWidth, uiHeight, uiWidth);
#endif

#if DEBUG_STRING
    if (psDebug)
    {
      std::stringstream ss(stringstream::out);
      printBlockToStream(ss, "###InvTran deq: ", m_plTempCoeff, uiWidth, uiHeight, uiWidth);
      (*psDebug)+=ss.str();
    }
#endif

    if(pcCU->getTransformSkip(uiAbsPartIdx, compID))
    {
      xITransformSkip( m_plTempCoeff, pcResidual, uiStride, rTu, compID );

#if DEBUG_STRING
      if (psDebug)
      {
        std::stringstream ss(stringstream::out);
        printBlockToStream(ss, "###InvTran resi: ", pcResidual, uiWidth, uiHeight, uiStride);
        (*psDebug)+=ss.str();
        (*psDebug)+="(<- was a Transform-skipped block)\n";
      }
#endif
    }
    else
    {
#if O0043_BEST_EFFORT_DECODING
      const Int channelBitDepth = pcCU->getSlice()->getSPS()->getStreamBitDepth(toChannelType(compID));
#else
      const Int channelBitDepth = pcCU->getSlice()->getSPS()->getBitDepth(toChannelType(compID));
#endif
      xIT( channelBitDepth, rTu.useDST(compID), m_plTempCoeff, pcResidual, uiStride, uiWidth, uiHeight, pcCU->getSlice()->getSPS()->getMaxLog2TrDynamicRange(toChannelType(compID)) );

#if DEBUG_STRING
      if (psDebug)
      {
        std::stringstream ss(stringstream::out);
        printBlockToStream(ss, "###InvTran resi: ", pcResidual, uiWidth, uiHeight, uiStride);
        (*psDebug)+=ss.str();
        (*psDebug)+="(<- was a Transformed block)\n";
      }
#endif
    }

#if DEBUG_TRANSFORM_AND_QUANTISE
    std::cout << g_debugCounter << ": " << uiWidth << "x" << uiHeight << " channel " << compID << " TU at output of inverse-transform\n";
    printBlock(pcResidual, uiWidth, uiHeight, uiStride);
    g_debugCounter++;
#endif
  }

  invRdpcmNxN( rTu, compID, pcResidual, uiStride );
}

Void TComTrQuant::invRecurTransformNxN( const ComponentID compID,
                                        TComYuv *pResidual,
                                        TComTU &rTu)
{
  if (!rTu.ProcessComponentSection(compID))
  {
    return;
  }

  TComDataCU* pcCU = rTu.getCU();
  UInt absPartIdxTU = rTu.GetAbsPartIdxTU();
  UInt uiTrMode=rTu.GetTransformDepthRel();
  if( (pcCU->getCbf(absPartIdxTU, compID, uiTrMode) == 0) && (isLuma(compID) || !pcCU->getSlice()->getPPS()->getPpsRangeExtension().getCrossComponentPredictionEnabledFlag()) )
  {
    return;
  }

  if( uiTrMode == pcCU->getTransformIdx( absPartIdxTU ) )
  {
    const TComRectangle &tuRect      = rTu.getRect(compID);
    const Int            uiStride    = pResidual->getStride( compID );
          Pel           *rpcResidual = pResidual->getAddr( compID );
          UInt           uiAddr      = (tuRect.x0 + uiStride*tuRect.y0);
          Pel           *pResi       = rpcResidual + uiAddr;
          TCoeff        *pcCoeff     = pcCU->getCoeff(compID) + rTu.getCoefficientOffset(compID);

    const QpParam cQP(*pcCU, compID);

    if(pcCU->getCbf(absPartIdxTU, compID, uiTrMode) != 0)
    {
      DEBUG_STRING_NEW(sTemp)
#if DEBUG_STRING
      std::string *psDebug=((DebugOptionList::DebugString_InvTran.getInt()&(pcCU->isIntra(absPartIdxTU)?1:(pcCU->isInter(absPartIdxTU)?2:4)))!=0) ? &sTemp : 0;
#endif

      invTransformNxN( rTu, compID, pResi, uiStride, pcCoeff, cQP DEBUG_STRING_PASS_INTO(psDebug) );

#if DEBUG_STRING
      if (psDebug != 0)
      {
        std::cout << (*psDebug);
      }
#endif
    }

    if (isChroma(compID) && (pcCU->getCrossComponentPredictionAlpha(absPartIdxTU, compID) != 0))
    {
      const Pel *piResiLuma = pResidual->getAddr( COMPONENT_Y );
      const Int  strideLuma = pResidual->getStride( COMPONENT_Y );
      const Int  tuWidth    = rTu.getRect( compID ).width;
      const Int  tuHeight   = rTu.getRect( compID ).height;

      if(pcCU->getCbf(absPartIdxTU, COMPONENT_Y, uiTrMode) != 0)
      {
        pResi = rpcResidual + uiAddr;
        const Pel *pResiLuma = piResiLuma + uiAddr;

        crossComponentPrediction( rTu, compID, pResiLuma, pResi, pResi, tuWidth, tuHeight, strideLuma, uiStride, uiStride, true );
      }
    }
  }
  else
  {
    TComTURecurse tuRecurseChild(rTu, false);
    do
    {
      invRecurTransformNxN( compID, pResidual, tuRecurseChild );
    } while (tuRecurseChild.nextSection(rTu));
  }
}

Void TComTrQuant::applyForwardRDPCM( TComTU& rTu, const ComponentID compID, Pel* pcResidual, const UInt uiStride, const QpParam& cQP, TCoeff* pcCoeff, TCoeff &uiAbsSum, const RDPCMMode mode )
{
  TComDataCU *pcCU=rTu.getCU();
  const UInt uiAbsPartIdx=rTu.GetAbsPartIdxTU();

  const Bool bLossless      = pcCU->getCUTransquantBypass( uiAbsPartIdx );
  const UInt uiWidth        = rTu.getRect(compID).width;
  const UInt uiHeight       = rTu.getRect(compID).height;
  const Bool rotateResidual = rTu.isNonTransformedResidualRotated(compID);
  const UInt uiSizeMinus1   = (uiWidth * uiHeight) - 1;

  UInt uiX = 0;
  UInt uiY = 0;

        UInt &majorAxis             = (mode == RDPCM_VER) ? uiX      : uiY;
        UInt &minorAxis             = (mode == RDPCM_VER) ? uiY      : uiX;
  const UInt  majorAxisLimit        = (mode == RDPCM_VER) ? uiWidth  : uiHeight;
  const UInt  minorAxisLimit        = (mode == RDPCM_VER) ? uiHeight : uiWidth;

  const Bool bUseHalfRoundingPoint  = (mode != RDPCM_OFF);

  uiAbsSum = 0;

  for ( majorAxis = 0; majorAxis < majorAxisLimit; majorAxis++ )
  {
    TCoeff accumulatorValue = 0; // 32-bit accumulator
    for ( minorAxis = 0; minorAxis < minorAxisLimit; minorAxis++ )
    {
      const UInt sampleIndex      = (uiY * uiWidth) + uiX;
      const UInt coefficientIndex = (rotateResidual ? (uiSizeMinus1-sampleIndex) : sampleIndex);
      const Pel  currentSample    = pcResidual[(uiY * uiStride) + uiX];
      const TCoeff encoderSideDelta = TCoeff(currentSample) - accumulatorValue;

      Pel reconstructedDelta;
      if ( bLossless )
      {
        pcCoeff[coefficientIndex] = encoderSideDelta;
        reconstructedDelta        = (Pel) encoderSideDelta;
      }
      else
      {
        transformSkipQuantOneSample(rTu, compID, encoderSideDelta, pcCoeff, coefficientIndex, cQP, bUseHalfRoundingPoint);
        invTrSkipDeQuantOneSample  (rTu, compID, pcCoeff[coefficientIndex], reconstructedDelta, cQP, coefficientIndex);
      }

      uiAbsSum += abs(pcCoeff[coefficientIndex]);

      if (mode != RDPCM_OFF)
      {
        accumulatorValue += reconstructedDelta;
      }
    }
  }
}

Void TComTrQuant::rdpcmNxN   ( TComTU& rTu, const ComponentID compID, Pel* pcResidual, const UInt uiStride, const QpParam& cQP, TCoeff* pcCoeff, TCoeff &uiAbsSum, RDPCMMode& rdpcmMode )
{
  TComDataCU *pcCU=rTu.getCU();
  const UInt uiAbsPartIdx=rTu.GetAbsPartIdxTU();

  if (!pcCU->isRDPCMEnabled(uiAbsPartIdx) || ((pcCU->getTransformSkip(uiAbsPartIdx, compID) == 0) && !pcCU->getCUTransquantBypass(uiAbsPartIdx)))
  {
    rdpcmMode = RDPCM_OFF;
  }
  else if ( pcCU->isIntra( uiAbsPartIdx ) )
  {
    const ChromaFormat chFmt = pcCU->getPic()->getPicYuvOrg()->getChromaFormat();
    const ChannelType chType = toChannelType(compID);
    const UInt uiChPredMode  = pcCU->getIntraDir( chType, uiAbsPartIdx );
    const TComSPS *sps=pcCU->getSlice()->getSPS();
    const UInt partsPerMinCU = 1<<(2*(sps->getMaxTotalCUDepth() - sps->getLog2DiffMaxMinCodingBlockSize()));
    const UInt uiChCodedMode = (uiChPredMode==DM_CHROMA_IDX && isChroma(compID)) ? pcCU->getIntraDir(CHANNEL_TYPE_LUMA, getChromasCorrespondingPULumaIdx(uiAbsPartIdx, chFmt, partsPerMinCU)) : uiChPredMode;
    const UInt uiChFinalMode = ((chFmt == CHROMA_422)       && isChroma(compID)) ? g_chroma422IntraAngleMappingTable[uiChCodedMode] : uiChCodedMode;

    if (uiChFinalMode == VER_IDX || uiChFinalMode == HOR_IDX)
    {
      rdpcmMode = (uiChFinalMode == VER_IDX) ? RDPCM_VER : RDPCM_HOR;
      applyForwardRDPCM( rTu, compID, pcResidual, uiStride, cQP, pcCoeff, uiAbsSum, rdpcmMode );
    }
    else
    {
      rdpcmMode = RDPCM_OFF;
    }
  }
  else // not intra, need to select the best mode
  {
    const UInt uiWidth  = rTu.getRect(compID).width;
    const UInt uiHeight = rTu.getRect(compID).height;

    RDPCMMode bestMode   = NUMBER_OF_RDPCM_MODES;
    TCoeff    bestAbsSum = std::numeric_limits<TCoeff>::max();
    TCoeff    bestCoefficients[MAX_TU_SIZE * MAX_TU_SIZE];

    for (UInt modeIndex = 0; modeIndex < NUMBER_OF_RDPCM_MODES; modeIndex++)
    {
      const RDPCMMode mode = RDPCMMode(modeIndex);

      TCoeff currAbsSum = 0;

      applyForwardRDPCM( rTu, compID, pcResidual, uiStride, cQP, pcCoeff, currAbsSum, mode );

      if (currAbsSum < bestAbsSum)
      {
        bestMode   = mode;
        bestAbsSum = currAbsSum;
        if (mode != RDPCM_OFF)
        {
          memcpy(bestCoefficients, pcCoeff, (uiWidth * uiHeight * sizeof(TCoeff)));
        }
      }
    }

    rdpcmMode = bestMode;
    uiAbsSum  = bestAbsSum;

    if (rdpcmMode != RDPCM_OFF) //the TU is re-transformed and quantised if DPCM_OFF is returned, so there is no need to preserve it here
    {
      memcpy(pcCoeff, bestCoefficients, (uiWidth * uiHeight * sizeof(TCoeff)));
    }
  }

  pcCU->setExplicitRdpcmModePartRange(rdpcmMode, compID, uiAbsPartIdx, rTu.GetAbsPartIdxNumParts(compID));
}

Void TComTrQuant::invRdpcmNxN( TComTU& rTu, const ComponentID compID, Pel* pcResidual, const UInt uiStride )
{
  TComDataCU *pcCU=rTu.getCU();
  const UInt uiAbsPartIdx=rTu.GetAbsPartIdxTU();

  if (pcCU->isRDPCMEnabled( uiAbsPartIdx ) && ((pcCU->getTransformSkip(uiAbsPartIdx, compID ) != 0) || pcCU->getCUTransquantBypass(uiAbsPartIdx)))
  {
    const UInt uiWidth  = rTu.getRect(compID).width;
    const UInt uiHeight = rTu.getRect(compID).height;

    RDPCMMode rdpcmMode = RDPCM_OFF;

    if ( pcCU->isIntra( uiAbsPartIdx ) )
    {
      const ChromaFormat chFmt = pcCU->getPic()->getPicYuvRec()->getChromaFormat();
      const ChannelType chType = toChannelType(compID);
      const UInt uiChPredMode  = pcCU->getIntraDir( chType, uiAbsPartIdx );
      const TComSPS *sps=pcCU->getSlice()->getSPS();
      const UInt partsPerMinCU = 1<<(2*(sps->getMaxTotalCUDepth() - sps->getLog2DiffMaxMinCodingBlockSize()));
      const UInt uiChCodedMode = (uiChPredMode==DM_CHROMA_IDX && isChroma(compID)) ? pcCU->getIntraDir(CHANNEL_TYPE_LUMA, getChromasCorrespondingPULumaIdx(uiAbsPartIdx, chFmt, partsPerMinCU)) : uiChPredMode;
      const UInt uiChFinalMode = ((chFmt == CHROMA_422)       && isChroma(compID)) ? g_chroma422IntraAngleMappingTable[uiChCodedMode] : uiChCodedMode;

      if (uiChFinalMode == VER_IDX || uiChFinalMode == HOR_IDX)
      {
        rdpcmMode = (uiChFinalMode == VER_IDX) ? RDPCM_VER : RDPCM_HOR;
      }
    }
    else  // not intra case
    {
      rdpcmMode = RDPCMMode(pcCU->getExplicitRdpcmMode( compID, uiAbsPartIdx ));
    }

    const TCoeff pelMin=(TCoeff) std::numeric_limits<Pel>::min();
    const TCoeff pelMax=(TCoeff) std::numeric_limits<Pel>::max();
    if (rdpcmMode == RDPCM_VER)
    {
      for( UInt uiX = 0; uiX < uiWidth; uiX++ )
      {
        Pel *pcCurResidual = pcResidual+uiX;
        TCoeff accumulator = *pcCurResidual; // 32-bit accumulator
        pcCurResidual+=uiStride;
        for( UInt uiY = 1; uiY < uiHeight; uiY++, pcCurResidual+=uiStride )
        {
          accumulator += *(pcCurResidual);
          *pcCurResidual = (Pel)Clip3<TCoeff>(pelMin, pelMax, accumulator);
        }
      }
    }
    else if (rdpcmMode == RDPCM_HOR)
    {
      for( UInt uiY = 0; uiY < uiHeight; uiY++ )
      {
        Pel *pcCurResidual = pcResidual+uiY*uiStride;
        TCoeff accumulator = *pcCurResidual;
        pcCurResidual++;
        for( UInt uiX = 1; uiX < uiWidth; uiX++, pcCurResidual++ )
        {
          accumulator += *(pcCurResidual);
          *pcCurResidual = (Pel)Clip3<TCoeff>(pelMin, pelMax, accumulator);
        }
      }
    }
  }
}

// ------------------------------------------------------------------------------------------------
// Logical transform
// ------------------------------------------------------------------------------------------------

/** Wrapper function between HM interface and core NxN forward transform (2D)
 *  \param channelBitDepth bit depth of channel
 *  \param useDST
 *  \param piBlkResi input data (residual)
 *  \param uiStride stride of input residual data
 *  \param psCoeff output data (transform coefficients)
 *  \param iWidth transform width
 *  \param iHeight transform height
 *  \param maxLog2TrDynamicRange
 */
Void TComTrQuant::xT( const Int channelBitDepth, Bool useDST, Pel* piBlkResi, UInt uiStride, TCoeff* psCoeff, Int iWidth, Int iHeight, const Int maxLog2TrDynamicRange )
{
#if MATRIX_MULT
  if( iWidth == iHeight)
  {
    xTr(channelBitDepth, piBlkResi, psCoeff, uiStride, (UInt)iWidth, useDST, maxLog2TrDynamicRange);
    return;
  }
#endif

  TCoeff block[ MAX_TU_SIZE * MAX_TU_SIZE ];
  TCoeff coeff[ MAX_TU_SIZE * MAX_TU_SIZE ];

  for (Int y = 0; y < iHeight; y++)
  {
    for (Int x = 0; x < iWidth; x++)
    {
      block[(y * iWidth) + x] = piBlkResi[(y * uiStride) + x];
    }
  }

  xTrMxN( channelBitDepth, block, coeff, iWidth, iHeight, useDST, maxLog2TrDynamicRange );

  memcpy(psCoeff, coeff, (iWidth * iHeight * sizeof(TCoeff)));
}

/** Wrapper function between HM interface and core NxN inverse transform (2D)
 *  \param channelBitDepth bit depth of channel
 *  \param useDST
 *  \param plCoef input data (transform coefficients)
 *  \param pResidual output data (residual)
 *  \param uiStride stride of input residual data
 *  \param iWidth transform width
 *  \param iHeight transform height
 *  \param maxLog2TrDynamicRange
 */
Void TComTrQuant::xIT( const Int channelBitDepth, Bool useDST, TCoeff* plCoef, Pel* pResidual, UInt uiStride, Int iWidth, Int iHeight, const Int maxLog2TrDynamicRange )
{
#if MATRIX_MULT
  if( iWidth == iHeight )
  {
    xITr(channelBitDepth, plCoef, pResidual, uiStride, (UInt)iWidth, useDST, maxLog2TrDynamicRange);
    return;
  }
#endif

  TCoeff block[ MAX_TU_SIZE * MAX_TU_SIZE ];
  TCoeff coeff[ MAX_TU_SIZE * MAX_TU_SIZE ];

  memcpy(coeff, plCoef, (iWidth * iHeight * sizeof(TCoeff)));

  xITrMxN( channelBitDepth, coeff, block, iWidth, iHeight, useDST, maxLog2TrDynamicRange );

  for (Int y = 0; y < iHeight; y++)
  {
    for (Int x = 0; x < iWidth; x++)
    {
      pResidual[(y * uiStride) + x] = Pel(block[(y * iWidth) + x]);
    }
  }
}

/** Wrapper function between HM interface and core 4x4 transform skipping
 *  \param piBlkResi input data (residual)
 *  \param uiStride stride of input residual data
 *  \param psCoeff output data (transform coefficients)
 *  \param rTu reference to transform data
 *  \param component colour component
 */
Void TComTrQuant::xTransformSkip( Pel* piBlkResi, UInt uiStride, TCoeff* psCoeff, TComTU &rTu, const ComponentID component )
{
  const TComRectangle &rect = rTu.getRect(component);
  const Int width           = rect.width;
  const Int height          = rect.height;
  const Int maxLog2TrDynamicRange = rTu.getCU()->getSlice()->getSPS()->getMaxLog2TrDynamicRange(toChannelType(component));
  const Int channelBitDepth = rTu.getCU()->getSlice()->getSPS()->getBitDepth(toChannelType(component));

  Int iTransformShift = getTransformShift(channelBitDepth, rTu.GetEquivalentLog2TrSize(component), maxLog2TrDynamicRange);
  if (rTu.getCU()->getSlice()->getSPS()->getSpsRangeExtension().getExtendedPrecisionProcessingFlag())
  {
    iTransformShift = std::max<Int>(0, iTransformShift);
  }

  const Bool rotateResidual = rTu.isNonTransformedResidualRotated(component);
  const UInt uiSizeMinus1   = (width * height) - 1;

  if (iTransformShift >= 0)
  {
    for (UInt y = 0, coefficientIndex = 0; y < height; y++)
    {
      for (UInt x = 0; x < width; x++, coefficientIndex++)
      {
        psCoeff[rotateResidual ? (uiSizeMinus1 - coefficientIndex) : coefficientIndex] = TCoeff(piBlkResi[(y * uiStride) + x]) << iTransformShift;
      }
    }
  }
  else //for very high bit depths
  {
    iTransformShift = -iTransformShift;
    const TCoeff offset = 1 << (iTransformShift - 1);

    for (UInt y = 0, coefficientIndex = 0; y < height; y++)
    {
      for (UInt x = 0; x < width; x++, coefficientIndex++)
      {
        psCoeff[rotateResidual ? (uiSizeMinus1 - coefficientIndex) : coefficientIndex] = (TCoeff(piBlkResi[(y * uiStride) + x]) + offset) >> iTransformShift;
      }
    }
  }
}

/** Wrapper function between HM interface and core NxN transform skipping
 *  \param plCoef input data (coefficients)
 *  \param pResidual output data (residual)
 *  \param uiStride stride of input residual data
 *  \param rTu reference to transform data
 *  \param component colour component ID
 */
Void TComTrQuant::xITransformSkip( TCoeff* plCoef, Pel* pResidual, UInt uiStride, TComTU &rTu, const ComponentID component )
{
  const TComRectangle &rect = rTu.getRect(component);
  const Int width           = rect.width;
  const Int height          = rect.height;
  const Int maxLog2TrDynamicRange = rTu.getCU()->getSlice()->getSPS()->getMaxLog2TrDynamicRange(toChannelType(component));
#if O0043_BEST_EFFORT_DECODING
  const Int channelBitDepth = rTu.getCU()->getSlice()->getSPS()->getStreamBitDepth(toChannelType(component));
#else
  const Int channelBitDepth = rTu.getCU()->getSlice()->getSPS()->getBitDepth(toChannelType(component));
#endif

  Int iTransformShift = getTransformShift(channelBitDepth, rTu.GetEquivalentLog2TrSize(component), maxLog2TrDynamicRange);
  if (rTu.getCU()->getSlice()->getSPS()->getSpsRangeExtension().getExtendedPrecisionProcessingFlag())
  {
    iTransformShift = std::max<Int>(0, iTransformShift);
  }

  const Bool rotateResidual = rTu.isNonTransformedResidualRotated(component);
  const UInt uiSizeMinus1   = (width * height) - 1;

  if (iTransformShift >= 0)
  {
    const TCoeff offset = iTransformShift==0 ? 0 : (1 << (iTransformShift - 1));

    for (UInt y = 0, coefficientIndex = 0; y < height; y++)
    {
      for (UInt x = 0; x < width; x++, coefficientIndex++)
      {
        pResidual[(y * uiStride) + x] =  Pel((plCoef[rotateResidual ? (uiSizeMinus1 - coefficientIndex) : coefficientIndex] + offset) >> iTransformShift);
      }
    }
  }
  else //for very high bit depths
  {
    iTransformShift = -iTransformShift;

    for (UInt y = 0, coefficientIndex = 0; y < height; y++)
    {
      for (UInt x = 0; x < width; x++, coefficientIndex++)
      {
        pResidual[(y * uiStride) + x] = Pel(plCoef[rotateResidual ? (uiSizeMinus1 - coefficientIndex) : coefficientIndex] << iTransformShift);
      }
    }
  }
}

/** RDOQ with CABAC
 * \param rTu reference to transform data
 * \param plSrcCoeff pointer to input buffer
 * \param piDstCoeff reference to pointer to output buffer
 * \param piArlDstCoeff
 * \param uiAbsSum reference to absolute sum of quantized transform coefficient
 * \param compID colour component ID
 * \param cQP reference to quantization parameters

 * Rate distortion optimized quantization for entropy
 * coding engines using probability models like CABAC
 */
Void TComTrQuant::xRateDistOptQuant                 (       TComTU       &rTu,
                                                            TCoeff      * plSrcCoeff,
                                                            TCoeff      * piDstCoeff,
#if ADAPTIVE_QP_SELECTION
                                                            TCoeff      * piArlDstCoeff,
#endif
                                                            TCoeff       &uiAbsSum,
                                                      const ComponentID   compID,
                                                      const QpParam      &cQP  )
{
  const TComRectangle  & rect             = rTu.getRect(compID);
  const UInt             uiWidth          = rect.width;
  const UInt             uiHeight         = rect.height;
        TComDataCU    *  pcCU             = rTu.getCU();
  const UInt             uiAbsPartIdx     = rTu.GetAbsPartIdxTU();
  const ChannelType      channelType      = toChannelType(compID);
  const UInt             uiLog2TrSize     = rTu.GetEquivalentLog2TrSize(compID);

  const Bool             extendedPrecision = pcCU->getSlice()->getSPS()->getSpsRangeExtension().getExtendedPrecisionProcessingFlag();
  const Int              maxLog2TrDynamicRange = pcCU->getSlice()->getSPS()->getMaxLog2TrDynamicRange(toChannelType(compID));
  const Int              channelBitDepth = rTu.getCU()->getSlice()->getSPS()->getBitDepth(channelType);

  /* for 422 chroma blocks, the effective scaling applied during transformation is not a power of 2, hence it cannot be
   * implemented as a bit-shift (the quantised result will be sqrt(2) * larger than required). Alternatively, adjust the
   * uiLog2TrSize applied in iTransformShift, such that the result is 1/sqrt(2) the required result (i.e. smaller)
   * Then a QP+3 (sqrt(2)) or QP-3 (1/sqrt(2)) method could be used to get the required result
   */

  // Represents scaling through forward transform
  Int iTransformShift = getTransformShift(channelBitDepth, uiLog2TrSize, maxLog2TrDynamicRange);
  if ((pcCU->getTransformSkip(uiAbsPartIdx, compID) != 0) && extendedPrecision)
  {
    iTransformShift = std::max<Int>(0, iTransformShift);
  }

  const Bool bUseGolombRiceParameterAdaptation = pcCU->getSlice()->getSPS()->getSpsRangeExtension().getPersistentRiceAdaptationEnabledFlag();
  const UInt initialGolombRiceParameter        = m_pcEstBitsSbac->golombRiceAdaptationStatistics[rTu.getGolombRiceStatisticsIndex(compID)] / RExt__GOLOMB_RICE_INCREMENT_DIVISOR;
        UInt uiGoRiceParam                     = initialGolombRiceParameter;
  Double     d64BlockUncodedCost               = 0;
  const UInt uiLog2BlockWidth                  = g_aucConvertToBit[ uiWidth  ] + 2;
  const UInt uiLog2BlockHeight                 = g_aucConvertToBit[ uiHeight ] + 2;
  const UInt uiMaxNumCoeff                     = uiWidth * uiHeight;
  assert(compID<MAX_NUM_COMPONENT);

  Int scalingListType = getScalingListType(pcCU->getPredictionMode(uiAbsPartIdx), compID);
  assert(scalingListType < SCALING_LIST_NUM);

#if ADAPTIVE_QP_SELECTION
  memset(piArlDstCoeff, 0, sizeof(TCoeff) *  uiMaxNumCoeff);
#endif

  Double pdCostCoeff [ MAX_TU_SIZE * MAX_TU_SIZE ];
  Double pdCostSig   [ MAX_TU_SIZE * MAX_TU_SIZE ];
  Double pdCostCoeff0[ MAX_TU_SIZE * MAX_TU_SIZE ];
  memset( pdCostCoeff, 0, sizeof(Double) *  uiMaxNumCoeff );
  memset( pdCostSig,   0, sizeof(Double) *  uiMaxNumCoeff );
  Int rateIncUp   [ MAX_TU_SIZE * MAX_TU_SIZE ];
  Int rateIncDown [ MAX_TU_SIZE * MAX_TU_SIZE ];
  Int sigRateDelta[ MAX_TU_SIZE * MAX_TU_SIZE ];
  TCoeff deltaU   [ MAX_TU_SIZE * MAX_TU_SIZE ];
  memset( rateIncUp,    0, sizeof(Int   ) *  uiMaxNumCoeff );
  memset( rateIncDown,  0, sizeof(Int   ) *  uiMaxNumCoeff );
  memset( sigRateDelta, 0, sizeof(Int   ) *  uiMaxNumCoeff );
  memset( deltaU,       0, sizeof(TCoeff) *  uiMaxNumCoeff );

  const Int iQBits = QUANT_SHIFT + cQP.per + iTransformShift;                   // Right shift of non-RDOQ quantizer;  level = (coeff*uiQ + offset)>>q_bits
  const Double *const pdErrScale = getErrScaleCoeff(scalingListType, (uiLog2TrSize-2), cQP.rem);
  const Int    *const piQCoef    = getQuantCoeff(scalingListType, cQP.rem, (uiLog2TrSize-2));

  const Bool   enableScalingLists             = getUseScalingList(uiWidth, uiHeight, (pcCU->getTransformSkip(uiAbsPartIdx, compID) != 0));
  const Int    defaultQuantisationCoefficient = g_quantScales[cQP.rem];
  const Double defaultErrorScale              = getErrScaleCoeffNoScalingList(scalingListType, (uiLog2TrSize-2), cQP.rem);

  const TCoeff entropyCodingMinimum = -(1 << maxLog2TrDynamicRange);
  const TCoeff entropyCodingMaximum =  (1 << maxLog2TrDynamicRange) - 1;

#if ADAPTIVE_QP_SELECTION
  Int iQBitsC = iQBits - ARL_C_PRECISION;
  Int iAddC =  1 << (iQBitsC-1);
#endif

  TUEntropyCodingParameters codingParameters;
  getTUEntropyCodingParameters(codingParameters, rTu, compID);
  const UInt uiCGSize = (1 << MLS_CG_SIZE);

  Double pdCostCoeffGroupSig[ MLS_GRP_NUM ];
  UInt uiSigCoeffGroupFlag[ MLS_GRP_NUM ];
  Int iCGLastScanPos = -1;

  UInt    uiCtxSet            = 0;
  Int     c1                  = 1;
  Int     c2                  = 0;
  Double  d64BaseCost         = 0;
  Int     iLastScanPos        = -1;

  UInt    c1Idx     = 0;
  UInt    c2Idx     = 0;
  Int     baseLevel;

  memset( pdCostCoeffGroupSig,   0, sizeof(Double) * MLS_GRP_NUM );
  memset( uiSigCoeffGroupFlag,   0, sizeof(UInt) * MLS_GRP_NUM );

  UInt uiCGNum = uiWidth * uiHeight >> MLS_CG_SIZE;
  Int iScanPos;
  coeffGroupRDStats rdStats;

  const UInt significanceMapContextOffset = getSignificanceMapContextOffset(compID);

  for (Int iCGScanPos = uiCGNum-1; iCGScanPos >= 0; iCGScanPos--)
  {
    UInt uiCGBlkPos = codingParameters.scanCG[ iCGScanPos ];
    UInt uiCGPosY   = uiCGBlkPos / codingParameters.widthInGroups;
    UInt uiCGPosX   = uiCGBlkPos - (uiCGPosY * codingParameters.widthInGroups);

    memset( &rdStats, 0, sizeof (coeffGroupRDStats));

    const Int patternSigCtx = TComTrQuant::calcPatternSigCtx(uiSigCoeffGroupFlag, uiCGPosX, uiCGPosY, codingParameters.widthInGroups, codingParameters.heightInGroups);

    for (Int iScanPosinCG = uiCGSize-1; iScanPosinCG >= 0; iScanPosinCG--)
    {
      iScanPos = iCGScanPos*uiCGSize + iScanPosinCG;
      //===== quantization =====
      UInt    uiBlkPos          = codingParameters.scan[iScanPos];
      // set coeff

      const Int    quantisationCoefficient = (enableScalingLists) ? piQCoef   [uiBlkPos] : defaultQuantisationCoefficient;
      const Double errorScale              = (enableScalingLists) ? pdErrScale[uiBlkPos] : defaultErrorScale;

      const Int64  tmpLevel                = Int64(abs(plSrcCoeff[ uiBlkPos ])) * quantisationCoefficient;

      const Intermediate_Int lLevelDouble  = (Intermediate_Int)min<Int64>(tmpLevel, std::numeric_limits<Intermediate_Int>::max() - (Intermediate_Int(1) << (iQBits - 1)));

#if ADAPTIVE_QP_SELECTION
      if( m_bUseAdaptQpSelect )
      {
        piArlDstCoeff[uiBlkPos]   = (TCoeff)(( lLevelDouble + iAddC) >> iQBitsC );
      }
#endif
      const UInt uiMaxAbsLevel  = std::min<UInt>(UInt(entropyCodingMaximum), UInt((lLevelDouble + (Intermediate_Int(1) << (iQBits - 1))) >> iQBits));

      const Double dErr         = Double( lLevelDouble );
      pdCostCoeff0[ iScanPos ]  = dErr * dErr * errorScale;
      d64BlockUncodedCost      += pdCostCoeff0[ iScanPos ];
      piDstCoeff[ uiBlkPos ]    = uiMaxAbsLevel;

      if ( uiMaxAbsLevel > 0 && iLastScanPos < 0 )
      {
        iLastScanPos            = iScanPos;
        uiCtxSet                = getContextSetIndex(compID, (iScanPos >> MLS_CG_SIZE), 0);
        iCGLastScanPos          = iCGScanPos;
      }

      if ( iLastScanPos >= 0 )
      {
        //===== coefficient level estimation =====
        UInt  uiLevel;
        UInt  uiOneCtx         = (NUM_ONE_FLAG_CTX_PER_SET * uiCtxSet) + c1;
        UInt  uiAbsCtx         = (NUM_ABS_FLAG_CTX_PER_SET * uiCtxSet) + c2;

        if( iScanPos == iLastScanPos )
        {
          uiLevel              = xGetCodedLevel( pdCostCoeff[ iScanPos ], pdCostCoeff0[ iScanPos ], pdCostSig[ iScanPos ],
                                                  lLevelDouble, uiMaxAbsLevel, significanceMapContextOffset, uiOneCtx, uiAbsCtx, uiGoRiceParam,
                                                  c1Idx, c2Idx, iQBits, errorScale, 1, extendedPrecision, maxLog2TrDynamicRange
                                                  );
        }
        else
        {
          UShort uiCtxSig      = significanceMapContextOffset + getSigCtxInc( patternSigCtx, codingParameters, iScanPos, uiLog2BlockWidth, uiLog2BlockHeight, channelType );

          uiLevel              = xGetCodedLevel( pdCostCoeff[ iScanPos ], pdCostCoeff0[ iScanPos ], pdCostSig[ iScanPos ],
                                                  lLevelDouble, uiMaxAbsLevel, uiCtxSig, uiOneCtx, uiAbsCtx, uiGoRiceParam,
                                                  c1Idx, c2Idx, iQBits, errorScale, 0, extendedPrecision, maxLog2TrDynamicRange
                                                  );

          sigRateDelta[ uiBlkPos ] = m_pcEstBitsSbac->significantBits[ uiCtxSig ][ 1 ] - m_pcEstBitsSbac->significantBits[ uiCtxSig ][ 0 ];
        }

        deltaU[ uiBlkPos ]        = TCoeff((lLevelDouble - (Intermediate_Int(uiLevel) << iQBits)) >> (iQBits-8));

        if( uiLevel > 0 )
        {
          Int rateNow = xGetICRate( uiLevel, uiOneCtx, uiAbsCtx, uiGoRiceParam, c1Idx, c2Idx, extendedPrecision, maxLog2TrDynamicRange );
          rateIncUp   [ uiBlkPos ] = xGetICRate( uiLevel+1, uiOneCtx, uiAbsCtx, uiGoRiceParam, c1Idx, c2Idx, extendedPrecision, maxLog2TrDynamicRange ) - rateNow;
          rateIncDown [ uiBlkPos ] = xGetICRate( uiLevel-1, uiOneCtx, uiAbsCtx, uiGoRiceParam, c1Idx, c2Idx, extendedPrecision, maxLog2TrDynamicRange ) - rateNow;
        }
        else // uiLevel == 0
        {
          rateIncUp   [ uiBlkPos ] = m_pcEstBitsSbac->m_greaterOneBits[ uiOneCtx ][ 0 ];
        }
        piDstCoeff[ uiBlkPos ] = uiLevel;
        d64BaseCost           += pdCostCoeff [ iScanPos ];

        baseLevel = (c1Idx < C1FLAG_NUMBER) ? (2 + (c2Idx < C2FLAG_NUMBER)) : 1;
        if( uiLevel >= baseLevel )
        {
          if (uiLevel > 3*(1<<uiGoRiceParam))
          {
            uiGoRiceParam = bUseGolombRiceParameterAdaptation ? (uiGoRiceParam + 1) : (std::min<UInt>((uiGoRiceParam + 1), 4));
          }
        }
        if ( uiLevel >= 1)
        {
          c1Idx ++;
        }

        //===== update bin model =====
        if( uiLevel > 1 )
        {
          c1 = 0;
          c2 += (c2 < 2);
          c2Idx ++;
        }
        else if( (c1 < 3) && (c1 > 0) && uiLevel)
        {
          c1++;
        }

        //===== context set update =====
        if( ( iScanPos % uiCGSize == 0 ) && ( iScanPos > 0 ) )
        {
          uiCtxSet          = getContextSetIndex(compID, ((iScanPos - 1) >> MLS_CG_SIZE), (c1 == 0)); //(iScanPos - 1) because we do this **before** entering the final group
          c1                = 1;
          c2                = 0;
          c1Idx             = 0;
          c2Idx             = 0;
          uiGoRiceParam     = initialGolombRiceParameter;
        }
      }
      else
      {
        d64BaseCost    += pdCostCoeff0[ iScanPos ];
      }
      rdStats.d64SigCost += pdCostSig[ iScanPos ];
      if (iScanPosinCG == 0 )
      {
        rdStats.d64SigCost_0 = pdCostSig[ iScanPos ];
      }
      if (piDstCoeff[ uiBlkPos ] )
      {
        uiSigCoeffGroupFlag[ uiCGBlkPos ] = 1;
        rdStats.d64CodedLevelandDist += pdCostCoeff[ iScanPos ] - pdCostSig[ iScanPos ];
        rdStats.d64UncodedDist += pdCostCoeff0[ iScanPos ];
        if ( iScanPosinCG != 0 )
        {
          rdStats.iNNZbeforePos0++;
        }
      }
    } //end for (iScanPosinCG)

    if (iCGLastScanPos >= 0)
    {
      if( iCGScanPos )
      {
        if (uiSigCoeffGroupFlag[ uiCGBlkPos ] == 0)
        {
          UInt  uiCtxSig = getSigCoeffGroupCtxInc( uiSigCoeffGroupFlag, uiCGPosX, uiCGPosY, codingParameters.widthInGroups, codingParameters.heightInGroups );
          d64BaseCost += xGetRateSigCoeffGroup(0, uiCtxSig) - rdStats.d64SigCost;;
          pdCostCoeffGroupSig[ iCGScanPos ] = xGetRateSigCoeffGroup(0, uiCtxSig);
        }
        else
        {
          if (iCGScanPos < iCGLastScanPos) //skip the last coefficient group, which will be handled together with last position below.
          {
            if ( rdStats.iNNZbeforePos0 == 0 )
            {
              d64BaseCost -= rdStats.d64SigCost_0;
              rdStats.d64SigCost -= rdStats.d64SigCost_0;
            }
            // rd-cost if SigCoeffGroupFlag = 0, initialization
            Double d64CostZeroCG = d64BaseCost;

            // add SigCoeffGroupFlag cost to total cost
            UInt  uiCtxSig = getSigCoeffGroupCtxInc( uiSigCoeffGroupFlag, uiCGPosX, uiCGPosY, codingParameters.widthInGroups, codingParameters.heightInGroups );

            if (iCGScanPos < iCGLastScanPos)
            {
              d64BaseCost  += xGetRateSigCoeffGroup(1, uiCtxSig);
              d64CostZeroCG += xGetRateSigCoeffGroup(0, uiCtxSig);
              pdCostCoeffGroupSig[ iCGScanPos ] = xGetRateSigCoeffGroup(1, uiCtxSig);
            }

            // try to convert the current coeff group from non-zero to all-zero
            d64CostZeroCG += rdStats.d64UncodedDist;  // distortion for resetting non-zero levels to zero levels
            d64CostZeroCG -= rdStats.d64CodedLevelandDist;   // distortion and level cost for keeping all non-zero levels
            d64CostZeroCG -= rdStats.d64SigCost;     // sig cost for all coeffs, including zero levels and non-zerl levels

            // if we can save cost, change this block to all-zero block
            if ( d64CostZeroCG < d64BaseCost )
            {
              uiSigCoeffGroupFlag[ uiCGBlkPos ] = 0;
              d64BaseCost = d64CostZeroCG;
              if (iCGScanPos < iCGLastScanPos)
              {
                pdCostCoeffGroupSig[ iCGScanPos ] = xGetRateSigCoeffGroup(0, uiCtxSig);
              }
              // reset coeffs to 0 in this block
              for (Int iScanPosinCG = uiCGSize-1; iScanPosinCG >= 0; iScanPosinCG--)
              {
                iScanPos      = iCGScanPos*uiCGSize + iScanPosinCG;
                UInt uiBlkPos = codingParameters.scan[ iScanPos ];

                if (piDstCoeff[ uiBlkPos ])
                {
                  piDstCoeff [ uiBlkPos ] = 0;
                  pdCostCoeff[ iScanPos ] = pdCostCoeff0[ iScanPos ];
                  pdCostSig  [ iScanPos ] = 0;
                }
              }
            } // end if ( d64CostAllZeros < d64BaseCost )
          }
        } // end if if (uiSigCoeffGroupFlag[ uiCGBlkPos ] == 0)
      }
      else
      {
        uiSigCoeffGroupFlag[ uiCGBlkPos ] = 1;
      }
    }
  } //end for (iCGScanPos)

  //===== estimate last position =====
  if ( iLastScanPos < 0 )
  {
    return;
  }

  Double  d64BestCost         = 0;
  Int     ui16CtxCbf          = 0;
  Int     iBestLastIdxP1      = 0;
  if( !pcCU->isIntra( uiAbsPartIdx ) && isLuma(compID) && pcCU->getTransformIdx( uiAbsPartIdx ) == 0 )
  {
    ui16CtxCbf   = 0;
    d64BestCost  = d64BlockUncodedCost + xGetICost( m_pcEstBitsSbac->blockRootCbpBits[ ui16CtxCbf ][ 0 ] );
    d64BaseCost += xGetICost( m_pcEstBitsSbac->blockRootCbpBits[ ui16CtxCbf ][ 1 ] );
  }
  else
  {
    ui16CtxCbf   = pcCU->getCtxQtCbf( rTu, channelType );
    ui16CtxCbf  += getCBFContextOffset(compID);
    d64BestCost  = d64BlockUncodedCost + xGetICost( m_pcEstBitsSbac->blockCbpBits[ ui16CtxCbf ][ 0 ] );
    d64BaseCost += xGetICost( m_pcEstBitsSbac->blockCbpBits[ ui16CtxCbf ][ 1 ] );
  }


  Bool bFoundLast = false;
  for (Int iCGScanPos = iCGLastScanPos; iCGScanPos >= 0; iCGScanPos--)
  {
    UInt uiCGBlkPos = codingParameters.scanCG[ iCGScanPos ];

    d64BaseCost -= pdCostCoeffGroupSig [ iCGScanPos ];
    if (uiSigCoeffGroupFlag[ uiCGBlkPos ])
    {
      for (Int iScanPosinCG = uiCGSize-1; iScanPosinCG >= 0; iScanPosinCG--)
      {
        iScanPos = iCGScanPos*uiCGSize + iScanPosinCG;

        if (iScanPos > iLastScanPos)
        {
          continue;
        }
        UInt   uiBlkPos     = codingParameters.scan[iScanPos];

        if( piDstCoeff[ uiBlkPos ] )
        {
          UInt   uiPosY       = uiBlkPos >> uiLog2BlockWidth;
          UInt   uiPosX       = uiBlkPos - ( uiPosY << uiLog2BlockWidth );

          Double d64CostLast= codingParameters.scanType == SCAN_VER ? xGetRateLast( uiPosY, uiPosX, compID ) : xGetRateLast( uiPosX, uiPosY, compID );
          Double totalCost = d64BaseCost + d64CostLast - pdCostSig[ iScanPos ];

          if( totalCost < d64BestCost )
          {
            iBestLastIdxP1  = iScanPos + 1;
            d64BestCost     = totalCost;
          }
          if( piDstCoeff[ uiBlkPos ] > 1 )
          {
            bFoundLast = true;
            break;
          }
          d64BaseCost      -= pdCostCoeff[ iScanPos ];
          d64BaseCost      += pdCostCoeff0[ iScanPos ];
        }
        else
        {
          d64BaseCost      -= pdCostSig[ iScanPos ];
        }
      } //end for
      if (bFoundLast)
      {
        break;
      }
    } // end if (uiSigCoeffGroupFlag[ uiCGBlkPos ])
  } // end for


  for ( Int scanPos = 0; scanPos < iBestLastIdxP1; scanPos++ )
  {
    Int blkPos = codingParameters.scan[ scanPos ];
    TCoeff level = piDstCoeff[ blkPos ];
    uiAbsSum += level;
    piDstCoeff[ blkPos ] = ( plSrcCoeff[ blkPos ] < 0 ) ? -level : level;
  }

  //===== clean uncoded coefficients =====
  for ( Int scanPos = iBestLastIdxP1; scanPos <= iLastScanPos; scanPos++ )
  {
    piDstCoeff[ codingParameters.scan[ scanPos ] ] = 0;
  }


  if( pcCU->getSlice()->getPPS()->getSignDataHidingEnabledFlag() && uiAbsSum>=2)
  {
    const Double inverseQuantScale = Double(g_invQuantScales[cQP.rem]);
    Int64 rdFactor = (Int64)(inverseQuantScale * inverseQuantScale * (1 << (2 * cQP.per))
                             / m_dLambda / 16 / (1 << (2 * DISTORTION_PRECISION_ADJUSTMENT(channelBitDepth - 8)))
                             + 0.5);

    Int lastCG = -1;
    Int absSum = 0 ;
    Int n ;

    for( Int subSet = (uiWidth*uiHeight-1) >> MLS_CG_SIZE; subSet >= 0; subSet-- )
    {
      Int  subPos     = subSet << MLS_CG_SIZE;
      Int  firstNZPosInCG=uiCGSize , lastNZPosInCG=-1 ;
      absSum = 0 ;

      for(n = uiCGSize-1; n >= 0; --n )
      {
        if( piDstCoeff[ codingParameters.scan[ n + subPos ]] )
        {
          lastNZPosInCG = n;
          break;
        }
      }

      for(n = 0; n <uiCGSize; n++ )
      {
        if( piDstCoeff[ codingParameters.scan[ n + subPos ]] )
        {
          firstNZPosInCG = n;
          break;
        }
      }

      for(n = firstNZPosInCG; n <=lastNZPosInCG; n++ )
      {
        absSum += Int(piDstCoeff[ codingParameters.scan[ n + subPos ]]);
      }

      if(lastNZPosInCG>=0 && lastCG==-1)
      {
        lastCG = 1;
      }

      if( lastNZPosInCG-firstNZPosInCG>=SBH_THRESHOLD )
      {
        UInt signbit = (piDstCoeff[codingParameters.scan[subPos+firstNZPosInCG]]>0?0:1);
        if( signbit!=(absSum&0x1) )  // hide but need tune
        {
          // calculate the cost
          Int64 minCostInc = std::numeric_limits<Int64>::max(), curCost = std::numeric_limits<Int64>::max();
          Int minPos = -1, finalChange = 0, curChange = 0;

          for( n = (lastCG==1?lastNZPosInCG:uiCGSize-1) ; n >= 0; --n )
          {
            UInt uiBlkPos   = codingParameters.scan[ n + subPos ];
            if(piDstCoeff[ uiBlkPos ] != 0 )
            {
              Int64 costUp   = rdFactor * ( - deltaU[uiBlkPos] ) + rateIncUp[uiBlkPos];
              Int64 costDown = rdFactor * (   deltaU[uiBlkPos] ) + rateIncDown[uiBlkPos]
                               -   ((abs(piDstCoeff[uiBlkPos]) == 1) ? sigRateDelta[uiBlkPos] : 0);

              if(lastCG==1 && lastNZPosInCG==n && abs(piDstCoeff[uiBlkPos])==1)
              {
                costDown -= (4<<15);
              }

              if(costUp<costDown)
              {
                curCost = costUp;
                curChange =  1;
              }
              else
              {
                curChange = -1;
                if(n==firstNZPosInCG && abs(piDstCoeff[uiBlkPos])==1)
                {
                  curCost = std::numeric_limits<Int64>::max();
                }
                else
                {
                  curCost = costDown;
                }
              }
            }
            else
            {
              curCost = rdFactor * ( - (abs(deltaU[uiBlkPos])) ) + (1<<15) + rateIncUp[uiBlkPos] + sigRateDelta[uiBlkPos] ;
              curChange = 1 ;

              if(n<firstNZPosInCG)
              {
                UInt thissignbit = (plSrcCoeff[uiBlkPos]>=0?0:1);
                if(thissignbit != signbit )
                {
                  curCost = std::numeric_limits<Int64>::max();
                }
              }
            }

            if( curCost<minCostInc)
            {
              minCostInc = curCost;
              finalChange = curChange;
              minPos = uiBlkPos;
            }
          }

          if(piDstCoeff[minPos] == entropyCodingMaximum || piDstCoeff[minPos] == entropyCodingMinimum)
          {
            finalChange = -1;
          }

          if(plSrcCoeff[minPos]>=0)
          {
            piDstCoeff[minPos] += finalChange ;
          }
          else
          {
            piDstCoeff[minPos] -= finalChange ;
          }
        }
      }

      if(lastCG==1)
      {
        lastCG=0 ;
      }
    }
  }
}


/** Pattern decision for context derivation process of significant_coeff_flag
 * \param sigCoeffGroupFlag pointer to prior coded significant coeff group
 * \param uiCGPosX column of current coefficient group
 * \param uiCGPosY row of current coefficient group
 * \param widthInGroups width of the block
 * \param heightInGroups height of the block
 * \returns pattern for current coefficient group
 */
Int  TComTrQuant::calcPatternSigCtx( const UInt* sigCoeffGroupFlag, UInt uiCGPosX, UInt uiCGPosY, UInt widthInGroups, UInt heightInGroups )
{
  if ((widthInGroups <= 1) && (heightInGroups <= 1))
  {
    return 0;
  }

  const Bool rightAvailable = uiCGPosX < (widthInGroups  - 1);
  const Bool belowAvailable = uiCGPosY < (heightInGroups - 1);

  UInt sigRight = 0;
  UInt sigLower = 0;

  if (rightAvailable)
  {
    sigRight = ((sigCoeffGroupFlag[ (uiCGPosY * widthInGroups) + uiCGPosX + 1 ] != 0) ? 1 : 0);
  }
  if (belowAvailable)
  {
    sigLower = ((sigCoeffGroupFlag[ (uiCGPosY + 1) * widthInGroups + uiCGPosX ] != 0) ? 1 : 0);
  }

  return sigRight + (sigLower << 1);
}


/** Context derivation process of coeff_abs_significant_flag
 * \param patternSigCtx pattern for current coefficient group
 * \param codingParameters coding parameters for the TU (includes the scan)
 * \param scanPosition current position in scan order
 * \param log2BlockWidth log2 width of the block
 * \param log2BlockHeight log2 height of the block
 * \param chanType channel type (CHANNEL_TYPE_LUMA/CHROMA)
 * \returns ctxInc for current scan position
 */
Int TComTrQuant::getSigCtxInc    (       Int                        patternSigCtx,
                                   const TUEntropyCodingParameters &codingParameters,
                                   const Int                        scanPosition,
                                   const Int                        log2BlockWidth,
                                   const Int                        log2BlockHeight,
                                   const ChannelType                chanType)
{
  if (codingParameters.firstSignificanceMapContext == significanceMapContextSetStart[chanType][CONTEXT_TYPE_SINGLE])
  {
    //single context mode
    return significanceMapContextSetStart[chanType][CONTEXT_TYPE_SINGLE];
  }

  const UInt rasterPosition = codingParameters.scan[scanPosition];
  const UInt posY           = rasterPosition >> log2BlockWidth;
  const UInt posX           = rasterPosition - (posY << log2BlockWidth);

  if ((posX + posY) == 0)
  {
    return 0; //special case for the DC context variable
  }

  Int offset = MAX_INT;

  if ((log2BlockWidth == 2) && (log2BlockHeight == 2)) //4x4
  {
    offset = ctxIndMap4x4[ (4 * posY) + posX ];
  }
  else
  {
    Int cnt = 0;

    switch (patternSigCtx)
    {
      //------------------

      case 0: //neither neighbouring group is significant
        {
          const Int posXinSubset     = posX & ((1 << MLS_CG_LOG2_WIDTH)  - 1);
          const Int posYinSubset     = posY & ((1 << MLS_CG_LOG2_HEIGHT) - 1);
          const Int posTotalInSubset = posXinSubset + posYinSubset;

          //first N coefficients in scan order use 2; the next few use 1; the rest use 0.
          const UInt context1Threshold = NEIGHBOURHOOD_00_CONTEXT_1_THRESHOLD_4x4;
          const UInt context2Threshold = NEIGHBOURHOOD_00_CONTEXT_2_THRESHOLD_4x4;

          cnt = (posTotalInSubset >= context1Threshold) ? 0 : ((posTotalInSubset >= context2Threshold) ? 1 : 2);
        }
        break;

      //------------------

      case 1: //right group is significant, below is not
        {
          const Int posYinSubset = posY & ((1 << MLS_CG_LOG2_HEIGHT) - 1);
          const Int groupHeight  = 1 << MLS_CG_LOG2_HEIGHT;

          cnt = (posYinSubset >= (groupHeight >> 1)) ? 0 : ((posYinSubset >= (groupHeight >> 2)) ? 1 : 2); //top quarter uses 2; second-from-top quarter uses 1; bottom half uses 0
        }
        break;

      //------------------

      case 2: //below group is significant, right is not
        {
          const Int posXinSubset = posX & ((1 << MLS_CG_LOG2_WIDTH)  - 1);
          const Int groupWidth   = 1 << MLS_CG_LOG2_WIDTH;

          cnt = (posXinSubset >= (groupWidth >> 1)) ? 0 : ((posXinSubset >= (groupWidth >> 2)) ? 1 : 2); //left quarter uses 2; second-from-left quarter uses 1; right half uses 0
        }
        break;

      //------------------

      case 3: //both neighbouring groups are significant
        {
          cnt = 2;
        }
        break;

      //------------------

      default:
        std::cerr << "ERROR: Invalid patternSigCtx \"" << Int(patternSigCtx) << "\" in getSigCtxInc" << std::endl;
        exit(1);
        break;
    }

    //------------------------------------------------

    const Bool notFirstGroup = ((posX >> MLS_CG_LOG2_WIDTH) + (posY >> MLS_CG_LOG2_HEIGHT)) > 0;

    offset = (notFirstGroup ? notFirstGroupNeighbourhoodContextOffset[chanType] : 0) + cnt;
  }

  return codingParameters.firstSignificanceMapContext + offset;
}


/** Get the best level in RD sense
 *
 * \returns best quantized transform level for given scan position
 *
 * This method calculates the best quantized transform level for a given scan position.
 */
__inline UInt TComTrQuant::xGetCodedLevel ( Double&          rd64CodedCost,          //< reference to coded cost
                                            Double&          rd64CodedCost0,         //< reference to cost when coefficient is 0
                                            Double&          rd64CodedCostSig,       //< rd64CodedCostSig reference to cost of significant coefficient
                                            Intermediate_Int lLevelDouble,           //< reference to unscaled quantized level
                                            UInt             uiMaxAbsLevel,          //< scaled quantized level
                                            UShort           ui16CtxNumSig,          //< current ctxInc for coeff_abs_significant_flag
                                            UShort           ui16CtxNumOne,          //< current ctxInc for coeff_abs_level_greater1 (1st bin of coeff_abs_level_minus1 in AVC)
                                            UShort           ui16CtxNumAbs,          //< current ctxInc for coeff_abs_level_greater2 (remaining bins of coeff_abs_level_minus1 in AVC)
                                            UShort           ui16AbsGoRice,          //< current Rice parameter for coeff_abs_level_minus3
                                            UInt             c1Idx,                  //< 
                                            UInt             c2Idx,                  //< 
                                            Int              iQBits,                 //< quantization step size
                                            Double           errorScale,             //< 
                                            Bool             bLast,                  //< indicates if the coefficient is the last significant
                                            Bool             useLimitedPrefixLength, //< 
                                            const Int        maxLog2TrDynamicRange   //< 
                                            ) const
{
  Double dCurrCostSig   = 0;
  UInt   uiBestAbsLevel = 0;

  if( !bLast && uiMaxAbsLevel < 3 )
  {
    rd64CodedCostSig    = xGetRateSigCoef( 0, ui16CtxNumSig );
    rd64CodedCost       = rd64CodedCost0 + rd64CodedCostSig;
    if( uiMaxAbsLevel == 0 )
    {
      return uiBestAbsLevel;
    }
  }
  else
  {
    rd64CodedCost       = MAX_DOUBLE;
  }

  if( !bLast )
  {
    dCurrCostSig        = xGetRateSigCoef( 1, ui16CtxNumSig );
  }

  UInt uiMinAbsLevel    = ( uiMaxAbsLevel > 1 ? uiMaxAbsLevel - 1 : 1 );
  for( Int uiAbsLevel  = uiMaxAbsLevel; uiAbsLevel >= uiMinAbsLevel ; uiAbsLevel-- )
  {
    Double dErr         = Double( lLevelDouble  - ( Intermediate_Int(uiAbsLevel) << iQBits ) );
    Double dCurrCost    = dErr * dErr * errorScale + xGetICost( xGetICRate( uiAbsLevel, ui16CtxNumOne, ui16CtxNumAbs, ui16AbsGoRice, c1Idx, c2Idx, useLimitedPrefixLength, maxLog2TrDynamicRange ) );
    dCurrCost          += dCurrCostSig;

    if( dCurrCost < rd64CodedCost )
    {
      uiBestAbsLevel    = uiAbsLevel;
      rd64CodedCost     = dCurrCost;
      rd64CodedCostSig  = dCurrCostSig;
    }
  }

  return uiBestAbsLevel;
}

/** Calculates the cost for specific absolute transform level
 * \param uiAbsLevel scaled quantized level
 * \param ui16CtxNumOne current ctxInc for coeff_abs_level_greater1 (1st bin of coeff_abs_level_minus1 in AVC)
 * \param ui16CtxNumAbs current ctxInc for coeff_abs_level_greater2 (remaining bins of coeff_abs_level_minus1 in AVC)
 * \param ui16AbsGoRice Rice parameter for coeff_abs_level_minus3
 * \param c1Idx
 * \param c2Idx
 * \param useLimitedPrefixLength
 * \param maxLog2TrDynamicRange
 * \returns cost of given absolute transform level
 */
__inline Int TComTrQuant::xGetICRate         ( const UInt    uiAbsLevel,
                                               const UShort  ui16CtxNumOne,
                                               const UShort  ui16CtxNumAbs,
                                               const UShort  ui16AbsGoRice,
                                               const UInt    c1Idx,
                                               const UInt    c2Idx,
                                               const Bool    useLimitedPrefixLength,
                                               const Int     maxLog2TrDynamicRange
                                               ) const
{
  Int  iRate      = Int(xGetIEPRate()); // cost of sign bit
  UInt baseLevel  = (c1Idx < C1FLAG_NUMBER) ? (2 + (c2Idx < C2FLAG_NUMBER)) : 1;

  if ( uiAbsLevel >= baseLevel )
  {
    UInt symbol     = uiAbsLevel - baseLevel;
    UInt length;
    if (symbol < (COEF_REMAIN_BIN_REDUCTION << ui16AbsGoRice))
    {
      length = symbol>>ui16AbsGoRice;
      iRate += (length+1+ui16AbsGoRice)<< 15;
    }
    else if (useLimitedPrefixLength)
    {
      const UInt maximumPrefixLength = (32 - (COEF_REMAIN_BIN_REDUCTION + maxLog2TrDynamicRange));

      UInt prefixLength = 0;
      UInt suffix       = (symbol >> ui16AbsGoRice) - COEF_REMAIN_BIN_REDUCTION;

      while ((prefixLength < maximumPrefixLength) && (suffix > ((2 << prefixLength) - 2)))
      {
        prefixLength++;
      }

      const UInt suffixLength = (prefixLength == maximumPrefixLength) ? (maxLog2TrDynamicRange - ui16AbsGoRice) : (prefixLength + 1/*separator*/);

      iRate += (COEF_REMAIN_BIN_REDUCTION + prefixLength + suffixLength + ui16AbsGoRice) << 15;
    }
    else
    {
      length = ui16AbsGoRice;
      symbol  = symbol - ( COEF_REMAIN_BIN_REDUCTION << ui16AbsGoRice);
      while (symbol >= (1<<length))
      {
        symbol -=  (1<<(length++));
      }
      iRate += (COEF_REMAIN_BIN_REDUCTION+length+1-ui16AbsGoRice+length)<< 15;
    }

    if (c1Idx < C1FLAG_NUMBER)
    {
      iRate += m_pcEstBitsSbac->m_greaterOneBits[ ui16CtxNumOne ][ 1 ];

      if (c2Idx < C2FLAG_NUMBER)
      {
        iRate += m_pcEstBitsSbac->m_levelAbsBits[ ui16CtxNumAbs ][ 1 ];
      }
    }
  }
  else if( uiAbsLevel == 1 )
  {
    iRate += m_pcEstBitsSbac->m_greaterOneBits[ ui16CtxNumOne ][ 0 ];
  }
  else if( uiAbsLevel == 2 )
  {
    iRate += m_pcEstBitsSbac->m_greaterOneBits[ ui16CtxNumOne ][ 1 ];
    iRate += m_pcEstBitsSbac->m_levelAbsBits[ ui16CtxNumAbs ][ 0 ];
  }
  else
  {
    iRate = 0;
  }

  return  iRate;
}

__inline Double TComTrQuant::xGetRateSigCoeffGroup  ( UShort                    uiSignificanceCoeffGroup,
                                                UShort                          ui16CtxNumSig ) const
{
  return xGetICost( m_pcEstBitsSbac->significantCoeffGroupBits[ ui16CtxNumSig ][ uiSignificanceCoeffGroup ] );
}

/** Calculates the cost of signaling the last significant coefficient in the block
 * \param uiPosX X coordinate of the last significant coefficient
 * \param uiPosY Y coordinate of the last significant coefficient
 * \param component colour component ID
 * \returns cost of last significant coefficient
 */
/*
 * \param uiWidth width of the transform unit (TU)
*/
__inline Double TComTrQuant::xGetRateLast   ( const UInt                      uiPosX,
                                              const UInt                      uiPosY,
                                              const ComponentID               component  ) const
{
  UInt uiCtxX   = g_uiGroupIdx[uiPosX];
  UInt uiCtxY   = g_uiGroupIdx[uiPosY];

  Double uiCost = m_pcEstBitsSbac->lastXBits[toChannelType(component)][ uiCtxX ] + m_pcEstBitsSbac->lastYBits[toChannelType(component)][ uiCtxY ];

  if( uiCtxX > 3 )
  {
    uiCost += xGetIEPRate() * ((uiCtxX-2)>>1);
  }
  if( uiCtxY > 3 )
  {
    uiCost += xGetIEPRate() * ((uiCtxY-2)>>1);
  }
  return xGetICost( uiCost );
}

__inline Double TComTrQuant::xGetRateSigCoef  ( UShort                          uiSignificance,
                                                UShort                          ui16CtxNumSig ) const
{
  return xGetICost( m_pcEstBitsSbac->significantBits[ ui16CtxNumSig ][ uiSignificance ] );
}

/** Get the cost for a specific rate
 * \param dRate rate of a bit
 * \returns cost at the specific rate
 */
__inline Double TComTrQuant::xGetICost        ( Double                          dRate         ) const
{
  return m_dLambda * dRate;
}

/** Get the cost of an equal probable bit
 * \returns cost of equal probable bit
 */
__inline Double TComTrQuant::xGetIEPRate      (                                               ) const
{
  return 32768;
}

/** Context derivation process of coeff_abs_significant_flag
 * \param uiSigCoeffGroupFlag significance map of L1
 * \param uiCGPosX column of current scan position
 * \param uiCGPosY row of current scan position
 * \param widthInGroups width of the block
 * \param heightInGroups height of the block
 * \returns ctxInc for current scan position
 */
UInt TComTrQuant::getSigCoeffGroupCtxInc  (const UInt*  uiSigCoeffGroupFlag,
                                           const UInt   uiCGPosX,
                                           const UInt   uiCGPosY,
                                           const UInt   widthInGroups,
                                           const UInt   heightInGroups)
{
  UInt sigRight = 0;
  UInt sigLower = 0;

  if (uiCGPosX < (widthInGroups  - 1))
  {
    sigRight = ((uiSigCoeffGroupFlag[ (uiCGPosY * widthInGroups) + uiCGPosX + 1 ] != 0) ? 1 : 0);
  }
  if (uiCGPosY < (heightInGroups - 1))
  {
    sigLower = ((uiSigCoeffGroupFlag[ (uiCGPosY + 1) * widthInGroups + uiCGPosX ] != 0) ? 1 : 0);
  }

  return ((sigRight + sigLower) != 0) ? 1 : 0;
}


/** set quantized matrix coefficient for encode
 * \param scalingList            quantized matrix address
 * \param format                 chroma format
 * \param maxLog2TrDynamicRange
 * \param bitDepths              reference to bit depth array for all channels
 */
Void TComTrQuant::setScalingList(TComScalingList *scalingList, const Int maxLog2TrDynamicRange[MAX_NUM_CHANNEL_TYPE], const BitDepths &bitDepths)
{
  const Int minimumQp = 0;
  const Int maximumQp = SCALING_LIST_REM_NUM;

  for(UInt size = 0; size < SCALING_LIST_SIZE_NUM; size++)
  {
    for(UInt list = 0; list < SCALING_LIST_NUM; list++)
    {
      for(Int qp = minimumQp; qp < maximumQp; qp++)
      {
        xSetScalingListEnc(scalingList,list,size,qp);
        xSetScalingListDec(*scalingList,list,size,qp);
        setErrScaleCoeff(list,size,qp,maxLog2TrDynamicRange, bitDepths);
      }
    }
  }
}
/** set quantized matrix coefficient for decode
 * \param scalingList quantized matrix address
 * \param format      chroma format
 */
Void TComTrQuant::setScalingListDec(const TComScalingList &scalingList)
{
  const Int minimumQp = 0;
  const Int maximumQp = SCALING_LIST_REM_NUM;

  for(UInt size = 0; size < SCALING_LIST_SIZE_NUM; size++)
  {
    for(UInt list = 0; list < SCALING_LIST_NUM; list++)
    {
      for(Int qp = minimumQp; qp < maximumQp; qp++)
      {
        xSetScalingListDec(scalingList,list,size,qp);
      }
    }
  }
}
/** set error scale coefficients
 * \param list                   list ID
 * \param size                   
 * \param qp                     quantization parameter
 * \param maxLog2TrDynamicRange
 * \param bitDepths              reference to bit depth array for all channels
 */
Void TComTrQuant::setErrScaleCoeff(UInt list, UInt size, Int qp, const Int maxLog2TrDynamicRange[MAX_NUM_CHANNEL_TYPE], const BitDepths &bitDepths)
{
  const UInt uiLog2TrSize = g_aucConvertToBit[ g_scalingListSizeX[size] ] + 2;
  const ChannelType channelType = ((list == 0) || (list == MAX_NUM_COMPONENT)) ? CHANNEL_TYPE_LUMA : CHANNEL_TYPE_CHROMA;

  const Int channelBitDepth    = bitDepths.recon[channelType];
  const Int iTransformShift = getTransformShift(channelBitDepth, uiLog2TrSize, maxLog2TrDynamicRange[channelType]);  // Represents scaling through forward transform

  UInt i,uiMaxNumCoeff = g_scalingListSize[size];
  Int *piQuantcoeff;
  Double *pdErrScale;
  piQuantcoeff   = getQuantCoeff(list, qp,size);
  pdErrScale     = getErrScaleCoeff(list, size, qp);

  Double dErrScale = (Double)(1<<SCALE_BITS);                                // Compensate for scaling of bitcount in Lagrange cost function
  dErrScale = dErrScale*pow(2.0,(-2.0*iTransformShift));                     // Compensate for scaling through forward transform

  for(i=0;i<uiMaxNumCoeff;i++)
  {
    pdErrScale[i] =  dErrScale / piQuantcoeff[i] / piQuantcoeff[i] / (1 << DISTORTION_PRECISION_ADJUSTMENT(2 * (bitDepths.recon[channelType] - 8)));
  }

  getErrScaleCoeffNoScalingList(list, size, qp) = dErrScale / g_quantScales[qp] / g_quantScales[qp] / (1 << DISTORTION_PRECISION_ADJUSTMENT(2 * (bitDepths.recon[channelType] - 8)));
}

/** set quantized matrix coefficient for encode
 * \param scalingList quantized matrix address
 * \param listId List index
 * \param sizeId size index
 * \param qp Quantization parameter
 * \param format chroma format
 */
Void TComTrQuant::xSetScalingListEnc(TComScalingList *scalingList, UInt listId, UInt sizeId, Int qp)
{
  UInt width  = g_scalingListSizeX[sizeId];
  UInt height = g_scalingListSizeX[sizeId];
  UInt ratio  = g_scalingListSizeX[sizeId]/min(MAX_MATRIX_SIZE_NUM,(Int)g_scalingListSizeX[sizeId]);
  Int *quantcoeff;
  Int *coeff  = scalingList->getScalingListAddress(sizeId,listId);
  quantcoeff  = getQuantCoeff(listId, qp, sizeId);

  Int quantScales = g_quantScales[qp];

  processScalingListEnc(coeff,
                        quantcoeff,
                        (quantScales << LOG2_SCALING_LIST_NEUTRAL_VALUE),
                        height, width, ratio,
                        min(MAX_MATRIX_SIZE_NUM, (Int)g_scalingListSizeX[sizeId]),
                        scalingList->getScalingListDC(sizeId,listId));
}

/** set quantized matrix coefficient for decode
 * \param scalingList quantaized matrix address
 * \param listId List index
 * \param sizeId size index
 * \param qp Quantization parameter
 * \param format chroma format
 */
Void TComTrQuant::xSetScalingListDec(const TComScalingList &scalingList, UInt listId, UInt sizeId, Int qp)
{
  UInt width  = g_scalingListSizeX[sizeId];
  UInt height = g_scalingListSizeX[sizeId];
  UInt ratio  = g_scalingListSizeX[sizeId]/min(MAX_MATRIX_SIZE_NUM,(Int)g_scalingListSizeX[sizeId]);
  Int *dequantcoeff;
  const Int *coeff  = scalingList.getScalingListAddress(sizeId,listId);

  dequantcoeff = getDequantCoeff(listId, qp, sizeId);

  Int invQuantScale = g_invQuantScales[qp];

  processScalingListDec(coeff,
                        dequantcoeff,
                        invQuantScale,
                        height, width, ratio,
                        min(MAX_MATRIX_SIZE_NUM, (Int)g_scalingListSizeX[sizeId]),
                        scalingList.getScalingListDC(sizeId,listId));
}

/** set flat matrix value to quantized coefficient
 */
Void TComTrQuant::setFlatScalingList(const Int maxLog2TrDynamicRange[MAX_NUM_CHANNEL_TYPE], const BitDepths &bitDepths)
{
  const Int minimumQp = 0;
  const Int maximumQp = SCALING_LIST_REM_NUM;

  for(UInt size = 0; size < SCALING_LIST_SIZE_NUM; size++)
  {
    for(UInt list = 0; list < SCALING_LIST_NUM; list++)
    {
      for(Int qp = minimumQp; qp < maximumQp; qp++)
      {
        xsetFlatScalingList(list,size,qp);
        setErrScaleCoeff(list,size,qp,maxLog2TrDynamicRange, bitDepths);
      }
    }
  }
}

/** set flat matrix value to quantized coefficient
 * \param list List ID
 * \param size size index
 * \param qp Quantization parameter
 * \param format chroma format
 */
Void TComTrQuant::xsetFlatScalingList(UInt list, UInt size, Int qp)
{
  UInt i,num = g_scalingListSize[size];
  Int *quantcoeff;
  Int *dequantcoeff;

  Int quantScales    = g_quantScales   [qp];
  Int invQuantScales = g_invQuantScales[qp] << 4;

  quantcoeff   = getQuantCoeff(list, qp, size);
  dequantcoeff = getDequantCoeff(list, qp, size);

  for(i=0;i<num;i++)
  {
    *quantcoeff++ = quantScales;
    *dequantcoeff++ = invQuantScales;
  }
}

/** set quantized matrix coefficient for encode
 * \param coeff quantaized matrix address
 * \param quantcoeff quantaized matrix address
 * \param quantScales Q(QP%6)
 * \param height height
 * \param width width
 * \param ratio ratio for upscale
 * \param sizuNum matrix size
 * \param dc dc parameter
 */
Void TComTrQuant::processScalingListEnc( Int *coeff, Int *quantcoeff, Int quantScales, UInt height, UInt width, UInt ratio, Int sizuNum, UInt dc)
{
  for(UInt j=0;j<height;j++)
  {
    for(UInt i=0;i<width;i++)
    {
      quantcoeff[j*width + i] = quantScales / coeff[sizuNum * (j / ratio) + i / ratio];
    }
  }

  if(ratio > 1)
  {
    quantcoeff[0] = quantScales / dc;
  }
}

/** set quantized matrix coefficient for decode
 * \param coeff quantaized matrix address
 * \param dequantcoeff quantaized matrix address
 * \param invQuantScales IQ(QP%6))
 * \param height height
 * \param width width
 * \param ratio ratio for upscale
 * \param sizuNum matrix size
 * \param dc dc parameter
 */
Void TComTrQuant::processScalingListDec( const Int *coeff, Int *dequantcoeff, Int invQuantScales, UInt height, UInt width, UInt ratio, Int sizuNum, UInt dc)
{
  for(UInt j=0;j<height;j++)
  {
    for(UInt i=0;i<width;i++)
    {
      dequantcoeff[j*width + i] = invQuantScales * coeff[sizuNum * (j / ratio) + i / ratio];
    }
  }

  if(ratio > 1)
  {
    dequantcoeff[0] = invQuantScales * dc;
  }
}

/** initialization process of scaling list array
 */
Void TComTrQuant::initScalingList()
{
  for(UInt sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
  {
    for(UInt qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
    {
      for(UInt listId = 0; listId < SCALING_LIST_NUM; listId++)
      {
        m_quantCoef   [sizeId][listId][qp] = new Int    [g_scalingListSize[sizeId]];
        m_dequantCoef [sizeId][listId][qp] = new Int    [g_scalingListSize[sizeId]];
        m_errScale    [sizeId][listId][qp] = new Double [g_scalingListSize[sizeId]];
      } // listID loop
    }
  }
}

/** destroy quantization matrix array
 */
Void TComTrQuant::destroyScalingList()
{
  for(UInt sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
  {
    for(UInt listId = 0; listId < SCALING_LIST_NUM; listId++)
    {
      for(UInt qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
      {
        if(m_quantCoef[sizeId][listId][qp])
        {
          delete [] m_quantCoef[sizeId][listId][qp];
        }
        if(m_dequantCoef[sizeId][listId][qp])
        {
          delete [] m_dequantCoef[sizeId][listId][qp];
        }
        if(m_errScale[sizeId][listId][qp])
        {
          delete [] m_errScale[sizeId][listId][qp];
        }
      }
    }
  }
}

Void TComTrQuant::transformSkipQuantOneSample(TComTU &rTu, const ComponentID compID, const TCoeff resiDiff, TCoeff* pcCoeff, const UInt uiPos, const QpParam &cQP, const Bool bUseHalfRoundingPoint)
{
        TComDataCU    *pcCU                           = rTu.getCU();
  const UInt           uiAbsPartIdx                   = rTu.GetAbsPartIdxTU();
  const TComRectangle &rect                           = rTu.getRect(compID);
  const UInt           uiWidth                        = rect.width;
  const UInt           uiHeight                       = rect.height;
  const Int            maxLog2TrDynamicRange          = pcCU->getSlice()->getSPS()->getMaxLog2TrDynamicRange(toChannelType(compID));
  const Int            channelBitDepth                = pcCU->getSlice()->getSPS()->getBitDepth(toChannelType(compID));
  const Int            iTransformShift                = getTransformShift(channelBitDepth, rTu.GetEquivalentLog2TrSize(compID), maxLog2TrDynamicRange);
  const Int            scalingListType                = getScalingListType(pcCU->getPredictionMode(uiAbsPartIdx), compID);
  const Bool           enableScalingLists             = getUseScalingList(uiWidth, uiHeight, true);
  const Int            defaultQuantisationCoefficient = g_quantScales[cQP.rem];

  assert( scalingListType < SCALING_LIST_NUM );
  const Int *const piQuantCoeff = getQuantCoeff( scalingListType, cQP.rem, (rTu.GetEquivalentLog2TrSize(compID)-2) );


  /* for 422 chroma blocks, the effective scaling applied during transformation is not a power of 2, hence it cannot be
  * implemented as a bit-shift (the quantised result will be sqrt(2) * larger than required). Alternatively, adjust the
  * uiLog2TrSize applied in iTransformShift, such that the result is 1/sqrt(2) the required result (i.e. smaller)
  * Then a QP+3 (sqrt(2)) or QP-3 (1/sqrt(2)) method could be used to get the required result
  */

  const Int iQBits = QUANT_SHIFT + cQP.per + iTransformShift;
  // QBits will be OK for any internal bit depth as the reduction in transform shift is balanced by an increase in Qp_per due to QpBDOffset

  const Int iAdd = ( bUseHalfRoundingPoint ? 256 : (pcCU->getSlice()->getSliceType() == I_SLICE ? 171 : 85) ) << (iQBits - 9);

  TCoeff transformedCoefficient;

  // transform-skip
  if (iTransformShift >= 0)
  {
    transformedCoefficient = resiDiff << iTransformShift;
  }
  else // for very high bit depths
  {
    const Int iTrShiftNeg  = -iTransformShift;
    const Int offset       = 1 << (iTrShiftNeg - 1);
    transformedCoefficient = ( resiDiff + offset ) >> iTrShiftNeg;
  }

  // quantization
  const TCoeff iSign = (transformedCoefficient < 0 ? -1: 1);

  const Int quantisationCoefficient = enableScalingLists ? piQuantCoeff[uiPos] : defaultQuantisationCoefficient;

  const Int64 tmpLevel = (Int64)abs(transformedCoefficient) * quantisationCoefficient;

  const TCoeff quantisedCoefficient = (TCoeff((tmpLevel + iAdd ) >> iQBits)) * iSign;

  const TCoeff entropyCodingMinimum = -(1 << maxLog2TrDynamicRange);
  const TCoeff entropyCodingMaximum =  (1 << maxLog2TrDynamicRange) - 1;
  pcCoeff[ uiPos ] = Clip3<TCoeff>( entropyCodingMinimum, entropyCodingMaximum, quantisedCoefficient );
}


Void TComTrQuant::invTrSkipDeQuantOneSample( TComTU &rTu, ComponentID compID, TCoeff inSample, Pel &reconSample, const QpParam &cQP, UInt uiPos )
{
        TComDataCU    *pcCU               = rTu.getCU();
  const UInt           uiAbsPartIdx       = rTu.GetAbsPartIdxTU();
  const TComRectangle &rect               = rTu.getRect(compID);
  const UInt           uiWidth            = rect.width;
  const UInt           uiHeight           = rect.height;
  const Int            QP_per             = cQP.per;
  const Int            QP_rem             = cQP.rem;
  const Int            maxLog2TrDynamicRange = pcCU->getSlice()->getSPS()->getMaxLog2TrDynamicRange(toChannelType(compID));
#if O0043_BEST_EFFORT_DECODING
  const Int            channelBitDepth    = pcCU->getSlice()->getSPS()->getStreamBitDepth(toChannelType(compID));
#else
  const Int            channelBitDepth    = pcCU->getSlice()->getSPS()->getBitDepth(toChannelType(compID));
#endif
  const Int            iTransformShift    = getTransformShift(channelBitDepth, rTu.GetEquivalentLog2TrSize(compID), maxLog2TrDynamicRange);
  const Int            scalingListType    = getScalingListType(pcCU->getPredictionMode(uiAbsPartIdx), compID);
  const Bool           enableScalingLists = getUseScalingList(uiWidth, uiHeight, true);
  const UInt           uiLog2TrSize       = rTu.GetEquivalentLog2TrSize(compID);

  assert( scalingListType < SCALING_LIST_NUM );

  const Int rightShift = (IQUANT_SHIFT - (iTransformShift + QP_per)) + (enableScalingLists ? LOG2_SCALING_LIST_NEUTRAL_VALUE : 0);

  const TCoeff transformMinimum = -(1 << maxLog2TrDynamicRange);
  const TCoeff transformMaximum =  (1 << maxLog2TrDynamicRange) - 1;

  // Dequantisation

  TCoeff dequantisedSample;

  if(enableScalingLists)
  {
    const UInt             dequantCoefBits     = 1 + IQUANT_SHIFT + SCALING_LIST_BITS;
    const UInt             targetInputBitDepth = std::min<UInt>((maxLog2TrDynamicRange + 1), (((sizeof(Intermediate_Int) * 8) + rightShift) - dequantCoefBits));

    const Intermediate_Int inputMinimum        = -(1 << (targetInputBitDepth - 1));
    const Intermediate_Int inputMaximum        =  (1 << (targetInputBitDepth - 1)) - 1;

    Int *piDequantCoef = getDequantCoeff(scalingListType,QP_rem,uiLog2TrSize-2);

    if(rightShift > 0)
    {
      const Intermediate_Int iAdd      = 1 << (rightShift - 1);
      const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, inSample));
      const Intermediate_Int iCoeffQ   = ((Intermediate_Int(clipQCoef) * piDequantCoef[uiPos]) + iAdd ) >> rightShift;

      dequantisedSample = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
    }
    else
    {
      const Int              leftShift = -rightShift;
      const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, inSample));
      const Intermediate_Int iCoeffQ   = (Intermediate_Int(clipQCoef) * piDequantCoef[uiPos]) << leftShift;

      dequantisedSample = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
    }
  }
  else
  {
    const Int scale     =  g_invQuantScales[QP_rem];
    const Int scaleBits =     (IQUANT_SHIFT + 1)   ;

    const UInt             targetInputBitDepth = std::min<UInt>((maxLog2TrDynamicRange + 1), (((sizeof(Intermediate_Int) * 8) + rightShift) - scaleBits));
    const Intermediate_Int inputMinimum        = -(1 << (targetInputBitDepth - 1));
    const Intermediate_Int inputMaximum        =  (1 << (targetInputBitDepth - 1)) - 1;

    if (rightShift > 0)
    {
      const Intermediate_Int iAdd      = 1 << (rightShift - 1);
      const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, inSample));
      const Intermediate_Int iCoeffQ   = (Intermediate_Int(clipQCoef) * scale + iAdd) >> rightShift;

      dequantisedSample = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
    }
    else
    {
      const Int              leftShift = -rightShift;
      const TCoeff           clipQCoef = TCoeff(Clip3<Intermediate_Int>(inputMinimum, inputMaximum, inSample));
      const Intermediate_Int iCoeffQ   = (Intermediate_Int(clipQCoef) * scale) << leftShift;

      dequantisedSample = TCoeff(Clip3<Intermediate_Int>(transformMinimum,transformMaximum,iCoeffQ));
    }
  }

  // Inverse transform-skip

  if (iTransformShift >= 0)
  {
    const TCoeff offset = iTransformShift==0 ? 0 : (1 << (iTransformShift - 1));
    reconSample =  Pel(( dequantisedSample + offset ) >> iTransformShift);
  }
  else //for very high bit depths
  {
    const Int iTrShiftNeg = -iTransformShift;
    reconSample = Pel(dequantisedSample << iTrShiftNeg);
  }
}


Void TComTrQuant::crossComponentPrediction(       TComTU      & rTu,
                                            const ComponentID   compID,
                                            const Pel         * piResiL,
                                            const Pel         * piResiC,
                                                  Pel         * piResiT,
                                            const Int           width,
                                            const Int           height,
                                            const Int           strideL,
                                            const Int           strideC,
                                            const Int           strideT,
                                            const Bool          reverse )
{
  const Pel *pResiL = piResiL;
  const Pel *pResiC = piResiC;
        Pel *pResiT = piResiT;

  TComDataCU *pCU = rTu.getCU();
  const Int alpha = pCU->getCrossComponentPredictionAlpha( rTu.GetAbsPartIdxTU( compID ), compID );
  const Int diffBitDepth = pCU->getSlice()->getSPS()->getDifferentialLumaChromaBitDepth();

  for( Int y = 0; y < height; y++ )
  {
    if (reverse)
    {
      // A constraint is to be added to the HEVC Standard to limit the size of pResiL and pResiC at this point.
      // The likely form of the constraint is to either restrict the values to CoeffMin to CoeffMax,
      // or to be representable in a bitDepthY+4 or bitDepthC+4 signed integer.
      //  The result of the constraint is that for 8/10/12bit profiles, the input values
      //  can be represented within a 16-bit Pel-type.
#if RExt__HIGH_BIT_DEPTH_SUPPORT
      for( Int x = 0; x < width; x++ )
      {
        pResiT[x] = pResiC[x] + (( alpha * rightShift( pResiL[x], diffBitDepth) ) >> 3);
      }
#else
      const Int minPel=std::numeric_limits<Pel>::min();
      const Int maxPel=std::numeric_limits<Pel>::max();
      for( Int x = 0; x < width; x++ )
      {
        pResiT[x] = Clip3<Int>(minPel, maxPel, pResiC[x] + (( alpha * rightShift<Int>(Int(pResiL[x]), diffBitDepth) ) >> 3));
      }
#endif
    }
    else
    {
      // Forward does not need clipping. Pel type should always be big enough.
      for( Int x = 0; x < width; x++ )
      {
        pResiT[x] = pResiC[x] - (( alpha * rightShift<Int>(Int(pResiL[x]), diffBitDepth) ) >> 3);
      }
    }

    pResiL += strideL;
    pResiC += strideC;
    pResiT += strideT;
  }
}

//! \}
