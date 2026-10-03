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
  FramePacing::FramePacing(const DemoAppConfig& config)
    : VulkanBasic::DemoAppVulkanBasic(config)
    , m_shared(config, "Vulkan.FramePacing", SamplePresentMethod::WaitThenPresent)
    , m_background(m_device, *GetContentManager())
    , m_gpuTimer(m_device, m_deviceQueue.QueueFamilyIndex, GetRenderConfig().MaxFramesInFlight)
  {
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
        m_shared.Draw();

        // Remember to call this as the last operation in your renderPass (this is also where the frame pacing marker is drawn)
        AddSystemUI(hCmdBuffer, currentFrameIndex);
      }
      rCmdBuffers.CmdEndRenderPass(currentFrameIndex);

      m_gpuTimer.EndFrame(hCmdBuffer, currentFrameIndex);
    }
    rCmdBuffers.End(currentFrameIndex);
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
