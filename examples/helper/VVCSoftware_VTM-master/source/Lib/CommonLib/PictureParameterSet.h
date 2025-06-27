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

#pragma once

#include "Common.h"
#include "SequenceParameterSet.h"

class PreCalcValues;

class SliceMap
{
private:
  uint32_t               m_sliceID = 0;         // slice identifier (slice index for rectangular slices, slice address for raser-scan slices)
  uint32_t               m_numTilesInSlice = 0; // number of tiles in slice (raster-scan slices only)
  uint32_t               m_numCtuInSlice = 0;   // number of CTUs in the slice
  std::vector<uint32_t>  m_ctuAddrInSlice;      // raster-scan addresses of all the CTUs in the slice

public:
  SliceMap() {};
  virtual ~SliceMap() {};

  void                   setSliceID( uint32_t u )             { m_sliceID = u;            }
  uint32_t               getSliceID() const                   { return m_sliceID;         }
  void                   setNumTilesInSlice( uint32_t u )     { m_numTilesInSlice = u;    }
  uint32_t               getNumTilesInSlice() const           { return m_numTilesInSlice; }
  void                   setNumCtuInSlice( uint32_t u )       { m_numCtuInSlice = u;      }
  uint32_t               getNumCtuInSlice() const             { return m_numCtuInSlice;   }
  std::vector<uint32_t>  getCtuAddrList( ) const              { return m_ctuAddrInSlice;  }
  uint32_t               getCtuAddrInSlice( int idx ) const   { CHECK(idx >= m_ctuAddrInSlice.size(), "CTU index exceeds number of CTUs in slice."); return m_ctuAddrInSlice[idx]; }
  void                   pushToCtuAddrInSlice( uint32_t u )   { m_ctuAddrInSlice.push_back(u); m_numCtuInSlice++;}

  void  initSliceMap()
  {
    m_sliceID = 0;
    m_numTilesInSlice = 0;
    m_numCtuInSlice = 0;
    m_ctuAddrInSlice.clear();
  }

  void  addCtusToSlice( uint32_t startX, uint32_t stopX, uint32_t startY, uint32_t stopY, uint32_t picWidthInCtbsY )
  {
    CHECK( startX >= stopX || startY >= stopY, "Invalid slice definition");
    for( uint32_t ctbY = startY; ctbY < stopY; ctbY++ )
    {
      for( uint32_t ctbX = startX; ctbX < stopX; ctbX++ )
      {
        m_ctuAddrInSlice.push_back( ctbY * picWidthInCtbsY + ctbX );
        m_numCtuInSlice++;
      }
    }
  }
};

class RectSlice
{
private:
  uint32_t         m_tileIdx = 0;             // tile index corresponding to the first CTU in the slice
  uint32_t         m_sliceWidthInTiles = 0;   // slice width in units of tiles
  uint32_t         m_sliceHeightInTiles = 0;  // slice height in units of tiles
  uint32_t         m_numSlicesInTile = 0;     // number of slices in current tile for the special case of multiple slices inside a single tile
  uint32_t         m_sliceHeightInCtu = 0;    // slice height in units of CTUs for the special case of multiple slices inside a single tile

public:
  RectSlice() {};
  virtual ~RectSlice() {};

  void             setSliceWidthInTiles( uint32_t u )   { m_sliceWidthInTiles = u;      }
  uint32_t         getSliceWidthInTiles( ) const        { return  m_sliceWidthInTiles;  }
  void             setSliceHeightInTiles( uint32_t u )  { m_sliceHeightInTiles = u;     }
  uint32_t         getSliceHeightInTiles( ) const       { return  m_sliceHeightInTiles; }
  void             setNumSlicesInTile( uint32_t u )     { m_numSlicesInTile = u;        }
  uint32_t         getNumSlicesInTile( ) const          { return  m_numSlicesInTile;    }
  void             setSliceHeightInCtu( uint32_t u )    { m_sliceHeightInCtu = u;       }
  uint32_t         getSliceHeightInCtu( ) const         { return  m_sliceHeightInCtu;   }
  void             setTileIdx( TileIdx u )              { m_tileIdx = u;                }
  TileIdx          getTileIdx( ) const                  { return  m_tileIdx;            }

};

class SubPic
{
private:
  uint32_t         m_subPicID = 0;                              // ID of subpicture
  uint32_t         m_subPicIdx = 0;                             // Index of subpicture
  uint32_t         m_numCTUsInSubPic = 0;                       // number of CTUs contained in this sub-picture
  uint32_t         m_subPicCtuTopLeftX = 0;                     // horizontal position of top left CTU of the subpicture in unit of CTU
  uint32_t         m_subPicCtuTopLeftY = 0;                     // vertical position of top left CTU of the subpicture in unit of CTU
  uint32_t         m_subPicWidth = 0;                           // the width of subpicture in units of CTU
  uint32_t         m_subPicHeight = 0;                          // the height of subpicture in units of CTU
  uint32_t         m_subPicWidthInLumaSample = 0;               // the width of subpicture in units of luma sample
  uint32_t         m_subPicHeightInLumaSample = 0;              // the height of subpicture in units of luma sample
  uint32_t         m_firstCtuInSubPic = 0;                      // the raster scan index of the first CTU in a subpicture
  uint32_t         m_lastCtuInSubPic = 0;                       // the raster scan index of the last CTU in a subpicture
  uint32_t         m_subPicLeft = 0;                            // the position of left boundary
  uint32_t         m_subPicRight = 0;                           // the position of right boundary
  uint32_t         m_subPicTop = 0;                             // the position of top boundary
  uint32_t         m_subPicBottom = 0;                          // the position of bottom boundary
  std::vector<uint32_t> m_ctuAddrInSubPic;                      // raster scan addresses of all the CTUs in the slice

