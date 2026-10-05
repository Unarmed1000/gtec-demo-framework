#ifndef FSLDEMOAPP_VULKAN_BASIC_PRESENTCALLRECORD_HPP
#define FSLDEMOAPP_VULKAN_BASIC_PRESENTCALLRECORD_HPP
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

#include <FslBase/Time/TickCount.hpp>
#include <cstdint>

namespace Fsl::VulkanBasic
{
  //! When the swapchain was called for a frame, as HighResolutionTimer timestamps. These two calls are where a frame loop waits for the
  //! display, so a app that looks at its frame pacing can see where the time of a frame went.
  struct PresentCallRecord
  {
    //! The id of the present (zero = nothing was presented yet)
    uint64_t PresentId{0};
    //! The index of the swapchain image that was presented
    uint32_t ImageIndex{0};
    //! When vkAcquireNextImageKHR was called and when it returned
    TickCount AcquireCallTime;
    TickCount AcquireReturnTime;
    //! The VkResult of vkAcquireNextImageKHR (zero is success, 1000001003 is VK_SUBOPTIMAL_KHR)
    int32_t AcquireResult{0};
    //! When vkQueuePresentKHR was called and when it returned
    TickCount PresentCallTime;
    TickCount PresentReturnTime;
  };
}

#endif
