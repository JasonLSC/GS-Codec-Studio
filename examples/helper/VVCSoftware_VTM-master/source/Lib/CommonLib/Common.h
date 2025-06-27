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

/** \file     Common.h
 *  \brief    Common 2D-geometrical structures
 */

#ifndef __COMMON__
#define __COMMON__

#include "CommonDef.h"

using PosType  = int32_t;
using SizeType = uint32_t;

struct Position
{
  PosType x;
  PosType y;

  Position()                                   : x(0),  y(0)  { }
  Position(const PosType _x, const PosType _y) : x(_x), y(_y) { }

  bool operator!=(const Position &other)  const { return x != other.x || y != other.y; }
  bool operator==(const Position &other)  const { return x == other.x && y == other.y; }

  Position offset(const Position pos)                 const { return Position(x + pos.x, y + pos.y); }
  Position offset(const PosType _x, const PosType _y) const { return Position(x + _x   , y + _y   ); }
  void     repositionTo(const Position newPos)              { x  = newPos.x; y  = newPos.y; }
  void     relativeTo  (const Position origin)              { x -= origin.x; y -= origin.y; }

  Position operator-( const Position &other )         const { return{ x - other.x, y - other.y }; }
};

struct Size
{
  SizeType width;
  SizeType height;

  Size()                                              : width(0),      height(0)       { }
  Size(const SizeType _width, const SizeType _height) : width(_width), height(_height) { }

  bool operator!=(const Size &other)      const { return (width != other.width) || (height != other.height); }
  bool operator==(const Size &other)      const { return (width == other.width) && (height == other.height); }
  uint32_t area()                             const { return (uint32_t) width * (uint32_t) height; }
#if REUSE_CU_RESULTS_WITH_MULTIPLE_TUS
  void resizeTo(const Size newSize)             { width = newSize.width; height = newSize.height; }
#endif
};

struct Area : public Position, public Size
{
  Area()                                                                         : Position(),       Size()       { }
  Area(const Position &_pos, const Size &_size)                                  : Position(_pos),   Size(_size)  { }
  Area(const PosType _x, const PosType _y, const SizeType _w, const SizeType _h) : Position(_x, _y), Size(_w, _h) { }

        Position& pos()                           { return *this; }
  const Position& pos()                     const { return *this; }
        Size&     size()                          { return *this; }
  const Size&     size()                    const { return *this; }

  const Position& topLeft()                 const { return *this; }
        Position  topRight()                const { return { (PosType) (x + width - 1), y                          }; }
        Position  bottomLeft()              const { return { x                        , (PosType) (y + height - 1) }; }
        Position  bottomRight()             const { return { (PosType) (x + width - 1), (PosType) (y + height - 1) }; }
        Position  center()                  const { return { (PosType) (x + width / 2), (PosType) (y + height / 2) }; }

  bool contains(const Area &_area)          const { return contains(_area.pos()) && contains(_area.bottomRight()); }
  bool contains(const Position &_pos) const
  {
    static_assert(std::is_unsigned<SizeType>::value, "SizeType should be unsigned");
    // NOTE: when _pos.x is smaller than x, SizeType(_pos.x - x) is a large unsigned number
    const bool condX = SizeType(_pos.x - x) < width;
    const bool condY = SizeType(_pos.y - y) < height;

    return condX && condY;
  }

#if GDR_ENABLED
  bool overlaps(const Area &_area) const
  {
    // check cases where one area contains the other completely
    if (contains(_area))
    {
      return false;
    }

    if (_area.contains(*this))
    {
      return false;
    }

    // at least one of the corners should be outside here
    const bool topLeft  = contains(_area.topLeft());
    const bool topRight = contains(_area.topRight());
    const bool botLeft  = contains(_area.bottomLeft());
    const bool botRight = contains(_area.bottomRight());

    return topLeft || topRight || botLeft || botRight;
  }
#endif

  bool operator!=(const Area &other)        const { return (Size::operator!=(other)) || (Position::operator!=(other)); }
  bool operator==(const Area &other)        const { return (Size::operator==(other)) && (Position::operator==(other)); }
};

struct UnitScale
{
  UnitScale()                 : posx( 0), posy( 0), area(posx+posy) {}
  UnitScale( int sx, int sy ) : posx(sx), posy(sy), area(posx+posy) {}
  int posx;
  int posy;
  int area;

  template<typename T> T scaleHor ( const T &in ) const { return in >> posx; }
  template<typename T> T scaleVer ( const T &in ) const { return in >> posy; }
  template<typename T> T scaleArea( const T &in ) const { return in >> area; }

  Position scale( const Position &pos  ) const { return { pos.x >> posx, pos.y >> posy }; }
  Size     scale( const Size     &size ) const { return { size.width >> posx, size.height >> posy }; }
  Area     scale( const Area    &_area ) const { return Area( scale( _area.pos() ), scale( _area.size() ) ); }
};