  bool             m_treatedAsPicFlag = false;                  // whether the subpicture is treated as a picture in the decoding process excluding in-loop filtering operations
  bool             m_loopFilterAcrossSubPicEnabledFlag = false; // whether in-loop filtering operations may be performed across the boundaries of the subpicture
  uint32_t         m_numSlicesInSubPic = 0;                     // Number of slices contained in this subpicture

public:
  SubPic() {};
  virtual ~SubPic() {};

  void             setSubPicID (uint32_t u)                {         m_subPicID = u;       }
  uint32_t         getSubPicID   ()                  const { return  m_subPicID;           }
  void             setSubPicIdx (uint32_t u)               {         m_subPicIdx = u;      }
  uint32_t         getSubPicIdx ()                   const { return  m_subPicIdx;          }
  void             setNumCTUsInSubPic   (uint32_t u)       {         m_numCTUsInSubPic = u;       }
  uint32_t         getNumCTUsInSubPic   ()           const { return  m_numCTUsInSubPic;           }
  void             setSubPicCtuTopLeftX (uint32_t u)       {         m_subPicCtuTopLeftX = u;     }
  uint32_t         getSubPicCtuTopLeftX ()           const { return  m_subPicCtuTopLeftX;         }
  void             setSubPicCtuTopLeftY (uint32_t u)       {         m_subPicCtuTopLeftY = u;     }
  uint32_t         getSubPicCtuTopLeftY ()           const { return  m_subPicCtuTopLeftY;         }
  void             setSubPicWidthInCTUs (uint32_t u)       {         m_subPicWidth = u;           }
  uint32_t         getSubPicWidthInCTUs ()           const { return  m_subPicWidth;               }
  void             setSubPicHeightInCTUs(uint32_t u)       {         m_subPicHeight = u;          }
  uint32_t         getSubPicHeightInCTUs()           const { return  m_subPicHeight;              }
  void             setFirstCTUInSubPic  (uint32_t u)       {         m_firstCtuInSubPic = u;      }
  uint32_t         getFirstCTUInSubPic  ()           const { return  m_firstCtuInSubPic;          }
  void             setLastCTUInSubPic   (uint32_t u)       {         m_lastCtuInSubPic = u;       }
  uint32_t         getLastCTUInSubPic   ()           const { return  m_lastCtuInSubPic;           }
  void             setSubPicLeft        (uint32_t u)       {         m_subPicLeft = u;            }
  uint32_t         getSubPicLeft        ()           const { return  m_subPicLeft;                }
  void             setSubPicRight       (uint32_t u)       {         m_subPicRight = u;           }
  uint32_t         getSubPicRight       ()           const { return  m_subPicRight;               }
  void             setSubPicTop         (uint32_t u)       {         m_subPicTop = u;             }
  uint32_t         getSubPicTop         ()           const { return  m_subPicTop;                 }
  void             setSubPicBottom      (uint32_t u)       {         m_subPicBottom = u;          }
  uint32_t         getSubPicBottom      ()           const { return  m_subPicBottom;              }

  void             setSubPicWidthInLumaSample (uint32_t u) {         m_subPicWidthInLumaSample = u;   }
  uint32_t         getSubPicWidthInLumaSample()      const { return  m_subPicWidthInLumaSample;       }
  void             setSubPicHeightInLumaSample(uint32_t u) {         m_subPicHeightInLumaSample = u;  }
  uint32_t         getSubPicHeightInLumaSample()     const { return  m_subPicHeightInLumaSample;      }

  std::vector<uint32_t> getCtuAddrList  ()           const { return  m_ctuAddrInSubPic;           }
  void                  clearCTUAddrList()                 { m_ctuAddrInSubPic.clear(); }
  void                  addCTUsToSubPic(std::vector<uint32_t> ctuAddrInSlice)
  {
    for (auto ctu:ctuAddrInSlice)
    {
      m_ctuAddrInSubPic.push_back(ctu);
    }
  }
  void  addAllCtusInPicToSubPic(uint32_t startX, uint32_t stopX, uint32_t startY, uint32_t stopY, uint32_t picWidthInCtbsY)
  {
    CHECK(startX >= stopX || startY >= stopY, "Invalid slice definition");
    for (uint32_t ctbY = startY; ctbY < stopY; ctbY++)
    {
      for (uint32_t ctbX = startX; ctbX < stopX; ctbX++)
      {
        m_ctuAddrInSubPic.push_back(ctbY * picWidthInCtbsY + ctbX);
      }
    }
  }
  bool                 isContainingPos(const Position& pos) const
  {
    return pos.x >= m_subPicLeft && pos.x <= m_subPicRight && pos.y >= m_subPicTop  && pos.y <= m_subPicBottom;
  }
  void             setTreatedAsPicFlag           (bool u)  {         m_treatedAsPicFlag = u;   }
  bool             getTreatedAsPicFlag           ()  const { return  m_treatedAsPicFlag;       }
  void             setloopFilterAcrossEnabledFlag(bool u)  {         m_loopFilterAcrossSubPicEnabledFlag = u; }
  bool             getloopFilterAcrossEnabledFlag()  const { return  m_loopFilterAcrossSubPicEnabledFlag;     }

