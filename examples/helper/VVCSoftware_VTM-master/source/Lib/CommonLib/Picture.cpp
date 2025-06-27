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

/** \file     Picture.cpp
 *  \brief    Description of a coded picture
 */

#include "Picture.h"
#include "SEI.h"
#include "ChromaFormat.h"
#include "CommonLib/InterpolationFilter.h"

// ---------------------------------------------------------------------------
// picture methods
// ---------------------------------------------------------------------------



Picture::Picture()
{
  cs                   = nullptr;
  m_isSubPicBorderSaved = false;
  m_extendedBorder        = false;
  m_wrapAroundValid    = false;
  m_wrapAroundOffset   = 0;
  usedByCurr           = false;
  longTerm             = false;
  reconstructed        = false;
  neededForOutput      = false;
  referenced           = false;
  temporalId           = std::numeric_limits<uint32_t>::max();
  fieldPic             = false;
  topField             = false;
  precedingDRAP        = false;
  edrapRapId           = -1;
  m_colourTranfParams     = nullptr;
  nonReferencePictureFlag = false;
  isEosPresentInPic    = false;

  m_prevQP.fill(-1);
  m_spliceIdx           = nullptr;
  m_ctuNums = 0;
  layerId = NOT_VALID;
  numSlices = 1;
  unscaledPic = nullptr;
  m_grainCharacteristic = nullptr;
  m_grainBuf            = nullptr;
}

void Picture::create(const bool useWrapAround, const ChromaFormat& _chromaFormat, const Size& size,
                     const unsigned _maxCUSize, const unsigned _margin, const bool _decoder, const int _layerId,
                     const bool enablePostFilteringForHFR)
{
  layerId = _layerId;
  UnitArea::operator=( UnitArea( _chromaFormat, Area( Position{ 0, 0 }, size ) ) );
  margin            =  MAX_SCALING_RATIO*_margin;
  const Area a      = Area( Position(), size );
  M_BUFS( 0, PIC_RECONSTRUCTION ).create( _chromaFormat, a, _maxCUSize, margin, MEMORY_ALIGN_DEF_SIZE );

  if (useWrapAround)
  {
    M_BUFS(0, PIC_RECON_WRAP).create(_chromaFormat, a, _maxCUSize, margin, MEMORY_ALIGN_DEF_SIZE);
  }

  if (enablePostFilteringForHFR)
  {
    M_BUFS(0, PIC_YUV_POST_REC).create(_chromaFormat, a, _maxCUSize, margin, MEMORY_ALIGN_DEF_SIZE);
  }

  if( !_decoder )
  {
    M_BUFS( 0, PIC_ORIGINAL ).    create( _chromaFormat, a );
  }
#if !KEEP_PRED_AND_RESI_SIGNALS
  m_ctuArea = UnitArea( _chromaFormat, Area( Position{ 0, 0 }, Size( _maxCUSize, _maxCUSize ) ) );
#endif
  m_hashMap.clearAll();
}

void Picture::destroy()
{
  for (uint32_t t = 0; t < NUM_PIC_TYPES; t++)
  {
    M_BUFS(jId, t).destroy();
  }
  m_hashMap.clearAll();
  if (cs)
  {
    cs->destroy();
    delete cs;
    cs = nullptr;
  }

  for (auto &ps: slices)
  {
    delete ps;
  }
  slices.clear();

  for (auto &psei: SEIs)
  {
    delete psei;
  }
  SEIs.clear();

  for (auto &nnpfc: m_nnpfcActivated)
  {
    delete nnpfc;
  }
  m_nnpfcActivated.clear();

  if (m_spliceIdx)
  {
    delete[] m_spliceIdx;
    m_spliceIdx = nullptr;
  }
  m_invColourTransfBuf = nullptr;
  m_grainBuf           = nullptr;
}

void Picture::createTempBuffers( const unsigned _maxCUSize, bool useFilterFrame, bool resChange, bool decoder, bool isFgFiltered)
{
#if KEEP_PRED_AND_RESI_SIGNALS
  const Area a( Position{ 0, 0 }, lumaSize() );
#else
  const Area a = m_ctuArea.Y();
#endif

  M_BUFS( jId, PIC_PREDICTION                   ).create( chromaFormat, a,   _maxCUSize );
  M_BUFS( jId, PIC_RESIDUAL                     ).create( chromaFormat, a,   _maxCUSize );

  if (!decoder)
  {
    const Area picArea(Position{ 0, 0 }, lumaSize());
    M_BUFS(jId, PIC_TRUE_ORIGINAL).create(chromaFormat, picArea, _maxCUSize);
    if (useFilterFrame)
    {
      M_BUFS(jId, PIC_FILTERED_ORIGINAL).create(chromaFormat, picArea, _maxCUSize);
    }
    if (resChange)
    {
      const Area aInput(Position{ 0, 0 }, Size(M_BUFS(jId, PIC_ORIGINAL_INPUT).Y().width, M_BUFS(jId, PIC_ORIGINAL_INPUT).Y().height));
      M_BUFS(jId, PIC_TRUE_ORIGINAL_INPUT).create(chromaFormat, aInput, _maxCUSize);
      if (useFilterFrame)
      {
        M_BUFS(jId, PIC_FILTERED_ORIGINAL_INPUT).create(chromaFormat, aInput, _maxCUSize);
      }
    }
    if (isFgFiltered)
    {
      M_BUFS(jId, PIC_FILTERED_ORIGINAL_FG).create(chromaFormat, picArea, _maxCUSize);
    }
  }

  if (cs)
  {
    cs->rebindPicBufs();
  }
}

void Picture::destroyTempBuffers()
{
  for (uint32_t t = 0; t < NUM_PIC_TYPES; t++)
  {
    if (t == PIC_RESIDUAL || t == PIC_PREDICTION
      || t == PIC_FILTERED_ORIGINAL || t == PIC_TRUE_ORIGINAL || t == PIC_FILTERED_ORIGINAL_FG
      || t == PIC_TRUE_ORIGINAL_INPUT || t == PIC_FILTERED_ORIGINAL_INPUT)
    {
      M_BUFS(0, t).destroy();
    }
  }

  if (cs)
  {
    cs->rebindPicBufs();
  }
}

       PelBuf     Picture::getOrigBuf(const CompArea &blk)        { return getBuf(blk,  PIC_ORIGINAL); }
const CPelBuf     Picture::getOrigBuf(const CompArea &blk)  const { return getBuf(blk,  PIC_ORIGINAL); }
       PelUnitBuf Picture::getOrigBuf(const UnitArea &unit)       { return getBuf(unit, PIC_ORIGINAL); }
const CPelUnitBuf Picture::getOrigBuf(const UnitArea &unit) const { return getBuf(unit, PIC_ORIGINAL); }
       PelUnitBuf Picture::getOrigBuf()                           { return M_BUFS(0,    PIC_ORIGINAL); }
const CPelUnitBuf Picture::getOrigBuf()                     const { return M_BUFS(0,    PIC_ORIGINAL); }

       PelBuf     Picture::getOrigBuf(const ComponentID compID)       { return getBuf(compID, PIC_ORIGINAL); }
const CPelBuf     Picture::getOrigBuf(const ComponentID compID) const { return getBuf(compID, PIC_ORIGINAL); }
       PelBuf     Picture::getTrueOrigBuf(const ComponentID compID)       { return getBuf(compID, PIC_TRUE_ORIGINAL); }
const CPelBuf     Picture::getTrueOrigBuf(const ComponentID compID) const { return getBuf(compID, PIC_TRUE_ORIGINAL); }
       PelUnitBuf Picture::getTrueOrigBuf()                           { return M_BUFS(0, PIC_TRUE_ORIGINAL); }
const CPelUnitBuf Picture::getTrueOrigBuf()                     const { return M_BUFS(0, PIC_TRUE_ORIGINAL); }
       PelBuf     Picture::getTrueOrigBuf(const CompArea &blk)        { return getBuf(blk, PIC_TRUE_ORIGINAL); }
const CPelBuf     Picture::getTrueOrigBuf(const CompArea &blk)  const { return getBuf(blk, PIC_TRUE_ORIGINAL); }

       PelUnitBuf Picture::getFilteredOrigBuf()                           { return M_BUFS(0, PIC_FILTERED_ORIGINAL); }
const CPelUnitBuf Picture::getFilteredOrigBuf()                     const { return M_BUFS(0, PIC_FILTERED_ORIGINAL); }
       PelBuf     Picture::getFilteredOrigBuf(const CompArea &blk)        { return getBuf(blk, PIC_FILTERED_ORIGINAL); }
const CPelBuf     Picture::getFilteredOrigBuf(const CompArea &blk)  const { return getBuf(blk, PIC_FILTERED_ORIGINAL); }

       PelBuf     Picture::getPredBuf(const CompArea &blk)        { return getBuf(blk,  PIC_PREDICTION); }
const CPelBuf     Picture::getPredBuf(const CompArea &blk)  const { return getBuf(blk,  PIC_PREDICTION); }
       PelUnitBuf Picture::getPredBuf(const UnitArea &unit)       { return getBuf(unit, PIC_PREDICTION); }
const CPelUnitBuf Picture::getPredBuf(const UnitArea &unit) const { return getBuf(unit, PIC_PREDICTION); }

       PelBuf     Picture::getResiBuf(const CompArea &blk)        { return getBuf(blk,  PIC_RESIDUAL); }
const CPelBuf     Picture::getResiBuf(const CompArea &blk)  const { return getBuf(blk,  PIC_RESIDUAL); }
       PelUnitBuf Picture::getResiBuf(const UnitArea &unit)       { return getBuf(unit, PIC_RESIDUAL); }
const CPelUnitBuf Picture::getResiBuf(const UnitArea &unit) const { return getBuf(unit, PIC_RESIDUAL); }

       PelBuf     Picture::getRecoBuf(const ComponentID compID, bool wrap)       { return getBuf(compID,                    wrap ? PIC_RECON_WRAP : PIC_RECONSTRUCTION); }
const CPelBuf     Picture::getRecoBuf(const ComponentID compID, bool wrap) const { return getBuf(compID,                    wrap ? PIC_RECON_WRAP : PIC_RECONSTRUCTION); }
       PelBuf     Picture::getRecoBuf(const CompArea &blk, bool wrap)            { return getBuf(blk,                       wrap ? PIC_RECON_WRAP : PIC_RECONSTRUCTION); }
const CPelBuf     Picture::getRecoBuf(const CompArea &blk, bool wrap)      const { return getBuf(blk,                       wrap ? PIC_RECON_WRAP : PIC_RECONSTRUCTION); }
       PelUnitBuf Picture::getRecoBuf(const UnitArea &unit, bool wrap)           { return getBuf(unit,                      wrap ? PIC_RECON_WRAP : PIC_RECONSTRUCTION); }
const CPelUnitBuf Picture::getRecoBuf(const UnitArea &unit, bool wrap)     const { return getBuf(unit,                      wrap ? PIC_RECON_WRAP : PIC_RECONSTRUCTION); }
       PelUnitBuf Picture::getRecoBuf(bool wrap)                                 { return M_BUFS(scheduler.getSplitPicId(), wrap ? PIC_RECON_WRAP : PIC_RECONSTRUCTION); }
const CPelUnitBuf Picture::getRecoBuf(bool wrap)                           const { return M_BUFS(scheduler.getSplitPicId(), wrap ? PIC_RECON_WRAP : PIC_RECONSTRUCTION); }

       PelUnitBuf Picture::getPostRecBuf()                           { return M_BUFS(scheduler.getSplitPicId(), PIC_YUV_POST_REC); }
const CPelUnitBuf Picture::getPostRecBuf()                     const { return M_BUFS(scheduler.getSplitPicId(), PIC_YUV_POST_REC); }

