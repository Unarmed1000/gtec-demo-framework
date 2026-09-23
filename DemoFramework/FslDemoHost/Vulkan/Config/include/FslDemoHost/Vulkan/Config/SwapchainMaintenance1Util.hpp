#ifndef FSLDEMOHOST_VULKAN_CONFIG_SWAPCHAINMAINTENANCE1UTIL_HPP
#define FSLDEMOHOST_VULKAN_CONFIG_SWAPCHAINMAINTENANCE1UTIL_HPP
/****************************************************************************************************************************************************
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the NXP. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslDemoHost/Vulkan/Config/FeatureRequest.hpp>
#include <vulkan/vulkan.h>
#include <deque>

// VK_KHR_swapchain_maintenance1 is a promotion of VK_EXT_swapchain_maintenance1 and the structure types share the same values,
// so the types below work with both extension names. Older headers might only contain the EXT version (or neither).
#if defined(VK_KHR_swapchain_maintenance1) || defined(VK_EXT_swapchain_maintenance1)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FSL_VULKAN_SWAPCHAIN_MAINTENANCE1_SUPPORTED 1
#endif

namespace Fsl::Vulkan::SwapchainMaintenance1Util
{
#if defined(VK_KHR_swapchain_maintenance1)
  using PhysicalDeviceSwapchainMaintenance1Features = VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR;
  using SwapchainPresentFenceInfo = VkSwapchainPresentFenceInfoKHR;
  constexpr VkStructureType PhysicalDeviceSwapchainMaintenance1FeaturesSType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;
  constexpr VkStructureType SwapchainPresentFenceInfoSType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR;
#elif defined(VK_EXT_swapchain_maintenance1)
  using PhysicalDeviceSwapchainMaintenance1Features = VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT;
  using SwapchainPresentFenceInfo = VkSwapchainPresentFenceInfoEXT;
  constexpr VkStructureType PhysicalDeviceSwapchainMaintenance1FeaturesSType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
  constexpr VkStructureType SwapchainPresentFenceInfoSType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
#endif

  //! Append the optional instance extensions that VK_KHR/EXT_swapchain_maintenance1 depends on.
  void AppendInstanceExtensionRequests(std::deque<FeatureRequest>& rExtensionRequests);

  //! Check if the instance extensions requested by AppendInstanceExtensionRequests are available and if the physical device supports the
  //! swapchainMaintenance1 feature.
  //! @return the name of the device extension to enable or nullptr if its unsupported.
  const char* TryGetDeviceExtensionName(const VkInstance instance, const VkPhysicalDevice physicalDevice);
}

#endif
