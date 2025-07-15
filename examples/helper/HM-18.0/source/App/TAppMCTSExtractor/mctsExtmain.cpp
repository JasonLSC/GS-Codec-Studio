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

 /** \file     mctsExtmain.cpp
  \brief    MCTS extractor application main
  */

#include <stdlib.h>  
#include <stdio.h>
#include <time.h>
#include "TAppMctsExtTop.h"

  //! \ingroup TAppMctsExt
  //! \{

  // ====================================================================================================================
  // Main function
  // ====================================================================================================================

int main(int argc, char* argv[])
{
  Int returnCode = EXIT_SUCCESS;

#if MCTS_EXTRACTION
  TAppMctsExtTop  cTAppMctsExtTop;

  // print information
  fprintf(stdout, "\n");
  fprintf(stdout, "HM software: MCTS Extractor Version [%s] ", NV_VERSION);
  fprintf(stdout, NVM_ONOS);
  fprintf(stdout, NVM_COMPILEDBY);
  fprintf(stdout, NVM_BITS);
  fprintf(stdout, "\n");

  // create application MCTS Extraction class
  cTAppMctsExtTop.create();

  // parse configuration
  if (!cTAppMctsExtTop.parseCfg(argc, argv))
  {
    cTAppMctsExtTop.destroy();
    returnCode = EXIT_FAILURE;
    return returnCode;
  }

  // starting time
  Double dResult;
  clock_t lBefore = clock();

  // call extraction function
  cTAppMctsExtTop.extract();

  // ending time
  dResult = (Double)(clock() - lBefore) / CLOCKS_PER_SEC;
  printf("\n Total Time: %12.3f sec.\n", dResult);

  // destroy application extractor class
  cTAppMctsExtTop.destroy();
#endif
  return returnCode;
}
//! \}
