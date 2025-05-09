#ifndef FSLDEMOSERVICE_GRAPHICS_IGRAPHICSSERVICE_HPP
#define FSLDEMOSERVICE_GRAPHICS_IGRAPHICSSERVICE_HPP
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

#include <FslBase/BasicTypes.hpp>
#include <FslGraphics/ToneMapping/BasicToneMapper.hpp>
#include <FslGraphics/PixelFormat.hpp>
#include <FslDemoService/Graphics/ColorSpaceType.hpp>
#include <memory>

namespace Fsl
{
  struct PxRectangle;
  class Bitmap;
  class IBasic2D;
  class IBasicRenderSystem;
  class INativeBatch2D;
  class INativeGraphics;

  class IGraphicsService
  {
  public:
    virtual ~IGraphicsService() = default;

    virtual ColorSpaceType GetColorSpaceType() const noexcept = 0;

    //! @brief Capture the current content of the display after a glFinish.
    //! @param rBitmap will be resized to fit the screen
    //! @param pixelFormat the desired pixel format (R8G8B8A8_UINT or B8G8R8A8_UINT)
    //! @note  Throws a exception if rBitmap has a unsupported bitmap format
    virtual void Capture(Bitmap& rBitmap, const PixelFormat pixelFormat, const BasicToneMapper toneMapper) = 0;

    //! @brief Capture the current content of the display after a glFinish.
    //! @param rBitmap will be resized to the srcRectangle dimensions.
    //! @param pixelFormat the desired pixel format (R8G8B8A8_UINT or B8G8R8A8_UINT)
    //! @note  Throws a exception if rBitmap has a unsupported bitmap format
    virtual void Capture(Bitmap& rBitmap, const PixelFormat pixelFormat, const BasicToneMapper toneMapper, const PxRectangle& srcRectanglePx) = 0;

    //! @brief Get the basic 2D interface
    virtual std::shared_ptr<IBasic2D> GetBasic2D() = 0;

    //! @brief Acquire the native batch2d interface if its present (throws a exception if it's not)
    //! @note  INativeBatch2D should be available for all OpenGLES API's
    virtual std::shared_ptr<INativeBatch2D> GetNativeBatch2D() = 0;

    //! @brief Acquire the native graphics interface if its present (throws a exception if it's not)
    virtual std::shared_ptr<INativeGraphics> GetNativeGraphics() = 0;

    //! @brief Acquire the native graphics interface if its present (throws a exception if it's not)
    virtual std::shared_ptr<IBasicRenderSystem> GetBasicRenderSystem() = 0;
  };
}

#endif
