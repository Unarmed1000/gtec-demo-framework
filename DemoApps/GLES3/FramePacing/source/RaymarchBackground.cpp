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

#include "RaymarchBackground.hpp"
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslDemoApp/Base/Service/Content/IContentManager.hpp>
#include <FslGraphics/Vertices/VertexPosition.hpp>
#include <algorithm>
#include <cmath>
#include <exception>

namespace Fsl
{
  using namespace GLES3;

  RaymarchBackground::RaymarchBackground(const IContentManager& contentManager)
  {
    constexpr std::array<VertexPosition, 4> Vertices = {
      VertexPosition(-1.0f, 1.0f, 0.0f),
      VertexPosition(-1.0f, -1.0f, 0.0f),
      VertexPosition(1.0f, 1.0f, 0.0f),
      VertexPosition(1.0f, -1.0f, 0.0f),
    };
    m_vertexBuffer.Reset(Vertices, GL_STATIC_DRAW);

    // Every scene that is not raymarched has a fragment shader of its own, with the same uniforms
    Load(m_raymarch, contentManager, "Raymarch.frag");
    Load(m_blobs, contentManager, "Blobs.frag");
    Load(m_lace, contentManager, "Lace.frag");
    Load(m_upscale, contentManager, "Upscale.frag");
    if (m_upscale.Program.IsValid())
    {
      m_upscale.LocSteps = m_upscale.Program.TryGetUniformLocation("Texture");
    }
  }


  void RaymarchBackground::Load(SceneProgram& rScene, const IContentManager& contentManager, const char* const pszFragmentShader)
  {
    try
    {
      rScene.Program.Reset(contentManager.ReadAllText("Raymarch.vert"), contentManager.ReadAllText(pszFragmentShader));
    }
    catch (const std::exception& ex)
    {
      // The sample works without its background, so a GPU that can not run the shader only loses the GPU load of that scene
      FSLLOG3_WARNING("The background '{}' is not available, the shader could not be compiled: {}", pszFragmentShader, ex.what());
      rScene.Program.Reset();
      return;
    }

    rScene.LocPhase = rScene.Program.TryGetUniformLocation("Phase");
    rScene.LocResolution = rScene.Program.TryGetUniformLocation("Resolution");
    rScene.LocSteps = rScene.Program.TryGetUniformLocation("Steps");

    constexpr auto VertexDecl = VertexPosition::GetVertexDeclarationArray();
    rScene.AttribLinks[0] =
      GLVertexAttribLink(rScene.Program.GetAttribLocation("VertexPosition"), VertexDecl.VertexElementGetIndexOf(VertexElementUsage::Position, 0u));
  }


  void RaymarchBackground::Draw(const RaymarchParams& params, const PxSize2D sizePx)
  {
    const SceneProgram& scene = params.Scene == RaymarchScene::Blobs ? m_blobs : (params.Scene == RaymarchScene::Lace ? m_lace : m_raymarch);
    if (params.Steps <= 0 || !scene.Program.IsValid())
    {
      return;
    }

    // The background is opaque and covers the screen (the rest of the sample is drawn by the native batch, which sets its own state)
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // At a lower resolution the scene is drawn into a part of a texture of the size of the window, which is then enlarged to the
    // screen. The texture is made when it is first needed and again when the window has another size.
    const bool scaled = params.RenderScale < 0.999f && m_upscale.Program.IsValid();
    PxSize2D drawSizePx = sizePx;
    GLint screenFrameBuffer = 0;
    if (scaled)
    {
      if (!m_offscreen.IsValid() || m_offscreen.GetSize() != sizePx)
      {
        m_offscreen.Reset(sizePx, GLTextureParameters(GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
                          GLTextureImageParameters(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE));
      }
      drawSizePx = PxSize2D::Create(std::max(static_cast<int32_t>(std::lround(static_cast<float>(sizePx.RawWidth()) * params.RenderScale)), 1),
                                    std::max(static_cast<int32_t>(std::lround(static_cast<float>(sizePx.RawHeight()) * params.RenderScale)), 1));
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &screenFrameBuffer);
      glBindFramebuffer(GL_FRAMEBUFFER, m_offscreen.Get());
    }
    glViewport(0, 0, drawSizePx.RawWidth(), drawSizePx.RawHeight());

    glUseProgram(scene.Program.Get());
    // The fourth value is the scene
    glUniform4f(scene.LocPhase, params.TravelPhase, params.SwayPhase, params.MorphPhase, params.SceneAsFloat());
    glUniform2f(scene.LocResolution, static_cast<float>(drawSizePx.RawWidth()), static_cast<float>(drawSizePx.RawHeight()));
    glUniform1f(scene.LocSteps, static_cast<float>(params.Steps));

    glBindBuffer(m_vertexBuffer.GetTarget(), m_vertexBuffer.Get());
    m_vertexBuffer.EnableAttribArrays(scene.AttribLinks);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_vertexBuffer.DisableAttribArrays(scene.AttribLinks);

    if (scaled)
    {
      // The part of the texture that was drawn into is enlarged to the screen
      glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screenFrameBuffer));
      glViewport(0, 0, sizePx.RawWidth(), sizePx.RawHeight());
      const auto textureWidth = static_cast<float>(sizePx.RawWidth());
      const auto textureHeight = static_cast<float>(sizePx.RawHeight());
      const auto drawWidth = static_cast<float>(drawSizePx.RawWidth());
      const auto drawHeight = static_cast<float>(drawSizePx.RawHeight());
      glUseProgram(m_upscale.Program.Get());
      // The part that was drawn into, and the middle of its last pixels: a sample further out would mix in what is outside it
      glUniform4f(m_upscale.LocPhase, drawWidth / textureWidth, drawHeight / textureHeight, (drawWidth - 0.5f) / textureWidth,
                  (drawHeight - 0.5f) / textureHeight);
      glUniform2f(m_upscale.LocResolution, textureWidth, textureHeight);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, m_offscreen.GetTextureInfo().Handle);
      glUniform1i(m_upscale.LocSteps, 0);
      m_vertexBuffer.EnableAttribArrays(m_upscale.AttribLinks);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      m_vertexBuffer.DisableAttribArrays(m_upscale.AttribLinks);
      glBindTexture(GL_TEXTURE_2D, 0);
    }
    glBindBuffer(m_vertexBuffer.GetTarget(), 0);
  }
}
