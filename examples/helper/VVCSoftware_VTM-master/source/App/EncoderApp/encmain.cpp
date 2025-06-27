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

/** \file     encmain.cpp
    \brief    Encoder application main
*/

#include <time.h>
#include <iostream>
#include <chrono>
#include <ctime>

#include "EncoderLib/EncLibCommon.h"
#include "EncApp.h"
#include "Utilities/program_options_lite.h"

//! \ingroup EncoderApp
//! \{

static constexpr uint32_t settingNameWidth = 66;
static constexpr uint32_t settingHelpWidth = 84;
static constexpr uint32_t settingValueWidth = 3;
// --------------------------------------------------------------------------------------------------------------------- //

//macro value printing function

#define PRINT_CONSTANT(NAME, NAME_WIDTH, VALUE_WIDTH) std::cout << std::setw(NAME_WIDTH) << #NAME << " = " << std::setw(VALUE_WIDTH) << NAME << std::endl;

static void printMacroSettings()
{
  if( g_verbosity >= DETAILS )
  {
    std::cout << "Non-environment-variable-controlled macros set as follows: \n" << std::endl;

    //------------------------------------------------

    //setting macros

    PRINT_CONSTANT( RExt__DECODER_DEBUG_BIT_STATISTICS,                         settingNameWidth, settingValueWidth );
    PRINT_CONSTANT( RExt__HIGH_BIT_DEPTH_SUPPORT,                               settingNameWidth, settingValueWidth );
    PRINT_CONSTANT( RExt__HIGH_PRECISION_FORWARD_TRANSFORM,                     settingNameWidth, settingValueWidth );

    //------------------------------------------------

    std::cout << std::endl;
  }
}

#ifdef __linux
#include <cstdlib>
#include <cstdio>
#include <cstring>

int getProcStatusValue(const char* key)
{
  FILE* file = fopen("/proc/self/status", "r");
  int result = -1;
  char line[128];

  int len = strlen(key);
  while (fgets(line, 128, file) != nullptr)
  {
    if (strncmp(line, key, len) == 0)
    {
      result = atoi(line+len);
      break;
    }
  }
  fclose(file);
  return result;
}
#endif

// ====================================================================================================================
// Main function
// ====================================================================================================================

