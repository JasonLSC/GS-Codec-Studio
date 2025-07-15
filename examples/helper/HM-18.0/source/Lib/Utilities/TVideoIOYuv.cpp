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

/** \file     TVideoIOYuv.cpp
    \brief    YUV file I/O class
*/

#include <cstdlib>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <memory.h>

#include "TLibCommon/TComRom.h"
#include "TVideoIOYuv.h"

using namespace std;

// ====================================================================================================================
// Local Functions
// ====================================================================================================================

/**
 * Scale all pixels in img depending upon sign of shiftbits by a factor of
 * 2<sup>shiftbits</sup>.
 *
 * @param img        pointer to image to be transformed
 * @param stride  distance between vertically adjacent pixels of img.
 * @param width   width of active area in img.
 * @param height  height of active area in img.
 * @param shiftbits if zero, no operation performed
 *                  if > 0, multiply by 2<sup>shiftbits</sup>, see scalePlane()
 *                  if < 0, divide and round by 2<sup>shiftbits</sup> and clip,
 *                          see invScalePlane().
 * @param minval  minimum clipping value when dividing.
 * @param maxval  maximum clipping value when dividing.
 */
static Void scalePlane(Pel* img, const UInt stride, const UInt width, const UInt height, Int shiftbits, Pel minval, Pel maxval)
{
  if (shiftbits > 0)
  {
    for (UInt y = 0; y < height; y++, img+=stride)
    {
      for (UInt x = 0; x < width; x++)
      {
        img[x] <<= shiftbits;
      }
    }
  }
  else if (shiftbits < 0)
  {
    shiftbits=-shiftbits;

    Pel rounding = 1 << (shiftbits-1);
    for (UInt y = 0; y < height; y++, img+=stride)
    {
      for (UInt x = 0; x < width; x++)
      {
        img[x] = Clip3(minval, maxval, Pel((img[x] + rounding) >> shiftbits));
      }
    }
  }
}

static Void
copyPlane(const TComPicYuv &src, const ComponentID srcPlane, TComPicYuv &dest, const ComponentID destPlane);

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/**
 * Open file for reading/writing Y'CbCr frames.
 *
 * Frames read/written have bitdepth fileBitDepth, and are automatically
 * formatted as 8 or 16 bit word values (see TVideoIOYuv::write()).
 *
 * Image data read or written is converted to/from internalBitDepth
 * (See scalePlane(), TVideoIOYuv::read() and TVideoIOYuv::write() for
 * further details).
 *
 * \param pchFile          file name string
 * \param bWriteMode       file open mode: true=write, false=read
 * \param fileBitDepth     bit-depth array of input/output file data.
 * \param MSBExtendedBitDepth
 * \param internalBitDepth bit-depth array to scale image data to/from when reading/writing.
 */
Void TVideoIOYuv::open( const std::string &fileName, Bool bWriteMode, const Int fileBitDepth[MAX_NUM_CHANNEL_TYPE], const Int MSBExtendedBitDepth[MAX_NUM_CHANNEL_TYPE], const Int internalBitDepth[MAX_NUM_CHANNEL_TYPE] )
{
  //NOTE: files cannot have bit depth greater than 16
  for(UInt ch=0; ch<MAX_NUM_CHANNEL_TYPE; ch++)
  {
    m_fileBitdepth       [ch] = std::min<UInt>(fileBitDepth[ch], 16);
    m_MSBExtendedBitDepth[ch] = MSBExtendedBitDepth[ch];
    m_bitdepthShift      [ch] = internalBitDepth[ch] - m_MSBExtendedBitDepth[ch];

    if (m_fileBitdepth[ch] > 16)
    {
      if (bWriteMode)
      {
        std::cerr << "\nWARNING: Cannot write a yuv file of bit depth greater than 16 - output will be right-shifted down to 16-bit precision\n" << std::endl;
      }
      else
      {
        std::cerr << "\nERROR: Cannot read a yuv file of bit depth greater than 16\n" << std::endl;
        exit(0);
      }
    }
  }

  if ( bWriteMode )
  {
    m_cHandle.open( fileName.c_str(), ios::binary | ios::out );

    if( m_cHandle.fail() )
    {
      printf("\nfailed to write reconstructed YUV file\n");
      exit(0);
    }
  }
  else
  {
    m_cHandle.open( fileName.c_str(), ios::binary | ios::in );

    if( m_cHandle.fail() )
    {
      printf("\nfailed to open Input YUV file\n");
      exit(0);
    }
  }

  return;
}

