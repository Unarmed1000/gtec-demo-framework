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
#include <FslBase/Time/TimeSpan.hpp>
#include <FslBase/UncheckedNumericCast.hpp>
#include <FslDemoService/Trace/ScopedTraceZone.hpp>
#include <array>
#include <optional>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! How old the read of the device clock and the clock of the framework may get before they are read again. The two do not
      //! run at the same rate, and by time and not by frames a slow frame rate is converted as well as a fast one.
      constexpr TimeSpan MaxCalibrationAge = TimeSpan::FromMilliseconds(250);
    }

    constexpr SampleSwapchainRefresh ToSampleSwapchainRefresh(const Vulkan::VUPresentRefreshMode mode) noexcept
    {
      switch (mode)
      {
      case Vulkan::VUPresentRefreshMode::Fixed:
        return SampleSwapchainRefresh::Fixed;
      case Vulkan::VUPresentRefreshMode::Variable:
        return SampleSwapchainRefresh::Variable;
      case Vulkan::VUPresentRefreshMode::Unknown:
      default:
        return SampleSwapchainRefresh::Unknown;
      }
    }

    VulkanBasic::DemoAppVulkanSetup CreateSetup()
    {
      VulkanBasic::DemoAppVulkanSetup setup;
      // Measure when the frames reach the display (if VK_EXT_present_timing is available)
      setup.PresentTiming = true;
      // Everything this app writes for a frame is per frame slot (push constants, the GPU timer, the present ids, and the batch
      // and the UI it draws with), so its frames in flight can overlap on the GPU
      setup.WaitForLastUseOfSwapchainImage = false;
      return setup;
    }
  }


  FramePacing::FramePacing(const DemoAppConfig& config)
    : VulkanBasic::DemoAppVulkanBasic(config, CreateSetup())
    , m_shared(config, "Vulkan.FramePacing", SamplePresentMethod::WaitThenPresent)
    , m_background(m_device, *GetContentManager())
    , m_gpuTimer(m_device, m_deviceQueue.QueueFamilyIndex, GetRenderConfig().MaxFramesInFlight)
    , m_gpuTimeCalibration(m_calibratedTimestamps, m_gpuTimer.GetTimestampPeriod(), m_gpuTimer.GetTimestampMask())
    , m_slotPresentIds(GetRenderConfig().MaxFramesInFlight)
    , m_slotFrameIds(GetRenderConfig().MaxFramesInFlight)
  {
    // Optional: with VK_KHR_calibrated_timestamps the GPU work of a frame can be placed on the timeline of the CPU
    m_gpuTimeCalibration.Calibrate();
    // Both measurements are optional and have a switch in the UI, so what they add can be seen
    m_shared.SetMeasurementSupport(IsPresentTimingSupported(), m_gpuTimeCalibration.IsSupported());
    // Give the UI a chance to intercept the various DemoApp events.
    RegisterExtension(m_shared.GetUIDemoAppExtension());
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
    // A pacer that names a present to wait for needs a swapchain that can be waited on (--VkPresentWait n), and n is then the
    // number of presents it lets wait
    m_shared.SetPresentWaitSupport(IsPresentWaitEnabled() ? GetPresentWaitFramesBack() : 0u);
    // The app base can wait until the GPU is done with a frame the pacer names: it is its wait for a frame slot
    m_shared.SetGpuWaitSupport(true);
    m_shared.Update(demoTime);
    // Follow the switch of the UI (a change recreates the swapchain before the next frame)
    SetPresentTimingRequested(m_shared.IsPresentTimingWanted());
    // With the frame pacer on the wait for a present is made below (OnVulkanFrameStart), where the pacer names one, and not by the
    // app base
    SetPresentWaitByApp(!m_shared.IsPresentWaitByHost());
  }


  void FramePacing::EndDraw(const FrameInfo& frameInfo)
  {
    base_type::EndDraw(frameInfo);

    // The frame was submitted to the GPU and is presented after this. The GPU works on it from now on, so the frame pacer is given the
    // GPU time of the last frame that was measured.
    m_shared.EndFrame(m_gpuTimer.GetGpuTime());
    // A FIFO present holds a frame for one refresh and there is no swap interval, so the frame pacer holds a frame of more than one
    // refresh by a time to wait until before the present. Where the swapchain can do it and the sample is asked to, the present is
    // also given the time the frame before stays on screen at least (zero is no such time).
    m_shared.WaitForPresent();
    SetPresentRelativeTargetTime(m_shared.GetPresentRelativeTarget());
  }


  void FramePacing::VulkanDraw(const DemoTime& /*demoTime*/, RapidVulkan::CommandBuffers& rCmdBuffers, const VulkanBasic::DrawContext& drawContext)
  {
    const uint32_t currentFrameIndex = drawContext.CurrentFrameIndex;

    {    // The waits of the app base before this frame, which the frame pacer did not ask for: it is told before the frame starts
      const VulkanBasic::FrameStartWaitRecord& waits = GetCurrentFrameStartWaits();
      m_shared.AddSystemWait(SamplePacerSystemWait::FrameSlot, waits.FrameSlotWaitBeginTime, waits.FrameSlotWaitEndTime);
      m_shared.AddSystemWait(SamplePacerSystemWait::Acquire, waits.AcquireCallTime, waits.AcquireReturnTime);
    }

    const VkCommandBuffer hCmdBuffer = rCmdBuffers[currentFrameIndex];
    rCmdBuffers.Begin(currentFrameIndex, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, VK_FALSE, 0, 0);
    {
      {
        const ScopedTraceZone traceZone(m_shared.TryGetTrace(), m_shared.GetGpuTimerZone());
        m_gpuTimer.BeginFrame(hCmdBuffer, currentFrameIndex);
      }
      {
        const ScopedTraceZone traceZone(m_shared.TryGetTrace(), m_shared.GetMeasurementsZone());
        UpdateMeasurements(currentFrameIndex);
      }

      const auto clearColor = FramePacingShared::ClearColor.ToVector4();
      std::array<VkClearValue, 1> clearValues{};
      clearValues[0].color = {{clearColor.X, clearColor.Y, clearColor.Z, clearColor.W}};

      VkRenderPassBeginInfo renderPassBeginInfo{};
      renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderPassBeginInfo.renderPass = m_dependentResources.MainRenderPass.Get();
      renderPassBeginInfo.framebuffer = drawContext.Framebuffer;
      renderPassBeginInfo.renderArea.offset.x = 0;
      renderPassBeginInfo.renderArea.offset.y = 0;
      renderPassBeginInfo.renderArea.extent = drawContext.SwapchainImageExtent;
      renderPassBeginInfo.clearValueCount = UncheckedNumericCast<uint32_t>(clearValues.size());
      renderPassBeginInfo.pClearValues = clearValues.data();

      // The frame starts here for the sample, before the render pass: the background is animated for the animation time of the frame
      const RaymarchParams raymarchParams = m_shared.GetRaymarchParams();

      rCmdBuffers.CmdBeginRenderPass(currentFrameIndex, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
      {
        {
          // The background is animated for the same time as the rest of the frame
          const ScopedTraceZone traceZone(m_shared.TryGetTrace(), m_shared.GetBackgroundDrawZone());
          m_background.Draw(hCmdBuffer, raymarchParams);
        }
        {    // The frame has started, so it can be given the id its present will get. The measurements of the frame refer to it.
          const uint64_t presentId = GetNextPresentId();
          m_shared.SetFramePresentId(presentId);
          m_slotPresentIds[currentFrameIndex] = presentId;
          m_slotFrameIds[currentFrameIndex] = m_shared.GetFrameId();
        }
        m_shared.Draw();

        // Remember to call this as the last operation in your renderPass (this is also where the frame pacing marker is drawn)
        AddSystemUI(hCmdBuffer, currentFrameIndex);
      }
      rCmdBuffers.CmdEndRenderPass(currentFrameIndex);

      m_gpuTimer.EndFrame(hCmdBuffer, currentFrameIndex);
    }
    rCmdBuffers.End(currentFrameIndex);
  }


  void FramePacing::UpdateMeasurements(const uint32_t currentFrameIndex)
  {
    // The GPU time that was just read is the one of the frame that used the frame slot before this one
    if (m_gpuTimer.GetMeasurementId() != m_gpuMeasurementId)
    {
      m_gpuMeasurementId = m_gpuTimer.GetMeasurementId();
      m_shared.AddGpuTime(m_slotFrameIds[currentFrameIndex], m_gpuTimer.GetGpuTime());
      TickCount gpuStartTime;
      TickCount gpuEndTime;
      if (m_shared.IsGpuTimelineWanted() && m_gpuTimeCalibration.TryToHostTime(m_gpuTimer.GetBeginTimestamp(), gpuStartTime) &&
          m_gpuTimeCalibration.TryToHostTime(m_gpuTimer.GetEndTimestamp(), gpuEndTime))
      {
        m_shared.AddGpuInterval(m_slotPresentIds[currentFrameIndex], gpuStartTime, gpuEndTime);
      }
    }
    // Read the device clock and the clock of the framework again now and then, so they do not drift apart. The log says how far
    // off a read can be and what the rate of the device clock was measured to be.
    if (m_gpuTimeCalibration.CalibrateIfOlderThan(LocalConfig::MaxCalibrationAge))
    {
      m_shared.AddGpuClockCalibration(
        m_gpuTimeCalibration.GetLastReadTime(), m_gpuTimeCalibration.GetCalibration().MaxDeviation,
        m_gpuTimeCalibration.HasMeasuredClockRate() ? std::optional<double>(m_gpuTimeCalibration.GetClockRateDeviationPpm()) : std::nullopt);
    }

    // When the frames reached the display, if the swapchain measures it (VK_EXT_present_timing). The measurements arrive a few frames
    // after the present.
    m_shared.SetPresentFeedback(IsPresentTimingEnabled(), GetPresentRefreshDuration());
    m_shared.SetPresentSchedulingSupport(IsPresentSchedulingSupported());
    m_shared.SetSwapchainRefresh(ToSampleSwapchainRefresh(GetPresentRefreshMode()));
    for (const Vulkan::VUPresentTimingRecord& record : GetPresentTimings())
    {
      m_shared.AddPresentTiming(record.PresentId, record.GetDisplayTime(), record.QueueOperationsEnd, record.IsComplete);
    }
  }


  void FramePacing::OnVulkanFrameStart()
  {
    {    // A pacer that gives plans is told about the present of the frame before, before it is asked about this frame
      const VulkanBasic::PresentCallRecord& presentCalls = GetLastPresentCalls();
      if (presentCalls.PresentId != 0u)
      {
        m_shared.AddPresentCall(presentCalls.PresentId, presentCalls.PresentCallTime, presentCalls.PresentReturnTime,
                                presentCalls.IsPresentAccepted());
      }
    }
    // The frame pacer can name a earlier present to wait for until it was shown: that wait comes first
    const SamplePresentWaitRequest waitRequest = m_shared.GetPresentWaitRequest();
    if (waitRequest.PresentId != 0u)
    {
      const VulkanBasic::PresentWaitRecord waitRecord = WaitForPresent(waitRequest.PresentId, waitRequest.Timeout);
      m_shared.AddPresentWait(waitRequest.PresentId, waitRecord.BeginTime, waitRecord.EndTime, waitRecord.IsPresented());
    }
    // Or it names a earlier frame to wait for until the GPU is done with it, which holds the loop where no present is waited for
    const SampleGpuWaitRequest gpuWaitRequest = m_shared.GetGpuWaitRequest();
    if (gpuWaitRequest.PresentId != 0u)
    {
      const VulkanBasic::GpuWorkWaitRecord gpuWaitRecord = WaitForGpuWork(gpuWaitRequest.PresentId, gpuWaitRequest.Timeout);
      m_shared.AddGpuWait(gpuWaitRequest.PresentId, gpuWaitRecord.BeginTime, gpuWaitRecord.EndTime, gpuWaitRecord.IsDone());
    }
    // The frame pacer holds the start of a frame here (the late profile, or a pacer that gives the time), before a swapchain image is
    // acquired for it
    m_shared.WaitForFrameStart();
  }


  VkRenderPass FramePacing::OnBuildResources(const VulkanBasic::BuildResourcesContext& context)
  {
    // Since we only draw using the NativeBatch and the background we just create the most basic render pass that is compatible
    m_dependentResources.MainRenderPass = CreateBasicRenderPass();
    m_background.OnBuildResources(context, m_dependentResources.MainRenderPass.Get());
    // A frame pacer that is given the work of the GPU needs to know how many frames are in the works at the same time. That is
    // what the app base runs with (one unless the user asks for more with --VkFramesInFlight), not the most the app is set up for.
    m_shared.SetMaxFramesInFlight(context.MaxFramesInFlight);
    // and how many frames the swapchain can hold
    m_shared.SetSwapchainImageCount(context.SwapchainImagesCount);
    // The swapchain is new: a frame pacer that waits for presents is told, as none of the swapchain before can be waited for
    m_shared.OnSwapchainRecreated();
    return m_dependentResources.MainRenderPass.Get();
  }


  void FramePacing::OnFreeResources()
  {
    m_background.OnFreeResources();
    m_dependentResources.Reset();
  }
}
