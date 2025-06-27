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

#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <memory.h>

#include "CommonLib/Rom.h"
#include "VideoIOYuv.h"
#include "CommonLib/Unit.h"

namespace Y4M
{
constexpr int  SIGNATURE_LENGTH                      = 10;
constexpr char SIGNATURE[SIGNATURE_LENGTH + 1]       = "YUV4MPEG2 ";
constexpr int  MAX_HEADER_LENGTH                     = 255;
constexpr int  FRAME_HEADER_LENGTH                   = 6;
constexpr char FRAME_HEADER[FRAME_HEADER_LENGTH + 1] = "FRAME\n";

struct SamplingFormat
{
  char             name[16];
  int              bitDepth;
  ChromaFormat     chromaFormat;
  Chroma420LocType locType;
};

static const std::array samplingFormats = {
  SamplingFormat{ "mono9", 9, ChromaFormat::_400, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "mono10", 10, ChromaFormat::_400, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "mono12", 12, ChromaFormat::_400, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "mono", 8, ChromaFormat::_400, Chroma420LocType::UNSPECIFIED },

  SamplingFormat{ "420jpeg", 8, ChromaFormat::_420, Chroma420LocType::CENTER },
  SamplingFormat{ "420mpeg2", 8, ChromaFormat::_420, Chroma420LocType::LEFT },
  SamplingFormat{ "420paldv", 8, ChromaFormat::_420, Chroma420LocType::TOP_LEFT },

  SamplingFormat{ "420p9", 9, ChromaFormat::_420, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "420p10", 10, ChromaFormat::_420, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "420p12", 12, ChromaFormat::_420, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "420", 8, ChromaFormat::_420, Chroma420LocType::UNSPECIFIED },

  SamplingFormat{ "422p9", 9, ChromaFormat::_422, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "422p10", 10, ChromaFormat::_422, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "422p12", 12, ChromaFormat::_422, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "422", 8, ChromaFormat::_422, Chroma420LocType::UNSPECIFIED },

  SamplingFormat{ "444p9", 9, ChromaFormat::_444, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "444p10", 10, ChromaFormat::_444, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "444p12", 12, ChromaFormat::_444, Chroma420LocType::UNSPECIFIED },
  SamplingFormat{ "444", 8, ChromaFormat::_444, Chroma420LocType::UNSPECIFIED },
};
};   // namespace Y4M

constexpr int MAX_FILE_BIT_DEPTH = 16;

// ====================================================================================================================
// Local Functions
// ====================================================================================================================

/**
 * Scale all pixels in img depending upon sign of shiftbits by a factor of
 * 2<sup>shiftbits</sup>.
 *
 * @param areabuf buffer to be scaled
 * @param shiftbits if zero, no operation performed
 *                  if > 0, multiply by 2<sup>shiftbits</sup>, see scalePlane()
 *                  if < 0, divide and round by 2<sup>shiftbits</sup> and clip,
 *                          see invScalePlane().
 * @param minval  minimum clipping value when dividing.
 * @param maxval  maximum clipping value when dividing.
 */
static void scalePlane( PelBuf& areaBuf, const int shiftbits, const Pel minval, const Pel maxval)
{
  const unsigned width  = areaBuf.width;
  const unsigned height = areaBuf.height;
  const ptrdiff_t stride = areaBuf.stride;
  Pel            *img    = areaBuf.bufAt(0, 0);

  if( 0 == shiftbits )
  {
    return;
  }

  if( shiftbits > 0)
  {
    for( unsigned y = 0; y < height; y++, img+=stride)
    {
      for( unsigned x = 0; x < width; x++)
      {
        img[x] <<= shiftbits;
      }
    }
  }
  else if (shiftbits < 0)
  {
    const int shiftbitsr =- shiftbits;
    const Pel rounding = 1 << (shiftbitsr-1);

    for( unsigned y = 0; y < height; y++, img+=stride)
    {
      for( unsigned x = 0; x < width; x++)
      {
        img[x] = Clip3(minval, maxval, Pel((img[x] + rounding) >> shiftbitsr));
      }
    }
  }
}


// ====================================================================================================================
// Public member functions
// ====================================================================================================================

void VideoIOYuv::open(const std::string& fileName, bool writeMode, const BitDepths& fileBitDepth,
                      const BitDepths& msbExtendedBitDepth, const BitDepths& internalBitDepth)
{
  //NOTE: files cannot have bit depth greater than 16
  for (const auto chType: { ChannelType::LUMA, ChannelType::CHROMA })
  {
    m_fileBitdepth[chType]        = std::min<uint32_t>(fileBitDepth[chType], MAX_FILE_BIT_DEPTH);
    m_msbExtendedBitDepth[chType] = msbExtendedBitDepth[chType];
    m_bitdepthShift[chType]       = internalBitDepth[chType] - m_msbExtendedBitDepth[chType];

    if (fileBitDepth[chType] > MAX_FILE_BIT_DEPTH)
    {
      if (writeMode)
      {
        std::cerr << "\nWARNING: Cannot write a yuv file of bit depth greater than 16 - output will be right-shifted down to 16-bit precision\n" << std::endl;
      }
      else
      {
        EXIT( "ERROR: Cannot read a yuv file of bit depth greater than 16" );
      }
    }
  }

  if (writeMode)
  {
    m_fileStream.open(fileName.c_str(), std::ios::binary | std::ios::out);

    if (m_fileStream.fail())
    {
      EXIT( "Failed to write reconstructed YUV file: " << fileName.c_str() );
    }
    if (isY4mFileExt(fileName))
    {
      writeY4mFileHeader();
      m_outY4m = true;
    }
  }
  else
  {
    if (isY4mFileExt(fileName))
    {
      if (m_inY4mFileHeaderLength == 0)
      {
        int          dummyWidth        = 0;
        int          dummyHeight       = 0;
        Fraction     dummyFrameRate;
        int          dummyBitDepth     = 0;
        ChromaFormat dummyChromaFormat = ChromaFormat::_420;
        Chroma420LocType dummyLocType      = Chroma420LocType::UNSPECIFIED;
        parseY4mFileHeader(fileName, dummyWidth, dummyHeight, dummyFrameRate, dummyBitDepth, dummyChromaFormat,
                           dummyLocType);
      }
    }
    m_fileStream.open(fileName.c_str(), std::ios::binary | std::ios::in);

    if (m_fileStream.fail())
    {
      EXIT( "Failed to open input YUV file: " << fileName.c_str() );
    }

    if (m_inY4mFileHeaderLength)
    {
      m_fileStream.seekg(m_inY4mFileHeaderLength, std::ios::cur);
    }
  }
}

