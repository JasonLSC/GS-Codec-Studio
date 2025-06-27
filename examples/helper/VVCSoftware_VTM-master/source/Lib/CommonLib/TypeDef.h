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

/** \file     TypeDef.h
    \brief    Define macros, basic types, new types and enumerations
*/

#ifndef __TYPEDEF__
#define __TYPEDEF__

#ifndef __COMMONDEF__
#error Include CommonDef.h not TypeDef.h
#endif

#include <array>
#include <vector>
#include <utility>
#include <sstream>
#include <cstddef>
#include <cstring>
#include <assert.h>
#include <cassert>
#include "CommonDef.h"

// clang-format off

//########### place macros to be removed in next cycle below this line ###############

#define JVET_AL0067_OMI_SEI_CONSTRAINTS                   1 // JVET-AL0067: On the OMI SEI

#define JVET_AL0324_AL0070_PRI_SEI                        1 // JVET-AL0324 & JVET-AL0070 miscellaneous changes in PRI SEI message

#define JVET_AL0301_MATRIXCOEFFS_CONSTRAINTS              1 // JVET_AL0301: Add constraints to matrix_coeffs

#define JVET_AL0123_AL0310_EOI                            1 // JVET-AL0123 & JVET-AL0310 EOI SEI signalling changes 

#define JVET_AJ0183_EOI_SEI_SRC_PIC_FLAG                  1 // JVET-AJ0183: Add source picture flag to EOI SEI message

#define JVET_AJ0105_SPO_COMPLEXITY_INFO                   1 // JVET-AJ0105: Add signalling about complexity information in SPO SEI message

#define JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI        1

#define JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG           1 // Signal flags for substreams to indicate substream dependency

#define JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION          1 // dsci_vss_implicit_association_mode_flag

#define JVET_AL0339_FGS_SEI_SPATIAL_RESOLUTION            1 // add fg_pic_width_in_luma_samples and fg_pic_height_in_luma_samples to the extension of FGC SEI

#define JVET_AK0326_NNPF_SEED                             1 // JVET-AK0326: seed value for NNPFC and NNPFA SEI messages

#define JVET_AI0070_BYTE_ALIGNMENT                        1 // JVET-AI0070: byte alignment in NNPFC SEI message

#define JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI             1 // Implementation of AI-usage restrictions SEI message

#define JVET_AL0056_EOI_SEI_QUANT_CONSTRAINT              1 // JVET-AL0056: On the encoder optimization information SEI message

//########### place macros to be be kept below this line ###############

#define JVET_AJ0104_NNPFA_PROMPT_UPDATE   1
#define JVET_AJ0114_NNPFA_NUM_PIC_SHIFT   1
#if JVET_AJ0114_NNPFA_NUM_PIC_SHIFT
#define JVET_AL0075_NNPFA_SELECTED_INPUT_FLAG   1
#endif

#ifndef JVET_AJ0151_DSC_SEI
#define JVET_AJ0151_DSC_SEI   1                      // Digitally signed content signing and verification (requires OpenSSL v3)
#endif

#define GDR_ENABLED   1

#if GDR_ENABLED
#define GDR_LEAK_TEST  0
#define GDR_ENC_TRACE  0
#define GDR_DEC_TRACE  0
#endif

#define JVET_S0257_DUMP_360SEI_MESSAGE                    1 // Software support of 360 SEI messages

#define NNPFC_SCAN_TYPE_IDC                               1 // nnpfc_scan_type_idc from JVET-AH0174 and KVET-AJ0073

#define JVET_R0164_MEAN_SCALED_SATD                       1 // JVET-R0164: Use a mean scaled version of SATD in encoder decisions

#define JVET_M0497_MATRIX_MULT                            0 // 0: Fast method; 1: Matrix multiplication

#define APPLY_SBT_SL_ON_MTS                               1 // apply save & load fast algorithm on inter MTS when SBT is on

#define REUSE_CU_RESULTS                                  1
#if REUSE_CU_RESULTS
#define REUSE_CU_RESULTS_WITH_MULTIPLE_TUS                1
#endif

#ifndef JVET_J0090_MEMORY_BANDWITH_MEASURE
#define JVET_J0090_MEMORY_BANDWITH_MEASURE                0
#endif

#ifndef EXTENSION_360_VIDEO
#define EXTENSION_360_VIDEO                               0   ///< extension for 360/spherical video coding support; this macro should be controlled by makefile, as it would be used to control whether the library is built and linked
#endif

#ifndef EXTENSION_HDRTOOLS
#define EXTENSION_HDRTOOLS                                0 //< extension for HDRTools/Metrics support; this macro should be controlled by makefile, as it would be used to control whether the library is built and linked
#endif

#define JVET_O0756_CONFIG_HDRMETRICS                      1
#if EXTENSION_HDRTOOLS
#define JVET_O0756_CALCULATE_HDRMETRICS                   1
#endif

#define DISABLE_PRE_POST_FILTER_FOR_IDR_CRA               1
#define ENABLE_USER_DEFINED_WEIGHTS                       0 // User can specify weights for both current and previous picture, such that their sum = 1


// clang-format on

// ====================================================================================================================
// General settings
// ====================================================================================================================

#ifndef ENABLE_TRACING
#define ENABLE_TRACING                                    0 // DISABLE by default (enable only when debugging, requires 15% run-time in decoding) -- see documentation in 'doc/DTrace for NextSoftware.pdf'
#endif

#if ENABLE_TRACING
#define K0149_BLOCK_STATISTICS                            1 // enables block statistics, which can be analysed with YUView (https://github.com/IENT/YUView)
#if K0149_BLOCK_STATISTICS
#define BLOCK_STATS_AS_CSV                                0 // statistics will be written in a comma separated value format. this is not supported by YUView
#endif
#endif

#define WCG_EXT                                           1
#define WCG_WPSNR                                         WCG_EXT

#define KEEP_PRED_AND_RESI_SIGNALS                        0

#ifndef GREEN_METADATA_SEI_ENABLED
#define GREEN_METADATA_SEI_ENABLED 0 //JVET-AB0072: Analyser for the Green Metadata SEI
#endif

#ifndef GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464
#if GREEN_METADATA_SEI_ENABLED
#define GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464 0   // WG03_N01464: Attenuation Maps in Green Metadata SEI
#else
#define GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464 0   // DO NOT CHANGE - SHOULD ALWAYS BE 0 HERE
#endif 
#endif

#if (GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464) && (!GREEN_METADATA_SEI_ENABLED)
#error ERROR: cannot enable GREEN_METADATA_SEI_AMI_ENABLED_WG03_N01464 without GREEN_METADATA_SEI_ENABLED
#endif

// ====================================================================================================================
// Debugging
// ====================================================================================================================

// most debugging tools are now bundled within the ENABLE_TRACING macro -- see documentation to see how to use

#define PRINT_MACRO_VALUES                                1 ///< When enabled, the encoder prints out a list of the non-environment-variable controlled macros and their values on startup


// TODO: rename this macro to DECODER_DEBUG_BIT_STATISTICS (may currently cause merge issues with other branches)
// This can be enabled by the makefile
#ifndef RExt__DECODER_DEBUG_BIT_STATISTICS
#define RExt__DECODER_DEBUG_BIT_STATISTICS                0 ///< 0 (default) = decoder reports as normal, 1 = decoder produces bit usage statistics (will impact decoder run time by up to ~10%)
#endif

