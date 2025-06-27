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

/** \file     Mv.h
    \brief    motion vector class (header)
*/

#ifndef __MV__
#define __MV__

#include "CommonDef.h"

//! \ingroup CommonLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

enum class MvPrecision : int
{
  FOUR      = -2,   // 4-pel
  ONE       = 0,    // 1-pel
  HALF      = 1,    // 1/2-pel
  QUARTER   = 2,    // 1/4-pel (the precision of regular MV difference signaling)
  SIXTEENTH = 4,    // 1/16-pel (the precision of internal MV)
  INTERNAL  = ONE + MV_FRACTIONAL_BITS_INTERNAL,
};

static inline constexpr int operator-(const MvPrecision &a, const MvPrecision &b)
{
  return to_underlying(a) - to_underlying(b);
}

/// basic motion vector class
class Mv
{
private:
  static const MvPrecision m_amvrPrecision[4];
  static const MvPrecision m_amvrPrecAffine[3];
  static const MvPrecision m_amvrPrecIbc[3];

public:
  int   hor;     ///< horizontal component of motion vector
  int   ver;     ///< vertical component of motion vector

  // ------------------------------------------------------------------------------------------------------------------
  // constructors
  // ------------------------------------------------------------------------------------------------------------------

  Mv(                    ) : hor( 0    ), ver( 0    ) {}
  Mv( int iHor, int iVer ) : hor( iHor ), ver( iVer ) {}

  // ------------------------------------------------------------------------------------------------------------------
  // set
  // ------------------------------------------------------------------------------------------------------------------

  void  set       ( int iHor, int iVer)     { hor = iHor;  ver = iVer; }
  void  setHor    ( int i )                 { hor = i;                 }
  void  setVer    ( int i )                 { ver = i;                 }
  void  setZero   ()                        { hor = ver = 0;           }

  // ------------------------------------------------------------------------------------------------------------------
  // get
  // ------------------------------------------------------------------------------------------------------------------

  int   getHor    () const { return hor;          }
  int   getVer    () const { return ver;          }
  int   getAbsHor () const { return abs( hor );   }
  int   getAbsVer () const { return abs( ver );   }

  // ------------------------------------------------------------------------------------------------------------------
  // operations
  // ------------------------------------------------------------------------------------------------------------------

  const Mv& operator += (const Mv& _rcMv)
  {
    {
      Mv rcMv = _rcMv;

      hor += rcMv.hor;
      ver += rcMv.ver;
    }
    return  *this;
  }

  const Mv& operator-= (const Mv& _rcMv)
  {
    {
      Mv rcMv = _rcMv;

      hor -= rcMv.hor;
      ver -= rcMv.ver;
    }
    return  *this;
  }

  const Mv& operator<<= (const int i)
  {
    hor *= 1 << i;
    ver *= 1 << i;
    return  *this;
  }

  // NOTE: the right shift operator uses symmetric rounding
  const Mv& operator>>= ( const int i )
  {
    if (i > 0)
    {
      CHECKD(i > 30, "overflow in offset calculation");

      const int offset = 1 << i >> 1;

      hor = (hor + offset + (~hor >> 31)) >> i;
      ver = (ver + offset + (~ver >> 31)) >> i;
    }
    return  *this;
  }

  const Mv operator - ( const Mv& rcMv ) const
  {
    return Mv( hor - rcMv.hor, ver - rcMv.ver );
  }

  const Mv operator + ( const Mv& rcMv ) const
  {
    return Mv( hor + rcMv.hor, ver + rcMv.ver );
  }

  bool operator== ( const Mv& rcMv ) const
  {
    return ( hor == rcMv.hor && ver == rcMv.ver );
  }

  bool operator!= ( const Mv& rcMv ) const
  {
    return !( *this == rcMv );
  }

  Mv getScaledMv(const int scale) const
  {
    Mv mv(scale * hor, scale * ver);
    mv >>= 8;
    mv.clipToStorageBitDepth();
    return mv;
  }