void Picture::finalInit( const VPS* vps, const SPS& sps, const PPS& pps, PicHeader *picHeader, APS** alfApss, APS* lmcsAps, APS* scalingListAps )
{
  for( auto &sei : SEIs )
  {
    delete sei;
  }
  SEIs.clear();
  clearSliceBuffer();

  const ChromaFormat chromaFormatIdc = sps.getChromaFormatIdc();
  const int          width           = pps.getPicWidthInLumaSamples();
  const int          height          = pps.getPicHeightInLumaSamples();

  if (cs == nullptr)
  {
    cs      = new CodingStructure(g_xuPool);
    cs->create(chromaFormatIdc, Area(0, 0, width, height), true, (bool) sps.getPLTMode());
  }

  cs->sps = &sps;
  cs->vps = vps;
  cs->picture = this;
  cs->slice   = nullptr;  // the slices for this picture have not been set at this point. update cs->slice after swapSliceObject()
  cs->pps     = &pps;
  picHeader->setSPSId( sps.getSPSId() );
  picHeader->setPPSId( pps.getPPSId() );
  cs->picHeader = picHeader;

  memcpy(cs->alfApss, alfApss, sizeof(cs->alfApss));
  cs->lmcsAps = lmcsAps;
  cs->scalinglistAps = scalingListAps;
  cs->pcv     = pps.pcv;
  m_conformanceWindow = pps.getConformanceWindow();
  m_scalingWindow = pps.getScalingWindow();
  mixedNaluTypesInPicFlag = pps.getMixedNaluTypesInPicFlag();
  nonReferencePictureFlag = picHeader->getNonReferencePictureFlag();
  m_chromaFormatIdc       = sps.getChromaFormatIdc();
  m_bitDepths = sps.getBitDepths();

  if (m_spliceIdx == nullptr)
  {
    m_ctuNums = cs->pcv->sizeInCtus;
    m_spliceIdx = new int[m_ctuNums];
    std::fill_n(m_spliceIdx, m_ctuNums, 0);
  }
}

void Picture::allocateNewSlice()
{
  slices.push_back(new Slice);
  Slice& slice = *slices.back();
  memcpy(slice.getAlfAPSs(), cs->alfApss, sizeof(cs->alfApss));

  slice.setPPS( cs->pps);
  slice.setSPS( cs->sps);
  slice.setVPS( cs->vps);
  if(slices.size()>=2)
  {
    slice.copySliceInfo( slices[slices.size()-2] );
    slice.initSlice();
  }
}

void Picture::fillSliceLossyLosslessArray(std::vector<uint16_t> sliceLosslessIndexArray, bool mixedLossyLossless)
{
  uint16_t numElementsinsliceLosslessIndexArray = (uint16_t)sliceLosslessIndexArray.size();
  uint32_t numSlices = this->cs->pps->getNumSlicesInPic();
  m_lossylosslessSliceArray.assign(numSlices, true); // initialize to all slices are lossless
  if (mixedLossyLossless)
  {
    m_lossylosslessSliceArray.assign(numSlices, false); // initialize to all slices are lossless
    CHECK(numElementsinsliceLosslessIndexArray == 0 , "sliceLosslessArray is empty, must need to configure for mixed lossy/lossless");

    // mixed lossy/lossless slices, set only lossless slices;
    for (uint16_t i = 0; i < numElementsinsliceLosslessIndexArray; i++)
    {
      CHECK(sliceLosslessIndexArray[i] >= numSlices || sliceLosslessIndexArray[i] < 0,
            "index of lossless slice is out of slice index bound");
      m_lossylosslessSliceArray[sliceLosslessIndexArray[i]] = true;
    }
  }
  CHECK(m_lossylosslessSliceArray.size() < numSlices, "sliceLosslessArray size is less than number of slices");
}

Slice *Picture::swapSliceObject(Slice * p, uint32_t i)
{
  p->setSPS(cs->sps);
  p->setPPS(cs->pps);
  p->setVPS(cs->vps);
  p->setAlfAPSs(cs->alfApss);


  Slice * pTmp = slices[i];
  slices[i] = p;
  pTmp->setSPS(0);
  pTmp->setPPS(0);
  pTmp->setVPS(0);
  memset(pTmp->getAlfAPSs(), 0, sizeof(*pTmp->getAlfAPSs())*ALF_CTB_MAX_NUM_APS);

  return pTmp;
}

void Picture::clearSliceBuffer()
{
  for (uint32_t i = 0; i < uint32_t(slices.size()); i++)
  {
    delete slices[i];
  }
  slices.clear();
}

