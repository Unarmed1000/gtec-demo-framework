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

#include <FslDemoHost/Vulkan/Config/PhysicalDeviceFeatureSet.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Fsl::Vulkan
{
  namespace
  {
    //! The feature structs are a sType, a pNext and then only VkBool32 members, this checks if any of the members are VK_TRUE
    template <typename TFeatureStruct>
    bool HasEnabledFeatures(const TFeatureStruct& features, const std::size_t firstMemberOffset, const std::size_t memberCount) noexcept
    {
      const auto* const pFirst = reinterpret_cast<const uint8_t*>(&features) + firstMemberOffset;
      for (std::size_t i = 0; i < memberCount; ++i)
      {
        VkBool32 value = VK_FALSE;
        std::memcpy(&value, pFirst + (i * sizeof(VkBool32)), sizeof(VkBool32));
        if (value != VK_FALSE)
        {
          return true;
        }
      }
      return false;
    }

    namespace Layout11
    {
      constexpr std::size_t FirstOffset = offsetof(VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess);
      constexpr std::size_t LastOffset = offsetof(VkPhysicalDeviceVulkan11Features, shaderDrawParameters);
      constexpr std::size_t Count = 12;
      static_assert(LastOffset == FirstOffset + ((Count - 1) * sizeof(VkBool32)), "VkPhysicalDeviceVulkan11Features layout changed");
      static_assert(sizeof(VkPhysicalDeviceVulkan11Features) - (LastOffset + sizeof(VkBool32)) < alignof(VkPhysicalDeviceVulkan11Features),
                    "VkPhysicalDeviceVulkan11Features has new members");
    }

    namespace Layout12
    {
      constexpr std::size_t FirstOffset = offsetof(VkPhysicalDeviceVulkan12Features, samplerMirrorClampToEdge);
      constexpr std::size_t LastOffset = offsetof(VkPhysicalDeviceVulkan12Features, subgroupBroadcastDynamicId);
      constexpr std::size_t Count = 47;
      static_assert(LastOffset == FirstOffset + ((Count - 1) * sizeof(VkBool32)), "VkPhysicalDeviceVulkan12Features layout changed");
      static_assert(sizeof(VkPhysicalDeviceVulkan12Features) - (LastOffset + sizeof(VkBool32)) < alignof(VkPhysicalDeviceVulkan12Features),
                    "VkPhysicalDeviceVulkan12Features has new members");
    }

    namespace Layout13
    {
      constexpr std::size_t FirstOffset = offsetof(VkPhysicalDeviceVulkan13Features, robustImageAccess);
      constexpr std::size_t LastOffset = offsetof(VkPhysicalDeviceVulkan13Features, maintenance4);
      constexpr std::size_t Count = 15;
      static_assert(LastOffset == FirstOffset + ((Count - 1) * sizeof(VkBool32)), "VkPhysicalDeviceVulkan13Features layout changed");
      static_assert(sizeof(VkPhysicalDeviceVulkan13Features) - (LastOffset + sizeof(VkBool32)) < alignof(VkPhysicalDeviceVulkan13Features),
                    "VkPhysicalDeviceVulkan13Features has new members");
    }
  }


  bool PhysicalDeviceFeatureSet::HasEnabledFeatures11() const noexcept
  {
    return HasEnabledFeatures(Features11, Layout11::FirstOffset, Layout11::Count);
  }


  bool PhysicalDeviceFeatureSet::HasEnabledFeatures12() const noexcept
  {
    return HasEnabledFeatures(Features12, Layout12::FirstOffset, Layout12::Count);
  }


  bool PhysicalDeviceFeatureSet::HasEnabledFeatures13() const noexcept
  {
    return HasEnabledFeatures(Features13, Layout13::FirstOffset, Layout13::Count);
  }


  PhysicalDeviceFeatureSet PhysicalDeviceFeatureSet::Query(const VkPhysicalDevice physicalDevice)
  {
    PhysicalDeviceFeatureSet result;
    result.Features11.pNext = &result.Features12;
    result.Features12.pNext = &result.Features13;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &result.Features11;
    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    result.Features = features2.features;
    result.Features11.pNext = nullptr;
    result.Features12.pNext = nullptr;
    result.Features13.pNext = nullptr;
    return result;
  }
}