  void changePrecision(const MvPrecision src, const MvPrecision dst)
  {
    const int shift = dst - src;
    if (shift >= 0)
    {
      *this <<= shift;
    }
    else
    {
      *this >>= -shift;
    }
  }

  void roundToPrecision(const MvPrecision& src, const MvPrecision& dst)
  {
    changePrecision(src, dst);
    changePrecision(dst, src);
  }

  // translational MV
  void changeTransPrecInternal2Amvr(const int amvr) { changePrecision(MvPrecision::INTERNAL, m_amvrPrecision[amvr]); }
  void changeTransPrecAmvr2Internal(const int amvr) { changePrecision(m_amvrPrecision[amvr], MvPrecision::INTERNAL); }
  void roundTransPrecInternal2Amvr(const int amvr) { roundToPrecision(MvPrecision::INTERNAL, m_amvrPrecision[amvr]); }

  // affine MV
  static double getAffineAmvrScale(int amvrIdx)
  {
    const int shift = m_amvrPrecAffine[amvrIdx] - MvPrecision::ONE;
    if (shift >= 0)
    {
      return 1.0 * (1 << shift);
    }
    else
    {
      return 1.0 / (1 << -shift);
    }
  };

  void changeAffinePrecInternal2Amvr(const int amvr) { changePrecision(MvPrecision::INTERNAL, m_amvrPrecAffine[amvr]); }
  void changeAffinePrecAmvr2Internal(const int amvr) { changePrecision(m_amvrPrecAffine[amvr], MvPrecision::INTERNAL); }
  void roundAffinePrecInternal2Amvr(const int amvr) { roundToPrecision(MvPrecision::INTERNAL, m_amvrPrecAffine[amvr]); }

  // IBC block vector
  void changeIbcPrecInternal2Amvr(const int amvr) { changePrecision(MvPrecision::INTERNAL, m_amvrPrecIbc[amvr]); }
  void changeIbcPrecAmvr2Internal(const int amvr) { changePrecision(m_amvrPrecIbc[amvr], MvPrecision::INTERNAL); }
  void roundIbcPrecInternal2Amvr(const int amvr) { roundToPrecision(MvPrecision::INTERNAL, m_amvrPrecIbc[amvr]); }

  Mv getSymmvdMv(const Mv& curMvPred, const Mv& tarMvPred)
  {
    return Mv(tarMvPred.hor - hor + curMvPred.hor, tarMvPred.ver - ver + curMvPred.ver);
  }

  bool isInRange() const { return hor >= MV_MIN && hor <= MV_MAX && ver >= MV_MIN && ver <= MV_MAX; }
  bool isInRangeDelta() const { return hor >= MVD_MIN && hor <= MVD_MAX && ver >= MVD_MIN && ver <= MVD_MAX; }

  void clipToStorageBitDepth()
  {
    hor = Clip3(MV_MIN, MV_MAX, hor);
    ver = Clip3(MV_MIN, MV_MAX, ver);
  }

  void foldToStorageBitDepth()
  {
    constexpr int MSB  = 1 << (MV_BITS - 1);
    constexpr int LSBS = MSB - 1;

    hor = (hor & LSBS) - (hor & MSB);
    ver = (ver & LSBS) - (ver & MSB);
  }
};

namespace std
{
  template<> struct hash<Mv>
  {
    size_t operator()(const Mv &value) const { return (((size_t) value.hor << 32) + value.ver); }
  };
};

extern void(*clipMv) ( Mv& rcMv, const struct Position& pos, const struct Size& size, const class SPS& sps, const class PPS& pps );
void clipMvInPic ( Mv& rcMv, const struct Position& pos, const struct Size& size, const class SPS& sps, const class PPS& pps );
void clipMvInSubpic ( Mv& rcMv, const struct Position& pos, const struct Size& size, const class SPS& sps, const class PPS& pps );

bool wrapClipMv( Mv& rcMv, const Position& pos,
                 const struct Size& size,
                 const SPS *sps
               , const PPS* pps
);

//! \}

#endif // __MV__
