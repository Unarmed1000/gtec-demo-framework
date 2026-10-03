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
#include <FslBase/UncheckedNumericCast.hpp>
#include <array>

namespace Fsl
{
  namespace
  {
    bool IsSrgbFormat(const VkFormat format) noexcept
    {
      return format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
    }
  }


  PixelArt::PixelArt(const DemoAppConfig& config)
    : VulkanBasic::DemoAppVulkanBasic(config)
    , m_srgbFramebuffer(IsSrgbFormat(GetSurfaceFormatInfo().Format))
    , m_renderer(m_device, m_deviceQueue, GetContentManager(), GetRenderConfig().MaxFramesInFlight, m_srgbFramebuffer)
    , m_shared(config, m_renderer, m_srgbFramebuffer)
    , m_gpuTimer(m_device, m_deviceQueue.QueueFamilyIndex, GetRenderConfig().MaxFramesInFlight)
  {
    m_shared.SetGpuTimerSupported(m_gpuTimer.IsSupported());
    FSLLOG3_INFO("sRGB framebuffer: {}", m_srgbFramebuffer ? "yes" : "no, the shader applies the gamma");
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


  void PixelArt::VulkanDraw(const DemoTime& /*demoTime*/, RapidVulkan::CommandBuffers& rCmdBuffers, const VulkanBasic::DrawContext& drawContext)
  {
    const uint32_t currentFrameIndex = drawContext.CurrentFrameIndex;
    const PixelArtFrameState& frameState = m_shared.GetFrameState();
    m_renderer.PrepareFrame(currentFrameIndex, frameState);

    const VkCommandBuffer hCmdBuffer = rCmdBuffers[currentFrameIndex];
    rCmdBuffers.Begin(currentFrameIndex, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, VK_FALSE, 0, 0);
    {
      m_gpuTimer.BeginFrame(hCmdBuffer, currentFrameIndex);
      if (m_gpuTimer.GetMeasurementId() != m_gpuMeasurementId)
      {
        m_gpuMeasurementId = m_gpuTimer.GetMeasurementId();
        m_shared.AddGpuTime(m_gpuTimer.GetGpuTime());
      }

      // The buffers are drawn before the main render pass
      m_renderer.RecordBufferPasses(hCmdBuffer, currentFrameIndex, frameState);

      std::array<VkClearValue, 1> clearValues{};
      clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

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
        m_renderer.RecordImagePass(hCmdBuffer, currentFrameIndex, frameState, drawContext.SwapchainImageExtent);
        m_shared.Draw();

        // Remember to call this as the last operation in your renderPass
        AddSystemUI(hCmdBuffer, currentFrameIndex);
      }
      rCmdBuffers.CmdEndRenderPass(currentFrameIndex);
      m_gpuTimer.EndFrame(hCmdBuffer, currentFrameIndex);
    }
    rCmdBuffers.End(currentFrameIndex);
  }


  VkRenderPass PixelArt::OnBuildResources(const VulkanBasic::BuildResourcesContext& /*context*/)
  {
    m_dependentResources.MainRenderPass = CreateBasicRenderPass();
    m_renderer.OnBuildResources(m_dependentResources.MainRenderPass.Get());
    return m_dependentResources.MainRenderPass.Get();
  }


  void PixelArt::OnFreeResources()
  {
    m_renderer.OnFreeResources();
    m_dependentResources.Reset();
  }
}
