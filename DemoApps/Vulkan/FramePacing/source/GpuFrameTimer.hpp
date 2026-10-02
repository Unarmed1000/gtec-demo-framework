#ifndef VULKAN_FRAMEPACING_GPUFRAMETIMER_HPP
#define VULKAN_FRAMEPACING_GPUFRAMETIMER_HPP
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

#include <FslBase/Time/TimeSpan.hpp>
#include <FslUtil/Vulkan1_0/VUDevice.hpp>
#include <RapidVulkan/QueryPool.hpp>
#include <vulkan/vulkan.h>
#include <cstdint>

namespace Fsl
{
  //! Measures the time the GPU needs for a frame with two timestamp queries around the commands of the frame.
  //! The result of a frame is read when the next frame is recorded, which is after the host waited for the GPU to finish the frame (the
  //! host keeps one frame in flight), so reading it never waits.
  class GpuFrameTimer final
  {
    VkDevice m_device{VK_NULL_HANDLE};
    RapidVulkan::QueryPool m_queryPool;
    //! The number of nanoseconds a timestamp counts in
    double m_timestampPeriod{0.0};
    //! The bits of a timestamp that are valid
    uint64_t m_timestampMask{0};
    bool m_hasPendingQuery{false};
    TimeSpan m_gpuTime;

  public:
    GpuFrameTimer(const GpuFrameTimer&) = delete;
    GpuFrameTimer& operator=(const GpuFrameTimer&) = delete;

    //! @param queueFamilyIndex the queue family the command buffers are submitted to
    GpuFrameTimer(const Vulkan::VUDevice& device, const uint32_t queueFamilyIndex);

    //! @return true if the queue supports timestamps (if not the GPU time stays zero)
    [[nodiscard]] bool IsSupported() const noexcept
    {
      return m_queryPool.IsValid();
    }

    //! @brief Call it first in the command buffer of a frame, outside a render pass.
    void BeginFrame(const VkCommandBuffer hCmdBuffer);

    //! @brief Call it last in the command buffer of a frame, outside a render pass.
    void EndFrame(const VkCommandBuffer hCmdBuffer);

    //! @return the GPU time of the last frame that was measured (zero if no frame was measured yet)
    [[nodiscard]] TimeSpan GetGpuTime() const noexcept
    {
      return m_gpuTime;
    }
  };
}

#endif