#ifndef RExt__DECODER_DEBUG_TOOL_MAX_FRAME_STATS
#define RExt__DECODER_DEBUG_TOOL_MAX_FRAME_STATS         (1 && RExt__DECODER_DEBUG_BIT_STATISTICS )   ///< 0 (default) = decoder reports as normal, 1 = decoder produces max frame bit usage statistics
#endif

#define TR_ONLY_COEFF_STATS                              (1 && RExt__DECODER_DEBUG_BIT_STATISTICS )   ///< 0 combine TS and non-TS decoder debug statistics. 1 = separate TS and non-TS decoder debug statistics.
#define EPBINCOUNT_FIX                                   (1 && RExt__DECODER_DEBUG_BIT_STATISTICS )   ///< 0 use count to represent number of calls to decodeBins. 1 = count and bins for EP bins are the same.

#ifndef RExt__DECODER_DEBUG_TOOL_STATISTICS
#define RExt__DECODER_DEBUG_TOOL_STATISTICS               0 ///< 0 (default) = decoder reports as normal, 1 = decoder produces tool usage statistics
#endif

#if RExt__DECODER_DEBUG_BIT_STATISTICS || RExt__DECODER_DEBUG_TOOL_STATISTICS
#define RExt__DECODER_DEBUG_STATISTICS                    1
#endif

// ====================================================================================================================
// Tool Switches - transitory (these macros are likely to be removed in future revisions)
// ====================================================================================================================

#define DECODER_CHECK_SUBSTREAM_AND_SLICE_TRAILING_BYTES  1 ///< TODO: integrate this macro into a broader conformance checking system.
                                                            // To use this capability enable config parameter LambdaFromQpEnable

// ====================================================================================================================
// Tool Switches
// ====================================================================================================================


// This can be enabled by the makefile
#ifndef RExt__HIGH_BIT_DEPTH_SUPPORT
#define RExt__HIGH_BIT_DEPTH_SUPPORT                      0 ///< 0 (default) use data type definitions for 8-10 bit video, 1 = use larger data types to allow for up to 16-bit video (originally developed as part of N0188)
#endif

// SIMD optimizations
#define SIMD_ENABLE                                       1                                                 ///< Enable SIMD optimizations if available on compilation environment
#ifdef TARGET_SIMD_X86
#define ENABLE_SIMD_OPT                                   SIMD_ENABLE                                       ///< SIMD optimizations, no impact on RD performance
#else
#define ENABLE_SIMD_OPT                                   0                                                 ///< SIMD optimizations, no impact on RD performance
#endif
#define ENABLE_SIMD_OPT_MCIF                            ( 1 && ENABLE_SIMD_OPT )                            ///< SIMD optimization for the interpolation filter, no impact on RD performance
#define ENABLE_SIMD_OPT_BUFFER                          ( 1 && ENABLE_SIMD_OPT )                            ///< SIMD optimization for the buffer operations, no impact on RD performance
#define ENABLE_SIMD_OPT_DIST                            ( 1 && ENABLE_SIMD_OPT )                            ///< SIMD optimization for the distortion calculations(SAD,SSE,HADAMARD), no impact on RD performance
#define ENABLE_SIMD_OPT_AFFINE_ME                       ( 1 && ENABLE_SIMD_OPT )                            ///< SIMD optimization for affine ME, no impact on RD performance
#define ENABLE_SIMD_OPT_ALF                             ( 1 && ENABLE_SIMD_OPT )                            ///< SIMD optimization for ALF
#define ENABLE_SIMD_OPT_IBC                             ( 1 && ENABLE_SIMD_OPT )                            ///< SIMD optimization for IBC
#if ENABLE_SIMD_OPT_BUFFER
#define ENABLE_SIMD_OPT_BCW                               1                                                 ///< SIMD optimization for Bcw
#endif

// End of SIMD optimizations


#define RDOQ_CHROMA_LAMBDA                                1 ///< F386: weighting of chroma for RDOQ

#define W0038_CQP_ADJ                                     1 ///< chroma QP adjustment based on TL, CQPTLAdjustEnabled is set to 1;

#define SHARP_LUMA_DELTA_QP                               1 ///< include non-normative LCU deltaQP and normative chromaQP change
#define ER_CHROMA_QP_WCG_PPS                              1 ///< Chroma QP model for WCG used in Anchor 3.2
#define ENABLE_QPA                                        1 ///< Non-normative perceptual QP adaptation according to JVET-H0047 and JVET-K0206. Deactivated by default, activated using encoder arguments --PerceptQPA=1 --SliceChromaQPOffsetPeriodicity=1
#define ENABLE_QPA_SUB_CTU                              ( 1 && ENABLE_QPA ) ///< when maximum delta-QP depth is greater than zero, use sub-CTU QPA


#define RDOQ_CHROMA                                       1 ///< use of RDOQ in chroma


// ====================================================================================================================
// Derived macros
// ====================================================================================================================

#if RExt__HIGH_BIT_DEPTH_SUPPORT
#define FULL_NBIT                                         1 ///< When enabled, use distortion measure derived from all bits of source data, otherwise discard (bitDepth - 8) least-significant bits of distortion
#define RExt__HIGH_PRECISION_FORWARD_TRANSFORM            1 ///< 0 use original 6-bit transform matrices for both forward and inverse transform, 1 (default) = use original matrices for inverse transform and high precision matrices for forward transform
#define JVET_V0106_DEP_QUANT_ENC_OPT                      1 ///< 0 use original g_goRiceBits[4][32] LUT for codeword length estimation at encoder, 1 (default) use extended g_goRiceBits[16][64] LUT for codeword length estimation at encoder
#else
#define FULL_NBIT                                         1 ///< When enabled, use distortion measure derived from all bits of source data, otherwise discard (bitDepth - 8) least-significant bits of distortion
#define RExt__HIGH_PRECISION_FORWARD_TRANSFORM            0 ///< 0 (default) use original 6-bit transform matrices for both forward and inverse transform, 1 = use original matrices for inverse transform and high precision matrices for forward transform
#define JVET_V0106_DEP_QUANT_ENC_OPT                      0 ///< 0 (default) use original g_goRiceBits[4][32] LUT for codeword length estimation at encoder, 1 - use extended g_goRiceBits[16][64] LUT for codeword length estimation at encoder
#endif

#if FULL_NBIT
#define DISTORTION_PRECISION_ADJUSTMENT(x)                0
#else
#define DISTORTION_ESTIMATION_BITS                        8
#define DISTORTION_PRECISION_ADJUSTMENT(x)                ((x>DISTORTION_ESTIMATION_BITS)? ((x)-DISTORTION_ESTIMATION_BITS) : 0)
#endif

// ====================================================================================================================
// Error checks
// ====================================================================================================================

#if ((RExt__HIGH_PRECISION_FORWARD_TRANSFORM != 0) && (RExt__HIGH_BIT_DEPTH_SUPPORT == 0))
#error ERROR: cannot enable RExt__HIGH_PRECISION_FORWARD_TRANSFORM without RExt__HIGH_BIT_DEPTH_SUPPORT
#endif

