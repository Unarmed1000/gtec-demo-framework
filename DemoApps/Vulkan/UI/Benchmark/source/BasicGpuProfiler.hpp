#ifndef VULKAN_UI_BENCHMARK_BASICGPUPROFILER_HPP
#define VULKAN_UI_BENCHMARK_BASICGPUPROFILER_HPP
/****************************************************************************************************************************************************
 * Copyright 2021 NXP
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

#include <RapidVulkan/QueryPool.hpp>
#include <Shared/UI/Benchmark/IBasicGpuProfiler.hpp>
#include <vector>

namespace Fsl
{
  //! @note Every frame in flight has its own two timestamp queries. A frame reads the result of the last frame that used its slot, which
  //!       the GPU has finished when the host lets the slot record again, so reading a result never waits for the device.
  class BasicGpuProfiler final : public IBasicGpuProfiler
  {
    enum class TimestampState
    {
      NotSet,
      BeginSet,
      BothSet
    };

    struct SlotRecord
    {
      bool HasPendingQuery{false};
    };

    struct DrawResources
    {
      VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};
      //! The frame slot that is being recorded and the first of its queries
      uint32_t FrameIndex{0};
      uint32_t FirstQuery{0};

      DrawResources() = default;

      DrawResources(const VkCommandBuffer commandBuffer, const uint32_t frameIndex, const uint32_t firstQuery)
        : CommandBuffer(commandBuffer)
        , FrameIndex(frameIndex)
        , FirstQuery(firstQuery)
      {
      }
    };


    struct Resources
    {
      bool IsEnabled{false};
      TimestampState QueueTimestampState{TimestampState::NotSet};
      uint64_t LastResult{0};
      DrawResources Draw;

      Resources() = default;
      explicit Resources(const bool enabled)
        : IsEnabled(enabled)
      {
      }
    };

    VkDevice m_device{VK_NULL_HANDLE};
    RapidVulkan::QueryPool m_queryPool;
    float m_timestampPeriod{0};
    //! One record per frame in flight
    std::vector<SlotRecord> m_slots;
    Resources m_resources;

  public:
    //! @param maxFramesInFlight the frames the host can have in flight (the frame index given to BeginDraw is below it)
    BasicGpuProfiler(const VkPhysicalDeviceProperties& physicalDeviceProperties, const VkDevice device, const uint32_t maxFramesInFlight);

    [[nodiscard]] bool IsEnabled() const noexcept final;
    void SetEnabled(const bool enabled) final;
    void BeginTimestamp() final;
    void EndTimestamp() final;
    [[nodiscard]] uint64_t GetResult() const noexcept final;

    //! @brief Call it at the start of a frame, before anything is drawn. The frame has no result until BeginDraw reads one.
    void BeginFrame();
    //! @brief Call it first in the command buffer of a frame, outside a render pass. It reads the result of the last frame that used the
    //!        slot of the frame index.
    void BeginDraw(const VkCommandBuffer commandBuffer, const uint32_t frameIndex);
    void EndDraw();

    static bool IsTimestampSupported(const VkPhysicalDeviceProperties& physicalDeviceProperties);

  private:
  };
}

#endif