  bool             isFirstCTUinSubPic(uint32_t ctuAddr) const { return  ctuAddr == m_firstCtuInSubPic;  }
  bool             isLastCTUinSubPic(uint32_t ctuAddr)  const { return  ctuAddr == m_lastCtuInSubPic;   }
  void             setNumSlicesInSubPic( uint32_t val )    { m_numSlicesInSubPic = val; }
  uint32_t         getNumSlicesInSubPic() const            { return m_numSlicesInSubPic; }

  bool containsCtu(const Position& pos) const
  {
    return pos.x >= m_subPicCtuTopLeftX && pos.x < m_subPicCtuTopLeftX + m_subPicWidth &&
           pos.y >= m_subPicCtuTopLeftY && pos.y < m_subPicCtuTopLeftY + m_subPicHeight;
  }
  bool containsCtu(int ctuAddr) const
  {
    for (auto & addr : m_ctuAddrInSubPic)
    {
      if (addr == ctuAddr)
      {
        return true;
      }
    }
    return false;
  }
};

struct ChromaQpAdj
{
  union
  {
    struct
    {
      int cbOffset;
      int crOffset;
      int jointCbCrOffset;
    } comp;
    int offset[3];
  } u;
};

// PPS class
class PPS
{
private:
  int              m_ppsId = 0;   // pic_parameter_set_id
  int              m_spsId = 0;   // seq_parameter_set_id
  int              m_picInitQPMinus26;
  bool             m_useDQP;
  bool             m_usePPSChromaTool;
  bool             m_sliceChromaQpFlag;   // slicelevel_chroma_qp_flag

  int              m_layerId;
  int              m_temporalId;
  int              m_puCounter;

  // access channel

  int              m_chromaCbQpOffset;
  int              m_chromaCrQpOffset;
  bool             m_chromaJointCbCrQpOffsetPresentFlag;
  int              m_chromaCbCrQpOffset;

  // Chroma QP Adjustments
  int              m_chromaQpOffsetListLen; // size (excludes the null entry used in the following array).

  // Array includes entry [0] for the null offset used when  cu_chroma_qp_offset_flag=0, and entries
  // [cu_chroma_qp_offset_idx+1...] otherwis
  ChromaQpAdj      m_chromaQpAdjTableIncludingNullEntry[1 + MAX_QP_OFFSET_LIST_SIZE];

  uint32_t         m_numRefIdxDefaultActive[NUM_REF_PIC_LIST_01];

  bool             m_rpl1IdxPresentFlag;

  bool             m_useWeightedPred;                   // Use of Weighting Prediction (P_SLICE)
  bool             m_useWeightedBiPred;                 // Use of Weighting Bi-Prediction (B_SLICE)
  bool             m_outputFlagPresentFlag;             // Indicates the presence of output_flag in slice header
  uint32_t         m_numSubPics;                        // number of sub-pictures used - must match SPS
  bool             m_subPicIdMappingInPpsFlag;
  uint32_t         m_subPicIdLen;                       // sub-picture ID length in bits

  std::vector<uint16_t> m_subPicId;                     // sub-picture ID for each sub-picture in the sequence
  bool             m_noPicPartitionFlag;                // no picture partitioning flag - single slice, single tile
  uint8_t          m_log2CtuSize;                       // log2 of the CTU size - required to match corresponding value in SPS
  uint8_t          m_ctuSize;                           // CTU size
  uint32_t         m_picWidthInCtu;                     // picture width in units of CTUs
  uint32_t         m_picHeightInCtu;                    // picture height in units of CTUs
  uint32_t         m_numExpTileCols;                    // number of explicitly specified tile columns
  uint32_t         m_numExpTileRows;                    // number of explicitly specified tile rows
  uint32_t         m_numTileCols;                       // number of tile columns
  uint32_t         m_numTileRows;                       // number of tile rows
  std::vector<uint32_t> m_tileColWidth;                 // tile column widths in units of CTUs
  std::vector<uint32_t> m_tileRowHeight;                // tile row heights in units of CTUs
  std::vector<uint32_t> m_tileColBd;                    // tile column left-boundaries in units of CTUs
  std::vector<uint32_t> m_tileRowBd;                    // tile row top-boundaries in units of CTUs
  std::vector<TileIdx> m_ctuToTileCol;                  // mapping between CTU horizontal address and tile column index
  std::vector<TileIdx> m_ctuToTileRow;                  // mapping between CTU vertical address and tile row index
  bool             m_rectSliceFlag;                     // rectangular slice flag
  bool             m_singleSlicePerSubPicFlag;          // single slice per sub-picture flag
  std::vector<uint32_t> m_ctuToSubPicIdx;               // mapping between CTU and Sub-picture index
  uint32_t         m_numSlicesInPic;                    // number of rectangular slices in the picture (raster-scan slice specified at slice level)
  bool             m_tileIdxDeltaPresentFlag;           // tile index delta present flag
  std::vector<RectSlice> m_rectSlices;                  // list of rectangular slice signalling parameters
  std::vector<SliceMap>  m_sliceMap;                    // list of CTU maps for each slice in the picture
  std::vector<SubPic>      m_subPics;                   // list of subpictures in the picture
  bool             m_loopFilterAcrossTilesEnabledFlag;  // loop filtering applied across tiles flag
  bool             m_loopFilterAcrossSlicesEnabledFlag; // loop filtering applied across slices flag


