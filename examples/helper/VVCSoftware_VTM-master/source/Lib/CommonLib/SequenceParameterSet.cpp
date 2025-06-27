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

#include "CommonDef.h"
#include "SequenceParameterSet.h"

ReferencePictureList::ReferencePictureList(const bool interLayerPicPresentFlag)
  : m_numberOfShorttermPictures(0)
  , m_numberOfLongtermPictures(0)
  , m_numberOfActivePictures(MAX_INT)
  , m_ltrpInSliceHeaderFlag(0)
  , m_interLayerPresentFlag(interLayerPicPresentFlag)
  , m_numberOfInterLayerPictures(0)
{
  ::memset(m_isLongtermRefPic, 0, sizeof(m_isLongtermRefPic));
  ::memset(m_refPicIdentifier, 0, sizeof(m_refPicIdentifier));
  ::memset(m_POC, 0, sizeof(m_POC));
  ::memset( m_isInterLayerRefPic, 0, sizeof( m_isInterLayerRefPic ) );
  ::memset( m_interLayerRefPicIdx, 0, sizeof( m_interLayerRefPicIdx ) );

  ::memset(m_deltaPOCMSBCycleLT, 0, sizeof(m_deltaPOCMSBCycleLT));
  ::memset(m_deltaPocMSBPresentFlag, 0, sizeof(m_deltaPocMSBPresentFlag));
}

ReferencePictureList::~ReferencePictureList()
{
}

void ReferencePictureList::setRefPicIdentifier( int idx, int identifier, bool isLongterm, bool isInterLayerRefPic, int interLayerIdx )
{
  m_refPicIdentifier[idx] = identifier;
  m_isLongtermRefPic[idx] = isLongterm;

  m_deltaPocMSBPresentFlag[idx] = false;
  m_deltaPOCMSBCycleLT[idx] = 0;

  m_isInterLayerRefPic[idx] = isInterLayerRefPic;
  m_interLayerRefPicIdx[idx] = interLayerIdx;
}

int ReferencePictureList::getRefPicIdentifier(int idx) const
{
  return m_refPicIdentifier[idx];
}


bool ReferencePictureList::isRefPicLongterm(int idx) const
{
  return m_isLongtermRefPic[idx];
}

void ReferencePictureList::setPOC(int idx, int POC)
{
  m_POC[idx] = POC;
}

int ReferencePictureList::getPOC(int idx) const
{
  return m_POC[idx];
}

void ReferencePictureList::setNumberOfActivePictures(int numberActive)
{
  m_numberOfActivePictures = numberActive;
}

int ReferencePictureList::getNumberOfActivePictures() const
{
  return m_numberOfActivePictures;
}

void ReferencePictureList::printRefPicInfo() const
{
  //DTRACE(g_trace_ctx, D_RPSINFO, "RefPics = { ");
  printf("RefPics = { ");
  int numRefPic = getNumRefEntries();
  for (int ii = 0; ii < numRefPic; ii++)
  {
    //DTRACE(g_trace_ctx, D_RPSINFO, "%d%s ", m_refPicIdentifier[ii], (m_isLongtermRefPic[ii] == 1) ? "[LT]" : "[ST]");
    printf("%d%s ", m_refPicIdentifier[ii], (m_isLongtermRefPic[ii] == 1) ? "[LT]" : "[ST]");
  }
  //DTRACE(g_trace_ctx, D_RPSINFO, "}\n");
  printf("}\n");
}


SPSRExt::SPSRExt()
 : m_transformSkipRotationEnabledFlag   (false)
 , m_transformSkipContextEnabledFlag    (false)
 , m_extendedPrecisionProcessingFlag    (false)
 , m_tsrcRicePresentFlag                (false)
 , m_highPrecisionOffsetsEnabledFlag    (false)
 , m_rrcRiceExtensionEnableFlag         (false)
 , m_persistentRiceAdaptationEnabledFlag(false)
 , m_reverseLastSigCoeffEnabledFlag     (false)
 , m_cabacBypassAlignmentEnabledFlag    (false)
{
}

