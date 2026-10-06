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
#include <FslDemoHost/Vulkan/Config/HostDeviceExtensions.hpp>
#include <FslUtil/Vulkan1_0/Util/InstanceUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceUtil.hpp>
#include <stdexcept>

namespace Fsl::Vulkan
{
  namespace
  {
    namespace LocalConfig
    {
      // The plain names are used so the EXT version can be selected no matter which one the header defines
      constexpr auto CalibratedTimestampsKHR = "VK_KHR_calibrated_timestamps";
      constexpr auto CalibratedTimestampsEXT = "VK_EXT_calibrated_timestamps";
    }

    [[maybe_unused]] bool IsDeviceExtensionAvailable(const VkPhysicalDevice physicalDevice, const char* const pszExtensionName)
    {
      return PhysicalDeviceUtil::IsDeviceExtensionsAvailable(physicalDevice, 1, &pszExtensionName);
    }

    //! Query the feature struct of a extension the physical device supports
    template <typename TFeatureStruct>
    [[maybe_unused]] TFeatureStruct QueryFeatures(const VkPhysicalDevice physicalDevice, const VkStructureType featureStructType)
    {
      TFeatureStruct features{};
      features.sType = featureStructType;

      VkPhysicalDeviceFeatures2 features2{};
      features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
      features2.pNext = &features;
      vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
      return features;
    }

    bool HasRequest(const std::vector<FeatureRequest>& requests, const char* const pszExtensionName)
    {
      for (const auto& request : requests)
      {
        if (request.Name == pszExtensionName)
        {
          return true;
        }
      }
      return false;
    }
  }


  HostDeviceExtensions::HostDeviceExtensions(const VkPhysicalDevice physicalDevice, std::vector<FeatureRequest>& rExtensionRequests,
                                             const OptionUserChoice presentTiming, const bool presentWait)
  {
    if (physicalDevice == VK_NULL_HANDLE)
    {
      throw std::invalid_argument("physicalDevice can not be VK_NULL_HANDLE");
    }

    SelectDeviceFault(physicalDevice, rExtensionRequests);
    SelectCalibratedTimestamps(physicalDevice, rExtensionRequests);
    SelectPresentModeFifoLatestReady(physicalDevice, rExtensionRequests);
    if (presentTiming != OptionUserChoice::Off)
    {
      SelectPresentTiming(physicalDevice, rExtensionRequests);
      FSLLOG3_WARNING_IF(presentTiming == OptionUserChoice::On && !m_features.PresentTiming, "Present timing was requested but is unsupported");
    }
    else
    {
      FSLLOG3_INFO("Present timing: disabled by user");
    }
    if (presentWait)
    {
      // After present timing, which enables the present ids as well
      SelectPresentWait(physicalDevice, rExtensionRequests);
      FSLLOG3_WARNING_IF(!m_features.PresentWait, "Present wait was requested but is unsupported (it needs VK_KHR_present_wait2)");
    }

    FSLLOG3_VERBOSE("Device fault: {}", m_features.DeviceFault == VUDeviceFaultApi::Khr
                                          ? "VK_KHR_device_fault"
                                          : (m_features.DeviceFault == VUDeviceFaultApi::Ext ? "VK_EXT_device_fault" : "unsupported"));
    FSLLOG3_VERBOSE("Calibrated timestamps: {}", m_features.CalibratedTimestamps ? "supported" : "unsupported");
    FSLLOG3_VERBOSE("Present timing: {}", m_features.PresentTiming ? "supported by the device" : "unsupported");
    FSLLOG3_VERBOSE_IF(presentWait, "Present wait: {}", m_features.PresentWait ? "supported by the device" : "unsupported");
    FSLLOG3_VERBOSE("Present mode FIFO latest ready: {}", m_features.PresentModeFifoLatestReady ? "supported by the device" : "unsupported");
  }


  VkBaseInStructure* HostDeviceExtensions::LinkDeviceCreateInfoChain(VkBaseInStructure* const pNext) noexcept
  {
    if (m_pChain == nullptr)
    {
      return pNext;
    }

    // The supplied chain continues after the last of our structs
    m_pChainTail->pNext = pNext;
    return m_pChain;
  }


