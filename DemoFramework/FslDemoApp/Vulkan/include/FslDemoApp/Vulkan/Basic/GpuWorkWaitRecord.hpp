#ifndef FSLDEMOAPP_VULKAN_BASIC_GPUWORKWAITRECORD_HPP
#define FSLDEMOAPP_VULKAN_BASIC_GPUWORKWAITRECORD_HPP
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
  //! What became of a wait for the GPU to be done with a frame that was submitted (DemoAppVulkanBasic::WaitForGpuWork), with
  //! HighResolutionTimer timestamps on the clock of PresentCallRecord.
  struct GpuWorkWaitRecord
  {
    //! The id of the present of the frame that was waited for
    uint64_t PresentId{0};
    //! When the wait began and when it ended
    TickCount BeginTime;
    TickCount EndTime;
    //! The VkResult of the wait: zero (VK_SUCCESS) the GPU is done with the frame, 2 (VK_TIMEOUT) the time ran out. A frame that is
    //! in no frame slot any more is one the GPU is done with: it is not waited for and the result is zero.
    int32_t Result{0};

    //! True if the GPU is done with the frame
    [[nodiscard]] constexpr bool IsDone() const noexcept
    {
      return Result == 0;
    }
  };
}

#endif