void ChromaQpMappingTable::setParams(const ChromaQpMappingTableParams &params, const int qpBdOffset)
{
  m_qpBdOffset = qpBdOffset;
  m_sameCQPTableForAllChromaFlag = params.m_sameCQPTableForAllChromaFlag;
  m_numQpTables = params.m_numQpTables;

  for (int i = 0; i < MAX_NUM_CQP_MAPPING_TABLES; i++)
  {
    m_numPtsInCQPTableMinus1[i] = params.m_numPtsInCQPTableMinus1[i];
    m_deltaQpInValMinus1[i] = params.m_deltaQpInValMinus1[i];
    m_qpTableStartMinus26[i] = params.m_qpTableStartMinus26[i];
    m_deltaQpOutVal[i] = params.m_deltaQpOutVal[i];
  }
}
void ChromaQpMappingTable::deriveChromaQPMappingTables()
{
  for (int i = 0; i < getNumQpTables(); i++)
  {
    const int qpBdOffsetC = m_qpBdOffset;
    const int numPtsInCQPTableMinus1 = getNumPtsInCQPTableMinus1(i);
    std::vector<int> qpInVal(numPtsInCQPTableMinus1 + 2), qpOutVal(numPtsInCQPTableMinus1 + 2);

    qpInVal[0]  = getQpTableStartMinus26(i) + QP26;
    qpOutVal[0] = qpInVal[0];
    for (int j = 0; j <= getNumPtsInCQPTableMinus1(i); j++)
    {
      qpInVal[j + 1] = qpInVal[j] + getDeltaQpInValMinus1(i, j) + 1;
      qpOutVal[j + 1] = qpOutVal[j] + getDeltaQpOutVal(i, j);
    }

    for (int j = 0; j <= getNumPtsInCQPTableMinus1(i); j++)
    {
      CHECK(qpInVal[j]  < -qpBdOffsetC || qpInVal[j]  > MAX_QP, "qpInVal out of range");
      CHECK(qpOutVal[j] < -qpBdOffsetC || qpOutVal[j] > MAX_QP, "qpOutVal out of range");
    }

    m_chromaQpMappingTables[i][qpInVal[0]] = qpOutVal[0];
    for (int k = qpInVal[0] - 1; k >= -qpBdOffsetC; k--)
    {
      m_chromaQpMappingTables[i][k] = Clip3(-qpBdOffsetC, MAX_QP, m_chromaQpMappingTables[i][k + 1] - 1);
    }
    for (int j = 0; j <= numPtsInCQPTableMinus1; j++)
    {
      int sh = (getDeltaQpInValMinus1(i, j) + 1) >> 1;
      for (int k = qpInVal[j] + 1, m = 1; k <= qpInVal[j + 1]; k++, m++)
      {
        m_chromaQpMappingTables[i][k] = m_chromaQpMappingTables[i][qpInVal[j]]
          + ((qpOutVal[j + 1] - qpOutVal[j]) * m + sh) / (getDeltaQpInValMinus1(i, j) + 1);
      }
    }
    for (int k = qpInVal[numPtsInCQPTableMinus1 + 1] + 1; k <= MAX_QP; k++)
    {
      m_chromaQpMappingTables[i][k] = Clip3(-qpBdOffsetC, MAX_QP, m_chromaQpMappingTables[i][k - 1] + 1);
    }
  }
}

