#ifndef FSLUTIL_VULKAN1_0_DEBUG_VUDEVICEFAULT_HPP
#define FSLUTIL_VULKAN1_0_DEBUG_VUDEVICEFAULT_HPP
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
#include <FslBase/Time/TimeSpan.hpp>
#include <FslUtil/Vulkan1_0/Common.hpp>
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace Fsl::Vulkan
{
  //! The device fault extension that is enabled on a device. The two have different entry points and rules.
  enum class VUDeviceFaultApi
  {
    //! No device fault extension is enabled
    Disabled,
    //! VK_EXT_device_fault. Its faults can only be queried once the device is lost.
    Ext,
    //! VK_KHR_device_fault. Its faults can be queried at any time.
    Khr
  };


  //! The faults a device reported
  struct VUDeviceFaultReport
  {
    //! The result of the query (VK_ERROR_EXTENSION_NOT_PRESENT if no device fault extension is enabled)
    VkResult Result{VK_ERROR_EXTENSION_NOT_PRESENT};
    //! A description of each fault
    std::vector<std::string> Faults;
  };


  //! Queries the faults of a device with VK_KHR_device_fault or VK_EXT_device_fault, which is how a driver explains why a device was lost.
  //! The extension and its deviceFault feature must have been enabled when the device was created.
  class VUDeviceFault final
  {
    VkDevice m_device{VK_NULL_HANDLE};
    VUDeviceFaultApi m_api{VUDeviceFaultApi::Disabled};
#ifdef VK_KHR_device_fault
    PFN_vkGetDeviceFaultReportsKHR m_pfnGetDeviceFaultReportsKHR{nullptr};
#endif
#ifdef VK_EXT_device_fault
    PFN_vkGetDeviceFaultInfoEXT m_pfnGetDeviceFaultInfoEXT{nullptr};
#endif

  public:
    VUDeviceFault() = default;

    //! @param device the device to query (it must outlive this object).
    //! @param api the device fault extension that was enabled on the device.
    VUDeviceFault(const VkDevice device, const VUDeviceFaultApi api);

    //! @return the api that is used for the queries (Disabled if api was Disabled or its entry point was not found).
    [[nodiscard]] VUDeviceFaultApi GetApi() const noexcept
    {
      return m_api;
    }

    [[nodiscard]] bool IsSupported() const noexcept
    {
      return m_api != VUDeviceFaultApi::Disabled;
    }

    //! @brief Query the faults of the device.
    //! @param timeout how long to wait for the fault reports to become available (only used by VUDeviceFaultApi::Khr).
    //! @note  With VUDeviceFaultApi::Ext this must only be called once the device is lost (VUID-vkGetDeviceFaultInfoEXT-device-07336).
    [[nodiscard]] VUDeviceFaultReport GetReport(const TimeSpan timeout) const;

    //! @brief Write the faults of the device to the log, call it when a Vulkan function returned VK_ERROR_DEVICE_LOST.
    //! @return the number of faults that were logged.
    uint32_t LogFaults() const noexcept;
  };
}

#endif
