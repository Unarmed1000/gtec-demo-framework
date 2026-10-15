#ifndef SHARED_FRAMEPACING_SAMPLEPACERTIERCHOICE_HPP
#define SHARED_FRAMEPACING_SAMPLEPACERTIERCHOICE_HPP
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


#include <Shared/FramePacing/SamplePacerCapabilities.hpp>
#include <Shared/FramePacing/SamplePacerKind.hpp>
#include <Shared/FramePacing/SamplePacerTier.hpp>
#include <array>
#include <cstddef>

//! What the sample needs to know to offer the tiers of the pacer library and to be in one: which kind asks for which tier and what
//! the pacer uses for it. Which tier a set of capabilities is worth is the library's rule (SamplePacer::Rate), not these.
namespace Fsl::SamplePacerTierChoiceUtil
{
  //! The tiers, the best first: four sub tiers for each major tier, in the same order in every one (SubTierKinds)
  inline constexpr std::array<SamplePacerTier, 12> Tiers = {SamplePacerTier::AtTimeVBlankWaitForPresent,
                                                            SamplePacerTier::AtTimeVBlankPeriodOnly,
                                                            SamplePacerTier::AtTimeTimerWaitForPresent,
                                                            SamplePacerTier::AtTimeTimerPeriodOnly,
                                                            SamplePacerTier::AfterDurationVBlankWaitForPresent,
                                                            SamplePacerTier::AfterDurationVBlankDisplayTimes,
                                                            SamplePacerTier::AfterDurationTimerWaitForPresent,
                                                            SamplePacerTier::AfterDurationTimerDisplayTimes,
                                                            SamplePacerTier::VBlankWaitForPresent,
                                                            SamplePacerTier::VBlankPeriodOnly,
                                                            SamplePacerTier::TimerWaitForPresent,
                                                            SamplePacerTier::TimerPeriodOnly};

  //! What the frame loop paces with in each sub tier, the first first: the same in every major tier
  inline constexpr std::array<SamplePacerKind, 4> SubTierKinds = {SamplePacerKind::VBlankWaitForPresent, SamplePacerKind::VBlankPeriodOnly,
                                                                  SamplePacerKind::TimerWaitForPresent, SamplePacerKind::TimerPeriodOnly};

  //! The tiers of a major tier and the sub tiers in it
  inline constexpr std::size_t MajorTierCount = 3;
  inline constexpr std::size_t SubTierCount = 4;

  //! The index in Tiers of a tier
  constexpr std::size_t ToTierIndex(const SamplePacerTier tier) noexcept
  {
    for (std::size_t i = 0; i < Tiers.size(); ++i)
    {
      if (Tiers[i] == tier)
      {
        return i;
      }
    }
    return Tiers.size() - 1u;
  }

  //! True for the tiers where the display places the frame against a clock: the present is given a time before which the frame is
  //! not shown (the first major tier)
  constexpr bool IsPresentAtTimeTier(const SamplePacerTier tier) noexcept
  {
    switch (tier)
    {
    case SamplePacerTier::AtTimeVBlankWaitForPresent:
    case SamplePacerTier::AtTimeTimerWaitForPresent:
    case SamplePacerTier::AtTimeVBlankPeriodOnly:
    case SamplePacerTier::AtTimeTimerPeriodOnly:
      return true;
    case SamplePacerTier::AfterDurationVBlankWaitForPresent:
    case SamplePacerTier::AfterDurationTimerWaitForPresent:
    case SamplePacerTier::AfterDurationVBlankDisplayTimes:
    case SamplePacerTier::AfterDurationTimerDisplayTimes:
    case SamplePacerTier::VBlankWaitForPresent:
    case SamplePacerTier::VBlankPeriodOnly:
    case SamplePacerTier::TimerWaitForPresent:
    case SamplePacerTier::TimerPeriodOnly:
      break;
    }
    return false;
  }

  //! True for the tiers where the display places the frame relative to the one before: the present is given a time the frame
  //! before stays on screen at least (the second major tier)
  constexpr bool IsPresentAfterDurationTier(const SamplePacerTier tier) noexcept
  {
    switch (tier)
    {
    case SamplePacerTier::AfterDurationVBlankWaitForPresent:
    case SamplePacerTier::AfterDurationTimerWaitForPresent:
    case SamplePacerTier::AfterDurationVBlankDisplayTimes:
    case SamplePacerTier::AfterDurationTimerDisplayTimes:
      return true;
    case SamplePacerTier::AtTimeVBlankWaitForPresent:
    case SamplePacerTier::AtTimeTimerWaitForPresent:
    case SamplePacerTier::AtTimeVBlankPeriodOnly:
    case SamplePacerTier::AtTimeTimerPeriodOnly:
    case SamplePacerTier::VBlankWaitForPresent:
    case SamplePacerTier::VBlankPeriodOnly:
    case SamplePacerTier::TimerWaitForPresent:
    case SamplePacerTier::TimerPeriodOnly:
      break;
    }
    return false;
  }

