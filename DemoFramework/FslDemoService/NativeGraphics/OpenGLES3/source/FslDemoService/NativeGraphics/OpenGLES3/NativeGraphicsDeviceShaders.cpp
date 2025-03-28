/****************************************************************************************************************************************************
 * Copyright 2022, 2024 NXP
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

#include <FslBase/Log/Log3Core.hpp>
#include <FslDemoService/NativeGraphics/OpenGLES3/NativeGraphicsDeviceShaders.hpp>
#include <cstring>

namespace Fsl::GLES3
{
  namespace
  {
    constexpr const char* const PositionColorVertexShader =
      "#version 300 es\n"
      "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
      "precision highp float;\n"
      "#else\n"
      "precision mediump float;\n"
      "#endif\n"
      "uniform mat4 MatModelViewProj;\n"
      "\n"
      "layout(location = 0) in vec4 inVertexPosition;\n"
      "layout(location = 1) in vec4 inVertexColor;\n"
      "\n"
      "out vec4 FragColor;\n"
      "\n"
      "void main()\n"
      "{"
      "  gl_Position = MatModelViewProj * inVertexPosition;\n"
      "  FragColor = inVertexColor;\n"
      "}";


    constexpr const char* const PositionColorFragmentShader =
      "#version 300 es\n"
      "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
      "precision highp float;\n"
      "#else\n"
      "precision mediump float;\n"
      "#endif\n"
      "\n"
      "uniform sampler2D Texture;\n"
      "\n"
      "in vec4 FragColor;\n"
      "\n"
      "out vec4 o_fragColor;\n"
      "\n"
      "void main()\n"
      "{\n"
      "  o_fragColor = FragColor;\n"
      "}\n";


    constexpr const char* const PositionColorTextureVertexShader =
      "#version 300 es\n"
      "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
      "precision highp float;\n"
      "#else\n"
      "precision mediump float;\n"
      "#endif\n"
      "uniform mat4 MatModelViewProj;\n"
      "\n"
      "layout(location = 0) in vec4 inVertexPosition;\n"
      "layout(location = 1) in vec4 inVertexColor;\n"
      "layout(location = 2) in vec2 inVertexTextureCoord;\n"
      "\n"
      "out vec4 FragColor;\n"
      "out vec2 FragTextureCoord;\n"
      "\n"
      "void main()\n"
      "{"
      "  gl_Position = MatModelViewProj * inVertexPosition;\n"
      "  FragColor = inVertexColor;\n"
      "  FragTextureCoord = inVertexTextureCoord;\n"
      "}";


    constexpr const char* const PositionColorTextureFragmentShader =
      "#version 300 es\n"
      "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
      "precision highp float;\n"
      "#else\n"
      "precision mediump float;\n"
      "#endif\n"
      "\n"
      "uniform sampler2D Texture;\n"
      "\n"
      "in vec4 FragColor;\n"
      "in vec2 FragTextureCoord;\n"
      "\n"
      "out vec4 o_fragColor;\n"
      "\n"
      "void main()\n"
      "{\n"
      "  o_fragColor = texture(Texture,FragTextureCoord) * FragColor;\n"
      "}\n";


    constexpr const char* const PositionColorTextureSDFFragmentShader =
      "#version 300 es\n"
      "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
      "precision highp float;\n"
      "#else\n"
      "precision mediump float;\n"
      "#endif\n"
      "\n"
      "uniform sampler2D Texture;\n"
      "uniform float Smoothing;\n"
      "\n"
      "in vec4 FragColor;\n"
      "in vec2 FragTextureCoord;\n"
      "\n"
      "out vec4 o_fragColor;\n"
      "\n"
      "void main()\n"
      "{\n"
      "  float distance = texture(Texture, FragTextureCoord).a;\n"
      "  float alpha = smoothstep(0.5 - Smoothing, 0.5 + Smoothing, distance);\n"
      "  o_fragColor = vec4(FragColor.rgb, FragColor.a * alpha);\n"
      "}\n";
  }

  ReadOnlySpan<uint8_t> NativeGraphicsDeviceShaders::GetPositionColorVertexShader()
  {
    // +1 to include the zero termination.
    return ReadOnlySpan<uint8_t>(reinterpret_cast<const uint8_t*>(PositionColorVertexShader), strlen(PositionColorVertexShader) + 1);
  }

  ReadOnlySpan<uint8_t> NativeGraphicsDeviceShaders::GetPositionColorFragmentShader()
  {
    // +1 to include the zero termination
    return ReadOnlySpan<uint8_t>(reinterpret_cast<const uint8_t*>(PositionColorFragmentShader), strlen(PositionColorFragmentShader) + 1);
  }

  ReadOnlySpan<uint8_t> NativeGraphicsDeviceShaders::GetPositionColorTextureVertexShader()
  {
    // +1 to include the zero termination.
    return ReadOnlySpan<uint8_t>(reinterpret_cast<const uint8_t*>(PositionColorTextureVertexShader), strlen(PositionColorTextureVertexShader) + 1);
  }

  ReadOnlySpan<uint8_t> NativeGraphicsDeviceShaders::GetPositionColorTextureFragmentShader()
  {
    // +1 to include the zero termination
    return ReadOnlySpan<uint8_t>(reinterpret_cast<const uint8_t*>(PositionColorTextureFragmentShader),
                                 strlen(PositionColorTextureFragmentShader) + 1);
  }

  ReadOnlySpan<uint8_t> NativeGraphicsDeviceShaders::GetPositionColorTextureSdfFragmentShader()
  {
    // +1 to include the zero termination
    return ReadOnlySpan<uint8_t>(reinterpret_cast<const uint8_t*>(PositionColorTextureSDFFragmentShader),
                                 strlen(PositionColorTextureSDFFragmentShader) + 1);
  }
}
