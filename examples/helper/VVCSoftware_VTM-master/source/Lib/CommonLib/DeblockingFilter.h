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

/** \file     DeblockingFilter.h
    \brief    deblocking filter (header)
*/

#ifndef __DEBLOCKINGFILTER__
#define __DEBLOCKINGFILTER__

#include "CommonDef.h"
#include "Unit.h"
#include "Picture.h"

//! \ingroup CommonLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// deblocking filter class
class DeblockingFilter
{
  static constexpr int LOG_GRID_SIZE  = 2;
  static constexpr int GRID_SIZE      = 1 << LOG_GRID_SIZE;
  static constexpr int GRID_SIZE_CHR  = 8;
  static constexpr int SUB_BLOCK_SIZE = 8;

  class EdgeStrengths
  {
    // bit  7  : transform edge flag
    // bit  6  : prediction edge flag
    // bits 4-5: boundary strengh Cr
    // bits 2-3: boundary strengh Cb
    // bits 0-1: boundary strengh Y
    uint8_t val;

    static constexpr int TEM = 0x80;   // transform edge mask
    static constexpr int PEM = 0x40;   // prediction edge mask
    static constexpr int BPS = 2;      // bits per strength

  public:
    EdgeStrengths() : val(0) {}

    void setTransEdge(bool b) { val = b ? val | TEM : val & ~TEM; }
    bool getTransEdge() const { return (val & TEM) != 0; }
    void setPredEdge(bool b) { val = b ? val | PEM : val & ~PEM; }
    bool getPredEdge() const { return (val & PEM) != 0; }
    bool hasEdge() const { return (val & (TEM | PEM)) != 0; }
    int  getBoundaryStrength(ComponentID c) const { return (val >> (BPS * c)) & ((1 << BPS) - 1); }

    EdgeStrengths &setBoundaryStrength(ComponentID c, int bs)
    {
      val |= bs << (BPS * c);
      return *this;
    }

    EdgeStrengths &operator|=(const EdgeStrengths &es)
    {
      val |= es.val;
      return *this;
    }
  };

public:
  enum class EdgeDir
  {
    VER = 0,
    HOR,
    NUM
  };

private:
  EnumArray<static_vector<EdgeStrengths, MAX_NUM_PARTS_IN_CTU>, EdgeDir> m_edgeStrengths;

  struct CuEdgeParams
  {
    bool internal;
    bool left;
    bool top;
  };

  CuEdgeParams m_filterCuEdge;

  int     m_ctuXLumaSamples, m_ctuYLumaSamples;                            // location of left-edge and top-edge of CTU

  // shift values to convert location from luma sample units to chroma sample units
  int m_shiftHor;
  int m_shiftVer;

  enum class FilterLen : uint8_t
  {
    _1,
    _2,
    _3,
    _5,
    _7,
    NUM
  };

  struct FilterLenPair
  {
    FilterLen p;
    FilterLen q;
  };

  static constexpr FilterLenPair DEFAULT_FL2 = { FilterLen::_7, FilterLen::_7 };

  // maxFilterLen for [channel type][luma/chroma sample distance from left edge of CTU]
  // [luma/chroma sample distance from top edge of CTU]
  EnumArray<FilterLenPair[MAX_CU_SIZE / GRID_SIZE][MAX_CU_SIZE / GRID_SIZE], ChannelType> m_maxFilterLen;

  // transform edge flag for [channel type][luma/chroma sample distance from left edge of CTU]
  // [luma/chroma sample distance from top edge of CTU]
  EnumArray<bool[MAX_CU_SIZE / GRID_SIZE][MAX_CU_SIZE / GRID_SIZE], ChannelType> m_transformEdge;

  PelStorage                   m_encPicYuvBuffer;
  bool                         m_enc;
private:
  static PosType getPos(const Position &p, EdgeDir dir) { return dir == EdgeDir::VER ? p.x : p.y; }

  void clearFilterLengthAndTransformEdge();

  // set / get functions
  void xSetDeblockingFilterParam        ( const CodingUnit& cu );

