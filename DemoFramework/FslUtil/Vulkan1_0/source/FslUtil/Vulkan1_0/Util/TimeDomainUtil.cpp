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

#include <FslUtil/Vulkan1_0/Util/TimeDomainUtil.hpp>

#ifdef FSL_VULKAN_CALIBRATED_TIMESTAMPS_SUPPORTED

namespace Fsl::Vulkan::TimeDomainUtil
{
  TickCount ToTickCount(const VkTimeDomainKHR timeDomain, const uint64_t value, const uint64_t performanceCounterFrequency) noexcept
  {
    switch (timeDomain)
    {
    case VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR:
      {
        if (performanceCounterFrequency == 0u)
        {
          return {};
        }
        // The same conversion HighResolutionTimer::GetTimestamp does, so the results can be compared
        const double countsPerTick = static_cast<double>(performanceCounterFrequency) / static_cast<double>(TickCount::TicksPerSecond);
        return TickCount(static_cast<int64_t>(static_cast<double>(value) / countsPerTick));
      }
    case VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR:
    case VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR:
      // Nanoseconds
      return TickCount(static_cast<int64_t>(value / TickCount::NanoSecondsPerTick));
    default:
      return {};
    }
  }


  uint64_t FromTickCount(const VkTimeDomainKHR timeDomain, const TickCount time, const uint64_t performanceCounterFrequency) noexcept
  {
    if (time.Ticks() <= 0)
    {
      return 0;
    }
    switch (timeDomain)
    {
    case VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR:
      {
        if (performanceCounterFrequency == 0u)
        {
          return 0;
        }
        // The inverse of the conversion of ToTickCount
        const double countsPerTick = static_cast<double>(performanceCounterFrequency) / static_cast<double>(TickCount::TicksPerSecond);
        return static_cast<uint64_t>(static_cast<double>(time.Ticks()) * countsPerTick);
      }
    case VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR:
    case VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR:
      // Nanoseconds
      return static_cast<uint64_t>(time.Ticks()) * TickCount::NanoSecondsPerTick;
    default:
      return 0;
    }
  }
}

#endif