  //! The kind of a tier: what the frame loop paces with in it, which is its sub tier
  constexpr SamplePacerKind ToKind(const SamplePacerTier tier) noexcept
  {
    switch (tier)
    {
    case SamplePacerTier::AfterDurationVBlankWaitForPresent:
    case SamplePacerTier::AtTimeVBlankWaitForPresent:
    case SamplePacerTier::VBlankWaitForPresent:
      return SamplePacerKind::VBlankWaitForPresent;
    case SamplePacerTier::AfterDurationVBlankDisplayTimes:
    case SamplePacerTier::AtTimeVBlankPeriodOnly:
    case SamplePacerTier::VBlankPeriodOnly:
      return SamplePacerKind::VBlankPeriodOnly;
    case SamplePacerTier::AfterDurationTimerWaitForPresent:
    case SamplePacerTier::AtTimeTimerWaitForPresent:
    case SamplePacerTier::TimerWaitForPresent:
      return SamplePacerKind::TimerWaitForPresent;
    case SamplePacerTier::AfterDurationTimerDisplayTimes:
    case SamplePacerTier::AtTimeTimerPeriodOnly:
    case SamplePacerTier::TimerPeriodOnly:
      break;
    }
    return SamplePacerKind::TimerPeriodOnly;
  }

  //! The tier of a kind where the frame loop places the frame (the last major tier): the tier every app has for it
  constexpr SamplePacerTier ToLoopPlacedTier(const SamplePacerKind kind) noexcept
  {
    switch (kind)
    {
    case SamplePacerKind::VBlankWaitForPresent:
      return SamplePacerTier::VBlankWaitForPresent;
    case SamplePacerKind::VBlankPeriodOnly:
      return SamplePacerTier::VBlankPeriodOnly;
    case SamplePacerKind::TimerWaitForPresent:
      return SamplePacerTier::TimerWaitForPresent;
    case SamplePacerKind::TimerPeriodOnly:
      break;
    }
    return SamplePacerTier::TimerPeriodOnly;
  }

  //! A order of the kinds that has every change from one of them to another exactly once when it is gone through from
  //! any place and around to that place again: twelve changes. For a run that measures what a change of the kind does.
  inline constexpr std::array<SamplePacerKind, 12> KindChangeOrder = {
    SamplePacerKind::TimerPeriodOnly,      SamplePacerKind::TimerWaitForPresent, SamplePacerKind::TimerPeriodOnly,
    SamplePacerKind::VBlankPeriodOnly,     SamplePacerKind::TimerPeriodOnly,     SamplePacerKind::VBlankWaitForPresent,
    SamplePacerKind::TimerWaitForPresent,  SamplePacerKind::VBlankPeriodOnly,    SamplePacerKind::TimerWaitForPresent,
    SamplePacerKind::VBlankWaitForPresent, SamplePacerKind::VBlankPeriodOnly,    SamplePacerKind::VBlankWaitForPresent};

  //! The first place of a kind in KindChangeOrder (zero for a kind that is not in it)
  constexpr std::size_t FindKindChangeIndex(const SamplePacerKind kind) noexcept
  {
    for (std::size_t i = 0; i < KindChangeOrder.size(); ++i)
    {
      if (KindChangeOrder[i] == kind)
      {
        return i;
      }
    }
    return 0;
  }

  //! The kinds with which the pacer is told where the refreshes of the display are
  constexpr bool IsVBlankKind(const SamplePacerKind kind) noexcept
  {
    return kind == SamplePacerKind::VBlankPeriodOnly || kind == SamplePacerKind::VBlankWaitForPresent;
  }

  //! The kinds with which the pacer names a present to wait for
  constexpr bool IsPresentWaitKind(const SamplePacerKind kind) noexcept
  {
    return kind == SamplePacerKind::TimerWaitForPresent || kind == SamplePacerKind::VBlankWaitForPresent;
  }

  //! What a run uses of what the app has, of what a tier is made of: the vertical blank times and the wait for a present, each
  //! where the kind is one that uses it, and the time on the present where the run is asked to use it. The tier the pacer library
  //! gives the result is the tier such a run is in: the one that was asked for where the app has what it takes, else the tier of
  //! what is left of it (the vertical blank times without a present to wait for, the wait without a window system that tells when
  //! the display refreshes, the timer without both, the frame loop placing the frame without a time on the present).
  //! @param presentAtTime true if the run is to use the time on the present before which the frame is not shown, where the app has it
  //! @param presentAfterDuration true if the run is to use the time the frame before stays on screen, where the app has it and the
  //!        present is not given the other time (a present takes one of the two)
  //! @param displayTimes true if the run gives the pacer the display times, where the app has them
  constexpr SamplePacerCapabilities ToUsedCapabilities(const SamplePacerCapabilities& has, const SamplePacerKind kind, const bool presentAtTime,
                                                       const bool presentAfterDuration, const bool displayTimes) noexcept
  {
    SamplePacerCapabilities uses;
    uses.VBlankTimes = has.VBlankTimes && IsVBlankKind(kind);
    uses.WaitForPresent = has.WaitForPresent && IsPresentWaitKind(kind);
    uses.PresentAtTime = has.PresentAtTime && presentAtTime;
    uses.PresentAfterDuration = has.PresentAfterDuration && presentAfterDuration && !uses.PresentAtTime;
    // A swap interval is how such a app presents, and the display times where the run gives them to the pacer
    uses.PresentSwapIntervalMax = has.PresentSwapIntervalMax;
    uses.DisplayTimes = has.DisplayTimes && displayTimes;
    // What the display does with a frame that is overdue is a fact of the system, not something a run chooses
    uses.PresentSkipsOverdue = has.PresentSkipsOverdue;
    return uses;
  }
}

#endif
