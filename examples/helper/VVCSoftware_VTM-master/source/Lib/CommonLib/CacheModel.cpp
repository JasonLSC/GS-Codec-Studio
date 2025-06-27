/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010 - 2025, ITU/ISO/IEC
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

/** \file     CacheModel.cpp
    \brief    general cache class
*/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <inttypes.h>
#define __STDC_FORMAT_MACROS
#ifdef WIN32
#define strdup _strdup
#endif

#include "Utilities/program_options_lite.h"
#include "CacheModel.h"
#if JVET_J0090_MEMORY_BANDWITH_MEASURE

#ifndef JVET_J0090_MEMORY_BANDWITH_MEASURE_PRINT_ACCESS_INFO
#define JVET_J0090_MEMORY_BANDWITH_MEASURE_PRINT_ACCESS_INFO 0
#endif
#ifndef JVET_J0090_MEMORY_BANDWITH_MEASURE_PRINT_FRAME
#define JVET_J0090_MEMORY_BANDWITH_MEASURE_PRINT_FRAME -1
#endif

enum CacheAddressMap
{
  CACHE_MODE_1D = 0,
  CACHE_MODE_2D,
  MAX_NUM_CACHE_MODE
};

namespace po = ProgramOptionsLite;

CacheModel::CacheModel()
{
  m_cacheEnable       = false;
  m_cacheEnableFilter = false;
  m_cacheLineSize     = 0;
  m_numCacheLine      = 0;
  m_numWay            = 0;
  m_cacheSize         = 0;
  m_shift             = 0;
  m_cacheAddr         = nullptr;
  m_cachePoc          = nullptr;
  m_cacheComp         = nullptr;
  m_available         = nullptr;
  m_refPoc            = 0;
  m_base              = nullptr;
  m_compID            = MAX_NUM_COMPONENT;
  m_hitCount          = nullptr;
  m_treeStatus        = nullptr;
  m_missHitCount      = 0;
  m_totalAccess       = 0;
  m_hitCountSeq       = 0;
  m_missHitCountSeq   = 0;
  m_totalAccessSeq    = 0;
  m_frameCount        = 0;
}

CacheModel::~CacheModel()
{
}

int CacheModel::xCalcPower( int num )
{
  int power = -1;

  for ( int i = 0 ; i < 32 ; i++ )
  {
    if ( num == (1 << i) )
    {
      power = i;
      break;
    }
  }
  if (power < 0)
  {
    THROW("non power of 2");
  }
  return power;
}

void CacheModel::xConfigure(const std::string& filename )
{
  po::Options opts;
  opts.addOptions()
  ("CacheEnable",   m_cacheEnable,   false, "Cache Enable" )
  ("CacheLineSize", m_cacheLineSize,   128, "Cache line size")
  ("NumCacheLine",  m_numCacheLine,     32, "Number of cache line")
  ("NumWay",        m_numWay,            4, "Number of way")
  ("CacheAddrMode", m_cacheAddrMode,     0, "Address mapping mode 0 : linear address 1 : 2D address")
  ("BlkWidth",      m_cacheBlkWidth,    32, "Block width in 2D address mode")
  ("BlkHeight",     m_cacheBlkHeight,   16, "Block height in 2D address mode")
  ("FrameReport",   m_frameReport,   false, "Report in each frame" )
  ;

  po::setDefaults(opts);
  po::parseConfigFile( opts, filename );

  if ( m_cacheLineSize > CACHE_MEM_ALIGN_SIZE )
  {
    fprintf( stderr, "cache line size is bigger that memory alignment\n" );
    fprintf( stderr, "This may lead mismatch among enviroments\n" );
  }
  if ( m_cacheAddrMode == CACHE_MODE_2D )
  {
    int blkSize = m_cacheBlkWidth * m_cacheBlkHeight;
    if (m_cacheLineSize % blkSize != 0 && blkSize % m_cacheLineSize)
    {
      THROW("CacheLineSize shall be multiple of BlkWidth x BlkHeight or BlkWidth x BlkHeight shall be multiple of CacheLineSize in 2D mode");
    }
  }
}

