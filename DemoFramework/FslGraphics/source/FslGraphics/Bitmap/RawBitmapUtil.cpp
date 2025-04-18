/****************************************************************************************************************************************************
 * Copyright (c) 2014 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslBase/UncheckedNumericCast.hpp>
#include <FslGraphics/Bitmap/RawBitmapEx.hpp>
#include <FslGraphics/Bitmap/RawBitmapUtil.hpp>
#include <FslGraphics/Exceptions.hpp>
#include <FslGraphics/PixelFormatUtil.hpp>
#include <cassert>
#include <cstring>

namespace Fsl
{
  int RawBitmapUtil::CalcAlignment(const PixelFormat pixelFormat, const uint32_t width, const uint32_t stride, const uint32_t bytesPerPixel)
  {
    FSL_PARAM_NOT_USED(pixelFormat);
    assert(PixelFormatUtil::GetBytesPerPixel(pixelFormat) == bytesPerPixel);

    const uint32_t minStride = width * bytesPerPixel;
    const uint32_t minStride8 = minStride + (((minStride & 7) != 0 ? (8 - (minStride & 7)) : 0));
    const uint32_t minStride4 = minStride + (((minStride & 3) != 0 ? (4 - (minStride & 3)) : 0));
    const uint32_t minStride2 = minStride + (minStride & 1);

    // Check for 8 byte alignment
    if ((stride & 7) == 0 && stride == minStride8)
    {
      return 8;
    }
    if ((stride & 3) == 0 && stride == minStride4)
    {
      return 4;
    }
    if ((stride & 1) == 0 && stride == minStride2)
    {
      return 2;
    }

    if (stride != minStride)
    {
      throw UnsupportedStrideExceptionEx("Stride does not fit with any supported alignment", stride);
    }
    return 1;
  }


  void RawBitmapUtil::CheckIsUsingMinimumStrideForAlignment(const ReadOnlyRawBitmap& rawBitmap, const uint32_t alignment)
  {
    const uint32_t bytesPerPixel = PixelFormatUtil::GetBytesPerPixel(rawBitmap.GetPixelFormat());
    const uint32_t actualMinStride = rawBitmap.RawUnsignedWidth() * bytesPerPixel;
    const uint32_t stride = rawBitmap.Stride();

    uint32_t minStride = 0;
    switch (alignment)
    {
    case 1:
      minStride = actualMinStride;
      break;
    case 2:
      minStride = actualMinStride + (actualMinStride & 1);
      break;
    case 4:
      minStride = actualMinStride + (((actualMinStride & 3) != 0 ? (4 - (actualMinStride & 3)) : 0));
      break;
    case 8:
      minStride = actualMinStride + (((actualMinStride & 7) != 0 ? (8 - (actualMinStride & 7)) : 0));
      break;
    default:
      throw UnsupportedAlignmentException("Alignment not supported", UncheckedNumericCast<int>(alignment));
    }

    if (stride != minStride)
    {
      throw UnsupportedStrideException("Stride does not fit the supported alignment", UncheckedNumericCast<int32_t>(stride));
    }
  }


  void RawBitmapUtil::Swizzle(RawBitmapEx& rBitmap, const uint32_t srcIdx0, const uint32_t srcIdx1, const uint32_t srcIdx2, const uint32_t srcIdx3)
  {
    if (!rBitmap.IsValid())
    {
      throw std::invalid_argument("Swizzle requires a valid bitmap");
    }

    switch (rBitmap.GetPixelFormatLayout())
    {
    case PixelFormatLayout::R8G8B8:
    case PixelFormatLayout::B8G8R8:
      Swizzle24(rBitmap, srcIdx0, srcIdx1, srcIdx2);
      break;
    case PixelFormatLayout::R8G8B8A8:
    case PixelFormatLayout::B8G8R8A8:
      Swizzle32(rBitmap, srcIdx0, srcIdx1, srcIdx2, srcIdx3);
      break;
    default:
      throw UnsupportedPixelFormatException("Swizzle only supports R8G8B8, B8G8R8, R8G8B8A8 or B8G8R8A8 pixel layout", rBitmap.GetPixelFormat());
    }
  }

  void RawBitmapUtil::Swizzle24From012To210(RawBitmapEx& rBitmap)
  {
    Swizzle24From012To210(rBitmap, rBitmap);
  }


  void RawBitmapUtil::Swizzle24(RawBitmapEx& rBitmap, const uint32_t srcIdx0, const uint32_t srcIdx1, const uint32_t srcIdx2)
  {
    if (srcIdx0 == 2 && srcIdx1 == 1 && srcIdx2 == 0)
    {
      Swizzle24From012To210(rBitmap);
    }
    else
    {
      Swizzle24(rBitmap, rBitmap, srcIdx0, srcIdx1, srcIdx2);
    }
  }


  void RawBitmapUtil::Swizzle32(RawBitmapEx& rBitmap, const uint32_t srcIdx0, const uint32_t srcIdx1, const uint32_t srcIdx2, const uint32_t srcIdx3)
  {
    Swizzle32(rBitmap, rBitmap, srcIdx0, srcIdx1, srcIdx2, srcIdx3);
  }


  void RawBitmapUtil::Swizzle32To24(RawBitmapEx& rBitmap, const PixelFormat dstPixelFormat, const uint32_t dstStride, const uint32_t srcIdx0,
                                    const uint32_t srcIdx1, const uint32_t srcIdx2)
  {
    ReadOnlyRawBitmap srcBitmap(rBitmap);    // NOLINT(cppcoreguidelines-slicing)
    rBitmap.SetStride(dstStride);
    rBitmap.SetPixelFormat(dstPixelFormat);

    Swizzle32To24(rBitmap, srcBitmap, srcIdx0, srcIdx1, srcIdx2);
  }


  void RawBitmapUtil::Swizzle32To8(RawBitmapEx& rBitmap, const PixelFormat dstPixelFormat, const uint32_t dstStride, const uint32_t srcIdx0)
  {
    if (!rBitmap.IsValid())
    {
      throw std::invalid_argument("Swizzle requires a valid bitmap");
    }
    if (rBitmap.GetPixelFormatLayout() != PixelFormatLayout::R8G8B8A8 && rBitmap.GetPixelFormatLayout() != PixelFormatLayout::B8G8R8A8)
    {
      throw UnsupportedPixelFormatException("Swizzle32To8 only supports R8G8B8A8 or B8G8R8A8 format layout", rBitmap.GetPixelFormat());
    }
    if (PixelFormatUtil::GetBytesPerPixel(dstPixelFormat) != 1)
    {
      throw UnsupportedPixelFormatException("Swizzle32To8 only supports 8bit dstPixelFormats", dstPixelFormat);
    }

    // The Swizzle32To8 method relies on the dstStride being <= the existing stride to support inplace swizzle
    if (!(dstStride <= rBitmap.Stride()))
    {
      throw std::invalid_argument("dstStride must be >= 0 and <= rBitmap.Stride()");
    }

    if (srcIdx0 > 3)
    {
      throw std::invalid_argument("srcIndex0 must be >0 and <= 3");
    }

    assert(PixelFormatUtil::GetBytesPerPixel(rBitmap.GetPixelFormat()) == 4);

    // Generic slow swizzle for 32bpp to 8bpp
    const uint32_t width = rBitmap.RawUnsignedWidth();
    const uint32_t height = rBitmap.RawUnsignedHeight();
    const uint32_t srcStride = rBitmap.Stride();
    auto* pDstBitmap = static_cast<uint8_t*>(rBitmap.Content());
    const uint8_t* const pDstBitmapEnd = pDstBitmap + (height * dstStride);
    const uint8_t* pSrcBitmap = pDstBitmap;
    while (pDstBitmap < pDstBitmapEnd)
    {
      for (uint32_t x = 0; x < width; ++x)
      {
        const uint8_t b0 = pSrcBitmap[(x * 4) + srcIdx0];
        pDstBitmap[x + 0] = b0;
      }
      pSrcBitmap += srcStride;
      pDstBitmap += dstStride;
    }
  }


  void RawBitmapUtil::Average24To8(RawBitmapEx& rBitmap, const PixelFormat dstPixelFormat, const uint32_t dstStride, const uint32_t srcIdx0,
                                   const uint32_t srcIdx1, const uint32_t srcIdx2)
  {
    ReadOnlyRawBitmap srcBitmap(rBitmap);    // NOLINT(cppcoreguidelines-slicing)
    rBitmap.SetStride(dstStride);
    rBitmap.SetPixelFormat(dstPixelFormat);

    Average24To8(rBitmap, srcBitmap, srcIdx0, srcIdx1, srcIdx2);
  }


  void RawBitmapUtil::Average32To8(RawBitmapEx& rBitmap, const PixelFormat dstPixelFormat, const uint32_t dstStride, const uint32_t srcIdx0,
                                   const uint32_t srcIdx1, const uint32_t srcIdx2)
  {
    ReadOnlyRawBitmap srcBitmap(rBitmap);    // NOLINT(cppcoreguidelines-slicing)
    rBitmap.SetStride(dstStride);
    rBitmap.SetPixelFormat(dstPixelFormat);
    Average32To8(rBitmap, srcBitmap, srcIdx0, srcIdx1, srcIdx2);
  }


  void RawBitmapUtil::ClearPadding(RawBitmapEx& rBitmap)
  {
    if (!rBitmap.IsValid())
    {
      throw std::invalid_argument("ClearPadding requires a valid bitmap");
    }

    // Generic padding clear
    const uint32_t minimumStride = PixelFormatUtil::CalcMinimumStride(rBitmap.Width(), rBitmap.GetPixelFormat());
    const uint32_t stride = rBitmap.Stride();
    const uint32_t paddingSize = stride - minimumStride;

    if (paddingSize > 0)
    {
      auto* pDst = static_cast<uint8_t*>(rBitmap.Content());
      const uint8_t* const pDstEnd = pDst + (rBitmap.RawUnsignedHeight() * stride);

      // Move to the padding area
      pDst += minimumStride;
      const uint8_t* pDstPaddingEnd = pDst + paddingSize;

      while (pDst < pDstEnd)
      {
        // Clear the padding area
        while (pDst < pDstPaddingEnd)
        {
          *pDst = 0;
          ++pDst;
        }
        pDst += minimumStride;
        pDstPaddingEnd += stride;
      }
    }
  }


  void RawBitmapUtil::FlipHorizontal(RawBitmapEx& rBitmap)
  {
    if (!rBitmap.IsValid())
    {
      throw std::invalid_argument("Swizzle requires a valid bitmap");
    }

    // Generic slow flip
    const uint32_t minimumStride = PixelFormatUtil::CalcMinimumStride(rBitmap.Width(), rBitmap.GetPixelFormat());

    const uint32_t dstStride = rBitmap.Stride();
    auto* pDst = static_cast<uint8_t*>(rBitmap.Content());
    const uint8_t* const pDstEnd = pDst + (dstStride * (rBitmap.RawUnsignedHeight() / 2));

    uint8_t* pSrc = pDst + (dstStride * (rBitmap.RawUnsignedHeight() - 1));

    uint8_t tmp = 0;

    const uint32_t dstStrideAdjust = dstStride - minimumStride;
    const uint32_t srcStride = dstStride + minimumStride;

    uint8_t* pDstTmp = pDst + minimumStride;
    while (pDst < pDstEnd)
    {
      while (pDst < pDstTmp)
      {
        tmp = *pDst;
        *pDst = *pSrc;
        *pSrc = tmp;
        ++pSrc;
        ++pDst;
      }
      pSrc -= srcStride;
      pDst += dstStrideAdjust;
      pDstTmp += dstStride;
    }

    switch (rBitmap.GetOrigin())
    {
    case BitmapOrigin::UpperLeft:
      rBitmap.SetBitmapOrigin(BitmapOrigin::LowerLeft);
      break;
    case BitmapOrigin::LowerLeft:
      rBitmap.SetBitmapOrigin(BitmapOrigin::UpperLeft);
      break;
    default:
      throw NotSupportedException("Unknown bitmap origin");
    }
  }


  void RawBitmapUtil::MemoryCopy(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("MemoryCopy requires valid bitmaps");
    }

    if (rDstBitmap.Width() != srcBitmap.Width() || rDstBitmap.Height() != srcBitmap.Height() || rDstBitmap.GetOrigin() != srcBitmap.GetOrigin())
    {
      throw std::invalid_argument("Memory copy requires that width, height and origin matches");
    }

    const auto srcLayout = srcBitmap.GetPixelFormatLayout();
    const auto dstLayout = rDstBitmap.GetPixelFormatLayout();
    const std::size_t bytesPerPixel = PixelFormatLayoutUtil::GetBytesPerPixel(srcLayout);
    if (bytesPerPixel != PixelFormatLayoutUtil::GetBytesPerPixel(dstLayout))
    {
      throw UsageErrorException("MemoryCopy requires that the src and dst bitmap has the same amount of bytes per pixel");
    }

    const auto* pSrc = static_cast<const uint8_t*>(srcBitmap.Content());
    const uint8_t* const pSrcEnd = pSrc + srcBitmap.GetByteSize();
    auto* pDst = static_cast<uint8_t*>(rDstBitmap.Content());
    uint8_t* const pDstEnd = pDst + rDstBitmap.GetByteSize();

    // The buffers can not overlap
    if (pSrc < pDstEnd && pSrcEnd > pDst)
    {
      throw UsageErrorException("MemoryCopy does not support overlapping buffers");
    }

    const std::size_t bytesPerLine = srcBitmap.RawUnsignedWidth() * bytesPerPixel;

    const std::size_t srcStride = srcBitmap.Stride();
    const std::size_t dstStride = rDstBitmap.Stride();

    assert(srcStride >= bytesPerLine);
    assert(dstStride >= bytesPerLine);

    if (srcStride != dstStride || bytesPerLine != srcStride)
    {
      while (pSrc < pSrcEnd)
      {
        std::memcpy(pDst, pSrc, bytesPerLine);
        pSrc += srcStride;
        pDst += dstStride;
      }
    }
    else
    {
      const std::size_t totalBytes = srcStride * srcBitmap.RawUnsignedHeight();
      assert(totalBytes == srcBitmap.GetByteSize());
      assert(totalBytes == rDstBitmap.GetByteSize());
      std::memcpy(pDst, pSrc, totalBytes);
    }
  }


  void RawBitmapUtil::Swizzle24From012To210(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("Swizzle24From012To210 requires valid bitmaps");
    }

    if (rDstBitmap.Width() != srcBitmap.Width() || rDstBitmap.Height() != srcBitmap.Height() || rDstBitmap.GetOrigin() != srcBitmap.GetOrigin())
    {
      throw std::invalid_argument("Swizzle24From012To210 requires that width, height and origin matches");
    }

    const auto srcLayout = srcBitmap.GetPixelFormatLayout();
    const auto dstLayout = rDstBitmap.GetPixelFormatLayout();
    const std::size_t bytesPerPixel = PixelFormatLayoutUtil::GetBytesPerPixel(srcLayout);
    if (bytesPerPixel != 3 || 3 != PixelFormatLayoutUtil::GetBytesPerPixel(dstLayout))
    {
      throw UsageErrorException("Swizzle24From012To210 requires 3 bytes per pixel");
    }

    const auto* pSrc = static_cast<const uint8_t*>(srcBitmap.Content());
    const uint8_t* const pSrcEnd = pSrc + srcBitmap.GetByteSize();
    auto* pDst = static_cast<uint8_t*>(rDstBitmap.Content());
    uint8_t* const pDstEnd = pDst + rDstBitmap.GetByteSize();

    const uint32_t srcStride = srcBitmap.Stride();
    const uint32_t dstStride = rDstBitmap.Stride();

    // The buffers can only overlap if pSrc == pDst && srcStride >= dstStride, if thats not the case they can not overlap
    if ((pSrc != pDst || (pSrc == pDst && srcStride < dstStride)) && pSrc < pDstEnd && pSrcEnd > pDst)
    {
      throw UsageErrorException("Swizzle24From012To210 does not support overlapping buffers");
    }

    // Generic slow swizzle for 24bpp formats
    const uint32_t srcWidthM3 = srcBitmap.RawUnsignedWidth() * 3;
    const uint32_t srcHeight = srcBitmap.RawUnsignedHeight();

    for (uint32_t y = 0; y < srcHeight; ++y)
    {
      for (uint32_t x = 0; x < srcWidthM3; x += 3)
      {
        const auto c0 = pSrc[x + 2];
        const auto c1 = pSrc[x + 1];
        const auto c2 = pSrc[x + 0];

        pDst[x + 0] = c0;
        pDst[x + 1] = c1;
        pDst[x + 2] = c2;
      }
      pSrc += srcStride;
      pDst += dstStride;
    }
  }

  void RawBitmapUtil::Swizzle24(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap, const uint32_t srcIdx0, const uint32_t srcIdx1,
                                const uint32_t srcIdx2)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("Swizzle24 requires valid bitmaps");
    }

    if (rDstBitmap.Width() != srcBitmap.Width() || rDstBitmap.Height() != srcBitmap.Height() || rDstBitmap.GetOrigin() != srcBitmap.GetOrigin())
    {
      throw std::invalid_argument("Swizzle24 requires that width, height and origin matches");
    }

    if (srcIdx0 > 2 || srcIdx1 > 2 || srcIdx2 > 2)
    {
      throw std::invalid_argument("srcIndex must be >0 and <= 2");
    }

    const auto srcLayout = srcBitmap.GetPixelFormatLayout();
    const auto dstLayout = rDstBitmap.GetPixelFormatLayout();
    const std::size_t bytesPerPixel = PixelFormatLayoutUtil::GetBytesPerPixel(srcLayout);
    if (bytesPerPixel != 3 || 3 != PixelFormatLayoutUtil::GetBytesPerPixel(dstLayout))
    {
      throw UsageErrorException("Swizzle24 requires 3 bytes per pixel");
    }

    const auto* pSrc = static_cast<const uint8_t*>(srcBitmap.Content());
    const uint8_t* const pSrcEnd = pSrc + srcBitmap.GetByteSize();
    auto* pDst = static_cast<uint8_t*>(rDstBitmap.Content());
    uint8_t* const pDstEnd = pDst + rDstBitmap.GetByteSize();

    const uint32_t srcStride = srcBitmap.Stride();
    const uint32_t dstStride = rDstBitmap.Stride();

    // The buffers can only overlap if pSrc == pDst && srcStride >= dstStride, if thats not the case they can not overlap
    if ((pSrc != pDst || (pSrc == pDst && srcStride < dstStride)) && pSrc < pDstEnd && pSrcEnd > pDst)
    {
      throw UsageErrorException("Swizzle24 does not support overlapping buffers");
    }

    // Generic slow swizzle for 24bpp formats

    const uint32_t srcWidth = srcBitmap.RawUnsignedWidth();
    const uint32_t srcHeight = srcBitmap.RawUnsignedHeight();

    for (uint32_t y = 0; y < srcHeight; ++y)
    {
      for (uint32_t x = 0; x < srcWidth; ++x)
      {
        const auto c0 = pSrc[(x * 3) + srcIdx0];
        const auto c1 = pSrc[(x * 3) + srcIdx1];
        const auto c2 = pSrc[(x * 3) + srcIdx2];
        pDst[(x * 3) + 0] = c0;
        pDst[(x * 3) + 1] = c1;
        pDst[(x * 3) + 2] = c2;
      }
      pSrc += srcStride;
      pDst += dstStride;
    }
  }


  void RawBitmapUtil::Swizzle32(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap, const uint32_t srcIdx0, const uint32_t srcIdx1,
                                const uint32_t srcIdx2, const uint32_t srcIdx3)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("Swizzle32 requires valid bitmaps");
    }

    if (rDstBitmap.Width() != srcBitmap.Width() || rDstBitmap.Height() != srcBitmap.Height() || rDstBitmap.GetOrigin() != srcBitmap.GetOrigin())
    {
      throw std::invalid_argument("Swizzle32 requires that width, height and origin matches");
    }

    if (srcIdx0 > 3 || srcIdx1 > 3 || srcIdx2 > 3 || srcIdx3 > 3)
    {
      throw std::invalid_argument("srcIndex must be >0 and <= 3");
    }

    const auto srcLayout = srcBitmap.GetPixelFormatLayout();
    const auto dstLayout = rDstBitmap.GetPixelFormatLayout();
    const std::size_t bytesPerPixel = PixelFormatLayoutUtil::GetBytesPerPixel(srcLayout);
    if (bytesPerPixel != 4 || 4 != PixelFormatLayoutUtil::GetBytesPerPixel(dstLayout))
    {
      throw UsageErrorException("Swizzle32 requires 4 bytes per pixel");
    }

    const auto* pSrc = static_cast<const uint8_t*>(srcBitmap.Content());
    const uint8_t* const pSrcEnd = pSrc + srcBitmap.GetByteSize();
    auto* pDst = static_cast<uint8_t*>(rDstBitmap.Content());
    uint8_t* const pDstEnd = pDst + rDstBitmap.GetByteSize();
    const uint32_t srcStride = srcBitmap.Stride();
    const uint32_t dstStride = rDstBitmap.Stride();

    // The buffers can only overlap if pSrc == pDst && srcStride >= dstStride, if thats not the case they can not overlap
    if ((pSrc != pDst || (pSrc == pDst && srcStride < dstStride)) && pSrc < pDstEnd && pSrcEnd > pDst)
    {
      throw UsageErrorException("Swizzle32 does not support overlapping buffers");
    }

    // Generic slow swizzle for 32bpp formats
    const uint32_t srcWidth = srcBitmap.RawUnsignedWidth();
    const uint32_t srcHeight = srcBitmap.RawUnsignedHeight();

    for (uint32_t y = 0; y < srcHeight; ++y)
    {
      for (uint32_t x = 0; x < srcWidth; ++x)
      {
        const uint8_t b0 = pSrc[(x * 4) + srcIdx0];
        const uint8_t b1 = pSrc[(x * 4) + srcIdx1];
        const uint8_t b2 = pSrc[(x * 4) + srcIdx2];
        const uint8_t b3 = pSrc[(x * 4) + srcIdx3];
        pDst[(x * 4) + 0] = b0;
        pDst[(x * 4) + 1] = b1;
        pDst[(x * 4) + 2] = b2;
        pDst[(x * 4) + 3] = b3;
      }
      pSrc += srcStride;
      pDst += dstStride;
    }
  }


  void RawBitmapUtil::Swizzle32To24(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap, const uint32_t srcIdx0, const uint32_t srcIdx1,
                                    const uint32_t srcIdx2)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("Swizzle32To24 requires valid bitmaps");
    }

    if (rDstBitmap.Width() != srcBitmap.Width() || rDstBitmap.Height() != srcBitmap.Height() || rDstBitmap.GetOrigin() != srcBitmap.GetOrigin())
    {
      throw std::invalid_argument("Swizzle32To24 requires that width, height and origin matches");
    }

    if (srcIdx0 > 3 || srcIdx1 > 3 || srcIdx2 > 3)
    {
      throw std::invalid_argument("srcIndex must be >0 and <= 3");
    }

    const auto srcLayout = srcBitmap.GetPixelFormatLayout();
    const auto dstLayout = rDstBitmap.GetPixelFormatLayout();
    const std::size_t bytesPerPixel = PixelFormatLayoutUtil::GetBytesPerPixel(srcLayout);
    if (bytesPerPixel != 4 || 3 != PixelFormatLayoutUtil::GetBytesPerPixel(dstLayout))
    {
      throw UsageErrorException("Swizzle32To24 requires src = 4 bytes per pixel and dst = 3 bytes per pixel");
    }

    const auto* pSrc = static_cast<const uint8_t*>(srcBitmap.Content());
    const uint8_t* const pSrcEnd = pSrc + srcBitmap.GetByteSize();
    auto* pDst = static_cast<uint8_t*>(rDstBitmap.Content());
    uint8_t* const pDstEnd = pDst + rDstBitmap.GetByteSize();
    const uint32_t srcStride = srcBitmap.Stride();
    const uint32_t dstStride = rDstBitmap.Stride();

    // The buffers can only overlap if pSrc == pDst && srcStride >= dstStride, if thats not the case they can not overlap
    if ((pSrc != pDst || (pSrc == pDst && srcStride < dstStride)) && pSrc < pDstEnd && pSrcEnd > pDst)
    {
      throw UsageErrorException("Swizzle32 does not support overlapping buffers");
    }

    // Generic slow swizzle for 32bpp to 24bpp formats
    const uint32_t width = srcBitmap.RawUnsignedWidth();
    while (pDst < pDstEnd)
    {
      for (uint32_t x = 0; x < width; ++x)
      {
        const uint8_t b0 = pSrc[(x * 4) + srcIdx0];
        const uint8_t b1 = pSrc[(x * 4) + srcIdx1];
        const uint8_t b2 = pSrc[(x * 4) + srcIdx2];
        pDst[(x * 3) + 0] = b0;
        pDst[(x * 3) + 1] = b1;
        pDst[(x * 3) + 2] = b2;
      }
      pSrc += srcStride;
      pDst += dstStride;
    }
  }


  void RawBitmapUtil::Swizzle24To32(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap, const uint32_t dstIdx0, const uint32_t dstIdx1,
                                    const uint32_t dstIdx2, const uint32_t dstIdx3, const uint8_t value3)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("Swizzle24To32 requires valid bitmaps");
    }

    if (rDstBitmap.Width() != srcBitmap.Width() || rDstBitmap.Height() != srcBitmap.Height() || rDstBitmap.GetOrigin() != srcBitmap.GetOrigin())
    {
      throw std::invalid_argument("Swizzle32To24 requires that width, height and origin matches");
    }

    if (dstIdx0 > 3 || dstIdx1 > 3 || dstIdx2 > 3 || dstIdx3 > 3)
    {
      throw std::invalid_argument("dstIndex must be >0 and <= 3");
    }

    const auto srcLayout = srcBitmap.GetPixelFormatLayout();
    const auto dstLayout = rDstBitmap.GetPixelFormatLayout();
    const std::size_t bytesPerPixel = PixelFormatLayoutUtil::GetBytesPerPixel(srcLayout);
    if (bytesPerPixel != 3 || 4 != PixelFormatLayoutUtil::GetBytesPerPixel(dstLayout))
    {
      throw UsageErrorException("Swizzle32To24 requires src = 3 bytes per pixel and dst = 4 bytes per pixel");
    }

    const auto* pSrc = static_cast<const uint8_t*>(srcBitmap.Content());
    const uint8_t* const pSrcEnd = pSrc + srcBitmap.GetByteSize();
    auto* pDst = static_cast<uint8_t*>(rDstBitmap.Content());
    uint8_t* const pDstEnd = pDst + rDstBitmap.GetByteSize();
    const uint32_t srcStride = srcBitmap.Stride();
    const uint32_t dstStride = rDstBitmap.Stride();

    // The buffers can only overlap if pSrc == pDst && srcStride >= dstStride, if thats not the case they can not overlap
    if ((pSrc != pDst || (pSrc == pDst && srcStride < dstStride)) && pSrc < pDstEnd && pSrcEnd > pDst)
    {
      throw UsageErrorException("Swizzle32 does not support overlapping buffers");
    }

    // Generic slow swizzle for 24bpp to 32bpp formats
    const int32_t width = srcBitmap.RawWidth() - 1;
    while (pDst < pDstEnd)
    {
      for (int32_t x = width; x >= 0; --x)
      {
        const uint8_t b0 = pSrc[(x * 3) + 0];
        const uint8_t b1 = pSrc[(x * 3) + 1];
        const uint8_t b2 = pSrc[(x * 3) + 2];
        pDst[(x * 4) + dstIdx0] = b0;
        pDst[(x * 4) + dstIdx1] = b1;
        pDst[(x * 4) + dstIdx2] = b2;
        pDst[(x * 4) + dstIdx3] = value3;
      }
      pSrc += srcStride;
      pDst += dstStride;
    }
  }

  void RawBitmapUtil::Average24To8(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap, const uint32_t srcIdx0, const uint32_t srcIdx1,
                                   const uint32_t srcIdx2)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("Average24To8 requires valid bitmaps");
    }

    if (rDstBitmap.Width() != srcBitmap.Width() || rDstBitmap.Height() != srcBitmap.Height() || rDstBitmap.GetOrigin() != srcBitmap.GetOrigin())
    {
      throw std::invalid_argument("Average24To8 requires that width, height and origin matches");
    }

    if (srcIdx0 > 2 || srcIdx1 > 2 || srcIdx2 > 2)
    {
      throw std::invalid_argument("srcIndex must be >0 and <= 2");
    }

    const auto srcLayout = srcBitmap.GetPixelFormatLayout();
    const auto dstLayout = rDstBitmap.GetPixelFormatLayout();
    const std::size_t bytesPerPixel = PixelFormatLayoutUtil::GetBytesPerPixel(srcLayout);
    if (bytesPerPixel != 3 || 1 != PixelFormatLayoutUtil::GetBytesPerPixel(dstLayout))
    {
      throw UsageErrorException("Average24To8 requires 3 bytes per pixel");
    }

    const auto* pSrc = static_cast<const uint8_t*>(srcBitmap.Content());
    const uint8_t* const pSrcEnd = pSrc + srcBitmap.GetByteSize();
    auto* pDst = static_cast<uint8_t*>(rDstBitmap.Content());
    uint8_t* const pDstEnd = pDst + rDstBitmap.GetByteSize();
    const uint32_t srcStride = srcBitmap.Stride();
    const uint32_t dstStride = rDstBitmap.Stride();

    // The buffers can only overlap if pSrc == pDst && srcStride >= dstStride, if thats not the case they can not overlap
    if ((pSrc != pDst || (pSrc == pDst && srcStride < dstStride)) && pSrc < pDstEnd && pSrcEnd > pDst)
    {
      throw UsageErrorException("Average24To8 does not support overlapping buffers");
    }

    // Generic slow average for 24bpp three channel to 8bpp one channel
    const uint32_t width = srcBitmap.RawUnsignedWidth();
    while (pDst < pDstEnd)
    {
      for (uint32_t x = 0; x < width; ++x)
      {
        const uint32_t b0 = pSrc[(x * 3) + srcIdx0];
        const uint32_t b1 = pSrc[(x * 3) + srcIdx1];
        const uint32_t b2 = pSrc[(x * 3) + srcIdx2];
        pDst[x] = static_cast<uint8_t>((b0 + b1 + b2) / 3);
      }
      pSrc += srcStride;
      pDst += dstStride;
    }
  }


  void RawBitmapUtil::Average32To8(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap, const uint32_t srcIdx0, const uint32_t srcIdx1,
                                   const uint32_t srcIdx2)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("Average32To8 requires valid bitmaps");
    }

    if (rDstBitmap.Width() != srcBitmap.Width() || rDstBitmap.Height() != srcBitmap.Height() || rDstBitmap.GetOrigin() != srcBitmap.GetOrigin())
    {
      throw std::invalid_argument("Average32To8 requires that width, height and origin matches");
    }

    if (srcIdx0 > 3 || srcIdx1 > 3 || srcIdx2 > 3)
    {
      throw std::invalid_argument("srcIndex must be >0 and <= 3");
    }

    const auto srcLayout = srcBitmap.GetPixelFormatLayout();
    const auto dstLayout = rDstBitmap.GetPixelFormatLayout();
    const std::size_t bytesPerPixel = PixelFormatLayoutUtil::GetBytesPerPixel(srcLayout);
    if (bytesPerPixel != 4 || 1 != PixelFormatLayoutUtil::GetBytesPerPixel(dstLayout))
    {
      throw UsageErrorException("Average32To8 requires 4 bytes per pixel");
    }

    const auto* pSrc = static_cast<const uint8_t*>(srcBitmap.Content());
    const uint8_t* const pSrcEnd = pSrc + srcBitmap.GetByteSize();
    auto* pDst = static_cast<uint8_t*>(rDstBitmap.Content());
    uint8_t* const pDstEnd = pDst + rDstBitmap.GetByteSize();
    const uint32_t srcStride = srcBitmap.Stride();
    const uint32_t dstStride = rDstBitmap.Stride();

    // The buffers can only overlap if pSrc == pDst && srcStride >= dstStride, if thats not the case they can not overlap
    if ((pSrc != pDst || (pSrc == pDst && srcStride < dstStride)) && pSrc < pDstEnd && pSrcEnd > pDst)
    {
      throw UsageErrorException("Average32To8 does not support overlapping buffers");
    }


    // Generic slow average for 32bpp three channel to 8bpp one channel
    const uint32_t width = srcBitmap.RawUnsignedWidth();
    while (pDst < pDstEnd)
    {
      for (uint32_t x = 0; x < width; ++x)
      {
        const uint32_t b0 = pSrc[(x * 4) + srcIdx0];
        const uint32_t b1 = pSrc[(x * 4) + srcIdx1];
        const uint32_t b2 = pSrc[(x * 4) + srcIdx2];
        pDst[x] = static_cast<uint8_t>((b0 + b1 + b2) / 3);
      }
      pSrc += srcStride;
      pDst += dstStride;
    }
  }

  void RawBitmapUtil::Expand1ByteToNBytes(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("Expand1ByteToNBytes requires valid bitmaps");
    }

    if (rDstBitmap.Width() != srcBitmap.Width() || rDstBitmap.Height() != srcBitmap.Height() || rDstBitmap.GetOrigin() != srcBitmap.GetOrigin())
    {
      throw std::invalid_argument("Expand1ByteToNBytes requires that width, height and origin matches");
    }

    const auto srcLayout = srcBitmap.GetPixelFormatLayout();
    const auto dstLayout = rDstBitmap.GetPixelFormatLayout();
    const std::size_t bytesPerPixel = PixelFormatLayoutUtil::GetBytesPerPixel(srcLayout);
    const std::size_t dstBytesPerPixel = PixelFormatLayoutUtil::GetBytesPerPixel(dstLayout);
    if (bytesPerPixel != 1 || dstBytesPerPixel < 1)
    {
      throw UsageErrorException("Expand1ByteToNBytes requires src=1 dst>=1 bytes per pixel");
    }

    const auto* pSrc = static_cast<const uint8_t*>(srcBitmap.Content());
    const uint8_t* const pSrcEnd = pSrc + srcBitmap.GetByteSize();
    auto* pDst = static_cast<uint8_t*>(rDstBitmap.Content());
    uint8_t* const pDstEnd = pDst + rDstBitmap.GetByteSize();

    const uint32_t srcStride = srcBitmap.Stride();
    const uint32_t dstStride = rDstBitmap.Stride();

    // The buffers can not overlap
    if (pSrc < pDstEnd && pSrcEnd > pDst)
    {
      throw UsageErrorException("Swizzle32 does not support overlapping buffers");
    }

    const uint32_t srcWidth = srcBitmap.RawUnsignedWidth();
    const uint32_t srcHeight = srcBitmap.RawUnsignedHeight();

    for (uint32_t y = 0; y < srcHeight; ++y)
    {
      for (uint32_t x = 0; x < srcWidth; ++x)
      {
        const uint8_t b0 = pSrc[x];
        for (uint32_t i = 0; i < dstBytesPerPixel; ++i)
        {
          pDst[(x * dstBytesPerPixel) + i] = b0;
        }
      }
      pSrc += srcStride;
      pDst += dstStride;
    }
  }

  void RawBitmapUtil::DownscaleNearest(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("DownscaleNearest requires valid bitmaps");
    }
    if (rDstBitmap.Width() > srcBitmap.Width() || rDstBitmap.Height() > srcBitmap.Height() || rDstBitmap.GetOrigin() != srcBitmap.GetOrigin())
    {
      throw std::invalid_argument("DownscaleNearest requires that dst width, height is less or equal to the src");
    }
    if (rDstBitmap.GetOrigin() != srcBitmap.GetOrigin() || rDstBitmap.GetPixelFormat() != srcBitmap.GetPixelFormat())
    {
      throw std::invalid_argument("DownscaleNearest requires that the origin and pixel format matches");
    }
    if (rDstBitmap.RawWidth() <= 0 || rDstBitmap.RawHeight() <= 0)
    {
      return;
    }

    switch (PixelFormatLayoutUtil::GetBytesPerPixel(srcBitmap.GetPixelFormatLayout()))
    {
    case 4:
      DownscaleNearest32(rDstBitmap, srcBitmap);
      break;
    default:
      throw NotSupportedException("PixelFormat is unsupported");
    }
  }


  void RawBitmapUtil::DownscaleNearest32(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("DownscaleNearest32 requires valid bitmaps");
    }
    if (rDstBitmap.Width() > srcBitmap.Width() || rDstBitmap.Height() > srcBitmap.Height())
    {
      throw std::invalid_argument("DownscaleNearest32 requires that dst width, height is less or equal to the src");
    }
    if (rDstBitmap.GetOrigin() != srcBitmap.GetOrigin() || rDstBitmap.GetPixelFormat() != srcBitmap.GetPixelFormat())
    {
      throw std::invalid_argument("DownscaleNearest32 requires that the origin and pixel format matches");
    }
    if (PixelFormatLayoutUtil::GetBytesPerPixel(srcBitmap.GetPixelFormatLayout()) != 4)
    {
      throw std::invalid_argument("DownscaleNearest32 requires that the pixel format uses four bytes per pixel");
    }
    if ((rDstBitmap.Stride() % 4) != 0 || (srcBitmap.Stride() % 4) != 0 || (rDstBitmap.GetByteSize() % 4) != 0 || (srcBitmap.GetByteSize() % 4) != 0)
    {
      throw NotSupportedException("DownscaleNearest32 requires that the src and dst bitmap is 32bit aligned");
    }
    if (srcBitmap.RawWidth() > 0xFFFF || srcBitmap.RawHeight() > 0xFFFF)
    {
      throw NotSupportedException("DownscaleNearest32 can only downscale sizes of 0xFFFF or less");
    }
    if (rDstBitmap.RawWidth() <= 0 || rDstBitmap.RawHeight() <= 0)
    {
      return;
    }

    const auto* const pSrcStart = static_cast<const uint32_t*>(srcBitmap.Content());
    const uint32_t srcStride = srcBitmap.Stride() / 4;
    const uint32_t srcXAdd = (srcBitmap.RawUnsignedWidth() << 16) / rDstBitmap.RawUnsignedWidth();
    const uint32_t srcYAdd = (srcBitmap.RawUnsignedHeight() << 16) / rDstBitmap.RawUnsignedHeight();

    auto* pDst = static_cast<uint32_t*>(rDstBitmap.Content());
    const uint32_t* const pDstEnd = pDst + (rDstBitmap.GetByteSize() / 4);
    const uint32_t dstStride = rDstBitmap.Stride() / 4;
    const uint32_t dstWidth = rDstBitmap.RawUnsignedWidth();
    assert(dstStride >= dstWidth);
    const uint32_t dstStrideAdd = dstStride - dstWidth;

    uint64_t srcY = 1 << 15;
    while (pDst < pDstEnd)
    {
      assert((srcY >> 16) < srcBitmap.RawUnsignedHeight());
      const uint32_t* const pSrc = pSrcStart + ((srcY >> 16) * srcStride);
      uint32_t srcX = 1 << 15;
      const uint32_t* const pDstEndX = (pDst + dstWidth);
      while (pDst < pDstEndX)
      {
        assert((srcX >> 16) < srcBitmap.RawUnsignedWidth());
        *pDst = pSrc[srcX >> 16];
        srcX += srcXAdd;
        ++pDst;
      }
      pDst += dstStrideAdd;
      srcY += srcYAdd;
    }
  }

  void RawBitmapUtil::DownscaleBoxFilter(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("DownscaleBoxFilter requires valid bitmaps");
    }
    if (rDstBitmap.GetOrigin() != srcBitmap.GetOrigin() || rDstBitmap.GetPixelFormat() != srcBitmap.GetPixelFormat())
    {
      throw std::invalid_argument("DownscaleBoxFilter requires that the origin and pixel format matches");
    }
    if (!PixelFormatLayoutUtil::IsSwizzleCompatible(srcBitmap.GetPixelFormatLayout(), PixelFormatLayout::R8G8B8A8))
    {
      throw std::invalid_argument("DownscaleBoxFilter unsupported pixel format");
    }
    if ((rDstBitmap.RawUnsignedWidth() * 2) != srcBitmap.RawUnsignedWidth() || (rDstBitmap.RawUnsignedHeight() * 2) != srcBitmap.RawUnsignedHeight())
    {
      if (rDstBitmap.RawUnsignedWidth() <= 0 || rDstBitmap.RawUnsignedHeight() <= 0)
      {
        return;
      }
      throw std::invalid_argument("DownscaleBoxFilter requires that dst width, height is half the size of the src");
    }

    switch (PixelFormatLayoutUtil::GetBytesPerPixel(srcBitmap.GetPixelFormatLayout()))
    {
    case 4:
      DownscaleBoxFilter32(rDstBitmap, srcBitmap);
      break;
    default:
      throw NotSupportedException("PixelFormat is unsupported");
    }
  }

  // MIPMAPPING hints
  // http://number-none.com/product/Mipmapping,%20Part%201/index.html
  // WARNING this does not take into account the gamma encoding during the averaging.
  void RawBitmapUtil::DownscaleBoxFilter32(RawBitmapEx& rDstBitmap, const ReadOnlyRawBitmap& srcBitmap)
  {
    if (!rDstBitmap.IsValid() && srcBitmap.IsValid())
    {
      throw std::invalid_argument("DownscaleBoxFilter32 requires valid bitmaps");
    }
    if (rDstBitmap.GetOrigin() != srcBitmap.GetOrigin() || rDstBitmap.GetPixelFormat() != srcBitmap.GetPixelFormat())
    {
      throw std::invalid_argument("DownscaleBoxFilter32 requires that the origin and pixel format matches");
    }
    if (!PixelFormatLayoutUtil::IsSwizzleCompatible(srcBitmap.GetPixelFormatLayout(), PixelFormatLayout::R8G8B8A8))
    {
      throw std::invalid_argument("DownscaleBoxFilter32 unsupported pixel format");
    }
    if ((rDstBitmap.Stride() % 4) != 0 || (srcBitmap.Stride() % 4) != 0 || (rDstBitmap.GetByteSize() % 4) != 0 || (srcBitmap.GetByteSize() % 4) != 0)
    {
      throw NotSupportedException("DownscaleBoxFilter32 requires that the src and dst bitmap is 32bit aligned");
    }
    if ((rDstBitmap.RawUnsignedWidth() * 2) != srcBitmap.RawUnsignedWidth() || (rDstBitmap.RawUnsignedHeight() * 2) != srcBitmap.RawUnsignedHeight())
    {
      if (rDstBitmap.RawUnsignedWidth() <= 0 || rDstBitmap.RawUnsignedHeight() <= 0)
      {
        return;
      }
      throw std::invalid_argument("DownscaleBoxFilter32 requires that dst width, height is half the size of the src");
    }

    const auto* pSrc = static_cast<const uint32_t*>(srcBitmap.Content());
    const uint32_t srcStride = srcBitmap.Stride() / 4;
    const uint32_t* const pSrcEnd = pSrc + (srcStride * (srcBitmap.RawUnsignedHeight() - 1));
    const uint32_t srcWidth = srcBitmap.RawUnsignedWidth();
    const uint32_t srcStrideAdd = (srcStride * 2) - srcWidth;

    auto* pDst = static_cast<uint32_t*>(rDstBitmap.Content());
    const uint32_t dstStride = rDstBitmap.Stride() / 4;
    const uint32_t dstWidth = rDstBitmap.RawUnsignedWidth();
    assert(dstStride >= dstWidth);
    const uint32_t dstStrideAdd = dstStride - dstWidth;

    while (pSrc < pSrcEnd)
    {
      const uint32_t* const pSrcEndX = pSrc + srcWidth;
      while (pSrc < pSrcEndX)
      {
        // Average the four adjacent pixels to generate the downscaled color
        const uint32_t col00 = pSrc[0];
        const uint32_t col10 = pSrc[1];
        const uint32_t col01 = pSrc[srcStride + 0];
        const uint32_t col11 = pSrc[srcStride + 1];

        const uint32_t c0 =
          (((((col00 & 0xFF000000) >> 8) + ((col10 & 0xFF000000) >> 8) + ((col01 & 0xFF000000) >> 8) + ((col11 & 0xFF000000) >> 8)) / 4) & 0xFF0000)
          << 8;
        const uint32_t c1 = (((col00 & 0xFF0000) + (col10 & 0xFF0000) + (col01 & 0xFF0000) + (col11 & 0xFF0000)) / 4) & 0xFF0000;
        const uint32_t c2 = (((col00 & 0xFF00) + (col10 & 0xFF00) + (col01 & 0xFF00) + (col11 & 0xFF00)) / 4) & 0xFF00;
        const uint32_t c3 = (((col00 & 0xFF) + (col10 & 0xFF) + (col01 & 0xFF) + (col11 & 0xFF)) / 4);

        *pDst = c0 | c1 | c2 | c3;
        pSrc += 2;
        ++pDst;
      }
      pDst += dstStrideAdd;
      pSrc += srcStrideAdd;
    }
  }

}
