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

/** \file     TDecConformance.h
    \brief    Decoder conformance functions (header)
*/

#ifndef __TDECCONFORMANCE__
#define __TDECCONFORMANCE__

// This can be enabled externally. Note that this is a PARTIAL CONFORMANCE CHECK - not a full check. More checks may be added later
#ifndef DECODER_PARTIAL_CONFORMANCE_CHECK
#define DECODER_PARTIAL_CONFORMANCE_CHECK                 0 ///< 0 (default) = do not check conformance. 1 = warn if conformance checks fail. 2 = error and quit if conformance checks fail. Note this is only a partial conformance check - not a full conformance check.
#endif



#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TLibCommon/CommonDef.h"
#if DPB_ENCODER_USAGE_CHECK
#include "TLibCommon/ProfileLevelTierFeatures.h"
#endif
#include <stdio.h>
#include <iostream>
#if DECODER_PARTIAL_CONFORMANCE_CHECK == 2
#include <stdlib.h>
#endif


// Forward declarations
class TComSlice;
#if !DPB_ENCODER_USAGE_CHECK
class TComSPS;
class TComPPS;
#endif
class InputNALUnit;
class TComPTL;
class TComPic;

#if !DPB_ENCODER_USAGE_CHECK
typedef enum TRISTATE
{
  DISABLED=0,
  OPTIONAL=1,
  ENABLED=2
} TRISTATE;


typedef enum HBRFACTOREQN
{
  HBR_1 = 0,
  HBR_1_OR_2 = 1,
  HBR_12_OR_24 = 2
} HBRFACTOREQN;


struct LevelTierFeatures
{
  Level::Name level;
  UInt        maxLumaPs;
  UInt        maxCpb[Level::NUMBER_OF_TIERS];    // in units of CpbVclFactor or CpbNalFactor bits
  UInt        maxSliceSegmentsPerPicture;
  UInt        maxTileRows;
  UInt        maxTileCols;
  UInt64      maxLumaSr;
  UInt        maxBr[Level::NUMBER_OF_TIERS];     // in units of BrVclFactor or BrNalFactor bits/s
  UInt        minCrBase[Level::NUMBER_OF_TIERS];
  UInt        getMaxPicWidthInLumaSamples()  const;
  UInt        getMaxPicHeightInLumaSamples() const;
};


struct ProfileFeatures
{
  Profile::Name            profile;
  const TChar            *pNameString;
  UInt                     maxBitDepth;
  ChromaFormat             maxChromaFormat;
  Bool                     generalIntraConstraintFlag;
  Bool                     generalOnePictureOnlyConstraintFlag;
  TRISTATE                 generalLowerBitRateConstraint;
  TRISTATE                 generalRExtToolsEnabled;
  TRISTATE                 extendedPrecisionProcessingFlag;
  TRISTATE                 chromaQpOffsetListEnabledFlag;
  TRISTATE                 cabacBypassAlignmentEnabledFlag;
  HBRFACTOREQN             hbrFactorEqn;
  Bool                     bWavefrontsAndTilesCanBeUsedSimultaneously;

  UInt                     minTileColumnWidthInLumaSamples;
  UInt                     minTileRowHeightInLumaSamples;
  Bool                     bCanUseLevel8p5;
  UInt                     cpbVclFactor;
  UInt                     cpbNalFactor;                // currently not used for checking
  UInt                     formatCapabilityFactorx1000; // currently not used for checking
  UInt                     minCrScaleFactorx10;         // currently not used for checking
  const LevelTierFeatures *pLevelTiersListInfo;

  Bool chromaFormatValid(ChromaFormat chFmt) const { return (profile == Profile::MAINREXT || profile == Profile::HIGHTHROUGHPUTREXT) ? chFmt<=maxChromaFormat : (chFmt == maxChromaFormat ); }
  Bool onlyIRAPPictures()                    const { return generalIntraConstraintFlag; }
  UInt getHbrFactor(Bool bLowerBitRateConstraintFlag) const    // currently not used for checking
  {
    return hbrFactorEqn==HBR_1_OR_2   ? (2-(bLowerBitRateConstraintFlag?1:0)) :
          (hbrFactorEqn==HBR_12_OR_24 ? 12*(2-(bLowerBitRateConstraintFlag?1:0)) :
                                        1);
  }
};


