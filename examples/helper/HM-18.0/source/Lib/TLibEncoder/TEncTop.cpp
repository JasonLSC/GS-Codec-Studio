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

/** \file     TEncTop.cpp
    \brief    encoder class
*/

#include "TLibCommon/CommonDef.h"
#include "TEncTop.h"
#include "TEncPic.h"
#include "TLibCommon/TComChromaFormat.h"
#if FAST_BIT_EST
#include "TLibCommon/ContextModel.h"
#endif

//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

TEncTop::TEncTop() :
    m_spsMap(MAX_NUM_SPS)
  , m_ppsMap(MAX_NUM_PPS)
{
  m_iPOCLast          = -1;
  m_iNumPicRcvd       =  0;
  m_uiNumAllPicCoded  =  0;
  m_pppcRDSbacCoder   =  NULL;
  m_pppcBinCoderCABAC =  NULL;
  m_cRDGoOnSbacCoder.init( &m_cRDGoOnBinCoderCABAC );
#if ENC_DEC_TRACE
  if (g_hTrace == NULL)
  {
    g_hTrace = fopen( "TraceEnc.txt", "wb" );
  }
  g_bJustDoIt = g_bEncDecTraceDisable;
  g_nSymbolCounter = 0;
#endif

  m_iMaxRefPicNum     = 0;

#if FAST_BIT_EST
  ContextModel::buildNextStateTable();
#endif
}

TEncTop::~TEncTop()
{
#if ENC_DEC_TRACE
  if (g_hTrace != stdout)
  {
    fclose( g_hTrace );
  }
#endif
}

Void TEncTop::create ()
{
  // initialize global variables
  initROM();

  // create processing unit classes
  m_cGOPEncoder.        create( );
  m_cSliceEncoder.      create( getSourceWidth(), getSourceHeight(), m_chromaFormatIDC, m_maxCUWidth, m_maxCUHeight, m_maxTotalCUDepth );
  m_cCuEncoder.         create( m_maxTotalCUDepth, m_maxCUWidth, m_maxCUHeight, m_chromaFormatIDC );
  if (m_bUseSAO)
  {
    m_cEncSAO.create( getSourceWidth(), getSourceHeight(), m_chromaFormatIDC, m_maxCUWidth, m_maxCUHeight, m_maxTotalCUDepth, m_log2SaoOffsetScale[CHANNEL_TYPE_LUMA], m_log2SaoOffsetScale[CHANNEL_TYPE_CHROMA] );
    m_cEncSAO.createEncData(getSaoCtuBoundary());
  }
#if ADAPTIVE_QP_SELECTION
  if (m_bUseAdaptQpSelect)
  {
    m_cTrQuant.initSliceQpDelta();
  }
#endif

  m_cLoopFilter.create( m_maxTotalCUDepth );

  if ( m_RCEnableRateControl )
  {
#if JVET_Y0105_SW_AND_QDF
    m_cRateCtrl.init( m_framesToBeEncoded, m_RCTargetBitrate, (Int)( (Double)m_iFrameRate/m_temporalSubsampleRatio + 0.5), m_iGOPSize, m_uiIntraPeriod, m_iSourceWidth, m_iSourceHeight,
                      m_maxCUWidth, m_maxCUHeight,m_RCKeepHierarchicalBit, m_RCUseLCUSeparateModel, m_GOPList );
#else
    m_cRateCtrl.init( m_framesToBeEncoded, m_RCTargetBitrate, (Int)( (Double)m_iFrameRate/m_temporalSubsampleRatio + 0.5), m_iGOPSize, m_iSourceWidth, m_iSourceHeight,
                      m_maxCUWidth, m_maxCUHeight,m_RCKeepHierarchicalBit, m_RCUseLCUSeparateModel, m_GOPList );
#endif
  }
  

  m_pppcRDSbacCoder = new TEncSbac** [m_maxTotalCUDepth+1];
#if FAST_BIT_EST
  m_pppcBinCoderCABAC = new TEncBinCABACCounter** [m_maxTotalCUDepth+1];
#else
  m_pppcBinCoderCABAC = new TEncBinCABAC** [m_maxTotalCUDepth+1];
#endif

  for ( Int iDepth = 0; iDepth < m_maxTotalCUDepth+1; iDepth++ )
  {
    m_pppcRDSbacCoder[iDepth] = new TEncSbac* [CI_NUM];
#if FAST_BIT_EST
    m_pppcBinCoderCABAC[iDepth] = new TEncBinCABACCounter* [CI_NUM];
#else
    m_pppcBinCoderCABAC[iDepth] = new TEncBinCABAC* [CI_NUM];
#endif

    for (Int iCIIdx = 0; iCIIdx < CI_NUM; iCIIdx ++ )
    {
      m_pppcRDSbacCoder[iDepth][iCIIdx] = new TEncSbac;
#if FAST_BIT_EST
      m_pppcBinCoderCABAC [iDepth][iCIIdx] = new TEncBinCABACCounter;
#else
      m_pppcBinCoderCABAC [iDepth][iCIIdx] = new TEncBinCABAC;
#endif
      m_pppcRDSbacCoder   [iDepth][iCIIdx]->init( m_pppcBinCoderCABAC [iDepth][iCIIdx] );
    }
  }
}

Void TEncTop::destroy ()
{
  // destroy processing unit classes
  m_cGOPEncoder.        destroy();
  m_cSliceEncoder.      destroy();
  m_cCuEncoder.         destroy();
  m_cEncSAO.            destroyEncData();
  m_cEncSAO.            destroy();
  m_cLoopFilter.        destroy();
  m_cRateCtrl.          destroy();
  m_cSearch.            destroy();
  Int iDepth;
  for ( iDepth = 0; iDepth < m_maxTotalCUDepth+1; iDepth++ )
  {
    for (Int iCIIdx = 0; iCIIdx < CI_NUM; iCIIdx ++ )
    {
      delete m_pppcRDSbacCoder[iDepth][iCIIdx];
      delete m_pppcBinCoderCABAC[iDepth][iCIIdx];
    }
  }

  for ( iDepth = 0; iDepth < m_maxTotalCUDepth+1; iDepth++ )
  {
    delete [] m_pppcRDSbacCoder[iDepth];
    delete [] m_pppcBinCoderCABAC[iDepth];
  }

  delete [] m_pppcRDSbacCoder;
  delete [] m_pppcBinCoderCABAC;

  // destroy ROM
  destroyROM();

  return;
}

Void TEncTop::init(Bool isFieldCoding)
{
  TComSPS &sps0=*(m_spsMap.allocatePS(0)); // NOTE: implementations that use more than 1 SPS need to be aware of activation issues.
  TComPPS &pps0=*(m_ppsMap.allocatePS(0));
  // initialize SPS
  xInitSPS(sps0);
  xInitVPS(m_cVPS, sps0);

  if (m_RCCpbSaturationEnabled)
  {
    m_cRateCtrl.initHrdParam(sps0.getVuiParameters()->getHrdParameters(), m_iFrameRate, m_RCInitialCpbFullness);
  }

  m_cRdCost.setCostMode(m_costMode);

  // initialize PPS
  xInitPPS(pps0, sps0);
  xInitRPS(sps0, isFieldCoding);
  xInitScalingLists(sps0, pps0);

  if (m_wcgChromaQpControl.isEnabled())
  {
    TComPPS &pps1=*(m_ppsMap.allocatePS(1));
    xInitPPS(pps1, sps0);
    xInitScalingLists(sps0, pps1);
  }

  // initialize processing unit classes
  m_cGOPEncoder.  init( this );
  m_cSliceEncoder.init( this );
  m_cCuEncoder.   init( this );
  m_cCuEncoder.setSliceEncoder(&m_cSliceEncoder);

  // initialize transform & quantization class
  m_pcCavlcCoder = getCavlcCoder();

  m_cTrQuant.init( 1 << m_uiQuadtreeTULog2MaxSize,
                   m_useRDOQ,
                   m_useRDOQTS,
                   m_useSelectiveRDOQ,
                   true
                  ,m_useTransformSkipFast
#if ADAPTIVE_QP_SELECTION
                  ,m_bUseAdaptQpSelect
#endif
                  );

  // initialize encoder search class
  m_cSearch.init( this, &m_cTrQuant, m_iSearchRange, m_bipredSearchRange, m_motionEstimationSearchMethod, m_maxCUWidth, m_maxCUHeight, m_maxTotalCUDepth, &m_cEntropyCoder, &m_cRdCost, getRDSbacCoder(), getRDGoOnSbacCoder() );

  m_iMaxRefPicNum = 0;
}