void VideoIOYuv::parseY4mFileHeader(const std::string& fileName, int& width, int& height, Fraction& frameRate,
                                    int& bitDepth, ChromaFormat& chromaFormat, Chroma420LocType& locType)
{
  m_fileStream.open(fileName.c_str(), std::ios::binary | std::ios::in);
  CHECK(m_fileStream.fail(), "File open failed.")

  char header[Y4M::MAX_HEADER_LENGTH];
  m_fileStream.read(header, sizeof(header));
  CHECK(strncmp(header, Y4M::SIGNATURE, Y4M::SIGNATURE_LENGTH), "The input is not a Y4M file!");

  // locate the end of the header
  for (int i = Y4M::SIGNATURE_LENGTH + 1; i < Y4M::MAX_HEADER_LENGTH; i++)
  {
    if (header[i] == '\n')
    {
      header[i]             = ' ';   // space is used as token end later
      m_inY4mFileHeaderLength = i + 1;
      break;
    }
  }
  // parse Y4M header info
  for (int i = Y4M::SIGNATURE_LENGTH; i < m_inY4mFileHeaderLength; i++)
  {
    int numerator = 0, denominator = 0;
    switch (header[i])
    {
    case 'W': sscanf(header + i + 1, "%d", &width); break;
    case 'H': sscanf(header + i + 1, "%d", &height); break;
    case 'C':
      for (const auto& cf: Y4M::samplingFormats)
      {
        if (strncmp(&header[i + 1], cf.name, strlen(cf.name)) == 0)
        {
          chromaFormat = cf.chromaFormat;
          locType      = cf.locType;
          bitDepth     = cf.bitDepth;
          break;
        }
      }
      break;
    case 'F':
      if (sscanf(header + i + 1, "%d:%d", &numerator, &denominator) == 2)
      {
        if (denominator != 0)
        {
          frameRate.num = numerator;
          frameRate.den = denominator;
        }
      }
      break;
    case 'I': CHECK(header[i + 1] != 'p', "Interlaced Y4M is not supported yet");
    case 'A':   // not support, ignore
    case 'X':   // not support, ignore
      break;
    default: CHECK(true, "Wrong Y4M file header!")
    }
    i = (int) (strchr(header + i + 1, ' ') - header);
  }

  m_fileStream.close();
}

void VideoIOYuv::setOutputY4mInfo(int width, int height, const Fraction& frameRate, int bitDepth,
                                  ChromaFormat chromaFormat, Chroma420LocType locType)
{
  m_outPicWidth     = width;
  m_outPicHeight    = height;
  m_outBitDepth     = bitDepth;
  m_outFrameRate    = frameRate;
  m_outChromaFormat = chromaFormat;
  m_outLocType      = locType;
}

void VideoIOYuv::writeY4mFileHeader()
{
  CHECK(m_outPicWidth == 0 || m_outPicHeight == 0 || m_outBitDepth == 0 || m_outFrameRate.num == 0,
        "Output Y4M file into has not been set");
  std::string header = Y4M::SIGNATURE;
  header += "W" + std::to_string(m_outPicWidth) + " ";
  header += "H" + std::to_string(m_outPicHeight) + " ";
  header += "F" + std::to_string(m_outFrameRate.num) + ":" + std::to_string(m_outFrameRate.den) + " ";
  header += "Ip A0:0 ";
  header += "C";
  bool found = false;
  for (const auto& cf: Y4M::samplingFormats)
  {
    if (m_outBitDepth == cf.bitDepth && m_outChromaFormat == cf.chromaFormat && m_outLocType == cf.locType)
    {
      header += cf.name;
      found = true;
      break;
    }
  }
  if (!found)
  {
    for (const auto& cf: Y4M::samplingFormats)
    {
      if (m_outBitDepth == cf.bitDepth && m_outChromaFormat == cf.chromaFormat
          && Chroma420LocType::UNSPECIFIED == cf.locType)
      {
        header += cf.name;
        found = true;
        msg(WARNING, "Value for chroma sample location unsupported by y4m. Signalling unspecified location.");
        break;
      }
    }
  }
  CHECK(!found, "Format unsupported by y4m");
  header += "\n";
  // not write extension/comment

  m_fileStream.write(header.c_str(), header.length());
}

void VideoIOYuv::close() { m_fileStream.close(); }

bool VideoIOYuv::isEof() { return m_fileStream.eof(); }

bool VideoIOYuv::isFail() { return m_fileStream.fail(); }

/**
 * Skip numFrames in input.
 *
 * This function correctly handles cases where the input file is not
 * seekable, by consuming bytes.
 */
#if EXTENSION_360_VIDEO
void VideoIOYuv::skipFrames(int numFrames, uint32_t width, uint32_t height, ChromaFormat format)
#else
void VideoIOYuv::skipFrames(uint32_t numFrames, uint32_t width, uint32_t height, ChromaFormat format)
#endif
{
  if (!numFrames)
  {
    return;
  }

  //------------------
  //set the frame size according to the chroma format
  std::streamoff frameSize = 0;
  uint32_t wordsize=1; // default to 8-bit, unless a channel with more than 8-bits is detected.
  for (uint32_t component = 0; component < getNumberValidComponents(format); component++)
  {
    ComponentID compID=ComponentID(component);
    frameSize += (width >> getComponentScaleX(compID, format)) * (height >> getComponentScaleY(compID, format));
    if (m_fileBitdepth[toChannelType(compID)] > 8)
    {
      wordsize=2;
    }
  }
  frameSize *= wordsize;
  //------------------
  if (m_inY4mFileHeaderLength)
  {
    frameSize += Y4M::FRAME_HEADER_LENGTH;
  }

  const std::streamoff offset = frameSize * numFrames;

  /* attempt to seek */
  if (!!m_fileStream.seekg(offset, std::ios::cur))
  {
    return; /* success */
  }
  m_fileStream.clear();

  /* fall back to consuming the input */
  char buf[512];
  const std::streamoff offsetModeBufsize = offset % sizeof(buf);
  for (std::streamoff i = 0; i < offset - offsetModeBufsize; i += sizeof(buf))
  {
    m_fileStream.read(buf, sizeof(buf));
  }
  m_fileStream.read(buf, offsetModeBufsize);
}

/**
 * Read width*height pixels from fd into dst, optionally
 * padding the left and right edges by edge-extension.  Input may be
 * either 8bit or 16bit little-endian lsb-aligned words.
 *
 * @param dst          destination image plane
 * @param fd           input file stream
 * @param is16bit      true if input file carries > 8bit data, false otherwise.
 * @param stride444    distance between vertically adjacent pixels of dst.
 * @param width444     width of active area in dst.
 * @param height444    height of active area in dst.
 * @param pad_x444     length of horizontal padding.
 * @param pad_y444     length of vertical padding.
 * @param compID       chroma component
 * @param destFormat   chroma format of image
 * @param fileFormat   chroma format of file
 * @param fileBitDepth component bit depth in file
 * @return true for success, false in case of error
 */