  // filtering functions
  EdgeStrengths xGetBoundaryStrengthSingle(const CodingUnit &cu, EdgeDir edgeDir, const Position &localPos,
                                           const ChannelType chType) const;

  void xSetEdgefilterMultiple(const CodingUnit &cu, EdgeDir edgeDir, const Area &area, const bool value,
                              const bool isTransEdge);
  void xEdgeFilterLuma(const CodingUnit &cu, EdgeDir edgeDir, const int edgeIdx);
  void xEdgeFilterChroma(const CodingUnit &cu, EdgeDir edgeDir, const int edgeIdx);

  int  deriveLADFShift(const Pel *src, const ptrdiff_t stride, EdgeDir edgeDir, const SPS *sps);
  void xSetMaxFilterLengthPQFromTransformSizes(EdgeDir edgeDir, const CodingUnit &cu, const TransformUnit &currTU,
                                               const int firstComponent);
  void xSetMaxFilterLengthPQForCodingSubBlocks(EdgeDir edgeDir, const CodingUnit &cu, const PredictionUnit &currPU,
                                               const bool &mvSubBlocks, const Area &areaPu);

  static void xFilteringPandQ(Pel *src, ptrdiff_t offset, FilterLenPair filterLen, int tc);
  static void xPelFilterLuma(Pel *src, const ptrdiff_t offset, const int tc, const bool sw, const bool partPNoFilter,
                             const bool partQNoFilter, const int thrCut, const bool bFilterSecondP,
                             const bool bFilterSecondQ, const ClpRng &clpRng, bool sidePisLarge = false,
                             bool sideQisLarge = false, FilterLenPair maxFilterLen = DEFAULT_FL2);
  inline void xPelFilterChroma(Pel *src, const ptrdiff_t offset, const int tc, const bool sw, const bool partPNoFilter,
                               const bool partQNoFilter, const ClpRng &clpRng, const bool largeBoundary,
                               const bool isChromaHorCTBBoundary) const;

  inline bool xUseStrongFiltering(Pel *src, const ptrdiff_t offset, const int d, const int beta, const int tc,
                                  bool sidePisLarge = false, bool sideQisLarge = false,
                                  FilterLenPair maxFilterLen = DEFAULT_FL2, bool isChromaHorCTBBoundary = false) const;

  inline bool isCrossedByVirtualBoundaries ( const int xPos, const int yPos, const int width, const int height, int& numHorVirBndry, int& numVerVirBndry, int horVirBndryPos[], int verVirBndryPos[], const PicHeader* picHeader );
  inline void xDeriveEdgefilterParam       ( const int xPos, const int yPos, const int numVerVirBndry, const int numHorVirBndry, const int verVirBndryPos[], const int horVirBndryPos[], bool &verEdgeFilter, bool &horEdgeFilter );

  inline int xCalcDP(Pel *src, const ptrdiff_t offset, const bool isChromaHorCTBBoundary = false) const;
  inline int xCalcDQ(Pel *src, const ptrdiff_t offset) const;

  static const uint16_t sm_tcTable[MAX_QP + 3];
  static const uint8_t sm_betaTable[MAX_QP + 1];

public:

  DeblockingFilter();
  ~DeblockingFilter();

  /// CU-level deblocking function
  void deblockCu(CodingUnit &cu, EdgeDir edgeDir);
  void initEncPicYuvBuffer(ChromaFormat chromaFormat, const Size &size, const unsigned maxCUSize);

  PelStorage& getDbEncPicYuvBuffer() { return m_encPicYuvBuffer; }
  void  setEnc(bool b) { m_enc = b; }

  void  create(const unsigned maxCUDepth);
  void  destroy                   ();

  /// picture-level deblocking filter
  void deblockingFilterPic        ( CodingStructure& cs );

  static int getBeta              ( const int qp )
  {
    const int indexB = Clip3( 0, MAX_QP, qp );
    return sm_betaTable[ indexB ];
  }

  void resetBsAndEdgeFilter(EdgeDir edgeDir);
  void resetFilterLengths();
};

//! \}

#endif
