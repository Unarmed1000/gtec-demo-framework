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
#include <FslUtil/Vulkan1_0/Debug/VUDeviceFault.hpp>
#include <RapidVulkan/Debug/Strings/VkResult.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <array>
#include <exception>
#include <iterator>
#include <stdexcept>

namespace Fsl::Vulkan
{
  namespace
  {
    namespace LocalConfig
    {
      //! How long LogFaults waits for the driver to produce its fault reports
      constexpr TimeSpan LogTimeout = TimeSpan::FromSeconds(1);
    }

#if defined(VK_KHR_device_fault) || defined(VK_EXT_device_fault)

    // The EXT names are used as both generations of the headers have them (the newer ones as aliases of the KHR names)
    const char* ToString(const VkDeviceFaultAddressTypeEXT addressType) noexcept
    {
      switch (addressType)
      {
      case VK_DEVICE_FAULT_ADDRESS_TYPE_NONE_EXT:
        return "none";
      case VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT:
        return "invalid read";
      case VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT:
        return "invalid write";
      case VK_DEVICE_FAULT_ADDRESS_TYPE_EXECUTE_INVALID_EXT:
        return "invalid execute";
      case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_UNKNOWN_EXT:
        return "instruction pointer unknown";
      case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_INVALID_EXT:
        return "instruction pointer invalid";
      case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_FAULT_EXT:
        return "instruction pointer fault";
      default:
        return "unknown";
      }
    }

    void AppendAddressInfo(fmt::memory_buffer& rBuffer, const char* const pszDesc, const VkDeviceFaultAddressInfoEXT& info)
    {
      fmt::format_to(std::back_inserter(rBuffer), "\n  {}: {} at {:#x} (precision {:#x})", pszDesc, ToString(info.addressType), info.reportedAddress,
                     info.addressPrecision);
    }

    void AppendVendorInfo(fmt::memory_buffer& rBuffer, const VkDeviceFaultVendorInfoEXT& info)
    {
      fmt::format_to(std::back_inserter(rBuffer), "\n  vendor: '{}' code {:#x} data {:#x}", static_cast<const char*>(info.description),
                     info.vendorFaultCode, info.vendorFaultData);
    }

#endif

#ifdef VK_KHR_device_fault

    void AppendFlags(fmt::memory_buffer& rBuffer, const VkDeviceFaultFlagsKHR flags)
    {
      struct FlagName
      {
        VkDeviceFaultFlagsKHR Flag;
        const char* Name;
      };
      constexpr std::array<FlagName, 6> FlagNames = {
        FlagName{VK_DEVICE_FAULT_FLAG_DEVICE_LOST_KHR, "device lost"},
        FlagName{VK_DEVICE_FAULT_FLAG_MEMORY_ADDRESS_KHR, "memory address"},
        FlagName{VK_DEVICE_FAULT_FLAG_INSTRUCTION_ADDRESS_KHR, "instruction address"},
        FlagName{VK_DEVICE_FAULT_FLAG_VENDOR_KHR, "vendor"},
        FlagName{VK_DEVICE_FAULT_FLAG_WATCHDOG_TIMEOUT_KHR, "watchdog timeout"},
        FlagName{VK_DEVICE_FAULT_FLAG_OVERFLOW_KHR, "overflow"},
      };

      bool isFirst = true;
      for (const auto& entry : FlagNames)
      {
        if ((flags & entry.Flag) != 0u)
        {
          fmt::format_to(std::back_inserter(rBuffer), "{}{}", isFirst ? "" : ", ", entry.Name);
          isFirst = false;
        }
      }
    }

    std::string ToFaultString(const VkDeviceFaultInfoKHR& info)
    {
      fmt::memory_buffer buffer;
      fmt::format_to(std::back_inserter(buffer), "'{}' (", static_cast<const char*>(info.description));
      AppendFlags(buffer, info.flags);
      fmt::format_to(std::back_inserter(buffer), ") group {}", info.groupId);
      // The flags tell which parts of the report that are valid
      if ((info.flags & VK_DEVICE_FAULT_FLAG_MEMORY_ADDRESS_KHR) != 0u)
      {
        AppendAddressInfo(buffer, "memory", info.faultAddressInfo);
      }
      if ((info.flags & VK_DEVICE_FAULT_FLAG_INSTRUCTION_ADDRESS_KHR) != 0u)
      {
        AppendAddressInfo(buffer, "instruction", info.instructionAddressInfo);
      }
      if ((info.flags & VK_DEVICE_FAULT_FLAG_VENDOR_KHR) != 0u)
      {
        AppendVendorInfo(buffer, info.vendorInfo);
      }
      return fmt::to_string(buffer);
    }

    VUDeviceFaultReport GetReportKHR(const PFN_vkGetDeviceFaultReportsKHR pfnGetDeviceFaultReportsKHR, const VkDevice device, const TimeSpan timeout)
    {
      VUDeviceFaultReport report;
      const auto timeoutNanoseconds = static_cast<uint64_t>(std::max(timeout.Ticks(), int64_t{0})) * 100u;

      uint32_t faultCount = 0;
      report.Result = pfnGetDeviceFaultReportsKHR(device, timeoutNanoseconds, &faultCount, nullptr);
      if ((report.Result != VK_SUCCESS && report.Result != VK_INCOMPLETE) || faultCount == 0u)
      {
        return report;
      }

      VkDeviceFaultInfoKHR emptyInfo{};
      emptyInfo.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_KHR;
      std::vector<VkDeviceFaultInfoKHR> faults(faultCount, emptyInfo);
      // The reports are available now, so there is nothing to wait for
      report.Result = pfnGetDeviceFaultReportsKHR(device, 0, &faultCount, faults.data());
      if (report.Result != VK_SUCCESS && report.Result != VK_INCOMPLETE)
      {
        return report;
      }

      faults.resize(std::min(static_cast<std::size_t>(faultCount), faults.size()));
      for (const auto& fault : faults)
      {
        report.Faults.push_back(ToFaultString(fault));
      }
      return report;
    }

#endif

#ifdef VK_EXT_device_fault

