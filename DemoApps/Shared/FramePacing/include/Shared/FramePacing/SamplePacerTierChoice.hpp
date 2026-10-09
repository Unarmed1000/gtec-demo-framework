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


#include <Shared/FramePacing/SamplePacerKind.hpp>
#include <Shared/FramePacing/SamplePacerRating.hpp>
#include <array>
#include <cstddef>

//! What the sample needs to know to offer the tiers of the pacer library and to be in one: which kind asks for which tier and what
//! the pacer uses for it. Which tier a set of capabilities is worth is the library's rule (SamplePacer::Rate), not these.
namespace Fsl::SamplePacerTierChoiceUtil
{
  //! The tiers, the best first
  inline constexpr std::array<SamplePacerTier, 4> Tiers = {SamplePacerTier::VBlankWaitForPresent, SamplePacerTier::VBlankPeriodOnly,
                                                           SamplePacerTier::TimerWaitForPresent, SamplePacerTier::TimerPeriodOnly};

  //! The kind that asks for a tier
  constexpr SamplePacerKind ToKind(const SamplePacerTier tier) noexcept
  {
    switch (tier)
    {
    case SamplePacerTier::VBlankWaitForPresent:
      return SamplePacerKind::VBlankWaitForPresent;
    case SamplePacerTier::VBlankPeriodOnly:
      return SamplePacerKind::VBlankPeriodOnly;
    case SamplePacerTier::TimerWaitForPresent:
      return SamplePacerKind::TimerWaitForPresent;
    case SamplePacerTier::TimerPeriodOnly:
      break;
    }
    return SamplePacerKind::TimerPeriodOnly;
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

  //! What a run with the given pacer uses of what the app has: the vertical blank times and the wait for a present, each where
  //! the pacer is one that uses it. The tier the pacer library gives the result is the tier such a run is in: the one of the
  //! pacer where the app has what it uses, else the tier of what is left of it (the vertical blank times without a present to
  //! wait for, the wait without a window system that tells when the display refreshes, the timer without both).
  constexpr SamplePacerCapabilities ToUsedCapabilities(const SamplePacerCapabilities& has, const SamplePacerKind kind) noexcept
  {
    SamplePacerCapabilities uses;
    uses.VBlankTimes = has.VBlankTimes && IsVBlankKind(kind);
    uses.WaitForPresent = has.WaitForPresent && IsPresentWaitKind(kind);
    return uses;
  }
}

#endif
