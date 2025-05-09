#ifndef FSLGRAPHICS_SPRITE_BASICIMAGESPRITE_HPP
#define FSLGRAPHICS_SPRITE_BASICIMAGESPRITE_HPP
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

#include <FslGraphics/Sprite/IImageSprite.hpp>
#include <FslGraphics/Sprite/Info/BasicImageSpriteInfo.hpp>
#include <FslGraphics/Sprite/Info/Core/RenderConverter.hpp>

namespace Fsl
{
  class SpriteNativeAreaCalc;
  class StringViewLite;

  class BasicImageSprite final : public IImageSprite
  {
    BasicImageSpriteInfo m_info;

  public:
    BasicImageSprite() = default;

    BasicImageSprite(const SpriteNativeAreaCalc& spriteNativeAreaCalc, const SpriteMaterialInfo& spriteMaterialInfo,
                     const PxRectangleU16& imageRectanglePx, const uint32_t imageDpi, const StringViewLite& debugName, const uint32_t densityDpi);

    void SetContent(const SpriteNativeAreaCalc& spriteNativeAreaCalc, const SpriteMaterialInfo& spriteMaterialInfo,
                    const PxRectangleU16& imageRectanglePx, const uint32_t imageDpi, const StringViewLite& debugName, const uint32_t densityDpi);


    PxSize2D GetRenderSizePx() const noexcept final
    {
      return m_info.RenderInfo.ScaledSizePx;
    }


    RenderImageInfo GetImageRenderInfo() const noexcept final
    {
      return RenderConverter::ToRenderImageInfo(GetRenderInfo());
    }


    const BasicImageSpriteInfo& GetInfo() const noexcept
    {
      return m_info;
    }

    const CoreBasicImageInfo& GetImageInfo() const noexcept
    {
      return m_info.ImageInfo;
    }

    const RenderBasicImageInfo& GetRenderInfo() const noexcept
    {
      return m_info.RenderInfo;
    }


    uint32_t GetMaterialCount() const noexcept final
    {
      return 1u;
    }

    const SpriteMaterialInfo& GetMaterialInfo(const uint32_t index) const final;
    void Resize(const uint32_t densityDpi) final;
  };
}

#endif