// ====================================================================================================================
// Named numerical types
// ====================================================================================================================

#if RExt__HIGH_BIT_DEPTH_SUPPORT
typedef       int             Pel;               ///< pixel type
typedef       int64_t         TCoeff;            ///< transform coefficient
typedef       int             TMatrixCoeff;      ///< transform matrix coefficient
typedef       int16_t         TFilterCoeff;      ///< filter coefficient
typedef       int64_t         Intermediate_Int;  ///< used as intermediate value in calculations
typedef       uint64_t        Intermediate_UInt; ///< used as intermediate value in calculations
#else
typedef       int16_t         Pel;               ///< pixel type
typedef       int             TCoeff;            ///< transform coefficient
typedef       int16_t         TMatrixCoeff;      ///< transform matrix coefficient
typedef       int16_t         TFilterCoeff;      ///< filter coefficient
typedef       int             Intermediate_Int;  ///< used as intermediate value in calculations
typedef       uint32_t        Intermediate_UInt; ///< used as intermediate value in calculations
#endif

typedef       uint64_t        SplitSeries;       ///< used to encoded the splits that caused a particular CU size
typedef       uint64_t        ModeTypeSeries;    ///< used to encoded the ModeType at different split depth

typedef       uint64_t        Distortion;        ///< distortion measurement

using TileIdx = uint16_t;

// ====================================================================================================================
// Enumeration
// ====================================================================================================================

// casts enum to underlying integer type
template<typename E> constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept
{
  return static_cast<typename std::underlying_type<E>::type>(e);
}

// array indexed by an enum type
template<class T, class E> struct EnumArray : public std::array<T, to_underlying(E::NUM)>
{
  using base            = std::array<T, to_underlying(E::NUM)>;
  using size_type       = E;
  using reference       = T &;
  using const_reference = const T &;

public:
  constexpr EnumArray() : base() {}
  constexpr EnumArray(std::initializer_list<T> l)
  {
    size_t j = 0;
    for (const auto &i: l)
    {
      base::at(j++) = i;
    }
  }

  reference                 operator[](size_type e) { return base::operator[](to_underlying(e)); }
  constexpr const_reference operator[](size_type e) const { return base::operator[](to_underlying(e)); }
};

enum class ApsType : uint8_t
{
  ALF = 0,
  LMCS,
  SCALING_LIST,
  NUM
};

enum QuantFlags
{
  Q_INIT           = 0x0,
  Q_USE_RDOQ       = 0x1,
  Q_RDOQTS         = 0x2,
  Q_SELECTIVE_RDOQ = 0x4,
};

enum class TransType
{
  DCT2 = 0,
  DCT8,
  DST7,
  NUM
};

enum class MtsType : int8_t
{
  NONE      = -1,
  DCT2_DCT2 = 0,
  SKIP,
  DST7_DST7,
  DCT8_DST7,
  DST7_DCT8,
  DCT8_DCT8,
  NUM
};

static inline constexpr int operator-(const MtsType &a, const MtsType &b)
{
  return to_underlying(a) - to_underlying(b);
}

static inline constexpr MtsType operator+(const MtsType &a, int b)
{
  return static_cast<MtsType>(to_underlying(a) + b);
}

static inline MtsType operator++(MtsType &a, int)
{
  MtsType b = a;

  a = static_cast<MtsType>(to_underlying(a) + 1);

  return b;
}

typedef std::pair<MtsType, bool> TrMode;
typedef std::pair<int, int>      TrCost;

enum class ISPType : int8_t
{
  // Note: numbering starts at -1 (NONE) and NUM is equal to 2 (HOR and VER cases)
  // to_uint() defined below can be used to convert to the range 0 (for NONE) to 3 (for NUM)
  NONE = -1,
  HOR  = 0,
  VER,
  NUM,
  RESERVED
};

static inline uint32_t to_uint(ISPType t) { return to_underlying(t) - to_underlying(ISPType::NONE); }

enum SbtIdx
{
  SBT_OFF_DCT  = 0,
  SBT_VER_HALF = 1,
  SBT_HOR_HALF = 2,
  SBT_VER_QUAD = 3,
  SBT_HOR_QUAD = 4,
  NUMBER_SBT_IDX,
  SBT_OFF_MTS, //note: must be after all SBT modes, only used in fast algorithm to discern the best mode is inter EMT
};

enum SbtPos
{
  SBT_POS0 = 0,
  SBT_POS1 = 1,
  NUMBER_SBT_POS
};

enum SbtMode
{
  SBT_VER_H0 = 0,
  SBT_VER_H1 = 1,
  SBT_HOR_H0 = 2,
  SBT_HOR_H1 = 3,
  SBT_VER_Q0 = 4,
  SBT_VER_Q1 = 5,
  SBT_HOR_Q0 = 6,
  SBT_HOR_Q1 = 7,
  NUMBER_SBT_MODE
};

enum class BdpcmMode : uint8_t
{
  NONE = 0,
  HOR,
  VER
};

/// supported slice type
enum SliceType
{
  B_SLICE               = 0,
  P_SLICE               = 1,
  I_SLICE               = 2,
  NUMBER_OF_SLICE_TYPES = 3
};

// chroma formats (according to how the monochrome or the color planes are intended to be coded)
enum class ChromaFormat : uint8_t
{
  _400 = 0,
  _420,
  _422,
  _444,
  NUM,
  UNDEFINED = NUM
};

enum class Chroma420LocType : uint8_t
{
  LEFT,
  CENTER,
  TOP_LEFT,
  TOP,
  BOTTOM_LEFT,
  BOTTOM,
  UNSPECIFIED,
  NUM,
};

enum class ChannelType : uint8_t
{
  LUMA   = 0,
  CHROMA = 1,
  NUM
};

static inline ChannelType operator++(ChannelType &ch, int)
{
  ChannelType r = ch;

  ch = static_cast<ChannelType>(static_cast<int>(ch) + 1);
  return r;
}

static constexpr auto MAX_NUM_CHANNEL_TYPE = to_underlying(ChannelType::NUM);

enum TreeType
{
  TREE_D = 0, //default tree status (for single-tree slice, TREE_D means joint tree; for dual-tree I slice, TREE_D means TREE_L for luma and TREE_C for chroma)
  TREE_L = 1, //separate tree only contains luma (may split)
  TREE_C = 2, //separate tree only contains chroma (not split), to avoid small chroma block
};

enum ModeType
{
  MODE_TYPE_ALL = 0, //all modes can try
  MODE_TYPE_INTER = 1, //can try inter
  MODE_TYPE_INTRA = 2, //can try intra, ibc, palette
};

enum ComponentID
{
  COMPONENT_Y         = 0,
  COMPONENT_Cb        = 1,
  COMPONENT_Cr        = 2,
  MAX_NUM_COMPONENT   = 3,
  JOINT_CbCr          = MAX_NUM_COMPONENT,
  MAX_NUM_TBLOCKS     = MAX_NUM_COMPONENT
};

#define MAP_CHROMA(c) (ComponentID(c))

