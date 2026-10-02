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
    try
    {
      m_program.Reset(contentManager.ReadAllText("Raymarch.vert"), contentManager.ReadAllText("Raymarch.frag"));
    }
    catch (const std::exception& ex)
    {
      // The sample works without its background, so a GPU that can not run the shader only loses the GPU load
      FSLLOG3_WARNING("The raymarched background is not available, the shader could not be compiled: {}", ex.what());
      m_program.Reset();
      return;
    }

    m_locPhase = m_program.TryGetUniformLocation("Phase");
    m_locResolution = m_program.TryGetUniformLocation("Resolution");
    m_locSteps = m_program.TryGetUniformLocation("Steps");

    constexpr std::array<VertexPosition, 4> Vertices = {
      VertexPosition(-1.0f, 1.0f, 0.0f),
      VertexPosition(-1.0f, -1.0f, 0.0f),
      VertexPosition(1.0f, 1.0f, 0.0f),
      VertexPosition(1.0f, -1.0f, 0.0f),
    };
    m_vertexBuffer.Reset(Vertices, GL_STATIC_DRAW);

    constexpr auto VertexDecl = VertexPosition::GetVertexDeclarationArray();
    m_attribLinks[0] =
      GLVertexAttribLink(m_program.GetAttribLocation("VertexPosition"), VertexDecl.VertexElementGetIndexOf(VertexElementUsage::Position, 0u));
  }


  void RaymarchBackground::Draw(const RaymarchParams& params, const PxSize2D sizePx)
  {
    if (params.Steps <= 0 || !m_program.IsValid())
    {
      return;
    }

    // The background is opaque and covers the screen (the rest of the sample is drawn by the native batch, which sets its own state)
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glViewport(0, 0, sizePx.RawWidth(), sizePx.RawHeight());

    glUseProgram(m_program.Get());
    // The fourth value is the scene
    glUniform4f(m_locPhase, params.TravelPhase, params.SwayPhase, params.MorphPhase, params.SceneAsFloat());
    glUniform2f(m_locResolution, static_cast<float>(sizePx.RawWidth()), static_cast<float>(sizePx.RawHeight()));
    glUniform1f(m_locSteps, static_cast<float>(params.Steps));

    glBindBuffer(m_vertexBuffer.GetTarget(), m_vertexBuffer.Get());
    m_vertexBuffer.EnableAttribArrays(m_attribLinks);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_vertexBuffer.DisableAttribArrays(m_attribLinks);
    glBindBuffer(m_vertexBuffer.GetTarget(), 0);
  }
}
