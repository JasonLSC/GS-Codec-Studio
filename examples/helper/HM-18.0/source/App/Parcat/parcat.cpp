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

#include <stdint.h>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cassert>

#define PRINT_NALUS 0

enum NalUnitType
{
  TRAIL_N = 0, // 0
  TRAIL_R,     // 1

  TSA_N,       // 2
  TSA_R,       // 3

  STSA_N,      // 4
  STSA_R,      // 5

  RADL_N,      // 6
  RADL_R,      // 7

  RASL_N,      // 8
  RASL_R,      // 9

  RESERVED_VCL_N10,
  RESERVED_VCL_R11,
  RESERVED_VCL_N12,
  RESERVED_VCL_R13,
  RESERVED_VCL_N14,
  RESERVED_VCL_R15,

  BLA_W_LP,    // 16
  BLA_W_RADL,  // 17
  BLA_N_LP,    // 18
  IDR_W_RADL,  // 19
  IDR_N_LP,    // 20
  CRA,         // 21
  RESERVED_IRAP_VCL22,
  RESERVED_IRAP_VCL23,

  RESERVED_VCL24,
  RESERVED_VCL25,
  RESERVED_VCL26,
  RESERVED_VCL27,
  RESERVED_VCL28,
  RESERVED_VCL29,
  RESERVED_VCL30,
  RESERVED_VCL31,

  VPS,                     // 32
  SPS,                     // 33
  PPS,                     // 34
  ACCESS_UNIT_DELIMITER,   // 35
  EOS,                     // 36
  EOB,                     // 37
  FILLER_DATA,             // 38
  PREFIX_SEI,              // 39
  SUFFIX_SEI,              // 40

  RESERVED_NVCL41,
  RESERVED_NVCL42,
  RESERVED_NVCL43,
  RESERVED_NVCL44,
  RESERVED_NVCL45,
  RESERVED_NVCL46,
  RESERVED_NVCL47,
  UNSPECIFIED_48,
  UNSPECIFIED_49,
  UNSPECIFIED_50,
  UNSPECIFIED_51,
  UNSPECIFIED_52,
  UNSPECIFIED_53,
  UNSPECIFIED_54,
  UNSPECIFIED_55,
  UNSPECIFIED_56,
  UNSPECIFIED_57,
  UNSPECIFIED_58,
  UNSPECIFIED_59,
  UNSPECIFIED_60,
  UNSPECIFIED_61,
  UNSPECIFIED_62,
  UNSPECIFIED_63,
  INVALID,
};

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
    "TRAIL_N",
    "TRAIL_R",
    "TSA_N",
    "TSA_R",
    "STSA_N",
    "STSA_R",
    "RADL_N",
    "RADL_R",
    "RASL_N",
    "RASL_R",
    "RSV_VCL_N10",
    "RSV_VCL_N12",
    "RSV_VCL_N14",
    "RSV_VCL_R11",
    "RSV_VCL_R13",
    "RSV_VCL_R15",
    "BLA_W_LP",
    "BLA_W_RADL",
    "BLA_N_LP",
    "IDR_W_RADL",
    "IDR_N_LP",
    "CRA_NUT",
    "RSV_IRAP_VCL22",
    "RSV_IRAP_VCL23",
    "unk",
    "unk",
    "unk",
    "unk",
    "unk",
    "unk",
    "unk",
    "unk",
    "VPS_NUT",
    "SPS_NUT",
    "PPS_NUT",
    "AUD_NUT",
    "EOS_NUT",
    "EOB_NUT",
    "FD_NUT",
    "PREFIX_SEI_NUT",
    "SUFFIX_SEI_NUT",
};

int calc_poc(int iPOClsb, int prevTid0POC, int getBitsForPOC, int nalu_type)
{
  int iPrevPOC = prevTid0POC;
  int iMaxPOClsb = 1<< getBitsForPOC;
  int iPrevPOClsb = iPrevPOC & (iMaxPOClsb - 1);
  int iPrevPOCmsb = iPrevPOC-iPrevPOClsb;
  int iPOCmsb;
  if( ( iPOClsb  <  iPrevPOClsb ) && ( ( iPrevPOClsb - iPOClsb )  >=  ( iMaxPOClsb / 2 ) ) )
  {
    iPOCmsb = iPrevPOCmsb + iMaxPOClsb;
  }
  else if( (iPOClsb  >  iPrevPOClsb )  && ( (iPOClsb - iPrevPOClsb )  >  ( iMaxPOClsb / 2 ) ) )
  {
    iPOCmsb = iPrevPOCmsb - iMaxPOClsb;
  }
  else
  {
    iPOCmsb = iPrevPOCmsb;
  }
  if ( nalu_type == BLA_W_LP
    || nalu_type == BLA_W_RADL
    || nalu_type == BLA_N_LP )
  {
    // For BLA picture types, POCmsb is set to 0.
    iPOCmsb = 0;
  }

  return iPOCmsb + iPOClsb;
}

