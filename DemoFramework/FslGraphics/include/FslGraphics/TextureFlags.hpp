#ifndef FSLGRAPHICS_TEXTUREFLAGS_HPP
#define FSLGRAPHICS_TEXTUREFLAGS_HPP
/****************************************************************************************************************************************************
 * Copyright (c) 2015 Freescale Semiconductor, Inc.
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

namespace Fsl
{
  enum class TextureFlags : uint32_t
  {
    // A empty flag
    NotDefined = 0x00,

    GenerateMipMaps = 0x01,
    //! If this is true any bitmap origin is allowed on create
    AllowAnyBitmapOrigin = 0x02,
    //! If this is true we want to use a exact texture format match
    ExactFormat = 0x04,
  };

  inline constexpr TextureFlags operator|(const TextureFlags lhs, const TextureFlags rhs) noexcept
  {
    return static_cast<TextureFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
  }

  inline constexpr TextureFlags operator&(const TextureFlags lhs, const TextureFlags rhs) noexcept
  {
    return static_cast<TextureFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
  }


  namespace TextureFlagsUtil
  {
    inline constexpr bool IsEnabled(const TextureFlags srcFlag, TextureFlags flag) noexcept
    {
      return (srcFlag & flag) == flag;
    }

    inline constexpr void Enable(TextureFlags& rDstFlag, TextureFlags flag) noexcept
    {
      rDstFlag = rDstFlag | flag;
    }


    inline constexpr void Disable(TextureFlags& rDstFlag, TextureFlags flag) noexcept
    {
      rDstFlag = rDstFlag & (static_cast<TextureFlags>(~static_cast<uint32_t>(flag)));
    }

    inline constexpr void Set(TextureFlags& rDstFlag, TextureFlags flag, const bool enabled) noexcept
    {
      rDstFlag = enabled ? (rDstFlag | flag) : (rDstFlag & (static_cast<TextureFlags>(~static_cast<uint32_t>(flag))));
    }
  };
}

#endif
