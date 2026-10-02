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

  //! Draws the raymarched background of the sample (its GPU load): a quad that covers the screen, the fragment shader does the work.
  class RaymarchBackground final
  {
    GLES2::GLProgram m_program;
    GLES2::GLVertexBuffer m_vertexBuffer;
    std::array<GLES2::GLVertexAttribLink, 1> m_attribLinks;
    GLint m_locPhase{-1};
    GLint m_locResolution{-1};
    GLint m_locSteps{-1};

  public:
    //! If the shader can not be compiled the background is not drawn (a warning is logged)
    explicit RaymarchBackground(const IContentManager& contentManager);

    //! Draw the background (it is not drawn if params.Steps is zero)
    //! @param sizePx the size of the window in pixels
    void Draw(const RaymarchParams& params, const PxSize2D sizePx);
  };
}

#endif