const TFilterCoeff DownsamplingFilterSRC[8][16][12] =
{
    { // D = 1
      {   0,   0,   0,   0,   0, 128,   0,   0,   0,   0,   0,   0 },
      {   0,   0,   0,   2,  -6, 127,   7,  -2,   0,   0,   0,   0 },
      {   0,   0,   0,   3, -12, 125,  16,  -5,   1,   0,   0,   0 },
      {   0,   0,   0,   4, -16, 120,  26,  -7,   1,   0,   0,   0 },
      {   0,   0,   0,   5, -18, 114,  36, -10,   1,   0,   0,   0 },
      {   0,   0,   0,   5, -20, 107,  46, -12,   2,   0,   0,   0 },
      {   0,   0,   0,   5, -21,  99,  57, -15,   3,   0,   0,   0 },
      {   0,   0,   0,   5, -20,  89,  68, -18,   4,   0,   0,   0 },
      {   0,   0,   0,   4, -19,  79,  79, -19,   4,   0,   0,   0 },
      {   0,   0,   0,   4, -18,  68,  89, -20,   5,   0,   0,   0 },
      {   0,   0,   0,   3, -15,  57,  99, -21,   5,   0,   0,   0 },
      {   0,   0,   0,   2, -12,  46, 107, -20,   5,   0,   0,   0 },
      {   0,   0,   0,   1, -10,  36, 114, -18,   5,   0,   0,   0 },
      {   0,   0,   0,   1,  -7,  26, 120, -16,   4,   0,   0,   0 },
      {   0,   0,   0,   1,  -5,  16, 125, -12,   3,   0,   0,   0 },
      {   0,   0,   0,   0,  -2,   7, 127,  -6,   2,   0,   0,   0 }
    },
    { // Kaiser(7)-windowed sinc ratio 1.35
      {   0,   0,   4, -14,  27,  94,  27, -14,   4,   0,   0,   0 },
      {   0,   0,   4, -13,  21,  94,  32, -14,   3,   1,   0,   0 },
      {   0,   0,   4, -12,  16,  93,  39, -15,   3,   1,  -1,   0 },
      {   0,   0,   4, -11,  11,  92,  45, -15,   2,   1,  -1,   0 },
      {   0,  -1,   4, -10,   7,  89,  51, -14,   1,   2,  -1,   0 },
      {   0,  -1,   4,  -8,   2,  86,  57, -13,   0,   2,  -1,   0 },
      {   0,  -1,   4,  -7,  -1,  82,  63, -12,  -1,   2,  -1,   0 },
      {   0,  -1,   4,  -5,  -5,  78,  68, -10,  -3,   3,  -1,   0 },
      {   0,  -1,   3,  -4,  -8,  74,  74,  -8,  -4,   3,  -1,   0 },
      {   0,  -1,   3,  -3, -10,  68,  78,  -5,  -5,   4,  -1,   0 },
      {   0,  -1,   2,  -1, -12,  63,  82,  -1,  -7,   4,  -1,   0 },
      {   0,  -1,   2,   0, -13,  57,  86,   2,  -8,   4,  -1,   0 },
      {   0,  -1,   2,   1, -14,  51,  89,   7, -10,   4,  -1,   0 },
      {   0,  -1,   1,   2, -15,  45,  92,  11, -11,   4,   0,   0 },
      {   0,  -1,   1,   3, -15,  39,  93,  16, -12,   4,   0,   0 },
      {   0,   0,   1,   3, -14,  32,  94,  21, -13,   4,   0,   0 }
    },
    { // D = 2
      {   0,   5,   -6,  -10,  37,  76,   37,  -10,  -6,    5,  0,   0}, //0
      {   0,   5,   -4,  -11,  33,  76,   40,  -9,    -7,    5,  0,   0}, //1
      //{   0,   5,   -3,  -12,  28,  75,   44,  -7,    -8,    5,  1,   0}, //2
      {  -1,   5,   -3,  -12,  29,  75,   45,  -7,    -8,   5,  0,   0}, //2 new coefficients in m24499
      {  -1,   4,   -2,  -13,  25,  75,   48,  -5,    -9,    5,  1,   0}, //3
      {  -1,   4,   -1,  -13,  22,  73,   52,  -3,    -10,  4,  1,   0}, //4
      {  -1,   4,   0,    -13,  18,  72,   55,  -1,    -11,  4,  2,  -1}, //5
      {  -1,   4,   1,    -13,  14,  70,   59,  2,    -12,  3,  2,  -1}, //6
      {  -1,   3,   1,    -13,  11,  68,   62,  5,    -12,  3,  2,  -1}, //7
      {  -1,   3,   2,    -13,  8,  65,   65,  8,    -13,  2,  3,  -1}, //8
      {  -1,   2,   3,    -12,  5,  62,   68,  11,    -13,  1,  3,  -1}, //9
      {  -1,   2,   3,    -12,  2,  59,   70,  14,    -13,  1,  4,  -1}, //10
      {  -1,   2,   4,    -11,  -1,  55,   72,  18,    -13,  0,  4,  -1}, //11
      {   0,   1,   4,    -10,  -3,  52,   73,  22,    -13,  -1,  4,  -1}, //12
      {   0,   1,   5,    -9,    -5,  48,   75,  25,    -13,  -2,  4,  -1}, //13
      //{   0,   1,   5,    -8,    -7,  44,   75,  28,    -12,  -3,  5,   0}, //14
      {    0,   0,   5,    -8,   -7,  45,   75,  29,    -12,  -3,  5,  -1}  , //14 new coefficients in m24499
      {   0,   0,   5,    -7,    -9,  40,   76,  33,    -11,  -4,  5,   0}, //15
    },
    { // D = 2.5
      {   2,  -3,   -9,  6,   39,  58,   39,  6,   -9,  -3,    2,    0}, // 0
      {   2,  -3,   -9,  4,   38,  58,   43,  7,   -9,  -4,    1,    0}, // 1
      {   2,  -2,   -9,  2,   35,  58,   44,  9,   -8,  -4,    1,    0}, // 2
      {   1,  -2,   -9,  1,   34,  58,   46,  11,   -8,  -5,    1,    0}, // 3
      //{   1,  -1,   -8,  -1,   31,  57,   48,  13,   -8,  -5,    1,    0}, // 4
      {   1,  -1,   -8,  -1,   31,  57,   47,  13,   -7,  -5,    1,    0},  // 4 new coefficients in m24499
      {   1,  -1,   -8,  -2,   29,  56,   49,  15,   -7,  -6,    1,    1}, // 5
      {   1,  0,   -8,  -3,   26,  55,   51,  17,   -7,  -6,    1,    1}, // 6
      {   1,  0,   -7,  -4,   24,  54,   52,  19,   -6,  -7,    1,    1}, // 7
      {   1,  0,   -7,  -5,   22,  53,   53,  22,   -5,  -7,    0,    1}, // 8
      {   1,  1,   -7,  -6,   19,  52,   54,  24,   -4,  -7,    0,    1}, // 9
      {   1,  1,   -6,  -7,   17,  51,   55,  26,   -3,  -8,    0,    1}, // 10
      {   1,  1,   -6,  -7,   15,  49,   56,  29,   -2,  -8,    -1,    1}, // 11
      //{   0,  1,   -5,  -8,   13,  48,   57,  31,   -1,  -8,    -1,    1}, // 12 new coefficients in m24499
      {   0,  1,   -5,  -7,   13,  47,  57,  31,  -1,    -8,   -1,    1}, // 12
      {   0,  1,   -5,  -8,   11,  46,   58,  34,   1,    -9,    -2,    1}, // 13
      {   0,  1,   -4,  -8,   9,    44,   58,  35,   2,    -9,    -2,    2}, // 14
      {   0,  1,   -4,  -9,   7,    43,   58,  38,   4,    -9,    -3,    2}, // 15
    },
    { // D = 3
      {  -2,  -7,   0,  17,  35,  43,  35,  17,   0,  -7,  -5,   2 },
      {  -2,  -7,  -1,  16,  34,  43,  36,  18,   1,  -7,  -5,   2 },
      {  -1,  -7,  -1,  14,  33,  43,  36,  19,   1,  -6,  -5,   2 },
      {  -1,  -7,  -2,  13,  32,  42,  37,  20,   3,  -6,  -5,   2 },
      {   0,  -7,  -3,  12,  31,  42,  38,  21,   3,  -6,  -5,   2 },
      {   0,  -7,  -3,  11,  30,  42,  39,  23,   4,  -6,  -6,   1 },
      {   0,  -7,  -4,  10,  29,  42,  40,  24,   5,  -6,  -6,   1 },
      {   1,  -7,  -4,   9,  27,  41,  40,  25,   6,  -5,  -6,   1 },
      {   1,  -6,  -5,   7,  26,  41,  41,  26,   7,  -5,  -6,   1 },
      {   1,  -6,  -5,   6,  25,  40,  41,  27,   9,  -4,  -7,   1 },
      {   1,  -6,  -6,   5,  24,  40,  42,  29,  10,  -4,  -7,   0 },
      {   1,  -6,  -6,   4,  23,  39,  42,  30,  11,  -3,  -7,   0 },
      {   2,  -5,  -6,   3,  21,  38,  42,  31,  12,  -3,  -7,   0 },
      {   2,  -5,  -6,   3,  20,  37,  42,  32,  13,  -2,  -7,  -1 },
      {   2,  -5,  -6,   1,  19,  36,  43,  33,  14,  -1,  -7,  -1 },
      {   2,  -5,  -7,   1,  18,  36,  43,  34,  16,  -1,  -7,  -2 }
    },
    { // D = 3.5
      {  -6,  -3,   5,  19,  31,  36,  31,  19,   5,  -3,  -6,   0 },
      {  -6,  -4,   4,  18,  31,  37,  32,  20,   6,  -3,  -6,  -1 },
      {  -6,  -4,   4,  17,  30,  36,  33,  21,   7,  -3,  -6,  -1 },
      {  -5,  -5,   3,  16,  30,  36,  33,  22,   8,  -2,  -6,  -2 },
      {  -5,  -5,   2,  15,  29,  36,  34,  23,   9,  -2,  -6,  -2 },
      {  -5,  -5,   2,  15,  28,  36,  34,  24,  10,  -2,  -6,  -3 },
      {  -4,  -5,   1,  14,  27,  36,  35,  24,  10,  -1,  -6,  -3 },
      {  -4,  -5,   0,  13,  26,  35,  35,  25,  11,   0,  -5,  -3 },
      {  -4,  -6,   0,  12,  26,  36,  36,  26,  12,   0,  -6,  -4 },
      {  -3,  -5,   0,  11,  25,  35,  35,  26,  13,   0,  -5,  -4 },
      {  -3,  -6,  -1,  10,  24,  35,  36,  27,  14,   1,  -5,  -4 },
      {  -3,  -6,  -2,  10,  24,  34,  36,  28,  15,   2,  -5,  -5 },
      {  -2,  -6,  -2,   9,  23,  34,  36,  29,  15,   2,  -5,  -5 },
      {  -2,  -6,  -2,   8,  22,  33,  36,  30,  16,   3,  -5,  -5 },
      {  -1,  -6,  -3,   7,  21,  33,  36,  30,  17,   4,  -4,  -6 },
      {  -1,  -6,  -3,   6,  20,  32,  37,  31,  18,   4,  -4,  -6 }
    },
    { // D = 4
      {  -9,   0,   9,  20,  28,  32,  28,  20,   9,   0,  -9,   0 },
      {  -9,   0,   8,  19,  28,  32,  29,  20,  10,   0,  -4,  -5 },
      {  -9,  -1,   8,  18,  28,  32,  29,  21,  10,   1,  -4,  -5 },
      {  -9,  -1,   7,  18,  27,  32,  30,  22,  11,   1,  -4,  -6 },
      {  -8,  -2,   6,  17,  27,  32,  30,  22,  12,   2,  -4,  -6 },
      {  -8,  -2,   6,  16,  26,  32,  31,  23,  12,   2,  -4,  -6 },
      {  -8,  -2,   5,  16,  26,  31,  31,  23,  13,   3,  -3,  -7 },
      {  -8,  -3,   5,  15,  25,  31,  31,  24,  14,   4,  -3,  -7 },
      {  -7,  -3,   4,  14,  25,  31,  31,  25,  14,   4,  -3,  -7 },
      {  -7,  -3,   4,  14,  24,  31,  31,  25,  15,   5,  -3,  -8 },
      {  -7,  -3,   3,  13,  23,  31,  31,  26,  16,   5,  -2,  -8 },
      {  -6,  -4,   2,  12,  23,  31,  32,  26,  16,   6,  -2,  -8 },
      {  -6,  -4,   2,  12,  22,  30,  32,  27,  17,   6,  -2,  -8 },
      {  -6,  -4,   1,  11,  22,  30,  32,  27,  18,   7,  -1,  -9 },
      {  -5,  -4,   1,  10,  21,  29,  32,  28,  18,   8,  -1,  -9 },
      {  -5,  -4,   0,  10,  20,  29,  32,  28,  19,   8,   0,  -9 }
    },
    { // D = 5.5
      {  -8,   7,  13,  18,  22,  24,  22,  18,  13,   7,   2, -10 },
      {  -8,   7,  13,  18,  22,  23,  22,  19,  13,   7,   2, -10 },
      {  -8,   6,  12,  18,  22,  23,  22,  19,  14,   8,   2, -10 },
      {  -9,   6,  12,  17,  22,  23,  23,  19,  14,   8,   3, -10 },
      {  -9,   6,  12,  17,  21,  23,  23,  19,  14,   9,   3, -10 },
      {  -9,   5,  11,  17,  21,  23,  23,  20,  15,   9,   3, -10 },
      {  -9,   5,  11,  16,  21,  23,  23,  20,  15,   9,   4, -10 },
      {  -9,   5,  10,  16,  21,  23,  23,  20,  15,  10,   4, -10 },
      { -10,   5,  10,  16,  20,  23,  23,  20,  16,  10,   5, -10 },
      { -10,   4,  10,  15,  20,  23,  23,  21,  16,  10,   5,  -9 },
      { -10,   4,   9,  15,  20,  23,  23,  21,  16,  11,   5,  -9 },
      { -10,   3,   9,  15,  20,  23,  23,  21,  17,  11,   5,  -9 },
      { -10,   3,   9,  14,  19,  23,  23,  21,  17,  12,   6,  -9 },
      { -10,   3,   8,  14,  19,  23,  23,  22,  17,  12,   6,  -9 },
      { -10,   2,   8,  14,  19,  22,  23,  22,  18,  12,   6,  -8 },
      { -10,   2,   7,  13,  19,  22,  23,  22,  18,  13,   7,  -8 }
    }
};

const TFilterCoeff m_lumaFilter12_alt[16][12] =
{
{ 0, 0, 0, 0, 0, 256, 0, 0, 0, 0, 0, 0, },
{ 1, -1, 0, 3, -12, 253, 16, -6, 2, 0, 0, 0, },
{ 0, 0, -3, 9, -24, 250, 32, -11, 4, -1, 0, 0, },
{ 0, 0, -4, 12, -32, 241, 52, -18, 8, -4, 2, -1, },
{ 0, 1, -6, 15, -38, 228, 75, -28, 14, -7, 3, -1, },
{ 0, 1, -7, 18, -43, 214, 96, -33, 16, -8, 3, -1, },
{ 1, 0, -6, 17, -44, 196, 119, -40, 20, -10, 4, -1, },
{ 0, 2, -9, 21, -47, 180, 139, -43, 20, -10, 4, -1, },
{ -1, 3, -9, 21, -46, 160, 160, -46, 21, -9, 3, -1, },
{ -1, 4, -10, 20, -43, 139, 180, -47, 21, -9, 2, 0, },
{ -1, 4, -10, 20, -40, 119, 196, -44, 17, -6, 0, 1, },
{ -1, 3, -8, 16, -33, 96, 214, -43, 18, -7, 1, 0, },
{ -1, 3, -7, 14, -28, 75, 228, -38, 15, -6, 1, 0, },
{ -1, 2, -4, 8, -18, 52, 241, -32, 12, -4, 0, 0, },
{ 0, 0, -1, 4, -11, 32, 250, -24, 9, -3, 0, 0, },
{ 0, 0, 0, 2, -6, 16, 253, -12, 3, 0, -1, 1, },
};
const TFilterCoeff m_chromaFilter6_alt[32][6] =
{
{0, 0, 256, 0, 0, 0, },
{ 1, -6, 256, 6, -1, 0, },
{ 2, -11, 254, 14, -4, 1, },
{ 4, -18, 252, 23, -6, 1, },
{ 6, -24, 249, 32, -9, 2, },
{ 6, -26, 244, 41, -12, 3, },
{ 7, -30, 239, 53, -18, 5, },
{ 8, -34, 235, 61, -19, 5, },
{ 10, -38, 228, 72, -22, 6, },
{ 10, -39, 220, 84, -26, 7, },
{ 10, -40, 213, 94, -29, 8, },
{ 11, -42, 205, 105, -32, 9, },
{ 11, -42, 196, 116, -35, 10, },
{ 11, -42, 186, 128, -37, 10, },
{ 11, -42, 177, 138, -38, 10, },
{ 11, -41, 167, 148, -40, 11, },
{ 11, -41, 158, 158, -41, 11, },
{ 11, -40, 148, 167, -41, 11, },
{ 10, -38, 138, 177, -42, 11, },
{ 10, -37, 128, 186, -42, 11, },
{ 10, -35, 116, 196, -42, 11, },
{ 9, -32, 105, 205, -42, 11, },
{ 8, -29, 94, 213, -40, 10, },
{ 7, -26, 84, 220, -39, 10, },
{ 6, -22, 72, 228, -38, 10, },
{ 5, -19, 61, 235, -34, 8, },
{ 5, -18, 53, 239, -30, 7, },
{ 3, -12, 41, 244, -26, 6, },
{ 2, -9, 32, 249, -24, 6, },
{ 1, -6, 23, 252, -18, 4, },
{ 1, -4, 14, 254, -11, 2, },
{ 0, -1, 6, 256, -6, 1, }
};

