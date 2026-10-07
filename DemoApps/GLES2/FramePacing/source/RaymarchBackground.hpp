#ifndef GLES2_FRAMEPACING_RAYMARCHBACKGROUND_HPP
#define GLES2_FRAMEPACING_RAYMARCHBACKGROUND_HPP
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
#include <FslUtil/OpenGLES2/GLProgram.hpp>
#include <FslUtil/OpenGLES2/GLVertexAttribLink.hpp>
#include <FslUtil/OpenGLES2/GLVertexBuffer.hpp>
#include <Shared/FramePacing/RaymarchParams.hpp>
#include <GLES2/gl2.h>
#include <array>

namespace Fsl
{
  class IContentManager;

  //! Draws the background of the sample (its GPU load): a quad that covers the screen, the fragment shader of the scene does the work.
  class RaymarchBackground final
  {
    //! The shader of a scene and where its uniforms and its vertex attribute are
    struct SceneProgram
    {
      GLES2::GLProgram Program;
      std::array<GLES2::GLVertexAttribLink, 1> AttribLinks;
      GLint LocPhase{-1};
      GLint LocResolution{-1};
      GLint LocSteps{-1};
    };

    //! The raymarched scenes (the flight and the hall)
    SceneProgram m_raymarch;
    SceneProgram m_blobs;
    SceneProgram m_lace;
    SceneProgram m_mandelbrot;
    GLES2::GLVertexBuffer m_vertexBuffer;

    //! If the shader can not be compiled the scene is not drawn (a warning is logged)
    static void Load(SceneProgram& rScene, const IContentManager& contentManager, const char* const pszFragmentShader);
    //! The shader that draws a scene
    [[nodiscard]] const SceneProgram& GetScene(const RaymarchScene scene) const noexcept;

  public:
    //! If the shader of a scene can not be compiled that scene is not drawn (a warning is logged)
    explicit RaymarchBackground(const IContentManager& contentManager);

    //! Draw the background (it is not drawn if params.Steps is zero).
    //! @param sizePx the size of the window in pixels
    void Draw(const RaymarchParams& params, const PxSize2D sizePx);
  };
}

#endif