  bool             m_cabacInitPresentFlag;

  bool             m_pictureHeaderExtensionPresentFlag;   // picture header extension flags present in picture headers or not
  bool             m_sliceHeaderExtensionPresentFlag;
  bool             m_deblockingFilterControlPresentFlag;
  bool             m_deblockingFilterOverrideEnabledFlag;
  bool             m_ppsDeblockingFilterDisabledFlag;
  int              m_deblockingFilterBetaOffsetDiv2;      // beta offset for deblocking filter
  int              m_deblockingFilterTcOffsetDiv2;        // tc offset for deblocking filter
  int              m_deblockingFilterCbBetaOffsetDiv2;    // beta offset for Cb deblocking filter
  int              m_deblockingFilterCbTcOffsetDiv2;      // tc offset for Cb deblocking filter
  int              m_deblockingFilterCrBetaOffsetDiv2;    // beta offset for Cr deblocking filter
  int              m_deblockingFilterCrTcOffsetDiv2;      // tc offset for Cr deblocking filter
  bool             m_listsModificationPresentFlag;

  bool             m_rplInfoInPhFlag;
  bool             m_dbfInfoInPhFlag;
  bool             m_saoInfoInPhFlag;
  bool             m_alfInfoInPhFlag;
  bool             m_wpInfoInPhFlag;
  bool             m_qpDeltaInfoInPhFlag;
  bool             m_mixedNaluTypesInPicFlag;

  bool             m_conformanceWindowFlag;
  uint32_t         m_picWidthInLumaSamples;
  uint32_t         m_picHeightInLumaSamples;
  Window           m_conformanceWindow;
  bool             m_explicitScalingWindowFlag;
  Window           m_scalingWindow;

  bool             m_wrapAroundEnabledFlag;               //< reference wrap around enabled or not
  unsigned         m_picWidthMinusWrapAroundOffset;          // <pic_width_in_minCbSizeY - wraparound_offset_in_minCbSizeY
  unsigned         m_wrapAroundOffset;                    //< reference wrap around offset in luma samples

public:
  PreCalcValues   *pcv;

public:
  PPS();
  virtual ~PPS();

  int                    getPPSId() const  { return m_ppsId; }
  void                   setPPSId(int i)   { m_ppsId = i; }
  int                    getSPSId() const  { return m_spsId; }
  void                   setSPSId(int i)   { m_spsId = i; }

  void                   setTemporalId( int i ) { m_temporalId = i; }
  int                    getTemporalId() const  { return m_temporalId; }
  void                   setPuCounter(int i)    { m_puCounter = i; }
  int                    getPuCounter() const   { return m_puCounter; }
  void                   setLayerId( int i )    { m_layerId = i; }
  int                    getLayerId() const     { return m_layerId; }

  int                    getPicInitQPMinus26() const                                      { return  m_picInitQPMinus26;                   }
  void                   setPicInitQPMinus26( int i )                                     { m_picInitQPMinus26 = i;                       }
  bool                   getUseDQP() const                                                { return m_useDQP;                              }
  void                   setUseDQP( bool b )                                              { m_useDQP   = b;                               }
  bool                   getPPSChromaToolFlag()                                     const { return  m_usePPSChromaTool;                   }
  void                   setPPSChromaToolFlag(bool b)                                     { m_usePPSChromaTool = b;                       }
  bool                   getSliceChromaQpFlag() const                                     { return m_sliceChromaQpFlag; }
  void                   setSliceChromaQpFlag(bool b)                                     { m_sliceChromaQpFlag = b; }

  bool                   getJointCbCrQpOffsetPresentFlag() const                          { return m_chromaJointCbCrQpOffsetPresentFlag;   }
  void                   setJointCbCrQpOffsetPresentFlag(bool b)                          { m_chromaJointCbCrQpOffsetPresentFlag = b;      }

  void                   setQpOffset(ComponentID compID, int i );
  int                    getQpOffset(ComponentID compID) const
  {
    return (compID==COMPONENT_Y) ? 0 : (compID==COMPONENT_Cb ? m_chromaCbQpOffset : compID==COMPONENT_Cr ? m_chromaCrQpOffset : m_chromaCbCrQpOffset );
  }

