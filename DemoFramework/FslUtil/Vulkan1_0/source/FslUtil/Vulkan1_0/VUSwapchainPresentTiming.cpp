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
#include <FslUtil/Vulkan1_0/VUSwapchainPresentTiming.hpp>
#include <RapidVulkan/Debug/Strings/VkResult.hpp>
#include <algorithm>
#include <array>
#include <stdexcept>

namespace Fsl::Vulkan
{
#ifdef FSL_VULKAN_PRESENT_TIMING_SUPPORTED

  namespace
  {
    namespace LocalConfig
    {
      //! The number of presents the swapchain can hold measurements for
      constexpr uint32_t QueueSize = 32;
      //! Timing is not requested for a present when this many are waiting to be collected, as a full queue makes the present fail
      constexpr uint32_t MaxOutstanding = QueueSize - 4;
      constexpr uint32_t MaxResultsPerCall = 8;
      constexpr uint32_t MaxCallsPerPoll = 4;
      //! One for each VkPresentStageFlagBitsEXT
      constexpr uint32_t MaxStages = 4;
      //! How often the clocks of the swapchain are related to the clock of the framework again, which bounds how far they can drift apart
      constexpr uint32_t PresentsPerCalibration = 256;
    }

    //! @return the index of the stage (the position of its bit), MaxStages if it is not a single known stage.
    constexpr uint32_t ToStageIndex(const VkPresentStageFlagsEXT stage) noexcept
    {
      for (uint32_t i = 0; i < LocalConfig::MaxStages; ++i)
      {
        if (stage == (VkPresentStageFlagsEXT{1} << i))
        {
          return i;
        }
      }
      return LocalConfig::MaxStages;
    }

    template <typename TFunction>
    TFunction GetDeviceFunction(const VkDevice device, const char* const pszName) noexcept
    {
      // The loader does not export the entry points of the extension, so they are looked up
      return reinterpret_cast<TFunction>(vkGetDeviceProcAddr(device, pszName));
    }

    struct SurfaceSupport
    {
      bool IsSupported{false};
      VkPresentStageFlagsEXT StageQueries{0};
      bool PresentAtAbsoluteTime{false};
      bool PresentAtRelativeTime{false};
    };

    SurfaceSupport GetSurfaceSupport(const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface)
    {
      if (physicalDevice == VK_NULL_HANDLE || surface == VK_NULL_HANDLE)
      {
        return {};
      }

      VkPresentTimingSurfaceCapabilitiesEXT timingCapabilities{};
      timingCapabilities.sType = VK_STRUCTURE_TYPE_PRESENT_TIMING_SURFACE_CAPABILITIES_EXT;

      VkSurfaceCapabilitiesPresentId2KHR presentId2Capabilities{};
      presentId2Capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_ID_2_KHR;
      presentId2Capabilities.pNext = &timingCapabilities;

      VkSurfaceCapabilities2KHR surfaceCapabilities{};
      surfaceCapabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
      surfaceCapabilities.pNext = &presentId2Capabilities;

      VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo{};
      surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
      surfaceInfo.surface = surface;

      if (vkGetPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &surfaceInfo, &surfaceCapabilities) != VK_SUCCESS)
      {
        return {};
      }

      FSLLOG3_VERBOSE2("Present timing: surface capabilities presentTiming {} presentAtAbsoluteTime {} presentAtRelativeTime {} stages {:#x}",
                       timingCapabilities.presentTimingSupported == VK_TRUE, timingCapabilities.presentAtAbsoluteTimeSupported == VK_TRUE,
                       timingCapabilities.presentAtRelativeTimeSupported == VK_TRUE, timingCapabilities.presentStageQueries);

