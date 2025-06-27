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

/** \file     CacheModel.h
    \brief    general cache class (header)
*/


#ifndef _CACHEMODEL_H_
#define _CACHEMODEL_H_
#include "Picture.h"

// function list
#if !JVET_J0090_MEMORY_BANDWITH_MEASURE
#define JVET_J0090_SET_CACHE_ENABLE( enable )          /* do nothing */
#define JVET_J0090_SET_REF_PICTURE( refPic, compID )   /* do nothing */
#define JVET_J0090_CACHE_ACCESS( src, fileName, line ) /* do nothing */
#else
#define JVET_J0090_SET_CACHE_ENABLE( enable )          m_cacheModel->setCacheEnable( enable )
#define JVET_J0090_SET_REF_PICTURE( refPic, compID )   m_cacheModel->setRefPicture( refPic, compID )
#define JVET_J0090_CACHE_ACCESS( src, fileName, line ) m_cacheModel->cacheAccess( src, fileName, line )



class CacheModel
{
private:
  // cache enable
  bool          m_cacheEnable;
  bool          m_cacheEnableFilter;
  // report level
  bool          m_frameReport;
  // cache parameters
  int           m_cacheLineSize;   // size of byte in each entry (shall be power of 2)
  int           m_numCacheLine;    // # of cache line
  int           m_numWay;          // # of way
  int           m_cacheSize;       // total entry numer (line number * way)
  int           m_cacheAddrMode;   // cache address mode
  int           m_cacheBlkWidth;   // block width in 2D access
  int           m_cacheBlkHeight;  // block height in 2D access
  // cache parameters for address calc
  int           m_shift;
  // cache entry
  size_t*       m_cacheAddr;
  int*          m_cachePoc;
  ComponentID*  m_cacheComp;
  bool*         m_available;
  // access Information
  int           m_refPoc;
  Pel*          m_base;
  ComponentID   m_compID;
  int           m_picWidth;
  // PLRU parameters
  int           m_treeDepth;
  int*          m_treeStatus;

  // stastical infromation for a frame
  int*          m_hitCount; // for each cache entry
  int           m_missHitCount; // for calc total bandwidth
  int           m_totalAccess;
  // stastical infromation for a sequence
  int64_t       m_hitCountSeq;
  int64_t       m_missHitCountSeq;
  int64_t       m_totalAccessSeq;
  int           m_frameCount;

public:
  CacheModel();
  ~CacheModel();
  bool isCacheEnable( ) { return m_cacheEnable; }
  void create(const std::string& cacheCfgFileName);
  void destroy( );
  void clear( );
  void reportFrame();
  void reportSequence();
  void cacheAccess( const Pel *addr, const std::string& fileName, const int lineNum );
  void accumulateFrame( );
  void setCacheEnable( bool enable );
  void setRefPicture( const Picture *refPic, const ComponentID compID );

protected:
  bool xIsCacheHit( int pos, size_t addr );
  int xCalcTreeSize( int way );
  int xCalcPower( int num );
  int xGetWay( int entry );
  size_t xMapAddress( size_t offset );
  void xConfigure(const std::string& filename);
  void xUpdateCache( int entry, size_t addr );
  void xUpdateCacheStatus( int entry, int way );
  // PLRU
  int xGetWayTreePLRU( int entry );
  void xUpdatePLRUStatus( int entry, int way );
};

#endif // JVET_J0090_MEMORY_BANDWITH_MEASURE
#endif // _CACHEMODEL_H_


