#ifndef FSLUTIL_VULKAN1_0_VUSWAPCHAINPRESENTWAIT_HPP
#define FSLUTIL_VULKAN1_0_VUSWAPCHAINPRESENTWAIT_HPP
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
#include <vulkan/vulkan.h>
#include <cstdint>

#if defined(VK_KHR_present_wait2) && defined(VK_KHR_present_id2) && defined(VK_KHR_get_surface_capabilities2)
//! Defined when the Vulkan headers the framework is built with know the extensions needed to wait for a present
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FSL_VULKAN_PRESENT_WAIT_SUPPORTED 1
#endif

namespace Fsl::Vulkan
{
  //! The struct that is chained to the VkPresentInfoKHR of one present, filled by VUSwapchainPresentWait::PreparePresent.
  struct VUPresentWaitPresentInfo
  {
#ifdef FSL_VULKAN_PRESENT_WAIT_SUPPORTED
    uint64_t Id{0};
    VkPresentId2KHR PresentId{};
#endif
  };


  //! Waits until a present of a swapchain was presented, with VK_KHR_present_wait2 and VK_KHR_present_id2.
  //!
  //! A fence or a semaphore of a frame says when the GPU is done with the frame. This says when the presentation engine began to present
  //! it, which is what a frame loop needs to know to not get ahead of the display. When that is in relation to the image being on the
  //! display is up to the window system, so it has to be measured before it is relied on.
  //!
  //! Use:
  //! - create the swapchain with the flags from GetSwapchainCreateFlags (zero means the surface does not support it),
  //! - call Reset with the new swapchain, and Reset() before the swapchain is destroyed,
  //! - give every present of the swapchain a id: chain what PreparePresent returns to its VkPresentInfoKHR, unless something else gives
  //!   the present a VkPresentId2KHR already (VUSwapchainPresentTiming does),
  //! - call Wait with the id of a present that vkQueuePresentKHR accepted.
  //!
  //! The device must have been created with the two extensions and their presentId2 and presentWait2 features enabled.
  class VUSwapchainPresentWait final
  {
    VkDevice m_device{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
#ifdef FSL_VULKAN_PRESENT_WAIT_SUPPORTED
    PFN_vkWaitForPresent2KHR m_pfnWaitForPresent{nullptr};
#endif

  public:
    VUSwapchainPresentWait() = default;

    //! @brief Get the swapchain create flags that are needed to wait for the presents of a swapchain of the surface.
    //! @return zero if the surface does not support it (or the Vulkan headers this was built with lack the extensions).
    static VkSwapchainCreateFlagsKHR GetSwapchainCreateFlags(const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface);

    //! @brief Call it before the swapchain is destroyed.
    void Reset() noexcept;

    //! @brief Use the swapchain.
    //! @param swapchain a swapchain that was created with the flags returned by GetSwapchainCreateFlags (which must not be zero).
    //! @return true if its presents can be waited for.
    bool Reset(const VkDevice device, const VkSwapchainKHR swapchain);

    //! @return true if the presents of the swapchain can be waited for.
    [[nodiscard]] bool IsEnabled() const noexcept
    {
      return m_swapchain != VK_NULL_HANDLE;
    }

    //! @brief Give a present its id.
    //! @param rPresentInfo the storage of the struct, it must stay alive until the present was queued.
    //! @param presentId the id of the present, it must be greater than the id of the previous present of the swapchain.
    //! @param pNext the pNext chain to continue with.
    //! @return the pNext to give to VkPresentInfoKHR (pNext if not enabled).
    [[nodiscard]] const void* PreparePresent(VUPresentWaitPresentInfo& rPresentInfo, const uint64_t presentId, const void* const pNext) noexcept;

    //! @brief Wait until the presentation engine began to present the present with the given id, or replaced it without presenting it.
    //! @param presentId the id of a present of the swapchain that vkQueuePresentKHR accepted.
    //! @param timeoutNanoseconds how long to wait at the most.
    //! @return VK_SUCCESS if it was presented, VK_TIMEOUT if it was not within the time, VK_ERROR_FEATURE_NOT_PRESENT if not enabled, else
    //!         what vkWaitForPresent2KHR returned (the swapchain can be out of date, the surface or the device lost).
    [[nodiscard]] VkResult Wait(const uint64_t presentId, const uint64_t timeoutNanoseconds) const noexcept;
  };
}

#endif
