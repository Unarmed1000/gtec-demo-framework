/****************************************************************************************************************************************************
 * Copyright 2017, 2024 NXP
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
 *    * Neither the name of the NXP. nor the names of
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

#include "OpenVX101.hpp"
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslGraphics/Bitmap/BitmapUtil.hpp>
#include <FslGraphics/Colors.hpp>
#include <FslUtil/OpenGLES3/Exceptions.hpp>
#include <FslUtil/OpenGLES3/GLCheck.hpp>
#include <RapidOpenVX/Check.hpp>
#include <RapidOpenVX/Context.hpp>
#include <RapidOpenVX/Exceptions.hpp>
#include <RapidOpenVX/Image.hpp>
#include <GLES3/gl3.h>
#include <VX/vx.h>
#include <VX/vxu.h>
#include <cmath>

namespace Fsl
{
  using namespace RapidOpenVX;
  using namespace GLES3;

  namespace
  {
    void CopyImageFromCPUToGPU(Image& rImage, const Bitmap& srcBitmap)
    {
      assert(srcBitmap.GetPixelFormat() == PixelFormat::EX_ALPHA8_UNORM);

      const uint32_t imageWidth = srcBitmap.RawUnsignedWidth();
      const uint32_t imageHeight = srcBitmap.RawUnsignedHeight();

      vx_rectangle_t imageRect = {0, 0, imageWidth, imageHeight};
      vx_imagepatch_addressing_t imageInfo = VX_IMAGEPATCH_ADDR_INIT;
      vx_map_id mapId = 0;
      void* pImageAddress = nullptr;
      RAPIDOPENVX_CHECK(vxMapImagePatch(rImage.Get(), &imageRect, 0, &mapId, &imageInfo, &pImageAddress, VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST, 0));

      if ((1 != imageInfo.step_y) || (1 != imageInfo.step_x) || (imageHeight != imageInfo.dim_y) || (imageWidth != imageInfo.dim_x))
      {
        throw std::runtime_error("vx procedure error");
      }

      {
        const Bitmap::ScopedDirectReadAccess scopedAccess(srcBitmap);
        const auto* pSrcBitmap = static_cast<const uint8_t*>(scopedAccess.AsRawBitmap().Content());
        const auto srcStride = scopedAccess.AsRawBitmap().Stride();

        for (uint32_t y = 0; y < imageHeight; ++y)
        {
          for (uint32_t x = 0; x < imageWidth; ++x)
          {
            auto* pImagePixels = static_cast<uint8_t*>(vxFormatImagePatchAddress2d(pImageAddress, x, y, &imageInfo));
            assert(pImagePixels != nullptr);
            *pImagePixels = pSrcBitmap[(y * srcStride) + x];
          }
        }
      }

      RAPIDOPENVX_CHECK(vxUnmapImagePatch(rImage.Get(), mapId));
    }


    void CopyImagesFromGPUToCPU(Bitmap& dstBitmap, const Image& srcImage1, const Image& srcImage2)
    {
      assert(dstBitmap.GetPixelFormat() == PixelFormat::EX_ALPHA8_UNORM);

      const uint32_t imageWidth = dstBitmap.RawUnsignedWidth();
      const uint32_t imageHeight = dstBitmap.RawUnsignedHeight();
      vx_rectangle_t imageRect = {0, 0, imageWidth, imageHeight};

      // transfer image from gpu to cpu
      vx_imagepatch_addressing_t imageInfo1 = VX_IMAGEPATCH_ADDR_INIT;
      vx_imagepatch_addressing_t imageInfo2 = VX_IMAGEPATCH_ADDR_INIT;
      vx_map_id mapId1 = 0;
      vx_map_id mapId2 = 0;
      void* pImageAddress1 = nullptr;
      void* pImageAddress2 = nullptr;
      RAPIDOPENVX_CHECK(vxMapImagePatch(srcImage1.Get(), &imageRect, 0, &mapId1, &imageInfo1, &pImageAddress1, VX_READ_ONLY, VX_MEMORY_TYPE_HOST, 0));
      RAPIDOPENVX_CHECK(vxMapImagePatch(srcImage2.Get(), &imageRect, 0, &mapId2, &imageInfo2, &pImageAddress2, VX_READ_ONLY, VX_MEMORY_TYPE_HOST, 0));

      if ((imageInfo2.dim_y != imageInfo1.dim_y) || (imageInfo2.dim_x != imageInfo1.dim_x) || (imageInfo2.step_y != imageInfo1.step_y) ||
          (imageInfo2.step_x != imageInfo1.step_x) || (imageInfo2.dim_y != imageHeight) || (imageInfo2.dim_x != imageWidth) ||
          (imageInfo2.step_y != 1) || (imageInfo2.step_x != 1))
      {
        throw std::runtime_error("vx procedure error");
      }

      Bitmap::ScopedDirectReadWriteAccess scopedAccess(dstBitmap);
      auto* pDstBitmap = static_cast<uint8_t*>(scopedAccess.AsRawBitmap().Content());
      const auto dstStride = scopedAccess.AsRawBitmap().Stride();

      for (uint32_t y = 0; y < imageHeight; ++y)
      {
        for (uint32_t x = 0; x < imageWidth; ++x)
        {
          const auto* pImagePixel1 = static_cast<const int16_t*>(vxFormatImagePatchAddress2d(pImageAddress1, x, y, &imageInfo1));
          const auto* pImagePixel2 = static_cast<const int16_t*>(vxFormatImagePatchAddress2d(pImageAddress2, x, y, &imageInfo2));

          const float value = static_cast<float>(*pImagePixel1) * static_cast<float>(*pImagePixel1) +
                              static_cast<float>(*pImagePixel2) * static_cast<float>(*pImagePixel2);
          auto dstPixel = static_cast<uint16_t>(std::sqrt(value));
          if (dstPixel > 0xff)
          {
            dstPixel = 0xff;
          }

          pDstBitmap[(y * dstStride) + x] = static_cast<uint8_t>(dstPixel);
        }
      }

      RAPIDOPENVX_CHECK(vxUnmapImagePatch(srcImage1.Get(), mapId1));
      RAPIDOPENVX_CHECK(vxUnmapImagePatch(srcImage2.Get(), mapId2));
    }


    GLTexture ToTexture(const Bitmap& srcBitmap, const PixelFormat pixelFormat)
    {
      Bitmap tmpBitmap(srcBitmap);
      BitmapUtil::Convert(tmpBitmap, pixelFormat);
      GLTextureParameters texParams;
      return {tmpBitmap, texParams};
    }
  }


  OpenVX101::OpenVX101(const DemoAppConfig& config)
    : DemoAppGLES3(config)
    , m_graphics(config.DemoServiceProvider.Get<IGraphicsService>())
    , m_nativeBatch(std::dynamic_pointer_cast<GLES3::NativeBatch2D>(m_graphics->GetNativeBatch2D()))
  {
    RapidOpenVX::Context context(ResetMode::Create);

    // Read the image data
    Bitmap bitmap;
    GetContentManager()->Read(bitmap, "Test_gray.png", PixelFormat::EX_ALPHA8_UNORM);

    m_texSrc = ToTexture(bitmap, PixelFormat::R8G8B8A8_UNORM);

    const uint32_t imageWidth = bitmap.RawUnsignedWidth();
    const uint32_t imageHeight = bitmap.RawUnsignedHeight();

    Image image0(context.Get(), imageWidth, imageHeight, VX_DF_IMAGE_U8);
    Image image1(context.Get(), imageWidth, imageHeight, VX_DF_IMAGE_S16);
    Image image2(context.Get(), imageWidth, imageHeight, VX_DF_IMAGE_S16);

    CopyImageFromCPUToGPU(image0, bitmap);

    // process image
    RAPIDOPENVX_CHECK(vxuSobel3x3(context.Get(), image0.Get(), image1.Get(), image2.Get()));

    CopyImagesFromGPUToCPU(bitmap, image1, image2);


    FSLLOG3_INFO("vx process success.");

    m_texDst = ToTexture(bitmap, PixelFormat::R8G8B8A8_UNORM);
  }


  OpenVX101::~OpenVX101() = default;


  void OpenVX101::Update(const DemoTime& /*demoTime*/)
  {
  }


  void OpenVX101::Draw(const FrameInfo& frameInfo)
  {
    FSL_PARAM_NOT_USED(frameInfo);

    auto resPx = GetWindowSizePx();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const PxSize1D halfWidth = resPx.Width() / PxSize1D::Create(2);
    PxRectangle leftRect(PxValue(0), PxValue(0), halfWidth, resPx.Height());
    PxRectangle rightRect(halfWidth, PxValue::Create(0), halfWidth, resPx.Height());

    m_nativeBatch->Begin();
    m_nativeBatch->Draw(m_texSrc, leftRect, Colors::White());
    m_nativeBatch->Draw(m_texDst, rightRect, Colors::White());
    m_nativeBatch->End();
  }
}