  // Device fault: lets the driver explain why a device was lost. The KHR extension replaces the EXT one, so it is preferred.
  void HostDeviceExtensions::SelectDeviceFault([[maybe_unused]] const VkPhysicalDevice physicalDevice,
                                               [[maybe_unused]] std::vector<FeatureRequest>& rExtensionRequests)
  {
#ifdef VK_KHR_device_fault
    if (IsDeviceExtensionAvailable(physicalDevice, VK_KHR_DEVICE_FAULT_EXTENSION_NAME) &&
        QueryFeatures<VkPhysicalDeviceFaultFeaturesKHR>(physicalDevice, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_KHR).deviceFault == VK_TRUE)
    {
      rExtensionRequests.emplace_back(VK_KHR_DEVICE_FAULT_EXTENSION_NAME, FeatureRequirement::Mandatory);
      m_faultFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_KHR;
      m_faultFeaturesKHR.deviceFault = VK_TRUE;
      PushFront(m_faultFeaturesKHR);
      m_features.DeviceFault = VUDeviceFaultApi::Khr;
    }
#endif
#ifdef VK_EXT_device_fault
    if (m_features.DeviceFault == VUDeviceFaultApi::Disabled && IsDeviceExtensionAvailable(physicalDevice, VK_EXT_DEVICE_FAULT_EXTENSION_NAME) &&
        QueryFeatures<VkPhysicalDeviceFaultFeaturesEXT>(physicalDevice, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT).deviceFault == VK_TRUE)
    {
      rExtensionRequests.emplace_back(VK_EXT_DEVICE_FAULT_EXTENSION_NAME, FeatureRequirement::Mandatory);
      m_faultFeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT;
      m_faultFeaturesEXT.deviceFault = VK_TRUE;
      PushFront(m_faultFeaturesEXT);
      m_features.DeviceFault = VUDeviceFaultApi::Ext;
    }
#endif
  }


  // Calibrated timestamps: relates the clock of the device to the clock of the host. The extension has no feature struct.
  void HostDeviceExtensions::SelectCalibratedTimestamps(const VkPhysicalDevice physicalDevice, std::vector<FeatureRequest>& rExtensionRequests)
  {
    // The extension has two names. The one that is enabled is remembered, as the entry points of the device are the ones of that name.
    if (IsDeviceExtensionAvailable(physicalDevice, LocalConfig::CalibratedTimestampsKHR))
    {
      rExtensionRequests.emplace_back(LocalConfig::CalibratedTimestampsKHR, FeatureRequirement::Mandatory);
      m_features.CalibratedTimestamps = true;
      m_features.CalibratedTimestampsApi = VUCalibratedTimestampsApi::Khr;
    }
    else if (IsDeviceExtensionAvailable(physicalDevice, LocalConfig::CalibratedTimestampsEXT))
    {
      rExtensionRequests.emplace_back(LocalConfig::CalibratedTimestampsEXT, FeatureRequirement::Mandatory);
      m_features.CalibratedTimestamps = true;
      m_features.CalibratedTimestampsApi = VUCalibratedTimestampsApi::Ext;
    }
  }


  // The FIFO latest ready present mode: at a refresh the newest image that is ready is shown and the ones before it are dropped.
  // It is only enabled, a swapchain uses it when the app or the user asks for the present mode.
  void HostDeviceExtensions::SelectPresentModeFifoLatestReady([[maybe_unused]] const VkPhysicalDevice physicalDevice,
                                                              [[maybe_unused]] std::vector<FeatureRequest>& rExtensionRequests)
  {
#ifdef VK_KHR_present_mode_fifo_latest_ready
    // The EXT version was promoted to KHR without changes, so its name is the only difference
    const char* pszExtensionName = nullptr;
    if (IsDeviceExtensionAvailable(physicalDevice, VK_KHR_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME))
    {
      pszExtensionName = VK_KHR_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME;
    }
#ifdef VK_EXT_present_mode_fifo_latest_ready
    else if (IsDeviceExtensionAvailable(physicalDevice, VK_EXT_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME))
    {
      pszExtensionName = VK_EXT_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME;
    }
#endif
    if (pszExtensionName == nullptr || QueryFeatures<VkPhysicalDevicePresentModeFifoLatestReadyFeaturesKHR>(
                                         physicalDevice, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR)
                                           .presentModeFifoLatestReady != VK_TRUE)
    {
      return;
    }

    rExtensionRequests.emplace_back(pszExtensionName, FeatureRequirement::Mandatory);
    m_fifoLatestReadyFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR;
    m_fifoLatestReadyFeatures.presentModeFifoLatestReady = VK_TRUE;
    PushFront(m_fifoLatestReadyFeatures);
    m_features.PresentModeFifoLatestReady = true;
#endif
  }


