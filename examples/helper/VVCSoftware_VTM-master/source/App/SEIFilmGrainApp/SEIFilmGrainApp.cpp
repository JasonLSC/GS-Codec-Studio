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

/** \file     SEIFilmGrainApp.cpp
    \brief    Decoder application class
*/

#include <list>
#include <vector>
#include <stdio.h>
#include <fcntl.h>

#include "SEIFilmGrainApp.h"
#include "DecoderLib/AnnexBread.h"
#include "EncoderLib/AnnexBwrite.h"

//! \ingroup SEIFilmGrainApp
//! \{

// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================

SEIFilmGrainApp::SEIFilmGrainApp()
{
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/**
 - create internal class
 - initialize internal class
 - until the end of the bitstream, call decoding function in SEIFilmGrainApp class
 - delete allocated buffers
 - destroy internal class
 - returns the number of mismatching pictures
 */

void read2(InputNALUnit& nalu)
{
  InputBitstream& bs = nalu.getBitstream();

  nalu.m_forbiddenZeroBit = bs.read(1);           // forbidden zero bit
  nalu.m_nuhReservedZeroBit = bs.read(1);         // nuh_reserved_zero_bit
  nalu.m_nuhLayerId = bs.read(6);                 // nuh_layer_id
  nalu.m_nalUnitType = (NalUnitType)bs.read(5);   // nal_unit_type
  nalu.m_temporalId = bs.read(3) - 1;             // nuh_temporal_id_plus1
}

void SEIFilmGrainApp::setSEIFilmGrainCharacteristics(SEIFilmGrainCharacteristics *pFgcParameters)
{
  //  Set SEI message parameters read from command line options
  pFgcParameters->m_filmGrainCharacteristicsCancelFlag = m_fgcSEICancelFlag;
  pFgcParameters->m_filmGrainCharacteristicsPersistenceFlag = m_fgcSEIPersistenceFlag;
  pFgcParameters->m_separateColourDescriptionPresentFlag = m_fgcSEISepColourDescPresentFlag;
  pFgcParameters->m_filmGrainModelId = m_fgcSEIModelID;
  pFgcParameters->m_blendingModeId = m_fgcSEIBlendingModeID;
  pFgcParameters->m_log2ScaleFactor = m_fgcSEILog2ScaleFactor;
  for (int i = 0; i < MAX_NUM_COMPONENT; i++)
  {
    pFgcParameters->m_compModel[i].presentFlag = m_fgcSEICompModelPresent[i];
    if (pFgcParameters->m_compModel[i].presentFlag)
    {
      pFgcParameters->m_compModel[i].numModelValues = 1 + m_fgcSEINumModelValuesMinus1[i];
      pFgcParameters->m_compModel[i].numIntensityIntervals = 1 + m_fgcSEINumIntensityIntervalMinus1[i];
      pFgcParameters->m_compModel[i].intensityValues.resize( pFgcParameters->m_compModel[i].numIntensityIntervals );
      for (int j = 0; j < pFgcParameters->m_compModel[i].numIntensityIntervals; j++)
      {
        pFgcParameters->m_compModel[i].intensityValues[j].intensityIntervalLowerBound = m_fgcSEIIntensityIntervalLowerBound[i][j];
        pFgcParameters->m_compModel[i].intensityValues[j].intensityIntervalUpperBound = m_fgcSEIIntensityIntervalUpperBound[i][j];
        pFgcParameters->m_compModel[i].intensityValues[j].compModelValue.resize( pFgcParameters->m_compModel[i].numModelValues );
        for (int k = 0; k < pFgcParameters->m_compModel[i].numModelValues; k++)
        {
          pFgcParameters->m_compModel[i].intensityValues[j].compModelValue[k] = m_fgcSEICompModelValue[i][j][k];
        }
      }
    }
  }
}

void SEIFilmGrainApp::printSEIFilmGrainCharacteristics(SEIFilmGrainCharacteristics *pFgcParameters)
{
  fprintf(stdout, "--------------------------------------\n");
  fprintf(stdout, "fg_characteristics_cancel_flag = %d\n", pFgcParameters->m_filmGrainCharacteristicsCancelFlag);
  fprintf(stdout, "fg_model_id = %d\n", pFgcParameters->m_filmGrainModelId);
  fprintf(stdout, "fg_separate_colour_description_present_flag = %d\n", pFgcParameters->m_separateColourDescriptionPresentFlag);
  fprintf(stdout, "fg_blending_mode_id = %d\n", pFgcParameters->m_blendingModeId);
  fprintf(stdout, "fg_log2_scale_factor = %d\n", pFgcParameters->m_log2ScaleFactor);
  for (int c = 0; c < MAX_NUM_COMPONENT; c++)
  {
    fprintf(stdout, "fg_comp_model_present_flag[c] = %d\n", pFgcParameters->m_compModel[c].presentFlag);
  }
  for (int c = 0; c < MAX_NUM_COMPONENT; c++)
  {
    if (pFgcParameters->m_compModel[c].presentFlag)
    {
      fprintf(stdout, "num_intensity_intervals_minus1[c] = %d\n", pFgcParameters->m_compModel[c].numIntensityIntervals - 1);
      fprintf(stdout, "fg_num_model_values_minus1[c] = %d\n", pFgcParameters->m_compModel[c].numModelValues - 1);
      for (int i = 0; i < pFgcParameters->m_compModel[c].numIntensityIntervals; i++)
      {
        fprintf(stdout, "fg_intensity_interval_lower_bound[c][i] = %d\n", pFgcParameters->m_compModel[c].intensityValues[i].intensityIntervalLowerBound);
        fprintf(stdout, "fg_intensity_interval_upper_bound[c][i] = %d\n", pFgcParameters->m_compModel[c].intensityValues[i].intensityIntervalUpperBound);
        for (int j = 0; j < pFgcParameters->m_compModel[c].numModelValues; j++)
        {
          fprintf(stdout, "fg_comp_model_value[c][i][j] = %d\n", pFgcParameters->m_compModel[c].intensityValues[i].compModelValue[j]);
        }
      }
    }
  }
  fprintf(stdout, "fg_characteristics_persistence_flag = %d\n", pFgcParameters->m_filmGrainCharacteristicsPersistenceFlag);
  fprintf(stdout, "--------------------------------------\n");
}

uint32_t SEIFilmGrainApp::process()
{
  std::ifstream bitstreamFileIn(m_bitstreamFileNameIn.c_str(), std::ifstream::in | std::ifstream::binary);
  if (!bitstreamFileIn)
  {
    EXIT( "failed to open bitstream file " << m_bitstreamFileNameIn.c_str() << " for reading" ) ;
  }

  std::ofstream bitstreamFileOut(m_bitstreamFileNameOut.c_str(), std::ifstream::out | std::ifstream::binary);

  InputByteStream bytestream(bitstreamFileIn);

  bitstreamFileIn.clear();
  bitstreamFileIn.seekg(0, std::ios::beg);

  int NALUcount = 0;
  int SEIcount = 0;

  while (!!bitstreamFileIn)
  {
    /* location serves to work around a design fault in the decoder, whereby
     * the process of reading a new slice that is the first slice of a new frame
     * requires the SEIFilmGrainApp::process() method to be called again with the same
     * nal unit. */
    AnnexBStats stats = AnnexBStats();

    InputNALUnit nalu;
    byteStreamNALUnit(bytestream, nalu.getBitstream().getFifo(), stats);

    bool writeNALU = true;
    bool removeSEI = false;
    bool insertSEI = false;

    // call actual decoding function
    if (nalu.getBitstream().getFifo().empty())
    {
      /* this can happen if the following occur:
       *  - empty input file
       *  - two back-to-back start_code_prefixes
       *  - start_code_prefix immediately followed by EOF
       */
      std::cerr << "Warning: Attempt to process an empty NAL unit" <<  std::endl;
    }
    else
    {
      read2(nalu);
      NALUcount++;
      SEIMessages SEIs;
      VPS *vps = nullptr;

      if (nalu.m_nalUnitType == NAL_UNIT_PPS && m_seiFilmGrainOption == 2)
      {
        fprintf(stdout, "Option %d: Insert FGC SEI message ...\n", m_seiFilmGrainOption);
        SEIFilmGrainCharacteristics *sei = new SEIFilmGrainCharacteristics;
        setSEIFilmGrainCharacteristics(sei);
        if (m_seiFilmGrainPrint)
        {
          printSEIFilmGrainCharacteristics(sei);
        }
        SEIs.push_back(sei);
        insertSEI = true;
      } // end PPS UnitType

      if (nalu.m_nalUnitType == NAL_UNIT_PREFIX_SEI)
      {
        // parse FGC SEI
        m_seiReader.parseSEImessage(&(nalu.getBitstream()), SEIs, nalu.m_nalUnitType, nalu.m_nuhLayerId, nalu.m_temporalId, vps, nullptr, m_hrd, &std::cout);

        SEI::PayloadType             payloadType;
        std::list<SEI*>::iterator message;
        SEIFilmGrainCharacteristics *fgcParameters;
        for (message = SEIs.begin(); message != SEIs.end(); ++message)
        {
          SEIcount++;
          payloadType = (*message)->payloadType();
          if (payloadType == SEI::PayloadType::FILM_GRAIN_CHARACTERISTICS)
          {
            if (m_seiFilmGrainOption == 1)  // remove FGC SEI
            {
              fprintf(stdout, "Option %d: Remove FGC SEI message ...\n", m_seiFilmGrainOption);
              removeSEI = true;
            }
            else if (m_seiFilmGrainOption == 3) // rewrite FGC SEI
            {
              fprintf(stdout, "Option %d: Rewrite FGC SEI message ...\n", m_seiFilmGrainOption);
              fgcParameters = static_cast<SEIFilmGrainCharacteristics*>(*message);
              setSEIFilmGrainCharacteristics(fgcParameters);
              if (m_seiFilmGrainPrint)
              {
                printSEIFilmGrainCharacteristics(fgcParameters);
              }
              removeSEI = true;
              insertSEI = true;
            }
          } // end FGC SEI
        }
      } // end SEI UnitType

      // write Nal Unit
      if (writeNALU && !removeSEI && bitstreamFileOut)
      {
        int iNumZeros = stats.m_numLeadingZero8BitsBytes + stats.m_numZeroByteBytes + stats.m_numStartCodePrefixBytes - 1;
        char ch = 0;
        for (int i = 0; i < iNumZeros; i++) { bitstreamFileOut.write(&ch, 1); }
        ch = 1; bitstreamFileOut.write(&ch, 1);
        bitstreamFileOut.write((const char*)nalu.getBitstream().getFifo().data(), nalu.getBitstream().getFifo().size());
      }

      // write FGC SEI
      if (writeNALU && insertSEI && bitstreamFileOut)
      {
        const bool useLongStartCode = (nalu.m_nalUnitType == NAL_UNIT_OPI || nalu.m_nalUnitType == NAL_UNIT_DCI || nalu.m_nalUnitType == NAL_UNIT_VPS || nalu.m_nalUnitType == NAL_UNIT_SPS
                                       || nalu.m_nalUnitType == NAL_UNIT_PPS || nalu.m_nalUnitType == NAL_UNIT_PREFIX_APS || nalu.m_nalUnitType == NAL_UNIT_SUFFIX_APS);
        SEIMessages   currentMessages  = extractSeisByType(SEIs, SEI::PayloadType::FILM_GRAIN_CHARACTERISTICS);
        OutputNALUnit outNalu(NAL_UNIT_PREFIX_SEI, nalu.m_nuhLayerId, nalu.m_temporalId);
        m_seiWriter.writeSEImessages(outNalu.m_bitstream, currentMessages, m_hrd, false, nalu.m_temporalId);
        NALUnitEBSP naluWithHeader(outNalu);
        writeAnnexBNalUnit(bitstreamFileOut, naluWithHeader, useLongStartCode);
      }
    }

  } // end bitstreamFileIn

  if (m_seiFilmGrainOption)
  {
    fprintf(stdout, "\n\n========================= SUMMARY =============================== \n");
    fprintf(stdout, "  Total NALU count: %d \n", NALUcount);
    fprintf(stdout, "  Total SEI count : %d \n", SEIcount);
    fprintf(stdout, "  FGC SEI process : %d \n", m_seiFilmGrainOption);
    fprintf(stdout, "================================================================= \n");
  }
  return 0;
}

//! \}