int main(int argc, char* argv[])
{
  // print information
  fprintf( stdout, "\n" );
  fprintf( stdout, "VVCSoftware: VTM Encoder Version %s ", VTM_VERSION );
  fprintf( stdout, NVM_ONOS );
  fprintf( stdout, NVM_COMPILEDBY );
  fprintf( stdout, NVM_BITS );
#if ENABLE_SIMD_OPT
  std::string SIMD;
  ProgramOptionsLite::Options opts;
  opts.addOptions()("SIMD", SIMD, std::string(""), "")("c", ProgramOptionsLite::parseConfigFile, "");
  ProgramOptionsLite::SilentReporter err;
  ProgramOptionsLite::scanArgv(opts, argc, (const char**) argv, err);
  fprintf( stdout, "[SIMD=%s] ", read_x86_extension( SIMD ) );
#endif
#if ENABLE_TRACING
  fprintf( stdout, "[ENABLE_TRACING] " );
#endif
  fprintf( stdout, "\n" );

  std::fstream bitstream;
  EncLibCommon encLibCommon;

  std::vector<EncApp*> pcEncApp(1);
  bool resized = false;
  int layerIdx = 0;

  initROM();

  char** layerArgv = new char*[argc];

  do
  {
    pcEncApp[layerIdx] = new EncApp( bitstream, &encLibCommon );
    // create application encoder class per layer
    pcEncApp[layerIdx]->create();

    // parse configuration per layer
    try
    {
      int j = 0;
      for( int i = 0; i < argc; i++ )
      {
        if( argv[i][0] == '-' && argv[i][1] == 'l' )
        {
          if (argc <= i + 1)
          {
            THROW("Command line parsing error: missing parameter after -lx\n");
          }
          int numParams = 1; // count how many parameters are consumed
          // check for long parameters, which start with "--"
          const std::string param = argv[i + 1];
          if (param.rfind("--", 0) != 0)
          {
            // only short parameters have a second parameter for the value
            if (argc <= i + 2)
            {
              THROW("Command line parsing error: missing parameter after -lx\n");
            }
            numParams++;
          }
          // check if correct layer index
          if( argv[i][2] == std::to_string( layerIdx ).c_str()[0] )
          {
            layerArgv[j] = argv[i + 1];
            if (numParams > 1)
            {
              layerArgv[j + 1] = argv[i + 2];
            }
            j+= numParams;
          }
          i += numParams;
        }
        else
        {
          layerArgv[j] = argv[i];
          j++;
        }
      }

      if( !pcEncApp[layerIdx]->parseCfg( j, layerArgv ) )
      {
        pcEncApp[layerIdx]->destroy();
        return 1;
      }
    }
    catch (ProgramOptionsLite::ParseFailure& e)
    {
      std::cerr << "Error parsing option \"" << e.arg << "\" with argument \"" << e.val << "\"." << std::endl;
      return 1;
    }

    pcEncApp[layerIdx]->createLib( layerIdx );

    if( !resized )
    {
      pcEncApp.resize( pcEncApp[layerIdx]->getMaxLayers() );
      resized = true;
    }

    layerIdx++;
  } while( layerIdx < pcEncApp.size() );

  delete[] layerArgv;

  if (layerIdx > 1)
  {
    int nbLayersUsingAlf = 0;
    int totalUsedAPSIDs = 0;
    std::array<uint8_t, ALF_CTB_MAX_NUM_APS> usedAlfAps;
    usedAlfAps.fill(0);
    bool overlapAPS = false;
    for (uint32_t i = 0; i < layerIdx; i++)
    {
      if (pcEncApp[i]->getALFEnabled())
      {
        nbLayersUsingAlf++;
        totalUsedAPSIDs += pcEncApp[i]->getMaxNumALFAPS();
        for (int apsid = 0; apsid < pcEncApp[i]->getMaxNumALFAPS(); apsid++)
        {
          usedAlfAps[apsid + pcEncApp[i]->getALFAPSIDShift()] ++;
          if (usedAlfAps[apsid + pcEncApp[i]->getALFAPSIDShift()] > 1)
          {
            overlapAPS = true;
          }
        }
      }
    }
    if (totalUsedAPSIDs > ALF_CTB_MAX_NUM_APS || overlapAPS)
    {
      msg(WARNING, "Number of configured ALF APS Ids exceeds maximum for multilayer, or overlap APS Ids - reconfiguring with automatic settings\n");
      int apsShift = 0;
      for (uint32_t i = 0; i < layerIdx; i++)
      {
        if (pcEncApp[i]->getALFEnabled())
        {
          int nbAPS = pcEncApp[i]->getMaxNumALFAPS();
          if (totalUsedAPSIDs > ALF_CTB_MAX_NUM_APS)
          {
            nbAPS = std::min(nbAPS, std::max(1, ALF_CTB_MAX_NUM_APS / nbLayersUsingAlf));
            nbAPS = std::min(nbAPS, ALF_CTB_MAX_NUM_APS - apsShift);
          }
          msg(WARNING, "\tlayer %d : %d: %d -> %d \n", i, nbAPS, apsShift, apsShift + nbAPS - 1);
          pcEncApp[i]->forceMaxNumALFAPS(nbAPS);
          pcEncApp[i]->forceALFAPSIDShift(apsShift);
          apsShift += nbAPS;
        }
      }
    }
    VPS* vps = pcEncApp[0]->getVPS();
    //check chroma format and bit-depth for dependent layers
    for (uint32_t i = 0; i < layerIdx; i++)
    {
      const ChromaFormat curLayerChromaFormatIdc = pcEncApp[i]->getChromaFormatIDC();
      int curLayerBitDepth = pcEncApp[i]->getBitDepth();
      for (uint32_t j = 0; j < layerIdx; j++)
      {
        if (vps->getDirectRefLayerFlag(i, j))
        {
          const ChromaFormat refLayerChromaFormatIdcInVPS = pcEncApp[j]->getChromaFormatIDC();
          CHECK(curLayerChromaFormatIdc != refLayerChromaFormatIdcInVPS, "The chroma formats of the current layer and the reference layer are different");
          int refLayerBitDepthInVPS = pcEncApp[j]->getBitDepth();
          CHECK(curLayerBitDepth != refLayerBitDepthInVPS, "The bit-depth of the current layer and the reference layer are different");
        }
      }
    }
  }

#if PRINT_MACRO_VALUES
  printMacroSettings();
#endif

  // starting time
  auto startTime  = std::chrono::steady_clock::now();
  std::time_t startTime2 = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  fprintf(stdout, " started @ %s", std::ctime(&startTime2) );
  clock_t startClock = clock();

  // call encoding function per layer
  bool eos = false;

  while( !eos )
  {
    // read GOP
    bool keepLoop = true;
    while( keepLoop )
    {
      for( auto & encApp : pcEncApp )
      {
#ifndef _DEBUG
        try
        {
#endif
          keepLoop = encApp->encodePrep( eos );
#ifndef _DEBUG
        }
        catch( Exception &e )
        {
          std::cerr << e.what() << std::endl;
          return EXIT_FAILURE;
        }
        catch( const std::bad_alloc &e )
        {
          std::cout << "Memory allocation failed: " << e.what() << std::endl;
          return EXIT_FAILURE;
        }
#endif
      }
    }

    // encode GOP
    keepLoop = true;
    while( keepLoop )
    {
      for( auto & encApp : pcEncApp )
      {
#ifndef _DEBUG
        try
        {
#endif
          keepLoop = encApp->encode();
#ifndef _DEBUG
        }
        catch( Exception &e )
        {
          std::cerr << e.what() << std::endl;
          return EXIT_FAILURE;
        }
        catch( const std::bad_alloc &e )
        {
          std::cout << "Memory allocation failed: " << e.what() << std::endl;
          return EXIT_FAILURE;
        }
#endif
      }
    }
  }
  for( auto & encApp : pcEncApp )
  {
    if (encApp->getNNPostFilterEnabled())
    {
      encApp->applyNnPostFilter();
    }
  }

#ifdef __linux
  int vm = getProcStatusValue("VmPeak:");
  int rm = getProcStatusValue("VmHWM:");
  printf("\nMemory Usage: VmPeak= %d KB ( %.1f GiB ),  VmHWM= %d KB ( %.1f GiB )\n", vm, (double)vm/(1024*1024), rm, (double)rm/(1024*1024));
#endif

  // ending time
  clock_t endClock = clock();
  auto endTime = std::chrono::steady_clock::now();
  std::time_t endTime2 = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#if JVET_O0756_CALCULATE_HDRMETRICS
  auto metricTime = pcEncApp[0]->getMetricTime();

  for( int layerIdx = 1; layerIdx < pcEncApp.size(); layerIdx++ )
  {
    metricTime += pcEncApp[layerIdx]->getMetricTime();
  }
  auto totalTime      = std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime ).count();
  auto encTime        = std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime - metricTime ).count();
  auto metricTimeuser = std::chrono::duration_cast<std::chrono::milliseconds>( metricTime ).count();
