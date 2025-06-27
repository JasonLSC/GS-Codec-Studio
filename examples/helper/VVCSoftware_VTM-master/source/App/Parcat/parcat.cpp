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

#include <stdint.h>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include "CommonLib/CommonDef.h"
#include "DecoderLib/NALread.h"
#include "VLCReader.h"
#if ENABLE_TRACING
#include "CommonLib/dtrace_next.h"
#endif

#define PRINT_NALUS 1

class ParcatHLSyntaxReader : public VLCReader
{
  public:
    void  parsePictureHeaderUpToPoc ( ParameterSetManager *parameterSetManager );
    bool  parsePictureHeaderInSliceHeaderFlag ( ParameterSetManager *parameterSetManager );
};

bool ParcatHLSyntaxReader::parsePictureHeaderInSliceHeaderFlag(ParameterSetManager *parameterSetManager) {


  uint32_t  uiCode;
  xReadFlag(uiCode, "sh_picture_header_in_slice_header_flag");
  return (uiCode==1);
}

void ParcatHLSyntaxReader::parsePictureHeaderUpToPoc ( ParameterSetManager *parameterSetManager )
{
  uint32_t  uiCode;

  PPS *pps = nullptr;
  SPS *sps = nullptr;

  uint32_t uiTmp;
  xReadFlag(uiTmp, "ph_gdr_or_irap_pic_flag");
  xReadFlag(uiCode, "ph_non_ref_pic_flag");
  if( uiTmp )
  {
    xReadFlag( uiCode, "ph_gdr_pic_flag" );
  }
  xReadFlag(uiCode, "ph_inter_slice_allowed_flag");
  if (uiCode)
  {
    xReadFlag(uiCode, "ph_intra_slice_allowed_flag");
  }
  // parameter sets
  xReadUvlc(uiCode, "ph_pic_parameter_set_id");
  pps = parameterSetManager->getPPS(uiCode);
  CHECK(pps == 0, "Invalid PPS");
  sps = parameterSetManager->getSPS(pps->getSPSId());
  CHECK(sps == 0, "Invalid SPS");
  return;
}

/**
 Find the beginning and end of a NAL (Network Abstraction Layer) unit in a byte buffer containing H264 bitstream data.
 @param[in]   buf        the buffer
 @param[in]   size       the size of the buffer
 @param[out]  nal_start  the beginning offset of the nal
 @param[out]  nal_end    the end offset of the nal
 @return                 the length of the nal, or 0 if did not find start of nal, or -1 if did not find end of nal
 */
// DEPRECATED - this will be replaced by a similar function with a slightly different API
int find_nal_unit(const uint8_t* buf, int size, int* nal_start, int* nal_end)
{
  int i;
  // find start
  *nal_start = 0;
  *nal_end = 0;

  i = 0;
  while (   //( next_bits( 24 ) != 0x000001 && next_bits( 32 ) != 0x00000001 )
    (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) &&
    (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0 || buf[i+3] != 0x01)
    )
  {
    i++; // skip leading zero
    if (i+4 >= size) { return 0; } // did not find nal start
  }

  if  (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) // ( next_bits( 24 ) != 0x000001 )
  {
    i++;
  }

  if  (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) { /* error, should never happen */ return 0; }
  i+= 3;
  *nal_start = i;

  while (//( next_bits( 24 ) != 0x000000 && next_bits( 24 ) != 0x000001 )
    i+3 < size &&
    (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0) &&
    (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01)
    )
  {
    i++;
    // FIXME the next line fails when reading a nal that ends exactly at the end of the data
  }

  if (i+3 == size)
  {
    *nal_end = size;
  }
  else
  {
    *nal_end = i;
  }

  return (*nal_end - *nal_start);
}

const bool verbose = false;

