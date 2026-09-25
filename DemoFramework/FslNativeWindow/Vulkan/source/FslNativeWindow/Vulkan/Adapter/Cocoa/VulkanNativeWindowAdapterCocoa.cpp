#if defined(FSL_WINDOWSYSTEM_COCOA)
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

#include "VulkanNativeWindowAdapterCocoa.hpp"
#include <FslNativeWindow/Vulkan/NativeVulkanSetup.hpp>
#include <RapidVulkan/Check.hpp>
#include "VulkanNativeWindowSystemAdapterCocoa.hpp"

namespace Fsl
{
  namespace
  {
    // MoltenVK presents through a CAMetalLayer
    const auto g_platformKhrSurfaceExtensionName = VK_EXT_METAL_SURFACE_EXTENSION_NAME;


    std::shared_ptr<IPlatformNativeWindowAdapter>
      AllocateWindow(const NativeWindowSetup& nativeWindowSetup, const PlatformNativeWindowParams& windowParams,
                     const PlatformNativeWindowAllocationParams* const pPlatformCustomWindowAllocationParams)
    {
      return std::make_shared<VulkanNativeWindowAdapterCocoa>(nativeWindowSetup, windowParams, pPlatformCustomWindowAllocationParams);
    }
  }    // namespace


  VulkanNativeWindowSystemAdapterCocoa::VulkanNativeWindowSystemAdapterCocoa(const NativeWindowSystemSetup& setup)
    : VulkanNativeWindowSystemAdapterTemplate<PlatformNativeWindowSystemAdapterCocoa>(setup, g_platformKhrSurfaceExtensionName, AllocateWindow)
  {
  }


  VulkanNativeWindowAdapterCocoa::VulkanNativeWindowAdapterCocoa(
    const NativeWindowSetup& nativeWindowSetup, const PlatformNativeWindowParams& windowParams,
    const PlatformNativeWindowAllocationParams* const pPlatformCustomWindowAllocationParams)
    : VulkanNativeWindowAdapter<PlatformNativeWindowAdapterCocoa>(nativeWindowSetup, windowParams, pPlatformCustomWindowAllocationParams)
  {
    VkMetalSurfaceCreateInfoEXT surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    surfaceCreateInfo.pNext = nullptr;
    surfaceCreateInfo.flags = 0;
    // Outside of Objective-C the Vulkan headers declare CAMetalLayer as void
    surfaceCreateInfo.pLayer = static_cast<const CAMetalLayer*>(GetPlatformWindow());

    RAPIDVULKAN_CHECK(vkCreateMetalSurfaceEXT(m_setup.Instance, &surfaceCreateInfo, nullptr, &m_surface));
  }


  VulkanNativeWindowAdapterCocoa::~VulkanNativeWindowAdapterCocoa() = default;


  PlatformNativeWindowType VulkanNativeWindowAdapterCocoa::GetWindowType() const
  {
    return GetPlatformWindow();
  }
}    // namespace Fsl
#endif