enum InputColourSpaceConversion // defined in terms of conversion prior to input of encoder.
{
  IPCOLOURSPACE_UNCHANGED               = 0,
  IPCOLOURSPACE_YCbCrtoYCrCb            = 1, // Mainly used for debug!
  IPCOLOURSPACE_YCbCrtoYYY              = 2, // Mainly used for debug!
  IPCOLOURSPACE_RGBtoGBR                = 3,
  NUMBER_INPUT_COLOUR_SPACE_CONVERSIONS = 4
};

enum MATRIX_COEFFICIENTS // Table E.5 (Matrix coefficients)
{
  MATRIX_COEFFICIENTS_RGB                           = 0,
  MATRIX_COEFFICIENTS_BT709                         = 1,
  MATRIX_COEFFICIENTS_UNSPECIFIED                   = 2,
  MATRIX_COEFFICIENTS_RESERVED_BY_ITUISOIEC         = 3,
  MATRIX_COEFFICIENTS_USFCCT47                      = 4,
  MATRIX_COEFFICIENTS_BT601_625                     = 5,
  MATRIX_COEFFICIENTS_BT601_525                     = 6,
  MATRIX_COEFFICIENTS_SMPTE240                      = 7,
  MATRIX_COEFFICIENTS_YCGCO                         = 8,
  MATRIX_COEFFICIENTS_BT2020_NON_CONSTANT_LUMINANCE = 9,
  MATRIX_COEFFICIENTS_BT2020_CONSTANT_LUMINANCE     = 10,
};

/// supported prediction type
enum PredMode
{
  MODE_INTER                 = 0,     ///< inter-prediction mode
  MODE_INTRA                 = 1,     ///< intra-prediction mode
  MODE_IBC                   = 2,     ///< ibc-prediction mode
  MODE_PLT                   = 3,     ///< plt-prediction mode
  NUMBER_OF_PREDICTION_MODES = 4,
};

/// reference list index
enum RefPicList
{
  REF_PIC_LIST_0               = 0,   ///< reference list 0
  REF_PIC_LIST_1               = 1,   ///< reference list 1
  NUM_REF_PIC_LIST_01          = 2,
  REF_PIC_LIST_X               = 100  ///< special mark
};

#define L0 REF_PIC_LIST_0
#define L1 REF_PIC_LIST_1

/// distortion function index
enum class DFunc
{
  // SSE functions by size
  SSE = 0,
  SSE2,
  SSE4,
  SSE8,
  SSE16,
  SSE32,
  SSE64,
  SSE16N,

  // SAD functions by size
  SAD,
  SAD2,
  SAD4,
  SAD8,
  SAD16,
  SAD32,
  SAD64,
  SAD16N,

  // HAD functions by size
  HAD,
  HAD2,
  HAD4,
  HAD8,
  HAD16,
  HAD32,
  HAD64,
  HAD16N,

  // SAD functions by size (odd sizes)
  SAD12,
  SAD24,
  SAD48,

  // Mean-removed versions
  // NOTE: order must be the same as the regular versions above

  MRSAD,
  MRSAD2,
  MRSAD4,
  MRSAD8,
  MRSAD16,
  MRSAD32,
  MRSAD64,
  MRSAD16N,

  MRHAD,
  MRHAD2,
  MRHAD4,
  MRHAD8,
  MRHAD16,
  MRHAD32,
  MRHAD64,
  MRHAD16N,

  MRSAD12,
  MRSAD24,
  MRSAD48,

  // Full precision versions of SAD functions
  SAD_FULL_NBIT,
  SAD_FULL_NBIT2,
  SAD_FULL_NBIT4,
  SAD_FULL_NBIT8,
  SAD_FULL_NBIT16,
  SAD_FULL_NBIT32,
  SAD_FULL_NBIT64,
  SAD_FULL_NBIT16N,

  SAD_INTERMEDIATE_BITDEPTH,

  SAD_WITH_MASK,

  NUM
};

enum class DFuncDiff;

static inline DFuncDiff operator-(const DFunc &a, const DFunc &b)
{
  return static_cast<DFuncDiff>(to_underlying(a) - to_underlying(b));
}
static inline DFunc operator+(const DFunc &a, const DFuncDiff &b)
{
  return static_cast<DFunc>(to_underlying(a) + to_underlying(b));
}
#if WCG_EXT
enum class DFuncWtd
{
  // Weighted SSE functions by size
  SSE_WTD,
  SSE2_WTD,
  SSE4_WTD,
  SSE8_WTD,
  SSE16_WTD,
  SSE32_WTD,
  SSE64_WTD,
  SSE16N_WTD,

  NUM
};
static inline DFuncWtd operator+(const DFuncWtd &a, const DFuncDiff &b)
{
  return static_cast<DFuncWtd>(to_underlying(a) + to_underlying(b));
}
#endif

/// motion vector predictor direction used in AMVP
enum MvpDir
{
  MD_LEFT = 0,          ///< MVP of left block
  MD_ABOVE,             ///< MVP of above block
  MD_ABOVE_RIGHT,       ///< MVP of above right block
  MD_BELOW_LEFT,        ///< MVP of below left block
  MD_ABOVE_LEFT         ///< MVP of above left block
};

enum TransformDirection
{
  TRANSFORM_FORWARD              = 0,
  TRANSFORM_INVERSE              = 1,
  TRANSFORM_NUMBER_OF_DIRECTIONS = 2
};

/// supported ME search methods
enum class MESearchMethod : int
{
  FULL = 0,
  DIAMOND,
  SELECTIVE,
  DIAMOND_ENHANCED,
  NUM
};

/// coefficient scanning type used in ACS
enum class CoeffScanType
{
  DIAG = 0,
  TRAV_HOR = 1,
  TRAV_VER = 2,
  NUM
};

static inline CoeffScanType operator++(CoeffScanType &a, int)
{
  CoeffScanType b = a;
  a = static_cast<CoeffScanType>(to_underlying(a) + 1);
  return b;
}

enum CoeffScanGroupType
{
  SCAN_UNGROUPED   = 0,
  SCAN_GROUPED_4x4 = 1,
  SCAN_NUMBER_OF_GROUP_TYPES = 2
};

enum ScalingListMode
{
  SCALING_LIST_OFF,
  SCALING_LIST_DEFAULT,
  SCALING_LIST_FILE_READ
};

enum ScalingListSize
{
  SCALING_LIST_1x1 = 0,
  SCALING_LIST_2x2,
  SCALING_LIST_4x4,
  SCALING_LIST_8x8,
  SCALING_LIST_16x16,
  SCALING_LIST_32x32,
  SCALING_LIST_64x64,
  SCALING_LIST_128x128,
  SCALING_LIST_SIZE_NUM,
  //for user define matrix
  SCALING_LIST_FIRST_CODED = SCALING_LIST_2x2,
  SCALING_LIST_LAST_CODED = SCALING_LIST_64x64
};

enum ScalingList1dStartIdx
{
  SCALING_LIST_1D_START_2x2    = 0,
  SCALING_LIST_1D_START_4x4    = 2,
  SCALING_LIST_1D_START_8x8    = 8,
  SCALING_LIST_1D_START_16x16  = 14,
  SCALING_LIST_1D_START_32x32  = 20,
  SCALING_LIST_1D_START_64x64  = 26,
};

// For use with decoded picture hash SEI messages, generated by encoder.
enum class HashType
{
  NONE     = -1,
  MD5      = 0,
  CRC      = 1,
  CHECKSUM = 2,
  NUM
};

