#ifndef FSLDEMOAPP_VULKAN_BASIC_PRESENTWAITRECORD_HPP
#define FSLDEMOAPP_VULKAN_BASIC_PRESENTWAITRECORD_HPP
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
  //! What became of a wait for a present of the swapchain to be presented (DemoAppVulkanBasic::WaitForPresent), with HighResolutionTimer
  //! timestamps on the clock of PresentCallRecord.
  struct PresentWaitRecord
  {
    //! The id of the present that was waited for
    uint64_t PresentId{0};
    //! When the wait began and when it ended
    TickCount BeginTime;
    TickCount EndTime;
    //! The VkResult of the wait: zero (VK_SUCCESS) the present was presented, 2 (VK_TIMEOUT) the time ran out,
    //! -8 (VK_ERROR_FEATURE_NOT_PRESENT) the swapchain can not be waited on, -1000001004 (VK_ERROR_OUT_OF_DATE_KHR) the present is not
    //! one of the swapchain as it is now, so it was not waited for.
    int32_t Result{0};

    //! True if the wait ended because the present was presented
    [[nodiscard]] constexpr bool IsPresented() const noexcept
    {
      return Result == 0;
    }
  };
}

#endif