  bool                   getCuChromaQpOffsetListEnabledFlag() const                       { return getChromaQpOffsetListLen()>0;            }
  int                    getChromaQpOffsetListLen() const                                 { return m_chromaQpOffsetListLen;                 }
  void                   clearChromaQpOffsetList()                                        { m_chromaQpOffsetListLen = 0;                    }
  const ChromaQpAdj&     getChromaQpOffsetListEntry(int cuChromaQpOffsetIdxPlus1) const;
  void                   setChromaQpOffsetListEntry(int cuChromaQpOffsetIdxPlus1, int cbOffset, int crOffset, int jointCbCrOffset);

  void                   setNumRefIdxDefaultActive(RefPicList l, int n)                   { m_numRefIdxDefaultActive[l] = n; }
  int                    getNumRefIdxDefaultActive(RefPicList l) const                    { return m_numRefIdxDefaultActive[l]; }

  void                   setRpl1IdxPresentFlag(bool isPresent)                            { m_rpl1IdxPresentFlag = isPresent;             }
  uint32_t               getRpl1IdxPresentFlag() const                                    { return m_rpl1IdxPresentFlag;                  }

  bool                   getUseWP() const                                                 { return m_useWeightedPred; }
  bool                   getWPBiPred() const                                              { return m_useWeightedBiPred;                   }
  void                   setUseWP(bool b)                                                  { m_useWeightedPred = b; }
  void                   setWPBiPred( bool b )                                            { m_useWeightedBiPred = b;                      }