static bool readPlane(Pel *dst, std::istream &fd, bool is16bit, ptrdiff_t stride444, uint32_t width444,
                      uint32_t height444, uint32_t pad_x444, uint32_t pad_y444, const ComponentID compID,
                      const ChromaFormat destFormat, const ChromaFormat fileFormat, const uint32_t fileBitDepth)
{
  const uint32_t csxFile = getComponentScaleX(compID, fileFormat);
  const uint32_t csyFile = getComponentScaleY(compID, fileFormat);
  const uint32_t csxDest = getComponentScaleX(compID, destFormat);
  const uint32_t csyDest = getComponentScaleY(compID, destFormat);

  const uint32_t widthDest  = width444 >> csxDest;
  const uint32_t heightDest = height444 >> csyDest;
  const uint32_t padDestX   = pad_x444 >> csxDest;
  const uint32_t padDestY   = pad_y444 >> csyDest;
#if EXTENSION_360_VIDEO
  const ptrdiff_t strideDest = stride444;
#else
  const ptrdiff_t strideDest  = stride444 >> csxDest;
#endif
  const uint32_t fullWidthDest  = widthDest + padDestX;
  const uint32_t fullHeightDest = heightDest + padDestY;

  const uint32_t       strideFile = (width444 * (is16bit ? 2 : 1)) >> csxFile;
  std::vector<uint8_t> bufVec(strideFile);
  uint8_t *buf=&(bufVec[0]);

  Pel*            pDstPad              = dst + strideDest * heightDest;
  Pel  *pDstBuf              = dst;
  const ptrdiff_t dstBufStride         = strideDest;

  if (compID != COMPONENT_Y && (!isChromaEnabled(fileFormat) || !isChromaEnabled(destFormat)))
  {
    if (isChromaEnabled(destFormat))
    {
      // set chrominance data to mid-range: (1<<(fileBitDepth-1))
      const Pel value=Pel(1<<(fileBitDepth-1));
      for (uint32_t y = 0; y < fullHeightDest; y++, pDstBuf += dstBufStride)
      {
        for (uint32_t x = 0; x < fullWidthDest; x++)
        {
          pDstBuf[x] = value;
        }
      }
    }

    if (isChromaEnabled(fileFormat))
    {
      const uint32_t heightFile = height444 >> csyFile;
      fd.seekg(heightFile * strideFile, std::ios::cur);
      if (fd.eof() || fd.fail() )
      {
        return false;
      }
    }
  }
  else
  {
    const uint32_t maskFileY = (1 << csyFile) - 1;
    const uint32_t maskDestY = (1 << csyDest) - 1;
    for(uint32_t y444=0; y444<height444; y444++)
    {
      if ((y444 & maskFileY) == 0)
      {
        // read a new line
        fd.read(reinterpret_cast<char*>(buf), strideFile);
        if (fd.eof() || fd.fail() )
        {
          return false;
        }
      }

      if ((y444 & maskDestY) == 0)
      {
        // process current destination line
        if (csxFile < csxDest)
        {
          // eg file is 444, dest is 422.
          const uint32_t sx = csxDest - csxFile;
          if (!is16bit)
          {
            for (uint32_t x = 0; x < widthDest; x++)
            {
              pDstBuf[x] = buf[x<<sx];
            }
          }
          else
          {
            for (uint32_t x = 0; x < widthDest; x++)
            {
              pDstBuf[x] = Pel(buf[(x<<sx)*2+0]) | (Pel(buf[(x<<sx)*2+1])<<8);
            }
          }
        }
        else
        {
          // eg file is 422, dest is 444.
          const uint32_t sx = csxFile - csxDest;
          if (!is16bit)
          {
            for (uint32_t x = 0; x < widthDest; x++)
            {
              pDstBuf[x] = buf[x>>sx];
            }
          }
          else
          {
            for (uint32_t x = 0; x < widthDest; x++)
            {
              pDstBuf[x] = Pel(buf[(x>>sx)*2+0]) | (Pel(buf[(x>>sx)*2+1])<<8);
            }
          }
        }

        // process right hand side padding
        for (uint32_t x = widthDest; x < fullWidthDest; x++)
        {
          pDstBuf[x] = pDstBuf[widthDest-1];
        }

        pDstBuf += dstBufStride;
      }
    }

    // process lower padding
    for (uint32_t y = heightDest; y < fullHeightDest; y++, pDstPad += strideDest)
    {
      for (uint32_t x = 0; x < fullWidthDest; x++)
      {
        pDstPad[x] = (pDstPad - strideDest)[x];
      }
    }
  }
  return true;
}

static bool verifyPlane(Pel *dst, ptrdiff_t stride444, uint32_t width444, uint32_t height444, uint32_t padX444,
                        uint32_t padY444, const ComponentID compID, const ChromaFormat cFormat, const uint32_t bitDepth)
{
  const uint32_t csx =getComponentScaleX(compID, cFormat);
  const uint32_t csy =getComponentScaleY(compID, cFormat);

#if EXTENSION_360_VIDEO
  const ptrdiff_t stride = stride444;
#else
  const ptrdiff_t stride      = stride444 >> csx;
#endif
  const uint32_t fullWidth  = (width444 + padX444) >> csx;
  const uint32_t fullHeight = (height444 +padY444) >> csy;

  Pel  *dstBuf              = dst;

  const Pel mask = ~((1 << bitDepth) - 1);

  for (uint32_t y = 0; y < fullHeight; y++, dstBuf+= stride)
  {
    for (uint32_t x = 0; x < fullWidth; x++)
    {
      if ( (dstBuf[x] & mask) != 0)
      {
        return false;
      }
    }
  }

  return true;
}


/**
 * Write an image plane (width444*height444 pixels) from src into output stream fd.
 *
 * @param fd         output file stream
 * @param src        source image
 * @param is16bit    true if input file carries > 8bit data, false otherwise.
 * @param stride444  distance between vertically adjacent pixels of src.
 * @param width444   width of active area in src.
 * @param height444  height of active area in src.
 * @param compID       chroma component
 * @param srcFormat    chroma format of image
 * @param fileFormat   chroma format of file
 * @param fileBitDepth component bit depth in file
 * @return true for success, false in case of error
 */
