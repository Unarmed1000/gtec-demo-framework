#ifndef FSLUTIL_VULKAN1_0_VUGPUFRAMETIMER_HPP
#define FSLUTIL_VULKAN1_0_VUGPUFRAMETIMER_HPP
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

// Make sure Common.hpp is the first include file (to make the error message as helpful as possible when disabled)
#include <FslBase/Time/TimeSpan.hpp>
#include <FslUtil/Vulkan1_0/Common.hpp>
#include <FslUtil/Vulkan1_0/VUDevice.hpp>
#include <FslUtil/Vulkan1_0/VUDeviceTimestamp.hpp>
#include <RapidVulkan/QueryPool.hpp>
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace Fsl::Vulkan
{
  //! @brief Measures the time the GPU needs for a frame with two timestamp queries around the commands of the frame.
  //! @note  Every frame in flight has its own two queries. A frame reads the result of the last frame that used its slot, which the GPU has
  //!        finished when the host lets the slot record again (the host waited for its fence), so reading never waits. A queue family
  //!        without timestamps is not supported, then the GPU time stays zero.
  class VUGpuFrameTimer final
  {
    struct SlotRecord
    {
      bool HasPendingQuery{false};
    };

    VkDevice m_device{VK_NULL_HANDLE};
    RapidVulkan::QueryPool m_queryPool;
    std::vector<SlotRecord> m_slots;
    //! The number of nanoseconds a timestamp counts in
    double m_timestampPeriod{0.0};
    //! The bits of a timestamp that are valid
    uint64_t m_timestampMask{0};
    TimeSpan m_gpuTime;
    //! The timestamps of the last frame that was measured
    VUDeviceTimestamp m_beginTimestamp;
    VUDeviceTimestamp m_endTimestamp;
    uint64_t m_measurementId{0};

  public:
    VUGpuFrameTimer(const VUGpuFrameTimer&) = delete;
    VUGpuFrameTimer& operator=(const VUGpuFrameTimer&) = delete;

    //! @param queueFamilyIndex the queue family the command buffers are submitted to
    //! @param maxFramesInFlight the frames the host can have in flight (the frame index given to BeginFrame and EndFrame is below it)
    VUGpuFrameTimer(const VUDevice& device, const uint32_t queueFamilyIndex, const uint32_t maxFramesInFlight);
    ~VUGpuFrameTimer() = default;

    //! @return true if the queue supports timestamps (if not the GPU time stays zero)
    [[nodiscard]] bool IsSupported() const noexcept
    {
      return m_queryPool.IsValid();
    }

    //! @brief Call it first in the command buffer of a frame, outside a render pass. It reads the GPU time of the last frame that used the
    //!        slot of the frame index.
    void BeginFrame(const VkCommandBuffer hCmdBuffer, const uint32_t frameIndex);

    //! @brief Call it last in the command buffer of a frame, outside a render pass.
    void EndFrame(const VkCommandBuffer hCmdBuffer, const uint32_t frameIndex);

    //! @return the GPU time of the last frame that was measured (zero if no frame was measured yet)
    [[nodiscard]] TimeSpan GetGpuTime() const noexcept
    {
      return m_gpuTime;
    }

    //! @return when the GPU began and ended the last frame that was measured, on the clock of the device. VUGpuTimeCalibration converts them
    //!         to the clock of the framework.
    [[nodiscard]] VUDeviceTimestamp GetBeginTimestamp() const noexcept
    {
      return m_beginTimestamp;
    }

    [[nodiscard]] VUDeviceTimestamp GetEndTimestamp() const noexcept
    {
      return m_endTimestamp;
    }

    //! @return the number of nanoseconds a device timestamp counts in (zero if not supported)
    [[nodiscard]] double GetTimestampPeriod() const noexcept
    {
      return m_timestampPeriod;
    }

    //! @return the bits of a device timestamp that are valid (zero if not supported)
    [[nodiscard]] uint64_t GetTimestampMask() const noexcept
    {
      return m_timestampMask;
    }

    //! @return a number that grows by one for every measured frame, so a caller can tell if GetGpuTime has a new value
    [[nodiscard]] uint64_t GetMeasurementId() const noexcept
    {
      return m_measurementId;
    }
  };
}

#endif
