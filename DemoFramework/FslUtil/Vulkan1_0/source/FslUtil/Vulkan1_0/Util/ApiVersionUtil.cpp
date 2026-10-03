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

#include <FslBase/Exceptions.hpp>
#include <FslUtil/Vulkan1_0/Util/ApiVersionUtil.hpp>
#include <fmt/format.h>

namespace Fsl::Vulkan::ApiVersionUtil
{
  uint32_t GetLoaderApiVersion()
  {
    // vkEnumerateInstanceVersion was added in Vulkan 1.1, so look it up to support reporting a error on a 1.0 loader
    auto* const pfnEnumerateInstanceVersion =
      reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (pfnEnumerateInstanceVersion == nullptr)
    {
      return VK_API_VERSION_1_0;
    }
    uint32_t apiVersion = VK_API_VERSION_1_0;
    if (pfnEnumerateInstanceVersion(&apiVersion) != VK_SUCCESS)
    {
      return VK_API_VERSION_1_0;
    }
    return apiVersion;
  }


  void CheckLoader()
  {
    const uint32_t loaderApiVersion = GetLoaderApiVersion();
    if (loaderApiVersion < MinimumApiVersion)
    {
      throw NotSupportedException(fmt::format("The Vulkan loader supports api version {}.{}, but {}.{} or newer is required",
                                              VK_API_VERSION_MAJOR(loaderApiVersion), VK_API_VERSION_MINOR(loaderApiVersion),
                                              VK_API_VERSION_MAJOR(MinimumApiVersion), VK_API_VERSION_MINOR(MinimumApiVersion)));
    }
  }


  bool IsSupported(const VkPhysicalDeviceProperties& properties) noexcept
  {
    return properties.apiVersion >= MinimumApiVersion;
  }


  void CheckPhysicalDevice(const VkPhysicalDeviceProperties& properties)
  {
    if (!IsSupported(properties))
    {
      throw NotSupportedException(fmt::format("The Vulkan physical device '{}' supports api version {}.{}, but {}.{} or newer is required",
                                              properties.deviceName, VK_API_VERSION_MAJOR(properties.apiVersion),
                                              VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_MAJOR(MinimumApiVersion),
                                              VK_API_VERSION_MINOR(MinimumApiVersion)));
    }
  }
}