      SurfaceSupport support;
      support.StageQueries = timingCapabilities.presentStageQueries;
      support.PresentAtAbsoluteTime = timingCapabilities.presentAtAbsoluteTimeSupported == VK_TRUE;
      support.PresentAtRelativeTime = timingCapabilities.presentAtRelativeTimeSupported == VK_TRUE;
      support.IsSupported = timingCapabilities.presentTimingSupported == VK_TRUE && presentId2Capabilities.presentId2Supported == VK_TRUE &&
                            timingCapabilities.presentStageQueries != 0u;
      return support;
    }

  }


  VkSwapchainCreateFlagsKHR VUSwapchainPresentTiming::GetSwapchainCreateFlags(const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface)
  {
    return GetSurfaceSupport(physicalDevice, surface).IsSupported
             ? (VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT | VK_SWAPCHAIN_CREATE_PRESENT_ID_2_BIT_KHR)
             : 0u;
  }


  void VUSwapchainPresentTiming::Reset() noexcept
  {
    m_swapchain = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_refreshDuration = {};
    m_outstanding = 0;
    m_queueSize = 0;
    m_canPresentAtRelativeTime = false;
    m_nextRelativeTargetTime = 0;
    m_canPresentAtAbsoluteTime = false;
    m_nextAbsoluteTargetTime = {};
    m_targetStage = 0;
    m_hasTimeDomain = false;
    m_hasStageOffset = {};
    m_presentsSinceCalibration = 0;
    m_state = {};
  }


  VUPresentTimingState VUSwapchainPresentTiming::GetState() const noexcept
  {
    VUPresentTimingState state = m_state;
    if (IsEnabled())
    {
      state.TimeDomain = static_cast<int32_t>(m_timeDomain);
      state.TimeDomainId = m_timeDomainId;
      state.StageQueries = m_stageQueries;
      state.StageOffsetTicks = m_stageOffsetTicks;
      state.HasStageOffset = m_hasStageOffset;
    }
    return state;
  }


  bool VUSwapchainPresentTiming::Reset(const VkPhysicalDevice physicalDevice, const VkDevice device, const VkSurfaceKHR surface,
                                       const VkSwapchainKHR swapchain, const VUCalibratedTimestamps& calibratedTimestamps)
  {
    Reset();
    if (device == VK_NULL_HANDLE || swapchain == VK_NULL_HANDLE)
    {
      throw std::invalid_argument("device and swapchain can not be VK_NULL_HANDLE");
    }

    const SurfaceSupport support = GetSurfaceSupport(physicalDevice, surface);
    if (!support.IsSupported)
    {
      return false;
    }

    m_pfnSetQueueSize = GetDeviceFunction<PFN_vkSetSwapchainPresentTimingQueueSizeEXT>(device, "vkSetSwapchainPresentTimingQueueSizeEXT");
    m_pfnGetTimingProperties = GetDeviceFunction<PFN_vkGetSwapchainTimingPropertiesEXT>(device, "vkGetSwapchainTimingPropertiesEXT");
    m_pfnGetTimeDomainProperties = GetDeviceFunction<PFN_vkGetSwapchainTimeDomainPropertiesEXT>(device, "vkGetSwapchainTimeDomainPropertiesEXT");
    m_pfnGetPastPresentationTiming = GetDeviceFunction<PFN_vkGetPastPresentationTimingEXT>(device, "vkGetPastPresentationTimingEXT");
    if (m_pfnSetQueueSize == nullptr || m_pfnGetTimingProperties == nullptr || m_pfnGetTimeDomainProperties == nullptr ||
        m_pfnGetPastPresentationTiming == nullptr)
    {
      FSLLOG3_WARNING("The present timing entry points were not found, was the device created with VK_EXT_present_timing enabled?");
      return false;
    }

    const VkResult queueResult = m_pfnSetQueueSize(device, swapchain, LocalConfig::QueueSize);
    if (queueResult != VK_SUCCESS)
    {
      FSLLOG3_WARNING("Present timing is disabled as vkSetSwapchainPresentTimingQueueSizeEXT failed with: {}",
                      RapidVulkan::Debug::ToString(queueResult));
      return false;
    }

    m_device = device;
    m_swapchain = swapchain;
    m_calibratedTimestamps = calibratedTimestamps;
    m_stageQueries = support.StageQueries;
    m_queueSize = LocalConfig::QueueSize;
    m_state.PresentAtAbsoluteTime = support.PresentAtAbsoluteTime;
    m_state.PresentAtRelativeTime = support.PresentAtRelativeTime;

    UpdateTimeDomain();
    if (!m_hasTimeDomain)
    {
      FSLLOG3_WARNING("Present timing is disabled as the swapchain has no usable time domain");
      Reset();
      return false;
    }
    UpdateTimingProperties();
    UpdateCalibration();
    FSLLOG3_VERBOSE("Present timing: enabled (stages: {:#x}, time domain: {}, refresh duration: {} ns)", m_stageQueries,
                    static_cast<int32_t>(m_timeDomain), m_refreshDuration.Ticks() * TickCount::NanoSecondsPerTick);
    return true;
  }


  bool VUSwapchainPresentTiming::TryEnablePresentAtRelativeTime() noexcept
  {
    m_canPresentAtRelativeTime = IsEnabled() && m_state.PresentAtRelativeTime && TrySelectTargetStage();
    return m_canPresentAtRelativeTime;
  }


  bool VUSwapchainPresentTiming::TryEnablePresentAtAbsoluteTime() noexcept
  {
    m_canPresentAtAbsoluteTime = IsEnabled() && m_state.PresentAtAbsoluteTime && TrySelectTargetStage();
    return m_canPresentAtAbsoluteTime;
  }


  bool VUSwapchainPresentTiming::TrySelectTargetStage() noexcept
  {
    if (m_targetStage == 0u)
    {
      // A target time is a time for a stage. The stage the image becomes visible in is the one the presentation engine aligns, the one
      // the first pixel leaves in is the next best where the surface does not report that.
      if ((m_stageQueries & VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT) != 0u)
      {
        m_targetStage = VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT;
      }
      else if ((m_stageQueries & VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT) != 0u)
      {
        m_targetStage = VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT;
      }
    }
    return m_targetStage != 0u;
  }


  uint64_t VUSwapchainPresentTiming::ToTargetStageTime(const TickCount time) const noexcept
  {
    if (time.Ticks() <= 0 || !m_hasTimeDomain)
    {
      return 0;
    }
    if (m_timeDomain == TimeDomainUtil::GetHostTimeDomain())
    {
      // The swapchain is on the clock of the framework
      return m_calibratedTimestamps.TickCountToHostTime(time);
    }
    const uint32_t stageIndex = ToStageIndex(m_targetStage);
    if (stageIndex >= LocalConfig::MaxStages || !m_hasStageOffset[stageIndex])
    {
      return 0;
    }
    // The inverse of what a time of the stage is converted with: nanoseconds on the clock of the swapchain
    const int64_t stageTicks = time.Ticks() - m_stageOffsetTicks[stageIndex];
    return stageTicks > 0 ? static_cast<uint64_t>(stageTicks) * TickCount::NanoSecondsPerTick : 0u;
  }


  const void* VUSwapchainPresentTiming::PreparePresent(VUPresentTimingPresentInfo& rPresentInfo, const uint64_t presentId,
                                                       const void* const pNext) noexcept
  {
    rPresentInfo = {};
    // A target time is for one present, and a present takes one: a time before which the image is not shown comes before a time the
    // image before it stays on screen
    const uint64_t absoluteTargetTime = m_canPresentAtAbsoluteTime ? ToTargetStageTime(m_nextAbsoluteTargetTime) : 0u;
    const uint64_t relativeTargetTime = (m_canPresentAtRelativeTime && absoluteTargetTime == 0u) ? m_nextRelativeTargetTime : 0u;
    m_nextAbsoluteTargetTime = {};
    m_nextRelativeTargetTime = 0;
    if (!IsEnabled())
    {
      return pNext;
    }

    rPresentInfo.Id = presentId;
    rPresentInfo.PresentId.sType = VK_STRUCTURE_TYPE_PRESENT_ID_2_KHR;
    rPresentInfo.PresentId.pNext = pNext;
    rPresentInfo.PresentId.swapchainCount = 1;
    rPresentInfo.PresentId.pPresentIds = &rPresentInfo.Id;

    // The measurements are held in a queue of the swapchain until they are collected and a present fails when the queue is full, so the
    // timing is only requested when there is room. A target time does not need room, it is given without asking for the stages.
    const bool requestTiming = m_outstanding < LocalConfig::MaxOutstanding;
    if (!requestTiming && relativeTargetTime == 0u && absoluteTargetTime == 0u)
    {
      return &rPresentInfo.PresentId;
    }

    rPresentInfo.TimingInfo.sType = VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT;
    rPresentInfo.TimingInfo.timeDomainId = m_timeDomainId;
    rPresentInfo.TimingInfo.presentStageQueries = requestTiming ? m_stageQueries : 0u;
    // Without a target time the present is only measured. With a absolute one the image is not shown before that time of the time
    // domain of the swapchain, with a relative one not before the time has passed since the image of the present before it was shown.
    if (absoluteTargetTime != 0u)
    {
      rPresentInfo.TimingInfo.targetTime = absoluteTargetTime;
      rPresentInfo.TimingInfo.targetTimeDomainPresentStage = m_targetStage;
    }
    else if (relativeTargetTime != 0u)
    {
      rPresentInfo.TimingInfo.targetTime = relativeTargetTime;
      rPresentInfo.TimingInfo.flags = VK_PRESENT_TIMING_INFO_PRESENT_AT_RELATIVE_TIME_BIT_EXT;
      rPresentInfo.TimingInfo.targetTimeDomainPresentStage = m_targetStage;
    }
    rPresentInfo.IsTimingRequested = requestTiming;
    rPresentInfo.RelativeTargetTimeNanoseconds = relativeTargetTime;
    rPresentInfo.AbsoluteTargetTime = absoluteTargetTime;

    rPresentInfo.TimingsInfo.sType = VK_STRUCTURE_TYPE_PRESENT_TIMINGS_INFO_EXT;
    rPresentInfo.TimingsInfo.pNext = &rPresentInfo.PresentId;
    rPresentInfo.TimingsInfo.swapchainCount = 1;
    rPresentInfo.TimingsInfo.pTimingInfos = &rPresentInfo.TimingInfo;
    return &rPresentInfo.TimingsInfo;
  }


  void VUSwapchainPresentTiming::OnPresent(const VUPresentTimingPresentInfo& presentInfo, const VkResult presentResult) noexcept
  {
    if (presentInfo.IsTimingRequested && (presentResult == VK_SUCCESS || presentResult == VK_SUBOPTIMAL_KHR))
    {
      ++m_outstanding;
    }
    if (IsEnabled() && ++m_presentsSinceCalibration >= LocalConfig::PresentsPerCalibration)
    {
      UpdateCalibration();
    }
  }


  void VUSwapchainPresentTiming::Poll(std::vector<VUPresentTimingRecord>& rRecords)
  {
    if (!IsEnabled())
    {
      return;
    }

    std::array<std::array<VkPresentStageTimeEXT, LocalConfig::MaxStages>, LocalConfig::MaxResultsPerCall> stageTimes{};
    std::array<VkPastPresentationTimingEXT, LocalConfig::MaxResultsPerCall> timings{};

    VkPastPresentationTimingInfoEXT timingInfo{};
    timingInfo.sType = VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_INFO_EXT;
    timingInfo.swapchain = m_swapchain;

    for (uint32_t callIndex = 0; callIndex < LocalConfig::MaxCallsPerPoll; ++callIndex)
    {
      for (std::size_t i = 0; i < timings.size(); ++i)
      {
        timings[i] = {};
        timings[i].sType = VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_EXT;
        timings[i].presentStageCount = LocalConfig::MaxStages;
        timings[i].pPresentStages = stageTimes[i].data();
      }

      VkPastPresentationTimingPropertiesEXT properties{};
      properties.sType = VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_PROPERTIES_EXT;
      properties.presentationTimingCount = static_cast<uint32_t>(timings.size());
      properties.pPresentationTimings = timings.data();

      const VkResult result = m_pfnGetPastPresentationTiming(m_device, &timingInfo, &properties);
      if (result != VK_SUCCESS && result != VK_INCOMPLETE)
      {
        FSLLOG3_VERBOSE3("vkGetPastPresentationTimingEXT failed with: {}", RapidVulkan::Debug::ToString(result));
        return;
      }

      const uint32_t timingCount = std::min(properties.presentationTimingCount, static_cast<uint32_t>(timings.size()));
      for (uint32_t i = 0; i < timingCount; ++i)
      {
        const VkPastPresentationTimingEXT& timing = timings[i];
        VUPresentTimingRecord record;
        record.PresentId = timing.presentId;
        record.IsComplete = timing.reportComplete == VK_TRUE;
        record.TimeDomain = static_cast<int32_t>(timing.timeDomain);
        record.TimeDomainId = timing.timeDomainId;
        const uint32_t stageCount = std::min(timing.presentStageCount, LocalConfig::MaxStages);
        for (uint32_t stageIndex = 0; stageIndex < stageCount; ++stageIndex)
        {
          const VkPresentStageTimeEXT& stageTime = timing.pPresentStages[stageIndex];
          FSLLOG3_VERBOSE4("Present timing: id {} stage {:#x} time {} domain {} id {} complete {} target {}", timing.presentId, stageTime.stage,
                           stageTime.time, static_cast<int32_t>(timing.timeDomain), timing.timeDomainId, timing.reportComplete, timing.targetTime);
          // A time of zero means the presentation engine has no result for the stage (which does not say the image was not shown)
          const uint32_t stageBitIndex = ToStageIndex(stageTime.stage);
          if (stageTime.time == 0u || stageBitIndex >= LocalConfig::MaxStages)
          {
            continue;
          }
          record.RawStageTimes[stageBitIndex] = stageTime.time;
          TickCount time;
          if (timing.timeDomain == TimeDomainUtil::GetHostTimeDomain())
          {
            time = m_calibratedTimestamps.HostTimeToTickCount(stageTime.time);
          }
          else if (timing.timeDomainId == m_timeDomainId && m_hasStageOffset[stageBitIndex])
          {
            // The time is in nanoseconds on a clock of the swapchain
            time = TickCount(static_cast<int64_t>(stageTime.time / TickCount::NanoSecondsPerTick) + m_stageOffsetTicks[stageBitIndex]);
          }
          else
          {
            continue;
          }
          switch (stageTime.stage)
          {
          case VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT:
            record.QueueOperationsEnd = time;
            break;
          case VK_PRESENT_STAGE_REQUEST_DEQUEUED_BIT_EXT:
            record.RequestDequeued = time;
            break;
          case VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT:
            record.FirstPixelOut = time;
            break;
          case VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT:
            record.FirstPixelVisible = time;
            break;
          default:
            break;
          }
        }
        rRecords.push_back(record);
      }
      m_outstanding -= std::min(m_outstanding, timingCount);

      // The counters change when the swapchain has new timing properties or time domains
      if (properties.timeDomainsCounter != m_timeDomainsCounter)
      {
        UpdateTimeDomain();
        UpdateCalibration();
      }
      if (properties.timingPropertiesCounter != m_timingPropertiesCounter)
      {
        UpdateTimingProperties();
      }
      if (result != VK_INCOMPLETE)
      {
        break;
      }
    }
  }


  void VUSwapchainPresentTiming::UpdateTimeDomain()
  {
    VkSwapchainTimeDomainPropertiesEXT properties{};
    properties.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_TIME_DOMAIN_PROPERTIES_EXT;
    uint64_t counter = 0;
    VkResult result = m_pfnGetTimeDomainProperties(m_device, m_swapchain, &properties, &counter);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || properties.timeDomainCount == 0u)
    {
      FSLLOG3_VERBOSE("Present timing: no time domains ({})", RapidVulkan::Debug::ToString(result));
      m_hasTimeDomain = false;
      return;
    }

    std::vector<VkTimeDomainKHR> timeDomains(properties.timeDomainCount);
    std::vector<uint64_t> timeDomainIds(properties.timeDomainCount);
    properties.pTimeDomains = timeDomains.data();
    properties.pTimeDomainIds = timeDomainIds.data();
    result = m_pfnGetTimeDomainProperties(m_device, m_swapchain, &properties, &counter);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE)
    {
      m_hasTimeDomain = false;
      return;
    }
    m_timeDomainsCounter = counter;

    const auto timeDomainCount = std::min(static_cast<std::size_t>(properties.timeDomainCount), timeDomains.size());
    m_hasTimeDomain = false;
    for (std::size_t i = 0; i < timeDomainCount; ++i)
    {
      FSLLOG3_VERBOSE2("Present timing: time domain {} id {}", static_cast<int32_t>(timeDomains[i]), timeDomainIds[i]);
      // The clock of the framework is preferred as its times need no conversion
      if (!m_hasTimeDomain || timeDomains[i] == TimeDomainUtil::GetHostTimeDomain())
      {
        m_timeDomain = timeDomains[i];
        m_timeDomainId = timeDomainIds[i];
        m_hasTimeDomain = true;
      }
    }
  }


  void VUSwapchainPresentTiming::UpdateCalibration()
  {
    m_presentsSinceCalibration = 0;
    m_hasStageOffset = {};
    if (!m_hasTimeDomain || m_timeDomain == TimeDomainUtil::GetHostTimeDomain())
    {
      // The times are reported on the clock of the framework, so there is nothing to relate
      return;
    }

    // Read the clock of the swapchain and the clock of the framework at the same moment. With VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT every
    // stage can have its own clock, so each of them is related to the clock of the framework.
    const bool isStageLocal = m_timeDomain == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT;
    bool hasCalibrated = false;
    for (uint32_t stageIndex = 0; stageIndex < LocalConfig::MaxStages; ++stageIndex)
    {
      const VkPresentStageFlagsEXT stage = VkPresentStageFlagsEXT{1} << stageIndex;
      if ((m_stageQueries & stage) == 0u)
      {
        continue;
      }

      VkSwapchainCalibratedTimestampInfoEXT swapchainInfo{};
      swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CALIBRATED_TIMESTAMP_INFO_EXT;
      swapchainInfo.swapchain = m_swapchain;
      swapchainInfo.presentStage = isStageLocal ? stage : 0u;
      swapchainInfo.timeDomainId = m_timeDomainId;

      std::array<VkCalibratedTimestampInfoKHR, 2> timestampInfos{};
      timestampInfos[0].sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR;
      timestampInfos[0].pNext = &swapchainInfo;
      timestampInfos[0].timeDomain = m_timeDomain;
      timestampInfos[1].sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR;
      timestampInfos[1].timeDomain = TimeDomainUtil::GetHostTimeDomain();

      std::array<uint64_t, 2> timestamps{};
      uint64_t maxDeviation = 0;
      const VkResult result =
        m_calibratedTimestamps.GetTimestamps(static_cast<uint32_t>(timestampInfos.size()), timestampInfos.data(), timestamps.data(), &maxDeviation);
      if (result != VK_SUCCESS)
      {
        FSLLOG3_VERBOSE2("Present timing: the clock of stage {:#x} could not be calibrated ({})", stage, RapidVulkan::Debug::ToString(result));
        continue;
      }
      const TickCount hostTime = m_calibratedTimestamps.HostTimeToTickCount(timestamps[1]);
      m_stageOffsetTicks[stageIndex] = hostTime.Ticks() - static_cast<int64_t>(timestamps[0] / TickCount::NanoSecondsPerTick);
      m_hasStageOffset[stageIndex] = true;
      m_state.StageMaxDeviationNanoseconds[stageIndex] = maxDeviation;
      hasCalibrated = true;
      FSLLOG3_VERBOSE3("Present timing: stage {:#x} clock {} ns is host time {} (max deviation {} ns)", stage, timestamps[0], hostTime.Ticks(),
                       maxDeviation);
    }
    if (hasCalibrated)
    {
      ++m_state.CalibrationCount;
    }
  }


  void VUSwapchainPresentTiming::UpdateTimingProperties()
  {
    VkSwapchainTimingPropertiesEXT properties{};
    properties.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_TIMING_PROPERTIES_EXT;
    uint64_t counter = 0;
    const VkResult result = m_pfnGetTimingProperties(m_device, m_swapchain, &properties, &counter);
    FSLLOG3_VERBOSE2("Present timing: timing properties {} refreshDuration {} ns refreshInterval {} ns counter {}",
                     RapidVulkan::Debug::ToString(result), properties.refreshDuration, properties.refreshInterval, counter);
    if (result != VK_SUCCESS)
    {
      // VK_NOT_READY: the swapchain does not know yet
      return;
    }
    m_timingPropertiesCounter = counter;
    m_refreshDuration = TimeSpan(static_cast<int64_t>(properties.refreshDuration / TickCount::NanoSecondsPerTick));
    m_state.RefreshDurationNanoseconds = properties.refreshDuration;
    m_state.RefreshIntervalNanoseconds = properties.refreshInterval;
    m_state.TimingPropertiesCounter = counter;
    ++m_state.TimingPropertiesReadCount;
  }

