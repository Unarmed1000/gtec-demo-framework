#ifndef SHARED_FRAMEPACING_SAMPLEPACERPROFILE_HPP
#define SHARED_FRAMEPACING_SAMPLEPACERPROFILE_HPP
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
  //! Where a frame waits for the time the frame pacer gives for the start of the next frame, where the app is the one that has to
  //! wait (a present of a swapchain need not make the loop wait for the display: it was measured to return at once, and a loop that
  //! does not wait then makes more frames than the display shows). The frame pacer gives one time, the profiles differ in which side
  //! of the present the wait is on. A frame is started once per swap interval with both. What is waited for is a time on the clock
  //! of the app, or the vsync of the window system where the hold is SamplePacerHold::VSync.
  enum class SamplePacerProfile
  {
    //! The frame is rendered right away and the present waits for the time of the pacer, so a present can not come before the display
    //! has taken the one before it. The next frame starts when the present was made. A frame that is done early waits finished, which
    //! keeps the time of the present apart from how long the work took.
    RenderEarly,
    //! The start of the frame waits for the time of the pacer and the frame is presented when it is done. The frame is as fresh as it
    //! can be when it is shown, and the time of its present moves with how long the work took.
    RenderLate,
    //! No wait: the frame starts when the loop gets there and is presented when it is done. What the present and the swapchain make
    //! of that is not frame pacing, it is here so a capture can show the loop without it.
    Off
  };
}

#endif