  void                   setWrapAroundEnabledFlag(bool b)                                 { m_wrapAroundEnabledFlag = b;                  }
  bool                   getWrapAroundEnabledFlag() const { return m_wrapAroundEnabledFlag; }
  void                   setPicWidthMinusWrapAroundOffset(unsigned offset)                { m_picWidthMinusWrapAroundOffset = offset;     }
  unsigned               getPicWidthMinusWrapAroundOffset() const                         { return m_picWidthMinusWrapAroundOffset;       }
  void                   setWrapAroundOffset(unsigned offset)                             { m_wrapAroundOffset = offset;                  }
  unsigned               getWrapAroundOffset() const                                      { return m_wrapAroundOffset;                    }
  void                   setOutputFlagPresentFlag(bool b) { m_outputFlagPresentFlag = b; }
  bool                   getOutputFlagPresentFlag() const { return m_outputFlagPresentFlag; }
  void                   setNumSubPics(uint32_t u )                                       { CHECK( u >= MAX_NUM_SUB_PICS, "Maximum number of subpictures exceeded" );
                                                                                            m_numSubPics = u;
                                                                                            m_subPicId.resize(m_numSubPics);
                                                                                          }
  uint32_t               getNumSubPics( ) const                                           { return  m_numSubPics;                         }
  void                   setSubPicIdMappingInPpsFlag( bool b )                            { m_subPicIdMappingInPpsFlag = b;               }
  bool                   getSubPicIdMappingInPpsFlag() const                              { return m_subPicIdMappingInPpsFlag;            }
  void                   setSubPicIdLen( uint32_t u )                                     { m_subPicIdLen = u;                            }
  uint32_t               getSubPicIdLen() const                                           { return  m_subPicIdLen;                        }
  void                   setSubPicId( int i, uint16_t u )                                 { m_subPicId[i] = u;     }
  void                   setSubPicId(const std::vector<uint16_t> &v)                      { CHECK(v.size()!=m_numSubPics, "number of vector entries must be equal to numSubPics") ; m_subPicId = v; }
  uint16_t               getSubPicId( int i ) const                                       { return  m_subPicId[i]; }
  const std::vector<uint16_t> getSubPicIds() const                                        { return  m_subPicId; }
  uint32_t               getSubPicIdxFromSubPicId( uint32_t subPicId ) const;
  void                   setNoPicPartitionFlag( bool b )                                  { m_noPicPartitionFlag = b;                     }
  bool                   getNoPicPartitionFlag( ) const                                   { return  m_noPicPartitionFlag;                 }
  void                   setLog2CtuSize( uint8_t u )                                      { m_log2CtuSize = u; m_ctuSize = 1 << m_log2CtuSize;
                                                                                            m_picWidthInCtu = (m_picWidthInLumaSamples  + m_ctuSize - 1) / m_ctuSize;
                                                                                            m_picHeightInCtu = (m_picHeightInLumaSamples  + m_ctuSize - 1) / m_ctuSize; }
  uint8_t                getLog2CtuSize( ) const                                          { return  m_log2CtuSize;                        }
  uint8_t                getCtuSize( ) const                                              { return  m_ctuSize;                            }
  uint32_t               getPicWidthInCtu( ) const                                        { return  m_picWidthInCtu;                      }
  uint32_t               getPicHeightInCtu( ) const                                       { return  m_picHeightInCtu;                     }
  void                   setNumExpTileColumns( uint32_t u )                               { m_numExpTileCols = u;                         }
  uint32_t               getNumExpTileColumns( ) const                                    { return  m_numExpTileCols;                     }
  void                   setNumExpTileRows( uint32_t u )                                  { m_numExpTileRows = u;                         }
  uint32_t               getNumExpTileRows( ) const                                       { return  m_numExpTileRows;                     }
  void                   setNumTileColumns( uint32_t u )                                  { m_numTileCols = u;                            }
  uint32_t               getNumTileColumns( ) const                                       { return  m_numTileCols;                        }
  void                   setNumTileRows( uint32_t u )                                     { m_numTileRows = u;                            }
  uint32_t               getNumTileRows( ) const                                          { return  m_numTileRows;                        }
  uint32_t               getNumTiles( ) const                                             { return  m_numTileCols * m_numTileRows;        }
  void                   setTileColumnWidths( std::vector<uint32_t> widths )              { m_tileColWidth = widths;                      }
  void                   setTileRowHeights( std::vector<uint32_t> heights )               { m_tileRowHeight = heights;                    }
  void                   addTileColumnWidth( uint32_t u )                                 { CHECK( m_tileColWidth.size()  >= MAX_TILE_COLS, "Number of tile columns exceeds valid range" ); m_tileColWidth.push_back(u);    }
  void                   addTileRowHeight( uint32_t u )                                   { m_tileRowHeight.push_back(u);   }
  uint32_t               getTileColumnWidth( int idx ) const                              { CHECK( idx >= m_tileColWidth.size(), "Tile column index exceeds valid range" );                 return  m_tileColWidth[idx];    }
  uint32_t               getTileRowHeight( int idx ) const                                { CHECK( idx >= m_tileRowHeight.size(), "Tile row index exceeds valid range" );                   return  m_tileRowHeight[idx];   }
  uint32_t               getTileColumnBd( int idx ) const                                 { CHECK( idx >= m_tileColBd.size(), "Tile column index exceeds valid range" );                    return  m_tileColBd[idx];       }
  uint32_t               getTileRowBd( int idx ) const                                    { CHECK( idx >= m_tileRowBd.size(), "Tile row index exceeds valid range" );                       return  m_tileRowBd[idx];       }
  uint32_t               ctuToTileCol( int ctuX ) const                                   { CHECK( ctuX >= m_ctuToTileCol.size(), "CTU address index exceeds valid range" ); return  m_ctuToTileCol[ctuX];                  }
  uint32_t               ctuToTileRow( int ctuY ) const                                   { CHECK( ctuY >= m_ctuToTileRow.size(), "CTU address index exceeds valid range" ); return  m_ctuToTileRow[ctuY];                  }
  uint32_t               ctuToTileColBd( int ctuX ) const                                 { return  getTileColumnBd(ctuToTileCol( ctuX ));                                                                                  }
  uint32_t               ctuToTileRowBd( int ctuY ) const                                 { return  getTileRowBd(ctuToTileRow( ctuY ));                                                                                     }
  bool                   ctuIsTileColBd( int ctuX ) const                                 { return  ctuX == ctuToTileColBd( ctuX );                                                                                         }
  bool                   ctuIsTileRowBd( int ctuY ) const                                 { return  ctuY == ctuToTileRowBd( ctuY );                                                                                         }
  TileIdx                getTileIdx( uint32_t ctuX, uint32_t ctuY ) const                 { return (ctuToTileRow( ctuY ) * getNumTileColumns()) + ctuToTileCol( ctuX );                                                     }
  TileIdx                getTileIdx( uint32_t ctuRsAddr) const                            { return getTileIdx( ctuRsAddr % m_picWidthInCtu,  ctuRsAddr / m_picWidthInCtu );                                                 }
  TileIdx                getTileIdx( const Position& pos ) const                          { return getTileIdx( pos.x / m_ctuSize, pos.y / m_ctuSize );                                                                      }
  void                   setRectSliceFlag( bool b )                                       { m_rectSliceFlag = b;                                                                                                            }
  bool                   getRectSliceFlag( ) const                                        { return  m_rectSliceFlag;                                                                                                        }
  void                   setSingleSlicePerSubPicFlag( bool b )                            { m_singleSlicePerSubPicFlag = b;                                                                                                 }
  bool                   getSingleSlicePerSubPicFlag( ) const                             { return  m_singleSlicePerSubPicFlag;                                                                                             }
  uint32_t               getCtuToSubPicIdx( int idx ) const                               { CHECK( idx >= m_ctuToSubPicIdx.size(), "CTU address index exceeds valid range" ); CHECK( getNumSubPics() < 1, "Number of subpicture cannot be 0" ); return  m_ctuToSubPicIdx[ idx ]; }
  void                   setNumSlicesInPic( uint32_t u )                                  { CHECK( u > MAX_SLICES, "Number of slices in picture exceeds valid range" ); m_numSlicesInPic = u;                               }
  uint32_t               getNumSlicesInPic( ) const                                       { return  m_numSlicesInPic;                                                                                                       }
  void                   setTileIdxDeltaPresentFlag( bool b )                             { m_tileIdxDeltaPresentFlag = b;                                                                                                  }
  bool                   getTileIdxDeltaPresentFlag( ) const                              { return  m_tileIdxDeltaPresentFlag;                                                                                              }
  void                   setSliceWidthInTiles( int idx, uint32_t u )                      { CHECK( idx >= m_numSlicesInPic, "Slice index exceeds valid range" );    m_rectSlices[idx].setSliceWidthInTiles( u );            }
  uint32_t               getSliceWidthInTiles( int idx ) const                            { CHECK( idx >= m_numSlicesInPic, "Slice index exceeds valid range" );    return  m_rectSlices[idx].getSliceWidthInTiles( );      }
  void                   setSliceHeightInTiles( int idx, uint32_t u )                     { CHECK( idx >= m_numSlicesInPic, "Slice index exceeds valid range" );    m_rectSlices[idx].setSliceHeightInTiles( u );           }
  uint32_t               getSliceHeightInTiles( int idx ) const                           { CHECK( idx >= m_numSlicesInPic, "Slice index exceeds valid range" );    return  m_rectSlices[idx].getSliceHeightInTiles( );     }
  void                   setNumSlicesInTile( int idx, uint32_t u )                        { CHECK( idx >= m_numSlicesInPic, "Slice index exceeds valid range" );    m_rectSlices[idx].setNumSlicesInTile( u );              }
  uint32_t               getNumSlicesInTile( int idx ) const                              { CHECK( idx >= m_numSlicesInPic, "Slice index exceeds valid range" );    return  m_rectSlices[idx].getNumSlicesInTile( );        }
  void                   setSliceHeightInCtu( int idx, uint32_t u )                       { CHECK( idx >= m_numSlicesInPic, "Slice index exceeds valid range" );    m_rectSlices[idx].setSliceHeightInCtu( u );             }
  uint32_t               getSliceHeightInCtu( int idx ) const                             { CHECK( idx >= m_numSlicesInPic, "Slice index exceeds valid range" );    return  m_rectSlices[idx].getSliceHeightInCtu( );       }
  void                   setSliceTileIdx(  int idx, uint32_t u )                          { CHECK( idx >= m_numSlicesInPic, "Slice index exceeds valid range" );    m_rectSlices[idx].setTileIdx( u );                      }
  uint32_t               getSliceTileIdx( int idx ) const                                 { CHECK( idx >= m_numSlicesInPic, "Slice index exceeds valid range" );    return  m_rectSlices[idx].getTileIdx( );                }
  void                   setRectSlices( std::vector<RectSlice> rectSlices )               { m_rectSlices = rectSlices;                                                                                                      }
  void                   setLoopFilterAcrossTilesEnabledFlag( bool b )                    { m_loopFilterAcrossTilesEnabledFlag = b;                                                                                         }
  bool                   getLoopFilterAcrossTilesEnabledFlag( ) const                     { return  m_loopFilterAcrossTilesEnabledFlag;                                                                                     }
  void                   setLoopFilterAcrossSlicesEnabledFlag( bool b )                   { m_loopFilterAcrossSlicesEnabledFlag = b;                                                                                        }
  bool                   getLoopFilterAcrossSlicesEnabledFlag( ) const                    { return  m_loopFilterAcrossSlicesEnabledFlag;                                                                                    }
  void                   resetTileSliceInfo();
  void                   initTiles();
  void                   initRectSlices();
  void                   initRectSliceMap(const SPS  *sps);
  std::vector<SubPic>    getSubPics()  const                                              { return m_subPics; };
  SubPic                 getSubPic(uint32_t idx) const                                    { return m_subPics[idx];}
  void                   initSubPic(const SPS &sps);
  const SubPic&          getSubPicFromPos(const Position& pos)  const;
  const SubPic&          getSubPicFromCU (const CodingUnit& cu) const;
  void                   initRasterSliceMap(const std::vector<uint32_t> &sizes );
  void                   checkSliceMap();
  SliceMap               getSliceMap(int idx) const                                       { CHECK( idx >= m_numSlicesInPic, "Slice index exceeds valid range" ); return m_sliceMap[idx];                             }

