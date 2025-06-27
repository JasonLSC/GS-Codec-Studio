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

 /** \file     SubpicMergeMain.cpp
     \brief    Subpicture merge main function and command line handling
 */

#include <cstdio>
#include <cctype>
#include <cstring>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <ios>
#include <algorithm>
#include "Utilities/program_options_lite.h"
#include "SubpicMergeApp.h"

namespace po = ProgramOptionsLite;

//! \ingroup SubpicMergeApp
//! \{


/**
  - Parse file name of an input bitstream or a YUV file
 */
void getSubpicFilename(std::istringstream &iss, std::string &fname)
{
  char currChar;

  while (iss.get(currChar) && isspace(currChar))
  {
  }

  if (currChar == '\"')  // Is file name delimited by quotes?
  {
    while (iss.get(currChar) && (currChar != '\"'))
    {
      fname.append((size_t)1, currChar);
    }
  }
  else  // Nn quote found - assume no whitespaces in file name
  {
    do {
      fname.append((size_t)1, currChar);
    } while (iss.get(currChar) && !isspace(currChar));
  }
}

/**
  - Parse subpic list file containing all subpictures to be merged
 */
bool parseSubpicListFile(const char *filename, std::vector<SubpicParams> &subpics)
{
  std::ifstream confFile;
  confFile.open(filename);
  if (!confFile.is_open())
  {
    std::cerr << "Error: cannot open subpic list file " << filename << " for reading" << std::endl;
    return false;
  }

  std::string line;
  while (std::getline(confFile, line))
  {
    if (line.at(0) != '#')  // Lines beginning with # are ignored
    {
      std::istringstream iss(line);
      std::string bitstreamFilename;

      subpics.emplace_back();
      SubpicParams &sf = subpics.back();

      if (!(iss >> sf.width >> sf.height >> sf.topLeftCornerX >> sf.topLeftCornerY))
      {
        std::cerr << "Error: subpic list file has incorrect formatting" << std::endl;
        return false;
      }

      getSubpicFilename(iss, bitstreamFilename);

      sf.fp.open(bitstreamFilename, std::ios_base::binary);
      if (!sf.fp.is_open())
      {
        std::cerr << "Error: cannot open input file " << bitstreamFilename << " for reading" << std::endl;
        return false;
      }
    }
  }

  return true;
}


/**
  - Parse command line parameters
*/
bool parseCmdLine(int argc, char* argv[], std::vector<SubpicParams> &subpics, std::ofstream &outputStream, bool &yuvMerge, int &yuvBitdepth, int &yuvChromaFormat, bool &mixedNaluFlag)
{
  bool doHelp = false;
  std::string subpicListFile;
  std::string outFile;

  po::Options opts;
  opts.addOptions()
    ("-help",                            doHelp,                                           false, "This help text")
    ("l",                                subpicListFile,                         std::string(""), "File containing list of input pictures to be merged")
    ("o",                                outFile,                                std::string(""), "Output file name")
    ("m",                                mixedNaluFlag,                                    false, "Enable mixed NALU type bitstreams merging")
    ("-yuv",                             yuvMerge,                                         false, "Perform YUV merging (instead of bitstream merging)")
    ("d",                                yuvBitdepth,                                          0, "Bitdepth for YUV merging")
    ("f",                                yuvChromaFormat,                                    420, "Chroma format for YUV merging, 420 (default), 400, 422 or 444")
    ;

  po::setDefaults(opts);
  po::ErrorReporter err;
  const std::list<const char*>& argvUnhandled = po::scanArgv(opts, argc, (const char**) argv, err);

  if (argc == 1 || doHelp)
  {
    /* argc == 1: no options have been specified */
    po::doHelp(std::cout, opts);
    std::cout << std::endl;
    std::cout << "Examples" << std::endl;
    std::cout << " Merge VVC bitstreams:" << std::endl;
    std::cout << "   SubpicMergeApp -l subpic_list.txt -o merged_stream.bin" << std::endl;
    std::cout << " Merge YUV files (10-bit):" << std::endl;
    std::cout << "   SubpicMergeApp -yuv 1 -d 10 -l subpic_list_yuv.txt -o merged_yuv.yuv" << std::endl;
    std::cout << std::endl;
    std::cout << "Subpic list file contains list of subpics to be merged, one line for each subpicture. Format of a line in subpic list file is given below. Unit of subpic dimensions is one luma sample. Input file is either bitstream file or YUV file depending on the -yuv option."  << std::endl;
    std::cout << std::endl;
    std::cout << "  subpic_width  subpic_height  subpic_x_pos  subpic_y_pos  subpic_input_file_name" << std::endl;
    std::cout << std::endl;
    return false;
  }

  if (yuvChromaFormat != 400 && yuvChromaFormat != 420 && yuvChromaFormat != 422 && yuvChromaFormat != 444 )
  {
    std::cerr << "Illegal chroma format: " << yuvChromaFormat << std::endl;
    return false;
  }

  for (std::list<const char*>::const_iterator it = argvUnhandled.begin(); it != argvUnhandled.end(); it++)
  {
    std::cerr << "Unhandled argument ignored: `" << *it << "'" << std::endl;
  }

  if (!parseSubpicListFile(subpicListFile.c_str(), subpics))
  {
    return false;
  }

  if (subpics.empty())
  {
    std::cerr << "Error: subpic list file does not define valid subpictures" << std::endl;
    return false;
  }

  outputStream.open(outFile, std::ios_base::binary);
  if (!outputStream.is_open())
  {
    std::cerr << "Error: cannot open output file " << outFile << " for writing" << std::endl;
    return false;
  }

  return true;
}


/**
  - Subpicture merge main()
 */
int main(int argc, char* argv[])
{
  std::vector<SubpicParams> subpics;
  std::ofstream outputStream;
  bool yuvMerge = false;
  int yuvBitdepth = -1;
  int yuvChromaFormat = -1;
  bool mixedNaluFlag = false;

  if (!parseCmdLine(argc, argv, subpics, outputStream, yuvMerge, yuvBitdepth, yuvChromaFormat, mixedNaluFlag))
  {
    return 1;
  }

  SubpicMergeApp *subpicMergeApp = new SubpicMergeApp(subpics, outputStream);

  if (!yuvMerge)
  {
    subpicMergeApp->mergeStreams(mixedNaluFlag);
  }
  else
  {
    subpicMergeApp->mergeYuvFiles(yuvBitdepth, yuvChromaFormat);
  }

  delete subpicMergeApp;

  return 0;
}

//! \}