const TFilterCoeff m_lumaFilter12[16][12] =
{
    { 0,     0,     0,     0,     0,   256,     0,     0,     0,     0,     0,     0, },
    {-1,     2,    -3,     6,   -14,   254,    16,    -7,     4,    -2,     1,     0, },
    {-1,     3,    -7,    12,   -26,   249,    35,   -15,     8,    -4,     2,     0, },
    {-2,     5,    -9,    17,   -36,   241,    54,   -22,    12,    -6,     3,    -1, },
    {-2,     5,   -11,    21,   -43,   230,    75,   -29,    15,    -8,     4,    -1, },
    {-2,     6,   -13,    24,   -48,   216,    97,   -36,    19,   -10,     4,    -1, },
    {-2,     7,   -14,    25,   -51,   200,   119,   -42,    22,   -12,     5,    -1, },
    {-2,     7,   -14,    26,   -51,   181,   140,   -46,    24,   -13,     6,    -2, },
    {-2,     6,   -13,    25,   -50,   162,   162,   -50,    25,   -13,     6,    -2, },
    {-2,     6,   -13,    24,   -46,   140,   181,   -51,    26,   -14,     7,    -2, },
    {-1,     5,   -12,    22,   -42,   119,   200,   -51,    25,   -14,     7,    -2, },
    {-1,     4,   -10,    19,   -36,    97,   216,   -48,    24,   -13,     6,    -2, },
    {-1,     4,    -8,    15,   -29,    75,   230,   -43,    21,   -11,     5,    -2, },
    {-1,     3,    -6,    12,   -22,    54,   241,   -36,    17,    -9,     5,    -2, },
    { 0,     2,    -4,     8,   -15,    35,   249,   -26,    12,    -7,     3,    -1, },
    { 0,     1,    -2,     4,    -7,    16,   254,   -14,     6,    -3,     2,    -1, },
};

const TFilterCoeff m_chromaFilter6[32][6] =
{
    {0, 0, 256, 0, 0, 0},
    {1, -6, 256, 7, -2, 0},
    {2, -11, 253, 15, -4, 1},
    {3, -16, 251, 23, -6, 1},
    {4, -21, 248, 33, -10, 2},
    {5, -25, 244, 42, -12, 2},
    {7, -30, 239, 53, -17, 4},
    {7, -32, 234, 62, -19, 4},
    {8, -35, 227, 73, -22, 5},
    {9, -38, 220, 84, -26, 7},
    {10, -40, 213, 95, -29, 7},
    {10, -41, 204, 106, -31, 8},
    {10, -42, 196, 117, -34, 9},
    {10, -41, 187, 127, -35, 8},
    {11, -42, 177, 138, -38, 10},
    {10, -41, 168, 148, -39, 10},
    {10, -40, 158, 158, -40, 10},
    {10, -39, 148, 168, -41, 10},
    {10, -38, 138, 177, -42, 11},
    {8, -35, 127, 187, -41, 10},
    {9, -34, 117, 196, -42, 10},
    {8, -31, 106, 204, -41, 10},
    {7, -29, 95, 213, -40, 10},
    {7, -26, 84, 220, -38, 9},
    {5, -22, 73, 227, -35, 8},
    {4, -19, 62, 234, -32, 7},
    {4, -17, 53, 239, -30, 7},
    {2, -12, 42, 244, -25, 5},
    {2, -10, 33, 248, -21, 4},
    {1, -6, 23, 251, -16, 3},
    {1, -4, 15, 253, -11, 2},
    {0, -2, 7, 256, -6, 1},
};

void Picture::sampleRateConv(const ScalingRatio scalingRatio, const int scaleX, const int scaleY,
                             const CPelBuf &beforeScale, const int beforeScaleLeftOffset,
                             const int beforeScaleTopOffset, const PelBuf &afterScale, const int afterScaleLeftOffset,
                             const int afterScaleTopOffset, const int bitDepth, const bool useLumaFilter,
                             const bool downsampling,
                              const bool horCollocatedPositionFlag, const bool verCollocatedPositionFlag,
                              const bool rescaleForDisplay, const int upscaleFilterForDisplay
)
{
  const Pel* orgSrc = beforeScale.buf;
  const int orgWidth = beforeScale.width;
  const int orgHeight = beforeScale.height;
  const ptrdiff_t orgStride = beforeScale.stride;

  Pel* scaledSrc = afterScale.buf;
  const int scaledWidth = afterScale.width;
  const int scaledHeight = afterScale.height;
  const ptrdiff_t scaledStride = afterScale.stride;

  if( orgWidth == scaledWidth && orgHeight == scaledHeight && scalingRatio == SCALE_1X && !beforeScaleLeftOffset && !beforeScaleTopOffset && !afterScaleLeftOffset && !afterScaleTopOffset )
  {
    for( int j = 0; j < orgHeight; j++ )
    {
      memcpy( scaledSrc + j * scaledStride, orgSrc + j * orgStride, sizeof( Pel ) * orgWidth );
    }

    return;
  }

  const TFilterCoeff* filterHor = useLumaFilter ? &InterpolationFilter::m_lumaFilter[0][0] : &InterpolationFilter::m_chromaFilter[0][0];
  const TFilterCoeff* filterVer = useLumaFilter ? &InterpolationFilter::m_lumaFilter[0][0] : &InterpolationFilter::m_chromaFilter[0][0];
  if (rescaleForDisplay)
  {
    if (upscaleFilterForDisplay != 0)
    {
      filterHor = useLumaFilter ? (upscaleFilterForDisplay == 1 ? &m_lumaFilter12_alt[0][0] : &m_lumaFilter12[0][0]) : (upscaleFilterForDisplay == 1 ? &m_chromaFilter6_alt[0][0] : &m_chromaFilter6[0][0]);
      filterVer = useLumaFilter ? (upscaleFilterForDisplay == 1 ? &m_lumaFilter12_alt[0][0] : &m_lumaFilter12[0][0]) : (upscaleFilterForDisplay == 1 ? &m_chromaFilter6_alt[0][0] : &m_chromaFilter6[0][0]);
    }
  }
  const int numFracPositions = useLumaFilter ? 15 : 31;
  const int numFracShift = useLumaFilter ? 4 : 5;

  const int posShiftX = ScalingRatio::BITS - numFracShift + scaleX;
  const int posShiftY = ScalingRatio::BITS - numFracShift + scaleY;

  const int addX =
    (1 << (posShiftX - 1)) + (beforeScaleLeftOffset << ScalingRatio::BITS)
    + ((int(1 - horCollocatedPositionFlag) * 8 * (scalingRatio.x - SCALE_1X.x) + (1 << (2 + scaleX))) >> (3 + scaleX));
  const int addY =
    (1 << (posShiftY - 1)) + (beforeScaleTopOffset << ScalingRatio::BITS)
    + ((int(1 - verCollocatedPositionFlag) * 8 * (scalingRatio.y - SCALE_1X.y) + (1 << (2 + scaleY))) >> (3 + scaleY));

  if( downsampling )
  {
    int verFilter = 0;
    int horFilter = 0;

    if (scalingRatio.x > (15 << ScalingRatio::BITS) / 4)
    {
      horFilter = 7;
    }
    else if (scalingRatio.x > (20 << ScalingRatio::BITS) / 7)
    {
      horFilter = 6;
    }
    else if (scalingRatio.x > (5 << ScalingRatio::BITS) / 2)
    {
      horFilter = 5;
    }
    else if (scalingRatio.x > (2 << ScalingRatio::BITS))
    {
      horFilter = 4;
    }
    else if (scalingRatio.x > (5 << ScalingRatio::BITS) / 3)
    {
      horFilter = 3;
    }
    else if (scalingRatio.x > (27 << ScalingRatio::BITS) / 20)
    {
      horFilter = 2;
    }
    else if (scalingRatio.x > (11 << ScalingRatio::BITS) / 10)
    {
      horFilter = 1;
    }

    if (scalingRatio.y > (15 << ScalingRatio::BITS) / 4)
    {
      verFilter = 7;
    }
    else if (scalingRatio.y > (20 << ScalingRatio::BITS) / 7)
    {
      verFilter = 6;
    }
    else if (scalingRatio.y > (5 << ScalingRatio::BITS) / 2)
    {
      verFilter = 5;
    }
    else if (scalingRatio.y > (2 << ScalingRatio::BITS))
    {
      verFilter = 4;
    }
    else if (scalingRatio.y > (5 << ScalingRatio::BITS) / 3)
    {
      verFilter = 3;
    }
    else if (scalingRatio.y > (27 << ScalingRatio::BITS) / 20)
    {
      verFilter = 2;
    }
    else if (scalingRatio.y > (11 << ScalingRatio::BITS) / 10)
    {
      verFilter = 1;
    }

    filterHor = &DownsamplingFilterSRC[horFilter][0][0];
    filterVer = &DownsamplingFilterSRC[verFilter][0][0];
  }

  int filterLengthsLuma[3] = { 8, 12, 12 };
  int filterLengthsChroma[3] = { 4, 6, 6 };
  int log2NormList[3] = { 12, 16, 16 };
  const int filterLength = downsampling ? 12 : (rescaleForDisplay ? (useLumaFilter ? filterLengthsLuma[upscaleFilterForDisplay] : filterLengthsChroma[upscaleFilterForDisplay]) : useLumaFilter ? NTAPS_LUMA : NTAPS_CHROMA);
  const int log2Norm = downsampling ? 14 : (rescaleForDisplay ? log2NormList[upscaleFilterForDisplay] : 12);
  int *buf = new int[orgHeight * scaledWidth];
  int maxVal = ( 1 << bitDepth ) - 1;

  CHECK( bitDepth > 17, "Overflow may happen!" );

  for( int i = 0; i < scaledWidth; i++ )
  {
    const Pel* org = orgSrc;

    int  refPos  = (((i << scaleX) - afterScaleLeftOffset) * scalingRatio.x + addX) >> posShiftX;
    int integer = refPos >> numFracShift;
    int frac = refPos & numFracPositions;
    int* tmp = buf + i;

    for( int j = 0; j < orgHeight; j++ )
    {
      int sum = 0;
      const TFilterCoeff* f = filterHor + frac * filterLength;

      for( int k = 0; k < filterLength; k++ )
      {
        int xInt = std::min<int>( std::max( 0, integer + k - filterLength / 2 + 1 ), orgWidth - 1 );
        sum += f[k] * org[xInt]; // postpone horizontal filtering gain removal after vertical filtering
      }

      *tmp = sum;

      tmp += scaledWidth;
      org += orgStride;
    }
  }

  Pel* dst = scaledSrc;

  for( int j = 0; j < scaledHeight; j++ )
  {
    int refPos  = (((j << scaleY) - afterScaleTopOffset) * scalingRatio.y + addY) >> posShiftY;
    int integer = refPos >> numFracShift;
    int frac = refPos & numFracPositions;

    for( int i = 0; i < scaledWidth; i++ )
    {
      int sum = 0;
      int* tmp = buf + i;
      const TFilterCoeff* f = filterVer + frac * filterLength;

      for( int k = 0; k < filterLength; k++ )
      {
        int yInt = std::min<int>( std::max( 0, integer + k - filterLength / 2 + 1 ), orgHeight - 1 );
        sum += f[k] * tmp[yInt*scaledWidth];
      }

      dst[i] = std::min<int>( std::max( 0, ( sum + ( 1 << ( log2Norm - 1 ) ) ) >> log2Norm ), maxVal );
    }

    dst += scaledStride;
  }

  delete[] buf;
}

void Picture::rescalePicture(const ScalingRatio scalingRatio, const CPelUnitBuf& beforeScaling,
                             const Window& scalingWindowBefore, const PelUnitBuf& afterScaling,
                             const Window& scalingWindowAfter, const ChromaFormat chromaFormatIdc,
                             const BitDepths& bitDepths, const bool useLumaFilter, const bool downsampling,
                             const bool horCollocatedChromaFlag, const bool verCollocatedChromaFlag,
                             bool rescaleForDisplay, int upscaleFilterForDisplay)
{
  for (int comp = 0; comp < ::getNumberValidComponents(chromaFormatIdc); comp++)
  {
    ComponentID compID = ComponentID( comp );
    const CPelBuf& beforeScale = beforeScaling.get( compID );
    const PelBuf& afterScale = afterScaling.get( compID );

    sampleRateConv(
      scalingRatio, ::getComponentScaleX(compID, chromaFormatIdc), ::getComponentScaleY(compID, chromaFormatIdc),
      beforeScale, scalingWindowBefore.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
      scalingWindowBefore.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc), afterScale,
      scalingWindowAfter.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
      scalingWindowAfter.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc), bitDepths[toChannelType(compID)],
      downsampling || useLumaFilter ? true : isLuma(compID), downsampling, isLuma(compID) ? 1 : horCollocatedChromaFlag,
      isLuma(compID) ? 1 : verCollocatedChromaFlag, rescaleForDisplay, upscaleFilterForDisplay);
  }
}

