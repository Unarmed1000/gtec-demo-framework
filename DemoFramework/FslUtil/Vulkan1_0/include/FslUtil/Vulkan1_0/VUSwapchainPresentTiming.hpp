#ifndef FSLUTIL_VULKAN1_0_VUSWAPCHAINPRESENTTIMING_HPP
#define FSLUTIL_VULKAN1_0_VUSWAPCHAINPRESENTTIMING_HPP
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
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslUtil/Vulkan1_0/Common.hpp>
#include <FslUtil/Vulkan1_0/VUCalibratedTimestamps.hpp>
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#if defined(VK_EXT_present_timing) && defined(VK_KHR_present_id2) && defined(VK_KHR_calibrated_timestamps) && \
  defined(VK_KHR_get_surface_capabilities2)
//! Defined when the Vulkan headers the framework is built with know the extensions needed for present timing
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FSL_VULKAN_PRESENT_TIMING_SUPPORTED 1
#endif

namespace Fsl::Vulkan
{
  //! When a presented image went through the stages of being presented, on the clock of the framework (comparable with
  //! HighResolutionTimer::GetTimestamp). A stage is only set if the presentation engine reports it.
  struct VUPresentTimingRecord
  {
    //! The id that was given to the present (VUSwapchainPresentTiming::PreparePresent)
    uint64_t PresentId{0};
    // A stage is empty if the swapchain does not report it, or if the presentation engine had no time for it for this present. The last
    // one happens (seen for FirstPixelOut on a desktop that composes the window) and does not mean the image was not shown.

    //! The queue operations of the present ended (the image was handed to the presentation engine)
    std::optional<TickCount> QueueOperationsEnd;
    //! The presentation engine took the present request from its queue
    std::optional<TickCount> RequestDequeued;
    //! The first pixel of the image left the device for the display
    std::optional<TickCount> FirstPixelOut;
    //! The first pixel of the image became visible on the display
    std::optional<TickCount> FirstPixelVisible;

    //! @return the best available time for 'the image reached the display': FirstPixelVisible if reported, else FirstPixelOut.
    [[nodiscard]] std::optional<TickCount> GetDisplayTime() const noexcept
    {
      return FirstPixelVisible.has_value() ? FirstPixelVisible : FirstPixelOut;
    }
  };


  //! The structs that are chained to the VkPresentInfoKHR of one present, filled by VUSwapchainPresentTiming::PreparePresent.
  //! It must stay alive until vkQueuePresentKHR returns.
  struct VUPresentTimingPresentInfo
  {
#ifdef FSL_VULKAN_PRESENT_TIMING_SUPPORTED
    uint64_t Id{0};
    VkPresentId2KHR PresentId{};
    VkPresentTimingInfoEXT TimingInfo{};
    VkPresentTimingsInfoEXT TimingsInfo{};
#endif
  };


  //! Measures when the images of a swapchain were presented with VK_EXT_present_timing and VK_KHR_present_id2.
  //!
  //! Use:
  //! - create the swapchain with the flags from GetSwapchainCreateFlags (zero means the surface does not support it),
  //! - call Reset with the new swapchain, and Reset() before the swapchain is destroyed,
  //! - chain what PreparePresent returns to every VkPresentInfoKHR and call OnPresent with the result of the present,
  //! - call Poll once per frame to collect the measurements.
  //!
  //! The device must have been created with the two extensions (and VK_KHR_calibrated_timestamps) and their presentTiming and presentId2
  //! features enabled.
  class VUSwapchainPresentTiming final
  {
    VkDevice m_device{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VUCalibratedTimestamps m_calibratedTimestamps;
    TimeSpan m_refreshDuration;
    uint32_t m_queueSize{0};
    //! The number of presents that requested timing and have not been collected yet
    uint32_t m_outstanding{0};
#ifdef FSL_VULKAN_PRESENT_TIMING_SUPPORTED
    PFN_vkSetSwapchainPresentTimingQueueSizeEXT m_pfnSetQueueSize{nullptr};
    PFN_vkGetSwapchainTimingPropertiesEXT m_pfnGetTimingProperties{nullptr};
    PFN_vkGetSwapchainTimeDomainPropertiesEXT m_pfnGetTimeDomainProperties{nullptr};
    PFN_vkGetPastPresentationTimingEXT m_pfnGetPastPresentationTiming{nullptr};
    //! The stages the surface can report
    VkPresentStageFlagsEXT m_stageQueries{0};
    VkTimeDomainKHR m_timeDomain{VK_TIME_DOMAIN_DEVICE_KHR};
    uint64_t m_timeDomainId{0};
    uint64_t m_timeDomainsCounter{0};
    uint64_t m_timingPropertiesCounter{0};
    bool m_hasTimeDomain{false};
    //! What to add to a time of a stage (converted to 100ns ticks) to get the time on the clock of the framework. One entry for each
    //! VkPresentStageFlagBitsEXT as a swapchain can report each stage on its own clock.
    std::array<int64_t, 4> m_stageOffsetTicks{};
    std::array<bool, 4> m_hasStageOffset{};
    uint32_t m_presentsSinceCalibration{0};
#endif

  public:
    VUSwapchainPresentTiming() = default;

    //! @brief Get the swapchain create flags that are needed to measure the presents of a swapchain of the surface.
    //! @return zero if the surface does not support present timing (or the Vulkan headers this was built with lack the extensions).
    static VkSwapchainCreateFlagsKHR GetSwapchainCreateFlags(const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface);

    //! @brief Stop measuring, call it before the swapchain is destroyed.
    void Reset() noexcept;

    //! @brief Start measuring the presents of the swapchain.
    //! @param swapchain a swapchain that was created with the flags returned by GetSwapchainCreateFlags (which must not be zero).
    //! @param calibratedTimestamps used when the swapchain does not report its times on the clock of the framework.
    //! @return true if the presents will be measured.
    bool Reset(const VkPhysicalDevice physicalDevice, const VkDevice device, const VkSurfaceKHR surface, const VkSwapchainKHR swapchain,
               const VUCalibratedTimestamps& calibratedTimestamps);

    //! @return true if the presents of the swapchain are being measured.
    [[nodiscard]] bool IsEnabled() const noexcept
    {
      return m_swapchain != VK_NULL_HANDLE;
    }

    //! @brief Prepare the structs of a present.
    //! @param rPresentInfo the storage of the structs, it must stay alive until the present was queued.
    //! @param presentId the id of the present, it must be greater than the id of the previous present of the swapchain.
    //! @param pNext the pNext chain to continue with.
    //! @return the pNext to give to VkPresentInfoKHR (pNext if not enabled).
    [[nodiscard]] const void* PreparePresent(VUPresentTimingPresentInfo& rPresentInfo, const uint64_t presentId, const void* const pNext) noexcept;

    //! @brief Call this with the result of the vkQueuePresentKHR that was given what PreparePresent returned.
    void OnPresent(const VUPresentTimingPresentInfo& presentInfo, const VkResult presentResult) noexcept;

    //! @brief Collect the measurements that are ready.
    //! @param rRecords the measurements are appended to this.
    void Poll(std::vector<VUPresentTimingRecord>& rRecords);

    //! @return the duration of a refresh cycle of the display as reported by the swapchain (zero if not known yet).
    [[nodiscard]] TimeSpan GetRefreshDuration() const noexcept
    {
      return m_refreshDuration;
    }

  private:
    void UpdateTimeDomain();
    void UpdateTimingProperties();
    void UpdateCalibration();
  };
}

#endif
