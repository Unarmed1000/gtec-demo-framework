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
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceUtil.hpp>
#include <stdexcept>

namespace Fsl::Vulkan
{
  namespace
  {
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
  }


  HostDeviceExtensions::HostDeviceExtensions(const VkPhysicalDevice physicalDevice, [[maybe_unused]] std::vector<FeatureRequest>& rExtensionRequests)
  {
    if (physicalDevice == VK_NULL_HANDLE)
    {
      throw std::invalid_argument("physicalDevice can not be VK_NULL_HANDLE");
    }

    // Device fault: lets the driver explain why a device was lost. The KHR extension replaces the EXT one, so it is preferred.
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

    FSLLOG3_VERBOSE("Device fault: {}", m_features.DeviceFault == VUDeviceFaultApi::Khr
                                          ? "VK_KHR_device_fault"
                                          : (m_features.DeviceFault == VUDeviceFaultApi::Ext ? "VK_EXT_device_fault" : "unsupported"));
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
}