std::vector<uint8_t> filter_segment(const std::vector<uint8_t> & v, int idx, int * poc_base, int * last_idr_poc)
{
  const uint8_t * p = v.data();
  const uint8_t * buf = v.data();
  int sz = (int) v.size();
  int nal_start, nal_end;
  int off = 0;
  int cnt = 0;
  bool idr_found = false;

  std::vector<uint8_t> out;
  out.reserve(v.size());

  int bits_for_poc = 8;
  bool skip_next_sei = false;

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
    int nalu_type = nalu[0] >> 1;
    int poc = -1;
    int poc_lsb = -1;
    int new_poc = -1;

    if(nalu_type == IDR_W_RADL || nalu_type == IDR_N_LP)
    {
      poc = 0;
      new_poc = *poc_base + poc;
    }

    if(nalu_type < 32 && nalu_type != IDR_W_RADL && nalu_type != IDR_N_LP)
    {
      int offset = 16;

      offset += 1; //first_slice_segment_in_pic_flag
      if (nalu_type >= BLA_W_LP && nalu_type <= RESERVED_IRAP_VCL23)
      {
        offset += 1; //no_output_of_prior_pics_flag
      }

      // determine offset for slice_pic_parameter_set_id TODO: ue(v)
      int byte_offset2 = offset / 8;
      int hi_bits2 = offset % 8;
      uint16_t data2 = (nalu[byte_offset2] << 8) | nalu[byte_offset2 + 1];
      int low_bits2 = 16 - hi_bits2 - 1;      
      if(((data2 >> low_bits2) % 2))
        offset += 1; // PPSId=0 
      else
        offset += 3; // PPSId=1 
      offset += 1; // slice_type TODO: ue(v)
      // separate_colour_plane_flag is not supported in JEM1.0
      if (nalu_type == CRA)
      {
        offset += 2;
      }
      int byte_offset = offset / 8;
      int hi_bits = offset % 8;
      uint16_t data = (nalu[byte_offset] << 8) | nalu[byte_offset + 1];
      int low_bits = 16 - hi_bits - bits_for_poc;
      poc_lsb = (data >> low_bits) & 0xff;
      poc = poc_lsb; //calc_poc(poc_lsb, 0, bits_for_poc, nalu_type);

      new_poc = poc + *poc_base;
      // int picOrderCntLSB = (pcSlice->getPOC()-pcSlice->getLastIDR()+(1<<pcSlice->getSPS()->getBitsForPOC())) & ((1<<pcSlice->getSPS()->getBitsForPOC())-1);
      unsigned picOrderCntLSB = (new_poc - *last_idr_poc +(1 << bits_for_poc)) & ((1<<bits_for_poc)-1);

      int low = data & ((1 << (low_bits + 1)) - 1);
      int hi = data >> (16 - hi_bits);
      data = (hi << (16 - hi_bits)) | (picOrderCntLSB << low_bits) | low;

      nalu[byte_offset] = data >> 8;
      nalu[byte_offset + 1] = data & 0xff;

      ++cnt;
    }

    if(idx > 1 && (nalu_type == IDR_W_RADL || nalu_type == IDR_N_LP))
    {
      skip_next_sei = true;
      idr_found = true;
    }

    if((idx > 1 && (nalu_type == IDR_W_RADL || nalu_type == IDR_N_LP )) || ((idx>1 && !idr_found) && ( nalu_type == VPS || nalu_type == SPS || nalu_type == PPS))
      || (nalu_type == SUFFIX_SEI && skip_next_sei))
    {
    }
    else
    {
      out.insert(out.end(), p - nal_start, p);
      out.insert(out.end(), nalu.begin(), nalu.end());
    }

    if(nalu_type == SUFFIX_SEI && skip_next_sei)
    {
      skip_next_sei = false;
    }


    p += (nal_end - nal_start);
    sz -= nal_end;
  }

  *poc_base += cnt;
  return out;
}

std::vector<uint8_t> process_segment(const char * path, int idx, int * poc_base, int * last_idr_poc)
{
  FILE * fdi = fopen(path, "rb");

  if (fdi == NULL)
  {
    fprintf(stderr, "Error: could not open input file: %s", path);
    exit(1);
  }

  fseek(fdi, 0, SEEK_END);
  size_t full_sz = ftell(fdi);
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
  if(argc < 3)
  {
    printf("usage: %s <bitstream1> [<bitstream2> ...] <outfile>\n", argv[0]);
    return -1;
  }

  FILE * fdo = fopen(argv[argc - 1], "wb");
  if (fdo==NULL)
  {
    fprintf(stderr, "Error: could not open output file: %s", argv[argc - 1]);
    exit(1);
  }
  int poc_base = 0;
  int last_idr_poc = 0;

  for(int i = 1; i < argc - 1; ++i)
  {
    std::vector<uint8_t> v = process_segment(argv[i], i, &poc_base, &last_idr_poc);

    fwrite(v.data(), 1, v.size(), fdo);
  }

  fclose(fdo);
}
