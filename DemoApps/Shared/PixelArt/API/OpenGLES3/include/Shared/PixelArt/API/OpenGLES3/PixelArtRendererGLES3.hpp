#ifndef SHARED_PIXELART_API_OPENGLES3_PIXELARTRENDERERGLES3_HPP
#define SHARED_PIXELART_API_OPENGLES3_PIXELARTRENDERERGLES3_HPP
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

#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslUtil/OpenGLES3/GLFrameBuffer.hpp>
#include <FslUtil/OpenGLES3/GLProgram.hpp>
#include <FslUtil/OpenGLES3/GLTexture.hpp>
#include <FslUtil/OpenGLES3/GLVertexArray.hpp>
#include <Shared/PixelArt/Base/IPixelArtSceneRenderer.hpp>
#include <Shared/PixelArt/Base/PixelArtFrameState.hpp>
#include <Shared/PixelArt/Base/PixelArtSceneDesc.hpp>
#include <array>
#include <map>
#include <memory>
#include <string>

namespace Fsl
{
  class IContentManager;

  //! Draws the PixelArt scenes with OpenGL ES 3: a fullscreen triangle per pass, the buffers are RGBA16F textures (two per buffer, so a
  //! buffer can read what it drew the frame before).
  class PixelArtRendererGLES3 final : public IPixelArtSceneRenderer
  {
    //! The GL sampler objects for every filter and wrap a channel can have
    class SamplerCache
    {
      std::array<GLuint, 6> m_samplers{};

    public:
      SamplerCache();
      ~SamplerCache() noexcept;
      SamplerCache(const SamplerCache&) = delete;
      SamplerCache& operator=(const SamplerCache&) = delete;
      SamplerCache(SamplerCache&&) = delete;
      SamplerCache& operator=(SamplerCache&&) = delete;

      [[nodiscard]] GLuint Get(const PixelArtFilter filter, const PixelArtWrap wrap) const noexcept;
    };

    struct ChannelRecord
    {
      PixelArtChannelSource Source{PixelArtChannelSource::Unused};
      //! The buffer (Source == Buffer)
      uint32_t BufferIndex{0};
      //! The texture or cubemap (otherwise the black texture of unused channels)
      std::shared_ptr<GLES3::GLTexture> Texture;
      GLuint Sampler{0};
    };

    struct PassRecord
    {
      bool Enabled{false};
      GLES3::GLProgram Program;
      GLint LocResolution{-1};
      GLint LocTime{-1};
      GLint LocTimeDelta{-1};
      GLint LocFrameRate{-1};
      GLint LocFrame{-1};
      GLint LocMouse{-1};
      GLint LocDate{-1};
      GLint LocSampleRate{-1};
      GLint LocChannelTime{-1};
      GLint LocChannelResolution{-1};
      GLint LocParams{-1};
      GLint LocPass{-1};
      std::array<GLint, PixelArtConfig::ChannelCount> LocChannels{-1, -1, -1, -1};
      std::array<ChannelRecord, PixelArtConfig::ChannelCount> Channels{};
    };

    struct SceneRecord
    {
      PixelArtOutput Output{PixelArtOutput::Gamma};
      std::array<PassRecord, PixelArtConfig::PassCount> Passes{};
    };

    std::shared_ptr<IContentManager> m_contentManager;
    bool m_srgbFramebuffer;
    std::string m_vertexShader;
    std::string m_prologue;
    GLES3::GLVertexArray m_vertexArray;
    SamplerCache m_samplers;
    //! A black texture for the channels a pass does not use
    std::shared_ptr<GLES3::GLTexture> m_blackTexture;
    //! The textures and cubemaps the scenes use, by path and settings
    std::map<std::string, std::shared_ptr<GLES3::GLTexture>> m_textures;
    std::unique_ptr<SceneRecord> m_scene;
    //! Two targets per buffer, m_currentTarget says which one has the last output of the buffer
    std::array<std::array<GLES3::GLFrameBuffer, 2>, PixelArtConfig::BufferCount> m_targets;
    std::array<uint32_t, PixelArtConfig::BufferCount> m_currentTarget{};
    PxSize2D m_targetSizePx;

  public:
    //! @param srgbFramebuffer true if the window framebuffer is sRGB (the GPU then encodes what the image pass writes)
    PixelArtRendererGLES3(std::shared_ptr<IContentManager> contentManager, const bool srgbFramebuffer);
    ~PixelArtRendererGLES3() final;

    void LoadScene(const PixelArtSceneDesc& scene) final;

    //! Draw all the passes of the scene, the image pass draws to the window framebuffer
    void Draw(const PixelArtFrameState& frameState);

  private:
    std::shared_ptr<GLES3::GLTexture> GetTexture(const PixelArtChannelDesc& channel);
    [[nodiscard]] std::string BuildFragmentShader(const PixelArtSceneDesc& scene, const PixelArtPass pass) const;
    void ResizeTargets(const PxSize2D sizePx);
    void ClearTargets();
    void DrawPass(const PassRecord& pass, const PixelArtFrameState& frameState, const PxSize2D passSizePx, const bool isImagePass);
  };
}

#endif