static bool writePlane(uint32_t orgWidth, uint32_t orgHeight, std::ostream& fd, const Pel* src, const bool is16bit,
                       const ptrdiff_t strideSrc, uint32_t width444, uint32_t height444, const ComponentID compID,
                       const ChromaFormat srcFormat, const ChromaFormat fileFormat, const uint32_t fileBitDepth,
                       const uint32_t packedYUVOutputMode = 0)
{
  const uint32_t csxFile = getComponentScaleX(compID, fileFormat);
  const uint32_t csyFile = getComponentScaleY(compID, fileFormat);
  const uint32_t csxSrc  = getComponentScaleX(compID, srcFormat);
  const uint32_t csySrc  = getComponentScaleY(compID, srcFormat);

  const uint32_t widthFile  = width444 >> csxFile;
  const uint32_t heightFile = height444 >> csyFile;
  const bool     writePYUV  = (packedYUVOutputMode > 0) && (fileBitDepth == 10 || fileBitDepth == 12)
                         && ((widthFile & (1 + (fileBitDepth & 3))) == 0);

  CHECK(csxFile != csxSrc, "Not supported");
  const uint32_t strideFile =
    writePYUV ? (orgWidth * fileBitDepth) >> (csxFile + 3) : (orgWidth * (is16bit ? 2 : 1)) >> csxFile;

  std::vector<uint8_t> bufVec(strideFile);
  uint8_t *buf=&(bufVec[0]);

  const Pel *pSrcBuf         = src;
  const ptrdiff_t srcBufStride    = strideSrc;

  if (writePYUV)
  {
    const uint32_t maskFileY  = (1 << csyFile) - 1;
    const uint32_t maskSrcY   = (1 << csySrc) - 1;
    const uint32_t widthFileS = widthFile >> (fileBitDepth == 12 ? 1 : 2);

    for (uint32_t y444 = 0; y444 < height444; y444++)
    {
      if ((y444 & maskFileY) == 0)   // write a new line to file
      {
        if (csxFile < csxSrc)
        {
          // eg file is 444, source is 422.
          const uint32_t sx = csxSrc - csxFile;

          if (fileBitDepth == 10)  // write 4 values into 5 bytes
          {
            for (uint32_t x = 0; x < widthFileS; x++)
            {
              const uint32_t src0 = pSrcBuf[(4*x  ) >> sx];
              const uint32_t src1 = pSrcBuf[(4*x+1) >> sx];
              const uint32_t src2 = pSrcBuf[(4*x+2) >> sx];
              const uint32_t src3 = pSrcBuf[(4*x+3) >> sx];

              buf[5*x  ] = ((src0     ) & 0xff); // src0:76543210
              buf[5*x+1] = ((src1 << 2) & 0xfc) + ((src0 >> 8) & 0x03);
              buf[5*x+2] = ((src2 << 4) & 0xf0) + ((src1 >> 6) & 0x0f);
              buf[5*x+3] = ((src3 << 6) & 0xc0) + ((src2 >> 4) & 0x3f);
              buf[5*x+4] = ((src3 >> 2) & 0xff); // src3:98765432
            }
          }
          else if (fileBitDepth == 12) //...2 values into 3 bytes
          {
            for (uint32_t x = 0; x < widthFileS; x++)
            {
              const uint32_t src0 = pSrcBuf[(2*x  ) >> sx];
              const uint32_t src1 = pSrcBuf[(2*x+1) >> sx];

              buf[3*x  ] = ((src0     ) & 0xff); // src0:76543210
              buf[3*x+1] = ((src1 << 4) & 0xf0) + ((src0 >> 8) & 0x0f);
              buf[3*x+2] = ((src1 >> 4) & 0xff); // src1:BA987654
            }
          }
        }
        else
        {
          // eg file is 422, source is 444.
          const uint32_t sx = csxFile - csxSrc;

          if (fileBitDepth == 10)  // write 4 values into 5 bytes
          {
            for (uint32_t x = 0; x < widthFileS; x++)
            {
              const uint32_t src0 = pSrcBuf[(4*x  ) << sx];
              const uint32_t src1 = pSrcBuf[(4*x+1) << sx];
              const uint32_t src2 = pSrcBuf[(4*x+2) << sx];
              const uint32_t src3 = pSrcBuf[(4*x+3) << sx];

              buf[5*x  ] = ((src0     ) & 0xff); // src0:76543210
              buf[5*x+1] = ((src1 << 2) & 0xfc) + ((src0 >> 8) & 0x03);
              buf[5*x+2] = ((src2 << 4) & 0xf0) + ((src1 >> 6) & 0x0f);
              buf[5*x+3] = ((src3 << 6) & 0xc0) + ((src2 >> 4) & 0x3f);
              buf[5*x+4] = ((src3 >> 2) & 0xff); // src3:98765432
            }
          }
          else if (fileBitDepth == 12) //...2 values into 3 bytes
          {
            for (uint32_t x = 0; x < widthFileS; x++)
            {
              const uint32_t src0 = pSrcBuf[(2*x  ) << sx];
              const uint32_t src1 = pSrcBuf[(2*x+1) << sx];

              buf[3*x  ] = ((src0     ) & 0xff); // src0:76543210
              buf[3*x+1] = ((src1 << 4) & 0xf0) + ((src0 >> 8) & 0x0f);
              buf[3*x+2] = ((src1 >> 4) & 0xff); // src1:BA987654
            }
          }
        }

        fd.write(reinterpret_cast<const char*>(buf), strideFile);
        if (fd.eof() || fd.fail())
        {
          return false;
        }
      }

      if ((y444 & maskSrcY) == 0)
      {
        pSrcBuf += srcBufStride;
      }
    }

    // here height444 and orgHeight are luma heights
    if ((compID == COMPONENT_Y) || (isChromaEnabled(fileFormat) && isChromaEnabled(srcFormat)))
    {
      for (uint32_t y444 = height444; y444 < orgHeight; y444++)
      {
        if ((y444 & maskFileY) == 0)   // if this is chroma, determine whether to skip every other row
        {
          memset(reinterpret_cast<char*>(buf), 0, strideFile);

          fd.write(reinterpret_cast<const char*>(buf), strideFile);
          if (fd.eof() || fd.fail())
          {
            return false;
          }
        }

        if ((y444 & maskSrcY) == 0)
        {
          pSrcBuf += srcBufStride;
        }
      }
    }
  }
  else // !writePYUV
    if (compID != COMPONENT_Y && (!isChromaEnabled(fileFormat) || !isChromaEnabled(srcFormat)))
    {
      if (isChromaEnabled(fileFormat))
      {
        const uint32_t value = 1 << (fileBitDepth - 1);

        for (uint32_t y = 0; y < heightFile; y++)
        {
          if (!is16bit)
          {
            uint8_t val(value);
            for (uint32_t x = 0; x < widthFile; x++)
            {
              buf[x] = val;
            }
          }
          else
          {
            uint16_t val(value);
            for (uint32_t x = 0; x < widthFile; x++)
            {
              buf[2 * x]     = (val >> 0) & 0xff;
              buf[2 * x + 1] = (val >> 8) & 0xff;
            }
          }

          fd.write(reinterpret_cast<const char*>(buf), strideFile);
          if (fd.eof() || fd.fail())
          {
            return false;
          }
        }
      }
  }
  else
  {
    const uint32_t maskFileY = (1 << csyFile) - 1;
    const uint32_t maskSrcY  = (1 << csySrc) - 1;

    for (uint32_t y444 = 0; y444 < height444; y444++)
    {
      if ((y444 & maskFileY) == 0)
      {
        // write a new line
        if (csxFile < csxSrc)
        {
          // eg file is 444, source is 422.
          const uint32_t sx = csxSrc - csxFile;
          if (!is16bit)
          {
            for (uint32_t x = 0; x < widthFile; x++)
            {
              buf[x] = (uint8_t)(pSrcBuf[x>>sx]);
            }
          }
          else
          {
            for (uint32_t x = 0; x < widthFile; x++)
            {
              buf[2*x  ] = (pSrcBuf[x>>sx]>>0) & 0xff;
              buf[2*x+1] = (pSrcBuf[x>>sx]>>8) & 0xff;
            }
          }
        }
        else
        {
          // eg file is 422, source is 444.
          const uint32_t sx = csxFile - csxSrc;
          if (!is16bit)
          {
            for (uint32_t x = 0; x < widthFile; x++)
            {
              buf[x] = (uint8_t)(pSrcBuf[x<<sx]);
            }
          }
          else
          {
            for (uint32_t x = 0; x < widthFile; x++)
            {
              buf[2*x  ] = (pSrcBuf[x<<sx]>>0) & 0xff;
              buf[2*x+1] = (pSrcBuf[x<<sx]>>8) & 0xff;
            }
          }
        }

        fd.write(reinterpret_cast<const char*>(buf), strideFile);
        if (fd.eof() || fd.fail())
        {
          return false;
        }
      }

      if ((y444 & maskSrcY) == 0)
      {
        pSrcBuf += srcBufStride;
      }
    }

    // here height444 and orgHeight are luma heights
    for( uint32_t y444 = height444; y444 < orgHeight; y444++ )
    {
      if ((y444 & maskFileY) == 0)   // if this is chroma, determine whether to skip every other row
      {
        if( !is16bit )
        {
          for (uint32_t x = 0; x < (orgWidth >> csxFile); x++)
          {
            buf[x] = 0;
          }
        }
        else
        {
          for (uint32_t x = 0; x < (orgWidth >> csxFile); x++)
          {
            buf[2 * x] = 0;
            buf[2 * x + 1] = 0;
          }
        }
        fd.write(reinterpret_cast<const char*>(buf), strideFile);
        if( fd.eof() || fd.fail() )
        {
          return false;
        }
      }

      if ((y444 & maskSrcY) == 0)
      {
        pSrcBuf += srcBufStride;
      }
    }

  }
  return true;
}

