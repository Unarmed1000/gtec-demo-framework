#ifndef FSLGRAPHICS_TEXTURE_TEXTUREUTIL_HPP
#define FSLGRAPHICS_TEXTURE_TEXTUREUTIL_HPP
/****************************************************************************************************************************************************
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
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

#include <FslBase/Math/Pixel/PxExtent3D.hpp>
#include <FslBase/Span/ReadOnlySpan.hpp>
#include <FslGraphics/Bitmap/BitmapOrigin.hpp>
#include <FslGraphics/Bitmap/SupportedConversion.hpp>
#include <FslGraphics/PixelFormat.hpp>

namespace Fsl
{
  class Texture;
  struct TextureInfo;

  //! @brief Contains helper methods for working on Texture's
  //         They exist to provide functionality but they are not performance optimized!
  class TextureUtil
  {
  public:
    static ReadOnlySpan<SupportedConversion> GetSupportedConversions() noexcept;

    //! @brief Convert the texture to the desired pixel format and origin else return false.
    static bool TryConvert(Texture& rTexture, const PixelFormat desiredPixelFormat, const BitmapOrigin desiredOrigin = BitmapOrigin::Undefined);

    //! @brief Convert the texture to the desired pixel format and origin (if possible, else throw a exception)
    static void Convert(Texture& rTexture, const PixelFormat desiredPixelFormat, const BitmapOrigin desiredOrigin = BitmapOrigin::Undefined);

    static PxExtent3D GetExtentForLevel(const PxExtent3D& extent, const uint32_t level);
    static uint32_t CalcTotalTexels(const PxExtent3D& extent, const TextureInfo& textureInfo);

    //! @brief Try to change the origin of the texture
    static bool TryChangeOrigin(Texture& rTexture, const BitmapOrigin desiredOrigin);
  };
}

#endif
