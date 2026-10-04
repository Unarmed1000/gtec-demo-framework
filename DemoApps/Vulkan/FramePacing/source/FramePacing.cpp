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
#include <FslBase/UncheckedNumericCast.hpp>
#include <array>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr uint32_t FramesPerCalibration = 240;
    }

    VulkanBasic::DemoAppVulkanSetup CreateSetup()
    {
      VulkanBasic::DemoAppVulkanSetup setup;
      // Measure when the frames reach the display (if VK_EXT_present_timing is available)
      setup.PresentTiming = true;
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


  void FramePacing::ConfigurationChanged(const DemoWindowMetrics& windowMetrics)
  {
    base_type::ConfigurationChanged(windowMetrics);
    m_shared.ConfigurationChanged(windowMetrics);
  }


  void FramePacing::Update(const DemoTime& demoTime)
  {
    m_shared.Update(demoTime);
    // Follow the switch of the UI (a change recreates the swapchain before the next frame)
    SetPresentTimingRequested(m_shared.IsPresentTimingWanted());
  }


  void FramePacing::EndDraw(const FrameInfo& frameInfo)
  {
    base_type::EndDraw(frameInfo);

    // The frame was submitted to the GPU and is presented after this. The GPU works on it from now on, so the frame pacer is given the
    // GPU time of the last frame that was measured.
    m_shared.EndFrame(m_gpuTimer.GetGpuTime());
    // A FIFO present holds a frame for one refresh and there is no swap interval, so the present of a frame the frame pacer holds for
    // more than one refresh is delayed instead
    m_shared.WaitForPresent();
  }


  void FramePacing::VulkanDraw(const DemoTime& /*demoTime*/, RapidVulkan::CommandBuffers& rCmdBuffers, const VulkanBasic::DrawContext& drawContext)
  {
    const uint32_t currentFrameIndex = drawContext.CurrentFrameIndex;

    const VkCommandBuffer hCmdBuffer = rCmdBuffers[currentFrameIndex];
    rCmdBuffers.Begin(currentFrameIndex, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, VK_FALSE, 0, 0);
    {
      m_gpuTimer.BeginFrame(hCmdBuffer, currentFrameIndex);
      UpdateMeasurements(currentFrameIndex);

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

      rCmdBuffers.CmdBeginRenderPass(currentFrameIndex, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
      {
        // The background is animated for the same time as the rest of the frame
        m_background.Draw(hCmdBuffer, m_shared.GetRaymarchParams());
        {    // The frame has started, so it can be given the id its present will get. The measurements of the frame refer to it.
          const uint64_t presentId = GetNextPresentId();
          m_shared.SetFramePresentId(presentId);
          m_slotPresentIds[currentFrameIndex] = presentId;
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
      TickCount gpuStartTime;
      TickCount gpuEndTime;
      if (m_shared.IsGpuTimelineWanted() && m_gpuTimeCalibration.TryToHostTime(m_gpuTimer.GetBeginTimestamp(), gpuStartTime) &&
          m_gpuTimeCalibration.TryToHostTime(m_gpuTimer.GetEndTimestamp(), gpuEndTime))
      {
        m_shared.AddGpuInterval(m_slotPresentIds[currentFrameIndex], gpuStartTime, gpuEndTime);
      }
    }
    // Read the device clock and the clock of the framework again now and then, so they do not drift apart
    if (++m_framesSinceCalibration >= LocalConfig::FramesPerCalibration)
    {
      m_framesSinceCalibration = 0;
      m_gpuTimeCalibration.Calibrate();
    }

    // When the frames reached the display, if the swapchain measures it (VK_EXT_present_timing). The measurements arrive a few frames
    // after the present.
    m_shared.SetPresentFeedback(IsPresentTimingEnabled(), GetPresentRefreshDuration());
    {    // When the frame before this one was presented: the frame pacer is told with the display time of that frame
      const VulkanBasic::PresentCallRecord& presentCalls = GetLastPresentCalls();
      if (presentCalls.PresentId != 0u)
      {
        m_shared.SetPresentCallTime(presentCalls.PresentId, presentCalls.PresentCallTime);
      }
    }
    for (const Vulkan::VUPresentTimingRecord& record : GetPresentTimings())
    {
      m_shared.AddPresentTiming(record.PresentId, record.GetDisplayTime(), record.QueueOperationsEnd);
    }
  }


  VkRenderPass FramePacing::OnBuildResources(const VulkanBasic::BuildResourcesContext& context)
  {
    // Since we only draw using the NativeBatch and the background we just create the most basic render pass that is compatible
    m_dependentResources.MainRenderPass = CreateBasicRenderPass();
    m_background.OnBuildResources(context, m_dependentResources.MainRenderPass.Get());
    return m_dependentResources.MainRenderPass.Get();
  }


  void FramePacing::OnFreeResources()
  {
    m_background.OnFreeResources();
    m_dependentResources.Reset();
  }
}
