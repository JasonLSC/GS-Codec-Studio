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

/** \file     AlfParameters.h
    \brief    Define types for storing ALF parameters
*/

#ifndef __ALFPARAMETERS__
#define __ALFPARAMETERS__

#include <vector>
#include "CommonDef.h"

//! \ingroup AlfParameters
//! \{

enum AlfFilterType
{
  ALF_FILTER_5,
  ALF_FILTER_7,
  CC_ALF,
  ALF_NUM_OF_FILTER_TYPES
};


static const int size_CC_ALF = -1;

struct AlfFilterShape
{
  AlfFilterShape(int size) : filterLength(size), numCoeff(size * size / 4 + 1)
  {
    if( size == 5 )
    {
      pattern = {
                 0,
             1,  2,  3,
         4,  5,  6,  5,  4,
             3,  2,  1,
                 0
      };

      filterType = ALF_FILTER_5;
    }
    else if( size == 7 )
    {
      pattern = {
                     0,
                 1,  2,  3,
             4,  5,  6,  7,  8,
         9, 10, 11, 12, 11, 10, 9,
             8,  7,  6,  5,  4,
                 3,  2,  1,
                     0
      };

      filterType = ALF_FILTER_7;
    }
    else if (size == size_CC_ALF)
    {
      size = 4;
      filterLength = 8;
      numCoeff = 8;
      filterType   = CC_ALF;
    }
    else
    {
      filterType = ALF_NUM_OF_FILTER_TYPES;
      THROW("Wrong ALF filter shape");
    }
  }

  AlfFilterType filterType;
  int filterLength;
  int              numCoeff;
  std::vector<int> pattern;
};

using AlfApsList = static_vector<int, ALF_CTB_MAX_NUM_APS>;

struct AlfParam
{
  bool                         enabledFlag[MAX_NUM_COMPONENT];                          // alf_slice_enable_flag, alf_chroma_idc
  EnumArray<bool, ChannelType> nonLinearFlag;
  AlfCoeff                     lumaCoeff[MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF];   // alf_coeff_luma_delta[i][j]
  AlfClipIdx                   lumaClipp[MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF];   // alf_clipp_luma_[i][j]
  int                          numAlternativesChroma;                                                  // alf_chroma_num_alts_minus_one + 1
  AlfCoeff chromaCoeff[ALF_MAX_NUM_ALTERNATIVES_CHROMA][MAX_NUM_ALF_CHROMA_COEFF];   // alf_coeff_chroma[i]
  AlfClipIdx chromaClipp[ALF_MAX_NUM_ALTERNATIVES_CHROMA][MAX_NUM_ALF_CHROMA_COEFF];   // alf_clipp_chroma[i]
  AlfBankIdx filterCoeffDeltaIdx[MAX_NUM_ALF_CLASSES];                                 // filter_coeff_delta[i]
  bool                         alfLumaCoeffFlag[MAX_NUM_ALF_CLASSES];                   // alf_luma_coeff_flag[i]
  int                          numLumaFilters;                                          // number_of_filters_minus1 + 1
  bool                         alfLumaCoeffDeltaFlag;                                   // alf_luma_coeff_delta_flag
  EnumArray<std::vector<AlfFilterShape>, ChannelType> *filterShapes;
  EnumArray<bool, ChannelType>                         newFilterFlag;

  AlfParam()
  {
    reset();
  }

  void reset()
  {
    std::memset( enabledFlag, false, sizeof( enabledFlag ) );
    nonLinearFlag.fill(false);
    std::memset( lumaCoeff, 0, sizeof( lumaCoeff ) );
    std::memset( lumaClipp, 0, sizeof( lumaClipp ) );
    numAlternativesChroma = 1;
    std::memset( chromaCoeff, 0, sizeof( chromaCoeff ) );
    std::memset( chromaClipp, 0, sizeof( chromaClipp ) );
    std::memset( filterCoeffDeltaIdx, 0, sizeof( filterCoeffDeltaIdx ) );
    std::memset( alfLumaCoeffFlag, true, sizeof( alfLumaCoeffFlag ) );
    numLumaFilters = 1;
    alfLumaCoeffDeltaFlag = false;
    newFilterFlag.fill(false);
  }