enum class SAOMode : uint8_t
{
  OFF = 0,
  NEW,
  MERGE,
  NUM
};

enum class SAOModeMergeTypes : int8_t
{
  NONE = -1,
  LEFT = 0,
  ABOVE,
  NUM
};

enum class SAOModeNewTypes : int8_t
{
  NONE     = -1,
  START_EO = 0,
  EO_0     = START_EO,
  EO_90,
  EO_135,
  EO_45,

  START_BO,
  BO = START_BO,

  NUM
};

static constexpr int NUM_SAO_EO_TYPES_LOG2 = 2;

enum SAOEOClasses
{
  SAO_CLASS_EO_FULL_VALLEY = 0,
  SAO_CLASS_EO_HALF_VALLEY = 1,
  SAO_CLASS_EO_PLAIN       = 2,
  SAO_CLASS_EO_HALF_PEAK   = 3,
  SAO_CLASS_EO_FULL_PEAK   = 4,
  NUM_SAO_EO_CLASSES,
};

enum EOI_OptimizationType
{
  UNDEFINED = 0,
  OBJECT_BASED_OPTIMIZATION = 1,
  TEMPORAL_RESAMPLING = 2,
  SPATIAL_RESAMPLING = 4,
  TEMPORAL_QUALITY_OPTIMIZATION = 8,
  SPATIAL_QUALITY_OPTIMIZATION = 16,
  PRIVACY_PROTECTION_OPTIMIZATION = 32,
};

enum EOI_PRIVACY_PROTECTION
{
  BLURRING = 1,
  REPLACING = 2,
  MASKING = 4,
  PIXELATION = 8
};

enum EOI_OBJECT_BASED
{
  BLURRED = 1,
  COARSER_QUANTIZATION = 2,
  OVERWRITTEN_CONSTANT = 4,
  OVERWRITTEN_NONCONSTANT = 8,
  SIZE_BASED = 16
};

enum NNPC_PaddingType
{
  ZERO_PADDING = 0,
  REPLICATION_PADDING = 1,
  REFLECTION_PADDING = 2,
  WRAP_AROUND_PADDING = 3,
  FIXED_PADDING = 4
};

enum NNPC_PurposeType
{
  UNKONWN                    = 0,
  VISUAL_QUALITY_IMPROVEMENT = 1,
  CHROMA_UPSAMPLING          = 2,
  RESOLUTION_UPSAMPLING      = 4,
  FRAME_RATE_UPSAMPLING      = 8,
  BIT_DEPTH_UPSAMPLING       = 16,
  COLOURIZATION              = 32,
  TEMPORAL_EXTRAPOLATION     = 64,
  SPATIAL_EXTRAPOLATION      = 128
};

enum POST_FILTER_MODE
{
  ISO_IEC_15938_17 = 0,
  URI = 1
};

#if  JVET_AK0114_AI_USAGE_RESTRICTIONS_SEI
enum AUR_CONTEXT
{
  AUR_UNDEFINED = 0,
  AUR_COMMERCIAL_USE= 1,
  AUR_NON_COMMERCIAL_USE = 2,
  AUR_OFFICIAL_GOVERNMENT_USE = 4,
  AUR_RESEARCH_ACADEMIC_USE = 8,
};
enum AUR_RESTICTION
{
  AUR_ANY_APPLICATION = 0,
  AUR_TRAINING,
  AUR_GERNERATIVE,
  AUR_INFERENCE,
};
#endif

struct Fraction
{
  int num = 0;
  int den = 1;

  int    getIntValRound() const { return (num + den / 2) / den; }
  double getFloatVal() const { return static_cast<double>(num) / den; }
};

inline bool operator==(const Fraction& a, const Fraction& b) { return a.num == b.num && a.den == b.den; }
inline bool operator!=(const Fraction& a, const Fraction& b) { return !(a == b); }

#define NUM_SAO_BO_CLASSES_LOG2  5
#define NUM_SAO_BO_CLASSES       (1<<NUM_SAO_BO_CLASSES_LOG2)

namespace Profile
{
  enum Name
  {
    NONE                                 = 0,
    INTRA                                = 8,
    STILL_PICTURE                        = 64,
    MAIN_10                              = 1,
    MAIN_10_STILL_PICTURE                = MAIN_10 | STILL_PICTURE,
    MULTILAYER_MAIN_10                   = 17,
    MULTILAYER_MAIN_10_STILL_PICTURE     = MULTILAYER_MAIN_10 | STILL_PICTURE,
    MAIN_10_444                          = 33,
    MAIN_10_444_STILL_PICTURE            = MAIN_10_444 | STILL_PICTURE,
    MULTILAYER_MAIN_10_444               = 49,
    MULTILAYER_MAIN_10_444_STILL_PICTURE = MULTILAYER_MAIN_10_444 | STILL_PICTURE,
    MAIN_12                              = 2,
    MAIN_12_444                          = 34,
    MAIN_16_444                          = 35,
    MAIN_12_INTRA                        = MAIN_12 | INTRA,
    MAIN_12_444_INTRA                    = MAIN_12_444 | INTRA,
    MAIN_16_444_INTRA                    = MAIN_16_444 | INTRA,
    MAIN_12_STILL_PICTURE                = MAIN_12 | STILL_PICTURE,
    MAIN_12_444_STILL_PICTURE            = MAIN_12_444 | STILL_PICTURE,
    MAIN_16_444_STILL_PICTURE            = MAIN_16_444 | STILL_PICTURE,
  };
}

namespace Level
{
  enum Tier
  {
    MAIN = 0,
    HIGH = 1,
    NUMBER_OF_TIERS=2
  };

  enum Name
  {
    // code = (major_level * 16 + minor_level * 3)
    NONE     = 0,
    LEVEL1   = 16,
    LEVEL2   = 32,
    LEVEL2_1 = 35,
    LEVEL3   = 48,
    LEVEL3_1 = 51,
    LEVEL4   = 64,
    LEVEL4_1 = 67,
    LEVEL5   = 80,
    LEVEL5_1 = 83,
    LEVEL5_2 = 86,
    LEVEL6   = 96,
    LEVEL6_1 = 99,
    LEVEL6_2 = 102,
    LEVEL6_3 = 105,
    LEVEL15_5 = 255,
  };
}

enum CostMode
{
  COST_STANDARD_LOSSY              = 0,
  COST_SEQUENCE_LEVEL_LOSSLESS     = 1,
  COST_LOSSLESS_CODING             = 2,
  COST_MIXED_LOSSLESS_LOSSY_CODING = 3
};

enum WeightedPredictionMethod
{
  WP_PER_PICTURE_WITH_SIMPLE_DC_COMBINED_COMPONENT                          =0,
  WP_PER_PICTURE_WITH_SIMPLE_DC_PER_COMPONENT                               =1,
  WP_PER_PICTURE_WITH_HISTOGRAM_AND_PER_COMPONENT                           =2,
  WP_PER_PICTURE_WITH_HISTOGRAM_AND_PER_COMPONENT_AND_CLIPPING              =3,
  WP_PER_PICTURE_WITH_HISTOGRAM_AND_PER_COMPONENT_AND_CLIPPING_AND_EXTENSION=4
};

