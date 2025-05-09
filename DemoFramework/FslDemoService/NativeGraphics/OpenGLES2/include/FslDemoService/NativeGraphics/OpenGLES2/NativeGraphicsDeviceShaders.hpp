#ifndef FSLDEMOSERVICE_NATIVEGRAPHICS_OPENGLES2_NATIVEGRAPHICSDEVICESHADERS_HPP
#define FSLDEMOSERVICE_NATIVEGRAPHICS_OPENGLES2_NATIVEGRAPHICSDEVICESHADERS_HPP
/****************************************************************************************************************************************************
 * Copyright 2022 NXP
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

#include <FslBase/BasicTypes.hpp>
#include <FslBase/Span/ReadOnlySpan.hpp>
#include <FslGraphics/Vertices/VertexAttributeDescriptionArray.hpp>

namespace Fsl::GLES2
{
  class NativeGraphicsDeviceShaders
  {
  public:
    static ReadOnlySpan<uint8_t> GetPositionColorVertexShader();
    static ReadOnlySpan<uint8_t> GetPositionColorFragmentShader();
    static ReadOnlySpan<uint8_t> GetPositionColorTextureVertexShader();
    static ReadOnlySpan<uint8_t> GetPositionColorTextureFragmentShader();
    static ReadOnlySpan<uint8_t> GetPositionColorTextureSdfFragmentShader();

    static constexpr VertexAttributeDescriptionArray<3> VertexShaderVertexPositionColorTextureDecl = {
      VertexAttributeDescription(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0, "inVertexPosition"),
      VertexAttributeDescription(1, VertexElementFormat::Vector4, VertexElementUsage::Color, 0, "inVertexColor"),
      VertexAttributeDescription(2, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0, "inVertexTextureCoord")};

    static constexpr VertexAttributeDescriptionArray<2> VertexShaderVertexPositionColorDecl = {
      VertexAttributeDescription(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0, "inVertexPosition"),
      VertexAttributeDescription(1, VertexElementFormat::Vector4, VertexElementUsage::Color, 0, "inVertexColor")};
  };
}

#endif
