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

/** \file     SEIRemovalAppCfg.cpp
    \brief    Decoder configuration class
*/

#include <cstdio>
#include <cstring>
#include <string>
#include "SEIRemovalAppCfg.h"
#include "Utilities/program_options_lite.h"

using namespace std;
namespace po = df::program_options_lite;

//! \ingroup DecoderApp
//! \{

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/** \param argc number of arguments
    \param argv array of arguments
 */
Bool SEIRemovalAppCfg::parseCfg( Int argc, TChar* argv[] )
{
  Bool do_help = false;
  Int warnUnknowParameter = 0;
  po::Options opts;
  opts.addOptions()

  ("help",                      do_help,                               false,      "this help text")
  ("BitstreamFileIn,b",         m_bitstreamFileNameIn,                 string(""), "bitstream input file name")
  ("BitstreamFileOut,o",        m_bitstreamFileNameOut,                string(""), "bitstream output file name")
  ("DiscardPrefixSEI,p",        m_discardPrefixSEIs,                   false,      "remove all prefix SEIs (default: 0)")
  ("DiscardSuffixSEI,s",        m_discardSuffixSEIs,                   true,       "remove all suffix SEIs (default: 1)")
  ("NumSkip",                   m_numNALUnitsToSkip,                   0,          "number of NAL units to skip (counted inclusive the units skipped with -p/-s options)" )
  ("NumWrite",                  m_numNALUnitsToWrite,                  -1,         "number of NAL units to write (counted inclusive the units skipped with -p/-s/--NumSkip options), -1 to disable" )

  ("WarnUnknowParameter,w",     warnUnknowParameter,                   0,          "warn for unknown configuration parameters instead of failing")
  ;

  po::setDefaults(opts);
  po::ErrorReporter err;
  const list<const TChar*>& argv_unhandled = po::scanArgv(opts, argc, (const TChar**) argv, err);

  for (list<const TChar*>::const_iterator it = argv_unhandled.begin(); it != argv_unhandled.end(); it++)
  {
    std::cerr << "Unhandled argument ignored: "<< *it << std::endl;
  }

  if (argc == 1 || do_help)
  {
    po::doHelp(cout, opts);
    return false;
  }

  if (err.is_errored)
  {
    if (!warnUnknowParameter)
    {
      /* errors have already been reported to stderr */
      return false;
    }
  }


  if (m_bitstreamFileNameIn.empty())
  {
    std::cerr << "No input file specified, aborting" << std::endl;
    return false;
  }
  if (m_bitstreamFileNameOut.empty())
  {
    std::cerr << "No output file specified, aborting" << std::endl;
    return false;
  }


  return true;
}

SEIRemovalAppCfg::SEIRemovalAppCfg()
: m_bitstreamFileNameIn()
, m_bitstreamFileNameOut()
, m_discardPrefixSEIs( false )
, m_discardSuffixSEIs( false )
{
}

SEIRemovalAppCfg::~SEIRemovalAppCfg()
{
}

//! \}