  const AlfParam& operator = ( const AlfParam& src )
  {
    std::memcpy( enabledFlag, src.enabledFlag, sizeof( enabledFlag ) );
    nonLinearFlag = src.nonLinearFlag;
    std::memcpy( lumaCoeff, src.lumaCoeff, sizeof( lumaCoeff ) );
    std::memcpy( lumaClipp, src.lumaClipp, sizeof( lumaClipp ) );
    numAlternativesChroma = src.numAlternativesChroma;
    std::memcpy( chromaCoeff, src.chromaCoeff, sizeof( chromaCoeff ) );
    std::memcpy( chromaClipp, src.chromaClipp, sizeof( chromaClipp ) );
    std::memcpy( filterCoeffDeltaIdx, src.filterCoeffDeltaIdx, sizeof( filterCoeffDeltaIdx ) );
    std::memcpy( alfLumaCoeffFlag, src.alfLumaCoeffFlag, sizeof( alfLumaCoeffFlag ) );
    numLumaFilters = src.numLumaFilters;
    alfLumaCoeffDeltaFlag = src.alfLumaCoeffDeltaFlag;
    filterShapes = src.filterShapes;
    newFilterFlag         = src.newFilterFlag;
    return *this;
  }

  bool operator==( const AlfParam& other )
  {
    if( memcmp( enabledFlag, other.enabledFlag, sizeof( enabledFlag ) ) )
    {
      return false;
    }
    if (nonLinearFlag != other.nonLinearFlag)
    {
      return false;
    }
    if( memcmp( lumaCoeff, other.lumaCoeff, sizeof( lumaCoeff ) ) )
    {
      return false;
    }
    if( memcmp( lumaClipp, other.lumaClipp, sizeof( lumaClipp ) ) )
    {
      return false;
    }
    if( memcmp( chromaCoeff, other.chromaCoeff, sizeof( chromaCoeff ) ) )
    {
      return false;
    }
    if( memcmp( chromaClipp, other.chromaClipp, sizeof( chromaClipp ) ) )
    {
      return false;
    }
    if( memcmp( filterCoeffDeltaIdx, other.filterCoeffDeltaIdx, sizeof( filterCoeffDeltaIdx ) ) )
    {
      return false;
    }
    if( memcmp( alfLumaCoeffFlag, other.alfLumaCoeffFlag, sizeof( alfLumaCoeffFlag ) ) )
    {
      return false;
    }
    if (newFilterFlag != other.newFilterFlag)
    {
      return false;
    }
    if( numAlternativesChroma != other.numAlternativesChroma )
    {
      return false;
    }
    if( numLumaFilters != other.numLumaFilters )
    {
      return false;
    }
    if( alfLumaCoeffDeltaFlag != other.alfLumaCoeffDeltaFlag )
    {
      return false;
    }

    return true;
  }

  bool operator!=( const AlfParam& other )
  {
    return !( *this == other );
  }
};

struct CcAlfFilterParam
{
  bool    ccAlfFilterEnabled[2];
  bool    ccAlfFilterIdxEnabled[2][MAX_NUM_CC_ALF_FILTERS];
  uint8_t ccAlfFilterCount[2];
  AlfCoeff ccAlfCoeff[2][MAX_NUM_CC_ALF_FILTERS][MAX_NUM_CC_ALF_CHROMA_COEFF];
  int     newCcAlfFilter[2];
  int     numberValidComponents;
  CcAlfFilterParam()
  {
    reset();
  }
  void reset()
  {
    std::memset( ccAlfFilterEnabled, false, sizeof( ccAlfFilterEnabled ) );
    std::memset( ccAlfFilterIdxEnabled, false, sizeof( ccAlfFilterIdxEnabled ) );
    std::memset( ccAlfCoeff, 0, sizeof( ccAlfCoeff ) );
    ccAlfFilterCount[0] = ccAlfFilterCount[1] = MAX_NUM_CC_ALF_FILTERS;
    numberValidComponents = 3;
    newCcAlfFilter[0] = newCcAlfFilter[1] = 0;
  }
  const CcAlfFilterParam& operator = ( const CcAlfFilterParam& src )
  {
    std::memcpy( ccAlfFilterEnabled, src.ccAlfFilterEnabled, sizeof( ccAlfFilterEnabled ) );
    std::memcpy( ccAlfFilterIdxEnabled, src.ccAlfFilterIdxEnabled, sizeof( ccAlfFilterIdxEnabled ) );
    std::memcpy( ccAlfCoeff, src.ccAlfCoeff, sizeof( ccAlfCoeff ) );
    ccAlfFilterCount[0] = src.ccAlfFilterCount[0];
    ccAlfFilterCount[1] = src.ccAlfFilterCount[1];
    numberValidComponents = src.numberValidComponents;
    newCcAlfFilter[0] = src.newCcAlfFilter[0];
    newCcAlfFilter[1] = src.newCcAlfFilter[1];

    return *this;
  }
};
//! \}

#endif  // end of #ifndef  __ALFPARAMETERS__