#else

  VkSwapchainCreateFlagsKHR VUSwapchainPresentTiming::GetSwapchainCreateFlags(const VkPhysicalDevice /*physicalDevice*/,
                                                                              const VkSurfaceKHR /*surface*/)
  {
    return 0;
  }


  void VUSwapchainPresentTiming::Reset() noexcept
  {
    m_swapchain = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_state = {};
  }


  bool VUSwapchainPresentTiming::TryEnablePresentAtRelativeTime() noexcept
  {
    return false;
  }


  bool VUSwapchainPresentTiming::TryEnablePresentAtAbsoluteTime() noexcept
  {
    return false;
  }


  VUPresentTimingState VUSwapchainPresentTiming::GetState() const noexcept
  {
    return m_state;
  }


  bool VUSwapchainPresentTiming::Reset(const VkPhysicalDevice /*physicalDevice*/, const VkDevice /*device*/, const VkSurfaceKHR /*surface*/,
                                       const VkSwapchainKHR /*swapchain*/, const VUCalibratedTimestamps& /*calibratedTimestamps*/)
  {
    Reset();
    return false;
  }


  const void* VUSwapchainPresentTiming::PreparePresent(VUPresentTimingPresentInfo& /*rPresentInfo*/, const uint64_t /*presentId*/,
                                                       const void* const pNext) noexcept
  {
    return pNext;
  }


  void VUSwapchainPresentTiming::OnPresent(const VUPresentTimingPresentInfo& /*presentInfo*/, const VkResult /*presentResult*/) noexcept
  {
  }


  void VUSwapchainPresentTiming::Poll(std::vector<VUPresentTimingRecord>& /*rRecords*/)
  {
  }


  void VUSwapchainPresentTiming::UpdateTimeDomain()
  {
  }


  void VUSwapchainPresentTiming::UpdateTimingProperties()
  {
  }


  void VUSwapchainPresentTiming::UpdateCalibration()
  {
  }

#endif
}
