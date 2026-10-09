#ifndef FSLDEMOAPP_VULKAN_BASIC_FRAMESTARTWAITRECORD_HPP
#define FSLDEMOAPP_VULKAN_BASIC_FRAMESTARTWAITRECORD_HPP
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

namespace Fsl::VulkanBasic
{
  //! The waits of the app base before the frame that is being drawn, as HighResolutionTimer timestamps on the clock of
  //! PresentCallRecord. They are where the system can hold a frame loop before a frame: the wait for the frame slot waits for the GPU
  //! to be done with a earlier frame, the acquire waits for a image of the swapchain to be free. A app that paces its frames can
  //! tell its pacer about them, so a frame the system held is not taken for one that is late.
  struct FrameStartWaitRecord
  {
    //! When the wait for the frame slot began and when it ended (the fence of the frame that used the slot before, and the fence
    //! of its present where there is one)
    TickCount FrameSlotWaitBeginTime;
    TickCount FrameSlotWaitEndTime;
    //! When vkAcquireNextImageKHR was called and when it returned
    TickCount AcquireCallTime;
    TickCount AcquireReturnTime;
  };
}

#endif