static bool writeField(std::ostream& fd, const Pel* top, const Pel* bottom, const bool is16bit,
                       const ptrdiff_t strideSrc, uint32_t width444, uint32_t height444, const ComponentID compID,
                       const ChromaFormat srcFormat, const ChromaFormat fileFormat, const uint32_t fileBitDepth,
                       const bool isTff, const uint32_t packedYUVOutputMode = 0)
{
  const uint32_t csxFile = getComponentScaleX(compID, fileFormat);
  const uint32_t csyFile = getComponentScaleY(compID, fileFormat);
  const uint32_t csxSrc  = getComponentScaleX(compID, srcFormat);
  const uint32_t csySrc  = getComponentScaleY(compID, srcFormat);

  const uint32_t widthFile  = width444 >> csxFile;
  const uint32_t heightFile = height444 >> csyFile;
  const bool     writePYUV  = (packedYUVOutputMode > 0) && (fileBitDepth == 10 || fileBitDepth == 12)
                         && ((widthFile & (1 + (fileBitDepth & 3))) == 0);
  const uint32_t strideFile =
    writePYUV ? (width444 * fileBitDepth) >> (csxFile + 3) : (width444 * (is16bit ? 2 : 1)) >> csxFile;

  std::vector<uint8_t> bufVec(strideFile * 2);
  uint8_t *buf=&(bufVec[0]);

  if (writePYUV)
  {
    // TODO
  }
  else // !writePYUV
    if (compID != COMPONENT_Y && (!isChromaEnabled(fileFormat) || !isChromaEnabled(srcFormat)))
    {
      if (isChromaEnabled(fileFormat))
      {
        const uint32_t value = 1 << (fileBitDepth - 1);

        for (uint32_t y = 0; y < heightFile; y++)
        {
          for (uint32_t field = 0; field < 2; field++)
          {
            uint8_t* fieldBuffer = buf + (field * strideFile);

            if (!is16bit)
            {
              uint8_t val(value);
              for (uint32_t x = 0; x < widthFile; x++)
              {
                fieldBuffer[x] = val;
              }
            }
            else
            {
              uint16_t val(value);
              for (uint32_t x = 0; x < widthFile; x++)
              {
                fieldBuffer[2 * x]     = (val >> 0) & 0xff;
                fieldBuffer[2 * x + 1] = (val >> 8) & 0xff;
              }
            }
          }

          fd.write(reinterpret_cast<const char*>(buf), (strideFile * 2));
          if (fd.eof() || fd.fail())
          {
            return false;
          }
        }
      }
  }
  else
  {
    const uint32_t maskFileY = (1 << csyFile) - 1;
    const uint32_t maskSrcY  = (1 << csySrc) - 1;
    for(uint32_t y444=0; y444<height444; y444++)
    {
      if ((y444 & maskFileY) == 0)
      {
        for (uint32_t field = 0; field < 2; field++)
        {
          uint8_t*   fieldBuffer = buf + (field * strideFile);
          const Pel *src     = (((field == 0) && isTff) || ((field == 1) && (!isTff))) ? top : bottom;

          // write a new line
          if (csxFile < csxSrc)
          {
            // eg file is 444, source is 422.
            const uint32_t sx = csxSrc - csxFile;
            if (!is16bit)
            {
              for (uint32_t x = 0; x < widthFile; x++)
              {
                fieldBuffer[x] = (uint8_t)(src[x>>sx]);
              }
            }
            else
            {
              for (uint32_t x = 0; x < widthFile; x++)
              {
                fieldBuffer[2*x  ] = (src[x>>sx]>>0) & 0xff;
                fieldBuffer[2*x+1] = (src[x>>sx]>>8) & 0xff;
              }
            }
          }
          else
          {
            // eg file is 422, src is 444.
            const uint32_t sx = csxFile - csxSrc;
            if (!is16bit)
            {
              for (uint32_t x = 0; x < widthFile; x++)
              {
                fieldBuffer[x] = (uint8_t)(src[x<<sx]);
              }
            }
            else
            {
              for (uint32_t x = 0; x < widthFile; x++)
              {
                fieldBuffer[2*x  ] = (src[x<<sx]>>0) & 0xff;
                fieldBuffer[2*x+1] = (src[x<<sx]>>8) & 0xff;
              }
            }
          }
        }

        fd.write(reinterpret_cast<const char*>(buf), (strideFile * 2));
        if (fd.eof() || fd.fail() )
        {
          return false;
        }
      }

      if ((y444 & maskSrcY) == 0)
      {
        top += strideSrc;
        bottom += strideSrc;
      }

    }
  }
  return true;
}

/**
 * Read one Y'CbCr frame, performing any required input scaling to change
 * from the bitdepth of the input file to the internal bit-depth.
 *
 * If a bit-depth reduction is required, and internalBitdepth >= 8, then
 * the input file is assumed to be ITU-R BT.601/709 compliant, and the
 * resulting data is clipped to the appropriate legal range, as if the
 * file had been provided at the lower-bitdepth compliant to Rec601/709.
 *
 * @param pPicYuvUser      input picture YUV buffer class pointer
 * @param pPicYuvTrueOrg
 * @param ipcsc
 * @param pad            source padding size, pad[0] = horizontal, pad[1] = vertical
 * @param format           chroma format
 * @return true for success, false in case of error
 */
