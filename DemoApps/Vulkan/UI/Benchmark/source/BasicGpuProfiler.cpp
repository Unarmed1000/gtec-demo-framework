/****************************************************************************************************************************************************
 * Copyright 2021-2022 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the NXP. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include "BasicGpuProfiler.hpp"
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslUtil/Vulkan1_0/VUDevice.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <stdexcept>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr const uint32_t Query0 = 0;
      constexpr const uint32_t Query1 = 1;
      // The queries of one frame slot
      constexpr const uint32_t QueryCount = 2;
    }

    RapidVulkan::QueryPool CreateQueryPool(const VkDevice device, const uint32_t maxFramesInFlight)
    {
      // Create query pool: every frame in flight has its own queries, so a result can be read without waiting for the device
      VkQueryPoolCreateInfo queryPoolInfo{};
      queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
      queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
      queryPoolInfo.queryCount = maxFramesInFlight * LocalConfig::QueryCount;

      return {device, queryPoolInfo};
    }
  }


  BasicGpuProfiler::BasicGpuProfiler(const VkPhysicalDeviceProperties& physicalDeviceProperties, const VkDevice device,
                                     const uint32_t maxFramesInFlight)
    : m_device(device)
    , m_queryPool(CreateQueryPool(m_device, maxFramesInFlight))
    , m_timestampPeriod(physicalDeviceProperties.limits.timestampPeriod)
    , m_slots(maxFramesInFlight)
  {
    if (!IsTimestampSupported(physicalDeviceProperties))
    {
      throw UsageErrorException("the physical device does not support timestamps");
    }
    if (m_device == VK_NULL_HANDLE)
    {
      throw std::invalid_argument("device can not be VK_NULL_HANDLE");
    }
  }


  bool BasicGpuProfiler::IsEnabled() const noexcept
  {
    return m_resources.IsEnabled;
  }


  void BasicGpuProfiler::SetEnabled(const bool enabled)
  {
    if (m_resources.IsEnabled != enabled)
    {
      if (enabled)
      {
        m_resources = Resources(true);
      }
      else
      {
        m_resources = {};
      }
      // What the slots hold was measured with the old setting
      std::fill(m_slots.begin(), m_slots.end(), SlotRecord());
    }
  }


  void BasicGpuProfiler::BeginTimestamp()
  {
    if (m_resources.Draw.CommandBuffer != VK_NULL_HANDLE && m_resources.QueueTimestampState == TimestampState::NotSet)
    {
      assert(m_queryPool.IsValid());
      vkCmdWriteTimestamp(m_resources.Draw.CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPool.Get(),
                          m_resources.Draw.FirstQuery + LocalConfig::Query0);
      m_resources.QueueTimestampState = TimestampState::BeginSet;
    }
    else
    {
      FSLLOG3_DEBUG_WARNING_IF(m_resources.Draw.CommandBuffer, "BeginTimestamp: BeginFrame was not called, ignoring request");
      FSLLOG3_DEBUG_WARNING_IF(m_resources.QueueTimestampState != TimestampState::NotSet, "BeginTimestamp: incorrect state, ignoring request");
    }
  }

  void BasicGpuProfiler::EndTimestamp()
  {
    if (m_resources.Draw.CommandBuffer != VK_NULL_HANDLE && m_resources.QueueTimestampState == TimestampState::BeginSet)
    {
      assert(m_queryPool.IsValid());
      vkCmdWriteTimestamp(m_resources.Draw.CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool.Get(),
                          m_resources.Draw.FirstQuery + LocalConfig::Query1);
      m_resources.QueueTimestampState = TimestampState::BothSet;
      m_slots[m_resources.Draw.FrameIndex].HasPendingQuery = true;
    }
    else
    {
      FSLLOG3_DEBUG_WARNING_IF(m_resources.Draw.CommandBuffer, "BeginTimestamp: BeginFrame was not called, ignoring request");
      FSLLOG3_DEBUG_WARNING_IF(m_resources.QueueTimestampState != TimestampState::BeginSet, "BeginTimestamp: incorrect state, ignoring request");
    }
  }

  uint64_t BasicGpuProfiler::GetResult() const noexcept
  {
    return m_resources.LastResult;
  }


  void BasicGpuProfiler::BeginDraw(const VkCommandBuffer commandBuffer, const uint32_t frameIndex)
  {
    if (commandBuffer == VK_NULL_HANDLE)
    {
      throw std::invalid_argument("command buffer can not be null");
    }
    if (m_resources.IsEnabled)
    {
      assert(m_queryPool.IsValid());
      SlotRecord& rSlot = m_slots.at(frameIndex);
      m_resources.Draw = DrawResources(commandBuffer, frameIndex, frameIndex * LocalConfig::QueryCount);

      if (rSlot.HasPendingQuery)
      {
        // The GPU finished the last frame of this slot before the host let the slot record again, so its timestamps are there and the
        // read does not wait for the device. If they are not (the frame was dropped) no result is reported for this frame.
        std::array<uint64_t, LocalConfig::QueryCount> resultBuffer{};
        const VkResult result =
          vkGetQueryPoolResults(m_device, m_queryPool.Get(), m_resources.Draw.FirstQuery, LocalConfig::QueryCount,
                                sizeof(uint64_t) * resultBuffer.size(), resultBuffer.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        if (result == VK_SUCCESS)
        {
          // timestampPeriod is the number of nanoseconds required for a timestamp query to be incremented by 1.
          m_resources.LastResult = static_cast<uint64_t>(
            std::round((static_cast<double>(resultBuffer[LocalConfig::Query1] - resultBuffer[LocalConfig::Query0]) * m_timestampPeriod) / 1000.0));
        }
        rSlot.HasPendingQuery = false;
      }

      vkCmdResetQueryPool(m_resources.Draw.CommandBuffer, m_queryPool.Get(), m_resources.Draw.FirstQuery, LocalConfig::QueryCount);
    }
  }

  void BasicGpuProfiler::EndDraw()
  {
    m_resources.Draw = {};
  }


  void BasicGpuProfiler::BeginFrame()
  {
    // A frame only reports a result if one is read while it is drawn (BeginDraw)
    m_resources.LastResult = 0;
    if (m_resources.IsEnabled)
    {
      switch (m_resources.QueueTimestampState)
      {
      case TimestampState::NotSet:
      case TimestampState::BothSet:
        break;
      case TimestampState::BeginSet:
        FSLLOG3_WARNING("Begin was set, but end was not");
        break;
      default:
        FSLLOG3_WARNING("Unsupported state");
        break;
      }
      m_resources.QueueTimestampState = TimestampState::NotSet;
    }
  }


  bool BasicGpuProfiler::IsTimestampSupported(const VkPhysicalDeviceProperties& physicalDeviceProperties)
  {
    bool isSupported = true;
    if (physicalDeviceProperties.limits.timestampPeriod <= 0.0f)
    {
      isSupported = false;
    }
    return isSupported;
  }
}
