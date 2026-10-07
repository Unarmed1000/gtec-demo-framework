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
#include <exception>

namespace Fsl
{
  using namespace GLES2;

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
    Load(m_mandelbrot, contentManager, "Mandelbrot.frag");
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


  const RaymarchBackground::SceneProgram& RaymarchBackground::GetScene(const RaymarchScene scene) const noexcept
  {
    switch (scene)
    {
    case RaymarchScene::Blobs:
      return m_blobs;
    case RaymarchScene::Lace:
      return m_lace;
    case RaymarchScene::Mandelbrot:
      return m_mandelbrot;
    case RaymarchScene::Flight:
    case RaymarchScene::Hall:
      break;
    }
    // The two raymarched scenes share a shader, which is told the scene
    return m_raymarch;
  }


  void RaymarchBackground::Draw(const RaymarchParams& params, const PxSize2D sizePx)
  {
    const SceneProgram& scene = GetScene(params.Scene);
    if (params.Steps <= 0 || !scene.Program.IsValid())
    {
      return;
    }

    // The background is opaque and covers the screen (the rest of the sample is drawn by the native batch, which sets its own state)
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glUseProgram(scene.Program.Get());
    // The fourth value is the scene
    glUniform4f(scene.LocPhase, params.TravelPhase, params.SwayPhase, params.MorphPhase, params.SceneAsFloat());
    glUniform2f(scene.LocResolution, static_cast<float>(sizePx.RawWidth()), static_cast<float>(sizePx.RawHeight()));
    glUniform1f(scene.LocSteps, static_cast<float>(params.Steps));

    glBindBuffer(m_vertexBuffer.GetTarget(), m_vertexBuffer.Get());
    m_vertexBuffer.EnableAttribArrays(scene.AttribLinks);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_vertexBuffer.DisableAttribArrays(scene.AttribLinks);

    glBindBuffer(m_vertexBuffer.GetTarget(), 0);
  }
}