const char * NALU_TYPE[] =
{
    "NAL_UNIT_CODED_SLICE_TRAIL",
    "NAL_UNIT_CODED_SLICE_STSA",
    "NAL_UNIT_CODED_SLICE_RADL",
    "NAL_UNIT_CODED_SLICE_RASL",
    "NAL_UNIT_RESERVED_VCL_4",
    "NAL_UNIT_RESERVED_VCL_5",
    "NAL_UNIT_RESERVED_VCL_6",
    "NAL_UNIT_CODED_SLICE_IDR_W_RADL",
    "NAL_UNIT_CODED_SLICE_IDR_N_LP",
    "NAL_UNIT_CODED_SLICE_CRA",
    "NAL_UNIT_CODED_SLICE_GDR",
    "NAL_UNIT_RESERVED_IRAP_VCL11",
    "NAL_UNIT_RESERVED_IRAP_VCL12",
    "NAL_UNIT_DPS",
    "NAL_UNIT_VPS",
    "NAL_UNIT_SPS",
    "NAL_UNIT_PPS",
    "NAL_UNIT_PREFIX_APS",
    "NAL_UNIT_SUFFIX_APS",
    "NAL_UNIT_PH",
    "NAL_UNIT_ACCESS_UNIT_DELIMITER",
    "NAL_UNIT_EOS",
    "NAL_UNIT_EOB",
    "NAL_UNIT_PREFIX_SEI",
    "NAL_UNIT_SUFFIX_SEI",
    "NAL_UNIT_FD",
    "NAL_UNIT_RESERVED_NVCL26",
    "NAL_UNIT_RESERVED_NVCL27",
    "NAL_UNIT_UNSPECIFIED_28",
    "NAL_UNIT_UNSPECIFIED_29",
    "NAL_UNIT_UNSPECIFIED_30",
    "NAL_UNIT_UNSPECIFIED_31"
};

int calcPoc(int pocLsb, int prevTid0POC, int getBitsForPOC, int nalu_type)
{
  int prevPoc    = prevTid0POC;
  int maxPocLsb  = 1 << getBitsForPOC;
  int prevPocLsb = prevPoc & (maxPocLsb - 1);
  int prevPocMsb = prevPoc - prevPocLsb;
  int pocMsb;
  if ((pocLsb < prevPocLsb) && ((prevPocLsb - pocLsb) >= (maxPocLsb / 2)))
  {
    pocMsb = prevPocMsb + maxPocLsb;
  }
  else if ((pocLsb > prevPocLsb) && ((pocLsb - prevPocLsb) > (maxPocLsb / 2)))
  {
    pocMsb = prevPocMsb - maxPocLsb;
  }
  else
  {
    pocMsb = prevPocMsb;
  }

  return pocMsb + pocLsb;
}

