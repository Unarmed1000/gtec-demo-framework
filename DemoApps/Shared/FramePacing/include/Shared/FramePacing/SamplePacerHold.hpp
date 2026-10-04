#ifndef SHARED_FRAMEPACING_SAMPLEPACERHOLD_HPP
#define SHARED_FRAMEPACING_SAMPLEPACERHOLD_HPP
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

namespace Fsl
{
  //! How a frame is held for more than one refresh where the present only holds it for one (a Vulkan FIFO present has no swap interval).
  //! A method the system can not do falls back: the vsync wait, then the sleep.
  //! What a platform can offer for each of them is listed in Doc/FramePacingPlatformSupport.md.
  enum class SamplePacerHold
  {
    //! The best method the system can do: the present with a target time, else the vsync wait, else the sleep
    Auto,
    //! The sample waits on the vsync of the window system: it presents during the refresh before the one the frame is aimed at, in the
    //! middle of the time the window system leaves for it. It needs the window system to say when the display refreshes
    //! (INativeWindow::TryGetVSyncInfo), nothing of the graphics API.
    VSync,
    //! The sample sleeps on a timer and presents then. It needs nothing, and does not know where the refreshes are: a guess.
    Wait,
    //! The present is given a target time and the presentation engine holds the frame (VK_EXT_present_timing with presentAtRelativeTime).
    Schedule
  };
}

#endif
