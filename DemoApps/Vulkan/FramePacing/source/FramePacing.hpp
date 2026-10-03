#ifndef VULKAN_FRAMEPACING_FRAMEPACING_HPP
#define VULKAN_FRAMEPACING_FRAMEPACING_HPP
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
#include <FslUtil/Vulkan1_0/VUGpuTimeCalibration.hpp>
#include <Shared/FramePacing/FramePacingShared.hpp>
#include <vector>
#include "RaymarchBackground.hpp"

namespace Fsl
{
  class FramePacing final : public VulkanBasic::DemoAppVulkanBasic
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
        // Reset in destruction order
        MainRenderPass.Reset();
      }
    };

    DependentResources m_dependentResources;

    //! All the actual sample code can be found in the shared class since its reused for all FramePacing samples.
    FramePacingShared m_shared;

    //! The raymarched background (the GPU load of the sample)
    RaymarchBackground m_background;
    //! The frame pacer is told how long the GPU needs for a frame
    Vulkan::VUGpuFrameTimer m_gpuTimer;
    //! Converts the timestamps of the GPU timer to the clock of the CPU (if VK_KHR_calibrated_timestamps is available)
    Vulkan::VUGpuTimeCalibration m_gpuTimeCalibration;
    //! The id of the present of the frame that was drawn last in each frame slot: the GPU time of a slot is read when the slot is used again
    std::vector<uint64_t> m_slotPresentIds;
    uint64_t m_gpuMeasurementId{0};
    uint32_t m_framesSinceCalibration{0};

  public:
    explicit FramePacing(const DemoAppConfig& config);

  protected:
    void OnKeyEvent(const KeyEvent& event) final;
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics) final;
    void Update(const DemoTime& demoTime) final;
    void EndDraw(const FrameInfo& frameInfo) final;
    void VulkanDraw(const DemoTime& demoTime, RapidVulkan::CommandBuffers& rCmdBuffers, const VulkanBasic::DrawContext& drawContext) final;

    VkRenderPass OnBuildResources(const VulkanBasic::BuildResourcesContext& context) final;
    void OnFreeResources() final;

  private:
    //! Give the sample the optional measurements that arrived: the GPU work of the last frame of the frame slot and the measured presents
    void UpdateMeasurements(const uint32_t currentFrameIndex);
  };
}

#endif