bool VideoIOYuv::read(PelUnitBuf& pic, PelUnitBuf& picOrg, const InputColourSpaceConversion ipcsc, int pad[2],
                      ChromaFormat format, const bool clipToRec709)
{
  // check end-of-file
  if ( isEof() )
  {
    return false;
  }

  if (format == ChromaFormat::UNDEFINED)
  {
    format = picOrg.chromaFormat;
  }

  bool is16bit = false;

  for (const auto bd: m_fileBitdepth)
  {
    if (bd > 8)
    {
      is16bit=true;
    }
  }

  if (m_inY4mFileHeaderLength)
  {
    char frameHeader[Y4M::FRAME_HEADER_LENGTH + 1];
    m_fileStream.read(frameHeader, Y4M::FRAME_HEADER_LENGTH);
    if (m_fileStream.eof() || m_fileStream.fail())
    {
      return false;
    }
    CHECK(strncmp(frameHeader, Y4M::FRAME_HEADER, Y4M::FRAME_HEADER_LENGTH), "Wrong Y4M frame header!");
  }

  const PelBuf areaBufY = picOrg.get(COMPONENT_Y);
#if !EXTENSION_360_VIDEO
  const ptrdiff_t stride444 = areaBufY.stride;
#endif
  // compute actual YUV width & height excluding padding size
  const uint32_t padH444 = pad[0];
  const uint32_t padV444 = pad[1];

  const uint32_t widthFull444  = areaBufY.width;
  const uint32_t heightFull444 = areaBufY.height;

  const uint32_t width444  = widthFull444 - padH444;
  const uint32_t height444 = heightFull444 - padV444;

  for( uint32_t comp=0; comp < ::getNumberValidComponents(format); comp++)
  {
    const ComponentID compID = ComponentID(comp);
    const ChannelType chType=toChannelType(compID);

    const int desiredBitDepth = m_msbExtendedBitDepth[chType] + m_bitdepthShift[chType];

    const bool rec709Compliance =
      (clipToRec709)
      && (m_bitdepthShift[chType] < 0
          && desiredBitDepth >= 8); /* ITU-R BT.709 compliant clipping for converting say 10b to 8b */
    const Pel  minval           = rec709Compliance ? ((1 << (desiredBitDepth - 8))) : 0;
    const Pel  maxval           = rec709Compliance ? ((0xff << (desiredBitDepth - 8)) - 1) : (1 << desiredBitDepth) - 1;
    const bool processComponent = (size_t)compID < picOrg.bufs.size();
    Pel* const dst = processComponent ? picOrg.get(compID).bufAt(0,0) : nullptr;
#if EXTENSION_360_VIDEO
    const ptrdiff_t stride444 = picOrg.get(compID).stride;
#endif
    if (!readPlane(dst, m_fileStream, is16bit, stride444, width444, height444, padH444, padV444, compID,
                   picOrg.chromaFormat, format, m_fileBitdepth[chType]))
    {
      return false;
    }

    if (processComponent)
    {
      if (!verifyPlane(dst, stride444, width444, height444, padH444, padV444, compID, picOrg.chromaFormat,
                       m_fileBitdepth[chType]))
      {
         EXIT("Source image contains values outside the specified bit range!");
      }
#if !RExt__HIGH_BIT_DEPTH_SUPPORT
      if (m_fileBitdepth[chType] > 14 && m_bitdepthShift[chType] < 0)
      {
        EXIT("RExt__HIGH_BIT_DEPTH_SUPPORT must be enabled for bit depths above 14 if INTERNALBITDEPTH < INPUTBITDEPTH");
      }
#endif
      scalePlane(picOrg.get(compID), m_bitdepthShift[chType], minval, maxval);
    }
  }

#if EXTENSION_360_VIDEO
  if (pic.chromaFormat != ChromaFormat::NUM)
#endif
  {
    colourSpaceConvert(picOrg, pic, ipcsc, true);
  }

  picOrg.copyFrom(pic);

  return true;
}

/**
 * Write one Y'CbCr frame. No bit-depth conversion is performed, pcPicYuv is
 * assumed to be at TVideoIO::m_fileBitdepth depth.
 *
 * @param pPicYuvUser      input picture YUV buffer class pointer
 * @param ipCSC
 * @param confLeft         conformance window left border
 * @param confRight        conformance window right border
 * @param confTop          conformance window top border
 * @param confBottom       conformance window bottom border
 * @param format           chroma format
 * @return true for success, false in case of error
 */
 // here orgWidth and orgHeight are for luma
bool VideoIOYuv::write(uint32_t orgWidth, uint32_t orgHeight, const CPelUnitBuf &pic,
                       const InputColourSpaceConversion ipCSC, const bool packedYuvOutputMode, int confLeft,
                       int confRight, int confTop, int confBottom, ChromaFormat format, const bool clipToRec709,
                       const bool subtractConfWindowOffsets)
{
  PelStorage interm;

  if (ipCSC!=IPCOLOURSPACE_UNCHANGED)
  {
    interm.create( pic.chromaFormat, Area( Position(), pic.Y()) );
    colourSpaceConvert(pic, interm, ipCSC, false);
  }

  const CPelUnitBuf& picC = (ipCSC==IPCOLOURSPACE_UNCHANGED) ? pic : interm;

  // compute actual YUV frame size excluding padding size
  bool is16bit = false;
  for (const auto bd: m_fileBitdepth)
  {
    if (bd > 8)
    {
      is16bit=true;
    }
  }

  bool nonZeroBitDepthShift = false;
  for (const auto bdShift: m_bitdepthShift)
  {
    if (bdShift != 0)
    {
      nonZeroBitDepthShift=true;
    }
  }

  bool retval = true;
  if (format == ChromaFormat::UNDEFINED)
  {
    format= picC.chromaFormat;
  }

  PelStorage picZ;
  if (nonZeroBitDepthShift)
  {
    picZ.create( picC.chromaFormat, Area( Position(), picC.Y() ) );
    picZ.copyFrom( picC );

    for(uint32_t comp=0; comp < ::getNumberValidComponents( picZ.chromaFormat ); comp++)
    {
      const ComponentID compID=ComponentID(comp);
      const ChannelType ch=toChannelType(compID);
      const bool        rec709Compliance =
        clipToRec709
        && (-m_bitdepthShift[ch] < 0
            && m_msbExtendedBitDepth[ch] >= 8); /* ITU-R BT.709 compliant clipping for converting say 10b to 8b */
      const Pel minval = rec709Compliance ? ((1 << (m_msbExtendedBitDepth[ch] - 8))) : 0;
      const Pel maxval =
        rec709Compliance ? ((0xff << (m_msbExtendedBitDepth[ch] - 8)) - 1) : (1 << m_msbExtendedBitDepth[ch]) - 1;

      scalePlane(picZ.get(compID), -m_bitdepthShift[ch], minval, maxval);
    }
  }

  const CPelUnitBuf& picO = nonZeroBitDepthShift ? picZ : picC;

  const CPelBuf areaY     = picO.get(COMPONENT_Y);
  const uint32_t    width444  = areaY.width - confLeft - confRight;
  const uint32_t    height444 = areaY.height -  confTop  - confBottom;

  if( subtractConfWindowOffsets )
  {
    orgWidth -= confLeft + confRight;
    orgHeight -= confTop + confBottom;
  }

  if ((width444 == 0) || (height444 == 0))
  {
    msg( WARNING, "\nWarning: writing %d x %d luma sample output picture!", width444, height444);
  }

  if (m_outY4m)
  {
    m_fileStream.write(Y4M::FRAME_HEADER, Y4M::FRAME_HEADER_LENGTH);
  }

  for(uint32_t comp=0; retval && comp < ::getNumberValidComponents(format); comp++)
  {
    const ComponentID compID      = ComponentID(comp);
    const ChannelType ch          = toChannelType(compID);
    const uint32_t    csx         = ::getComponentScaleX(compID, format);
    const uint32_t    csy         = ::getComponentScaleY(compID, format);
    const CPelBuf     area        = picO.get(compID);
    const ptrdiff_t   planeOffset = (confLeft >> csx) + (confTop >> csy) * area.stride;
    if (!writePlane(orgWidth, orgHeight, m_fileStream, area.bufAt(0, 0) + planeOffset, is16bit, area.stride, width444,
                    height444, compID, picO.chromaFormat, format, m_fileBitdepth[ch], packedYuvOutputMode ? 1 : 0))
    {
      retval = false;
    }
  }

  return retval;
}