std::vector<uint8_t> filter_segment(const std::vector<uint8_t> & v, int idx, int * poc_base, int * last_idr_poc)
{
  const uint8_t * p = v.data();
  const uint8_t * buf = v.data();
  int sz = (int) v.size();
  int nal_start, nal_end;
  int off = 0;
  int cnt[MAX_VPS_LAYERS] = { 0 };
  bool idr_found[MAX_VPS_LAYERS] = { false };
  bool is_pre_sei_before_idr = true;

  std::vector<uint8_t> out;
  out.reserve(v.size());

  int  bitsForPoc                   = 8;
  bool skip_next_sei = false;
  bool change_poc = false;
  bool first_idr_slice_after_ph_nal = false;

  while(find_nal_unit(p, sz, &nal_start, &nal_end) > 0)
  {
    if(verbose)
    {
       printf( "!! Found NAL at offset %lld (0x%04llX), size %lld (0x%04llX) \n",
          (long long int)(off + (p - buf)),
          (long long int)(off + (p - buf)),
          (long long int)(nal_end - nal_start),
          (long long int)(nal_end - nal_start) );
    }

    p += nal_start;

    std::vector<uint8_t> nalu(p, p + nal_end - nal_start);
    int nalu_type = nalu[1] >> 3;
#if ENABLE_TRACING
    printf ("NALU Type: %d (%s)\n", nalu_type, NALU_TYPE[nalu_type]);
#endif
    int poc = -1;
    int pocLsb = -1;
    int newPoc = -1;

    HLSyntaxReader HLSReader;
    static ParameterSetManager parameterSetManager;
    ParcatHLSyntaxReader parcatHLSReader;
    InputNALUnit inp_nalu;
    std::vector<uint8_t> & nalu_bs = inp_nalu.getBitstream().getFifo();
    nalu_bs = nalu;
    read(inp_nalu);

    if( inp_nalu.m_nalUnitType == NAL_UNIT_SPS )
    {
      SPS* sps = new SPS();
      HLSReader.setBitstream( &inp_nalu.getBitstream() );
      HLSReader.parseSPS( sps );
      parameterSetManager.storeSPS( sps, inp_nalu.getBitstream().getFifo() );
    }

    if( inp_nalu.m_nalUnitType == NAL_UNIT_PPS )
    {
      PPS* pps = new PPS();
      HLSReader.setBitstream( &inp_nalu.getBitstream() );
      HLSReader.parsePPS( pps );
      parameterSetManager.storePPS( pps, inp_nalu.getBitstream().getFifo() );
    }
    int nalu_layerId = nalu[0] & 0x3F;

    if (nalu_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || nalu_type == NAL_UNIT_CODED_SLICE_IDR_N_LP)
    {
      is_pre_sei_before_idr = false;
    }
    if(nalu_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || nalu_type == NAL_UNIT_CODED_SLICE_IDR_N_LP)
    {
      poc = 0;
      newPoc = *poc_base + poc;
      if (first_idr_slice_after_ph_nal)
      {
        cnt[nalu_layerId]--;
      }
      first_idr_slice_after_ph_nal = false;
    }
    if(inp_nalu.m_nalUnitType == NAL_UNIT_PH || (nalu_type < NAL_UNIT_CODED_SLICE_IDR_W_RADL) || (nalu_type > NAL_UNIT_CODED_SLICE_IDR_N_LP && nalu_type <= NAL_UNIT_RESERVED_IRAP_VCL_11) )
    {
      parcatHLSReader.setBitstream( &inp_nalu.getBitstream() );
      if (inp_nalu.m_nalUnitType == NAL_UNIT_PH)
      {
        change_poc = true;
        first_idr_slice_after_ph_nal = true;
      }
      else
      {
        change_poc = parcatHLSReader.parsePictureHeaderInSliceHeaderFlag(&parameterSetManager);
      }
      if (change_poc)
      {
        // beginning of picture header parsing
        parcatHLSReader.parsePictureHeaderUpToPoc(&parameterSetManager);
        int num_bits_up_to_poc_lsb = parcatHLSReader.getBitstream()->getNumBitsRead();
        int offset = num_bits_up_to_poc_lsb;

        int      byteOffset = offset / 8;
        int      hiBits     = offset % 8;
        uint16_t data       = (nalu[byteOffset] << 8) | nalu[byteOffset + 1];
        int      lowBits    = 16 - hiBits - bitsForPoc;
        pocLsb              = (data >> lowBits) & 0xff;
        poc                 = pocLsb;   // calcPoc(pocLsb, 0, bitsForPoc, nalu_type);

        newPoc                  = poc + *poc_base;
        // int picOrderCntLSB = (pcSlice->getPOC()-pcSlice->getLastIDR()+(1<<pcSlice->getSPS()->getBitsForPOC())) & ((1<<pcSlice->getSPS()->getBitsForPOC())-1);
        unsigned picOrderCntLSB = (newPoc - *last_idr_poc + (1 << bitsForPoc)) & ((1 << bitsForPoc) - 1);

        int low = data & ((1 << lowBits) - 1);
        int hi  = data >> (16 - hiBits);
        data    = (hi << (16 - hiBits)) | (picOrderCntLSB << lowBits) | low;

        nalu[byteOffset]     = data >> 8;
        nalu[byteOffset + 1] = data & 0xff;

#if ENABLE_TRACING
        std::cout << "Changed poc " << poc << " to " << newPoc << std::endl;
#endif
        ++cnt[nalu_layerId];
        change_poc = false;
      }
    }

    if(idx > 1 && (nalu_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || nalu_type == NAL_UNIT_CODED_SLICE_IDR_N_LP))
    {
      skip_next_sei = true;
      idr_found[nalu_layerId] = true;
    }
    if ((idx > 1 && (nalu_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || nalu_type == NAL_UNIT_CODED_SLICE_IDR_N_LP))
      || ((idx > 1 && !idr_found[nalu_layerId]) && (nalu_type == NAL_UNIT_OPI || nalu_type == NAL_UNIT_DCI || nalu_type == NAL_UNIT_VPS || nalu_type == NAL_UNIT_SPS || nalu_type == NAL_UNIT_PPS || nalu_type == NAL_UNIT_PREFIX_APS || nalu_type == NAL_UNIT_SUFFIX_APS || nalu_type == NAL_UNIT_PH || nalu_type == NAL_UNIT_ACCESS_UNIT_DELIMITER))
      || (nalu_type == NAL_UNIT_SUFFIX_SEI && skip_next_sei)
      || (idx > 1 && nalu_type == NAL_UNIT_PREFIX_SEI && is_pre_sei_before_idr))
    {
    }
    else
    {
      out.insert(out.end(), p - nal_start, p);
      out.insert(out.end(), nalu.begin(), nalu.end());
    }

    if(nalu_type == NAL_UNIT_SUFFIX_SEI && skip_next_sei)
    {
      skip_next_sei = false;
    }


    p += (nal_end - nal_start);
    sz -= nal_end;
  }

  *poc_base += *std::max_element(std::begin(cnt), std::end(cnt));
  return out;
}

