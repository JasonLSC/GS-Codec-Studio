/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2020, ITU/ISO/IEC
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
#include "TLibDecoder/AnnexBread.h"

#if JVET_X0048_X0103_FILM_GRAIN
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

Void read2(InputNALUnit& nalu)
{
  TComInputBitstream& bs = nalu.getBitstream();

  Bool forbidden_zero_bit = bs.read(1);           // forbidden_zero_bit
  if (forbidden_zero_bit != 0)
  {
    std::cerr << "Forbidden zero-bit not '0'" << std::endl;
    exit(1);
  }
  nalu.m_nalUnitType = (NalUnitType) bs.read(6);  // nal_unit_type
  nalu.m_nuhLayerId = bs.read(6);                 // nuh_layer_id
  nalu.m_temporalId = bs.read(3) - 1;             // nuh_temporal_id_plus1
}

Void SEIFilmGrainApp::setSEIFilmGrainCharacteristics(SEIFilmGrainCharacteristics *pFgcParameters)
{
  //  Set SEI message parameters read from command line options
  pFgcParameters->m_filmGrainCharacteristicsCancelFlag = m_fgcSEICancelFlag;
  pFgcParameters->m_filmGrainCharacteristicsPersistenceFlag = m_fgcSEIPersistenceFlag;
  pFgcParameters->m_separateColourDescriptionPresentFlag = m_fgcSEISepColourDescPresentFlag;
  pFgcParameters->m_filmGrainModelId = m_fgcSEIModelID;
  pFgcParameters->m_blendingModeId = m_fgcSEIBlendingModeID;
  pFgcParameters->m_log2ScaleFactor = m_fgcSEILog2ScaleFactor;
  for (UInt c = 0; c < MAX_NUM_COMPONENT; c++)
  {
    pFgcParameters->m_compModel[c].bPresentFlag = m_fgcSEICompModelPresent[c];
    if (pFgcParameters->m_compModel[c].bPresentFlag)
    {
      pFgcParameters->m_compModel[c].numModelValues = 1 + m_fgcSEINumModelValuesMinus1[c];
      pFgcParameters->m_compModel[c].numIntensityIntervals = 1 + m_fgcSEINumIntensityIntervalMinus1[c];
      pFgcParameters->m_compModel[c].intensityValues.resize(pFgcParameters->m_compModel[c].numIntensityIntervals);
      for (UInt i = 0; i < pFgcParameters->m_compModel[c].numIntensityIntervals; i++)
      {
        pFgcParameters->m_compModel[c].intensityValues[i].intensityIntervalLowerBound = m_fgcSEIIntensityIntervalLowerBound[c][i];
        pFgcParameters->m_compModel[c].intensityValues[i].intensityIntervalUpperBound = m_fgcSEIIntensityIntervalUpperBound[c][i];
        pFgcParameters->m_compModel[c].intensityValues[i].compModelValue.resize(pFgcParameters->m_compModel[c].numModelValues);
        for (UInt j = 0; j < pFgcParameters->m_compModel[c].numModelValues; j++)
        {
          pFgcParameters->m_compModel[c].intensityValues[i].compModelValue[j] = m_fgcSEICompModelValue[c][i][j];
        }
      }
    }
  }
}

