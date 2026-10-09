#ifndef SHARED_FRAMEPACING_SAMPLEPACERRATING_HPP
#define SHARED_FRAMEPACING_SAMPLEPACERRATING_HPP
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


#include <cstdint>

namespace Fsl
{
  //! The tiers of the pacer library where the frame loop places the frame (its major tier 3, the last four of its PacerTier), the
  //! best first. A tier is the capabilities a app needs to reach it. The definition is the library's: the sample has none of its
  //! own. The library has the same four where the display places the frame, with a present at a time (its major tier 2), which the
  //! sample says next to the tier (SamplePacerRating::TimedPresent), and once more for a display that skips a frame that is overdue
  //! (major tier 1), which has no pacer.
  enum class SamplePacerTier
  {
    //! Vertical blank times and a wait for a present
    VBlankWaitForPresent,
    //! Vertical blank times
    VBlankPeriodOnly,
    //! A wait for a present, on a timer
    TimerWaitForPresent,
    //! A timer and the refresh period only: every app reaches this
    TimerPeriodOnly
  };

  //! What the capabilities of a app are worth, as the pacer library rates them (its PacerRating)
  struct SamplePacerRating
  {
    //! The best tier the app reaches, which names its pacer
    SamplePacerTier Tier{SamplePacerTier::TimerPeriodOnly};
    //! The app reaches that tier with a present at a time on top, so the display places the frame: one of the tiers of major tier
    //! 2 of the pacer library, which are the four of SamplePacerTier with such a present. A present that takes the time the frame
    //! before stays on screen (SamplePacerCapabilities::PresentAfterDuration) is not one: it changes no tier.
    bool TimedPresent{false};
    //! The present can hold a frame for two refreshes or more (a time, a minimum duration, or a swap interval of two or more). It is
    //! no tier: every pacer uses it when it is there.
    bool DisplaySideHolds{false};

    constexpr bool operator==(const SamplePacerRating&) const noexcept = default;
  };

  //! What a app can do or can tell a pacer, as far as the rating depends on it (the capabilities of the pacer library, in the types
  //! of the framework). Every app has the baseline, which is none of them: a clock, the refresh period of the display, a wait until a
  //! time and a present.
  struct SamplePacerCapabilities
  {
    //! The longest swap interval the present takes (zero: the present takes none). It only holds a frame for more than one refresh
    //! when it can be two or more.
    uint32_t PresentSwapIntervalMax{0};
    //! The present takes a time the frame before it stays on screen at least
    bool PresentAfterDuration{false};
    //! The app knows when a vertical blank was and the period between them
    bool VBlankTimes{false};
    //! The app can wait until a present it names was shown
    bool WaitForPresent{false};
    //! The app is told when a present was shown, frames later
    bool DisplayTimes{false};
    //! The app can wait until the GPU is done with a frame it names
    bool WaitForGpuWork{false};

    constexpr bool operator==(const SamplePacerCapabilities&) const noexcept = default;
  };
}

#endif