void Picture::saveSubPicBorder(int POC, int subPicX0, int subPicY0, int subPicWidth, int subPicHeight)
{

  // 1.1 set up margin for back up memory allocation
  int xMargin = margin >> getComponentScaleX(COMPONENT_Y, cs->area.chromaFormat);
  int yMargin = margin >> getComponentScaleY(COMPONENT_Y, cs->area.chromaFormat);

  // 1.2 measure the size of back up memory
  Area areaAboveBelow(0, 0, subPicWidth + 2 * xMargin, yMargin);
  Area areaLeftRight(0, 0, xMargin, subPicHeight);
  UnitArea unitAreaAboveBelow(cs->area.chromaFormat, areaAboveBelow);
  UnitArea unitAreaLeftRight(cs->area.chromaFormat, areaLeftRight);

  // 1.3 create back up memory
  m_bufSubPicAbove.create(unitAreaAboveBelow);
  m_bufSubPicBelow.create(unitAreaAboveBelow);
  m_bufSubPicLeft.create(unitAreaLeftRight);
  m_bufSubPicRight.create(unitAreaLeftRight);
  m_bufWrapSubPicAbove.create(unitAreaAboveBelow);
  m_bufWrapSubPicBelow.create(unitAreaAboveBelow);

  for (int comp = 0; comp < getNumberValidComponents(cs->area.chromaFormat); comp++)
  {
    ComponentID compID = ComponentID(comp);

    // 2.1 measure the margin for each component
    int xmargin = margin >> getComponentScaleX(compID, cs->area.chromaFormat);
    int ymargin = margin >> getComponentScaleY(compID, cs->area.chromaFormat);

    // 2.2 calculate the origin of the subpicture
    int left = subPicX0 >> getComponentScaleX(compID, cs->area.chromaFormat);
    int top = subPicY0 >> getComponentScaleY(compID, cs->area.chromaFormat);

    // 2.3 calculate the width/height of the subPic
    int width = subPicWidth >> getComponentScaleX(compID, cs->area.chromaFormat);
    int height = subPicHeight >> getComponentScaleY(compID, cs->area.chromaFormat);


    // 3.1.1 set reconstructed picture
    PelBuf s = M_BUFS(0, PIC_RECONSTRUCTION).get(compID);
    Pel *src = s.bufAt(left, top);

    // 3.2.1 set back up buffer for left
    PelBuf dBufLeft   = m_bufSubPicLeft.getBuf(compID);
    Pel    *dstLeft   = dBufLeft.bufAt(0, 0);


    // 3.2.2 set back up buffer for right
    PelBuf dBufRight  = m_bufSubPicRight.getBuf(compID);
    Pel    *dstRight  = dBufRight.bufAt(0, 0);

    // 3.2.3 copy to recon picture to back up buffer
    Pel *srcLeft  = src - xmargin;
    Pel *srcRight = src + width;
    for (int y = 0; y < height; y++)
    {
      ::memcpy(dstLeft  + y *  dBufLeft.stride, srcLeft  + y * s.stride, sizeof(Pel) * xmargin);
      ::memcpy(dstRight + y * dBufRight.stride, srcRight + y * s.stride, sizeof(Pel) * xmargin);
    }

    // 3.3.1 set back up buffer for above
    PelBuf dBufTop = m_bufSubPicAbove.getBuf(compID);
    Pel    *dstTop = dBufTop.bufAt(0, 0);

    // 3.3.2 set back up buffer for below
    PelBuf dBufBottom = m_bufSubPicBelow.getBuf(compID);
    Pel    *dstBottom = dBufBottom.bufAt(0, 0);

    // 3.3.3 copy to recon picture to back up buffer
    Pel *srcTop    = src - xmargin - ymargin * s.stride;
    Pel *srcBottom = src - xmargin +  height * s.stride;
    for (int y = 0; y < ymargin; y++)
    {
      ::memcpy(dstTop    + y *    dBufTop.stride, srcTop    + y * s.stride, sizeof(Pel) * (2 * xmargin + width));
      ::memcpy(dstBottom + y * dBufBottom.stride, srcBottom + y * s.stride, sizeof(Pel) * (2 * xmargin + width));
    }

    // back up recon wrap buffer
    if (cs->sps->getWrapAroundEnabledFlag())
    {
      PelBuf sWrap = M_BUFS(0, PIC_RECON_WRAP).get(compID);
      Pel *srcWrap = sWrap.bufAt(left, top);

      // 3.4.1 set back up buffer for above
      PelBuf dBufTopWrap = m_bufWrapSubPicAbove.getBuf(compID);
      Pel    *dstTopWrap = dBufTopWrap.bufAt(0, 0);

      // 3.4.2 set back up buffer for below
      PelBuf dBufBottomWrap = m_bufWrapSubPicBelow.getBuf(compID);
      Pel    *dstBottomWrap = dBufBottomWrap.bufAt(0, 0);

      // 3.4.3 copy recon wrap picture to back up buffer
      Pel *srcTopWrap    = srcWrap - xmargin - ymargin * sWrap.stride;
      Pel *srcBottomWrap = srcWrap - xmargin +  height * sWrap.stride;
      for (int y = 0; y < ymargin; y++)
      {
        ::memcpy(dstTopWrap    + y *    dBufTopWrap.stride, srcTopWrap    + y * sWrap.stride, sizeof(Pel) * (2 * xmargin + width));
        ::memcpy(dstBottomWrap + y * dBufBottomWrap.stride, srcBottomWrap + y * sWrap.stride, sizeof(Pel) * (2 * xmargin + width));
      }
    }
  }
}

void Picture::extendSubPicBorder(int POC, int subPicX0, int subPicY0, int subPicWidth, int subPicHeight)
{

  for (int comp = 0; comp < getNumberValidComponents(cs->area.chromaFormat); comp++)
  {
    ComponentID compID = ComponentID(comp);

    // 2.1 measure the margin for each component
    int xmargin = margin >> getComponentScaleX(compID, cs->area.chromaFormat);
    int ymargin = margin >> getComponentScaleY(compID, cs->area.chromaFormat);

    // 2.2 calculate the origin of the Subpicture
    int left = subPicX0 >> getComponentScaleX(compID, cs->area.chromaFormat);
    int top = subPicY0 >> getComponentScaleY(compID, cs->area.chromaFormat);

    // 2.3 calculate the width/height of the Subpicture
    int width = subPicWidth >> getComponentScaleX(compID, cs->area.chromaFormat);
    int height = subPicHeight >> getComponentScaleY(compID, cs->area.chromaFormat);

    // 3.1 set reconstructed picture
    PelBuf s = M_BUFS(0, PIC_RECONSTRUCTION).get(compID);
    Pel *src = s.bufAt(left, top);

    // 4.1 apply padding for left and right
    {
      Pel *dstLeft  = src - xmargin;
      Pel *dstRight = src + width;
      Pel *srcLeft  = src + 0;
      Pel *srcRight = src + width - 1;

      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < xmargin; x++)
        {
          dstLeft[x]  = *srcLeft;
          dstRight[x] = *srcRight;
        }
        dstLeft += s.stride;
        dstRight += s.stride;
        srcLeft += s.stride;
        srcRight += s.stride;
      }
    }

    // 4.2 apply padding on bottom
    Pel *srcBottom = src + s.stride * (height - 1) - xmargin;
    Pel *dstBottom = srcBottom + s.stride;
    for (int y = 0; y < ymargin; y++)
    {
      ::memcpy(dstBottom, srcBottom, sizeof(Pel)*(2 * xmargin + width));
      dstBottom += s.stride;
    }

    // 4.3 apply padding for top
    // si is still (-marginX, SubpictureHeight-1)
    Pel *srcTop = src - xmargin;
    Pel *dstTop = srcTop - s.stride;
    // si is now (-marginX, 0)
    for (int y = 0; y < ymargin; y++)
    {
      ::memcpy(dstTop, srcTop, sizeof(Pel)*(2 * xmargin + width));
      dstTop -= s.stride;
    }

    // Appy padding for recon wrap buffer
    if (cs->sps->getWrapAroundEnabledFlag())
    {
      // set recon wrap picture
      PelBuf sWrap = M_BUFS(0, PIC_RECON_WRAP).get(compID);
      Pel *srcWrap = sWrap.bufAt(left, top);

      // apply padding on bottom
      Pel *srcBottomWrap = srcWrap + sWrap.stride * (height - 1) - xmargin;
      Pel *dstBottomWrap = srcBottomWrap + sWrap.stride;
      for (int y = 0; y < ymargin; y++)
      {
        ::memcpy(dstBottomWrap, srcBottomWrap, sizeof(Pel)*(2 * xmargin + width));
        dstBottomWrap += sWrap.stride;
      }

      // apply padding for top
      // si is still (-marginX, SubpictureHeight-1)
      Pel *srcTopWrap = srcWrap - xmargin;
      Pel *dstTopWrap = srcTopWrap - sWrap.stride;
      // si is now (-marginX, 0)
      for (int y = 0; y < ymargin; y++)
      {
        ::memcpy(dstTopWrap, srcTopWrap, sizeof(Pel)*(2 * xmargin + width));
        dstTopWrap -= sWrap.stride;
      }
    }
  } // end of for
}

