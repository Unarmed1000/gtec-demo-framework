#ifndef FSLDEMOHOST_VULKAN_CONFIG_HOSTDEVICEEXTENSIONS_HPP
#define FSLDEMOHOST_VULKAN_CONFIG_HOSTDEVICEEXTENSIONS_HPP
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

#include <FslDemoHost/Vulkan/Config/FeatureRequest.hpp>
#include <FslDemoHost/Vulkan/Config/OptionUserChoice.hpp>
#include <FslDemoHost/Vulkan/Config/VulkanHostDeviceFeatures.hpp>
#include <vulkan/vulkan.h>
#include <vector>

#if defined(VK_EXT_present_timing) && defined(VK_KHR_present_id2) && defined(VK_KHR_calibrated_timestamps) && \
  defined(VK_KHR_get_surface_capabilities2)
//! Defined when the Vulkan headers the framework is built with know the extensions needed for present timing
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FSL_VULKAN_HOST_PRESENT_TIMING_SUPPORTED 1
#endif

namespace Fsl::Vulkan
{
  //! Selects the optional device extensions the host wants (the ones the physical device supports) and owns the feature structs that enable
  //! them.
  //!
  //! The feature structs are part of the pNext chain given to the device create info, so this object must stay alive (and can not be moved) until
  //! the device has been created.
  class HostDeviceExtensions final
  {
#ifdef VK_KHR_device_fault
    VkPhysicalDeviceFaultFeaturesKHR m_faultFeaturesKHR{};
#endif
#ifdef VK_EXT_device_fault
    VkPhysicalDeviceFaultFeaturesEXT m_faultFeaturesEXT{};
#endif
#ifdef FSL_VULKAN_HOST_PRESENT_TIMING_SUPPORTED
    VkPhysicalDevicePresentId2FeaturesKHR m_presentId2Features{};
    VkPhysicalDevicePresentTimingFeaturesEXT m_presentTimingFeatures{};
#endif
#ifdef VK_KHR_present_mode_fifo_latest_ready
    VkPhysicalDevicePresentModeFifoLatestReadyFeaturesKHR m_fifoLatestReadyFeatures{};
#endif
    //! The first and last of the feature structs that are in use (null if none)
    VkBaseInStructure* m_pChain{nullptr};
    VkBaseInStructure* m_pChainTail{nullptr};
    VulkanHostDeviceFeatures m_features;

  public:
    HostDeviceExtensions(const HostDeviceExtensions&) = delete;
    HostDeviceExtensions& operator=(const HostDeviceExtensions&) = delete;
    ~HostDeviceExtensions() = default;

    //! @brief Examine what the physical device supports.
    //! @param rExtensionRequests the device extensions to enable are appended to this.
    //! @param presentTiming Off leaves the present timing extensions disabled.
    HostDeviceExtensions(const VkPhysicalDevice physicalDevice, std::vector<FeatureRequest>& rExtensionRequests,
                         const OptionUserChoice presentTiming = OptionUserChoice::Default);

    //! @brief Get the feature structs to add to the pNext chain of the device create info.
    //! @param pNext the chain to continue with after the feature structs (it becomes the tail of the returned chain).
    //! @return the head of the chain, which is pNext if no feature struct is needed.
    [[nodiscard]] VkBaseInStructure* LinkDeviceCreateInfoChain(VkBaseInStructure* const pNext) noexcept;

    //! @return what will be enabled on a device that is created with the extension requests and the feature structs.
    [[nodiscard]] const VulkanHostDeviceFeatures& GetFeatures() const noexcept
    {
      return m_features;
    }

  private:
    void SelectDeviceFault(const VkPhysicalDevice physicalDevice, std::vector<FeatureRequest>& rExtensionRequests);
    void SelectCalibratedTimestamps(const VkPhysicalDevice physicalDevice, std::vector<FeatureRequest>& rExtensionRequests);
    void SelectPresentTiming(const VkPhysicalDevice physicalDevice, std::vector<FeatureRequest>& rExtensionRequests);
    void SelectPresentModeFifoLatestReady(const VkPhysicalDevice physicalDevice, std::vector<FeatureRequest>& rExtensionRequests);

    template <typename TFeatureStruct>
    void PushFront(TFeatureStruct& rFeatures) noexcept
    {
      auto* const pFeatures = reinterpret_cast<VkBaseInStructure*>(&rFeatures);
      pFeatures->pNext = m_pChain;
      if (m_pChain == nullptr)
      {
        m_pChainTail = pFeatures;
      }
      m_pChain = pFeatures;
    }
  };
}

#endif
