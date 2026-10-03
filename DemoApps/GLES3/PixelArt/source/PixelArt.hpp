#ifndef GLES3_PIXELART_PIXELART_HPP
#define GLES3_PIXELART_PIXELART_HPP
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

#include <FslBase/ITag.hpp>
#include <FslDemoApp/OpenGLES3/DemoAppGLES3.hpp>
#include <FslUtil/OpenGLES3/GLGpuFrameTimer.hpp>
#include <Shared/PixelArt/API/OpenGLES3/PixelArtRendererGLES3.hpp>
#include <Shared/PixelArt/Base/PixelArtShared.hpp>

namespace Fsl
{
  //! Tells the app if the host could create a sRGB window surface
  class PixelArtUserTag final : public ITag
  {
  public:
    bool SrgbFramebuffer{false};
  };

  //! Plays the shadertoy style scenes of PixelArt/Shaders with OpenGL ES 3
  class PixelArt final : public DemoAppGLES3
  {
    using base_type = DemoAppGLES3;

    bool m_srgbFramebuffer;
    PixelArtRendererGLES3 m_renderer;
    //! The scene switcher, the UI and everything else that is the same for the OpenGL ES and the Vulkan app
    PixelArtShared m_shared;
    //! The GPU time of a frame, for the chart
    GLES3::GLGpuFrameTimer m_gpuTimer;
    uint64_t m_gpuMeasurementId{0};

  public:
    explicit PixelArt(const DemoAppConfig& config);

  protected:
    void OnKeyEvent(const KeyEvent& event) final;
    void OnMouseButtonEvent(const MouseButtonEvent& event) final;
    void OnMouseMoveEvent(const MouseMoveEvent& event) final;
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics) final;
    void Update(const DemoTime& demoTime) final;
    void Draw(const FrameInfo& frameInfo) final;
  };
}

#endif