bool VideoIOYuv::write(const CPelUnitBuf &picTop, const CPelUnitBuf &picBottom, const InputColourSpaceConversion ipCSC,
                       const bool packedYuvOutputMode, int confLeft, int confRight, int confTop, int confBottom,
                       ChromaFormat format, const bool isTff, const bool clipToRec709)
{
  PelStorage intermTop;
  PelStorage intermBottom;

  if( ipCSC != IPCOLOURSPACE_UNCHANGED )
  {
    intermTop   .create( picTop.   chromaFormat, Area( Position(), picTop.   Y()) );
    intermBottom.create( picBottom.chromaFormat, Area( Position(), picBottom.Y()) );
    colourSpaceConvert(picTop, intermTop, ipCSC, false);
    colourSpaceConvert(picBottom, intermBottom, ipCSC, false);
  }
  const CPelUnitBuf& picTopC    = (ipCSC==IPCOLOURSPACE_UNCHANGED) ? picTop    : intermTop;
  const CPelUnitBuf& picBottomC = (ipCSC==IPCOLOURSPACE_UNCHANGED) ? picBottom : intermBottom;

  bool is16bit = false;
  for (const auto bd: m_fileBitdepth)
  {
    if (bd > 8)
    {
      is16bit=true;
    }
  }

  bool nonZeroBitDepthShift = false;
  for (const auto bdShift: m_bitdepthShift)
  {
    if (bdShift != 0)
    {
      nonZeroBitDepthShift=true;
    }
  }

  PelStorage picTopZ;
  PelStorage picBottomZ;

  for (uint32_t field = 0; field < 2; field++)
  {
    const CPelUnitBuf& picC    = (field == 0) ? picTopC : picBottomC;

    if (format == ChromaFormat::UNDEFINED)
    {
      format = picC.chromaFormat;
    }

    PelStorage& picZ    = (field == 0) ? picTopZ : picBottomZ;

    if (nonZeroBitDepthShift)
    {
      picZ.create( picC.chromaFormat, Area( Position(), picC.Y() ) );
      picZ.copyFrom( picC );

      for(uint32_t comp=0; comp < ::getNumberValidComponents( picZ.chromaFormat ); comp++)
      {
        const ComponentID compID=ComponentID(comp);
        const ChannelType ch=toChannelType(compID);
        const bool        rec709Compliance =
          clipToRec709
          && (-m_bitdepthShift[ch] < 0
              && m_msbExtendedBitDepth[ch] >= 8); /* ITU-R BT.709 compliant clipping for converting say 10b to 8b */
        const Pel minval = rec709Compliance ? ((1 << (m_msbExtendedBitDepth[ch] - 8))) : 0;
        const Pel maxval =
          rec709Compliance ? ((0xff << (m_msbExtendedBitDepth[ch] - 8)) - 1) : (1 << m_msbExtendedBitDepth[ch]) - 1;

        scalePlane(picZ.get(compID), -m_bitdepthShift[ch], minval, maxval);
      }
    }
  }

  const CPelUnitBuf& picTopO     = nonZeroBitDepthShift ? picTopZ    : picTopC;
  const CPelUnitBuf& picBottomO  = nonZeroBitDepthShift ? picBottomZ : picBottomC;

  bool retval = true;
  CHECK( picTopO.chromaFormat != picBottomO.chromaFormat, "Incompatible formats of bottom and top fields" );

  const ChromaFormat dstChrFormat = picTopO.chromaFormat;
  for (uint32_t comp = 0; retval && comp < ::getNumberValidComponents(dstChrFormat); comp++)
  {
    const ComponentID compID     = ComponentID(comp);
    const ChannelType ch         = toChannelType(compID);
    const CPelBuf     areaTop    = picTopO.   get( compID );
    const CPelBuf     areaBottom = picBottomO.get( compID );
    const CPelBuf     areaTopY   = picTopO.Y();
    const uint32_t    width444   = areaTopY.width  - (confLeft + confRight);
    const uint32_t    height444  = areaTopY.height - (confTop + confBottom);

    CHECK(areaTop.width  != areaBottom.width , "Incompatible formats");
    CHECK(areaTop.height != areaBottom.height, "Incompatible formats");
    CHECK(areaTop.stride != areaBottom.stride, "Incompatible formats");

    if ((width444 == 0) || (height444 == 0))
    {
      msg( WARNING, "\nWarning: writing %d x %d luma sample output picture!", width444, height444);
    }

    const uint32_t csx = ::getComponentScaleX(compID, dstChrFormat );
    const uint32_t csy = ::getComponentScaleY(compID, dstChrFormat );
    const ptrdiff_t planeOffset =
      (confLeft >> csx)
      + (confTop >> csy)
          * areaTop.stride;   // offset is for entire frame - round up for top field and down for bottom field

    if (!writeField(m_fileStream, (areaTop.bufAt(0, 0) + planeOffset), (areaBottom.bufAt(0, 0) + planeOffset), is16bit,
                    areaTop.stride, width444, height444, compID, dstChrFormat, format, m_fileBitdepth[ch], isTff,
                    packedYuvOutputMode ? 1 : 0))
    {
      retval=false;
    }
  }

  return retval;
}