enum FastInterSearchMode
{
  FASTINTERSEARCH_DISABLED = 0,
  FASTINTERSEARCH_MODE1    = 1, // TODO: assign better names to these.
  FASTINTERSEARCH_MODE2    = 2,
  FASTINTERSEARCH_MODE3    = 3
};

enum SPSExtensionFlagIndex
{
  SPS_EXT__REXT           = 0,
  NUM_SPS_EXTENSION_FLAGS = 8
};

// TODO: Existing names used for the different NAL unit types can be altered to better reflect the names in the spec.
//       However, the names in the spec are not yet stable at this point. Once the names are stable, a cleanup
//       effort can be done without use of macros to alter the names used to indicate the different NAL unit types.
enum NalUnitType
{
  NAL_UNIT_CODED_SLICE_TRAIL = 0,   // 0
  NAL_UNIT_CODED_SLICE_STSA,        // 1
  NAL_UNIT_CODED_SLICE_RADL,        // 2
  NAL_UNIT_CODED_SLICE_RASL,        // 3

  NAL_UNIT_RESERVED_VCL_4,
  NAL_UNIT_RESERVED_VCL_5,
  NAL_UNIT_RESERVED_VCL_6,

  NAL_UNIT_CODED_SLICE_IDR_W_RADL,  // 7
  NAL_UNIT_CODED_SLICE_IDR_N_LP,    // 8
  NAL_UNIT_CODED_SLICE_CRA,         // 9
  NAL_UNIT_CODED_SLICE_GDR,         // 10

  NAL_UNIT_RESERVED_IRAP_VCL_11,
  NAL_UNIT_OPI,                     // 12
  NAL_UNIT_DCI,                     // 13
  NAL_UNIT_VPS,                     // 14
  NAL_UNIT_SPS,                     // 15
  NAL_UNIT_PPS,                     // 16
  NAL_UNIT_PREFIX_APS,              // 17
  NAL_UNIT_SUFFIX_APS,              // 18
  NAL_UNIT_PH,                      // 19
  NAL_UNIT_ACCESS_UNIT_DELIMITER,   // 20
  NAL_UNIT_EOS,                     // 21
  NAL_UNIT_EOB,                     // 22
  NAL_UNIT_PREFIX_SEI,              // 23
  NAL_UNIT_SUFFIX_SEI,              // 24
  NAL_UNIT_FD,                      // 25

  NAL_UNIT_RESERVED_NVCL_26,
  NAL_UNIT_RESERVED_NVCL_27,

  NAL_UNIT_UNSPECIFIED_28,
  NAL_UNIT_UNSPECIFIED_29,
  NAL_UNIT_UNSPECIFIED_30,
  NAL_UNIT_UNSPECIFIED_31,
  NAL_UNIT_INVALID
};


#if SHARP_LUMA_DELTA_QP
enum LumaLevelToDQPMode
{
  LUMALVL_TO_DQP_DISABLED   = 0,
  LUMALVL_TO_DQP_AVG_METHOD = 1, // use average of CTU to determine luma level
  LUMALVL_TO_DQP_NUM_MODES  = 2
};
#endif

enum class MergeType : uint8_t
{
  DEFAULT_N = 0,
  SUBPU_ATMVP,
  IBC,
  NUM
};

static constexpr int MAX_NUM_APS(ApsType t)
{
  switch (t)
  {
  case ApsType::ALF:
  case ApsType::SCALING_LIST:
    return 8;
  case ApsType::LMCS:
    return 4;
  default:
    return 32;
  }
}

static constexpr int ALF_NUM_FIXED_FILTER_SETS       = 16;
static constexpr int ALF_CTB_MAX_NUM_APS             = MAX_NUM_APS(ApsType::ALF);
static constexpr int ALF_MAX_NUM_ALTERNATIVES_CHROMA = 8;

enum class AlfMode : int8_t
{
  OFF         = -1,
  LUMA_FIXED0 = 0,
  LUMA0       = LUMA_FIXED0 + ALF_NUM_FIXED_FILTER_SETS,
  CHROMA0     = LUMA0 + ALF_CTB_MAX_NUM_APS,
  NUM         = CHROMA0 + ALF_MAX_NUM_ALTERNATIVES_CHROMA
};

inline AlfMode operator+(AlfMode i, int j) { return static_cast<AlfMode>(to_underlying(i) + j); }
inline int     operator-(AlfMode i, AlfMode j) { return to_underlying(i) - to_underlying(j); }

inline bool isAlfLumaFixed(AlfMode m) { return m >= AlfMode::LUMA_FIXED0 && m < AlfMode::LUMA0; }

using AlfCoeff = int16_t;
using AlfBankIdx = uint8_t;
using AlfClipIdx = uint8_t;

//////////////////////////////////////////////////////////////////////////
// Encoder modes to try out
//////////////////////////////////////////////////////////////////////////

enum EncModeFeature
{
  ENC_FT_FRAC_BITS = 0,
  ENC_FT_DISTORTION,
  ENC_FT_RD_COST,
  ENC_FT_ENC_MODE_TYPE,
  ENC_FT_ENC_MODE_OPTS,
  ENC_FT_ENC_MODE_PART,
  NUM_ENC_FEATURES
};

enum ImvMode
{
  IMV_OFF = 0,
  IMV_FPEL,
  IMV_4PEL,
  IMV_HPEL,
  NUM_IMV_MODES
};


// ====================================================================================================================
// Type definition
// ====================================================================================================================

/// parameters for adaptive loop filter

#define MAX_NUM_SAO_CLASSES  32  //(NUM_SAO_EO_GROUPS > NUM_SAO_BO_GROUPS)?NUM_SAO_EO_GROUPS:NUM_SAO_BO_GROUPS

struct SAOOffset
{
  SAOMode modeIdc; // NEW, MERGE, OFF
  union
  {
    SAOModeMergeTypes mergeType;
    SAOModeNewTypes   newType;
  } typeIdc;
  int typeAuxInfo; // BO: starting band index
  int offset[MAX_NUM_SAO_CLASSES];

  SAOOffset();
  ~SAOOffset();
  void reset();

  const SAOOffset& operator= (const SAOOffset& src);
};

struct SAOBlkParam
{

  SAOBlkParam();
  ~SAOBlkParam();
  void reset();
  const SAOBlkParam& operator= (const SAOBlkParam& src);
  SAOOffset& operator[](int compIdx){ return offsetParam[compIdx];}
  const SAOOffset& operator[](int compIdx) const { return offsetParam[compIdx];}
private:
  SAOOffset offsetParam[MAX_NUM_COMPONENT];

};

using BitDepths = EnumArray<int, ChannelType>;

struct ScalingRatio
{
  static constexpr int BITS = 14;

  int x;
  int y;

  bool operator==(const ScalingRatio &s) const { return x == s.x && y == s.y; }
  bool operator!=(const ScalingRatio &s) const { return x != s.x || y != s.y; }
};

enum class PLTRunMode : uint8_t
{
  INDEX = 0,
  COPY,
  NUM
};

struct LutModel
{
  bool             presentFlag = false;
  int              numLutValues = 0;
  std::vector<Pel> lutValues;
};

struct PictureHash
{
  std::vector<uint8_t> hash;

