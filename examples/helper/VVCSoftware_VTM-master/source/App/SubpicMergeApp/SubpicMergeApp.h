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

 /** \file     SubpicMergeApp.h
     \brief    Subpicture merge application header file
 */

#include <vector>
#include <fstream>


 //! \ingroup SubpicMergeApp
 //! \{



struct SubpicParams {
  int                                  width;
  int                                  height;
  int                                  topLeftCornerX;
  int                                  topLeftCornerY;
  std::ifstream                        fp;
};


struct Subpicture;
class InputByteStream;
class HLSyntaxReader;
class SEIReader;
class SEI;
class DCI;
class ParameterSetManager;
class PicHeader;
class InputNALUnit;
class Slice;
class OutputBitstream;
class VPS;
class SPS;
class PPS;
struct OutputNALUnit;
class AccessUnit;


class SubpicMergeApp
{
public:
  SubpicMergeApp(std::vector<SubpicParams> &subpicParams, std::ofstream &outputStream);
  ~SubpicMergeApp();

  void mergeStreams(bool mixedNaluFlag);
  void mergeYuvFiles(int bitdepth, int chromaFormat);

private:
  std::vector<Subpicture> *m_subpics;
  std::ofstream &m_outputStream;
  int m_prevPicPOC;
  int m_picWidth;
  int m_picHeight;

  void getOutputPicSize();
  bool isNewPicture(std::ifstream *bitstreamFile, InputByteStream *bytestream, bool firstSliceInPicture);
  bool parseDCI(HLSyntaxReader &hlsReader, DCI &dci);
  int parseVPS(HLSyntaxReader &hlsReader, ParameterSetManager &psManager);
  int parseSPS(HLSyntaxReader &hlsReader, ParameterSetManager &psManager);
  int parsePPS(HLSyntaxReader &hlsReader, ParameterSetManager &psManager);
  void parseAPS(HLSyntaxReader &hlsReader, ParameterSetManager &psManager, int &apsId, int &apsType);
  void parseSEI(SEIReader& seiReader, InputNALUnit &nalu, const VPS *vps, const SPS *sps, SEI *&decodePictureHashSei);
  void parsePictureHeader(HLSyntaxReader &hlsReader, PicHeader &picHeader, ParameterSetManager &psManager);
  void parseSliceHeader(HLSyntaxReader &hlsReader, InputNALUnit &nalu, Slice &slice, PicHeader &picHeader, OutputBitstream &sliceData, ParameterSetManager &psManager, int prevTid0Poc);
  void decodeNalu(Subpicture &subpic, InputNALUnit &nalu, SEI *&decodePictureHashSei);
  void parseSubpic(Subpicture &subpic, bool &morePictures);
  void generateMergedStreamVPSes(std::vector<VPS*> &vpsList);
  void generateMergedStreamSPSes(std::vector<SPS*> &spsList);
  void getTileDimensions(std::vector<int> &tileWidths, std::vector<int> &tileHeights);
  void generateMergedStreamPPSes(ParameterSetManager &psManager, std::vector<PPS*> &ppsList);
  void updateSliceHeadersForMergedStream(ParameterSetManager &psManager);
  void copyInputNaluToOutputNalu(OutputNALUnit &outNalu, InputNALUnit &inNalu);
  void copyNalUnitsToAccessUnit(AccessUnit &accessUnit, std::vector<InputNALUnit> &nalus, int naluType);
  bool getMixedNalPicFlag();
  Subpicture &selectSubpicForPicHeader(bool isMixedNaluPic);
  void generateMergedPic(ParameterSetManager &psManager, bool mixedNaluFlag);
  void validateSubpics();
};


//! \}