void Picture::restoreSubPicBorder(int POC, int subPicX0, int subPicY0, int subPicWidth, int subPicHeight)
{
  for (int comp = 0; comp < getNumberValidComponents(cs->area.chromaFormat); comp++)
  {
    ComponentID compID = ComponentID(comp);

    // 2.1 measure the margin for each component
    int xmargin = margin >> getComponentScaleX(compID, cs->area.chromaFormat);
    int ymargin = margin >> getComponentScaleY(compID, cs->area.chromaFormat);

    // 2.2 calculate the origin of the subpicture
    int left = subPicX0 >> getComponentScaleX(compID, cs->area.chromaFormat);
    int top = subPicY0 >> getComponentScaleY(compID, cs->area.chromaFormat);

    // 2.3 calculate the width/height of the subpicture
    int width = subPicWidth >> getComponentScaleX(compID, cs->area.chromaFormat);
    int height = subPicHeight >> getComponentScaleY(compID, cs->area.chromaFormat);

    // 3.1 set reconstructed picture
    PelBuf s = M_BUFS(0, PIC_RECONSTRUCTION).get(compID);
    Pel *src = s.bufAt(left, top);

    // 4.2.1 copy from back up buffer to recon picture
    PelBuf dBufLeft = m_bufSubPicLeft.getBuf(compID);
    Pel    *dstLeft = dBufLeft.bufAt(0, 0);

    // 4.2.2 set back up buffer for right
    PelBuf dBufRight = m_bufSubPicRight.getBuf(compID);
    Pel    *dstRight = dBufRight.bufAt(0, 0);

    // 4.2.3 copy to recon picture to back up buffer
    Pel *srcLeft  = src - xmargin;
    Pel *srcRight = src + width;

    for (int y = 0; y < height; y++)
    {
      // the destination and source position is reversed on purpose
      ::memcpy(srcLeft  + y * s.stride,  dstLeft + y *  dBufLeft.stride, sizeof(Pel) * xmargin);
      ::memcpy(srcRight + y * s.stride, dstRight + y * dBufRight.stride, sizeof(Pel) * xmargin);
    }


    // 4.3.1 set back up buffer for above
    PelBuf dBufTop = m_bufSubPicAbove.getBuf(compID);
    Pel    *dstTop = dBufTop.bufAt(0, 0);

    // 4.3.2 set back up buffer for below
    PelBuf dBufBottom = m_bufSubPicBelow.getBuf(compID);
    Pel    *dstBottom = dBufBottom.bufAt(0, 0);

    // 4.3.3 copy to recon picture to back up buffer
    Pel *srcTop = src - xmargin - ymargin * s.stride;
    Pel *srcBottom = src - xmargin + height * s.stride;

    for (int y = 0; y < ymargin; y++)
    {
      ::memcpy(srcTop    + y * s.stride, dstTop    + y *    dBufTop.stride, sizeof(Pel) * (2 * xmargin + width));
      ::memcpy(srcBottom + y * s.stride, dstBottom + y * dBufBottom.stride, sizeof(Pel) * (2 * xmargin + width));
    }

    // restore recon wrap buffer
    if (cs->sps->getWrapAroundEnabledFlag())
    {
      // set recon wrap picture
      PelBuf sWrap = M_BUFS(0, PIC_RECON_WRAP).get(compID);
      Pel *srcWrap = sWrap.bufAt(left, top);

      // set back up buffer for above
      PelBuf dBufTopWrap = m_bufWrapSubPicAbove.getBuf(compID);
      Pel    *dstTopWrap = dBufTopWrap.bufAt(0, 0);

      // set back up buffer for below
      PelBuf dBufBottomWrap = m_bufWrapSubPicBelow.getBuf(compID);
      Pel    *dstBottomWrap = dBufBottomWrap.bufAt(0, 0);

      // copy to recon wrap picture from back up buffer
      Pel *srcTopWrap = srcWrap - xmargin - ymargin * sWrap.stride;
      Pel *srcBottomWrap = srcWrap - xmargin + height * sWrap.stride;

      for (int y = 0; y < ymargin; y++)
      {
        ::memcpy(srcTopWrap    + y * sWrap.stride, dstTopWrap    + y *    dBufTopWrap.stride, sizeof(Pel) * (2 * xmargin + width));
        ::memcpy(srcBottomWrap + y * sWrap.stride, dstBottomWrap + y * dBufBottomWrap.stride, sizeof(Pel) * (2 * xmargin + width));
      }
    }
  }

  // 5.0 destroy the back up memory
  m_bufSubPicAbove.destroy();
  m_bufSubPicBelow.destroy();
  m_bufSubPicLeft.destroy();
  m_bufSubPicRight.destroy();
  m_bufWrapSubPicAbove.destroy();
  m_bufWrapSubPicBelow.destroy();
}

void Picture::extendPicBorder( const SPS *sps, const PPS *pps )
{
  if (m_extendedBorder)
  {
    if( isWrapAroundEnabled( sps, pps ) && ( !m_wrapAroundValid || m_wrapAroundOffset != pps->getWrapAroundOffset() ) )
    {
      extendWrapBorder( pps );
    }
    return;
  }

  for(int comp=0; comp<getNumberValidComponents( cs->area.chromaFormat ); comp++)
  {
    ComponentID compID = ComponentID( comp );
    PelBuf p = M_BUFS( 0, PIC_RECONSTRUCTION ).get( compID );
    Pel *piTxt = p.bufAt(0,0);
    int xmargin = margin >> getComponentScaleX( compID, cs->area.chromaFormat );
    int ymargin = margin >> getComponentScaleY( compID, cs->area.chromaFormat );

    Pel*  pi = piTxt;
    // do left and right margins
    for (int y = 0; y < p.height; y++)
    {
      for (int x = 0; x < xmargin; x++)
      {
        pi[-xmargin + x] = pi[0];
        pi[p.width + x]  = pi[p.width - 1];
      }
      pi += p.stride;
    }

    // pi is now the (0,height) (bottom left of image within bigger picture
    pi -= (p.stride + xmargin);
    // pi is now the (-marginX, height-1)
    for (int y = 0; y < ymargin; y++ )
    {
      ::memcpy( pi + (y+1)*p.stride, pi, sizeof(Pel)*(p.width + (xmargin << 1)));
    }

    // pi is still (-marginX, height-1)
    pi -= ((p.height-1) * p.stride);
    // pi is now (-marginX, 0)
    for (int y = 0; y < ymargin; y++ )
    {
      ::memcpy( pi - (y+1)*p.stride, pi, sizeof(Pel)*(p.width + (xmargin<<1)) );
    }

    // reference picture with horizontal wrapped boundary
    if ( isWrapAroundEnabled(sps, pps ) )
    {
      extendWrapBorder( pps );
    }
    else
    {
      m_wrapAroundValid = false;
      m_wrapAroundOffset = 0;
    }
  }

  m_extendedBorder = true;
}

void Picture::extendWrapBorder( const PPS *pps )
{
  for(int comp=0; comp<getNumberValidComponents( cs->area.chromaFormat ); comp++)
  {
    ComponentID compID = ComponentID( comp );
    PelBuf p = M_BUFS( 0, PIC_RECON_WRAP ).get( compID );
    p.copyFrom(M_BUFS( 0, PIC_RECONSTRUCTION ).get( compID ));
    Pel *piTxt = p.bufAt(0,0);
    int xmargin = margin >> getComponentScaleX( compID, cs->area.chromaFormat );
    int ymargin = margin >> getComponentScaleY( compID, cs->area.chromaFormat );
    Pel*  pi = piTxt;
    int xoffset = pps->getWrapAroundOffset() >> getComponentScaleX( compID, cs->area.chromaFormat );
    for (int y = 0; y < p.height; y++)
    {
      for (int x = 0; x < xmargin; x++ )
      {
        if( x < xoffset )
        {
          pi[ -x - 1 ] = pi[ -x - 1 + xoffset ];
          pi[  p.width + x ] = pi[ p.width + x - xoffset ];
        }
        else
        {
          pi[ -x - 1 ] = pi[ 0 ];
          pi[  p.width + x ] = pi[ p.width - 1 ];
        }
      }
      pi += p.stride;
    }
    pi -= (p.stride + xmargin);
    for (int y = 0; y < ymargin; y++ )
    {
      ::memcpy( pi + (y+1)*p.stride, pi, sizeof(Pel)*(p.width + (xmargin << 1)));
    }
    pi -= ((p.height-1) * p.stride);
    for (int y = 0; y < ymargin; y++ )
    {
      ::memcpy( pi - (y+1)*p.stride, pi, sizeof(Pel)*(p.width + (xmargin<<1)) );
    }
  }
  m_wrapAroundValid = true;
  m_wrapAroundOffset = pps->getWrapAroundOffset();
}

PelBuf Picture::getBuf( const ComponentID compID, const PictureType &type )
{
  return M_BUFS( ( type == PIC_ORIGINAL || type == PIC_TRUE_ORIGINAL || type == PIC_FILTERED_ORIGINAL || type == PIC_ORIGINAL_INPUT || type == PIC_TRUE_ORIGINAL_INPUT || type == PIC_FILTERED_ORIGINAL_INPUT ) ? 0 : scheduler.getSplitPicId(), type ).getBuf( compID );
}

const CPelBuf Picture::getBuf( const ComponentID compID, const PictureType &type ) const
{
  return M_BUFS( ( type == PIC_ORIGINAL || type == PIC_TRUE_ORIGINAL || type == PIC_FILTERED_ORIGINAL || type == PIC_ORIGINAL_INPUT || type == PIC_TRUE_ORIGINAL_INPUT || type == PIC_FILTERED_ORIGINAL_INPUT ) ? 0 : scheduler.getSplitPicId(), type ).getBuf( compID );
}

PelBuf Picture::getBuf( const CompArea &blk, const PictureType &type )
{
  if( !blk.valid() )
  {
    return PelBuf();
  }

#if !KEEP_PRED_AND_RESI_SIGNALS
  if( type == PIC_RESIDUAL || type == PIC_PREDICTION )
  {
    CompArea localBlk = blk;
    localBlk.x &= ( cs->pcv->maxCUWidthMask  >> getComponentScaleX( blk.compID, blk.chromaFormat ) );
    localBlk.y &= ( cs->pcv->maxCUHeightMask >> getComponentScaleY( blk.compID, blk.chromaFormat ) );

    return M_BUFS( jId, type ).getBuf( localBlk );
  }
#endif

  return M_BUFS( jId, type ).getBuf( blk );
}

const CPelBuf Picture::getBuf( const CompArea &blk, const PictureType &type ) const
{
  if( !blk.valid() )
  {
    return PelBuf();
  }

#if !KEEP_PRED_AND_RESI_SIGNALS
  if( type == PIC_RESIDUAL || type == PIC_PREDICTION )
  {
    CompArea localBlk = blk;
    localBlk.x &= ( cs->pcv->maxCUWidthMask  >> getComponentScaleX( blk.compID, blk.chromaFormat ) );
    localBlk.y &= ( cs->pcv->maxCUHeightMask >> getComponentScaleY( blk.compID, blk.chromaFormat ) );

    return M_BUFS( jId, type ).getBuf( localBlk );
  }
#endif

  return M_BUFS( jId, type ).getBuf( blk );
}

PelUnitBuf Picture::getBuf( const UnitArea &unit, const PictureType &type )
{
  if (!isChromaEnabled(chromaFormat))
  {
    return PelUnitBuf( chromaFormat, getBuf( unit.Y(), type ) );
  }
  else
  {
    return PelUnitBuf( chromaFormat, getBuf( unit.Y(), type ), getBuf( unit.Cb(), type ), getBuf( unit.Cr(), type ) );
  }
}

const CPelUnitBuf Picture::getBuf( const UnitArea &unit, const PictureType &type ) const
{
  if (!isChromaEnabled(chromaFormat))
  {
    return CPelUnitBuf( chromaFormat, getBuf( unit.Y(), type ) );
  }
  else
  {
    return CPelUnitBuf( chromaFormat, getBuf( unit.Y(), type ), getBuf( unit.Cb(), type ), getBuf( unit.Cr(), type ) );
  }
}

Pel* Picture::getOrigin( const PictureType &type, const ComponentID compID ) const
{
  return M_BUFS( jId, type ).getOrigin( compID );
}

void Picture::createSpliceIdx(int nums)
{
  m_ctuNums = nums;
  m_spliceIdx = new int[m_ctuNums];
  memset(m_spliceIdx, 0, m_ctuNums * sizeof(int));
}

bool Picture::getSpliceFull()
{
  int count = 0;
  for (int i = 0; i < m_ctuNums; i++)
  {
    if (m_spliceIdx[i] != 0)
    {
      count++;
    }
  }
  if (count < m_ctuNums * 0.25)
  {
    return false;
  }
  return true;
}