SPS::SPS()
  : m_spsId(0)
  , m_vpsId(0)
  , m_layerId(0)
  , m_affineAmvrEnabledFlag(false)
  , m_DMVR(false)
  , m_MMVD(false)
  , m_SBT(false)
  , m_ISP(false)
  , m_maxSubLayers(1)
  , m_ptlDpbHrdParamsPresentFlag(1)
  , m_subLayerDpbParamsFlag(0)
  // Structure
  , m_maxWidthInLumaSamples(352)
  , m_maxHeightInLumaSamples(288)
  , m_subPicInfoPresentFlag(false)
  , m_numSubPics(1)
  , m_independentSubPicsFlag(false)
  , m_subPicSameSizeFlag(false)
  , m_subPicIdMappingExplicitlySignalledFlag(false)
  , m_subPicIdMappingPresentFlag(false)
  , m_subPicIdLen(16)
  , m_log2MinCodingBlockSize(2)
  , m_CTUSize(0)
  , m_minQT{ 0, 0, 0 }
  , m_maxMTTHierarchyDepth{ MAX_BT_DEPTH, MAX_BT_DEPTH_INTER, MAX_BT_DEPTH_C }
  , m_maxBTSize{ 0, 0, 0 }
  , m_maxTTSize{ 0, 0, 0 }
  , m_maxCuWidth(32)
  , m_maxCuHeight(32)
  , m_numRpl{ 0, 0 }
  , m_rpl1CopyFromRpl0Flag(false)
  , m_rpl1IdxPresentFlag(false)
  , m_allRplEntriesHasSameSignFlag(true)
  , m_longTermRefsPresent(false)
  // Tool list
  , m_transformSkipEnabledFlag(false)
  , m_log2MaxTransformSkipBlockSize(2)
  , m_bdpcmEnabledFlag(false)
  , m_jointCbCrEnabledFlag(false)
  , m_entropyCodingSyncEnabledFlag(false)
  , m_entryPointPresentFlag(false)
  , m_sbtmvpEnabledFlag(false)
  , m_bdofEnabledFlag(false)
  , m_fpelMmvdEnabledFlag(false)
  , m_bdofControlPresentInPhFlag(false)
  , m_dmvrControlPresentInPhFlag(false)
  , m_profControlPresentInPhFlag(false)
  , m_bitsForPoc(8)
  , m_pocMsbCycleFlag(false)
  , m_pocMsbCycleLen(1)
  , m_numExtraPHBytes(0)
  , m_numExtraSHBytes(0)
  , m_numLongTermRefPicSPS(0)
  , m_log2MaxTbSize(6)
  , m_useWeightPred(false)
  , m_useWeightedBiPred(false)
  , m_saoEnabledFlag(false)
  , m_temporalIdNestingFlag(false)
  , m_scalingListEnabledFlag(false)
  , m_virtualBoundariesEnabledFlag(0)
  , m_virtualBoundariesPresentFlag(0)
  , m_numVerVirtualBoundaries(0)
  , m_numHorVirtualBoundaries(0)
  , m_generalHrdParametersPresentFlag(false)
  , m_fieldSeqFlag(false)
  , m_vuiParametersPresentFlag(false)
  , m_vuiParameters()
  , m_wrapAroundEnabledFlag(false)
  , m_ibcFlag(0)
  , m_PLTMode(0)
  , m_lmcsEnabled(false)
  , m_AMVREnabledFlag(false)
  , m_LMChroma(false)
  , m_horCollocatedChromaFlag(true)
  , m_verCollocatedChromaFlag(false)
  , m_LFNST(false)
  , m_Affine(false)
  , m_AffineType(false)
  , m_PROF(false)
  , m_ciip(false)
  , m_Geo(false)
  , m_ladfEnabled(false)
  , m_ladfNumIntervals(0)
  , m_ladfQpOffset{ 0 }
  , m_ladfIntervalLowerBound{ 0 }
  , m_MRL(false)
  , m_MIP(false)
  , m_GDREnabledFlag(true)
  , m_SubLayerCbpParametersPresentFlag(true)
  , m_rprEnabledFlag(false)
  , m_resChangeInClvsEnabledFlag(false)
  , m_interLayerPresentFlag(false)
  , m_gopBasedRPREnabledFlag(false)
  , m_maxNumMergeCand(MRG_MAX_NUM_CANDS)
  , m_maxNumAffineMergeCand(AFFINE_MRG_MAX_NUM_CANDS)
  , m_maxNumIBCMergeCand(IBC_MRG_MAX_NUM_CANDS)
  , m_maxNumGeoCand(0)
  , m_scalingMatrixAlternativeColourSpaceDisabledFlag(false)
  , m_scalingMatrixDesignatedColourSpaceFlag(true)
  , m_disableScalingMatrixForLfnstBlks(true)
{
  m_bitDepths.fill(8);
  m_internalMinusInputBitDepth.fill(0);
  for(int ch=0; ch<MAX_NUM_CHANNEL_TYPE; ch++)
  {
    m_qpBDOffset   [ch] = 0;
  }

  for ( int i = 0; i < MAX_TLAYER; i++ )
  {
    m_maxLatencyIncreasePlus1[i] = 0;
    m_maxDecPicBuffering[i]      = 1;
    m_maxNumReorderPics[i]    = 0;
  }

  ::memset(m_ltRefPicPocLsbSps, 0, sizeof(m_ltRefPicPocLsbSps));
  ::memset(m_usedByCurrPicLtSPSFlag, 0, sizeof(m_usedByCurrPicLtSPSFlag));
  ::memset(m_virtualBoundariesPosX, 0, sizeof(m_virtualBoundariesPosX));
  ::memset(m_virtualBoundariesPosY, 0, sizeof(m_virtualBoundariesPosY));
  ::memset(m_ppsValidFlag, 0, sizeof(m_ppsValidFlag));
}

SPS::~SPS()
{
}

void SPS::setNumSubPics(const uint32_t u)
{
  CHECK( u >= MAX_NUM_SUB_PICS, "Maximum number of subpictures exceeded" );
  m_numSubPics = u;
  m_subPicCtuTopLeftX.resize(m_numSubPics);
  m_subPicCtuTopLeftY.resize(m_numSubPics);
  m_subPicWidth.resize(m_numSubPics);
  m_subPicHeight.resize(m_numSubPics);
  m_subPicTreatedAsPicFlag.resize(m_numSubPics);
  m_loopFilterAcrossSubpicEnabledFlag.resize(m_numSubPics);
  m_subPicId.resize(m_numSubPics);
}

void SPS::createRplList(const RefPicList l, const int numRPL)
{
  m_rplList[l].destroy();
  m_rplList[l].create(numRPL);
  m_numRpl[l]          = numRPL;
  m_rpl1IdxPresentFlag = m_numRpl[REF_PIC_LIST_0] != m_numRpl[REF_PIC_LIST_1];
}