  void                   setCabacInitPresentFlag( bool flag )                             { m_cabacInitPresentFlag = flag;                }
  bool                   getCabacInitPresentFlag() const                                  { return m_cabacInitPresentFlag;                }
  void                   setDeblockingFilterControlPresentFlag(bool val)                  { m_deblockingFilterControlPresentFlag = val;   }
  bool                   getDeblockingFilterControlPresentFlag() const                    { return m_deblockingFilterControlPresentFlag;  }
  void                   setDeblockingFilterOverrideEnabledFlag(bool val)                 { m_deblockingFilterOverrideEnabledFlag = val;  }
  bool                   getDeblockingFilterOverrideEnabledFlag() const                   { return m_deblockingFilterOverrideEnabledFlag; }
  void                   setPPSDeblockingFilterDisabledFlag(bool val)                     { m_ppsDeblockingFilterDisabledFlag = val;      }
  bool                   getPPSDeblockingFilterDisabledFlag() const                       { return m_ppsDeblockingFilterDisabledFlag;     }
  void                   setDeblockingFilterBetaOffsetDiv2(int val)                       { m_deblockingFilterBetaOffsetDiv2 = val;       }
  int                    getDeblockingFilterBetaOffsetDiv2() const                        { return m_deblockingFilterBetaOffsetDiv2;      }
  void                   setDeblockingFilterTcOffsetDiv2(int val)                         { m_deblockingFilterTcOffsetDiv2 = val;         }
  int                    getDeblockingFilterTcOffsetDiv2() const                          { return m_deblockingFilterTcOffsetDiv2;        }
  void                   setDeblockingFilterCbBetaOffsetDiv2(int val)                     { m_deblockingFilterCbBetaOffsetDiv2 = val;     }
  int                    getDeblockingFilterCbBetaOffsetDiv2() const                      { return m_deblockingFilterCbBetaOffsetDiv2;    }
  void                   setDeblockingFilterCbTcOffsetDiv2(int val)                       { m_deblockingFilterCbTcOffsetDiv2 = val;       }
  int                    getDeblockingFilterCbTcOffsetDiv2() const                        { return m_deblockingFilterCbTcOffsetDiv2;      }
  void                   setDeblockingFilterCrBetaOffsetDiv2(int val)                     { m_deblockingFilterCrBetaOffsetDiv2 = val;     }
  int                    getDeblockingFilterCrBetaOffsetDiv2() const                      { return m_deblockingFilterCrBetaOffsetDiv2;    }
  void                   setDeblockingFilterCrTcOffsetDiv2(int val)                       { m_deblockingFilterCrTcOffsetDiv2 = val;       }
  int                    getDeblockingFilterCrTcOffsetDiv2() const                        { return m_deblockingFilterCrTcOffsetDiv2;      }
  bool                   getListsModificationPresentFlag() const                          { return m_listsModificationPresentFlag;        }
  void                   setListsModificationPresentFlag(bool b)                          { m_listsModificationPresentFlag = b;           }
  bool                   getPictureHeaderExtensionPresentFlag() const                     { return m_pictureHeaderExtensionPresentFlag;   }
  void                   setPictureHeaderExtensionPresentFlag(bool val)                   { m_pictureHeaderExtensionPresentFlag = val;    }
  bool                   getSliceHeaderExtensionPresentFlag() const                       { return m_sliceHeaderExtensionPresentFlag;     }
  void                   setSliceHeaderExtensionPresentFlag(bool val)                     { m_sliceHeaderExtensionPresentFlag = val;      }

