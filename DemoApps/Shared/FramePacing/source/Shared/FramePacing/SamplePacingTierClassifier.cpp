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

#include <FslNativeWindow/Base/NativeWindowTimingSupport.hpp>
#include <Shared/FramePacing/SamplePacingTierClassifier.hpp>
#include <algorithm>
#include <string>
#include <vector>

namespace Fsl::SamplePacingTierClassifier
{
  namespace
  {
    struct TierRecord
    {
      SamplePacingTier Tier{SamplePacingTier::Tier3};
      SamplePacingTierReason Reason{SamplePacingTierReason::Timer};
    };

    //! What the hold method 'auto' gives: the present with a target time, else the vsync wait, else the timer
    constexpr TierRecord BestHoldOfSample(const SamplePacingTierFacts& facts) noexcept
    {
      if (facts.PresentSchedulingSupported)
      {
        return {SamplePacingTier::Tier1, SamplePacingTierReason::TimedPresent};
      }
      if (facts.HasVSyncTime)
      {
        return {SamplePacingTier::Tier2, SamplePacingTierReason::VSyncTime};
      }
      return {SamplePacingTier::Tier3, SamplePacingTierReason::Timer};
    }

    //! The tier of the method the sample holds a frame with
    constexpr TierRecord HoldOfSample(const SamplePacingTierFacts& facts) noexcept
    {
      switch (facts.HoldMethod)
      {
      case SamplePacerHold::Schedule:
        return {SamplePacingTier::Tier1, SamplePacingTierReason::TimedPresent};
      case SamplePacerHold::VSync:
        return {SamplePacingTier::Tier2, SamplePacingTierReason::VSyncTime};
      case SamplePacerHold::Auto:
        return BestHoldOfSample(facts);
      case SamplePacerHold::Wait:
        break;
      }
      return {SamplePacingTier::Tier3, SamplePacingTierReason::Timer};
    }

    bool Contains(const std::vector<std::string>& names, const std::string_view name) noexcept
    {
      return std::find(names.begin(), names.end(), name) != names.end();
    }
  }


  SamplePacingTierInfo Classify(const SamplePacingTierFacts& facts) noexcept
  {
    SamplePacingTierInfo info;
    info.ExplicitSync = facts.ExplicitSync;

    // The best this run could have: a present with a swap interval that can be longer than one refresh holds the frame itself, else
    // it is what 'auto' gives
    const bool swapIntervalCanHold = facts.PresentHasSwapInterval && (facts.PresentSwapIntervalMax == 0u || facts.PresentSwapIntervalMax >= 2u);
    const TierRecord best = swapIntervalCanHold ? TierRecord{SamplePacingTier::Tier1, SamplePacingTierReason::SwapInterval} : BestHoldOfSample(facts);
    info.Best = best.Tier;
    info.BestReason = best.Reason;

    if (!facts.PacerOn)
    {
      info.InUse = SamplePacingTier::Tier4;
      info.InUseReason = SamplePacingTierReason::PacerOff;
      return info;
    }
    // A present with a swap interval holds the frame, unless the frame is to be held for longer than the swap interval can be: then
    // the sample holds it like it does for a present without one
    const TierRecord inUse = (facts.PresentHasSwapInterval && !facts.SampleHeldFrame)
                               ? TierRecord{SamplePacingTier::Tier1, SamplePacingTierReason::SwapInterval}
                               : HoldOfSample(facts);
    info.InUse = inUse.Tier;
    info.InUseReason = inUse.Reason;
    return info;
  }


  SampleExplicitSync ToExplicitSync(const NativeWindowTimingSupport& support) noexcept
  {
    if (Contains(support.Available, ExplicitSyncGlobalName))
    {
      return SampleExplicitSync::Offered;
    }
    return Contains(support.NotAvailable, ExplicitSyncGlobalName) ? SampleExplicitSync::NotOffered : SampleExplicitSync::NotApplicable;
  }


  int32_t ToNumber(const SamplePacingTier tier) noexcept
  {
    switch (tier)
    {
    case SamplePacingTier::Tier1:
      return 1;
    case SamplePacingTier::Tier2:
      return 2;
    case SamplePacingTier::Tier3:
      return 3;
    case SamplePacingTier::Tier4:
      break;
    }
    return TierCount;
  }


  std::string_view ToDisplayString(const SamplePacingTierReason reason) noexcept
  {
    switch (reason)
    {
    case SamplePacingTierReason::PacerOff:
      return "pacer off";
    case SamplePacingTierReason::TimedPresent:
      return "timed present";
    case SamplePacingTierReason::SwapInterval:
      return "swap interval";
    case SamplePacingTierReason::VSyncTime:
      return "vsync time";
    case SamplePacingTierReason::Timer:
      break;
    }
    return "timer";
  }


  std::string_view ToLogString(const SamplePacingTierReason reason) noexcept
  {
    switch (reason)
    {
    case SamplePacingTierReason::PacerOff:
      return "pacerOff";
    case SamplePacingTierReason::TimedPresent:
      return "timedPresent";
    case SamplePacingTierReason::SwapInterval:
      return "swapInterval";
    case SamplePacingTierReason::VSyncTime:
      return "vsyncTime";
    case SamplePacingTierReason::Timer:
      break;
    }
    return "timer";
  }


  std::string_view ToDisplayString(const SampleExplicitSync value) noexcept
  {
    switch (value)
    {
    case SampleExplicitSync::NotOffered:
      return "not offered (not in use)";
    case SampleExplicitSync::Offered:
      return "offered by the compositor";
    case SampleExplicitSync::NotApplicable:
      break;
    }
    return {};
  }
}
