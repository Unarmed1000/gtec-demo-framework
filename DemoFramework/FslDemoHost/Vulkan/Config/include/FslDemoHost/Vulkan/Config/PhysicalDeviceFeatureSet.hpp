#ifndef FSLDEMOHOST_VULKAN_CONFIG_PHYSICALDEVICEFEATURESET_HPP
#define FSLDEMOHOST_VULKAN_CONFIG_PHYSICALDEVICEFEATURESET_HPP
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
#include <FslUtil/Vulkan1_0/Common.hpp>
#include <FslUtil/Vulkan1_0/Util/StructUtil.hpp>
#include <vulkan/vulkan.h>

namespace Fsl::Vulkan
{
  //! @brief The Vulkan 1.0 features and the Vulkan 1.1, 1.2 and 1.3 core features of a physical device.
  //! @note The pNext of the 1.1+ structs is always nullptr, a pNext chain is only built where the structs are used.
  //!       This keeps the struct safe to copy.
  struct PhysicalDeviceFeatureSet
  {
    VkPhysicalDeviceFeatures Features{};
    VkPhysicalDeviceVulkan11Features Features11 =
      StructUtil::Create<VkPhysicalDeviceVulkan11Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
    VkPhysicalDeviceVulkan12Features Features12 =
      StructUtil::Create<VkPhysicalDeviceVulkan12Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
    VkPhysicalDeviceVulkan13Features Features13 =
      StructUtil::Create<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);

    //! @brief Check if any VkPhysicalDeviceVulkan11Features feature is enabled
    [[nodiscard]] bool HasEnabledFeatures11() const noexcept;
    //! @brief Check if any VkPhysicalDeviceVulkan12Features feature is enabled
    [[nodiscard]] bool HasEnabledFeatures12() const noexcept;
    //! @brief Check if any VkPhysicalDeviceVulkan13Features feature is enabled
    [[nodiscard]] bool HasEnabledFeatures13() const noexcept;

    //! @brief Query the features supported by the physical device (requires a Vulkan 1.3 instance and physical device)
    static PhysicalDeviceFeatureSet Query(const VkPhysicalDevice physicalDevice);
  };
}

#endif
