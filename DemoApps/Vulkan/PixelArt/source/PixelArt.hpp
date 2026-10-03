#ifndef VULKAN_PIXELART_PIXELART_HPP
#define VULKAN_PIXELART_PIXELART_HPP
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

#include <FslDemoApp/Vulkan/Basic/DemoAppVulkanBasic.hpp>
#include <FslUtil/Vulkan1_0/VUGpuFrameTimer.hpp>
#include <Shared/PixelArt/API/Vulkan/PixelArtRendererVulkan.hpp>
#include <Shared/PixelArt/Base/PixelArtShared.hpp>

namespace Fsl
{
  //! Plays the shadertoy style scenes of PixelArt/Shaders with Vulkan
  class PixelArt final : public VulkanBasic::DemoAppVulkanBasic
  {
    using base_type = VulkanBasic::DemoAppVulkanBasic;

    struct DependentResources
    {
      RapidVulkan::RenderPass MainRenderPass;

      DependentResources() = default;
      DependentResources(const DependentResources&) = delete;
      DependentResources& operator=(const DependentResources&) = delete;
      DependentResources(DependentResources&& other) noexcept = delete;
      DependentResources& operator=(DependentResources&& other) noexcept = delete;
      ~DependentResources() = default;

      void Reset() noexcept
      {
        MainRenderPass.Reset();
      }
    };

    bool m_srgbFramebuffer;
    PixelArtRendererVulkan m_renderer;
    //! The scene switcher, the UI and everything else that is the same for the OpenGL ES and the Vulkan app
    PixelArtShared m_shared;
    //! The GPU time of a frame, for the chart
    Vulkan::VUGpuFrameTimer m_gpuTimer;
    uint64_t m_gpuMeasurementId{0};
    DependentResources m_dependentResources;

  public:
    explicit PixelArt(const DemoAppConfig& config);

  protected:
    void OnKeyEvent(const KeyEvent& event) final;
    void OnMouseButtonEvent(const MouseButtonEvent& event) final;
    void OnMouseMoveEvent(const MouseMoveEvent& event) final;
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics) final;
    void Update(const DemoTime& demoTime) final;
    void VulkanDraw(const DemoTime& demoTime, RapidVulkan::CommandBuffers& rCmdBuffers, const VulkanBasic::DrawContext& drawContext) final;

    VkRenderPass OnBuildResources(const VulkanBasic::BuildResourcesContext& context) final;
    void OnFreeResources() final;
  };
}

#endif
