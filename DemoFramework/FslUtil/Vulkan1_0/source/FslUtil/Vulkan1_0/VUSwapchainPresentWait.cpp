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
#include <FslUtil/Vulkan1_0/VUSwapchainPresentWait.hpp>
#include <stdexcept>

namespace Fsl::Vulkan
{
#ifdef FSL_VULKAN_PRESENT_WAIT_SUPPORTED

  VkSwapchainCreateFlagsKHR VUSwapchainPresentWait::GetSwapchainCreateFlags(const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface)
  {
    if (physicalDevice == VK_NULL_HANDLE || surface == VK_NULL_HANDLE)
    {
      return 0;
    }

    VkSurfaceCapabilitiesPresentWait2KHR presentWait2Capabilities{};
    presentWait2Capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_WAIT_2_KHR;

    VkSurfaceCapabilitiesPresentId2KHR presentId2Capabilities{};
    presentId2Capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_ID_2_KHR;
    presentId2Capabilities.pNext = &presentWait2Capabilities;

    VkSurfaceCapabilities2KHR surfaceCapabilities{};
    surfaceCapabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
    surfaceCapabilities.pNext = &presentId2Capabilities;

    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
    surfaceInfo.surface = surface;

    if (vkGetPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &surfaceInfo, &surfaceCapabilities) != VK_SUCCESS)
    {
      return 0;
    }

    FSLLOG3_VERBOSE2("Present wait: surface capabilities presentId2 {} presentWait2 {}", presentId2Capabilities.presentId2Supported == VK_TRUE,
                     presentWait2Capabilities.presentWait2Supported == VK_TRUE);
    return (presentId2Capabilities.presentId2Supported == VK_TRUE && presentWait2Capabilities.presentWait2Supported == VK_TRUE)
             ? (VK_SWAPCHAIN_CREATE_PRESENT_ID_2_BIT_KHR | VK_SWAPCHAIN_CREATE_PRESENT_WAIT_2_BIT_KHR)
             : 0u;
  }


  void VUSwapchainPresentWait::Reset() noexcept
  {
    m_swapchain = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_pfnWaitForPresent = nullptr;
  }


  bool VUSwapchainPresentWait::Reset(const VkDevice device, const VkSwapchainKHR swapchain)
  {
    Reset();
    if (device == VK_NULL_HANDLE || swapchain == VK_NULL_HANDLE)
    {
      throw std::invalid_argument("device and swapchain can not be VK_NULL_HANDLE");
    }

    // The loader does not export the entry point of the extension, so it is looked up. It is the one of the extension the device was
    // created with: the older VK_KHR_present_wait has a entry point of its own, which is not looked for.
    m_pfnWaitForPresent = reinterpret_cast<PFN_vkWaitForPresent2KHR>(vkGetDeviceProcAddr(device, "vkWaitForPresent2KHR"));
    if (m_pfnWaitForPresent == nullptr)
    {
      FSLLOG3_WARNING("vkWaitForPresent2KHR was not found, was the device created with VK_KHR_present_wait2 enabled?");
      return false;
    }
    m_device = device;
    m_swapchain = swapchain;
    return true;
  }


  const void* VUSwapchainPresentWait::PreparePresent(VUPresentWaitPresentInfo& rPresentInfo, const uint64_t presentId,
                                                     const void* const pNext) noexcept
  {
    rPresentInfo = {};
    if (!IsEnabled())
    {
      return pNext;
    }
    rPresentInfo.Id = presentId;
    rPresentInfo.PresentId.sType = VK_STRUCTURE_TYPE_PRESENT_ID_2_KHR;
    rPresentInfo.PresentId.pNext = pNext;
    rPresentInfo.PresentId.swapchainCount = 1;
    rPresentInfo.PresentId.pPresentIds = &rPresentInfo.Id;
    return &rPresentInfo.PresentId;
  }


  VkResult VUSwapchainPresentWait::Wait(const uint64_t presentId, const uint64_t timeoutNanoseconds) const noexcept
  {
    if (!IsEnabled())
    {
      return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    VkPresentWait2InfoKHR waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_PRESENT_WAIT_2_INFO_KHR;
    waitInfo.presentId = presentId;
    waitInfo.timeout = timeoutNanoseconds;
    return m_pfnWaitForPresent(m_device, m_swapchain, &waitInfo);
  }

#else

  VkSwapchainCreateFlagsKHR VUSwapchainPresentWait::GetSwapchainCreateFlags(const VkPhysicalDevice /*physicalDevice*/, const VkSurfaceKHR /*surface*/)
  {
    return 0;
  }


  void VUSwapchainPresentWait::Reset() noexcept
  {
    m_swapchain = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
  }


  bool VUSwapchainPresentWait::Reset(const VkDevice /*device*/, const VkSwapchainKHR /*swapchain*/)
  {
    Reset();
    return false;
  }


  const void* VUSwapchainPresentWait::PreparePresent(VUPresentWaitPresentInfo& /*rPresentInfo*/, const uint64_t /*presentId*/,
                                                     const void* const pNext) noexcept
  {
    return pNext;
  }


  VkResult VUSwapchainPresentWait::Wait(const uint64_t /*presentId*/, const uint64_t /*timeoutNanoseconds*/) const noexcept
  {
    return VK_ERROR_FEATURE_NOT_PRESENT;
  }

#endif
}