  void                   setRplInfoInPhFlag(bool flag)                                    { m_rplInfoInPhFlag = flag;                     }
  bool                   getRplInfoInPhFlag() const                                       { return m_rplInfoInPhFlag;                     }
  void                   setDbfInfoInPhFlag(bool flag)                                    { m_dbfInfoInPhFlag = flag;                     }
  bool                   getDbfInfoInPhFlag() const                                       { return m_dbfInfoInPhFlag;                     }
  void                   setSaoInfoInPhFlag(bool flag)                                    { m_saoInfoInPhFlag = flag;                     }
  bool                   getSaoInfoInPhFlag() const                                       { return m_saoInfoInPhFlag;                     }
  void                   setAlfInfoInPhFlag(bool flag)                                    { m_alfInfoInPhFlag = flag;                     }
  bool                   getAlfInfoInPhFlag() const                                       { return m_alfInfoInPhFlag;                     }
  void                   setWpInfoInPhFlag(bool flag)                                     { m_wpInfoInPhFlag = flag;                      }
  bool                   getWpInfoInPhFlag() const                                        { return m_wpInfoInPhFlag;                      }
  void                   setQpDeltaInfoInPhFlag(bool flag)                                { m_qpDeltaInfoInPhFlag = flag;                 }
  bool                   getQpDeltaInfoInPhFlag() const                                   { return m_qpDeltaInfoInPhFlag; }


  void                    setPicWidthInLumaSamples(uint32_t u)                            { m_picWidthInLumaSamples = u; }
  uint32_t                getPicWidthInLumaSamples() const                                { return  m_picWidthInLumaSamples; }
  void                    setPicHeightInLumaSamples(uint32_t u)                           { m_picHeightInLumaSamples = u; }
  uint32_t                getPicHeightInLumaSamples() const                               { return  m_picHeightInLumaSamples; }

  void                    setConformanceWindowFlag(bool flag)                             { m_conformanceWindowFlag = flag; }
  bool                    getConformanceWindowFlag() const                                { return m_conformanceWindowFlag; }
  Window&                 getConformanceWindow()                                          { return  m_conformanceWindow; }
  const Window&           getConformanceWindow() const                                    { return  m_conformanceWindow; }
  void                    setConformanceWindow(Window& conformanceWindow)                 { m_conformanceWindow = conformanceWindow; }

  void                    setExplicitScalingWindowFlag(bool flag)                         { m_explicitScalingWindowFlag = flag; }
  bool                    getExplicitScalingWindowFlag() const                            { return m_explicitScalingWindowFlag; }
  Window&                 getScalingWindow()                                              { return  m_scalingWindow; }
  const Window&           getScalingWindow() const                                        { return  m_scalingWindow; }
  void                    setScalingWindow(Window& scalingWindow)                         { m_scalingWindow = scalingWindow; }

  bool                    getMixedNaluTypesInPicFlag() const                              { return m_mixedNaluTypesInPicFlag; }
  void                    setMixedNaluTypesInPicFlag(bool flag)                           { m_mixedNaluTypesInPicFlag = flag; }
};