#else
  auto encTime = std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime).count();
#endif
#if GREEN_METADATA_SEI_ENABLED
  for( auto & encApp : pcEncApp )
  {
    FeatureCounterStruct  featureCounterFinal = encApp->getFeatureCounter();
    featureCounterFinal.bytes = encApp->getTotalNumberOfBytes();
    FeatureCounterStruct dummy;
    writeGMFAOutput(featureCounterFinal, dummy, encApp->getGMFAFile(),true);
  }
#endif
  for( auto & encApp : pcEncApp )
  {
    encApp->destroyLib();

    // destroy application encoder class per layer
    encApp->destroy();

    delete encApp;
  }

  // destroy ROM
  destroyROM();

  pcEncApp.clear();

  printf( "\n finished @ %s", std::ctime(&endTime2) );

#if JVET_O0756_CALCULATE_HDRMETRICS
  printf(" Encoding Time (Total Time): %12.3f ( %12.3f ) sec. [user] %12.3f ( %12.3f ) sec. [elapsed]\n",
         ((endClock - startClock) * 1.0 / CLOCKS_PER_SEC) - (metricTimeuser/1000.0),
         (endClock - startClock) * 1.0 / CLOCKS_PER_SEC,
         encTime / 1000.0,
         totalTime / 1000.0);
#else
  printf(" Total Time: %12.3f sec. [user] %12.3f sec. [elapsed]\n",
         (endClock - startClock) * 1.0 / CLOCKS_PER_SEC,
         encTime / 1000.0);
#endif

  return 0;
}

//! \}
