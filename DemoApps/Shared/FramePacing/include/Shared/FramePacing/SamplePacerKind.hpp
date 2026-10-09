#ifndef SHARED_FRAMEPACING_SAMPLEPACERKIND_HPP
#define SHARED_FRAMEPACING_SAMPLEPACERKIND_HPP
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
  //! What the pacer of the mb-framepacing library (experimental, its TierPacer) is asked to pace the frames with: each kind is a set of
  //! what the app can do that the pacer is told to use, and the pacer of one of its tiers. The pacer holds every rule and every time
  //! calculation: the sample asks it before a frame starts and before a frame is presented, waits for what it is given and computes
  //! no time itself.
  enum class SamplePacerKind
  {
    //! A timer and the refresh period of the display, nothing else: the tier every app reaches.
    TimerPeriodOnly,
    //! A timer with a wait for a present. As TimerPeriodOnly, and before a frame the pacer also names an earlier present to wait for
    //! until the display showed it, which keeps the presents that wait to be shown few. It needs an app that can wait for a present
    //! (Vulkan with VK_KHR_present_wait2), without one the sample uses TimerPeriodOnly.
    TimerWaitForPresent,
    //! The vertical blank times with the refresh period only. As TimerPeriodOnly, and the pacer is told where the refreshes of the
    //! display are, so it aims a frame at a vertical blank instead of at a step of its own clock. It needs a window system that tells
    //! when the display of the window refreshes (NativeWindowVSyncInfo), without one the sample uses TimerPeriodOnly.
    VBlankPeriodOnly,
    //! The vertical blank times with a wait for a present: VBlankPeriodOnly with the wait of TimerWaitForPresent. A wait that held
    //! the loop tells the pacer which vertical blank a frame was shown at, and it learns from that where in a refresh a frame has to
    //! be ready. It needs both: a window system that tells when the display refreshes and a app that can wait for a present. Without
    //! the wait the sample uses VBlankPeriodOnly, without the vertical blank times TimerWaitForPresent.
    VBlankWaitForPresent
  };
}

#endif
