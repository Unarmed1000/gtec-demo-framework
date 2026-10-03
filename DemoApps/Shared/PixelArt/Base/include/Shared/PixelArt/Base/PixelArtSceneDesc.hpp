#ifndef SHARED_PIXELART_BASE_PIXELARTSCENEDESC_HPP
#define SHARED_PIXELART_BASE_PIXELARTSCENEDESC_HPP
//****************************************************************************************************************************************************
//* BSD 3-Clause License
//*
//* Copyright (c) 2026, Mana Battery
//* All rights reserved.
//*
//* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
//*
//* 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//* 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
//*    documentation and/or other materials provided with the distribution.
//* 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
//*    software without specific prior written permission.
//*
//* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
//* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
//* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//****************************************************************************************************************************************************

#include <FslBase/IO/Path.hpp>
#include <FslBase/IO/PathView.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Fsl
{
  namespace PixelArtConfig
  {
    //! Buffer A to D
    constexpr uint32_t BufferCount = 4;
    //! The buffers and the image
    constexpr uint32_t PassCount = BufferCount + 1;
    //! iChannel0 to iChannel3
    constexpr uint32_t ChannelCount = 4;
    //! The adjustable constants a scene can have (the shaders hold them in 16 vec4)
    constexpr uint32_t MaxParams = 64;

    //! The shader tree in the content folder: a folder per scene, and a Common folder
    constexpr IO::PathView ShaderRoot("PixelArt/Shaders");
    //! The list of scenes
    constexpr IO::PathView SceneList("PixelArt/Shaders/Scenes.json");
  }

  //! The passes of a scene, in the order they are drawn (the tabs of shadertoy)
  enum class PixelArtPass
  {
    BufferA = 0,
    BufferB = 1,
    BufferC = 2,
    BufferD = 3,
    Image = 4
  };

  //! What a iChannel reads
  enum class PixelArtChannelSource
  {
    Unused,
    //! A buffer: earlier in the frame it is the output of this frame, the pass itself or a later one gives the output of the last frame
    Buffer,
    //! A 2D texture from the content
    Texture,
    //! A cubemap from the content
    Cubemap
  };

  enum class PixelArtFilter
  {
    Nearest,
    Linear,
    Mipmap
  };

  enum class PixelArtWrap
  {
    Clamp,
    Repeat
  };

  //! What the image pass writes
  enum class PixelArtOutput
  {
    //! Linear colors, the GPU applies the gamma (a sRGB framebuffer, or the shader if there is none)
    Linear,
    //! Gamma corrected colors, like a shader on shadertoy that does its own gamma
    Gamma
  };

  enum class PixelArtParamType
  {
    Float,
    Int
  };

  //! One iChannel of a pass, with the settings of the channel on shadertoy
  struct PixelArtChannelDesc
  {
    PixelArtChannelSource Source{PixelArtChannelSource::Unused};
    //! The buffer (Source == Buffer)
    PixelArtPass Buffer{PixelArtPass::BufferA};
    //! The texture or cubemap in the content folder
    IO::Path Path;
    PixelArtFilter Filter{PixelArtFilter::Linear};
    PixelArtWrap Wrap{PixelArtWrap::Clamp};
    //! A texture: true if the first row is the bottom of the image (shadertoy's default)
    bool VFlip{true};
    //! A texture or cubemap: true if the GPU turns its sRGB colors into linear colors
    bool Srgb{false};
  };

  struct PixelArtPassDesc
  {
    bool Enabled{false};
    std::array<PixelArtChannelDesc, PixelArtConfig::ChannelCount> Channels{};
  };

  //! A adjustable constant of a scene. The shaders read it with PARAM(n), n is its index in the scene.
  struct PixelArtParamDesc
  {
    //! The name of the define in the shaders
    std::string Name;
    std::string Label;
    //! The UI shows the constants by group, in the order the groups are first used
    std::string Group;
    PixelArtParamType Type{PixelArtParamType::Float};
    float Default{0.0f};
    float Min{0.0f};
    float Max{1.0f};
    //! The value is a multiple of the step away from Min (0 = any value)
    float Step{0.0f};
    //! The fmt format of the value in the UI
    std::string Format;
  };

  //! A scene: the passes and the adjustable constants. It is read from PixelArt/Shaders/<Id>/Scene.json.
  struct PixelArtSceneDesc
  {
    //! The name of the scene folder
    std::string Id;
    std::string Name;
    std::string Description;
    PixelArtOutput Output{PixelArtOutput::Gamma};
    //! True if the scene has a Common tab (Common.glsl), it is put in front of every tab
    bool HasCommonTab{false};
    std::array<PixelArtPassDesc, PixelArtConfig::PassCount> Passes{};
    std::vector<PixelArtParamDesc> Params;

    [[nodiscard]] const PixelArtPassDesc& GetPass(const PixelArtPass pass) const
    {
      return Passes.at(static_cast<uint32_t>(pass));
    }
  };

  namespace PixelArtPassUtil
  {
    //! The name of the tab, which is also the name of its shader file (BufferA ... Image)
    const char* GetTabName(const PixelArtPass pass) noexcept;

    constexpr PixelArtPass FromIndex(const uint32_t index) noexcept
    {
      return static_cast<PixelArtPass>(index);
    }

    constexpr uint32_t ToIndex(const PixelArtPass pass) noexcept
    {
      return static_cast<uint32_t>(pass);
    }
  }
}

#endif
