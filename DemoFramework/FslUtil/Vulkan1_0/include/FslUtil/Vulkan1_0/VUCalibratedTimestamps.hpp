#ifndef FSLUTIL_VULKAN1_0_VUCALIBRATEDTIMESTAMPS_HPP
#define FSLUTIL_VULKAN1_0_VUCALIBRATEDTIMESTAMPS_HPP
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
#include <FslUtil/Vulkan1_0/Util/TimeDomainUtil.hpp>
#include <FslUtil/Vulkan1_0/VUCalibratedTimestampsApi.hpp>
#include <FslUtil/Vulkan1_0/VUDeviceTimestamp.hpp>
#include <vulkan/vulkan.h>
#include <cstdint>

namespace Fsl::Vulkan
{
  //! The same moment read on the clock of the device (what a timestamp query writes) and on the clock of the framework.
  struct VUCalibratedTimestamp
  {
    //! The time on the clock of the device
    VUDeviceTimestamp DeviceTimestamp;
    //! The time on the clock of the framework (comparable with HighResolutionTimer::GetTimestamp)
    TickCount HostTime;
    //! How far the two can be from being the same moment
    TimeSpan MaxDeviation;
  };


  //! Relates the clock of the device to the clock of the framework with VK_KHR_calibrated_timestamps (or the EXT version).
  //! The extension must have been enabled when the device was created.
  class VUCalibratedTimestamps final
  {
    VkDevice m_device{VK_NULL_HANDLE};
    //! Always set, so HostTimeToTickCount works for a object that is not supported
    uint64_t m_performanceCounterFrequency{0};
    bool m_isSupported{false};
#ifdef FSL_VULKAN_CALIBRATED_TIMESTAMPS_SUPPORTED
    PFN_vkGetCalibratedTimestampsKHR m_pfnGetCalibratedTimestamps{nullptr};
#endif

  public:
    VUCalibratedTimestamps();

    //! @param instance the instance of the device, used to find out which time domains can be calibrated.
    //! @param device the device (it must outlive this object).
    //! @param api the calibrated timestamps extension the device was created with. Only its entry points are looked up, the ones of
    //!        the other name of the extension are not asked for. Disabled gives a object that is not supported.
    VUCalibratedTimestamps(const VkInstance instance, const VkPhysicalDevice physicalDevice, const VkDevice device,
                           const VUCalibratedTimestampsApi api);

    //! @return true if TryGet works: the entry points were found and both the device and the host clock can be calibrated.
    [[nodiscard]] bool IsSupported() const noexcept
    {
      return m_isSupported;
    }

    //! @return true if the entry point was found, which is all GetTimestamps needs.
    [[nodiscard]] bool HasEntryPoint() const noexcept;

    //! @brief Read the device clock and the host clock at (nearly) the same moment.
    bool TryGet(VUCalibratedTimestamp& rTimestamp) const noexcept;

    //! @brief Convert a value of the host time domain (TimeDomainUtil::GetHostTimeDomain) to the time of the framework clock.
    [[nodiscard]] TickCount HostTimeToTickCount(const uint64_t hostTime) const noexcept;

#ifdef FSL_VULKAN_CALIBRATED_TIMESTAMPS_SUPPORTED
    //! @brief vkGetCalibratedTimestampsKHR for callers that need other time domains.
    //! @return VK_ERROR_EXTENSION_NOT_PRESENT if the entry point was not found.
    VkResult GetTimestamps(const uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* const pTimestampInfos, uint64_t* const pTimestamps,
                           uint64_t* const pMaxDeviation) const noexcept;
#endif
  };
}

#endif