// initilize cache information such as size
void CacheModel::create(const std::string& cacheCfgFileName)
{
  bool init = cacheCfgFileName == "";

  if (cacheCfgFileName.length() > 1000)
  {
    THROW("config file name for cache model is too long. It shall be < 1000\n");
  }
  if ( init )
  {
    return; // no cache config
  }
  xConfigure(cacheCfgFileName);

  if ( !m_cacheEnable )
  {
    return;
  }

  // set parameters
  m_cacheSize = m_numCacheLine * m_numWay;
  // calc address calculation parameter
  m_shift = xCalcPower( m_cacheLineSize );
  // keep memory
  m_cacheAddr  = new size_t [m_cacheSize];
  m_cachePoc   = new int [m_cacheSize];
  m_cacheComp  = new ComponentID [m_cacheSize];
  m_available  = new bool  [m_cacheSize];
  m_hitCount   = new int  [m_cacheSize];
  // PLRU
  m_treeDepth  = xCalcPower( m_numWay );
  m_treeStatus = new int [m_numCacheLine];
  if ( m_cacheLineSize > 0 && m_numCacheLine > 0 && m_numWay > 0 )
  {
    m_cacheEnableFilter = true;
  }
}

// free memory
void CacheModel::destroy()
{
  if ( m_cacheAddr )
  {
    delete [] m_cacheAddr;
  }
  if ( m_cachePoc )
  {
    delete [] m_cachePoc;
  }
  if ( m_cacheComp )
  {
    delete [] m_cacheComp;
  }
  if ( m_available )
  {
    delete [] m_available;
  }
  if ( m_hitCount )
  {
    delete [] m_hitCount;
  }
  if ( m_treeStatus )
  {
    delete [] m_treeStatus;
  }
}

// clear cache status (set invalid for each entry)
void CacheModel::clear()
{
  if ( m_cacheEnable )
  {
    ::memset( m_available, 0, m_cacheSize * sizeof(bool) );
    ::memset( m_hitCount,  0, m_cacheSize * sizeof(int) );
    m_missHitCount = 0;
    m_totalAccess  = 0;
  }
}

// accuulate result for sequence level
void CacheModel::accumulateFrame( )
{
  if ( m_cacheEnable )
  {
    for ( int i = 0 ; i < m_cacheSize ; i++ )
    {
      m_hitCountSeq += m_hitCount[ i ];
    }
    m_missHitCountSeq += m_missHitCount;
    m_totalAccessSeq  += m_totalAccess;

    if ( m_totalAccessSeq < 0 )
    {
      fprintf( stdout, "detect overflow\n" );
    }
  }
}

// report bandwidth, hit ratio and so on in a Frame
void CacheModel::reportFrame( )
{
  if ( m_cacheEnable )
  {
    if ( m_frameReport )
    {
      int hitCount = 0;

      for ( int i = 0 ; i < m_cacheSize ; i++ )
      {
        hitCount += m_hitCount[ i ];
      }

      fprintf( stdout, "Cache Statics in frame %d\n", m_frameCount );
      fprintf( stdout, "Hit ratio %5.2f [%%]\n", (100 * (double)(hitCount)) / m_totalAccess );
      fprintf( stdout, "Required bandwidth %.1f [MB]\n", ((double)(m_missHitCount) * m_cacheLineSize) / (1024 * 1024) );
    }
    m_frameCount++;
  }
}

void CacheModel::reportSequence( )
{
  if ( m_cacheEnable )
  {
    fprintf( stdout, "Cache config\n" );
    fprintf( stdout, "Cache line size: %d\n", m_cacheLineSize  );
    fprintf( stdout, "Cache line number %d\n", m_numCacheLine );
    fprintf( stdout, "Cache way number %d\n\n", m_numWay );

    fprintf( stdout, "Cache Statics in total\n" );
    fprintf( stdout, "Hit ratio %5.2f [%%]\n", (100 * (double)(m_hitCountSeq)) / m_totalAccessSeq );
#ifdef _MSC_VER
    fprintf( stdout, "Hit count / total %I64d / %I64d\n", m_hitCountSeq, m_totalAccessSeq );
#else
    fprintf( stdout, "Hit count / total %" PRIi64 " / %" PRIi64 "\n", m_hitCountSeq, m_totalAccessSeq );
#endif
    fprintf( stdout, "Required bandwidth %.1f [MB] / frame\n", (((double)m_missHitCountSeq) * m_cacheLineSize) / (m_frameCount * 1024 * 1024) );
  }
}

void CacheModel::setRefPicture( const Picture *refPic, const ComponentID CompID )
{
  m_refPoc   = refPic->getPOC();
  m_base     = refPic->getOrigin(PIC_RECONSTRUCTION, CompID);
  m_compID   = CompID;
  m_picWidth = refPic->getRecoBuf(CompID).stride;
}

bool CacheModel::xIsCacheHit( int pos, size_t addr )
{
  bool ret = false;

  if ( m_available[pos] )
  {
    if ( addr == m_cacheAddr[pos] )
    {
      if ( m_refPoc == m_cachePoc[pos] )
      {
        if ( m_compID == m_cacheComp[pos] )
        {
          ret = true;
        }
      }
    }
  }
  return ret;
}