// static member
void VideoIOYuv::colourSpaceConvert(const CPelUnitBuf& src, PelUnitBuf& dest,
                                    const InputColourSpaceConversion conversion, bool isForwards)
{
  const ChromaFormat  format       = src.chromaFormat;
  const uint32_t          numValidComp = ::getNumberValidComponents(format);

  switch (conversion)
  {
    case IPCOLOURSPACE_YCbCrtoYYY:
      if (format != ChromaFormat::_444)
      {
        // only 444 is handled.
        CHECK(format != ChromaFormat::_444, "Chroma format other than 444 not supported");
      }

      {
        for(uint32_t comp=0; comp<numValidComp; comp++)
        {
          dest.get(ComponentID(comp)).copyFrom(src.get(ComponentID(isForwards ? 0 : comp)));
        }
      }
      break;
    case IPCOLOURSPACE_YCbCrtoYCrCb:
      {
        for(uint32_t comp=0; comp<numValidComp; comp++)
        {
          dest.get( ComponentID((numValidComp-comp)%numValidComp) ).copyFrom( src.get( ComponentID(comp) ) );
        }
      }
      break;

    case IPCOLOURSPACE_RGBtoGBR:
      {
        if (format != ChromaFormat::_444)
        {
          // only 444 is handled.
          CHECK(format != ChromaFormat::_444, "Chroma format other than 444 not supported");
        }

        // channel re-mapping
        for(uint32_t comp=0; comp<numValidComp; comp++)
        {
          const ComponentID compIDsrc=ComponentID((comp+1)%numValidComp);
          const ComponentID compIDdst=ComponentID(comp);

          dest.get(isForwards ? compIDdst : compIDsrc).copyFrom(src.get(isForwards ? compIDsrc : compIDdst));
        }
      }
      break;

    case IPCOLOURSPACE_UNCHANGED:
    default:
      {
        for(uint32_t comp=0; comp<numValidComp; comp++)
        {
          dest.get( ComponentID(comp) ).copyFrom( src.get( ComponentID(comp) ) );
        }
      }
      break;
  }
}

bool VideoIOYuv::writeUpscaledPicture(const SPS &sps, const PPS &pps, const CPelUnitBuf &pic,
                                      const InputColourSpaceConversion ipCSC, const bool packedYuvOutputMode,
                                      int outputChoice, ChromaFormat format, const bool clipToRec709,
                                      int upscaleFilterForDisplay, int maxWidth, int maxHeight)
{
  ChromaFormat chromaFormatIdc = sps.getChromaFormatIdc();
  bool ret = false;

  Window afterScaleWindowFullResolution; // initialized to zero
  // keep zero window offsets when overriding SPS maxWidth/Height with user-provided parameters,
  // otherwise get info from SPS
  if (!maxWidth || !outputChoice)
  {
    maxWidth = sps.getMaxPicWidthInLumaSamples();
    afterScaleWindowFullResolution.setWindowLeftOffset(sps.getConformanceWindow().getWindowLeftOffset());
    afterScaleWindowFullResolution.setWindowRightOffset(sps.getConformanceWindow().getWindowRightOffset());
  }
  if (!maxHeight || !outputChoice)
  {
    maxHeight = sps.getMaxPicHeightInLumaSamples();
    afterScaleWindowFullResolution.setWindowTopOffset(sps.getConformanceWindow().getWindowTopOffset());
    afterScaleWindowFullResolution.setWindowBottomOffset(sps.getConformanceWindow().getWindowBottomOffset());
  }

  // decoder does not have information about upscaled picture scaling and conformance windows, store this information when full resolution picutre is encountered
  if( maxWidth == pps.getPicWidthInLumaSamples() && maxHeight == pps.getPicHeightInLumaSamples() )
  {
    afterScaleWindowFullResolution = pps.getConformanceWindow();
  }

  if( outputChoice && ( maxWidth != pic.get( COMPONENT_Y ).width || maxHeight != pic.get( COMPONENT_Y ).height ) )
  {
    if( outputChoice == 2 )
    {
      PelStorage upscaledPic;
      upscaledPic.create(chromaFormatIdc, Area(Position(), Size(maxWidth, maxHeight)));

      int curPicWidth = maxWidth   - SPS::getWinUnitX( sps.getChromaFormatIdc() ) * ( afterScaleWindowFullResolution.getWindowLeftOffset() + afterScaleWindowFullResolution.getWindowRightOffset() );
      int curPicHeight = maxHeight - SPS::getWinUnitY( sps.getChromaFormatIdc() ) * ( afterScaleWindowFullResolution.getWindowTopOffset()  + afterScaleWindowFullResolution.getWindowBottomOffset() );

      const Window& beforeScalingWindow = pps.getConformanceWindow();
      int refPicWidth = pps.getPicWidthInLumaSamples()   - SPS::getWinUnitX( sps.getChromaFormatIdc() ) * ( beforeScalingWindow.getWindowLeftOffset() + beforeScalingWindow.getWindowRightOffset() );
      int refPicHeight = pps.getPicHeightInLumaSamples() - SPS::getWinUnitY( sps.getChromaFormatIdc() ) * ( beforeScalingWindow.getWindowTopOffset()  + beforeScalingWindow.getWindowBottomOffset() );

      const int xScale = ((refPicWidth << ScalingRatio::BITS) + (curPicWidth >> 1)) / curPicWidth;
      const int yScale = ((refPicHeight << ScalingRatio::BITS) + (curPicHeight >> 1)) / curPicHeight;

      bool rescaleForDisplay = true;
      Picture::rescalePicture({ xScale, yScale }, pic, pps.getScalingWindow(), upscaledPic,
                              afterScaleWindowFullResolution, chromaFormatIdc, sps.getBitDepths(), false, false,
                              sps.getHorCollocatedChromaFlag(), sps.getVerCollocatedChromaFlag(), rescaleForDisplay,
                              upscaleFilterForDisplay);
      ret = write(maxWidth, maxHeight, upscaledPic, ipCSC,
                  packedYuvOutputMode, afterScaleWindowFullResolution.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
                  afterScaleWindowFullResolution.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
                  afterScaleWindowFullResolution.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
                  afterScaleWindowFullResolution.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc),
                  ChromaFormat::UNDEFINED, clipToRec709);
    }
    else
    {
      const Window &conf = pps.getConformanceWindow();
      int curPicWidth = maxWidth   - SPS::getWinUnitX( sps.getChromaFormatIdc() ) * ( afterScaleWindowFullResolution.getWindowLeftOffset() + afterScaleWindowFullResolution.getWindowRightOffset() );
      int curPicHeight = maxHeight - SPS::getWinUnitY( sps.getChromaFormatIdc() ) * ( afterScaleWindowFullResolution.getWindowTopOffset()  + afterScaleWindowFullResolution.getWindowBottomOffset() );

      ret = write(curPicWidth, curPicHeight, pic, ipCSC,
                  packedYuvOutputMode, conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
                  conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
                  conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
                  conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc), ChromaFormat::UNDEFINED,
                  clipToRec709, false);
    }
  }
  else
  {
    const Window &conf = pps.getConformanceWindow();

    ret =
      write(pic.get(COMPONENT_Y).width, pic.get(COMPONENT_Y).height, pic, ipCSC, packedYuvOutputMode,
            conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIdc),
            conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIdc),
            conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIdc),
            conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIdc), ChromaFormat::UNDEFINED, clipToRec709);
  }

  return ret;
}

bool isY4mFileExt(const std::string &fileName)
{
  auto pos = fileName.rfind(".y4m");
  // ".y4m" must be at the end of the file name
  return (pos != std::string::npos && pos + 4 == fileName.length());
}
