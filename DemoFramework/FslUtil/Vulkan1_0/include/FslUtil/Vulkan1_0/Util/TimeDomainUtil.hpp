#ifndef FSLUTIL_VULKAN1_0_UTIL_TIMEDOMAINUTIL_HPP
#define FSLUTIL_VULKAN1_0_UTIL_TIMEDOMAINUTIL_HPP
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
#include <FslUtil/Vulkan1_0/Common.hpp>
#include <vulkan/vulkan.h>
#include <cstdint>

#ifdef VK_KHR_calibrated_timestamps
//! Defined when the Vulkan headers the framework is built with know VK_KHR_calibrated_timestamps (and with it the VkTimeDomainKHR type)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FSL_VULKAN_CALIBRATED_TIMESTAMPS_SUPPORTED 1
#endif

#ifdef FSL_VULKAN_CALIBRATED_TIMESTAMPS_SUPPORTED

//! Relates the Vulkan time domains to the clock of the framework (HighResolutionTimer).
namespace Fsl::Vulkan::TimeDomainUtil
{
  //! @return the time domain of the clock HighResolutionTimer reads on this platform.
  constexpr VkTimeDomainKHR GetHostTimeDomain() noexcept
  {
#ifdef _WIN32
    return VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR;
#else
    return VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR;
#endif
  }

  //! @return true if a value of the time domain can be converted with ToTickCount.
  constexpr bool IsHostClock(const VkTimeDomainKHR timeDomain) noexcept
  {
    return timeDomain == VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR || timeDomain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR ||
           timeDomain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR;
  }

  //! @brief Convert a value of a host clock time domain to a TickCount.
  //!        For the GetHostTimeDomain() domain the result can be compared with HighResolutionTimer::GetTimestamp.
  //! @param timeDomain a time domain that IsHostClock.
  //! @param value the time. Nanoseconds for the CLOCK_MONOTONIC domains and counts for QUERY_PERFORMANCE_COUNTER.
  //! @param performanceCounterFrequency the counts per second of the performance counter (HighResolutionTimer::GetNativeTickFrequency), only
  //!        used for QUERY_PERFORMANCE_COUNTER.
  //! @return the time in 100ns ticks (a default TickCount if the time domain is not a host clock or the frequency is zero).
  TickCount ToTickCount(const VkTimeDomainKHR timeDomain, const uint64_t value, const uint64_t performanceCounterFrequency) noexcept;

  //! @brief Convert a TickCount to a value of a host clock time domain: the inverse of ToTickCount.
  //! @param timeDomain a time domain that IsHostClock.
  //! @param time a time of the clock HighResolutionTimer reads.
  //! @param performanceCounterFrequency the counts per second of the performance counter, only used for QUERY_PERFORMANCE_COUNTER.
  //! @return the time in the unit of the time domain: nanoseconds for the CLOCK_MONOTONIC domains and counts for QUERY_PERFORMANCE_COUNTER
  //!         (zero if the time domain is not a host clock, the frequency is zero or the time is not after zero).
  uint64_t FromTickCount(const VkTimeDomainKHR timeDomain, const TickCount time, const uint64_t performanceCounterFrequency) noexcept;
}

#endif
#endif
