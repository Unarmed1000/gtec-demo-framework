#ifndef GLES2_FRAMEPACING_FRAMEPACING_HPP
#define GLES2_FRAMEPACING_FRAMEPACING_HPP
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

#include <FslDemoApp/OpenGLES2/DemoAppGLES2.hpp>
#include <FslUtil/OpenGLES2/GLGpuFrameTimer.hpp>
#include <Shared/FramePacing/EGL/EGLSwapInterval.hpp>
#include <Shared/FramePacing/FramePacingShared.hpp>
#include "RaymarchBackground.hpp"

namespace Fsl
{
  class FramePacing final : public DemoAppGLES2
  {
    using base_type = DemoAppGLES2;

    //! All the actual sample code can be found in the shared class since its reused for all FramePacing samples.
    FramePacingShared m_shared;

    //! The raymarched background (the GPU load of the sample)
    RaymarchBackground m_background;
    //! The number of display refreshes the swap of the host holds a frame for
    EGLSwapInterval m_swapInterval;
    //! The time the GPU works on a frame (GL_EXT_disjoint_timer_query), read without waiting for the GPU
    GLES2::GLGpuFrameTimer m_gpuTimer;
    //! The frames since the clock of the GL was related to the clock of the framework (the first frame does it)
    uint32_t m_framesSinceCalibration;

  public:
    explicit FramePacing(const DemoAppConfig& config);

  protected:
    void OnKeyEvent(const KeyEvent& event) final;
    void OnWindowFocusEvent(const WindowFocusEvent& event) final;
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics) final;
    void Update(const DemoTime& demoTime) final;
    void Draw(const FrameInfo& frameInfo) final;
    void EndDraw(const FrameInfo& frameInfo) final;
  };
}

#endif