void Picture::addPictureToHashMapForInter()
{
  int picWidth = slices[0]->getPPS()->getPicWidthInLumaSamples();
  int picHeight = slices[0]->getPPS()->getPicHeightInLumaSamples();
  uint32_t* blockHashValues[2][2];
  bool     *isBlockSame[2][3];

  for (int i = 0; i < 2; i++)
  {
    for (int j = 0; j < 2; j++)
    {
      blockHashValues[i][j] = new uint32_t[picWidth*picHeight];
    }

    for (int j = 0; j < 3; j++)
    {
      isBlockSame[i][j] = new bool[picWidth * picHeight];
    }
  }
  m_hashMap.create(picWidth, picHeight);
  m_hashMap.generateBlock2x2HashValue(getOrigBuf(), picWidth, picHeight, slices[0]->getSPS()->getBitDepths(),
                                      blockHashValues[0], isBlockSame[0]);   // 2x2
  m_hashMap.generateBlockHashValue(picWidth, picHeight, 4, 4, blockHashValues[0], blockHashValues[1], isBlockSame[0],
                                   isBlockSame[1]);   // 4x4
  m_hashMap.addToHashMapByRowWithPrecalData(blockHashValues[1], isBlockSame[1][2], picWidth, picHeight, 4, 4);

  m_hashMap.generateBlockHashValue(picWidth, picHeight, 8, 8, blockHashValues[1], blockHashValues[0], isBlockSame[1],
                                   isBlockSame[0]);   // 8x8
  m_hashMap.addToHashMapByRowWithPrecalData(blockHashValues[0], isBlockSame[0][2], picWidth, picHeight, 8, 8);

  m_hashMap.generateBlockHashValue(picWidth, picHeight, 16, 16, blockHashValues[0], blockHashValues[1], isBlockSame[0],
                                   isBlockSame[1]);   // 16x16
  m_hashMap.addToHashMapByRowWithPrecalData(blockHashValues[1], isBlockSame[1][2], picWidth, picHeight, 16, 16);

  m_hashMap.generateBlockHashValue(picWidth, picHeight, 32, 32, blockHashValues[1], blockHashValues[0], isBlockSame[1],
                                   isBlockSame[0]);   // 32x32
  m_hashMap.addToHashMapByRowWithPrecalData(blockHashValues[0], isBlockSame[0][2], picWidth, picHeight, 32, 32);

  m_hashMap.generateBlockHashValue(picWidth, picHeight, 64, 64, blockHashValues[0], blockHashValues[1], isBlockSame[0],
                                   isBlockSame[1]);   // 64x64
  m_hashMap.addToHashMapByRowWithPrecalData(blockHashValues[1], isBlockSame[1][2], picWidth, picHeight, 64, 64);

  m_hashMap.setInitial();

  for (int i = 0; i < 2; i++)
  {
    for (int j = 0; j < 2; j++)
    {
      delete[] blockHashValues[i][j];
    }

    for (int j = 0; j < 3; j++)
    {
      delete[] isBlockSame[i][j];
    }
  }
}

void Picture::createGrainSynthesizer(bool firstPictureInSequence, SEIFilmGrainSynthesizer *grainCharacteristics, PelStorage *grainBuf, int width, int height, ChromaFormat fmt, int bitDepth)
{
  m_grainCharacteristic = grainCharacteristics;
  m_grainBuf            = grainBuf;

  // Padding to make wd and ht multiple of max fgs window size(64)
  int paddedWdFGS = ((width - 1) | 0x3F) + 1 - width;
  int paddedHtFGS = ((height - 1) | 0x3F) + 1 - height;
  m_padValue      = (paddedWdFGS > paddedHtFGS) ? paddedWdFGS : paddedHtFGS;

  if (firstPictureInSequence)
  {
    // Create and initialize the Film Grain Synthesizer
    m_grainCharacteristic->create(width, height, fmt, bitDepth, 1);

    // Frame level PelStorage buffer created to blend Film Grain Noise into it
    m_grainBuf->create(chromaFormat, Area(0, 0, width, height), 0, m_padValue, 0, false);

    m_grainCharacteristic->fgsInit();
  }
}

PelUnitBuf Picture::getDisplayBufFG(bool wrap)
{
  SEI::PayloadType           payloadType;
  std::list<SEI *>::iterator message;

  for (message = SEIs.begin(); message != SEIs.end(); ++message)
  {
    payloadType = (*message)->payloadType();
    if (payloadType == SEI::PayloadType::FILM_GRAIN_CHARACTERISTICS)
    {
      m_grainCharacteristic->m_errorCode       = -1;
      *m_grainCharacteristic->m_fgcParameters = *static_cast<SEIFilmGrainCharacteristics *>(*message);
      /* Validation of Film grain characteristic parameters for the constrains of SMPTE-RDD5*/
      m_grainCharacteristic->m_errorCode = m_grainCharacteristic->grainValidateParams();
      break;
    }
  }

  if (FGS_SUCCESS == m_grainCharacteristic->m_errorCode)
  {
#if JVET_AL0339_FGS_SEI_SPATIAL_RESOLUTION
    int width = getRecoBuf().get(COMPONENT_Y).width;
    int height = getRecoBuf().get(COMPONENT_Y).height;
    if (m_grainBuf->get(COMPONENT_Y).width != width || m_grainBuf->get(COMPONENT_Y).height != height)
    {
      m_grainCharacteristic->create(width, height, chromaFormat, m_bitDepths[ChannelType::LUMA], 1);
      m_grainBuf->destroy();
      int paddedWdFGS = ((width - 1) | 0x3F) + 1 - width;
      int paddedHtFGS = ((height - 1) | 0x3F) + 1 - height;
      m_padValue      = (paddedWdFGS > paddedHtFGS) ? paddedWdFGS : paddedHtFGS;
      m_grainBuf->create(chromaFormat, Area(0, 0, width, height), 0, m_padValue, 0, false);
      m_grainCharacteristic->fgsInit();
    }
#endif
    m_grainBuf->copyFrom(getRecoBuf());
    m_grainBuf->extendBorderPel(m_padValue); // Padding to make wd and ht multiple of max fgs window size(64)

    m_grainCharacteristic->m_poc = getPOC();
    m_grainCharacteristic->grainSynthesizeAndBlend(m_grainBuf, slices[0]->getIdrPicFlag());

    return *m_grainBuf;
  }
  else
  {
#if JVET_AL0339_FGS_SEI_SPATIAL_RESOLUTION
    if (message != SEIs.end())
#else
    if (payloadType == SEI::PayloadType::FILM_GRAIN_CHARACTERISTICS)
#endif
    {
      msg(WARNING, "Film Grain synthesis is not performed. Error code: 0x%x \n", m_grainCharacteristic->m_errorCode);
    }
    return M_BUFS(scheduler.getSplitPicId(), wrap ? PIC_RECON_WRAP : PIC_RECONSTRUCTION);
  }
}

#if JVET_AL0339_FGS_SEI_SPATIAL_RESOLUTION

bool Picture::filmGrainAfterUpscale()
{
  SEIMessages seiList;
  seiList = getSeisByType(SEIs, SEI::PayloadType::FILM_GRAIN_CHARACTERISTICS);
  if (!seiList.empty())
  {
    SEIFilmGrainCharacteristics *sei = static_cast<SEIFilmGrainCharacteristics *>(*seiList.begin());
    if (!sei->m_spatialResolutionPresentFlag)
    {
      return false;
    }
    else
    {
      return (getRecoBuf().get(COMPONENT_Y).width != sei->m_picWidthInLumaSamples || getRecoBuf().get(COMPONENT_Y).height != sei->m_picHeightInLumaSamples);
    }
  }
  else
  {
    return false;
  }
}

PelUnitBuf Picture::getDisplayBufFGUpscaled(const SPS& sps, const PPS& pps, int outputChoice, ChromaFormat format,
                                            int upscaleFilterForDisplay, int maxWidth, int maxHeight, bool wrap )
{
  SEI::PayloadType           payloadType;
  std::list<SEI *>::iterator message;

  for (message = SEIs.begin(); message != SEIs.end(); ++message)
  {
    payloadType = (*message)->payloadType();
    if (payloadType == SEI::PayloadType::FILM_GRAIN_CHARACTERISTICS)
    {
      m_grainCharacteristic->m_errorCode       = -1;
      *m_grainCharacteristic->m_fgcParameters = *static_cast<SEIFilmGrainCharacteristics *>(*message);
      /* Validation of Film grain characteristic parameters for the constrains of SMPTE-RDD5*/
      m_grainCharacteristic->m_errorCode = m_grainCharacteristic->grainValidateParams();
      break;
    }
  }

  if (FGS_SUCCESS == m_grainCharacteristic->m_errorCode && outputChoice == 2)
  {
    if (!maxWidth || !outputChoice)
    {
      maxWidth = sps.getMaxPicWidthInLumaSamples();
    }
    if (!maxHeight || !outputChoice)
    {
      maxHeight = sps.getMaxPicHeightInLumaSamples();
    }

    int width = getRecoBuf().get(COMPONENT_Y).width;
    int height = getRecoBuf().get(COMPONENT_Y).height;
    int paddedWdFGS = ((maxWidth - 1) | 0x3F) + 1 - maxWidth;
    int paddedHtFGS = ((maxHeight - 1) | 0x3F) + 1 - maxHeight;
    m_padValue      = (paddedWdFGS > paddedHtFGS) ? paddedWdFGS : paddedHtFGS;
    if (m_grainBuf->get(COMPONENT_Y).width != maxWidth || m_grainBuf->get(COMPONENT_Y).height != maxHeight)
    {
      m_grainCharacteristic->create(maxWidth, maxHeight, chromaFormat, m_bitDepths[ChannelType::LUMA], 1);
      m_grainBuf->destroy();
      m_grainBuf->create(chromaFormat, Area(0, 0, maxWidth, maxHeight), 0, m_padValue, 0, false);
      m_grainCharacteristic->fgsInit();
    }

    if (maxWidth != width || maxHeight != height)
    {
      int curPicWidth = maxWidth;
      int curPicHeight = maxHeight;

      int refPicWidth = pps.getPicWidthInLumaSamples();
      int refPicHeight = pps.getPicHeightInLumaSamples();

      const int xScale = ((refPicWidth << ScalingRatio::BITS) + (curPicWidth >> 1)) / curPicWidth;
      const int yScale = ((refPicHeight << ScalingRatio::BITS) + (curPicHeight >> 1)) / curPicHeight;

      bool rescaleForDisplay = true;
      Window win(0, 0, 0, 0);
      PelUnitBuf buf = m_grainBuf->getBuf(UnitArea(chromaFormat, Area(0, 0, m_grainBuf->get(COMPONENT_Y).width, m_grainBuf->get(COMPONENT_Y).height)));
      Picture::rescalePicture({ xScale, yScale }, getRecoBuf(), win, buf,
        win, chromaFormat, sps.getBitDepths(), false, false,
        sps.getHorCollocatedChromaFlag(), sps.getVerCollocatedChromaFlag(), rescaleForDisplay,
        upscaleFilterForDisplay);
    }
    else
    {
      m_grainBuf->copyFrom(getRecoBuf());
    }

    m_grainBuf->extendBorderPel(m_padValue); // Padding to make wd and ht multiple of max fgs window size(64)

    m_grainCharacteristic->m_poc = getPOC();
    m_grainCharacteristic->grainSynthesizeAndBlend(m_grainBuf, slices[0]->getIdrPicFlag());

    return *m_grainBuf;
  }
  else
  {
    if (message != SEIs.end())
    {
      msg(WARNING, "Film Grain synthesis is not performed. Error code: 0x%x \n", m_grainCharacteristic->m_errorCode);
    }
    return M_BUFS(scheduler.getSplitPicId(), wrap ? PIC_RECON_WRAP : PIC_RECONSTRUCTION);
  }
}

#endif

void Picture::createColourTransfProcessor(bool firstPictureInSequence, SEIColourTransformApply* ctiCharacteristics, PelStorage* ctiBuf, int width, int height, ChromaFormat fmt, int bitDepth)
{
  m_colourTranfParams = ctiCharacteristics;
  m_invColourTransfBuf = ctiBuf;
  if (firstPictureInSequence)
  {
    // Create and initialize the Colour Transform Processor
    m_colourTranfParams->create(width, height, fmt, bitDepth);

    //Frame level PelStorage buffer created to apply the Colour Transform
    m_invColourTransfBuf->create(UnitArea(chromaFormat, Area(0, 0, width, height)));
  }
}