  bool operator==(const PictureHash &other) const
  {
    if (other.hash.size() != hash.size())
    {
      return false;
    }
    for(uint32_t i=0; i<uint32_t(hash.size()); i++)
    {
      if (other.hash[i] != hash[i])
      {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const PictureHash &other) const
  {
    return !(*this == other);
  }
};

struct SEITimeSet
{
  SEITimeSet() : clockTimeStampFlag(false),
                     numUnitFieldBasedFlag(false),
                     countingType(0),
                     fullTimeStampFlag(false),
                     discontinuityFlag(false),
                     cntDroppedFlag(false),
                     numberOfFrames(0),
                     secondsValue(0),
                     minutesValue(0),
                     hoursValue(0),
                     secondsFlag(false),
                     minutesFlag(false),
                     hoursFlag(false),
                     timeOffsetLength(0),
                     timeOffsetValue(0)
  { }
  bool clockTimeStampFlag;
  bool numUnitFieldBasedFlag;
  int  countingType;
  bool fullTimeStampFlag;
  bool discontinuityFlag;
  bool cntDroppedFlag;
  int  numberOfFrames;
  int  secondsValue;
  int  minutesValue;
  int  hoursValue;
  bool secondsFlag;
  bool minutesFlag;
  bool hoursFlag;
  int  timeOffsetLength;
  int  timeOffsetValue;
};

struct SEIMasteringDisplay
{
  bool      colourVolumeSEIEnabled;
  uint32_t      maxLuminance;
  uint32_t      minLuminance;
  uint16_t    primaries[3][2];
  uint16_t    whitePoint[2];
};

struct SEIQualityMetrics
{
  double psnr = 0;
  double ssim = 0;
  double wpsnr = 0;
  double wspsnr = 0;
};

struct SEIComplexityMetrics
{
  uint32_t portionNonZeroBlocksArea = 0;
  uint32_t portionNonZero_4_8_16BlocksArea = 0;
  uint32_t portionNonZero_32_64_128BlocksArea = 0;
  uint32_t portionNonZero_256_512_1024BlocksArea = 0;
  uint32_t portionNonZero_2048_4096BlocksArea = 0;
  uint32_t portionNonZeroTransformCoefficientsArea = 0;
  uint32_t portionIntraPredictedBlocksArea = 0;
  uint32_t portionBdofBlocksArea = 0;
  uint32_t portionBiAndGpmPredictedBlocksArea = 0;
  uint32_t portionDeblockingInstances = 0;
  uint32_t portionSaoInstances = 0;
  uint32_t portionAlfInstances = 0;
};


#if SHARP_LUMA_DELTA_QP
struct LumaLevelToDeltaQPMapping
{
  LumaLevelToDQPMode                 mode;             ///< use deltaQP determined by block luma level
  double                             maxMethodWeight;  ///< weight of max luma value when mode = 2
  std::vector< std::pair<int, int> > mapping;          ///< first=luma level, second=delta QP.
#if ENABLE_QPA
  bool isEnabled() const { return (mode != LUMALVL_TO_DQP_DISABLED && mode != LUMALVL_TO_DQP_NUM_MODES); }
#else
  bool isEnabled() const { return mode!=LUMALVL_TO_DQP_DISABLED; }
#endif
};
#endif

#if ER_CHROMA_QP_WCG_PPS
struct WCGChromaQPControl
{
  bool isEnabled() const { return enabled; }
  bool   enabled;         ///< Enabled flag (0:default)
  double chromaCbQpScale; ///< Chroma Cb QP Scale (1.0:default)
  double chromaCrQpScale; ///< Chroma Cr QP Scale (1.0:default)
  double chromaQpScale;   ///< Chroma QP Scale (0.0:default)
  double chromaQpOffset;  ///< Chroma QP Offset (0.0:default)
};
#endif

class ChromaCbfs
{
public:
  ChromaCbfs()
    : Cb(true), Cr(true)
  {}
  ChromaCbfs( bool _cbf )
    : Cb( _cbf ), Cr( _cbf )
  {}
public:
  bool sigChroma( ChromaFormat chromaFormat ) const
  {
    if (chromaFormat == ChromaFormat::_400)
    {
      return false;
    }
    return   ( Cb || Cr );
  }
  bool& cbf( ComponentID compID )
  {
    bool *cbfs[MAX_NUM_TBLOCKS] = { nullptr, &Cb, &Cr };

    return *cbfs[compID];
  }
public:
  bool Cb;
  bool Cr;
};


enum MsgLevel
{
  SILENT  = 0,
  ERROR   = 1,
  WARNING = 2,
  INFO    = 3,
  NOTICE  = 4,
  VERBOSE = 5,
  DETAILS = 6
};
enum RESHAPE_SIGNAL_TYPE
{
  RESHAPE_SIGNAL_SDR = 0,
  RESHAPE_SIGNAL_PQ  = 1,
  RESHAPE_SIGNAL_HLG = 2,
  RESHAPE_SIGNAL_NULL = 100,
};

enum EncType
{
  ENC_PRE = 0,
  ENC_FULL = 1,
};

// ---------------------------------------------------------------------------
// exception class
// ---------------------------------------------------------------------------

class Exception : public std::exception
{
public:
  Exception( const std::string& _s ) : m_str( _s ) { }
  Exception( const Exception& _e ) : std::exception( _e ), m_str( _e.m_str ) { }
  virtual ~Exception() noexcept { };
  virtual const char* what() const noexcept { return m_str.c_str(); }
  Exception& operator=( const Exception& _e ) { std::exception::operator=( _e ); m_str = _e.m_str; return *this; }
  template<typename T> Exception& operator<<( T t ) { std::ostringstream oss; oss << t; m_str += oss.str(); return *this; }
private:
  std::string m_str;
};

// if a check fails with THROW or CHECK, please check if ported correctly from assert in revision 1196)
#define THROW(x)            throw( Exception( "\nERROR: In function \"" ) << __FUNCTION__ << "\" in " << __FILE__ << ":" << __LINE__ << ": " << x )
#define CHECK(c,x)          if(c){ THROW(x); }
#define EXIT(x)             throw( Exception( "\n" ) << x << "\n" )
#define CHECK_NULLPTR(_ptr) CHECK( !( _ptr ), "Accessing an empty pointer pointer!" )

#if !NDEBUG  // for non MSVC compiler, define _DEBUG if in debug mode to have same behavior between MSVC and others in debug
#ifndef _DEBUG
#define _DEBUG 1
#endif
#endif

#if defined( _DEBUG )
#define CHECKD(c,x)         if(c){ THROW(x); }
#else
#define CHECKD(c,x)
#endif // _DEBUG

// ---------------------------------------------------------------------------
// static vector
// ---------------------------------------------------------------------------

template<typename T, size_t N>
class static_vector
{
  T _arr[ N ];
  size_t _size;

public:

  typedef T         value_type;
  typedef size_t    size_type;
  typedef ptrdiff_t difference_type;
  typedef T&        reference;
  typedef T const&  const_reference;
  typedef T*        pointer;
  typedef T const*  const_pointer;
  typedef T*        iterator;
  typedef T const*  const_iterator;

  static constexpr size_type max_num_elements = N;

  static_vector() : _size( 0 )                                 { }
  static_vector( size_t N_ ) : _size( N_ )                     { }
  static_vector( size_t N_, const T& _val ) : _size( 0 )       { resize( N_, _val ); }
  template<typename It>
  static_vector( It _it1, It _it2 ) : _size( 0 )               { while( _it1 < _it2 ) _arr[ _size++ ] = *_it1++; }
  static_vector( std::initializer_list<T> _il ) : _size( 0 )
  {
    typename std::initializer_list<T>::iterator _src1 = _il.begin();
    typename std::initializer_list<T>::iterator _src2 = _il.end();

    while( _src1 < _src2 ) _arr[ _size++ ] = *_src1++;

    CHECKD( _size > N, "capacity exceeded" );
  }
  static_vector& operator=( std::initializer_list<T> _il )
  {
    _size = 0;

    typename std::initializer_list<T>::iterator _src1 = _il.begin();
    typename std::initializer_list<T>::iterator _src2 = _il.end();

    while( _src1 < _src2 ) _arr[ _size++ ] = *_src1++;

    CHECKD( _size > N, "capacity exceeded" );

    return *this;
  }

  void resize( size_t N_ )                      { CHECKD( N_ > N, "capacity exceeded" ); while(_size < N_) _arr[ _size++ ] = T() ; _size = N_; }
  void resize( size_t N_, const T& _val )       { CHECKD( N_ > N, "capacity exceeded" ); while(_size < N_) _arr[ _size++ ] = _val; _size = N_; }
  void reserve( size_t N_ )                     { CHECKD( N_ > N, "capacity exceeded" ); }
  void push_back( const T& _val )               { CHECKD( _size >= N, "capacity exceeded" ); _arr[ _size++ ] = _val; }
  void push_back( T&& val )                     { CHECKD( _size >= N, "capacity exceeded" ); _arr[ _size++ ] = std::forward<T>( val ); }
  void pop_back()                               { CHECKD( _size == 0, "calling pop_back on an empty vector" ); _size--; }
  void pop_front()                              { CHECKD( _size == 0, "calling pop_front on an empty vector" ); _size--; for( int i = 0; i < _size; i++ ) _arr[i] = _arr[i + 1]; }
  void clear()                                  { _size = 0; }
  reference       at( size_t _i )               { CHECKD( _i >= _size, "Trying to access an out-of-bound-element" ); return _arr[ _i ]; }
  const_reference at( size_t _i ) const         { CHECKD( _i >= _size, "Trying to access an out-of-bound-element" ); return _arr[ _i ]; }
  reference       operator[]( size_t _i )       { CHECKD( _i >= _size, "Trying to access an out-of-bound-element" ); return _arr[ _i ]; }
  const_reference operator[]( size_t _i ) const { CHECKD( _i >= _size, "Trying to access an out-of-bound-element" ); return _arr[ _i ]; }
  reference       front()                       { CHECKD( _size == 0, "Trying to access the first element of an empty vector" ); return _arr[ 0 ]; }
  const_reference front() const                 { CHECKD( _size == 0, "Trying to access the first element of an empty vector" ); return _arr[ 0 ]; }
  reference       back()                        { CHECKD( _size == 0, "Trying to access the last element of an empty vector" );  return _arr[ _size - 1 ]; }
  const_reference back() const                  { CHECKD( _size == 0, "Trying to access the last element of an empty vector" );  return _arr[ _size - 1 ]; }
  pointer         data()                        { return _arr; }
  const_pointer   data() const                  { return _arr; }
  iterator        begin()                       { return _arr; }
  const_iterator  begin() const                 { return _arr; }
  const_iterator  cbegin() const                { return _arr; }
  iterator        end()                         { return _arr + _size; }
  const_iterator  end() const                   { return _arr + _size; };
  const_iterator  cend() const                  { return _arr + _size; };
  size_type       size() const                  { return _size; };
  size_type       byte_size() const             { return _size * sizeof( T ); }
  bool            empty() const                 { return _size == 0; }

  size_type       capacity() const              { return N; }
  size_type       max_size() const              { return N; }
  size_type       byte_capacity() const         { return sizeof(_arr); }

  iterator        insert( const_iterator _pos, const T& _val )
                                                { CHECKD( _size >= N, "capacity exceeded" );
                                                  for( difference_type i = _size - 1; i >= _pos - _arr; i-- ) _arr[i + 1] = _arr[i];
                                                  *const_cast<iterator>( _pos ) = _val;
                                                  _size++;
                                                  return const_cast<iterator>( _pos ); }

  iterator        insert( const_iterator _pos, T&& _val )
                                                { CHECKD( _size >= N, "capacity exceeded" );
                                                  for( difference_type i = _size - 1; i >= _pos - _arr; i-- ) _arr[i + 1] = _arr[i];
                                                  *const_cast<iterator>( _pos ) = std::forward<T>( _val );
                                                  _size++; return const_cast<iterator>( _pos ); }
  template<class InputIt>
  iterator        insert( const_iterator _pos, InputIt first, InputIt last )
                                                { const difference_type numEl = last - first;
                                                  CHECKD( _size + numEl > N, "capacity exceeded" );
                                                  for( difference_type i = _size - 1; i >= _pos - _arr; i-- ) _arr[i + numEl] = _arr[i];
                                                  iterator it = const_cast<iterator>( _pos ); _size += numEl;
                                                  while( first != last ) *it++ = *first++;
                                                  return const_cast<iterator>( _pos ); }

  iterator        insert( const_iterator _pos, size_t numEl, const T& val )
                                                { //const difference_type numEl = last - first;
                                                  CHECKD( _size + numEl > N, "capacity exceeded" );
                                                  for( difference_type i = _size - 1; i >= _pos - _arr; i-- ) _arr[i + numEl] = _arr[i];
                                                  iterator it = const_cast<iterator>( _pos ); _size += numEl;
                                                  for ( int k = 0; k < numEl; k++) *it++ = val;
                                                  return const_cast<iterator>( _pos ); }

  void            erase( const_iterator _pos )  { iterator it   = const_cast<iterator>( _pos ) - 1;
                                                  iterator last = end() - 1;
                                                  while( ++it != last ) *it = *( it + 1 );
                                                  _size--; }
};


// ---------------------------------------------------------------------------
// This class contains a pool of objects that can be used and reused
// while minimizing the amount of required memory allocation and
// deallocation operations.
// ---------------------------------------------------------------------------

template<typename T> class Pool
{
  std::vector<T *> m_items;

public:
  ~Pool() { deleteEntries(); }

  void deleteEntries()
  {
    for (auto &p: m_items)
    {
      delete p;
      p = nullptr;
    }

    m_items.clear();
  }

  T* get()
  {
    T* ret;

    if (!m_items.empty())
    {
      ret = m_items.back();
      m_items.pop_back();
    }
    else
    {
      ret = new T;
    }

    return ret;
  }

  void giveBack(T *el) { m_items.push_back(el); }

  void giveBack(std::vector<T *> &vel)
  {
    m_items.insert(m_items.end(), vel.begin(), vel.end());
    vel.clear();
  }
};

typedef Pool<struct CodingUnit>     CuPool;
typedef Pool<struct PredictionUnit> PuPool;
typedef Pool<struct TransformUnit>  TuPool;

struct XuPool
{
  CuPool cuPool;
  PuPool puPool;
  TuPool tuPool;
};



//! \}

#endif


