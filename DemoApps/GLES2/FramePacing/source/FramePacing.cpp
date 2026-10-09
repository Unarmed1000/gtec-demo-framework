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

#include "FramePacing.hpp"
#include <FslDemoApp/Base/FrameInfo.hpp>
#include <FslDemoHost/EGL/Config/Service/IEGLHostInfo.hpp>
#include <FslDemoService/Trace/ScopedTraceZone.hpp>
#include <GLES2/gl2.h>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! How often the clock of the GL is related to the clock of the framework again: the two drift apart
      constexpr uint32_t FramesPerCalibration = 60;
    }
  }


  FramePacing::FramePacing(const DemoAppConfig& config)
    : DemoAppGLES2(config)
    , m_shared(config, "GLES2.FramePacing", SamplePresentMethod::SwapInterval)
    , m_background(*GetContentManager())
    , m_swapInterval(config.DemoServiceProvider.Get<IEGLHostInfo>())
    , m_framesSinceCalibration(LocalConfig::FramesPerCalibration)
  {
    // Give the UI a chance to intercept the various DemoApp events.
    RegisterExtension(m_shared.GetUIDemoAppExtension());
    // What the swap interval can hold a frame for is up to the EGL config
    m_shared.SetPresentSwapIntervalMax(m_swapInterval.Max());
  }


  void FramePacing::OnKeyEvent(const KeyEvent& event)
  {
    base_type::OnKeyEvent(event);
    m_shared.OnKeyEvent(event);
  }


  void FramePacing::OnWindowFocusEvent(const WindowFocusEvent& event)
  {
    base_type::OnWindowFocusEvent(event);
    m_shared.OnWindowFocusEvent(event);
  }


  void FramePacing::ConfigurationChanged(const DemoWindowMetrics& windowMetrics)
  {
    base_type::ConfigurationChanged(windowMetrics);
    m_shared.ConfigurationChanged(windowMetrics);
  }


  void FramePacing::Update(const DemoTime& demoTime)
  {
    m_shared.Update(demoTime);
  }


  void FramePacing::Draw(const FrameInfo& /*frameInfo*/)
  {
    // The time the GPU finished a frame is a time of the GL, so its clock is related to the clock of the framework now and then. It
    // is done here, before the first command of the frame: reading the time of the GL sends what was issued so far to the GPU, and
    // here there is nothing to send.
    if (++m_framesSinceCalibration >= LocalConfig::FramesPerCalibration)
    {
      m_framesSinceCalibration = 0;
      const uint64_t calibrationId = m_gpuTimer.GetCalibrationId();
      m_gpuTimer.Calibrate();
      if (m_gpuTimer.GetCalibrationId() != calibrationId)
      {
        m_shared.AddGpuClockCalibration(m_gpuTimer.GetCalibrationReadTime());
      }
    }

    {
      // The GPU time is measured around all the commands of the frame (this also reads the times the GPU has for the earlier frames)
      const ScopedTraceZone traceZone(m_shared.TryGetTrace(), m_shared.GetGpuTimerZone());
      m_gpuTimer.BeginFrame(m_shared.GetFrameId());
      for (const auto& measurement : m_gpuTimer.GetNewMeasurements())
      {
        // A time of an earlier frame, the sample is told which
        m_shared.AddGpuTime(measurement.FrameTag, measurement.GpuTime, measurement.EndTime);
      }
    }

    const auto clearColor = FramePacingShared::ClearColor.ToVector4();
    glClearColor(clearColor.X, clearColor.Y, clearColor.Z, clearColor.W);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    {
      // The background is animated for the same time as the rest of the frame. The frame starts in the update with this present
      // method, so the parameters are only read here.
      const RaymarchParams raymarchParams = m_shared.GetRaymarchParams();
      const ScopedTraceZone traceZone(m_shared.TryGetTrace(), m_shared.GetBackgroundDrawZone());
      m_background.Draw(raymarchParams, GetWindowSizePx());
    }
    m_shared.Draw();
  }


  void FramePacing::EndDraw(const FrameInfo& frameInfo)
  {
    base_type::EndDraw(frameInfo);

    // The frame and the frame pacing marker were drawn and the host swaps the buffers after this. The GPU works on the frame from now
    // on and the app does not wait for it, so the frame pacer is given the GPU time of the last frame that was measured.
    m_gpuTimer.EndFrame();
    if (m_shared.IsFlushWanted())
    {
      // Asked for on the command line: the GPU is asked to work on the frame now. Without it the driver decides when, which is the
      // swap at the latest, so a frame whose swap is delayed below can be one the GPU only starts on after the wait.
      m_shared.MarkFlush();
      const ScopedTraceZone traceZone(m_shared.TryGetTrace(), m_shared.GetFlushZone());
      glFlush();
    }
    m_shared.EndFrame(m_gpuTimer.GetGpuTime());
    // The swap holds the frame for the refreshes the frame pacer gives the present. It was told the longest swap interval the EGL
    // config allows, so it asks for no more, and a frame the swap can not hold that long it holds by a time to wait until.
    m_swapInterval.Set(m_shared.GetSwapInterval());
    if (m_shared.IsPresentWaitPlanned())
    {
      m_shared.WaitForPresent();
    }
  }
}