PelUnitBuf Picture::getDisplayBuf()
{
  SEI::PayloadType          payloadType;
  std::list<SEI*>::iterator message;

  for (message = SEIs.begin(); message != SEIs.end(); ++message)
  {
    payloadType = (*message)->payloadType();
    if (payloadType == SEI::PayloadType::COLOUR_TRANSFORM_INFO)
    {
      // re-init parameters
      *m_colourTranfParams->m_pColourTransfParams = *static_cast<SEIColourTransformInfo*>(*message);
      //m_colourTranfParams->m_pColourTransfParams = static_cast<SEIColourTransformInfo*>(*message);
      break;
    }
  }

  m_invColourTransfBuf->copyFrom(getRecoBuf());

  if (m_colourTranfParams->m_pColourTransfParams != nullptr)
  {
    m_colourTranfParams->generateColourTransfLUTs();
    m_colourTranfParams->inverseColourTransform(m_invColourTransfBuf);
  }

  return *m_invColourTransfBuf;
}


void Picture::copyToPic(const SPS *sps, PelStorage *pcPicYuvSrc, PelStorage *pcPicYuvDst)
{
  const ChromaFormat chromaFormatIdc    = sps->getChromaFormatIdc();
  int                numValidComponents = getNumberValidComponents(chromaFormatIdc);

  Pel *srcPxl, *dstPxl;
  ptrdiff_t srcStride;
  int       srcHeight, srcWidth;
  ptrdiff_t dstStride;

  for (int comp = 0; comp < numValidComponents; comp++)
  {
    if (comp == COMPONENT_Y)
    {
      srcPxl = pcPicYuvSrc->Y().buf;
      dstPxl = pcPicYuvDst->Y().buf;
      srcStride = pcPicYuvSrc->Y().stride;
      srcHeight = pcPicYuvSrc->Y().height;
      srcWidth  = pcPicYuvSrc->Y().width;
      dstStride = pcPicYuvSrc->Y().stride;
    }
    else if (comp == COMPONENT_Cb)
    {
      srcPxl = pcPicYuvSrc->Cb().buf;
      dstPxl = pcPicYuvDst->Cb().buf;
      srcStride = pcPicYuvSrc->Cb().stride;
      srcHeight = pcPicYuvSrc->Cb().height;
      srcWidth  = pcPicYuvSrc->Cb().width;
      dstStride = pcPicYuvSrc->Cb().stride;
    }
    else
    {
      srcPxl = pcPicYuvSrc->Cr().buf;
      dstPxl = pcPicYuvDst->Cr().buf;
      srcStride = pcPicYuvSrc->Cr().stride;
      srcHeight = pcPicYuvSrc->Cr().height;
      srcWidth  = pcPicYuvSrc->Cr().width;
      dstStride = pcPicYuvSrc->Cr().stride;
    }

    if (srcStride == dstStride)
    {
      ::memcpy(dstPxl, srcPxl, sizeof(Pel) * srcStride * srcHeight /*getTotalHeight(compId)*/);
    }
    else
    {
      for (int y = 0; y < srcHeight; y++, srcPxl += srcStride, dstPxl += dstStride)
      {
        ::memcpy(dstPxl, srcPxl, srcWidth * sizeof(Pel));
      }
    }
  }
}

Picture* Picture::findNextPicPOC(Picture* pcPic, PicList* pcListPic)
{
  Picture           *nextPic     = nullptr;
  Picture           *listPic     = nullptr;
  PicList::iterator  iterListPic = pcListPic->begin();
  for (int i = 0; i < (int)(pcListPic->size()); i++)
  {
    listPic = *(iterListPic);
    if (listPic->getPOC() == pcPic->getPOC() + 1)
    {
      nextPic = *(iterListPic);
    }
    iterListPic++;
  }
  return nextPic;
}


Picture* Picture::findPrevPicPOC(Picture* pcPic, PicList* pcListPic)
{
  Picture           *prevPic     = nullptr;
  Picture           *listPic     = nullptr;
  PicList::iterator  iterListPic = pcListPic->begin();
  for (int i = 0; i < (int)(pcListPic->size()); i++)
  {
    listPic = *(iterListPic);
    if (listPic->getPOC() == pcPic->getPOC() - 1)
    {
      prevPic = *(iterListPic);
    }
    iterListPic++;
  }
  return prevPic;
}

void Picture::xOutputPostFilteredPic(Picture* pcPic, PicList* pcListPic, int blendingRatio)
{
  const SPS *sps = pcPic->cs->sps;
  const ChromaFormat chromaFormatIdc = sps->getChromaFormatIdc();

  if ((pcPic->getPOC()) % blendingRatio != 0 || pcPic->getPOC() == 0)
    pcPic->getPostRecBuf().copyFrom(pcPic->getRecoBuf());

  if ((pcPic->getPOC() + 1) % blendingRatio == 0)
  {
    Picture* nextPic = findNextPicPOC(pcPic, pcListPic);
    if (nextPic)
    {
#if DISABLE_PRE_POST_FILTER_FOR_IDR_CRA
      if((nextPic->m_pictureType == NAL_UNIT_CODED_SLICE_IDR_W_RADL) ||
        (nextPic->m_pictureType == NAL_UNIT_CODED_SLICE_IDR_N_LP) ||
        (nextPic->m_pictureType == NAL_UNIT_CODED_SLICE_CRA))
      {
        nextPic->getPostRecBuf().copyFrom(nextPic->getRecoBuf());
        return;
      }
#endif
      PelUnitBuf currTmp = pcPic->getRecoBuf();
      PelUnitBuf nextTmp = nextPic->getRecoBuf();
      PelUnitBuf postTmp = nextPic->getPostRecBuf();

      PelUnitBuf* currYuv = &currTmp;
      PelUnitBuf* nextYuv = &nextTmp;
      PelUnitBuf* postYuv = &postTmp;

      int numValidComponents = getNumberValidComponents(chromaFormatIdc);
      for (int chan = 0; chan < numValidComponents; chan++)
      {
        const ComponentID ch = ComponentID(chan);
        const ChannelType cType          = (ch == COMPONENT_Y) ? ChannelType::LUMA : ChannelType::CHROMA;
        const int bitDepth = pcPic->cs->sps->getBitDepth(cType);
        const int maxOutputValue = (1 << bitDepth) - 1;

        Pel *currPxl, *nextPxl, *postPxl;
        ptrdiff_t stride;
        int       height, width;
        if (ch == COMPONENT_Y)
        {
          currPxl = currYuv->Y().buf;
          nextPxl = nextYuv->Y().buf;
          postPxl = postYuv->Y().buf;
          stride  = currYuv->Y().stride;
          height  = currYuv->Y().height;
          width   = currYuv->Y().width;
        }
        else if (ch == COMPONENT_Cb)
        {
          nextPxl = nextYuv->Cb().buf;
          currPxl = currYuv->Cb().buf;
          postPxl = postYuv->Cb().buf;
          stride  = currYuv->Cb().stride;
          height  = currYuv->Cb().height;
          width   = currYuv->Cb().width;
        }
        else
        {
          nextPxl = nextYuv->Cr().buf;
          currPxl = currYuv->Cr().buf;
          postPxl = postYuv->Cr().buf;
          stride  = currYuv->Cr().stride;
          height  = currYuv->Cr().height;
          width   = currYuv->Cr().width;
        }
        for (int y = 0; y < height; y++)
        {
          for (int x = 0; x < width; x++)
          {
#if ENABLE_USER_DEFINED_WEIGHTS
            postPxl[x] = std::min(maxOutputValue, std::max(0, (int)(((nextPxl[x]) / SII_PF_W2) - ((currPxl[x] * SII_PF_W1) / SII_PF_W2))));
#else
            postPxl[x] = std::min(maxOutputValue, std::max(0, (((nextPxl[x] * (blendingRatio + 1)) / blendingRatio) - (currPxl[x] / blendingRatio))));
#endif
          }
          currPxl += stride;
          nextPxl += stride;
          postPxl += stride;
        }
      }
    }
  }
}

void Picture::xOutputPreFilteredPic(Picture* pcPic, PicList* pcListPic, int blendingRatio, int intraPeriod)
{
  const SPS *sps = pcPic->cs->sps;
  const ChromaFormat chromaFormatIdc = sps->getChromaFormatIdc();
#if DISABLE_PRE_POST_FILTER_FOR_IDR_CRA
  if (pcPic->getPOC() == 0 ||
    (pcPic->getPOC() % intraPeriod == 0))
  {
    return;
  }
#endif
  if (pcPic->getPOC() % blendingRatio == 0)
  {
    Picture* prevPic = findPrevPicPOC(pcPic, pcListPic);
    if (prevPic)
    {
      PelStorage* currYuv = &pcPic->m_bufs[PIC_ORIGINAL];
      PelStorage* prevYuv = &prevPic->m_bufs[PIC_ORIGINAL];
      const int   numValidComponents = getNumberValidComponents(chromaFormatIdc);
      for (int chan = 0; chan < numValidComponents; chan++)
      {
        const ComponentID ch = ComponentID(chan);
        const ChannelType cType          = toChannelType(ch);
        const int bitDepth = pcPic->cs->sps->getBitDepth(cType);
        const int maxOutputValue = (1 << bitDepth) - 1;

        Pel *currPxl, *prevPxl;
        ptrdiff_t stride;
        int       height, width;
        if (ch == COMPONENT_Y)
        {
          currPxl = currYuv->Y().buf;
          prevPxl = prevYuv->Y().buf;
          stride  = currYuv->Y().stride;
          height  = currYuv->Y().height;
          width   = currYuv->Y().width;
        }
        else if (ch == COMPONENT_Cb)
        {
          prevPxl = prevYuv->Cb().buf;
          currPxl = currYuv->Cb().buf;
          stride  = currYuv->Cb().stride;
          height  = currYuv->Cb().height;
          width   = currYuv->Cb().width;
        }
        else
        {
          prevPxl = prevYuv->Cr().buf;
          currPxl = currYuv->Cr().buf;
          stride  = currYuv->Cr().stride;
          height  = currYuv->Cr().height;
          width   = currYuv->Cr().width;
        }

        for (int y = 0; y < height; y++)
        {
          for (int x = 0; x < width; x++)
          {
#if ENABLE_USER_DEFINED_WEIGHTS
            currPxl[x] = std::min(maxOutputValue, std::max(0, (int)((currPxl[x] * SII_PF_W2) + (prevPxl[x] * SII_PF_W1));
#else
            currPxl[x] = std::min(maxOutputValue, std::max(0, (((currPxl[x] * blendingRatio) / (blendingRatio + 1)) + (prevPxl[x] / (blendingRatio + 1)))));
#endif
          }
          currPxl += stride;
          prevPxl += stride;
        }
      }
    }
  }
}

void Picture::copyAlfData(const Picture &p)
{
  for (int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++)
  {
    CHECK(p.m_alfModes[compIdx].size() != m_alfModes[compIdx].size(), "Size mismatch");

    std::copy(p.m_alfModes[compIdx].begin(), p.m_alfModes[compIdx].end(), m_alfModes[compIdx].begin());
  }
}

void Picture::resizeAlfData(const int numEntries)
{
  for (int compIdx = 0; compIdx < MAX_NUM_COMPONENT; compIdx++)
  {
    m_alfModes[compIdx].resize(numEntries);
    std::fill(m_alfModes[compIdx].begin(), m_alfModes[compIdx].end(), AlfMode::OFF);
  }
}