class ProfileLevelTierFeatures
{
  private:
    const ProfileFeatures   *m_pProfile;
    const LevelTierFeatures *m_pLevelTier;
    UInt                     m_hbrFactor;               // currently not used for checking
    Level::Tier              m_tier;
    UInt                     m_maxRawCtuBits;
  public:
    ProfileLevelTierFeatures() : m_pProfile(0), m_pLevelTier(0), m_hbrFactor(0), m_tier(Level::MAIN), m_maxRawCtuBits(0) { }

    Void activate(const TComSPS &sps);

    const ProfileFeatures     *getProfileFeatures()   const { return m_pProfile; }
    const LevelTierFeatures   *getLevelTierFeatures() const { return m_pLevelTier; }
    Level::Tier                getTier() const { return m_tier; }
    UInt64 getCpbSizeInBits()            const { return (m_pLevelTier!=0 && m_pProfile!=0) ? UInt64(m_pProfile->cpbVclFactor) * m_pLevelTier->maxCpb[m_tier?1:0] : UInt64(0); }
    Double getMinCr()                    const { return (m_pLevelTier!=0 && m_pProfile!=0) ? (m_pProfile->minCrScaleFactorx10 * m_pLevelTier->minCrBase[m_tier?1:0])/10.0 : 0.0 ; }   // currently not used for checking
    UInt getMaxRawCtuBits()              const { return m_maxRawCtuBits; }
};
#endif


class TDecConformanceCheck
{
private:
#if MCTS_ENC_CHECK
  Bool m_tmctsCheckEnabled;
#endif
#if DECODER_PARTIAL_CONFORMANCE_CHECK
  UInt  m_numberOfSlicesInPicture;
  UInt64 m_bytesInPicture;
#endif
  ProfileLevelTierFeatures m_activatedFeatures;

public:

  // Static member functions

  static inline Bool doChecking()
  {
    return DECODER_PARTIAL_CONFORMANCE_CHECK != 0;
  }

  static UInt getMinLog2CtbSize(const TComPTL &ptl, UInt layerPlus1=0);
  static UInt getMaxLog2CtbSize(const TComPTL &ptl, UInt layerPlus1=0);


#if DECODER_PARTIAL_CONFORMANCE_CHECK == 0
  static inline std::ostream &getStream()  { return std::cout; }

  static inline Void finishWarningReport() { }

  template <class T>
  static Void checkRange(const T& , const TChar *, const T& , const T& ) { }

#else


  static inline std::ostream &getStream()
  {
#if DECODER_PARTIAL_CONFORMANCE_CHECK == 1
    std::cout << "WARNING: Conformance failure - ";
    return std::cout;
#else
    std::cerr << "ERROR: Conformance failure - ";
    return std::cerr;
#endif
  }

  static inline Void finishWarningReport()
  {
#if DECODER_PARTIAL_CONFORMANCE_CHECK == 2
    exit(1);
#endif
  }

  template <class T>
  static Void checkRange(const T& val, const TChar *name, const T& minValInclusive, const T& maxValInclusive)
  {
    if (val<minValInclusive || val>maxValInclusive)
    {
      getStream() << name << " must be in the range of " << minValInclusive << " to " << maxValInclusive << " (inclusive) - decoded value of " << val << "\n";
      finishWarningReport();
    }
  }

#endif


  // Member functions

  TDecConformanceCheck();

#if DECODER_PARTIAL_CONFORMANCE_CHECK == 0
  Void
  checkSliceActivation(const TComSlice &/*slice*/,
                       const InputNALUnit &/*nalu*/,
                       const TComPic &/*pic*/,
                       const Bool /*bFirstSliceInStream*/,
                       const Bool /*bFirstSliceInSequence*/,
                       const Bool /*bFirstSliceInPicture*/) { }

  Void
  checkCtuDecoding(const UInt numUsedBits) { }
#else
  Void
  checkSliceActivation(const TComSlice &slice,
                       const InputNALUnit &nalu,
                       const TComPic &pic,
                       const Bool bFirstSliceInStream,
                       const Bool bFirstSliceInSequence,
                       const Bool bFirstSliceInPicture);

  Void
  checkCtuDecoding(const UInt numUsedBits);
#endif

#if MCTS_ENC_CHECK
  Void enableTMctsCheck(Bool enabled) { m_tmctsCheckEnabled = enabled; };
  Bool getTMctsCheck() const { return m_tmctsCheckEnabled;  }
  Void flagTMctsError(const char *error)
  {
    fprintf(stderr, "TMCTS check error: %s\n", error);
  }
#endif
};


#endif
