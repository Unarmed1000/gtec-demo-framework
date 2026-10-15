#ifndef SHARED_FRAMEPACING_SAMPLEPACERTIER_HPP
#define SHARED_FRAMEPACING_SAMPLEPACERTIER_HPP
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
  //! The tiers of the pacer library (its PacerTier), the best first. A tier is the capabilities a app needs to reach it, and the
  //! definition is the library's: the sample has none of its own. There are three major tiers, by who places a frame on its
  //! refresh, of four sub tiers each, and a tier is written as the two numbers ("3.1", SamplePacer::GetTierNumber):
  //! - 1: the display places the frame against a clock, from a time on the present before which the frame is not shown.
  //! - 2: the display places the frame relative to the one before, from a time the frame before stays on screen at least (or a
  //!   swap interval of two or more): the loop may present early. It needs a wait for a present or display times, so the pacer sees
  //!   a late frame.
  //! - 3: the frame loop places the frame: it has to make the present at the right moment. Every app reaches its last sub tier.
  //! A sub tier is what the loop paces with, the same in every major tier: .1 the vertical blank times of the window system and a
  //! wait for a present, .2 the vertical blank times, .3 a wait for a present on a timer, .4 a timer (in major tier 2 the display
  //! times take the place of the wait in .2 and .4).
  enum class SamplePacerTier
  {
    //! 1.1: a time on the present, vertical blank times and a wait for a present
    AtTimeVBlankWaitForPresent,
    //! 1.2: a time on the present and vertical blank times
    AtTimeVBlankPeriodOnly,
    //! 1.3: a time on the present and a wait for a present, on a timer
    AtTimeTimerWaitForPresent,
    //! 1.4: a time on the present, on a timer and the refresh period only
    AtTimeTimerPeriodOnly,
    //! 2.1: a duration on the present, vertical blank times and a wait for a present
    AfterDurationVBlankWaitForPresent,
    //! 2.2: a duration on the present, vertical blank times and display times
    AfterDurationVBlankDisplayTimes,
    //! 2.3: a duration on the present and a wait for a present, on a timer
    AfterDurationTimerWaitForPresent,
    //! 2.4: a duration on the present and display times, on a timer
    AfterDurationTimerDisplayTimes,
    //! 3.1: vertical blank times and a wait for a present
    VBlankWaitForPresent,
    //! 3.2: vertical blank times
    VBlankPeriodOnly,
    //! 3.3: a wait for a present, on a timer
    TimerWaitForPresent,
    //! 3.4: a timer and the refresh period only: every app reaches this
    TimerPeriodOnly
  };
}

#endif
