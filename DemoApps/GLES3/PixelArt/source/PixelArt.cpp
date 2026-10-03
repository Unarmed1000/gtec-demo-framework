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

#include "PixelArt.hpp"
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslDemoApp/Base/FrameInfo.hpp>
#include <GLES3/gl3.h>
#include <memory>

namespace Fsl
{
  namespace
  {
    bool IsSrgbFramebuffer(const std::shared_ptr<ITag>& tag)
    {
      const auto userTag = std::dynamic_pointer_cast<PixelArtUserTag>(tag);
      return userTag && userTag->SrgbFramebuffer;
    }
  }


  PixelArt::PixelArt(const DemoAppConfig& config)
    : DemoAppGLES3(config)
    , m_srgbFramebuffer(IsSrgbFramebuffer(config.CustomConfig.AppRegistrationUserTag))
    , m_renderer(GetContentManager(), m_srgbFramebuffer)
    , m_shared(config, m_renderer, m_srgbFramebuffer)
  {
    FSLLOG3_INFO("sRGB framebuffer: {}", m_srgbFramebuffer ? "yes" : "no, the shader applies the gamma");
    m_shared.SetGpuTimerSupported(m_gpuTimer.IsSupported());
    // Give the UI a chance to intercept the various DemoApp events.
    RegisterExtension(m_shared.GetUIDemoAppExtension());
  }


  void PixelArt::OnKeyEvent(const KeyEvent& event)
  {
    base_type::OnKeyEvent(event);
    m_shared.OnKeyEvent(event);
  }


  void PixelArt::OnMouseButtonEvent(const MouseButtonEvent& event)
  {
    base_type::OnMouseButtonEvent(event);
    m_shared.OnMouseButtonEvent(event);
  }


  void PixelArt::OnMouseMoveEvent(const MouseMoveEvent& event)
  {
    base_type::OnMouseMoveEvent(event);
    m_shared.OnMouseMoveEvent(event);
  }


  void PixelArt::ConfigurationChanged(const DemoWindowMetrics& windowMetrics)
  {
    base_type::ConfigurationChanged(windowMetrics);
    m_shared.ConfigurationChanged(windowMetrics);
  }


  void PixelArt::Update(const DemoTime& demoTime)
  {
    m_shared.Update(demoTime);
  }


  void PixelArt::Draw(const FrameInfo& /*frameInfo*/)
  {
    m_gpuTimer.BeginFrame();
    if (m_gpuTimer.GetMeasurementId() != m_gpuMeasurementId)
    {
      m_gpuMeasurementId = m_gpuTimer.GetMeasurementId();
      m_shared.AddGpuTime(m_gpuTimer.GetGpuTime());
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    m_renderer.Draw(m_shared.GetFrameState());
    m_shared.Draw();
    m_gpuTimer.EndFrame();
  }
}