Void TVideoIOYuv::close()
{
  m_cHandle.close();
}

Bool TVideoIOYuv::isEof()
{
  return m_cHandle.eof();
}

Bool TVideoIOYuv::isFail()
{
  return m_cHandle.fail();
}

/**
 * Skip numFrames in input.
 *
 * This function correctly handles cases where the input file is not
 * seekable, by consuming bytes.
 */
Void TVideoIOYuv::skipFrames(Int numFrames, UInt width, UInt height, ChromaFormat format)
{
  if (numFrames==0)
  {
    return;
  }

  //------------------
  //set the frame size according to the chroma format
  streamoff frameSize = 0;
  UInt wordsize=1; // default to 8-bit, unless a channel with more than 8-bits is detected.
  for (UInt component = 0; component < getNumberValidComponents(format); component++)
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

  const streamoff offset = frameSize * numFrames;

  /* attempt to seek */
  if (!!m_cHandle.seekg(offset, ios::cur))
  {
    return; /* success */
  }
  m_cHandle.clear();

  /* fall back to consuming the input */
  TChar buf[512];
  const streamoff offset_mod_bufsize = offset % sizeof(buf);
  for (streamoff i = 0; i < offset - offset_mod_bufsize; i += sizeof(buf))
  {
    m_cHandle.read(buf, sizeof(buf));
  }
  m_cHandle.read(buf, offset_mod_bufsize);
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
static Bool readPlane(Pel* dst,
                      istream& fd,
                      Bool is16bit,
                      UInt stride444,
                      UInt width444,
                      UInt height444,
                      UInt pad_x444,
                      UInt pad_y444,
                      const ComponentID compID,
                      const ChromaFormat destFormat,
                      const ChromaFormat fileFormat,
                      const UInt fileBitDepth)
{
  const UInt csx_file =getComponentScaleX(compID, fileFormat);
  const UInt csy_file =getComponentScaleY(compID, fileFormat);
  const UInt csx_dest =getComponentScaleX(compID, destFormat);
  const UInt csy_dest =getComponentScaleY(compID, destFormat);

  const UInt width_dest       = width444 >>csx_dest;
  const UInt height_dest      = height444>>csy_dest;
  const UInt pad_x_dest       = pad_x444>>csx_dest;
  const UInt pad_y_dest       = pad_y444>>csy_dest;
  const UInt stride_dest      = stride444>>csx_dest;

  const UInt full_width_dest  = width_dest+pad_x_dest;
  const UInt full_height_dest = height_dest+pad_y_dest;

  const UInt stride_file      = (width444 * (is16bit ? 2 : 1)) >> csx_file;
  std::vector<UChar> bufVec(stride_file);
  UChar *buf=&(bufVec[0]);

  if (compID!=COMPONENT_Y && (fileFormat==CHROMA_400 || destFormat==CHROMA_400))
  {
    if (destFormat!=CHROMA_400)
    {
      // set chrominance data to mid-range: (1<<(fileBitDepth-1))
      const Pel value=Pel(1<<(fileBitDepth-1));
      for (UInt y = 0; y < full_height_dest; y++, dst+=stride_dest)
      {
        for (UInt x = 0; x < full_width_dest; x++)
        {
          dst[x] = value;
        }
      }
    }

    if (fileFormat!=CHROMA_400)
    {
      const UInt height_file      = height444>>csy_file;
      fd.seekg(height_file*stride_file, ios::cur);
      if (fd.eof() || fd.fail() )
      {
        return false;
      }
    }
  }
  else
  {
    const UInt mask_y_file=(1<<csy_file)-1;
    const UInt mask_y_dest=(1<<csy_dest)-1;
    for(UInt y444=0; y444<height444; y444++)
    {
      if ((y444&mask_y_file)==0)
      {
        // read a new line
        fd.read(reinterpret_cast<TChar*>(buf), stride_file);
        if (fd.eof() || fd.fail() )
        {
          return false;
        }
      }

      if ((y444&mask_y_dest)==0)
      {
        // process current destination line
        if (csx_file < csx_dest)
        {
          // eg file is 444, dest is 422.
          const UInt sx=csx_dest-csx_file;
          if (!is16bit)
          {
            for (UInt x = 0; x < width_dest; x++)
            {
              dst[x] = buf[x<<sx];
            }
          }
          else
          {
            for (UInt x = 0; x < width_dest; x++)
            {
              dst[x] = Pel(buf[(x<<sx)*2+0]) | (Pel(buf[(x<<sx)*2+1])<<8);
            }
          }
        }
        else
        {
          // eg file is 422, dest is 444.
          const UInt sx=csx_file-csx_dest;
          if (!is16bit)
          {
            for (UInt x = 0; x < width_dest; x++)
            {
              dst[x] = buf[x>>sx];
            }
          }
          else
          {
            for (UInt x = 0; x < width_dest; x++)
            {
              dst[x] = Pel(buf[(x>>sx)*2+0]) | (Pel(buf[(x>>sx)*2+1])<<8);
            }
          }
        }

        // process right hand side padding
        const Pel val=dst[width_dest-1];
        for (UInt x = width_dest; x < full_width_dest; x++)
        {
          dst[x] = val;
        }

        dst += stride_dest;
      }
    }

    // process lower padding
    for (UInt y = height_dest; y < full_height_dest; y++, dst+=stride_dest)
    {
      for (UInt x = 0; x < full_width_dest; x++)
      {
        dst[x] = (dst - stride_dest)[x];
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
static Bool writePlane(ostream& fd, Pel* src, Bool is16bit,
                       UInt stride444,
                       UInt width444, UInt height444,
                       const ComponentID compID,
                       const ChromaFormat srcFormat,
                       const ChromaFormat fileFormat,
                       const UInt fileBitDepth)
{
  const UInt csx_file =getComponentScaleX(compID, fileFormat);
  const UInt csy_file =getComponentScaleY(compID, fileFormat);
  const UInt csx_src  =getComponentScaleX(compID, srcFormat);
  const UInt csy_src  =getComponentScaleY(compID, srcFormat);

  const UInt stride_src      = stride444>>csx_src;

  const UInt stride_file      = (width444 * (is16bit ? 2 : 1)) >> csx_file;
  const UInt width_file       = width444 >>csx_file;
  const UInt height_file      = height444>>csy_file;

  std::vector<UChar> bufVec(stride_file);
  UChar *buf=&(bufVec[0]);

  if (compID!=COMPONENT_Y && (fileFormat==CHROMA_400 || srcFormat==CHROMA_400))
  {
    if (fileFormat!=CHROMA_400)
    {
      const UInt value=1<<(fileBitDepth-1);

      for(UInt y=0; y< height_file; y++)
      {
        if (!is16bit)
        {
          UChar val(value);
          for (UInt x = 0; x < width_file; x++)
          {
            buf[x]=val;
          }
        }
        else
        {
          UShort val(value);
          for (UInt x = 0; x < width_file; x++)
          {
            buf[2*x+0]= (val>>0) & 0xff;
            buf[2*x+1]= (val>>8) & 0xff;
          }
        }

        fd.write(reinterpret_cast<const TChar*>(buf), stride_file);
        if (fd.eof() || fd.fail() )
        {
          return false;
        }
      }
    }
  }
  else
  {
    const UInt mask_y_file=(1<<csy_file)-1;
    const UInt mask_y_src =(1<<csy_src )-1;
    for(UInt y444=0; y444<height444; y444++)
    {
      if ((y444&mask_y_file)==0)
      {
        // write a new line
        if (csx_file < csx_src)
        {
          // eg file is 444, source is 422.
          const UInt sx=csx_src-csx_file;
          if (!is16bit)
          {
            for (UInt x = 0; x < width_file; x++)
            {
              buf[x] = (UChar)(src[x>>sx]);
            }
          }
          else
          {
            for (UInt x = 0; x < width_file; x++)
            {
              buf[2*x  ] = (src[x>>sx]>>0) & 0xff;
              buf[2*x+1] = (src[x>>sx]>>8) & 0xff;
            }
          }
        }
        else
        {
          // eg file is 422, src is 444.
          const UInt sx=csx_file-csx_src;
          if (!is16bit)
          {
            for (UInt x = 0; x < width_file; x++)
            {
              buf[x] = (UChar)(src[x<<sx]);
            }
          }
          else
          {
            for (UInt x = 0; x < width_file; x++)
            {
              buf[2*x  ] = (src[x<<sx]>>0) & 0xff;
              buf[2*x+1] = (src[x<<sx]>>8) & 0xff;
            }
          }
        }

        fd.write(reinterpret_cast<const TChar*>(buf), stride_file);
        if (fd.eof() || fd.fail() )
        {
          return false;
        }
      }

      if ((y444&mask_y_src)==0)
      {
        src += stride_src;
      }

    }
  }
  return true;
}

static Bool writeField(ostream& fd, Pel* top, Pel* bottom, Bool is16bit,
                       UInt stride444,
                       UInt width444, UInt height444,
                       const ComponentID compID,
                       const ChromaFormat srcFormat,
                       const ChromaFormat fileFormat,
                       const UInt fileBitDepth, const Bool isTff)
{
  const UInt csx_file =getComponentScaleX(compID, fileFormat);
  const UInt csy_file =getComponentScaleY(compID, fileFormat);
  const UInt csx_src  =getComponentScaleX(compID, srcFormat);
  const UInt csy_src  =getComponentScaleY(compID, srcFormat);

  const UInt stride_src      = stride444>>csx_src;

  const UInt stride_file      = (width444 * (is16bit ? 2 : 1)) >> csx_file;
  const UInt width_file       = width444 >>csx_file;
  const UInt height_file      = height444>>csy_file;

  std::vector<UChar> bufVec(stride_file * 2);
  UChar *buf=&(bufVec[0]);

  if (compID!=COMPONENT_Y && (fileFormat==CHROMA_400 || srcFormat==CHROMA_400))
  {
    if (fileFormat!=CHROMA_400)
    {
      const UInt value=1<<(fileBitDepth-1);

      for(UInt y=0; y< height_file; y++)
      {
        for (UInt field = 0; field < 2; field++)
        {
          UChar *fieldBuffer = buf + (field * stride_file);

          if (!is16bit)
          {
            UChar val(value);
            for (UInt x = 0; x < width_file; x++)
            {
              fieldBuffer[x]=val;
            }
          }
          else
          {
            UShort val(value);
            for (UInt x = 0; x < width_file; x++)
            {
              fieldBuffer[2*x+0]= (val>>0) & 0xff;
              fieldBuffer[2*x+1]= (val>>8) & 0xff;
            }
          }
        }

        fd.write(reinterpret_cast<const TChar*>(buf), (stride_file * 2));
        if (fd.eof() || fd.fail() )
        {
          return false;
        }
      }
    }
  }
  else
  {
    const UInt mask_y_file=(1<<csy_file)-1;
    const UInt mask_y_src =(1<<csy_src )-1;
    for(UInt y444=0; y444<height444; y444++)
    {
      if ((y444&mask_y_file)==0)
      {
        for (UInt field = 0; field < 2; field++)
        {
          UChar *fieldBuffer = buf + (field * stride_file);
          Pel   *src         = (((field == 0) && isTff) || ((field == 1) && (!isTff))) ? top : bottom;

          // write a new line
          if (csx_file < csx_src)
          {
            // eg file is 444, source is 422.
            const UInt sx=csx_src-csx_file;
            if (!is16bit)
            {
              for (UInt x = 0; x < width_file; x++)
              {
                fieldBuffer[x] = (UChar)(src[x>>sx]);
              }
            }
            else
            {
              for (UInt x = 0; x < width_file; x++)
              {
                fieldBuffer[2*x  ] = (src[x>>sx]>>0) & 0xff;
                fieldBuffer[2*x+1] = (src[x>>sx]>>8) & 0xff;
              }
            }
          }
          else
          {
            // eg file is 422, src is 444.
            const UInt sx=csx_file-csx_src;
            if (!is16bit)
            {
              for (UInt x = 0; x < width_file; x++)
              {
                fieldBuffer[x] = (UChar)(src[x<<sx]);
              }
            }
            else
            {
              for (UInt x = 0; x < width_file; x++)
              {
                fieldBuffer[2*x  ] = (src[x<<sx]>>0) & 0xff;
                fieldBuffer[2*x+1] = (src[x<<sx]>>8) & 0xff;
              }
            }
          }
        }

        fd.write(reinterpret_cast<const TChar*>(buf), (stride_file * 2));
        if (fd.eof() || fd.fail() )
        {
          return false;
        }
      }

      if ((y444&mask_y_src)==0)
      {
        top    += stride_src;
        bottom += stride_src;
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
 * @param aiPad            source padding size, aiPad[0] = horizontal, aiPad[1] = vertical
 * @param format           chroma format
 * @return true for success, false in case of error
 */
Bool TVideoIOYuv::read ( TComPicYuv*  pPicYuvUser, TComPicYuv* pPicYuvTrueOrg, const InputColourSpaceConversion ipcsc, Int aiPad[2], ChromaFormat format, const Bool bClipToRec709 )
{
  // check end-of-file
  if ( isEof() )
  {
    return false;
  }
  TComPicYuv *pPicYuv=pPicYuvTrueOrg;
  if (format>=NUM_CHROMA_FORMAT)
  {
    format=pPicYuv->getChromaFormat();
  }

  Bool is16bit = false;

  for(UInt ch=0; ch<MAX_NUM_CHANNEL_TYPE; ch++)
  {
    if (m_fileBitdepth[ch] > 8)
    {
      is16bit=true;
    }
  }

  const UInt stride444      = pPicYuv->getStride(COMPONENT_Y);

  // compute actual YUV width & height excluding padding size
  const UInt pad_h444       = aiPad[0];
  const UInt pad_v444       = aiPad[1];

  const UInt width_full444  = pPicYuv->getWidth(COMPONENT_Y);
  const UInt height_full444 = pPicYuv->getHeight(COMPONENT_Y);

  const UInt width444       = width_full444 - pad_h444;
  const UInt height444      = height_full444 - pad_v444;

  for(UInt comp=0; comp<MAX_NUM_COMPONENT; comp++)
  {
    const ComponentID compID = ComponentID(comp);
    const ChannelType chType=toChannelType(compID);

    const Int desired_bitdepth = m_MSBExtendedBitDepth[chType] + m_bitdepthShift[chType];

    const Bool b709Compliance=(bClipToRec709) && (m_bitdepthShift[chType] < 0 && desired_bitdepth >= 8);     /* ITU-R BT.709 compliant clipping for converting say 10b to 8b */
    const Pel minval = b709Compliance? ((   1 << (desired_bitdepth - 8))   ) : 0;
    const Pel maxval = b709Compliance? ((0xff << (desired_bitdepth - 8)) -1) : (1 << desired_bitdepth) - 1;

    if (! readPlane(pPicYuv->getAddr(compID), m_cHandle, is16bit, stride444, width444, height444, pad_h444, pad_v444, compID, pPicYuv->getChromaFormat(), format, m_fileBitdepth[chType]))
    {
      return false;
    }

    if (compID < pPicYuv->getNumberValidComponents() )
    {
      const UInt csx=getComponentScaleX(compID, pPicYuv->getChromaFormat());
      const UInt csy=getComponentScaleY(compID, pPicYuv->getChromaFormat());
      scalePlane(pPicYuv->getAddr(compID), stride444>>csx, width_full444>>csx, height_full444>>csy, m_bitdepthShift[chType], minval, maxval);
    }
  }

  if(pPicYuvUser)
  {
    ColourSpaceConvert(*pPicYuvTrueOrg, *pPicYuvUser, ipcsc, true);
  }

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
Bool TVideoIOYuv::write( TComPicYuv* pPicYuvUser, const InputColourSpaceConversion ipCSC, Int confLeft, Int confRight, Int confTop, Int confBottom, ChromaFormat format, const Bool bClipToRec709 )
{
  TComPicYuv cPicYuvCSCd;
  if (ipCSC!=IPCOLOURSPACE_UNCHANGED)
  {
    cPicYuvCSCd.createWithoutCUInfo(pPicYuvUser->getWidth(COMPONENT_Y), pPicYuvUser->getHeight(COMPONENT_Y), pPicYuvUser->getChromaFormat() );
    ColourSpaceConvert(*pPicYuvUser, cPicYuvCSCd, ipCSC, false);
  }
  TComPicYuv *pPicYuv=(ipCSC==IPCOLOURSPACE_UNCHANGED) ? pPicYuvUser : &cPicYuvCSCd;

  // compute actual YUV frame size excluding padding size
  Bool is16bit = false;
  Bool nonZeroBitDepthShift=false;

  for(UInt ch=0; ch<MAX_NUM_CHANNEL_TYPE; ch++)
  {
    if (m_fileBitdepth[ch] > 8)
    {
      is16bit=true;
    }
    if (m_bitdepthShift[ch] != 0)
    {
      nonZeroBitDepthShift=true;
    }
  }

  TComPicYuv *dstPicYuv = NULL;
  Bool retval = true;
  if (format>=NUM_CHROMA_FORMAT)
  {
    format=pPicYuv->getChromaFormat();
  }

  if (nonZeroBitDepthShift)
  {
    dstPicYuv = new TComPicYuv;
    dstPicYuv->createWithoutCUInfo( pPicYuv->getWidth(COMPONENT_Y), pPicYuv->getHeight(COMPONENT_Y), pPicYuv->getChromaFormat() );

    for(UInt comp=0; comp<dstPicYuv->getNumberValidComponents(); comp++)
    {
      const ComponentID compID=ComponentID(comp);
      const ChannelType ch=toChannelType(compID);
      const Bool b709Compliance = bClipToRec709 && (-m_bitdepthShift[ch] < 0 && m_MSBExtendedBitDepth[ch] >= 8);     /* ITU-R BT.709 compliant clipping for converting say 10b to 8b */
      const Pel minval = b709Compliance? ((   1 << (m_MSBExtendedBitDepth[ch] - 8))   ) : 0;
      const Pel maxval = b709Compliance? ((0xff << (m_MSBExtendedBitDepth[ch] - 8)) -1) : (1 << m_MSBExtendedBitDepth[ch]) - 1;

      copyPlane(*pPicYuv, compID, *dstPicYuv, compID);
      scalePlane(dstPicYuv->getAddr(compID), dstPicYuv->getStride(compID), dstPicYuv->getWidth(compID), dstPicYuv->getHeight(compID), -m_bitdepthShift[ch], minval, maxval);
    }
  }
  else
  {
    dstPicYuv = pPicYuv;
  }

  const Int  stride444 = dstPicYuv->getStride(COMPONENT_Y);
  const UInt width444  = dstPicYuv->getWidth(COMPONENT_Y) - confLeft - confRight;
  const UInt height444 = dstPicYuv->getHeight(COMPONENT_Y) -  confTop  - confBottom;

  if ((width444 == 0) || (height444 == 0))
  {
    printf ("\nWarning: writing %d x %d luma sample output picture!", width444, height444);
  }

  for(UInt comp=0; retval && comp<dstPicYuv->getNumberValidComponents(); comp++)
  {
    const ComponentID compID = ComponentID(comp);
    const ChannelType ch=toChannelType(compID);
    const UInt csx = dstPicYuv->getComponentScaleX(compID);
    const UInt csy = dstPicYuv->getComponentScaleY(compID);
    const Int planeOffset =  (confLeft>>csx) + (confTop>>csy) * dstPicYuv->getStride(compID);
    if (! writePlane(m_cHandle, dstPicYuv->getAddr(compID) + planeOffset, is16bit, stride444, width444, height444, compID, dstPicYuv->getChromaFormat(), format, m_fileBitdepth[ch]))
    {
      retval=false;
    }
  }

  if (nonZeroBitDepthShift)
  {
    dstPicYuv->destroy();
    delete dstPicYuv;
  }

  cPicYuvCSCd.destroy();

  return retval;
}

Bool TVideoIOYuv::write( TComPicYuv* pPicYuvUserTop, TComPicYuv* pPicYuvUserBottom, const InputColourSpaceConversion ipCSC, Int confLeft, Int confRight, Int confTop, Int confBottom, ChromaFormat format, const Bool isTff, const Bool bClipToRec709 )
{

  TComPicYuv cPicYuvTopCSCd;
  TComPicYuv cPicYuvBottomCSCd;
  if (ipCSC!=IPCOLOURSPACE_UNCHANGED)
  {
    cPicYuvTopCSCd   .createWithoutCUInfo(pPicYuvUserTop   ->getWidth(COMPONENT_Y), pPicYuvUserTop   ->getHeight(COMPONENT_Y), pPicYuvUserTop   ->getChromaFormat() );
    cPicYuvBottomCSCd.createWithoutCUInfo(pPicYuvUserBottom->getWidth(COMPONENT_Y), pPicYuvUserBottom->getHeight(COMPONENT_Y), pPicYuvUserBottom->getChromaFormat() );
    ColourSpaceConvert(*pPicYuvUserTop,    cPicYuvTopCSCd,    ipCSC, false);
    ColourSpaceConvert(*pPicYuvUserBottom, cPicYuvBottomCSCd, ipCSC, false);
  }
  TComPicYuv *pPicYuvTop    = (ipCSC==IPCOLOURSPACE_UNCHANGED) ? pPicYuvUserTop    : &cPicYuvTopCSCd;
  TComPicYuv *pPicYuvBottom = (ipCSC==IPCOLOURSPACE_UNCHANGED) ? pPicYuvUserBottom : &cPicYuvBottomCSCd;

  Bool is16bit = false;
  Bool nonZeroBitDepthShift=false;

  for(UInt ch=0; ch<MAX_NUM_CHANNEL_TYPE; ch++)
  {
    if (m_fileBitdepth[ch] > 8)
    {
      is16bit=true;
    }
    if (m_bitdepthShift[ch] != 0)
    {
      nonZeroBitDepthShift=true;
    }
  }

  TComPicYuv *dstPicYuvTop    = NULL;
  TComPicYuv *dstPicYuvBottom = NULL;

  for (UInt field = 0; field < 2; field++)
  {
    TComPicYuv *pPicYuv = (field == 0) ? pPicYuvTop : pPicYuvBottom;

    if (format>=NUM_CHROMA_FORMAT)
    {
      format=pPicYuv->getChromaFormat();
    }

    TComPicYuv* &dstPicYuv = (field == 0) ? dstPicYuvTop : dstPicYuvBottom;

    if (nonZeroBitDepthShift)
    {
      dstPicYuv = new TComPicYuv;
      dstPicYuv->createWithoutCUInfo( pPicYuv->getWidth(COMPONENT_Y), pPicYuv->getHeight(COMPONENT_Y), pPicYuv->getChromaFormat() );

      for(UInt comp=0; comp<dstPicYuv->getNumberValidComponents(); comp++)
      {
        const ComponentID compID=ComponentID(comp);
        const ChannelType ch=toChannelType(compID);
        const Bool b709Compliance=bClipToRec709 && (-m_bitdepthShift[ch] < 0 && m_MSBExtendedBitDepth[ch] >= 8);     /* ITU-R BT.709 compliant clipping for converting say 10b to 8b */
        const Pel minval = b709Compliance? ((   1 << (m_MSBExtendedBitDepth[ch] - 8))   ) : 0;
        const Pel maxval = b709Compliance? ((0xff << (m_MSBExtendedBitDepth[ch] - 8)) -1) : (1 << m_MSBExtendedBitDepth[ch]) - 1;

        copyPlane(*pPicYuv, compID, *dstPicYuv, compID);
        scalePlane(dstPicYuv->getAddr(compID), dstPicYuv->getStride(compID), dstPicYuv->getWidth(compID), dstPicYuv->getHeight(compID), -m_bitdepthShift[ch], minval, maxval);
      }
    }
    else
    {
      dstPicYuv = pPicYuv;
    }
  }

  Bool retval = true;

  assert(dstPicYuvTop->getNumberValidComponents() == dstPicYuvBottom->getNumberValidComponents());
  assert(dstPicYuvTop->getChromaFormat()          == dstPicYuvBottom->getChromaFormat()         );

  for(UInt comp=0; retval && comp<dstPicYuvTop->getNumberValidComponents(); comp++)
  {
    const ComponentID compID = ComponentID(comp);
    const ChannelType ch=toChannelType(compID);

    assert(dstPicYuvTop->getWidth          (compID) == dstPicYuvBottom->getWidth          (compID));
    assert(dstPicYuvTop->getHeight         (compID) == dstPicYuvBottom->getHeight         (compID));
    assert(dstPicYuvTop->getComponentScaleX(compID) == dstPicYuvBottom->getComponentScaleX(compID));
    assert(dstPicYuvTop->getComponentScaleY(compID) == dstPicYuvBottom->getComponentScaleY(compID));
    assert(dstPicYuvTop->getStride         (compID) == dstPicYuvBottom->getStride         (compID));

    const UInt width444   = dstPicYuvTop->getWidth(COMPONENT_Y)  - (confLeft + confRight);
    const UInt height444  = dstPicYuvTop->getHeight(COMPONENT_Y) - (confTop + confBottom);

    if ((width444 == 0) || (height444 == 0))
    {
      printf ("\nWarning: writing %d x %d luma sample output picture!", width444, height444);
    }

    const UInt csx = dstPicYuvTop->getComponentScaleX(compID);
    const UInt csy = dstPicYuvTop->getComponentScaleY(compID);
    const Int planeOffset  = (confLeft>>csx) + ( confTop>>csy) * dstPicYuvTop->getStride(compID); //offset is for entire frame - round up for top field and down for bottom field

    if (! writeField(m_cHandle,
                     (dstPicYuvTop   ->getAddr(compID) + planeOffset),
                     (dstPicYuvBottom->getAddr(compID) + planeOffset),
                     is16bit,
                     dstPicYuvTop->getStride(COMPONENT_Y),
                     width444, height444, compID, dstPicYuvTop->getChromaFormat(), format, m_fileBitdepth[ch], isTff))
    {
      retval=false;
    }
  }

  if (nonZeroBitDepthShift)
  {
    dstPicYuvTop->destroy();
    dstPicYuvBottom->destroy();
    delete dstPicYuvTop;
    delete dstPicYuvBottom;
  }

  cPicYuvTopCSCd.destroy();
  cPicYuvBottomCSCd.destroy();

  return retval;
}

static Void
copyPlane(const TComPicYuv &src, const ComponentID srcPlane, TComPicYuv &dest, const ComponentID destPlane)
{
  const UInt width=src.getWidth(srcPlane);
  const UInt height=src.getHeight(srcPlane);
  assert(dest.getWidth(destPlane) == width);
  assert(dest.getHeight(destPlane) == height);
  const Pel *pSrc=src.getAddr(srcPlane);
  Pel *pDest=dest.getAddr(destPlane);
  const UInt strideSrc=src.getStride(srcPlane);
  const UInt strideDest=dest.getStride(destPlane);
  for(UInt y=0; y<height; y++, pSrc+=strideSrc, pDest+=strideDest)
  {
    memcpy(pDest, pSrc, width*sizeof(Pel));
  }
}

// static member
Void TVideoIOYuv::ColourSpaceConvert(const TComPicYuv &src, TComPicYuv &dest, const InputColourSpaceConversion conversion, Bool bIsForwards)
{
  const ChromaFormat  format=src.getChromaFormat();
  const UInt          numValidComp=src.getNumberValidComponents();

  switch (conversion)
  {
    case IPCOLOURSPACE_YCbCrtoYYY:
      if (format!=CHROMA_444)
      {
        // only 444 is handled.
        assert(format==CHROMA_444);
        exit(1);
      }

      {
        for(UInt comp=0; comp<numValidComp; comp++)
        {
          copyPlane(src, ComponentID(bIsForwards?0:comp), dest, ComponentID(comp));
        }
      }
      break;
    case IPCOLOURSPACE_YCbCrtoYCrCb:
      {
        for(UInt comp=0; comp<numValidComp; comp++)
        {
          copyPlane(src, ComponentID(comp), dest, ComponentID((numValidComp-comp)%numValidComp));
        }
      }
      break;

    case IPCOLOURSPACE_RGBtoGBR:
      {
        if (format!=CHROMA_444)
        {
          // only 444 is handled.
          assert(format==CHROMA_444);
          exit(1);
        }

        // channel re-mapping
        for(UInt comp=0; comp<numValidComp; comp++)
        {
          const ComponentID compIDsrc=ComponentID((comp+1)%numValidComp);
          const ComponentID compIDdst=ComponentID(comp);
          copyPlane(src, bIsForwards?compIDsrc:compIDdst, dest, bIsForwards?compIDdst:compIDsrc);
        }
      }
      break;

    case IPCOLOURSPACE_UNCHANGED:
    default:
      {
        for(UInt comp=0; comp<numValidComp; comp++)
        {
          copyPlane(src, ComponentID(comp), dest, ComponentID(comp));
        }
      }
      break;
  }
}