Void SEIFilmGrainApp::printSEIFilmGrainCharacteristics(SEIFilmGrainCharacteristics *pFgcParameters)
{
  fprintf(stdout, "--------------------------------------\n");
  fprintf(stdout, "fg_characteristics_cancel_flag = %d\n", pFgcParameters->m_filmGrainCharacteristicsCancelFlag);
  fprintf(stdout, "fg_model_id = %d\n", pFgcParameters->m_filmGrainModelId);
  fprintf(stdout, "fg_separate_colour_description_present_flag = %d\n", pFgcParameters->m_separateColourDescriptionPresentFlag);
  fprintf(stdout, "fg_blending_mode_id = %d\n", pFgcParameters->m_blendingModeId);
  fprintf(stdout, "fg_log2_scale_factor = %d\n", pFgcParameters->m_log2ScaleFactor);
  for (int c = 0; c < MAX_NUM_COMPONENT; c++)
  {
    fprintf(stdout, "fg_comp_model_present_flag[c] = %d\n", pFgcParameters->m_compModel[c].bPresentFlag);
  }
  for (int c = 0; c < MAX_NUM_COMPONENT; c++)
  {
    if (pFgcParameters->m_compModel[c].bPresentFlag)
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

UInt SEIFilmGrainApp::process()
{
  ifstream bitstreamFileIn(m_bitstreamFileNameIn.c_str(), ifstream::in | ifstream::binary);
  if (!bitstreamFileIn)
  {
    std::cerr << "failed to open bitstream file " << m_bitstreamFileNameIn.c_str() << " for reading";
    exit(1);
  }

  ofstream bitstreamFileOut(m_bitstreamFileNameOut.c_str(), ifstream::out | ifstream::binary);

  InputByteStream bytestream(bitstreamFileIn);

  bitstreamFileIn.clear();
  bitstreamFileIn.seekg( 0, ios::beg );

  Int NALUcount = 0;
  Int SEIcount = 0;

  while (!!bitstreamFileIn)
  {
    /* location serves to work around a design fault in the decoder, whereby
     * the process of reading a new slice that is the first slice of a new frame
     * requires the SEIFilmGrainApp::process() method to be called again with the same
     * nal unit. */
    AnnexBStats stats = AnnexBStats();

    InputNALUnit nalu;
    byteStreamNALUnit(bytestream, nalu.getBitstream().getFifo(), stats);

    Bool bWrite = true;
    Bool bRemoveSEI = false;
    Bool bInsertSEI = false;

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
      read2( nalu );
      NALUcount++;
      SEIMessages SEIs;

      if (nalu.m_nalUnitType == NAL_UNIT_PPS && m_seiFilmGrainOption == 2)
      {
        bInsertSEI = true;
        SEIFilmGrainCharacteristics *sei = new SEIFilmGrainCharacteristics;
        setSEIFilmGrainCharacteristics(sei);
        if (m_seiFilmGrainPrint)
        {
          printSEIFilmGrainCharacteristics(sei);
        }
        SEIs.push_back(sei);
      } // end Coded Slice UnitType

      if (nalu.m_nalUnitType == NAL_UNIT_PREFIX_SEI && m_seiFilmGrainOption)
      {
        // parse FGC SEI
        m_seiReader.parseSEImessage(&(nalu.getBitstream()), SEIs, nalu.m_nalUnitType, m_parameterSetManager.getActiveSPS(), &std::cout);

        int payloadType = 0;
        std::list<SEI*>::iterator message;
        for (message = SEIs.begin(); message != SEIs.end(); ++message)
        {
          SEIcount++;
          payloadType = (*message)->payloadType();
          if (payloadType == SEI::FILM_GRAIN_CHARACTERISTICS)
          {
            bRemoveSEI = true;
            if (m_seiFilmGrainOption == 3) // rewrite FGC SEI
            {
              SEIFilmGrainCharacteristics *fgcParameters = static_cast<SEIFilmGrainCharacteristics*>(*message);
              setSEIFilmGrainCharacteristics(fgcParameters);
              if (m_seiFilmGrainPrint)
              {
                printSEIFilmGrainCharacteristics(fgcParameters);
              }
              bInsertSEI = true;
            }
          } // end FGC SEI
        }
      } // end SEI UnitType

      // write regular NalUnit
      if (bWrite && !bRemoveSEI && bitstreamFileOut)
      {
        if (nalu.m_nalUnitType < 16 && m_seiFilmGrainOption == 1) { stats.m_numZeroByteBytes++; }
        if (nalu.m_nalUnitType < 16 && m_seiFilmGrainOption == 2) { stats.m_numZeroByteBytes--; }
        int iNumZeros = stats.m_numLeadingZero8BitsBytes + stats.m_numZeroByteBytes + stats.m_numStartCodePrefixBytes - 1;
        char ch = 0;
        for (int i = 0; i < iNumZeros; i++) { bitstreamFileOut.write(&ch, 1); }
        ch = 1; bitstreamFileOut.write(&ch, 1);
        bitstreamFileOut.write((const char*)nalu.getBitstream().getFifo().data(), nalu.getBitstream().getFifo().size());
      }

      // write FGC SEI NalUnit
      if (bWrite && bInsertSEI && bitstreamFileOut)
      {
        Bool useLongStartCode = (nalu.m_nalUnitType == NAL_UNIT_VPS || nalu.m_nalUnitType == NAL_UNIT_SPS || nalu.m_nalUnitType == NAL_UNIT_PPS);
        SEIMessages currentMessages = extractSeisByType(SEIs, SEI::FILM_GRAIN_CHARACTERISTICS);
        OutputNALUnit outNalu(NAL_UNIT_PREFIX_SEI, nalu.m_temporalId);
        m_seiWriter.writeSEImessages(outNalu.m_Bitstream, currentMessages, m_parameterSetManager.getActiveSPS(), false);
        NALUnitEBSP naluWithHeader(outNalu);
        static const UChar startCodePrefix[] = { 0,0,0,1 };
        if (useLongStartCode) { bitstreamFileOut.write(reinterpret_cast<const char*>(startCodePrefix), 4); }
        else { bitstreamFileOut.write(reinterpret_cast<const char*>(startCodePrefix + 1), 3); }
        bitstreamFileOut << naluWithHeader.m_nalUnitData.str();
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
#endif