//-- PLRU

int CacheModel::xGetWayTreePLRU( int entry )
{
  int shift  = 0;
  int way    = 0;
  for ( int i = 0; i < m_treeDepth ; i++ )
  {
    int flag  = (m_treeStatus[ entry ] >> shift) & 0x1;
    shift = (shift << 1) + flag + 1;
    way   = (way << 1) | (flag ^ 0x1);
  }
  xUpdateCacheStatus( entry, way );

  return way;
}

void CacheModel::xUpdatePLRUStatus( int entry, int way )
{
  int val   = m_treeStatus[ entry ];
  int shift = 0;

  for ( int i = 0 ; i < m_treeDepth ; i++ )
  {
    int flag = (way >> (m_treeDepth - i - 1)) & 0x1;
    val = (val & (~0 ^ (1 << shift))) | (flag << shift); // only set shift-th bit
    shift = (shift << 1) + 2 - flag;
  }

  m_treeStatus[ entry ] = val;
}

//-- other cache alg. (for future use)

// get update way based on each update algorithm (Now Tree PLRU only)
int CacheModel::xGetWay( int entry )
{
  // single way
  if ( m_numWay == 1 )
  {
    return 0;
  }
  // multiway
  return xGetWayTreePLRU( entry );
}

// update cache entry
void CacheModel::xUpdateCache( int entry, size_t addr )
{
  int way = xGetWay( entry );

  if (entry * m_numWay + way >= m_cacheSize || entry * m_numWay + way < 0)
  {
    THROW("incorrect cache info");
  }

  m_cacheAddr[ entry * m_numWay + way ] = addr;
  m_cachePoc[ entry * m_numWay + way ]  = m_refPoc;
  m_cacheComp[ entry * m_numWay + way ] = m_compID;
  m_available[ entry * m_numWay + way ] = true;

}

void CacheModel::xUpdateCacheStatus( int entry, int way )
{
  if ( m_numWay == 1 )
  {
    return;
  }
  xUpdatePLRUStatus( entry, way );
}

size_t CacheModel::xMapAddress( size_t offset ) {

  size_t ret;
  size_t xInPic, yInPic, blkPosX, blkPosY, xInBlk, yInBlk;

  switch (m_cacheAddrMode)
  {
  case CACHE_MODE_1D:   // diret mapping
    return offset;
  case CACHE_MODE_2D:   // 2D address mapping
    xInPic  = offset % m_picWidth;
    yInPic  = offset / m_picWidth;
    blkPosX = xInPic / m_cacheBlkWidth;
    blkPosY = yInPic / m_cacheBlkHeight;
    xInBlk  = xInPic % m_cacheBlkWidth;
    yInBlk  = yInPic % m_cacheBlkHeight;
    ret     = m_picWidth * blkPosY * m_cacheBlkHeight;
    ret += blkPosX * m_cacheBlkWidth * m_cacheBlkHeight;
    ret += yInBlk * m_cacheBlkWidth;
    ret += xInBlk;
    return ret;
  default: THROW("Unknown address mode " << m_cacheAddrMode); return 0;
  }
}

// check cache hit/miss
void CacheModel::cacheAccess( const Pel *addr, const std::string& fileName, const int lineNum )
{
  if ( !m_cacheEnable || !m_cacheEnableFilter )
  {
    return;
  }
  bool hit = false;
  size_t cacheAddr = xMapAddress( (size_t) (addr - m_base) ) >> m_shift;
  int  entry = (int) (cacheAddr % m_numCacheLine);
  int  pos   = entry * m_numWay;
  int  way;

  // check cache hit in each way
  for ( way = 0 ; way < m_numWay ; way++ )
  {
    if (xIsCacheHit(pos + way, cacheAddr))
    {
      hit = true;
      break;
    }
  }
#if JVET_J0090_MEMORY_BANDWITH_MEASURE_PRINT_ACCESS_INFO
  if ( m_frameCount == JVET_J0090_MEMORY_BANDWITH_MEASURE_PRINT_FRAME )
  {
    fprintf( stdout, "%s %d:%p\n", fileName.c_str(), lineNum, addr );
  }
#endif

  if ( !hit )
  {
    // read data from external memory
    m_missHitCount++;
    // update cache entry
    xUpdateCache( entry, cacheAddr );
  }
  else
  {
    // update hit status
    m_hitCount[ pos + way ] ++;
    xUpdateCacheStatus( entry, way );
  }
  m_totalAccess++;
}

void CacheModel::setCacheEnable( bool enable )
{
  m_cacheEnableFilter = enable;
}
#endif // JVET_J0090_MEMORY_BANDWITH_MEASURE