Void TEncTop::xInitScalingLists(TComSPS &sps, TComPPS &pps)
{
  // Initialise scaling lists
  // The encoder will only use the SPS scaling lists. The PPS will never be marked present.
  const Int maxLog2TrDynamicRange[MAX_NUM_CHANNEL_TYPE] =
  {
      sps.getMaxLog2TrDynamicRange(CHANNEL_TYPE_LUMA),
      sps.getMaxLog2TrDynamicRange(CHANNEL_TYPE_CHROMA)
  };
  if(getUseScalingListId() == SCALING_LIST_OFF)
  {
    getTrQuant()->setFlatScalingList(maxLog2TrDynamicRange, sps.getBitDepths());
    getTrQuant()->setUseScalingList(false);
    sps.setScalingListPresentFlag(false);
    pps.setScalingListPresentFlag(false);
  }
  else if(getUseScalingListId() == SCALING_LIST_DEFAULT)
  {
    sps.getScalingList().setDefaultScalingList ();
    sps.setScalingListPresentFlag(false);
    pps.setScalingListPresentFlag(false);

    getTrQuant()->setScalingList(&(sps.getScalingList()), maxLog2TrDynamicRange, sps.getBitDepths());
    getTrQuant()->setUseScalingList(true);
  }
  else if(getUseScalingListId() == SCALING_LIST_FILE_READ)
  {
    sps.getScalingList().setDefaultScalingList ();
    if(sps.getScalingList().xParseScalingList(getScalingListFileName()))
    {
      Bool bParsedScalingList=false; // Use of boolean so that assertion outputs useful string
      assert(bParsedScalingList);
      exit(1);
    }
    sps.getScalingList().checkDcOfMatrix();
    sps.setScalingListPresentFlag(sps.getScalingList().checkDefaultScalingList());
    pps.setScalingListPresentFlag(false);
    getTrQuant()->setScalingList(&(sps.getScalingList()), maxLog2TrDynamicRange, sps.getBitDepths());
    getTrQuant()->setUseScalingList(true);
  }
  else
  {
    printf("error : ScalingList == %d not supported\n",getUseScalingListId());
    assert(0);
  }

  if (getUseScalingListId() != SCALING_LIST_OFF)
  {
    // Prepare delta's:
    for(UInt sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
      const Int predListStep = (sizeId == SCALING_LIST_32x32? (SCALING_LIST_NUM/NUMBER_OF_PREDICTION_MODES) : 1); // if 32x32, skip over chroma entries.

      for(UInt listId = 0; listId < SCALING_LIST_NUM; listId+=predListStep)
      {
        sps.getScalingList().checkPredMode( sizeId, listId );
      }
    }
  }
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

Void TEncTop::deletePicBuffer()
{
  TComList<TComPic*>::iterator iterPic = m_cListPic.begin();
  Int iSize = Int( m_cListPic.size() );

  for ( Int i = 0; i < iSize; i++ )
  {
    TComPic* pcPic = *(iterPic++);

    pcPic->destroy();
    delete pcPic;
    pcPic = NULL;
  }
}

/**
 - Application has picture buffer list with size of GOP + 1
 - Picture buffer list acts like as ring buffer
 - End of the list has the latest picture
 .
 \param   flush               cause encoder to encode a partial GOP
 \param   pcPicYuvOrg         original YUV picture
 \param   pcPicYuvTrueOrg     
 \param   snrCSC
 \retval  rcListPicYuvRecOut  list of reconstruction YUV pictures
 \retval  accessUnitsOut      list of output access units
 \retval  iNumEncoded         number of encoded pictures
 */
#if JVET_X0048_X0103_FILM_GRAIN
Void TEncTop::encode(Bool flush, TComPicYuv* pcPicYuvOrg, TComPicYuv* pcPicYuvTrueOrg, TComPicYuv* pcfilteredOrgPicForFG, const InputColourSpaceConversion ipCSC, const InputColourSpaceConversion snrCSC, TComList<TComPicYuv*>& rcListPicYuvRecOut, std::list<AccessUnit>& accessUnitsOut, Int& iNumEncoded)
#else
Void TEncTop::encode( Bool flush, TComPicYuv* pcPicYuvOrg, TComPicYuv* pcPicYuvTrueOrg, const InputColourSpaceConversion ipCSC, const InputColourSpaceConversion snrCSC, TComList<TComPicYuv*>& rcListPicYuvRecOut, std::list<AccessUnit>& accessUnitsOut, Int& iNumEncoded )
#endif
{
  if (pcPicYuvOrg != NULL)
  {
    // get original YUV
    TComPic* pcPicCurr = NULL;

    Int ppsID=-1; // Use default PPS ID
    if (getWCGChromaQPControl().isEnabled())
    {
      ppsID=getdQPs()[ m_iPOCLast+1 ];
    }
    xGetNewPicBuffer( pcPicCurr, ppsID );
    pcPicYuvOrg->copyToPic( pcPicCurr->getPicYuvOrg() );
    pcPicYuvTrueOrg->copyToPic( pcPicCurr->getPicYuvTrueOrg() );
#if JVET_X0048_X0103_FILM_GRAIN
    if (m_fgcSEIAnalysisEnabled && m_fgcSEIExternalDenoised.empty())
    {
      pcfilteredOrgPicForFG->copyToPic(pcPicCurr->getPicFilteredFG());
    }
#endif

#if SHUTTER_INTERVAL_SEI_PROCESSING
    if ( getShutterFilterFlag() )
    {
      pcPicCurr->xOutputPreFilteredPic(pcPicCurr, &m_cListPic);
      pcPicCurr->getPicYuvOrg()->copyToPic(pcPicYuvOrg);
    }
#endif

    // compute image characteristics
    if ( getUseAdaptiveQP() )
    {
      m_cPreanalyzer.xPreanalyze( dynamic_cast<TEncPic*>( pcPicCurr ) );
    }
  }

  if ((m_iNumPicRcvd == 0) || (!flush && (m_iPOCLast != 0) && (m_iNumPicRcvd != m_iGOPSize) && (m_iGOPSize != 0)))
  {
    iNumEncoded = 0;
    return;
  }

  if ( m_RCEnableRateControl )
  {
    m_cRateCtrl.initRCGOP( m_iNumPicRcvd );
  }

  // compress GOP
  m_cGOPEncoder.compressGOP(m_iPOCLast, m_iNumPicRcvd, m_cListPic, rcListPicYuvRecOut, accessUnitsOut, false, false, ipCSC, snrCSC, getOutputLogControl());

  if ( m_RCEnableRateControl )
  {
    m_cRateCtrl.destroyRCGOP();
  }

  iNumEncoded         = m_iNumPicRcvd;
  m_iNumPicRcvd       = 0;
  m_uiNumAllPicCoded += iNumEncoded;
}

/**------------------------------------------------
 Separate interlaced frame into two fields
 -------------------------------------------------**/
Void separateFields(Pel* org, Pel* dstField, UInt stride, UInt width, UInt height, Bool isTop)
{
  if (!isTop)
  {
    org += stride;
  }
  for (Int y = 0; y < height>>1; y++)
  {
    for (Int x = 0; x < width; x++)
    {
      dstField[x] = org[x];
    }

    dstField += stride;
    org += stride*2;
  }

}

Void TEncTop::encode(Bool flush, TComPicYuv* pcPicYuvOrg, TComPicYuv* pcPicYuvTrueOrg, const InputColourSpaceConversion ipCSC, const InputColourSpaceConversion snrCSC, TComList<TComPicYuv*>& rcListPicYuvRecOut, std::list<AccessUnit>& accessUnitsOut, Int& iNumEncoded, Bool isTff)
{
  iNumEncoded = 0;

  for (Int fieldNum=0; fieldNum<2; fieldNum++)
  {
    if (pcPicYuvOrg)
    {

      /* -- field initialization -- */
      const Bool isTopField=isTff==(fieldNum==0);

      TComPic *pcField;
      xGetNewPicBuffer( pcField, -1 );
      pcField->setReconMark (false);                     // where is this normally?

      if (fieldNum==1)                                   // where is this normally?
      {
        TComPicYuv* rpcPicYuvRec;

        // org. buffer
        if ( rcListPicYuvRecOut.size() >= (UInt)m_iGOPSize+1 ) // need to maintain field 0 in list of RecOuts while processing field 1. Hence +1 on m_iGOPSize.
        {
          rpcPicYuvRec = rcListPicYuvRecOut.popFront();
        }
        else
        {
          rpcPicYuvRec = new TComPicYuv;
          rpcPicYuvRec->create( m_iSourceWidth, m_iSourceHeight, m_chromaFormatIDC, m_maxCUWidth, m_maxCUHeight, m_maxTotalCUDepth, true);
        }
        rcListPicYuvRecOut.pushBack( rpcPicYuvRec );
      }

      pcField->getSlice(0)->setPOC( m_iPOCLast );        // superfluous?
#if !REDUCED_ENCODER_MEMORY
      pcField->getPicYuvRec()->setBorderExtension(false);// where is this normally?
#endif

      pcField->setTopField(isTopField);                  // interlaced requirement

      for (UInt componentIndex = 0; componentIndex < pcPicYuvOrg->getNumberValidComponents(); componentIndex++)
      {
        const ComponentID component = ComponentID(componentIndex);
        const UInt stride = pcPicYuvOrg->getStride(component);

        separateFields((pcPicYuvOrg->getBuf(component) + pcPicYuvOrg->getMarginX(component) + (pcPicYuvOrg->getMarginY(component) * stride)),
                       pcField->getPicYuvOrg()->getAddr(component),
                       pcPicYuvOrg->getStride(component),
                       pcPicYuvOrg->getWidth(component),
                       pcPicYuvOrg->getHeight(component),
                       isTopField);

        separateFields((pcPicYuvTrueOrg->getBuf(component) + pcPicYuvTrueOrg->getMarginX(component) + (pcPicYuvTrueOrg->getMarginY(component) * stride)),
                       pcField->getPicYuvTrueOrg()->getAddr(component),
                       pcPicYuvTrueOrg->getStride(component),
                       pcPicYuvTrueOrg->getWidth(component),
                       pcPicYuvTrueOrg->getHeight(component),
                       isTopField);
      }

      // compute image characteristics
      if ( getUseAdaptiveQP() )
      {
        m_cPreanalyzer.xPreanalyze( dynamic_cast<TEncPic*>( pcField ) );
      }
    }

    if ( m_iNumPicRcvd && ((flush&&fieldNum==1) || (m_iPOCLast/2)==0 || m_iNumPicRcvd==m_iGOPSize ) )
    {
      // compress GOP
      m_cGOPEncoder.compressGOP(m_iPOCLast, m_iNumPicRcvd, m_cListPic, rcListPicYuvRecOut, accessUnitsOut, true, isTff, ipCSC, snrCSC, getOutputLogControl());
      iNumEncoded += m_iNumPicRcvd;
      m_uiNumAllPicCoded += m_iNumPicRcvd;
      m_iNumPicRcvd = 0;
    }
  }
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

/**
 - Application has picture buffer list with size of GOP + 1
 - Picture buffer list acts like as ring buffer
 - End of the list has the latest picture
 .
 \retval rpcPic obtained picture buffer
 */
Void TEncTop::xGetNewPicBuffer ( TComPic*& rpcPic, Int ppsId )
{
  rpcPic=0;

  // At this point, the SPS and PPS can be considered activated - they are copied to the new TComPic.
  const TComPPS *pPPS=(ppsId<0) ? m_ppsMap.getFirstPS() : m_ppsMap.getPS(ppsId);
  assert (pPPS!=0);
  const TComPPS &pps=*pPPS;

  const TComSPS *pSPS=m_spsMap.getPS(pps.getSPSId());
  assert (pSPS!=0);
  const TComSPS &sps=*pSPS;

  TComSlice::sortPicList(m_cListPic);

  // use an entry in the buffered list if the maximum number that need buffering has been reached:
  if (m_cListPic.size() >= (UInt)(m_iGOPSize + getMaxDecPicBuffering(MAX_TLAYER-1) + 2) )
  {
    TComList<TComPic*>::iterator iterPic  = m_cListPic.begin();
    Int iSize = Int( m_cListPic.size() );
    for ( Int i = 0; i < iSize; i++ )
    {
      rpcPic = *iterPic;
      if(rpcPic->getSlice(0)->isReferenced() == false)
      {
        break;
      }
      iterPic++;
    }

    // If PPS ID is the same, we will assume that it has not changed since it was last used
    // and return the old object.
    if (pps.getPPSId() == rpcPic->getPicSym()->getPPS().getPPSId())
    {
#if REDUCED_ENCODER_MEMORY
      rpcPic->releaseAllReconstructionData();
#if JVET_X0048_X0103_FILM_GRAIN
      rpcPic->prepareForEncoderSourcePicYuv( getFilmGrainAnalysisEnabled() );
#else
      rpcPic->prepareForEncoderSourcePicYuv();
#endif
#endif
    }
    else
    {
      // the IDs differ - free up an entry in the list, and then create a new one, as with the case where the max buffering state has not been reached.
      delete rpcPic;
      m_cListPic.erase(iterPic);
      rpcPic=0;
    }
  }

  if (rpcPic==0)
  {
    if ( getUseAdaptiveQP() )
    {
      TEncPic* pcEPic = new TEncPic;
#if REDUCED_ENCODER_MEMORY
      pcEPic->create(sps, pps, pps.getMaxCuDQPDepth() + 1
#if SHUTTER_INTERVAL_SEI_PROCESSING
                    , getShutterFilterFlag()
#endif
#if JVET_X0048_X0103_FILM_GRAIN
                    , getFilmGrainAnalysisEnabled()
#endif
                    );
#else
      pcEPic->create( sps, pps, pps.getMaxCuDQPDepth() + 1, false
#if SHUTTER_INTERVAL_SEI_PROCESSING
                    , getShutterFilterFlag()
#endif
#if JVET_X0048_X0103_FILM_GRAIN
                    , getFilmGrainAnalysisEnabled()
#endif
                    );
#endif
      rpcPic = pcEPic;
    }
    else
    {
      rpcPic = new TComPic;
#if REDUCED_ENCODER_MEMORY
      rpcPic->create( sps, pps, true, false
#if SHUTTER_INTERVAL_SEI_PROCESSING
                    , getShutterFilterFlag()
#endif
#if JVET_X0048_X0103_FILM_GRAIN
                    , getFilmGrainAnalysisEnabled()
#endif
                    );
#else
      rpcPic->create( sps, pps, false
#if SHUTTER_INTERVAL_SEI_PROCESSING
                    , getShutterFilterFlag()
#endif
#if JVET_X0048_X0103_FILM_GRAIN
                    , getFilmGrainAnalysisEnabled()
#endif
                    );
#endif
    }

    m_cListPic.pushBack( rpcPic );
  }
  rpcPic->setReconMark (false);

  m_iPOCLast++;
  m_iNumPicRcvd++;

  rpcPic->getSlice(0)->setPOC( m_iPOCLast );
#if !REDUCED_ENCODER_MEMORY
  // mark it should be extended
  rpcPic->getPicYuvRec()->setBorderExtension(false);
#endif
}

Void TEncTop::xInitVPS(TComVPS &vps, const TComSPS &sps)
{
  // The SPS must have already been set up.
  // set the VPS profile information.
  *vps.getPTL() = *sps.getPTL();
  vps.setMaxOpSets(1);
  vps.getTimingInfo()->setTimingInfoPresentFlag       ( false );
  vps.setNumHrdParameters( 0 );

  vps.createHrdParamBuffer();
  for( UInt i = 0; i < vps.getNumHrdParameters(); i ++ )
  {
    vps.setHrdOpSetIdx( 0, i );
    vps.setCprmsPresentFlag( false, i );
    // Set up HrdParameters here.
  }
}

Void TEncTop::xInitSPS(TComSPS &sps)
{
  ProfileTierLevel& profileTierLevel = *sps.getPTL()->getGeneralPTL();
  profileTierLevel.setLevelIdc(m_level);
  profileTierLevel.setTierFlag(m_levelTier);
  profileTierLevel.setProfileIdc(m_profile);
  profileTierLevel.setProfileCompatibilityFlag(m_profile, 1);
  profileTierLevel.setProgressiveSourceFlag(m_progressiveSourceFlag);
  profileTierLevel.setInterlacedSourceFlag(m_interlacedSourceFlag);
  profileTierLevel.setNonPackedConstraintFlag(m_nonPackedConstraintFlag);
  profileTierLevel.setFrameOnlyConstraintFlag(m_frameOnlyConstraintFlag);
  profileTierLevel.setBitDepthConstraint(m_bitDepthConstraintValue);
  profileTierLevel.setChromaFormatConstraint(m_chromaFormatConstraintValue);
  profileTierLevel.setIntraConstraintFlag(m_intraConstraintFlag);
  profileTierLevel.setOnePictureOnlyConstraintFlag(m_onePictureOnlyConstraintFlag);
  profileTierLevel.setLowerBitRateConstraintFlag(m_lowerBitRateConstraintFlag);

  if ((m_profile == Profile::MAIN10) && (m_bitDepth[CHANNEL_TYPE_LUMA] == 8) && (m_bitDepth[CHANNEL_TYPE_CHROMA] == 8))
  {
    /* The above constraint is equal to Profile::MAIN */
    profileTierLevel.setProfileCompatibilityFlag(Profile::MAIN, 1);
  }
  if (m_profile == Profile::MAIN)
  {
    /* A Profile::MAIN10 decoder can always decode Profile::MAIN */
    profileTierLevel.setProfileCompatibilityFlag(Profile::MAIN10, 1);
  }
  /* XXX: should Main be marked as compatible with still picture? */
  /* XXX: may be a good idea to refactor the above into a function
   * that chooses the actual compatibility based upon options */

  sps.setPicWidthInLumaSamples  ( m_iSourceWidth      );
  sps.setPicHeightInLumaSamples ( m_iSourceHeight     );
  sps.setConformanceWindow      ( m_conformanceWindow );
  sps.setMaxCUWidth             ( m_maxCUWidth        );
  sps.setMaxCUHeight            ( m_maxCUHeight       );
  sps.setMaxTotalCUDepth        ( m_maxTotalCUDepth   );
  sps.setChromaFormatIdc( m_chromaFormatIDC);
  sps.setLog2DiffMaxMinCodingBlockSize(m_log2DiffMaxMinCodingBlockSize);

  Int minCUSize = sps.getMaxCUWidth() >> ( sps.getLog2DiffMaxMinCodingBlockSize() );
  Int log2MinCUSize = 0;
  while(minCUSize > 1)
  {
    minCUSize >>= 1;
    log2MinCUSize++;
  }

  sps.setLog2MinCodingBlockSize(log2MinCUSize);

  sps.setPCMLog2MinSize (m_uiPCMLog2MinSize);
  sps.setUsePCM        ( m_usePCM           );
  sps.setPCMLog2MaxSize( m_pcmLog2MaxSize  );

  sps.setQuadtreeTULog2MaxSize( m_uiQuadtreeTULog2MaxSize );
  sps.setQuadtreeTULog2MinSize( m_uiQuadtreeTULog2MinSize );
  sps.setQuadtreeTUMaxDepthInter( m_uiQuadtreeTUMaxDepthInter    );
  sps.setQuadtreeTUMaxDepthIntra( m_uiQuadtreeTUMaxDepthIntra    );

  sps.setSPSTemporalMVPEnabledFlag((getTMVPModeId() == 2 || getTMVPModeId() == 1));

  sps.setMaxTrSize   ( 1 << m_uiQuadtreeTULog2MaxSize );

  sps.setUseAMP ( m_useAMP );

  for (UInt channelType = 0; channelType < MAX_NUM_CHANNEL_TYPE; channelType++)
  {
    sps.setBitDepth      (ChannelType(channelType), m_bitDepth[channelType] );
#if O0043_BEST_EFFORT_DECODING
    sps.setStreamBitDepth(ChannelType(channelType), m_bitDepth[channelType] );
#endif
    sps.setQpBDOffset  (ChannelType(channelType), (6 * (m_bitDepth[channelType] - 8)));
    sps.setPCMBitDepth (ChannelType(channelType), m_PCMBitDepth[channelType]         );
  }

  sps.setUseSAO( m_bUseSAO );

  sps.setMaxTLayers( m_maxTempLayer );
  sps.setTemporalIdNestingFlag( ( m_maxTempLayer == 1 ) ? true : false );

  for (Int i = 0; i < min(sps.getMaxTLayers(),(UInt) MAX_TLAYER); i++ )
  {
    sps.setMaxDecPicBuffering(m_maxDecPicBuffering[i], i);
    sps.setNumReorderPics(m_numReorderPics[i], i);
  }

  sps.setPCMFilterDisableFlag  ( m_bPCMFilterDisableFlag );
  sps.setScalingListFlag ( (m_useScalingListId == SCALING_LIST_OFF) ? 0 : 1 );
  sps.setUseStrongIntraSmoothing( m_useStrongIntraSmoothing );
  sps.setVuiParametersPresentFlag(getVuiParametersPresentFlag());

  if (sps.getVuiParametersPresentFlag())
  {
    TComVUI* pcVUI = sps.getVuiParameters();
    pcVUI->setAspectRatioInfoPresentFlag(getAspectRatioInfoPresentFlag());
    pcVUI->setAspectRatioIdc(getAspectRatioIdc());
    pcVUI->setSarWidth(getSarWidth());
    pcVUI->setSarHeight(getSarHeight());
    pcVUI->setOverscanInfoPresentFlag(getOverscanInfoPresentFlag());
    pcVUI->setOverscanAppropriateFlag(getOverscanAppropriateFlag());
    pcVUI->setVideoSignalTypePresentFlag(getVideoSignalTypePresentFlag());
    pcVUI->setVideoFormat(getVideoFormat());
    pcVUI->setVideoFullRangeFlag(getVideoFullRangeFlag());
    pcVUI->setColourDescriptionPresentFlag(getColourDescriptionPresentFlag());
    pcVUI->setColourPrimaries(getColourPrimaries());
    pcVUI->setTransferCharacteristics(getTransferCharacteristics());
    pcVUI->setMatrixCoefficients(getMatrixCoefficients());
    pcVUI->setChromaLocInfoPresentFlag(getChromaLocInfoPresentFlag());
    pcVUI->setChromaSampleLocTypeTopField(getChromaSampleLocTypeTopField());
    pcVUI->setChromaSampleLocTypeBottomField(getChromaSampleLocTypeBottomField());
    pcVUI->setNeutralChromaIndicationFlag(getNeutralChromaIndicationFlag());
    pcVUI->setDefaultDisplayWindow(getDefaultDisplayWindow());
    pcVUI->setFrameFieldInfoPresentFlag(getFrameFieldInfoPresentFlag());
    pcVUI->setFieldSeqFlag(false);
    pcVUI->setHrdParametersPresentFlag(false);
    pcVUI->getTimingInfo()->setPocProportionalToTimingFlag(getPocProportionalToTimingFlag());
    pcVUI->getTimingInfo()->setNumTicksPocDiffOneMinus1   (getNumTicksPocDiffOneMinus1()   );
    pcVUI->setBitstreamRestrictionFlag(getBitstreamRestrictionFlag());
    pcVUI->setTilesFixedStructureFlag(getTilesFixedStructureFlag());
    pcVUI->setMotionVectorsOverPicBoundariesFlag(getMotionVectorsOverPicBoundariesFlag());
    pcVUI->setMinSpatialSegmentationIdc(getMinSpatialSegmentationIdc());
    pcVUI->setMaxBytesPerPicDenom(getMaxBytesPerPicDenom());
    pcVUI->setMaxBitsPerMinCuDenom(getMaxBitsPerMinCuDenom());
    pcVUI->setLog2MaxMvLengthHorizontal(getLog2MaxMvLengthHorizontal());
    pcVUI->setLog2MaxMvLengthVertical(getLog2MaxMvLengthVertical());
  }
  sps.setNumLongTermRefPicSPS(NUM_LONG_TERM_REF_PIC_SPS);
  assert (NUM_LONG_TERM_REF_PIC_SPS <= MAX_NUM_LONG_TERM_REF_PICS);
  for (Int k = 0; k < NUM_LONG_TERM_REF_PIC_SPS; k++)
  {
    sps.setLtRefPicPocLsbSps(k, 0);
    sps.setUsedByCurrPicLtSPSFlag(k, 0);
  }

  if( getPictureTimingSEIEnabled() || getDecodingUnitInfoSEIEnabled() || getCpbSaturationEnabled() )
  {
    xInitHrdParameters(sps);
  }
  if( getBufferingPeriodSEIEnabled() || getPictureTimingSEIEnabled() || getDecodingUnitInfoSEIEnabled() )
  {
    sps.getVuiParameters()->setHrdParametersPresentFlag( true );
  }

  // Set up SPS range extension settings
  sps.getSpsRangeExtension().setTransformSkipRotationEnabledFlag(m_transformSkipRotationEnabledFlag);
  sps.getSpsRangeExtension().setTransformSkipContextEnabledFlag(m_transformSkipContextEnabledFlag);
  for (UInt signallingModeIndex = 0; signallingModeIndex < NUMBER_OF_RDPCM_SIGNALLING_MODES; signallingModeIndex++)
  {
    sps.getSpsRangeExtension().setRdpcmEnabledFlag(RDPCMSignallingMode(signallingModeIndex), m_rdpcmEnabledFlag[signallingModeIndex]);
  }
  sps.getSpsRangeExtension().setExtendedPrecisionProcessingFlag(m_extendedPrecisionProcessingFlag);
  sps.getSpsRangeExtension().setIntraSmoothingDisabledFlag( m_intraSmoothingDisabledFlag );
  sps.getSpsRangeExtension().setHighPrecisionOffsetsEnabledFlag(m_highPrecisionOffsetsEnabledFlag);
  sps.getSpsRangeExtension().setPersistentRiceAdaptationEnabledFlag(m_persistentRiceAdaptationEnabledFlag);
  sps.getSpsRangeExtension().setCabacBypassAlignmentEnabledFlag(m_cabacBypassAlignmentEnabledFlag);
}

// calculate scale value of bitrate and initial delay
Int calcScale(Int x)
{
  if (x==0)
  {
    return 0;
  }
  UInt iMask = 0xffffffff;
  Int ScaleValue = 32;

  while ((x&iMask) != 0)
  {
    ScaleValue--;
    iMask = (iMask >> 1);
  }

  return ScaleValue;
}

Void TEncTop::xInitHrdParameters(TComSPS &sps)
{
  Bool useSubCpbParams = (getSliceMode() > 0) || (getSliceSegmentMode() > 0);
  Int  bitRate         = getTargetBitrate();
  Bool isRandomAccess  = getIntraPeriod() > 0;
  Int cpbSize          = getCpbSize();
  assert (cpbSize!=0);  // CPB size may not be equal to zero. ToDo: have a better default and check for level constraints
  if( !getVuiParametersPresentFlag() && !getCpbSaturationEnabled() )
  {
    return;
  }

  TComVUI *vui = sps.getVuiParameters();
  TComHRD *hrd = vui->getHrdParameters();

  TimingInfo *timingInfo = vui->getTimingInfo();
  timingInfo->setTimingInfoPresentFlag( true );
  switch( getFrameRate() )
  {
  case 24:
    timingInfo->setNumUnitsInTick( 1125000 );    timingInfo->setTimeScale    ( 27000000 );
    break;
  case 25:
    timingInfo->setNumUnitsInTick( 1080000 );    timingInfo->setTimeScale    ( 27000000 );
    break;
  case 30:
    timingInfo->setNumUnitsInTick( 900900 );     timingInfo->setTimeScale    ( 27000000 );
    break;
  case 50:
    timingInfo->setNumUnitsInTick( 540000 );     timingInfo->setTimeScale    ( 27000000 );
    break;
  case 60:
    timingInfo->setNumUnitsInTick( 450450 );     timingInfo->setTimeScale    ( 27000000 );
    break;
  default:
    timingInfo->setNumUnitsInTick( 1001 );       timingInfo->setTimeScale    ( 60000 );
    break;
  }

  if (getTemporalSubsampleRatio()>1)
  {
    UInt temporalSubsampleRatio = getTemporalSubsampleRatio();
    if ( Double(timingInfo->getNumUnitsInTick()) * temporalSubsampleRatio > std::numeric_limits<UInt>::max() )
    {
      timingInfo->setTimeScale( timingInfo->getTimeScale() / temporalSubsampleRatio );
    }
    else
    {
      timingInfo->setNumUnitsInTick( timingInfo->getNumUnitsInTick() * temporalSubsampleRatio );
    }
  }

  Bool rateCnt = ( bitRate > 0 );
  hrd->setNalHrdParametersPresentFlag( rateCnt );
  hrd->setVclHrdParametersPresentFlag( rateCnt );
  hrd->setSubPicCpbParamsPresentFlag( useSubCpbParams );

  if( hrd->getSubPicCpbParamsPresentFlag() )
  {
    hrd->setTickDivisorMinus2( 100 - 2 );                          //
    hrd->setDuCpbRemovalDelayLengthMinus1( 7 );                    // 8-bit precision ( plus 1 for last DU in AU )
    hrd->setSubPicCpbParamsInPicTimingSEIFlag( true );
    hrd->setDpbOutputDelayDuLengthMinus1( 5 + 7 );                 // With sub-clock tick factor of 100, at least 7 bits to have the same value as AU dpb delay
  }
  else
  {
    hrd->setSubPicCpbParamsInPicTimingSEIFlag( false );
  }

  if (calcScale(bitRate) <= 6)
  {
    hrd->setBitRateScale(0);
  }
  else
  {
    hrd->setBitRateScale(calcScale(bitRate) - 6);
  }

  if (calcScale(cpbSize) <= 4)
  {
    hrd->setCpbSizeScale(0);
  }
  else
  {
    hrd->setCpbSizeScale(calcScale(cpbSize) - 4);
  }

  hrd->setDuCpbSizeScale( 6 );                                     // in units of 2^( 4 + 6 ) = 1,024 bit

  hrd->setInitialCpbRemovalDelayLengthMinus1(15);                  // assuming 0.5 sec, log2( 90,000 * 0.5 ) = 16-bit
  if( isRandomAccess )
  {
    hrd->setCpbRemovalDelayLengthMinus1(5);                        // 32 = 2^5 (plus 1)
    hrd->setDpbOutputDelayLengthMinus1 (5);                        // 32 + 3 = 2^6
  }
  else
  {
    hrd->setCpbRemovalDelayLengthMinus1(9);                        // max. 2^10
    hrd->setDpbOutputDelayLengthMinus1 (9);                        // max. 2^10
  }

  // Note: parameters for all temporal layers are initialized with the same values
  Int i, j;
  UInt bitrateValue, cpbSizeValue;
  UInt duCpbSizeValue;
  UInt duBitRateValue = 0;

  for( i = 0; i < MAX_TLAYER; i ++ )
  {
    hrd->setFixedPicRateFlag( i, 1 );
    hrd->setPicDurationInTcMinus1( i, 0 );
    hrd->setLowDelayHrdFlag( i, 0 );
    hrd->setCpbCntMinus1( i, 0 );

    //! \todo check for possible PTL violations
    // BitRate[ i ] = ( bit_rate_value_minus1[ i ] + 1 ) * 2^( 6 + bit_rate_scale )
    bitrateValue = bitRate / (1 << (6 + hrd->getBitRateScale()) );      // bitRate is in bits, so it needs to be scaled down
    // CpbSize[ i ] = ( cpb_size_value_minus1[ i ] + 1 ) * 2^( 4 + cpb_size_scale )
    cpbSizeValue = cpbSize / (1 << (4 + hrd->getCpbSizeScale()) );      // using bitRate results in 1 second CPB size

    // DU CPB size could be smaller (i.e. bitrateValue / number of DUs), but we don't know 
    // in how many DUs the slice segment settings will result 
    duCpbSizeValue = bitrateValue;
    duBitRateValue = cpbSizeValue;

    for( j = 0; j < ( hrd->getCpbCntMinus1( i ) + 1 ); j ++ )
    {
      hrd->setBitRateValueMinus1( i, j, 0, ( bitrateValue - 1 ) );
      hrd->setCpbSizeValueMinus1( i, j, 0, ( cpbSizeValue - 1 ) );
      hrd->setDuCpbSizeValueMinus1( i, j, 0, ( duCpbSizeValue - 1 ) );
      hrd->setDuBitRateValueMinus1( i, j, 0, ( duBitRateValue - 1 ) );
      hrd->setCbrFlag( i, j, 0, false );

      hrd->setBitRateValueMinus1( i, j, 1, ( bitrateValue - 1) );
      hrd->setCpbSizeValueMinus1( i, j, 1, ( cpbSizeValue - 1 ) );
      hrd->setDuCpbSizeValueMinus1( i, j, 1, ( duCpbSizeValue - 1 ) );
      hrd->setDuBitRateValueMinus1( i, j, 1, ( duBitRateValue - 1 ) );
      hrd->setCbrFlag( i, j, 1, false );
    }
  }
}

Void TEncTop::xInitPPS(TComPPS &pps, const TComSPS &sps)
{
  // pps ID already initialised.
  pps.setSPSId(sps.getSPSId());

  pps.setConstrainedIntraPred( m_bUseConstrainedIntraPred );
  Bool bUseDQP = (getMaxCuDQPDepth() > 0)? true : false;

  if((getMaxDeltaQP() != 0 )|| getUseAdaptiveQP())
  {
    bUseDQP = true;
  }

  if ( getLumaLevelToDeltaQPMapping().isEnabled() )
  {
    bUseDQP = true;
  }

#if JVET_V0078
  if (getSmoothQPReductionEnable())
  {
    bUseDQP = true;
  }
#endif
#if JVET_Y0077_BIM
  if (m_bimEnabled)
  {
    bUseDQP = true;
  }
#endif

  if (m_costMode==COST_SEQUENCE_LEVEL_LOSSLESS || m_costMode==COST_LOSSLESS_CODING)
  {
    bUseDQP=false;
  }


  if ( m_RCEnableRateControl )
  {
    pps.setUseDQP(true);
    pps.setMaxCuDQPDepth( 0 );
  }
  else if(bUseDQP)
  {
    pps.setUseDQP(true);
    pps.setMaxCuDQPDepth( m_iMaxCuDQPDepth );
  }
  else
  {
    pps.setUseDQP(false);
    pps.setMaxCuDQPDepth( 0 );
  }

  if ( m_diffCuChromaQpOffsetDepth >= 0 )
  {
    pps.getPpsRangeExtension().setDiffCuChromaQpOffsetDepth(m_diffCuChromaQpOffsetDepth);
    pps.getPpsRangeExtension().clearChromaQpOffsetList();
    pps.getPpsRangeExtension().setChromaQpOffsetListEntry(1, 6, 6);
    /* todo, insert table entries from command line (NB, 0 should not be touched) */
  }
  else
  {
    pps.getPpsRangeExtension().setDiffCuChromaQpOffsetDepth(0);
    pps.getPpsRangeExtension().clearChromaQpOffsetList();
  }
  pps.getPpsRangeExtension().setCrossComponentPredictionEnabledFlag(m_crossComponentPredictionEnabledFlag);
  pps.getPpsRangeExtension().setLog2SaoOffsetScale(CHANNEL_TYPE_LUMA,   m_log2SaoOffsetScale[CHANNEL_TYPE_LUMA  ]);
  pps.getPpsRangeExtension().setLog2SaoOffsetScale(CHANNEL_TYPE_CHROMA, m_log2SaoOffsetScale[CHANNEL_TYPE_CHROMA]);

  if (getWCGChromaQPControl().isEnabled())
  {
    const Int baseQp=m_iQP+pps.getPPSId();
    const Double chromaQp = m_wcgChromaQpControl.chromaQpScale * baseQp + m_wcgChromaQpControl.chromaQpOffset;
    const Double dcbQP = m_wcgChromaQpControl.chromaCbQpScale * chromaQp;
    const Double dcrQP = m_wcgChromaQpControl.chromaCrQpScale * chromaQp;
    const Int cbQP =(Int)(dcbQP + ( dcbQP < 0 ? -0.5 : 0.5) );
    const Int crQP =(Int)(dcrQP + ( dcrQP < 0 ? -0.5 : 0.5) );
    pps.setQpOffset(COMPONENT_Cb, Clip3( -12, 12, min(0, cbQP) + m_chromaCbQpOffset ));
    pps.setQpOffset(COMPONENT_Cr, Clip3( -12, 12, min(0, crQP) + m_chromaCrQpOffset));
  }
  else
  {
    pps.setQpOffset(COMPONENT_Cb, m_chromaCbQpOffset );
    pps.setQpOffset(COMPONENT_Cr, m_chromaCrQpOffset );
  }
  Bool bChromaDeltaQPEnabled = false;
  {
    bChromaDeltaQPEnabled = ( m_sliceChromaQpOffsetIntraOrPeriodic[0] || m_sliceChromaQpOffsetIntraOrPeriodic[1] );
    if( !bChromaDeltaQPEnabled )
    {
      for( Int i=0; i<m_iGOPSize; i++ )
      {
        if( m_GOPList[i].m_CbQPoffset || m_GOPList[i].m_CrQPoffset )
        {
          bChromaDeltaQPEnabled = true;
          break;
        }
      }
    }
  }
  pps.setSliceChromaQpFlag(bChromaDeltaQPEnabled);

  pps.setEntropyCodingSyncEnabledFlag( m_entropyCodingSyncEnabledFlag );
  pps.setTilesEnabledFlag( (m_iNumColumnsMinus1 > 0 || m_iNumRowsMinus1 > 0) );
  pps.setUseWP( m_useWeightedPred );
  pps.setWPBiPred( m_useWeightedBiPred );
  pps.setOutputFlagPresentFlag( false );
  pps.setSignDataHidingEnabledFlag(getSignDataHidingEnabledFlag());

  if ( getDeblockingFilterMetric() )
  {
    pps.setDeblockingFilterOverrideEnabledFlag(true);
    pps.setPPSDeblockingFilterDisabledFlag(false);
  }
  else
  {
    pps.setDeblockingFilterOverrideEnabledFlag( !getLoopFilterOffsetInPPS() );
    pps.setPPSDeblockingFilterDisabledFlag( getLoopFilterDisable() );
  }

  if (! pps.getPPSDeblockingFilterDisabledFlag())
  {
    pps.setDeblockingFilterBetaOffsetDiv2( getLoopFilterBetaOffset() );
    pps.setDeblockingFilterTcOffsetDiv2( getLoopFilterTcOffset() );
  }
  else
  {
    pps.setDeblockingFilterBetaOffsetDiv2(0);
    pps.setDeblockingFilterTcOffsetDiv2(0);
  }

  // deblockingFilterControlPresentFlag is true if any of the settings differ from the inferred values:
  const Bool deblockingFilterControlPresentFlag = pps.getDeblockingFilterOverrideEnabledFlag() ||
                                                  pps.getPPSDeblockingFilterDisabledFlag()     ||
                                                  pps.getDeblockingFilterBetaOffsetDiv2() != 0 ||
                                                  pps.getDeblockingFilterTcOffsetDiv2() != 0;

  pps.setDeblockingFilterControlPresentFlag(deblockingFilterControlPresentFlag);

  pps.setLog2ParallelMergeLevelMinus2   (m_log2ParallelMergeLevelMinus2 );
  pps.setCabacInitPresentFlag(CABAC_INIT_PRESENT_FLAG);
  pps.setLoopFilterAcrossSlicesEnabledFlag( m_bLFCrossSliceBoundaryFlag );


  Int histogram[MAX_NUM_REF + 1];
  for( Int i = 0; i <= MAX_NUM_REF; i++ )
  {
    histogram[i]=0;
  }
  for( Int i = 0; i < getGOPSize(); i++)
  {
    assert(getGOPEntry(i).m_numRefPicsActive >= 0 && getGOPEntry(i).m_numRefPicsActive <= MAX_NUM_REF);
    histogram[getGOPEntry(i).m_numRefPicsActive]++;
  }

  Int maxHist=-1;
  Int bestPos=0;
  for( Int i = 0; i <= MAX_NUM_REF; i++ )
  {
    if(histogram[i]>maxHist)
    {
      maxHist=histogram[i];
      bestPos=i;
    }
  }
  assert(bestPos <= 15);
  pps.setNumRefIdxL0DefaultActive(bestPos);
  pps.setNumRefIdxL1DefaultActive(bestPos);
  pps.setTransquantBypassEnabledFlag(getTransquantBypassEnabledFlag());
  pps.setUseTransformSkip( m_useTransformSkip );
  pps.getPpsRangeExtension().setLog2MaxTransformSkipBlockSize( m_log2MaxTransformSkipBlockSize  );

  if (m_sliceSegmentMode != NO_SLICES)
  {
    pps.setDependentSliceSegmentsEnabledFlag( true );
  }

  xInitPPSforTiles(pps);
}

//Function for initializing m_RPSList, a list of TComReferencePictureSet, based on the GOPEntry objects read from the config file.
Void TEncTop::xInitRPS(TComSPS &sps, Bool isFieldCoding)
{
  TComReferencePictureSet*      rps;

  sps.createRPSList(getGOPSize() + m_extraRPSs + 1);
  TComRPSList* rpsList = sps.getRPSList();

  for( Int i = 0; i < getGOPSize()+m_extraRPSs; i++)
  {
    const GOPEntry &ge = getGOPEntry(i);
    rps = rpsList->getReferencePictureSet(i);
    rps->setNumberOfPictures(ge.m_numRefPics);
    rps->setNumRefIdc(ge.m_numRefIdc);
    Int numNeg = 0;
    Int numPos = 0;
    for( Int j = 0; j < ge.m_numRefPics; j++)
    {
      rps->setDeltaPOC(j,ge.m_referencePics[j]);
      rps->setUsed(j,ge.m_usedByCurrPic[j]);
      if(ge.m_referencePics[j]>0)
      {
        numPos++;
      }
      else
      {
        numNeg++;
      }
    }
    rps->setNumberOfNegativePictures(numNeg);
    rps->setNumberOfPositivePictures(numPos);

    // handle inter RPS intialization from the config file.
    rps->setInterRPSPrediction(ge.m_interRPSPrediction > 0);  // not very clean, converting anything > 0 to true.
    rps->setDeltaRIdxMinus1(0);                               // index to the Reference RPS is always the previous one.
    TComReferencePictureSet*     RPSRef = i>0 ? rpsList->getReferencePictureSet(i-1): NULL;  // get the reference RPS

    if (ge.m_interRPSPrediction == 2)  // Automatic generation of the inter RPS idc based on the RIdx provided.
    {
      assert (RPSRef!=NULL);
      Int deltaRPS = getGOPEntry(i-1).m_POC - ge.m_POC;  // the ref POC - current POC
      Int numRefDeltaPOC = RPSRef->getNumberOfPictures();

      rps->setDeltaRPS(deltaRPS);           // set delta RPS
      rps->setNumRefIdc(numRefDeltaPOC+1);  // set the numRefIdc to the number of pictures in the reference RPS + 1.
      Int count=0;
      for (Int j = 0; j <= numRefDeltaPOC; j++ ) // cycle through pics in reference RPS.
      {
        Int RefDeltaPOC = (j<numRefDeltaPOC)? RPSRef->getDeltaPOC(j): 0;  // if it is the last decoded picture, set RefDeltaPOC = 0
        rps->setRefIdc(j, 0);
        for (Int k = 0; k < rps->getNumberOfPictures(); k++ )  // cycle through pics in current RPS.
        {
          if (rps->getDeltaPOC(k) == ( RefDeltaPOC + deltaRPS))  // if the current RPS has a same picture as the reference RPS.
          {
              rps->setRefIdc(j, (rps->getUsed(k)?1:2));
              count++;
              break;
          }
        }
      }
      if (count != rps->getNumberOfPictures())
      {
        printf("Warning: Unable fully predict all delta POCs using the reference RPS index given in the config file.  Setting Inter RPS to false for this RPS.\n");
        rps->setInterRPSPrediction(0);
      }
    }
    else if (ge.m_interRPSPrediction == 1)  // inter RPS idc based on the RefIdc values provided in config file.
    {
      assert (RPSRef!=NULL);
      rps->setDeltaRPS(ge.m_deltaRPS);
      rps->setNumRefIdc(ge.m_numRefIdc);
      for (Int j = 0; j < ge.m_numRefIdc; j++ )
      {
        rps->setRefIdc(j, ge.m_refIdc[j]);
      }
      // the following code overwrite the deltaPOC and Used by current values read from the config file with the ones
      // computed from the RefIdc.  A warning is printed if they are not identical.
      numNeg = 0;
      numPos = 0;
      TComReferencePictureSet      RPSTemp;  // temporary variable

      for (Int j = 0; j < ge.m_numRefIdc; j++ )
      {
        if (ge.m_refIdc[j])
        {
          Int deltaPOC = ge.m_deltaRPS + ((j < RPSRef->getNumberOfPictures())? RPSRef->getDeltaPOC(j) : 0);
          RPSTemp.setDeltaPOC((numNeg+numPos),deltaPOC);
          RPSTemp.setUsed((numNeg+numPos),ge.m_refIdc[j]==1?1:0);
          if (deltaPOC<0)
          {
            numNeg++;
          }
          else
          {
            numPos++;
          }
        }
      }
      if (numNeg != rps->getNumberOfNegativePictures())
      {
        printf("Warning: number of negative pictures in RPS is different between intra and inter RPS specified in the config file.\n");
        rps->setNumberOfNegativePictures(numNeg);
        rps->setNumberOfPictures(numNeg+numPos);
      }
      if (numPos != rps->getNumberOfPositivePictures())
      {
        printf("Warning: number of positive pictures in RPS is different between intra and inter RPS specified in the config file.\n");
        rps->setNumberOfPositivePictures(numPos);
        rps->setNumberOfPictures(numNeg+numPos);
      }
      RPSTemp.setNumberOfPictures(numNeg+numPos);
      RPSTemp.setNumberOfNegativePictures(numNeg);
      RPSTemp.sortDeltaPOC();     // sort the created delta POC before comparing
      // check if Delta POC and Used are the same
      // print warning if they are not.
      for (Int j = 0; j < ge.m_numRefIdc; j++ )
      {
        if (RPSTemp.getDeltaPOC(j) != rps->getDeltaPOC(j))
        {
          printf("Warning: delta POC is different between intra RPS and inter RPS specified in the config file.\n");
          rps->setDeltaPOC(j,RPSTemp.getDeltaPOC(j));
        }
        if (RPSTemp.getUsed(j) != rps->getUsed(j))
        {
          printf("Warning: Used by Current in RPS is different between intra and inter RPS specified in the config file.\n");
          rps->setUsed(j,RPSTemp.getUsed(j));
        }
      }
    }
  }
  //In case of field coding, we need to set special parameters for the first bottom field of the sequence, since it is not specified in the cfg file.
  //The position = GOPSize + extraRPSs which is (a priori) unused is reserved for this field in the RPS.
  if (isFieldCoding)
  {
    rps = rpsList->getReferencePictureSet(getGOPSize()+m_extraRPSs);
    rps->setNumberOfPictures(1);
    rps->setNumberOfNegativePictures(1);
    rps->setNumberOfPositivePictures(0);
    rps->setNumberOfLongtermPictures(0);
    rps->setDeltaPOC(0,-1);
    rps->setPOC(0,0);
    rps->setUsed(0,true);
    rps->setInterRPSPrediction(false);
    rps->setDeltaRIdxMinus1(0);
    rps->setDeltaRPS(0);
    rps->setNumRefIdc(0);
  }
}

   // This is a function that
   // determines what Reference Picture Set to use
   // for a specific slice (with POC = POCCurr)
Void TEncTop::selectReferencePictureSet(TComSlice* slice, Int POCCurr, Int GOPid )
{
  slice->setRPSidx(GOPid);

  for(Int extraNum=m_iGOPSize; extraNum<m_extraRPSs+m_iGOPSize; extraNum++)
  {
    if(m_uiIntraPeriod > 0 && getDecodingRefreshType() > 0)
    {
      Int POCIndex = POCCurr%m_uiIntraPeriod;
      if(POCIndex == 0)
      {
        POCIndex = m_uiIntraPeriod;
      }
      if(POCIndex == m_GOPList[extraNum].m_POC)
      {
        slice->setRPSidx(extraNum);
      }
    }
    else
    {
      if(POCCurr==m_GOPList[extraNum].m_POC)
      {
        slice->setRPSidx(extraNum);
      }
    }
  }

  if(POCCurr == 1 && slice->getPic()->isField())
  {
    slice->setRPSidx(m_iGOPSize+m_extraRPSs);
  }

  const TComReferencePictureSet *rps = (slice->getSPS()->getRPSList()->getReferencePictureSet(slice->getRPSidx()));
  slice->setRPS(rps);
}

Int TEncTop::getReferencePictureSetIdxForSOP(Int POCCurr, Int GOPid )
{
  Int rpsIdx = GOPid;

  for(Int extraNum=m_iGOPSize; extraNum<m_extraRPSs+m_iGOPSize; extraNum++)
  {
    if(m_uiIntraPeriod > 0 && getDecodingRefreshType() > 0)
    {
      Int POCIndex = POCCurr%m_uiIntraPeriod;
      if(POCIndex == 0)
      {
        POCIndex = m_uiIntraPeriod;
      }
      if(POCIndex == m_GOPList[extraNum].m_POC)
      {
        rpsIdx = extraNum;
      }
    }
    else
    {
      if(POCCurr==m_GOPList[extraNum].m_POC)
      {
        rpsIdx = extraNum;
      }
    }
  }

  return rpsIdx;
}

Void  TEncTop::xInitPPSforTiles(TComPPS &pps)
{
  pps.setTileUniformSpacingFlag( m_tileUniformSpacingFlag );
  pps.setNumTileColumnsMinus1( m_iNumColumnsMinus1 );
  pps.setNumTileRowsMinus1( m_iNumRowsMinus1 );
  if( !m_tileUniformSpacingFlag )
  {
    pps.setTileColumnWidth( m_tileColumnWidth );
    pps.setTileRowHeight( m_tileRowHeight );
  }
  pps.setLoopFilterAcrossTilesEnabledFlag( m_loopFilterAcrossTilesEnabledFlag );

  // # substreams is "per tile" when tiles are independent.
}

Void  TEncCfg::xCheckGSParameters()
{
  Int   iWidthInCU = ( m_iSourceWidth%m_maxCUWidth ) ? m_iSourceWidth/m_maxCUWidth + 1 : m_iSourceWidth/m_maxCUWidth;
  Int   iHeightInCU = ( m_iSourceHeight%m_maxCUHeight ) ? m_iSourceHeight/m_maxCUHeight + 1 : m_iSourceHeight/m_maxCUHeight;
  UInt  uiCummulativeColumnWidth = 0;
  UInt  uiCummulativeRowHeight = 0;

  //check the column relative parameters
  if( m_iNumColumnsMinus1 >= (1<<(LOG2_MAX_NUM_COLUMNS_MINUS1+1)) )
  {
    printf( "The number of columns is larger than the maximum allowed number of columns.\n" );
    exit( EXIT_FAILURE );
  }

  if( m_iNumColumnsMinus1 >= iWidthInCU )
  {
    printf( "The current picture can not have so many columns.\n" );
    exit( EXIT_FAILURE );
  }

  if( m_iNumColumnsMinus1 && !m_tileUniformSpacingFlag )
  {
    for(Int i=0; i<m_iNumColumnsMinus1; i++)
    {
      uiCummulativeColumnWidth += m_tileColumnWidth[i];
    }

    if( uiCummulativeColumnWidth >= iWidthInCU )
    {
      printf( "The width of the column is too large.\n" );
      exit( EXIT_FAILURE );
    }
  }

  //check the row relative parameters
  if( m_iNumRowsMinus1 >= (1<<(LOG2_MAX_NUM_ROWS_MINUS1+1)) )
  {
    printf( "The number of rows is larger than the maximum allowed number of rows.\n" );
    exit( EXIT_FAILURE );
  }

  if( m_iNumRowsMinus1 >= iHeightInCU )
  {
    printf( "The current picture can not have so many rows.\n" );
    exit( EXIT_FAILURE );
  }

  if( m_iNumRowsMinus1 && !m_tileUniformSpacingFlag )
  {
    for(Int i=0; i<m_iNumRowsMinus1; i++)
    {
      uiCummulativeRowHeight += m_tileRowHeight[i];
    }

    if( uiCummulativeRowHeight >= iHeightInCU )
    {
      printf( "The height of the row is too large.\n" );
      exit( EXIT_FAILURE );
    }
  }
}

Void TEncTop::setParamSetChanged(Int spsId, Int ppsId)
{
  m_ppsMap.setChangedFlag(ppsId);
  m_spsMap.setChangedFlag(spsId);
}

Bool TEncTop::PPSNeedsWriting(Int ppsId)
{
  Bool bChanged=m_ppsMap.getChangedFlag(ppsId);
  m_ppsMap.clearChangedFlag(ppsId);
  return bChanged;
}

Bool TEncTop::SPSNeedsWriting(Int spsId)
{
  Bool bChanged=m_spsMap.getChangedFlag(spsId);
  m_spsMap.clearChangedFlag(spsId);
  return bChanged;
}

Int TEncCfg::getQPForPicture(const UInt gopIndex, const TComSlice *pSlice) const
{
  const Int lumaQpBDOffset = pSlice->getSPS()->getQpBDOffset(CHANNEL_TYPE_LUMA);
  Int qp;

  if (getCostMode()==COST_LOSSLESS_CODING)
  {
    qp=LOSSLESS_AND_MIXED_LOSSLESS_RD_COST_TEST_QP;
  }
  else
  {
    const SliceType sliceType=pSlice->getSliceType();

    qp = getBaseQP();

    // modify QP if a fractional QP was originally specified, cause dQPs to be 0 or 1.
    const Int* pdQPs = getdQPs();
    if ( pdQPs )
    {
      qp += pdQPs[ pSlice->getPOC() ];
    }

    if(sliceType==I_SLICE)
    {
      qp += getIntraQPOffset();
    }
    else
    {
      // Only adjust QP when not lossless
#if JVET_Y0077_BIM
      if (!((getMaxDeltaQP() == 0) && (!getLumaLevelToDeltaQPMapping().isEnabled()) && (!getSmoothQPReductionEnable()) && (!getBIM()) && (qp == -lumaQpBDOffset) && (pSlice->getPPS()->getTransquantBypassEnabledFlag())))
#else
#if JVET_V0078
      if (!((getMaxDeltaQP() == 0) && (!getLumaLevelToDeltaQPMapping().isEnabled()) && (!getSmoothQPReductionEnable()) && (qp == -lumaQpBDOffset) && (pSlice->getPPS()->getTransquantBypassEnabledFlag())))
#else
      if (!(( getMaxDeltaQP() == 0 ) && (!getLumaLevelToDeltaQPMapping().isEnabled()) && (qp == -lumaQpBDOffset ) && (pSlice->getPPS()->getTransquantBypassEnabledFlag())))
#endif
#endif
      {
        const GOPEntry &gopEntry=getGOPEntry(gopIndex);
        // adjust QP according to the QP offset for the GOP entry.
        qp +=gopEntry.m_QPOffset;

        // adjust QP according to QPOffsetModel for the GOP entry.
        Double dqpOffset=qp*gopEntry.m_QPOffsetModelScale+gopEntry.m_QPOffsetModelOffset+0.5;
        Int qpOffset = (Int)floor(Clip3<Double>(0.0, 3.0, dqpOffset));
        qp += qpOffset ;
      }
    }

  }
  qp = Clip3( -lumaQpBDOffset, MAX_QP, qp );
  return qp;
}

//! \}
