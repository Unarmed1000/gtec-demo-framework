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
  //! The tiers, the best first: four sub tiers for each major tier that has them, in the same order in every one (SubTierKinds),
  //! and the system's hold, a major tier of one tier without sub tiers
  inline constexpr std::array<SamplePacerTier, 13> Tiers = {SamplePacerTier::AtTimeVBlankWaitForPresent,
                                                            SamplePacerTier::AtTimeVBlankPeriodOnly,
                                                            SamplePacerTier::AtTimeTimerWaitForPresent,
                                                            SamplePacerTier::AtTimeTimerPeriodOnly,
                                                            SamplePacerTier::AfterDurationVBlankWaitForPresent,
                                                            SamplePacerTier::AfterDurationVBlankDisplayTimes,
                                                            SamplePacerTier::AfterDurationTimerWaitForPresent,
                                                            SamplePacerTier::AfterDurationTimerDisplayTimes,
                                                            SamplePacerTier::SystemHoldsLoop,
                                                            SamplePacerTier::VBlankWaitForPresent,
                                                            SamplePacerTier::VBlankPeriodOnly,
                                                            SamplePacerTier::TimerWaitForPresent,
                                                            SamplePacerTier::TimerPeriodOnly};

  //! What the frame loop paces with in each sub tier, the first first: the same in every major tier
  inline constexpr std::array<SamplePacerKind, 4> SubTierKinds = {SamplePacerKind::VBlankWaitForPresent, SamplePacerKind::VBlankPeriodOnly,
                                                                  SamplePacerKind::TimerWaitForPresent, SamplePacerKind::TimerPeriodOnly};

  //! The major tiers, and the most sub tiers a major tier has (the system's hold has none: SubTierCountOf)
  inline constexpr std::size_t MajorTierCount = 4;
  inline constexpr std::size_t SubTierCount = 4;

  //! The index in Tiers of the first tier of each major tier, and the sub tiers each has
  inline constexpr std::array<std::size_t, MajorTierCount> MajorTierFirstIndex = {0, 4, 8, 9};
  inline constexpr std::array<std::size_t, MajorTierCount> MajorTierSubTierCount = {4, 4, 0, 4};

  //! The sub tiers of a major tier (index 0 to MajorTierCount - 1): four, and none for the system's hold
  constexpr std::size_t SubTierCountOf(const std::size_t majorIndex) noexcept
  {
    return majorIndex < MajorTierCount ? MajorTierSubTierCount[majorIndex] : 0u;
  }

  //! The first tier of a major tier (the tier itself where it has no sub tiers)
  constexpr SamplePacerTier FirstTierOf(const std::size_t majorIndex) noexcept
  {
    return Tiers[MajorTierFirstIndex[majorIndex < MajorTierCount ? majorIndex : MajorTierCount - 1u]];
  }

  //! The index in Tiers of a sub tier of a major tier, the major tier's own tier where it has no sub tiers
  constexpr std::size_t ToTierIndex(const std::size_t majorIndex, const std::size_t subIndex) noexcept
  {
    const std::size_t major = majorIndex < MajorTierCount ? majorIndex : MajorTierCount - 1u;
    const std::size_t subTiers = MajorTierSubTierCount[major];
    if (subTiers == 0u)
    {
      return MajorTierFirstIndex[major];
    }
    return MajorTierFirstIndex[major] + (subIndex < subTiers ? subIndex : subTiers - 1u);
  }

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

  //! The major tier of a tier, 0 to MajorTierCount - 1
  constexpr std::size_t ToMajorIndex(const SamplePacerTier tier) noexcept
  {
    const std::size_t index = ToTierIndex(tier);
    std::size_t major = 0;
    for (std::size_t i = 1; i < MajorTierCount; ++i)
    {
      if (index >= MajorTierFirstIndex[i])
      {
        major = i;
      }
    }
    return major;
  }

  //! The sub tier of a tier, 0 to SubTierCount - 1, or SubTierCount for a tier whose major tier has no sub tiers
  constexpr std::size_t ToSubIndex(const SamplePacerTier tier) noexcept
  {
    const std::size_t major = ToMajorIndex(tier);
    return MajorTierSubTierCount[major] > 0u ? ToTierIndex(tier) - MajorTierFirstIndex[major] : SubTierCount;
  }

  //! True for the tier where the system holds the loop (the third major tier, no sub tiers)
  constexpr bool IsSystemHoldTier(const SamplePacerTier tier) noexcept
  {
    return tier == SamplePacerTier::SystemHoldsLoop;
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
    case SamplePacerTier::SystemHoldsLoop:
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
    case SamplePacerTier::SystemHoldsLoop:
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
    // Where the system holds the loop the loop paces with neither vertical blank times nor a wait for a present
    case SamplePacerTier::SystemHoldsLoop:
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
  //! where the kind is one that uses it, the time on the present where the run is asked to use it, and the waits of the system's
  //! hold where the run is asked to use them. The tier the pacer library
  //! gives the result is the tier such a run is in: the one that was asked for where the app has what it takes, else the tier of
  //! what is left of it (the vertical blank times without a present to wait for, the wait without a window system that tells when
  //! the display refreshes, the timer without both, the frame loop placing the frame without a time on the present).
  //! @param presentAtTime true if the run is to use the time on the present before which the frame is not shown, where the app has it
  //! @param presentAfterDuration true if the run is to use the time the frame before stays on screen, where the app has it and the
  //!        present is not given the other time (a present takes one of the two)
  //! @param displayTimes true if the run gives the pacer the display times, where the app has them
  //! @param systemHolds true if the run tells the pacer that its present or its call for the next image waits while the swap chain's
  //!        queue is full, where the app has that: the system then holds the loop (the third major tier, where no time on the present
  //!        puts the run in a better one)
  constexpr SamplePacerCapabilities ToUsedCapabilities(const SamplePacerCapabilities& has, const SamplePacerKind kind, const bool presentAtTime,
                                                       const bool presentAfterDuration, const bool displayTimes, const bool systemHolds) noexcept
  {
    SamplePacerCapabilities uses;
    uses.PresentWaits = has.PresentWaits && systemHolds;
    uses.AcquireWaits = has.AcquireWaits && systemHolds;
    uses.VBlankTimes = has.VBlankTimes && IsVBlankKind(kind);
    uses.WaitForPresent = has.WaitForPresent && IsPresentWaitKind(kind);
    // The kinds that wait for a present can do it with a frame callback. The library says when it is used for that.
    uses.FrameCallback = has.FrameCallback && IsPresentWaitKind(kind);
    uses.PresentAtTime = has.PresentAtTime && presentAtTime;
    uses.PresentAfterDuration = has.PresentAfterDuration && presentAfterDuration && !uses.PresentAtTime;
    // A swap interval is how such a app presents, and the display times where the run gives them to the pacer
    uses.PresentSwapIntervalMax = has.PresentSwapIntervalMax;
    uses.PresentSwapIntervalMin = has.PresentSwapIntervalMin;
    uses.DisplayTimes = has.DisplayTimes && displayTimes;
    // What the display does with a frame that is overdue is a fact of the system, not something a run chooses
    uses.PresentSkipsOverdue = has.PresentSkipsOverdue;
    return uses;
  }
}

#endif
