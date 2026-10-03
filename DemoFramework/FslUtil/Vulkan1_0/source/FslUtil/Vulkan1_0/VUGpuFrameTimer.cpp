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

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslUtil/Vulkan1_0/VUGpuFrameTimer.hpp>
#include <array>
#include <cmath>
#include <stdexcept>

namespace Fsl::Vulkan
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr uint32_t QueriesPerSlot = 2;
      constexpr uint32_t QueryBegin = 0;
      constexpr uint32_t QueryEnd = 1;
      constexpr double NanosecondsPerTick = 100.0;
    }

    //! The number of valid bits in a timestamp of the queue family (0 = the queue family does not support timestamps)
    uint32_t GetTimestampValidBits(const VkPhysicalDevice physicalDevice, const uint32_t queueFamilyIndex)
    {
      uint32_t count = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
      std::vector<VkQueueFamilyProperties> properties(count);
      vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, properties.data());
      return queueFamilyIndex < count ? properties[queueFamilyIndex].timestampValidBits : 0u;
    }
  }


  VUGpuFrameTimer::VUGpuFrameTimer(const VUDevice& device, const uint32_t queueFamilyIndex, const uint32_t maxFramesInFlight)
    : m_device(device.Get())
    , m_slots(maxFramesInFlight)
  {
    if (maxFramesInFlight == 0)
    {
      throw std::invalid_argument("maxFramesInFlight must be at least one");
    }
    const auto& physicalDevice = device.GetPhysicalDevice();
    const uint32_t validBits = GetTimestampValidBits(physicalDevice.Device, queueFamilyIndex);
    m_timestampPeriod = static_cast<double>(physicalDevice.Properties.limits.timestampPeriod);
    if (validBits == 0u || m_timestampPeriod <= 0.0)
    {
      FSLLOG3_INFO("GPU timestamps are not supported by the queue family, the GPU time of a frame is not measured");
      return;
    }
    m_timestampMask = validBits >= 64u ? ~uint64_t{0} : ((uint64_t{1} << validBits) - 1u);

    VkQueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolInfo.queryCount = maxFramesInFlight * LocalConfig::QueriesPerSlot;
    m_queryPool.Reset(m_device, queryPoolInfo);
  }


  void VUGpuFrameTimer::BeginFrame(const VkCommandBuffer hCmdBuffer, const uint32_t frameIndex)
  {
    if (!m_queryPool.IsValid())
    {
      return;
    }
    SlotRecord& rSlot = m_slots.at(frameIndex);
    const uint32_t firstQuery = frameIndex * LocalConfig::QueriesPerSlot;

    if (rSlot.HasPendingQuery)
    {
      // The GPU finished the last frame of this slot before the host let the slot record again, so its timestamps are there. If they are
      // not (the frame was dropped) the old time is kept.
      std::array<uint64_t, LocalConfig::QueriesPerSlot> timestamps{};
      const VkResult result =
        vkGetQueryPoolResults(m_device, m_queryPool.Get(), firstQuery, LocalConfig::QueriesPerSlot, sizeof(uint64_t) * timestamps.size(),
                              timestamps.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
      if (result == VK_SUCCESS)
      {
        const uint64_t elapsed = (timestamps[LocalConfig::QueryEnd] - timestamps[LocalConfig::QueryBegin]) & m_timestampMask;
        m_gpuTime = TimeSpan(std::llround((static_cast<double>(elapsed) * m_timestampPeriod) / LocalConfig::NanosecondsPerTick));
        m_beginTimestamp = VUDeviceTimestamp(timestamps[LocalConfig::QueryBegin] & m_timestampMask);
        m_endTimestamp = VUDeviceTimestamp(timestamps[LocalConfig::QueryEnd] & m_timestampMask);
        ++m_measurementId;
      }
      rSlot.HasPendingQuery = false;
    }

    vkCmdResetQueryPool(hCmdBuffer, m_queryPool.Get(), firstQuery, LocalConfig::QueriesPerSlot);
    vkCmdWriteTimestamp(hCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPool.Get(), firstQuery + LocalConfig::QueryBegin);
  }


  void VUGpuFrameTimer::EndFrame(const VkCommandBuffer hCmdBuffer, const uint32_t frameIndex)
  {
    if (!m_queryPool.IsValid())
    {
      return;
    }
    const uint32_t firstQuery = frameIndex * LocalConfig::QueriesPerSlot;
    vkCmdWriteTimestamp(hCmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool.Get(), firstQuery + LocalConfig::QueryEnd);
    m_slots.at(frameIndex).HasPendingQuery = true;
  }
}
