#ifndef FSLGRAPHICS_SPRITE_INFO_IMAGESPRITEINFO_HPP
#define FSLGRAPHICS_SPRITE_INFO_IMAGESPRITEINFO_HPP
/****************************************************************************************************************************************************
 * Copyright 2020 NXP
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

#include <FslBase/Math/Dp/DpExtent2D.hpp>
#include <FslBase/Math/Pixel/PxExtent2D.hpp>
#include <FslBase/Math/Pixel/PxRectangleU16.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Math/Pixel/PxThickness.hpp>
#include <FslBase/Math/Pixel/PxThicknessU.hpp>
#include <FslGraphics/NativeTextureArea.hpp>
#include <FslGraphics/Sprite/Info/Core/CoreImageInfo.hpp>
#include <FslGraphics/Sprite/Info/Core/RenderImageInfo.hpp>
#include <FslGraphics/Sprite/Material/SpriteMaterialInfo.hpp>
#include <FslGraphics/Sprite/SpriteDpConfig.hpp>

namespace Fsl
{
  class SpriteNativeAreaCalc;
  class StringViewLite;

  //! @brief Represents a atlas texture 2d image with no trim
  struct ImageSpriteInfo
  {
    //! Information about the render material
    SpriteMaterialInfo MaterialInfo;

    //! Image information
    CoreImageInfo ImageInfo;

    uint32_t ImageDpi{SpriteDpConfig::BaseDpi};

    RenderImageInfo RenderInfo;

    ImageSpriteInfo() = default;
    ImageSpriteInfo(const SpriteNativeAreaCalc& spriteNativeAreaCalc, const SpriteMaterialInfo& spriteMaterialInfo,
                    const PxThicknessU& imageTrimMarginPx, const PxRectangleU16& imageTrimmedRectanglePx, const uint32_t imageDpi,
                    const StringViewLite& debugName);

    bool IsValid() const
    {
      return MaterialInfo.IsValid();
    }

    bool operator==(const ImageSpriteInfo& rhs) const
    {
      return MaterialInfo == rhs.MaterialInfo && ImageInfo == rhs.ImageInfo && ImageDpi == rhs.ImageDpi && RenderInfo == rhs.RenderInfo;
    }

    bool operator!=(const ImageSpriteInfo& rhs) const
    {
      return !(*this == rhs);
    }
  };
}

#endif