  // Present timing: lets a swapchain report when its images were presented.
  void HostDeviceExtensions::SelectPresentTiming([[maybe_unused]] const VkPhysicalDevice physicalDevice,
                                                 [[maybe_unused]] std::vector<FeatureRequest>& rExtensionRequests)
  {
#ifdef FSL_VULKAN_HOST_PRESENT_TIMING_SUPPORTED
    // The instance extension is requested as optional by the host, so if it is available it is enabled.
    const char* const pszSurfaceCapabilities2 = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
    if (!InstanceUtil::IsInstanceExtensionsAvailable(1, &pszSurfaceCapabilities2))
    {
      return;
    }
    // It depends on the KHR version of calibrated timestamps and on present id 2
    if (!HasRequest(rExtensionRequests, VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME) ||
        !IsDeviceExtensionAvailable(physicalDevice, VK_KHR_PRESENT_ID_2_EXTENSION_NAME) ||
        !IsDeviceExtensionAvailable(physicalDevice, VK_EXT_PRESENT_TIMING_EXTENSION_NAME))
    {
      return;
    }
    if (QueryFeatures<VkPhysicalDevicePresentId2FeaturesKHR>(physicalDevice, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_2_FEATURES_KHR)
            .presentId2 != VK_TRUE ||
        QueryFeatures<VkPhysicalDevicePresentTimingFeaturesEXT>(physicalDevice, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT)
            .presentTiming != VK_TRUE)
    {
      return;
    }

    rExtensionRequests.emplace_back(VK_KHR_PRESENT_ID_2_EXTENSION_NAME, FeatureRequirement::Mandatory);
    rExtensionRequests.emplace_back(VK_EXT_PRESENT_TIMING_EXTENSION_NAME, FeatureRequirement::Mandatory);

    m_presentId2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_2_FEATURES_KHR;
    m_presentId2Features.presentId2 = VK_TRUE;
    PushFront(m_presentId2Features);

    // The feedback is always enabled. Of the scheduling of presents the relative form is enabled where the device has it (a present a
    // time after the one before it), the absolute form (presentAtAbsoluteTime) is not used.
    m_presentTimingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT;
    m_presentTimingFeatures.presentTiming = VK_TRUE;
    if (QueryFeatures<VkPhysicalDevicePresentTimingFeaturesEXT>(physicalDevice, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT)
          .presentAtRelativeTime == VK_TRUE)
    {
      m_presentTimingFeatures.presentAtRelativeTime = VK_TRUE;
      m_features.PresentAtRelativeTime = true;
    }
    PushFront(m_presentTimingFeatures);

    m_features.PresentTiming = true;
#endif
  }


  // Present wait: lets a app wait until a present of a swapchain was presented.
  void HostDeviceExtensions::SelectPresentWait([[maybe_unused]] const VkPhysicalDevice physicalDevice,
                                               [[maybe_unused]] std::vector<FeatureRequest>& rExtensionRequests)
  {
#ifdef FSL_VULKAN_HOST_PRESENT_WAIT_SUPPORTED
    // The instance extension is requested as optional by the host, so if it is available it is enabled.
    const char* const pszSurfaceCapabilities2 = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
    if (!InstanceUtil::IsInstanceExtensionsAvailable(1, &pszSurfaceCapabilities2))
    {
      return;
    }
    // It depends on present id 2. The older VK_KHR_present_wait (with VK_KHR_present_id) is another extension with entry points and
    // structs of its own and is not used.
    if (!IsDeviceExtensionAvailable(physicalDevice, VK_KHR_PRESENT_ID_2_EXTENSION_NAME) ||
        !IsDeviceExtensionAvailable(physicalDevice, VK_KHR_PRESENT_WAIT_2_EXTENSION_NAME))
    {
      return;
    }
    if (QueryFeatures<VkPhysicalDevicePresentId2FeaturesKHR>(physicalDevice, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_2_FEATURES_KHR)
            .presentId2 != VK_TRUE ||
        QueryFeatures<VkPhysicalDevicePresentWait2FeaturesKHR>(physicalDevice, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_2_FEATURES_KHR)
            .presentWait2 != VK_TRUE)
    {
      return;
    }

    if (!HasRequest(rExtensionRequests, VK_KHR_PRESENT_ID_2_EXTENSION_NAME))
    {
      // Present timing did not enable the present ids
      rExtensionRequests.emplace_back(VK_KHR_PRESENT_ID_2_EXTENSION_NAME, FeatureRequirement::Mandatory);
      m_presentId2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_2_FEATURES_KHR;
      m_presentId2Features.presentId2 = VK_TRUE;
      PushFront(m_presentId2Features);
    }
    rExtensionRequests.emplace_back(VK_KHR_PRESENT_WAIT_2_EXTENSION_NAME, FeatureRequirement::Mandatory);
    m_presentWait2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_2_FEATURES_KHR;
    m_presentWait2Features.presentWait2 = VK_TRUE;
    PushFront(m_presentWait2Features);

    m_features.PresentWait = true;
#endif
  }
}
