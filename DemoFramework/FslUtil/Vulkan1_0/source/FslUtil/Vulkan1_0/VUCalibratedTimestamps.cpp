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
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslUtil/Vulkan1_0/VUCalibratedTimestamps.hpp>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace Fsl::Vulkan
{
  VUCalibratedTimestamps::VUCalibratedTimestamps()
    : m_performanceCounterFrequency(HighResolutionTimer().GetNativeTickFrequency())
  {
  }

#ifdef FSL_VULKAN_CALIBRATED_TIMESTAMPS_SUPPORTED

  namespace
  {
    //! The time domains that can be calibrated (empty if the entry point was not found)
    //! @param pszName the name of the entry point of the extension the device was created with
    std::vector<VkTimeDomainKHR> GetCalibrateableTimeDomains(const VkInstance instance, const VkPhysicalDevice physicalDevice,
                                                             const char* const pszName)
    {
      // The loader does not export the entry points of the extension, so it is looked up. The EXT version has the same signature.
      const auto pfnGetTimeDomains = reinterpret_cast<PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR>(vkGetInstanceProcAddr(instance, pszName));
      if (pfnGetTimeDomains == nullptr)
      {
        return {};
      }

      uint32_t count = 0;
      if (pfnGetTimeDomains(physicalDevice, &count, nullptr) != VK_SUCCESS || count == 0u)
      {
        return {};
      }
      std::vector<VkTimeDomainKHR> timeDomains(count);
      const VkResult result = pfnGetTimeDomains(physicalDevice, &count, timeDomains.data());
      if (result != VK_SUCCESS && result != VK_INCOMPLETE)
      {
        return {};
      }
      timeDomains.resize(std::min(static_cast<std::size_t>(count), timeDomains.size()));
      return timeDomains;
    }
  }


  VUCalibratedTimestamps::VUCalibratedTimestamps(const VkInstance instance, const VkPhysicalDevice physicalDevice, const VkDevice device,
                                                 const VUCalibratedTimestampsApi api)
    : m_device(device)
    , m_performanceCounterFrequency(HighResolutionTimer().GetNativeTickFrequency())
  {
    if (instance == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE)
    {
      throw std::invalid_argument("instance, physicalDevice and device can not be VK_NULL_HANDLE");
    }
    if (api == VUCalibratedTimestampsApi::Disabled)
    {
      return;
    }

    // Only the entry points of the extension the device was created with are asked for. The extension has two names, and a driver
    // that has it under the old name need not know the entry points of the new one: what it then hands out for such a name can
    // not be relied on, so the other name is never tried.
    const bool isKhr = api == VUCalibratedTimestampsApi::Khr;
    const char* const pszGetTimestamps = isKhr ? "vkGetCalibratedTimestampsKHR" : "vkGetCalibratedTimestampsEXT";
    const char* const pszGetTimeDomains = isKhr ? "vkGetPhysicalDeviceCalibrateableTimeDomainsKHR" : "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT";

    m_pfnGetCalibratedTimestamps = reinterpret_cast<PFN_vkGetCalibratedTimestampsKHR>(vkGetDeviceProcAddr(device, pszGetTimestamps));
    if (m_pfnGetCalibratedTimestamps == nullptr)
    {
      FSLLOG3_WARNING("The entry point {} was not found, was the device created with the extension enabled?", pszGetTimestamps);
      return;
    }

    const std::vector<VkTimeDomainKHR> timeDomains = GetCalibrateableTimeDomains(instance, physicalDevice, pszGetTimeDomains);
    const auto contains = [&timeDomains](const VkTimeDomainKHR timeDomain)
    { return std::find(timeDomains.begin(), timeDomains.end(), timeDomain) != timeDomains.end(); };
    m_isSupported = contains(VK_TIME_DOMAIN_DEVICE_KHR) && contains(TimeDomainUtil::GetHostTimeDomain());
    FSLLOG3_VERBOSE_IF(!m_isSupported, "Calibrated timestamps: the device clock or the host clock can not be calibrated");
  }


  bool VUCalibratedTimestamps::HasEntryPoint() const noexcept
  {
    return m_pfnGetCalibratedTimestamps != nullptr;
  }


  bool VUCalibratedTimestamps::TryGet(VUCalibratedTimestamp& rTimestamp) const noexcept
  {
    if (!m_isSupported)
    {
      rTimestamp = {};
      return false;
    }

    std::array<VkCalibratedTimestampInfoKHR, 2> timestampInfos{};
    timestampInfos[0].sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR;
    timestampInfos[0].timeDomain = VK_TIME_DOMAIN_DEVICE_KHR;
    timestampInfos[1].sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR;
    timestampInfos[1].timeDomain = TimeDomainUtil::GetHostTimeDomain();

    std::array<uint64_t, 2> timestamps{};
    uint64_t maxDeviationNanoseconds = 0;
    if (GetTimestamps(static_cast<uint32_t>(timestampInfos.size()), timestampInfos.data(), timestamps.data(), &maxDeviationNanoseconds) != VK_SUCCESS)
    {
      rTimestamp = {};
      return false;
    }

    rTimestamp.DeviceTimestamp = VUDeviceTimestamp(timestamps[0]);
    rTimestamp.HostTime = HostTimeToTickCount(timestamps[1]);
    rTimestamp.MaxDeviation = TimeSpan(static_cast<int64_t>(maxDeviationNanoseconds / TickCount::NanoSecondsPerTick));
    return true;
  }


  TickCount VUCalibratedTimestamps::HostTimeToTickCount(const uint64_t hostTime) const noexcept
  {
    return TimeDomainUtil::ToTickCount(TimeDomainUtil::GetHostTimeDomain(), hostTime, m_performanceCounterFrequency);
  }


  uint64_t VUCalibratedTimestamps::TickCountToHostTime(const TickCount time) const noexcept
  {
    return TimeDomainUtil::FromTickCount(TimeDomainUtil::GetHostTimeDomain(), time, m_performanceCounterFrequency);
  }


  VkResult VUCalibratedTimestamps::GetTimestamps(const uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* const pTimestampInfos,
                                                 uint64_t* const pTimestamps, uint64_t* const pMaxDeviation) const noexcept
  {
    if (m_pfnGetCalibratedTimestamps == nullptr)
    {
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    return m_pfnGetCalibratedTimestamps(m_device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation);
  }

#else

  VUCalibratedTimestamps::VUCalibratedTimestamps(const VkInstance /*instance*/, const VkPhysicalDevice /*physicalDevice*/, const VkDevice device,
                                                 const VUCalibratedTimestampsApi /*api*/)
    : m_device(device)
    , m_performanceCounterFrequency(HighResolutionTimer().GetNativeTickFrequency())
  {
  }


  bool VUCalibratedTimestamps::HasEntryPoint() const noexcept
  {
    return false;
  }


  bool VUCalibratedTimestamps::TryGet(VUCalibratedTimestamp& rTimestamp) const noexcept
  {
    rTimestamp = {};
    return false;
  }


  TickCount VUCalibratedTimestamps::HostTimeToTickCount(const uint64_t /*hostTime*/) const noexcept
  {
    return {};
  }


  uint64_t VUCalibratedTimestamps::TickCountToHostTime(const TickCount /*time*/) const noexcept
  {
    return 0;
  }

#endif
}
