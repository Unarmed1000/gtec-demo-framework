#ifndef VULKAN_SHADERCLOCK_SHADERCLOCKDEVICECUSTOMIZER_HPP
#define VULKAN_SHADERCLOCK_SHADERCLOCKDEVICECUSTOMIZER_HPP
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

#include <FslDemoHost/Vulkan/Config/IVulkanDeviceCreationCustomizer.hpp>
#include <FslUtil/Vulkan1_0/Util/InstanceUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceUtil.hpp>
#include <vulkan/vulkan.h>

namespace Fsl
{
  //! Enables the supported VK_KHR_shader_clock features (the extension itself is requested in ShaderClock_Register.cpp)
  class ShaderClockDeviceCustomizer final : public Vulkan::IVulkanDeviceCreationCustomizer
  {
    VkPhysicalDeviceShaderClockFeaturesKHR m_features{};
    bool m_enabled{false};

  public:
    void Configure(const VkInstance instance, const VkPhysicalDevice physicalDevice) final
    {
      m_features = {};
      m_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;
      m_enabled = false;

      // Both extensions are requested as optional, so if they are available they are enabled
      const char* const pszInstanceExtension = "VK_KHR_get_physical_device_properties2";
      const char* const pszDeviceExtension = "VK_KHR_shader_clock";
      if (!Vulkan::InstanceUtil::IsInstanceExtensionsAvailable(1, &pszInstanceExtension) ||
          !Vulkan::PhysicalDeviceUtil::IsDeviceExtensionsAvailable(physicalDevice, 1, &pszDeviceExtension))
      {
        return;
      }

      auto* const pfnGetPhysicalDeviceFeatures2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR"));
      if (pfnGetPhysicalDeviceFeatures2 == nullptr)
      {
        return;
      }

      VkPhysicalDeviceFeatures2 features2{};
      features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
      features2.pNext = &m_features;
      pfnGetPhysicalDeviceFeatures2(physicalDevice, &features2);
      m_features.pNext = nullptr;

      // Enable exactly the supported features
      m_enabled = m_features.shaderSubgroupClock == VK_TRUE || m_features.shaderDeviceClock == VK_TRUE;
    }

    [[nodiscard]] const void* GetVkDeviceCreateInfoNextPointer() const final
    {
      return m_enabled ? &m_features : nullptr;
    }

    [[nodiscard]] bool IsDeviceClockEnabled() const noexcept
    {
      return m_enabled && m_features.shaderDeviceClock == VK_TRUE;
    }
  };
}

#endif