    VUDeviceFaultReport GetReportEXT(const PFN_vkGetDeviceFaultInfoEXT pfnGetDeviceFaultInfoEXT, const VkDevice device)
    {
      VUDeviceFaultReport report;

      VkDeviceFaultCountsEXT faultCounts{};
      faultCounts.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT;
      report.Result = pfnGetDeviceFaultInfoEXT(device, &faultCounts, nullptr);
      if (report.Result != VK_SUCCESS && report.Result != VK_INCOMPLETE)
      {
        return report;
      }

      std::vector<VkDeviceFaultAddressInfoEXT> addressInfos(faultCounts.addressInfoCount);
      std::vector<VkDeviceFaultVendorInfoEXT> vendorInfos(faultCounts.vendorInfoCount);
      // The vendor binary is not requested
      faultCounts.vendorBinarySize = 0;

      VkDeviceFaultInfoEXT faultInfo{};
      faultInfo.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT;
      faultInfo.pAddressInfos = addressInfos.empty() ? nullptr : addressInfos.data();
      faultInfo.pVendorInfos = vendorInfos.empty() ? nullptr : vendorInfos.data();
      report.Result = pfnGetDeviceFaultInfoEXT(device, &faultCounts, &faultInfo);
      if (report.Result != VK_SUCCESS && report.Result != VK_INCOMPLETE)
      {
        return report;
      }

      addressInfos.resize(std::min(static_cast<std::size_t>(faultCounts.addressInfoCount), addressInfos.size()));
      vendorInfos.resize(std::min(static_cast<std::size_t>(faultCounts.vendorInfoCount), vendorInfos.size()));

      fmt::memory_buffer buffer;
      fmt::format_to(std::back_inserter(buffer), "'{}'", static_cast<const char*>(faultInfo.description));
      for (const auto& info : addressInfos)
      {
        AppendAddressInfo(buffer, "address", info);
      }
      for (const auto& info : vendorInfos)
      {
        AppendVendorInfo(buffer, info);
      }
      report.Faults.push_back(fmt::to_string(buffer));
      return report;
    }

#endif
  }


  VUDeviceFault::VUDeviceFault(const VkDevice device, const VUDeviceFaultApi api)
    : m_device(device)
  {
    if (device == VK_NULL_HANDLE)
    {
      throw std::invalid_argument("device can not be VK_NULL_HANDLE");
    }

    // The loader does not export the entry points of the extensions, so they are looked up
    switch (api)
    {
    case VUDeviceFaultApi::Khr:
#ifdef VK_KHR_device_fault
      m_pfnGetDeviceFaultReportsKHR = reinterpret_cast<PFN_vkGetDeviceFaultReportsKHR>(vkGetDeviceProcAddr(device, "vkGetDeviceFaultReportsKHR"));
      m_api = m_pfnGetDeviceFaultReportsKHR != nullptr ? VUDeviceFaultApi::Khr : VUDeviceFaultApi::Disabled;
#endif
      break;
    case VUDeviceFaultApi::Ext:
#ifdef VK_EXT_device_fault
      m_pfnGetDeviceFaultInfoEXT = reinterpret_cast<PFN_vkGetDeviceFaultInfoEXT>(vkGetDeviceProcAddr(device, "vkGetDeviceFaultInfoEXT"));
      m_api = m_pfnGetDeviceFaultInfoEXT != nullptr ? VUDeviceFaultApi::Ext : VUDeviceFaultApi::Disabled;
#endif
      break;
    case VUDeviceFaultApi::Disabled:
    default:
      break;
    }
    FSLLOG3_WARNING_IF(api != VUDeviceFaultApi::Disabled && m_api == VUDeviceFaultApi::Disabled,
                       "The device fault entry point was not found, was the device created with the extension enabled?");
  }


  VUDeviceFaultReport VUDeviceFault::GetReport([[maybe_unused]] const TimeSpan timeout) const
  {
    switch (m_api)
    {
#ifdef VK_KHR_device_fault
    case VUDeviceFaultApi::Khr:
      return GetReportKHR(m_pfnGetDeviceFaultReportsKHR, m_device, timeout);
#endif
#ifdef VK_EXT_device_fault
    case VUDeviceFaultApi::Ext:
      return GetReportEXT(m_pfnGetDeviceFaultInfoEXT, m_device);
#endif
    default:
      return {};
    }
  }


  uint32_t VUDeviceFault::LogFaults() const noexcept
  {
    if (!IsSupported())
    {
      return 0;
    }

    try
    {
      const VUDeviceFaultReport report = GetReport(LocalConfig::LogTimeout);
      if (report.Faults.empty())
      {
        FSLLOG3_ERROR("Vulkan device fault: the driver reported no faults ({})", RapidVulkan::Debug::ToString(report.Result));
        return 0;
      }
      for (const auto& fault : report.Faults)
      {
        FSLLOG3_ERROR("Vulkan device fault: {}", fault);
      }
      return static_cast<uint32_t>(report.Faults.size());
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_ERROR("Vulkan device fault: the query failed: {}", ex.what());
      return 0;
    }
  }
}
