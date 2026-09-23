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

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslDemoHost/Vulkan/Config/SwapchainMaintenance1Util.hpp>
#include <FslUtil/Vulkan1_0/Util/InstanceUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceUtil.hpp>

namespace Fsl::Vulkan::SwapchainMaintenance1Util
{
#ifdef FSL_VULKAN_SWAPCHAIN_MAINTENANCE1_SUPPORTED
  namespace
  {
    namespace LocalConfig
    {
      // We use the plain names so we can select between the KHR and EXT versions no matter which one the header defines.
      constexpr const char* const GetPhysicalDeviceProperties2 = "VK_KHR_get_physical_device_properties2";
      constexpr const char* const GetSurfaceCapabilities2 = "VK_KHR_get_surface_capabilities2";
      constexpr const char* const SurfaceMaintenance1KHR = "VK_KHR_surface_maintenance1";
      constexpr const char* const SurfaceMaintenance1EXT = "VK_EXT_surface_maintenance1";
      constexpr const char* const SwapchainMaintenance1KHR = "VK_KHR_swapchain_maintenance1";
      constexpr const char* const SwapchainMaintenance1EXT = "VK_EXT_swapchain_maintenance1";
    }

    bool IsInstanceExtensionAvailable(const char* const pszExtensionName)
    {
      return InstanceUtil::IsInstanceExtensionsAvailable(1, &pszExtensionName);
    }

    bool IsDeviceExtensionAvailable(const VkPhysicalDevice physicalDevice, const char* const pszExtensionName)
    {
      return PhysicalDeviceUtil::IsDeviceExtensionsAvailable(physicalDevice, 1, &pszExtensionName);
    }

    //! The KHR and EXT versions have matching instance and device extensions
    const char* TryGetExtensionName(const VkPhysicalDevice physicalDevice)
    {
      if (IsInstanceExtensionAvailable(LocalConfig::SurfaceMaintenance1KHR) &&
          IsDeviceExtensionAvailable(physicalDevice, LocalConfig::SwapchainMaintenance1KHR))
      {
        return LocalConfig::SwapchainMaintenance1KHR;
      }
      if (IsInstanceExtensionAvailable(LocalConfig::SurfaceMaintenance1EXT) &&
          IsDeviceExtensionAvailable(physicalDevice, LocalConfig::SwapchainMaintenance1EXT))
      {
        return LocalConfig::SwapchainMaintenance1EXT;
      }
      return nullptr;
    }
  }


  void AppendInstanceExtensionRequests(std::deque<FeatureRequest>& rExtensionRequests)
  {
    rExtensionRequests.emplace_back(LocalConfig::GetPhysicalDeviceProperties2, FeatureRequirement::Optional);
    rExtensionRequests.emplace_back(LocalConfig::GetSurfaceCapabilities2, FeatureRequirement::Optional);
    rExtensionRequests.emplace_back(LocalConfig::SurfaceMaintenance1KHR, FeatureRequirement::Optional);
    rExtensionRequests.emplace_back(LocalConfig::SurfaceMaintenance1EXT, FeatureRequirement::Optional);
  }


  const char* TryGetDeviceExtensionName(const VkInstance instance, const VkPhysicalDevice physicalDevice)
  {
    // The instance extensions are requested as optional by AppendInstanceExtensionRequests, so if they are available they are enabled.
    if (instance == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE || !IsInstanceExtensionAvailable(LocalConfig::GetPhysicalDeviceProperties2) ||
        !IsInstanceExtensionAvailable(LocalConfig::GetSurfaceCapabilities2))
    {
      return nullptr;
    }

    const char* const pszExtensionName = TryGetExtensionName(physicalDevice);
    if (pszExtensionName == nullptr)
    {
      return nullptr;
    }

    // The extension being available does not guarantee that the feature is, so query it
    auto* const pfnGetPhysicalDeviceFeatures2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR"));
    if (pfnGetPhysicalDeviceFeatures2 == nullptr)
    {
      return nullptr;
    }

    PhysicalDeviceSwapchainMaintenance1Features swapchainMaintenance1Features{};
    swapchainMaintenance1Features.sType = PhysicalDeviceSwapchainMaintenance1FeaturesSType;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &swapchainMaintenance1Features;
    pfnGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    return swapchainMaintenance1Features.swapchainMaintenance1 == VK_TRUE ? pszExtensionName : nullptr;
  }
#else
  void AppendInstanceExtensionRequests(std::deque<FeatureRequest>& /*rExtensionRequests*/)
  {
  }


  const char* TryGetDeviceExtensionName(const VkInstance /*instance*/, const VkPhysicalDevice /*physicalDevice*/)
  {
    return nullptr;
  }
#endif
}