std::vector<uint8_t> process_segment(const char * path, int idx, int * poc_base, int * last_idr_poc)
{
  FILE * fdi = fopen(path, "rb");

  if (fdi == nullptr)
  {
    fprintf(stderr, "Error: could not open input file: %s", path);
    exit(1);
  }

  fseek(fdi, 0, SEEK_END);
  long full_sz = ftell(fdi);
  fseek(fdi, 0, SEEK_SET);

  std::vector<uint8_t> v(full_sz);

  size_t sz = fread((char*) v.data(), 1, full_sz, fdi);
  fclose(fdi);

  if(sz != full_sz)
  {
    fprintf(stderr, "Error: input file was not read completely.");
    exit(1);
  }

  return filter_segment(v, idx, poc_base, last_idr_poc);
}

int main(int argc, char * argv[])
{
#if ENABLE_TRACING
  std::string tracingFile;
  std::string tracingRule;

  g_trace_ctx = tracing_init(tracingFile, tracingRule);
#endif
  if(argc < 3)
  {
    printf("parcat version VTM %s\n", VTM_VERSION);
    printf("usage: %s <bitstream1> [<bitstream2> ...] <outfile>\n", argv[0]);
    return -1;
  }

  FILE * fdo = fopen(argv[argc - 1], "wb");
  if (fdo == nullptr)
  {
    fprintf(stderr, "Error: could not open output file: %s", argv[argc - 1]);
    exit(1);
  }
  int poc_base = 0;
  int last_idr_poc = 0;

  initROM();

  for(int i = 1; i < argc - 1; ++i)
  {
    std::vector<uint8_t> v = process_segment(argv[i], i, &poc_base, &last_idr_poc);

    fwrite(v.data(), 1, v.size(), fdo);
  }

  fclose(fdo);
#if ENABLE_TRACING
  tracing_uninit(g_trace_ctx);
#endif
}