namespace std
{
  template<> struct hash<Position>
  {
    size_t operator()(const Position &value) const { return (((size_t) value.x << 32) + value.y); }
  };

  template<> struct hash<Size>
  {
    size_t operator()(const Size &value) const { return (((size_t) value.width << 32) + value.height); }
  };
}

inline ptrdiff_t rsAddr(const Position &pos, const uint32_t stride, const UnitScale &unitScale)
{
  return (ptrdiff_t) (stride >> unitScale.posx) * (ptrdiff_t) (pos.y >> unitScale.posy)
         + (ptrdiff_t) (pos.x >> unitScale.posx);
}

inline size_t rsAddr(const Position &pos, const Position &origin, const uint32_t stride, const UnitScale &unitScale )
{
  return (stride >> unitScale.posx) * ((pos.y - origin.y) >> unitScale.posy) + ((pos.x - origin.x) >> unitScale.posx);
}

inline ptrdiff_t rsAddr(const Position &pos, const ptrdiff_t stride)
{
  return stride * (ptrdiff_t) pos.y + (ptrdiff_t) pos.x;
}

inline size_t rsAddr(const Position &pos, const Position &origin, const ptrdiff_t stride)
{
  return stride * (pos.y - origin.y) + (pos.x - origin.x);
}

inline Area clipArea(const Area &_area, const Area &boundingBox)
{
  Area area = _area;

  if (area.x + area.width > boundingBox.x + boundingBox.width)
  {
    area.width = boundingBox.x + boundingBox.width - area.x;
  }

  if (area.y + area.height > boundingBox.y + boundingBox.height)
  {
    area.height = boundingBox.y + boundingBox.height - area.y;
  }

  return area;
}


class SizeIndexInfo
{
protected:
  static constexpr SizeType UNUSED = std::numeric_limits<SizeType>::max();

public:
  SizeIndexInfo(){}
  virtual ~SizeIndexInfo(){}
  SizeType numAllWidths()               { return (SizeType)m_idxToSizeTab.size(); }
  SizeType numAllHeights()              { return (SizeType)m_idxToSizeTab.size(); }
  SizeType numWidths()                  { return (SizeType)m_numBlkSizes; }
  SizeType numHeights()                 { return (SizeType)m_numBlkSizes; }
  SizeType sizeFrom( SizeType idx )     { return m_idxToSizeTab[idx]; }
  SizeType idxFrom(SizeType size)
  {
    CHECKD(m_sizeToIdxTab[size] == UNUSED, "Index of given size does NOT EXIST!");
    return m_sizeToIdxTab[size];
  }
  bool     isCuSize( SizeType size )    { return m_isCuSize[size]; }
  virtual void init( SizeType maxSize ) {}

protected:

  void xInit()
  {
    m_isCuSize.resize( m_sizeToIdxTab.size(), false );

    std::vector<SizeType> grpSizes;

    int n = 0;

    for( int i = 0; i < m_sizeToIdxTab.size(); i++ )
    {
      if (m_sizeToIdxTab[i] != UNUSED)
      {
        m_sizeToIdxTab[i] = n;
        m_idxToSizeTab.push_back( i );
        n++;
      }

      if (m_sizeToIdxTab[i] != UNUSED && m_sizeToIdxTab[i >> 1] != UNUSED && i >= MIN_CU_SIZE)
      {
        m_isCuSize[i] = true;
      }

      // collect group sizes (for coefficient group coding)
      SizeType grpSize = i >> ( ( i & 3 ) != 0 ? 1 : 2 );
      if (m_sizeToIdxTab[i] != UNUSED && m_sizeToIdxTab[grpSize] == UNUSED)
      {
        grpSizes.push_back( grpSize );
      }
    }

    CHECK(n > MAX_NUM_SIZES, "MAX_NUM_SIZES is too small");

    m_numBlkSizes = (SizeType)m_idxToSizeTab.size();

    for( SizeType grpSize : grpSizes )
    {
      if (grpSize > 0 && m_sizeToIdxTab[grpSize] == UNUSED)
      {
        m_sizeToIdxTab[grpSize] = (SizeType)m_idxToSizeTab.size();
        m_idxToSizeTab.push_back( grpSize );
      }
    }
  };

  std::vector<bool    > m_isCuSize;
  int                   m_numBlkSizes; // as opposed to number all sizes, which also contains grouped sizes
  std::vector<SizeType> m_sizeToIdxTab;
  std::vector<SizeType> m_idxToSizeTab;
};

class SizeIndexInfoLog2 : public SizeIndexInfo
{
public:
  SizeIndexInfoLog2(){}
  ~SizeIndexInfoLog2(){};

  void init( SizeType maxSize )
  {
    for( int i = 0, n = 0; i <= maxSize; i++ )
    {
      SizeType val = UNUSED;
      if( i == ( 1 << n ) )
      {
        n++;
        val = i;
      }
      m_sizeToIdxTab.push_back( val );
    }
    SizeIndexInfo::xInit();
  }
};



#endif
